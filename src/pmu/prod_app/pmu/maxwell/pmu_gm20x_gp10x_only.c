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
 * @file    pmu_pmugm20x_gp10x_only.c
 *          PMU HAL Functions for GM20X and GP10X GPUs
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Enable debug features
 */
void
pmuPreInitDebugFeatures_GM20X(void)
{
    LwU32 val = 0;

    // Enable support for ICD commands in LS mode
    val = FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_STOP,     _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_RUN,      _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_RUNB,     _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_STEP,     _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_EMASK,    _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_RREG_SPR, _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_RSTAT,    _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _PRIVWL_IBRKPT,      _ENABLE, val) |
          FLD_SET_DRF(_CPWR, _FALCON_DBGCTL, _ICD_CMDWL_RREG_GPR, _ENABLE, val);

    REG_WR32(CSB, LW_CPWR_FALCON_DBGCTL, val);
}
