/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_pm_sensor_clw_mig_v10.h
 * @brief   PMU PERF_CF PM CLW MIG Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_PM_SENSOR_CLW_MIG_V10_H
#define PERF_CF_PM_SENSOR_CLW_MIG_V10_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_PM_SENSOR_CLW_MIG_V10 PERF_CF_PM_SENSOR_CLW_MIG_V10;

/*!
 * PERF_CF_PM_SENSOR child class providing attributes of
 * PMU PERF_CF PM Sensor.
 */
struct PERF_CF_PM_SENSOR_CLW_MIG_V10
{
    /*!
     * PERF_CF_PM_SENSOR parent class.
     * Must be first element of the structure!
     */
    PERF_CF_PM_SENSOR  super;

    /*!
     * The ID of the MIG object, It is set
     * by RM with the value of MIG's swizzId during MIG creation
     */
    LwU8 swizzId;

    /*!
     * The GPC/FBP/SYS configuration (enablement) of
     * the MIG object
     */
    LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_CFG migCfg;
};


/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_MIG_V10)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_MIG_V10");
BoardObjIfaceModel10GetStatus           (perfCfPmSensorIfaceModel10GetStatus_CLW_MIG_V10)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorIfaceModel10GetStatus_CLW_MIG_V10");

// PERF_CF_PM_SENSOR interfaces.
PerfCfPmSensorLoad      (perfCfPmSensorLoad_CLW_MIG_V10)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPmSensorLoad_CLW_MIG_V10");

PerfCfPmSensorSnap      (perfCfPmSensorSnap_CLW_MIG_V10)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorSnap_CLW_MIG_V10");

PerfCfPmSensorSignalsUpdate(perfCfPmSensorSignalsUpdate_CLW_MIG_V10)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorSignalsUpdate_CLW_MIG_V10");

#endif // PERF_CF_PM_SENSOR_CLW_MIG_V10_H
