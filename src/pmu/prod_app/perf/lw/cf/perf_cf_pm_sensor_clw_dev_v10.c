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
 * @file    perf_cf_pm_sensor_clw_dev_v10.c
 * @copydoc perf_cf_pm_sensor_clw_dev_v10.h
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
static FLCN_STATUS s_perfCfPmSensorsClwDevV10MaskGet(LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_CFG *pClwMask)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsClwDevV10MaskGet");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * structure variables storing the CLW Masks of DEV.
 */
static LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_CFG pmSensorClwDevMask
    GCC_ATTRIB_SECTION("dmem_perfCfPmSensors", "pmSensorClwDevMask");

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */

FLCN_STATUS
perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_DEV_V10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10 *pDescPmSensor =
        (RM_PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10 *)pBoardObjDesc;
    PERF_CF_PM_SENSOR_CLW_DEV_V10 *pPmSensorClwDevV10;
    FLCN_STATUS status = FLCN_OK;
    LwU16 signalNumber;

    // Call _SUPER constructor.
    status = perfCfPmSensorGrpIfaceModel10ObjSetImpl_SUPER(pModel10,
                 ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_DEV_V10_exit;
    }

    pPmSensorClwDevV10 = (PERF_CF_PM_SENSOR_CLW_DEV_V10 *)*ppBoardObj;

    signalNumber = boardObjGrpMaskBitIdxHighest(&pPmSensorClwDevV10->super.signalsSupportedMask);

    /* check max signalNumber */
    PMU_ASSERT_TRUE_OR_GOTO(status, (signalNumber < (PERF_CF_PM_SENSORS_CLW_V10_DEV_COUNTER_ROWS * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW)),
                        FLCN_ERR_OUT_OF_RANGE, perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_DEV_V10_exit);
    // Initialise variables here
    (void)pDescPmSensor;

perfCfPmSensorGrpIfaceModel10ObjSetImpl_CLW_DEV_V10_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfPmSensorIfaceModel10GetStatus_CLW_DEV_V10
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_PM_SENSOR_CLW_DEV_V10_GET_STATUS *)pPayload;
    PERF_CF_PM_SENSOR_CLW_DEV_V10 *pPmSensor = (PERF_CF_PM_SENSOR_CLW_DEV_V10 *)pBoardObj;
    FLCN_STATUS status;

    status = perfCfPmSensorIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorIfaceModel10GetStatus_CLW_DEV_V10_exit;
    }

    // Populate V10 status here ....
    (void)pStatus;
    (void)pPmSensor;

perfCfPmSensorIfaceModel10GetStatus_CLW_DEV_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorLoad
 */
FLCN_STATUS
perfCfPmSensorLoad_CLW_DEV_V10
(
    PERF_CF_PM_SENSOR *pSensor
)
{
    PERF_CF_PM_SENSOR_CLW_DEV_V10 *pSensorClwDev = (PERF_CF_PM_SENSOR_CLW_DEV_V10 *)pSensor;
    FLCN_STATUS status = FLCN_OK;

    status = perfCfPmSensorLoad_SUPER(pSensor);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorLoad_CLW_DEV_V10_exit;
    }

    /* Retrieve GPC/FBP/SYS enable mask from CLW initilization configuration */
    PMU_ASSERT_OK_OR_GOTO(status, s_perfCfPmSensorsClwDevV10MaskGet(&pmSensorClwDevMask),
        perfCfPmSensorLoad_CLW_DEV_V10_exit);

    // class implementation.
    (void)pSensorClwDev;

perfCfPmSensorLoad_CLW_DEV_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorSnap
 */
FLCN_STATUS
perfCfPmSensorSnap_CLW_DEV_V10
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    FLCN_STATUS status = FLCN_OK;

    PMU_ASSERT_OK_OR_GOTO(status, perfCfPmSensorClwV10SnapMaskSet(&pmSensorClwDevMask),
        perfCfPmSensorSnap_CLW_DEV_V10_exit);

perfCfPmSensorSnap_CLW_DEV_V10_exit:
    return status;
}

/*!
 * @copydoc PerfCfPmSensorSignalsUpdate
 */
