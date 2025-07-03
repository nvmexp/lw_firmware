/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DIAGNOSTICS_H
#define THRMPOLICY_DIAGNOSTICS_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "thrmpolicy_fwd_dec.h"
#include "thrmpolicy_diagnostics_limit_countdown.h"
#include "thrmpolicy_diagnostics_capped.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Structure to encapsulate functionality for Thermal Policy Diagnostic
 * features at a per-channel level.
 */
struct THERM_POLICYS_CHANNEL_DIAGNOSTICS
{
    /*!
     * Thermal Channel Index.
     */
    LwBoardObjIdx chIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN))
    /*!
     * Structure to encapsulate functionality for limitCountdown metrics
     * (Bug 3276847) at a per-channel level.
     */
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN limitCountdown;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_CAPPED))
    /*!
     * Structure to encapsulate functionality for limiting temperature metrics
     * (Bug 3287873) at a per-channel level.
     */
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED          capped;
#endif
};

/*!
 * Structure to encapsulate functionality for Thermal Policy Diagnostic
 * features at a policyGrp level.
 */
typedef struct
{
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN))
    /*!
     * Structure to encapsulate functionality for limitCountdown metrics
     * (Bug 3276847) at a policyGrp level.
     */
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN limitCountdown;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_CAPPED))
    /*!
     * Structure to encapsulate functionality for limiting temperature metrics
     * (Bug 3287873) at a policyGrp level.
     */
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED          capped;
#endif
} THERM_POLICYS_GLOBAL_DIAGNOSTICS;

/*!
 * Structure to encapsulate functionality for Thermal Policy Diagnostic
 * features at a policyGrp and a per-channel level.
 */
typedef struct
{
    /*!
     * Number of unique THERM_CHANNEL entries being used by THERM_POLICY
     * entries.
     */
    LwU8                               numChannels;

    /*!
     * Pointer to buffer containing the per-channel diagnostic metrics.
     */
    THERM_POLICYS_CHANNEL_DIAGNOSTICS *pChannels;

    /*!
     * Structure containing dynamic data related to Thermal Policy diagnostic
     * features at a policyGrp level.
     */
    THERM_POLICYS_GLOBAL_DIAGNOSTICS   global;
} THERM_POLICYS_DIAGNOSTICS;

/*!
 * Structure to encapsulate functionality for Thermal Policy Diagnostic
 * features at a per-policy level.
 */
typedef struct
{
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN))
    /*!
     * Structure to encapsulate functionality for limitCountdown metrics (Bug 3276847)
     * at a per-policy level.
     */
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN      limitCountdown;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_CAPPED))
    /*!
     * Structure to encapsulate functionality for limiting temperature metrics
     * (Bug 3287873) at a per-policy level.
     */
    THERM_POLICY_DIAGNOSTICS_CAPPED capped;
#endif
} THERM_POLICY_DIAGNOSTICS;

/* ------------------------- Defines ---------------------------------------- */
/*!
 * @brief  Macro that iterates over THERM_POLICY entries and returns
 *         the corresponding THERM_POLICY_DIAGNOSTICS entries
 *
 * @param  _ppPolicy            Pointer to Pointer to @ref THERM_POLICY
 * @param  _ppPolicyDiagnostics Pointer to Pointer to @ref THERM_POLICY_DIAGNOSTICS
 * @param  _status              Status variable to be set in case of failure
 * @param  _i                   Loop iterator
 * @param  _exitLabel           Label to jump to in case of failure
 */
#define THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_BEGIN(_ppPolicy, _ppPolicyDiagnostics, _i, _status, \
                                                           _exitLabel)                               \
do                                                                                                   \
{                                                                                                    \
    CHECK_SCOPE_BEGIN(THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY);                                     \
                                                                                                     \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                 \
        ((_ppPolicy != NULL) && (_ppPolicyDiagnostics != NULL)),                                     \
        FLCN_ERR_ILWALID_ARGUMENT,                                                                   \
        _exitLabel);                                                                                 \
                                                                                                     \
    BOARDOBJGRP_ITERATOR_BEGIN(THERM_POLICY, *(_ppPolicy), _i, NULL)                                 \
    {                                                                                                \
                                                                                                     \
        PMU_ASSERT_OK_OR_GOTO(_status,                                                               \
            thermPolicyDiagnosticsGet(*(_ppPolicy), _ppPolicyDiagnostics),                           \
            _exitLabel);                                                                             \
                                                                                                     \
        PMU_ASSERT_TRUE_OR_GOTO(_status,                                                             \
            (*(_ppPolicyDiagnostics) != NULL),                                                       \
            FLCN_ERR_ILWALID_ARGUMENT,                                                               \
            _exitLabel);

/*!
 * @brief  Macro that must be called in conjunction with
 *         THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_BEGIN.
 */
#define THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY_END           \
    }                                                          \
    BOARDOBJGRP_ITERATOR_END;                                  \
    CHECK_SCOPE_END(THERM_POLICY_DIAGNOSTICS_FOR_EACH_POLICY); \
} while (LW_FALSE)

