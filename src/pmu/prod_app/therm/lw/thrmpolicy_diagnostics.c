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
 * @file  thrmpolicy_diagnostics.c
 * @brief Thermal Policy Diagnostics
 *
 * This module is a collection of functions managing and manipulating state
 * related to Thermal Policy Diagnostics.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "therm/objtherm.h"
#include "therm/thrmpolicy_diagnostics.h"
#include "therm/thrmpolicy.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Static Function Prototypes --------------------- */
static FLCN_STATUS s_thermPolicysChannelsDiagnosticsChIdxsPopulate(THERM_POLICYS_DIAGNOSTICS *pDiagnostics,
        BOARDOBJGRPMASK_E32 *pActiveChannelMask)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "s_thermPolicysChannelsDiagnosticsChIdxsPopulate");

static FLCN_STATUS s_thermPolicysGlobalDiagnosticsReinit(THERM_POLICYS_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "s_thermPolicysGlobalDiagnosticsReinit");

static FLCN_STATUS s_thermPolicysChannelsDiagnosticsReinit(THERM_POLICYS_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "s_thermPolicysChannelsDiagnosticsReinit");

static FLCN_STATUS s_thermPolicysGlobalDiagnosticsEvaluate(THERM_POLICYS_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicysGlobalDiagnosticsEvaluate");

static FLCN_STATUS s_thermPolicysChannelsDiagnosticsEvaluate(THERM_POLICYS_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "s_thermPolicysChannelsDiagnosticsEvaluate");

static FLCN_STATUS s_thermPolicysGlobalDiagnosticsGetStatus(THERM_POLICYS_DIAGNOSTICS *pDiagnostics,
        LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicysGlobalDiagnosticsGetStatus");

static FLCN_STATUS s_thermPolicysChannelsDiagnosticsGetStatus(THERM_POLICYS_DIAGNOSTICS *pDiagnostics,
        LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicysChannelsDiagnosticsGetStatus");

static FLCN_STATUS s_thermPolicysChannelsDiagnosticsEvaluate(THERM_POLICYS_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicysChannelsDiagnosticsEvaluate");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief To construct Thermal Policy Diagnostics metrics at a policyGrp and a
 *        per-channel level
 *
 * @param[in]      pBObjGrp     BOARDOBJGRP pointer
 * @param[in, out] pDiagnostics THERM_POLICYS_DIAGNOSTICS pointer
 * @param[in]      pInfo        LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_INFO pointer
 */
FLCN_STATUS
thermPolicysDiagnosticsConstruct
(
    BOARDOBJGRP                                  *pBObjGrp,
    THERM_POLICYS_DIAGNOSTICS                    *pDiagnostics,
    LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_INFO *pInfo
)
{
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN  *pGlobalLimitCountdown;
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED           *pGlobalCapped;
    BOARDOBJGRPMASK_E32                                activeChannelMask;
    LwU8                                               numChannelsLwrr;
    FLCN_STATUS                                        status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (pInfo != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysDiagnosticsConstruct_exit);

    // Construct limitCountdown metrics (Bug 3276847) at a policyGrp level.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsLimitCountdownGet(&pDiagnostics->global,
            &pGlobalLimitCountdown),
        thermPolicysDiagnosticsConstruct_exit);
    if (pGlobalLimitCountdown != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysDiagnosticsLimitCountdownConstruct(
                pGlobalLimitCountdown, &pInfo->global.limitCountdown),
            thermPolicysDiagnosticsConstruct_exit);
    }

    //
    // Construct limiting temperature metrics (Bug 3287873) at a policyGrp
    // level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsCappedGet(&pDiagnostics->global,
            &pGlobalCapped),
        thermPolicysDiagnosticsConstruct_exit);
    if (pGlobalCapped != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysDiagnosticsCappedConstruct(
                pGlobalCapped, &pInfo->global.capped),
            thermPolicysDiagnosticsConstruct_exit);
    }

    // Obtain the Active Channel Mask sent from RM to PMU.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_thermPolicysDiagnosticsExtractActiveChannelMask(&activeChannelMask,
            pInfo),
        thermPolicysDiagnosticsConstruct_exit);

    //
    // Extract the number of unique thermal channels being used by thermal
    // policies based on the number of set bits in the Active Channel Mask
    // communicated from RM to PMU.
    //
    pDiagnostics->numChannels =
        boardObjGrpMaskBitSetCount(&activeChannelMask);

    //
    // Ensure that the number of unique thermal channels being used by
    // thermal policies is within the maximum number of thermal channels
    // allowed.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics->numChannels <= LW2080_CTRL_THERMAL_POLICY_MAX_CHANNELS),
        FLCN_ERR_ILWALID_STATE,
        thermPolicysDiagnosticsConstruct_exit);

    //
    // Allocate memory for Thermal Policy Diagnostic metrics at a per-channel
    // level.
    //
    if ((pDiagnostics->pChannels == NULL) &&
        (pDiagnostics->numChannels != 0))
    {
        //
        // Dynamic memory allocation for the buffer holding diagnostic metrics
        // at a per-channel level.
        //
        pDiagnostics->pChannels = lwosCallocType(pBObjGrp->ovlIdxDmem,
            pDiagnostics->numChannels, THERM_POLICYS_CHANNEL_DIAGNOSTICS);

        // Ensure that the memory allocation operation did not fail.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pDiagnostics->pChannels != NULL),
            FLCN_ERR_NO_FREE_MEM,
            thermPolicysDiagnosticsConstruct_exit);

        //
        // Populate the channel index values for entries in the Channel
        // Diagnostics buffer.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            s_thermPolicysChannelsDiagnosticsChIdxsPopulate(pDiagnostics,
                &activeChannelMask),
            thermPolicysDiagnosticsConstruct_exit);
    }
    else
    {
        //
        // Extract the number of unique thermal channels being used by thermal
        // policies based on the number of set bits in the Active Channel Mask
        // communicated from RM to PMU.
        //
        numChannelsLwrr =
            boardObjGrpMaskBitSetCount(&activeChannelMask);

        //
        // Check that the information about the number of unique thermal
        // channels being used by thermal policies has not changed across
        // construct calls.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (numChannelsLwrr == pDiagnostics->numChannels),
            FLCN_ERR_ILWALID_STATE,
            thermPolicysDiagnosticsConstruct_exit);
    }

    //
    // Re-initialize dynamic functionality of Thermal Policy Diagnostic metrics
    // at a policyGrp and a per-channel level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysDiagnosticsReinit(pDiagnostics, LW_FALSE),
        thermPolicysDiagnosticsConstruct_exit);

