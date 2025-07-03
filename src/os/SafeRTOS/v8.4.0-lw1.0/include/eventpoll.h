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


#ifndef EVENT_POLL_H
#define EVENT_POLL_H

#ifdef __cplusplus
extern "C" {
#endif


/*-----------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/
/* The following are the valid values for the uxEvents parameter in calls to
 * xEventPollAddObjectEvents() and xEventPollModifyObjectEvents():
 * eventpollQUEUE_MESSAGE_WAITING       - The queue contains at least one message
 *                                        waiting to be received.
 * eventpollQUEUE_SPACE_AVAILABLE       - The queue has space for at least one
 *                                        message to be sent.
 * eventpollSEMAPHORE_AVAILABLE         - The semaphore (either binary or
 *                                        counting) is available to take.
 * eventpollMUTEX_AVAILABLE             - The mutex is available to take.
 * eventpollTASK_NOTIFICATION_RECEIVED  - The task that owns the event poll
 *                                        object has received a task
 *                                        notification.
 * eventpollEVENT_GROUP_BITS_SET        - The event group has been updated due
 *                                        to a call to either
 *                                        xEventGroupSetBits() or
 *                                        xEventGroupSetBitsFromISR(). */
#define eventpollQUEUE_MESSAGE_WAITING          ( ( portUnsignedBaseType ) 0x00000001 )
#define eventpollQUEUE_SPACE_AVAILABLE          ( ( portUnsignedBaseType ) 0x00000002 )
#define eventpollSEMAPHORE_AVAILABLE            ( ( portUnsignedBaseType ) 0x00000001 )
#define eventpollMUTEX_AVAILABLE                ( ( portUnsignedBaseType ) 0x00000001 )
#define eventpollTASK_NOTIFICATION_RECEIVED     ( ( portUnsignedBaseType ) 0x00000010 )
#define eventpollEVENT_GROUP_BITS_SET           ( ( portUnsignedBaseType ) 0x00000100 )

/* This constant should not be used directly by the user application.
 * Its purpose is to allow a call to xEventPollWait() to unblock if an event
 * group associated with the event poll object is deleted while the task is
 * blocking. */
#define eventpollEVENT_GROUP_DELETED            ( ( portUnsignedBaseType ) 0x80000000U )

/* This constant indicates that no object-events have oclwrred. It is useful for
 * checking the uxEvents member of elements of the axObjectEvents parameter used
 * in calls to xEventPollWait(). */
#define eventpollNO_EVENTS                      ( ( portUnsignedBaseType ) 0 )


/*-----------------------------------------------------------------------------
 * Type Definitions
 *---------------------------------------------------------------------------*/
/* Type by which event poll objects are referenced. */
typedef void *eventPollHandleType;

/* Structure that defines an event poll "object-events". */
typedef struct eventPollObjectEvents
{
    void                   *pvObjectHandle;
    portUnsignedBaseType    uxEvents;
} eventPollObjectEventsType;

/* Control structure for each "object-events" type. */
typedef struct eventPollObjectEventControl
{
    eventPollObjectEventsType   xObjectEvent;
    xListItem                   xObjectEventListItem;
    portUnsignedBaseType        uxLwrrentEvents;
} eventPollObjectEventControlType;

/* Definition of the event poll object. */
typedef struct eventPoll
{
    xTCB                               *pxOwnerTCB;         /* Allows the event poll object to access the owner task. */
    xListItem                           xEventPollListItem; /* Allows the owner task to access the event poll object. */
    portBaseType                        xTaskIsBlocked;
    portUnsignedBaseType                uxOwnerTCBMirror;
    portUnsignedBaseType                uxNumberOfRegisteredObjectEvents;
    portUnsignedBaseType                uxMaximumRegisteredObjectEvents;
    eventPollObjectEventControlType    *paxRegisteredObjectEvents;
} eventPollType;


/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/
/* Provides the required size (in bytes) of the buffer that should be passed to
 * xEventPollCreate() in order to create an event poll object capable of holding
 * uxMaximumRegisteredObjectEvents object-events.  */
#define eventpollGET_REQUIRED_CREATE_BUFFER_SIZE( uxMaximumRegisteredObjectEvents ) \
                ( ( portUnsignedBaseType ) sizeof( eventPollType ) + ( ( portUnsignedBaseType ) sizeof( eventPollObjectEventControlType ) * ( uxMaximumRegisteredObjectEvents ) ) )

/* Initialises the supplied event poll object and supplies the corresponding
 * handle to the calling function. */
KERNEL_CREATE_FUNCTION portBaseType xEventPollCreate( portInt8Type *pcEventPollMemoryBuffer,
                                                      portUnsignedBaseType uxBufferLengthInBytes,
                                                      portUnsignedBaseType uxMaximumRegisteredObjectEvents,
                                                      portTaskHandleType xOwnerTaskHandle,
                                                      eventPollHandleType *pxEventPollHandle );

/* Registers an object-event consisting of the target object and the specified
 * events with the event poll object referenced from the supplied event poll
 * object handle.
 * Note that this can only be done if there is no current association between
 * the target object and the event poll object. If an association already
 * exists, use xEventPollModifyObjectEvents() instead. */
KERNEL_FUNCTION portBaseType xEventPollAddObjectEvents( eventPollHandleType xEventPollHandle,
                                                        void *pvTargetObjectHandle,
                                                        portUnsignedBaseType uxEvents );

/* Modifies the set of object-events for the target object that are registered
 * with the event poll object referenced from the supplied event poll object
 * handle. */
KERNEL_FUNCTION portBaseType xEventPollModifyObjectEvents( eventPollHandleType xEventPollHandle,
                                                           const void *pvTargetObjectHandle,
                                                           portUnsignedBaseType uxEvents );

/* Removes the registration of the specified target object from the event poll
 * object referenced from the supplied event poll object handle. */
KERNEL_FUNCTION portBaseType xEventPollRemoveObjectEvents( eventPollHandleType xEventPollHandle,
                                                           const void *pvTargetObjectHandle );

/* If any of the object-events registered with the event poll object referenced
 * from the supplied event poll object handle have oclwrred, they are copied to
 * the supplied object-events array. If none of the registered object-events
 * have oclwrred, xEventPollWait() will block for a maximum of xTicksToWait. */
KERNEL_FUNCTION portBaseType xEventPollWait( eventPollHandleType xEventPollHandle,
                                             eventPollObjectEventsType axObjectEvents[],
                                             portUnsignedBaseType uxObjectEventsArraySize,
                                             portUnsignedBaseType *puxNumberOfObjectEvents,
                                             portTickType xTicksToWait );


/*-----------------------------------------------------------------------------
 * Private API
 *---------------------------------------------------------------------------*/
KERNEL_DELETE_FUNCTION void vEventPollTaskDeleted( xTCB *pxTCB );

/* Function that updates all registered event poll objects whenever task
 * notifications are sent or retrieved. */
KERNEL_FUNCTION portBaseType xEventPollUpdateTaskEventPollObjects( const xTCB *pxTCB, portUnsignedBaseType uxLwrrentStatus );

/* Function that updates all registered event poll objects whenever messages are
 * sent or received on a queue. */
KERNEL_FUNCTION portBaseType xEventPollUpdateQueueEventPollObjects( const xQUEUE *const pxQueue );

/* Updates all registered event poll objects whenever event group bits are set
 * or cleared. */
KERNEL_FUNCTION portBaseType xEventPollUpdateEventGroupEventPollObjects( const eventGroupType *pxEventGroup );

/* Informs all registered event poll objects that the event group has been
 * deleted. */
KERNEL_DELETE_FUNCTION portBaseType xEventPollRemoveEventGroupEventPollObjects( const eventGroupType *pxEventGroup );

/* Initialise event poll object lists */
KERNEL_FUNCTION void vEventPollInitQueueEventPollObjects( xQUEUE *const pxQueue );
KERNEL_FUNCTION void vEventPollInitTaskEventPollObjects( xTCB *const pxTCB );
KERNEL_FUNCTION void vEventPollInitEventGroupEventPollObjects( eventGroupType *pxEventGroup );

/*---------------------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif

#endif /* EVENT_POLL_H */
