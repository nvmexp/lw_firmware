/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrviolation_propgain.h
 * @copydoc pwrviolation_propgain.c
 */

#ifndef PWRVIOLATION_PROPGAIN_H
#define PWRVIOLATION_PROPGAIN_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrviolation.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_VIOLATION_PROPGAIN PWR_VIOLATION_PROPGAIN;

/*!
 * Extends PWR_VIOLATION to implement the "proportional gain" sub-class.
 *
 * Logic:
 *          Multiply the error (violation target - violation rate) by the
 * proportional gain and use the result to adjust the power limits of the
 * power policy relationships from index @ref policyRelIdxFirst to index
 * @ref policyRelIdxLast.
 */
struct PWR_VIOLATION_PROPGAIN
{
    /*!
     * PWR_VIOLATION super class.
     */
    PWR_VIOLATION   super;
    /*!
     * Signed FXP20.12 proportional gain value to use to adjust the power limit.
     */
    LwSFXP20_12     propGain;
    /*!
     * Index of first Power Policy Relationship in the Power Policy Table.
     */
    LwU8            policyRelIdxFirst;
    /*!
     * Index of last Power Policy Relationship in the Power Policy Table.
     */
    LwU8            policyRelIdxLast;
    /*!
     * Computed limit adjustment by scaling gain and normalizing units.
     */
    LwS32           pwrLimitAdj;
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * @copydoc PwrViolationsLoad
 *
 * Default implementation to parent class implementation.
 */
#define pwrViolationLoad_PROPGAIN(_pViolation, _ticksNow) \
    pwrViolationLoad_SUPER((_pViolation), (_ticksNow))

/*!
 * Restrict the MAX value of the limit input set by PWR_VIOLATION_PROPGAIN to
 * some safe power number.
 */
#define PWR_VIOLATION_PROPGAIN_LIMIT_INPUT_MAX                          1024000

/* ------------------------- Function Prototypes --------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
// BOARDOBJ interfaces.
BoardObjGrpIfaceModel10ObjSet       (pwrViolationGrpIfaceModel10ObjSetImpl_PROPGAIN)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrViolationGrpIfaceModel10ObjSetImpl_PROPGAIN");

// PWR_VIOLATION interfaces.
PwrViolationEvaluate    (pwrViolationEvaluate_PROPGAIN)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrViolationEvaluate_PROPGAIN");
BoardObjIfaceModel10GetStatus           (pwrViolationIfaceModel10GetStatus_PROPGAIN)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrViolationIfaceModel10GetStatus_PROPGAIN");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRVIOLATION_PROPGAIN_H
