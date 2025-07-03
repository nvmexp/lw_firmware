/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once
#include <lwtypes.h>

#include "libos.h"
#include "libos_log.h"

static inline void LibosBreakpoint()
{
    __asm volatile("ebreak");
}

#if LIBOS_DEBUG
#define LibosValidate(x)                                                         \
    if (!(x))                                                                  \
    {                                                                          \
        LibosPrintf("Assertion violation %s:%d %s\n", __FILE__, __LINE__, #x); \
        LibosPanic();                                                     \
    }
#else
#define LibosValidate(x)                                                         \
    if (!(x))                                                                  \
    {                                                                          \
        LibosPanic();                                                     \
    }
#endif

#if LIBOS_DEBUG
#define LibosAssert(x)                                                         \
    if (!(x))                                                                  \
    {                                                                          \
        LibosPrintf("Assertion violation %s:%d %s\n", __FILE__, __LINE__, #x); \
        LibosBreakpoint();                                                     \
    }
#else
#    define LibosAssert(x)
#endif
