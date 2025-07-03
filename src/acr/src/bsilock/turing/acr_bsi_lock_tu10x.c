/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_bsi_lock_tu10x.c
 */
#include "acr.h"
#include "acrdrfmacros.h"
#include "mmu/mmucmn.h"
#include "dev_fb.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_fuse.h"
#include "dev_therm.h"
#include "dev_therm_addendum.h"
#include "dev_disp.h"
#include "dev_master.h"
#include "dev_top.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

#ifdef BSI_LOCK
// UDE version where secure scratch 12 register is used to store local memory range
#define UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_GP104   (4)
#define UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_GP102   (1)
#define UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_GP107   (0)
#define UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_DEFAULT (0)
#endif

// Extern Variable g_desc
extern RM_FLCN_ACR_DESC g_desc;

// Copied from RM gc6 code
#ifdef ACR_BSI_PHASE
#define GC6_PHASE_INDEX(phase) (phase + LW_PGC6_BSI_PHASE_OFFSET)
#endif

/*!
 * @brief: Enable/Disable VPR by writing the VPR_IN_USE bit
 */
void
acrEnableDisableVpr_TU10X(LwBool bEnable)
{
    LwU32 vprCmd = 0;

    // Read VPR CYA_LO info
    vprCmd = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_CYA_LO);

    // Update IN_USE bit to what we want to set.
    vprCmd = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_VPR, _CYA_LO_IN_USE, bEnable, vprCmd);

    // Write back CYA_LO.
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_CYA_LO, vprCmd);
}

/*!
 * @brief Check if display engine is enabled in fuse
 */
LwBool
acrIsDispEngineEnabledInFuse_TU10X(void)
{
    return FLD_TEST_DRF_DEF(BAR0, _FUSE, _STATUS_OPT_DISPLAY, _DATA, _ENABLE);
}

#ifdef BSI_LOCK
ACR_STATUS
acrVerifyAcrBsiLockToGC6UDEHandoffIsReset_TU10X()
{
    if (!FLD_TEST_DRF_DEF(BAR0, _THERM, _SELWRE_WR_SCRATCH_1, _ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF, _VAL_RESET))
    {
        //
        // After GC6 exit, this binary has been run once and at that time we had updated the ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF = VAL_ACR_DONE.
        // Finding this value again means attacker has triggered us again, so let's just refuse to run.
        //
        return ACR_ERROR_UNEXPECTED_VALUE_FOUND_FOR_ACR_BSI_LOCK_PHASE_HANDOFF;
    }

    return ACR_OK;
}

ACR_STATUS
acrProgramAcrBsiLockToGC6UDEHandoff_TU10X()
{
    // RMW operation: Read SCRATCH_1 register, update the handoff value and write back to the register.
    FLD_WR_DRF_DEF(BAR0, _THERM, _SELWRE_WR_SCRATCH_1, _ACR_BSI_LOCK_PHASE_TO_GC6_UDE_HANDOFF, _VAL_ACR_DONE);

    return ACR_OK;
}
#endif

#ifdef BSI_LOCK
/*!
 * @brief ACR routine to setup usable memory ranges.
 */
ACR_STATUS
acrProgramMemoryRanges_TU10X(void)
{
    // Reading memory range from secure scratch register and updating in MMU
    LwU32 udeVersion         = DRF_VAL( _PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VBIOS_UDE_VERSION, ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15));
    LwU32 requiredUdeVersion = 0xFFFFFFFF;          // Safe value to init so that we fail if it is not overwritten

    LwU32 chip               = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    switch (chip)
    {
        case LW_PMC_BOOT_42_CHIP_ID_GP104:
            requiredUdeVersion = UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_GP104;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GP102:
        case LW_PMC_BOOT_42_CHIP_ID_GP106:
            requiredUdeVersion = UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_GP102;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_GP107:
        case LW_PMC_BOOT_42_CHIP_ID_GP108:
            requiredUdeVersion = UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_GP107;
            break;
        default:
            // This case is expected for Volta given the current hal wiring
            if (chip >= LW_PMC_BOOT_42_CHIP_ID_GV100)
            {
                requiredUdeVersion = UDE_VERSION_USING_SCRATCH_12_FOR_LOCAL_MEMORY_RANGE_DEFAULT;
            }
            break;
    }

    if (udeVersion >= requiredUdeVersion)
    {
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE, ACR_REG_RD32(BAR0, LW_PGC6_BSI_SELWRE_SCRATCH_MMU_LOCAL_MEMORY_RANGE));
    }
    else
    {
        ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_LOCAL_MEMORY_RANGE, g_desc.mmuMemoryRange);
    }

    return ACR_OK;
}
#endif // BSI_LOCK

