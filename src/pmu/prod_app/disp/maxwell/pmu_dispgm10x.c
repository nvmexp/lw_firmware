/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_dispgmxxx.c
 * @brief  HAL-interface for the GMxxx Display Engine.
 *
 * The following Display HAL routines service all GM10X chips, including chips
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
#include "dev_fuse.h"
#include "dev_disp.h"
#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objdisp.h"
#include "pmu_disp.h"

#include "config/g_disp_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Prototypes-------------------------------------- */
/* -------------------------  Global variables  -------------------------------*/

/*!
 * Enable/disable DWCF rising edge.
 */
void
dispProgramDwcfIntr_GM10X
(
    LwBool bEnable
)
{
    LwU32 val;
    if (bEnable)
    {
        // clear and then enable.
        val = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING);
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING, _FBH_DWCF, _PENDING, val);
        REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING, val);
        val = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING);

        val = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN, _FBH_DWCF, _ENABLED, val);
        REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, val);

        val = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
    }
    else
    {
        val = REG_RD32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN);
        val = FLD_SET_DRF(_CPWR, _PMU_GPIO_1_INTR_RISING_EN, _FBH_DWCF, _DISABLED, val);
        REG_WR32(CSB, LW_CPWR_PMU_GPIO_1_INTR_RISING_EN, val);
    }
}
