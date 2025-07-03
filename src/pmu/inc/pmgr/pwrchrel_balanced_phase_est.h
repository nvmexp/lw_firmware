/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel_balanced_phase_est.h
 * @brief @copydoc pwrchrel_balanced_phase_est.c
 */

#ifndef PWRCHREL_BALANCED_PHASE_EST_H
#define PWRCHREL_BALANCED_PHASE_EST_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchrel.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_CHRELATIONSHIP_BALANCED_PHASE_EST PWR_CHRELATIONSHIP_BALANCED_PHASE_EST;

/*!
 */
struct PWR_CHRELATIONSHIP_BALANCED_PHASE_EST
{
    /*!
     * @copydoc PWR_CHRELATIONSHIP
     */
    PWR_CHRELATIONSHIP super;

    /*!
     * Total number of phases by which to scale up the estimated power.
     */
    LwU8 numTotalPhases;
    /*!
     * Number of static phases which should be included in the evaluation of
     * this Channel Relationship.
     */
    LwU8 numStaticPhases;
    /*!
     * Index of first _BALANCE Power Policy Relationship representing a
     * balanced phase.  This relationship must be of type _BALANCE.
     */
    LwU8 balancedPhasePolicyRelIdxFirst;
    /*!
     * Number of balanced phases (each represented by a _BALANCED Power Policy
     * Relationship) which should be included in this relationship.  This
     * Channel Relationship object will evaluate all Power Policy Relationship
     * Indexes in the range [@ref balancedPhasePolicyRelIdxFirst, @ref
     * balancedPhasePolicyRelIdxFirst + @ref numBalancedPhases).
     */
    LwU8 numBalancedPhases;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHRELATIONSHIP interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (pwrChRelationshipGrpIfaceModel10ObjSet_BALANCED_PHASE_EST)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChRelationshipGrpIfaceModel10ObjSet_BALANCED_PHASE_EST");
PwrChRelationshipWeightGet  (pwrChRelationshipWeightGet_BALANCED_PHASE_EST)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChRelationshipWeightGet_BALANCED_PHASE_EST");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHREL_BALANCED_PHASE_EST_H
