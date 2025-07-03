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
 * @file    changeseq-mock.c
 * @brief   Mock implementations of changeseq public interfaces.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/changeseq.h"
#include "perf/changeseq-mock.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
PERF_CHANGE_SEQ_INFO_GET_SUPER_MOCK_CONFIG perfChangeSeqInfoGet_SUPER_MOCK_CONFIG;
PERF_CHANGE_SEQ_INFO_SET_SUPER_MOCK_CONFIG perfChangeSeqInfoSet_SUPER_MOCK_CONFIG;
PERF_CHANGE_SEQ_CONSTRUCT_SUPER_MOCK_CONFIG perfChangeSeqConstruct_SUPER_MOCK_CONFIG;
PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE_MOCK_CONFIG perfChangeSeqProcessPendingChange_MOCK_CONFIG;
PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_ENQUEUE_MOCK_CONFIG perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG;
PERF_CHANGE_SEQ_VALIDATE_CLK_DOMAIN_INPUT_MASK_MOCK_CONFIG perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG;

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   MOCK implementation of perfChangeSeqInfoGet_SUPER
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref PERF_CHANGE_SEQ_INFO_GET_SUPER_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq  IGNORED. Must not be NULL.
 * @param[in]   pInfo       IGNORED. Must not be NULL.
 *
 * @return      perfChangeSeqInfoGet_SUPER_MOCK_CONFIG.status.
 */
FLCN_STATUS
perfChangeSeqInfoGet_SUPER_MOCK
(
    CHANGE_SEQ                         *pChangeSeq,
    RM_PMU_PERF_CHANGE_SEQ_INFO_GET    *pInfo
)
{
    return perfChangeSeqInfoGet_SUPER_MOCK_CONFIG.returlwalue;
}

/*!
 * @brief   MOCK implementation of perfChangeSeqInfoSet_SUPER
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfChangeSeqInfoSet_SUPER_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq  IGNORED. Must not be NULL.
 * @param[in]   pInfo       IGNORED. Must not be NULL.
 *
 * @return      perfChangeSeqInfoGet_SUPER_MOCK_CONFIG.status.
 */
FLCN_STATUS
perfChangeSeqInfoSet_SUPER_MOCK
(
    CHANGE_SEQ                         *pChangeSeq,
    RM_PMU_PERF_CHANGE_SEQ_INFO_SET    *pInfo
)
{
    return perfChangeSeqInfoSet_SUPER_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of perfChangeSeqConstruct_SUPER
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfChangeSeqConstruct_SUPER_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq
 * @param[in]   pInfo
 *
 * @return      perfChangeSeqConstruct_SUPER_MOCK_CONFIG.status.
 */
FLCN_STATUS
perfChangeSeqConstruct_SUPER_MOCK
(
    CHANGE_SEQ    **ppChangeSeq,
    LwU16           size,
    LwU8            ovlIdx
)
{
    return perfChangeSeqConstruct_SUPER_MOCK_CONFIG.status;
}

/*!
 * @brief Initializes the mock configuration data for
 * perfChangeSeqProcessPendingChange().
 *
 * Zeros out the structure. Responsibility of the test to provide expected
 * values and return values prior to running tests.
 */
void perfChangeSeqProcessPendingChangeMockInit(void)
{
    memset(&perfChangeSeqProcessPendingChange_MOCK_CONFIG, 0x00, sizeof(perfChangeSeqProcessPendingChange_MOCK_CONFIG));
}

/*!
 * @brief Adds an entry to the perfChangeSeqProcessPendingChange_MOCK_CONFIG
 * data.
 *
 * If the pointer to the expected values is null, the mock function will not
 * check the expected values when the mock function is called.
 *
 * @param[in]  entry   The entry (or call number) for the test.
 * @param[in]  status  Value to return from the mock function
 */
void perfChangeSeqProcessPendingChangeMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE_MOCK_MAX_ENTRIES);
    perfChangeSeqProcessPendingChange_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Returns the number of times the function was called during a test case.
 *
 * @return the number of times the function was called.
 */
LwU8 perfChangeSeqProcessPendingChangeMockNumCalls(void)
{
    return perfChangeSeqProcessPendingChange_MOCK_CONFIG.numCalls;
}

/*!
 * @brief   MOCK implementation of perfChangeSeqProcessPendingChange
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfChangeSeqProcessPendingChange_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq
 * @param[in]   pInfo
 *
 * @return      perfChangeSeqProcessPendingChange_MOCK_CONFIG.status.
 */
FLCN_STATUS
perfChangeSeqProcessPendingChange_MOCK
(
    CHANGE_SEQ *pChangeSeq
)
{
    LwU8 entry = perfChangeSeqProcessPendingChange_MOCK_CONFIG.numCalls;
    perfChangeSeqProcessPendingChange_MOCK_CONFIG.numCalls++;

    return perfChangeSeqProcessPendingChange_MOCK_CONFIG.entries[entry].status;
}

/*!
 * @brief   MOCK implementation of perfChangeSeqSyncChangeQueueEnqueue
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq
 * @param[in]   pChangeRequest
 *
 * @return      perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG.status.
 */
FLCN_STATUS
perfChangeSeqSyncChangeQueueEnqueue_MOCK
(
    CHANGE_SEQ                                 *pChangeSeq,
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE   *pChangeRequest
)
{
    return perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of perfChangeSeqValidateClkDomainInputMask
 *
 * @details Configuration for this mock interface is controlled via values in
 *          @ref perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.
 *
 * @param[in]   pChangeSeq
 * @param[in]   pChangeRequest
 *
 * @return      perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status.
 */
FLCN_STATUS
perfChangeSeqValidateClkDomainInputMask_MOCK
(
    CHANGE_SEQ             *pChangeSeq,
    BOARDOBJGRPMASK_E32    *pClkDomainsMask
)
{
    return perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG.status;
}

/*!
 * @brief   MOCK implementation of perfChangeSeqUpdateProfilingHistogram
 *
 * @param[in,out]   pChangeSeq
 */
void
perfChangeSeqUpdateProfilingHistogram_MOCK
(
    CHANGE_SEQ *pChangeSeq
)
{
}
