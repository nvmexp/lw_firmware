/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_ls_falcon_boot_tu10x.c
 */
#include "acr.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_sec_pri.h"
#include "dev_falcon_v4.h"
#include "dev_sec_pri.h"
#include "dev_lwdec_pri.h"
#include "dev_gsp.h"
#include "dev_graphics_nobundle.h"
#include "dev_gc6_island.h"
#include "dev_fbif_v4.h"
#include "dev_pwr_pri.h"
#include "dev_master.h"
#include "mmu/mmucmn.h"

#ifdef ACR_RISCV_LS
#include "dev_riscv_pri.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif

// Starting from Ampere, LW_PMC_ENABLE is getting deprecated and move to LW_PMC_DEVICE_ENABLE(i) to disable/enable Falcon device.
#ifdef LW_PMC_DEVICE_ENABLE
    #define READ_DEVICE_ENABLE_REG()  ACR_REG_RD32(BAR0, LW_PMC_DEVICE_ENABLE(pFlcnCfg->pmcEnableRegIdx))
    #define WRITE_DEVICE_ENABLE_REG() ACR_REG_WR32(BAR0, LW_PMC_DEVICE_ENABLE(pFlcnCfg->pmcEnableRegIdx), data);
#else
    #define READ_DEVICE_ENABLE_REG()  ACR_REG_RD32(BAR0, LW_PMC_ENABLE)
    #define WRITE_DEVICE_ENABLE_REG() ACR_REG_WR32(BAR0, LW_PMC_ENABLE, data);
#endif

// Global Variables
#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))

extern ACR_DMA_PROP     g_dmaProp;
extern LwU8             g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX];

/*!
 * @brief Program Dmem range registers
 */
static void
_acrlibProgramDmemRange
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32 data  = 0;
    LwU32 rdata = 0;

    //
    // Setup DMEM carveouts
    // ((endblock<<16)|startblock)
    // Just opening up one last block
    //
    data = acrlibFindTotalDmemBlocks_HAL(pAcrlib, pFlcnCfg) - 1;
    data = (data<<16)|(data);
    if (pFlcnCfg->bIsBoOwner)
    {
        ACR_REG_WR32_STALL(CSB, pFlcnCfg->range0Addr, data);
        rdata = ACR_REG_RD32_STALL(CSB, pFlcnCfg->range0Addr);
    }
    else
    {
        ACR_REG_WR32(BAR0, pFlcnCfg->range0Addr, data);
        rdata = ACR_REG_RD32(BAR0, pFlcnCfg->range0Addr);
    }

    //
    // This is to make sure RANGE0 is programmable..
    // If it is not, open up DMEM to make sure we dont break
    // the existing SW.. TODO: Remove once RANGE0 becomes
    // programmable..
    //
    if (rdata != data)
    {
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK,
                                   ACR_PLMASK_READ_L0_WRITE_L0);
    }
}

#ifdef ASB
/*!
 * @brief ACR initialization routine
 */
ACR_STATUS acrBootFalcon_TU10X(void)
{
    ACR_STATUS   status     = ACR_OK;
    LwU32        wprIndex   = LSF_WPR_EXPECTED_REGION_ID;

    if ((status = acrPopulateDMAParameters_HAL(pAcr, wprIndex)) != ACR_OK)
    {
        goto Cleanup;
    }

#ifdef NEW_WPR_BLOBS
    if ((status = acrBootstrapFalconExt_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }
#else
    if ((status = acrBootstrapFalcon_HAL(pAcr)) != ACR_OK)
    {
        goto Cleanup;
    }
#endif

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrProgramFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1,
                                        ILWALID_WPR_ADDR_LO, ILWALID_WPR_ADDR_HI, ACR_UNLOCK_READ_MASK, ACR_UNLOCK_WRITE_MASK, LW_FALSE));

    acrStartSec2Rtos_HAL(pAcr);

    acrClearGc6ExitSelwreInterrupt_HAL(pAcr);

Cleanup:
    return status;
}

/*!
 * @brief SEC2 RTOS start routine
 */
void acrStartSec2Rtos_TU10X(void)
{
    LwBool bGc6Exit = acrGetGpuGc6ExitStatus_HAL(pAcr);

    if (bGc6Exit)
    {
        ACR_REG_WR32(BAR0, LW_PSEC_FALCON_CPUCTL_ALIAS,
                     DRF_DEF(_PSEC, _FALCON_CPUCTL_ALIAS, _STARTCPU, _TRUE));
    }
}

