/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_boot_gsprm_tu10x.c
 */

//
// Includes
//
#include "booter.h"

#include "dev_gsp.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_sys_addendum.h"

static BOOTER_STATUS s_booterLockGspRegSpace(const LwBool, LwU32 *, LwU32 *) ATTR_OVLY(OVL_NAME);

/*
 * @brief 
 */
BOOTER_STATUS
booterResetAndPollForGsp_TU10X
(
    LwBool bSaveAndRestoreMailboxRegs
)
{
    BOOTER_STATUS status = BOOTER_OK;
    LwU32 mailbox0       = 0;
    LwU32 mailbox1       = 0;

    if (bSaveAndRestoreMailboxRegs)
    {
        // Save mailbox values
        mailbox0 = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_MAILBOX0);
        mailbox1 = BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_MAILBOX1);
    }

    // Put GSP in reset first
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_ENGINE, LW_PGSP_FALCON_ENGINE_RESET_TRUE);

    // Dummy read
    BOOTER_REG_RD32(BAR0, LW_PGSP_FALCON_ENGINE);

    // Bring GSP out of reset
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_ENGINE, LW_PGSP_FALCON_ENGINE_RESET_FALSE);

    // Poll GSP for IMEM|DMEM scrubbing
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterPollForScrubbing_HAL(pBooter));
 
    if (bSaveAndRestoreMailboxRegs)
    {
        // Restore mailbox values
        BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_MAILBOX0, mailbox0);
        BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_MAILBOX1, mailbox1);
    }

    return BOOTER_OK;
}

/*!
 * Use PRI Source Isolation to lock GSP register space to the engine
 * on which Booter is running
 *
 * @param[in]     bLock  whether to lock or unlock register space
 * @param[inout]  pTargetMaskPlmOldValue
 * @param[inout]  pTargetMaskOldValue
 */
static BOOTER_STATUS
s_booterLockGspRegSpace
(
    const LwBool bLock,
    LwU32 *pTargetMaskPlmOldValue,
    LwU32 *pTargetMaskOldValue
)
{
    BOOTER_STATUS status = BOOTER_OK;

    LwU32 targetMaskValData;
    LwU32 targetMaskPlm;
    LwU32 targetMaskIndexData;

    const LwU32 targetMaskIndex = LW_PPRIV_SYS_PRI_MASTER_fecs2gsp_pri;  // aka. target is GSP

#if defined(BOOTER_LOAD)
    const LwU32 sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_SEC2;  // aka. source is SEC2
#elif defined(BOOTER_RELOAD)
    const LwU32 sourceMask = LW_PRIV_LEVEL_MASK_SOURCE_ENABLE_LWDEC0;  // aka. source is LWDEC0
#else
    return BOOTER_ERROR_UNKNOWN;
#endif

    if (bLock)
    {
        //
        // Step 1: Read, store and program TARGET_MASK PLM register.
        //
        targetMaskPlm = BOOTER_REG_RD32(BAR0, LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK);

        // Save plm value to be restored during unlock
        *pTargetMaskPlmOldValue = targetMaskPlm;

        // Verify if PLM of TARGET_MASK does not allow programming from Booter's engine
        if (!(DRF_VAL(_PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, targetMaskPlm) & BIT(sourceMask)))
        {
            return BOOTER_ERROR_PRI_SOURCE_NOT_ALLOWED;
        }

        // Program PLM of TARGET_MASK registers to be accessible only from Booter's engine
        targetMaskPlm = FLD_SET_DRF_NUM( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_ENABLE, BIT(sourceMask), targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCK, targetMaskPlm);
        targetMaskPlm = FLD_SET_DRF( _PPRIV, _SYS_TARGET_MASK_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCK, targetMaskPlm);
        BOOTER_REG_WR32(BAR0, LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK, targetMaskPlm);
 
        //
        // Step 2: Read and program TARGET_MASK_INDEX register.
        //
        targetMaskIndexData = BOOTER_REG_RD32(BAR0, LW_PPRIV_SYS_TARGET_MASK_INDEX);
 
        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, targetMaskIndex, targetMaskIndexData);
        BOOTER_REG_WR32(BAR0, LW_PPRIV_SYS_TARGET_MASK_INDEX, targetMaskIndexData);
 
        //
        // Step 3: Read, store and program TARGET_MASK register.
        //
        targetMaskValData  = BOOTER_REG_RD32(BAR0, LW_PPRIV_SYS_TARGET_MASK);

        // Save Target Mask value to be restored
        *pTargetMaskOldValue = targetMaskValData;
 
        // Give only Read permission to other PRI sources and Read/Write permission to Booter's engine
        targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _VAL, _W_DISABLED_R_ENABLED_PL0, targetMaskValData);

        // Give Read/Write permisssion to Booter's engine PRI source
