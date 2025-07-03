/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   main.c
 * @brief  Test application main.
 */
#include <stdint.h>
#include <stdbool.h>
#include <lwmisc.h>
#include "io.h"
#include "common.h"
#include <lwriscv/csr.h>
#include <lwriscv/fence.h>

typedef enum TestErrorCode {
    TestErrorCode_Hwcfg_Incorrect = 0xFF,
    TestErrorCode_TcmRange_ValidAddressFailure = 0xFE,
    TestErrorCode_TcmRange_IlwalidAddressSucceeded = 0xFD,
} TestErrorCode;

typedef enum {
    TestId_Hwcfg,
    TestId_TcmRanges,
    TestIdCount,
} TestId;

#define GCC_ATTRIB_ALIGN(n)             __attribute__((aligned(n)))

extern volatile uint64_t trap_mepc;
extern volatile uint64_t trap_mcause;
extern volatile uint64_t trap_mtval;
#define TRAP_CLEAR() do { \
  trap_mepc = trap_mtval = trap_mcause = 0; \
} while(0)

#define MAKE_DEBUG_VALUE(CODE, IDX, EXPECTVAL, GOTVAL) (\
  ((uint64_t)(CODE) << 56) | \
  ((uint64_t)(IDX) << 40) | \
  ((uint64_t)(EXPECTVAL) << 16) | \
  ((uint64_t)(GOTVAL)) \
)

#define PASSING_DEBUG_VALUE 0xCAFECAFECAFECAFEULL

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


// configuration
#if ENGINE_sec
const uint32_t imem_size = 0x21000;
const uint32_t dmem_size = 0x15000;
const uint32_t emem_size = 0x2000;
#elif ENGINE_gsp
const uint32_t imem_size = 0x16000;
const uint32_t dmem_size = 0x10000;
const uint32_t emem_size = 0x2000;
#elif ENGINE_pmu
const uint32_t imem_size = 0x10000;
const uint32_t dmem_size = 0x10000;
const uint32_t emem_size = 0;
#elif ENGINE_lwdec
const uint32_t imem_size = 0xc000;
const uint32_t dmem_size = 0xa000;
const uint32_t emem_size = 0; // 0x2000;
#else
#error "Engine not defined."
#endif