/*!
 * @brief Read SCI SW Interrupt register to check GC6 exit.
 */
LwBool acrGetGpuGc6ExitStatus_TU10X(void)
{
    return FLD_TEST_DRF(_PGC6_SCI, _SW_SEC_INTR_STATUS, _GC6_EXIT, _PENDING,
                        (ACR_REG_RD32(BAR0, LW_PGC6_SCI_SW_SEC_INTR_STATUS)));
}
#endif //ASB

#if defined(ASB)
ACR_STATUS
acrBootstrapFalcon_TU10X(void)
{
    ACR_STATUS              status     = ACR_OK;
    PLSF_WPR_HEADER         pWprHeader = NULL;
    LSF_LSB_HEADER          lsbHeader;
    LwU32                   index      = 0;

    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadWprHeader_HAL(pAcr));

    for (index = 0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        if (pWprHeader->falconId == LSF_FALCON_ID_SEC2)
        {
            break;
        }
    }

    if (index > LSF_FALCON_ID_END)
    {
        return ACR_ERROR_ACRLIB_HOSTING_FALCON_NOT_FOUND;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeader_HAL(pAcr, pWprHeader, &lsbHeader));

    // Setup the LS falcon
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupLSFalcon_HAL(pAcr, pWprHeader, &lsbHeader));

    return status;
}
#endif

/*
 * General setup for decode traps related to action, PLM, and config for Locking falcon regspace
 * TODO: Use pri-source-isolation TARGET_MASK for locking falcon regspace, Bug 2155192
 */
ACR_STATUS
acrLockFalconRegSpaceViaDecodeTrapCommon_TU10X(void)
{
    // Force return decode trap error
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX),
                            DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP_ACTION, _FORCE_ERROR_RETURN, _ENABLE));

    //
    // Set decode trap PLM to write protect for all levels except level3, and read allowed for all levels
    // We cannot clobber complete register here, as SOURCE_ENABLE fields are added
    //
    LwU32 regPlm = ACR_REG_RD32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX));
    regPlm = FLD_SET_DRF( _PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _READ_PROTECTION,         _ALL_LEVELS_ENABLED, regPlm);   /* read protection all levels enabled */
    regPlm = FLD_SET_DRF( _PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE,            regPlm);   /* write protection all levels disabled except level3 (HW default value) */
    regPlm = FLD_SET_DRF( _PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE,            regPlm);
    regPlm = FLD_SET_DRF( _PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE,            regPlm);
    regPlm = FLD_SET_DRF( _PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR,       regPlm);
    regPlm = FLD_SET_DRF( _PPRIV, _SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR,       regPlm);
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_PRIV_LEVEL_MASK(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX), regPlm);

    //
    // Set TRAP_APPLICATION to trap levels 0, 1, and 2 and ensure only level 3 can write the registers covered by PLM
    // Ignore trap on reads
    //
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH_CFG(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX),
                    DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL0, _ENABLE) |   /* set TRAP_APPLICATION to trap all levels except level 3 */
                    DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL1, _ENABLE) |
                    DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL2, _ENABLE) |
                    DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _TRAP_APPLICATION_LEVEL3, _DISABLE) |
                    DRF_DEF( _PPRIV, _SYS_PRI_DECODE_TRAP_MATCH_CFG, _IGNORE_READ,             _IGNORED));    /* Ignore trap on reads */

    return ACR_OK;
}

/*
 * *************************************** WARNING ****************************************
 * This function only supports SEC2 at the moment as this is called from ASB and not ACRLIB
 * ****************************************************************************************
 * Use decode traps to lock falcon register space
 * Note that we are setting this trap only for ACRlib falcon from ACR_LOAD,
 * suport for other falcons in ACRlib is tracked in Bug 2155192
 * TODO: Use pri-source-isolation TARGET_MASK for locking falcon regspace, Bug 2155192
 */
ACR_STATUS
acrlibLockFalconRegSpace_TU10X(LwU32 sourceId, PACR_FLCN_CONFIG pTargetFlcnCfg, LwBool setTrap , LwU32 *pTargetMaskPlmOldValue, LwU32 *pTargetMaskOldValue)
{

#ifdef ACR_BUILD_ONLY
    if (NULL == pTargetMaskPlmOldValue)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (NULL == pTargetMaskOldValue)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // Clear mask and match
    // MASK needs to be cleared first else we will trap starting offset 0 and whatever mask covers after the first write
    //
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MASK(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX), 0);

    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX), 0);

    // Lock the falcon reg space
    if (setTrap)
    {
        if (sourceId == pTargetFlcnCfg->falconId)
        {
            return ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
        }

        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX), pTargetFlcnCfg->registerBase);
        ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP_MASK(DECODE_TRAP_FOR_FALCON_REG_SPACE_INDEX), DECODE_TRAP_MASK_LSF_FALCON_SEC2);
    }
