/*
 * OpenRTOS - Copyright (C) Wittenstein High Integrity Systems.
 *
 * OpenRTOS is distributed exclusively by Wittenstein High Integrity Systems,
 * and is subject to the terms of the License granted to your organization,
 * including its warranties and limitations on distribution.  It cannot be
 * copied or reproduced in any way except as permitted by the License.
 *
 * Licenses are issued for each conlwrrent user working on a specified product
 * line.
 *
 * WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
 * aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
 * Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
 * Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
 * E-mail: info@HighIntegritySystems.com
 * Registered in England No. 3711047; VAT No. GB 729 1583 15
 *
 * http://www.HighIntegritySystems.com
 */

#ifndef OPEN_RTOS_BUILD
    #error This header file must not be included outside OpenRTOS build.
#endif

#ifndef TASK_H
#define TASK_H

#include "portable.h"
#include "list.h"

/*-----------------------------------------------------------
 * MACROS AND DEFINITIONS
 *----------------------------------------------------------*/

#define tskKERNEL_VERSION_NUMBER "V4.1.3"

/*
 * Descriptor table index value for OS.
 * This is temporary define and will eventually come from a linker script.
 */
#define TEMP_OVL_INDEX_OS   ( ( unsigned portCHAR ) 0 )

/**
 * Type by which tasks are referenced.  For example, a call to xTaskCreate
 * returns (via a pointer parameter) an xTaskHandle variable that can then
 * be used as a parameter to vTaskDelete to delete the task.
 *
 * \page xTaskHandle xTaskHandle
 * \ingroup Tasks
 */
typedef void * xTaskHandle;

/*
 * Task control block.  A task control block (TCB) is allocated to each task,
 * and stores the context of the task.
 * 
 * Fields pxTopOfStack, pvTcbPvt and pcStackBaseAddress are used in context
 * switching assembly and should be updated when those fields are changed.
 */
typedef struct tskTaskControlBlock
{
    volatile portSTACK_TYPE *pxTopOfStack;      /*< Points to the location of the last item placed on the tasks stack.  THIS MUST BE THE FIRST MEMBER OF THE STRUCT. */
    void                    *pvTcbPvt;          /*< Private application (or porting-layer) specific data associated with the TCB. */
    portSTACK_TYPE          *pxStackBaseAddress;/*< The base address/bottom of the task stack. */
#ifdef GCC_FALCON
#ifdef TASK_RESTART
    pdTASK_CODE             pxCode;             /*< Entry function of the task. */
    unsigned portBASE_TYPE  uxStackSizeDwords;  /*< The total size of the task stack in d-words. */
    unsigned portBASE_TYPE  uxDefaultPriority;  /*< The default priority of the task where 0 is the lowest priority. */
#endif // TASK_RESTART
#endif // GCC_FALCON
    xListItem               xGenericListItem;   /*< List item used to place the TCB in ready and blocked queues. */
    xListItem               xEventListItem;     /*< List item used to place the TCB in event lists. */
    unsigned portBASE_TYPE  uxPriority;         /*< The current priority of the task where 0 is the lowest priority. */
    unsigned portCHAR       ucTaskID;           /*< Unique task ID provided by the caller of xTaskCreate().  Value zero is reserved for system IDLE task. */
#ifdef SCHEDULER_2X
    unsigned portCHAR       ucTicksToRun;       /*< The upper bound of the conselwtive timer ticks this task is allowed to run. */
#endif
} tskTCB;

/*
 * Used internally only.
 */
typedef struct xTIME_OUT
{
    portBASE_TYPE xOverflowCount;
    portTickType  xTimeOnEntering;
} xTimeOutType;

/*
 * The value used to fill the stack of a task when the task is created.  This
 * is used purely for checking the high water mark for tasks.
 */
#define tskSTACK_FILL_BYTE  ( 0xa5 )
#define tskSTACK_FILL_WORD  ( 0xa5a5a5a5 )

