/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   biftu10xga100.c
 * @brief  Contains TU10X/GA100 based HAL implementations
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"

#include "dev_lw_xp.h"
#include "dev_lw_xve.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "config/g_bif_private.h"
#include "pmu_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 * @brief bifWriteL1Threshold_TU10X()
 *
 * param[in]  threshold
 * param[in]  thresholdFrac
 * param[in]  genSpeed
 */
void
bifWriteL1Threshold_TU10X
(
    LwU8                   threshold,
    LwU8                   thresholdFrac,
    RM_PMU_BIF_LINK_SPEED  genSpeed
)
{
    LwU32 data;
    LwU8  l1Threshold     = 0;
    LwU8  l1ThresholdFrac = 0;

    if (Bif.platformParams.bAspmThreshTablePresent)
    {
        switch (genSpeed)
        {
            case RM_PMU_BIF_LINK_SPEED_GEN1PCIE:
            {
                l1Threshold     = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN1_INDEX].l1Threshold;
                l1ThresholdFrac = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN1_INDEX].l1ThresholdFrac;
                break;
            }

            case RM_PMU_BIF_LINK_SPEED_GEN2PCIE:
            {
                if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN2PCIE, _SUPPORTED, Bif.bifCaps))
                {
                    l1Threshold     = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN2_INDEX].l1Threshold;
                    l1ThresholdFrac = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN2_INDEX].l1ThresholdFrac;
                }
                break;
            }

            case RM_PMU_BIF_LINK_SPEED_GEN3PCIE:
            {
                if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN3PCIE, _SUPPORTED, Bif.bifCaps))
                {
                    l1Threshold     = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN3_INDEX].l1Threshold;
                    l1ThresholdFrac = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN3_INDEX].l1ThresholdFrac;
                }
                break;
            }
            case RM_PMU_BIF_LINK_SPEED_GEN4PCIE:
            {
                if (FLD_TEST_DRF(_RM_PMU, _BIF_CAPS, _GEN4PCIE, _SUPPORTED, Bif.bifCaps))
                {
                    l1Threshold     = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN4_INDEX].l1Threshold;
                    l1ThresholdFrac = Bif.platformParams.l1ThresholdData[RM_PMU_BIF_GEN4_INDEX].l1ThresholdFrac;
                }
                break;
            }
            default:
            {
                l1Threshold     = threshold;
                l1ThresholdFrac = thresholdFrac;
                break;
            }
        }
    }
    else
    {
        l1Threshold     = threshold;
        l1ThresholdFrac = thresholdFrac;
    }

    // Verify values are within HW defined range
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if ((l1Threshold < LW_XP_DL_MGR_1_L1_THRESHOLD_MIN) ||
            (l1Threshold > LW_XP_DL_MGR_1_L1_THRESHOLD_MAX) ||
            (l1ThresholdFrac < LW_XP_DL_MGR_1_L1_THRESHOLD_FRAC_MIN) ||
            (l1ThresholdFrac > LW_XP_DL_MGR_1_L1_THRESHOLD_FRAC_MAX))
        {
            PMU_BREAKPOINT();
            goto bifWriteL1Threshold_exit;
        }
    }

    data = BAR0_REG_RD32(LW_XP_DL_MGR_1(0));
    data = FLD_SET_DRF_NUM (_XP, _DL_MGR_1, _L1_THRESHOLD,      l1Threshold, data);
    data = FLD_SET_DRF_NUM (_XP, _DL_MGR_1, _L1_THRESHOLD_FRAC, l1ThresholdFrac, data);
    BAR0_REG_WR32(LW_XP_DL_MGR_1(0), data);

bifWriteL1Threshold_exit:
    return;
}

/*!
 * @brief bifCheckL1ThresholdData_TU10X()
 *
 * @param[in]  pPlatformParams
 */
FLCN_STATUS
bifCheckL1Threshold_TU10X(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 idx;

    if (Bif.platformParams.bAspmThreshTablePresent)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_BIF))
        {
            if (((DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_VAL,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_BIF_LTR_L1_2_THRESHOLD_VALUE) &&
                 (DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_VAL,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_XVE_L1_PM_SUBSTATES_CTRL1_LTR_L1_2_THRES_VAL_INIT)) ||
                ((DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_SCALE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_BIF_LTR_L1_2_THRESHOLD_SCALE) &&
                 (DRF_VAL(_XVE, _L1_PM_SUBSTATES_CTRL1, _LTR_L1_2_THRES_SCALE,
                          Bif.l1ssRegs.aspmL1SsCtrlEnable) !=
                  LW_XVE_L1_PM_SUBSTATES_CTRL1_LTR_L1_2_THRES_SCALE_INIT)))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto bifCheckL1ThresholdData_exit;
            }
        }

        for (idx = RM_PMU_BIF_GEN1_INDEX; idx < RM_PMU_BIF_MAX_GEN_SPEED_SUPPORTED; ++idx)
        {
            if ((Bif.platformParams.l1ThresholdData[idx].l1Threshold <
                 LW_XP_DL_MGR_1_L1_THRESHOLD_MIN)      ||
                (Bif.platformParams.l1ThresholdData[idx].l1Threshold >
                 LW_XP_DL_MGR_1_L1_THRESHOLD_MAX)      ||
                (Bif.platformParams.l1ThresholdData[idx].l1ThresholdFrac <
                 LW_XP_DL_MGR_1_L1_THRESHOLD_FRAC_MIN) ||
                (Bif.platformParams.l1ThresholdData[idx].l1ThresholdFrac >
                 LW_XP_DL_MGR_1_L1_THRESHOLD_FRAC_MAX))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto bifCheckL1ThresholdData_exit;
            }
        }
    }

bifCheckL1ThresholdData_exit:
    return status;
}
