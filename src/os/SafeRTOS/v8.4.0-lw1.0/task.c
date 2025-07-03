/*
    Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

    This file is part of the SafeRTOS product, see projdefs.h for version number
    information.

    SafeRTOS is distributed exclusively by WITTENSTEIN high integrity systems,
    and is subject to the terms of the License granted to your organization,
    including its warranties and limitations on use and distribution. It cannot be
    copied or reproduced in any way except as permitted by the License.

    Licenses authorize use by processor, compiler, business unit, and product.

    WITTENSTEIN high integrity systems is a trading name of WITTENSTEIN
    aerospace & simulation ltd, Registered Office: Brown's Court, Long Ashton
    Business Park, Yanley Lane, Long Ashton, Bristol, BS41 9LB, UK.
    Tel: +44 (0) 1275 395 600, fax: +44 (0) 1275 393 630.
    E-mail: info@HighIntegritySystems.com

    www.HighIntegritySystems.com
*/

/* Scheduler includes. */
#define KERNEL_SOURCE_FILE
#include "SafeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "eventgroups.h"
#include "mutex.h"
#include "eventpoll.h"


/* Constant Definitions -----------------------------------------------------*/

/* Global Variables ---------------------------------------------------------*/

/* pxLwrrentTCB points to the xTCB structure of the lwrrently exelwting task
 * (the Ready state task). This can only be null prior to any tasks being
 * created. */
KERNEL_DATA xTCB *volatile pxLwrrentTCB = NULL;

/* Lists for ready and blocked tasks. ---------------------------------------*/

KERNEL_DATA xList xReadyTasksLists[ configMAX_PRIORITIES ] = { { 0U, NULL, { 0U, NULL, NULL, NULL } } }; /* Prioritised ready tasks. */
KERNEL_DATA static xList xDelayedTaskList1 = { 0U };                        /* Delayed tasks. */
KERNEL_DATA static xList xDelayedTaskList2 = { 0U };                        /* Delayed tasks (two lists are used - one for delays that have overflowed the current tick count. */
KERNEL_DATA static xList *volatile pxDelayedTaskList = NULL;                /* Points to the delayed task list lwrrently being used. */
KERNEL_DATA static xList *volatile pxOverflowDelayedTaskList = NULL;        /* Points to the delayed task list lwrrently being used to hold tasks that have overflowed the current tick count. */
KERNEL_DATA xList xPendingReadyList = { 0U };                               /* Tasks that have been readied while the scheduler was suspended.  They will be moved to the ready queue when the scheduler is resumed. */
KERNEL_DATA static xList xSuspendedTaskList = { 0U };                       /* Tasks that are lwrrently suspended. */


/* File private variables. --------------------------------------------------*/

KERNEL_DATA static volatile portUnsignedBaseType uxLwrrentNumberOfTasks = 0U;
KERNEL_DATA static volatile portUnsignedBaseType uxTopReadyPriority     = taskIDLE_PRIORITY;
KERNEL_DATA static portBaseType xSchedulerRunning                       = pdFALSE;
KERNEL_DATA static volatile portUnsignedBaseType uxSchedulerSuspended   = 0U;
KERNEL_DATA static volatile portBaseType xMissedYield                   = pdFALSE;
KERNEL_DATA static volatile portUnsignedBaseType uxNumOfOverflows       = 0U;

KERNEL_DATA volatile portUnsignedBaseType uxMissedTicks                 = 0U;
KERNEL_DATA volatile portTickType xTickCount                            = ( portTickType ) 0U;
KERNEL_DATA_MIRROR volatile portTickType xTickCountMirror               = ( portTickType ) 0U;
KERNEL_DATA volatile portTickType xNextTaskUnblockTime                  = ( portTickType ) 0U; /* Initialised to portMAX_DELAY before the scheduler starts. */


/* File private functions. --------------------------------------------------*/

/*
 * Utility to ready all the lists used by the scheduler.  This is called
 * automatically upon the creation of the first task.
 */
KERNEL_INIT_FUNCTION static void prvInitialiseTaskLists( void );

/*
 * Checks the validity of the parameters passed to xTaskCreate.  Returns
 * greater than zero only if all parameters are valid.
 */
KERNEL_CREATE_FUNCTION static portBaseType prvCheckTaskCreateParameters( const xTaskParameters *const pxTaskParameters );

/*
 * Process a system tick increment.
 */
KERNEL_FUNCTION static portBaseType prvTaskIncrementTick( void );

/* MCDC Test Point: PROTOTYPE */

