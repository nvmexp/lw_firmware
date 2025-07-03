/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_disptu10x.c
 * @brief  HAL-interface for the TU10X Display Engine.
 *
 * The following Display HAL routines service all TU10X chips, including chips
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
#include "dev_trim.h"
#include "dev_fuse.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objdisp.h"
#include "pmu_disp.h"
#include "pmu_objpg.h"
#include "class/clc57d.h"
#include "lw_ref_dev_sr.h"
#include "pmu_objvbios.h"
#include "pmu_objseq.h"
#include "pmu_objfuse.h"
#include "pmu/pmuifdisp.h"

#include "config/g_disp_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_DISP_PMU_REG_POLL_RASTER_EXTN_TIMEOUT_NS      (50 * 1000 * 1000) // 50 ms
#define LW_DISP_METHOD_COMPLETION_TIMEOUT_NS             (10 * 1000 * 1000) // 10 ms
#define WAIT_FOR_HW_DETECT_MACROPLL_LOCK                 (10 * 1000)        // 10 us
#define WAIT_FOR_PLL_LOCK_READY                          (100 *1000)        // 100 us
#define WAIT_FOR_LOCK_STATUS_UPDATE                      (6 *1000)          // 6 us
#define LW_DISP_RG_RASTER_V_EXTEND_FRONT_PORCH_LIMIT     (2 << DRF_SIZE(LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH_SET_HEIGHT))
/* ------------------------- Prototypes-------------------------------------- */
static void s_dispProgramOsmIntr_TU10X(LwU8, LwBool)
    GCC_ATTRIB_SECTION("imem_libRmOneshot", "s_dispProgramOsmIntr_TU10X");
static FLCN_STATUS s_dispProgramMclkMempoolOverrides_TU10X(RM_PMU_DISP_IMP_MCLK_SWITCH_MEMPOOL *pMempoolLwrr,
                                                           RM_PMU_DISP_IMP_MCLK_SWITCH_MEMPOOL *pMempoolNew)
    GCC_ATTRIB_SECTION("imem_dispImp", "dispProgramMclkMempoolOverrides_TU10X");

/* ---------------------------Private functions ----------------------------- */
static FLCN_STATUS s_dispModesetHandleSv_TU10X(void);
// Making it NONINLINE to save stack size.
static FLCN_STATUS s_dispWaitForNLTComplete_TU10X(void)
    GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_dispPollForSvInterruptAndReset_TU10X(LwU32 dispSvIntrNum);
static void        s_dispResumeSvX_TU10X(void);
static void        s_dispRestoreXBarSettings_TU10X(void);
static FLCN_STATUS s_dispRestoreCoreChannel_TU10X(void);
static FLCN_STATUS s_dispRestoreWindowChannel_TU10X(void);
static void        s_dispRestorePostcompScalerCoefficients_TU10X(void);
static void        s_dispRestoreOperationalClass_TU10X(void);
static FLCN_STATUS s_dispModeset_TU10X(void);
static void        s_dispEnableChannelInterrupts_TU10X(void);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Enable/Disable RM_ONESHOT interrupts
 * Function enable/disables _LOADV interrupts.
 */
LwBool
dispRmOneshotStateUpdate_TU10X(void)
{
    LwU32  stallLock;
    LwU32  runMode;
    LwBool bLwrrentActive                  = LW_FALSE;
    LwBool bDeactivated                    = LW_FALSE;
    DISP_RM_ONESHOT_CONTEXT* pRmOneshotCtx = DispContext.pRmOneshotCtx;

    //
    // Step1) Check current state of oneshot mode.
    //        Oneshot mode will be active if -
    //        a) RM allows oneshot mode        AND
    //        b) Stall Lock is set to ONESHOT
    //
    if (pRmOneshotCtx->bRmAllow)
    {
        // Read the STALL LOCK register for stall lock status
        stallLock = REG_RD32(FECS, LW_UDISP_FE_CHN_ARMED_BASEADR(LW_PDISP_CHN_NUM_CORE) +
                             LWC57D_HEAD_SET_STALL_LOCK(pRmOneshotCtx->head.idx));

        // Read the DISPLAY RATE register for display rate mode
        runMode = REG_RD32(FECS, LW_UDISP_FE_CHN_ARMED_BASEADR(LW_PDISP_CHN_NUM_CORE) +
                           LWC57D_HEAD_SET_DISPLAY_RATE(pRmOneshotCtx->head.idx));

        if (FLD_TEST_DRF(C57D, _HEAD_SET_STALL_LOCK,   _ENABLE,   _TRUE,     stallLock) &&
            FLD_TEST_DRF(C57D, _HEAD_SET_STALL_LOCK,   _MODE,     _ONE_SHOT, stallLock) &&
            FLD_TEST_DRF(C57D, _HEAD_SET_DISPLAY_RATE, _RUN_MODE, _ONE_SHOT, runMode))
        {
            bLwrrentActive = LW_TRUE;
        }

    }

    //
    // Step2) Update interrupt state if new state does not match with
    //        old state of oneshot mode
    //
    if (bLwrrentActive != pRmOneshotCtx->bActive)
    {
        // Step2.1) Enable/Disable _LOADV and DWCF interrupt for PMU
        s_dispProgramOsmIntr_TU10X(pRmOneshotCtx->head.idx, bLwrrentActive);

        // Step2.2) Update state cache
        pRmOneshotCtx->bActive = bLwrrentActive;

        if (!bLwrrentActive)
        {
            bDeactivated                 = LW_TRUE;
            pRmOneshotCtx->bNisoMsActive = LW_FALSE;
        }
    }

    return bDeactivated;
}

/*!
 * @brief Activate/Deactivate MS Disp RG Line WAR
 *
 * It's callers responsibility to provide a valid headIdx value.
 *
 * @param[in]   rgLineNum   RgLine number for RG Line interrupt
 * @param[in]   headIdx     Head index value on which interrupt
 *                          needs to be enabled
 * @param[in]   bActivate   LW_TRUE - Activate the WAR
 *                          LW_FALSE - Deactivate the WAR
 *
 * @return      FLCN_OK                   On success
 * @return      FLCN_ERR_NOT_SUPPORTED    Invalid argument/configuration
 *              FLCN_ERR_ILWALID_ARGUMENT
 * @note
 *     Do not call on displayless chips. Calls to this function should be
 *     preceded by a check DISP_IS_PRESENT(). Without any heads, it is not
 *     logical to enable interrupts.
 */
