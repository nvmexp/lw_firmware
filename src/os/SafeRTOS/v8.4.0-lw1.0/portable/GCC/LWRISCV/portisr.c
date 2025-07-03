/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#define KERNEL_SOURCE_FILE

/* Scheduler Includes */
#include "SafeRTOS.h"
#include "task.h"
#include "lwrtosHooks.h"

// MMINTS-TODO: make this an lwrtos hook
extern void appHalt(void);

/*-----------------------------------------------------------*/
/* Variables.                                                */
/*-----------------------------------------------------------*/

/* Set to pdTRUE to pend a context switch from an ISR. */
KERNEL_DATA volatile LwBool IcCtxSwitchReq = pdFALSE;

/*-----------------------------------------------------------*/

/*
 * External interrupts, for now support only cmdqueue
 */
KERNEL_FUNCTION void vPortHandleExtIrq( void )
{
    lwrtosRiscvExtIrqHook();
}
/*-----------------------------------------------------------*/

KERNEL_FUNCTION void vPortHandleSwIrq( void )
{
    lwrtosRiscvSwIrqHook();
}
/*-----------------------------------------------------------*/

KERNEL_FUNCTION void vPortHandleTimerIrq( void )
{
    vPortProcessSystemTick();
}
/*-----------------------------------------------------------*/

/*
 * This handler handles *ONLY* exceptions excluding ECALL
 */
KERNEL_FUNCTION void vPortHandleException( xPortTaskContext * pxCtx,
                                           portUnsignedBaseType uxCause,
                                           portUnsignedBaseType uxCause2,
                                           portUnsignedBaseType uxTrapVal )
{
    if( NULL == pxCtx )
    {
        appHalt(); /* That should not happen, we support nested exceptions. */
    }

    lwrtosRiscvExceptionHook( pxCtx, uxCause, uxCause2, uxTrapVal );
}
