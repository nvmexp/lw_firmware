/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwostmrcallback.c
 * @copydoc lwostmrcallback.h
 */

/* ------------------------ System includes --------------------------------- */
#include "lwrtosHooks.h"
/* ------------------------ Application includes ---------------------------- */
#include "lwostmrcallback.h"
#include "lwoslayer.h"
#include "lwostimer.h"
#include "lib_lw64.h"
#include "dmemovl.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
void  lwosHookTimerTickSpecHandler(LwU32 ticksNow);

/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Prototypes -------------------------------------- */
static LwBool
_osTmrCallbackIsBefore
(
    OS_TMR_CALLBACK     *pCallbackA,
    OS_TMR_CALLBACK     *pCallbackB,
    OS_TMR_CALLBACK_MODE mode
);
static void s_osTmrCallbackListInsert(OS_TMR_CALLBACK *pCallback);
static void s_osTmrCallbackListRemove(OS_TMR_CALLBACK *pCallback);
static void
s_osTmrCallbackNextTicksSet
(
    OS_TMR_CALLBACK *pCallback,
    LwU32            ticksBase
);
static FLCN_STATUS s_osTmrCallbackMapAdd(OS_TMR_CALLBACK *pCallback);

/* ------------------------ Global variables -------------------------------- */
/*!
 * @brief  The singular global variable that stores the centralized callback
 *         infrastructure information.
 */
OS_TMR_CALLBACK_STRUCT OsTmrCallback = { OS_TMR_CALLBACK_MODE_NORMAL, {0}, {0}};
OS_TMR_CALLBACK    **LwOsTmrCallbackMap;
OS_TMR_CB_MAP_HANDLE LwOsTmrCallbackMapHandleMax;

// Keeps track of current number of elements in callback map
OS_TMR_CB_MAP_HANDLE cbMapCount = 0;

/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Public Functions  ------------------------------- */
FLCN_STATUS
osTmrCallbackCreate_IMPL
(
    OS_TMR_CALLBACK     *pCallback,
    OS_TMR_CALLBACK_TYPE type,
    LwU8                 ovlImem,
    OsTmrCallbackFuncPtr pTmrCallbackFunc,
    LwrtosQueueHandle    queueHandle,
    LwU32                periodNormalUs,
    LwU32                periodSleepUs,
    LwBool               bRelaxedUseSleep,
    LwU8                 taskId
)
{
    FLCN_STATUS status = FLCN_OK;

    // Input validation
    if ((pCallback == NULL) || (pTmrCallbackFunc == NULL) ||
        ((periodNormalUs == OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER) &&
         (periodSleepUs  == OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER)))
    {
        OS_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto osTmrCallbackCreate_exit;
    }

    lwrtosENTER_CRITICAL();
    {
        if (pCallback->state == OS_TMR_CALLBACK_STATE_START)
        {
            pCallback->type             = type;
            pCallback->bCancelPending   = LW_FALSE;
            pCallback->ovlImem          = ovlImem;
            pCallback->pTmrCallbackFunc = pTmrCallbackFunc;
            pCallback->queueHandle      = queueHandle;
            pCallback->taskId           = taskId;

#if (OS_CALLBACKS_WSTATS)
            // Initializing callback's run-time statistics.
            memset(&pCallback->stats, 0, sizeof(pCallback->stats));
            // Tracking of the minimum must start from highest possible value.
            pCallback->stats.ticksTimeExecMin = LW_U32_MAX;
#endif // (OS_CALLBACKS_WSTATS)

            pCallback->state = OS_TMR_CALLBACK_STATE_CREATED;

            status = osTmrCallbackUpdate(pCallback, periodNormalUs, periodSleepUs, bRelaxedUseSleep,
                        OS_TIMER_UPDATE_USE_BASE_LWRRENT);
            if (status != FLCN_OK)
            {
                pCallback->state = OS_TMR_CALLBACK_STATE_ERROR;
                OS_BREAKPOINT();
                goto osTmrCallbackCreate_exitCritical;
            }

            // Add new callback to map
            status = s_osTmrCallbackMapAdd(pCallback);
            if (status != FLCN_OK)
            {
                OS_BREAKPOINT();
            }
        }
        else
        {
            status = FLCN_ERR_ILLEGAL_OPERATION;
            OS_BREAKPOINT();
        }

    }
osTmrCallbackCreate_exitCritical:
    lwrtosEXIT_CRITICAL();

osTmrCallbackCreate_exit:
    return status;
}