thermPolicysDiagnosticsConstruct_exit:
    return status;
}

/*!
 * @brief To evaluate Thermal Policy Diagnostics metrics at a policyGrp and a
 *        per-channel level
 *
 * @param[in]  pDiagnostics THERM_POLICYS_DIAGNOSTICS pointer
 */
FLCN_STATUS
thermPolicysDiagnosticsEvaluate
(
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysDiagnosticsEvaluate_exit);

    // Evaluate Thermal Policy Diagnostic metrics at a policyGrp level.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_thermPolicysGlobalDiagnosticsEvaluate(pDiagnostics),
        thermPolicysDiagnosticsEvaluate_exit);

    // Evaluate Thermal Policy Diagnostic metrics at a per-channel level.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_thermPolicysChannelsDiagnosticsEvaluate(pDiagnostics),
        thermPolicysDiagnosticsEvaluate_exit);

thermPolicysDiagnosticsEvaluate_exit:
    return status;
}

/*!
 * @brief   Re-initialize dynamic functionality of Thermal Policy Diagnostic
 *          metrics at a policyGrp and a per-channel level.
 *
 * @param[in]   pDiagnostics  Pointer to a THERM_POLICYS_DIAGNOSTICS object
 * @param[in]   bReinitPolicy Boolean denoting whether dynamic state of Thermal
 *                            Policy diagnostic features is to be reinitialized
 *                            at a per-policy level for all thermal Policies
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
FLCN_STATUS
thermPolicysDiagnosticsReinit
(
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics,
    LwBool                     bReinitPolicy
)
{
    THERM_POLICY             *pPolicy;
    THERM_POLICY_DIAGNOSTICS *pPolicyDiagnostics;
    LwBoardObjIdx             i;
    FLCN_STATUS               status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysDiagnosticsReinit_exit);

    //
    // Re-initialize dynamic functionality of Thermal Policy Diagnostic metrics
    // at a policyGrp level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_thermPolicysGlobalDiagnosticsReinit(pDiagnostics),
        thermPolicysDiagnosticsReinit_exit);

    //
    // Re-initialize dynamic functionality of Thermal Policy Diagnostic metrics
    // at a per-channel level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_thermPolicysChannelsDiagnosticsReinit(pDiagnostics),
        thermPolicysDiagnosticsReinit_exit);

    //
    // If requested by the caller, re-initialize dynamic functionality of
    // Thermal Policy Diagnostic metrics at a per-policy level for all Thermal
    // Policies.
    //
    if (bReinitPolicy)
    {
        THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_BEGIN(&pPolicy,
            &pPolicyDiagnostics, i, status,
            thermPolicysDiagnosticsReinit_exit)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicyDiagnosticsReinit(pPolicyDiagnostics),
                thermPolicysDiagnosticsReinit_exit);
        }
        THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_END;
    }

thermPolicysDiagnosticsReinit_exit:
    return status;
}

/*!
 * @brief To Get Status of Thermal Policy Diagnostics metrics at a policyGrp
 *        and a per-channel level
 *
 * @param[in]      pDiagnostics THERM_POLICYS_DIAGNOSTICS pointer
 * @param[in, out] pStatus        LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_INFO pointer
 */
