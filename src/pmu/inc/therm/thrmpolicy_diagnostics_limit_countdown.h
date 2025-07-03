/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DIAGNOSTICS_LIMIT_COUNTDOWN_H
#define THRMPOLICY_DIAGNOSTICS_LIMIT_COUNTDOWN_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "thrmpolicy_fwd_dec.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Structure to encapsulate functionality for limitCountdown metrics
 * (Bug 3276847) at a per-channel level.
 */
typedef struct
{
    /*!
     * Structure containing dynamic data related to limitCountdown metrics
     * (Bug 3276847) at a per-channel level.
     */
    LW2080_CTRL_THERMAL_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN
        status;
} THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN;

/*!
 * Structure to encapsulate functionality for limitCountdown metrics
 * (Bug 3276847) at a policyGrp level.
 */
typedef struct
{
    /*!
     * Structure containing static data related to limitCountdown metrics
     * (Bug 3276847) at a policyGrp level.
     */
    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN
        info;

    /*!
     * Structure containing dynamic data related to limitCountdown metrics
     * (Bug 3276847) at a policyGrp level.
     */
    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN
        status;
} THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN;

/*!
 * Structure to encapsulate functionality for limitCountdown metrics (Bug 3276847)
 * at a per-policy level.
 */
typedef struct
{
    /*!
     * Structure containing static data related to limitCountdown metrics
     * (Bug 3276847) at a per-policy level.
     */
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN   info;

    /*!
     * Structure containing dynamic data related to limitCountdown metrics
     * (Bug 3276847) at a per-policy level.
     */
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN status;
} THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN;

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Inline Functions ------------------------------ */
/*!
 * @brief   Re-initialize dynamic functionality of limitCountdown metrics
 *          at a policyGrp level.
 *
 * @param[in, out]  pGlobalLimitCountdown  Pointer to @ref THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN
 *
 * @returns FLCN_OK if the transaction is successful, error otherwise.
 *          error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysGlobalDiagnosticsLimitCountdownReinit
(
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN *pGlobalLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pGlobalLimitCountdown != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysGlobalDiagnosticsLimitCountdownReinit_done);

    pGlobalLimitCountdown->status.limitCountdown = RM_PMU_LW_TEMP_ILWALID;

thermPolicysGlobalDiagnosticsLimitCountdownReinit_done:
    return status;
}

/*!
 * @brief   Re-initialize dynamic functionality of limitCountdown metrics
 *          at a per-channel level.
 *
 * @param[in, out]  pChannelLimitCountdown  Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN
 *
 * @returns FLCN_OK if the transaction is successful, error otherwise.
 *          error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysChannelDiagnosticsLimitCountdownReinit
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN *pChannelLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pChannelLimitCountdown != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsLimitCountdownReinit_done);

    pChannelLimitCountdown->status.limitCountdown = RM_PMU_LW_TEMP_ILWALID;

thermPolicysChannelDiagnosticsLimitCountdownReinit_done:
    return status;
}

/*!
 * @brief   Re-initialize dynamic functionality of limitCountdown metrics
 *          at a per-policy level.
 *
 * @param[in, out]  pLimitCountdown  Pointer to THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN
 *
 * @returns FLCN_OK if the transaction is successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicyDiagnosticsLimitCountdownReinit
(
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pLimitCountdown != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsLimitCountdownReinit_exit);

    pLimitCountdown->status.limitCountdown = RM_PMU_LW_TEMP_ILWALID;

thermPolicyDiagnosticsLimitCountdownReinit_exit:
    return status;
}

/* ------------------------- Function Prototypes  -------------------------- */
FLCN_STATUS thermPolicysDiagnosticsLimitCountdownConstruct(
                THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN *pGlobalLimitCountdown,
                    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN *pInfoGlobalLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicysDiagnosticsLimitCountdownConstruct");

FLCN_STATUS thermPolicysGlobalDiagnosticsLimitCountdownEvaluate(
                THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN *pGlobalLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysGlobalDiagnosticsLimitCountdownEvaluate");

FLCN_STATUS thermPolicysChannelDiagnosticsLimitCountdownEvaluate(
                THERM_POLICYS_CHANNEL_DIAGNOSTICS *pChannelDiagnostics,
                    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN *pChannelLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysChannelDiagnosticsLimitCountdownEvaluate");

FLCN_STATUS thermPolicysGlobalDiagnosticsLimitCountdownGetStatus(
                THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN *pGlobalLimitCountdown,
                    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN *pStatusGlobalLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysGlobalDiagnosticsLimitCountdownGetStatus");

FLCN_STATUS thermPolicysChannelDiagnosticsLimitCountdownGetStatus(
                THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN *pChannelLimitCountdown,
                    LW2080_CTRL_THERMAL_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN *pStatusChannelLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysChannelDiagnosticsLimitCountdownGetStatus");

FLCN_STATUS thermPolicyDiagnosticsLimitCountdownConstruct(THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown,
                LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_LIMIT_COUNTDOWN *pInfoLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyDiagnosticsLimitCountdownConstruct");

FLCN_STATUS thermPolicyDiagnosticsLimitCountdownEvaluate(THERM_POLICY *pPolicy,
                THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDiagnosticsLimitCountdownEvaluate");

FLCN_STATUS thermPolicyDiagnosticsLimitCountdownGetStatus(THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN *pLimitCountdown,
                LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_LIMIT_COUNTDOWN *pStatusLimitCountdown)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDiagnosticsLimitCountdownGetStatus");

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // THRMPOLICY_DIAGNOSTICS_LIMIT_COUNTDOWN_H
