/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    thrmgv10xonly.c
 * @brief   Thermal PMU HAL functions for GV10X only
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"

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
thermEventViolationPreInit_GV10X(void)
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
    //  - 1 EDPP event:
    //      [0x1C] - RM_PMU_THERM_EVENT_VOLTAGE_HW_ADC
    //
    // GV10X is no longer packing event indexes.
    //
    #define THERM_PMU_EVENT_ARRAY_SIZE_GV10X    29

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_GV10X[THERM_PMU_EVENT_ARRAY_SIZE_GV10X] = {{{ 0 }}};
#else
    static THERM_PMU_EVENT_VIOLATION_ENTRY
        violations_GV10X[THERM_PMU_EVENT_ARRAY_SIZE_GV10X] = {{ 0 }};
#endif

    static FLCN_TIMESTAMP
        notifications_GV10X[THERM_PMU_EVENT_ARRAY_SIZE_GV10X] = {{ 0 }};

    Therm.evt.pViolData       = violations_GV10X;
    Therm.evt.pNextNotif      = notifications_GV10X;
    Therm.evt.maskIntrLevelHw =
        LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE &
        LW_CPWR_THERM_EVENT_MASK_INTERRUPT;
    Therm.evt.maskSupportedHw = LW_CPWR_THERM_EVENT_MASK_HW_FAILSAFE;
}
