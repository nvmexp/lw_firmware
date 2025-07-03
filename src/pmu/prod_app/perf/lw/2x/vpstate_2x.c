/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   vpstate_2x.c
 * @brief  Pstate 2.0 and earlier Interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_perf.h"
#include "pmu_objperf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
ct_assert(PERF_DOMAIN_GROUP_MAX_GROUPS == (PERF_DOMAIN_GROUP_GPC2CLK + 1));
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc@ VpstateGetBySemanticIdx
 */
VPSTATE *
vpstateGetBySemanticIdx
(
    RM_PMU_PERF_VPSTATE_IDX vPstateIdx
)
{
    // Make sure the vpstate index is not invalid!
    if (Perf.tables.vPstateTbl.vPstateIdxs[vPstateIdx] !=
            RM_PMU_PERF_VPSTATE_IDX_ILWALID)
    {
        return ((VPSTATE *)&Perf.tables.vPstateTbl.pVPstates[
                Perf.tables.vPstateTbl.vPstateIdxs[vPstateIdx]]);
    }

    return NULL;
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
    RM_PMU_PERF_VPSTATE_INFO   *pInfo   = (RM_PMU_PERF_VPSTATE_INFO *)pVpstate;
    FLCN_STATUS                 status  = FLCN_OK;

    // Check against number of domain groups supported in system.
    if (domGrpIdx >= PERF_DOMAIN_GROUP_MAX_GROUPS)
    {
        *pValue = 0;
        return FLCN_ERR_ILWALID_INDEX;
    }

    *pValue = pInfo->values[domGrpIdx];

    return status;
}

/*!
 * Helper function used by ref@ PerfPstatePackPstateCeilingToDomGrpLimits to
 * get the MAX clock information from legacy VPSTATE code.
 */
FLCN_STATUS
perfVPstatePackPstateCeilingToDomGrpLimits
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    VPSTATE    *pVpstate =
        vpstateGetBySemanticIdx(RM_PMU_PERF_VPSTATE_IDX_MAX_CLOCK);
    if (pVpstate == NULL)
    {
        return FLCN_ERR_ILWALID_INDEX;
    }

    perfVPstatePackVpstateToDomGrpLimits(pVpstate, pDomGrpLimits);

    return FLCN_OK;
}

/*!
 * Helper function used to pack teh vpstate domain group info to the
 * ref@ RM_PMU_PERF_DOMAIN_GROUP_LIMITS struct.
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

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
