/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_pm_sensor_clw_mig_v10.c
 * @copydoc perf_cf_pm_sensor_clw_mig_v10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"
#include "hwref/hopper/gh100/dev_perf.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/cf/perf_cf_pm_sensor.h"
#include "pmu_objpmu.h"
#include "perf/cf/perf_cf.h"
#include "config/g_perf_cf_private.h"
#include "perf/cf/perf_cf_pm_sensor_clw_v10.h"

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
perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_MIG_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10 *pDescPmSensor =
        (RM_PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10 *)pBoardObjDesc;
    PERF_CF_PM_SENSOR_CLW_MIG_V10 *pPmSensorClwMigV10;
    FLCN_STATUS status = FLCN_OK;
    LwU16 signalNumber;

    // Call _SUPER constructor.
    status = perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10,
                 ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_MIG_V10_exit;
    }

    pPmSensorClwMigV10 = (PERF_CF_PM_SENSOR_CLW_MIG_V10 *)*ppBoardObj;

    signalNumber = boardObjGrpMaskBitIdxHighest(&pPmSensorClwMigV10->super.signalsSupportedMask);

    /* check max signalNumber */
    PMU_ASSERT_TRUE_OR_GOTO(status, (signalNumber < (PERF_CF_PM_SENSORS_CLW_V10_MIG_COUNTER_ROWS * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW)),
                        FLCN_ERR_OUT_OF_RANGE, perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_MIG_V10_exit);

    // Initialise variables here
    pPmSensorClwMigV10->swizzId = pDescPmSensor->swizzId;
    pPmSensorClwMigV10->migCfg = pDescPmSensor->migCfg;

perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_MIG_V10_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfPmSensorIfaceModel10GetStatus_CLW_MIG_V10
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_PM_SENSOR_CLW_MIG_V10_GET_STATUS *)pPayload;
    PERF_CF_PM_SENSOR_CLW_MIG_V10 *pPmSensor = (PERF_CF_PM_SENSOR_CLW_MIG_V10 *)pBoardObj;
    FLCN_STATUS status;

    status = perfCfPmSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorIfaceModel10GetStatus_CLW_MIG_V10_exit;
    }

    // Populate V10 status here ....
    (void)pStatus;
    (void)pPmSensor;

perfCfPmSensorIfaceModel10GetStatus_CLW_MIG_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorLoad
 */
