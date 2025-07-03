/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOSLAYER_H
#define LWOSLAYER_H

/*!
 * @file    lwoslayer.h
 * @copydoc lwoslayer.c
 */

/* ------------------------ System Includes --------------------------------- */
#include "lwtypes.h"
#include "lwuproc.h"
#include "lwrtos.h"
#ifdef UPROC_RISCV
# ifdef IS_SSP_ENABLED
#  include <shlib/ssp.h>
# endif // IS_SSP_ENABLED
# if defined(PMU_RTOS) || defined(GSP_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS)
#  include "config/g_sections_riscv.h"
# endif // defined(PMU_RTOS) || defined(GSP_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS)
#endif // UPROC_RISCV

/* ------------------------ Application Includes ---------------------------- */
#include "lw_rtos_extension.h"
#include "lwostask.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
extern LwU32 _heap_end[];   // created in the linker script

/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Defines ----------------------------------------- */
#if defined(UPROC_FALCON)
/*!
 * Without enabling large-code support, the compiler and linker need help with
 * labels and symbols which resolve to values greater than or equal to 0x8000.
 * When labels or symbols of this sort are directly accessed, the linker will
 * complain since the compiler will use plain mv instructions for the accesses
 * and mv takes a signed 16-bit operand (and this cannot handle values above
 * 0x8000). Use the following macros to wrap accesses to labels and symbols
 * above 0x8000 to workaround this problem.
 *
 * Additional background may be found here:
 *
 * https://wiki.lwpu.com/gpuhwdept/index.php/Falcon_IP/Tools_Q&As\
 *      #What_to_do_with_the_error_message_.22R_FALCON_16_H_S.22_when\
 *      _using_new_falcontools.28gcc4.3.2.29.3F
 */
# define OS_LABEL(label)                  falc_mv32i(label)
# define PTR16(ptr)                       ((void*)falc_mv32i(ptr))
# define ADD_LINKER_SYMBOL_PREFIX(ptr)    ptr
#elif defined(UPROC_RISCV)    // UPROC_FALCON
# define OS_LABEL(label)                  ((LwUPtr)(label))
# define CONCATSYM(prefix, x)             (prefix##x)
# define PTR16(ptr)                       ((void *)(&(CONCATSYM(_, ptr))))
#else    // UPROC_RISCV
# define OS_LABEL(label)                  (LwUPtr)(label)
# define PTR16(ptr)                       (ptr)
#endif

/*!
 * @copydoc lwrtosHALT()
 */
#define OS_HALT()                   lwrtosHALT()

/*!
 * @brief Halt if assert condition is not satisfied
 *
 * Halt when cond is evaluated to be LW_FALSE. Do not abuse this macro as
 * it takes code space and doesn't get lwlled out. Use it to assert critical
 * conditions.
 */
#define OS_ASSERT(cond)                                                        \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            OS_HALT();                                                         \
        }                                                                      \
    } while (0)

/*!
 * @copydoc lwrtosBREAKPOINT()
 */
#ifdef USE_TRUE_BREAKPOINT
#define OS_BREAKPOINT()             lwrtosHALT()
#else // USE_TRUE_BREAKPOINT
#define OS_BREAKPOINT()             lwrtosBREAKPOINT()
#endif // USE_TRUE_BREAKPOINT

/*!
 * @brief   Temporary refactoring macro for JIRA task GPUSWDTMRD-745
 */
#ifdef USE_TRUE_BREAKPOINT
#define OS_TRUE_BP()                lwrtosBREAKPOINT() // TODO: mmints
#else // USE_TRUE_BREAKPOINT
#define OS_TRUE_BP()                OS_BREAKPOINT()
#endif // USE_TRUE_BREAKPOINT

/*!
 * Utility macro for waiting on an event for at most the supplied number of
 * ticks, as opposed to forever.  Will return when event is received or the us
 * timer expires - whichever comes first.  Useful for unit-tasks which should
 * block for events in the middle of a timed (e.g. 1Hz) loop.
 */
#define OS_QUEUE_WAIT_TICKS(queue, pParam, ticks)                              \
    (FLCN_OK == lwrtosQueueReceive((queue), (pParam), sizeof(*pParam), (ticks)))

