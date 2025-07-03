/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dispflcn_icgv10x.c
 * @brief  Contains all Interrupt Control routines specific to GSP-Lite (GV10X).
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "lwostimer.h"
#include "dev_disp.h"
#include "dispflcn_regmacros.h"
#include "dev_gsp_csb_addendum.h"
#include "dev_gsp_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "lwdpu.h"
#include "dpu_task.h"
#include "dpu_gpuarch.h"
#include "dpu_objic.h"
#include "dpu_rttimer.h"
#include "dpu_mgmt.h"
#include "lwosdebug.h"

#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
#define MAX_POLL_WAIT       (10) // Wait for a max of 10usecs

/* ------------------------ Global Variables ------------------------------- */
LwBool IcCtxSwitchReq;

/* ------------------------ Static Variables ------------------------------- */
static void _icServiceDisplayIntr(void);
static void _icServiceCmdIntr(void);

/* ------------------------ Public Functions ------------------------------- */

/*!
 * Responsible for all initialization or setup pertaining to GSP-Lite interrupts.
 * This includes interrupt routing as well as setting the default enable-state
 * for all GSP-Lite interupt-sources (command queues, gptmr, gpios, etc ...).
 *
 * Notes:
 *     - The interrupt-enables for both falcon interrupt vectors should be OFF/
 *       disabled prior to calling this function.
 *     - This function does not modify the interrupt-enables for either vector.
 *     - This function MUST be called PRIOR to starting the scheduler.
 */
void
icInit_GV10X(void)
{
    LwU32  regVal;

    //
    // Setup interrupt routing
    //
    // Watchdog, halt, exterr, swgen0 (MsgQ intr to RM), swgen1 (not actively
    // used lwrrently) get routed to host. The rest are routed to falcon on IV0.
    //
    regVal = (DFLCN_DRF_DEF(IRQDEST, _HOST_WDTMR,         _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_HALT,          _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_EXTERR,        _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_SWGEN0,        _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_SWGEN1,        _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_GPTMR,         _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _HOST_CTXSW,         _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _HOST_EXT_EXTIRQ5,   _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _HOST_EXT_EXTIRQ6,   _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _HOST_EXT_EXTIRQ8,   _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_WDTMR,       _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_HALT,        _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_EXTERR,      _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_SWGEN0,      _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_SWGEN1,      _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_EXT_EXTIRQ5, _FALCON_IRQ0) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_EXT_EXTIRQ8, _FALCON_IRQ0));

    //
    // If we're using the RTTimer for OS scheduler, route it to falcon on IV1,
    // and route GPTMR to IV0.
    // Otherwise, route the RTTimer(EXTIRQ6) to IV0, and route GPTMR to IV1.
    // OS scheduler always runs on an interrupt at IV1. The actual interrupt
    // bit can be set using @ref rtosFlcnSetOsTickIntrMask
    //
#if (DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS))
    {
        regVal |= DFLCN_DRF_DEF(IRQDEST, _TARGET_GPTMR,       _FALCON_IRQ0) |
                  DFLCN_DRF_DEF(IRQDEST, _TARGET_EXT_EXTIRQ6, _FALCON_IRQ1);
    }
#else
    {
        // Still route Rttimer to IV0 for hdcp22 timer usage.
        regVal |= DFLCN_DRF_DEF(IRQDEST, _TARGET_GPTMR,       _FALCON_IRQ1) |
                  DFLCN_DRF_DEF(IRQDEST, _TARGET_EXT_EXTIRQ6, _FALCON_IRQ0);
    }
#endif

    DFLCN_REG_WR32(IRQDEST, regVal);

    // Setup command queue interrupt to be edge triggered
    regVal = DFLCN_REG_RD32(IRQMODE);
    regVal = DFLCN_FLD_SET_DRF(IRQMODE, _LVL_EXT_EXTIRQ5, _FALSE, regVal);

