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
 * @file pmu_riscv.c
 *
 * Temporary functions which enables riscv build
 */

/* ------------------------ System includes --------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"
#include "scp_common.h"

/* ------------------------ Application Includes ---------------------------- */
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>
#include <portfeatures.h>
#include <portmacro.h>
#include <riscv_csr.h>
#include <drivers/drivers.h>
#include <sbilib/sbilib.h>
#include <shlib/syscall.h>
#include <memops.h>
#include <dev_top.h>
#include <dev_pwr_csb.h>
#include "dev_ltc.h"
#include "pmu_objpmu.h"
#include "pmu_objic.h"
#include "perf/cf/perf_cf.h"
#include "osptimer.h"
#include "lwoscmdmgmt.h"
#if PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
#include "acr/task_acr.h"
#endif // PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)

/* ------------------------ Global Variables -------------------------------- */
extern LwU64 memTuneControllerVbiosMaxMclkkHz;

#if PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
sysKERNEL_DATA LwU32 irqMaskBackup = 0;
#endif // PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)

/* ------------------------- Macros and Defines ----------------------------- */
#define LWRISCV_MTIME_TICK_FREQ_HZ  (1000000000ULL >> LWRISCV_MTIME_TICK_SHIFT)

/*!
 * Put PMU core into ICD via raising SW interrupt, then hang.
 */
__attribute__((noreturn)) static void s_pmuIcdStop(void)
{
    if (!lwrtosIS_KERNEL())
    {
        //
        // Try to go into a proper ISR handler if this is called from
        // non-kernel (non-ISR) code by some magical means.
        //
        PMU_HALT();
    }

    //
    // MMINTS-TODO: this doesn't do anything on GA10X, but makes sure the core goes into ICD
    // on configs where we haven't upgraded to the new ICD-via-swintr mechanism.
    // This will have to be removed later!
    //
    REG_WR32(CSB, LW_CPWR_RISCV_ICD_CMD, DRF_DEF(_CPWR, _RISCV_ICD_CMD, _OPC, _STOP));

    // Put the core into ICD (via software interrupt, set up in EMASK by PMU manifest)

    // Go into a safe dead-hang on intr
    csr_write(LW_RISCV_CSR_STVEC, vPortInfiniteLoop);
    //
    // Once we set XSTATUS.XIE, immediately interrupts will be checked according to
    // XIE.XEIE/.XTIE/.XSIE, so we disable all except .XSIE to avoiding a spurious
    // external/timer interrupt taking over on our effort to go into ICD.
    //
    csr_clear(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _SEIE, 0x1) |
                                DRF_NUM64(_RISCV, _CSR_SIE, _STIE, 0x1));
    // After this, explicitly make sure XSIE is enabled in XIE!
    csr_set(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _SSIE, 0x1));
    // Set the sw intr to pending
    csr_set(LW_RISCV_CSR_SIP, DRF_NUM64(_RISCV, _CSR_SIP, _SSIP, 0x1));
    // Finally, enable interrupts for current mode (s-mode)!
    csr_set(LW_RISCV_CSR_SSTATUS, DRF_NUM64(_RISCV, _CSR_SSTATUS, _SIE, 0x1));

    // Hang forever to wait for interrupt
    while (LW_TRUE)
    {
        // NOP
    }
}

/* ------------------------ Public Functions -------------------------------- */

/*
 * Original function present in GSP kernel source code. Called from safeRTOS.
 *
 * Returns value that is added to time to program systick interrupt in mtimecmp.
 */
