/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ba15hw.c
 * @brief   Block Activity v1.5 HW sensor/controller.
 *
 * @implements PWR_DEVICE
 *
 * @note    Only supports/implements single PWR_DEVICE Provider - index 0.
 *          Provider index parameter will be ignored in all interfaces.
 *
 * @note    Is a subset of BA1XHW device, and used for energy alwmmulation alone.
 *
 * @note    For information on BA1XHW device see RM::pwrdev_ba1xhw.c
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba1xhw.h"
#include "dev_pwr_csb.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */
/*!
 * Construct a BA15HW PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_BA15HW
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_BA15HW                               *pBa15Hw;
    RM_PMU_PMGR_PWR_DEVICE_DESC_BA15HW_BOARDOBJ_SET *pBa15HwSet =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_BA15HW_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                      status = FLCN_OK;

    // Construct and initialize parent-class object.
    status = pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA15HW_done;
    }
    pBa15Hw = (PWR_DEVICE_BA15HW *)*ppBoardObj;

    pBa15Hw->gpuAdcDevIdx       = pBa15HwSet->gpuAdcDevIdx;
    pBa15Hw->bFactorALutEnable  = pBa15HwSet->bFactorALutEnable;
    pBa15Hw->bLeakageCLutEnable = pBa15HwSet->bLeakageCLutEnable;

pwrDevGrpIfaceModel10ObjSetImpl_BA15HW_done:
    return status;
}

/*!
 * BA15HW power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_BA15HW
(
    PWR_DEVICE *pDev
)
{
    PWR_DEVICE_BA15HW  *pBa15Hw = (PWR_DEVICE_BA15HW *)pDev;
    LwU8                limitIdx;

    // Cache "shiftA" since it will not change.
    pBa15Hw->super.shiftA =
            pwrEquationBA15ScaleComputeShiftA(
                (PWR_EQUATION_BA15_SCALE *)pBa15Hw->super.pScalingEqu,
                pBa15Hw->super.bLwrrent);

    // Reset limit thresholds uppon load()
    for (limitIdx = 0; limitIdx < PWR_DEVICE_BA1XHW_LIMIT_COUNT; limitIdx++)
    {
        thermPwrDevBaLimitSet_HAL(&Therm, pDev, limitIdx, PWR_DEVICE_NO_LIMIT);
    }

    return thermPwrDevBaLoad_HAL(&Therm, pDev);
}

/*!
 * BA15HW power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_BA15HW
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    FLCN_STATUS status;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_GPUADC1X) &&
        (limitIdx >= PWR_DEVICE_BA1XHW_LIMIT_COUNT) &&
        (limitIdx < RM_PMU_PMGR_PWR_DEVICE_GPUADC1X_EXTENDED_THRESHOLD_NUM))
    {
        PWR_DEVICE_BA15HW   *pBa15Hw  = (PWR_DEVICE_BA15HW *)pDev;
        PWR_DEVICE_GPUADC1X *pGpuAdc1x;

        // Sanity check GPUADC device index
        pGpuAdc1x = (PWR_DEVICE_GPUADC1X *)PWR_DEVICE_GET(pBa15Hw->gpuAdcDevIdx);
        if (pGpuAdc1x == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrDevSetLimit_BA15HW_exit;
        }

        limitIdx -= PWR_DEVICE_BA1XHW_LIMIT_COUNT;

        // The GPUADC1X interface will call the GPUADC type-specific function
        status = pwrDevGpuAdc1xBaSetLimit(pGpuAdc1x, pBa15Hw->super.windowIdx,
                     limitIdx, limitUnit, limitValue);
    }
    else if (limitIdx < PWR_DEVICE_BA1XHW_LIMIT_COUNT)
    {
        status = pwrDevSetLimit_BA1XHW(pDev, provIdx, limitIdx, limitUnit,
                                     limitValue);
    }
    else
    {
        // limitIdx is outside expected range
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
    }


pwrDevSetLimit_BA15HW_exit:
    return status;
}

/*!
 * Setting related to enablement of BA DMA Mode
 */
void
pwrDevBaDmaMode_BA15HW(void)
{
    if (thermPwrDevBaTrainingIsDmaSet_HAL())
    {
        REG_WR32(CSB, LW_CPWR_FBIF_TRANSCFG(RM_PMU_DMAIDX_END),
            (DRF_DEF(_CPWR, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
             DRF_DEF(_CPWR, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB)));
    }
}
