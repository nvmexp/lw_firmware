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
 * @file   pwrequation_leakage.c
 * @brief  PMGR Power Equation Leakage Model Management
 *
 * The Leakage Group class is a virtual class which provides a common
 * arguments to control leakage equations defined in Power Equation Table.
 * This module does not include any equation.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Equation_Table_1.X
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrequation_leakage.h"
#include "pmu_objpsi.h"
#include "pmu_objpg.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION_LEAKAGE implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_BOARDOBJ_SET   *pLeakageDesc =
        (RM_PMU_PMGR_PWR_EQUATION_LEAKAGE_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_EQUATION_LEAKAGE                            *pLeakage;
    FLCN_STATUS                                      status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pLeakage = (PWR_EQUATION_LEAKAGE *)*ppBoardObj;

    // Set the class-specific values, if any
    pLeakage->fsEff = pLeakageDesc->fsEff;
    pLeakage->pgEff = pLeakageDesc->pgEff;

    return status;
}

/*!
 * @copydoc PwrEquationGetLeakage
 */
FLCN_STATUS
pwrEquationGetLeakage
(
    PWR_EQUATION           *pEquation,
    LwU32                   voltageuV,
    LwBool                  bIsUnitmA,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                  *pLeakageVal
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check
    PMU_ASSERT_OK_OR_GOTO(status,
        ((pEquation == NULL) || (pLeakageVal == NULL)) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        pwrEquationGetLeakage_exit);

    if (bIsUnitmA)
    {
        status = pwrEquationLeakageEvaluatemA((PWR_EQUATION_LEAKAGE *)pEquation, voltageuV, pPgRes, pLeakageVal);
    }
    else
    {
        status = pwrEquationLeakageEvaluatemW((PWR_EQUATION_LEAKAGE *)pEquation, voltageuV, pPgRes, pLeakageVal);
    }

pwrEquationGetLeakage_exit:
    return status;
}

/*!
 * @copydoc PwrEquationLeakageEvaluatemA
 *
 * @note    pwrEquationLeakageEvaluatemA_CORE() is a helper call that does not
 *          have any side effects on PG/MSCG code (PSI feature).
 */
FLCN_STATUS
pwrEquationLeakageEvaluatemA_CORE
(
    PWR_EQUATION_LEAKAGE   *pLeakage,
    LwU32                   voltageuV,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                  *pLeakageVal
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check
    PMU_ASSERT_OK_OR_GOTO(status,
        (pLeakageVal == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        pwrEquationLeakageEvaluatemA_CORE_exit);

    //
    // Class-specific handling to evaluate leakage current using
    // PWR_EQUATION_LEAKAGE_<xyz> equation for a given voltage.
    //
    switch (pLeakage->super.super.type)
    {
        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_LEAKAGE_DTCS11:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS11);
            status = pwrEquationLeakageEvaluatemA_DTCS11(pLeakage, voltageuV, pPgRes, pLeakageVal);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_LEAKAGE_DTCS12:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS12);
            status = pwrEquationLeakageEvaluatemA_DTCS12(pLeakage, voltageuV, pPgRes, pLeakageVal);
            break;
        }
        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_LEAKAGE_DTCS13:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS13);
            status = pwrEquationLeakageEvaluatemA_DTCS13(pLeakage, voltageuV, pPgRes, pLeakageVal);
            break;
        }
        default:
        {
            // MUST provide valid type.
            PMU_BREAKPOINT();
            return 0;
        }
    }

    PMU_ASSERT_OK_OR_GOTO(status, ((status == FLCN_OK) ? FLCN_OK : status), 
        pwrEquationLeakageEvaluatemA_CORE_exit);

    //
    // Handling for efficiency.
    //
    // Numerical Analysis:-
    // We need to callwlate:
    // *pLeakageVal = *pLeakageVal * pLeakage->fsEff
    //
    // *pLeakageVal: 32.0
    // pLeakage->fsEff: FXP4.12
    //
    // For AD10X_AND_LATER chips:-
    //
    // The multiplication is handled by a PMGR wrapper over
    // multUFXP20_12FailSafe() which does intermediate math safely in 64-bit as
    // follows:-
    //
    // 4.12 is typecast to 20.12
    // 32.0 * 20.12 => 52.12
    // 52.12 >> 12 => 52.0
    //
    // If the upper 32 bits are non-zero (=> 32-bit overflow), it
    // returns LW_U32_MAX, otherwise
    // LWU64_LO(52.0) => 32.0 (the least significant 32 bits are
    //                         returned)
    //
    // Note: For PRE_AD10X chips, there is a possible FXP20.12 overflow
    //       at 1048576 mA.
    //
    *pLeakageVal = pmgrMultUFXP20_12OverflowFixes(*pLeakageVal,
                       pLeakage->fsEff);

pwrEquationLeakageEvaluatemA_CORE_exit:
    return status;
}

/*!
 * @copydoc PwrEquationLeakageEvaluatemA
 */
FLCN_STATUS
pwrEquationLeakageEvaluatemA
(
    PWR_EQUATION_LEAKAGE   *pLeakage,
    LwU32                   voltageuV,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                  *pLeakageVal
)
{
    FLCN_STATUS status = pwrEquationLeakageEvaluatemA_CORE(pLeakage, voltageuV, pPgRes, pLeakageVal);

    // Sanity check
    PMU_ASSERT_OK_OR_GOTO(status,
        (pLeakageVal == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        pwrEquationLeakageEvaluatemA_exit);

    //
    // Estimated sleep current needs to be callwlated for current aware PSI.
    // Power saving feature engaging PSI needs the most recent sleep current to
    // decide if PSI should be engaged. The sleepmA callwlation fits into the
    // leakage model, and the polling period for leakage callwlations also
    // satisfy PSI requirements. Hence we are using existing leakage
    // infrastructure to callwlate this estimated sleep current.
    //
    if ((!PMUCFG_FEATURE_ENABLED(PMU_PSI_PMGR_SLEEP_CALLBACK))  &&
        PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_LWRRENT_AWARE)   &&
        PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_2X))
    {
        psiSleepLwrrentCalcMonoRail(pLeakage);
    }

pwrEquationLeakageEvaluatemA_exit:
    return status;
}

/*!
 * @copydoc PwrEquationLeakageEvaluatemW
 */
FLCN_STATUS
pwrEquationLeakageEvaluatemW
(
    PWR_EQUATION_LEAKAGE   *pLeakage,
    LwU32                   voltageuV,
    PMGR_LPWR_RESIDENCIES  *pPgRes,
    LwU32                  *pLeakageVal
)
{
    FLCN_STATUS status    = pwrEquationLeakageEvaluatemA(pLeakage, voltageuV, pPgRes, pLeakageVal);

    // Sanity check
    PMU_ASSERT_OK_OR_GOTO(status,
        (pLeakageVal == NULL) ? FLCN_ERR_ILWALID_ARGUMENT : FLCN_OK,
        pwrEquationLeakageEvaluatemW_exit);

    LwU32       voltagemV = LW_UNSIGNED_ROUNDED_DIV(voltageuV, 1000);

    // power (mW) = current (mA) * voltage (mV) / 1000
    *pLeakageVal = LW_UNSIGNED_ROUNDED_DIV((*pLeakageVal) * voltagemV, 1000);

pwrEquationLeakageEvaluatemW_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
