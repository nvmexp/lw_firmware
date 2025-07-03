/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_KERNEL_H__
#define LIBOS_KERNEL_H__

#include <sdk/lwpu/inc/lwtypes.h>
#include <stddef.h>

// Core RISC-V defines
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"

// LibOS Configuration (based on target)
#include "libos-config.h"

#include "bootstrap.h"

#define LIBOS_SECTION_IMEM_PINNED       __attribute__((section(".tcm.text")))
#define LIBOS_SECTION_DMEM_PINNED(name) __attribute__((section(".tcm.data." #name))) name
#define LIBOS_SECTION_TEXT_STARTUP      __attribute__((section(".text.startup")))
#define LIBOS_SECTION_KERNEL_BOOTSTRAP  __attribute__((section(".kernel_bootstrap")))
#define LIBOS_NORETURN                  __attribute__((noreturn, used))
#define LIBOS_NAKED                     __attribute__((naked))
#define LIBOS_SECTION_TEXT_COLD         __attribute__((noinline, section(".text.cold")))

// Pointers inside the ELF are guaranteed by medlow memory model (and linker asserts)
// to be below 32-bit
#define LIBOS_PTR32(x)           LwU32
#define LIBOS_PTR32_EXPAND(x, y) ((x *)((LwU64)(y)))
#define LIBOS_PTR32_NARROW(y)    ((LwU32)((LwU64)(y)))

// Use for accessing linker symbols where the value isn't an address
// but rather a value (such as a size).
#define LIBOS_DECLARE_LINKER_SYMBOL_AS_INTEGER(x) extern LwU8 notaddr_##x[];

#define LIBOS_REFERENCE_LINKER_SYMBOL_AS_INTEGER(x) static const LwU64 x = (LwU64)notaddr_##x;

#define PEREGRINE_EXTIO_PRI_BASE_VA 0x300000000ULL

// TODO: Move falcon memory map into addendum manuals
#define PEREGRINE_OFFSET 0x1000ULL
#define FBIF_OFFSET      0x600ULL

#define riscv_write(addr, value)                                                                             \
    *(volatile LwU32 *)(PEREGRINE_EXTIO_PRI_BASE_VA + PEREGRINE_OFFSET + (addr)) = (value);
#define riscv_read(addr)          (*(volatile LwU32 *)(PEREGRINE_EXTIO_PRI_BASE_VA + PEREGRINE_OFFSET + (addr)))
#define falcon_write(addr, value) *(volatile LwU32 *)(PEREGRINE_EXTIO_PRI_BASE_VA + (addr)) = (value);
#define falcon_read(addr)         (*(volatile LwU32 *)(PEREGRINE_EXTIO_PRI_BASE_VA + (addr)))
#define fbif_write(addr, value)   *(volatile LwU32 *)(PEREGRINE_EXTIO_PRI_BASE_VA + FBIF_OFFSET + (addr)) = (value);
#define fbif_read(addr)           (*(volatile LwU32 *)(PEREGRINE_EXTIO_PRI_BASE_VA + FBIF_OFFSET + (addr)))

#if defined(LWRISCV_VERSION) && LWRISCV_VERSION >= LWRISCV_2_2
// These should only be used where the INTIO range exists (LWRISCV 2.0+)
// However, we only map it in GH100 (LWRISCV 2.2)
#define PEREGRINE_INTIO_PRI_BASE_VA 0x380000000ULL
#define prgnlcl_write(addr, value) \
    *(volatile LwU32 *)(PEREGRINE_INTIO_PRI_BASE_VA + ((addr) - LW_RISCV_AMAP_INTIO_START)) = (value)
#define prgnlcl_read(addr) \
    (*(volatile LwU32 *)(PEREGRINE_INTIO_PRI_BASE_VA + ((addr) - LW_RISCV_AMAP_INTIO_START)))
#endif

// Internal APIs
void kernel_report_interrupt(LwU32 interruptMask);

// CSR helpers
static inline LwU64 csr_read(LwU64 csr)
{
    LwU64 tmp;
    __asm__ volatile("csrr %0, %1" : "=r"(tmp) : "i"(csr));

    return tmp;
}

