/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrdbg.h
 */

#ifndef TEGRA_ACR_DBG_H
#define TEGRA_ACR_DBG_H

#include "lwctassert.h"

#ifndef UPROC_RISCV
# include <falc_trace.h>
#endif // UPROC_RISCV

#ifdef PMU_RTOS
// When building as PMU submake, we want to use cpu-intrinsics.h for lwuc_halt.
# include "cpu-intrinsics.h"
#else // PMU_RTOS
# ifndef lwuc_halt
#  define lwuc_halt() falc_halt()
# endif // lwuc_halt
# ifndef lwuc_trap1
#  define lwuc_trap1() falc_trap1()
# endif // lwuc_trap1
#endif // PMU_RTOS

#ifdef ACRDBG_PRINTF
# ifdef UPROC_FALCON
#  define DBG_PRINTF(x) falc_debug x
# else // UPROC_FALCON
// MMINTS-TODO: plug in RISCV prints here if needed (unlikely)
# endif // UPROC_FALCON
#else // ACRDBG_PRINTF
# if defined(ACR_FALCON_PMU) || !defined(PMU_RTOS)
// Hack: On PMU-RTOS builds we only add this when #include-d in the ACR_FALCON_PMU submake
#  define DBG_PRINTF(x)
# endif // defined(ACR_FALCON_PMU) || !defined(PMU_RTOS)
#endif // ACRDBG_PRINTF

/*!
 * ACR_HALT halts the current exelwtion of code.
 */
#define ACR_HALT() lwuc_halt()

/*!
 * @brief Set a breakpoint recoverable by RM
 *
 * ACR_BREAKPOINT will halt the current exelwtion of code. This is very much
 * like lwuc_halt, but RM will try to recover from the halt state and resume
 * exelwtion. Use this if the error is not critical thus resuming will unlikely
 * cause other problems.
 */
#define ACR_BREAKPOINT() lwuc_trap1()

/*!
 * @brief Halt ACR if assert condition is not satisfied
 *
 * Halt when cond is evaluated to be LW_FALSE. Do not abuse this
 * macro as it takes code space and doesn't get lwlled out. Use it to assert
 * critical conditions.
 */
#define ACR_ASSERT(cond)                                                      \
    if (!(cond))                                                              \
    {                                                                         \
        ACR_HALT();                                                           \
    }

/*!
 * ACR_HALT_COND is identical to ACR_ASSERT.
 */
#define ACR_HALT_COND(cond)                                                   \
    if (!(cond))                                                              \
    {                                                                         \
        ACR_HALT();                                                           \
    }

/*!
 * ACR_BREAKPOINT_COND is identical to ACR_ASSERT except that the exelwtion of
 * code will be resumed by RM.
 */
#define ACR_BREAKPOINT_COND(cond)                                             \
    if (!(cond))                                                              \
    {                                                                         \
        ACR_BREAKPOINT();                                                     \
    }

/*!
 * @brief ACR Compile-Time Assert
 *
 * ACR_CT_ASSERT asserts a condition that can be caught during compile time. It
 * ensures checking certain conditions without adding any code or data.
 *
 * Example:
 *  ACR_CT_ASSERT(sizeof(LwU32) == 4);
 *  ACR_CT_ASSERT(LWSTOM_DEFINE == LW_DEVICE_REGISTER_FIELD_VAL);
 */
#define ACR_CT_ASSERT(cond)  ct_assert(cond)

#endif // TEGRA_ACR_DBG_H
