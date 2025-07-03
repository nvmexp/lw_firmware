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
 * @file    perfrmcmd-test.c
 * @brief   Unit tests for perfrmcmd.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "g_pmurpc.h"
#include "boardobj/boardobjgrp-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void* testSetup(void);

/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Globals ----------------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_PERFRMCMD,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_PERFRMCMD, Test1,
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
UT_CASE_RUN(PMU_PERFRMCMD, Test1)
{
    FLCN_STATUS status;
    RM_PMU_RPC_STRUCT_PERF_BOARD_OBJ_GRP_CMD params;

    boardObjGrpCmdHandlerDispatchInit();

    boardObjGrpCmdHandlerDispatchAddEntry(0, FLCN_ERR_MSGBOX_TIMEOUT);
    params.commandId = RM_PMU_BOARDOBJGRP_CMD_SET;
    status = pmuRpcPerfBoardObjGrpCmd(&params); // TODO - revisit and fix this, chips_a is significantly different to CHIPSSAFETY435_35
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_MSGBOX_TIMEOUT);

    boardObjGrpCmdHandlerDispatchAddEntry(1, FLCN_ERR_DMA_NACK);
    params.commandId = RM_PMU_BOARDOBJGRP_CMD_GET_STATUS;
    status = pmuRpcPerfBoardObjGrpCmd(&params);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_DMA_NACK);

    params.commandId = RM_PMU_BOARDOBJGRP_CMD__LAST + 1;
    status = pmuRpcPerfBoardObjGrpCmd(&params);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_INDEX);

    UT_ASSERT_TRUE(LW_TRUE);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    return NULL;
}

/* ------------------------- End of File ------------------------------------ */
