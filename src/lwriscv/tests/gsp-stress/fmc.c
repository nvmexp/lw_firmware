/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwriscv/csr.h>
#include <lwriscv/peregrine.h>
#include <lwriscv/fence.h>
#include <liblwriscv/io.h>

#include "fmc.h"

static void _partitionGetEntryPoint(LwU64 *partitionStart)
{
    LwU64 fmcSource = DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG, _TARGET,
                                localRead(LW_PRGNLCL_RISCV_BCR_DMACFG));

    LwU32 fmcLo      = 0;
    LwU32 fmcHi      = 0;

    fmcLo   = localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO);
    fmcHi   = localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI);
    LwU64 partitionOffset = ((LwU64)fmcHi) << 32 | fmcLo;
    partitionOffset      <<= 8;
    partitionOffset      += (LwU64)(&__fmc_size);

    // Align up to 4K
    partitionOffset = LW_ALIGN_UP64(partitionOffset, 0x1000);
    switch (fmcSource)
    {
        case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_LOCAL_FB:
            partitionOffset += LW_RISCV_AMAP_FBGPA_START;
            break;
#ifdef LW_RISCV_AMAP_SYSGPA_START
        case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_COHERENT_SYSMEM:
            // fall-thru
        case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_NONCOHERENT_SYSMEM:
            partitionOffset += LW_RISCV_AMAP_SYSGPA_START;
            break;
#endif // LW_RISCV_AMAP_SYSGPA_START
        default:
            // Invalid specification
        fmc_icd();
            break;
    }

    // Remap (some of - likely too much) code
    *partitionStart = partitionOffset;
}

// This code runs in *M* mode
void fmc_main(void)
{
    LwU64 partitionStart;

    csr_write(LW_RISCV_CSR_MEDELEG, 0xb0FF);
    csr_write(LW_RISCV_CSR_MIDELEG, 0x222);
    csr_write(LW_RISCV_CSR_MCOUNTEREN, 0x7ff); //enable perf counters
    csr_write(LW_RISCV_CSR_SCOUNTEREN, 0x7ff); //enable perf counters

    // Set bare-mode access attributes (cacheable = 1, wpr + coherent inherited)
    csr_write(LW_RISCV_CSR_MFETCHATTR, DRF_NUM64(_RISCV_CSR, _MFETCHATTR, _CACHEABLE, 1) |
                                       DRF_NUM64(_RISCV_CSR, _MFETCHATTR, _COHERENT, 1) |
                                       0);
    csr_write(LW_RISCV_CSR_MLDSTATTR, DRF_NUM64(_RISCV_CSR, _MLDSTATTR, _CACHEABLE, 1) |
                                      DRF_NUM64(_RISCV_CSR, _MLDSTATTR, _COHERENT, 1) |
                                      0);

    _partitionGetEntryPoint(&partitionStart);
    csr_write(LW_RISCV_CSR_MEPC, partitionStart);
    csr_write(LW_RISCV_CSR_MTVEC, (LwUPtr)fmc_trap_handler);

    csr_write(LW_RISCV_CSR_MSTATUS, FLD_SET_DRF64(_RISCV, _CSR_MSTATUS, _MPP, _SUPERVISOR, csr_read(LW_RISCV_CSR_MSTATUS)));
    riscvLwfenceRWIO();

    __asm__ volatile ("mv a0, %0\n"
                      "mret" :: "r"(partitionStart) : "a0", "a1", "a2");

    // should not be reached ever
    fmc_icd();
    while (1);
}
