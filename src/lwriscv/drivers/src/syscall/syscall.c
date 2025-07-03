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
 * @file    syscall.c
 * @brief   Kernel-User interface
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
// MK TODO: this is required to link with SafeRTOS core (that we must link with).
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>
#include <portfeatures.h>
#include <task.h>

#include <lwrtos.h>
#include <lwrtosHooks.h>

#include <lwriscv-mpu.h>
#include <shlib/syscall.h>
#if LWRISCV_PARTITION_SWITCH
#include <shlib/partswitch.h>
#endif // LWRISCV_PARTITION_SWITCH
/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include "drivers/drivers.h"
#include "drivers/mm.h"
#include "drivers/mpu.h"
#include "drivers/vm.h"
#include "drivers/odp.h"

//
// Handle syscall, called from asm, assumptions:
// - ret is passed as return value for task (a0) - it must match syscall
//   function signature (proper type cast to LwU64).
// - MUST accept 8 arguments in that order (we have saved a0-a7, a7 is syscall number)
// - Current context is in gp (it is also in pxLwrrentTask)
//
sysKERNEL_CODE void
lwrtosHookSyscall
(
    LwU64 arg0,
    LwU64 arg1,
    LwU64 arg2,
    LwU64 arg3,
    LwU64 arg4,
    LwU64 arg5,
    LwU64 arg6,
    LwU64 num
)
{
    //
    // We're called from assembly, current context is saved in GP;
    // It may be either task context (then gp == pxLwrrentTCB) or kernel
    // context that is stored on (kernel) stack
    //
    register xPortTaskContext *pCtx __asm__("gp");
    LwBool   bInKernelCtx = (xPortGetExceptionNesting() > 1);
    LwU64    ret = 0;

    //
    // MK TODO: Rethink and enable security model for syscalls
    // It is pretty bad and hard to do, as ecall is perfect ROP gadget.
    //

    // For kernel context, we do not permit any non-optimized syscalls
    if (bInKernelCtx)
    {
        ret = FLCN_ERR_ILWALID_STATE;
        if (num >= LW_SYSCALL_APPLICATION_START)
        {
            dbgPrintf(LEVEL_ALWAYS,
                        "App syscall %lld not permitted from kernel context.\n",
                        num - LW_SYSCALL_APPLICATION_START);
        }
        else
        {
            dbgPrintf(LEVEL_ALWAYS, "Non-fast-path RTOS syscall %lld not permitted from kernel context.\n",
                        num);
        }

        kernelVerboseCrash(0);
        goto out;
    }

#if defined(INSTRUMENTATION) || defined(USTREAMER)
    // Handle instrumentation if enabled
    lwrtosHookInstrumentationBegin();
#endif // INSTRUMENTATION || USTREAMER

    switch (num)
    {
    case LW_SYSCALL_YIELD:
#ifdef portYIELD_IMMEDIATE
        portYIELD_IMMEDIATE();
#else
        // Fallback branch for v5
        vTaskSelectNextTask();
#endif
        break;
    case LW_SYSCALL_TASK_EXIT:
        vPortErrorHook(pxLwrrentTCB, errILWALID_TASK_CODE_POINTER);
        break;
    case LW_SYSCALL_INTR_DISABLE:
    case LW_SYSCALL_INTR_ENABLE:
    case LW_SYSCALL_SET_KERNEL:
    case LW_SYSCALL_SET_USER:
    case LW_SYSCALL_SET_KERNEL_INTR_DISABLED:
    case LW_SYSCALL_VIRT_TO_PHYS:
    case LW_SYSCALL_DMA_TRANSFER:
        dbgPrintf(LEVEL_CRIT, "Illegal syscall 0x%llx, syscall should have been optimized!\n", num);
        kernelVerboseCrash(0);
        break;
    case LW_APP_SYSCALL_PUTS:
        dbgPuts(LEVEL_ALWAYS, (const char *)arg0);
        break;
    case LW_APP_SYSCALL_FBHUB_GATED:
        // Set this flag before disabling MPU mappings, so that any prints in
        // mpuNotifyFbhubGated will not attempt to access dmesg buffer in FB.
        fbhubGatedSet(LW_TRUE);

#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
        mpuNotifyFbhubGated(LW_TRUE);
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)
        break;
    case LW_APP_SYSCALL_FBHUB_UNGATED:
#if defined(LWRISCV_MPU_FBHUB_SUSPEND)
        mpuNotifyFbhubGated(LW_FALSE);
#endif // defined(LWRISCV_MPU_FBHUB_SUSPEND)

        // Set this flag AFTER re-enabling mappings, so that any prints in
        // mpuNotifyFbhubUngated will not attempt to access dmesg buffer in FB.
        fbhubGatedSet(LW_FALSE);
        break;
#if LWRISCV_PRINT_RAW_MODE
    case LW_APP_SYSCALL_DBG_DISPATCH_RAW_DATA:
        dbgDispatchRawDataKernel((LwU32)arg0, (const LwUPtr*)arg1);
        break;
#endif // LWRISCV_PRINT_RAW_MODE
#ifdef ODP_ENABLED
    case LW_APP_SYSCALL_ODP_PIN_SECTION:
        ret = odpPinSection(arg0);
        break;
    case LW_APP_SYSCALL_ODP_UNPIN_SECTION:
        ret = odpUnpinSection(arg0);
        break;
#endif // ODP_ENABLED
#ifndef PMU_RTOS
    case LW_APP_SYSCALL_CMDQ_REGISTER_NOTIFIER:
        ret = cmdqRegisterNotifier(arg0);
        break;
    case LW_APP_SYSCALL_CMDQ_UNREGISTER_NOTIFIER:
        ret = cmdqUnregisterNotifier(arg0);
        break;
    case LW_APP_SYSCALL_CMDQ_ENABLE_NOTIFIER:
        ret = cmdqEnableNotifier(arg0);
        break;
    case LW_APP_SYSCALL_CMDQ_DISABLE_NOTIFIER:
        ret = cmdqDisableNotifier(arg0);
        break;
#endif // !PMU_RTOS
    case LW_APP_SYSCALL_FIRE_SWGEN:
        irqFireSwGen(arg0);
        break;
#ifdef GSP_RTOS
#if SCHEDULER_ENABLED
    case LW_APP_SYSCALL_NOTIFY_REGISTER_NOTIFIER:
        ret = notifyRegisterTaskNotifier(pxLwrrentTCB, arg0,
                                         (LwrtosQueueHandle) arg1,
                                         (void*) arg2, arg3);
        break;
    case LW_APP_SYSCALL_NOTIFY_UNREGISTER_NOTIFIER:
        ret = notifyUnregisterTaskNotifier(pxLwrrentTCB);
        break;
    case LW_APP_SYSCALL_NOTIFY_PROCESSING_DONE:
        ret = notifyProcessingDone(pxLwrrentTCB);
        break;
    case LW_APP_SYSCALL_NOTIFY_REQUEST_GROUP:
        ret = notifyRequestGroup(pxLwrrentTCB, arg0);
        break;
    case LW_APP_SYSCALL_NOTIFY_RELEASE_GROUP:
        ret = notifyReleaseGroup(pxLwrrentTCB, arg0);
        break;
    case LW_APP_SYSCALL_NOTIFY_QUERY_GROUPS:
        ret = notifyQueryGroups(pxLwrrentTCB);
        break;
    case LW_APP_SYSCALL_NOTIFY_QUERY_IRQ:
        ret = notifyQueryIrqs(pxLwrrentTCB, arg0);
        break;
    case LW_APP_SYSCALL_NOTIFY_ENABLE_IRQ:
        ret = notifyEnableIrq(pxLwrrentTCB, arg0, arg1);
        break;
    case LW_APP_SYSCALL_NOTIFY_DISABLE_IRQ:
        ret = notifyDisableIrq(pxLwrrentTCB, arg0, arg1);
        break;
    case LW_APP_SYSCALL_NOTIFY_ACK_IRQ:
        ret = notifyAckIrq(pxLwrrentTCB, arg0, arg1);
        break;
#endif // SCHEDULER_ENABLED
#endif // GSP_RTOS
    case LW_APP_SYSCALL_SHUTDOWN:
        appShutdown(); // should never return
        break;
    case LW_APP_SYSCALL_OOPS:
        kernelVerboseOops(pxLwrrentTCB);
        break;

#ifdef PMU_RTOS
    case LW_APP_SYSCALL_SET_TIMESTAMP:
        ret = lwrtosHookSyscallSetSelwreKernelTimestamp(arg0);
        break;

    case LW_APP_SYSCALL_SET_VBIOS_MCLK:
        ret = lwrtosHookSyscallSetSelwreKernelVbiosMclk(arg0);
        break;

    case LW_APP_SYSCALL_GET_VBIOS_MCLK:
        ret = lwrtosHookSyscallGetSelwreKernelVbiosMclk();
        break;
#endif // PMU_RTOS

#if LWRISCV_PARTITION_SWITCH
    case LW_APP_SYSCALL_PARTITION_SWITCH:
        ret = partitionSwitch(arg5, arg0, arg1, arg2, arg3, arg4);
        break;
#endif

    case LW_APP_SYSCALL_WFI:
        //
        // From the RISCV spec (https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf):
        // The WFI instruction can also be exelwted when interrupts are disabled. The operation of WFI must
        // be unaffected by the global interrupt bits in mstatus(MIE/SIE/UIE) and the delegation registers[m|s|u]ideleg
        // (i.e., the hart must resume if a locally enabled interrupt becomes pending, even if
        // it has been delegated to a less-privileged mode), but should honor the individual interrupt enables
        // (e.g, MTIE) (i.e., implementations should avoid resuming the hart if the interrupt is pending but
        // not individually enabled).  WFI is also required to resume exelwtion for locally enabled interrupts
        // pending at any privilege level, regardless of the global interrupt enable at each privilege level.
        //
        // This allows for using wfi in s-mode, as here, in a syscall handler. The interrupt-enable bits which
        // it respects (XIE.XTIE/XSIE/XEIE) are the ones which we use to enter critical sections, and the
        // interrupt-enables which it ignores (xstatus.xie) are what we use to disable interrupts in the
        // syscall handler. Therefore, this is a correct usage of wfi.
        //
        __asm__ volatile("wfi;");
        break;

    default:
        dbgPrintf(LEVEL_ERROR, "Unknown syscall from task %p: %llx\n",
                  pxLwrrentTCB, num);
        // Cause doesn't matter - it's passed as stop reason to core dump.
        kernelVerboseCrash(0);
        break;
    }

out:
    // Save returned value if we context switch
    pCtx->uxGpr[LWRISCV_GPR_A0] = ret;
    pCtx->uxPc += 4;
}
