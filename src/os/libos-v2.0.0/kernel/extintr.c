/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dev_falcon_v4.h"
#include "dev_falcon_v4_addendum.h"
#include "dev_gsp_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "dev_riscv_pri.h"
#include "kernel.h"
#include "libos_defines.h"
#include "libos_syscalls.h"
#include "lwtypes.h"
#include "port.h"
#include "riscv-isa.h"
#include "task.h"
#include "libos_status.h"
#include "extintr.h"

/**
 *
 * @file extintr.c
 *
 * @brief Management of the external interrupt
 */

/**
 * @brief Programs the external interrupt registers
 *
 * @param void
 *
 */
LIBOS_SECTION_TEXT_COLD void kernel_external_interrupt_load(void)
{
    // Configure Peregrine core interrupt registers to send MEMERR to RISCV.

    // Clear the error source registers
    riscv_write(LW_PRISCV_RISCV_PRIV_ERR_STAT, REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID, _FALSE));
    riscv_write(LW_PRISCV_RISCV_HUB_ERR_STAT, REF_DEF(LW_PRISCV_RISCV_HUB_ERR_STAT_VALID, _FALSE));

    // Clear any pending MEMERR interrupt, as it's edge triggerred
    falcon_write(LW_PFALCON_FALCON_IRQSCLR, REF_DEF(LW_PFALCON_FALCON_IRQSCLR_MEMERR, _SET));

    // Allow MEMERR for EXTINTR.
    riscv_write(LW_PRISCV_RISCV_IRQMSET, REF_DEF(LW_PRISCV_RISCV_IRQMSET_MEMERR, _SET));

    // Enable the external interrupt
    csr_set(LW_RISCV_CSR_XIE, REF_NUM64(LW_RISCV_CSR_XIE_XEIE, 1));

    fence_rw_rw();
}

static LwU32 LIBOS_SECTION_DMEM_PINNED(interruptWaitingMask) = 0;

LIBOS_SECTION_IMEM_PINNED void kernel_syscall_interrupt_arm_external(LwU32 interruptWaitingMask_)
{
    // Enable the interrupts we'd like to wait on
    riscv_write(LW_PRISCV_RISCV_IRQMSET, interruptWaitingMask_);

    // Save the mask for later calls (we want to disable this precise list on EINTR)
    interruptWaitingMask = interruptWaitingMask_;
}

LIBOS_SECTION_IMEM_PINNED void kernel_shuttle_retire_single(libos_shuttle_t *shuttle);

LIBOS_SECTION_TEXT_COLD void kernel_external_interrupt_report(void)
{
    extern libos_port_t port_extintr_instance;

    // Turn off external interupt
    riscv_write(LW_PRISCV_RISCV_IRQMCLR, interruptWaitingMask);

    // Deque the waiter
    libos_shuttle_t *waiter = LIBOS_PTR32_EXPAND(libos_shuttle_t, port_extintr_instance.waiting_future);
    port_extintr_instance.waiting_future  = 0;
    
    // No waiter? Disable interrupts.  When they re-arm interrupts they'll pick it up.
    if (!waiter)
        return;

    // Retire the waiting shuttle
    waiter->error_code = LIBOS_OK;
    waiter->payload_size = 0;
    kernel_shuttle_retire_single(waiter);
}


/**
 * @brief Handle interrupts outside of hot-path
 */
LIBOS_SECTION_TEXT_COLD /* LIBOS_NORETURN */ void kernel_external_interrupt_memerr()
{
    // Hub MEMERR debugging is awful compared to PRIV.
    LwU32 intr = riscv_read(LW_PRISCV_RISCV_HUB_ERR_STAT);
    if (intr & REF_DEF(LW_PRISCV_RISCV_HUB_ERR_STAT_VALID, _TRUE))
    {
        self->state.xbadaddr = 0;           

        // Clear the interrupt source
        riscv_write(LW_PRISCV_RISCV_HUB_ERR_STAT, REF_DEF(LW_PRISCV_RISCV_HUB_ERR_STAT_VALID, _FALSE));
    }

    // It is possible both can happen, but that's usually
    // artificially induced through debugging.
    // As PRIV error gives more useful info, we will overwrite HUB instead.
    self->state.lwriscv.priv_err_info = 0;
    intr = riscv_read(LW_PRISCV_RISCV_PRIV_ERR_STAT);
#if LWRISCV_VERSION >= LWRISCV_2_2
    // With LWRISCV 2.2+, LACC faults caused by IO errors also log info
    // in RISCV_PRIV_ERR_*. Only consume the error context here if it's
    // associated with a write, to prevent mixing two errors' info together.
    if ((intr & REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID, _TRUE)) &&
        (intr & REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_WRITE, _TRUE)))
#else
    if (intr & REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID, _TRUE))
#endif
    {
        self->state.xbadaddr = riscv_read(LW_PRISCV_RISCV_PRIV_ERR_ADDR);
#if LWRISCV_VERSION >= LWRISCV_2_0
        self->state.xbadaddr |= ((LwU64)riscv_read(LW_PRISCV_RISCV_PRIV_ERR_ADDR_HI) << 32);
#endif

        self->state.lwriscv.priv_err_info = riscv_read(LW_PRISCV_RISCV_PRIV_ERR_INFO);

        // Clear the interrupt source
        riscv_write(LW_PRISCV_RISCV_PRIV_ERR_STAT, REF_DEF(LW_PRISCV_RISCV_PRIV_ERR_STAT_VALID, _FALSE));
    }

    // Clear any pending MEMERR interrupt, as it's edge triggerred
    falcon_write(LW_PFALCON_FALCON_IRQSCLR, REF_DEF(LW_PFALCON_FALCON_IRQSCLR_MEMERR, _SET));

    // Panic the task
    // @todo: Uncomment to enable printing.  The exception is imprecise so this may panic
    //        the wrong task.  The long term solution is to notify the init task by an asynchronous port
    //        to print the address and suspected task origin.
    // kernel_task_panic();
}

/**
 * @brief Handle external interrupt
 */
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void kernel_external_interrupt(void)
{
    LwU32 intr = falcon_read(LW_PFALCON_FALCON_IRQSTAT);

    // Handle user-mode interrupts
    if (intr & interruptWaitingMask)
        kernel_external_interrupt_report();

    // Handle MEMERR
    if (intr & REF_DEF(LW_PFALCON_FALCON_IRQSTAT_MEMERR, _TRUE))
        kernel_external_interrupt_memerr();

    kernel_return();
}
