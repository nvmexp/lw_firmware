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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OpenRTOS.h"
#include "task.h"
#include "lwrtos.h"

#include "dmemovl.h"

#ifdef GCC_FALCON
#include "OpenRTOSFalcon.h"
#include "lwrtosHooks.h"
#endif

/*
 * Default a definitions for backwards compatibility with old
 * portmacro.h files.
 */
#ifndef INCLUDE_xTaskResumeFromISR
    #define INCLUDE_xTaskResumeFromISR 1
#endif

/*lint -e956 */

tskTCB * volatile pxLwrrentTCB = NULL;

/* Lists for ready and blocked tasks. --------------------*/

static xList pxReadyTasksLists[ configMAX_PRIORITIES ]; /*< Prioritised ready tasks. */
static xList xDelayedTaskList1;                         /*< Delayed tasks. */
static xList xDelayedTaskList2;                         /*< Delayed tasks (two lists are used - one for delays that have overflowed the current tick count. */
static xList * volatile pxDelayedTaskList;              /*< Points to the delayed task list lwrrently being used. */
static xList * volatile pxOverflowDelayedTaskList;      /*< Points to the delayed task list lwrrently being used to hold tasks that have overflowed the current tick count. */
static xList xPendingReadyList;                         /*< Tasks that have been readied while the scheduler was suspended.  They will be moved to the ready queue when the scheduler is resumed. */

#if ( INCLUDE_vTaskSuspend == 1 )

    static xList xSuspendedTaskList;                    /*< Tasks that are lwrrently suspended. */

#endif

/* File private variables. --------------------------------*/
static volatile unsigned portBASE_TYPE uxLwrrentNumberOfTasks   = ( unsigned portBASE_TYPE ) 0;
static volatile portTickType xTickCount                         = ( portTickType ) 0;
static unsigned portBASE_TYPE uxTopUsedPriority                 = tskIDLE_PRIORITY;
static volatile unsigned portBASE_TYPE uxTopReadyPriority       = tskIDLE_PRIORITY;
       volatile signed portBASE_TYPE xSchedulerRunning          = pdFALSE;
static volatile unsigned portBASE_TYPE uxSchedulerSuspended     = ( unsigned portBASE_TYPE ) pdFALSE;
static volatile unsigned portBASE_TYPE uxMissedTicks            = ( unsigned portBASE_TYPE ) 0;
static volatile portBASE_TYPE xMissedYield                      = ( portBASE_TYPE ) pdFALSE;
static volatile portBASE_TYPE xNumOfOverflows                   = ( portBASE_TYPE ) 0;
/* Debugging and trace facilities private variables and macros. ------------*/

/*
 * Macros used by vListTask to indicate which state a task is in.
 */
#define tskBLOCKED_CHAR     ( ( signed portCHAR ) 'B' )
#define tskREADY_CHAR       ( ( signed portCHAR ) 'R' )
#define tskDELETED_CHAR     ( ( signed portCHAR ) 'D' )
#define tskSUSPENDED_CHAR   ( ( signed portCHAR ) 'S' )

/*
 * Place the task represented by pxTCB into the appropriate ready queue for
 * the task.  It is inserted at the end of the list.  One quirk of this is
 * that if the task being inserted is at the same priority as the lwrrently
 * exelwting task, then it will only be rescheduled after the lwrrently
 * exelwting task has been rescheduled.
 */
static void prvAddTaskToReadyQueue( tskTCB *pxTCB )
{
    if( pxTCB->uxPriority > uxTopReadyPriority )
    {
        uxTopReadyPriority = pxTCB->uxPriority;
    }
    vListInsertEnd( ( xList * ) &( pxReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xGenericListItem ) );
}

/*
 * Macro that looks at the list of tasks that are lwrrently delayed to see if
 * any require waking.
 *
 * Tasks are stored in the queue in the order of their wake time - meaning
 * once one tasks has been found whose timer has not expired we need not look
 * any further down the list.
 */
#define prvCheckDelayedTasks()                                                                                      \
{                                                                                                                   \
register tskTCB *pxTCB;                                                                                             \
                                                                                                                    \
    while( ( pxTCB = ( tskTCB * ) listGET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList ) ) != NULL )                      \
    {                                                                                                               \
        if( xTickCount < listGET_LIST_ITEM_VALUE( &( pxTCB->xGenericListItem ) ) )                                  \
        {                                                                                                           \
            break;                                                                                                  \
        }                                                                                                           \
        vListRemove( &( pxTCB->xGenericListItem ) );                                                                \
        /* Is the task waiting on an event also? */                                                                 \
        if( pxTCB->xEventListItem.pvContainer )                                                                     \
        {                                                                                                           \
            vListRemove( &( pxTCB->xEventListItem ) );                                                              \
        }                                                                                                           \
        prvAddTaskToReadyQueue( pxTCB );                                                        \
    }                                                                                                               \
}

/* File private functions. --------------------------------*/

/*
 * Utility to ready a TCB for a given task.  Mainly just copies the parameters
 * into the TCB structure.
 */
static void prvInitialiseTCBVariables( tskTCB *pxTCB,
                                       unsigned portCHAR ucTaskID,
                                       unsigned portBASE_TYPE uxPriority,
                                       void *pvTcbPvt ) __attribute__((section (".imem_init")));

/*
 * Utility to ready all the lists used by the scheduler.  This is called
 * automatically upon the creation of the first task.
 */
static void prvInitialiseTaskLists( void ) __attribute__((section (".imem_init")));

