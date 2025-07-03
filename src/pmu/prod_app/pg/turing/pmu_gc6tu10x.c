/*
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6tu10x.c
 * @brief  PMU GC6 Hal Functions for TU10X
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pmgr.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "pmu_bar0.h"
#include "dev_master.h"
#include "dev_pwr_pri.h"
#include "dev_fuse.h"
#include "dev_fuse_addendum.h"
#include "dev_therm.h"
#include "dev_trim.h"
#include "dev_top.h"
#include "pmu_gc6.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */

// Size of BSI descriptor in dwords
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE              5
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_PRIV       4
#define LW_PGC6_SCI_TO_THERM_MULTIPLIER                  4

// 2ms for PME_TO_ack timeout, see See http://lwbugs/2452328/74
#define GCX_GC6_PME_TO_ACK_TIMEOUT_2_MSEC                (2*1000*1000)
/*-------------------------- Macros------------------------------------------ */

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Helper function to check if the current platform is FMODEL
 *
 * @return  LW_TRUE     The platform is FMODEL
 *          LW_FALSE    otherwise
 */
LwBool
pgGc6IsOnFmodel_TU10X(void)
{
    LwU32  val = REG_RD32(BAR0, LW_PTOP_PLATFORM);

    return FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, val);
}

/*!
 * @brief Helper function to check if the current platform is RTL
 *
 * @return  LW_TRUE     The platform is RTL
 *          LW_FALSE    otherwise
 */
LwBool
pgGc6IsOnRtl_TU10X(void)
{
    LwU32  val = REG_RD32(BAR0, LW_PTOP_PLATFORM);

    return FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _RTL, val);
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief RTD3 init settings. Lwrrently to disable PME_TO_ACK
 */
void
pgGc6PreInitRtd3Entry_TU10X()
{
    // Disable XVE auto send to PME_TO_ACKS
    LwU32 val;

    // For RTD3, only RTD3_CPU_EVENT should be set
    val = DRF_DEF(_PGC6, _SCI_WAKE_EN, _RTD3_CPU_EVENT, _ENABLE);
    REG_WR32(FECS, LW_PGC6_SCI_WAKE_EN, val);

    //
    // For RTD3, pexrst# will always be a rising edge so we can ignore it.
    // HW suggests to set all bit enable except _RTD3_CPU_EVENT
    // See manual for detail
    //
    val = (DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _GPU_EVENT,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _WAKE_TIMER,     _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _I2CS,           _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _BSI_EVENT,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _RTD3_CPU_EVENT, _DISABLE) |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _HPD_IRQ_0,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _HPD_IRQ_1,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _HPD_IRQ_2,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _HPD_IRQ_3,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _HPD_IRQ_4,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _HPD_IRQ_5,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _HPD_IRQ_6,      _ENABLE)  |
           DRF_DEF(_PGC6, _SCI_HW_INTR_EN, _RTD3_GPU_EVENT, _ENABLE));
    REG_WR32(FECS, LW_PGC6_SCI_HW_INTR_EN, val);

    //Enable SW intr status report of RTD3 events
    val = REG_RD32(FECS, LW_PGC6_SCI_SW_INTR0_EN);
    val = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _RTD3_CPU_EVENT, _ENABLE, val);
    val = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _RTD3_GPU_EVENT, _ENABLE, val);
    val = FLD_SET_DRF(_PGC6, _SCI_SW_INTR0_EN, _GC6_EXIT, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_SCI_SW_INTR0_EN, val);

}

/*!
 * @brief GC6 SCI trigger for Turing and onward chips.
 * @param[in]  isRTD3 True for RTD3 and False for legacy GC6
 */
