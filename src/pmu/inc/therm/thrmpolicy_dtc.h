/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DTC_H
#define THRMPOLICY_DTC_H

/* ------------------------ Includes --------------------------------------- */

/* ------------------------ Defines -----------------------------------------*/
typedef struct THERM_POLICY_DTC THERM_POLICY_DTC, THERM_POLICY_DTC_BASE;

/*!
 * This structure contains the different thresholds used by the controller.
 */
typedef struct
{
    /*!
     * Temperature value such that if the value exceeds this, the controller
     * shall impose its most restrictive perf. limit, lowering GPU performance
     * dramatically.
     */
    LwTemp critical;

    /*!
     * Temperature value such that if the value exceeds this, the controller
     * shall begin to aggressively impose perf. limits, decreasing performance
     * a high rate.
     */
    LwTemp aggressive;

    /*!
     * Temperature value such that if the value exceeds this, the controller
     * shall begin to slowly impose perf. limits, gradually decreasing
     * performance.
     */
    LwTemp moderate;

    /*!
     * Temperature value such that if the value is below this, the controller
     * shall begin to remove perf. limits, gradually increasing performance.
     */
    LwTemp release;

    /*!
     * Temperature value such that if the value is below this, the controller
     * shall remove all perf. limits.
     */
    LwTemp disengage;
} THERM_POLICY_DTC_THRESHOLDS;

/*!
 * Structure representing a Distance-To-Critical (DTC) controller interface
 * called by another THERM_POLICY object.
 */
struct THERM_POLICY_DTC
{
    /*!
     * BOARDOBJ super class.
     */
    BOARDOBJ super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * Specifies the number of levels to change when in the aggressive
     * temperature range.
     */
    LwU8     aggressiveStep;

    /*!
     * Specifies the number of levels to change when in the release temperature
     * range.
     */
    LwU8     releaseStep;

    /*!
     * Keeps track of the number of samples taken for the current threshold
     * range. If the number of samples exceeds the sample threshold, the
     * controller will alter its behavior.
     */
    LwU8     sampleCount;

    /*!
     * The number of samples in the hold region needed before increasing the
     * clocks. If the value is INVALID, then no adjustments will be made.
     */
    LwU8     holdSampleThreshold;

    /*!
     * The number of conselwtive samples in a "step" region (aggressive,
     * moderate, release) that must be sampled before holding the clocks.
     * If the value is INVALID, then no adjustments will be made.
     */
    LwU8    stepSampleThreshold;

    /*!
     * Specifies the temperature thresholds for the different limits of the
     * DTC algorithm.
     */
    THERM_POLICY_DTC_THRESHOLDS thresholds;
};

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Limit change value if the current temperature for the DTC thermal policy is
 * below the Disengage Threshold.
 */
#define THERM_POLICY_DTC_LIMIT_CHANGE_IF_DISENGAGED                (LW_S32_MAX)

/* ------------------------ Function Prototypes ---------------------------- */
LwS32      thermPolicyDtcGetLevelChange(THERM_POLICY_DTC *pDtc, LwTemp temperature)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDtcGetLevelChange");
FLCN_STATUS thermPolicyDtcQuery         (THERM_POLICY_DTC *pDtc, RM_PMU_THERM_POLICY_QUERY_DTC *pPayload)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDtcQuery");
/* ------------------------- Public Functions ------------------------------ */
/*!
 * THERM_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet   (thermPolicyGrpIfaceModel10ObjSetImpl_DTC)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyGrpIfaceModel10ObjSetImpl_DTC");

/* ------------------------ Include Derived Types -------------------------- */

#endif // THRMPOLICY_DTC_H
