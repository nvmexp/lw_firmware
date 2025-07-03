/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq-test.c
 * @brief   Unit tests for logic in CHANGESEQ.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "pmu_objperf.h"
#include "boardobj/boardobjgrpmask-mock.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "lwos_dma-mock.h"
#include "perf/changeseq.h"
#include "perf/35/changeseq_35.h"
#include "perf/changeseq-mock.h"
#include "perf/pstate-mock.h"
#include "rmpmusupersurfif.h"
#include "volt/voltrail-mock.h"
#include "dmemovl.h"
#include "lwos_dma.h"

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Local Data -------------------------------------- */
/*!
 *
 */
OBJPERF Perf;
OBJVOLT Volt;
const LwU32 _overlay_id_dmem_perfChangeSeqChangeNext = 0;
const LwU32 _overlay_id_dmem_perfChangeSeqClient = 1;

/* ------------------------ Defines and Macros ------------------------------ */
#define CALLOC_MOCK_MAX_ENTRIES                                             4U

/*!
 * Configuration data for the mock version of calloc().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        LwBool                      bCheckExpectedValues;
        void                       *returnPtr;
        CALLOC_MOCK_EXPECTED_VALUE  expected;
    } entries[CALLOC_MOCK_MAX_ENTRIES];
} CALLOC_MOCK_CONFIG;

CALLOC_MOCK_CONFIG calloc_MOCK_CONFIG;

#define LWRTOS_QUEUE_SEND_BLOCKING_MOCK_MAX_ENTRIES                         4U

/*!
 * Configuration data for the mock version of lwrtosQueueSendBlocking().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        LwBool                                      bCheckExpectedValues;
        FLCN_STATUS                                 status;
        LWRTOS_QUEUE_SEND_BLOCKING_EXPECTED_VALUE   expected;
    } entries[LWRTOS_QUEUE_SEND_BLOCKING_MOCK_MAX_ENTRIES];
} LWRTOS_QUEUE_SEND_BLOCKING_MOCK_CONFIG;

LWRTOS_QUEUE_SEND_BLOCKING_MOCK_CONFIG lwrtosQueueSendBlocking_MOCK_CONFIG;

#define OS_GET_PERFORMACE_COUNTER_MOCK_MAX_ENTRIES                          4U

/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    LwU64 *pTimeInNs;
} OS_GET_PERFORMANCE_COUNTER_EXPECTED_VALUE;

/*!
 * Configuration data for the mock version of osGetPerformanceCounter().
 */
typedef struct
{
    LwU8 numCalls;
    struct
    {
        LwBool                                      bCheckExpectedValues;
        LW_STATUS                                   status;
        LwU64                                       timerValue;
        OS_GET_PERFORMANCE_COUNTER_EXPECTED_VALUE   expected;
    } entries[OS_GET_PERFORMACE_COUNTER_MOCK_MAX_ENTRIES];
} OS_GET_PERFORMANCE_COUNTER_MOCK_CONFIG;

OS_GET_PERFORMANCE_COUNTER_MOCK_CONFIG osGetPerformanceCounter_MOCK_CONFIG;

/* ------------------------ Static Functions -------------------------------- */
static void utChangeSeqInit(CHANGE_SEQ *pChangeSeq)
{
    if (pChangeSeq != NULL)
    {
        memset(pChangeSeq, 0x00, sizeof(*pChangeSeq));
    }
    Perf.changeSeqs.pChangeSeq = pChangeSeq;
    pChangeSeq->changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
}

static void utPstatesInit(PSTATES *pPstates)
{
    if (pPstates != NULL)
    {
        memset(pPstates, 0x00, sizeof(*pPstates));
        Perf.pstates = *pPstates;
    }
}

void callocMockInit()
{
    memset(&calloc_MOCK_CONFIG, 0x00, sizeof(calloc_MOCK_CONFIG));
}

void callocMockAddEntry(LwU8 entry, void *returnPtr, const CALLOC_MOCK_EXPECTED_VALUE *pExpectedValues)
{
    UT_ASSERT_TRUE(entry < CALLOC_MOCK_MAX_ENTRIES);
    calloc_MOCK_CONFIG.entries[entry].returnPtr = returnPtr;

    if (pExpectedValues != NULL)
    {
        calloc_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        calloc_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        calloc_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }
}

LwU8 callocMockNumCalls()
{
    return calloc_MOCK_CONFIG.numCalls;
}

void lwrtosQueueSendBlockingMockInit(void)
{
    memset(&lwrtosQueueSendBlocking_MOCK_CONFIG, 0x00, sizeof(lwrtosQueueSendBlocking_MOCK_CONFIG));
}

void lwrtosQueueSendBlockingMockAddEntry(LwU8 entry, FLCN_STATUS status, const LWRTOS_QUEUE_SEND_BLOCKING_EXPECTED_VALUE *pExpectedValues)
{
    UT_ASSERT_TRUE(entry < LWRTOS_QUEUE_SEND_BLOCKING_MOCK_MAX_ENTRIES);
    lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].status = status;

    if (pExpectedValues != NULL)
    {
        lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }
}

LwU8 lwrtosQueueSendBlockingMockNumCalls(void)
{
    return lwrtosQueueSendBlocking_MOCK_CONFIG.numCalls;
}

void osGetPerformanceCounterMockInit(void)
{
    memset(&osGetPerformanceCounter_MOCK_CONFIG, 0x00, sizeof(osGetPerformanceCounter_MOCK_CONFIG));
}

void osGetPerformanceCounterMockAddEntry(LwU8 entry, LW_STATUS status, LwU64 timerValue, const OS_GET_PERFORMANCE_COUNTER_EXPECTED_VALUE *pExpectedValues)
{
    UT_ASSERT_TRUE(entry < OS_GET_PERFORMACE_COUNTER_MOCK_MAX_ENTRIES);
    osGetPerformanceCounter_MOCK_CONFIG.entries[entry].status = status;
    osGetPerformanceCounter_MOCK_CONFIG.entries[entry].timerValue = timerValue;

    if (pExpectedValues != NULL)
    {
        osGetPerformanceCounter_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_TRUE;
        osGetPerformanceCounter_MOCK_CONFIG.entries[entry].expected = *pExpectedValues;
    }
    else
    {
        osGetPerformanceCounter_MOCK_CONFIG.entries[entry].bCheckExpectedValues = LW_FALSE;
    }
}

LwU8 osGetPerformanceCounterMockNumCalls(void)
{
    return osGetPerformanceCounter_MOCK_CONFIG.numCalls;
}

/*!
 *
 */
static void utChangeRequestInit(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChange, LwU32 pstateIdx, LwU32 flags)
{
    memset(pChange, 0x00, sizeof(*pChange));
    pChange->pstateIndex = pstateIdx;
    pChange->flags = flags;
}

/*!
 *
 */
static void* pmuChangeSeqTestCaseSetup(void)
{
    // Object initialization
    memset(&Perf, 0x00, sizeof(Perf));
    memset(&Volt, 0x00, sizeof(Volt));
    memset(&Clk, 0x00, sizeof(Clk));

    // Mock function initialization
    callocMockInit();
    dmaMockInit();
    boardObjGrpMaskImportMockInit();
    boardObjGrpMaskIsSubsetMockInit();
    boardObjGrpMaskIsEqualMockInit();
    perfPstateClkFreqGetMockInit();
    voltRailGetVoltageMockInit();
    lwrtosQueueSendBlockingMockInit();
    osGetPerformanceCounterMockInit();
    cmdmgmtRpcMockInit();
    perfChangeSeqProcessPendingChangeMockInit();

    for (LwU8 i = 0, j = 16; i < 16; ++i, --j)
    {
        UcodeQueueIdToQueueHandleMap[i] = (LwrtosQueueHandle)(j * 10000);
    }

    return NULL;
}

/* ------------------------ Mock functions -----------------------------------*/
/*!
 * @brief   MOCK implementation of lwosCalloc
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref calloc_MOCK_CONFIG.
 *
 * @param[in]      ovlIdx
 * @param[in]      size
 *
 * @return      calloc_MOCK_CONFIG.status.
 */
void *lwosCalloc_MOCK(LwU8 ovlIdx, LwU32 size)
{
    LwU8 entry = calloc_MOCK_CONFIG.numCalls;
    calloc_MOCK_CONFIG.numCalls++;

    if (calloc_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_UINT32(ovlIdx, calloc_MOCK_CONFIG.entries[entry].expected.ovlIdx);
        UT_ASSERT_EQUAL_UINT8(size, calloc_MOCK_CONFIG.entries[entry].expected.size);
    }

    return calloc_MOCK_CONFIG.entries[entry].returnPtr;
}

/*!
 * @brief   MOCK implementation of lwrtosQueueSendBlocking
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref lwrtosQueueSendBlocking_MOCK_CONFIG.
 *
 * @param[in]      queueHandle
 * @param[in]      pItemToQueue
 * @param[in]      size
 *
 * @return      lwrtosQueueSendBlocking_MOCK_CONFIG.status.
 */
FLCN_STATUS lwrtosQueueSendBlocking(LwrtosQueueHandle queueHandle, const void *pItemToQueue, LwU32 size)
{
    LwU8 entry = lwrtosQueueSendBlocking_MOCK_CONFIG.numCalls;
    lwrtosQueueSendBlocking_MOCK_CONFIG.numCalls++;

    if (lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_PTR(queueHandle, lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].expected.queueHandle);
        UT_ASSERT_EQUAL_PTR(pItemToQueue, lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].expected.pItemToQueue);
        UT_ASSERT_EQUAL_UINT(size, lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].expected.size);
    }

    return lwrtosQueueSendBlocking_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief   MOCK implementation of @ref osGetPerformanceCounter().
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref osGetPerformanceCounter_MOCK_CONFIG.
 *
 * @param[in,out]  pTimeInNs
 *
 * @return      osGetPerformanceCounter_MOCK_CONFIG.status.
 */
LW_STATUS osGetPerformanceCounter(LwU64 *pTimeInNs)
{
    LwU8 entry = osGetPerformanceCounter_MOCK_CONFIG.numCalls;
    osGetPerformanceCounter_MOCK_CONFIG.numCalls++;

    if (osGetPerformanceCounter_MOCK_CONFIG.entries[entry].bCheckExpectedValues)
    {
        UT_ASSERT_EQUAL_PTR(pTimeInNs, osGetPerformanceCounter_MOCK_CONFIG.entries[entry].expected.pTimeInNs);
    }

    *pTimeInNs = osGetPerformanceCounter_MOCK_CONFIG.entries[entry].timerValue;
    return osGetPerformanceCounter_MOCK_CONFIG.entries[entry].status;
}

/* ------------------------ Test Suite Declaration--------------------------- */
//
// Since the change sequencer unit tests are pushing up against the maximum
// binary size, we have to disable tests we are not actively working on. The
// tests have been broken up by the function.
//
// To enable tests for the function, define the function set to be tested.
//

//#define PERF_CHANGE_SEQ_CONSTRUCT
//#define PERF_CHANGE_SEQ_INFO_GET
//#define PERF_CHANGE_SEQ_INFO_SET
//#define PERF_CHANGE_SEQ_QUERY
//#define PERF_CHANGE_SEQ_CONSTRUCT_35
//#define PERF_CHANGE_SEQ_INFO_SET_SUPER
//#define PERF_CHANGE_SEQ_LOAD
//#define PERF_CHANGE_SEQ_RPC_SET_CONTROL
//#define PERF_CHANGE_SEQ_LOCK
//#define PERF_CHANGE_SEQ_VALIDATE_CLK_DOMAIN_INPUT_MASK
//#define PERF_CHANGE_SEQ_QUEUE_VOLT_OFFSET
//#define PERF_CHANGE_SEQ_ADJUST_VOLT_OFFSET
//#define PERF_CHANGE_SEQ_QUEUE_CHANGE
//#define PERF_CHANGE_SEQ_SCRIPT_COMPLETION
//#define PERF_CHANGE_SEQ_SEND_COMPLETION_NOTIFICATION
//#define PERF_CHANGE_SEQ_RPC_SEQ_QUERY
//#define PERF_CHANGE_SEQ_CHANGE_QUEUE_ENQUEUE
//#define PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE

/*!
 * @brief The test group for the base change sequencer.
 *
 * @defgroup CHANGESEQ_TEST_SUITE
 */
// UT_SUITE_DEFINE(PMU_CHANGESEQ,
//                 UT_SUITE_SET_COMPONENT("Change Sequencer")
//                 UT_SUITE_SET_DESCRIPTION("TODO")
//                 UT_SUITE_SET_OWNER("Jeff Hutchins")
//                 UT_SUITE_SETUP_HOOK(pmuChangeSeqTestCaseSetup))

