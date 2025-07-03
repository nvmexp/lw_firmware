/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrdev_ba1xhw.c
 * @brief   Block Activity v1.X HW sensor/controller.
 *
 * @implements PWR_DEVICE
 *
 * @note    Only supports/implements single PWR_DEVICE Provider - index 0.
 *          Provider index parameter will be ignored in all interfaces.
 *
 * @note    For information on BA1XHW device see RM::pwrdev_ba1xhw.c
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmgr/pwrdev_ba1xhw.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- PWR_DEVICE Interfaces -------------------------- */

/*!
 * Construct a BA1XHW PWR_DEVICE object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE_BA1XHW                               *pBa1xHw;
    RM_PMU_PMGR_PWR_DEVICE_DESC_BA1XHW_BOARDOBJ_SET *pBa1xHwSet =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_BA1XHW_BOARDOBJ_SET *)pBoardObjDesc;
    FLCN_STATUS                                      status = FLCN_OK;

    // Sanity check input
    if (PMUCFG_FEATURE_ENABLED_PWR_DEVICE_PRE_BA13X)
    {
        if (((Pmgr.pwr.equations.objMask.super.pData[0] & BIT(pBa1xHwSet->scalingEquIdx)) == 0) ||
            ((Pmgr.pwr.equations.objMask.super.pData[0] & BIT(pBa1xHwSet->leakageEquIdx)) == 0))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW_exit;
        }
    }
    else if (PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA13X_THRU_BA15)
    {
        if ((Pmgr.pwr.equations.objMask.super.pData[0] & BIT(pBa1xHwSet->scalingEquIdx)) == 0)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (FLCN_OK != thermCheckHwBaWindowIdx_HAL(&Therm, pBa1xHwSet->windowIdx))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW_exit;
        }
    }

    // Construct and initialize parent-class object.
    status = pwrDevGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW_exit;
    }
    pBa1xHw = (PWR_DEVICE_BA1XHW *)*ppBoardObj;

    pBa1xHw->bLwrrent        = pBa1xHwSet->bLwrrent;
    pBa1xHw->windowIdx       = pBa1xHwSet->windowIdx;
    pBa1xHw->configuration   = pBa1xHwSet->configuration;
    pBa1xHw->constW          = pBa1xHwSet->constW;

    if (!(PMUCFG_FEATURE_ENABLED_PWR_DEVICE_PRE_BA13X) &&
        !(PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA13X_THRU_BA15) &&
         (pBa1xHwSet->scalingEquIdx == LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID))
    {
        pBa1xHw->pScalingEqu = NULL;
    }
    else
    {
        pBa1xHw->pScalingEqu = PWR_EQUATION_GET(pBa1xHwSet->scalingEquIdx);
        if (pBa1xHw->pScalingEqu == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW_exit;
        }
    }

    if (pBa1xHwSet->leakageEquIdx == LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID)
    {
        pBa1xHw->pLeakageEqu = NULL;
    }
    else
    {
        pBa1xHw->pLeakageEqu = PWR_EQUATION_GET(pBa1xHwSet->leakageEquIdx);
        if (pBa1xHw->pLeakageEqu == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW_exit;
        }
    }

#if (PMUCFG_FEATURE_ENABLED_PWR_DEVICE_BA13X_THRU_BA15)
    pBa1xHw->voltRailIdx     = pBa1xHwSet->voltRailIdx;
    pBa1xHw->edpPConfigC0C1  = pBa1xHwSet->edpPConfigC0C1;
    pBa1xHw->edpPConfigC2Dba = pBa1xHwSet->edpPConfigC2Dba;
#endif

    if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA15) &&
        !PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA16))
    {
        // Cache "shiftA" since it will not change.
        pBa1xHw->shiftA =
            pwrEquationBA1xScaleComputeShiftA(
                (PWR_EQUATION_BA1X_SCALE *)pBa1xHw->pScalingEqu,
                pBa1xHw->constW,
                pBa1xHw->bLwrrent);
    }
    else
    {
        pBa1xHw->shiftA = 0;
    }

pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW_exit:
    return status;
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad_BA1XHW
(
    PWR_DEVICE *pDev
)
{
    LwU8    limitIdx;

    // Reset limit thresholds uppon load()
    for (limitIdx = 0; limitIdx < PWR_DEVICE_BA1XHW_LIMIT_COUNT; limitIdx++)
    {
        thermPwrDevBaLimitSet_HAL(&Therm, pDev, limitIdx, PWR_DEVICE_NO_LIMIT);
    }

    return thermPwrDevBaLoad_HAL(&Therm, pDev);
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevGetVoltage
 */
