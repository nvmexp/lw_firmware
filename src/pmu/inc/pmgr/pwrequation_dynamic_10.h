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
 * @file    pwrequation_dynamic_10.h
 * @copydoc pwrequation_dynamic_10.c
 */

#ifndef PWREQUATION_DYNAMIC_10_H
#define PWREQUATION_DYNAMIC_10_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation_dynamic.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_EQUATION_DYNAMIC_10 PWR_EQUATION_DYNAMIC_10;

/*!
 * Extends PWR_EQUATION to implement DYNAMIC10 Power Equation.
 */
struct PWR_EQUATION_DYNAMIC_10
{
    /*!
     * PWR_EQUATION_DYNAMIC super class.
     */
    PWR_EQUATION_DYNAMIC   super;

    /*!
     * Voltage scaling exponent for dynamic current (mA) in UFXP20.12 unitless
     */
    LwUFXP20_12            dynLwrrentVoltExp;
};

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes  --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION interfaces
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC_10)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC_10");

/*!
 * @interface PWR_EQUATION_DYNAMIC_10
 */
PwrEquationDynamicScaleVoltageLwrrent (pwrEquationDynamicScaleVoltageLwrrent_DYNAMIC_10)
    GCC_ATTRIB_SECTION("imem_libPwrEquation", "pwrEquationDynamicScaleVoltageLwrrent_DYNAMIC_10");

PwrEquationDynamicScaleVoltagePower (pwrEquationDynamicScaleVoltagePower_DYNAMIC_10)
    GCC_ATTRIB_SECTION("imem_libPwrEquation", "pwrEquationDynamicScaleVoltagePower_DYNAMIC_10");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#endif // PWREQUATION_DYNAMIC_10_H
