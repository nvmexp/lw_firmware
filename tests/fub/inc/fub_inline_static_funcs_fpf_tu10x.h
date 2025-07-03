/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef _FUB_INLINE_STATIC_FUNCS_FPF_TU10X_H_
#define _FUB_INLINE_STATIC_FUNCS_FPF_TU10X_H_

/*!
 * @file: fub_inline_static_funcs_fpf_tu10x.h
 */

/* ------------------------- System Includes -------------------------------- */
#include <falcon-intrinsics.h>
#include "fubutils.h"
#include "fubovl.h"
#include "lwuproc.h"
#include "lwRmReg.h"
#include "fubreg.h"
#include "fubScpDefines.h"
#include "config/fub-config.h"

#include "dev_master.h"
#include "dev_lwdec_csb.h"
#include "dev_sec_csb.h"
#include "dev_therm.h"
#if (FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116))
#include "dev_fpf.h"
#include "dev_fpf_addendum.h"
#endif // FUBCFG_FEATURE_ENABLED(FUBCORE_TU10X) || FUBCFG_FEATURE_ENABLED(FUBCORE_TU116)
#include "dev_therm_vmacro.h"
#include "dev_fuse.h"
#include "dev_gc6_island_addendum.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_fb.h"
#include "dev_se_seb.h"
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

