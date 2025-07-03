/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file       lwoswatchdog.c
 *
 * @brief      API implementation for the Watchdog
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwoslayer.h"
#include "lwrtosHooks.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lwoswatchdog.h"

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief      Maximum taskID any lwosWatchdog* method can accept.
 *
 * @note       Since the bitmask is a 32 bit value, the maximum taskID that can
 *             fit and be mapped properly is [0-32).
 */
#define WATCHDOG_MAX_TASK_ID (32U)

/* ------------------------ External Definitions ---------------------------- */
extern void watchdogTimeViolationCallback(LwU32 failingTaskMask);

/* ------------------------ Static Variables -------------------------------- */
/*!
 * @brief      Struct pointer holding all of the tracked values of the watchdog.
 *
 * @details    Initialized during initialization based on how many tasks are
 *             actually enabled for tracking. Maintains information for all
 *             tasks except IDLE and WATCHDOG. Updated by various API calls.
 */
static WATCHDOG_TASK_STATE *pWatchdogTaskInfo;

/*!
 * @brief      Bitmask that represents the active tasks.
 *
 * @details    Initialized during initialization based on how many tasks are
 *             actually enabled for tracking. Not all tasks are enabled on every
 *             platform and so the task id is not a direct mapping to its
 *             location in the WATCHDOG_TASK_STATE struct. This mask maintains
 *             this mapping.
 *
 *
 *             Ex) 1010 => Tasks 1 and 3 are enabled,
 *
 *             => Task 1 has 0 enabled bits before and is in position 0,
 *
 *             => Task 3 has 1 enabled bit before and is in position 1.
 */
static LwU32 WatchdogActiveTaskMask = 0;

/* ------------------------ Static Function Prototypes  --------------------- */
static FLCN_STATUS s_lwosWatchdogGetTaskInfo(const LwU8 taskId, WATCHDOG_TASK_STATE **ppWatchdogTaskInfo)
    GCC_ATTRIB_SECTION("imem_resident", "s_lwosWatchdogGetTaskInfo");
static FLCN_STATUS s_lwosWatchdogStartItem(const LwU8 taskId)
    GCC_ATTRIB_SECTION("imem_watchdogClient", "s_lwosWatchdogStartItem")
    GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_lwosWatchdogCompleteItem(const LwU8 taskId)
    GCC_ATTRIB_SECTION("imem_watchdogClient", "s_lwosWatchdogCompleteItem")
    GCC_ATTRIB_NOINLINE();

/* ------------------------ Public Functions  ------------------------------- */
/*!
 * @brief      Indicate that an item has been inserted into a task's queue.
 *
 * @param[in]  taskId  The task identifier.
 *
 * @return     FLCN_OK on success,
 * @return     Error codes from lwosWatchdogAddItemFromISR.
 */
FLCN_STATUS
lwosWatchdogAddItem
(
    const LwU8 taskId
)
{
    FLCN_STATUS status;

    lwrtosENTER_CRITICAL();
    {
        status = lwosWatchdogAddItemFromISR(taskId);
    }
    lwrtosEXIT_CRITICAL();

    return status;
}

/*!
 * @brief      Indicate the specified task has a new item added to its queue.
 *
 * @param[in]  taskId  The task identifier
 *
 * @return     FLCN_OK on success,
 * @return     FLCN_ERR_ILLEGAL_OPERATION if the item count would overflow,
 * @return     Additional errors from callees.
 */
FLCN_STATUS
lwosWatchdogAddItemFromISR_IMPL
(
    const LwU8 taskId
)
{
    WATCHDOG_TASK_STATE *pLwrrentTaskInfo = NULL;
    FLCN_STATUS          status;

    status = s_lwosWatchdogGetTaskInfo(taskId, &pLwrrentTaskInfo);
    if (FLCN_OK == status)
    {
        //
        // Overflow check. We assume that no queue shall have be able to hold
        // more than 255 elements, so treat such overflow as an error.
        //
        if (pLwrrentTaskInfo->itemsInQueue == LW_U8_MAX)
        {
            lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32)FLCN_ERR_ILLEGAL_OPERATION);
            status = FLCN_ERR_ILLEGAL_OPERATION;
        }
        else
        {
            pLwrrentTaskInfo->itemsInQueue++;
        }
    }

    return status;
}

