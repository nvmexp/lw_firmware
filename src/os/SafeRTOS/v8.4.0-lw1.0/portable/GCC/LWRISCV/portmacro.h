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

    http://www.HighIntegritySystems.com
*/

#ifndef PORTMACRO_H
#define PORTMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <string.h>
#include <lwmisc.h>

#include "SafeRTOSConfig.h"
#include "portSyscall.h"
#include "portcommon.h"

/*-----------------------------------------------------------------------------
 * Portable Layer Version Number
 *---------------------------------------------------------------------------*/
/* Portable layer major version number */
#define portPORT_MAJOR_VERSION    ( 1 )

/* Portable layer minor version number */
#define portPORT_MINOR_VERSION    ( 10 )


/*-----------------------------------------------------------------------------
 * Type Definitions
 *---------------------------------------------------------------------------*/
typedef char                    portCharType;

typedef signed char             portInt8Type;
typedef signed short            portInt16Type;
typedef signed int              portInt32Type;
typedef signed long long        portInt64Type;

typedef unsigned char           portUInt8Type;
typedef unsigned short          portUInt16Type;
typedef unsigned int            portUInt32Type;
typedef unsigned long long      portUInt64Type;

typedef float                   portFloat32Type;
typedef double                  portFloat64Type;

typedef unsigned long           portStackType;
typedef unsigned long           portUIntPtrType;

typedef signed long             portBaseType;
typedef unsigned long           portUnsignedBaseType;
typedef unsigned long           portTickType;
typedef void *                  portTaskHandleType;

/*-----------------------------------------------------------------------------
 * Architecture specifics
 *---------------------------------------------------------------------------*/
#define portMAX_DELAY                   ( ( portTickType ) 0xFFFFFFFFFFFFFFFFULL )
#define portMAX_LIST_ITEM_VALUE         ( ( portTickType ) 0xFFFFFFFFFFFFFFFFULL )

/* From spec: In the standard RISC-V calling convention, the stack grows
 * downward and the stack pointer isalways kept 16-byte aligned. */
#define portWORD_ALIGNMENT                ( 8U )
#define portWORD_ALIGNMENT_MASK           ( ( portUnsignedBaseType ) ( portWORD_ALIGNMENT - 1U ) )
#define portSTACK_ALIGNMENT               ( 16U )
#define portSTACK_ALIGNMENT_MASK          ( ( portUnsignedBaseType ) ( portSTACK_ALIGNMENT - 1U ) )

/* SAFERTOSTRACE PORTQUEUEOVERHEADBYTES */
#define portQUEUE_OVERHEAD_BYTES        ( 280U )
/* SAFERTOSTRACE PORTQUEUEOVERHEADBYTESENDIF */

#define portSTACK_IN_USE                ( 0xA5A5A5A5A5A5A5A5ULL )
#define portSTACK_NOT_IN_USE            ( 0x5A5A5A5A5A5A5A5AULL )

#define portTICK_COUNT_NUM_BITS         ( sizeof( portTickType ) * 8U )

/* Allow for weak stub functions. */
#define portWEAK_FUNCTION               __attribute__( ( weak ) )

/* MCDC Test Point: PROTOTYPE */

/*-----------------------------------------------------------------------------
 * Scheduler utilities.
 *---------------------------------------------------------------------------*/

#define portNOP()                       do { } while( pdFALSE )

/* portYIELD_IMMEDIATE() is used to request context switch during epilog of
 * trap handler. */
#define portYIELD_IMMEDIATE()                                           \
do {                                                                    \
extern volatile LwBool IcCtxSwitchReq;                                  \
                                                                        \
    IcCtxSwitchReq = LW_TRUE;                                           \
} while(0)

/* Yielding from an ISR is only performed if the input argument is TRUE. */
#define portYIELD_FROM_ISR( xSwitchRequired )                \
do {                                                         \
    if( pdFALSE != ( portBaseType ) ( xSwitchRequired ) )    \
    {                                                        \
        portYIELD_IMMEDIATE();                               \
    }                                                        \
} while(0)

#define portYIELD()                                   syscall0( LW_SYSCALL_YIELD )
#define portYIELD_WITHIN_API()                        portYIELD()

void vTaskExit( void );

/*-----------------------------------------------------------------------------
 * Critical section management.
 *---------------------------------------------------------------------------*/

portUnsignedBaseType xPortGetExceptionNesting(void);

/* SafeRTOS is not expected to ever need to enter critical sections.
 * All SafeRTOS code is only called in kernel-mode or in
 * priv-raised-interrupt-disabled task code. */
#define portENTER_CRITICAL()                portNOP()
#define portEXIT_CRITICAL()                 portNOP()
#define portENTER_CRITICAL_WITHIN_API()     portNOP()
#define portEXIT_CRITICAL_WITHIN_API()      portNOP()

#define portSET_INTERRUPT_MASK()            portNOP()
/* Interrupts are *always* disabled in ISR (for RISC-V) */
#define portSET_INTERRUPT_MASK_FROM_ISR()   ( 0U )
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedStatusValue ) \
    ( (void)(uxSavedStatusValue) )

/*---------------------------------------------------------------------------*/

/* These macros are defined away because portTickType is atomic. */
#define portTICK_TYPE_ENTER_CRITICAL()                                        portNOP()
#define portTICK_TYPE_EXIT_CRITICAL()                                         portNOP()
#define portTICK_TYPE_SET_INTERRUPT_MASK_FROM_ISR()                           ( 0U )
#define portTICK_TYPE_CLEAR_INTERRUPT_MASK_FROM_ISR( uxSavedInterruptStatus ) ( ( void ) ( uxSavedInterruptStatus ) )
#define portTASK_HANDLE_ENTER_CRITICAL()
#define portTASK_HANDLE_EXIT_CRITICAL()


/*-----------------------------------------------------------------------------
 * Memcpy wrappers
 *---------------------------------------------------------------------------*/


/* vPortSetWordAlignedBuffer() is used for zeroing out memory blocks.
 * We know the last parameter is a constant (sizeof) and the middle parameter is 0. */
void vPortSetWordAlignedBuffer( void *pvDestination, portBaseType xValue, portUnsignedBaseType uxLength );
void vPortProcessSystemTick( void );

/* Copy data to/from a task functions that check if the scheduler has been
 * started before ilwoking the user application provided functions with the
 * appropriate parameters. */
portBaseType xPortCopyDataToTask( void *pvApplicationDestinationAddress,
                                  const void *pvKernelSourceAddress,
                                  portUnsignedBaseType uxLength );
portBaseType xPortCopyDataFromTask( void *pvKernelDestinationAddress,
                                    const void *pvApplicationSourceAddress,
                                    portUnsignedBaseType uxLength );

/* Application defined functions that perform any necessary checks before
 * performing the copy operation. If the copy operation is successfully
 * performed, these functions should return pdPASS; otherwise pdFAIL should be
 * returned. */
portBaseType xApplicationCopyDataToTask( portTaskHandleType xLwrrentTaskHandle,
                                         void *pvApplicationDestinationAddress,
                                         const void *pvKernelSourceAddress,
                                         portUnsignedBaseType uxLength );
portBaseType xApplicationCopyDataFromTask( portTaskHandleType xLwrrentTaskHandle,
                                           void *pvKernelDestinationAddress,
                                           const void *pvApplicationSourceAddress,
                                           portUnsignedBaseType uxLength );
portBaseType xApplicationCopyDataToISR( void *pvApplicationDestinationAddress,
                                        const void *pvKernelSourceAddress,
                                        portUnsignedBaseType uxLength );
portBaseType xApplicationCopyDataFromISR( void *pvKernelDestinationAddress,
                                          const void *pvApplicationSourceAddress,
                                          portUnsignedBaseType uxLength );

#define portCOPY_DATA_TO_TASK   xPortCopyDataToTask
#define portCOPY_DATA_FROM_TASK xPortCopyDataFromTask
#define portCOPY_DATA_TO_ISR    xPortCopyDataToTask
#define portCOPY_DATA_FROM_ISR  xPortCopyDataFromTask

/* MCDC Test Point: PROTOTYPE */

/*-----------------------------------------------------------------------------
 * Debug utilities.
 *---------------------------------------------------------------------------*/

__attribute__((noreturn)) void vPortInfiniteLoop( void );

/*---------------------------------------------------------------------------*/

#ifndef SAFE_RTOS_BUILD
#error This header file must not be included outside SafeRTOS build.
#endif
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* PORTMACRO_H */
