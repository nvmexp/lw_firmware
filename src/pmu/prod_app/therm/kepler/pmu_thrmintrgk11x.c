/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmintrgk11x.c
 * @brief   PMU HAL functions for GK11X+ for thermal interrupt control
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Manually handle power supply hot unplug shutdown event
 */
void
thermPwrSupplyHotUnplugShutdownManualTrigger_GM10X(void)
{
    LwU32   overtCtrl;

    // Forcefully assert EXT_OVERT by flipping the ilwersion bit.
    overtCtrl = REG_RD32(CSB, LW_CPWR_THERM_OVERT_CTRL);

    if (FLD_TEST_DRF(_CPWR_THERM, _OVERT_CTRL, _POLARITY, _ACTIVE_LOW,
                     overtCtrl))
    {
        overtCtrl = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _POLARITY,
                                _ACTIVE_HIGH, overtCtrl);
    }
    else
    {
        overtCtrl = FLD_SET_DRF(_CPWR_THERM, _OVERT_CTRL, _POLARITY,
                                _ACTIVE_LOW, overtCtrl);
    }

    REG_WR32(CSB, LW_CPWR_THERM_OVERT_CTRL, overtCtrl);
}

/*!
 * @brief   Initialize static GPU dependant Event Violation data
 */
void
thermEventViolationPreInit_GM10X(void)
{
    //
    // Declaring within HAL interface to reduce visibility.
    //
    // Size is determined as (in given order):
    //  - 3 EXT/GPIO events:
    //      [0x00]<->[0x00] - RM_PMU_THERM_EVENT_EXT_OVERT
    //      [0x01]<->[0x01] - RM_PMU_THERM_EVENT_EXT_ALERT
    //      [0x02]<->[0x02] - RM_PMU_THERM_EVENT_EXT_POWER
    //  - 7 thermal events:
    //      [0x03]<->[0x03] - RM_PMU_THERM_EVENT_OVERT
    //      [0x04]<->[0x04] - RM_PMU_THERM_EVENT_ALERT_0H
    //      [0x05]<->[0x05] - RM_PMU_THERM_EVENT_ALERT_1H
    //      [0x06]<->[0x06] - RM_PMU_THERM_EVENT_ALERT_2H
    //      [0x07]<->[0x07] - RM_PMU_THERM_EVENT_ALERT_3H
    //      [0x08]<->[0x08] - RM_PMU_THERM_EVENT_ALERT_4H
    //      [0x09]<->[0x09] - RM_PMU_THERM_EVENT_ALERT_NEG1H
    //  - 6 BA events:
    //      [0x0A]<->[0x14] - RM_PMU_THERM_EVENT_BA_W0_T1H
    //      [0x0B]<->[0x15] - RM_PMU_THERM_EVENT_BA_W0_T2H
    //      [0x0C]<->[0x16] - RM_PMU_THERM_EVENT_BA_W1_T1H
    //      [0x0D]<->[0x17] - RM_PMU_THERM_EVENT_BA_W1_T2H
    //      [0x0E]<->[0x18] - RM_PMU_THERM_EVENT_BA_W2_T1H
    //      [0x0F]<->[0x19] - RM_PMU_THERM_EVENT_BA_W2_T2H
    //  - 1/2 derived events:
    //      [0x10]<->[0x1A] - RM_PMU_THERM_EVENT_META_ANY_HW_FS
    //      [0x11]<->[0x1B] - RM_PMU_THERM_EVENT_META_ANY_EDP
    //          (only if PMUCFG_FEATURE_ENABLED(PMU_THERM_META_EVENT_ANY_EDP))
    //
    // Gap of 10 indexes (0x0A..0x13) => remap BA & ANY_BA_FS to start after
    // _NEG1H.
    //
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_META_EVENT_ANY_EDP)
    #define THERM_PMU_EVENT_ARRAY_SIZE_GM10X    18
#else
    #define THERM_PMU_EVENT_ARRAY_SIZE_GM10X    17
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_GM10X[THERM_PMU_EVENT_ARRAY_SIZE_GM10X] = {{{ 0 }}};
#else
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_GM10X[THERM_PMU_EVENT_ARRAY_SIZE_GM10X] = {{ 0 }};
#endif

    static FLCN_TIMESTAMP
        notifications_GM10X[THERM_PMU_EVENT_ARRAY_SIZE_GM10X] = {{ 0 }};

    Therm.evt.pViolData       = violations_GM10X;
    Therm.evt.pNextNotif      = notifications_GM10X;
    Therm.evt.maskIntrLevelHw =
        LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE &
        LW_CPWR_THERM_EVENT_MASK_INTERRUPT;
    Therm.evt.maskSupportedHw = LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE;
}

/*!
 * @brief   Transforms generic THERM event index to a packed format.
 *
 * Generic THERM event indexes (RM_PMU_THERM_EVENT_<xyz>) are not conselwtive
 * values (in all cases), therefore they are not very memory efficient if used
 * as an indexes to @ref THERM_PMU_EVENT_VIOLATION_ENTRY arrays. This function
 * transforms a RM_PMU_THERM_EVENT_<xyz> event index to a HAL specific packed
 * format assuring minimum DMEM footprint.
 *
 * @param[in]   evtIdx  Generic THERM event index (RM_PMU_THERM_EVENT_<xyz>)
 *
 * @return  Packed THERM event index
 */