/*!
 * @brief      Resident IMEM wrapper to reduce task IMEM impact.
 *
 * @details    To reduce resident IMEM impact, the resident portion of the
 *             method just attaches and detaches the overlay that @ref
 *             s_lwosWatchdogStartItem is loaded in. All the logic is contained
 *             within @ref s_lwosWatchdogStartItem.
 *
 * @return     FLCN_OK on success,
 * @return     Error codes from s_lwosWatchdogStartItem.
 */
FLCN_STATUS
lwosWatchdogAttachAndStartItem(void)
{
    FLCN_STATUS status;
    LwU8 taskId;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, watchdogClient)
    };

    taskId = osTaskIdGet();

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = s_lwosWatchdogStartItem(taskId);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief      Resident IMEM wrapper to reduce task IMEM impact.
 *
 * @details    To reduce resident IMEM impact, the resident portion of the
 *             method just attaches and detaches the overlay that @ref
 *             s_lwosWatchdogCompleteItem is loaded in. All the logic is
 *             contained within @ref s_lwosWatchdogCompleteItem.
 *
 * @return     FLCN_OK on success,
 * @return     Error codes from s_lwosWatchdogCompleteItem.
 */
FLCN_STATUS
lwosWatchdogAttachAndCompleteItem(void)
{
    FLCN_STATUS status;
    LwU8 taskId;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, watchdogClient)
    };

    taskId = osTaskIdGet();

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = s_lwosWatchdogCompleteItem(taskId);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief         Check status of tasks monitored by the Watchdog.
 *
 * @param[in,out] pFailingTaskMask  Bitmask of task ids that have timed out.
 *
 * @return        FLCN_OK on success,
 * @return        FLCN_ERR_TIMEOUT if at least one task has exceeded its timeout,
 * @return        Additional error codes from callees.
 */
