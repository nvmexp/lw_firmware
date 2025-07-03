/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwrtos.c
 * @copydoc lwrtos.h
 */

#define KERNEL_SOURCE_FILE

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ SafeRTOS Includes ------------------------------- */
#include "SafeRTOS.h"
#ifdef TASK_RESTART
#include "SafeRTOSRiscv.h"
#endif
#include "queue.h"
#include "task.h"
#include "semaphore.h"
#include "list.h"
#include "portmacro.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lwrtos.h"
#include "lwrtosHooks.h"
#include "sections.h"

#include "lwmisc.h"
#include "lw_rtos_extension.h"
#ifdef TASK_QUEUE_MAP
#include "lwostask.h"
#include "lwoswatchdog.h"
#endif
#include "dmemovl.h"

#include "shlib/string.h"

/* ------------------------ Macros and Defines ------------------------------ */
#define TEMP_OVL_INDEX_OS ((LwU8)0)

/* Make sure this is in sync with defines in semaphore.c */
#define semBINARY_SEMAPHORE_QUEUE_LENGTH    ( ( portUnsignedBaseType ) 1 )
#define semSEMAPHORE_QUEUE_ITEM_LENGTH      ( ( portUnsignedBaseType ) 0 )
#define semINITIAL_BINARY_COUNT_GIVEN       ( ( portUnsignedBaseType ) 1 )
#define semINITIAL_BINARY_COUNT_TAKEN       ( ( portUnsignedBaseType ) 0 )
#define semINITIAL_BINARY_COUNT             semINITIAL_BINARY_COUNT_GIVEN

/*!
 * @brief   For queue IDs, determines if the ID corresponds to a task's queue
 *
 * @param[in]   id  Queue ID to check
 *
 * @return  LW_TRUE     if the queue ID corresponds to a task's queue
 * @return  LW_FALSE    if the queue ID does not correspond to a task's queue
 */
#define QUEUE_ID_IS_FOR_TASK(id) \
    ((id) <= UcodeQueueIdLastTaskQueue)

// MK TODO: that is *INSELWRE*, susceptible to ROP.
// Figure out how to selwrely track who can raise privilege.
#define WRAP_CALL_V(call) \
do { \
    LwrtosTaskPrivate *pvt = lwrtosTaskPrivateRegister; \
    if (!pvt->bIsPrivileged) \
    { \
        sysPrivilegeRaiseIntrDisabled(); \
        (call); \
        sysPrivilegeLower(); \
    } else { \
        (call); \
    } \
} while (LW_FALSE)

#define WRAP_CALL_TYPED(type, call) \
({\
    type ret; \
    LwrtosTaskPrivate *pvt = lwrtosTaskPrivateRegister; \
    if (!pvt->bIsPrivileged) \
    { \
        sysPrivilegeRaiseIntrDisabled(); \
        ret = (call); \
        sysPrivilegeLower(); \
    } else { \
        ret = (call); \
    }\
    ret; \
})

#define WRAP_CALL(call) WRAP_CALL_TYPED(FLCN_STATUS, call)

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ External Definitions ---------------------------- */

void sysTaskExit(const char *pStr, FLCN_STATUS err);
void sysPrivilegeRaiseIntrDisabled(void);
void sysPrivilegeLower(void);

#ifdef TASK_QUEUE_MAP
extern LwrtosQueueHandle UcodeQueueIdToQueueHandleMap[];
extern LwU8              UcodeQueueIdToTaskIdMap[];
extern const LwU8        UcodeQueueIdLastTaskQueue;
#endif /* TASK_QUEUE_MAP */

/* ------------------------ Function Prototypes ----------------------------- */
#ifdef TASK_QUEUE_MAP
static sysSHARED_CODE FLCN_STATUS s_lwrtosQueueIdSend(LwU8 queueID, const void *pItemToQueue, LwU32 size, LwUPtr ticksToWait);
static sysSHARED_CODE FLCN_STATUS s_lwrtosQueueIdSendBlocking(LwU8 queueID, const void *pItemToQueue, LwU32 size);
static sysSHARED_CODE FLCN_STATUS s_lwrtosQueueIdSendFromAtomic(LwU8 queueID, const void *pItemToQueue, LwU32 size);
#endif /* TASK_QUEUE_MAP */
static sysKERNEL_CODE_CREATE FLCN_STATUS s_lwrtosSemaphoreCreate(LwrtosSemaphoreHandle *pxSemaphore, LwU8 ovlIdxDmem, LwU32 maxCount, LwU32 initialCount);

/* ------------------------ Global Variables -------------------------------- */
volatile LwU64 LwosUcodeVersion =
    LW_RM_GENERATE_LWOS_UCODE_VERSION(_SAFERTOS_V8040_LW10_LWRISCV);

/* ------------------------ Code-in-Sync Checks ----------------------------- */
ct_assert(sizeof(portCharType)          == sizeof(LwU8));
ct_assert(sizeof(portUInt16Type)        == sizeof(LwU16));
ct_assert(sizeof(portTickType)          == sizeof(portBaseType));
ct_assert(sizeof(portUIntPtrType)       == sizeof(LwU64));
ct_assert(sizeof(portStackType)         == sizeof(LwU64));
ct_assert(sizeof(portBaseType)          == sizeof(LwU64));

