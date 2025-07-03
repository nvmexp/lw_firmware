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
 * @file    trace.c
 * @brief   Trace buffer driver (a.k.a. blackbox)
 *
 * This code configures trace buffer, and dump it's contents.
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <lwtypes.h>

// Register headers
#include <dev_riscv_pri.h>
#include <riscv_csr.h>

/* ------------------------ Module Includes -------------------------------- */
#include "shlib/string.h"
#include "drivers/drivers.h"
#include "sbilib/sbilib.h"
#include "memops.h"

#define TRACE_ENABLED_MASK (DRF_DEF(_PRGNLCL_RISCV, _TRACECTL, _UMODE_ENABLE, _TRUE) | \
                            DRF_DEF(_PRGNLCL_RISCV, _TRACECTL, _SMODE_ENABLE, _TRUE) | \
                            DRF_DEF(_PRGNLCL_RISCV, _TRACECTL, _MMODE_ENABLE, _TRUE))

#define LW_RISCV_TRACEPC_HI_INIT  0xdeaddead
#define LW_RISCV_TRACEPC_LO_INIT  0xdeadca75

#define LW_RISCV_TRACEPC_MAX_SIZE 64

// We use mod2 as an easy heuristic to check for erroneous values and stop trace
ct_assert(LW_RISCV_TRACEPC_LO_INIT % 2 == 1);

sysKERNEL_CODE FLCN_STATUS traceInit(void)
{
    if ((intioRead(LW_PRGNLCL_RISCV_TRACECTL) & TRACE_ENABLED_MASK) != 0)
    {
        dbgPrintf(LEVEL_INFO, "Trace buffer already configured. Not reconfiguring.\n");
    }
    else
    {
        LwU32 i;
        LwU32 bufferSize;

        // Reset/disable trace buffer
        riscvTraceCtlSet(0);

        bufferSize = DRF_VAL(_PRGNLCL_RISCV, _TRACE_RDIDX, _MAXIDX, intioRead(LW_PRGNLCL_RISCV_TRACE_RDIDX)) + 1U;

        // Pre-fill buffer to easily tell apart garbage data
        for (i = 0; i < bufferSize; i++)
        {
            intioWrite(LW_PRGNLCL_RISCV_TRACE_WTIDX, DRF_NUM(_PRGNLCL_RISCV, _TRACE_WTIDX, _WTIDX, i));
            intioRead(LW_PRGNLCL_RISCV_TRACE_WTIDX); // to be safe with posted writes
            intioWrite(LW_PRGNLCL_RISCV_TRACEPC_HI, DRF_NUM(_PRGNLCL_RISCV, _TRACEPC_HI, _PC, LW_RISCV_TRACEPC_HI_INIT));
            intioWrite(LW_PRGNLCL_RISCV_TRACEPC_LO, DRF_NUM(_PRGNLCL_RISCV, _TRACEPC_LO, _PC, LW_RISCV_TRACEPC_LO_INIT));
        }

        dbgPrintf(LEVEL_INFO, "Trace bufer enabled in stack mode.\n");

        //
        // Reset read and write indices. This is important to achieve here because it helps us identify the _FULL
        // (the "overflow happened at least once") case correctly.
        // It seems that WTIDX gets reset to MAXIDX on first ret (MMINTS-TODO: uncodumented behavior?),
        // so set both indices accordingly here.
        //
        intioWrite(LW_PRGNLCL_RISCV_TRACE_WTIDX, DRF_NUM(_PRGNLCL_RISCV, _TRACE_WTIDX, _WTIDX, bufferSize - 1U));
        intioWrite(LW_PRGNLCL_RISCV_TRACE_RDIDX, DRF_NUM(_PRGNLCL_RISCV, _TRACE_RDIDX, _RDIDX, bufferSize - 1U));

        // Enable trace
        riscvTraceCtlSet(
                   TRACE_ENABLED_MASK |
                   DRF_DEF(_PRGNLCL_RISCV, _TRACECTL, _MODE, _STACK) |
                   DRF_DEF(_PRGNLCL_RISCV, _TRACECTL, _INTR_ENABLE, _FALSE) |
                   DRF_DEF(_PRGNLCL_RISCV, _TRACECTL, _HIGH_THSHD, _INIT));
    }

    return FLCN_OK;
}

