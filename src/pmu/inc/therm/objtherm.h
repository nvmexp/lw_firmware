/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    objtherm.h
 * @brief   Declarations, type definitions, and macros for the THERM SW module.
 */

#ifndef OBJTHERM_H
#define OBJTHERM_H

#include "g_objtherm.h"

#ifndef G_OBJTHERM_H
#define G_OBJTHERM_H

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "therm/thrmslct.h"
#include "lib/lib_pwm.h"
#include "pmgr/i2cdev.h"
#include "pmgr/pwrdev.h"
#include "therm/thrmchannel.h"
#include "therm/thrmdevice.h"
#include "therm/thrmpolicy.h"
#include "therm/thrmmon.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmgr/i2cdev.h"
#include "pmgr/pmgrtach.h"
#include "config/g_therm_hal.h"
#include "therm/therm_hal-mock.h"
#include "config/pmu-config.h"

/* ------------------------ Defines ---------------------------------------- */
/*!
 * @brief Known working range of internal sensor in Celsius
 */
#define THERM_INT_SENSOR_WORKING_TEMP_MAX                                  (191)
#define THERM_INT_SENSOR_WORKING_TEMP_MIN                                  (-64)

/*!
 * @brief Thermal sensor types
 */
#define PMU_THERM_SENSOR_GPU_0                                        (LwU8)0x00
#define PMU_THERM_SENSOR_GPU_1                                        (LwU8)0x01
#define PMU_THERM_SENSOR_BOARD                                        (LwU8)0x10
#define PMU_THERM_SENSOR_MEMORY                                       (LwU8)0x11
#define PMU_THERM_SENSOR_ILWALID                                      (LwU8)0xFF

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * @brief   THERM Object definition
 * 
 * @extends BOARDOBJGRP_E32
 */
typedef struct
{
#if PMUCFG_FEATURE_ENABLED(PMUTASK_THERM)
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SW_SLOW))
    /*!
     * @copydoc THERM_RM_PMU_SW_SLOW
     */
    THERM_RM_PMU_SW_SLOW    rmPmuSwSlow;
#endif // (PMUCFG_FEATURE_ENABLED(PMU_THERM_SW_SLOW))

    /*!
     * @copydoc THERM_PMU_EVENT_DATA
     */
    THERM_PMU_EVENT_DATA    evt;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_SENSOR_2X)
    /*!
     * Thermal sensor 2.0 definitions.
     */

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE)
    /*! 
     * @copydoc BOARDOBJGRP_E32
     */
    BOARDOBJGRP_E32         devices;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL)
    /*! 
     * @copydoc BOARDOBJGRP_E32
     */
    BOARDOBJGRP_E32         channels;

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)
    /*! 
     * @copydoc BOARDOBJGRP_E32
     */
    BOARDOBJGRP_E32         policies;

    /*!
     * @brief Mask of valid THERM_POLICY_DOMGRP objects in the ppThermPolicies table.
     */
    LwU32                   domGrpPolicyMask;

    /*!
     * @copydoc LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS
     */
    LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS  lwrrentPerfLimits;

    /*!
     * @copydoc THERM_POLICY_METADATA
     */
    THERM_POLICY_METADATA   policyMetadata;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL)
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE)
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_SENSOR_2X)

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR)
    /*!
     * @copydoc THRM_MON_DATA
     */
    THRM_MON_DATA           monData;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR)
#endif // PMUCFG_FEATURE_ENABLED(PMUTASK_THERM)

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY)
    /*!
     * @brief Tracks if temperature reads from memory unit are allowed.
     */
    LwBool                  bMemTempAvailable;

    /*!
     * @brief Timestamp at memory temperature interface initialization.
     */
    LwU32                   memTempInitTimestamp1024ns;
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_MEMORY_TEMPERATURE_READ_DELAY)

} OBJTHERM;

/* ------------------------ External Definitions --------------------------- */
/*!
 * @brief External reference to @ref OBJTHERM.
 */
extern OBJTHERM             Therm;

/*!
 * @brief External reference to @ref RM_PMU_THERM_CONFIG.
 */
extern RM_PMU_THERM_CONFIG  ThermConfig;

/*!
 * @brief GPU's thermal sensor calibration values in normalized form (F0.16).
 *        Normalized to hide differencies between various GPUs (GF10X -> GF11X).
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_SENSOR))
extern LwS32 ThermSensorA;
extern LwS32 ThermSensorB;
#endif

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
LwU32 thermGetLwrrentTimeInUs(void);

#endif // G_OBJTHERM_H
#endif // OBJTHERM_H
