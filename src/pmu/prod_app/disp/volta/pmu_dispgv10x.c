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
 * @file   pmu_dispgv10x.c
 * @brief  HAL-interface for the GV10X Display Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_fuse.h"
#include "dev_disp.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objdisp.h"
#include "pmu_objfuse.h"
#include "pmu_objic.h"
#include "pmu_objpg.h"
#include "pmu_objms.h"
#include "pmu_disp.h"
#include "pmu_didle.h"
#include "class/clc37d.h"

#include "config/g_disp_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * This has to be adjusted based on the time DISP is really taking to process
 * the method submitted to the debug channel. For now setting an aggressive
 * timeout.
 */
#define DISP_METHOD_COMPLETION_WAIT_DELAY_US 4

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static LwBool s_dispCheckForMethodCompletion_GV10X (LwU32 feDebugCtl)
     GCC_ATTRIB_SECTION("imem_disp", "s_dispCheckForMethodCompletion_GV10X") GCC_ATTRIB_USED();
static void s_dispForwardSVInterrupt_GV10X(LwU8 intrType, LwU8 intrFwdTo)
    GCC_ATTRIB_SECTION("imem_resident", "s_dispForwardSVInterrupt_GV10X");

/*!
 * One-time Display-engine related HW/SW initialization.
 *
 * @param[in/out]  pBDispPresent   Pointer to the buffer storing the display
 *                                 present state
 *
 * @note This HAL is always safe to call.
 */
void
dispPreInit_GV10X
(
    LwBool *pBDispPresent
)
{
    //
    // Determine if this chip has a Display engine. Do NOT access any PDISP
    // registers prior to this point. Any PDISP register accesses that follow
    // must be protected by checks that ensure the Display engine is present
    // (either checks with DISP_IS_PRESENT() macro or checks based on features
    // that will be disabled when there is no Display).
    //
    *pBDispPresent = FLD_TEST_DRF(_FUSE, _STATUS_OPT_DISPLAY, _DATA, _ENABLE,
                                  fuseRead(LW_FUSE_STATUS_OPT_DISPLAY));
}

/*!
 * This function is responsible for processing DISP interrupts as well as for
 * notification of all required tasks.
 *
 * @note
 *    Do not call on displayless chips. Without a Display, there is no need to
 *    service Display interrupts as there will not be an Display interrupts to
 *    speak of.
 */
