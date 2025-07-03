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
 * @file: acr_rv_boott234.c
 */

#include "rv_acr.h"

#include <lwtypes.h>
#include <liblwriscv/io.h>
#include <liblwriscv/io_pri.h>
#include <liblwriscv/dma.h>
#include <liblwriscv/libc.h>

#include "dev_fb.h"
#include "dev_pwr_pri.h"
#include "dev_bus.h"
#include "dev_pwr_csb.h"
#include "dev_falcon_v4.h"

/* Static Functions */
#if defined(ACR_SIG_VERIF)
static ACR_STATUS    acrScrubContentAndWriteWpr(PLSF_LSB_HEADER_V2 pLsfHeader);
static inline LwU8 * acrGetUcodeSignature(PLSF_LSB_HEADER_V2 pLsfHeader, LwU32 sectionIndex) GCC_ATTRIB_ALWAYSINLINE();
#endif

/*! g_bSetReqCtx Global variable to store if REQUIRE_CTX needs to be set or not for falcons. */
LwBool            g_bSetReqCtx         ATTR_OVLY(".data") = LW_FALSE;
/*! g_bIsDebug Global variable to store the STATUS of the Debug Signal going to the SCP for the board on which current binary is exelwting */
LwBool            g_bIsDebug           ATTR_OVLY(".data") = LW_FALSE;
/*! xmemStore Global variable to store ucode contents data/code */
LwU32             xmemStore[FLCN_IMEM_BLK_SIZE_IN_BYTES/LW_SIZEOF32(LwU32)] ATTR_OVLY(".data") ATTR_ALIGNED(FLCN_IMEM_BLK_SIZE_IN_BYTES);
LSF_LSB_HEADER_V2 lsbHeaders;

/*! GET_WPR_HEADER Macro to get desired WPR header using index(ind) as argument from global buffer "g_pWprHeader" containing all WPR headers. */
#define GET_WPR_HEADER(ind)          ((g_pWprHeader) + (ind))
#define right_shift_8bits(v)    (v >> 8U)
#define left_shift_8bits(v)     (v << 8U)

/*!
 * @brief ACR routine to check if falcon is in DEBUG mode or not\n
 *        Check by reading DEBUG_MODE bit of LW_PRGNLCL_SCP_CTL_STAT.
 *
 * @return TRUE if debug mode
 * @return FALSE if production mode
 *
 */
LwBool
acrIsDebugModeEnabled_T234(void)
{
    LwU32   scpCtl = 0;

    scpCtl = localRead(LW_PRGNLCL_SCP_CTL_STAT);

    return !FLD_TEST_DRF( _PRGNLCL, _SCP_CTL_STAT, _DEBUG_MODE, _DISABLED, scpCtl);
}

/*!
 * @brief Polls for Falcon reset to complete.
 *        Make sure not to reset bootstrapping owner falcon i.e. PMU falcon here.\n
 *        Call acrlibPollForScrubbing_HAL which polls for scrubbing to complete.\n
 *
 * @param[in] validIndexMap  Mask of all valid falcon IDs
 *
 * @return ACR_OK If reset completes successfully.
 * @return ACR_ERR_TIMEOUT If Poll for scrubbing timeouts.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If supported falcon Id is not found in WPR headers.
 *
 */
ACR_STATUS
acrPollForResetCompletion_T234
( 
    LwU32        validIndexMap
)
{
    ACR_STATUS      status       = ACR_OK;
    LwU32           index        = 0;
    PLSF_WPR_HEADER pWprHeader   = LW_NULL;
    ACR_FLCN_CONFIG flcnCfg;

    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        if ((BIT32(index) & validIndexMap) == 0U)
        {
            continue;
        }

        pWprHeader = GET_WPR_HEADER(index);

        // Dont reset if falcon id is bootstrap owner
        if (pWprHeader->falconId == LSF_FALCON_ID_PMU_RISCV)
        {
            continue;
        }

        // Get the specifics of this falconID
        if (acrlibGetFalconConfig_HAL(pWprHeader->falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg) 
                                     == ACR_ERROR_FLCN_ID_NOT_FOUND)
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

        //
        // Poll for scrubbing to complete
        // TODO: Do polling for all falcons at once. Saves perf.
        //
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPollForScrubbing_HAL(&flcnCfg)); 
    }

    return status;
}

