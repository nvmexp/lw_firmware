/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   start.S
 * @brief  Entrypoint, some testing
 */

#include <lwriscv-csr.h>
#include "fail_codes.h"

#define MPU_REGION_COUNT 64

        .section        .text.front
/* Test entrypoint shouldn't be fixed to "first image address" */
textFront:
        li      a0, FAILCODE_ENTRYPOINT_WRONG
        jal     failed

        .section        .rdata.front, "a"
/* Test section loading */
rdataFront:
        .word   0xDEADCAFE

        .section        .data.front
/* Test section loading */
dataFront:
        .word   0xBEA7D00D

        .section        .bss.front
bssFront:
        .word   0

        .section        .noload0
noload0Front:
        .word   0xBADD00D5

        .section        .noload1
noload1Front:
        .word   0x5ADFAC75

        .section        .noload2
noload2Front:
        .word   0xBADBEEF

        .section        .text
/* Test entrypoint shouldn't be fixed to first address in .text */
        li      a0, FAILCODE_ENTRYPOINT_WRONG
        jal     failed

        .globl  _start
_start:
/* NOTE: If bootloader passes arguments, checking some a regs should be skipped */
        bnez    a0, failed_a0
        li      a0, FAILCODE_GPRS_NOT_CLEARED
/* Test GPRs cleared */
/* Do not check ra, as it contains address of _start */
        bnez    sp, failed
        bnez    gp, failed
        bnez    tp, failed
        bnez    t0, failed
        bnez    t1, failed
        bnez    t2, failed
        bnez    s0, failed
        bnez    s1, failed
        bnez    a1, failed
        bnez    a2, failed
        bnez    a3, failed
        bnez    a4, failed
        bnez    a5, failed
        bnez    a6, failed
        bnez    a7, failed
        bnez    s2, failed
        bnez    s3, failed
        bnez    s4, failed
        bnez    s5, failed
        bnez    s6, failed
        bnez    s7, failed
        bnez    s8, failed
        bnez    s9, failed
        bnez    s10, failed
        bnez    s11, failed
        bnez    t3, failed
        bnez    t4, failed
        bnez    t5, failed
        bnez    t6, failed

/* Verify mstatus is zero (no MPU, no interrupts) */
        li      a0, FAILCODE_MSTATUS_NOT_CORRECT
        csrr    t0, mstatus
        li      t1, 0
        bne     t0, t1, failed

/* Get base address of performance counters */
        li      s0, 0x100000000 + DMEM_SIZE
        lwu     t1, -4(s0)
        li      t2, 0xF007BA11
        bne     t1, t2, 1f
        /* Using DMEM debug ring buffer, adjust s0 to behind it */
        lwu     t1, -8(s0)
        addi    t1, t1, 16
        sub     s0, s0, t1
1:
        /* s0 = base of perfctrs */
        addi    s0, s0, -3 * 8

/* Check performance counters */
        csrr    t1, cycle
        li      a0, FAILCODE_PERFCTR_CYCLE_ZERO
        beqz    t1, failed
        ld      t2, 0(s0)
        li      a0, FAILCODE_PERFCTR_CYCLE_STUCK
        beq     t1, t2, failed

        csrr    t1, time
        li      a0, FAILCODE_PERFCTR_TIME_ZERO
        beqz    t1, failed
        ld      t2, 8(s0)
        li      a0, FAILCODE_PERFCTR_TIME_STUCK
        beq     t1, t2, failed

        csrr    t1, instret
        li      a0, FAILCODE_PERFCTR_INSTRET_ZERO
        beqz    t1, failed
        ld      t2, 16(s0)
        li      a0, FAILCODE_PERFCTR_INSTRET_STUCK
        beq     t1, t2, failed

/* Verify MPU regions are cleared */
        li      a0, FAILCODE_MPU_REGION_CONTENTS
        li      t0, MPU_REGION_COUNT
1:
        addi    t0, t0, -1
        csrw    CSR_LW_MMPUIDX, t0
        csrr    t1, CSR_LW_MMPUVA
        csrr    t2, CSR_LW_MMPUPA
        csrr    t3, CSR_LW_MMPURNG
        csrr    t4, CSR_LW_MMPUATTR
        bnez    t1, failed
        bnez    t2, failed
        bnez    t3, failed
        bnez    t4, failed

        bnez    t0, 1b

/* Verify sections are loaded properly */
        li      a0, FAILCODE_TEXT_SECTION
        lwu     t0, textFront
        li      t1, 0x00030537
        bne     t0, t1, failed

        li      a0, FAILCODE_RODATA_SECTION
        lwu     t0, rdataFront
        li      t1, 0xDEADCAFE
        bne     t0, t1, failed

        li      a0, FAILCODE_DATA_SECTION
        lwu     t0, dataFront
        li      t1, 0xBEA7D00D
        bne     t0, t1, failed

        li      a0, FAILCODE_BSS_SECTION
        lwu     t0, bssFront
        li      t1, 0
        bne     t0, t1, failed

        li      a0, FAILCODE_STACK_SECTION
        lwu     t0, stackFront
        li      t1, 0
        bne     t0, t1, failed

        li      a0, FAILCODE_NOLOAD0_SECTION
        lwu     t0, noload0Front
        li      t1, 0xBADD00D5
        beq     t0, t1, failed

        li      a0, FAILCODE_NOLOAD1_SECTION
        lwu     t0, noload1Front
        li      t1, 0x5ADFAC75
        beq     t0, t1, failed

        li      a0, FAILCODE_NOLOAD2_SECTION
        lwu     t0, noload2Front
        li      t1, 0xBADBEEF
        beq     t0, t1, failed

/* Check DTCM */
#define BOOTLOADER_STACK_SIZE 0x240
        li      a0, FAILCODE_DTCM_NOT_CLEAN
        addi    t0, s0, -BOOTLOADER_STACK_SIZE
        li      t3, 0x5A5A5A5A5A5A5A5A
        li      t1, 0x100000000
1:
        ld      t2, (t1)
        bne     t2, t3, failed
        add     t1, t1, 8
        bne     t1, t0, 1b

/* We would check ITCM here, but we can't actually know where the bootloader is. */

/* Check exception handler
 * Since there are only three instructions (lui a0, 1 ; addiw a0, a0, 0xBAD ; j dbgExit)
 * this is a pretty short check. We can't actually check the third instruction,
 * because it's a relative jump, and will change any time the bootloader is recompiled.
 * Theoretically just checking the first two instructions should be enough.
 */
        li      a0, FAILCODE_BAD_EXCEPTION_HANDLER
        csrr    t0, mtvec
        lwu     t1, 0(t0)
        li      t2, 0x00001537  /* li a0, 0xBAD : lui a0, 0x1 */
        bne     t1, t2, failed
        lwu     t1, 4(t0)
        li      t2, 0xBAD5051B  /*              : addiw a0, a0, 0xBAD */
        bne     t1, t2, failed

/* All tests passed! */
        li      a0, FAILCODE_SUCCESS
        csrw    CSR_LW_MTOHOST, a0
1:      j       1b

failed_a0:
        li      a0, FAILCODE_GPRS_NOT_CLEARED
        j       failed

failed:
        csrw    CSR_LW_MTOHOST, a0
1:      j       1b
