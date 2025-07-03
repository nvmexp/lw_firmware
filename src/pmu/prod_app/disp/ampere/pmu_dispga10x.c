/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispga10x.c
 * @brief  HAL-interface for the GA10x Display Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_disp.h"
#include "pmusw.h"
#include "lwosreg.h"
#include "lib/lib_pwm.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_disp.h"  /// XXXEB RECHECK LIST HERE
#include "pmu_objseq.h"
#include "pmu_objvbios.h"
#include "lw_ref_dev_sr.h"
#include "pmu_i2capi.h"
#include "task_i2c.h"
#include "dev_pmgr.h"
#include "dev_trim.h"
#include "main.h"
#include "config/g_disp_private.h"
#include "pmu_objdisp.h"
#include "displayport/dpcd.h"

/* ---------------------------Public functions ------------------------------ */

/*!
 * @brief  Run No link training
 *
 * @return FLCN_STATUS FLCN_OK if link training completed
 */
FLCN_STATUS
dispRunDpNoLinkTraining_GA10X(void)
{
    LwU8            dpAuxData;
    FLCN_STATUS     status;
    FLCN_TIMESTAMP  lwrTimeStamp;
    LwU32           value;
    LwU32           laneCount;
    LwU32           padCtl;
    LwU32           headId = Disp.pPmsBsodCtx->head.headId;

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libVbios));

    // Set DPCD link settings and make sure NLT is supported
    status = dispSetDpcdLinkSettings_HAL(&Disp);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // EnableSpread IED script
    status = vbiosIedExelwteScript(&Disp.pPmsBsodCtx->vbios.imageDesc,
        Disp.pPmsBsodCtx->vbios.iedScripts.enableSpread,
        Disp.pPmsBsodCtx->vbios.conditionTableOffset,
        Disp.pPmsBsodCtx->dpInfo.portNum,
        Disp.pPmsBsodCtx->head.orIndex,
        Disp.pPmsBsodCtx->dpInfo.linkIndex);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // Power on Display
    dpAuxData = LW_DPCD_SET_POWER_VAL_D0_NORMAL;
    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_DPCD_SET_POWER, LW_TRUE, &dpAuxData);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // Power up the CML buffers for the padlink
    status = dispEnablePowerStateOfPadlinkBuffer_HAL(&Disp);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // beforeLT IED script
    status = vbiosIedExelwteScript(&Disp.pPmsBsodCtx->vbios.imageDesc,
        Disp.pPmsBsodCtx->vbios.iedScripts.beforeLT,
        Disp.pPmsBsodCtx->vbios.conditionTableOffset,
        Disp.pPmsBsodCtx->dpInfo.portNum,
        Disp.pPmsBsodCtx->head.orIndex,
        Disp.pPmsBsodCtx->dpInfo.linkIndex);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // BeforeLinkSpeed IED script
    status = vbiosIedExelwteScriptTable(&Disp.pPmsBsodCtx->vbios.imageDesc,
        Disp.pPmsBsodCtx->vbios.iedScripts.beforeLinkSpeed,
        Disp.pPmsBsodCtx->vbios.conditionTableOffset,
        Disp.pPmsBsodCtx->dpInfo.portNum,
        Disp.pPmsBsodCtx->head.orIndex,
        Disp.pPmsBsodCtx->dpInfo.linkIndex,
        Disp.pPmsBsodCtx->dpInfo.linkSpeedMul, 1, IED_TABLE_COMPARISON_EQ);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    dispConfigureSorSettings_HAL(&Disp);

    // Now write the lanecount
    switch (Disp.pPmsBsodCtx->dpInfo.laneCount)
    {
    default:
    case laneCount_0:
        laneCount = LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_ZERO;
        break;
    case laneCount_1:
        laneCount = LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_ONE;
        break;
    case laneCount_2:
        laneCount = LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_TWO;
        break;
    case laneCount_4:
        laneCount = LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_FOUR;
        break;
    case laneCount_8:
        laneCount = LW_PDISP_SOR_DP_LINKCTL_LANECOUNT_EIGHT;
        break;
    }
    value = REG_RD32(FECS, LW_PDISP_SOR_DP_LINKCTL(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.linkIndex));
    value = FLD_SET_DRF_NUM(_PDISP, _SOR_DP_LINKCTL, _LANECOUNT, laneCount, value);
    value = FLD_SET_DRF(_PDISP, _SOR_DP_LINKCTL, _ENABLE, _YES, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_DP_LINKCTL(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.linkIndex), value);

    // Restore the last trained lane settings needed for NLT
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_LANE_DRIVE_LWRRENT(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.dpIndex), Disp.pPmsBsodCtx->dpInfo.laneDriveLwrrent);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_LANE4_DRIVE_LWRRENT(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.dpIndex), Disp.pPmsBsodCtx->dpInfo.lane4DriveLwrrent);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_LANE_PREEMPHASIS(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.dpIndex), Disp.pPmsBsodCtx->dpInfo.lanePreEmphasis);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_LANE4_PREEMPHASIS(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.dpIndex), Disp.pPmsBsodCtx->dpInfo.lane4PreEmphasis);

    // Enable all lanes
    padCtl = FLD_SET_DRF(_PDISP_SOR, _DP_PADCTL, _PD_TXD_0, _NO, Disp.pPmsBsodCtx->dpInfo.padCtl);
    padCtl = FLD_SET_DRF(_PDISP_SOR, _DP_PADCTL, _PD_TXD_1, _NO, padCtl);
    padCtl = FLD_SET_DRF(_PDISP_SOR, _DP_PADCTL, _PD_TXD_2, _NO, padCtl);
    padCtl = FLD_SET_DRF(_PDISP_SOR, _DP_PADCTL, _PD_TXD_3, _NO, padCtl);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_DP_PADCTL(Disp.pPmsBsodCtx->head.orIndex, Disp.pPmsBsodCtx->dpInfo.dpIndex), padCtl);

    // Trigger lane sequencer
    value = REG_RD32(BAR0, LW_PDISP_SOR_LANE_SEQ_CTL(Disp.pPmsBsodCtx->head.orIndex));
    value = FLD_SET_DRF(_PDISP, _SOR_LANE_SEQ_CTL, _SETTING_NEW, _TRIGGER, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_LANE_SEQ_CTL(Disp.pPmsBsodCtx->head.orIndex), value);

    // Poll for LW_PDISP_SOR_LANE_SEQ_CTL_SETTING_NEW to change to _DONE.
    if (!PMU_REG32_POLL_US(USE_FECS(LW_PDISP_SOR_LANE_SEQ_CTL(Disp.pPmsBsodCtx->head.orIndex)),
        DRF_SHIFTMASK(LW_PDISP_SOR_LANE_SEQ_CTL_SETTING_NEW),
        LW_PDISP_SOR_LANE_SEQ_CTL_SETTING_NEW_DONE,
        LW_DISP_CNTL_TRIGGER_UDPATE_DONE_TIME_US,
        PMU_REG_POLL_MODE_BAR0_EQ))
    {
        status = FLCN_ERR_TIMEOUT;
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // Force Idle Pattern
    dispForceIdlePattern_HAL(&Disp, LW_TRUE);

    // Wait 20 us
    OS_PTIMER_SPIN_WAIT_US(20);

    // Enable backlight
    value = REG_RD32(BAR0, LW_GPIO_OUTPUT_CNTL(Disp.pPmsBsodCtx->dpInfo.lcdBacklightGpioPin));
    value = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _SEL, _NORMAL, value);
    value = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _1, value);
    value = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _YES, value);
    value = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_INPUT, _1, value);
    value = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUT_ILW, _DISABLE, value);
    value = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _PULLUD, _NONE, value);
    value = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _OPEN_DRAIN, _DISABLE, value);
    PMU_BAR0_WR32_SAFE(LW_GPIO_OUTPUT_CNTL(Disp.pPmsBsodCtx->dpInfo.lcdBacklightGpioPin), value);

    REG_RD32(BAR0, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER);
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL_TRIGGER, _UPDATE, _TRIGGER, value);
    PMU_BAR0_WR32_SAFE(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER, value);

    // Poll for LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER to change to _DONE.
    if (!PMU_REG32_POLL_US(USE_FECS(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER),
        DRF_SHIFTMASK(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE),
        LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_DONE,
        LW_DISP_CNTL_TRIGGER_UDPATE_DONE_TIME_US,
        PMU_REG_POLL_MODE_BAR0_EQ))
    {
        status = FLCN_ERR_TIMEOUT;
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // Wait 10 us
    OS_PTIMER_SPIN_WAIT_US(10);

    // Trigger LT
    dpAuxData = DRF_DEF(_SR, _NLT_CTL, _NLT_START, _INITIATE);
    status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_SR_NLT_CTL, LW_TRUE, &dpAuxData);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // Trigger the BS override to do a scrambler reset
    value = REG_RD32(FECS, LW_PDISP_SOR_DP_BS(Disp.pPmsBsodCtx->head.orIndex));
    value = FLD_SET_DRF(_PDISP, _SOR_DP_BS, _OVERRIDE, _TRIGGER, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SOR_DP_BS(Disp.pPmsBsodCtx->head.orIndex), value);

    // Wait for LT
    osPTimerTimeNsLwrrentGet(&lwrTimeStamp);
    do
    {
        if ((osPTimerTimeNsElapsedGet(&lwrTimeStamp)) > LW_DISP_TIMEOUT_NS_NLTS_AUTO_CLEAR)
        {
            status = FLCN_ERR_TIMEOUT;
            goto dispRunDpNoLinkTraining_GP10X_exit;
        }

        status = dispReadOrWriteRegViaAux_HAL(&Disp, Disp.pPmsBsodCtx->dpInfo.portNum, LW_SR_NLT_CTL, LW_FALSE, &dpAuxData);
        if (status != FLCN_OK)
        {
            goto dispRunDpNoLinkTraining_GP10X_exit;
        }

        // Spin-wait between condition checks.
        OS_PTIMER_SPIN_WAIT_NS(100);

    } while (!FLD_TEST_DRF(_SR, _NLT_CTL, _NLT_START, _SYNCED, dpAuxData));

    // Disable Idle Pattern
    dispForceIdlePattern_HAL(&Disp, LW_FALSE);

    // AfterLT IED script
    status = vbiosIedExelwteScript(&Disp.pPmsBsodCtx->vbios.imageDesc,
        Disp.pPmsBsodCtx->vbios.iedScripts.afterLT,
        Disp.pPmsBsodCtx->vbios.conditionTableOffset,
        Disp.pPmsBsodCtx->dpInfo.portNum,
        Disp.pPmsBsodCtx->head.orIndex,
        Disp.pPmsBsodCtx->dpInfo.linkIndex);
    if (status != FLCN_OK)
    {
        goto dispRunDpNoLinkTraining_GP10X_exit;
    }

    // Restore DP_LINKCTL
    PMU_BAR0_WR32_SAFE(LW_PDISP_SF_DP_LINKCTL(headId), Disp.pPmsBsodCtx->dpInfo.linkCtl);
    value = REG_RD32(BAR0, LW_PDISP_SF_DP_LINKCTL(headId));
    value = FLD_SET_DRF(_PDISP, _SF_DP_LINKCTL, _FORCE_RATE_GOVERN, _TRIGGER, value);
    PMU_BAR0_WR32_SAFE(LW_PDISP_SF_DP_LINKCTL(headId), value);

