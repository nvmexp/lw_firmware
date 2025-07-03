/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobjgrpmask-mock.c
 * @brief   Mock implementations of BOARDOBJGRPMASK public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrpmask.h"
#include "boardobj/boardobjgrpmask-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Configuration data for the mock version of boardObjGrpMaskImport().
 */
BOARD_OBJ_GRP_MASK_IMPORT_MOCK_CONFIG boardObjGrpMaskImport_MOCK_CONFIG;
BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_CONFIG boardObjGrpMaskIsSubset_MOCK_CONFIG;
BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_CONFIG boardObjGrpMaskIsEqual_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the mock configuration data for boardObjGrpMaskImport().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void boardObjGrpMaskImportMockInit()
{
    memset(&boardObjGrpMaskImport_MOCK_CONFIG, 0x00, sizeof(boardObjGrpMaskImport_MOCK_CONFIG));
}

/*!
 * @brief Adds an entry to the boardObjGrpMaskImport_MOCK_CONFIG data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry            The entry (or call number) for the test.
 * @param[in]  status           Value to return from the mock function
 * @param[in]  pExpectedValues  Expected values to compare against when the mock
 *                              function is called. If null, the mock function
 *                              will skip the comparisons.
 */
void
boardObjGrpMaskImportMockAddEntry
(
    LwU8                                           entry,
    FLCN_STATUS                                    status,
    BOARD_OBJ_GRP_MASK_IMPORT_MOCK_EXPECTED_VALUE *pExpectedValues
)
{
    UT_ASSERT_TRUE(entry < BOARD_OBJ_GRP_MASK_IMPORT_MOCK_MAX_ENTRIES);
    boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].status = status;

    if (pExpectedValues != NULL)
    {
        boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }
}

/*!
 * @brief   MOCK implementation of boardObjGrpMaskImport_FUNC
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref boardObjGrpMaskImport_MOCK_CONFIG.
 *
 * @param[in,out]  pMask
 * @param[in]      bitSize
 * @param[in]      pExtMask
 *
 * @return      perfChangeSeqConstruct_SUPER_MOCK_CONFIG.status.
 */
FLCN_STATUS
boardObjGrpMaskImport_FUNC_MOCK
(
    BOARDOBJGRPMASK                *pMask,
    LwBoardObjIdx                   bitSize,
    LW2080_CTRL_BOARDOBJGRP_MASK   *pExtMask
)
{
    LwU8 entry = boardObjGrpMaskImport_MOCK_CONFIG.numCalled;
    boardObjGrpMaskImport_MOCK_CONFIG.numCalled++;

    if (boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_PTR(pMask, boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].expected.pMask);
        UT_ASSERT_EQUAL_PTR(pExtMask, boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].expected.pExtMask);
        UT_ASSERT_EQUAL_UINT8(bitSize, boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].expected.bitSize);
    }

    for (LwU8 i = 0; i < bitSize / 32; i++)
    {
        pMask->pData[i] = pExtMask->pData[i];
    }
    return boardObjGrpMaskImport_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief Initializes the mock configuration data for boardObjGrpMaskIsSubset().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void boardObjGrpMaskIsSubsetMockInit()
{
    memset(&boardObjGrpMaskIsSubset_MOCK_CONFIG, 0x00, sizeof(boardObjGrpMaskIsSubset_MOCK_CONFIG));
}

/*!
 * @brief Adds an entry to the boardObjGrpMaskIsSubset_MOCK_CONFIG data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry            The entry (or call number) for the test.
 * @param[in]  bIsSubset        Value to return from the mock function
 * @param[in]  pExpectedValues  Expected values to compare against when the mock
 *                              function is called. If null, the mock function
 *                              will skip the comparisons.
 */
void
boardObjGrpMaskIsSubsetMockAddEntry
(
    LwU8                                              entry,
    LwBool                                            bIsSubset,
    BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_EXPECTED_VALUE *pExpectedValues
)
{
    UT_ASSERT_TRUE(entry < BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_MAX_ENTRIES);
    boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].bIsSubset = bIsSubset;

    if (pExpectedValues != NULL)
    {
        boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }
}

/*!
 * @brief   MOCK implementation of boardObjGrpMaskIsSubset_FUNC
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref boardObjGrpMaskIsSubset_MOCK_CONFIG.
 *
 * @param[in]  pOp1
 * @param[in]  pOp2
 *
 * @return      boardObjGrpMaskIsSubset_MOCK_CONFIG.bIsSubset.
 */
LwBool
boardObjGrpMaskIsSubset_FUNC_MOCK
(
    BOARDOBJGRPMASK *pOp1,
    BOARDOBJGRPMASK *pOp2
)
{
    LwU8 entry = boardObjGrpMaskIsSubset_MOCK_CONFIG.numCalled;
    boardObjGrpMaskIsSubset_MOCK_CONFIG.numCalled++;

    if (boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_PTR(pOp1, boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].expected.pOp1);
        UT_ASSERT_EQUAL_PTR(pOp2, boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].expected.pOp2);
    }

    return boardObjGrpMaskIsSubset_MOCK_CONFIG.entries[entry].bIsSubset;
}

/*!
 * @brief Initializes the mock configuration data for boardObjGrpMaskIsEqual().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void boardObjGrpMaskIsEqualMockInit()
{
    memset(&boardObjGrpMaskIsEqual_MOCK_CONFIG, 0x00, sizeof(boardObjGrpMaskIsEqual_MOCK_CONFIG));
}

/*!
 * @brief Adds an entry to the boardObjGrpMaskIsEqual_MOCK_CONFIG data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry            The entry (or call number) for the test.
 * @param[in]  bIsEqual         Value to return from the mock function
 * @param[in]  pExpectedValues  Expected values to compare against when the mock
 *                              function is called. If null, the mock function
 *                              will skip the comparisons.
 */
void
boardObjGrpMaskIsEqualMockAddEntry
(
    LwU8                                             entry,
    LwBool                                           bIsEqual,
    BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_EXPECTED_VALUE *pExpectedValues
)
{
    UT_ASSERT_TRUE(entry < BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_MAX_ENTRIES);
    boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].bIsEqual = bIsEqual;

    if (pExpectedValues != NULL)
    {
        boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }
}

/*!
 * @brief   MOCK implementation of boardObjGrpMaskIsEqual_FUNC
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref boardObjGrpMaskIsEqual_MOCK_CONFIG.
 *
 * @param[in]  pOp1
 * @param[in]  pOp2
 *
 * @return      boardObjGrpMaskIsEqual_MOCK_CONFIG.bIsEqual.
 */
LwBool
boardObjGrpMaskIsEqual_FUNC_MOCK
(
    BOARDOBJGRPMASK *pOp1,
    BOARDOBJGRPMASK *pOp2
)
{
    LwU8 entry = boardObjGrpMaskIsEqual_MOCK_CONFIG.numCalled;
    boardObjGrpMaskIsEqual_MOCK_CONFIG.numCalled++;

    if (boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_PTR(pOp1, boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].expected.pOp1);
        UT_ASSERT_EQUAL_PTR(pOp2, boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].expected.pOp2);
    }

    return boardObjGrpMaskIsEqual_MOCK_CONFIG.entries[entry].bIsEqual;
}
