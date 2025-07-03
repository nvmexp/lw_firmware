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
 * @file    perf_cf_topology_sensed.h
 * @brief   Sensed Base Common PERF_CF Topology related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_TOPOLOGY_SENSED_H
#define PERF_CF_TOPOLOGY_SENSED_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_topology.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_TOPOLOGY child class providing attributes of
 * Sensed Base PERF_CF Topology.
 */
typedef struct
{
    /*!
     * PERF_CF_TOPOLOGY parent class.
     * Must be first element of the structure!
     */
    PERF_CF_TOPOLOGY    super;
    /*!
     * Index into the Performance Sensor Table for sensor.
     */
    LwBoardObjIdx       sensorIdx;
} PERF_CF_TOPOLOGY_SENSED;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfTopologyGrpIfaceModel10ObjSetImpl_SENSED");
BoardObjIfaceModel10GetStatus           (perfCfTopologyIfaceModel10GetStatus_SENSED)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologyIfaceModel10GetStatus_SENSED");

// PERF_CF_TOPOLOGY interfaces.
PerfCfTopologyUpdateSensorsMask (perfCfTopologyUpdateSensorsMask_SENSED)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologyUpdateSensorsMask_SENSED");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_TOPOLOGY_SENSED_H
