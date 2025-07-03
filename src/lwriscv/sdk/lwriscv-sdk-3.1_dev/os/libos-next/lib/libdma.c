/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "libos.h"

#if LIBOS_CONFIG_GDMA_SUPPORT

LibosStatus LibosGdmaTransfer(LwU64 dstVa, LwU64 srcVa, LwU32 size, LwU64 flags)
{
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_GDMA_TRANSFER;
#else
    extern LibosStatus KernelSyscallGdmaTransfer(LwU64 dstVa, LwU64 srcVa, LwU32 size, LwU64 flags);
    register LwU64 t0 __asm__("t0") = (LwU64)KernelSyscallGdmaTransfer;
#endif
    register LwU64 a0 __asm__("a0") = dstVa;
    register LwU64 a1 __asm__("a1") = srcVa;
    register LwU32 a2 __asm__("a2") = size;
    register LwU64 a3 __asm__("a3") = flags;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0), "r"(a1), "r"(a2), "r"(a3) : "memory");
    return a0;
}

LibosStatus LibosGdmaFlush(void)
{
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_GDMA_FLUSH;
#else
    extern LibosStatus KernelSyscallGdmaFlush();
    register LwU64 t0 __asm__("t0") = (LwU64)KernelSyscallGdmaFlush;
#endif

    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "=r"(a0) : "r"(t0) : "memory");
    return a0;
}

#endif
