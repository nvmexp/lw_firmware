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
#include <lwriscv/csr.h>

typedef enum TestErrorCode {
  TestErrorCode_EntryCount_UnexpectedException = 0xFF,
  TestErrorCode_EntryCount_DataMismatch = 0xFE,
  TestErrorCode_EntryCount_SMPUATTRMismatch = 0xFD,
  TestErrorCode_EntryCount_SMPURNGMismatch = 0xFC,
  TestErrorCode_EntryCount_SMPUPAMismatch = 0xFB,
  TestErrorCode_EntryCount_SMPUVAMismatch = 0xFA,
  TestErrorCode_Permission_Mismatch = 0xF9,
  TestErrorCode_TlbIlwalidate_PerformanceMismatch = 0xF8,
  TestErrorCode_WfpDis_PerformanceMismatch = 0xF7,
  TestErrorCode_Parallel_UnexpectedException = 0xF6,
  TestErrorCode_Parallel_DataMismatch = 0xF5,
  TestErrorCode_Parallel_PerformanceMismatch = 0xF4,
  TestErrorCode_VldDtyAcc_UnexpectedException = 0xF3,
  TestErrorCode_VldDtyAcc_DataMismatch = 0xF2,
  TestErrorCode_VldDtyAcc_DirtyMismatch = 0xF1,
  TestErrorCode_VldDtyAcc_AccessMismatch = 0xF0,
  TestErrorCode_Virtualized_UnexpectedException = 0xEF,
  TestErrorCode_Virtualized_MissedException = 0xEE,
  TestErrorCode_Virtualized_DataMismatch = 0xED,
} TestErrorCode;

typedef enum MpuTestId {
  MpuTestId_EntryCount,
  MpuTestId_Permission,
  MpuTestId_TlbIlwalidate,
  MpuTestId_WfpDis,
  MpuTestId_Parallel,
  MpuTestId_VldDtyAcc,
  MpuTestId_Virtualized,
  MpuTestIdCount,
} MpuTestId;

#define GCC_ATTRIB_ALIGN(n)             __attribute__((aligned(n)))

#define EXPECTED_MPU_IDX_COUNT LW_RISCV_CSR_MMPUCTL_ENTRY_COUNT_RST
// 4-way search
#define EXPECTED_MPU_PARALLELIZATION 4
#define EXPECTED_MPU_PARALLEL_SIZE (EXPECTED_MPU_IDX_COUNT / EXPECTED_MPU_PARALLELIZATION)

extern volatile uint64_t trap_mepc;
extern volatile uint64_t trap_mcause;
extern volatile uint64_t trap_mtval;
#define TRAP_CLEAR() do { \
  trap_mepc = trap_mtval = trap_mcause = 0; \
} while(0)

bool is_fmodel;
uint8_t g_test_data[0x1000] GCC_ATTRIB_ALIGN(0x1000);

#define MAKE_DEBUG_VALUE(CODE, IDX, EXPECTVAL, GOTVAL) (\
  ((uint64_t)(CODE) << 56) | \
  ((uint64_t)(IDX) << 40) | \
  ((uint64_t)(EXPECTVAL) << 16) | \
  ((uint64_t)(GOTVAL)) \
)

#define PASSING_DEBUG_VALUE 0xCAFECAFECAFECAFEULL