FLCN_STATUS
osTmrCallbackSchedule_IMPL
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status = FLCN_OK;

    // Input validation
    if (pCallback == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        OS_BREAKPOINT();
        goto osTmrCallbackSchedule_exit;
    }

    lwrtosENTER_CRITICAL();
    {
        if (pCallback->bCancelPending)
        {
            // If callback was waiting to get canceled just abort cancellation.
            pCallback->bCancelPending = LW_FALSE;
        }
        else
        {
            // Otherwise just cleanly schedule.
            if (pCallback->state == OS_TMR_CALLBACK_STATE_CREATED)
            {
                s_osTmrCallbackListInsert(pCallback);
                pCallback->state = OS_TMR_CALLBACK_STATE_SCHEDULED;
            }
            else
            {
                status = FLCN_ERR_ILLEGAL_OPERATION;
                OS_BREAKPOINT();
            }
        }
    }
    lwrtosEXIT_CRITICAL();

osTmrCallbackSchedule_exit:
    return status;
}

FLCN_STATUS
osTmrCallbackUpdate_IMPL
(
    OS_TMR_CALLBACK *pCallback,
    LwU32            periodNewNormalUs,
    LwU32            periodNewSleepUs,
    LwBool           bRelaxedUseSleep,
    LwBool           bUpdateUseLwrrent
)
{
    FLCN_STATUS status = FLCN_OK;

    // Input validation
#ifndef OS_CALLBACKS_SINGLE_MODE
    if ((pCallback == NULL) ||
        ((periodNewNormalUs == OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER) &&
         (periodNewSleepUs  == OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER)))
#else // OS_CALLBACKS_SINGLE_MODE
    if ((pCallback == NULL) ||
        ((periodNewNormalUs == OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER)))
#endif // OS_CALLBACKS_SINGLE_MODE
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        OS_BREAKPOINT();
        goto osTmrCallbackUpdate_exit;
    }

    lwrtosENTER_CRITICAL();
    {
        LwBool  bInsertBack     = LW_FALSE;
        LwBool  bNextTickUpdate = LW_TRUE;

        switch (pCallback->state)
        {
            case OS_TMR_CALLBACK_STATE_START:
                status = FLCN_ERR_ILLEGAL_OPERATION;
                break;
            case OS_TMR_CALLBACK_STATE_CREATED:
                break;
            case OS_TMR_CALLBACK_STATE_SCHEDULED:
                s_osTmrCallbackListRemove(pCallback);
                bInsertBack = LW_TRUE;
                break;
            case OS_TMR_CALLBACK_STATE_QUEUED:
                break;
            case OS_TMR_CALLBACK_STATE_EXELWTED:
                bNextTickUpdate = LW_FALSE;
                break;
            default:
                status = FLCN_ERR_ILWALID_STATE;
                pCallback->state = OS_TMR_CALLBACK_STATE_ERROR;
                break;
        }

        if (status == FLCN_OK)
        {
            LwU32 ticksBase;

            // Callwlate the previous tick base before updating period if needed
            if (!bUpdateUseLwrrent)
            {
                ticksBase = pCallback->modeData[OS_TMR_CALLBACK_MODE_NORMAL].ticksNext -
                    pCallback->modeData[OS_TMR_CALLBACK_MODE_NORMAL].ticksPeriod;
            }
            else
            {
                // Init tick base with current ticks
                ticksBase = lwrtosTaskGetTickCount32();
            }

            //
            // Note that special handling is not required for the period value of
            // OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER.
            //
            pCallback->modeData[OS_TMR_CALLBACK_MODE_NORMAL].ticksPeriod =
                LWRTOS_TIME_US_TO_TICKS(periodNewNormalUs);

#ifndef OS_CALLBACKS_SINGLE_MODE
            pCallback->modeData[OS_TMR_CALLBACK_MODE_SLEEP_ALWAYS].ticksPeriod =
                LWRTOS_TIME_US_TO_TICKS(periodNewSleepUs);
#ifdef OS_CALLBACKS_RELAXED_MODE
            if (bRelaxedUseSleep)
            {
                pCallback->modeData[OS_TMR_CALLBACK_MODE_SLEEP_RELAXED].ticksPeriod =
                    LWRTOS_TIME_US_TO_TICKS(periodNewSleepUs);
            }
            else
            {
                pCallback->modeData[OS_TMR_CALLBACK_MODE_SLEEP_RELAXED].ticksPeriod =
                    LWRTOS_TIME_US_TO_TICKS(periodNewNormalUs);
            }
#endif // OS_CALLBACKS_RELAXED_MODE
#endif // OS_CALLBACKS_SINGLE_MODE

            // update before adding to the list.
            if (bNextTickUpdate)
            {
                s_osTmrCallbackNextTicksSet(pCallback, ticksBase);
            }

            if (bInsertBack)
            {
                s_osTmrCallbackListInsert(pCallback);
            }
        }
        else
        {
            OS_BREAKPOINT();
        }
    }
    lwrtosEXIT_CRITICAL();

