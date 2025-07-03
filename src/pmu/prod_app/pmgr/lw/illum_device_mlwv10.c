/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    illum_device_mlwv10.c
 * @copydoc illum_device_mlwv10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmgr/illum_device_mlwv10.h"
#include "pmu_objpmgr.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#define MLWV10_WRITE8(pI2cDev, reg, pData)                                     \
    i2cDevWriteReg8((pI2cDev), Pmgr.i2cQueue, (reg), (pData))

#define MLWV10_WRITEEXT(pI2cDev, reg, pData, size)                             \
    i2cDevRWIndex((pI2cDev), Pmgr.i2cQueue, (reg), (pData), (size), LW_FALSE)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_ILLUM_DEVICE_MLWV10 *pDescIllumDeviceMLWV10 =
        (RM_PMU_PMGR_ILLUM_DEVICE_MLWV10 *)pBoardObjDesc;
    ILLUM_DEVICE_MLWV10    *pIllumDeviceMLWV10;
    I2C_DEVICE             *pI2cDev;
    FLCN_STATUS             status;

    pI2cDev = I2C_DEVICE_GET(pDescIllumDeviceMLWV10->i2cDevIdx);
    if (pI2cDev == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        goto illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10_exit;
    }

    status = illumDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10_exit;
    }
    pIllumDeviceMLWV10 = (ILLUM_DEVICE_MLWV10 *)*ppBoardObj;

    // Set member variables.
    pIllumDeviceMLWV10->pI2cDev = pI2cDev;

illumDeviceGrpIfaceModel10ObjSetImpl_MLWV10_exit:
    return status;
}

/*!
 * @copydoc IllumDeviceDataSet
 */