#endif

    return ACR_OK;
}

ACR_STATUS
acrResetAndPollForSec2_TU10X
(
    PLSF_WPR_HEADER pWprHeader
)
{
    ACR_FLCN_CONFIG  flcnCfg;
    ACR_STATUS status;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, pWprHeader->falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPreEngineResetSequenceBug200590866_HAL(pAcrlib, &flcnCfg));

    // Put SEC2 in reset first
    ACR_REG_WR32(BAR0, LW_PSEC_FALCON_ENGINE, LW_PSEC_FALCON_ENGINE_RESET_TRUE);

    // Dummy read.
    ACR_REG_RD32(BAR0, LW_PSEC_FALCON_ENGINE);

    // Bring SEC2 out of reset
    ACR_REG_WR32(BAR0, LW_PSEC_FALCON_ENGINE, LW_PSEC_FALCON_ENGINE_RESET_FALSE);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCoreSelect_HAL(pAcrlib, &flcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPostEngineResetSequenceBug200590866_HAL(pAcrlib, &flcnCfg));

    // Poll SEC2 for IMEM|DMEM scrubbing
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPollForScrubbing_HAL(pAcrlib, &flcnCfg));

    return ACR_OK;
}

#ifdef ACR_BUILD_ONLY
#if !defined(ACR_UNLOAD) && !defined(ACR_UNLOAD_ON_SEC2)
/*!
 * @brief Setup Falcon in LS mode. This ilwolves
 *        - Setting up LS registers (TODO)
 *        - Copying bootloader from FB into target falcon memory
 *        - Programming carveout registers in target falcon
 *
 * @param[in] pWprHeader    WPR header
 * @param[in] pLsbHeader    LSB header
 */
ACR_STATUS
acrSetupLSFalcon_TU10X
(
    PLSF_WPR_HEADER  pWprHeader,
    PLSF_LSB_HEADER  pLsbHeader
)
{
    ACR_STATUS       status                = ACR_OK;
    ACR_STATUS       acrStatusCleanup      = ACR_OK;
    ACR_FLCN_CONFIG  flcnCfg;
    LwU64            blWprBase;
    LwU32            dst;
    LwU32            targetMaskPlmOldValue = 0;
    LwU32            targetMaskOldValue    = 0;

    // Configure common settings for decode traps locking falcon reg space
    status = acrLockFalconRegSpaceViaDecodeTrapCommon_HAL(pAcr);

    // Get the specifics of target falconID
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, pWprHeader->falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // Lock falcon reg space using decode trap/pri source isolation.
    // For trap based locking:
    //     Reuse the same trap such that the trap lasts only for a single loop iteration
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibLockFalconRegSpace_HAL(pAcrlib, LSF_FALCON_ID_GSPLITE, &flcnCfg, LW_TRUE, &targetMaskPlmOldValue, &targetMaskOldValue));

    // Reset the SEC2 falcon.
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrResetAndPollForSec2_HAL(pAcr, pWprHeader));

    pLsbHeader->blCodeSize = LW_ALIGN_UP(pLsbHeader->blCodeSize, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibSetupTargetRegisters_HAL(pAcrlib, &flcnCfg));

    // Override the value of IMEM/DMEM PLM to final value for the falcons being booted by this level 3 binary
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, flcnCfg.imemPLM);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, &flcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, flcnCfg.dmemPLM);

    //
    // Load code into IMEM
    // BL starts at offset, but since the IMEM VA is not zero, we need to make
    // sure FBOFF is equal to the expected IMEM VA. So adjusting the FBBASE to make
    // sure FBOFF equals to VA as expected.
    //

    blWprBase = ((g_dmaProp.wprBase) + (pLsbHeader->ucodeOffset >> FLCN_IMEM_BLK_ALIGN_BITS)) -
                 (pLsbHeader->blImemOffset >> FLCN_IMEM_BLK_ALIGN_BITS);

    // Check if code needs to be loaded at start of IMEM or at end of IMEM
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _LOAD_CODE_AT_0, _TRUE, pLsbHeader->flags))
    {
        dst = 0;
    }
    else
    {
        dst = (acrlibFindFarthestImemBl_HAL(pAcrlib, &flcnCfg, pLsbHeader->blCodeSize) *
               FLCN_IMEM_BLK_SIZE_IN_BYTES);
    }

    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibIssueTargetFalconDma_HAL(pAcrlib,
        dst, blWprBase, pLsbHeader->blImemOffset,
        pLsbHeader->blCodeSize, g_dmaProp.regionID, LW_TRUE, LW_TRUE, LW_FALSE, &flcnCfg));

    // Load data into DMEM
    CHECK_STATUS_OK_OR_GOTO_CLEANUP(acrlibIssueTargetFalconDma_HAL(pAcrlib,
        0, g_dmaProp.wprBase, pLsbHeader->blDataOffset, pLsbHeader->blDataSize,
        g_dmaProp.regionID, LW_TRUE, LW_FALSE, LW_FALSE, &flcnCfg));

    // Set the BOOTVEC
    acrlibSetupBootvec_HAL(pAcrlib, &flcnCfg, pLsbHeader->blImemOffset);

    // Check if Falcon wants virtual ctx
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _SET_VA_CTX, _TRUE, pLsbHeader->flags))
    {
        acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, flcnCfg.ctxDma, LW_FALSE);
    }
    else
    {
        acrlibSetupCtxDma_HAL(pAcrlib, &flcnCfg, flcnCfg.ctxDma, LW_TRUE);
    }

    // Check if Falcon wants REQUIRE_CTX
    if (FLD_TEST_DRF(_FLCN, _ACR_LSF_FLAG, _DMACTL_REQ_CTX, _TRUE, pLsbHeader->flags))
    {
        acrlibSetupDmaCtl_HAL(pAcrlib, &flcnCfg, LW_TRUE);
    }

    //
    // Clear decode trap to free falcon reg space
    // The trap from last loop iteration is still around and if we don't clear it then falcon will not be reachable via underprivileged clients
    //
