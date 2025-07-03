/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmgrtestgh100.c
 * @brief   PMU HAL functions related to PMGR tests for GH100+
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmu_objpmu.h"

#include "config/g_pmgr_private.h"

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- Public Function Prototypes  -------------------- */
/* ------------------------- Static Function Prototypes  -------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief  Sets the number of bits for VID_DOWNSHIFT for the PWMVID generator
 *         specified by @ref pwmVidIdx.
 *
 * @param[in]  pwmVidIdx     Index of the PWMVID generator to configure
 * @param[in]  downShiftVal  Number of bits for VID_DOWNSHIFT
 */
void
pmgrTestIpcPwmVidDownshiftSet_GH100
(
    LwU8 pwmVidIdx,
    LwU8 downShiftVal
)
{
    LwU32 regIpc = REG_RD32(CSB, LW_CPWR_THERM_VID5_PWM(pwmVidIdx));

    regIpc = FLD_SET_DRF_NUM(_CPWR_THERM, _VID5_PWM, _VID_DOWNSHIFT,
                downShiftVal, regIpc);
    REG_WR32(CSB, LW_CPWR_THERM_VID5_PWM(pwmVidIdx), regIpc);
}
