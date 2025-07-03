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
 * @file    boardobj-mock.c
 * @brief   Mock implementations of BOARDOBJ public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobj-mock.h"
#include "boardobj/boardobjgrp_iface_model_10.h"

/* ------------------------ boardObjDynamicCast() --------------------------- */
#define GRP_OBJ_GET_MAX_ENTRIES                                            (64U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        BOARDOBJ   *pReturn;
        LwBool      bSetup;
    } entries[GRP_OBJ_GET_MAX_ENTRIES];
} GRP_OBJ_GET_CONFIG;

static GRP_OBJ_GET_CONFIG grp_obj_get_MOCK_CONFIG = { 0 };

/*!
 * @brief Initializes the mock configuration data for boardObjDynamicCast().
 *
 * Sets the return value to FLCN_ERROR for each entry. It is the responsibility
 * of the test to provide the mock return values prior to running the test.
 */
void boardObjGrpObjGetMockInit()
{
    memset(&grp_obj_get_MOCK_CONFIG, 0x00, sizeof(grp_obj_get_MOCK_CONFIG));
    for (LwU8 i = 0; i < GRP_OBJ_GET_MAX_ENTRIES; ++i)
    {
        grp_obj_get_MOCK_CONFIG.entries[i].pReturn = NULL;
        grp_obj_get_MOCK_CONFIG.entries[i].bSetup  = LW_FALSE;
    }
}

/*!
 * @brief Adds an entry to the mock data for boardObjGrpObjGet().
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void boardObjGrpObjGetMockAddEntry(LwU8 entry, BOARDOBJ *pReturn)
{
    UT_ASSERT_TRUE(entry < GRP_OBJ_GET_MAX_ENTRIES);
    grp_obj_get_MOCK_CONFIG.entries[entry].pReturn = pReturn;
    grp_obj_get_MOCK_CONFIG.entries[entry].bSetup  = LW_TRUE;
}

/*!
 * @brief Mock implementation of boardObjGrpObjGet().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref grp_obj_get_MOCK_CONFIG.
 */
BOARDOBJ *
boardObjGrpObjGet_MOCK
(
    BOARDOBJGRP  *pBoardObjGrp,
    LwBoardObjIdx objIdx
)
{
    LwU8 entry = grp_obj_get_MOCK_CONFIG.numCalls;
    ++grp_obj_get_MOCK_CONFIG.numCalls;

    if (entry >= GRP_OBJ_GET_MAX_ENTRIES)
    {
        return boardObjGrpObjGet_IMPL(pBoardObjGrp, objIdx);
    }

    if (grp_obj_get_MOCK_CONFIG.entries[entry].bSetup)
    {
        return grp_obj_get_MOCK_CONFIG.entries[entry].pReturn;
    }
    else
    {
        return boardObjGrpObjGet_IMPL(pBoardObjGrp, objIdx);
    }
}


/* ------------------------ boardObjGrpIfaceModel10CmdHandlerDispatch() ----------------- */
#define CMD_HANDLER_DISPATCH_MAX_ENTRIES                                    (4U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        LwBool      bSetup;
    } entries[CMD_HANDLER_DISPATCH_MAX_ENTRIES];
} CMD_HANDLER_DISPATCH_CONFIG;

static CMD_HANDLER_DISPATCH_CONFIG cmd_handler_dispatch_MOCK_CONFIG = { 0 };

/*!
 * @brief Initializes the mock configuration data for boardObjDynamicCast().
 *
 * Sets the return value to FLCN_ERROR for each entry. It is the responsibility
 * of the test to provide the mock return values prior to running the test.
 */
void boardObjGrpCmdHandlerDispatchInit()
{
    memset(&cmd_handler_dispatch_MOCK_CONFIG, 0x00, sizeof(cmd_handler_dispatch_MOCK_CONFIG));
    for (LwU8 i = 0; i < CMD_HANDLER_DISPATCH_MAX_ENTRIES; ++i)
    {
        cmd_handler_dispatch_MOCK_CONFIG.entries[i].status   = FLCN_ERROR;
        cmd_handler_dispatch_MOCK_CONFIG.entries[i].bSetup   = LW_FALSE;
    }
}

/*!
 * @brief Adds an entry to the mock data for boardObjGrpObjGet().
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void boardObjGrpCmdHandlerDispatchAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < CMD_HANDLER_DISPATCH_MAX_ENTRIES);
    cmd_handler_dispatch_MOCK_CONFIG.entries[entry].status   = status;
    cmd_handler_dispatch_MOCK_CONFIG.entries[entry].bSetup   = LW_TRUE;
}

/*!
 * Mock implementation of @ref boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandlerDispatch()
 */
FLCN_STATUS
boardObjGrpIfaceModel10CmdHandlerDispatch_MOCK
(
    LW2080_CTRL_BOARDOBJGRP_CLASS_ID    classId,
    PMU_DMEM_BUFFER                    *pBuffer,
    LwU8                                numEntries,
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY      *pEntries,
    RM_PMU_BOARDOBJGRP_CMD              commandId
)
{
    LwU8 entry = cmd_handler_dispatch_MOCK_CONFIG.numCalls;
    ++cmd_handler_dispatch_MOCK_CONFIG.numCalls;

    if (entry >= CMD_HANDLER_DISPATCH_MAX_ENTRIES)
    {
        return boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(classId,
                                                  pBuffer,
                                                  numEntries,
                                                  pEntries,
                                                  commandId);
    }

    if (cmd_handler_dispatch_MOCK_CONFIG.entries[entry].bSetup)
    {
        return cmd_handler_dispatch_MOCK_CONFIG.entries[entry].status;
    }
    else
    {
        return boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(classId,
                                                  pBuffer,
                                                  numEntries,
                                                  pEntries,
                                                  commandId);
    }
}
