/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_command_ls_boot.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "acr.h"
#include "acr_objacr.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_acr_private.h"
#include "config/g_hal_register.h"
#include <liblwriscv/print.h>
#include <partitions.h>
#include "sec2/sec2ifacr.h"
#include "tegra_se.h"
#include "hwproject.h"
#include "dev_fb.h"
#include "dev_lwdec_pri.h"
#include "dev_fbif_v4.h"
#include "dev_graphics_nobundle.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_graphics_nobundle_addendum.h"
#include "dev_smcarb_addendum.h"

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ External Definitions --------------------------- */
extern RM_FLCN_ACR_DESC g_desc;
extern LwU8 g_wprHeaderWrappers[LSF_WPR_HEADERS_WRAPPER_TOTAL_SIZE_MAX];
extern LwU8 g_lsbHeaderWrapper[LSF_LSB_HEADER_WRAPPER_SIZE_ALIGNED_BYTE];
extern HS_FMC_PARAMS g_hsFmcParams;

// Random number used in BUG 3395727 as a WAR for G000 fmodel fix.
#if defined(ACR_FMODEL_BUILD)
#define ACR_FIXED_MAGIC_NUM       (0x03395727)
#endif

/* ------------------------ Function Prototypes ---------------------------- */
static ACR_STATUS
_acrBootstrapFalcon(LwU32 falconId, LwU32 falconInstance, LwU32 flags)
ATTR_OVLY(OVL_NAME);

static ACR_STATUS
_acrGetWprHeader(LwU32 falconId, PLSF_WPR_HEADER_WRAPPER *ppWprHeaderWrapper)
ATTR_OVLY(OVL_NAME);

static ACR_STATUS
_acrSetupNSFalcon(PRM_FLCN_U64 pWprBase, LwU32 wprRegionID, PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper,
                  PACR_FLCN_CONFIG pFlcnCfg, LSF_LSB_HEADER_WRAPPER *pLsbHeaderWrapper)
ATTR_OVLY(OVL_NAME);

static ACR_STATUS
_acrSetupLSFalcon(PRM_FLCN_U64 pWprBase, LwU32 wprRegionID, PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper,
                  PACR_FLCN_CONFIG pFlcnCfg, LSF_LSB_HEADER_WRAPPER *pLsbHeaderWrapper)
ATTR_OVLY(OVL_NAME);

static ACR_STATUS
_acrSetupLSRiscvExt(PRM_FLCN_U64 pWprBase, LwU32 wprRegionID, PACR_FLCN_CONFIG pFlcnCfg, LSF_LSB_HEADER_WRAPPER *pLsbHeaderWrapper)
ATTR_OVLY(OVL_NAME);

ACR_STATUS
_acrSetupLSRiscvExternalBoot(PRM_FLCN_U64 pWprBase, LwU32 wprRegionID, PACR_FLCN_CONFIG pFlcnCfg, PLSF_LSB_HEADER_WRAPPER pLsbHeaderWrapper)
ATTR_OVLY(OVL_NAME);


/*!
 * @brief Top level function to handle BOOTSTRAP_ENGINE command
 * This function calls other functions to execute the command.
 */
ACR_STATUS
acrCmdLsBoot(LwU32 engId, LwU32 engInstance, LwU32 engIndexMask, LwU32 flags)
{
    ACR_STATUS status = ACR_OK;

    // GSP can't boot itself!
    if (engId == LSF_FALCON_ID_GSP_RISCV)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_ILWALID_ARGUMENT);
    }

    // Is engine instance valid?
    if (!acrIsFalconInstanceValid_HAL(pAcr, engId, engInstance))
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_ILWALID_ARGUMENT);
    }

    // Is index mask valid?
    if (!acrIsFalconIndexMaskValid_HAL(pAcr, engId, engIndexMask))
    {
       CHECK_STATUS_OK_OR_GOTO_CLEANUP(ACR_ERROR_ILWALID_ARGUMENT);
    }

    if (engIndexMask == LSF_FALCON_INDEX_MASK_DEFAULT_0)
    {
        status = _acrBootstrapFalcon(engId, engInstance, flags);
    }
    else
    {
        LwU32 engIndexMaskLocal = engIndexMask;
        LwU32 engIndex          = 0x0;

        // Unicast bootloading of instances of same falcon
        while(engIndexMaskLocal != 0x0)
        {
            if ((engIndexMaskLocal & 0x1) == 0x1)
            {
                CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrBootstrapFalcon(engId, engIndex, flags));
            }
            engIndex += 1;
            engIndexMaskLocal >>= 1;
        }
    }

Cleanup:
    return status;
}

/*!
 * @brief _acrBootstrapFalcon
 *        Falcon's UC is selwred in ACR region and only LS clients like SEC2 can
 *        access it. During resetting an engine, RM needs to contact SEC2 to reset
 *        particular falcon.
 *
 * @param[in] falconId              Falcon ID which needs to be reset
 * @param[in] falconInstance        Falcon Instance/Index of same falcon to be bootstrapped
 * @param[in] flags                 Flags used for Bootstraping Falcon
 * @param[in] bUseVA                Set if Falcon uses Virtual Address
 * @param[in] pWprBaseVirtual       VA corresponding to WPR start
 *
 * @return ACR_STATUS:    OK if successful reset of Falcon
 */
