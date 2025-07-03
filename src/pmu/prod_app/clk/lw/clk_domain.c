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
 * @file clk_domain.c
 *
 * @brief Module managing all state related to the CLK_DOMAIN structure.
 * This structure defines the set of clocks that the RM will manage and any
 * state related to any particular management schemes, per the VBIOS
 * specification.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "clk/clk_clockmon.h"
#include "volt/objvolt.h"
#include "pmu_objperf.h"
#include "clk/clk_domain_pmumon.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_clkDomainIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkDomainIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_clkDomainIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "s_clkDomainIfaceModel10SetEntry");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_DOMAIN handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
clkDomainBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    BOARDOBJGRP *pDomains   = (BOARDOBJGRP *)BOARDOBJGRP_GRP_GET(CLK_DOMAIN);
    FLCN_STATUS  status;
    LwBool       bFirstTime = BOARDOBJGRP_IS_EMPTY(CLK_DOMAIN);

    // Increment the VF Points cache counter due to change in CLK_DOMAINs params
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT))
    {
        clkVfPointsVfPointsCacheCounterIncrement();
    }

    status = BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,    // _grpType
        CLK_DOMAIN,                                 // _class
        pBuffer,                                    // _pBuffer
        s_clkDomainIfaceModel10SetHeader,                 // _hdrFunc
        s_clkDomainIfaceModel10SetEntry,                  // _entryFunc
        pascalAndLater.clk.clkDomainGrpSet,         // _ssElement
        OVL_INDEX_DMEM(perf),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto _clkDomainBoardObjGrpSet_exit;
    }

    //
    // For PSTATES 3.0, run any general handlers which are required for
    // CLK_DOMAIN.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_30) &&
        bFirstTime)
    {
        status = perfClkDomainsBoardObjGrpSet_30(pDomains);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto _clkDomainBoardObjGrpSet_exit;
        }
    }

    //
    // On version 35, Ilwalidate the CLK DOMAINs on SET CONTROL.
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
            goto _clkDomainBoardObjGrpSet_exit;
        }
    }

_clkDomainBoardObjGrpSet_exit:
    return status;
}

/*!
 * CLK_DOMAIN super-class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkDomainGrpIfaceModel10ObjSet_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET *)pBoardObjDesc;
    CLK_DOMAIN *pDomain;
    FLCN_STATUS status;

    //
    // Call into BOARDOBJ super-constructor, which will do actual memory
    // allocation.
    //
    status = boardObjVtableGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainGrpIfaceModel10ObjSet_SUPER_exit;
    }
    pDomain = (CLK_DOMAIN *)*ppBoardObj;

    // Copy the CLK_DOMAIN-specific data.
    pDomain->domain           = pSet->domain;
    pDomain->apiDomain        = pSet->apiDomain;
    pDomain->perfDomainGrpIdx = pSet->perfDomainGrpIdx;

clkDomainGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsGetIdxByDomainGrpIdx
 */
LwU8
clkDomainsGetIdxByDomainGrpIdx
(
    LwU8 domainGrpIdx
)
{
    CLK_DOMAIN   *pDomain = NULL;
    LwBoardObjIdx i;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &(CLK_CLK_DOMAINS_GET()->progDomainsMask.super))
    {
        if (pDomain->perfDomainGrpIdx == domainGrpIdx)
        {
            return BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain->super.super);
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return LW_U8_MAX;
}

/*!
 * @copydoc ClkDomainsGetByDomain
 */
