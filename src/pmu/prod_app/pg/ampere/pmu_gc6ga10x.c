/*
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6ga10x.c
 * @brief  PMU GC6 Hal Functions for GA10X
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "dev_lw_xp.h"
#include "dev_lw_xve.h"
#include "dev_therm.h"
#include "pmu_gc6.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */

/*-------------------------- Macros------------------------------------------ */

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Private Functions ------------------------------ */

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Write LW_XVE_PRIV_XV_0 register in GC6 exit
 */
void
pgGc6ExitSeq_GA10X(void)
{
    LwU32 valXv         = REG_RD32(BAR0_CFG, LW_XVE_PRIV_XV_0);
    LwU32 valXvCya      = REG_RD32(BAR0_CFG, LW_XVE_PRIV_XV_0_CYA_RESET_CFG);

    valXv    = FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L0S_ENABLE, _DISABLED, valXv);
    valXv    = FLD_SET_DRF(_XVE, _PRIV_XV_0, _CYA_L1_ENABLE, _DISABLED, valXv);
    valXvCya = FLD_SET_DRF(_XVE, _PRIV_XV_0_CYA_RESET_CFG, _DL_UP, _NORESET, valXvCya);
    REG_WR32(BAR0_CFG, LW_XVE_PRIV_XV_0, valXv);
    REG_WR32(BAR0_CFG, LW_XVE_PRIV_XV_0_CYA_RESET_CFG, valXvCya);
}

/*!
 * @brief restore CFG space register, bypass if L3
 */
void
pgGc6CfgSpaceRestore_GA10X(LwU32 regOffset, LwU32 regValue)
{
    //
    // TEMP: xve_override_id and xve_rom_ctrl is priv protected to level 3.
    // This code is to unblock tot emu testing until
    // the valid bit map is updated by HW to take PLMs into account
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_GC6_PRIV_AWARE_CONFIG_SPACE_RESTORE)) &&
         ((regOffset == LW_XVE_OVERRIDE_ID)                ||
          (regOffset == LW_XVE_ROM_CTRL)                   ||
          (regOffset == LW_XVE_PRIV_MISC_CYA_RESET_GPU)    ||
          (regOffset == LW_XVE_SW_RESET)))
    {
        goto pgGc6CfgSpaceRestore_GA10X_EXIT;
    }

    REG_WR32(BAR0_CFG, regOffset, regValue);

pgGc6CfgSpaceRestore_GA10X_EXIT:
    return;
}

/*!
 * @brief Write GC6 entry status to scratch register for debug purpose
 *        Any error detected in GC6 entry is fatal and we need to fall into
 *        error path and detect it in the next boot.
 *
 *        In order to log all the error encountered, need to transfer the
 *        status code into mask so we can have multiple error log.
 *
 * @param[in]   status    the checkpoint to update in the scratch register
 */
void
pgGc6UpdateErrorStatusScratch_GA10X()
{
    LwU32 status;

    status = REG_RD32(FECS, LW_PGC6_SCI_LW_SCRATCH_GC6_ENTRY_STATUS);

    if ((GC6_ENTRY_STATUS_INIT_MAGIC == status) &&
        (GC6_ENTRY_STATUS_MASK_INIT != PmuGc6.gc6EntryStatusMask))
    {
        // The first write is a overwrite.
        REG_WR32(FECS, LW_PGC6_SCI_LW_SCRATCH_GC6_ENTRY_STATUS, PmuGc6.gc6EntryStatusMask);
        PmuGc6.gc6EntryStatusMask = GC6_ENTRY_STATUS_MASK_INIT;
    }
}

/*!
 * @brief Init GC6 entry status scratch register for debug purpose
 */
void
pgGc6InitErrorStatusLogger_GA10X()
{
    // Initial entry status mask for SW caching and the register to be written to.
    PmuGc6.gc6EntryStatusMask = GC6_ENTRY_STATUS_MASK_INIT;
    REG_WR32(FECS, LW_PGC6_SCI_LW_SCRATCH_GC6_ENTRY_STATUS, GC6_ENTRY_STATUS_INIT_MAGIC);
}

/*!
 * @brief Poll for link state to change
 * @params[in] gc6DetachMask: the mask is NOP in Ampere
 * @params[out] D3Hot or L2 state
 */
LwU8
pgGc6WaitForLinkState_GA10X(LwU16 gc6DetachMask)
{
    // Skip link state
    if (pgGc6IsOnFmodel_HAL())
    {
        goto pgGc6WaitForLinkState_GA10X_EXIT;
    }

    //
    // this needs to have a better approach.
    // If ACPI call fails; pmu should timeout and recover.
    //
    while (LW_TRUE)
    {
        // RTD3 only check L2 state
        if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_L2, _TRUE, REG_RD32(FECS, LW_XP_PL_LTSSM_STATE)))
        {
            break;
        }

        if (pgGc6IsD3Hot_HAL())
        {
            // Expecting link to be disabled or L2 but not => D3HOT
            return LINK_D3_HOT_CYCLE;
        }
    }

