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
 * @file    perf_cf_pm_sensor.h
 * @brief   Common PERF_CF PM Sensor related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_PM_SENSOR_H
#define PERF_CF_PM_SENSOR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_PM_SENSOR PERF_CF_PM_SENSOR, PERF_CF_PM_SENSOR_BASE;

typedef struct PERF_CF_PM_SENSORS PERF_CF_PM_SENSORS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up PERF_CF_PM_SENSORS.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))
#define PERF_CF_PM_GET_SENSORS() (&(PERF_CF_GET_OBJ()->pmSensors))
#else
#define PERF_CF_PM_GET_SENSORS() ((PERF_CF_PM_SENSORS *)NULL)
#endif

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR))
#define BOARDOBJGRP_DATA_LOCATION_PERF_CF_PM_SENSOR                           \
    (&(PERF_CF_PM_GET_SENSORS()->super.super))
#endif

/*!
 * @copydoc boardObjGrpMaskInit_E32
 */
#define PERF_CF_PM_SENSORS_STATUS_MASK_INIT(_pMask)                           \
    do                                                                        \
    {                                                                         \
        boardObjGrpMaskInit_E32(_pMask);                                      \
    } while (LW_FALSE)                                                        \

/*!
 * @brief   List of ovl. descriptors required by PERF_CF PM sensors code paths.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PM_SENSORS                             \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_BASE                                   \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfCfPmSensors)                \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfPmSensors)
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_PM_SENSORS                             \
        OVL_INDEX_ILWALID
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)

/*!
 * Structure storing statuses of all PERF_CF_PM_SENSORS.
 */
typedef struct
{
    /*!
     * Mask of requested sensors, subset of the mask of all (valid) sensors.
     */
    BOARDOBJGRPMASK_E32 mask;
    /*!
     * Array of PERF_CF_PM_SENSOR statuses.
     */
    RM_PMU_PERF_CF_PM_SENSOR_BOARDOBJ_GET_STATUS_UNION
        pmSensors[LW2080_CTRL_PERF_PERF_CF_PM_SENSORS_MAX_OBJECTS];
} PERF_CF_PM_SENSORS_STATUS;

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface   PERF_CF_PM_SENSOR
 *
 * @brief   Loads a PERF_CF_PM_SENSOR.
 *
 * @param[in]   pPmSensor  PERF_CF_PM_SENSOR object pointer
 *
 * @return  FLCN_OK     PERF_CF_PM_SENSOR loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPmSensorLoad(fname) FLCN_STATUS (fname)(PERF_CF_PM_SENSOR *pPmSensor)

/*!
 * @interface   PERF_CF_PM_SENSOR
 *
 * @brief   Load all PERF_CF_PM_SENSORs.
 *
 * @param[in]   pPmSensors    PERF_CF_PM_SENSORS object pointer
 * @param[in]   bLoad       LW_TRUE on load(), LW_FALSE on unload().
 *
 * @return  FLCN_OK     All PERF_CF_PM_SENSORs loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPmSensorsLoad(fname) FLCN_STATUS (fname)(PERF_CF_PM_SENSORS *pPmSensors, LwBool bLoad)

/*!
 * @interface   PERF_CF_PM_SENSOR
 *
 * @brief   Retrieve status for the requested sensor objects.
 *
 * @param[in/out]   pStatus   Pointer to the buffer to hold the status.
 *
 * @return  FLCN_OK     On successfull retrieval
 * @return  other       Propagates return values from various calls
 */
#define PerfCfPmSensorsStatusGet(fname) FLCN_STATUS (fname)(PERF_CF_PM_SENSORS *pPmSensors, PERF_CF_PM_SENSORS_STATUS *pStatus)

/*!
 * @brief Trigger BA PM data collection of each PM sensor
 *
 * @param[in] pPmSensor    PM sensor object
 *
 * @return FLCN_OK
 *            If the function is supported on GPU
 *         FLCN_ERR_NOT_SUPPORTED
 *            If the function is not supported on GPU
 */
#define PerfCfPmSensorSnap(fname) FLCN_STATUS (fname)(PERF_CF_PM_SENSOR *pPmSensor)

/*!
 * @brief Update sw continuous signal counter 
 *
 * @param[in] pPmSensor    PM sensor object
 *
 * @return FLCN_OK
 *            If the function is supported on GPU
 *         FLCN_ERR_NOT_SUPPORTED
 *            If the function is not supported on GPU
 */
#define PerfCfPmSensorSignalsUpdate(fname) FLCN_STATUS (fname)(PERF_CF_PM_SENSOR *pPmSensor)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all PERF_CF PM Sensors.
 */
struct PERF_CF_PM_SENSOR
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ             super;
    /*!
     * Mask of signals which this PERF_CF_PM_SENSOR supports.
     */
    BOARDOBJGRPMASK_E1024 signalsSupportedMask;
    /*!
     * A 64-bit continuously aclwmulating/incrementing counter array. This
     * will wrap-around on overflow. Valid indexes in this array correspond
     * to bits set in the @ref::signalsSupportedMask.
     */
    LwU64               *pSignals;
};

