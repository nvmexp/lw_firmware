/*
    Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

    This file is part of the SafeRTOS product, see projdefs.h for version number information.

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

#define KERNEL_SOURCE_FILE

#include "SafeRTOS.h"
#include "queue.h"
#include "task.h"
#include "mutex.h"
#include "eventgroups.h"

/*---------------------------------------------------------------------------*/
/* Constant Definitions.                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Macros                                                                    */
/*---------------------------------------------------------------------------*/

/* A mutex is a special type of queue. */
#define mutexQUEUE_LENGTH           ( ( portUnsignedBaseType ) 1 )
#define mutexQUEUE_ITEM_LENGTH      ( ( portUnsignedBaseType ) 0 )


/*---------------------------------------------------------------------------*/
/* Type Definitions.                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variables.                                                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Function prototypes.                                                      */
/*---------------------------------------------------------------------------*/
KERNEL_CREATE_FUNCTION static void prvMutexInitialiseMutex( xQUEUE *pxQueue );

KERNEL_DELETE_FUNCTION static portBaseType prvMutexForcedMutexRelease( xQueueHandle xMutex );

KERNEL_FUNCTION static portUnsignedBaseType prvGetBlockedTaskPriority( xQueueHandle xQueue );

/* MCDC Test Point: PROTOTYPE */