FLCN_STATUS
perfCfPmSensorSignalsUpdate_CLW_DEV_V10
(
    PERF_CF_PM_SENSOR *pPmSensor
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU64 *pBuffer = NULL;
    LwU32 bufSize = 0;
    LwU32 maskOffset = 0;
    LwU32 rdRowStart = 0;
    LwU32 rdRowNumber = 0;
    BOARDOBJGRPMASK *pMask = &pPmSensor->signalsSupportedMask.super;
    LwU32 devFbpRowStart = pmSensorClwFbpsInitConfig.fbp[0].ltsp[0].deviceStartRowIdx;
    LwU32 fpbIdx = 0;
    LwU16 fbpLtspIdx = 0;
    LwU16 fbpCntIdx = 0;

    /* Step 1: Device counter rows read: 0 ~ 79 rows */
    pBuffer = &pPmSensor->pSignals[0];
    bufSize = PERF_CF_PM_SENSORS_CLW_V10_DEV_COUNTER_ROWS * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW;
    rdRowStart = 0;
    rdRowNumber = PERF_CF_PM_SENSORS_CLW_V10_DEV_COUNTER_ROWS;
    maskOffset = 0;
    status = perfCfPmSensorsClwV10OobrcRead(pBuffer, bufSize, rdRowStart, rdRowNumber, pMask, maskOffset, LW_TRUE);
    if (status != FLCN_OK)
    {
        goto perfCfPmSensorSignalsUpdate_CLW_DEV_V10_exit;
    }

    /* Step 2: Aggregation FBPs Ext counters:
    * For Hopper, Device OOBRC rows #80 to #128 are used for FBP8 ~ FBP11.
    * Driver needs to aggregate the counters from FBP8 to FBP11 to the device
    * FBP counters.
    * Each time one LTSP section (3 rows) is read and aggregated to FBP in Dev
    */
    pBuffer = &pPmSensor->pSignals[devFbpRowStart * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW]; //destination buffer
    bufSize = PERF_CF_PM_SENSORS_CLW_V10_COUNTER_BUFFER_SIZE;
    rdRowNumber = PERF_CF_PM_SENSORS_CLW_V10_COUNTER_ROWS_PER_LTSP; //read 3 rows each time

    maskOffset = devFbpRowStart * PERF_CF_PM_SENSORS_CLW_V10_COUNTERS_PER_OOBRC_ROW; //The offset Signal Mask of LTSP
    
    for (fbpLtspIdx = 0; fbpLtspIdx < PERF_CF_PM_SENSORS_CLW_V10_FBP_EXT_LTSP_NUM; fbpLtspIdx++)
    {
        fpbIdx = PERF_CF_PM_SENSORS_CLW_V10_FBP_EXT_START_ID + (fbpLtspIdx / LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_LTSP_NUM);
        PMU_ASSERT_TRUE_OR_GOTO(status, (fpbIdx < LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_NUM),
                    FLCN_ERR_OUT_OF_RANGE, perfCfPmSensorSignalsUpdate_CLW_DEV_V10_exit);
        rdRowStart = pmSensorClwFbpsInitConfig.fbp[fpbIdx].ltsp[fbpLtspIdx%LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_FBP_LTSP_NUM].deviceStartRowIdx;

        /* Read one LTSP section */
        status = perfCfPmSensorsClwV10OobrcRead(pmSensorClwCounterBuffer, bufSize, 
                                                rdRowStart, rdRowNumber, pMask, maskOffset, LW_TRUE);
        if (status != FLCN_OK)
        {
            goto perfCfPmSensorSignalsUpdate_CLW_DEV_V10_exit;
        }

        //aggregation
        for (fbpCntIdx = 0; fbpCntIdx < PERF_CF_PM_SENSORS_CLW_V10_FBP_COUNTERS; fbpCntIdx++)
        {
            if (boardObjGrpMaskBitGet_SUPER(pMask, maskOffset + fbpCntIdx))
            {
                /* Check the write boundary */
                PMU_ASSERT_TRUE_OR_GOTO(status, (fbpCntIdx < bufSize),
                    FLCN_ERR_OUT_OF_RANGE, perfCfPmSensorSignalsUpdate_CLW_DEV_V10_exit);

                /* Aggregation */
                lw64Add(&pBuffer[fbpCntIdx], &pBuffer[fbpCntIdx], &pmSensorClwCounterBuffer[fbpCntIdx]);
            }
        }
    }

perfCfPmSensorSignalsUpdate_CLW_DEV_V10_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief Helper function to retrieve GPC/FBP/SYS enable mask
 * from CLW initilization configuration
 *
 * @param[out]  pClwMask  The poiner to the CLW Mask
 *
 * @return FLCN_OK
 *     GPC snap mask is set successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Incorrect pointer parameters (i.e. NULL) provided.
 */
static FLCN_STATUS
s_perfCfPmSensorsClwDevV10MaskGet
(
    LW2080_CTRL_PERF_PERF_CF_PM_SENSOR_CLW_V10_CFG *pClwMask
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8  gpcIdx;
    LwU16 fbpIdx;

    PMU_ASSERT_TRUE_OR_GOTO(status, (pClwMask != NULL), FLCN_ERR_ILWALID_ARGUMENT,
                        s_perfCfPmSensorsClwDevV10MaskGet_exit);

    /* retrieve GPCs mask */
    pClwMask->gpcMask = pmSensorClwGpcsInitConfig.gpcMask;
    FOR_EACH_INDEX_IN_MASK(8, gpcIdx, pmSensorClwGpcsInitConfig.gpcMask)
    {
        pClwMask->gpcTpcMask[gpcIdx] = pmSensorClwGpcsInitConfig.gpc[gpcIdx].tpcMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* retrieve FBPs mask */
    pClwMask->fbpMask = pmSensorClwFbpsInitConfig.fbpMask;
    FOR_EACH_INDEX_IN_MASK(16, fbpIdx, pmSensorClwFbpsInitConfig.fbpMask)
    {
        pClwMask->fbpLtspMask[fbpIdx] = pmSensorClwFbpsInitConfig.fbp[fbpIdx].ltspMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /* retrieve SYSs mask */
    pClwMask->sysMask = pmSensorClwSysInitConfig.sysMask;

s_perfCfPmSensorsClwDevV10MaskGet_exit:
    return status;
}


