/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispgf11x.c
 * @brief  HAL-interface for the GF11x Display Engine.
 *
 * The following Display HAL routines service all GF11X chips, including chips
 * that do not have a Display engine. At this time, we do NOT stub out the
 * functions on displayless chips. It is therefore unsafe to call a specific
 * set of the following HALs on displayless chips. You must always refer to the
 * function dolwmenation of HAL to learn if it is safe to call on displayless
 * chips. In cases where the dolwmenation indicates it is unsafe to call, the
 * dolwmenation will provide additional information on how (and why) to avoid
 * the call. HALs that are unsafe to call on displayless chips will still
 * minimally debug-break if there is no Display engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"
#include "dev_top.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"
#include "pmu_objdisp.h"
#include "pmu_objic.h"
#include "pmu_objpg.h"
#include "pmu_didle.h"
#include "dbgprintf.h"
#include "pmu_disp.h"
#include "config/g_disp_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
static void s_dispForwardSVInterrupt_GMXXX(LwU8 intrType, LwU8 intrFwdTo)
    GCC_ATTRIB_SECTION("imem_resident", "s_dispForwardSVInterrupt_GMXXX");

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
dispService_GMXXX(void)
{
    DISPATCH_LPWR  lpwrEvt;
    LwU32          pmuDispatch;
    LwU32          intrHead;
    LwU32          intrGeneric;
    LwU32          exceptionChannelNumMask;
    LwU8           i;
    LwU8           intrType;
    LwU8           intrFwdTo;
    DISP_RM_ONESHOT_CONTEXT* pRmOneshotCtx = DispContext.pRmOneshotCtx;

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
        pmuDispatch = REG_RD32(BAR0, LW_PDISP_DSI_PMU_INTR_DISPATCH);

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
            if (FLD_IDX_TEST_DRF(_PDISP, _DSI_PMU_INTR_DISPATCH, _HEAD, i,
                                 _PENDING, pmuDispatch))
            {
                intrHead = REG_RD32(BAR0, LW_PDISP_DSI_PMU_INTR_HEAD(i));

                // Check for the RG_VBLANK.
                if (FLD_TEST_DRF(_PDISP, _DSI_PMU_INTR_HEAD, _RG_VBLANK,
                                 _PENDING, intrHead))
                {
                    //
                    // We pass the RG_VBLANK IRQ only to a single task, and that
                    // one has a responsibility to notify others if appropriate.
                    //
                    if (PMUCFG_FEATURE_ENABLED(PMUTASK_DISP))
                    {
                        // Notify DISP task of RG_VBLANK interrupt.
                        dispReceiveRGVblankISR(i);
                    }
                }

                //
                // Clear all pending HEAD interrupts. Lwrrently we support the
                // RG_VBLANK, RG_LINE and the SD3_BUCKET_WALK_DONE.
                //
                REG_WR32(BAR0, LW_PDISP_DSI_PMU_INTR_HEAD(i), intrHead);
            }
        }

        //
        // Handle the Supervisor interrupts
        //
        if (FLD_TEST_DRF(_PDISP, _DSI_PMU_INTR_DISPATCH,
                         _SUPERVISOR_VBIOS, _PENDING, pmuDispatch))
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_MS))
            {
                intrGeneric = REG_RD32(BAR0, LW_PDISP_DSI_PMU_INTR_SV);

                DBG_PRINTF_ISR(("--DISPIRQ: DISPATCH 0x%x, SV=0x%x\n",
                                pmuDispatch, intrGeneric, 0, 0));

                lpwrEvt.hdr.eventType = LPWR_EVENT_ID_MS_DISP_INTR;
                lpwrEvt.intr.ctrlId   = RM_PMU_LPWR_CTRL_ID_MS_MSCG;
                intrType          = 0;
                intrFwdTo         = RM_PMU_DISP_INTR_FORWARD_TO_ILWALID;

                if (FLD_TEST_DRF(_PDISP, _DSI_PMU_INTR_SV,
                                 _SUPERVISOR1, _PENDING, intrGeneric))
                {
                    intrType  = RM_PMU_DISP_INTR_SV_1;
                    intrFwdTo = RM_PMU_DISP_INTR_FORWARD_TO_DPU;
                }
                else if (FLD_TEST_DRF(_PDISP, _DSI_PMU_INTR_SV,
                                      _SUPERVISOR3, _PENDING, intrGeneric))
                {
                    intrType  = RM_PMU_DISP_INTR_SV_3;
                    intrFwdTo = RM_PMU_DISP_INTR_FORWARD_TO_RM;
                }
                else if (FLD_TEST_DRF(_PDISP, _DSI_PMU_INTR_SV,
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
                lpwrEvt.intr.intrStat = intrType;
                lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, LPWR),
                                         &lpwrEvt, sizeof(lpwrEvt));

                s_dispForwardSVInterrupt_GMXXX(intrType, intrFwdTo);
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
        if (FLD_TEST_DRF(_PDISP, _DSI_PMU_INTR_DISPATCH,
                         _EXCEPTION, _PENDING, pmuDispatch))
        {
            exceptionChannelNumMask = REG_RD32(FECS, LW_PDISP_DSI_PMU_INTR_EXCEPTION);

            if ((PMUCFG_FEATURE_ENABLED(PMU_MS_OSM)) &&
                (pRmOneshotCtx != NULL))
            {
                LwU8  channelNum;
                LwU32 exceptiolwal;

                channelNum   = pRmOneshotCtx->head.trappingChan;
                exceptiolwal = REG_RD32(FECS, LW_PDISP_EXCEPT(channelNum));

                //
                // Its FLIP exception if -
                //1) Exception on trappingChan AND
                //2) Exception type is trap exception
                //
                // Process Flip Exception:
                // Step1) Reset DWCF flag.
                // Step2) Send TRAP interrupt to PG task for further processing
                //
                if (FLD_IDX_TEST_DRF(_PDISP, _DSI_PMU_INTR_EXCEPTION, _CHN, channelNum,
                                     _PENDING, exceptionChannelNumMask) &&
                    FLD_TEST_DRF(_PDISP, _EXCEPT, _REASON, _TRAP, exceptiolwal))
                {
                    //
                    // Do nothing functionality is moved to PBI interrupt
                    // To do: Remove Trap hook from PMU
                    //
                }
            }

            //
            // Forwad exception to RM
            // TODO: We should forward exception to RM after PMU completely exits
            // Sleep State.
            //
            REG_WR32 (BAR0, LW_PDISP_DSI_PMU_FWD2RM_EXCEPTION, exceptionChannelNumMask);
        }
    }
}

