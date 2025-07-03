/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_sect234.c
 */
//
// Includes
//
#include "acr.h"
#include "rmlsfm.h"
#include "dev_master.h"

#include "dev_fb.h"
#include "dev_fuse.h"
#include "dev_hshub_SW.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"
#include "dev_graphics_nobundle.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_gpc_addendum.h"

#include "g_acr_private.h"
#include "g_acrlib_private.h"

/*! T234 ACR UCODE SW VERSION */
#define ACR_T234_UCODE_VERSION    (0x1)


#define FIELD_BITS(TARGET)  ((LW_PPRIV_SYS_PRI_SOURCE_ID_##TARGET << 1) + 1) : \
                            (LW_PPRIV_SYS_PRI_SOURCE_ID_##TARGET << 1)

#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL                            FIELD_BITS(PMU)
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_RW_ENABLED                      0x00000003
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_RW_L0                           0x00000002
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_BLOCK_WRITES_READ_L0            0x00000001
#define LW_PPRIV_SYS_TARGET_MASK_PMU_ACCESS_CONTROL_RW_DISABLED                     0x00000000

#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL                            FIELD_BITS(GSP)
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_RW_ENABLED                      0x00000003
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_RW_L0                           0x00000002
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_BLOCK_WRITES_READ_L0            0x00000001
#define LW_PPRIV_SYS_TARGET_MASK_GSP_ACCESS_CONTROL_RW_DISABLED                     0x00000000

/*!
 *
 * @brief Get the SW fuse version of ACR ucode from the SW VERSION MACRO defined. This MACRO is updated(revision increased) with important security fixes.
 * @param[out] fuseVersion ptr to return SW version value to callee.
 *
 */
void
acrGetFuseVersionSW_T234(LwU32* fuseVersion)
{
    *fuseVersion = ACR_T234_UCODE_VERSION;
}

/*!
 * @brief Verify CHIP ID is same as intended.\n
 *        This is a security feature so as to constrain a binary for a particular chip.
 *
 * @return ACR_OK If chip-id is as same as intended for your build.
 * @return ACR_ERROR_ILWALID_CHIP_ID If chip-id is not same as intended.
 */
ACR_STATUS
acrCheckIfBuildIsSupported_T234(void)
{
    LwU32 chip = DRF_VAL(_PMC, _BOOT_42, _CHIP_ID, ACR_REG_RD32(BAR0, LW_PMC_BOOT_42));

    if (chip == LW_PMC_BOOT_42_CHIP_ID_GA10B ||
            chip == LW_PMC_BOOT_42_CHIP_ID_GA10F)
    {
        return ACR_OK;
    }
    return  ACR_ERROR_ILWALID_CHIP_ID;
}
#ifdef ACR_BUILD_ONLY
/*!
 * @brief Setup decode trap for CPU to access FBHUB registers.\n
 *        These registers can only be accessed by PL3 client by default.
 */
void acrSetupFbhubDecodeTrap_T234(void)
{
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP4_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP4_MATCH, _ADDR, LW_PFB_HSHUB_NUM_ACTIVE_LTCS(0)) |
                                                            DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP4_MATCH, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP4_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP4_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP4_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP4_MASK, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP4_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP4_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP4_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP4_ACTION, _SET_PRIV_LEVEL, _ENABLE));

    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP5_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP5_MATCH, _ADDR, LW_PFB_FBHUB_NUM_ACTIVE_LTCS) |
                                                            DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP5_MATCH, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP5_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP5_MATCH, _CFG_TRAP_APPLICATION_LEVEL0, _ENABLE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP5_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP5_MASK, _SOURCE_ID, PRI_SOURCE_ID_XVE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP5_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP5_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP5_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP5_ACTION, _SET_PRIV_LEVEL, _ENABLE));
}

/*!
 * @brief Setup decode trap for LS PMU to access GR falcon SCTL registers.\n
 *        These registers can only be accessed by PL3 client by default.
 */
void acrSetupSctlDecodeTrap_T234(void)
{
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP6_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _ADDR, LW_PGRAPH_PRI_FECS_FALCON_SCTL) |
                                                            DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP6_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP6_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP6_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP6_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP6_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP6_ACTION, _SET_PRIV_LEVEL, _ENABLE));

    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP7_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _ADDR, LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_SCTL) |
                                                            DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP7_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP7_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP7_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP7_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP7_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP7_ACTION, _SET_PRIV_LEVEL, _ENABLE));

    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP8_MATCH, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _ADDR, LW_PGRAPH_PRI_GPC0_GPCCS_FALCON_SCTL) |
                                                            DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _SOURCE_ID, PRI_SOURCE_ID_PMU));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP8_MATCH_CFG, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_MATCH, _CFG_TRAP_APPLICATION_LEVEL2, _ENABLE));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP8_MASK, DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MASK, _SOURCE_ID, PRI_SOURCE_ID_PMU) |
                                                           DRF_NUM(_PPRIV_SYS, _PRI_DECODE_TRAP8_MASK, _ADDR, 0x8000));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP8_DATA1, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_DATA1, _V_FOR_ACTION_SET_PRIV_LEVEL, _VALUE_LEVEL_3));
    ACR_REG_WR32(BAR0, LW_PPRIV_SYS_PRI_DECODE_TRAP8_ACTION, DRF_DEF(_PPRIV_SYS, _PRI_DECODE_TRAP8_ACTION, _SET_PRIV_LEVEL, _ENABLE));
}
#endif
/*!
 * @brief Lock target falcon register during LS loading process in ACR ucode.\n
 *        This is a security feature so as to constrain access to only L3 ucode during LS loading and reduce options for attack.
 *
 * @param[in] pFlcnCfg     Falcon config
 * @param[in] setLock      TRUE if reg space is to be locked.
 *
 * @return ACR_OK If opt_selwre_target_mask_wr_selwre fuse is set to L3 and register programming was successful.
 * @return ACR_ERROR_ILWALID_CHIP_ID If opt_selwre_target_mask_wr_selwre is not set to L3.
 */
