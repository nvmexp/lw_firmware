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
 * @file    perf_cf_sensor.h
 * @brief   Common PERF_CF Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_SENSOR_H
#define PERF_CF_SENSOR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_SENSOR PERF_CF_SENSOR, PERF_CF_SENSOR_BASE;

typedef struct PERF_CF_SENSORS PERF_CF_SENSORS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up PERF_CF_SENSORS.
 */
#define PERF_CF_GET_SENSORS() \
    (&(PERF_CF_GET_OBJ()->sensors))

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PERF_CF_SENSOR \
    (&(PERF_CF_GET_SENSORS()->super.super))

/*!
 * Structure storing statuses of all PERF_CF_SENSORS.
 */
typedef struct
{
    /*!
     * Mask of requested sensors, subset of the mask of all (valid) sensors.
     */
    BOARDOBJGRPMASK_E32 mask;
    /*!
     * Array of PERF_CF_SENSOR statuses.
     */
    RM_PMU_PERF_CF_SENSOR_BOARDOBJ_GET_STATUS_UNION
                        sensors[LW2080_CTRL_PERF_PERF_CF_SENSORS_MAX_OBJECTS];
} PERF_CF_SENSORS_STATUS;

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface   PERF_CF_SENSOR
 *
 * @brief   Loads a PERF_CF_SENSOR.
 *
 * @param[in]   pSensor  PERF_CF_SENSOR object pointer
 *
 * @return  FLCN_OK     PERF_CF_SENSOR loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfSensorLoad(fname) FLCN_STATUS (fname)(PERF_CF_SENSOR *pSensor)

/*!
 * @interface   PERF_CF_SENSOR
 *
 * @brief   Load all PERF_CF_SENSORs.
 *
 * @param[in]   pSensors    PERF_CF_SENSORS object pointer
 * @param[in]   bLoad       LW_TRUE on load(), LW_FALSE on unload().
 *
 * @return  FLCN_OK     All PERF_CF_SENSORs loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfSensorsLoad(fname) FLCN_STATUS (fname)(PERF_CF_SENSORS *pSensors, LwBool bLoad)

/*!
 * @interface   PERF_CF_SENSOR
 *
 * @brief   Retrieve status for the requested sensor objects.
 *
 * @param[in/out]   pStatus     Pointer to the buffer to hold the status.
 *
 * @return  FLCN_OK     On successfull retrieval
 * @return  other       Propagates return values from various calls
 */
#define PerfCfSensorsStatusGet(fname) FLCN_STATUS (fname)(PERF_CF_SENSORS *pSensors, PERF_CF_SENSORS_STATUS *pStatus)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all PERF_CF Sensors.
 */
struct PERF_CF_SENSOR
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ    super;
};

/*!
 * Collection of all PERF_CF Sensors and related information.
 */
struct PERF_CF_SENSORS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32 super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler   (perfCfSensorBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler   (perfCfSensorBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorBoardObjGrpIfaceModel10GetStatus");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfSensorGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus           (perfCfSensorIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorIfaceModel10GetStatus_SUPER");

// PERF_CF_SENSOR(s) interfaces.
PerfCfSensorLoad        (perfCfSensorLoad_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorLoad_SUPER");

PerfCfSensorsLoad       (perfCfSensorsLoad)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfSensorsLoad");
PerfCfSensorsStatusGet  (perfCfSensorsStatusGet)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "perfCfSensorsStatusGet");

FLCN_STATUS perfCfSensorsPostInit(PERF_CF_SENSORS *pSensors)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfSensorsPostInit");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_sensor_pmu.h"
#include "perf/cf/perf_cf_sensor_pex.h"
#include "perf/cf/perf_cf_sensor_ptimer.h"
#include "perf/cf/perf_cf_sensor_clkcntr.h"
#include "perf/cf/perf_cf_sensor_pgtime.h"
#include "perf/cf/perf_cf_sensor_therm_monitor.h"

#endif // PERF_CF_SENSOR_H
