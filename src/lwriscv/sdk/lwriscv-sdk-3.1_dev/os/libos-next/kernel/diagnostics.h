/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_DIAGNOSTICS_H__
#define LIBOS_DIAGNOSTICS_H__
#include "kernel.h"

#if LIBOS_CONFIG_KERNEL_LOG
    __attribute__((format(printf, 1, 2)))
    void KernelPrintf(const char* format, ...);
#else
#   define KernelPrintf(...)
#endif

#if LIBOS_CONFIG_KERNEL_TRACE
#   define KernelTrace KernelPrintf
#else
#   define KernelTrace(...)
#endif


LIBOS_NORETURN void KernelPanic();

#if LIBOS_DEBUG
#   define KernelAssert(cond)                           \
        if (!(cond))                                \
        {                                           \
            KernelPrintf("Assert: " #cond " @ " __FILE__ ":%d\n", __LINE__);  \
            KernelPanic();                         \
        }
#else
#   define KernelAssert(cond)
#endif

#   define KernelPanilwnless(cond)                  \
        if (!(cond))                                \
        {                                           \
            KernelPrintf("Runtime failure: " #cond " @ " __FILE__ ":%d\n", __LINE__);  \
            KernelPanic();                         \
        }

#endif