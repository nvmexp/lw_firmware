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
 * @file    pstate-mock.h
 * @brief   Data required for configuring mock PSTATE interfaces.
 */

#ifndef PSTATE_MOCK_H
#define PSTATE_MOCK_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "boardobj/boardobjgrp.h"
#include "perf/pstate.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
#define PERF_PSTATE_CLK_FREQ_GET_MOCK_MAX_ENTRIES                           32

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    PSTATE *pPstate;
    LwU8    clkDomainIdx;
} PERF_PSTATE_CLK_FREQ_GET_MOCK_EXPECTED_VALUE;

typedef struct
{
    LwU8 numCalled;
    struct
    {
        LwBool                                       bCheckExpectedValues;
        FLCN_STATUS                                  status;
        PERF_PSTATE_CLK_FREQ_GET_MOCK_EXPECTED_VALUE expected;
    } entries[PERF_PSTATE_CLK_FREQ_GET_MOCK_MAX_ENTRIES];
} PERF_PSTATE_CLK_FREQ_GET_MOCK_CONFIG;

/* ------------------------ Global Variables -------------------------------- */
extern PERF_PSTATE_CLK_FREQ_GET_MOCK_CONFIG perfPstateClkFreqGet_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
void perfPstateClkFreqGetMockInit();
void perfPstateClkFreqGetMockAddEntry(LwU8 entry, FLCN_STATUS status, PERF_PSTATE_CLK_FREQ_GET_MOCK_EXPECTED_VALUE *pExpectedValues);

void perfPstateConstructSuperMockInit();
void perfPstateConstructSuperMockAddEntry(LwU8 entry, FLCN_STATUS status);
void perfPstateConstructSuper_StubWithCallback(LwU8 entry, BoardObjGrpIfaceModel10ObjSet(pCallback));

#endif // PSTATE_MOCK_H