FLCN_STATUS
illumDeviceDataSet_MLWV10
(
    ILLUM_DEVICE *pIllumDevice,
    ILLUM_ZONE   *pIllumZone
)
{
    ILLUM_DEVICE_MLWV10 *pIllumDeviceMLWV10 = (ILLUM_DEVICE_MLWV10 *)pIllumDevice;
    FLCN_STATUS          status             = FLCN_OK;

    switch (pIllumZone->ctrlMode)
    {
        case LW2080_CTRL_ILLUM_CTRL_MODE_MANUAL_RGB:
        {
            ILLUM_ZONE_RGB *pIllumZoneRGB = (ILLUM_ZONE_RGB *)pIllumZone;
            LW2080_CTRL_PMGR_ILLUM_ZONE_CTRL_MODE_MANUAL_RGB_PARAMS
                 *pManualRGBParams = &pIllumZoneRGB->ctrlModeParams.manualRGB.rgbParams;
            LwU32 modeVal          = 0;

            LwU8 data[] = { 
                            pManualRGBParams->colorR,
                            pManualRGBParams->colorG,
                            pManualRGBParams->colorB,
                            pManualRGBParams->brightnessPct
                          };

            // Populate control mode.
            modeVal = FLD_SET_DRF(_SMBUS, _ZONE_MODE, _FUNCTION, _MANUAL, modeVal);
            status = MLWV10_WRITE8(pIllumDeviceMLWV10->pI2cDev, LW_SMBUS_ZONE_MODE(pIllumZone->provIdx), modeVal);
            if (status != FLCN_OK) 
            {
                goto illumDeviceDataSet_MLWV10_dump;
            }

            status = MLWV10_WRITEEXT(pIllumDeviceMLWV10->pI2cDev, LW_SMBUS_ZONE_COLOR_A_RED(pIllumZone->provIdx), data, sizeof(data));
            if (status != FLCN_OK) 
            {
                goto illumDeviceDataSet_MLWV10_dump;
            }

            // Finally set the trigger bit for the requested zone.
            status = MLWV10_WRITE8(pIllumDeviceMLWV10->pI2cDev, LW_SMBUS_ZONE_TRIGGER, BIT(pIllumZone->provIdx));
            if (status != FLCN_OK) 
            {
                goto illumDeviceDataSet_MLWV10_dump;
            }

            break;
        }
        case LW2080_CTRL_ILLUM_CTRL_MODE_PIECEWISE_LINEAR_RGB:
        {
            ILLUM_ZONE_RGB *pIllumZoneRGB = (ILLUM_ZONE_RGB *)pIllumZone;
            LW2080_CTRL_PMGR_ILLUM_ZONE_CTRL_MODE_PIECEWISE_LINEAR_RGB
                 *pPcLnrRGBParams = &pIllumZoneRGB->ctrlModeParams.piecewiseLinearRGB;
            LwU32 modeVal         = 0;

            // Populate control mode.
            modeVal = FLD_SET_DRF(_SMBUS, _ZONE_MODE, _FUNCTION, _PIECEWISE_LINEAR, modeVal);
            modeVal = FLD_SET_DRF_NUM(_SMBUS, _ZONE_MODE, _CYCLE_TYPE, pPcLnrRGBParams->cycleType, modeVal);
            LwU8 data[]= {
                            modeVal,
                            pPcLnrRGBParams->grpCount,
                            (LwU8)(pPcLnrRGBParams->riseTimems),
                            (LwU8)(pPcLnrRGBParams->riseTimems >> 8),
                            (LwU8)(pPcLnrRGBParams->fallTimems),
                            (LwU8)(pPcLnrRGBParams->fallTimems >> 8),
                            (LwU8)(pPcLnrRGBParams->ATimems),
                            (LwU8)(pPcLnrRGBParams->ATimems >> 8),
                            (LwU8)(pPcLnrRGBParams->BTimems),
                            (LwU8)(pPcLnrRGBParams->BTimems >> 8),
                            (LwU8)(pPcLnrRGBParams->grpIdleTimems),
                            (LwU8)(pPcLnrRGBParams->grpIdleTimems >> 8),
                            pPcLnrRGBParams->rgbParams[0].colorR,
                            pPcLnrRGBParams->rgbParams[0].colorG,
                            pPcLnrRGBParams->rgbParams[0].colorB,
                            pPcLnrRGBParams->rgbParams[0].brightnessPct,
                            pPcLnrRGBParams->rgbParams[1].colorR,
                            pPcLnrRGBParams->rgbParams[1].colorG,
                            pPcLnrRGBParams->rgbParams[1].colorB,
                            pPcLnrRGBParams->rgbParams[1].brightnessPct,
                            (LwU8)(pPcLnrRGBParams->phaseOffsetms),
                            (LwU8)(pPcLnrRGBParams->phaseOffsetms >> 8),
            };

            status = MLWV10_WRITEEXT(pIllumDeviceMLWV10->pI2cDev, LW_SMBUS_ZONE_MODE(pIllumZone->provIdx), data, sizeof(data));
            if (status != FLCN_OK)
            {
                goto illumDeviceDataSet_MLWV10_dump;
            }

            // Finally set the trigger bit for the requested zone.
            status = MLWV10_WRITE8(pIllumDeviceMLWV10->pI2cDev, LW_SMBUS_ZONE_TRIGGER, BIT(pIllumZone->provIdx));
            if (status != FLCN_OK)
            {
                goto illumDeviceDataSet_MLWV10_dump;
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_STATE;

            // Uncomment below code once PMU_TRUE_BP() is enabled.
            // PMU_TRUE_BP();
            goto illumDeviceDataSet_MLWV10_exit;
        }
    }

illumDeviceDataSet_MLWV10_dump:
    if (status != FLCN_OK)
    {
        //
        // This is temporary code being added to help debug OCA issues.
        // Refer bug https://lwbugs/3248504 for more details.
        //
        // pmgrIllumDebugInfoCapture_HAL(&Pmgr, pIllumZone, status);


        // Uncomment below code once PMU_TRUE_BP() is enabled via bug 200717054.
        // PMU_TRUE_BP();
    }

illumDeviceDataSet_MLWV10_exit:
    return status;
}

/*!
 * @copydoc IllumDeviceSync
 */
FLCN_STATUS
illumDeviceSync_MLWV10
(
    ILLUM_DEVICE             *pIllumDevice,
    RM_PMU_ILLUM_DEVICE_SYNC *pSyncData
)
{
    ILLUM_DEVICE_MLWV10 *pIllumDeviceMLWV10 = (ILLUM_DEVICE_MLWV10 *)pIllumDevice;
    FLCN_STATUS          status             = FLCN_OK;

    LwU8 data[] = { 
                    LW_SMBUS_SYNC_SIGNAL_NUM_BYTES,
                    (LwU8)(pSyncData->timeStampms.lo),
                    (LwU8)((pSyncData->timeStampms.lo) >> 8),
                    (LwU8)((pSyncData->timeStampms.lo) >> 16),
                    (LwU8)((pSyncData->timeStampms.lo) >> 24),
                    (LwU8)(pSyncData->timeStampms.hi),
                    (LwU8)((pSyncData->timeStampms.hi) >> 8),
                    (LwU8)((pSyncData->timeStampms.hi) >> 16),
                    (LwU8)((pSyncData->timeStampms.hi) >> 24)
                  };

    status = MLWV10_WRITEEXT(pIllumDeviceMLWV10->pI2cDev, LW_SMBUS_SYNC_SIGNAL, data, sizeof(data));
    if (status != FLCN_OK)
    {
        // Uncomment below code once PMU_TRUE_BP() is enabled.
        // PMU_TRUE_BP();
        goto illumDeviceSync_MLWV10_exit;
    }

illumDeviceSync_MLWV10_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
