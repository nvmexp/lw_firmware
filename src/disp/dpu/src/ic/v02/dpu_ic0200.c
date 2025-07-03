/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_ic0200.c
 * @brief  Contains all Interrupt Control routines specific to DPU v0200.
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "lwostimer.h"

#include "dev_disp.h"
#include "dispflcn_regmacros.h"

/* ------------------------ Application includes --------------------------- */
#include "lwdpu.h"
#include "dpu_task.h"
#include "dpu_gpuarch.h"
#include "dpu_objic.h"
#include "dpu_mgmt.h"
#include "dpu_scanoutlogging.h"

#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwBool IcCtxSwitchReq;

/* ------------------------ Static Variables ------------------------------- */
static void _icServiceDSI(void);
static void _icServiceCmd(void);
static void _icServiceSupervisorVbiosIntr(void);

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
icInit_v02_00(void)
{
    LwU32  regVal;

    //
    // Setup interrupt routing
    //
    // Watchdog, halt, stallreq, swgen0 (MsgQ intr to RM), get routed to host.
    // The rest are routed to falcon on IV0; except gptimer which gets routed
    // to falcon on IV1.
    //
    regVal = (DFLCN_DRF_DEF(IRQDEST, _HOST_WDTMR,    _HOST)          |
              DFLCN_DRF_DEF(IRQDEST, _HOST_HALT,     _HOST)          |
              DFLCN_DRF_DEF(IRQDEST, _HOST_EXTERR,   _HOST)          |
              DFLCN_DRF_DEF(IRQDEST, _HOST_SWGEN0,   _HOST)          |
              DFLCN_DRF_DEF(IRQDEST, _HOST_GPTMR,    _FALCON)        |
              DFLCN_DRF_DEF(IRQDEST, _HOST_CTXSW,    _FALCON)        |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_WDTMR,  _HOST_NORMAL)   |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_HALT,   _HOST_NORMAL)   |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_EXTERR, _HOST_NORMAL)   |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_SWGEN0, _HOST_NORMAL)   |
              DFLCN_DRF_DEF(IRQDEST, _TARGET_GPTMR,  _FALCON_IRQ1));

    DFLCN_REG_WR32(IRQDEST, regVal);

    //
    // Enable the top-level Falcon interrupt sources
    //     - GPTMR     : general-purpose timer for OS scheduler tick
    //     - DSI       : interrupt from disp engine
    //     - CMDQ      : for communication with RM via command queue
    //
    // Note (Bug 709877):
    // We can't enable any intr which is targeted to RM at initial since the
    // intr may cause the RM to serve some disp intrs in an unexpected timing.
    // We leave it to RM to enable these bits when the RM is ready to serve the
    // disp intrs. So all the intrs fire during this period will be deferred
    // until the RM enables these bits (see RM dispEnableInterrupts_v02_00).
    //
    regVal = DFLCN_DRF_DEF(IRQMSET, _GPTMR, _SET) |
             DFLCN_DRF_DEF(IRQMSET, _DSI,   _SET) |
             DFLCN_DRF_DEF(IRQMSET, _CMDQ,  _SET);

    DFLCN_REG_WR32(IRQMSET, regVal);

    // general-purpose timer setup (used by the scheduler)
    DFLCN_REG_WR_NUM(GPTMRINT, _VAL,
        (DpuInitArgs.cpuFreqHz / LWRTOS_TICK_RATE_HZ - 1));

    DFLCN_REG_WR_NUM(GPTMRVAL, _VAL,
        (DpuInitArgs.cpuFreqHz / LWRTOS_TICK_RATE_HZ - 1));

    // enable the general purpose timer
    DFLCN_REG_WR32(GPTMRCTL,
        DRF_SHIFTMASK(LW_PDISP_FALCON_GPTMRCTL_GPTMREN));
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
icService_v02_00(void)
{
    LwU32 status;
    LwU32 mask;
    LwU32 dest;

    IcCtxSwitchReq = LW_FALSE;

    mask   = DFLCN_REG_RD32(IRQMASK);
    dest   = DFLCN_REG_RD32(IRQDEST);

    // only handle level 0 interrupts that are routed to falcon
    status = DFLCN_REG_RD32(IRQSTAT)
                 & mask
                 & ~(dest >> 16)
                 & ~(dest);

    DBG_PRINTF_ISR(("---IRQ0: STAT 0x%x, MASK=0x%x, DEST=0x%x\n",
                    status, mask, dest, 0));

    //
    // DSI interrupt
    //
    if (DFLCN_FLD_TEST_DRF(IRQSTAT, _DSI, _TRUE, status))
    {
        DBG_PRINTF_ISR(("DSI interrupt\n", 0, 0, 0, 0));
        _icServiceDSI();
    }

    //
    // CMD Queue Interrupt
    //
    if (DFLCN_FLD_TEST_DRF(IRQSTAT, _CMDQ, _TRUE, status))
    {
        _icServiceCmd();
    }

    // clear interrupt using of blocking write
    DFLCN_REG_WR32_STALL(IRQSCLR, status);
    return IcCtxSwitchReq;
}

/*!
 * @brief Unmask command-queue interrupt
 */
void
icCmdQueueIntrUnmask_v02_00(void)
{
    //
    // For now, we only support one command queue, so we only unmask that one
    // queue head interrupt
    //
    DFLCN_REG_WR32(CMDQ_INTRMASK, DFLCN_DRF_DEF(CMDQ, _INTRMASK0, _ENABLE));
}

/*!
 * @brief Mask command-queue interrupt
 */