dispRunDpNoLinkTraining_GP10X_exit:
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libVbios));
    return status;
}

/*!
 * @brief     Returns the frame time that's lwrrently programmed 
 *
 * @param[in] index of head, counted within the domain of VRR heads
 *
 * @return    frame time value programmed in the register, in Us
 */
LwU32 
dispGetLwrrentFrameTimeUs_GA10X
(
    LwU32 idx
)
{
    LwU32 lwrrFrameTimeUs;

    lwrrFrameTimeUs = 
        DRF_VAL(_PDISP, _IHUB_LWRS_FRAME_TIME, _IN_USEC, REG_RD32(BAR0,
            LW_PDISP_IHUB_LWRS_FRAME_TIME(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex)));

    return lwrrFrameTimeUs;
}

/*!
 * @brief     Programs the new frame time value in the register
 *
 * @param[in] index of head, counted within the domain of VRR heads
 */
void 
dispProgramNewFrameTime_GA10X
(
    LwU32 idx
)
{
    LwU32 regVal;

    regVal = DRF_NUM(_PDISP, _IHUB_LWRS_FRAME_TIME, _IN_USEC,
                     (DispImpMclkSwitchSettings.vrrBasedMclk.oldFrameTimeUs[idx] +
                      DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnUs));

    // Program new frame time.
    REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_FRAME_TIME(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);
}

