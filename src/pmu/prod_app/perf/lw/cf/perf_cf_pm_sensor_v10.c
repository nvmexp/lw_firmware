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
 * @file    perf_cf_pm_sensor_v10.c
 * @copydoc perf_cf_pm_sensor_v10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor_v10.h"
#include "pmu_objpmu.h"
#include "perf/cf/perf_cf.h"
#include "config/g_perf_cf_private.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static LwBool   s_perfCfPmSensorSwTriggerSnapCleared_V10(void *pArgs)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorSwTriggerSnapCleared_V10");

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Timeout for HW to snap the BA PM signals to a BA PM RAM, on a SW trigger.
 */
#define PM_SENSOR_V10_SW_TRIGGER_SNAP_TIMEOUT_US                            (40U)

/*!
 * @brief Min PM Signal index for GPC.
 *
 * All signals indexes in range [@ref
 * PM_SENSOR_V10_GPC_SIGNAL_IDX_MIN, @ref
 * PM_SENSOR_V10_GPC_SIGNAL_IDX_MAX] are GPC signals.
 */
#define PM_SENSOR_V10_GPC_SIGNAL_IDX_MIN                                      0U
/*!
 * @brief Max PM Signal index for GPC.
 *
 * All signals indexes in range [@ref
 * PM_SENSOR_V10_GPC_SIGNAL_IDX_MIN, @ref
 * PM_SENSOR_V10_GPC_SIGNAL_IDX_MAX] are GPC signals.
 */
#define PM_SENSOR_V10_GPC_SIGNAL_IDX_MAX                                    207U

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_PM_SENSOR_V10 *pDescPmSensor =
        (RM_PMU_PERF_CF_PM_SENSOR_V10 *)pBoardObjDesc;
    PERF_CF_PM_SENSOR_V10 *pPmSensorV10;
    PERF_CF_PM_SENSORS_SNAP_ELCG_WAR *pSnapElcgWar = NULL;
    FLCN_STATUS status;

    // Call _SUPER constructor.
    status = perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10,
                 ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10_exit;
    }
    pPmSensorV10 =
        (PERF_CF_PM_SENSOR_V10 *)*ppBoardObj;;

    // Initialise variables here
    (void)pDescPmSensor;

    //
    // Determine if this PERF_CF_PM_SENSOR_V1 needs the ELCG WAR per
    // the set of siganls being sampled.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPmSensorsSnapElcgWarGet(PERF_CF_PM_GET_SENSORS(), &pSnapElcgWar),
        perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        (pSnapElcgWar != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE,
        perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10_exit);
    if (pSnapElcgWar != NULL)
    {
        BOARDOBJGRPMASK_E1024 gpcPmSignalMask;

        // Build mask of GPC PM signals.
        boardObjGrpMaskInit_E1024(&gpcPmSignalMask);
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskBitRangeSet(&gpcPmSignalMask,
                PM_SENSOR_V10_GPC_SIGNAL_IDX_MIN, PM_SENSOR_V10_GPC_SIGNAL_IDX_MAX),
            perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10_exit);

        //
        // Intersect GPC PM signal mask with this sensor's
        // signalSupportedMask.  If intersection is non-zero, then WAR
        // will be enabled.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskAnd(&gpcPmSignalMask, &gpcPmSignalMask,
                                &pPmSensorV10->super.signalsSupportedMask),
            perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10_exit);
        pSnapElcgWar->bEnabled |= !boardObjGrpMaskIsZero(&gpcPmSignalMask);
    }

perfCfPmSensorGrpIfaceModel10ObjSetImpl_V10_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfPmSensorIfaceModel10GetStatus_V10
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_PM_SENSOR_V10_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_PM_SENSOR_V10_GET_STATUS *)pPayload;
    PERF_CF_PM_SENSOR_V10 *pPmSensorV10 = (PERF_CF_PM_SENSOR_V10 *)pBoardObj;
    FLCN_STATUS status;

    status = perfCfPmSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorIfaceModel10GetStatus_V10_exit;
    }

    // Populate V10 status here ....
    (void)pStatus;
    (void)pPmSensorV10;

perfCfPmSensorIfaceModel10GetStatus_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorLoad
 */
