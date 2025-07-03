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
 * @file    lwrtos.h
 * @brief   NJ-TODO: Document.
 */

#ifndef _LWRTOS_H_
#define _LWRTOS_H_

/* ------------------------ System Includes --------------------------------- */
#include "lwctassert.h"
#include "lwtypes.h"
#include "flcnifcmn.h"
#include "falcon.h"
#include "string.h"

/* ------------------------ OpenRTOS Includes ------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
typedef void (*LwrtosTaskFunction)(void *);

typedef void  *LwrtosTaskHandle;
typedef void  *LwrtosQueueHandle;
typedef void  *LwrtosSemaphoreHandle;

typedef struct {
    LwrtosTaskFunction  pTaskCode;      // Function to run as the task.
    void               *pTaskStack;     // Task stack base
    void               *pParameters;    // Parameters passed to task function.
    void               *pTcbPvt;        // Private TCB data pointer.
    LwU32               priority;       // Task priority.
    LwU16               stackDepth;     // Task stack depth in words
    LwU8                taskID;         //
} LwrtosTaskCreateParameters;

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ External Definitions ---------------------------- */
extern void  vPortEnterCritical(void);
extern void  vPortExitCritical (void);

#ifdef FREEABLE_HEAP
extern void *pvPortMallocFreeable(LwU32 size);
extern void vPortFreeFreeable(void *pv);
#endif

#ifdef TASK_QUEUE_MAP
extern LwrtosQueueHandle UcodeQueueIdToQueueHandleMap[];
#endif /* TASK_QUEUE_MAP */

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * For now keep the behavior identical.
 */
#define lwrtosYIELD()                   lwrtosYield()
#define lwrtosRELOAD()                  lwrtosYield()

/*!
 * Must be in sync with portTASK_FUNCTION().
 */
#define lwrtosTASK_FUNCTION(_funcName, _params)                                \
    void _funcName( void *_params )

#define lwrtosENTER_CRITICAL()          vPortEnterCritical()
#define lwrtosEXIT_CRITICAL()           vPortExitCritical()

#define lwrtosBYTE_ALIGNMENT            4

#define lwrtosBYTE_ALIGN(_val)                                                 \
    (((_val) + (lwrtosBYTE_ALIGNMENT - 1)) & ~(lwrtosBYTE_ALIGNMENT - 1))

#define lwrtosMAX_DELAY                 LW_U32_MAX

#define lwrtosTASK_FUNC_ARG_COLD_START  ((void *) (0))
#define lwrtosTASK_FUNC_ARG_RESTART     ((void *) (1))

#define lwrtosTASK_PRIORITY_IDLE        0
#define lwrtosTASK_PRIORITY_MAX         8

/*!
 * Define the tick-frequency (ticks/second) for the timer used to drive the
 * scheduler. This is used to program the timer interval of the falcon's
 * general-purpose timer. The value will be divided into the falcon's clock
 * frequency to achieve a tick-period of ~30us (establishes the duration of
 * a single time-slice for a task).
 *
 * Must be kept in sync with @ref LWRTOS_TICK_PERIOD_US.
 */
#define LWRTOS_TICK_RATE_HZ             33333

/*!
 * These macros define the tick period in [us]/[ns] respectively of the falcon
 * scheduler. This approximately equals 30[us] (actually ~29.999) lwrrently.
 *
 * Must be kept in sync with @ref LWRTOS_TICK_RATE_HZ.
 */
#define LWRTOS_TICK_PERIOD_US           30
#define LWRTOS_TICK_PERIOD_NS           ( LWRTOS_TICK_PERIOD_US * 1000 )

/*!
 * @brief   Colwerts time in [us] to OS ticks, while always rounding up.
 */
#define LWRTOS_TIME_US_TO_TICKS(_timeUs)    LW_CEIL((_timeUs), LWRTOS_TICK_PERIOD_US)

/*!
 * Halts the current exelwtion of code.
 */
#define lwrtosHALT()                    falc_halt();

/*!
 * @brief Set a breakpoint recoverable by RM
 *
 * lwrtosBREAKPOINT will halt the current exelwtion of code. This is very
 * much like lwrtosHALT, but RM will try to recover from the halt state and
 * resume exelwtion. Use this if the error is not critical thus resuming will
 * unlikely cause other problems.
 */
#define lwrtosBREAKPOINT()               falc_trap1()

/*!
 * lwrtosQueueSendFromAtomic is an alias for lwrtosQueueSendFromISRWithStatus
 */
#define lwrtosQueueSendFromAtomic(xQueue, pvItemToQueue, size)                 \
    lwrtosQueueSendFromISRWithStatus(xQueue, pvItemToQueue, size)

#ifdef TASK_QUEUE_MAP
#define lwrtosQueueIdReceive(ucQueueID, pvBuffer, size, ticksToWait)                   \
    lwrtosQueueReceive(UcodeQueueIdToQueueHandleMap[ucQueueID], pvBuffer, size, ticksToWait)

#define lwrtosQueueIdSend(ucQueueID, pvItemToQueue, size, ticksToWait)                 \
    lwrtosQueueSend(UcodeQueueIdToQueueHandleMap[ucQueueID], pvItemToQueue, size, ticksToWait)

#define lwrtosQueueIdSendBlocking(ucQueueID, pvItemToQueue, size)                      \
    lwrtosQueueSendBlocking(UcodeQueueIdToQueueHandleMap[ucQueueID], pvItemToQueue, size)

#define lwrtosQueueIdSendFromISR(ucQueueID, pvItemToQueue, size)                       \
    lwrtosQueueSendFromISR(UcodeQueueIdToQueueHandleMap[ucQueueID], pvItemToQueue, size)

