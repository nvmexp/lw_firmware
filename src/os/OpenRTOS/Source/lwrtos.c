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
 * @file    lwrtos.c
 * @copydoc lwrtos.h
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ OpenRTOS Includes ------------------------------- */
#include "OpenRTOS.h"
#include "OpenRTOSFalcon.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "portmacro.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lwrtos.h"
#include "lwmisc.h"
#include "lw_rtos_extension.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
volatile LwU64 LwosUcodeVersion = LW_RM_GENERATE_LWOS_UCODE_VERSION(_OPENRTOS_V413_LW10_FALCON);

/* ------------------------ Code-in-Sync Checks ----------------------------- */
ct_assert(sizeof(portCHAR)       == sizeof(LwU8));
ct_assert(sizeof(portSHORT)      == sizeof(LwU16));
ct_assert(sizeof(portBASE_TYPE)  == sizeof(LwU32));
ct_assert(sizeof(portLONG)       == sizeof(LwU32));
ct_assert(sizeof(portSTACK_TYPE) == sizeof(LwU32));
ct_assert(sizeof(portTickType)   == sizeof(LwU32));

ct_assert(tskSTACK_FILL_WORD            == RM_FALC_STACK_FILL);
ct_assert(portBYTE_ALIGNMENT            == lwrtosBYTE_ALIGNMENT);
ct_assert(portMAX_DELAY                 == lwrtosMAX_DELAY);
ct_assert(PORT_TASK_FUNC_ARG_COLD_START == lwrtosTASK_FUNC_ARG_COLD_START);
ct_assert(PORT_TASK_FUNC_ARG_RESTART    == lwrtosTASK_FUNC_ARG_RESTART);
ct_assert(tskIDLE_PRIORITY              == lwrtosTASK_PRIORITY_IDLE);
ct_assert(configMAX_PRIORITIES          == lwrtosTASK_PRIORITY_MAX);
ct_assert(configMAX_PRIORITIES          <= LW_U8_MAX);

/* ------------------------ Public Functions -------------------------------- */
void
lwrtosYield(void)
{
    asm volatile( "trap0;" );
}

void
lwrtosTimeOffsetus
(
    LwU32 timeus
)
{
    vTaskAddTicks(timeus / LWRTOS_TICK_PERIOD_US);
}

FLCN_STATUS
lwrtosTaskSchedulerInitialize(void)
{
    // Ensure the symbol is compiled in.
    LwosUcodeVersion += 0;

    vPortInitialize();

    return FLCN_OK;
}

FLCN_STATUS
lwrtosTaskSchedulerStart(void)
{
    vTaskStartScheduler();

    return FLCN_OK;
}

LwBool
lwrtosTaskSchedulerIsStarted(void)
{
    extern volatile signed portBASE_TYPE xSchedulerRunning;

    return (xSchedulerRunning == pdTRUE);
}

void
lwrtosTaskSchedulerSuspend(void)
{
    vTaskSuspendAll();
}

void
lwrtosTaskSchedulerResume(void)
{
    (void)xTaskResumeAll();
}

FLCN_STATUS
lwrtosTaskCreate
(
    LwrtosTaskHandle                 *pTaskHandle,
    const LwrtosTaskCreateParameters *pParams
)
{
    FLCN_STATUS status = FLCN_OK;
    xTaskHandle xHandle;

    if ((pParams == NULL) || (pParams->priority >= configMAX_PRIORITIES))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else
    {
        xHandle = xTaskCreate(pParams->pTaskCode, pParams->taskID,
                              pParams->pTaskStack, pParams->stackDepth, pParams->pParameters,
                              pParams->priority, pParams->pTcbPvt);
        if (NULL != xHandle)
        {
            *pTaskHandle = (LwrtosTaskHandle)xHandle;
        }
        else
        {
            status = FLCN_ERR_NO_FREE_MEM;
        }
    }

    return status;
}

FLCN_STATUS
lwrtosTaskRestart
(
    LwrtosTaskHandle xTask
)
{
#ifdef TASK_RESTART
    vTaskRestart(xTask);
#else
    lwrtosHALT();
#endif

    return FLCN_OK;
}

#ifdef TASK_RESTART
FLCN_STATUS
lwrtosTaskStackStartGet
(
    LwrtosTaskHandle xTask,
    LwUPtr          *pxTaskStart
)
{
    FLCN_STATUS status = FLCN_ERROR;
    tskTCB *pxTCB;

    pxTCB = prvGetTCBFromHandle(xTask);

    if ((NULL != pxTCB) && (NULL != pxTaskStart))
    {
        *pxTaskStart = (LwUPtr)(pxTCB->pxStackBaseAddress + pxTCB->uxStackSizeDwords);
        status = FLCN_OK;
    }

    return status;
}
#endif