static inline void csr_write(LwU64 csr, LwU64 value)
{
    __asm__ volatile("csrw %0, %1" ::"i"(csr), "r"(value));
}

static inline void csr_set(LwU64 csr, LwU64 value) { __asm__ volatile("csrs %0, %1" ::"i"(csr), "r"(value)); }
static inline void csr_clear(LwU64 csr, LwU64 value) { __asm__ volatile("csrc %0, %1" ::"i"(csr), "r"(value)); }

static inline void fence_rw_rw(void) { asm volatile("fence rw,rw" : : : "memory"); }

// Main kernel entry point for chipmux
LIBOS_NORETURN void libos_start(libos_bootstrap_args_t *args);

// Per chip entry point for kernel
LIBOS_NORETURN LIBOS_SECTION_KERNEL_BOOTSTRAP LIBOS_NAKED void kernel_bootstrap(
  libos_bootstrap_args_t *args);

LIBOS_NORETURN void kernel_init(
  libos_bootstrap_args_t *args,
  LwS64 phdr_boot_virtual_to_physical_delta);

LIBOS_NORETURN void panic_kernel(void);

#define REF_SHIFT64(drf)        ((1==0?drf)% 64u)
#define REF_MASK64(drf)         (0xFFFFFFFFFFFFFFFFULL>>(63u-((1==1?drf) % 64u)+REF_SHIFT64(drf)))
#define REF_NUM64(drf, n) (((LwU64)(n)&REF_MASK64(drf)) << REF_SHIFT64(drf))
#define REF_VAL64(drf, v)        ((((LwU64)(v))>>(1==0?drf))&REF_MASK64(drf))
#define REF_DEF64(drf, d)    ((LwU64)(drf##d) << REF_SHIFT64(drf))
#define REF_SHIFTMASK64(drf) (REF_MASK64(drf) << REF_SHIFT64(drf))

#define REF_SHIFT(drf)        ((1==0?drf)% 32u)
#define REF_MASK(drf)         (0xFFFFFFFFU>>(31u-((1==1?drf) % 32u)+REF_SHIFT(drf)))
#define REF_NUM(drf, n) (((LwU32)(n)&REF_MASK(drf)) << REF_SHIFT(drf))
#define REF_VAL(drf, v)        ((((LwU64)(v))>>(1==0?drf)&REF_MASK(drf)))
#define REF_DEF(drf, d)    ((LwU32)(drf##d) << REF_SHIFT(drf))
#define REF_SHIFTMASK(drf) (REF_MASK(drf) << REF_SHIFT(drf))

#define LO32(x) ((LwU32)(x))
#define HI32(x) ((LwU32)(x >> 32))

#define IS_ALIGNED(v, gran) (0u == ((v) & ((gran) - 1u)))

typedef LwU32 kernel_handle;

// Support for suffices
#define U(x)   (x##U)
#define ULL(x) (x##ULL)

// MISRA workarounds for sign colwersion
static inline __attribute__((always_inline)) LwU32 to_uint32(LwS32 val) { return (LwU32)val; }

static inline __attribute__((always_inline)) LwU64 to_uint64(LwS64 val) { return (LwU64)val; }

static inline __attribute__((always_inline)) LwS32 to_int32(LwU32 val) { return (LwS32)val; }

static inline __attribute__((always_inline)) LwU64 widen(LwU32 val) { return val; }

static inline __attribute__((always_inline)) LwU64 abs(LwS64 v) { return v >= 0 ? v : -v; }

typedef struct
{
    LIBOS_PTR32(list_header_t) next, prev;
} list_header_t;

LIBOS_SECTION_IMEM_PINNED LwU32 pow2_to_log(LwU64 pow2);

#ifdef DEBUG
#    define LIBOS_ASSERT(x)                                                                                  \
        do                                                                                                   \
        {                                                                                                    \
            if (!(x))                                                                                        \
            {                                                                                                \
                panic_kernel();                                                                              \
            }                                                                                                \
        } while (0);
#else
#    define LIBOS_ASSERT(x)
#endif