ACR_STATUS
_acrBootstrapFalcon
(
    LwU32           engId,
    LwU32           engInstance,
    LwU32           flags
)
{
    ACR_STATUS              status                  = ACR_OK;
    ACR_STATUS              statusCleanup           = ACR_OK;
    ACR_FLCN_CONFIG         flcnCfg;
    PLSF_WPR_HEADER_WRAPPER pWprHeaderWrapper;
    LwU32                   targetMaskPlmOldValue   = 0;
    LwU32                   targetMaskOldValue      = 0;
    RM_FLCN_U64             localWprBase;

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrConfigureSubWprForAcr_HAL(pAcr, LW_TRUE));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrGetFalconConfig_HAL(pAcr, engId, engInstance, &flcnCfg));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrGetWprHeader(engId, &pWprHeaderWrapper));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrReadLsbHeaderWrapper_HAL(pAcr, pWprHeaderWrapper, (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper));

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLockFalconRegSpace_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, &flcnCfg,
                                    LW_TRUE, &targetMaskPlmOldValue, &targetMaskOldValue));

#ifdef LWDEC_PRESENT
    if (!FLD_TEST_DRF(_RM_SEC2_ACR, _CMD_BOOTSTRAP, _FALCON_FLAGS_RESET, _NO, flags) ||
        (engId != LSF_FALCON_ID_LWDEC))
#endif // LWDEC_PRESENT
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrResetFalcon_HAL(pAcr, engId, engInstance));
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrPollForScrubbing_HAL(pAcr, &flcnCfg));
    }

    {
        //
        // The g_desc contains 256B aligned wpr start address. Hence, we need to left shift it by 8 bits in order
        // to get its actual physical address.
        //
        LwU64 localWprAddr = (LwU64)g_desc.regions.regionProps[ACR_WPR1_REGION_IDX].startAddress << (LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT - 4);
        RM_FLCN_U64_PACK(&localWprBase, &localWprAddr);
    }

    if (flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrSetupLSRiscvExt(&localWprBase, LSF_WPR_EXPECTED_REGION_ID, &flcnCfg, (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper));
    }
    else if (flcnCfg.uprocType == ACR_TARGET_ENGINE_CORE_RISCV_EB)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrSetupLSRiscvExternalBoot(&localWprBase, LSF_WPR_EXPECTED_REGION_ID, &flcnCfg, (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper));
    }
    else
    {
        if (flcnCfg.bIsNsFalcon)
        {
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrSetupNSFalcon(&localWprBase, LSF_WPR_EXPECTED_REGION_ID, pWprHeaderWrapper, &flcnCfg, (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper));
        }
        else
        {
            CHECK_STATUS_OK_OR_GOTO_CLEANUP(_acrSetupLSFalcon(&localWprBase, LSF_WPR_EXPECTED_REGION_ID, pWprHeaderWrapper, &flcnCfg, (PLSF_LSB_HEADER_WRAPPER)&g_lsbHeaderWrapper));
        }
    }

    //
    // Generate a Random Number using SE-lite/libCCC and pass it to the engine scratch. 
    // NOTE : We are doing this only for specific engines, not all of them
    //
    if (flcnCfg.sizeOfRandNum > 0)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrPassRandomNumberToEngine_HAL(pAcr, engId, &flcnCfg));
    }

Cleanup:
    // Setup engine RESET and CPUCTL PLM's
    statusCleanup = acrSetupTargetResetPLMs_HAL(pAcr, &flcnCfg);
    
    status = (status != ACR_OK ? status : statusCleanup);

    statusCleanup = acrLockFalconRegSpace_HAL(pAcr, LSF_FALCON_ID_GSP_RISCV, &flcnCfg,
                                    LW_FALSE, &targetMaskPlmOldValue, &targetMaskOldValue);

    status = (status != ACR_OK ? status : statusCleanup);

    statusCleanup = acrConfigureSubWprForAcr_HAL(pAcr, LW_FALSE);

    return (status != ACR_OK ? status : statusCleanup);
}



/*!
 * @brief _acrGetWprHeader
 *        points to the WPR header of falconId
 *
 * @param[in]  engId    :Falcon Id
 * @param[out] ppWprHeader :WPR header of falcon Id
 *
 * @return ACR_OK    Success
 *         ACR_ERROR Failed to located WPR header of falcon Id
 */