#define FILL_TEST_DATA() do { \
  for(uint64_t i = 0; i < sizeof(g_test_data); i++) { \
    g_test_data[i] = i ^ (i >> 6) ^ (i >> 12); \
  } \
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
//-------------------------------------------------------------------------------------------------
// Test that we have the expected number of MPU entries and that they work
uint64_t runTestEntryCount(void) {
  uint64_t addr = 0x1000000000ULL;// Theoretically a good unused address
  uint64_t va = addr | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  uint64_t pa = (uintptr_t)g_test_data;
  uint64_t attr = DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, 1);
  uint64_t rng = sizeof(g_test_data);

  FILL_TEST_DATA();
  volatile uint8_t *ptr = (volatile uint8_t*)addr;

  for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) {
    csr_write(LW_RISCV_CSR_SMPUIDX, i);
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
  }

  for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) {
    csr_write(LW_RISCV_CSR_SMPUIDX, i);
    csr_write(LW_RISCV_CSR_SMPUATTR, attr);
    csr_write(LW_RISCV_CSR_SMPURNG, rng);
    csr_write(LW_RISCV_CSR_SMPUPA, pa);
    csr_write(LW_RISCV_CSR_SMPUVA, va);

    MPRV_ENABLE(DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR));
    register uint64_t read_value __asm__("a7") = ptr[i];
    MPRV_DISABLE();
    if(trap_mepc != 0) {
      return MAKE_DEBUG_VALUE(TestErrorCode_EntryCount_UnexpectedException, i,
                              trap_mcause, trap_mtval);
    }
    if(read_value != g_test_data[i]) {
      return MAKE_DEBUG_VALUE(TestErrorCode_EntryCount_DataMismatch, i,
                              g_test_data[i], read_value);
    }

    // Disable entry
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
  }

  // Fill up with callwlated garbage and then verify them
  for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) {
    uint64_t val;

    csr_write(LW_RISCV_CSR_SMPUIDX, i);

    val = ((i << 6) ^ (i << 4) ^ i) & ~(DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUATTR_WPRI0) |
                                        DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUATTR_WPRI1) |
                                        DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUATTR_WPRI2));
    csr_write(LW_RISCV_CSR_SMPUATTR, val);
    val = ((i << 7) ^ (i << 4) ^ i) & ~DRF_SHIFTMASK64(LW_RISCV_CSR_SMPURNG_WPRI);
    csr_write(LW_RISCV_CSR_SMPURNG, val);
    val = ((i << 5) ^ (i << 4) ^ (i >> 1)) & ~DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUPA_WPRI);
    csr_write(LW_RISCV_CSR_SMPUPA, val);
    val = ((i << 14) ^ (i << 12) ^ (i << 10)) & ~(DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUVA_WPRI) |
                                                  DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUVA_WPRI1));
    csr_write(LW_RISCV_CSR_SMPUVA, val);
  }

  for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) {
    uint64_t val;
    csr_write(LW_RISCV_CSR_SMPUIDX, i);

    val = ((i << 6) ^ (i << 4) ^ i) & ~(DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUATTR_WPRI0) |
                                        DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUATTR_WPRI1) |
                                        DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUATTR_WPRI2));
    if(csr_read(LW_RISCV_CSR_SMPUATTR) != val)
      return MAKE_DEBUG_VALUE(TestErrorCode_EntryCount_SMPUATTRMismatch, i,
                              val, csr_read(LW_RISCV_CSR_SMPUATTR));

    val = ((i << 7) ^ (i << 4) ^ i) & ~DRF_SHIFTMASK64(LW_RISCV_CSR_SMPURNG_WPRI);
    if(csr_read(LW_RISCV_CSR_SMPURNG) != val)
      return MAKE_DEBUG_VALUE(TestErrorCode_EntryCount_SMPURNGMismatch, i,
                              val, csr_read(LW_RISCV_CSR_SMPURNG));

    val = ((i << 5) ^ (i << 4) ^ (i >> 1)) & ~DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUPA_WPRI);
    if(csr_read(LW_RISCV_CSR_SMPUPA) != val)
      return MAKE_DEBUG_VALUE(TestErrorCode_EntryCount_SMPUPAMismatch, i,
                              val, csr_read(LW_RISCV_CSR_SMPUPA));

    val = ((i << 14) ^ (i << 12) ^ (i << 10)) & ~(DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUVA_WPRI) |
                                                  DRF_SHIFTMASK64(LW_RISCV_CSR_SMPUVA_WPRI1));
    if(csr_read(LW_RISCV_CSR_SMPUVA) != val)
      return MAKE_DEBUG_VALUE(TestErrorCode_EntryCount_SMPUVAMismatch, i,
                              val >> 10, csr_read(LW_RISCV_CSR_SMPUVA) >> 10);
  }
  return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
// Test that supervisor read/write and user read/write are enforced properly
#define PERMISSIONS_TEST_BIT_COUNT 4
static const uint64_t c_permissions_table[PERMISSIONS_TEST_BIT_COUNT] = {
  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, 1),
  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, 1),
  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1),
  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1),
};

#define PERMISSIONS_TEST_PRIVMODE_COUNT 2
static const uint64_t c_privmode_table[PERMISSIONS_TEST_PRIVMODE_COUNT] = {
  DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _USER),
  DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR),
};

