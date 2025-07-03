/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cg_elcggpxxxadxxx.c
 * @brief  ELCG related functions.
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_therm.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_objfifo.h"
#include "pmu_cg.h"
#include "config/g_lpwr_private.h"

//
// Compile time check to ensure LW_CPWR_THERM_GATE_CTRL__SIZE_1 is
// supported by SW framework
//
ct_assert(LW_CPWR_THERM_GATE_CTRL__SIZE_1 <= RM_PMU_LPWR_CG_ELCG_CTRL__COUNT);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief ELCG Pre-initialization
 */
void
lpwrCgElcgPreInit_GP10X(void)
{
    LwU32 regVal;
    LwU32 engHwId;

    // Initialize all ELCG domains
    for (engHwId = 0; engHwId < LW_CPWR_THERM_GATE_CTRL__SIZE_1; engHwId++)
    {
        regVal = THERM_GATE_CTRL_REG_RD32(engHwId);

        // Initialize Idle threshold
        regVal = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_IDLE_FILT_EXP,
                               __PROD, regVal);
        regVal = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_IDLE_FILT_MANT,
                               __PROD, regVal);

        // Initialize delays for CG
        regVal = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_DELAY_BEFORE,
                               __PROD, regVal);
        regVal = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_DELAY_AFTER,
                               __PROD, regVal);

        // Ensure that ELCG is by default disabled
        regVal = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK,
                             _RUN, regVal);

        THERM_GATE_CTRL_REG_WR32(engHwId, regVal);
    }

    // Initialize FECS idle filter
    regVal = REG_RD32(CSB, LW_CPWR_THERM_FECS_IDLE_FILTER);
    regVal = FLD_SET_DRF(_CPWR_THERM, _FECS_IDLE_FILTER, _VALUE,
                         __PROD, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_FECS_IDLE_FILTER, regVal);

    // Initialize HUBMMU idle filter
    regVal = REG_RD32(CSB, LW_CPWR_THERM_HUBMMU_IDLE_FILTER);
    regVal = FLD_SET_DRF(_CPWR_THERM, _HUBMMU_IDLE_FILTER, _VALUE,
                         __PROD, regVal);
    REG_WR32(CSB, LW_CPWR_THERM_HUBMMU_IDLE_FILTER, regVal);
}
