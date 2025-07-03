/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispga10xonly.c
 * @brief  HAL-interface for the GA10x Display Engine. 
 *  
 * Functions in this file are expected to be used on GA10X only, i.e., not
 * GA10X and later chips.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objdisp.h"
#include "pmu_disp.h"
#include "main.h"
#include "config/g_disp_private.h"

#if (PMU_PROFILE_GA10X_RISCV)
#include <lwriscv/print.h>
#endif
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_DISP_PMU_WORST_ISR_SERVICE_LATENCY_US            (20U) // 20 us

/* ---------------------------Public functions ------------------------------ */

/*!
 * @brief  Program Frame Timer to disable VBlank MSCG.
 *
 * @param[in] bPreMclk    Tells whether it is premclk switch or post mclk switch call
 *
 * @return void
 */
void
dispProgramFrameTimerVblankMscgDisable_GA10X(LwBool bPreMclk)
{
    LwU8  headIdx;

    if (!DISP_IS_PRESENT() ||
        !Disp.rmModesetData.bDisableVBlankMSCG ||
        !PMUCFG_FEATURE_ENABLED(PMU_DISP_MSCG_LOW_FPS_WAR_200637376))
    {
        return;
    }

    for (headIdx = 0U; headIdx < Disp.numHeads; headIdx++)
    {
        if (DispImpMclkSwitchSettings.bActiveHead[headIdx])
        {
            //
            // MSCG is allowed for a single head or a single 2H1OR stream. For
            // 2H1OR, VBlank MSCG WAR needs to be applied for primary head and
            // primary head would always be lower index than secondary head so
            // break as soon as we find active head
            //
            break;
        }
    }

    if (headIdx < Disp.numHeads)
    {
        if (bPreMclk)
        {
            REG_WR32(BAR0,
                     LW_PDISP_IHUB_LWRS_FRAME_TIME(headIdx),
                     Disp.rmModesetData.frameTimeActualUs);

            //
            // Frame Timer update will happen on next START_FETCH. FB
            // Falcon will wait for one additional frame for mclk_ok_to_switch
            // to be asserted
            //
        }
        else
        {
            REG_WR32(BAR0,
                     LW_PDISP_IHUB_LWRS_FRAME_TIME(headIdx),
                     DispContext.frameTimeVblankMscgDisableUs);
        }
    }
}

/*!
 * @brief  Program frame counter to wakeup MSCG earlier
 *
 * On Ampere, it was observed that the reading of the end-of-frame notifier
 * was delayed because MSCG was engaged at the time of the read attempt.  The
 * read attempt triggered a wakeup, but the MSCG exit delay pushed the reading
 * past the end of the frame, so the notifier was counted for the next frame
 * instead of the current frame.  This reduced FPS perf by 50%.  (Bug
 * 200637376.)
 *
 * To work around the problem, the frame time programmed in
 * LW_PDISP_IHUB_LWRS_FRAME_TIME is being reduced, so that MSCG will wake up
 * earlier.  Ideally, we would like to be awake by the time the last data for
 * the frame is released to the precomp pipe (i.e., when the LAST_DATA
 * interrupt fires), but it is diffilwlt for RM/PMU to callwlate this, because
 * it depends on the frame's blanking time, and that depends on the window and
 * cursor sizes, positions, and scaling ratios.
 *
 * Instead, PMU records the line number when the LAST_DATA interrupt fires,
 * and uses that to callwlate the frame counter for the desired MSCG exit time
 * for the NEXT frame.  This is acceptable because it is very rare for the
 * LAST_DATA line number to change between frames, and if it does change, it
 * will only inlwr a power or perf loss for that one frame.
 */
void
dispProcessLastData_GA10X(void)
{
#if PMUCFG_FEATURE_ENABLED(PMU_DISP_IMP_INFRA)
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispImp)
    };

    if (!PMUCFG_FEATURE_ENABLED(PMU_PG_MS) ||
        !PMUCFG_FEATURE_ENABLED(PMU_DISP_MSCG_LOW_FPS_WAR_200637376) ||
        !DISP_IS_PRESENT())
    {
        return;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    LwU32 lineComponents;
    LwU32 lineComponentsUs;
    LwU32 adjFrameTimeSettingUs;
    LwU32 subtractionsUs;

    if (Disp.rmModesetData.bEnableMscg &&
        Disp.rmModesetData.bDisableVBlankMSCG &&
        (DispContext.lastDataLineCnt != 0U) &&
        !DispContext.bMclkSwitchOwnsFrameCounter)
    {
        //
        // The adjusted frame time is equal to:
        //   LW_MIN(LAST_DATA + ELV_START - START_FETCH_DELAY, originalFrameTime)
        //
        // Start with LAST_DATA and add ELV_START.
        //
        lineComponents = DispContext.lastDataLineCnt +
                         Disp.rmModesetData.mscgHeadElvStartLines;
    
        // Colwert lines to microseconds.
        lineComponentsUs = (LwU32) ((LwU64) lineComponents *
                                    Disp.rmModesetData.mscgRasterLineTimeNs /
                                    1000ULL);
        //
        // Subtract START_FETCH_DELAY and worst case PMU ISR service latency.
        // We need to compensate for worst case PMU ISR service latency for delay
        // between interrupt raised to handler reading RG lines count.
        // (Make sure it doesn't go negative.)
        //
        subtractionsUs = Disp.rmModesetData.mscgHeadStartFetchDelayUs + 
                         LW_DISP_PMU_WORST_ISR_SERVICE_LATENCY_US;

        adjFrameTimeSettingUs = (lineComponentsUs > subtractionsUs) ?
                                (lineComponentsUs - subtractionsUs) :
                                0U;

        //
        // Make sure we haven't come up with a result that is somehow larger
        // than the original frame time.
        //
        adjFrameTimeSettingUs =
            LW_MIN(adjFrameTimeSettingUs,
                   Disp.rmModesetData.frameTimeActualUs);

        // Program new frame timer value only when it is different than previous
        if (DispContext.frameTimeVblankMscgDisableUs !=
            adjFrameTimeSettingUs)
        {
            DispContext.frameTimeVblankMscgDisableUs =
                adjFrameTimeSettingUs;

            // Set Frame Timer value as per new callwlations for non-VRR head only
            if (!dispIsOneShotModeEnabled_HAL())
            {
                REG_WR32(BAR0,
                        LW_PDISP_IHUB_LWRS_FRAME_TIME(Disp.rmModesetData.mscgWarHeadIndex),
                        DispContext.frameTimeVblankMscgDisableUs);
            }
        }
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
#endif
}

/*!
 * @brief       Restore VPLLs
 * @see         bug 2440060
 *
 * @details     The order of these register writes is important.
 *
 *              These data are captured on the RM side in clkCaptureParamsForPmsOnBsod.
 *
 *              This implementation assumes the hybrid tsmc7 PLL used in Ampere
 *              and later.  Starting with Ampere, VPLLs are tsmc7 cells (HPLL),
 *              which do not have all of the same registers as the older tsmc16ffb.
 *              See bug 2440060: GA10X: Moving PLLs from tsmc16ffb to tsmc7 cells
 */
void
dispPmsModeSetRestoreVpll_GA10X(LwU32 headId)
{
    REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_CFG3(headId),  Disp.pPmsBsodCtx->vplls.regs.hybrid.cfg3);
    REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_CFG4(headId),  Disp.pPmsBsodCtx->vplls.regs.hybrid.cfg4);
    REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_CFG(headId),   Disp.pPmsBsodCtx->vplls.cfg);
    REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_COEFF(headId), Disp.pPmsBsodCtx->vplls.coeff);
}