FLCN_STATUS
perfCfPmSensorLoad_V10
(
    PERF_CF_PM_SENSOR *pSensor
)
{
    PERF_CF_PM_SENSOR_V10 *pSensorV10 = (PERF_CF_PM_SENSOR_V10 *)pSensor;
    FLCN_STATUS            status     = FLCN_OK;
    LwU32                  regVal;

    status = perfCfPmSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorLoad_V10_exit;
    }

    // V00 class implementation.
    (void)pSensorV10;

    //
    // Enable data collection for PM sensor and turn on SW trigger
    // mode.
    //
    // Note: These actions are intentionally done as two separate
    //       writes as HW intermittently hangs if they are done
    //       simultaneously.
    //
    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_SNAP_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_SNAP_CTRL,
                             _BUFRAM_SEL, _SHADOW, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_SNAP_CTRL, regVal);

    // Read current register settings.
    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL);

    //
    // Fix timeout issue during wait for BA SW trigger snap to complete.
    // During GPC-RG we are getting timeout error. Alex suggested this
    // could be due to the fact that when GPC is off, there is no way
    // you can get ACK from gpcpwr for BA handshake.
    //
    // We are using the following CYA to address this bug.
    // BA_TRAINING_CTRL_SKIP_HW_FLUSH[31] : That's a CYA we added in HW to prevent hang like this.
    // BA_TRAINING_CTRL_MIN_FLUSH_PERIOD[30:20] == _4us
    //
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                             _SKIP_HW_FLUSH, _ENABLED, regVal);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                             _MIN_FLUSH_PERIOD, _4US, regVal);

    // Turn on the SW trigger mode.
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                             _SW_TRIGGER_MODE, _ON, regVal);

    // Write the register settings.
    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL, regVal);

    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                             _SW_TRIGGER_SNAP, _TRIGGER, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL, regVal);

perfCfPmSensorLoad_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorSnap
 */
FLCN_STATUS
perfCfPmSensorSnap_V10
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 regVal;

    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_CTRL,
                     _SW_TRIGGER_SNAP, _TRIGGER, regVal);

    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL, regVal);

    //
    // Ensure that the SW triggered BA snap has cleared. This means the
    // BA PM signals have been saved to the BA PM RAM.
    //
    // Consider moving this back to @ref
    // perfCfPmSensorSignalsUpdate_V10() once ELCG WAR isn't required
    // to be implemented within critical section in @ref
    // s_perfCfPmSensorsSnap().
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        OS_PTIMER_SPIN_WAIT_US_COND(s_perfCfPmSensorSwTriggerSnapCleared_V10, NULL,
                                    PM_SENSOR_V10_SW_TRIGGER_SNAP_TIMEOUT_US) ? FLCN_OK : FLCN_ERR_TIMEOUT,
        perfCfPmSensorSnap_V10_exit);

perfCfPmSensorSnap_V10_exit:
    return FLCN_OK;
}

/*!
 * @copydoc PerfCfPmSensorSignalsUpdate
 */
FLCN_STATUS
perfCfPmSensorSignalsUpdate_V10
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    LwU32         regVal;
    LwU64         data;
    LwBoardObjIdx i;
    LwBoardObjIdx lasti = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
    FLCN_STATUS   status = FLCN_OK;

    //
    // Point the BA PM reading logic to start reading from the first BA PM signal
    // and enable auto-increment.
    //
    regVal = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_SNAP_CTRL);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_SNAP_CTRL,
                             _INDEX, _INIT, regVal);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_SNAP_CTRL,
                             _LOW_RD_INCR, _ENABLE, regVal);
    regVal = FLD_SET_DRF(_CPWR_THERM, _BA_TRAINING_SNAP_CTRL,
                             _SNAP, _TRIGGER, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_SNAP_CTRL, regVal);

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pPmSensor->signalsSupportedMask, i)
    {
        //
        // Since Auto increment is enabled, we have to read DATA_LOW even if
        // the signal is not supported (in order for the index to increment)
        // Hence if there are gaps in signalSupportedMask, we will have to
        // "burn" reads to increment to next index. Since writes might be
        // faster than reading the incrementing field (i.e DATA_LOW) the logic
        // below "burns" the index and increments it to next.
        //
        if (((LwBoardObjIdx)(i - lasti)) != 1)
        {
            regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _BA_TRAINING_SNAP_CTRL,
                             _INDEX, i, regVal);
            REG_WR32(CSB, LW_CPWR_THERM_BA_TRAINING_SNAP_CTRL, regVal);
        }

        //
        // Note: Intentionally add a burn-read to loosen this loop.
        //       BA HW takes ~5 UTILCLK cycles = ~25 PWRCLK to snap the new BA
        //       data into the data registers. A read on RISCV takes ~25 PWRCLK cycles.
        //
        (void)REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_SNAP_CTRL);

        data = (LwU64)REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_DATA_HIGH);
        lw64Lsl(&data, &data, 32);
        lw64Add(&pPmSensor->pSignals[i], &pPmSensor->pSignals[i], &data);
        data = (LwU64)REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_DATA_LOW);
        lw64Add(&pPmSensor->pSignals[i], &pPmSensor->pSignals[i], &data);
        lasti = i;
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief Wait for BA SW trigger snap to complete
 *
 * @param    pArgs   Unused
 *
 * @return   LW_TRUE if SW trigger is cleared, LW_FALSE otherwise.
 */
static LwBool
s_perfCfPmSensorSwTriggerSnapCleared_V10
(
    void *pArgs
)
{
    return FLD_TEST_DRF(_CPWR_THERM, _BA_TRAINING_CTRL, _SW_TRIGGER_SNAP, _CLEARED,
                        REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL));
}