#ifdef BSI_LOCK
/*!
 * @brief: Locks VPR range
 */
ACR_STATUS
acrRestoreVPROnResumeFromGC6_TU10X(void)
{
    //
    // Skip VPR restoration in case of displayless GPU as it uses PDISP registers,
    // so we will get bad read, and also VPR wont be enabled on DISPLAYLESS sku
    //
    if (!acrIsDispEngineEnabledInFuse_HAL(pAcr))
    {
        return ACR_OK;
    }

    LwU32 vprScratch13 = ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13);

    //
    // Check if scrubber binary was run before ACR BSI lock phase is being
    // exelwted. However, we can not do this check across the board and need
    // to ensure that max vpr size is set to non-zero by vbios. Note that checking
    // spare_bit_13 is insufficient since fusing is decided upfront while
    // amount of DRAM stuffed can change later. So, it's possibe to have
    // spare_bit_13 = 1 but the DRAM stuffed <= 2GB. In such cases, VBIOS will
    // set max vpr size to 0 to indicate we need to avoid doing anything with VPR
    //
    if ( (DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _MAX_VPR_SIZE_MB, vprScratch13) != 0) &&
         (!FLD_TEST_DRF_DEF(BAR0, _PGC6, _BSI_VPR_SELWRE_SCRATCH_15, _VPR_HANDOFF, _VALUE_SCRUBBER_BIN_DONE)) )
    {
        //
        // Scrubber binary must be completed. if scrubber binary was skipped somehow,
        // system would not be in a stable state, and we should not have reached here. Return early.
        //
        return ACR_ERROR_SCRUBBER_BINARY_COMPLETION_HANDOFF_NOT_FOUND;
    }

    LwU32   vprStartAddr4KAligned = ILWALID_VPR_ADDR_LO;
    LwU32   vprEndAddr4KAligned   = ILWALID_VPR_ADDR_HI;
    LwBool  bVprEnabled           = LW_FALSE;
    LwU32   vprBlankingPolicies   = 0;

    LwU32 vprSizeMB     = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_13, _LWRRENT_VPR_SIZE_MB, vprScratch13);
    LwU32 vprScratch14  = ACR_REG_RD32(BAR0, LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14);
    LwU32 regAddrLo     = 0;
    LwU32 regAddrHi     = 0;

    if (vprSizeMB)
    {
        // VPR was active before GC6 and to be restored. Need to reassign client trust levels.
        acrSetupClientTrustLevelsOnResumeFromGC6_HAL();

        // Non-zero size means valid VPR was present before we went into GC6.
        vprStartAddr4KAligned = DRF_VAL(_PGC6, _BSI_VPR_SELWRE_SCRATCH_14, _LWRRENT_VPR_RANGE_START_ADDR_MB_ALIGNED, vprScratch14);

        // Colwert to 4K aligned address (We need to write 4K aligned addresses to MMU)
        vprStartAddr4KAligned <<= COLWERT_MB_ALIGNED_TO_4K_ALIGNED_SHIFT;

        // end address = start address + size - 1;
        vprEndAddr4KAligned = vprStartAddr4KAligned + (vprSizeMB<<COLWERT_MB_ALIGNED_TO_4K_ALIGNED_SHIFT) - 1;

        // vprSizeMB != 0 means VPR is enabled.
        bVprEnabled = LW_TRUE;

        //
        // Updating VPR_NON_COMPRESSIBLE bit under PROTECTED_MODE index is not required on Turing,
        // since the HW init value of that bit has been updated to come out TRUE after HW reset,
        // which is the expected value.
        //

        vprBlankingPolicies = ACR_REG_RD32(BAR0, LW_PGC6_AON_VPR_SELWRE_SCRATCH_VPR_BLANKING_POLICY_CTRL);
    }
    else
    {
        //
        // Zero VPR size means there was no valid VPR before we went into GC6.
        // Set VPR blanking policies to allow everything except SLI. SLI is always enabled.
        //
        vprBlankingPolicies = ACR_REG_RD32(BAR0, LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL);

        vprBlankingPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_NO_HDCP,        _DISABLE,   vprBlankingPolicies);
        vprBlankingPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP1X,         _DISABLE,   vprBlankingPolicies);
        vprBlankingPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE0,   _DISABLE,   vprBlankingPolicies);
        vprBlankingPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_HDCP22_TYPE1,   _DISABLE,   vprBlankingPolicies);
        vprBlankingPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_INTERNAL,       _DISABLE,   vprBlankingPolicies);
        // SLI  banking is always enabled, we do the same in VPR app.
        vprBlankingPolicies = FLD_SET_DRF(_PDISP_UPSTREAM, _HDCP_VPR_POLICY_CTRL, _BLANK_VPR_ON_SLI,            _ENABLE,    vprBlankingPolicies);

        // HW init value of "CHANGE_EFFECTIVE" is "IMMEDIATE" and thats what we "think" we need, so we are not changing it in code for now.
    }

    // Write out updated values for VPR start & end addresses, and VPR enable bit.
    regAddrLo = DRF_NUM(_PFB, _PRI_MMU_VPR_ADDR_LO, _VAL, vprStartAddr4KAligned);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_ADDR_LO, regAddrLo);

    regAddrHi = DRF_NUM(_PFB, _PRI_MMU_VPR_ADDR_HI, _VAL, vprEndAddr4KAligned);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_ADDR_HI, regAddrHi);

    acrEnableDisableVpr_HAL(pAcr, bVprEnabled);

    ACR_REG_WR32(BAR0, LW_PDISP_UPSTREAM_HDCP_VPR_POLICY_CTRL, vprBlankingPolicies);

    return ACR_OK;
}


