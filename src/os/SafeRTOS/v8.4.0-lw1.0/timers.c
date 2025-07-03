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

/*-----------------------------------------------------------------------------
 * Constant Definitions.
 *---------------------------------------------------------------------------*/

/* IDs for commands that can be sent/received on the timer queue. */
#define timerCOMMAND_START                          ( ( portBaseType ) 0 )
#define timerCOMMAND_STOP                           ( ( portBaseType ) 1 )
#define timerCOMMAND_CHANGE_PERIOD                  ( ( portBaseType ) 2 )
#define timerCOMMAND_DELETE                         ( ( portBaseType ) 3 )

/* Kernel service requests use negative numbers. */
#define timerCMD_EXELWTE_EVENT_GROUP_SET_BITS       ( ( portBaseType ) -1 )
#define timerCMD_EXELWTE_EVENT_GROUP_CLEAR_BITS     ( ( portBaseType ) -2 )


#define timerNO_DELAY                               ( ( portTickType ) 0 )

/*-----------------------------------------------------------------------------
 * Variables.
 *---------------------------------------------------------------------------*/


/* Define a default instance of the timer feature. This defines the timer task
 * parameters, the timer lists and queues required to manage an instance of the
 * timer feature. */
KERNEL_DATA static timerInstanceParametersType xTimerDefaultTimerInstance = { 0 };

/*-----------------------------------------------------------------------------
 * Function prototypes.
 *---------------------------------------------------------------------------*/

/* The timer service task (daemon).  Timer functionality is controlled by this
 * task.  Other tasks communicate with the timer service task using the
 * timer message queue associated with a timer instance. */
KERNEL_FUNCTION static void prvTimerTask( void *pvParameters );

/* Called by the timer service task to interpret and process a command it
 * received on the timer queue. */
KERNEL_FUNCTION static portBaseType prvProcessReceivedCommands( timerQueueMessageType xMessage,
                                                                portTickType xTimeNow );

/* An active timer has reached its expire time.  Reload the timer if it is a
 * periodic timer, then call its callback. */
KERNEL_FUNCTION static void prvProcessExpiredTimer( const timerInstanceParametersType *const pxTimerInstance,
                                                    portTickType xNextExpireTime,
                                                    portTickType xTimeNow );

/* The tick count has overflowed.  Switch the timer lists after ensuring the
 * current timer list does not still reference some timers. */
KERNEL_FUNCTION static void prvSwitchTimerLists( timerInstanceParametersType *pxTimerInstance );

/* Obtain the current tick count, setting *pxTimerListsWereSwitched to pdTRUE
 * if a tick count overflow oclwrred since prvSampleTimeNow() was last
 * called. */
KERNEL_FUNCTION static portTickType prvSampleTimeNow( timerInstanceParametersType *pxTimerInstance,
                                                      portBaseType *pxTimerListsWereSwitched );

/* If the timer list contains any active timers then return the expire time of
 * the timer that will expire first and set *pxListWasEmpty to pdFALSE.  If the
 * timer list does not contain any timers then return 0 and set *pxListWasEmpty
 * to pdTRUE. */
KERNEL_FUNCTION static portTickType prvGetNextExpireTime( const timerInstanceParametersType *const pxTimerInstance,
                                                          portBaseType *pxListWasEmpty, portTickType xTimeNow );

/* Insert the timer into either xActiveTimerList1, or xActiveTimerList2,
 * depending on if the expire time causes a timer counter overflow. */
KERNEL_FUNCTION static portBaseType prvInsertTimerInActiveList( timerControlBlockType *pxTimer,
                                                                portTickType xNextExpiryTime,
                                                                portTickType xTimeNow,
                                                                portTickType xCommandTime );

/* The function that makes the callback.
 * NOTE: it cannot be placed in the kernel function region because, if the
 * callback function lowers the privilege level, a fault will be generated
 * when it returns to prvPerformTimerCallback().
 */
KERNEL_UNPRIV_FUNCTION static portNO_INLINE_FUNC_DEF void prvPerformTimerCallback( timerControlBlockType *pxTimer );

/* Generic timer command function that writes the API timer command requests to
the timer queue. */
KERNEL_FUNCTION static portBaseType prvTimerGenericCommand( timerHandleType xTimer,
                                                            portBaseType xCommandID, portTickType xOptionalValue,
                                                            portTickType xBlockTime );

/* ISR version of the generic timer command function that writes the API timer
command requests to the timer queue. */
KERNEL_FUNCTION static portBaseType prvTimerGenericCommandISR( timerHandleType xTimer,
                                                               portBaseType xCommandID, portTickType xOptionalValue,
                                                               portBaseType *pxHigherPriorityTaskWoken );

/* MCDC Test Point: PROTOTYPE */

/*---------------------------------------------------------------------------*/