uint64_t runTestPermission(void) {
  uint64_t addr = 0x1000000000ULL;// Theoretically a good unused address
  uint64_t va = addr | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  uint64_t pa = (uintptr_t)g_test_data;
  uint64_t rng = sizeof(g_test_data);

  FILL_TEST_DATA();
  volatile uint8_t *ptr = (volatile uint8_t*)addr;

  for(int i = 0; i < (1 << PERMISSIONS_TEST_BIT_COUNT); i++) {
    uint64_t attr = 0;
    for(int l = 0; l < PERMISSIONS_TEST_BIT_COUNT; l++) {
      if(i & (1 << l)) {
        attr |= c_permissions_table[l];
      }
    }
    csr_write(LW_RISCV_CSR_SMPUIDX, 0);
    csr_write(LW_RISCV_CSR_SMPUATTR, attr);
    csr_write(LW_RISCV_CSR_SMPURNG, rng);
    csr_write(LW_RISCV_CSR_SMPUPA, pa);
    csr_write(LW_RISCV_CSR_SMPUVA, va);

    int handled = 0;
    for(int l = 0; l < PERMISSIONS_TEST_PRIVMODE_COUNT; l++) {
      // Check reading
      MPRV_ENABLE(c_privmode_table[l]);
      (void)ptr[i];
      MPRV_DISABLE();
      if(trap_mepc == 0) {
        handled |= 1 << (l << 1);
      }else{
        TRAP_CLEAR();
      }

      // Check writing
      MPRV_ENABLE(c_privmode_table[l]);
      ptr[i] = i;
      MPRV_DISABLE();
      if(trap_mepc == 0) {
        handled |= 2 << (l << 1);
      }else{
        TRAP_CLEAR();
      }
    }

    if(handled != i) {
      return MAKE_DEBUG_VALUE(TestErrorCode_Permission_Mismatch, i, i, handled);
    }

    // Disable entry
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
  }
  return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
// Test that TLB ilwalidation works on all expected instructions
#define TLBILWALIDATE_SAMPLE_ACCESS() do { \
  __asm__ volatile( \
    /* Check time to refresh TLB */ \
    " fence.i\n" \
    " csrr  %[start_cycle], cycle\n" \
    " lbu   t0, 0(%[ptr])\n" \
    " fence.i\n" \
    " csrr  %[end_cycle], cycle\n" \
    " sub   %[miss_time], %[end_cycle], %[start_cycle]\n" \
    " fence.i\n" \
    /* Check after TLB was refreshed */ \
    " csrr  %[start_cycle], cycle\n" \
    " lbu   t0, 0(%[ptr])\n" \
    " fence.i\n" \
    " csrr  %[end_cycle], cycle\n" \
    " sub   t0, %[end_cycle], %[start_cycle]\n" \
    " ble   %[miss_time], %[bench], 1f\n" \
    " ble   t0, %[bench], 2f\n" \
    " ori   %[failed_idx], %[failed_idx], 0x10\n" \
    "1:\n" \
    " ori   %[failed_idx], %[failed_idx], 0x80\n" /* indicate failure */\
    "2:\n" \
    : [start_cycle]"+r"(start_cycle), \
      [end_cycle]"+r"(end_cycle), \
      [ptr]"+r"(ptr), \
      [miss_time]"+r"(miss_time), \
      [bench]"+r"(bench), \
      [failed_idx]"+r"(failed_idx) \
    : \
    : "t0"); \
  if(failed_idx & 0x80) { \
    failed_idx &= ~0x80; \
    goto _done; \
  } \
} while(0)

uint64_t runTestTlbIlwalidate(void) {
  // TLB doesn't seem to be modeled in FMODEL
  if(is_fmodel) {
    return PASSING_DEBUG_VALUE;
  }

  uint64_t addr = 0x1000000000ULL;// Theoretically a good unused address
  uint64_t va = addr | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  uint64_t pa = (uintptr_t)g_test_data;
  uint64_t attr = DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, 1);
  uint64_t rng = sizeof(g_test_data);

  FILL_TEST_DATA();
  volatile uint8_t *ptr = (volatile uint8_t*)addr;

  for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) { // Clear all MPU entries
    csr_write(LW_RISCV_CSR_SMPUIDX, i);
    csr_write(LW_RISCV_CSR_SMPUATTR, 0);
    csr_write(LW_RISCV_CSR_SMPURNG, 0);
    csr_write(LW_RISCV_CSR_SMPUPA, 0);
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
  }

  csr_write(LW_RISCV_CSR_SMPUIDX, EXPECTED_MPU_IDX_COUNT - 1); // Use the slowest-to-read entry
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, rng);
  csr_write(LW_RISCV_CSR_SMPUPA, pa);
  csr_write(LW_RISCV_CSR_SMPUVA, va);

  csr_write(LW_RISCV_CSR_SMPUIDX, 0); // Move to an unused MPU index for free thrashing

  register uint64_t start_cycle __asm__("a7");
  register uint64_t end_cycle __asm__("a6");
  register uint64_t bench __asm__("a5");
  register uint64_t miss_time __asm__("a3");
  register uint64_t failed_idx __asm__("a4") = 0;

  MPRV_ENABLE(DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR));
  // Access to load dTLB (to make the benchmark equal the TLB-loaded time)
  __asm__ volatile(
    /* Make sure the pointer is in the TLB before we sample it */
    " lbu   t0, 0(%[ptr])\n"
    " fence.i\n"
    " csrr  %[start_cycle], cycle\n"
    " lbu   t0, 0(%[ptr])\n"
    " fence.i\n"
    " csrr  %[end_cycle], cycle\n"
    : [start_cycle]"+r"(start_cycle),
      [end_cycle]"+r"(end_cycle),
      [ptr]"+r"(ptr)
    :
    : "t0");
  bench = end_cycle - start_cycle;

  // Check sfence.vma
  __asm__ volatile("sfence.vma");
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();
  // Check smpuva
  csr_write(LW_RISCV_CSR_SMPUVA, 0xDEADFACE);
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();
  // Check smpupa
  csr_write(LW_RISCV_CSR_SMPUPA, 0xF007BA11);
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();
  // Check smpurng
  csr_write(LW_RISCV_CSR_SMPURNG, 0xBADFEE7);
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();
  // Check smpuattr
  csr_write(LW_RISCV_CSR_SMPUATTR, 0xCAFED00D);
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();
  // Check mldstattr
  csr_write(LW_RISCV_CSR_MLDSTATTR, DRF_NUM64(_RISCV_CSR, _MLDSTATTR, _COHERENT, 1));
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();
  // Check mfetchattr
  csr_write(LW_RISCV_CSR_MFETCHATTR, DRF_NUM64(_RISCV_CSR, _MFETCHATTR, _COHERENT, 1));
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();
  // Check satp
  csr_set(LW_RISCV_CSR_SATP, DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _LWMPU));
  failed_idx++;
  TLBILWALIDATE_SAMPLE_ACCESS();

  failed_idx = 0;