#if (DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS))
    // Setup RT timer interrupt to be edge triggered
    regVal = DFLCN_FLD_SET_DRF(IRQMODE, _LVL_EXT_EXTIRQ6, _FALSE, regVal);
#endif

    DFLCN_REG_WR32(IRQMODE, regVal);


    //
    // Enable the top-level Falcon interrupt sources
    //     - GPTMR/RTTIMER: general-purpose timer for OS scheduler tick
    //     - _EXT_EXTIRQ6 : realtime timer for OS scheduler tick
    //     - _EXT_EXTIRQ5 : for communication with RM via command queue
    //     - _EXT_EXTIRQ8 : for display interrupts
    //     - _HALT        : CPU transitioned from running into halt
    //     - _EXTERR      : for external mem access error interrupt
    //     - _SWGEN0      : for communication with RM via message queue
    //
#if (DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS))
    {
        regVal = DFLCN_DRF_DEF(IRQMSET, _EXT_EXTIRQ6, _SET);

    #if defined(GSPLITE_RTTIMER_WAR_ENABLED)
        //
        // Bug 200348935 that Volta RTTimer CG cause timer doesn't work correctly.
        // The WAR is to enable GPTimer together that avoid it enters low power state.
        //
        regVal |= DFLCN_DRF_DEF(IRQMSET, _GPTMR, _SET);
    #endif
    }
#else
    {
        regVal = DFLCN_DRF_DEF(IRQMSET, _GPTMR, _SET);
    }
#endif

    regVal |= DFLCN_DRF_DEF(IRQMSET, _EXT_EXTIRQ5, _SET);
    regVal |= DFLCN_DRF_DEF(IRQMSET, _EXT_EXTIRQ8, _SET);
    regVal |= DFLCN_DRF_DEF(IRQMSET, _HALT,        _SET);
    regVal |= DFLCN_DRF_DEF(IRQMSET, _SWGEN0,      _SET);
    regVal |= DFLCN_DRF_DEF(IRQMSET, _EXTERR,      _SET);

    DFLCN_REG_WR32(IRQMSET, regVal);

    DENG_REG_WR32(MISC_EXTIO_IRQMSET, DENG_DRF_DEF(MISC_EXTIO_IRQMSET, _DISP_INTR, _SET));
}

/*!
 * @brief  This function will setup the Rttimer
 * @param[in]   bStartTimer     Start or stop timer
 * @param[in]   intervalUs      Timer interval in usec
 * @param[in]   bIsSrcPtimer    Source of timer DPU/PTIMER
 * @param[in]   bIsModeOneShot  Timer is one shot/continuous
 * @return      FLCN_OK succeed else error.
 */
FLCN_STATUS
icSetupRttimerIntr_GV10X
(
    LwBool      bStartTimer,
    LwU32       intervalUs,
    LwBool      bIsSrcPtimer,
    LwBool      bIsModeOneShot
)
{
    LwU32 data;
    FLCN_STATUS retval = FLCN_OK;

    data = DFLCN_REG_RD32(TIMER_CTRL);

    if (bStartTimer)
    {
        LwU32 ptimer0Start;

        // Let's reset the timer first
        data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _GATE, _STOP, data);
        data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _RESET, _TRIGGER, data);
        DFLCN_REG_WR32(TIMER_CTRL, data);

        // Poll for the LW_CSEC_TIMER_CTRL_RESET
        ptimer0Start = DFLCN_REG_RD32(PTIMER0);
        do
        {
            data = DFLCN_REG_RD32(TIMER_CTRL);
            if (DFLCN_FLD_TEST_DRF(TIMER_CTRL, _RESET, _DONE, data))
            {
                break;
            }
        } while ((DFLCN_REG_RD32(PTIMER0) - ptimer0Start) < 1000 * MAX_POLL_WAIT);

        // Write the interval first:
        if (bIsModeOneShot)
        {
            DFLCN_REG_WR32(TIMER_0_INTERVAL, INTERVALUS_TO_TIMER0_ONESHOT_VAL(intervalUs));
        }
        else
        {
            DFLCN_REG_WR32(TIMER_0_INTERVAL, INTERVALUS_TO_TIMER0_CONTINUOUS_VAL(intervalUs));
        }

        // Program the ctrl to start the timer
        data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _GATE, _RUN, 0);
        if (bIsSrcPtimer)
        {
            data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _SOURCE, _PTIMER_1US, data);
        }
        else
        {
            data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _SOURCE, _DPU_CLK, data);
        }

        if (bIsModeOneShot)
        {
            data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _MODE, _ONE_SHOT, data);
        }
        else
        {
            data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _MODE, _CONTINUOUS, data);
        }

        // Write Ctrl now
        DFLCN_REG_WR32(TIMER_CTRL, data);
    }
    else
    {
        data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _GATE, _STOP, data);
        DFLCN_REG_WR32(TIMER_CTRL, data);
    }

    return retval;
}

