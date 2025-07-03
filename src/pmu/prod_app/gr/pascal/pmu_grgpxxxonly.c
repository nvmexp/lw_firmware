/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_grgpxxxonly.c
 * @brief  HAL-interface for the GP10X.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_pwr_csb.h"
#include "dev_graphics_nobundle.h"
#include "dev_master.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objgr.h"
#include "pmu_objfifo.h"
#include "pmu_bar0.h"

#include "dbgprintf.h"
#include "config/g_gr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------  Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes --------------------------------------*/
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Initializes the idle mask for GR.
 */
void
grInitSetIdleMasks_GPXXX(void)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_GR_PG);
    LwU32       im0      = 0;
    LwU32       im1      = 0;

    im0 = DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR,     _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_SYS, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_GPC, _ENABLED) |
          DRF_DEF(_CPWR, _PMU_IDLE_MASK, _GR_BE,  _ENABLED);

    im1 = DRF_DEF(_CPWR, _PMU_IDLE_MASK_1, _GR_INTR_NOSTALL,  _ENABLED);

    if (FIFO_IS_ENGINE_PRESENT(CE0))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_0, _ENABLED, im0);
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1,
                                 _CE0_INTR_NOSTALL, _ENABLED, im1);
    }

    if (FIFO_IS_ENGINE_PRESENT(CE1))
    {
        im0 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK, _CE_1, _ENABLED, im0);
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1,
                                _CE1_INTR_NOSTALL, _ENABLED, im1);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2_RTOS_WAR_200089154))
    {
        im1 = FLD_SET_DRF(_CPWR, _PMU_IDLE_MASK_1, _SW0, _ENABLED, im1);
    }

    pPgState->idleMask[0] = im0;
    pPgState->idleMask[1] = im1;
}

/*!
 * @brief Check for graphics sub system interrupts
 *
 * @return LW_TRUE  If Interrupt pending on GR
 *         LW_FALSE Otherwise
 */
LwBool
grIsIntrPending_GPXXX(void)
{
    LwU32 intr0;
    LwU32 intr1;

    intr0 = REG_RD32(BAR0, LW_PMC_INTR(0));
    intr1 = REG_RD32(BAR0, LW_PMC_INTR(1));

    if (FLD_TEST_DRF(_PMC, _INTR, _PGRAPH, _PENDING, intr0) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE0,    _PENDING, intr0) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE1,    _PENDING, intr0) ||
        FLD_TEST_DRF(_PMC, _INTR, _PGRAPH, _PENDING, intr1) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE0,    _PENDING, intr1) ||
        FLD_TEST_DRF(_PMC, _INTR, _CE1,    _PENDING, intr1))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}
/* ------------------------- Private Functions ------------------------------ */