FLCN_STATUS
lwrtosTaskGetLwTcbExt
(
    LwrtosTaskHandle    xTask,
    void              **ppLwTcbExt
)
{
    tskTCB *pxTCB;

    pxTCB = prvGetTCBFromHandle(xTask);

    //
    // Cast to pointer with same size as void** to avoid casting to the
    // specific PRM_RTOS_TCB_PVT struct, because we don't want to include LwOS headers.
    //
    *((portBASE_TYPE**)ppLwTcbExt) = pxTCB->pvTcbPvt;

    return FLCN_OK;
}

LwU32
lwrtosTaskGetTickCount32(void)
{
    return xTaskGetTickCount();
}

void
lwrtosTaskCriticalEnter(void)
{
    vPortEnterCritical();
}

void
lwrtosTaskCriticalExit(void)
{
    vPortExitCritical();
}

FLCN_STATUS
lwrtosLwrrentTaskPriorityGet
(
    LwU32 *pPriority
)
{
    *pPriority = uxTaskPriorityGet(NULL);

    return FLCN_OK;
}

FLCN_STATUS
lwrtosLwrrentTaskPrioritySet
(
    LwU32 newPriority
)
{
    FLCN_STATUS status = FLCN_OK;

    if (newPriority >= configMAX_PRIORITIES)
    {
        lwrtosHALT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }
    else
    {
        vTaskPrioritySet(NULL, newPriority);
    }

    return status;
}

FLCN_STATUS
lwrtosLwrrentTaskDelayTicks
(
    LwU32 ticksToDelay
)
{
    vTaskDelay(ticksToDelay);

    return FLCN_OK;
}

LwU32
lwrtosTaskReadyTaskCountGetFromISR(void)
{
    return uxTaskReadyTaskCountGetFromISR();
}

LwU32
lwrtosTaskReadyTaskCountGet(void)
{
    LwU32 taskReadyCount;

    lwrtosTaskCriticalEnter();
    {
        taskReadyCount = lwrtosTaskReadyTaskCountGetFromISR();
    }
    lwrtosTaskCriticalExit();

    return taskReadyCount;
}

FLCN_STATUS
lwrtosQueueCreate
(
    LwrtosQueueHandle  *pxQueue,
    LwU32               queueLength,
    LwU32               itemSize,
    LwU8                ovlIdxDmem
)
{
    FLCN_STATUS status = FLCN_OK;

    *pxQueue = xQueueCreateInOvl(queueLength, itemSize, ovlIdxDmem);

    if ((NULL == *pxQueue) && (queueLength > 0))
    {
        status = FLCN_ERR_NO_FREE_MEM;
    }

    return status;
}

FLCN_STATUS
lwrtosQueueCreateOvlRes
(
    LwrtosQueueHandle  *pxQueue,
    LwU32               queueLength,
    LwU32               itemSize
)
{
    return lwrtosQueueCreate(pxQueue, queueLength, itemSize, TEMP_OVL_INDEX_OS);
}

FLCN_STATUS
lwrtosQueueReceive
(
    LwrtosQueueHandle   xQueue,
    void               *pvBuffer,
    LwU32               size,
    LwU32               ticksToWait
)
{
    FLCN_STATUS          status = FLCN_ERROR;
    signed portBASE_TYPE xReturn;

    xReturn = xQueueReceive(xQueue, pvBuffer, size, ticksToWait);

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
    LwrtosQueueHandle   xQueue,
    const void         *pvItemToQueue,
    LwU32               size,
    LwU32               ticksToWait
)
{
    FLCN_STATUS          status = FLCN_ERROR;
    signed portBASE_TYPE xReturn;

    xReturn = xQueueSend(xQueue, pvItemToQueue, size, ticksToWait);

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
    LwrtosQueueHandle   xQueue,
    const void         *pvItemToQueue,
    LwU32               size
)
{
    // The most efficient value for 'ticksToWait' is 'portMAX_DELAY'
    // This change has been made to these functions in SafeRTOS
    // To maintain past behavior, this change has not been applied to OpenRTOS
    while (FLCN_OK != lwrtosQueueSend(xQueue, pvItemToQueue, size, (portTickType)1));

    // TODO: Change once falc_halt() is removed from lwrtosQueueSend().
    return FLCN_OK;
}