ct_assert(portWORD_ALIGNMENT            == lwrtosBYTE_ALIGNMENT);
ct_assert(portSTACK_ALIGNMENT           >= lwrtosBYTE_ALIGNMENT);

ct_assert(portMAX_DELAY                 == lwrtosMAX_DELAY);
#ifdef TASK_RESTART
ct_assert(PORT_TASK_FUNC_ARG_COLD_START == lwrtosTASK_FUNC_ARG_COLD_START);
ct_assert(PORT_TASK_FUNC_ARG_RESTART    == lwrtosTASK_FUNC_ARG_RESTART);
#endif
ct_assert(taskIDLE_PRIORITY             == lwrtosTASK_PRIORITY_IDLE);
ct_assert(configMAX_PRIORITIES          == lwrtosTASK_PRIORITY_MAX);
ct_assert(configMAX_PRIORITIES          <= LW_U8_MAX);

ct_assert(pdTRUE                        == LW_TRUE);
ct_assert(pdFALSE                       == LW_FALSE);

/* ------------------------ Public Functions -------------------------------- */

void
lwrtosYield(void)
{
    if (!lwrtosIS_KERNEL())
    {
        syscall0(LW_SYSCALL_YIELD);
    }
    else
    {
        // We should never reach this code
        lwrtosHALT();
    }
}

// MK TODO: unused?
void
lwrtosTimeOffsetus
(
    LwU32 timeus
)
{
    //
    // This method only exists to enable compilation.
    // This method was added as a WAR for Maxwell only.
    //
    lwrtosHALT();
}

// MK TODO: unused?
#ifdef TASK_RESTART
FLCN_STATUS
lwrtosTaskStackStartGet
(
    LwrtosTaskHandle xTask,
    LwUPtr          *pxTaskStart
)
{
    FLCN_STATUS status = FLCN_ERROR;
    xTCB       *pxTCB;

    if (!lwrtosIS_KERNEL())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    pxTCB = taskGET_TCB_FROM_HANDLE(xTask);
    if ((NULL != pxTCB) && (NULL != pxTaskStart))
    {
        *pxTaskStart = (LwUPtr)pxTCB->pcStackLastAddress;
        status = FLCN_OK;
    }
    else
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    return status;
}
#endif

FLCN_STATUS
lwrtosTaskSchedulerInitialize(void)
{
    FLCN_STATUS  status;
    portBaseType xReturn;

    if (!lwrtosIS_KERNEL())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Ensure the symbol is compiled in.
    LwosUcodeVersion += 0;

    /* AM-TODO: Add actual parameters to lwrtos function */
    xReturn = xTaskInitializeScheduler(NULL);

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else
    {
        status = FLCN_ERROR;
        lwrtosBREAKPOINT();
    }

    return status;
}

FLCN_STATUS
lwrtosTaskSchedulerStart(void)
{
    FLCN_STATUS  status = FLCN_ERROR;
    portBaseType xReturn;

    if (!lwrtosIS_KERNEL())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    /* AM-TODO: Add actual parameters to lwrtos function */
    xReturn = xTaskStartScheduler(pdTRUE);

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }

    return status;
}

LwBool
lwrtosTaskSchedulerIsStarted(void)
{
    if (lwrtosIS_KERNEL())
    {
        return (pdTRUE == xTaskIsSchedulerStarted());
    }
    else
    {
        return LW_TRUE;
    }
}

void
lwrtosTaskSchedulerSuspend(void)
{
    WRAP_CALL_V(vTaskSuspendScheduler());
}

void
lwrtosTaskSchedulerResume(void)
{
    WRAP_CALL_V((void)xTaskResumeScheduler());
}