CLK_DOMAIN *
clkDomainsGetByDomain
(
    RM_PMU_CLK_CLKWHICH domain
)
{
    CLK_DOMAIN   *pDomain = NULL;
    LwBoardObjIdx i;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i, NULL)
    {
        if (pDomain->domain == domain)
        {
            return pDomain;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return NULL;
}

/*!
 * @copydoc ClkDomainsGetByApiDomain
 */
CLK_DOMAIN *
clkDomainsGetByApiDomain
(
    LwU32 apiDomain
)
{
    CLK_DOMAIN   *pDomain = NULL;
    LwBoardObjIdx i;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i, NULL)
    {
        if (pDomain->apiDomain == apiDomain)
        {
            return pDomain;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return NULL;
}

/*!
 * @copydoc ClkDomainsGetIndexByApiDomain
 */
FLCN_STATUS
clkDomainsGetIndexByApiDomain
(
    LwU32 apiDomain,
    LwU32 *pIndex
)
{
    CLK_DOMAIN   *pDomain = NULL;
    LwBoardObjIdx i;
    FLCN_STATUS   status  = FLCN_ERR_ILWALID_INDEX;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i, NULL)
    {
        if (pDomain->apiDomain == apiDomain)
        {
            *pIndex = i;
            status  = FLCN_OK;
            goto clkDomainsGetIndexByApiDomain_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

clkDomainsGetIndexByApiDomain_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsCache
 */
FLCN_STATUS
clkDomainsCache
(
    LwBool bVFEEvalRequired
)
{
    FLCN_STATUS status;

    //
    // Iterate over the _3X_PRIMARY objects to cache their PRIMARY VF lwrves.
    // On VF 3.5+, we are also caching their CLK_PROG @ref offsettedFreqMaxMHz
    //
    if ((PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_30)) ||
        (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35)))
    {
        status = clkDomainsCache_3X(bVFEEvalRequired);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
    {
        status = clkDomainsCache_40(bVFEEvalRequired);
    }
    else
    {
        status = FLCN_OK;
    }

    return status;
}

/*!
 * @copydoc ClkDomainsCache
 */
FLCN_STATUS
clkDomainsOffsetVFCache(void)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
    {
        status = clkDomainsOffsetVFCache_40();
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @copydoc ClkDomainsBaseVFCache
 */
FLCN_STATUS
clkDomainsBaseVFCache
(
    LwU8 cacheIdx
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Validate the cache index.
    if (cacheIdx == CLK_CLK_VF_POINT_VF_CACHE_IDX_ILWALID)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto clkDomainsBaseVFCache_exit;
    }

    // Ilwalidate all VF lwrves of this cache index.
    clkVfPointsVfCacheIsValidClear(
        LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI, cacheIdx);
    clkVfPointsVfCacheIsValidClear(
        LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0, cacheIdx);

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
    {
        status = clkDomainsBaseVFCache_40(cacheIdx);
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkDomainsBaseVFCache_exit;
    }

    // Mark VF lwrves for this cache index as valid.
    clkVfPointsVfCacheIsValidSet(
        LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI, cacheIdx);
    clkVfPointsVfCacheIsValidSet(
        LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0, cacheIdx);

clkDomainsBaseVFCache_exit:
    return status;
}

/*!
 * @copydoc ClkDomainsLoad
 */
FLCN_STATUS
clkDomainsLoad(void)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx i;
    FLCN_STATUS   status   = FLCN_OK;

    // Iterate over the CLK_DOMAIN objects to load their VF lwrves.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &Clk.domains.progDomainsMask.super)
    {
        status = clkDomainLoad(pDomain);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkDomainsLoad_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAINS_PMUMON))
    {
        status = clkClkDomainsPmumonLoad();
        if (status != FLCN_OK)
        {
            goto clkDomainsLoad_exit;
        }
    }

    // Set the CLK_DOMAINS::bLoaded flag.
    Clk.domains.bLoaded = LW_TRUE;

clkDomainsLoad_exit:
    return status;
}

/*!
 * @brief Helper interface to sanity check the input clock list for a given
 * perf change request.
 *
 * @param[in]   pClkList            LW2080_CTRL_CLK_CLK_DOMAIN_LIST pointer
 * @param[in]   pVoltList           LW2080_CTRL_VOLT_VOLT_RAIL_LIST pointer
 * @param[in]   bVfPointCheckIgnore Request to skip the VF check
 *
 * @return FLCN_OK if the sanity check passed.
 * @return FLCN_ERR_ILWALID_STATE if the sanity check failed.
 */
FLCN_STATUS
clkDomainsClkListSanityCheck
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST    *pClkList,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST    *pVoltList,
    LwBool                              bVfPointCheckIgnore
)
{
    CLK_DOMAINS  *pDomains = CLK_CLK_DOMAINS_GET();
    VOLT_RAIL    *pRail;
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx idx;
    LwBoardObjIdx railIdx;
    FLCN_STATUS   status  = FLCN_OK;

    //
    // Sanity check that @ref CLK_DOMAINS::progDomainsMask is propery
    // set to non-zero.  Otherwise, this function cannot check
    // anything.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        (!boardObjGrpMaskIsZero(&pDomains->progDomainsMask) ?
            FLCN_OK : FLCN_ERR_ILWALID_STATE),
        clkDomainsClkListSanityCheck_exit);

    // Iterate over all the voltrails.
    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, railIdx, NULL)
    {
        LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pVoltListItem =
            &pVoltList->rails[railIdx];
        BOARDOBJGRPMASK_E32  clkDomainsProgMask;
        BOARDOBJGRPMASK_E32 *pRailClkDomainsProgMask;

        //
        // Sanity check that the _VOLT_RAIL_LIST_ITEM's value is
        // properly set.
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            (((pVoltListItem->railIdx != railIdx) ||
              (pVoltListItem->voltageuV == 0U)) ? FLCN_ERR_ILWALID_STATE : FLCN_OK),
            clkDomainsClkListSanityCheck_exit);

        //
        // Build the mask of CLK_DOMAINs to check as the intersection
        // of @ref CLK_DOMAINS::progDomainsMask and the @ref
        // VOLT_RAIL::clkDomainsProgMask, if applicable/available.
        //
        boardObjGrpMaskInit_E32(&clkDomainsProgMask);
        if ((pRailClkDomainsProgMask = voltRailGetClkDomainsProgMask(pRail))
                != NULL)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                boardObjGrpMaskAnd(&clkDomainsProgMask, &pDomains->progDomainsMask,
                    pRailClkDomainsProgMask),
                clkDomainsClkListSanityCheck_exit);
        }
        else
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                boardObjGrpMaskCopy(&clkDomainsProgMask, &pDomains->progDomainsMask),
                clkDomainsClkListSanityCheck_exit);
        }

        // Sanity check each clock domain entry.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, idx,
            &clkDomainsProgMask.super)
        {
            LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM   *pClkListItem =
                &pClkList->clkDomains[idx];

            if ((pClkListItem->clkDomain != clkDomainApiDomainGet(pDomain)) ||
                (pClkListItem->clkFreqKHz == 0U))
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto clkDomainsClkListSanityCheck_exit;
            }

            // Skip this check if explicitely requested through regkey / RMCTRL.
            if (!bVfPointCheckIgnore)
            {
                //
                // Sanity check the noise aware frequency request. We want to make
                // the sure that the frequency selected is not greater than the Fmax
                // at target logic voltage
                //
                CLK_DOMAIN_PROG            *pDomainProg;
                LW2080_CTRL_CLK_VF_INPUT    input;
                LW2080_CTRL_CLK_VF_OUTPUT   output;

                pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
                if (pDomainProg == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto clkDomainsClkListSanityCheck_exit;
                }

                // Initialize the input params
                LW2080_CTRL_CLK_VF_INPUT_INIT(&input);
                LW2080_CTRL_CLK_VF_OUTPUT_INIT(&output);
                input.value   = pVoltListItem->voltageuV;

                status = clkDomainProgVoltToFreq(
                            pDomainProg,                        // pDomainProg
                            BOARDOBJ_GRP_IDX_TO_8BIT(railIdx),  // voltRailIdx
                            LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,   // voltageType
                            &input,                             // pInput
                            &output,                            // pOutput
                            NULL);                              // pVfIterState
                if ((status != FLCN_OK) ||
                    (LW2080_CTRL_CLK_VF_VALUE_ILWALID == output.inputBestMatch))
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto clkDomainsClkListSanityCheck_exit;
                }

                //
                // The maxValue is in MHz while the targetFreqKHz is in KHz.
                // Scale all the clocks by 1000, except the PCI-E genspeed
                //
                if (pDomain->domain != clkWhich_PCIEGenClk)
                {
                    output.value *= 1000U;
                }

                //
                // Ensure that the target frequency is NOT greater than the max supported.
                //
                if (pClkListItem->clkFreqKHz > output.value)
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto clkDomainsClkListSanityCheck_exit;
                }
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    BOARDOBJGRP_ITERATOR_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLOCK_MON_FAULT_STATUS_CHECK) &&
        clkDomainsIsClkMonEnabled())
    {
        // Attach overlays
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClockMon)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Sanity check each clock domain fault status
            status = clkClockMonSanityCheckFaultStatusAll();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