FLCN_STATUS
dispMsRgLineWarActivate_TU10X
(
    LwU32  rgLineNum,
    LwU8   headIdx,
    LwBool bActivate
)
{
    LwU32 regVal;
    FLCN_STATUS status = FLCN_OK;

    //
    // Return failure if either of the condition is ture:
    // disp is not present or
    // headIdx is invalid
    //
    if ((!DISP_IS_PRESENT()) ||
        (headIdx >= LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING__SIZE_1))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;

        goto dispMsRgLineWarActivate_exit;
    }

    appTaskCriticalEnter();
    {
        if (bActivate)
        {
            // Reset/Clear the Pending _LOADV and RGLINE_B interrupt Event Bits
            regVal = REG_RD32(BAR0, LW_PDISP_FE_EVT_STAT_HEAD_TIMING(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_STAT_HEAD_TIMING, _LOADV,
                                 _RESET, regVal);
            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_STAT_HEAD_TIMING, _RG_LINE_B,
                                 _RESET, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_EVT_STAT_HEAD_TIMING(headIdx), regVal);

            // Enable the interrupts mask target as PMU
            regVal = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _LOADV,
                                 _ENABLE, regVal);
            regVal = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _RG_LINE_B,
                                 _ENABLE, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx), regVal);

            // Enable the _LOADV and RG_LINE_B interrupt events.
            regVal = REG_RD32(BAR0, LW_PDISP_FE_EVT_EN_SET_HEAD_TIMING(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_EN_SET_HEAD_TIMING, _LOADV,
                                 _SET, regVal);
            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_EN_SET_HEAD_TIMING, _RG_LINE_B,
                                 _SET, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_EVT_EN_SET_HEAD_TIMING(headIdx), regVal);

            // RG Line should be non zero number
            if (rgLineNum == 0)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;

                goto dispMsRgLineWarActivate_exit;
            }

            // Make sure RG Vblank extension is complete
            if (!PMU_REG32_POLL_NS(LW_PDISP_RG_RASTER_V_EXTEND_BACK_PORCH(headIdx),
                DRF_SHIFTMASK(LW_PDISP_RG_RASTER_V_EXTEND_BACK_PORCH_UPDATE),
                REF_DEF(LW_PDISP_RG_RASTER_V_EXTEND_BACK_PORCH_UPDATE, _DONE),
                LW_DISP_PMU_REG_POLL_RASTER_EXTN_TIMEOUT_NS, PMU_REG_POLL_MODE_BAR0_EQ))
            {
                status = FLCN_ERR_TIMEOUT;
                goto dispMsRgLineWarActivate_exit;
            }

            //
            // Program the RG Line Number and enable the interrupt based on
            // Line Number
            //
            regVal = REG_RD32(BAR0, LW_PDISP_RG_LINE_B_INTR(headIdx));

            regVal = FLD_SET_DRF_NUM(_PDISP, _RG_LINE_B_INTR, _LINE_CNT, rgLineNum, regVal);
            regVal = FLD_SET_DRF(_PDISP, _RG_LINE_B_INTR, _ENABLE, _YES, regVal);
            REG_WR32(BAR0, LW_PDISP_RG_LINE_B_INTR(headIdx), regVal);

            // Mark the War as active inside the MS Object
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_MS) &&
                pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
            {
                Disp.bMsRgLineWarActive = LW_TRUE;
            }
        }
        else
        {
            // Mark the War as deactive inside the MS Object
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_MS) &&
                pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
            {
                Disp.bMsRgLineWarActive = LW_FALSE;
            }

            // Reset the RG Count and disable interrupt
            regVal = REG_RD32(BAR0, LW_PDISP_RG_LINE_B_INTR(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _RG_LINE_B_INTR, _LINE_CNT, _INIT, regVal);
            regVal = FLD_SET_DRF(_PDISP, _RG_LINE_B_INTR, _ENABLE, _NO, regVal);
            REG_WR32(BAR0, LW_PDISP_RG_LINE_B_INTR(headIdx), regVal);

            // Disable the _LOADV and _RGLINE_B interrupt events.
            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_EN_CLR_HEAD_TIMING, _LOADV,
                                 _CLEAR, 0);
            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_EN_CLR_HEAD_TIMING, _RG_LINE_B,
                                 _CLEAR, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_EVT_EN_CLR_HEAD_TIMING(headIdx), regVal);

            // Disable the interrupts target as PMU
            regVal = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _LOADV,
                                 _DISABLE, regVal);
            regVal = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _RG_LINE_B,
                                 _DISABLE, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx), regVal);

            //
            // The RG line WAR is only active for a small period of time at the
            // end of a frame, but just in case the deactivate function happens
            // to be called during this brief interval, call the MSCG enable
            // function to remove the RG_LINE_WAR disable reason, so that MSCG
            // will not be blocked.
            //
            isohubMscgEnableDisableIsr_HAL(&Disp,
                                           LW_TRUE,
                                           RM_PMU_DISP_MSCG_DISABLE_REASON_RG_LINE_WAR);
        }
    }
    appTaskCriticalExit();

dispMsRgLineWarActivate_exit:

    return status;
}

/* ------------------------ Private Functions  ------------------------------ */

/*!
 * @brief Enable/disable DISP _LOADV interrupt on given head and DWCF
 * PMU interrupt
 *
 * It's callers responsibility to provide a valid headIdx value.
 *
 * @note
 *     Do not call on displayless chips. Calls to this function should be
 *     preceded by a check DISP_IS_PRESENT(). Without any heads, it is not
 *     logical to enable interrupts.
 */
static void
s_dispProgramOsmIntr_TU10X
(
    LwU8   headIdx,
    LwBool bEnable
)
{
    LwU32 regVal;

    // BREAKPOINT_COND
    if (!DISP_IS_PRESENT())
    {
        PMU_BREAKPOINT();
    }

    PMU_HALT_COND(headIdx < LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING__SIZE_1);

    appTaskCriticalEnter();
    {
        if (bEnable)
        {
            // Reset/Clear the Pending _LOADV interrupt Event Bits
            regVal = REG_RD32(BAR0, LW_PDISP_FE_EVT_STAT_HEAD_TIMING(headIdx));
            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_STAT_HEAD_TIMING, _LOADV,
                                 _RESET, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_EVT_STAT_HEAD_TIMING(headIdx), regVal);

            // Enable the interrupts mask target as PMU
            regVal = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _LOADV,
                                 _ENABLE, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx), regVal);

            // Enable the _LOADV interrupt events.
            regVal = REG_RD32(BAR0, LW_PDISP_FE_EVT_EN_SET_HEAD_TIMING(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_EN_SET_HEAD_TIMING, _LOADV,
                                 _SET, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_EVT_EN_SET_HEAD_TIMING(headIdx), regVal);

            // Enable the DWCF Interrupt
            dispProgramDwcfIntr_HAL(&Disp, LW_TRUE);
        }
        else
        {
            //
            // Disable the _LOADV interrupt events.
            //
            // Clear the _LOADV interrupt events enable bit.
            // Note: we are not using the read/modify/write seqeunce here
            // reason being, this register is used to disable the interrupt
            // event and when we read this register, it show the current
            // state of events. So if required bit is 1, then event is enabled
            // otherwise disabled.
            //
            // To disable the event, we also need to write 1 to that bit.
            // so if we do a read-modify-write, we will end up in disabling
            // all the events which are already enabled. That is the reason
            // we are just writing the required bit here by setting mask
            // for that.
            //
            regVal = FLD_SET_DRF(_PDISP, _FE_EVT_EN_CLR_HEAD_TIMING, _LOADV,
                                 _CLEAR, 0);
            REG_WR32(BAR0, LW_PDISP_FE_EVT_EN_CLR_HEAD_TIMING(headIdx), regVal);

            // Disable the interrupts target as PMU
            regVal = REG_RD32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx));

            regVal = FLD_SET_DRF(_PDISP, _FE_PMU_INTR_MSK_HEAD_TIMING, _LOADV,
                                 _DISABLE, regVal);
            REG_WR32(BAR0, LW_PDISP_FE_PMU_INTR_MSK_HEAD_TIMING(headIdx), regVal);

            // Disable the DWCF Interrupt
            dispProgramDwcfIntr_HAL(&Disp, LW_FALSE);
        }
    }
    appTaskCriticalExit();
}

/*!
 * @brief     Run the sequencer scripts which does modeset by PMU.
 *
 * @param[in] modesetType   Type of modeset to execute.
 *
 * @return    FLCN_OK if modeset completed.
 */
FLCN_STATUS
dispPmsModeSet_TU10X
(
    LwU32  modesetType,
    LwBool bTriggerSrExit
)
{
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, vbios)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispPms)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (Disp.pPmsBsodCtx == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto dispPmsModeSet_TU10X_exit;
        }

        switch (modesetType)
        {
            case DISP_GC6_EXIT_MODESET:
            {
                if (bTriggerSrExit)
                {
                    //
                    // RM has already triggered NLT.
                    // Wait for NLT completion before trigerring SR exit.
                    //
                    status = s_dispWaitForNLTComplete_TU10X();
                    if (status != FLCN_OK)
                    {
                        PMU_HALT();
                        goto dispPmsModeSet_TU10X_exit;
                    }

                    //
                    // Trigger sparse SR exit (Resync delay = 1).
                    // Modeset and SR exit will run parallel here.
                    //
                    status = dispPsrDisableSparseSr_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum);
                    if (status != FLCN_OK)
                    {
                        PMU_HALT();
                        goto dispPmsModeSet_TU10X_exit;
                    }
                }

                // Push UPDATE method via debug channel.
                status = dispEnqMethodForChannel_HAL(&Disp, LW_PDISP_CHN_NUM_CORE, LWC57D_UPDATE, 0);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                // Handle SV
                status = s_dispModesetHandleSv_TU10X();
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                break;
            }
            case DISP_MODESET_BSOD:
            {
                // Check VBIOS validity
                status = vbiosCheckValid(&Disp.pPmsBsodCtx->vbios.imageDesc);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                // Restore XBAR settings
                s_dispRestoreXBarSettings_TU10X();

                // Restore SOR PWR settings
                status = dispRestoreSorPwrSettings_HAL(&Disp);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                // Link training
                status = dispRunDpNoLinkTraining_HAL(&Disp);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                // Restore Core Channel
                status = s_dispRestoreCoreChannel_TU10X();
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                // Restore Window Channel
                status = s_dispRestoreWindowChannel_TU10X();
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                // Restore PostcompScaler coefficients
                s_dispRestorePostcompScalerCoefficients_TU10X();

                // Enable channel interrupts
                s_dispEnableChannelInterrupts_TU10X();

                status = s_dispModeset_TU10X();
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                // Disable Sparse SR
                status = dispPsrDisableSparseSr_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }
                break;
            }
            case DISP_MODESET_SR_EXIT:
            {
                // Disable Sparse SR
                status = dispPsrDisableSparseSr_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum);
                if (status != FLCN_OK)
                {
                    PMU_HALT();
                    goto dispPmsModeSet_TU10X_exit;
                }

                break;
            }
            default :
            {
                // Should not get here.
                status = FLCN_ERR_ILWALID_STATE;
                goto dispPmsModeSet_TU10X_exit;

            }
        }

