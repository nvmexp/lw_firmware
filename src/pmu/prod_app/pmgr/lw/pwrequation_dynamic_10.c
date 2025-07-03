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
 * @file    pwrequation_dynamic_10.c
 * @brief   PMGR Power Equation DYNAMIC 1x support
 *
 * This module is a collection of functions managing and manipulating state
 * related to Power Equation DYNAMIC_10 objects in the Power Equation Table.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Equation_Table_1.X
 *
 * The equation stores the voltage exponent for dynamic current (mA) component. The class provides
 * interfaces to model the core workload parameter and dynamic current or power component using the
 * the exponent and is defined for a per process node.
 *                  Idyn = V^dynLwrrentVoltExp     * freq * Workload
 *                  Pdyn = V^(dynLwrrentVoltExp+1) * freq * Workload
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "lib/lib_fxp.h"
#include "pmgr/pwrequation_dynamic_10.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Construct a new PMGR PWR_EQUATION_DYNAMIC_10 object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC_10
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_EQUATION_DYNAMIC_10_BOARDOBJ_SET   *pDyn10Desc =
        (RM_PMU_PMGR_PWR_EQUATION_DYNAMIC_10_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_EQUATION_DYNAMIC_10                            *pDyn10;
    FLCN_STATUS                                         status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDyn10 = (PWR_EQUATION_DYNAMIC_10 *)*ppBoardObj;

    // Set the type-specific values.
    pDyn10->dynLwrrentVoltExp = pDyn10Desc->dynLwrrentVoltExp;

    return status;
}

/*!
 * DYNAMIC 1.0 implementation
 * @copydoc PwrEquationDynamicScaleVoltageLwrrent
 */
LwUFXP20_12
pwrEquationDynamicScaleVoltageLwrrent_DYNAMIC_10
(
    PWR_EQUATION_DYNAMIC  *pDynamic,
    LwUFXP20_12            voltageV
)
{
    PWR_EQUATION_DYNAMIC_10  *pDyn10 = (PWR_EQUATION_DYNAMIC_10 *)pDynamic;

    return powFXP(voltageV, (LwSFXP20_12)pDyn10->dynLwrrentVoltExp);
}

/*!
 * DYNAMIC 1.0 implementation
 * @copydoc PwrEquationDynamicScaleVoltagePower
 */
LwUFXP20_12
pwrEquationDynamicScaleVoltagePower_DYNAMIC_10
(
    PWR_EQUATION_DYNAMIC  *pDynamic,
    LwUFXP20_12            voltageV
)
{

    return (multUFXP20_12(voltageV,
        pwrEquationDynamicScaleVoltageLwrrent_DYNAMIC_10(pDynamic, voltageV)));
}
