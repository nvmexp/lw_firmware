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
 * @file   pwrequation_leakage_dtcs11.c
 * @brief  PMGR Power Equation Leakage DTCS 1.1 Model Management
 *
 * This module is a collection of functions managing and manipulating state
 * related to Power Equation Leakage DTCS 1.1 objects in the Power Equation Table.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Equation_Table_1.X
 *
 * The DTCS 1.1 Power Leakage Equation type is an equation specified by the
 * Desktop Chip Soltuions Team.  This equation uses the following independent
 * variables:
 *     IDDQ - Quiescent Current (mA).  Unsigned 32-bit integer.
 *     Vset - Voltage (mV).  Unsigned 32-bit integer.
 *     tj   - Temperature (C). Signed FXP 24.8 value.
 *
 * The DTCS 1.1 Power Leakage Equation is really just a slight modification of
 * the DTCS 1.0 type, moving all the math from floating point to fixed-point
 * precision.
 *
 * Each equation entry specifies the following set of coefficients:
 *     k0 - Unsigned integer value.
 *     K1 - Unsigned FXP 20.12 value.
 *     k2 - Unsigned FXP 20.12 value.
 *     k3 - Signed FXP 24.8 value.
 *
 * With these values the DTCS 1.1 equation class's power can be callwlated as:
 *     Pclass = Vset * IDDQ * (Vset / k0) ^ (k1) * k2 ^ (tj + k3)
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "pmgr/pwrequation_leakage_dtcs11.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * DTCS 1.1 implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS11
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS11_BOARDOBJ_SET    *pDtcs11Desc =
        (RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS11_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_EQUATION_LEAKAGE_DTCS11                             *pDtcs11;
    FLCN_STATUS                                              status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcs11 = (PWR_EQUATION_LEAKAGE_DTCS11 *)*ppBoardObj;

    // Set the class-specific values:
    pDtcs11->k0 = pDtcs11Desc->k0;
    pDtcs11->k1 = pDtcs11Desc->k1;
    pDtcs11->k2 = pDtcs11Desc->k2;
    pDtcs11->k3 = pDtcs11Desc->k3;

    return status;
}

/*!
 * @copydoc PwrEquationLeakageDtcs11EvaluatemA
 * Callwlates leakage current by the DTCS 1.1 equation in FXP.
 *
 * Pclass = Vset * IDDQ * (Vset / k0) ^ (k1) * k2 ^ (tj + k3)
 */
FLCN_STATUS
pwrEquationLeakageDtcs11EvaluatemA
(
    PWR_EQUATION_LEAKAGE_DTCS11    *pDtcs11,
    LwU32                           voltageuV,
    LwU32                           iddqmA,
    LwTemp                          tj,
    LwU32                          *pLeakageVal
)
{
    FLCN_STATUS status = FLCN_OK;
    LwUFXP20_12                     leakagemA;
    LwU32                           voltagemV;

    // Vset = voltageuV / 1000.
    voltagemV = LW_UNSIGNED_ROUNDED_DIV(voltageuV, 1000);

    leakagemA = iddqmA *
            multUFXP20_12(
            powFXP(LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12, voltagemV,
                    pDtcs11->k0),
                pDtcs11->k1),
            powFXP(pDtcs11->k2,
                (tj + pDtcs11->k3) << 4));

    *pLeakageVal = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, leakagemA);

    //
    // Check the correctness of leakage value.  When input voltage >
    // 0, then callwlated leakage must be > 0.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        ((voltageuV != 0) && (*pLeakageVal == 0)) ? FLCN_ERR_ILWALID_STATE : FLCN_OK,
        pwrEquationLeakageDtcs11EvaluatemA_exit);

pwrEquationLeakageDtcs11EvaluatemA_exit:
    return status;
}


/*!
 * DTCS 1.1 implementation.
 *
 * @copydoc PwrEquationLeakageEvaluatemA
 */
FLCN_STATUS
pwrEquationLeakageEvaluatemA_DTCS11
(
    PWR_EQUATION_LEAKAGE   *pLeakage,
    LwU32                   voltageuV,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                  *pLeakageVal
)
{
    LwTemp                       tj;

    // Read Tj.
    tj = thermGetTempInternal_HAL(&Therm,
             LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_TSENSE);

    return pwrEquationLeakageDtcs11EvaluatemA(
            (PWR_EQUATION_LEAKAGE_DTCS11 *)pLeakage, voltageuV, Pmgr.iddqmA, tj, pLeakageVal);
}

/* ------------------------- Private Functions ------------------------------ */