/*
 * The idle task, which as all tasks is implemented as a never ending loop.
 * The idle task is automatically created and added to the ready lists upon
 * creation of the first user task.
 *
 * The portTASK_FUNCTION() macro is used to allow port/compiler specific
 * language extensions.  The equivalent prototype for this function is:
 *
 * void vIdleTask( void *pvParameters );
 *
 */
static portTASK_FUNCTION( vIdleTask, pvParameters );

/*lint +e956 */

/*-----------------------------------------------------------
 * TASK CREATION API dolwmented in task.h
 *----------------------------------------------------------*/
xTaskHandle xTaskCreate( pdTASK_CODE pvTaskCode, unsigned portCHAR ucTaskID, portSTACK_TYPE *pxTaskStack, unsigned portSHORT usStackDepth, void *pvParameters, unsigned portBASE_TYPE uxPriority, void *pvTcbPvt )
{
tskTCB *pxNewTCB;

    /* Allocate space for the TCB.  Where the memory comes from depends on
    the implementation of the port calloc function. */
    pxNewTCB = ( tskTCB * ) lwosCallocResident( sizeof( tskTCB ) );

    if( pxNewTCB != NULL )
    {
        portSTACK_TYPE *pxTopOfStack;

        /* Assign stack bottom so assembly can store it into STACKCFG register field that was added in falcon6.
        Used in context __restore_ctx_merged/_handler_context_save code in falcon6 assembly.
        Also used in SW stack overflow checking in earlier falcon versions. */
        pxNewTCB->pxStackBaseAddress = pxTaskStack;

        /* Setup the newly allocated TCB with the initial state of the task. */
        prvInitialiseTCBVariables( pxNewTCB, ucTaskID, uxPriority, pvTcbPvt );

        /* Callwlate the address of the top of stack.  Stack grows from higher
        memory addresses (top) towards lower memory addresses */
        pxTopOfStack = pxTaskStack + ( usStackDepth - 1 );

        /* Initialize the TCB stack to look as if the task was already running,
        but had been interrupted by the scheduler.  The return address is set
        to the start of the task function. Once the stack has been initialised
        the top of stack variable is updated. */
        pxNewTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pvTaskCode, pvParameters );

#ifdef GCC_FALCON
#ifdef TASK_RESTART
        /* Cache off the entry function and priority for use with task
        restarts. */
        pxNewTCB->pxCode            = pvTaskCode;
        pxNewTCB->uxDefaultPriority = uxPriority;
        pxNewTCB->uxStackSizeDwords = usStackDepth;
#endif // TASK_RESTART
#endif // GCC_FALCON

        /* We are going to manipulate the task queues to add this task to a
        ready list, so must make sure no interrupts occur. */
        portENTER_CRITICAL();
        {
            uxLwrrentNumberOfTasks++;
            if( uxLwrrentNumberOfTasks == ( unsigned portBASE_TYPE ) 1 )
            {
                /* As this is the first task it must also be the current task. */
                pxLwrrentTCB =  pxNewTCB;

                /* This is the first task to be created so do the preliminary
                initialisation required.  We will not recover if this call
                fails, but we will report the failure. */
                prvInitialiseTaskLists();
            }
            else
            {
                /* If the scheduler is not already running, make this task the
                current task if it is the highest priority task to be created
                so far. */
                if( xSchedulerRunning == pdFALSE )
                {
                    if( pxLwrrentTCB->uxPriority <= uxPriority )
                    {
                        pxLwrrentTCB = pxNewTCB;
                    }
                }
            }

            /* Remember the top priority to make context switching faster.  Use
            the priority in pxNewTCB as this has been capped to a valid value. */
            if( pxNewTCB->uxPriority > uxTopUsedPriority )
            {
                uxTopUsedPriority = pxNewTCB->uxPriority;
            }

            prvAddTaskToReadyQueue( pxNewTCB );
        }
        portEXIT_CRITICAL();
    }
    else
    {
        falc_halt();
    }

    if( xSchedulerRunning != pdFALSE )
    {
        /* If the created task is of a higher priority than the current task
        then it should run now. */
        if( pxLwrrentTCB->uxPriority < uxPriority )
        {
            taskYIELD();
        }
    }

    /* Pass the TCB out - in an anonymous way.  The calling function/task can
    use this as a handle to delete the task later if required.*/
    return ( xTaskHandle ) pxNewTCB;
}

/*-----------------------------------------------------------
 * TASK CONTROL API dolwmented in task.h
 *----------------------------------------------------------*/

#ifdef GCC_FALCON
#ifdef TASK_RESTART
    void vTaskRestart( xTaskHandle pxTaskToRestart )
    {
    tskTCB *pxTCB;

        if ( pxTaskToRestart == NULL )
        {
            /* We must receive a valid task handle to restart. */
            return;
        }

        taskENTER_CRITICAL();
        {
            portSTACK_TYPE *pxTopOfStack;

            if ( pxTaskToRestart == pxLwrrentTCB )
            {
                /* We cannot restart ourselves. */
                taskEXIT_CRITICAL();
                return;
            }

            pxTCB = prvGetTCBFromHandle( pxTaskToRestart );

            /* The task may be in the ready list or the delayed list. Without
            caring which one it is in, blindly remove it from whichever list it
            belongs to. We'll add it back to the ready list right after. */
            vListRemove( &( pxTCB->xGenericListItem ) );

            /* Restore the task priority to its default level in case it had
            changed. Then adding it to the ready list will automatically add it
            to the ready list belonging to its default priority level. */
            pxTCB->uxPriority = pxTCB->uxDefaultPriority;
            prvAddTaskToReadyQueue( pxTCB );

            /* If the task waiting on an event, remove it from there too. */
            if( pxTCB->xEventListItem.pvContainer )
            {
                vListRemove( &( pxTCB->xEventListItem ) );
            }

            lwrtosTaskStackStartGet( pxTaskToRestart, ( LwUPtr * )&pxTopOfStack );
            /* lwrtosTaskStackStartGet yields the absolute stack top
            so shift down to first d-word. */
            pxTopOfStack--;

            /* Reinitialize the stack itself so that the next time this task is
            scheduled, it starts exelwting from its entry function.
            This time, place an integer on the stack that indicates to the task
            that it is a restart so that it can do its own cleanup. */
            pxTCB->pxTopOfStack = pxPortInitialiseStack( pxTopOfStack, pxTCB->pxCode, PORT_TASK_FUNC_ARG_RESTART );
        }
        taskEXIT_CRITICAL();
    }