osTmrCallbackUpdate_exit:
    return status;
}

FLCN_STATUS
osTmrCallbackExelwte_IMPL
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status       = FLCN_OK;
    LwBool      bEarlyReturn = LW_FALSE;
    LwBool      bReschedule  = LW_TRUE;
    LwU32       ticksExpired;
    LwU32       ticksBefore;
    LwU32       ticksAfter;
    LwU32       ticksBase    = 0;

    if (pCallback == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        OS_BREAKPOINT();
        goto osTmrCallbackExelwte_exit;
    }

    // Check and change callback state.
    lwrtosENTER_CRITICAL();
    {
        // Only allow queued (expired) callback.
        if (pCallback->state == OS_TMR_CALLBACK_STATE_QUEUED)
        {
            // Allow canceled callback to reschedule.
            if (pCallback->bCancelPending)
            {
                pCallback->state = OS_TMR_CALLBACK_STATE_CREATED;
                pCallback->bCancelPending = LW_FALSE;
                bEarlyReturn = LW_TRUE;
            }
            else
            {
                pCallback->state = OS_TMR_CALLBACK_STATE_EXELWTED;
            }
        }
        else
        {
            status = FLCN_ERR_ILLEGAL_OPERATION;
            OS_BREAKPOINT();
        }
    }
    lwrtosEXIT_CRITICAL();

    if ((status != FLCN_OK) || bEarlyReturn)
    {
        goto osTmrCallbackExelwte_exit;
    }

    // Collect time stamps and execute callback.
    ticksExpired = OS_TMR_CALLBACK_TICKS_EXPIRED_GET(pCallback);

    if (!(OS_TMR_IS_BEFORE(lwrtosTaskGetTickCount32(), ticksExpired)))
    {
        // Attach an overlay (if required).
        if (pCallback->ovlImem != OVL_INDEX_ILWALID)
        {
            OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(pCallback->ovlImem);
        }

        ticksBefore = lwrtosTaskGetTickCount32();

        // Execute callback.
        status = pCallback->pTmrCallbackFunc(pCallback);
        if (status != FLCN_OK)
        {
            OS_BREAKPOINT();
            goto osTmrCallbackExelwte_exit;
        }

        ticksAfter = lwrtosTaskGetTickCount32();

        // Detach an overlay (if required).
        if (pCallback->ovlImem != OVL_INDEX_ILWALID)
        {
            OSTASK_DETACH_IMEM_OVERLAY(pCallback->ovlImem);
        }

#if (OS_CALLBACKS_WSTATS)
        // Updating callback's run-time statistics.
        {
            LwU32 ticksTimeExec;

            ticksTimeExec = ticksAfter - ticksBefore;
            // Assure that fast callbacks are tracked with non-zero time.
            ticksTimeExec = LW_MAX(ticksTimeExec, 1U);

            pCallback->stats.ticksTimeExecMin =
                LW_MIN(pCallback->stats.ticksTimeExecMin, ticksTimeExec);
            pCallback->stats.ticksTimeExecMax =
                LW_MAX(pCallback->stats.ticksTimeExecMax, ticksTimeExec);
            lw64Add32(&pCallback->stats.totalTicksTimeExelwted, ticksTimeExec);

            lw64Add32(&pCallback->stats.totalTicksTimeDrifted,
                      ticksBefore - ticksExpired);

            pCallback->stats.countExec++;
        }
#endif // (OS_CALLBACKS_WSTATS)

        // Reschedule for next exelwtion based on callback type.
        switch (pCallback->type)
        {
            case OS_TMR_CALLBACK_TYPE_ONE_SHOT:
                bReschedule = LW_FALSE;
                break;

            case OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_NO_SKIP:
            case OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED:
            {
                LwU32 ticksPeriod;
                LwU32 ticksElapsed;
                OS_TMR_CALLBACK_MODE mode;

#ifndef OS_CALLBACKS_SINGLE_MODE
                //
                // Align base to `NORMAL` period except when `NORMAL` period is
                // `OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER`.
                //
                mode =
                    (pCallback->modeData[OS_TMR_CALLBACK_MODE_NORMAL].ticksPeriod !=
                     OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER) ?
                    OS_TMR_CALLBACK_MODE_NORMAL :
                    OS_TMR_CALLBACK_MODE_SLEEP_ALWAYS;
#else // OS_CALLBACKS_SINGLE_MODE
                mode = OS_TMR_CALLBACK_MODE_NORMAL;
#endif // OS_CALLBACKS_SINGLE_MODE
                ticksPeriod = pCallback->modeData[mode].ticksPeriod;
                ticksBase   = pCallback->modeData[mode].ticksNext;

                if (pCallback->type ==
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED)
                {
                    // Push time further to the future.
                    ticksExpired = ticksAfter;
                }

                ticksElapsed = ticksExpired - ticksBase;
                ticksBase   += ticksPeriod * (ticksElapsed / ticksPeriod);
                break;
            }

            case OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_START:
                ticksBase = ticksBefore;
                break;

            case OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_ACTUAL_END:
                ticksBase = ticksAfter;
                break;

            default:
                pCallback->state = OS_TMR_CALLBACK_STATE_ERROR;
                status = FLCN_ERR_ILWALID_STATE;
                OS_BREAKPOINT();
                goto osTmrCallbackExelwte_exit;
        }

        if (bReschedule)
        {
            s_osTmrCallbackNextTicksSet(pCallback, ticksBase);
        }
    }

    //
    // Handle cancellation request received during callback's exelwtion.
    // I.e. callback can cancel itself if determines that no longer required.
    //
    lwrtosENTER_CRITICAL();
    {
        pCallback->state = OS_TMR_CALLBACK_STATE_CREATED;

        if (pCallback->bCancelPending)
        {
            pCallback->bCancelPending = LW_FALSE;
            bReschedule = LW_FALSE;
        }

        if (bReschedule)
        {
            status = osTmrCallbackSchedule(pCallback);
            if (status != FLCN_OK)
            {
                pCallback->state = OS_TMR_CALLBACK_STATE_ERROR;
                OS_BREAKPOINT();
            }
        }
    }
    lwrtosEXIT_CRITICAL();

