/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_hwthreshold.h
 * @brief @copydoc pwrpolicy_hwthreshold.c
 */

#ifndef PWRPOLICY_HWTHRESHOLD_H
#define PWRPOLICY_HWTHRESHOLD_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_limit.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_HW_THRESHOLD PWR_POLICY_HW_THRESHOLD;

/*!
 * Structure representing a hw-threshold based power policy.
 */
struct PWR_POLICY_HW_THRESHOLD
{
    /*!
     * @copydoc PWR_POLICY_LIMIT
     */
    PWR_POLICY_LIMIT    super;
    /*!
     * Threshold index number.
     */
    LwU8                thresholdIdx;
    /*!
     * Low threshold index number.
     */
    LwU8                lowThresholdIdx;
    /*!
     * Specifies if low threshold should be used or not.
     */
    LwBool              bUseLowThreshold;
    /*!
     * Value of low threshold relative to threshold limit.
     */
    LwUFXP4_12          lowThresholdValue;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_LWRRENT_COLWERSION))
    /*!
     * Information to do Power to Current colwersion.
     */
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_DATA_HW_THRESHOLD_POWER_TO_LWRR_COLW pcc;
#endif
};

/* ------------------------- Defines --------------------------------------- */
/*
 * Helper Macro to get a pointer to the Power to Current Colwersion data from
 * HW_THRESHOLD power policy
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_LWRRENT_COLWERSION))
#define pwrPolicyHWThresPccGet(pHt) (&(pHt)->pcc)
#else
#define pwrPolicyHWThresPccGet(pHt) \
    ((LW2080_CTRL_PMGR_PWR_POLICY_INFO_DATA_HW_THRESHOLD_POWER_TO_LWRR_COLW *)(NULL))
#endif

/*
 * Helper Macro to set a pointer to the Power to Current Colwersion data from
 * HW_THRESHOLD power policy
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_LWRRENT_COLWERSION))
#define pwrPolicyHWThresPccSet(pHtDst, pHtSrc) ((pHtDst)->pcc = (pHtSrc)->pcc)
#else
#define pwrPolicyHWThresPccSet(pHtDst, pHtSrc) do {} while (LW_FALSE)
#endif
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet      (pwrPolicyGrpIfaceModel10ObjSetImpl_HW_THRESHOLD)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_HW_THRESHOLD");
PwrPolicyLoad          (pwrPolicyLoad_HW_THRESHOLD)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicyLoad_HW_THRESHOLD");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRPOLICY_HWTHRESHOLD_H
