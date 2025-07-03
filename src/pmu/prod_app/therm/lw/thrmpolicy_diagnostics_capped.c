/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  thrmpolicy_diagnostics_capped.c
 * @brief Thermal Policy Diagnostics for limiting temperature metrics
 *        (Bug 3287873)
 *
 * This module is a collection of functions managing and manipulating state
 * related to Thermal Policy Diagnostics for limiting temperature  metrics
 * (Bug 3287873).
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "therm/thrmpolicy_diagnostics.h"
#include "therm/thrmpolicy_diagnostics_capped.h"
#include "therm/thrmpolicy.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief To construct limiting temperature metrics metrics at a global and a per-channel level
 *
 * @param[in, out] pGlobalCapped     THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED pointer
 * @param[in]      pInfoGlobalCapped LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_CAPPED pointer
 */
FLCN_STATUS
thermPolicysDiagnosticsCappedConstruct
(
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED                    *pGlobalCapped,
    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_CAPPED *pInfoGlobalCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pGlobalCapped != NULL) && (pInfoGlobalCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysDiagnosticsCappedConstruct_exit);

    pGlobalCapped->info.bEnable = pInfoGlobalCapped->bEnable;

thermPolicysDiagnosticsCappedConstruct_exit:
    return status;
}

/*!
 * @brief To evaluate limiting temperature metrics at a per-channel level
 *
 * @param[in]      pChannelDiagnostics THERM_POLICYS_CHANNEL_DIAGNOSTICS pointer
 * @param[in, out] pChannelCapped      THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED pointer
 */
FLCN_STATUS
thermPolicysChannelDiagnosticsCappedEvaluate
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS        *pChannelDiagnostics,
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED *pChannelCapped
)
{
    THERM_POLICY                    *pPolicy;
    THERM_POLICY_DIAGNOSTICS        *pDiagnostics;
    THERM_POLICY_DIAGNOSTICS_CAPPED *pCapped;
    LwBoardObjIdx                    i;
    FLCN_STATUS                      status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pChannelDiagnostics != NULL) && (pChannelCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsCappedEvaluate_exit);

    THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_BEGIN(&pPolicy, &pDiagnostics, i,
        status, thermPolicysChannelDiagnosticsCappedEvaluate_exit)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsCappedGet(pDiagnostics, &pCapped),
            thermPolicysChannelDiagnosticsCappedEvaluate_exit);

        // The per-policy capped pointer cannot be NULL here.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pCapped != NULL),
            FLCN_ERR_ILWALID_STATE,
            thermPolicysChannelDiagnosticsCappedEvaluate_exit);

        //
        // Sanity check the thermal channel pointer for the current thermal
        // policy.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pPolicy->pChannel != NULL),
            FLCN_ERR_ILWALID_STATE,
            thermPolicysChannelDiagnosticsCappedEvaluate_exit);

        //
        // Update the bit corresponding to the current policy in the policy
        // mask for the thermal channel used by the policy. This bit reflects
        // if the current policy is capped (if set) or uncapped (if unset).
        //
        if (pCapped->info.bEnable &&
            (pChannelDiagnostics->chIdx ==
                BOARDOBJ_GET_GRP_IDX(&pPolicy->pChannel->super)))
        {
            if (pCapped->status.bIsCapped)
            {
                boardObjGrpMaskBitSet(
                    &pChannelCapped->status.thermalCappingPolicyMask, i);
            }
            else
            {
                boardObjGrpMaskBitClr(
                    &pChannelCapped->status.thermalCappingPolicyMask, i);
            }
        }
    }
    THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_END;

thermPolicysChannelDiagnosticsCappedEvaluate_exit:
    return status;
}

/*!
 * @brief   To Get Status of limiting temperature metrics at a per-channel
 *          level
 *
 * @param[in]      pChannelCapped       THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED pointer
 * @param[in, out] pStatusChannelCapped LW2080_CTRL_THERMAL_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_CAPPED pointer
 */
FLCN_STATUS
thermPolicysChannelDiagnosticsCappedGetStatus
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED                      *pChannelCapped,
    LW2080_CTRL_THERMAL_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_CAPPED *pStatusChannelCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pChannelCapped != NULL) &&
         (pStatusChannelCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsCappedGetStatus_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskExport_E32(
            &pChannelCapped->status.thermalCappingPolicyMask,
            &pStatusChannelCapped->thermalCappingPolicyMask),
        thermPolicysChannelDiagnosticsCappedGetStatus_exit);

thermPolicysChannelDiagnosticsCappedGetStatus_exit:
    return status;
}

/*!
 * @brief To construct limiting temperature metrics at a per-policy level
 *
 * @param[in]  pCapped     THERM_POLICY_DIAGNOSTICS_CAPPED pointer
 * @param[out] pInfoCapped LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_CAPPED pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsCappedConstruct
(
    THERM_POLICY_DIAGNOSTICS_CAPPED                    *pCapped,
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_CAPPED *pInfoCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pCapped != NULL) && (pInfoCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsCappedConstruct_exit);

    pCapped->info.bEnable = pInfoCapped->bEnable;

thermPolicyDiagnosticsCappedConstruct_exit:
    return status;
}

/*!
 * @brief To evaluate limiting temperature metrics at a per-policy level
 *
 * @param[in]      pPolicy THERM_POLICY pointer
 * @param[in, out] pCapped THERM_POLICY_DIAGNOSTICS_CAPPED pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsCappedEvaluate
(
    THERM_POLICY                    *pPolicy,
    THERM_POLICY_DIAGNOSTICS_CAPPED *pCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPolicy != NULL) && (pCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsCappedEvaluate_exit);

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_THERMAL_POLICY_TYPE_DTC_PWR:
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicyDiagnosticsCappedEvaluate_DTC_PWR(pPolicy, pCapped),
                thermPolicyDiagnosticsCappedEvaluate_exit);
            break;
        }

        default:
            // Unsupported Thermal Policy type.
            PMU_ASSERT_OK_OR_GOTO(status,
                FLCN_ERR_NOT_SUPPORTED,
                thermPolicyDiagnosticsCappedEvaluate_exit);
    }

thermPolicyDiagnosticsCappedEvaluate_exit:
    return status;
}

/*!
 * @brief To Get Status of limiting temperature metrics at a per-policy level
 *
 * @param[in]  pCapped       THERM_POLICY_DIAGNOSTICS_CAPPED pointer
 * @param[out] pStatusCapped LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_CAPPED pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsCappedGetStatus
(
    THERM_POLICY_DIAGNOSTICS_CAPPED                      *pCapped,
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_CAPPED *pStatusCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pCapped != NULL) && (pStatusCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsCappedGetStatus_exit);

    pStatusCapped->bIsCapped = pCapped->status.bIsCapped;

thermPolicyDiagnosticsCappedGetStatus_exit:
    return status;
}