_done:
  MPRV_DISABLE();

  if(failed_idx != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_TlbIlwalidate_PerformanceMismatch, failed_idx,
                            bench, (miss_time << 8) | (end_cycle - start_cycle));
  }

  return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
// Test that *WFP_DIS bits work as expected
uint64_t runTestWfpDis(void) {
  // The pipeline flush cost isn't visible on fmodel
  if(is_fmodel) {
    return PASSING_DEBUG_VALUE;
  }

  register uint64_t start_cycle __asm__("a7");
  register uint64_t end_cycle __asm__("a6");

  start_cycle = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
  csr_write(LW_RISCV_CSR_SMPUVA, 0xDEADF00D);
  end_cycle = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
  uint64_t bench = end_cycle - start_cycle;

  csr_set(LW_RISCV_CSR_MCFG, DRF_DEF64(_RISCV_CSR, _MCFG, _MWFP_DIS, _TRUE) |
                             DRF_DEF64(_RISCV_CSR, _MCFG, _SWFP_DIS, _TRUE));
  MPRV_ENABLE(DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR));
  start_cycle = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
  csr_write(LW_RISCV_CSR_SMPUVA, 0xBADBEEF);
  end_cycle = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);
  MPRV_DISABLE();
  uint64_t result = end_cycle - start_cycle;
  if(result >= bench) {
    return MAKE_DEBUG_VALUE(TestErrorCode_WfpDis_PerformanceMismatch, 0, bench, result);
  }

  csr_clear(LW_RISCV_CSR_MCFG, DRF_DEF64(_RISCV_CSR, _MCFG, _MWFP_DIS, _TRUE) |
                               DRF_DEF64(_RISCV_CSR, _MCFG, _SWFP_DIS, _TRUE));

  return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
// Test that parallel MPU search works
uint64_t runTestParallel(void) {
  uint64_t addr = 0x1000000000ULL;// Theoretically a good unused address
  uint64_t va = addr | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  uint64_t pa = (uintptr_t)g_test_data;
  uint64_t attr = DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, 1);

  FILL_TEST_DATA();

  // fmodel doesn't implement timing very well
  if(!is_fmodel) {
    // Setup MPU for timing test
    for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) {
      csr_write(LW_RISCV_CSR_SMPUIDX, i);
      csr_write(LW_RISCV_CSR_SMPUATTR, attr);
      csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
      csr_write(LW_RISCV_CSR_SMPUPA, pa);
      csr_write(LW_RISCV_CSR_SMPUVA, va + (i * 0x400));
    }

    // Gather timings for hitting each of the MPU indices
    uint64_t timings[EXPECTED_MPU_IDX_COUNT];
    volatile uint8_t *ptr = (volatile uint8_t*)addr;
    for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) {
      register uint64_t start_cycle __asm__("a7");
      register uint64_t end_cycle __asm__("a6");

      MPRV_ENABLE(DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR));
      __asm__ volatile(
        /* Make sure the pointer is not in the TLB before we sample it */
        " sfence.vma\n"
        " csrr  %[start_cycle], cycle\n"
        " lbu   t0, 0(%[ptr])\n"
        " fence.i\n"
        " csrr  %[end_cycle], cycle\n"
        : [start_cycle]"+r"(start_cycle),
          [end_cycle]"+r"(end_cycle),
          [ptr]"+r"(ptr)
        :
        : "t0");
      MPRV_DISABLE();
      // remap the timings for easier comparison
      timings[(i / EXPECTED_MPU_PARALLELIZATION) +
              ((i % EXPECTED_MPU_PARALLELIZATION) * EXPECTED_MPU_PARALLEL_SIZE)] = end_cycle -
                                                                                   start_cycle;
      ptr += 0x400;
    }

    // perform analysis on the timings
    int idx = 0;
    for(int l = 0; l < EXPECTED_MPU_PARALLELIZATION; l++) {
      for(int i = 0; i < EXPECTED_MPU_PARALLEL_SIZE; i++, idx++) {
        // First of a group should be faster than the last of the previous group
        if(i == 0) {
          if(idx != 0 && timings[idx] >= timings[idx - 1]) {
            return MAKE_DEBUG_VALUE(TestErrorCode_Parallel_PerformanceMismatch, idx,
                                    timings[idx - 1], timings[idx]);
          }
        }else{
          if(timings[idx] <= timings[idx - 1]) {
            return MAKE_DEBUG_VALUE(TestErrorCode_Parallel_PerformanceMismatch, idx,
                                    timings[idx - 1], timings[idx]);
          }
        }
      }
    }
  }

  // Validate that within the parallel search, the lowest index always hits
  for(int i = 0; i < EXPECTED_MPU_IDX_COUNT; i++) {
    csr_write(LW_RISCV_CSR_SMPUIDX, i);
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
  }

  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa + 0x400);
  csr_write(LW_RISCV_CSR_SMPUVA, va + (0 * 0x400));

  csr_write(LW_RISCV_CSR_SMPUIDX, EXPECTED_MPU_PARALLEL_SIZE);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa);
  csr_write(LW_RISCV_CSR_SMPUVA, va + (0 * 0x400));

  csr_write(LW_RISCV_CSR_SMPUIDX, EXPECTED_MPU_PARALLEL_SIZE - 1);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa);
  csr_write(LW_RISCV_CSR_SMPUVA, va + (1 * 0x400));

  csr_write(LW_RISCV_CSR_SMPUIDX, EXPECTED_MPU_PARALLEL_SIZE + 1);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa + 0x400);
  csr_write(LW_RISCV_CSR_SMPUVA, va + (1 * 0x400));

  register uint8_t read_value __asm__("a5");
  volatile uint8_t *ptr = (volatile uint8_t*)addr;

  MPRV_ENABLE(DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR));
  __asm__ volatile("sfence.vma\n");
  read_value = ptr[0];
  MPRV_DISABLE();
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Parallel_UnexpectedException, 0,
                            trap_mcause, trap_mtval);
  }
  if(read_value != g_test_data[0x400]) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Parallel_DataMismatch, 0,
                            g_test_data[0x400], read_value);
  }

  MPRV_ENABLE(DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR));
  __asm__ volatile("sfence.vma\n");
  read_value = ptr[0x400];
  MPRV_DISABLE();
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Parallel_UnexpectedException, 1,
                            trap_mcause, trap_mtval);
  }
  if(read_value != g_test_data[0]) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Parallel_DataMismatch, 1,
                            g_test_data[0], read_value);
  }

  return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