FLCN_STATUS
lwrtosTaskCreate
(
    LwrtosTaskHandle                 *pTaskHandle,
    const LwrtosTaskCreateParameters *pParams
)
{
    xTCB *pxTCB;
    xTaskParameters xTaskParams;
    portBaseType    xReturn;
    LwUPtr size;
    if (!lwrtosIS_KERNEL())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    if ((pParams == NULL) ||
        (pParams->priority >= (LwU32)configMAX_PRIORITIES) ||
        ((pParams->priority == lwrtosTASK_PRIORITY_IDLE) &&
            (pParams->taskID != lwrtosIDLE_TASK_ID)))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    size = sizeof(xTCB);

#if ( configLW_FPU_SUPPORTED == 1U )
    if (pParams->bUsingFPU)
    {
        xTaskParams.hFPUCtxOffset = size;
        size += sizeof(xPortFpuTaskContext);
    }
    else
    {
        xTaskParams.hFPUCtxOffset = 0;
    }
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

    pxTCB = (xTCB *)lwosCalloc(TEMP_OVL_INDEX_OS, size);
    if (pxTCB == NULL)
    {
        return FLCN_ERR_NO_FREE_MEM;
    }

    xTaskParams.pcStackBuffer  = (portInt8Type *)pParams->pTaskStack;
    xTaskParams.uxPriority     = pParams->priority;
    xTaskParams.pvParameters   = pParams->pParameters;
    xTaskParams.pvObject       = pParams->pTcbPvt;
    xTaskParams.pvTaskCode     = pParams->pTaskCode;
    xTaskParams.pxTCB          = pxTCB;
    xTaskParams.pxMpuInfo      = pParams->pMpuInfo;
    xTaskParams.uxTlsSize      = sizeof(LwrtosTaskPrivate);
    xTaskParams.uxTlsAlignment = __alignof(LwrtosTaskPrivate);

    lwrtosHookTaskMap(pParams->pMpuInfo);

    //
    // OpenRTOS passed stackDepth as number of portStackTypes the stack can hold.
    // SafeRTOS requires the total number of bytes.
    //
    xTaskParams.uxStackDepthBytes = ((LwU32)pParams->stackDepth) * sizeof(portStackType);

    xReturn = xTaskCreate((const xTaskParameters *)&xTaskParams, pTaskHandle);

    lwrtosHookTaskUnmap(pParams->pMpuInfo);

    if (pdPASS == xReturn)
    {
        return FLCN_OK;
    }
    else
    {
        return FLCN_ERROR;
    }
}

/*
 * Function to initialize TLS per-task/kernel structure.
 * pTls -> memory allocated at task/kernel stack
 */
void
lwrtosTlsInit(LwrtosTaskHandle pCtx, void *pTls)
{
    xTCB *pxTCB = taskGET_TCB_FROM_HANDLE(pCtx);
    LwrtosTaskPrivate *pPvt = (LwrtosTaskPrivate *)pTls;

    memset(pPvt, 0, sizeof(LwrtosTaskPrivate));
    pPvt->pHeapMspace   = NULL;
    pPvt->bIsPrivileged = LW_FALSE;
    pPvt->bIsKernel     = LW_FALSE;
    pPvt->pvObject      = pxTCB->pvObject;
    pPvt->taskHandle    = pCtx;
}

#ifdef TASK_RESTART
FLCN_STATUS
lwrtosTaskRestart
(
    LwrtosTaskHandle xTaskToRestart
)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (!lwrtosIS_KERNEL())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    if ((pdPASS != xPortIsTaskHandleValid(xTaskToRestart)) ||
        (NULL == xTaskToRestart) ||
        (xTaskToRestart == pxLwrrentTCB))
    {
        // Invalid task to restart.
    }
    else
    {
        portStackType *pxTopOfStack;
        xTCB          *pxTCB;

        pxTCB = taskGET_TCB_FROM_HANDLE(xTaskToRestart);

        //
        // The task may be in the ready list or the delayed list. Without
        // caring which one it is in, blindly remove it from whichever list it
        // belongs to. We'll add it back to the ready list right after.
        //
        vListRemove(&(pxTCB->xStateListItem));

        //
        // Restore the task priority to its default level in case it had
        // changed. Then adding it to the ready list will automatically add it
        // to the ready list belonging to its default priority level.
        //
        pxTCB->ucPriority = pxTCB->ucDefaultPriority;

        KERNEL_FUNCTION void prvAddTaskToReadyList(xTCB *pxTCB);
        prvAddTaskToReadyList(pxTCB);

        // If the task waiting on an event, remove it from there too.
        if (pxTCB->xEventListItem.pvContainer)
        {
            vListRemove(&(pxTCB->xEventListItem));
        }

        lwrtosTaskStackStartGet(xTaskToRestart, (LwUPtr*) &pxTopOfStack);

        //
        // Reinitialize the stack itself so that the next time this task is
        // scheduled, it starts exelwting from its entry function.
        // This time, place an integer on the stack that indicates to the task
        // that it is a restart so that it can do its own cleanup.
        //
        pxTCB->pxTopOfStack = pxPortInitialiseStack(pxTopOfStack, pxTCB->pvTaskCode,
                                                    PORT_TASK_FUNC_ARG_RESTART);
        pxTCB->uxTopOfStackMirror = ~((portUIntPtrType)pxTCB->pxTopOfStack);

        status = FLCN_OK;
    }

    return status;
}
#endif /* TASK_RESTART */

FLCN_STATUS
lwrtosTaskGetLwTcbExt
(
    LwrtosTaskHandle taskHandle,
    void           **ppLwTcbExt
)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (lwrtosIS_PRIV())
    {
        xTCB       *pxTCB;

        pxTCB = taskGET_TCB_FROM_HANDLE(taskHandle);
        if ((NULL != pxTCB) && (NULL != ppLwTcbExt))
        {
            //
            // Cast to pointer with same size as void** to avoid casting to the
            // specific PRM_RTOS_TCB_PVT struct, because we don't want to include LwOS headers.
            //
            *((portUIntPtrType**)ppLwTcbExt) = pxTCB->pvObject;
            status = FLCN_OK;
        }
        else
        {
            // TODO: Remove and assure that all use-cases properly handle errors.
            lwrtosHALT();
        }
    }
    else
    {
        LwrtosTaskPrivate *pPvt = lwrtosTaskPrivateRegister;

        // Normal task can only access their tcb
        if ((taskHandle == NULL) || (taskHandle == pPvt->taskHandle))
        {
            if (ppLwTcbExt != NULL)
            {
                *ppLwTcbExt = pPvt->pvObject;
                status = FLCN_OK;
            }
        }
        else
        {
            status = FLCN_ERR_ILWALID_STATE;
        }
    }

    return status;
}

