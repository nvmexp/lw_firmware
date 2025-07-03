/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_H
#define THRMPOLICY_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmgr/pff.h"
#include "thrmpolicy_fwd_dec.h"
#include "thrmpolicy_diagnostics.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Types Definitions ----------------------------- */
//
// The forward declaration for THERM_POLICY has been moved to
// thrmpolicy_helper.h since the forward declaration is also required by
// thrmpolicy_diagnostics.h as well as feature-specific header files. Since
// thrmpolicy.h includes thrmpolicy_diagnostics.h and thrmpolicy_diagnostics.h
// includes the feature-specific header files, keeping the forward declaration
// here in thrmpolicy.h leads to a cirlwlar dependency.
//

/*!
 * @interface THERM_POLICY
 *
 * Loads the THERM_POLICY SW state
 *
 * @param[in/out]  pPolicy  THERM_POLICY pointer
 *
 * @return FLCN_OK
 *      THERM_POLICY state was successfully loaded.
 * @return Other errors
 *      Errors propagated up from functions called.
 */
#define ThermPolicyLoad(fname) FLCN_STATUS (fname)(THERM_POLICY *pPolicy)

/*!
 * @interface THERM_POLICY
 *
 * Performs the timer callback for the thermal policy.
 *
 * @param[in]  pPolicy  THERM_POLICY pointer
 *
 * @return FLCN_OK
 *      The timer callback was successful.
 * @return Other errors
 *      Error code specific to the failure of the timer callback.
 */
#define ThermPolicyCallbackExelwte(fname) FLCN_STATUS (fname)(THERM_POLICY *pPolicy)

/*!
 * @interface THERM_POLICY
 *
 * To get the cached PFF frequency floor which was evaluted by the given THERM_POLICY.
 *
 * @param[in]  pPolicy      THERM_POLICY pointer
 * @param[out] pFreqkHz     Output of pff evaluation
 */
#define ThermPolicyGetPffCachedFrequencyFloor(fname) FLCN_STATUS (fname)(THERM_POLICY *pPolicy, LwU32 *pFreqkHz)

/*!
 * @interface THERM_POLICY
 *
 * Interface called when vf lwrve has been ilwalidated. Frequencies of PFF tuples will
 * be adjusted at this time with any OC from the VF lwrve for example.
 *
 * @param[in]  pPolicy      THERM_POLICY pointer
 */
#define ThermPolicyVfIlwalidate(fname) FLCN_STATUS (fname)(THERM_POLICY *pPolicy)

/*!
 * Extends BOARDOBJ providing attributes common to all Thermal Policy behavior.
 */
struct THERM_POLICY
{
    /*!
     * BOARDOBJ super class.
     */
    BOARDOBJ                         super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Current RM limit value to enforce. Must always be within range of
     * [@ref limitMin, @ref limitMax].
     */
    LwTemp                           limitLwrr;

    /*!
     * The period, in microseconds, of the callback. A value of 0 indicates the
     * policy does not poll.
     */
    LwU32                            callbackPeriodus;

    /*!
     * PMU specific parameters.
     */

    /*!
     * Pointer to the corresponding THERM_CHANNEL object in the Thermal
     * Channel Table.
     */
    THERM_CHANNEL                   *pChannel;

    /*!
     * Last value read from the thermal channel. Used to determine behavior
     * of the policy.
     */
    LwTemp                           channelReading;

    /*!
     * The time, in microseconds, the next callback should occur. If the policy
     * does not poll, this value shall be THERM_POLICY_TIME_INFINITY.
     */
    LwU32                            callbackTimestampus;

#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
    /*!
     * Control and status information defining the optional piecewise linear
     * frequency flooring lwrve that this policy may enforce.
     */
    PIECEWISE_FREQUENCY_FLOOR        pff;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_TEMPERATURE_OFFSET))
    /*!
     * Maximum Memory Specification Temperature. This represents the temperature
     * limit as specified by the vendor.
     */
    LwTemp                           maxMemSpecTj;

    /*!
     * Maximum Memory Reported Temperature. This represents the maximum desired
     * temperature to report as the upper limit.
     */
    LwTemp                           maxMemReportTj;
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS))
    /*!
     * Thermal Policy Diagnostics functionality at a per-policy level.
     */
    THERM_POLICY_DIAGNOSTICS         diagnostics;
#endif
};

/*
 * Main structure for all Thermal Policy functionality.
 */
