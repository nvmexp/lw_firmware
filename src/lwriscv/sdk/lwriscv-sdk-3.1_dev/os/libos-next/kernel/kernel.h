/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_KERNEL_H__
#define LIBOS_KERNEL_H__

#include <lwtypes.h>
#include "libos-config.h"
#include "lwriscv.h"
#include "peregrine-headers.h"
#include <stddef.h>

// Forward defines for kernel objects
typedef struct Port Port;
typedef struct Timer Timer;
typedef struct Task Task;
typedef struct Port Port;
typedef struct Shuttle Shuttle;
typedef struct Partition Partition;
typedef struct Lock Lock;

#define LIBOS_SECTION_IMEM_PINNED       __attribute__((section(".hot.text")))
#define LIBOS_SECTION_DMEM_PINNED(name) __attribute__((section(".hot.data." #name))) name
#define LIBOS_SECTION_DMEM_COLD(name)   __attribute__((section(".cold.data." #name))) name
#define LIBOS_NORETURN                  __attribute__((noreturn, used, nothrow))
#define LIBOS_NAKED                     __attribute__((naked))
#define LIBOS_SECTION_TEXT_COLD         __attribute__((noinline, section(".cold.text")))

// Use for accessing linker symbols where the value isn't an address
// but rather a value (such as a size).
#define LIBOS_DECLARE_LINKER_SYMBOL_AS_INTEGER(x) extern LwU8 notaddr_##x[];

#define LIBOS_REFERENCE_LINKER_SYMBOL_AS_INTEGER(x) static const LwU64 x = (LwU64)notaddr_##x;

#if LIBOS_LWRISCV < LIBOS_LWRISCV_2_0
#define KernelMmioWrite(addr, value)                                                                             \
    *(volatile LwU32 *)(LIBOS_CONFIG_PRI_MAPPING_BASE + addr) = (value);
#define KernelMmioRead(addr)          (*(volatile LwU32 *)(LIBOS_CONFIG_PRI_MAPPING_BASE + addr))
#else
#define KernelMmioWrite(addr, value)                                                                             \
    *(volatile LwU32 *)(LIBOS_CONFIG_PRI_MAPPING_BASE + addr - LW_RISCV_AMAP_INTIO_START) = (value);
#define KernelMmioRead(addr)          (*(volatile LwU32 *)(LIBOS_CONFIG_PRI_MAPPING_BASE + addr - LW_RISCV_AMAP_INTIO_START))
#endif

#ifndef LIBOS_HOST

static inline LwU64 KernelCsrSaveAndClearImmediate(LwU64 csr, LwU32 mask) {
    LwU64 tmp;
    __asm__ volatile("csrrci %0, %1, %2" : "=r"(tmp) : "i"(csr), "i"(mask));
    return tmp;
}

// CSR helpers
static inline LwU64 KernelCsrRead(LwU64 csr)
{
    LwU64 tmp;
    __asm__ volatile("csrr %0, %1" : "=r"(tmp) : "i"(csr));

    return tmp;
}

static inline void KernelCsrWrite(LwU64 csr, LwU64 value)
{
    __asm__ volatile("csrw %0, %1" ::"i"(csr), "r"(value));
}

static inline void KernelCsrSet(LwU64 csr, LwU64 value) { __asm__ volatile("csrs %0, %1" ::"i"(csr), "r"(value)); }
static inline void KernelCsrClear(LwU64 csr, LwU64 value) { __asm__ volatile("csrc %0, %1" ::"i"(csr), "r"(value)); }

/*
    Global memory/io fence
        - Ensures all previous stores have completed
        - Ensures all register IO has completed.
*/
static inline void KernelFenceFull(void) 
{ 
#if LIBOS_CONFIG_HAS_FBIO
    asm volatile("fence rw,rw" : : : "memory"); 
#else
    asm volatile ("csrrw zero, %0, zero" : : "i"(LW_RISCV_CSR_LWFENCEALL));
#endif
}
#endif

#if LIBOS_FEATURE_PAGING
LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelInit(LwS64 PHDRBootVirtualToPhysicalDelta);
#else
LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelInit(); 
#endif

#define LO32(x) ((LwU32)(x))
#define HI32(x) ((LwU32)(x >> 32))

// MISRA workarounds for sign colwersion
static inline __attribute__((always_inline)) LwU32 CastUInt32(LwS32 val) { return (LwU32)val; }

static inline __attribute__((always_inline)) LwU64 CastUInt64(LwS64 val) { return (LwU64)val; }

static inline __attribute__((always_inline)) LwS32 CastInt32(LwU32 val) { return (LwS32)val; }

static inline __attribute__((always_inline)) LwU64 Widen(LwU32 val) { return val; }

static inline __attribute__((always_inline)) LwU64 KernelRiscvRegisterEncodeLwU32(LwU32 val) { return (LwU64) ((LwS64) ((LwS32) val)); }

LIBOS_SECTION_IMEM_PINNED LwU32 Log2OfPowerOf2(LwU64 pow2);

#include "libos_xcsr.h"

#if LIBOS_TINY
void KernelMpuLoad();
#endif

#endif

