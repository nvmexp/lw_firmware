/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrchannel_sum.c
 * @brief  Interface specification for a PWR_CHANNEL_SUMMATION.
 *
 * A "Summation" Power Channel is a logical construct representing a power
 * channel whose power value is callwlated as the sum of power readings of
 * several other channels.
 *
 * This channel is implemented with first and last indexes into the Power
 * Topology Relationship Table stored in PWR_MONITOR.  All indexes from first to
 * last index (inclusive) are summed to callwlate the power reading for the
 * channel.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel_sum.h"
#include "pmgr/pwrchrel.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _SUMMATION PWR_CHANNEL object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChannelGrpIfaceModel10ObjSetImpl_SUMMATION
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHANNEL_SUMMATION_BOARDOBJ_SET *pSummationDesc =
        (RM_PMU_PMGR_PWR_CHANNEL_SUMMATION_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHANNEL_SUMMATION             *pSummation;
    FLCN_STATUS                        status;

    // Construct and initialize parent-class object.
    status = pwrChannelGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pSummation = (PWR_CHANNEL_SUMMATION *)*ppBoardObj;

    // Set class-specific data.
    pSummation->relIdxFirst = pSummationDesc->relIdxFirst;
    pSummation->relIdxLast  = pSummationDesc->relIdxLast;

    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_1X)
/*!
 * _SUMMATION power channel implementation.
 *
 * @copydoc PwrChannelRefresh
 */
void
pwrChannelRefresh_SUMMATION
(
    PWR_CHANNEL *pChannel
)
{
    PWR_CHANNEL_SUMMATION *pSummation = (PWR_CHANNEL_SUMMATION *)pChannel;
    LwS32 relPwrAvgmW = 0;
    LwU8  i;

    // Iterate over the set of relationships, summing their power.
    for (i = pSummation->relIdxFirst; i <= pSummation->relIdxLast; i++)
    {
        PWR_CHRELATIONSHIP *pRelationship;

        pRelationship = PWR_CHREL_GET(i);
        if (pRelationship == NULL)
        {
            PMU_BREAKPOINT();
            return;
        }

        relPwrAvgmW += pwrChRelationshipEvaluatemW(pRelationship,
                    BOARDOBJ_GET_GRP_IDX_8BIT(&pSummation->super.super));
        DBG_PRINTF_PMGR(("CRSUM: i: %d, r: %d\n", i, relPwrAvgmW, 0, 0));
    }

    //
    // The power channel relationships return a signed value.  They should
    // always sum to >= 0, but ensure here when casting to unsigned.
    //
    pChannel->pwrAvgmW = (LwU32)LW_MAX(0, relPwrAvgmW);

    DBG_PRINTF_PMGR(("CR: pS: %d, c: %d\n", pChannel->pwrAvgmW, 1, 0, 0));
}
#endif

/*!
 * _SUMMATION power channel implementation.
 *
 * @copydoc PwrChannelContains
 */
LwBool
pwrChannelContains_SUMMATION
(
    PWR_CHANNEL *pChannel,
    LwU8         chIdx,
    LwU8         chIdxEval
)
{
    PWR_CHANNEL_SUMMATION *pSummation = (PWR_CHANNEL_SUMMATION *)pChannel;
    LwU8 i;

    //
    // Iterate over the set of relationships, checking their referenced
    // PWR_CHANNELs.
    //
    for (i = pSummation->relIdxFirst; i <= pSummation->relIdxLast; i++)
    {
        PWR_CHRELATIONSHIP *pRelationship;

        pRelationship = PWR_CHREL_GET(i);
        if (pRelationship == NULL)
        {
            PMU_BREAKPOINT();
            break;
        }

        if (pwrChannelContains(
            PWR_CHANNEL_GET(pRelationship->chIdx),
            chIdx, chIdxEval))
        {
            return LW_TRUE;
        }
    }

    return LW_FALSE;
}

/*!
 * _SUMMATION power channel implementation.
 *
 * @copydoc PwrChannelTupleStatusGet
 */
FLCN_STATUS
pwrChannelTupleStatusGet_SUMMATION
(
    PWR_CHANNEL                *pChannel,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_CHANNEL_SUMMATION          *pSummation  =
        (PWR_CHANNEL_SUMMATION *)pChannel;
    PWR_CHRELATIONSHIP_TUPLE_STATUS rel         = { 0 };
    LwS32                           relPwrmW    = 0;
    LwS32                           relLwrrmA   = 0;
    LwU8                            i;
    FLCN_STATUS                     status      = FLCN_OK;

    // Iterate over the set of relationships, summing their tuple values.
    for (i = pSummation->relIdxFirst; i <= pSummation->relIdxLast; i++)
    {
        PWR_CHRELATIONSHIP *pRelationship;

        pRelationship = PWR_CHREL_GET(i);
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pRelationship != NULL),
            FLCN_ERR_ILWALID_INDEX,
            pwrChannelTupleStatusGet_SUMMATION_exit);

        status = pwrChRelationshipTupleEvaluate(pRelationship,
            BOARDOBJ_GET_GRP_IDX_8BIT(&pSummation->super.super), &rel);
        if (status != FLCN_OK)
        {
            goto pwrChannelTupleStatusGet_SUMMATION_exit;
        }

        relPwrmW  += rel.pwrmW;
        relLwrrmA += rel.lwrrmA;
    }

    //
    // The power channel relationships return a signed value. They should
    // always sum to >= 0, but ensure here when casting to unsigned.
    //
    pTuple->pwrmW  = (LwU32)LW_MAX(0, relPwrmW);
    pTuple->lwrrmA = (LwU32)LW_MAX(0, relLwrrmA);

    // Obtain the voltage value from power and current.
    PMU_ASSERT_OK_OR_GOTO(status,
        pmgrComputeVoltuVFromPowermWAndLwrrmA(pTuple->pwrmW, pTuple->lwrrmA,
            &pTuple->voltuV),
        pwrChannelTupleStatusGet_SUMMATION_exit);

pwrChannelTupleStatusGet_SUMMATION_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
