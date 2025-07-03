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
#include "SafeRTOS_API.h"

/* MCDC Test Point: PROTOTYPE */

/*-----------------------------------------------------------------------------
 * Local Type Definitions
 *---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 * Local Constant Definitions
 *---------------------------------------------------------------------------*/

/* Constant that represents a zero wait time. */
#define evgrpNO_BLOCK_TIME          ( ( portTickType ) 0 )


/*-----------------------------------------------------------------------------
 * Local Macro Definitions
 *---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Local Function Declarations
 *---------------------------------------------------------------------------*/
/* Test the bits set in xLwrrentEventBits to see if the wait condition is met.
 * The wait condition is defined by xWaitForAllBits.  If xWaitForAllBits is
 * pdTRUE then the wait condition is met if all the bits set in xBitsToWaitFor
 * are also set in xLwrrentEventBits.  If xWaitForAllBits is pdFALSE then the
 * wait condition is met if any of the bits set in xBitsToWait for are also set
 * in xLwrrentEventBits. */
KERNEL_FUNCTION static portBaseType prvTestWaitCondition( const eventBitsType xLwrrentEventBits,
                                                          const eventBitsType xBitsToWaitFor,
                                                          const portBaseType xWaitForAllBits );

KERNEL_FUNCTION static void prvPlaceOnUnorderedEventList( xList *pxEventList,
                                                          const portTickType xItemValue,
                                                          const portTickType xTicksToWait );

KERNEL_FUNCTION static portBaseType prvRemoveFromUnorderedEventList( xListItem *pxEventListItem,
                                                                     const portTickType xItemValue );

KERNEL_FUNCTION static portTickType prvResetEventItemValue( void );

