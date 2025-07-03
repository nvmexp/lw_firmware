/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
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
#include "boardobj/boardobj.h"
#include "boardobj/boardobj_interface.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void* testSetup(void);

/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Type Definitions -------------------------------- */

typedef struct TEST_INTERFACE
{
    BOARDOBJ_INTERFACE super;
} TEST_INTERFACE;

typedef struct TEST_OBJECT
{
    BOARDOBJ        super;
    TEST_INTERFACE  interface;
} TEST_OBJECT;

/* ------------------------ Globals ----------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
/*!
 * Main structure for all CLK_DOMAIN_3X_PRIMARY_VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE TestInterfaceVirtualTable =
{
    LW_OFFSETOF(TEST_OBJECT, interface) // offset
};

/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_BOARDOBJ_INTERFACE,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_BOARDOBJ_INTERFACE, boardObjInterfaceGetBoardObjFromInterface,
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
UT_CASE_RUN(PMU_BOARDOBJ_INTERFACE, boardObjInterfaceGetBoardObjFromInterface)
{
    FLCN_STATUS status;
    TEST_OBJECT object;
    PBOARDOBJ   pBoardobj;

    status = boardObjInterfaceConstruct(NULL,
                                        &object.interface.super,
                                        NULL,
                                        &TestInterfaceVirtualTable);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);


    pBoardobj = boardObjInterfaceGetBoardObjFromInterface(&object.interface.super);
    UT_ASSERT_EQUAL_PTR(pBoardobj, &object.super);

    pBoardobj = boardObjInterfaceGetBoardObjFromInterface(NULL);
    UT_ASSERT_EQUAL_PTR(pBoardobj, NULL);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    return NULL;
}

/* ------------------------- End of File ------------------------------------ */
