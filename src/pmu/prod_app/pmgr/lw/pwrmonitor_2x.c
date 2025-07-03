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
 * @file  pwrmonitor_2x.c
 * @brief PMGR Power Monitoring Specific to Power Topology Table 2.0.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "pmgr/pwrmonitor_2x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static FLCN_STATUS  s_pwrMonitorChannelsTupleQuery(PWR_MONITOR *pMon, LwU32 channelMask, LW2080_CTRL_BOARDOBJGRP_MASK_E32 *pChRelMask, RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD *pPayload, LwBool bFromRM)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "s_pwrMonitorChannelsTupleQuery")
    GCC_ATTRIB_NOINLINE();

static FLCN_STATUS  s_pwrMonitorChannelsTupleStatusGet(PWR_MONITOR *pMon, LwU32 channelMask, RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD *pPayload)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "s_pwrMonitorChannelsTupleStatusGet")
    GCC_ATTRIB_NOINLINE();

static FLCN_STATUS  s_pwrMonitorChannelRelsStatusGet(PWR_MONITOR *pMon, LW2080_CTRL_BOARDOBJGRP_MASK_E32 *pChRelMask, RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD *pPayload)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "s_pwrMonitorChannelRelsStatusGet")
    GCC_ATTRIB_NOINLINE();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PwrMonitorChannelTupleStatusGet
 *
 * @note Avoid directly calling this interface for obtaining a PWR_CHANNEL
 *       TUPLE as it might return cached values. Instead use @ref
 *       pwrMonitorChannelsTupleQuery for querying one or more PWR_CHANNELS.
 */
FLCN_STATUS
pwrMonitorChannelTupleStatusGet
(
    PWR_MONITOR                *pMon,
    LwU8                        channelIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_CHANNEL *pChannel;

    // Make sure that requested channel is supported!
    if ((Pmgr.pwr.channels.objMask.super.pData[0] & BIT(channelIdx)) == 0)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X_READABLE_CHANNEL))
    {
        // If requested channel is not readable, return tuple as 0
        if (!boardObjGrpMaskBitGet(&(pMon->readableChannelMask), channelIdx))
        {
            memset(pTuple, 0, sizeof(LW2080_CTRL_PMGR_PWR_TUPLE));
            return FLCN_OK;
        }
    }

    pChannel = PWR_CHANNEL_GET(channelIdx);
    if (pChannel == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_INDEX;
    }

    return pwrChannelTupleStatusGet(pChannel, pTuple);
}

/*!
 * @copydoc PwrMonitorChannelsTupleQuery
 */
