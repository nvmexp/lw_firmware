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

#include <cpu-intrinsics.h>
#include "lwmisc.h"
#include "lwtypes.h"

// lwmisc.h utility aliases
#define ALIGN_UP         LW_ALIGN_UP
#define ALIGN_DOWN       LW_ALIGN_DOWN
#define VAL_IS_ALIGNED   LW_IS_ALIGNED

#ifndef NULL
#define NULL (void *)0
#endif

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
 * @brief   Returns a specific value based on if ODP is enabled or not.
 *
 * @param[in]   odp     Value to be returend when ODP is enabled.
 * @param[in]   no_odp  Value to be returend when ODP is disabled.
 *
 * @return  One of the two input parameters based on ODP enablement.
 */
#ifdef ON_DEMAND_PAGING_BLK
#define LW_ODP_VAL(odp, no_odp)      odp
#else // ON_DEMAND_PAGING_BLK
#define LW_ODP_VAL(odp, no_odp)      no_odp
#endif // ON_DEMAND_PAGING_BLK

/*!
 * Various attributes are required by GCC to properly link the image for the
 * PMU. These attributes are specific to GCC and must be compiled away when
 * testing in simulation (which may not use GCC).  The following macros should
 * be used to mark function and variable attributes instead of using the
 * attribute tags directly.
 */
#ifndef UNITTEST
#define GCC_ATTRIB_ALIGN(n)                         __attribute__((aligned(n)))
#define GCC_ATTRIB_SECTION(sec, item)               __attribute__((section("." sec "._" item)))
#define GCC_ATTRIB_SECTION_COLD_ERROR(sec, item)    \
        GCC_ATTRIB_SECTION(LW_ODP_VAL("imem_errorHandlingCold", sec), item)
#define GCC_ATTRIB_USED()                           __attribute__((__used__))
#define GCC_ATTRIB_NOINLINE()                       __attribute__((__noinline__))
#define GCC_ATTRIB_PACKED()                         __attribute__((packed))

/*!
 * Generally, functions are not inlined unless optimization is specified. For
 * functions declared inline, this attribute inlines the function even if no
 * optimization level is specified.
 */
#define GCC_ATTRIB_ALWAYSINLINE()       __attribute__((always_inline))
#else
#define GCC_ATTRIB_ALIGN(n)
#define GCC_ATTRIB_SECTION(s, i)
#define GCC_ATTRIB_SECTION_COLD_ERROR(s, i)
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
    #ifdef UPROC_FALCON
        #define GCC_ATTRIB_NO_STACK_PROTECT()    __attribute__((no_stack_protect))
    #else
        #define GCC_ATTRIB_NO_STACK_PROTECT()
    #endif
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
#endif /* UPROC_FALCON */

#ifdef UTF_FUNCTION_MOCKING
#define MOCKABLE(funcname) funcname##_MOCK
#else
#define MOCKABLE(funcname) funcname##_IMPL
#endif

/*!
 * @brief In some unit tests, the UTF will need to access static members within
 * unit. Declare them with UPROC_STATIC so that they are not actually static in
 * the UTF build.
 */
#ifdef UTF_UCODE_BUILD
#define UPROC_STATIC
#else // UTF_UCODE_BUILD
#define UPROC_STATIC static
#endif // UTF_UCODE_BUILD

/*!
 * @brief   Compile time checker to be used at the begining of a _BEGIN/_END
 *          macro pair like BOARDOBJGRP_ITERATOR_(BEGIN|_END) to ensure that the
 *          correct closing scope macro is being used.
 *
 * @note    An opening brace `{` should be used to declare the beginning of a
 *          scope before this macro is called.
 *
 * @note    Example output:
 *          error: unused variable 'MY_ITERATOR_MISMATCHED_BEGIN_END_MACROS'.
 *
 * @param[in]   _scope  A name for the scope, BOARDOBJGRP_ITERATOR for example.
 */
#define CHECK_SCOPE_BEGIN(_scope)                                              \
    LwU32 _scope##_MISMATCHED_BEGIN_END_MACROS

/*!
 * @brief   Compile time checker to be used at the end of a _BEGIN/_END
 *          macro pair like BOARDOBJGRP_ITERATOR_(BEGIN|_END) to ensure that the
 *          correct opening scope macro is being used.
 *
 * @note    A closing brace `}` should be used to define an end of scope after
 *          this macro is called.
 *
 * @note    Example output:
 *          error: 'MY_ITERATOR_MISMATCHED_BEGIN_END_MACROS' undeclared (first
 *                 use in this function).
 *
 * @param[in]   _scope  A name for the scope, BOARDOBJGRP_ITERATOR for example.
 */
#define CHECK_SCOPE_END(_scope)                                                \
    (void)_scope##_MISMATCHED_BEGIN_END_MACROS

/*!
 * @brief   Aligns v down to the level of gran
 *
 * @param[in]   v       Value to align
 * @param[in]   gran    Granularity to which to align
 *
 * @return  v aligned down to gran
 *
 * @note    Gives unexpected results if gran is not a power of 2.
 *
 * @note    The `_CT` macro implementation is provided for compile-time use.
 *          For run-time, the static inline functions should be preferred.
 */
#define LWUPROC_ALIGN_DOWN_CT(v, gran) \
    ((v) & ~((gran) - 1U))

/*!
 * @brief   Aligns v up to the level of gran
 *
 * @param[in]   v       Value to align
 * @param[in]   gran    Granularity to which to align
 *
 * @return  v aligned up to gran
 *
 * @note    Gives unexpected results if gran is not a power of 2.
 *
 * @note    The `_CT` macro implementation is provided for compile-time use.
 *          For run-time, the static inline functions should be preferred.
 */
#define LWUPROC_ALIGN_UP_CT(v, gran) \
    (((v) + ((gran) - 1U)) & ~((gran) - 1U))

/*!
 * @copydoc LWUPROC_ALIGN_DOWN_CT
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
LWUPROC_ALIGN_DOWN(LwU32 v, LwU32 gran)
{
    return LWUPROC_ALIGN_DOWN_CT(v, gran);
}

/*!
 * @copydoc LWUPROC_ALIGN_UP_CT
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU32
LWUPROC_ALIGN_UP(LwU32 v, LwU32 gran)
{
    return LWUPROC_ALIGN_UP_CT(v, gran);
}

/*!
 * @copydoc LWUPROC_ALIGN_DOWN_CT
 *
 * @note    Provided for 64-bit alignments.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU64
LWUPROC_ALIGN_DOWN64(LwU64 v, LwU64 gran)
{
    return LWUPROC_ALIGN_DOWN_CT(v, gran);
}

/*!
 * @copydoc LWUPROC_ALIGN_UP_CT
 *
 * @note    Provided for 64-bit alignments.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline LwU64
LWUPROC_ALIGN_UP64(LwU64 v, LwU64 gran)
{
    return LWUPROC_ALIGN_UP_CT(v, gran);
}

#endif // LIBS_LWUPROC_H
