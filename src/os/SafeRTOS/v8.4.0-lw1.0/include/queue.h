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

/*
    API documentation is included in the user manual.  Do not refer to this
    file for documentation.
*/

#ifndef QUEUE_H
#define QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#define queueQUEUE_IS_MUTEX     ( 0xA5A5A5A5U )
#define queueQUEUE_IS_QUEUE     ( ~0xA5A5A5A5U )

#define queueSEND_TO_BACK       ( ( portBaseType ) 0 )
#define queueSEND_TO_FRONT      ( ( portBaseType ) 1 )


typedef void *xQueueHandle;

/*
 * Definition of the queue used by the scheduler. This is meant for internal Kernel use, not to be used directly by user code.
 * Items are queued by copy, not reference.
 */
typedef struct QueueDefinition
{
    portInt8Type *pcHead;                       /* Points to the beginning of the queue storage area. */
    portInt8Type *pcTail;                       /* Points to the byte at the end of the queue storage area. */

    portInt8Type *pcWriteTo;                    /* Points to the free next place in the storage area. */
    portInt8Type *pcReadFrom;                   /* Points to the last place that a queued item was read from. */

    xList xTasksWaitingToSend;                  /* List of tasks that are blocked waiting to post onto this queue.  Stored in priority order. */
    xList xTasksWaitingToReceive;               /* List of tasks that are blocked waiting to read from this queue.  Stored in priority order. */

    portUnsignedBaseType uxItemsWaiting;        /* The number of items lwrrently in the queue. */
    portUnsignedBaseType uxMaxNumberOfItems;    /* The length of the queue defined as the number of items it will hold, not the number of bytes. */
    portUnsignedBaseType uxItemSize;            /* The size of each item that the queue will hold. */

    portBaseType xRxLock;                       /* Set to queueUNLOCKED when the queue is not locked, or notes whether or not the queue has been accessed if the queue is locked. */
    portBaseType xTxLock;                       /* Set to queueUNLOCKED when the queue is not locked, or notes whether or not the queue has been accessed if the queue is locked. */

    portUnsignedBaseType uxQueueType;           /* Indicates whether the queue is a mutex or just a regular queue. */
    portUnsignedBaseType uxMutexRelwrsiveDepth; /* Indicates the depth of relwrsive calls to a mutex. */
    xListItem xMutexHolder;                     /* A list item so that tasks can keep a record of multiple mutexes held. */

    xList xRegisteredEventPollObjects;          /* The list of event poll objects that have registered an interest in this queue. */

    portUnsignedBaseType uxHeadMirror;          /* The uxHeadMirror will be set to the bitwise ilwerse (XOR) of the value assigned to pcHead. */

    /* SAFERTOSTRACE QUEUEDEFINITION */
} xQUEUE;
/*---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xQueueCreate( portInt8Type *pcQueueMemory, portUnsignedBaseType uxBufferLength, portUnsignedBaseType uxQueueLength, portUnsignedBaseType uxItemSize, xQueueHandle *pxCreatedQueue );
KERNEL_FUNCTION portBaseType xQueueSend( xQueueHandle xQueue, const void *const pvItemToQueue, portTickType xTicksToWait );
KERNEL_FUNCTION portBaseType xQueueSendToFront( xQueueHandle xQueue, const void *const pvItemToQueue, portTickType xTicksToWait );
KERNEL_FUNCTION portBaseType xQueueReceive( xQueueHandle xQueue, void *const pvBuffer, portTickType xTicksToWait );
KERNEL_FUNCTION portBaseType xQueuePeek( xQueueHandle xQueue, void *const pvBuffer, portTickType xTicksToWait );
KERNEL_FUNCTION portBaseType xQueueMessagesWaiting( const xQueueHandle xQueue, portUnsignedBaseType *puxMessagesWaiting );
KERNEL_FUNCTION portBaseType xQueueSendFromISR( xQueueHandle xQueue, const void *const pvItemToQueue, portBaseType *pxHigherPriorityTaskWoken );
KERNEL_FUNCTION portBaseType xQueueSendToFrontFromISR( xQueueHandle xQueue, const void *const pvItemToQueue, portBaseType *pxHigherPriorityTaskWoken );
KERNEL_FUNCTION portBaseType xQueueReceiveFromISR( xQueueHandle xQueue, void *const pvBuffer, portBaseType *pxHigherPriorityTaskWoken );

/* SAFERTOSTRACE QUEUEFUNCTIONS */

/*-----------------------------------------------------------------------------
 * Private API
 *---------------------------------------------------------------------------*/
/* Functions beyond this part are not part of the public API and are intended
for use by the kernel only. */

/*
 * Since xQueueReceive, xQueuePeek and xMutexTake require essentially the
 * same functionality, the API functions are redirected to xQueueReceiveDataFromQueue.
 */
portBaseType xQueueReceiveDataFromQueue( xQUEUE *pxQueue, void *const pvBuffer, portTickType xTicksToWait, portBaseType xRemoveDataFromQueue );

/*
 * Since xQueueSend and xMutexGive require essentially the same
 * functionality, the API functions are redirected to xQueueSendDataToQueue.
 */
KERNEL_FUNCTION portBaseType xQueueSendDataToQueue( xQUEUE *const pxQueue, const void *const pvItemToQueue, portTickType xTicksToWait, const portBaseType xPosition );

KERNEL_FUNCTION void vQueueRemoveListItemAndCheckInheritance( xListItem *pxListItem );

/* Tests the validity of the queue, returning pdTRUE if the queue is valid,
 * pdFALSE otherwise. */
KERNEL_FUNCTION portBaseType xQueueIsQueueValid( const xQUEUE *pxQueue );

#ifdef __cplusplus
}
#endif

#endif /* QUEUE_H */

