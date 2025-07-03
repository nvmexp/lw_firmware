/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispgp10x.c
 * @brief  HAL-interface for the GP10x Display Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_disp.h"
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_disp.h"  /// XXXEB RECHECK LIST HERE
#include "pmu_objseq.h"
#include "pmu_objvbios.h"
#include "lw_ref_dev_sr.h"
#include "dev_pmgr.h"
#include "pmu_objdisp.h"
#include "displayport/dpcd.h"
#include "config/g_disp_private.h"

/* ---------------------------Public functions ------------------------------ */

/*!
 * @brief  Run No link training
 *
 * @return FLCN_STATUS FLCN_OK if link training completed
 */
FLCN_STATUS
dispRunDpNoLinkTraining_GP10X(void)
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
    value = REG_RD32(BAR0, LW_PMGR_GPIO_OUTPUT_CNTL(Disp.pPmsBsodCtx->dpInfo.lcdBacklightGpioPin));
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _SEL, _NORMAL, value);
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_OUTPUT, _1, value);
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _YES, value);
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_INPUT, _1, value);
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _IO_OUT_ILW, _DISABLE, value);
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _PULLUD, _NONE, value);
    value = FLD_SET_DRF(_PMGR_GPIO, _OUTPUT_CNTL, _OPEN_DRAIN, _DISABLE, value);
    PMU_BAR0_WR32_SAFE(LW_PMGR_GPIO_OUTPUT_CNTL(Disp.pPmsBsodCtx->dpInfo.lcdBacklightGpioPin), value);

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