#if defined(BOOTER_LOAD)
        targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _SEC2_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
#elif defined(BOOTER_RELOAD)
        targetMaskValData = FLD_SET_DRF(_PPRIV, _SYS_TARGET_MASK, _LWDEC0_ACCESS_CONTROL, _RW_ENABLED, targetMaskValData);
#else
        return BOOTER_ERROR_UNKNOWN;
#endif

        BOOTER_REG_WR32(BAR0, LW_PPRIV_SYS_TARGET_MASK, targetMaskValData);
    }
    else
    {
        // Reprogram TARGET_MASK_INDEX register
        targetMaskIndexData = BOOTER_REG_RD32(BAR0, LW_PPRIV_SYS_TARGET_MASK_INDEX);
 
        targetMaskIndexData = FLD_SET_DRF_NUM( _PPRIV_SYS, _TARGET_MASK_INDEX, _INDEX, targetMaskIndex, targetMaskIndexData);
        BOOTER_REG_WR32(BAR0, LW_PPRIV_SYS_TARGET_MASK_INDEX, targetMaskIndexData);
 
        // Restore TARGET_MASK register to previous value
        targetMaskValData = *pTargetMaskOldValue;
        BOOTER_REG_WR32(BAR0, LW_PPRIV_SYS_TARGET_MASK, targetMaskValData);
 
        // Restore TARGET_MASK_PLM to previous value
        targetMaskPlm = *pTargetMaskPlmOldValue;
        BOOTER_REG_WR32(BAR0, LW_PPRIV_SYS_TARGET_MASK_PRIV_LEVEL_MASK, targetMaskPlm);
    }

    return status;
}

/*
 * @brief 
 */
BOOTER_STATUS
booterSetupGspRiscv_TU10X(GspFwWprMeta *pWprMeta)
{
    BOOTER_STATUS status;

    LwU32 targetMaskPlmOldValue;
    LwU32 targetMaskOldValue;

    // Lock GSP register space (via PRI source isolation)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(s_booterLockGspRegSpace(LW_TRUE, &targetMaskPlmOldValue, &targetMaskOldValue));

    // Reset the GSP RISCV
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterResetAndPollForGsp_HAL(pBooter, LW_TRUE));

    // Set up target registers
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterSetupTargetRegisters_HAL(pBooter));
    
    //
    // Set the bootvec, RISCV can run directly from FB
    //
    // Note: on Turing and GA100, this requires loading a small stub to first
    // set uaccessattr, which will then jump to the BL.
    //
    // See Bug 2031674 for more information.
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterSetupBootvec_HAL(pBooter, 2, pWprMeta));

    // Unlock GSP register space (via PRI source isolation)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(s_booterLockGspRegSpace(LW_FALSE, &targetMaskPlmOldValue, &targetMaskOldValue));

    return BOOTER_OK;
}

/*!
 * @brief
 */
BOOTER_STATUS
booterPrepareGspRm_TU10X(GspFwWprMeta *pWprMeta)
{
    BOOTER_STATUS status;

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterSetupGspRiscv_HAL(pBooter, pWprMeta));

#if defined(BOOTER_LOAD)
    // Program GSP sub-WPR for GSP-RM to use (all levels R/W, covering all WPR2 except FRTS)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(booterProgramSubWprGsp_HAL(pBooter,
        GSP_SUB_WPR_ID_0_UCODE_CODE_SECTION_WPR2, LW_TRUE,
        pWprMeta->gspFwWprStart, pWprMeta->frtsOffset,
        BOOTER_SUB_WPR_MMU_RMASK_ALL_LEVELS_ENABLED, BOOTER_SUB_WPR_MMU_WMASK_ALL_LEVELS_ENABLED));
#endif

    return status;
}