/*!
 * @brief  Private macro that iterates over THERM_POLICYS_CHANNEL_DIAGNOSTICS entries,
           starting at a startingIndex
 *
 * @param  _pDiagnostics         Pointer to @ref THERM_POLICYS_DIAGNOSTICS
 * @param  _ppChannelDiagnostics Pointer to Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS
 * @param  _entryIdx             Loop iterator
 * @param  _status               Status variable to be set in case of failure
 * @param  _exitLabel            Label to jump to in case of failure
 * @param  _startingIndex        Index to start from
 */
#define THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY__PRIVATE_BEGIN(_pDiagnostics, _ppChannelDiagnostics, _entryIdx, \
                                                                             _status, _exitLabel, _startingIndex)        \
do                                                                                                                       \
{                                                                                                                        \
    CHECK_SCOPE_BEGIN(THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY__PRIVATE);                                        \
                                                                                                                         \
    PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                     \
        ((_pDiagnostics != NULL) && (_ppChannelDiagnostics != NULL)),                                                    \
        FLCN_ERR_ILWALID_ARGUMENT,                                                                                       \
        _exitLabel);                                                                                                     \
                                                                                                                         \
    for ((_entryIdx) = (_startingIndex); (_entryIdx) < _pDiagnostics->numChannels; (_entryIdx)++)                        \
    {                                                                                                                    \
        PMU_ASSERT_OK_OR_GOTO(_status,                                                                                   \
            thermPolicysChannelDiagnosticsGet(_pDiagnostics, _ppChannelDiagnostics, _entryIdx),                          \
            _exitLabel);                                                                                                 \
                                                                                                                         \
        PMU_ASSERT_TRUE_OR_GOTO(_status,                                                                                 \
        (*(_ppChannelDiagnostics) != NULL),                                                                              \
        FLCN_ERR_ILWALID_STATE,                                                                                          \
        _exitLabel); 

/*!
 * @brief  Macro that must be called in conjunction with
 *         THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY__PRIVATE_BEGIN.
 */
#define THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY__PRIVATE_END           \
    }                                                                           \
    CHECK_SCOPE_END(THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY__PRIVATE); \
} while (LW_FALSE)

/*!
 * @brief  Macro that iterates over THERM_POLICYS_CHANNEL_DIAGNOSTICS entries
 *
 * @note   Must be called in conjunction with
 *         THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_END
 *
 * @param  _pDiagnostics         Pointer to @ref THERM_POLICYS_DIAGNOSTICS
 * @param  _ppChannelDiagnostics Pointer to Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS
 * @param  _entryIdx             Loop iterator
 * @param  _status               Status variable to be set in case of failure
 * @param  _exitLabel            Label to jump to in case of failure
 */
#define THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_BEGIN(_pDiagnostics, _ppChannelDiagnostics, _entryIdx, \
                                                                   _status, _exitLabel)                         \
do                                                                                                              \
{                                                                                                               \
    CHECK_SCOPE_BEGIN(THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY);                                        \
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY__PRIVATE_BEGIN(_pDiagnostics, _ppChannelDiagnostics,       \
                                                                        _entryIdx, _status, _exitLabel, 0U)     \
    {

/*!
 * @brief  Macro that must be called in conjunction with
 *         THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_BEGIN.
 */
#define THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY_END           \
    }                                                                  \
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY__PRIVATE_END;     \
    CHECK_SCOPE_END(THERM_POLICYS_CHANNEL_DIAGNOSTICS_FOR_EACH_ENTRY); \
} while (LW_FALSE)

