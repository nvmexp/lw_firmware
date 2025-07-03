/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrequation.c
 * @brief  Interface specification for a PWR_EQUATION.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Equation_Table_1.X
 *
 * A "Power Equation" is a logical construct representing a power equation,
 * which specifies set of independent variables and coefficients that can be
 * used to callwlate various power related expressions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrequation.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    return boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
pmgrPwrEquationIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    FLCN_STATUS status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto pmgrPwrEquationIfaceModel10SetHeader_exit;
    }

    // Set the IDDQ value.
    Pmgr.iddqmA =
        ((RM_PMU_PMGR_PWR_EQUATION_BOARDOBJGRP_SET_HEADER *)pHdrDesc)->iddqmA;

pmgrPwrEquationIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
pmgrPwrEquationIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_LEAKAGE_DTCS11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS11);
            return pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS11(pModel10, ppBoardObj,
                    sizeof(PWR_EQUATION_LEAKAGE_DTCS11), pBuf);

        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_BA1X_SCALE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_BA1X_SCALE);
            return pwrEquationGrpIfaceModel10ObjSetImpl_BA1X_SCALE(pModel10, ppBoardObj,
                    sizeof(PWR_EQUATION_BA1X_SCALE), pBuf);

        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_LEAKAGE_DTCS12:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS12);
            return pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS12(pModel10, ppBoardObj,
                    sizeof(PWR_EQUATION_LEAKAGE_DTCS12), pBuf);

        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_LEAKAGE_DTCS13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_LEAKAGE_DTCS13);
            return pwrEquationGrpIfaceModel10ObjSetImpl_LEAKAGE_DTCS13(pModel10, ppBoardObj,
                    sizeof(PWR_EQUATION_LEAKAGE_DTCS13), pBuf);

        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_DYNAMIC_10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_DYNAMIC_10);
            return pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC_10(pModel10, ppBoardObj,
                    sizeof(PWR_EQUATION_DYNAMIC_10), pBuf);

        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_BA15_SCALE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_BA15_SCALE);
            return pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE(pModel10, ppBoardObj,
                    sizeof(PWR_EQUATION_BA15_SCALE), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/* ------------------------- Private Functions ------------------------------ */
