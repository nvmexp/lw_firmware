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
 * @file       task_watchdog.c
 *
 * @brief      PMU Watchdog Task
 *
 * @details    The PMU Watchdog works in conjunction with lwoswatchdog.c and
 *             watchdog_tu10x.c. The Watchdog task is responsible for
 *             initializing all the tracking data for each monitored task in the
 *             system, and checking the health of all monitored tasks everytime
 *             the task is scheduled. The Watchdog task shall ensure that each
 *             monitored task completes each work item the task starts within
 *             the task's deadline. The Watchdog task does NOT ensure that a
 *             task shall start working on an item in its queue, the Watchdog
 *             task ONLY ensures that if a task starts working on an item, the
 *             task completes working on time.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwrtosHooks.h"
#include "lwoswatchdog.h"
#include "lwrtos.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_oslayer.h"
#include "pmu_objtimer.h"
#include "pmu_objpmu.h"
#include "task_watchdog.h"

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief      Access the WATCHDOG_TIMEOUT attribute from @ref g_tasks.h.
 *
 * @param      name  The name of the task.
 *
 * @return     The per-task timeout in nanoseconds.
 */
#define WATCHDOG_TASK_TIMEOUT_NS(name) (OSTASK_ATTRIBUTE_##name##_WATCHDOG_TIMEOUT_US * 1000)

/*!
 * @brief      Retrieve WATCHDOG_TRACKING_ENABLED attribute from @ref g_tasks.h.
 *
 * @param      name  The name of the task.
 *
 * @return     The per-task enablement flag value in @ref g_tasks.h.
 */
#define WATCHDOG_TASK_TRACKING_ENABLED(name) (OSTASK_ATTRIBUTE_##name##_WATCHDOG_TRACKING_ENABLED)

/* ------------------------ Global Variables -------------------------------- */
/*!
 * @brief      Task handle for the Watchdog Task. Used for task creation and
 *             scheduling.
 */
LwrtosTaskHandle OsTaskWatchdog;

/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS s_watchdogGetTaskConfig(const LwU8 taskId, LwU32 *pTimeoutNs, LwBool *pBEnabled)
    GCC_ATTRIB_SECTION("imem_init", "s_watchdogGetTaskConfig");

GCC_ATTRIB_SECTION("imem_init", "s_watchdogInitMonitor")
static FLCN_STATUS s_watchdogInitMonitor(void);

/* ------------------------- Compile Time Asserts --------------------------- */
/*!
 * @brief      The number RM_PMU_TASK_IDs must be less than or equal to @ref
 *             WatchdogActiveTaskMask (32 bits) in lwoswatchdog.c.
 *
 * @note       If the number of task IDs ever goes past 32, you MUST update the
 *             FOR_EACH loops that this variable is used in.
 */
ct_assert(RM_PMU_TASK_ID__END <= 32U);

/*!
 * @brief      s_watchdogGetTaskConfig has hardcoded Task IDs and any update to
 *             the number of tasks must be reflected there.
 */
ct_assert(RM_PMU_TASK_ID__END == 0x17U);

/* ------------------------- Public Functions ------------------------------- */
lwrtosTASK_FUNCTION(task_watchdog, pvParameters)
    GCC_ATTRIB_SECTION("imem_watchdog", "task_watchdog");

/*!
 * @brief      Initialize the WATCHDOG Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
watchdogPreInitTask_IMPL(void)
{
    FLCN_STATUS status = FLCN_OK;

    OSTASK_CREATE(status, PMU, WATCHDOG);
    if (status != FLCN_OK)
    {
        goto watchdogPreInitTask_exit;
    }

watchdogPreInitTask_exit:
    return status;
}

/*!
 * @brief      Entry point for the Watchdog task.
 *
 * @details    The Watchdog task loop infinitely loops, going to sleep for @ref
 *             WATCHDOG_TASK_PERIOD_OS_TICKS and then waking to check the status
 *             of all monitored tasks.
 * @param[in]  pvParameters  Pointer to task parameters, defined at task
 *                           creation. It is not used for the Watchdog.
 */
lwrtosTASK_FUNCTION(task_watchdog, pvParameters)
{
    // No specific INIT actions in this task, just mark it as done.
    lwosTaskInitDone();

    //
    // lwrtosLwrrentTaskDelayTicks will fail if the scheduler is suspended when
    // called. However, this should not happen as the Watchdog task is the
    // highest priority task in the system. So it cannot be runnable before
    // another task suspends the scheduler because we will switch to the
    // Watchdog task immediately. The Watchdog task itself suspends the
    // scheduler but we have an error hook if that were to fail and so it is
    // safe to have the delay here.
    //
    while (lwrtosLwrrentTaskDelayTicks(WATCHDOG_TASK_PERIOD_OS_TICKS) == FLCN_OK)
    {
        FLCN_STATUS status = FLCN_OK;
        LwU32 failingTaskMask = 0;

        status = lwosWatchdogCheckStatus(&failingTaskMask);
        if (status == FLCN_OK)
        {
            timerWatchdogPet_HAL(&Timer);
        }
        else
        {
            watchdogTimeViolationCallback(failingTaskMask);
        }
    }

    // Task loop should never terminate, raise error if it does.
    lwrtosHookError(OsTaskWatchdog, (LwS32)FLCN_ERROR);
}