dispPmsModeSet_TU10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief  Wait for NLT completion
 *
 * @return FLCN_STATUS FLCN_OK if link training completed
 */
static FLCN_STATUS
s_dispWaitForNLTComplete_TU10X(void)
{
    FLCN_TIMESTAMP  lwrTimeStamp;
    LwU8            dpAuxData;
    FLCN_STATUS     status;

    // Wait for LT
    osPTimerTimeNsLwrrentGet(&lwrTimeStamp);
    do
    {
        if ((osPTimerTimeNsElapsedGet(&lwrTimeStamp)) > LW_DISP_TIMEOUT_NS_NLTS_AUTO_CLEAR)
        {
            status = FLCN_ERR_TIMEOUT;
            goto s_dispWaitForNLTComplete_TU10X_exit;
        }

        status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_SR_NLT_CTL, LW_FALSE, &dpAuxData);
        if (status != FLCN_OK)
        {
            goto s_dispWaitForNLTComplete_TU10X_exit;
        }

        // Spin-wait between condition checks.
        OS_PTIMER_SPIN_WAIT_NS(100);

    } while (!FLD_TEST_DRF(_SR, _NLT_CTL, _NLT_START, _SYNCED, dpAuxData));

    // Disable Idle Pattern
    dispForceIdlePattern_HAL(&Disp, LW_FALSE);

s_dispWaitForNLTComplete_TU10X_exit:
    return status;
}

/*!
 * @brief  Trigger modeset and process
 *         supervisor interrupts.
 *
 * @return FLCN_OK if core channel restored
 */
static FLCN_STATUS
s_dispModesetHandleSv_TU10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       headId = Disp.pPmsBsodCtx->head.headId;
    LwU32       dpFrequency10KHz = Disp.pPmsBsodCtx->dpInfo.linkSpeedMul * 2700;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVbios)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // SV1
        status = s_dispPollForSvInterruptAndReset_TU10X(RM_PMU_DISP_INTR_SV_1);
        if (status != FLCN_OK)
        {
            goto dispModeset_TU10X_exit;
        }

        s_dispResumeSvX_TU10X();

        // SV2
        status = s_dispPollForSvInterruptAndReset_TU10X(RM_PMU_DISP_INTR_SV_2);
        if (status != FLCN_OK)
        {
            goto dispModeset_TU10X_exit;
        }

        // Restore VPLLs
        dispPmsModeSetRestoreVpll_HAL(&Disp, headId);

        // On SV2 IED script
        status = vbiosIedExelwteScriptTable(&Disp.pPmsBsodCtx->vbios.imageDesc,
            Disp.pPmsBsodCtx->vbios.iedScripts.onSv2,
            Disp.pPmsBsodCtx->vbios.conditionTableOffset,
            Disp.pPmsBsodCtx->dpInfo.portNum,
            Disp.pPmsBsodCtx->head.orIndex,
            Disp.pPmsBsodCtx->dpInfo.linkIndex,
            dpFrequency10KHz, 2, IED_TABLE_COMPARISON_GE);
        if (status != FLCN_OK)
        {
            goto dispModeset_TU10X_exit;
        }

        s_dispResumeSvX_TU10X();

        // SV3
        status = s_dispPollForSvInterruptAndReset_TU10X(RM_PMU_DISP_INTR_SV_3);
        if (status != FLCN_OK)
        {
            goto dispModeset_TU10X_exit;
        }

        // On SV3 IED script
        status = vbiosIedExelwteScriptTable(&Disp.pPmsBsodCtx->vbios.imageDesc,
            Disp.pPmsBsodCtx->vbios.iedScripts.onSv3,
            Disp.pPmsBsodCtx->vbios.conditionTableOffset,
            Disp.pPmsBsodCtx->dpInfo.portNum,
            Disp.pPmsBsodCtx->head.orIndex,
            Disp.pPmsBsodCtx->dpInfo.linkIndex,
            dpFrequency10KHz, 2, IED_TABLE_COMPARISON_GE);
        if (status != FLCN_OK)
        {
            goto dispModeset_TU10X_exit;
        }

        s_dispResumeSvX_TU10X();

dispModeset_TU10X_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief  Disable the debug mode for the channel specied
 *
 * @param[in]   dispChannelNum   Channel Number
 *
 * @return void
 */
FLCN_STATUS
dispDisableDebugMode_TU10X
(
    LwU32 dispChannelNum
)
{
    LwU32   feDebugCtl;
    LwU32   debugCtlRegVal;

    if (dispChannelNum > LW_PDISP_FE_DEBUG_CTL__SIZE_1)
    {
        // Unexpected channel number.
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Read Debug control and data registers.
    feDebugCtl = LW_PDISP_FE_DEBUG_CTL(dispChannelNum);
    debugCtlRegVal = REG_RD32(BAR0, feDebugCtl);

    // Wait for core channel to be idle
    if (!PMU_REG32_POLL_NS(feDebugCtl, DRF_SHIFTMASK(LW_PDISP_FE_DEBUG_CTL_NEW_METHOD),
        REF_DEF(LW_PDISP_FE_DEBUG_CTL_NEW_METHOD, _DONE),
        LW_DISP_METHOD_COMPLETION_TIMEOUT_NS, PMU_REG_POLL_MODE_BAR0_EQ))
    {
        return FLCN_ERR_TIMEOUT;
    }

    debugCtlRegVal = REG_RD32(BAR0, feDebugCtl);
    debugCtlRegVal = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _MODE, _DISABLE, debugCtlRegVal);
    REG_WR32(BAR0, feDebugCtl, debugCtlRegVal);

    return FLCN_OK;
}

/*!
 * @brief  Wait for and reset a supervisor interrupts
 *
 * @param[in] dispSvIntrNum    Supervisor number
 *
 * @return FLCN_STATUS FLCN_OK if interrupt is found
 */
static FLCN_STATUS
s_dispPollForSvInterruptAndReset_TU10X
(
    LwU32 dispSvIntrNum
)
{
    LwU32          data;
    LwU32          elapsedTime = 0;
    FLCN_TIMESTAMP lwrrentTime;
    LwBool         bFound      = LW_FALSE;

    osPTimerTimeNsLwrrentGet(&lwrrentTime);

    do
    {
        data = REG_RD32(BAR0, LW_PDISP_FE_EVT_STAT_CTRL_DISP);
        switch (dispSvIntrNum)
        {
        case RM_PMU_DISP_INTR_SV_1:
            if (FLD_TEST_DRF(_PDISP, _FE_EVT_STAT_CTRL_DISP, _SUPERVISOR1,
                _PENDING, data))
            {
                bFound = LW_TRUE;
            }
            break;
        case RM_PMU_DISP_INTR_SV_2:
            if (FLD_TEST_DRF(_PDISP, _FE_EVT_STAT_CTRL_DISP, _SUPERVISOR2,
                _PENDING, data))
            {
                bFound = LW_TRUE;
            }
            break;
        case RM_PMU_DISP_INTR_SV_3:
            if (FLD_TEST_DRF(_PDISP, _FE_EVT_STAT_CTRL_DISP, _SUPERVISOR3,
                _PENDING, data))
            {
                bFound = LW_TRUE;
            }
            break;
        default:
            return FLCN_ERR_ILWALID_ARGUMENT;
        }

        if (bFound || (elapsedTime > LW_DISP_TIMEOUT_FOR_SVX_INTR_NSECS))
        {
            break;
        }

        lwrtosYIELD();
        elapsedTime = osPTimerTimeNsElapsedGet(&lwrrentTime);
    } while (LW_TRUE);

    if (!bFound)
    {
        return FLCN_ERR_TIMEOUT;
    }

    data = DRF_IDX_DEF(_PDISP, _FE_EVT_STAT_CTRL_DISP, _SUPERVISOR, (dispSvIntrNum - RM_PMU_DISP_INTR_SV_1), _RESET);
    REG_WR32(BAR0, LW_PDISP_FE_EVT_STAT_CTRL_DISP, data);

    return FLCN_OK;
}

/*!
 * @brief  Resume supervisor interrupts
 *
 * @return void
 */
static void
s_dispResumeSvX_TU10X(void)
{
    LwU32 svReg = 0;

    // Disable snooze frames.
    svReg = FLD_SET_DRF(_PDISP, _FE_SUPERVISOR_HEAD, _FORCE_NOBLANK_WAKEUP, _YES, svReg);
    svReg = FLD_SET_DRF(_PDISP, _FE_SUPERVISOR_HEAD, _FORCE_NOBLANK_SHUTDOWN, _YES, svReg);
    REG_WR32(BAR0, LW_PDISP_FE_SUPERVISOR_HEAD(Disp.pPmsBsodCtx->head.headId), svReg);

    svReg = REG_RD32(BAR0, LW_PDISP_FE_SUPERVISOR_MAIN);

    svReg = FLD_SET_DRF(_PDISP, _FE_SUPERVISOR_MAIN, _SKIP_SECOND_INT, _NO, svReg);
    svReg = FLD_SET_DRF(_PDISP, _FE_SUPERVISOR_MAIN, _SKIP_THIRD_INT, _NO, svReg);

    // Trigger the restart.
    svReg = FLD_SET_DRF(_PDISP, _FE_SUPERVISOR_MAIN, _RESTART, _TRIGGER, svReg);

    REG_WR32(BAR0, LW_PDISP_FE_SUPERVISOR_MAIN, svReg);
}