/*!
 * Utility macro for waiting on an event queue forever.  Useful for unit-tasks
 * which should block until the next event.
 */
#define OS_QUEUE_WAIT_FOREVER(queue, pParam)                                   \
    OS_QUEUE_WAIT_TICKS((queue), (pParam), lwrtosMAX_DELAY)


/*!
 * SCP-specific defines and function prototypes
 */
#if defined(IS_SSP_ENABLED) && defined(IS_SSP_ENABLED_WITH_SCP)
# include "scp_internals.h"

/*
 * The SCP-based stack canary scheme requires that a true random number be
 * generated and kept secret in a register in the secure co-processor (and never
 * retrieved or modified). To help ensure that, we do the following for the
 * register we want to use (SCP_R7):
 *  1.) Ensure it has the numerical value we expect (i.e., 7)
 *  2.) #undef the original macro definition so that it will not be used
 *  3.) #define a new macro to the original macro's value
 */
ct_assert(SCP_R7 == 7U);
# undef SCP_R7


/*!
 * @brief   Register in SCP reserved for use by LWOS SSP code; it *MUST NOT* be
 *          accessed by non-SSP code.
 */
# define LWOS_SSP_SCP_RESERVED_REGISTER                                       7U

/*!
 * @brief   Initializes a source of true randomness for use with the stack
 *          canary
 *
 * @note    After this function exelwtes, it will leave a true random number in
 *          the SCP register specified by @ref LWOS_SSP_SCP_RESERVED_REGISTER.
 *          This register *MUST NOT* be accessed after this function exelwtes.
 */
void lwosSspRandInit(void)
    GCC_ATTRIB_NO_STACK_PROTECT()
    // lwosSspRandInit must be in resident kernel code because it's called in
    // syscall context on partition re-entry!
    GCC_ATTRIB_SECTION(LW_ARCH_VAL("imem_init", "kernel_code"), "lwosSspRandInit");

/*!
 * @brief   Retrieves a pseudo-random value that has been encrypted using a true
 *          random key, providing a stronger value as a stack canary.
 *
 * @return  The encrypted value to be used as the stack canary
 */
LwUPtr lwosSspGetEncryptedCanary(void)
    GCC_ATTRIB_NO_STACK_PROTECT()
    GCC_ATTRIB_SECTION("imem_resident", "lwosSspGetEncryptedCanary");

#define LWOS_GET_CANARY()  lwosSspGetEncryptedCanary()

#elif defined(IS_SSP_ENABLED)

static inline LwUPtr lwosSspGetTimeBasedRandomCanary(void)
{
# ifdef UPROC_FALCON
    //
    // Set stack canary value to XOR of PTIMER0 and PTIMER1. As PTIMER0's
    // lower four bits are almost always 0, this introduces more randomness.
    //
    FLCN_TIMESTAMP lwrTimeInNs;
    osPTimerTimeNsLwrrentGet(&lwrTimeInNs);
    return (LwUPtr)lwrTimeInNs.parts.lo ^ lwrTimeInNs.parts.hi;
# else // ! UPROC_FALCON
    // Use lwriscv implementation instead
    return getTimeBasedRandomCanary();
# endif // UPROC_FALCON
}

# define LWOS_GET_CANARY()  lwosSspGetTimeBasedRandomCanary()

#endif // if defined(IS_SSP_ENABLED) && defined(IS_SSP_ENABLED_WITH_SCP)


#ifdef IS_SSP_ENABLED
# define LWOS_SETUP_STACK_CANARY_VAR(_canaryName)                              \
    do                                                                         \
    {                                                                          \
        (_canaryName) = LW_LWUPTR_TO_PTR(LWOS_GET_CANARY());                   \
    }                                                                          \
    while (LW_FALSE)
#else
# define LWOS_SETUP_STACK_CANARY_VAR(_canaryName) lwosNOP()
#endif // IS_SSP_ENABLED


/*!
 * Macro to set __stack_chk_guard to a random canary value.
 *
 * This should be used carefully as it can change the stack canary under
 * functions already using it. The canary value for a task is saved/restored
 * when it is unloaded/loaded.
 *
 * !!!!!!!!!!!!!!!!!!! WARNING WARNING WARNING WARNING !!!!!!!!!!!!!!!!!!!!!
 * This macro should be used at setup only. When on RISCV it assumes code is
 * running in kernel mode.
 *
 */
