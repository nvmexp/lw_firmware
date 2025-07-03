/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "cache.h"
#include "debug.h"
#include "main.h"

void halt(int line)
{
    printf("Called halt from %d\n", line);
    riscvWrite(LW_PRISCV_RISCV_ICD_CMD, LW_PRISCV_RISCV_ICD_CMD_OPC_STOP);
}

LwBool exceptionExpectSet = LW_FALSE;
LwU64 exceptionExpectCause = 0;

void exceptionExpect(LwU64 cause)
{
    exceptionExpectSet = LW_TRUE;
    exceptionExpectCause = cause;
}

void exceptionReset(void)
{
    exceptionExpectSet = LW_FALSE;
    exceptionExpectCause = 0;
}

void mToS(void)
{
    LwU64 mstatus = csr_read(LW_RISCV_CSR_MSTATUS);
    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _SUPERVISOR, mstatus));
    csr_write(LW_RISCV_CSR_MEPC, &&done);
    __asm__ volatile ("mret");
    done:
    __asm__ volatile ("nop"); // Fix gcc bug

    return;
}

void mToU(void)
{
    LwU64 mstatus = csr_read(LW_RISCV_CSR_MSTATUS);
    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _USER, mstatus));
    csr_write(LW_RISCV_CSR_MEPC, &&done);
    __asm__ volatile ("mret");
    done:
    __asm__ volatile ("nop"); // Fix gcc bug

    return;
}

static void skipInstruction(void)
{
    LwU64 mepc = csr_read(LW_RISCV_CSR_MEPC);
    LwU16 inst = *((LwU16 *) mepc);

    // Check if instruction is 2 or 4 bytes when skipping it
    if ((inst & 0x3) == 0x3)
        csr_write(LW_RISCV_CSR_MEPC, mepc + 4);
    else
        csr_write(LW_RISCV_CSR_MEPC, mepc + 2);
}


LwBool tryLoad(LwU64 addr, LwU32 *pVal)
{
    LwBool ret;
    LwU32 val;

    lwfenceAll();
    exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT);
    lwfenceAll();
    __asm__ __volatile__ ("lw %0, 0(%1)" : "=r"(val): "r"(addr): );
    lwfenceAll();
    ret = exceptionExpectSet;
    exceptionReset();
    if (ret && pVal)
    {
        *pVal = val;
    }
    return ret;
}

LwBool tryStore(LwU64 addr, LwU32 val)
{
    LwBool ret;

    lwfenceAll();
    exceptionExpect(LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT);
    lwfenceAll();
    __asm__ __volatile__ ("sw %0, 0(%1)" : : "r"(val), "r"(addr): );
    lwfenceAll();
    ret = exceptionExpectSet;
    exceptionReset();
    return ret;
}

void tcmTest(void);
void fpuTest(void);
void csrTest(void);

int main()
{
    if (csr_read(LW_RISCV_CSR_PMPCFG0) == 0)
    {
        printf("Running without bootrom. HALT!\n");
        HALT();
    }

    // configure fbif
    engineWrite(ENGINE_REG(_FBIF_CTL),
                  (DRF_DEF(_PFALCON_FBIF, _CTL, _ENABLE, _TRUE) |
                   DRF_DEF(_PFALCON_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
    engineWrite(ENGINE_REG(_FBIF_CTL2),
                 LW_PFALCON_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);

    // Mateusz hacks
    riscvWrite(LW_PRISCV_RISCV_DBGCTL, 0x1FFFF);
    csr_write(LW_RISCV_CSR_MDBGCTL, 1);
    bar0Write(ENGINE_REG(_FALCON_LOCKPMB), 0);
    riscvWrite(LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN, 0);
    // engineWrite(pEngine, LW_PFALCON_FALCON_OS, 0x01a12c4b);

    csr_write(LW_RISCV_CSR_MCFG, 0xbb0a); // enable posted writes
    csr_write(LW_RISCV_CSR_MBPCFG, 0x7); // turn on branch predictors

    tcmTest();
    fpuTest();
    csrTest();

    printf("Tests finished. Entering ICD.\n");
    enterIcd();
    return 0;
}

void mtrap_handler(void)
{
    LwU64 mstatus = csr_read(LW_RISCV_CSR_MSTATUS);
    LwU64 mcause = csr_read(LW_RISCV_CSR_MCAUSE);

    if (((LwS64) mcause) < 0)
    {
        // Interrupt
        HALT();
    }
    else
    {
        // Trap
        if (exceptionExpectSet)
        {
            if (mcause == exceptionExpectCause)
            {
                exceptionExpectSet = LW_FALSE;
            }
            else
            {
                printf("Exception, mcause: %d\n", mcause);
                HALT();
            }

            skipInstruction();
            return;
        }

        switch (mcause & 0xFUL) {
            case LW_RISCV_CSR_MCAUSE_EXCODE_UCALL:
            case LW_RISCV_CSR_MCAUSE_EXCODE_SCALL:
                csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _MACHINE, mstatus));
                skipInstruction();
                break;
            default:
                printf("Exception, mcause: %d\n", mcause);
                HALT();
                break;
        }
    }
}

void strap_handler(void)
{
    printf("REEEEEEEEEEEEEEE\n");
}

