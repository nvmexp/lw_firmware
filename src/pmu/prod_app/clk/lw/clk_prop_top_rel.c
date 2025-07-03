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
 * @file clk_prop_top_rel.c
 *
 * @brief Module managing all state related to the CLK_PROP_TOP_REL structure.
 *
 * @ref - https://confluence.lwpu.com/display/RMPER/Clock+Propagation
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkPropTopRelIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkPropTopRelIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkPropTopRelIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkPropTopRelIfaceModel10SetEntry");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP_TOP_REL handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkPropTopRelBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    LwBool      bFirstTime = (Clk.propTopRels.super.super.objSlots == 0U);

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E255,   // _grpType
        CLK_PROP_TOP_REL,                           // _class
        pBuffer,                                    // _pBuffer
        s_clkPropTopRelIfaceModel10SetHeader,             // _hdrFunc
        s_clkPropTopRelIfaceModel10SetEntry,              // _entryFunc
        ga10xAndLater.clk.clkPropTopRelGrpSet,      // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopRelBoardObjGrpIfaceModel10Set_exit;
    }

    // Ilwalidate on SET CONTROL.
    if (!bFirstTime)
    {
        // Ilwalidate PMU arbiter if supported.
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
        {
            //
            // Detach VFE IMEM and DMEM as it's not required anymore
            // and it's not possible to accommidate PERF, PERF_LIMIT,
            // and VFE DMEM/IMEM at the same time.
            //
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, perfVfe)
                OSTASK_OVL_DESC_BUILD(_DMEM, _DETACH, perfVfe)
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLw64Umul)
                OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libLwF32)
                PERF_LIMIT_OVERLAYS_BASE
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                // Skip further processing if perf_limits has not been inititalized
                if (!BOARDOBJGRP_IS_EMPTY(PERF_LIMIT))
                {
                    status = perfLimitsIlwalidate(PERF_PERF_LIMITS_GET(),
                                LW_TRUE,
                                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
                }
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkPropTopRelBoardObjGrpIfaceModel10Set_exit;
            }
        }
    }

clkPropTopRelBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLK_PROP_TOP_REL class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropTopRelGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_TOP_REL_BOARDOBJ_SET *pPropTopRelSet =
        (RM_PMU_CLK_CLK_PROP_TOP_REL_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP_TOP_REL   *pPropTopRel;
    FLCN_STATUS         status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopRelGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pPropTopRel = (CLK_PROP_TOP_REL *)*ppBoardObj;

    // Copy the CLK_PROP_TOP_REL-specific data.
    pPropTopRel->clkDomainIdxSrc = pPropTopRelSet->clkDomainIdxSrc;
    pPropTopRel->clkDomainIdxDst = pPropTopRelSet->clkDomainIdxDst;
    pPropTopRel->bBiDirectional  = pPropTopRelSet->bBiDirectional;

clkPropTopRelGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * CLK_PROP_TOP_REL implementation.
 *
 * @copydoc ClkPropTopRelFreqPropagate
 */
FLCN_STATUS
clkPropTopRelFreqPropagate
(
    CLK_PROP_TOP_REL   *pPropTopRel,
    LwBool              bSrcToDstProp,
    LwU16              *pFreqMHz
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Sanity checks.
    if (pFreqMHz == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_exit;
    }

    //
    // Ensure that relationship supports bi-direction propagation
    // when client is requesting dst -> src propagation.
    //
    if ((!bSrcToDstProp) &&
        (!pPropTopRel->bBiDirectional))
    {
        status = FLCN_ERR_ILLEGAL_OPERATION;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pPropTopRel))
    {
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_RATIO:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
            {
                status = clkPropTopRelFreqPropagate_1X_RATIO(pPropTopRel,
                                                             bSrcToDstProp,
                                                             pFreqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_TABLE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
            {
                status = clkPropTopRelFreqPropagate_1X_TABLE(pPropTopRel,
                                                             bSrcToDstProp,
                                                             pFreqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
            {
                status = clkPropTopRelFreqPropagate_1X_VOLT(pPropTopRel,
                                                            bSrcToDstProp,
                                                            pFreqMHz);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VFE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL_VFE))
            {
                status = clkPropTopRelFreqPropagate_1X_VFE(pPropTopRel,
                                                           bSrcToDstProp,
                                                           pFreqMHz);
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

clkPropTopRelFreqPropagate_exit:
    return status;
}

/*!
 * CLK_PROP_TOP_REL implementation.
 *
 * @copydoc ClkPropTopRelCache
 */
FLCN_STATUS
clkPropTopRelCache
(
    CLK_PROP_TOP_REL   *pPropTopRel
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Sanity checks.
    if (pPropTopRel == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkPropTopRelCache_exit;
    }

    switch (BOARDOBJ_GET_TYPE(pPropTopRel))
    {
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_TABLE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
            {
                status = clkPropTopRelCache_1X_TABLE(pPropTopRel);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_RATIO:
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VOLT:
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VFE:
        {
            // Nothing to do
            status = FLCN_OK;
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

clkPropTopRelCache_exit:
    return status;
}

/*!
 * CLK_PROP_TOP_RELS implementation.
 *
 * @copydoc ClkPropTopRelsCache
 */
FLCN_STATUS
clkPropTopRelsCache(void)
{
    CLK_PROP_TOP_REL   *pPropTopRel;
    LwBoardObjIdx       idx;
    FLCN_STATUS         status = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_PROP_TOP_REL, pPropTopRel, idx, NULL)
    {
        status = clkPropTopRelCache(pPropTopRel);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopRelsCache_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkPropTopRelsCache_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_PROP_TOP_REL implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkPropTopRelIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROP_TOP_REL_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_CLK_CLK_PROP_TOP_REL_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLK_PROP_TOP_RELS  *pPropTopRels = (CLK_PROP_TOP_RELS *)pBObjGrp;
    FLCN_STATUS         status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkPropTopRelIfaceModel10SetHeader_exit;
    }

    // Copy global CLK_PROP_TOPS state.
    pPropTopRels->tableRelTupleCount = pSet->tableRelTupleCount;

    (void)memcpy(pPropTopRels->tableRelTuple, pSet->tableRelTuple,
        (sizeof(LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE) *
             LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE_MAX));

    (void)memcpy(pPropTopRels->offAdjTableRelTuple, pSet->tableRelTuple,
        (sizeof(LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE) *
             LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TABLE_REL_TUPLE_MAX));

s_clkPropTopRelIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_PROP_TOP_REL implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkPropTopRelIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBoardObjSet->type)
    {
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_RATIO:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
            {
                status = clkPropTopRelGrpIfaceModel10ObjSet_1X_RATIO(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_PROP_TOP_REL_1X_RATIO),
                                  pBoardObjSet);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_TABLE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
            {
                status = clkPropTopRelGrpIfaceModel10ObjSet_1X_TABLE(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_PROP_TOP_REL_1X_TABLE),
                                  pBoardObjSet);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VOLT:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL))
            {
                status = clkPropTopRelGrpIfaceModel10ObjSet_1X_VOLT(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_PROP_TOP_REL_1X_VOLT),
                                  pBoardObjSet);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_PROP_TOP_REL_TYPE_1X_VFE:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP_REL_VFE))
            {
                status = clkPropTopRelGrpIfaceModel10ObjSet_1X_VFE(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_PROP_TOP_REL_1X_VFE),
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
