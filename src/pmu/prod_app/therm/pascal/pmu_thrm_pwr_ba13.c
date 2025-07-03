/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_pwr_ba13.c
 * @brief   PMU HAL functions for BA v1.3 power device (GP10X)
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "volt/objvolt.h"


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
thermPwrDevBaLoad_BA13
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

    // Initialize FACTOR C to 0
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa1xHw->windowIdx), 0);

    // Setting up CONFIG11/12 for EDPp Support
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG11(pBa1xHw->windowIdx),
                         pBa1xHw->edpPConfigC0C1);

    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG12(pBa1xHw->windowIdx),
                         pBa1xHw->edpPConfigC2Dba);

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
thermPwrDevBaLimitSet_BA13
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
thermPwrDevBaGetReading_BA13
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
thermPwrDevBaStateSync_BA13
(
    PWR_DEVICE *pDev,
    LwBool      bVoltageChanged
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw       = (PWR_DEVICE_BA1XHW *)pDev;
    VOLT_RAIL          *pRail;
    LwU32               leakagemX;
    LwU32               voltageuV;
    LwUFXP20_12         scaleA;
    FLCN_STATUS         status        = FLCN_OK;
    OSTASK_OVL_DESC     ovlDescList[] = {
#ifdef DMEM_VA_SUPPORTED
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
#endif
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pRail  = VOLT_RAIL_GET(pBa1xHw->voltRailIdx);
        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            goto thermPwrDevBaStateSync_BA13_detach;
        }

        //
        // Get Voltage and compare it with stored voltage to get the idea that
        // voltage has been changed or not.
        //
        PMU_HALT_OK_OR_GOTO(status,
            voltRailGetVoltage(pRail, &voltageuV),
            thermPwrDevBaStateSync_BA13_detach);

        // If Voltage changed then update stored value and FACTOR A
        if (pBa1xHw->voltageuV != voltageuV)
        {
            pBa1xHw->voltageuV = voltageuV;
            bVoltageChanged = LW_TRUE;
        }

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
        // If LeakageEqu is NULL then dont update FACTOR C.
        // Bug 200317824 is having GP100 SRAM Overshoot Issue and solution is to
        // configure BA in a way that Weights which effects SRAM would be valid and
        // others would be zero. For this solution to work, we have to set leakage 
        // as 0 because its a noice in final BA_SUM as other units will report 0.
        //
        if (pBa1xHw->pLeakageEqu != NULL)
        {
            //
            // Offset parameter "C" is always updated
            // Callwlate leakage power for given voltage.
            //
            PMU_HALT_OK_OR_GOTO(status,
                voltRailGetLeakage(pRail, pBa1xHw->voltageuV, pBa1xHw->bLwrrent,
                                   NULL, /* PGres = 0.0 */ &leakagemX),
                thermPwrDevBaStateSync_BA13_detach);

            // Update offset "C" in HW (avoid RMW sequence to reduce code size)
            REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa1xHw->windowIdx),
                     DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9, _LEAKAGE_C, leakagemX));
        }
thermPwrDevBaStateSync_BA13_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
