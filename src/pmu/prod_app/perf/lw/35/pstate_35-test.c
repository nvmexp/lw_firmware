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
 * @file    pstate_35-test.c
 * @brief   Unit tests for pstate_35.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "perf/pstate-mock.h"
#include "perf/35/pstate_35.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobj-mock.h"
#include "boardobj/boardobjgrp-mock.h"
#include "perf/changeseq-mock.h" // for calloc mocking

/* ------------------------ Static Function Prototypes ---------------------- */
static void* testSetup(void);
static FLCN_STATUS perfPstateConstructSuper_STATIC_MOCK(PBOARDOBJGRP pBObjGrp, PBOARDOBJ *ppBoardObj, LwLength size, PRM_PMU_BOARDOBJ pBoardObjDesc);

/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Globals ----------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
static PSTATE_35 mock_pstate35 = { 0 };

/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_PSTATE_35,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_PSTATE_35, perfPstateGrpIfaceModel10ObjSet_35,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_PSTATE_35, pstateIfaceModel10GetStatus_35,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_PSTATE_35, perfPstateClkFreqGet_35,
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
UT_CASE_RUN(PMU_PSTATE_35, perfPstateGrpIfaceModel10ObjSet_35)
{
    BOARDOBJGRP     localGroup  = { 0 };
    BOARDOBJ       *pBoardobj   = NULL;
    RM_PMU_BOARDOBJ localRmPmuBoardobj;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY clockEntries[20];
    FLCN_STATUS     status;

    // First pass, return error from perfPstateIfaceModel10Set_3X (perfPstateGrpIfaceModel10ObjSet_SUPER)
    perfPstateConstructSuperMockAddEntry(0, FLCN_ERROR);
    status = perfPstateGrpIfaceModel10ObjSet_35(&localGroup,
                                    &pBoardobj,
                                    sizeof(PSTATE_35),
                                    &localRmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // Second pass, return error in case of failed dynamic cast
    perfPstateConstructSuper_StubWithCallback(1, perfPstateConstructSuper_STATIC_MOCK);
    boardObjDynamicCastMockAddEntry(0, NULL);
    status = perfPstateGrpIfaceModel10ObjSet_35(&localGroup,
                                    &pBoardobj,
                                    sizeof(PSTATE_35),
                                    &localRmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_CAST);

    // Third pass, pPstate35->pClkEntries == NULL, ensure failure to allocate is reported
    perfPstateConstructSuper_StubWithCallback(2, perfPstateConstructSuper_STATIC_MOCK);
    boardObjDynamicCastMockAddEntry(1, (void *)&mock_pstate35);
    callocMockAddEntry(0, NULL, NULL);
    status = perfPstateGrpIfaceModel10ObjSet_35(&localGroup,
                                    &pBoardobj,
                                    sizeof(PSTATE_35),
                                    &localRmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NO_FREE_MEM);

    // Fourth pass, pPstate35->pClkEntries == NULL, ensure this is allocated
    perfPstateConstructSuper_StubWithCallback(3, perfPstateConstructSuper_STATIC_MOCK);
    boardObjDynamicCastMockAddEntry(2, (void *)&mock_pstate35);
    callocMockAddEntry(1, clockEntries, NULL);
    status = perfPstateGrpIfaceModel10ObjSet_35(&localGroup,
                                    &pBoardobj,
                                    sizeof(PSTATE_35),
                                    &localRmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(clockEntries, mock_pstate35.pClkEntries);

    // Fifth pass, pPstate35->pClkEntries is not NULL, ensure this is not re-allocated
    perfPstateConstructSuper_StubWithCallback(4, perfPstateConstructSuper_STATIC_MOCK);
    boardObjDynamicCastMockAddEntry(3, (void *)&mock_pstate35);
    callocMockAddEntry(2, clockEntries + 1, NULL);
    status = perfPstateGrpIfaceModel10ObjSet_35(&localGroup,
                                    &pBoardobj,
                                    sizeof(PSTATE_35),
                                    &localRmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(clockEntries, mock_pstate35.pClkEntries);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_PSTATE_35, pstateIfaceModel10GetStatus_35)
{
    RM_PMU_PERF_PSTATE_35_GET_STATUS    payload;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY clockEntries[20];
    FLCN_STATUS                         status;

    // First pass, test reaction to failed dynamic cast
    boardObjDynamicCastMockAddEntry(0, NULL);
    status = pstateIfaceModel10GetStatus_35(&mock_pstate35.super.super.super,
                                &payload.super.boardObj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_CAST);

    // Second pass, test reaction to failed pstateIfaceModel10GetStatus_SUPER (boardObjIfaceModel10GetStatus)
    boardObjDynamicCastMockAddEntry(1, (void *)&mock_pstate35);
    boardObjQueryMockAddEntry(0, FLCN_ERROR);
    status = pstateIfaceModel10GetStatus_35(&mock_pstate35.super.super.super,
                                &payload.super.boardObj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // Third pass, everything works
    boardObjDynamicCastMockAddEntry(2, (void *)&mock_pstate35);
    boardObjQueryMockAddEntry(1, FLCN_OK);
    mock_pstate35.pClkEntries = clockEntries;
    status = pstateIfaceModel10GetStatus_35(&mock_pstate35.super.super.super,
                                &payload.super.boardObj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_PSTATE_35, perfPstateClkFreqGet_35)
{
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY clockEntries[20];
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY clockEntry;
    FLCN_STATUS                         status;
    BOARDOBJ                            dummyBoardObj;

    // First pass, test reaction to failed dynamic cast
    boardObjDynamicCastMockAddEntry(0, NULL);
    status = perfPstateClkFreqGet_35_IMPL(&mock_pstate35.super.super,
                                          0,
                                          &clockEntry);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_CAST);

    // Second pass, NULL pPstate pointer
    boardObjDynamicCastMockAddEntry(1, (void *)&mock_pstate35);
    status = perfPstateClkFreqGet_35_IMPL(NULL,
                                          0,
                                          &clockEntry);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    // Third pass, NULL pPStateClkEntry pointer
    boardObjDynamicCastMockAddEntry(2, (void *)&mock_pstate35);
    status = perfPstateClkFreqGet_35_IMPL(&mock_pstate35.super.super,
                                          0,
                                          NULL);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    // Fourth pass, invalid clock domain index
    boardObjDynamicCastMockAddEntry(3, (void *)&mock_pstate35);
    boardObjGrpObjGetMockAddEntry(0, NULL); // to control BOARDOBJGRP_IS_VALID
    status = perfPstateClkFreqGet_35_IMPL(&mock_pstate35.super.super,
                                          0,
                                          &clockEntry);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    // Fifth pass, everything works
    boardObjDynamicCastMockAddEntry(4, (void *)&mock_pstate35);
    boardObjGrpObjGetMockAddEntry(1, &dummyBoardObj); // to control BOARDOBJGRP_IS_VALID
    mock_pstate35.pClkEntries = clockEntries;
    clockEntries[0].min.origFreqkHz = 1;
    status = perfPstateClkFreqGet_35_IMPL(&mock_pstate35.super.super,
                                          0,
                                          &clockEntry);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(clockEntry.min.origFreqkHz, 1);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    perfPstateConstructSuperMockInit();
    boardObjDynamicCastMockInit();
    callocMockInit();
    boardObjQueryMockInit();
    boardObjGrpObjGetMockInit();
    memset(&mock_pstate35, 0x00, sizeof(mock_pstate35));

    return NULL;
}

static FLCN_STATUS
perfPstateConstructSuper_STATIC_MOCK
(
    PBOARDOBJGRP        pBObjGrp,
    PBOARDOBJ          *ppBoardObj,
    LwLength            size,
    PRM_PMU_BOARDOBJ    pBoardObjDesc
)
{
    if (*ppBoardObj == NULL)
    {
        *ppBoardObj             = &mock_pstate35.super.super.super;
        (*ppBoardObj)->type     = pBoardObjDesc->type;
        (*ppBoardObj)->grpIdx   = pBoardObjDesc->grpIdx;
        (*ppBoardObj)->class    = pBObjGrp->classId;
    }

    return FLCN_OK;
}


/* ------------------------- End of File ------------------------------------ */