/*-----------------------------------------------------------------------------
 * TASK CREATION API dolwmented in task.h
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xTaskCreate( const xTaskParameters *const pxTaskParameters, portTaskHandleType *pxCreatedTask )
{
    portBaseType xReturn;
    xTCB *pxNewTCB;

    /* Check the validity of the supplied parameters. */
    xReturn = prvCheckTaskCreateParameters( pxTaskParameters );

    if( pdPASS == xReturn )
    {
        /* Set a pointer to the TCB buffer. */
        pxNewTCB = pxTaskParameters->pxTCB;

        /*
         * Initialise the TCB and then setup the task stack to look as if the
         * task was already running, but had been interrupted by the scheduler.
         * The return address is set to the start of the task function. Once the
         * stack has been initialised the top of stack variable is updated to
         * point to the top after the initial context has been placed on the
         * stack.
         */
        vPortInitialiseTask( pxTaskParameters );

        /* We are going to manipulate the task lists to add this task to a
        ready list, so must make sure no interrupts occur. */
        portENTER_CRITICAL_WITHIN_API();
        {
            if( NULL != pxCreatedTask )
            {
                /* Pass the TCB out - in an anonymous way.
                 * The calling function/task can use this as a handle to the
                 * task later if required. */
                if( pdPASS != portCOPY_DATA_TO_TASK( pxCreatedTask, &pxNewTCB, ( portUnsignedBaseType ) sizeof( portTaskHandleType ) ) )
                {
                    xReturn = errILWALID_DATA_RANGE;
                    /* MCDC Test Point: STD_IF "xTaskCreate" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskCreate" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskCreate" */

            if( pdPASS == xReturn )
            {
                uxLwrrentNumberOfTasks++;

                /* Is this the first task to be created? */
                if( 1U == uxLwrrentNumberOfTasks )
                {
                    /* As this is the first task it must also be the current task. */
                    pxLwrrentTCB = pxNewTCB;

                    /* MCDC Test Point: STD_IF "xTaskCreate" */
                }
                else
                {
                    /* If the scheduler is not already running, make this task the
                    current task if it is the highest priority task to be created
                    so far. */
                    if( pdFALSE == xSchedulerRunning )
                    {
                        if( pxLwrrentTCB->uxPriority <= pxTaskParameters->uxPriority )
                        {
                            pxLwrrentTCB = pxNewTCB;

                            /* MCDC Test Point: STD_IF "xTaskCreate" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xTaskCreate" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskCreate" */

                    /* MCDC Test Point: STD_ELSE "xTaskCreate" */
                }

                /* SAFERTOSTRACE TASKCREATE */

                vTaskAddTaskToReadyList( pxNewTCB );
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        if( pdFALSE != xSchedulerRunning )
        {
            /* If the created task is of a higher priority than the current
             * task then it should run now. */
            if( pxLwrrentTCB->uxPriority < pxTaskParameters->uxPriority )
            {
                taskYIELD_WITHIN_API();

                /* MCDC Test Point: STD_IF "xTaskCreate" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskCreate" */
        }
        /* MCDC Test Point: ADD_ELSE "xTaskCreate" */
    }
    /* MCDC Test Point: ADD_ELSE "xTaskCreate" */

    /* SAFERTOSTRACE TASKCREATE_FAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_DELETE_FUNCTION portBaseType xTaskDelete( portTaskHandleType xTaskToDelete )
{
    xTCB *pxTCB;
    portBaseType xReturn;

    /* Ensure a yield is performed if the current task is being deleted. */
    if( xTaskToDelete == pxLwrrentTCB )
    {
        xTaskToDelete = NULL;

        /* MCDC Test Point: STD_IF "xTaskDelete" */
    }
    /* MCDC Test Point: ADD_ELSE "xTaskDelete" */

    /* If null is passed in here then we are deleting ourselves. */
    /* MCDC Test Point: EXP_IF_MACRO "taskGET_TCB_FROM_HANDLE(include/task.h)" "NULL == xTaskToDelete" */
    pxTCB = taskGET_TCB_FROM_HANDLE( xTaskToDelete );

    portENTER_CRITICAL_WITHIN_API();
    {
        if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
        {
            xReturn = errILWALID_TASK_HANDLE;

            /* MCDC Test Point: STD_IF "xTaskDelete" */
        }
        else
        {
            xReturn = pdPASS;

            /* If we hold any mutexes then we must release them. */
            vMutexReleaseAll( pxTCB );

            /* If this task owns any event poll objects, they must be left in a
             * sensible state with all links to target objects removed. */
            vEventPollTaskDeleted( pxTCB );

            /* This will stop the task from be scheduled. */
            vListRemove( &( pxTCB->xStateListItem ) );

            /* Is the task waiting on an event also? */
            if( xListIsContainedWithinAList( &( pxTCB->xEventListItem ) ) != pdFALSE )
            {
                /* Remove ourselves from the list, and re-evaluate any priority inheritance if the list is a mutex. */
                vQueueRemoveListItemAndCheckInheritance( &( pxTCB->xEventListItem ) );

                /* MCDC Test Point: STD_IF "xTaskDelete" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskDelete" */

            /* Mark the task stack as 'not in use'. */
            *( pxTCB->pxStackInUseMarker ) = portSTACK_NOT_IN_USE;

            /* Ilwalidate the task handle. */
            pxTCB->uxStackLimitMirror = ( portUnsignedBaseType ) pxTCB->pxStackLimit;

            /* There is now one less task! */
            uxLwrrentNumberOfTasks--;

            /* Let the host application know that the memory can be used
            again. */
            vPortTaskDeleteHook( pxTCB );

            /* Reset the next expected unblock time in case it referred to the task that
             * has just been deleted. */
            vTaskResetNextTaskUnblockTime();

            /* SAFERTOSTRACE TASKDELETE */

            /* Force a reschedule if we have just deleted the current task.  The
            task will never run again after this yield as it no longer exists in
            the system. */
            if( NULL == xTaskToDelete )
            {
                taskYIELD_WITHIN_API();

                /* MCDC Test Point: STD_IF "xTaskDelete" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskDelete" */
            /* MCDC Test Point: STD_ELSE "xTaskDelete" */
        }

    }
    portEXIT_CRITICAL_WITHIN_API();

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskDelayUntil( portTickType *pxPreviousWakeTime, portTickType xTimeIncrement )
{
    portTickType xTimeToWake;
    portTickType xLocalTickCount;
    portBaseType xAlreadyYielded;
    portBaseType xReturn;

    if( 0U != uxSchedulerSuspended )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xTaskDelayUntil" */
    }
    else if( NULL == pxPreviousWakeTime )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xTaskDelayUntil" */
    }
    else
    {
        vTaskSuspendScheduler();
        {
            xReturn = errDID_NOT_YIELD;

            /* Cache xTickCount, as it is volatile (the scheduler is suspended,
             * so xTickCount will not change). */
            xLocalTickCount = xTickCount;

            /* Generate the tick time at which the task wants to wake. */
            xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

            if( xLocalTickCount < *pxPreviousWakeTime )
            {
                /* The tick count has overflowed since this function was
                lasted called.  In this case the only time we should ever
                actually delay is if the wake time has also overflowed,
                and the wake time is greater than the tick time. When this
                is the case it is as if neither time had overflowed. */
                if( ( xTimeToWake < *pxPreviousWakeTime ) && ( xTimeToWake > xLocalTickCount ) )
                {
                    xReturn = pdPASS;

                    /* MCDC Test Point: STD_IF "xTaskDelayUntil" */
                }
                /* MCDC Test Point: EXP_IF_AND "xTaskDelayUntil" "( xTimeToWake < *pxPreviousWakeTime )" "( xTimeToWake > xLocalTickCount )" "Deviate: FF" */
            }
            else
            {
                /* The tick time has not overflowed.  In this case we will
                delay if either the wake time has overflowed, and/or the
                tick time is less than the wake time. */
                if( ( xTimeToWake < *pxPreviousWakeTime ) || ( xTimeToWake > xLocalTickCount ) )
                {
                    xReturn = pdPASS;

                    /* MCDC Test Point: STD_IF "xTaskDelayUntil" */
                }
                /* MCDC Test Point: EXP_IF_OR "xTaskDelayUntil" "( xTimeToWake < *pxPreviousWakeTime )" "( xTimeToWake > xLocalTickCount )" "Deviate: TT" */
                /* MCDC Test Point: STD_ELSE "xTaskDelayUntil" */
            }

            /* Update the wake time ready for the next call. */
            if( pdPASS != portCOPY_DATA_TO_TASK( pxPreviousWakeTime, &xTimeToWake, ( portUnsignedBaseType ) sizeof( portTickType ) ) )
            {
                xReturn = errILWALID_DATA_RANGE;
                /* MCDC Test Point: STD_IF "xTaskDelayUntil" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskDelayUntil" */

            if( pdPASS == xReturn )
            {
                /* SAFERTOSTRACE TASKDELAYUNTIL */

                /* vTaskAddLwrrentTaskToDelayedList() needs the block time, not
                 * the time to wake, so subtract the current tick count. */
                vTaskAddLwrrentTaskToDelayedList( xTimeToWake - xLocalTickCount );

                /* MCDC Test Point: STD_IF "xTaskDelayUntil" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskDelayUntil" */
        }
        xAlreadyYielded = xTaskResumeScheduler();

        /* Force a reschedule if xTaskResumeScheduler has not already done so, we may
        have put ourselves to sleep. */
        if( ( pdFALSE == xAlreadyYielded ) && ( pdPASS == xReturn ) )
        {
            taskYIELD_WITHIN_API();

            /* MCDC Test Point: STD_IF "xTaskDelayUntil" */
        }
        /* MCDC Test Point: EXP_IF_AND "xTaskDelayUntil" "( pdFALSE == xAlreadyYielded )" "( pdPASS == xReturn )" "Deviate: FF" */
        /* MCDC Test Point: STD_ELSE "xTaskDelayUntil" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskDelay( portTickType xTicksToDelay )
{
    portBaseType xAlreadyYielded = pdFALSE;
    portBaseType xReturn;

    if( 0U != uxSchedulerSuspended )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xTaskDelay" */
    }
    else
    {
        xReturn = pdPASS;

        /* A delay time of zero just forces a reschedule. */
        if( xTicksToDelay > ( portTickType ) 0U )
        {
            vTaskSuspendScheduler();
            {
                /* SAFERTOSTRACE TASKDELAY */

                /* A task that is removed from the event list while the
                 * scheduler is suspended will not get placed in the ready list
                 * or removed from the blocked list until the scheduler is
                 * resumed.
                 * This task cannot be in an event list as it is the lwrrently
                 * exelwting task. */
                vTaskAddLwrrentTaskToDelayedList( xTicksToDelay );

                /* MCDC Test Point: STD_IF "xTaskDelay" */
            }
            xAlreadyYielded = xTaskResumeScheduler();
        }
        /* MCDC Test Point: ADD_ELSE "xTaskDelay" */

        /* Force a reschedule if xTaskResumeScheduler has not already done so, we may
        have put ourselves to sleep. */
        if( pdFALSE == xAlreadyYielded )
        {
            taskYIELD_WITHIN_API();

            /* MCDC Test Point: STD_IF "xTaskDelay" */
        }
        /* MCDC Test Point: ADD_ELSE "xTaskDelay" */
        /* MCDC Test Point: STD_ELSE "xTaskDelay" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskPriorityGet( portTaskHandleType xTask, portUnsignedBaseType *puxPriority )
{
    xTCB *pxTCB;
    portBaseType xReturn;

    if( NULL == puxPriority )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xTaskPriorityGet" */
    }
    else
    {
        /* If null is passed in here then we obtaining the priority of the
         * calling task. */
        pxTCB = taskGET_TCB_FROM_HANDLE( xTask );

        portENTER_CRITICAL_WITHIN_API();
        {
            if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
            {
                xReturn = errILWALID_TASK_HANDLE;

                /* MCDC Test Point: STD_IF "xTaskPriorityGet" */
            }
            else
            {
                if( pdPASS != portCOPY_DATA_TO_TASK( puxPriority, &( pxTCB->uxPriority ), ( portUnsignedBaseType ) sizeof( portUnsignedBaseType ) ) )
                {
                    xReturn = errILWALID_DATA_RANGE;
                    /* MCDC Test Point: STD_IF "xTaskPriorityGet" */
                }
                else
                {
                    /* The priority has been successfully retrieved. */
                    xReturn = pdPASS;

                    /* MCDC Test Point: STD_ELSE "xTaskPriorityGet" */
                }

                /* MCDC Test Point: STD_ELSE "xTaskPriorityGet" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xTaskPriorityGet" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskPrioritySet( portTaskHandleType xTask, portUnsignedBaseType uxNewPriority )
{
    xTCB *pxTCB;
    portUnsignedBaseType uxLwrrentPriority;
    portBaseType xReturn;

    if( uxNewPriority >= configMAX_PRIORITIES )
    {
        xReturn = errILWALID_PRIORITY;

        /* MCDC Test Point: STD_IF "xTaskPrioritySet" */
    }
    else
    {
        /* If null is passed in here then we are changing the priority of the
         * current task. */
        pxTCB = taskGET_TCB_FROM_HANDLE( xTask );

        portENTER_CRITICAL_WITHIN_API();
        {
            if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
            {
                xReturn = errILWALID_TASK_HANDLE;

                /* MCDC Test Point: STD_IF "xTaskPrioritySet" */
            }
            else
            {
                xReturn = pdPASS;

                /* SAFERTOSTRACE TASKPRIORITYSET */

                /* Save the current priority so we know which ready list we are
                 * lwrrently located in. */
                uxLwrrentPriority = pxTCB->uxPriority;

                /* If any mutexes are held then the situation is more complex
                 * as we have to consider priority inheritance. Even if the task
                 * is not lwrrently using an inherited priority it is possible
                 * that the change in priority will put us into a situation
                 * where inheritance is required. */

                /* The base priority gets set whether or not the change will
                 * be applied immediately. */
                pxTCB->uxBasePriority = uxNewPriority;

                /* uxMutexFindHighestBlockedTaskPriority() checks against uxBasePriority.
                 * We will never return a priority which is lower than the base priority.*/
                pxTCB->uxPriority = uxMutexFindHighestBlockedTaskPriority( pxTCB );

                /* If the actual priority used by the task has changed then we
                 * need to actually apply the change. */
                if( pxTCB->uxPriority != uxLwrrentPriority )
                {
                    /* If the event list item is not being used for a secondary
                     * purpose (such as blocking on an event group), it can be
                     * updated now. Otherwise, it will be updated when the task
                     * unblocks. */
                    if( 0U == ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) )
                    {
                        /* If the task is blocked on a queue, semaphore or mutex
                         * then we should relocate it to ensure that it is queued
                         * in correctly in priority order. */
                        vListRelocateOrderedItem( &( pxTCB->xEventListItem ), ( portTickType )( configMAX_PRIORITIES - uxNewPriority ) );

                        /* MCDC Test Point: STD_IF "xTaskPrioritySet" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskPrioritySet" */

                    /* If the task is in the blocked or suspended list we need
                     * do nothing more than change its priority variable.
                     * However, if the task is in a ready list it needs to be
                     * removed and placed in the queue appropriate to its new
                     * priority. */
                    if( xListIsContainedWithin( &( xReadyTasksLists[ uxLwrrentPriority ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
                    {
                        /* The task is lwrrently in its ready list.
                        Remove before adding to its new ready list. */
                        vListRemove( &( pxTCB->xStateListItem ) );

                        vTaskAddTaskToReadyList( pxTCB );

                        /* MCDC Test Point: STD_IF "xTaskPrioritySet" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskPrioritySet" */

                    /* The priority change may have readied a task of higher
                     * priority than the calling task. */
                    taskYIELD_WITHIN_API();
                }
                /* MCDC Test Point: ADD_ELSE "xTaskPrioritySet" */

                /* MCDC Test Point: STD_ELSE "xTaskPrioritySet" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xTaskPrioritySet" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskSuspend( portTaskHandleType xTaskToSuspend )
{
    xTCB *pxTCB;
    portBaseType xReturn;

    if( 0U != uxSchedulerSuspended )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xTaskSuspend" */
    }
    else
    {
        /* Ensure a yield is performed if the current task is being
         * suspended. */
        if( xTaskToSuspend == pxLwrrentTCB )
        {
            xTaskToSuspend = NULL;

            /* MCDC Test Point: STD_IF "xTaskSuspend" */
        }
        /* MCDC Test Point: ADD_ELSE "xTaskSuspend" */

        /* If null is passed in here then we are suspending ourselves. */
        pxTCB = taskGET_TCB_FROM_HANDLE( xTaskToSuspend );

        portENTER_CRITICAL_WITHIN_API();
        {
            if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
            {
                xReturn = errILWALID_TASK_HANDLE;

                /* MCDC Test Point: STD_IF "xTaskSuspend" */
            }
            else if( xListIsContainedWithin( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
            {
                xReturn = errTASK_ALREADY_SUSPENDED;

                /* MCDC Test Point: STD_ELSE_IF "xTaskSuspend" */
            }
            else
            {
                xReturn = pdPASS;

                /* SAFERTOSTRACE TASKSUSPEND */

                /* Remove task from the ready/delayed list and place in the suspended list. */
                vListRemove( &( pxTCB->xStateListItem ) );

                /* Is the task waiting on an event also? */
                if( xListIsContainedWithinAList( &( pxTCB->xEventListItem ) ) != pdFALSE )
                {
                    vQueueRemoveListItemAndCheckInheritance( &( pxTCB->xEventListItem ) );

                    /* MCDC Test Point: STD_IF "xTaskSuspend" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskSuspend" */

                vListInsertEnd( &xSuspendedTaskList, &( pxTCB->xStateListItem ) );

                /* Reset the next expected unblock time in case it referred to
                 * the task that is now in the Suspended state. */
                vTaskResetNextTaskUnblockTime();

                /* We may have just suspended the current task. */
                if( NULL == xTaskToSuspend )
                {
                    taskYIELD_WITHIN_API();

                    /* MCDC Test Point: STD_IF "xTaskSuspend" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskSuspend" */
                /* MCDC Test Point: STD_ELSE "xTaskSuspend" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xTaskSuspend" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskResume( portTaskHandleType xTaskToResume )
{
    xTCB *pxTCB;
    portBaseType xYieldRequired;
    portBaseType xReturn;

    /* Remove the task from whichever list it is lwrrently in, and place
    it in the ready list. */
    pxTCB = ( xTCB * ) xTaskToResume;

    if( NULL == pxTCB )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xTaskResume" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
            {
                xReturn = errILWALID_TASK_HANDLE;

                /* MCDC Test Point: STD_IF "xTaskResume" */
            }
            else if( xListIsContainedWithin( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) == pdFALSE )
            {
                xReturn = errTASK_WAS_NOT_SUSPENDED;

                /* MCDC Test Point: STD_ELSE_IF "xTaskResume" */
            }
            else
            {
                xReturn = pdPASS;

                /* SAFERTOSTRACE TASKRESUME */

                if( pxTCB->uxPriority >= pxLwrrentTCB->uxPriority )
                {
                    xYieldRequired = pdTRUE;

                    /* MCDC Test Point: STD_IF "xTaskResume" */
                }
                else
                {
                    xYieldRequired = pdFALSE;

                    /* MCDC Test Point: STD_ELSE "xTaskResume" */
                }

                vListRemove( &( pxTCB->xStateListItem ) );
                vTaskAddTaskToReadyList( pxTCB );

                /* We may have just resumed a higher priority task. */
                if( xYieldRequired != pdFALSE )
                {
                    /* This yield may not cause the task just resumed to run, but
                    will leave the lists in the correct state for the next yield. */
                    taskYIELD_WITHIN_API();

                    /* MCDC Test Point: STD_IF "xTaskResume" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskResume" */
                /* MCDC Test Point: STD_ELSE "xTaskResume" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xTaskResume" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_INIT_FUNCTION portBaseType xTaskInitializeScheduler( const xPORT_INIT_PARAMETERS *const pxPortInitParameters )
{
    portBaseType xReturn;

    pxLwrrentTCB = NULL;
    vPortSetWordAlignedBuffer( ( void * ) xReadyTasksLists, 0U, ( portUnsignedBaseType ) sizeof( xReadyTasksLists ) );
    vPortSetWordAlignedBuffer( &xDelayedTaskList1, 0U, ( portUnsignedBaseType ) sizeof( xDelayedTaskList1 ) );
    vPortSetWordAlignedBuffer( &xDelayedTaskList2, 0U, ( portUnsignedBaseType ) sizeof( xDelayedTaskList2 ) );
    pxDelayedTaskList = NULL;
    pxOverflowDelayedTaskList = NULL;
    vPortSetWordAlignedBuffer( &xPendingReadyList, 0U, ( portUnsignedBaseType ) sizeof( xPendingReadyList ) );
    vPortSetWordAlignedBuffer( &xSuspendedTaskList, 0U, ( portUnsignedBaseType ) sizeof( xSuspendedTaskList ) );
    uxLwrrentNumberOfTasks = 0U;
    xTickCount = ( portTickType ) 0U;
    xTickCountMirror = ( portTickType ) 0U;
    uxTopReadyPriority = taskIDLE_PRIORITY;
    xSchedulerRunning = pdFALSE;
    uxSchedulerSuspended = 0U;
    uxMissedTicks = 0U;
    xMissedYield = pdFALSE;
    uxNumOfOverflows = 0U;

    /* Initialise the task lists ahead of the first task being created. */
    prvInitialiseTaskLists();

    /* Initialise run-time statistics here. */
    vInitialiseRunTimeStatistics();

    /* Initialise the port specific parameters. */
    xReturn = xPortInitialize( pxPortInitParameters );

    if( pdPASS == xReturn )
    {
        /* Initialise the timer task and command queue. */
        xReturn = xTimerInitialiseFeature( NULL,    /* use the default timer instance */
                                           pxPortInitParameters );

        /* MCDC Test Point: STD_IF "xTaskInitializeScheduler" */
    }
    /* MCDC Test Point: ADD_ELSE "xTaskInitializeScheduler" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_INIT_FUNCTION portBaseType xTaskStartScheduler( portBaseType xUseKernelConfigurationChecks )
{
    portBaseType xReturn;

    /* Check a task has been created. */
    if( NULL == pxLwrrentTCB )
    {
        xReturn = errNO_TASKS_CREATED;

        /* MCDC Test Point: STD_IF "xTaskStartScheduler" */
    }
    else if( xSchedulerRunning != pdFALSE )
    {
        xReturn = errSCHEDULER_ALREADY_RUNNING;

        /* MCDC Test Point: STD_ELSE_IF "xTaskStartScheduler" */
    }
    else
    {
        /* Interrupts are turned off here, to ensure a tick does not occur
        before or during the call to xPortStartScheduler().  The stacks of the
        created tasks contain a status word with interrupts switched on so
        interrupts will automatically get re-enabled when the first task starts
        to run.

        STEPPING THROUGH HERE USING A DEBUGGER CAN CAUSE BIG PROBLEMS IF THE
        DEBUGGER ALLOWS INTERRUPTS TO BE PROCESSED. */
        portSET_INTERRUPT_MASK();

        xNextTaskUnblockTime = portMAX_DELAY;
        xSchedulerRunning = pdTRUE;
        xTickCount = ( portTickType ) 0U;
        xTickCountMirror = ~( ( portTickType ) 0U );

        /* SAFERTOSTRACE TASKSWITCHEDIN */

        /* Setting up the timer tick is hardware specific and thus in the
        portable interface. */
        xReturn = xPortStartScheduler( xUseKernelConfigurationChecks );

        /* We will only get here if xPortStartScheduler() returned an error
        code. */
        xSchedulerRunning = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xTaskStartScheduler" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskSuspendScheduler( void )
{
    portENTER_CRITICAL_WITHIN_API();
    {
        ++uxSchedulerSuspended;

        /* MCDC Test Point: STD "vTaskSuspendScheduler" */
    }
    portEXIT_CRITICAL_WITHIN_API();
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskResumeScheduler( void )
{
    xTCB *pxTCB = NULL;
    portBaseType xAlreadyYielded = pdFALSE;
    portBaseType xYieldRequired = pdFALSE;
    portUnsignedBaseType uxLocalMissedTicks;
    portBaseType xLocalMissedYield;

    if( 0U == uxSchedulerSuspended )
    {
        xAlreadyYielded = errSCHEDULER_WAS_NOT_SUSPENDED;

        /* MCDC Test Point: STD_IF "xTaskResumeScheduler" */
    }
    else
    {
        /* It is possible that an ISR caused a task to be removed from an event
        list while the scheduler was suspended.  If this was the case then the
        removed task will have been added to the xPendingReadyList.  Once the
        scheduler has been resumed it is safe to move all the pending ready
        tasks from this list into their appropriate ready list. */
        portENTER_CRITICAL_WITHIN_API();
        {
            --uxSchedulerSuspended;

            if( 0U == uxSchedulerSuspended )
            {
                if( uxLwrrentNumberOfTasks > ( portUnsignedBaseType ) 0U )
                {
                    /* Move any readied tasks from the pending list into the
                     * appropriate ready list. */

                    /* MCDC Test Point: WHILE_EXTERNAL "xTaskResumeScheduler" "( 0U != listLWRRENT_LIST_LENGTH( &xPendingReadyList ) )" */
                    while( 0U != listLWRRENT_LIST_LENGTH( &xPendingReadyList ) )
                    {
                        /* Get the next task in the Pending list.
                         * We have already checked that the list is not empty,
                         * so we can use the guaranteed version. */
                        pxTCB = ( xTCB * ) listGUARANTEED_GET_OWNER_OF_HEAD_ENTRY( &xPendingReadyList );

                        /* Remove the task from all lists and add it to the
                         * Ready list. */
                        vListRemove( &( pxTCB->xEventListItem ) );
                        vListRemove( &( pxTCB->xStateListItem ) );
                        vTaskAddTaskToReadyList( pxTCB );

                        /* If we have moved a task that has a priority higher than
                        the current task then we should yield. */
                        if( pxTCB->uxPriority > pxLwrrentTCB->uxPriority )
                        {
                            xYieldRequired = pdTRUE;

                            /* MCDC Test Point: STD_IF "xTaskResumeScheduler" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xTaskResumeScheduler" */

                        /* MCDC Test Point: WHILE_INTERNAL "xTaskResumeScheduler" "( 0U != listLWRRENT_LIST_LENGTH( &xPendingReadyList ) )" */
                    }

                    /* Was any task unblocked? */
                    if( NULL != pxTCB )
                    {
                        /* A task was unblocked while the scheduler was
                         * suspended, which may have prevented the next unblock
                         * time from being re-callwlated, in which case
                         * re-callwlate it now. */
                        vTaskResetNextTaskUnblockTime();

                        /* MCDC Test Point: STD_IF "xTaskResumeScheduler" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskResumeScheduler" */

                    /* If any ticks oclwrred while the scheduler was suspended,
                     * then they should be processed now.
                     * This ensures the tick count does not slip, and that any
                     * delayed tasks are resumed at the correct time. */

                    uxLocalMissedTicks = uxMissedTicks; /* cache for performance. */
                    if( uxLocalMissedTicks > ( portUnsignedBaseType ) 0U )
                    {
                        do
                        {
                            if( prvTaskIncrementTick() != pdFALSE )
                            {
                                xYieldRequired = pdTRUE;
                                /* MCDC Test Point: STD_IF "xTaskResumeScheduler" */
                            }
                            /* MCDC Test Point: ADD_ELSE "xTaskResumeScheduler" */

                            --uxLocalMissedTicks;

                            /* MCDC Test Point: WHILE_INTERNAL "xTaskResumeScheduler" "( uxLocalMissedTicks > ( portUnsignedBaseType ) 0U )" */
                        } while( uxLocalMissedTicks > ( portUnsignedBaseType ) 0U );

                        uxMissedTicks = 0U;
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskResumeScheduler" */

                    xLocalMissedYield = xMissedYield;   /* cache for performance. */

                    /* MCDC Test Point: EXP_IF_OR "xTaskResumeScheduler" "( pdTRUE == xYieldRequired )" "( pdTRUE == xLocalMissedYield )" */
                    if( ( pdTRUE == xYieldRequired ) || ( pdTRUE == xLocalMissedYield ) )
                    {
                        xAlreadyYielded = pdTRUE;
                        xMissedYield = pdFALSE;
                        taskYIELD_WITHIN_API();

                        /* MCDC Test Point: NULL "xTaskResumeScheduler" */
                    }
                }
                /* MCDC Test Point: ADD_ELSE "xTaskResumeScheduler" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskResumeScheduler" */
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_ELSE "xTaskResumeScheduler" */
    }

    return xAlreadyYielded;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portTickType xTaskGetTickCount( void )
{
    portTickType xTicks;

    /* Critical section required if running on a 16 bit processor. */
    portTICK_TYPE_ENTER_CRITICAL();
    {
        xTicks = xTickCount;
        /* MCDC Test Point: STD "xTaskGetTickCount" */
    }
    portTICK_TYPE_EXIT_CRITICAL();

    return xTicks;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portTickType xTaskGetTickCountFromISR( void )
{
    portTickType xReturn;
    portUnsignedBaseType uxSavedInterruptStatus;

    uxSavedInterruptStatus = portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR();
    {
        xReturn = xTickCount;
        /* MCDC Test Point: STD "xTaskGetTickCountFromISR" */
    }
    portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvTaskIncrementTick( void )
{
    xTCB *pxTCB;
    xList *pxTemp;
    portTickType xLocalTickCount;
    portTickType xLocalNextTaskUnblockTime;
    portTickType xItemValue;
    portBaseType xSwitchRequired = pdFALSE;


    /* SAFERTOSTRACE INCREMENTTICK */

    /* Called by the portable layer each time a tick interrupt oclwrs.
     * Increments the tick then checks to see if the new tick value will cause
     * any tasks to be unblocked. */
    if( 0U == uxSchedulerSuspended )
    {
        /* Minor optimisation: the tick count cannot change in this block. */
        xLocalTickCount = xTickCount;

        /* Check integrity of the tick count. */
        if( xTickCountMirror != ~xLocalTickCount )
        {
            /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */

            /* Call the error hook, taking care to avoid 'order of evaluation'
             * issues when accessing (the volatile) pxLwrrentTCB. */
            pxTCB = pxLwrrentTCB;
            vPortErrorHook( pxTCB, errILWALID_TICK_VALUE );
        }
        /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */

        /* Increment the RTOS tick. */
        ++xLocalTickCount;
        xTickCount = xLocalTickCount;
        xTickCountMirror = ~xLocalTickCount;

        /* Switch the delayed and overflowed delayed lists if it wraps to 0. */
        if( ( portTickType ) 0U == xLocalTickCount )
        {
            /* Tick count has overflowed so we need to swap the delay lists. */
            pxTemp = pxDelayedTaskList;
            pxDelayedTaskList = pxOverflowDelayedTaskList;
            pxOverflowDelayedTaskList = pxTemp;

            uxNumOfOverflows++;
            vTaskResetNextTaskUnblockTime();

            /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */
        }
        /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */

        /* See if this tick has made a timeout expire.
         * Tasks are stored in the queue in the order of their wake time -
         * meaning once one task has been found whose timer has not expired
         * we need not look any further down the list.
         */
        if( xLocalTickCount >= xNextTaskUnblockTime )
        {
            /* Start assuming that the delayed list is empty.
             * Set xNextTaskUnblockTime to the maximum possible value
             * so it is extremely unlikely that the
             * if( xTickCount >= xNextTaskUnblockTime ) test will pass
             * next time through. */
            xLocalNextTaskUnblockTime = portMAX_DELAY;

            /* MCDC Test Point: WHILE_EXTERNAL "prvTaskIncrementTick" "( xListIsListEmpty( ( xList * ) pxDelayedTaskList ) == pdFALSE )" */
            while( xListIsListEmpty( ( xList * ) pxDelayedTaskList ) == pdFALSE )
            {
                /* The delayed list is not empty, get the value of the item
                 * at the head of the delayed list. This is the time at
                 * which the task at the head of the delayed list must be
                 * removed from the Blocked state. */
                pxTCB = ( xTCB * ) listGUARANTEED_GET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );

                xItemValue = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );
                if( xLocalTickCount < xItemValue )
                {
                    /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */

                    /* It is not time to unblock this item yet, but the
                     * item value is the time at which the task at the head
                     * of the blocked list must be removed from the Blocked
                     * state - so record the item value in
                     * xNextTaskUnblockTime. */
                    xLocalNextTaskUnblockTime = xItemValue;
                    break;
                }
                /* MCDC Test Point: ADD_ELSE "xTaskIncrementTick" */

                /* It is time to remove the item from the Blocked state. */
                vListRemove( &( pxTCB->xStateListItem ) );

                /* Is the task waiting on an event also?
                 * If so, remove it from the event list. */
                if( xListIsContainedWithinAList( &( pxTCB->xEventListItem ) ) != pdFALSE )
                {
                    vListRemove( &( pxTCB->xEventListItem ) );

                    /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */
                }
                /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */

                /* Place the unblocked task into the appropriate ready list. */
                vTaskAddTaskToReadyList( pxTCB );

                /* A context switch should only be performed if the
                 * unblocked task has a priority that is equal to or higher
                 * than the lwrrently exelwting task. */
                if( pxTCB->uxPriority >= pxLwrrentTCB->uxPriority )
                {
                    xSwitchRequired = pdTRUE;

                    /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */
                }
                /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */

                /* MCDC Test Point: WHILE_INTERNAL "prvTaskIncrementTick" "( xListIsListEmpty( ( xList * ) pxDelayedTaskList ) == pdFALSE )" */
            }

            /* Update xNextTaskUnblockTime. */
            xNextTaskUnblockTime = xLocalNextTaskUnblockTime;
        }
        /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */

        /* Tasks of equal priority to the lwrrently running task will share
         * processing time (time slice) if preemption is on, and the application
         * writer has not explicitly turned time slicing off. */
        if( listLWRRENT_LIST_LENGTH( &( xReadyTasksLists[ pxLwrrentTCB->uxPriority ] ) ) > 1U )
        {
            xSwitchRequired = pdTRUE;

            /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */
        }
        /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */

        /*
         * Guard against the tick hook being called when the missed tick count
         * is being unwound (when the scheduler is being unlocked).
         */
        if( ( portUnsignedBaseType ) 0U == uxMissedTicks )
        {
            vPortTickHook();

            /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */
        }
        /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */
    }
    else
    {
        /* If scheduler is suspended, we need to update statistics since */
        /* vTaskSelectNextTask() won't be called, and we may lose ticks. */
        vUpdateRunTimeStatistics();

        ++uxMissedTicks;

        /*
         * The tick hook gets called at regular intervals, even if the scheduler
         * is locked.
         */
        vPortTickHook();

        /* MCDC Test Point: STD_ELSE "prvTaskIncrementTick" */
    }

    if( pdFALSE != xMissedYield )
    {
        xSwitchRequired = pdTRUE;

        /* MCDC Test Point: STD_IF "prvTaskIncrementTick" */
    }
    /* MCDC Test Point: ADD_ELSE "prvTaskIncrementTick" */

    return xSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskSelectNextTask( void )
{
    /* uxPriority and pxLwrrentTCB are declared volatile so use a copy. */
    portUnsignedBaseType uxPriority;
    xTCB *pxTCB;

    if( 0U != uxSchedulerSuspended )
    {
        /* The scheduler is lwrrently suspended - do not allow a context switch. */
        xMissedYield = pdTRUE;

        /* MCDC Test Point: STD_IF "vTaskSelectNextTask" */
    }
    else
    {
        /* Update run-time statistics as we are about to switch tasks. */
        vUpdateRunTimeStatistics();

        /* Find the highest priority queue that contains ready tasks. */
        uxPriority = uxTopReadyPriority;

        /* MCDC Test Point: WHILE_EXTERNAL "vTaskSelectNextTask" "( xListIsListEmpty( &( xReadyTasksLists[ uxTopReadyPriority ] ) ) == pdTRUE )" */
        while( xListIsListEmpty( &( xReadyTasksLists[ uxPriority ] ) ) == pdTRUE )
        {
            if( ( portUnsignedBaseType ) 0U == uxPriority )
            {
                /* MCDC Test Point: STD_IF "vTaskSelectNextTask" */

                vPortErrorHook( pxLwrrentTCB, errILWALID_TASK_SELECTED );
            }
            else
            {
                --uxPriority;

                /* MCDC Test Point: STD_ELSE "vTaskSelectNextTask" */
            }
            /* MCDC Test Point: WHILE_INTERNAL "vTaskSelectNextTask" "( xListIsListEmpty( &( xReadyTasksLists[ uxTopReadyPriority ] ) ) )" */
        }

        /* listGET_OWNER_OF_NEXT_ENTRY walks through the list, so the tasks of
         * the same priority get an equal share of the processor time. */
        listGET_OWNER_OF_NEXT_ENTRY( pxTCB, &( xReadyTasksLists[ uxPriority ] ) );

        /* Update global volatile variables. */
        uxTopReadyPriority = uxPriority;
        pxLwrrentTCB = pxTCB;

        /* Check the new TCB. */
        if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
        {
            /* MCDC Test Point: STD_IF "vTaskSelectNextTask" */

            vPortErrorHook( pxTCB, errILWALID_TASK_SELECTED );
        }
        /* MCDC Test Point: ADD_ELSE "vTaskSelectNextTask" */

        /* SAFERTOSTRACE TASKSWITCHEDINELSE */
        /* MCDC Test Point: STD_ELSE "vTaskSelectNextTask" */
    }
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskPlaceOnEventList( xList *pxEventList, portTickType xTicksToWait )
{
    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED. */

    /* Place the event list item of the TCB in the appropriate event list.
     * This is placed in the list in priority order so the highest priority
     * task is the first to be woken by the event. */
    vListInsertOrdered( pxEventList, &( pxLwrrentTCB->xEventListItem ) );

    /* Add the current task to the appropriate delay list. */
    vTaskAddLwrrentTaskToDelayedList( xTicksToWait );

    /* MCDC Test Point: STD "vTaskPlaceOnEventList" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskRemoveFromEventList( const xList *pxEventList )
{
    xTCB *pxUnblockedTCB;
    portBaseType xReturn;

    /* THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED OR THE
    SCHEDULER SUSPENDED.  It can also be called from within an ISR. */

    /* The event list is sorted in priority order, so we can remove the
    first in the list, remove the TCB from the delayed list, and add
    it to the ready list.

    If an event is for a queue that is locked then this function will never
    get called - the lock count on the queue will get modified instead.  This
    means we can always expect exclusive access to the event list here. */
    pxUnblockedTCB = ( xTCB * ) listGUARANTEED_GET_OWNER_OF_HEAD_ENTRY( pxEventList );
    vListRemove( &( pxUnblockedTCB->xEventListItem ) );

    if( 0U == uxSchedulerSuspended )
    {
        vListRemove( &( pxUnblockedTCB->xStateListItem ) );
        vTaskAddTaskToReadyList( pxUnblockedTCB );

        /* If a task is blocked on a kernel object, then xNextTaskUnblockTime
         * might be set to the blocked task's time out time. If the task is
         * unblocked for a reason other than a timeout xNextTaskUnblockTime is
         * normally left unchanged, because it is automatically reset to a new
         * value when the tick count equals xNextTaskUnblockTime. However, if
         * tickless idling is used, it might be more important to enter sleep
         * mode at the earliest possible time - so reset xNextTaskUnblockTime
         * here to ensure it is updated at the earliest possible time. */
        vTaskResetNextTaskUnblockTime();

        /* MCDC Test Point: STD_IF "xTaskRemoveFromEventList" */
    }
    else
    {
        /* We cannot access the delayed or ready lists, so will hold this
        task pending until the scheduler is resumed. */
        vListInsertEnd( &( xPendingReadyList ), &( pxUnblockedTCB->xEventListItem ) );

        /* MCDC Test Point: STD_ELSE "xTaskRemoveFromEventList" */
    }

    if( pxUnblockedTCB->uxPriority > pxLwrrentTCB->uxPriority )
    {
        /* Return true if the task removed from the event list has a higher
         * priority than the calling task. This allows the calling task to know
         * if it should force a context switch now. */
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xTaskRemoveFromEventList" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xTaskRemoveFromEventList" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portTaskHandleType xTaskGetLwrrentTaskHandle( void )
{
    portTaskHandleType xReturn;

    portTASK_HANDLE_ENTER_CRITICAL();
    {
        xReturn = ( portTaskHandleType ) pxLwrrentTCB;

        /* MCDC Test Point: STD "xTaskGetLwrrentTaskHandle" */
    }
    portTASK_HANDLE_EXIT_CRITICAL();

    return xReturn;
}
/*---------------------------------------------------------------------------*/

/*
 * The Idle task.
 */
KERNEL_FUNCTION void vIdleTask( void *pvParameters )
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
        if( listLWRRENT_LIST_LENGTH( &( xReadyTasksLists[ taskIDLE_PRIORITY ] ) ) > 1U )
        {
            taskYIELD_WITHIN_API();

            /* MCDC Test Point: STD_IF "vIdleTask" */
        }
        /* MCDC Test Point: ADD_ELSE "vIdleTask" */

        vPortIdleHook();
    }
}


/*-----------------------------------------------------------------------------
 * File private functions dolwmented at the top of the file.
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION static portBaseType prvCheckTaskCreateParameters( const xTaskParameters *const pxTaskParameters )
{
    portBaseType xReturn;
    portStackType *pxStackInUseMarker;

    if( NULL == pxTaskParameters )
    {
        /* If pxTaskParameters is NULL, we can't check any more. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "prvCheckTaskCreateParameters" */
    }
    else
    {
        if( NULL == pxTaskParameters->pvTaskCode )
        {
            xReturn = errILWALID_TASK_CODE_POINTER;

            /* MCDC Test Point: STD_IF "prvCheckTaskCreateParameters" */
        }
        else if( pxTaskParameters->uxPriority >= configMAX_PRIORITIES )
        {
            /* The priority is above the stated maximum. */
            xReturn = errILWALID_PRIORITY;

            /* MCDC Test Point: STD_ELSE_IF "prvCheckTaskCreateParameters" */
        }
        else if( NULL == pxTaskParameters->pcStackBuffer )
        {
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: STD_ELSE_IF "prvCheckTaskCreateParameters" */
        }
        else if( NULL == pxTaskParameters->pxTCB )
        {
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: STD_ELSE_IF "prvCheckTaskCreateParameters" */
        }
        else
        {
            /* No errors have been detected so far, so pass the port specific
            parameters to the port layer for checking. */
            xReturn = xPortCheckTaskParameters( pxTaskParameters );

            /* If no errors are detected in the port layer, check that the task
            stack isn't already in use. */
            if( pdPASS == xReturn )
            {
                /* Locate the 'stack in use' marker. */
                pxStackInUseMarker = ( portStackType * )( ( portUnsignedBaseType ) pxTaskParameters->pcStackBuffer + ( pxTaskParameters->uxStackDepthBytes & ~portSTACK_ALIGNMENT_MASK ) );
                pxStackInUseMarker--;

                /* Is the task stack already in use? */
                if( portSTACK_IN_USE == *pxStackInUseMarker )
                {
                    xReturn = errTASK_STACK_ALREADY_IN_USE;
                    /* MCDC Test Point: STD_IF "prvCheckTaskCreateParameters" */
                }
                /* MCDC Test Point: ADD_ELSE "prvCheckTaskCreateParameters" */
            }
            /* MCDC Test Point: ADD_ELSE "prvCheckTaskCreateParameters" */

            /* MCDC Test Point: STD_ELSE "prvCheckTaskCreateParameters" */
        }
        /* MCDC Test Point: STD_ELSE "prvCheckTaskCreateParameters" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_INIT_FUNCTION static void prvInitialiseTaskLists( void )
{
    portUnsignedBaseType uxPriority;

    for( uxPriority = ( portUnsignedBaseType ) 0; uxPriority < configMAX_PRIORITIES; uxPriority++ )
    {
        vListInitialise( &( xReadyTasksLists[ uxPriority ] ) );

        /* MCDC Test Point: STD "prvInitialiseTaskLists" */
    }

    vListInitialise( &xDelayedTaskList1 );
    vListInitialise( &xDelayedTaskList2 );
    vListInitialise( &xPendingReadyList );
    vListInitialise( &xSuspendedTaskList );

    /* Start with pxDelayedTaskList using list1 and the pxOverflowDelayedTaskList
    using list2. */
    pxDelayedTaskList = &xDelayedTaskList1;
    pxOverflowDelayedTaskList = &xDelayedTaskList2;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskAddTaskToReadyList( xTCB *pxTCB )
{
    /* SAFERTOSTRACE TASKMOVEDTOREADYSTATE */

    if( pxTCB->uxPriority > uxTopReadyPriority )
    {
        uxTopReadyPriority = pxTCB->uxPriority;

        /* MCDC Test Point: STD_IF "vTaskAddTaskToReadyList" */
    }
    /* MCDC Test Point: ADD_ELSE "vTaskAddTaskToReadyList" */

    vListInsertEnd( &( xReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xStateListItem ) );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskAddLwrrentTaskToDelayedList( portTickType xTicksToWait )
{
    portTickType xTimeToWake;
    xTCB *const pxTCB = pxLwrrentTCB;   /* pxLwrrentTCB is declared volatile so take a copy. */

    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED OR FROM WITHIN
     * A CRITICAL SECTION. */

    /* Callwlate the time at which the task should be woken if the event does
     * not occur.  This may overflow but this doesn't matter. */
    xTimeToWake = xTickCount + xTicksToWait;

    /* We must remove ourselves from the ready list before adding ourselves to
     * the blocked list as the same list item is used for both lists.  We have
     * exclusive access to the ready lists as the scheduler is locked. */
    vListRemove( &( pxTCB->xStateListItem ) );

    listSET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ), xTimeToWake );

    if( xTimeToWake < xTickCount )
    {
        /* Wake time has overflowed.  Place this item in the overflow list. */
        vListInsertOrdered( ( xList * ) pxOverflowDelayedTaskList, &( pxTCB->xStateListItem ) );

        /* MCDC Test Point: STD_IF "vTaskAddLwrrentTaskToDelayedList" */
    }
    else
    {
        /* The wake time has not overflowed, so we can use the current block list. */
        vListInsertOrdered( ( xList * ) pxDelayedTaskList, &( pxTCB->xStateListItem ) );

        /* If the task entering the blocked state was placed at the head of the
         * list of blocked tasks then xNextTaskUnblockTime needs to be updated,
         * too. */
        if( xTimeToWake < xNextTaskUnblockTime )
        {
            xNextTaskUnblockTime = xTimeToWake;

            /* MCDC Test Point: STD_IF "vTaskAddLwrrentTaskToDelayedList" */
        }
        /* MCDC Test Point: ADD_ELSE "vTaskAddLwrrentTaskToDelayedList" */

        /* MCDC Test Point: STD_ELSE "vTaskAddLwrrentTaskToDelayedList" */
    }
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskMoveTaskToReadyList( xTCB *pxTaskToMakeReady )
{
    portBaseType xSwitchRequired;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION OR WITH THE
     * SCHEDULER SUSPENDED.  It can also be called from within an ISR. */

    if( 0U == uxSchedulerSuspended )
    {
        /* MCDC Test Point: STD_IF "xTaskMoveTaskToReadyList" */

        /* Remove the task from the delayed list and add it to the ready list.
         * As this function is only called from within a critical section,
         * interrupts will not be accessing the ready lists. */
        vListRemove( &( pxTaskToMakeReady->xStateListItem ) );
        vTaskAddTaskToReadyList( pxTaskToMakeReady );

        /* If a task is blocked on a kernel object, then xNextTaskUnblockTime
         * might be set to the blocked task's timeout time. If the task is
         * unblocked for a reason other than a timeout, xNextTaskUnblockTime is
         * normally left unchanged, because it is automatically reset to a new
         * value when the tick count equals xNextTaskUnblockTime. However, if
         * tickless idling is used, it might be more important to enter sleep
         * mode at the earliest possible time - so reset xNextTaskUnblockTime
         * here to ensure it is updated at the earliest possible time. */
        vTaskResetNextTaskUnblockTime();
    }
    else
    {
        /* MCDC Test Point: STD_ELSE "xTaskMoveTaskToReadyList" */

        /* The delayed and ready lists cannot be accessed, so hold this task
         * pending until the scheduler is resumed. */
        vListInsertEnd( &( xPendingReadyList ),
                        &( pxTaskToMakeReady->xEventListItem ) );
    }

    if( pxTaskToMakeReady->uxPriority > pxLwrrentTCB->uxPriority )
    {
        /* MCDC Test Point: STD_IF "xTaskMoveTaskToReadyList" */

        /* Return true if the unblocked task has a higher priority than the
         * calling task. This allows the calling task to know if it should force
         * a context switch now. */
        xSwitchRequired = pdTRUE;
    }
    else
    {
        /* MCDC Test Point: STD_ELSE "xTaskMoveTaskToReadyList" */

        xSwitchRequired = pdFALSE;
    }

    return xSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskIsSchedulerSuspended( void )
{
    portBaseType xReturn;

    if( 0U != uxSchedulerSuspended )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xTaskIsSchedulerSuspended" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xTaskIsSchedulerSuspended" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskStackCheckFailed( void )
{
    xTCB *pxTCB;

    /* MCDC Test Point: STD "vTaskStackCheckFailed" */

    /* Call the error hook, taking care to avoid 'order of evaluation' issues
     * when accessing (the volatile) pxLwrrentTCB. */
    pxTCB = pxLwrrentTCB;
    vPortErrorHook( pxTCB, errTASK_STACK_OVERFLOW );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskSetTimeOut( xTimeOutType *pxTimeOut )
{
    /* uxNumOfOverflows is incremented each time xTickCount
    overflows ( each time the delay lists are swapped ). Just looking for a
    change in the value of pxDelayedTaskList would remove the need for this
    new variable, but would require more instructions to perform the compare
    on 8 bit devices. */
    pxTimeOut->uxOverflowCount = uxNumOfOverflows;
    pxTimeOut->xTimeOnEntering = xTickCount;

    /* MCDC Test Point: STD "vTaskSetTimeOut" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskCheckForTimeOut( xTimeOutType *pxTimeOut, portTickType *pxTicksToWait )
{
    portBaseType xReturn;
    portTickType xLocalTickCount;

    portENTER_CRITICAL_WITHIN_API();
    {
        /* Cache volatile variable for performance and to avoid side effects on
         * the right of a logical operator (xTickCount cannot change inside a
         * critical section). */
        xLocalTickCount = xTickCount;

        if( ( uxNumOfOverflows != pxTimeOut->uxOverflowCount ) && ( xLocalTickCount >= pxTimeOut->xTimeOnEntering ) )
        {
            /* The tick count is greater than the time at which vTaskSetTimeout() was
            called, but has also overflowed since vTaskSetTimeOut() was called.
            It must have wrapped all the way around and gone past us again. This
            passed since vTaskSetTimeout() was called. */
            xReturn = pdTRUE;

            /* MCDC Test Point: STD_IF "xTaskCheckForTimeOut" */
        }
        else if( ( xLocalTickCount - pxTimeOut->xTimeOnEntering ) < *pxTicksToWait )
        {
            /* Not a genuine timeout. Adjust parameters for time remaining. */
            *pxTicksToWait -= ( xLocalTickCount - pxTimeOut->xTimeOnEntering );

            vTaskSetTimeOut( pxTimeOut );

            xReturn = pdFALSE;

            /* MCDC Test Point: STD_ELSE_IF "xTaskCheckForTimeOut" */
        }
        else
        {
            xReturn = pdTRUE;

            /* MCDC Test Point: STD_ELSE "xTaskCheckForTimeOut" */
        }

        /* MCDC Test Point: EXP_IF_AND "xTaskCheckForTimeOut" "( uxNumOfOverflows != pxTimeOut->uxOverflowCount )" "( xLocalTickCount >= pxTimeOut->xTimeOnEntering )" "Deviate: FF" */
    }
    portEXIT_CRITICAL_WITHIN_API();

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskPendYield( void )
{
    xMissedYield = pdTRUE;

    /* MCDC Test Point: STD "vTaskPendYield" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskIsSchedulerStarted( void )
{
    /* MCDC Test Point: STD "xTaskIsSchedulerStarted" */

    return xSchedulerRunning;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskIsSchedulerStartedFromISR( void )
{
    /* MCDC Test Point: STD "xTaskIsSchedulerStartedFromISR" */

    return xSchedulerRunning;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskNotifyWait( portUnsignedBaseType uxBitsToClearOnEntry,
                                              portUnsignedBaseType uxBitsToClearOnExit,
                                              portUnsignedBaseType *puxNotificatiolwalue,
                                              portTickType xTicksToWait )
{
    xTimeOutType xTimeOut;
    portBaseType xReturn;
    /* Save a non-volatile copy of the current TCB to improve the performance.
     * NOTE: even though this function may trigger a context switch, pxLwrrentTCB
     * will be pointing to the same TCB when the task unblocks, therefore the
     * pointer can be considered constant. */
    xTCB *const pxTCB = pxLwrrentTCB;

    if( 0U != uxSchedulerSuspended )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xTaskNotifyWait" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            vTaskSetTimeOut( &xTimeOut );
            do
            {
                /* The call to xTaskNotifyWait() is valid. */
                xReturn = pdPASS;

                /* Only block if a notification is not already pending. */
                if( pxTCB->xNotifyState != taskNOTIFICATION_NOTIFIED )
                {
                    /* MCDC Test Point: STD_IF "xTaskNotifyWait" */

                    /* Clear bits in the task's notification value as bits may get
                     * set by the notifying task or interrupt. This can be used to
                     * clear the value to zero. */
                    pxTCB->uxNotifiedValue &= ~uxBitsToClearOnEntry;

                    /* Mark this task as waiting for a notification. */
                    pxTCB->xNotifyState = taskNOTIFICATION_WAITING;

                    if( xTicksToWait > ( portTickType ) 0U )
                    {
                        /* MCDC Test Point: STD_IF "xTaskNotifyWait" */

                        /* Change the state of this task to Blocked. */
                        vTaskAddLwrrentTaskToDelayedList( xTicksToWait );

                        /* SAFERTOSTRACE TASKNOTIFYWAITBLOCK */

                        /* All ports are written to allow a yield in a critical
                         * section (some will yield immediately, others wait until
                         * the critical section exits) - but it is not something
                         * that application code should ever do. */
                        taskYIELD_WITHIN_API();
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */

                /* We reach this point when we have finished waiting or if a
                 * notification was already pending. */
                if( NULL != puxNotificatiolwalue )
                {
                    /* MCDC Test Point: STD_IF "xTaskNotifyWait" */

                    /* Output the current notification value, which may or may not
                     * have changed. */
                    if( pdPASS != portCOPY_DATA_TO_TASK( puxNotificatiolwalue, ( const void * ) &( pxTCB->uxNotifiedValue ), ( portUnsignedBaseType ) sizeof( portUnsignedBaseType ) ) )
                    {
                        xReturn = errILWALID_DATA_RANGE;
                        /* MCDC Test Point: STD_IF "xTaskNotifyWait" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */

                if( pdPASS == xReturn )
                {
                    /* MCDC Test Point: STD_IF "xTaskNotifyWait" */

                    /* If xNotifyState is set then either the task never entered the
                     * blocked state (because a notification was already pending) or
                     * the task unblocked because of a notification. Otherwise the task
                     * unblocked because of a timeout. */
                    if( taskNOTIFICATION_WAITING == pxTCB->xNotifyState )
                    {
                        /* A notification was not received. */
                        xReturn = errNOTIFICATION_NOT_RECEIVED;

                        /* MCDC Test Point: STD_IF "xTaskNotifyWait" */
                    }
                    else
                    {
                        /* A notification was already pending or a notification was
                         * received while the task was waiting. */
                        pxTCB->uxNotifiedValue &= ~uxBitsToClearOnExit;

                        /* Update the the status of all event poll objects
                         * registered with this task. Note, as there is no
                         * longer a task notification value waiting to be
                         * retrieved, the return value of this call will always
                         * be pdFALSE and therefore can be ignored. */
                        ( void ) xEventPollUpdateTaskEventPollObjects( pxTCB, eventpollNO_EVENTS );

                        /* MCDC Test Point: STD_ELSE "xTaskNotifyWait" */
                    }

                    if( errNOTIFICATION_NOT_RECEIVED == xReturn )
                    {
                        /* MCDC Test Point: STD_IF "xTaskNotifyWait" */

                        if( xTicksToWait > ( portTickType ) 0U )
                        {
                            /* MCDC Test Point: STD_IF "xTaskNotifyWait" */

                            if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                            {
                                xReturn = errERRONEOUS_UNBLOCK;
                                /* MCDC Test Point: STD_IF "xTaskNotifyWait" */
                            }
                            /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskNotifyWait" */

                /* MCDC Test Point: WHILE_INTERNAL "xTaskNotifyWait" "( errERRONEOUS_UNBLOCK == xReturn )" */
            } while( errERRONEOUS_UNBLOCK == xReturn );

            pxTCB->xNotifyState = taskNOTIFICATION_NOT_WAITING;
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_ELSE "xTaskNotifyWait" */
    }

    /* SAFERTOSTRACE TASKNOTIFYWAIT */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskNotifySend( portTaskHandleType xTaskToNotify,
                                              portBaseType xAction,
                                              portUnsignedBaseType uxValue )
{
    xTCB *pxTCB;
    portBaseType xOriginalNotifyState;
    portBaseType xReturn = pdPASS;
    portBaseType xContextSwitchRequired = pdFALSE;

    if( NULL == xTaskToNotify )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xTaskNotifySend" */
    }
    else
    {
        pxTCB = taskGET_TCB_FROM_HANDLE( xTaskToNotify );

        portENTER_CRITICAL_WITHIN_API();
        {
            if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
            {
                xReturn = errILWALID_TASK_HANDLE;

                /* MCDC Test Point: STD_IF "xTaskNotifySend" */
            }
            else
            {
                xOriginalNotifyState = pxTCB->xNotifyState;

                switch( xAction )
                {
                    case taskNOTIFICATION_SET_BITS:
                        pxTCB->uxNotifiedValue |= uxValue;

                        /* MCDC Test Point: STD "xTaskNotifySend" */
                        break;

                    case taskNOTIFICATION_INCREMENT:
                        ( pxTCB->uxNotifiedValue )++;

                        /* MCDC Test Point: STD "xTaskNotifySend" */
                        break;

                    case taskNOTIFICATION_SET_VALUE_WITH_OVERWRITE:
                        pxTCB->uxNotifiedValue = uxValue;

                        /* MCDC Test Point: STD "xTaskNotifySend" */
                        break;

                    case taskNOTIFICATION_SET_VALUE_WITHOUT_OVERWRITE:
                        if( taskNOTIFICATION_NOTIFIED != xOriginalNotifyState )
                        {
                            pxTCB->uxNotifiedValue = uxValue;

                            /* MCDC Test Point: STD_IF "xTaskNotifySend" */
                        }
                        else
                        {
                            /* The value could not be written to the task. */
                            xReturn = errNOTIFICATION_ALREADY_PENDING;

                            /* MCDC Test Point: STD_ELSE "xTaskNotifySend" */
                        }
                        break;

                    case taskNOTIFICATION_NO_ACTION:
                        /* The task is being notified without its notify value
                         * being updated. */

                        /* MCDC Test Point: STD "xTaskNotifySend" */

                        break;

                    default:
                        /* Parameter not valid. */
                        xReturn = errILWALID_PARAMETERS;

                        /* MCDC Test Point: STD "xTaskNotifySend" */
                        break;
                }

                /* SAFERTOSTRACE TASKNOTIFYSEND */

                if( pdPASS == xReturn )
                {
                    pxTCB->xNotifyState = taskNOTIFICATION_NOTIFIED;

                    /* If the task is in the blocked state specifically to wait
                     * for a notification then unblock it now. */
                    if( taskNOTIFICATION_WAITING == xOriginalNotifyState )
                    {
                        /* The task should not have been on an event list. */
                        if( xListIsContainedWithinAList( &( pxTCB->xEventListItem ) ) != pdFALSE )
                        {
                            /* MCDC Test Point: STD_IF "xTaskNotifySend" */

                            /* The task should not have been on an event list. */
                            vPortErrorHook( pxTCB, errTASK_WAS_ALSO_ON_EVENT_LIST );
                        }
                        else if( pdFALSE == xListIsContainedWithin( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) )
                        {
                            if( 0U == uxSchedulerSuspended )
                            {
                                vListRemove( &( pxTCB->xStateListItem ) );
                                vTaskAddTaskToReadyList( pxTCB );

                                /* For tickless idling, it might be important
                                 * to enter sleep mode at the earliest possible
                                 * time - so reset xNextTaskUnblockTime here to
                                 * ensure it is updated at the earliest
                                 * possible time. */
                                vTaskResetNextTaskUnblockTime();

                                if( pxTCB->uxPriority > pxLwrrentTCB->uxPriority )
                                {
                                    /* The notified task has a priority above
                                     * the lwrrently exelwting task, so a yield
                                     * is required. */
                                    xContextSwitchRequired = pdTRUE;

                                    /* MCDC Test Point: STD_IF "xTaskNotifySend" */
                                }
                                /* MCDC Test Point: ADD_ELSE "xTaskNotifySend" */
                            }
                            else
                            {
                                /* We cannot access the delayed or ready lists,
                                 * so will hold this task pending until the
                                 * scheduler is resumed. */
                                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );

                                /* MCDC Test Point: STD_ELSE "xTaskNotifySend" */
                            }

                            /* MCDC Test Point: STD_ELSE_IF "xTaskNotifySend" */
                        }
                        else
                        {
                            /* The task is lwrrently suspended, so leave it in
                             * the Suspended state. */

                            /* MCDC Test Point: STD_ELSE "xTaskNotifySend" */
                        }
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifySend" */

                    /* If event poll has been included, update all registered event poll objects.
                     * Otherwise, an empty weak function will be called. */
                    if( pdTRUE == xEventPollUpdateTaskEventPollObjects( pxTCB, eventpollTASK_NOTIFICATION_RECEIVED ) )
                    {
                        /* MCDC Test Point: STD_IF "xTaskNotifySend" */

                        /* A higher priority task has been unblocked due to
                         * updating an event poll object. */
                        xContextSwitchRequired = pdTRUE;
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifySend" */

                    /* Is a context switch required? */
                    if( pdTRUE == xContextSwitchRequired )
                    {
                        /* MCDC Test Point: STD_IF "xTaskNotifySend" */

                        /* Yield if a higher priority task has been made
                         * ready. */
                        taskYIELD_WITHIN_API();
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifySend" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskNotifySend" */
                /* MCDC Test Point: STD_ELSE "xTaskNotifySend" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xTaskNotifySend" */
    }

    /* SAFERTOSTRACE TASKNOTIFYSENDERROR */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskNotifySendFromISR( portTaskHandleType xTaskToNotify,
                                                     portBaseType xAction,
                                                     portUnsignedBaseType uxValue,
                                                     portBaseType *pxHigherPriorityTaskWoken )
{
    xTCB *pxTCB;
    portBaseType xOriginalNotifyState;
    portBaseType xReturn = pdPASS;
    portUnsignedBaseType uxSavedInterruptStatus;
    portBaseType xHigherPriorityTaskWoken = pdFALSE;

    if( NULL == pxHigherPriorityTaskWoken )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */
    }
    else if( NULL == xTaskToNotify )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xTaskNotifySendFromISR" */
    }
    else
    {
        pxTCB = taskGET_TCB_FROM_HANDLE( xTaskToNotify );

        uxSavedInterruptStatus = taskSET_INTERRUPT_MASK_FROM_ISR();
        {
            if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
            {
                xReturn = errILWALID_TASK_HANDLE;

                /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */
            }
            else
            {
                xOriginalNotifyState = pxTCB->xNotifyState;

                switch( xAction )
                {
                    case taskNOTIFICATION_SET_BITS:
                        pxTCB->uxNotifiedValue |= uxValue;

                        /* MCDC Test Point: STD "xTaskNotifySendFromISR" */
                        break;

                    case taskNOTIFICATION_INCREMENT:
                        ( pxTCB->uxNotifiedValue )++;

                        /* MCDC Test Point: STD "xTaskNotifySendFromISR" */
                        break;

                    case taskNOTIFICATION_SET_VALUE_WITH_OVERWRITE:
                        pxTCB->uxNotifiedValue = uxValue;

                        /* MCDC Test Point: STD "xTaskNotifySendFromISR" */
                        break;

                    case taskNOTIFICATION_SET_VALUE_WITHOUT_OVERWRITE:
                        if( taskNOTIFICATION_NOTIFIED != xOriginalNotifyState )
                        {
                            pxTCB->uxNotifiedValue = uxValue;

                            /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */
                        }
                        else
                        {
                            /* The value could not be written to the task. */
                            xReturn = errNOTIFICATION_ALREADY_PENDING;

                            /* MCDC Test Point: STD_ELSE "xTaskNotifySendFromISR" */
                        }
                        break;

                    case taskNOTIFICATION_NO_ACTION:
                        /* The task is being notified without its notify value
                         * being updated. */

                        /* MCDC Test Point: STD "xTaskNotifySendFromISR" */
                        break;

                    default:
                        /* Parameter not valid. */
                        xReturn = errILWALID_PARAMETERS;

                        /* MCDC Test Point: STD "xTaskNotifySendFromISR" */
                        break;
                }

                /* SAFERTOSTRACE TASKNOTIFYSENDFROMISR */

                if( pdPASS == xReturn )
                {
                    pxTCB->xNotifyState = taskNOTIFICATION_NOTIFIED;

                    /* If the task is in the blocked state specifically to wait
                     * for a notification then unblock it now. */
                    if( taskNOTIFICATION_WAITING == xOriginalNotifyState )
                    {
                        if( xListIsContainedWithinAList( &( pxTCB->xEventListItem ) ) != pdFALSE )
                        {
                            /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */

                            /* The task should not have been on an event list. */
                            vPortErrorHook( pxTCB, errTASK_WAS_ALSO_ON_EVENT_LIST );
                        }
                        else if( pdFALSE == xListIsContainedWithin( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) )
                        {
                            if( 0U == uxSchedulerSuspended )
                            {
                                vListRemove( &( pxTCB->xStateListItem ) );
                                vTaskAddTaskToReadyList( pxTCB );

                                /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */
                            }
                            else
                            {
                                /* The delayed and ready lists cannot be
                                 * accessed, so hold this task pending until
                                 * the scheduler is resumed. */
                                vListInsertEnd( &( xPendingReadyList ), &( pxTCB->xEventListItem ) );

                                /* MCDC Test Point: STD_ELSE "xTaskNotifySendFromISR" */
                            }

                            if( pxTCB->uxPriority > pxLwrrentTCB->uxPriority )
                            {
                                /* The notified task has a priority above the
                                 * lwrrently exelwting task, so a yield is
                                 * required. */
                                xHigherPriorityTaskWoken = pdTRUE;

                                /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */
                            }
                            /* MCDC Test Point: ADD_ELSE "xTaskNotifySendFromISR" */

                            /* MCDC Test Point: STD_ELSE_IF "xTaskNotifySendFromISR" */
                        }
                        else
                        {
                            /* The task is lwrrently suspended, so leave it in
                             * the Suspended state. */

                            /* MCDC Test Point: STD_ELSE "xTaskNotifySendFromISR" */
                        }
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifySendFromISR" */

                    /* If event poll has been included, update all registered event poll objects.
                     * Otherwise, an empty weak function will be called. */
                    if( pdTRUE == xEventPollUpdateTaskEventPollObjects( pxTCB, eventpollTASK_NOTIFICATION_RECEIVED ) )
                    {
                        /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */

                        /* A higher priority task has been unblocked due to
                         * updating an event poll object. */
                        xHigherPriorityTaskWoken = pdTRUE;
                    }
                    /* MCDC Test Point: ADD_ELSE "xTaskNotifySendFromISR" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskNotifySendFromISR" */
                /* MCDC Test Point: STD_ELSE "xTaskNotifySendFromISR" */
            }

            /* Has a higher priority task been woken? */
            if( pdTRUE == xHigherPriorityTaskWoken )
            {
                /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */

                if( pdPASS != portCOPY_DATA_TO_ISR( pxHigherPriorityTaskWoken, &xHigherPriorityTaskWoken, ( portUnsignedBaseType ) sizeof( portBaseType ) ) )
                {
                    xReturn = errILWALID_DATA_RANGE;

                    /* MCDC Test Point: STD_IF "xTaskNotifySendFromISR" */
                }
                /* MCDC Test Point: ADD_ELSE "xTaskNotifySendFromISR" */
            }
            /* MCDC Test Point: ADD_ELSE "xTaskNotifySendFromISR" */
        }
        taskCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );
        /* MCDC Test Point: STD_ELSE "xTaskNotifySendFromISR" */
    }
    /* SAFERTOSTRACE TASKNOTIFYSENDFROMISRERROR */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void *pvTaskTLSObjectGet( void )
{
    /* MCDC Test Point: STD "pvTaskTLSObjectGet" */
    return pxLwrrentTCB->pvObject;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskProcessSystemTickFromISR( void )
{
    portUnsignedBaseType uxLwrrentMask;

    /* On ports which support interrupt nesting, we need to ensure that this
     * operation cannot be interrupted. */
    uxLwrrentMask = portSET_INTERRUPT_MASK_FROM_ISR();
    {
        if( prvTaskIncrementTick() != pdFALSE )
        {
            /* Request a context switch. */
            portYIELD_IMMEDIATE();
            /* MCDC Test Point: STD_IF "vTaskProcessSystemTickFromISR" */
        }
        /* MCDC Test Point: ADD_ELSE "vTaskProcessSystemTickFromISR" */
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR( uxLwrrentMask );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vTaskResetNextTaskUnblockTime( void )
{
    xTCB *pxTCB;

    /* THIS FUNCTION MUST BE CALLED WITH INTERRUPTS DISABLED OR THE SCHEDULER
     * SUSPENDED. */

    if( xListIsListEmpty( ( xList * ) pxDelayedTaskList ) == pdTRUE )
    {
        /* The new current delayed list is empty.
         * Set xNextTaskUnblockTime to the maximum possible value so it is
         * extremely unlikely that the if( xTickCount >= xNextTaskUnblockTime )
         * test will pass until there is an item in the delayed list. */
        xNextTaskUnblockTime = portMAX_DELAY;

        /* MCDC Test Point: STD_IF "vTaskResetNextTaskUnblockTime" */
    }
    else
    {
        /* The new current delayed list is not empty, get the value of the item
         * at the head of the delayed list. This is the time at which the task
         * at the head of the delayed list should be removed from the Blocked
         * state. */
        pxTCB = ( xTCB * ) listGUARANTEED_GET_OWNER_OF_HEAD_ENTRY( pxDelayedTaskList );
        xNextTaskUnblockTime = listGET_LIST_ITEM_VALUE( &( pxTCB->xStateListItem ) );

        /* MCDC Test Point: STD_ELSE "vTaskResetNextTaskUnblockTime" */
    }
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTaskIsTaskSuspended( const xTCB *pxTCB )
{
    portBaseType xReturn;

    /* Is this task lwrrently suspended? */
    if( xListIsContainedWithin( &xSuspendedTaskList, &( pxTCB->xStateListItem ) ) != pdFALSE )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xTaskIsTaskSuspended" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xTaskIsTaskSuspended" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Weak stub Function Definitions
 *---------------------------------------------------------------------------*/

/* If the timers module is not included then this stub function is required. */
KERNEL_INIT_FUNCTION portWEAK_FUNCTION portBaseType xTimerInitialiseFeature(
    timerInstanceParametersType *pxTimerInstance,
    const xPORT_INIT_PARAMETERS *const pxPortInitParameters )
{
    /* MCDC Test Point: STD "WEAK_xTimerInitialiseFeature" */

    ( void ) pxTimerInstance;
    ( void ) pxPortInitParameters;

    return pdPASS;
}
/*---------------------------------------------------------------------------*/

/* If run time statistics is not included then this stub function is required. */
KERNEL_INIT_FUNCTION portWEAK_FUNCTION void vInitialiseRunTimeStatistics( void )
{
    /* MCDC Test Point: STD "WEAK_vInitialiseRunTimeStatistics" */
}
/*---------------------------------------------------------------------------*/

/* If run time statistics is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vUpdateRunTimeStatistics( void )
{
    /* MCDC Test Point: STD "WEAK_vUpdateRunTimeStatistics" */
}
/*---------------------------------------------------------------------------*/

/* If run time statistics is not included then this stub function is required. */
KERNEL_CREATE_FUNCTION portWEAK_FUNCTION void vInitialiseTaskRunTimeStatistics( portTaskHandleType xHandle )
{
    /* MCDC Test Point: STD "WEAK_vInitialiseTaskRunTimeStatistics" */

    ( void ) xHandle;
}
/*---------------------------------------------------------------------------*/

/* If mutex is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vMutexReleaseAll( xTCB *pxTCB )
{
    /* MCDC Test Point: STD "WEAK_vMutexReleaseAll" */

    ( void ) pxTCB;
}
/*---------------------------------------------------------------------------*/

/* If mutex is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION portUnsignedBaseType uxMutexFindHighestBlockedTaskPriority( xTCB *pxTCB )
{
    /* MCDC Test Point: STD "WEAK_uxMutexFindHighestBlockedTaskPriority" */

    return pxTCB->uxBasePriority;
}
/*---------------------------------------------------------------------------*/

/* If event poll is not included then this stub function is required. */
KERNEL_DELETE_FUNCTION portWEAK_FUNCTION void vEventPollTaskDeleted( xTCB *pxTCB )
{
    /* MCDC Test Point: STD "WEAK_vEventPollTaskDeleted" */

    ( void ) pxTCB;
}
/*---------------------------------------------------------------------------*/

/* If event poll is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION portBaseType xEventPollUpdateTaskEventPollObjects( const xTCB *pxTCB, portUnsignedBaseType uxLwrrentStatus )
{
    /* MCDC Test Point: STD "WEAK_xEventPollUpdateTaskEventPollObjects" */

    ( void ) pxTCB;
    ( void ) uxLwrrentStatus;

    return pdFALSE;
}
/*---------------------------------------------------------------------------*/

/* SAFERTOSTRACE UXGETTASKNUMBER */

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_taskc.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "TaskCTestHeaders.h"
    #include "TaskCTest.h"
    #include "TaskInitializeSchedulerTest.h"
#endif
