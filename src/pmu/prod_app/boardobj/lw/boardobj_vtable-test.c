/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobj_vtable-test.c
 * @brief   Unit tests for BOARDOBJ_VTABLE.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj-mock.h"
#include "boardobj/boardobj_vtable.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void* testSetup(void);
static FLCN_STATUS boardObjConstruct_STATIC_MOCK(BOARDOBJGRP *pBObjGrp, BOARDOBJ **ppBoardObj, LwLength size, RM_PMU_BOARDOBJ *pBoardObjDesc);

/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Globals ----------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
static BOARDOBJ_VTABLE mock_boardobjVtable = { 0 };

/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_BOARDOBJ_VTABLE,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_BOARDOBJ_VTABLE, boardObjVtableGrpIfaceModel10ObjSet,
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
UT_CASE_RUN(PMU_BOARDOBJ_VTABLE, boardObjVtableGrpIfaceModel10ObjSet)
{
    // TODO-aherring: make sure that this works with the changes to an interface
    BOARDOBJGRP_IFACE_MODEL_10  localModel10  = { 0 };
    BOARDOBJ               *pBoardobj   = NULL;
    RM_PMU_BOARDOBJ         rmPmuBoardobj;
    FLCN_STATUS             status;

    // First pass, test that boardObjGrpIfaceModel10ObjSet failure is bubbled up
    boardObjConstructMockAddEntry(0, FLCN_ERROR);
    status = boardObjVtableGrpIfaceModel10ObjSet(&localModel10,
                                     &pBoardobj,
                                     sizeof(BOARDOBJ_VTABLE),
                                     &rmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);
    UT_ASSERT_EQUAL_PTR(pBoardobj, NULL);

    // Second pass, everything works as expected
    boardObjConstruct_StubWithCallback(1, boardObjConstruct_STATIC_MOCK);
    status = boardObjVtableGrpIfaceModel10ObjSet(&localModel10,
                                     &pBoardobj,
                                     sizeof(BOARDOBJ_VTABLE),
                                     &rmPmuBoardobj);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &mock_boardobjVtable.super);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    boardObjConstructMockInit();
    return NULL;
}

static FLCN_STATUS
boardObjConstruct_STATIC_MOCK
(
    BOARDOBJGRP        *pBObjGrp,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    if (*ppBoardObj == NULL)
    {
        *ppBoardObj             = &mock_boardobjVtable.super;
        (*ppBoardObj)->type     = pBoardObjDesc->type;
        (*ppBoardObj)->grpIdx   = pBoardObjDesc->grpIdx;
        (*ppBoardObj)->class    = pBObjGrp->classId;
    }

    return FLCN_OK;
}

/* ------------------------- End of File ------------------------------------ */
