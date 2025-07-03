/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgkxxxtuxxx.c
 * @brief  HAL-interface for the GKXXX-TUXXX (pre-Ampere) Graphics Engine.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_top.h"
#include "dev_graphics_nobundle.h"
#include "dev_pri_ringmaster.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pri_ringstation_gpc.h"
#include "dev_pri_ringstation_sys.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objfifo.h"
#include "pmu_objpmu.h"
#include "pmu_bar0.h"
#include "dev_pwr_pri.h"
#include "config/g_gr_private.h"
#include "dev_master.h"

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Asserts GPC reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertGPCReset_GMXXX(LwBool bAssert)
{
    LwU32 gpcReset;

    gpcReset = REG_RD32(BAR0, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL);

    if (bAssert)
    {
        // Assert GPC Engine and Context Reset
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,
                               _GPC_ENGINE_RESET, _ENABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,
                               _GPC_CONTEXT_RESET, _ENABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,
                               _ZLWLL_RT_RESET, _ENABLED, gpcReset);
    }
    else
    {
        // De-assert GPC Engine and Context Reset
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,
                               _GPC_ENGINE_RESET, _DISABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,
                               _GPC_CONTEXT_RESET, _DISABLED, gpcReset);
        gpcReset = FLD_SET_DRF(_PGRAPH, _PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL,
                               _ZLWLL_RT_RESET, _DISABLED, gpcReset);
    }

    REG_WR32(BAR0, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL, gpcReset);

    // Force read for flush
    REG_RD32(BAR0, LW_PGRAPH_PRI_GPCS_GPCCS_CTXSW_GPC_RESET_CTL);
}

/*!
 * Asserts FECS reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertFECSReset_GMXXX(LwBool bAssert)
{
    LwU32 fecsReset;

    fecsReset = REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL);

    if (bAssert)
    {
        // Assert FECS Engine and Context Reset
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                            _SYS_ENGINE_RESET, _ENABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                            _SYS_CONTEXT_RESET, _ENABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                            _BE_ENGINE_RESET, _ENABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                            _BE_CONTEXT_RESET, _ENABLED, fecsReset);
    }
    else
    {
        // De-assert FECS Engine and Context Reset
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _SYS_CONTEXT_RESET, _DISABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _SYS_ENGINE_RESET, _DISABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _BE_CONTEXT_RESET, _DISABLED, fecsReset);
        fecsReset = FLD_SET_DRF(_PGRAPH, _PRI_FECS_CTXSW_RESET_CTL,
                              _BE_ENGINE_RESET, _DISABLED, fecsReset);
    }

    REG_WR32(BAR0, LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL, fecsReset);

    // Force read for flush
    REG_RD32(BAR0, LW_PGRAPH_PRI_FECS_CTXSW_RESET_CTL);
}

/*!
 * Asserts BECS reset
 *
 * @param[in] bAssert   True to assert, False otherwise
 */
void
grAssertBECSReset_GMXXX(LwBool bAssert)
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

/*!
* @brief Reset BL[CP]G controller
*
* @bug 754593 : WAR BLxG controller needs to be reset to avoid
* GBL_WRITE_ERROR_GPC.
*/
void
grResetBlxgWar_GMXXX(void)
{
    // disable BLxG controller
    REG_FLD_WR_DRF_DEF(BAR0, _PMC, _ENABLE, _BLG, _DISABLED);

    //
    // enable BLxG controller
    // read PMC_ENABLE to prevent the writes to this register from arriving on
    // back to back cycles, which can cause the reset pulse to be short enough
    // that the reset domain does not see it.
    //
    // Note that this is not strictly needed for PMU as only non-posted traffic
    // can be issued, but since it's generally a good practice, we follow this
    // trend here.
    //

    REG_FLD_WR_DRF_DEF(BAR0, _PMC, _ENABLE, _BLG, _ENABLED);
}

/*!
 * @brief Initializes the idle mask for GR engine.
 */
void
grInitSetIdleMasks_GMXXX(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;

    im0 = DRF_DEF(_PPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_PPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_PPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED) |
          DRF_DEF(_PPWR, _PMU_IDLE_MASK, _GR_BE,  _ENABLED);

    im1 = DRF_DEF(_PPWR, _PMU_IDLE_MASK_1, _GR_INTR_NOSTALL,  _ENABLED);

    if (FIFO_IS_ENGINE_PRESENT(CE2))
    {
        im0 = FLD_SET_DRF(_PPWR, _PMU_IDLE_MASK, _CE_2,
                                _ENABLED, im0);
        im1 = FLD_SET_DRF(_PPWR, _PMU_IDLE_MASK_1,
                                _CE2_INTR_NOSTALL, _ENABLED, im1);
    }

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
}