sysKERNEL_CODE void traceDump(void)
{
    LwU32  ctl;
    LwU32  ridx;
    LwU32  widx;
    LwU32  bufferSize;
    LwU32  orgRidx;
    LwBool bWasFull;

    ctl = intioRead(LW_PRGNLCL_RISCV_TRACECTL);

    if ((ctl & TRACE_ENABLED_MASK) == 0)
    {
        dbgPrintf(LEVEL_ALWAYS, "Trace buffer is not enabled. Can't dump trace.\n");
        return;
    }

    // Reset and disable buffer, we don't need it during dump
    riscvTraceCtlSet(0);

    bWasFull = FLD_TEST_DRF_NUM(_PRGNLCL_RISCV, _TRACECTL, _FULL, 1, ctl);

    if (bWasFull)
    {
        dbgPrintf(LEVEL_ALWAYS, "Trace buffer was filled at some point; "
                                "entries may have been lost, some older entries may be invalid.\n");
    }

    widx = intioRead(LW_PRGNLCL_RISCV_TRACE_WTIDX);
    widx = DRF_VAL(_PRGNLCL_RISCV, _TRACE_WTIDX, _WTIDX, widx);

    ridx = intioRead(LW_PRGNLCL_RISCV_TRACE_RDIDX);
    orgRidx = ridx;
    bufferSize = DRF_VAL(_PRGNLCL_RISCV, _TRACE_RDIDX, _MAXIDX, ridx) + 1U;
    ridx = DRF_VAL(_PRGNLCL_RISCV, _TRACE_RDIDX, _RDIDX, ridx);

    if (bufferSize > 0U)
    {
        LwU32 entry;
        const char *mode;
        LwBool bOptimizePrints = LW_FALSE;

        static LwUPtr entries[LW_RISCV_TRACEPC_MAX_SIZE];

        if (bufferSize <= LW_RISCV_TRACEPC_MAX_SIZE)
        {
            bOptimizePrints = LW_TRUE;
        }

        switch (DRF_VAL(_PRGNLCL_RISCV, _TRACECTL, _MODE, ctl))
        {
        case LW_PRGNLCL_RISCV_TRACECTL_MODE_FULL:
            mode = DEF_KSTRLIT("full");
            break;
        case LW_PRGNLCL_RISCV_TRACECTL_MODE_REDUCED:
            mode = DEF_KSTRLIT("reduced");
            break;
        case LW_PRGNLCL_RISCV_TRACECTL_MODE_STACK:
            mode = DEF_KSTRLIT("stack");
            break;
        default:
            mode = DEF_KSTRLIT("unknown");
        }

        dbgPrintf(LEVEL_ALWAYS, "Tracebuffer operates in %s mode; entries (most recent first):\n", mode);

        ridx = widx;
        for (entry = 0; entry < bufferSize; entry++)
        {
            LwUPtr pc;
            LwU32  tracepcHi;
            LwU32  tracepcLo;

            if (ridx > 0)
            {
                ridx--;
            }
            else
            {
                ridx = bufferSize - 1U;
            }

            intioWrite(LW_PRGNLCL_RISCV_TRACE_RDIDX,
                       DRF_NUM(_PRGNLCL_RISCV, _TRACE_RDIDX, _RDIDX, ridx));
            // Make sure write went through before the hi/lo reads, to be safe in case of posted writes
            intioRead(LW_PRGNLCL_RISCV_TRACE_RDIDX);

            tracepcHi = DRF_VAL(_PRGNLCL_RISCV, _TRACEPC_HI, _PC, intioRead(LW_PRGNLCL_RISCV_TRACEPC_HI));
            tracepcLo = DRF_VAL(_PRGNLCL_RISCV, _TRACEPC_LO, _PC, intioRead(LW_PRGNLCL_RISCV_TRACEPC_LO));

            // Non-mod2 values are invalid here, so stop (this likely indicates an init-marker val)
            if (tracepcLo % 2U != 0U)
            {
                break;
            }

            pc = (((LwU64)tracepcHi) << 32) | tracepcLo;

            if (bOptimizePrints)
            {
                entries[entry] = pc;
            }
            else
            {
                dbgPrintf(LEVEL_ALWAYS, "[*] %p\n", (void*)pc);
            }
        }


        if (bOptimizePrints)
        {
            LwU32 i;

            // 8 is arbitrarily chosen as the print step size
            for (i = 0; (i + 8) <= entry; i += 8)
            {
                dbgPrintf(LEVEL_ALWAYS,
                            "[*] %p\n"
                            "[*] %p\n"
                            "[*] %p\n"
                            "[*] %p\n"
                            "[*] %p\n"
                            "[*] %p\n"
                            "[*] %p\n"
                            "[*] %p\n",
                            (void*)entries[i + 0],
                            (void*)entries[i + 1],
                            (void*)entries[i + 2],
                            (void*)entries[i + 3],
                            (void*)entries[i + 4],
                            (void*)entries[i + 5],
                            (void*)entries[i + 6],
                            (void*)entries[i + 7]);
            }

            // print out the remainder
            for (; i < entry; i++)
            {
                dbgPrintf(LEVEL_ALWAYS, "[*] %p\n", (void*)entries[i]);
            }
        }
    }

    //
    // Restore read index.
    // No need to save/restore write index since we never changed it, and the
    // debug buffer has been disabled in-between.
    //
    intioWrite(LW_PRGNLCL_RISCV_TRACE_RDIDX, orgRidx);

    // Re-enable buffer
    riscvTraceCtlSet(ctl);
}

