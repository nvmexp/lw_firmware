/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel.h
 * @brief @copydoc pwrchrel.c
 */

#ifndef PWRCHREL_H
#define PWRCHREL_H

/* ------------------------- System Includes ------------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PWR_CHRELATIONSHIP PWR_CHRELATIONSHIP, PWR_CHRELATIONSHIP_BASE;

/* ------------------------------ Macros ------------------------------------*/
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PWR_CHRELATIONSHIP \
    (&(Pmgr.pwr.chRels.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PWR_CHREL_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(PWR_CHRELATIONSHIP, (_objIdx)))

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Main structure representing a power capping channel
 */
struct PWR_CHRELATIONSHIP
{
    /*!
     * BOARDOBJ super-class.
     */
    BOARDOBJ    super;

    /*!
     * Index of PWR_CHANNEL for this relationship.
     */
    LwU8        chIdx;

};

/*!
 * @interface PWR_CHRELATIONSHIP
 *
 * Evaluates a PWR_CHRELATIONSHIP entry to retrieve the power associated with
 * the relationship.  Most implementations will retrieve the power associated
 * with the channel pointed at by chIdx and then perform some
 * callwlation/operation on that value.
 *
 * @param[in] pChRel    Pointer to PWR_CHRELATIONSHIP object
 * @param[in] chIdxEval
 *     Index of PWR_CHANNEL object which has requested evaluation of this
 *     PWR_CHRELATIONSHIP object.
 *
 * @return Estimated power associated with this PWR_CHRELATIONSHIP object.
 */
#define PwrChRelationshipEvaluatemW(fname) LwS32 (fname) (PWR_CHRELATIONSHIP *pChRel, LwU8 chIdxEval)

/*!
 * @interface PWR_CHRELATIONSHIP
 *
 * Evaluates a PWR_CHRELATIONSHIP entry to retrieve the weight associated with
 * the relationship.
 *
 * @param[in] pChRel    Pointer to PWR_CHRELATIONSHIP object
 * @param[in] chIdxEval
 *     Index of PWR_CHANNEL object which has requested evaluation of this
 *     PWR_CHRELATIONSHIP object.
 *
 * @return Weight associated with this PWR_CHRELATIONSHIP object.
 */
#define PwrChRelationshipWeightGet(fname) LwSFXP20_12 (fname) (PWR_CHRELATIONSHIP *pChRel, LwU8 chIdxEval)

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
PwrChRelationshipEvaluatemW (pwrChRelationshipEvaluatemW)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipEvaluatemW");
PwrChRelationshipWeightGet  (pwrChRelationshipWeightGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipWeightGet");
BoardObjGrpIfaceModel10ObjSet           (pwrChRelationshipGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChRelationshipGrpIfaceModel10ObjSet_SUPER");

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetEntry   (pmgrPwrChRelIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pmgrPwrChRelIfaceModel10SetEntry");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrchrel_2x.h"
#include "pmgr/pwrchrel_weighted.h"
#include "pmgr/pwrchrel_weight.h"
#include "pmgr/pwrchrel_balanced_phase_est.h"
#include "pmgr/pwrchrel_balancing_pwm_weight.h"
#include "pmgr/pwrchrel_regulator_loss_est.h"
#include "pmgr/pwrchrel_regulator_eff_est_v1.h"

#endif // PWRCHREL_H
