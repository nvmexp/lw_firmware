/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel_regulator_loss_est.h
 * @brief @copydoc pwrchrel_regulator_loss_est.c
 */

#ifndef PWRCHREL_REGULATOR_LOSS_EST_H
#define PWRCHREL_REGULATOR_LOSS_EST_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchrel.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST;

struct PWR_CHRELATIONSHIP_REGULATOR_LOSS_EST
{
    /*!
     * @copydoc PWR_CHRELATIONSHIP
     */
    PWR_CHRELATIONSHIP super;

    /*!
     * Constant Coefficient 0 (mW, F20.12 signed)
     */
    LwSFXP20_12 coefficient0;

    /*!
     * Constant Coefficient 1 (mV, F20.12 signed)
     */
    LwSFXP20_12 coefficient1;

    /*!
     * First-order Coefficient 2 (mW / mV, F20.12 signed)
     */
    LwSFXP20_12 coefficient2;

    /*!
     * First-order Coefficient 3 (mW / mV, F20.12 signed)
     */
    LwSFXP20_12 coefficient3;

    /*!
     * First-order Coefficient 4 (mV / mV, F20.12 signed)
     */
    LwSFXP20_12 coefficient4;

    /*!
     * First-order Coefficient 5 (mV / mV, F20.12 signed)
     */
    LwSFXP20_12 coefficient5;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
BoardObjGrpIfaceModel10ObjSet               (pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_LOSS_EST)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChRelationshipGrpIfaceModel10ObjSet_REGULATOR_LOSS_EST");
PwrChRelationshipTupleEvaluate  (pwrChRelationshipTupleEvaluate_REGULATOR_LOSS_EST)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipTupleEvaluate_REGULATOR_LOSS_EST");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHREL_REGULATOR_LOSS_EST_H