FLCN_STATUS
thermPolicysDiagnosticsGetStatus
(
    THERM_POLICYS_DIAGNOSTICS                      *pDiagnostics,
    LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS *pStatus
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (pStatus != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysDiagnosticsGetStatus_exit);

    // Get status of Thermal Policy Diagnostic metrics at a policyGrp level.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_thermPolicysGlobalDiagnosticsGetStatus(pDiagnostics, pStatus),
        thermPolicysDiagnosticsGetStatus_exit);

    // Get Status of Thermal Policy Diagnostic metrics at a per-channel level.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_thermPolicysChannelsDiagnosticsGetStatus(pDiagnostics, pStatus),
        thermPolicysDiagnosticsGetStatus_exit);

thermPolicysDiagnosticsGetStatus_exit:
    return status;
}

/*!
 * @brief To construct Thermal Policy Diagnostics metrics at a per-policy level
 *
 * @param[in]  pDiagnostics THERM_POLICY_DIAGNOSTICS pointer
 * @param[out] pInfo        LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsConstruct
(
    THERM_POLICY_DIAGNOSTICS                    *pDiagnostics,
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO *pInfo
)
{
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown;
    THERM_POLICY_DIAGNOSTICS_CAPPED          *pCapped;
    FLCN_STATUS                               status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (pInfo != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsConstruct_exit);

    // Construct limitCountdown metrics (Bug 3276847) at a per-policy level.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsLimitCountdownGet(pDiagnostics, &pLimitCountdown),
        thermPolicyDiagnosticsConstruct_exit);
    if (pLimitCountdown != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsLimitCountdownConstruct(
                &pDiagnostics->limitCountdown, &pInfo->limitCountdown),
            thermPolicyDiagnosticsConstruct_exit);
    }

    // Construct limiting temperature metrics (Bug 3287873) at a per-policy level.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsCappedGet(pDiagnostics, &pCapped),
        thermPolicyDiagnosticsConstruct_exit);
    if (pCapped != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsCappedConstruct(
                &pDiagnostics->capped, &pInfo->capped),
            thermPolicyDiagnosticsConstruct_exit);
    }

    //
    // Re-initialize dynamic functionality of Thermal Policy Diagnostic metrics
    // at a per-policy level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsReinit(pDiagnostics),
        thermPolicyDiagnosticsConstruct_exit);

thermPolicyDiagnosticsConstruct_exit:
    return status;
}

/*!
 * @brief   Re-initialize dynamic functionality of Thermal Policy Diagnostic
 *          metrics at a per-policy level.
 *
 * @param[in]   pDiagnostics  Pointer to a THERM_POLICY_DIAGNOSTICS object
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
FLCN_STATUS
thermPolicyDiagnosticsReinit
(
    THERM_POLICY_DIAGNOSTICS *pDiagnostics
)
{
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown;
    THERM_POLICY_DIAGNOSTICS_CAPPED          *pCapped;
    FLCN_STATUS                               status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsReinit_exit);

    //
    // Re-initialize dynamic functionality of limitCountdown metrics
    // (Bug 3276847) at a per-policy level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsLimitCountdownGet(pDiagnostics,
            &pLimitCountdown),
        thermPolicyDiagnosticsReinit_exit);
    if ((pLimitCountdown != NULL) &&
         pLimitCountdown->info.bEnable)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsLimitCountdownReinit(pLimitCountdown),
            thermPolicyDiagnosticsReinit_exit);
    }

    //
    // Re-initialize dynamic functionality of limiting temperature metrics
    // (Bug 3287873) at a per-policy level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsCappedGet(pDiagnostics,
            &pCapped),
        thermPolicyDiagnosticsReinit_exit);
    if ((pCapped != NULL) &&
         pCapped->info.bEnable)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsCappedReinit(pCapped),
            thermPolicyDiagnosticsReinit_exit);
    }

thermPolicyDiagnosticsReinit_exit:
    return status;
}

/*!
 * @brief To evaluate Thermal Policy Diagnostics metrics at a per-policy level
 *
 * @param[in]  pPolicy      THERM_POLICY pointer
 * @param[in]  pDiagnostics THERM_POLICY_DIAGNOSTICS pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsEvaluate
(
    THERM_POLICY             *pPolicy,
    THERM_POLICY_DIAGNOSTICS *pDiagnostics
)
{
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown;
    THERM_POLICY_DIAGNOSTICS_CAPPED          *pCapped;
    FLCN_STATUS                               status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPolicy != NULL) && (pDiagnostics != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsEvaluate_exit);

    // Evaluate limitCountdown metrics (Bug 3276847) at a per-policy level.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsLimitCountdownGet(pDiagnostics, &pLimitCountdown),
        thermPolicyDiagnosticsEvaluate_exit);
    if ((pLimitCountdown != NULL) &&
         pLimitCountdown->info.bEnable)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsLimitCountdownEvaluate(pPolicy,
                pLimitCountdown),
            thermPolicyDiagnosticsEvaluate_exit);
    }

    //
    // Evaluate limiting temperature metrics (Bug 3287873) at a per-policy
    // level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsCappedGet(pDiagnostics, &pCapped),
        thermPolicyDiagnosticsEvaluate_exit);
    if ((pCapped != NULL) &&
         pCapped->info.bEnable)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsCappedEvaluate(pPolicy, pCapped),
            thermPolicyDiagnosticsEvaluate_exit);
    }

thermPolicyDiagnosticsEvaluate_exit:
    return status;
}

/*!
 * @brief To Get Status of Thermal Policy Diagnostics metrics at a per-policy
 *        level
 *
 * @param[in]  pDiagnostics THERM_POLICY_DIAGNOSTICS pointer
 * @param[out] pStatus      LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS pointer
 */