/*
 * Defines the priority used by the idle task.  This must not be modified.
 *
 * \ingroup TaskUtils
 */
#define tskIDLE_PRIORITY            ( ( unsigned portBASE_TYPE ) 0 )

/**
 * Macro for forcing a context switch.
 *
 * \page taskYIELD taskYIELD
 * \ingroup SchedulerControl
 */
#define taskYIELD()                 portYIELD()

/**
 * Macro to mark the start of a critical code region.  Preemptive context
 * switches cannot occur when in a critical region.
 *
 * NOTE: This may alter the stack (depending on the portable implementation)
 * so must be used with care!
 *
 * \page taskENTER_CRITICAL taskENTER_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskENTER_CRITICAL()        portENTER_CRITICAL()

/**
 * Macro to mark the end of a critical code region.  Preemptive context
 * switches cannot occur when in a critical region.
 *
 * NOTE: This may alter the stack (depending on the portable implementation)
 * so must be used with care!
 *
 * \page taskEXIT_CRITICAL taskEXIT_CRITICAL
 * \ingroup SchedulerControl
 */
#define taskEXIT_CRITICAL()         portEXIT_CRITICAL()

extern tskTCB * volatile pxLwrrentTCB;

/*
 * Several functions take an xTaskHandle parameter that can optionally be NULL,
 * where NULL is used to indicate that the handle of the lwrrently exelwting
 * task should be used in place of the parameter.  This macro simply checks to
 * see if the parameter is NULL and returns a pointer to the appropriate TCB.
 */
#define prvGetTCBFromHandle( pxHandle ) ( ( pxHandle == NULL ) ? ( tskTCB * ) pxLwrrentTCB : ( tskTCB * ) pxHandle )

/*-----------------------------------------------------------
 * TASK CREATION API
 *----------------------------------------------------------*/

/**
 * Create a new task and add it to the list of tasks that are ready to run.
 *
 * @param pvTaskCode Pointer to the task entry function.  Tasks
 * must be implemented to never return (i.e. continuous loop).
 *
 * @param ucTaskID An unique numeric task identifier.
 *
 * @param pxTaskStack Pointer to the stack allocated for this task.
 *
 * @param usStackDepth The size of the task stack specified as the number of
 * variables the stack can hold - not the number of bytes.  For example, if
 * the stack is 16 bits wide and usStackDepth is defined as 100, 200 bytes
 * will be allocated for stack storage.
 *
 * @param pvParameters Pointer that will be used as the parameter for the task
 * being created.
 *
 * @param uxPriority The priority at which the task should run.
 *
 * @param pvTcbPvt The pointer to the private TCB of created task.
 *
 * @return  Handle by which the created task can be referenced
 *
 * \defgroup xTaskCreate xTaskCreate
 * \ingroup Tasks
 */
xTaskHandle xTaskCreate( pdTASK_CODE pvTaskCode,
                         unsigned portCHAR ucTaskID,
                         portSTACK_TYPE *pxTaskStack,
                         unsigned portSHORT usStackDepth,
                         void *pvParameters,
                         unsigned portBASE_TYPE uxPriority,
                         void *pvTcbPvt) __attribute__((section (".imem_init._xTaskCreate")));

/*-----------------------------------------------------------
 * TASK CONTROL API
 *----------------------------------------------------------*/

/**
 * Delay a task for a given number of ticks.  The actual time that the
 * task remains blocked depends on the tick rate.  The constant
 * portTICK_RATE_MS can be used to callwlate real time from the tick
 * rate - with the resolution of one tick period.
 *
 * INCLUDE_vTaskDelay must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * @param xTicksToDelay The amount of time, in tick periods, that
 * the calling task should block.
 *
 * \defgroup vTaskDelay vTaskDelay
 * \ingroup TaskCtrl
 */
void vTaskDelay( portTickType xTicksToDelay );

