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
 * @file  pwrpolicy_totalgpu.h
 * @brief @copydoc pwrpolicy_totalgpu.c
 */

#ifndef PWRPOLICY_TOTALGPU_H
#define PWRPOLICY_TOTALGPU_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy_limit.h"
#include "pmgr/3x/pwrpolicy_3x.h"
#include "pmgr/pff.h"
#include "pmgr/pwrpolicy_totalgpu_iface.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_POLICY_TOTAL_GPU PWR_POLICY_TOTAL_GPU;

/*!
 * Structure representing a proportional-limit-based power policy.
 */
struct PWR_POLICY_TOTAL_GPU
{
    /*!
     * @copydoc PWR_POLICY_LIMIT
     */
    PWR_POLICY_LIMIT super;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_TGP_MULTI_INFLECTIONS))
    /*!
     * Inflection point 2
     */
    LwU32 limitInflection2;
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
    /*!
     * Control and status information defining the optional piecewise linear
     * frequency flooring lwrve that this policy may enforce.
     */
    PIECEWISE_FREQUENCY_FLOOR pff;
#endif
    /*!
     * TGP interface
     */
    PWR_POLICY_TOTAL_GPU_INTERFACE tgpIface;
};

/* ------------------------- Defines --------------------------------------- */
/*!
 * Helper macro to get a pointer to the piecewise frequency floor data from
 * the given TOTAL_GPU power policy.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define pwrPolicyTotalGpuPffGet(pTgp)   (&(pTgp)->pff)
#else
#define pwrPolicyTotalGpuPffGet(pTgp)   ((PIECEWISE_FREQUENCY_FLOOR *)(NULL))
#endif

/*!
 * Helper macro to set the piecewise frequency floor data used by the the given
 * TOTAL_GPU power policy.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR))
#define pwrPolicyTotalGpuPffSet(pTgp, pffControl)   ((pTgp)->pff.control = pffControl)
#else
#define pwrPolicyTotalGpuPffSet(pTgp, pffControl)   do {} while (LW_FALSE)
#endif

/*!
 * Accessor macro for a TOTAL_GPU_INTERFACE interface from TOTAL_GPU super class.
 */
#define PWR_POLICY_TOTAL_GPU_IFACE_GET(pTotalGpu)                             \
    &((pTotalGpu)->tgpIface)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet                   (pwrPolicyGrpIfaceModel10ObjSetImpl_TOTAL_GPU)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_TOTAL_GPU");
BoardObjIfaceModel10GetStatus                       (pwrPolicyIfaceModel10GetStatus_TOTAL_GPU)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyIfaceModel10GetStatus_TOTAL_GPU");
PwrPolicyFilter                     (pwrPolicyFilter_TOTAL_GPU)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyFilter_TOTAL_GPU");
PwrPolicyGetPffCachedFrequencyFloor (pwrPolicyGetPffCachedFrequencyFloor_TOTAL_GPU)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyGetPffCachedFrequencyFloor_TOTAL_GPU");
PwrPolicyVfIlwalidate               (pwrPolicyVfIlwalidate_TOTAL_GPU)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyVfIlwalidate_TOTAL_GPU");

/*!
 * PWR_POLICY_3X interfaces
 */

/*!
 * PWR_POLICY_LIMIT interfaces
 */
PwrPolicyLimitEvaluate    (pwrPolicyLimitEvaluate_TOTAL_GPU)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicyLimitEvaluate_TOTAL_GPU");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRPOLICY_TOTALGPU_H
