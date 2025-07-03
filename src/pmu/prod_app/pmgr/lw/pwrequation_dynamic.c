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
 * @file    pwrequation_dynamic.c
 * @brief   PMGR Power Equation DYNAMIC Super Class
 *
 * The Dynamic Group class is a virtual class which provides a common
 * arguments to control dynamic equations defined in Power Equation Table.
 * This module does not include any equation.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Equation_Table_1.X
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrequation_dynamic.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a new PMGR PWR_EQUATION_DYNAMIC object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS    status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Set the class-specific values, if any

    return status;
}

/*!
 * DYNAMIC implementation
 * @copydoc PwrEquationDynamicScaleVoltageLwrrent
 */
LwUFXP20_12
pwrEquationDynamicScaleVoltageLwrrent
(
    PWR_EQUATION_DYNAMIC  *pDynamic,
    LwUFXP20_12            voltageV
)
{
    switch (pDynamic->super.super.type)
    {
        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_DYNAMIC_10:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_DYNAMIC_10);
            return pwrEquationDynamicScaleVoltageLwrrent_DYNAMIC_10(pDynamic, voltageV);
        }
        default:
        {
            // MUST provide valid type.
            PMU_BREAKPOINT();
            return 0;
        }
    }
}

/*!
 * DYNAMIC implementation
 * @copydoc PwrEquationDynamicScaleVoltagePower
 */
LwUFXP20_12
pwrEquationDynamicScaleVoltagePower
(
    PWR_EQUATION_DYNAMIC  *pDynamic,
    LwUFXP20_12            voltageV
)
{
    switch (pDynamic->super.super.type)
    {
        case LW2080_CTRL_PMGR_PWR_EQUATION_TYPE_DYNAMIC_10:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_EQUATION_DYNAMIC_10);
            return pwrEquationDynamicScaleVoltagePower_DYNAMIC_10(pDynamic, voltageV);
        }
        default:
        {
            // MUST provide valid type.
            PMU_BREAKPOINT();
            return 0;
        }
    }
}
