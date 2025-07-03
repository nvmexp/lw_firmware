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
 * @file    pwrequation_dynamic.h
 * @copydoc pwrequation_dynamic.c
 */

#ifndef PWREQUATION_DYNAMIC_H
#define PWREQUATION_DYNAMIC_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_EQUATION_DYNAMIC PWR_EQUATION_DYNAMIC;

/*!
 * Extends PWR_EQUATION to implement DYNAMIC Power Equation class.
 */
struct PWR_EQUATION_DYNAMIC
{
    /*!
     * PWR_EQUATION super class.
     */
    PWR_EQUATION    super;
};

/*!
 * @interface PWR_EQUATION_DYNAMIC
 *
 * Scale and return the input voltage specified in UFXP 20_12 Volts
 * (i.e voltageV ^ expInLwrrent)
 *
 * @param[in] pDynamic      Pointer to PWR_EQUATION_DYNAMIC object
 * @param[in] voltageV      The input voltage in UFXP 20_12 format and unit of Volts (V)
 *
 * @pre       Caller is expected to have attached "libFxp" overlays
 *
 * @return    Scaled voltage in volts unit (V) and in LwUFXP20_12 fromat
 */
#define PwrEquationDynamicScaleVoltageLwrrent(fname) LwUFXP20_12 (fname)(PWR_EQUATION_DYNAMIC *pDynamic, LwUFXP20_12 voltageV)

/*!
 * @interface PWR_EQUATION_DYNAMIC
 *
 * Scale and return the input voltage specified in UFXP 20_12 Volts.
 * (i.e voltageV * (voltageV ^ expInLwrrent))
 *
 * @param[in] pDynamic      Pointer to PWR_EQUATION_DYNAMIC object
 * @param[in] voltageV      The input voltage in UFXP 20_12 format and unit of Volts (V)
 *
 * @pre       Caller is expected to have attached "libFxp" overlays
 *
 * @return    Scaled voltage in volts unit (V) and in LwUFXP20_12 fromat
 */
#define PwrEquationDynamicScaleVoltagePower(fname) LwUFXP20_12 (fname)(PWR_EQUATION_DYNAMIC *pDynamic, LwUFXP20_12 voltageV)

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes  --------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_DYNAMIC");

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @interfaces PWR_EQUATION_DYNAMIC
 */
PwrEquationDynamicScaleVoltageLwrrent (pwrEquationDynamicScaleVoltageLwrrent)
    GCC_ATTRIB_SECTION("imem_libPwrEquation", "pwrEquationDynamicScaleVoltageLwrrent");

PwrEquationDynamicScaleVoltagePower (pwrEquationDynamicScaleVoltagePower)
    GCC_ATTRIB_SECTION("imem_libPwrEquation", "pwrEquationDynamicScaleVoltagePower");

/* ------------------------ Defines ----------------------------------------- */
/*!
 * @brief   List of an overlay descriptors required by PWR_EQUATION_DYNAMIC
 *          during pwr policy evaluation.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_DYNAMIC))
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_EQUATION_DYNAMIC      \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPwrEquation)                      \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64
#else
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_EQUATION_DYNAMIC      \
    OSTASK_OVL_DESC_ILWALID
#endif


/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#include "pmgr/pwrequation_dynamic_10.h"

#endif // PWREQUATION_DYNAMIC_H
