/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwrtosHooks.h
 * @brief   TODO: Document.
 */

#ifndef _LWRTOS_HOOKS_H_
#define _LWRTOS_HOOKS_H_

/* ------------------------ System Includes --------------------------------- */
#include "lwtypes.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
void lwrtosHookTimerTick(LwU32 xTicks)
    __attribute__((section (".imem_resident._lwrtosHookTimerTick")));
void lwrtosHookError(void *pxTask, LwS32 xErrorCode)
    __attribute__((section (".imem_resident._lwrtosHookError")));
void lwrtosHookIdle(void)
    __attribute__((section (".imem_resident._lwrtosHookIdle")));
LwS32 lwrtosHookIV0Handler(void)
    __attribute__((section (".imem_resident._lwrtosHookIV0Handler")));
#ifdef SCHEDULER_2X
LwU32 lwrtosHookTickAdjustGet(void)
    __attribute__((section (".imem_resident._lwrtosHookTickAdjustGet")));
#endif /* SCHEDULER_2X */

#endif // _LWRTOS_HOOKS_H_
