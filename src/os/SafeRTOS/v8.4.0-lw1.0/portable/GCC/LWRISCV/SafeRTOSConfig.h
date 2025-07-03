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

#ifndef SAFERTOS_CONFIG_H
#define SAFERTOS_CONFIG_H

/* Configuration options for the RTOS. */
#define configMAX_PRIORITIES                        ( 8U )

/* These defines are used to enable RTOS features that were added by LWPU. */

#ifdef FPU_SUPPORTED
#define configLW_FPU_SUPPORTED                      ( 1U )
#else /* FPU_SUPPORTED */
#define configLW_FPU_SUPPORTED                      ( 0U )
#endif /* FPU_SUPPORTED */

/* These defines control legacy features and shold be removed. */
#define configRTOS_USE_TASK_NOTIFICATIONS           ( 1U )
#define configLW_USE_QUEUE_VARIABLE_ITEM_SIZE       ( 0U )

/* Store thread state inside of TCB on context switch and not on stack. */
#define configLW_THREAD_CONTEXT_IN_TCB              ( 1U )

/* Feature inclusion macros.  */
#define configINCLUDE_EVENT_GROUPS                  ( 1U )
#define configINCLUDE_EVENT_POLL                    ( 1U )
#define configINCLUDE_TIMERS                        ( 0U )
#define configINCLUDE_RUN_TIME_STATS                ( 0U )
#define configINCLUDE_SEMAPHORES                    ( 1U )
#define configINCLUDE_MUTEXES                       ( 1U )

#if ( configINCLUDE_TIMERS == 1U )
#error "configINCLUDE_TIMERS is not supported by lwpu yet!!!"
#endif /* ( configINCLUDE_TIMERS == 1U ) */

#if ( configLW_THREAD_CONTEXT_IN_TCB == 0U )
#error "Storing task state on user stack is not supported!!!"
#endif /* ( configLW_THREAD_CONTEXT_IN_TCB == 0U ) */

#endif /* SAFERTOS_CONFIG_H */