void
lwrtosQueueSendFromISR
(
    LwrtosQueueHandle   xQueue,
    const void         *pvItemToQueue,
    LwU32               size
)
{
    FLCN_STATUS status;

    status = lwrtosQueueSendFromISRWithStatus(xQueue, pvItemToQueue, size);

    if (FLCN_ERR_TIMEOUT == status)
    {
        #ifdef QUEUE_HALT_ON_FULL
        {
            // TODO: Remove and assure that all use-cases properly handle errors.
            lwrtosHALT();
        }
        #endif
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
    LwrtosQueueHandle   xQueue,
    const void         *pvItemToQueue,
    LwU32               size
)
{
    extern LwBool IcCtxSwitchReq;
    signed portBASE_TYPE xReturn;
    signed portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    FLCN_STATUS          status;

    xReturn = xQueueSendFromISR(xQueue, pvItemToQueue, size, &xHigherPriorityTaskWoken);

    if (pdPASS == xReturn)
    {
        status = FLCN_OK;
        if (pdTRUE == xHigherPriorityTaskWoken)
        {
            IcCtxSwitchReq = LW_TRUE;
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
lwrtosQueueGetLwrrentNumItems
(
    LwrtosQueueHandle xQueue,
    LwU32            *pItemsWaiting
)
{
    *pItemsWaiting = uxQueueMessagesWaiting(xQueue);
    return FLCN_OK;
}

FLCN_STATUS
lwrtosQueueGetMaxNumItems
(
    LwrtosQueueHandle xQueue,
    LwU32            *pMaxNumberOfItems
)
{
    *pMaxNumberOfItems = uxQueueSize(xQueue);
    return FLCN_OK;
}

FLCN_STATUS
lwrtosSemaphoreCreateBinaryTaken
(
    LwrtosSemaphoreHandle  *pxSemaphore,
    LwU8                    ovlIdxDmem
)
{
    FLCN_STATUS status;

    status = lwrtosQueueCreate((LwrtosQueueHandle *)pxSemaphore,
                               semBINARY_SEMAPHORE_QUEUE_LENGTH,
                               semSEMAPHORE_QUEUE_ITEM_LENGTH,
                               ovlIdxDmem);
    if (FLCN_OK != status)
    {
        // TODO: Remove and assure that all use-cases properly handle errors.
        lwrtosHALT();
    }

    return status;
}

FLCN_STATUS
lwrtosSemaphoreCreateBinaryGiven
(
    LwrtosSemaphoreHandle  *pxSemaphore,
    LwU8                    ovlIdxDmem
)
{
    FLCN_STATUS status;

    status = lwrtosSemaphoreCreateBinaryTaken(pxSemaphore, ovlIdxDmem);
    if (FLCN_OK == status)
    {
        status = lwrtosSemaphoreGive(*pxSemaphore);
    }

    return status;
}

FLCN_STATUS
lwrtosSemaphoreTake
(
    LwrtosSemaphoreHandle   xSemaphore,
    LwU32                   blockTime
)
{
    FLCN_STATUS status;

    status = lwrtosQueueReceive((LwrtosQueueHandle)xSemaphore, NULL,
                                semSEMAPHORE_QUEUE_ITEM_LENGTH, blockTime);

    return status;
}

void
lwrtosSemaphoreTakeWaitForever
(
    LwrtosSemaphoreHandle   xSemaphore
)
{
    // The most efficient value for 'ticksToWait' is 'portMAX_DELAY'
    // This change has been made to these functions in SafeRTOS
    // To maintain past behavior, this change has not been applied to OpenRTOS
    while (FLCN_OK != lwrtosSemaphoreTake(xSemaphore, 100));
}

FLCN_STATUS
lwrtosSemaphoreGive
(
    LwrtosSemaphoreHandle   xSemaphore
)
{
    FLCN_STATUS status;

    status = lwrtosQueueSend((LwrtosQueueHandle)xSemaphore, NULL,
                             semSEMAPHORE_QUEUE_ITEM_LENGTH, semGIVE_BLOCK_TIME);

    return status;
}

void
lwrtosSemaphoreGiveFromISR
(
    LwrtosSemaphoreHandle   xSemaphore
)
{
    lwrtosQueueSendFromISR((LwrtosQueueHandle)xSemaphore, NULL,
                            semSEMAPHORE_QUEUE_ITEM_LENGTH);
}

void
lwrtosFlcnSetTickIntrMask
(
    LwU32 mask
)
{
    vPortFlcnSetTickIntrMask(mask);
}

void
lwrtosFlcnSetOsTickIntrClearRegAndValue
(
    LwU32 reg,
    LwU32 val
)
{
    vPortFlcnSetOsTickIntrClearRegAndValue(reg, val);
}

/* ------------------------ Private Functions ------------------------------- */

