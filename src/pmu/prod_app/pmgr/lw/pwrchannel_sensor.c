/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_sensor.c
 * @brief PMGR Power Channel Specific to Sensor Power Channel Class specified in
 * Power Topology Table.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel_sensor.h"
#include "pmgr/pwrpolicies.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _SENSOR PWR_CHANNEL object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChannelGrpIfaceModel10ObjSetImpl_SENSOR
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHANNEL_SENSOR_BOARDOBJ_SET *pSensorDesc =
        (RM_PMU_PMGR_PWR_CHANNEL_SENSOR_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHANNEL_SENSOR             *pSensor;
    FLCN_STATUS                     status;

    // Make sure the specified power sensor (PWR_DEVICE) is available.
    if ((Pmgr.pwr.devices.objMask.super.pData[0] & BIT(pSensorDesc->pwrDevIdx)) == 0)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Construct and initialize parent-class object.
    status = pwrChannelGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pSensor = (PWR_CHANNEL_SENSOR *)*ppBoardObj;

    // Set class-specific data.
    pSensor->pwrDevIdx     = pSensorDesc->pwrDevIdx;
    pSensor->pwrDevProvIdx = pSensorDesc->pwrDevProvIdx;

    return status;
}

/* ----------------------- Type-Specific Channel Interfaces ----------------- */
/*!
 * _SENSOR-specific implementation.
 *
 * @copydoc PwrChannelLimitSet
 */
FLCN_STATUS
pwrChannelLimitSet_SENSOR
(
    PWR_CHANNEL    *pChannel,
    LwU8            limitIdx,
    LwU8            limitUnit,
    LwU32           limitValue
)
{
    PWR_CHANNEL_SENSOR *pSensor = (PWR_CHANNEL_SENSOR *)pChannel;
    PWR_DEVICE         *pDev    = PWR_DEVICE_GET(pSensor->pwrDevIdx);
    LwU32               provIdx = pSensor->pwrDevProvIdx;
    FLCN_STATUS         status;

    if (pDev == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrChannelLimitSet_SENSOR_exit;
    }

    status = pwrDevSetLimit(pDev, provIdx, limitIdx, limitUnit, limitValue);

pwrChannelLimitSet_SENSOR_exit:
    return status;
}

/*!
 * _SENSOR-specific implementation.
 *
 * @copydoc PwrChannelSample_IMPL
 */
FLCN_STATUS
pwrChannelSample_SENSOR
(
    PWR_CHANNEL  *pChannel,
    LwU32        *pPowermW
)
{
    PWR_CHANNEL_SENSOR *pSensor = (PWR_CHANNEL_SENSOR *)pChannel;
    PWR_DEVICE         *pDev    = PWR_DEVICE_GET(pSensor->pwrDevIdx);
    FLCN_STATUS         status;

    if (pDev == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrChannelSample_SENSOR_exit;
    }

    status = pwrDevGetPower(pDev, pSensor->pwrDevProvIdx, pPowermW);

pwrChannelSample_SENSOR_exit:
    return status;
}

/*!
 * _SENSOR-specific implementation.
 *
 * @copydoc PwrChannelTupleStatusGet
 */
FLCN_STATUS
pwrChannelTupleStatusGet_SENSOR
(
    PWR_CHANNEL                *pChannel,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_CHANNEL_SENSOR *pSensor = (PWR_CHANNEL_SENSOR *)pChannel;
    PWR_DEVICE         *pDev    = PWR_DEVICE_GET(pSensor->pwrDevIdx);
    FLCN_STATUS         status  = FLCN_ERR_NOT_SUPPORTED;

    if (pDev == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrChannelTupleStatusGet_SENSOR_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_2X))
    {
        status = pwrDevTupleGet(pDev, pSensor->pwrDevProvIdx, pTuple);

        if ((PMUCFG_FEATURE_ENABLED(PMU_PMGR_TAMPER_DETECT_HANDLE_2X)) &&
            (status == FLCN_ERR_PWR_DEVICE_TAMPERED))
        {
            //
            // When tampering has been detected, call
            // pwrPoliciesPwrChannelTamperDetected to capture penalty start time.
            //
            // @see _pwrPoliciesArbitrateAndApply for details on tamper handling
            //
            pwrPoliciesPwrChannelTamperDetected(Pmgr.pPwrPolicies);

            // Reset status to FLCN_OK, since we hace already cached tampered state.
            status = FLCN_OK;
        }
    }

pwrChannelTupleStatusGet_SENSOR_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
