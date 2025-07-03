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


#ifndef TIMERS_H
#define TIMERS_H

#ifdef __cplusplus
extern "C" {
#endif


/*---------------------------------------------------------------------------*/
/* Type Definitions.                                                         */
/*---------------------------------------------------------------------------*/

/* Type by which software timers are referenced. */
typedef void *timerHandleType;

/* Define the prototype to which timer callback function pointers must conform. */
typedef void ( *timerCallbackFunctionPtrType )( timerHandleType xTimer );

/* Forward reference the timerInstanceParametersType and timerQueueMessageType types. */
typedef struct timerInstanceParameters timerInstanceParametersType;
typedef struct timerQueueMessage timerQueueMessageType;

/* Define the prototype for a timer instance specific message handling function. */
typedef portBaseType ( *timerMessageHandlerFunctionPtrType )(
    timerQueueMessageType xMessage, portTickType xTimeNow );

/* The structure of the timer task parameters. */
struct timerInstanceParameters
{
    xQueueHandle                                xTimerQueueHandle;
    xList                                     *pxLwrrentTimerList;
    xList                                     *pxOverflowTimerList;
    xList                                       xActiveTimerList1;
    xList                                       xActiveTimerList2;
    xTCB                                        xTimerTaskTCB;
    volatile timerMessageHandlerFunctionPtrType pxInstanceMessageHandler;
    portUnsignedBaseType                        uxTimerQueueHandleMirror;
    volatile portUnsignedBaseType               uxInstanceMessageHandlerMirror;
    portTickType                                xLastSampleTime;
};

/* The definition of the timers themselves. NOTE: this should not be accessed
directly by the user application, it is defined here simply to allow correctly
sized buffers to be defined.*/
typedef struct timerControlBlock
{
    timerCallbackFunctionPtrType    pxCallbackFunction; /* The function that will be called when the timer expires. */
    const portCharType             *pcTimerName;        /* Text name.  This is not used by the kernel, it is included simply to make debugging easier. */
    xListItem                       xTimerListItem;     /* Standard linked list item as used by all kernel features for event management. */
    portTickType                    xTimerPeriodInTicks;/* How quickly and often the timer expires. */
    portBaseType                    xIsPeriodic;        /* Set to pdTRUE if the timer should be automatically restarted once expired.  Set to pdFALSE if the timer is, in effect, a one shot timer. */
    portBaseType                    xIsActive;          /* Set to pdTRUE while the timer is active. */
    portBaseType                    xTimerID;           /* ID parameter - for application use to assist in identifying instances of timers when a common callback is used. */
    timerInstanceParametersType    *pxTimerInstance;    /* The timer instance to which this timer belongs. */
    void                           *pvObject;           /* Points to the instance of the C++ object that tracks this structure. */
    portUnsignedBaseType            uxCallbackMirror;   /* A mirror of the function pointer, used to validate the timer structure. */
    portTickType                    xTimerPeriodMirror; /* A mirror of xTimerPeriodInTicks. */
    /* SAFERTOSTRACE TIMERNUMBER */
} timerControlBlockType;

/* The structure supplied to xTimerCreate */
typedef struct timerInitParameters
{
    const portCharType *pcTimerName;
    portTickType xTimerPeriodInTicks;
    portBaseType xIsPeriodic;
    portBaseType xTimerID;
    timerControlBlockType *pxNewTimer;
    timerCallbackFunctionPtrType pxCallbackFunction;
    timerInstanceParametersType *pxTimerInstance;
    void *pvObject;
} timerInitParametersType;

/* The definition of messages that can be sent and received on the timer
queue.  NOTE: this should not be accessed directly by the user application,
it is defined here simply to allow correctly sized buffers to be defined. */
struct timerQueueMessage
{
    portBaseType            xMessageID;         /* The command being sent to the timer service task. */
    portTickType            xMessageValue;      /* An optional value used by a subset of commands, for example, when changing the period of a timer. */
    void                   *pvMessageObject;    /* The type of the message depends on the message type. */
};

/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/* xTimerCreate creates a new timer. */
KERNEL_CREATE_FUNCTION portBaseType xTimerCreate( const timerInitParametersType *const pxTimerParameters, timerHandleType *pxCreatedTimer );

/* Functions to control a timer (start/stop/reset/change and delete). */
KERNEL_FUNCTION portBaseType xTimerStart( timerHandleType xTimer, portTickType xBlockTime );
KERNEL_FUNCTION portBaseType xTimerStop( timerHandleType xTimer, portTickType xBlockTime );
KERNEL_FUNCTION portBaseType xTimerChangePeriod( timerHandleType xTimer, portTickType xNewPeriodInTicks, portTickType xBlockTime );
KERNEL_DELETE_FUNCTION portBaseType xTimerDelete( timerHandleType xTimer, portTickType xBlockTime );
/* ISR friendly versions or the timer command functions. */
KERNEL_FUNCTION portBaseType xTimerStartFromISR( timerHandleType xTimer, portBaseType *pxHigherPriorityTaskWoken );
KERNEL_FUNCTION portBaseType xTimerStopFromISR( timerHandleType xTimer, portBaseType *pxHigherPriorityTaskWoken );
KERNEL_FUNCTION portBaseType xTimerChangePeriodFromISR( timerHandleType xTimer, portTickType xNewPeriodInTicks, portBaseType *pxHigherPriorityTaskWoken );

/* xTimerIsTimerActive: Queries a timer to see if it is active or dormant. */
KERNEL_FUNCTION portBaseType xTimerIsTimerActive( timerHandleType xTimer );

/* Returns the ID assigned to the timer. */
KERNEL_FUNCTION portBaseType xTimerGetTimerID( timerHandleType xTimer, portBaseType *pxTimerID );

/* Returns a pointer to the Timer Local Storage Object */
KERNEL_FUNCTION void *pvTimerTLSObjectGet( timerHandleType xTimer );

/*-----------------------------------------------------------------------------
 * Private API
 *---------------------------------------------------------------------------*/
/* Functions beyond this part are not part of the public API and are intended
for use by the kernel only. */

KERNEL_INIT_FUNCTION portBaseType xTimerInitialiseFeature(
    timerInstanceParametersType *pxTimerInstance,
    const xPORT_INIT_PARAMETERS *const pxPortInitParameters );

/* Used by the checkpoint module to start a checkpoint timer. */
KERNEL_FUNCTION portBaseType xTimerStartImmediate( timerHandleType xTimer,
                                                   portTickType xRefTime,
                                                   portTickType xTimeNow,
                                                   portTickType xCommandTime );

/* Check the validity of the timer handle. */
KERNEL_FUNCTION portBaseType xTimerIsHandleValid( timerHandleType xTimer );

/* Verify that the timer instance structure is still valid. */
KERNEL_FUNCTION portBaseType xTimerCheckInstance( const timerInstanceParametersType *const pxTimerInstance );

/* Function to allow an instance specific message handler to be registered with
 * a timer task. */
KERNEL_FUNCTION portBaseType xTimerRegisterMsgHandler(
    timerInstanceParametersType *pxTimerInstance,
    timerMessageHandlerFunctionPtrType pxInstanceMessageHandler );

/* Functions that sends a message to the timer task queue requesting that the
 * xEventGroupSetBits() or the xEventGroupSetBits() function be exelwted from
 * the timer task context. */
KERNEL_FUNCTION portBaseType xTimerPendEventGroupSetBitsFromISR( void *pvEventGroup,
                                                                 portTickType xBitsToManipulate,
                                                                 portBaseType *pxHigherPriorityTaskWoken );

KERNEL_FUNCTION portBaseType xTimerPendEventGroupClearBitsFromISR( void *pvEventGroup,
                                                                   portTickType xBitsToManipulate,
                                                                   portBaseType *pxHigherPriorityTaskWoken );


#ifdef __cplusplus
}
#endif
#endif /* TIMERS_H */



