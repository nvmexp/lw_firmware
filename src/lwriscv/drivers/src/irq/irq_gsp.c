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
 * @file    irq_ext.c
 * @brief   Interrupt driver - GSP specific part
 *
 *  Most of this driver is actually platform independent, it's GSP only because
 *  we assume interrupt tree belongs to GSP (and parse it as such).
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <flcnifcmn.h>

/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>
#include <dev_top.h>
#include <riscv-intrinsics.h>

/* ------------------------ SafeRTOS Includes ------------------------------ */
// MK TODO: this is required to link with SafeRTOS core (that we must link with).
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>

#include <portfeatures.h>
#include <task.h>
#include <lwrtos.h>

#include <lwriscv/print.h>
/* ------------------------ Module Includes -------------------------------- */
#include "drivers/drivers.h"

//
// For now interrupt handler code enabled (and owns) most of interrupts in engine.
// MK TODO: make it configurable (or requestable by tasks/kernel).
//

//------------------------------------------------------------------------------
// Code
//------------------------------------------------------------------------------

sysKERNEL_CODE FLCN_STATUS
irqInit(void)
{
    lwrtosTaskCriticalEnter();
    {

        LwU32 irqMset = DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN0, _SET)      |
                        DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN1, _SET)      |
                        DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _EXT_EXTIRQ5, _SET) | // CMDQ
                        DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _EXT_EXTIRQ8, _SET) | // extio
                        DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _MEMERR, _SET);       // PRI/HUB errors
        LwU32 irqDest = intioRead(LW_PRGNLCL_RISCV_IRQDEST);
        LwU32 irqType = intioRead(LW_PRGNLCL_RISCV_IRQTYPE);
        LwU32 irqMode = intioRead(LW_PRGNLCL_FALCON_IRQMODE);

        dbgPrintf(LEVEL_INFO, "Configuring interrupts...\n");

        irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN0, _HOST, irqDest);
        irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN1, _HOST, irqDest);
        irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _EXT_EXTIRQ5, _RISCV, irqDest);
        irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _EXT_EXTIRQ8, _RISCV, irqDest);
        irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _MEMERR, _RISCV, irqDest);

        irqType = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQTYPE, _SWGEN0, _HOST_NORMAL, irqType);
        irqType = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQTYPE, _SWGEN1, _HOST_NORMAL, irqType);

        irqMode = FLD_SET_DRF(_PGSP, _FALCON_IRQMODE, _LVL_SWGEN0, _FALSE, irqMode);
        irqMode = FLD_SET_DRF(_PGSP, _FALCON_IRQMODE, _LVL_SWGEN1, _FALSE, irqMode);
        irqMode = FLD_SET_DRF(_PGSP, _FALCON_IRQMODE, _LVL_EXT_EXTIRQ5, _FALSE, irqMode);
        irqMode = FLD_SET_DRF(_PGSP, _FALCON_IRQMODE, _LVL_EXT_EXTIRQ8, _FALSE, irqMode);
        irqMode = FLD_SET_DRF(_PGSP, _FALCON_IRQMODE, _LVL_MEMERR, _TRUE, irqMode);

        LwU32 irqDeleg = intioRead(LW_PRGNLCL_RISCV_IRQDELEG);
        irqDeleg = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDELEG, _SWGEN0, _RISCV_SEIP, irqDeleg);
        irqDeleg = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDELEG, _SWGEN1, _RISCV_SEIP, irqDeleg);
        irqDeleg = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDELEG, _EXT_EXTIRQ5, _RISCV_SEIP, irqDeleg); // CMDQ
        irqDeleg = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDELEG, _EXT_EXTIRQ8, _RISCV_SEIP, irqDeleg); // extio
        irqDeleg = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDELEG, _MEMERR, _RISCV_SEIP, irqDeleg); // PRI/HUB errors
        intioWrite(LW_PRGNLCL_RISCV_IRQDELEG, irqDeleg);

        intioWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
        intioWrite(LW_PRGNLCL_RISCV_IRQTYPE, irqType);
        intioWrite(LW_PRGNLCL_FALCON_IRQMODE, irqMode);

        // Enable Some interrupts for risc-v (TODO: drivers should enable them. )
        intioWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
        intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);
    }
    lwrtosTaskCriticalExit();

    return FLCN_OK;
}

sysKERNEL_CODE void
lwrtosRiscvExtIrqHook(void)
{
    LwU32 irqstat, irqmask, irqdest;
    LwU32 irqLine, unexpectedIrq;

    irqstat  = intioRead(LW_PRGNLCL_FALCON_IRQSTAT);
    irqmask  = intioRead(LW_PRGNLCL_RISCV_IRQMASK);
    irqdest  = intioRead(LW_PRGNLCL_RISCV_IRQDEST); // ignore interrupts routed to host
    irqstat  = (irqstat & irqmask) & ~(irqdest);
    unexpectedIrq = 0;

    // mask all interrupts
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, 0xFFFFFFFFU);

    for (irqLine=0; irqLine <= DRF_BASE(LW_PGSP_FALCON_IRQSTAT_MEMERR); ++irqLine)
    {
         if (irqstat & BIT(irqLine))
         {
             // ack - that'd be noop for level based irq
             intioWrite(LW_PRGNLCL_FALCON_IRQSCLR, BIT(irqLine));

             switch(irqLine)
             {
             case DRF_BASE(LW_PGSP_FALCON_IRQSTAT_EXT_EXTIRQ5):
                 cmdqIrqHandler();
                 break;
             case DRF_BASE(LW_PGSP_FALCON_IRQSTAT_MEMERR):
                 irqHandleMemerr();
                 break;
             default:
                 unexpectedIrq |= (irqstat & BIT(irqLine));
                 break;
             }
         }
    }

    if (unexpectedIrq)
    {
        dbgPrintf(LEVEL_CRIT, "Unexpected interrupts: %x\n", unexpectedIrq);
    }

    // unmask all interrupts
    intioWrite(LW_PRGNLCL_RISCV_IRQMSET, irqmask);
}