ACR_STATUS
acrlibLockFalconRegSpace_T234
(
    LwU32 sourceId,
    PACR_FLCN_CONFIG pTargetFlcnCfg,
    LwU32 *pTargetMaskPlmOldValue,
    LwU32 *pTargetMaskOldValue,
    LwBool setLock
)
{

    ACR_STATUS  status                  = ACR_OK;
    LwU32       targetMaskValData       = 0;
    LwU32       targetMaskPlm           = 0;
    LwU32       targetMaskIndexData     = 0;
    LwU32       sourceMask              = 0;
    LwU32       targetMaskRegister      = LW_PPRIV_SYS_TARGET_MASK;
    LwU32       targetMaskPlmRegister   = LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK;
    LwU32       targetMaskIndexRegister = LW_PPRIV_SYS_TARGET_MASK_INDEX;

    if ((pTargetFlcnCfg == LW_NULL) || (pTargetMaskPlmOldValue == LW_NULL) || (pTargetMaskOldValue == LW_NULL))
    {
        status = ACR_ERROR_ILWALID_ARGUMENT;
        goto Cleanup;
    }

    if (sourceId == pTargetFlcnCfg->falconId)
    {
        status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
        return status;
    }

    // All falcons except GPC can be locked through PPRIV_SYS, GPC is locked through PPRIV_GPC
    if (pTargetFlcnCfg->falconId == LSF_FALCON_ID_GPCCS)
    {
        targetMaskRegister      = LW_PPRIV_GPC_GPCS_TARGET_MASK;
        targetMaskPlmRegister   = LW_PPRIV_GPC_GPCS_TARGET_MASK_PRIV_LEVEL_MASK;
        targetMaskIndexRegister = LW_PPRIV_GPC_GPCS_TARGET_MASK_INDEX;
    }

    // Lock falcon
    if (setLock == LW_TRUE)
    {

        // Step 1: Read, store and program TARGET_MASK PLM register.
        targetMaskPlm = ACR_REG_RD32(BAR0, targetMaskPlmRegister);

        // Save plm value to be restored during unlock
        *pTargetMaskPlmOldValue = targetMaskPlm;

        switch (sourceId)
        {
            case LSF_FALCON_ID_PMU:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_PMU;
                break;
            case LSF_FALCON_ID_GSPLITE:
                sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_GSP;
                break;
            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        // Verify if PLM of TARGET_MASK does not allow programming from PMU
        if (!(DRF_VAL(_PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, targetMaskPlm) & BIT(sourceMask)))
        {
            status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
            goto Cleanup;
        }

        //
        // Program PLM of TARGET_MASK registers to be accessible only from PMU.
        // And Block accesses from other sources
        //
        targetMaskPlm = FLD_SET_DRF_NUM( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(sourceMask), targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCK, targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCK, targetMaskPlm);
        ACR_REG_WR32(BAR0, targetMaskPlmRegister, targetMaskPlm);

        // Step 2: Read and program TARGET_MASK_INDEX register.
        targetMaskIndexData = ACR_REG_RD32(BAR0, targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        ACR_REG_WR32(BAR0, targetMaskIndexRegister, targetMaskIndexData);

        // Step 3: Read, store and program TARGET_MASK register.
        targetMaskValData  = ACR_REG_RD32(BAR0, targetMaskRegister);
        // Save Target Mask value to be restored.
        *pTargetMaskOldValue = targetMaskValData;

        // Give only read permission to other PRI sources and Read/Write permission to PMU.
	// TODO: Increase the protection by disabling read permission for other sources. Lwrrently it is not added due to
	// fmodel bug: https://lwbugs/200730932 [T234][VDK][GA10B]:
	// using LW_PPRIV_SYS_TARGET_MASK register to isolate FECS falcon causes TARGET_MASK_VIOLATION when accessing PMU falcon from CPU.
        targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _VAL, _W_DISABLED_R_ENABLED_PL0, targetMaskValData);

        switch (sourceId)
        {
            case LSF_FALCON_ID_PMU:
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _PMU_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
                break;
            case LSF_FALCON_ID_GSPLITE:
                targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _GSP_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
                break;
            default:
                status = ACR_ERROR_PRI_SOURCE_NOT_ALLOWED;
                goto Cleanup;
        }

        ACR_REG_WR32(BAR0, targetMaskRegister, targetMaskValData);
    }
    else // Release lock
    {
        // Reprogram TARGET_MASK_INDEX register.
        targetMaskIndexData = ACR_REG_RD32(BAR0, targetMaskIndexRegister);

        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, pTargetFlcnCfg->targetMaskIndex, targetMaskIndexData);
        ACR_REG_WR32(BAR0, targetMaskIndexRegister, targetMaskIndexData);

        // Restore TARGET_MASK register to previous value.
        targetMaskValData = *pTargetMaskOldValue;
        ACR_REG_WR32(BAR0, targetMaskRegister, targetMaskValData);

        //
        // Restore TARGET_MASK_PLM to previous value to
        // enable all the sources to access TARGET_MASK registers.
        //
        targetMaskPlm = *pTargetMaskPlmOldValue;
        ACR_REG_WR32(BAR0, targetMaskPlmRegister, targetMaskPlm);
    }

Cleanup:
    return status;
}
