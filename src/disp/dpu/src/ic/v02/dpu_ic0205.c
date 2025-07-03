/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_ic0205.c
 * @brief  Contains all Interrupt Control routines specific to DPU v0205.
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "dev_disp.h"
#include "dispflcn_regmacros.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu_gpuarch.h"
#include "dpu_objic.h"
#include "dpu_rttimer.h"
#include "lib_intfchdcp22wired.h"
#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Responsible for all initialization or setup pertaining to DPU interrupts.
 * This includes interrupt routing as well as setting the default enable-state
 * for all DPU interupt-sources (command queues, gptmr, gpios, etc ...).
 *
 * Notes:
 *     - The interrupt-enables for both falcon interrupt vectors should be OFF/
 *       disabled prior to calling this function.
 *     - This function does not modify the interrupt-enables for either vector.
 *     - This function MUST be called PRIOR to starting the scheduler.
 */
void
icInit_v02_05(void)
{
    LwU32  regVal;

    //
    // Setup interrupt routing
    //
    // Watchdog, halt, stallreq, swgen0 (MsgQ intr to RM), get routed to host.
    // The rest are routed to falcon on IV0; except gptimer/timer0(EXTIRQ3)
    // which gets routed to falcon on IV1.
    //
    regVal = (DFLCN_DRF_DEF(IRQDEST, _HOST_WDTMR,       _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_HALT,        _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_EXTERR,      _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_SWGEN0,      _HOST)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_GPTMR,       _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _HOST_CTXSW,       _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _HOST_EXT_EXTIRQ3, _FALCON)      |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_WDTMR,     _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_HALT,      _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_EXTERR,    _HOST_NORMAL) |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_SWGEN0,    _HOST_NORMAL));
    //
    // If we're using the Timer0 for OS scheduler, route it to falcon on IV1,
    // and route GPTMR to IV0.
    // Otherwise, route the Timer0(EXTIRQ3) to IV0, and route GPTMR to IV1.
    // OS scheduler always runs on an interrupt at IV1. The actual interrupt
    // bit can be set using @ref rtosFlcnSetOsTickIntrMask
    //
#if (DPUCFG_FEATURE_ENABLED(DPUTIMER0_FOR_OS_TICKS))
    {
        regVal |= DFLCN_DRF_DEF(IRQDEST, _TARGET_EXT_EXTIRQ3, _FALCON_IRQ1);
    }
#else
    {
        // Still route Timer0 to IV0 for hdcp22 timer usage.
        regVal |= DFLCN_DRF_DEF(IRQDEST, _TARGET_GPTMR,       _FALCON_IRQ1) |
                  DFLCN_DRF_DEF(IRQDEST, _TARGET_EXT_EXTIRQ3, _FALCON_IRQ0);
    }
#endif

    DFLCN_REG_WR32(IRQDEST, regVal);

    //
    // Enable the top-level Falcon interrupt sources
    //     - GPTMR/TIMER0 : general-purpose timer for OS scheduler tick
    //     - DSI          : interrupt from disp engine
    //     - CMDQ         : for communication with RM via command queue
    //
    // Note (Bug 709877):
    // We can't enable any intr which is targeted to RM at initial since the
    // intr may cause the RM to serve some disp intrs in an unexpected timing.
    // We leave it to RM to enable these bits when the RM is ready to serve the
    // disp intrs. So all the intrs fire during this period will be deferred
    // until the RM enables these bits (see RM dispEnableInterrupts_v02_00).
    //
#if (DPUCFG_FEATURE_ENABLED(DPUTIMER0_FOR_OS_TICKS))
    {
        regVal = DFLCN_DRF_DEF(IRQMSET, _EXT_EXTIRQ3, _SET);
    }
#else
    {
        regVal = DFLCN_DRF_DEF(IRQMSET, _GPTMR, _SET);
    }
#endif

    regVal |= DFLCN_DRF_DEF(IRQMSET, _DSI,  _SET) |
              DFLCN_DRF_DEF(IRQMSET, _CMDQ, _SET);

    DFLCN_REG_WR32(IRQMSET, regVal);
}

/*!
 * @brief  This function will setup the Timer0
 * @param[in]   bStartTimer     Start or stop timer
 * @param[in]   intervalUs      Timer interval in usec
 * @param[in]   bIsSrcPtimer    Source of timer DPU/PTIMER
 * @param[in]   bIsModeOneShot  Timer is one shot/continuous
 *
 */
