/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file client_clk_prop_top_pol.c
 *
 * @brief Module managing all state related to the CLIENT_CLK_PROP_TOP_POL structure.
 *
 * @ref - https://confluence.lwpu.com/display/RMPER/Clock+Propagation
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkClientClkPropTopPolIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkClientClkPropTopPolIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkClientClkPropTopPolIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkClientClkPropTopPolIfaceModel10SetEntry");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLIENT_CLK_PROP_TOP_POL handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkClientClkPropTopPolBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,        // _grpType
        CLIENT_CLK_PROP_TOP_POL,                        // _class
        pBuffer,                                        // _pBuffer
        s_clkClientClkPropTopPolIfaceModel10SetHeader,        // _hdrFunc
        s_clkClientClkPropTopPolIfaceModel10SetEntry,         // _entryFunc
        ga10xAndLater.clk.clkClientClkPropTopPolGrpSet, // _ssElement
        OVL_INDEX_DMEM(perf),                           // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);        // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkClientClkPropTopPolBoardObjGrpIfaceModel10Set_exit;
    }

    // Skip the arbitration if there are no policys.
    if (!BOARDOBJGRP_IS_EMPTY(CLIENT_CLK_PROP_TOP_POL))
    {
        // Arbitrate among the active policys to determine and force topology.
        PMU_ASSERT_OK_OR_GOTO(status,
            clkClientClkPropTopPolsArbitrate(CLK_CLIENT_CLK_PROP_TOP_POLS_GET()),
            clkClientClkPropTopPolBoardObjGrpIfaceModel10Set_exit);
    }

clkClientClkPropTopPolBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLIENT_CLK_PROP_TOP_POL class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkClientClkPropTopPolsArbitrate
(
    CLIENT_CLK_PROP_TOP_POLS *pPropTopPols
)
{
    CLK_PROP_TOPS           *pPropTops = CLK_CLK_PROP_TOPS_GET();
    CLIENT_CLK_PROP_TOP_POL *pPropTopPol;
    LwBoardObjIdx            idx;
    LwU8                     topId     = LW2080_CTRL_CLK_CLK_PROP_TOP_ID_ILWALID;
    FLCN_STATUS              status;

    //
    // @note
    // The current implementation assumes that there is only one
    // valid policy. Based on this assumption there is no need for
    // arbitration among all policys request for final topology to
    // force.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        ((boardObjGrpMaskBitSetCount(&pPropTopPols->super.objMask) == 1) ?
            FLCN_OK : FLCN_ERR_NOT_SUPPORTED),
        clkClientClkPropTopPolsArbitrate_exit);

    // Evaluate the final topology index to force.
    BOARDOBJGRP_ITERATOR_BEGIN(CLIENT_CLK_PROP_TOP_POL, pPropTopPol, idx, NULL)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            clkClientClkPropTopPolGetChosenTopId(pPropTopPol, &topId),
            clkClientClkPropTopPolsArbitrate_exit);
    }
    BOARDOBJGRP_ITERATOR_END;

    // Force the active topology.
    PMU_ASSERT_OK_OR_GOTO(status,
        clkPropTopsSetActiveTopologyForced(pPropTops, topId),
        clkClientClkPropTopPolsArbitrate_exit);

clkClientClkPropTopPolsArbitrate_exit:
    return status;
}

/*!
 * CLIENT_CLK_PROP_TOP_POL class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkClientClkPropTopPolGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_BOARDOBJ_SET *pPropTopPolSet =
        (RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_BOARDOBJ_SET *)pBoardObjSet;
    CLIENT_CLK_PROP_TOP_POL   *pPropTopPol;
    FLCN_STATUS                status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkClientClkPropTopPolGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pPropTopPol = (CLIENT_CLK_PROP_TOP_POL *)*ppBoardObj;

    // Copy the CLIENT_CLK_PROP_TOP_POL-specific data.
    pPropTopPol->topPolId = pPropTopPolSet->topPolId;

clkClientClkPropTopPolGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * CLK_PROP_TOP_POL implementation.
 *
 * @copydoc ClkClientClkPropTopPolGetChosenTopId
 */
FLCN_STATUS
clkClientClkPropTopPolGetChosenTopId
(
    CLIENT_CLK_PROP_TOP_POL *pPropTopPol,
    LwU8                    *pTopId
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Init
    (*pTopId) = LW2080_CTRL_CLK_CLK_PROP_TOP_ID_ILWALID;

    switch (BOARDOBJ_GET_TYPE(pPropTopPol))
    {
        case LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_TYPE_1X_SLIDER:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_PROP_TOP_POL))
            {
                status = clkClientClkPropTopPolGetChosenTopId_1X_SLIDER(pPropTopPol,
                                                                        pTopId);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLIENT_CLK_PROP_TOP_POL implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkClientClkPropTopPolIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_CLK_CLIENT_CLK_PROP_TOP_POL_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLIENT_CLK_PROP_TOP_POLS  *pPropTopPols = (CLIENT_CLK_PROP_TOP_POLS *)pBObjGrp;
    FLCN_STATUS                status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkClientClkPropTopPolIfaceModel10SetHeader_exit;
    }

    // Copy global CLK_PROP_TOPS state.
    pPropTopPols->topPolHal = pSet->topPolHal;

s_clkClientClkPropTopPolIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLIENT_CLK_PROP_TOP_POL implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkClientClkPropTopPolIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBoardObjSet->type)
    {
        case LW2080_CTRL_CLK_CLIENT_CLK_PROP_TOP_POL_TYPE_1X_SLIDER:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLIENT_CLK_PROP_TOP_POL))
            {
                status = clkClientClkPropTopPolGrpIfaceModel10ObjSet_1X_SLIDER(pModel10,
                                  ppBoardObj,
                                  sizeof(CLIENT_CLK_PROP_TOP_POL_1X_SLIDER),
                                  pBoardObjSet);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}