static ACR_STATUS
_acrGetWprHeader
(
    LwU32 engId,
    PLSF_WPR_HEADER_WRAPPER *ppWprHeaderWrapper
)
{
    LwU8 index;
    PLSF_WPR_HEADER_WRAPPER pWrapper = (PLSF_WPR_HEADER_WRAPPER)g_wprHeaderWrappers;
    LwU32 wprEngId;
    LwU32 lsbOffset;
    ACR_STATUS status;

    if (ppWprHeaderWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *ppWprHeaderWrapper = NULL;

    // Go through falcon header
    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET,
                                                                            &pWrapper[index], (void *)&lsbOffset));

        CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrLsfWprHeaderWrapperCtrl_HAL(pAcr, LSF_WPR_HEADER_COMMAND_GET_FALCON_ID,
                                                                            &pWrapper[index], (void *)&wprEngId));

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(wprEngId, lsbOffset))
        {
            break;
        }

        if (wprEngId == engId)
        {
            *ppWprHeaderWrapper = (PLSF_WPR_HEADER_WRAPPER)&pWrapper[index];
            break;
        }
    }

    if (*ppWprHeaderWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

Cleanup:
    return status;
}


/*!
 * @brief _acrSetupLSFalcon:  Copies IMEM and DMEM into LS falcon
 *
 * @param[in] pWprBase   : Base addr of WPR Region
 * @param[in] regionId   : ACR Region ID
 * @param[in] pWprheader : WPR Header
 * @param[in] pFlcnCfg   : Falcon Config register
 * @param[in] pLsbHeader : LSB Header
 */
static ACR_STATUS
_acrSetupLSFalcon
(
    PRM_FLCN_U64             pWprBase,
    LwU32                    regionId,
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper,
    PACR_FLCN_CONFIG         pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       addrHi;
    LwU32       addrLo;
    LwU32       wprBase;
    LwU32       blWprBase;
    LwU32       dst;
    LwU32       blCodeSize;
    LwU32       ucodeOffset;
    LwU32       appCodeOffset;
    LwU32       appCodeSize;
    LwU32       appDataOffset;
    LwU32       appDataSize;
    LwU32       blImemOffset;
    LwU32       blDataOffset;
    LwU32       blDataSize;
    LwU32       flags;
#ifdef BOOT_FROM_HS_BUILD
    LwU32       appImemOffset = 0;
    LwU32       appCodebase   = 0;
    LwU32       appDmemOffset = 0;
    LwU32       appDatabase = 0;
#endif

    //
    // Earlier wprOffset was added to lower address and then lower address was
    // checked for overflow. Gourav found that wproffset is always 0.
    // Hence we forced wprOffset as 0 in this task. We are still confirming with
    // Gobi, if there are cases where we can have wprOffset as non zero.
    //
    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    wprBase = (addrLo >> FLCN_IMEM_BLK_ALIGN_BITS) |
              (addrHi << (32 - FLCN_IMEM_BLK_ALIGN_BITS));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));
    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

#ifdef BOOT_FROM_HS_BUILD
    LwU64       localWprBase;
    RM_FLCN_U64_UNPACK(&localWprBase, pWprBase);

    // WPR base is expected to be 256 bytes aligned
    localWprBase = localWprBase >> 8;

    if ((pFlcnCfg->falconId == LSF_FALCON_ID_GSP_RISCV) || (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconCodeDataBaseInScratchGroup_HAL(pAcr, localWprBase, pLsbHeaderWrapper, pFlcnCfg));
    }
#endif // BOOT_FROM_HS_BUILD

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupTargetRegisters_HAL(pAcr, pFlcnCfg));

    if (pFlcnCfg->falconId == LSF_FALCON_ID_FECS)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupCtxswRegisters_HAL(pAcr, pFlcnCfg));
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_FLAGS,
                                                                        pLsbHeaderWrapper, (void *)&flags));

    // Check if this falcon should be loaded via priv copy or DMA
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _FORCE_PRIV_LOAD, _TRUE, flags))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&appCodeOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&appCodeSize));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&appDataSize));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&appDataOffset));

        // Load ucode into IMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPrivLoadTargetFalcon_HAL(pAcr, 0,
              ucodeOffset + appCodeOffset, appCodeSize, regionId, LW_TRUE, pFlcnCfg));
        // Load data into DMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPrivLoadTargetFalcon_HAL(pAcr, 0,
              ucodeOffset + appDataOffset, appDataSize, regionId, LW_FALSE, pFlcnCfg));

        // Set the BOOTVEC
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupBootvec_HAL(pAcr, pFlcnCfg, 0));
    }
    else
    {
        //
        // BL starts at offset, but since the IMEM VA is not zero, we need to make
        // sure FBOFF is equal to the expected IMEM VA. So adjusting the FB BASE to
        // make sure FBOFF equals to VA as expected.
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

        blWprBase = ((wprBase) +
                     ((ucodeOffset) >> FLCN_IMEM_BLK_ALIGN_BITS)) -
                     ((blImemOffset) >> FLCN_IMEM_BLK_ALIGN_BITS);

        // Check if code needs to be loaded at start of IMEM or at end of IMEM
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, flags))
        {
            dst = 0;
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&blCodeSize));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindFarthestImemBl_HAL(pAcr, pFlcnCfg, blCodeSize, &dst));
            dst = dst * FLCN_IMEM_BLK_SIZE_IN_BYTES;
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                                pLsbHeaderWrapper, (void *)&blCodeSize));

