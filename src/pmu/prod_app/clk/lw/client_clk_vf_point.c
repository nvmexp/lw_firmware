/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file client_clk_vf_point.c
 *
 * @brief TODO
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "dmemovl.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clientClkVfPointIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clientClkVfPointIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clientClkVfPointIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clientClkVfPointIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader (s_clientClkVfPointIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clientClkVfPointIfaceModel10GetStatusHeader");
static BoardObjIfaceModel10GetStatus              (s_clientClkVfPointIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clientClkVfPointIfaceModel10GetStatus");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLIENT_CLK_VF_POINT handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clientClkVfPointBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status     = FLCN_ERR_NOT_SUPPORTED;
    LwBool      bFirstTime = (Clk.clientVfPoints.super.super.objSlots == 0U);

    // Increment the VF Points cache counter due to change in CLK_DOMAINs params
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    {
        clkVfPointsVfPointsCacheCounterIncrement();
    }

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E255,   // _grpType
        CLIENT_CLK_VF_POINT,                        // _class
        pBuffer,                                    // _pBuffer
        s_clientClkVfPointIfaceModel10SetHeader,          // _hdrFunc
        s_clientClkVfPointIfaceModel10SetEntry,           // _entryFunc
        ga10xAndLater.clk.clientClkVfPointGrpSet,   // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkVfPointBoardObjGrpIfaceModel10Set_exit;
    }

    // Ilwalidate the VF lwrve on SET CONTROL.
    if (!bFirstTime)
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
        };

        // VFE evaluation is not required.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = clkIlwalidate(LW_FALSE);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clientClkVfPointBoardObjGrpIfaceModel10Set_exit;
        }
    }

clientClkVfPointBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLIENT_CLK_VF_POINT handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clientClkVfPointBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E255,          // _grpType
        CLIENT_CLK_VF_POINT,                                // _class
        pBuffer,                                            // _pBuffer
        s_clientClkVfPointIfaceModel10GetStatusHeader,                  // _hdrFunc
        s_clientClkVfPointIfaceModel10GetStatus,                        // _entryFunc
        ga10xAndLater.clk.clientClkVfPointGrpGetStatus);    // _ssElement
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkVfPointBoardObjGrpIfaceModel10GetStatus_exit;
    }

clientClkVfPointBoardObjGrpIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * CLIENT_CLK_VF_POINT super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clientClkVfPointGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLIENT_CLK_VF_POINT_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLIENT_CLK_VF_POINT_BOARDOBJ_SET *)pBoardObjDesc;
    CLIENT_CLK_VF_POINT    *pClientVfPoint;
    LwU8                    idx;
    FLCN_STATUS             status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkVfPointGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pClientVfPoint = (CLIENT_CLK_VF_POINT *)*ppBoardObj;

    // Copy out client_vf_point specific parameters
    pClientVfPoint->numLinks = pSet->numLinks;

    for (idx = 0U; idx < pSet->numLinks; idx++)
    {
        pClientVfPoint->link[idx].super.type       = pSet->link[idx].super.type;
        pClientVfPoint->link[idx].super.vfPointIdx = pSet->link[idx].super.vfPointIdx;

        // Type specific params.
        switch (pClientVfPoint->link[idx].super.type)
        {
            case CLIENT_CLK_VF_POINT_LINK_TYPE_FREQ:
            {
                pClientVfPoint->link[idx].freq.vfRelIdx = pSet->link[idx].freq.vfRelIdx;
                break;
            }
            case CLIENT_CLK_VF_POINT_LINK_TYPE_VOLT:
            {
                // Nothing to do.
                break;
            }
            default:
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto clientClkVfPointGrpIfaceModel10ObjSet_SUPER_exit;
            }
        }
    }

clientClkVfPointGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
clientClkVfPointIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_CLK_CLIENT_CLK_VF_POINT_BOARDOBJ_GET_STATUS *pVfPointStatus =
        (RM_PMU_CLK_CLIENT_CLK_VF_POINT_BOARDOBJ_GET_STATUS *)pPayload;
    CLIENT_CLK_VF_POINT    *pClientVfPoint  = (CLIENT_CLK_VF_POINT *)pBoardObj;
    CLK_VF_POINT           *pVfPoint;
    FLCN_STATUS             status;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkVfPointIfaceModel10GetStatus_SUPER_exit;
    }

    pVfPoint = CLK_VF_POINT_GET(PRI,
        pClientVfPoint->link[LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_LINK_PRIMARY].super.vfPointIdx);
    if (pVfPoint == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto clientClkVfPointIfaceModel10GetStatus_SUPER_exit;
    }

    status = clkVfPointClientVfTupleGet(pVfPoint, LW_TRUE, &pVfPointStatus->vfTupleOffset);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clientClkVfPointIfaceModel10GetStatus_SUPER_exit;
    }

clientClkVfPointIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLIENT_CLK_VF_POINT implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clientClkVfPointIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    FLCN_STATUS  status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clientClkVfPointIfaceModel10SetHeader_exit;
    }

    // Copy global CLIENT_CLK_VF_POINTS state.

s_clientClkVfPointIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * Constructs a CLIENT_CLK_VF_POINT of the corresponding type.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clientClkVfPointIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_FIXED_20:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))
            {
                status =  clientClkVfPointGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj,
                    sizeof(CLIENT_CLK_VF_POINT), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_PROG_20_FREQ:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))
            {
                status =  clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_FREQ(pModel10, ppBoardObj,
                    sizeof(CLIENT_CLK_VF_POINT_PROG_20_FREQ), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_PROG_20_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))
            {
                status =  clientClkVfPointGrpIfaceModel10ObjSet_PROG_20_VOLT(pModel10, ppBoardObj,
                    sizeof(CLIENT_CLK_VF_POINT_PROG_20_VOLT), pBuf);
            }
            break;
        }
        default:
        {
            // Not Supported
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_clientClkVfPointIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10         *pModel10,
    RM_PMU_BOARDOBJGRP  *pBuf,
    BOARDOBJGRPMASK     *pMask
)
{
    return FLCN_OK;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_clientClkVfPointIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_PROG_20:      // fall through
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_PROG_20_FREQ: // fall through
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_PROG_20_VOLT: // fall through
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))
            {
                status =  clientClkVfPointIfaceModel10GetStatus_PROG_20(pModel10, pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_PROG:     // fall through
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_FIXED:    // fall through
        case LW2080_CTRL_CLK_CLIENT_CLK_VF_POINT_TYPE_FIXED_20: // fall through
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_VF_POINT))
            {
                status =  clientClkVfPointIfaceModel10GetStatus_SUPER(pModel10, pBuf);
            }
            break;
        }
        default:
        {
            // Not Supported
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
