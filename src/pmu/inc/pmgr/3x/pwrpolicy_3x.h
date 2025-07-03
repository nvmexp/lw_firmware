/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicy_3x.h
 * @brief @copydoc pwrpolicy_3x.c
 */

#ifndef PWRPOLICY_3X_H
#define PWRPOLICY_3X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrpolicy.h"
#include "perf/cf/perf_cf_controller_status.h"

/* ------------------------- Types Definitions ----------------------------- */
// Alias PWR_POLICY_3X_FILTER_PAYLOAD_TYPE for PWR_POLICY_30
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30)
/*!
 * PWR_POLICY_30 implementation of PWR_POLICY_3X_FILTER_PAYLOAD_TYPE
 */
#define PWR_POLICY_3X_FILTER_PAYLOAD_TYPE                                      \
    RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD
/*!
 * PWR_POLICY_30 implementation of LW2080_CTRL_PMGR_PWR_TUPLE accessor for @ref
 * PWR_POLICY_3X_FILTER_PAYLOAD_TYPE.
 *
 * @param[in]  pPayload
 *     PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pointer
 * @param[in]  chIdx
 *     Index of PWR_CHANNEL to retrieve from the payload.
 *
 * @return Pointer to LW2080_CTRL_PMGR_PWR_TUPLE
 */
#define pwrPolicy3xFilterPayloadTupleGet(pPayload, chIdx)                      \
    &((pPayload)->channels[(chIdx)])

// Alias PWR_POLICY_3X_FILTER_PAYLOAD_TYPE for PWR_POLICY_35
#elif PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
/*!
 * PWR_POLICY_30 implementation of PWR_POLICY_3X_FILTER_PAYLOAD_TYPE
 */
#define PWR_POLICY_3X_FILTER_PAYLOAD_TYPE                                      \
    PWR_POLICY_MULTIPLIER_DATA
/*!
 * PWR_POLICY_30 implementation of LW2080_CTRL_PMGR_PWR_TUPLE accessor for @ref
 * PWR_POLICY_3X_FILTER_PAYLOAD_TYPE.
 *
 * @param[in]  pPayload
 *     PWR_POLICY_3X_FILTER_PAYLOAD_TYPE pointer
 * @param[in]  chIdx
 *     Index of PWR_CHANNEL to retrieve from the payload.
 *
 * @return Pointer to LW2080_CTRL_PMGR_PWR_TUPLE
 */
#define pwrPolicy3xFilterPayloadTupleGet(pPayload, chIdx)                      \
    pwrChannelsStatusChannelTupleGet(&pPayload->channelsStatus, chIdx)

// Alias PWR_POLICY_3X_FILTER_PAYLOAD_TYPE for unsupported implementations.
#else
#define PWR_POLICY_3X_FILTER_PAYLOAD_TYPE                                      \
    void
#define pwrPolicy3xFilterPayloadTupleGet(pPayload, chIdx)                      \
    NULL
#endif

/*!
 * @interface PWR_POLICY_3X
 *
 * Interface for power policy 3.X specific filter function. This interface will
 * take additional @ref RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD pointer as
 * parameter for filter usage.
 *
 * @param[in]   pPolicy     PWR_POLICY pointer
 * @param[in]   pMon        PWR_MONITOR pointer
 * @param[in]   pPayload    Pointer to queried tuples data
 *
 * @return      FLCN_OK     Filter operation completed successfully
 * @return      FLCN_ERR_NOT_SUPPORTED
 *      The limitUnit for this policy is not recognized
 * @return      other
 *      Error message propagated from inner functions
 */
#define PwrPolicy3XFilter(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy, PWR_MONITOR *pMon, PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload)

/*!
 * @interface PWR_POLICY_3X
 *
 * Interface that constructs power policy 3.X specific filters (i.e block,
 * moving average and iir etc.).
 *
 * @param[in]   pPolicy       PWR_POLICY pointer
 * @param[in]   pFilter       PWR_POLICY_3X_FILTER pointer
 * @param[in]   pBoardObjSet  RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET pointer
 * @param[in]   ovlIdxDmem    Overlay in which to allocate filter data.
 *
 * @return      FLCN_OK     Filter construction completed successfully
 * @return      FLCN_ERR_NOT_SUPPORTED
 *      If unknown filter type
 * @return      other
 *      Error message propagated from inner functions
 */
#define PwrPolicy3XConstructInputFilter(fname) FLCN_STATUS (fname)(PWR_POLICY *pPolicy, PWR_POLICY_3X_FILTER *pFilter, RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet, LwU8 ovlIdxDmem)

/*!
 * @interface PWR_POLICY_3X
 *
 * Evaluate power policy 3.X specific filters (i.e block average, moving
 * average and iir etc.).
 *
 * @param[in]   pFilter     PWR_POLICY_3X_FILTER pointer
 * @param[in]   input       Input value to feed into filter
 * @param[out]  pOutput     Pointer to output buffer which should be written with filter
 *     output when filtering is complete
 *
 * @return      FLCN_OK     Filter construction completed successfully
 * @return      FLCN_ERR_NOT_SUPPORTED
 *      If unknown filter type
 * @return      other
 *      Error message propagated from inner functions
 */
#define PwrPolicy3XInputFilter(fname) FLCN_STATUS (fname)(PWR_POLICY_3X_FILTER *pFilter, LwU32 input, LwU32 *pOutput)

/*!
 * @interface PWR_POLICY_3X
 *
 * Get the channel mask for given power policy
 *
 * @param[in]   pPolicy
 *      Pointer to PWR_POLICY structure
 *
 * @return
 *      Mask of channel index supported by given pwr policy
 */
#define PwrPolicy3XChannelMaskGet(fname) LwU32 (fname)(PWR_POLICY *pPolicy)

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * Constructor for the PWR_POLICY super-class.
 */
BoardObjGrpIfaceModel10ObjSet                   (pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X");
PwrPolicy3XConstructInputFilter     (pwrPolicy3XConstructInputFilter)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicy3XConstructInputFilter");
PwrPolicy3XFilter                   (pwrPolicy3XFilter)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter");
PwrPolicy3XFilter                   (pwrPolicy3XFilter_SUPER)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XFilter_SUPER");
PwrPolicy3XInputFilter              (pwrPolicy3XInputFilter)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XInputFilter");
PwrPolicy3XChannelMaskGet           (pwrPolicy3XChannelMaskGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPolicy3XChannelMaskGet");

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/3x/pwrpolicy_30.h"

#endif // PWRPOLICY_3X_H