Cleanup:
    acrStatusCleanup = acrlibLockFalconRegSpace_HAL(pAcrlib, LSF_FALCON_ID_GSPLITE, &flcnCfg, LW_FALSE, &targetMaskPlmOldValue, &targetMaskOldValue);

    return ((status != ACR_OK) ? status : acrStatusCleanup);
}
#endif // Not defined ACR_UNLOAD and not defined ACR_UNLOAD_ON_SEC2
#endif // ACR_BUILD_ONLY

/*!
 * @brief Find the IMEM block from the end to load BL code
 */
LwU32
acrlibFindFarthestImemBl_TU10X
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    LwU32              codeSizeInBytes
)
{
    LwU32 hwcfg    = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    LwU32 imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);
    LwU32 codeBlks = DIV_ROUND_UP(codeSizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    return (imemSize - codeBlks);
}

/*!
 * @brief Find the total number of DMEM blocks
 */
LwU32
acrlibFindTotalDmemBlocks_TU10X
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    LwU32 hwcfg    = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    return DRF_VAL(_PFALCON_FALCON, _HWCFG, _DMEM_SIZE, hwcfg);
}

/*!
 * @brief Issue target falcon DMA. Supports only FB -> Flcn and not the other way.
 *        Supports DMAing into both IMEM and DMEM of target falcon from FB.
 *        Always uses physical addressing for memory transfer. Expects size to be
 *        256 multiple.
 *
 * @param[in] dstOff       Destination offset in either target falcon DMEM or IMEM
 * @param[in] fbBase       Base address of fb region to be copied
 * @param[in] fbOff        Offset from fbBase
 * @param[in] sizeInBytes  Number of bytes to be transferred
 * @param[in] regionID     ACR region ID to be used for this transfer
 * @param[in] bIsSync      Is synchronous transfer?
 * @param[in] bIsDstImem   TRUE if destination is IMEM
 * @param[in] bIsSelwre    TRUE if destination is a secure IMEM block. Valid only for IMEM destinations
 * @param[in] pFlcnCfg     Falcon config
 *
 * @return ACR_OK on success
 *         ACR_ERROR_UNEXPECTED_ARGS for mismatched arguments
 *         ACR_ERROR_TGT_DMA_FAILURE if the source, destination or size is not aligned
 */