/* ------------------ perfChangeSeqConstruct_SUPER_IMPL() ------------------- */
#ifdef PERF_CHANGE_SEQ_CONSTRUCT
/*!
 * @brief Test the case where the system cannot allocate the necessary memory
 * to construct the change sequencer.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ScriptInsertTooManySteps
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Configure @ref lwosCalloc_MOCK() to return NULL.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqConstruct_SUPER_IMPL().
 * @li Expect @ref perfChangeSeqConstruct_SUPER_IMPL() to return
 * @ref FLCN_ERR_NO_FREE_MEM.
 * @li Verify the content of @a ppChangeSeq is NULL.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ConstructSuperCannotAllocateChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Cannot allocate memory for the change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ConstructSuperCannotAllocateChangeSeq)
// {
//     CHANGE_SEQ *pChangeSeq = NULL;
//     CHANGE_SEQ **ppChangeSeq = &pChangeSeq;
//     CALLOC_MOCK_EXPECTED_VALUE callocParams = { 3, sizeof(CHANGE_SEQ) };

//     callocMockAddEntry(0, NULL, &callocParams);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqConstruct_SUPER_IMPL(ppChangeSeq, sizeof(CHANGE_SEQ), 3), FLCN_ERR_NO_FREE_MEM);
//     UT_ASSERT_EQUAL_PTR(*ppChangeSeq, NULL);
//     UT_ASSERT_EQUAL_INT(callocMockNumCalls(), 1);
// }

/*!
 * @brief Test the case where the system is able to allocate the necessary
 * memory to construct the change sequencer.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ConstructSuperAllocationSuccess
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Configure @ref lwosCalloc_MOCK() to return to a pointer to a change
 * sequencer structure on the stack (the PMU/UTF is not able to free items off
 * the heap).
 *
 * Test Exelwtion:
 * @li Expect @ref perfChangeSeqConstruct_SUPER_IMPL() to return @ref FLCN_OK.
 * @li Verify the content of @a ppChangeSeq is equal to the address of the
 * change sequencer structure allocated on the stack.
 * @li Verify the @a version field of the change sequencer structure has been
 * set to LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_UNKNOWN.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ConstructSuperAllocationSuccess,
//                 UT_CASE_SET_DESCRIPTION("Allocate memory and initialize the change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ConstructSuperAllocationSuccess)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     CHANGE_SEQ *pChangeSeq = NULL;
//     CHANGE_SEQ **ppChangeSeq = &pChangeSeq;
//     CALLOC_MOCK_EXPECTED_VALUE callocParams = { 3, sizeof(CHANGE_SEQ) };

//     callocMockAddEntry(0, &changeSeq, &callocParams);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqConstruct_SUPER_IMPL(ppChangeSeq, sizeof(CHANGE_SEQ), 3), FLCN_OK);
//     UT_ASSERT_EQUAL_PTR(*ppChangeSeq, &changeSeq);
//     UT_ASSERT_EQUAL_INT(callocMockNumCalls(), 1);
//     UT_ASSERT_EQUAL_UINT(changeSeq.version, LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_UNKNOWN);
// }

/*!
 * @brief Test the case where the change sequencer has previously allocated.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ConstructSuperAlreadyAllocated
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Construct a change sequencer object.
 *
 * Test Exelwtion:
 * @li Expect @ref perfChangeSeqConstruct_SUPER_IMPL() to return @ref FLCN_OK.
 * @li Verify the content of @a ppChangeSeq is equal to the address of the
 * change sequencer structure allocated on the stack.
 * @li Call @ref perfChangeSeqConstruct_SUPER_IMPL() a second time. Expect
 * @ref FLCN_OK to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ConstructSuperAlreadyAllocated,
//                 UT_CASE_SET_DESCRIPTION("Initialize an already allocated change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ConstructSuperAlreadyAllocated)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     CHANGE_SEQ *pChangeSeq = &changeSeq;
//     CHANGE_SEQ **ppChangeSeq = &pChangeSeq;

//     changeSeq.version = 45;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqConstruct_SUPER_IMPL(ppChangeSeq, sizeof(CHANGE_SEQ), 3), FLCN_OK);
//     UT_ASSERT_EQUAL_PTR(*ppChangeSeq, &changeSeq);
//     UT_ASSERT_EQUAL_INT(callocMockNumCalls(), 0);
//     UT_ASSERT_EQUAL_UINT(changeSeq.version, LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_UNKNOWN);
// }
#endif // PERF_CHANGE_SEQ_CONSTRUCT

/* ------------------------- perfChangeSeqInfoGet() ------------------------- */
#ifdef PERF_CHANGE_SEQ_INFO_GET
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structure is not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoGetNullChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoGet(). Expect @ref FLCN_ERR_NOT_SUPPORTED
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoGetNullChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoGetNullChangeSeq)
// {
//     RM_PMU_PERF_CHANGE_SEQ_INFO_GET info = { 0 };

//     utChangeSeqInit(NULL);
//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoGet(&info), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where an unsupported version of the @ref CHANGESEQ is
 * passed in.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoGetUnknownChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGSEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the version of @ref CHANGESEQ data structure to be unknown.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoGet(). Expect @ref FLCN_ERR_NOT_SUPPORTED
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoGetUnknownChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is of an unknown type.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoGetUnknownChangeSeq)
// {
//     RM_PMU_PERF_CHANGE_SEQ_INFO_GET info = { 0 };
//     CHANGE_SEQ changeSeq = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.version = LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_UNKNOWN;
//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoGet(&info), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where the base class returns an error.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoGetValidChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the version of @ref CHANGSEQ data structure to 35.
 * @li Set the mocked version of @ref perfChangeSeqInfoGet_SUPER() to return
 * @ref FLCN_ERR_ILWALID_ARGUMENT.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoGet(). Expect @ref FLCN_ERR_ILWALID_ARGUMENT
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoGetValidChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is of a known type.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoGetValidChangeSeq)
// {
//     // TODO-JBH: perfChangeSeqInfoGet_35() is a macro wrapper that redirects
//     // to perfChangeSeqInfoGet_SUPER(). We stub out perfChangeSeqInfoGet_SUPER()
//     // instead of creating perfChangeSeqInfoGet_35().
//     RM_PMU_PERF_CHANGE_SEQ_INFO_GET info = { 0 };
//     CHANGE_SEQ changeSeq = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.version = LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35;

//     perfChangeSeqInfoGet_SUPER_MOCK_CONFIG.returlwalue = FLCN_ERR_ILWALID_ARGUMENT;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoGet(&info), FLCN_ERR_ILWALID_ARGUMENT);
// }

#endif // PERF_CHANGE_SEQ_INFO_GET

/* ------------------------- perfChangeSeqInfoSet() ------------------------- */
#ifdef PERF_CHANGE_SEQ_INFO_SET
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structure is not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoSetNullChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoSet(). Expect @ref FLCN_ERR_NOT_SUPPORTED to
 * be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoSetNullChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoSetNullChangeSeq)
// {
//     RM_PMU_PERF_CHANGE_SEQ_INFO_SET info = { 0 };

//     utChangeSeqInit(NULL);
//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoSet(&info), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where an unsupported version of the @ref CHANGESEQ is
 * passed in.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoSetUnknownChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGSEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the version of @ref CHANGESEQ data structure to be unknown.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoSet(). Expect @ref FLCN_ERR_NOT_SUPPORTED
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoSetUnknownChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is of an unknown type.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoSetUnknownChangeSeq)
// {
//     RM_PMU_PERF_CHANGE_SEQ_INFO_SET info = { 0 };
//     CHANGE_SEQ changeSeq = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.version = LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_UNKNOWN;
//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoSet(&info), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where the base class returns an error.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoSetValidChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the version of @ref CHANGSEQ data structure to 35.
 * @li Set the mocked version of @ref perfChangeSeqInfoSet_SUPER() to return
 * @ref FLCN_ERR_ILWALID_INDEX.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoGet(). Expect @ref FLCN_ERR_ILWALID_INDEX
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoSetValidChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is of a known type.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoSetValidChangeSeq)
// {
//     // TODO-JBH: perfChangeSeqInfoSet_35() is a macro wrapper that redirects
//     // to perfChangeSeqInfoSet_SUPER(). We stub out perfChangeSeqInfoSet_SUPER()
//     // instead of creating perfChangeSeqInfoSet_35().
//     RM_PMU_PERF_CHANGE_SEQ_INFO_SET info = { 0 };
//     CHANGE_SEQ changeSeq = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.version = LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35;

//     perfChangeSeqInfoSet_SUPER_MOCK_CONFIG.status = FLCN_ERR_ILWALID_INDEX;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoSet(&info), FLCN_ERR_ILWALID_INDEX);
// }
#endif // PERF_CHANGE_SEQ_INFO_SET

/* -------------------------- perfChangeSeqQuery() -------------------------- */
#ifdef PERF_CHANGE_SEQ_QUERY
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structure is not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueryNullChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQuery(). Expect @ref FLCN_ERR_NOT_SUPPORTED to be
 * returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueryNullChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueryNullChangeSeq)
// {
//     RM_PMU_PERF_CHANGE_SEQ_QUERY query = { 0 };
//     LwU8 buffer[1024];

//     utChangeSeqInit(NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQuery(&query, (void*)buffer), FLCN_ERR_NOT_SUPPORTED);
//     UT_ASSERT_EQUAL_UINT(dmaMockReadNumCalls(), 0);
// }