FLCN_STATUS
thermPolicyDiagnosticsGetStatus
(
    THERM_POLICY_DIAGNOSTICS                      *pDiagnostics,
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS *pStatus
)
{
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown;
    THERM_POLICY_DIAGNOSTICS_CAPPED          *pCapped;
    FLCN_STATUS                               status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (pStatus != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsGetStatus_exit);

    //
    // Get Status of limitCountdown metrics (Bug 3276847) at a per-policy
    // level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsLimitCountdownGet(pDiagnostics, &pLimitCountdown),
        thermPolicyDiagnosticsGetStatus_exit);
    if (pLimitCountdown != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsLimitCountdownGetStatus(
                &pDiagnostics->limitCountdown, &pStatus->limitCountdown),
            thermPolicyDiagnosticsGetStatus_exit);
    }

    //
    // Get Status of limiting temperature metrics (Bug 3287873) at a per-policy
    // level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicyDiagnosticsCappedGet(pDiagnostics, &pCapped),
        thermPolicyDiagnosticsGetStatus_exit);
    if (pCapped != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicyDiagnosticsCappedGetStatus(
                &pDiagnostics->capped, &pStatus->capped),
            thermPolicyDiagnosticsGetStatus_exit);
    }

thermPolicyDiagnosticsGetStatus_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Populate the channel index values for entries in the Channel
 *          Diagnostics buffer
 *
 * @param[in, out]   pDiagnostics       Pointer to a THERM_POLICYS_DIAGNOSTICS object
 * @param[in]        pActiveChannelMask Pointer to the Active Channel Mask
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
static FLCN_STATUS
s_thermPolicysChannelsDiagnosticsChIdxsPopulate
(
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics,
    BOARDOBJGRPMASK_E32       *pActiveChannelMask
)
{
    THERM_POLICYS_CHANNEL_DIAGNOSTICS *pChannelDiagnostics;
    LwBoardObjIdx                      i;
    LwBoardObjIdx                      entryIdx = 0;
    FLCN_STATUS                        status   = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (pActiveChannelMask != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysChannelsDiagnosticsChIdxsPopulate_exit);

    //
    // In the following loop, we traverse over the set bits of the Active
    // Channel Mask sent from RM to PMU. The index of each set bit in the mask
    // is a channel index that we should track diagnostic metrics for. Hence,
    // for each index in the mask corresponding to a set bit, we use the index
    // to populate the channel index in the dynamically allocated buffer for
    // Channel Diagnostics.
    //
    // i: Mask iterator variable which provides the index of a set bit in each
    //    iteration.
    // entryIndex: Each entry in the dynamically allocated buffer for Channel
    //             Diagnostics.
    //
    BOARDOBJGRPMASK_FOR_EACH_BEGIN(pActiveChannelMask, i)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysChannelDiagnosticsGet(pDiagnostics,
                &pChannelDiagnostics, entryIdx++),
            s_thermPolicysChannelsDiagnosticsChIdxsPopulate_exit);

        // Sanity check.
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pChannelDiagnostics != NULL),
            FLCN_ERR_ILWALID_STATE,
            s_thermPolicysChannelsDiagnosticsChIdxsPopulate_exit);

        //
        // Populate the channel index for the current Channel Diagnostics
        // Entry.
        //
        pChannelDiagnostics->chIdx = i;
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

    //
    // Sanity check - if we have processed all the set bits in the mask but
    // the number of Channel Diagnostics entries populated is less than
    // pDiagnostics->numChannels, this means the contents of the mask have
    // somehow changed in the meantime which is not expected behaviour.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (entryIdx == pDiagnostics->numChannels),
        FLCN_ERR_ILWALID_STATE,
        s_thermPolicysChannelsDiagnosticsChIdxsPopulate_exit);