void
dispService_GV10X(void)
{
    DISPATCH_LPWR  lpwrEvt;
    LwU32          pmuDispatch;
    LwU32          intrHead;
    LwU32          intrGeneric;
    LwU32          exceptionChannelNumMask;
    LwU8           i;
    LwU8           intrType;
    LwU8           intrFwdTo;

    // BREAKPOINT_COND
    if (!DISP_IS_PRESENT())
    {
        PMU_BREAKPOINT();
    }

    while (LW_TRUE)
    {
        //
        // GF11x is using direct BAR0 access in PMU_BAR0_REG_RD/WR32() macros so
        // we can use them within ISR. Pre-GF11x this macros were exelwting code
        // that was wrapped into critical section so they were not usable within
        // ISRs.
        //
        // Due to the way how DISP interrupts are being aggregated and passed to
        // the PMU we need to assure that we do not leave this ISR if there is a
        // single DISP interrupt still pending. Hence if we leave ISR while DISP
        // interrupts are still pending than DISP_2PMU_INTR will never go to the
        // LOW state and we will never again have a raising edge of that signal.
        // That will completely block PMU from receiving a subsequent DISP IRQs.
        //
        // Additionally a HW bug in DISP v02_00 was found (details in bug 754559)
        // that if there are no DSI pri accesses between the moment when the IRQ
        // is cleared and when some other DISP IRQ fires again then the IRQ will
        // automatically clear itself and subsequently cause that the SW can not
        // detect it. The WAR for this issue is to access DSI pri register after
        // clearing pending interrupts and following DSI pri access handles that
        // as well. This HW bug should be fixed in class021x.
        //
        pmuDispatch = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_DISPATCH);

        //
        // We exit this ISR only if there is not a single DISP interrupt pending
        // to the PMU. We may fall into an endless loop only if the PMU does not
        // handle (and/or clear) the IRQ that is received, in which case the PMU
        // will stuck here and that will help us to identify an erroneous cases
        // like: incorrectly enabled, routed, and/or forwarded DISP interrupts.
        //
        if (pmuDispatch == 0)
        {
            break;
        }

        // Analyze and process HEAD interrupts.
        for (i = 0; i < Disp.numHeads; i++)
        {
            if (FLD_IDX_TEST_DRF(_PDISP, _FE_PMU_INTR_DISPATCH, _HEAD_TIMING, i,
                                 _PENDING, pmuDispatch))
            {
                intrHead = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_STAT_HEAD_TIMING(i));

                //
                // Details of the WAR for Bug 200401032:
                // There is bug in Turing display HW where MSCG can engage just
                // before the vblank region and there might not be enough time
                // to wake MSCG for next frame fetch and we will see underflow.
                // So we need to implement this WAR to tackle this scenario.
                //
                // Implementation Details:
                //
                // a. Once Last Frame is fetched and being rendered to display,
                //    then at certain RG LINE number (programmed based on the
                //    resolution and MSCG entry/exit latency), we will get the RG
                //    Line interrupt, at which point we need to disable MSCG.
                //
                // b. RG line number is callwlated to make sure that we will
                //    have enough time to wake MSCG before the next frame fetch
                //    starts.
                //
                // c. Now MSCG will be kept disabled until the start of the
                //    next frame.
                //
                // d. At the start of next frame, LOADV interrupt will come and
                //    we will reenable MSCG.
                //
                // e. In the new frame, MSCG will not actually engage until
                //    mempool has enough pixels and mscg_ok signal is asserted.
                //
                // For this purpose we have enabled the RG_LINE_B and LOADV
                // interrupts for coninuous mode MSCG (i.e ISO MSCG).
                //
                // TODO: We need to add handling of MSCG in OSM here
                //
                if (PMUCFG_FEATURE_ENABLED(PMU_DISP_MS_RG_LINE_WAR_200401032) &&
                    Disp.bMsRgLineWarActive)
                {
                    // Check for _RG_LINE_B Interrupt
                    if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_STAT_HEAD_TIMING,
                                     _RG_LINE_B, _PENDING, intrHead))
                    {
                        // Disable MSCG from IHUB.
                        isohubMscgEnableDisableIsr_HAL(&Disp,
                                                       LW_FALSE,
                                                       RM_PMU_DISP_MSCG_DISABLE_REASON_RG_LINE_WAR);
                    }

                    // Check for _LOADV Interrupt
                    if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_STAT_HEAD_TIMING,
                                     _LOADV, _PENDING, intrHead))
                    {
                        // Reenable MSCG from IHUB.
                        isohubMscgEnableDisableIsr_HAL(&Disp,
                                                       LW_TRUE,
                                                       RM_PMU_DISP_MSCG_DISABLE_REASON_RG_LINE_WAR);
                    }
                }

                // Check for _LAST_DATA interrupt.
                if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_STAT_HEAD_TIMING, _LAST_DATA,
                                 _PENDING, intrHead))
                {
                    LwU32 reg32;
                    DISPATCH_DISP disp2disp;

                    // Forward interrupt to RM immediately so that VBlank callbacks are not delayed
                    reg32 = REG_RD32(BAR0, LW_PDISP_FE_PMU_FWD2RM_HEAD_TIMING(i));
                    reg32 = FLD_SET_DRF(_PDISP, _FE_PMU_FWD2RM_HEAD_TIMING, _LAST_DATA,
                                        _FORWARD, reg32);

                    REG_WR32(BAR0, LW_PDISP_FE_PMU_FWD2RM_HEAD_TIMING(i), reg32);

                    if (PMUCFG_FEATURE_ENABLED(PMU_DISP_MSCG_LOW_FPS_WAR_200637376))
                    {
                        //
                        // Call Display task to callwlate new frame timer if WAR is enabled
                        // and MSCG is enabled
                        //
                        if (Disp.rmModesetData.bDisableVBlankMSCG &&
                            (Disp.mscgDisableReasons == 0) &&
                            (Disp.rmModesetData.mscgWarHeadIndex == i))
                        {
                            //
                            // Read and save the current line, while still in the
                            // interrupt context, in order to get an accurate reading,
                            // without any callback delays.
                            //
                            reg32 = REG_RD32(BAR0, LW_PDISP_RG_DPCA(i));
                            DispContext.lastDataLineCnt =
                                DRF_VAL(_PDISP, _RG_DPCA, _LINE_CNT, reg32);

                            disp2disp.hdr.eventType     = DISP_SIGNAL_EVTID;
                            disp2disp.signal.signalType = DISP_SIGNAL_LAST_DATA;
                            lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, DISP), &disp2disp,
                                                     sizeof(disp2disp));
                        }
                    }

                    // Don't clear the forwarded interrupt
                    intrHead =
                        FLD_SET_DRF(_PDISP, _FE_PMU_INTR_STAT_HEAD_TIMING, _LAST_DATA, _NOT_PENDING,
                                    intrHead);
                }

                // Check for the RG VBLANK.
                if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_STAT_HEAD_TIMING, _VBLANK,
                                 _PENDING, intrHead))
                {
                    //
                    // We pass the RG VBLANK IRQ only to a single task, and that
                    // one has a responsibility to notify others if appropriate.
                    //
                    if (PMUCFG_FEATURE_ENABLED(PMUTASK_DISP))
                    {
                        // Notify DISP task of RG VBLANK interrupt.
                        dispReceiveRGVblankISR(i);
                    }
                }

                //
                // Clear all pending HEAD interrupts. Lwrrently we support the
                // RG VBLANK, RG_LINE_A and the SD3_BUCKET_WALK_DONE.
                //
                REG_WR32(BAR0, LW_PDISP_FE_EVT_STAT_HEAD_TIMING(i), intrHead);
            }
        }

        //
        // Handle the Supervisor interrupts
        //
        if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_DISPATCH,
                         _CTRL_DISP, _PENDING, pmuDispatch))
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_MS))
            {
                intrGeneric = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_STAT_CTRL_DISP);

                DBG_PRINTF_ISR(("--DISPIRQ: DISPATCH 0x%x, SV=0x%x\n",
                                pmuDispatch, intrGeneric, 0, 0));

                lpwrEvt.hdr.eventType = LPWR_EVENT_ID_MS_DISP_INTR;
                lpwrEvt.intr.ctrlId = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
                intrType          = 0;
                intrFwdTo         = RM_PMU_DISP_INTR_FORWARD_TO_ILWALID;

                if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_STAT_CTRL_DISP,
                                 _SUPERVISOR1, _PENDING, intrGeneric))
                {
                    intrType  = RM_PMU_DISP_INTR_SV_1;
                    intrFwdTo = RM_PMU_DISP_INTR_FORWARD_TO_GSP;
                }
                else if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_STAT_CTRL_DISP,
                                      _SUPERVISOR3, _PENDING, intrGeneric))
                {
                    intrType  = RM_PMU_DISP_INTR_SV_3;
                    intrFwdTo = RM_PMU_DISP_INTR_FORWARD_TO_RM;
                }
                else if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_STAT_CTRL_DISP,
                                      _SUPERVISOR2, _PENDING, intrGeneric))
                {
                }
                else
                {
                    //
                    // Don't really expect any other interrupts. Instead of
                    // clearing the interrupts, halt to catch this condition.
                    //
                    PMU_HALT();
                }

                // Forward the SV interrupt
                lpwrEvt.intr.intrStat  = intrType;
                lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, LPWR),
                                         &lpwrEvt, sizeof(lpwrEvt));

                s_dispForwardSVInterrupt_GV10X(intrType, intrFwdTo);
            }
            else
            {
                //
                // Don't expect any SV interrupts when MSCG is disabled. Instead
                // of clearing the interrupts, halt to catch this condition.
                //
                PMU_HALT();
            }
        }

        // Handle the channel exception interrupts
        if (FLD_TEST_DRF(_PDISP, _FE_PMU_INTR_DISPATCH,
                         _EXC_WIN, _PENDING, pmuDispatch))
        {
            exceptionChannelNumMask = REG_RD32(FECS, LW_PDISP_FE_PMU_INTR_STAT_EXC_WIN);

            //
            // Forwad exception to RM
            // TODO: We should forward exception to RM after PMU completely exits
            // Sleep State.
            //
            REG_WR32(BAR0, LW_PDISP_FE_PMU_FWD2RM_EXC_OTHER, exceptionChannelNumMask);
        }
    }
}