FLCN_STATUS
perfCfPmSensorLoad_CLW_MIG_V10
(
    PERF_CF_PM_SENSOR *pSensor
)
{
    PERF_CF_PM_SENSOR_CLW_MIG_V10 *pSensorClwMig = (PERF_CF_PM_SENSOR_CLW_MIG_V10 *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfPmSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorLoad_CLW_MIG_V10_exit;
    }

    // class implementation.
    (void)pSensorClwMig;

perfCfPmSensorLoad_CLW_MIG_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorSnap
 */
FLCN_STATUS
perfCfPmSensorSnap_CLW_MIG_V10
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    FLCN_STATUS status = FLCN_OK;
    PERF_CF_PM_SENSOR_CLW_MIG_V10 *pSensorClwMig = (PERF_CF_PM_SENSOR_CLW_MIG_V10 *)pPmSensor;

    /* Setup the Snap Mask of the MIG */
    PMU_ASSERT_OK_OR_GOTO(status, perfCfPmSensorClwV10SnapMaskSet(&pSensorClwMig->migCfg),
        perfCfPmSensorSnap_CLW_MIG_V10_exit);

perfCfPmSensorSnap_CLW_MIG_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorSignalsUpdate
 */
FLCN_STATUS
perfCfPmSensorSignalsUpdate_CLW_MIG_V10
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    FLCN_STATUS status = FLCN_OK;
    PERF_CF_PM_SENSOR_CLW_MIG_V10 *pSensorClwMig = (PERF_CF_PM_SENSOR_CLW_MIG_V10 *)pPmSensor;
    LwU64 *pBuffer = NULL;
    LwU8  gpcIdx;
    LwU16 fbpIdx;
    LwU8  ltspIdx;
    LwU32 maskOffset = 0;
    LwU32 rdRowStart = 0;
    LwU32 rdRowNumber = 0;
    BOARDOBJGRPMASK *pMask = &pPmSensor->signalsSupportedMask.super;
    LwU32 cntIdx = 0;
    LwBool bDevPar = LW_FALSE;
    LwU32 numSignals = 0;

    /* Step 0: Clear the buffer */
    numSignals = boardObjGrpMaskBitIdxHighest_FUNC(pMask) + 1U;
    memset(pPmSensor->pSignals,  0, PERF_CF_PM_SENSORS_CLW_V10_BYTES_PER_COUNTER * numSignals);

    /* Step 1: Read and Aggregate GPC/TPC counters -- all TPs in a GPC are assigned to the same MIG*/
    pBuffer = &pPmSensor->pSignals[0];
    bDevPar = LW_FALSE;
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pSensorClwMig->migCfg.gpcMask)
    {
        rdRowStart = pmSensorClwGpcsInitConfig.gpc[gpcIdx].tpc[0].partitionStartRowIdx;
        rdRowNumber = PERF_CF_PM_SENSORS_CLW_V10_GPC_COUNTER_ROWS;
        maskOffset = 0;
        status = perfCfPmSensorsClwV10OobrcRead(pmSensorClwCounterBuffer, PERF_CF_PM_SENSORS_CLW_V10_COUNTER_BUFFER_SIZE,
                    rdRowStart, rdRowNumber, pMask, maskOffset, bDevPar);
        if (status != FLCN_OK)
        {
            goto perfCfPmSensorSignalsUpdate_CLW_MIG_V10_exit;
        }

        //aggregation
        for (cntIdx = 0; cntIdx < (PERF_CF_PM_SENSORS_CLW_V10_GPC_COUNTER_ROWS * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW); cntIdx++)
        {
            if (boardObjGrpMaskBitGet_SUPER(pMask, maskOffset + cntIdx))
            {
                /* Aggregation */
                lw64Add(&pBuffer[cntIdx], &pBuffer[cntIdx], &pmSensorClwCounterBuffer[cntIdx]);
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* Step 2: Read and Aggregate FBP/LTSP counters */
    pBuffer = &pPmSensor->pSignals[PERF_CF_PM_SENSORS_CLW_V10_GPC_COUNTER_ROWS * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW];

    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pSensorClwMig->migCfg.fbpMask)
    {
        FOR_EACH_INDEX_IN_MASK(8, ltspIdx, pSensorClwMig->migCfg.fbpLtspMask[fbpIdx])
        {
            if (fbpIdx < PERF_CF_PM_SENSORS_CLW_V10_FBP_EXT_START_ID)
            {
                /* FBP0~7, in Partition section */
                rdRowStart = pmSensorClwFbpsInitConfig.fbp[fbpIdx].ltsp[ltspIdx].partitionStartRowIdx;
                bDevPar = LW_FALSE;
            }
            else
            {
                /* FBP ext, in Device section */
                rdRowStart = pmSensorClwFbpsInitConfig.fbp[fbpIdx].ltsp[ltspIdx].deviceStartRowIdx;
                bDevPar = LW_TRUE;
            }

            rdRowNumber = PERF_CF_PM_SENSORS_CLW_V10_COUNTER_ROWS_PER_LTSP;
            maskOffset = PERF_CF_PM_SENSORS_CLW_V10_GPC_COUNTER_ROWS * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW;
            status = perfCfPmSensorsClwV10OobrcRead(pmSensorClwCounterBuffer, PERF_CF_PM_SENSORS_CLW_V10_COUNTER_BUFFER_SIZE,
                        rdRowStart, rdRowNumber, pMask, maskOffset, bDevPar);
            if (status != FLCN_OK)
            {
                goto perfCfPmSensorSignalsUpdate_CLW_MIG_V10_exit;
            }

            //aggregation
            for (cntIdx = 0; cntIdx < (PERF_CF_PM_SENSORS_CLW_V10_COUNTER_ROWS_PER_LTSP * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW); cntIdx++)
            {
                if (boardObjGrpMaskBitGet_SUPER(pMask, maskOffset + cntIdx))
                {
                    /* Aggregation */
                    lw64Add(&pBuffer[cntIdx], &pBuffer[cntIdx], &pmSensorClwCounterBuffer[cntIdx]);
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    FOR_EACH_INDEX_IN_MASK_END;

perfCfPmSensorSignalsUpdate_CLW_MIG_V10_exit:
    return status;
}
