/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_cg_elcggk10x.c
 * @brief  LPWR ELCG Operation
 *
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
#include "dbgprintf.h"

#include "config/g_lpwr_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Private Prototypes ----------------------------- */
FLCN_STATUS s_lpwrCgGrElcgStateChange_GMXXX(LwBool bEnable);
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief ELCG post initialization
 */
FLCN_STATUS
lpwrCgElcgPostInit_GM10X(void)
{
    // Cache the current state of GR-ELCG as part of GR-ELCG <-> OSM CG WAR.
    if (PMUCFG_FEATURE_ENABLED(PMU_CG_GR_ELCG_OSM_WAR))
    {
        Lpwr.grElcgState = THERM_GATE_CTRL_REG_RD32(GET_FIFO_ENG(PMU_ENGINE_GR));
    }

    return FLCN_OK;
}

/*!
 * @brief API to enable ELCG Ctrl using HW engine ID
 *
 * @param[in] engHwId   Engine ID
 * @param[in] reasonId  Enable reason ID
 *
 * @return  FLCN_OK on success
 */
FLCN_STATUS
lpwrCgElcgCtrlEnable_GM10X
(
    LwU32 engHwId,
    LwU32 reasonId
)
{
    LwU32       engHwIdGr = GET_FIFO_ENG(PMU_ENGINE_GR);
    FLCN_STATUS status    = FLCN_OK;

    if ((PMUCFG_FEATURE_ENABLED(PMU_CG_GR_ELCG_OSM_WAR)) &&
        (engHwId == engHwIdGr))
    {
        Lpwr.grElcgDisableReasonMask &= ~BIT(reasonId);

        if (Lpwr.grElcgDisableReasonMask == 0)
        {
            status = s_lpwrCgGrElcgStateChange_GMXXX(LW_TRUE);
        }
    }
    else
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief API to Disable ELCG Ctrl using HW engine ID
 *
 * @param[in] engHwId   Engine ID
 * @param[in] reasonId  Disable reason ID
 *
 * @return  FLCN_OK on success
 */
FLCN_STATUS
lpwrCgElcgCtrlDisable_GM10X
(
    LwU32 engHwId,
    LwU32 reasonId
)
{
    LwU32       engHwIdGr = GET_FIFO_ENG(PMU_ENGINE_GR);
    FLCN_STATUS status    = FLCN_OK;

    if ((PMUCFG_FEATURE_ENABLED(PMU_CG_GR_ELCG_OSM_WAR)) &&
        (engHwId == engHwIdGr))
    {
        if (Lpwr.grElcgDisableReasonMask == 0)
        {
            status = s_lpwrCgGrElcgStateChange_GMXXX(LW_FALSE);
        }

        Lpwr.grElcgDisableReasonMask |= LWBIT(reasonId);
    }
    else
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Enable/Disable GR-ELCG State machine
 *
 * This function enables/disables GR-ELCG SM as part of ELCG<->OSM CG WAR. This
 * WAR disables GR-ELCG SM before gating GPC and LTC clocks from OSM. It restores
 * state of GR-ELCG SM after ungating clocks from OSM.
 *
 * Refer @lpwr_cg.c for details.
 *
 * @param[in] bEnable     - LW_TRUE  -> Restore GR-ELCG state
 *                          LW_FALSE -> Disable GR-ELCG
 */
FLCN_STATUS
s_lpwrCgGrElcgStateChange_GMXXX
(
    LwBool bEnable
)
{
    LwU32 engHwIdGr = GET_FIFO_ENG(PMU_ENGINE_GR);
    LwU32 gateCtrl;

    // Enable request restores the value of GR-ELCG
    if (bEnable)
    {
        // Restore value from cache
        gateCtrl = Lpwr.grElcgState;
    }
    else
    {
        // Read current state of GR-ELCG
        gateCtrl = THERM_GATE_CTRL_REG_RD32(engHwIdGr);

        // Cache the current state
        Lpwr.grElcgState = gateCtrl;

        // Disable GR ELCG SM
        gateCtrl = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_CLK, _RUN, gateCtrl);
        gateCtrl = FLD_SET_DRF(_CPWR_THERM, _GATE_CTRL, _ENG_PWR, _ON, gateCtrl);
    }

    THERM_GATE_CTRL_REG_WR32(engHwIdGr, gateCtrl);

    return FLCN_OK;
}
