/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmutu10x.c
 * @brief   PMU HAL functions for TU10X.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "dev_pwr_csb.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Enable precise exceptions for DMEM VA.
 */
void
pmuPreInitEnablePreciseExceptions_TU10X(void)
{
#if defined(ON_DEMAND_PAGING_BLK) && defined(UPROC_FALCON)
    REG_WR32(CSB, LW_CPWR_FALCON_DMCYA,
             DRF_DEF(_CPWR, _FALCON_DMCYA, _ROBSPR_DIS, _TRUE)    |
             DRF_DEF(_CPWR, _FALCON_DMCYA, _ROBWB_DIS,  _TRUE)    |
             DRF_DEF(_CPWR, _FALCON_DMCYA, _ROB_DIS,    _FALSE));
    REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_DBGCTL, _CYA, _ENABLE);
#endif // defined(ON_DEMAND_PAGING_BLK) && defined(UPROC_FALCON)
}

/* ------------------------- Private Functions ------------------------------ */