KERNEL_INIT_FUNCTION portBaseType xTimerInitialiseFeature( timerInstanceParametersType *pxTimerInstance,
                                                           const xPORT_INIT_PARAMETERS *const pxPortInitParameters )
{
    portBaseType xReturn;
    xTaskParameters xTimerTaskParameters;
    static const portCharType acTimerFeatureTaskName[] = "KernelTimerMgr";

    /* if pxTimerInstance is not specified (i.e. NULL) then use the default
     * timer instance. */
    if( NULL == pxTimerInstance )
    {
        pxTimerInstance = &xTimerDefaultTimerInstance;
        /* MCDC Test Point: STD_IF "xTimerInitialiseFeature" */
    }
    /* MCDC Test Point: ADD_ELSE "xTimerInitialiseFeature" */

    /* Initialise the list from which active timers are referenced, and create
    the queue used to communicate with the timer service */
    vListInitialise( & ( pxTimerInstance->xActiveTimerList1 ) );
    vListInitialise( & ( pxTimerInstance->xActiveTimerList2 ) );
    pxTimerInstance->pxLwrrentTimerList = & ( pxTimerInstance->xActiveTimerList1 );
    pxTimerInstance->pxOverflowTimerList = & ( pxTimerInstance->xActiveTimerList2 );
    pxTimerInstance->pxInstanceMessageHandler = NULL;
    pxTimerInstance->xLastSampleTime = ( portTickType ) 0;

    /* Create the queue that will allow commands to be sent to the timers. */
    xReturn = xQueueCreate( pxPortInitParameters->pcTimerCommandQueueBuffer,
                            pxPortInitParameters->uxTimerCommandQueueBufferSize,
                            pxPortInitParameters->uxTimerCommandQueueLength,
                            sizeof( timerQueueMessageType ),
                            &( pxTimerInstance->xTimerQueueHandle ) );

    if( pdPASS == xReturn )
    {
        /* Create the timer task without storing its handle. */
        xTimerTaskParameters.pvTaskCode = &prvTimerTask;
        xTimerTaskParameters.pcTaskName = acTimerFeatureTaskName;
        xTimerTaskParameters.pxTCB = & ( pxTimerInstance->xTimerTaskTCB );
        xTimerTaskParameters.pcStackBuffer = pxPortInitParameters->pcTimerTaskStackBuffer;
        xTimerTaskParameters.uxStackDepthBytes = pxPortInitParameters->uxTimerTaskStackSize;
        xTimerTaskParameters.pvParameters = ( void * ) pxTimerInstance;
        xTimerTaskParameters.uxPriority = pxPortInitParameters->uxTimerTaskPriority;

        vPortAddKernelTaskParams( &xTimerTaskParameters );

        xTimerTaskParameters.pvObject = NULL;

        /* Create the task */
        xReturn = xTaskCreate( &xTimerTaskParameters, NULL );

        /* Assuming that all is OK, callwlate the mirror variables which are
         * used to assess the validity of the structure. */
        if( pdPASS == xReturn )
        {
            pxTimerInstance->uxInstanceMessageHandlerMirror = ~( ( portUnsignedBaseType ) NULL );
            pxTimerInstance->uxTimerQueueHandleMirror = ~( ( portUnsignedBaseType ) pxTimerInstance->xTimerQueueHandle );

            /* MCDC Test Point: STD_IF "xTimerInitialiseFeature" */
        }
        /* MCDC Test Point: ADD_ELSE "xTimerInitialiseFeature" */
    }
    /* MCDC Test Point: ADD_ELSE "xTimerInitialiseFeature" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerRegisterMsgHandler( timerInstanceParametersType *pxTimerInstance,
                                                       timerMessageHandlerFunctionPtrType pxInstanceMessageHandler )
{
    portBaseType xReturn;

    if( pdPASS != xTimerCheckInstance( pxTimerInstance ) )
    {
        xReturn = errILWALID_TIMER_TASK_INSTANCE;

        /* MCDC Test Point: STD_IF "xTimerRegisterMsgHandler" */
    }
    else
    {
        /* A valid timer instance has been supplied. */
        xReturn = pdPASS;

        portENTER_CRITICAL_WITHIN_API();
        {
            pxTimerInstance->pxInstanceMessageHandler = pxInstanceMessageHandler;
            pxTimerInstance->uxInstanceMessageHandlerMirror = ~( ( portUnsignedBaseType ) pxTimerInstance->pxInstanceMessageHandler );

            /* MCDC Test Point: STD_ELSE "xTimerRegisterMsgHandler" */
        }
        portEXIT_CRITICAL_WITHIN_API();
    }
    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xTimerCreate( const timerInitParametersType *const pxTimerParameters,
                                                  timerHandleType *pxCreatedTimer )
{
    portBaseType xReturn;
    timerInstanceParametersType *pxTimerInstance;
    timerControlBlockType *pxNewTimer;

    if( NULL == pxTimerParameters )
    {
        /* Error if a initialisation structure is not provided. */
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xTimerCreate" */
    }
    else
    {
        if( ( portTickType ) 0U == pxTimerParameters->xTimerPeriodInTicks )
        {
            /* A timer must have a time associated. */
            xReturn = errILWALID_PARAMETERS;

            /* MCDC Test Point: STD_IF "xTimerCreate" */
        }
        else if( ( timerControlBlockType * ) NULL == pxTimerParameters->pxNewTimer )
        {
            /* Error if a timer structure is not provided. */
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: STD_ELSE_IF "xTimerCreate" */
        }
        else if( NULL == pxTimerParameters->pxCallbackFunction )
        {
            /* Error if a callback function is not provided. */
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: STD_ELSE_IF "xTimerCreate" */
        }
        else if( ( timerHandleType * ) NULL == pxCreatedTimer )
        {
            /* Error if a timer handle is not provided. */
            xReturn = errNULL_PARAMETER_SUPPLIED;

            /* MCDC Test Point: STD_ELSE_IF "xTimerCreate" */
        }
        else if( ( pdFALSE != pxTimerParameters->xIsPeriodic ) &&
                 ( pdTRUE != pxTimerParameters->xIsPeriodic ) )
        {
            /* Error if the reload is not true or false. */
            xReturn = errILWALID_PARAMETERS;

            /* MCDC Test Point: NULL "xTimerCreate" */
        }
        else
        {
            if( NULL == pxTimerParameters->pxTimerInstance )
            {
                pxTimerInstance = &xTimerDefaultTimerInstance;

                /* MCDC Test Point: STD_IF "xTimerCreate" */
            }
            else
            {
                pxTimerInstance = pxTimerParameters->pxTimerInstance;
                /* MCDC Test Point: STD_ELSE "xTimerCreate" */
            }


            if( pdPASS != xTimerCheckInstance( pxTimerInstance ) )
            {
                xReturn = errILWALID_TIMER_TASK_INSTANCE;

                /* MCDC Test Point: STD_IF "xTimerCreate" */
            }
            else
            {
                /* Local copy of pxNewTimer for efficiency */
                pxNewTimer = pxTimerParameters->pxNewTimer;

                portENTER_CRITICAL_WITHIN_API();
                {
                    if( pdTRUE == xTimerIsHandleValid( ( timerHandleType ) pxNewTimer ) )
                    {
                        /* Error if the provided timer handle is valid. A timer must
                         * be explicitly deleted before it can be recreated. */
                        xReturn = errTIMER_ALREADY_IN_USE;

                        /* MCDC Test Point: STD_IF "xTimerCreate" */
                    }
                    else if( pdPASS != portCOPY_DATA_TO_TASK( pxCreatedTimer, &pxNewTimer, ( portUnsignedBaseType ) sizeof( timerHandleType ) ) )
                    {
                        xReturn = errILWALID_DATA_RANGE;
                        /* MCDC Test Point: STD_ELSE_IF "xTimerCreate" */
                    }
                    else
                    {
                        /* All the input parameters are valid, therefore the
                         * timer can be created. */
                        xReturn = pdPASS;

                        /* Initialise the timer structure members using the function
                         * parameters. */
                        pxNewTimer->pxCallbackFunction = pxTimerParameters->pxCallbackFunction;
                        pxNewTimer->pcTimerName = pxTimerParameters->pcTimerName;
                        pxNewTimer->xTimerPeriodInTicks = pxTimerParameters->xTimerPeriodInTicks;
                        pxNewTimer->xIsPeriodic = pxTimerParameters->xIsPeriodic;
                        pxNewTimer->xTimerID = pxTimerParameters->xTimerID;
                        vListInitialiseItem( &( pxNewTimer->xTimerListItem ) );
                        pxNewTimer->xIsActive = pdFALSE;

                        pxNewTimer->pxTimerInstance = pxTimerInstance;

                        pxNewTimer->pvObject = pxTimerParameters->pvObject;

                        pxNewTimer->uxCallbackMirror = ~( ( portUnsignedBaseType ) pxTimerParameters->pxCallbackFunction );
                        pxNewTimer->xTimerPeriodMirror = ~( pxTimerParameters->xTimerPeriodInTicks );

                        /* MCDC Test Point: STD_ELSE "xTimerCreate" */
                    }
                }
                portEXIT_CRITICAL_WITHIN_API();

                /* MCDC Test Point: STD_ELSE "xTimerCreate" */
            }
            /* MCDC Test Point: STD_ELSE "xTimerCreate" */
        }
        /* MCDC Test Point: STD_ELSE "xTimerCreate" */

        /* MCDC Test Point: EXP_IF_AND "xTimerCreate" "( pdFALSE != pxTimerParameters->xIsPeriodic )" "( pdTRUE != pxTimerParameters->xIsPeriodic )" "Deviate: FF" */
    }

    /* SAFERTOSTRACE TIMERCREATE */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerStart( timerHandleType xTimer,
                                          portTickType xBlockTime )
{
    /* MCDC Test Point: STD "xTimerStart" */

    return prvTimerGenericCommand( xTimer, timerCOMMAND_START, xTaskGetTickCount(),
                                   xBlockTime );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerStop( timerHandleType xTimer,
                                         portTickType xBlockTime )
{
    /* MCDC Test Point: STD "xTimerStop" */

    return prvTimerGenericCommand( xTimer, timerCOMMAND_STOP, ( portTickType ) 0,
                                   xBlockTime );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerChangePeriod( timerHandleType xTimer,
                                                 portTickType xNewPeriodInTicks,
                                                 portTickType xBlockTime )
{
    portBaseType xReturn;

    if( ( portTickType ) 0 == xNewPeriodInTicks )
    {
        /* A timer must have a time associated. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_IF "xTimerChangePeriod" */
    }
    else
    {
        xReturn = prvTimerGenericCommand( xTimer, timerCOMMAND_CHANGE_PERIOD,
                                          xNewPeriodInTicks, xBlockTime );

        /* MCDC Test Point: STD_ELSE "xTimerChangePeriod" */
    }
    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_DELETE_FUNCTION portBaseType xTimerDelete( timerHandleType xTimer,
                                                  portTickType xBlockTime )
{
    /* MCDC Test Point: STD "xTimerDelete" */

    return prvTimerGenericCommand( xTimer, timerCOMMAND_DELETE, ( portTickType ) 0,
                                   xBlockTime );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerStartFromISR( timerHandleType xTimer,
                                                 portBaseType *pxHigherPriorityTaskWoken )
{
    /* MCDC Test Point: STD "xTimerStartFromISR" */

    return prvTimerGenericCommandISR( xTimer, timerCOMMAND_START,
                                      xTaskGetTickCountFromISR(),
                                      pxHigherPriorityTaskWoken );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerStopFromISR( timerHandleType xTimer,
                                                portBaseType *pxHigherPriorityTaskWoken )
{
    /* MCDC Test Point: STD "xTimerStopFromISR" */

    return prvTimerGenericCommandISR( xTimer, timerCOMMAND_STOP, ( portTickType ) 0,
                                      pxHigherPriorityTaskWoken );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerChangePeriodFromISR( timerHandleType xTimer,
                                                        portTickType xNewPeriodInTicks,
                                                        portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn;

    if( ( portTickType ) 0 == xNewPeriodInTicks )
    {
        /* A timer must have a time associated. */
        xReturn = errILWALID_PARAMETERS;

        /* MCDC Test Point: STD_IF "xTimerChangePeriodFromISR" */
    }
    else
    {
        xReturn = prvTimerGenericCommandISR( xTimer, timerCOMMAND_CHANGE_PERIOD,
                                             xNewPeriodInTicks, pxHigherPriorityTaskWoken );

        /* MCDC Test Point: STD_ELSE "xTimerChangePeriodFromISR" */
    }
    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerIsTimerActive( timerHandleType xTimer )
{
    portBaseType xTimerIsActive;
    timerControlBlockType *pxTimer = ( timerControlBlockType * ) xTimer;

    if( pdTRUE == xTimerIsHandleValid( xTimer ) )
    {
        /* Is the timer lwrrently active? */
        xTimerIsActive = pxTimer->xIsActive;

        /* MCDC Test Point: STD_IF "xTimerIsTimerActive" */
    }
    else
    {
        xTimerIsActive = errILWALID_TIMER_HANDLE;

        /* MCDC Test Point: STD_ELSE "xTimerIsTimerActive" */
    }

    return xTimerIsActive;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerGetTimerID( timerHandleType xTimer, portBaseType *pxTimerID )
{
    timerControlBlockType *pxTimer = ( timerControlBlockType * ) xTimer;
    portBaseType xReturn;

    if( NULL == pxTimerID )
    {
        xReturn = errNULL_PARAMETER_SUPPLIED;

        /* MCDC Test Point: STD_IF "xTimerGetTimerID" */
    }
    else if( pdFALSE == xTimerIsHandleValid( xTimer ) )
    {
        xReturn = errILWALID_TIMER_HANDLE;

        /* MCDC Test Point: STD_ELSE_IF "xTimerGetTimerID" */
    }
    else if( pdPASS != portCOPY_DATA_TO_TASK( ( void * ) pxTimerID, ( void * ) &( pxTimer->xTimerID ), ( portUnsignedBaseType ) sizeof( portBaseType ) ) )
    {
        xReturn = errILWALID_DATA_RANGE;
        /* MCDC Test Point: STD_ELSE_IF "xTimerGetTimerID" */
    }
    else
    {
        xReturn = pdPASS;

        /* MCDC Test Point: STD_ELSE "xTimerGetTimerID" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerStartImmediate( timerHandleType xTimer,
                                                   portTickType xRefTime,
                                                   portTickType xTimeNow,
                                                   portTickType xCommandTime )
{
    portBaseType xReturn;
    timerControlBlockType *pxTimer = ( timerControlBlockType * ) xTimer;

    if( pdTRUE == xTimerIsHandleValid( xTimer ) )
    {
        /* Start the timer. */
        xReturn = prvInsertTimerInActiveList( pxTimer,
                                              xRefTime + pxTimer->xTimerPeriodInTicks,
                                              xTimeNow, xCommandTime );
        if( pdTRUE == xReturn )
        {
            /* The timer expired before it was added to the active timer list.
             * Process it now. */
            prvPerformTimerCallback( pxTimer );

            /* MCDC Test Point: STD_IF "xTimerStartImmediate" */
        }
        /* MCDC Test Point: ADD_ELSE "xTimerStartImmediate" */
    }
    else
    {
        xReturn = errILWALID_TIMER_HANDLE;

        /* MCDC Test Point: STD_ELSE "xTimerStartImmediate" */
    }

    return xReturn;
}

/*---------------------------------------------------------------------------*/


KERNEL_FUNCTION static portBaseType prvTimerGenericCommand( timerHandleType xTimer,
                                                            portBaseType xCommandID, portTickType xOptionalValue,
                                                            portTickType xBlockTime )
{
    portBaseType xReturn;
    timerQueueMessageType xMessage;
    timerControlBlockType *pxTimer = ( timerControlBlockType * ) xTimer;

    if( pdTRUE != xTimerIsHandleValid( xTimer ) )
    {
        xReturn = errILWALID_TIMER_HANDLE;

        /* MCDC Test Point: STD_IF "prvTimerGenericCommand" */
    }
    else
    {
        /* Send a message to the timer service task to perform a particular
         * action on a particular timer definition. */

        xMessage.xMessageID = xCommandID;
        xMessage.xMessageValue = xOptionalValue;
        xMessage.pvMessageObject = xTimer;

        if( pdTRUE == xTaskIsSchedulerStarted() )
        {
            xReturn = xQueueSend( pxTimer->pxTimerInstance->xTimerQueueHandle, &xMessage, xBlockTime );

            /* MCDC Test Point: STD_IF "prvTimerGenericCommand" */
        }
        else
        {
            xReturn = xQueueSend( pxTimer->pxTimerInstance->xTimerQueueHandle, &xMessage, timerNO_DELAY );

            /* MCDC Test Point: STD_ELSE "prvTimerGenericCommand" */
        }
        /* MCDC Test Point: STD_ELSE "prvTimerGenericCommand" */
    }
    /* SAFERTOSTRACE TIMERGENERICCOMMAND */
    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvTimerGenericCommandISR( timerHandleType xTimer,
                                                               portBaseType xCommandID, portTickType xOptionalValue,
                                                               portBaseType *pxHigherPriorityTaskWoken )
{
    portBaseType xReturn = pdPASS;
    timerQueueMessageType xMessage;
    portUnsignedBaseType uxSavedInterruptStatus;
    timerControlBlockType *pxTimer = ( timerControlBlockType * ) xTimer;

    if( NULL == xTimer )
    {
        xReturn = errILWALID_TIMER_HANDLE;

        /* MCDC Test Point: STD_IF "prvTimerGenericCommandISR" */
    }
    else
    {
        uxSavedInterruptStatus = taskSET_INTERRUPT_MASK_FROM_ISR();
        {
            if( ( ( ( portUnsignedBaseType ) ( pxTimer->pxCallbackFunction ) ) != ~( pxTimer->uxCallbackMirror ) ) ||
                    ( pxTimer->xTimerPeriodInTicks != ~( pxTimer->xTimerPeriodMirror ) ) )
            {
                xReturn = errILWALID_TIMER_HANDLE;

                /* MCDC Test Point: STD_IF "prvTimerGenericCommandISR" */
            }
            /* MCDC Test Point: EXP_IF_OR "prvTimerGenericCommandISR" "( ( ( portUnsignedBaseType ) ( pxTimer->pxCallbackFunction ) ) != ~( pxTimer->uxCallbackMirror ) )" "( pxTimer->xTimerPeriodInTicks != ~( pxTimer->xTimerPeriodMirror ) )" */
        }
        taskCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus );

        if( pdPASS == xReturn )
        {
            /* Send a message to the timer service task to perform a particular action
            on a particular timer definition. */

            xMessage.xMessageID = xCommandID;
            xMessage.xMessageValue = xOptionalValue;
            xMessage.pvMessageObject = xTimer;

            xReturn = xQueueSendFromISR( pxTimer->pxTimerInstance->xTimerQueueHandle, &xMessage, pxHigherPriorityTaskWoken );

            /* MCDC Test Point: STD_IF "prvTimerGenericCommandISR" */
        }
        /* MCDC Test Point: ADD_ELSE "prvTimerGenericCommandISR" */
        /* MCDC Test Point: STD_ELSE "prvTimerGenericCommandISR" */
    }
    /* SAFERTOSTRACE TIMERGENERICCOMMANDISR */
    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerIsHandleValid( timerHandleType xTimer )
{
    timerControlBlockType *pxTimer = ( timerControlBlockType * ) xTimer;
    portBaseType xReturn = pdTRUE;

    if( NULL == xTimer )
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_IF "xTimerIsHandleValid" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            /* MCDC Test Point: EXP_IF_OR "xTimerIsHandleValid" "( ( ( portUnsignedBaseType ) ( pxTimer->pxCallbackFunction ) ) != ~( pxTimer->uxCallbackMirror ) )" "( pxTimer->xTimerPeriodInTicks != ~( pxTimer->xTimerPeriodMirror ) )" */
            if( ( ( ( portUnsignedBaseType ) ( pxTimer->pxCallbackFunction ) ) != ~( pxTimer->uxCallbackMirror ) ) ||
                    ( pxTimer->xTimerPeriodInTicks != ~( pxTimer->xTimerPeriodMirror ) ) )
            {
                xReturn = pdFALSE;

                /* MCDC Test Point: NULL "xTimerIsHandleValid" */
            }
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xTimerIsHandleValid" */
    }
    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static void prvTimerTask( void *pvParameters )
{
    portTickType xNextExpireTime;
    portBaseType xListWasEmpty;
    portTickType xTimeNow;
    portBaseType xTimerListsWereSwitched;
    timerQueueMessageType xMessage;
    timerInstanceParametersType *pxTimerInstance;
    portBaseType xMessageHandlerResult;
    timerMessageHandlerFunctionPtrType pxCopyOfInstanceMessageHandler;

    /* Set pxTimerInstance to the parameters for this instance of the feature. */
    pxTimerInstance = ( timerInstanceParametersType * ) pvParameters;

    /* If we have not been provided valid parameters then do not start the task. */
    if( pdPASS != xTimerCheckInstance( pxTimerInstance ) )
    {
        /* MCDC Test Point: STD_IF "prvTimerTask" */

        vPortErrorHook( xTaskGetLwrrentTaskHandle(),
                        errILWALID_TIMER_TASK_INSTANCE );
    }
    /* MCDC Test Point: ADD_ELSE "prvTimerTask" */

    /* Do not need to check for switching lists as the task is created at
    feature initialisation, therefore just get time now. */
    xTimeNow = xTaskGetTickCount();

    /* At this point the value of xNextExpireTime would be zero if we called
    prvGetNextExpireTime() with no active timers; however this would result in
    a block time of 0 within prvProcessReceivedCommands(). At start-up we
    actually want to block until a command is received, therefore initialize
    to portMAX_DELAY. */
    xNextExpireTime = portMAX_DELAY / ( portTickType ) 2;

    for( ;; )
    {
        if( pdPASS == xQueueReceive( pxTimerInstance->xTimerQueueHandle, &xMessage, ( xNextExpireTime - xTimeNow ) ) )
        {
            /* We have unblocked and there is a message to process, update the
            xTimeNow variable before processing the message. */
            xTimeNow = prvSampleTimeNow( pxTimerInstance, &xTimerListsWereSwitched );

            /* If the timer instance structure indicates that there is an instance
             * specific command handler available, then call it to attempt to process
             * the message.
             */
            xMessageHandlerResult = pdFALSE;
            pxCopyOfInstanceMessageHandler = pxTimerInstance->pxInstanceMessageHandler;
            if( pxTimerInstance->uxInstanceMessageHandlerMirror == ~( ( portUnsignedBaseType ) pxCopyOfInstanceMessageHandler ) )
            {
                if( NULL != pxCopyOfInstanceMessageHandler )
                {
                    xMessageHandlerResult = pxCopyOfInstanceMessageHandler( xMessage, xTimeNow );

                    /* MCDC Test Point: STD_IF "prvTimerTask" */
                }
                /* MCDC Test Point: ADD_ELSE "prvTimerTask" */
            }
            else
            {
                /* MCDC Test Point: STD_ELSE "prvTimerTask" */

                /* If the message handler is invalid then the timer instance
                 * parameters are corrupt. */
                vPortErrorHook( xTaskGetLwrrentTaskHandle(),
                                errILWALID_TIMER_TASK_INSTANCE );
            }

            if( pdFALSE == xMessageHandlerResult )
            {
                /* If there is no instance specific command handler or the message was
                 * not processed by the handler then pass the message to the default
                 * timer message handler. */
                xMessageHandlerResult = prvProcessReceivedCommands( xMessage, xTimeNow );

                /* MCDC Test Point: STD_IF "prvTimerTask" */
            }
            /* MCDC Test Point: ADD_ELSE "prvTimerTask" */

            if( pdTRUE != xMessageHandlerResult )
            {
                /* MCDC Test Point: STD_IF "prvTimerTask" */

                /* If the message has not been handled for whatever reason then
                 * ilwoke the error hook - all messages were valid when they were
                 * added to the message queue so this is indicative of a serious
                 * error.
                 */
                vPortErrorHook( xTaskGetLwrrentTaskHandle(),
                                xMessageHandlerResult );
            }
            /* MCDC Test Point: ADD_ELSE "prvTimerTask" */
        }
        else
        {
            /* If we have unblocked and not received a message to process then
            a timer must have expired or the tick count has overflowed. Most
            of the time xTimeNow==xNextExpireTime, however update it to do the
            xTimerListsWereSwitched check. */
            xTimeNow = prvSampleTimeNow( pxTimerInstance, &xTimerListsWereSwitched );

            /* MCDC Test Point: STD_ELSE "prvTimerTask" */
        }

        /* Processing a message may have changed the next expire time, also
        if the lists were switched then the expire time will have changed.
        Query the timers list to see if it contains any timers, and if so,
        obtain the time at which the next timer will expire. */
        xNextExpireTime = prvGetNextExpireTime( pxTimerInstance, &xListWasEmpty, xTimeNow );

        /* Process any timers that have expired. */
        /* MCDC Test Point: EXP_WHILE_EXTERNAL_AND "prvTimerTask" "( pdFALSE == xListWasEmpty )" "( xNextExpireTime <= xTimeNow )" */
        while( ( pdFALSE == xListWasEmpty ) &&
                ( xNextExpireTime <= xTimeNow ) )
        {
            /* There is a timer in the list and it has expired. */
            prvProcessExpiredTimer( pxTimerInstance, xNextExpireTime, xTimeNow );

            /* Get the next expiry time. */
            xNextExpireTime = prvGetNextExpireTime( pxTimerInstance, &xListWasEmpty, xTimeNow );

            /* MCDC Test Point: EXP_WHILE_INTERNAL_AND "prvTimerTask" "( pdFALSE == xListWasEmpty )" "( xNextExpireTime <= xTimeNow )" */
        }
    }
}
/*---------------------------------------------------------------------------*/


KERNEL_FUNCTION static portTickType prvGetNextExpireTime( const timerInstanceParametersType *const pxTimerInstance,
                                                          portBaseType *pxListWasEmpty, portTickType xTimeNow )
{
    portTickType xNextExpireTime;

    /* Timers are listed in expiry time order, with the head of the list
    referencing the task that will expire first.  Obtain the time at which
    the timer with the nearest expiry time will expire.  If there are no
    active timers then just set the next expire time to 0.  That will cause
    this task to unblock when the tick count overflows, at which point the
    timer lists will be switched and the next expiry time can be
    re-assessed. */
    *pxListWasEmpty = xListIsListEmpty( pxTimerInstance->pxLwrrentTimerList );
    if( pdFALSE == *pxListWasEmpty )
    {
        xNextExpireTime = listGUARANTEED_GET_VALUE_OF_HEAD_ENTRY( pxTimerInstance->pxLwrrentTimerList );

        /* MCDC Test Point: STD_IF "prvGetNextExpireTime" */
    }
    else
    {
        /* Ensure the task unblocks when the tick count rolls over. */
        if( ( portTickType ) 0 == xTimeNow )
        {
            xNextExpireTime = portMAX_DELAY / ( portTickType ) 2;

            /* MCDC Test Point: STD_IF "prvGetNextExpireTime" */
        }
        else
        {
            xNextExpireTime = ( portTickType ) 0;

            /* MCDC Test Point: STD_ELSE "prvGetNextExpireTime" */
        }
        /* MCDC Test Point: STD_ELSE "prvGetNextExpireTime" */
    }

    return xNextExpireTime;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static void prvProcessExpiredTimer( const timerInstanceParametersType *const pxTimerInstance,
                                                    portTickType xNextExpireTime,
                                                    portTickType xTimeNow )
{
    timerControlBlockType *pxTimer;
    portTickType xTimeoutTimeInTicks;
    portBaseType xTimerInsertedInList;

    /* Get the timerControlBlockType struct pointer from pxLwrrentTimerList. */
    pxTimer = ( timerControlBlockType * ) listGET_OWNER_OF_HEAD_ENTRY( pxTimerInstance->pxLwrrentTimerList );

    /* Check that the timer structure is valid as we are about to use a
    function pointer. */
    if( pdTRUE == xTimerIsHandleValid( ( timerHandleType ) pxTimer ) )
    {
        /* Remove the timer from the list. */
        vListRemove( &( pxTimer->xTimerListItem ) );

        /* If the timer is a periodic timer then callwlate the next expiry time
         * and re-insert the timer in the list of active timers. */
        if( pdTRUE == pxTimer->xIsPeriodic )
        {
            /* Callwlate the next event time. */
            xTimeoutTimeInTicks = xNextExpireTime;

            /* Attempt to add the timer to the active list - this will only fail
             * if the timer has already expired. */
            xTimerInsertedInList = prvInsertTimerInActiveList( pxTimer,
                                                               ( xTimeoutTimeInTicks + pxTimer->xTimerPeriodInTicks ),
                                                               xTimeNow,
                                                               xTimeoutTimeInTicks );

            /* MCDC Test Point: WHILE_EXTERNAL "prvProcessExpiredTimer" "( pdTRUE == xTimerInsertedInList )" */
            while( pdTRUE == xTimerInsertedInList )
            {
                /* The timer expired before it was added to the active timer
                 * list. Process it now. */
                prvPerformTimerCallback( pxTimer );

                /* Callwlate the next event time. */
                xTimeoutTimeInTicks += pxTimer->xTimerPeriodInTicks;

                xTimerInsertedInList = prvInsertTimerInActiveList( pxTimer,
                                                                   ( xTimeoutTimeInTicks + pxTimer->xTimerPeriodInTicks ),
                                                                   xTimeNow,
                                                                   xTimeoutTimeInTicks );

                /* MCDC Test Point: WHILE_INTERNAL "prvProcessExpiredTimer" "( pdTRUE == xTimerInsertedInList )" */
            }
        }
        /* MCDC Test Point: ADD_ELSE "prvProcessExpiredTimer" */

        /* Call the timer callback. */
        prvPerformTimerCallback( pxTimer );

        /* If the timer is a one-shot timer, set it as inactive. */
        if( pdFALSE == pxTimer->xIsPeriodic )
        {
            pxTimer->xIsActive = pdFALSE;

            /* MCDC Test Point: STD_IF "prvProcessExpiredTimer" */
        }
        /* MCDC Test Point: ADD_ELSE "prvProcessExpiredTimer" */
    }
    else
    {
        /* MCDC Test Point: STD_ELSE "prvProcessExpiredTimer" */

        vPortErrorHook( xTaskGetLwrrentTaskHandle(),
                        errILWALID_TIMER_HANDLE );
    }
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portTickType prvSampleTimeNow( timerInstanceParametersType *pxTimerInstance,
                                                      portBaseType *pxTimerListsWereSwitched )
{
    portTickType xTimeNow;

    /* Suspend the scheduler as we need the time and the list switch decision
    to be coherent. Note that prvSwitchTimerLists() is potentially a very long
    operation. */
    vTaskSuspendScheduler();
    {
        xTimeNow = xTaskGetTickCount();

        if( xTimeNow < pxTimerInstance->xLastSampleTime )
        {
            prvSwitchTimerLists( pxTimerInstance );
            *pxTimerListsWereSwitched = pdTRUE;

            /* MCDC Test Point: STD_IF "prvSampleTimeNow" */
        }
        else
        {
            *pxTimerListsWereSwitched = pdFALSE;

            /* MCDC Test Point: STD_ELSE "prvSampleTimeNow" */
        }

        pxTimerInstance->xLastSampleTime = xTimeNow;
    }
    ( void ) xTaskResumeScheduler();

    return xTimeNow;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvInsertTimerInActiveList( timerControlBlockType *pxTimer,
                                                                portTickType xNextExpiryTime,
                                                                portTickType xTimeNow,
                                                                portTickType xCommandTime )
{
    portBaseType xProcessTimerNow = pdFALSE;

    listSET_LIST_ITEM_VALUE( &( pxTimer->xTimerListItem ), xNextExpiryTime );
    listSET_LIST_ITEM_OWNER( &( pxTimer->xTimerListItem ), pxTimer );

    if( xNextExpiryTime <= xTimeNow )
    {
        /* Has the expiry time elapsed between the command to start/reset a
        timer was issued, and the time the command was processed? */
        /* MCDC Test Point: EXP_IF_OR "prvInsertTimerInActiveList" "( xCommandTime <= xNextExpiryTime )" "( xTimeNow < xCommandTime )" "Deviate: TT" */
        if( ( xCommandTime <= xNextExpiryTime ) || ( xTimeNow < xCommandTime ) )
        {
            /* The time between a command being issued and the command being
            processed actually exceeds the timers period. */
            xProcessTimerNow = pdTRUE;

            /* MCDC Test Point: NULL (separate expansion) "prvInsertTimerInActiveList" */
        }
        else
        {
            /* Add the timer to the overflow timer list. */
            vListInsertOrdered( pxTimer->pxTimerInstance->pxOverflowTimerList, &( pxTimer->xTimerListItem ) );

            /* Mark the timer as being active. */
            pxTimer->xIsActive = pdTRUE;

            /* MCDC Test Point: NULL (separate expansion) "prvInsertTimerInActiveList" */
        }
    }
    else
    {
        /* MCDC Test Point: EXP_IF_AND "prvInsertTimerInActiveList" "( xTimeNow < xCommandTime )" "( xNextExpiryTime >= xCommandTime )" "Deviate: FF" */
        if( ( xTimeNow < xCommandTime ) && ( xNextExpiryTime >= xCommandTime ) )
        {
            /* If, since the command was issued, the tick count has overflowed
            but the expiry time has not, then the timer must have already passed
            its expiry time and should be processed immediately. */
            xProcessTimerNow = pdTRUE;

            /* MCDC Test Point: NULL (separate expansion) "prvInsertTimerInActiveList" */
        }
        else
        {
            /* Add the timer to the current timer list. */
            vListInsertOrdered( pxTimer->pxTimerInstance->pxLwrrentTimerList, &( pxTimer->xTimerListItem ) );

            /* Mark the timer as being active. */
            pxTimer->xIsActive = pdTRUE;

            /* MCDC Test Point: NULL (separate expansion) "prvInsertTimerInActiveList" */
        }
        /* MCDC Test Point: STD_ELSE "prvInsertTimerInActiveList" */
    }

    return xProcessTimerNow;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static portBaseType prvProcessReceivedCommands( timerQueueMessageType xMessage,
                                                                portTickType xTimeNow )
{
    timerControlBlockType *pxTimer;
    portBaseType xResult = pdTRUE; /* default to message handled. */
    portTickType xTimeoutTimeInTicks;
    portBaseType xTimerInsertedInList;

    /* Is this message requesting a kernel service? */
    if( timerCMD_EXELWTE_EVENT_GROUP_SET_BITS == xMessage.xMessageID )
    {
        /* This is a special case for event groups, where it is required to
         * execute a function from a kernel task context. */

        /* Execute the xEventGroupSetBits() function. */
        ( void ) xEventGroupSetBits( xMessage.pvMessageObject,
                                     xMessage.xMessageValue );

        /* MCDC Test Point: STD_IF "prvProcessReceivedCommands" */
    }
    else if( timerCMD_EXELWTE_EVENT_GROUP_CLEAR_BITS == xMessage.xMessageID )
    {
        /* This is a special case for event groups, where it is required to
         * execute a function from a kernel task context. */

        /* Execute the xEventGroupClearBits() function. */
        ( void ) xEventGroupClearBits( xMessage.pvMessageObject,
                                       xMessage.xMessageValue );

        /* MCDC Test Point: STD_ELSE_IF "prvProcessReceivedCommands" */
    }
    else
    {
        /* Determine which timer this message relates to. */
        pxTimer = ( timerControlBlockType * ) xMessage.pvMessageObject;

        if( pdTRUE == xTimerIsHandleValid( ( timerHandleType ) pxTimer ) )
        {
            /* Is the timer already in a list of active timers? */
            if( pdTRUE == xListIsContainedWithinAList( &( pxTimer->xTimerListItem ) ) )
            {
                /* The timer is in a list, remove it. */
                vListRemove( &( pxTimer->xTimerListItem ) );

                /* MCDC Test Point: STD_IF "prvProcessReceivedCommands" */
            }
            /* MCDC Test Point: ADD_ELSE "prvProcessReceivedCommands" */
            /* SAFERTOSTRACE TIMERPROCESSCMD */

            switch( xMessage.xMessageID )
            {
                case timerCOMMAND_START :
                    /* Start or restart a timer. */
                    xTimeoutTimeInTicks = xMessage.xMessageValue + pxTimer->xTimerPeriodInTicks;
                    if( pdTRUE == prvInsertTimerInActiveList( pxTimer, xTimeoutTimeInTicks,
                                                              xTimeNow, xMessage.xMessageValue ) )
                    {
                        /* The timer expired before it was added to the active
                         * timer list.  Process it now. */
                        prvPerformTimerCallback( pxTimer );

                        if( pdTRUE == pxTimer->xIsPeriodic )
                        {
                            xTimerInsertedInList = prvInsertTimerInActiveList( pxTimer,
                                                                               xTimeoutTimeInTicks + pxTimer->xTimerPeriodInTicks,
                                                                               xTimeNow,
                                                                               xTimeoutTimeInTicks );
                            /* MCDC Test Point: WHILE_EXTERNAL "prvProcessReceivedCommands" "( pdTRUE == xTimerInsertedInList )" */
                            while( pdTRUE == xTimerInsertedInList )
                            {
                                /* The timer expired before it was added to the
                                 * active timer list.  Process it now. */
                                prvPerformTimerCallback( pxTimer );

                                /* Callwlate the next event time. */
                                xTimeoutTimeInTicks += pxTimer->xTimerPeriodInTicks;

                                xTimerInsertedInList = prvInsertTimerInActiveList( pxTimer,
                                                                                   xTimeoutTimeInTicks + pxTimer->xTimerPeriodInTicks,
                                                                                   xTimeNow,
                                                                                   xTimeoutTimeInTicks );
                                /* MCDC Test Point: WHILE_INTERNAL "prvProcessReceivedCommands" "( pdTRUE == xTimerInsertedInList )" */
                            }
                        }
                        else
                        {
                            /* The timer is a one-shot timer, so set it as
                             * inactive. */
                            pxTimer->xIsActive = pdFALSE;

                            /* MCDC Test Point: STD_ELSE "prvProcessReceivedCommands" */
                        }
                    }
                    /* MCDC Test Point: ADD_ELSE "prvProcessReceivedCommands" */
                    break;

                case timerCOMMAND_STOP :
                    /* The timer has already been removed from the active list.
                     * Mark the timer as being inactive. */
                    pxTimer->xIsActive = pdFALSE;

                    /* MCDC Test Point: STD "prvProcessReceivedCommands" */
                    break;

                case timerCOMMAND_CHANGE_PERIOD :
                    /* Update the timer period and the corresponding mirror
                     * variable. */
                    portENTER_CRITICAL_WITHIN_API();
                    {
                        pxTimer->xTimerPeriodInTicks = xMessage.xMessageValue;
                        pxTimer->xTimerPeriodMirror = ~( xMessage.xMessageValue );
                    }
                    portEXIT_CRITICAL_WITHIN_API();

                    /* The return value is ignored as the function cannot
                     * fail to insert in a list. */
                    ( void ) prvInsertTimerInActiveList( pxTimer,
                                                         ( xTimeNow + pxTimer->xTimerPeriodInTicks ),
                                                         xTimeNow, xTimeNow );

                    /* MCDC Test Point: STD "prvProcessReceivedCommands" */
                    break;

                case timerCOMMAND_DELETE :
                    /* The timer has already been removed from the active list,
                    ilwalidate the timer structure. */
                    pxTimer->pxCallbackFunction = NULL;
                    pxTimer->pcTimerName = NULL;
                    pxTimer->xTimerPeriodInTicks = 0UL;
                    pxTimer->xIsPeriodic = pdFALSE;
                    pxTimer->xIsActive = pdFALSE;
                    pxTimer->uxCallbackMirror = 0UL;
                    pxTimer->xTimerPeriodMirror = 0UL;
                    pxTimer->pxTimerInstance = NULL;
                    pxTimer->xTimerID = 0L;
                    vListInitialiseItem( &( pxTimer->xTimerListItem ) );

                    /* MCDC Test Point: STD "prvProcessReceivedCommands" */
                    break;

                default :
                    pxTimer->xIsActive = pdFALSE;
                    xResult = pdFALSE; /* Message not handled */

                    /* MCDC Test Point: STD "prvProcessReceivedCommands" */
                    break;
            }
        }
        else
        {
            xResult = errILWALID_TIMER_HANDLE;

            /* MCDC Test Point: STD_ELSE "prvProcessReceivedCommands" */
        }

        /* MCDC Test Point: STD_ELSE "prvProcessReceivedCommands" */
    }

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION static void prvSwitchTimerLists( timerInstanceParametersType *pxTimerInstance )
{
    portTickType xNextExpireTime, xReloadTime;
    xList *pxTemp;
    timerControlBlockType *pxTimer;

    /* The tick count has overflowed. The timer lists must be switched.
     * If there are any timers still referenced from the current timer list,
     * then they must have expired and should be processed before the lists are
     * switched. */
    /* MCDC Test Point: WHILE_EXTERNAL "prvSwitchTimerLists" "( pdFALSE == xListIsListEmpty( pxTimerInstance->pxLwrrentTimerList ) )" */
    while( pdFALSE == xListIsListEmpty( pxTimerInstance->pxLwrrentTimerList ) )
    {
        /* Get the timerControlBlockType struct pointer from pxLwrrentTimerList. */
        pxTimer = ( timerControlBlockType * ) listGUARANTEED_GET_OWNER_OF_HEAD_ENTRY( pxTimerInstance->pxLwrrentTimerList );

        xNextExpireTime = listGUARANTEED_GET_VALUE_OF_HEAD_ENTRY( pxTimerInstance->pxLwrrentTimerList );

        /* Only execute the callback and restart if the timer structure is valid. */
        if( pdTRUE == xTimerIsHandleValid( ( timerHandleType ) pxTimer ) )
        {
            /* Remove the timer from the list. */
            vListRemove( &( pxTimer->xTimerListItem ) );

            /* Execute its callback, then send a command to restart the timer if
             * it is a periodic timer.  It cannot be restarted here as the lists
             * have not yet been switched. */
            prvPerformTimerCallback( pxTimer );

            if( pdTRUE == pxTimer->xIsPeriodic )
            {
                /* Callwlate the reload value of the timer. */
                xReloadTime = ( xNextExpireTime + pxTimer->xTimerPeriodInTicks );

                /* Insert the timer in either the current or overflow lists
                 * using a call to prvInsertTimerInActiveLists().
                 * Note that using xNextExpireTime as both the command time and
                 * current time ensures that the timer will always be inserted
                 * in a list and therefore the return value can be ignored. */
                ( void ) prvInsertTimerInActiveList( pxTimer, xReloadTime, xNextExpireTime, xNextExpireTime );

                /* MCDC Test Point: STD_IF "prvSwitchTimerLists" */
            }
            else
            {
                /* The timer is a one-shot timer, so set it as inactive. */
                pxTimer->xIsActive = pdFALSE;

                /* MCDC Test Point: STD_ELSE "prvSwitchTimerLists" */
            }
        }
        else
        {
            /* MCDC Test Point: STD_ELSE "prvSwitchTimerLists" */

            vPortErrorHook( xTaskGetLwrrentTaskHandle(),
                            errILWALID_TIMER_HANDLE );
        }
        /* MCDC Test Point: WHILE_INTERNAL "prvSwitchTimerLists" "( pdFALSE == xListIsListEmpty( pxTimerInstance->pxLwrrentTimerList ) )" */
    }

    /* Swap the lists over. */
    pxTemp = pxTimerInstance->pxLwrrentTimerList;
    pxTimerInstance->pxLwrrentTimerList = pxTimerInstance->pxOverflowTimerList;
    pxTimerInstance->pxOverflowTimerList = pxTemp;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerCheckInstance( const timerInstanceParametersType *const pxTimerInstance )
{
    portBaseType xReturn = pdPASS;
    timerMessageHandlerFunctionPtrType pxCopyOfInstanceMessageHandler;

    if( NULL == pxTimerInstance )
    {
        xReturn = pdFAIL;

        /* MCDC Test Point: STD_IF "xTimerCheckInstance" */
    }
    else
    {
        portENTER_CRITICAL_WITHIN_API();
        {
            /* A valid timer instance will always have
             * (pxLwrrentTimerList==xActiveTimerList1 AND pxOverflowTimerList==xActiveTimerList2) OR
             * (pxLwrrentTimerList==xActiveTimerList2 AND pxOverflowTimerList==xActiveTimerList1)
             *
             * This can be done in one big if statement, however for readability...
             */
            if( pxTimerInstance->pxLwrrentTimerList == &( pxTimerInstance->xActiveTimerList1 ) )
            {
                if( pxTimerInstance->pxOverflowTimerList != &( pxTimerInstance->xActiveTimerList2 ) )
                {
                    xReturn = pdFAIL;

                    /* MCDC Test Point: STD_IF "xTimerCheckInstance" */
                }
                /* MCDC Test Point: ADD_ELSE "xTimerCheckInstance" */
            }
            else if( pxTimerInstance->pxOverflowTimerList == &( pxTimerInstance->xActiveTimerList1 ) )
            {
                if( pxTimerInstance->pxLwrrentTimerList != & ( pxTimerInstance->xActiveTimerList2 ) )
                {
                    xReturn = pdFAIL;

                    /* MCDC Test Point: STD_IF "xTimerCheckInstance" */
                }
                /* MCDC Test Point: ADD_ELSE "xTimerCheckInstance" */
                /* MCDC Test Point: STD_ELSE_IF "xTimerCheckInstance" */
            }
            else /* xActiveTimerList1 is not in either of the pointers... */
            {
                xReturn = pdFAIL;

                /* MCDC Test Point: STD_ELSE "xTimerCheckInstance" */
            }

            if( pdPASS == xReturn )
            {
                /* OK so far, now check the mirror variables. */
                pxCopyOfInstanceMessageHandler = pxTimerInstance->pxInstanceMessageHandler;
                if( pxTimerInstance->uxInstanceMessageHandlerMirror != ~( ( portUnsignedBaseType ) pxCopyOfInstanceMessageHandler ) )
                {
                    xReturn = pdFAIL;

                    /* MCDC Test Point: STD_IF "xTimerCheckInstance" */
                }
                else if( pxTimerInstance->uxTimerQueueHandleMirror != ~( ( portUnsignedBaseType ) pxTimerInstance->xTimerQueueHandle ) )
                {
                    xReturn = pdFAIL;

                    /* MCDC Test Point: STD_ELSE_IF "xTimerCheckInstance" */
                }
                else
                {
                    /* Deliberately blank. */
                    /* MCDC Test Point: STD_ELSE "xTimerCheckInstance" */
                }
            }
            /* MCDC Test Point: ADD_ELSE "xTimerCheckInstance" */
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xTimerCheckInstance" */
    }
    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerPendEventGroupSetBitsFromISR( void *pvEventGroup,
                                                                 portTickType xBitsToManipulate,
                                                                 portBaseType *pxHigherPriorityTaskWoken )
{
    timerQueueMessageType xMessage;
    portBaseType xReturn;

    /* Send a message to the timer service task to execute the
     * xEventGroupSetBits() callback function. */
    xMessage.xMessageID = timerCMD_EXELWTE_EVENT_GROUP_SET_BITS;
    xMessage.pvMessageObject = pvEventGroup;
    xMessage.xMessageValue = xBitsToManipulate;

    xReturn = xQueueSendFromISR( xTimerDefaultTimerInstance.xTimerQueueHandle,
                                 &xMessage, pxHigherPriorityTaskWoken );

    /* MCDC Test Point: STD "xTimerPendEventGroupSetBitsFromISR" */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xTimerPendEventGroupClearBitsFromISR( void *pvEventGroup,
                                                                   portTickType xBitsToManipulate,
                                                                   portBaseType *pxHigherPriorityTaskWoken )
{
    timerQueueMessageType xMessage;
    portBaseType xReturn;

    /* Send a message to the timer service task to execute the
     * xEventGroupClearBits() callback function. */
    xMessage.xMessageID = timerCMD_EXELWTE_EVENT_GROUP_CLEAR_BITS;
    xMessage.pvMessageObject = pvEventGroup;
    xMessage.xMessageValue = xBitsToManipulate;

    xReturn = xQueueSendFromISR( xTimerDefaultTimerInstance.xTimerQueueHandle,
                                 &xMessage, pxHigherPriorityTaskWoken );

    /* MCDC Test Point: STD "xTimerPendEventGroupClearBitsFromISR" */

    return xReturn;
}

/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void *pvTimerTLSObjectGet( timerHandleType xTimer )
{
    void *pvReturn;
    timerControlBlockType *pxTimer = ( timerControlBlockType * ) xTimer;

    if( NULL == pxTimer )
    {
        pvReturn = NULL;

        /* MCDC Test Point: STD_IF "pvTimerTLSObjectGet" */
    }
    else
    {
        pvReturn = pxTimer->pvObject;

        /* MCDC Test Point: STD_ELSE "pvTimerTLSObjectGet" */
    }

    return pvReturn;
}

/*-----------------------------------------------------------------------------
 * Private Function Definitions
 *---------------------------------------------------------------------------*/

KERNEL_UNPRIV_FUNCTION static portNO_INLINE_FUNC_DEF void prvPerformTimerCallback( timerControlBlockType *pxTimer )
{
    /* SAFERTOSTRACE TIMERCALLBACK */

    /* Call the timer callback. */
    pxTimer->pxCallbackFunction( ( timerHandleType ) pxTimer );

    /* Restore the required operating privileges of this task. */
    vPortRestoreKernelTaskPrivileges();

    /* MCDC Test Point: STD "prvPerformTimerCallback" */
}

/*-----------------------------------------------------------------------------
 * Weak stub Function Definitions
 *---------------------------------------------------------------------------*/

/* If event groups are not included then this stub function is required. */
portWEAK_FUNCTION KERNEL_FUNCTION portBaseType xEventGroupSetBits( eventGroupHandleType xEventGroupHandle,
                                                                   const eventBitsType xBitsToSet )
{
    /* MCDC Test Point: STD "WEAK_xEventGroupSetBits" */

    ( void ) xEventGroupHandle;
    ( void ) xBitsToSet;
    return pdFAIL;
}
/*---------------------------------------------------------------------------*/

/* If event groups are not included then this stub function is required. */
portWEAK_FUNCTION KERNEL_FUNCTION portBaseType xEventGroupClearBits( eventGroupHandleType xEventGroupHandle,
                                                                     const eventBitsType xBitsToClear )
{
    /* MCDC Test Point: STD "WEAK_xEventGroupClearBits" */

    ( void ) xEventGroupHandle;
    ( void ) xBitsToClear;
    return pdFAIL;
}
/*---------------------------------------------------------------------------*/

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_timersc.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "timersTestHeaders.h"
    #include "timersTest.h"
#endif