#endif // TASK_RESTART
#endif // GCC_FALCON

/*-----------------------------------------------------------
 * TASK CONTROL API dolwmented in task.h
 *----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelayUntil == 1 )

    void vTaskDelayUntil( portTickType *pxPreviousWakeTime, portTickType xTimeIncrement )
    {
    portTickType xTimeToWake;
    portBASE_TYPE xAlreadyYielded, xShouldDelay = pdFALSE;

        vTaskSuspendAll();
        {
            /* Generate the tick time at which the task wants to wake. */
            xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

            if( xTickCount < *pxPreviousWakeTime )
            {
                /* The tick count has overflowed since this function was
                lasted called.  In this case the only time we should ever
                actually delay is if the wake time has also overflowed,
                and the wake time is greater than the tick time.  When this
                is the case it is as if neither time had overflowed. */
                if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xTickCount ) )
                {
                    xShouldDelay = pdTRUE;
                }
            }
            else
            {
                /* The tick time has not overflowed.  In this case we will
                delay if either the wake time has overflowed, and/or the
                tick time is less than the wake time. */
                if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xTickCount ) )
                {
                    xShouldDelay = pdTRUE;
                }
            }

            /* Update the wake time ready for the next call. */
            *pxPreviousWakeTime = xTimeToWake;

            if( xShouldDelay )
            {
                /* We must remove ourselves from the ready list before adding
                ourselves to the blocked list as the same list item is used for
                both lists. */
                vListRemove( ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );

                /* The list item will be inserted in wake time order. */
                listSET_LIST_ITEM_VALUE( &( pxLwrrentTCB->xGenericListItem ), xTimeToWake );

                if( xTimeToWake < xTickCount )
                {
                    /* Wake time has overflowed.  Place this item in the
                    overflow list. */
                    vListInsert( ( xList * ) pxOverflowDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
                }
                else
                {
                    /* The wake time has not overflowed, so we can use the
                    current block list. */
                    vListInsert( ( xList * ) pxDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
                }
            }
        }
        xAlreadyYielded = xTaskResumeAll();

        /* Force a reschedule if xTaskResumeAll has not already done so, we may
        have put ourselves to sleep. */
        if( !xAlreadyYielded )
        {
            taskYIELD();
        }
    }

#endif
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskDelay == 1 )

    void vTaskDelay( portTickType xTicksToDelay )
    {
    portTickType xTimeToWake;
    signed portBASE_TYPE xAlreadyYielded = pdFALSE;

        /* A delay time of zero just forces a reschedule. */
        if( xTicksToDelay > ( portTickType ) 0 )
        {
            vTaskSuspendAll();
            {
                /* A task that is removed from the event list while the
                scheduler is suspended will not get placed in the ready
                list or removed from the blocked list until the scheduler
                is resumed.

                This task cannot be in an event list as it is the lwrrently
                exelwting task. */

                /* Callwlate the time to wake - this may overflow but this is
                not a problem. */
                xTimeToWake = xTickCount + xTicksToDelay;

                /* We must remove ourselves from the ready list before adding
                ourselves to the blocked list as the same list item is used for
                both lists. */
                vListRemove( ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );

                /* The list item will be inserted in wake time order. */
                listSET_LIST_ITEM_VALUE( &( pxLwrrentTCB->xGenericListItem ), xTimeToWake );

                if( xTimeToWake < xTickCount )
                {
                    /* Wake time has overflowed.  Place this item in the
                    overflow list. */
                    vListInsert( ( xList * ) pxOverflowDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
                }
                else
                {
                    /* The wake time has not overflowed, so we can use the
                    current block list. */
                    vListInsert( ( xList * ) pxDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
                }
            }
            xAlreadyYielded = xTaskResumeAll();
        }

        /* Force a reschedule if xTaskResumeAll has not already done so, we may
        have put ourselves to sleep. */
        if( !xAlreadyYielded )
        {
            taskYIELD();
        }
    }

#endif
/*-----------------------------------------------------------*/

#if ( INCLUDE_uxTaskPriorityGet == 1 )

    unsigned portBASE_TYPE uxTaskPriorityGet( xTaskHandle pxTask )
    {
    tskTCB *pxTCB;
    unsigned portBASE_TYPE uxReturn;

        taskENTER_CRITICAL();
        {
            /* If null is passed in here then we are changing the
            priority of the calling function. */
            pxTCB = prvGetTCBFromHandle( pxTask );
            uxReturn = pxTCB->uxPriority;
        }
        taskEXIT_CRITICAL();

        return uxReturn;
    }

#endif
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskPrioritySet == 1 )

    void vTaskPrioritySet( xTaskHandle pxTask, unsigned portBASE_TYPE uxNewPriority )
    {
    tskTCB *pxTCB;
    unsigned portBASE_TYPE uxLwrrentPriority, xYieldRequired = pdFALSE;

        taskENTER_CRITICAL();
        {
            /* If null is passed in here then we are changing the
            priority of the calling function. */
            pxTCB = prvGetTCBFromHandle( pxTask );
            uxLwrrentPriority = pxTCB->uxPriority;

            if( uxLwrrentPriority != uxNewPriority )
            {
                /* The priority change may have readied a task of higher
                priority than the calling task. */
                if( uxNewPriority > pxLwrrentTCB->uxPriority )
                {
                    if( pxTask != NULL )
                    {
                        /* The priority of another task is being raised.  If we
                        were raising the priority of the lwrrently running task
                        there would be no need to switch as it must have already
                        been the highest priority task. */
                        xYieldRequired = pdTRUE;
                    }
                }
                else if( pxTask == NULL )
                {
                    /* Setting our own priority down means there may now be another
                    task of higher priority that is ready to execute. */
                    xYieldRequired = pdTRUE;
                }

                pxTCB->uxPriority = uxNewPriority;
                listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), configMAX_PRIORITIES - ( portTickType ) uxNewPriority );

                /* If the task is in the blocked or suspended list we need do
                nothing more than change it's priority variable. However, if
                the task is in a ready list it needs to be removed and placed
                in the queue appropriate to its new priority. */
                if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxLwrrentPriority ] ), &( pxTCB->xGenericListItem ) ) )
                {
                    /* The task is lwrrently in its ready list - remove before adding
                    it to it's new ready list.  As we are in a critical section we
                    can do this even if the scheduler is suspended. */
                    vListRemove( &( pxTCB->xGenericListItem ) );
                    prvAddTaskToReadyQueue( pxTCB );
                }

                if( xYieldRequired == pdTRUE )
                {
                    taskYIELD();
                }
            }
        }
        taskEXIT_CRITICAL();
    }