/*!
 * @brief: Setup Client Trust levels on resume from GC6
 */
void
acrSetupClientTrustLevelsOnResumeFromGC6_TU10X(void)
{
    LwU32 cyaLo = 0;
    LwU32 cyaHi = 0;

    //
    // Setup VPR CYA's
    // To program CYA register, we can't do R-M-W, as there is no way to read the build in settings
    // RTL built in settings for all fields are provided by James Deming
    // Apart from these built in settings, we are only updating PD/SKED in CYA_LO and FECS/GPCCS in CYA_HI
    //

    // VPR_AWARE list from CYA_LO : all CE engines, SEC2, SCC, L1, TEX, PE
    cyaLo = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_DEFAULT, LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CPU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_HOST,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PERF,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PMU,     LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_CE2,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SEC,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_FE,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PD,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SCC,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_SKED,    LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_L1,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_TEX,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_LO_TRUST_PE,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE));

    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_CYA_LO, cyaLo);

    //
    // VPR_AWARE list from CYA_HI : RASTER, GCC, PROP, all LWENCs, LWDEC
    // Note that the value is changing here when compared to GP10X because DWBIF is not present on GV100_and_later
    // TODO pjambhlekar: Revisit VPR trust levels and check if accesses can be restristred further
    //
    cyaHi = (DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_RASTER,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GCC,        LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GPCCS,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP,       LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_READ,  LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_PROP_WRITE, LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_DNISO,      LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWDEC,      LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_GSP,        LW_PFB_PRI_MMU_VPR_CYA_UNTRUSTED) |
             DRF_NUM(_PFB, _PRI_MMU, _VPR_CYA_HI_TRUST_LWENC1,     LW_PFB_PRI_MMU_VPR_CYA_VPR_AWARE));

    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_CYA_HI, cyaHi);
}