#define lwrtosQueueIdSendFromISRWithStatus(ucQueueID, pvItemToQueue, size)             \
    lwrtosQueueSendFromISRWithStatus(UcodeQueueIdToQueueHandleMap[ucQueueID], pvItemToQueue, size)

#define lwrtosQueueIdSendFromAtomic(ucQueueID, pvItemToQueue, size)                    \
    lwrtosQueueSendFromAtomic(UcodeQueueIdToQueueHandleMap[ucQueueID], pvItemToQueue, size)

#endif /* TASK_QUEUE_MAP */

/* ------------------------ Function Prototypes ----------------------------- */
void        lwrtosYield(void);

void        lwrtosTimeOffsetus(LwU32 timeus)
    __attribute__((section (".imem_libDi._lwrtosTimeOffsetus")));

FLCN_STATUS lwrtosTaskSchedulerInitialize(void)
    __attribute__((section (".imem_init._lwrtosTaskSchedulerInitialize")));

FLCN_STATUS lwrtosTaskSchedulerStart(void)
    __attribute__((section (".imem_init._lwrtosTaskSchedulerStart")));
LwBool      lwrtosTaskSchedulerIsStarted(void);

void        lwrtosTaskSchedulerSuspend(void);
void        lwrtosTaskSchedulerResume(void);

FLCN_STATUS lwrtosTaskCreate        (LwrtosTaskHandle *pTaskHandle, const LwrtosTaskCreateParameters *pParams)
    __attribute__((section (".imem_init._lwrtosTaskCreate")));

FLCN_STATUS lwrtosTaskStackStartGet     (LwrtosTaskHandle xTask, LwUPtr *pxTaskStart);
FLCN_STATUS lwrtosTaskRestart           (LwrtosTaskHandle xTask);
FLCN_STATUS lwrtosTaskGetLwTcbExt       (LwrtosTaskHandle xTask, void **ppLwTcbExt);
LwU32       lwrtosTaskGetTickCount32    (void);
void        lwrtosTaskCriticalEnter     (void);
void        lwrtosTaskCriticalExit      (void);
FLCN_STATUS lwrtosLwrrentTaskPriorityGet(LwU32 *pPriority);
FLCN_STATUS lwrtosLwrrentTaskPrioritySet(LwU32 newPriority);
FLCN_STATUS lwrtosLwrrentTaskDelayTicks (LwU32 ticksToDelay);

LwU32       lwrtosTaskReadyTaskCountGetFromISR (void)
    __attribute__((section (".imem_lpwr._lwrtosTaskReadyTaskCountGetFromISR")));
LwU32       lwrtosTaskReadyTaskCountGet        (void)
    __attribute__((section (".imem_lpwr._lwrtosTaskReadyTaskCountGet")));

FLCN_STATUS lwrtosQueueCreate       (LwrtosQueueHandle *pxQueue, LwU32 queueLength, LwU32 itemSize, LwU8 ovlIdxDmem)
    __attribute__((section (".imem_libInit._lwrtosQueueCreate")));
FLCN_STATUS lwrtosQueueCreateOvlRes (LwrtosQueueHandle *pxQueue, LwU32 queueLength, LwU32 itemSize)
    __attribute__((section (".imem_init._lwrtosQueueCreateOvlRes")));

FLCN_STATUS lwrtosQueueReceive               (LwrtosQueueHandle xQueue,       void *pvBuffer,      LwU32 size, LwU32 ticksToWait);
FLCN_STATUS lwrtosQueueSend                  (LwrtosQueueHandle xQueue, const void *pvItemToQueue, LwU32 size, LwU32 ticksToWait);
FLCN_STATUS lwrtosQueueSendBlocking          (LwrtosQueueHandle xQueue, const void *pvItemToQueue, LwU32 size);
void        lwrtosQueueSendFromISR           (LwrtosQueueHandle xQueue, const void *pvItemToQueue, LwU32 size);
FLCN_STATUS lwrtosQueueSendFromISRWithStatus (LwrtosQueueHandle xQueue, const void *pvItemToQueue, LwU32 size);
FLCN_STATUS lwrtosQueueGetLwrrentNumItems    (LwrtosQueueHandle xQueue,      LwU32 *pItemsWaiting);
FLCN_STATUS lwrtosQueueGetMaxNumItems        (LwrtosQueueHandle xQueue,      LwU32 *pMaxNumberOfItems);

FLCN_STATUS lwrtosSemaphoreCreateBinaryTaken(LwrtosSemaphoreHandle *pxSemaphore, LwU8 ovlIdxDmem)
    __attribute__((section (".imem_libInit._lwrtosSemaphoreCreateBinaryTaken")));
FLCN_STATUS lwrtosSemaphoreCreateBinaryGiven(LwrtosSemaphoreHandle *pxSemaphore, LwU8 ovlIdxDmem)
    __attribute__((section (".imem_libInit._lwrtosSemaphoreCreateBinaryGiven")));

FLCN_STATUS lwrtosSemaphoreTake             (LwrtosSemaphoreHandle xSemaphore, LwU32 blockTime);
void        lwrtosSemaphoreTakeWaitForever  (LwrtosSemaphoreHandle xSemaphore);
FLCN_STATUS lwrtosSemaphoreGive             (LwrtosSemaphoreHandle xSemaphore);
void        lwrtosSemaphoreGiveFromISR      (LwrtosSemaphoreHandle xSemaphore);

void        lwrtosFlcnSetTickIntrMask(LwU32 mask)
    __attribute__((section(".imem_init._lwrtosFlcnSetTickIntrMask")));

void        lwrtosFlcnSetOsTickIntrClearRegAndValue(LwU32 reg, LwU32 val)
    __attribute__((section (".imem_init._lwrtosFlcnSetOsTickIntrClearRegAndValue")));

#endif // _LWRTOS_H_