LwU64
lwrtosHookGetTimerPeriod(void)
{
    LwU64 tick = 0;

    //
    // Verify platform type. That is mostly to fix dFPGA
    //
    switch (DRF_VAL(_PTOP, _PLATFORM, _TYPE, bar0Read(LW_PTOP_PLATFORM)))
    {
        //
        // PMU is not supported on dFPGA!
        //
        case LW_PTOP_PLATFORM_TYPE_FPGA:
        {
            tick = 0;
            break;
        }
        //
        // For Emulation and FMODEL we must decrease systick frequency to avoid
        // thrashing from overly-frequent timer interrupts.
        //
        case LW_PTOP_PLATFORM_TYPE_FMODEL:
        case LW_PTOP_PLATFORM_TYPE_EMU:
        {
            tick = LWRISCV_MTIME_TICK_FREQ_HZ * 5U / LWRTOS_TICK_RATE_HZ;
            break;
        }
        //
        // For Silicon and RTL use real timings based on what is expected in PTIMER.
        // Pre-GH100 one mtime tick is 32ns == 31250000 Hz.
        //
        case LW_PTOP_PLATFORM_TYPE_SILICON:
        case LW_PTOP_PLATFORM_TYPE_RTL:
        default:
        {
            tick = LWRISCV_MTIME_TICK_FREQ_HZ / LWRTOS_TICK_RATE_HZ;
            break;
        }
    }

    if (tick == 0U)
    {
        appHalt();
    }

    return tick;
}

FLCN_STATUS lwrtosHookSyscallSetSelwreKernelTimestamp(LwU64 timeDataId)
{
    //
    // On PMU, the only kernel-mode timestamps we want to allow setting
    // through syscalls are the secure memtune controller timestamps.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
    {
        FLCN_TIMESTAMP *pTime = NULL;

        if (timeDataId == PERF_MEM_TUNE_CONTROLLER_TIMESTAMP)
        {
            pTime = &memTuneControllerTimeStamp;
        }
        else if (timeDataId == PERF_MEM_TUNE_CONTROLLER_MONITOR_TIMESTAMP)
        {
            pTime = &memTuneControllerMonitorTimeStamp;
        }
        else
        {
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        // Don't worry about paging here, both of the allowed pTime values point to resident vars
        osPTimerTimeNsLwrrentGet(pTime);

        return FLCN_OK;
    }
    else
    {
        // This shouldn't be called if the feature is not enabled!
        return FLCN_ERR_ILLEGAL_OPERATION;
    }
}

FLCN_STATUS lwrtosHookSyscallSetSelwreKernelVbiosMclk(LwU64 mclkkHz)
{
    //
    // On PMU, the only kernel-mode mclk we want to allow setting
    // through syscalls are the secure memtune controller timestamps.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
    {
        if (memTuneControllerVbiosMaxMclkkHz == 0)
        {
            memTuneControllerVbiosMaxMclkkHz = mclkkHz;
            return FLCN_OK;
        }
        else
        {
            // This isn't allowed as vbiosmaxclk is already set
            return FLCN_ERR_ILLEGAL_OPERATION;
        }
    }
    else
    {
        // This shouldn't be called if the feature is not enabled!
        return FLCN_ERR_ILLEGAL_OPERATION;
    }
}

LwU64 lwrtosHookSyscallGetSelwreKernelVbiosMclk()
{
    //
    // On PMU, the only kernel-mode mclk we want to allow setting
    // through syscalls are the secure memtune controller timestamps.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
    {
        return memTuneControllerVbiosMaxMclkkHz; 
    }
    else
    {
        // This shouldn't be called if the feature is not enabled!
        return 0;
    }
}

/*
 * Prepare for partition switch - wait for all queues to be flushed.
 */
sysKERNEL_CODE void lwrtosHookPartitionPreSwitch(LwU64 partition)
{
#if PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
    //
    // We must make sure we emptied all PMU->RM communication buffers
    // before going into an HS partition, since it does priv lockdown!
    //

    //
    // Partition id check doesn't really do much right now (we only have 1 other partition),
    // but can be useful in the future.
    //
    if (partition == PARTITION_ID_TEGRA_ACR)
    {
        LwU32 irqMset = 0;

        // Make sure the debug buffer is emptied (internally calls appHalt on timeout)
        debugBufferWaitForEmpty();

        // Wait for debug buffer interrupt to be cleared
        if (irqWaitForSwGenClear(SYS_INTR_SWGEN1) != FLCN_OK)
        {
            PMU_HALT();
        }

        // Make sure the PMU->RM RPC buffer is emptied
        if (osCmdmgmtQueueWaitForEmpty(&OsCmdmgmtQueueDescriptorRM) != FLCN_OK)
        {
            PMU_HALT();
        }

        // Wait for PMU->RM RPC interrupt to be cleared
        if (irqWaitForSwGenClear(SYS_INTR_SWGEN0) != FLCN_OK)
        {
            PMU_HALT();
        }

        // Back up the current value of irqmask, this will be restored later.
        irqMaskBackup = REG_RD32(CSB, LW_CPWR_RISCV_IRQMASK);

        //
        // The interrupts we want enabled in the partition.
        // We rely on MEMERR and IOPMP intrs being set up with IRQDEST==RISCV.
        // HALT should be set with IRQDEST==HOST.
        //
        irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _MEMERR, _SET, irqMset);
        irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _IOPMP, _SET, irqMset);
        irqMset = FLD_SET_DRF(_CPWR, _RISCV_IRQMSET, _HALT, _SET, irqMset);

        //
        // Clear all external intrs in the mask and enable only the necessary intrs.
        // This is done so e.g. PG interrupts or RPCs don't cause random failures
        // in the ACR partition.
        //
        REG_WR32(CSB, LW_CPWR_RISCV_IRQMCLR, ~irqMset);
        REG_WR32(CSB, LW_CPWR_RISCV_IRQMSET, irqMset);
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
    (void)partition;
#endif // PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
}

