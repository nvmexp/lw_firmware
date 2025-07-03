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

#include <stdlib.h>
#include <string.h>
#include "OpenRTOS.h"
#include "queue.h"
#include "task.h"
#include "dmemovl.h"

/*-----------------------------------------------------------
 * PUBLIC LIST API dolwmented in list.h
 *----------------------------------------------------------*/

/* Constants used with the cRxLock and cTxLock structure members. */
#define queueUNLOCKED   ( ( signed portBASE_TYPE ) -1 )
#define queueERRONEOUS_UNBLOCK                  ( -1 )

/*
 * Linked list used to keep track of all queues being created
 */
xQueueHandle pxQueueListHead = NULL;

/*
 * Prototypes for public functions are included here so we don't have to
 * include the API header file (as it defines xQueueHandle differently).  These
 * functions are dolwmented in the API header file.
 */
xQueueHandle xQueueCreate     ( unsigned portBASE_TYPE uxQueueLength, unsigned portBASE_TYPE uxItemSize )
            __attribute__((section (".imem_init._xQueueCreate")));
xQueueHandle xQueueCreateInOvl( unsigned portBASE_TYPE uxQueueLength, unsigned portBASE_TYPE uxItemSize, unsigned portCHAR ucOvlIdx )
            __attribute__((section (".imem_libInit._xQueueCreateInOvl")));
signed portBASE_TYPE xQueueSend( xQueueHandle xQueue, const void * pvItemToQueue, unsigned portBASE_TYPE uxSize, portTickType xTicksToWait );
unsigned portBASE_TYPE uxQueueMessagesWaiting( xQueueHandle pxQueue );
unsigned portBASE_TYPE uxQueueSize( xQueueHandle pxQueue );
signed portBASE_TYPE xQueueSendFromISR( xQueueHandle pxQueue, const void *pvItemToQueue, unsigned portBASE_TYPE uxSize, signed portBASE_TYPE *pxHigherPriorityTaskWoken );
signed portBASE_TYPE xQueueReceive( xQueueHandle pxQueue, void *pvBuffer, unsigned portBASE_TYPE uxSize, portTickType xTicksToWait );

/*
 * Locks a queue.  Locking a queue prevents an ISR from accessing the queue
 * event lists.
 */
static void prvLockQueue( xQueueHandle pxQueue );

/*
 * Unlocks a queue locked by a call to prvLockQueue.  Locking a queue does not
 * prevent an ISR from adding or removing items to the queue, but does prevent
 * an ISR from removing tasks from the queue event lists.  If an ISR finds a
 * queue is locked it will instead increment the appropriate queue lock count
 * to indicate that a task may require unblocking.  When the queue in unlocked
 * these lock counts are inspected, and the appropriate action taken.
 */
static void prvUnlockQueue( xQueueHandle pxQueue );

/*
 * Uses a critical section to determine if there is any data in a queue.
 *
 * @return pdTRUE if the queue contains no items, otherwise pdFALSE.
 */
static signed portBASE_TYPE prvIsQueueEmpty( const xQueueHandle pxQueue );

/*
 * Uses a critical section to determine if there is any space in a queue.
 *
 * @return pdTRUE if there is no space, otherwise pdFALSE;
 */
static signed portBASE_TYPE prvIsQueueFull( const xQueueHandle pxQueue );

/*
 * Helper that copies an item into the queue.  This is done by copying the item
 * byte for byte, not by reference.  Updates the queue state to ensure it's
 * integrity after the copy.
 */
static void prvCopyQueueData( xQueueHandle pxQueue, const void *pvItemToQueue, unsigned portBASE_TYPE uxSize );
/*-----------------------------------------------------------*/


/*-----------------------------------------------------------
 * PUBLIC QUEUE MANAGEMENT API dolwmented in queue.h
 *----------------------------------------------------------*/

xQueueHandle xQueueCreate( unsigned portBASE_TYPE uxQueueLength, unsigned portBASE_TYPE uxItemSize )
{
    /* By default queues are created in resident DMEM (OS_HEAP overlay). */
    return xQueueCreateInOvl( uxQueueLength, uxItemSize, TEMP_OVL_INDEX_OS );
}

