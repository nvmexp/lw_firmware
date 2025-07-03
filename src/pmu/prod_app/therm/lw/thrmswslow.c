/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrmswslow.c
 * @brief   Thermal RM_PMU_SW_SLOW feature related code
 *
 * The following is a thermal/power control module that aims to control the
 * thermal RM_PMU_SW_SLOW feature on PMU.
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "therm/objtherm.h"

#include "g_pmurpc.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
static void s_thermSlowCtrlSwSlowSync(LwU8 clkIndex)
    // Called from multiple overlays -> resident code.
    GCC_ATTRIB_SECTION("imem_resident", "s_thermSlowCtrlSwSlowSync");

/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Function Prototypes  --------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * @brief   Sets initial state for the SW_SLOW feature.
 */
void
thermSlowCtrlSwSlowInitialize(void)
{
    LwU8    clkIndex;
    LwU8    reason;

    for (clkIndex = 0; clkIndex < RM_PMU_THERM_CLK_COUNT_MAX; clkIndex++)
    {
        Therm.rmPmuSwSlow.clock[clkIndex].bEnabled = LW_TRUE;
        for (reason = 0; reason < THERM_PMU_SW_SLOW_REASON_COUNT; reason++)
        {
            Therm.rmPmuSwSlow.clock[clkIndex].slowdown[reason].factor =
                RM_PMU_THERM_SLOW_FACTOR_DISABLED;
        }
    }
}

/*!
 * @brief   Controls the state of RM_PMU_SW_SLOW feature arbiter.
 *
 * @param[in]   bActivateArbiter    New state of feature arbiter.
 */
void
thermSlowCtrlSwSlowActivateArbiter
(
    LwBool bActivateArbiter
)
{
    LwU8    clkIndex;

    Therm.rmPmuSwSlow.bArbiterActive = bActivateArbiter;

    for (clkIndex = 0; clkIndex < RM_PMU_THERM_CLK_COUNT_MAX; clkIndex++)
    {
        if (Therm.rmPmuSwSlow.bArbiterActive)
        {
            s_thermSlowCtrlSwSlowSync(clkIndex);
        }
        else
        {
            // When deactivating we preserve SW state and reset HW settings.
            thermSlctRmPmuSwSlowHwSet_HAL(&Therm, clkIndex,
                                          RM_PMU_THERM_SLOW_FACTOR_DISABLED);
        }
    }
}

/*!
 * @brief   Exelwtes SW_SLOW_SET RPC.
 *
 * Exelwtes SW_SLOW_SET RPC by accepting new RM settings and then reevaluating
 * and applying new HW state.
 */
FLCN_STATUS
pmuRpcThermSwSlowSet
(
    RM_PMU_RPC_STRUCT_THERM_SW_SLOW_SET *pParams
)
{
    Therm.rmPmuSwSlow.clock[pParams->clkIndex].bEnabled = pParams->bEnabled;
    Therm.rmPmuSwSlow.clock[pParams->clkIndex].slowdown[THERM_PMU_SW_SLOW_REASON_RM_NORMAL].factor =
        pParams->factorNormal;
    Therm.rmPmuSwSlow.clock[pParams->clkIndex].slowdown[THERM_PMU_SW_SLOW_REASON_RM_FORCED].factor =
        pParams->factorForced;
    Therm.rmPmuSwSlow.clock[pParams->clkIndex].slowdown[THERM_PMU_SW_SLOW_REASON_RM_FORCED].bAlwaysActive =
        LW_TRUE;

    s_thermSlowCtrlSwSlowSync(pParams->clkIndex);

    return FLCN_OK;
}

/* ------------------------ Static Functions ------------------------------- */
/*!
 * @brief   Forces reevaluation of the RM_PMU_SW_SLOW settings for the given
 *          CLK domain.
 *
 * @note    This function is resident and can be called by any PMU task,
 *          but not ISRs.
 *
 * @note    With PMUCFG_FEATURE_ENABLED(PMU_CLK_NDIV_SLOWDOWN) this function
 *          can call a function in the overlay12, so must only be called from
 *          the THERM task.
 *
 * @param[in]   clkIndex        Thermal index of target CLK
 */
static void
s_thermSlowCtrlSwSlowSync
(
    LwU8    clkIndex
)
{
    LwU8    reason;
    LwU8    factorLwrrent;
    LwU8    factorMax = RM_PMU_THERM_SLOW_FACTOR_DISABLED;

    // When PMU arbiter is inactive we maintain SW state and skip HW updates.
    if (!Therm.rmPmuSwSlow.bArbiterActive)
    {
        return;
    }

    for (reason = 0; reason < THERM_PMU_SW_SLOW_REASON_COUNT; reason++)
    {
        if (Therm.rmPmuSwSlow.clock[clkIndex].bEnabled ||
            Therm.rmPmuSwSlow.clock[clkIndex].slowdown[reason].bAlwaysActive)
        {
            factorLwrrent =
                Therm.rmPmuSwSlow.clock[clkIndex].slowdown[reason].factor;
            factorMax = LW_MAX(factorLwrrent, factorMax);
        }
    }

    thermSlctRmPmuSwSlowHwSet_HAL(&Therm, clkIndex, factorMax);
}

