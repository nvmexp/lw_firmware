/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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
#include <liblwriscv/shutdown.h>

#include <lwriscv/csr.h>
#include <lwriscv/fence.h>

#include <pmu/pmuifcmn.h>

#include <riscv_sbi.h>

/*
 * Code in this file is single-use springboard, its sole purpose is to jump to bootloader.
 * After that happens, the bootloader is expected to overwrite this code with actual
 * RTOS partition code.
 */

typedef void (*BootloaderEntryPointFn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static LW_FORCEINLINE void s_initEngine(void)
{
    // On startup, PMU needs to ensure LOCKPMB is set correctly to allow reading from DMEM (e.g. for prints)
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB,
               DRF_DEF(_PRGNLCL, _FALCON_LOCKPMB, _IMEM, _LOCK) |
               DRF_DEF(_PRGNLCL, _FALCON_LOCKPMB, _DMEM, _UNLOCK));

    // Enable nack-as-ack in FBIF
    localWrite(LW_PRGNLCL_FBIF_CTL2, DRF_DEF(_PRGNLCL, _FBIF_CTL2, _NACK_MODE, _NACK_AS_ACK));

    // Engage SCP lockdown
    localWrite(LW_PRGNLCL_SCP_CTL_CFG,
               FLD_SET_DRF(_PRGNLCL_SCP, _CTL_CFG, _LOCKDOWN_SCP, _ENABLE, localRead(LW_PRGNLCL_SCP_CTL_CFG)));
}

static LW_FORCEINLINE void s_fbifCfgSetupFb(uint32_t wprId)
{
    // Set up default values for TRANSCFG and REGIONCFG with WPR in FB

    sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_TRANSCFG_SET, RM_PMU_DMAIDX_UCODE,
             (DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));

    sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_REGIONCFG_SET, RM_PMU_DMAIDX_UCODE, wprId);
}

static LW_FORCEINLINE void s_fbifCfgSetupSysmem(uint32_t wprId)
{
    // Set up default values for TRANSCFG and REGIONCFG with WPR in sysmem

    sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_TRANSCFG_SET, RM_PMU_DMAIDX_PHYS_SYS_COH,
             (DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
              DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM)));

    sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_REGIONCFG_SET, RM_PMU_DMAIDX_PHYS_SYS_COH, wprId);
}

static LW_FORCEINLINE void s_selwrityInitCorePrivilege(void)
{
    uint64_t data64;

#if USE_PL0_CONFIG
    // Only allow L0 for U-mode
    data64 = csr_read(LW_RISCV_CSR_SSPM);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SSPM, _UPLM, _LEVEL0, data64);
    csr_write(LW_RISCV_CSR_SSPM, data64);

    // Set S-mode priv level to L0
    data64 = csr_read(LW_RISCV_CSR_SRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL0, data64);
    csr_write(LW_RISCV_CSR_SRSP, data64);

    // Set U-mode priv level to L0 as a default.
    data64 = csr_read(LW_RISCV_CSR_RSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _RSP, _URPL, _LEVEL0, data64);
    csr_write(LW_RISCV_CSR_RSP, data64);
#else // USE_PL0_CONFIG
    // Allow L2|L0 for U-mode
    data64 = csr_read(LW_RISCV_CSR_SSPM);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SSPM, _UPLM, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_SSPM, data64);

    // Set S-mode priv level to L2
    data64 = csr_read(LW_RISCV_CSR_SRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_SRSP, data64);

    //
    // Set U-mode priv level to L0 as a default. In reality, we have
    // per-task RSP.URPL configs, so tasks will set up their own values
    // as needed.
    //
    data64 = csr_read(LW_RISCV_CSR_RSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _RSP, _URPL, _LEVEL0, data64);
    csr_write(LW_RISCV_CSR_RSP, data64);
#endif // USE_PL0_CONFIG

    //
    // Set SRSEC to SEC as a default, RTOS code is free to lower it later.
    // Having it SEC by default is helpful for doing e.g. scpStartRand
    // with lockdown enabled (although enabling lockdown itself
    // doesn't require SEC).
    // Make sure URSEC is set to INSEC, it should always be INSEC.
    //
    data64 = csr_read(LW_RISCV_CSR_SRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRSEC, _SEC, data64);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _URSEC, _INSEC, data64);
    csr_write(LW_RISCV_CSR_SRSP, data64);
}

// Boolean for sanity check - this can only be started once!
static bool s_partition_0_started = false;

extern uint64_t __fmc_dmem_size[];
extern uint64_t __fmc_imem_size[];

__attribute__((noreturn))
__attribute__((__optimize__("no-stack-protector")))
void partition_rtos_init(uint64_t sbiVersion, uint64_t misa, uint64_t marchId, uint64_t mimpd, uint64_t mvendorid, uint64_t mfetchattr, uint64_t mldstattr, uint64_t zero)
{
    if (s_partition_0_started)
    {
        riscvPanic();
    }
    s_partition_0_started = true;

    s_selwrityInitCorePrivilege();

    s_initEngine();

    {
        BootloaderEntryPointFn bldFn;
        const uint64_t fmcSize = (uint64_t)&__fmc_imem_size;
        uintptr_t partitionOffset;
        uint32_t wprId = DRF_VAL(_PRGNLCL_RISCV, _BCR_DMACFG_SEC, _WPRID, localRead(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC));

        partitionOffset  = ((uint64_t)localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_HI)) << 32;
        partitionOffset |= localRead(LW_PRGNLCL_RISCV_BCR_DMAADDR_FMCCODE_LO);
        partitionOffset <<= 8;
        partitionOffset += fmcSize;
        partitionOffset  = LW_ALIGN_UP64(partitionOffset, 0x1000U);

        switch (DRF_VAL64(_PRGNLCL_RISCV, _BCR_DMACFG, _TARGET,
                          localRead(LW_PRGNLCL_RISCV_BCR_DMACFG)))
        {
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_LOCAL_FB:
                s_fbifCfgSetupFb(wprId);
                partitionOffset += LW_RISCV_AMAP_FBGPA_START;
                break;
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_COHERENT_SYSMEM:    // fall-through
            case LW_PRGNLCL_RISCV_BCR_DMACFG_TARGET_NONCOHERENT_SYSMEM:
                s_fbifCfgSetupSysmem(wprId);
                partitionOffset += LW_RISCV_AMAP_SYSGPA_START;
            break;
            default:
                riscvShutdown();
                break;
        }

        riscvLwfenceRWIO();

        // Jump to bootloader
        bldFn = (BootloaderEntryPointFn)partitionOffset;
        bldFn(sbiVersion, misa, marchId, mimpd, mvendorid, mfetchattr, mldstattr, zero);
    }

    riscvShutdown();
}
