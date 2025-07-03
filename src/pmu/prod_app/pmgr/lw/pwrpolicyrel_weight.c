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
 * @file  pwrpolicyrel_weight.c
 * @brief Interface specification for a PWR_POLICY_RELATIONSHIP_WEIGHT.
 *
 * Weighted Power Channel Relationships specify a weight/proporition of another
 * channel's power to use to callwlate the power of a given channel.  These
 * relationships specify a fixed pointe (UFXP 20.12) weight/proportion which
 * will scale the other channel's power.
 *
 *     relationshipPowermW = (weightFxp * channelPowermW + 0x800) >> 12
 *
 * @note The use of the UFXP 20.12 FXP weight will overflow at 1048576 mW.  This
 * will be a safe assumption for the foreseeable future.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy.h"
#include "pmgr/pwrpolicyrel_weight.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function prototypes -------------------- */
static LwU32 s_pwrPolicyRelationshipWeightScaleForGet(PWR_POLICY_RELATIONSHIP_WEIGHT *pRelWeight, LwU32 param)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "_s_pwrPolicyRelationshipWeightScaleForGet");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _WEIGHT PWR_POLICY_RELATIONSHIP object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_WEIGHT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT_BOARDOBJ_SET *pWeightSet =
        (RM_PMU_PMGR_PWR_POLICY_RELATIONSHIP_WEIGHT_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_RELATIONSHIP_WEIGHT                          *pWeight;
    FLCN_STATUS                                              status;

    // Construct and initialize parent-class object.
    status = pwrPolicyRelationshipGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj,
                                                      size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    pWeight = (PWR_POLICY_RELATIONSHIP_WEIGHT *)*ppBoardObj;

    // Set class-specific data.
    pWeight->weight = pWeightSet->weight;

    return status;
}

/*!
 * _WEIGHT power channel relationship implementation.
 *
 * @copydoc PwrPolicyRelationshipValueLwrrGet
 */
LwU32
pwrPolicyRelationshipValueLwrrGet_WEIGHT
(
    PWR_POLICY_RELATIONSHIP *pPolicyRel
)
{
    PWR_POLICY_RELATIONSHIP_WEIGHT *pWeight =
        (PWR_POLICY_RELATIONSHIP_WEIGHT *)pPolicyRel;

    // Scale value by the weight.
    return s_pwrPolicyRelationshipWeightScaleForGet(pWeight,
                pwrPolicyValueLwrrGet(pWeight->super.pPolicy));
}

/*!
 * _WEIGHT power channel relationship implementation.
 *
 * @copydoc PwrPolicyRelationshipLimitLwrrGet
 */
LwU32
pwrPolicyRelationshipLimitLwrrGet_WEIGHT
(
    PWR_POLICY_RELATIONSHIP *pPolicyRel
)
{
    PWR_POLICY_RELATIONSHIP_WEIGHT *pWeight =
        (PWR_POLICY_RELATIONSHIP_WEIGHT *)pPolicyRel;

    // Scale limit by the weight.
    return s_pwrPolicyRelationshipWeightScaleForGet(pWeight,
                pwrPolicyLimitLwrrGet(pWeight->super.pPolicy));
}

/*!
 * _WEIGHT power channel relationship implementation.
 *
 * @copydoc PwrPolicyRelationshipLimitInputGet
 */
LwU32
pwrPolicyRelationshipLimitInputGet_WEIGHT
(
    PWR_POLICY_RELATIONSHIP  *pPolicyRel,
    LwU8                      policyIdx
)
{
    PWR_POLICY_RELATIONSHIP_WEIGHT *pWeight =
        (PWR_POLICY_RELATIONSHIP_WEIGHT *)pPolicyRel;

    // Scale limit by the weight.
    return s_pwrPolicyRelationshipWeightScaleForGet(pWeight,
                pwrPolicyLimitArbInputGet(pWeight->super.pPolicy, policyIdx));
}

/*!
 * _WEIGHT power channel relationship implementation.
 *
 * @copydoc PwrPolicyRelationshipLimitInputSet
 */
FLCN_STATUS
pwrPolicyRelationshipLimitInputSet_WEIGHT
(
    PWR_POLICY_RELATIONSHIP  *pPolicyRel,
    LwU8                      policyIdx,
    LwBool                    bDryRun,
    LwU32                    *pLimit
)
{
    PWR_POLICY_RELATIONSHIP_WEIGHT *pWeight =
        (PWR_POLICY_RELATIONSHIP_WEIGHT *)pPolicyRel;
    FLCN_STATUS status = FLCN_OK;
    LwU32       limit  = *pLimit;

    // If limit = 0, there is no need to apply any weight
    if (limit == 0)
    {
        goto pwrPolicyRelationshipLimitInputSet_WEIGHT_done;
    }

    // If weight != 1.0, scale value by the weight.
    if (pWeight->weight != LW_TYPES_U32_TO_UFXP_X_Y(4, 12, 1))
    {
        //
        // 20.12 * 32.0 = 20.12 - Will overflow at 1048576 mW and that might not
        // always be a safe assumption.
        //
        // Detect possible overflow and apply a ceiling at 0xFFFFFFFF.
        //
        if ((limit != 0) && (pWeight->weight > LW_U32_MAX / limit))
        {
           limit = (LwUFXP20_12)LW_U32_MAX;
        }
        else
        {
            limit = (LwUFXP20_12)pWeight->weight * limit;
        }
        *pLimit = limit = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12, limit);
    }

pwrPolicyRelationshipLimitInputSet_WEIGHT_done:
    if (!bDryRun)
    {
        status = pwrPolicyLimitArbInputSet(pWeight->super.pPolicy, policyIdx, limit);
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Helper call to perform weight scaling in get() operations.
 */
static LwU32
s_pwrPolicyRelationshipWeightScaleForGet
(
    PWR_POLICY_RELATIONSHIP_WEIGHT *pRelWeight,
    LwU32                           param
)
{
    LwUFXP52_12 paramU52_12;

    // If weight != 1.0, scale value by the weight.
    if (pRelWeight->weight != LW_TYPES_U32_TO_UFXP_X_Y(4, 12, 1))
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_32BIT_OVERFLOW_FIXES))
        {
            //
            // 32.0 is typecast to 64.0
            // 64.0 << 12 => 52.12
            //
            paramU52_12 = LW_TYPES_U64_TO_UFXP_X_Y(52, 12, param);

            // 52.12 / 52.12 => 64.0 (param52_12 now contains a 64.0 value)
            lwU64DivRounded(
                &(paramU52_12),
                &(paramU52_12),
                &(LwUFXP52_12){ pRelWeight->weight });

            //
            // Check for 32-bit overflow and if true, cap param to LW_U32_MAX.
            // Otherwise, extract the lower 32 bits of the 64-bit result.
            //
            if (LwU64_HI32(paramU52_12))
            {
                param = LW_U32_MAX;
            }
            else
            {
                param = LwU64_LO32(paramU52_12);
            }
        }
        else
        {
            //
            // Division removes fractional precision, so need to first shift value
            // to have 12 fractional bits.
            //
            // Will overflow for value >= 1048576, this should be a safe assumption.
            //
            //   32.0 << 12 => 20.12
            // / 20.12      => 20.12
            // ---------------------
            //                 20.0
            //
            param = LW_TYPES_U32_TO_UFXP_X_Y(20, 12, param);
            param = LW_UNSIGNED_ROUNDED_DIV(param, pRelWeight->weight);
        }
    }

    return param;
}