/*!
 * @brief Resets engine and falcon if applicable. For some engines,
 *        only falcon can be reset and not the entire unit. Also waits
 *        for scrubbing to complete. Donot reset if falcon id is bootstrap owner.
 *
 * @param[in] validIndexMap List of valid falcon IDs
 * @return  ACR_OK If falcon reset engine completes successfully.
 * @return  ACR_ERR_TIMEOUT If falcon reset engine timesout due to scrubbing not complete.
 * @return  ACR_ERROR_FLCN_ID_NOT_FOUND If supported falcon Id is not found in WPR headers.
 *
 */
ACR_STATUS
acrResetEngineFalcon_T234
(
    LwU32        validIndexMap
)
{
    ACR_STATUS       status          = ACR_OK;
    LwU32            index           = 0;
    PLSF_WPR_HEADER  pWprHeader      = LW_NULL;
    ACR_FLCN_CONFIG  flcnCfg;

    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        if ((BIT32(index) & validIndexMap) == 0U)
        {
            continue;
        }
        pWprHeader = GET_WPR_HEADER(index);

        // Dont reset if falcon id is bootstrap owner
        if (acrlibIsBootstrapOwner_HAL(pWprHeader->falconId))
        {
            continue;
        }

        // Get the specifics of this falconID
        if (acrlibGetFalconConfig_HAL(pWprHeader->falconId,
                                LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg)
                                          == ACR_ERROR_FLCN_ID_NOT_FOUND)
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibResetFalcon_HAL(&flcnCfg, LW_FALSE));
    }

    return status;
}

/*!
 * @brief Reads LSB header into a global buffer which is then copied to local buffer pointed out by pLsbHeader.
 *        Global buffer is used to match the required alignment as enforced by falcon DMA engine. This is achieved by calling IssueDma_HAL().
 *
 * @param[in]  pWprHeader WPR header for a particular falcon ID of interest
 * @param[out] pLsbHeader Pointer to global buffer holding LSB header contents.
 *
 * @return ACR_OK Reading of LSB header is successful from WPR.
 * @return ACR_ERROR_DMA_FAILURE Reading of LSB header fails due to DMA failure.
 *
 */
static ACR_STATUS
acrReadLsbHeader_T234
(
    PLSF_WPR_HEADER    pWprHeader,
    PLSF_LSB_HEADER_V2 pLsbHeader
)
{
    LwU32 lsbHeaderSize = sizeof(LSF_LSB_HEADER_V2);

    // Read the LSB  header
    if (dmaPa((LwU64)pLsbHeader, (g_dmaProp.wprBase +
                                (LwU64)pWprHeader->lsbOffset), lsbHeaderSize, 
                                (LwU8)g_dmaProp.ctxDma, LW_TRUE) != LWRV_OK)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}

/*!
 * @brief Function which will look if Bootstrap owner ucode is present in WPR.
 *        Loop through each WPR header for all valid falcon ids and break the loop when falcon id matches with LWRRENT_BOOTSTRAP_OWNER_ID
 *
 * @param[out] Pointer to if bootstrap owner present bool flag  
 */
static void isBootstrapOwnPresent(LwBool *bootstrapOwnPresent)
{
    LwU32 index;
    PLSF_WPR_HEADER         pWprHeader;

    *bootstrapOwnPresent = LW_FALSE;
    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index=0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            break;
        }
        if (acrlibIsBootstrapOwner_HAL( pWprHeader->falconId) == LW_TRUE)
        {
            *bootstrapOwnPresent = LW_TRUE;
            break;
        }
    }
    return;
}

#ifdef ACR_SIG_VERIF
/*
 * @brief Scrub the code and data content on error scenarios. Also write the WPR header back to WPR.
 *        g_scrubState.zeroBuf global zero buffer is used to scrub code & data of falcon. Scrubbing is achieved by DMAing zero buffer to WPR region.
 *
 * @param[in] pLsfHeader LSB header.
 *
 * @return ACR_OK If scrubbing of content i.e. code/data and writing back of WPR headers is successful.
 * @return ACR_ERROR_DMA_FAILURE If scrubbing of data/code fails due to DMA failure. 
 */
