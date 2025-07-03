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
 * @file    pwrviolation.h
 * @copydoc pwrviolation.c
 */

#ifndef PWRVIOLATION_H
#define PWRVIOLATION_H

/* ------------------------- System Includes ------------------------------- */
#include "pmgr/lib_pmgr.h"
#include "boardobj/boardobjgrp.h"

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PWR_VIOLATION PWR_VIOLATION, PWR_VIOLATION_BASE;

/* ------------------------------ Macros ------------------------------------*/
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION)
#define BOARDOBJGRP_DATA_LOCATION_PWR_VIOLATION \
    (&(Pmgr.pwr.violations.super))
#else
#define BOARDOBJGRP_DATA_LOCATION_PWR_VIOLATION \
    ((BOARDOBJGRP *)NULL)
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_VIOLATION)

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PWR_VIOLATION_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(PWR_VIOLATION, (_objIdx)))

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Main structure representing a power capping channel
 */
struct PWR_VIOLATION
{
    /*!
     * BOARDOBJ super-class.
     */
    BOARDOBJ                                   super;
    /*!
     * Structure containing info of type of therm index and union of therm
     * indexes based on type.
     */
    LW2080_CTRL_PMGR_PWR_VIOLATION_THERM_INDEX thrmIdx;
    /*!
     * Violation structure that will contain the information about the
     * violation observed on the given thrmIdx
     */
    LIB_PMGR_VIOLATION                         violation;
    /*!
     * Period of this power policy (in OS ticks).
     */
    LwU32                                      ticksPeriod;
    /*!
     * Expiration time (next callback) of this power policy (is OS ticks).
     */
    LwU32                                      ticksNext;
};

/*!
 * @interface PWR_VIOLATION
 *
 * Loads a PWR_VIOLATION specified by pViolation.
 *
 * @param[in]   pViolation  PWR_VIOLATION object pointer.
 * @param[in]   ticksNow    OS ticks timestamp to synchronize all load() code
 *
 * @return FLCN_OK
 *      The load() for this power violation object exelwted successfully.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *      This interface is not supported on specific power violation object.
 *
 * @return Other errors.
 *      Unexpected errors propogated up from type-specific implementations.
 */
#define PwrViolationLoad(fname) FLCN_STATUS (fname)(PWR_VIOLATION *pViolation, LwU32 ticksNow)

/*!
 * @interface PWR_VIOLATION
 *
 * Evaluates PWR_VIOLATION object specified by pViolation.
 *
 * @param[in]   pViolation  PWR_VIOLATION object pointer.
 *
 * @return FLCN_OK
 *      The evaluate() for this power violation object exelwted successfully.
 *
 * @return FLCN_ERR_NOT_SUPPORTED
 *      This interface is not supported on specific power violation object.
 *
 * @return Other errors.
 *      Unexpected errors propogated up from type-specific implementations.
 */
#define PwrViolationEvaluate(fname) FLCN_STATUS (fname)(PWR_VIOLATION *pViolation)

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
// BOARDOBJ interfaces.
BoardObjGrpIfaceModel10ObjSet       (pwrViolationGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrViolationGrpIfaceModel10ObjSetImpl_SUPER");

// PWR_VIOLATION interfaces.
PwrViolationLoad        (pwrViolationLoad_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrViolationLoad_SUPER");
PwrViolationEvaluate    (pwrViolationEvaluate_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrViolationEvaluate_SUPER");

// Public interfaces.
FLCN_STATUS pwrViolationsLoad(LwU32 ticksNow)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrViolationsLoad");
LwBool     pwrViolationsExpiredMaskGet(LwU32 ticksNow, BOARDOBJGRPMASK_E32 *pM32)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "pwrViolationsExpiredMaskGet");
FLCN_STATUS pwrViolationsEvaluate(BOARDOBJGRPMASK *pMask)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrViolationsEvaluate");
BoardObjIfaceModel10GetStatus           (pwrViolationIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrViolationIfaceModel10GetStatus_SUPER");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrviolation_propgain.h"

#endif // PWRVIOLATION_H
