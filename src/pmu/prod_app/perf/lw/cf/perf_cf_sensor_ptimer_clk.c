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
 * @file    perf_cf_sensor_ptimer_clk.c
 * @copydoc perf_cf_sensor_ptimer_clk.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_sensor_ptimer_clk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_SENSOR_PTIMER_CLK  *pDescSensor =
        (RM_PMU_PERF_CF_SENSOR_PTIMER_CLK *)pBoardObjDesc;
    PERF_CF_SENSOR_PTIMER_CLK  *pSensorPtimerClk;
    LwBool                      bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS                 status          = FLCN_OK;

    status = perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK_exit;
    }
    pSensorPtimerClk = (PERF_CF_SENSOR_PTIMER_CLK *)*ppBoardObj;

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if (pSensorPtimerClk->clkFreqKHz != pDescSensor->clkFreqKHz)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK_exit;
        }
    }

    // Set member variables.
    pSensorPtimerClk->clkFreqKHz = pDescSensor->clkFreqKHz;

    if (pSensorPtimerClk->clkFreqKHz == 0)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK_exit;
    }

perfCfSensorGrpIfaceModel10ObjSetImpl_PTIMER_CLK_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfSensorIfaceModel10GetStatus_PTIMER_CLK
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_SENSOR_PTIMER_CLK_GET_STATUS   *pStatus =
        (RM_PMU_PERF_CF_SENSOR_PTIMER_CLK_GET_STATUS *)pPayload;
    PERF_CF_SENSOR_PTIMER_CLK                     *pSensorPtimerClk =
        (PERF_CF_SENSOR_PTIMER_CLK *)pBoardObj;
    FLCN_STATUS status      = FLCN_OK;
    LwU64       timestamp;
    LwU64       tmp;
    LwU64       clkFreqKHz  = pSensorPtimerClk->clkFreqKHz;
    LwU64       divKHz2GHz  = 1000000;

    // PTIMER status would provide the timestamp reading.
    status = perfCfSensorIfaceModel10GetStatus_PTIMER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfSensorIfaceModel10GetStatus_PTIMER_CLK_exit;
    }

    LwU64_ALIGN32_UNPACK(&timestamp, &pStatus->super.super.reading);
    lw64Sub(&tmp, &timestamp, &pSensorPtimerClk->last.data);
    pSensorPtimerClk->last.data = timestamp;

    //
    // Timestamp difference is 32 bits (up to 4 seconds).  clkFreqKHz is 32
    // bits. So each step of this sequence should fit in 64 bits.
    //
    lwU64Mul(&tmp, &tmp, &clkFreqKHz);
    lwU64Div(&tmp, &tmp, &divKHz2GHz);
    lw64Add(&pSensorPtimerClk->reading, &pSensorPtimerClk->reading, &tmp);

    LwU64_ALIGN32_PACK(&pStatus->super.super.reading, &pSensorPtimerClk->reading);

perfCfSensorIfaceModel10GetStatus_PTIMER_CLK_exit:
    return status;
}

/*!
 * @copydoc PerfCfSensorLoad
 */
FLCN_STATUS
perfCfSensorLoad_PTIMER_CLK
(
    PERF_CF_SENSOR *pSensor
)
{
    PERF_CF_SENSOR_PTIMER_CLK *pSensorPtimerClk = (PERF_CF_SENSOR_PTIMER_CLK *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfSensorLoad_PTIMER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfSensorLoad_PTIMER_CLK_exit;
    }

    // Get latest timestamp.
    osPTimerTimeNsLwrrentGet(&(pSensorPtimerClk->last));

perfCfSensorLoad_PTIMER_CLK_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