osTmrCallbackExelwte_exit:
    return status;
}

LwBool
osTmrCallbackCancel_IMPL
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwBool bRetVal = LW_FALSE;

    if (pCallback == NULL)
    {
        bRetVal = LW_FALSE;
        OS_BREAKPOINT();
        goto osTmrCallbackCancel_exit;
    }

    lwrtosENTER_CRITICAL();
    {
        switch (pCallback->state)
        {
            case OS_TMR_CALLBACK_STATE_START:
            case OS_TMR_CALLBACK_STATE_CREATED:
            {
                // Caller should not attempt to cancel in these states.
                break;
            }
            case OS_TMR_CALLBACK_STATE_SCHEDULED:
            {
                s_osTmrCallbackListRemove(pCallback);

                // Mark _CREATED to allow subsequent schedule() (if required).
                pCallback->state = OS_TMR_CALLBACK_STATE_CREATED;
                bRetVal = LW_TRUE;
                break;
            }
            case OS_TMR_CALLBACK_STATE_QUEUED:
            case OS_TMR_CALLBACK_STATE_EXELWTED:
            {
                // Forcing callers to avoid multiple cancellations.
                if (!pCallback->bCancelPending)
                {
                    // Callback will get canceled on the next state transition.
                    pCallback->bCancelPending = LW_TRUE;
                    bRetVal = LW_TRUE;
                }
                break;
            }
            default:
                bRetVal = LW_FALSE;
                pCallback->state = OS_TMR_CALLBACK_STATE_ERROR;
                OS_BREAKPOINT();
                break;
        }
    }
    lwrtosEXIT_CRITICAL();

