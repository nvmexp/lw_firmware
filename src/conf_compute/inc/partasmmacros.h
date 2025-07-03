/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CC_PARTASMMACROS_H
#define CC_PARTASMMACROS_H

#include "partitions.h"

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

#define PARTITION_SWITCH_TOK(X,Y) PARTITION_SWITCH_TOK2(X, Y)
#define PARTITION_SWITCH_TOK2(X,Y) partition##X##Y

#define PARTITION_SWITCH_RETURN_NAME PARTITION_SWITCH_TOK(PARTITION_NAME, SwitchReturn)
#define PARTITION_SWITCH_NAME PARTITION_SWITCH_TOK(PARTITION_NAME, SwitchInt)

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

#define FPREGBYTES      4

.macro fpContextRestoreCaller
    flw     f0,   FPREGBYTES * 0(s1)
    flw     f1,   FPREGBYTES * 1(s1)
    flw     f2,   FPREGBYTES * 2(s1)
    flw     f3,   FPREGBYTES * 3(s1)
    flw     f4,   FPREGBYTES * 4(s1)
    flw     f5,   FPREGBYTES * 5(s1)
    flw     f6,   FPREGBYTES * 6(s1)
    flw     f7,   FPREGBYTES * 7(s1)
    flw     f10,  FPREGBYTES * 10(s1)
    flw     f11,  FPREGBYTES * 11(s1)
    flw     f12,  FPREGBYTES * 12(s1)
    flw     f13,  FPREGBYTES * 13(s1)
    flw     f14,  FPREGBYTES * 14(s1)
    flw     f15,  FPREGBYTES * 15(s1)
    flw     f16,  FPREGBYTES * 16(s1)
    flw     f17,  FPREGBYTES * 17(s1)
    flw     f28,  FPREGBYTES * 28(s1)
    flw     f29,  FPREGBYTES * 29(s1)
    flw     f30,  FPREGBYTES * 30(s1)
    flw     f31,  FPREGBYTES * 31(s1)
.endm

.macro fpContextSaveCaller
    fsw     f0,   FPREGBYTES * 0(s1)
    fsw     f1,   FPREGBYTES * 1(s1)
    fsw     f2,   FPREGBYTES * 2(s1)
    fsw     f3,   FPREGBYTES * 3(s1)
    fsw     f4,   FPREGBYTES * 4(s1)
    fsw     f5,   FPREGBYTES * 5(s1)
    fsw     f6,   FPREGBYTES * 6(s1)
    fsw     f7,   FPREGBYTES * 7(s1)
    fsw     f10,  FPREGBYTES * 10(s1)
    fsw     f11,  FPREGBYTES * 11(s1)
    fsw     f12,  FPREGBYTES * 12(s1)
    fsw     f13,  FPREGBYTES * 13(s1)
    fsw     f14,  FPREGBYTES * 14(s1)
    fsw     f15,  FPREGBYTES * 15(s1)
    fsw     f16,  FPREGBYTES * 16(s1)
    fsw     f17,  FPREGBYTES * 17(s1)
    fsw     f28,  FPREGBYTES * 28(s1)
    fsw     f29,  FPREGBYTES * 29(s1)
    fsw     f30,  FPREGBYTES * 30(s1)
    fsw     f31,  FPREGBYTES * 31(s1)
.endm

.macro fpContextRestoreCallee
    flw     f8,    FPREGBYTES * 8(s1)
    flw     f9,    FPREGBYTES * 9(s1)
    flw     f18,   FPREGBYTES * 18(s1)
    flw     f19,   FPREGBYTES * 19(s1)
    flw     f20,   FPREGBYTES * 20(s1)
    flw     f21,   FPREGBYTES * 21(s1)
    flw     f22,   FPREGBYTES * 22(s1)
    flw     f23,   FPREGBYTES * 23(s1)
    flw     f24,   FPREGBYTES * 24(s1)
    flw     f25,   FPREGBYTES * 25(s1)
    flw     f26,   FPREGBYTES * 26(s1)
    flw     f27,   FPREGBYTES * 27(s1)