#endif
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

    void vTaskSuspend( xTaskHandle pxTaskToSuspend )
    {
    tskTCB *pxTCB;

        taskENTER_CRITICAL();
        {
            /* Ensure a yield is performed if the current task is being
            suspended. */
            if( pxTaskToSuspend == pxLwrrentTCB )
            {
                pxTaskToSuspend = NULL;
            }

            /* If null is passed in here then we are suspending ourselves. */
            pxTCB = prvGetTCBFromHandle( pxTaskToSuspend );

            /* Remove task from the ready/delayed list and place in the suspended list. */
            vListRemove( &( pxTCB->xGenericListItem ) );

            /* Is the task waiting on an event also? */
            if( pxTCB->xEventListItem.pvContainer )
            {
                vListRemove( &( pxTCB->xEventListItem ) );
            }

            vListInsertEnd( ( xList * ) &xSuspendedTaskList, &( pxTCB->xGenericListItem ) );
        }
        taskEXIT_CRITICAL();

        /* We may have just suspended the current task. */
        if( ( void * ) pxTaskToSuspend == NULL )
        {
            taskYIELD();
        }
    }

#endif
/*-----------------------------------------------------------*/

#if ( INCLUDE_vTaskSuspend == 1 )

    void vTaskResume( xTaskHandle pxTaskToResume )
    {
    tskTCB *pxTCB;

        /* Remove the task from whichever list it is lwrrently in, and place
        it in the ready list. */
        pxTCB = ( tskTCB * ) pxTaskToResume;

        /* The parameter cannot be NULL as it is impossible to resume the
        lwrrently exelwting task. */
        if( pxTCB != NULL )
        {
            taskENTER_CRITICAL();
            {
                /* Is the task we are attempting to resume actually suspended? */
                if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xGenericListItem ) ) != pdFALSE )
                {
                    /* Has the task already been resumed from within an ISR? */
                    if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) != pdTRUE )
                    {
                        /* As we are in a critical section we can access the ready
                        lists even if the scheduler is suspended. */
                        vListRemove(  &( pxTCB->xGenericListItem ) );
                        prvAddTaskToReadyQueue( pxTCB );

                        /* We may have just resumed a higher priority task. */
                        if( pxTCB->uxPriority >= pxLwrrentTCB->uxPriority )
                        {
                            /* This yield may not cause the task just resumed to run, but
                            will leave the lists in the correct state for the next yield. */
                            taskYIELD();
                        }
                    }
                }
            }
            taskEXIT_CRITICAL();
        }
    }

#endif

/*-----------------------------------------------------------*/

#if ( ( INCLUDE_xTaskResumeFromISR == 1 ) && ( INCLUDE_vTaskSuspend == 1 ) )

    portBASE_TYPE xTaskResumeFromISR( xTaskHandle pxTaskToResume )
    {
    portBASE_TYPE xYieldRequired = pdFALSE;
    tskTCB *pxTCB;

        pxTCB = ( tskTCB * ) pxTaskToResume;

        /* Is the task we are attempting to resume actually suspended? */
        if( listIS_CONTAINED_WITHIN( &xSuspendedTaskList, &( pxTCB->xGenericListItem ) ) != pdFALSE )
        {
            /* Has the task already been resumed from within an ISR? */
            if( listIS_CONTAINED_WITHIN( &xPendingReadyList, &( pxTCB->xEventListItem ) ) != pdTRUE )
            {
                if( uxSchedulerSuspended == ( unsigned portBASE_TYPE ) pdFALSE )
                {
                    xYieldRequired = ( pxTCB->uxPriority >= pxLwrrentTCB->uxPriority );
                    vListRemove(  &( pxTCB->xGenericListItem ) );
                    prvAddTaskToReadyQueue( pxTCB );
                }
                else
                {
                    /* We cannot access the delayed or ready lists, so will hold this
                    task pending until the scheduler is resumed, at which point a
                    yield will be preformed if necessary. */
                    vListInsertEnd( ( xList * ) &( xPendingReadyList ), &( pxTCB->xEventListItem ) );
                }
            }
        }

        return xYieldRequired;
    }