/*!
 * @brief     Gets the old frame time value and programs the register
 *
 * @param[in] index of head, counted within the domain of VRR heads
 */
void 
dispRestoreFrameTime_GA10X
(
    LwU32 idx
)
{
    LwU32 regVal;

    regVal = DRF_NUM(_PDISP, _IHUB_LWRS_FRAME_TIME, _IN_USEC, DispImpMclkSwitchSettings.vrrBasedMclk.oldFrameTimeUs[idx]);

    REG_WR32(BAR0, LW_PDISP_IHUB_LWRS_FRAME_TIME(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);
}

/*!
 * @brief Extends vblank for VRR heads for VRR based MCLK switch.
 *
 * @return FLCN_STATUS
 *      FLCN_OK if raster FP is extended and vblank is programmed.
 *      FLCN_ERR_TIMEOUT if timeout in write operation.
 *      FLCN_ERR_ILWALID_STATE if display is not present.
 */
FLCN_STATUS
dispProgramVblankForVrrExtendedFrame_GA10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       idx;
    LwU32       regVal;

    // Return early if display is disabled
    if (!DISP_IS_PRESENT())
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto dispProgramVblankForVrrExtendedFrame_GA10X_exit;
    }

    // Program vblank extension only if VRR based MCLK switch is supported.
    if ((DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask &
        LWBIT(RM_PMU_DISP_VRR)) != 0U)
    {
        if (DispImpMclkSwitchSettings.vrrBasedMclk.bVrrHeadRasterLockEnabled)
        {
            for (idx = 0U; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                regVal = 0U;
                regVal = FLD_SET_DRF(_PDISP,
                                     _RG_RASTER_V_EXTEND_FRONT_PORCH,
                                     _SET_ENABLE, _YES, regVal);
                regVal = FLD_SET_DRF_NUM(_PDISP, _RG_RASTER_V_EXTEND_FRONT_PORCH,
                                         _SET_HEIGHT,
                                         DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                                         regVal);
                REG_WR32(BAR0,
                    LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex),
                    regVal);

                // Set _TRIGGER signal for all heads corresponding to head 0.
                if (idx != 0U)
                {
                    REG_WR32(BAR0,
                        LW_PDISP_RG_RASTER_EXTEND_TRIGGER_SELECT(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex),
                        DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[0U].vrrHeadIndex);
                }

                // Save current frame time.
                DispImpMclkSwitchSettings.vrrBasedMclk.oldFrameTimeUs[idx] = dispGetLwrrentFrameTimeUs_HAL(&Disp, idx);
            }

            // Trigger Head 0 _UPDATE and wait for raster extension completion.
            dispSetRasterExtendHeightFp_HAL(&Disp,
                DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[0U].vrrHeadIndex,
                DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[0U].vrrVblankExtnLines,
                LW_TRUE,
                LW_TRUE);

            for (idx = 0U; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                dispProgramNewFrameTime_HAL(&Disp, idx);
            }
        }
        else
        {
            for (idx = 0U; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                // Extend vblank.
                dispSetRasterExtendHeightFp_HAL(&Disp,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                    LW_TRUE, LW_TRUE);

                // Save current frame time.
                DispImpMclkSwitchSettings.vrrBasedMclk.oldFrameTimeUs[idx] =
                    dispGetLwrrentFrameTimeUs_HAL(&Disp, idx);

                dispProgramNewFrameTime_HAL(&Disp, idx);
            }
        }
    }
