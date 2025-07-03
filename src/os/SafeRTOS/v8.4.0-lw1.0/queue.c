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
#include "eventgroups.h"
#include "eventpoll.h"
#include "mutex.h"

/* Constants used with the xRxLock and xTxLock structure members. */
#define queueUNLOCKED           ( ( portBaseType ) -1 )
#define queueLOCKED_UNMODIFIED  ( ( portBaseType ) 0 )

/*
 * FromISR version of xQueueSendDataToQueue().
 */
KERNEL_FUNCTION static portBaseType prvQueueSendDataToQueueFromISR( xQUEUE *const pxQueue, const void *const pvItemToQueue, const portBaseType xPosition, portBaseType *pxHigherPriorityTaskWoken );

/*
 * Checks the validity of all the parameters passed to the xQueueCreate
 * function.  Returns a negative number if any parameter is incorrect.
 */
KERNEL_CREATE_FUNCTION static portBaseType prvCheckQueueCreateParameters( portInt8Type *pcQueueMemory, portUnsignedBaseType uxBufferLength, portUnsignedBaseType uxQueueLength, portUnsignedBaseType uxItemSize, const xQueueHandle *pxQueue );

/*
 * Checks the validity of commonly passed parameters to Queue API functions.
 * Returns a negative number if any parameter is incorrect.
 */
KERNEL_FUNCTION static portBaseType prvCheckCommonParameters( xQUEUE *pxQueue, const void *const pvBuffer );

/*
 * Unlocks a queue locked by a call to prvLOCK_QUEUE.  Locking a queue does not
 * prevent an ISR from adding or removing items to the queue, but does prevent
 * an ISR from removing tasks from the queue event lists.  If an ISR finds a
 * queue is locked it will instead set the appropriate queue lock to MODIFIED
 * to indicate that a task may require unblocking.  When the queue in unlocked
 * these lock variables are inspected, and the appropriate action taken.
 */
KERNEL_FUNCTION static void prvUnlockQueue( xQueueHandle xQueue );

/*
 * Uses a critical section to determine if there is any data in a queue.
 *
 * @return pdTRUE if the queue contains no items, otherwise pdFALSE.
 */
KERNEL_FUNCTION static portBaseType prvIsQueueEmpty( const xQueueHandle xQueue );

/*
 * Uses a critical section to determine if there is any space in a queue.
 *
 * @return pdTRUE if there is no space, otherwise pdFALSE.
 */
KERNEL_FUNCTION static portBaseType prvIsQueueFull( const xQueueHandle xQueue );

/*
 * Function that copies an item into the front of a queue.
 * This is done by copying the item byte for byte, not by reference.
 * Updates the queue state to ensure its integrity after the copy.
 */
KERNEL_FUNCTION static portBaseType prvCopyDataToQueueFront( xQUEUE *const pxQueue, const void *const pvItemToQueue, portBaseType xIsFromISR );

/*
 * Function that copies an item into the back of a queue.
 * This is done by copying the item byte for byte, not by reference.
 * Updates the queue state to ensure its integrity after the copy.
 */
KERNEL_FUNCTION static portBaseType prvCopyDataToQueueBack( xQUEUE *const pxQueue, const void *const pvItemToQueue, portBaseType xIsFromISR );

/*
 * Function that copies an item from a queue into a buffer.
 * This is done by copying the item byte for byte, not by reference.
 * Updates the queue state to insure its integrity after the copy.
*/
KERNEL_FUNCTION static portBaseType prvCopyDataFromQueue( xQUEUE *const pxQueue, void *const pvBuffer, portBaseType xIsFromISR );

/* MCDC Test Point: PROTOTYPE */

/*---------------------------------------------------------------------------*/

/*
 * Macro to mark a queue as locked.  Locking a queue prevents an ISR from
 * accessing the queue event lists.  Only set it to locked if it is lwrrently
 * unlocked as it may already be locked and modified.
 */
#define prvLOCK_QUEUE( pxQueue )                                    \
    portENTER_CRITICAL_WITHIN_API();                                \
    {                                                               \
        if( queueUNLOCKED == ( pxQueue )->xRxLock )                 \
        {                                                           \
            ( pxQueue )->xRxLock = queueLOCKED_UNMODIFIED;          \
                                                                    \
            /* MCDC Test Point: STD_IF_IN_MACRO "prvLOCK_QUEUE" */  \
        }                                                           \
        /* MCDC Test Point: ADD_ELSE_IN_MACRO "prvLOCK_QUEUE" */    \
                                                                    \
        if( queueUNLOCKED == ( pxQueue )->xTxLock )                 \
        {                                                           \
            ( pxQueue )->xTxLock = queueLOCKED_UNMODIFIED;          \
                                                                    \
            /* MCDC Test Point: STD_IF_IN_MACRO "prvLOCK_QUEUE" */  \
        }                                                           \
        /* MCDC Test Point: ADD_ELSE_IN_MACRO "prvLOCK_QUEUE" */    \
    }                                                               \
    portEXIT_CRITICAL_WITHIN_API()

