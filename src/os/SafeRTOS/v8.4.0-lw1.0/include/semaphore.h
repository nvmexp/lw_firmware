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


#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
 * MACROS AND DEFINITIONS
 *---------------------------------------------------------------------------*/

typedef void *xSemaphoreHandle;

/* do not block when giving semaphores */
#define semSEM_GIVE_BLOCK_TIME              ( ( portTickType ) 0 )

/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/* The following macro's are part of the semaphore API however they can be
   mapped directly to the equivalent queue functions and hence are implemented
   as macro's for efficiency purposes. */

#define xSemaphoreTake( xSemaphore, xBlockTime )                        xQueueReceive( ( ( xQueueHandle ) ( xSemaphore ) ), NULL, ( xBlockTime ) )
#define xSemaphoreGive( xSemaphore )                                    xQueueSend( ( ( xQueueHandle ) ( xSemaphore ) ), NULL, semSEM_GIVE_BLOCK_TIME )
#define xSemaphoreGiveFromISR( xSemaphore, pxHigherPriorityTaskWoken )  xQueueSendFromISR( ( ( xQueueHandle ) ( xSemaphore ) ), NULL, ( pxHigherPriorityTaskWoken ) )
#define xSemaphoreTakeFromISR( xSemaphore, pxHigherPriorityTaskWoken )  xQueueReceiveFromISR( ( ( xQueueHandle ) ( xSemaphore ) ), NULL, ( pxHigherPriorityTaskWoken ) )

#define xSemaphoreGetCountDepth( xSemaphore, puxCountDepth )            xQueueMessagesWaiting( ( ( xQueueHandle ) ( xSemaphore ) ), ( puxCountDepth ) )

KERNEL_CREATE_FUNCTION portBaseType xSemaphoreCreateBinary( portInt8Type *pcSemaphoreBuffer, xSemaphoreHandle *pxSemaphore );
KERNEL_CREATE_FUNCTION portBaseType xSemaphoreCreateCounting( portUnsignedBaseType uxMaxCount, portUnsignedBaseType uxInitialCount, portInt8Type *pcSemaphoreBuffer, xSemaphoreHandle *pxSemaphore );


#ifdef __cplusplus
}
#endif

#endif /* SEMAPHORE_H */