//-------------------------------------------------------------------------------------------------
uint64_t runTestHwcfg(void)
{
    uint32_t expectedMask;

    const uint32_t expectedHwcfg3 = DRF_NUM(_PRGNLCL_FALCON, _HWCFG3, _IMEM_TOTAL_SIZE, imem_size >> 8) |
                     DRF_NUM(_PRGNLCL_FALCON, _HWCFG3, _DMEM_TOTAL_SIZE, dmem_size >> 8);

    expectedMask   = DRF_SHIFTMASK(LW_PRGNLCL_FALCON_HWCFG3_IMEM_TOTAL_SIZE) |
                     DRF_SHIFTMASK(LW_PRGNLCL_FALCON_HWCFG3_DMEM_TOTAL_SIZE);
    uint32_t hwcfg3 = localRead(LW_PRGNLCL_FALCON_HWCFG3);

    if (expectedHwcfg3 != (hwcfg3 & expectedMask))
    {
        // we cant return 2 32bit numbers :(
        return MAKE_DEBUG_VALUE(TestErrorCode_Hwcfg_Incorrect, 0, 0, hwcfg3);
    }

    return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
uint64_t runTestTcmRange(void)
{
    uint64_t validOffsets[] = {
        LW_RISCV_AMAP_IMEM_START,
        LW_RISCV_AMAP_IMEM_START + imem_size - 8,
        LW_RISCV_AMAP_DMEM_START,
        LW_RISCV_AMAP_DMEM_START + dmem_size - 8,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START : 0,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START + emem_size - 8 : 0,
        0
    };
    uint64_t ilwalidOffsets[] = {
        LW_RISCV_AMAP_IMEM_START - 8,
        LW_RISCV_AMAP_IMEM_START + imem_size,
        LW_RISCV_AMAP_DMEM_START - 8,
        LW_RISCV_AMAP_DMEM_START + dmem_size,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START - 8: 0,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START + emem_size : 0,
        0
    };
    int i = 0;
    int results = 0;

    while (validOffsets[i])
    {
        // Try reading memory
        volatile uint64_t *ptr = (volatile uint64_t *)validOffsets[i];
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        (void)ptr[0];
        riscvLwfenceRWIO();
        if (trap_mepc == 0)
            results |= (1 << i);
        else
            TRAP_CLEAR();
        i++;
    }
    if (((1 << i) - 1) != results)
        return MAKE_DEBUG_VALUE(TestErrorCode_TcmRange_ValidAddressFailure, 0, ((1 << i) - 1), results);

    i = 0;
    results = 0;

    while (ilwalidOffsets[i])
    {
        // Try reading memory
        volatile uint64_t *ptr = (volatile uint64_t *)ilwalidOffsets[i];
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        (void)ptr[0];
        riscvLwfenceRWIO();
        if ((trap_mepc != 0) && (trap_mcause == 5))
        {
            results |= (1 << i);
            TRAP_CLEAR();
        }
        i++;
    }
    if (((1 << i) - 1) != results)
        return MAKE_DEBUG_VALUE(TestErrorCode_TcmRange_IlwalidAddressSucceeded, 0, ((1 << i) - 1), results);

    return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
typedef uint64_t (*TestCallback)(void);
static const TestCallback c_test_callbacks[TestIdCount] = {
  [TestId_Hwcfg]    = runTestHwcfg,
  [TestId_TcmRanges]= runTestTcmRange,
};

//#define RUN_ONE_TEST MpuTestId_Parallel

void pseudoBrom(void)
{
  localWrite(LW_PRGNLCL_RISCV_DBGCTL, 0x1FFFF);
  csr_write(LW_RISCV_CSR_MDBGCTL, 0x1);
  csr_write(LW_RISCV_CSR_PMPADDR(0), 0x000000000004FFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(1), 0x000000000006FFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(2), 0x000000000048FFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(3), 0x00000000004AFFFF);
  csr_write(LW_RISCV_CSR_PMPADDR(4), 0x7FFFFFFFFFFFFFFF);
  csr_write(LW_RISCV_CSR_PMPCFG0, 0x0f1b1b1b1f);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(0), 0x8BBBBBBB);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(1), 0xBBB9BBBB);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(2), 0xBBBBBBBB);
  localWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(3), 0xB);
}

int main() {

  pseudoBrom();
  // configure fbif
  #ifdef LW_PRGNLCL_FBIF_CTL
  localWrite(ENGINE_REG(_FBIF_CTL), (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                                      DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
  localWrite(ENGINE_REG(_FBIF_CTL2), LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);
  #endif
  localWrite(LW_PRGNLCL_RISCV_DBGCTL, 0x1FFFF);
  csr_write(LW_RISCV_CSR_MDBGCTL, 1);
  localWrite(ENGINE_REG(_FALCON_LOCKPMB), 0);
  localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN, 0);

  printInit(512);
  printf("Testing RISC-V configuration...\n");

  // Clear trap recording values
  TRAP_CLEAR();
  bool pass = true;
  for(int i = 0; i < TestIdCount; i++) {
    uint64_t retval = c_test_callbacks[i]();
    if(retval != PASSING_DEBUG_VALUE) {
      csr_write(LW_RISCV_CSR_MSCRATCH, retval);
      pass = false;
      break;
    }
  }
  if(pass) {
    csr_write(LW_RISCV_CSR_MSCRATCH, PASSING_DEBUG_VALUE);
  }
  printf("Test finished with result 0x%llx\n", csr_read(LW_RISCV_CSR_MSCRATCH));

  localWrite(LW_PRGNLCL_RISCV_ICD_CMD, LW_PRGNLCL_RISCV_ICD_CMD_OPC_STOP);
    __asm__ volatile("fence");
  return 0;
}
