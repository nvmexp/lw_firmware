/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    irq.c
 * @brief   Interrupt driver - engine-independent
 *
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <flcnifcmn.h>
#include <lwctassert.h>
#include <lw_rtos_extension.h>
#include <osptimer.h>

/* ------------------------ Register Includes ------------------------------ */
#include <dev_falcon_v4.h>
#include <dev_riscv_pri.h>
#include <riscv_csr.h>
#include <lwriscv-mpu.h>
#include <riscv-intrinsics.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
// MK TODO: this is required to link with SafeRTOS core (that we must link with).
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>

#include <portfeatures.h>
#include <task.h>
#include <lwrtos.h>
#include <lwrtosHooks.h>

/* ------------------------ Module Includes -------------------------------- */
#include "shlib/string.h"
#include "shlib/syscall.h"
#include "drivers/drivers.h"
#include "drivers/mpu.h"

// Shorthand for the many string literals in this file
#define _KS(x) DEF_KSTRLIT(x)

// MMINTS-TODO: 500 ms for now, re-evalute timeout to a more reasonable value later.
#define LW_RISCV_IRQ_SWGENSET_WAIT_TIMEOUT_MS (500U)
#define LW_RISCV_IRQ_SWGENSET_WAIT_TIMEOUT_US (LW_RISCV_IRQ_SWGENSET_WAIT_TIMEOUT_MS * 1000U)

//------------------------------------------------------------------------------
// External Functions
//------------------------------------------------------------------------------

void vPortTrapHandler(void);



//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------

sysKERNEL_CODE FLCN_STATUS
exceptionInit(void)
{
    csr_write(LW_RISCV_CSR_STVEC, vPortTrapHandler);
    dbgPrintf(LEVEL_INFO, "Exception vector at %p\n", vPortTrapHandler);

    return FLCN_OK;
}

sysKERNEL_CODE FLCN_STATUS
irqFireSwGen(LwU8 no)
{
    //
    // DO NOT put any prints in this function or you will loop to death.
    // This function is also called when print buffer itself is full.
    //
    switch (no)
    {
    case SYS_INTR_SWGEN0:
        intioWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN0, _SET));
        break;
    case SYS_INTR_SWGEN1:
        intioWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_DEF(_PRGNLCL, _FALCON_IRQSSET, _SWGEN1, _SET));
        break;
    default:
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    return FLCN_OK;
}

/*!
 * This function checks if a SWGEN intr is cleared.
 *
 * @param[in]   pNo   ptr to 8-bit SWGEN no value (0/1).
 *
 * @return 'LW_TRUE'    when the swgen intr is cleared (also returned for invalid swgen ids).
 * @return 'LW_FALSE'   when the swgen intr is not cleared
 */
sysKERNEL_CODE LwBool
irqIsSwGenClear(void *pNo)
{
    LwU8  no = *((LwU8*)pNo);
    LwU32 irqStat = intioRead(LW_PRGNLCL_FALCON_IRQSTAT);

    switch (no)
    {
    case SYS_INTR_SWGEN0:
        return FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _SWGEN0, _FALSE, irqStat);
    case SYS_INTR_SWGEN1:
        return FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _SWGEN1, _FALSE, irqStat);
    default:
        return LW_TRUE;
    }
}

/*!
 * This function is to be called by code that wants to wait until a SWGEN intr is cleared.
 * Does a busy wait.
 * Timeout here is considered fatal.
 *
 * @param[in]   no   SWGEN no (0/1).
 *
 * @return 'FLCN_OK'          when the swgen intr is cleared.
 * @return 'FLCN_ERR_TIMEOUT' if a timeout has been hit on waiting for the swgen intr to be cleared.
 */
