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

/* MCDC Test Point: PROTOTYPE */

/*-----------------------------------------------------------------------------
 * Private Type Definitions
 *---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 * Private Constant Definitions
 *---------------------------------------------------------------------------*/
/* The set of all valid queue events. */
#define eventpollALL_QUEUE_EVENTS               ( eventpollQUEUE_MESSAGE_WAITING | eventpollQUEUE_SPACE_AVAILABLE )

/* A non-zero constant used to indicate an invalid object-event index. */
#define eventpollILWALID_OBJECT_EVENT_INDEX     ( portMAX_LIST_ITEM_VALUE )


/*-----------------------------------------------------------------------------
 * Private Function Declarations
 *---------------------------------------------------------------------------*/
/* prvCheckEventPollCreateParameters() checks the input parameters of a call to
 * xEventPollCreate().
 * If the parameters are all valid, prvCheckEventPollCreateParameters() returns
 * pdPASS; otherwise an appropriate error code is returned. */
KERNEL_CREATE_FUNCTION static portBaseType prvCheckEventPollCreateParameters( portInt8Type *pcEventPollMemoryBuffer,
                                                                              portUnsignedBaseType uxBufferLengthInBytes,
                                                                              portUnsignedBaseType uxMaximumRegisteredObjectEvents,
                                                                              portTaskHandleType xOwnerTaskHandle,
                                                                              const eventPollHandleType *pxEventPollHandle );

/* prvCheckEventPollConfigureParameters() checks the input parameters to the
 * functions that configure an event poll object - xEventPollAddObjectEvents(),
 * xEventPollModifyObjectEvents() and xEventPollRemoveObjectEvents().
 * If the parameters are valid, prvCheckEventPollConfigureParameters() returns
 * pdPASS; otherwise an appropriate error code is returned. */
KERNEL_FUNCTION static portBaseType prvCheckEventPollConfigureParameters( const eventPollType *pxEventPollObject,
                                                                          const void *pvTargetObjectHandle,
                                                                          portUnsignedBaseType uxEvents );

/* prvGetObjectEventIndex() inspects the array of object-events belonging to the
 * event poll object in order to retrieve either the index of the object-events
 * structure that relates to the specified object handle or the index of the
 * next free location in the object-events array.
 * If the index is successfully retrieved, the memory location referenced by
 * puxObjectEventIndex is set to the retrieved index and
 * prvGetObjectEventIndex() returns pdPASS.
 * If the index cannot be found, the memory location referenced by
 * puxObjectEventIndex is set to either the first free location in the
 * object-events array or eventpollILWALID_OBJECT_EVENT_INDEX if there are no
 * free elements remaining - either way, pdFAIL is returned to the calling
 * function. */
KERNEL_FUNCTION static portBaseType prvGetObjectEventIndex( const eventPollType *pxEventPollObject,
                                                            const void *pvTargetObjectHandle,
                                                            portUnsignedBaseType *puxObjectEventIndex );

KERNEL_FUNCTION static portBaseType prvUpdateObjectList( const xList *pxRegisteredEventPollObjects,
                                                         portUnsignedBaseType uxUpdatedStatus );

KERNEL_FUNCTION static portBaseType prvUpdateTargetObjectStatus( const xListItem *pxObjectEventListItem,
                                                                 portUnsignedBaseType uxLwrrentEvents );

/* If the supplied events are valid, prvRegisterEventPollObject() calls the
 * appropriate function to add the event poll object to the target object's list
 * of registered event poll objects. */
KERNEL_FUNCTION static portBaseType prvRegisterEventPollObject( void *pvTargetObjectHandle,
                                                                portUnsignedBaseType uxEvents,
                                                                xListItem *pxEventPollObjectEventListItem );

/* If any of the object-events associated with the event poll object have
 * oclwrred, they are copied into the supplied axObjectEvents array. */
KERNEL_FUNCTION static portBaseType prvGetOclwrredObjectEvents( eventPollType *pxEventPollObject,
                                                                eventPollObjectEventsType axObjectEvents[],
                                                                portUnsignedBaseType uxObjectEventsArraySize,
                                                                portUnsignedBaseType *puxNumberOfObjectEvents );

/* Free the specified element of the object-events array. */
KERNEL_FUNCTION static void prvFreeObjectEvent( eventPollType *pxEventPollObject,
                                                eventPollObjectEventControlType *pxObjectEventControl );

KERNEL_FUNCTION static portBaseType prvQueueAddEventPollListItem( xQueueHandle xQueue,
                                                                  xListItem *pxEventPollObjectEventListItem );

/* Add the specified event poll object list item to the list of event poll
 * objects owned by this task. */
KERNEL_FUNCTION static portBaseType prvTaskAddEventPollListItem( xTCB *pxTCB,
                                                                 xListItem *pxEventPollListItem );

/* Add the specified event poll object to the event group's list of event poll
 * objects to be updated whenever bits are set in the event group. */
KERNEL_FUNCTION static portBaseType prvEventGroupAddEventPollListItem( eventGroupHandleType xEventGroupHandle,
                                                                       xListItem *pxEventPollObjectEventListItem );

/* Retrieves the current notification status of the specified task for the
 * purpose of updating the associated event poll object-events. */
KERNEL_FUNCTION static portBaseType prvQueryTaskNotificationStatus( portTaskHandleType xTask,
                                                                    portUnsignedBaseType *puxLwrrentEvents );

KERNEL_FUNCTION static portUnsignedBaseType prvGetEventPollQueueStatus( const xQUEUE *const pxQueue );

KERNEL_FUNCTION static portUnsignedBaseType prvAreAnyEventGroupBitsSet( const eventGroupType *pxEventGroup );

KERNEL_FUNCTION static portBaseType prvEventGroupDeleted( xListItem *pxObjectEventListItem );

