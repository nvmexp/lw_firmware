/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
  * @file pwrchrel_regulator_eff_est_v1.h
  * @brief @copydoc pwrchrel_regulator_eff_est_v1.c
  */

#ifndef PWRCHREL_REGULATOR_EFF_EST_V1_H_
#define PWRCHREL_REGULATOR_EFF_EST_V1_H_
/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchrel.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1;

struct PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1
{
    /*!
     * @copydoc PWR_CHRELATIONSHIP
     */
    PWR_CHRELATIONSHIP super;

    /*!
     * Secondary channel index.
     */
    LwU8 chIdxSecondary;

    /*!
     * Primary channel unit.
     */
    LwU8 unitPrimary;

    /*!
     * Direction, input to output:efficiency needs to be multiplied
     * output to input: efficiency needs to be divided with primary channel
     */
    LwU8 direction;

    /*!
     * Second-order Coefficient 0 (Volt^2, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient0;

    /*!
     * Second-order Coefficient 1 (Volt^2, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient1;

    /*!
     * Second-order Coefficient 2 (mW^2 or mA^2 based on direction, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient2;

    /*!
     * First-order Coefficient 3 (V, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient3;

    /*!
     * First-order Coefficient 4 (V, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient4;

    /*!
     * First-order Coefficient 5 (W or A based on direction, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient5;

    /*!
     * Second-order Coefficient 6 (V^2, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient6;

    /*!
     * Second-order Coefficient 7 (V*W or V*A based on direction, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient7;

    /*!
     * Second-order Coefficient 8 (V*W or V*A based on direction, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient8;

    /*!
     * Constant Coefficient 9 (unitless, FXP20.12 signed)
     */
    LwSFXP20_12 coefficient9;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/*!
 * @interface PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1
 *
 * Estimates the regulator efficiency based on the following two equations,
 * applies the efficiency to callwlate the secondary tuples (mW, mA) from primary
 *
 * 1. When the primary channel input unit is specified in milliWatts (mW), the
 * efficiency equation is
 * Efficiency = C0 * v(P)^2 + C1 * v(S)^2 + C2 * w(P)^2/10^6 +C3 * v(P)    +
 *              C4 * v(S) + C5 * w(P) + C6 * v(P) * v(S) +C7 * v(P) * w(P) +
 *              C8 * v(S) * w(P) + C9
 *
 * 2. When the primary channel unit is specified in milliAmps (mA), the
 * efficiency equation is
 * Efficiency = C0 * v(P)^2 + C1 * v(S)^2 + C2 * I(P)^2/10^6 +C3 * v(P)   +
 *              C4 * v(S) + C5 * I(P) + C6* v(P) * v(S) +C7 * v(P) * I(P) +
 *              C8 * v(S) * I(P) + C9
 *
 * @param[in]      pReffE  Pointer to PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 object.
 * @param[in]      pTuplePrimary  Pointer to the primary channel tuple structure.
 * @param[in/out]  pTupleSecondary  Pointer to the secondary channel tuple structure.
 *
 * @return  FLCN_OK  on success
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT if any input pointers are NULL
 */
#define PwrChRelationshipRegulatorEffEstV1EvalSecondary(fname) FLCN_STATUS (fname) (PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 *pReffE, LW2080_CTRL_PMGR_PWR_TUPLE *pTuplePrimary, LW2080_CTRL_PMGR_PWR_TUPLE *pTupleSecondary)

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
BoardObjGrpIfaceModel10ObjSet               (pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_EFF_EST_V1)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_EFF_EST_V1");
PwrChRelationshipTupleEvaluate  (pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipTupleEvaluate_REGULATOR_EFF_EST_V1");

/*!
 * PWR_CHRELATIONSHIP_REGULATOR_EFF_EST_V1 interfaces
 */
PwrChRelationshipRegulatorEffEstV1EvalSecondary(pwrChRelationshipRegulatorEffEstV1EvalSecondary)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipRegulatorEffEstV1EvalSecondary");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHREL_REGULATOR_EFF_EST_V1_H_
