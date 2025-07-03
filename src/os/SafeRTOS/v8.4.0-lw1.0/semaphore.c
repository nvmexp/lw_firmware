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

#define KERNEL_SOURCE_FILE

#include "SafeRTOS.h"
#include "queue.h"
#include "semaphore.h"

/*---------------------------------------------------------------------------*/
/* Constant Definitions.                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Macros                                                                    */
/*---------------------------------------------------------------------------*/

/* A binary semaphore is a null queue of length 1. */
#define semBINARY_SEMAPHORE_QUEUE_LENGTH    ( ( portUnsignedBaseType ) 1 )
#define semSEMAPHORE_QUEUE_ITEM_LENGTH      ( ( portUnsignedBaseType ) 0 )
#define semINITIAL_BINARY_COUNT             ( ( portUnsignedBaseType ) 1 )

/*---------------------------------------------------------------------------*/
/* Type Definitions.                                                         */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Variables.                                                                */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Function prototypes.                                                      */
/*---------------------------------------------------------------------------*/
KERNEL_CREATE_FUNCTION static void prvSetSemaphoreInitialCount( xQueueHandle xQueue, portUnsignedBaseType uxInitialCount );

/* MCDC Test Point: PROTOTYPE */

/*---------------------------------------------------------------------------*/
/* External declarations                                                     */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Public API functions                                                      */
/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xSemaphoreCreateBinary( portInt8Type *pcSemaphoreBuffer, xSemaphoreHandle *pxSemaphore )
{
    portBaseType xReturn;

    /* A critical section is used to ensure that the semaphore cannot be used
    until it is created and placed in the correct initial state. */
    portENTER_CRITICAL_WITHIN_API();
    {
        /* A binary semaphore is just a queue with a length of 1 and zero
        item size. */
        xReturn = xQueueCreate( pcSemaphoreBuffer,
                                portQUEUE_OVERHEAD_BYTES,
                                semBINARY_SEMAPHORE_QUEUE_LENGTH,
                                semSEMAPHORE_QUEUE_ITEM_LENGTH,
                                pxSemaphore );

        if( pdPASS == xReturn )
        {
            /* Start with the item in the given state. This could be achieved by
            calling xSemaphoreGive, however as we are in a critical section and
            do not want to allow the semaphore to block, it is more efficient to
            set the initial count directly. */
            prvSetSemaphoreInitialCount( *pxSemaphore, semINITIAL_BINARY_COUNT );

            /* MCDC Test Point: STD_IF "xSemaphoreCreateBinary" */
        }
        /* MCDC Test Point: ADD_ELSE "xSemaphoreCreateBinary" */

    }
    portEXIT_CRITICAL_WITHIN_API();

    /* SAFERTOSTRACE SEMAPHORECREATEBINARY */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xSemaphoreCreateCounting( portUnsignedBaseType uxMaxCount, portUnsignedBaseType uxInitialCount, portInt8Type *pcSemaphoreBuffer, xSemaphoreHandle *pxSemaphore )
{
    portBaseType xReturn;

    /* Check that the supplied count values are not invalid. */
    if( uxInitialCount > uxMaxCount )
    {
        xReturn = errILWALID_INITIAL_SEMAPHORE_COUNT;

        /* MCDC Test Point: STD_IF "xSemaphoreCreateCounting" */
    }
    else
    {
        /* A critical section is used to ensure that the semaphore cannot be used
        until it is created and placed in the correct initial state. */
        portENTER_CRITICAL_WITHIN_API();
        {
            /* A counting semaphore is just a queue with a size given by uxMaxCount
            and zero item size. */
            xReturn = xQueueCreate( pcSemaphoreBuffer,
                                    portQUEUE_OVERHEAD_BYTES,
                                    uxMaxCount,
                                    semSEMAPHORE_QUEUE_ITEM_LENGTH,
                                    pxSemaphore );

            if( pdPASS == xReturn )
            {
                /* Set the initial count of the counting semaphore. This could be
                achieved by calling xSemaphoreGive multiple times, however this is
                inefficient for large counts so call an internal queue function to
                initialise the messages waiting member of the queue structure. */
                prvSetSemaphoreInitialCount( *pxSemaphore, uxInitialCount );

                /* MCDC Test Point: STD_IF "xSemaphoreCreateCounting" */
            }
            /* MCDC Test Point: ADD_ELSE "xSemaphoreCreateCounting" */
        }
        portEXIT_CRITICAL_WITHIN_API();
        /* MCDC Test Point: STD_ELSE "xSemaphoreCreateCounting" */
    }

    /* SAFERTOSTRACE SEMAPHORECREATECOUNTING */

    return xReturn;
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* File private functions                                                    */
/*---------------------------------------------------------------------------*/
KERNEL_CREATE_FUNCTION static void prvSetSemaphoreInitialCount( xQueueHandle xQueue, portUnsignedBaseType uxInitialCount )
{
    /* This function is only called from the semaphore creation routines if
    the queue handle is valid. As this is within a critical section no further
    parameter checking is performed here and no status is returned. This
    routine is provided here as we need access to the internal queue structure
    to set the initial count. */
    /* Cast param 'xQueueHandle xQueue' to xQUEUE* type. */
    xQUEUE *pxQueue = ( xQUEUE * ) xQueue;

    pxQueue->uxItemsWaiting = uxInitialCount;

    /* SAFERTOSTRACE QUEUESEMAPHORECREATE */

    /* MCDC Test Point: STD "prvSetSemaphoreInitialCount" */
}
/*---------------------------------------------------------------------------*/

#ifdef SAFERTOS_TEST
    #include "SectionLocationCheckList_semaphorec.h"
#endif

#ifdef SAFERTOS_MODULE_TEST
    #include "SemaphoreCTestHeaders.h"
    #include "SemaphoreCTest.h"
#endif /* SAFERTOS_MODULE_TEST */