/*!
 * @brief Test the case where the pointer to the query structure is null.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueryNullQuery
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQuery() with @a pQuery set to NULL. Expect
 * @ref FLCN_ERR_NOT_SUPPORTED to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueryNullQuery,
//                 UT_CASE_SET_DESCRIPTION("Null query argument.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueryNullQuery)
// {
//     CHANGE_SEQ changeSeq;
//     LwU8 buffer[1024];

//     utChangeSeqInit(&changeSeq);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQuery(NULL, (void*)buffer), FLCN_ERR_NOT_SUPPORTED);
//     UT_ASSERT_EQUAL_UINT(dmaMockReadNumCalls(), 0);
// }

/*!
 * @brief Test the case where the pointer to the data buffer is null.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueryNullBuffer
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQuery() with @a pBuf set to NULL. Expect
 * @ref FLCN_ERR_NOT_SUPPORTED to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueryNullBuffer,
//                 UT_CASE_SET_DESCRIPTION("Null buffer argument.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueryNullBuffer)
// {
//     CHANGE_SEQ changeSeq;
//     RM_PMU_PERF_CHANGE_SEQ_QUERY query = { 0 };

//     utChangeSeqInit(&changeSeq);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQuery(&query, NULL), FLCN_ERR_NOT_SUPPORTED);
//     UT_ASSERT_EQUAL_UINT(dmaMockReadNumCalls(), 0);
// }

/*!
 * @brief Test the case where the @ref CHANGESEQ does not contain any steps
 * in the @ref CHANGESEQ_SCRIPT.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueryNoSteps
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Initialize the @ref CHANGESEQ_SCRIPT to have zero steps.
 * @li Configure the mocked versions of @ref dmaRead() and @ref dmaWrite() to
 * return @ref FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQuery(). Expect @ref FLCN_OK to be returned.
 * @li Verify that the mocked versions of both @ref dmaRead() and
 * @ref dmaWrite() have been called twice: once to copy out the @ref CHANGESEQ
 * structure and once to copy out the @ref CHANGESEQ_SCRIPT header.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueryNoSteps,
//                 UT_CASE_SET_DESCRIPTION("Script does not contain any steps.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueryNoSteps)
// {
//     CHANGE_SEQ changeSeq;
//     RM_PMU_PERF_CHANGE_SEQ_QUERY query = { 0 };
//     LwU8 i;
//     LwU8 buffer[1024];
//     RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT scriptLast;
//     RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT scriptQuery;

//     DMA_MOCK_MEMORY_REGION scriptLastRegion =
//     {
//         .pMemDesc = &PmuInitArgs.superSurface,
//         .pData = &scriptLast,
//         .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptLast),
//         .sizeOfData = sizeof(scriptLast),
//     };

//     DMA_MOCK_MEMORY_REGION scriptQueryRegion =
//     {
//         .pMemDesc = &PmuInitArgs.superSurface,
//         .pData = &scriptQuery,
//         .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptQuery),
//         .sizeOfData = sizeof(scriptQuery),
//     };

//     utChangeSeqInit(&changeSeq);

//     memset(&scriptLast, 0x00, sizeof(scriptLast));
//     for (i = 0; i < 2; i++)
//     {
//         dmaMockReadAddEntry(i, FLCN_OK);
//         dmaMockWriteAddEntry(i, FLCN_OK);
//     }
//     dmaMockConfigMemoryRegionInsert(&scriptLastRegion);
//     dmaMockConfigMemoryRegionInsert(&scriptQueryRegion);


//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQuery(&query, buffer), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT(dmaMockReadNumCalls(), 2);
//     UT_ASSERT_EQUAL_UINT(dmaMockWriteNumCalls(), 2);
// }

/*!
 * @brief Test the case where the @ref CHANGESEQ contains multiple steps in the
 * @ref CHANGESEQ_SCRIPT.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueryFourSteps
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Initialize the @ref CHANGESEQ_SCRIPT to have four steps.
 * @li Configure the mocked versions of @ref dmaRead() and @ref dmaWrite() to
 * return @ref FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQuery(). Expect @ref FLCN_OK to be returned.
 * @li Verify that the mocked versions of both @ref dmaRead() and
 * @ref dmaWrite() have been called six times: once to copy out the @ref CHANGESEQ
 * structure, once to copy out the @ref CHANGESEQ_SCRIPT header, and four times
 * to copy out individual steps of the CHANGESEQ_SCRIPT.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueryFourSteps,
//                 UT_CASE_SET_DESCRIPTION("Script does not contain any steps.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueryFourSteps)
// {
//     CHANGE_SEQ changeSeq;
//     RM_PMU_PERF_CHANGE_SEQ_QUERY query = { 0 };
//     LwU8 buffer[1024];
//     RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT scriptLast;
//     RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT scriptQuery;
//     LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED *pHdr = &scriptLast.hdr;
//     LwU8 i;

//     DMA_MOCK_MEMORY_REGION scriptLastRegion =
//     {
//         .pMemDesc = &PmuInitArgs.superSurface,
//         .pData = &scriptLast,
//         .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptLast),
//         .sizeOfData = sizeof(scriptLast),
//     };

//     DMA_MOCK_MEMORY_REGION scriptQueryRegion =
//     {
//         .pMemDesc = &PmuInitArgs.superSurface,
//         .pData = &scriptQuery,
//         .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptQuery),
//         .sizeOfData = sizeof(scriptQuery),
//     };

//     utChangeSeqInit(&changeSeq);

//     memset(&scriptLast, 0x00, sizeof(scriptLast));
//     pHdr->data.numSteps = 4;
//     for (i = 0; i < 6; i++)
//     {
//         dmaMockReadAddEntry(i, FLCN_OK);
//         dmaMockWriteAddEntry(i, FLCN_OK);
//     }
//     dmaMockConfigMemoryRegionInsert(&scriptLastRegion);
//     dmaMockConfigMemoryRegionInsert(&scriptQueryRegion);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQuery(&query, buffer), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT(dmaMockReadNumCalls(), 6);
//     UT_ASSERT_EQUAL_UINT(dmaMockWriteNumCalls(), 6);
// }
#endif // PERF_CHANGE_SEQ_QUERY

/* ----------------------- perfChangeSeqConstruct_35() ---------------------- */
#ifdef PERF_CHANGE_SEQ_CONSTRUCT_35
/*!
 * @brief Test the case where the system cannot construct the base class.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.Construct35CannotConstructSuper
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Configure @ref lwosCalloc_MOCK() to return NULL.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqConstruct_35(). Expect the function to return
 * @ref FLCN_ERR_NO_FREE_MEM.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, Construct35CannotConstructSuper,
//                 UT_CASE_SET_DESCRIPTION("Failed constructing base class.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, Construct35CannotConstructSuper)
// {
//     CHANGE_SEQ_35 changeSeq35;
//     CHANGE_SEQ *pChangeSeq = (CHANGE_SEQ*)&changeSeq35;
//     CHANGE_SEQ **ppChangeSeq = &pChangeSeq;

//     perfChangeSeqConstruct_SUPER_MOCK_CONFIG.status = FLCN_ERR_CSB_PRIV_READ_ERROR;
//     UT_ASSERT_EQUAL_UINT(perfChangeSeqConstruct_35(ppChangeSeq, sizeof(changeSeq35), 4), FLCN_ERR_CSB_PRIV_READ_ERROR);
// }

/*!
 * @brief Test the case where the system cannot allocate the memory for the
 * @ref OBJPERF::changeSeqs::pChangeLast.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.Construct35CannotAllocateLastChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Configure @ref lwosCalloc_MOCK() to return values in the following order:
 * (a) pointer to the @ref CHANGESEQ_35 object allocated on the stack, (b) NULL.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqConstruct_35(). Expect the function to return
 * @ref FLCN_ERR_NO_FREE_MEM.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, Construct35CannotAllocateLastChange,
//                 UT_CASE_SET_DESCRIPTION("Failed to allocate last change request storage.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, Construct35CannotAllocateLastChange)
// {
//     CHANGE_SEQ_35 changeSeq35 = { 0 };
//     CHANGE_SEQ *pChangeSeq = (CHANGE_SEQ*)&changeSeq35;
//     CHANGE_SEQ **ppChangeSeq = &pChangeSeq;
//     CALLOC_MOCK_EXPECTED_VALUE callocParams = { OVL_INDEX_DMEM(perfChangeSeqClient), sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE) };

//     callocMockAddEntry(0, NULL, &callocParams);
//     perfChangeSeqConstruct_SUPER_MOCK_CONFIG.status = FLCN_OK;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqConstruct_35(ppChangeSeq, sizeof(changeSeq35), 3), FLCN_ERR_NO_FREE_MEM);
//     UT_ASSERT_EQUAL_INT(callocMockNumCalls(), 1);
// }

/*!
 * @brief Test the case where the system cannot allocate the memory for the
 * @ref OBJPERF::changeSeq::pChangeNext.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.Construct35CannotAllocateNextChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Configure @ref lwosCalloc_MOCK() to return values in the following order:
 * (a) pointer to the @ref CHANGESEQ_35 object allocated on the stack,
 * (b) pointer to the @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE object allocated
 * on the stack, (c) NULL.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqConstruct_35(). Expect the function to return
 * @ref FLCN_ERR_NO_FREE_MEM.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, Construct35CannotAllocateNextChange,
//                 UT_CASE_SET_DESCRIPTION("Failed to allocate next change request stotage.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, Construct35CannotAllocateNextChange)
// {
//     CHANGE_SEQ_35 changeSeq35 = { 0 };
//     CHANGE_SEQ *pChangeSeq = (CHANGE_SEQ*)&changeSeq35;
//     CHANGE_SEQ **ppChangeSeq = &pChangeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     CALLOC_MOCK_EXPECTED_VALUE callocParams0 = { OVL_INDEX_DMEM(perfChangeSeqClient), sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE) };
//     CALLOC_MOCK_EXPECTED_VALUE callocParams1 = { OVL_INDEX_DMEM(perfChangeSeqChangeNext), sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE) };

//     callocMockAddEntry(0, &lastChange, &callocParams0);
//     callocMockAddEntry(1, NULL, &callocParams1);
//     perfChangeSeqConstruct_SUPER_MOCK_CONFIG.status = FLCN_OK;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqConstruct_35(ppChangeSeq, sizeof(changeSeq35), 3), FLCN_ERR_NO_FREE_MEM);
//     UT_ASSERT_EQUAL_INT(callocMockNumCalls(), 2);
// }

/*!
 * @brief Test the case where the @ref CHANGESEQ_35 was successfully
 * constructed.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.Construct35CannotAllocateNextChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Configure @ref lwosCalloc_MOCK() to return values in the following order:
 * (a) pointer to the @ref CHANGSEQ_35 object allocated on the stack,
 * (b) pointer to the @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE object allocated
 * on the stack, (c) pointer to a second @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE
 * object allocated on the stack.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqConstruct_35(). Expect the function to return
 * @ref FLCN_OK.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, Construct35Success,
//                 UT_CASE_SET_DESCRIPTION("Successful call.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, Construct35Success)
// {
//     CHANGE_SEQ_35 changeSeq35 = { 0 };
//     CHANGE_SEQ *pChangeSeq = (CHANGE_SEQ*)&changeSeq35;
//     CHANGE_SEQ **ppChangeSeq = &pChangeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     CALLOC_MOCK_EXPECTED_VALUE callocParams0 = { OVL_INDEX_DMEM(perfChangeSeqClient), sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE) };
//     CALLOC_MOCK_EXPECTED_VALUE callocParams1 = { OVL_INDEX_DMEM(perfChangeSeqChangeNext), sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE) };

//     callocMockAddEntry(0, &lastChange, &callocParams0);
//     callocMockAddEntry(1, &nextChange, &callocParams1);
//     perfChangeSeqConstruct_SUPER_MOCK_CONFIG.status = FLCN_OK;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqConstruct_35(ppChangeSeq, sizeof(changeSeq35), 3), FLCN_OK);
//     UT_ASSERT_EQUAL_INT(callocMockNumCalls(), 2);
//     UT_ASSERT_EQUAL_UINT(changeSeq35.super.changeSize, sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE));
// }
#endif // PERF_CHANGE_SEQ_CONSTRUCT_35

/* -------------------- perfChangeSeqInfoSet_SUPER_IMPL() ------------------- */
#ifdef PERF_CHANGE_SEQ_INFO_SET_SUPER
/*!
 * @brief Test the case where the version of the @ref CHANGESEQ structure does
 * not match the version of the @ref RM_PMU_PERF_CHANGE_SEQ_INFO_SET structure.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoSetSuperVersionMismatch
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Set the version of the @ref CHANGESEQ structure to 35.
 * @li Set the version of the @ref RM_PMU_PERF_CHANGE_SEQ_INFO_SET structure to
 * 30.
 *
 * Test Exelwtion:
 * @li Call perfChangeSeqInfoSet_SUPER_IMPL(). Expect the function to return
 * @ref FLCN_ERR_ILWALID_VERSION.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoSetSuperVersionMismatch,
//                 UT_CASE_SET_DESCRIPTION("Version mismatch in parameters.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoSetSuperVersionMismatch)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_INFO_SET info = { 0 };

//     changeSeq.version = 35;
//     info.super.version = 30;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoSet_SUPER_IMPL(&changeSeq, &info), FLCN_ERR_ILWALID_VERSION);
// }

/*!
 * @brief Test the case where importing the board object group mask fails.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoSetSuperMaskImportFailed
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Set the version of the @ref CHANGSEQ structure to 35.
 * @li Set the version of the @ref RM_PMU_PERF_CHANGE_SEQ_INFO_SET structure to
 * 35.
 * @li Configure the mocked version of @ref boardObjGrpMaskImport_E32() to
 * return a value other than FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoSet_SUPER_IMPL(). Expect the function to
 * return the value returned by the mocked version of
 * @ref boardObjGrpMaskImport_E32().
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoSetSuperMaskImportFailed,
//                 UT_CASE_SET_DESCRIPTION("Import of LW2080 mask failed.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoSetSuperMaskImportFailed)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_INFO_SET info = { 0 };

//     changeSeq.version = 35;
//     info.super.version = 35;
//     boardObjGrpMaskImportMockAddEntry(0, FLCN_ERR_OUT_OF_RANGE, NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoSet_SUPER_IMPL(&changeSeq, &info), FLCN_ERR_OUT_OF_RANGE);
// }

/*!
 * @brief Test the case where @a pInfo does not lock the @ref CHANGESEQ.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoSetSuperDoNotLock
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Set the version of the @ref CHANGESEQ structure to 35.
 * @li Set the state of the @ref CHANGESEQ structure to
 * LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS.
 * @li Set the version of the @ref RM_PMU_PERF_CHANGE_SEQ_INFO_SET structure to
 * 35.
 * @li Configure the mocked version of @ref boardObjGrpMaskImport_E32() to
 * return @ref FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoSet_SUPER_IMPL(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify the state of the @ref CHANGESEQ structure to be
 * LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoSetSuperDoNotLock,
//                 UT_CASE_SET_DESCRIPTION("Do not lock the change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoSetSuperDoNotLock)
// {
//     const LwU32 expectedMask = 0x446688AA;
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_INFO_SET info = { 0 };

//     changeSeq.version = 35;
//     info.super.version = 35;
//     info.pmu.cpuStepIdMask = expectedMask;
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;
//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoSet_SUPER_IMPL(&changeSeq, &info), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT32(changeSeq.cpuStepIdMask, expectedMask);
//     UT_ASSERT_NOT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.profiling.queue[LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_QUEUE_SIZE - 1].seqId, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID);
// }

//                 UT_CASE_SET_DESCRIPTION("Lock the change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief Test the case where @a pInfo locks the @ref CHANGESEQ.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.InfoSetSuperLock
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Set the version of the @ref CHANGESEQ structure to 35.
 * @li Set the state of the @ref CHANGESEQ structure to
 * LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS.
 * @li Set the version of the @ref RM_PMU_PERF_CHANGE_SEQ_INFO_SET structure to
 * 35.
 * @li Set the lock state of @ref RM_PMU_PERF_CHANGE_SEQ_INFO_SET to true.
 * @li Configure the mocked version of @ref boardObjGrpMaskImport_E32() to
 * return @ref FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqInfoSet_SUPER_IMPL(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify the state of the @ref CHANGESEQ structure to be
 * LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, InfoSetSuperLock,
//                 UT_CASE_SET_DESCRIPTION("Do not lock the change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, InfoSetSuperLock)
// {
//     const LwU32 expectedMask = 0x446688AA;
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_INFO_SET info = { 0 };

//     changeSeq.version = 35;
//     info.super.version = 35;
//     info.pmu.cpuStepIdMask = expectedMask;
//     info.pmu.bLock = LW_TRUE;
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;
//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqInfoSet_SUPER_IMPL(&changeSeq, &info), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT32(changeSeq.cpuStepIdMask, expectedMask);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.profiling.queue[LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_QUEUE_SIZE / 2].seqId, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID);
// }
#endif // PERF_CHANGE_SEQ_INFO_SET_SUPER

/* --------------------------- perfChangeSeqLoad() -------------------------- */
#ifdef PERF_CHANGE_SEQ_LOAD
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structure is not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LoadNullChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLoad(). Expect @ref FLCN_ERR_NOT_SUPPORTED to be
 * returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LoadNullChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LoadNullChangeSeq)
// {
//     utChangeSeqInit(NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLoad(LW_FALSE), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where the change seqquencer is lwrrently processing a
 * change request.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LoadStateInProgress
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the change sequencer state to be
 * LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLoad(). Expect @ref FLCN_ERR_ILWALID_STATE to be
 * returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LoadStateInProgress,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is busy.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LoadStateInProgress)
// {
//     CHANGE_SEQ changeSeq = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLoad(LW_FALSE), FLCN_ERR_ILWALID_STATE);
// }