clkDomainsClkListSanityCheck_exit:
    return status;
}

/*!
 * @copydoc ClkDomainGetSource
 */
LwU8
clkDomainGetSource
(
    CLK_DOMAIN *pDomain,
    LwU32       freqMHz
)
{
    LwU8 source = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID;

    // Call type-specific implementation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_PROG:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
            {
                source = clkDomainGetSource_3X_PROG(pDomain, freqMHz);
            }
            break;
        }
        default:
        {
            // Nothing to do
            break;
        }
    }
    if (source == LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID)
    {
        PMU_BREAKPOINT();
    }

    return source;
}

/*!
 * @copydoc ClkDomainLoad
 */
FLCN_STATUS
clkDomainLoad_IMPL
(
    CLK_DOMAIN *pDomain
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call type-specific implementation.
    switch (BOARDOBJ_GET_TYPE(pDomain))
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))
            {
                status = clkDomainLoad_35_Primary(pDomain);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainLoad_40_PROG(pDomain);
            }
            break;
        }
        default:
        {
            // Return OK on prefiles that does not required to be loaded.
            status = FLCN_OK;
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
 * @copydoc ClkDomainIsFixed
 */
LwBool
clkDomainIsFixed_IMPL
(
    CLK_DOMAIN *pDomain
)
{
    if ((pDomain != NULL) &&
        (LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED == BOARDOBJ_GET_TYPE(pDomain)))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * CLK_DOMAIN implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_clkDomainIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_CLK_CLK_DOMAIN_BOARDOBJGRP_SET_HEADER *pSet =
        (RM_PMU_CLK_CLK_DOMAIN_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    CLK_DOMAINS *pDomains = (CLK_DOMAINS *)pBObjGrp;
    FLCN_STATUS  status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkDomainIfaceModel10SetHeader_exit;
    }

    // Sanity check the maximum number of Voltage rails before copy it
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pSet->voltRailsMax > LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto s_clkDomainIfaceModel10SetHeader_exit;
        }
    }

    // Copy global CLK_DOMAINS state.
    pDomains->bLoaded                = LW_FALSE;
    pDomains->version                = pSet->version;
    pDomains->cntrSamplingPeriodms   = pSet->cntrSamplingPeriodms;
    pDomains->bOverrideOVOC          = pSet->bOverrideOVOC;
    pDomains->bDebugMode             = pSet->bDebugMode;
    pDomains->bEnforceVfMonotonicity = pSet->bEnforceVfMonotonicity;
    pDomains->bEnforceVfSmoothening  = pSet->bEnforceVfSmoothening;
    pDomains->bGrdFreqOCEnabled      = pSet->bGrdFreqOCEnabled;
    pDomains->voltRailsMax           = pSet->voltRailsMax;
    pDomains->deltas.freqDelta       = pSet->deltas.freqDelta;

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLOCK_MON)
    pDomains->bClkMonEnabled         = pSet->bClkMonEnabled;
    pDomains->clkMonRefWinUsec       = pSet->clkMonRefWinUsec;
