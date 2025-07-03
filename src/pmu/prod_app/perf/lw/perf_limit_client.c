/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_limit_client.c
 * @copydoc Arbitration implementation file.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "dmemovl.h"
#include "pmu_objperf.h"
#include "task_perf.h"
#include "perf/perf_limit_client.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc PerfLimitsClientInit
 */
FLCN_STATUS
perfLimitsClientInit
(
    PERF_LIMITS_CLIENT **ppClient,
    LwU8                 dmemOvl,
    LwU8                 numEntries,
    LwBool               bSyncArbRequired
)
{
    PERF_LIMITS_CLIENT *pClient = NULL;
    FLCN_STATUS         status  = FLCN_OK;
    LwU8                i;

    // Sanity check
    if (0U == numEntries)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsClientInit_exit;
    }
    if (!LWOS_DMEM_OVL_VALID(dmemOvl))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsClientInit_exit;
    }

    // Allocate the client/entries
    pClient = lwosCallocType(dmemOvl, 1U, PERF_LIMITS_CLIENT);
    if (pClient == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfLimitsClientInit_exit;
    }
    pClient->pEntries = lwosCallocType(dmemOvl, numEntries, PERF_LIMITS_CLIENT_ENTRY);
    if (pClient->pEntries == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NO_FREE_MEM;
        goto perfLimitsClientInit_exit;
    }

    // Initialize the header
    status = lwrtosSemaphoreCreateBinaryGiven(&(pClient->hdr.clientSemaphore), OVL_INDEX_OS_HEAP);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsClientInit_exit;
    }

    if (bSyncArbRequired)
    {
        status = lwrtosQueueCreate(&(pClient->hdr.syncQueueHandle), 1,
                    sizeof(PERF_CHANGE_SEQ_PMU_CLIENT_COMPLETE_NOTIFICATION),
                    OVL_INDEX_OS_HEAP);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsClientInit_exit;
        }
    }
    else
    {
        pClient->hdr.syncQueueHandle = NULL;
    }

    pClient->hdr.applyFlags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE;
    pClient->hdr.dmemOvl    = dmemOvl;
    pClient->hdr.numEntries = numEntries;
    pClient->hdr.bQueued    = LW_FALSE;

    // Initialize the entries
    for (i = 0; i < pClient->hdr.numEntries; ++i)
    {
        pClient->pEntries[i].limitId = LW2080_CTRL_PERF_PERF_LIMIT_ID_NOT_SET;
        pClient->pEntries[i].clientInput.type =
            LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_DISABLED;
    }

perfLimitsClientInit_exit:
    *ppClient = pClient;
    return status;
}

/*!
 * @copydoc PerfLimitsClientEntryGet
 */
PERF_LIMITS_CLIENT_ENTRY *
perfLimitsClientEntryGet
(
    PERF_LIMITS_CLIENT            *pClient,
    LW2080_CTRL_PERF_PERF_LIMIT_ID limitId
)
{
    PERF_LIMITS_CLIENT_ENTRY *pEntry = NULL;
    LwU8                      i;

    for (i = 0; i < pClient->hdr.numEntries; ++i)
    {
        if (limitId == pClient->pEntries[i].limitId)
        {
            // Found the entry we were looking for.
            pEntry = &pClient->pEntries[i];
            break;
        }

        if ((LW2080_CTRL_PERF_PERF_LIMIT_ID_NOT_SET == pClient->pEntries[i].limitId) &&
            (pEntry == NULL))
        {
            //
            // Found the first empty entry while looking for the entry with
            // a matching limit ID. Want to hold on to this entry in case we
            // do not find a matching entry, as the caller can use this entry.
            //
            pEntry = &pClient->pEntries[i];
        }
    }

    return pEntry;
}

/*!
 * @copydoc PerfLimitsClientArbitrateAndApply
 */
FLCN_STATUS
perfLimitsClientArbitrateAndApply
(
    PERF_LIMITS_CLIENT *pClient
)
{
    PERF_CHANGE_SEQ_PMU_CLIENT_COMPLETE_NOTIFICATION notification;
    DISPATCH_PERF disp2perf;
    FLCN_STATUS   status = FLCN_OK;
    LwBool        bSync  = (pClient->hdr.applyFlags & LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC) == 0;

    // Verify that the client has control of the data semaphore
    if (osTaskIdGet() != pClient->hdr.taskOwner)
    {
        //
        // The current task is not the owner of the semaphore. This is
        // an error condition.
        //
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto perfLimitsClientArbitrateAndApply_exit;
    }

    // If synchronous arbitration request, validate parameters
    if (bSync &&
        (pClient->hdr.syncQueueHandle == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfLimitsClientArbitrateAndApply_exit;
    }

    if (!pClient->hdr.bQueued)
    {
        // Update the start time.
        osPTimerTimeNsLwrrentGet(&pClient->hdr.timeStart);

        //
        // If a command is not already sent to the PERF task, populate the
        // DISPATCH_PERF structure and send the arbitrate command
        //
        disp2perf.hdr.eventType =
            PERF_EVENT_ID_PERF_LIMITS_CLIENT;
        disp2perf.limitsClient.dmemOvl = pClient->hdr.dmemOvl;
        disp2perf.limitsClient.pClient = pClient;
        pClient->hdr.bQueued = LW_TRUE;

        lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF),
                                  &disp2perf, sizeof(disp2perf));

        if (bSync)
        {
            //
            // This is a synchronous request. Wait for the notification from
            // change sequencer that the change has completed.
            //
            // Releasing the lock to the client is required while we wait for
            // the arbitration to complete. If we do not, then arbiter will
            // not be able to access the client data to perform the actual
            // arbitration.
            //
            PERF_LIMITS_CLIENT_SEMAPHORE_GIVE(pClient);
            status = lwrtosQueueReceive(pClient->hdr.syncQueueHandle,
                                        &notification,
                                        sizeof(notification),
                                        lwrtosMAX_DELAY);
            PERF_LIMITS_CLIENT_SEMAPHORE_TAKE(pClient);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsClientArbitrateAndApply_exit;
            }
        }
    }

perfLimitsClientArbitrateAndApply_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
