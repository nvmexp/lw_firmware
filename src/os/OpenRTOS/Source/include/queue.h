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

#ifndef QUEUE_H
#define QUEUE_H

#include "list.h"

/*
 * Definition of the queue used by the scheduler.
 * Items are queued by copy, not reference.
 */
typedef struct QueueDefinition
{
    signed portCHAR *pcHead;                    /*< Points to the beginning of the queue storage area. */
    signed portCHAR *pcTail;                    /*< Points to the byte at the end of the queue storage area.  Once more byte is allocated than necessary to store the queue items, this is used as a marker. */

    signed portCHAR *pcWriteTo;                 /*< Points to the free next place in the storage area. */
    signed portCHAR *pcReadFrom;                /*< Points to the last place that a queued item was read from. */

    xList xTasksWaitingToSend;                  /*< List of tasks that are blocked waiting to post onto this queue.  Stored in priority order. */
    xList xTasksWaitingToReceive;               /*< List of tasks that are blocked waiting to read from this queue.  Stored in priority order. */

    unsigned portBASE_TYPE uxMessagesWaiting;   /*< The number of items lwrrently in the queue. */
    unsigned portBASE_TYPE uxLength;            /*< The length of the queue defined as the number of items it will hold, not the number of bytes. */
    unsigned portBASE_TYPE uxItemSize;          /*< The size of each items that the queue will hold. */

    signed portBASE_TYPE xRxLock;               /*< Stores the number of items received from the queue (removed from the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */
    signed portBASE_TYPE xTxLock;               /*< Stores the number of items transmitted to the queue (added to the queue) while the queue was locked.  Set to queueUNLOCKED when the queue is not locked. */

    struct QueueDefinition *pNext;              /*< Points to the next queue in the linked list */
} xQUEUE;

/*-----------------------------------------------------------*/

/*
 * Inside this file xQueueHandle is a pointer to a xQUEUE structure.
 * To keep the definition private the API header file defines it as a
 * pointer to void.
 */
typedef xQUEUE * xQueueHandle;

#define xMIN(a,b) (((a)<(b))?(a):(b))

/**
 * Creates a new queue instance.  This allocates the storage required by the
 * new queue and returns a handle for the queue.
 *
 * @param uxQueueLength The maximum number of items that the queue can contain.
 *
 * @param uxItemSize The number of bytes each item in the queue will require.
 * Items are queued by copy, not by reference, so this is the number of bytes
 * that will be copied for each posted item.  Each item on the queue must be
 * the same size.
 *
 * @return If the queue is successfully create then a handle to the newly
 * created queue is returned.  If the queue cannot be created then 0 is
 * returned.
 *
 * \defgroup xQueueCreate xQueueCreate
 * \ingroup QueueManagement
 */
xQueueHandle xQueueCreate     ( unsigned portBASE_TYPE uxQueueLength, unsigned portBASE_TYPE uxItemSize );
xQueueHandle xQueueCreateInOvl( unsigned portBASE_TYPE uxQueueLength, unsigned portBASE_TYPE uxItemSize, unsigned portCHAR ucOvlIdx );
/**
 * Post an item on a queue.  The item is queued by copy, not by reference.
 * This function must not be called from an interrupt service routine.
 * See xQueueSendFromISR () for an alternative which may be used in an ISR.
 *
 * @param xQueue The handle to the queue on which the item is to be posted.
 *
 * @param pvItemToQueue A pointer to the item that is to be placed on the
 * queue.  The size of the items the queue will hold was defined when the
 * queue was created, so this many bytes will be copied from pvItemToQueue
 * into the queue storage area.
 *
 * @param uxSize The size of the item that is to be placed on the queue.
 *
 * @param xTicksToWait The maximum amount of time the task should block
 * waiting for space to become available on the queue, should it already
 * be full.  The call will return immediately if this is set to 0.  The
 * time is defined in tick periods so the constant portTICK_RATE_MS
 * should be used to colwert to real time if this is required.
 *
 * @return pdTRUE if the item was successfully posted, otherwise errQUEUE_FULL.
 *
 * \defgroup xQueueSend xQueueSend
 * \ingroup QueueManagement
 */
signed portBASE_TYPE xQueueSend( xQueueHandle xQueue, const void * pvItemToQueue, unsigned portBASE_TYPE uxSize, portTickType xTicksToWait );

/**
 * Receive an item from a queue.  The item is received by copy so a buffer of
 * adequate size must be provided.  The number of bytes copied into the buffer
 * was defined when the queue was created.
 *
 * This function must not be used in an interrupt service routine.  See
 * xQueueReceiveFromISR for an alternative that can (deleted from TOT).
 *
 * @param pxQueue The handle to the queue from which the item is to be
 * received.
 *
 * @param pvBuffer Pointer to the buffer into which the received item will
 * be copied.
 *
 * @param xTicksToWait The maximum amount of time the task should block
 * waiting for an item to receive should the queue be empty at the time
 * of the call.    The time is defined in tick periods so the constant
 * portTICK_RATE_MS should be used to colwert to real time if this is required.
 *
 * @return pdTRUE if an item was successfully received from the queue,
 * otherwise pdFALSE.
 *
 * \defgroup xQueueReceive xQueueReceive
 * \ingroup QueueManagement
 */
signed portBASE_TYPE xQueueReceive( xQueueHandle xQueue, void *pvBuffer, unsigned portBASE_TYPE uxSize, portTickType xTicksToWait );

/**
 * Return the number of messages stored in a queue.
 *
 * @param xQueue A handle to the queue being queried.
 *
 * @return The number of messages available in the queue.
 *
 * \page uxQueueMessagesWaiting uxQueueMessagesWaiting
 * \ingroup QueueManagement
 */
unsigned portBASE_TYPE uxQueueMessagesWaiting( xQueueHandle xQueue );

/**
 * Return the length of a queue.
 * This function is for the WAR of bug 1810498, 1801088.by
 * stop queueing commands to PG queue if PG queue is about full.
 *
 * @param xQueue A handle to the queue being queried.
 *
 * @return The total length in the queue.
 *
 * \page uxQueueSize uxQueueSize
 * \ingroup QueueManagement
 */
unsigned portBASE_TYPE uxQueueSize( xQueueHandle xQueue );

/**
 * Post an item on a queue.  It is safe to use this function from within an
 * interrupt service routine.
 *
 * Items are queued by copy not reference so it is preferable to only
 * queue small items, especially when called from an ISR.  In most cases
 * it would be preferable to store a pointer to the item being queued.
 *
 * @param xQueue The handle to the queue on which the item is to be posted.
 *
 * @param pvItemToQueue A pointer to the item that is to be placed on the
 * queue.  The size of the items the queue will hold was defined when the
 * queue was created, so this many bytes will be copied from pvItemToQueue
 * into the queue storage area.
 *
 * @param uxSize The size of the item that is to be placed on the queue.
 *
 * @param pxHigherPriorityTaskWoken A pointer to a logical variable to hold
 * the information if task was woken when queueing a mesage to it.
 *
 * @return pdPASS if item was successfully placed into the queue.
 *
 * \defgroup xQueueSendFromISR xQueueSendFromISR
 * \ingroup QueueManagement
 */
signed portBASE_TYPE xQueueSendFromISR( xQueueHandle pxQueue, const void *pvItemToQueue, unsigned portBASE_TYPE uxSize, signed portBASE_TYPE *pxHigherPriorityTaskWoken );

#endif