/*!
 * @brief  restore the Xbar settings
 *
 * @return void
 */
static void
s_dispRestoreXBarSettings_TU10X(void)
{
    LwU32 padlink;

    // Restore padlink state
    for (padlink = 0; padlink < Disp.pPmsBsodCtx->numPadlinks; padlink++)
    {
        if (BIT(padlink) & Disp.pPmsBsodCtx->padLinkMask)
        {
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CMGR_CLK_LINK_CTRL(padlink),
                Disp.pPmsBsodCtx->savedPadlinkState[padlink]);
        }
    }
}

/*!
 * @brief     Configure display SOR settings
 *
 * @param[in] void
 *
 * @return    void
 */
void
dispConfigureSorSettings_TU10X(void)
{
    LwU32 value;
    LwU32 sorPll4;
    LwU32 padPll;
    FLCN_TIMESTAMP lwrTimeStamp;

    sorPll4 = REG_RD32(FECS, LW_PDISP_SOR_PLL4(Disp.pPmsBsodCtx->head.orIndex));

    // put the SOR  to DP safe
    value = REG_RD32(FECS, LW_PDISP_FE_CMGR_CLK_SOR(Disp.pPmsBsodCtx->head.orIndex));
    value = FLD_SET_DRF(_PDISP, _FE_CMGR_CLK_SOR, _MODE_BYPASS, _DP_SAFE, value);
    value = FLD_SET_DRF(_PDISP, _FE_CMGR_CLK_SOR, _CLK_SOURCE, _DIFF_DPCLK, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CMGR_CLK_SOR(Disp.pPmsBsodCtx->head.orIndex), value);

    // program its DP B/W and ...
    value = FLD_SET_DRF_NUM(_PDISP, _FE_CMGR_CLK_SOR, _LINK_SPEED, Disp.pPmsBsodCtx->dpInfo.linkSpeedMul, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CMGR_CLK_SOR(Disp.pPmsBsodCtx->head.orIndex), value);

    if (!FLD_TEST_DRF(_PDISP, _SOR_PLL4, _ENB_LOCKDET, _YES, sorPll4))
    {
        //
        // If _PLL4_ENB_LOCKDET != _YES, do not override the setting. But instead
        // wait for default 10usec (6usec + buffer time) and return OK.
        //
        OS_PTIMER_SPIN_WAIT_NS(WAIT_FOR_HW_DETECT_MACROPLL_LOCK);
        goto dispConfigureSorSettings_TU10X_exit;
    }

    //
    // A Simple poll for PLL_LOCK_READY will not help here. Why?
    // dp_normal -> dp_safe temporarily gates off the clock thats being switched;
    // This clock is needed to update lock status.
    // Lock status cannot get updated until the clock is back on.
    // We need to wait long enough to ensure that clock is back on and lock
    // status has been updated to reflect that pll is no longer locked;
    //
    // HW suggested time is around 6usec. (This is also default used by
    // CMGR HW pre/post_clk state machines).
    // HW suggested timeout is ~16usec. Increase further to 100 just in case.
    //
    OS_PTIMER_SPIN_WAIT_NS(WAIT_FOR_LOCK_STATUS_UPDATE);

    osPTimerTimeNsLwrrentGet(&lwrTimeStamp);
    do
    {
        if ((osPTimerTimeNsElapsedGet(&lwrTimeStamp)) > LW_DISP_TIMEOUT_NS_NLTS_AUTO_CLEAR)
        {
            goto dispConfigureSorSettings_TU10X_exit;
        }
        OS_PTIMER_SPIN_WAIT_NS(WAIT_FOR_PLL_LOCK_READY);
        padPll = REG_RD32(FECS, LW_PDISP_SOR_DP_PADPLL(Disp.pPmsBsodCtx->head.orIndex,  Disp.pPmsBsodCtx->dpInfo.linkIndex));
    } while (FLD_TEST_DRF(_PDISP, _SOR_DP_PADPLL, _MACROPLL_LOCK_READY, _NO, padPll));

    // put SOR back to  _DP_NORMAL
    value = FLD_SET_DRF(_PDISP, _FE_CMGR_CLK_SOR, _MODE_BYPASS, _DP_NORMAL, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CMGR_CLK_SOR(Disp.pPmsBsodCtx->head.orIndex), value);

    value = REG_RD32(FECS, LW_PDISP_SOR_DP_LINKCTL(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.linkIndex));
    value = FLD_SET_DRF(_PDISP, _SOR_DP_LINKCTL, _ENABLE, _YES, value);
    value = FLD_SET_DRF(_PDISP, _SOR_DP_LINKCTL, _ASYNC_FIFO_BLOCK, _DISABLE,
                        value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_DP_LINKCTL(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.linkIndex), value);

dispConfigureSorSettings_TU10X_exit:
        return;
}

/*!
 * @brief  Restore core channel
 *
 * @return FLCN_STATUS FLCN_OK if core channel restored
 */
static FLCN_STATUS
s_dispRestoreCoreChannel_TU10X(void)
{
    FLCN_STATUS status;
    LwU32       channelCtl;
    LwU32       pushBufCtl;

    // Set display owner
    status = dispSetDisplayOwner_HAL(&Disp);
    if (status != FLCN_OK)
    {
        return status;
    }

    // To restore LW_PDISP_FE_CLASSES.
    s_dispRestoreOperationalClass_TU10X();

    // Restore LW_PDISP_FE_INST_MEM0
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_INST_MEM0, Disp.pPmsBsodCtx->coreChannel.instMem0);

    // Restore LW_PDISP_FE_INST_MEM1
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_INST_MEM1, Disp.pPmsBsodCtx->coreChannel.instMem1);

    // Restore LW_PDISP_FE_SW_SOR_CAP
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_SOR_CAP(Disp.pPmsBsodCtx->head.orIndex), Disp.pPmsBsodCtx->cap.sorCap);

    // Restore LW_PDISP_FE_SW_SYS_CAP
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_SYS_CAP, Disp.pPmsBsodCtx->cap.sysCap);

    // Restore LW_PDISP_FE_SW_SYS_CAPB
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_SYS_CAPB, Disp.pPmsBsodCtx->cap.sysCapB);

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_MISC_CAPA, Disp.pPmsBsodCtx->cap.miscCapA);

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_LOCK_PIN_CAP, Disp.pPmsBsodCtx->cap.pinCapA);

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_HEAD_RG_CAPA(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.headCapA);

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_POSTCOMP_HEAD_HDR_CAPA(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.postCompHDRCapA);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_POSTCOMP_HEAD_HDR_CAPB(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.postCompHDRCapB);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_POSTCOMP_HEAD_HDR_CAPC(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.postCompHDRCapC);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_POSTCOMP_HEAD_HDR_CAPD(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.postCompHDRCapD);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_POSTCOMP_HEAD_HDR_CAPE(Disp.pPmsBsodCtx->head.headId), Disp.pPmsBsodCtx->cap.postCompHDRCapE);

    // Restore core channel address
    pushBufCtl = DRF_DEF(_PDISP, _FE_PBBASE, _PUSHBUFFER_TARGET, _PHYS_LWM) | DRF_NUM(_PDISP, _FE_PBBASE, _PUSHBUFFER_ADDR, Disp.pPmsBsodCtx->coreChannel.pushbufFbPhys4K);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBBASEHI(LW_PDISP_CHN_NUM_CORE), pushBufCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBBASE(LW_PDISP_CHN_NUM_CORE), pushBufCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBSUBDEV(LW_PDISP_CHN_NUM_CORE), DRF_NUM(_PDISP, _FE_PBSUBDEV, _SUBDEVICE_ID, Disp.pPmsBsodCtx->coreChannel.subDeviceMask));
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBCLIENT(LW_PDISP_CHN_NUM_CORE), DRF_NUM(_PDISP, _FE_PBCLIENT, _CLIENT_ID, Disp.pPmsBsodCtx->coreChannel.hClient & DRF_MASK(LW_UDISP_HASH_TBL_CLIENT_ID)));

     // Enable put pointer
    channelCtl = REG_RD32(BAR0, LW_PDISP_FE_CHNCTL_CORE);
    channelCtl = FLD_SET_DRF(_PDISP, _FE_CHNCTL_CORE, _PUTPTR_WRITE, _ENABLE, channelCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CHNCTL_CORE, channelCtl);

    // Set initial put pointer
    PMU_BAR0_WR32_SAFE(LW_UDISP_FE_CHN_ASSY_BASEADR_CORE, 0);

    // Allocate the channel and connect to push buffer
    channelCtl = (DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _ALLOCATION, _ALLOCATE) |
    DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _CONNECTION, _CONNECT) |
    DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _PUTPTR_WRITE, _ENABLE) |
    DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _EFI, _DISABLE) |
    DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _SKIP_NOTIF, _DISABLE) |
    DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _IGNORE_INTERLOCK, _DISABLE) |
    DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _TRASH_MODE, _DISABLE) |
    DRF_DEF(_PDISP, _FE_CHNCTL_CORE, _INTR_DURING_SHTDWN, _DISABLE));
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CHNCTL_CORE, channelCtl);

    // Wait for core channel to be idle
    if (!PMU_REG32_POLL_NS(LW_PDISP_FE_CHNSTATUS_CORE, DRF_SHIFTMASK(LW_PDISP_FE_CHNSTATUS_CORE_STATE),
        REF_DEF(LW_PDISP_FE_CHNSTATUS_CORE_STATE, _IDLE),
        LW_DISP_PMU_REG_POLL_PMS_TIMEOUT, PMU_REG_POLL_MODE_BAR0_EQ))
    {
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}