#ifdef BOOT_FROM_HS_BUILD
        // Load BL into IMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_FMC_PARAMS,
                                                                            pLsbHeaderWrapper, &g_hsFmcParams));
        if (g_hsFmcParams.bHsFmc)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr,
                dst, blWprBase, blImemOffset, blCodeSize, regionId,
                LW_TRUE, LW_TRUE, LW_TRUE, pFlcnCfg));
        }
        else
#endif // BOOT_FROM_HS_BUILD
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr,
                  dst, blWprBase, blImemOffset, blCodeSize,
                  regionId, LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                                pLsbHeaderWrapper, (void *)&blDataOffset));

#ifdef BOOT_FROM_HS_BUILD
        if (g_hsFmcParams.bHsFmc)
        {
            blDataSize = 0; // HS FMC has no data
        }
        else
#endif // BOOT_FROM_HS_BUILD
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                                    pLsbHeaderWrapper, (void *)&blDataSize));
        }

        // Load data into DMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr,
            0, wprBase, blDataOffset, blDataSize,
            regionId, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));

#ifdef BOOT_FROM_HS_BUILD
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_HS_FMC_PARAMS,
                                                                            pLsbHeaderWrapper, &g_hsFmcParams));

        if (g_hsFmcParams.bHsFmc)
        {
            // Load the RTOS + App code to IMEM
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_IMEM_OFFSET,
                                                                                pLsbHeaderWrapper, &appImemOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET,
                                                                                pLsbHeaderWrapper, &appCodeOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE,
                                                                                pLsbHeaderWrapper, &appCodeSize));

            // Ensure that app code does not overlap BL code in IMEM
            if ((appImemOffset >= dst) || ((appImemOffset + appCodeSize) >= dst))
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(ACR_ERROR_ILWALID_ARGUMENT);
            }

            // Start the app/OS code DMA with the DMA base set beyond the BL, so the tags are right
            appCodebase = wprBase + (ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS) + (blCodeSize >> FLCN_IMEM_BLK_ALIGN_BITS);

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr,
                appImemOffset, appCodebase, (appCodeOffset - blCodeSize), appCodeSize, regionId,
                LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));

            // Load data for App.
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DMEM_OFFSET,
                                                                                pLsbHeaderWrapper, &appDmemOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET,
                                                                                pLsbHeaderWrapper, &appDataOffset));

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE,
                                                                                pLsbHeaderWrapper, &appDataSize));

            if (appDataSize != 0)
            {
                appDatabase = wprBase + (ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS) + (appDataOffset >> FLCN_IMEM_BLK_ALIGN_BITS);

                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr,
                    appDmemOffset, appDatabase, 0, appDataSize,
                    regionId, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));
            }

            //
            // The signature is to be setup in the target falcon right after the App data
            // appDmemOffset and appDataSize should be already aligned
            //
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupHsFmcSignatureValidation_HAL(pAcr, pFlcnCfg, &g_hsFmcParams, blImemOffset, blCodeSize, appDmemOffset + appDataSize));
        }
#endif // BOOT_FROM_HS_BUILD

#ifdef WAR_BUG_200670718
        if (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC)
        {
             CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramDmaBase_HAL(pAcr, pFlcnCfg, blWprBase));
        }
#endif

        // Set the BOOTVEC
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupBootvec_HAL(pAcr, pFlcnCfg, blImemOffset));
    }

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, flags))
    {
        acrSetupCtxDma_HAL(pAcr, pFlcnCfg, pFlcnCfg->ctxDma, LW_FALSE);
    }

    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, flags))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupDmaCtl_HAL(pAcr, pFlcnCfg, LW_TRUE));
    }
    else
    {
        acrSetupDmaCtl_HAL(pAcr, pFlcnCfg, LW_FALSE);
    }
    //
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acr is done loading the
    // code onto the falcon.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM));

    return status;
}

/*!
 * @brief _acrSetupNSFalcon:  Copies IMEM and DMEM into NS falcon
 *
 * @param[in] pWprBase   : Base addr of WPR Region
 * @param[in] regionId   : ACR Region ID
 * @param[in] pWprheader : WPR Header
 * @param[in] pFlcnCfg   : Falcon Config register
 * @param[in] pLsbHeader : LSB Header
 */
