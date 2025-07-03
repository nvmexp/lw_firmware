/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_didlegm20x.c
 * @brief  HAL-interface for GM20x Deepidle Engine
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_trim.h"
#include "dev_trim_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objdi.h"
#include "pmu_objpmu.h"

#include "config/g_di_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Declarations -------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Checks if DISPCLK driven from alternate path
 *
 * This function checks clock sourcing path for DISPCLK. It returns LW_TRUE if
 * DISPCLK is driven by alternate path.
 *
 * @return      LW_TRUE     if Display clocks are driven from alternate path
 *              LW_FALSE    otherwise
 */
LwBool
diSeqIsDispclkOnAltPath_GM20X(void)
{
    LwU32  selVcoStatus;
    LwU32  vclkIndex;

    selVcoStatus = REG_RD32(FECS, LW_PVTRIM_SYS_STATUS_SEL_VCO);

    // VCLK is expected on alternate path. ASSERT if VCLK is not on alternate path.
    for (vclkIndex = 0; vclkIndex < LW_PVTRIM_SYS_STATUS_SEL_VCO_VCLK_OUT__SIZE_1;
         vclkIndex++)
    {
        PMU_HALT_COND(FLD_IDX_TEST_DRF(_PVTRIM, _SYS_STATUS_SEL_VCO, _VCLK_OUT,
                                    vclkIndex, _ALT_PATH, selVcoStatus));
    }

    if (FLD_TEST_DRF(_PVTRIM, _SYS_STATUS_SEL_VCO, _DISPCLK_OUT,
                        _ALT_PATH, selVcoStatus))
    {
        return LW_TRUE;
    }
    else
    {
        DI_SEQ_ABORT_REASON_UPDATE_NAME(DISP_ON_VCO_PATH);
        return LW_FALSE;
    }
}
