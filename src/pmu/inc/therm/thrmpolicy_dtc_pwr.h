/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DTC_PWR_H
#define THRMPOLICY_DTC_PWR_H

/* ------------------------ Includes --------------------------------------- */
#include "therm/thrmpolicy.h"
#include "therm/thrmpolicy_dtc.h"

/*!
 * Extends THERM_POLICY to implement a marching controller using power
 * as the limiting type.
 */
typedef struct
{
    /*!
     * THERM_POLICY super class.
     */
    THERM_POLICY        super;

    /*!
     * THERM_POLICY_DTC interface.
     */
    THERM_POLICY_DTC    dtc;

    /*!
     * The size of the power steps (in mW) the controller will take.
     */
    LwU32               powerStepmW;

    /*!
     * Current limit level imposed by the controller. A lower number indicates
     * cooler limits (slower performance) while a higher number indicates
     * hotter limits (faster performance).
     */
    LwU32               limitLwrrmW;

    /*!
     * The semantic power policy index the controller will use to do the
     * actual limits.
     */
    LwU8                powerPolicyIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS))
    /*!
     * Boolean to denote whether this DTC_PWR Thermal Policy is lwrrently engaged.
     */
    LwBool              bIsEngaged;
#endif
} THERM_POLICY_DTC_PWR;

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Macros ----------------------------------------- */
/*!
 * Helper macro to get the boolean denoting whether the DTC_PWR Thermal Policy
 * is lwrrently engaged.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS))
#define thermPolicyDtcPwrIsEngagedGet(_pDtcPwr) \
    (&(_pDtcPwr)->bIsEngaged)
#else
#define thermPolicyDtcPwrIsEngagedGet(_pDtcPwr) \
    ((LwBool *)(NULL))
#endif

/*!
 * Helper macro to set the boolean denoting whether the DTC_PWR Thermal Policy
 * is lwrrently engaged.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY_DIAGNOSTICS))
#define thermPolicyDtcPwrIsEngagedSet(_pDtcPwr, _bIsEngaged) \
    ((_pDtcPwr)->bIsEngaged = _bIsEngaged)
#else
#define thermPolicyDtcPwrIsEngagedSet(_pDtcPwr, _bIsEngaged) \
    do {} while (LW_FALSE)
#endif

/* ------------------------- Inline Functions ------------------------------ */
/*!
 * @brief   To evaluate limiting temperature metrics at a per-policy level for
 *          thermal policies of Type DTC_PWR.
 *
 * @param[in]       pPolicy  Pointer to THERM_POLICY
 * @param[in, out]  pCapped  Pointer to THERM_POLICY_DIAGNOSTICS_CAPPED
 *
 * @returns FLCN_OK if the transaction is successful, error otherwise.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
thermPolicyDiagnosticsCappedEvaluate_DTC_PWR
(
    THERM_POLICY                    *pPolicy,
    THERM_POLICY_DIAGNOSTICS_CAPPED *pCapped
)
{
    LwBool      *pBIsEngaged;
    FLCN_STATUS  status = FLCN_OK;

    // Sanity check.
    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pPolicy != NULL) && (pCapped != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        thermPolicyDiagnosticsCappedEvaluate_DTC_PWR_exit);

    //
    // Obtain information whether the DTC_PWR thermal policy is engaged as per
    // the latest evaluation cycle.
    //
    pBIsEngaged = thermPolicyDtcPwrIsEngagedGet((THERM_POLICY_DTC_PWR *)pPolicy);

    //
    // Sanity check: If we have reached this point, the above pointer cannot be
    // NULL.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pBIsEngaged != NULL),
        FLCN_ERR_ILWALID_STATE,
        thermPolicyDiagnosticsCappedEvaluate_DTC_PWR_exit);

    //
    // Update diagnostic information with regards to whether the policy is
    // capped.
    //
    pCapped->status.bIsCapped = *pBIsEngaged;

thermPolicyDiagnosticsCappedEvaluate_DTC_PWR_exit:
    return status;
}

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * THERM_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (thermPolicyGrpIfaceModel10ObjSetImpl_DTC_PWR)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyGrpIfaceModel10ObjSetImpl_DTC_PWR");
BoardObjIfaceModel10GetStatus               (thermPolicyIfaceModel10GetStatus_DTC_PWR)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyIfaceModel10GetStatus_DTC_PWR");
ThermPolicyCallbackExelwte  (thermPolicyCallbackExelwte_DTC_PWR)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyCallbackExelwte_DTC_PWR");

#endif // THRMPOLICY_DTC_PWR_H