/*!
 * @brief: Release global memory lock
 */
ACR_STATUS
acrReleaseGlobalMemoryLock_TU10X(void)
{
    LwU32 vprMode  = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_VPR_MODE);
    LwU32 fuseLock = ACR_REG_RD32(BAR0, LW_FUSE_OPT_MEMORY_LOCKED_ENABLED);

    if (!FLD_TEST_DRF(_FUSE, _OPT_MEMORY_LOCKED_ENABLED, _DATA, _YES, fuseLock))
    {
        // Global memory lock fuse must be set otherwise return error
        return ACR_ERROR_GLOBAL_MEM_LOCK_FUSE_FOUND_UNSET;
    }

    if(!FLD_TEST_DRF(_PFB, _PRI_MMU_VPR_MODE, _MEMORY_LOCKED, _TRUE, vprMode))
    {
        // Global memory lock must be found enabled here otherwise return error
        return ACR_ERROR_GLOBAL_MEM_LOCK_FOUND_UNSET_IN_BSI_LOCK;
    }

    // Clear global memory lock
    vprMode = FLD_SET_DRF(_PFB, _PRI_MMU_VPR_MODE, _MEMORY_LOCKED, _FALSE, vprMode);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_VPR_MODE, vprMode);

    return ACR_OK;
}
#endif // BSI_LOCK

/*
 * @brief Restore particular falcon subWpr setting from secure scratch registers
 */
ACR_STATUS
acrRestoreFalconSubWpr_TU10X(LwU32 falconId, LwU8 flcnSubWprId)
{
    ACR_STATUS      status  = ACR_OK;
    ACR_FLCN_CONFIG flcnCfg;
    LwU32           regCfga = 0;
    LwU32           regCfgb = 0;
    LwU32           scratchCfga = ILWALID_REG_ADDR;
    LwU32           scratchCfgb = ILWALID_REG_ADDR;
    LwU32           scratchPlm  = ILWALID_REG_ADDR;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetSelwreScratchAllocationForSubWpr_HAL(pAcr, falconId, flcnSubWprId, &scratchCfga, &scratchCfgb, &scratchPlm));

    regCfga = ACR_REG_RD32(BAR0, scratchCfga);
    regCfgb = ACR_REG_RD32(BAR0, scratchCfgb);

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

    //
    // SubWpr registers CFGA and CFGB for single WPR are of 4 bytes and conselwtive
    // Registers for next subWpr of same falcon are 8 bytes seperated (4 bytes of CFGA + 4 bytes of CFGB)
    //
    ACR_REG_WR32(BAR0, (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)), regCfga);
    ACR_REG_WR32(BAR0, (flcnCfg.subWprRangeAddrBase + (FALCON_SUB_WPR_INDEX_REGISTER_STRIDE * flcnSubWprId)
                                                    + FALCON_SUB_WPR_CONFIG_REGISTER_OFFSET_DIFF), regCfgb);

    return status;
}

/*
 * @brief Restore subWpr settings on resume from GC6
 */
ACR_STATUS
acrRestoreSubWprsOnResumeFromGC6_TU10X(void)
{
    ACR_STATUS status = ACR_OK;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_1_FRTS_DATA_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_PMU, PMU_SUB_WPR_ID_2_UCODE_DATA_SECTION_WPR1));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FECS, FECS_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FECS, FECS_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LWDEC0_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LWDEC0_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_LWDEC, LWDEC0_SUB_WPR_ID_2_PLAYREADY_SHARED_DATA_WPR1));

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_0_ACRLIB_FULL_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_1_PLAYREADY_SHARED_DATA_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_3_UCODE_CODE_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_SEC2, SEC2_SUB_WPR_ID_4_UCODE_DATA_SECTION_WPR1));

