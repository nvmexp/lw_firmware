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

LwU64 LibosTimeNs(void)
{
    //
    // WAR: inline version of KernelCsrRead(LW_RISCV_CSR_TIME)<<5 because
    // we can't get LW_RISCV_CSR_TIME thru a non-GPU-specific header.
    //
    // Add memory barrier to ensure no reordering here (we don't want
    // to inlwr a page fault while we're getting time value).
    //
    LwU64 val;
    __asm __volatile__("csrr %[val], time" : [ val ] "=r"(val) : : "memory");

    return LIBOS_CONFIG_TIME_TO_NS(val);
}

LibosStatus LibosTimerSet(LibosPortHandle timer, LwU64 timestamp)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TIMER_SET;
    register LwU64 a0 __asm__("a0") = timer;
    register LwU64 a1 __asm__("a1") = timestamp;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0), "r"(a1) : "memory");    
    return (LibosStatus)a0;
}

LibosStatus LibosTimerClear(LibosPortHandle timer)
{
    return LibosTimerSet(timer, LibosTimeoutInfinite);
}