LwBool
lwrtosTaskIsBlocked
(
    LwrtosTaskHandle taskHandle
)
{
    return (pdTRUE == xListIsContainedWithinAList(
        &(taskGET_TCB_FROM_HANDLE(taskHandle)->xEventListItem)));
}

LwU32
lwrtosTaskGetTickCount32(void)
{
    return WRAP_CALL_TYPED(LwU32, xTaskGetTickCount());
}

LwU64
lwrtosTaskGetTickCount64(void)
{
    return WRAP_CALL_TYPED(LwU64, xTaskGetTickCount());
}

void
lwrtosTaskCriticalEnter(void)
{
    // Critical section handling stubbed out in kernel-mode!
    if (!lwrtosIS_KERNEL())
    {
        LwrtosTaskPrivate *pPvt = lwrtosTaskPrivateRegister;

        if (pPvt->criticalNesting < LW_U8_MAX)
        {
            pPvt->criticalNesting++;

            if (pPvt->criticalNesting == 1)
            {
                syscall0(LW_SYSCALL_INTR_DISABLE);
            }
        }
        else
        {
            sysTaskExit(DEF_STRLIT("Critical nesting too deep.\n"), FLCN_ERR_ILWALID_STATE);
        }
    }
}

void
lwrtosTaskCriticalExit(void)
{
    // Critical section handling stubbed out in kernel-mode!
    if (!lwrtosIS_KERNEL())
    {
        LwrtosTaskPrivate *pPvt = lwrtosTaskPrivateRegister;

        if (pPvt->criticalNesting > 0)
        {
            pPvt->criticalNesting--;
            if (pPvt->criticalNesting == 0)
            {
                syscall0(LW_SYSCALL_INTR_ENABLE);
            }
        }
        else
        {
            sysTaskExit(DEF_STRLIT("Not in critical section.\n"), FLCN_ERR_ILWALID_STATE);
        }
    }
}

FLCN_STATUS
lwrtosLwrrentTaskPriorityGet
(
    LwU32 *pPriority
)
{
    FLCN_STATUS          status = FLCN_ERROR;
    portUnsignedBaseType xResult = 0;
    portBaseType         xReturn;

    xReturn = WRAP_CALL_TYPED(portBaseType, xTaskPriorityGet(NULL, &xResult));

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    // Cast is safe since priority should always fit into LwU32
    *pPriority = (LwU32)xResult;

    return status;
}

FLCN_STATUS
lwrtosLwrrentTaskPrioritySet
(
    LwU32 newPriority
)
{
    FLCN_STATUS  status = FLCN_ERROR;
    portBaseType xReturn;

    if (newPriority == lwrtosTASK_PRIORITY_IDLE)
    {
        // Setting a task's priority to idle is not allowed
        status = FLCN_ERR_ILLEGAL_OPERATION;
    }
    else
    {
        xReturn = WRAP_CALL_TYPED(portBaseType, xTaskPrioritySet(NULL, newPriority));

        if (pdPASS == xReturn)
        {
            status = FLCN_OK;
        }
    }

    if (status != FLCN_OK)
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    return status;
}

FLCN_STATUS
lwrtosLwrrentTaskDelayTicks
(
    LwUPtr ticksToDelay
)
{
    FLCN_STATUS  status = FLCN_ERROR;
    portBaseType xReturn;

    xReturn = WRAP_CALL_TYPED(portBaseType, xTaskDelay(ticksToDelay));

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    return status;
}

LwrtosTaskHandle
lwrtosTaskGetLwrrentTaskHandle(void)
{
    LwrtosTaskPrivate *pvt = lwrtosTaskPrivateRegister;

    if (!pvt->bIsKernel)
    {
        return pvt->taskHandle;
    } else
    {
        return xTaskGetLwrrentTaskHandle();
    }
}

LwU32
lwrtosTaskReadyTaskCountGetFromISR(void)
{
    LwU32 uxIndex;
    LwU32 uxReadyTaskCount = 0U;
    extern xList xReadyTasksLists[];

    for (uxIndex = lwrtosTASK_PRIORITY_IDLE; uxIndex < lwrtosTASK_PRIORITY_MAX; uxIndex++)
    {
        uxReadyTaskCount += listLWRRENT_LIST_LENGTH(&(xReadyTasksLists[uxIndex]));
    }

    return uxReadyTaskCount;
}

LwU32
lwrtosTaskReadyTaskCountGet(void)
{
    LwU32 taskReadyCount;

    lwrtosTaskCriticalEnter();
    {
        // MMINTS-TODO: replace with syscall (and remove critical section) ASAP!
        taskReadyCount = WRAP_CALL_TYPED(LwU32, lwrtosTaskReadyTaskCountGetFromISR());
    }
    lwrtosTaskCriticalExit();

    return taskReadyCount;
}

