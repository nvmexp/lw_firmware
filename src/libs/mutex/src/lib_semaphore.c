/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_semaphore.c
 * @copydoc lib_semaphore.h
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "lib_semaphore.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
PSEMAPHORE_RW
osSemaphoreCreateRW_IMPL
(
    LwU8 ovlIdx
)
{
    PSEMAPHORE_RW pSem = lwosCalloc(ovlIdx, sizeof(SEMAPHORE_RW));
    if (pSem == NULL)
    {
        goto osSemaphoreCreateRW_IMPL_exit;
    }

    lwrtosSemaphoreCreateBinaryGiven(&pSem->pSemRd, ovlIdx);
    if (pSem->pSemRd == NULL)
    {
        pSem = NULL;
        goto osSemaphoreCreateRW_IMPL_exit;
    }

    lwrtosSemaphoreCreateBinaryGiven(&pSem->pSemWr, ovlIdx);
    if (pSem->pSemWr == NULL)
    {
        pSem = NULL;
        goto osSemaphoreCreateRW_IMPL_exit;
    }

    lwrtosSemaphoreCreateBinaryGiven(&pSem->pSemResourceRd, ovlIdx);
    if (pSem->pSemResourceRd == NULL)
    {
        pSem = NULL;
        goto osSemaphoreCreateRW_IMPL_exit;
    }

    lwrtosSemaphoreCreateBinaryGiven(&pSem->pSemResourceWr, ovlIdx);
    if (pSem->pSemResourceWr == NULL)
    {
        pSem = NULL;
        goto osSemaphoreCreateRW_IMPL_exit;
    }

    // Init the counters.
    pSem->readCount  = 0;
    pSem->writeCount = 0;

osSemaphoreCreateRW_IMPL_exit:
    return pSem;
}

void
osSemaphoreRWTakeRD
(
    PSEMAPHORE_RW   pSem
)
{
    //
    // Set and quickly release read sub-semaphore to ensure:
    // 1. Simultaneous reading.
    // 2. No current active writers waiting or holding the resource. New readers
    //    will be blocked here from further operation to provide writer
    //    privilege.
    //
    lwrtosSemaphoreTakeWaitForever(pSem->pSemRd);
    lwrtosSemaphoreGive(pSem->pSemRd);

    lwrtosSemaphoreTakeWaitForever(pSem->pSemResourceRd);
    pSem->readCount++;
    // The first reader sets write sub-semaphore to block writing operation.
    if (pSem->readCount == 1U)
    {
        lwrtosSemaphoreTakeWaitForever(pSem->pSemWr);
    }
    lwrtosSemaphoreGive(pSem->pSemResourceRd);
}

void
osSemaphoreRWGiveRD
(
    PSEMAPHORE_RW   pSem
)
{
    lwrtosSemaphoreTakeWaitForever(pSem->pSemResourceRd);
    pSem->readCount--;
    // The last reader releases writer sub-semaphore to allow writing operation.
    if (pSem->readCount == 0U)
    {
        lwrtosSemaphoreGive(pSem->pSemWr);
    }
    lwrtosSemaphoreGive(pSem->pSemResourceRd);
}

void
osSemaphoreRWTakeWR
(
    PSEMAPHORE_RW   pSem
)
{
    lwrtosSemaphoreTakeWaitForever(pSem->pSemResourceWr);
    pSem->writeCount++;
    //
    // The first writer sets reader sub-semaphore to block further reader
    // access.
    //
    if (pSem->writeCount == 1U)
    {
        lwrtosSemaphoreTakeWaitForever(pSem->pSemRd);
    }
    lwrtosSemaphoreGive(pSem->pSemResourceWr);

    // Each writer sets writer sub-semaphore to block other writing operations.
    lwrtosSemaphoreTakeWaitForever(pSem->pSemWr);
}

void
osSemaphoreRWTakeWRCond
(
    PSEMAPHORE_RW   pSem,
    LwBool         *pBSuccess
)
{
    FLCN_STATUS status;
    (*pBSuccess) = LW_FALSE;

    // Grab writer resource semaphore.
    status = lwrtosSemaphoreTake(pSem->pSemResourceWr, 0U);
    if (status != FLCN_OK)
    {
        goto osSemaphoreRWTakeWRCond_exit;
    }

    // If writer count is non-zero, the write access is not available.
    if (pSem->writeCount != 0U)
    {
        goto osSemaphoreRWTakeWRCond_SemResourceWr_Success;
    }

    // Block readers from entry.
    status = lwrtosSemaphoreTake(pSem->pSemRd, 0U);
    if (status != FLCN_OK)
    {
        goto osSemaphoreRWTakeWRCond_SemResourceWr_Success;
    }

    // Grab write access.
    status = lwrtosSemaphoreTake(pSem->pSemWr, 0U);
    if (status != FLCN_OK)
    {
        //
        // Release reader access/entry semaphore ONLY if the writer access is
        // not granted, otherwise it will be released along with write access.
        //
        lwrtosSemaphoreGive(pSem->pSemRd);

        goto osSemaphoreRWTakeWRCond_SemResourceWr_Success;
    }

    pSem->writeCount++;
    (*pBSuccess) = LW_TRUE;

osSemaphoreRWTakeWRCond_SemResourceWr_Success:

    // Release writer resource semaphore.
    lwrtosSemaphoreGive(pSem->pSemResourceWr);

osSemaphoreRWTakeWRCond_exit:
    return;
}

void
osSemaphoreRWGiveWR
(
    PSEMAPHORE_RW   pSem
)
{
    // Release writer sub-semaphore to allow other reading or writing operation.
    lwrtosSemaphoreGive(pSem->pSemWr);

    lwrtosSemaphoreTakeWaitForever(pSem->pSemResourceWr);
    pSem->writeCount--;
    //
    // The last writer releases the read sub-semaphore to allow further reader
    // access.
    //
    if (pSem->writeCount == 0U)
    {
        lwrtosSemaphoreGive(pSem->pSemRd);
    }
    lwrtosSemaphoreGive(pSem->pSemResourceWr);
}