#ifdef LWRISCV_VERSION
    // M-mode to S-mode CSR changes (only when a target is specified)
    #if LIBOS_CONFIG_RISCV_S_MODE
        #define LW_RISCV_CSR_XMPUIDX                   LW_RISCV_CSR_SMPUIDX
        #define LW_RISCV_CSR_XMPUVA                    LW_RISCV_CSR_SMPUVA
        #define LW_RISCV_CSR_XMPUPA                    LW_RISCV_CSR_SMPUPA
        #define LW_RISCV_CSR_XMPUATTR                  LW_RISCV_CSR_SMPUATTR
        #define LW_RISCV_CSR_XMPURNG                   LW_RISCV_CSR_SMPURNG
        #define LW_RISCV_CSR_XMPUATTR_XW               LW_RISCV_CSR_SMPUATTR_SW
        #define LW_RISCV_CSR_XMPUATTR_UW               LW_RISCV_CSR_SMPUATTR_UW
        #define LW_RISCV_CSR_XMPUATTR_XR               LW_RISCV_CSR_SMPUATTR_SR
        #define LW_RISCV_CSR_XMPUATTR_UR               LW_RISCV_CSR_SMPUATTR_UR
        #define LW_RISCV_CSR_XMPUATTR_XX               LW_RISCV_CSR_SMPUATTR_SX
        #define LW_RISCV_CSR_XMPUATTR_UX               LW_RISCV_CSR_SMPUATTR_UX
        #define LW_RISCV_CSR_XMPUATTR_COHERENT         LW_RISCV_CSR_SMPUATTR_COHERENT
        #define LW_RISCV_CSR_XMPUATTR_CACHEABLE        LW_RISCV_CSR_SMPUATTR_CACHEABLE
        #define LW_RISCV_CSR_XMPUATTR_WPR              LW_RISCV_CSR_SMPUATTR_WPR
        #define LW_RISCV_CSR_XEPC                      LW_RISCV_CSR_SEPC
        #define LW_RISCV_CSR_XTVEC                     LW_RISCV_CSR_STVEC
        #define LW_RISCV_CSR_XCAUSE                    LW_RISCV_CSR_SCAUSE
        #define LW_RISCV_LAST_EXCEPTION                LW_RISCV_CSR_SCAUSE_EXCODE_USS
        #define LW_RISCV_LAST_INTERRUPT                LW_RISCV_CSR_SCAUSE_EXCODE_S_EINT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_IACC_FAULT  LW_RISCV_CSR_SCAUSE_EXCODE_IACC_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_IAMA        LW_RISCV_CSR_SCAUSE_EXCODE_IAMA
        #define LW_RISCV_CSR_XCAUSE_EXCODE_ILL         LW_RISCV_CSR_SCAUSE_EXCODE_ILL
        #define LW_RISCV_CSR_XCAUSE_EXCODE_BKPT        LW_RISCV_CSR_SCAUSE_EXCODE_BKPT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_LPAGE_FAULT LW_RISCV_CSR_SCAUSE_EXCODE_LPAGE_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_SPAGE_FAULT LW_RISCV_CSR_SCAUSE_EXCODE_SPAGE_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT  LW_RISCV_CSR_SCAUSE_EXCODE_LACC_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT  LW_RISCV_CSR_SCAUSE_EXCODE_SACC_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_LAMA        LW_RISCV_CSR_SCAUSE_EXCODE_LAMA
        #define LW_RISCV_CSR_XCAUSE_EXCODE_SAMA        LW_RISCV_CSR_SCAUSE_EXCODE_SAMA
        #define LW_RISCV_CSR_XCAUSE_EXCODE_X_TINT      LW_RISCV_CSR_SCAUSE_EXCODE_S_TINT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_UCALL       LW_RISCV_CSR_SCAUSE_EXCODE_UCALL
        #define LW_RISCV_CSR_XSTATUS                   LW_RISCV_CSR_SSTATUS
        #define LW_RISCV_CSR_XSTATUS_XPP               LW_RISCV_CSR_SSTATUS_SPP
        #define LW_RISCV_CSR_XSTATUS_XPP_USER          LW_RISCV_CSR_SSTATUS_SPP_USER
        #define LW_RISCV_CSR_XSTATUS_XPIE              LW_RISCV_CSR_SSTATUS_SPIE
        #define LW_RISCV_CSR_XSTATUS_XPIE_ENABLE       LW_RISCV_CSR_SSTATUS_SPIE_ENABLE
        #define LW_RISCV_CSR_XCOUNTEREN                LW_RISCV_CSR_SCOUNTEREN
        #define LW_RISCV_CSR_XCOUNTEREN_TM             LW_RISCV_CSR_SCOUNTEREN_TM
        #define LW_RISCV_CSR_XCOUNTEREN_CY             LW_RISCV_CSR_SCOUNTEREN_CY
        #define LW_RISCV_CSR_XIP                       LW_RISCV_CSR_SIP
        #define LW_RISCV_CSR_XIP_XEIP                  LW_RISCV_CSR_SIP_SEIP
        #define LW_RISCV_CSR_XIE                       LW_RISCV_CSR_SIE
        #define LW_RISCV_CSR_XIE_XTIE                  LW_RISCV_CSR_SIE_STIE
        #define LW_RISCV_CSR_XIE_XEIE                  LW_RISCV_CSR_SIE_SEIE
        #define LW_RISCV_CSR_XDCACHEOP                 LW_RISCV_CSR_DCACHEOP
        #define LW_RISCV_CSR_XDCACHEOP_MODE            LW_RISCV_CSR_DCACHEOP_MODE
        #define LW_RISCV_CSR_XDCACHEOP_MODE_ILW_LINE   LW_RISCV_CSR_DCACHEOP_MODE_ILW_LINE
        #define LW_RISCV_CSR_XDCACHEOP_MODE_ILW_ALL    LW_RISCV_CSR_DCACHEOP_MODE_ILW_ALL
        #define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE       LW_RISCV_CSR_DCACHEOP_ADDR_MODE
        #define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE_VA    LW_RISCV_CSR_DCACHEOP_ADDR_MODE_VA
        #define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE_PA    LW_RISCV_CSR_DCACHEOP_ADDR_MODE_PA
        #define LW_RISCV_CSR_CYCLE                     LW_RISCV_CSR_HPMCOUNTER_CYCLE
    #else // LIBOS_CONFIG_RISCV_S_MODE
        #define LW_RISCV_CSR_XMPUIDX                   LW_RISCV_CSR_MMPUIDX
        #define LW_RISCV_CSR_XMPUVA                    LW_RISCV_CSR_MMPUVA
        #define LW_RISCV_CSR_XMPUPA                    LW_RISCV_CSR_MMPUPA
        #define LW_RISCV_CSR_XMPURNG                   LW_RISCV_CSR_MMPURNG
        #define LW_RISCV_CSR_XMPUATTR                  LW_RISCV_CSR_MMPUATTR
        #define LW_RISCV_CSR_XMPUATTR_XW               LW_RISCV_CSR_MMPUATTR_MW
        #define LW_RISCV_CSR_XMPUATTR_UW               LW_RISCV_CSR_MMPUATTR_UW
        #define LW_RISCV_CSR_XMPUATTR_XR               LW_RISCV_CSR_MMPUATTR_MR
        #define LW_RISCV_CSR_XMPUATTR_UR               LW_RISCV_CSR_MMPUATTR_UR
        #define LW_RISCV_CSR_XMPUATTR_XX               LW_RISCV_CSR_MMPUATTR_MX
        #define LW_RISCV_CSR_XMPUATTR_UX               LW_RISCV_CSR_MMPUATTR_UX
        #define LW_RISCV_CSR_XMPUATTR_COHERENT         LW_RISCV_CSR_MMPUATTR_COHERENT
        #define LW_RISCV_CSR_XMPUATTR_CACHEABLE        LW_RISCV_CSR_MMPUATTR_CACHEABLE
        #define LW_RISCV_CSR_XMPUATTR_WPR              LW_RISCV_CSR_MMPUATTR_WPR
        #define LW_RISCV_CSR_XEPC                      LW_RISCV_CSR_MEPC
        #define LW_RISCV_CSR_XTVEC                     LW_RISCV_CSR_MTVEC
        #define LW_RISCV_CSR_XCAUSE                    LW_RISCV_CSR_MCAUSE
        #define LW_RISCV_LAST_EXCEPTION                LW_RISCV_CSR_MCAUSE_EXCODE_SSS
        #define LW_RISCV_LAST_INTERRUPT                LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_IACC_FAULT  LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_IAMA        LW_RISCV_CSR_MCAUSE_EXCODE_IAMA
        #define LW_RISCV_CSR_XCAUSE_EXCODE_ILL         LW_RISCV_CSR_MCAUSE_EXCODE_ILL
        #define LW_RISCV_CSR_XCAUSE_EXCODE_BKPT        LW_RISCV_CSR_MCAUSE_EXCODE_BKPT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_LPAGE_FAULT LW_RISCV_CSR_MCAUSE_EXCODE_LPAGE_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_SPAGE_FAULT LW_RISCV_CSR_MCAUSE_EXCODE_SPAGE_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_LACC_FAULT  LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_SACC_FAULT  LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_LAMA        LW_RISCV_CSR_MCAUSE_EXCODE_LAMA
        #define LW_RISCV_CSR_XCAUSE_EXCODE_SAMA        LW_RISCV_CSR_MCAUSE_EXCODE_SAMA
        #define LW_RISCV_CSR_XCAUSE_EXCODE_X_TINT      LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT
        #define LW_RISCV_CSR_XCAUSE_EXCODE_UCALL       LW_RISCV_CSR_MCAUSE_EXCODE_UCALL
        #define LW_RISCV_CSR_XSTATUS                   LW_RISCV_CSR_MSTATUS
        #define LW_RISCV_CSR_XSTATUS_XPP               LW_RISCV_CSR_MSTATUS_MPP
        #define LW_RISCV_CSR_XSTATUS_XPP_USER          LW_RISCV_CSR_MSTATUS_MPP_USER
        #define LW_RISCV_CSR_XSTATUS_XPIE              LW_RISCV_CSR_MSTATUS_MPIE
        #define LW_RISCV_CSR_XSTATUS_XPIE_ENABLE       LW_RISCV_CSR_MSTATUS_MPIE_ENABLE
        #define LW_RISCV_CSR_XCOUNTEREN                LW_RISCV_CSR_MUCOUNTEREN
        #define LW_RISCV_CSR_XCOUNTEREN_TM             LW_RISCV_CSR_MUCOUNTEREN_TM
        #define LW_RISCV_CSR_XCOUNTEREN_CY             LW_RISCV_CSR_MUCOUNTEREN_CY
        #define LW_RISCV_CSR_XIP                       LW_RISCV_CSR_MIP
        #define LW_RISCV_CSR_XIP_XEIP                  LW_RISCV_CSR_MIP_MEIP
        #define LW_RISCV_CSR_XIE                       LW_RISCV_CSR_MIE
        #define LW_RISCV_CSR_XIE_XTIE                  LW_RISCV_CSR_MIE_MTIE
        #define LW_RISCV_CSR_XIE_XEIE                  LW_RISCV_CSR_MIE_MEIE
        #define LW_RISCV_CSR_XDCACHEOP                 LW_RISCV_CSR_MDCACHEOP
        #define LW_RISCV_CSR_XDCACHEOP_MODE            LW_RISCV_CSR_MDCACHEOP_MODE
        #define LW_RISCV_CSR_XDCACHEOP_MODE_ILW_LINE   LW_RISCV_CSR_MDCACHEOP_MODE_ILW_LINE
        #define LW_RISCV_CSR_XDCACHEOP_MODE_ILW_ALL    LW_RISCV_CSR_MDCACHEOP_MODE_ILW_ALL
        #define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE       LW_RISCV_CSR_MDCACHEOP_ADDR_MODE
        #define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE_VA    LW_RISCV_CSR_MDCACHEOP_ADDR_MODE_VA
        #define LW_RISCV_CSR_XDCACHEOP_ADDR_MODE_PA    LW_RISCV_CSR_MDCACHEOP_ADDR_MODE_PA
    #endif // LIBOS_CONFIG_RISCV_S_MODE
#endif // LWRISCV_VERSION

#endif
