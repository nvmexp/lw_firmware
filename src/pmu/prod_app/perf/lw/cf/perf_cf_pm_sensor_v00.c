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
 * @file    perf_cf_pm_sensor_v00.c
 * @copydoc perf_cf_pm_sensor_v00.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "dev_chiplet_pwr.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor_v00.h"
#include "pmu_objpmu.h"
#include "perf/cf/perf_cf.h"
#include "config/g_perf_cf_private.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Maximum signals supported for GPCFBP for V00
 */
#define PERF_CF_PM_SENSORS_V00_SIGNALS_GPCFBP_MAX  385
/*!
 * Maximum signals supported for XBAR for V00
 */
#define PERF_CF_PM_SENSORS_V00_SIGNALS_XBAR_MAX    5
/*!
 * Maximum signals supported for V00 = GPCFBP + XBAR
 */
#define PERF_CF_PM_SENSORS_V00_SIGNALS_MAX         (PERF_CF_PM_SENSORS_V00_SIGNALS_GPCFBP_MAX + PERF_CF_PM_SENSORS_V00_SIGNALS_XBAR_MAX)

/*!
 * Time taken for the PM signals to reflect in PM ram after they are snapped
 */
#define PERF_CF_PM_SENSORS_V00_SNAP_TIME_NS       15000
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
FLCN_TIMESTAMP lwrrentTime;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPmSensorGrpIfaceModel10ObjSetImpl_V00
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_PM_SENSOR_V00 *pDescPmSensor =
        (RM_PMU_PERF_CF_PM_SENSOR_V00 *)pBoardObjDesc;
    PERF_CF_PM_SENSOR_V00 *pPmSensorV00 =
        (PERF_CF_PM_SENSOR_V00 *)*ppBoardObj;;
    FLCN_STATUS  status;

    status = perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10, 
                 ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorGrpIfaceModel10ObjSetImpl_V00_exit;
    }

    // Initialise variables here
    (void)pPmSensorV00;
    (void)pDescPmSensor;

perfCfPmSensorGrpIfaceModel10ObjSetImpl_V00_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfPmSensorIfaceModel10GetStatus_V00
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_PM_SENSOR_V00_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_PM_SENSOR_V00_GET_STATUS *)pPayload;
    PERF_CF_PM_SENSOR_V00 *pPmSensorV00 = (PERF_CF_PM_SENSOR_V00 *)pBoardObj;
    FLCN_STATUS status;

    status = perfCfPmSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorIfaceModel10GetStatus_V00_exit;
    }

    // Populate V00 status here ....
    (void)pStatus;
    (void)pPmSensorV00;

perfCfPmSensorIfaceModel10GetStatus_V00_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorLoad
 */
FLCN_STATUS
perfCfPmSensorLoad_V00
(
    PERF_CF_PM_SENSOR *pSensor
)
{
    PERF_CF_PM_SENSOR_V00 *pSensorV00 = (PERF_CF_PM_SENSOR_V00 *)pSensor;
    FLCN_STATUS            status     = FLCN_OK;
    LwU32                  regVal;

    status = perfCfPmSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorLoad_V00_exit;
    }

    // V00 class implementation.
    (void)pSensorV00;

    // Enable data collection for PM sensor in training mode
    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                             _PRIV_POLLING_MODE, _ON, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL, regVal);

perfCfPmSensorLoad_V00_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorSnap
 */
FLCN_STATUS
perfCfPmSensorSnap_V00
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    LwU32 regVal;

    // BA in functional mode should be disabled
    if (FLD_TEST_DRF(_CPWR_THERM, _CONFIG1, _BA_ENABLE, _YES,
        REG_RD32(CSB, LW_CPWR_THERM_CONFIG1))          ||
        FLD_TEST_DRF(_PCHIPLET_PWR_FBPS, _CONFIG_1, _BA_ENABLE, _YES,
        REG_RD32(BAR0, LW_PCHIPLET_PWR_FBPS_CONFIG_1)) ||
        FLD_TEST_DRF(_PCHIPLET_PWR_GPCS, _CONFIG_1, _BA_ENABLE, _YES,
        REG_RD32(BAR0, LW_PCHIPLET_PWR_GPCS_CONFIG_1)))
    {
        PMU_BREAKPOINT();
        return  FLCN_ERR_NOT_SUPPORTED;
    }

    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                             _MTRAIN_SWITCH, _OFF, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL, regVal);

    // Get the time when the signals are snapped
    osPTimerTimeNsLwrrentGet(&lwrrentTime);

    return FLCN_OK;
}

/*!
 * @copydoc PerfCfPmSensorSignalsUpdate
 */
FLCN_STATUS
perfCfPmSensorSignalsUpdate_V00
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    LwU32          regVal;
    LwU64          data64;
    LwBoardObjIdx  hwIdx;
    LwBoardObjIdx  swIdx;
    LwU32          elapsedTimeNs;
    FLCN_STATUS    status = FLCN_OK;

    //
    // It takes around 1500 ticks of utils clock @108MHz to get the data in
    // PM Ram. So wait for ~ 15 us.
    //
    elapsedTimeNs = osPTimerTimeNsElapsedGet(&lwrrentTime);

    if (PERF_CF_PM_SENSORS_V00_SNAP_TIME_NS > elapsedTimeNs)
    {
        OS_PTIMER_SPIN_WAIT_NS(PERF_CF_PM_SENSORS_V00_SNAP_TIME_NS
                               - elapsedTimeNs);
    }

    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_MTRAIN_SNAP_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_MTRAIN_SNAP_CTRL,
                             _INDEX, _INIT, regVal);

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pPmSensor->signalsSupportedMask, swIdx)
    {
        if (swIdx < PERF_CF_PM_SENSORS_V00_SIGNALS_GPCFBP_MAX)
        {
            hwIdx  = swIdx;
            regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_MTRAIN_SNAP_CTRL,
                                 _SEL, _GPCFBP, regVal);
        }
        else if (swIdx < PERF_CF_PM_SENSORS_V00_SIGNALS_MAX)
        {
            hwIdx  = swIdx - PERF_CF_PM_SENSORS_V00_SIGNALS_GPCFBP_MAX;
            regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_MTRAIN_SNAP_CTRL,
                                 _SEL, _XBAR, regVal);
        }
        else
        {
            // This should really be a PMU_BREAKPOINT() and failure.
            continue;
        }

        regVal = FLD_SET_DRF_NUM(_CPWR_THERM,
                    _BA_TRAINING_MTRAIN_SNAP_CTRL, _INDEX, hwIdx, regVal);
        regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_MTRAIN_SNAP_CTRL,
                    _SNAP, _TRIGGER, regVal);
        REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_MTRAIN_SNAP_CTRL, regVal);

        // Wait until the SNAP has CLEARED
        while (!FLD_TEST_DRF(_CPWR_THERM, _BA_TRAINING_MTRAIN_SNAP_CTRL, _SNAP, _CLEARED,
                             REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_MTRAIN_SNAP_CTRL)))
        {
            lwosNOP();
        }

        data64 = (LwU64)REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_MTRAIN_DATA_LOW);
        lw64Add(&pPmSensor->pSignals[swIdx], &pPmSensor->pSignals[swIdx], &data64);

        data64 = (LwU64)REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_MTRAIN_DATA_HIGH);
        lw64Lsl(&data64, &data64, 32);
        lw64Add(&pPmSensor->pSignals[swIdx], &pPmSensor->pSignals[swIdx], &data64);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                             _MTRAIN_SWITCH, _ON, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL, regVal);

    return status;
}
