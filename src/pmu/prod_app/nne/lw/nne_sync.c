/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    nne_sync.c
 * @copydoc nne_sync.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwrtos.h"

/* ------------------------- Application Includes --------------------------- */
#include "nne/nne_sync.h"
#include "objnne.h"
#include "pmu_oslayer.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/*!
 * @brief   Returns synchronization object to a consistent state on failure to
 *          receive notification of completion.
 *
 * @param[in]   pSync   @ref NNE_SYNC data structure for which to handle failure
 *
 * @return  FLCN_OK     Recovered from wait error
 * @return  FLCN_ERROR  Error recovering from error
 * @return  Others      Errors from callees
 */
static FLCN_STATUS s_nneSyncHandleWaitError(NNE_SYNC *pSync);

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc nneSyncInit
 */
FLCN_STATUS
nneSyncInit
(
    NNE_SYNC   *pSync,
    LwU8        ovlIdxDmem
)
{
    pSync->bWaitForSignal = LW_FALSE;
    pSync->pParams        = NULL;
    return lwrtosSemaphoreCreateBinaryTaken(&pSync->semaphore, ovlIdxDmem);
}

/*!
 * @copydoc nneSyncTriggerAndWait
 */
FLCN_STATUS
nneSyncTriggerAndWait
(
    NNE_SYNC                   *pSync,
    NNE_SYNC_TRIGGER_PARAMS    *pParams,
    LwUPtr                      timeout
)
{
    FLCN_STATUS status;

    pSync->pParams = pParams;
    pSync->bWaitForSignal = LW_TRUE;

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescInferenceTrigger_HAL(&Nne, pParams),
        nneSyncTriggerAndWait_exit);

    status = lwrtosSemaphoreTake(pSync->semaphore, timeout);
    if (status != FLCN_OK)
    {
        FLCN_STATUS handleErrorStatus = s_nneSyncHandleWaitError(pSync);
        if (handleErrorStatus == FLCN_OK)
        {
            status = FLCN_OK;
        }
    }

nneSyncTriggerAndWait_exit:
    return status;
}

/*!
 * @copydoc nneSyncSignal
 */
void
nneSyncSignal
(
    NNE_SYNC *pSync
)
{
    appTaskCriticalEnter();
    {
        // Now that we're in a critical section, call the FromISR implementation
        nneSyncSignalFromISR(pSync);
    }
    appTaskCriticalExit();
}

/*!
 * @copydoc nneSyncSignalFromISR
 */
void
nneSyncSignalFromISR
(
    NNE_SYNC *pSync
)
{
    if (pSync->bWaitForSignal)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
        {
            NNE_DESC_INFERENCE_PROFILE_END(pSync->pParams->pSyncTimeNs);
        }

        pSync->bWaitForSignal = LW_FALSE;
        lwrtosSemaphoreGiveFromISR(pSync->semaphore);
    }
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc s_nneSyncHandleWaitError
 */
static FLCN_STATUS
s_nneSyncHandleWaitError
(
    NNE_SYNC *pSync
)
{
    FLCN_STATUS status = FLCN_ERROR;
    LwBool bTakeSemaphore = LW_FALSE;

    appTaskCriticalEnter();
    {
        if (!pSync->bWaitForSignal)
        {
            //
            // This condition means that the completion was signalled between
            // the timeout/error return and before the critical section.
            //
            // We have to consume the signal on the semaphore so the next call
            // to trigger/wait does not consume it.
            //
            // Note that we defer actually taking it until we exit the critical
            // section.
            //
            bTakeSemaphore = LW_TRUE;
        }
        else
        {
            //
            // In this case, the timeout has expired or we've otherwise
            // experienced some error where we will not end up taking on the
            // semaphore.
            //
            // We have to:
            //  a.) Mark ourselves as no longer waiting for the signal (because
            //      the signaller has not and will not)
            //  b.) Cancel the lwrrently outstanding request so that it does not
            //      spuriously signal any future triggers/waits
            //
            pSync->bWaitForSignal = LW_FALSE;
            nneDescInferenceCancel_HAL(&Nne);
        }
    }
    appTaskCriticalExit();

    if (bTakeSemaphore)
    {
        //
        // Note: at this point, the semaphore is going to be in a totally
        // inconsistent state and this should likely be a completely fatal
        // error.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            lwrtosSemaphoreTake(pSync->semaphore, 0U),
            s_nneSyncHandleWaitError_exit);
    }

s_nneSyncHandleWaitError_exit:
    return status;
}