typedef struct
{
    /*!
     * Thermal Policy Table index for Thermal Policy controlling the GPS
     * temperature controller.
     *
     * @note LW2080_CTRL_THERMAL_THERM_CHANNEL_INDEX_ILWALID indicates that
     * this policy is not present/specified on this GPU.
     */
    LwU8          gpsPolicyIdx;

    /*!
     * Thermal Policy Table index for Thermal Policy controlling acoustics.
     * This policy allows the GPU to trade cooling solution acoustics for
     * changes in performance.
     *
     * @note LW2080_CTRL_THERMAL_THERM_CHANNEL_INDEX_ILWALID indicates that
     * this policy is not present/specified on this GPU.
     */
    LwU8          acousticPolicyIdx;

    /*!
     * Thermal Policy Table index for Thermal Policy controlling the memory
     * temperature.
     *
     * @note LW2080_CTRL_THERMAL_THERM_CHANNEL_INDEX_ILWALID indicates that
     * this policy is not present/specified on this GPU.
     */
    LwU8          memPolicyIdx;

    /*!
     * Thermal Policy Table index for Thermal Policy controlling GPU
     * SW slowdown.
     *
     * @note LW2080_CTRL_THERMAL_THERM_CHANNEL_INDEX_ILWALID indicates that
     * this policy is not present/specified on this GPU.
     */
    LwU8          gpuSwSlowdownPolicyIdx;

    /*!
     * The worst case Tmax_gpu to Tavg_gpu delta to be used for HW slowdown/shutdown
     * limit reporting for TSOSCs.
     */
    LwTemp        lwTempWorstCaseGpuMaxToAvgDelta;

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS))
    /*!
     * Thermal Policy Diagnostics functionality at a policyGrp and a per-channel
     * level.
     */
    THERM_POLICYS_DIAGNOSTICS
                  diagnostics;
#endif
} THERM_POLICY_METADATA;

/* ------------------------- Inline Functions ------------------------------ */
/*!
 * @brief   Obtain THERM_POLICYS_DIAGNOSTICS object pointer from
 *          THERM_POLICY_METADATA object pointer.
 *
 * @param[in]       pThermPolicyMetadata  Pointer to @ref THERM_POLICY_METADATA
 * @param[in, out]  ppPolicysDiagnostics  Pointer to Pointer to @ref THERM_POLICYS_DIAGNOSTICS
 *                  Output parameters:
 *                      *ppPolicysDiagnostics
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicysDiagnosticsGet
(
    THERM_POLICY_METADATA      *pThermPolicyMetadata,
    THERM_POLICYS_DIAGNOSTICS **ppPolicysDiagnostics
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pThermPolicyMetadata != NULL) && (ppPolicysDiagnostics != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicysDiagnosticsGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS))
    *ppPolicysDiagnostics = &(pThermPolicyMetadata->diagnostics);
#else
    *ppPolicysDiagnostics = NULL;
#endif

thermPolicysDiagnosticsGet_done:
    return status;
}

/*!
 * @brief   Obtain THERM_POLICY_DIAGNOSTICS object pointer from THERM_POLICY
 *          object pointer.
 *
 * @param[in]       pThermPolicy  Pointer to @ref THERM_POLICY
 * @param[in, out]  ppDiagnostics Pointer to Pointer to @ref THERM_POLICY_DIAGNOSTICS
 *                  Output parameters:
 *                      *ppDiagnostics
 *
 * @return  FLCN_OK if the transaction was successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicyDiagnosticsGet
(
    THERM_POLICY              *pThermPolicy,
    THERM_POLICY_DIAGNOSTICS **ppDiagnostics
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pThermPolicy != NULL) && (ppDiagnostics != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsGet_done);

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS))
    *ppDiagnostics = &(pThermPolicy->diagnostics);
#else
    *ppDiagnostics = NULL;
#endif

thermPolicyDiagnosticsGet_done:
    return status;
}

/* ------------------------ Defines ---------------------------------------- */
/*!
 * Used for thermal policies that do not require timer callbacks.
 */
#define THERM_POLICY_TIME_INFINITY                                   LW_U32_MAX

