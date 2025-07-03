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
 * @file   fbga100.c
 * @brief  GA100-only HALs.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_ltc.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfb.h"

#include "config/g_fb_private.h"

/* ------------------------ Public Functions  ------------------------------- */

/*!
 * SW WAR for Bug 2805650.
 */
void
fbWarBug2805650_GA100(void)
{
    LwU32 reg;

    reg = REG_RD32(BAR0,
                   LW_PLTCG_LTCS_LTSS_TSTG_CFG_5);
    reg = FLD_SET_DRF(_PLTCG_LTCS, _LTSS_TSTG_CFG_5, _DISCARD_TEX_ALL_DATA, __PROD, reg);
    REG_WR32(BAR0, LW_PLTCG_LTCS_LTSS_TSTG_CFG_5, reg);
}