#endif




/*-----------------------------------------------------------
 * PUBLIC SCHEDULER CONTROL dolwmented in task.h
 *----------------------------------------------------------*/


void vTaskStartScheduler( void )
{
    void vApplicationIdleTaskCreate( pdTASK_CODE pvTaskCode, unsigned portBASE_TYPE uxPriority );

    /* Add the idle task at the lowest priority and assign it task ID zero. */
    vApplicationIdleTaskCreate( vIdleTask, tskIDLE_PRIORITY );

    /* Interrupts are turned off here, to ensure a tick does not occur
    before or during the call to xPortStartScheduler().  The stacks of
    the created tasks contain a status word with interrupts switched on
    so interrupts will automatically get re-enabled when the first task
    starts to run.

    STEPPING THROUGH HERE USING A DEBUGGER CAN CAUSE BIG PROBLEMS IF THE
    DEBUGGER ALLOWS INTERRUPTS TO BE PROCESSED. */
    portENTER_CRITICAL();

    xSchedulerRunning = pdTRUE;
    xTickCount = ( portTickType ) 0;

    /* Setting up the timer tick is hardware specific and thus in the
    portable interface. */
    if( xPortStartScheduler() )
    {
        /* Should not reach here as if the scheduler is running the
        function will not return. */
    }
    else
    {
        /* Should only reach here if a task calls xTaskEndScheduler(). */
    }
}
/*-----------------------------------------------------------*/

void vTaskEndScheduler( void )
{
    /* Stop the scheduler interrupts and call the portable scheduler end
    routine so the original ISRs can be restored if necessary.  The port
    layer must ensure interrupts enable bit is left in the correct state. */
    portENTER_CRITICAL();
    xSchedulerRunning = pdFALSE;
    vPortEndScheduler();
}
/*----------------------------------------------------------*/

void vTaskSuspendAll( void )
{
    portENTER_CRITICAL();
        ++uxSchedulerSuspended;
    portEXIT_CRITICAL();
}
/*----------------------------------------------------------*/

signed portBASE_TYPE xTaskResumeAll( void )
{
register tskTCB *pxTCB;
signed portBASE_TYPE xAlreadyYielded = pdFALSE;

    /* It is possible that an ISR caused a task to be removed from an event
    list while the scheduler was suspended.  If this was the case then the
    removed task will have been added to the xPendingReadyList.  Once the
    scheduler has been resumed it is safe to move all the pending ready
    tasks from this list into their appropriate ready list. */
    portENTER_CRITICAL();
    {
        --uxSchedulerSuspended;

        if( uxSchedulerSuspended == ( unsigned portBASE_TYPE ) pdFALSE )
        {
            if( uxLwrrentNumberOfTasks > ( unsigned portBASE_TYPE ) 0 )
            {
                portBASE_TYPE xYieldRequired = pdFALSE;

                /* Move any readied tasks from the pending list into the
                appropriate ready list. */
                while( ( pxTCB = ( tskTCB * ) listGET_OWNER_OF_HEAD_ENTRY(  ( ( xList * ) &xPendingReadyList ) ) ) != NULL )
                {
                    vListRemove( &( pxTCB->xEventListItem ) );
                    vListRemove( &( pxTCB->xGenericListItem ) );
                    prvAddTaskToReadyQueue( pxTCB );

                    /* If we have moved a task that has a priority higher than
                    the current task then we should yield. */
                    if( pxTCB->uxPriority >= pxLwrrentTCB->uxPriority )
                    {
                        xYieldRequired = pdTRUE;
                    }
                }

                /* If any ticks oclwrred while the scheduler was suspended then
                they should be processed now.  This ensures the tick count does not
                slip, and that any delayed tasks are resumed at the correct time. */
                if( uxMissedTicks > ( unsigned portBASE_TYPE ) 0 )
                {
                    while( uxMissedTicks > ( unsigned portBASE_TYPE ) 0 )
                    {
                        vTaskIncrementTick();
                        --uxMissedTicks;
                    }

                    /* As we have processed some ticks it is appropriate to yield
                    to ensure the highest priority task that is ready to run is
                    the task actually running. */
                    xYieldRequired = pdTRUE;
                }

                if( ( xYieldRequired == pdTRUE ) || ( xMissedYield == pdTRUE ) )
                {
                    xAlreadyYielded = pdTRUE;
                    xMissedYield = pdFALSE;
                    taskYIELD();
                }
            }
        }
    }
    portEXIT_CRITICAL();

    return xAlreadyYielded;
}






/*-----------------------------------------------------------
 * PUBLIC TASK UTILITIES dolwmented in task.h
 *----------------------------------------------------------*/



portTickType xTaskGetTickCount( void )
{
    return xTickCount;
}
/*-----------------------------------------------------------*/

unsigned portBASE_TYPE uxTaskGetNumberOfTasks( void )
{
unsigned portBASE_TYPE uxNumberOfTasks;

    taskENTER_CRITICAL();
        uxNumberOfTasks = uxLwrrentNumberOfTasks;
    taskEXIT_CRITICAL();

    return uxNumberOfTasks;
}
/*-----------------------------------------------------------
 * SCHEDULER INTERNALS AVAILABLE FOR PORTING PURPOSES
 * dolwmented in task.h
 *----------------------------------------------------------*/
