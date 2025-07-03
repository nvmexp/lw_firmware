/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBS_LWUPROC_H
#define LIBS_LWUPROC_H

#include "external/lwmisc.h"
#include "lwtypes.h"

// lwmisc.h utility aliases
#define ALIGN_UP         LW_ALIGN_UP
#define ALIGN_DOWN       LW_ALIGN_DOWN
#define VAL_IS_ALIGNED   LW_IS_ALIGNED

/*!
 * Inline swap of two values x and y.
 */
#define SWAP(type, x, y)    \
do {                        \
    type _tmp;              \
    _tmp = x;               \
    x    = y;               \
    y    = _tmp;            \
} while (LW_FALSE)

/*!
 * @brief   Returns an architecture specific value.
 *
 * @param[in]   falcon  Value to be returend on Falcon architecture.
 * @param[in]   riscv   Value to be returend on RISC-V architecture.
 *
 * @return  One of the two input parameters based on architecture.
 */
#ifdef UPROC_RISCV
#define LW_ARCH_VAL(falcon, riscv)      riscv
#else // UPROC_RISCV
#define LW_ARCH_VAL(falcon, riscv)      falcon
#endif // UPROC_RISCV

/*!
 * Various attributes are required by GCC to properly link the image for the
 * PMU. These attributes are specific to GCC and must be compiled away when
 * testing in simulation (which may not use GCC).  The following macros should
 * be used to mark function and variable attributes instead of using the
 * attribute tags directly.
 */
#ifndef UNITTEST
#define GCC_ATTRIB_ALIGN(n)             __attribute__((aligned(n)))
#define GCC_ATTRIB_SECTION(sec, item)   __attribute__((section("." sec "._" item)))
#define GCC_ATTRIB_USED()               __attribute__((__used__))
#define GCC_ATTRIB_NOINLINE()           __attribute__((__noinline__))

/*!
 * Generally, functions are not inlined unless optimization is specified. For
 * functions declared inline, this attribute inlines the function even if no
 * optimization level is specified.
 */
#define GCC_ATTRIB_ALWAYSINLINE()       __attribute__((always_inline))
#else
#define GCC_ATTRIB_ALIGN(n)
#define GCC_ATTRIB_SECTION(s, i)
#define GCC_ATTRIB_USED()
#define GCC_ATTRIB_NOINLINE()
#define GCC_ATTRIB_ALWAYSINLINE()
#endif

/*!
 * Following macros insert attribute to explicitly enable/disable
 * Stack Smashing Protection per function
 */
#ifdef IS_SSP_ENABLED
    #define GCC_ATTRIB_STACK_PROTECT()       __attribute__((stack_protect))
    #define GCC_ATTRIB_NO_STACK_PROTECT()    __attribute__((no_stack_protect))
#else
    #define GCC_ATTRIB_STACK_PROTECT()
    #define GCC_ATTRIB_NO_STACK_PROTECT()
#endif

/*!
 * Helper macros colwerting address space between Uproc VA and DMEM PA
 */
#if UPROC_FALCON
#define LW_UPROC_VA_TO_DMEM_PA(addr)     (LwUPtr)(addr)
#define LW_DMEM_PA_TO_UPROC_VA(addr)     (addr)
#else
extern LwU8 __dtcm_virt_start[];
#define LW_UPROC_VA_TO_DMEM_PA(addr)     ((LwU32)((LwUPtr)(addr) - (LwUPtr)__dtcm_virt_start))
#define LW_DMEM_PA_TO_UPROC_VA(addr)     ((LwUPtr)__dtcm_virt_start + (LwUPtr)(addr))
#endif

#ifdef UTF_FUNCTION_MOCKING
#define MOCKABLE(funcname) funcname##_MOCK
#else
#define MOCKABLE(funcname) funcname##_IMPL
#endif

#endif // LIBS_LWUPROC_H
