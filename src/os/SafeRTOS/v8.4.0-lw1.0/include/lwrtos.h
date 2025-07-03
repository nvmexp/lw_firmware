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
 * @file    lwrtos.h
 * @brief   VR-TODO: Write documentation.
 */

#ifndef LWRTOS_H
#define LWRTOS_H

/* ------------------------ System Includes --------------------------------- */
#include "lwctassert.h"
#include "lwtypes.h"
#include "flcnifcmn.h"
#include "sections.h"

#include "portSyscall.h"

#include "string.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ External Definitions ---------------------------- */
extern void  vPortEnterCritical(void);
extern void  vPortExitCritical (void);

#ifdef FREEABLE_HEAP
extern void *pvPortMallocFreeable(LwUPtr size);
extern void vPortFreeFreeable(void *pv);
#endif /* FREEABLE_HEAP */

/* ------------------------ Macros and Defines ------------------------------ */
#define lwrtosTASK_FUNCTION(_funcName, _params)                                \
    void _funcName( void *_params )

#define lwrtosENTER_CRITICAL()          lwrtosTaskCriticalEnter()
#define lwrtosEXIT_CRITICAL()           lwrtosTaskCriticalExit()

/*!
 * For now keep the behavior identical.
 */
#define lwrtosYIELD()                   lwrtosYield()
#define lwrtosRELOAD()                  lwrtosYield()

#define lwrtosBYTE_ALIGNMENT           8U

#define lwrtosBYTE_ALIGN(_val)                                                 \
    (((_val) + (lwrtosBYTE_ALIGNMENT - 1U)) & ~(lwrtosBYTE_ALIGNMENT - 1U))

#define lwrtosMAX_DELAY                 LW_U64_MAX
#define lwrtosTASK_FUNC_ARG_COLD_START  ((void *) (0))
#define lwrtosTASK_FUNC_ARG_RESTART     ((void *) (1))

#define lwrtosTASK_PRIORITY_IDLE       0U
#define lwrtosTASK_PRIORITY_MAX        8U

#define lwrtosIDLE_TASK_ID             0U
#define lwrtosIDLE_PRIV_LEVEL_EXT      ((LwU8) 0)
#define lwrtosIDLE_PRIV_LEVEL_CSB      ((LwU8) 0)

/**
 * @brief      Define the tick-frequency (ticks/second) for the timer used to
 *             drive the scheduler. This is used to callwlate and program the
 *             timer interval of the falcon's general-purpose timer.
 *
 *             Must be kept in sync with @ref LWRTOS_TICK_PERIOD_US.
 */
#define LWRTOS_TICK_RATE_HZ            20000U

/**
 * @brief      Define the tick period in [us]/[ns] respectively of the falcon
 *             scheduler. This number might be rounded based on the value of
 *             LWRTOS_TICK_RATE_HZ.
 *
 *             Might be rounded so use with caution for accuracy.
 *             Must be kept in sync with @ref LWRTOS_TICK_RATE_HZ.
 */
#define LWRTOS_TICK_PERIOD_US          50U
#define LWRTOS_TICK_PERIOD_NS          ( LWRTOS_TICK_PERIOD_US * 1000U )

/**
 * @brief   Colwerts time in [us] to OS ticks, while always rounding up.
 */
#define LWRTOS_TIME_US_TO_TICKS(_timeUs)    LW_CEIL((_timeUs), LWRTOS_TICK_PERIOD_US)

/*
 * Notification API macros - they directly map to their SafeRTOS counterparts.
 */
#define lwrtosTaskNOTIFICATION_NO_ACTION                      ( 0U )   /* Notify the task without updating its notify value. */
#define lwrtosTaskNOTIFICATION_SET_BITS                       ( 1U )   /* Set bits in the task's notification value. */
#define lwrtosTaskNOTIFICATION_INCREMENT                      ( 2U )   /* Increment the task's notification value. */
#define lwrtosTaskNOTIFICATION_SET_VALUE_WITH_OVERWRITE       ( 3U )   /* Set the task's notification value to a specific value even if the previous value has not yet been read by the task. */
#define lwrtosTaskNOTIFICATION_SET_VALUE_WITHOUT_OVERWRITE    ( 4U )   /* Set the task's notification value if the previous value has been read by the task. */

#define LWRTOS_SECTION(X)

/*!
 * Halts the current exelwtion of code.
 */
#define lwrtosHALT()                    lwuc_halt();

/*!
 * @brief Set a breakpoint recoverable by RM
 *
 * lwrtosBREAKPOINT will halt the current exelwtion of code. This is very
 * much like lwrtosHALT, but RM will try to recover from the halt state and
 * resume exelwtion. Use this if the error is not critical thus resuming will
 * unlikely cause other problems.
 */
#define lwrtosBREAKPOINT()               lwuc_trap1()

/*!
 * @brief Checks if exelwting in kernel context
 */