osTmrCallbackCancel_exit:
    return bRetVal;
}

/*!
 * @brief    Detects callback expiration and notifies the dispatcher.
 *
 * @details  This interface should be called on each OS tick. It changes the
 *           state of expired callback(s) from `SCHEDULED` to `QUEUED`.
 *           @ref osTmrCallbackExelwte is expected to be called when the task
 *           receives the notification.
 *
 * @param[in]   ticksNow    Current time in OS ticks.
 */
void
lwrtosHookTimerTick
(
    LwU32 ticksNow
)
{
    OS_TMR_CALLBACK     *pCallback;
    OS_TMR_CALLBACK     *pCallbackNext;
    OS_TMR_CALLBACK_MODE mode = OsTmrCallback.mode;

#if defined(PMU_RTOS) && defined(UPROC_RISCV)
    // MMINTS-TODO: need to extend definition of this hook to remove these checks

    //
    // Additional ucode-specific processing for the timer tick,
    // only defined for PMU right now.
    //
    lwosHookTimerTickSpecHandler(ticksNow);
#endif // defined(PMU_RTOS) && defined(UPROC_RISCV)

    // Timer has not been Inited. Not an error, just nothing to process yet.
    if (LwOsTmrCallbackMap == NULL)
    {
        return;
    }
    pCallback = LwOsTmrCallbackMap[OsTmrCallback.headHandle[mode]];

    while (pCallback != NULL)
    {
        //
        // If this callback has not expired (yet) since list is sorted none
        // after it could expire either so simply abort further processing.
        //
        if (OS_TMR_IS_BEFORE(ticksNow, pCallback->modeData[mode].ticksNext))
        {
            break;
        }

        // Cache next callback pointer since it will get lost during removal.
        pCallbackNext = LwOsTmrCallbackMap[pCallback->modeData[mode].nextHandle];

        // If we fail to notify the task we will reattempt in subsequnet tick.
        if (osTmrCallbackNotifyTaskISR(pCallback))
        {
            if (pCallback->state == OS_TMR_CALLBACK_STATE_SCHEDULED)
            {
                pCallback->state = OS_TMR_CALLBACK_STATE_QUEUED;
                pCallback->modeExpiredIn = mode;
            }
            else
            {
                pCallback->state = OS_TMR_CALLBACK_STATE_ERROR;
                OS_BREAKPOINT();
            }

            s_osTmrCallbackListRemove(pCallback);
        }
        else
        {
#if (OS_CALLBACKS_WSTATS)
            // Updating callback's run-time statistics.
            lw64Add32(&pCallback->stats.totalTicksTimeDriftedQF, 1);
#endif // (OS_CALLBACKS_WSTATS)
        }

        pCallback = pCallbackNext;
    }
}


