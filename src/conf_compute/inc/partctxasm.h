/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CC_PARTCTXASM_H
#define CC_PARTCTXASM_H

#include "partasmmacros.h"
// Context switch helpers to save/restore partition state
// This file should be included in partition's entry.S with pre-configured defines
// (on what and where to save, partition name etc)
// MK TODO: this is ugly, but probably most reliable, alternative would be "build it yourself"

// You need:
// PARTITION_NAME       - partition "name"
// PARTCTX_PRESERVE_GPR - save/restore GPR
// PARTCTX_PRESERVE_FPU - save/restore FPU
// PARTCTX_PRESERVE_CSR - save/restore CSR (general)
// PARTCTX_PRESERVE_MPU - save/restore MPU (WARNING : won't work without identity mapping)
// PARTCTX_REENABLE_MPU - just turn MPU on (only if partition has carveout)

// Partition switch return code
FUNC PARTITION_SWITCH_RETURN_NAME

#if PARTCTX_PRESERVE_MPU
  // Restore MPU context
  li t0, 0

  // Limit restore to entries we have access to
  csrr t1, LW_RISCV_CSR_SMPUCTL
  srli t1, t1, 16   // bits 16:23 is entry counts
  andi t1, t1, 0xFF // max 256 entries
  la t2, _mpu_context
1:
  ld s0, 0(t2) // va
  ld s1, 8(t2) // pa
  ld s2, 16(t2) // range
  ld s3, 24(t2) //attr
  csrw LW_RISCV_CSR_SMPUIDX, t0
  csrw LW_RISCV_CSR_SMPUVA, s0
  csrw LW_RISCV_CSR_SMPUPA, s1
  csrw LW_RISCV_CSR_SMPURNG, s2
  csrw LW_RISCV_CSR_SMPUATTR, s3

  addi t2, t2, 8 * 4
  addi t0, t0, 1
  addi t1, t1, -1
  bnez t1, 1b
#endif

#if PARTCTX_PRESERVE_CSR
  la t0, _misc_csr
  // restore CSR's
  RCSR LW_RISCV_CSR_SSTATUS      CSR_INDEX_SSTATUS
  RCSR LW_RISCV_CSR_STVEC        CSR_INDEX_STVEC
  RCSR LW_RISCV_CSR_SIE          CSR_INDEX_SIE
  RCSR LW_RISCV_CSR_SCOUNTEREN   CSR_INDEX_SCOUNTEREN
  RCSR LW_RISCV_CSR_SSPM         CSR_INDEX_SSPM
  RCSR LW_RISCV_CSR_SRSP         CSR_INDEX_SRSP
  RCSR LW_RISCV_CSR_SSCRATCH     CSR_INDEX_SSCRATCH
  RCSR LW_RISCV_CSR_SSCRATCH2    CSR_INDEX_SSCRATCH2
  // MK TODO: what about SCFG? Also should we use it to optimize CSR restore?
#endif

#if PARTCTX_PRESERVE_MPU || PARTCTX_REENABLE_MPU
  // Restore MPU dirty/clean bits, do it naively assuming 128 entries
  addi t1, zero, 1
  csrw LW_RISCV_CSR_SMPUIDX2, t1
  RCSR LW_RISCV_CSR_SMPUVLD   CSR_INDEX_SMPUVLD
  RCSR LW_RISCV_CSR_SMPUDTY   CSR_INDEX_SMPUDTY
  RCSR LW_RISCV_CSR_SMPUACC   CSR_INDEX_SMPUACC
  addi t1, zero, 0
  csrw LW_RISCV_CSR_SMPUIDX2, t1
  RCSR LW_RISCV_CSR_SMPUVLD   CSR_INDEX_SMPUVLD2
  RCSR LW_RISCV_CSR_SMPUDTY   CSR_INDEX_SMPUDTY2
  RCSR LW_RISCV_CSR_SMPUACC   CSR_INDEX_SMPUACC2

  // Possibly turn MPU back on
  RCSR LW_RISCV_CSR_SATP         CSR_INDEX_SATP
  // Instruction fence
  fence.i
#endif

  // Restore FPU context (if FPU is enabled)
#if PARTCTX_PRESERVE_FPU
  csrr   s0, xstatus
