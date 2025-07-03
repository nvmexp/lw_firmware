/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "lwmisc.h"
#include "lwtypes.h"

#include "kernel.h"
#include "libos.h"
#include "lwtypes.h"
#include "sched/port.h"
#include "libriscv.h"
#include "task.h"
#include "libos.h"
#include "extintr.h"
#include "sched/port.h"
#include "scheduler.h"
#include "mm/objectpool.h"
#include "sched/global_port.h"

/*
 *  Include adapter header to present LW_PRGNLCL interface on LWRISCV 1.x GSP
 */
#if LIBOS_LWRISCV < 200
#   include "prgnlcl.h"
#endif

/**
 *
 * @file extintr.c
 *
 * @brief Management of the external interrupt
 */

Port * externalInterruptPort = 0;

/**
 * @brief Programs the external interrupt registers
 *
 * @param void
 *
 */
LIBOS_SECTION_TEXT_COLD void KernelExternalInterruptLoad(void)
{
    // Configure Peregrine core interrupt registers to send MEMERR to RISCV.

#if LIBOS_CONFIG_INTEGRITY_MEMERR
    // Clear the error source registers
    KernelMmioWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, REF_DEF(LW_PRGNLCL_RISCV_PRIV_ERR_STAT_VALID, _FALSE));
    KernelMmioWrite(LW_PRGNLCL_RISCV_HUB_ERR_STAT, REF_DEF(LW_PRGNLCL_RISCV_HUB_ERR_STAT_VALID, _FALSE));

    // Clear any pending MEMERR interrupt, as it's edge triggerred
    KernelMmioWrite(LW_PRGNLCL_FALCON_IRQSCLR, REF_DEF(LW_PRGNLCL_FALCON_IRQSCLR_MEMERR, _SET));

    // Allow MEMERR for EXTINTR.
    KernelMmioWrite(LW_PRGNLCL_RISCV_IRQMSET, REF_DEF(LW_PRGNLCL_RISCV_IRQMSET_MEMERR, _SET));
#endif

    // Enable the external interrupt
    KernelCsrSet(LW_RISCV_CSR_XIE, REF_NUM64(LW_RISCV_CSR_XIE_XEIE, 1));

    /*  
        This fence serves two functions:
            (a) Ensures all previously issued stores have completed
                to ensure memerr containment.
            (b) Ensures that the CSR writes have completed before returning
    */
    KernelFenceFull();
}

static LwU32 LIBOS_SECTION_DMEM_PINNED(interruptWaitingMask) = 0;

LIBOS_SECTION_IMEM_PINNED LibosStatus KernelSyscallExternalInterruptBind(LibosPortHandle handle)
{
    Task * self = KernelSchedulerGetTask();

#if LIBOS_TINY
    if (externalInterruptPort || self != &TaskInit)
#else
    if (externalInterruptPort || self != TaskInit)
#endif
        return LibosErrorAccess;

    OWNED_PTR Port * interruptPort = KernelObjectResolve(KernelSchedulerGetTask(), handle, Port, LibosGrantPortSend);

    if (!interruptPort)
        return LibosErrorAccess;

    if (externalInterruptPort)
        KernelPoolRelease(externalInterruptPort);

    // Transfer ownership
    externalInterruptPort = interruptPort;

    return LibosOk;
}

LIBOS_SECTION_IMEM_PINNED void KernelSyscallExternalInterruptArm(LwU32 interruptWaitingMask_)
{
    // Enable the interrupts we'd like to wait on
    KernelMmioWrite(LW_PRGNLCL_RISCV_IRQMSET, interruptWaitingMask_);

    // Save the mask for later calls (we want to disable this precise list on EINTR)
    interruptWaitingMask = interruptWaitingMask_;
}

LIBOS_SECTION_TEXT_COLD void KernelExternalInterruptReport(void)
{
    // Turn off external interupt
    KernelMmioWrite(LW_PRGNLCL_RISCV_IRQMCLR, interruptWaitingMask);

    // No bound port?
    if (!externalInterruptPort)
        return;

    // Deque the waiter
    if (PortHasReceiver(externalInterruptPort))
    {
        Shuttle *waiter = PortDequeReceiver(externalInterruptPort);
    
        // Retire the waiting shuttle
        waiter->errorCode = LibosOk;
        waiter->payloadSize = 0;
        ShuttleRetireSingle(waiter);
    }
}


/**
 * @brief Handle interrupts outside of hot-path
 */
#if LIBOS_CONFIG_INTEGRITY_MEMERR
LIBOS_SECTION_TEXT_COLD LIBOS_NORETURN void KernelExternalInterruptMemoryError()
{
    // Hub MEMERR debugging is awful compared to PRIV.
    LwU32 intr = KernelMmioRead(LW_PRGNLCL_RISCV_HUB_ERR_STAT);
    if (intr & REF_DEF(LW_PRGNLCL_RISCV_HUB_ERR_STAT_VALID, _TRUE))
    {
        KernelSchedulerGetTask()->state.xbadaddr = 0;           

        // Clear the interrupt source
        KernelMmioWrite(LW_PRGNLCL_RISCV_HUB_ERR_STAT, REF_DEF(LW_PRGNLCL_RISCV_HUB_ERR_STAT_VALID, _FALSE));
    }

    // It is possible both can happen, but that's usually
    // artificially induced through debugging.
    // As PRIV error gives more useful info, we will overwrite HUB instead.
    intr = KernelMmioRead(LW_PRGNLCL_RISCV_PRIV_ERR_STAT);
    if (intr & REF_DEF(LW_PRGNLCL_RISCV_PRIV_ERR_STAT_VALID, _TRUE))
    {
        KernelSchedulerGetTask()->state.xbadaddr = ((LwU64)KernelMmioRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO) << 32) |
                                KernelMmioRead(LW_PRGNLCL_RISCV_PRIV_ERR_ADDR);

        // Clear the interrupt source
        KernelMmioWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, REF_DEF(LW_PRGNLCL_RISCV_PRIV_ERR_STAT_VALID, _FALSE));
    }

    // Clear any pending MEMERR interrupt, as it's edge triggerred
    KernelMmioWrite(LW_PRGNLCL_FALCON_IRQSCLR, REF_DEF(LW_PRGNLCL_FALCON_IRQSCLR_MEMERR, _SET));

    // Panic the task
    // @todo: Uncomment to enable printing.  The exception is imprecise so this may panic
    //        the wrong task.  The long term solution is to notify the init task by an asynchronous port
    //        to print the address and suspected task origin.
    KernelTaskPanic();
}
#endif


/**
 * @brief Handle external interrupt
 */
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelExternalInterrupt(void)
{
    LwU32 intr = KernelMmioRead(LW_PRGNLCL_FALCON_IRQSTAT);

    // Handle user-mode interrupts
    if (intr & interruptWaitingMask)
        KernelExternalInterruptReport();

#if LIBOS_CONFIG_INTEGRITY_MEMERR
    // Handle MEMERR
    if (intr & REF_DEF(LW_PRGNLCL_FALCON_IRQSTAT_MEMERR, _TRUE))
        KernelExternalInterruptMemoryError();
#endif
    KernelTaskReturn();
}