/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_sensor_ptimer.h
 * @brief   PTIMER PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_PTIMER_H
#define PERF_CF_SENSOR_PTIMER_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR_PTIMER PERF_CF_SENSOR_PTIMER;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_SENSOR child class providing attributes of
 * PTIMER PERF_CF Sensor.
 */
struct PERF_CF_SENSOR_PTIMER
{
    /*!
     * PERF_CF_SENSOR parent class.
     * Must be first element of the structure!
     */
    PERF_CF_SENSOR  super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_PTIMER)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_PTIMER");

// PERF_CF_SENSOR interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_PTIMER)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_PTIMER");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_sensor_ptimer_clk.h"

#endif // PERF_CF_SENSOR_PTIMER_H