void
icSetupTimer0Intr_v02_05
(
    LwBool      bStartTimer,
    LwU32       intervalUs,
    LwBool      bIsSrcPtimer,
    LwBool      bIsModeOneShot
)
{
    LwU32 data = 0;

    if (bStartTimer)
    {
        // Enable TIMER0
        data = DFLCN_REG_RD32(TIMER_0_INTR_EN);
        data = DFLCN_FLD_SET_DRF(TIMER_0_INTR_EN, _ENABLE, _YES, data);
        DFLCN_REG_WR32(TIMER_0_INTR_EN, data);

        // Lets reset the timer first
        data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _RESET, _TRIGGER, 0);
        DFLCN_REG_WR32(TIMER_CTRL, data);

        // Poll for reset to complete
        data = DFLCN_REG_RD32(TIMER_CTRL);
        while ((!DFLCN_FLD_TEST_DRF(TIMER_CTRL, _RESET, _DONE, data)))
        {
            data = DFLCN_REG_RD32(TIMER_CTRL);
        }

        // Write the interval first
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

        data = (bIsSrcPtimer) ?
            DFLCN_FLD_SET_DRF(TIMER_CTRL, _SOURCE, _PTIMER_1US, data):
            DFLCN_FLD_SET_DRF(TIMER_CTRL, _SOURCE, _DPU_CLK, data);

        data = (bIsModeOneShot) ?
            DFLCN_FLD_SET_DRF(TIMER_CTRL, _MODE, _ONE_SHOT, data):
            DFLCN_FLD_SET_DRF(TIMER_CTRL, _MODE, _CONTINUOUS, data);

        // Write Ctrl now
        DFLCN_REG_WR32(TIMER_CTRL, data);
    }
    else
    {
        data = DFLCN_REG_RD32(TIMER_CTRL);
        data = DFLCN_FLD_SET_DRF(TIMER_CTRL, _GATE, _STOP, data);
        DFLCN_REG_WR32(TIMER_CTRL, data);
    }
}

/*!
 * @brief Service the OR related intrs.
 *
 * When HDCP2.2 is enabled, once a SOR interrupt is received by DPU, it will
 * notify this as a event to HDCP 2.2 task which will in turn disable HDCP 2.2,
 * if it is found enabled.
 *
 */
void
icServiceOrIntr_v02_05(void)
{
    LwU8                 sorNum;
    LwU32                orIntr;
    LwU32                fwd2RmMsk;
    DISPATCH_HDCP22WIRED disp2Hdcp22;

    orIntr = REG_RD32(CSB, LW_PDISP_DSI_DPU_INTR_OR);

    for (sorNum = 0; sorNum < LW_PDISP_DSI_DPU_INTR_OR_SOR__SIZE_1; sorNum++)
    {
        if (FLD_IDX_TEST_DRF(_PDISP, _DSI_DPU_INTR_OR,
                             _SOR, sorNum, _PENDING, orIntr))
        {
            break;
        }
    }

    // So far we only care SOR intr when HDCP22WIRED is enabled
    if (DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP22WIRED) &&
        (sorNum != LW_PDISP_DSI_DPU_INTR_OR_SOR__SIZE_1))
    {
        // Forward the interrupt back to RM.
        fwd2RmMsk = REG_RD32(CSB, LW_PDISP_DSI_DPU_FWD2RM_OR);
        fwd2RmMsk = FLD_IDX_SET_DRF(_PDISP, _DSI_DPU_FWD2RM_OR,
                                     _SOR, sorNum, _FORWARD, fwd2RmMsk);
        REG_WR32(CSB, LW_PDISP_DSI_DPU_FWD2RM_OR, fwd2RmMsk);

        // Inject a command into HDCP22WIRED queue.
        disp2Hdcp22.hdcp22Evt.eventType = DISP2UNIT_EVT_SIGNAL;
        disp2Hdcp22.hdcp22Evt.subType   = HDCP22_SOR_INTR;
        disp2Hdcp22.hdcp22Evt.eventInfo.sorNum = sorNum;
        lwrtosQueueSendFromISR(Hdcp22WiredQueue, &disp2Hdcp22,
                               sizeof(disp2Hdcp22));
    }
    else
    {
        // Unexpected intr, clear it directly since we are unable to serve it.
        REG_WR32(CSB, LW_PDISP_DSI_DPU_INTR_OR, orIntr);
    }
}

/*!
 * Service the VBIOS Attention intr. Lwrrently VBIOS_ATTENTION is unused by RM
 * or VBIOS, thus is used as the command intr from SEC2 (for PlayReady).
 */
void
icServiceVbiosAttentionIntr_v02_05(void)
{
    DISPATCH_HDCP22WIRED   disp2Hdcp22;

    //
    // Try to process and respond SEC2's request in isr first. Only queue event
    // to HDCP22 task when we are not able to finish all the required operations
    // within isr.
    //
    if (hdcp22EarlyProcessSec2ReqInIsr() == FLCN_ERR_MORE_PROCESSING_REQUIRED)
    {
        // Inject a command to hdcp22 task to handle remaining operations.
        disp2Hdcp22.hdcp22Evt.eventType = DISP2UNIT_EVT_SIGNAL;
        disp2Hdcp22.hdcp22Evt.subType   = HDCP22_SEC2_INTR;
        lwrtosQueueSendFromISR(Hdcp22WiredQueue, &disp2Hdcp22,
                               sizeof(disp2Hdcp22));
    }

    // Clear the VBIOS_ATTENTION intr
    REG_WR32(CSB, LW_PDISP_DSI_DPU_INTR_SV,
        DRF_DEF(_PDISP, _DSI_DPU_INTR_SV, _VBIOS_ATTENTION, _RESET));

    // Re-enable the _DSI interrupts
    DFLCN_REG_WR32(IRQMSET,
        DFLCN_DRF_DEF(IRQMSET, _DSI, _SET));
}

