/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_FENCE_H
#define LWRISCV_FENCE_H

#include <stdint.h>
#include <lwriscv/csr.h>

// Lightweight fence, may not be coherent with rest of the system

static inline void riscvLwfenceIO(void)
{
    __asm__ volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEIO));
}

static inline void riscvLwfenceRW(void)
{
    __asm__ volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEMEM));
}

static inline void riscvLwfenceRWIO(void)
{
    __asm__ volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEALL));
}

// Heavy-weight fence, mem fence may not work on dGPU with FB locked

static inline void riscvFenceIO(void)
{
    __asm__ volatile ("fence io,io");
}

static inline void riscvFenceRW(void)
{
    __asm__ volatile ("fence rw,rw");
}

static inline void riscvFenceRWIO(void)
{
    __asm__ volatile ("fence iorw,iorw");
}

static inline void riscvSfenceVMA(uint8_t asid, uint8_t vaddr)
{
    __asm__ volatile ("sfence.vma %0, %1" :: "r" (asid), "r" (vaddr));
}

#endif // LWRISCV_FENCE_H
