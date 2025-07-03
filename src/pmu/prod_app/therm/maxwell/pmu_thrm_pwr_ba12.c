/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_pwr_ba12.c
 * @brief   PMU HAL functions for BA v1.2 power device (GM10X)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"

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
thermPwrDevBaLoad_BA12
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;
  
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(pBa1xHw->windowIdx),
             pBa1xHw->configuration);
    //
    // Since for Maxwell A and C have moved to the beginning and
    // BA is being scaled by shiftA we need to compensate for that
    // by right-shifting the scaled product of A*BA by the same 
    // amount.
    //    
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa1xHw->windowIdx),
             FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8, _BA_SUM_SHIFT,
                             pBa1xHw->shiftA,
                             REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa1xHw->windowIdx))));

    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1,
             FLD_SET_DRF(_CPWR_THERM, _CONFIG1, _BA_ENABLE, _YES,
                         REG_RD32(CSB, LW_CPWR_THERM_CONFIG1)));

    // Enable interrupts for BA(windowIdx) window.
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG6(pBa1xHw->windowIdx),
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG6, _TRIGGER_CFG_1H, _INTR) |
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG6, _TRIGGER_CFG_1L, _INTR));
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG7(pBa1xHw->windowIdx),
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG7, _TRIGGER_CFG_2H, _INTR) |
        DRF_DEF(_CPWR_THERM, _PEAKPOWER_CONFIG7, _TRIGGER_CFG_2L, _INTR));

    return FLCN_OK;
}
/*!
 * @brief   Update BA power device limit's threshold
 *
 * @param[in]   pDev        Pointer to a PWR_DEVICE object
 * @param[in]   limitIdx    Index of the limit threshold
 * @param[in]   limitValue  Limit value to apply
 */
void
thermPwrDevBaLimitSet_BA12
(
    PWR_DEVICE *pDev,
    LwU8        limitIdx,
    LwU32       limitValue
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;
    LwU32               address = 0;
    LwU32               reg32;

    if (limitValue == PWR_DEVICE_NO_LIMIT)
    {
        reg32 = DRF_DEF(_CPWR_THERM, _BA1XHW_THRESHOLD, _EN, _DISABLED);
    }
    else
    {
        reg32 = DRF_NUM(_CPWR_THERM, _BA1XHW_THRESHOLD, _VAL, limitValue);
        reg32 =
            FLD_SET_DRF(_CPWR_THERM, _BA1XHW_THRESHOLD, _EN, _ENABLED, reg32);
    }

    switch (limitIdx)
    {
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_1H:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG2(pBa1xHw->windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_2H:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG3(pBa1xHw->windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_1L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG4(pBa1xHw->windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_2L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG5(pBa1xHw->windowIdx);
            break;
        default:
            PMU_HALT();
            break;
    }

    REG_WR32(CSB, address, reg32);
}

/*!
 * @brief   Read power/current from BA power device
 *
 * @param[in]   pDev        Pointer to a PWR_DEVICE object
 * @param[in]   provIdx     Index of the requested provider
 * @param[in]   pReading    Buffer to store retrieved power/current reading
 *
 * @return  FLCN_OK                 Reading succesfully retrieved
 * @return  FLCN_ERR_NOT_SUPPORTED   Invalid provider index
 */
FLCN_STATUS
thermPwrDevBaGetReading_BA12
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU32      *pReading
)
{
    FLCN_STATUS         status  = FLCN_ERR_NOT_SUPPORTED;
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;
    LwU32               totalmX;

    totalmX =
        DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG10, _WIN_SUM_VALUE,
            REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG10(pBa1xHw->windowIdx)));

    // TODO: Hack exposing current estimations through power interface
    if ((provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_TOTAL_POWER) ||
        (provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_TOTAL_LWRRENT))
    {
        *pReading = totalmX;
        status = FLCN_OK;
    }
    // TODO: Hack exposing current estimations through power interface
    else if ((provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_DYNAMIC_POWER) ||
             (provIdx == RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_DYNAMIC_LWRRENT))
    {
        totalmX -=
            DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG9, _LEAKAGE_C,
                REG_RD32(CSB,
                    LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa1xHw->windowIdx)));
        *pReading = totalmX;
        status = FLCN_OK;
    }

    return status;
}

/*!
 * @brief   Update BA power device's internal state (A&C)
 *
 * This interface is designed to be called in following two situations:
 * - on each change of GPU core voltage (both A & C parameters require update)
 * - periodically to compensate for temperature changes (C parameter only)
 *
 * RM precomputes parameters shiftA/constA and final PMU formulas look like:
 * - current mode:
 *   A(hw) = (constA * core_Voltage) << shiftA
 *   C(hw) = leakage(mA)
 * - power mode:
 *   A(hw) = (constA * core_Voltage^2) << shiftA
 *   C(hw) = leakage(mW)
 *
 * @note    Detail explanation of formulas used for BA11HW device are located
 *          within RM code that precomputes constant parameters constA/constH
 *
 * @param[in]   pDev            Pointer to a PWR_DEVICE object
 * @param[in]   bVoltageChanged Set if update is triggered by voltage change
 */
FLCN_STATUS
thermPwrDevBaStateSync_BA12
(
    PWR_DEVICE *pDev,
    LwBool      bVoltageChanged
)
{
    FLCN_STATUS         status    = FLCN_OK;
    PWR_DEVICE_BA1XHW  *pBa1xHw   = (PWR_DEVICE_BA1XHW *)pDev;
    LwU32               leakagemX = 0;
    LwUFXP20_12         scaleA;

    //
    // Scaling parameter "A" is updated only on voltage change
    //

    if (bVoltageChanged)
    {
        scaleA =
            pwrEquationBA1xScaleComputeScaleA(
                (PWR_EQUATION_BA1X_SCALE *)pBa1xHw->pScalingEqu,
                pBa1xHw->constW,
                pBa1xHw->bLwrrent,
                pBa1xHw->voltageuV);
        scaleA <<= pBa1xHw->shiftA;

        // Update scaling "A" in HW 
        REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa1xHw->windowIdx),
                 FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8, _FACTOR_A,
                                 LW_TYPES_UFXP_X_Y_TO_U32(20, 12, scaleA),
                                 REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa1xHw->windowIdx))));
    }
    //
    // Offset parameter "C" is always updated
    //
    // Evaluate leakage equation in desired units
    if (pBa1xHw->bLwrrent)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrEquationLeakageEvaluatemA(
                (PWR_EQUATION_LEAKAGE *)pBa1xHw->pLeakageEqu,
                pBa1xHw->voltageuV,
                NULL /* PGres = 0.0 */, &leakagemX),
            thermPwrDevBaStateSync_BA12_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            pwrEquationLeakageEvaluatemW(
                (PWR_EQUATION_LEAKAGE *)pBa1xHw->pLeakageEqu,
                pBa1xHw->voltageuV,
                NULL /* PGres = 0.0 */, &leakagemX),
            thermPwrDevBaStateSync_BA12_exit);
    }

    // Update offset "C" in HW (avoid RMW sequence to reduce code size)
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa1xHw->windowIdx),
             DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9, _LEAKAGE_C, leakagemX));

thermPwrDevBaStateSync_BA12_exit:
    return status;
}
