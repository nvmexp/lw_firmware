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
 * @file clk_prop_top.c
 *
 * @brief Module managing all state related to the CLK_PROP_TOP structure.
 *
 * @ref - https://confluence.lwpu.com/display/RMPER/Clock+Propagation
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetEntry  (s_clkPropTopIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkPropTopIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10SetHeader (s_clkPropTopIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkPropTopIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10GetStatusHeader (s_clkPropTopIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "s_clkPropTopIfaceModel10GetStatusHeader");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP_TOP handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkPropTopBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    LwBool      bFirstTime = (Clk.propTops.super.super.objSlots == 0U);

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
        CLK_PROP_TOP,                               // _class
        pBuffer,                                    // _pBuffer
        s_clkPropTopIfaceModel10SetHeader,                // _hdrFunc
        s_clkPropTopIfaceModel10SetEntry,                 // _entryFunc
        ga10xAndLater.clk.clkPropTopGrpSet,         // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopBoardObjGrpIfaceModel10Set_exit;
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
                goto clkPropTopBoardObjGrpIfaceModel10Set_exit;
            }
        }
    }

clkPropTopBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * CLK_PROP_TOP handler for the RM_PMU_BOARDOBJ_CMD_GRP GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkPropTopBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,     // _grpType
        CLK_PROP_TOP,                               // _class
        pBuffer,                                    // _bu
        s_clkPropTopIfaceModel10GetStatusHeader,                // _hf
        boardObjIfaceModel10GetStatus,                              // _ef
        ga10xAndLater.clk.clkPropTopGrpGetStatus);  // _sse
}

/*!
 * CLK_PROP_TOP class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropTopGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJ_SET *pPropTopSet =
        (RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP_TOP   *pPropTop;
    FLCN_STATUS     status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pPropTop = (CLK_PROP_TOP *)*ppBoardObj;

    // Copy the CLK_PROP_TOP-specific data.
    pPropTop->topId          = pPropTopSet->topId;
    pPropTop->domainsDstPath = pPropTopSet->domainsDstPath;

    // Init and copy in the mask.
    boardObjGrpMaskInit_E255(&(pPropTop->clkPropTopRelMask));
    status = boardObjGrpMaskImport_E255(&(pPropTop->clkPropTopRelMask),
                                        &(pPropTopSet->clkPropTopRelMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkPropTopGrpIfaceModel10ObjSet_SUPER_exit;
    }

clkPropTopGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc ClkPropTopsGetTopologyIdxFromId
 */
