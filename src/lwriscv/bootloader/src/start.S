/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   start.S
 * @brief  Entrypoint, straps main.
 */

#include "engine.h"
#include "dmem_addrs.h"
#include "riscv_asm_helpers.h"
#include <riscv_csr.h>

/*
 * The assembler doesn't understand DRF macros due to the ternary operator,
 * but the C preprocessor does, so we can use that to somewhat get around using
 * hard-coded field offsets.
 */
 #define RISCV_IRQMSET_MEMERR 18
 #if RISCV_IRQMSET_MEMERR != (0 ? LW_PRGNLCL_RISCV_IRQMSET_MEMERR)
 #error "Wrong bit for IRQMSET_MEMERR!"
 #endif

        .section        .text.init
        .globl  _entrypoint
        .globl  selfBase
selfBase:
_entrypoint:
        j       _start

        .section        .text
_start:
/*
 * Initialize x1 (i.e. ra) to a valid value, otherwise its value will be "X" on simulator and
 * will cause HW assertion on simulator when SW writes it to somewhere (Bug 2055192).
 *
 * Other GPR is normally initialized by SW before using it, so we are not explicitly initialize
 * them here.
 */
        li      x1, 0

/* Install trap handler */
        la      t0, _trapEntry
        csrw    stvec, t0

/* Set up MEMERR interrupt */
        li      t0, 1 << RISCV_IRQMSET_MEMERR /* MEMERR bit, can't use DRF here */
/* Set MEMERR to LVL */
        li      a0, LW_PRGNLCL_FALCON_IRQMODE
        lw      t1, 0(a0)
        or      t1, t1, t0 // LVL == 1
        sw      t1, 0(a0)
/* Clear MEMERR sources */
        li      a0, LW_PRGNLCL_RISCV_PRIV_ERR_STAT
        sw      zero, 0(a0)
        li      a0, LW_PRGNLCL_RISCV_HUB_ERR_STAT
        sw      zero, 0(a0)
/* Enable MEMERR to catch invalid PRI writes */
        li      a0, LW_PRGNLCL_RISCV_IRQMSET
        sw      t0, 0(a0)

/* Checking 64-bit vs 32-bit. */
        li      a0, 4
        li      t0, 1
        slli    t0, t0, 31
/* Compiled for wrong size! */
#if __riscv_xlen == 64
        bltz    t0, _hwcheckFailed
#else
        bgez    t0, _hwcheckFailed
#endif

/* Set stack pointer near end of DMEM. */
        li      sp, DMEM_STACK_BASE + STACK_SIZE

        /* ZZZZ */
        li      t4, 0x5A5A5A5A
#if __riscv_xlen == 64
        slli    t5, t4, 32
        add     t4, t4, t5
#endif

/* bootloaderMain(selfBase, _ucodeCfg) */
        la      a0, selfBase
        la      a1, _ucodeCfg
        j       bootloaderMain

_hwcheckFailed:
/* a0 contains the "section" that failed, details are in other regs. */
        j       dbgExit

        .section        .text
_trapEntry:
        li      a0, 0xBAD

        /* Swap exception handler so we won't get into infinite exception */
        la      t0, _hang
        csrw    stvec, t0
        /* fall-thru */
FUNC dbgExit
        /* Enter ICD, that's what RTOS does */
        li      t1, LW_PRGNLCL_RISCV_ICD_CMD
        sw      zero, 0(t1)
        fence iorw, iorw
        /* fall-thru */
EFUNC dbgExit

_hang:
        j       _hang

        .section        .LWPU.ucode
        .p2align 3
_ucodeCfg:
/* Actual ucode config and ELF file goes here, concatenated to the end of the binary. */
