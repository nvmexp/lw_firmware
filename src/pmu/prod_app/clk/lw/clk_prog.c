/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prog.c
 *
 * @brief Module managing all state related to the CLK_PROG structure.  This
 * structure defines how to program a given frequency on given CLK_DOMAIN -
 * including which frequency generator to use and potentially the necessary VF
 * mapping, per the VBIOS specification.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkProgIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkProgIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkProgIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkProgIfaceModel10SetEntry");
static BoardObjIfaceModel10GetStatus              (s_clkProgIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkProgIfaceModel10GetStatus");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROG handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkProgBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    LwBool      bFirstTime = (Clk.progs.super.super.objSlots == 0U);

    // Increment the VF Points cache counter due to change in CLK_PROGs params
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    {
        clkVfPointsVfPointsCacheCounterIncrement();
    }

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E255,            // _grpType
        CLK_PROG,                                   // _class
        pBuffer,                                    // _pBuffer
        s_clkProgIfaceModel10SetHeader,                   // _hdrFunc
        s_clkProgIfaceModel10SetEntry,                    // _entryFunc
        pascalAndLater.clk.clkProgGrpSet,           // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgBoardObjGrpIfaceModel10Set_exit;
    }

    //
    // On version 35, Ilwalidate the CLK PROGs on SET CONTROL.
    // @note SET_CONTROL are not supported on AUTO as we do not
    // support OVOC on AUTO.
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_OVOC_SUPPORTED)) &&
        (!bFirstTime))
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
            goto clkProgBoardObjGrpIfaceModel10Set_exit;
        }
    }

clkProgBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLK_PROG handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkProgBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E255,    // _grpType
        CLK_PROG,                                   // _class
        pBuffer,                                    // _pBuffer
        NULL,                                       // _hdrFunc
        s_clkProgIfaceModel10GetStatus,                         // _entryFunc
        pascalAndLater.clk.clkProgGrpGetStatus);    // _ssElement
}

/*!
 * CLK_PROG super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkProgGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjVtableGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkProgGrpIfaceModel10ObjSet_SUPER_exit;
    }

    // Copy the CLK_PROG-specific data.

clkProgGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_PROG implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkProgIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROG_BOARDOBJGRP_SET_HEADER *pProgsSet =
        (RM_PMU_CLK_CLK_PROG_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLK_PROGS *pProgs = (CLK_PROGS *)pBObjGrp;
    FLCN_STATUS status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkProgIfaceModel10SetHeader_exit;
    }

    // Sanity check the number of secondary entries before copy to global CLK_PROGS
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if ((pProgsSet->secondaryEntryCount >
             LW2080_CTRL_CLK_PROG_1X_PRIMARY_MAX_SECONDARY_ENTRIES) ||
            (pProgsSet->vfEntryCount >
             LW2080_CTRL_CLK_CLK_PROG_1X_PRIMARY_VF_ENTRY_MAX_ENTRIES) ||
            (pProgsSet->vfSecEntryCount >
             LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_SEC_VF_ENTRY_VOLTRAIL_MAX))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto s_clkProgIfaceModel10SetHeader_exit;
        }
    }

    // Copy global CLK_PROGS state.
    pProgs->secondaryEntryCount = pProgsSet->secondaryEntryCount;
    pProgs->vfEntryCount    = pProgsSet->vfEntryCount;
    pProgs->vfSecEntryCount = pProgsSet->vfSecEntryCount;

s_clkProgIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_PROG implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkProgIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG_30))
            {
                status = clkProgGrpIfaceModel10ObjSet_30(pModel10, ppBoardObj,
                            sizeof(CLK_PROG_30), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_RATIO:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG_30))
            {
                status = clkProgGrpIfaceModel10ObjSet_30_PRIMARY_RATIO(pModel10, ppBoardObj,
                            sizeof(CLK_PROG_30_PRIMARY_RATIO), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_30_PRIMARY_TABLE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG_30))
            {
                status = clkProgGrpIfaceModel10ObjSet_30_PRIMARY_TABLE(pModel10, ppBoardObj,
                            sizeof(CLK_PROG_30_PRIMARY_TABLE), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG_BASE_35))
            {
                status = clkProgGrpIfaceModel10ObjSet_35(pModel10, ppBoardObj,
                            sizeof(CLK_PROG_35), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG_BASE_35))
            {
                status = clkProgGrpIfaceModel10ObjSet_35_PRIMARY_RATIO(pModel10, ppBoardObj,
                            sizeof(CLK_PROG_35_PRIMARY_RATIO), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG_BASE_35))
            {
                status = clkProgGrpIfaceModel10ObjSet_35_PRIMARY_TABLE(pModel10, ppBoardObj,
                            sizeof(CLK_PROG_35_PRIMARY_TABLE), pBuf);
            }
            break;
        }
        default:
        {
            // Do Nothing
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
 * @copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_clkProgIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_RATIO:
        case LW2080_CTRL_CLK_CLK_PROG_TYPE_35_PRIMARY_TABLE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROG_BASE_35))
            {
                status = clkProgIfaceModel10GetStatus_35(pModel10, pBuf);
            }
            break;
        }
        default:
        {
            // Do Nothing
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }
    return status;
}