static
ACR_STATUS acrScrubContentAndWriteWpr(PLSF_LSB_HEADER_V2 pLsfHeader)
{
    ACR_STATUS status = ACR_OK;

    // Scrub code
    if ((dmaPa((LwU64)g_scrubState.zeroBuf,
        (g_dmaProp.wprBase) + pLsfHeader->ucodeOffset,
        pLsfHeader->ucodeSize,
        (LwU8)g_dmaProp.ctxDma, LW_FALSE)) != LWRV_OK)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Scrub data   
    if ((dmaPa((LwU64)g_scrubState.zeroBuf,
        (g_dmaProp.wprBase) + pLsfHeader->ucodeOffset + pLsfHeader->ucodeSize,
        pLsfHeader->dataSize,
        (LwU8)g_dmaProp.ctxDma, LW_FALSE)) != LWRV_OK)
    {
        return ACR_ERROR_DMA_FAILURE;
    } 

    // Write WPR header back to FB and return...
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrWriteWprHeader_HAL());
    return status;
}
#endif

/*!
 * @brief Setup IMEMC/DMEMC to configure IMEM/DMEM for writing based on whether transfer request target is IMEM or DMEM.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM 
 * @param[in] pFlcnCfg     Falcon config
 * @param[in] bIsDstImem TRUE if destination is IMEM
 *
 */
static void
acrSetupMemConfigRegisters
(
    PACR_FLCN_CONFIG      pFlcnCfg,
    LwU32                 dstOff,
    LwBool                bIsDstImem
)
{
    if (bIsDstImem)
    {
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_IMEMC(0U),
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_OFFS, 0U) |
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_BLK, dstOff >> 8) | 
                               REF_NUM(LW_PFALCON_FALCON_IMEMC_AINCW, 1U));
    }
    else
    {
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMEMC(0U),
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_OFFS, 0U) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_BLK, dstOff >> 8) |
                               REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, 1U));
    }
}

/*!
 * @brief Use priv access/writes to load target falcon memory.
 *        -# Support loading into both IMEM and DMEM of target falcon from WPR.
 *        -# Expect size to be 256 multiple.
 *        -# Make sure that size of ucode to be transferred is not greater than target falcon's IMEM/DMEM size.
 *        -# Setup IMEM/DMEM for writing by calling acrSetupMemConfigRegisters.
 *        -# Loop till (bytesXfered < sizeInBytes) and DMA one block from SYSMEM(WPR) to xmemStore global storage.
 *        -# xmemStore is then used to write to IMEM/DMEM by using IMEMD/DMEMD register.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM
 * @param[in] fbOff        Offset from wprBase
 * @param[in] sizeInBytes  Number of bytes to be transferred
 * @param[in] regionID     ACR region ID to be used for this transfer
 * @param[in] bIsDstImem   TRUE if destination is IMEM
 * @param[in] pFlcnCfg     Falcon config
 *
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW  If variable size overflow is detected.
 * @return ACR_ERROR_TGT_DMA_FAILURE  If dstOff, fbOff and sizeInBytes is not aligned to FLCN_IMEM_BLK_SIZE_IN_BYTES.
 * @return ACR_ERROR_DMA_FAILURE  If DMA failure is seen.
 * @return ACR_ERROR_UNEXPECTED_ARGS if size of ucode to be loaded on target falcon exceeds its IMEM/DMEM size.
 * @return ACR_OK  If acrPrivLoadTargetFalcon_GM200 successfully exelwtes.
 */
