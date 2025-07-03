/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_objic.c
 * @brief  Container-object for the DPU Interrupt Control routines.  Contains
 *         generic non-HAL interrupt-routines plus logic required to hook-up
 *         chip-specific interrupt HAL-routines.
 *
 * @section _bCtxSwitchReq Interrupt Service Routine Context Switching
 *
 * The common theme for the interrupt service routines (including helpers) is
 * that all routines return a boolean value which indicates if a context-switch
 * is required as a result of the interrupt processing.  This oclwrs frequently
 * as tasks coordinate with the various unit-tasks.  If at any point during
 * interrupt-processing a task is "woken" (exited the "blocked" state) which is
 * of higher-priority than the interrupted-task, a context-switch is required
 * before the ISR completes.  The necessity of performing this context-switch
 * is communicated back up to the top-level interrupt-handler by way of the
 * return value of DPU application ISR (icService_XXX). @a TRUE indicates that
 * context-switch is required (preempt the current/interrupted task); @a FALSE
 * otherwise.
 *
 * @endsection
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "lwdpu.h"
#include "dpu_objic.h"
#include "dpu_objhal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Global Variables ------------------------------- */
IC_HAL_IFACES IcHal;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
LwS32 lwrtosHookIV0Handler(void)
    GCC_ATTRIB_SECTION("imem_resident", "lwrtosHookIV0Handler");

/* ------------------------ Public Functions ------------------------------- */

void
constructIc(void)
{
    IfaceSetup->icHalIfacesSetupFn(&IcHal);
}

// IV0 Falcon interrupt handler.
LwS32 lwrtosHookIV0Handler(void)
{
    return (LwS32)icService_HAL(&IcHal);
}