/*!
 * @brief  Restore display operational class
 */
static void
s_dispRestoreOperationalClass_TU10X(void)
{
    LwU32 lwPDispClass = 0;

    // Write LW_PDISP_FE_CLASS_[HW/API/CLASS]_REV

    lwPDispClass = REG_RD32(BAR0, LW_PDISP_FE_CLASSES);

    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _FE_CLASSES, _HW_REV,
        DRF_VAL(_PDISP, _FE_CLASSES, _HW_REV, Disp.pPmsBsodCtx->coreChannel.swOperationalClassSet),
                lwPDispClass);
    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _FE_CLASSES, _API_REV,
        DRF_VAL(_PDISP, _FE_CLASSES, _API_REV, Disp.pPmsBsodCtx->coreChannel.swOperationalClassSet),
                lwPDispClass);
    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _FE_CLASSES, _CLASS_REV,
        DRF_VAL(_PDISP, _FE_CLASSES, _CLASS_REV, Disp.pPmsBsodCtx->coreChannel.swOperationalClassSet),
                lwPDispClass);
    lwPDispClass = FLD_SET_DRF_NUM(_PDISP, _FE_CLASSES, _CLASS_REV,
        DRF_VAL(_PDISP, _FE_CLASSES, _CLASS_ID, Disp.pPmsBsodCtx->coreChannel.swOperationalClassSet),
        lwPDispClass);

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CLASSES, lwPDispClass);
}

/*!
 * @brief  Restore Window channel
 *
 * @return FLCN_STATUS FLCN_OK if Window channel restored
 */
static FLCN_STATUS
s_dispRestoreWindowChannel_TU10X(void)
{
    LwU32       pushBufCtl;
    LwU32       channelCtl;
    LwU32       window = 0;
    LwU32       inst;

    for (window = 0; window < RM_PMU_DISP_NUMBER_OF_WINDOWS_MAX; window++)
    {
        if (Disp.pPmsBsodCtx->windowChannel[window].hClient != 0 &&  Disp.pPmsBsodCtx->windowChannel[window].pushBufAddr4K)
        {
            // Restore Window channel address
            pushBufCtl = DRF_DEF(_PDISP, _FE_PBBASE, _PUSHBUFFER_TARGET, _PHYS_LWM) | DRF_NUM(_PDISP, _FE_PBBASE, _PUSHBUFFER_ADDR, Disp.pPmsBsodCtx->windowChannel[window].pushBufAddr4K);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBBASEHI_WIN(window), pushBufCtl);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBBASE_WIN(window), pushBufCtl);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBSUBDEV_WIN(window), DRF_NUM(_PDISP, _FE_PBSUBDEV, _SUBDEVICE_ID, Disp.pPmsBsodCtx->windowChannel[window].subDeviceMask));
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_PBCLIENT_WIN(window), DRF_NUM(_PDISP, _FE_PBCLIENT, _CLIENT_ID, Disp.pPmsBsodCtx->windowChannel[window].hClient & DRF_MASK(LW_UDISP_HASH_TBL_CLIENT_ID)));

            // Enable put pointer
            channelCtl = REG_RD32(BAR0, LW_PDISP_FE_CHNCTL_WIN(window));
            channelCtl = FLD_SET_DRF(_PDISP, _FE_CHNCTL_WIN, _PUTPTR_WRITE, _ENABLE, channelCtl);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CHNCTL_WIN(window), channelCtl);

            // Set initial put pointer
            PMU_BAR0_WR32_SAFE(LW_UDISP_FE_CHN_ASSY_BASEADR_WIN(window), 0);

            // Allocate the channel and connect to push buffer
            channelCtl = (DRF_DEF(_PDISP, _FE_CHNCTL_WIN, _ALLOCATION, _ALLOCATE) |
            DRF_DEF(_PDISP, _FE_CHNCTL_WIN, _CONNECTION, _CONNECT) |
            DRF_DEF(_PDISP, _FE_CHNCTL_WIN, _PUTPTR_WRITE, _ENABLE) |
            DRF_DEF(_PDISP, _FE_CHNCTL_WIN, _SKIP_NOTIF, _DISABLE) |
            DRF_DEF(_PDISP, _FE_CHNCTL_WIN, _IGNORE_INTERLOCK, _DISABLE) |
            DRF_DEF(_PDISP, _FE_CHNCTL_WIN, _TRASH_MODE, _DISABLE));
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_CHNCTL_WIN(window), channelCtl);

            //Wait for window channel to be idle
            if (!PMU_REG32_POLL_NS(LW_PDISP_FE_CHNSTATUS_WIN(window), DRF_SHIFTMASK(LW_PDISP_FE_CHNSTATUS_WIN_STATE),
                REF_DEF(LW_PDISP_FE_CHNSTATUS_WIN_STATE, _IDLE),
                LW_DISP_PMU_REG_POLL_PMS_TIMEOUT, PMU_REG_POLL_MODE_BAR0_EQ))
            {
                return FLCN_ERR_TIMEOUT;
            }

            inst = 0;
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_PRECOMP_WIN_PIPE_HDR_CAPA(window), Disp.pPmsBsodCtx->cap.preCompHDRCaps[inst++][window]);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_PRECOMP_WIN_PIPE_HDR_CAPB(window), Disp.pPmsBsodCtx->cap.preCompHDRCaps[inst++][window]);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_PRECOMP_WIN_PIPE_HDR_CAPC(window), Disp.pPmsBsodCtx->cap.preCompHDRCaps[inst++][window]);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_PRECOMP_WIN_PIPE_HDR_CAPD(window), Disp.pPmsBsodCtx->cap.preCompHDRCaps[inst++][window]);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_PRECOMP_WIN_PIPE_HDR_CAPE(window), Disp.pPmsBsodCtx->cap.preCompHDRCaps[inst++][window]);
            PMU_BAR0_WR32_SAFE(LW_PDISP_FE_SW_PRECOMP_WIN_PIPE_HDR_CAPF(window), Disp.pPmsBsodCtx->cap.preCompHDRCaps[inst++][window]);
        }
    }

    return FLCN_OK;

}

/*!
 * @brief  Trigger modeset and process
 *         supervisor interrupts.
 *
 * @return FLCN_OK if core channel restored
 */
static FLCN_STATUS
s_dispModeset_TU10X(void)
{
    LwU32 status;
    LwU32 winIdx;

    // Update core channel put pointer
    PMU_BAR0_WR32_SAFE(LW_UDISP_FE_PUT(LW_PDISP_CHN_NUM_CORE), Disp.pPmsBsodCtx->coreChannel.putOffset0);

    status = s_dispModesetHandleSv_TU10X();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    // Update window channel put pointer
    for (winIdx = 0; winIdx < RM_PMU_DISP_NUMBER_OF_WINDOWS_MAX; winIdx++)
    {
        if (Disp.pPmsBsodCtx->windowChannel[winIdx].putOffset != 0)
        {
            PMU_BAR0_WR32_SAFE(LW_UDISP_FE_PUT(LW_PDISP_CHN_NUM_WIN(winIdx)), Disp.pPmsBsodCtx->windowChannel[winIdx].putOffset);
        }
    }

    // Update core channel put pointer
    PMU_BAR0_WR32_SAFE(LW_UDISP_FE_PUT(LW_PDISP_CHN_NUM_CORE), Disp.pPmsBsodCtx->coreChannel.putOffset1);
    status = s_dispModesetHandleSv_TU10X();

    return status;
}

/*!
 * @brief  Enable the interrupts for the core and window channels
 *
 * @return void
 */