ACR_STATUS
acrlibIssueTargetFalconDma_TU10X
(
    LwU32            dstOff,
    LwU64            fbBase,
    LwU32            fbOff,
    LwU32            sizeInBytes,
    LwU32            regionID,
    LwU8             bIsSync,
    LwU8             bIsDstImem,
    LwU8             bIsSelwre,
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    ACR_STATUS     status        = ACR_OK;
    LwU32          data          = 0;
    LwU32          dmaCmd        = 0;
    LwU32          bytesXfered   = 0;
    LwS32          timeoutLeftNs = 0;
    ACR_TIMESTAMP  startTimeNs;

    //
    // Sanity checks
    // Only does 256B DMAs
    //
    if ((!VAL_IS_ALIGNED(sizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES)) ||
        (!VAL_IS_ALIGNED(dstOff, FLCN_IMEM_BLK_SIZE_IN_BYTES))      ||
        (!VAL_IS_ALIGNED(fbOff, FLCN_IMEM_BLK_SIZE_IN_BYTES)))
    {
        return ACR_ERROR_TGT_DMA_FAILURE;
    }

    if (!bIsDstImem && bIsSelwre)
    {
        // DMEM Secure block transfers are invalid 
        return ACR_ERROR_UNEXPECTED_ARGS;
    }

    //
    // Program Transcfg to point to physical mode
    //
    acrlibSetupCtxDma_HAL(pAcrlib, pFlcnCfg, pFlcnCfg->ctxDma, LW_TRUE);

    if (pFlcnCfg->bFbifPresent)
    {
        //
        // Disable CTX requirement for falcon DMA engine
        //
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL);
        data = FLD_SET_DRF(_PFALCON, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, data);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FBIF, LW_PFALCON_FBIF_CTL, data);
    }

    // Set REQUIRE_CTX to false
    acrlibSetupDmaCtl_HAL(pAcrlib, pFlcnCfg, LW_FALSE);

    //Program REGCONFIG
    acrlibProgramRegionCfg_HAL(pAcrlib, pFlcnCfg, LW_FALSE, pFlcnCfg->ctxDma, regionID);

    //
    // Program DMA registers
    // Write DMA base address
    //
    acrlibProgramDmaBase_HAL(pAcrlib, pFlcnCfg, fbBase);

    // prepare DMA command
    {
        dmaCmd = 0;
        if (bIsDstImem)
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _TRUE, dmaCmd);
        }
        else
        {
            dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _IMEM, _FALSE, dmaCmd);
        }

        if (bIsSelwre)
        {
            dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _SEC, 0x1, dmaCmd);
        }

        // Allow only FB->Flcn
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _WRITE, _FALSE, dmaCmd);

        // Allow only 256B transfers
        dmaCmd = FLD_SET_DRF(_PFALCON, _FALCON_DMATRFCMD, _SIZE, _256B, dmaCmd);

        dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _CTXDMA, pFlcnCfg->ctxDma, dmaCmd);
    }

    while (bytesXfered < sizeInBytes)
    {
        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFMOFFS, _OFFS, (dstOff + bytesXfered), 0);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFMOFFS, data);

        data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFFBOFFS, _OFFS, (fbOff + bytesXfered), 0);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFFBOFFS, data);

        // Write the command
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD, dmaCmd);

        //
        // Poll for completion
        // TODO: Make use of bIsSync
        //
        acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        while(FLD_TEST_DRF(_PFALCON_FALCON, _DMATRFCMD, _IDLE, _FALSE, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFCMD);
        }

        bytesXfered += FLCN_IMEM_BLK_SIZE_IN_BYTES;
    }

    return status;
}

/*!
 * @brief Poll for IMEM and DMEM scrubbing to complete
 *
 * @param[in] pFlcnCfg Falcon Config
 */
ACR_STATUS
acrlibPollForScrubbing_TU10X
(
    PACR_FLCN_CONFIG  pFlcnCfg
)
{
    ACR_STATUS    status          = ACR_OK;
    LwU32         data            = 0;
    ACR_TIMESTAMP startTimeNs;
    LwS32         timeoutLeftNs;


    // Poll for SRESET to complete
    acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
    data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMACTL);
    while((FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _DMEM_SCRUBBING, _PENDING, data)) ||
         (FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _IMEM_SCRUBBING, _PENDING, data)))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, (LwU32)ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMACTL);
    }

    return status;
}

/*!
 * @brief Program DMA base register
 *
 * @param[in] pFlcnCfg   Structure to hold falcon config
 * @param[in] fbBase     Base address of fb region to be copied
 */
