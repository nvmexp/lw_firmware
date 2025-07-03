/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrchannel.c
 * @brief Interface specification for a PWR_CHANNEL.
 *
 * A "Power Channel" is a logical construct representing a collection of power
 * sensors whose combined power (and possibly voltage or current, in the future)
 * are monitored together as input to a PWR_MONITOR algorithm.
 *
 * This input will be "sampled" 1 or more times for each "iteration" and
 * statistical data (mean, min, max - possibly will be expanded) will be
 * collected from the samples as possible input for each iteration of the
 * PWR_MONITOR algorithm.
 *
 * This channel of readings might may also be assigned a power limit which a
 * PWR_MONITOR algorithm may try to enforce with some sort of corrective
 * algorithm (clock slowdown, pstate changes, etc.).
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel.h"
#include "pmgr/pwrchannel_1x.h"
#include "pmgr/pwrchannel_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_CHANNEL handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
pwrChannelBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        PWR_CHANNEL,                                // _class
        pBuffer,                                    // _pBuffer
        pmgrPwrMonitorIfaceModel10SetHeader_SUPER,        // _hdrFunc
        pmgrPwrChannelIfaceModel10SetEntry,               // _entryFunc
        all.pmgr.pwrChannelPack.channels,           // _ssElement
        OVL_INDEX_DMEM(pmgr),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrChannelBoardObjGrpIfaceModel10Set_done;
    }

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        PWR_CHRELATIONSHIP,                         // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        pmgrPwrChRelIfaceModel10SetEntry,                 // _entryFunc
        all.pmgr.pwrChannelPack.chRels,             // _ssElement
        OVL_INDEX_DMEM(pmgr),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables

pwrChannelBoardObjGrpIfaceModel10Set_done:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
pmgrPwrChannelIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_DEFAULT:
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_ESTIMATION:
            return pwrChannelGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj,
                sizeof(PWR_CHANNEL), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            return pwrChannelGrpIfaceModel10ObjSetImpl_SENSOR(pModel10, ppBoardObj,
                sizeof(PWR_CHANNEL_SENSOR), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            return pwrChannelGrpIfaceModel10ObjSetImpl_SUMMATION(pModel10, ppBoardObj,
                sizeof(PWR_CHANNEL_SUMMATION), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_PSTATE_ESTIMATION_LUT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_CHANNEL_PSTATE_ESTIMATION_LUT);
            return pwrChannelGrpIfaceModel10ObjSetImpl_PSTATE_ESTIMATION_LUT(pModel10, ppBoardObj,
                sizeof(PWR_CHANNEL_PSTATE_ESTIMATION_LUT), pBuf);

        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR_CLIENT_ALIGNED:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_CHANNEL_SENSOR_CLIENT_ALIGNED);
            return pwrChannelGrpIfaceModel10ObjSetImpl_SENSOR_CLIENT_ALIGNED(pModel10, ppBoardObj,
                sizeof(PWR_CHANNEL_SENSOR_CLIENT_ALIGNED), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
pwrChannelBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PWR_MONITOR_QUERY_PROLOGUE(Pmgr.pPwrMonitor, LW_TRUE);
        {
            status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
                PWR_CHANNEL,                                    // _class
                pBuffer,                                        // _pBuffer
                pwrChannelIfaceModel10GetStatusHeaderPhysical,              // _hdrFunc
                pwrChannelIfaceModel10GetStatus,                            // _entryFunc
                all.pmgr.pwrChannelGrpGetStatusPack.channels);  // _ssElement
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrChannelBoardObjGrpIfaceModel10GetStatus_exit;
            }

            status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
                PWR_CHANNEL,                                    // _class
                pBuffer,                                        // _pBuffer
                pwrChannelIfaceModel10GetStatusHeaderLogical,               // _hdrFunc
                pwrChannelIfaceModel10GetStatus,                            // _entryFunc
                all.pmgr.pwrChannelGrpGetStatusPack.channels);  // _ssElement
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrChannelBoardObjGrpIfaceModel10GetStatus_exit;
            }

            if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_TOPOLOGY))
            {
                status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                    // _grpType
                    PWR_CHRELATIONSHIP,                                 // _class
                    pBuffer,                                            // _pBuffer
                    NULL,                                               // _hdrFunc
                    pwrChRelationshipIfaceModel10GetStatus,                         // _entryFunc
                    all.pmgr.pwrChannelGrpGetStatusPack.channelRels);   // _ssElement
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrChannelBoardObjGrpIfaceModel10GetStatus_exit;
                }
            }