static void
s_dispEnableChannelInterrupts_TU10X(void)
{
    LwU32       intrEn;
    LwU32       i;

    //
    // First enable the AWAKEN and EXCEPTION interrupts.
    // Some might not be needed. To revisit when feature completed.
    //
    intrEn = REG_RD32(BAR0, LW_PDISP_FE_RM_INTR_MSK_CTRL_DISP);
    intrEn = FLD_SET_DRF(_PDISP, _FE_RM_INTR_MSK_CTRL_DISP, _AWAKEN, _ENABLE,
                         intrEn);
    intrEn = FLD_SET_DRF(_PDISP, _FE_RM_INTR_MSK_EXC_OTHER, _CORE, _ENABLE, intrEn);
    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_RM_INTR_MSK_EXC_OTHER, intrEn);

    intrEn = REG_RD32(BAR0, LW_PDISP_FE_RM_INTR_MSK_EXC_WIN);

    for (i = 0; i < RM_PMU_DISP_NUMBER_OF_WINDOWS_MAX; i++)
    {
        intrEn = FLD_IDX_SET_DRF_DEF(_PDISP, _FE_RM_INTR_MSK_EXC_WIN, _CH,
                                            i, _ENABLE, intrEn);
    }

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_RM_INTR_MSK_EXC_WIN, intrEn);
    intrEn= REG_RD32(BAR0, LW_PDISP_FE_RM_INTR_EN_CTRL_DISP);

    // Enable supervisor intrs
    intrEn = FLD_SET_DRF(_PDISP, _FE_RM_INTR_EN_CTRL_DISP, _SUPERVISOR1, _ENABLE, intrEn);
    intrEn = FLD_SET_DRF(_PDISP, _FE_RM_INTR_EN_CTRL_DISP, _SUPERVISOR2, _ENABLE, intrEn);
    intrEn = FLD_SET_DRF(_PDISP, _FE_RM_INTR_EN_CTRL_DISP, _SUPERVISOR3, _ENABLE, intrEn);

    // Enable Error Interrupts
    intrEn = FLD_SET_DRF(_PDISP, _FE_RM_INTR_EN_CTRL_DISP, _ERROR, _ENABLE, intrEn);

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_RM_INTR_EN_CTRL_DISP, intrEn);

    intrEn = REG_RD32(BAR0, LW_PDISP_FE_RM_INTR_MSK_OR);

    // Enable SOR intrs
    for (i = 0; i < LW_PDISP_FE_RM_INTR_MSK_OR_SOR__SIZE_1; ++i)
    {
        intrEn = FLD_IDX_SET_DRF(_PDISP, _FE_RM_INTR_MSK_OR, _SOR, i, _ENABLE, intrEn);
    }

    PMU_BAR0_WR32_SAFE(LW_PDISP_FE_RM_INTR_MSK_OR, intrEn);
}

/*!
 * @brief Program IMP settings prior to an MCLK switch to ensure it is glitchless
 */
FLCN_STATUS
dispProgramPreMClkSwitchImpSettings_TU10X(void)
{
    LwU32   regVal  = 0;
    LwU32   status  = FLCN_OK;

    // It'd make sense to program the settings only if display is enabled
    if (!DISP_IS_PRESENT())
    {
        return status;
    }

    //
    // Program settings for MEMFETCH_VBLANK based MCLK switch based on
    // whether it's enabled/disabled
    // This is only being done for Turing. For Ampere onwards we will use
    // modeset time programming for worst case MCLK switch time to reduce the
    // number of register writes since the worst case MCLK switch time is a
    // constant value across all modesets.
    //
    dispProgramMemFetchVblankSettings_HAL(&Disp);

#if (PMUCFG_FEATURE_ENABLED(PMU_DISP_PROG_ONE_SHOT_START_DELAY))
    {
        dispProgramOneShotModeDelays_HAL(&Disp, LW_TRUE);
    }
#endif

    // Program mempool settings required for DWCF, midframe MCLK switch.
    status = s_dispProgramMclkMempoolOverrides_TU10X(
                &(DispImpMclkSwitchSettings.mempoolNominal),
                &(DispImpMclkSwitchSettings.mempoolOverride));


    //
    // Even if mempool reduction did not complete in the given time so we can
    // attempt mclk switch as it could still be possible for mclk switch to occur.
    //

    //
    // Program settings for VRR based MCLK switch based on
    // whether it's enabled/disabled
    //
    DispContext.bMclkSwitchOwnsFrameCounter = LW_TRUE;
    if (!dispIsOneShotModeEnabled_HAL())
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_DISP_MSCG_LOW_FPS_WAR_200637376))
        {
            // Program Frame Timer to actual frame timer value
            dispProgramFrameTimerVblankMscgDisable_HAL(&Disp, LW_TRUE);
        }

        //
        // Program settings for VRR based MCLK switch based on
        // whether it's enabled/disabled
        //
        (void) dispProgramVblankForVrrExtendedFrame_HAL(&Disp);
    }

    // Enable ISOHUB MCLK switch functionality
    if (DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask != 0)
    {
        regVal = REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL);
        regVal = FLD_SET_DRF(_PDISP, _IHUB_COMMON_MISC_CTL, _SWITCH,
                             _ENABLE, regVal);
        REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL, regVal);

        //
        // Enable force switch to override critical watermark. We need to do
        // this otherwise the midframe and/or dwcf watermarks will not hit and
        // we would not get ok_to_switch signal.
        //
        regVal = REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_PMU_CTL);
        regVal = FLD_SET_DRF(_PDISP, _IHUB_COMMON_PMU_CTL, _FORCE_SWITCH,
                             _ENABLE, regVal);
        REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_PMU_CTL, regVal);
    }

    return status;
}

/*!
 * @brief     Set raster extention FP.
 *
 * @param[in] head                   head index.
 * @param[in] rasterExtension        number of lines to extend FP.
 * @param[in] bEnable                enable raster extension.
 * @param[in] bGuaranteeCompletion   Guarantee for raster extension to take in effect.
 *
 * @return FLCN_STATUS
 *      FLCN_OK if raster FP is extended.
 *      FLCN_ERR_TIMEOUT if timeout in write operation.
 *      FLCN_ERR_ILWALID_ARGUMENT invalid raster extension value.
 */
FLCN_STATUS
dispSetRasterExtendHeightFp_TU10X
(
    LwU32 head,
    LwU32 rasterExtension,
    LwU32 bEnable,
    LwU32 bGuaranteeCompletion
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 data         = 0;

    if (bGuaranteeCompletion)
    {
        // Wait for raster extension write to complete.
        if (!PMU_REG32_POLL_NS(LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH(head),
            DRF_SHIFTMASK(LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH_UPDATE),
            REF_DEF(LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH_UPDATE, _DONE),
            LW_DISP_PMU_REG_POLL_RASTER_EXTN_TIMEOUT_NS, PMU_REG_POLL_MODE_BAR0_EQ))
        {
            status = FLCN_ERR_TIMEOUT;
            goto dispSetRasterExtendHeightFp_exit;
        }
    }

    data = (bEnable) ?
        FLD_SET_DRF(_PDISP, _RG_RASTER_V_EXTEND_FRONT_PORCH, _SET_ENABLE, _YES, data) :
        FLD_SET_DRF(_PDISP, _RG_RASTER_V_EXTEND_FRONT_PORCH, _SET_ENABLE, _NO, data);

    if (rasterExtension >= LW_DISP_RG_RASTER_V_EXTEND_FRONT_PORCH_LIMIT)
    {
        // The raster extension is too large.
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto dispSetRasterExtendHeightFp_exit;
    }

    data = FLD_SET_DRF_NUM(_PDISP, _RG_RASTER_V_EXTEND_FRONT_PORCH, _SET_HEIGHT, rasterExtension, data);
    data = FLD_SET_DRF(_PDISP, _RG_RASTER_V_EXTEND_FRONT_PORCH, _UPDATE, _TRIGGER, data);
    REG_WR32(BAR0, LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH(head), data);

    if (bGuaranteeCompletion)
    {
        // Wait for raster extension write to complete.
        if (!PMU_REG32_POLL_NS(LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH(head),
            DRF_SHIFTMASK(LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH_UPDATE),
            REF_DEF(LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH_UPDATE, _DONE),
            LW_DISP_PMU_REG_POLL_RASTER_EXTN_TIMEOUT_NS, PMU_REG_POLL_MODE_BAR0_EQ))
        {
            status = FLCN_ERR_TIMEOUT;
            goto dispSetRasterExtendHeightFp_exit;
        }

        // Wait for RG to be in active region to ensure vblank extension has taken effect in HW.
        if (!PMU_REG32_POLL_NS(LW_PDISP_RG_STATUS(head),
            DRF_SHIFTMASK(LW_PDISP_RG_STATUS_VBLNK),
            REF_DEF(LW_PDISP_RG_STATUS_VBLNK, _INACTIVE),
            LW_DISP_PMU_REG_POLL_RG_STATUS_VBLNK_TIMEOUT_NS, PMU_REG_POLL_MODE_BAR0_EQ))
        {
            status = FLCN_ERR_TIMEOUT;
            goto dispSetRasterExtendHeightFp_exit;
        }
    }

dispSetRasterExtendHeightFp_exit:
    return status;
}