void
icCmdQueueIntrMask_v02_00(void)
{
    //
    // For now, we only support one command queue, so we only mask that one
    // queue head interrupt. By default, all queue heads are masked, and we
    // only unmask head0 when we initialize. Only that needs to be masked.
    //
    DFLCN_REG_WR32(CMDQ_INTRMASK, DFLCN_DRF_DEF(CMDQ, _INTRMASK0, _DISABLE));
}

/*!
 * @brief Mask the HALT interrupt so that the falcon can be safely halted
 * without generating an interrupt to RM
 */
void
icHaltIntrMask_v02_00(void)
{
    DFLCN_REG_WR_DEF_STALL(IRQMCLR, _HALT, _SET);
}

/*!
 * SW WAR for HW bug 743012 in DISP v02_00 -
 * If there is no DSI pri access after IRQ get cleared, then the next DISP
 * interrupts will be cleared automatically. For DPU, it even won't get into
 * this service routine if interrupts get cleared automatically. The WAR is add
 * redundant access to DSI pri. This HW bug should be fixed in class021x.
 */
void
icApplyWar743012_v02_00(void)
{
    REG_RD32(CSB, LW_PDISP_DSI_DPU_INTR_DISPATCH);
}

/* ------------------------ Static Functions ------------------------------- */

/*!
 * This function is responsible for handling the dsi interrupt. We need to
 * first check the _DPU_INTR_DISPATCH register to know the category of the
 * interrupt. Then we need to check the interrupt register of that category to
 * get the detail interrupt reason.
 */
static void
_icServiceDSI(void)
{
    LwU32 dsiDispatchIntr;
    LwU8  headNum;

    // disable _DSI interrupt
    DFLCN_REG_WR32(IRQMCLR, DFLCN_DRF_DEF(IRQMCLR, _DSI, _SET));

    dsiDispatchIntr = REG_RD32(CSB, LW_PDISP_DSI_DPU_INTR_DISPATCH);

    if (FLD_TEST_DRF(_PDISP, _DSI_DPU_INTR_DISPATCH,
                     _SUPERVISOR_VBIOS, _PENDING, dsiDispatchIntr))
    {
        _icServiceSupervisorVbiosIntr();

        //
        // After this intr is handled, the corresponding tasks will take care
        // the remaining intr handling (including re-enable DSI intr), so we
        // early return here.
        //
        return;
    }

    for (headNum = 0; headNum < LW_PDISP_DSI_DPU_INTR_DISPATCH_HEAD__SIZE_1; headNum++)
    {
        if (FLD_IDX_TEST_DRF(_PDISP, _DSI_DPU_INTR_DISPATCH,
                             _HEAD, headNum, _PENDING, dsiDispatchIntr))
        {
            icServiceHeadIntr_HAL(&IcHal, headNum);
        }
    }

    if (FLD_TEST_DRF(_PDISP, _DSI_DPU_INTR_DISPATCH,
                     _OR, _PENDING, dsiDispatchIntr))
    {
        icServiceOrIntr_HAL(&IcHal);
    }

    //
    // Do a redundant access to DSI pri to WAR the HW bug 743012. This HW bug
    // should be fixed after class021x.
    //
    icApplyWar743012_HAL(&IcHal);

    // re-enable the _DSI interrupts
    DFLCN_REG_WR32(IRQMSET, DFLCN_DRF_DEF(IRQMSET, _DSI, _SET));
}

/*!
 * This function is responsible for signaling the command dispatcher task wake-
 * up and dispatch the new command to the appropriate unit-task.  This function
 * also disables all command-queue interrupts to prevent new interrupts from
 * coming in while the current command is being dispatched.  The interrupt must
 * be re-enabled by the command dispatcher task.
 */
static void
_icServiceCmd(void)
{
    DISPATCH_CMDMGMT  dispatch;

    dispatch.disp2unitEvt = DISP2UNIT_EVT_COMMAND;

    // Mask further cmd queue interrupts.
    icCmdQueueIntrMask_HAL(&IcHal);

    lwrtosQueueSendFromISR(DpuMgmtCmdDispQueue, &dispatch,
                           sizeof(dispatch));
}

/*!
 * Service the Supervisor or VBIOS related intrs
 */
static void
_icServiceSupervisorVbiosIntr(void)
{
    LwU32 dsiIntr;

    dsiIntr = REG_RD32(CSB, LW_PDISP_DSI_DPU_INTR_SV);

    if (DPUCFG_FEATURE_ENABLED(DPUTASK_HDCP22WIRED) &&
        FLD_TEST_DRF(_PDISP, _DSI_DPU_INTR_SV, _VBIOS_ATTENTION, _PENDING, dsiIntr)
#if (DPU_PROFILE_v0205)
        // VBIOS_ATTENTION is now only used by SEC2 PlayReady task on chips later than GM206
        && (DISP_IP_VER_GREATER_OR_EQUAL(DISP_VERSION_V02_06))
#endif
    )
    {
        icServiceVbiosAttentionIntr_HAL(&IcHal);
    }
    else
    {
        // Unexpected intr, clear it directly since we are unable to serve it.
        REG_WR32(CSB, LW_PDISP_DSI_DPU_INTR_SV, dsiIntr);

        //
        // Re-enable the _DSI interrupt (In the cases above, the _DSI intr will
        // get enabled again after the intr is handled in its handler, so we
        // don't need to enable it in this function)
        //
        DFLCN_REG_WR32(IRQMSET, DFLCN_DRF_DEF(IRQMSET, _DSI, _SET));
    }
}