dispProgramVblankForVrrExtendedFrame_GA10X_exit:
    return status;
}

/*!
 * @brief Restore vblank amd frame time for VRR heads after VRR based MCLK switch.
 *
 * @return FLCN_STATUS
 *      FLCN_OK if raster FP is extended and vblank is programmed.
 *      FLCN_ERR_TIMEOUT if timeout in write operation.
 *      FLCN_ERR_ILWALID_STATE if display is not present.
 */
FLCN_STATUS
dispRestoreVblankForVrrExtendedFrame_GA10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       regVal;
    LwU32       idx;

    // Return early if display is disabled
    if (!DISP_IS_PRESENT())
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto dispRestoreVblankForVrrExtendedFrame_GA10X_exit;
    }

    if ((DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask &
        LWBIT(RM_PMU_DISP_VRR)) != 0U)
    {
        if (DispImpMclkSwitchSettings.vrrBasedMclk.bVrrHeadRasterLockEnabled)
        {
            for (idx = 0U; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                // Restore frame time.
                dispRestoreFrameTime_HAL(&Disp, idx);
            }

            for (idx = 0U; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                regVal = 0U;
                regVal = FLD_SET_DRF(_PDISP,
                                     _RG_RASTER_V_EXTEND_FRONT_PORCH,
                                     _SET_ENABLE, _NO, regVal);
                regVal = FLD_SET_DRF_NUM(_PDISP, _RG_RASTER_V_EXTEND_FRONT_PORCH,
                                         _SET_HEIGHT,
                                         DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                                         regVal);
                REG_WR32(BAR0,
                    LW_PDISP_RG_RASTER_V_EXTEND_FRONT_PORCH(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex),
                    regVal);
            }

            // Trigger Head 0 _UPDATE and wait for raster extension completion.
            dispSetRasterExtendHeightFp_HAL(&Disp,
                DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[0U].vrrHeadIndex,
                DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[0U].vrrVblankExtnLines,
                LW_FALSE,
                LW_TRUE);

            // Restore the _TRIGGER signal of all heads to _INIT value.
            for (idx = 0U; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                REG_WR32(BAR0,
                        LW_PDISP_RG_RASTER_EXTEND_TRIGGER_SELECT(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex),
                        LW_PDISP_RG_RASTER_EXTEND_TRIGGER_SELECT_VFP_INIT);
            }
        }
        else
        {
            for (idx = 0U; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                dispRestoreFrameTime_HAL(&Disp, idx);

                // Disable vblank extension.
                dispSetRasterExtendHeightFp_HAL(&Disp,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                    LW_FALSE, LW_TRUE);
            }
        }
    }