/*!
 * @brief Override mempool before MCLK switch so that MCLK switch can be
 *        possible using midframe + (DWCF/memfetch vblank).
 *
 * @param[in]   pMempoolLwrr   Current mempool settings for windows/lwrsors
 *                             which would be overridden.
 * @param[in]   pMempoolNew    Mempool settings for windows/lwrsors to which we
 *                             need to  override to.
 *
 * @return      FLCN_OK             On success
 * @return      FLCN_ERR_TIMEOUT    If mempool has not changed within the
 *                                  timeout period.
 */
static FLCN_STATUS
s_dispProgramMclkMempoolOverrides_TU10X
(
    RM_PMU_DISP_IMP_MCLK_SWITCH_MEMPOOL *pMempoolLwrr,
    RM_PMU_DISP_IMP_MCLK_SWITCH_MEMPOOL *pMempoolNew
)
{
    LwBool          bMempoolReductionsDone = LW_FALSE;
    LwU8            window;
    LwU8            cursor;
    LwU16          *pMempoolLwrrWin;
    LwU16          *pMempoolNewWin;
    LwU16          *pMempoolLwrrLwrs;
    LwU16          *pMempoolNewLwrs;
    LwU32           regVal;
    LwU32           elapsedTimeNs          = 0;
    FLCN_TIMESTAMP  lwrrentTimeNs;

    if (!DispImpMclkSwitchSettings.bOverrideMempool)
    {
        return FLCN_OK;
    }

    pMempoolLwrrWin  = pMempoolLwrr->window;
    pMempoolNewWin   = pMempoolNew->window;
    pMempoolLwrrLwrs = pMempoolLwrr->cursor;
    pMempoolNewLwrs  = pMempoolNew->cursor;

    // Change decreasing mempool first.
    for (window = 0; window < DispImpMclkSwitchSettings.numWindows; window++)
    {
        if (pMempoolLwrrWin[window] > pMempoolNewWin[window])
        {
            regVal = DRF_NUM(_PDISP, _IHUB_WINDOW_POOL_CONFIG, _ENTRIES,
                             pMempoolNewWin[window]) |
                     DRF_DEF(_PDISP, _IHUB_WINDOW_POOL_CONFIG, _UPDATE, _GLOBAL);
            REG_WR32(BAR0,
                     LW_PDISP_IHUB_WINDOW_POOL_CONFIG(window),
                     regVal);
        }
    }

    for (cursor = 0; cursor < Disp.numHeads; cursor++)
    {
        if (pMempoolLwrrLwrs[cursor] > pMempoolNewLwrs[cursor])
        {
            regVal = DRF_NUM(_PDISP, _IHUB_LWRS_POOL_CONFIG, _ENTRIES,
                             pMempoolNewLwrs[cursor]) |
                     DRF_DEF(_PDISP, _IHUB_LWRS_POOL_CONFIG, _UPDATE, _GLOBAL);
            REG_WR32(BAR0,
                     LW_PDISP_IHUB_LWRS_POOL_CONFIG(cursor),
                     regVal);
        }
    }

    // Trigger the mempool allocation reductions to execute immediately.
    regVal = DRF_DEF(_PDISP, _IHUB_COMMON_CONFIG_CONTROL, _MODE, _IMMEDIATE) |
             DRF_DEF(_PDISP, _IHUB_COMMON_CONFIG_CONTROL, _UPDATE, _TRIGGER);
    REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_CONFIG_CONTROL, regVal);

    //
    // Wait for the mempool allocation reductions to take effect.  (Pixels may
    // need to be scanned out to reduce oclwpancy before the allocation can be
    // reduced, so it may take some time.)
    //
    osPTimerTimeNsLwrrentGet(&lwrrentTimeNs);

    do
    {
        bMempoolReductionsDone = LW_TRUE;

        for (window = 0; window < DispImpMclkSwitchSettings.numWindows; window++)
        {
            regVal = REG_RD32(BAR0, LW_PDISP_IHUB_WINDOW_POOL_CONFIG(window));

            if (!FLD_TEST_DRF(_PDISP, _IHUB_WINDOW_POOL_CONFIG, _STATUS, _DONE, regVal))
            {
                bMempoolReductionsDone = LW_FALSE;
                break;
            }
        }

        for (cursor = 0; cursor < Disp.numHeads; cursor++)
        {
            regVal = REG_RD32(BAR0, LW_PDISP_IHUB_LWRS_POOL_CONFIG(cursor));

            if (!FLD_TEST_DRF(_PDISP, _IHUB_LWRS_POOL_CONFIG, _STATUS, _DONE, regVal))
            {
                bMempoolReductionsDone = LW_FALSE;
                break;
            }
        }


        // Timeout value is 100 milliseconds
        if (bMempoolReductionsDone ||
            (elapsedTimeNs > (100000000)))
        {
            break;
        }

        lwrtosYIELD();
        elapsedTimeNs = osPTimerTimeNsElapsedGet(&lwrrentTimeNs);
    } while (LW_TRUE);

    //
    // Now that the mempool allocation reductions have completed, it should be
    // safe to do the mempool allocation increases.
    //
    for (window = 0; window < DispImpMclkSwitchSettings.numWindows; window++)
    {
        if (pMempoolLwrrWin[window] < pMempoolNewWin[window])
        {
            regVal = DRF_NUM(_PDISP, _IHUB_WINDOW_POOL_CONFIG, _ENTRIES,
                             pMempoolNewWin[window]) |
                     DRF_DEF(_PDISP, _IHUB_WINDOW_POOL_CONFIG, _UPDATE, _GLOBAL);
            REG_WR32(BAR0,
                     LW_PDISP_IHUB_WINDOW_POOL_CONFIG(window),
                     regVal);
        }
    }

    for (cursor = 0; cursor < Disp.numHeads; cursor++)
    {
        if (pMempoolLwrrLwrs[cursor] < pMempoolNewLwrs[cursor])
        {
            regVal = DRF_NUM(_PDISP, _IHUB_LWRS_POOL_CONFIG, _ENTRIES,
                             pMempoolNewLwrs[cursor]) |
                     DRF_DEF(_PDISP, _IHUB_LWRS_POOL_CONFIG, _UPDATE, _GLOBAL);
            REG_WR32(BAR0,
                     LW_PDISP_IHUB_LWRS_POOL_CONFIG(cursor),
                     regVal);
        }
    }

    regVal = DRF_DEF(_PDISP, _IHUB_COMMON_CONFIG_CONTROL, _MODE, _IMMEDIATE) |
             DRF_DEF(_PDISP, _IHUB_COMMON_CONFIG_CONTROL, _UPDATE, _TRIGGER);
    REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_CONFIG_CONTROL, regVal);

    if (!bMempoolReductionsDone)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_TIMEOUT;
    }

    return FLCN_OK;
}

/*!
 * @brief Restore the IMP settings programmed in @dispProgramPreMClkSwitchImpSettings
 *        post an MCLK switch.
 */
FLCN_STATUS
dispProgramPostMClkSwitchImpSettings_TU10X(void)
{
    LwU32   regVal  = 0;
    LwU32   status  = FLCN_OK;

    // Restore the settings only if display is enabled
    if (!DISP_IS_PRESENT())
    {
        return status;
    }

    // Restore the modeset mempool values.
    status = s_dispProgramMclkMempoolOverrides_TU10X(
                &(DispImpMclkSwitchSettings.mempoolOverride),
                &(DispImpMclkSwitchSettings.mempoolNominal));

#if (PMUCFG_FEATURE_ENABLED(PMU_DISP_PROG_ONE_SHOT_START_DELAY))
    {
        dispProgramOneShotModeDelays_HAL(&Disp, LW_FALSE);
    }
#endif

    // Disable ISOHUB MCLK switch functionality
    if (DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask != 0)
    {
        regVal = REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL);
        regVal = FLD_SET_DRF(_PDISP, _IHUB_COMMON_MISC_CTL, _SWITCH, _DISABLE,
                             regVal);
        REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL, regVal);

        //
        // Need to disable force switch so that critical watermark can be
        // enabled.
        //
        regVal = REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_PMU_CTL);
        regVal = FLD_SET_DRF(_PDISP, _IHUB_COMMON_PMU_CTL, _FORCE_SWITCH,
                             _DISABLE, regVal);
        REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_PMU_CTL, regVal);
    }

    // Restore VRR Based Mclk Switch values.
    if (!dispIsOneShotModeEnabled_HAL())
    {
        (void) dispRestoreVblankForVrrExtendedFrame_HAL(&Disp);

        if (PMUCFG_FEATURE_ENABLED(PMU_DISP_MSCG_LOW_FPS_WAR_200637376))
        {
            // Program Frame Timer to disable VBlank MSCG
            dispProgramFrameTimerVblankMscgDisable_HAL(&Disp, LW_FALSE);
        }
    }
    DispContext.bMclkSwitchOwnsFrameCounter = LW_FALSE;

    return status;
}