void
acrlibProgramDmaBase_TU10X
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU64               fbBase
)
{
    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE, (LwU32)LwU64_LO32(fbBase));
    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMATRFBASE1, (LwU32)LwU64_HI32(fbBase));
}

/*!
 * @brief Programs the bootvector for Falcons
 *
 * @param[in] pFlcnCfg   Falcon configuration information
 * @param[in] bootvec    Boot vector
 */
void
acrlibSetupBootvec_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 bootvec
)
{
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg,
          REG_LABEL_FLCN_BOOTVEC, bootvec);
}

/*!
 * @brief Programs target registers to ensure the falcon goes into LS mode
 */
ACR_STATUS
acrlibSetupTargetRegisters_TU10X
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS       status     = ACR_OK;
    LwU32            data       = 0;
    LwU32            i          = 0;

    //
    // For few LS falcons ucodes, we use bootloader to load actual ucode,
    // so we need to restrict them to be loaded from WPR only
    //
    if (acrlibCheckIfFalconIsBootstrappedWithLoader_HAL(pAcr, pFlcnCfg->falconId))
    {
        for (i = 0; i < LW_PFALCON_FBIF_TRANSCFG__SIZE_1; i++)
        {
            acrlibProgramRegionCfg_HAL(pAcrlib, pFlcnCfg, LW_FALSE, i, LSF_WPR_EXPECTED_REGION_ID);
        }
    }

    // Step-1: Set SCTL priv level mask to only HS accessible
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L3);

    //
    // Step-2: Set
    //    SCTL_RESET_LVLM_EN to FALSE
    //    SCTL_STALLREQ_CLR_EN to FALSE
    //    SCTL_AUTH_EN to TRUE
    //

    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _FALSE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _FALSE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_SCTL, data);

    // Step-3: Set CPUCTL_ALIAS_EN to FALSE
    data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _FALSE, 0);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data);

#ifdef ACR_RISCV_LS
    //
    // TODO: Create a proper, RISC-V-specific variant of this function.
    //       See bugs 2720166 and 2720167.
    //
    if (pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
#ifdef LW_PRISCV_RISCV_CPUCTL_ALIAS_EN
        data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL, _ALIAS_EN, _FALSE, 0);
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_RISCV_CPUCTL, data);
#endif
    }
#endif

    //
    // Step-4: Setup
    //        IMEM_PRIV_LEVEL_MASK
    //        DMEM_PRIV_LEVEL_MASK
    //        CPUCTL_PRIV_LEVEL_MASK
    //        DEBUG_PRIV_LEVEL_MASK
    //        EXE_PRIV_LEVEL_MASK
    //        IRQTMR_PRIV_LEVEL_MASK
    //        MTHDCTX_PRIV_LEVEL_MASK
    //        BOOTVEC_PRIV_LEVEL_MASK
    //        DMA_PRIV_LEVEL_MASK 
    //        FBIF_REGIONCFG_PRIV_LEVEL_MASK
    //
    // Use level 2 protection for writes to IMEM/DMEM. We do not need read protection and leaving IMEM/DMEM
    // open can aid debug if a problem happens before the PLM is overwritten by ACR binary or the PMU task to
    // the final value. Note that level 2 is the max we can use here because this code is run inside PMU at
    // level 2. In future when we move the acr task from PMU to SEC2 and make it HS, we can afford to use
    // the final values from pFlcnCfg->{imem, dmem}PLM directly over here.
    //
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK, pFlcnCfg->imemPLM);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK, pFlcnCfg->dmemPLM);

    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L0);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L0);

#ifdef NEW_WPR_BLOBS    
    // Raise BOOTVEC, DMA registers to be only level 3 writeable
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibUpdateBootvecPlmLevel3Writeable_HAL(pAcrlib, pFlcnCfg));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibUpdateDmaPlmLevel3Writeable_HAL(pAcrlib, pFlcnCfg));
#endif // NEW_WPR_BLOBS

    if (pFlcnCfg->bFbifPresent)
    {
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK, ACR_PLMASK_READ_L0_WRITE_L2);
    }
    else
    {
        //
        // FECS and GPCCS case, as they do not support FBIF
        // Changing this to R-M-W, as we have Pri-Source-Isolation from Turing
        // because of which we have more field in REGIONCFG PLM
        //
        LwU32 reg = ACR_REG_RD32(BAR0, pFlcnCfg->regCfgMaskAddr);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _READ_PROTECTION,         _ALL_LEVELS_ENABLED, reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR,       reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE,            reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE,            reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE,             reg);
        reg = FLD_SET_DRF(_PGRAPH, _PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR,       reg);
        ACR_REG_WR32(BAR0, pFlcnCfg->regCfgMaskAddr, reg);
    }

    // Setup target falcon PLMs (Lwrrently only PRIVSTATE_PLM, more to be added)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibSetupTargetFalconPlms_HAL(pAcrlib, pFlcnCfg));

    // ASB already resets SEC2 before setting up registers.
