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


#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

/* The top 8 bits of the eventBitsType are used to hold control flags. */
#define evgrpCONTROL_FLAGS_SHIFT            ( portTICK_COUNT_NUM_BITS - 8U )

#define evgrpCLEAR_EVENTS_ON_EXIT_BIT       ( ( ( eventBitsType ) 0x01U << evgrpCONTROL_FLAGS_SHIFT ) )
#define evgrpUNBLOCKED_DUE_TO_BIT_SET       ( ( ( eventBitsType ) 0x02U << evgrpCONTROL_FLAGS_SHIFT ) )
#define evgrpWAIT_FOR_ALL_BITS              ( ( ( eventBitsType ) 0x04U << evgrpCONTROL_FLAGS_SHIFT ) )
#define evgrpUNBLOCKED_DUE_TO_DELETION      ( ( ( eventBitsType ) 0x08U << evgrpCONTROL_FLAGS_SHIFT ) )
#define evgrpEVENT_LIST_ITEM_VALUE_IN_USE   ( ( ( eventBitsType ) 0x80U << evgrpCONTROL_FLAGS_SHIFT ) )
#define evgrpEVENT_BITS_CONTROL_BYTES       ( ( ( eventBitsType ) 0xFFU << evgrpCONTROL_FLAGS_SHIFT ) )

/* Constant representing the case where no event bits are set. */
#define evgrpNO_EVENT_BITS_SET              ( ( eventBitsType ) 0 )


/*-----------------------------------------------------------------------------
 * Type Definitions
 *---------------------------------------------------------------------------*/

/* Type by which event groups are referenced. */
typedef void *eventGroupHandleType;

/* The type that holds event bits matches portTickType (necessary because of the
 * use of lists to manage events). */
typedef portTickType eventBitsType;

/* The definition of the event groups themselves. NOTE: this should not be
 * accessed directly by the user application, it is defined here simply to allow
 * correctly sized buffers to be defined. */
typedef struct eventGroup
{
    eventBitsType           xEventBits;                     /* The bits that can be set to indicate that an event has oclwrred. */
    xList                   xTasksWaitingForBits;           /* The list of tasks waiting for one or more bits to be set. */
    xList                   xRegisteredEventPollObjects;    /* The list of event poll objects that have registered an interest in this event group. */
    portUnsignedBaseType    uxEventMirror;                  /* The uxEventMirror will be set to the bitwise ilwerse (XOR) of the address of the eventGroup object. */
    /* SAFERTOSTRACE EVENTGROUPNUMBER */
} eventGroupType;


/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/* xEventGroupCreate creates a new event group. */
KERNEL_CREATE_FUNCTION portBaseType xEventGroupCreate( eventGroupType *pxEventGroup,
                                                       eventGroupHandleType *pxEventGroupHandle );

/* xEventGroupGetBits() retrieves the set of bits that are lwrrently set within
 * the event group. */
KERNEL_FUNCTION portBaseType xEventGroupGetBits( eventGroupHandleType xEventGroupHandle,
                                                 eventBitsType *pxEventBitsSet );

/* xEventGroupGetBitsFromISR() is a version of xEventGroupGetBits() that can be
 * called from an ISR. */
KERNEL_FUNCTION portBaseType xEventGroupGetBitsFromISR( eventGroupHandleType xEventGroupHandle,
                                                        eventBitsType *pxEventBitsSet );

/* xEventGroupSetBits() set bits within an event group. Setting bits in an event
 * group will automatically unblock tasks that are blocked waiting for the
 * bits. */
KERNEL_FUNCTION portBaseType xEventGroupSetBits( eventGroupHandleType xEventGroupHandle,
                                                 const eventBitsType xBitsToSet );

/* xEventGroupSetBitsFromISR() is a version of xEventGroupSetBits() that can be
 * called from an ISR. Setting bits in an event group is not a deterministic
 * operation because there are an unknown number of tasks that may be waiting
 * for the bit or bits being set. Therefore, xEventGroupSetBitsFromISR() sends a
 * message to the timer task to have the set operation performed in the context
 * of the timer task. */
KERNEL_FUNCTION portBaseType xEventGroupSetBitsFromISR( eventGroupHandleType xEventGroupHandle,
                                                        const eventBitsType xBitsToSet,
                                                        portBaseType *pxHigherPriorityTaskWoken );

/* xEventGroupClearBits() clears the specified bits within the event group. The
 * original setting of the event bits can be optionally retrieved by supplying a
 * non-NULL pointer for pxEventBitsSet. */
KERNEL_FUNCTION portBaseType xEventGroupClearBits( eventGroupHandleType xEventGroupHandle,
                                                   const eventBitsType xBitsToClear );

/* xEventGroupClearBitsFromISR() is a version of xEventGroupClearBits() that can
 * be called from an ISR. */
KERNEL_FUNCTION portBaseType xEventGroupClearBitsFromISR( eventGroupHandleType xEventGroupHandle,
                                                          const eventBitsType xBitsToClear,
                                                          portBaseType *pxHigherPriorityTaskWoken );

/* xEventGroupWaitBits() blocks to wait for one or more bits to be set within
 * the specified event group. */
KERNEL_FUNCTION portBaseType xEventGroupWaitBits( eventGroupHandleType xEventGroupHandle,
                                                  const eventBitsType xBitsToWaitFor,
                                                  const portBaseType xClearOnExit,
                                                  const portBaseType xWaitForAllBits,
                                                  eventBitsType *pxEventBitsSet,
                                                  portTickType xTicksToWait );

/* xEventGroupDelete() deletes an event group that was previously created by a
 * call to xEventGroupCreate().  Tasks that are blocked on the event group will
 * be unblocked and obtain 0 as the event group's value. */
KERNEL_DELETE_FUNCTION portBaseType xEventGroupDelete( eventGroupHandleType xEventGroupHandle );

/* SAFERTOSTRACE UXGETEVENTGROUPNUMBERPROTO */

/*-----------------------------------------------------------------------------
 * Private API
 *---------------------------------------------------------------------------*/

/* Functions beyond this part are not part of the public API and are intended
 * for use by the kernel only. */

/* Tests the validity of the event group, returning pdTRUE if the event group
 * is valid, pdFALSE otherwise. */
portBaseType xEventGroupIsEventGroupHandleValid( const eventGroupType *pxEventGroup );

/*---------------------------------------------------------------------------*/


#ifdef __cplusplus
}
#endif

#endif /* EVENT_GROUPS_H */