FLCN_STATUS
pwrMonitorChannelsTupleQuery
(
    PWR_MONITOR                                    *pMon,
    LwU32                                           channelMask,
    LW2080_CTRL_BOARDOBJGRP_MASK_E32               *pChRelMask,
    RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD   *pPayload,
    LwBool                                          bFromRM
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibQuery)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj) // index #1
        OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR
    };

    //
    // Note that this code hard-codes index of 'libBoardObj' within ovlDescList!
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X_REDUCED_CHANNEL_STATUS) ||
        (pChRelMask == NULL))
    {
#ifndef ON_DEMAND_PAGING_BLK
        ovlDescList[1] = OSTASK_OVL_DESC_ILWALID_VAL;
#endif // ON_DEMAND_PAGING_BLK
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = s_pwrMonitorChannelsTupleQuery(pMon, channelMask, pChRelMask,
                                               pPayload, bFromRM);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc PwrMonitorChannelTupleStatusGet
 */
FLCN_STATUS
pwrMonitorChannelTupleQuery
(
    PWR_MONITOR                *pMon,
    LwU8                        channelIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS status;

    // Make sure that requested channel is supported!
    if ((Pmgr.pwr.channels.objMask.super.pData[0] & BIT(channelIdx)) == 0)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    PWR_MONITOR_QUERY_PROLOGUE(pMon, LW_FALSE);
    {
        status = pwrMonitorChannelTupleStatusGet(pMon, channelIdx, pTuple);
    }
    PWR_MONITOR_QUERY_EPILOGUE(pMon, LW_FALSE);

    return status;
}

/*!
 * Function to reset tuple status for all channels.
 */
void
pwrMonitorChannelsTupleReset
(
    PWR_MONITOR    *pMon,
    LwBool          bFromRM
)
{
    PWR_CHANNEL *pChannel;
    LwU8         c;

    FOR_EACH_INDEX_IN_MASK(32, c, Pmgr.pwr.channels.objMask.super.pData[0])
    {
        pChannel = PWR_CHANNEL_GET(c);
        if (pChannel == NULL)
        {
            PMU_BREAKPOINT();
            return;
        }
        pwrChannelTupleStatusReset(pChannel, bFromRM);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * Function to get the depended physical PWR_CHANNEL mask from an input
 * PWR_CHANNEL mask.
 *
 * Will return the mask of physical PWR_CHANNELs on which the specified set of
 * PWR_CHANNELs depends, including PWR_CHANNELs in the mask which are themselves
 * physical.
 *
 * @param[in]  pMon
 *     PWR_MONITOR pointer
 * @param[in]  channelMask
 *     Mask of PWR_CHANNELs for which to retrive the mask of their depended
 *     physical PWR_CHANNELs.
 *
 * @return Mask of depended physical PWR_CHANNELs.
 */
LwU32
pwrMonitorDependentPhysicalChMaskGet
(
    PWR_MONITOR  *pMon,
    LwU32         channelMask
)
{
    LwU32 depChMask = channelMask;
    LwU8  c;

    FOR_EACH_INDEX_IN_MASK(32, c, channelMask)
    {
        PWR_CHANNEL *pChannel;

        pChannel = PWR_CHANNEL_GET(c);
        if (pChannel == NULL)
        {
            PMU_BREAKPOINT();
            return 0;
        }

        depChMask |= pChannel->dependentChMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return (depChMask & pMon->physicalChannelMask);
}

/*!
 * Function to get the depended logical PWR_CHANNEL mask from an input
 * PWR_CHANNEL mask.
 *
 * Will return the mask of logical PWR_CHANNELs on which the
 * specified set of PWR_CHANNELs depends, including PWR_CHANNELs in the mask
 * which themselsvs are logical.
 *
 * @param[in]  pMon
 *     PWR_MONITOR pointer
 * @param[in]  channelMask
 *     Mask of PWR_CHANNELs for which to retrive the mask of their depended
 *     physical PWR_CHANNELs.
 *
 * @return Mask of depended physical PWR_CHANNELs.
 */
LwU32
pwrMonitorDependentLogicalChMaskGet
(
    PWR_MONITOR  *pMon,
    LwU32         channelMask
)
{
    LwU32 depChMask = channelMask;
    LwU8  c;

    FOR_EACH_INDEX_IN_MASK(32, c, channelMask)
    {
        PWR_CHANNEL *pChannel;

        pChannel = PWR_CHANNEL_GET(c);
        if (pChannel == NULL)
        {
            PMU_BREAKPOINT();
            return 0;
        }

        depChMask |= pChannel->dependentChMask;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return (depChMask & ~pMon->physicalChannelMask);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc PwrMonitorChannelsTupleQuery
 */
static FLCN_STATUS
s_pwrMonitorChannelsTupleQuery
(
    PWR_MONITOR                                    *pMon,
    LwU32                                           channelMask,
    LW2080_CTRL_BOARDOBJGRP_MASK_E32               *pChRelMask,
    RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD   *pPayload,
    LwBool                                          bFromRM
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32      depPhyChMask;
    //
    // As we have a hack that decreases local variable's size in pwrPoliciesFilter_3X() on
    // GM10x, the size of the array to be initialized should be cut to half as well.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X_REDUCED_CHANNEL_STATUS))
    {
        memset(pPayload->channels, 0,
            sizeof(LW2080_CTRL_PMGR_PWR_TUPLE) *
            LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS / 2);
    }
    else
    {
        memset(pPayload->channels, 0,
            sizeof(LW2080_CTRL_PMGR_PWR_TUPLE) *
            LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS);

        if (pChRelMask != NULL)
        {
            //
            // Doing a conditional memset here to reduce stack footprint for the
            // PWR_POLICY evaluation case where channel relationships do not need
            // to be queried.
            //
            memset(pPayload->chRels, 0,
                sizeof(RM_PMU_PMGR_PWR_CHRELATIONSHIP_BOARDOBJ_GET_STATUS_UNION) *
                LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHRELATIONSHIPS);
        }
    }

    PWR_MONITOR_QUERY_PROLOGUE(pMon, bFromRM);

    // Get the dependent physical channels for the input channelMask
    depPhyChMask =
        pwrMonitorDependentPhysicalChMaskGet(pMon, channelMask);

    // Cache tuple for physical channels first
    status = s_pwrMonitorChannelsTupleStatusGet(pMon, depPhyChMask, pPayload);
    if (status != FLCN_OK)
    {
        goto s_pwrMonitorChannelsTupleQuery_exit;
    }

    // remove physical channels which have already been queried above
    channelMask &= ~depPhyChMask;

    // Cache tuple for the rest of the channels
    status = s_pwrMonitorChannelsTupleStatusGet(pMon, channelMask, pPayload);
    if (status != FLCN_OK)
    {
        goto s_pwrMonitorChannelsTupleQuery_exit;
    }

    // Cache status of the channel relationships now
    if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X_REDUCED_CHANNEL_STATUS) &&
        (pChRelMask != NULL))
    {
        status = s_pwrMonitorChannelRelsStatusGet(pMon, pChRelMask, pPayload);
        if (status != FLCN_OK)
        {
            goto s_pwrMonitorChannelsTupleQuery_exit;
        }
    }

s_pwrMonitorChannelsTupleQuery_exit:
    PWR_MONITOR_QUERY_EPILOGUE(pMon, bFromRM);
    return status;
}

/*!
 * Helper call to loop over channels and cache their tuple
 */
static FLCN_STATUS
s_pwrMonitorChannelsTupleStatusGet
(
    PWR_MONITOR                                  *pMon,
    LwU32                                         channelMask,
    RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD *pPayload
)
{
    LwU8        channelIdx;
    FLCN_STATUS status = FLCN_OK;

    FOR_EACH_INDEX_IN_MASK(32, channelIdx, channelMask)
    {
        status = pwrMonitorChannelTupleStatusGet(pMon, channelIdx,
            &pPayload->channels[channelIdx]);
        if (status != FLCN_OK)
        {
            return status;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return status;
}

/*!
 * Helper call to loop over channel relationships and cache their status
 */
static FLCN_STATUS
s_pwrMonitorChannelRelsStatusGet
(
    PWR_MONITOR                                  *pMon,
    LW2080_CTRL_BOARDOBJGRP_MASK_E32             *pChRelMask,
    RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD *pPayload
)
{
    PWR_CHRELATIONSHIP *pChRel = NULL;
    LwBoardObjIdx       c;
    FLCN_STATUS         status = FLCN_OK;
    BOARDOBJGRPMASK_E32 chRelStatusMask;

    // Init and copy in the mask
    boardObjGrpMaskInit_E32(&chRelStatusMask);
    status = boardObjGrpMaskImport_E32(&chRelStatusMask,
                                       pChRelMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(PWR_CHRELATIONSHIP, pChRel, c,
        &chRelStatusMask.super)
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pChRel->super);

        status = pwrChRelationshipIfaceModel10GetStatus(pObjModel10,
            (RM_PMU_BOARDOBJ *)&pPayload->chRels[c]);
        if (status != FLCN_OK)
        {
            return status;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return status;
}