#ifndef ASB
    // Step-5: Check if the falcon is HALTED
    if (!acrlibIsFalconHalted_HAL(pAcrlib, pFlcnCfg))
    {
        // Return error in case target falcon is not at HALT state.
        return ACR_ERROR_FLCN_NOT_IN_HALTED_STATE;
    }
#endif

    //
    // Step-6: Authorize falcon
    // TODO: Let the individual falcons ucodes take care of setting up TRANSCFG/REGIONCFG
    //

    // Program LSMODE here
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, 0);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _STALLREQ_CLR_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    data = FLD_SET_DRF(_PFALCON, _FALCON_SCTL, _LSMODE, _TRUE, data);
    data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_SCTL, _LSMODE_LEVEL, 0x2, data);
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_SCTL, data);

    // Check if AUTH_EN is still TRUE
    data = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_SCTL);
    if (!FLD_TEST_DRF(_PFALCON, _FALCON_SCTL, _AUTH_EN, _TRUE, data))
    {
        return ACR_ERROR_LS_BOOT_FAIL_AUTH;
    }

    if (pFlcnCfg->bOpenCarve)
    {
        _acrlibProgramDmemRange(pFlcnCfg);
    }

    // Step-7: Set CPUCTL_ALIAS_EN to TRUE
#ifdef ACR_RISCV_LS
    if (pFlcnCfg->uprocType == ACR_TARGET_ENGINE_CORE_RISCV)
    {
#ifdef LW_PRISCV_RISCV_CPUCTL_ALIAS_EN
        data = FLD_SET_DRF(_PRISCV, _RISCV_CPUCTL, _ALIAS_EN, _TRUE, 0);
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_RISCV_CPUCTL, data);
#endif
    }
    else
#endif
    {
        data = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _ALIAS_EN, _TRUE, 0);
        acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_CPUCTL, data);
    }

#ifndef ACR_BSI_BOOT_PATH
    // There are some SEC2-specific registers to be initialized
    if (pFlcnCfg->falconId == LSF_FALCON_ID_SEC2)
    {
        acrlibSetupSec2Registers_HAL(pAcrlib, pFlcnCfg);
    }
#endif

    return status;
}


/*!
 * @brief Clear GC6 secure interrupt to come out of GC6 (this interrupt is checked by level3 ucodes)
 */
void
acrClearGc6ExitSelwreInterrupt_TU10X(void)
{
    // Clear the GC6 secure interrupt

    LwU32 intr = ACR_REG_RD32(BAR0, LW_PGC6_SCI_SW_SEC_INTR_STATUS);
    if (FLD_TEST_DRF( _PGC6, _SCI_SW_SEC_INTR_STATUS, _GC6_EXIT, _PENDING, intr))
    {
        intr = FLD_SET_DRF( _PGC6, _SCI_SW_SEC_INTR_STATUS, _GC6_EXIT, _CLEAR, intr);
        ACR_REG_WR32(BAR0, LW_PGC6_SCI_SW_SEC_INTR_STATUS, intr);
    }
}

/*!
 * @brief Check falcons for which we use bootloader to load actual LS ucode
 *        We need to restrict such LS falcons to be loaded only from WPR, Bug 1969345
 */
LwBool
acrlibCheckIfFalconIsBootstrappedWithLoader_TU10X
(
    LwU32 falconId
)
{
    // From GP10X onwards, PMU, SEC2 and DPU(GSPLite for GV100) are loaded via bootloader
    if ((falconId == LSF_FALCON_ID_PMU) || (falconId == LSF_FALCON_ID_DPU) || (falconId == LSF_FALCON_ID_SEC2))
    {
        return LW_TRUE;
    }

#ifdef ACR_RISCV_LS
    if (falconId == LSF_FALCON_ID_GSP_RISCV || falconId == LSF_FALCON_ID_PMU_RISCV)
    {
        return LW_TRUE;
    }
#endif

    return LW_FALSE;
}

/*!
 * @brief Resets a given falcon
 */