s_thermPolicysChannelsDiagnosticsChIdxsPopulate_exit:
    return status;
}

/*!
 * @brief   Evaluate Thermal Policy Diagnostic metrics at a policyGrp level.
 *
 * @param[in]   pDiagnostics  Pointer to a THERM_POLICYS_DIAGNOSTICS object
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
static FLCN_STATUS
s_thermPolicysGlobalDiagnosticsEvaluate
(
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics
)
{
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN  *pGlobalLimitCountdown;
    FLCN_STATUS                                        status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysGlobalDiagnosticsEvaluate_exit);

    // Evaluate limitCountdown metrics (Bug 3276847) at a policyGrp level.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsLimitCountdownGet(&pDiagnostics->global,
            &pGlobalLimitCountdown),
        s_thermPolicysGlobalDiagnosticsEvaluate_exit);
    if ((pGlobalLimitCountdown != NULL) && 
         pGlobalLimitCountdown->info.bEnable)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysGlobalDiagnosticsLimitCountdownEvaluate(
                pGlobalLimitCountdown),
            s_thermPolicysGlobalDiagnosticsEvaluate_exit);
    }

s_thermPolicysGlobalDiagnosticsEvaluate_exit:
    return status;
}

/*!
 * @brief   Evaluate Thermal Policy Diagnostic metrics at a per-channel level.
 *
 * @param[in]   pDiagnostics  Pointer to a THERM_POLICYS_DIAGNOSTICS object
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
static FLCN_STATUS
s_thermPolicysChannelsDiagnosticsEvaluate
(
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics
)
{
    THERM_POLICYS_CHANNEL_DIAGNOSTICS                 *pChannelDiagnostics;
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN *pChannelLimitCountdown;
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN  *pGlobalLimitCountdown;
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED          *pChannelCapped;
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED           *pGlobalCapped;
    LwBoardObjIdx                                      i;
    FLCN_STATUS                                        status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysChannelsDiagnosticsEvaluate_exit);

    // Obtain the Global Diagnostics pointer for limitCountdown metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsLimitCountdownGet(
            &pDiagnostics->global, &(pGlobalLimitCountdown)),
        s_thermPolicysChannelsDiagnosticsEvaluate_exit);

    // Obtain the Global Diagnostics pointer for limiting temperature metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsCappedGet(
            &pDiagnostics->global, &(pGlobalCapped)),
        s_thermPolicysChannelsDiagnosticsEvaluate_exit);

    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_BEGIN(pDiagnostics,
        &pChannelDiagnostics, i, status,
        s_thermPolicysChannelsDiagnosticsEvaluate_exit); 
    {
        //
        // Evaluate limitCountdown metrics (Bug 3276847) at a per-channel
        // level.
        //
        if ((pGlobalLimitCountdown != NULL) &&
             pGlobalLimitCountdown->info.bEnable)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsLimitCountdownGet(
                    pChannelDiagnostics, &pChannelLimitCountdown),
                s_thermPolicysChannelsDiagnosticsEvaluate_exit);

            // Sanity check.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pChannelLimitCountdown != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_thermPolicysChannelsDiagnosticsEvaluate_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsLimitCountdownEvaluate(
                    pChannelDiagnostics, pChannelLimitCountdown),
                s_thermPolicysChannelsDiagnosticsEvaluate_exit);
        }

        //
        // Evaluate limiting temperature metrics (Bug 3287873) at a per-channel
        // level.
        //
        if ((pGlobalCapped != NULL) &&
             pGlobalCapped->info.bEnable)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsCappedGet(
                    pChannelDiagnostics, &pChannelCapped),
                s_thermPolicysChannelsDiagnosticsEvaluate_exit);

            // Sanity check.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pChannelCapped != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_thermPolicysChannelsDiagnosticsEvaluate_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsCappedEvaluate(
                    pChannelDiagnostics, pChannelCapped),
                s_thermPolicysChannelsDiagnosticsEvaluate_exit);
        }
    }
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_END;

s_thermPolicysChannelsDiagnosticsEvaluate_exit:
    return status;
}

/*!
 * @brief   Re-initialize dynamic functionality of Thermal Policy Diagnostic
 *          metrics at a policyGrp level.
 *
 * @param[in]   pDiagnostics  Pointer to a THERM_POLICYS_DIAGNOSTICS object
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
static FLCN_STATUS
s_thermPolicysGlobalDiagnosticsReinit
(
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics
)
{
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN  *pGlobalLimitCountdown;
    FLCN_STATUS                                        status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysGlobalDiagnosticsReinit_exit);

    //
    // Re-initialize dynamic functionality of limitCountdown metrics
    // (Bug 3276847) at a policyGrp level.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsLimitCountdownGet(&pDiagnostics->global,
            &pGlobalLimitCountdown),
        s_thermPolicysGlobalDiagnosticsReinit_exit);
    if ((pGlobalLimitCountdown != NULL) && 
         pGlobalLimitCountdown->info.bEnable)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysGlobalDiagnosticsLimitCountdownReinit(
                pGlobalLimitCountdown),
            s_thermPolicysGlobalDiagnosticsReinit_exit);
    }

s_thermPolicysGlobalDiagnosticsReinit_exit:
    return status;
}

/*!
 * @brief   Re-initialize dynamic functionality of Thermal Policy Diagnostic
 *          metrics at a per-channel level.
 *
 * @param[in]   pDiagnostics  Pointer to a THERM_POLICYS_DIAGNOSTICS object
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
static FLCN_STATUS
s_thermPolicysChannelsDiagnosticsReinit
(
    THERM_POLICYS_DIAGNOSTICS *pDiagnostics
)
{
    THERM_POLICYS_CHANNEL_DIAGNOSTICS                 *pChannelDiagnostics;
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN *pChannelLimitCountdown;
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED          *pChannelCapped;
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN  *pGlobalLimitCountdown;
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED           *pGlobalCapped;
    LwBoardObjIdx                                      i;
    FLCN_STATUS                                        status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDiagnostics != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysChannelsDiagnosticsReinit_exit);

    // Obtain the Global Diagnostics pointer for limitCountdown metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsLimitCountdownGet(
            &pDiagnostics->global, &(pGlobalLimitCountdown)),
        s_thermPolicysChannelsDiagnosticsReinit_exit);

    // Obtain the Global Diagnostics pointer for limiting temperature metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsCappedGet(&pDiagnostics->global,
            &(pGlobalCapped)),
        s_thermPolicysChannelsDiagnosticsReinit_exit);

    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_BEGIN(pDiagnostics,
        &pChannelDiagnostics, i, status,
        s_thermPolicysChannelsDiagnosticsReinit_exit); 
    {
        //
        // Re-initialize dynamic functionality of limitCountdown metrics
        // (Bug 3276847) at a per-channel level.
        //
        if ((pGlobalLimitCountdown != NULL) &&
             pGlobalLimitCountdown->info.bEnable)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsLimitCountdownGet(
                    pChannelDiagnostics, &pChannelLimitCountdown),
                s_thermPolicysChannelsDiagnosticsReinit_exit);

            // Sanity check.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pChannelLimitCountdown != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_thermPolicysChannelsDiagnosticsReinit_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsLimitCountdownReinit(
                    pChannelLimitCountdown),
                s_thermPolicysChannelsDiagnosticsReinit_exit);
        }

        //
        // Re-initialize dynamic functionality of limiting temperature metrics
        // (Bug 3287873) at a per-channel level.
        //
        if ((pGlobalCapped != NULL) &&
             pGlobalCapped->info.bEnable)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsCappedGet(pChannelDiagnostics,
                    &pChannelCapped),
                s_thermPolicysChannelsDiagnosticsReinit_exit);

            // Sanity check.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pChannelCapped != NULL),
                FLCN_ERR_ILWALID_ARGUMENT,
                s_thermPolicysChannelsDiagnosticsReinit_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsCappedReinit(
                    pChannelCapped),
                s_thermPolicysChannelsDiagnosticsReinit_exit);
        }
    }
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_END;

s_thermPolicysChannelsDiagnosticsReinit_exit:
    return status;
}

/*!
 * @brief   Get Status of Thermal Policy Diagnostics metrics at a policyGrp
 *          level
 *
 * @param[in]        pDiagnostics  Pointer to a THERM_POLICYS_DIAGNOSTICS object
 * @param[in, out]   pStatus       Pointer to a LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS object
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
static FLCN_STATUS
s_thermPolicysGlobalDiagnosticsGetStatus
(
    THERM_POLICYS_DIAGNOSTICS                      *pDiagnostics,
    LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS *pStatus
)
{
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN  *pGlobalLimitCountdown;
    FLCN_STATUS                                        status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (pStatus != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysGlobalDiagnosticsGetStatus_exit);

    // Get Status of limitCountdown metrics (Bug 3276847) at a policyGrp level.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsLimitCountdownGet(&pDiagnostics->global,
            &pGlobalLimitCountdown),
        s_thermPolicysGlobalDiagnosticsGetStatus_exit);
    if (pGlobalLimitCountdown != NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            thermPolicysGlobalDiagnosticsLimitCountdownGetStatus(
                pGlobalLimitCountdown, &pStatus->global.limitCountdown),
            s_thermPolicysGlobalDiagnosticsGetStatus_exit);
    }

s_thermPolicysGlobalDiagnosticsGetStatus_exit:
    return status;
}

/*!
 * @brief   Get Status of Thermal Policy Diagnostics metrics at a per-channel
 *          level
 *
 * @param[in]        pDiagnostics  Pointer to a THERM_POLICYS_DIAGNOSTICS object
 * @param[in, out]   pStatus       Pointer to a LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS object
 *
 * @returns FLCN_OK if the transaction is successful,
 *          error otherwise.
 */
