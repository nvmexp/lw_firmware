/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * This file implements code to save/restore starte of Peregrine on
 * partition switch.
 * On partition return, SK jumps to start of partition (that should ber always
 * partitionSwitchReturn function) and core state is wiped (all GPR, most CSR).
 *
 * This code exelwtes with MPU *off*, so every code and data
 * in this file must be resident ant in indentity mapping (so we can toggle MPU
 * back on).
 */

#define SAFE_RTOS_BUILD
#include <SafeRTOSConfig.h>
#include <portasmmacro.h>
#include <riscv_csr.h>

// Because sbi.h is assembler-unfriendly
#define SBI_EXTENSION_LWIDIA 0x090001EB
#define SBI_LWFUNC_FIRST     0

// Indexes where we save CSR's context
#define CSR_INDEX_SSTATUS      0x0
#define CSR_INDEX_STVEC        0x1
#define CSR_INDEX_SIE          0x2
#define CSR_INDEX_SATP         0x3
#define CSR_INDEX_SCOUNTEREN   0x4
#define CSR_INDEX_SSPM         0x5
#define CSR_INDEX_SRSP         0x6
#define CSR_INDEX_SSCRATCH     0x7
#define CSR_INDEX_SSCRATCH2    0x8
#define CSR_INDEX_SMPUVLD      0x9
#define CSR_INDEX_SMPUDTY      0xa
#define CSR_INDEX_SMPUACC      0xb
#define CSR_INDEX_SMPUVLD2     0xc
#define CSR_INDEX_SMPUDTY2     0xd
#define CSR_INDEX_SMPUACC2     0xe
#define CSR_INDEX_SIZE         0xf

// Restore CSR macro
.macro  RCSR name, offs
  ld s0, \offs * 8(t0)
  csrw \name, s0
.endm

// Save CSR macro
.macro  SCSR name, offs
csrr s0, \name
sd s0, \offs * 8(t0)
.endm

// Entry from partition switch, has to be at start of imem identity section
.section  .imem_identity, "x", @progbits
// Must be first, that's where SK jumps to
FUNC partitionSwitchReturn
  // Restore MPU context
  li t0, 0

  // Limit restore to entries we have access to
  // MK TODO: we may need to diffrentiate between GA10X because of virtualization bug
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

  // Restore FPU context (if FPU is enabled)
#if ( configLW_FPU_SUPPORTED == 1U )
  csrr   s0, sstatus
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
EFUNC partitionSwitchReturn

// That's function calling partition switch SBI
// Function signature:
//LwU64 partitionSwitchKernel(LwU64 arg1, LwU64 arg2, LwU64 arg3, LwU64 arg4, LwU64 arg5, LwU64 partition);

FUNC partitionSwitchKernel
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
#if ( configLW_FPU_SUPPORTED == 1U )
  csrr   s0, sstatus
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

  la t0, _misc_csr
  SCSR LW_RISCV_CSR_SSTATUS      CSR_INDEX_SSTATUS
  SCSR LW_RISCV_CSR_STVEC        CSR_INDEX_STVEC
  SCSR LW_RISCV_CSR_SIE          CSR_INDEX_SIE
  SCSR LW_RISCV_CSR_SATP         CSR_INDEX_SATP
  SCSR LW_RISCV_CSR_SCOUNTEREN   CSR_INDEX_SCOUNTEREN
  SCSR LW_RISCV_CSR_SSPM         CSR_INDEX_SSPM
  SCSR LW_RISCV_CSR_SRSP         CSR_INDEX_SRSP
  SCSR LW_RISCV_CSR_SSCRATCH     CSR_INDEX_SSCRATCH
  SCSR LW_RISCV_CSR_SSCRATCH2    CSR_INDEX_SSCRATCH2

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

  // Save MPU state
  li t0, 0
  // read number of entries, assume they start at 0 (can't do otherwise on ga10x)
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

  // Setup SBI function id / number
  li a7, SBI_EXTENSION_LWIDIA // Extension
  li a6, SBI_LWFUNC_FIRST     // Function
  // now do partition switch
  ecall
    // does not return, and when it does it goes to partitionSwitchReturn
1:
  j 1b
EFUNC partitionSwitchKernel

// Context saved on partition switch.
// Rather bulky because we have to save whole MPU state
.section .dmem_identity
_mpu_context: // MPU entries, 4 register for each entry
  .fill 128 * 4, 8, 0
_misc_csr: // CSRs
  .fill CSR_INDEX_SIZE, 8, 0
_gpr_context: // GPR's, 32 for easier save/restore
  .fill 32, 8, 0
_fpu_csr: // FPU status
  .fill 1, 8, 0
_fpu_context: // FPU registers
  .fill 32, FPREGBYTES, 0

  // MK TODO
