/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_fbgm200.c
 * @brief  FB Hal Functions for GM200
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pri_ringmaster.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfb.h"
#include "dbgprintf.h"
#include "config/g_fb_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */

/*!
 * @brief Gets the active LTC count.
 *
 * @param[in]   pGpu    OBJGPU pointer.
 * @param[in]   pFb     OBJFB  pointer.
 *
 * @return      The active LTC count.
 */
LwU32
fbGetActiveLTCCount_GM200(void)
{
    //
    // Starting with GM20X, there may not be a 1:1 correspondence of LTCs and
    // FBPs. Thus, use the new ROP_L2 (LTC) enumeration register to determine
    // the number of LTCs (as opposed to the FBP enumeration register).
    //
    LwU32 val;
    LwU32 numActiveLTCs;

    val           = REG_RD32(FECS, LW_PPRIV_MASTER_RING_ENUMERATE_RESULTS_ROP_L2);
    numActiveLTCs = DRF_VAL(_PPRIV, _MASTER_RING_ENUMERATE_RESULTS_ROP_L2, _COUNT, val);
    PMU_HALT_COND(numActiveLTCs > 0);
    return numActiveLTCs;
}
