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
 * @file    changeseq-mock.h
 * @brief   Data required for configuring mock changeseq interfaces.
 */

#ifndef CHANGESEQ_MOCK_H
#define CHANGESEQ_MOCK_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    LwU32 ovlIdx;
    LwU32 size;
} CALLOC_MOCK_EXPECTED_VALUE;

/*!
 * Expected values passed in by the function under test.
 */
typedef struct
{
    void               *pBuf;
    RM_FLCN_MEM_DESC   *pMemDesc;
    LwU32               offset;
    LwU32               numBytes;
} DMA_MOCK_EXPECTED_VALUE;

typedef struct
{
    LwrtosQueueHandle   queueHandle;
    void               *pItemToQueue;
    LwU32               size;
} LWRTOS_QUEUE_SEND_BLOCKING_EXPECTED_VALUE;

typedef struct
{
    FLCN_STATUS returlwalue;
} PERF_CHANGE_SEQ_INFO_GET_SUPER_MOCK_CONFIG;
extern PERF_CHANGE_SEQ_INFO_GET_SUPER_MOCK_CONFIG perfChangeSeqInfoGet_SUPER_MOCK_CONFIG;

typedef struct
{
    FLCN_STATUS status;
} PERF_CHANGE_SEQ_INFO_SET_SUPER_MOCK_CONFIG;
extern PERF_CHANGE_SEQ_INFO_SET_SUPER_MOCK_CONFIG perfChangeSeqInfoSet_SUPER_MOCK_CONFIG;

typedef struct
{
    FLCN_STATUS status;
} PERF_CHANGE_SEQ_CONSTRUCT_SUPER_MOCK_CONFIG;
extern PERF_CHANGE_SEQ_CONSTRUCT_SUPER_MOCK_CONFIG perfChangeSeqConstruct_SUPER_MOCK_CONFIG;

#define PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE_MOCK_MAX_ENTRIES             4U
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
    } entries[PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE_MOCK_MAX_ENTRIES];
} PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE_MOCK_CONFIG;
extern PERF_CHANGE_SEQ_PROCESS_PENDING_CHANGE_MOCK_CONFIG perfChangeSeqProcessPendingChange_MOCK_CONFIG;

typedef struct
{
    FLCN_STATUS status;
} PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_ENQUEUE_MOCK_CONFIG;
extern PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_ENQUEUE_MOCK_CONFIG perfChangeSeqSyncChangeQueueEnqueue_MOCK_CONFIG;

typedef struct
{
    FLCN_STATUS status;
} PERF_CHANGE_SEQ_VALIDATE_CLK_DOMAIN_INPUT_MASK_MOCK_CONFIG;
extern PERF_CHANGE_SEQ_VALIDATE_CLK_DOMAIN_INPUT_MASK_MOCK_CONFIG perfChangeSeqValidateClkDomainInputMask_MOCK_CONFIG;

void callocMockInit();
void callocMockAddEntry(LwU8 entry, void *returnPtr, const CALLOC_MOCK_EXPECTED_VALUE *pExpectedValues);
LwU8 callocMockNumCalls();

void perfChangeSeqProcessPendingChangeMockInit(void);
void perfChangeSeqProcessPendingChangeMockAddEntry(LwU8 entry, FLCN_STATUS status);
LwU8 perfChangeSeqProcessPendingChangeMockNumCalls(void);

void lwrtosQueueSendBlockingMockInit(void);
void lwrtosQueueSendBlockingMockAddEntry(LwU8 entry, FLCN_STATUS status, const LWRTOS_QUEUE_SEND_BLOCKING_EXPECTED_VALUE *pExpectedValues);
LwU8 lwrtosQueueSendBlockingMockNumCalls(void);

#endif // CHANGESEQ_MOCK_H
