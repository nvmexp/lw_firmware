/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file   pmu_disptu10xonly.c
 * @brief  HAL-interface for the TU10x Display Engine only.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_disp.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objdisp.h"
#include "pmu_disp.h"
#include "config/g_disp_private.h"
#include "dev_trim.h"

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Programs memfetch vblank duration watermarks and other settings for
 *        MCLK switch
 *
 * To-do akshatam: Memfetch vblank settings can also be programmed at modeset time
 * instead of mclk switch time to reduce the reqister writes. However, we wanted
 * to bring up one of the MCLK switch types as a part of this infra development to
 * ensure everything is in place and memfetch vblank was something that had to be
 * verified anyway hence thought of adding the support to program the watermarks mclk
 * switch time. Once things are stable and the support for rest of the MCLK switches
 * is up, we can re-visit this and limit it to just modeset time programming.
 */
void
dispProgramMemFetchVblankSettings_TU10X(void)
{
    LwU32   regVal  = 0;
    LwU8    headIdx;

    // Return early if display is disabled
    if (!DISP_IS_PRESENT())
    {
        return;
    }

    if ((DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask &
        LWBIT(RM_PMU_DISP_MEMFETCH_VBLANK)) != 0)
    {
        regVal = FLD_SET_DRF_NUM(_PDISP, _FE_MEMFETCH_VBLANK_WATERMARK, _VALUE,
                         DispImpMclkSwitchSettings.memfetchVblank.maxMclkFbstopTimeUs,
                         regVal);
    }
    else
    {
        //
        // Program the watermark to the maximum possible value before the MCLK
        // switch if memfetch vblank method is disabled.
        // https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=200354957&cmtNo=35
        //
        regVal = FLD_SET_DRF_NUM(_PDISP, _FE_MEMFETCH_VBLANK_WATERMARK, _VALUE,
                                 DRF_MASK(LW_PDISP_FE_MEMFETCH_VBLANK_WATERMARK_VALUE),
                                 regVal);
    }

    for (headIdx = 0; headIdx < Disp.numHeads; headIdx++)
    {
        if (DispImpMclkSwitchSettings.bActiveHead[headIdx])
        {
            regVal = FLD_SET_DRF(_PDISP, _FE_MEMFETCH_VBLANK_WATERMARK, _MASK,
                                 _ENABLE, regVal);
        }
        else
        {
            regVal = FLD_SET_DRF(_PDISP, _FE_MEMFETCH_VBLANK_WATERMARK, _MASK,
                                 _DISABLE, regVal);
        }
        REG_WR32(BAR0, LW_PDISP_FE_MEMFETCH_VBLANK_WATERMARK(headIdx), regVal);
    }
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
dispProgramVblankForVrrExtendedFrame_TU10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       idx;
    LwU32       regVal;

    // Return early if display is disabled
    if (!DISP_IS_PRESENT())
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto dispProgramVblankForVrrExtendedFrame_TU10X_exit;
    }

    // Program vblank extension only if VRR based MCLK switch is supported.
    if ((DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask &
        LWBIT(RM_PMU_DISP_VRR)) != 0)
    {
        if (DispImpMclkSwitchSettings.vrrBasedMclk.bVrrHeadRasterLockEnabled)
        {
            // Wait for RG to be in active region to have sufficient time to program front porch in single frame.
            if (!PMU_REG32_POLL_NS(LW_PDISP_RG_STATUS(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[0].vrrHeadIndex),
                DRF_SHIFTMASK(LW_PDISP_RG_STATUS_VBLNK),
                REF_DEF(LW_PDISP_RG_STATUS_VBLNK, _INACTIVE),
                LW_DISP_PMU_REG_POLL_RG_STATUS_VBLNK_TIMEOUT_NS, PMU_REG_POLL_MODE_BAR0_EQ))
            {
                status = FLCN_ERR_TIMEOUT;
                goto dispProgramVblankForVrrExtendedFrame_TU10X_exit;
            }

            for (idx = 0; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                // Extend vblank.
                dispSetRasterExtendHeightFp_HAL(&Disp,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                    LW_TRUE, ((idx == (DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads - 1)) ? LW_TRUE : LW_FALSE));

                // Save current memfetch vblank duration.
                DispImpMclkSwitchSettings.vrrBasedMclk.oldMemFetchVblankDurationUs[idx] =
                    DRF_VAL(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _VALUE, REG_RD32(BAR0,
                        LW_PDISP_FE_MEMFETCH_VBLANK_DURATION(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex)));
            }

            for (idx = 0; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                regVal =
                    DRF_NUM(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _VALUE,
                    (DispImpMclkSwitchSettings.vrrBasedMclk.oldMemFetchVblankDurationUs[idx] +
                        DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnUs)) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _WRITE_MODE, _ACTIVE) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _UPDATE, _IMMEDIATE);

                // Program memfetch vblank duration.
                REG_WR32(BAR0, LW_PDISP_FE_MEMFETCH_VBLANK_DURATION(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);
            }
        }
        else
        {
            for (idx = 0; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                // Extend vblank.
                dispSetRasterExtendHeightFp_HAL(&Disp,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                    LW_TRUE, LW_TRUE);

                // Save current memfetch vblank duration.
                DispImpMclkSwitchSettings.vrrBasedMclk.oldMemFetchVblankDurationUs[idx] =
                    DRF_VAL(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _VALUE, REG_RD32(BAR0,
                        LW_PDISP_FE_MEMFETCH_VBLANK_DURATION(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex)));

                regVal =
                    DRF_NUM(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _VALUE,
                    (DispImpMclkSwitchSettings.vrrBasedMclk.oldMemFetchVblankDurationUs[idx] +
                        DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnUs)) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _WRITE_MODE, _ACTIVE) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _UPDATE, _IMMEDIATE);

                // Program memfetch vblank duration.
                REG_WR32(BAR0, LW_PDISP_FE_MEMFETCH_VBLANK_DURATION(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);
            }
        }
    }
