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
 * @file    pwrpolicyrel.h
 * @copydoc pwrpolicyrel.c
 */

#ifndef PWRPOLICYREL_H
#define PWRPOLICYREL_H

/* ------------------------- System Includes ------------------------------- */
#include "pmgr/pwrpolicy.h"

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PWR_POLICY_RELATIONSHIP PWR_POLICY_RELATIONSHIP, PWR_POLICY_RELATIONSHIP_BASE;

/* ------------------------------ Macros ------------------------------------*/
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PWR_POLICY_RELATIONSHIP \
    (&(Pmgr.pwr.policyRels.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define PWR_POLICY_REL_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(PWR_POLICY_RELATIONSHIP, (_objIdx)))

/*!
 * Helper accessor to the PWR_POLICY object
 */
#define PWR_POLICY_RELATIONSHIP_POLICY_GET(pPolicyRel)                        \
    (pPolicyRel->pPolicy)

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Main structure representing a power capping channel
 */
struct PWR_POLICY_RELATIONSHIP
{
    /*!
     * BOARDOBJ super-class.
     */
    BOARDOBJ    super;

    /*!
     * PWR_POLICY pointer for this relationship.
     */
    PWR_POLICY *pPolicy;
};

/*!
 * @interface PWR_POLICY_RELATIONSHIP
 *
 * Loads a PWR_POLICY_RELATIONSHIP. This function will be called after
 * pwrPolicyRelationshipConstruct() finished and will program any necessary HW
 * state.
 *
 * @param[in] pPolicyRel  Pointer to PWR_POLICY_RELATIONSHIP object
 *
 * @return FLCN_OK
 *      Relationship successfully loaded.
 *
 * @return Other errors
 *      Unexpected errors propogated up from type-specific implementations.
 */
#define PwrPolicyRelationshipLoad(fname) FLCN_STATUS (fname) (PWR_POLICY_RELATIONSHIP *pPolicyRel)

/*!
 * @interface PWR_POLICY_RELATIONSHIP
 *
 * TODO
 *
 * @param[in] pPolicyRel  Pointer to PWR_POLICY_RELATIONSHIP object
 *
 * @return valueLwrr from the corresponding PWR_POLICY with any necessary
 *      adjustments for scaling.
 */
#define PwrPolicyRelationshipValueLwrrGet(fname) LwU32 (fname) (PWR_POLICY_RELATIONSHIP *pPolicyRel)

/*!
 * @interface PWR_POLICY_RELATIONSHIP
 *
 * Retrieves the current limit of the encapsulated PWR_POLICY object.
 *
 * @param[in]   pPolicyRel  Pointer to PWR_POLICY_RELATIONSHIP object
 *
 * @return  Current limit value of the encapsulated PWR_POLICY object.
 */
#define PwrPolicyRelationshipLimitLwrrGet(fname) LwU32 (fname) (PWR_POLICY_RELATIONSHIP *pPolicyRel)

/*!
 * @interface PWR_POLICY_RELATIONSHIP
 *
 * Interface by which a client PWR_POLICY can get the most recent limit value
 * it requested for this PWR_POLICY. This interface is used by PWR_POLICYs which
 * take corrective actions by altering the power limits of other PWR_POLICYs.
 *
 * @param[in]      pPolicyRel   PWR_POLICY_RELATIONSHIP pointer
 * @param[in]      pwrPolicyIdx PWR_POLICY index value for the requesting client.
 *
 * @return
 *     Last requested Limit value from given client ref@ pwrPolicyIdx.
 */
#define PwrPolicyRelationshipLimitInputGet(fname) LwU32 (fname) (PWR_POLICY_RELATIONSHIP *pPolicyRel, LwU8 policyIdx)

/*!
 * @interface PWR_POLICY_RELATIONSHIP
 *
 * Interface by which a client PWR_POLICY can request a new limit value for this
 * PWR_POLICY.  This interface is used by PWR_POLICYs which take corrective
 * actions by altering the power limits of other PWR_POLICYs.
 *
 * @param[in]      pPolicyRel   PWR_POLICY_RELATIONSHIP pointer
 * @param[in]      pwrPolicyIdx PWR_POLICY index value for the requesting client.
 * @param[in]      bDryRun
 *     Boolean indicating that the PWR_POLICY_REL should only do any applicable
 *     limit scaling and update the limit value in the buffer, but not actually
 *     apply the requested limit to the applicable PWR_POLICY objects.
 * @param[in/out]  pLimitValue
 *     Pointer to limit value this client is requesting.
 *
 * @return FLCN_STATUS returned by @ref PwrPolicyLimitInputSet().
 */
#define PwrPolicyRelationshipLimitInputSet(fname) FLCN_STATUS (fname) (PWR_POLICY_RELATIONSHIP *pPolicyRel, LwU8 policyIdx, LwBool bDryRun, LwU32 *pLimit)

/*!
 * Helper function to get the latest channel power reading for given channel
 * that is being used by the pwr policy relationship
 *
 * @param[in]   pPolicyRel  PWR_POLICY_RELATIONSHIP pointer
 * @param[in]   pMon        PWR_MONITOR pointer
 * @param[in]   pPayload    Pointer to queried tuples data
 *
 * @return      FLCN_OK     Successfully fatched current power reading
 * @return      FLCN_ERR_NOT_SUPPORTED
 *      The limitUnit for the policy that is refered by this policy rel is not recognized
 */
#define PwrPolicyRelationshipChannelPwrGet(fname) FLCN_STATUS (fname)(PWR_POLICY_RELATIONSHIP *pPolicyRel, PWR_MONITOR *pMon, PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload)

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY_RELATIONSHIP interfaces
 */
PwrPolicyRelationshipLoad          (pwrPolicyRelationshipLoad)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyRelationshipLoad");
BoardObjIfaceModel10GetStatus                      (pwrPolicyRelationshipIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyRelationshipIfaceModel10GetStatus");
PwrPolicyRelationshipValueLwrrGet  (pwrPolicyRelationshipValueLwrrGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipValueLwrrGet");
PwrPolicyRelationshipLimitLwrrGet  (pwrPolicyRelationshipLimitLwrrGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipLimitLwrrGet");
PwrPolicyRelationshipLimitInputGet (pwrPolicyRelationshipLimitInputGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipLimitInputGet");
PwrPolicyRelationshipLimitInputSet (pwrPolicyRelationshipLimitInputSet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipLimitInputSet");
PwrPolicyRelationshipChannelPwrGet (pwrPolicyRelationshipChannelPwrGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyRelationshipChannelPwrGet");

/*!
 * Constructor for the PWR_POLICY_RELATIONSHIP super-class.
 */
BoardObjGrpIfaceModel10ObjSet                  (pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrpolicyrel_balance.h"
#include "pmgr/pwrpolicyrel_weight.h"

#endif // PWRPOLICYREL_H
