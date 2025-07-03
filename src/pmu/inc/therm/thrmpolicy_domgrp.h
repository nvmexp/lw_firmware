/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef THRMPOLICY_DOMGRP_H
#define THRMPOLICY_DOMGRP_H

/* ------------------------ Includes --------------------------------------- */
#include "therm/thrmpolicy.h"

/* ------------------------ Datatypes -------------------------------------- */
typedef struct THERM_POLICY_DOMGRP THERM_POLICY_DOMGRP;

/*!
 * Specifies whether or not the THERM_POLICY_DOMGRP is limited to the boost
 * clock range or not. If not, the THERM_POLICY_DOMGRP is allowed to limit
 * perf for all clock ranges.
 *
 * @param[in]  pPolicy  THERM_POLICY_DOMGRP pointer
 *
 * @return LW_TRUE  THERM_POLICY_DOMGRP is limited to boost clocks
 * @return LW_FALSE THERM_POLICY_DOMGRP can use all clocks.
 */
#define ThermPolicyDomGrpIsRatedTdpLimited(fname) LwBool (fname)(THERM_POLICY_DOMGRP *pPolicy)

/*!
 *
 * @param[in]  pPolicy  THERM_POLICY_DOMGRP pointer
 * @param[in]  pLimits
 *              Structure holding thermal policy perf. limits. If a particular
 *              limit does not need to be applied, the caller sets it to
 *              LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_VAL_NONE.
 *
 * @return FLCN_OK
 *      Limits were applied successfully to the policy; the arbiter has applied
 *      the most restrictive limits to the GPU.
 * @return Other errors
 *      Errors progatated up from called functions.
 */
#define ThermPolicyDomGrpApplyLimits(fname) FLCN_STATUS (fname)(THERM_POLICY_DOMGRP *pPolicy, LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS *pLimits)

/*!
 * Base Temperature Controller thermal policy class.
 */
struct THERM_POLICY_DOMGRP
{
    /*!
     * THERM_POLICY super class.
     */
    THERM_POLICY super;

    /*!
     * Parameters obtained from RM.
     */

    /*!
     * A boolean flag to indicate that the output limits computed by this
     * Temperature Controller should be floored to a virtual P-state.
     */
    LwBool       bRatedTdpVpstateFloor;

    /*!
     * Specifies the virtual P-state to use as the floor limit.
     */
    LwU8         vpstateFloorIdx;

    /*!
     * Parameters callwlated in the PMU.
     */

    /*!
     * Limits imposed by the controller.
     */
    LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS limits;
};

/* ------------------------ Defines -----------------------------------------*/
/* ------------------------ Macros ----------------------------------------- */
#define thermPolicyLoad_DOMGRP(pPolicy)     thermPolicyLoad_SUPER(pPolicy)

/*!
 * @brief   Helper macro clearing thermal policy's perf limits.
 *
 * @param   _pLimits    Ptr. of LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS to clear.
 */
#define thermPolicyPerfLimitsClear(_pLimits)                                   \
    do {                                                                       \
        LwU32 i;                                                               \
        for (i = 0; i < LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_NUM_LIMITS; i++) \
        {                                                                      \
            (_pLimits)->limit[i] =                                             \
                LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_VAL_NONE;                \
        }                                                                      \
    } while (LW_FALSE)

/* ------------------------- Public Functions ------------------------------ */
/*!
 * THERM_POLICY interfaces
 */
BoardObjGrpIfaceModel10ObjSet                   (thermPolicyGrpIfaceModel10ObjSetImpl_DOMGRP)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermPolicyGrpIfaceModel10ObjSetImpl_DOMGRP");

BoardObjIfaceModel10GetStatus                       (thermPolicyIfaceModel10GetStatus_DOMGRP)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyIfaceModel10GetStatus_DOMGRP");

RM_PMU_PERF_VF_ENTRY_INFO          *thermPolicyDomGrpPerfVfEntryGet(LwU8 vfEntryIdx)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDomGrpPerfVfEntryGet");

ThermPolicyDomGrpIsRatedTdpLimited  (thermPolicyDomGrpIsRatedTdpLimited)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDomGrpIsRatedTdpLimited");

ThermPolicyDomGrpApplyLimits        (thermPolicyDomGrpApplyLimits)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "thermPolicyDomGrpApplyLimits");

/* ------------------------ Include Derived Types -------------------------- */
#include "therm/thrmpolicy_dtc_vf.h"
#include "therm/thrmpolicy_dtc_volt.h"

#endif // THRMPOLICY_DOMGRP_H