void
pgGc6IslandTrigger_TU10X(LwBool isRTD3, LwBool isFGC6)
{
    LwU32 val;
    LwU32 val2;

    //
    // To trigger reset only when driver unload is finish.
    // Host reset implemented and if we do island trigger too early the host reset
    // affects engine post unload status so need to wait here
    // Bubble will write the value in RTD3 entry
    // The magic value is 0xdeadcafe
    //
    if (pgGc6IsOnFmodel_HAL())
    {
        while (LW_TRUE)
        {
            val = REG_RD32(FECS, LW_PGC6_BSI_SCRATCH(LW_PGC6_BSI_SCRATCH_INDEX_BSI_RESET_SYNC));
            if (val == GC6_ON_FMODEL)
            {
                // clear for next run
                REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(LW_PGC6_BSI_SCRATCH_INDEX_BSI_RESET_SYNC), 0x0);
                break;
            }
        }
    }

    // actual island trigger behavior starts here
    val = REG_RD32(FECS, LW_PGC6_BSI_CTRL);
    val = FLD_SET_DRF(_PGC6, _BSI_CTRL, _MODE, _GC6, val);

    val2 = REG_RD32(FECS, LW_PGC6_SCI_CNTL);

    if (isRTD3)
    {
        val2 = FLD_SET_DRF(_PGC6, _SCI_CNTL, _ENTRY_TRIGGER, _RTD3, val2);
    }
    else
    {
        val2 = FLD_SET_DRF(_PGC6, _SCI_CNTL, _ENTRY_TRIGGER, _GC6, val2);
    }

    // trigger the writes close-by
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, val);
    REG_WR32(FECS, LW_PGC6_SCI_CNTL, val2);
}

/*!
 * @brief Sanity check if PMU can trigger island for GC6 entry
 */
GCX_GC6_ENTRY_STATUS
pgGc6EntryPlmSanityCheck_TU10X()
{
    GCX_GC6_ENTRY_STATUS status = GCX_GC6_OK;

    if (FLD_TEST_DRF(_PGC6, _SCI_MAST_FSM_PRIV_LEVEL_MASK,_WRITE_PROTECTION, _LEVEL2_DISABLE,
            REG_RD32(FECS, LW_PGC6_SCI_MAST_FSM_PRIV_LEVEL_MASK)))
    {
        status |= GCX_GC6_SCI_PLM_ERROR;
    }

    if (FLD_TEST_DRF(_PGC6, _BSI_CTRL_PRIV_LEVEL_MASK ,_WRITE_PROTECTION, _LEVEL2_DISABLE,
            REG_RD32(FECS, LW_PGC6_BSI_CTRL_PRIV_LEVEL_MASK)))
    {
        status |= GCX_GC6_BSI_PLM_ERROR;
    }

    return status;
}

/*
 * Patch the writes to LW_THERM_VID* so we have the optimized voltage set
 * for GC6 exit when devinit runs
 */