/*-----------------------------------------------------------------------------
 * Local Variable Declarations
 *---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 * Public Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xEventGroupCreate( eventGroupType *pxEventGroup,
                                                       eventGroupHandleType *pxEventGroupHandle )
{
    portBaseType xReturn;

    if( NULL == pxEventGroup )
    {
        /* A valid Event Group handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupCreate" */
    }
    else if( NULL == pxEventGroupHandle )
    {
        /* A valid pointer must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupCreate" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            if( pdTRUE == xEventGroupIsEventGroupHandleValid( pxEventGroup ) )
            {
                /* Error if the provided event group handle is valid. An event group
                 * must be explicitly deleted before it can be recreated. */
                xReturn = errEVENT_GROUP_ALREADY_IN_USE;

                /* MCDC Test Point: STD_IF "xEventGroupCreate" */
            }
            else if( pdPASS != portCOPY_DATA_TO_TASK( pxEventGroupHandle, &pxEventGroup, ( portUnsignedBaseType ) sizeof( eventGroupHandleType ) ) )
            {
                /* We pre-initialise the handle to make sure the destination address is valid. */

                xReturn = errILWALID_DATA_RANGE;

                /* MCDC Test Point: STD_ELSE_IF "xEventGroupCreate" */
            }
            else
            {
                /* SAFERTOSTRACE EVENTGROUPCREATE */

                /* The input parameters are valid, so the event group can be created. */
                xReturn = pdPASS;

                /* Ensure all the event bits are reset. */
                pxEventGroup->xEventBits = evgrpNO_EVENT_BITS_SET;

                /* Initialize the list of tasks waiting for bits to be set. */
                vListInitialise( &( pxEventGroup->xTasksWaitingForBits ) );

                /* If event poll is included, initialise the list of registered event poll objects.
                 * Otherwise an empty weak function will be called. */
                vEventPollInitEventGroupEventPollObjects( pxEventGroup );

                /* Finally, set the mirror value. */
                pxEventGroup->uxEventMirror = ~( ( portUnsignedBaseType ) pxEventGroup );

                /* MCDC Test Point: STD_ELSE "xEventGroupCreate" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_ELSE "xEventGroupCreate" */
    }

    /* SAFERTOSTRACE EVENTGROUPCREATE_FAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventGroupGetBits( eventGroupHandleType xEventGroupHandle,
                                                 eventBitsType *pxEventBitsSet )
{
    portBaseType xReturn;
    eventGroupType *pxEventGroup;

    if( NULL == xEventGroupHandle )
    {
        /* A valid Event Group handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupGetBits" */
    }
    else if( NULL == pxEventBitsSet )
    {
        /* A valid pointer must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupGetBits" */
    }
    else
    {
        /* Obtain access to the Event Group structure. */
        pxEventGroup = ( eventGroupType * ) xEventGroupHandle;

        portENTER_CRITICAL_WITHIN_API();
        {
            /* Check the mirror within a critical section, since another task
             * may delete the event group before we access xEventBits. */
            if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
            {
                /* A valid Event Group handle must be supplied. */
                xReturn = errILWALID_EVENT_GROUP_HANDLE;

                /* MCDC Test Point: STD_IF "xEventGroupGetBits" */
            }
            else
            {
                /* Retrieve the current setting of the event bits. */
                if( pdPASS != portCOPY_DATA_TO_TASK( pxEventBitsSet, &( pxEventGroup->xEventBits ), ( portUnsignedBaseType ) sizeof( eventBitsType ) ) )
                {
                    xReturn = errILWALID_DATA_RANGE;

                    /* MCDC Test Point: STD_IF "xEventGroupGetBits" */
                }
                else
                {
                    /* The event bits have been successfully retrieved. */
                    xReturn = pdPASS;

                    /* MCDC Test Point: STD_ELSE "xEventGroupGetBits" */
                }

                /* MCDC Test Point: STD_ELSE "xEventGroupGetBits" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_ELSE "xEventGroupGetBits" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventGroupGetBitsFromISR( eventGroupHandleType xEventGroupHandle,
                                                        eventBitsType *pxEventBitsSet )
{
    portBaseType xReturn;
    eventGroupType *pxEventGroup = ( eventGroupType * ) xEventGroupHandle;

    if( NULL == xEventGroupHandle )
    {
        /* A valid handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupGetBitsFromISR" */
    }
    else if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
    {
        /* Since event groups cannot be deleted from an ISR, we can check the
         * mirror while interrupts are enabled. */

        /* A valid Event Group handle must be supplied. */
        xReturn = errILWALID_EVENT_GROUP_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupGetBitsFromISR" */
    }
    else if( NULL == pxEventBitsSet )
    {
        /* A valid pointer must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupGetBitsFromISR" */
    }
    else
    {
        /* Retrieve the current setting of the event bits. */
        if( pdPASS != portCOPY_DATA_TO_ISR( pxEventBitsSet, &( pxEventGroup->xEventBits ), ( portUnsignedBaseType ) sizeof( eventBitsType ) ) )
        {
            xReturn = errILWALID_DATA_RANGE;

            /* MCDC Test Point: STD_IF "xEventGroupGetBitsFromISR" */
        }
        else
        {
            /* The event bits have been successfully retrieved. */
            xReturn = pdPASS;

            /* MCDC Test Point: STD_ELSE "xEventGroupGetBitsFromISR" */
        }

        /* MCDC Test Point: STD_ELSE "xEventGroupGetBitsFromISR" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventGroupSetBits( eventGroupHandleType xEventGroupHandle,
                                                 const eventBitsType xBitsToSet )
{
    portBaseType xReturn;
    eventGroupType *pxEventGroup = ( eventGroupType * ) xEventGroupHandle;
    xList *pxTaskList;
    const xMiniListItem *pxTaskListEnd;
    xListItem *pxListItem;
    xListItem *pxNextListItem;
    eventBitsType xBitsWaitedFor;
    eventBitsType xControlBits;
    eventBitsType xBitsToClear = evgrpNO_EVENT_BITS_SET;
    portBaseType xMatchFound;
    portBaseType xYieldRequired = pdFALSE;
    portBaseType xAlreadyYielded;
    portBaseType xHigherPriorityTaskAwoken;

    if( NULL == xEventGroupHandle )
    {
        /* A valid handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupSetBits" */
    }
    else if( evgrpNO_EVENT_BITS_SET != ( xBitsToSet & evgrpEVENT_BITS_CONTROL_BYTES ) )
    {
        /* Setting any bit in the control byte is not permitted. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupSetBits" */
    }
    else
    {
        /* Obtain the list of waiting tasks. */
        pxTaskList = &( pxEventGroup->xTasksWaitingForBits );

        /* Obtain the end of list marker for the list of waiting tasks. */
        pxTaskListEnd = listGET_END_MARKER( pxTaskList );

        /* Ensure no context switch oclwrs while the current task is accessing
         * the Event Group. This does not disable interrupts. */
        vTaskSuspendScheduler();
        {
            /* Check the mirror with the scheduler disabled, since another task
             * may delete the event group. */
            if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
            {
                /* A valid Event Group handle must be supplied. */
                xReturn = errILWALID_EVENT_GROUP_HANDLE;

                /* MCDC Test Point: STD_IF "xEventGroupSetBits" */
            }
            else
            {
                /* SAFERTOSTRACE EVENTGROUPSETBITS */

                /* The input parameters are valid. */
                xReturn = pdPASS;

                /* Set the specified event bits. */
                pxEventGroup->xEventBits |= xBitsToSet;

                /* Obtain the head entry in the list of waiting tasks. */
                pxListItem = listGET_HEAD_ENTRY( pxTaskList );

                /* See if the new bit value should unblock any tasks. */
                /* MCDC Test Point: WHILE_EXTERNAL "xEventGroupSetBits" "( ( const xMiniListItem * ) pxListItem != pxTaskListEnd )" */
                while( ( const xMiniListItem * ) pxListItem != pxTaskListEnd )
                {
                    pxNextListItem = listGET_NEXT( pxListItem );
                    xBitsWaitedFor = listGET_LIST_ITEM_VALUE( pxListItem );
                    xMatchFound = pdFALSE;

                    /* Split the bits waited for from the control bits. */
                    xControlBits = xBitsWaitedFor & evgrpEVENT_BITS_CONTROL_BYTES;
                    xBitsWaitedFor &= ~evgrpEVENT_BITS_CONTROL_BYTES;

                    if( evgrpNO_EVENT_BITS_SET == ( xControlBits & evgrpWAIT_FOR_ALL_BITS ) )
                    {
                        /* Just looking for single bit being set. */
                        if( ( xBitsWaitedFor & pxEventGroup->xEventBits ) != evgrpNO_EVENT_BITS_SET )
                        {
                            xMatchFound = pdTRUE;

                            /* MCDC Test Point: STD_IF "xEventGroupSetBits" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xEventGroupSetBits" */
                    }
                    else if( ( xBitsWaitedFor & pxEventGroup->xEventBits ) == xBitsWaitedFor )
                    {
                        /* All bits are set. */
                        xMatchFound = pdTRUE;

                        /* MCDC Test Point: STD_ELSE_IF "xEventGroupSetBits" */
                    }
                    else
                    {
                        /* Need all bits to be set, but not all the bits were set. */

                        /* MCDC Test Point: STD_ELSE "xEventGroupSetBits" */
                    }

                    if( pdFALSE != xMatchFound )
                    {
                        /* The bits match.  Should the bits be cleared on exit? */
                        if( ( xControlBits & evgrpCLEAR_EVENTS_ON_EXIT_BIT ) != evgrpNO_EVENT_BITS_SET )
                        {
                            xBitsToClear |= xBitsWaitedFor;

                            /* MCDC Test Point: STD_IF "xEventGroupSetBits" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xEventGroupSetBits" */

                        /* Store the actual event flag value in the task's event
                         * list item before removing the task from the event list.
                         * The evgrpUNBLOCKED_DUE_TO_BIT_SET bit is set so the task
                         * knows that is was unblocked due to its required bits
                         * matching, rather than because it timed out. */
                        xHigherPriorityTaskAwoken = prvRemoveFromUnorderedEventList( pxListItem,
                                                                                     ( pxEventGroup->xEventBits |
                                                                                       evgrpUNBLOCKED_DUE_TO_BIT_SET ) );
                        if( pdFALSE != xHigherPriorityTaskAwoken )
                        {
                            xYieldRequired = pdTRUE;

                            /* MCDC Test Point: STD_IF "xEventGroupSetBits" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xEventGroupSetBits" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xEventGroupSetBits" */

                    /* Move onto the next list item.  Note pxListItem->pxNext is not
                     * used here as the list item may have been removed from the
                     * event list and inserted into the ready/pending reading
                     * list. */
                    pxListItem = pxNextListItem;

                    /* MCDC Test Point: WHILE_INTERNAL "xEventGroupSetBits" "( ( const xMiniListItem * ) pxListItem != pxTaskListEnd )" */
                }

                /* Clear any bits that matched when the
                 * evgrpCLEAR_EVENTS_ON_EXIT_BIT bit was set in the control word. */
                pxEventGroup->xEventBits &= ~xBitsToClear;

                /* If event poll has been included, update all registered event poll objects.
                 * Otherwise, an empty weak function will be called. */
                if( pdTRUE == xEventPollUpdateEventGroupEventPollObjects( pxEventGroup ) )
                {
                    xYieldRequired = pdTRUE;

                    /* MCDC Test Point: STD_IF "xEventGroupSetBits" */
                }
                /* MCDC Test Point: ADD_ELSE "xEventGroupSetBits" */

                /* MCDC Test Point: STD_ELSE "xEventGroupSetBits" */
            }
        }
        xAlreadyYielded = xTaskResumeScheduler();

        /* If one or more higher priority tasks have been moved to the ready
         * lists, ensure a context switch takes place. */
        /* MCDC Test Point: EXP_IF_AND "xEventGroupSetBits" "( pdFALSE == xAlreadyYielded )" "( pdFALSE != xYieldRequired )" */
        if( ( pdFALSE == xAlreadyYielded ) && ( pdFALSE != xYieldRequired ) )
        {
            taskYIELD_WITHIN_API();

            /* MCDC Test Point: NULL "xEventGroupSetBits" */
        }

        /* MCDC Test Point: STD_ELSE "xEventGroupSetBits" */
    }

    /* SAFERTOSTRACE EVENTGROUPSETBITSFAILED */


    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventGroupSetBitsFromISR( eventGroupHandleType xEventGroupHandle,
                                                        const eventBitsType xBitsToSet,
                                                        portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn;
    const eventGroupType *pxEventGroup = ( const eventGroupType * ) xEventGroupHandle;

    if( NULL == xEventGroupHandle )
    {
        /* A valid Event Group handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupSetBitsFromISR" */
    }
    else if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
    {
        /* Since event groups cannot be deleted from an ISR, we can check the
         * mirror with interrupts enabled. */

        /* A valid Event Group handle must be supplied. */
        xReturn = errILWALID_EVENT_GROUP_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupSetBitsFromISR" */
    }
    else if( evgrpNO_EVENT_BITS_SET != ( xBitsToSet & evgrpEVENT_BITS_CONTROL_BYTES ) )
    {
        /* Setting any bit in the control byte is not permitted. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupSetBitsFromISR" */
    }
    else
    {

        /* Send a message to the timer task requesting that xEventGroupSetBits()
         * be exelwted from the timer task context. */
        xReturn = xTimerPendEventGroupSetBitsFromISR( xEventGroupHandle, xBitsToSet,
                                                      pxHigherPriorityTaskWoken );

        /* MCDC Test Point: STD_ELSE "xEventGroupSetBitsFromISR" */
    }
    /* SAFERTOSTRACE EVENTGROUPSETBITSISR */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventGroupClearBits( eventGroupHandleType xEventGroupHandle,
                                                   const eventBitsType xBitsToClear )
{
    portBaseType xReturn;
    eventGroupType *pxEventGroup = ( eventGroupType * ) xEventGroupHandle;

    if( NULL == xEventGroupHandle )
    {
        /* A valid Event Group handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupClearBits" */
    }
    else if( evgrpNO_EVENT_BITS_SET != ( xBitsToClear & evgrpEVENT_BITS_CONTROL_BYTES ) )
    {
        /* Clearing any bit in the control byte is not permitted. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupClearBits" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            /* Check the mirror within a critical section, since another task
             * may delete the event group before we access xEventBits. */
            if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
            {
                /* A valid Event Group handle must be supplied. */
                xReturn = errILWALID_EVENT_GROUP_HANDLE;

                /* MCDC Test Point: STD_IF "xEventGroupClearBits" */
            }
            else
            {
                /* SAFERTOSTRACE EVENTGROUPCLEARBITS */

                /* The input parameters are valid. */
                xReturn = pdPASS;

                /* Clear the specified bits. */
                pxEventGroup->xEventBits &= ~xBitsToClear;

                /* If event poll has been included, update all registered event poll objects.
                 * Otherwise, an empty weak function will be called.
                 * NOTE: A task cannot block on an event poll object waiting for
                 * event group bits to be cleared, therefore calling
                 * xEventPollUpdateEventGroupEventPollObjects() will not cause the
                 * owner task to be unblocked, so the return value can be safely
                 * ignored. */
                ( void ) xEventPollUpdateEventGroupEventPollObjects( pxEventGroup );

                /* MCDC Test Point: STD_ELSE "xEventGroupClearBits" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();

        /* MCDC Test Point: STD_ELSE "xEventGroupClearBits" */
    }

    /* SAFERTOSTRACE EVENTGROUPCLEARBITSFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventGroupClearBitsFromISR( eventGroupHandleType xEventGroupHandle,
                                                          const eventBitsType xBitsToClear,
                                                          portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn;
    const eventGroupType *pxEventGroup = ( const eventGroupType * ) xEventGroupHandle;

    if( NULL == xEventGroupHandle )
    {
        /* A valid Event Group handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupClearBitsFromISR" */
    }
    else if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
    {
        /* Since event groups cannot be deleted from an ISR, we can check the
         * mirror with interrupts enabled. */

        /* A valid Event Group handle must be supplied. */
        xReturn = errILWALID_EVENT_GROUP_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupClearBitsFromISR" */
    }
    else if( evgrpNO_EVENT_BITS_SET != ( xBitsToClear & evgrpEVENT_BITS_CONTROL_BYTES ) )
    {
        /* Clearing any bit in the control byte is not permitted. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupClearBitsFromISR" */
    }
    else
    {
        /* Send a message to the timer task requesting that xEventGroupClearBits()
         * be exelwted from the timer task context. */
        xReturn = xTimerPendEventGroupClearBitsFromISR( xEventGroupHandle, xBitsToClear,
                                                        pxHigherPriorityTaskWoken );

        /* MCDC Test Point: STD_ELSE "xEventGroupClearBitsFromISR" */
    }

    /* SAFERTOSTRACE EVENTGROUPCLEARBITSISR */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xEventGroupWaitBits( eventGroupHandleType xEventGroupHandle,
                                                  const eventBitsType xBitsToWaitFor,
                                                  const portBaseType xClearOnExit,
                                                  const portBaseType xWaitForAllBits,
                                                  eventBitsType *pxEventBitsSet,
                                                  portTickType xTicksToWait )
{
    portBaseType xReturn;
    eventGroupType *pxEventGroup = ( eventGroupType * ) xEventGroupHandle;
    eventBitsType xEventBitsSet = evgrpNO_EVENT_BITS_SET;
    eventBitsType xControlBits = evgrpNO_EVENT_BITS_SET;
    eventBitsType xEventBitsToCopy;
    portBaseType xWaitConditionMet;
    portBaseType xAlreadyYielded;
    xTimeOutType xTimeOut;

    /* Ensure the scheduler is not suspended. */
    if( xTaskIsSchedulerSuspended() != pdFALSE )
    {
        xReturn = errSCHEDULER_IS_SUSPENDED;

        /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
    }
    else if( NULL == xEventGroupHandle )
    {
        /* A valid Event Group handle must be supplied. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupWaitBits" */
    }
    else if( evgrpNO_EVENT_BITS_SET != ( xBitsToWaitFor & evgrpEVENT_BITS_CONTROL_BYTES ) )
    {
        /* Waiting for any bit in the control byte is not permitted. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupWaitBits" */
    }
    else if( evgrpNO_EVENT_BITS_SET == xBitsToWaitFor )
    {
        /* At least one wait bit must be specified. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_ELSE_IF "xEventGroupWaitBits" */
    }
    else
    {
        /* Ensure no context switch oclwrs while the current task is accessing
         * the Event Group. This does not disable interrupts. */
        vTaskSuspendScheduler();
        {

            /* This is the first time through the function - note the
             * time now. */
            vTaskSetTimeOut( &xTimeOut );

            if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
            {
                /* A valid Event Group handle must be supplied. */
                xReturn = errILWALID_EVENT_GROUP_HANDLE;

                /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
            }
            else
            {
                /* The call to xEventGroupWaitBits() is valid. Perform the
                 * following actions until either the bits are set, the block
                 * time expires or the event group is deleted. */
                do
                {
                    /* Retrieve the current setting of the event bits. */
                    xEventBitsSet = pxEventGroup->xEventBits;

                    /* Check to see if the wait condition has been met. */
                    xWaitConditionMet = prvTestWaitCondition( xEventBitsSet,
                                                              xBitsToWaitFor,
                                                              xWaitForAllBits );

                    if( pdFALSE != xWaitConditionMet )
                    {
                        /* The wait condition has been met. */
                        xReturn = pdPASS;

                        /* Clear the wait bits if requested to do so. */
                        if( pdFALSE != xClearOnExit )
                        {
                            portENTER_CRITICAL_WITHIN_API();
                            {
                                pxEventGroup->xEventBits &= ~xBitsToWaitFor;

                                /* If event poll has been included, update all registered event poll objects.
                                 * Otherwise, an empty weak function will be called.
                                 * NOTE: A task cannot block on an event poll object
                                 * waiting for event group bits to be cleared, therefore
                                 * calling xEventPollUpdateEventGroupEventPollObjects() will
                                 * not cause the owner task to be unblocked, so the
                                 * return value can be safely ignored. */
                                ( void ) xEventPollUpdateEventGroupEventPollObjects( pxEventGroup );
                            }
                            portEXIT_CRITICAL_WITHIN_API();

                            /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                        }
                        /* MCDC Test Point: ADD_ELSE "xEventGroupWaitBits" */

                        /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                    }
                    else
                    {
                        /* The requested bits have not yet been set. */
                        xReturn = errEVENT_GROUP_BITS_NOT_SET;

                        /* Is there any block time remaining? */
                        if( xTaskCheckForTimeOut( &xTimeOut, &xTicksToWait ) == pdFALSE )
                        {
                            /* The task is going to block to wait for its required bits
                             * to be set.  xControlBits are used to remember the
                             * specified behaviour of this call to xEventGroupWaitBits()
                             * - for use when the event bits unblock the task. */
                            if( pdFALSE != xClearOnExit )
                            {
                                xControlBits |= evgrpCLEAR_EVENTS_ON_EXIT_BIT;

                                /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                            }
                            /* MCDC Test Point: ADD_ELSE "xEventGroupWaitBits" */

                            if( pdFALSE != xWaitForAllBits )
                            {
                                xControlBits |= evgrpWAIT_FOR_ALL_BITS;

                                /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                            }
                            /* MCDC Test Point: ADD_ELSE "xEventGroupWaitBits" */

                            /* Store the bits that the calling task is waiting for in
                             * the task's event list item so the kernel knows when a
                             * match is found. Then enter the blocked state. */
                            prvPlaceOnUnorderedEventList( &( pxEventGroup->xTasksWaitingForBits ),
                                                          ( xBitsToWaitFor | xControlBits ),
                                                          xTicksToWait );

                            /* SAFERTOSTRACE EVENTGROUPWAITBITS_BLOCK */

                            /* Resume the scheduler. */
                            xAlreadyYielded = xTaskResumeScheduler();

                            /* Force a reschedule if xTaskResumeScheduler() has not already done
                             * so, in case this task is now in the blocked state. */
                            if( pdFALSE == xAlreadyYielded )
                            {
                                /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                                taskYIELD_WITHIN_API();
                            }
                            /* MCDC Test Point: ADD_ELSE "xEventGroupWaitBits" */

                            /* Suspend the scheduler before examining the event group again. */
                            vTaskSuspendScheduler();

                            /* The task blocked to wait for its required bits to be set,
                             * so at this point, either the required bits were set, the
                             * event group was deleted or the task was suspended and then
                             * resumed before the block time expired.
                             * If the required bits were set, they will have been
                             * stored in the task's event list item, and they should now be
                             * retrieved then cleared. */
                            xEventBitsSet = prvResetEventItemValue();

                            if( evgrpNO_EVENT_BITS_SET == ( xEventBitsSet & evgrpUNBLOCKED_DUE_TO_BIT_SET ) )
                            {
                                if( evgrpNO_EVENT_BITS_SET != ( xEventBitsSet & evgrpUNBLOCKED_DUE_TO_DELETION ) )
                                {
                                    /* The event group has been deleted.
                                     * If the event bits must be returned to the caller,
                                     * make sure that no bits are set. */
                                    xEventBitsSet = 0U;

                                    /* Let the caller know. */
                                    xReturn = errEVENT_GROUP_DELETED;

                                    /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                                }
                                /* MCDC Test Point: ADD_ELSE "xEventGroupWaitBits" */

                                /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                            }
                            else
                            {
                                /* The task unblocked because the bits were set. */
                                xReturn = pdPASS;
                                /* MCDC Test Point: STD_ELSE "xEventGroupWaitBits" */
                            }

                            /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
                        }
                        else
                        {
                            /* The block time has expired, ensure the loop is exited. */
                            xTicksToWait = evgrpNO_BLOCK_TIME;

                            /* MCDC Test Point: STD_ELSE "xEventGroupWaitBits" */
                        }

                        /* MCDC Test Point: STD_ELSE "xEventGroupWaitBits" */
                    }
                    /* MCDC Test Point: EXP_WHILE_INTERNAL_AND "xEventGroupWaitBits" "( errEVENT_GROUP_BITS_NOT_SET == xReturn )" "( xTicksToWait > evgrpNO_BLOCK_TIME )" */
                } while( ( errEVENT_GROUP_BITS_NOT_SET == xReturn ) && ( xTicksToWait > evgrpNO_BLOCK_TIME ) );

                /* MCDC Test Point: STD_ELSE "xEventGroupWaitBits" */
            }
        }
        ( void )xTaskResumeScheduler();

        /* Does the application want to know the setting of the event bits? */
        /* MCDC Test Point: EXP_IF_AND "xEventGroupWaitBits" "( errILWALID_EVENT_GROUP_HANDLE != xReturn )" "( NULL != pxEventBitsSet )" */
        if( ( errILWALID_EVENT_GROUP_HANDLE != xReturn ) && ( NULL != pxEventBitsSet ) )
        {
            /* Ensure no control bits are reported to the application. */
            xEventBitsToCopy = ( xEventBitsSet & ~evgrpEVENT_BITS_CONTROL_BYTES );

            /* Copy the event bits. */
            if( pdPASS != portCOPY_DATA_TO_TASK( pxEventBitsSet, &xEventBitsToCopy, ( portUnsignedBaseType ) sizeof( eventBitsType ) ) )
            {
                xReturn = errILWALID_DATA_RANGE;

                /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
            }
            /* MCDC Test Point: ADD_ELSE "xEventGroupWaitBits" */

            /* MCDC Test Point: STD_IF "xEventGroupWaitBits" */
        }

        /* MCDC Test Point: STD_ELSE "xEventGroupWaitBits" */
    }

    /* SAFERTOSTRACE EVENTGROUPWAITBITS_END */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_DELETE_FUNCTION portBaseType xEventGroupDelete( eventGroupHandleType xEventGroupHandle )
{
    eventGroupType *pxEventGroup = ( eventGroupType * ) xEventGroupHandle;
    const xList *pxTasksWaitingForBits;
    portBaseType xReturn;
    portBaseType xYieldRequired = pdFALSE;
    portBaseType xAlreadyYielded;
    portBaseType xHigherPriorityTaskAwoken;

    if( NULL == xEventGroupHandle )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xEventGroupDelete" */
    }
    else
    {
        vTaskSuspendScheduler();
        {
            if( xEventGroupIsEventGroupHandleValid( pxEventGroup ) == pdFALSE )
            {
                /* A valid Event Group handle must be supplied. */
                xReturn = errILWALID_EVENT_GROUP_HANDLE;

                /* MCDC Test Point: STD_IF "xEventGroupDelete" */
            }
            else
            {
                /* SAFERTOSTRACE EVENTGROUPDELETE */

                /* The input parameter is valid. */
                xReturn = pdPASS;

                /* Ilwalidate mirror, so the event group will be detected as
                 * invalid. */
                pxEventGroup->uxEventMirror = 0U;

                /* Obtain access to the event list. */
                pxTasksWaitingForBits = &( pxEventGroup->xTasksWaitingForBits );

                /* MCDC Test Point: WHILE_EXTERNAL "xEventGroupDelete" "( xListIsListEmpty( pxTasksWaitingForBits ) == pdFALSE )" */
                while( xListIsListEmpty( pxTasksWaitingForBits ) == pdFALSE )
                {
                    /* Unblock the task, returning 0 as the event list is being
                     * deleted and cannot therefore have any bits set. */
                    xHigherPriorityTaskAwoken = prvRemoveFromUnorderedEventList( pxTasksWaitingForBits->xListEnd.pxNext,
                                                                                 evgrpUNBLOCKED_DUE_TO_DELETION );
                    if( pdFALSE != xHigherPriorityTaskAwoken )
                    {
                        xYieldRequired = pdTRUE;

                        /* MCDC Test Point: STD_IF "xEventGroupDelete" */
                    }
                    /* MCDC Test Point: ADD_ELSE "xEventGroupDelete" */

                    /* MCDC Test Point: WHILE_INTERNAL "xEventGroupDelete" "( xListIsListEmpty( pxTasksWaitingForBits ) == pdFALSE )" */
                }

                /* If event poll has been included, remove this event group from
                 * any event poll objects it is registered with.
                 * Otherwise, an empty weak function will be called. */
                if( pdTRUE == xEventPollRemoveEventGroupEventPollObjects( pxEventGroup ) )
                {
                    xYieldRequired = pdTRUE;

                    /* MCDC Test Point: STD_IF "xEventGroupDelete" */
                }
                /* MCDC Test Point: ADD_ELSE "xEventGroupDelete" */

                /* MCDC Test Point: STD_ELSE "xEventGroupDelete" */
            }
        }
        xAlreadyYielded = xTaskResumeScheduler();

        /* If one or more higher priority tasks have been moved to the ready
         * lists, ensure a context switch takes place. */
        /* MCDC Test Point: EXP_IF_AND "xEventGroupDelete" "( pdFALSE == xAlreadyYielded )" "( pdFALSE != xYieldRequired )" */
        if( ( pdFALSE == xAlreadyYielded ) && ( pdFALSE != xYieldRequired ) )
        {
            taskYIELD_WITHIN_API();

            /* MCDC Test Point: NULL "xEventGroupDelete" */
        }

        /* MCDC Test Point: STD_ELSE "xEventGroupDelete" */
    }

    /* SAFERTOSTRACE EVENTGROUPDELETEFAILED */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Private API Function Definitions
 *---------------------------------------------------------------------------*/

portBaseType xEventGroupIsEventGroupHandleValid( const eventGroupType *pxEventGroup )
{
    portBaseType xReturn;

    if( ( portUnsignedBaseType ) pxEventGroup == ( portUnsignedBaseType ) ~( pxEventGroup->uxEventMirror ) )
    {
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "xEventGroupIsEventGroupHandleValid" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "xEventGroupIsEventGroupHandleValid" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Local Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvTestWaitCondition( const eventBitsType xLwrrentEventBits,
                                                          const eventBitsType xBitsToWaitFor,
                                                          const portBaseType xWaitForAllBits )
{
    portBaseType xWaitConditionMet;

    if( pdFALSE == xWaitForAllBits )
    {
        /* The task only has to wait for one bit within xBitsToWaitFor to be
         * set.  Is one already set? */
        if( ( xLwrrentEventBits & xBitsToWaitFor ) != evgrpNO_EVENT_BITS_SET )
        {
            xWaitConditionMet = pdTRUE;

            /* MCDC Test Point: STD_IF "prvTestWaitCondition" */
        }
        else
        {
            xWaitConditionMet = pdFALSE;

            /* MCDC Test Point: STD_ELSE "prvTestWaitCondition" */
        }
    }
    else
    {
        /* The task has to wait for all the bits in xBitsToWaitFor to be set.
         * Are they set already? */
        if( ( xLwrrentEventBits & xBitsToWaitFor ) == xBitsToWaitFor )
        {
            xWaitConditionMet = pdTRUE;

            /* MCDC Test Point: STD_IF "prvTestWaitCondition" */
        }
        else
        {
            xWaitConditionMet = pdFALSE;

            /* MCDC Test Point: STD_ELSE "prvTestWaitCondition" */
        }

        /* MCDC Test Point: STD_ELSE "prvTestWaitCondition" */
    }

    return xWaitConditionMet;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static void prvPlaceOnUnorderedEventList( xList *pxEventList,
                                                          const portTickType xItemValue,
                                                          const portTickType xTicksToWait )
{
    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED.
     * It is used by the event groups implementation. */
    xTCB *const pxTCB = pxLwrrentTCB;   /* pxLwrrentTCB is declared volatile so take a copy. */

    /* Store the item value in the event list item.  It is safe to access the
     * event list item here as interrupts won't access the event list item of a
     * task that is not in the Blocked state. */
    listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE ) );

    /* Place the event list item of the TCB at the end of the appropriate event
     * list. It is safe to access the event list here because it is part of an
     * event group implementation - and interrupts don't access event groups
     * directly (instead they access them indirectly by pending function calls
     * to the task level). */
    vListInsertEnd( pxEventList, &( pxTCB->xEventListItem ) );

    /* Add the current task to the appropriate delay list. */
    vTaskAddLwrrentTaskToDelayedList( xTicksToWait );

    /* MCDC Test Point: STD "prvPlaceOnUnorderedEventList" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static  portBaseType prvRemoveFromUnorderedEventList( xListItem *pxEventListItem,
                                                                      const portTickType xItemValue )
{
    xTCB *pxUnblockedTCB;
    portBaseType xReturn;

    /* THIS FUNCTION MUST BE CALLED WITH THE SCHEDULER SUSPENDED.
     * It is used by the event flags implementation. */

    /* Store the item value in the supplied event list item. */
    listSET_LIST_ITEM_VALUE( pxEventListItem, ( xItemValue | taskEVENT_LIST_ITEM_VALUE_IN_USE ) );

    /* Remove the event list item from the event list.
     * Interrupts do not access event lists. */
    pxUnblockedTCB = ( xTCB * ) listGET_LIST_ITEM_OWNER( pxEventListItem );
    vListRemove( pxEventListItem );

    /* Remove the task from the delayed list and add it to the ready list.
     * The scheduler is suspended so interrupts will not be accessing the ready
     * lists. */
    vListRemove( &( pxUnblockedTCB->xStateListItem ) );
    vTaskAddTaskToReadyList( pxUnblockedTCB );

    if( pxUnblockedTCB->uxPriority > pxLwrrentTCB->uxPriority )
    {
        /* Return true if the task removed from the event list has a higher
         * priority than the calling task. This allows the calling task to know
         * if it should force a context switch now. */
        xReturn = pdTRUE;

        /* MCDC Test Point: STD_IF "prvRemoveFromUnorderedEventList" */
    }
    else
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_ELSE "prvRemoveFromUnorderedEventList" */
    }

    /* If a task is blocked on a kernel object, then xNextTaskUnblockTime
     * might be set to the blocked task's time out time. If the task is
     * unblocked for a reason other than a timeout xNextTaskUnblockTime is
     * normally left unchanged, because it is automatically reset to a new
     * value when the tick count equals xNextTaskUnblockTime. However, if
     * tickless idling is used, it might be more important to enter sleep
     * mode at the earliest possible time - so reset xNextTaskUnblockTime
     * here to ensure it is updated at the earliest possible time. */
    vTaskResetNextTaskUnblockTime();

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portTickType prvResetEventItemValue( void )
{
    portTickType xReturn;
    xTCB *const pxTCB = pxLwrrentTCB;   /* pxLwrrentTCB is declared volatile so take a copy. */

    /* Retrieve the event bits from the event list item. */
    xReturn = listGET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ) );

    /* Reset the event list item to its normal value - so it can be used with
     * queues and semaphores. */
    listSET_LIST_ITEM_VALUE( &( pxTCB->xEventListItem ), ( portTickType )( configMAX_PRIORITIES - pxTCB->uxPriority ) );

    /* MCDC Test Point: STD "prvResetEventItemValue" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
 * Weak stub Function Definitions
 *---------------------------------------------------------------------------*/

/* If event poll is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION void vEventPollInitEventGroupEventPollObjects( eventGroupType *pxEventGroup )
{
    /* MCDC Test Point: STD "WEAK_vEventPollInitEventGroupEventPollObjects" */

    ( void ) pxEventGroup;
}
/*---------------------------------------------------------------------------*/

/* If event poll is not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION portBaseType xEventPollUpdateEventGroupEventPollObjects( const eventGroupType *pxEventGroup )
{
    /* MCDC Test Point: STD "WEAK_xEventPollUpdateEventGroupEventPollObjects" */

    ( void ) pxEventGroup;
    return pdFALSE;
}
/*---------------------------------------------------------------------------*/

/* If event poll is not included then this stub function is required. */
KERNEL_DELETE_FUNCTION portWEAK_FUNCTION portBaseType xEventPollRemoveEventGroupEventPollObjects( const eventGroupType *pxEventGroup )
{
    /* MCDC Test Point: STD "WEAK_xEventPollRemoveEventGroupEventPollObjects" */

    ( void ) pxEventGroup;
    return pdFALSE;
}
/*---------------------------------------------------------------------------*/

/* If timers are not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION portBaseType xTimerPendEventGroupClearBitsFromISR( void *pvEventGroup,
                                                                                     portTickType xBitsToManipulate,
                                                                                     portBaseType *pxHigherPriorityTaskWoken )
{
    /* MCDC Test Point: STD "WEAK_xTimerPendEventGroupClearBitsFromISR" */

    ( void ) pvEventGroup;
    ( void ) xBitsToManipulate;
    ( void ) pxHigherPriorityTaskWoken;
    return pdFAIL;
}
/*---------------------------------------------------------------------------*/

/* If timers are not included then this stub function is required. */
KERNEL_FUNCTION portWEAK_FUNCTION portBaseType xTimerPendEventGroupSetBitsFromISR( void *pvEventGroup,
                                                                                   portTickType xBitsToManipulate,
                                                                                   portBaseType *pxHigherPriorityTaskWoken )
{
    /* MCDC Test Point: STD "WEAK_xTimerPendEventGroupSetBitsFromISR" */

    ( void ) pvEventGroup;
    ( void ) xBitsToManipulate;
    ( void ) pxHigherPriorityTaskWoken;
    return pdFAIL;
}
/*---------------------------------------------------------------------------*/

/* SAFERTOSTRACE UXGETEVENTGROUPNUMBER */

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_eventgroupsc.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "eventGroupsTestHeaders.h"
    #include "eventGroupsTest.h"
#endif