/* ------------------------- Inline Functions ------------------------------ */
/*!
 * @brief   Extract the Active Channel Mask sent from RM to PMU.
 *
 * @param[in, out]  pActiveChannelMask Pointer to the field that will hold the Active Channel Mask
 * @param[in]       pInfo              Pointer to LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_INFO
 *                  Output parameters:
 *                      *pActiveChannelMask
 *
 * @returns FLCN_OK if the transaction is successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_thermPolicysDiagnosticsExtractActiveChannelMask
(
    BOARDOBJGRPMASK_E32                          *pActiveChannelMask,
    LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_INFO *pInfo
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pActiveChannelMask != NULL) && (pInfo != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        s_thermPolicysDiagnosticsExtractActiveChannelMask_exit);

    //
    // Copy in the Active Channel Mask that contains information about the
    // number of unique thermal channels being used by thermal policies.
    //
    boardObjGrpMaskInit_E32(pActiveChannelMask);
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E32(pActiveChannelMask,
            &pInfo->activeChannelMask),
        s_thermPolicysDiagnosticsExtractActiveChannelMask_exit);

s_thermPolicysDiagnosticsExtractActiveChannelMask_exit:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICYS_CHANNEL_DIAGNOSTICS object pointer at the offset
 *          specified by @ref entryIndex, from THERM_POLICYS_DIAGNOSTICS object pointer.
 *
 * @param[in]       pPolicysDiagnostics  Pointer to @ref THERM_POLICYS_DIAGNOSTICS
 * @param[in, out]  ppChannelDiagnostics Pointer to Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS
 * @param[in]       entryIndex           Index into the dynamically allocated buffer
 *                  Output parameters:
 *                      *ppChannelDiagnostics
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysChannelDiagnosticsGet
(
    THERM_POLICYS_DIAGNOSTICS          *pPolicysDiagnostics,
    THERM_POLICYS_CHANNEL_DIAGNOSTICS **ppChannelDiagnostics,
    LwBoardObjIdx                       entryIdx
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPolicysDiagnostics != NULL) && (ppChannelDiagnostics != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsGet_done);

    //
    // Sanity check - If we have reached this point, the buffer holding
    // diagnostic metrics for thermal channels cannot be NULL.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPolicysDiagnostics->pChannels != NULL),
        FLCN_ERR_ILWALID_STATE,
        thermPolicysChannelDiagnosticsGet_done);

    // Check if the provided Entry Index provided is valid.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (entryIdx < pPolicysDiagnostics->numChannels),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsGet_done);

    *ppChannelDiagnostics = &(pPolicysDiagnostics->pChannels[entryIdx]);

thermPolicysChannelDiagnosticsGet_done:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN object
 *          pointer from THERM_POLICYS_CHANNEL_DIAGNOSTICS object pointer.
 *
 * @param[in]       pChannelDiagnostics  Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS
 * @param[in, out]  ppLimitCountdown     Pointer to Pointer to@ref THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN
 *                  Output parameters:
 *                      *ppLimitCountdown
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysChannelDiagnosticsLimitCountdownGet
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS                  *pChannelDiagnostics,
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_LIMIT_COUNTDOWN **ppLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pChannelDiagnostics != NULL) && (ppLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsLimitCountdownGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN))
    *ppLimitCountdown = &(pChannelDiagnostics->limitCountdown);
#else
    *ppLimitCountdown = NULL;
#endif

thermPolicysChannelDiagnosticsLimitCountdownGet_done:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED object pointer from
 *          THERM_POLICYS_CHANNEL_DIAGNOSTICS object pointer.
 *
 * @param[in]       pChannelDiagnostics  Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS
 * @param[in, out]  ppCapped             Pointer to Pointer to @ref THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED
 *                  Output parameters:
 *                      *ppCapped
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysChannelDiagnosticsCappedGet
(
    THERM_POLICYS_CHANNEL_DIAGNOSTICS         *pChannelDiagnostics,
    THERM_POLICYS_CHANNEL_DIAGNOSTICS_CAPPED **ppCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pChannelDiagnostics != NULL) && (ppCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysChannelDiagnosticsCappedGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_CAPPED))
    *ppCapped = &(pChannelDiagnostics->capped);
#else
    *ppCapped = NULL;
#endif

thermPolicysChannelDiagnosticsCappedGet_done:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN object
 *          pointer from THERM_POLICYS_GLOBAL_DIAGNOSTICS object pointer.
 *
 * @param[in]       pGlobalDiagnostics  Pointer to @ref THERM_POLICYS_GLOBAL_DIAGNOSTICS
 * @param[in, out]  ppLimitCountdown    Pointer to Pointer to
                                        @ref THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN
 *                  Output parameters:
 *                      *ppLimitCountdown
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysGlobalDiagnosticsLimitCountdownGet
(
    THERM_POLICYS_GLOBAL_DIAGNOSTICS                  *pGlobalDiagnostics,
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_LIMIT_COUNTDOWN **ppLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pGlobalDiagnostics != NULL) && (ppLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysGlobalDiagnosticsLimitCountdownGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN))
    *ppLimitCountdown = &(pGlobalDiagnostics->limitCountdown);
#else
    *ppLimitCountdown = NULL;
#endif

thermPolicysGlobalDiagnosticsLimitCountdownGet_done:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED object pointer from
 *          THERM_POLICYS_GLOBAL_DIAGNOSTICS object pointer.
 *
 * @param[in]       pGlobalDiagnostics  Pointer to @ref THERM_POLICYS_GLOBAL_DIAGNOSTICS
 * @param[in, out]  ppCapped            Pointer to Pointer to @ref THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED
 *                  Output parameters:
 *                      **ppCapped
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysGlobalDiagnosticsCappedGet
(
    THERM_POLICYS_GLOBAL_DIAGNOSTICS         *pGlobalDiagnostics,
    THERM_POLICYS_GLOBAL_DIAGNOSTICS_CAPPED **ppCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pGlobalDiagnostics != NULL) && (ppCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysGlobalDiagnosticsCappedGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_CAPPED))
    *ppCapped = &(pGlobalDiagnostics->capped);
#else
    *ppCapped = NULL;
#endif

thermPolicysGlobalDiagnosticsCappedGet_done:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN object pointer
 *          ofrom THERM_POLICY_DIAGNOSTICS object pointer.
 *
 * @param[in]       pDiagnostics     Pointer to @ref THERM_POLICY_DIAGNOSTICS
 * @param[in, out]  ppLimitCountdown Pointer to Pointer to
 *                                   @ref THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN
 *                  Output parameters:
 *                      *ppLimitCountdown
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicyDiagnosticsLimitCountdownGet
(
    THERM_POLICY_DIAGNOSTICS                  *pDiagnostics,
    THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN **ppLimitCountdown
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (ppLimitCountdown != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsLimitCountdownGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_LIMIT_COUNTDOWN))
    *ppLimitCountdown = &(pDiagnostics->limitCountdown);
#else
    *ppLimitCountdown = NULL;
#endif

thermPolicyDiagnosticsLimitCountdownGet_done:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICY_DIAGNOSTICS_CAPPED object pointer
 *          from THERM_POLICY_DIAGNOSTICS object pointer.
 *
 * @param[in]       pDiagnostics  Pointer to @ref THERM_POLICY_DIAGNOSTICS
 * @param[in, out]  ppCapped      Pointer to Pointer to 
 *                                @ref THERM_POLICY_DIAGNOSTICS_CAPPED
 *                  Output parameters:
 *                      *ppCapped
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicyDiagnosticsCappedGet
(
    THERM_POLICY_DIAGNOSTICS         *pDiagnostics,
    THERM_POLICY_DIAGNOSTICS_CAPPED **ppCapped
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pDiagnostics != NULL) && (ppCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsCappedGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS_CAPPED))
    *ppCapped = &(pDiagnostics->capped);
#else
    *ppCapped = NULL;
#endif

thermPolicyDiagnosticsCappedGet_done:
    return status;
}

/* ------------------------- Function Prototypes  -------------------------- */
FLCN_STATUS thermPolicysDiagnosticsConstruct(BOARDOBJGRP *pBObjGrp, THERM_POLICYS_DIAGNOSTICS *pDiagnostics,
                                                LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_INFO *pInfo)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyDiagnosticsConstruct");

