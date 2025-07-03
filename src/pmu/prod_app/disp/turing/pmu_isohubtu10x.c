/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_isohubtu10x.c
 * @brief  HAL-interface for the TU10X Display Engine.
 */

#include "pmusw.h"
#include "pmu/ssurface.h"

#include "dev_disp.h"
#include "pmu_objdisp.h"
#include "pmu_objlpwr.h"
#include "pmu_objperf.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/*!
 * @brief  Program the MSCG/DIFR watermarks (provided by RM) for the current pState.
 *         Both MSCG and DIFR use MSCG watermark registers with separate watermarks
 *         callwlated by RM.
 *  
 * @returns FLCN_STATUS propagated from the DMA operation used to get the 
 *          watermarks from RM 
 */
FLCN_STATUS
isohubProgramMscgWatermarks_TU10X()
{
    FLCN_STATUS status;
    LwU32 miscConfigA;
    LwU32 maxLwr;
    LwU32 maxWindows;
    LwU32 winIdx;
    LwU32 lwrIdx;
    LwU32 regVal;
    LwU32 pState;
    RM_PMU_DISP_IMP_PSTATE_SETTINGS pstateSettings;
    RM_PMU_DISP_PSTATE_MSCG_SETTINGS *pPstateMscgSettings;

    if ((Disp.bUseMscgWatermarks && !PMUCFG_FEATURE_ENABLED(PMU_PG_MS)) ||
        (!Disp.bUseMscgWatermarks && !PMUCFG_FEATURE_ENABLED(PMU_LPWR_DIFR)) ||
        !DISP_IS_PRESENT())
    {
        return FLCN_OK;
    }

    miscConfigA = REG_RD32(BAR0, LW_PDISP_FE_MISC_CONFIGA);
    maxLwr      = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_HEADS, miscConfigA);
    maxWindows  = DRF_VAL(_PDISP, _FE_MISC_CONFIGA, _NUM_WINDOWS, miscConfigA);

    // Attach required overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Get the current pstate.  If there are no pstates, RM will put the IMP
        // data in the first slot, for pstate level 0.
        //
        if (BOARDOBJGRP_IS_EMPTY(PSTATE))
        {
            pState = 0;
        }
        else
        {
            pState = perfPstateGetLevelByIndex(Lpwr.targetPerfIdx);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Read the MSCG and DIFR watermarks for this pstate.
    status =
        SSURFACE_RD(&pstateSettings,
            turingAndLater.disp.dispImpWatermarks.data.pstates[pState]);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    //
    // Skip watermark programming for MSCG or DIFR (whichever is selected)
    // if it is not enabled for the current perf level
    //
    if ((Disp.bUseMscgWatermarks && !pstateSettings.mscgSettings.bIsAllowed) ||
        (!Disp.bUseMscgWatermarks && !pstateSettings.difrSettings.bIsAllowed))
    {
        return FLCN_OK;
    }

    if (Disp.bUseMscgWatermarks)
    {
        pPstateMscgSettings = &(pstateSettings.mscgSettings);
    }
    else
    {
        pPstateMscgSettings = &(pstateSettings.difrSettings);
    }

    //
    // Use the smaller of the number of windows the HW supports, and the number
    // of windows in the array from RM, in case they are different.
    //
    maxWindows =
        LW_MIN(maxWindows,
               RM_PMU_SUPER_SURFACE_MEMBER_SIZE(turingAndLater.disp.dispImpWatermarks.data.pstates[0].mscgSettings.window) /
               RM_PMU_SUPER_SURFACE_MEMBER_SIZE(turingAndLater.disp.dispImpWatermarks.data.pstates[0].mscgSettings.window[0]));

    // Program the window MSCG/DIFR watermarks.
    for (winIdx = 0; winIdx < maxWindows; winIdx++)
    {
        if (pPstateMscgSettings->window[winIdx].bEnable)
        {
            regVal =
                DRF_NUM(_PDISP, _IHUB_WINDOW_MSCG_CTLB, _MSCG_POWERUP_WATERMARK,
                        pPstateMscgSettings->window[winIdx].wakeupWatermark);
            REG_WR32(BAR0, LW_PDISP_IHUB_WINDOW_MSCG_CTLB(winIdx), regVal);

            regVal =
                DRF_NUM(_PDISP, _IHUB_WINDOW_MSCG_CTLC, _MSCG_HIGH_WATERMARK,
                        pPstateMscgSettings->window[winIdx].highWatermark);
            REG_WR32(BAR0, LW_PDISP_IHUB_WINDOW_MSCG_CTLC(winIdx), regVal);

            regVal =
                DRF_DEF(_PDISP, _IHUB_WINDOW_MSCG_CTLA, _MODE, _ENABLE) |
                DRF_NUM(_PDISP, _IHUB_WINDOW_MSCG_CTLA, _MSCG_LOW_WATERMARK,
                        pPstateMscgSettings->window[winIdx].lowWatermark);
        }
        else
        {
            regVal = DRF_DEF(_PDISP, _IHUB_WINDOW_MSCG_CTLA, _MODE, _IGNORE);
        }
        REG_WR32(BAR0, LW_PDISP_IHUB_WINDOW_MSCG_CTLA(winIdx), regVal);
    }

    //
    // Use the smaller of the number of lwrsors the HW supports, and the number
    // of lwrsors in the array from RM, in case they are different.
    //
    maxLwr =
        LW_MIN(maxLwr,
               RM_PMU_SUPER_SURFACE_MEMBER_SIZE(turingAndLater.disp.dispImpWatermarks.data.pstates[0].mscgSettings.cursor) /
               RM_PMU_SUPER_SURFACE_MEMBER_SIZE(turingAndLater.disp.dispImpWatermarks.data.pstates[0].mscgSettings.cursor[0]));

    // Program the cursor MSCG/DIFR watermarks.
    for (lwrIdx = 0; lwrIdx < maxLwr; lwrIdx++)
    {
        if (pPstateMscgSettings->cursor[lwrIdx].bEnable)
        {
            regVal =
                DRF_NUM(_PDISP, _IHUB_LWRS_MSCG_CTLB, _MSCG_POWERUP_WATERMARK,
                        pPstateMscgSettings->cursor[lwrIdx].wakeupWatermark);
            REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_MSCG_CTLB(lwrIdx), regVal);

            regVal =
                DRF_NUM(_PDISP, _IHUB_LWRS_MSCG_CTLC, _MSCG_HIGH_WATERMARK,
                        pPstateMscgSettings->cursor[lwrIdx].highWatermark);
            REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_MSCG_CTLC(lwrIdx), regVal);

            regVal = DRF_DEF(_PDISP, _IHUB_LWRS_MSCG_CTLA, _MODE, _ENABLE) |
                     DRF_NUM(_PDISP, _IHUB_LWRS_MSCG_CTLA, _MSCG_LOW_WATERMARK,
                             pPstateMscgSettings->cursor[lwrIdx].lowWatermark);
        }
        else
        {
            regVal = DRF_DEF(_PDISP, _IHUB_LWRS_MSCG_CTLA, _MODE, _IGNORE);
        }
        REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_MSCG_CTLA(lwrIdx), regVal);
    }

    isohubProgramMscgCycleAndExitTimeUs_HAL(&Disp, pPstateMscgSettings);
    isohubEnableOsmVblankMscg_HAL(&Disp, pPstateMscgSettings, maxWindows, maxLwr, LW_FALSE);

    return FLCN_OK;
}

