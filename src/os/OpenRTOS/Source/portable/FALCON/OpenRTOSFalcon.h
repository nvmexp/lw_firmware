/*
 * Copyright 2008-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */

#ifndef OPEN_RTOS_BUILD
    #error This header file must not be included outside OpenRTOS build.
#endif

#ifndef _OPENRTOS_FALCON_H_
#define _OPENRTOS_FALCON_H_

/*!
 * @file OpenRTOSFalcon.h
 *
 * Falcon-Specific OpenRTOS Interface Functions
 */

/* ------------------------ System Includes -------------------------------- */
#include "falcon.h"
/* ------------------------ OpenRTOS Includes ------------------------------ */
#include "task.h"

/* ------------------------ Application Includes --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/*!
 * Define macros needed for setting/resetting falcon privilege level
 */
#ifdef DYNAMIC_FLCN_PRIV_LEVEL
    extern void osFlcnPrivLevelSetFromLwrrTask(void);
    extern void osFlcnPrivLevelReset(void);

    #define portFLCN_PRIV_LEVEL_SET_FROM_LWRR_TASK()                   \
        osFlcnPrivLevelSetFromLwrrTask()
    #define portFLCN_PRIV_LEVEL_RESET()                                \
        osFlcnPrivLevelReset()
#else
    #define portFLCN_PRIV_LEVEL_SET_FROM_LWRR_TASK()
    #define portFLCN_PRIV_LEVEL_RESET()
#endif

/*!
 * Define macro needed for detecting stack overflow in ISR/ESR stacks
 */
#ifndef HW_STACK_LIMIT_MONITORING
    #define  portISR_STACK_OVERFLOW_DETECT()                           \
        osDebugISRStackOverflowDetect()
    #define  portESR_STACK_OVERFLOW_DETECT()                           \
        osDebugESRStackOverflowDetect()
#else
    #define  portISR_STACK_OVERFLOW_DETECT()
    #define  portESR_STACK_OVERFLOW_DETECT()
#endif

/*!
 * Defines for values taken by the integer passed to the task entry points.
 * Today, we only use the value to separate a task cold start from a restart.
 * The RTOS task entry function prototypes expect a void pointer as the                                                                         .
 * input parameter, so instead of changing the prorotypes, we will just cast                                                                                                                                        .
 * to what the tasks expect.                                                                                                                                                                                                                 .
 */
#define PORT_TASK_FUNC_ARG_COLD_START   ((void *) (0))
#define PORT_TASK_FUNC_ARG_RESTART      ((void *) (1))

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

extern void osDebugISRStackOverflowDetect(void);

extern void osDebugESRStackOverflowDetect(void);

void vPortFlcnSetTickIntrMask(unsigned int mask)
    __attribute__((section (".imem_init._vPortFlcnSetTickIntrMask")));

void vPortFlcnSetOsTickIntrClearRegAndValue(unsigned int reg, unsigned int value)
    __attribute__((section (".imem_init._vPortFlcnSetOsTickIntrClearRegAndValue")));

#endif  /* _OPENRTOS_FALCON_H_ */