static ACR_STATUS
acrPrivLoadTargetFalcon_T234
(
    LwU32               dstOff,
    LwU32               fbOff,
    LwU32               sizeInBytes,
    LwU32               regionID,
    LwBool              bIsDstImem,
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS  status = ACR_OK;
    LwU32       bytesXfered   = 0;
    LwU32       tag;
    LwU32       i;
    LwBool      bIsSizeOk     = LW_FALSE; 

    //
    // Sanity Checks
    // Only does 256B Transfer
    //
    if ((!VAL_IS_ALIGNED(sizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES)) ||
        (!VAL_IS_ALIGNED(dstOff, FLCN_IMEM_BLK_SIZE_IN_BYTES))      ||
        (!VAL_IS_ALIGNED(fbOff, FLCN_IMEM_BLK_SIZE_IN_BYTES)))
    {
        return ACR_ERROR_TGT_DMA_FAILURE;
    }

    bIsSizeOk = acrlibCheckIfUcodeFitsFalcon_HAL(pFlcnCfg, sizeInBytes, bIsDstImem);
    if (bIsSizeOk == LW_FALSE)
    {
        return ACR_ERROR_UNEXPECTED_ARGS;
    }

    // Setup the MEMC Register
    acrSetupMemConfigRegisters(pFlcnCfg, dstOff, bIsDstImem);

    // Start the DMA and bitbang the ucode.
    tag = 0;
    while (bytesXfered < sizeInBytes)
    {
        // Read the next block
        if((dmaPa((LwU64)xmemStore, (g_dmaProp.wprBase) + (LwU64)(fbOff + bytesXfered), 
            FLCN_IMEM_BLK_SIZE_IN_BYTES, (LwU8)g_dmaProp.ctxDma, LW_TRUE))
            != LWRV_OK)
        {
            return ACR_ERROR_DMA_FAILURE;
        }
 
        if (bIsDstImem)
        {
            // Setup the tags for the instruction memory.
            acrlibFlcnRegWrite_T234(pFlcnCfg, BAR0_FLCN,
                                LW_PFALCON_FALCON_IMEMT(0U),
                                REF_NUM(LW_PFALCON_FALCON_IMEMT_TAG, tag));
            for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / LW_SIZEOF32(LwU32); i++)
            {
                acrlibFlcnRegWrite_T234(pFlcnCfg, BAR0_FLCN,
                                LW_PFALCON_FALCON_IMEMD(0U), xmemStore[i]);
            }
            tag++;
            CHECK_WRAP_AND_ERROR(tag == LW_U16_MAX + 1U);
        }
        else
        {
            for (i = 0; i < FLCN_IMEM_BLK_SIZE_IN_BYTES / LW_SIZEOF32(LwU32); i++)
            {
                acrlibFlcnRegWrite_T234(pFlcnCfg, BAR0_FLCN,
                                LW_PFALCON_FALCON_DMEMD(0U), xmemStore[i]);
            }
        }
        bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;

    }

    return status;
}
 
static
ACR_STATUS acrSetupLSPMUBrom_T234(PLSF_LSB_HEADER_V2  pLsbHeader)
{

    RISCV_PMU_DESC desc;
    LwU64 fmc_code_addr = 0;
    LwU64 fmc_data_addr = 0;
    LwU64 manifest_addr = 0;
    LwU32 ret;

    ret = dmaPa((LwU64)&desc, g_desc.pmuUcodeDescBase,
                sizeof(RISCV_PMU_DESC), RM_PMU_DMAIDX_PHYS_VID, LW_TRUE);
    if (ret != LWRV_OK)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    priWrite(LW_PPWR_RISCV_BCR_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L2_WRITE_L2);
    priWrite(LW_PPWR_RISCV_BCR_DMACFG_SEC_PRIV_LEVEL_MASK,
                                                ACR_PLMASK_READ_L2_WRITE_L2);

    fmc_code_addr = right_shift_8bits((g_dmaProp.wprBase +
                    (LwU64)pLsbHeader->ucodeOffset + desc.monitor_code_offset));

    fmc_data_addr = right_shift_8bits((g_dmaProp.wprBase +
                    (LwU64)pLsbHeader->ucodeOffset + desc.monitor_data_offset));

    manifest_addr = right_shift_8bits((g_dmaProp.wprBase +
                    (LwU64)pLsbHeader->ucodeOffset + desc.manifest_offset));

    priWrite(LW_PPWR_RISCV_BCR_DMAADDR_FMCCODE_LO, lwU64Lo32(fmc_code_addr));
    priWrite(LW_PPWR_RISCV_BCR_DMAADDR_FMCCODE_HI, lwU64Hi32(fmc_code_addr));
 
    priWrite(LW_PPWR_RISCV_BCR_DMAADDR_FMCDATA_LO, lwU64Lo32(fmc_data_addr));
    priWrite(LW_PPWR_RISCV_BCR_DMAADDR_FMCDATA_HI, lwU64Hi32(fmc_data_addr));
 
    priWrite(LW_PPWR_RISCV_BCR_DMAADDR_PKCPARAM_LO, lwU64Lo32(manifest_addr));
    priWrite(LW_PPWR_RISCV_BCR_DMAADDR_PKCPARAM_HI, lwU64Hi32(manifest_addr));
 
    priWrite(LW_PPWR_RISCV_BCR_DMACFG_SEC, (g_desc.wprRegionID << 16U)| g_desc.wprRegionID);
    priWrite(LW_PPWR_RISCV_BCR_DMACFG, DRF_DEF(_PPWR_RISCV, _BCR_DMACFG, _TARGET,
                                             _NONCOHERENT_SYSMEM) |
                                       DRF_DEF(_PPWR_RISCV, _BCR_DMACFG, _LOCK,
                                             _LOCKED));
 
    priWrite(LW_PPWR_RISCV_BCR_CTRL,
                        DRF_DEF(_PPWR_RISCV, _BCR_CTRL, _VALID, _TRUE) |
                        DRF_DEF(_PPWR_RISCV, _BCR_CTRL, _CORE_SELECT, _RISCV) |
                        DRF_DEF(_PPWR_RISCV, _BCR_CTRL, _BRFETCH, _TRUE));

    return ACR_OK;
}