void
pgGc6PatchForOptimizedVoltage_TU10X(LwU32 phase)
{
    LwU32 val;
    LwU32 ramCtrl;
    LwU32 count;
    LwU32 numDwords;
    LwU32 srcOffset;
    LwU32 addr;
    LwU32 target;

    // These registers are level 2 protected, so we have to patch that too
    LwU32 bar0Ctl =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                      DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _TRIG, _TRUE) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _2 ));

    addr = ((phase * LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE) + LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_PRIV) * sizeof(LwU32);
    ramCtrl = 0;
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _DISABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, addr, ramCtrl);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);

    // parse the number of dwords and location of actual writes in the priv section
    val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
    numDwords = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_NUMWRITES, val);
    srcOffset = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_SRCADDR, val);

    // Set priv section address in BSI RAM
    ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _ENABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, srcOffset, ramCtrl);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);
    count = 0;

    while (count < numDwords)
    {
        // Read the first dword in the sequence
        val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

        // Check if this is a BAR0 or priv write
        if (FLD_TEST_DRF(_PGC6, _BSI_RAMDATA, _OPCODE, _BAR0, val))
        {
            // check to see if we have our target
            if (val == (LW_THERM_VID0_PWM(0) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0))
            {
                break;
            }
            // dummy reads to skip data and ctl section
            REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
            REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

            count += LW_PGC6_BAR0_OP_SIZE_DWORD;
        }
        else
        {
            // dummy read to skip data section
            REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

            count += LW_PGC6_PRIV_OP_SIZE_DWORD;
        }
    }

    // We didn't find anything to patch.
    if (count >= numDwords)
    {
        return;
    }

    // See bug 1751032 for the detailed sequence

    // 1. LW_THERM_VID0_PWM(0) -> set period to 4x value in corresponding SCI_VID
    val = REG_RD32(FECS, LW_THERM_VID0_PWM(0));
    target = LW_PGC6_SCI_TO_THERM_MULTIPLIER * REF_VAL(LW_PGC6_SCI_VID_CFG0_PERIOD, REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(0)));
    val = FLD_SET_DRF_NUM(_THERM, _VID0_PWM, _PERIOD, target, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 2. LW_THERM_VID0_PWM(1) -> trigger bit and 4x interval in corresponding SCI SEQ table
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_THERM_VID0_PWM(1) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = LW_THERM_VID_PWM_TRIGGER_MASK_SETTING_NEW_TRIGGER |
          (LW_PGC6_SCI_TO_THERM_MULTIPLIER*REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(6)));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 3. LW_THERM_VID1_PWM(0) -> set period to 4x value in corresponding SCI_VID
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_THERM_VID1_PWM(0) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = REG_RD32(FECS, LW_THERM_VID1_PWM(0));
    target = LW_PGC6_SCI_TO_THERM_MULTIPLIER * REF_VAL(LW_PGC6_SCI_VID_CFG0_PERIOD, REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(1)));
    val = FLD_SET_DRF_NUM(_THERM, _VID0_PWM, _PERIOD, target, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 4. LW_THERM_VID1_PWM(1) -> trigger bit and 4x interval in corresponding SCI SEQ table
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_THERM_VID1_PWM(1) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = LW_THERM_VID_PWM_TRIGGER_MASK_SETTING_NEW_TRIGGER |
          (LW_PGC6_SCI_TO_THERM_MULTIPLIER*REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(7)));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

}

/*!
 * @brief power island configuration function before trigger
 *
 * This function sets up BSI/SCI settings so SCI/BSI GPIO/INTR can work
 * as configured.
 */