sysKERNEL_CODE FLCN_STATUS
irqWaitForSwGenClear(LwU8 no)
{
    // Wait until debug buffer is no longer empty
    if (!OS_PTIMER_SPIN_WAIT_US_COND(irqIsSwGenClear,
        &no, LW_RISCV_IRQ_SWGENSET_WAIT_TIMEOUT_US))
    {
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}

sysKERNEL_CODE void
lwrtosRiscvSwIrqHook(void)
{
    dbgPrintf(LEVEL_ALWAYS,  "Unexpected SW Interrupt.\n" );
    kernelVerboseCrash(0);
}

sysKERNEL_CODE void
kernelVerboseOops(struct xPortTaskControlBlock *pTcb)
{
    xPortTaskContext *pCtx = &pTcb->xCtx;

    dbgPrintf(LEVEL_ALWAYS,
              "AppVersion: %d\n"
              "Current ctx: %p, task: %p\n"
              "pvData: %p, pc: %p\n",
              intioRead(LW_PRGNLCL_FALCON_OS),
              pCtx, pxLwrrentTCB,
              ((pxLwrrentTCB != NULL) ? pxLwrrentTCB->pvObject : NULL), (void*)pCtx->uxPc);

    if ((pCtx == NULL) && (pxLwrrentTCB != NULL))
    {
        pCtx = &pxLwrrentTCB->xCtx;
    }

    if (pCtx != NULL)
    {
#ifdef PMU_RTOS
        // Stash PC and taskID for NOCAT for later access from RM
        intioWrite(LW_PRGNLCL_FALCON_MAILBOX0, (LwU32)(pCtx->uxPc));
        if (pxLwrrentTCB != NULL)
        {
            intioWrite(LW_PRGNLCL_FALCON_MAILBOX1, ((PRM_RTOS_TCB_PVT)pxLwrrentTCB->pvObject)->taskID);
        }
        else
        {
            intioWrite(LW_PRGNLCL_FALCON_MAILBOX1, 0xFFFFFFFF);
        }
#endif // PMU_RTOS

        // Dump registers
        dbgPrintf(LEVEL_ALWAYS,
            "xstatus 0x%lx, flags 0x%lx, xie 0x%lx\n\n"
            "GPRs:\n"
            "ra = %p, sp = %p,\n"
            "gp = 0x%016lx, tp = 0x%016lx,\n"
            "t0 = 0x%016lx, t1 = 0x%016lx, t2 = 0x%016lx,\n"
            "s0 = 0x%016lx, s1 = 0x%016lx,\n"
            "a0 = 0x%016lx, a1 = 0x%016lx, a2 = 0x%016lx, a3 = 0x%016lx,\n"
            "a4 = 0x%016lx, ",
            pCtx->uxSstatus, pCtx->uxFlags, pCtx->uxSie,
            (void*)pCtx->uxGpr[LWRISCV_GPR_RA], (void*)pCtx->uxGpr[LWRISCV_GPR_SP],
            pCtx->uxGpr[LWRISCV_GPR_GP], pCtx->uxGpr[LWRISCV_GPR_TP],
            pCtx->uxGpr[LWRISCV_GPR_T0], pCtx->uxGpr[LWRISCV_GPR_T1], pCtx->uxGpr[LWRISCV_GPR_T2],
            pCtx->uxGpr[LWRISCV_GPR_S0], pCtx->uxGpr[LWRISCV_GPR_S1],
            pCtx->uxGpr[LWRISCV_GPR_A0], pCtx->uxGpr[LWRISCV_GPR_A1], pCtx->uxGpr[LWRISCV_GPR_A2], pCtx->uxGpr[LWRISCV_GPR_A3],
            pCtx->uxGpr[LWRISCV_GPR_A4]);
        dbgPrintf(LEVEL_ALWAYS,
            "a5 = 0x%016lx, a6 = 0x%016lx, a7 = 0x%016lx,\n"
            "s2 = 0x%016lx, s3 = 0x%016lx, s4 = 0x%016lx, s5 = 0x%016lx,\n"
            "s6 = 0x%016lx, s7 = 0x%016lx, s8 = 0x%016lx, s9 = 0x%016lx,\n"
            "s10 = 0x%016lx, s11 = 0x%016lx,\n"
            "t3 = 0x%016lx, t4 = 0x%016lx, t5 = 0x%016lx, t6 = 0x%016lx\n\n",
            pCtx->uxGpr[LWRISCV_GPR_A5], pCtx->uxGpr[LWRISCV_GPR_A6], pCtx->uxGpr[LWRISCV_GPR_A7],
            pCtx->uxGpr[LWRISCV_GPR_S2], pCtx->uxGpr[LWRISCV_GPR_S3], pCtx->uxGpr[LWRISCV_GPR_S4], pCtx->uxGpr[LWRISCV_GPR_S5],
            pCtx->uxGpr[LWRISCV_GPR_S6], pCtx->uxGpr[LWRISCV_GPR_S7], pCtx->uxGpr[LWRISCV_GPR_S8], pCtx->uxGpr[LWRISCV_GPR_S9],
            pCtx->uxGpr[LWRISCV_GPR_S10], pCtx->uxGpr[LWRISCV_GPR_S11],
            pCtx->uxGpr[LWRISCV_GPR_T3], pCtx->uxGpr[LWRISCV_GPR_T4], pCtx->uxGpr[LWRISCV_GPR_T5], pCtx->uxGpr[LWRISCV_GPR_T6]);
    }

    if (LWRISCV_MPU_DUMP_ENABLED == 1)
    {
        mpuDump();
    }

    traceDump();

    regsDump();
}

sysKERNEL_CODE
void irqHandleMemerr(void)
{
    dbgPrintf(LEVEL_ALWAYS, "\n*** RISCV ENCOUNTERED MEMERR ***\n\n");

    // Check if it's HUB or PRI error (or both)
    if (FLD_TEST_DRF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _VALID, _TRUE, intioRead(LW_PRGNLCL_RISCV_PRIV_ERR_STAT)))
    {
        LwU64 priAddr = 0;

#ifdef LW_PRGNLCL_RISCV_PRIV_ERR_ADDR_HI
        priAddr = intioRead(LW_PRGNLCL_RISCV_PRIV_ERR_ADDR_HI);
        priAddr <<= 32;
#endif // LW_PRGNLCL_RISCV_PRIV_ERR_ADDR_HI
        priAddr |= intioRead(LW_PRGNLCL_RISCV_PRIV_ERR_ADDR);

        dbgPrintf(LEVEL_ALWAYS, "*** PRI write error at 0x%llx, err_info: 0x%08x ***\n",
                  priAddr,
                  intioRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO));

        // Clear error
        intioWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, 0);
    }

    if (FLD_TEST_DRF(_PRGNLCL, _RISCV_HUB_ERR_STAT, _VALID, _TRUE, intioRead(LW_PRGNLCL_RISCV_HUB_ERR_STAT)))
    {
        dbgPrintf(LEVEL_ALWAYS, "*** HUB write error ***\n");

        // Clear error
        intioWrite(LW_PRGNLCL_RISCV_HUB_ERR_STAT, 0);
    }

    kernelVerboseCrash(0);
}