/*!
 * @brief Setup Falcon in LS mode. This ilwolves
 *        -# Check if the code validation completed before going forward.
 *        -# Setup LS Falcon registers by calling acrSetupLSFalcon_HAL.
 *        -# Override the value of IMEM/DMEM PLM to final value for the falcons being booted by this level 3 binary.
 *        -# Check if code needs to be loaded at start of IMEM or at end of IMEM using LSF flag.
 *        -# If the code needs to be loaded at end of IMEM, find farthest IMEM block from where loading can start using acrlibFindFarthestImemBl_HAL.
 *        -# BASED on LSF header's "FORCE_PRIV_LOAD" flag, load LS falcon's IMEM/DMEM with falcon code/data by PRIV writes i.e. acrPrivLoadTargetFalcon_GM200 or by DMA i.e. acrlibIssueTargetFalconDma_HAL.
 *        -# Read back code/data written to LS falcon's IMEM/DMEM and compare it with WPR contents.
 *        -# Setup/Program BOOTVEC for falcon.
 *        -# Check if Falcon wants virtual ctx based on LSF flag and set accordingly.
 *        -# Update WPR header status to "LSF_IMAGE_STATUS_BOOTSTRAP_READY" and return. 
 *
 * @param[in] pLsbHeader  LSB header
 * @param[in] validIndexMap List of valid falcon IDs
 *
 * @return ACR_ERROR_DMA_FAILURE In case there is DMA failure.
 * @return ACR_OK If acrSetupLSFalcon_GM200 completes successfully.  
 * @return ACR_ERROR_VARIABLE_SIZE_OVERFLOW  If variable size overflow is detected.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If supported falcon Id is not found in WPR headers.
 */
