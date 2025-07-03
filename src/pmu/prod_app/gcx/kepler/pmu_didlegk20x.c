/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_didlegk20x.c
 * @brief  HAL-interface for the GK20x Deepidle Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_didle.h"

#include "config/g_gcx_private.h"

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
LwU32 elcgState[LW_CPWR_THERM_GATE_CTRL__SIZE_1];

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Disables ELCG on all the engines as we gate most of the clocks
 *        in deepidle
 */
void
gcxDidleDisableElcg_GK20X(void)
{
    LwU32 regVal;
    LwU32 i;

    for (i = 0; i < LW_CPWR_THERM_GATE_CTRL__SIZE_1; i++)
    {
        regVal = elcgState[i] = REG_RD32(CSB, LW_CPWR_THERM_GATE_CTRL(i));
        regVal = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK, _RUN, regVal);
        REG_WR32(CSB, LW_CPWR_THERM_GATE_CTRL(i), regVal);
    }
}