FLCN_STATUS
lwosWatchdogCheckStatus
(
    LwU32 *pFailingTaskMask
)
{
    WATCHDOG_TASK_STATE  *pLwrrentTaskInfo = NULL;
    FLCN_STATUS           status           = FLCN_OK;
    FLCN_TIMESTAMP        lwrrentTime;
    LwU8                  taskId;

    osPTimerTimeNsLwrrentGet(&lwrrentTime);

    FOR_EACH_INDEX_IN_MASK(32, taskId, WatchdogActiveTaskMask)
    {
        LwU64 deadline;
        LwBool bTaskCompleted;
        LwBool bTaskWaiting;
        FLCN_STATUS taskMonitoredStatus = FLCN_OK;

        taskMonitoredStatus = s_lwosWatchdogGetTaskInfo(taskId, &pLwrrentTaskInfo);

        if (taskMonitoredStatus != FLCN_OK)
        {
            status = taskMonitoredStatus;
            break;
        }
        else
        {
            deadline       = pLwrrentTaskInfo->lastItemStartTime + pLwrrentTaskInfo->taskTimeoutNs;
            bTaskCompleted = ((pLwrrentTaskInfo->taskStatus & LWBIT32(LWOS_WATCHDOG_ITEM_COMPLETED)) != 0U);
            bTaskWaiting   = ((pLwrrentTaskInfo->taskStatus & LWBIT32(LWOS_WATCHDOG_TASK_WAITING)) != 0U);

            if (pLwrrentTaskInfo->itemsInQueue != 0U)
            {
                if (bTaskCompleted)
                {
                    //
                    // If a task has completed its item this period but has not had a
                    // chance to start another one and is "waiting", give it a pass.
                    //
                    pLwrrentTaskInfo->taskStatus = LWBIT32(LWOS_WATCHDOG_TASK_WAITING);
                }
                else if (bTaskWaiting)
                {
                    //
                    // A task has not completed an item this period and has not started
                    // despite having items in the queue.
                    //
                    continue;
                }
                else if (lwrrentTime.data > deadline)
                {
                    //
                    // A task has missed its deadline.
                    //
                    pLwrrentTaskInfo->taskStatus |= LWBIT32(LWOS_WATCHDOG_ITEM_TIMEOUT);
                    status = FLCN_ERR_TIMEOUT;

                    if (pFailingTaskMask != NULL)
                    {
                        (*pFailingTaskMask) |= LWBIT32(taskId);
                    }
                }
                else
                {
                    //
                    // The task is in the STARTED state and has not timed out working
                    // on the item.
                    //
                    continue;
                }
            }
            else
            {
                //
                // No items in the queue - clear any warnings that it may have.
                //
                pLwrrentTaskInfo->taskStatus = LWBIT32(LWOS_WATCHDOG_TASK_WAITING);
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return status;
}

/*!
 * @brief      Set pointers to watchdog values that are initialized by the task.
 *
 * @param[in]  pTaskInfo       The pointer to watchdog task information.
 * @param[in]  activeTaskMask  The value of the watchdog active task mask.
 *
 * @return     FLCN_OK on success,
 * @return     FLCN_ERR_ILWALID_ARGUMENT if data not initalized properly.
 */
FLCN_STATUS
lwosWatchdogSetParameters
(
    WATCHDOG_TASK_STATE *pTaskInfo,
    LwU32                activeTaskMask
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // If there are no monitored tasks in the system, there is no need to
    // allocate tracking information. Future API calls will fail (safely) but
    // those can be ignored.
    //
    if ((activeTaskMask != 0U) && (pTaskInfo == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else
    {
        pWatchdogTaskInfo = pTaskInfo;
        WatchdogActiveTaskMask = activeTaskMask;
    }

    return status;
}

/*!
 * @brief         Determine a task's actual location in WATCHDOG_TASK_STATE.
 *
 * @param[in]     activeMask  The active mask.
 * @param[in]     taskId      The task identifier.
 * @param[in,out] pTaskIdx    Pointer to store the task index in the monitor
 *                            structure.
 *
 * @return        FLCN_OK on success
 * @return        FLCN_ERR_ILWALID_ARGUMENT if taskId or pTaskIdx are invalid
 * @return        FLCN_ERR_OBJECT_NOT_FOUND if the task is not monitored.
 */
FLCN_STATUS
lwosWatchdogGetTaskIndex
(
    const LwU32 activeMask,
    const LwU8  taskId,
    LwU8       *pTaskIdx
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((taskId >= WATCHDOG_MAX_TASK_ID) || (pTaskIdx == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else if ((activeMask & LWBIT32(taskId)) == 0U)
    {
        status = FLCN_ERR_OBJECT_NOT_FOUND;
    }
    else
    {
        //
        // lwMaskPos32 returns the number of bits that
        // are set to 1 in the bitmask, here activeMask,
        // starting from the provided bit position, here taskId.
        //
        *pTaskIdx = (LwU8)lwMaskPos32(activeMask, taskId);
    }

    return status;
}

/*!
 * @brief      Callback ilwoked by the ISR when the Watchdog timer fires.
 */
void
lwosWatchdogCallbackExpired_IMPL(void)
{
    lwrtosHookError(NULL, (LwS32)FLCN_ERR_TIMEOUT);
}
/* ------------------------ Private Functions  ------------------------------ */
/*!
 * @brief         Get pointer to task-specific data.
 *
 * @param[in]     taskId              The task identifier.
 * @param[in,out] ppWatchdogTaskInfo  Pointer to task info.
 *
 * @return        FLCN_OK on success
 * @return        FLCN_ERR_ILWALID_ARGUMENT if ppWatchdogTaskInfo is NULL,
 * @return        Additional error codes from callees.
 */
static FLCN_STATUS
s_lwosWatchdogGetTaskInfo
(
    const LwU8             taskId,
    WATCHDOG_TASK_STATE  **ppWatchdogTaskInfo
)
{
    FLCN_STATUS status = FLCN_OK;

    if (ppWatchdogTaskInfo == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else
    {
        LwU8 index;
        status = lwosWatchdogGetTaskIndex(WatchdogActiveTaskMask, taskId, &index);
        if (status == FLCN_OK)
        {
            *ppWatchdogTaskInfo = &(pWatchdogTaskInfo[index]);
        }
    }

    return status;
}

/*!
 * @brief      Indicate that the task has started processing an item in the
 *             queue.
 *
 * @param[in]  taskId  The task identifier.
 *
 * @return     FLCN_OK on success,
 * @return     FLCN_ERR_OBJECT_NOT_FOUND if taskId is not monitored,
 * @return     Additional error codes from callees.
 */
static FLCN_STATUS
s_lwosWatchdogStartItem
(
    const LwU8 taskId
)
{
    WATCHDOG_TASK_STATE *pLwrrentTaskInfo = NULL;
    FLCN_STATUS          status = FLCN_OK;
    FLCN_TIMESTAMP       lwrrentTime;

    status = s_lwosWatchdogGetTaskInfo(taskId, &pLwrrentTaskInfo);
    if (FLCN_OK == status)
    {
        if (pLwrrentTaskInfo->itemsInQueue != 0U)
        {
            lwrtosENTER_CRITICAL();
            {
                //
                // Clear all warnings when starting a new item.
                //
                osPTimerTimeNsLwrrentGet(&lwrrentTime);
                pLwrrentTaskInfo->lastItemStartTime = lwrrentTime.data;
                pLwrrentTaskInfo->taskStatus = LWBIT32(LWOS_WATCHDOG_ITEM_STARTED);
            }
            lwrtosEXIT_CRITICAL();
        }
        else
        {
            lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32)FLCN_ERR_ILLEGAL_OPERATION);
            status = FLCN_ERR_ILLEGAL_OPERATION;
        }
    }

    return status;
}

/*!
 * @brief      Indicate that the task has completed processing an item in the
 *             queue.
 *
 * @param[in]  taskId  The task identifier.
 *
 * @return     FLCN_OK on success,
 * @return     FLCN_ERR_ILLEGAL_OPERATION if the item count would underflow,
 * @return     FLCN_ERR_TIMEOUT if the task violated its timeout,
 * @return     Additional errors from callees.
 */
static FLCN_STATUS
s_lwosWatchdogCompleteItem
(
    const LwU8 taskId
)
{
    WATCHDOG_TASK_STATE *pLwrrentTaskInfo = NULL;
    FLCN_STATUS          status = FLCN_OK;
    FLCN_TIMESTAMP       lwrrentTime;

    status = s_lwosWatchdogGetTaskInfo(taskId, &pLwrrentTaskInfo);
    if (FLCN_OK == status)
    {
        lwrtosENTER_CRITICAL();
        {
            // Ensure there is no underflow.
            if (pLwrrentTaskInfo->itemsInQueue == 0U)
            {
                lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32)FLCN_ERR_ILLEGAL_OPERATION);
                status = FLCN_ERR_ILLEGAL_OPERATION;
            }
            else
            {
                osPTimerTimeNsLwrrentGet(&lwrrentTime);
                if ((pLwrrentTaskInfo->lastItemStartTime + pLwrrentTaskInfo->taskTimeoutNs) < lwrrentTime.data)
                {
                    // The task missed its deadline, so inform the application.
                    watchdogTimeViolationCallback(LWBIT32(taskId));
                    pLwrrentTaskInfo->taskStatus |= LWBIT32(LWOS_WATCHDOG_ITEM_TIMEOUT);
                    status = FLCN_ERR_TIMEOUT;
                }
                pLwrrentTaskInfo->itemsInQueue--;
                pLwrrentTaskInfo->taskStatus |= LWBIT32(LWOS_WATCHDOG_ITEM_COMPLETED);
            }
        }
        lwrtosEXIT_CRITICAL();
    }

    return status;
}