void vTaskAddTicks( unsigned portLONG ticks )
{
    /* Increment the tick count by the amount passed in to compensate
    for idleness and sync cpu time with real time */
    taskENTER_CRITICAL();
    {
        /* The scheduler is now suspended and we magically change the
        CPMU time. Any subsequent expiries should be taken care of naturally
        in the next tick. */
        if (xTickCount + ticks < xTickCount)
        {
            // Overflow case. Swap queues
            xList *pxTemp;

            /* Tick count has overflowed so we need to swap the delay lists.
            If there are any items in pxDelayedTaskList here then there is
            an error! */
            pxTemp = pxDelayedTaskList;
            pxDelayedTaskList = pxOverflowDelayedTaskList;
            pxOverflowDelayedTaskList = pxTemp;
            xNumOfOverflows++;
        }
        xTickCount += ticks;
    }
    taskEXIT_CRITICAL();
    /* Any expiries will be taken care of in the next tick. Just exit now. */
}

inline void vTaskIncrementTick( void )
{
    /* Called by the portable layer each time a tick interrupt oclwrs.
    Increments the tick then checks to see if the new tick value will cause any
    tasks to be unblocked. */
    if( uxSchedulerSuspended == ( unsigned portBASE_TYPE ) pdFALSE )
    {
        ++xTickCount;
        if( xTickCount == ( portTickType ) 0 )
        {
            xList *pxTemp;

            /* Tick count has overflowed so we need to swap the delay lists.
            If there are any items in pxDelayedTaskList here then there is
            an error! */
            pxTemp = pxDelayedTaskList;
            pxDelayedTaskList = pxOverflowDelayedTaskList;
            pxOverflowDelayedTaskList = pxTemp;
            xNumOfOverflows++;
        }

        prvCheckDelayedTasks();
        if (0 == uxMissedTicks)
        {
            /* See if this tick has made a callback expire. */
            #if (OS_CALLBACKS)
                lwrtosHookTimerTick( xTickCount );
            #endif
        }
    }
    else
    {
        ++uxMissedTicks;
        /* See if this tick has made a callback expire. */
        #if (OS_CALLBACKS)
            lwrtosHookTimerTick( xTickCount );
        #endif
    }

}
/*-----------------------------------------------------------*/

void vTaskSwitchContext( void )
{
    if( uxSchedulerSuspended != ( unsigned portBASE_TYPE ) pdFALSE )
    {
        /* The scheduler is lwrrently suspended - do not allow a context
        switch. */
        xMissedYield = pdTRUE;
        return;
    }

    #ifndef HW_STACK_LIMIT_MONITORING
    // Try to catch stack overflows in the task when its context switches out.
    vPortStackOverflowDetect(pxLwrrentTCB);
    #endif

    //
    // Keep track of the number of tasks that failed to load due to
    // suspended DMA per ready-list and globally.
    //
    unsigned char uxLoadFailuresList   = 0;
    unsigned char uxLoadFailuresGlobal = 0;

    // keep track the priority of the first task that failed to load
    unsigned int  uxTopFailedPriority  = tskIDLE_PRIORITY;

#ifdef SCHEDULER_2X
    tskTCB *pxPrevTCB = pxLwrrentTCB;
#endif

    // loop forever until a task is found and loaded
    for (;;)
    {
        /* Find the highest priority queue that contains ready tasks. */
        while( listLIST_IS_EMPTY( &( pxReadyTasksLists[ uxTopReadyPriority ] ) ) )
        {
            --uxTopReadyPriority;
            uxLoadFailuresList = 0;
        }

#ifdef SCHEDULER_2X
        /* If previously running task has more availabe ticks to run, use those
        instead of context switching to the task of the same or lower priority. */
        if( pxPrevTCB->ucTicksToRun > 0 )
        {
            if( listIS_CONTAINED_WITHIN( &( pxReadyTasksLists[ uxTopReadyPriority ] ), &( pxPrevTCB->xGenericListItem )))
            {
                int blocks;

                pxPrevTCB->ucTicksToRun--;
                pxLwrrentTCB = pxPrevTCB;

                blocks = xPortLoadTask( ( xTaskHandle )pxLwrrentTCB, NULL );
                if( blocks >= 0 )
                {
                    break;
                }
            }
        }
#endif

        /* listGET_OWNER_OF_NEXT_ENTRY walks through the list, so the tasks of the
        same priority get an equal share of the processor time. */
        listGET_OWNER_OF_NEXT_ENTRY( pxLwrrentTCB, &( pxReadyTasksLists[ uxTopReadyPriority ] ) );

        {
            int blocks;
            unsigned int xLoadTimeTicks;
            blocks = xPortLoadTask( ( xTaskHandle )pxLwrrentTCB, &xLoadTimeTicks );

            // break-out as soon as a task has been loaded
            if (blocks >= 0)
            {
#ifdef SCHEDULER_2X
                /* After being loaded task will run till the current timer tick
                expires and that can be any value between zero and the lenght of
                the timer tick. Extending task exelwtion for one or more timer
                ticks elliminates corner case where task runs closely to zero
                bringing falcon's thruput down (in worst case to zero as well). */

                // TODO: Next CL(s) will bring actual policy determining this value.
                pxLwrrentTCB->ucTicksToRun = 1 + xLoadTimeTicks;
#endif
                break;
            }
        }

        // update load failure counters
        uxLoadFailuresList++;
        uxLoadFailuresGlobal++;

        //
        // On the first load failure, save the current top-ready
        // priority. Save it so that it may be restored later after a
        // new (potentially lower-priority) task is found and loaded.
        //
        if (uxLoadFailuresGlobal == 1)
        {
            uxTopFailedPriority = uxTopReadyPriority;
        }

        //
        // If none of the tasks in the current ready-list could be
        // loaded, move on to the next ready-list.
        //
        if (uxLoadFailuresList == listLWRRENT_LIST_LENGTH(
                &( pxReadyTasksLists[ uxTopReadyPriority ] ) ))
        {
            --uxTopReadyPriority;
            uxLoadFailuresList = 0;
        }
    }

    // restore the top-ready priority of any load-failures oclwrred.
    if (uxLoadFailuresGlobal != 0)
    {
        uxTopReadyPriority = uxTopFailedPriority;
    }

    portFLCN_PRIV_LEVEL_SET_FROM_LWRR_TASK();
}
/*-----------------------------------------------------------*/

