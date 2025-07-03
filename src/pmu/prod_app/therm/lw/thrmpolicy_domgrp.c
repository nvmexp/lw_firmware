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
 * @file   thrmpolicy_domgrp.c
 * @brief  Domain Group Thermal Policy Model Management
 *
 * This module is a collection of functions managing and manipulating state
 * related to Domain Group Thermal Policy objects in the Thermal Policy Table.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/Thermal_Control/Thermal_Policy_Table_1.0_Specification
 *
 * A Domain Group policy is a generic object representing a behavior to
 * enforce on the GPU, as specified by a limit value. The behavior taken is
 * dependent on the policy itself.
 *
 * THERM_POLICY_DOMGRP is a virtual class/interface. It alone does not specify
 * a full thermal policy/mechanism. Classes which implement THERM_POLICY_DOMGRP
 * specify a full mechanism for control.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "therm/thrmpolicy_domgrp.h"
#include "pmu_objperf.h"
#include "g_pmurmifrpc.h"

/* ------------------------ Static Function Prototypes --------------------- */
static FLCN_STATUS s_thermPolicyDomGrpArbitrateLimits(void)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicyDomGrpArbitrateLimits");
static void       s_thermSendPerfLimitChange(LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS *pLimits)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermSendPerfLimitChange");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * Constructor for the THERM_POLICY_DOMGRP class.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermPolicyGrpIfaceModel10ObjSetImpl_DOMGRP
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_THERM_THERM_POLICY_DOMGRP_BOARDOBJ_SET *pDomGrpSet =
        (RM_PMU_THERM_THERM_POLICY_DOMGRP_BOARDOBJ_SET *)pBoardObjSet;
    LwBool                  bFirstConstruct = (*ppBoardObj == NULL);
    THERM_POLICY_DOMGRP    *pDomGrp;
    FLCN_STATUS             status;

    // Initialize the super class.
    status = thermPolicyGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDomGrp = (THERM_POLICY_DOMGRP *)*ppBoardObj;

    // Set class-specific data.
    pDomGrp->bRatedTdpVpstateFloor = pDomGrpSet->bRatedTdpVpstateFloor;
    pDomGrp->vpstateFloorIdx       = pDomGrpSet->vpstateFloorIdx;

    if (bFirstConstruct)
    {
        //
        // Only initialize the limits on the first construct. Subsequent
        // constructs need to leave the limits alone.
        //
        thermPolicyPerfLimitsClear(&pDomGrp->limits);
    }

    return status;
}

/*!
 * DOMGRP-class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
thermPolicyIfaceModel10GetStatus_DOMGRP
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_THERM_THERM_POLICY_DOMGRP_BOARDOBJ_GET_STATUS *pGetStatus;
    THERM_POLICY_DOMGRP                                  *pDomGrp;
    FLCN_STATUS                                           status;

    status = thermPolicyIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDomGrp    = (THERM_POLICY_DOMGRP *)pBoardObj;
    pGetStatus =
        (RM_PMU_THERM_THERM_POLICY_DOMGRP_BOARDOBJ_GET_STATUS *)pPayload;

    pGetStatus->limits = pDomGrp->limits;

    return FLCN_OK;
}

/*!
 * @brief   Accessor for PERF VF entry information.
 *
 * @param[in]   vfEntryIndex    Index of requested VF entry.
 *
 * @return Pointer to corresponding RM_PMU_PERF_VF_ENTRY_INFO structure.
 */
RM_PMU_PERF_VF_ENTRY_INFO *
thermPolicyDomGrpPerfVfEntryGet
(
    LwU8 vfEntryIdx
)
{
    RM_PMU_PERF_VF_ENTRY_INFO *pVfEntryInfo = PERF_VF_ENTRY_GET(vfEntryIdx);

    PMU_HALT_COND(pVfEntryInfo != NULL);
    return pVfEntryInfo;
}

/*!
 * @copydoc ThermPolicyDomGrpIsRatedTdpLimited.
 */
LwBool
thermPolicyDomGrpIsRatedTdpLimited
(
    THERM_POLICY_DOMGRP *pPolicy
)
{
    return pPolicy->bRatedTdpVpstateFloor;
}

/*!
 * @copy doc ThermPolicyDomGrpApplyLimits
 */
FLCN_STATUS
thermPolicyDomGrpApplyLimits
(
    THERM_POLICY_DOMGRP                    *pPolicy,
    LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS *pLimits
)
{
    if (memcmp(&pPolicy->limits, pLimits, sizeof(*pLimits)) == 0)
    {
        // Return if there is no change in limits.
        return FLCN_OK;
    }

    // Copy new limits for the policy.
    pPolicy->limits = *pLimits;

    // Arbitrate the policies' limits.
    return s_thermPolicyDomGrpArbitrateLimits();
}

/* ------------------------ Private Static Functions ----------------------- */
/*!
 * Arbitrates and applies perf. limits based on all the domain group policies'
 * requests.
 *
 * @return FLCN_OK
 *      Limits were successfully applied.
 * @return Other errors
 *      Errors propagated up from functions called.
 */
static FLCN_STATUS
s_thermPolicyDomGrpArbitrateLimits(void)
{
    THERM_POLICY *pPolicy;
    LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS limits;
    LwBoardObjIdx i;
    LwU8          j;

    // Set to no limits
    thermPolicyPerfLimitsClear(&limits);

    // Determine the most restrictive limits
    BOARDOBJGRP_ITERATOR_BEGIN(THERM_POLICY, pPolicy, i, NULL) // Really???
    {
        for (j = 0; j < LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_NUM_LIMITS; j++)
        {
            limits.limit[j] =
                LW_MIN(limits.limit[j],
                       ((THERM_POLICY_DOMGRP *)pPolicy)->limits.limit[j]);
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    //
    // RM will take care of applying the limits. The PMU is responsible for
    // sending a message to RM with all the limits. Also need to make sure that
    // the new limits that we want to send to RM aren't the same as the current
    // limits. This is done to minimize PMU->RM communication
    //
    if (memcmp(&Therm.lwrrentPerfLimits, &limits, sizeof(limits)) != 0)
    {
        Therm.lwrrentPerfLimits = limits;

        s_thermSendPerfLimitChange(&limits);
    }

    return FLCN_OK;
}

/*!
 * Sends perf limits to RM. RM is responsible for applying these limits.
 *
 * @param[in] limits    Array of THERM_POLICY limits
 *
 */
static void
s_thermSendPerfLimitChange
(
    LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS *pLimits
)
{
    FLCN_STATUS                             status;
    PMU_RM_RPC_STRUCT_THERM_PERF_LIMIT_SET  rpc;

    rpc.limits = *pLimits;

    // Send the Event RPC to the RM to change apply the limits.
    PMU_RM_RPC_EXELWTE_BLOCKING(status, THERM, PERF_LIMIT_SET, &rpc);
}