sysKERNEL_CODE
void irqHandleIopmpErr(void)
{
    // Check if there is a valid error and only display error details if there is.
    if (FLD_TEST_DRF(_PRGNLCL, _RISCV_IOPMP_ERR_STAT, _VALID, _FALSE, intioRead(LW_PRGNLCL_RISCV_IOPMP_ERR_STAT)))
    {
        dbgPrintf(LEVEL_ALWAYS, "Bad call to irqHandleIopmpErr\n");
    }
    else
    {
        LwU64 iopmpAddr = 0;

        dbgPrintf(LEVEL_ALWAYS, "\n*** RISCV ENCOUNTERED an IOPMP ERROR ***\n\n");

#ifdef LW_PRISCV_RISCV_IOPMP_ERR_ADDR_HI
        iopmpAddr = intioRead(LW_PRGNLCL_RISCV_IOPMP_ERR_ADDR_HI);
        iopmpAddr <<= 32;
#endif // LW_PRISCV_RISCV_IOPMP_ERR_ADDR_HI
        iopmpAddr |= intioRead(LW_PRGNLCL_RISCV_IOPMP_ERR_ADDR_LO);
        iopmpAddr <<= 2; // IOPMP_ERR_ADDR reports a word-address, so shift it left by 2 bits.

        dbgPrintf(LEVEL_ALWAYS, "*** IOPMP error at 0x%llx, err_info: 0x%08x, capen: 0x%08x ***\n",
                  iopmpAddr,
                  intioRead(LW_PRGNLCL_RISCV_IOPMP_ERR_INFO),
                  intioRead(LW_PRGNLCL_RISCV_IOPMP_ERR_CAPEN));
    }

    //
    // MMINTS-TODO: this causes a crash on a few Windows tests in DVS.
    // Accesses seem to originate from outside of RM. This needs to be fixed.
    // Alternatively, we can consider disabling PMB in CAPEN on some chips
    // if we want to avoid having PMU crash on getting its TCM probed.
    //
    //kernelVerboseCrash(0);
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, DRF_DEF(_PRGNLCL, _RISCV_IRQMCLR, _IOPMP, _SET)); // remove this when no longer needed
}