/*!
 * @brief    Init the callback map to be populated by Create
 */
void
osTmrCallbackMapInit(void)
{
    LwOsTmrCallbackMapHandleMax = osTmrCallbackMaxCountGet();

    // Increase Max count by 1 since index[0] will be NULL pointer
    LwOsTmrCallbackMapHandleMax++;

    // Create space for the map
    if (LwOsTmrCallbackMap == NULL)
    {
        LwOsTmrCallbackMap =
            (OS_TMR_CALLBACK **)lwosCallocResident(LwOsTmrCallbackMapHandleMax *
                                                   sizeof(OS_TMR_CALLBACK *));
    }

    // Init local map count
    cbMapCount = 0;

    // Assign NULL to index 0 and increase map count by 1
    LwOsTmrCallbackMap[cbMapCount++] = NULL;
}

/* ------------------------ Static Functions  ------------------------------- */
/*!
 * @brief   Checks if calbackA expires before callbackB for given mode.
 *
 * @param[in]   pCallbackA  First callback to compare.
 * @param[in]   pCallbackB  Second callback to compare.
 * @param[in]   mode        ID of the mode in which to compare callbacks.
 *
 * @return  LW_TRUE if first callback is before second, LW_FALSE otherwise.
 */
static LwBool
_osTmrCallbackIsBefore
(
    OS_TMR_CALLBACK     *pCallbackA,
    OS_TMR_CALLBACK     *pCallbackB,
    OS_TMR_CALLBACK_MODE mode
)
{
    if (pCallbackB == NULL)
    {
        return LW_TRUE;
    }

    return OS_TMR_IS_BEFORE(pCallbackA->modeData[mode].ticksNext,
                            pCallbackB->modeData[mode].ticksNext);
}

/*!
 * @brief   Inserts callback into doubly linked lists of all supported modes.
 *
 * Insertion preserves existing sorting order (by increasing expiration time).
 *
 * @param[in]   pCallback   Callback node to insert.
 *
 * @pre     Must be called within the critical section !!!
 * @pre     Caller must assure that the callback is not in the list !!!
 */
static void
s_osTmrCallbackListInsert
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwU32 mode;

    for (mode = 0; mode < OS_TMR_CALLBACK_MODE_COUNT; mode++)
    {
        OS_TMR_CALLBACK *pPrev = NULL;
        OS_TMR_CALLBACK *pNext = LwOsTmrCallbackMap[OsTmrCallback.headHandle[mode]];

        // Callback that never expires is not added into the doubly linked list.
        if (OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER ==
            pCallback->modeData[mode].ticksPeriod)
        {
            return;
        }

        while (!_osTmrCallbackIsBefore(pCallback, pNext, mode))
        {
            pPrev = pNext;
            pNext = LwOsTmrCallbackMap[pNext->modeData[mode].nextHandle];
        }

        // Callback is not in the list => isolated => both pointers are NULL.

        if (pNext != NULL)
        {
            pCallback->modeData[mode].nextHandle = pNext->mapIdx;
            pNext->modeData[mode].prevHandle = pCallback->mapIdx;
        }
        else
        {
            // Adding to the end of the list => nothing to do.
        }

        if (pPrev != NULL)
        {
            pCallback->modeData[mode].prevHandle = pPrev->mapIdx;
            pPrev->modeData[mode].nextHandle = pCallback->mapIdx;
        }
        else
        {
            // Adding to the beginning of the list => update head pointer.
            OsTmrCallback.headHandle[mode] = pCallback->mapIdx;
        }
    }
}

