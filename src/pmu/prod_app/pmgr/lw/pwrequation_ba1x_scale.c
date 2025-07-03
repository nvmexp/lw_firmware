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
 * @file    pwrequation_ba1x_scale.c
 * @brief   PMGR Power Equation BA v1.x Scale Model Management
 *
 * @implements PWR_EQUATION
 *
 * @note    For information on BA1XSW device see RM::pwrequation_ba1x_scale.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_fxp.h"
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrequation_ba1x_scale.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * BA v1.x Scale implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrEquationGrpIfaceModel10ObjSetImpl_BA1X_SCALE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    RM_PMU_PMGR_PWR_EQUATION_BA1X_SCALE_BOARDOBJ_SET    *pBa1xScaleDesc =
        (RM_PMU_PMGR_PWR_EQUATION_BA1X_SCALE_BOARDOBJ_SET *)pDesc;
    PWR_EQUATION_BA1X_SCALE                             *pBa1xScale;
    FLCN_STATUS                                          status;

    // Construct and initialize parent-class object.
    status = pwrEquationGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pBa1xScale = (PWR_EQUATION_BA1X_SCALE *)*ppBoardObj;

    // Set the class-specific values:
    pBa1xScale->constF       = pBa1xScaleDesc->constF;
    pBa1xScale->maxVoltageuV = pBa1xScaleDesc->maxVoltageuV;

    return status;
}

/*!
 * BA v1.x Scale implementation.
 *
 * @copydoc PwrEquationBA1xScaleComputeShiftA
 */
LwU8
pwrEquationBA1xScaleComputeShiftA
(
    PWR_EQUATION_BA1X_SCALE *pBa1xScale,
    LwUFXP20_12              constW,
    LwBool                   bLwrrent
)
{
    LwUFXP20_12 maxScaleA;

    maxScaleA = pwrEquationBA1xScaleComputeScaleA(pBa1xScale, constW, bLwrrent,
                                                  pBa1xScale->maxVoltageuV);

    HIGHESTBITIDX_32(maxScaleA);
    if (maxScaleA > 19)
    {
        // Incorrect VBIOS settings for BA_SUM_SHIFT.
        PMU_HALT();
    }
    maxScaleA = LW_MAX(maxScaleA, 11);

    return 8 - ((LwU8)maxScaleA - 11);
}

/*!
 * BA v1.x Scale implementation.
 *
 * @copydoc PwrEquationBA1xScaleComputeScaleA
 */
LwUFXP20_12
pwrEquationBA1xScaleComputeScaleA
(
    PWR_EQUATION_BA1X_SCALE *pBa1xScale,
    LwUFXP20_12              constW,
    LwBool                   bLwrrent,
    LwU32                    voltageuV
)
{
    LwU32       voltagemV;
    LwUFXP20_12 voltage;
    LwUFXP20_12 scaleA;

    voltagemV  = LW_UNSIGNED_ROUNDED_DIV(voltageuV, 1000);
    voltage    = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12, voltagemV, 1000);
    scaleA     = multUFXP20_12(pBa1xScale->constF, constW);
    scaleA     = multUFXP20_12(scaleA, voltage);
    if (!bLwrrent)
    {
        scaleA = multUFXP20_12(scaleA, voltage);
    }

    return scaleA;
}

/* ------------------------- Private Functions ------------------------------ */