dispProgramVblankForVrrExtendedFrame_TU10X_exit:
    return status;
}

/*!
 * @brief Restore vblank amd memFetch for VRR heads for VRR based MCLK switch.
 *
 * @return FLCN_STATUS
 *      FLCN_OK if raster FP is extended and vblank is programmed.
 *      FLCN_ERR_TIMEOUT if timeout in write operation.
 *      FLCN_ERR_ILWALID_STATE if display is not present.
 */
FLCN_STATUS
dispRestoreVblankForVrrExtendedFrame_TU10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       regVal;
    LwU32       idx;

    // Return early if display is disabled
    if (!DISP_IS_PRESENT())
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto dispRestoreVblankForVrrExtendedFrame_TU10X_exit;
    }

    if ((DispImpMclkSwitchSettings.mclkSwitchTypesSupportedMask &
        LWBIT(RM_PMU_DISP_VRR)) != 0)
    {
        if (DispImpMclkSwitchSettings.vrrBasedMclk.bVrrHeadRasterLockEnabled)
        {
            for (idx = 0; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                // Restore memfetch vblank duration.
                regVal =
                    DRF_NUM(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _VALUE,
                        DispImpMclkSwitchSettings.vrrBasedMclk.oldMemFetchVblankDurationUs[idx]) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _WRITE_MODE, _ACTIVE) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _UPDATE, _IMMEDIATE);

                REG_WR32(BAR0, LW_PDISP_FE_MEMFETCH_VBLANK_DURATION(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);
            }

            // Wait for RG to be in active region to have sufficient time to program front porch in single frame.
            if (!PMU_REG32_POLL_NS(LW_PDISP_RG_STATUS(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[0].vrrHeadIndex),
                DRF_SHIFTMASK(LW_PDISP_RG_STATUS_VBLNK),
                REF_DEF(LW_PDISP_RG_STATUS_VBLNK, _INACTIVE),
                LW_DISP_PMU_REG_POLL_RG_STATUS_VBLNK_TIMEOUT_NS, PMU_REG_POLL_MODE_BAR0_EQ))
            {
                status = FLCN_ERR_TIMEOUT;
                goto dispRestoreVblankForVrrExtendedFrame_TU10X_exit;
            }

            for (idx = 0; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                // Disable vblank extension.
                dispSetRasterExtendHeightFp_HAL(&Disp,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                    LW_FALSE, ((idx == (DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads - 1)) ? LW_TRUE : LW_FALSE));
            }
        }
        else
        {
            for (idx = 0; idx < DispImpMclkSwitchSettings.vrrBasedMclk.numVrrHeads; idx++)
            {
                // Restore memfetch vblank duration.
                regVal =
                    DRF_NUM(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _VALUE,
                        DispImpMclkSwitchSettings.vrrBasedMclk.oldMemFetchVblankDurationUs[idx]) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _WRITE_MODE, _ACTIVE) |
                    DRF_DEF(_PDISP, _FE_MEMFETCH_VBLANK_DURATION, _UPDATE, _IMMEDIATE);

                REG_WR32(BAR0, LW_PDISP_FE_MEMFETCH_VBLANK_DURATION(DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex), regVal);

                // Disable vblank extension.
                dispSetRasterExtendHeightFp_HAL(&Disp,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrHeadIndex,
                    DispImpMclkSwitchSettings.vrrBasedMclk.vrrInfo[idx].vrrVblankExtnLines,
                    LW_FALSE, LW_TRUE);
            }
        }
    }
dispRestoreVblankForVrrExtendedFrame_TU10X_exit:
    return status;
}

/*!
 * @brief       Restore VPLLs
 *
 * @details     The order of these register writes is important.
 *
 *              These data are captured in the RM in clkCaptureParamsForPmsOnBsod.
 *
 *              This implementation assumes the analog tsmc16ffb VPLLs used in
 *              Turing and prior.
 */
void
dispPmsModeSetRestoreVpll_TU10X(LwU32 headId)
{
        REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_SSD0(headId),  Disp.pPmsBsodCtx->vplls.regs.analog.ssd0);
        REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_SSD1(headId),  Disp.pPmsBsodCtx->vplls.regs.analog.ssd1);
        REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_CFG(headId),   Disp.pPmsBsodCtx->vplls.cfg);
        REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_CFG2(headId),  Disp.pPmsBsodCtx->vplls.regs.analog.cfg2);
        REG_WR32(BAR0, LW_PVTRIM_SYS_VPLL_COEFF(headId), Disp.pPmsBsodCtx->vplls.coeff);
}
