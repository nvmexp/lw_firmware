/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_icgp10x.c
 * @brief  Contains all Interrupt Control routines specific to SEC2 GP10x.
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "dev_sec_csb.h"
#include "sec2_objsec2.h"
#include "sec2_cmdmgmt.h"

#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * Responsible for all initialization or setup pertaining to SEC2 interrupts.
 * This includes interrupt routing as well as setting the default enable-state
 * for all SEC2 interupt-sources (command queues, gptmr, etc ...).
 *
 * Notes:
 *     - The interrupt-enables for both falcon interrupt vectors should be OFF/
 *       disabled prior to calling this function.
 *     - This function does not modify the interrupt-enables for either vector.
 *     - This function MUST be called PRIOR to starting the scheduler.
 *     - This function should be called to set up the SEC2 interrupts before
 *       calling @ref sec2StartOsTicksTimer to start the OS timer.
 */
void
icPreInit_GP10X(void)
{
    LwU32  regVal;

    //
    // Setup interrupt routing
    //
    // Halt, exterr, swgen0 (msg queue intr to RM), swgen1 (TRAP on
    // SEMAPHORE_D) get routed to host.
    // extirq5 (cmd queue intr to SEC2), extirq6 (RT timer intr) mthd, ctxsw
    // and watchdog timer are routed to falcon on IV0.
    //
    regVal = (DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_HALT,          _HOST)          |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_EXTERR,        _HOST)          |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_SWGEN0,        _HOST)          | /* msg queue intr */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_SWGEN1,        _HOST)          | /* SEMAPHORE_D TRAP */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_EXT_EXTIRQ5,   _FALCON)        | /* cmd queue intr */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_EXT_EXTIRQ6,   _FALCON)        | /* RT timer intr */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_GPTMR,         _FALCON)        |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_CTXSW,         _FALCON)        |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_MTHD,          _FALCON)        |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _HOST_WDTMR,         _FALCON)        | /* watchdog timer */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_HALT,        _HOST_NORMAL)   |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_EXTERR,      _HOST_NORMAL)   |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_SWGEN0,      _HOST_NORMAL)   | /* msg queue intr */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_SWGEN1,      _HOST_NONSTALL) | /* SEMAPHORE_D TRAP */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_EXT_EXTIRQ5, _FALCON_IRQ0)   | /* cmd queue intr */
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_CTXSW,       _FALCON_IRQ0)   |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_MTHD,        _FALCON_IRQ0)   |
              DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_WDTMR,       _FALCON_IRQ0));

    //
    // If we're using the RT Timer for OS scheduler, route it to falcon on IV1,
    // and route GPTMR to IV0.
    // Otherwise, route the RT Timer to IV0, and route GPTMR to IV1.
    // OS scheduler always runs on an interrupt at IV1. The actual interrupt
    // bit can be set using @ref rtosFlcnSetOsTickIntrMask
    //
    if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
    {
        regVal |= DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_GPTMR,       _FALCON_IRQ0) |
                  DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_EXT_EXTIRQ6, _FALCON_IRQ1);
    }
    else
    {
        regVal |= DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_GPTMR,       _FALCON_IRQ1) |
                  DRF_DEF(_CSEC, _FALCON_IRQDEST, _TARGET_EXT_EXTIRQ6, _FALCON_IRQ0);
    }
    REG_WR32(CSB, LW_CSEC_FALCON_IRQDEST, regVal);

    // Setup command queue interrupt to be edge triggered
    regVal = REG_RD32(CSB, LW_CSEC_FALCON_IRQMODE);
    regVal = FLD_SET_DRF(_CSEC, _FALCON_IRQMODE,
                         _LVL_EXT_EXTIRQ5, _FALSE, regVal);
    // Setup RT timer interrupt to be edge triggered
    regVal = FLD_SET_DRF(_CSEC, _FALCON_IRQMODE,
                         _LVL_EXT_EXTIRQ6, _FALSE, regVal);
    //
    // Setup ctxsw and mthd interrupts to be level triggered. These interrupts
    // come from host and are masked within the interrupt handler for deferred
    // processing once unmasked. Hence, they need to be level triggered; else
    // edge triggered interrupts would never cause an interrupt to be raised
    // once unmasked.
    //
    regVal = FLD_SET_DRF(_CSEC, _FALCON_IRQMODE, _LVL_CTXSW, _TRUE, regVal);
    regVal = FLD_SET_DRF(_CSEC, _FALCON_IRQMODE, _LVL_MTHD, _TRUE, regVal);
    REG_WR32(CSB, LW_CSEC_FALCON_IRQMODE, regVal);

    //
    // Unmask the top-level Falcon interrupt sources
    //     - HALT      : CPU transitioned from running into halt
    //     - EXTERR    : for external mem access error interrupt
    //     - SWGEN0    : for communication with RM via message queue
    //     - SWGEN1    : SEMAPHORE_D TRAP to wake up channels waiting on the trap
    //     - EXTIRQ5   : for communication with RM via command queue
    //     - CTXSW     : falcon context-switch interface
    //     - MTHD      : method interface to host
    //     - WDTMR     : watchdog timer
    //
    regVal = (DRF_DEF(_CSEC, _FALCON_IRQMSET, _HALT,        _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _EXTERR,      _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _SWGEN0,      _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _SWGEN1,      _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _EXT_EXTIRQ5, _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _CTXSW,       _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _MTHD,        _SET) |
              DRF_DEF(_CSEC, _FALCON_IRQMSET, _WDTMR,       _SET));

    //
    // Unmask either the RT timer or GPTMR, based on which is used for the
    // scheduler
    //
    if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
    {
        regVal |= DRF_DEF(_CSEC, _FALCON_IRQMSET, _EXT_EXTIRQ6, _SET);
    }
    else
    {
        regVal |= DRF_DEF(_CSEC, _FALCON_IRQMSET, _GPTMR, _SET);
    }

    REG_WR32(CSB, LW_CSEC_FALCON_IRQMSET, regVal);
}