/*!
 * @brief Get the IRQSTAT mask for the OS timer
 */
LwU32
dpuGetOsTickIntrMask_GV10X(void)
{
#if (DPUCFG_FEATURE_ENABLED(DPURTTIMER_FOR_OS_TICKS))
    {
        return DFLCN_DRF_SHIFTMASK(IRQSTAT_EXT_EXTIRQ6);
    }
#else
    {
        return DFLCN_DRF_SHIFTMASK(IRQSTAT_GPTMR);
    }
#endif
}

/*!
 * Handler-routine for dealing with all first-level DPU interrupts.  First-
 * level DPU interrupts include (but not limited to):
 *    -
 *
 * In call cases, processing is delegated to interrupt-specific handler
 * functions to allow this function to main generic.
 *
 * @return  See @ref _bCtxSwitchReq for details.
 */
LwBool
icService_GV10X(void)
{
    LwU32 status;
    LwU32 mask;
    LwU32 dest;

    IcCtxSwitchReq = LW_FALSE;

    mask   = DFLCN_REG_RD32(IRQMASK);
    dest   = DFLCN_REG_RD32(IRQDEST);

    // Only handle level 0 interrupts that are routed to falcon
    status = DFLCN_REG_RD32(IRQSTAT)
                 & mask
                 & ~(dest >> 16)
                 & ~(dest);

    // Service display interrupt
    if (DFLCN_FLD_TEST_DRF(IRQSTAT, _EXT_EXTIRQ8, _TRUE, status))
    {
        _icServiceDisplayIntr();
    }

    // Service CMD Queue interrupt
    if (DFLCN_FLD_TEST_DRF(IRQSTAT, _EXT_EXTIRQ5, _TRUE, status))
    {
        _icServiceCmdIntr();
    }

    // Clear interrupt using of blocking write
    DFLCN_REG_WR32_STALL(IRQSCLR, status);
    return IcCtxSwitchReq;
}

/*!
 * @brief Unmask command-queue interrupt
 */
void
icCmdQueueIntrUnmask_GV10X(void)
{
    //
    // For now, we only support one command queue, so we only unmask that one
    // queue head interrupt
    //
    DENG_REG_WR_DEF_STALL(CMD_INTREN, _HEAD_0_UPDATE, _ENABLED);
}

/*!
 * @brief Mask command-queue interrupt
 */
void
icCmdQueueIntrMask_GV10X(void)
{
    //
    // For now, we only support one command queue, so we only mask that one
    // queue head interrupt. By default, all queue heads are masked, and we
    // only unmask head0 when we initialize. Only that needs to be masked.
    //
    DENG_REG_WR_DEF_STALL(CMD_INTREN, _HEAD_0_UPDATE, _DISABLE);
}

/*!
 * @brief Clear the command-queue interrupt
 */
void
icCmdQueueIntrClear_GV10X(void)
{
    //
    // We only support one cmd queue today. When we add support for more
    // queues, we can clear the interrupt status for all queue heads.
    //
    DENG_REG_WR_DEF_STALL(CMD_HEAD_INTRSTAT, _0, _CLEAR);
}

