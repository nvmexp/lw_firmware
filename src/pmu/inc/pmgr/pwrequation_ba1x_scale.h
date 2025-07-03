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
 * @file    pwrequation_ba1x_scale.h
 * @copydoc pwrequation_ba1x_scale.c
 */

#ifndef PWREQUATION_BA1X_SCALE_H
#define PWREQUATION_BA1X_SCALE_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
typedef struct PWR_EQUATION_BA1X_SCALE PWR_EQUATION_BA1X_SCALE;

/*!
 * Structure representing a BA v1.x Scale equation entry.
 */
struct PWR_EQUATION_BA1X_SCALE
{
    /*!
     * PWR_EQUATION super class.
     */
    PWR_EQUATION    super;

    /*!
     * Derived constant "F" as unsigned FXP 20.12 value [A*s/V]
     */
    LwUFXP20_12     constF;
    /*!
     * Max voltage [uV] that can be applied on given system. Used to ensure that
     * we do not overflow during the computation of the 8-bit scaling factor.
     */
    LwU32           maxVoltageuV;
};

/*!
 * @interface PWR_EQUATION_BA1X_SCALE
 *
 * Evaluating the equation for shift amount that needs to be applied to BA's
 * offset (C) and threshold in order to ensure highest precision of the result.
 *
 * @param[in]   pBa1xScale  PWR_EQUATION object pointer for BA1X scaling equ.
 * @param[in]   constW      BA averaging window precomputed constant
 * @param[in]   bLwrrent    Set if device measures/estimates GPU current
 *
 * @return      Shift amount to be applied before programming BA registers
 */
#define PwrEquationBA1xScaleComputeShiftA(fname) LwU8 (fname)(PWR_EQUATION_BA1X_SCALE *pBa1xScale, LwUFXP20_12 constW, LwBool bLwrrent)

/*!
 * @interface PWR_EQUATION_BA1X_SCALE
 *
 * Evaluating the equation for BA's scaling parameter (A).
 *
 * @param[in]   pBa1xScale  PWR_EQUATION object pointer for BA1X scaling equ.
 * @param[in]   constW      BA averaging window precomputed constant
 * @param[in]   bLwrrent    Set if device measures/estimates GPU current
 * @param[in]   voltageuV   Current GPU core voltage [uV]
 *
 * @return      Scaling parameter (A) for BA to mW/mA colwersion
 */
#define PwrEquationBA1xScaleComputeScaleA(fname) LwUFXP20_12 (fname)(PWR_EQUATION_BA1X_SCALE *pBa1xScale, LwUFXP20_12 constW, LwBool bLwrrent, LwU32 voltageuV)

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION interfaces
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_BA1X_SCALE)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_BA1X_SCALE");

/*!
 * PWR_EQUATION_BA1X_SCALE interfaces
 */
PwrEquationBA1xScaleComputeShiftA   (pwrEquationBA1xScaleComputeShiftA);
PwrEquationBA1xScaleComputeScaleA   (pwrEquationBA1xScaleComputeScaleA);

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#endif // PWREQUATION_BA1X_SCALE_H