// Test that valid/dirty/access bits and their dedicated CSRs work
#define VLDDTYACC_MASK (DRF_NUM64(_RISCV_CSR, _SMPUVA, _A, 1) | \
                        DRF_NUM64(_RISCV_CSR, _SMPUVA, _D, 1) | \
                        DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1))

uint64_t runTestVldDtyAcc(void) {
  uint64_t addr = 0x1000000000ULL;// Theoretically a good unused address
  uint64_t va = addr | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  uint64_t pa = (uintptr_t)g_test_data;
  uint64_t attr = DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, 1);

  FILL_TEST_DATA();
  volatile uint8_t *ptr = (volatile uint8_t*)addr;

  // Set up MPU entries to work with:
  // 0= ptr[0x000..0x3FF] = g_test_data[0x000..0x3FF]
  // 1= ptr[0x000..0x3FF] = g_test_data[0x400..0x7FF] (+acc)
  // 2= ptr[0x400..0x7FF] = g_test_data[0x000..0x3FF] (+dty)
  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa);
  csr_write(LW_RISCV_CSR_SMPUVA, va);

  csr_write(LW_RISCV_CSR_SMPUIDX, 1);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa + 0x400);
  csr_write(LW_RISCV_CSR_SMPUVA, va |
                                 DRF_NUM64(_RISCV_CSR, _SMPUVA, _A, 1));

  csr_write(LW_RISCV_CSR_SMPUIDX, 2);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa);
  csr_write(LW_RISCV_CSR_SMPUVA, (va + 0x400) |
                                 DRF_NUM64(_RISCV_CSR, _SMPUVA, _D, 1));

  // Clear remaining MPU
  for(int i = 3; i < EXPECTED_MPU_IDX_COUNT; i++) {
    csr_write(LW_RISCV_CSR_SMPUIDX, i);
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
  }

  // Check that bits didn't change, and that they can be manually changed, and that they match the regs
  csr_write(LW_RISCV_CSR_SMPUIDX2, 0);
  if(csr_read(LW_RISCV_CSR_SMPUVLD) != 7) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 0, 7, csr_read(LW_RISCV_CSR_SMPUVLD));
  }
  if(csr_read(LW_RISCV_CSR_SMPUACC) != 2) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 1, 2, csr_read(LW_RISCV_CSR_SMPUACC));
  }
  if(csr_read(LW_RISCV_CSR_SMPUDTY) != 4) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 2, 4, csr_read(LW_RISCV_CSR_SMPUDTY));
  }
  csr_write(LW_RISCV_CSR_SMPUACC, 4);
  csr_write(LW_RISCV_CSR_SMPUDTY, 2);

  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  uint64_t val = csr_read(LW_RISCV_CSR_SMPUVA) & VLDDTYACC_MASK;
  uint64_t expect = DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  if(val != expect) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 0x10, expect, val);
  }
  csr_set(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _A, 1));
  val = csr_read(LW_RISCV_CSR_SMPUVA) & VLDDTYACC_MASK;
  expect |= DRF_NUM64(_RISCV_CSR, _SMPUVA, _A, 1);
  if(val != expect) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 0x11, expect, val);
  }
  csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _A, 1));

  csr_write(LW_RISCV_CSR_SMPUIDX, 1);
  val = csr_read(LW_RISCV_CSR_SMPUVA) & VLDDTYACC_MASK;
  expect = DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1) |
           DRF_NUM64(_RISCV_CSR, _SMPUVA, _D, 1);
  if(val != expect) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 0x12, expect, val);
  }
  csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _D, 1));
  val = csr_read(LW_RISCV_CSR_SMPUVA) & VLDDTYACC_MASK;
  expect = DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  if(val != expect) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 0x13, expect, val);
  }

  csr_write(LW_RISCV_CSR_SMPUIDX, 2);
  val = csr_read(LW_RISCV_CSR_SMPUVA) & VLDDTYACC_MASK;
  expect = DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1) |
           DRF_NUM64(_RISCV_CSR, _SMPUVA, _A, 1);
  if(val != expect) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 0x14, expect, val);
  }
  csr_clear(LW_RISCV_CSR_SMPUVA, DRF_NUM64(_RISCV_CSR, _SMPUVA, _A, 1));
  val = csr_read(LW_RISCV_CSR_SMPUVA) & VLDDTYACC_MASK;
  expect = DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  if(val != expect) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 0x15, expect, val);
  }

  if(csr_read(LW_RISCV_CSR_SMPUVLD) != 7) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 3, 7, csr_read(LW_RISCV_CSR_SMPUVLD));
  }
  if(csr_read(LW_RISCV_CSR_SMPUACC) != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 4, 0, csr_read(LW_RISCV_CSR_SMPUACC));
  }
  if(csr_read(LW_RISCV_CSR_SMPUDTY) != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DataMismatch, 5, 0, csr_read(LW_RISCV_CSR_SMPUDTY));
  }

  // Test access bits
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  csr_set(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR) |
                                DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE));
  __asm__ volatile("fence.i");
  (void)ptr[0];
  (void)ptr[0x400];
  __asm__ volatile("fence");
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  __asm__ volatile("fence.i");
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_UnexpectedException, 0,
                            trap_mcause, trap_mtval);
  }
  if(csr_read(LW_RISCV_CSR_SMPUACC) != 5) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_AccessMismatch, 0, 5, csr_read(LW_RISCV_CSR_SMPUACC));
  }
  if(csr_read(LW_RISCV_CSR_SMPUDTY) != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DirtyMismatch, 0, 0, csr_read(LW_RISCV_CSR_SMPUDTY));
  }
  csr_write(LW_RISCV_CSR_SMPUACC, 0);

  // Test dirty bits
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  csr_set(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR) |
                                DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE));
  __asm__ volatile("fence.i");
  ptr[0] = 9;
  (void)ptr[0x400];
  __asm__ volatile("fence");
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  __asm__ volatile("fence.i");
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_UnexpectedException, 1,
                            trap_mcause, trap_mtval);
  }
  if(csr_read(LW_RISCV_CSR_SMPUACC) != 5) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_AccessMismatch, 1, 5, csr_read(LW_RISCV_CSR_SMPUACC));
  }
  if(csr_read(LW_RISCV_CSR_SMPUDTY) != 1) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DirtyMismatch, 1, 1, csr_read(LW_RISCV_CSR_SMPUDTY));
  }
  csr_write(LW_RISCV_CSR_SMPUACC, 0);
  csr_write(LW_RISCV_CSR_SMPUDTY, 0);

  // Test both bits
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  csr_set(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR) |
                                DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE));
  __asm__ volatile("fence.i");
  ptr[0] = 9;
  (void)ptr[0];
  __asm__ volatile("fence");
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  __asm__ volatile("fence.i");
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_UnexpectedException, 1,
                            trap_mcause, trap_mtval);
  }
  if(csr_read(LW_RISCV_CSR_SMPUACC) != 1) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_AccessMismatch, 2, 1, csr_read(LW_RISCV_CSR_SMPUACC));
  }
  if(csr_read(LW_RISCV_CSR_SMPUDTY) != 1) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DirtyMismatch, 2, 1, csr_read(LW_RISCV_CSR_SMPUDTY));
  }
  csr_write(LW_RISCV_CSR_SMPUACC, 0);
  csr_write(LW_RISCV_CSR_SMPUDTY, 0);

  // Test both bits (opposite order)
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  csr_set(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR) |
                                DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE));
  __asm__ volatile("fence.i");
  (void)ptr[0];
  ptr[0] = 9;
  __asm__ volatile("fence");
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  __asm__ volatile("fence.i");
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_UnexpectedException, 1,
                            trap_mcause, trap_mtval);
  }
  if(csr_read(LW_RISCV_CSR_SMPUACC) != 1) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_AccessMismatch, 3, 1, csr_read(LW_RISCV_CSR_SMPUACC));
  }
  if(csr_read(LW_RISCV_CSR_SMPUDTY) != 1) {
    return MAKE_DEBUG_VALUE(TestErrorCode_VldDtyAcc_DirtyMismatch, 3, 1, csr_read(LW_RISCV_CSR_SMPUDTY));
  }
  csr_write(LW_RISCV_CSR_SMPUACC, 0);
  csr_write(LW_RISCV_CSR_SMPUDTY, 0);

  return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