/*!
 * @brief Unmask command queue interrupt
 */
void
icCmdQueueIntrUnmask_GP10X(LwU8 queueId)
{
    if (queueId == SEC2_CMDMGMT_CMD_QUEUE_RM)
    {
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _CMD_INTREN, _HEAD_0_UPDATE, _ENABLED);
    }
    else
    {
        // For now, we only support one command queue (_RM) till GA100
        SEC2_HALT();
    }
}

/*!
 * @brief Clear the cmd queue interrupt
 */
void
icCmdQueueIntrClear_GP10X(LwU8 queueId)
{
    if (queueId == SEC2_CMDMGMT_CMD_QUEUE_RM)
    {
        REG_WR32(CSB, LW_CSEC_CMD_HEAD_INTRSTAT,
                 DRF_DEF(_CSEC, _CMD_HEAD_INTRSTAT, _0, _CLEAR));
    }
    else
    {
        // For now, we only support one command queue (_RM) till GA100
        SEC2_HALT();
    }
}

/*!
 * @brief Mask command queue interrupt
 */
void
icCmdQueueIntrMask_GP10X(LwU8 queueId)
{
    if (queueId == SEC2_CMDMGMT_CMD_QUEUE_RM)
    {
        REG_FLD_WR_DRF_DEF_STALL(CSB, _CSEC, _CMD_INTREN, _HEAD_0_UPDATE, _DISABLE);
    }
    else
    {
        // For now, we only support one command queue (_RM) till GA100
        SEC2_HALT();
    }
}

/*!
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
LwBool
icIsCtxSwIntrPending_GP10X(void)
{
    return FLD_TEST_DRF(_CSEC, _FALCON_IRQSTAT, _CTXSW, _TRUE,
                        REG_RD32(CSB, LW_CSEC_FALCON_IRQSTAT));
}

/*!
 * @brief Get the IRQSTAT mask for the context switch timer
 */
LwU32
icOsTickIntrMaskGet_GP10X(void)
{
    if (SEC2CFG_FEATURE_ENABLED(SEC2RTTIMER_FOR_OS_TICKS))
    {
        return DRF_SHIFTMASK(LW_CSEC_FALCON_IRQSTAT_EXT_EXTIRQ6);
    }
    else
    {
        return DRF_SHIFTMASK(LW_CSEC_FALCON_IRQSTAT_GPTMR);
    }
}

/*!
 * @brief Trigger a SEMAPHORE_D TRAP interrupt
 */
void
icSemaphoreDTrapIntrTrigger_GP10X(void)
{
    REG_WR32(CSB, LW_CSEC_FALCON_IRQSSET,
             DRF_SHIFTMASK(LW_CSEC_FALCON_IRQSSET_SWGEN1));
}