LwU8
thermEventViolationPackEventIndex_GM10X
(
    LwU8    evtIdx
)
{
    //
    // See @ref thermEventViolationPreInit_GM10X():
    // -> Gap of 10 indexes (0x0A..0x13) => remap BA & ANY_BA_FS to start after
    // _NEG1H.
    //
    if (evtIdx >= RM_PMU_THERM_EVENT_BA_W0_T1H)
    {
        evtIdx -= RM_PMU_THERM_EVENT_BA_W0_T1H;
        evtIdx += (RM_PMU_THERM_EVENT_ALERT_NEG1H + 1);
    }

    return evtIdx;
}

/*!
 * @brief   Initialize THERM event interrupts (GPIO + thermal).
 *
 * Performs following operations:
 *  1. Disable all THERM event level interrupts so that they don't trigger
 *      after their level is programmed and before they are actually enabled.
 *  2. Configure all THERM event level interrupts to trigger on HI level.
 *  3. Enable all THERM event level interrupts.
 */
void
thermEventIntrPreInit_GM10X(void)
{
    // #1: Disable all THERM event level interrupts.
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE,
             REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE) &
             (~Therm.evt.maskIntrLevelHw));

    // On GK11X+ default interrupt routing is _TO_PMU -> no action.

    // #2: Configure all THERM event level interrupts to trigger on HI level.
    thermEventLevelIntrCtrlMaskSet_HAL(&Therm, Therm.evt.maskIntrLevelHw);

    // #3: Enable all THERM event level interrupts.
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE,
             REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE) |
             Therm.evt.maskIntrLevelHw);
}

/*!
 * @brief   Returns generic bitmask of THERM events enabled by USE bits
 *
 * @return  Generic bit mask of enabled THERM events
 */
LwU32
thermEventUseMaskGet_GM10X(void)
{
    return thermEventMapHwToGenericMask_HAL(&Therm,
                REG_RD32(CSB, LW_CPWR_THERM_USE_A) &
                REG_RD32(CSB, LW_CPWR_THERM_USE_B));
}

/*!
 * @brief   Returns generic bitmask of THERM events enabled by USE bits
 *
 * @return  Generic bit mask of enabled THERM events
 */
LwU32
thermEventAssertedMaskGet_GM10X(void)
{
    return thermEventMapHwToGenericMask_HAL(&Therm,
                REG_RD32(CSB, LW_CPWR_THERM_EVENT_TRIGGER));
}

/*!
 * @brief   Set THERM event level interrupt's control mask
 *
 * If a particular bit is 1 the THERM event level interrupt should be triggered
 * at HIGH level, otherwise at LOW level.
 *
 * @param[in]   maskIntrLevelHiHw   Level control mask to set
 *
 * @pre     All HW failsafe slowdown interrupts must be disabled prior to
 *          calling this function (with the exception of THERM ISR).
 */
void
thermEventLevelIntrCtrlMaskSet_GM10X
(
    LwU32   maskIntrLevelHiHw
)
{
    LwU32   reg32;

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_TRIGGERED);
    reg32 &= ~Therm.evt.maskIntrLevelHw;
    reg32 |= (maskIntrLevelHiHw & Therm.evt.maskIntrLevelHw);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_TRIGGERED, reg32);

    reg32 = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_NORMAL);
    reg32 &= ~Therm.evt.maskIntrLevelHw;
    reg32 |= (~maskIntrLevelHiHw & Therm.evt.maskIntrLevelHw);
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_NORMAL, reg32);

    // Update level control mask
    Therm.evt.maskIntrLevelHiHw = maskIntrLevelHiHw;
}

/*!
 * @brief   Maps GPU-HAL dependant to GPU-HAL independent THERM event mask.
 *
 * @return  generic bit mask of HW failsafe events
 */
LwU32
thermEventMapHwToGenericMask_GM10X
(
    LwU32   maskHw
)
{
    //
    // Hopefully we will manage to maintain this 1-1 mapping in the future, but
    // mask with supported HW events in case mappings change.
    //
    return maskHw & Therm.evt.maskSupportedHw;
}

/*!
 * @brief   Service THERM event interrupts
 */
void
thermEventIntrService_GM10X(void)
{
    LwU32   reg32;

    //
    // First handle EXT/thermal event interrupts (level triggered ones).
    //
    // We must only look at HW failsafe interrupts which are enabled. Register
    // EVENT_INTR_PENDING holds the state of the interrupt events regardless
    // of whether they actually generate interrupts to HOST/PMU (controlled
    // by EVENT_INTR_ENABLE).
    //
    reg32  = REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_PENDING);
    reg32 &= Therm.evt.maskIntrLevelHw;
    if (reg32 != 0)
    {
        reg32 &= REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
        if (reg32 != 0)
        {
            // Flip polarity of recently triggered level interrupts
            thermEventLevelIntrCtrlMaskSet_HAL(&Therm,
                Therm.evt.maskIntrLevelHiHw ^ reg32);

            // Clear all pending interrupts
            REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_PENDING, reg32);
        }
    }

    //
    // If supported, handle BA event interrupts (edge triggered ones).
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA))
    {
        reg32 = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_TRIGGER_STAT);
        if (reg32 != 0)
        {
            // BA interrupts are edge triggered => just clear them.
            REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_TRIGGER_STAT, reg32);

            //
            // If any bit within PEAKPOWER_TRIGGER_STAT was set, than this in
            // INTR_0 was set as well (and vice versa). Therefore we skip one
            // register read and corresponding check if the bit was set, however
            // we need to clear it (or BA interrupts will never toggle again).
            //
            REG_WR32(CSB, LW_CPWR_THERM_INTR_0,
                     DRF_DEF(_CPWR_THERM, _INTR_0, _THERMAL_TRIGGER_PEAKPOWER,
                             _PENDING));
        }
    }

    // Update THERM event statistics and notify THERM task.
    thermEventService();
}