xQueueHandle xQueueCreateInOvl( unsigned portBASE_TYPE uxQueueLength, unsigned portBASE_TYPE uxItemSize, unsigned portCHAR ucOvlIdx )
{
xQUEUE *pxNewQueue;
size_t xQueueSizeInBytes;

    /* Allocate the new queue structure. */
    if( uxQueueLength > ( unsigned portBASE_TYPE ) 0 )
    {
        pxNewQueue = ( xQUEUE * ) lwosCalloc( ucOvlIdx, sizeof( xQUEUE ) );
        if( pxNewQueue != NULL )
        {
            /* Create the list of pointers to queue items.  The queue is one byte
            longer than asked for to make wrap checking easier/faster. */
            xQueueSizeInBytes = ( size_t ) ( uxQueueLength * uxItemSize ) + ( size_t ) 1;

            pxNewQueue->pcHead = ( signed portCHAR * ) lwosCalloc( ucOvlIdx, xQueueSizeInBytes );
            if( pxNewQueue->pcHead != NULL )
            {
                /* Initialise the queue members as described above where the
                queue type is defined. */
                pxNewQueue->pcTail = pxNewQueue->pcHead + ( uxQueueLength * uxItemSize );
                pxNewQueue->uxMessagesWaiting = 0;
                pxNewQueue->pcWriteTo = pxNewQueue->pcHead;
                pxNewQueue->pcReadFrom = pxNewQueue->pcHead + ( ( uxQueueLength - 1 ) * uxItemSize );
                pxNewQueue->uxLength = uxQueueLength;
                pxNewQueue->uxItemSize = uxItemSize;
                pxNewQueue->xRxLock = queueUNLOCKED;
                pxNewQueue->xTxLock = queueUNLOCKED;
                pxNewQueue->pNext = pxQueueListHead;
                pxQueueListHead = pxNewQueue;

                /* Likewise ensure the event queues start with the correct state. */
                vListInitialise( &( pxNewQueue->xTasksWaitingToSend ) );
                vListInitialise( &( pxNewQueue->xTasksWaitingToReceive ) );

                return  pxNewQueue;
            }
        }
        /* Failed to create queue. */
        falc_halt();
    }

    /* Will only reach here if we could not allocate enough memory or no memory
    was required. */
    return NULL;
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xQueueSend( xQueueHandle pxQueue, const void *pvItemToQueue, unsigned portBASE_TYPE uxSize, portTickType xTicksToWait )
{
signed portBASE_TYPE xReturn;
xTimeOutType xTimeOut;

    /* Make sure that requested size is valid. */
    if( uxSize > pxQueue->uxItemSize )
    {
        return errILWALID_SIZE;
    }
    else
    {
        /* Make sure other tasks do not access the queue. */
        vTaskSuspendAll();
        {
            /* Capture the current time status for future reference. */
            vTaskSetTimeOutState( &xTimeOut );

            /* It is important that this is the only thread/ISR that modifies the
            ready or delayed lists until xTaskResumeAll() is called.  Places where
            the ready/delayed lists are modified include:

                + vTaskDelay() -  Nothing can call vTaskDelay as the scheduler is
                  suspended, vTaskDelay() cannot be called from an ISR.
                + vTaskPrioritySet() - Has a critical section around the access.
                + vTaskSwitchContext() - This will not get exelwted while the scheduler
                  is suspended.
                + prvCheckDelayedTasks() - This will not get exelwted while the
                  scheduler is suspended.
                + xTaskCreate() - Has a critical section around the access.
                + vTaskResume() - Has a critical section around the access.
                + xTaskResumeAll() - Has a critical section around the access.
                + xTaskRemoveFromEventList - Checks to see if the scheduler is
                  suspended.  If so then the TCB being removed from the event is
                  removed from the event and added to the xPendingReadyList.
            */

            /* Make sure interrupts do not access the queue event list. */
            prvLockQueue( pxQueue );
            {
                /* It is important that interrupts to not access the event list of the
                queue being modified here.  Places where the event list is modified
                include:

                    + xQueueSendFromISR().  This checks the lock on the queue to see if
                      it has access.  If the queue is locked then the Tx lock count is
                      incremented to signify that a task waiting for data can be made ready
                      once the queue lock is removed.  If the queue is not locked then
                      a task can be moved from the event list, but will not be removed
                      from the delayed list or placed in the ready list until the scheduler
                      is unlocked.
                */

                /* If the queue is already full we may have to block. */
                do
                {
                    if( prvIsQueueFull( pxQueue ) )
                    {
                        /* The queue is full - do we want to block or just leave without
                        posting? */
                        if( xTicksToWait > ( portTickType ) 0 )
                        {
                            /* We are going to place ourselves on the xTasksWaitingToSend event
                            list, and will get woken should the delay expire, or space become
                            available on the queue.

                            As detailed above we do not require mutual exclusion on the event
                            list as nothing else can modify it or the ready lists while we
                            have the scheduler suspended and queue locked.

                            It is possible that an ISR has removed data from the queue since we
                            checked if any was available.  If this is the case then the data
                            will have been copied from the queue, and the queue variables
                            updated, but the event list will not yet have been checked to see if
                            anything is waiting as the queue is locked. */
                            vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                            /* Force a context switch now as we are blocked.  We can do
                            this from within a critical section as the task we are
                            switching to has its own context.  When we return here (i.e. we
                            unblock) we will leave the critical section as normal.

                            It is possible that an ISR has caused an event on an unrelated and
                            unlocked queue.  If this was the case then the event list for that
                            queue will have been updated but the ready lists left unchanged -
                            instead the readied task will have been added to the pending ready
                            list. */
                            taskENTER_CRITICAL();
                            {
                                /* We can safely unlock the queue and scheduler here as
                                interrupts are disabled.  We must not yield with anything
                                locked, but we can yield from within a critical section.

                                Tasks that have been placed on the pending ready list cannot
                                be tasks that are waiting for events on this queue.  See
                                in comment xTaskRemoveFromEventList(). */
                                prvUnlockQueue( pxQueue );

                                /* Resuming the scheduler may cause a yield.  If so then there
                                is no point yielding again here. */
                                if( !xTaskResumeAll() )
                                {
                                    taskYIELD();
                                }

                                /* Before leaving the critical section we have to ensure
                                exclusive access again. */
                                vTaskSuspendAll();
                                prvLockQueue( pxQueue );
                            }
                            taskEXIT_CRITICAL();
                        }
                    }

                    /* When we are here it is possible that we unblocked as space became
                    available on the queue.  It is also possible that an ISR posted to the
                    queue since we left the critical section, so it may be that again there
                    is no space.  This would only happen if a task and ISR post onto the
                    same queue. */
                    taskENTER_CRITICAL();
                    {
                        if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
                        {
                            /* There is room in the queue, copy the data into the queue. */
                            prvCopyQueueData( pxQueue, pvItemToQueue, uxSize );
                            xReturn = pdPASS;

                            /* Update the TxLock count so prvUnlockQueue knows to check for
                            tasks waiting for data to become available in the queue. */
                            ++( pxQueue->xTxLock );
                        }
                        else
                        {
                            xReturn = errQUEUE_FULL;
                        }
                    }
                    taskEXIT_CRITICAL();

                    if( xReturn == errQUEUE_FULL )
                    {
                        if( xTicksToWait > 0 )
                        {
                            if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                            {
                                xReturn = queueERRONEOUS_UNBLOCK;
                            }
                        }
                    }
                }
                while( xReturn == queueERRONEOUS_UNBLOCK );

            }
            prvUnlockQueue( pxQueue );
        }
        xTaskResumeAll();
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xQueueSendFromISR( xQueueHandle pxQueue, const void *pvItemToQueue, unsigned portBASE_TYPE uxSize, signed portBASE_TYPE *pxHigherPriorityTaskWoken )
{
signed portBASE_TYPE xReturn;

    /* Make sure that requested size is valid. */
    if( uxSize > pxQueue->uxItemSize )
    {
        return errILWALID_SIZE;
    }
    else
    {
        /* Similar to xQueueSend, except we don't block if there is no room in the
        queue.  Also we don't directly wake a task that was blocked on a queue
        read, instead we return a flag to say whether a context switch is required
        or not (i.e. has a task with a higher priority than us been woken by this
        post). */
        if( pxQueue->uxMessagesWaiting < pxQueue->uxLength )
        {
            prvCopyQueueData( pxQueue, pvItemToQueue, uxSize );

            *pxHigherPriorityTaskWoken = pdFALSE;

            /* If the queue is locked we do not alter the event list.  This will
            be done when the queue is unlocked later. */
            if( pxQueue->xTxLock == queueUNLOCKED )
            {
                if( !listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) )
                {
                    if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                    {
                        /* The task waiting has a higher priority so record that a
                        context switch is required. */
                        *pxHigherPriorityTaskWoken = pdTRUE;
                    }
                }
            }
            else
            {
                /* Increment the lock count so the task that unlocks the queue
                knows that data was posted while it was locked. */
                ++( pxQueue->xTxLock );
            }

            xReturn = pdPASS;
        }
        else
        {
            xReturn = errQUEUE_FULL;
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

signed portBASE_TYPE xQueueReceive( xQueueHandle pxQueue, void *pvBuffer, unsigned portBASE_TYPE uxSize, portTickType xTicksToWait )
{
signed portBASE_TYPE xReturn;
xTimeOutType xTimeOut;

    /* This function is very similar to xQueueSend().  See comments within
    xQueueSend() for a more detailed explanation. */

    /* Make sure that requested size is valid. */
    if( uxSize > pxQueue->uxItemSize )
    {
        return errILWALID_SIZE;
    }
    else
    {
        /* Make sure other tasks do not access the queue. */
        vTaskSuspendAll();
        {
            /* Capture the current time status for future reference. */
            vTaskSetTimeOutState( &xTimeOut );

            /* Make sure interrupts do not access the queue. */
            prvLockQueue( pxQueue );
            {
                do
                {
                    /* If there are no messages in the queue we may have to block. */
                    if( prvIsQueueEmpty( pxQueue ) )
                    {
                        /* There are no messages in the queue, do we want to block or just
                        leave with nothing? */
                        if( xTicksToWait > ( portTickType ) 0 )
                        {
                            vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                            taskENTER_CRITICAL();
                            {
                                prvUnlockQueue( pxQueue );
                                if( !xTaskResumeAll() )
                                {
                                    taskYIELD();
                                }

                                vTaskSuspendAll();
                                prvLockQueue( pxQueue );
                            }
                            taskEXIT_CRITICAL();
                        }
                    }

                    taskENTER_CRITICAL();
                    {
                        if( pxQueue->uxMessagesWaiting > ( unsigned portBASE_TYPE ) 0 )
                        {
                            pxQueue->pcReadFrom += pxQueue->uxItemSize;
                            if( pxQueue->pcReadFrom >= pxQueue->pcTail )
                            {
                                pxQueue->pcReadFrom = pxQueue->pcHead;
                            }
                            --( pxQueue->uxMessagesWaiting );
                            memcpy( ( void * ) pvBuffer, ( void * ) pxQueue->pcReadFrom,
                                    xMIN( ( unsigned ) pxQueue->uxItemSize, uxSize ) );

                            /* Increment the lock count so prvUnlockQueue knows to check for
                            tasks waiting for space to become available on the queue. */
                            ++( pxQueue->xRxLock );
                            xReturn = pdPASS;
                        }
                        else
                        {
                            xReturn = errQUEUE_EMPTY;
                        }
                    }
                    taskEXIT_CRITICAL();

                    if( xReturn == errQUEUE_EMPTY )
                    {
                        if( xTicksToWait > 0 )
                        {
                            if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                            {
                                xReturn = queueERRONEOUS_UNBLOCK;
                            }
                        }
                    }
                } while( xReturn == queueERRONEOUS_UNBLOCK );
            }
            /* We no longer require exclusive access to the queue. */
            prvUnlockQueue( pxQueue );
        }
        xTaskResumeAll();
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

unsigned portBASE_TYPE uxQueueMessagesWaiting( xQueueHandle pxQueue )
{
unsigned portBASE_TYPE uxReturn;

    taskENTER_CRITICAL();
        uxReturn = pxQueue->uxMessagesWaiting;
    taskEXIT_CRITICAL();

    return uxReturn;
}
/*-----------------------------------------------------------*/

unsigned portBASE_TYPE uxQueueSize( xQueueHandle pxQueue )
{
unsigned portBASE_TYPE uxReturn;

    /* No need for critical section since
    uxLength will not be changed after queue is created */
    uxReturn = pxQueue->uxLength;

    return uxReturn;
}
/*-----------------------------------------------------------*/

static void prvLockQueue( xQueueHandle pxQueue )
{
    taskENTER_CRITICAL();
        ++( pxQueue->xRxLock );
        ++( pxQueue->xTxLock );
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

static void prvUnlockQueue( xQueueHandle pxQueue )
{
    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED. */

    /* The lock counts contains the number of extra data items placed or
    removed from the queue while the queue was locked.  When a queue is
    locked items can be added or removed, but the event lists cannot be
    updated. */
    taskENTER_CRITICAL();
    {
        --( pxQueue->xTxLock );

        /* See if data was added to the queue while it was locked. */
        if( pxQueue->xTxLock > queueUNLOCKED )
        {
            pxQueue->xTxLock = queueUNLOCKED;

            /* Data was posted while the queue was locked.  Are any tasks
            blocked waiting for data to become available? */
            if( !listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToReceive ) ) )
            {
                /* Tasks that are removed from the event list will get added to
                the pending ready list as the scheduler is still suspended. */
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    /* The task waiting has a higher priority so record that a
                    context switch is required. */
                    vTaskMissedYield();
                }
            }
        }
    }
    taskEXIT_CRITICAL();

    /* Do the same for the Rx lock. */
    taskENTER_CRITICAL();
    {
        --( pxQueue->xRxLock );

        if( pxQueue->xRxLock > queueUNLOCKED )
        {
            pxQueue->xRxLock = queueUNLOCKED;

            if( !listLIST_IS_EMPTY( &( pxQueue->xTasksWaitingToSend ) ) )
            {
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    vTaskMissedYield();
                }
            }
        }
    }
    taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

static signed portBASE_TYPE prvIsQueueEmpty( const xQueueHandle pxQueue )
{
signed portBASE_TYPE xReturn;

    taskENTER_CRITICAL();
        xReturn = ( pxQueue->uxMessagesWaiting == ( unsigned portBASE_TYPE ) 0 );
    taskEXIT_CRITICAL();

    return xReturn;
}
/*-----------------------------------------------------------*/

static signed portBASE_TYPE prvIsQueueFull( const xQueueHandle pxQueue )
{
signed portBASE_TYPE xReturn;

    taskENTER_CRITICAL();
        xReturn = ( pxQueue->uxMessagesWaiting == pxQueue->uxLength );
    taskEXIT_CRITICAL();

    return xReturn;
}
/*-----------------------------------------------------------*/

static void prvCopyQueueData( xQueueHandle pxQueue, const void *pvItemToQueue, unsigned portBASE_TYPE uxSize )
{
    memcpy( ( void * ) pxQueue->pcWriteTo, pvItemToQueue, uxSize );
    ++( pxQueue->uxMessagesWaiting );
    pxQueue->pcWriteTo += pxQueue->uxItemSize;
    if( pxQueue->pcWriteTo >= pxQueue->pcTail )
    {
        pxQueue->pcWriteTo = pxQueue->pcHead;
    }
}
/*-----------------------------------------------------------*/