/**
 * INCLUDE_vTaskDelayUntil must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Delay a task until a specified time.  This function can be used by cyclical
 * tasks to ensure a constant exelwtion frequency.
 *
 * This function differs from vTaskDelay () in one important aspect:  vTaskDelay () will
 * cause a task to block for the specified number of ticks from the time vTaskDelay () is
 * called.  It is therefore diffilwlt to use vTaskDelay () by itself to generate a fixed
 * exelwtion frequency as the time between a task starting to execute and that task
 * calling vTaskDelay () may not be fixed [the task may take a different path though the
 * code between calls, or may get interrupted or preempted a different number of times
 * each time it exelwtes].
 *
 * Whereas vTaskDelay () specifies a wake time relative to the time at which the function
 * is called, vTaskDelayUntil () specifies the absolute (exact) time at which it wishes to
 * unblock.
 *
 * The constant portTICK_RATE_MS can be used to callwlate real time from the tick
 * rate - with the resolution of one tick period.
 *
 * @param pxPreviousWakeTime Pointer to a variable that holds the time at which the
 * task was last unblocked.  The variable must be initialised with the current time
 * prior to its first use (see the example below).  Following this the variable is
 * automatically updated within vTaskDelayUntil ().
 *
 * @param xTimeIncrement The cycle time period.  The task will be unblocked at
 * time *pxPreviousWakeTime + xTimeIncrement.  Calling vTaskDelayUntil with the
 * same xTimeIncrement parameter value will cause the task to execute with
 * a fixed interface period.
 *
 * \defgroup vTaskDelayUntil vTaskDelayUntil
 * \ingroup TaskCtrl
 */
void vTaskDelayUntil( portTickType *pxPreviousWakeTime, portTickType xTimeIncrement );

/**
 * INCLUDE_xTaskPriorityGet must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Obtain the priority of any task.
 *
 * @param pxTask Handle of the task to be queried.  Passing a NULL
 * handle results in the priority of the calling task being returned.
 *
 * @return The priority of pxTask.
 *
 * \defgroup uxTaskPriorityGet uxTaskPriorityGet
 * \ingroup TaskCtrl
 */
unsigned portBASE_TYPE uxTaskPriorityGet( xTaskHandle pxTask );

/**
 * INCLUDE_vTaskPrioritySet must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Set the priority of any task.
 *
 * A context switch will occur before the function returns if the priority
 * being set is higher than the lwrrently exelwting task.
 *
 * @param pxTask Handle to the task for which the priority is being set.
 * Passing a NULL handle results in the priority of the calling task being set.
 *
 * @param uxNewPriority The priority to which the task will be set.
 *
 * \defgroup vTaskPrioritySet vTaskPrioritySet
 * \ingroup TaskCtrl
 */
void vTaskPrioritySet( xTaskHandle pxTask, unsigned portBASE_TYPE uxNewPriority );

/**
 * INCLUDE_vTaskSuspend must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Suspend any task.  When suspended a task will never get any microcontroller
 * processing time, no matter what its priority.
 *
 * Calls to vTaskSuspend are not aclwmulative -
 * i.e. calling vTaskSuspend () twice on the same task still only requires one
 * call to vTaskResume () to ready the suspended task.
 *
 * @param pxTaskToSuspend Handle to the task being suspended.  Passing a NULL
 * handle will cause the calling task to be suspended.
 *
 * \defgroup vTaskSuspend vTaskSuspend
 * \ingroup TaskCtrl
 */
void vTaskSuspend( xTaskHandle pxTaskToSuspend );

/**
 * INCLUDE_vTaskSuspend must be defined as 1 for this function to be available.
 * See the configuration section for more information.
 *
 * Resumes a suspended task.
 *
 * A task that has been suspended by one of more calls to vTaskSuspend ()
 * will be made available for running again by a single call to
 * vTaskResume ().
 *
 * @param pxTaskToResume Handle to the task being readied.
 *
 * \defgroup vTaskResume vTaskResume
 * \ingroup TaskCtrl
 */