/*!
 * @brief Test the case to unload the change sequencer.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LoadStateInProgress
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLoad() with @a bLoad set to LW_FALSE. Expect
 * @ref FLCN_OK to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LoadUnload,
//                 UT_CASE_SET_DESCRIPTION("Unload the change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LoadUnload)
// {
//     CHANGE_SEQ changeSeq = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLoad(LW_FALSE), FLCN_OK);
// }

/*!
 * @brief Test the case where an error happened while writing the last change
 * to the frame buffer.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LoadDmaWriteError
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the mocked version of @ref dmaWrite() to return a non-FLCN_OK
 * value.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLoad(). Expect the function to return the value
 * returned by the mocked version of dmaWrite().
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LoadDmaWriteError,
//                 UT_CASE_SET_DESCRIPTION("Error performing DMA write.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LoadDmaWriteError)
// {
//     CHANGE_SEQ changeSeq = { 0 };

//     utChangeSeqInit(&changeSeq);
//     dmaMockWriteAddEntry(0, FLCN_ERR_HPD_UNPLUG);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLoad(LW_TRUE), FLCN_ERR_HPD_UNPLUG);
// }

/*!
 * @brief Test the case where an error happened while obtaining the clock domain
 * list.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LoadErrorObtainingFrequency
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the clock domains to contain 3 domains.
 * @li Configure the mocked version of @ref perfPstateClkFreqGet() to return
 * a value other than FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLoad(). Expect the function to return the value
 * returned by the mocked version of perfPstateClkFreqGet().
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LoadErrorObtainingFrequency,
//                 UT_CASE_SET_DESCRIPTION("Error oclwrred while obtaining frequency of a clock domain.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LoadErrorObtainingFrequency)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     PSTATE pstate = { 0 };
//     CLK_DOMAIN clkDomain = { 0 };
//     BOARDOBJ *clkDomainObjs[32];
//     BOARDOBJ *pstateObjs[32];
//     PERF_PSTATE_CLK_FREQ_GET_MOCK_EXPECTED_VALUE call0 = { &pstate, 3 };

//     utChangeSeqInit(&changeSeq);
//     perfPstateClkFreqGetMockAddEntry(0, FLCN_ERR_LOCK_NOT_AVAILABLE, &call0);

//     memset(&clkDomain, 0x00, sizeof(clkDomain));
//     memset(clkDomainObjs, 0x00, sizeof(clkDomainObjs));

//     pstateObjs[5] = (BOARDOBJ*)&pstate;
//     Perf.pstates.bootPstateIdx = 5;
//     Perf.pstates.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
//     Perf.pstates.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
//     Perf.pstates.super.super.objSlots = 32;
//     Perf.pstates.super.super.ovlIdxDmem = 3;
//     Perf.pstates.super.super.bConstructed = LW_TRUE;
//     Perf.pstates.super.super.ppObjects = pstateObjs;
//     boardObjGrpMaskInit_SUPER(&Perf.pstates.super.objMask,
//                               LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
//     boardObjGrpMaskBitSet(&Perf.pstates.super.objMask, 5);

//     clkDomainObjs[3] = (BOARDOBJ*)&clkDomain;
//     Clk.domains.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
//     Clk.domains.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
//     Clk.domains.super.super.objSlots = 32;
//     Clk.domains.super.super.ovlIdxDmem = 3;
//     Clk.domains.super.super.bConstructed = LW_TRUE;
//     Clk.domains.super.super.ppObjects = clkDomainObjs;
//     boardObjGrpMaskInit_SUPER(&Clk.domains.super.objMask,
//                               LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
//     boardObjGrpMaskBitSet(&Clk.domains.super.objMask, 3);

//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLoad(LW_TRUE), FLCN_ERR_LOCK_NOT_AVAILABLE);
// }

/*!
 * @brief Test the case where an error happened while obtaining the volt rail
 * list.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LoadErrorObtainingVoltage
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the clock domains to contain a volt rail.
 * @li Configure the mocked version of @ref voltRailGetVoltage() to return
 * a value other than FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLoad(). Expect the function to return the value
 * returned by the mocked version of voltRailGetVoltage().
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LoadErrorObtainingVoltage,
//                 UT_CASE_SET_DESCRIPTION("Error oclwrred while obtaining voltage of rails.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LoadErrorObtainingVoltage)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     VOLT_RAIL voltRail;
//     BOARDOBJ *ppBoardObj[32];
//     VOLT_RAIL_GET_VOLTAGE_MOCK_EXPECTED_VALUE call0 = { &voltRail };

//     utChangeSeqInit(&changeSeq);
//     voltRailGetVoltageMockAddEntry(0, FLCN_ERR_DPU_IS_BUSY, &call0);

//     memset(&voltRail, 0x00, sizeof(voltRail));
//     memset(ppBoardObj, 0x00, sizeof(ppBoardObj));

//     ppBoardObj[1] = (BOARDOBJ*)&voltRail;

//     Volt.railMetadata.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
//     Volt.railMetadata.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
//     Volt.railMetadata.super.super.objSlots = 32;
//     Volt.railMetadata.super.super.ovlIdxDmem = 3;
//     Volt.railMetadata.super.super.bConstructed = LW_TRUE;
//     Volt.railMetadata.super.super.ppObjects = ppBoardObj;
//     boardObjGrpMaskInit_SUPER(&Volt.railMetadata.super.objMask,
//                               LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 1);

//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLoad(LW_TRUE), FLCN_ERR_DPU_IS_BUSY);
// }
#endif // PERF_CHANGE_SEQ_LOAD

/* --------------------- pmuRpcPerfChangeSeqSetControl() -------------------- */
#ifdef PERF_CHANGE_SEQ_RPC_SET_CONTROL
FLCN_STATUS pmuRpcPerfChangeSeqSetControl(RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_SET_CONTROL *pParams);

/*!
 * @brief Test the case where we attempt to unlock a @ref CHANGSEQ that is not
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.RPCSetControlNotLocked
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the @ref RM_PMU_PERF_CHANGE_SEQ_SET_CONTROL to unlock the
 * change sequencer.
 *
 * Test Exelwtion:
 * @li Call @ref pmuRpcPerfChangeSeqSetControl(). Expect the function to return
 * @ref FLCN_ERR_ILWALID_STATE.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, RPCSetControlNotLocked,
//                 UT_CASE_SET_DESCRIPTION("Attempting to call RPC while change sequencer is not locked.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, RPCSetControlNotLocked)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_SET_CONTROL control = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;

//     FLCN_STATUS status = pmuRpcPerfChangeSeqSetControl(&control);
//     UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

//     //
//     // For some reason, doing a call/return check does not work on this particular
//     // function. The return value is FLCN_ERR_ILWALID_STATE (0x35), but when it's
//     // put into UT_ASSERT_EQUAL_UINT or UT_ASSERT_EQUAL_UINT8, it gets colwerted
//     // to 0xFF35; thus, it fails the assert. Creating a temporary status doesn't
//     // generate such a problem.
//     //
//     // UT_ASSERT_EQUAL_UINT(pmuRpcPerfChangeSeqSetControl(&control), FLCN_ERR_ILWALID_STATE);

// }
#endif // PERF_CHANGE_SEQ_RPC_SET_CONTROL

/* --------------------------- perfChangeSeqLock() -------------------------- */
#ifdef PERF_CHANGE_SEQ_LOCK
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structure is not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LockNullChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect @ref FLCN_ERR_NOT_SUPPORTED
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LockNullChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LockNullChangeSeq)
// {
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(NULL);
//     lock.pmu.bLock = LW_TRUE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where an attempt is made to lock an already locked
 * @ref CHANGESEQ.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LockLwrrentlyLocked
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the CHANGESEQ state to be locked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect @ref FLCN_ERR_ILWALID_STATE to be
 * returned.
 * @li Verify the @ref CHANGESEQ::bLockWaiting flag is not set.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LockLwrrentlyLocked,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LockLwrrentlyLocked)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
//     lock.pmu.bLock = LW_TRUE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_ERR_ILWALID_STATE);
//     UT_ASSERT_FALSE(changeSeq.pmu.bLockWaiting);
// }

/*!
 * @brief Test the case where an attempt is made to lock a @ref CHANGESEQ that
 * is lwrrently processing a change request (via the daemon).
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LockChangeInProgress
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the CHANGESEQ state to be in progress
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect @ref FLCN_ERR_LOCK_NOT_AVAILABLE is
 * returned.
 * @li Verify the @ref CHANGESEQ::bLockWaiting flag is set.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LockChangeInProgress,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is processing a change.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LockChangeInProgress)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;
//     lock.pmu.bLock = LW_TRUE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_ERR_LOCK_NOT_AVAILABLE);
//     UT_ASSERT_TRUE(changeSeq.pmu.bLockWaiting);
// }

/*!
 * @brief Test the case where an attempt to lock the @ref CHANGESEQ is
 * successful.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LockSuccessful
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the CHANGESEQ state to be idle
 * LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect @ref FLCN_OK to be returned.
 * @li Verify the state to be locked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED).
 * @li Verify the @ref CHANGESEQ::bLockWaiting flag is not set.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LockSuccessful,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is locked.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LockSuccessful)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     lock.pmu.bLock = LW_TRUE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED);
//     UT_ASSERT_FALSE(changeSeq.pmu.bLockWaiting);
// }

/*!
 * @brief Test the case where an attempt to lock the @ref CHANGESEQ is made when
 * the change sequencer is an unknown state.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LockUnknownState
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the CHANGESEQ state to be unknown (non-enumerated value of
 * 0xFF).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect @ref FLCN_ERR_ILWALID_STATE to be
 * returned.
 * @li Verify the state to be unchanged (0xFF).
 * @li Verify the @ref CHANGESEQ::bLockWaiting flag is not set.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, LockUnknownState,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is in an unknown state.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, LockUnknownState)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = 0xFF;
//     lock.pmu.bLock = LW_TRUE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_ERR_ILWALID_STATE);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, 0xFF);
//     UT_ASSERT_FALSE(changeSeq.pmu.bLockWaiting);
// }

/*!
 * @brief Test case where an attempt to unlock the @ref CHANGESEQ is made on a
 * change sequencer that is not locked.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.UnlockNotLocked
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the CHANGESEQ state to be unlocked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect @ref FLCN_ERR_ILWALID_STATE to be
 * returned.
 * @li Verify the change sequencer state has not been modified.
 * @li Verify the @ref CHANGESEQ::bLockWaiting flag is not set.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, UnlockNotLocked,
//                 UT_CASE_SET_DESCRIPTION("Unlock a change sequencer that isn't locked.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, UnlockNotLocked)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     lock.pmu.bLock = LW_FALSE;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_ERR_ILWALID_STATE);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE);
//     UT_ASSERT_FALSE(changeSeq.pmu.bLockWaiting);
// }

/*!
 * @brief Test case where an error happens processing any pending requests after
 * unlocking the @ref CHANGESEQ.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.UnlockErrorProcessingPendingChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the CHANGESEQ state to be locked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED).
 * @li Configure the mocked version of @ref perfChangeSeqProcessPendingChange()
 * to return a non-FLCN_OK value.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect the value returned by the mocked
 * version of @ref perfChangeSeqProcessPendingChange() to be returned.
 * @li Verify the change sequencer state is unlocked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Verify the @ref CHANGESEQ::bLockWaiting flag is not set.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, UnlockErrorProcessingPendingChange,
//                 UT_CASE_SET_DESCRIPTION("Error processing next change during unlock.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, UnlockErrorProcessingPendingChange)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
//     lock.pmu.bLock = LW_FALSE;
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_ERR_DMA_NACK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_ERR_DMA_NACK);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE);
//     UT_ASSERT_FALSE(changeSeq.pmu.bLockWaiting);
// }

/*!
 * @brief Test case when an attempt to unlock the @ref CHANGESEQ is successful.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.UnlockSuccessful
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the CHANGESEQ state to be locked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED).
 * @li Configure the mocked version of @ref perfChangeSeqProcessPendingChange()
 * to return @ref FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqLock(). Expect @ref FLCN_OK to be returned.
 * @li Verify the change sequencer state is unlocked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Verify the @ref CHANGESEQ::bLockWaiting flag is not set.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, UnlockSuccessful,
//                 UT_CASE_SET_DESCRIPTION("Successfully unlock the change sequencer.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, UnlockSuccessful)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     RM_PMU_PERF_CHANGE_SEQ_LOCK lock;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
//     lock.pmu.bLock = LW_FALSE;
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqLock(&lock), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE);
//     UT_ASSERT_FALSE(changeSeq.pmu.bLockWaiting);
// }
#endif // PERF_CHANGE_SEQ_LOCK

/* ------------- perfChangeSeqValidateClkDomainInputMask_IMPL() ------------- */
#ifdef PERF_CHANGE_SEQ_VALIDATE_CLK_DOMAIN_INPUT_MASK
/*!
 * @brief Test case where the input mask does not contain a subset of clock
 * domains.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ValidateClkDomainInputMaskNotSubset
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure exclusion mask in the change sequencer to have bits[31:10] set.
 * @li Configure inclusion mask in the change sequencer to have bits[9:0] set.
 * @li Configure programmable clock domain mask in the change sequencer to have
 * bits 0, 2, and 7 set.
 * @li Configure the input mask to have bits 0 and 3 set.
 *
 * Test Exelwtion:
 * @li Call @perfChangeSeqValidateClkDomainInputMask_IMPL(). Expect the function
 * to return @ref FLCN_ERR_ILWALID_ARGUMENT.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ValidateClkDomainInputMaskNotSubset,
//                 UT_CASE_SET_DESCRIPTION("Mask provided is not a subset.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ValidateClkDomainInputMaskNotSubset)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     BOARDOBJGRPMASK_E32 clkDomainsMask;
//     boardObjGrpMaskInit_E32(&clkDomainsMask);
//     boardObjGrpMaskBitSet(&clkDomainsMask, 0);
//     boardObjGrpMaskBitSet(&clkDomainsMask, 3);

//     boardObjGrpMaskInit_E32(&changeSeq.clkDomainsExclusionMask);
//     boardObjGrpMaskBitRangeSet(&changeSeq.clkDomainsExclusionMask, 10, 31);
//     boardObjGrpMaskInit_E32(&changeSeq.clkDomainsInclusionMask);
//     boardObjGrpMaskBitRangeSet(&changeSeq.clkDomainsInclusionMask, 0, 9);

//     boardObjGrpMaskInit_E32(&Clk.domains.progDomainsMask);
//     boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 0);
//     boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 2);
//     boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 7);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqValidateClkDomainInputMask_IMPL(&changeSeq, &clkDomainsMask), FLCN_ERR_ILWALID_ARGUMENT);
// }

/*!
 * @brief Test case where the input mask does not contain a subset of clock
 * domains.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ValidateClkDomainInputMaskSubset
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure exclusion mask in the change sequencer to have bits[31:10] set.
 * @li Configure inclusion mask in the change sequencer to have bits[9:0] set.
 * @li Configure programmable clock domain mask in the change sequencer to have
 * bits 0, 2, and 7 set.
 * @li Configure the input mask to have bits 0 and 7 set.
 *
 * Test Exelwtion:
 * @li Call @perfChangeSeqValidateClkDomainInputMask_IMPL(). Expect the function
 * to return @ref FLCN_OK.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ValidateClkDomainInputMaskSubset,
//                 UT_CASE_SET_DESCRIPTION("Mask provided is a subset.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ValidateClkDomainInputMaskSubset)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     BOARDOBJGRPMASK_E32 clkDomainsMask;
//     boardObjGrpMaskInit_E32(&clkDomainsMask);
//     boardObjGrpMaskBitSet(&clkDomainsMask, 0);
//     boardObjGrpMaskBitSet(&clkDomainsMask, 7);

//     boardObjGrpMaskInit_E32(&changeSeq.clkDomainsExclusionMask);
//     boardObjGrpMaskBitRangeSet(&changeSeq.clkDomainsExclusionMask, 10, 31);
//     boardObjGrpMaskInit_E32(&changeSeq.clkDomainsInclusionMask);
//     boardObjGrpMaskBitRangeSet(&changeSeq.clkDomainsInclusionMask, 0, 9);

//     boardObjGrpMaskInit_E32(&Clk.domains.progDomainsMask);
//     boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 0);
//     boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 2);
//     boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 7);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqValidateClkDomainInputMask_IMPL(&changeSeq, &clkDomainsMask), FLCN_ERR_ILWALID_ARGUMENT);
// }
#endif // PERF_CHANGE_SEQ_VALIDATE_CLK_DOMAIN_INPUT_MASK

/* --------------------- perfChangeSeqQueueVoltOffset() --------------------- */
#ifdef PERF_CHANGE_SEQ_QUEUE_VOLT_OFFSET
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structureis not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.LockNullChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueVoltOffset(). Expect the function to return
 * @ref FLCN_ERR_NOT_SUPPORTED.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueVoltOffsetNullChangeSequencer,
//                 UT_CASE_SET_DESCRIPTION("No change sequencer present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueVoltOffsetNullChangeSequencer)
// {
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET input = { 0 };
//     utChangeSeqInit(NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueVoltOffset(&input), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where the parameter is NULL.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueVoltOffsetNullParameter
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueVoltOffset() with a null pointer. Expect the
 * function to return @ref FLCN_ERR_NOT_SUPPORTED.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueVoltOffsetNullParameter,
//                 UT_CASE_SET_DESCRIPTION("Null pointer passed in.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueVoltOffsetNullParameter)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     utChangeSeqInit(&changeSeq);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueVoltOffset(NULL), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test case where the input mask does not contain a subset of volt
 * rails.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueVoltOffsetNotSubset
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Configure the volt rail mask to have bits[3:0] set.
 * @li Configure the input mask to have bits [7:4] set.
 *
 * Test Exelwtion:
 * @li Call @perfChangeSeqQueueVoltOffset(). Expect the function
 * to return @ref FLCN_ERR_ILWALID_ARGUMENT.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueVoltOffsetNotSubset,
//                 UT_CASE_SET_DESCRIPTION("Null pointer passed in.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueVoltOffsetNotSubset)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET input = { 0 };

//     utChangeSeqInit(&changeSeq);

//     boardObjGrpMaskInit_SUPER(&Volt.railMetadata.super.objMask,
//                               LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 0);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 1);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 2);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 3);

//     input.voltRailsMask.super.pData[0] = 0x000000F0;
//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
//     boardObjGrpMaskIsSubsetMockAddEntry(0, LW_FALSE, NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueVoltOffset(&input), FLCN_ERR_ILWALID_ARGUMENT);
// }

// static void utVoltRailInit(BOARDOBJ **ppBoardObjs)
// {
//     Volt.railMetadata.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
//     Volt.railMetadata.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
//     Volt.railMetadata.super.super.objSlots = 32;
//     Volt.railMetadata.super.super.ovlIdxDmem = 3;
//     Volt.railMetadata.super.super.bConstructed = LW_TRUE;
//     Volt.railMetadata.super.super.ppObjects = ppBoardObjs;
// }

/*!
 * @brief Test case where the input mask does not contain a subset of volt
 * rails.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueVoltOffsetQueueChangeNoProcess
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Initialize the volt rails to contain a single volt rail.
 * @li Ilwalidate CHANGESEQ::pChangeNext and CHANGESEQ::pChangeForce.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueVoltOffset(). Verify @ref FLCN_OK is
 * returned.
 * @li Verify a change request has been queued up in the normal queue.
 * @li Verify the queued change request is asynchronous.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueVoltOffsetQueueChangeNoProcess,
//                 UT_CASE_SET_DESCRIPTION("Queues up a change.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueVoltOffsetQueueChangeNoProcess)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET input = { 0 };
//     VOLT_RAIL voltRail;
//     BOARDOBJ *ppBoardObj[32];

//     ppBoardObj[2] = (BOARDOBJ*)&voltRail;

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
//     changeSeq.changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
//     changeSeq.pChangeForce = &forceChange;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeLast = &lastChange;
//     changeSeq.pChangeLwrr = &lwrrentChange;

//     utVoltRailInit(ppBoardObj);

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);
//     utChangeRequestInit(&lastChange, 2, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE);
//     utChangeRequestInit(&lwrrentChange, 3, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE_CLOCKS);

//     boardObjGrpMaskInit_SUPER(&Volt.railMetadata.super.objMask,
//                               LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 2);
//     input.voltRailsMask.super.pData[0] = 0x00000004;

//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
//     boardObjGrpMaskIsSubsetMockAddEntry(0, LW_TRUE, NULL);

//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++i)
//     {
//         input.rails[2].voltOffsetuV[i] = i * 1000;
//     }

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueVoltOffset(&input), FLCN_OK);
//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++i)
//     {
//         UT_ASSERT_EQUAL_UINT(changeSeq.voltOffsetCached.rails[2].voltOffsetuV[i], i * 1000);
//     }
//     UT_ASSERT_EQUAL_UINT(nextChange.pstateIndex, 2);
//     UT_ASSERT_EQUAL_UINT(nextChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
// }

// The following test cases are disabled, as they cause the UTF to fail
// initialization. Need to work with the UTF owners to resolve.
// * QueueVoltOffsetQueueChangeInProgress
// * QueueVoltOffsetQueueChangeProcess

/*!
 * @brief Test the case where the daemon is lwrrently processing a change
 * request.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueVoltOffsetQueueChangeNoProcess
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set teh @ref CHANGESEQ status to in progress
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS).
 * @li Initialize the volt rails to contain a single volt rail.
 * @li Ilwalidate CHANGESEQ::pChangeNext and CHANGESEQ::pChangeForce.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueVoltOffset(). Verify @ref FLCN_OK is
 * returned.
 * @li Verify a the current change is copied as the next change request.
 * @li Verify the queued change request is asynchronous.
 * @li Verify @ref perfChangeSeqProcessPendingChange() was not called.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueVoltOffsetQueueChangeInProgress,
//                 UT_CASE_SET_DESCRIPTION("Queues up a change while one is lwrrently in progress.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueVoltOffsetQueueChangeInProgress)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET input = { 0 };
//     VOLT_RAIL voltRail[2];
//     BOARDOBJ *ppBoardObj[32];

//     ppBoardObj[2] = (BOARDOBJ*)&voltRail[0];
//     ppBoardObj[4] = (BOARDOBJ*)&voltRail[1];

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;
//     changeSeq.changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
//     changeSeq.pChangeForce = &forceChange;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeLast = &lastChange;
//     changeSeq.pChangeLwrr = &lwrrentChange;

//     utVoltRailInit(ppBoardObj);

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);
//     utChangeRequestInit(&lastChange, 2, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE);
//     utChangeRequestInit(&lwrrentChange, 3, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE_CLOCKS);

//     boardObjGrpMaskInit_SUPER(&Volt.railMetadata.super.objMask,
//                               LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 2);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 4);
//     input.voltRailsMask.super.pData[0] = 0x00000014;

//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
//     boardObjGrpMaskIsSubsetMockAddEntry(0, LW_TRUE, NULL);

//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++i)
//     {
//         input.rails[2].voltOffsetuV[i] = i * 4000;
//         input.rails[4].voltOffsetuV[i] = i * 500;
//     }

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueVoltOffset(&input), FLCN_OK);
//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++i)
//     {
//         UT_ASSERT_EQUAL_UINT(changeSeq.voltOffsetCached.rails[2].voltOffsetuV[i], i * 4000);
//         UT_ASSERT_EQUAL_UINT(changeSeq.voltOffsetCached.rails[4].voltOffsetuV[i], i * 500);
//     }
//     UT_ASSERT_EQUAL_UINT(nextChange.pstateIndex, 3);
//     UT_ASSERT_EQUAL_UINT(nextChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
// }

/*!
 * @brief Test the case where the daemon is lwrrently processing a change
 * request.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueVoltOffsetQueueChangeProcess
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the @ref CHANGESEQ state to idle
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Initialize the volt rails to contain a single volt rail.
 * @li Ilwalidate CHANGESEQ::pChangeNext and CHANGESEQ::pChangeForce.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueVoltOffset(). Verify @ref FLCN_OK is
 * returned.
 * @li Verify a the current change is copied as the next change request.
 * @li Verify the queued change request is asynchronous.
 * @li Verify @ref perfChangeSeqProcessPendingChange() was called.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueVoltOffsetQueueChangeProcess,
//                 UT_CASE_SET_DESCRIPTION("Queues up a change and process (with error happening).")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueVoltOffsetQueueChangeProcess)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET input = { 0 };
//     VOLT_RAIL voltRail;
//     BOARDOBJ *ppBoardObj[32];

//     ppBoardObj[4] = (BOARDOBJ*)&voltRail;

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
//     changeSeq.pChangeForce = &forceChange;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeLast = &lastChange;
//     changeSeq.pChangeLwrr = &lwrrentChange;

//     utVoltRailInit(ppBoardObj);

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);
//     utChangeRequestInit(&lastChange, 2, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE);
//     utChangeRequestInit(&lwrrentChange, 3, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE_CLOCKS);

//     boardObjGrpMaskInit_SUPER(&Volt.railMetadata.super.objMask,
//                               LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 4);
//     input.voltRailsMask.super.pData[0] = 0x00000010;

//     boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
//     boardObjGrpMaskIsSubsetMockAddEntry(0, LW_TRUE, NULL);
//     perfChangeSeqProcessPendingChange_MOCK_CONFIG.status = FLCN_ERR_HDCP22_FLUSH_TYPE_LOCK_ACTIVE;

//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++i)
//     {
//         input.rails[4].voltOffsetuV[i] = i * 2000;
//     }

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueVoltOffset(&input), FLCN_ERR_HDCP22_FLUSH_TYPE_LOCK_ACTIVE);
//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++i)
//     {
//         UT_ASSERT_EQUAL_UINT(changeSeq.voltOffsetCached.rails[4].voltOffsetuV[i], i * 2000);
//     }
//     UT_ASSERT_EQUAL_UINT(nextChange.pstateIndex, 2);
//     UT_ASSERT_EQUAL_UINT(nextChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
// }
#endif // PERF_CHANGE_SEQ_QUEUE_VOLT_OFFSET

/* ------------------ perfChangeSeqAdjustVoltOffsetValues() ----------------- */
#ifdef PERF_CHANGE_SEQ_ADJUST_VOLT_OFFSET
/*!
 * @brief Test case where the total voltage offset are determined to be greater
 * than 0.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.AdjustVoltOffsetValuesPositive
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the cached voltage offset mask with bits[3:0] set.
 * @li Configure the cached voltage offset to known values.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqAdjustVoltOffsetValues().
 * @li Verify that @a pChangeLwrr has the same voltage offsets as the change
 * sequencer @a pChangeSeq.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, AdjustVoltOffsetValuesPositive,
//                 UT_CASE_SET_DESCRIPTION("The sum of the voltage offsets are positive.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, AdjustVoltOffsetValuesPositive)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE change = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.voltOffsetCached.voltRailsMask.super.pData[0] = 0x000000F;
//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; ++i)
//     {
//         for (LwU8 j = 0; j < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++j)
//         {
//             changeSeq.voltOffsetCached.rails[i].voltOffsetuV[j] = (i * j) * 1000;
//         }
//     }

//     perfChangeSeqAdjustVoltOffsetValues(&changeSeq, &change);
//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; ++i)
//     {
//         for (LwU8 j = 0; j < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++j)
//         {
//             UT_ASSERT_EQUAL_UINT32(change.voltList.rails[i].voltOffsetuV[j], (i * j) * 1000);
//         }
//     }
// }

// The following test cases are disabled, need to understand what is being done
// when the voltage offsets are negative.
// * AdjustVoltOffsetValuesNegative

/*!
 * @brief Test case where the total voltage offset are determined to be less
 * than 0.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.AdjustVoltOffsetValuesNegative
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the cached voltage offset mask with bits[3:0] set.
 * @li Configure the cached voltage offset to known values.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqAdjustVoltOffsetValues().
 * @li Verify that @a pChangeLwrr has the same voltage offsets as the change
 * sequencer @a pChangeSeq.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, AdjustVoltOffsetValuesNegative,
//                 UT_CASE_SET_DESCRIPTION("The sum of the voltage offsets are negative.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, AdjustVoltOffsetValuesNegative)
// {
//     CHANGE_SEQ changeSeq = { 0 };
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE change = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.voltOffsetCached.voltRailsMask.super.pData[0] = 0x000000F;
//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; ++i)
//     {
//         for (LwU8 j = 0; j < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++j)
//         {
//             changeSeq.voltOffsetCached.rails[i].voltOffsetuV[j] = (i + 1) * (j + 1) * -1000;
//         }
//         change.voltList.rails[i].voltageMinNoiseUnawareuV = 750000;
//         change.voltList.rails[i].voltageuV = (LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS - i) * 10000;
//     }

//     perfChangeSeqAdjustVoltOffsetValues(&changeSeq, &change);
//     for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; ++i)
//     {
//         for (LwU8 j = 0; j < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++j)
//         {
//             UT_ASSERT_EQUAL_UINT32(
//                 change.voltList.rails[i].voltOffsetuV[j],
//                 changeSeq.voltOffsetCached.rails[i].voltOffsetuV[j]
//             );
//         }
//     }

//     UT_ASSERT_TRUE(LW_FALSE);
// }
#endif // PERF_CHANGE_SEQ_ADJUST_VOLT_OFFSET

/* ----------------------- perfChangeSeqQueueChange() ----------------------- */
#ifdef PERF_CHANGE_SEQ_QUEUE_CHANGE
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structure is not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeNullChangeSequencer
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange(). Expect @ref FLCN_ERR_NOT_SUPPORTED
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeNullChangeSequencer,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeNullChangeSequencer)
// {
//     utChangeSeqInit(NULL);
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where a null pointer is passed in as either parameter.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeNullParameter
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with @a pChangeInput set to NULL.
 * Expect @ref FLCN_ERR_NOT_SUPPORTED to be returned.
 * @li Call @ref perfChangeSeqQueueChange() with @a pChangeRequest set to NULL.
 * Expect @ref FLCN_ERR_NOT_SUPPORTED to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeNullParameter,