FLCN_STATUS
lwrtosTaskCopyToKernel
(
    void        *pDestKernel,
    const void  *pSrlwser,
    LwU32       size
)
{
    portBaseType xReturn;

    if ((pDestKernel == NULL) || (pSrlwser == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (!lwrtosIS_KERNEL())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    xReturn = portCOPY_DATA_FROM_TASK((void *) pDestKernel, (const void *) pSrlwser, size);
    if (pdPASS == xReturn)
    {
        return FLCN_OK;
    }
    else
    {
        return FLCN_ERROR;
    }
}

FLCN_STATUS
lwrtosTaskCopyFromKernel
(
    void        *pDestUser,
    const void  *pSrcKernel,
    LwU32       size
)
{
    portBaseType xReturn;

    if ((pDestUser == NULL) || (pSrcKernel == NULL))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (!lwrtosIS_KERNEL())
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    xReturn = portCOPY_DATA_TO_TASK((void *) pDestUser, (const void *) pSrcKernel, size);
    if (pdPASS == xReturn)
    {
        return FLCN_OK;
    }
    else
    {
        return FLCN_ERROR;
    }
}

FLCN_STATUS
lwrtosQueueCreate
(
    LwrtosQueueHandle  *pQueueHandle,
    LwU32               queueLength,
    LwU32               itemSize,
    LwU8                ovlIdxDmem
)
{
    FLCN_STATUS          status = FLCN_ERROR;
    portUnsignedBaseType uxBufferLength;
    portInt8Type        *pcQueueMemory;

    //
    // PMU needs to create a queue from task mode in perfLimitsClientInit,
    // to deliver PERF_CHANGE_SEQ_PMU_CLIENT_COMPLETE_NOTIFICATION messages.
    // TODO: refactor to create a queue for
    // PERF_CHANGE_SEQ_PMU_CLIENT_COMPLETE_NOTIFICATION messages in main.c.
    //
#ifndef PMU_RTOS
    if (!lwrtosIS_KERNEL())
    {
        sysTaskExit(DEF_STRLIT("lwrtosQueueCreate can't be called in task context.\n"), FLCN_ERR_ILWALID_STATE);
        return FLCN_ERR_ILWALID_STATE;
    }
#endif // PMU_RTOS

    // SafeRTOS requires an additional portQUEUE_OVERHEAD_BYTES.
    uxBufferLength = (queueLength * itemSize) + portQUEUE_OVERHEAD_BYTES;
    pcQueueMemory = (portInt8Type *)lwosCalloc(ovlIdxDmem, uxBufferLength);

    if (NULL == pcQueueMemory)
    {
        status = FLCN_ERR_NO_FREE_MEM;
    }
#ifdef QUEUE_MSG_BUF_SIZE
    else if (itemSize > QUEUE_MSG_BUF_SIZE)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
    }
#endif /* QUEUE_MSG_BUF_SIZE */
    else
    {
        portBaseType xReturn;

        xReturn = WRAP_CALL_TYPED(portBaseType, xQueueCreate(pcQueueMemory, uxBufferLength, queueLength, itemSize, pQueueHandle));

        if (pdPASS == xReturn)
        {
            status = FLCN_OK;
        }
    }

    if (FLCN_OK != status)
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    return status;
}

FLCN_STATUS
lwrtosQueueCreateOvlRes
(
    LwrtosQueueHandle  *pQueueHandle,
    LwU32               queueLength,
    LwU32               itemSize
)
{
    return lwrtosQueueCreate(pQueueHandle, queueLength, itemSize, TEMP_OVL_INDEX_OS);
}

FLCN_STATUS
lwrtosQueueReceive
(
    LwrtosQueueHandle    queueHandle,
    void                *pBuffer,
    LwU32                size,
    LwUPtr               ticksToWait
)
{
    FLCN_STATUS   status  = FLCN_ERROR;
    portBaseType  xReturn = pdFAIL;

#ifdef QUEUE_MSG_BUF_SIZE
    LwU8 itemDataBuffer[QUEUE_MSG_BUF_SIZE];

    if (size > QUEUE_MSG_BUF_SIZE)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
    }
    else
    {
        xReturn = WRAP_CALL_TYPED(portBaseType, xQueueReceive(queueHandle, itemDataBuffer, ticksToWait));
        memcpy(pBuffer, itemDataBuffer, size);
    }
#else  /* QUEUE_MSG_BUF_SIZE */
    (void) size;
    xReturn = WRAP_CALL_TYPED(portBaseType, xQueueReceive(queueHandle, pBuffer, ticksToWait));
#endif /* QUEUE_MSG_BUF_SIZE */

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else if (errQUEUE_EMPTY == xReturn)
    {
        status = FLCN_ERR_TIMEOUT;
    }
    else
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    return status;
}

