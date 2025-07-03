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
 * @file    perf_cf_topology_min_max.h
 * @brief   Min-max PERF_CF Topology related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_TOPOLOGY_MIN_MAX_H
#define PERF_CF_TOPOLOGY_MIN_MAX_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_topology.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_TOPOLOGY_MIN_MAX PERF_CF_TOPOLOGY_MIN_MAX;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_CF_TOPOLOGY child class providing attributes of
 * Min-max PERF_CF Topology.
 */
struct PERF_CF_TOPOLOGY_MIN_MAX
{
    /*!
     * PERF_CF_TOPOLOGY parent class.
     * Must be first element of the structure!
     */
    PERF_CF_TOPOLOGY    super;
    /*!
     * Index into the Performance Topolgy Table for 1st topology.
     */
    LwU8                topologyIdx1;
    /*!
     * Index into the Performance Topolgy Table for 2nd topology.
     */
    LwU8                topologyIdx2;
    /*!
     * LW_TRUE = max. LW_FALSE = min.
     */
    LwBool              bMax;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfTopologyGrpIfaceModel10ObjSetImpl_MIN_MAX)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfTopologyGrpIfaceModel10ObjSetImpl_MIN_MAX");
BoardObjIfaceModel10GetStatus           (perfCfTopologyIfaceModel10GetStatus_MIN_MAX)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologyIfaceModel10GetStatus_MIN_MAX");

// PERF_CF_TOPOLOGY interfaces.
PerfCfTopologyUpdateTmpReading      (perfCfTopologyUpdateTmpReading_MIN_MAX)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologyUpdateTmpReading_MIN_MAX");
PerfCfTopologyUpdateDependencyMask  (perfCfTopologyUpdateDependencyMask_MIN_MAX)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfTopologyUpdateDependencyMask_MIN_MAX");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_CF_TOPOLOGY_MIN_MAX_H