/*---------------------------------------------------------------------------*/
/* External declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Public API functions                                                      */
/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xMutexCreate( portInt8Type *pcMutexBuffer, xMutexHandleType *pxMutex )
{
    portBaseType xReturn;
    xMutexHandleType xCreatedMutex = NULL;

    /* A critical section is not required as the mutex cannot be used until the
     * handle is returned to the calling task. */

    /* Check the supplied mutex handle. */
    if( NULL != pxMutex )
    {
        xReturn = xQueueCreate( pcMutexBuffer,
                                portQUEUE_OVERHEAD_BYTES,
                                mutexQUEUE_LENGTH,
                                mutexQUEUE_ITEM_LENGTH,
                                &xCreatedMutex );

        if( pdPASS == xReturn )
        {
            /* Initialise the queue elements that are specific to mutex type operation. */
            prvMutexInitialiseMutex( ( xQUEUE * ) xCreatedMutex );

            /* Set the returned mutex handle. */
            if( pdPASS != portCOPY_DATA_TO_TASK( pxMutex, &xCreatedMutex, ( portUnsignedBaseType ) sizeof( xMutexHandleType ) ) )
            {
                xReturn = errILWALID_DATA_RANGE;

                /* MCDC Test Point: STD_IF "xMutexCreate" */
            }
            /* MCDC Test Point: ADD_ELSE "xMutexCreate" */

            /* MCDC Test Point: STD_IF "xMutexCreate" */
        }
        /* MCDC Test Point: ADD_ELSE "xMutexCreate" */
    }
    else
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE "xMutexCreate" */
    }

    /* SAFERTOSTRACE MUTEXCREATEFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vMutexInitHeldList( xTCB *pxTCB )
{
    vListInitialise( &( pxTCB->xMutexesHeldList ) );

    /* MCDC Test Point: STD "vMutexInitHeldList" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vMutexInitQueueVariables(xQUEUE *pxQueue )
{
    pxQueue->uxMutexRelwrsiveDepth = 0U;
    vListInitialiseItem( &( pxQueue->xMutexHolder ) );
    listSET_LIST_ITEM_OWNER( &( pxQueue->xMutexHolder ), pxQueue );

    /* MCDC Test Point: STD "vMutexInitQueueVariables" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xMutexTake( xMutexHandleType xMutex, portTickType xTicksToWait )
{
    portBaseType xReturn;
    xQUEUE *pxMutex = ( xQUEUE * ) xMutex;

    if( pdFALSE != xTaskIsSchedulerSuspended() )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xMutexTake" */
    }
    else if( NULL == pxMutex )
    {
        xReturn = errILWALID_MUTEX_HANDLE;
        /* MCDC Test Point: STD_ELSE_IF "xMutexTake" */
    }
    else if( xQueueIsQueueValid( pxMutex ) == pdFALSE )
    {
        xReturn = errILWALID_MUTEX_HANDLE;
        /* MCDC Test Point: STD_ELSE_IF "xMutexTake" */
    }
    else if( queueQUEUE_IS_MUTEX != pxMutex->uxQueueType )
    {
        xReturn = errILWALID_MUTEX_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xMutexTake" */
    }
    else
    {
        /* Comments regarding mutual exclusion as per those within xMutexGive(). */

        if( ( portTaskHandleType ) listGET_LIST_ITEM_VALUE( &( pxMutex->xMutexHolder ) ) == xTaskGetLwrrentTaskHandle() )
        {
            ( pxMutex->uxMutexRelwrsiveDepth )++;
            xReturn = pdPASS;

            /* MCDC Test Point: STD_IF "xMutexTake" */
        }
        else
        {
            xReturn = xQueueReceiveDataFromQueue( pxMutex, NULL, xTicksToWait, pdTRUE );


            /* pdPASS will only be returned if the mutex was successfully
            obtained.  The calling task may have entered the Blocked state
            before reaching here. */
            if( pdPASS == xReturn )
            {
                ( pxMutex->uxMutexRelwrsiveDepth )++;

                /* MCDC Test Point: STD_IF "xMutexTake" */
            }
            /* MCDC Test Point: ADD_ELSE "xMutexTake" */

            /* MCDC Test Point: STD_ELSE "xMutexTake" */
        }
        /* MCDC Test Point: STD_ELSE "xMutexTake" */
    }

    /* SAFERTOSTRACE QUEUETAKEMUTEXFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xMutexGive( xMutexHandleType xMutex )
{
    portBaseType xReturn;
    xQUEUE *pxMutex = ( xQUEUE * ) xMutex;

    if( pdFALSE != xTaskIsSchedulerSuspended() )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xMutexGive" */
    }
    else if( NULL == pxMutex )
    {
        xReturn = errILWALID_MUTEX_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xMutexGive" */
    }
    else if( xQueueIsQueueValid( pxMutex ) == pdFALSE )
    {
        xReturn = errILWALID_MUTEX_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xMutexGive" */
    }
    else if( queueQUEUE_IS_MUTEX != pxMutex->uxQueueType )
    {
        xReturn = errILWALID_MUTEX_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xMutexGive" */
    }
    else
    {
        /* If this is the task that holds the mutex then pxMutexHolder will not
        change outside of this task.  If this task does not hold the mutex then
        pxMutexHolder can never coincidentally equal the tasks handle, and as
        this is the only condition we are interested in it does not matter if
        pxMutexHolder is accessed simultaneously by another task.  Therefore no
        mutual exclusion is required to test the pxMutexHolder variable. */
        if( ( portTaskHandleType ) listGET_LIST_ITEM_VALUE( &( pxMutex->xMutexHolder ) ) == xTaskGetLwrrentTaskHandle() )
        {
            /* uxRelwrsiveCallCount cannot be zero if pxMutexHolder is equal to
            the task handle, however check for corruption. Also,
            uxRelwrsiveCallCount is only modified by the mutex holder, and as
            there can only be one, no mutual exclusion is required to modify the
            uxRelwrsiveCallCount member. */
            if( 0U == pxMutex->uxMutexRelwrsiveDepth )
            {
                xReturn = errMUTEX_CORRUPTED;
                /* MCDC Test Point: STD_IF "xMutexGive" */
            }
            else
            {
                ( pxMutex->uxMutexRelwrsiveDepth )--;

                /* Has the relwrsive call count unwound to 0? */
                if( 0U == pxMutex->uxMutexRelwrsiveDepth )
                {
                    /* Return the mutex.  This will automatically unblock any other
                    task that might be waiting to access the mutex. */
                    xReturn = xQueueSendDataToQueue( pxMutex, NULL, 0U, queueSEND_TO_BACK );

                    /* MCDC Test Point: STD_IF "xMutexGive" */
                }
                else
                {
                    xReturn = pdPASS;
                    /* MCDC Test Point: STD_ELSE "xMutexGive" */
                }
                /* MCDC Test Point: STD_ELSE "xMutexGive" */
            }
        }
        else
        {
            /* The mutex cannot be given because the calling task is not the
            holder. */
            xReturn = errMUTEX_NOT_OWNED_BY_CALLER;

            /* MCDC Test Point: STD_ELSE "xMutexGive" */
        }
        /* MCDC Test Point: STD_ELSE "xMutexGive" */
    }

    /* SAFERTOSTRACE QUEUEGIVEMUTEXFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xMutexPriorityDisinherit( xQUEUE *pxQueue )
{
    xTCB *pxTCB;
    xListItem *pxMutexListItem;
    portBaseType xReturn = pdFALSE;
    portUnsignedBaseType uxRequiredInheritedPriority;
    const xMiniListItem *pxMutexListEnd;
    xListItem *pxIteratedListItem;
    xMutexHandleType xMutexHandle;
    portUnsignedBaseType uxBlockedTaskPriority;

    /* xMutexPriorityDisinherit() is called from within a critical section,
     * therefore it is safe to operate on the list items within the TCB and
     * mutex objects. */

    if( queueQUEUE_IS_MUTEX == pxQueue->uxQueueType )
    {
        pxMutexListItem = &( pxQueue->xMutexHolder );

        /* Get the mutex owner from the mutex, does not matter if its NULL at this
         * stage. */
        pxTCB = ( xTCB * ) listGET_LIST_ITEM_VALUE( pxMutexListItem );

        if( NULL != pxTCB )
        {
            /* A task can only have an inherited priority if it holds the mutex.
            If the mutex is held by a task then it cannot be given from an
            interrupt, and if a mutex is given by the holding task then it must
            be the running state task. The check of pxLwrrentTCB against pxTCB
            has been performed in xMutexGive() and so is not necessary
            here. When this is ilwoked as part of prvMutexForcedMutexRelease()
            then pxLwrrentTCB may not equal pxTCB but it is allowable as the
            task is being deleted. */

            /* Remove the mutex from the task list. */
            vListRemove( pxMutexListItem );

            /* Clear the list element. */
            listSET_LIST_ITEM_VALUE( pxMutexListItem, ( portTickType ) 0U );

            /* Has the holder of the mutex inherited the priority of another
            task? */
            if( pxTCB->uxPriority != pxTCB->uxBasePriority )
            {
                /* Start with the assumption that we are going back to base
                 * priority. */
                uxRequiredInheritedPriority = pxTCB->uxBasePriority;

                /* The simple case where there are no mutexes held means
                 * that the following complex algorithm can be skipped. */
                if( 0U != listLWRRENT_LIST_LENGTH( &( pxTCB->xMutexesHeldList ) ) )
                {
                    /* We need to determine the highest priority task waiting
                     * for any of the mutexes that we still hold. This is the
                     * new inherited priority of this task which we then need
                     * to apply. */

                    /* Obtain the end of list marker for the list of held mutexes. */
                    pxMutexListEnd = listGET_END_MARKER( &( pxTCB->xMutexesHeldList ) );

                    /* Obtain the head entry of the list of held mutexes. */
                    pxIteratedListItem = listGET_HEAD_ENTRY( &( pxTCB->xMutexesHeldList ) );

                    /* Iterate through the list of held mutexes, there is at least one. */
                    do
                    {
                        xMutexHandle = ( xMutexHandleType ) listGET_LIST_ITEM_OWNER( pxIteratedListItem );

                        /* Get the priority of the highest priority task blocked
                         * on the mutex. */
                        uxBlockedTaskPriority = prvGetBlockedTaskPriority( xMutexHandle );

                        if( uxBlockedTaskPriority > uxRequiredInheritedPriority )
                        {
                            uxRequiredInheritedPriority = uxBlockedTaskPriority;
                            /* MCDC Test Point: STD_IF "xMutexPriorityDisinherit" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xMutexPriorityDisinherit" */

                        pxIteratedListItem = listGET_NEXT( pxIteratedListItem );

                        /* MCDC Test Point: WHILE_INTERNAL "xMutexPriorityDisinherit" "( ( const xMiniListItem * ) pxIteratedListItem != pxMutexListEnd )" */
                    } while( ( const xMiniListItem * ) pxIteratedListItem != pxMutexListEnd );
                }
                /* MCDC Test Point: ADD_ELSE "xMutexPriorityDisinherit" */

                /* Maybe a priority change is not required. */
                if( pxTCB->uxPriority != uxRequiredInheritedPriority )
                {
                    /* Remove ourselves from the ready list. */
                    vListRemove( &( pxTCB->xStateListItem ) );

                    /* Apply the required priority. */
                    pxTCB->uxPriority = uxRequiredInheritedPriority;

                    /* SAFERTOSTRACE TASKPRIORITYDISINHERIT */

                    /* Reset the event list item value.  It cannot be in use for
                    any other purpose if this task is running, and it must be
                    running to give back the mutex. */
                    listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( portTickType )( configMAX_PRIORITIES - pxTCB->uxPriority ) );
                    vTaskAddTaskToReadyList( pxTCB );

                    /* Return true to indicate that a context switch is required.
                    This is only actually required in the corner case whereby
                    multiple mutexes were held and the mutexes were given back
                    in an order different to that in which they were taken.
                    If a context switch did not occur when the first mutex was
                    returned, even if a task was waiting on it, then a context
                    switch should occur when the last mutex is returned whether
                    a task is waiting on it or not. */
                    xReturn = pdTRUE;

                    /* MCDC Test Point: STD_IF "xMutexPriorityDisinherit" */
                }
                /* MCDC Test Point: ADD_ELSE "xMutexPriorityDisinherit" */
            }
            /* MCDC Test Point: ADD_ELSE "xMutexPriorityDisinherit" */
        }
        /* MCDC Test Point: ADD_ELSE "xMutexPriorityDisinherit" */
        /* MCDC Test Point: STD_IF "xMutexPriorityDisinherit" */
    }
    /* MCDC Test Point: ADD_ELSE "xMutexPriorityDisinherit" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vMutexReEvaluateInheritedPriority( const xQUEUE *const pxQueue )
{
    xTCB *pxTCB;
    portUnsignedBaseType uxRequiredInheritedPriority = 0U;

    const xListItem *const pxMutexListItem = &( pxQueue->xMutexHolder );

    if( queueQUEUE_IS_MUTEX == pxQueue->uxQueueType )
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            /* Get the mutex owner from the mutex, does not matter if its NULL at this
             * stage. */
            pxTCB = ( xTCB * ) listGET_LIST_ITEM_VALUE( pxMutexListItem );
            if( NULL != pxTCB )
            {
                /* Has the holder of the mutex inherited the priority of another
                task? This will exclude the case where a task has just obtained
                a mutex as it will always have its own priority, inheritance can
                only occur when someone else tries to take it. */
                if( pxTCB->uxPriority != pxTCB->uxBasePriority )
                {
                    /* only need to re-evaluate if the inherited priority is the current
                     * priority. In all cases pxTCB->uxPriority should never be less
                     * than pxLwrrentTCB->uxPriority, however it is possible that it is
                     * greater than pxLwrrentTCB->uxPriority and blocked on some other
                     * event, in which case we do not need to re-evaluate the priority. */
                    if( pxTCB->uxPriority == pxLwrrentTCB->uxPriority )
                    {
                        /* We need to determine the highest priority task waiting
                         * for any of the mutexes that are held by pxTCB. This is
                         * the new inherited priority of this task which we then need
                         * to apply.
                         *
                         * uxMutexFindHighestBlockedTaskPriority() checks against uxBasePriority.
                         * We will never return a priority which is lower than the base priority.
                         */
                        pxTCB->uxPriority = uxMutexFindHighestBlockedTaskPriority( pxTCB );

                        /* If the holding task is READY then we need to remove it from
                         * the ready list and add it back at the correct priority. The
                         * only situation where this can occur is the the holding task
                         * is at the same priority as the current task and a round
                         * robin type context switch has oclwrred. */

                        if( xListIsContainedWithin( &( xReadyTasksLists[ pxTCB->uxPriority ] ), &( pxTCB->xStateListItem ) ) != pdFALSE )
                        {
                            vListRemove( &( pxTCB->xStateListItem ) );

                            vTaskAddTaskToReadyList( pxTCB );

                            /* MCDC Test Point: STD_IF "vMutexReEvaluateInheritedPriority" */
                        }
                        /* MCDC Test Point: ADD_ELSE "vMutexReEvaluateInheritedPriority" */

                        /* If the event list item is not being used for a secondary
                         * purpose (such as blocking on an event group), it can be
                         * updated now. Otherwise, it will be updated when the task
                         * unblocks. */
                        if( 0U == ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) )
                        {
                            /* If the task is blocked on a queue, semaphore or mutex
                             * then we should relocate it to ensure that it is queued
                             * in correctly in priority order. */
                            vListRelocateOrderedItem( &( pxTCB->xEventListItem ), ( portTickType )( configMAX_PRIORITIES - uxRequiredInheritedPriority ) );

                            /* MCDC Test Point: STD_IF "vMutexReEvaluateInheritedPriority" */
                        }
                        /* MCDC Test Point: ADD_ELSE "vMutexReEvaluateInheritedPriority" */
                    }
                    /* MCDC Test Point: ADD_ELSE "vMutexReEvaluateInheritedPriority" */
                }
                /* MCDC Test Point: ADD_ELSE "vMutexReEvaluateInheritedPriority" */
            }
            /* MCDC Test Point: ADD_ELSE "vMutexReEvaluateInheritedPriority" */
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_IF "vMutexReEvaluateInheritedPriority" */
    }
    /* MCDC Test Point: ADD_ELSE "vMutexReEvaluateInheritedPriority" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vMutexPriorityInherit( const xQUEUE *pxQueue )
{
    xTCB *pxTCB;
    portUnsignedBaseType uxOriginalPriority;
    portUnsignedBaseType uxLwrrentPriority;

    const xListItem *pxMutexListItem = &( pxQueue->xMutexHolder );

    if( pxQueue->uxQueueType == queueQUEUE_IS_MUTEX )
    {
        portENTER_CRITICAL_WITHIN_API();
        {

            /* Get the mutex owner from the mutex, does not matter if its NULL at this
             * stage. */
            pxTCB = ( xTCB * ) listGET_LIST_ITEM_VALUE( pxMutexListItem );

            /* pxTCB cannot legitimately be NULL, however do a check as basic defensive
             * coding. */
            if( NULL != pxTCB )
            {
                uxOriginalPriority = pxTCB->uxPriority;
                uxLwrrentPriority = pxLwrrentTCB->uxPriority;

                /* If the holder of the mutex has a priority below the priority of
                the task attempting to obtain the mutex then it will temporarily
                inherit the priority of the task attempting to obtain the mutex. */
                if( uxOriginalPriority < uxLwrrentPriority )
                {
                    /* Inherit the priority. */
                    pxTCB->uxPriority = uxLwrrentPriority;

                    /* SAFERTOSTRACE TASKPRIORITYINHERIT */

                    /* Adjust the mutex holder state to account for its new priority.
                     * Only reset the event list item value if the value is not being
                     * used for anything else. */
                    if( 0U == ( listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) ) & taskEVENT_LIST_ITEM_VALUE_IN_USE ) )
                    {
                        /* If the task is blocked on a queue, semaphore or mutex
                         * then we should relocate it to ensure that it is queued
                         * in correctly in priority order. */
                        vListRelocateOrderedItem( &( pxTCB->xEventListItem ), ( portTickType )( configMAX_PRIORITIES - uxLwrrentPriority ) );

                        /* MCDC Test Point: STD_IF "vMutexPriorityInherit" */
                    }
                    /* MCDC Test Point: ADD_ELSE "vMutexPriorityInherit" */

                    /* If the task being modified is in the ready state it will need
                    to be moved into a new list. */
                    if( pdFALSE != xListIsContainedWithin( &( xReadyTasksLists[ uxOriginalPriority ] ), &( pxTCB->xStateListItem ) ) )
                    {
                        vListRemove( &( pxTCB->xStateListItem ) );
                        vTaskAddTaskToReadyList( pxTCB );

                        /* MCDC Test Point: STD_IF "vMutexPriorityInherit" */
                    }
                    /* MCDC Test Point: ADD_ELSE "vMutexPriorityInherit" */
                }
                /* MCDC Test Point: ADD_ELSE "vMutexPriorityInherit" */
            }
            /* MCDC Test Point: ADD_ELSE "vMutexPriorityInherit" */
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_IF "vMutexPriorityInherit" */
    }
    /* MCDC Test Point: ADD_ELSE "vMutexPriorityInherit" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vMutexLogMutexTaken( xQUEUE *pxQueue )
{
    xTCB *pxTCB = pxLwrrentTCB; /* pxLwrrentTCB is declared volatile so take a copy. */
    xListItem *pxNewListItem = &( pxQueue->xMutexHolder );

    if( queueQUEUE_IS_MUTEX == pxQueue->uxQueueType )
    {
        listSET_LIST_ITEM_VALUE( pxNewListItem, ( portTickType ) pxTCB );

        if( NULL != pxTCB )
        {
            vListInsertEnd( &( pxTCB->xMutexesHeldList ), pxNewListItem );

            /* MCDC Test Point: STD_IF "vMutexLogMutexTaken" */
        }
        /* MCDC Test Point: ADD_ELSE "vMutexLogMutexTaken" */
    }
    /* MCDC Test Point: ADD_ELSE "vMutexLogMutexTaken" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vMutexReleaseAll( xTCB *pxTCB )
{
    const xMiniListItem *pxMutexListEnd = NULL;
    xListItem *pxIteratedListItem = NULL;

    if( 0U != listLWRRENT_LIST_LENGTH( &( pxTCB->xMutexesHeldList ) ) )
    {
        /* Obtain the end of list marker for the list of held mutexes. */
        pxMutexListEnd = listGET_END_MARKER( &( pxTCB->xMutexesHeldList ) );

        /* Obtain the head entry of the list of held mutexes. */
        pxIteratedListItem = listGET_HEAD_ENTRY( &( pxTCB->xMutexesHeldList ) );

        /* Iterate through the list of held mutexes, we know that there is at least one. */
        do
        {
            /* Return the mutex, we cannot do anything useful if an
             * error is reported so ignore. */
            ( void ) prvMutexForcedMutexRelease( listGET_LIST_ITEM_OWNER( pxIteratedListItem ) );

            pxIteratedListItem = listGET_HEAD_ENTRY( &( pxTCB->xMutexesHeldList ) );

            /* MCDC Test Point: WHILE_INTERNAL "vMutexReleaseAll" "( ( const xMiniListItem * ) pxIteratedListItem != pxMutexListEnd )" */
        } while( ( const xMiniListItem * ) pxIteratedListItem != pxMutexListEnd );
        /* MCDC Test Point: STD_IF "vMutexReleaseAll" */
    }
    /* MCDC Test Point: ADD_ELSE "vMutexReleaseAll" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portUnsignedBaseType uxMutexFindHighestBlockedTaskPriority( xTCB *pxTCB )
{
    portUnsignedBaseType uxBlockedTaskPriority;
    const xMiniListItem *pxMutexListEnd = NULL;
    xListItem *pxIteratedListItem = NULL;
    xMutexHandleType xMutexHandle = NULL;

    /* We shall go no lower than the base priority */
    portUnsignedBaseType uxHighestFoundPriority = pxTCB->uxBasePriority;

    /* Obtain the end of list marker for the list of held mutexes. */
    pxMutexListEnd = listGET_END_MARKER( &( pxTCB->xMutexesHeldList ) );

    /* Obtain the head entry of the list of held mutexes. */
    pxIteratedListItem = listGET_HEAD_ENTRY( &( pxTCB->xMutexesHeldList ) );

    /* Iterate through the list of held mutexes. If the list is empty,
     * the head will be equal to the list end, and we will not execute the loop.*/

    /* MCDC Test Point: WHILE_EXTERNAL "uxMutexFindHighestBlockedTaskPriority" "( ( const xMiniListItem * ) pxIteratedListItem != pxMutexListEnd )" */
    while( ( const xMiniListItem * ) pxIteratedListItem != pxMutexListEnd )
    {
        xMutexHandle = ( xMutexHandleType ) listGET_LIST_ITEM_OWNER( pxIteratedListItem );

        /* Get the priority of the highest priority task blocked
         * on the mutex. */
        uxBlockedTaskPriority = prvGetBlockedTaskPriority( xMutexHandle );

        if( uxBlockedTaskPriority > uxHighestFoundPriority )
        {
            uxHighestFoundPriority = uxBlockedTaskPriority;
            /* MCDC Test Point: STD_IF "uxMutexFindHighestBlockedTaskPriority" */
        }
        /* MCDC Test Point: ADD_ELSE "uxMutexFindHighestBlockedTaskPriority" */

        pxIteratedListItem = listGET_NEXT( pxIteratedListItem );

        /* MCDC Test Point: WHILE_INTERNAL "uxMutexFindHighestBlockedTaskPriority" "( ( const xMiniListItem * ) pxIteratedListItem != pxMutexListEnd )" */
    }

    return uxHighestFoundPriority;
}