void
pgGc6IslandConfig_TU10X
(
    RM_PMU_GC6_DETACH_DATA *pGc6Detach
)
{
    LwU16 gc6DetachMask = pGc6Detach->gc6DetachMask;
    LwU32 override      = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_OVERRIDE_SEQ_STATE);
    LwU32 entryVec0     = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC0);
    LwU32 entryVec1     = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC1);
    LwU32 fsmCfg        = REG_RD32(FECS, LW_PGC6_SCI_FSM_CFG);

    // legacy behavior before Turing and later specific change
    pgGc6DisableAbortAndClearInterrupt_HAL();

    // To copy LWVDD/MSVDD PWM from THERM to SCI, Ampere and later only
    pgGc6IslandConfigPwm_HAL();

    if (!pGc6Detach->bVbiosFeatureControl)
    {
        return;
    }

    if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK, _RTD3_GC6_ENTRY, _TRUE, gc6DetachMask))
    {
        //
        // RTD3 settings
        // Update SCI power sequencer if the AUX is switched or not
        // Default is  (a) 12V ON - Low (Keep 12V) ,12V OFF - High (Switch to 3V3 AUX)
        // IDLE_IN_SW GPIO control has to be implemented after GC6_FB_EN is asserted @ RTD3 transition.
        //
        if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                         _RTD3_AUX_RAIL_SWITCH, _TRUE, gc6DetachMask))
        {
            override = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE,
                           _IDLE_IN_SW, _INSTR, override);
        }
        else
        {
            override = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE,
                           _IDLE_IN_SW, _DEASSERT, override);
        }

        override = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE,
                       _PEXVDD_VID, _DEASSERT, override);

        entryVec0 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC0,
                        _GC6_POWERUP, pGc6Detach->rtd3Gc6.powerUp, entryVec0);
        entryVec0 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC0,
                        _GC6_POWERDOWN, pGc6Detach->rtd3Gc6.powerDown, entryVec0);

        entryVec1 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC1,
                        _GC6_ENTRY, pGc6Detach->rtd3Gc6.entry, entryVec1);
        entryVec1 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC1,
                        _GC6_EXIT, pGc6Detach->rtd3Gc6.exit, entryVec1);

        fsmCfg = FLD_SET_DRF(_PGC6, _SCI_FSM_CFG, _INTERNAL_RESET_MON,
                     _DISABLE, fsmCfg);
    }
    else if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK, _FAST_GC6_ENTRY, _TRUE, gc6DetachMask))
    {
        // FAST GC6 settings
        override = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE, _IDLE_IN_SW,
                       _DEASSERT, override);
        override = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE, _PEXVDD_VID,
                       _INSTR, override);

        entryVec0 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC0, _GC6_POWERUP,
                        pGc6Detach->fastGc6.powerUp, entryVec0);
        entryVec0 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC0, _GC6_POWERDOWN,
                        pGc6Detach->fastGc6.powerDown, entryVec0);

        entryVec1 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC1, _GC6_ENTRY,
                        pGc6Detach->fastGc6.entry, entryVec1);
        entryVec1 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC1, _GC6_EXIT,
                        pGc6Detach->fastGc6.exit, entryVec1);

        fsmCfg = FLD_SET_DRF(_PGC6, _SCI_FSM_CFG, _INTERNAL_RESET_MON,
                     _ENABLE, fsmCfg);
    }
    else
    {
        // Legacy GC6 Settings
        override = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE, _IDLE_IN_SW,
                       _DEASSERT, override);
        override = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_OVERRIDE_SEQ_STATE, _PEXVDD_VID,
                       _DEASSERT, override);

        entryVec0 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC0, _GC6_POWERUP,
                        pGc6Detach->legacyGc6.powerUp, entryVec0);
        entryVec0 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC0, _GC6_POWERDOWN,
                        pGc6Detach->legacyGc6.powerDown, entryVec0);

        entryVec1 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC1, _GC6_ENTRY,
                        pGc6Detach->legacyGc6.entry, entryVec1);
        entryVec1 = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ_ENTRY_VEC1, _GC6_EXIT,
                        pGc6Detach->legacyGc6.exit, entryVec1);

        fsmCfg = FLD_SET_DRF(_PGC6, _SCI_FSM_CFG, _INTERNAL_RESET_MON,
                     _ENABLE, fsmCfg);
    }

    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_OVERRIDE_SEQ_STATE, override);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC0, entryVec0);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_ENTRY_VEC1, entryVec1);
    REG_WR32(FECS, LW_PGC6_SCI_FSM_CFG, fsmCfg);
}

/*!
 * @brief A 1ms delay is added into the SCI sequence when switching
 * from AUX to 12V on RTD3 GC6 exit. This delay is not necessary on
 * unsupported platforms like NB. Delay table index from Bug2254706
 * added to dev_gc6_island_addendum.h
 *
 * @param[in]  bAuxRailSupported TRUE if AUX rail switch is supported on this platform
 */
void
pgGc6Rtd3VoltRemoveSciDelay_TU10X(void)
{
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_DELAY_TABLE(LW_PGC6_SCI_DELAY_TABLE_IDX_RTD3_AUX_RAIL_SWITCH), 0);
}

/*
 * @brief Turn smartfan off in GC6/RTD3 entry
 * @param[in] void
 *
 * @return The error status of fanCoolerPwmSet()
 */
GCX_GC6_ENTRY_STATUS
pgGc6TurnFanOff_TU10X(void)
{
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
    FAN_COOLER   *pCooler;
    LwBoardObjIdx i;
    FLCN_STATUS   status  = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(FAN_COOLER, pCooler, i, NULL)
    {
        // TODO - SC. return fan index as part of the error information.
        status = fanCoolerPwmSet(pCooler, LW_FALSE, LW_GC6_RTD3_PWM_VALUE);
    }
    BOARDOBJGRP_ITERATOR_END;

    if (status != FLCN_OK)
    {
        goto pgGc6TurnFanOff_EXIT;
    }

    return GCX_GC6_OK;

pgGc6TurnFanOff_EXIT:
    return GCX_GC6_FAN_OFF_ERROR;
#else //PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_MULTI_FAN)
    return GCX_GC6_OK;
#endif
}