LwBoardObjIdx
clkPropTopsGetTopologyIdxFromId
(
    LwU8 clkPropTopId
)
{
    CLK_PROP_TOP *pPropTop;
    LwBoardObjIdx idx;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_PROP_TOP, pPropTop, idx, NULL)
    {
        if (pPropTop->topId == clkPropTopId)
        {
            return BOARDOBJ_GET_GRP_IDX(&pPropTop->super);
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID;
}

/*!
 * CLK_PROP_TOPS implementation.
 *
 * @copydoc ClkPropTopsSetActiveTopologyForced
 */
FLCN_STATUS
clkPropTopsSetActiveTopologyForced
(
    CLK_PROP_TOPS   *pPropTops,
    LwU8             topId
)
{
    LwBool      bIlwalidationReq;
    FLCN_STATUS status  = FLCN_OK;

    // Check whether we need to trigger ilwalidation.
    bIlwalidationReq = (pPropTops->activeTopIdForced != topId);

    // Update the forced topology.
    pPropTops->activeTopIdForced = topId;

    // Trigger ilwalidation if required.
    if (bIlwalidationReq)
    {
        status = perfLimitsIlwalidate(PERF_PERF_LIMITS_GET(),
                    LW_TRUE,
                    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopsSetActiveTopologyForced_exit;
        }
    }

clkPropTopsSetActiveTopologyForced_exit:
    return FLCN_OK;
}

/*!
 * CLK_PROP_TOP implementation.
 *
 * @copydoc ClkPropTopFreqPropagate
 */
FLCN_STATUS
clkPropTopFreqPropagate
(
    CLK_PROP_TOP       *pPropTop,
    CLK_DOMAIN         *pDomainSrc,
    CLK_DOMAIN         *pDomainDst,
    LwU16              *pFreqMHz
)
{
    CLK_PROP_TOP_REL   *pPropTopRel     = NULL;
    LwU8                clkDomainIdxSrc;
    LwU8                clkDomainIdxDst;
    LwU8                propTopRelIdx;
    LwBool              bSrcToDstProp;
    FLCN_STATUS         status          = FLCN_OK;

    // Init the src and dst clock domain index.
    clkDomainIdxSrc = BOARDOBJ_GET_GRP_IDX_8BIT(&pDomainSrc->super.super);
    clkDomainIdxDst = BOARDOBJ_GET_GRP_IDX_8BIT(&pDomainDst->super.super);

    PMU_HALT_OK_OR_GOTO(status,
        LW2080_CTRL_CLK_CLK_PROP_TOP_CLK_DOMAIN_DST_PATH_IDX_VALID(clkDomainIdxDst) ?
            FLCN_OK : FLCN_ERR_ILWALID_INDEX,
        clkPropTopFreqPropagate_exit);

    // PP-TODO : Temporary usecase until we add more production use cases.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_40_TEST))
    {
        CLK_PROP_REGIME *pPropRegime = clkPropRegimesGetByRegimeId(LW2080_CTRL_CLK_CLK_PROP_REGIME_ID_DRAM_STRICT);
        if (pPropRegime == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkPropTopFreqPropagate_exit;
        }
    }

    // Sanity check
    if ((pFreqMHz == NULL) ||
        ((*pFreqMHz) == LW2080_CTRL_CLK_VF_VALUE_ILWALID))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkPropTopFreqPropagate_exit;
    }

    // Perform frequency propagation following the POR topology graph.
    while (clkDomainIdxDst != clkDomainIdxSrc)
    {
        PMU_HALT_OK_OR_GOTO(status,
            LW2080_CTRL_CLK_CLK_PROP_TOP_CLK_DOMAINS_DST_PATH_IDX_VALID(clkDomainIdxSrc) ?
                FLCN_OK : FLCN_ERR_ILWALID_INDEX,
            clkPropTopFreqPropagate_exit);

        // Get the clock propagation topology relationship index.
        propTopRelIdx = pPropTop->domainsDstPath.domainDstPath[clkDomainIdxSrc].dstPath[clkDomainIdxDst];

        pPropTopRel = CLK_PROP_TOP_REL_GET(propTopRelIdx);
        if (pPropTopRel == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkPropTopFreqPropagate_exit;
        }

        // Check whether we need ilwerse callwlation (dst -> src) from this relationship.
        bSrcToDstProp = (clkDomainIdxSrc == clkPropTopRelClkDomainIdxSrcGet(pPropTopRel));

        // Propagate the frequency from intermediate src to intermediate dst.
        status = clkPropTopRelFreqPropagate(pPropTopRel, bSrcToDstProp, pFreqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopFreqPropagate_exit;
        }

        // If the F -> V -> F is not possible due to very low V, short circuit.
        if ((*pFreqMHz) == LW2080_CTRL_CLK_VF_VALUE_ILWALID)
        {
            // This is not an error. Client (Arbiter) will set the default values.
            status = FLCN_OK;
            goto clkPropTopFreqPropagate_exit;
        }

        // Update new intermediate src clock domain index to intermediate dst.
        if (bSrcToDstProp)
        {
            clkDomainIdxSrc = clkPropTopRelClkDomainIdxDstGet(pPropTopRel);
        }
        else
        {
            clkDomainIdxSrc = clkPropTopRelClkDomainIdxSrcGet(pPropTopRel);
        }
    }

clkPropTopFreqPropagate_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_PROP_TOP implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkPropTopIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLK_PROP_TOPS *pPropTops = (CLK_PROP_TOPS *)pBObjGrp;
    FLCN_STATUS    status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkPropTopIfaceModel10SetHeader_exit;
    }

    // Copy global CLK_PROP_TOPS state.
    pPropTops->topHal            = pSet->topHal;
    pPropTops->activeTopId       = pSet->activeTopId;
    pPropTops->activeTopIdForced = pSet->activeTopIdForced;

s_clkPropTopIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_PROP_TOP implementation to parse BOARDOBJ entry.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkPropTopIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBoardObjSet->type)
    {
        case LW2080_CTRL_CLK_CLK_PROP_TOP_TYPE_1X:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_PROP_TOP))
            {
                status = clkPropTopGrpIfaceModel10ObjSet_SUPER(pModel10,
                                  ppBoardObj,
                                  sizeof(CLK_PROP_TOP),
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

/*!
 * CLK_PROP_TOP implementation
 *
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_clkPropTopIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJGRP_GET_STATUS_HEADER *pPropTopsStatus =
        (RM_PMU_CLK_CLK_PROP_TOP_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    CLK_PROP_TOPS *pPropTops = (CLK_PROP_TOPS *)pBoardObjGrp;

    // Copy out group-specific header data.
    pPropTopsStatus->activeTopId = clkPropTopsGetActiveTopologyId(pPropTops);

    return FLCN_OK;
}

