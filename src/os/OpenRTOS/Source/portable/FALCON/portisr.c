/*
 * Copyright 2008-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */

#include <stdio.h>
#include <falcon-intrinsics.h>

#include "lwrtosHooks.h"
#include "OpenRTOS.h"
#include "OpenRTOSFalcon.h"
#include "task.h"

#ifdef SEC2_CSB_ACCESS
#include "dev_sec_csb.h"
#elif SOE_CSB_ACCESS
#include "dev_soe_csb.h"
#elif GSPLITE_CSB_ACCESS
#include "dev_gsp_csb.h"
#else
#include "dev_pwr_falcon_csb.h"
#endif

#include "falcon.h"
#include "lwmisc.h"

/*!
 *  Debug control register (DEBUGINFO) state to indicate that the falcon
 *  processor is out of the debugging mode.
 */
#define DEBUG_RESET 0

/*!
 *  Debug control register (DEBUGINFO) state to indicate an exception has
 *  being raised other than trap. Mostly raised due to a hardware breakpoint
 *  trigger.
 */
#define DEBUG_HW_EXCEPTION 1

/*!
 *  Debug control register (DEBUGINFO) state to indicate a software
 *  interrupt (SWGEN1) being raised. Generally raised during asynchronous
 *  interrupt being raised from debugger.
 */
#define DEBUG_SW_ASYNC 2

/*!
 *  Debug control register (DEBUGINFO) state to indicate that debug mode
 *  processing is completed by the debugger.
 */
#define DEBUG_COMPLETE 4

static unsigned int lwrtosTickIntrMask = 0;

#if defined(DPU_RTOS)
static unsigned int lwrtosTickIntrReg = 0;
static unsigned int lwrtosTickIntrRegClearValue = 0;
#endif

unsigned int csbBaseAddress = 0;

void EV_preinit_routine_entryC(void)
    __attribute__((section (".imem_init.EV_preinit_routine_entryC")));

void
vPortFlcnSetTickIntrMask
(
    unsigned int mask
)
{
    lwrtosTickIntrMask = mask;
}

#if defined(DPU_RTOS)
//
// DPU needs to clear the TMER0 interrupt in a register different from IRQSCLR
// (at the source). Typically, all edge triggered interrupts should be okay to
// clear at IRQSCLR, but for some reason, this interrupt does not work without
// clearing at the source.
//
void
vPortFlcnSetOsTickIntrClearRegAndValue
(
    unsigned int reg,
    unsigned int value
)
{
    lwrtosTickIntrReg = reg;
    lwrtosTickIntrRegClearValue = value;
}
#endif

static inline void irq1_interrupt(void)
{
    unsigned int status;

    status = falc_ldx_i(CSB_REG(_IRQSTAT), 0);

    if (status & lwrtosTickIntrMask)
    {
#ifdef SCHEDULER_2X
        unsigned portBASE_TYPE uxTickCount;

        uxTickCount = 1 + lwrtosHookTickAdjustGet();

        while( uxTickCount > ( unsigned portBASE_TYPE ) 0 )
        {
            vTaskIncrementTick();
            --uxTickCount;
        }
#else /* SCHEDULER_2X */
        vTaskIncrementTick();
#endif /* SCHEDULER_2X */
        vTaskSwitchContext();

#ifdef DPU_RTOS
        if (lwrtosTickIntrReg == 0)
#endif
        {
            falc_stx(CSB_REG(_IRQSCLR), lwrtosTickIntrMask);
        }
    }

#ifdef DPU_RTOS
    if (lwrtosTickIntrReg != 0)
    {
        falc_stx((unsigned int *)lwrtosTickIntrReg, lwrtosTickIntrRegClearValue);
    }
#endif
}

static inline void exception_handler(void)
{
    unsigned int  exci;
    unsigned char excause;

    falc_rspr(exci, EXCI);
    excause = exci >> 20;

    switch (excause)
    {
        //
        // taskYIELD is implemented using a trap0 instruction. So when the
        // cause of the exception is trap0, force a context-switch.
        //
        case EXCI_TRAP0:
        case EXCI_TRAP2:
        {
            vTaskSwitchContext();
            break;
        }
        //
        // When the falcon ICD resumes from a breakpoint, it will raise an
        // "instruction breakpoint" exception. No action is required from SW
        // other than to gracefully exit the exception handler (i.e. do NOT
        // fall-through and treat this as an unhandled exception).
        //
        // Falcon-GDB also uses trap3 instructions to implement software-
        // breakpoints. Treat exceptions of this type in the same way as
        // "instruction breakpoint" exceptions.
        //
        case EXCI_IBREAKPOINT:
        case EXCI_TRAP3:
        {
#ifdef FLCNDBG_ENABLED
            extern void vApplicationIntOnBreak(void);
            vApplicationIntOnBreak();
#endif // FLCNDBG_ENABLED
            break;
        }
#ifdef ON_DEMAND_PAGING_OVL_IMEM
        case EXCI_IMEM_MISS:
        {
            unsigned int  exPc;
            extern void lwosOvlImemOnDemandPaging(unsigned int);

            exPc = DRF_VAL(_CSEC, _FALCON_EXCI2, _ADDRESS,
                              falc_ldx_i(CSB_REG(_EXCI2), 0));
            lwosOvlImemOnDemandPaging(exPc);
            vTaskSwitchContext();
            break;
        }
#endif  // ON_DEMAND_PAGING_OVL_IMEM
        case EXCI_ILLEGAL_INST:
        {
            //
            // Explicitly handle the illegal instruction exception in the OS and
            // bring the system down instead of allowing the application to
            // decide what to do
            //
            falc_halt();
            break;
        }
        // Application might want to override this.
        default:
        {
            falc_halt();
        }
    }
}

void IV0_routine_entryC(void)
{
    portFLCN_PRIV_LEVEL_RESET();

    if (lwrtosHookIV0Handler())
    {
        vTaskSwitchContext();
    }

    portISR_STACK_OVERFLOW_DETECT();

    portFLCN_PRIV_LEVEL_SET_FROM_LWRR_TASK();
}

void IV1_routine_entryC(void)
{
    portFLCN_PRIV_LEVEL_RESET();

    irq1_interrupt();

    portISR_STACK_OVERFLOW_DETECT();

    portFLCN_PRIV_LEVEL_SET_FROM_LWRR_TASK();
}

void EV_routine_entryC(void)
{
    portFLCN_PRIV_LEVEL_RESET();

    exception_handler();

    portESR_STACK_OVERFLOW_DETECT();

    portFLCN_PRIV_LEVEL_SET_FROM_LWRR_TASK();
}

void EV_preinit_routine_entryC(void)
{
    // MH-TODO: add proper handling of debugging breakpoints
    falc_halt();

    portESR_STACK_OVERFLOW_DETECT();
}