/*!
 * @brief Mask the HALT interrupt so that the falcon can be safely halted
 * without generating an interrupt to RM
 */
void
icHaltIntrMask_GV10X(void)
{
    DFLCN_REG_WR_DEF_STALL(IRQMCLR, _HALT, _SET);
}

/*!
 * @brief Writes ICD command and IBRKPT registers when interrupt handler gets called.
 *
 * This routine writes the ICD command and IBRKPT registers when the interrupt handler gets
 * called in response to the SW interrrupt generated during breakpoint hit.
 */
void
icServiceSwIntr_GV10X(void)
{
    LwU32 data = 0;
    LwU32 ptimer0Start = 0;

    // Program EMASK of ICD
    data = DFLCN_FLD_SET_DRF(ICD_CMD, _OPC, _EMASK, data);
    data = DFLCN_FLD_SET_DRF(ICD_CMD, _EMASK_EXC_IBREAK, _TRUE, data);
    DFLCN_REG_WR32(ICD_CMD, data);

    // Wait for ICD done 1000 Ns
    ptimer0Start = DFLCN_REG_RD32(PTIMER0);
    while ((DFLCN_REG_RD32(PTIMER0) - ptimer0Start) <
            FLCN_DEBUG_DEFAULT_TIMEOUT)
    {
        // NOP
    }

    // Read back ICD and check for CMD ERROR
    data = DFLCN_REG_RD32(ICD_CMD);

    // regVal & 0x4000 != 0
    if (DFLCN_FLD_TEST_DRF(ICD_CMD, _ERROR, _TRUE, data))
    {
        lwuc_halt();
    }

    // The first breakpoint register
    data = DFLCN_REG_RD32(IBRKPT1);
    data = DFLCN_FLD_SET_DRF(IBRKPT1, _SUPPRESS, _ENABLE, data);
    DFLCN_REG_WR32(IBRKPT1, data);

    // The second breakpoint register
    data = DFLCN_REG_RD32(IBRKPT2);
    data = DFLCN_FLD_SET_DRF(IBRKPT2, _SUPPRESS, _ENABLE, data);
    DFLCN_REG_WR32(IBRKPT2, data);

    // Write SW GEN0 interrupt
    DFLCN_REG_WR32(IRQSSET,
            DFLCN_DRF_SHIFTMASK(IRQSSET_SWGEN0));

    // Write debug info register
    DFLCN_REG_WR32(DEBUGINFO, FLCN_DEBUG_DEFAULT_VALUE);
}

/*!
 * @brief Service the OR related intrs.
 *
 * When HDCP2.2 is enabled, once a SOR interrupt is received by GSP-Lite, it will
 * notify this as a event to HDCP 2.2 task which will in turn disable HDCP 2.2,
 * if it is found enabled.
 */
void
icServiceOrIntr_GV10X(void)
{
    // TODO: this function is not implemented yet.
}

/* ------------------------ Static Functions ------------------------------- */

/*!
 * Service display interrupt.
 */
static void
_icServiceDisplayIntr(void)
{
    //
    // Receving a display interrupt in GSP-Lite is not expected for now,
    // so we're halting the GSP-Lite for debug purpose.
    //
    DPU_HALT();
}

/*!
 * This function is responsible for signaling the command dispatcher task wake-
 * up and dispatch the new command to the appropriate unit-task.  This function
 * also disables all command-queue interrupts to prevent new interrupts from
 * coming in while the current command is being dispatched.  The interrupt must
 * be re-enabled by the command dispatcher task.
 */
static void
_icServiceCmdIntr(void)
{
    DISPATCH_CMDMGMT  dispatch;

    dispatch.disp2unitEvt = DISP2UNIT_EVT_COMMAND;

    // Mask further cmd queue interrupts.
    icCmdQueueIntrMask_HAL(&IcHal);

    lwrtosQueueSendFromISR(DpuMgmtCmdDispQueue, &dispatch, sizeof(dispatch));
}