static FLCN_STATUS
s_thermPolicysChannelsDiagnosticsGetStatus
(
    THERM_POLICYS_DIAGNOSTICS                      *pDiagnostics,
    LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS *pStatus
)
{
    THERM_POLICYS_CHANNEL_DIAGNOSTICS                 *pChannelDiagnostics;
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN *pChannelLimitCountdown;
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED          *pChannelCapped;
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN  *pGlobalLimitCountdown;
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED           *pGlobalCapped;
    LwBoardObjIdx                                      i;
    FLCN_STATUS                                        status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (pStatus != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysChannelsDiagnosticsGetStatus_exit);

    //
    // Get the number of unique thermal channel entries being used by thermal
    // policy entries.
    //
    pStatus->numChannels = pDiagnostics->numChannels;

    // Obtain the Global Diagnostics pointer for limitCountdown metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsLimitCountdownGet(
            &pDiagnostics->global, &(pGlobalLimitCountdown)),
        s_thermPolicysChannelsDiagnosticsGetStatus_exit);

    // Obtain the Global Diagnostics pointer for limiting temperature metrics.
    PMU_ASSERT_OK_OR_GOTO(status,
        thermPolicysGlobalDiagnosticsCappedGet(&pDiagnostics->global,
            &(pGlobalCapped)),
        s_thermPolicysChannelsDiagnosticsGetStatus_exit);

    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_BEGIN(pDiagnostics,
        &pChannelDiagnostics, i, status,
       s_thermPolicysChannelsDiagnosticsGetStatus_exit); 
    {
        //
        // Sanity check - The channel index for the channel diagnostics entry
        // should be within range.
        //
        // Note - At this point, the channel index is expected to be within
        // range but adding this check since is we use the channel index to
        // index into a statically allocated array.
        //
        PMU_ASSERT_TRUE_OR_GOTO(status,
            (pChannelDiagnostics->chIdx <
                LW2080_CTRL_THERMAL_POLICY_MAX_CHANNELS),
            FLCN_ERR_ILWALID_STATE,
            s_thermPolicysChannelsDiagnosticsGetStatus_exit);

        //
        // Get Status of limitCountdown metrics (Bug 3276847) at a per-channel
        // level.
        //
        if ((pGlobalLimitCountdown != NULL) &&
             pGlobalLimitCountdown->info.bEnable)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsLimitCountdownGet(
                    pChannelDiagnostics, &pChannelLimitCountdown),
                s_thermPolicysChannelsDiagnosticsGetStatus_exit);

            // Sanity check.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pChannelLimitCountdown != NULL),
                FLCN_ERR_ILWALID_STATE,
                s_thermPolicysChannelsDiagnosticsGetStatus_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsLimitCountdownGetStatus(
                    pChannelLimitCountdown,
                    &pStatus->channels[pChannelDiagnostics->chIdx].limitCountdown),
                s_thermPolicysChannelsDiagnosticsGetStatus_exit);
        }

        //
        // Get Status of limiting temperature metrics (Bug 3287873) at a
        // per-channel level.
        //
        if ((pGlobalCapped != NULL) &&
             pGlobalCapped->info.bEnable)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsCappedGet(pChannelDiagnostics,
                    &pChannelCapped),
                s_thermPolicysChannelsDiagnosticsGetStatus_exit);

            // Sanity check.
            PMU_ASSERT_TRUE_OR_GOTO(status,
                (pChannelCapped != NULL),
                FLCN_ERR_ILWALID_ARGUMENT,
                s_thermPolicysChannelsDiagnosticsGetStatus_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                thermPolicysChannelDiagnosticsCappedGetStatus(
                    pChannelCapped,
                    &pStatus->channels[pChannelDiagnostics->chIdx].capped),
                s_thermPolicysChannelsDiagnosticsGetStatus_exit);
        }
    }
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_END;

s_thermPolicysChannelsDiagnosticsGetStatus_exit:
    return status;
}