dispRestoreVblankForVrrExtendedFrame_GA10X_exit:
    return status;
}

/*!
 * @brief     Enable power state of padlink buffer
 *
 * @param[in] padlink
 *
 * @return    FLCN_STATUS
 */
FLCN_STATUS
dispEnablePowerStateOfPadlinkBuffer_GA10X(void)
{

    LwU32 value;

    for (LwU32 padlink = 0; padlink < Disp.pPmsBsodCtx->numPadlinks; padlink++)
    {
        if (BIT(padlink) & Disp.pPmsBsodCtx->padLinkMask)
        {
            value = REG_RD32(BAR0, LW_PVTRIM_SYS_LINK_REFCLK_CFG(padlink));
            value = FLD_SET_DRF(_PVTRIM, _SYS_LINK_REFCLK_CFG, _POWERDOWN, _NO, value);

            PMU_BAR0_WR32_SAFE(LW_PVTRIM_SYS_LINK_REFCLK_CFG(padlink), value);
        }
    }

    return FLCN_OK;
}


/*!
 * @brief   Constructs the PWM source descriptor data for requested PWM source.
 *
 * @note    This function assumes that the overlay described by @ref ovlIdxDmem
 *          is already attached by the caller.
 *
 * @param[in] pwmSource   Requested PWM source as RM_PMU_PMGR_PWM_SOURCE_<xyz>
 * @param[in] ovlIdxDmem  DMEM Overlay index in which to construct the PWM
 *                        source descriptor
 *
 * @return  PWM_SOURCE_DESCRIPTOR   Pointer to PWM_SOURCE_DESCRIPTOR structure.
 * @return  NULL                    In case of invalid PWM source.
 */