/*-----------------------------------------------------------------------------
 * Private Variable Declarations
 *---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 * Public Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xEventPollCreate( portInt8Type *pcEventPollMemoryBuffer,
                                                      portUnsignedBaseType uxBufferLengthInBytes,
                                                      portUnsignedBaseType uxMaximumRegisteredObjectEvents,
                                                      portTaskHandleType xOwnerTaskHandle,
                                                      eventPollHandleType *pxEventPollHandle )
{
    portBaseType xResult;
    eventPollType *pxEventPollObject;
    eventPollObjectEventControlType *pxObjectEventControl;
    xTCB *pxOwnerTCB;
    portUnsignedBaseType uxAddress;
    portUnsignedBaseType uxIndex;

    /* Check the input parameters. */
    xResult = prvCheckEventPollCreateParameters( pcEventPollMemoryBuffer,
                                                 uxBufferLengthInBytes,
                                                 uxMaximumRegisteredObjectEvents,
                                                 xOwnerTaskHandle,
                                                 pxEventPollHandle );
    if( pdPASS == xResult )
    {
        /* Obtain access to the event poll object. */
        pxEventPollObject = ( eventPollType * ) pcEventPollMemoryBuffer;

        portENTER_CRITICAL_WITHIN_API();
        {
            /* Is this event poll object already in use? */
            if( ( portUnsignedBaseType )( pxEventPollObject->pxOwnerTCB ) == ~( pxEventPollObject->uxOwnerTCBMirror ) )
            {
                /* If the provided event poll object is valid, then it must be
                 * in use already and cannot be reused. */
                xResult = errEVENT_POLL_OBJECT_ALREADY_IN_USE;

                /* MCDC Test Point: STD_IF "xEventPollCreate" */
            }
            /* Pre-initialise the handle to make sure the destination address is
             * valid. */
            else if( pdPASS != portCOPY_DATA_TO_TASK( pxEventPollHandle, &pxEventPollObject, ( portUnsignedBaseType ) sizeof( eventPollHandleType ) ) )
            {
                xResult = errILWALID_DATA_RANGE;

                /* MCDC Test Point: STD_ELSE_IF "xEventPollCreate" */
            }
            else
            {
                /* Determine the owner of this event poll object. */
                pxOwnerTCB = taskGET_TCB_FROM_HANDLE( xOwnerTaskHandle );

                /* Initialise the list item used to associate this event poll
                 * object with a task.
                 * Note: The xItemValue member of the event poll list item is
                 * used to locate the object-event that is configured for a task
                 * notification event. */
                vListInitialiseItem( &( pxEventPollObject->xEventPollListItem ) );
                listSET_LIST_ITEM_OWNER( &( pxEventPollObject->xEventPollListItem ),
                                         pxEventPollObject );
                listSET_LIST_ITEM_VALUE( &( pxEventPollObject->xEventPollListItem ),
                                         eventpollILWALID_OBJECT_EVENT_INDEX );

                /* Register the event poll object with the owner task. */
                xResult = prvTaskAddEventPollListItem( pxOwnerTCB,
                                                       &( pxEventPollObject->xEventPollListItem ) );
                if( pdPASS == xResult )
                {
                    /* Set the owner of this event poll object. */
                    pxEventPollObject->pxOwnerTCB = pxOwnerTCB;

                    /* The task is not lwrrently blocked. */
                    pxEventPollObject->xTaskIsBlocked = pdFALSE;

                    /* Initialise the number of registered object-events and
                     * store the maximum number of object-events that can be
                     * registered with this event poll object. */
                    pxEventPollObject->uxNumberOfRegisteredObjectEvents = ( portUnsignedBaseType ) 0;
                    pxEventPollObject->uxMaximumRegisteredObjectEvents = uxMaximumRegisteredObjectEvents;

                    /* Store the location of the array of
                     * eventPollObjectEventControlType for this event poll
                     * object. */
                    uxAddress = ( portUnsignedBaseType ) pcEventPollMemoryBuffer;
                    uxAddress += ( portUnsignedBaseType ) sizeof( eventPollType );
                    pxEventPollObject->paxRegisteredObjectEvents = ( eventPollObjectEventControlType * ) uxAddress;

                    /* Initialise this event poll object's register of
                     * object-events. */
                    for( uxIndex = 0U; uxIndex < uxMaximumRegisteredObjectEvents; uxIndex++ )
                    {
                        pxObjectEventControl = &( pxEventPollObject->paxRegisteredObjectEvents[ uxIndex ] );

                        pxObjectEventControl->xObjectEvent.pvObjectHandle = NULL;
                        pxObjectEventControl->xObjectEvent.uxEvents = eventpollNO_EVENTS;
                        pxObjectEventControl->uxLwrrentEvents = eventpollNO_EVENTS;

                        vListInitialiseItem( &( pxObjectEventControl->xObjectEventListItem ) );
                        listSET_LIST_ITEM_OWNER( &( pxObjectEventControl->xObjectEventListItem ),
                                                 pxEventPollObject );
                        listSET_LIST_ITEM_VALUE( &( pxObjectEventControl->xObjectEventListItem ),
                                                 ( ( portTickType ) uxIndex ) );

                        /* MCDC Test Point: STD "xEventPollCreate" */
                    }

                    /* Set the mirror variable. */
                    pxEventPollObject->uxOwnerTCBMirror = ~( ( portUnsignedBaseType ) pxEventPollObject->pxOwnerTCB );

                    /* MCDC Test Point: STD_IF "xEventPollCreate" */
                }
                /* MCDC Test Point: ADD_ELSE "xEventPollCreate" */

                /* MCDC Test Point: STD_ELSE "xEventPollCreate" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_IF "xEventPollCreate" */
    }
    /* MCDC Test Point: ADD_ELSE "xEventPollCreate" */

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventPollAddObjectEvents( eventPollHandleType xEventPollHandle,
                                                        void *pvTargetObjectHandle,
                                                        portUnsignedBaseType uxEvents )
{
    portBaseType xResult;
    eventPollType *pxEventPollObject;
    portUnsignedBaseType uxObjectEventIndex = 0U;
    eventPollObjectEventControlType *pxObjectEvent;

    /* Obtain access to the event poll object. */
    pxEventPollObject = ( eventPollType * ) xEventPollHandle;

    /* Check the input parameters. */
    xResult = prvCheckEventPollConfigureParameters( pxEventPollObject,
                                                    pvTargetObjectHandle,
                                                    uxEvents );
    if( pdPASS == xResult )
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            /* Is the specified object handle already registered with this event
             * poll object? */
            if( pdPASS == prvGetObjectEventIndex( pxEventPollObject,
                                                  pvTargetObjectHandle,
                                                  &uxObjectEventIndex ) )
            {
                /* The specified target object is already registered with this
                 * event poll object, so can't be added again. */
                xResult = errILWALID_EVENT_POLL_OPERATION;

                /* MCDC Test Point: STD_IF "xEventPollAddObjectEvents" */
            }
            else
            {
                /* If there are no free elements in the array of object-events
                 * belonging to this event poll object, uxObjectEventIndex will
                 * have been set to eventpollILWALID_OBJECT_EVENT_INDEX. */
                if( ( portUnsignedBaseType ) eventpollILWALID_OBJECT_EVENT_INDEX != uxObjectEventIndex )
                {
                    /* Register the event poll object with the specified target
                     * object. */
                    pxObjectEvent = &( pxEventPollObject->paxRegisteredObjectEvents[ uxObjectEventIndex ] );
                    xResult = prvRegisterEventPollObject( pvTargetObjectHandle,
                                                          uxEvents,
                                                          &( pxObjectEvent->xObjectEventListItem ) );
                    if( pdPASS == xResult )
                    {
                        /* Increment the number of registered object-events. */
                        pxEventPollObject->uxNumberOfRegisteredObjectEvents++;

                        /* Register this object handle and associated events
                         * with the event poll object. */
                        pxObjectEvent->xObjectEvent.pvObjectHandle = pvTargetObjectHandle;
                        pxObjectEvent->xObjectEvent.uxEvents = uxEvents;

                        if( eventpollTASK_NOTIFICATION_RECEIVED == uxEvents )
                        {
                            /* In the case of task notification events, store
                             * the object-event index in the event poll object
                             * list item in order to make updating/removing the
                             * object-event easier. */
                            listSET_LIST_ITEM_VALUE( &( pxEventPollObject->xEventPollListItem ),
                                                     ( portTickType ) uxObjectEventIndex );

                            /* MCDC Test Point: STD_IF "xEventPollAddObjectEvents" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xEventPollAddObjectEvents" */

                        /* MCDC Test Point: STD_IF "xEventPollAddObjectEvents" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xEventPollAddObjectEvents" */

                    /* MCDC Test Point: STD_IF "xEventPollAddObjectEvents" */
                }
                else
                {
                    /* The event poll object cannot accept any more
                     * object-events. */
                    xResult = errEVENT_POLL_OBJECT_EVENTS_LIMIT_REACHED;

                    /* MCDC Test Point: STD_ELSE "xEventPollAddObjectEvents" */
                }

                /* MCDC Test Point: STD_ELSE "xEventPollAddObjectEvents" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_IF "xEventPollAddObjectEvents" */
    }
    /* MCDC Test Point: ADD_ELSE "xEventPollAddObjectEvents" */

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventPollModifyObjectEvents( eventPollHandleType xEventPollHandle,
                                                           const void *pvTargetObjectHandle,
                                                           portUnsignedBaseType uxEvents )
{
    portBaseType xResult;
    eventPollType *pxEventPollObject;
    portUnsignedBaseType uxObjectEventIndex = 0U;

    /* Obtain access to the event poll object. */
    pxEventPollObject = ( eventPollType * ) xEventPollHandle;

    /* Check the input parameters. */
    xResult = prvCheckEventPollConfigureParameters( pxEventPollObject,
                                                    pvTargetObjectHandle,
                                                    uxEvents );
    if( pdPASS == xResult )
    {
        /* Is the specified object handle already registered with this event
         * poll object? */
        if( pdPASS == prvGetObjectEventIndex( pxEventPollObject,
                                              pvTargetObjectHandle,
                                              &uxObjectEventIndex ) )
        {
            /* Update the events associated with the specified object handle. */
            pxEventPollObject->paxRegisteredObjectEvents[ uxObjectEventIndex ].xObjectEvent.uxEvents = uxEvents;

            /* MCDC Test Point: STD_IF "xEventPollModifyObjectEvents" */
        }
        else
        {
            /* The specified object handle is not already registered with this
             * event poll object, therefore this call to
             * xEventPollModifyObjectEvents() is invalid. */
            xResult = errILWALID_EVENT_POLL_OPERATION;

            /* MCDC Test Point: STD_ELSE "xEventPollModifyObjectEvents" */
        }

        /* MCDC Test Point: STD_IF "xEventPollModifyObjectEvents" */
    }
    /* MCDC Test Point: ADD_ELSE "xEventPollModifyObjectEvents" */

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventPollRemoveObjectEvents( eventPollHandleType xEventPollHandle,
                                                           const void *pvTargetObjectHandle )
{
    portBaseType xResult;
    eventPollType *pxEventPollObject;
    portUnsignedBaseType uxObjectEventIndex = 0U;
    eventPollObjectEventControlType *pxObjectEvent;

    /* Obtain access to the event poll object. */
    pxEventPollObject = ( eventPollType * ) xEventPollHandle;

    /* Check the input parameters. */
    xResult = prvCheckEventPollConfigureParameters( pxEventPollObject,
                                                    pvTargetObjectHandle,
                                                    ~eventpollNO_EVENTS );
    if( pdPASS == xResult )
    {
        /* Is the specified object handle already registered with this event
         * poll object? */
        if( pdPASS == prvGetObjectEventIndex( pxEventPollObject,
                                              pvTargetObjectHandle,
                                              &uxObjectEventIndex ) )
        {
            portENTER_CRITICAL_WITHIN_API();
            {
                /* Locate the object-event element that is being removed. */
                pxObjectEvent = &( pxEventPollObject->paxRegisteredObjectEvents[ uxObjectEventIndex ] );

                /* Is the target object the task that owns the event poll
                 * object? If it is, then the object-event is a task
                 * notification and needs to be handled differently. */
                if( pvTargetObjectHandle == pxEventPollObject->pxOwnerTCB )
                {
                    /* Reset the xItemValue member of the event poll list item
                     * to indicate that the event poll object is no longer
                     * interested in task notification events. */
                    listSET_LIST_ITEM_VALUE( &( pxEventPollObject->xEventPollListItem ),
                                             eventpollILWALID_OBJECT_EVENT_INDEX );

                    /* MCDC Test Point: STD_IF "xEventPollRemoveObjectEvents" */
                }
                else
                {
                    /* Remove the object event list item from the target
                     * object's event list. */
                    vListRemove( &( pxObjectEvent->xObjectEventListItem ) );

                    /* MCDC Test Point: STD_ELSE "xEventPollRemoveObjectEvents" */
                }

                /* Free the element in the object-events array for this object. */
                prvFreeObjectEvent( pxEventPollObject, pxObjectEvent );
            }
            portEXIT_CRITICAL_WITHIN_API();

            /* MCDC Test Point: STD_IF "xEventPollRemoveObjectEvents" */
        }
        else
        {
            /* The specified object handle is not already registered with this
             * event poll object, therefore this call to
             * xEventPollRemoveObjectEvents() is invalid. */
            xResult = errILWALID_EVENT_POLL_OPERATION;

            /* MCDC Test Point: STD_ELSE "xEventPollRemoveObjectEvents" */
        }

        /* MCDC Test Point: STD_IF "xEventPollRemoveObjectEvents" */
    }
    /* MCDC Test Point: ADD_ELSE "xEventPollRemoveObjectEvents" */

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventPollWait( eventPollHandleType xEventPollHandle,
                                             eventPollObjectEventsType axObjectEvents[],
                                             portUnsignedBaseType uxObjectEventsArraySize,
                                             portUnsignedBaseType *puxNumberOfObjectEvents,
                                             portTickType xTicksToWait )
{
    portBaseType xResult;
    eventPollType *pxEventPollObject;
    xTimeOutType xTimeOut;

    /* Has the scheduler started? */
    if( pdFALSE == xTaskIsSchedulerStarted() )
    {
        /* An event poll object is tied to the pxLwrrentTCB, therefore it cannot
         * be configured until the scheduler is running. */
        xResult = errSCHEDULER_IS_NOT_RUNNING;

        /* MCDC Test Point: STD_IF "xEventPollWait" */
    }
    else if( pdFALSE != xTaskIsSchedulerSuspended() )
    {
        /* Calls to xEventPollWait() while the scheduler is suspended are not
         * permitted. */
        xResult = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_ELSE_IF "xEventPollWait" */
    }
    else if( NULL == xEventPollHandle )
    {
        /* A valid pointer to an event poll handle must be supplied. */
        xResult = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xEventPollWait" */
    }
    else if( NULL == axObjectEvents )
    {
        /* A valid array for storing object-events must be supplied. */
        xResult = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xEventPollWait" */
    }
    else if( 0U == uxObjectEventsArraySize )
    {
        /* The length of the array for storing object-events must be non-zero. */
        xResult = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "xEventPollWait" */
    }
    else if( NULL == puxNumberOfObjectEvents )
    {
        /* A valid pointer must be supplied. */
        xResult = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xEventPollWait" */
    }
    else
    {
        /* Obtain access to the event poll object. */
        pxEventPollObject = ( eventPollType * ) xEventPollHandle;

        /* Is this a valid event poll object? */
        if( ( portUnsignedBaseType )( pxEventPollObject->pxOwnerTCB ) != ~( pxEventPollObject->uxOwnerTCBMirror ) )
        {
            /* A valid event poll handle must be supplied. */
            xResult = errILWALID_EVENT_POLL_HANDLE;

            /* MCDC Test Point: STD_IF "xEventPollWait" */
        }
        /* Is this event poll object owned by the calling task? */
        else if( pxLwrrentTCB != pxEventPollObject->pxOwnerTCB )
        {
            /* A valid event poll handle must be supplied. */
            xResult = errILWALID_EVENT_POLL_HANDLE;

            /* MCDC Test Point: STD_ELSE_IF "xEventPollWait" */
        }
        /* Are there any object-events registered with this event poll object? */
        else if( ( portUnsignedBaseType ) 0U == pxEventPollObject->uxNumberOfRegisteredObjectEvents )
        {
            /* At least one object-event must be registered before calling
             * xEventPollWait(). */
            xResult = errILWALID_EVENT_POLL_OPERATION;

            /* MCDC Test Point: STD_ELSE_IF "xEventPollWait" */
        }
        else
        {
            /* Enter a critical section to ensure that:
             * 1. No ticks occur during the call to vTaskSetTimeOut().
             * 2. The array of object-events is not modified while it is being
             *    accessed in this function.
             * 3. No ISR or context switch during call to
             *    vTaskAddLwrrentTaskToDelayedList(). */
            portENTER_CRITICAL_WITHIN_API();
            {
                /* Record the time on entry to xEventPollWait() so that a
                 * timeout can be checked for later. */
                vTaskSetTimeOut( &xTimeOut );

                do
                {
                    /* Have any object-events oclwrred? */
                    xResult = prvGetOclwrredObjectEvents( pxEventPollObject,
                                                          axObjectEvents,
                                                          uxObjectEventsArraySize,
                                                          puxNumberOfObjectEvents );

                    /* If object-events have oclwrred or there was an error
                     * copying them to the application supplied array, there is
                     * nothing else to do before returning. */
                    if( errEVENT_POLL_NO_EVENTS_OCLWRRED == xResult )
                    {
                        /* Need to call xTaskCheckForTimeout() as time could
                         * have passed since it was last called (if this is
                         * not the first time around this loop).
                         * NOTE: If no block time has been supplied, this call
                         * to xTaskCheckForTimeout() will return pdTRUE. */
                        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                        {
                            /* Add to the current task to the delayed list -
                             * this call MUST either be made with the
                             * scheduler suspended or from within a critical
                             * section. */
                            vTaskAddLwrrentTaskToDelayedList( xTicksToWait );
                            pxEventPollObject->xTaskIsBlocked = pdTRUE;

                            /* Force a reschedule. */
                            taskYIELD_WITHIN_API();

                            /* Higher priority tasks and interrupts could have
                             * exelwted between this task being unblocked and
                             * entering the Running state, therefore the event
                             * that caused this task to unblock could possibly
                             * have been negated when this line is exelwted. */

                            /* Ensure the 'task is blocked' flag is cleared in
                             * case the unblock was due to a timeout. */
                            pxEventPollObject->xTaskIsBlocked = pdFALSE;

                            /* Check again if any events have oclwrred. */
                            xResult = errERRONEOUS_UNBLOCK;

                            /* MCDC Test Point: STD_IF "xEventPollWait" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xEventPollWait" */

                        /* MCDC Test Point: STD_IF "xEventPollWait" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xEventPollWait" */

                    /* MCDC Test Point: WHILE_INTERNAL "xEventPollWait" "( errERRONEOUS_UNBLOCK == xResult )" */
                } while( errERRONEOUS_UNBLOCK == xResult );
            }
            portEXIT_CRITICAL_WITHIN_API();

            /* MCDC Test Point: STD_ELSE "xEventPollWait" */
        }

        /* MCDC Test Point: STD_ELSE "xEventPollWait" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 * Private API Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vEventPollInitQueueEventPollObjects( xQUEUE *const pxQueue )
{
    vListInitialise( &( pxQueue->xRegisteredEventPollObjects ) );

    /* MCDC Test Point: STD "vEventPollInitQueueEventPollObjects" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vEventPollInitTaskEventPollObjects( xTCB *const pxTCB )
{
    vListInitialise( &( pxTCB->xEventPollObjectsList ) );

    /* MCDC Test Point: STD "vEventPollInitTaskEventPollObjects" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vEventPollInitEventGroupEventPollObjects( eventGroupType *pxEventGroup )
{
    vListInitialise( &( pxEventGroup->xRegisteredEventPollObjects ) );

    /* MCDC Test Point: STD "vEventPollInitEventGroupEventPollObjects" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventPollUpdateTaskEventPollObjects( const xTCB *pxTCB, portUnsignedBaseType uxLwrrentStatus )
{
    portBaseType xContextSwitchRequired = pdFALSE;

    /* Does this task own any event poll objects? */
    if( listLWRRENT_LIST_LENGTH( &( pxTCB->xEventPollObjectsList ) ) > 0U )
    {
        xContextSwitchRequired = prvUpdateObjectList( &( pxTCB->xEventPollObjectsList ), uxLwrrentStatus );

        /* MCDC Test Point: STD_IF "xEventPollUpdateTaskEventPollObjects" */
    }
    else
    {
        xContextSwitchRequired = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xEventPollUpdateTaskEventPollObjects" */
    }

    return xContextSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventPollUpdateQueueEventPollObjects( const xQUEUE *const pxQueue )
{
    portBaseType xContextSwitchRequired;
    portUnsignedBaseType uxLwrrentStatus;

    /* Are there any event poll objects registered with this queue? */
    if( listLWRRENT_LIST_LENGTH( &( pxQueue->xRegisteredEventPollObjects ) ) > 0U )
    {
        uxLwrrentStatus = prvGetEventPollQueueStatus( pxQueue );

        xContextSwitchRequired = prvUpdateObjectList( &( pxQueue->xRegisteredEventPollObjects ), uxLwrrentStatus );

        /* MCDC Test Point: STD_IF "xEventPollUpdateQueueEventPollObjects" */
    }
    else
    {
        xContextSwitchRequired = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xEventPollUpdateQueueEventPollObjects" */
    }

    return xContextSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventPollUpdateEventGroupEventPollObjects( const eventGroupType *pxEventGroup )
{
    portBaseType xContextSwitchRequired;
    portUnsignedBaseType uxLwrrentEvents;

    portENTER_CRITICAL_WITHIN_API();
    {
        /* Are there any event poll objects registered with this event group? */
        if( listLWRRENT_LIST_LENGTH( &( pxEventGroup->xRegisteredEventPollObjects ) ) > 0U )
        {
            uxLwrrentEvents = prvAreAnyEventGroupBitsSet( pxEventGroup );

            xContextSwitchRequired = prvUpdateObjectList( &( pxEventGroup->xRegisteredEventPollObjects ), uxLwrrentEvents);

            /* MCDC Test Point: STD_IF "xEventPollUpdateEventGroupEventPollObjects" */
        }
        else
        {
            xContextSwitchRequired = pdFALSE;

            /* MCDC Test Point: STD_ELSE "xEventPollUpdateEventGroupEventPollObjects" */
        }
    }
    portEXIT_CRITICAL_WITHIN_API();

    return xContextSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_DELETE_FUNCTION portBaseType xEventPollRemoveEventGroupEventPollObjects( const eventGroupType *pxEventGroup )
{
    portBaseType xContextSwitchRequired = pdFALSE;
    const xMiniListItem *pxEventPollObjectListEnd;
    xListItem *pxEventPollObjectListItem;

    portENTER_CRITICAL_WITHIN_API();
    {
        /* Are there any event poll objects registered with this event group? */
        if( listLWRRENT_LIST_LENGTH( &( pxEventGroup->xRegisteredEventPollObjects ) ) > 0U )
        {
            /* Obtain the end of list marker for the list of registered event
             * poll objects. */
            pxEventPollObjectListEnd = listGET_END_MARKER( &( pxEventGroup->xRegisteredEventPollObjects ) );

            /* Obtain the head entry in the list of registered event poll
             * objects. */
            pxEventPollObjectListItem = listGET_HEAD_ENTRY( &( pxEventGroup->xRegisteredEventPollObjects ) );

            /* Loop through the registered event poll objects (this function has
             * already determined that there is at least one), removing the
             * association with this event group. */
            do
            {
                /* Remove the association with this event group from the event
                 * poll object-event (this will also remove this object-event
                 * list item from the list of registered event poll objects. */
                if( pdTRUE == prvEventGroupDeleted( pxEventPollObjectListItem ) )
                {
                    /* A higher priority task has been unblocked. */
                    xContextSwitchRequired = pdTRUE;

                    /* MCDC Test Point: STD_IF "xEventPollRemoveEventGroupEventPollObjects" */
                }
                /* MCDC Test Point: ADD_ELSE "xEventPollRemoveEventGroupEventPollObjects" */

                /* Obtain the new head entry in the list of registered event
                 * poll objects. */
                pxEventPollObjectListItem = listGET_HEAD_ENTRY( &( pxEventGroup->xRegisteredEventPollObjects ) );

                /* MCDC Test Point: WHILE_INTERNAL "xEventPollRemoveEventGroupEventPollObjects" "( ( const xMiniListItem * ) pxEventPollObjectListItem != pxEventPollObjectListEnd )" */
            } while( ( const xMiniListItem * ) pxEventPollObjectListItem != pxEventPollObjectListEnd );

            /* MCDC Test Point: STD_IF "xEventPollRemoveEventGroupEventPollObjects" */
        }
        /* MCDC Test Point: ADD_ELSE "xEventPollRemoveEventGroupEventPollObjects" */
    }
    portEXIT_CRITICAL_WITHIN_API();

    return xContextSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_DELETE_FUNCTION void vEventPollTaskDeleted( xTCB *pxTCB )
{
    const xMiniListItem *pxOwnedEventPollObjectListEnd;
    xListItem *pxOwnedEventPollObjectListItem;
    eventPollType *pxEventPollObject;
    portUnsignedBaseType uxIndex;
    eventPollObjectEventControlType *pxObjectEventControl;

    const xList *pxEventPollObjectsList = &( pxTCB->xEventPollObjectsList );

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */


    /* Obtain the end of list marker for the list of event poll objects owned by
     * the task that is being deleted. */
    pxOwnedEventPollObjectListEnd = listGET_END_MARKER( pxEventPollObjectsList );

    /* Obtain the head entry in the list of event poll objects owned by the task
     * that is being deleted. */
    pxOwnedEventPollObjectListItem = listGET_HEAD_ENTRY( pxEventPollObjectsList );

    /* Loop through the owned event poll objects. */
    /* MCDC Test Point: WHILE_EXTERNAL "vEventPollTaskDeleted" "( ( const xMiniListItem * ) pxOwnedEventPollObjectListItem != pxOwnedEventPollObjectListEnd )" */
    while( ( const xMiniListItem * ) pxOwnedEventPollObjectListItem != pxOwnedEventPollObjectListEnd )
    {
        pxEventPollObject = ( eventPollType * ) listGET_LIST_ITEM_OWNER( pxOwnedEventPollObjectListItem );

        /* Loop through all the object-events associated with this event poll
         * object, removing the registration with any target objects. */
        for( uxIndex = 0U; uxIndex < pxEventPollObject->uxMaximumRegisteredObjectEvents; uxIndex++ )
        {
            /* Locate the next object-event element. */
            pxObjectEventControl = &( pxEventPollObject->paxRegisteredObjectEvents[ uxIndex ] );

            /* Is this object-event element associated with a target object? */
            if( pxObjectEventControl->xObjectEvent.pvObjectHandle != NULL )
            {
                /* Is the target object the task that owns the event poll
                 * object? If it is, then the object-event is a task
                 * notification and needs to be handled differently. */
                if( ( portUnsignedBaseType ) listGET_LIST_ITEM_VALUE( &( pxEventPollObject->xEventPollListItem ) ) == uxIndex )
                {
                    /* Reset the xItemValue member of the event poll list item
                     * to indicate that the event poll object is no longer
                     * interested in task notification events. */
                    listSET_LIST_ITEM_VALUE( &( pxEventPollObject->xEventPollListItem ),
                                             eventpollILWALID_OBJECT_EVENT_INDEX );

                    /* MCDC Test Point: STD_IF "vEventPollTaskDeleted" */
                }
                else
                {
                    /* Remove the object event list item from the target
                     * object's event list. */
                    vListRemove( &( pxObjectEventControl->xObjectEventListItem ) );

                    /* MCDC Test Point: STD_ELSE "vEventPollTaskDeleted" */
                }

                /* Free the element in the object-events array for this
                 * object. */
                prvFreeObjectEvent( pxEventPollObject, pxObjectEventControl );

                /* MCDC Test Point: STD_IF "vEventPollTaskDeleted" */
            }
            /* MCDC Test Point: ADD_ELSE "vEventPollTaskDeleted" */

            /* MCDC Test Point: STD "vEventPollTaskDeleted" */
        }

        /* Remove the event poll object list item from the task's list of event
         * poll objects it owns. */
        vListRemove( &( pxEventPollObject->xEventPollListItem ) );

        /* Break the link between this event poll object and the task that owns
         * it. */
        pxEventPollObject->pxOwnerTCB = NULL;

        /* Set the uxOwnerTCBMirror member so that the event poll object is
         * invalid. */
        pxEventPollObject->uxOwnerTCBMirror = 0U;

        /* Get the new head entry in the list of event poll objects owned by the
         * task that is being deleted. */
        pxOwnedEventPollObjectListItem = listGET_HEAD_ENTRY( pxEventPollObjectsList );

        /* MCDC Test Point: WHILE_INTERNAL "vEventPollTaskDeleted" "( ( const xMiniListItem * ) pxOwnedEventPollObjectListItem != pxOwnedEventPollObjectListEnd )" */
    }
}
/*---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Private Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION static portBaseType prvCheckEventPollCreateParameters( portInt8Type *pcEventPollMemoryBuffer,
                                                                              portUnsignedBaseType uxBufferLengthInBytes,
                                                                              portUnsignedBaseType uxMaximumRegisteredObjectEvents,
                                                                              portTaskHandleType xOwnerTaskHandle,
                                                                              const eventPollHandleType *pxEventPollHandle )
{
    portBaseType xResult = pdPASS;

    /* Check the input parameters. */
    if( NULL == pcEventPollMemoryBuffer )
    {
        /* A valid pointer to a memory buffer must be supplied. */
        xResult = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "prvCheckEventPollCreateParameters" */
    }
    else if( 0U != ( ( ( portUnsignedBaseType ) pcEventPollMemoryBuffer ) & portWORD_ALIGNMENT_MASK ) )
    {
        /* The buffer must be aligned correctly as it will be cast to an
         * eventPollType structure. */
        xResult = errILWALID_ALIGNMENT;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollCreateParameters" */
    }
    else if( 0U == uxMaximumRegisteredObjectEvents )
    {
        /* The buffer must have space for at least one object-event control
         * structure. */
        xResult = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollCreateParameters" */
    }
    else if( ( ( portUnsignedBaseType ) sizeof( eventPollType ) + ( ( portUnsignedBaseType ) sizeof( eventPollObjectEventControlType ) * uxMaximumRegisteredObjectEvents ) ) != uxBufferLengthInBytes )
    {
        /* The buffer must be sized correctly. */
        xResult = errILWALID_BUFFER_SIZE;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollCreateParameters" */
    }
    else if( NULL == pxEventPollHandle )
    {
        /* A valid pointer to an event poll handle must be supplied. */
        xResult = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollCreateParameters" */
    }
    else
    {
        /* Has the scheduler started? */
        if( pdFALSE == xTaskIsSchedulerStarted() )
        {
            /* The scheduler has not yet started, so check that a NULL pointer
             * has not been supplied. */
            if( NULL == xOwnerTaskHandle )
            {
                xResult = errNULL_PARAMETER_SUPPLIED;

                /* MCDC Test Point: STD_IF "prvCheckEventPollCreateParameters" */
            }
            /* MCDC Test Point: ADD_ELSE "prvCheckEventPollCreateParameters" */

            /* MCDC Test Point: STD_IF "prvCheckEventPollCreateParameters" */
        }
        else
        {
            /* The scheduler has been started, therefore the only valid values
             * for xOwnerTaskHandle are NULL (taken to mean the current task)
             * and the handle of the current task. */
            if( NULL != xOwnerTaskHandle )
            {
                if( pxLwrrentTCB != ( xTCB * ) xOwnerTaskHandle )
                {
                    /* Report the error. */
                    xResult = errILWALID_TASK_HANDLE;

                    /* MCDC Test Point: STD_IF "prvCheckEventPollCreateParameters" */
                }
                /* MCDC Test Point: ADD_ELSE "prvCheckEventPollCreateParameters" */

                /* MCDC Test Point: STD_IF "prvCheckEventPollCreateParameters" */
            }
            /* MCDC Test Point: ADD_ELSE "prvCheckEventPollCreateParameters" */

            /* MCDC Test Point: STD_ELSE "prvCheckEventPollCreateParameters" */
        }

        /* MCDC Test Point: STD_ELSE "prvCheckEventPollCreateParameters" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvCheckEventPollConfigureParameters( const eventPollType *pxEventPollObject,
                                                                          const void *pvTargetObjectHandle,
                                                                          portUnsignedBaseType uxEvents )
{
    portBaseType xResult;

    /* Has the scheduler started? */
    if( pdFALSE == xTaskIsSchedulerStarted() )
    {
        /* An event poll object is tied to the pxLwrrentTCB, therefore it cannot
         * be configured until the scheduler is running. */
        xResult = errSCHEDULER_IS_NOT_RUNNING;

        /* MCDC Test Point: STD_IF "prvCheckEventPollConfigureParameters" */
    }
    else if( NULL == pxEventPollObject )
    {
        /* A valid pointer to an event poll object must be supplied. */
        xResult = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollConfigureParameters" */
    }
    else if( NULL == pvTargetObjectHandle )
    {
        /* A valid object handle must be supplied. */
        xResult = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollConfigureParameters" */
    }
    else if( eventpollNO_EVENTS == uxEvents )
    {
        /* At least one event should be specified. */
        xResult = errILWALID_EVENT_POLL_EVENTS;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollConfigureParameters" */
    }
    /* Is this a valid event poll object? */
    else if( ( portUnsignedBaseType )( pxEventPollObject->pxOwnerTCB ) != ~( pxEventPollObject->uxOwnerTCBMirror ) )
    {
        /* A pointer to a valid event poll object must be supplied. */
        xResult = errILWALID_EVENT_POLL_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollConfigureParameters" */
    }
    /* Is this event poll object owned by the calling task? */
    else if( pxLwrrentTCB != pxEventPollObject->pxOwnerTCB )
    {
        /* The calling task must own this event poll object. */
        xResult = errILWALID_EVENT_POLL_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "prvCheckEventPollConfigureParameters" */
    }
    else
    {
        /* The configuration parameters are valid. */
        xResult = pdPASS;

        /* MCDC Test Point: STD_ELSE "prvCheckEventPollConfigureParameters" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvGetObjectEventIndex( const eventPollType *pxEventPollObject,
                                                            const void *pvTargetObjectHandle,
                                                            portUnsignedBaseType *puxObjectEventIndex )
{
    portBaseType xResult = pdFAIL;
    portUnsignedBaseType uxIndex;
    portUnsignedBaseType uxNumberOfObjectEvents = pxEventPollObject->uxMaximumRegisteredObjectEvents;
    eventPollObjectEventControlType *pxObjectEvent;

    /* Set the return index to the default value. */
    *puxObjectEventIndex = ( portUnsignedBaseType ) eventpollILWALID_OBJECT_EVENT_INDEX;

    /* Search through the array of object-events belonging to the event poll
     * object in order to determine the index of the object-events structure
     * that relates to the specified object handle. */
    for( uxIndex = 0U; uxIndex < uxNumberOfObjectEvents; uxIndex++ )
    {
        /* Does this element relate to the specified object handle? */
        pxObjectEvent = &( pxEventPollObject->paxRegisteredObjectEvents[ uxIndex ] );
        if( pvTargetObjectHandle == pxObjectEvent->xObjectEvent.pvObjectHandle )
        {
            /* Return the index of this element. */
            *puxObjectEventIndex = uxIndex;
            xResult = pdPASS;

            /* MCDC Test Point: STD_IF "prvGetObjectEventIndex" */

            /* Exit the loop now that the required element has been
             * identified. */
            break;
        }
        /* Has the first unused element been located? */
        else if( ( portUnsignedBaseType ) eventpollILWALID_OBJECT_EVENT_INDEX == *puxObjectEventIndex )
        {
            /* Is this element lwrrently unused? */
            if( NULL == pxObjectEvent->xObjectEvent.pvObjectHandle )
            {
                /* Set the index of the first unused element. */
                *puxObjectEventIndex = uxIndex;

                /* MCDC Test Point: STD_IF "prvGetObjectEventIndex" */
            }
            /* MCDC Test Point: ADD_ELSE "prvGetObjectEventIndex" */

            /* MCDC Test Point: STD_ELSE_IF "prvGetObjectEventIndex" */
        }
        else
        {
            /* This clause is deliberately left empty. */

            /* MCDC Test Point: STD_ELSE "prvGetObjectEventIndex" */
        }

        /* MCDC Test Point: STD "prvGetObjectEventIndex" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvUpdateObjectList( const xList *pxRegisteredEventPollObjects, portUnsignedBaseType uxUpdatedStatus )
{
    portBaseType xContextSwitchRequired = pdFALSE;
    const xMiniListItem *pxEventPollObjectListEnd;
    xListItem *pxEventPollObjectListItem;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Obtain the end of list marker for the list of registered event
     * poll objects. */
    pxEventPollObjectListEnd = listGET_END_MARKER( pxRegisteredEventPollObjects );

    /* Obtain the head entry in the list of registered event poll
     * objects. */
    pxEventPollObjectListItem = listGET_HEAD_ENTRY( pxRegisteredEventPollObjects );

    /* Loop through the registered event poll objects (the calling function has
     * already determined that there is at least one), updating the
     * status of the objects. */
    do
    {
        /* Update the status of the event poll object-event. */
        if( pdTRUE == prvUpdateTargetObjectStatus( pxEventPollObjectListItem,
                                                   uxUpdatedStatus ) )
        {
            /* A higher priority task has been unblocked. */
            xContextSwitchRequired = pdTRUE;

            /* MCDC Test Point: STD_IF "prvUpdateObjectList" */
        }
        /* MCDC Test Point: ADD_ELSE "prvUpdateObjectList" */

        /* Get the next event poll object list item. */
        pxEventPollObjectListItem = listGET_NEXT( pxEventPollObjectListItem );

        /* MCDC Test Point: WHILE_INTERNAL "prvUpdateObjectList" "( ( const xMiniListItem * ) pxEventPollObjectListItem != pxEventPollObjectListEnd )" */
    } while( ( const xMiniListItem * ) pxEventPollObjectListItem != pxEventPollObjectListEnd );

    return xContextSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvUpdateTargetObjectStatus( const xListItem *pxObjectEventListItem,
                                                                 portUnsignedBaseType uxLwrrentEvents )
{
    eventPollType *pxEventPollObject;
    portUnsignedBaseType uxObjectEventIndex;
    portBaseType xSwitchRequired = pdFALSE;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Obtain the handle of the event poll object from the object-event list
     * item. */
    pxEventPollObject = ( eventPollType * ) listGET_LIST_ITEM_OWNER( pxObjectEventListItem );

    /* Obtain the index into the object-events array to update from the
     * object-event list item. */
    uxObjectEventIndex = ( portUnsignedBaseType ) listGET_LIST_ITEM_VALUE( pxObjectEventListItem );

    /* If this call to prvUpdateTargetObjectStatus() has been made due to
     * a task notification but the event poll object isn't interested in the
     * notification, then the uxObjectEventIndex will not be valid. */
    if( ( portUnsignedBaseType ) eventpollILWALID_OBJECT_EVENT_INDEX != uxObjectEventIndex )
    {
        /* Update the event poll object. */
        pxEventPollObject->paxRegisteredObjectEvents[ uxObjectEventIndex ].uxLwrrentEvents = uxLwrrentEvents;

        /* If the task that owns the event poll object is lwrrently blocked
         * waiting for object-events, unblock it now. */
        if( pdTRUE == pxEventPollObject->xTaskIsBlocked )
        {
            if( pdFALSE == xTaskIsTaskSuspended( pxEventPollObject->pxOwnerTCB ) )
            {
                /* Unblock the task and return it to the appropriate ready list. */
                xSwitchRequired = xTaskMoveTaskToReadyList( pxEventPollObject->pxOwnerTCB );

                /* Clear the 'task is blocked' flag. */
                pxEventPollObject->xTaskIsBlocked = pdFALSE;

                /* MCDC Test Point: STD_IF "prvUpdateTargetObjectStatus" */
            }
            /* MCDC Test Point: ADD_ELSE "prvUpdateTargetObjectStatus" */

            /* MCDC Test Point: STD_IF "prvUpdateTargetObjectStatus" */
        }
        /* MCDC Test Point: ADD_ELSE "prvUpdateTargetObjectStatus" */

        /* MCDC Test Point: STD_IF "prvUpdateTargetObjectStatus" */
    }
    /* MCDC Test Point: ADD_ELSE "prvUpdateTargetObjectStatus" */

    return xSwitchRequired;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvRegisterEventPollObject( void *pvTargetObjectHandle,
                                                                portUnsignedBaseType uxEvents,
                                                                xListItem *pxEventPollObjectEventListItem )
{
    portBaseType xResult;
    portUnsignedBaseType uxLwrrentEvents = eventpollNO_EVENTS;

    /* Do the supplied events relate to a queue? */
    if( ( uxEvents & eventpollALL_QUEUE_EVENTS ) != eventpollNO_EVENTS )
    {
        /* Check that no spurious events have been requested. */
        if( eventpollNO_EVENTS == ( uxEvents & ~eventpollALL_QUEUE_EVENTS ) )
        {
            /* Register the event poll object with the specified queue. */
            xResult = prvQueueAddEventPollListItem( pvTargetObjectHandle,
                                                    pxEventPollObjectEventListItem );

            /* MCDC Test Point: STD_IF "prvRegisterEventPollObject" */
        }
        else
        {
            /* An invalid combination of events have been specified. */
            xResult = errILWALID_EVENT_POLL_EVENTS;

            /* MCDC Test Point: STD_ELSE "prvRegisterEventPollObject" */
        }

        /* MCDC Test Point: STD_IF "prvRegisterEventPollObject" */
    }
    /* Is the event a task notification? */
    else if( eventpollTASK_NOTIFICATION_RECEIVED == uxEvents )
    {
        /* Task notifications are a special case as the task that owns the event
         * poll object keeps a list of all event poll objects it owns, not just
         * those that are interested in task notifications. */

        /* Retrieve the current notification status for the specified task. Note
         * that this includes checking that the supplied task handle is that of
         * the lwrrently running task. */
        xResult = prvQueryTaskNotificationStatus( pvTargetObjectHandle,
                                                  &uxLwrrentEvents );
        if( pdPASS == xResult )
        {
            /* Update the the current events in the specified object-event.
             * Note: As this function is being called from within
             * xEventPollAddObjectEvents(), it will not cause the current task
             * to be unblocked, so the return value can be ignored. */
            ( void ) prvUpdateTargetObjectStatus( pxEventPollObjectEventListItem,
                                                  uxLwrrentEvents );

            /* MCDC Test Point: STD_IF "prvRegisterEventPollObject" */
        }
        /* MCDC Test Point: ADD_ELSE "prvRegisterEventPollObject" */

        /* MCDC Test Point: STD_ELSE_IF "prvRegisterEventPollObject" */
    }
    /* Is the event the setting of bits in an event group? */
    else if( eventpollEVENT_GROUP_BITS_SET == uxEvents )
    {
        /* Register the event poll object with the specified event group. */
        xResult = prvEventGroupAddEventPollListItem( pvTargetObjectHandle,
                                                     pxEventPollObjectEventListItem );

        /* MCDC Test Point: STD_ELSE_IF "prvRegisterEventPollObject" */
    }
    else
    {
        /* An invalid set of events have been specified. */
        xResult = errILWALID_EVENT_POLL_EVENTS;

        /* MCDC Test Point: STD_ELSE "prvRegisterEventPollObject" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvGetOclwrredObjectEvents( eventPollType *pxEventPollObject,
                                                                eventPollObjectEventsType axObjectEvents[],
                                                                portUnsignedBaseType uxObjectEventsArraySize,
                                                                portUnsignedBaseType *puxNumberOfObjectEvents )
{
    portBaseType xResult = errEVENT_POLL_NO_EVENTS_OCLWRRED;
    portUnsignedBaseType uxEventPollObjectIndex;
    portUnsignedBaseType uxNumberOfObjectEvents = pxEventPollObject->uxMaximumRegisteredObjectEvents;
    eventPollObjectEventControlType *pxObjectEvent;
    portUnsignedBaseType uxObjectEventArrayIndex = 0U;
    eventPollObjectEventsType uxOclwrredObjectEvent;

    /* Loop through the object-events associated with the event poll object to
     * determine if any have oclwrred - if any have, copy as many as possible
     * into the supplied array. */
    for( uxEventPollObjectIndex = 0U; uxEventPollObjectIndex < uxNumberOfObjectEvents; uxEventPollObjectIndex++ )
    {
        /* Have any of the events of interest associated with this object
         * oclwrred? */
        pxObjectEvent = &( pxEventPollObject->paxRegisteredObjectEvents[ uxEventPollObjectIndex ] );
        uxOclwrredObjectEvent.uxEvents = pxObjectEvent->uxLwrrentEvents;
        uxOclwrredObjectEvent.uxEvents &= pxObjectEvent->xObjectEvent.uxEvents;

        /* A special case is if this object-event relates to an event group that
         * has been deleted. */
        if( eventpollEVENT_GROUP_DELETED == pxObjectEvent->uxLwrrentEvents )
        {
            /* Free the element in the object-events array for this object. */
            prvFreeObjectEvent( pxEventPollObject, pxObjectEvent );

            /* Report the error and exit the loop. */
            xResult = errEVENT_GROUP_DELETED;

            /* MCDC Test Point: STD_IF "prvGetOclwrredObjectEvents" */
            break;
        }
        else if( uxOclwrredObjectEvent.uxEvents != eventpollNO_EVENTS )
        {
            /* Report the events of interest to the caller. */
            uxOclwrredObjectEvent.pvObjectHandle = pxObjectEvent->xObjectEvent.pvObjectHandle;
            if( pdPASS != portCOPY_DATA_TO_TASK( &axObjectEvents[ uxObjectEventArrayIndex ],
                                                 &uxOclwrredObjectEvent,
                                                 ( portUnsignedBaseType ) sizeof( eventPollObjectEventsType ) ) )
            {
                /* Report the error and exit the loop. */
                xResult = errILWALID_DATA_RANGE;

                /* MCDC Test Point: STD_IF "prvGetOclwrredObjectEvents" */
                break;
            }
            else
            {
                /* Set return value to report success. */
                xResult = pdPASS;

                /* MCDC Test Point: STD_ELSE "prvGetOclwrredObjectEvents" */
            }

            /* Increment the index into the specified array. */
            uxObjectEventArrayIndex++;

            /* If the supplied array is now full, there is no point looking
             * for any more object-events. */
            if( uxObjectEventArrayIndex >= uxObjectEventsArraySize )
            {
                /* MCDC Test Point: STD_IF "prvGetOclwrredObjectEvents" */
                break;
            }
            /* MCDC Test Point: ADD_ELSE "prvGetOclwrredObjectEvents" */

            /* MCDC Test Point: STD_ELSE_IF "prvGetOclwrredObjectEvents" */
        }
        else
        {
            /* Continue with the next object-event. */

            /* MCDC Test Point: STD_ELSE "prvGetOclwrredObjectEvents" */
        }

        /* MCDC Test Point: STD "prvGetOclwrredObjectEvents" */
    }

    if( pdPASS == xResult )
    {
        /* Report the number of object-events that have oclwrred. */
        if( pdPASS != portCOPY_DATA_TO_TASK( puxNumberOfObjectEvents,
                                             &uxObjectEventArrayIndex,
                                             ( portUnsignedBaseType ) sizeof( portUnsignedBaseType ) ) )
        {
            /* Report the error. */
            xResult = errILWALID_DATA_RANGE;

            /* MCDC Test Point: STD_IF "prvGetOclwrredObjectEvents" */
        }
        /* MCDC Test Point: ADD_ELSE "prvGetOclwrredObjectEvents" */

        /* MCDC Test Point: STD_IF "prvGetOclwrredObjectEvents" */
    }
    /* MCDC Test Point: ADD_ELSE "prvGetOclwrredObjectEvents" */

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvQueueAddEventPollListItem( xQueueHandle xQueue,
                                                                  xListItem *pxEventPollObjectEventListItem )
{
    portBaseType xResult;
    /* Cast param 'xQueueHandle xQueue' to xQUEUE * type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;
    portUnsignedBaseType uxLwrrentEvents;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Check that a valid queue handle has been supplied. */
    if( xQueueIsQueueValid( pxQueue ) == pdFALSE )
    {
        xResult = errILWALID_QUEUE_HANDLE;

        /* MCDC Test Point: STD_IF "prvQueueAddEventPollListItem" */
    }
    else
    {
        /* A valid queue handle has been supplied. */
        xResult = pdPASS;

        /* Add the specified event poll object to the list of registered event
         * poll objects. */
        vListInsertEnd( &( pxQueue->xRegisteredEventPollObjects ),
                        pxEventPollObjectEventListItem );


        uxLwrrentEvents = prvGetEventPollQueueStatus( pxQueue );


        /* Update the the status of the queue in the event poll object.
         * Note: This function is called from within xEventPollAddObjectEvents(),
         * therefore it will not cause the owner task to be unblocked. */
        ( void ) prvUpdateTargetObjectStatus( pxEventPollObjectEventListItem,
                                              uxLwrrentEvents );

        /* MCDC Test Point: STD_ELSE "prvQueueAddEventPollListItem" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvTaskAddEventPollListItem( xTCB *pxTCB,
                                                                 xListItem *pxEventPollListItem )
{
    portBaseType xResult;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Check that the TCB is valid. */
    if( xPortIsTaskHandleValid( pxTCB ) == pdFALSE )
    {
        xResult = errILWALID_TASK_HANDLE;

        /* MCDC Test Point: STD_IF "prvTaskAddEventPollListItem" */
    }
    else
    {
        /* A valid TCB has been supplied. */
        xResult = pdPASS;

        /* Add the specified event poll list item to the list of event poll
         * objects owned by this task. */
        vListInsertEnd( &( pxTCB->xEventPollObjectsList ),
                        pxEventPollListItem );

        /* MCDC Test Point: STD_ELSE "prvTaskAddEventPollListItem" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvEventGroupAddEventPollListItem( eventGroupHandleType xEventGroupHandle,
                                                                       xListItem *pxEventPollObjectEventListItem )
{
    portBaseType xResult;
    eventGroupType *pxEventGroup;
    portUnsignedBaseType uxLwrrentEvents;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Obtain access to the Event Group structure. */
    pxEventGroup = ( eventGroupType * ) xEventGroupHandle;

    /* Check that handle of a valid event group has been supplied. */
    if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
    {
        /* A valid Event Group handle must be supplied. */
        xResult = errILWALID_EVENT_GROUP_HANDLE;

        /* MCDC Test Point: STD_IF "prvEventGroupAddEventPollListItem" */
    }
    else
    {
        /* The handle of a valid event group has been supplied. */
        xResult = pdPASS;

        /* Add the specified event poll object to the list of registered event
         * poll objects. */
        vListInsertEnd( &( pxEventGroup->xRegisteredEventPollObjects ),
                        pxEventPollObjectEventListItem );

        uxLwrrentEvents = prvAreAnyEventGroupBitsSet( pxEventGroup );

        /* Update the the status of the event group in the event poll object.
         * Note: This function is called from within xEventPollAddObjectEvents(),
         * therefore it will not cause the owner task to be unblocked. */
        ( void ) prvUpdateTargetObjectStatus( pxEventPollObjectEventListItem,
                                              uxLwrrentEvents );

        /* MCDC Test Point: STD_ELSE "prvEventGroupAddEventPollListItem" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static void prvFreeObjectEvent( eventPollType *pxEventPollObject,
                                                eventPollObjectEventControlType *pxObjectEventControl )
{
    /* Free the element in the object-events array for this object. */
    pxObjectEventControl->xObjectEvent.pvObjectHandle = NULL;
    pxObjectEventControl->xObjectEvent.uxEvents = eventpollNO_EVENTS;
    pxObjectEventControl->uxLwrrentEvents = eventpollNO_EVENTS;

    /* Decrement the number of registered object-events. */
    pxEventPollObject->uxNumberOfRegisteredObjectEvents--;

    /* MCDC Test Point: STD "prvFreeObjectEvent" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvQueryTaskNotificationStatus( portTaskHandleType xTask,
                                                                    portUnsignedBaseType *puxLwrrentEvents )
{
    portBaseType xResult;
    xTCB *pxTCB;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Retrieve the TCB from the task handle - this is guaranteed to be
     * non-NULL. */
    pxTCB = taskGET_TCB_FROM_HANDLE( xTask );

    /* Check that this is the TCB of the current task. */
    if( pxTCB != pxLwrrentTCB )
    {
        xResult = errILWALID_TASK_HANDLE;

        /* MCDC Test Point: STD_IF "prvQueryTaskNotificationStatus" */
    }
    else
    {
        /* This is the TCB of the current task. */
        xResult = pdPASS;

        /* Is there a task notification available? */
        if( taskNOTIFICATION_NOTIFIED == pxTCB->xNotifyState )
        {
            *puxLwrrentEvents = eventpollTASK_NOTIFICATION_RECEIVED;

            /* MCDC Test Point: STD_IF "prvQueryTaskNotificationStatus" */
        }
        else
        {
            *puxLwrrentEvents = eventpollNO_EVENTS;

            /* MCDC Test Point: STD_ELSE "prvQueryTaskNotificationStatus" */
        }

        /* MCDC Test Point: STD_ELSE "prvQueryTaskNotificationStatus" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portUnsignedBaseType prvGetEventPollQueueStatus( const xQUEUE *const pxQueue )
{
    portUnsignedBaseType uxStatus = 0U;

    /* Is there a message available to receive? */
    if( pxQueue->uxItemsWaiting > 0U )
    {
        uxStatus |= eventpollQUEUE_MESSAGE_WAITING;

        /* MCDC Test Point: STD_IF "prvGetEventPollQueueStatus" */
    }
    /* MCDC Test Point: ADD_ELSE "prvGetEventPollQueueStatus" */

    /* Is there space in the queue to send another message? */
    if( pxQueue->uxItemsWaiting < pxQueue->uxMaxNumberOfItems )
    {
        uxStatus |= eventpollQUEUE_SPACE_AVAILABLE;

        /* MCDC Test Point: STD_IF "prvGetEventPollQueueStatus" */
    }
    /* MCDC Test Point: ADD_ELSE "prvGetEventPollQueueStatus" */

    return uxStatus;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portUnsignedBaseType prvAreAnyEventGroupBitsSet( const eventGroupType *pxEventGroup )
{
    portUnsignedBaseType uxLwrrentEvents;

    /* Are there any event bits set in this event group? */
    if( evgrpNO_EVENT_BITS_SET != ( pxEventGroup->xEventBits & ~evgrpEVENT_BITS_CONTROL_BYTES ) )
    {
        uxLwrrentEvents = eventpollEVENT_GROUP_BITS_SET;

        /* MCDC Test Point: STD_IF "prvAreAnyEventGroupBitsSet" */
    }
    else
    {
        /* There are no event bits set in this event group. */
        uxLwrrentEvents = eventpollNO_EVENTS;

        /* MCDC Test Point: STD_ELSE "prvAreAnyEventGroupBitsSet" */
    }

    return uxLwrrentEvents;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvEventGroupDeleted( xListItem *pxObjectEventListItem )
{
    eventPollType *pxEventPollObject;
    portUnsignedBaseType uxObjectEventIndex;
    eventPollObjectEventControlType *pxObjectEvent;
    portBaseType xSwitchRequired;

    /* THIS FUNCTION MUST BE CALLED FROM WITHIN A CRITICAL SECTION. */

    /* Obtain the handle of the event poll object from the object-event list
     * item. */
    pxEventPollObject = ( eventPollType * ) listGET_LIST_ITEM_OWNER( pxObjectEventListItem );

    /* Obtain the index into the object-events array to update from the
     * object-event list item. */
    uxObjectEventIndex = ( portUnsignedBaseType ) listGET_LIST_ITEM_VALUE( pxObjectEventListItem );

    /* Assign a pointer to the index to be updated. */
    pxObjectEvent = &( pxEventPollObject->paxRegisteredObjectEvents[ uxObjectEventIndex ] );

    /* Remove the object event list item from the target object's event list. */
    vListRemove( pxObjectEventListItem );

    /* If the task that owns the event poll object is not lwrrently blocked
     * waiting for object-events, the event group can be removed from the array
     * of object-events now. */
    if( pdFALSE == pxEventPollObject->xTaskIsBlocked )
    {
        /* Free the element in the object-events array for this object. */
        prvFreeObjectEvent( pxEventPollObject, pxObjectEvent );

        /* The task is not blocked, therefore no switch is required. */
        xSwitchRequired = pdFALSE;

        /* MCDC Test Point: STD_IF "prvEventGroupDeleted" */
    }
    else
    {
        /* Update the status of the event group object-event to indicate that
         * the event group has been deleted. */
        pxObjectEvent->uxLwrrentEvents = eventpollEVENT_GROUP_DELETED;

        /* Unblock the task and return it to the appropriate ready list. */
        xSwitchRequired = xTaskMoveTaskToReadyList( pxEventPollObject->pxOwnerTCB );

        /* Clear the 'task is blocked' flag. */
        pxEventPollObject->xTaskIsBlocked = pdFALSE;

        /* MCDC Test Point: STD_ELSE "prvEventGroupDeleted" */
    }

    return xSwitchRequired;
}

/*-----------------------------------------------------------------------------
 * Weak stub Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_FUNCTION portWEAK_FUNCTION portBaseType xEventGroupIsEventGroupHandleValid( const eventGroupType *pxEventGroup )
{
    /* MCDC Test Point: STD "WEAK_xEventGroupIsEventGroupHandleValid" */

    ( void ) pxEventGroup;
    return pdFALSE;
}
/*---------------------------------------------------------------------------*/

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_eventpollc.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "EventPollCTestHeaders.h"
    #include "EventPollCTest.h"
#endif