/*!
 * @brief   Removes callback from doubly linked lists of all supported modes.
 *
 * @param[in]   pCallback   Callback node to remove.
 *
 * @pre     Must be called within the critical section !!!
 * @pre     Caller must assure that the callback is within the list !!!
 */
static void
s_osTmrCallbackListRemove
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwU32 mode;

    for (mode = 0; mode < OS_TMR_CALLBACK_MODE_COUNT; mode++)
    {
        OS_TMR_CALLBACK_MODE_DATA *pModeData = &pCallback->modeData[mode];

        // Callback that never expires is not in the doubly linked list.
        if (OS_TMR_CALLBACK_PERIOD_EXPIRE_NEVER ==
            pCallback->modeData[mode].ticksPeriod)
        {
            return;
        }

        if (pModeData->nextHandle != CB_MAP_HANDLE_NULL)
        {
            LwOsTmrCallbackMap[pModeData->nextHandle]->modeData[mode].prevHandle = pModeData->prevHandle;
        }
        if (pModeData->prevHandle != CB_MAP_HANDLE_NULL)
        {
            LwOsTmrCallbackMap[pModeData->prevHandle]->modeData[mode].nextHandle = pModeData->nextHandle;
        }
        else
        {
            // In case that the callback is not in the list this corrupts head.
            OsTmrCallback.headHandle[mode] = pModeData->nextHandle;
        }
        pModeData->prevHandle = CB_MAP_HANDLE_NULL;
        pModeData->nextHandle = CB_MAP_HANDLE_NULL;
    }
}

/*!
 * @brief   Updates expiration times for all modes relative to provided base.
 *
 * @param[in]   pCallback   Callback to update.
 * @param[in]   ticksBase   Base-time (in OS ticks).
 *
 * @pre     Caller must assure that the callback is not in the lists !!!
 *          This guarantees that schedule() & dispatch() will not fail.
 */
static void
s_osTmrCallbackNextTicksSet
(
    OS_TMR_CALLBACK *pCallback,
    LwU32            ticksBase
)
{
    LwU32 mode;

    for (mode = 0; mode < OS_TMR_CALLBACK_MODE_COUNT; mode++)
    {
        pCallback->modeData[mode].ticksNext =
            ticksBase + pCallback->modeData[mode].ticksPeriod;
    }
}

/*!
 * @brief   Adds new callback to map
 *
 * @param[in]   pCallback   Callback to add
 *
 * @return  FLCN_OK if callback is added to map successfully
 *          FLCN_ERR_NO_FREE_MEM if max callbacks has been added to map
 */
static FLCN_STATUS
s_osTmrCallbackMapAdd
(
    OS_TMR_CALLBACK *pCallback
)
{
    OS_TMR_CB_MAP_HANDLE i;

    //
    // Check if max callbacks has reached. If this error is hit,
    // the max size needs to be increased accordingly in
    // osTmrCallbackMaxCountGet. This is most likely to happen if
    // a new callback is added without making changes to MaxCountGet.
    //
    if (cbMapCount == LwOsTmrCallbackMapHandleMax)
    {
        return FLCN_ERR_NO_FREE_MEM;
    }

    // Check if an entry has already been made in the map
    for (i = 1; i < cbMapCount ; i++)
    {
        if (pCallback == LwOsTmrCallbackMap[i])
        {
            break;
        }
    }

    pCallback->mapIdx = i;

    // If no previous entry exists, add to map
    if (i == cbMapCount)
    {
        LwOsTmrCallbackMap[i] = pCallback;
        cbMapCount++;
    }

    return FLCN_OK;
}
