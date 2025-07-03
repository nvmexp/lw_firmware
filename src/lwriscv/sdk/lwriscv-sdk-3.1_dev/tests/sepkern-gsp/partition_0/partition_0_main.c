/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <lwmisc.h>

#include <liblwriscv/libc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>

#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/fence.h>
#include <lwriscv/manifest.h>

/*
 * Code in this file is single-use trampoline whose sole purpose is to jump to bootloader.
 * After that happens, bootloader is expected to overwrite this code with actual
 * RTOS partition switch code.
 *
 * This code is mostly borrowed from C SK, as it has to handle some things
 * that MP-SK is not doing.
 */

static void
_selwrityInitCorePrivilege(void)
{
    // TODO: Add check against BR programmed value
    // hardcode to level 2 for now
    uint64_t data64;

    data64 = csr_read(LW_RISCV_CSR_SSPM);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SSPM, _UPLM, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_SSPM, data64);

    data64 = csr_read(LW_RISCV_CSR_SRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_SRSP, data64);

    data64 = csr_read(LW_RISCV_CSR_RSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _RSP, _URPL, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_RSP, data64);
}

// if set, no reinit needed
static bool partition_0_started = false;

extern uint64_t __fmc_dmem_size[];
extern uint64_t __fmc_imem_size[];

void partition_0_init(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)  __attribute__ ((noreturn));
void partition_0_init(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)
{
    uint64_t sbiVersion = arg0;
    uint64_t misa = arg1;
    uint64_t marchId = arg2;
    uint64_t mimpd = arg3;
    uint64_t mvendorid = arg4;
    uint64_t mfetchattr = arg5;
    uint64_t mldstattr = arg6;
    uint64_t zero = arg7;
    uint32_t dmemSize = 0;

    if (partition_0_started)
    {
        printf("We should never enter partition 0 init trampoline twice. Entering ICD. \n");
        while(1)
            localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
    }
    partition_0_started = true;


    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    // Configure print to the start of DMEM, queue 7, irq swgen1
    // This is in case code dies before bootloader taking over (bootloader will
    // wipe the buffer).
    printInit((uint8_t*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x1000, 0x1000, 7, 1);

    printf("Hello from Partition 0 trampoline!\n");
    printf("Printing initial arguments set by SK: \n");
    
    printf("a0 - SBI Version: 0x%llX \n",  sbiVersion); 
    printf("a1 - misa: 0x%llX \n",         misa);
    printf("a2 - marchid: 0x%llX \n",      marchId);
    printf("a3 - mimpd: 0x%llX \n",        mimpd);
    printf("a4 - mvendorid: 0x%llX \n",    mvendorid);
    printf("a5 - mfetchattr: 0x%llX \n",   mfetchattr);
    printf("a6 - mldstattr: 0x%llX \n",    mldstattr);
    printf("a7 - always zero: 0x%llX; \n", zero);

    printf("And now switch to bootloader.\n");
    {
        void (*bld)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
        uint64_t fmcSize       = 0;
        uintptr_t partitionOffset;

        // Total size of FMC = size of IMEM + size of DMEM + Manifest size + Signature
        fmcSize = (uint64_t)&__fmc_dmem_size + (uint64_t)&__fmc_imem_size +
                MANIFEST_SIZE + RSA_SIGNATURE_SIZE;
        // Align up to 4K
        fmcSize = LW_ALIGN_UP(fmcSize, 0x1000U);

        partitionOffset  = ((uint64_t)localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_HI)) << 32;
        partitionOffset |= localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_PKCPARAM_LO);
        partitionOffset <<= 8;
        partitionOffset += fmcSize;

        switch (DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG, _TARGET,
                          localRead(LW_PRGNLCL_RISCV_BCR_DMACFG)))
        {
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_LOCAL_FB:
                partitionOffset += LW_RISCV_AMAP_FBGPA_START;
                break;
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_COHERENT_SYSMEM:    // fall-thru
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_NONCOHERENT_SYSMEM:
                partitionOffset += LW_RISCV_AMAP_SYSGPA_START;
            break;
            default:
                // Invalid specification
                break;
        }
        printf("Configuring core privileges...\n");
        _selwrityInitCorePrivilege();

        printf("Jumping to bootloader at %p\n", partitionOffset);
        riscvLwfenceRWIO();

        // Jump to bootloader
        bld = (void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))partitionOffset;
        bld(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    }

        printf("We should never reach this in partition 0 code.\n");
        sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
        while (1)
            ;
}

void partition_0_trap_handler(void) 
{
    printf("In Partition 0 trap handler at %p, cause 0x%llx val 0x%llx\n",
           csr_read(LW_RISCV_CSR_SEPC),
           csr_read(LW_RISCV_CSR_SCAUSE),
           csr_read(LW_RISCV_CSR_STVAL));
    printf("Entering ICD\n");
    while(1)
        localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
}
