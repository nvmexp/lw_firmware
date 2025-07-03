/*
    Copyright (C)2006 onward WITTENSTEIN aerospace & simulation limited. All rights reserved.

    This file is part of the SafeRTOS product, see projdefs.h for version number information.

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

#ifndef RELWRSIVE_MUTEX_H
#define RELWRSIVE_MUTEX_H

#ifdef __cplusplus
extern "C" {
#endif

/*-----------------------------------------------------------------------------
 * MACROS AND DEFINITIONS
 *---------------------------------------------------------------------------*/

typedef void *xMutexHandleType;

/*-----------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

/* Macros to assist in turning the result from xQueueMessagesWaiting() into
 * something meaningful to mutexes. */
#define mutexTAKEN                                              ( 0 )   /* the underlying queue is empty */
#define mutexAVAILABLE                                          ( 1 )   /* the underlying queue is full */

/* The following macro is part of the Mutex API, however it can be mapped
 * directly to the equivalent queue functions and hence are implemented as
 * macro's for efficiency purposes. */
#define xMutexGetState( xMutex, puxMutexState )                 xQueueMessagesWaiting(  ( xQueueHandle ) ( xMutex ), ( puxMutexState ) )


KERNEL_CREATE_FUNCTION portBaseType xMutexCreate( portInt8Type *pcMutexBuffer, xMutexHandleType *pxMutex );
KERNEL_FUNCTION portBaseType xMutexTake( xMutexHandleType xMutex, portTickType xTicksToWait );
KERNEL_FUNCTION portBaseType xMutexGive( xMutexHandleType xMutex );


/*-----------------------------------------------------------------------------
 * Private API
 *---------------------------------------------------------------------------*/
/* Functions beyond this part are not part of the public API and are intended
for use by the kernel only. */

KERNEL_FUNCTION void vMutexInitHeldList( xTCB *pxTCB );
KERNEL_FUNCTION void vMutexInitQueueVariables(xQUEUE *pxQueue );

KERNEL_FUNCTION void vMutexReleaseAll( xTCB *pxTCB );

KERNEL_FUNCTION void vMutexPriorityInherit( const xQUEUE *pxQueue );
KERNEL_FUNCTION portBaseType xMutexPriorityDisinherit( xQUEUE *pxQueue );
KERNEL_FUNCTION void vMutexReEvaluateInheritedPriority( const xQUEUE *const pxQueue );
KERNEL_FUNCTION portUnsignedBaseType uxMutexFindHighestBlockedTaskPriority( xTCB *pxTCB );
KERNEL_FUNCTION void vMutexLogMutexTaken( xQUEUE *pxQueue );

#ifdef __cplusplus
}
#endif

#endif /* RELWRSIVE_MUTEX_H */

