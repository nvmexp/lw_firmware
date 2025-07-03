/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    sec2_icgh100.c
 * @brief   Interrupt Service Routines
 *          Implements ISRs for gh100.
 *
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <flcnifcmn.h>

/* ------------------------ Register Includes ------------------------------ */
#include <engine.h>
#include <dev_top.h>
#include <riscv-intrinsics.h>

#include "dev_sec_csb.h"
#include "dev_ctrl.h"

// MK TODO: this is required to link with SafeRTOS core (that we must link with).
#define SAFE_RTOS_BUILD
#include <SafeRTOS.h>

#include <portfeatures.h>
#include <task.h>
#include <lwrtos.h>

#include <lwriscv/print.h>
/* ------------------------ Module Includes -------------------------------- */
#include "drivers/drivers.h"
#include "unit_dispatch.h"
#include "sec2utils.h"
#include "chnlmgmt.h"
#include "cmdmgmt.h"
#include "regmacros.h"
#include "csb.h"


/*!
 * This function is responsible for signaling the channel management task to
 * wake up and dispatch the new methods to the appropriate unit-task. This
 * function also masks method and ctxsw interrupts to prevent new interrupts
 * from coming in while the current interrupt is being handled. The interrupts
 * must be unmasked by the channel management task.
 */
sysKERNEL_CODE static void
_icServiceMthd_GH100(void)
{
    DISPATCH_CHNMGMT dispatch;
    LwU32 irqMclr = (DRF_DEF(_PRGNLCL, _RISCV_IRQMCLR, _MTHD, _SET) | DRF_DEF(_PRGNLCL, _RISCV_IRQMCLR, _CTXSW, _SET));
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, irqMclr);

    dispatch.eventType = DISP2CHNMGMT_EVT_METHOD;

    lwrtosQueueSendFromISR(Sec2ChnMgmtCmdDispQueue, &dispatch, sizeof(dispatch));
}

/*!
 * This function is responsible for signaling the channel management task to
 * wake up and save off the new context. This function also masks method and
 * ctxsw interrupts to prevent new interrupts from coming in while the current
 * interrupt is being handled. The interrupts must be unmasked by the channel
 * management task.
 */
sysKERNEL_CODE static void
icServiceCtxSw_GH100(void)
{
    DISPATCH_CHNMGMT dispatch;
    LwU32 irqMclr = (DRF_DEF(_PRGNLCL, _RISCV_IRQMCLR, _MTHD, _SET) | DRF_DEF(_PRGNLCL, _RISCV_IRQMCLR, _CTXSW, _SET));
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, irqMclr);

    dispatch.eventType = DISP2CHNMGMT_EVT_CTXSW;

    lwrtosQueueSendFromISR(Sec2ChnMgmtCmdDispQueue, &dispatch, sizeof(dispatch));
}

/* ------------------------ Public Functions ------------------------------- */
/*!
 * Handler-routine for dealing with all first-level SEC2 interrupts.
 *
 * In all cases, processing is delegated to interrupt-specific handler
 * functions to allow this function to remain generic.
 *
 */
sysKERNEL_CODE LwBool
icService_GH100(void)
{
    LwU32 irqstat, irqmask, irqdest;
    LwU32 irqLine, unexpectedIrq;

    irqstat  = intioRead(LW_PRGNLCL_FALCON_IRQSTAT);
    irqmask  = intioRead(LW_PRGNLCL_RISCV_IRQMASK);
    irqdest  = intioRead(LW_PRGNLCL_RISCV_IRQDEST); // ignore interrupts routed to host
    irqstat  = (irqstat & irqmask) & ~(irqdest);
    unexpectedIrq = 0;

    for (irqLine=0; irqLine <= DRF_BASE(LW_PRGNLCL_FALCON_IRQSTAT_MEMERR); ++irqLine)
    {
         if (irqstat & BIT(irqLine))
         {
             // ack - that'd be noop for level based irq
             intioWrite(LW_PRGNLCL_FALCON_IRQSCLR, BIT(irqLine));
             switch(irqLine)
             {
             case DRF_BASE(LW_PRGNLCL_FALCON_IRQSTAT_MTHD):
                 _icServiceMthd_GH100();
                 dbgPuts(LEVEL_INFO, "MTHD Interrupt\n");
                 break;
             case DRF_BASE(LW_PRGNLCL_FALCON_IRQSTAT_CTXSW):
                 icServiceCtxSw_GH100();
                 dbgPuts(LEVEL_INFO, "CTXSW Interrupt\n");
                 break;
             case DRF_BASE(LW_PRGNLCL_FALCON_IRQSTAT_EXT_EXTIRQ5):
                 dbgPuts(LEVEL_INFO, "EXT Interrupt\n");
                 cmdqIrqHandler();
                 break;
             case DRF_BASE(LW_PRGNLCL_FALCON_IRQSTAT_MEMERR):
                 dbgPuts(LEVEL_INFO, "MEMERR Interrupt\n");
                 irqHandleMemerr();
                 break;
             default:
                 dbgPuts(LEVEL_INFO, "Default Interrupt\n");
                 unexpectedIrq |= (irqstat & BIT(irqLine));
                 break;
             }
         }
    }
    if (unexpectedIrq)
    {
        dbgPrintf(LEVEL_CRIT, "Unexpected interrupts: %x\n", unexpectedIrq);
    }
    
    return LW_TRUE;
}