FLCN_STATUS
lwrtosQueueSend
(
    LwrtosQueueHandle    queueHandle,
    const void          *pItemToQueue,
    LwU32                size,
    LwUPtr               ticksToWait
)
{
    FLCN_STATUS  status  = FLCN_ERROR;
    portBaseType xReturn = pdFAIL;

#ifdef QUEUE_MSG_BUF_SIZE
    LwU8 itemDataBuffer[QUEUE_MSG_BUF_SIZE];

    if (size > QUEUE_MSG_BUF_SIZE)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
    }
    else
    {
        pItemToQueue = memcpy(itemDataBuffer, pItemToQueue, size);
    }
#else  /* QUEUE_MSG_BUF_SIZE */
    (void) size;
#endif /* QUEUE_MSG_BUF_SIZE */

    xReturn = WRAP_CALL_TYPED(portBaseType, xQueueSend(queueHandle, pItemToQueue, ticksToWait));

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else if (errQUEUE_FULL == xReturn)
    {
        status = FLCN_ERR_TIMEOUT;
    }
    else
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    return status;
}

FLCN_STATUS
lwrtosQueueSendBlocking
(
    LwrtosQueueHandle    queueHandle,
    const void          *pItemToQueue,
    LwU32                size
)
{
    while (FLCN_OK != lwrtosQueueSend(queueHandle, pItemToQueue, size,
                                      (portTickType)portMAX_DELAY))
    {
        // NOP
    }

    // TODO: Change once lwuc_halt() is removed from lwrtosQueueSend().
    return FLCN_OK;
}

void
lwrtosQueueSendFromISR
(
    LwrtosQueueHandle   queueHandle,
    const void         *pItemToQueue,
    LwU32               size
)
{
    FLCN_STATUS status;

    status = lwrtosQueueSendFromISRWithStatus(queueHandle, pItemToQueue, size);

    if (FLCN_ERR_TIMEOUT == status)
    {
        #ifdef QUEUE_HALT_ON_FULL
        {
            // TODO: Remove and assure that all use-cases properly handle errors.
            lwrtosHALT();
        }
        #endif /* QUEUE_HALT_ON_FULL */
    }
    else if (FLCN_OK != status)
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }
}

FLCN_STATUS
lwrtosQueueSendFromISRWithStatus
(
    LwrtosQueueHandle   queueHandle,
    const void         *pItemToQueue,
    LwU32               size
)
{
    portBaseType  xHigherPriorityTaskWoken = pdFALSE;
    FLCN_STATUS   status;
    portBaseType  xReturn = pdFAIL;

#ifdef QUEUE_MSG_BUF_SIZE
    LwU8 itemDataBuffer[QUEUE_MSG_BUF_SIZE];

    if (size > QUEUE_MSG_BUF_SIZE)
    {
        return FLCN_ERR_OUT_OF_RANGE;
    }
    else
    {
        pItemToQueue = memcpy(itemDataBuffer, pItemToQueue, size);
    }
#else  /* QUEUE_MSG_BUF_SIZE */
    (void) size;
#endif /* QUEUE_MSG_BUF_SIZE */

    xReturn = xQueueSendFromISR(queueHandle, pItemToQueue, &xHigherPriorityTaskWoken);

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
        if (pdTRUE == xHigherPriorityTaskWoken)
        {
            portYIELD_IMMEDIATE();
        }
    }
    else if (errQUEUE_FULL == xReturn)
    {
        status = FLCN_ERR_TIMEOUT;
    }
    else
    {
        status = FLCN_ERROR;
    }

    return status;
}

FLCN_STATUS
lwrtosQueueSendFromAtomic
(
    LwrtosQueueHandle   queueHandle,
    const void         *pItemToQueue,
    LwU32               size
)
{
    portBaseType  xHigherPriorityTaskWoken = pdFALSE;
    FLCN_STATUS   status;
    portBaseType  xReturn = pdFAIL;

#ifdef QUEUE_MSG_BUF_SIZE
    LwU8 itemDataBuffer[QUEUE_MSG_BUF_SIZE];

    if (size > QUEUE_MSG_BUF_SIZE)
    {
        return FLCN_ERR_OUT_OF_RANGE;
    }
    else
    {
        pItemToQueue = memcpy(itemDataBuffer, pItemToQueue, size);
    }
#else  /* QUEUE_MSG_BUF_SIZE */
    (void) size;
#endif /* QUEUE_MSG_BUF_SIZE */

    // MMINTS-TODO: replace with syscall when called from task code!
    xReturn = WRAP_CALL_TYPED(portBaseType, xQueueSendFromISR(queueHandle, pItemToQueue, &xHigherPriorityTaskWoken));

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else if (errQUEUE_FULL == xReturn)
    {
        status = FLCN_ERR_TIMEOUT;
    }
    else
    {
        status = FLCN_ERROR;
    }

    return status;
}

