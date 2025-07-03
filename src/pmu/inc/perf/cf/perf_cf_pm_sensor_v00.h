/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_pm_sensor_v00.h
 * @brief   PMU PERF_CF PM Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_PM_SENSOR_V00_H
#define PERF_CF_PM_SENSOR_V00_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_PM_SENSOR_V00 PERF_CF_PM_SENSOR_V00;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_PM_SENSOR child class providing attributes of
 * PMU PERF_CF PM Sensor.
 */
struct PERF_CF_PM_SENSOR_V00
{
    /*!
     * PERF_CF_PM_SENSOR parent class.
     * Must be first element of the structure!
     */
    PERF_CF_PM_SENSOR  super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfPmSensorGrpIfaceModel10ObjSetImpl_V00)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPmSensorGrpIfaceModel10ObjSetImpl_V00");
BoardObjIfaceModel10GetStatus           (perfCfPmSensorIfaceModel10GetStatus_V00)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorIfaceModel10GetStatus_V00");

// PERF_CF_PM_SENSOR interfaces.
PerfCfPmSensorLoad      (perfCfPmSensorLoad_V00)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPmSensorLoad_V00");

PerfCfPmSensorSnap      (perfCfPmSensorSnap_V00)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorSnap_V00");

PerfCfPmSensorSignalsUpdate(perfCfPmSensorSignalsUpdate_V00)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorSignalsUpdate_V00");

#endif // PERF_CF_PM_SENSOR_V00_H
