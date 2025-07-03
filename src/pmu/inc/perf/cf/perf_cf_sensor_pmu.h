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
 * @file    perf_cf_sensor_pmu.h
 * @brief   PMU PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_PMU_H
#define PERF_CF_SENSOR_PMU_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR_PMU PERF_CF_SENSOR_PMU;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_SENSOR child class providing attributes of
 * PMU PERF_CF Sensor.
 */
struct PERF_CF_SENSOR_PMU
{
    /*!
     * PERF_CF_SENSOR parent class.
     * Must be first element of the structure!
     */
    PERF_CF_SENSOR  super;
    /*!
     * Latest sensor reading.
     */
    LwU64           reading;
    /*!
     * HW idle mask 0 value.
     */
    LwU32           idleMask0;
    /*!
     * HW idle mask 1 value.
     */
    LwU32           idleMask1;
    /*!
     * HW idle mask 2 value.
     */
    LwU32           idleMask2;
    /*!
     * Last read HW counter value.
     */
    LwU32           last;
    /*!
     * HW counter index.
     */
    LwU8            counterIdx;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_PMU)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_PMU");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_PMU)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_PMU");

// PERF_CF_SENSOR interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_PMU)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_PMU");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_sensor_pmu_fb.h"

#endif // PERF_CF_SENSOR_PMU_H