FLCN_STATUS
lwrtosQueueGetLwrrentNumItems
(
    LwrtosQueueHandle queueHandle,
    LwU32            *pItemsWaiting
)
{
    FLCN_STATUS          status;
    portUnsignedBaseType xResult = 0;
    portBaseType         xReturn;

    xReturn = WRAP_CALL_TYPED(portBaseType, xQueueMessagesWaiting(queueHandle,
                                                                  &xResult));

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else
    {
        status = FLCN_ERROR;
    }

    // Cast is safe since size of any queue should always fit into LwU32
    *pItemsWaiting = (LwU32)xResult;

    return status;
}

#ifdef TASK_QUEUE_MAP
FLCN_STATUS
lwrtosQueueIdReceive
(
    LwU8   queueID,
    void  *pBuffer,
    LwU32  size,
    LwUPtr ticksToWait
)
{
    return lwrtosQueueReceive(UcodeQueueIdToQueueHandleMap[queueID], pBuffer, size, ticksToWait);
}


FLCN_STATUS
lwrtosQueueIdSend
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size,
    LwUPtr      ticksToWait
)
{
    return s_lwrtosQueueIdSend(queueID, pItemToQueue, size, ticksToWait);
}

FLCN_STATUS
lwrtosQueueIdSendBlocking
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size
)
{
    return s_lwrtosQueueIdSendBlocking(queueID, pItemToQueue, size);
}

void
lwrtosQueueIdSendFromISR
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size
)
{
    if (QUEUE_ID_IS_FOR_TASK(queueID))
    {
        LWOS_WATCHDOG_ADD_ITEM_FROM_ISR(UcodeQueueIdToTaskIdMap[queueID]);
    }
    lwrtosQueueSendFromISR(UcodeQueueIdToQueueHandleMap[queueID], pItemToQueue, size);
}

FLCN_STATUS
lwrtosQueueIdSendFromISRWithStatus
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size
)
{
    if (QUEUE_ID_IS_FOR_TASK(queueID))
    {
        LWOS_WATCHDOG_ADD_ITEM_FROM_ISR(UcodeQueueIdToTaskIdMap[queueID]);
    }
    return lwrtosQueueSendFromISRWithStatus(UcodeQueueIdToQueueHandleMap[queueID], pItemToQueue, size);
}

FLCN_STATUS
lwrtosQueueIdSendFromAtomic
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size
)
{
    return s_lwrtosQueueIdSendFromAtomic(queueID, pItemToQueue, size);
}
#endif /* TASK_QUEUE_MAP */

FLCN_STATUS
lwrtosSemaphoreCreateBinaryTaken
(
    LwrtosSemaphoreHandle  *pSemaphoreHandle,
    LwU8                    ovlIdxDmem
)
{
    FLCN_STATUS status;

    status = WRAP_CALL(s_lwrtosSemaphoreCreate(pSemaphoreHandle, ovlIdxDmem,
                                               semBINARY_SEMAPHORE_QUEUE_LENGTH,
                                               semINITIAL_BINARY_COUNT_TAKEN));

    return status;
}

FLCN_STATUS
lwrtosSemaphoreCreateBinaryGiven
(
    LwrtosSemaphoreHandle  *pSemaphoreHandle,
    LwU8                    ovlIdxDmem
)
{
    FLCN_STATUS status;

    status = WRAP_CALL(s_lwrtosSemaphoreCreate(pSemaphoreHandle, ovlIdxDmem,
                                               semBINARY_SEMAPHORE_QUEUE_LENGTH,
                                               semINITIAL_BINARY_COUNT_GIVEN));

    return status;
}

FLCN_STATUS
lwrtosSemaphoreTake
(
    LwrtosSemaphoreHandle semaphoreHandle,
    LwUPtr                blockTime
)
{
    FLCN_STATUS status;

    status = lwrtosQueueReceive((LwrtosQueueHandle)semaphoreHandle, NULL,
                                 semSEMAPHORE_QUEUE_ITEM_LENGTH, blockTime);

    return status;
}

void
lwrtosSemaphoreTakeWaitForever
(
    LwrtosSemaphoreHandle semaphoreHandle
)
{
    while (FLCN_OK != lwrtosSemaphoreTake(semaphoreHandle, portMAX_DELAY))
    {
        // NOP
    }
}

FLCN_STATUS
lwrtosSemaphoreGive
(
    LwrtosSemaphoreHandle semaphoreHandle
)
{
    FLCN_STATUS status;

    status = lwrtosQueueSend((LwrtosQueueHandle)semaphoreHandle, NULL,
                             semSEMAPHORE_QUEUE_ITEM_LENGTH, semSEM_GIVE_BLOCK_TIME);
    return status;
}

void
lwrtosSemaphoreGiveFromISR
(
    LwrtosSemaphoreHandle semaphoreHandle
)
{
    lwrtosQueueSendFromISR((LwrtosQueueHandle)semaphoreHandle, NULL,
                           semSEMAPHORE_QUEUE_ITEM_LENGTH);
}