.endm

.macro fpContextSaveCallee
    fsw     f8,    FPREGBYTES * 8(s1)
    fsw     f9,    FPREGBYTES * 9(s1)
    fsw     f18,   FPREGBYTES * 18(s1)
    fsw     f19,   FPREGBYTES * 19(s1)
    fsw     f20,   FPREGBYTES * 20(s1)
    fsw     f21,   FPREGBYTES * 21(s1)
    fsw     f22,   FPREGBYTES * 22(s1)
    fsw     f23,   FPREGBYTES * 23(s1)
    fsw     f24,   FPREGBYTES * 24(s1)
    fsw     f25,   FPREGBYTES * 25(s1)
    fsw     f26,   FPREGBYTES * 26(s1)
    fsw     f27,   FPREGBYTES * 27(s1)
.endm


// Does partition return if needed (if not pass through)
// Cleans t0/t1 but it's not a problem as its supposed to be called at entry point
.macro partitionReturnIfNeeded
  la t0, _bwaiting_for_return
  ld t1, 0(t0)
  beqz t1, 1f
  j PARTITION_SWITCH_RETURN_NAME
1:
.endm

// mark that we wait for partition return
.macro partitionMarkReturn
  la t0, _bwaiting_for_return
  addi t1, zero, 1
  sd t1, 0(t0)
.endm

// Initialize and setup mapping for stack (on first run) or just turn on mpu (on successive runs)
// stack_start is beginning of stack buffer (stack_start > stack_end).
// Must be exelwted just after partition entry (with mpu disabled)
.macro partitionInitSpAndMpu stack_start, stack_end
  // This is VA base address of stack, picked to be also SATP_LWMPU value
  li s0, PARTITION_STACK_VA_START

  // On exit from SK, assuming MPU is partitioned (which it should be),
  // our 0-th entry VA should be non-zero as that's stack mapping
  csrwi LW_RISCV_CSR_SMPUIDX, 0
  csrr t0, LW_RISCV_CSR_SMPUVA

  // If it's negative (== top bit set) it means we have non-zero entry,
  // mpu was configured and preserved so we can bypass configuration
  blt t0, zero, mpu_already_configured

  // otherwise - configure MPU
  // Stack mapping - entry 0
  csrwi LW_RISCV_CSR_SMPUIDX, 0
  ori t0, s0, 1
  csrw LW_RISCV_CSR_SMPUVA, t0
  la t0, \stack_start
  csrw LW_RISCV_CSR_SMPUPA, t0
  la t0, \stack_end
  la t1, \stack_start
  sub s1, t0, t1 // compute mapping size
  csrw LW_RISCV_CSR_SMPURNG, s1
  li t0, 0x1b //S:RW, U:RW
  csrw LW_RISCV_CSR_SMPUATTR, t0

  // Identity mapping - entry 1
  csrwi LW_RISCV_CSR_SMPUIDX, 1
  csrwi LW_RISCV_CSR_SMPUVA, 1 // VA: 0, enabled
  csrw LW_RISCV_CSR_SMPUPA, zero
  li t0, 0xFFFFFFFFFFFFF000
  csrw LW_RISCV_CSR_SMPURNG, t0
  li t0, 0x3F | (1 << 18) | (1 << 19) // S+U RWX, cacheable+coherent
  csrw LW_RISCV_CSR_SMPUATTR, t0

mpu_already_configured:
 // enable MPU
  csrw LW_RISCV_CSR_SATP, s0
  sfence.vma
  // set stack pointer
  li sp, PARTITION_STACK_VA_START
  la t0, \stack_end
  la t1, \stack_start
  sub s1, t0, t1 // compute mapping size
  add sp, sp, s1 // Add stack size
  // init stack
  addi sp, sp, -0x10
  sd zero, 0(sp)
  sd zero, 8(sp)
.endm

#endif // CC_PARTASMMACROS_H
