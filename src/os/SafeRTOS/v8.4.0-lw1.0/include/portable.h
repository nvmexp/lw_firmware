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

/*-----------------------------------------------------------------------------
 * Portable layer API.  Each function must be defined for each port.
 *---------------------------------------------------------------------------*/

#ifndef PORTABLE_H
#define PORTABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "portmacro.h"
#include "portfeatures.h"


/*
 * xPortInitialise is used in the ROMable versions to allow applications to set
 * the address of callback functions, hardware dependent parameters, etc.  This
 * function is called by xTaskInitializeScheduler() and should not be called
 * directly by application code.
 */
KERNEL_INIT_FUNCTION portBaseType xPortInitialize( const xPORT_INIT_PARAMETERS *const pxInitParameters );

/*
 * Examine the port specific parameters used to create a task to ensure that
 * they are valid. If they are not, an error code is returned.
 */
KERNEL_CREATE_FUNCTION portBaseType xPortCheckTaskParameters( const xTaskParameters *const pxTaskParameters );

/*
 * Setup the TCB and stack of a new task so it is ready to be placed under the
 * scheduler control.  The registers have to be placed on the stack in
 * the order that the port expects to find them.
 */
KERNEL_CREATE_FUNCTION void vPortInitialiseTask( const xTaskParameters *const pxTaskParameters );

/*
 * Setup the hardware ready for the scheduler to take control.  This generally
 * sets up a tick interrupt and sets timers for the correct tick frequency.
 *
 * The scheduler will only be started if a set of pre-conditions are met.
 * These pre-conditions can be ignored by setting to
 * xUseKernelConfigurationChecks to pdFALSE - in which case the scheduler will
 * be started regardless of its state.  This can be useful in ROMed versions
 * if any of the ROM code is being replaced by a FLASH equivalent.
 */
KERNEL_INIT_FUNCTION portBaseType xPortStartScheduler( portBaseType xUseKernelConfigurationChecks );

/* THIS FUNCTION MUST ONLY BE CALLED FROM WITHIN A CRITICAL SECTION.
 * Inspects the TCB associated with the supplied task handle and determines if
 * its contents are valid.
 */
KERNEL_FUNCTION portBaseType xPortIsTaskHandleValid( portTaskHandleType xTaskToCheck );

/* Port specific helper function that will add any required port specific
 * declarations to a kernel task parameter structure. */
KERNEL_INIT_FUNCTION void vPortAddKernelTaskParams( xTaskParameters *pxKernelTaskParams );

/* Port specific helper function that will restore a set of default privileges
 * and required MPU settings for a kernel task. */
void vPortRestoreKernelTaskPrivileges( void );

/* Port specific helper function that will permanently lower the privilege
 * level of a task. */
void vPortLowerTaskBasePrivilege( void );

KERNEL_FUNCTION void vPortErrorHook( portTaskHandleType xHandleOfTaskWithError, portBaseType xErrorCode );
KERNEL_DELETE_FUNCTION void vPortTaskDeleteHook( portTaskHandleType xTaskBeingDeleted );
KERNEL_FUNCTION void vPortTickHook( void );

/*-----------------------------------------------------------------------------
 * Hook Functions
 *---------------------------------------------------------------------------*/

void vApplicationErrorHook( portTaskHandleType xHandleOfTaskWithError,
                            portBaseType xErrorCode );

void vApplicationTaskDeleteHook( portTaskHandleType xTaskBeingDeleted );

void vApplicationIdleHook( void );

void vApplicationTickHook( void );

void vApplicationSetupTickInterruptHook( portUInt32Type ulClockHz, portUInt32Type ulRateHz );

/* The Idle Hook may be exelwted in unprivileged mode. */
void vPortIdleHook( void );


#ifdef __cplusplus
}
#endif

#endif /* PORTABLE_H */
