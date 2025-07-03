/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>

#include <lwriscv/csr.h>
#include <lwriscv/peregrine.h>
#include <lwriscv/fence.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>

#include "utils.h"
#include "testharness.h"

/*
 *  Misc functions that must be in identity mapping but are not part of fmc
 */

LwU64 g_beginOfImemFreeSpace = 0;

void appRunFromImem(void)
{
    csr_write(LW_RISCV_CSR_SMPUIDX, 8);
    csr_clear(LW_RISCV_CSR_SMPUVA, 1);
    __asm__ __volatile__("sfence.vma");
    __asm__ __volatile__("fence.i");
}

void appRunFromFb(void)
{
    csr_write(LW_RISCV_CSR_SMPUIDX, 8);
    csr_set(LW_RISCV_CSR_SMPUVA, 1);
    __asm__ __volatile__("sfence.vma");
    __asm__ __volatile__("fence.i");
}

void appEnableRiscvPerf(void)
{
    sToM();

    csr_write(LW_RISCV_CSR_MSTATUS,(csr_read(LW_RISCV_CSR_MSTATUS) & 0xFFFFFFFFFFFDE7FFULL)
                                                                   | 0x0000000000002800ULL); //might wanna enable FPU...

    //turn on all perf features
    csr_write(LW_RISCV_CSR_MCFG, 0xbb0a); // enable posted writes
    csr_write(LW_RISCV_CSR_MBPCFG, 0x7); // turn on branch predictors
    csr_write(LW_RISCV_CSR_MMISCOPEN, 1); //enable cacheops
    __asm__ __volatile__("fence.i");
    riscvFenceRWIO();
    mToS();
}


void sToM(void)
{
    __asm__ volatile ("ecall");
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

void releasePriLockdown(void)
{
    sToM();
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));
    mToS();
}

void appShutdown(void)
{
    sToM();
    csr_write(LW_RISCV_CSR_MIE, 0);
    riscvLwfenceIO();
    __asm__ volatile ("csrrwi zero, %0, %1" : :"i"(LW_RISCV_CSR_MOPT), "i"( DRF_DEF64(_RISCV, _CSR_MOPT, _CMD, _HALT)));
    while(1);
}

void changeFbCachable(LwBool bEnable)
{
    for (unsigned i=0; i<128; ++i)
    {
        csr_write(LW_RISCV_CSR_SMPUIDX, i);
        if (csr_read(LW_RISCV_CSR_SMPUVA) & 1) // enabled entry
        {
            LwU64 attr = csr_read(LW_RISCV_CSR_SMPUATTR);

            if (bEnable)
            {
                attr = FLD_SET_DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1, attr);
                attr = FLD_SET_DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT, 1, attr);
            } else
            {
                attr = FLD_SET_DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 0, attr);
                attr = FLD_SET_DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT, 0, attr);
            }
            csr_write(LW_RISCV_CSR_SMPUATTR, attr);
        }
    }
    riscvLwfenceRWIO();
    riscvSfenceVMA(0, 0);
}