void vTaskResume( xTaskHandle pxTaskToResume );

/**
 * INCLUDE_xTaskResumeFromISR must be defined as 1 for this function to be
 * available.  See the configuration section for more information.
 *
 * An implementation of vTaskResume() that can be called from within an ISR.
 *
 * A task that has been suspended by one of more calls to vTaskSuspend ()
 * will be made available for running again by a single call to
 * xTaskResumeFromISR ().
 *
 * @param pxTaskToResume Handle to the task being readied.
 *
 * \defgroup vTaskResumeFromISR vTaskResumeFromISR
 * \ingroup TaskCtrl
 */
portBASE_TYPE xTaskResumeFromISR( xTaskHandle pxTaskToResume );

#ifdef GCC_FALCON
#ifdef TASK_RESTART
/**
 * Restart a task
 *
 * GCC_FALCON and TASK_RESTART must be defined for this function
 * to be available.
 *
 * @param pxTaskToRestart The handle of the task to be
 * restarted.
 *
 * \defgroup vTaskRestart vTaskRestart
 * \ingroup TaskCtrl
 */
void vTaskRestart( xTaskHandle pxTaskToRestart );
#endif // TASK_RESTART
#endif // GCC_FALCON

/*-----------------------------------------------------------
 * SCHEDULER CONTROL
 *----------------------------------------------------------*/

/**
 * Starts the real time kernel tick processing.  After calling the kernel
 * has control over which tasks are exelwted and when.  This function
 * does not return until an exelwting task calls vTaskEndScheduler ().
 *
 * At least one task should be created via a call to xTaskCreate ()
 * before calling vTaskStartScheduler ().  The idle task is created
 * automatically when the first application task is created.
 *
 * See the demo application file main.c for an example of creating
 * tasks and starting the kernel.
 *
 * \defgroup vTaskStartScheduler vTaskStartScheduler
 * \ingroup SchedulerControl
 */
void vTaskStartScheduler( void ) __attribute__((section (".imem_init")));

/**
 * Stops the real time kernel tick.  All created tasks will be automatically
 * deleted and multitasking (either preemptive or cooperative) will
 * stop.  Exelwtion then resumes from the point where vTaskStartScheduler ()
 * was called, as if vTaskStartScheduler () had just returned.
 *
 * See the demo application file main. c in the demo/PC directory for an
 * example that uses vTaskEndScheduler ().
 *
 * vTaskEndScheduler () requires an exit function to be defined within the
 * portable layer (see vPortEndScheduler () in port. c for the PC port).  This
 * performs hardware specific operations such as stopping the kernel tick.
 *
 * vTaskEndScheduler () will cause all of the resources allocated by the
 * kernel to be freed - but will not free resources allocated by application
 * tasks.
 *
 * \defgroup vTaskEndScheduler vTaskEndScheduler
 * \ingroup SchedulerControl
 */
void vTaskEndScheduler( void );

/**
 * Suspends all real time kernel activity while keeping interrupts (including the
 * kernel tick) enabled.
 *
 * After calling vTaskSuspendAll () the calling task will continue to execute
 * without risk of being swapped out until a call to xTaskResumeAll () has been
 * made.
 *
 * \defgroup vTaskSuspendAll vTaskSuspendAll
 * \ingroup SchedulerControl
 */
void vTaskSuspendAll( void );

/**
 * Resumes real time kernel activity following a call to vTaskSuspendAll ().
 * After a call to vTaskSuspendAll () the kernel will take control of which
 * task is exelwting at any time.
 *
 * @return If resuming the scheduler caused a context switch then pdTRUE is
 *         returned, otherwise pdFALSE is returned.
 *
 * \defgroup xTaskResumeAll xTaskResumeAll
 * \ingroup SchedulerControl
 */
signed portBASE_TYPE xTaskResumeAll( void );


/*-----------------------------------------------------------
 * TASK UTILITIES
 *----------------------------------------------------------*/

