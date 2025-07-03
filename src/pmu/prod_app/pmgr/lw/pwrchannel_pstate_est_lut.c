/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrchannel_pstate_est_lut.c
 * @brief  Interface specification for a PWR_CHANNEL_PSTATE_ESTIMATION_LUT.
 *
 * A "Pstate Estimation LUT" Power Channel is a look-up table with
 * a power value associated to each pstate in the table.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel_pstate_est_lut.h"
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
 * Construct a _PSTATE_ESTIMATION_LUT PWR_CHANNEL object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChannelGrpIfaceModel10ObjSetImpl_PSTATE_ESTIMATION_LUT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_BOARDOBJ_SET *pPstateEstLUTDesc =
        (RM_PMU_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHANNEL_PSTATE_ESTIMATION_LUT             *pPstateEstLUT;
    FLCN_STATUS                                    status;
    LwU32                                          i;

    // Construct and initialize parent-class object.
    status = pwrChannelGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pPstateEstLUT = (PWR_CHANNEL_PSTATE_ESTIMATION_LUT *)*ppBoardObj;

    // Sanity check.
     PMU_ASSERT_TRUE_OR_GOTO(status,
         (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT_DYNAMIC) ||
         (!pPstateEstLUT->lutEntry[0].bDynamicLut && !pPstateEstLUT->lutEntry[1].bDynamicLut)),
         FLCN_ERR_ILWALID_STATE,
         pwrChannelGrpIfaceModel10ObjSetImpl_PSTATE_ESTIMATION_LUT_exit);    

    // Set class-specific data.
    for (i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_MAX_ENTRIES; i++)
    {
        pPstateEstLUT->lutEntry[i].pstateIndex =
            pPstateEstLUTDesc->lutEntry[i].pstateIndex;
        pPstateEstLUT->lutEntry[i].bDynamicLut =
            pPstateEstLUTDesc->lutEntry[i].bDynamicLut;
        pPstateEstLUT->lutEntry[i].data        =
            pPstateEstLUTDesc->lutEntry[i].data;
    }

pwrChannelGrpIfaceModel10ObjSetImpl_PSTATE_ESTIMATION_LUT_exit:
    return status;
}

/*!
 * _PSTATE_ESTIMATION_LUT power channel implementation.
 *
 * @copydoc PwrChannelTupleStatusGet
 */
FLCN_STATUS
pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT
(
    PWR_CHANNEL                *pChannel,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS status                               =
        FLCN_OK;
    PWR_CHANNEL_PSTATE_ESTIMATION_LUT *pPstateEstLUT =
        (PWR_CHANNEL_PSTATE_ESTIMATION_LUT *)pChannel;
    PWR_CHRELATIONSHIP_TUPLE_STATUS rel              =
        { 0 };
    LwU32                              lwrrPstateIndex;
    LwU8                               i;

    lwrrPstateIndex = perfDomGrpGetValue(RM_PMU_PERF_DOMAIN_GROUP_PSTATE);

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pTuple != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT_exit);

    // Initialize pTuple to its default values.
    LW2080_CTRL_PMGR_PWR_TUPLE_INIT(pTuple);

    for (i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_PSTATE_ESTIMATION_LUT_MAX_ENTRIES; i++)
    {
        //
        // We start from index 0 of the LUT which corresponds to the lower
        // pstate and then move to the next higher pstate. If the current
        // pstate is lower than or equal to the pstate in this entry use this
        // entry's power value else move to the next entry. We are using pstate
        // index here. Lower pstate index means lower perf.
        //
        if (lwrrPstateIndex <= pPstateEstLUT->lutEntry[i].pstateIndex)
        {
            if (!pwrChannelPstateEstimationLutDynamicEnabled(pPstateEstLUT))
            {
                pTuple->pwrmW = pPstateEstLUT->lutEntry[i].data.staticPowerOffsetmW;
            }
            else
            {
                PWR_CHRELATIONSHIP *pChRel = PWR_CHREL_GET(
                    pPstateEstLUT->lutEntry[i].data.dynChRelIdx);

                // Sanity check.
                PMU_ASSERT_TRUE_OR_GOTO(status,
                    (pChRel != NULL),
                    FLCN_ERR_ILWALID_ARGUMENT,
                    pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT_exit);

                // Populate Channel Relationship Tuple values.
                PMU_ASSERT_OK_OR_GOTO(status,
                    pwrChRelationshipTupleEvaluate(pChRel,
                        BOARDOBJ_GET_GRP_IDX_8BIT(&pPstateEstLUT->super.super), &rel),
                    pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT_exit);

                // Copy Channel Relationship Tuple values into Power Tuple fields.
                PMU_ASSERT_OK_OR_GOTO(status,
                    pwrChRelationshipTupleCopy(pTuple, &rel),
                    pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT_exit);
            }
            break;
        }
    }

    if (!pwrChannelPstateEstimationLutDynamicEnabled(pPstateEstLUT))
    {
        pTuple->voltuV = pChannel->voltFixeduV;

        // Obtain the current value from power and voltage.
        PMU_ASSERT_OK_OR_GOTO(status,
            pmgrComputeLwrrmAFromPowermWAndVoltuV(pTuple->pwrmW, pTuple->voltuV,
                &pTuple->lwrrmA),
            pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT_exit);
    }

pwrChannelTupleStatusGet_PSTATE_ESTIMATION_LUT_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