/*
 * Re-initialize HW state that might have been touched by another partition.
 */
sysKERNEL_CODE void lwrtosHookPartitionReentry(LwU64 partition)
{
    extern LwU32 dmaIndex;

    lwFenceAll();

    // Reset SCP RNG state for ssp stack canary
    lwosSspRandInit();

    // Restore DMACTL
    dmaInit(dmaIndex);

    // Make sure priv lockdown is released
    riscvReleasePrivLockdown();

#if PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
    //
    // Partition id check doesn't really do much right now (we only have 1 other partition),
    // but can be useful in the future.
    //
    if (partition == PARTITION_ID_TEGRA_ACR)
    {
        // Finally, restore the backed up IRQMASK settings.
        REG_WR32(CSB, LW_CPWR_RISCV_IRQMCLR, ~irqMaskBackup);
        REG_WR32(CSB, LW_CPWR_RISCV_IRQMSET, irqMaskBackup);
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
    (void)partition;
#endif // PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
}

void
appHalt(void)
{
#ifdef IS_SSP_ENABLED_WITH_SCP
    scpRegistersClearAll();
#endif // IS_SSP_ENABLED_WITH_SCP

    // Manually trigger the _HALT interrupt to RM so RM can handle it for better debugability
    icHaltIntrEnable_HAL();

    // Disable the ICD interrupt to make sure we only send the HALT interrupt for this case
    icRiscvIcdIntrDisable_HAL();

    //
    // Trigger notification to the Memory Tuning Controller to take the necessary
    // actions on PMU exception events.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MEM_TUNE))
    {
        perfCfMemTuneControllerPmuExceptionHandler_HAL();
    }

    // Send HALT signal to RM
    REG_WR32(CSB, LW_CPWR_FALCON_IRQSSET, DRF_DEF(_CPWR, _FALCON_IRQSSET, _HALT, _SET));

    // Send core into ICD and hang
    s_pmuIcdStop();
}

void
appShutdown(void)
{
#ifdef IS_SSP_ENABLED_WITH_SCP
    scpRegistersClearAll();
#endif // IS_SSP_ENABLED_WITH_SCP

    //
    // This is a "clean" shutdown, so disable HALT interrupt
    // to prevent the shutdown() call from sending a HALT to RM
    //
    icHaltIntrDisable_HAL();

    //
    // MK TODO: this should also send "i shut down" message to RM
    // (TODO once messaging is handled in kernel)
    //

    dbgPrintf(LEVEL_ALWAYS, "Shutting down...\n");

    // Finish unload sequence
    pmuDetachUnloadSequence_HAL(&Pmu, LW_FALSE);

    sbiShutdown();
}
