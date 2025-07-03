/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grga100.c
 * @brief  HAL-interface for the GA100 Graphics Engine, without ROP-in-GPC.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_graphics_nobundle.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_bar0.h"

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Asserts BECS reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertBECSReset_GA100(LwBool bAssert)
{
    LwU32 becsReset;

    becsReset = REG_RD32(BAR0, LW_PGRAPH_PRI_BES_BECS_CTXSW_BE_RESET_CTL);

    if (bAssert)
    {
        // Assert BECS Engine and Context Reset
        becsReset = FLD_SET_DRF(_PGRAPH, _PRI_BES_BECS_CTXSW_BE_RESET_CTL,
                              _BE_ENGINE_RESET, _ENABLED, becsReset);
        becsReset = FLD_SET_DRF(_PGRAPH, _PRI_BES_BECS_CTXSW_BE_RESET_CTL,
                             _BE_CONTEXT_RESET, _ENABLED, becsReset);
    }
    else
    {
        // De-assert BECS Engine and Context Reset
        becsReset = FLD_SET_DRF(_PGRAPH, _PRI_BES_BECS_CTXSW_BE_RESET_CTL,
                              _BE_ENGINE_RESET, _DISABLED, becsReset);
        becsReset = FLD_SET_DRF(_PGRAPH, _PRI_BES_BECS_CTXSW_BE_RESET_CTL,
                              _BE_CONTEXT_RESET, _DISABLED, becsReset);
    }

    REG_WR32(BAR0, LW_PGRAPH_PRI_BES_BECS_CTXSW_BE_RESET_CTL, becsReset);

    // Force read for flush
    REG_RD32(BAR0, LW_PGRAPH_PRI_BES_BECS_CTXSW_BE_RESET_CTL);
}