void vTaskPlaceOnEventList( xList *pxEventList, portTickType xTicksToWait )
{
portTickType xTimeToWake;

    /* THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED OR THE
    SCHEDULER SUSPENDED. */

    /* Place the event list item of the TCB in the appropriate event list.
    This is placed in the list in priority order so the highest priority task
    is the first to be woken by the event. */
    vListInsert( ( xList * ) pxEventList, ( xListItem * ) &( pxLwrrentTCB->xEventListItem ) );

    /* We must remove ourselves from the ready list before adding ourselves
    to the blocked list as the same list item is used for both lists.  We have
    exclusive access to the ready lists as the scheduler is locked. */
    vListRemove( ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );


    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        if( xTicksToWait == portMAX_DELAY )
        {
            /* Add ourselves to the suspended task list instead of a delayed task
            list to ensure we are not woken by a timing event.  We will block
            indefinitely. */
            vListInsertEnd( ( xList * ) &xSuspendedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
        }
        else
        {
            /* Callwlate the time at which the task should be woken if the event does
            not occur.  This may overflow but this doesn't matter. */
            xTimeToWake = xTickCount + xTicksToWait;

            listSET_LIST_ITEM_VALUE( &( pxLwrrentTCB->xGenericListItem ), xTimeToWake );

            if( xTimeToWake < xTickCount )
            {
                /* Wake time has overflowed.  Place this item in the overflow list. */
                vListInsert( ( xList * ) pxOverflowDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
            }
            else
            {
                /* The wake time has not overflowed, so we can use the current block list. */
                vListInsert( ( xList * ) pxDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
            }
        }
    }
    #else
    {
            /* Callwlate the time at which the task should be woken if the event does
            not occur.  This may overflow but this doesn't matter. */
            xTimeToWake = xTickCount + xTicksToWait;

            listSET_LIST_ITEM_VALUE( &( pxLwrrentTCB->xGenericListItem ), xTimeToWake );

            if( xTimeToWake < xTickCount )
            {
                /* Wake time has overflowed.  Place this item in the overflow list. */
                vListInsert( ( xList * ) pxOverflowDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
            }
            else
            {
                /* The wake time has not overflowed, so we can use the current block list. */
                vListInsert( ( xList * ) pxDelayedTaskList, ( xListItem * ) &( pxLwrrentTCB->xGenericListItem ) );
            }
    }
    #endif
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xTaskRemoveFromEventList( const xList *pxEventList )
{
tskTCB *pxUnblockedTCB;
portBASE_TYPE xReturn;

    /* THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED OR THE
    SCHEDULER SUSPENDED.  It can also be called from within an ISR. */

    /* The event list is sorted in priority order, so we can remove the
    first in the list, remove the TCB from the delayed list, and add
    it to the ready list.

    If an event is for a queue that is locked then this function will never
    get called - the lock count on the queue will get modified instead.  This
    means we can always expect exclusive access to the event list here. */
    pxUnblockedTCB = ( tskTCB * ) listGET_OWNER_OF_HEAD_ENTRY( pxEventList );
    vListRemove( &( pxUnblockedTCB->xEventListItem ) );

    if( uxSchedulerSuspended == ( unsigned portBASE_TYPE ) pdFALSE )
    {
        vListRemove( &( pxUnblockedTCB->xGenericListItem ) );
        prvAddTaskToReadyQueue( pxUnblockedTCB );
    }
    else
    {
        /* We cannot access the delayed or ready lists, so will hold this
        task pending until the scheduler is resumed. */
        vListInsertEnd( ( xList * ) &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );
    }

#ifndef SCHEDULER_2X
    if( pxUnblockedTCB->uxPriority >= pxLwrrentTCB->uxPriority )
#else
    if( pxUnblockedTCB->uxPriority > pxLwrrentTCB->uxPriority )
#endif
    {
        /* Return true if the task removed from the event list has
        a higher priority than the calling task.  This allows
        the calling task to know if it should force a context
        switch now. */
        xReturn = pdTRUE;
    }
    else
    {
        xReturn = pdFALSE;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

void vTaskSetTimeOutState( xTimeOutType *pxTimeOut )
{
    pxTimeOut->xOverflowCount = xNumOfOverflows;
    pxTimeOut->xTimeOnEntering = xTickCount;
}
/*-----------------------------------------------------------*/

portBASE_TYPE xTaskCheckForTimeOut( xTimeOutType *pxTimeOut, portTickType *pxTicksToWait )
{
portBASE_TYPE xReturn;

    if( ( xNumOfOverflows != pxTimeOut->xOverflowCount ) && ( xTickCount > pxTimeOut->xTimeOnEntering ) )
    {
        /* The tick count is greater than the time at which vTaskSetTimeout()
        was called, but has also overflowed since vTaskSetTimeOut() was called.
        It must have wrapped all the way around and gone past us again. This
        passed since vTaskSetTimeout() was called. */
        xReturn = pdTRUE;
    }
    else if( ( xTickCount - pxTimeOut->xTimeOnEntering ) < *pxTicksToWait )
    {
        /* Not a genuine timeout. Adjust parameters for time remaining. */
        *pxTicksToWait -= ( xTickCount - pxTimeOut->xTimeOnEntering );
        vTaskSetTimeOutState( pxTimeOut );
        xReturn = pdFALSE;
    }
    else
    {
        xReturn = pdTRUE;
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

void vTaskMissedYield( void )
{
    xMissedYield = pdTRUE;
}

/*
 * -----------------------------------------------------------
 * The Idle task.
 * ----------------------------------------------------------
 *
 * The portTASK_FUNCTION() macro is used to allow port/compiler specific
 * language extensions.  The equivalent prototype for this function is:
 *
 * void vIdleTask( void *pvParameters );
 *
 */
static portTASK_FUNCTION( vIdleTask, pvParameters )
{
    /* Stop warnings. */
    ( void ) pvParameters;

    for( ;; )
    {

        /* When using preemption tasks of equal priority will be
        timesliced.  If a task that is sharing the idle priority is ready
        to run then the idle task should yield before the end of the
        timeslice.

        A critical region is not required here as we are just reading from
        the list, and an occasional incorrect value will not matter.  If
        the ready list at the idle priority contains more than one task
        then a task other than the idle task is ready to execute. */
        if( listLWRRENT_LIST_LENGTH( &( pxReadyTasksLists[ tskIDLE_PRIORITY ] ) ) > ( unsigned portBASE_TYPE ) 1 )
        {
            taskYIELD();
        }

        #if ( configUSE_IDLE_HOOK == 1 )
        {
            /* Call the user defined function from within the idle task.  This
            allows the application designer to add background functionality
            without the overhead of a separate task.
            NOTE: lwrtosHookIdle() MUST NOT, UNDER ANY CIRLWMSTANCES,
            CALL A FUNCTION THAT MIGHT BLOCK. */
            lwrtosHookIdle();
        }
        #endif
    }
} /*lint !e715 pvParameters is not accessed but all task functions require the same prototype. */

/*-----------------------------------------------------------
 * File private functions dolwmented at the top of the file.
 *----------------------------------------------------------*/

static void prvInitialiseTCBVariables( tskTCB *pxTCB, unsigned portCHAR ucTaskID, unsigned portBASE_TYPE uxPriority, void *pvTcbPvt )
{
    pxTCB->ucTaskID = ucTaskID;
#ifdef SCHEDULER_2X
    pxTCB->ucTicksToRun = 0;
#endif

    /* This is used as an array index so must ensure it's not too large. */
    if( uxPriority >= configMAX_PRIORITIES )
    {
            //
            // The original source would simply truncate the priority down to the
            // maximum priority defined which leads to hard to catch errors.
            // It is more appropriate to simply ABORT.
            //
            configABORT("Task priority exceeds maximum allowed");
    }

    pxTCB->uxPriority = uxPriority;
    pxTCB->pvTcbPvt   = pvTcbPvt;

    vListInitialiseItem( &( pxTCB->xGenericListItem ) );
    vListInitialiseItem( &( pxTCB->xEventListItem ) );

    /* Set the pxTCB as a link back from the xListItem.  This is so we can get
    back to the containing TCB from a generic item in a list. */
    listSET_LIST_ITEM_OWNER( &( pxTCB->xGenericListItem ), pxTCB );

    /* Event lists are always in priority order. */
    listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), configMAX_PRIORITIES - ( portTickType ) uxPriority );
    listSET_LIST_ITEM_OWNER( &( pxTCB->xEventListItem ), pxTCB );
}
/*-----------------------------------------------------------*/

static void prvInitialiseTaskLists( void )
{
unsigned portBASE_TYPE uxPriority;

    for( uxPriority = 0; uxPriority < configMAX_PRIORITIES; uxPriority++ )
    {
        vListInitialise( ( xList * ) &( pxReadyTasksLists[ uxPriority ] ) );
    }

    vListInitialise( ( xList * ) &xDelayedTaskList1 );
    vListInitialise( ( xList * ) &xDelayedTaskList2 );
    vListInitialise( ( xList * ) &xPendingReadyList );

    #if ( INCLUDE_vTaskSuspend == 1 )
    {
        vListInitialise( ( xList * ) &xSuspendedTaskList );
    }
    #endif

    /* Start with pxDelayedTaskList using list1 and the pxOverflowDelayedTaskList
    using list2. */
    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
/*-----------------------------------------------------------*/

xTaskHandle xTaskGetLwrrentTaskHandle( void )
{
xTaskHandle xReturn;

    xReturn = ( xTaskHandle ) pxLwrrentTCB;

    return xReturn;
}
/*-----------------------------------------------------------*/

/*!
 * @brief   Returns total number of ready tasks present in task pool.
 *
 * Note: We are not using critical section to compute total task in
 *       ready list because state of ready list changes dynamically.
 *       E.g. state can change even after summing-up all ready lists.
 *       Its up to the caller whether to use critical section or not.
 *
 * @return  Number of ready tasks
 */
unsigned portBASE_TYPE uxTaskReadyTaskCountGetFromISR( void )
{
unsigned portBASE_TYPE index;
unsigned portBASE_TYPE uxReadyTaskCount = 0;

    for( index = 0; index <= uxTopReadyPriority; index++ )
    {
        uxReadyTaskCount += pxReadyTasksLists[index].uxNumberOfItems;
    }

    return uxReadyTaskCount;

}
/*-----------------------------------------------------------*/