#endif // PMUCFG_FEATURE_ENABLED(PMU_CLK_CLOCK_MON)

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
    pDomains->xbarBoostVfeIdx        = pSet->xbarBoostVfeIdx;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)

    // Sanity check the feature enable to ensure RM - PMU are in sync.
    PMU_HALT_COND(((clkDomainsVersionGet() == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_40) &&
                    (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40)))      ||
               ((clkDomainsVersionGet() == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_35) &&
                    (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))) ||
               ((clkDomainsVersionGet() == LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_30) &&
                    (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_30))));

    // Init and copy in the mask.
    boardObjGrpMaskInit_E32(&(pDomains->progDomainsMask));
    status = boardObjGrpMaskImport_E32(&(pDomains->progDomainsMask),
                                       &(pSet->progDomainsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkDomainIfaceModel10SetHeader_exit;
    }

    boardObjGrpMaskInit_E32(&(pDomains->clkMonDomainsMask));
    status = boardObjGrpMaskImport_E32(&(pDomains->clkMonDomainsMask),
                                       &(pSet->clkMonDomainsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_clkDomainIfaceModel10SetHeader_exit;
    }

    // Allocate the array of size equal to max voltage rails
    if (pDomains->deltas.pVoltDeltauV == NULL)
    {
        pDomains->deltas.pVoltDeltauV =
            lwosCallocType(pBObjGrp->ovlIdxDmem, pDomains->voltRailsMax, LwS32);
        if (pDomains->deltas.pVoltDeltauV == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto s_clkDomainIfaceModel10SetHeader_exit;
        }
    }
    memcpy(pDomains->deltas.pVoltDeltauV, pSet->deltas.voltDeltauV, (sizeof(LwS32) * pDomains->voltRailsMax));

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_CLIENT_INDEX_MAP)
    memcpy(pDomains->clientToInternalIdxMap,
           pSet->clientToInternalIdxMap,
           sizeof(pDomains->clientToInternalIdxMap));
#endif

s_clkDomainIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * CLK_DOMAIN implementation to parse BOARDOBJ entry and construct
 * a CLK_DOMAIN of the corresponding type.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_clkDomainIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE))
            {
                status = clkDomainGrpIfaceModel10ObjSet_3X_FIXED(pModel10, ppBoardObj,
                    sizeof(CLK_DOMAIN_3X_FIXED), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_PRIMARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_30))
            {
                status = clkDomainGrpIfaceModel10ObjSet_30_PRIMARY(pModel10, ppBoardObj,
                    sizeof(CLK_DOMAIN_30_PRIMARY), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_30_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_30))
            {
                status = clkDomainGrpIfaceModel10ObjSet_30_SECONDARY(pModel10, ppBoardObj,
                    sizeof(CLK_DOMAIN_30_SECONDARY), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))
            {
                status = clkDomainGrpIfaceModel10ObjSet_35_PRIMARY(pModel10, ppBoardObj,
                    sizeof(CLK_DOMAIN_35_PRIMARY), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_BASE_35))
            {
                status = clkDomainGrpIfaceModel10ObjSet_35_SECONDARY(pModel10, ppBoardObj,
                    sizeof(CLK_DOMAIN_35_SECONDARY), pBuf);
            }
            break;
        }
        case LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_40_PROG:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_40))
            {
                status = clkDomainGrpIfaceModel10ObjSet_40_PROG(pModel10, ppBoardObj,
                    sizeof(CLK_DOMAIN_40_PROG), pBuf);
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
