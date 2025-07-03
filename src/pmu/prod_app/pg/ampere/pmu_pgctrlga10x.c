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
 * @file   pmu_pgctrlga10x.c
 * @brief  PMU PG Controller Operation (Ampere)
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"

#include "config/g_pg_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief Configure Auto-wakeup functionality
 *
 * We do the following -
 *  1. Update the Auto-wakeup interval
 *  2. Enable the Auto-wakeup functionality
 *
 * NOTE - We configure/enable Auto-wakeup only when
 *        the wakeup interval is set greater than 0
 *
 * @param[in]  ctrlId   PG controller id
 */
void
pgCtrlAutoWakeupConfig_GA10X
(
    LwU32  ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       hwEngIdx = PG_GET_ENG_IDX(ctrlId);
    LwU32       data;

    // Check if auto wakeup interval is greater than 0
    if (pPgState->autoWakeupIntervalMs > 0)
    {
        //
        // Update Auto-wakeup interval value
        // Register accepts the value in usec
        //
        data = REG_RD32(CSB, LW_CPWR_PMU_PG_WAKEUP_INTERVAL(hwEngIdx));
        data = FLD_SET_DRF_NUM(_CPWR, _PMU_PG_WAKEUP_INTERVAL, _VAL,
                               pPgState->autoWakeupIntervalMs * 1000, data);
        REG_WR32(CSB, LW_CPWR_PMU_PG_WAKEUP_INTERVAL(hwEngIdx), data);

        // Enable Auto-wakeup functionality
        data = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx));
        data = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _AUTO_WAKEUP,
                           _EN, data);
        REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL(hwEngIdx), data);
    }
}

/*!
 * @brief Determine if Auto-wakeup is asserted
 *
 * @param[in]  ctrlId   PG controller id
 *
 * @return   LW_TRUE    If Auto-wakeup is asserted
 *           LW_FALSE   otherwise
 */
LwBool
pgCtrlIsAutoWakeupAsserted_GA10X
(
    LwU32  ctrlId
)
{
    LwU32 data;
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);

    data = REG_RD32(CSB, LW_CPWR_PMU_PG_STAT(hwEngIdx));

    return FLD_TEST_DRF(_CPWR, _PMU_PG_STAT, _AUTO_WAKEUP,
                        _ASSERTED, data);
}

/*!
 * @brief Initialize the Min/Max idle threshold for LPWR CTRLs
 *
 * @param[in]  ctrlId   PG controller id
 *
 */
void
pgCtrlMinMaxIdleThresholdInit_GA10X
(
    LwU32  ctrlId
)
{
    OBJPGSTATE *pPgState  = GET_OBJPGSTATE(ctrlId);
    LwU32       pwrClkMHz = PMU_GET_PWRCLK_HZ() / (1000000);

    // colwert the minimum idle threshold time in terms of power clock cycles
    pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_PG_CTRL_IDLE_THRESHOLD_MINIMUM_US);

    // colwert the maximum idle threshold time in terms of power clock cycles
    pPgState->thresholdCycles.maxIdle = (pwrClkMHz * RM_PMU_PG_CTRL_IDLE_THRESHOLD_MAXIMUM_US);

    switch (ctrlId)
    {
        case RM_PMU_LPWR_CTRL_ID_GR_RG:
        {
            // colwert the minimum idle threshold time in terms of power clock cycles
            pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_GR_RG_IDLE_THRESHOLD_MINIMUM_US);

            // colwert the maximum idle threshold time in terms of power clock cycles
            pPgState->thresholdCycles.maxIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_GR_RG_IDLE_THRESHOLD_MAXIMUM_US);

            break;
        }
        case RM_PMU_LPWR_CTRL_ID_MS_MSCG:
        {
            // colwert the minimum idle threshold time in terms of power clock cycles
            pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_MS_GA10X_IDLE_THRESHOLD_MINIMUM_US);
            break;
        }
        case RM_PMU_LPWR_CTRL_ID_EI:
        {
            // colwert the minimum idle threshold time in terms of power clock cycles
            pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_EI_IDLE_THRESHOLD_MINIMUM_US);

            // colwert the maximum idle threshold time in terms of power clock cycles
            pPgState->thresholdCycles.maxIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_EI_IDLE_THRESHOLD_MAXIMUM_US);

           break;
        }
        case RM_PMU_LPWR_CTRL_ID_DFPR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR))
            {
                // colwert the minimum idle threshold time in terms of power clock cycles
                pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_DFPR_IDLE_THRESHOLD_MINIMUM_US);

                // colwert the maximum idle threshold time in terms of power clock cycles
                pPgState->thresholdCycles.maxIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_DFPR_IDLE_THRESHOLD_MAXIMUM_US);
            }

           break;
        }
        case RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR))
            {
                // colwert the minimum idle threshold time in terms of power clock cycles
                pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_MS_DIFR_SW_ASR_IDLE_THRESHOLD_MINIMUM_US);
            }

           break;
        }
        case RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_CG))
            {
                // colwert the minimum idle threshold time in terms of power clock cycles
                pPgState->thresholdCycles.minIdle = (pwrClkMHz * RM_PMU_LPWR_CTRL_MS_DIFR_CG_IDLE_THRESHOLD_MINIMUM_US);
            }

           break;
        }
    }
}