/*!
 * @brief Init DISP RG VBLANK interrupts on given head.
 *
 * Set RM and GSP's RG VBLANK intr mask to disabled and clear pending intrs.
 * By default RM RG VBALNK intr mask is set to enabled.  Not disabling will
 * cause RG VBLANK interrupts to not appear on PMU side.
 */
void
dispInitVblankIntr_GV10X(LwU8 headIndex)
{
    LwU32 headMask;

    // BREAKPOINT_COND
    if (!DISP_IS_PRESENT())
    {
        PMU_BREAKPOINT();
    }

    appTaskCriticalEnter();
    {
        // Set PMU RG VBLANK mask to DISABLE
        headMask = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _VBLANK,
                               _DISABLE, headMask);
        REG_WR32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIndex), headMask);

        // Set GSP RG VBLANK mask to DISABLE
        headMask = REG_RD32(BAR0, LW_PDISP_FE_GSP_INTR_MSK_HEAD_TIMING(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _FE_GSP_INTR_MSK_HEAD_TIMING, _VBLANK,
                                   _DISABLE, headMask);
        REG_WR32(BAR0, LW_PDISP_FE_GSP_INTR_MSK_HEAD_TIMING(headIndex), headMask);

        // Set RM RG VBLANK mask to DISABLE
        headMask = REG_RD32(BAR0, LW_PDISP_FE_RM_INTR_MSK_HEAD_TIMING(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _FE_RM_INTR_MSK_HEAD_TIMING, _VBLANK,
                                   _DISABLE, headMask);
        REG_WR32(BAR0, LW_PDISP_FE_RM_INTR_MSK_HEAD_TIMING(headIndex), headMask);

        // Reset RG VBLANK intr
        headMask = REG_RD32(BAR0, LW_PDISP_FE_EVT_STAT_HEAD_TIMING(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _FE_EVT_STAT_HEAD_TIMING, _VBLANK,
                                   _RESET, headMask);
        REG_WR32(BAR0, LW_PDISP_FE_EVT_STAT_HEAD_TIMING(headIndex), headMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief Enable/disable DISP RG VBLANK interrupts on given head.
 *
 * It's callers responsibility to provide a valid headIndex value. Since
 * multiple tasks may require RG VBLANK interrupts we control access using a
 * semaphore (per-head) that counts enable/disable requests. RG VBLANK
 * interrupts are enabled when the semaphore count transitions from 0->1 and
 * disabled on 1->0 tranistions.
 *
 * @note
 *     Do not call on displayless chips. Calls to this function should be
 *     preceded by a check DISP_IS_PRESENT(). Without any heads, it is not
 *     logical to enable vblank interrupts.
 */
void
dispSetEnableVblankIntr_GV10X(LwU8 headIndex, LwBool bEnable)
{
    LwU32 headMask;

    // BREAKPOINT_COND
    if (!DISP_IS_PRESENT())
    {
        PMU_BREAKPOINT();
    }

    appTaskCriticalEnter();
    {
        if (bEnable)
        {
            if (DispVblankControlSemaphore[headIndex]++ == 0)
            {
                headMask =
                    REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIndex));
                headMask = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _VBLANK,
                                       _ENABLE, headMask);
                REG_WR32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIndex),
                                  headMask);
            }
        }
        else
        {
            //
            // Assuming that callers will ensure to execute disable call only IFF
            // enable was call was made before.
            //
            if (--DispVblankControlSemaphore[headIndex] == 0)
            {
                headMask =
                    REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIndex));
                headMask = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _VBLANK,
                                       _DISABLE, headMask);
                REG_WR32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIndex),
                                  headMask);
            }
        }
    }
    appTaskCriticalExit();
}