// Skip if FPU was not enabled (we can't restore it in that case)
// MK TODO: maybe restore anyway even if disabled on switch?
  bge    s0, zero, 1f

  la s1, _fpu_csr
  ld s0, 0(s1)
  fscsr  s0

  la s1, _fpu_context
  fpContextRestoreCaller
  fpContextRestoreCallee
1:
#endif

  // now restore saved GPR
  la t0, _gpr_context
  LREG    x1,   1 * REGBYTES(t0) // ra
  LREG    x2,   2 * REGBYTES(t0) // sp
  LREG    x3,   3 * REGBYTES(t0) // gp
  LREG    x4,   4 * REGBYTES(t0) // tp
  // missing t0, we'll restore it later
  LREG    x6,   6 * REGBYTES(t0) // t1
  LREG    x7,   7 * REGBYTES(t0) // t2
  LREG    x8,   8 * REGBYTES(t0) // s0/fp
  LREG    x9,   9 * REGBYTES(t0) // s1
  LREG    x18, 18 * REGBYTES(t0) // s2
  LREG    x19, 19 * REGBYTES(t0) // s3
  LREG    x20, 20 * REGBYTES(t0) // s4
  LREG    x21, 21 * REGBYTES(t0) // s5
  LREG    x22, 22 * REGBYTES(t0) // s6
  LREG    x23, 23 * REGBYTES(t0) // s7
  LREG    x24, 24 * REGBYTES(t0) // s8
  LREG    x25, 25 * REGBYTES(t0) // s9
  LREG    x26, 26 * REGBYTES(t0) // s10
  LREG    x27, 27 * REGBYTES(t0) // s11
  LREG    x28, 28 * REGBYTES(t0) // t3
  LREG    x29, 29 * REGBYTES(t0) // t4
  LREG    x30, 30 * REGBYTES(t0) // t5
  LREG    x31, 31 * REGBYTES(t0) // t6

  // finally t0
  LREG    x5,   5 * REGBYTES(t0) // t0
  // now return from function call
  ret
EFUNC PARTITION_SWITCH_RETURN_NAME

// That's function calling partition switch SBI
// Function signature (must be unique per partition):
//LwU64 partition<x>SwitchInt(LwU64 partition, LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5);

FUNC PARTITION_SWITCH_NAME
  sd t0, -8(sp)
  la t0, _gpr_context
  SREG    x1,   1 * REGBYTES(t0) // ra
  SREG    x2,   2 * REGBYTES(t0) // sp
  SREG    x3,   3 * REGBYTES(t0) // gp
  SREG    x4,   4 * REGBYTES(t0) // tp
  // missing t0
  SREG    x6,   6 * REGBYTES(t0) // t1
  SREG    x7,   7 * REGBYTES(t0) // t2
  SREG    x8,   8 * REGBYTES(t0) // s0/fp
  SREG    x9,   9 * REGBYTES(t0) // s1
  SREG    x18, 18 * REGBYTES(t0) // s2
  SREG    x19, 19 * REGBYTES(t0) // s3
  SREG    x20, 20 * REGBYTES(t0) // s4
  SREG    x21, 21 * REGBYTES(t0) // s5
  SREG    x22, 22 * REGBYTES(t0) // s6
  SREG    x23, 23 * REGBYTES(t0) // s7
  SREG    x24, 24 * REGBYTES(t0) // s8
  SREG    x25, 25 * REGBYTES(t0) // s9
  SREG    x26, 26 * REGBYTES(t0) // s10
  SREG    x27, 27 * REGBYTES(t0) // s11
  SREG    x28, 28 * REGBYTES(t0) // t3
  SREG    x29, 29 * REGBYTES(t0) // t4
  SREG    x30, 30 * REGBYTES(t0) // t5
  SREG    x31, 31 * REGBYTES(t0) // t6

  add t1, t0, zero
  ld t0, -8(sp)

  // finally t0
  SREG    x5,   5 * REGBYTES(t1) // t0

// Save FPU context if enabled
#if PARTCTX_PRESERVE_FPU
  csrr   s0, xstatus
  // Skip if FPU is not on
  // MK TODO: we may have to save it anyway if RTOS will optimize FPU context switches
  bge    s0, zero, 1f

  la s1, _fpu_csr
  frcsr  s0
  sd s0, 0(s1)

  la s1, _fpu_context
  fpContextSaveCaller
  fpContextSaveCallee