//                 UT_CASE_SET_DESCRIPTION("Null pointers as parameters.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeNullParameter)
// {
//     CHANGE_SEQ changeSeq;
//     utChangeSeqInit(&changeSeq);
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(NULL, &request), FLCN_ERR_NOT_SUPPORTED);
//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, NULL), FLCN_ERR_NOT_SUPPORTED);
// }

/*!
 * @brief Test the case where an invalid change request is passed in.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeIlwalidChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT::pstateIndex
 * to be LW2080_CTRL_BOARDOBJ_IDX_ILWALID.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with the invalid change input.
 * Expect @ref FLCN_ERR_ILWALID_ARGUMENT to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeIlwalidChange,
//                 UT_CASE_SET_DESCRIPTION("Change input is invalid.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeIlwalidChange)
// {
//     CHANGE_SEQ changeSeq;
//     utChangeSeqInit(&changeSeq);
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     input.pstateIndex = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_ERR_ILWALID_ARGUMENT);
// }

/*!
 * @brief Test the case where a called function returns an error.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeIlwalidChange
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the mocked version of @ref perfChangeSeqSyncChangeQueueEnqueue()
 * to return a non-FLCN_OK value.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with the valid change input.
 * Expect the non-FLCN_OK value returned by the mocked function to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeErrorFromCalledFunction,
//                 UT_CASE_SET_DESCRIPTION("Recovery from error returned from called function.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeErrorFromCalledFunction)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeForce = &forceChange;

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);

//     input.pstateIndex = 0;
//     input.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE;
//     perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG.status = FLCN_ERR_ILWALID_VERSION;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_ERR_ILWALID_VERSION);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&forceChange));
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
// }

/*!
 * @brief Test the case where a normal priority change is queued.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeNormalPriority
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the CHANGESEQ structure's @p pChangeForce and @p pChangeNext
 * data member to use structures allocated on the stack.
 * @li Ilwalidate the CHANGESEQ structure's @p pChangeForce and @p pChangeNext.
 * @li Configure the mocked version of @ref boardObjGrpMaskInit_E32() and
 * @ref perfChangeSeqProcessPendingChange() to return FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with a normal change input.
 * Expect FLCN_OK to be returned.
 * @li Verify @ref CHANGESEQ::pNextChange equals the input change request.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeNormalPriority,
//                 UT_CASE_SET_DESCRIPTION("Queue normal request with no items queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeNormalPriority)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeForce = &forceChange;

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);

//     input.pstateIndex = 1;
//     input.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC;

//     perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status = FLCN_OK;
//     boardObjGrpMaskIsEqualMockAddEntry(0, LW_TRUE, NULL);
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_OK);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&forceChange));
//     UT_ASSERT_EQUAL_UINT32(nextChange.pstateIndex, 1);
//     UT_ASSERT_EQUAL_UINT32(nextChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
// }

/*!
 * @brief Test the case where a forced priority change is queued.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeForcePriority
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the CHANGESEQ structure's @p pChangeForce and @p pChangeNext
 * data member to use structures allocated on the stack.
 * @li Ilwalidate the CHANGESEQ structure's @p pChangeForce and @p pChangeNext.
 * @li Configure the mocked version of @ref boardObjGrpMaskInit_E32() and
 * @ref perfChangeSeqProcessPendingChange() to return FLCN_OK.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with a forced change input.
 * Expect FLCN_OK to be returned.
 * @li Verify @ref CHANGESEQ::pChangeForce equals the input change request.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeForcePriority,
//                 UT_CASE_SET_DESCRIPTION("Queue forced request with no items queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeForcePriority)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeForce = &forceChange;

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);

//     input.pstateIndex = 4;
//     input.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC | LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE;

//     perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status = FLCN_OK;
//     boardObjGrpMaskIsEqualMockAddEntry(0, LW_TRUE, NULL);
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT32(forceChange.pstateIndex, 4);
//     UT_ASSERT_EQUAL_UINT32(forceChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC | LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
// }

/*!
 * @brief Test the case where a normal priority change is queued when another
 * normal change is queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeNormalPriorityWithNormalQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the CHANGESEQ structure's @p pChangeForce and @p pChangeNext
 * data member to use structures allocated on the stack.
 * @li Ilwalidate the CHANGESEQ structure's @p pChangeForce and @p pChangeNext.
 * @li Configure the mocked version of @ref boardObjGrpMaskInit_E32() and
 * @ref perfChangeSeqProcessPendingChange() to return FLCN_OK.
 * @li Queue a normal change request.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with a normal change input.
 * Expect FLCN_OK to be returned.
 * @li Verify @ref CHANGESEQ::pChangeNext equals the input change request.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeNormalPriorityWithNormalQueued,
//                 UT_CASE_SET_DESCRIPTION("Queue normal request with a normal priority request queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeNormalPriorityWithNormalQueued)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeForce = &forceChange;

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     nextChange.pstateIndex = 3;
//     nextChange.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SKIP_VBLANK_WAIT;

//     input.pstateIndex = 4;
//     input.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC;

//     perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status = FLCN_OK;
//     boardObjGrpMaskIsEqualMockAddEntry(0, LW_TRUE, NULL);
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_OK);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&forceChange));
//     UT_ASSERT_EQUAL_UINT32(nextChange.pstateIndex, 4);
//     UT_ASSERT_EQUAL_UINT32(nextChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
// }

/*!
 * @brief Test the case where a forced priority change is queued when a normal
 * change is queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeForcePriorityWithNormalQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the CHANGESEQ structure's @p pChangeForce and @p pChangeNext
 * data member to use structures allocated on the stack.
 * @li Ilwalidate the CHANGESEQ structure's @p pChangeForce and @p pChangeNext.
 * @li Configure the mocked version of @ref boardObjGrpMaskInit_E32() and
 * @ref perfChangeSeqProcessPendingChange() to return FLCN_OK.
 * @li Queue a normal change request.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with a forced change input.
 * Expect FLCN_OK to be returned.
 * @li Verify @ref CHANGESEQ::pChangeForce equals the input change.
 * @li Verify @ref CHANGESEQ::pChangeNext is invalid.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeForcePriorityWithNormalQueued,
//                 UT_CASE_SET_DESCRIPTION("Queue forced request with a normal priority request queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeForcePriorityWithNormalQueued)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeForce = &forceChange;

//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     nextChange.pstateIndex = 6;
//     nextChange.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SKIP_VBLANK_WAIT;

//     input.pstateIndex = 10;
//     input.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE;

//     perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG.status = FLCN_OK;
//     perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status = FLCN_OK;
//     boardObjGrpMaskIsEqualMockAddEntry(0, LW_TRUE, NULL);
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT32(forceChange.pstateIndex, 10);
//     UT_ASSERT_EQUAL_UINT32(forceChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
// }

/*!
 * @brief Test the case where a normal priority change is queued when another
 * forced change is queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeNormalPriorityWithForceQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the CHANGESEQ structure's @p pChangeForce and @p pChangeNext
 * data member to use structures allocated on the stack.
 * @li Ilwalidate the CHANGESEQ structure's @p pChangeForce and @p pChangeNext.
 * @li Configure the mocked version of @ref boardObjGrpMaskInit_E32() and
 * @ref perfChangeSeqProcessPendingChange() to return FLCN_OK.
 * @li Queue a forced change request.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with a forced change input.
 * Expect FLCN_OK to be returned.
 * @li Verify @ref CHANGESEQ::pChangeForce is unchanged.
 * @li Verify @ref CHANGESEQ::pChangeNext equals the input.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeNormalPriorityWithForceQueued,
//                 UT_CASE_SET_DESCRIPTION("Queue normal request with a forced priority request queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeNormalPriorityWithForceQueued)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeForce = &forceChange;

//     forceChange.pstateIndex = 7;
//     forceChange.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);

//     input.pstateIndex = 21;
//     input.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC;

//     perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status = FLCN_OK;
//     boardObjGrpMaskIsEqualMockAddEntry(0, LW_TRUE, NULL);
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT32(forceChange.pstateIndex, 7);
//     UT_ASSERT_EQUAL_UINT32(forceChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE);
//     UT_ASSERT_EQUAL_UINT32(nextChange.pstateIndex, 21);
//     UT_ASSERT_EQUAL_UINT32(nextChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
// }

/*!
 * @brief Test the case where a forced priority change is queued when another
 * forced change is queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.QueueChangeForcePriorityWithForceQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure.
 * @li Configure the CHANGESEQ structure's @p pChangeForce and @p pChangeNext
 * data member to use structures allocated on the stack.
 * @li Ilwalidate the CHANGESEQ structure's @p pChangeForce and @p pChangeNext.
 * @li Configure the mocked version of @ref boardObjGrpMaskInit_E32() and
 * @ref perfChangeSeqProcessPendingChange() to return FLCN_OK.
 * @li Queue a forced change request.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqQueueChange() with a forced change input.
 * Expect FLCN_ERR_ILLEGAL_OPERATION to be returned.
 * @li Verify @ref CHANGESEQ::pChangeForce is unchanged.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, QueueChangeForcePriorityWithForceQueued,
//                 UT_CASE_SET_DESCRIPTION("Queue force request with a forced priority request queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, QueueChangeForcePriorityWithForceQueued)
// {
//     // This test case fails. There is a bug in the change sequencer where it
//     // doesn't actually check to see a forced request is already queued up
//     // before copying the new request to the force queue.
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT input = { 0 };
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     changeSeq.pChangeNext = &nextChange;
//     changeSeq.pChangeForce = &forceChange;

//     forceChange.pstateIndex = 13;
//     forceChange.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);

//     input.pstateIndex = 11;
//     input.flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE;

//     perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status = FLCN_OK;
//     boardObjGrpMaskIsEqualMockAddEntry(0, LW_TRUE, NULL);
//     perfChangeSeqProcessPendingChangeMockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqQueueChange(&input, &request), FLCN_ERR_ILLEGAL_OPERATION);
//     UT_ASSERT_EQUAL_UINT32(forceChange.pstateIndex, 13);
//     UT_ASSERT_EQUAL_UINT32(forceChange.flags, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
// }
#endif // PERF_CHANGE_SEQ_QUEUE_CHANGE

/* --------------------- perfChangeSeqScriptCompletion() -------------------- */
#ifdef PERF_CHANGE_SEQ_SCRIPT_COMPLETION
/*!
 * @brief Test the case where the function is called while the state is not
 * busy.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ScriptCompletionNotInProgress
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be idle
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqScriptCompletion().
 * @li Verify the state has not changed.
 * @li Verify @ref perfChangeSeqProcessPendingChange() has not been called.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ScriptCompletionNotInProgress,
//                 UT_CASE_SET_DESCRIPTION("No change lwrrently being processed.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ScriptCompletionNotInProgress)
// {
//     CHANGE_SEQ changeSeq;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;

//     perfChangeSeqScriptCompletion(FLCN_OK);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE);
//     UT_ASSERT_EQUAL_UINT8(perfChangeSeqProcessPendingChangeMockNumCalls(), 0);
// }

/*!
 * @brief Test the case where the function is called when the daemon responded
 * with nothing to do.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ScriptCompletionNothingToDo
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be busy
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqScriptCompletion() with FLCN_WARN_NOTHING_TO_DO
 * as the parameter.
 * @li Verify the state is now waiting
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_WAITING).
 * @li Verify @ref perfChangeSeqProcessPendingChange() has been called once.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ScriptCompletionNothingToDo,
//                 UT_CASE_SET_DESCRIPTION("Daemon response FLCN_WARN_NOTHING_TO_DO.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ScriptCompletionNothingToDo)
// {
//     CHANGE_SEQ changeSeq;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;

//     perfChangeSeqScriptCompletion(FLCN_WARN_NOTHING_TO_DO);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_WAITING);
//     UT_ASSERT_EQUAL_UINT8(perfChangeSeqProcessPendingChangeMockNumCalls(), 1);
// }

/*!
 * @brief Test the case where the function is called when the daemon responded
 * with an error.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ScriptCompletionErrorResponse
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be busy
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqScriptCompletion() with a non-FLCN_OK value
 * as the parameter.
 * @li Verify the state is now in error
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_ERROR).
 * @li Verify @ref perfChangeSeqProcessPendingChange() has not been called.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ScriptCompletionErrorResponse,
//                 UT_CASE_SET_DESCRIPTION("Daemon response with error.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ScriptCompletionErrorResponse)
// {
//     CHANGE_SEQ changeSeq;
//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;

//     perfChangeSeqScriptCompletion(FLCN_ERR_ILWALID_STATE);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_ERROR);
//     UT_ASSERT_EQUAL_UINT8(perfChangeSeqProcessPendingChangeMockNumCalls(), 0);
// }

/*!
 * @brief Test the case where the function is called when the daemon responded
 * with success, but a lock is pending.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ScriptCompletionLockPending
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be busy
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS).
 * @li Set @ref CHANGESEQ::pmu.bLockWaiting flag.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqScriptCompletion() with FLCN_OK as the parameter.
 * @li Verify the state is now locked
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED).
 * @li Verify @ref CHANGESEQ::pmu.bLockWaiting is not set.
 * @li Verify @ref perfChangeSeqProcessPendingChange() has not been called.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ScriptCompletionLockPending,
//                 UT_CASE_SET_DESCRIPTION("A request to lock the sequencer was made while processing a change request.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ScriptCompletionLockPending)
// {
//     CHANGE_SEQ changeSeq;
//     RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT scriptLast;
//     DMA_MOCK_MEMORY_REGION scriptLastRegion =
//     {
//         .pMemDesc = &PmuInitArgs.superSurface,
//         .pData = &scriptLast,
//         .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptLast),
//         .sizeOfData = sizeof(scriptLast),
//     };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;
//     changeSeq.pmu.bLockWaiting = LW_TRUE;

//     dmaMockWriteAddEntry(0, FLCN_OK);
//     dmaMockWriteAddEntry(1, FLCN_OK);
//     dmaMockConfigMemoryRegionInsert(&scriptLastRegion);

//     lwrtosQueueSendBlockingMockAddEntry(0, FLCN_ERR_ILWALID_STATE, NULL);

//     // for PERF_CHANGE_SEQ_PROFILE_END()
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER0, 0x00000000);
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER1, 0x00000000);

//     perfChangeSeqScriptCompletion(FLCN_OK);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED);
//     UT_ASSERT_EQUAL_UINT8(changeSeq.pmu.bLockWaiting, LW_FALSE);
//     UT_ASSERT_EQUAL_UINT(dmaMockReadNumCalls(), 0);
//     UT_ASSERT_EQUAL_UINT(dmaMockWriteNumCalls(), 2);
//     UT_ASSERT_EQUAL_UINT8(perfChangeSeqProcessPendingChangeMockNumCalls(), 0);
// }

/*!
 * @brief Test the case where the function is called when the daemon responded
 * with success.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ScriptCompletionSendNotification
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be busy
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS).
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqScriptCompletion() with FLCN_OK as the parameter.
 * @li Verify the state is now idle
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Verify @ref perfChangeSeqProcessPendingChange() has been called once.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ScriptCompletionSendNotification,
//                 UT_CASE_SET_DESCRIPTION("Send notification upon completion for sync requests.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ScriptCompletionSendNotification)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ALIGNED changeLwrr;
//     RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT scriptLast;
//     RM_PMU_PERF_CHANGE_SEQ_PMU_SCRIPT scriptLwrr;

//     DMA_MOCK_MEMORY_REGION scriptLastRegion =
//     {
//         .pMemDesc = &PmuInitArgs.superSurface,
//         .pData = &scriptLast,
//         .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptLast),
//         .sizeOfData = sizeof(scriptLast),
//     };

//     DMA_MOCK_MEMORY_REGION scriptLwrrRegion =
//     {
//         .pMemDesc = &PmuInitArgs.superSurface,
//         .pData = &scriptLwrr,
//         .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptLwrr),
//         .sizeOfData = sizeof(scriptLwrr),
//     };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;
//     changeSeq.script.hdr.data.numSteps = 4;
//     changeSeq.pChangeLwrr = &changeLwrr;
//     changeSeq.pChangeLwrr->data.pmu.seqId = 0x21;
//     for (LwU8 i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
//     {
//         changeSeq.pmu.syncChangeQueue[i].clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU;
//         changeSeq.pmu.syncChangeQueue[i].seqId = 0x20 + i;
//         changeSeq.pmu.syncChangeQueue[i].data.pmu.queueHandle = (LwrtosQueueHandle)(0x00010000 * 2);
//         lwrtosQueueSendBlockingMockAddEntry(i, FLCN_OK, NULL);
//     }

//     for (LwU8 i = 0; i < 6; i++)
//     {
//         dmaMockReadAddEntry(i, FLCN_OK);
//         dmaMockWriteAddEntry(i, FLCN_OK);
//     }
//     dmaMockConfigMemoryRegionInsert(&scriptLastRegion);
//     dmaMockConfigMemoryRegionInsert(&scriptLwrrRegion);

//     // for PERF_CHANGE_SEQ_PROFILE_END()
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER0, 0x00000000);
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER1, 0x00000000);

//     perfChangeSeqScriptCompletion(FLCN_OK);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.state, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE);
//     UT_ASSERT_EQUAL_UINT(dmaMockReadNumCalls(), 4);
//     UT_ASSERT_EQUAL_UINT(dmaMockWriteNumCalls(), 6);
//     UT_ASSERT_EQUAL_UINT8(lwrtosQueueSendBlockingMockNumCalls(), 2);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.syncChangeQueue[0].clientId, LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.syncChangeQueue[1].clientId, LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.syncChangeQueue[2].clientId, LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU);
//     UT_ASSERT_EQUAL_UINT(changeSeq.pmu.syncChangeQueue[3].clientId, LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU);
//     UT_ASSERT_EQUAL_UINT8(perfChangeSeqProcessPendingChangeMockNumCalls(), 1);
// }
#endif // PERF_CHANGE_SEQ_SCRIPT_COMPLETION

/* ---------------- perfChangeSeqSendCompletionNotification() --------------- */
#ifdef PERF_CHANGE_SEQ_SEND_COMPLETION_NOTIFICATION
/*!
 * @brief Test the case where the global @ref CHANGESEQ data structure is not
 * present.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SendCompletionNotificationNoChangeSeq
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ structure to be non-existant.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSendCompletionNotification(). Expect the function
 * to return @ref FLCN_ERR_ILWALID_STATE.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SendCompletionNotificationNoChangeSeq,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer is not present.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SendCompletionNotificationNoChangeSeq)
// {
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(NULL);
//     request.seqId = 0x1234;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSendCompletionNotification(LW_FALSE, &request), FLCN_ERR_ILWALID_STATE);
//     UT_ASSERT_EQUAL_UINT32(request.seqId, 0x1234);
// }

/*!
 * @brief Test the case where the pointer to the change request is null.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SendCompletionNotificationNullParameter
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSendCompletionNotification with @a pChangeRequest
 * set to NULL. Expect @ref FLCN_ERR_ILWALID_STATE to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SendCompletionNotificationNullParameter,
//                 UT_CASE_SET_DESCRIPTION("pChangeRequest is null.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SendCompletionNotificationNullParameter)
// {
//     CHANGE_SEQ changeSeq;

//     utChangeSeqInit(&changeSeq);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSendCompletionNotification(LW_FALSE, NULL), FLCN_ERR_ILWALID_STATE);
// }

/*!
 * @brief Test the case where the change request is asynchronous request.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SendCompletionNotificationAsync
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSendCompletionNotification with @a bSync set to
 * LW_TRUE. Expect @ref FLCN_OK to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SendCompletionNotificationAsync,
//                 UT_CASE_SET_DESCRIPTION("Change request was asynchronous.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SendCompletionNotificationAsync)
// {
//     CHANGE_SEQ changeSeq;
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pmu.seqIdCounter = 0xAABBCCDD;
//     request.seqId = 0x88776655;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSendCompletionNotification(LW_FALSE, &request), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT32(request.seqId, 0xAABBCCDD);
// }

/*!
 * @brief Test the case where the recently change request is synchronouse but
 * has yet to be processed.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SendCompletionNotificationSyncNotProcessed
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the last change request in @ref CHANGESEQ processed to a value.
 * @li Set the change request's ID to a value greater than the last change
 * request processed by @ref CHANGESEQ.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSendCompletionNotification with @a bSync set to
 * LW_TRUE. Expect @ref FLCN_OK to be returned.
 * @li Verify the request's sequence ID has not been modified.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SendCompletionNotificationSyncNotProcessed,
//                 UT_CASE_SET_DESCRIPTION("Synchronous request placed in notification queue.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SendCompletionNotificationSyncNotProcessed)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pChangeLast = &lastChange;
//     changeSeq.pmu.seqIdCounter = 0x80;
//     lastChange.data.pmu.seqId = 0x77;
//     request.clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU;
//     request.seqId = 0x10;
//     // Have perfChangeSeqSyncChangeQueueEnqueue return an error to verify code
//     // path has been exelwted
//     perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG.status = FLCN_ERR_CSB_PRIV_READ_ERROR;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSendCompletionNotification(LW_TRUE, &request), FLCN_ERR_CSB_PRIV_READ_ERROR);
//     UT_ASSERT_EQUAL_UINT32(request.seqId, 0x10);
// }

/*!
 * @brief Test the case where the change request input does not provide a queue
 * handle to write back to.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SendCompletionNotificationSyncNoHandle
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the last change request ID be a non-zero value.
 * @li Set the change sequencer sequencer ID to be less than the change
 * request's ID.
 * @li Set the @ref PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE::data.pmu.queueHandle
 * to be NULL.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSendCompletionNotification(). Expect the function
 * to return @ref FLCN_ERR_ILWALID_ARGUMENT.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SendCompletionNotificationSyncNoHandle,
//                 UT_CASE_SET_DESCRIPTION("Synchronous request with no queue handle.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SendCompletionNotificationSyncNoHandle)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pChangeLast = &lastChange;
//     changeSeq.pmu.seqIdCounter = 0x100;
//     lastChange.data.pmu.seqId = 0x125;
//     request.clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU;
//     request.seqId = 0x67;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSendCompletionNotification(LW_TRUE, &request), FLCN_ERR_ILWALID_ARGUMENT);
//     UT_ASSERT_EQUAL_UINT32(request.seqId, 0x67);
// }

/*!
 * @brief Test the case where the change request input does not provide a queue
 * handle to write back to.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SendCompletionNotificationSyncNoHandle
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the last change request ID be a non-zero value.
 * @li Set the change sequencer sequencer ID to be less than the change
 * request's ID.
 * @li Set the @ref PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE::data.pmu.queueHandle
 * to be NULL.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSendCompletionNotification(). Expect the function
 * to return @ref FLCN_ERR_ILWALID_ARGUMENT.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SendCompletionNotificationSyncSendNotification,
//                 UT_CASE_SET_DESCRIPTION("Synchronous request to send notification.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SendCompletionNotificationSyncSendNotification)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     changeSeq.pChangeLast = &lastChange;
//     changeSeq.pmu.seqIdCounter = 0x100;
//     lastChange.data.pmu.seqId = 0x125;
//     request.clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU;
//     request.seqId = 0x67;
//     request.data.pmu.queueHandle = (LwrtosQueueHandle)0x0BAD0CAB;

//     // Have lwrtosQueueSendBlocking return an error to verify code path as been
//     // exelwted
//     lwrtosQueueSendBlockingMockAddEntry(0, FLCN_ERR_HPD_UNPLUG, NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSendCompletionNotification(LW_TRUE, &request), FLCN_ERR_HPD_UNPLUG);
//     UT_ASSERT_EQUAL_UINT32(request.seqId, 0x67);
// }
#endif // PERF_CHANGE_SEQ_SEND_COMPLETION_NOTIFICATION

/* ----------------------- pmuRpcPerfChangeSeqQuery() ----------------------- */
#ifdef PERF_CHANGE_SEQ_RPC_SEQ_QUERY
// Keep the following test case commented out until the code is fixed. See the
// test case implementation for details.
/*!
 * @brief Test the case where the RPC buffer is not large enough to store the
 * @ref CHANGE SEQUENCER information.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.RpcChangeSeqQuery
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Create a buffer that has a size that is less than the size of the
 * CHANGESEQ.
 *
 * Test Exelwtion:
 * @li Call @ref pmuRpcPerfChangeSeqQuery(). Expect @ref FLCN_ERR_OUT_OF_RANGE
 * to be returned.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, RpcChangeSeqQuery,
//                 UT_CASE_SET_DESCRIPTION("RPC buffer is not large enough.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, RpcChangeSeqQuery)
// {
//     // The function checks to see if the RPC buffer size (PMU_DMEM_BUFFER) is
//     // less than the size of a buffer required to queue the change sequencer.
//     // The RPC buffer size is set to LW_U32_MAX, and will never fail the
//     // comparison. Therefore, that code is pointless and this function has a
//     // modified cyclomatic complexit of 1.
//     //
//     // Keeping this unit test here in case the comparison ever becomes
//     // meaningful. Should it become meaningful, we'll need to mock
//     // perfChangeSeqQuery().
//     UT_ASSERT_TRUE(LW_TRUE);
// }
#endif // PERF_CHANGE_SEQ_RPC_SEQ_QUERY

/* --------------- perfChangeSeqSyncChangeQueueEnqueue_IMPL() --------------- */
#ifdef PERF_CHANGE_SEQ_CHANGE_QUEUE_ENQUEUE
/*!
 * @brief Test the case where the sync change queue is full.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SyncChangeQueueEnqueueNoFreeSlots
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Completely fill the sync change queue.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSyncChangeQueueEnqueue_IMPL(). Expect the function
 * to return @ref FLCN_ERR_ILWALID_STATE.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SyncChangeQueueEnqueueNoFreeSlots,
//                 UT_CASE_SET_DESCRIPTION("Attempt to queue a notification with no free slots.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SyncChangeQueueEnqueueNoFreeSlots)
// {
//     CHANGE_SEQ changeSeq;
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     for (LwU8 i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
//     {
//         changeSeq.pmu.syncChangeQueue[i].clientId = 100 + i;
//     }

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSyncChangeQueueEnqueue_IMPL(&changeSeq, &request), FLCN_ERR_ILWALID_STATE);
// }

/*!
 * @brief Test the case where the change request is queued.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SyncChangeQueueEnqueueRMClient
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Create a change request that is RM-based.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSyncChangeQueueEnqueue_IMPL(). Expect the function
 * to return @ref FLCN_OK.
 * @li Verify the change request has been queued.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SyncChangeQueueEnqueueRMClient,
//                 UT_CASE_SET_DESCRIPTION("Attempt to queue a notification for an RM client.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SyncChangeQueueEnqueueRMClient)
// {
//     CHANGE_SEQ changeSeq;
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     for (LwU8 i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
//     {
//         changeSeq.pmu.syncChangeQueue[i].clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID;
//     }
//     request.clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_RM;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSyncChangeQueueEnqueue_IMPL(&changeSeq, &request), FLCN_OK);
//     UT_ASSERT_EQUAL_UINT32(changeSeq.pmu.syncChangeQueue[0].clientId, LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_RM);
//     for (LwU8 i = 1; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
//     {
//         UT_ASSERT_EQUAL_UINT32(changeSeq.pmu.syncChangeQueue[i].clientId, LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID);
//     }
// }

/*!
 * @brief Test the case where the PMU-based change request is lacks the command
 * queue to write to.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.SyncChangeQueueEnqueuePMUClientNoQueue
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Create a change request that is PMU-based. Configure to not have the
 * handle to the command queue to write back to.
 *
 * Test Exelwtion:
 * @li Call @ref perfChangeSeqSyncChangeQueueEnqueue_IMPL(). Expect the function
 * to return @ref FLCN_ERR_ILWALID_ARGUMENT.
 * @li Verify the change request has not been queued.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, SyncChangeQueueEnqueuePMUClientNoQueue,
//                 UT_CASE_SET_DESCRIPTION("Attempt to queue a notification for an PMU client without a queue.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, SyncChangeQueueEnqueuePMUClientNoQueue)
// {
//     CHANGE_SEQ changeSeq;
//     PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE request = { 0 };

//     utChangeSeqInit(&changeSeq);
//     for (LwU8 i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
//     {
//         changeSeq.pmu.syncChangeQueue[i].clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID;
//     }
//     request.clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU;
//     request.data.pmu.queueHandle = NULL;

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqSyncChangeQueueEnqueue_IMPL(&changeSeq, &request), FLCN_ERR_ILWALID_ARGUMENT);
//     for (LwU8 i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
//     {
//         UT_ASSERT_EQUAL_UINT32(changeSeq.pmu.syncChangeQueue[i].clientId, LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID);
//     }
// }
#endif // PERF_CHANGE_SEQ_CHANGE_QUEUE_ENQUEUE

/* ---------------- perfChangeSeqProcessPendingChange_IMPL() ---------------- */
#ifdef PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE
/*!
 * @brief Test the case where the daemon is lwrrently processing a change
 * request (or the change sequencer is locked or encoutered an error).
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ProcessPendingChangeInProgress
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the state of the CHANGESEQ to in progress
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS).
 *
 * Test Exelwtion:
 * @li Call perfChangeSeqProcessPendingChange_IMPL(). Expect the function to
 * return @ref FLCN_ERR_ILWALID_STATE.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ProcessPendingChangeInProgress,
//                 UT_CASE_SET_DESCRIPTION("Change sequencer daemon is lwrrently processing a change.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ProcessPendingChangeInProgress)
// {
//     CHANGE_SEQ changeSeq;


//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;
//     osGetPerformanceCounterMockAddEntry(0, LW_OK, 0, NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqProcessPendingChange_IMPL(&changeSeq), FLCN_ERR_ILWALID_STATE);
// }

/*!
 * @brief Test the case where no change is lwrrently queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ProcessPendingChangeNoChangeQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the state of the CHANGESEQ to idle
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Ilwalidate CHANGESEQ::pChangeNext, CHANGESEQ::pChangeForce, and
 * CHANGESEQ::pChangeLwrr.
 *
 * Test Exelwtion:
 * @li Call perfChangeSeqProcessPendingChange_IMPL(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify CHANGE::pChangeNext, CHANGESEQ::pChangeForce, and
 * CHANGESEQ::pChangeLwrr are invalid.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ProcessPendingChangeNoChangeQueued,
//                 UT_CASE_SET_DESCRIPTION("No items are queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ProcessPendingChangeNoChangeQueued)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;

//     changeSeq.changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);

//     changeSeq.pChangeForce = &forceChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);

//     changeSeq.pChangeNext = &nextChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);

//     changeSeq.pChangeLwrr = &lwrrentChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &lwrrentChange);

//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     osGetPerformanceCounterMockAddEntry(0, LW_OK, 0, NULL);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqProcessPendingChange_IMPL(&changeSeq), FLCN_OK);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&forceChange));
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&lwrrentChange));
// }

/*!
 * @brief Test the case where a normal change is lwrrently queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ProcessPendingChangeNormalChangeQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the state of the CHANGESEQ to idle
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Ilwalidate CHANGESEQ::pChangeForce, and CHANGESEQ::pChangeLwrr.
 * @li Create a valid change in CHANGESEQ::pChangeNext.
 *
 * Test Exelwtion:
 * @li Call perfChangeSeqProcessPendingChange_IMPL(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify CHANGESEQ::pChangeNext and CHANGESEQ::pChangeForce are invalid.
 * @li Verify CHANGESEQ::pChangeLwrr is valid.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ProcessPendingChangeNormalChangeQueued,
//                 UT_CASE_SET_DESCRIPTION("A normal change request is queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ProcessPendingChangeNormalChangeQueued)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;

//     changeSeq.changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);

//     changeSeq.pChangeForce = &forceChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);

//     changeSeq.pChangeNext = &nextChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);
//     nextChange.pstateIndex = 3;

//     changeSeq.pChangeLwrr = &lwrrentChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &lwrrentChange);

//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     osGetPerformanceCounterMockAddEntry(0, LW_OK, 0, NULL);

//     // for PERF_CHANGE_SEQ_PROFILE_END()
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER0, 0x00000000);
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER1, 0x00000000);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqProcessPendingChange_IMPL(&changeSeq), FLCN_OK);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&forceChange));
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
//     UT_ASSERT_TRUE(PERF_CHANGE_SEQ_IS_CHANGE_VALID(&lwrrentChange));
//     UT_ASSERT_EQUAL_UINT32(lwrrentChange.pstateIndex, 3);
// }

/*!
 * @brief Test the case where a forced change is lwrrently queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ProcessPendingChangeForcedChangeQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the state of the CHANGESEQ to idle
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Ilwalidate CHANGESEQ::pChangeNext and CHANGESEQ::pChangeLwrr.
 * @li Create a valid change in CHANGESEQ::pChangeForce.
 *
 * Test Exelwtion:
 * @li Call perfChangeSeqProcessPendingChange_IMPL(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify CHANGESEQ::pChangeNext and CHANGESEQ::pChangeForce are invalid.
 * @li Verify CHANGESEQ::pChangeLwrr is valid and matched the forced change.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ProcessPendingChangeForcedChangeQueued,
//                 UT_CASE_SET_DESCRIPTION("A forced change request is queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ProcessPendingChangeForcedChangeQueued)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;

//     changeSeq.changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);

//     changeSeq.pChangeForce = &forceChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     forceChange.pstateIndex = 7;

//     changeSeq.pChangeNext = &nextChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);

//     changeSeq.pChangeLwrr = &lwrrentChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &lwrrentChange);

//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     osGetPerformanceCounterMockAddEntry(0, LW_OK, 0, NULL);

//     // for PERF_CHANGE_SEQ_PROFILE_END()
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER0, 0x00000000);
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER1, 0x00000000);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqProcessPendingChange_IMPL(&changeSeq), FLCN_OK);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&forceChange));
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
//     UT_ASSERT_TRUE(PERF_CHANGE_SEQ_IS_CHANGE_VALID(&lwrrentChange));
//     UT_ASSERT_EQUAL_UINT32(lwrrentChange.pstateIndex, 7);
// }

/*!
 * @brief Test the case where both a forced change request and normel change
 * request are queued up.
 *
 * @verbatim
 * Test Case ID   : PMU_CHANGESEQ.ProcessPendingChangeBothChangeQueued
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup CHANGESEQ_TEST_SUITE
 *
 * Test Setup:
 * @li Initialize the global @ref CHANGESEQ data structure.
 * @li Set the state of the CHANGESEQ to idle
 * (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE).
 * @li Create a valid change in CHANGESEQ::pChangeForce and
 * CHANGESEQ::pChangeNext.
 *
 * Test Exelwtion:
 * @li Call perfChangeSeqProcessPendingChange_IMPL(). Expect the function to
 * return @ref FLCN_OK.
 * @li Verify CHANGESEQ::pChangeForce is invalid.
 * @li Verify CHANGESEQ::pChangeNext is valid.
 * @li Verify CHANGESEQ::pChangeLwrr is valid and matched the forced change.
 */
