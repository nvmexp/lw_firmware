/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_objic.c
 * @brief  Container-object for the PMU Interrupt Control routines.  Contains
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
 * return value of PMU application ISR (icService_XXX). @a LW_TRUE indicates
 * that context-switch is required (preempt the current/interrupted task);
 * @a LW_FALSE otherwise.
 *
 * @endsection
 *
 * Implementation Notes:
 *    # Within ISRs only use macro DBG_PRINTF_ISR (never DBG_PRINTF).
 */

/* ------------------------- System Includes -------------------------------- */
#include "flcntypes.h"
#include "pmusw.h"

/* ------------------------ RTOS Includes ----------------------------------- */
#include "lwrtosHooks.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objic.h"
#include "pmu_objhal.h"
#include "pmu_objtimer.h"

#include "config/g_ic_private.h"
#if(IC_MOCK_FUNCTIONS_GENERATED)
#include "config/g_ic_mock.c"
#endif

/* ------------------------- Global Variables ------------------------------- */
OBJIC Ic;

/* ------------------------- Public Functions ------------------------------- */
FLCN_STATUS
constructIc_IMPL(void)
{
    return FLCN_OK;
}

#if PMUCFG_FEATURE_ENABLED(ARCH_FALCON)
// IV0 Falcon interrupt handler.
LwS32 lwrtosHookIV0Handler(void)
{
    return (LwS32)icService_HAL(&Ic, icGetPendingUprocInterrupts_HAL(&Ic));
}
#else // PMUCFG_FEATURE_ENABLED(ARCH_FALCON)
sysKERNEL_CODE void
lwrtosRiscvExtIrqHook(void)
{
    icService_HAL(&Ic, icGetPendingUprocInterrupts_HAL(&Ic));
}
#endif // PMUCFG_FEATURE_ENABLED(ARCH_FALCON)
