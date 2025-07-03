/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrm_intr_gp10x.c
 * @brief   Thermal Interrupt PMU HAL functions for GP10X+
 */

/* ------------------------- System Includes -------------------------------- */
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
 * @brief   Initialize static GPU dependant Event Violation data
 */
void
thermEventViolationPreInit_GP10X(void)
{
    //
    // Declaring within HAL interface to reduce visibility.
    //
    // Size is determined as (in given order):
    //  - 3 EXT/GPIO events:
    //      [0x00] - RM_PMU_THERM_EVENT_EXT_OVERT
    //      [0x01] - RM_PMU_THERM_EVENT_EXT_ALERT
    //      [0x02] - RM_PMU_THERM_EVENT_EXT_POWER
    //  - 12 thermal events:
    //      [0x03] - RM_PMU_THERM_EVENT_THERMAL_0
    //      [0x04] - RM_PMU_THERM_EVENT_THERMAL_1
    //      [0x05] - RM_PMU_THERM_EVENT_THERMAL_2
    //      [0x06] - RM_PMU_THERM_EVENT_THERMAL_3
    //      [0x07] - RM_PMU_THERM_EVENT_THERMAL_4
    //      [0x08] - RM_PMU_THERM_EVENT_THERMAL_5
    //      [0x09] - RM_PMU_THERM_EVENT_THERMAL_6
    //      [0x0A] - RM_PMU_THERM_EVENT_THERMAL_7
    //      [0x0B] - RM_PMU_THERM_EVENT_THERMAL_7
    //      [0x0C] - RM_PMU_THERM_EVENT_THERMAL_9
    //      [0x0D] - RM_PMU_THERM_EVENT_THERMAL_10
    //      [0x0E] - RM_PMU_THERM_EVENT_THERMAL_11
    //  - 5 unused events:
    //      [0x0F]..[0x13]
    //  - 6 BA events:
    //      [0x14] - RM_PMU_THERM_EVENT_BA_W0_T1
    //      [0x15] - RM_PMU_THERM_EVENT_BA_W0_T2
    //      [0x16] - RM_PMU_THERM_EVENT_BA_W1_T1
    //      [0x17] - RM_PMU_THERM_EVENT_BA_W1_T2
    //      [0x18] - RM_PMU_THERM_EVENT_BA_W2_T1
    //      [0x19] - RM_PMU_THERM_EVENT_BA_W2_T2
    //  - 2 derived events:
    //      [0x1A] - RM_PMU_THERM_EVENT_META_ANY_HW_FS
    //      [0x1B] - RM_PMU_THERM_EVENT_META_ANY_EDP
    //
    // GP10X is no longer packing event indexes.
    //
    #define THERM_PMU_EVENT_ARRAY_SIZE_GP10X    28

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_GP10X[THERM_PMU_EVENT_ARRAY_SIZE_GP10X] = {{{ 0 }}};
#else
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_GP10X[THERM_PMU_EVENT_ARRAY_SIZE_GP10X] = {{ 0 }};
#endif

    static FLCN_TIMESTAMP
        notifications_GP10X[THERM_PMU_EVENT_ARRAY_SIZE_GP10X] = {{ 0 }};

    Therm.evt.pViolData       = violations_GP10X;
    Therm.evt.pNextNotif      = notifications_GP10X;
    Therm.evt.maskIntrLevelHw =
        LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE &
        LW_CPWR_THERM_EVENT_MASK_INTERRUPT;
    Therm.evt.maskSupportedHw = LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE;
}

/*!
 * @brief   Transforms generic THERM event index to a packed format.
 *
 * @note    Lwrrently we do NOT pack event indexes on GP10X+.
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
thermEventViolationPackEventIndex_GP10X
(
    LwU8    evtIdx
)
{
    return evtIdx;
}

/*!
 * @brief   Service THERM event interrupts
 */
void
thermEventIntrService_GP10X(void)
{
    LwU32   reg32 = Therm.evt.maskSupportedHw;

    //
    // All event interrupts are now edge triggered. Track only enabled ones.
    //
    // We must only look at HW failsafe interrupts which are enabled. Register
    // EVENT_INTR_PENDING holds the state of the interrupt events regardless or
    // whether they actually generate interrupts to HOST/PMU (controlled by
    // EVENT_INTR_ENABLE).
    //
    reg32 &= REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_PENDING);
    if (reg32 != 0)
    {
        reg32 &= REG_RD32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE);
        if (reg32 != 0)
        {
            // Clear all pending interrupts
            REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_PENDING, reg32);
        }
    }

    // Update THERM event statistics and notify THERM task.
    thermEventService();
}

/*!
 * @brief   Initialize THERM interrupts (GPIO + thermal).
 */
void
thermEventIntrPreInit_GP10X(void)
{
    // #1: All interrupts should be routed to PMU and that is default state.

    // #2: All interrupts should be edge triggered and that is default state.

    // #3: Select all raising THERM event interrupts.
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_TRIGGERED,
             LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE);

    // #4: Select all falling THERM event interrupts.
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ON_NORMAL,
             LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE);

    // #5: Enable all supported THERM event interrupts.
    REG_WR32(CSB, LW_CPWR_THERM_EVENT_INTR_ENABLE,
             LW_CPWR_THERM_EVENT_MASK_EXTERNAL |
             LW_CPWR_THERM_EVENT_MASK_THERMAL);

    // #6: Disable all other THERM interrupts by default.
    REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0,
             RM_PMU_THERM_EVENT_MASK_NONE);
}

/*!
 * @brief   Automatically configure power supply hot unplug shutdown event
 */
void
thermPwrSupplyHotUnplugShutdownAutoConfig_GP10X(void)
{
    LwU32   overtEn;

    //
    // Set EXT_POWER bit in OVERT_EN for OVERT to get asserted on
    // EXT_POWER event.
    //
    overtEn = REG_RD32(CSB, LW_CPWR_THERM_OVERT_EN);
    overtEn =
        FLD_SET_DRF(_CPWR_THERM, _OVERT_EN, _EXT_POWER, _ENABLED, overtEn);
    REG_WR32(CSB, LW_CPWR_THERM_OVERT_EN, overtEn);
}