/*!
 * @brief Enables or disables MSCG from IHUB
 *
 * If bEnable is false, isohubMscgEnableDisableIsr disables MSCG (if it is not
 * already disabled) and records the specified disable reason.  bEnable may 
 * then be set to true in a subsequent call to reenable MSCG, if the same 
 * disable reason is specified. 
 *  
 * If multiple disable reasons are set, MSCG will not be enabled until all of 
 * them are cleared. 
 *  
 * Each specific disable reason may be cleared with a single enable call, even 
 * if that same disable reason was specified in multiple disable calls. 
 * 
 * This function is the same as isohubMscgEnableDisable except that it does not
 * use a critical section.  The caller must insure that it is not called in a
 * reentrant manner.  Also, this function is in resident code.
 *
 * @param[in] bEnable               Enable or disable
 * @param[in] mscgDisableReasons    RM_PMU_DISP_MSCG_DISABLE_REASON_xxx bit
 */
void
isohubMscgEnableDisableIsr_TU10X
(
    const LwBool bEnable,
    const LwU32 mscgDisableReasons
)
{
    LwU32   regVal;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PG_MS))
    {
        return;
    }

    if (bEnable)
    {
        Disp.mscgDisableReasons &= ~mscgDisableReasons;
        if (Disp.mscgDisableReasons == 0)
        {
            regVal = REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL);
            regVal = FLD_SET_DRF(_PDISP, _IHUB_COMMON_MISC_CTL, _MSCG, _ENABLE, regVal);
            REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL, regVal);
        }
    }
    else
    {
        if (Disp.mscgDisableReasons == 0)
        {
            regVal = REG_RD32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL);
            regVal = FLD_SET_DRF(_PDISP, _IHUB_COMMON_MISC_CTL, _MSCG, _DISABLE, regVal);
            REG_WR32(BAR0, LW_PDISP_IHUB_COMMON_MISC_CTL, regVal);
        }
        Disp.mscgDisableReasons |= mscgDisableReasons;
    }
}

/*!
 * @brief Enables or disables MSCG from IHUB
 * 
 * If bEnable is false, isohubMscgEnableDisable disables MSCG (if it is not 
 * already disabled) and records the specified disable reason.  bEnable may 
 * then be set to true in a subsequent call to reenable MSCG, if the same 
 * disable reason is specified. 
 *  
 * If multiple disable reasons are set, MSCG will not be enabled until all of 
 * them are cleared. 
 *  
 * Each specific disable reason may be cleared with a single enable call, even 
 * if that same disable reason was specified in multiple disable calls. 
 *  
 * @param[in] bEnable               Enable or disable
 * @param[in] mscgDisableReasons    RM_PMU_DISP_MSCG_DISABLE_REASON_xxx bit
 */
void
isohubMscgEnableDisable_TU10X
(
    const LwBool bEnable,
    const LwU32 mscgDisableReasons
)
{
    if (!PMUCFG_FEATURE_ENABLED(PMU_PG_MS) ||
        !DISP_IS_PRESENT())
    {
        return;
    }

    appTaskCriticalEnter();
    {
        isohubMscgEnableDisableIsr_HAL(&Disp, bEnable, mscgDisableReasons);
    }
    appTaskCriticalExit();
}
