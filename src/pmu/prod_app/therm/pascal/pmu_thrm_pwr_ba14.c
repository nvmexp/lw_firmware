/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_pwr_ba14.c
 * @brief   PMU Energy HAL functions for BA v1.4 power device (GP10X except GP100)
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "volt/objvolt.h"
#include "pmgr/pwrdev_ba13hw.h"
#include "pmu_objpmgr.h"

#include "config/g_therm_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Compile-time checks----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
thermPwrDevBaLoad_BA14
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;
    FLCN_STATUS         status  = FLCN_OK;

    status = thermPwrDevBaLoad_BA13(pDev);

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_ENERGY_COUNTER) && (status == FLCN_OK))
    {
        REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_DEBUG_CTRL(pBa1xHw->windowIdx),
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_DEBUG_CTRL, _BA_RESULT_RST, _TRIGGER) |
            DRF_DEF(_CPWR_THERM, _PEAKPOWER_DEBUG_CTRL, _LOGGING, _ENABLED));
    }

    return FLCN_OK;
}

/*!
 * @brief   Read energy from BA power device
 *
 * @param[in]   pDev        Pointer to a PWR_DEVICE object
 * @param[in]   provIdx     Index of the requested provider
 * @param[out]  pReading    Buffer to store retreived energy reading
 *
 * @pre Requires libLw64 and libLw64Umul to be loaded before invocation
 */
FLCN_STATUS
thermPwrDevBaGetEnergyReading_BA14
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU64      *pReading
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw   = (PWR_DEVICE_BA1XHW *)pDev;
    FLCN_STATUS         status    = FLCN_OK;
    LwU64               result    = 0;
    LwU32               totalBAmW = 0;
    LwU64               arg1;
    LwU64               arg2;
    LwU32               winPeriodPow;

    // Only provider for TOTAL_POWER supported  for BA14 energy
    if (provIdx != RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_TOTAL_POWER)
    {
        PMU_HALT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto thermPwrDevBaGetEnergyReading_BA14_exit;
    }

    // Needs utilsclock value for BA energy callwlation
    if (Pmgr.baInfo.utilsClkkHz == 0)
    {
        PMU_HALT();
        status = FLCN_ERR_ILWALID_STATE;
        goto thermPwrDevBaGetEnergyReading_BA14_exit;
    }

    //
    // NOTE: ns unit had to be used in order to accommodate all possible
    // configurations for WINDOW_PERIOD.
    // stepPeriodNs = windowPeriodNs / 64
    // windowPeriodNs  = ((2 ^ CONFIG1(w)) * utilsPeriodNs
    // utilsPeriodNs = 1000 * 1000 * 1000 / utilsClkHz
    // => windowPeriodNs = ((2 ^ CONFIG1(w)) * 1000 * 1000 * 1000) / utilsClkHz
    // stepPeriodNs = ((2 ^ CONFIG1(w)) * 1000 * 1000 * 1000) / (utilsClkHz * 64)
    // 1. stepPeriodNs = ((2 ^ CONFIG1(w)) * 1000 * 1000) / (utilsClkKHz * 64)
    // 2. totalDelEnergy(pJ) = stepPeriodNs * totalBAmW
    // =  ((2 ^ CONFIG1(w)) * 1000 * 1000) * totalBAmW / (utilsClkKHz * 64)
    // 3. totalDelEnergy(mJ) = totalDelEnergy(pJ) / 1000000000
    // =  (2 ^ CONFIG1(w)) * totalBAmW) / (utilsClkKHz * 64 * 1000)
    //
    winPeriodPow = DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG1, _WINDOW_PERIOD,
        REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(pBa1xHw->windowIdx)));

    // Snap the value and reset register for next reading
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_DEBUG_CTRL(pBa1xHw->windowIdx),
                FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_DEBUG_CTRL, _BA_RESULT_RST, _TRIGGER,
                FLD_SET_DRF(_CPWR_THERM, _PEAKPOWER_DEBUG_CTRL, _BA_RESULT_SNAP,
                            _TRIGGER, REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_DEBUG_CTRL(pBa1xHw->windowIdx)))));

    //
    // The PEAKPOWER_DEBUG1 gets updated every 2 ^ CONFIG1_BA_WINDOW_SIZE * 9.26ns
    // So it might overflow between 0.5 msec (SIZE_MIN = 1) and a worst case power 150W
    // to a really long time in days 2^64 * 9.26ns ( SIZE_MAX = 64) for the same worst case power.
    //

    // Total BA SUM (Dyn + Leakage) in mW
    totalBAmW =
        DRF_VAL(_CPWR_THERM, _PEAKPOWER_DEBUG1, _TOTAL_POWER,
            REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_DEBUG1(pBa1xHw->windowIdx)));

    // In step (3) above (2 ^ CONFIG1(w)) * totalBAmW)
    arg1 = ((LwU64)1 << winPeriodPow);
    arg2 = (LwU64)totalBAmW;
    lwU64Mul(&result, &arg1, &arg2);

    // In step (3) above (2 ^ CONFIG1(w)) * totalBAmW)/(utilsClkKHz)
    arg1 = result;
    arg2 = (LwU64)Pmgr.baInfo.utilsClkkHz;
    lwU64Div(&result, &arg1, &arg2);

    // In step 3 above (2 ^ CONFIG1(w)) * totalBAmW)/(utilsClkKHz)/(64 * 1000)
    arg2 = 64000;
    lwU64Div(pReading, &result, &arg2);

thermPwrDevBaGetEnergyReading_BA14_exit:
    return status;
}
