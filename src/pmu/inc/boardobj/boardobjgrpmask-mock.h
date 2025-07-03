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
 * @file    boardobjgrpmask-mock.h
 * @brief   Mock interface of BOARDOBJGRPMASK public interfaces.
 */

#ifndef BOARDOBJGRPMASK_MOCK_H
#define BOARDOBJGRPMASK_MOCK_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#define BOARD_OBJ_GRP_MASK_IMPORT_MOCK_MAX_ENTRIES                          4
#define BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_MAX_ENTRIES                       4
#define BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_MAX_ENTRIES                        4

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    BOARDOBJGRPMASK                *pMask;
    LwU8                            bitSize;
    LW2080_CTRL_BOARDOBJGRP_MASK   *pExtMask;
} BOARD_OBJ_GRP_MASK_IMPORT_MOCK_EXPECTED_VALUE;

/*!
 * Configuration data for the mock version of boardObjGrpMaskImport_FUNC().
 */
typedef struct
{
    LwU8 numCalled;
    struct
    {
        LwBool                                        bCheckExpectedValues;
        FLCN_STATUS                                   status;
        BOARD_OBJ_GRP_MASK_IMPORT_MOCK_EXPECTED_VALUE expected;
    } entries[BOARD_OBJ_GRP_MASK_IMPORT_MOCK_MAX_ENTRIES];
} BOARD_OBJ_GRP_MASK_IMPORT_MOCK_CONFIG;

/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    BOARDOBJGRPMASK *pOp1;
    BOARDOBJGRPMASK *pOp2;
} BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_EXPECTED_VALUE;

/*!
 * Configuration data for the mock version of boardObjGrpMaskImport_FUNC().
 */
typedef struct
{
    LwU8 numCalled;
    struct
    {
        LwBool                                           bCheckExpectedValues;
        LwBool                                           bIsSubset;
        BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_EXPECTED_VALUE expected;
    } entries[BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_MAX_ENTRIES];
} BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_CONFIG;

/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    BOARDOBJGRPMASK *pOp1;
    BOARDOBJGRPMASK *pOp2;
} BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_EXPECTED_VALUE;

/*!
 * Configuration data for the mock version of boardObjGrpMaskImport_FUNC().
 */
typedef struct
{
    LwU8 numCalled;
    struct
    {
        LwBool                                          bCheckExpectedValues;
        LwBool                                          bIsEqual;
        BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_EXPECTED_VALUE expected;
    } entries[BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_MAX_ENTRIES];
} BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_CONFIG;

/* ------------------------ Global Variables -------------------------------- */
extern BOARD_OBJ_GRP_MASK_IMPORT_MOCK_CONFIG boardObjGrpMaskImport_MOCK_CONFIG;
extern BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_CONFIG boardObjGrpMaskIsSubset_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
void boardObjGrpMaskImportMockInit();
void boardObjGrpMaskImportMockAddEntry(LwU8 entry, FLCN_STATUS status, BOARD_OBJ_GRP_MASK_IMPORT_MOCK_EXPECTED_VALUE *pExpectedValues);

void boardObjGrpMaskIsSubsetMockInit();
void boardObjGrpMaskIsSubsetMockAddEntry(LwU8 entry, LwBool bIsSubset, BOARD_OBJ_GRP_MASK_IS_SUBSET_MOCK_EXPECTED_VALUE *pExpectedValues);

void boardObjGrpMaskIsEqualMockInit();
void boardObjGrpMaskIsEqualMockAddEntry(LwU8 entry, LwBool bIsEqual, BOARD_OBJ_GRP_MASK_IS_EQUAL_MOCK_EXPECTED_VALUE *pExpected);

#endif // BOARDOBJGRPMASK_MOCK_H
