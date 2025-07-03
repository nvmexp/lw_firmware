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
 * @file    pstate-test.c
 * @brief   Unit tests for pstate.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "perf/pstate.h"
#include "perf/35/pstate_35-mock.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobj-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void* testSetup(void);
static FLCN_STATUS boardObjConstruct_STATIC_MOCK(PBOARDOBJGRP pBObjGrp, PBOARDOBJ *ppBoardObj, LwLength size, PRM_PMU_BOARDOBJ pBoardObjDesc);

/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Globals ----------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
static PSTATE mock_pstate = { 0 };

/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_PSTATE,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_PSTATE, perfPstateGrpIfaceModel10ObjSet_SUPER,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_PSTATE, perfPstateClkFreqGet,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/* ------------------------ Test Suite Implementation ----------------------- */
/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_PSTATE, perfPstateGrpIfaceModel10ObjSet_SUPER)
{
    BOARDOBJGRP             localGroup  = { 0 };
    BOARDOBJ               *pBoardobj   = NULL;
    RM_PMU_PERF_PSTATE      rmPmuPstate;
    FLCN_STATUS             status;

    // First pass, boardObjGrpIfaceModel10ObjSet fails and forwards status
    boardObjConstructMockAddEntry(0, FLCN_ERROR);
    status = perfPstateGrpIfaceModel10ObjSet_SUPER_IMPL(&localGroup,
                                            &pBoardobj,
                                            sizeof(PSTATE),
                                            &rmPmuPstate.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);
    UT_ASSERT_EQUAL_PTR(pBoardobj, NULL);


    // Second pass, the dynamic cast fails and forwards status
    boardObjConstruct_StubWithCallback(1, boardObjConstruct_STATIC_MOCK);
    boardObjDynamicCastMockAddEntry(0, NULL);
    pBoardobj = NULL;
    status = perfPstateGrpIfaceModel10ObjSet_SUPER_IMPL(&localGroup,
                                            &pBoardobj,
                                            sizeof(PSTATE),
                                            &rmPmuPstate.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_CAST);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &mock_pstate.super);


    // Third pass, everything works as expected
    boardObjConstruct_StubWithCallback(2, boardObjConstruct_STATIC_MOCK);
    boardObjDynamicCastMockAddEntry(1, (void *)&mock_pstate);
    pBoardobj = NULL;
    status = perfPstateGrpIfaceModel10ObjSet_SUPER_IMPL(&localGroup,
                                            &pBoardobj,
                                            sizeof(PSTATE),
                                            &rmPmuPstate.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &mock_pstate.super);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_PSTATE, perfPstateClkFreqGet)
{
    BOARDOBJ                               *pBoardobj   = NULL;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY     clockEntry;
    FLCN_STATUS                             status;

    // First pass, NULL pPstate argument
    status = perfPstateClkFreqGet_IMPL(NULL, 0, &clockEntry);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    // Second pass, invalid pstate type
    mock_pstate.super.type = LW2080_CTRL_PERF_PSTATE_TYPE_UNKNOWN;
    status = perfPstateClkFreqGet_IMPL(&mock_pstate, 0, &clockEntry);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);

    // Third pass, return status from routed function call
    mock_pstate.super.type = LW2080_CTRL_PERF_PSTATE_TYPE_35;
    perfPstateClkFreqGet35MockAddEntry(0, FLCN_OK);
    status = perfPstateClkFreqGet_IMPL(&mock_pstate, 0, &clockEntry);
    UT_ASSERT_EQUAL_UINT(1, perfPstateClkFreqGet35MockNumCalls());
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    boardObjConstructMockInit();
    boardObjDynamicCastMockInit();
    perfPstateClkFreqGet35MockInit();
    memset(&mock_pstate, 0x00, sizeof(mock_pstate));

    return NULL;
}

static FLCN_STATUS
boardObjConstruct_STATIC_MOCK
(
    PBOARDOBJGRP        pBObjGrp,
    PBOARDOBJ          *ppBoardObj,
    LwLength            size,
    PRM_PMU_BOARDOBJ    pBoardObjDesc
)
{
    if (*ppBoardObj == NULL)
    {
        *ppBoardObj             = &mock_pstate.super;
        (*ppBoardObj)->type     = pBoardObjDesc->type;
        (*ppBoardObj)->grpIdx   = pBoardObjDesc->grpIdx;
        (*ppBoardObj)->class    = pBObjGrp->classId;
    }

    return FLCN_OK;
}

/* ------------------------- End of File ------------------------------------ */
