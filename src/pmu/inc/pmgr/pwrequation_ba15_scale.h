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
 * @file    pwrequation_ba15_scale.h
 * @copydoc pwrequation_ba15_scale.c
 */

#ifndef PWREQUATION_BA15_SCALE_H
#define PWREQUATION_BA15_SCALE_H

/* ------------------------- System Includes -------------------------------- */
#include "pmgr/pwrequation.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------------ Macros -------------------------------------*/
// Vexp (Voltage Exponent) is set to 1.4 as default
#define PWR_EQUATION_BA15_SCALE_VOLTAGE_EXP_DEFAULT         0x1666

/* ------------------------- Types Definitions ------------------------------ */

typedef struct PWR_EQUATION_BA15_SCALE PWR_EQUATION_BA15_SCALE;

/*!
 * Structure representing a BA v1.5 Scale equation entry.
 */
struct PWR_EQUATION_BA15_SCALE
{
    /*!
     * PWR_EQUATION super class.
     */
    PWR_EQUATION    super;

    /*!
     * Reference voltage [uV].
     */
    LwU32           refVoltageuV;
    /*!
     * BA2mW scale factor [unitless unsigned FXP 20.12 value].
     */
    LwUFXP20_12     ba2mW;
    /*!
     * Reference GPCCLK [MHz].
     */
    LwU32           gpcClkMHz;
    /*!
     * Reference UTILSCLK [MHz].
     */
    LwU32           utilsClkMHz;
    /*!
     * Index into Power Equation Table (PWR_EQUATION) for the Dynamic equation.
     */
    LwU8            dynamicEquIdx;
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
 * @interface PWR_EQUATION_BA15_SCALE
 *
 * Evaluating the equation for shift amount that needs to be applied to BA's
 * offset (C) and threshold in order to ensure highest precision of the result.
 *
 * @param[in]   pBa15Scale  PWR_EQUATION object pointer for BA15 scaling equ.
 * @param[in]   bLwrrent    Set if device measures/estimates GPU current
 *
 * @return      Shift amount to be applied before programming BA registers
 */
#define PwrEquationBA15ScaleComputeShiftA(fname) LwU8 (fname)(PWR_EQUATION_BA15_SCALE *pBa15Scale, LwBool bLwrrent)

/*!
 * @interface PWR_EQUATION_BA15_SCALE
 *
 * Evaluating the equation for BA's scaling parameter (A).
 *
 * @param[in]   pBa15Scale  PWR_EQUATION object pointer for BA15 scaling equ.
 * @param[in]   bLwrrent    Set if device measures/estimates GPU current
 * @param[in]   voltageuV   Current GPU core voltage [uV]
 *
 * @return      Scaling parameter (A) for BA to mW/mA colwersion
 */
#define PwrEquationBA15ScaleComputeScaleA(fname) LwUFXP20_12 (fname)(PWR_EQUATION_BA15_SCALE *pBa15Scale, LwBool bLwrrent, LwU32 voltageuV)

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_EQUATION interfaces
 */
BoardObjGrpIfaceModel10ObjSet (pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrEquationGrpIfaceModel10ObjSetImpl_BA15_SCALE");

/*!
 * PWR_EQUATION_BA15_SCALE interfaces
 */
PwrEquationBA15ScaleComputeShiftA   (pwrEquationBA15ScaleComputeShiftA);
PwrEquationBA15ScaleComputeScaleA   (pwrEquationBA15ScaleComputeScaleA);

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#endif // PWREQUATION_BA15_SCALE_H