/*!
 * @brief      Watchdog initializer function called from INIT task to set up
 *             watchdog.
 *
 * @return     FLCN_OK on success,
 * @return     Additional error codes from callees.
 */
FLCN_STATUS
watchdogPreInit_IMPL(void)
{
    FLCN_STATUS status;

    status = s_watchdogInitMonitor();
    if (status == FLCN_OK)
    {
        timerWatchdogInit_HAL(&Timer);
    }

    return status;
}

/*!
 * @brief      Handle monitored task(s) exceeding their per-item time limit.
 *
 * @param[in]  failingTaskMask  Bitmap where each set bit corresponds to a
 *                              failing task with a task ID equal to the bit
 *                              position.
 *
 * @note       Lwrrently the input parameter failingTaskMask is not used.
 *             However, the parameter exists if in the future the PMU has
 *             task-specific error recovery mechanisms.
 */
void
watchdogTimeViolationCallback(LwU32 failingTaskMask)
{
    lwrtosHookError(OsTaskWatchdog, (LwS32)FLCN_ERROR);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief      Initialize the Watchdog monitoring struct.
 *
 * @details    This method determines which tasks shall be monitored, creates
 *             the global array to store all the tracking information, and
 *             initializes the watchdog framework by providing it with the
 *             monitored tasks' information. At each step, validation is
 *             performed.
 *
 * @return     FLCN_OK on success,
 * @return     FLCN_ERR_NO_FREE_MEM if status struct could not be allocated,
 * @return     FLCN_ERR_ILWALID_ARGUMENT if a task has an invalid task timeout,
 * @return     Additional error codes from callees.
 */
static FLCN_STATUS
s_watchdogInitMonitor(void)
{
    LwU8                 taskId;
    LwU8                 numberOfTasks = 0;
    LwU32                taskTimeoutsNs[RM_PMU_TASK_ID__END];
    LwU32                watchdogActiveTaskMask = 0;
    LwBool               bEnabled = LW_FALSE;
    FLCN_STATUS          status;
    WATCHDOG_TASK_STATE *pWatchdogTaskInfo = NULL;

    //
    // Determine number of tasks marked for monitoring.
    // NOTE: This is independent of the number of active tasks in the build
    // since each task can individually enable/disable watchdog monitoring.
    //
    for (taskId = 0; taskId < RM_PMU_TASK_ID__END; taskId++)
    {
        status = s_watchdogGetTaskConfig(taskId, &taskTimeoutsNs[taskId], &bEnabled);
        if (status != FLCN_OK)
        {
            goto s_watchdogInitMonitor_exit;
        }
        if (bEnabled)
        {
            numberOfTasks++;
            watchdogActiveTaskMask |= LWBIT32(taskId);

            //
            // Task timeout is set to default if the provided value is 0.
            // Error will be reported if the application requires non zero less
            // than minimum allowed timeoout value.
            //
            if (taskTimeoutsNs[taskId] == 0U)
            {
                taskTimeoutsNs[taskId] = WATCHDOG_APPLICATION_DEFAULT_TIMEOUT_NS;
            }
            if (taskTimeoutsNs[taskId] < WATCHDOG_APPLICATION_MINIMUM_TIMEOUT_NS)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto s_watchdogInitMonitor_exit;
            }
        }
    }

    if (numberOfTasks > 0U)
    {
        pWatchdogTaskInfo = lwosCallocResident(sizeof(WATCHDOG_TASK_STATE) * numberOfTasks);
        if (pWatchdogTaskInfo == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            goto s_watchdogInitMonitor_exit;
        }

        FOR_EACH_INDEX_IN_MASK(32, taskId, watchdogActiveTaskMask)
        {
            LwU8 taskIndex;
            status = lwosWatchdogGetTaskIndex(watchdogActiveTaskMask, taskId, &taskIndex);
            if (status != FLCN_OK)
            {
                goto s_watchdogInitMonitor_exit;
            }
            else
            {
                pWatchdogTaskInfo[taskIndex].taskTimeoutNs     = taskTimeoutsNs[taskId];
                pWatchdogTaskInfo[taskIndex].itemsInQueue      = 0;
                pWatchdogTaskInfo[taskIndex].lastItemStartTime = 0;
                pWatchdogTaskInfo[taskIndex].taskStatus        = LWBIT(LWOS_WATCHDOG_TASK_WAITING);
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    status = lwosWatchdogSetParameters(pWatchdogTaskInfo, watchdogActiveTaskMask);

s_watchdogInitMonitor_exit:
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief         Function to retrieve the relevant task values. Reads the
 *                generated attribute at runtime using the taskId.
 *
 * @details       In Tasks.pm, each task has the attribute WATCHDOG_TIMEOUT_US
 *                and WATCHDOG_TRACKING_ENABLED. Those two attributes control
 *                how long a task can take to execute a single work item and if
 *                the task is to be monitored by the watchdog. This method
 *                accesses and returns those values for the specified task.
 *
 * @param[in]     taskId      The task identifier.
 * @param[in,out] pTimeoutNs  The timeout in milliseconds.
 * @param[in,out] pBEnabled   The status of enablement.
 *
 * @return        FLCN_OK on success,
 * @return        FLCN_ERR_OUT_OF_RANGE if taskId is greater than max,
 * @return        FLCN_ERR_NOT_SUPPORTED if this method has not been updated to
 *                support a new task,
 * @return        FLCN_ERR_ILWALID_ARGUMENT if pointers are invalid.
 */
static FLCN_STATUS
s_watchdogGetTaskConfig
(
    const LwU8  taskId,
    LwU32      *pTimeoutNs,
    LwBool     *pBEnabled
)
{
    FLCN_STATUS status = FLCN_OK;

    if (taskId >= RM_PMU_TASK_ID__END)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        goto s_watchdogGetTaskConfig_exit;
    }

    if ((pTimeoutNs == NULL) || (pBEnabled == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_watchdogGetTaskConfig_exit;
    }

    *pBEnabled = 0;
    *pTimeoutNs = 0;

    //
    // The following tasks are deprecated and will not be monitored:
    // FBBA, PCM, PERFMON, ACR, HDCP.
    //
    // The following tasks will never be monitored:
    // IDLE, WATCHDOG.
    //
    // Make sure to keep this watchdog updated with the task list.
    //
    switch (taskId)
    {
        case RM_PMU_TASK_ID_CMDMGMT:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(CMDMGMT);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(CMDMGMT);
            break;
        }
        case RM_PMU_TASK_ID_GCX:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(GCX);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(GCX);
            break;
        }
        case RM_PMU_TASK_ID_LPWR:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(LPWR);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(LPWR);
            break;
        }
        case RM_PMU_TASK_ID_LPWR_LP:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(LPWR_LP);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(LPWR_LP);
            break;
        }
        case RM_PMU_TASK_ID_I2C:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(I2C);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(I2C);
            break;
        }
        case RM_PMU_TASK_ID_SEQ:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(SEQ);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(SEQ);
            break;
        }
        case RM_PMU_TASK_ID_PCMEVT:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(PCMEVT);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(PCMEVT);
            break;
        }
        case RM_PMU_TASK_ID_PMGR:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(PMGR);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(PMGR);
            break;
        }
        case RM_PMU_TASK_ID_DISP:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(DISP);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(DISP);
            break;
        }
        case RM_PMU_TASK_ID_THERM:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(THERM);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(THERM);
            break;
        }
        case RM_PMU_TASK_ID_SPI:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(SPI);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(SPI);
            break;
        }
        case RM_PMU_TASK_ID_PERF:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(PERF);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(PERF);
            break;
        }
        case RM_PMU_TASK_ID_LOWLATENCY:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(LOWLATENCY);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(LOWLATENCY);
            break;
        }
        case RM_PMU_TASK_ID_PERF_DAEMON:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(PERF_DAEMON);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(PERF_DAEMON);
            break;
        }
        case RM_PMU_TASK_ID_BIF:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(BIF);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(BIF);
            break;
        }
        case RM_PMU_TASK_ID_PERF_CF:
        {
            *pBEnabled = WATCHDOG_TASK_TRACKING_ENABLED(PERF_CF);
            *pTimeoutNs = WATCHDOG_TASK_TIMEOUT_NS(PERF_CF);
            break;
        }
        case RM_PMU_TASK_ID__IDLE:
        case RM_PMU_TASK_ID_WATCHDOG:
        case RM_PMU_TASK_ID_PCM:
        case RM_PMU_TASK_ID_HDCP:
        case RM_PMU_TASK_ID_ACR:
        case RM_PMU_TASK_ID_PERFMON:
        case RM_PMU_TASK_ID_NNE:
        {
            // Unmonitored
            break;
        }
        default:
        {
            // Task not found
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

s_watchdogGetTaskConfig_exit:
    return status;
}