PWM_SOURCE_DESCRIPTOR *
dispPwmSourceDescriptorConstruct_GA10X
(
    RM_PMU_PMGR_PWM_SOURCE pwmSource,
    LwU8                   ovlIdxDmem
)
{
    PWM_SOURCE_DESCRIPTOR *pPwmSrcDesc = NULL;
    PWM_SOURCE_DESCRIPTOR_CLKSRC pwmSrcDescClkSrc;

    switch (pwmSource)
    {
        case RM_PMU_PMGR_PWM_SOURCE_DISP_SOR_PWM_0:
        case RM_PMU_PMGR_PWM_SOURCE_DISP_SOR_PWM_1:
        case RM_PMU_PMGR_PWM_SOURCE_DISP_SOR_PWM_2:
        case RM_PMU_PMGR_PWM_SOURCE_DISP_SOR_PWM_3:
        {
            ct_assert(DRF_MASK(LW_PDISP_SOR_PWM_DIV_DIVIDE) ==
                        DRF_MASK(LW_PDISP_SOR_PWM_CTL_DUTY_CYCLE));
            ct_assert(DRF_BASE(LW_PDISP_SOR_PWM_DIV_DIVIDE) == 0);
            ct_assert(LW_PDISP_SOR_PWM_CTL_SETTING_NEW_DONE == 0);
            ct_assert(LW_PDISP_SOR_PWM_CTL_SETTING_NEW_TRIGGER == 1);

            LwU8 idx = (LwU8)(pwmSource - RM_PMU_PMGR_PWM_SOURCE_DISP_SOR_PWM_0);

            pwmSrcDescClkSrc.super.addrPeriod    = LW_PDISP_SOR_PWM_DIV(idx);
            pwmSrcDescClkSrc.super.addrDutycycle = LW_PDISP_SOR_PWM_CTL(idx);
            pwmSrcDescClkSrc.super.mask          = DRF_MASK(LW_PDISP_SOR_PWM_DIV_DIVIDE);
            pwmSrcDescClkSrc.super.triggerIdx    = DRF_BASE(LW_PDISP_SOR_PWM_CTL_SETTING_NEW);
            pwmSrcDescClkSrc.super.doneIdx       = DRF_BASE(LW_PDISP_SOR_PWM_CTL_SETTING_NEW);
            pwmSrcDescClkSrc.super.bus           = REG_BUS_BAR0;
            pwmSrcDescClkSrc.super.bCancel       = LW_FALSE;
            pwmSrcDescClkSrc.super.bTrigger      = LW_TRUE;
            pwmSrcDescClkSrc.clkSelBaseIdx       = DRF_BASE(LW_PDISP_SOR_PWM_CTL_CLKSEL);
            pwmSrcDescClkSrc.clkSel              = LW_PDISP_SOR_PWM_CTL_CLKSEL_XTAL;
            break;
        }
        default:
        {
            goto dispPwmSourceDescriptorConstruct_GA10X_exit;
        }
    }

    pwmSrcDescClkSrc.super.type = PWM_SOURCE_DESCRIPTOR_TYPE_CLKSRC;

    pPwmSrcDesc = pwmSourceDescriptorAllocate(ovlIdxDmem, pwmSource, &pwmSrcDescClkSrc.super);

dispPwmSourceDescriptorConstruct_GA10X_exit:
    return pPwmSrcDesc;
}

/*!
 * @brief       Force turn on SOR CLK
 * @see         Bug 200629496
 *
 * @details     The usecase is like SOR is not working (does not owned by any
 *              head and it's in sleep & safe mode) but we want to use the PWM
 *              with xtal as refclk. 
 *              sor_clk will be gated by BLCG and only on when there's register
 *              access. It takes cycles to pass through the register value from
 *              sor_clk to refclk, it may happen that sor_clk is gated before
 *              register reach refclk domain.
 *              The WAR is force sor_clk on by LW_PDISP_FE_CMGR_ORCLK_OVR.
 *              DISP team will be fixing this on Hopper - http://lwbugs/200633021
 */
void
dispForceSorClk_GA10X
(
    LwBool bEnable
)
{
    LwU32 reg = REG_RD32(BAR0, LW_PDISP_FE_CMGR_ORCLK_OVR);

    //
    // Forcefully turn on the SOR CLK in order to program
    // DISP PWM sources
    //
    if (bEnable)
    {
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR0_CLK, _ENABLE, reg);
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR1_CLK, _ENABLE, reg);
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR2_CLK, _ENABLE, reg);
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR3_CLK, _ENABLE, reg);
    }
    else
    {
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR0_CLK, _AUTO, reg);
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR1_CLK, _AUTO, reg);
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR2_CLK, _AUTO, reg);
        reg = FLD_SET_DRF(_PDISP, _FE_CMGR_ORCLK_OVR, _SOR3_CLK, _AUTO, reg);
    }

    REG_WR32(BAR0, LW_PDISP_FE_CMGR_ORCLK_OVR, reg);
}