#if configRTOS_USE_TASK_NOTIFICATIONS
FLCN_STATUS
lwrtosTaskNotifyWait
(
    LwUPtr  bitsToClearOnEntry,
    LwUPtr  bitsToClearOnExit,
    LwUPtr *pNotificatiolwalue,
    LwUPtr  ticksToWait
)
{
    FLCN_STATUS  status = FLCN_ERROR;
    portBaseType xReturn;

    xReturn = WRAP_CALL_TYPED(portBaseType, xTaskNotifyWait((portUnsignedBaseType)bitsToClearOnEntry,
                                                            (portUnsignedBaseType)bitsToClearOnExit,
                                                            (portUnsignedBaseType*)pNotificatiolwalue,
                                                            ticksToWait));

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else if (errNOTIFICATION_NOT_RECEIVED == xReturn)
    {
        status = FLCN_ERR_TIMEOUT;
    }
    else
    {
        lwrtosHALT();
    }

    return status;
}

FLCN_STATUS
lwrtosTaskNotifySend
(
    LwrtosTaskHandle taskToNotify,
    LwSPtr           action,
    LwUPtr           value
)
{
    FLCN_STATUS  status = FLCN_ERROR;
    portBaseType xReturn;

    xReturn = WRAP_CALL_TYPED(portBaseType, xTaskNotifySend(taskToNotify,
                                                            (portBaseType)action,
                                                            (portUnsignedBaseType)value));

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
    }
    else
    {
        lwrtosHALT();
    }

    return status;
}

FLCN_STATUS
lwrtosTaskNotifySendFromISR
(
    LwrtosTaskHandle taskToNotify,
    LwSPtr           action,
    LwUPtr           value,
    LwBool           *pbHigherPriorityTaskWoken
)
{
    FLCN_STATUS  status = FLCN_ERROR;
    portBaseType xReturn;
    portBaseType highPrioWoken;

    if (NULL == pbHigherPriorityTaskWoken)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else
    {
        xReturn = xTaskNotifySendFromISR(taskToNotify,
                                         (portBaseType)action,
                                         (portUnsignedBaseType)value,
                                         &highPrioWoken);

        if (pdPASS == xReturn)
        {
            status = FLCN_OK;
        }
        else
        {
            lwrtosHALT();
        }

        *pbHigherPriorityTaskWoken = (highPrioWoken == pdPASS);
    }

    return status;
}
#endif /* configRTOS_USE_TASK_NOTIFICATIONS */

/* ------------------------ Private Functions ------------------------------- */

#ifdef TASK_QUEUE_MAP
static FLCN_STATUS
s_lwrtosQueueIdSend
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size,
    LwUPtr      ticksToWait
)
{
    if (QUEUE_ID_IS_FOR_TASK(queueID))
    {
        LWOS_WATCHDOG_ADD_ITEM(UcodeQueueIdToTaskIdMap[queueID]);
    }
    return lwrtosQueueSend(UcodeQueueIdToQueueHandleMap[queueID], pItemToQueue, size, ticksToWait);
}

static FLCN_STATUS
s_lwrtosQueueIdSendBlocking
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size
)
{
    if (QUEUE_ID_IS_FOR_TASK(queueID))
    {
        LWOS_WATCHDOG_ADD_ITEM(UcodeQueueIdToTaskIdMap[queueID]);
    }
    return lwrtosQueueSendBlocking(UcodeQueueIdToQueueHandleMap[queueID], pItemToQueue, size);
}

static FLCN_STATUS
s_lwrtosQueueIdSendFromAtomic
(
    LwU8        queueID,
    const void *pItemToQueue,
    LwU32       size
)
{
    if (QUEUE_ID_IS_FOR_TASK(queueID))
    {
        LWOS_WATCHDOG_ADD_ITEM(UcodeQueueIdToTaskIdMap[queueID]);
    }
    return lwrtosQueueSendFromAtomic(UcodeQueueIdToQueueHandleMap[queueID], pItemToQueue, size);
}
#endif /* TASK_QUEUE_MAP */

static FLCN_STATUS
s_lwrtosSemaphoreCreate
(
    LwrtosSemaphoreHandle *pxSemaphore,
    LwU8                   ovlIdxDmem,
    LwU32                  maxCount,
    LwU32                  initialCount
)
{
    FLCN_STATUS   status = FLCN_ERROR;
    portInt8Type *pcSemaphoreBuffer;

#ifdef ON_DEMAND_PAGING_BLK
    pcSemaphoreBuffer = (portInt8Type *)lwosCallocResident(portQUEUE_OVERHEAD_BYTES);
#else /* ON_DEMAND_PAGING_BLK */
    pcSemaphoreBuffer = (portInt8Type *)lwosCalloc(ovlIdxDmem, portQUEUE_OVERHEAD_BYTES);
#endif /* ON_DEMAND_PAGING_BLK */

    if (NULL == pcSemaphoreBuffer)
    {
        status = FLCN_ERR_NO_FREE_MEM;
    }
    else
    {
        portBaseType  xReturn;

        xReturn = xSemaphoreCreateCounting(maxCount,
                                           initialCount,
                                           pcSemaphoreBuffer,
                                           pxSemaphore);
        if (pdPASS == xReturn)
        {
            status = FLCN_OK;
        }
        else
        {
            // TODO: Remove and assure that all use-cases properly handle errors.
            lwrtosHALT();
        }
    }

    return status;
}