static ACR_STATUS
_acrSetupNSFalcon
(
    PRM_FLCN_U64             pWprBase,
    LwU32                    regionId,
    PLSF_WPR_HEADER_WRAPPER  pWprHeaderWrapper,
    PACR_FLCN_CONFIG         pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       addrHi;
    LwU32       addrLo;
    LwU32       wprBase;
    LwU32       blWprBase;
    LwU32       dst;
    LwU32       blCodeSize;
    LwU32       ucodeOffset;
    LwU32       blImemOffset;
    LwU32       blDataOffset;
    LwU32       blDataSize;
    LwU32       flags;
    LwU32       data;

    //
    // Need to program IMEM/DMEM PLMs before DMA load data to IMEM/DMEM.
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acr is done loading the
    // code onto the falcon.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrUpdateBootvecPlmLevel3Writeable_HAL(pAcr, pFlcnCfg));

    // Bug 3289559 , update Dmabase register PLM to Level3 for OFA , LWDEC1 and LWJPG falcons   
    if ((pFlcnCfg->falconId == LSF_FALCON_ID_OFA)   || 
        (pFlcnCfg->falconId == LSF_FALCON_ID_LWJPG) || 
        (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC1))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrUpdateDmaPlmLevel3Writeable_HAL(pAcr, pFlcnCfg));
    }

    //
    // Earlier wprOffset was added to lower address and then lower address was
    // checked for overflow. Gourav found that wproffset is always 0.
    // Hence we forced wprOffset as 0 in this task. We are still confirming with
    // Gobi, if there are cases where we can have wprOffset as non zero.
    //
    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    wprBase = (addrLo >> FLCN_IMEM_BLK_ALIGN_BITS) |
              (addrHi << (32 - FLCN_IMEM_BLK_ALIGN_BITS));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));
    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_FLAGS,
                                                                        pLsbHeaderWrapper, (void *)&flags));

    //
    // BL starts at offset, but since the IMEM VA is not zero, we need to make
    // sure FBOFF is equal to the expected IMEM VA. So adjusting the FB BASE to
    // make sure FBOFF equals to VA as expected.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

    blWprBase = ((wprBase) +
                 ((ucodeOffset) >> FLCN_IMEM_BLK_ALIGN_BITS)) -
                 ((blImemOffset) >> FLCN_IMEM_BLK_ALIGN_BITS);

    // Check if code needs to be loaded at start of IMEM or at end of IMEM
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, flags))
    {
        dst = 0;
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&blCodeSize));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindFarthestImemBl_HAL(pAcr, pFlcnCfg, blCodeSize, &dst));
        dst = dst * FLCN_IMEM_BLK_SIZE_IN_BYTES;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr, dst, blWprBase, blImemOffset, blCodeSize,
                                      regionId, LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blDataOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blDataSize));

    // Load data into DMEM
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr,
                                      0, wprBase, blDataOffset, blDataSize,
                                      regionId, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));

    // WAR Bug 3289559, ACR needs to Program DMA base to skip from RM side
    if ((pFlcnCfg->falconId == LSF_FALCON_ID_OFA)   || 
        (pFlcnCfg->falconId == LSF_FALCON_ID_LWJPG) || 
        (pFlcnCfg->falconId == LSF_FALCON_ID_LWDEC1))
    {
        acrProgramDmaBase_HAL(pAcr, pFlcnCfg, blWprBase);
    }

    // Set the BOOTVEC
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupBootvec_HAL(pAcr, pFlcnCfg, blImemOffset));

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, flags))
    {
        acrSetupCtxDma_HAL(pAcr, pFlcnCfg, pFlcnCfg->ctxDma, LW_FALSE);
    }

    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, flags))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupDmaCtl_HAL(pAcr, pFlcnCfg, LW_TRUE));
    }
    else
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupDmaCtl_HAL(pAcr, pFlcnCfg, LW_FALSE));
    }

    //
    // Note, this instruction must be at the end of function.
    // Only everything is programmed correctly; then we just enable CPUCTL_ALIAS_EN.
    // LWENC, LWDEC1 and OFA are all NS falcons, we just need to set CPUCTL_ALIAS_EN to true.
    //
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _TRUE, 0);
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data));

    return status;
}

/*!
 * @brief Prepares a RISC-V uproc for boot with new wpr blob defines.
 *
 * TODO: Update this function per the results of bugs 2720166 and 2720167.
 *
 * @param[in] pWprBase     Base address of WPR
 * @param[in] wprRegionID  ACR region ID
 * @param[in] pFlcnCfg     Falcon configuration
 * @param[in] pLsbHeader   LSB header
 */
ACR_STATUS
_acrSetupLSRiscvExt
(
    PRM_FLCN_U64                pWprBase,
    LwU32                       wprRegionID,
    PACR_FLCN_CONFIG            pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER     pLsbHeaderWrapper
)
{
    ACR_STATUS  status  = ACR_OK;
    LwU32       ucodeOffset;

    if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV)
    {
        return ACR_ERROR_ILWALID_OPERATION;
    }

    status = acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                               pLsbHeaderWrapper, (void *)&ucodeOffset);
    if (status != ACR_OK)
    {
        return status;
    }

    RM_FLCN_U64 physAddr = {0};
    physAddr.lo          = ucodeOffset;
    LwU64_ALIGN32_ADD(&physAddr, pWprBase, &physAddr);

    LwU64 wprAddr = 0;
    LwU64_ALIGN32_UNPACK(&wprAddr, &physAddr);
    status = acrSetupBootvecRiscvExt_HAL(pAcr, pFlcnCfg, wprRegionID, pLsbHeaderWrapper, wprAddr);

    return status;
}

