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
 * @file   pmu_pgtu10x.c
 * @brief  PMU Power Gating Object
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_therm.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objap.h"

#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Configures AP Supplemental Idle Counter.
 *
 * It sets Idle Mask for Supplemental Idle Counter
 *
 * @param[in]  suppMaskHwIdx    Index to HW counter
 * @param[in]  pSuppMask        Mask of Supplemental Idle Signal
 */
void
pgApConfigIdleMask_TU10X
(
    LwU32  suppMaskHwIdx,
    LwU32 *pSuppMask
)
{
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_SUPP(suppMaskHwIdx), pSuppMask[0]);
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_1_SUPP(suppMaskHwIdx), pSuppMask[1]);
    REG_WR32(CSB, LW_CPWR_PMU_IDLE_MASK_2_SUPP(suppMaskHwIdx), pSuppMask[2]);
}