pwrChannelBoardObjGrpIfaceModel10GetStatus_exit:
            lwosNOP();
        }
        PWR_MONITOR_QUERY_EPILOGUE(Pmgr.pPwrMonitor, LW_TRUE);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Update pMask to include dependent physical channels which will be queried
 * first.
 *
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
pwrChannelIfaceModel10GetStatusHeaderPhysical
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    FLCN_STATUS         status  = FLCN_OK;
    BOARDOBJGRPMASK_E32 mask;

    // Init and copy in the header mask.
    boardObjGrpMaskInit_E32(&mask);
    status = boardObjGrpMaskImport_E32(&mask, &(pHdr->super.objMask));
    if (status != FLCN_OK)
    {
        goto pwrChannelIfaceModel10GetStatusHeaderPhysical_exit;
    }

    // Update the to-be-traversed mask to include only dependent physical channels.
    pMask->pData[0] = pwrMonitorDependentPhysicalChMaskGet(Pmgr.pPwrMonitor,
        mask.super.pData[0]);

pwrChannelIfaceModel10GetStatusHeaderPhysical_exit:
    return status;
}

/*!
 * Update pMask to include rest of the channels which were not queried
 * with @ref pwrChannelIfaceModel10GetStatusHeaderPhysical and update header mask to
 * include all dependent and requested channels.
 *
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
pwrChannelIfaceModel10GetStatusHeaderLogical
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    FLCN_STATUS         status = FLCN_OK;
    BOARDOBJGRPMASK_E32 mask;

    // Init and copy in the header mask.
    boardObjGrpMaskInit_E32(&mask);
    status = boardObjGrpMaskImport_E32(&mask, &(pHdr->super.objMask));
    if (status != FLCN_OK)
    {
        goto pwrChannelIfaceModel10GetStatusHeaderLogical_exit;
    }

    // Mask of the rest of the channels
    pMask->pData[0] = pwrMonitorDependentLogicalChMaskGet(Pmgr.pPwrMonitor,
                        mask.super.pData[0]);

    // Create flattened mask of all channels requested and dependent channels.
    mask.super.pData[0] = pMask->pData[0] |
                            pwrMonitorDependentPhysicalChMaskGet(Pmgr.pPwrMonitor,
                                mask.super.pData[0]);

    // Copy out the flattened mask to the header mask.
    status = boardObjGrpMaskExport_E32(&mask, &(pHdr->super.objMask));
    if (status != FLCN_OK)
    {
        goto pwrChannelIfaceModel10GetStatusHeaderLogical_exit;
    }

pwrChannelIfaceModel10GetStatusHeaderLogical_exit:
    return status;
}

/*!
 * @copydoc PwrChannelLimitSet
 * Default behavior of this function would be a PMU_ASSERT i.e. This
 * function should only be called on those channels with implementation
 * of this interface.
 */
FLCN_STATUS
pwrChannelLimitSet
(
    PWR_CHANNEL    *pChannel,
    LwU8            limitIdx,
    LwU8            limitUnit,
    LwU32           limitValue
)
{
    switch (BOARDOBJ_GET_TYPE(pChannel))
    {
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            return pwrChannelLimitSet_SENSOR(pChannel, limitIdx, limitUnit,
                                             limitValue);

        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SENSOR_CLIENT_ALIGNED:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_CHANNEL_SENSOR_CLIENT_ALIGNED);
            return pwrChannelLimitSet_SENSOR_CLIENT_ALIGNED(pChannel, limitIdx, limitUnit,
                                             limitValue);
    }
    PMU_HALT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PwrChannelContains
 */