// Test that virtualized MPU search window works
// NOTE: Both FMODEL and EMU have the same issues with this test.
//       They do not throw an exception on invalid mmpuctl programming.
//       They do not adjust the base of SMPUIDX or SMPUIDX2 for the virtualized range.
// Updated for virtualized index for t23x and gh100+
uint64_t runTestVirtualized(void) {
  uint64_t addr = 0x1000000000ULL;// Theoretically a good unused address
  uint64_t va = addr | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1);
  uint64_t pa = (uintptr_t)g_test_data;
  uint64_t attr = DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, 1) |
                  DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, 1);

  FILL_TEST_DATA();
  volatile uint8_t *ptr = (volatile uint8_t*)addr;

  // Verify bad programming causes illegal instruction
  if(0) {
    csr_write(LW_RISCV_CSR_MMPUCTL, DRF_SHIFTMASK(LW_RISCV_CSR_MMPUCTL_START_INDEX) |
                                    DRF_SHIFTMASK(LW_RISCV_CSR_MMPUCTL_ENTRY_COUNT));
    if(trap_mepc == 0) {
      return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_MissedException, 0, 2, 0xFF);
    }else if(trap_mcause != 2) {
      return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_UnexpectedException, 0, 2, trap_mcause);
    }
    TRAP_CLEAR();
  }

  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  csr_write(LW_RISCV_CSR_SMPUATTR, 0x3);
  csr_write(LW_RISCV_CSR_SMPUIDX, 2);
  csr_write(LW_RISCV_CSR_SMPUATTR, 0xF);

  // Check that SMPUIDX virtualization works
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 2) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, 3));
  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  csr_write(LW_RISCV_CSR_SMPUATTR, 7);
  csr_write(LW_RISCV_CSR_SMPUIDX, 2);
  csr_write(LW_RISCV_CSR_SMPUATTR, 7);

  // Reset mmpuctl now
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 0) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, EXPECTED_MPU_IDX_COUNT));
  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  if(csr_read(LW_RISCV_CSR_SMPUATTR) != 3) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 0, 3, csr_read(LW_RISCV_CSR_SMPUATTR));
  }
  csr_write(LW_RISCV_CSR_SMPUIDX, 2);
  if(csr_read(LW_RISCV_CSR_SMPUATTR) != 0x7) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 1, 0x7, csr_read(LW_RISCV_CSR_SMPUATTR));
  }
  csr_write(LW_RISCV_CSR_SMPUIDX, 4);
  if(csr_read(LW_RISCV_CSR_SMPUATTR) != 0x7) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 1, 0xF, csr_read(LW_RISCV_CSR_SMPUATTR));
  }

  // Set up MPU entries to work with:
  // 0= ptr[0x000..0x3FF] = g_test_data[0x000..0x3FF]
  // 1= ptr[0x000..0x3FF] = g_test_data[0x400..0x7FF]
  // 2= ptr[0x400..0x7FF] = g_test_data[0x000..0x3FF]
  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa);
  csr_write(LW_RISCV_CSR_SMPUVA, va);

  csr_write(LW_RISCV_CSR_SMPUIDX, 1);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa + 0x400);
  csr_write(LW_RISCV_CSR_SMPUVA, va);

  csr_write(LW_RISCV_CSR_SMPUIDX, 2);
  csr_write(LW_RISCV_CSR_SMPUATTR, attr);
  csr_write(LW_RISCV_CSR_SMPURNG, 0x400);
  csr_write(LW_RISCV_CSR_SMPUPA, pa);
  csr_write(LW_RISCV_CSR_SMPUVA, va + 0x400);

  // Clear remaining MPU
  for(int i = 3; i < EXPECTED_MPU_IDX_COUNT; i++) {
    csr_write(LW_RISCV_CSR_SMPUIDX, i);
    csr_write(LW_RISCV_CSR_SMPUVA, 0);
  }

  // Verify writing extra entries are ignored
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 0) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, 3));
  csr_write(LW_RISCV_CSR_SMPUIDX, 3);
  csr_write(LW_RISCV_CSR_SMPUVA, 0xDEAD);
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 0) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, EXPECTED_MPU_IDX_COUNT));
  csr_write(LW_RISCV_CSR_SMPUIDX, 3);
  if(csr_read(LW_RISCV_CSR_SMPUVA) != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 2, 0, csr_read(LW_RISCV_CSR_SMPUVA));
  }

  // Check restricted set
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 0) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, 3));

  // Check entry 0&2
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  csr_set(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR) |
                                DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE));
  __asm__ volatile("fence.i");
  register uint64_t read_value __asm__("a7") = ptr[0];
  register uint64_t read_value2 __asm__("a6") = ptr[0x400];
  __asm__ volatile("fence");
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  __asm__ volatile("fence.i");
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_UnexpectedException, 1,
                            trap_mcause, trap_mtval);
  }
  if(read_value != g_test_data[0]) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 3,
                            g_test_data[0], read_value);
  }
  if(read_value2 != g_test_data[0]) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 4,
                            g_test_data[0], read_value2);
  }

  // Check entry 1 became enabled
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 1) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, 2));
  csr_set(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR) |
                                DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE));
  __asm__ volatile("fence.i");
  read_value = ptr[0];
  __asm__ volatile("fence");
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  __asm__ volatile("fence.i");
  if(trap_mepc != 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_UnexpectedException, 2,
                            trap_mcause, trap_mtval);
  }
  if(read_value != g_test_data[0x400]) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 5,
                            g_test_data[0x400], read_value);
  }

  // Check entry 0&1 are disabled
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 2) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, 1));
  csr_set(LW_RISCV_CSR_MSTATUS, DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPP, _SUPERVISOR) |
                                DRF_DEF64(_RISCV_CSR, _MSTATUS, _MPRV, _ENABLE));
  __asm__ volatile("fence.i");
  read_value = ptr[0];
  __asm__ volatile("fence");
  csr_clear(LW_RISCV_CSR_MSTATUS, DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPP) |
                                  DRF_SHIFTMASK64(LW_RISCV_CSR_MSTATUS_MPRV));
  __asm__ volatile("fence.i");
  if(trap_mepc == 0) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_MissedException, 1, 0, 0);
  }

  // Check that smpuidx2 works
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 2) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, 2));
  csr_write(LW_RISCV_CSR_SMPUIDX2, 0);
  if(csr_read(LW_RISCV_CSR_SMPUVLD) != 1) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 6,
                            1, csr_read(LW_RISCV_CSR_SMPUVLD)); // 4->1
  }
  // Check that writes to SMPUVLD works
  csr_write(LW_RISCV_CSR_SMPUVLD, 2); // 8->2
  // Reset mmpuctl now
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 0) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, EXPECTED_MPU_IDX_COUNT));
  csr_write(LW_RISCV_CSR_SMPUIDX, 0);
  if(!FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1, csr_read(LW_RISCV_CSR_SMPUVA))) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 7,
                            1, csr_read(LW_RISCV_CSR_SMPUVA) & 1);
  }
  csr_write(LW_RISCV_CSR_SMPUIDX, 1);
  if(!FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1, csr_read(LW_RISCV_CSR_SMPUVA))) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 8,
                            1, csr_read(LW_RISCV_CSR_SMPUVA) & 1);
  }
  csr_write(LW_RISCV_CSR_SMPUIDX, 2);
  if(!FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 0, csr_read(LW_RISCV_CSR_SMPUVA))) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 9,
                            0, csr_read(LW_RISCV_CSR_SMPUVA) & 1);
  }
  csr_write(LW_RISCV_CSR_SMPUIDX, 3);
  if(!FLD_TEST_DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1, csr_read(LW_RISCV_CSR_SMPUVA))) {
    return MAKE_DEBUG_VALUE(TestErrorCode_Virtualized_DataMismatch, 10,
                            1, csr_read(LW_RISCV_CSR_SMPUVA) & 1);
  }

  return PASSING_DEBUG_VALUE;
}