FLCN_STATUS thermPolicysDiagnosticsReinit(THERM_POLICYS_DIAGNOSTICS *pDiagnostics, LwBool bReinitPolicy)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysDiagnosticsReinit");

FLCN_STATUS thermPolicysDiagnosticsEvaluate(THERM_POLICYS_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysDiagnosticsEvaluate");

FLCN_STATUS thermPolicysDiagnosticsGetStatus(THERM_POLICYS_DIAGNOSTICS *pDiagnostics,
                                                LW2080_CTRL_THERMAL_POLICYS_DIAGNOSTICS_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicysDiagnosticsGetStatus");

FLCN_STATUS thermPolicyDiagnosticsConstruct(THERM_POLICY_DIAGNOSTICS *pDiagnostics,
                                                LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_INFO *pInfo)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyDiagnosticsConstruct");

FLCN_STATUS thermPolicyDiagnosticsReinit(THERM_POLICY_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDiagnosticsReinit");

FLCN_STATUS thermPolicyDiagnosticsEvaluate( THERM_POLICY *pPolicy, THERM_POLICY_DIAGNOSTICS *pDiagnostics)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDiagnosticsEvaluate");

FLCN_STATUS thermPolicyDiagnosticsGetStatus(THERM_POLICY_DIAGNOSTICS *pDiagnostics,
                                                LW2080_CTRL_THERMAL_POLICY_DIAGNOSTICS_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDiagnosticsGetStatus");

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // THRMPOLICY_DIAGNOSTICS_H