1:
#endif

#if PARTCTX_PRESERVE_CSR
  la t0, _misc_csr
  SCSR LW_RISCV_CSR_SSTATUS      CSR_INDEX_SSTATUS
  SCSR LW_RISCV_CSR_STVEC        CSR_INDEX_STVEC
  SCSR LW_RISCV_CSR_SIE          CSR_INDEX_SIE
  SCSR LW_RISCV_CSR_SCOUNTEREN   CSR_INDEX_SCOUNTEREN
  SCSR LW_RISCV_CSR_SSPM         CSR_INDEX_SSPM
  SCSR LW_RISCV_CSR_SRSP         CSR_INDEX_SRSP
  SCSR LW_RISCV_CSR_SSCRATCH     CSR_INDEX_SSCRATCH
  SCSR LW_RISCV_CSR_SSCRATCH2    CSR_INDEX_SSCRATCH2
#endif
#if PARTCTX_PRESERVE_MPU || PARTCTX_REENABLE_MPU
  SCSR LW_RISCV_CSR_SATP         CSR_INDEX_SATP
  // Save MPU dirty/clean bits, do it naively assuming 128 entries
  addi t1, zero, 1
  csrw LW_RISCV_CSR_SMPUIDX2, t1
  SCSR LW_RISCV_CSR_SMPUVLD   CSR_INDEX_SMPUVLD
  SCSR LW_RISCV_CSR_SMPUDTY   CSR_INDEX_SMPUDTY
  SCSR LW_RISCV_CSR_SMPUACC   CSR_INDEX_SMPUACC
  addi t1, zero, 0
  csrw LW_RISCV_CSR_SMPUIDX2, t1
  SCSR LW_RISCV_CSR_SMPUVLD   CSR_INDEX_SMPUVLD2
  SCSR LW_RISCV_CSR_SMPUDTY   CSR_INDEX_SMPUDTY2
  SCSR LW_RISCV_CSR_SMPUACC   CSR_INDEX_SMPUACC2
#endif

#if PARTCTX_PRESERVE_MPU
  // Save MPU state
  li t0, 0
  csrr t1, LW_RISCV_CSR_SMPUCTL
  srli t1, t1, 16   // bits 16:23 is entry counts
  andi t1, t1, 0xFF // max 256 entries
  la t2, _mpu_context
1:
  csrw LW_RISCV_CSR_SMPUIDX, t0
  csrr s0, LW_RISCV_CSR_SMPUVA
  csrr s1, LW_RISCV_CSR_SMPUPA
  csrr s2, LW_RISCV_CSR_SMPURNG
  csrr s3, LW_RISCV_CSR_SMPUATTR
  sd s0, 0(t2) // va
  sd s1, 8(t2) // pa
  sd s2, 16(t2) // range
  sd s3, 24(t2) //attr

  addi t2, t2, 8 * 4
  addi t0, t0, 1
  addi t1, t1, -1
  bnez t1, 1b
#endif

  // Setup SBI function id / number
  li a7, SBI_EXTENSION_LWIDIA // Extension
  li a6, SBI_LWFUNC_FIRST     // Function

  partitionMarkReturn
  // now do partition switch
  ecall
    // does not return, and when it does it goes to partitionSwitchReturn
1:
  j 1b
EFUNC PARTITION_SWITCH_NAME

// Saved context
.section .partition_context
.balign 8
_bwaiting_for_return: // Do we wait for return from context switch?
.fill 1, 8, 0
_gpr_context: // GPR's, 32 for easier save/restore
.fill 32, 8, 0
#if PARTCTX_PRESERVE_MPU
_mpu_context: // MPU entries, 4 register for each entry
  .fill 128 * 4, 8, 0
#endif
#if PARTCTX_PRESERVE_CSR || PARTCTX_REENABLE_MPU
_misc_csr: // CSRs
  .fill CSR_INDEX_SIZE, 8, 0
#endif
#if PARTCTX_PRESERVE_FPU
_fpu_csr: // FPU status
  .fill 1, 8, 0
_fpu_context: // FPU registers
  .fill 32, FPREGBYTES, 0
#endif

#endif // PARTCTXASM_H