/*!
 * @brief Prepares a RISCV uproc with external boot configuration.
 *
 * TODO: Update this function per the results of bugs 2720166 and 2720167.
 *
 * @param[in] pWprBase     Base address of WPR
 * @param[in] wprRegionID  ACR region ID
 * @param[in] pFlcnCfg     Falcon configuration
 * @param[in] pLsbHeader   LSB header
 */
ACR_STATUS
_acrSetupLSRiscvExternalBoot
(
    PRM_FLCN_U64             pWprBase,
    LwU32                    wprRegionID,
    PACR_FLCN_CONFIG         pFlcnCfg,
    PLSF_LSB_HEADER_WRAPPER  pLsbHeaderWrapper
)
{
    ACR_STATUS    status            = ACR_OK;
    LwU32         addrHi            = 0;
    LwU32         addrLo            = 0;
    LwU64         wprBase           = 0;
    LwU32         blCodeSize        = 0;
    LwU32         ucodeOffset       = 0;
    LwU32         blImemOffset      = 0;
    LwU32         blDataSize        = 0;
    LwU32         blDataOffset      = 0;
    RM_FLCN_U64   physAddr          = {0};
    LwU64         wprAddr           = 0;
    LwU32         dst               = 0;
    LwU32         data              = 0;
    LwU64         riscvAddrPhy      = 0;
    LwU32         bootItcmOffset    = 0;
    LwU32         i                 = 0;
    LwS32         timeoutLeftNs     = 0;
    ACR_TIMESTAMP startTimeNs;

    if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV_EB)
    {
        return ACR_ERROR_ILWALID_OPERATION;
    }

    // Reset RISCV and set core to RISCV
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPutFalconInReset_HAL(pAcr, pFlcnCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrBringFalconOutOfReset_HAL(pAcr, pFlcnCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCoreSelect_HAL(pAcr, pFlcnCfg));

    //
    // TODO: 1) imemPLM and dmemPLM should be set as L0 accessible in config
    //      2) Uncomment below code to program IMEM amd DMEM
    //
    // Need to program IMEM/DMEM PLMs before DMA load data to IMEM/DMEM.
    // Program the IMEM and DMEM PLMs to the final desired values (which may
    // be level 3 for certain falcons) now that acr is done loading the
    // code onto the falcon.
    //
    // acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    // acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);
    // CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrUpdateBootvecPlmLevel3Writeable_HAL(pAcr, pFlcnCfg));
    //

    for (i = 0; i < LW_PFALCON_FBIF_TRANSCFG__SIZE_1; i++)
    {
        acrProgramRegionCfg_HAL(pAcr, pFlcnCfg, i, LSF_WPR_EXPECTED_REGION_ID);
    }

    addrHi = pWprBase->hi;
    addrLo = pWprBase->lo;

    wprBase = ((addrLo >> FLCN_IMEM_BLK_ALIGN_BITS) |
              ((LwU64)addrHi << (32 - FLCN_IMEM_BLK_ALIGN_BITS)));

    // Read sw-bootrom code size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

    // For RISCV-EB, sw-bootrom size must NOT be zero.
    if (!blCodeSize)
    {
        return ACR_ERROR_SW_BOOTROM_SIZE_ILWALID;
    }

    blCodeSize = LW_ALIGN_UP(blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    // Write back aligned BL code size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blCodeSize));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&ucodeOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blImemOffset));

    // Load BL into the last several blcoks, finding enough space to store BL.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFindFarthestImemBl_HAL(pAcr, pFlcnCfg, blCodeSize, &dst));
    dst = dst * FLCN_IMEM_BLK_SIZE_IN_BYTES;

    // Load sw-bootrom code to IMEM
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr, dst, wprBase, (ucodeOffset + blImemOffset), blCodeSize,
                                      wprRegionID, LW_TRUE, LW_TRUE, LW_FALSE, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET,
                                                                        pLsbHeaderWrapper, (void *)&blDataOffset));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE,
                                                                        pLsbHeaderWrapper, (void *)&blDataSize));

    //
    // !!! Warning !!! lwrrently swbootrom data size is always zero, we don't validate below code yet.
    //      ________________________
    //     |   RISCV-EB DMEM layout |
    // 0   |------------------------|
    //     |                        |
    //     |    other's app data    |
    //     |                        |
    //     |------------------------|
    //     | swbootrom data blob    |
    //     | size is blDataSize     |
    //     | (or = swBromCodeSize)  |
    //     |------------------------|
    //     |  manifest data blob    |
    //     | size is manifest size  |
    // MAX |________________________|
    //
    if (blDataSize > 0)
    {
        LwU32 blDmemOffset;
        LwU32 manifestSize;
        LwU32 dmemSize;

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLsfLsbHeaderWrapperCtrl_HAL(pAcr, LSF_LSB_HEADER_COMMAND_GET_MANIFEST_SIZE,
                                                                            pLsbHeaderWrapper, (void *)&manifestSize));

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetDmemSize_HAL(pAcr, pFlcnCfg, &dmemSize));

        // callwlate BL DMEM offset per above DMEM layout.
        blDmemOffset = dmemSize - manifestSize - blDataSize;

        // Load sw-bootrom data to DMEM
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrIssueTargetFalconDma_HAL(pAcr, blDmemOffset, wprBase, blDataOffset, blDataSize,
                                          wprRegionID, LW_TRUE, LW_FALSE, LW_FALSE, pFlcnCfg));
    }

    // Set BCR priv protection level.
    data = ACR_REG_RD32(BAR0,  pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_READ_PROTECTION, _ALL_LEVELS_ENABLED, data);
    data = FLD_SET_DRF(_PRISCV, _RISCV_BCR_PRIV_LEVEL, _MASK_WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->riscvRegisterBase + LW_PRISCV_RISCV_BCR_PRIV_LEVEL_MASK, data);

