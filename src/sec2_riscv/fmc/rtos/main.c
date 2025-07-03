/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdint.h>
#include <lwmisc.h>

#include <riscvifriscv.h>

#include <liblwriscv/libc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>

#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/fence.h>
#include <lwriscv/manifest_gh100.h>

#include <dev_sec_pri.h>
#include <conditionalchecks.h>

/*
 * Code in this file is single-use trampoline whose sole purpose is to jump to bootloader.
 * After that happens, bootloader is expected to overwrite this code with actual
 * RTOS partition switch code.
 *
 * This code is mostly borrowed from C SK, as it has to handle some things
 * that MP-SK is not doing.
 */

static void initEngine(void)
{
    LwU32 dmemSize = 0;

    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    // clean debug buffer (in case it's not clean)
    memset((LwU8*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x800, 0, 0x800);

    // configure print to the start of DMEM
    printInitEx((LwU8*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x800, 0x800,
                LW_PSEC_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE),
                LW_PSEC_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE), 1);

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));
}

static void _selwrityInitCorePrivilege(void)
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

void partition_rtos_init(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)  __attribute__ ((noreturn));
void partition_rtos_init(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)
{
    uint64_t sbiVersion = arg0;
    uint64_t misa = arg1;
    uint64_t marchId = arg2;
    uint64_t mimpd = arg3;
    uint64_t mvendorid = arg4;
    uint64_t mfetchattr = arg5;
    uint64_t mldstattr = arg6;
    uint64_t zero = arg7;

    if (partition_0_started)
    {
        printf("We should never enter partition 0 init trampoline twice. Entering ICD. \n");
        while(1)
            localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
    }
    partition_0_started = true;
    initEngine();

    printf("Hello from SEC2 RTOS trampoline!\n");
    printf("Printing initial arguments set by SK: \n");

    printf("a0 - SBI Version: 0x%lX \n",  sbiVersion);
    printf("a1 - misa: 0x%lX \n",         misa);
    printf("a2 - marchid: 0x%lX \n",      marchId);
    printf("a3 - mimpd: 0x%lX \n",        mimpd);
    printf("a4 - mvendorid: 0x%lX \n",    mvendorid);
    printf("a5 - mfetchattr: 0x%lX \n",   mfetchattr);
    printf("a6 - mldstattr: 0x%lX \n",    mldstattr);
    printf("a7 - always zero: 0x%lX; \n", zero);

    printf("And now switch to bootloader.\n");
    {
        void (*bld)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
        const uint64_t fmcSize       = (uint64_t)&__fmc_imem_size;
        uintptr_t partitionOffset;

        partitionOffset  = ((uint64_t)localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI)) << 32;
        partitionOffset |= localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO);
        partitionOffset <<= 8;
        CHECK_UNSIGNED_INTEGER_ADDITION_AND_GOTO_CLEANUP_IF_OUT_OF_BOUNDS(partitionOffset, fmcSize, LW_U64_MAX);
        partitionOffset += fmcSize;
        partitionOffset = LW_ALIGN_UP64(partitionOffset, 0x1000U);

        switch (DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG, _TARGET,
                          localRead(LW_PRGNLCL_RISCV_BCR_DMACFG)))
        {
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_LOCAL_FB:
                CHECK_UNSIGNED_INTEGER_ADDITION_AND_GOTO_CLEANUP_IF_OUT_OF_BOUNDS(partitionOffset, LW_RISCV_AMAP_FBGPA_START, LW_U64_MAX);
                partitionOffset += LW_RISCV_AMAP_FBGPA_START;
                break;
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_COHERENT_SYSMEM:    // fall-thru
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_NONCOHERENT_SYSMEM:
                CHECK_UNSIGNED_INTEGER_ADDITION_AND_GOTO_CLEANUP_IF_OUT_OF_BOUNDS(partitionOffset, LW_RISCV_AMAP_SYSGPA_START, LW_U64_MAX);
                partitionOffset += LW_RISCV_AMAP_SYSGPA_START;
            break;
            default:
                // Invalid specification
                break;
        }
        printf("Configuring core privileges...\n");
        _selwrityInitCorePrivilege();

        printf("Jumping to bootloader at 0x%lx\n", partitionOffset);
        riscvLwfenceRWIO();

        // Jump to bootloader
        bld = (void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))partitionOffset;
        bld(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    }

Cleanup:
        printf("We should never reach this in partition 0 code.\n");
        sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
        while (1)
            ;
}

void partition_rtos_trap_handler(void)
{
    // MK TODO: this print will never show up for now
    printf("In Partition 0 trap handler at 0x%lx, cause 0x%lx val 0x%lx\n",
           csr_read(LW_RISCV_CSR_SEPC),
           csr_read(LW_RISCV_CSR_SCAUSE),
           csr_read(LW_RISCV_CSR_STVAL));
    printf("Entering ICD\n");

    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
    while(1)
    {
    }
}