sysKERNEL_CODE void regsDump(void)
{
    LwU32 i;

    // Lwrrently only RSTAT3/4 are valid
    for (i = 3; i < 5; i++)
    {
        intioWrite(LW_PRGNLCL_RISCV_ICD_CMD,
            DRF_DEF(_PRGNLCL_RISCV, _ICD_CMD, _OPC, _RSTAT) |
            DRF_DEF(_PRGNLCL_RISCV, _ICD_CMD, _SZ, _DW) |
            DRF_NUM(_PRGNLCL_RISCV, _ICD_CMD, _IDX, i));
        SYS_FLUSH_IO();
        dbgPrintf(LEVEL_ALWAYS, "RSTAT%d          = 0x%08x %08x\n", i,
                  intioRead(LW_PRGNLCL_RISCV_ICD_RDATA1),
                  intioRead(LW_PRGNLCL_RISCV_ICD_RDATA0));
    }

    dbgPrintf(LEVEL_ALWAYS,
        "IRQMASK         = 0x%08x\n"
        "IRQTYPE         = 0x%08x\n"
        "IRQDEST         = 0x%08x\n"
        "INTRSTAT        = 0x%08x\n"
        "DBGCTL          = 0x%08x\n"
        "CPUCTL          = 0x%08x\n"
        "EXTERRSTAT      = 0x%08x\n"
        "EXTERRADDR      = 0x%08x\n"
        "EXTERRINFO      = 0x%08x\n"
        "HUBERRSTAT      = 0x%08x\n",
        intioRead(LW_PRGNLCL_RISCV_IRQMASK),
        intioRead(LW_PRGNLCL_RISCV_IRQTYPE),
        intioRead(LW_PRGNLCL_RISCV_IRQDEST),
        intioRead(LW_PRGNLCL_RISCV_INTR_STATUS),
        intioRead(LW_PRGNLCL_RISCV_DBGCTL),
        intioRead(LW_PRGNLCL_RISCV_CPUCTL),
        intioRead(LW_PRGNLCL_RISCV_PRIV_ERR_STAT),
        intioRead(LW_PRGNLCL_RISCV_PRIV_ERR_ADDR),
        intioRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO),
        intioRead(LW_PRGNLCL_RISCV_HUB_ERR_STAT));

    // Engine specific registers
    dbgPrintf(LEVEL_ALWAYS,
        "MAILBOX0        = 0x%08x\n"
        "MAILBOX1        = 0x%08x\n"
        "IRQSTAT         = 0x%08x\n"
        "DMACTL          = 0x%08x\n"
        "DMATRFCMD       = 0x%08x\n"
        "DMATRFBASE      = 0x%08x\n"
        "DMATRFMOFFS     = 0x%08x\n"
        "DMATRFFBOFFS    = 0x%08x\n"
        "IDLESTATE       = 0x%08x\n",
        intioRead(LW_PRGNLCL_FALCON_MAILBOX0),
        intioRead(LW_PRGNLCL_FALCON_MAILBOX1),
        intioRead(LW_PRGNLCL_FALCON_IRQSTAT),
        intioRead(LW_PRGNLCL_FALCON_DMACTL),
        intioRead(LW_PRGNLCL_FALCON_DMATRFCMD),
        intioRead(LW_PRGNLCL_FALCON_DMATRFBASE),
        intioRead(LW_PRGNLCL_FALCON_DMATRFMOFFS),
        intioRead(LW_PRGNLCL_FALCON_DMATRFFBOFFS),
        intioRead(LW_PRGNLCL_FALCON_IDLESTATE));

#ifdef LW_PRGNLCL_TFBIF_TRANSCFG
    dbgPrintf(LEVEL_ALWAYS, "TFBIF_CTL       = 0x%08x\n", intioRead(LW_PRGNLCL_TFBIF_CTL));
#else // LW_PRGNLCL_TFBIF_TRANSCFG
    dbgPrintf(LEVEL_ALWAYS, "FBIF_CTL        = 0x%08x\n", intioRead(LW_PRGNLCL_FBIF_CTL));
#endif // LW_PRGNLCL_TFBIF_TRANSCFG

#ifdef PMU_RTOS
    //
    // PMU mailboxes are rarely non-zero, so this doesn't cost us much space
    // in both text-mode and in raw-mode token-based logs.
    //
    LwU32 val;
    for (i = 0; i < LW_PPWR_PMU_MAILBOX__SIZE_1; i++)
    {
        val = csbRead(ENGINE_BUS(PWR_PMU_MAILBOX(i)));
        if (val != 0)
        {
            dbgPrintf(LEVEL_ALWAYS, "PMU_MAILBOX(%02d)       = 0x%08x\n",i , val);
        }
    }
#endif // PMU_RTOS
}