#ifdef BOOT_FROM_HS_BUILD
    RM_FLCN_U64_UNPACK(&wprAddr, pWprBase);

    // WPR base is expected to be 256 bytes aligned
    wprAddr = wprAddr >> 8;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconCodeDataBaseInScratchGroup_HAL(pAcr, wprAddr, pLsbHeaderWrapper, pFlcnCfg));
#endif

    physAddr.lo  = ucodeOffset;
    LwU64_ALIGN32_ADD(&physAddr, pWprBase, &physAddr);
    LwU64_ALIGN32_UNPACK(&wprAddr, &physAddr);

    // This function just set BCR.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupBootvecRiscvExt_HAL(pAcr, pFlcnCfg, wprRegionID, pLsbHeaderWrapper, wprAddr));

    // Get IMEM size
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetImemSize_HAL(pAcr, pFlcnCfg, &data));
    bootItcmOffset = data - blCodeSize;
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRiscvGetPhysicalAddress_HAL(pAcr, LW_RISCV_MEM_IMEM, (LwU64)bootItcmOffset, &riscvAddrPhy));

    // External boot rom (aka. hw-bootrom) set LW_RISCV_MEM_IMEM address to boot vector registers.
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_RISCV_BOOT_VECTOR_HI, LwU64_HI32(riscvAddrPhy)));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_RISCV_BOOT_VECTOR_LO, LwU64_LO32(riscvAddrPhy)));

    // Start CPU
    acrFlcnRegLabelWrite_HAL(pAcr, pFlcnCfg, REG_LABEL_RISCV_CPUCTL, DRF_DEF(_PRISCV_RISCV, _CPUCTL, _STARTCPU, _TRUE));

    //
    // Poll For PRIV Lockdown to be enabled as TARGET_MASK 
    // could be released only when PRIV lockdown is enabled.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BR_RETCODE, &data));
    acrGetLwrrentTimeNs_HAL(pAcr, &startTimeNs);
    while(FLD_TEST_DRF( _PRISCV, _RISCV_BR_RETCODE, _RESULT , _INIT, data))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrCheckTimeout_HAL(pAcr, ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrFlcnRegRead_HAL(pAcr, pFlcnCfg, BAR0_RISCV, LW_PRISCV_RISCV_BR_RETCODE, &data));    
    } 
   
    return status;
}

/*!
 * @brief acrPassRandomNumberToEngine_GH100
 *        Generate a random number using GSP SE-lite/libCCC and store it in the scratch
 *        allocated to the respective engine. This function will only be called for
 *        some engines. The Changes are done as requirements for BUG 3395727, 3400512
 *
 * @param[in]  falconId   Falcon Id
 * @param[in]  pFlcnCfg   Falcon configuration struct
 *
 * @return ACR_OK                     :  If operation succeeeds
 *         ACR_ERROR_ILWALID_ARGUMENT :  If invalid argument is passed
 */