/*
 * @brief: Returns supported binary version blowned in fuses.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubGetHwFuseVersion_TU10X(LwU32 *pFuseVersionHw)
{
    FUB_STATUS fubStatus = FUB_OK;
    if (!pFuseVersionHw)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    //
    // When FUB starts revoking itself, Fuse version will be number of bits burnt in SPARE_FUSE_1 as
    // then reverting already burnt fuse is not possible.
    // But for now FUB relies on SLT to burn this fuse to version value so can be read directly.
    //
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_OPT_FIELD_SPARE_FUSES_1, pFuseVersionHw));

ErrorExit:
    return fubStatus;
}

/*!
 * @brief Get SW ucode version, the define comes from fub-profiles.lwmk -> make-profile.lwmk
 *
 * @param[in] pVersionSw pointer to parameter containg SW Version
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubGetSwUcodeVersion_TU10X(LwU32 *pVersionSw)
{
    FUB_STATUS fubStatus = FUB_OK;
    LwU32      chip      = 0;

    if (!pVersionSw)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PMC_BOOT_42, &chip));
    chip = DRF_VAL( _PMC, _BOOT_42, _CHIP_ID, chip);

    //
    // Added compile time assert to warn in advance when we are close to running out of #bits
    // for FUB version. For fuse allocation refer BUG 200450823.
    //
    CT_ASSERT(FUB_TU102_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_FPF_OPT_FIELD_SPARE_FUSES_1_DATA));
    CT_ASSERT(FUB_TU104_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_FPF_OPT_FIELD_SPARE_FUSES_1_DATA));
    CT_ASSERT(FUB_TU106_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_FPF_OPT_FIELD_SPARE_FUSES_1_DATA));
    CT_ASSERT(FUB_TU10A_UCODE_BUILD_VERSION + 3 < DRF_MASK(LW_FPF_OPT_FIELD_SPARE_FUSES_1_DATA));
    switch(chip)
    {
#ifdef AUTO_FUB
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            *pVersionSw = FUB_TU10A_UCODE_BUILD_VERSION;
            break;
#else
        case LW_PMC_BOOT_42_CHIP_ID_TU102:
            *pVersionSw = FUB_TU102_UCODE_BUILD_VERSION;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU104:
            *pVersionSw = FUB_TU104_UCODE_BUILD_VERSION;
            break;
        case LW_PMC_BOOT_42_CHIP_ID_TU106:
            *pVersionSw = FUB_TU106_UCODE_BUILD_VERSION;
            break;
#endif
        default:
            CHECK_FUB_STATUS(FUB_ERR_UNSUPPORTED_CHIP);
    }

ErrorExit:
    return fubStatus;
}

/*!
 * @brief: Revoke FUB if necessary, based on HW & SW fuse version, and chip ID in PMC_BOOT_42
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubRevokeHsBin_TU10X(void)
{
    LwU32       fuseVersionHW  = 0;
    LwU32       ucodeVersion   = 0;
    FUB_STATUS  fubStatus      = FUB_OK;

    // Check if binary is running on expected chip
    CHECK_FUB_STATUS(fubIsChipSupported_HAL());

    CHECK_FUB_STATUS(fubGetHwFuseVersion_HAL(pFub, &fuseVersionHW));

    CHECK_FUB_STATUS(fubGetSwUcodeVersion_HAL(pFub, &ucodeVersion));

    if (ucodeVersion < fuseVersionHW)
    {
        CHECK_FUB_STATUS(FUB_ERR_BIN_REVOKED);
    }

ErrorExit:
    return fubStatus;
}

/*!
 * @brief Raise/Lower few priv level masks at start/end of FUB
 * FUB is protecting below Registers :-
 *  1) Disable GC6 trigger - GC6 can also control GPIO registers used by FUB for turning on power source.
 *  2) FUSECTRL mask       - HW init value allows write from level 0, so PLMs needs to be raised.
 *  3) VidCtrl PLM         - Fuse burn will fail if LWVDD rail voltage are tampered.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubProtectRegisters_TU10X(FUB_PRIV_MASKS_RESTORE *pPrivMask, LwBool bProtect)
{
    FUB_STATUS fubStatus             = FUB_ERR_GENERIC;
    LwU32      regGc6BsiCtrlMask     = 0;
    LwU32      regGc6SciMastMask     = 0;
    LwU32      regGc6SciSecTimerMask = 0;
    LwU32      regFpfFuseCtrlMask    = 0;
    LwU32      regThermVidPwmMask    = 0;

    if (pPrivMask == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    if (bProtect)
    {
        // Raise PLMs at start of FUB
        if (pPrivMask->bAreMasksRaised == LW_TRUE)
        {
            CHECK_FUB_STATUS(FUB_ERR_BIN_PLM_PROTECTION_ALREADY_RAISED);
        }

        // Disable GC6 trigger
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, &regGc6BsiCtrlMask));
        pPrivMask->gc6BsiCtrlMask = DRF_VAL(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regGc6BsiCtrlMask);
        regGc6BsiCtrlMask          = FLD_SET_DRF(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regGc6BsiCtrlMask);
        regGc6BsiCtrlMask          = FLD_SET_DRF(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regGc6BsiCtrlMask);
        regGc6BsiCtrlMask          = FLD_SET_DRF(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regGc6BsiCtrlMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, regGc6BsiCtrlMask));

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, &regGc6SciMastMask));
        pPrivMask->gc6SciMastMask = DRF_VAL(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regGc6SciMastMask);
        regGc6SciMastMask         = FLD_SET_DRF(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regGc6SciMastMask);
        regGc6SciMastMask         = FLD_SET_DRF(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regGc6SciMastMask);
        regGc6SciMastMask         = FLD_SET_DRF(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regGc6SciMastMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, regGc6SciMastMask));

        // Protect SCI SEC TIMER
        // a) Read PLM
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_SCI_SEC_TIMER_PRIV_LEVEL_MASK, &regGc6SciSecTimerMask));
        // b) Save PLM value for restore
        pPrivMask->gc6SciSecTimerMask = DRF_VAL(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regGc6SciSecTimerMask);
        // c) Update PLM to restrict only LVL 3 access
        regGc6SciSecTimerMask = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regGc6SciSecTimerMask);
        regGc6SciSecTimerMask = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regGc6SciSecTimerMask);
        regGc6SciSecTimerMask = FLD_SET_DRF(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regGc6SciSecTimerMask);
        // d) Write PLM register for settings to take effect
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PGC6_SCI_SEC_TIMER_PRIV_LEVEL_MASK, regGc6SciSecTimerMask));

        // Protect FPF fuse ctrl registers
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSECTRL_PRIV_LEVEL_MASK, &regFpfFuseCtrlMask));
        pPrivMask->fpfFuseCtrlMask = DRF_VAL(_FPF, _FUSECTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regFpfFuseCtrlMask);
        // LW_FPF_FUSECTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ONLY_LEVEL3_ENABLED_FUSE1 has value 0x8
        regFpfFuseCtrlMask         = FLD_SET_DRF(_FPF, _FUSECTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED_FUSE1, regFpfFuseCtrlMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_FUSECTRL_PRIV_LEVEL_MASK, regFpfFuseCtrlMask));

        // Protect LWVDD rail voltage registers
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_THERM_VIDCTRL_PRIV_LEVEL_MASK, &regThermVidPwmMask));
        pPrivMask->thermVidCtrlMask = DRF_VAL(_THERM, _VIDCTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, regThermVidPwmMask);
        regThermVidPwmMask          = FLD_SET_DRF(_THERM, _VIDCTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE, regThermVidPwmMask);
        regThermVidPwmMask          = FLD_SET_DRF(_THERM, _VIDCTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE, regThermVidPwmMask);
        regThermVidPwmMask          = FLD_SET_DRF(_THERM, _VIDCTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE, regThermVidPwmMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_THERM_VIDCTRL_PRIV_LEVEL_MASK, regThermVidPwmMask));

        pPrivMask->bAreMasksRaised = LW_TRUE;
    }
    else
    {
        // Restore PLMs before FUB exit
        if (pPrivMask->bAreMasksRaised == LW_FALSE)
        {
            CHECK_FUB_STATUS(FUB_ERR_BIN_PLM_PROTECTION_NOT_RAISED);
        }

        // Enable GC6 trigger
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, &regGc6BsiCtrlMask));
        regGc6BsiCtrlMask = FLD_SET_DRF_NUM(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->gc6BsiCtrlMask, regGc6BsiCtrlMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK, regGc6BsiCtrlMask));

        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, &regGc6SciMastMask));
        regGc6SciMastMask = FLD_SET_DRF_NUM(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->gc6SciMastMask, regGc6SciMastMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK, regGc6SciMastMask));

        // Restore SCI SEC TIMER PLM

        // a) Read PLM value to be updated
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_PGC6_SCI_SEC_TIMER_PRIV_LEVEL_MASK, &regGc6SciSecTimerMask));
        // b) Restore saved plm settings
        regGc6SciSecTimerMask = FLD_SET_DRF_NUM(_PGC6, _SCI_SEC_TIMER_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->gc6SciSecTimerMask, regGc6SciSecTimerMask);
        // c) Write PLM register for settings to take effect
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_PGC6_SCI_SEC_TIMER_PRIV_LEVEL_MASK, regGc6SciSecTimerMask));

        // Restore FPF fuse ctrl PLM
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSECTRL_PRIV_LEVEL_MASK, &regFpfFuseCtrlMask));
        regFpfFuseCtrlMask = FLD_SET_DRF_NUM(_FPF, _FUSECTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->fpfFuseCtrlMask, regFpfFuseCtrlMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_FUSECTRL_PRIV_LEVEL_MASK, regFpfFuseCtrlMask));

        // Restore LWVDD rail voltage registers
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_THERM_VIDCTRL_PRIV_LEVEL_MASK, &regThermVidPwmMask));
        regThermVidPwmMask = FLD_SET_DRF_NUM(_THERM, _VIDCTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, pPrivMask->thermVidCtrlMask, regThermVidPwmMask);
        CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_THERM_VIDCTRL_PRIV_LEVEL_MASK, regThermVidPwmMask));

        pPrivMask->bAreMasksRaised = LW_FALSE;
    }
ErrorExit:
    return fubStatus;
}

/*!
 * @brief Need to enable and Disable VQPS multiple times so moving them to single function.
 *        Wait till voltage shifts level from 0 to 1.8V in case of enabling and 1.8V to 0 while disabling.
 *
 * @param[in] vqpsEnableDisable Enable or disable VQPS
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubProgramVqpsForVoltageShift_TU10X(FUB_VQPS_OPERATION vqpsEnableDisable)
{
    FUB_STATUS fubStatus = FUB_ERR_VQPS_PROGRAMMING_FAILED;
    LwU32      val       = 0;
    LwU32      valCheck  = 0;

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_DEBUGCTRL, &val));

    // If VQPS is already disabled and request also is to diable it, then no action required
    valCheck = DRF_VAL(_FPF, _DEBUGCTRL, _FUSE_SRC_EN, val);
    if ((valCheck == LW_FPF_DEBUGCTRL_FUSE_SRC_EN_DISABLE) && (vqpsEnableDisable == DISABLE_VQPS))
    {
        fubStatus = FUB_OK;
        goto ErrorExit;
    }

    // If VQPS is already enabled and request also is to enable it, then no action required
    if ((valCheck == LW_FPF_DEBUGCTRL_FUSE_SRC_EN_ENABLE) && (vqpsEnableDisable == ENABLE_VQPS))
    {
        fubStatus = FUB_OK;
        goto ErrorExit;
    }

    switch(vqpsEnableDisable)
    {
        case ENABLE_VQPS:
            val = FLD_SET_DRF(_FPF, _DEBUGCTRL, _FUSE_SRC_EN, _ENABLE, val);
            CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_DEBUGCTRL, val));
            break;
        case DISABLE_VQPS:
            val = FLD_SET_DRF(_FPF, _DEBUGCTRL, _FUSE_SRC_EN, _DISABLE, val);
            CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_DEBUGCTRL, val));
            break;
        default:
            CHECK_FUB_STATUS(FUB_ERR_VQPS_PROGRAMMING_FAILED);
    }

    // Wait till voltage climbs/drops to 1.8V/0V
    CHECK_FUB_STATUS(fubWaitForVoltageShift_HAL());
ErrorExit:
    return fubStatus;
}

/*!
 * @brief Re-sense after burning all fuses.
 *        Re-sense is required for coping updated fuse values to PRIV registers.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubResenseFuse_TU10X(void)
{

    FUB_STATUS fubStatus = FUB_OK;
    LwU32      val       = 0;

    // Write 0=>1=>0 pulse to LW_FPF_PRIV2INTFC
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_PRIV2INTFC, LW_PRIV2INTFC_WRITE_0));
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_PRIV2INTFC, LW_PRIV2INTFC_WRITE_1));
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_PRIV2INTFC, LW_PRIV2INTFC_WRITE_0));

    // Wait for Fuse Sensing to be over
    fubReportStatus_HAL(pFub, FUB_ERR_BIN_WAITING_FOR_FUSE_SENSE);
    do
    {
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSECTRL, &val));
        val = DRF_VAL(_FPF, _FUSECTRL, _FUSE_SENSE_DONE, val);
    } while(val != LW_FPF_FUSECTRL_FUSE_SENSE_DONE_YES );

    // FUB is out of loop, so clear error code
    fubReportStatus_HAL(pFub, FUB_OK);

ErrorExit:
    return fubStatus;
}


/*!
 * @brief Clear all registers before Exit
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubClearFpfRegisters_TU10X(void)
{
    FUB_STATUS fubStatus = FUB_OK;
    LwU32      val       = 0;
    // LW_FPF_FUSEADDR is accessible from NON HS levels as well, so clearing it here
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSEADDR, &val));
    val = FLD_SET_DRF(_FPF, _FUSEADDR, _VLDFLD, _INIT, val);
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_FUSEADDR, val));

    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSEWDATA, &val));
    // coverity[bit_and_with_zero]
    val = FLD_SET_DRF(_FPF, _FUSEWDATA, _DATA, _INIT, val);
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_FUSEWDATA, val));

ErrorExit:
    return fubStatus;
}


/*!
 * @brief Burn Fuse. This function expects and so checks FUSECTRL PLM to be raised to HS level.
 *        Below steps are followed for burning fuse
 *        1) Write Fuse row address in LW_FPF_FUSEADDR
 *        2) Write version to which fuse is to be burnt in LW_FPF_FUSEWDATA.
 *        3) Issue write command LW_FPF_FUSECTRL_CMD_WRITE
 *        4) Wait till status LW_FPF_FUSECTRL_STATE reads LW_FPF_FUSECTRL_STATE_IDLE
 *
 * @param[in] fuseAddr Row number of fuse.
 * @param[in] data Value to be written to fuse.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubWriteFieldProgFuse_TU10X(LwU32 fuseAddr, LwU32 data)
{
    FUB_STATUS fubStatus          = FUB_OK;
    LwU32      val                = 0;
    LwU32      regFpfFuseCtrlMask = 0;

    // Data to be burnt in Fuse can not be 0.
    if (data == 0)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

    // Check if Row number is valid
    if (fuseAddr < LW_FPF_ADDRESS_ALLOWED_RANGE_LOWER_LIMIT || fuseAddr >  LW_FPF_ADDRESS_ALLOWED_RANGE_UPPER_LIMIT)
    {
        CHECK_FUB_STATUS(FUB_ERR_FUSE_NOT_IN_RANGE);
    }

    //
    // PLM of control register allows access from NS level as well. At the start of FUB,
    // it is raised to be writable from only HS level, so Non HS unit does not interfere with FUB.
    // Exit early if PLM is not raised.
    //
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSECTRL_PRIV_LEVEL_MASK, &regFpfFuseCtrlMask));
    if (!FLD_TEST_DRF(_FPF, _FUSECTRL_PRIV_LEVEL_MASK, _WRITE_PROTECTION, _ONLY_LEVEL3_ENABLED_FUSE1, regFpfFuseCtrlMask))
    {
        CHECK_FUB_STATUS(FUB_ERR_FUSECTRL_PLM_NOT_RAISED);
    }

    // Write Row address and data to be burnt
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSEADDR, &val));
    fuseAddr = FLD_SET_DRF_NUM(_FPF, _FUSEADDR, _VLDFLD, fuseAddr, val);
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_FUSEADDR, fuseAddr));

    // coverity[bit_and_with_zero]
    data = FLD_SET_DRF_NUM(_FPF, _FUSEWDATA, _DATA, data, data );
    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_FUSEWDATA, data));

    // Issue Burn Command
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSECTRL, &val));
    val = FLD_SET_DRF(_FPF, _FUSECTRL, _CMD, _WRITE, val);

    CHECK_FUB_STATUS(FALC_REG_WR32(BAR0, LW_FPF_FUSECTRL, val));

    // Wait for Burning to be over
    fubReportStatus_HAL(pFub, FUB_ERR_BIN_WAITING_FOR_FUSECTRL_TO_IDLE_AFTER_WRITE);
    do
    {
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, LW_FPF_FUSECTRL, &val));
        val = DRF_VAL(_FPF, _FUSECTRL, _STATE, val);
    } while(val != LW_FPF_FUSECTRL_STATE_IDLE );

    // FUB is out of loop, so clear error code in mailbox
    fubReportStatus_HAL(pFub, FUB_OK);

ErrorExit:
    return fubStatus;
}

#if FUBCFG_FEATURE_ENABLED(FUB_BURN_SELF_REVOCATION_FUSE)
/*!
 * @brief Create structure with all the information required for burning Self Revocation Fuse.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubFillFuseInfoForSelfRevocation_TU10X(FPF_FUSE_INFO *pFuseInfo)
{
    LwU32      fuseBurlwalue = 0;
    FUB_STATUS fubStatus     = FUB_OK;

    // Return early if pointer passed is NULL
    if (pFuseInfo == NULL)
    {
        CHECK_FUB_STATUS(FUB_ERR_ILWALID_ARGUMENT);
    }

#ifdef FUB_ON_LWDEC
    // Bit from LW_FPF_OPT_LWDEC_UCODE1_VERSION will be burnt to revoke FUB
    pFuseInfo->fuseBar0Address = LW_FPF_OPT_LWDEC_UCODE1_VERSION;
#elif FUB_ON_SEC2
    // Bit from LW_FPF_OPT_SEC2_UCODE7_VERSION will be burnt to revoke FUB
    pFuseInfo->fuseBar0Address = LW_FPF_OPT_SEC2_UCODE7_VERSION;
#endif
    // Check to make sure Fuse burn value is as expected
    ct_assert(LW_FPF_OPT_SEC2_FUSE_UCODE_SELF_REVOKE_BUG_2623776 == BIT(FUSE_VERSION + 1) - 1);

    // 
    // Fuse value here should be set here such that all FUB will burn all bits till Intended Version.
    // e.g. If FUB is signed at version 2, fubSelfRevoke_ function should increase it to 3. Here fuseIntendedVal should be 0b111(0x7)
    // to make sure 0th and 1st position bits are also burned. 
    //
    pFuseInfo->fuseIntendedVal = LW_FPF_OPT_SEC2_FUSE_UCODE_SELF_REVOKE_BUG_2623776; 
#ifdef FUB_ON_LWDEC
    pFuseInfo->fusePriAddress  = LW_FPF_OPT_LWDEC_UCODE1_VERSION__PRI_ALIAS_0;
    pFuseInfo->fuseRedAddress  = LW_FPF_OPT_LWDEC_UCODE1_VERSION__RED_ALIAS_0;
#elif FUB_ON_SEC2
    pFuseInfo->fusePriAddress  = LW_FPF_OPT_SEC2_UCODE7_VERSION__PRI_ALIAS_0;
    pFuseInfo->fuseRedAddress  = LW_FPF_OPT_SEC2_UCODE7_VERSION__RED_ALIAS_0;
#endif
    // Zero out already Burnt Fuse bits.
    CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, pFuseInfo->fuseBar0Address, &fuseBurlwalue));

    //
    //  Fuse values will increase in below fashion,
    //  1) LW_FPF_OPT_LWDEC_UCODE1_VERSION will increment in 0x1, 0x3, 0x7, ...
    //  2) Falcon BROM will read these as 0x1, 0x2, 0x3, ...
    //  3) siggen needs to be passed with values in #2 i.e. 0x1, 0x2, 0x3, ... 
    //
    
    fuseBurlwalue = (pFuseInfo->fuseIntendedVal) & (~fuseBurlwalue);
    // Not all fuses start at 0th bit of row. So adjusting fuse value as per location of fuse in row.
#ifdef FUB_ON_LWDEC
    pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_LWDEC_UCODE1_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
#elif FUB_ON_SEC2
    pFuseInfo->fuseAdjustedBurlwalue = DRF_NUM(_FPF, _OPT_SEC2_UCODE7_VERSION, __PRI_ALIAS_0_DATA , fuseBurlwalue);
#endif

ErrorExit:
    return fubStatus;
}

/*!
 * @brief   FUB is being shipped out of LWPU for SKU rework use case. To reduce attack surface, FUB
 *          is switched to use ENHANCED AES sig verif and it will also revoke itself by burning UCODE1 fuse.
 *          It will limit FUB exelwtion to 1 run per chip with current version.
 *
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline
FUB_STATUS
fubSelfRevoke_TU10X()
{
    FUB_STATUS    fubStatus     = FUB_ERR_GENERIC;
    LwU32         fuseBar0Value = 0;
    FPF_FUSE_INFO fuseInfo;

    // Populate Fuse Information Structure
    CHECK_FUB_STATUS(fubFillFuseInfoForSelfRevocation_HAL(pFub, &fuseInfo));

    // Proceed to Burning only if New fuseValue bits are not already burnt
    if (fuseInfo.fuseAdjustedBurlwalue)
    {
        // Burn Primary Fuse
        CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fusePriAddress, fuseInfo.fuseAdjustedBurlwalue));

        // Burn Redundant Fuse
        CHECK_FUB_STATUS(fubWriteFieldProgFuse_HAL(pFub, fuseInfo.fuseRedAddress, fuseInfo.fuseAdjustedBurlwalue));

        // Resense here to make sure Fuse values are copied to BAR0 priv register and limit attack surface.
        CHECK_FUB_STATUS(fubResenseFuse_HAL());

        // Make sure Fuse burn is reflected in BAR0 mapping of Fuse.
        CHECK_FUB_STATUS(FALC_REG_RD32(BAR0, fuseInfo.fuseBar0Address, &fuseBar0Value));
        if (fuseBar0Value != fuseInfo.fuseIntendedVal)
        {
            CHECK_FUB_STATUS(FUB_ERR_SELF_REVOCATION_FAILED);
        }
    }
ErrorExit:
    return fubStatus;
}
#endif    // FUBCFG_FEATURE_ENABLED(FUB_BURN_SELF_REVOCATION_FUSE)


#endif // _FUB_INLINE_STATIC_FUNCS_FPF_TU10X_H_
