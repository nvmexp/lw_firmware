/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cgga10b.c
 * @brief  LPWR Clock Gating related functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_cg.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @Brief : LPWR Clock Gating Enable API
 *
 * @Description : API enables Clock Gating for LPWR Engine
 *
 */
void
lpwrCgElpgSequencerEnable_GA10B(void)
{
    LwU32 val;
 
    val = REG_RD32(CSB, LW_CPWR_PMU_ELPG_SEQ_CG);
    val = FLD_SET_DRF(_CPWR_PMU, _ELPG_SEQ_CG, _EN, _ENABLED, val);
    REG_WR32(CSB, LW_CPWR_PMU_ELPG_SEQ_CG, val);
}