//-------------------------------------------------------------------------------------------------
typedef uint64_t (*TestCallback)(void);
static const TestCallback c_test_callbacks[MpuTestIdCount] = {
  [MpuTestId_EntryCount]    = runTestEntryCount,
  [MpuTestId_Permission]    = runTestPermission,
  [MpuTestId_TlbIlwalidate] = runTestTlbIlwalidate,
  [MpuTestId_WfpDis]        = runTestWfpDis,
  [MpuTestId_Parallel]      = runTestParallel,
  [MpuTestId_VldDtyAcc]     = runTestVldDtyAcc,
  [MpuTestId_Virtualized]   = runTestVirtualized,
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

  // Turn on MPU for non-M mode
  csr_set(LW_RISCV_CSR_SATP, DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _LWMPU));

  // Clear MPU control
  csr_write(LW_RISCV_CSR_MMPUCTL, DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX, 0) |
                                  DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT, EXPECTED_MPU_IDX_COUNT));

  is_fmodel = true; // FPGA doesn't have this register in Orin

  // Clear trap recording values
  TRAP_CLEAR();
#ifdef RUN_ONE_TEST
  uint64_t retval = c_test_callbacks[RUN_ONE_TEST]();
  csr_write(LW_RISCV_CSR_MSCRATCH, retval);
#else
  bool pass = true;
  for(int i = 0; i < MpuTestIdCount; i++) {
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
#endif
  localWrite(LW_PRGNLCL_RISCV_ICD_CMD, LW_PRGNLCL_RISCV_ICD_CMD_OPC_STOP);
    __asm__ volatile("fence");
  return 0;
}