#ifdef FBFALCON_PRESENT
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_FBFALCON, FBFALCON_SUB_WPR_ID_2_FRTS_DATA_WPR1));
#endif

#ifdef GSP_PRESENT
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_2_ASB_FULL_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR1));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrRestoreFalconSubWpr_HAL(pAcr, LSF_FALCON_ID_GSPLITE, GSP_SUB_WPR_ID_1_UCODE_DATA_SECTION_WPR1));
#endif

    return ACR_OK;
}

/*!
 * @brief Restore WPR settings while resuming from GC6
 */
ACR_STATUS
acrRestoreWPRsOnResumeFromGC6_TU10X
(
    LwU32 *pWprIndex
)
{
    //
    // We do not need to acquire mutex here before reading the BSI secure scratch regs that have WPR information
    // This is because this restoration is only part of ACR BSI lock binary. And BSI lock binary only runs
    // when GPU is off the bus. All other HS clients run when GPU is on the bus. So that's the way we have
    // exclusive access to BSI secure scratch regs
    //

    // TODO  for vyadav by GP104 PS: Use a loop on WPR region IDs to restore WPR settings

    LwU32 wprScratch11  = ACR_REG_RD32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_11);

    LwU32 wpr1Size128K  = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_11, _WPR1_SIZE_IN_128KB_BLOCKS, wprScratch11);
    LwU32 wpr2Size128K  = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_11, _WPR2_SIZE_IN_128KB_BLOCKS, wprScratch11);

#ifndef UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE
    LwU8 wpr1ReadPermissions    = 0;
    LwU8 wpr1WritePermissions   = 0;
    LwU8 wpr2ReadPermissions    = 0;
    LwU8 wpr2WritePermissions   = 0;
#endif

    LwU32 wpr1StartAddr128KAligned, wpr1EndAddr128KAligned;
    LwU32 wpr2StartAddr128KAligned, wpr2EndAddr128KAligned;
    LwU32 data = 0;

    if (wpr1Size128K)
    {
        LwU32 wprScratch9 = ACR_REG_RD32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9);

        wpr1StartAddr128KAligned    = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_9, _WPR1_START_ADDR_128K_ALIGNED, wprScratch9);
        wpr1EndAddr128KAligned      = wpr1StartAddr128KAligned +  wpr1Size128K - 1;
#ifndef UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE
        wpr1ReadPermissions    = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_9, _ALLOW_READ_WPR1,  wprScratch9);
        wpr1WritePermissions   = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_9, _ALLOW_WRITE_WPR1, wprScratch9);
#endif
    }
    else
    {
        wpr1StartAddr128KAligned = ILWALID_WPR_ADDR_LO;
        wpr1EndAddr128KAligned   = ILWALID_WPR_ADDR_HI;
    }

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO);
    data = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_WPR1_ADDR_LO, _VAL, 
                           wpr1StartAddr128KAligned << COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_LO, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI);
    data = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_WPR1_ADDR_HI, _VAL, 
                           wpr1EndAddr128KAligned << COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR1_ADDR_HI, data);

    if (wpr2Size128K)
    {
        LwU32 wprScratch10 = ACR_REG_RD32(BAR0, LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10);

        wpr2StartAddr128KAligned    = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_10, _WPR2_START_ADDR_128K_ALIGNED, wprScratch10) << COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT;
        wpr2EndAddr128KAligned      = wpr2StartAddr128KAligned +  wpr2Size128K - 1;

#ifndef UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE
        wpr2ReadPermissions    = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_10, _ALLOW_READ_WPR2,   wprScratch10);
        wpr2WritePermissions   = DRF_VAL(_PGC6, _BSI_WPR_SELWRE_SCRATCH_10, _ALLOW_WRITE_WPR2,  wprScratch10);
