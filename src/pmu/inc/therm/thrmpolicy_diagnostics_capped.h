/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DIAGNOSTICS_CAPPED_H
#define THRMPOLICY_DIAGNOSTICS_CAPPED_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "thrmpolicy_fwd_dec.h"
#include "boardobj/boardobjgrp.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Structure containing dynamic data related to limiting temperature metrics
 * (Bug 3287873) at a per-channel level.
 */
typedef struct THERM_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_CAPPED {

    /*!
     * Thermal policy mask corresponding to a particular thermal channel.
     * Policies using this channel that are lwrrently capping will have the
     * corresponding bit set in the mask. Policies using this channel that are
     * lwrrently not capping will have the corresponding bit unset in the mask.
     */
    BOARDOBJGRPMASK_E32 thermalCappingPolicyMask;
} THERM_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_CAPPED;

/*!
 * Structure to encapsulate functionality for limiting temperature metrics
 * (Bug 3287873) at a per-channel level.
 */
typedef struct
{
    /*!
     * Structure containing static data related to limiting temperature metrics
     * (Bug 3287873) at a per-channel level.
     */
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_CAPPED status;
} THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED;

/*!
 * Structure to encapsulate functionality for limiting temperature metrics
 * (Bug 3287873) at a policyGrp level.
 */
typedef struct
{
    /*!
     * Structure containing static data related to limiting temperature metrics
     * (Bug 3287873) at a policyGrp level.
     */
    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_CAPPED info;
} THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED;

/*!
 * Structure to encapsulate functionality to report whether a policy is
 * capping (Bug 3287873).
 */
typedef struct
{
    /*!
     * Structure containing static data related to limiting temperature metrics
     * (Bug 3287873) at a per-policy level.
     */
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_CAPPED   info;

    /*!
     * Structure containing dynamic data related to limiting temperature metrics
     * (Bug 3287873) at a per-policy level.
     */
    LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_CAPPED status;
} THERM_POLICY_DIAGNOSTICS_CAPPED;

/* ------------------------- Inline Functions ------------------------------ */
/*!
 * @brief   Re-initialize dynamic functionality of limiting temperature metrics
 *          at a per-channel level.
 *
 * @param[in, out]  pChannelCapped  Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED
 *
 * @returns FLCN_OK if the transaction is successful, error otherwise.
 *          error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysChannelDiagnosticsCappedReinit
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED *pChannelCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pChannelCapped != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsCappedReinit_done);

    boardObjGrpMaskInit_E32(
        &(pChannelCapped->status.thermalCappingPolicyMask));

thermPolicysChannelDiagnosticsCappedReinit_done:
    return status;
}

/*!
 * @brief   Re-initialize dynamic functionality of limiting temperature metrics
 *          at a per-policy level.
 *
 * @param[in, out]  pCapped  Pointer to THERM_POLICY_DIAGNOSTICS_CAPPED
 *
 * @returns FLCN_OK if the transaction is successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicyDiagnosticsCappedReinit
(
    THERM_POLICY_DIAGNOSTICS_CAPPED *pCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pCapped != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsCappedReinit_exit);

    pCapped->status.bIsCapped = LW_FALSE;

thermPolicyDiagnosticsCappedReinit_exit:
    return status;
}

/* ------------------------- Function Prototypes  -------------------------- */
FLCN_STATUS thermPolicysDiagnosticsCappedConstruct(
                THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED *pGlobalCapped,
                    LW2080_CTRL_THERMAL_POLICYS_GLOBAL_DIAGNOSTICS_INFO_CAPPED *pInfoGlobalCapped)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicysDiagnosticsCappedConstruct");

FLCN_STATUS thermPolicysChannelDiagnosticsCappedEvaluate(
                THERM_POLICYS_CHANNEL_DIAGNOSTICS *pChannelDiagnostics,
                    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED *pChannelCapped)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysChannelDiagnosticsCappedEvaluate");

FLCN_STATUS thermPolicysChannelDiagnosticsCappedGetStatus(
                THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED *pChannelCapped,
                    LW2080_CTRL_THERMAL_POLICYS_CHANNEL_DIAGNOSTICS_STATUS_CAPPED *pStatusChannelCapped)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysChannelDiagnosticsCappedGetStatus");

FLCN_STATUS thermPolicyDiagnosticsCappedConstruct(THERM_POLICY_DIAGNOSTICS_CAPPED *pCapped,
                LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO_CAPPED *pInfoCapped)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyDiagnosticsCappedConstruct");

FLCN_STATUS thermPolicyDiagnosticsCappedEvaluate(THERM_POLICY *pPolicy,
                THERM_POLICY_DIAGNOSTICS_CAPPED *pCapped)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDiagnosticsCappedEvaluate");

FLCN_STATUS thermPolicyDiagnosticsCappedGetStatus(THERM_POLICY_DIAGNOSTICS_CAPPED *pCapped,
                LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS_CAPPED *pStatusCapped)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDiagnosticsCappedGetStatus");

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // THRMPOLICY_DIAGNOSTICS_CAPPED_H