ACR_STATUS
acrSetupLSFalcon_T234
(
    PLSF_LSB_HEADER_V2 pLsbHeader,
    LwU32              validIndexMap
)
{
    ACR_STATUS          status     = ACR_OK;
    LwU32               index      = 0;
    PLSF_WPR_HEADER     pWprHeader;
    ACR_FLCN_CONFIG     flcnCfg;
    LwU64               blWprBase;
    LwU64               blWprOffset;
    LwU32               farthestImemBl;
    LwU32               dst                   = 0;
    LwU32               targetMaskPlmOldValue = 0;
    LwU32               targetMaskOldValue    = 0;

    for (index = 0; index < LSF_FALCON_ID_END; index++)
    {
        if ((BIT32(index) & validIndexMap) == 0U)
        {
            continue;
        }

        pWprHeader = GET_WPR_HEADER(index);

        // Get the specifics of this falconID
        if (acrlibGetFalconConfig_HAL(pWprHeader->falconId,
                    LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg) 
                                       == ACR_ERROR_FLCN_ID_NOT_FOUND)
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

        memset(&lsbHeaders, 0, sizeof(LSF_LSB_HEADER_V2));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeader_T234(pWprHeader,
                                                                pLsbHeader));

        //
        // Lock falcon reg space.
        if (pWprHeader->falconId != LSF_FALCON_ID_GSP_RISCV &&
                    pWprHeader->falconId != LSF_FALCON_ID_PMU_RISCV)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(LSF_FALCON_ID_GSP_RISCV, &flcnCfg,
                                  &targetMaskPlmOldValue, &targetMaskOldValue, LW_TRUE));
        }

        if (pWprHeader->falconId == LSF_FALCON_ID_PMU_RISCV)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupLSPMUBrom_T234(pLsbHeader));
            acrSetupSctlDecodeTrap_HAL();
            continue;
        }

        CHECK_WRAP_AND_ERROR(pLsbHeader->blCodeSize >
                                (LW_U32_MAX - FLCN_IMEM_BLK_SIZE_IN_BYTES));
        pLsbHeader->blCodeSize = LW_ALIGN_UP(pLsbHeader->blCodeSize,
                                                FLCN_IMEM_BLK_SIZE_IN_BYTES);
        if (pWprHeader->falconId != LSF_FALCON_ID_PMU_RISCV)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupTargetRegisters_HAL(&flcnCfg));

            // Override the value of IMEM/DMEM PLM to final value for the falcons being booted by this level 3 binary
            acrlibFlcnRegLabelWrite_HAL(&flcnCfg,
                        REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, flcnCfg.imemPLM);
            acrlibFlcnRegLabelWrite_HAL(&flcnCfg,
                        REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, flcnCfg.dmemPLM);
        }
        //
        // Load code into IMEM
        // BL starts at offset, but since the IMEM VA is not zero, we need to make
        // sure FBOFF is equal to the expected IMEM VA. So adjusting the FBBASE to make
        // sure FBOFF equals to VA as expected.
        //

        blWprOffset = (LwU64)pLsbHeader->ucodeOffset -
                                            (LwU64)pLsbHeader->blImemOffset;

        blWprBase = ((g_dmaProp.wprBase >> 8U) +
                            ((LwU64)blWprOffset >> FLCN_IMEM_BLK_ALIGN_BITS));
        CHECK_WRAP_AND_ERROR(blWprBase < (g_dmaProp.wprBase >> 8U));

        // Check if code needs to be loaded at start of IMEM or at end of IMEM
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE,
                                                            pLsbHeader->flags))
        {
            dst = 0;
        }
        else
        {
                if (pWprHeader->falconId != LSF_FALCON_ID_PMU_RISCV) {
                    farthestImemBl = acrlibFindFarthestImemBl_HAL(&flcnCfg,
                                                    pLsbHeader->blCodeSize);
                    CHECK_WRAP_AND_ERROR(farthestImemBl >
                                        LW_U32_MAX/FLCN_IMEM_BLK_SIZE_IN_BYTES);
                    dst = (farthestImemBl * FLCN_IMEM_BLK_SIZE_IN_BYTES);
                }
        }

        if(pWprHeader->falconId != LSF_FALCON_ID_PMU_RISCV)
        {
            if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _FORCE_PRIV_LOAD, _TRUE,
                                                            pLsbHeader->flags))
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPrivLoadTargetFalcon_T234(0,
                        pLsbHeader->ucodeOffset + pLsbHeader->appCodeOffset,
                        pLsbHeader->appCodeSize,
                        g_dmaProp.regionID, LW_TRUE, &flcnCfg))

                // Load data into DMEM
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPrivLoadTargetFalcon_T234(0,
                        pLsbHeader->ucodeOffset + pLsbHeader->appDataOffset,
                        pLsbHeader->appDataSize,
                        g_dmaProp.regionID, LW_FALSE, &flcnCfg))

                // Set the BOOTVEC
                acrlibSetupBootvec_HAL(&flcnCfg, 0);
            }
            else
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(
                        dst, blWprBase, pLsbHeader->blImemOffset,
                        pLsbHeader->blCodeSize, g_dmaProp.regionID,
                        LW_TRUE, LW_TRUE, &flcnCfg))

                // Load data into DMEM
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibIssueTargetFalconDma_HAL(
                        0, (g_dmaProp.wprBase >> 8U), pLsbHeader->blDataOffset,
                        pLsbHeader->blDataSize,
                        g_dmaProp.regionID, LW_TRUE, LW_FALSE, &flcnCfg))

                // Set the BOOTVEC
                acrlibSetupBootvec_HAL(&flcnCfg, pLsbHeader->blImemOffset);
            }
        }

        // Check if Falcon wants virtual ctx
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE,
                                                            pLsbHeader->flags))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupCtxDma_HAL(&flcnCfg,
                                                    flcnCfg.ctxDma, LW_FALSE));
        }
        else
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupCtxDma_HAL(&flcnCfg,
                                                    flcnCfg.ctxDma, LW_TRUE));
        }

        g_bSetReqCtx = LW_FALSE;
        // Check if Falcon wants REQUIRE_CTX
        if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE,
                                                            pLsbHeader->flags))
        {
            g_bSetReqCtx = LW_TRUE;
        }
        //
        // For the falcon which is lwrrently exelwting this, set REQUIRE_CTX only after
        // all DMAs are completed
        //
        if (!acrlibIsBootstrapOwner_HAL(pWprHeader->falconId))
        {
            acrlibSetupDmaCtl_HAL(&flcnCfg, g_bSetReqCtx);
        }

        // Set the status in WPR header
        pWprHeader->status = LSF_IMAGE_STATUS_BOOTSTRAP_READY;

        //
        // Free falcon reg space
        //
        if (pWprHeader->falconId != LSF_FALCON_ID_GSP_RISCV &&
                    pWprHeader->falconId != LSF_FALCON_ID_PMU_RISCV)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(LSF_FALCON_ID_GSP_RISCV, &flcnCfg,
                                  &targetMaskPlmOldValue, &targetMaskOldValue, LW_FALSE));
        }
    }

    return status;
}