/*!
 * @brief Init DISP RG_VBLANK interrupts on given head.
 *
 * Set RM and DPU's RG_VBLANK intr mask to disabled and clear pending intrs.
 * By default RM RG_VBALNK intr mask is set to enabled.  Not disabling will
 * cause RG_VBLANK interrupts to not appear on PMU side.
 */
void
dispInitVblankIntr_GMXXX(LwU8 headIndex)
{
    LwU32 headMask;

    // BREAKPOINT_COND
    if (!DISP_IS_PRESENT())
    {
        PMU_BREAKPOINT();
    }

    appTaskCriticalEnter();
    {
        // Set PMU RG_VBLANK mask to DISABLE
        headMask = REG_RD32(BAR0, LW_PDISP_DSI_PMU_INTR_MSK_HEAD(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _DSI_PMU_INTR_MSK_HEAD, _RG_VBLANK,
                               _DISABLE, headMask);
        REG_WR32(BAR0, LW_PDISP_DSI_PMU_INTR_MSK_HEAD(headIndex), headMask);

        // Set DPU RG_VBLANK mask to DISABLE
        headMask = REG_RD32(BAR0, LW_PDISP_DSI_DPU_INTR_MSK_HEAD(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _DSI_DPU_INTR_MSK_HEAD, _RG_VBLANK,
                               _DISABLE, headMask);
        REG_WR32(BAR0, LW_PDISP_DSI_DPU_INTR_MSK_HEAD(headIndex), headMask);

        // Reset DPU RG_VBLANK
        headMask = REG_RD32(BAR0, LW_PDISP_DSI_DPU_INTR_HEAD(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _DSI_DPU_INTR_HEAD, _RG_VBLANK,
                               _RESET, headMask);
        REG_WR32(BAR0, LW_PDISP_DSI_DPU_INTR_HEAD(headIndex), headMask);

        // Set RM RG_VBLANK mask to DISABLE
        headMask = REG_RD32(BAR0, LW_PDISP_DSI_RM_INTR_MSK_HEAD(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _DSI_RM_INTR_MSK_HEAD, _RG_VBLANK,
                               _DISABLE, headMask);
        REG_WR32(BAR0, LW_PDISP_DSI_RM_INTR_MSK_HEAD(headIndex), headMask);

        // Reset RM RG_VBLANK
        headMask = REG_RD32(BAR0, LW_PDISP_DSI_RM_INTR_HEAD(headIndex));
        headMask = FLD_SET_DRF(_PDISP, _DSI_RM_INTR_HEAD, _RG_VBLANK,
                               _RESET, headMask);
        REG_WR32(BAR0, LW_PDISP_DSI_RM_INTR_HEAD(headIndex), headMask);
    }
    appTaskCriticalExit();
}

/*!
 * @brief Enable/disable DISP RG_VBLANK interrupts on given head.
 *
 * It's callers responsibility to provide a valid headIndex value. Since
 * multiple tasks may require RG_VBLANK interrupts we control access using a
 * semaphore (per-head) that counts enable/disable requests. RG_VBLANK
 * interrupts are enabled when the semaphore count transitions from 0->1 and
 * disabled on 1->0 tranistions.
 *
 * @note
 *     Do not call on displayless chips. Calls to this function should be
 *     preceded by a check DISP_IS_PRESENT(). Without any heads, it is not
 *     logical to enable vblank interrupts.
 */
void
dispSetEnableVblankIntr_GMXXX(LwU8 headIndex, LwBool bEnable)
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
                    REG_RD32(BAR0, LW_PDISP_DSI_PMU_INTR_MSK_HEAD(headIndex));
                headMask = FLD_SET_DRF(_PDISP, _DSI_PMU_INTR_MSK_HEAD, _RG_VBLANK,
                                       _ENABLE, headMask);
                REG_WR32(BAR0, LW_PDISP_DSI_PMU_INTR_MSK_HEAD(headIndex),
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
                    REG_RD32(BAR0, LW_PDISP_DSI_PMU_INTR_MSK_HEAD(headIndex));
                headMask = FLD_SET_DRF(_PDISP, _DSI_PMU_INTR_MSK_HEAD, _RG_VBLANK,
                                       _DISABLE, headMask);
                REG_WR32(BAR0, LW_PDISP_DSI_PMU_INTR_MSK_HEAD(headIndex),
                                  headMask);
            }
        }
    }
    appTaskCriticalExit();
}

/*!
 * @brief Forward the supervisor interrupts to either RM or DPU
 *
 * Refer the dev_disp.ref manuals for the HW implications of the forwarding
 * mechanism to RM/PMU/DPU OR optionally refer to the following document
 * //hw/doc/gpu/display/fermi/specifications/GF11x_InterruptForwarding.doc
 *
 * @param[in]  intrType   Type of SV interrupt. SV1 or SV3
 * @param[in]  intrFwdTo  Target to forward the interrupt to. RM or DPU
 *
 * @note
 *     Do not call on displayless chips. Without a Display engine, there will
 *     not be any SV interrupts to forward anyway.
 */
static void
s_dispForwardSVInterrupt_GMXXX
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
            reg32 = REG_RD32(BAR0, LW_PDISP_DSI_PMU_FWD2RM_SV);
            reg32 = FLD_IDX_SET_DRF(_PDISP, _DSI_PMU_FWD2RM_SV, _SUPERVISOR,
                                    intrType, _FORWARD, reg32);
            REG_WR32(BAR0, LW_PDISP_DSI_PMU_FWD2RM_SV, reg32);
            break;
        }
        case RM_PMU_DISP_INTR_FORWARD_TO_DPU:
        {
            reg32 = REG_RD32(BAR0, LW_PDISP_DSI_PMU_FWD2DPU_SV);
            reg32 = FLD_IDX_SET_DRF(_PDISP, _DSI_PMU_FWD2DPU_SV, _SUPERVISOR,
                                    intrType, _FORWARD, reg32);
            REG_WR32(BAR0, LW_PDISP_DSI_PMU_FWD2DPU_SV, reg32);
            break;
        }
        default:
            break;
    }
}