ACR_STATUS
acrPassRandomNumberToEngine_GH100
(
    LwU32               falconId,
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS                 status             = ACR_OK;
    LwU8                       scratchGroupIndex  = 0;
    LwU32                      sourceMask         = 0;
    LwU32                      readMask           = 0;
    LwU32                      writeMask          = 0;
    LwU32                      plmVal             = 0;
    LwU32                      plmAddr            = 0;
    LwU32                      regAddr            = 0;
    LwU32                      randomVal          = 0;
    LwU8                       randomNumBuffer[ACR_RANDOM_NUMBER_BUFFER_MAX_SIZE_BYTE];
#ifndef ACR_FMODEL_BUILD
    status_t                   statusCcc          = ERR_GENERIC;    
    struct se_data_params      seInputParams      = {0};
    se_engine_rng_context_t    seEngineContext    = {0};
    engine_t                   *pEngineStruct     = NULL;
#endif // ACR_FMODEL_BUILD

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    LwU32                      engInstance        = pFlcnCfg->falconInstance;

    // Clear the buffer as a preventive measure
    acrMemset_HAL(pAcr, randomNumBuffer, 0x0, ACR_RANDOM_NUMBER_BUFFER_MAX_SIZE_BYTE);

#ifndef ACR_FMODEL_BUILD
    // Select the engine and the appropriate classes
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(ccc_select_engine((const engine_t **)&pEngineStruct, 
                                                                               ENGINE_CLASS_AES, 
                                                                               ENGINE_SE0_AES1), 
                                                                               ACR_ERROR_LWPKA_SELECT_ENGINE_FAILED);
    // Configure the parameters
    seEngineContext.engine     = pEngineStruct;
    seInputParams.dst          = randomNumBuffer;
    seInputParams.output_size  = 4U * pFlcnCfg->sizeOfRandNum;

    // Fetch the random Number in the buffer
    CHECK_STATUS_CCC_AND_RETURN_IF_NOT_OK(engine_genrnd_generate_locked(&seInputParams, &seEngineContext), 
                                                                       ACR_ERROR_SE_RNG_OP_NUMBER_GENERATION_FAILED);
#else
    //  
    // We got ERR_NOT_ALLOWED error from engine_genrnd_generate_locked API in G000 fmodel working.
    // Passing the following magic number as a fix for this.
    // TODO @chetang : Drop this else case and pass the random number generated from API itself
    // to fmodel profiles as well when the error case is resolved.
    //
    *(LwU32 *)(randomNumBuffer) = (LwU32)ACR_FIXED_MAGIC_NUM;
#endif // ACR_FMODEL_BUILD
                                                                     
    // Configure the PLM settings
    switch (falconId)
    {
        case LSF_FALCON_ID_FECS:
            plmAddr  = LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_CTXSW_SELWRE_SCRATCH_GROUP1_PRIV_LEVEL_MASK, engInstance);
            break;

        case LSF_FALCON_ID_GPCCS:
            plmAddr  = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_CTXSW_SELWRE_SCRATCH_GROUP1_PRIV_LEVEL_MASK, engInstance);
            break;

        default:
            plmAddr  = pFlcnCfg->registerBase + LW_PFALCON_FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK;
            break;
    }

    if ( pFlcnCfg->bIsNsFalcon)
    {
        readMask   = LW_PFALCON_FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED;
        writeMask  = LW_PFALCON_FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED;
    }
    else
    {
        readMask   = LW_PFALCON_FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED;
        writeMask  = LW_PFALCON_FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2_ENABLED;
    }

    sourceMask = BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP) | BIT(LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LOCAL_SOURCE_FALCON);    

    // Update the PLM fields 
    plmVal = ACR_REG_RD32(BAR0, plmAddr);
    plmVal = FLD_SET_DRF_NUM(_PFALCON, _FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK, _READ_PROTECTION, readMask, plmVal);
    plmVal = FLD_SET_DRF_NUM(_PFALCON, _FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK, _WRITE_PROTECTION, writeMask, plmVal);
    plmVal = FLD_SET_DRF_NUM(_PFALCON, _FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK, _SOURCE_ENABLE, sourceMask, plmVal);
    plmVal = FLD_SET_DRF(_PFALCON, _FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED, plmVal);
    plmVal = FLD_SET_DRF(_PFALCON, _FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED, plmVal);
    plmVal = FLD_SET_DRF(_PFALCON, _FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR, plmVal);
    plmVal = FLD_SET_DRF(_PFALCON, _FALCON_COMMON_SCRATCH_GROUP_3_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR, plmVal);

    ACR_REG_WR32(BAR0, plmAddr, plmVal); 

    // Write the random number to specified scratch
    switch (falconId)
    {
        case LSF_FALCON_ID_FECS:
            randomVal = *(LwU32 *)(randomNumBuffer);
            regAddr   =  LW_GET_PGRAPH_SMC_REG(LW_PGRAPH_PRI_FECS_CTXSW_SELWRE_SCRATCH_GROUP1(1), engInstance);
            ACR_REG_WR32(BAR0, regAddr, randomVal);
            break;
    
        case LSF_FALCON_ID_GPCCS:
            randomVal = *(LwU32 *)(randomNumBuffer);
            regAddr   = SMC_LEGACY_UNICAST_ADDR(LW_PGRAPH_PRI_GPC0_GPCCS_CTXSW_SELWRE_SCRATCH_GROUP1(1), engInstance); 
            ACR_REG_WR32(BAR0, regAddr, randomVal);
            break;

        default:
            for (scratchGroupIndex = 0; scratchGroupIndex < pFlcnCfg->sizeOfRandNum; scratchGroupIndex++)
            {
                randomVal = *(LwU32 *)(randomNumBuffer + (4 * scratchGroupIndex));
                regAddr   = (pFlcnCfg->registerBase) + (LwU32)LW_PFALCON_FALCON_COMMON_SCRATCH_GROUP_3(scratchGroupIndex);
                ACR_REG_WR32(BAR0, regAddr, randomVal);
            }
            break;
    }

    // Clear the buffer to erase the random number
    acrMemset_HAL(pAcr, randomNumBuffer, 0x0, ACR_RANDOM_NUMBER_BUFFER_MAX_SIZE_BYTE);

    return status;
}