FLCN_STATUS
pwrDevGetVoltage_BA1XHW
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU32      *pVoltageuV
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;

    *pVoltageuV = pBa1xHw->voltageuV;

    return FLCN_OK;
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevGetLwrrent
 */
FLCN_STATUS
pwrDevGetLwrrent_BA1XHW
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU32      *pLwrrentmA
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;

    if (!pBa1xHw->bLwrrent)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    return thermPwrDevBaGetReading_HAL(&Therm, pDev, provIdx, pLwrrentmA);
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevGetPower
 */
FLCN_STATUS
pwrDevGetPower_BA1XHW
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU32      *pPowermW
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;

    // TODO: Hack exposing current estimations through power interface
    if (provIdx > RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_DYNAMIC_POWER)
    {
        return pwrDevGetLwrrent_BA1XHW(pDev, provIdx, pPowermW);
    }

    if (pBa1xHw->bLwrrent)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    return thermPwrDevBaGetReading_HAL(&Therm, pDev, provIdx, pPowermW);
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevVoltageSetChanged
 */
FLCN_STATUS
pwrDevVoltageSetChanged_BA1XHW
(
    BOARDOBJ *pBoardObj,
    LwU32     voltageuV
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pBoardObj;

    if (pBa1xHw->voltageuV != voltageuV)
    {
        pBa1xHw->voltageuV = voltageuV;

        thermPwrDevBaStateSync_HAL(&Therm, (PWR_DEVICE *)pBoardObj, LW_TRUE);
    }

    return FLCN_OK;
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit_BA1XHW
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;

    if (((limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA) &&
         (pBa1xHw->bLwrrent)) ||
        ((limitUnit == LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW) &&
         (!pBa1xHw->bLwrrent)))
    {
        thermPwrDevBaLimitSet_HAL(&Therm, pDev, limitIdx, limitValue);
        return FLCN_OK;
    }

    PMU_HALT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevStateSync
 */
FLCN_STATUS
pwrDevStateSync_BA1XHW
(
    PWR_DEVICE *pDev
)
{
    thermPwrDevBaStateSync_HAL(&Therm, pDev, LW_FALSE);

    return FLCN_OK;
}

/*!
 * BA1XHW power device implementation.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet_BA1XHW
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS status;
    LwU32       voltagemV;
    PWR_DEVICE_BA1XHW  *pBa1xHw = (PWR_DEVICE_BA1XHW *)pDev;

    if (provIdx >= RM_PMU_PMGR_PWR_DEVICE_BA1X_PROV_NUM)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Get Voltage
    pTuple->voltuV = pBa1xHw->voltageuV;

    voltagemV = LW_UNSIGNED_ROUNDED_DIV(pTuple->voltuV, 1000);

    if (pBa1xHw->bLwrrent)
    {
        // Get Current
        status = thermPwrDevBaGetReading_HAL(&Therm, pDev, provIdx, &(pTuple->lwrrmA));
        if (status != FLCN_OK)
        {
            return status;
        }

        // Callwlate Power
        pTuple->pwrmW    = voltagemV * (pTuple->lwrrmA) / 1000;
    }
    else
    {
        // Get Power
        status = thermPwrDevBaGetReading_HAL(&Therm, pDev, provIdx, &(pTuple->pwrmW));
        if (status != FLCN_OK)
        {
            return status;
        }

        // Callwlate current
        pTuple->lwrrmA   = LW_UNSIGNED_ROUNDED_DIV(((pTuple->pwrmW) * 1000), voltagemV);
    }

    return FLCN_OK;
}
