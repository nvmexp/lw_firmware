/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_sensor_therm_monitor.h
 * @brief   Thermal Monitor PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_THERM_MONITOR_H
#define PERF_CF_SENSOR_THERM_MONITOR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR_THERM_MONITOR PERF_CF_SENSOR_THERM_MONITOR;

/* ------------------------ Macros and Defines ------------------------------ */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_SENSOR_THERM_MONITOR))
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_SENSOR_THERM_MONITOR                       \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, therm)
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_SENSOR_THERM_MONITOR                       \
        OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_SENSOR child class providing attributes of
 * Thermal Monitor PERF_CF Sensor.
 */
struct PERF_CF_SENSOR_THERM_MONITOR
{
    /*!
     * PERF_CF_SENSOR parent class.
     * Must be first element of the structure!
     */
    PERF_CF_SENSOR  super;
    /*!
     * Thermal monitor object index to sample.
     */
    LwU8            thrmMonIdx;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_THERM_MONITOR)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_THERM_MONITOR");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_THERM_MONITOR)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_THERM_MONITOR");

// PERF_CF_SENSOR interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_THERM_MONITOR)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_THERM_MONITOR");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_SENSOR_THERM_MONITOR_H
