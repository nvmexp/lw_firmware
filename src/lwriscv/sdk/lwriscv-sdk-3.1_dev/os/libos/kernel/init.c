/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwmisc.h"
#include "lwtypes.h"
#include "libelf.h"
#include "extintr.h"
#include "kernel.h"
#include "profiler.h"
#include "libriscv.h"
#include "task.h"
#include "timer.h"
#include "scheduler.h"
#include "trap.h"
#include "mm.h"
#include "diagnostics.h"
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
#include "gdma.h"
#endif

/*!
 * @file init.c
 * @brief This module is responsible for configuring and booting the kernel.
 *   Control comes from the bootstrap (@see kernel_bootstap)
 *
 *   This is the first code that exelwtes with the MPU enabled.
 */

__attribute__((used,aligned(32))) LwU8 LIBOS_SECTION_DMEM_PINNED(KernelStack)[LIBOS_CONFIG_KERNEL_STACK_SIZE] = {0};

/*!
 * @brief This is the main kernel entrypoint.
 *
 * \param PHDRBootVirtualToPhysicalDelta
 *   This is equivalent to kernel_va - kernel_pa.
 *   It is used by the paging subsystem to program later MPU entries.
 *
 * \return Does not return.
 */
#if LIBOS_FEATURE_PAGING
LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelInit
(
    // Params passed from SK or other partition
    LwU64 param1,
    LwU64 param2,
    LwU64 param3,
    LwU64 param4,
    LwU64 param5,

    LwS64 PHDRBootVirtualToPhysicalDelta
)  
#else
LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelInit
(
    // Params passed from SK or other partition
    LwU64 param1,
    LwU64 param2,
    LwU64 param3,
    LwU64 param4,
    LwU64 param5
)  
#endif
{
    static LwBool LIBOS_SECTION_DMEM_COLD(firstBoot) = LW_TRUE;

    KerneMpuLoad();

    KernelPrintf("LIBOS Kernel started.\n");

#if LIBOS_FEATURE_PAGING
    // Copy resident kernel into IMEM/DMEM
    KernelPagingLoad(PHDRBootVirtualToPhysicalDelta);
#endif

    // Configure user-mode access to cycle and timer counters
    KernelCsrSet(
        LW_RISCV_CSR_XCOUNTEREN,
        REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_TM, 1u) | REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_CY, 1u));

    if (firstBoot)
    {
        firstBoot = LW_FALSE;
        
#if LIBOS_CONFIG_PROFILER
        ProfilerInit();
#endif

        KernelTaskInit();

        KernelTimerInit();
    }

    // Set the initial arguments from SK/bootloader or initial partition switch
    // arguments from another partition as arguments for the init task.
    // Or, set the subsequent partition switch arguments from another partition
    // as return values for the resumeTask.
    resumeTask->state.registers[RISCV_A0] = param1;
    resumeTask->state.registers[RISCV_A1] = param2;
    resumeTask->state.registers[RISCV_A2] = param3;
    resumeTask->state.registers[RISCV_A3] = param4;
    resumeTask->state.registers[RISCV_A4] = param5;

#if LIBOS_FEATURE_PAGING
    KernelPagingLoad(PHDRBootVirtualToPhysicalDelta);
#endif

    // Load trap handler.  Enabling interrupts must be done after trap handler.
    KernelTrapLoad();

    KernelTimerLoad();

#if LIBOS_FEATURE_PARTITION_SWITCH
    KernelSchedulerLoad(resumeTask);
#else
    KernelSchedulerLoad(&TaskInit);
#endif
    

#if LIBOS_CONFIG_EXTERNAL_INTERRUPT
    KernelExternalInterruptLoad();
#endif

#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    KernelGdmaLoad();
#endif

    KernelCsrWrite(LW_RISCV_CSR_XSTATUS, userXStatus);

    KernelTaskReturn();
}