/*!
 * @brief  Check if one shot mode is enabled at least on one head
 *
 * @return true if one shot mode is enabled
 */
LwBool
dispIsOneShotModeEnabled_TU10X(void)
{
    LwU32  runMode;
    LwU32  stallLock;
    LwU32  headIdx;

    for (headIdx = 0U; headIdx < Disp.numHeads; headIdx++)
    {
        if (DispImpMclkSwitchSettings.bActiveHead[headIdx])
        {
            // Read the STALL LOCK register for stall lock status
            stallLock = REG_RD32(FECS, LW_UDISP_FE_CHN_ARMED_BASEADR(LW_PDISP_CHN_NUM_CORE) +
                                 LWC57D_HEAD_SET_STALL_LOCK(headIdx));

            // Read the DISPLAY RATE register for display rate mode
            runMode = REG_RD32(FECS, LW_UDISP_FE_CHN_ARMED_BASEADR(LW_PDISP_CHN_NUM_CORE) +
                               LWC57D_HEAD_SET_DISPLAY_RATE(headIdx));

            if (FLD_TEST_DRF(C57D, _HEAD_SET_STALL_LOCK,   _ENABLE,   _TRUE,     stallLock) &&
                FLD_TEST_DRF(C57D, _HEAD_SET_STALL_LOCK,   _MODE,     _ONE_SHOT, stallLock) &&
                FLD_TEST_DRF(C57D, _HEAD_SET_DISPLAY_RATE, _RUN_MODE, _ONE_SHOT, runMode))
            {
                return LW_TRUE;
            }
        }
    }
    return LW_FALSE;
}

/*!
 * @brief  Returns if notebook Gsync is supported on this GPU
 *
 * @return LW_TRUE if the GPU supports Notebook DirectDrive, LW_FALSE otherwise
 */
LwBool
dispIsNotebookGsyncSupported_TU10X(void)
{
    LwU32 gpuDevIdObf;
    LwU32 fuseVal;

    //
    // TODO : to add support for HULK license for bug#2275289
    // Use Secure scratch register instead of BRSS
    //

    // Check fuse BIT_INTERNAL_USE_ONLY
    fuseVal = fuseRead(LW_FUSE_OPT_INTERNAL_SKU);
    if (FLD_TEST_DRF_NUM(_FUSE, _OPT_INTERNAL_SKU, _DATA, 0x1, fuseVal))
    {
        return LW_TRUE;
    }

#define DEVID_OBFUSCATE(d) ((d) * 2 - 3)

    gpuDevIdObf = DEVID_OBFUSCATE(Pmu.gpuDevId);
    //
    // Check if the GPU is in the approved list for Notebook directDrive
    // Obfuscated so it has less chance to appear in clear in binary.
    //
    return ((gpuDevIdObf == DEVID_OBFUSCATE(0x1ED0)) || // TU104
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1EEC)) || // TU104
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1F50)) || // TU106
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1F51)) || // TU106
        (gpuDevIdObf == DEVID_OBFUSCATE(0x1E50)));  // Ace
}

/*!
 * @brief  Restore Postcomp scaler coefficients
 *
 * @return void
 */
static void
s_dispRestorePostcompScalerCoefficients_TU10X(void)
{
    LwU32 regVal;
    LwU32 idx;

    // restore post comp scaler coefficients for LWSR head
    for (idx = 0; idx < MAX_POSTCOMP_SCALER_COEFF; idx++)
    {
        regVal = Disp.pPmsBsodCtx->cap.postcompScalerCoeff[Disp.pPmsBsodCtx->head.headId][idx];
        regVal = FLD_SET_DRF_NUM(_PDISP, _POSTCOMP_HEAD_SCALER_SET_COEFF_VALUE, _INDEX, idx, regVal);
        // SET_READ_INDEX to 0 , so that specified coefficient will be written
        regVal = FLD_SET_DRF_NUM(_PDISP, _POSTCOMP_HEAD_SCALER_SET_COEFF_VALUE, _SET_READ_INDEX, 0x0, regVal);
        PMU_BAR0_WR32_SAFE(LW_PDISP_POSTCOMP_HEAD_SCALER_SET_COEFF_VALUE(Disp.pPmsBsodCtx->head.headId),regVal);
    }
}

/*!
 * @brief  Program One shot Mode delays (RG unstall spoolup and One shot
 * start delay) during MCLK switch
 *
 * @param[in] bPreMclk    Tells whether it is premclk switch or post mclk switch call
 *
 * @return void
 */
void
dispProgramOneShotModeDelays_TU10X(LwBool bPreMclk)
{
    LwU8  headIdx;
    LwU8  activeHeadIdx = RM_PMU_DISP_NUMBER_OF_HEADS_MAX;
    LwU32 oneShotStartDelayUs;
    LwU32 rgUnstallForMclk;
    LwU32 lwrrentVal;
    LwU32 headCount = 0U;

    for (headIdx = 0U; headIdx < Disp.numHeads; headIdx++)
    {
        if (DispImpMclkSwitchSettings.bActiveHead[headIdx])
        {
            headCount++;
            activeHeadIdx = headIdx;
        }
    }

    // Program one shot mode delays if there is only single head
    if ((headCount == 1U) && (activeHeadIdx != RM_PMU_DISP_NUMBER_OF_HEADS_MAX))
    {
        lwrrentVal = DRF_VAL(_PDISP, _FE_ONE_SHOT_START_DELAY, _VALUE,
                             REG_RD32(BAR0, LW_PDISP_FE_ONE_SHOT_START_DELAY(activeHeadIdx)));
        if (bPreMclk)
        {
            oneShotStartDelayUs = Disp.DispImpOsmMclkSwitchSettings.oneShotStartDelayUs;
            rgUnstallForMclk = Disp.DispImpOsmMclkSwitchSettings.rgUnstallForMclk[activeHeadIdx];
        }
        else
        {
            oneShotStartDelayUs = Disp.DispImpOsmMclkSwitchSettings.oneShotStartDelayUsForNonMclk[activeHeadIdx];
            rgUnstallForMclk = Disp.DispImpOsmMclkSwitchSettings.rgUnstallForNonMclk[activeHeadIdx];
        }

        if (oneShotStartDelayUs <= lwrrentVal)
        {
            oneShotStartDelayUs =
                DRF_NUM(_PDISP, _FE_ONE_SHOT_START_DELAY, _VALUE, oneShotStartDelayUs) |
                DRF_DEF(_PDISP, _FE_ONE_SHOT_START_DELAY, _WRITE_MODE, _ACTIVE) |
                DRF_DEF(_PDISP, _FE_ONE_SHOT_START_DELAY, _UPDATE, _IMMEDIATE);
            REG_WR32(BAR0, LW_PDISP_FE_ONE_SHOT_START_DELAY(activeHeadIdx), oneShotStartDelayUs);

            rgUnstallForMclk =
                DRF_NUM(_PDISP, _RG_UNSTALL_SPOOLUP, _VALUE, rgUnstallForMclk) |
                DRF_DEF(_PDISP, _RG_UNSTALL_SPOOLUP, _WRITE_MODE, _ASSEMBLY) |
                DRF_DEF(_PDISP, _RG_UNSTALL_SPOOLUP, _UPDATE, _IMMEDIATE);
            REG_WR32(BAR0, LW_PDISP_RG_UNSTALL_SPOOLUP(activeHeadIdx), rgUnstallForMclk);
        }
        else
        {
            rgUnstallForMclk =
                DRF_NUM(_PDISP, _RG_UNSTALL_SPOOLUP, _VALUE, rgUnstallForMclk) |
                DRF_DEF(_PDISP, _RG_UNSTALL_SPOOLUP, _WRITE_MODE, _ACTIVE) |
                DRF_DEF(_PDISP, _RG_UNSTALL_SPOOLUP, _UPDATE, _IMMEDIATE);
            REG_WR32(BAR0, LW_PDISP_RG_UNSTALL_SPOOLUP(activeHeadIdx), rgUnstallForMclk);

            oneShotStartDelayUs =
                DRF_NUM(_PDISP, _FE_ONE_SHOT_START_DELAY, _VALUE, oneShotStartDelayUs) |
                DRF_DEF(_PDISP, _FE_ONE_SHOT_START_DELAY, _WRITE_MODE, _ASSEMBLY) |
                DRF_DEF(_PDISP, _FE_ONE_SHOT_START_DELAY, _UPDATE, _IMMEDIATE);
            REG_WR32(BAR0, LW_PDISP_FE_ONE_SHOT_START_DELAY(activeHeadIdx), oneShotStartDelayUs);
        }
    }
}