#ifdef ACR_SIG_VERIF
/*!
 * @brief Fetch ucode signatures based on Index as well as board type i.e DEBUG/PROD.
 *
 * @param[in]  pLsfHeader   Pointer to LSF header for falcon.
 * @param[in]  sectionIndex Value indicating which data section the signatures are to be fetched for.
 *
 * @return pSignature Pointer to the corresponding ucode signature.
 */
static inline LwU8 *
acrGetUcodeSignature
(
    PLSF_LSB_HEADER_V2 pLsfHeader,
    LwU32              sectionIndex
)
{
    LwU8   *pSignature;

    if (g_bIsDebug) {
        pSignature = pLsfHeader->signature.lsfUcodeDescV2.debugSig[sectionIndex];
    } else {
        pSignature = pLsfHeader->signature.lsfUcodeDescV2.prodSig[sectionIndex];
    }

    return pSignature;
}

/*!
 * @brief Function which will take care of validating/authenticating the LSF ucodes.
 *        -# Mark LSF IMAGE status to IMAGE_COPY. It means LSF IMAGE for a particular falcon ID as specified by loop is present in WPR(i.e. successfully copied).
 *        -# pLsfHeader->signature LSF header contains signatures for both debug/productions type of boards.
 *        -# Fetch ucode signatures by calling acrGetUcodeSignatures.
 *        -# Validate/Authenticate data/code part of LS image by calling acrVerifySignature_HAL. 
 *        -# Check for LS ucode revocation by using falconBilwersions by calling acrCheckForRevocation_HAL.
 *        -# In case of LS ucode signature validation/Revocation failure, scrub WPR and return with error to calling function.  
 *        -# Mark status in LSF image header as LSF_IMAGE_STATUS_VALIDATION_DONE if validation of signature and revocation check passes.
 * 
 * @param[in] pWprHeader WPR header for a particular falcon ID of interest.
 * @param[in] pLsfHeader Pointer to LSF header for falcon.
 * 
 * @return ACR_OK                 If functions compeletes without any failures and loading of LS falcons is complete.
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If WPR Header does not belong to any PMU or GR falcons.
 */
static ACR_STATUS
acrAuthenticateLSFUcodes
(
    PLSF_WPR_HEADER         pWprHeader,
    PLSF_LSB_HEADER_V2      pLsfHeader,
    PLSF_UCODE_DESC_V2      pUcodeDesc    
)
{
    ACR_STATUS   status        = ACR_OK;
    ACR_STATUS   oldstatus     = ACR_OK;
    pWprHeader->status = LSF_IMAGE_STATUS_COPY;

    if (pWprHeader->falconId == LSF_FALCON_ID_PMU_RISCV || (pWprHeader->falconId == LSF_FALCON_ID_FECS) ||
        (pWprHeader->falconId == LSF_FALCON_ID_GPCCS))
    {
        LwU8   *pSignature = NULL;

        // Get the ucode signature
        pSignature = acrGetUcodeSignature(pLsfHeader,
                                            ACR_LS_VERIF_SIGNATURE_CODE_INDEX);
        //
        // Verify code signature
        //
        if ((status = acrVerifySignature_HAL(pSignature, pLsfHeader->ucodeSize, 
                                pLsfHeader->ucodeOffset, pUcodeDesc)) != ACR_OK)
        {
            // Ucode verif failed
            pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED;
            goto VERIF_FAIL;

        }

        // Get the data signature
        pSignature = acrGetUcodeSignature(pLsfHeader,
                                            ACR_LS_VERIF_SIGNATURE_DATA_INDEX);
        //
        // Verify data signature
        //
        if ((status = acrVerifySignature_HAL(pSignature, pLsfHeader->dataSize, 
                                        (pLsfHeader->ucodeSize +
                                            pLsfHeader->ucodeOffset),
                                        pUcodeDesc)) != ACR_OK)
        {
            // Ucode verif failed
            pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED;
            goto VERIF_FAIL;
        }

VERIF_FAIL:
        if ((pWprHeader->status == LSF_IMAGE_STATUS_VALIDATION_DATA_FAILED) || 
            (pWprHeader->status == LSF_IMAGE_STATUS_VALIDATION_CODE_FAILED))
        {
            oldstatus = status;
            status    = acrScrubContentAndWriteWpr(pLsfHeader);

            if (oldstatus != ACR_OK) {
                return oldstatus;
            } else {
                return status;
            }
        }
        else
        {
            // Both ucode and data validation passed, set the status now
            pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_DONE;

        }
    }
    else
    {
        pWprHeader->status = LSF_IMAGE_STATUS_VALIDATION_SKIPPED;
        return ACR_ERROR_FLCN_ID_NOT_FOUND;
    }

    return status;
}
#endif