/*!
 * Get the number of heads.
 *
 * @return Number of heads.
 *
 * @note
 *     This HAL is always safe to call.
 */
LwU8
dispGetNumberOfHeads_GV10X(void)
{
    return DISP_IS_PRESENT() ?
        DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_HEADS,
            REG_RD32(BAR0, LW_PDISP_FE_MISC_CONFIGA)) : 0;
}

/*!
 * Get the number of sors.
 *
 * @return Number of sors.
 *
 * @note
 *     This HAL is always safe to call.
 */
LwU8
dispGetNumberOfSors_GV10X(void)
{
    return DISP_IS_PRESENT() ?
        DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_SORS,
            REG_RD32(BAR0, LW_PDISP_FE_MISC_CONFIGA)) : 0;
}

/*!
 * @brief Forward the supervisor interrupts to either RM or GSP
 *
 * Refer the dev_disp_fe.ref manual for the HW implications of the forwarding
 * mechanism to RM/PMU/GSP
 *
 * @param[in]  intrType   Type of SV interrupt. SV1 or SV3
 * @param[in]  intrFwdTo  Target to forward the interrupt to. RM or GSP
 *
 * @note
 *     Do not call on displayless chips. Without a Display engine, there will
 *     not be any SV interrupts to forward anyway.
 */
