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
 * @file  pwrchannel_sensor_client_aligned.c
 * @brief PMGR Power Channel Specific to Sensor Client Aligned Power Channel
 * Class specified in Power Topology Table.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel_sensor_client_aligned.h"
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
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_CHANNEL_STATUS_SENSOR_CLIENT_ALIGNED *pStatus =
        (RM_PMU_PMGR_PWR_CHANNEL_STATUS_SENSOR_CLIENT_ALIGNED *)pPayload;
    PWR_CHANNEL_SENSOR_CLIENT_ALIGNED *pSensorClientAligned =
        (PWR_CHANNEL_SENSOR_CLIENT_ALIGNED *)pBoardObj;
    PWR_DEVICE  *pDev = PWR_DEVICE_GET(pSensorClientAligned->super.pwrDevIdx);
    LW2080_CTRL_PMGR_PWR_TUPLE_ACC tupleAcc = pStatus->lastTupleAcc;
    LwU64                          last;
    LwU64                          lwrr;
    LwU64                          diff;
    LWU64_TYPE                     result;
    LwU64                          temp;
    LwU64                          timeusResult;
    FLCN_TIMESTAMP                 timensLwrr;
    FLCN_STATUS                    status = FLCN_OK;

    if (pDev == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED_exit;
    }

    //
    // Critical section to ensure that the time and aclwmulators are sampled as
    // close together as possible.
    //
    appTaskCriticalEnter();
    {
        status = pwrDevTupleAccGet(pDev, pSensorClientAligned->super.pwrDevProvIdx,
            &tupleAcc);
        osPTimerTimeNsLwrrentGet(&timensLwrr);
    }
    appTaskCriticalExit();
    if (status == FLCN_ERR_ACC_SEQ_MISMATCH)
    {
        // Return full range power tuple
        status = pwrDevTupleFullRangeGet(pDev,
            pSensorClientAligned->super.pwrDevProvIdx, &pStatus->super.tuple);
        if (status != FLCN_OK)
        {
            goto pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED_exit;
        }
        else
        {
            goto pwrChannelGetStatus_SENSOR_CLIENT_ALIGNED_save_and_exit;
        }
    }
    else if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED_exit;
    }

    // Callwlate time difference since last aclwmulator reading.
    LwU64_ALIGN32_UNPACK(&last, &pStatus->lastTimens);
    temp = (LwU64)1000;
    lw64Sub(&timeusResult, &timensLwrr.data, &last);
    lwU64Div(&timeusResult, &timeusResult, &temp);
    LwU64_ALIGN32_PACK(&pStatus->lastTimens, &timensLwrr.data);

    // Make sure that we don't do a divide by zero.
    if (timeusResult == 0)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED_exit;
    }

    // Callwlate difference since last sensor reading.

    // Callwlating current
    LwU64_ALIGN32_UNPACK(&last, &pStatus->lastTupleAcc.lwrrAccnC);
    LwU64_ALIGN32_UNPACK(&lwrr, &tupleAcc.lwrrAccnC);
    lw64Sub(&diff, &lwrr, &last);

    lwU64Div(&(result.val), &diff, &timeusResult);

    if (LWU64_HI(result))
    {
        pStatus->super.tuple.lwrrmA = LW_U32_MAX;
    }
    else
    {
        pStatus->super.tuple.lwrrmA = LWU64_LO(result);
    }

    // Callwlating voltage
    LwU64_ALIGN32_UNPACK(&last, &pStatus->lastTupleAcc.voltAccmVus);
    LwU64_ALIGN32_UNPACK(&lwrr, &tupleAcc.voltAccmVus);
    lw64Sub(&diff, &lwrr, &last);

    temp = (LwU64)1000;
    lwU64Mul(&diff, &diff, &temp);
    lwU64Div(&(result.val), &diff, &timeusResult);

    if (LWU64_HI(result))
    {
        pStatus->super.tuple.voltuV = LW_U32_MAX;
    }
    else
    {
        pStatus->super.tuple.voltuV = LWU64_LO(result);
    }

    // Callwlating power
    LwU64_ALIGN32_UNPACK(&last, &pStatus->lastTupleAcc.pwrAccnJ);
    LwU64_ALIGN32_UNPACK(&lwrr, &tupleAcc.pwrAccnJ);
    lw64Sub(&diff, &lwrr, &last);

    lwU64Div(&(result.val), &diff, &timeusResult);

    if (LWU64_HI(result))
    {
        pStatus->super.tuple.pwrmW = LW_U32_MAX;
    }
    else
    {
        pStatus->super.tuple.pwrmW = LWU64_LO(result);
    }

    // Callwlating energy
    LwU64_ALIGN32_UNPACK(&(result.val), &tupleAcc.pwrAccnJ);

    //
    // Colwerting pwrAccnJ to energymJ
    // energymJ = pwrAccnJ / 1000000
    //          = pwrAccnJ * (1/1000000)
    //          = pwrAccnJ * scaleFactor
    // scaleFactor = 1/1000000 represented as FXP 8.24
    // pwrAccnJ is 64.0 and multiplication by scaleFactor is roughly equivalent
    // to right shift by 20 bits
    // 64.0  nJ *  8.24 mJ/nJ => 44.24 mJ
    // 44.24 mJ >> 24         => 44.0  mJ
    // shiftBits = 24 to discard the fractional bits after scaling
    //
    pSensorClientAligned->energy.scaleFactor =
        LW_TYPES_U32_TO_UFXP_X_Y_SCALED(8, 24, 1, 1000000);
    pSensorClientAligned->energy.shiftBits   = 24;
    pmgrAclwpdate(&(pSensorClientAligned->energy), &result);

    // Colwert to LwU64_ALIGN32 and save in tuple
    LwU64_ALIGN32_PACK(&pStatus->super.tuple.energymJ,
        PMGR_ACC_DST_GET(&(pSensorClientAligned->energy)));

pwrChannelGetStatus_SENSOR_CLIENT_ALIGNED_save_and_exit:
    // Save the current aclwmulator values
    pStatus->lastTupleAcc = tupleAcc;

    // Call into _SUPER implementation.
    status = pwrChannelGetStatus_SUPER(pBoardObj, pPayload, LW_TRUE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED_exit;
    }

    // Cache the tuple.
    pSensorClientAligned->super.super.channelCachedTuple = pStatus->super.tuple;
    pSensorClientAligned->super.super.bTupleCached = LW_TRUE;

pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