LwBool
pwrChannelContains
(
    PWR_CHANNEL *pChannel,
    LwU8         chIdx,
    LwU8         chIdxEval
)
{
    // Check for NULL here, so that the callers don't have to check
    if (pChannel == NULL)
    {
        PMU_BREAKPOINT();
        return LW_FALSE;
    }

    // Simple equality check.
    if (BOARDOBJ_GET_GRP_IDX_8BIT(&pChannel->super) == chIdx)
    {
        return LW_TRUE;
    }

    //
    // If evaluating PWR_CHANNEL is detected, this is a cirlwlar reference.
    // Mark this as LW_FALSE.
    //
    if (BOARDOBJ_GET_GRP_IDX_8BIT(&pChannel->super) == chIdxEval)
    {
        return LW_FALSE;
    }

    // If not same channel, see if referenced via PWR_CHRELATIONSHIPs.
    switch (BOARDOBJ_GET_TYPE(pChannel))
    {
        case LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_MONITOR_TOPOLOGY);
            return pwrChannelContains_SUMMATION(pChannel, chIdx, chIdxEval);
        }
    }

    return LW_FALSE;
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrChannelGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJ_SET *pDescTmp =
        (RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_CHANNEL    *pChannel          = NULL;
    FLCN_STATUS     status            = FLCN_OK;
    LwBool          bFirstConstruct   = (*ppBoardObj == NULL);

    // Allocate the object if not already allocated
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrChannelGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pChannel = (PWR_CHANNEL *)*ppBoardObj;

    // Set class-specific data.
    pChannel->pwrRail = pDescTmp->pwrRail;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_1X))
    {
        status = pwrChannelConstructSuper_1X(pChannel, pDescTmp, bFirstConstruct);
        if (status != FLCN_OK)
        {
            goto pwrChannelGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X))
    {
        status = pwrChannelConstructSuper_2X(pChannel, pDescTmp, bFirstConstruct);
        if (status != FLCN_OK)
        {
            goto pwrChannelGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }

pwrChannelGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc PwrChannelsStatusGet
 */
FLCN_STATUS
pwrChannelsStatusGet
(
    PWR_CHANNELS_STATUS *pChannelsStatus
)
{
    PWR_CHANNEL        *pChannel;
    LwU32               depPhysChMask;
    BOARDOBJGRPMASK_E32 mask;
    LwBoardObjIdx       i;
    FLCN_STATUS         status = FLCN_OK;

    // Initialize the mask of the objects to be traversed.
    boardObjGrpMaskInit_E32(&mask);

    // Sanity check that requested mask is a sub-set of the PWR_CHANNELS group.
    if (!boardObjGrpMaskIsSubset(&(pChannelsStatus->mask), &(Pmgr.pwr.channels.objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        return status;
    }

    PWR_MONITOR_QUERY_PROLOGUE(Pmgr.pPwrMonitor, LW_FALSE);

    // Update the to-be-traversed mask to include only dependent physical channels.
    depPhysChMask = mask.super.pData[0] = pwrMonitorDependentPhysicalChMaskGet(Pmgr.pPwrMonitor,
        pChannelsStatus->mask.super.pData[0]);

    // First pass to query dependent physical channels.
    BOARDOBJGRP_ITERATOR_BEGIN(PWR_CHANNEL, pChannel, i, &(mask.super))
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pChannel->super);

        status = pwrChannelIfaceModel10GetStatus(
            pObjModel10,
            &pChannelsStatus->channels[i].boardObj);
        if (status != FLCN_OK)
        {
            goto pwrChannelsStatusGet_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Mask of the rest of the channels
    mask.super.pData[0] = pwrMonitorDependentLogicalChMaskGet(Pmgr.pPwrMonitor,
        pChannelsStatus->mask.super.pData[0]);

    // Second pass to query rest of the channels.
    BOARDOBJGRP_ITERATOR_BEGIN(PWR_CHANNEL, pChannel, i, &(mask.super))
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pChannel->super);

        status = pwrChannelIfaceModel10GetStatus(
            pObjModel10,
            &pChannelsStatus->channels[i].boardObj);
        if (status != FLCN_OK)
        {
            goto pwrChannelsStatusGet_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    //
    // Update the mask of PWR_CHANNELS actually referenced as the union of the
    // dependent physical and logical masks.
    //
    pChannelsStatus->mask.super.pData[0] = mask.super.pData[0] | depPhysChMask;

pwrChannelsStatusGet_exit:

    PWR_MONITOR_QUERY_EPILOGUE(Pmgr.pPwrMonitor, LW_FALSE);
    return status;
}

/*!
 * @brief   Public function to load all power channels.
 */
FLCN_STATUS
pwrChannelsLoad(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
    {
        status = pwrChannelsLoad_35();
        if (status != FLCN_OK)
        {
            goto pwrChannelsLoad_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNELS_PMUMON))
    {
        status = pmgrPwrChannelsPmumonLoad();
        if (status != FLCN_OK)
        {
            goto pwrChannelsLoad_exit;
        }
    }

pwrChannelsLoad_exit:
    return status;
}

/*!
 * @brief   Public function to unload all power channels.
 */
void pwrChannelsUnload(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNELS_PMUMON))
    {
        pmgrPwrChannelsPmumonUnload();
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
    {
        pwrChannelsUnload_35();
    }
}

/* ------------------------- Private Functions ------------------------------ */