// UT_CASE_DEFINE(PMU_CHANGESEQ, ProcessPendingChangeBothChangeQueued,
//                 UT_CASE_SET_DESCRIPTION("Both types of change requests are queued.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(PMU_CHANGESEQ, ProcessPendingChangeBothChangeQueued)
// {
//     CHANGE_SEQ changeSeq;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE forceChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE nextChange;
//     LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;

//     changeSeq.changeSize = sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);

//     changeSeq.pChangeForce = &forceChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &forceChange);
//     forceChange.pstateIndex = 0xC;

//     changeSeq.pChangeNext = &nextChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &nextChange);
//     nextChange.pstateIndex = 0x3;

//     changeSeq.pChangeLwrr = &lwrrentChange;
//     PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(&changeSeq, &lwrrentChange);

//     changeSeq.pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
//     osGetPerformanceCounterMockAddEntry(0, LW_OK, 0, NULL);

//     // for PERF_CHANGE_SEQ_PROFILE_END()
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER0, 0x00000000);
//     UTF_IO_WRITE32(LW_CMSDEC_FALCON_PTIMER1, 0x00000000);

//     UT_ASSERT_EQUAL_UINT(perfChangeSeqProcessPendingChange_IMPL(&changeSeq), FLCN_OK);
//     UT_ASSERT_TRUE(!PERF_CHANGE_SEQ_IS_CHANGE_VALID(&forceChange));
//     UT_ASSERT_TRUE(PERF_CHANGE_SEQ_IS_CHANGE_VALID(&nextChange));
//     UT_ASSERT_TRUE(PERF_CHANGE_SEQ_IS_CHANGE_VALID(&lwrrentChange));
//     UT_ASSERT_EQUAL_UINT32(lwrrentChange.pstateIndex, 0xC);
// }
#endif // PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE
