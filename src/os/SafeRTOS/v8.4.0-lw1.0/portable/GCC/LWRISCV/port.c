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

#define KERNEL_SOURCE_FILE

/* Scheduler Includes */
#include "SafeRTOS.h"

#include "portfeatures.h"
#include "task.h"
#include "queue.h"
#include "mutex.h"
#include "eventgroups.h"
#include "eventpoll.h"
#include "lwrtosHooks.h"
#include "riscv_csr.h"
#include "sbilib/sbilib.h"
#include "sections.h"
#include "portSyscall.h"
#include "portcommon.h"

/*-----------------------------------------------------------------------------
 * External Functions.
 *---------------------------------------------------------------------------*/
KERNEL_FUNCTION void vPortTrapHandler( void );

/*-----------------------------------------------------------------------------
 * Local Prototypes
 *---------------------------------------------------------------------------*/

/* Configures the TCB within the host application provided buffer. */
KERNEL_CREATE_FUNCTION static xTCB *prvSetupTCB( const xTaskParameters * const pxTaskParameters );

/* MCDC Test Point: PROTOTYPE */

/*-----------------------------------------------------------------------------
 * Variables.
 *---------------------------------------------------------------------------*/

/* End of stack - should be defined by linker or application */
KERNEL_DATA extern portStackType __stack_end[];

/* This variable represents on how deep we're in exception nesting
 *
 * 0 == we're running in user mode (but we may call directly into kernel)
 * 1 == we're running in kernel mode, but came from user
 * 2 == we're running in kernel mode and came from kernel
 * 3 == ...
 *
 * This number also reflects number of ERET to go back to user mode.
 * Nesting stops once we run out of kernel stack or hit double error
 * (fault while handling fault or saving/restoring context).
 * In that case RTOS enters infinite loop. */
KERNEL_DATA portStackType xPortExceptionNesting = 1; // we start at kernel mode

KERNEL_DATA portStackType *pxPortKernelStackTop = __stack_end;

/* This is the value of the timer tick period in units used by the timer compare
 * register (mtimecmp) on RISC-V to properly program next timer interrupt.
 * Call to lwrtosHookGetTimerPeriod() overrides the default setting. */
KERNEL_DATA static portUnsignedBaseType uxPortSysclkMtimePeriod = ( 1000000000ULL >> LWRISCV_MTIME_TICK_SHIFT ) / LWRTOS_TICK_RATE_HZ;

/* This caches the value that was programmed into the RISC-V mtimecmp register.
 * It is used to re-schedule next timer tick without aliasing/drifts. */
KERNEL_DATA static portUnsignedBaseType uxPortSysMtimeCmp = ~0;

/*---------------------------------------------------------------------------*/

ct_assert( LW_OFFSETOF( xTCB, xCtx.uxPc )                  == ( TCB_OFFSET_PC        * sizeof ( portUnsignedBaseType ) ) );
ct_assert( LW_OFFSETOF( xTCB, xCtx.uxSstatus )             == ( TCB_OFFSET_SSTATUS   * sizeof ( portUnsignedBaseType ) ) );
ct_assert( LW_OFFSETOF( xTCB, xCtx.uxFlags )               == ( TCB_OFFSET_FLAGS     * sizeof ( portUnsignedBaseType ) ) );
ct_assert( LW_OFFSETOF( xTCB, xCtx.uxSie)                  == ( TCB_OFFSET_SIE       * sizeof ( portUnsignedBaseType ) ) );
ct_assert( sizeof( xPortTaskContext )                      == ( TCB_CTX_SIZE_WORDS   * sizeof ( portUnsignedBaseType ) ) );
ct_assert( LW_OFFSETOF( xTCB, uxTopOfStackMirror )         == ( TCB_OFFSET_SP_MIRROR * sizeof ( portUnsignedBaseType ) ) );
/*---------------------------------------------------------------------------*/

sysSHARED_CODE void vTaskExitError( void ) __attribute__( ( noreturn ) );
sysSHARED_CODE void vTaskExitError( void )
{
    /* Notify OS that we died. */
    syscall0( LW_SYSCALL_TASK_EXIT );
    __builtin_unreachable();
}
/*---------------------------------------------------------------------------*/

KERNEL_INIT_FUNCTION portBaseType xPortInitialize( const xPORT_INIT_PARAMETERS * const pxInitParameters )
{
    return pdPASS;
}
/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION portBaseType xPortCheckTaskParameters( const xTaskParameters * const pxTaskParameters )
{
portBaseType xReturn = pdPASS;

    /* RISC-V Stack must be 16-byte aligned. */
    if( ( portUIntPtrType ) pxTaskParameters->pcStackBuffer & 0xF )
    {
        xReturn = errILWALID_ALIGNMENT;
    }
    else if( pxTaskParameters->uxStackDepthBytes & 0xF )
    {
        xReturn = errILWALID_ALIGNMENT;
    }

    return xReturn;
}

/*---------------------------------------------------------------------------*/

KERNEL_CREATE_FUNCTION void vPortInitialiseTask( const xTaskParameters * const pxTaskParameters )
{
xTCB *pxTCB = pxTaskParameters->pxTCB;
xPortTaskContext *pxCtx = &pxTCB->xCtx;
portInt8Type *pcStackEnd = pxTaskParameters->pcStackBuffer + pxTaskParameters->uxStackDepthBytes;
portStackType *pxSpTop = ( portStackType * ) pcStackEnd;

    vPortSetWordAlignedBuffer( pxTaskParameters->pcStackBuffer, 0U,
                               pxTaskParameters->uxStackDepthBytes );
    /* Clear registers */
    vPortSetWordAlignedBuffer( pxCtx->uxGpr, 0, sizeof( pxCtx->uxGpr ) );

    prvSetupTCB( pxTaskParameters );

    /* Initialize bottom of stack */
    pxSpTop = ( portStackType * ) pxTCB->xCtx.uxGpr[LWRISCV_GPR_SP];
    --pxSpTop;
    --pxSpTop;

    pxSpTop = ( portStackType * )( ( ( portBaseType )pxSpTop ) & ~portSTACK_ALIGNMENT_MASK);

    /* Setup (remaining) registers */
    pxCtx->uxGpr[ LWRISCV_GPR_RA ] = ( portStackType ) vTaskExit;
    pxCtx->uxGpr[ LWRISCV_GPR_SP ] = ( portStackType ) pxSpTop;
    pxCtx->uxGpr[ LWRISCV_GPR_A0 ] = ( portStackType ) pxTaskParameters->pvParameters;
    pxCtx->uxPc                    = ( portStackType ) pxTaskParameters->pvTaskCode;
    pxCtx->uxFlags                 = TASK_CONTEXT_FLAG_SWITCH_FULL;
    pxCtx->uxSie                   = portSIE_ALL;
    pxCtx->uxSstatus               = 0;

    pxTCB->uxTopOfStackMirror      = ~( ( portUIntPtrType ) pxSpTop );

    /* Setup FPU Control */
#if ( configLW_FPU_SUPPORTED == 1U )
    if ( ( pxTCB->hFPUCtxOffset ) != 0 )
    {
        pxCtx->uxSstatus = FLD_SET_DRF64( _RISCV_CSR, _SSTATUS, _FS, _INITIAL, pxCtx->uxSstatus);
    }
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

#if TASK_RESTART
    pxTCB->pvTaskCode              = pxTaskParameters->pvTaskCode;
    pxTCB->ucDefaultPriority       = pxTaskParameters->uxPriority;
#endif /* TASK_RESTART */

    /* Indicate that this task stack is in use. */
    *( pxTCB->pxStackInUseMarker ) = portSTACK_IN_USE;

}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portUnsignedBaseType xPortGetExceptionNesting(void)
{
    return xPortExceptionNesting;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xPortIsTaskHandleValid( portTaskHandleType xTaskToCheck )
{
portBaseType xReturn;
xTCB *pxTCB;

    /* First ensure we've not been given a NULL pointer to check. */
    if( NULL == xTaskToCheck )
    {
        xReturn = pdFALSE;

        /* MCDC Test Point: STD_IF "xPortIsTaskHandleValid" */
    }
    else
    {
        /* Set a pointer to the TCB buffer. */
        pxTCB = ( xTCB * ) xTaskToCheck;

#if ( configLW_THREAD_CONTEXT_IN_TCB == 0U )
        if( ( portUnsignedBaseType )( pxTCB->pxStackLimit ) != ~( pxTCB->uxStackLimitMirror ) )
        {
            /* The stack limit mirror is not as expected. */
            xReturn = pdFALSE;
            /* MCDC Test Point: STD_IF "xPortIsTaskHandleValid" */
        }
        else
#endif /* ( configLW_THREAD_CONTEXT_IN_TCB == 0U ) */
        if( ( portUnsignedBaseType )( pxTCB->xCtx.uxGpr[LWRISCV_GPR_SP] ) != ~( pxTCB->uxTopOfStackMirror ) )
        {
            /* The top of stack mirror is not as expected. */
            xReturn = pdFALSE;
            /* MCDC Test Point: STD_ELSE_IF "xPortIsTaskHandleValid" */
        }
#if ( configLW_FPU_SUPPORTED == 1U )
        else if( ( pxTCB->hFPUCtxOffset ) != ~( pxTCB->hFPUCtxOffsetMirror ) )
        {
            /* The 'Using FPU' flag mirror is not as expected. */
            xReturn = pdFALSE;
            /* MCDC Test Point: STD_ELSE_IF "xPortIsTaskHandleValid" */
        }
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */
        else
        {
            /* Task handle is valid. */
            xReturn = pdTRUE;

            /* MCDC Test Point: STD_ELSE "xPortIsTaskHandleValid" */
        }

        /* MCDC Test Point: STD_ELSE "xPortIsTaskHandleValid" */
    }

    return xReturn;
}
/*---------------------------------------------------------------------------*/

/* Sets the next timer interrupt.
 * Using the previous value of the mtimecmp assures absence of time drifts.
 * 
 * csr time read permission is controlled with [m|s]counteren register BIT1.
 * if bit is set, csr time can be read from S/U mode;
 * if not set, it will be trapped into M mode and monitor is free to
 * provide emulated implementation.
 * (on spike, it is always trapped && emulated even bit in mcounteren is set,
 * sounds like a bug) */
KERNEL_FUNCTION static portUnsignedBaseType prvSetNextTimerInterrupt( void )
{
portUnsignedBaseType uxTicksStepped = 0;
portUnsignedBaseType now;

    now = csr_read( LW_RISCV_CSR_HPMCOUNTER_TIME );

    do
    {
        uxPortSysMtimeCmp += uxPortSysclkMtimePeriod;
        uxTicksStepped++;
    } while( now >= uxPortSysMtimeCmp );

    sbiSetTimer( now + uxPortSysclkMtimePeriod );

    return uxTicksStepped;
}

/*---------------------------------------------------------------------------*/

/* Sets and enables the first timer interrupt */
KERNEL_FUNCTION void vPortSetupTimer( void )
{
    uxPortSysclkMtimePeriod = lwrtosHookGetTimerPeriod();

    uxPortSysMtimeCmp = csr_read( LW_RISCV_CSR_HPMCOUNTER_TIME ) + uxPortSysclkMtimePeriod;

    sbiSetTimer( uxPortSysMtimeCmp );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vPortProcessSystemTick( void )
{
portUnsignedBaseType uxCount;
portUnsignedBaseType uxTicksStepped = 0;

    uxTicksStepped = prvSetNextTimerInterrupt();

    for( uxCount = 0; uxCount < uxTicksStepped; uxCount++ )
    {
        vTaskProcessSystemTickFromISR();
        /* MCDC Test Point: STD "vPortProcessSystemTick" */
    }
}
/*-----------------------------------------------------------------------------
 * Hook Functions.
 *---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vPortErrorHook( portTaskHandleType xHandleOfTaskWithError,
                                     portBaseType xErrorCode )
{
    lwrtosHookError( xHandleOfTaskWithError, xErrorCode );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vPortIdleHook( void )
{
    lwrtosHookIdle();
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vPortTickHook( void )
{
portUnsignedBaseType uxTicks;

    uxTicks = xTaskGetTickCount();
    lwrtosHookTimerTick( uxTicks );
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION void vPortTaskDeleteHook( portTaskHandleType xTaskBeingDeleted )
{
    /* AM-TODO: What goes here? */
}

/*-----------------------------------------------------------------------------
 * Filescope functions
 *---------------------------------------------------------------------------*/
KERNEL_CREATE_FUNCTION static xTCB *prvSetupTCB( const xTaskParameters * const pxTaskParameters )
{
xTCB *pxNewTCB;
portInt8Type *pcStackLastAddress;
portUnsignedBaseType uxStackDepthBytes;
portUIntPtrType uxTls;

    /* Set a pointer to the TCB buffer. */
    pxNewTCB = pxTaskParameters->pxTCB;

    /* Callwlate the last address of the task stack. */
    uxStackDepthBytes = pxTaskParameters->uxStackDepthBytes & ~portSTACK_ALIGNMENT_MASK;
    pcStackLastAddress = pxTaskParameters->pcStackBuffer + ( uxStackDepthBytes - sizeof( portStackType ) );

    /* Allocate TLS structure at the end of task stack. */
    uxTls = ( portUIntPtrType )( pcStackLastAddress - pxTaskParameters->uxTlsSize );
    uxTls &= ~( pxTaskParameters->uxTlsAlignment - 1 );

    pcStackLastAddress = ( portInt8Type * )( ( uxTls - sizeof( portStackType ) ) & ~portSTACK_ALIGNMENT_MASK );
    uxStackDepthBytes = pcStackLastAddress - pxTaskParameters->pcStackBuffer;

    /* Save stack information */
    pxNewTCB->pcStackLastAddress = pcStackLastAddress;
    pxNewTCB->uxStackSizeBytes = uxStackDepthBytes;

    /* Set the 'StackInUse' and 'Top of Stack' pointers ensuring correct
     * alignment. */
    pxNewTCB->pxStackInUseMarker = ( portStackType * ) pcStackLastAddress;
    pxNewTCB->xCtx.uxGpr[LWRISCV_GPR_SP] = ( ( ( portBaseType )pcStackLastAddress - sizeof( portStackType ) ) & ~portSTACK_ALIGNMENT_MASK );
    pxNewTCB->xCtx.uxGpr[LWRISCV_GPR_TP] = uxTls;

    /* Setup the TCB. */
    pxNewTCB->uxPriority = pxTaskParameters->uxPriority;
#if ( configINCLUDE_MUTEXES == 1U )
    pxNewTCB->uxBasePriority = pxTaskParameters->uxPriority;
#endif /* ( configINCLUDE_MUTEXES == 1U ) */
    vListInitialiseItem( &( pxNewTCB->xStateListItem ) );
    vListInitialiseItem( &( pxNewTCB->xEventListItem ) );

    /* Set the pxTCB as a link back from the xListItem. This is so we can get
     * back to the containing TCB from a generic item in a list. */
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xStateListItem ), pxNewTCB );

    /* Tasks are stored within event lists such that the list references tasks
     * in priority order - high priority to low priority. The ItemValue is used
     * to hold the priority in this case - but because ordered lists actually
     * sort themselves so that low values come before high values, the priority
     * assigned to the ItemValue has to be ilwerted. Thus high priority tasks
     * get a low ItemValue value, and low priority tasks get a high ItemValue
     * value. */
    listSET_LIST_ITEM_VALUE( &( pxNewTCB->xEventListItem ), ( portTickType ) ( configMAX_PRIORITIES - pxTaskParameters->uxPriority ) );
    listSET_LIST_ITEM_OWNER( &( pxNewTCB->xEventListItem ), pxNewTCB );

    /* Initialise notification parameters. */
    pxNewTCB->uxNotifiedValue = 0U;
    pxNewTCB->xNotifyState = taskNOTIFICATION_NOT_WAITING;

    /* Initialise TLS Object. */
    pxNewTCB->pvObject = pxTaskParameters->pvObject;

    /* Initialise the mutex held list. */
#if ( configINCLUDE_MUTEXES == 1U )
    vMutexInitHeldList( pxNewTCB );
#endif /* ( configINCLUDE_MUTEXES == 1U ) */

    /* Initialise the list of event poll objects owned by this task. */
#if ( configINCLUDE_EVENT_POLL == 1U )
    vListInitialise( &( pxNewTCB->xEventPollObjectsList ) );
#endif /* ( configINCLUDE_EVENT_POLL == 1U ) */

    pxNewTCB->pxMpuInfo = pxTaskParameters->pxMpuInfo;

    /* Initialise the run-time statistics members of the TCB. */
    vInitialiseTaskRunTimeStatistics( pxNewTCB );

#if ( configLW_FPU_SUPPORTED == 1U )
    /* Initialise the FPU members of the TCB. */
    pxNewTCB->hFPUCtxOffset = pxTaskParameters->hFPUCtxOffset;
    pxNewTCB->hFPUCtxOffsetMirror = ~( pxNewTCB->hFPUCtxOffset );
#endif /* ( configLW_FPU_SUPPORTED == 1U ) */

    /* Fill TLS */
    lwrtosTlsInit( pxNewTCB, ( void * )uxTls );

    return pxNewTCB;
}
/*---------------------------------------------------------------------------*/


KERNEL_FUNCTION void vPortSetWordAlignedBuffer( void * pvDestination, portBaseType xValue, portUnsignedBaseType uxLength )
{
portBaseType *pxDestination = ( portBaseType * ) pvDestination;
portUnsignedBaseType uxIdx = 0;

    uxLength /= 8;

    for( uxIdx = 0; uxIdx < uxLength; uxIdx++ )
    {
        pxDestination[ uxIdx ] = xValue;
    }
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xApplicationCopyDataToTask( portTaskHandleType xLwrrentTaskHandle,
                                                         void *pvApplicationDestinationAddress,
                                                         const void *pvKernelSourceAddress,
                                                         portUnsignedBaseType uxLength )
{
    /* VG-TODO: if we can guarantee pointers are aligned we can use faster copies */
    memcpy ( pvApplicationDestinationAddress, pvKernelSourceAddress, uxLength );

    return pdPASS;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xApplicationCopyDataFromTask( portTaskHandleType xLwrrentTaskHandle,
                                                           void *pvKernelDestinationAddress,
                                                           const void *pvApplicationSourceAddress,
                                                           portUnsignedBaseType uxLength )
{
    /* VG-TODO: if we can guarantee pointers are aligned we can use faster copies */
    memcpy ( pvKernelDestinationAddress, pvApplicationSourceAddress, uxLength );

    return pdPASS;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xPortCopyDataToTask( void *pvApplicationDestinationAddress,
                                                  const void *pvKernelSourceAddress,
                                                  portUnsignedBaseType uxLength )
{
portBaseType xResult;
portTaskHandleType xLwrrentTaskHandle;

    /* Has the scheduler been started? */
    if( xTaskIsSchedulerStarted() == pdFALSE )
    {
        xLwrrentTaskHandle = NULL;

        /* MCDC Test Point: STD_IF "xPortCopyDataToTask" */
    }
    else
    {
        xLwrrentTaskHandle = ( portTaskHandleType ) pxLwrrentTCB;

        /* MCDC Test Point: STD_ELSE "xPortCopyDataToTask" */
    }

    /* Call the user application defined function to perform any necessary
     * validity checking as well as the actual copy operation. */
    xResult = xApplicationCopyDataToTask( xLwrrentTaskHandle,
                                          pvApplicationDestinationAddress,
                                          pvKernelSourceAddress, uxLength );

    return xResult;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portBaseType xPortCopyDataFromTask( void *pvKernelDestinationAddress,
                                                    const void *pvApplicationSourceAddress,
                                                    portUnsignedBaseType uxLength )
{
portBaseType xResult;
portTaskHandleType xLwrrentTaskHandle;

    /* Has the scheduler been started? */
    if( xTaskIsSchedulerStarted() == pdFALSE )
    {
        xLwrrentTaskHandle = NULL;

        /* MCDC Test Point: STD_IF "xPortCopyDataFromTask" */
    }
    else
    {
        xLwrrentTaskHandle = ( portTaskHandleType ) pxLwrrentTCB;

        /* MCDC Test Point: STD_ELSE "xPortCopyDataFromTask" */
    }

    /* Call the user application defined function to perform any necessary
     * validity checking as well as the actual copy operation. */
    xResult = xApplicationCopyDataFromTask( xLwrrentTaskHandle,
                                            pvKernelDestinationAddress,
                                            pvApplicationSourceAddress,
                                            uxLength );

    return xResult;
}
/*---------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------
 * Weak stub function definitions
 *---------------------------------------------------------------------------*/

KERNEL_FUNCTION portWEAK_FUNCTION void vMutexInitHeldList( xTCB * pxTCB )
{
    /* MCDC Test Point: STD "vMutexInitHeldList" */

    ( void ) pxTCB;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portWEAK_FUNCTION void vEventPollInitTaskEventPollObjectst( xTCB * const  pxTCB )
{
    /* MCDC Test Point: STD "vEventPollInitTaskEventPollObjectst" */

    ( void ) pxTCB;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portWEAK_FUNCTION void vApplicationTaskDeleteHook( portTaskHandleType xTaskBeingDeleted )
{
    /* MCDC Test Point: STD "vApplicationTaskDeleteHook" */

    ( void ) xTaskBeingDeleted;
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portWEAK_FUNCTION void vApplicationIdleHook( void )
{
    /* MCDC Test Point: STD "vApplicationIdleHook" */
}
/*---------------------------------------------------------------------------*/

KERNEL_FUNCTION portWEAK_FUNCTION void vApplicationTickHook( void )
{
    /* MCDC Test Point: STD "vApplicationTickHook" */
}
/*---------------------------------------------------------------------------*/


