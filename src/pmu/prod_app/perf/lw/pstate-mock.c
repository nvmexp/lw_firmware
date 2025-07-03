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
 * @file    voltrail-mock.c
 * @brief   Mock implementations of voltrail public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/pstate.h"
#include "perf/pstate-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
PERF_PSTATE_CLK_FREQ_GET_MOCK_CONFIG perfPstateClkFreqGet_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Initializes the mock configuration data for perfPstateClkFreqGet().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfPstateClkFreqGetMockInit()
{
    memset(&perfPstateClkFreqGet_MOCK_CONFIG, 0x00, sizeof(perfPstateClkFreqGet_MOCK_CONFIG));
}

/*!
 * @brief Adds an entry to the voltRailGetVoltage_MOCK_CONFIG data.
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
void perfPstateClkFreqGetMockAddEntry
(
    LwU8                                            entry,
    FLCN_STATUS                                     status,
    PERF_PSTATE_CLK_FREQ_GET_MOCK_EXPECTED_VALUE   *pExpectedValues
)
{
    UT_ASSERT_TRUE(entry < PERF_PSTATE_CLK_FREQ_GET_MOCK_MAX_ENTRIES);
    perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].status = status;

    if (pExpectedValues != NULL)
    {
        perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }

}

/*!
 * @brief   MOCK implementation of perfPstateClkFreqGet
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfPstateClkFreqGet_MOCK_CONFIG.
 *
 * @param[in]   pPstate          Value passed in by function under test.
 * @param[in]   clkDomainIdx     Value passed in by function under test.
 * @param[out]  pPstateClkEntry  Input value for the function under test.
 *
 * @return      perfPstateClkFreqGet_MOCK_CONFIG.status.
 */
FLCN_STATUS
perfPstateClkFreqGet_MOCK
(
    PSTATE                                 *pPstate,
    LwU8                                    clkDomainIdx,
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY    *pPstateClkEntry
)
{
    LwU8 entry = perfPstateClkFreqGet_MOCK_CONFIG.numCalled;
    perfPstateClkFreqGet_MOCK_CONFIG.numCalled++;

    if (perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_PTR(pPstate, perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].expected.pPstate);
        UT_ASSERT_EQUAL_UINT8(clkDomainIdx, perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].expected.clkDomainIdx);
    }

    return perfPstateClkFreqGet_MOCK_CONFIG.entries[entry].status;
}

/* ------------------------ boardObjGrpIfaceModel10ObjSet() ----------------------------- */
#define CONSTRUCT_MAX_ENTRIES                                               (8U)

typedef struct
{
    LwU8 numCalls;
    struct
    {
        LwBool bUseCallback;
        FLCN_STATUS status;
        BoardObjGrpIfaceModel10ObjSet(*pCallback);
        LwBool bSetup;
    } entries[CONSTRUCT_MAX_ENTRIES];
} CONSTRUCT_MOCK_CONFIG;

static CONSTRUCT_MOCK_CONFIG construct_MOCK_CONFIG = { 0 };

/*!
 * @brief Initializes the mock configuration data for boardObjGrpIfaceModel10ObjSet().
 *
 * Sets the return value to FLCN_ERROR for each entry. It is the responsibility
 * of the test to provide the mock return values prior to running the test.
 */
void perfPstateConstructSuperMockInit()
{
    memset(&construct_MOCK_CONFIG, 0x00, sizeof(construct_MOCK_CONFIG));
    for (LwU8 i = 0; i < CONSTRUCT_MAX_ENTRIES; ++i)
    {
        construct_MOCK_CONFIG.entries[i].bUseCallback   = LW_FALSE;
        construct_MOCK_CONFIG.entries[i].status         = FLCN_ERROR;
        construct_MOCK_CONFIG.entries[i].pCallback      = NULL;
        construct_MOCK_CONFIG.entries[i].bSetup         = LW_FALSE;
    }
}

/*!
 * @brief Adds an entry to the mock data for boardObjGrpIfaceModel10ObjSet().
 *
 * @param[in]  entry    The entry (or call number) for the test.
 * @param[in]  status   Value to return from the mock function.
 */
void perfPstateConstructSuperMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < CONSTRUCT_MAX_ENTRIES);
    construct_MOCK_CONFIG.entries[entry].status = status;
    construct_MOCK_CONFIG.entries[entry].bSetup = LW_TRUE;
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
void perfPstateConstructSuper_StubWithCallback(LwU8 entry, BoardObjGrpIfaceModel10ObjSet(pCallback))
{
    UT_ASSERT_TRUE(entry < CONSTRUCT_MAX_ENTRIES);
    construct_MOCK_CONFIG.entries[entry].bUseCallback   = LW_TRUE;
    construct_MOCK_CONFIG.entries[entry].pCallback      = pCallback;
    construct_MOCK_CONFIG.entries[entry].bSetup         = LW_TRUE;
}

/*!
 * @brief Mock implementation of boardObjGrpIfaceModel10ObjSet().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref construct_MOCK_CONFIG.
 */
FLCN_STATUS
perfPstateGrpIfaceModel10ObjSet_SUPER_MOCK
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

    if (construct_MOCK_CONFIG.entries[entry].bSetup)
    {
        if (construct_MOCK_CONFIG.entries[entry].bUseCallback)
        {
            return construct_MOCK_CONFIG.entries[entry].pCallback(
                pBObjGrp, ppBoardObj, size, pBoardObjDesc);
        }
        else
        {
            return construct_MOCK_CONFIG.entries[entry].status;
        }
    }
    else
    {
        return perfPstateGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    }
}