#define lwrtosIS_KERNEL()                                                      \
({                                                                             \
    LwrtosTaskPrivate *pvt = lwrtosTaskPrivateRegister;                        \
    pvt->bIsKernel;                                                            \
})

/*!
 * @brief Checks if exelwting in kernel context or in priviliged mode
 *        (via sysPrivilegeRaise)
 */
#define lwrtosIS_PRIV()                                                        \
({                                                                             \
    LwrtosTaskPrivate *pvt = lwrtosTaskPrivateRegister;                        \
    pvt->bIsKernel || pvt->bIsPrivileged;                                      \
})

/* ------------------------ Types Definitions ------------------------------- */
typedef void (*LwrtosTaskFunction)(void *);

typedef void  *LwrtosTaskHandle;
typedef void  *LwrtosQueueHandle;
typedef void  *LwrtosSemaphoreHandle;

typedef struct {
    void             *pHeapMspace;    // Heap information, for dlmalloc (right now)
    LwBool           bIsPrivileged;   // True if task runs with Kernel privilege mode
    LwBool           bIsKernel;       // True if this is kernel TLS
    LwU8             criticalNesting; // Tracks critical nesting > 1
    void             *pvObject;       // "legacy" TCB PVT
    LwrtosTaskHandle taskHandle;      // Current task handle
} LwrtosTaskPrivate;

typedef struct {
    LwrtosTaskFunction  pTaskCode;      // Function to run as the task.
    void               *pTaskStack;     // Task stack base
    void               *pParameters;    // Parameters passed to task function.
    void               *pTcbPvt;        // Private TCB data pointer.
    struct TaskMpuInfo *pMpuInfo;       // MPU configuration.
    LwU32               priority;       // Task priority.
    LwU16               stackDepth;     // Task stack depth in words
    LwU8                taskID;         //
#ifdef FPU_SUPPORTED
    LwU8                bUsingFPU;      // Enable hw FPU.
#endif /* FPU_SUPPORTED */
} LwrtosTaskCreateParameters;

/* ------------------------ Function Prototypes ----------------------------- */
/* NOTE: Only functions with sysSHARED_CODE can be called from tasks. */

sysSHARED_CODE void                    lwrtosYield                            (void);
sysKERNEL_CODE void                    lwrtosTimeOffsetus                     (LwU32 timeus);

sysKERNEL_CODE FLCN_STATUS             lwrtosTaskStackStartGet                (LwrtosTaskHandle taskHandle, LwUPtr *pTaskStart);
sysKERNEL_CODE_INIT FLCN_STATUS        lwrtosTaskSchedulerInitialize          (void);
sysKERNEL_CODE_INIT FLCN_STATUS        lwrtosTaskSchedulerStart               (void);

sysSHARED_CODE LwBool                  lwrtosTaskSchedulerIsStarted           (void);
sysSHARED_CODE void                    lwrtosTaskSchedulerSuspend             (void);
sysSHARED_CODE void                    lwrtosTaskSchedulerResume              (void);

sysKERNEL_CODE_CREATE FLCN_STATUS      lwrtosTaskCreate                       (LwrtosTaskHandle *pTaskHandle, const LwrtosTaskCreateParameters *pParams);
sysKERNEL_CODE_CREATE void             lwrtosTlsInit                          (LwrtosTaskHandle pCtx, void *pTls);

sysKERNEL_CODE FLCN_STATUS             lwrtosTaskRestart                      (LwrtosTaskHandle taskHandle);
sysSHARED_CODE FLCN_STATUS             lwrtosTaskGetLwTcbExt                  (LwrtosTaskHandle taskHandle, void **ppLwTcbExt);

sysKERNEL_CODE LwBool                  lwrtosTaskIsBlocked                    (LwrtosTaskHandle taskHandle);

sysSHARED_CODE LwU32                   lwrtosTaskGetTickCount32               (void);
sysSHARED_CODE LwU64                   lwrtosTaskGetTickCount64               (void);
sysSHARED_CODE void                    lwrtosTaskCriticalEnter                (void);
sysSHARED_CODE void                    lwrtosTaskCriticalExit                 (void);

sysSHARED_CODE FLCN_STATUS             lwrtosLwrrentTaskPriorityGet           (LwU32 *pPriority);
sysSHARED_CODE FLCN_STATUS             lwrtosLwrrentTaskPrioritySet           (LwU32 newPriority);
sysSHARED_CODE FLCN_STATUS             lwrtosLwrrentTaskDelayTicks            (LwUPtr ticksToDelay);
sysSHARED_CODE LwrtosTaskHandle        lwrtosTaskGetLwrrentTaskHandle         (void);

sysKERNEL_CODE LwU32                   lwrtosTaskReadyTaskCountGetFromISR     (void);
sysSHARED_CODE LwU32                   lwrtosTaskReadyTaskCountGet            (void);

sysKERNEL_CODE FLCN_STATUS             lwrtosTaskCopyToKernel                 (void *pDestKernel, const void *pSrlwser, LwU32 size);
sysKERNEL_CODE FLCN_STATUS             lwrtosTaskCopyFromKernel               (void *pDestUser, const void *pSrcKernel, LwU32 size);

