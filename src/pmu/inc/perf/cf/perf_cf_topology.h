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
 * @file    perf_cf_topology.h
 * @brief   Common PERF_CF Topology related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_TOPOLOGY_H
#define PERF_CF_TOPOLOGY_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "perf/cf/perf_cf_sensor.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_TOPOLOGY PERF_CF_TOPOLOGY, PERF_CF_TOPOLOGY_BASE;

typedef struct PERF_CF_TOPOLOGYS PERF_CF_TOPOLOGYS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up PERF_CF_TOPOLOGYS.
 */
#define PERF_CF_GET_TOPOLOGYS() \
    (&(PERF_CF_GET_OBJ()->topologys))

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PERF_CF_TOPOLOGY \
    (&(PERF_CF_GET_TOPOLOGYS()->super.super))

/*!
 * @brief   List of ovl. descriptors required by PERF_CF Topology code paths.
 */
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_TOPOLOGY                                \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_BASE                                    \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfCfTopology)                  \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfTopology)                  \
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER                                     \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLpwr)                         \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_SENSOR_THERM_MONITOR

/*!
 * Structure storing statuses of all PERF_CF_TOPOLOGYS.
 */
typedef struct
{
    /*!
     * Mask of requested topologys, subset of the mask of all (valid) topologys.
     */
    BOARDOBJGRPMASK_E32 mask;
    /*!
     * Array of PERF_CF_TOPOLOGY statuses.
     */
    RM_PMU_PERF_CF_TOPOLOGY_BOARDOBJ_GET_STATUS_UNION
                        topologys[LW2080_CTRL_PERF_PERF_CF_TOPOLOGYS_MAX_OBJECTS];
} PERF_CF_TOPOLOGYS_STATUS;

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface   PERF_CF_TOPOLOGY
 *
 * @brief   Load all PERF_CF_TOPOLOGYs and start timer callback.
 *
 * @param[in]   pTopologys  PERF_CF_TOPOLOGYS object pointer.
 * @param[in]   bLoad       LW_TRUE on load(), LW_FALSE on unload().
 *
 * @return  FLCN_OK     All PERF_CF_TOPOLOGYs loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfTopologysLoad(fname) FLCN_STATUS (fname)(PERF_CF_TOPOLOGYS *pTopologys, LwBool bLoad)

/*!
 * @interface   PERF_CF
 *
 * @brief   Retrieve status for the requested topology objects.
 *
 * @param[in/out]   pStatus     Pointer to the buffer to hold the status.
 *
 * @return  FLCN_OK     On successfull retrieval
 * @return  other       Propagates return values from various calls
 */
#define PerfCfTopologysStatusGet(fname) FLCN_STATUS (fname)(PERF_CF_TOPOLOGYS *pTopologys, PERF_CF_TOPOLOGYS_STATUS *pStatus)

/*!
 * @interface   PERF_CF
 *
 * @brief   Update the mask with the sensor(s) that the topology depends on.
 *
 * @param[in]       pTopology   Pointer to the topology
 * @param[out]      pMask       Pointer to hold sensors mask
 *
 * @return  FLCN_OK     Sensors mask set successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfTopologyUpdateSensorsMask(fname) FLCN_STATUS (fname)(PERF_CF_TOPOLOGY *pTopology, BOARDOBJGRPMASK_E32 *pMask)

/*!
 * @interface   PERF_CF
 *
 * @brief   Callwlate topology reading and cache it temporary.
 *
 * @param[in]   pTopology   Pointer to the topology
 *
 * @return  FLCN_OK     Temporary reading callwlated and updated successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfTopologyUpdateTmpReading(fname) FLCN_STATUS (fname)(PERF_CF_TOPOLOGY *pTopology)

/*!
 * @interface   PERF_CF
 *
 * @brief   Update the mask with the topology(s) that the topology depends on.
 *
 * @param[in]       pTopology   Pointer to the topology
 * @param[out]      pMask       Pointer to hold dependency mask
 * @param[in]       pCheckMask  Pointer to mask of topologys already referenced in relwrsion, to catch loop
 *
 * @return  FLCN_OK     Dependency mask set successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfTopologyUpdateDependencyMask(fname) FLCN_STATUS (fname)(PERF_CF_TOPOLOGY *pTopology, BOARDOBJGRPMASK_E32 *pMask, BOARDOBJGRPMASK_E32 *pCheckMask)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all PERF_CF Topologies.
 */
struct PERF_CF_TOPOLOGY
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ            super;
    /*!
     * Temporary reading. Mainly used by MIN_MAX get status relwrsion.
     */
    LwUFXP40_24         tmpReading;
    /*!
     * Mask of topology(s) referenced, including self.
     */
    BOARDOBJGRPMASK_E32 dependencyMask;
    /*!
     * Tagged for GPUMON logging.
     * @ref LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_<xyz>.
     */
    LwU8                gpumonTag;
    /*!
     * This topology is not actually available (e.g. engine is floorsweept).
     */
    LwBool              bNotAvailable;
};

/*!
 * Collection of all PERF_CF Topologies and related information.
 */
struct PERF_CF_TOPOLOGYS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32             super;
    /*!
     * Structure containing the last periodically polled topologys status.
     */
    PERF_CF_TOPOLOGYS_STATUS   *pPollingStatus;
    /*!
     * Temporary sensors status used by topologys get status.
     */
    PERF_CF_SENSORS_STATUS     *pTmpSensorsStatus;
    /*!
     * Mask of "sense type" (e.g. _SENSED_BASE) topologys.
     */
    BOARDOBJGRPMASK_E32         senseTypeMask;
    /*!
     * Mask of "reference type" (e.g. _MIN_MAX) topologys.
     */
    BOARDOBJGRPMASK_E32         refTypeMask;
    /*!
     * Last known timestamp used to callwlate the delay between samples [ns].
     */
    FLCN_TIMESTAMP              lastGpumonTimestamp;
    /*!
     * RM advertized polling period that will be used by the PMU to poll the
     * corresponding performance sensors [us].
     */
    LwU32                       pollingPeriodus;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler   (perfCfTopologyBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfTopologyBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler   (perfCfTopologyBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfTopologyBoardObjGrpIfaceModel10GetStatus");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfTopologyGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus           (perfCfTopologyIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologyIfaceModel10GetStatus_SUPER");

// PERF_CF_TOPOLOGY interfaces.
PerfCfTopologysLoad                 (perfCfTopologysLoad)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfTopologysLoad");
PerfCfTopologysStatusGet            (perfCfTopologysStatusGet)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologysStatusGet");
PerfCfTopologyUpdateSensorsMask     (perfCfTopologyUpdateSensorsMask)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologyUpdateSensorsMask");
PerfCfTopologyUpdateTmpReading      (perfCfTopologyUpdateTmpReading)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfTopologyUpdateTmpReading");
PerfCfTopologyUpdateDependencyMask  (perfCfTopologyUpdateDependencyMask)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfTopologyUpdateDependencyMask");

FLCN_STATUS perfCfTopologysPostInit(PERF_CF_TOPOLOGYS *pTopologys)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfTopologysPostInit");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_topology_sensed_base.h"
#include "perf/cf/perf_cf_topology_min_max.h"
#include "perf/cf/perf_cf_topology_sensed.h"

#endif // PERF_CF_TOPOLOGY_H