#endif
    }
    else
    {
        wpr2StartAddr128KAligned = ILWALID_WPR_ADDR_LO;
        wpr2EndAddr128KAligned   = ILWALID_WPR_ADDR_HI;
    }

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO);
    data = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_WPR2_ADDR_LO, _VAL, 
                           wpr2StartAddr128KAligned << COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_LO, data);

    data = ACR_REG_RD32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI);
    data = FLD_SET_DRF_NUM(_PFB, _PRI_MMU_WPR2_ADDR_HI, _VAL, 
                           wpr2EndAddr128KAligned << COLWERT_128K_ALIGNED_TO_4K_ALIGNED_SHIFT, data);
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR2_ADDR_HI, data);

    //
    // Write permissions directly rather than calling acrWriteVprWprData since it does not support writing permissions, but only addresses.
    // We could make acrWriteVprWprData intelligent to write permissions correctly, but that will add additional code to BSI lock binary,
    // and we're in crunch situation on that binary.
    //
    /*******************************************************************************************/
    /* WARNING: Hardcoding the values here because of BSI RAM size constraints, Bug 200346512  */
    /*          Do not copy this elsewhere                                                     */
    /*******************************************************************************************/
#ifndef UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ,
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR1, wpr1ReadPermissions) |
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR2, wpr2ReadPermissions));

    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE,
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_WRITE_WPR1, wpr1WritePermissions) |
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_WRITE_WPR2, wpr2WritePermissions));
#else
    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_READ,
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR1, LSF_WPR_REGION_RMASK) |
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_READ_WPR2, 0x0));

    ACR_REG_WR32(BAR0, LW_PFB_PRI_MMU_WPR_ALLOW_WRITE,
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_WRITE_WPR1, LSF_WPR_REGION_WMASK) |
                        DRF_NUM(_PFB, _PRI_MMU_WPR, _ALLOW_WRITE_WPR2, 0x0));
#endif
    return ACR_OK;
}

#ifdef ACR_BSI_PHASE
ACR_STATUS
acrVerifyBsiPhase_TU10X()
{
    LwU32 lwrBootPhase;

#ifdef BSI_LOCK
    LwU32 phaseId = LW_PGC6_BSI_PHASE_ACR_LOCKDOWN_SELWRE;
#elif ACR_BSI_BOOT_PATH
    LwU32 phaseId = LW_PGC6_BSI_ACR_BOOTSTRAP_SELWRE;
#else
    ct_assert(0);
#endif

    // DEBUG1 register isnt present on FMODEL. Filed HW bug 200179861
#ifdef ACR_FMODEL_BUILD
    return ACR_OK;
#endif

    lwrBootPhase = ACR_REG_RD32(BAR0, LW_PGC6_BSI_DEBUG1);
    lwrBootPhase = DRF_VAL(_PGC6, _BSI_DEBUG1, _LWRRBOOTPHASE, lwrBootPhase);

    switch(phaseId)
    {
        case LW_PGC6_BSI_PHASE_ACR_LOCKDOWN_SELWRE:
        case LW_PGC6_BSI_ACR_BOOTSTRAP_SELWRE:
        break;
        default:
            // Put invalid bootphase
            phaseId = LW_PGC6_BSI_DEBUG1_LWRRBOOTPHASE_MAX;
         break;
    }

    //
    // For maxwell, we didnt need this changes, as the DEBUG1 register had HW bug, where it was giving result 1 > the phase number defined in addendum file
    // On GP10X, we are reading correct value from DEBUG1 register, so we need to add the gc5 offset
    //
    phaseId = GC6_PHASE_INDEX(phaseId);

    if(phaseId == lwrBootPhase)
    {
        return ACR_OK;
    }

    return ACR_ERROR_ILWALID_BSI_PHASE;
}

#endif // ACR_BSI_PHASE