/*!
 * @brief Unmask host interface interrupts (ctxsw and method interrupts)
 */
sysSYSLIB_CODE void
icHostIfIntrUnmask_GH100(void)
{
    intioWrite(LW_PRGNLCL_RISCV_IRQMSET,
               (DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _MTHD,  _SET) |
                DRF_DEF(_PRGNLCL, _RISCV_IRQMSET, _CTXSW, _SET)));
}


/*!
 * @brief Trigger a SEMAPHORE_D TRAP interrupt
 */
sysSYSLIB_CODE void
icSemaphoreDTrapIntrTrigger_GH100(void)
{
    intioWrite(LW_PRGNLCL_FALCON_IRQSSET, DRF_SHIFTMASK(LW_PRGNLCL_FALCON_IRQSSET_SWGEN1));
}

/*!
 *  * @brief Set the GFID in the nonstall interrupt control
 *   */
sysSYSLIB_CODE void
icSetNonstallIntrCtrlGfid_GH100
(
    LwU32 gfid
)
{
    // The notification (nonstall) tree is at index 1
    LwU32 intrCtrl = intioRead(LW_PRGNLCL_FALCON_INTR_CTRL(1));

    //
    // HW unit registers, including SEC2's, don't have field defines since the units
    // send a transparent interrupt message; the field defines are in LW_CTRL.
    //
    intrCtrl = FLD_SET_DRF_NUM(_CTRL, _INTR_CTRL_ACCESS_DEFINES, _GFID, gfid, intrCtrl);
    intioWrite(LW_PRGNLCL_FALCON_INTR_CTRL(1), intrCtrl);
}

/*!
 * @brief Unmask command queue interrupt
 */
sysSYSLIB_CODE void
icCmdQueueIntrUnmask_GH100(LwU8 queueId)
{
    LwU32 regVal = 0;

    // For now, we only support RM queue
    if (queueId != SEC2_CMDMGMT_CMD_QUEUE_RM)
    {
        SEC2_BREAKPOINT();
    }

    regVal = csbRead(ENGINE_REG(_CMD_INTREN));

    regVal = FLD_IDX_SET_DRF(_CSEC, _CMD_INTREN, _HEAD_UPDATE,
                             queueId, _ENABLED, regVal);

    csbWrite(ENGINE_REG(_CMD_INTREN), regVal);
}

/*!
 * @brief Clear the cmd queue interrupt
 */
sysSYSLIB_CODE void
icCmdQueueIntrClear_GH100(LwU8 queueId)
{
    LwU32 regVal = 0;

    // For now, we only support RM queue
    if (queueId != SEC2_CMDMGMT_CMD_QUEUE_RM)
    {
        SEC2_BREAKPOINT();
    }

    regVal = csbRead(ENGINE_REG(_CMD_HEAD_INTRSTAT));

    regVal = FLD_IDX_SET_DRF(_CSEC, _CMD_HEAD_INTRSTAT, _FIELD,
                             queueId, _CLEAR, regVal);

    csbWrite(ENGINE_REG(_CMD_HEAD_INTRSTAT), regVal);
}


/*!
 * @brief Mask the HALT interrupt so that the SEC2 can be safely halted
 * without generating an interrupt to RM
 */
sysSYSLIB_CODE void
icHaltIntrMask_GH100(void)
{
    LwU32 regVal = 0;

    regVal = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMCLR, _HALT, _SET, regVal);
    intioWrite(LW_PRGNLCL_RISCV_IRQMCLR, regVal);
}

/*
 * @brief Returns whether a ctxsw interrupt is pending for use within RC error
 * recovery
 *
 * During RC recovery, we expect to have masked off both the ctxsw and mthd
 * interrupts. If we have a pending request during RC recovery, we would need
 * special handling for it.
 *
 * @returns  LW_TRUE  if ctxsw interrupt is pending
 *           LW_FALSE if ctxsw interrupt is not pending
 */
sysSYSLIB_CODE LwBool
icIsCtxSwIntrPending_GH100(void)
{
    return FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _CTXSW, _TRUE,
                        intioRead(LW_PRGNLCL_FALCON_IRQSTAT));
}

/*
*
* @brief Unmask CTXSW interrupt only
*/
sysSYSLIB_CODE void
icHostIfIntrCtxswUnmask_GH100(void)
{
    intioWrite(LW_PRGNLCL_FALCON_IRQMSET,
               DRF_DEF(_PRGNLCL, _FALCON_IRQMSET, _CTXSW,  _SET));
}
