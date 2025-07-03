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
 * @file  thrmpolicy_diagnostics_limit_countdown.c
 * @brief Thermal Policy Diagnostics for limitCountdown metrics (Bug 3276847)
 *
 * This module is a collection of functions managing and manipulating state
 * related to Thermal Policy Diagnostics for limitCountdown metrics (Bug 3276847).
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "therm/thrmpolicy_diagnostics.h"
#include "therm/thrmpolicy_diagnostics_limit_countdown.h"
#include "therm/thrmpolicy.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief To construct limitCountdown metrics at a policyGrp and a per-channel level
 *
 * @param[in, out] pGlobalLimitCountdown     THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 * @param[in]      pInfoGlobalLimitCountdown LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicysDiagnosticsLimitCountdownConstruct
(
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN                    *pGlobalLimitCountdown,
    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN *pInfoGlobalLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pGlobalLimitCountdown != NULL) && (pInfoGlobalLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysDiagnosticsLimitCountdownConstruct_exit);

    pGlobalLimitCountdown->info.bEnable = pInfoGlobalLimitCountdown->bEnable;

thermPolicysDiagnosticsLimitCountdownConstruct_exit:
    return status;
}

/*!
 * @brief To evaluate limitCountdown metrics at a policyGrp level
 *
 * @param[in, out] pGlobalLimitCountdown     THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicysGlobalDiagnosticsLimitCountdownEvaluate
(
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN *pGlobalLimitCountdown
)
{
    THERM_POLICY                             *pPolicy;
    THERM_POLICY_DIAGNOSTICS                 *pDiagnostics;
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown;
    LwBoardObjIdx                             i;
    FLCN_STATUS                               status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pGlobalLimitCountdown != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysGlobalDiagnosticsLimitCountdownEvaluate_exit);

    THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_BEGIN(&pPolicy, &pDiagnostics, i,
        status, thermPolicysGlobalDiagnosticsLimitCountdownEvaluate_exit)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsLimitCountdownGet(pDiagnostics,
                &pLimitCountdown),
            thermPolicysGlobalDiagnosticsLimitCountdownEvaluate_exit);

        // The per-policy limitCountdown pointer cannot be NULL here.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pLimitCountdown != NULL),
            FLCN_ERR_ILWALID_ARGUMENT,
            thermPolicysGlobalDiagnosticsLimitCountdownEvaluate_exit);

        //
        // Update the limitCountdown value at a policyGrp level if the
        // limitCountdown value of the current policy is the lowest we
        // have seen so far.
        //
        if (pLimitCountdown->info.bEnable)
        {
            pGlobalLimitCountdown->status.limitCountdown =
                LW_MIN(pGlobalLimitCountdown->status.limitCountdown,
                    pLimitCountdown->status.limitCountdown);
        }
    }
    THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_END;

thermPolicysGlobalDiagnosticsLimitCountdownEvaluate_exit:
    return status;
}

/*!
 * @brief To evaluate limitCountdown metrics at a per-channel level
 *
 * @param[in]      pChannelDiagnostics    THERM_POLICYS_CHANNEL_DIAGNOSTICS pointer
 * @param[in, out] pChannelLimitCountdown THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicysChannelDiagnosticsLimitCountdownEvaluate
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS                 *pChannelDiagnostics,
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN *pChannelLimitCountdown
)
{
    THERM_POLICY                             *pPolicy;
    THERM_POLICY_DIAGNOSTICS                 *pDiagnostics;
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown;
    LwBoardObjIdx                             i;
    FLCN_STATUS                               status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pChannelDiagnostics != NULL) && (pChannelLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsLimitCountdownEvaluate_exit);

    THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_BEGIN(&pPolicy, &pDiagnostics, i,
        status, thermPolicysChannelDiagnosticsLimitCountdownEvaluate_exit)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsLimitCountdownGet(pDiagnostics,
                &pLimitCountdown),
            thermPolicysChannelDiagnosticsLimitCountdownEvaluate_exit);

        // The per-policy limitCountdown pointer cannot be NULL here.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pLimitCountdown != NULL),
            FLCN_ERR_ILWALID_STATE,
            thermPolicysChannelDiagnosticsLimitCountdownEvaluate_exit);

        //
        // Sanity check the thermal channel pointer for the current thermal
        // policy.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pPolicy->pChannel != NULL),
            FLCN_ERR_ILWALID_STATE,
            thermPolicysChannelDiagnosticsLimitCountdownEvaluate_exit);

        //
        // Update the limitCountdown value at a per-channel level if the
        // limitCountdown value for the thermal channel used by the current
        // policy is the lowest we have seen so far.
        //
        if (pLimitCountdown->info.bEnable &&
            (pChannelDiagnostics->chIdx ==
                BOARDOBJ_GET_GRP_IDX(&pPolicy->pChannel->super)))
        {
            pChannelLimitCountdown->status.limitCountdown =
                LW_MIN(pChannelLimitCountdown->status.limitCountdown,
                    pLimitCountdown->status.limitCountdown);
        }
    }
    THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_END;

thermPolicysChannelDiagnosticsLimitCountdownEvaluate_exit:
    return status;
}

/*!
 * @brief   To Get Status of limitCountdown metrics at a policyGrp level
 *
 * @param[in]      pGlobalLimitCountdown       THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 * @param[in, out] pStatusGlobalLimitCountdown LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicysGlobalDiagnosticsLimitCountdownGetStatus
(
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN                      *pGlobalLimitCountdown,
    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN *pStatusGlobalLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pGlobalLimitCountdown != NULL) &&
         (pStatusGlobalLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysGlobalDiagnosticsLimitCountdownGetStatus_exit);

    pStatusGlobalLimitCountdown->limitCountdown =
        pGlobalLimitCountdown->status.limitCountdown;

thermPolicysGlobalDiagnosticsLimitCountdownGetStatus_exit:
    return status;
}

/*!
 * @brief   To Get Status of limitCountdowns metrics at a per-channel level
 *
 * @param[in]      pChannelLimitCountdown       THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 * @param[in, out] pStatusChannelLimitCountdown LW2080_CTRL_THERMAL_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicysChannelDiagnosticsLimitCountdownGetStatus
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN                      *pChannelLimitCountdown,
    LW2080_CTRL_THERMAL_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN *pStatusChannelLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pChannelLimitCountdown != NULL) &&
         (pStatusChannelLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsLimitCountdownGetStatus_exit);

    pStatusChannelLimitCountdown->limitCountdown =
        pChannelLimitCountdown->status.limitCountdown;

thermPolicysChannelDiagnosticsLimitCountdownGetStatus_exit:
    return status;
}

/*!
 * @brief To construct limitCountdown metrics at a per-policy level
 *
 * @param[in, out] pLimitCountdown     THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 * @param[in]      pInfoLimitCountdown LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsLimitCountdownConstruct
(
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN                    *pLimitCountdown,
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN *pInfoLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pLimitCountdown != NULL) && (pInfoLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsLimitCountdownConstruct_exit);

    pLimitCountdown->info.bEnable = pInfoLimitCountdown->bEnable;

thermPolicyDiagnosticsLimitCountdownConstruct_exit:
    return status;
}

/*!
 * @brief To evaluate limitCountdown metrics at a per-policy level
 *
 * @param[in]      pPolicy             THERM_POLICY pointer
 * @param[in, out] pLimitCountdown     THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsLimitCountdownEvaluate
(
    THERM_POLICY                             *pPolicy,
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPolicy != NULL) && (pLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsLimitCountdownEvaluate_exit);

    // Update the limitCountdown value at a per-policy level.
    pLimitCountdown->status.limitCountdown = THERM_POLICY_LIMIT_GET(pPolicy) -
                                                 pPolicy->channelReading;

thermPolicyDiagnosticsLimitCountdownEvaluate_exit:
    return status;
}

/*!
 * @brief To Get Status of limitCountdown metrics at a per-policy level
 *
 * @param[in, out] pLimitCountdown       THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN pointer
 * @param[in]      pStatusLimitCountdown LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsLimitCountdownGetStatus
(
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN                      *pLimitCountdown,
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN *pStatusLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pLimitCountdown != NULL) && (pStatusLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsLimitCountdownGetStatus_exit);

    pStatusLimitCountdown->limitCountdown =
        pLimitCountdown->status.limitCountdown;

thermPolicyDiagnosticsLimitCountdownGetStatus_exit:
    return status;
}