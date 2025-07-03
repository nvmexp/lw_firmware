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
 * @file   pmu_dispgp10xtu10x.c
 * @brief  HAL-interface for the GP10x through TU10x Display Engine only.
 */

/* ------------------------- System Includes -------------------------------- */

#include "pmusw.h"
#include "dev_disp.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */

#include "pmu_disp.h"
#include "pmu_objdisp.h"
#include "pmu_objseq.h"

/* ------------------------- Type Definitions ------------------------------- */

typedef enum
{
    padlink_A = 0,
    padlink_B = 1,
    padlink_C = 2,
    padlink_D = 3,
    padlink_E = 4,
    padlink_F = 5,
    padlink_G = 6,
    padlink_ilwalid
} LW_DISP_DFPPADLINK;


/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private functions ------------------------------ */

/*!
 * @brief     Enable power state of padlink buffer
 *
 * @param[in] padlink
 *
 * @return    FLCN_STATUS
 */
FLCN_STATUS
dispEnablePowerStateOfPadlinkBuffer_GP10X(void)
{
    LwU32 padlink;

    for (padlink = 0; padlink < Disp.pPmsBsodCtx->numPadlinks; padlink++)
    {
        if (BIT(padlink) & Disp.pPmsBsodCtx->padLinkMask)
        {
            LwU32 regVal = (padlink < padlink_E) ?
                REG_RD32(BAR0, LW_PVTRIM_SYS_SPPLL0_CTRL) :
                REG_RD32(BAR0, LW_PVTRIM_SYS_SPPLL1_CTRL);

            if (padlink < padlink_E)
            {
                if ((padlink == padlink_A) || (padlink == padlink_B))
                {
                    if (FLD_IDX_TEST_DRF(_PVTRIM, _SYS_SPPLL0, _CTRL_DIFFMUX_PD, dispGetPairPadLink_HAL(&Disp, padlink), _ENABLE, regVal))
                    {
                        // Power up CML buffer for padlink B as well
                        regVal = FLD_IDX_SET_DRF(_PVTRIM, _SYS_SPPLL0, _CTRL_DIFFMUX_PD, dispGetPairPadLink_HAL(&Disp, padlink), _DISABLE, regVal);
                        PMU_BAR0_WR32_SAFE(LW_PVTRIM_SYS_SPPLL0_CTRL, regVal);
                    }
                }
                else if (padlink == padlink_D)
                {
                    if (FLD_IDX_TEST_DRF(_PVTRIM, _SYS_SPPLL0, _CTRL_DIFFMUX_PD, padlink_C, _ENABLE, regVal))
                    {
                        regVal = FLD_IDX_SET_DRF(_PVTRIM, _SYS_SPPLL0, _CTRL_DIFFMUX_PD, padlink_C, _DISABLE, regVal);
                        PMU_BAR0_WR32_SAFE(LW_PVTRIM_SYS_SPPLL0_CTRL, regVal);
                    }
                }
                if (FLD_IDX_TEST_DRF(_PVTRIM, _SYS_SPPLL0, _CTRL_DIFFMUX_PD, padlink, _ENABLE, regVal))
                {
                    regVal = FLD_IDX_SET_DRF(_PVTRIM, _SYS_SPPLL0, _CTRL_DIFFMUX_PD, padlink, _DISABLE, regVal);
                    PMU_BAR0_WR32_SAFE(LW_PVTRIM_SYS_SPPLL0_CTRL, regVal);
                }
            }
            else
            {
                LwU32 adjustedPadlinkOffset = padlink - LW_DISP_PADLINK_ADJ_FACTOR;

                if (padlink == padlink_F)
                {
                    if (FLD_IDX_TEST_DRF(_PVTRIM, _SYS_SPPLL1, _CTRL_DIFFMUX_PD, (padlink_E - LW_DISP_PADLINK_ADJ_FACTOR), _ENABLE, regVal))
                    {
                        regVal = FLD_IDX_SET_DRF(_PVTRIM, _SYS_SPPLL1, _CTRL_DIFFMUX_PD, (padlink_E - LW_DISP_PADLINK_ADJ_FACTOR), _DISABLE, regVal);
                        PMU_BAR0_WR32_SAFE(LW_PVTRIM_SYS_SPPLL1_CTRL, regVal);
                    }
                }
                if (FLD_IDX_TEST_DRF(_PVTRIM, _SYS_SPPLL1, _CTRL_DIFFMUX_PD, adjustedPadlinkOffset, _ENABLE, regVal))
                {
                    regVal = FLD_IDX_SET_DRF(_PVTRIM, _SYS_SPPLL1, _CTRL_DIFFMUX_PD, adjustedPadlinkOffset, _DISABLE, regVal);
                    PMU_BAR0_WR32_SAFE(LW_PVTRIM_SYS_SPPLL1_CTRL, regVal);
                }
            }
        }
    }

    // Done
    return FLCN_OK;
}