/* ------------------------- Macros ----------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)
#define BOARDOBJGRP_DATA_LOCATION_THERM_POLICY \
    (&(Therm.policies.super))
#else
#define BOARDOBJGRP_DATA_LOCATION_THERM_POLICY \
    ((BOARDOBJGRP *)NULL)
#endif

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define THERM_POLICY_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(THERM_POLICY, (_objIdx)))

/*!
 * @brief Return the thermal policy limit temperature
 *
 * @param[in]   pThermPolicy    THERM_POLICY pointer
 *
 * @return      thermal policy limit temperature
 */
#define THERM_POLICY_LIMIT_GET(pThermPolicy)    ((pThermPolicy)->limitLwrr)

/*!
 * Helper macro to get a pointer to the piecewise frequency floor data from
 * the given TOTAL_GPU power policy.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define thermPolicyPffGet(pThermPolicy)   (&(pThermPolicy)->pff)
#else
#define thermPolicyPffGet(pThermPolicy)   ((PIECEWISE_FREQUENCY_FLOOR *)(NULL))
#endif

/*!
 * Helper macro to set the piecewise frequency floor data used by the the given
 * TOTAL_GPU power policy.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define thermPolicyPffSet(pThermPolicy, pffControl)   ((pThermPolicy)->pff.control = pffControl)
#else
#define thermPolicyPffSet(pThermPolicy, pffControl)   do {} while (LW_FALSE)
#endif


/*!
 * Helper macro to get the Maximum Memory Spec Tj for the given Therm policy
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_TEMPERATURE_OFFSET))
#define thermPolicyGetMaxMemSpecTj(pThermPolicy)    ((pThermPolicy)->maxMemSpecTj)
#else
#define thermPolicyGetMaxMemSpecTj(pThermPolicy)    0
#endif

/*!
 * Helper macro to get the Maximum Memory Report Tj for the given Therm policy
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_DEVICE_TEMPERATURE_OFFSET))
#define thermPolicyGetMaxMemReportTj(pThermPolicy)    ((pThermPolicy)->maxMemReportTj)
#else
#define thermPolisyGetMaxMemReportTj(pThermPolicy)    0
#endif

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
void        thermPoliciesPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "thermPoliciesPreInit");
FLCN_STATUS thermPoliciesLoad(void)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPoliciesLoad");
void        thermPoliciesUnload(void)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPoliciesUnload");
FLCN_STATUS thermPoliciesVfIlwalidate(void)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPoliciesVfIlwalidate");
LwU32       thermPolicyCallbackGetTime(THERM_POLICY *pPolicy)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyCallbackGetTime");
LwU32       thermPolicyCallbackUpdateTime(THERM_POLICY *pPolicy, LwU32 timeus)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyCallbackUpdateTime");

ThermPolicyLoad                         (thermPolicyLoad_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyLoad_SUPER");
ThermPolicyLoad                         (thermPolicyLoad)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyLoad");
ThermPolicyCallbackExelwte              (thermPolicyCallbackExelwte)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyCallbackExelwte");
ThermPolicyGetPffCachedFrequencyFloor   (thermPolicyGetPffCachedFrequencyFloor_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "thermPolicyGetPffCachedFrequencyFloor_SUPER");
ThermPolicyGetPffCachedFrequencyFloor   (thermPolicyGetPffCachedFrequencyFloor)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "thermPolicyGetPffCachedFrequencyFloor");
ThermPolicyVfIlwalidate                 (thermPolicyVfIlwalidate_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyVfIlwalidate_SUPER");
ThermPolicyVfIlwalidate                 (thermPolicyVfIlwalidate)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyVfIlwalidate");

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10ObjSet           (thermPolicyGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus               (thermPolicyIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyIfaceModel10GetStatus_SUPER");
BoardObjGrpIfaceModel10CmdHandler       (thermPolicyBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler       (thermPolicyBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyBoardObjGrpIfaceModel10GetStatus");
BoardObjGrpIfaceModel10SetHeader  (thermThermPolicyIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermThermPolicyIfaceModel10SetHeader");
BoardObjGrpIfaceModel10SetEntry   (thermThermPolicyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermThermPolicyIfaceModel10SetEntry");
BoardObjGrpIfaceModel10GetStatusHeader  (thermThermPolicyIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermThermPolicyIfaceModel10GetStatusHeader");
BoardObjIfaceModel10GetStatus               (thermThermPolicyIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermThermPolicyIfaceModel10GetStatus");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "therm/thrmpolicy_domgrp.h"
#include "therm/thrmpolicy_dtc_pwr.h"

#endif // THRMPOLICY_H