/**
 * @return The count of ticks since vTaskStartScheduler was called.
 *
 * \page xTaskGetTickCount xTaskGetTickCount
 * \ingroup TaskUtils
 */
portTickType xTaskGetTickCount( void );

/**
 * @return The number of tasks that the real time kernel is lwrrently managing.
 * This includes all ready, blocked and suspended tasks.  A task that
 * has been deleted but not yet freed by the idle task will also be
 * included in the count.
 *
 * \page uxTaskGetNumberOfTasks uxTaskGetNumberOfTasks
 * \ingroup TaskUtils
 */
unsigned portBASE_TYPE uxTaskGetNumberOfTasks( void );

/*-----------------------------------------------------------
 * SCHEDULER INTERNALS AVAILABLE FOR PORTING PURPOSES
 *----------------------------------------------------------*/

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS ONLY
 * INTENDED FOR USE WHEN IMPLEMENTING A PORT OF THE SCHEDULER AND IS
 * AN INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * Called from the real time kernel tick (either preemptive or cooperative),
 * this increments the tick count and checks if any tasks that are blocked
 * for a finite period required removing from a blocked list and placing on
 * a ready list.
 */
inline void vTaskIncrementTick( void );

void vTaskAddTicks( unsigned portLONG )
    __attribute__((section (".imem_libDi._vTaskAddTicks")));

unsigned portBASE_TYPE uxTaskReadyTaskCountGetFromISR( void )
    __attribute__((section (".imem_lpwr._uxTaskReadyTaskCountGetFromISR")));

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS AN
 * INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED.
 *
 * Removes the calling task from the ready list and places it both
 * on the list of tasks waiting for a particular event, and the
 * list of delayed tasks.  The task will be removed from both lists
 * and replaced on the ready list should either the event occur (and
 * there be no higher priority tasks waiting on the same event) or
 * the delay period expires.
 *
 * @param pxEventList The list containing tasks that are blocked waiting
 * for the event to occur.
 *
 * @param xTicksToWait The maximum amount of time that the task should wait
 * for the event to occur.  This is specified in kernel ticks,the constant
 * portTICK_RATE_MS can be used to colwert kernel ticks into a real time
 * period.
 */
void vTaskPlaceOnEventList( xList *pxEventList, portTickType xTicksToWait );

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS AN
 * INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED.
 *
 * Removes a task from both the specified event list and the list of blocked
 * tasks, and places it on a ready queue.
 *
 * xTaskRemoveFromEventList () will be called if either an event oclwrs to
 * unblock a task, or the block timeout period expires.
 *
 * @return pdTRUE if the task being removed has a higher priority than the task
 * making the call, otherwise pdFALSE.
 */
signed portBASE_TYPE xTaskRemoveFromEventList( const xList *pxEventList );

/*
 * THIS FUNCTION MUST NOT BE USED FROM APPLICATION CODE.  IT IS ONLY
 * INTENDED FOR USE WHEN IMPLEMENTING A PORT OF THE SCHEDULER AND IS
 * AN INTERFACE WHICH IS FOR THE EXCLUSIVE USE OF THE SCHEDULER.
 *
 * Sets the pointer to the current TCB to the TCB of the highest priority task
 * that is ready to run.
 */
inline void vTaskSwitchContext( void );

/*
 * Return the handle of the calling task.
 */
xTaskHandle xTaskGetLwrrentTaskHandle( void );

/*
 * Capture the current time status for future reference.
 */
void vTaskSetTimeOutState( xTimeOutType *pxTimeOut );

/*
 * Compare the time status now with that previously captured to see if the
 * timeout has expired.
 */
portBASE_TYPE xTaskCheckForTimeOut( xTimeOutType *pxTimeOut, portTickType *pxTicksToWait );

/*
 * Shortlwt used by the queue implementation to prevent unnecessary call to
 * taskYIELD();
 */
void vTaskMissedYield( void );

#endif /* TASK_H */



