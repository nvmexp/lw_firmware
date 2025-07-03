/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_thrm_pwr_ba15.c
 * @brief   PMU HAL functions for BA v1.5 power device (GV10X_and_later)
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "volt/objvolt.h"
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
thermPwrDevBaLoad_BA15
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA15HW  *pBa15Hw = (PWR_DEVICE_BA15HW *)pDev;
    OSTASK_OVL_DESC     ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibFactorA)
    };

    if (pBa15Hw->bFactorALutEnable)
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            thermInitFactorALut_HAL(&Therm, pDev);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG1(pBa15Hw->super.windowIdx),
             pBa15Hw->super.configuration);
    //
    // Since for Maxwell A and C have moved to the beginning and
    // BA is being scaled by shiftA we need to compensate for that
    // by right-shifting the scaled product of A*BA by the same 
    // amount.
    //
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa15Hw->super.windowIdx),
             FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8, _BA_SUM_SHIFT,
                             pBa15Hw->super.shiftA,
                             REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa15Hw->super.windowIdx))));

    // Initialize FACTOR C to 0
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa15Hw->super.windowIdx), 0);

    REG_WR32(CSB, LW_CPWR_THERM_CONFIG1,
             FLD_SET_DRF(_CPWR_THERM, _CONFIG1, _BA_ENABLE, _YES,
                         REG_RD32(CSB, LW_CPWR_THERM_CONFIG1)));

    // Setting up CONFIG11/12 for EDPp Support
    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG11(pBa15Hw->super.windowIdx),
                         pBa15Hw->super.edpPConfigC0C1);

    REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG12(pBa15Hw->super.windowIdx),
                         pBa15Hw->super.edpPConfigC2Dba);

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
thermPwrDevBaLimitSet_BA15
(
    PWR_DEVICE *pDev,
    LwU8        limitIdx,
    LwU32       limitValue
)
{
    PWR_DEVICE_BA15HW  *pBa15Hw = (PWR_DEVICE_BA15HW *)pDev;
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
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG2(pBa15Hw->super.windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_2H:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG3(pBa15Hw->super.windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_1L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG4(pBa15Hw->super.windowIdx);
            break;
        case LW_CPWR_THERM_BA1XHW_THRESHOLD_2L:
            address = LW_CPWR_THERM_PEAKPOWER_CONFIG5(pBa15Hw->super.windowIdx);
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
thermPwrDevBaGetReading_BA15
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU32      *pReading
)
{
    FLCN_STATUS         status  = FLCN_ERR_NOT_SUPPORTED;
    PWR_DEVICE_BA15HW  *pBa15Hw = (PWR_DEVICE_BA15HW *)pDev;
    LwU32               totalmX;

    totalmX =
        DRF_VAL(_CPWR_THERM, _PEAKPOWER_CONFIG10, _WIN_SUM_VALUE,
            REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG10(pBa15Hw->super.windowIdx)));

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
                    LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa15Hw->super.windowIdx)));
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
thermPwrDevBaStateSync_BA15
(
    PWR_DEVICE *pDev,
    LwBool      bVoltageChanged
)
{
    PWR_DEVICE_BA15HW  *pBa15Hw       = (PWR_DEVICE_BA15HW *)pDev;
    VOLT_RAIL          *pRail;
    LwU32               leakagemX;
    LwU32               voltageuV;
    LwU32               factorA;
    LwU32               regVal;
    FLCN_STATUS         status        = FLCN_OK;
    OSTASK_OVL_DESC     ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibFactorA)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pRail  = VOLT_RAIL_GET(pBa15Hw->super.voltRailIdx);
        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            goto thermPwrDevBaStateSync_BA15_detach;
        }

        //
        // Get Voltage and compare it with stored voltage to get the idea that
        // voltage has been changed or not.
        //
        PMU_HALT_OK_OR_GOTO(status,
            voltRailGetVoltage(pRail, &voltageuV),
            thermPwrDevBaStateSync_BA15_detach);

        // If Voltage changed then update stored value and FACTOR A
        if (pBa15Hw->super.voltageuV != voltageuV)
        {
            pBa15Hw->super.voltageuV = voltageuV;
            bVoltageChanged = LW_TRUE;
        }

        //
        // Scaling parameter "A" is updated only on voltage change
        //
        if (bVoltageChanged && !pBa15Hw->bFactorALutEnable)
        {
            factorA = pmgrComputeFactorA(pDev, pBa15Hw->super.voltageuV);
            regVal = REG_RD32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa15Hw->super.windowIdx));
            regVal = FLD_SET_DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG8, _FACTOR_A, factorA, regVal);

            // Update factor "A" in HW 
            REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG8(pBa15Hw->super.windowIdx), regVal);
        }

        //
        // If LeakageEqu is NULL then dont update FACTOR C.
        // Bug 200317824 is having GP100 SRAM Overshoot Issue and solution is to
        // configure BA in a way that Weights which effects SRAM would be valid and
        // others would be zero. For this solution to work, we have to set leakage 
        // as 0 because its a noice in final BA_SUM as other units will report 0.
        //
        if (pBa15Hw->super.pLeakageEqu != NULL)
        {
            //
            // Offset parameter "C" is always updated
            // Callwlate leakage power for given voltage.
            //
            PMU_HALT_OK_OR_GOTO(status,
                voltRailGetLeakage(pRail, pBa15Hw->super.voltageuV,
                    pBa15Hw->super.bLwrrent, NULL, /* PGres = 0.0 */ &leakagemX),
                thermPwrDevBaStateSync_BA15_detach);

            // Update offset "C" in HW (avoid RMW sequence to reduce code size)
            REG_WR32(CSB, LW_CPWR_THERM_PEAKPOWER_CONFIG9(pBa15Hw->super.windowIdx),
                     DRF_NUM(_CPWR_THERM, _PEAKPOWER_CONFIG9, _LEAKAGE_C, leakagemX));
        }

thermPwrDevBaStateSync_BA15_detach:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Check if BA Training DMA Mode is SET in VBIOS or not
 *
 * @return  LW_TRUE     If LW_CPWR_THERM_BA_TRAINING_CTRL_DMA_MODE_VBIOS_SET to _TRUE
 * @return  LW_FALSE    Otherwise
 */
LwBool
thermPwrDevBaTrainingIsDmaSet_BA15()
{
#if !(PMU_PROFILE_GV10X)
    LwU32   data = 0;

    data = REG_RD32(CSB, LW_CPWR_THERM_BA_TRAINING_CTRL);
    if (FLD_TEST_DRF(_CPWR, _THERM_BA_TRAINING_CTRL, _DMA_MODE_VBIOS_SET, _TRUE, data))
    {
        return LW_TRUE;
    }
#endif

    return LW_FALSE;
}

