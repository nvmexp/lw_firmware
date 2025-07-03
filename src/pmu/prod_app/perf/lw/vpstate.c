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
 * @file    vpstate.c
 * @copydoc vpstate.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/vpstate.h"
#include "pmu_objperf.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader   (s_vpstateIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libVpstateConstruct", "s_vpstateIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry    (s_vpstateIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libVpstateConstruct", "s_vpstateIfaceModel10SetEntry");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
vpstateBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVpstateConstruct)
    };
    LwBool bFirstTime = (Perf.vpstates.super.super.objSlots == 0U);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E255,            // _grpType
            VPSTATE,                                    // _class
            pBuffer,                                    // _pBuffer
            s_vpstateIfaceModel10SetHeader,                   // _hdrFunc
            s_vpstateIfaceModel10SetEntry,                    // _entryFunc
            pascalAndLater.perf.vpstateGrpSet,          // _ssOffset
            OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto vpstateBoardObjGrpIfaceModel10Set_exit;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_INFRA))
        {
            if (!bFirstTime)
            {
                OSTASK_OVL_DESC ovlDescListIlwalidate[] = {
                    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListIlwalidate);
                status = vpstatesIlwalidate();
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListIlwalidate);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto vpstateBoardObjGrpIfaceModel10Set_exit;
                }
            }
        }
vpstateBoardObjGrpIfaceModel10Set_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
vpstateGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    return boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
}

/*!
 * @copydoc@ VpstateGetBySemanticIdx
 */
VPSTATE *
vpstateGetBySemanticIdx
(
    RM_PMU_PERF_VPSTATE_IDX vPstateIdx
)
{
    return BOARDOBJGRP_OBJ_GET(VPSTATE, Perf.vpstates.vPstateIdxs[vPstateIdx]);
}

/*!
 * @copydoc@ VpstateDomGrpGet
 */
FLCN_STATUS
vpstateDomGrpGet
(
    VPSTATE    *pVpstate,
    LwU8        domGrpIdx,
    LwU32      *pValue
)
{
    FLCN_STATUS status = FLCN_ERR_ILWALID_INDEX;

    if (domGrpIdx >= PERF_DOMAIN_GROUP_MAX_GROUPS)
    {
        PMU_BREAKPOINT();
        return status;
    }

    // Call into type-specific implementations
    switch (BOARDOBJ_GET_TYPE(pVpstate))
    {
        case LW2080_CTRL_PERF_VPSTATE_TYPE_3X:
        {
            status = vpstateDomGrpGet_3X(pVpstate, domGrpIdx, pValue);
        }
    }

    return status;
}

/*!
 * Helper function used to pack teh vpstate domain group info to the
 * ref@ PERF_DOMAIN_GROUP_LIMITS struct.
 *
 * @param[in]   pVpstate        Pointer to VPSTATE struct.
 * @param[out]  pDomGrpLimits   Pointer to RM_PMU_PERF_DOMAIN_GROUP_LIMITS struct.
 */
FLCN_STATUS
perfVPstatePackVpstateToDomGrpLimits
(
    VPSTATE    *pVpstate,
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    LwU8        i;
    LwU32       value;
    FLCN_STATUS status;

    for (i = 0; i < PERF_DOMAIN_GROUP_MAX_GROUPS; i++)
    {
        status = vpstateDomGrpGet(pVpstate, i, &value);
        if (status != FLCN_OK)
        {
            return status;
        }
        pDomGrpLimits->values[i] = value;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR) &&
        (pDomGrpLimits->values[PERF_DOMAIN_GROUP_XBARCLK] == 0) &&
        (pDomGrpLimits->values[PERF_DOMAIN_GROUP_GPC2CLK] != 0U))
    {
        PSTATE                   *pPstate;
        PERF_LIMITS_PSTATE_RANGE  pstateRange;
        PERF_LIMITS_VF_ARRAY      vfArrayIn;
        PERF_LIMITS_VF_ARRAY      vfArrayOut;
        BOARDOBJGRPMASK_E32       primaryClkDomainMask;
        BOARDOBJGRPMASK_E32       secondaryClkDomainMask;
        LwU32                     primaryClkDomIdx;
        LwU32                     secondaryClkDomIdx;

        // Init the primary and secondary clock domain masks
        boardObjGrpMaskInit_E32(&primaryClkDomainMask);
        boardObjGrpMaskInit_E32(&secondaryClkDomainMask);

        // Init the input/output struct.
        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
        PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut);

        // Get CLK_DOMAIN index associated with API domain.
        status = clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_GPCCLK,
                                               (&primaryClkDomIdx));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return status;
        }

        status = clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_XBARCLK,
                                     (&secondaryClkDomIdx));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return status;
        }

        pPstate = PSTATE_GET_BY_LEVEL(perfGetPstateCount() - 1);
        if (pPstate == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NOT_SUPPORTED;
        }
        PERF_LIMITS_RANGE_SET_VALUE(&pstateRange,
            BOARDOBJ_GET_GRP_IDX(&pPstate->super));

        boardObjGrpMaskBitSet(&primaryClkDomainMask, primaryClkDomIdx);
        vfArrayIn.pMask = &primaryClkDomainMask; 
        vfArrayIn.values[primaryClkDomIdx] =
        pDomGrpLimits->values[PERF_DOMAIN_GROUP_GPC2CLK];

        // populate vfarraymask.out
        boardObjGrpMaskBitSet(&secondaryClkDomainMask, secondaryClkDomIdx);
            vfArrayOut.pMask = &secondaryClkDomainMask;

        status = perfLimitsFreqPropagate(&pstateRange,
                                &vfArrayIn,
                                &vfArrayOut,
                                LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID);
        if (status != FLCN_OK)
        {
            return status;
        }

        pDomGrpLimits->values[PERF_DOMAIN_GROUP_XBARCLK] =
            vfArrayOut.values[secondaryClkDomIdx];
    }

    return FLCN_OK;
}

/*!
 * @copydoc     PerfVpstatesIlwalidate()
 *
 * @memberof    VPSTATES
 *
 * @public
 */
FLCN_STATUS
vpstatesIlwalidate
(
    void
)
{
    FLCN_STATUS status = FLCN_OK;
 
    // Ilwalidate PMU arbiter if supported.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
    {
        //
        // Detach VFE IMEM and DMEM as it's not required anymore
        // and it's not possible to accommidate PERF, PERF_LIMIT,
        // and VFE DMEM/IMEM at the same time.
        //
        OSTASK_OVL_DESC ovlDescList[] = {
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
            goto vpstatesIlwalidate_exit;
        }
    }

vpstatesIlwalidate_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * PERF_VPSTATE implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_vpstateIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_VPSTATE_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_PERF_VPSTATE_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    VPSTATES   *pVpstates = (VPSTATES *)pBObjGrp;
    FLCN_STATUS status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_vpstateIfaceModel10SetHeader_exit;
    }

    // Copy global VPSTATES state.
    pVpstates->nDomainGroups = pSet->nDomainGroups;

    memcpy(pVpstates->vPstateIdxs, pSet->vPstateIdxs,
            sizeof(LwU8) * RM_PMU_PERF_VPSTATE_IDX_MAX_IDXS);


s_vpstateIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_vpstateIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_VPSTATE_TYPE_2X:
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NOT_SUPPORTED;
        }
        case LW2080_CTRL_PERF_VPSTATE_TYPE_3X:
        {
            return vpstateGrpIfaceModel10ObjSetImpl_3X(pModel10, ppBoardObj,
                        sizeof(VPSTATE_3X), pBuf);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
