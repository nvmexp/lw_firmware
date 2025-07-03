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

// @see libos.h for API definition
LibosStatus LibosDmaCopy(LwU64 tcmVa, LwU64 dmaVA, LwU64 size, LwBool fromTcm)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_DMA_COPY;
    register LwU64 a0 __asm__("a0") = tcmVa;
    register LwU64 a1 __asm__("a1") = dmaVA;
    register LwU64 a2 __asm__("a2") = size;
    register LwU64 a3 __asm__("a3") = fromTcm;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0), "r"(a1), "r"(a2), "r"(a3) : "memory");
    // coverity[mixed_enum_type]
    return a0;
}

void LibosDmaFlush(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_DMA_FLUSH;
    // coverity[set_but_not_used]
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__("ecall" : "=r"(a0) : "r"(t0) : "memory");
}

#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0

LibosStatus LibosGdmaTransfer(LwU64 dstVa, LwU64 srcVa, LwU32 size, LwU64 flags)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_GDMA_TRANSFER;
    register LwU64 a0 __asm__("a0") = dstVa;
    register LwU64 a1 __asm__("a1") = srcVa;
    register LwU32 a2 __asm__("a2") = size;
    register LwU64 a3 __asm__("a3") = flags;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0), "r"(a1), "r"(a2), "r"(a3) : "memory");
    // coverity[mixed_enum_type]
    return a0;
}

LibosStatus LibosGdmaFlush(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_GDMA_FLUSH;
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__("ecall" : "=r"(a0) : "r"(t0) : "memory");
    // coverity[mixed_enum_type]
    return a0;
}

#endif
