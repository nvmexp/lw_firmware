/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_totalgpu_iface.c
 * @brief  Interface specification for a PWR_POLICY_TOTAL_GPU_INTERFACE.
 *
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _TOTAL_GPU_INTERFACE PWR_POLICY object.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
pwrPolicyConstructImpl_TOTAL_GPU_INTERFACE
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_INTERFACE_BOARDOBJ_SET *pTotalGpuSet =
        (RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_INTERFACE_BOARDOBJ_SET *)pInterfaceDesc;
    PWR_POLICY_TOTAL_GPU_INTERFACE  *tgpIface;
    FLCN_STATUS  status;

    // Construct and initialize parent-class object.
    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    tgpIface = (PWR_POLICY_TOTAL_GPU_INTERFACE *)pInterface;

    // Copy in the _TOTAL_GPU type-specific data.
    tgpIface->fbPolicyRelIdx   = pTotalGpuSet->fbPolicyRelIdx;
    tgpIface->corePolicyRelIdx = pTotalGpuSet->corePolicyRelIdx;
    tgpIface->bUseChannelIdxForStatic
                               = pTotalGpuSet->bUseChannelIdxForStatic;
    tgpIface->staticVal        = pTotalGpuSet->staticVal;
    tgpIface->limitInflection0 = pTotalGpuSet->limitInflection0;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_TGP_MULTI_INFLECTIONS))
    tgpIface->limitInflection1 = pTotalGpuSet->limitInflection1;
#endif

    return status;
}

/*!
 * Query interface implementation for the TGP interface class
 *
 * @param[in] pWorkIface   PWR_POLICY_TOTAL_GPU_INTERFACE *pointer.
 * @param[in] pGetStatus   RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_INTERFACE_BOARDOBJ_GET_STATUS pointer
 *
 * @return FLCN_OK
 */
FLCN_STATUS
pwrPolicyGetStatus_TOTAL_GPU_INTERFACE
(
    PWR_POLICY_TOTAL_GPU_INTERFACE                                  *pTgpIface,
    RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_INTERFACE_BOARDOBJ_GET_STATUS  *pGetStatus
)
{
    FLCN_STATUS                     status = FLCN_OK;

    return status;
}

/*!
 * Interface implementation to provide a mask of topologies
 * that this policy evaluates.
 *
 * @param[in] pTgpIface   PWR_POLICY_TOTAL_GPU_INTERFACE *pointer.
 *
 * @return LwU32 channel mask for all the related rails.
 */
LwU32
pwrPolicy3XChannelMaskGet_TOTAL_GPU_INTERFACE
(
    PWR_POLICY_TOTAL_GPU_INTERFACE  *pTgpIface
)
{
    PWR_POLICY            *pPolicy     =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pTgpIface);
    LwU32                  channelMask = 0;

    // Update the channelMask with super class channel.
    channelMask = BIT(pPolicy->chIdx);

    // Update the channelMask with index for retrieving static rail power.
    if (pTgpIface->bUseChannelIdxForStatic)
    {
        channelMask |= BIT(pTgpIface->staticVal.pwrChannelIdx);
    }

    return channelMask;
}


/*!
 * Interface implementation for power policy 3.X specific filter function. This interface will
 * take additional @ref RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD pointer as
 * parameter for filter usage.
 *
 * @param[in]   pTgpIface   PWR_POLICY_TOTAL_GPU_INTERFACE pointer
 * @param[in]   pMon        PWR_MONITOR pointer
 * @param[in]   pPayload    Pointer to queried tuples data
 *
 * @return      FLCN_OK     Filter operation completed successfully
 * @return      other
 *      Error message propagated from inner functions
 */
FLCN_STATUS
pwrPolicy3XFilter_TOTAL_GPU_INTERFACE
(
    PWR_POLICY_TOTAL_GPU_INTERFACE    *pTgpIface,
    PWR_MONITOR                       *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    PWR_POLICY                     *pPolicy   =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pTgpIface);
    LW2080_CTRL_PMGR_PWR_TUPLE     *pTuple;
    FLCN_STATUS                     status    = FLCN_OK;

    // Call the super class implementation.
    status = pwrPolicy3XFilter_SUPER(pPolicy, pMon, pPayload);
    if (status != FLCN_OK)
    {
        goto pwrPolicy3XFilter_TOTAL_GPU_INTERFACE_done;
    }

    // Return early if this field is FALSE. We don't need to filter.
    if (!pTgpIface->bUseChannelIdxForStatic)
    {
        status = FLCN_OK;
        goto pwrPolicy3XFilter_TOTAL_GPU_INTERFACE_done;
    }

    pTuple = pwrPolicy3xFilterPayloadTupleGet(pPayload, pTgpIface->staticVal.pwrChannelIdx);
    pTgpIface->lastStaticPower = pTuple->pwrmW;

pwrPolicy3XFilter_TOTAL_GPU_INTERFACE_done:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