sysKERNEL_CODE void
kernelVerboseCrash(LwU64 cause)
{
    // gp should always be set to the context that caused exception
    register xTCB *pTcb asm("gp");
    register xPortTaskContext *pCtx asm("gp");

    dbgPrintf(LEVEL_ALWAYS, "\n**** RTOS HAS CRASHED ****\n\n");

#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
    if (fbhubGatedGet())
    {
        dbgPrintf(LEVEL_ALWAYS, "\n**** WHILE FBHUB WAS GATED ****\n\n");
    }
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)

    kernelVerboseOops(pTcb);

    coreDump(pCtx, cause);

    //
    // We stop to speedup debugging and not hide problems.
    // For production it should have fallback modes.
    //
    appHalt();
}

sysKERNEL_CODE void
lwrtosRiscvExceptionHook(LwrtosTaskHandle handle,
                         LwU64 cause,
                         LwU64 cause2,
                         LwU64 trapVal)
{
    xTCB *pTcb = taskGET_TCB_FROM_HANDLE(handle);
    xPortTaskContext *pCtx = &(pTcb->xCtx);
    LwBool isIllegalInstr = (cause == LW_RISCV_CSR_SCAUSE_EXCODE_ILL);

    const char *xExceptionDecoder[] =
    {
        _KS("Instruction address misaligned"),
        _KS("Instruction access fault"),
        _KS("Illegal instruction"),
        _KS("Breakpoint"),
        _KS("Load address misaligned"),
        _KS("Load access fault"),
        _KS("Store address misaligned"),
        _KS("Store access fault"),
    };

    //
    // Context must be first element of TCB because that is assumption of assembly
    // when we hit exception from kernel mode (it saves kernel context to stack,
    // and passess it here as task handle).
    //
    ct_assert(LW_OFFSETOF(xTCB, xCtx) == 0);

    dbgPrintf(LEVEL_ALWAYS,
              "\nException cause %llx cause2 %llx at %p",
              cause, cause2, (pCtx != NULL ? (void*)pCtx->uxPc : NULL));

    // For illegal instruction, stval is failing instruction (opcode)
    if (isIllegalInstr)
    {
        dbgPrintf(LEVEL_ALWAYS, " badinstr 0x%llx", trapVal);
    }
    else if (cause != LW_RISCV_CSR_SCAUSE_EXCODE_BKPT)
    {
        // For non-illegal instruction traps, stval is address. Skip on bp for readability
        dbgPrintf(LEVEL_ALWAYS, " badaddr %p", (void*)trapVal);
    }

    if (cause < LW_ARRAY_ELEMENTS(xExceptionDecoder))
    {
        dbgPrintf(LEVEL_ALWAYS, ": %s\n", xExceptionDecoder[cause]);
    }
    else
    {
        dbgPrintf(LEVEL_ALWAYS, "\n");
    }

    if (cause == LW_RISCV_CSR_SCAUSE_EXCODE_BKPT)
    {
        LwU64 arg     = pCtx->uxGpr[LWRISCV_GPR_A0];
        LwU64 haltLoc = DRF_VAL(_LWRISCV_INTRINSIC, _TRAP_ARG, _LOC, arg);

        if (FLD_TEST_DRF(_LWRISCV_INTRINSIC, _TRAP_ARG, _BP_TYPE, _HALT, arg))
        {
#if LWRISCV_PRINT_RAW_MODE
            //
            // With raw-mode, instead of passing a lineno, we pass a logging metadata
            // pointer (shifted by 1)
            //
            LwUPtr tokens[] = { (LwUPtr)(haltLoc << 1U) }; // metadata ptr shifted by 1
            printRawModeDispatch(1U, tokens);
#else // LWRISCV_PRINT_RAW_MODE
            dbgPrintf(LEVEL_ALWAYS, "Halted at line number %lld\n", haltLoc);
#endif // LWRISCV_PRINT_RAW_MODE

            kernelVerboseCrash(cause);
        }
        else
        {
            // MMINTS-TODO! Implement resumable breakpoint here!
            kernelVerboseCrash(cause);
        }
    }
    else
    {
        kernelVerboseCrash(cause);
    }
}
