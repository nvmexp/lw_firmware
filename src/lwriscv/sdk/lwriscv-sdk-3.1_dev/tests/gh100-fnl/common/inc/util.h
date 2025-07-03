/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include <lwmisc.h>
#include <lwriscv/csr.h>

#if defined LWRISCV_IS_ENGINE_gsp
#include <dev_gsp.h>
#define ENGINE_PREFIX _PGSP
#define ENGINE_REG(X) LW_PGSP ## X
#define ENGINE_NAME "GSP"
#elif defined LWRISCV_IS_ENGINE_pmu
#include <dev_pwr_pri.h>
#define ENGINE_PREFIX _PPWR
#define ENGINE_REG(X) LW_PPWR ## X
#define ENGINE_NAME "PMU"
#elif defined LWRISCV_IS_ENGINE_minion
#include <dev_minion_ip.h>
#define ENGINE_PREFIX _PMINION
#define ENGINE_REG(X) LW_PMINION ## X
#define ENGINE_NAME "MINION"
#elif defined LWRISCV_IS_ENGINE_sec
#include <dev_sec_pri.h>
#define ENGINE_PREFIX _PSEC
#define ENGINE_REG(X) LW_PSEC ## X
#define ENGINE_NAME "SEC2"
#elif defined LWRISCV_IS_ENGINE_fsp
#include <dev_fsp_pri.h>
#define ENGINE_PREFIX _PFSP
#define ENGINE_REG(X) LW_PFSP ## X
#define ENGINE_NAME "FSP"
#elif defined LWRISCV_IS_ENGINE_lwdec
#include <dev_lwdec_pri.h>
#define ENGINE_PREFIX _PLWDEC
#define ENGINE_REG(X) LW_PLWDEC ## X
#define LWDEC_INSTANCE 0
#define ENGINE_NAME "LWDEC"
#endif

#define GCC_ATTRIB_ALIGN(n)             __attribute__((aligned(n)))

extern volatile uint64_t trap_mepc;
extern volatile uint64_t trap_mcause;
extern volatile uint64_t trap_mtval;
#define TRAP_CLEAR() do { \
  trap_mepc = trap_mtval = trap_mcause = 0; \
} while(0)

#define MPRV_ENABLE(MODE) do { \
    csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) | \
                                    DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV)); \
    csr_set(LW_RISCV_CSR_MSTATUS, (MODE) | \
                                  DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE)); \
    __asm__ volatile("fence.i"); \
} while(0);

#define MPRV_DISABLE() do { \
    csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) | \
                                    DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV)); \
    __asm__ volatile("fence.i"); \
} while(0);


static inline void loadAddress(uint64_t address)
{
    __asm__ volatile("ld zero, 0(%0)" : : "r"(address));
}

static inline void storeAddress(uint64_t address, uint64_t val)
{
    __asm__ volatile("sd %1, 0(%0)" :: "r"(address), "r"(val));
}

static inline void loadAddress32(uint64_t address)
{
    __asm__ volatile("lw zero, 0(%0)" : : "r"(address));
}

static inline void storeAddress32(uint64_t address, uint32_t val)
{
    __asm__ volatile("sw %1, 0(%0)" :: "r"(address), "r"(val));
}

static inline void mToS(void)
{
    uint64_t mstatus = csr_read(LW_RISCV_CSR_MSTATUS);
    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _SUPERVISOR, mstatus));
    csr_write(LW_RISCV_CSR_MEPC, &&done);
    __asm__ volatile ("mret");
    done:
    __asm__ volatile ("nop"); // Fix gcc bug

    return;
}

static inline void mToU(void)
{
    uint64_t mstatus = csr_read(LW_RISCV_CSR_MSTATUS);

    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _USER, mstatus));
    csr_write(LW_RISCV_CSR_MEPC, &&done);
    __asm__ volatile ("mret");
    done:
    __asm__ volatile ("nop"); // Fix gcc bug

    return;
}
#define sToM() do { __asm__ volatile ("ecall"); } while(0)
#define uToM() do { __asm__ volatile ("ecall"); } while(0)

#define MPRV_ENABLE(MODE) do { \
    csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) | \
                                    DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV)); \
    csr_set(LW_RISCV_CSR_MSTATUS, (MODE) | \
                                  DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE)); \
    __asm__ volatile("fence.i"); \
} while(0);

#define MPRV_DISABLE() do { \
    csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) | \
                                    DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV)); \
    __asm__ volatile("fence.i"); \
} while(0);

#define PLM_L0 LW_RISCV_CSR_MRSP_MRPL_LEVEL0
#define PLM_L2 LW_RISCV_CSR_MRSP_MRPL_LEVEL2
#define PLM_L1 LW_RISCV_CSR_MRSP_MRPL_LEVEL1
#define PLM_L3 LW_RISCV_CSR_MRSP_MRPL_LEVEL3

static inline void setPlm(int m, int s, int u)
{
    // SRSP and RSP are just copies of MRSP
    csr_write(LW_RISCV_CSR_MRSP,
              DRF_NUM(_RISCV_CSR, _MRSP, _MRPL, m) |
              DRF_NUM(_RISCV_CSR, _MRSP, _SRPL, s) |
              DRF_NUM(_RISCV_CSR, _MRSP, _URPL, u)
              );
}

static inline void setPlmAll(int l)
{
    setPlm(l, l, l);
}

#endif // UTIL_H