/*-----------------------------------------------------------------------------
 * Private Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION static void prvMutexInitialiseMutex( xQUEUE *pxQueue )
{
    /* This function is only called with a valid handle. The queue create
     * function will set all the queue structure members correctly for a
     * generic queue, but this function is creating a mutex.  Overwrite
     * those members that need to be set differently. */

    /* Queue is a mutex. */
    pxQueue->uxQueueType = queueQUEUE_IS_MUTEX;

    /* Start with the mutex in the 'non taken' state. */
    pxQueue->uxItemsWaiting = 1U;

    /* SAFERTOSTRACE QUEUEMUTEXCREATE */

    /* MCDC Test Point: STD "prvMutexInitialiseMutex" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portUnsignedBaseType prvGetBlockedTaskPriority( xQueueHandle xQueue )
{
    portUnsignedBaseType uxEventListValue;
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue ;
    xListItem *pxBlockedListItem;

    if( pdFALSE == xListIsListEmpty( &( pxQueue->xTasksWaitingToReceive ) ) )
    {
        pxBlockedListItem = listGET_HEAD_ENTRY( &( pxQueue->xTasksWaitingToReceive ) );
        uxEventListValue = listGET_LIST_ITEM_VALUE( pxBlockedListItem );
        /* MCDC Test Point: STD_IF "prvGetBlockedTaskPriority" */
    }
    else
    {
        /* return 0 if no tasks are waiting for the queue item, so set to max. */
        uxEventListValue = configMAX_PRIORITIES;
        /* MCDC Test Point: STD_ELSE "prvGetBlockedTaskPriority" */
    }

    return ( configMAX_PRIORITIES - uxEventListValue );
}
/*---------------------------------------------------------------------------*/

KERNEL_DELETE_FUNCTION static portBaseType prvMutexForcedMutexRelease( xQueueHandle xMutex )
{
    xQUEUE *pxMutex = ( xQUEUE * ) xMutex;

    /* We are in the process of deleting a task holding the mutex and we are in
     * a critical section therefore there is no need to consider mutual
     * exclusion or simultaneous access. */

    /* Reset the relwrsive depth */
    pxMutex->uxMutexRelwrsiveDepth = 0U;

    /* MCDC Test Point: STD "prvMutexForcedMutexRelease" */

    /* Return the mutex.  This will automatically unblock any other
     * task that might be waiting to access the mutex. */
    return xQueueSendDataToQueue( pxMutex, NULL, 0U, queueSEND_TO_BACK );
}
/*---------------------------------------------------------------------------*/

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_mutexc.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "MutexCTestHeaders.h"
    #include "MutexCTest.h"
#endif /* SAFERTOS_MODULE_TEST */
