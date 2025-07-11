/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_sensor_pgtime.h
 * @brief   PGTIME PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_PGTIME_H
#define PERF_CF_SENSOR_PGTIME_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR_PGTIME PERF_CF_SENSOR_PGTIME;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_SENSOR child class providing attributes of
 * PGTIME PERF_CF Sensor.
 */
struct PERF_CF_SENSOR_PGTIME
{
    /*!
     * PERF_CF_SENSOR parent class.
     * Must be first element of the structure!
     */
    PERF_CF_SENSOR  super;
    /*!
     * Power gating engine ID to sample.
     * @ref LW2080_CTRL_MC_POWERGATING_ENGINE_ID_<xyz>.
     */
    LwU8            pgEngineId;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_PGTIME");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_PGTIME)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_PGTIME");

// PERF_CF_SENSOR interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_PGTIME)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_PGTIME");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_SENSOR_PGTIME_H