/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xQueueCreate( portInt8Type *pcQueueMemory,
                                                  portUnsignedBaseType uxBufferLength,
                                                  portUnsignedBaseType uxQueueLength,
                                                  portUnsignedBaseType uxItemSize,
                                                  xQueueHandle *pxCreatedQueue )
{
    portBaseType xReturn;
    xQUEUE *pxQueue = ( xQUEUE * ) pcQueueMemory;

    xReturn = prvCheckQueueCreateParameters( pcQueueMemory, uxBufferLength, uxQueueLength, uxItemSize, pxCreatedQueue );

    if( pdPASS == xReturn )
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            if( xQueueIsQueueValid( pxQueue ) != pdFALSE )
            {
                xReturn = errQUEUE_ALREADY_IN_USE;

                /* MCDC Test Point: STD_IF "xQueueCreate" */
            }
            else if( pdPASS != portCOPY_DATA_TO_TASK( pxCreatedQueue, &pxQueue, ( portUnsignedBaseType ) sizeof( xQueueHandle ) ) )
            {
                /* We pre-initialise the handle to make sure the destination address is valid. */

                xReturn = errILWALID_DATA_RANGE;

                /* MCDC Test Point: STD_ELSE_IF "xQueueCreate" */
            }
            else
            {
                /* Initialise the queue members as described above where the
                 * queue type is defined. */
                pxQueue->pcHead = ( portInt8Type * )( ( portUnsignedBaseType ) pcQueueMemory + ( portUnsignedBaseType ) sizeof( xQUEUE ) );
                pxQueue->pcTail = ( portInt8Type * )( ( portUnsignedBaseType ) pxQueue->pcHead + ( uxQueueLength * uxItemSize ) );
                pxQueue->uxItemsWaiting = ( portUnsignedBaseType ) 0U;
                pxQueue->pcWriteTo = pxQueue->pcHead;
                pxQueue->pcReadFrom = ( portInt8Type * )( ( portUnsignedBaseType ) pxQueue->pcHead + ( ( uxQueueLength - 1U ) * uxItemSize ) );
                pxQueue->uxMaxNumberOfItems = uxQueueLength;
                pxQueue->uxItemSize = uxItemSize;
                pxQueue->xRxLock = queueUNLOCKED;
                pxQueue->xTxLock = queueUNLOCKED;
                pxQueue->uxQueueType = queueQUEUE_IS_QUEUE;

                /* These elements are only used if the queue is a mutex, however
                 * ensure that they are initialised anyway. */
                vMutexInitQueueVariables( pxQueue );

                /* Likewise ensure the event lists start with the correct state. */
                vListInitialise( &( pxQueue->xTasksWaitingToSend ) );
                vListInitialise( &( pxQueue->xTasksWaitingToReceive ) );

                /* Initialise event poll object member if event poll is included.
                 * Otherwise a weak function will be called which does nothing. */
                vEventPollInitQueueEventPollObjects( pxQueue );

                /* The mutex inheritance mechanism requires a reverse look up
                 * so set the owner of the end marker to the queue itself. */
                listSET_LIST_ITEM_OWNER( &( pxQueue->xTasksWaitingToSend.xListEnd ), pxQueue );
                listSET_LIST_ITEM_OWNER( &( pxQueue->xTasksWaitingToReceive.xListEnd ), pxQueue );

                /* Finally, Set Queue Head Memory Check Mirror Value */
                pxQueue->uxHeadMirror = ~( ( portUnsignedBaseType ) pxQueue->pcHead );

                /* SAFERTOSTRACE QUEUECREATE */

                /* MCDC Test Point: STD_ELSE "xQueueCreate" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
    }
    /* MCDC Test Point: ADD_ELSE "xQueueCreate" */

    /* SAFERTOSTRACE QUEUECREATEFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueSend( xQueueHandle xQueue, const void *const pvItemToQueue, portTickType xTicksToWait )
{
    portBaseType xReturn;

    /* Cast param 'xQueueHandle xQueue' to 'xQUEUE *' type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    if( xTaskIsSchedulerSuspended() != pdFALSE )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xQueueSend" */
    }
    else
    {
        xReturn = prvCheckCommonParameters( pxQueue, pvItemToQueue );

        /* MCDC Test Point: STD_ELSE "xQueueSend" */
    }

    if( pdPASS == xReturn )
    {
        xReturn = xQueueSendDataToQueue( pxQueue, pvItemToQueue, xTicksToWait, queueSEND_TO_BACK );

        /* MCDC Test Point: STD_IF "xQueueSend" */
    }
    /* MCDC Test Point: ADD_ELSE "xQueueSend" */

    /* SAFERTOSTRACE QUEUESENDBACKFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueSendToFront( xQueueHandle xQueue, const void *const pvItemToQueue, portTickType xTicksToWait )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to 'xQUEUE *' type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    if( xTaskIsSchedulerSuspended() != pdFALSE )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xQueueSendToFront" */
    }
    else
    {
        xReturn = prvCheckCommonParameters( pxQueue, pvItemToQueue );

        /* MCDC Test Point: STD_ELSE "xQueueSendToFront" */
    }

    if( pdPASS == xReturn )
    {
        xReturn = xQueueSendDataToQueue( pxQueue, pvItemToQueue, xTicksToWait, queueSEND_TO_FRONT );

        /* MCDC Test Point: STD_IF "xQueueSendToFront" */
    }
    /* MCDC Test Point: ADD_ELSE "xQueueSendToFront" */

    /* SAFERTOSTRACE QUEUESENDFRONTFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueSendFromISR( xQueueHandle xQueue, const void *const pvItemToQueue, portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    /* Similar to xQueueSend, except we don't block if there is no room
     * in the queue. Also we don't directly wake a task that was blocked on a
     * queue read, instead we return a flag to say whether a context switch is
     * required or not (i.e. has a task with a higher priority than us been
     * woken by this post). */

    xReturn = prvCheckCommonParameters( pxQueue, pvItemToQueue );

    if( pdPASS == xReturn )
    {
        if( NULL == pxHigherPriorityTaskWoken )
        {
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: STD_IF "xQueueSendFromISR" */
        }
        else
        {
            xReturn = prvQueueSendDataToQueueFromISR( pxQueue, pvItemToQueue, queueSEND_TO_BACK, pxHigherPriorityTaskWoken );

            /* MCDC Test Point: STD_ELSE "xQueueSendFromISR" */
        }
    }
    /* MCDC Test Point: ADD_ELSE "xQueueSendFromISR" */

    /* SAFERTOSTRACE QUEUESENDBACKFROMISRFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueSendToFrontFromISR( xQueueHandle xQueue, const void *const pvItemToQueue, portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    /* Similar to xQueueSendToFront, except we don't block if there is no room
     * in the queue. Also we don't directly wake a task that was blocked on a
     * queue read, instead we return a flag to say whether a context switch is
     * required or not (i.e. has a task with a higher priority than us been
     * woken by this post). */

    xReturn = prvCheckCommonParameters( pxQueue, pvItemToQueue );

    if( pdPASS == xReturn )
    {
        if( NULL == pxHigherPriorityTaskWoken )
        {
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: STD_IF "xQueueSendToFrontFromISR" */
        }
        else
        {
            xReturn = prvQueueSendDataToQueueFromISR( pxQueue, pvItemToQueue, queueSEND_TO_FRONT, pxHigherPriorityTaskWoken );

            /* MCDC Test Point: STD_ELSE "xQueueSendToFrontFromISR" */
        }
    }
    /* MCDC Test Point: ADD_ELSE "xQueueSendToFrontFromISR" */

    /* SAFERTOSTRACE QUEUESENDFRONTFROMISRFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvQueueSendDataToQueueFromISR( xQUEUE *const pxQueue, const void *const pvItemToQueue, const portBaseType xPosition, portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn;
    portUnsignedBaseType uxSavedInterruptStatus;
    portBaseType xHigherPriorityTaskWoken = pdFALSE;

    uxSavedInterruptStatus = taskSET_INTERRUPT_MASK_FROM_ISR();
    {
        if( pxQueue->uxItemsWaiting < pxQueue->uxMaxNumberOfItems )
        {
            /* SAFERTOSTRACE QUEUESENDFROMISR */

            if( queueSEND_TO_FRONT == xPosition )
            {
                xReturn = prvCopyDataToQueueFront( pxQueue, pvItemToQueue, pdTRUE );

                /* MCDC Test Point: STD_IF "prvQueueSendDataToQueueFromISR" */
            }
            else
            {
                xReturn = prvCopyDataToQueueBack( pxQueue, pvItemToQueue, pdTRUE );

                /* MCDC Test Point: STD_ELSE "prvQueueSendDataToQueueFromISR" */
            }

            if( pdPASS == xReturn )
            {
                /* If the queue is locked we do not alter the event list.
                 * This will be done when the queue is unlocked later. */
                if( queueUNLOCKED == pxQueue->xTxLock )
                {
                    if( xListIsListEmpty( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                        {
                            /* The task waiting has a higher priority so record that a
                             * context switch is required. */
                            xHigherPriorityTaskWoken = pdTRUE;

                            /* MCDC Test Point: STD_IF "prvQueueSendDataToQueueFromISR" */
                        }
                        /* MCDC Test Point: ADD_ELSE "prvQueueSendDataToQueueFromISR" */
                    }
                    /* MCDC Test Point: ADD_ELSE "prvQueueSendDataToQueueFromISR" */
                }
                else
                {
                    /* Increment the lock count so the task that unlocks the queue
                    knows how many data items were posted while it was locked. */
                    ++( pxQueue->xTxLock );

                    /* MCDC Test Point: STD_ELSE "prvQueueSendDataToQueueFromISR" */
                }

                /* If event poll has been included, update all registered event poll objects.
                 * Otherwise, an empty weak function will be called. */
                if( pdTRUE == xEventPollUpdateQueueEventPollObjects( pxQueue ) )
                {
                    /* A higher priority task has been unblocked due to updating
                     * an event poll object. */
                    xHigherPriorityTaskWoken = pdTRUE;

                    /* MCDC Test Point: STD_IF "prvQueueSendDataToQueueFromISR" */
                }
                /* MCDC Test Point: ADD_ELSE "prvQueueSendDataToQueueFromISR" */
            }
            /* MCDC Test Point: ADD_ELSE "prvQueueSendDataToQueueFromISR" */
        }
        else
        {
            xReturn = errQUEUE_FULL;

            /* MCDC Test Point: STD_ELSE "prvQueueSendDataToQueueFromISR" */
        }

        /* Has a higher priority task been woken? */
        if( pdTRUE == xHigherPriorityTaskWoken )
        {
            /* MCDC Test Point: STD_IF "prvQueueSendDataToQueueFromISR" */

            if( pdPASS != portCOPY_DATA_TO_ISR( pxHigherPriorityTaskWoken, &xHigherPriorityTaskWoken, ( portUnsignedBaseType ) sizeof( portBaseType ) ) )
            {
                xReturn = errILWALID_DATA_RANGE;

                /* MCDC Test Point: STD_IF "prvQueueSendDataToQueueFromISR" */
            }
            /* MCDC Test Point: ADD_ELSE "prvQueueSendDataToQueueFromISR" */
        }
        /* MCDC Test Point: ADD_ELSE "prvQueueSendDataToQueueFromISR" */
    }
    taskCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueReceive( xQueueHandle xQueue, void *const pvBuffer, portTickType xTicksToWait )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    if( xTaskIsSchedulerSuspended( ) != pdFALSE )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xQueueReceive" */
    }
    else
    {
        xReturn = prvCheckCommonParameters( pxQueue, pvBuffer );

        /* MCDC Test Point: STD_ELSE "xQueueReceive" */
    }

    if( pdPASS == xReturn )
    {
        /* Pass the parameters straight to xQueueReceiveDataFromQueue() with
         * xRemoveDataFromQueue set to pdTRUE so that the data is actually removed
         * from the queue. */
        xReturn = xQueueReceiveDataFromQueue( pxQueue, pvBuffer, xTicksToWait, pdTRUE );

        /* MCDC Test Point: STD_IF "xQueueReceive" */
    }
    /* MCDC Test Point: ADD_ELSE "xQueueReceive" */

    /* SAFERTOSTRACE QUEUERECEIVEFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueuePeek( xQueueHandle xQueue, void *const pvBuffer, portTickType xTicksToWait )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    if( xTaskIsSchedulerSuspended( ) != pdFALSE )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xQueuePeek" */
    }
    else
    {
        xReturn = prvCheckCommonParameters( pxQueue, pvBuffer );

        /* MCDC Test Point: STD_ELSE "xQueuePeek" */
    }

    if( pdPASS == xReturn )
    {
        /* Pass the parameters straight to xQueueReceiveDataFromQueue() with
         * xRemoveDataFromQueue set to pdFALSE so that the data is left on the
         * queue */
        xReturn = xQueueReceiveDataFromQueue( pxQueue, pvBuffer, xTicksToWait, pdFALSE );

        /* MCDC Test Point: STD_IF "xQueuePeek" */
    }
    /* MCDC Test Point: ADD_ELSE "xQueuePeek" */

    /* SAFERTOSTRACE QUEUEPEEKFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueReceiveFromISR( xQueueHandle xQueue, void *const pvBuffer, portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn;
    portUnsignedBaseType uxSavedInterruptStatus;
    /* Cast param 'xQueueHandle xQueue' to 'xQUEUE *' type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;
    portBaseType xHigherPriorityTaskWoken = pdFALSE;

    if( NULL == pxHigherPriorityTaskWoken )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xQueueReceiveFromISR" */
    }
    else
    {
        xReturn = prvCheckCommonParameters( pxQueue, pvBuffer );

        /* MCDC Test Point: STD_ELSE "xQueueReceiveFromISR" */
    }

    if( pdPASS == xReturn )
    {
        uxSavedInterruptStatus = taskSET_INTERRUPT_MASK_FROM_ISR();
        {
            if( pxQueue->uxItemsWaiting > 0U )
            {
                /* SAFERTOSTRACE QUEUERECEIVEFROMISR */

                /* We cannot block from an ISR, so check there is data available. */

                /* Copy the data from the queue. */
                xReturn = prvCopyDataFromQueue( pxQueue, pvBuffer, pdTRUE );
                if( pdPASS == xReturn )
                {
                    /* If the queue is locked we will not modify the event list.
                     * Instead we update the lock count so the task that unlocks the queue
                     * will know that an ISR has removed data while the queue was locked. */
                    if( queueUNLOCKED == pxQueue->xRxLock )
                    {
                        if( xListIsListEmpty( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                        {
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                            {
                                /* The task waiting has a higher priority than us so
                                 * report that a context switch is required. */
                                xHigherPriorityTaskWoken = pdTRUE;

                                /* MCDC Test Point: STD_IF "xQueueReceiveFromISR" */
                            }
                            /* MCDC Test Point: ADD_ELSE "xQueueReceiveFromISR" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xQueueReceiveFromISR" */
                    }
                    else
                    {
                        /* Increment the Rx lock so the task that unlocks the queue knows
                         * how many items were removed while it was locked. */
                        ++( pxQueue->xRxLock );

                        /* MCDC Test Point: STD_ELSE "xQueueReceiveFromISR" */
                    }

                    /* If event poll has been included, update all registered event poll objects.
                     * Otherwise, an empty weak function will be called. */
                    if( pdTRUE == xEventPollUpdateQueueEventPollObjects( pxQueue ) )
                    {
                        /* A higher priority task has been unblocked due to
                         * updating an event poll object. */
                        xHigherPriorityTaskWoken = pdTRUE;

                        /* MCDC Test Point: STD_IF "xQueueReceiveFromISR" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xQueueReceiveFromISR" */
                }
                /* MCDC Test Point: ADD_ELSE "xQueueReceiveFromISR" */
            }
            else
            {
                xReturn = errQUEUE_EMPTY;

                /* MCDC Test Point: STD_ELSE "xQueueReceiveFromISR" */
            }

            /* Has a higher priority task been woken? */
            if( pdTRUE == xHigherPriorityTaskWoken )
            {
                /* MCDC Test Point: STD_IF "xQueueReceiveFromISR" */

                if( pdPASS != portCOPY_DATA_TO_ISR( pxHigherPriorityTaskWoken, &xHigherPriorityTaskWoken, ( portUnsignedBaseType ) sizeof( portBaseType ) ) )
                {
                    xReturn = errILWALID_DATA_RANGE;

                    /* MCDC Test Point: STD_IF "xQueueReceiveFromISR" */
                }
                /* MCDC Test Point: ADD_ELSE "xQueueReceiveFromISR" */
            }
            /* MCDC Test Point: ADD_ELSE "xQueueReceiveFromISR" */
        }
        taskCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

        /* MCDC Test Point: STD_IF "xQueueReceiveFromISR" */
    }
    /* MCDC Test Point: ADD_ELSE "xQueueReceiveFromISR" */


    /* SAFERTOSTRACE QUEUERECEIVEFROMISRFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueMessagesWaiting( const xQueueHandle xQueue, portUnsignedBaseType *puxMessagesWaiting )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to 'xQUEUE *' type. */
    const xQUEUE *pxQueue = ( const xQUEUE * ) xQueue;

    if( NULL == pxQueue )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xQueueMessagesWaiting" */
    }
    else if( xQueueIsQueueValid( pxQueue ) == pdFALSE )
    {
        xReturn = errILWALID_QUEUE_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xQueueMessagesWaiting" */
    }
    else if( NULL == puxMessagesWaiting )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xQueueMessagesWaiting" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            if( pdPASS != portCOPY_DATA_TO_TASK( puxMessagesWaiting, &pxQueue->uxItemsWaiting, ( portUnsignedBaseType ) sizeof( portUnsignedBaseType ) ) )
            {
                xReturn = errILWALID_DATA_RANGE;
                /* MCDC Test Point: STD_IF "xQueueMessagesWaiting" */
            }
            else
            {
                /* The number of messages waiting has been successfully retrieved. */
                xReturn = pdPASS;

                /* MCDC Test Point: STD_ELSE "xQueueMessagesWaiting" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_ELSE "xQueueMessagesWaiting" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static void prvUnlockQueue( xQueueHandle xQueue )
{
    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED. */
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    /* The lock variable indicates if extra data items were placed or
    removed from the queue while the queue was locked.  When a queue is
    locked items can be added or removed, but the event lists cannot be
    updated. */
    portENTER_CRITICAL_WITHIN_API();
    {
        /* MCDC Test Point: WHILE_EXTERNAL "prvUnlockQueue" "( pxQueue->xTxLock > queueLOCKED_UNMODIFIED )" */

        /* See if data was added to the queue while it was locked.  We should
        unblock a task for each posted item as we don't want a task to unblock,
        remove just one multiple items, then go and do something else without
        unblocking any other tasks due to the remaining data.  If the first
        task drains the queue then any other unblocked tasks will simply
        re-block for the remains of their block period. */
        while( pxQueue->xTxLock > queueLOCKED_UNMODIFIED )
        {
            /* Data was posted while the queue was locked.  Are any tasks
            blocked waiting for data to become available? */
            if( xListIsListEmpty( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
            {
                /* Tasks that are removed from the event list will get added to
                the pending ready list as the scheduler is still suspended. */
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) != pdFALSE )
                {
                    /* The task waiting has a higher priority so record that a
                    context switch is required. */
                    vTaskPendYield();

                    /* MCDC Test Point: STD_IF "prvUnlockQueue" */
                }
                /* MCDC Test Point: ADD_ELSE "prvUnlockQueue" */

                --( pxQueue->xTxLock );
            }
            else
            {
                /* MCDC Test Point: STD_ELSE "prvUnlockQueue" */

                break;
            }
            /* MCDC Test Point: WHILE_INTERNAL "prvUnlockQueue" "( pxQueue->xTxLock > queueLOCKED_UNMODIFIED )" */
        }

        pxQueue->xTxLock = queueUNLOCKED;
    }
    portEXIT_CRITICAL_WITHIN_API();

    /* Do the same for the Rx lock. */
    portENTER_CRITICAL_WITHIN_API();
    {
        /* MCDC Test Point: WHILE_EXTERNAL "prvUnlockQueue" "( pxQueue->xRxLock > queueLOCKED_UNMODIFIED )" */
        while( pxQueue->xRxLock > queueLOCKED_UNMODIFIED )
        {
            if( xListIsListEmpty( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
            {
                if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) ) != pdFALSE )
                {
                    vTaskPendYield();

                    /* MCDC Test Point: STD_IF "prvUnlockQueue" */
                }
                /* MCDC Test Point: ADD_ELSE "prvUnlockQueue" */

                --( pxQueue->xRxLock );
            }
            else
            {
                /* MCDC Test Point: STD_ELSE "prvUnlockQueue" */

                break;
            }
            /* MCDC Test Point: WHILE_INTERNAL "prvUnlockQueue" "( pxQueue->xRxLock > queueLOCKED_UNMODIFIED )" */
        }

        pxQueue->xRxLock = queueUNLOCKED;
    }
    portEXIT_CRITICAL_WITHIN_API();
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvIsQueueEmpty( const xQueueHandle xQueue )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    const xQUEUE *pxQueue = ( const xQUEUE * ) xQueue;


    /* This operation doesn't need to be in a critical section, as base types are atomically accessed */
    if( 0U == pxQueue->uxItemsWaiting )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "prvIsQueueEmpty" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "prvIsQueueEmpty" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvIsQueueFull( const xQueueHandle xQueue )
{
    portBaseType xReturn;
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    const xQUEUE *pxQueue = ( const xQUEUE * ) xQueue;


    /* This operation doesn't need to be in a critical section, as base types are atomically accessed */
    if( pxQueue->uxMaxNumberOfItems == pxQueue->uxItemsWaiting )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "prvIsQueueFull" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "prvIsQueueFull" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvCopyDataToQueueFront( xQUEUE *const pxQueue, const void *const pvItemToQueue, portBaseType xIsFromISR )
{
    portBaseType xReturn;

    /* Copy the data. */
    if( pdFALSE == xIsFromISR )
    {
        xReturn = portCOPY_DATA_FROM_TASK( pxQueue->pcReadFrom, pvItemToQueue, pxQueue->uxItemSize );
        /* MCDC Test Point: STD_IF "prvCopyDataToQueueFront" */
    }
    else
    {
        xReturn = portCOPY_DATA_FROM_ISR( pxQueue->pcReadFrom, pvItemToQueue, pxQueue->uxItemSize );
        /* MCDC Test Point: STD_ELSE "prvCopyDataToQueueFront" */
    }

    /* Update control members. */
    if( pdPASS == xReturn )
    {
        /* Increment number of items. */
        ++( pxQueue->uxItemsWaiting );

        /* Increment write pointer. */
        pxQueue->pcReadFrom -= pxQueue->uxItemSize;
        if( pxQueue->pcReadFrom < pxQueue->pcHead )
        {
            pxQueue->pcReadFrom = ( portInt8Type * )( ( portUnsignedBaseType ) pxQueue->pcTail - pxQueue->uxItemSize );

            /* MCDC Test Point: STD_IF "prvCopyDataToQueueFront" */
        }
        /* MCDC Test Point: ADD_ELSE "prvCopyDataToQueueFront" */
    }
    /* MCDC Test Point: ADD_ELSE "prvCopyDataToQueueFront" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvCopyDataToQueueBack( xQUEUE *const pxQueue, const void *const pvItemToQueue, portBaseType xIsFromISR )
{
    portBaseType xReturn;

    /* Copy the data. */
    if( pdFALSE == xIsFromISR )
    {
        xReturn = portCOPY_DATA_FROM_TASK( pxQueue->pcWriteTo, pvItemToQueue, pxQueue->uxItemSize );
        /* MCDC Test Point: STD_IF "prvCopyDataToQueueBack" */
    }
    else
    {
        xReturn = portCOPY_DATA_FROM_ISR( pxQueue->pcWriteTo, pvItemToQueue, pxQueue->uxItemSize );
        /* MCDC Test Point: STD_ELSE "prvCopyDataToQueueBack" */
    }

    /* Update control members. */
    if( pdPASS == xReturn )
    {
        /* Increment number of items. */
        ++( pxQueue->uxItemsWaiting );

        /* Increment write pointer. */
        pxQueue->pcWriteTo += pxQueue->uxItemSize;
        if( pxQueue->pcWriteTo >= pxQueue->pcTail )
        {
            pxQueue->pcWriteTo = pxQueue->pcHead;

            /* MCDC Test Point: STD_IF "prvCopyDataToQueueBack" */
        }
        /* MCDC Test Point: ADD_ELSE "prvCopyDataToQueueBack" */
    }
    /* MCDC Test Point: ADD_ELSE "prvCopyDataToQueueBack" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvCopyDataFromQueue( xQUEUE *const pxQueue, void *const pvBuffer, portBaseType xIsFromISR )
{
    portBaseType xReturn;
    portInt8Type *pcReadFrom;

    /* Increment read pointer. */
    pcReadFrom = ( portInt8Type * )( ( portUnsignedBaseType ) pxQueue->pcReadFrom + pxQueue->uxItemSize );
    if( pcReadFrom >= pxQueue->pcTail )
    {
        pcReadFrom = pxQueue->pcHead;

        /* MCDC Test Point: STD_IF "prvCopyDataFromQueue" */
    }
    /* MCDC Test Point: ADD_ELSE "prvCopyDataFromQueue" */

    /* Copy the data. */
    if( pdFALSE == xIsFromISR )
    {
        xReturn = portCOPY_DATA_TO_TASK( pvBuffer, pcReadFrom, pxQueue->uxItemSize );
        /* MCDC Test Point: STD_IF "prvCopyDataFromQueue" */
    }
    else
    {
        xReturn = portCOPY_DATA_TO_ISR( pvBuffer, pcReadFrom, pxQueue->uxItemSize );
        /* MCDC Test Point: STD_ELSE "prvCopyDataFromQueue" */
    }

    if( pdPASS == xReturn )
    {
        /* Update read pointer. */
        pxQueue->pcReadFrom = pcReadFrom;

        /* Decrement number of items. */
        --( pxQueue->uxItemsWaiting );

        /* MCDC Test Point: STD_IF "prvCopyDataFromQueue" */
    }
    /* MCDC Test Point: ADD_ELSE "prvCopyDataFromQueue" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueSendDataToQueue( xQUEUE *const pxQueue, const void *const pvItemToQueue, portTickType xTicksToWait, const portBaseType xPosition )
{
    portBaseType xReturn = pdPASS;
    portBaseType xContextSwitchRequired;
    xTimeOutType xTimeOut;

    do
    {
        /* If xTicksToWait is zero then we are not going to block even
        if there is no room in the queue to post. */
        if( xTicksToWait > ( portTickType ) 0U )
        {
            /* Ensure no scheduling or queue manipulation can occur.
            This does not disable interrupts. */
            vTaskSuspendScheduler();
            prvLOCK_QUEUE( pxQueue );

            if( pdPASS == xReturn )
            {
                /* This is the first time through the function - note the
                time now.  This must be done with the scheduler suspended
                to ensure time does not elapse before the call to
                xTaskCheckForTimeOut() below - otherwise we would not
                attempt to block. */
                vTaskSetTimeOut( &xTimeOut );

                /* MCDC Test Point: STD_IF "xQueueSendDataToQueue" */
            }
            /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */

            if( pdTRUE == prvIsQueueFull( pxQueue ) )
            {
                /* Need to call xTaskCheckForTimeout again as time could
                have passed since it was last called if this is not the
                first time around this loop. */
                if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                {
                    /* SAFERTOSTRACE QUEUESENDBLOCKED */

                    /* Place ourselves on the event list of the queue. */
                    vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToSend ), xTicksToWait );

                    /* Unlocking the queue means queue events can effect the
                    event list.  It is possible that interrupts oclwrring now
                    remove this task from the event list again - but as the
                    scheduler is suspended the task will go onto the pending
                    ready last instead of the actual ready list. */
                    prvUnlockQueue( pxQueue );

                    /* Resuming the scheduler will move tasks from the pending
                    ready list into the ready list - so it is feasible that this
                    task is already in a ready list before it yields - in which
                    case the yield will not cause a context switch unless there
                    is also a higher priority task in the pending ready list. */
                    if( pdFALSE == xTaskResumeScheduler() )
                    {
                        taskYIELD_WITHIN_API();

                        /* MCDC Test Point: STD_IF "xQueueSendDataToQueue" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */
                }
                else
                {
                    prvUnlockQueue( pxQueue );
                    ( void ) xTaskResumeScheduler();

                    /* MCDC Test Point: STD_ELSE "xQueueSendDataToQueue" */
                }
            }
            else
            {
                /* The queue was not full so we can just unlock the
                scheduler and queue again before carrying on. */
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeScheduler();

                /* MCDC Test Point: STD_ELSE "xQueueSendDataToQueue" */
            }
        }
        /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */

        /* Higher priority tasks and interrupts can execute during
        this time and could possibly refill the queue - even if we
        unblocked because space became available. */

        portENTER_CRITICAL_WITHIN_API();
        {
            /* Is there room on the queue now?  To be running we must be
            the highest priority task wanting to access the queue. */
            if( pxQueue->uxItemsWaiting < pxQueue->uxMaxNumberOfItems )
            {
                /* SAFERTOSTRACE QUEUESEND */

                if( queueSEND_TO_FRONT == xPosition )
                {
                    xReturn = prvCopyDataToQueueFront( pxQueue, pvItemToQueue, pdFALSE );

                    /* MCDC Test Point: STD_IF "xQueueSendDataToQueue" */
                }
                else
                {
                    xReturn = prvCopyDataToQueueBack( pxQueue, pvItemToQueue, pdFALSE );

                    /* MCDC Test Point: STD_ELSE "xQueueSendDataToQueue" */
                }

                if( pdPASS == xReturn )
                {

                    /* If the queue is a mutex, it is no longer being held and we need to tell
                     * the task to remove the mutex from its list and disinherit priority
                     * if necessary. */
                    xContextSwitchRequired = xMutexPriorityDisinherit( pxQueue );

                    /* If there was a task waiting for data to arrive on the queue,
                     * then unblock it now. */
                    if( xListIsListEmpty( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                    {
                        if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) == pdTRUE )
                        {
                            /* The unblocked task has a priority higher than
                            our own so request an immediate yield. */
                            xContextSwitchRequired = pdTRUE;

                            /* MCDC Test Point: STD_IF "xQueueSendDataToQueue" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */

                    /* If event poll has been included, update all registered event poll objects.
                     * Otherwise, an empty weak function will be called. */
                    if( pdTRUE == xEventPollUpdateQueueEventPollObjects( pxQueue ) )
                    {
                        /* A higher priority task has been unblocked due to updating
                         * an event poll object. */
                        xContextSwitchRequired = pdTRUE;

                        /* MCDC Test Point: STD_IF "xQueueSendDataToQueue" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */

                    if( pdTRUE == xContextSwitchRequired )
                    {
                        /* Yield if requested due to a queue unblock event or
                         * a mutex priority (dis)inheritance action. */
                        taskYIELD_WITHIN_API();

                        /* MCDC Test Point: STD_IF "xQueueSendDataToQueue" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */
                }
                /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */
            }
            else
            {
                /* Setting xReturn to errQUEUE_FULL will force its timeout
                to be re-evaluated.  This is necessary in case interrupts
                and higher priority tasks accessed the queue between this
                task being unblocked and subsequently attempting to write
                to the queue. */
                xReturn = errQUEUE_FULL;

                /* MCDC Test Point: STD_ELSE "xQueueSendDataToQueue" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        if( errQUEUE_FULL == xReturn )
        {
            if( xTicksToWait > ( portTickType ) 0U )
            {
                if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                {
                    xReturn = errERRONEOUS_UNBLOCK;

                    /* MCDC Test Point: STD_IF "xQueueSendDataToQueue" */
                }
                /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */
            }
            /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */
        }
        /* MCDC Test Point: ADD_ELSE "xQueueSendDataToQueue" */

        /* MCDC Test Point: WHILE_INTERNAL "xQueueSendDataToQueue" "( errERRONEOUS_UNBLOCK == xReturn )" */
    } while( errERRONEOUS_UNBLOCK == xReturn );

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueReceiveDataFromQueue( xQUEUE *pxQueue, void *const pvBuffer, portTickType xTicksToWait, portBaseType xRemoveDataFromQueue )
{
    portBaseType xReturn = pdPASS;
    xTimeOutType xTimeOut;
    portInt8Type *pcOriginalReadPosition;
    portBaseType xContextSwitchRequired = pdFALSE;

    do
    {
        if( xTicksToWait > ( portTickType ) 0U )
        {
            vTaskSuspendScheduler();
            prvLOCK_QUEUE( pxQueue );

            if( pdPASS == xReturn )
            {
                /* This is the first time through the function - note the
                time now.  This must be done with the scheduler suspended
                to ensure time does not elapse before the call to
                xTaskCheckForTimeOut() below - otherwise we would not
                attempt to block. */
                vTaskSetTimeOut( &xTimeOut );

                /* MCDC Test Point: STD_IF "xQueueReceiveDataFromQueue" */
            }
            /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */

            if( pdTRUE == prvIsQueueEmpty( pxQueue ) )
            {
                /* Need to call xTaskCheckForTimeout again as time could
                have passed since it was last called if this is not the
                first time around this loop. */
                if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                {
                    /* If the queue is a mutex, inherit priority */
                    vMutexPriorityInherit( pxQueue );

                    /* SAFERTOSTRACE QUEUERECEIVEBLOCKED */
                    vTaskPlaceOnEventList( &( pxQueue->xTasksWaitingToReceive ), xTicksToWait );
                    prvUnlockQueue( pxQueue );
                    if( xTaskResumeScheduler() == pdFALSE )
                    {
                        taskYIELD_WITHIN_API();
                        /* MCDC Test Point: STD_IF "xQueueReceiveDataFromQueue" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */


                    /* If this is a mutex, check to see whether we need to
                     * disinherit the priority of the task holding it
                     * as a result of a blocking task timing out. This call
                     * will also be made if the mutex has been obtained but
                     * in this case no actions will be taken. */
                    vMutexReEvaluateInheritedPriority( pxQueue );
                }
                else
                {
                    prvUnlockQueue( pxQueue );
                    ( void ) xTaskResumeScheduler();

                    /* MCDC Test Point: STD_ELSE "xQueueReceiveDataFromQueue" */
                }
            }
            else
            {
                prvUnlockQueue( pxQueue );
                ( void ) xTaskResumeScheduler();

                /* MCDC Test Point: STD_ELSE "xQueueReceiveDataFromQueue" */
            }
        }
        /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */

        portENTER_CRITICAL_WITHIN_API();
        {
            if( pxQueue->uxItemsWaiting > 0U )
            {
                /*
                 * Remember our read position so it can be reset if the data
                 * is to be left on the queue.
                 */
                pcOriginalReadPosition = pxQueue->pcReadFrom;

                /* SAFERTOSTRACE QUEUERECEIVE */

                /* Retrieve the data. */
                xReturn = prvCopyDataFromQueue( pxQueue, pvBuffer, pdFALSE );
                if( pdPASS == xReturn )
                {
                    if( pdTRUE == xRemoveDataFromQueue )
                    {
                        /* Record the information required to implement
                        priority inheritance should it become necessary. */
                        vMutexLogMutexTaken( pxQueue );

                        /* Are there any tasks waiting to send to the queue? */
                        if( xListIsListEmpty( &( pxQueue->xTasksWaitingToSend ) ) == pdFALSE )
                        {
                            xContextSwitchRequired = xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToSend ) );

                            /* MCDC Test Point: STD_IF "xQueueReceiveDataFromQueue" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */

                        /* If event poll has been included, update all registered event poll objects.
                         * Otherwise, an empty weak function will be called. */
                        if( pdTRUE == xEventPollUpdateQueueEventPollObjects( pxQueue ) )
                        {
                            /* A higher priority task has been unblocked due to
                             * updating an event poll object. */
                            xContextSwitchRequired = pdTRUE;

                            /* MCDC Test Point: STD_IF "xQueueReceiveDataFromQueue" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */

                        if( pdTRUE == xContextSwitchRequired )
                        {
                            /*
                             * The unblocked task has a higher priority than
                             * this task.
                             */
                            taskYIELD_WITHIN_API();

                            /* MCDC Test Point: STD_IF "xQueueReceiveDataFromQueue" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */
                    }
                    else
                    {
                        /*
                         * We are not removing the data, so reset the read
                         * pointer and message count.
                         */
                        pxQueue->pcReadFrom = pcOriginalReadPosition;
                        ++( pxQueue->uxItemsWaiting );

                        /*
                         * The data is being left in the queue, so see if there
                         * are any other tasks waiting for the data.
                         */
                        if( xListIsListEmpty( &( pxQueue->xTasksWaitingToReceive ) ) == pdFALSE )
                        {
                            if( xTaskRemoveFromEventList( &( pxQueue->xTasksWaitingToReceive ) ) == pdTRUE )
                            {
                                /*
                                 * The unblocked task has a higher priority than
                                 * this task.
                                 */
                                taskYIELD_WITHIN_API();

                                /* MCDC Test Point: STD_IF "xQueueReceiveDataFromQueue" */
                            }
                            /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */

                        /* MCDC Test Point: STD_ELSE "xQueueReceiveDataFromQueue" */
                    }
                }
                /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */
            }
            else
            {
                xReturn = errQUEUE_EMPTY;

                /* MCDC Test Point: STD_ELSE "xQueueReceiveDataFromQueue" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        if( errQUEUE_EMPTY == xReturn )
        {
            if( xTicksToWait > ( portTickType ) 0U )
            {
                if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                {
                    xReturn = errERRONEOUS_UNBLOCK;

                    /* MCDC Test Point: STD_IF "xQueueReceiveDataFromQueue" */
                }
                /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */
            }
            /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */
        }
        /* MCDC Test Point: ADD_ELSE "xQueueReceiveDataFromQueue" */

        /* MCDC Test Point: WHILE_INTERNAL "xQueueReceiveDataFromQueue" "( errERRONEOUS_UNBLOCK == xReturn )" */
    } while( errERRONEOUS_UNBLOCK == xReturn );

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vQueueRemoveListItemAndCheckInheritance( xListItem *pxListItem )
{
    xQUEUE *pxQueue;
    xList *pxTaskWaitingList;

    /* This function is called from within a critical section, therefore it is
     * safe to operate on the list items within the TCB and mutex objects. */

    /* Identify the list that contains the specified xListItem - this must be
     * done BEFORE removing the item from the list. */
    pxTaskWaitingList = pxListItem->pvContainer;

    /* If the list is part of a Queue/Mutex, then the Queue/Mutex handle will
     * be the owner of the end marker. */
    pxQueue = ( xQUEUE * ) listGET_LIST_ITEM_OWNER( listGET_END_MARKER( pxTaskWaitingList ) );

    /* Remove the xListItem from the list. */
    vListRemove( pxListItem );

    /* If the list was an event group, then there is no 'owner' of the list. */
    if( NULL != pxQueue )
    {
        /* MCDC Test Point: STD_IF "vQueueRemoveListItemAndCheckInheritance" */

        /* If the list is a mutex then force re-evaluation of any
         * inherited priority. */
        vMutexReEvaluateInheritedPriority( pxQueue );
    }
    /* MCDC Test Point: ADD_ELSE "vQueueRemoveListItemAndCheckInheritance" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xQueueIsQueueValid( const xQUEUE *pxQueue )
{
    portBaseType xReturn;

    if( ( portUnsignedBaseType )( pxQueue->pcHead ) == ( portUnsignedBaseType ) ~( pxQueue->uxHeadMirror ) )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xQueueIsQueueValid" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xQueueIsQueueValid" */
    }

    return xReturn;

}
/*---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Local Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION static portBaseType prvCheckQueueCreateParameters( portInt8Type *pcQueueMemory, portUnsignedBaseType uxBufferLength, portUnsignedBaseType uxQueueLength, portUnsignedBaseType uxItemSize, const xQueueHandle *pxQueue )
{
    portBaseType xReturn = pdPASS;

    if( 0U != ( ( ( portUnsignedBaseType ) pcQueueMemory ) & portWORD_ALIGNMENT_MASK ) )
    {
        xReturn = errILWALID_ALIGNMENT;

        /* MCDC Test Point: STD_IF "prvCheckQueueCreateParameters" */
    }
    /* MCDC Test Point: ADD_ELSE "prvCheckQueueCreateParameters" */

    if( 0U == uxQueueLength )
    {
        xReturn = errILWALID_QUEUE_LENGTH;

        /* MCDC Test Point: STD_IF "prvCheckQueueCreateParameters" */
    }
    /* MCDC Test Point: ADD_ELSE "prvCheckQueueCreateParameters" */

    if( uxBufferLength != ( ( uxQueueLength * uxItemSize ) + ( portUnsignedBaseType ) sizeof( xQUEUE ) ) )
    {
        xReturn = errILWALID_BUFFER_SIZE;

        /* MCDC Test Point: STD_IF "prvCheckQueueCreateParameters" */
    }
    /* MCDC Test Point: ADD_ELSE "prvCheckQueueCreateParameters" */

    if( ( NULL == pcQueueMemory ) || ( NULL == pxQueue ) )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: NULL (Separate expansion) */
    }
    /* MCDC Test Point: EXP_IF_OR "prvCheckQueueCreateParameters" "( NULL == pcQueueMemory )" "( NULL == pxQueue )" "Deviate: TT" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvCheckCommonParameters( xQUEUE *pxQueue, const void *const pvBuffer )
{
    portBaseType xReturn;

    if( NULL == pxQueue )
    {
        xReturn = errILWALID_QUEUE_HANDLE;

        /* MCDC Test Point: STD_IF "prvCheckCommonParameters" */
    }
    else
    {
        if( xQueueIsQueueValid( pxQueue ) == pdFALSE )
        {
            xReturn = errILWALID_QUEUE_HANDLE;

            /* MCDC Test Point: STD_IF "prvCheckCommonParameters" */
        }

        /* SAFERTOSTRACE QUEUENOTSEMAPHORETRACE */

        else if( queueQUEUE_IS_QUEUE != pxQueue->uxQueueType )

            /* SAFERTOSTRACE QUEUENOTSEMAPHORETYPE */

        {
            xReturn = errILWALID_QUEUE_HANDLE;

            /* MCDC Test Point: STD_ELSE_IF "prvCheckCommonParameters" */
        }
        else if( ( NULL == pvBuffer ) && ( 0U != pxQueue->uxItemSize ) )
        {
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: NULL (expanded elsewhere) "prvCheckCommonParameters" */
        }
        else
        {
            xReturn = pdPASS;

            /* MCDC Test Point: STD_ELSE "prvCheckCommonParameters" */
        }
        /* MCDC Test Point: EXP_IF_AND "prvCheckCommonParameters" "( NULL == pvBuffer )" "( 0U != pxQueue->uxItemSize )" "Deviate: FF" */

        /* MCDC Test Point: STD_ELSE "prvCheckCommonParameters" */
    }

    return xReturn;
}

/*-----------------------------------------------------------------------------
 * Weak stub Function Definitions
 *---------------------------------------------------------------------------*/

/* If event poll is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION portBaseType xEventPollUpdateQueueEventPollObjects( const xQUEUE *const pxQueue )
{
    /* MCDC Test Point: STD "WEAK_xEventPollUpdateQueueEventPollObjects" */

    ( void ) pxQueue;
    return pdFALSE;
}
/*---------------------------------------------------------------------------*/

/* If event poll is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vEventPollInitQueueEventPollObjects( xQUEUE *const pxQueue )
{
    /* MCDC Test Point: STD "WEAK_vEventPollInitQueueEventPollObjects" */

    ( void ) pxQueue;
}
/*---------------------------------------------------------------------------*/

/* If mutex is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vMutexInitQueueVariables( xQUEUE *pxQueue )
{
    /* MCDC Test Point: STD "WEAK_vMutexInitQueueVariables" */

    ( void ) pxQueue;
}
/*---------------------------------------------------------------------------*/

/* If mutex is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vMutexLogMutexTaken( xQUEUE *pxQueue )
{
    /* MCDC Test Point: STD "WEAK_vMutexLogMutexTaken" */

    ( void ) pxQueue;
}
/*---------------------------------------------------------------------------*/

/* If mutex is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vMutexPriorityInherit( const xQUEUE *pxQueue )
{
    /* MCDC Test Point: STD "WEAK_vMutexPriorityInherit" */

    ( void ) pxQueue;
}
/*---------------------------------------------------------------------------*/

/* If mutex is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION portBaseType xMutexPriorityDisinherit( xQUEUE *pxQueue )
{
    /* MCDC Test Point: STD "WEAK_xMutexPriorityDisinherit" */

    ( void ) pxQueue;
    return pdFALSE;
}
/*---------------------------------------------------------------------------*/

/* If mutex is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vMutexReEvaluateInheritedPriority( const xQUEUE *const pxQueue )
{
    /* MCDC Test Point: STD "WEAK_vMutexReEvaluateInheritedPriority" */

    ( void ) pxQueue;
}
/*---------------------------------------------------------------------------*/

/* SAFERTOSTRACE QUEUEGETQUEUETYPE */

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_queuec.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "QueueCTestHeaders.h"
    #include "QueueCTest.h"
#endif /* SAFERTOS_MODULE_TEST */