/*!
 * @brief Function which will take care of bootstrapping the falcons
 *        after once WPR is validated
 *
 * @param[in] none  
 * @param[out] status of bootstrap  
 */
ACR_STATUS acrBootstrapUcode(void)
{
    LwU32              index;
    PLSF_WPR_HEADER    pWprHeader;
    LwU32              validWprMask = 0;
    ACR_STATUS         status       = ACR_OK;
    PLSF_LSB_HEADER_V2 pLsfHeader;
#ifdef ACR_SIG_VERIF
    status_t           ret          = 0;
#endif

    LwBool bootstrapOwnPresent = LW_FALSE;
    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadWprHeader_HAL());
    isBootstrapOwnPresent(&bootstrapOwnPresent);
#ifdef ACR_SIG_VERIF
    ret = init_crypto_devices();
    if (ret != NO_ERROR)
    {
        return ACR_ERROR_LS_SIG_VERIF_FAIL;
    }
#endif
    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index=0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            break;
        }

        //Sanity check all lsbOffset passed from lwgpu before first use.
        if ((g_desc.ucodeBlobSize != 0U) &&
            (pWprHeader->lsbOffset > g_desc.ucodeBlobSize))
        {
            return ACR_ERROR_ILWALID_ARGUMENT;
        }
        validWprMask |= BIT32(index);

        pLsfHeader = &lsbHeaders;

        memset(&lsbHeaders, 0, sizeof(LSF_LSB_HEADER_V2));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeader_T234(pWprHeader,
                                                                pLsfHeader));

        //Skip check for Rail gate.
        if (g_desc.ucodeBlobSize != 0U)
        {
            //Sanity check all offset passed from lwgpu.
            if ((pLsfHeader->ucodeOffset > g_desc.ucodeBlobSize) ||
                (pLsfHeader->ucodeSize > g_desc.ucodeBlobSize) ||
                (pLsfHeader->dataSize > g_desc.ucodeBlobSize) ||
                (pLsfHeader->appCodeOffset > g_desc.ucodeBlobSize) ||
                (pLsfHeader->appCodeSize > g_desc.ucodeBlobSize) ||
                (pLsfHeader->appDataOffset > g_desc.ucodeBlobSize) ||
                (pLsfHeader->appDataSize > g_desc.ucodeBlobSize))
            {
                return ACR_ERROR_ILWALID_ARGUMENT;
            }
        }

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(
                    acrSetupFalconCodeAndDataSubWprs_HAL(pWprHeader->falconId,
                                                        pLsfHeader));
#ifdef ACR_SIG_VERIF
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAuthenticateLSFUcodes(pWprHeader, pLsfHeader, &(pLsfHeader->signature.lsfUcodeDescV2)));
#endif
        if ((pWprHeader->bLazyBootstrap != 0U) && (bootstrapOwnPresent == LW_TRUE)) 
        {
            continue;
        }
        validWprMask |= BIT32(index);

    }

    // Reset the engine and falcon
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrResetEngineFalcon_HAL(validWprMask));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrPollForResetCompletion_HAL(validWprMask));

    //
    // Setup the LS falcon
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupLSFalcon_HAL(&lsbHeaders,
                                                            validWprMask));

    return status;
}