static void
s_dispForwardSVInterrupt_GV10X
(
    LwU8 intrType,
    LwU8 intrFwdTo
)
{
    LwU32 reg32;

    switch (intrFwdTo)
    {
        case RM_PMU_DISP_INTR_FORWARD_TO_RM:
        {
            reg32 = REG_RD32(BAR0, LW_PDISP_FE_PMU_FWD2RM_CTRL_DISP);
            reg32 = FLD_IDX_SET_DRF(_PDISP, _FE_PMU_FWD2RM_CTRL_DISP, _SUPERVISOR,
                                    intrType, _FORWARD, reg32);
            REG_WR32(BAR0, LW_PDISP_FE_PMU_FWD2RM_CTRL_DISP, reg32);
            break;
        }
        case RM_PMU_DISP_INTR_FORWARD_TO_DPU:
        {
            // Volta doesn't have DPU.
            PMU_HALT();
            break;
        }
        case RM_PMU_DISP_INTR_FORWARD_TO_GSP:
        {
            reg32 = REG_RD32(BAR0, LW_PDISP_FE_PMU_FWD2GSP_CTRL_DISP);
            reg32 = FLD_IDX_SET_DRF(_PDISP, _FE_PMU_FWD2GSP_CTRL_DISP, _SUPERVISOR,
                                    intrType, _FORWARD, reg32);
            REG_WR32(BAR0, LW_PDISP_FE_PMU_FWD2GSP_CTRL_DISP, reg32);
            break;
        }
        default:
            break;
    }
}

/*!
 * Enqueues method, data for a channel with given dbgctrl register.
 *
 * @param[in] dispChnlNum  Display channel number.
 * @param[in] methodOffset Display method offset.
 * @param[in] data         Method data.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if channel is invalid.
 * @return FLCN_ERROR on timeout of method completion.
 * @return FLCN_OK otherwise.
 */
FLCN_STATUS
dispEnqMethodForChannel_GV10X
(
    LwU32   dispChnlNum,
    LwU32   methodOffset,
    LwU32   data
)
{
    LwUPtr  feDebugCtl;
    LwU32   feDebugDat;
    LwU32   debugCtlRegVal;

    if (dispChnlNum >= LW_PDISP_FE_DEBUG_CTL__SIZE_1)
    {
        // Unexpected channel number.
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read Debug control and data registers.
    feDebugCtl = LW_PDISP_FE_DEBUG_CTL(dispChnlNum);
    feDebugDat = LW_PDISP_FE_DEBUG_DAT(dispChnlNum);

    // Method offset in clC37*.h is in bytes. Let's colwert it to DWORD offset
    // because that's what we use everywhere
    methodOffset >>= 2;

    // First write the data.
    REG_WR32(BAR0, feDebugDat, data);

    // Now write the method and the trigger
    debugCtlRegVal = REG_RD32(BAR0, feDebugCtl);
    debugCtlRegVal = FLD_SET_DRF_NUM(_PDISP, _FE_DEBUG_CTL, _METHOD_OFS,
                                     methodOffset, debugCtlRegVal);
    debugCtlRegVal = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _CTXDMA, _NORMAL,
                                 debugCtlRegVal);
    debugCtlRegVal = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _NEW_METHOD, _TRIGGER,
                                 debugCtlRegVal);
    debugCtlRegVal = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _MODE, _ENABLE,
                                 debugCtlRegVal);

    REG_WR32(BAR0, feDebugCtl, debugCtlRegVal);

    //
    // Do not wait if it is UPDATE method.
    // HW will reset LW_PDISP_FE_DEBUG_CTL_NEW_METHOD_PENDING bit after handling supervisors
    // in case of _UPDATE method.
    //
    if ((methodOffset << 2) != LWC37D_UPDATE)
    {
        if (!OS_PTIMER_SPIN_WAIT_US_COND(s_dispCheckForMethodCompletion_GV10X,
            feDebugCtl, DISP_METHOD_COMPLETION_WAIT_DELAY_US))
        {
            // Method timed out, hence restore debug mode setting and return error.
            return FLCN_ERROR;
        }
    }
    return FLCN_OK;
}

/*!
 * Checks to see if the method submitted is consumed / pending.
 *
 * @param[in] feDebutCtl  BAR0 register offset of DEBUG_CTL.
 *
 * @return LW_TRUE if method exelwtion is complete.
 */
static LwBool
s_dispCheckForMethodCompletion_GV10X
(
    LwU32 feDebugCtl
)
{
    LwU32 regVal = REG_RD32(BAR0, feDebugCtl);

    if (FLD_TEST_DRF(_PDISP, _FE_DEBUG_CTL, _NEW_METHOD, _PENDING, regVal))
    {
        return LW_FALSE;
    }
    else
    {
        return LW_TRUE;
    }
}
