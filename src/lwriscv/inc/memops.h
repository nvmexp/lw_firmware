/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef SYSLIB_MEMOPS_H
#define SYSLIB_MEMOPS_H

/*!
 * @file    syslib-memops.h
 * @brief   User & Kernel mode memory/cache flush & ilwalidate wrappers
 */

/* ------------------------ LW Includes ------------------------------------ */

#include "engine.h"

/* ------------------------ Usage ------------------------------------------ */
//
// SYS_FLUSH_MEM -  Synchronize between all memory accesses.  Pipeline will
//                  stall until:
//                  1) dcache is flushed
//                  2) SYS_MEMBAR issued to memory
//                  3) Wall pending memory requests are complete
// SYS_FLUSH_IO -   Synchronize all IO operations.
// SYS_FLUSH_ALL -  Synchronize all IO and memory operations.
// SYS_DCACHEOP -   Ilwalidate data cache
//                  [0:1] Mode (0=entire cache, 1=cache line)
//                  [2] Address mode (0=VA, 1=PA)
//                  [63:6] = Address of cache line to ilwalidate
// SYS_L2SYSILW -   Issue equivalent of GPU flush _L2_SYSMEM_ILWALIDATE
// SYS_L2PEERILW -  Issue equivalent of GPU flush _L2_PEERMEM_ILWALIDATE
// SYS_L2CLNCOMP -  Issue equivalent of GPU flush _L2_CLEAN_COMPTAGS
// SYS_L2FLHDTY -   Issue equivalent of GPU flush _L2_FLUSH_DIRTY
//
/* ------------------------ Defines ---------------------------------------- */

#ifdef LWRISCV_SDK
#include <liblwriscv/g_lwriscv_config.h>
#include <lwriscv/fence.h>
#define SYS_FLUSH_MEM()         riscvFenceRW()
#define SYS_FLUSH_IO()          riscvFenceIO()
#define SYS_FLUSH_ALL()         riscvFenceRWIO()
#define lwFenceAll()            riscvLwfenceRWIO()
#else /* LWRISCV_SDK */
#define SYS_FLUSH_MEM()         ({ __asm__ volatile("fence rw,rw"); })
#define SYS_FLUSH_IO()          ({ __asm__ volatile("fence io,io"); })
#define SYS_FLUSH_ALL()         ({ __asm__ volatile("fence iorw,iorw"); })
// Lightweight fence - useful for cases when we don't need MPU access to fbhub
#define lwFenceAll()            ({ __asm__ volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEALL)); })
#endif /* LWRISCV_SDK */

#define SYS_DCACHEOP(_addr)     csr_write(LW_RISCV_CSR_DCACHEOP, _addr)

#if 0
// TODO: TLBILW seems to cause instruction exceptions, at least on TU102
#define SYS_UTLBILWOP(_ilwdata, _ilwop)              \
    csr_write(LW_RISCV_CSR_TLBILWDATA1, _ilwdata);  \
    csr_write(LW_RISCV_CSR_TLBILWOP, _ilwop)

#define SYS_MTLBILWOP(_ilwdata, _ilwop)              \
    csr_write(LW_RISCV_CSR_MTLBILWDATA1, _ilwdata); \
    csr_write(LW_RISCV_CSR_MTLBILWOP, _ilwop)
#endif  //0

//
// These memops are only available in user mode *iff* explicitly enabled.
// A more rigid implementation could check LW_RISCV_CSR_MSYSOPEN_*
// and instead make a syscall.
//
#define SYS_L2SYSILW()          csr_write(LW_RISCV_CSR_L2SYSILW, 0x0)
#define SYS_L2PEERILW()         csr_write(LW_RISCV_CSR_L2PEERILW, 0x0)
#define SYS_L2CLNCOMP()         csr_write(LW_RISCV_CSR_L2CLNCOMP, 0x0)
#define SYS_L2FLHDTY()          csr_write(LW_RISCV_CSR_L2FLHDTY, 0x0)

#endif  // SYSLIB_MEMOPS_H