pgGc6WaitForLinkState_GA10X_EXIT:
    // Link is L2
    return LINK_IN_L2_DISABLED;
}

/*!
 * @brief Enable/Disable asserting PCIE WAKE# pin
 * @params[in] gc6DetachMask: the mask is NOP in Ampere
 */
void
pgGc6AssertWakePin_GA10X(LwBool bEnable)
{
    if (bEnable)
    {
        REG_WR32(FECS, LW_PGC6_SCI_XVE_PME, FLD_SET_DRF(_PGC6, _SCI_XVE_PME, _EN, _ENABLE,
            REG_RD32(FECS, LW_PGC6_SCI_XVE_PME)));
    }
    else
    {
        REG_WR32(FECS, LW_PGC6_SCI_XVE_PME, FLD_SET_DRF(_PGC6, _SCI_XVE_PME, _EN, _DISABLE,
            REG_RD32(FECS, LW_PGC6_SCI_XVE_PME)));
    }
}

/*!
 * @brief Copy LWVDD/MSVDD PWM settings from THERM to SCI
 * @params[in] gc6DetachMask: the mask is NOP in Ampere
 * @TODO-SC To use index passed by GPIO parsing from RM instead of hard code index
 */
void
pgGc6IslandConfigPwm_GA10X()
{
    LwU32 target, val;

    // 1. LW_THERM_VID0_PWM(0)_PWM to LW_PGC6_SCI_VID_CFG0_PWM for LWVDD
    target = REF_VAL(LW_THERM_VID0_PWM_PERIOD, REG_RD32(FECS, LW_THERM_VID0_PWM(0))) >> 2;
    val = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(0));
    val = FLD_SET_DRF_NUM(_PGC6, _SCI_VID_CFG0, _PERIOD, target, val);
    REG_WR32(FECS, LW_PGC6_SCI_VID_CFG0(0) , val);

    // 2. LW_THERM_VID0_PWM(0)_HI to LW_PGC6_SCI_VID_CFG1_HI for LWVDD, also trigger the update
    target = REF_VAL(LW_THERM_VID1_PWM_HI, REG_RD32(FECS, LW_THERM_VID1_PWM(0))) >>2;
    val = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG1(0));
    val = FLD_SET_DRF_NUM(_PGC6, _SCI_VID_CFG1, _HI, target, val);
    val = FLD_SET_DRF(_PGC6, _SCI_VID_CFG1, _UPDATE, _TRIGGER, val);
    REG_WR32(FECS, LW_PGC6_SCI_VID_CFG1(0), val);

    // 3. LW_THERM_VID0_PWM(1)_PWM to LW_PGC6_SCI_VID_CFG0_PWM for MSVDD
    val = REG_RD32(FECS, LW_THERM_VID0_PWM(1));
    target =  REF_VAL(LW_THERM_VID0_PWM_PERIOD, val) >> 2;
    val = FLD_SET_DRF_NUM(_PGC6, _SCI_VID_CFG0, _PERIOD, target, val);
    REG_WR32(FECS, LW_PGC6_SCI_VID_CFG0(1), val);

    // 4. LW_THERM_VID0_PWM(0)_HI to LW_PGC6_SCI_VID_CFG1_HI for MSVDD, also trigger the update
    target = REF_VAL(LW_THERM_VID1_PWM_HI, REG_RD32(FECS, LW_THERM_VID1_PWM(1))) >>2;
    val = REG_RD32(FECS, LW_PGC6_SCI_VID_CFG1(1));
    val = FLD_SET_DRF_NUM(_PGC6, _SCI_VID_CFG1, _HI, target, val);
    val = FLD_SET_DRF(_PGC6, _SCI_VID_CFG1, _UPDATE, _TRIGGER, val);
    REG_WR32(FECS, LW_PGC6_SCI_VID_CFG1(1), val);
}

/*!
 * @brief Sanity check for GC6 mode. Only RTD3 is allows for Turing and later
 *        Check starts Ampere so avoid touching previous HAL behavior
 */
GCX_GC6_ENTRY_STATUS
pgGc6EntryCtrlSanityCheck_GA10X(LwBool isRTD3, LwBool isFGC6)
{
    GCX_GC6_ENTRY_STATUS status = GCX_GC6_OK;

    if (isFGC6 || !isRTD3)
    {
        status = GCX_GC6_MODE_ERROR;
    }

    return status;
}
