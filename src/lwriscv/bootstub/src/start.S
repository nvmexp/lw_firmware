/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
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

#include "bootstub.h"
#include "dmem_addrs.h"
#include "engine.h"
#include "riscv_asm_helpers.h"

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
_selfBase:
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

        /* Set stack pointer near end of DMEM. */
        li      sp, DMEM_STACK_BASE + STACK_SIZE

        /* Check for return from partition-switch */
        li      t0, RETURN_FLAG_BASE
        ld      t1, 0(t0)
        bne     t1, zero, _partitionReturn
        li      t1, 1
        sd      t1, 0(t0)

        /* Set up MEMERR interrupt */
        li      t0, 1 << RISCV_IRQMSET_MEMERR /* MEMERR bit, can't use DRF here */
        /* Set MEMERR to LVL */
        li      t2, LW_PRGNLCL_FALCON_IRQMODE
        lw      t1, 0(t2)
        or      t1, t1, t0 // LVL == 1
        sw      t1, 0(t2)
        /* Clear MEMERR sources */
        li      t2, LW_PRGNLCL_RISCV_PRIV_ERR_STAT
        sw      zero, 0(t2)
        li      t2, LW_PRGNLCL_RISCV_HUB_ERR_STAT
        sw      zero, 0(t2)
        /* Enable MEMERR to catch invalid PRI writes */
        li      t2, LW_PRGNLCL_RISCV_IRQMSET
        sw      t0, 0(t2)

        /* Checking 64-bit vs 32-bit. */
        li      t0, 1
        slli    t0, t0, 31
        /* Compiled for wrong size! */
#if __riscv_xlen == 64
        bltz    t0, _hwcheckFailed
#else
        bgez    t0, _hwcheckFailed
#endif

        /* bootstubMain(_selfBase, _paramBase) */
        la      a0, _selfBase
        la      a1, _paramBase
        j       bootstubMain

_partitionReturn:
        /* Save arguments. */
        mv      s0, a0
        mv      s1, a1
        mv      s2, a2
        mv      s3, a3
        mv      s4, a4
        mv      s5, a5
        mv      s6, a6
        mv      s7, a7

        /* ra = bootstubGetReturnAddress(_paramBase) */
        la      a0, _paramBase
        jal     ra, bootstubGetReturnAddress
        mv      ra, a0

        /* Restore arguments. */
        mv      a0, s0
        mv      a1, s1
        mv      a2, s2
        mv      a3, s3
        mv      a4, s4
        mv      a5, s5
        mv      a6, s6
        mv      a7, s7

        /* Return to application. */
        jr      ra

_hwcheckFailed:
        /* Set error code. */
        li      a0, BOOTSTUB_ERROR_HARDWARE_CHECK
        j       dbgExit

_trapEntry:
        li      a0, BOOTSTUB_ERROR_EXCEPTION

        /* Swap exception handler so we won't get into infinite exception */
        la      t0, _hang
        csrw    stvec, t0

        /* fall-thru */

FUNC dbgExit
        /* Enter ICD, that's what RTOS does */
        li      t1, LW_PRGNLCL_RISCV_ICD_CMD
        sw      zero, 0(t1)
        fence   iorw, iorw

        /* fall-thru */

EFUNC dbgExit

_hang:
        j       _hang

        .section        .LWPU.bootParams
        .p2align 3
_paramBase:
        /* Boot parameters are concatenated here by rvmkfmc.pl. */
