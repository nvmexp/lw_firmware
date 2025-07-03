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


/* ------------------------ boardObjGrpIfaceModel10ObjSet() ----------------------------- */
#define CONSTRUCT_MAX_ENTRIES                                               (8U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        LwBool bUseCallback;
        FLCN_STATUS status;
        boardObjConstruct_MOCK_CALLBACK *pCallback;
    } entries[CONSTRUCT_MAX_ENTRIES];
} CONSTRUCT_MOCK_CONFIG;

static CONSTRUCT_MOCK_CONFIG construct_MOCK_CONFIG;

/*!
 * @brief Initializes the mock configuration data for boardObjGrpIfaceModel10ObjSet().
 *
 * Sets the return value to FLCN_ERROR for each entry. It is the responsibility
 * of the test to provide the mock return values prior to running the test.
 */
void boardObjConstructMockInit()
{
    memset(&construct_MOCK_CONFIG, 0x00, sizeof(construct_MOCK_CONFIG));
    for (LwU8 i = 0; i < CONSTRUCT_MAX_ENTRIES; ++i)
    {
        construct_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the mock data for boardObjGrpIfaceModel10ObjSet().
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void boardObjConstructMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < CONSTRUCT_MAX_ENTRIES);
    construct_MOCK_CONFIG.entries[entry].bUseCallback = LW_FALSE;
    construct_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Adds an entry to the mock data for boardObjGrpIfaceModel10ObjSet().
 *
 * This is the approach to use when the test wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param[in]  entry        The entry (or call number) for the test.
 * @param[in]  pCallback    Function to call.
 */
void boardObjConstruct_StubWithCallback(LwU8 entry, boardObjConstruct_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < CONSTRUCT_MAX_ENTRIES);
    construct_MOCK_CONFIG.entries[entry].bUseCallback = LW_TRUE;
    construct_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of boardObjGrpIfaceModel10ObjSet().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref construct_MOCK_CONFIG.
 */
FLCN_STATUS
boardObjGrpIfaceModel10ObjSet_MOCK
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    LwU8 entry = construct_MOCK_CONFIG.numCalls;
    ++construct_MOCK_CONFIG.numCalls;

    if (construct_MOCK_CONFIG.entries[entry].bUseCallback)
    {
        FLCN_STATUS status;
        status = construct_MOCK_CONFIG.entries[entry].pCallback(
            pBObjGrp, ppBoardObj, size, pBoardObjDesc);
        return status;
    }
    else
    {
        return construct_MOCK_CONFIG.entries[entry].status;
    }
}

/* ------------------------ boardObjIfaceModel10GetStatus() --------------------------------- */
#define QUERY_MAX_ENTRIES                                                   (4U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[QUERY_MAX_ENTRIES];
} QUERY_MOCK_CONFIG;

static QUERY_MOCK_CONFIG query_MOCK_CONFIG;

/*!
 * @brief Initializes the mock configuration data for boardObjIfaceModel10GetStatus().
 *
 * Sets the return value to FLCN_ERROR for each entry. It is the responsibility
 * of the test to provide the mock return values prior to running the test.
 */
void boardObjQueryMockInit()
{
    memset(&query_MOCK_CONFIG, 0x00, sizeof(query_MOCK_CONFIG));
    for (LwU8 i = 0; i < QUERY_MAX_ENTRIES; ++i)
    {
        query_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
    }
}

/*!
 * @brief Adds an entry to the mock data for boardObjIfaceModel10GetStatus().
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void boardObjQueryMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < QUERY_MAX_ENTRIES);
    query_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Mock implementation of boardObjIfaceModel10GetStatus().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref query_MOCK_CONFIG.
 */
FLCN_STATUS
boardObjIfaceModel10GetStatus_MOCK
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    LwU8 entry = query_MOCK_CONFIG.numCalls;
    ++query_MOCK_CONFIG.numCalls;
    return query_MOCK_CONFIG.entries[entry].status;
}

/* ------------------------ boardObjDynamicCast() --------------------------- */
#define DYNAMIC_CAST_MAX_ENTRIES                                            (16U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        void       *pReturn;
        LwBool      bSetup;
    } entries[DYNAMIC_CAST_MAX_ENTRIES];
} DYNAMIC_CAST_CONFIG;

static DYNAMIC_CAST_CONFIG dynamic_cast_MOCK_CONFIG = { 0 };

/*!
 * @brief Initializes the mock configuration data for boardObjDynamicCast().
 *
 * Sets the return value to FLCN_ERROR for each entry. It is the responsibility
 * of the test to provide the mock return values prior to running the test.
 */
void boardObjDynamicCastMockInit()
{
    memset(&dynamic_cast_MOCK_CONFIG, 0x00, sizeof(dynamic_cast_MOCK_CONFIG));
    for (LwU8 i = 0; i < DYNAMIC_CAST_MAX_ENTRIES; ++i)
    {
        dynamic_cast_MOCK_CONFIG.entries[i].pReturn = NULL;
        dynamic_cast_MOCK_CONFIG.entries[i].bSetup  = LW_FALSE;
    }
}

/*!
 * @brief Adds an entry to the mock data for boardObjDynamicCast().
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void boardObjDynamicCastMockAddEntry(LwU8 entry, void* pReturn)
{
    UT_ASSERT_TRUE(entry < DYNAMIC_CAST_MAX_ENTRIES);
    dynamic_cast_MOCK_CONFIG.entries[entry].pReturn = pReturn;
    dynamic_cast_MOCK_CONFIG.entries[entry].bSetup  = LW_TRUE;
}

/*!
 * @brief Mock implementation of boardObjDynamicCast().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref dynamic_cast_MOCK_CONFIG.
 */
void *
boardObjDynamicCast_MOCK
(
    BOARDOBJGRP                        *pBoardObjGrp,
    BOARDOBJ                           *pBoardObj,
    LW2080_CTRL_BOARDOBJGRP_CLASS_ID    requestedClass,
    LwU8                                requestedType
)
{
    LwU8 entry = dynamic_cast_MOCK_CONFIG.numCalls;
    ++dynamic_cast_MOCK_CONFIG.numCalls;
    if (dynamic_cast_MOCK_CONFIG.entries[entry].bSetup)
    {
        return dynamic_cast_MOCK_CONFIG.entries[entry].pReturn;
    }
    else
    {
        return boardObjDynamicCast_IMPL(pBoardObjGrp,
                                        pBoardObj,
                                        requestedClass,
                                        requestedType);
    }
}