ACR_STATUS
acrlibResetFalcon_TU10X
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    ACR_STATUS status = ACR_OK;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPreEngineResetSequenceBug200590866_HAL(pAcrlib, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPutFalconInReset_HAL(pAcrlib, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibBringFalconOutOfReset_HAL(pAcrlib, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCoreSelect_HAL(pAcrlib, pFlcnCfg));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibPostEngineResetSequenceBug200590866_HAL(pAcrlib, pFlcnCfg));

    return status;
}

/*!
 * @brief Put the given falcon into reset state
 */
ACR_STATUS
acrlibPutFalconInReset_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32      data   = 0;
    ACR_STATUS status = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    //
    // For the Falcon doesn't have a full-blown engine reset, we reset Falcon only.
    //
    if ((pFlcnCfg->pmcEnableMask == 0) && (pFlcnCfg->regSelwreResetAddr == 0))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibFalconOnlyReset_HAL(pAcrlib, pFlcnCfg));
    }
    else
    {
        if (pFlcnCfg->pmcEnableMask != 0)
        {
            data = READ_DEVICE_ENABLE_REG();
            data &= ~pFlcnCfg->pmcEnableMask;
            WRITE_DEVICE_ENABLE_REG();
            READ_DEVICE_ENABLE_REG();
        }

        if (pFlcnCfg->regSelwreResetAddr != 0)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibAssertEngineReset_HAL(pAcrlib, pFlcnCfg));
        }
    }

    return ACR_OK;
}

/*!
 * @brief Bring the given falcon out of reset state
 */
ACR_STATUS
acrlibBringFalconOutOfReset_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32      data   = 0;
    ACR_STATUS status = ACR_OK;

    if (pFlcnCfg == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    if (pFlcnCfg->regSelwreResetAddr != 0)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibDeassertEngineReset_HAL(pAcrlib, pFlcnCfg));
    }

    if (pFlcnCfg->pmcEnableMask != 0)
    {
        data = READ_DEVICE_ENABLE_REG();
        data |= pFlcnCfg->pmcEnableMask;
        WRITE_DEVICE_ENABLE_REG();
        READ_DEVICE_ENABLE_REG();
    }

    return ACR_OK;
}

/*!
 * @brief Check if falcon is halted or not
 */
LwBool
acrlibIsFalconHalted_TU10X
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    LwU32 data = 0;
    data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
    return FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _HALTED, _TRUE, data);
}

/*!
 * @brief Reset only the falcon part
 */
ACR_STATUS
acrlibFalconOnlyReset_TU10X
(
    PACR_FLCN_CONFIG  pFlcnCfg
)
{
    ACR_STATUS    status = ACR_OK;
    LwU32         sftR   = 0;
    LwU32         data   = 0;
    ACR_TIMESTAMP startTimeNs;
    LwS32         timeoutLeftNs;

    if (pFlcnCfg->bFbifPresent)
    {
        // TODO: Revisit this once FECS halt bug has been fixed

        // Wait for STALLREQ
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE, 0);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_ENGCTL, sftR);

        // Poll for FHSTATE.EXT_HALTED to complete
        acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE);
        while(FLD_TEST_DRF_NUM(_PFALCON, _FALCON_FHSTATE, _EXT_HALTED, 0, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                                       startTimeNs, &timeoutLeftNs));
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE);
        }

        // Do SFTRESET
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, 0);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET, sftR);

        // Poll for SFTRESET to complete
        acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET);
        while(FLD_TEST_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                                       startTimeNs, &timeoutLeftNs));
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET);
        }
    }

    // Do falcon reset
    sftR = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _SRESET, _TRUE, 0);
    acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL, sftR);

    // Poll for SRESET to complete
    acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
    data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
    while(FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _SRESET, _TRUE, data))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                        startTimeNs, &timeoutLeftNs));
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
    }

    return ACR_OK;
}

ACR_STATUS
acrlibAssertEngineReset_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32 data = 0;

    data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
    data = FLD_SET_DRF(_PPWR, _FALCON_ENGINE, _RESET, _TRUE, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);

    return ACR_OK;
}

ACR_STATUS
acrlibDeassertEngineReset_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg
)
{
    LwU32 data = 0;

    data = ACR_REG_RD32(BAR0, pFlcnCfg->regSelwreResetAddr);
    data = FLD_SET_DRF(_PPWR, _FALCON_ENGINE, _RESET, _FALSE, data);
    ACR_REG_WR32(BAR0, pFlcnCfg->regSelwreResetAddr, data);

    return ACR_OK;
}
