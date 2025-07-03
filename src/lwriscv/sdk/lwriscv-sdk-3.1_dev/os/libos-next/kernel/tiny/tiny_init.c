/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <libos-config.h>
#include "lwmisc.h"
#include "lwtypes.h"
#include "libelf.h"
#include "extintr.h"
#include "kernel.h"
#include "profiler.h"
#include "libriscv.h"
#include "task.h"
#include "sched/timer.h"
#include "scheduler.h"
#include "trap.h"
#include "diagnostics.h"
#include "mm/pagetable.h"
#include "mm/memorypool.h"
#include "task.h"
#include "sched/global_port.h"
#include "mm/objectpool.h"
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
#include "../root/root.h"
#include "lwriscv-2.0/sbi.h"
#endif
#if LIBOS_CONFIG_GDMA_SUPPORT
#include "gdma.h"
#endif
#include "mm/address_space.h"
#include "mm/softmmu.h"
#include "server/server.h"

/*!
 * @file init.c
 * @brief This module is responsible for configuring and booting the kernel.
 *   Control comes from the bootstrap (@see kernel_bootstap)
 *
 *   This is the first code that exelwtes with the MPU enabled.
 */

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelInit(
    LwU64 initArg0, // initArg# passed from SK 
    LwU64 initArg1, 
    LwU64 initArg2, 
    LwU64 initArg3, 
    LwU64 initArg4, 
    LwU64 callingPartition,
    LwU64 reserved1,
    LwU64 reserved2)  
{
    static LwBool LIBOS_SECTION_DMEM_COLD(firstBoot) = LW_TRUE;
    
    // Load trap handler 
    KernelTrapLoad();

    // Configure user-mode access to cycle and timer counters
    KernelCsrSet(
        LW_RISCV_CSR_XCOUNTEREN,
        REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_TM, 1u) | REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_CY, 1u));


    KernelMpuLoad();

    if (firstBoot)
    {           
#if LIBOS_CONFIG_PROFILER
        ProfilerInit();
#endif

        KernelInitTask();

        KernelInitTimer();
    }

    // Set the initial arguments from SK/bootloader or initial partition switch
    // arguments from another partition as arguments for the init task.
    // Or, set the subsequent partition switch arguments from another partition
    // as return values for the resumeTask.
    extern Task * KernelInterruptedTask; // @todo
    if (firstBoot)
    {
        KernelInterruptedTask = &TaskInit;
        firstBoot = LW_FALSE;
    }
    else
    {    
        KernelInterruptedTask->state.registers[RISCV_A0] = initArg0;
        KernelInterruptedTask->state.registers[RISCV_A1] = initArg1;
        KernelInterruptedTask->state.registers[RISCV_A2] = initArg2;
        KernelInterruptedTask->state.registers[RISCV_A3] = initArg3;
        KernelInterruptedTask->state.registers[RISCV_A4] = initArg4;
    }

    KernelTimerLoad();

    KernelSchedulerLoad();

    KernelExternalInterruptLoad();

    KernelCsrWrite(LW_RISCV_CSR_XSTATUS, userXStatus);

#if LIBOS_CONFIG_GDMA_SUPPORT
    KernelGdmaLoad();
#endif

    KernelTaskReturn();    
}