/*!
 * @brief Structure representing ELCG WAR for the BA PM sensor.  If
 * ELCG is engaged when SW initiates _SW_TRIGGER_SNAP = _TRIGGER, we
 * can see potential corruption of signals (on GA10X/_V10 -
 * partilwlarly GPC signals).  To WAR this, PMU will disable ELCG
 * around snap triggers.
 */
typedef struct {
    /*!
     * @brief Boolean indicating whether the ELCG WAR is enabled on
     * this GPU.  Will be requested by individual PERF_CF_PM_SENSOR
     * objects if they determine that they need it.
     */
    LwBool bEnabled;
} PERF_CF_PM_SENSORS_SNAP_ELCG_WAR;

/*!
 * Collection of all PERF_CF PM Sensors and related information.
 */
struct PERF_CF_PM_SENSORS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32 super;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_SNAP_ELCG_WAR)
    /*!
     * @copydoc PERF_CF_PM_SENSORS_SNAP_ELCG_WAR
     */
    PERF_CF_PM_SENSORS_SNAP_ELCG_WAR snapElcgWar;
#endif
};

/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * @brief Helper accessor function to retrieve the @ref
 * PERF_CF_PM_SENSOR_V10_ELCG_WAR structure from a corresponding @ref
 * PERF_CF_PM_SENSOR_V10 object.
 *
 * @param[in]  pPmSensors
 *     @ref PERF_CF_PM_SENSORS object from which to retrieve its
 *     @ref PERF_CF_PM_SENSORS_SNAP_ELCG_WAR structure.
 * @param[out] ppSnapElcgWar
 *     Pointer in which to return the pointer to the
 *     @ref PERF_CF_PM_SENSORS_SNAP_ELCG_WAR structure.
 *
 * @return FLCN_OK
 *     @ref PERF_CF_PM_SENSORS_SNAP_ELCG_WAR pointer successfully retrieved.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Called provided invalid (i.e. NULL) pointer arguments.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPmSensorsSnapElcgWarGet
(
    PERF_CF_PM_SENSORS                *pPmSensors,
    PERF_CF_PM_SENSORS_SNAP_ELCG_WAR **ppSnapElcgWar
)
{
    FLCN_STATUS status = FLCN_OK;

    // NULL-check parameters.
    PMU_ASSERT_OK_OR_GOTO(status,
        (((pPmSensors != NULL) &&
          (ppSnapElcgWar != NULL)) ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        perfCfPmSensorsSnapElcgWarGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR_SNAP_ELCG_WAR)
    *ppSnapElcgWar = &pPmSensors->snapElcgWar;
#else
    *ppSnapElcgWar = NULL;
#endif

perfCfPmSensorsSnapElcgWarGet_exit:
    return status;
}

/*!
 * @brief Helper to initialize a @ref PERF_CF_PM_SENSORS_SNAP_ELCG_WAR
 * structure.
 *
 * @param[out] pSnapElcgWar
 *     Pointer to @ref PERF_CF_PM_SENSORS_SNAP_ELCG_WAR structure to
 *     initialize.
 *
 * @return FLCN_OK
 *     @ref PERF_CF_PM_SENSORS_SNAP_ELCG_WAR structure succesfully initialized.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Called provided invalid (i.e. NULL) pointer arguments.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfPmSensorsSnapElcgWarInit
(
    PERF_CF_PM_SENSORS_SNAP_ELCG_WAR *pSnapElcgWar
)
{
    FLCN_STATUS status = FLCN_OK;

    // NULL-check parameters.
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pSnapElcgWar != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT),
        perfCfPmSensorsSnapElcgWarInit_exit);

    pSnapElcgWar->bEnabled = LW_FALSE;

perfCfPmSensorsSnapElcgWarInit_exit:
    return status;
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10CmdHandler   (perfCfPmSensorBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPmSensorBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler   (perfCfPmSensorBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPmSensorBoardObjGrpIfaceModel10GetStatus");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus           (perfCfPmSensorIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorIfaceModel10GetStatus_SUPER");

// PERF_CF_PM_SENSOR(s) interfaces.
PerfCfPmSensorLoad        (perfCfPmSensorLoad_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPmSensorLoad_SUPER");

PerfCfPmSensorsLoad       (perfCfPmSensorsLoad)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfPmSensorsLoad");
PerfCfPmSensorsStatusGet  (perfCfPmSensorsStatusGet)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "perfCfPmSensorsStatusGet");

FLCN_STATUS perfCfPmSensorsPostInit(PERF_CF_PM_SENSORS *pPmSensors)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfPmSensorsPostInit");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_pm_sensor_v00.h"
#include "perf/cf/perf_cf_pm_sensor_v10.h"
#include "perf/cf/perf_cf_pm_sensor_clw_dev_v10.h"
#include "perf/cf/perf_cf_pm_sensor_clw_mig_v10.h"

#endif // PERF_CF_PM_SENSOR_H