// MMINTS-TODO: lwrtosQueueCreate needs to be moved to sysKERNEL_CODE_CREATE once all explicit use-cases in task code have been removed
sysSHARED_CODE_CREATE FLCN_STATUS      lwrtosQueueCreate                      (LwrtosQueueHandle *pQueueHandle, LwU32 queueLength, LwU32 itemSize, LwU8 ovlIdxDmem);
sysKERNEL_CODE_CREATE FLCN_STATUS      lwrtosQueueCreateOvlRes                (LwrtosQueueHandle *pQueueHandle, LwU32 queueLength, LwU32 itemSize);

sysSHARED_CODE FLCN_STATUS             lwrtosQueueReceive                     (LwrtosQueueHandle queueHandle, void *pBuffer           , LwU32 size, LwUPtr ticksToWait);
sysSHARED_CODE FLCN_STATUS             lwrtosQueueSend                        (LwrtosQueueHandle queueHandle, const void *pItemToQueue, LwU32 size, LwUPtr ticksToWait);
sysSHARED_CODE FLCN_STATUS             lwrtosQueueSendBlocking                (LwrtosQueueHandle queueHandle, const void *pItemToQueue, LwU32 size);
sysKERNEL_CODE void                    lwrtosQueueSendFromISR                 (LwrtosQueueHandle queueHandle, const void *pItemToQueue, LwU32 size);
sysKERNEL_CODE FLCN_STATUS             lwrtosQueueSendFromISRWithStatus       (LwrtosQueueHandle queueHandle, const void *pItemToQueue, LwU32 size);
sysSHARED_CODE FLCN_STATUS             lwrtosQueueSendFromAtomic              (LwrtosQueueHandle queueHandle, const void *pItemToQueue, LwU32 size);
sysSHARED_CODE FLCN_STATUS             lwrtosQueueGetLwrrentNumItems          (LwrtosQueueHandle queueHandle, LwU32 *pItemsWaiting);

sysSHARED_CODE FLCN_STATUS             lwrtosQueueIdReceive                   (LwU8 queueID, void *pBuffer           , LwU32 size, LwUPtr ticksToWait);
sysSHARED_CODE FLCN_STATUS             lwrtosQueueIdSend                      (LwU8 queueID, const void *pItemToQueue, LwU32 size, LwUPtr ticksToWait);
sysSHARED_CODE FLCN_STATUS             lwrtosQueueIdSendBlocking              (LwU8 queueID, const void *pItemToQueue, LwU32 size);
sysKERNEL_CODE void                    lwrtosQueueIdSendFromISR               (LwU8 queueID, const void *pItemToQueue, LwU32 size);
sysKERNEL_CODE FLCN_STATUS             lwrtosQueueIdSendFromISRWithStatus     (LwU8 queueID, const void *pItemToQueue, LwU32 size);
sysSHARED_CODE FLCN_STATUS             lwrtosQueueIdSendFromAtomic            (LwU8 queueID, const void *pItemToQueue, LwU32 size);

sysSHARED_CODE FLCN_STATUS             lwrtosSemaphoreCreateBinaryTaken       (LwrtosSemaphoreHandle *pSemaphoreHandle, LwU8 ovlIdxDmem);
sysSHARED_CODE FLCN_STATUS             lwrtosSemaphoreCreateBinaryGiven       (LwrtosSemaphoreHandle *pSemaphoreHandle, LwU8 ovlIdxDmem);

sysSHARED_CODE FLCN_STATUS             lwrtosSemaphoreTake                    (LwrtosSemaphoreHandle semaphoreHandle, LwUPtr blockTime);
sysSHARED_CODE void                    lwrtosSemaphoreTakeWaitForever         (LwrtosSemaphoreHandle semaphoreHandle);
sysSHARED_CODE FLCN_STATUS             lwrtosSemaphoreGive                    (LwrtosSemaphoreHandle semaphoreHandle);
sysKERNEL_CODE void                    lwrtosSemaphoreGiveFromISR             (LwrtosSemaphoreHandle semaphoreHandle);

sysSHARED_CODE FLCN_STATUS             lwrtosTaskNotifyWait                   (LwUPtr bitsToClearOnEntry , LwUPtr bitsToClearOnExit, LwUPtr *pNotificatiolwalue, LwUPtr ticksToWait);
sysSHARED_CODE FLCN_STATUS             lwrtosTaskNotifySend                   (LwrtosTaskHandle taskToNotifyHandle, LwSPtr action, LwUPtr value);
sysKERNEL_CODE FLCN_STATUS             lwrtosTaskNotifySendFromISR            (LwrtosTaskHandle taskToNotifyHandle, LwSPtr action, LwUPtr value, LwBool *pHigherPriorityTaskWoken);

register LwrtosTaskPrivate *lwrtosTaskPrivateRegister __asm__("tp");


#endif /* LWRTOS_H_ */