#if defined(IS_SSP_ENABLED) && defined(UPROC_RISCV)
# ifdef IS_SSP_ENABLED_PER_TASK
//
// SSP is enabled per-task, so we don't need __task_stack_chk_guard. Only set
// the kernel canary and an initial user-mode canary.
// Set the initial canary to the kernel canary for pre-init code.
//
#  define LWOS_SETUP_STACK_CANARY()                                            \
    do                                                                         \
    {                                                                          \
        LWOS_SETUP_STACK_CANARY_VAR(__kernel_stack_chk_guard);                 \
        __stack_chk_guard = __kernel_stack_chk_guard;                          \
    }                                                                          \
    while (LW_FALSE)
# else // !IS_SSP_ENABLED_PER_TASK
//
// SSP is not enabled per-task, so we need both __kernel_stack_chk_guard and
// __task_stack_chk_guard (the common stack canary for all RISCV tasks).
// Set the initial canary to the kernel canary for pre-init code.
//
#  define LWOS_SETUP_STACK_CANARY()                                            \
    do                                                                         \
    {                                                                          \
        LWOS_SETUP_STACK_CANARY_VAR(__kernel_stack_chk_guard);                 \
        LWOS_SETUP_STACK_CANARY_VAR(__task_stack_chk_guard);                   \
        __stack_chk_guard = __kernel_stack_chk_guard;                          \
    }                                                                          \
    while (LW_FALSE)
# endif // IS_SSP_ENABLED_PER_TASK
#else // ! if defined(IS_SSP_ENABLED) && defined(UPROC_RISCV)
# define LWOS_SETUP_STACK_CANARY()  LWOS_SETUP_STACK_CANARY_VAR(__stack_chk_guard)
#endif // if defined(IS_SSP_ENABLED) && defined(UPROC_RISCV)

/*
 * Helper macros for translating between virtual and physical addresses,
 * should work for Falcon and RISCV.
 */
#ifdef UPROC_FALCON
#define LWOS_SECTION_VA_TO_PA(sec, addr)  ((LwUPtr)(addr))
#define LWOS_SECTION_PA_TO_VA(sec, addr)  (addr)
#else /* UPROC_FALCON */
#if defined(PMU_RTOS) || defined(GSP_RTOS) || defined(SEC2_RTOS) || defined(SOE_RTOS)
extern LwUPtr SectionDescStartPhysical[UPROC_SECTION_COUNT];

// Here we can use definitions from g_sections_riscv.h
#define LWOS_SECTION_VA_TO_PA(sec, addr)    ((addr) -                         \
                                             UPROC_SECTION_VIRT_OFFSET(sec) + \
                                             SectionDescStartPhysical[UPROC_SECTION_REF(sec)])

#define LWOS_SECTION_PA_TO_VA(sec, addr)    ((addr) -                         \
                                             SectionDescStartPhysical[UPROC_SECTION_REF(sec)] + \
                                             UPROC_SECTION_VIRT_OFFSET(sec))

#define LWOS_SECTION_CONTAINS_VA(sec, addr) (((LwUPtr)(addr)) >= UPROC_SECTION_VIRT_OFFSET(sec) && \
                                             (((LwUPtr)(addr)) < UPROC_SECTION_VIRT_OFFSET(sec) + \
                                                    UPROC_SECTION_MAX_SIZE(sec)))
#endif /* defined(PMU_RTOS) || defined(GSP_RTOS) ||  defined(SEC2_RTOS) ||  defined(SOE_RTOS)*/

#endif /* UPROC_FALCON */

#define LWOS_SHARED_MEM_VA_TO_PA(addr)  LWOS_SECTION_VA_TO_PA(UPROC_SHARED_DMEM_SECTION_NAME, addr)
#define LWOS_SHARED_MEM_PA_TO_VA(addr)  LWOS_SECTION_PA_TO_VA(UPROC_SHARED_DMEM_SECTION_NAME, addr)

#endif // LWOSLAYER_H
