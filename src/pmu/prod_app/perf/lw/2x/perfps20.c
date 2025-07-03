/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   perfps20.c
 * @brief  Pstate 2.0 and earlier Interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_perf.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct PERF object associated with Pstates 2.0
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
perfConstructPs20(void)
{
    Perf.status.vPstateLwrrIdx = PERF_VPSTATE_ILWALID_IDX;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_2X)
    memset(&Perf.tables.vPstateTbl.vPstateIdxs, RM_PMU_PERF_VPSTATE_IDX_ILWALID,
            sizeof(Perf.tables.vPstateTbl.vPstateIdxs));
#endif

    return FLCN_OK;
}

/*!
 * Processes RM_PMU_PERF_STATUS objects received from the RM as pstates/domain
 * groups are switched.
 *
 * @param[in]  pStatus  RM_PMU_PERF_STATUS object to process
 *
 * @return 'FLCN_OK'     Upon successful processing of the object
 * @return 'FLCN_ERROR'  Reserved for future error-checking
 */
FLCN_STATUS
perfProcessPerfStatus
(
    RM_PMU_PERF_STATUS *pStatus
)
{
    LwU8 i;

    // Copy in the new status of the domain groups.
    Perf.status = *pStatus;

    // Handle PMGR objects changes when a perfStatus is updated.
    pmgrProcessPerfStatus(pStatus, Perf.status.voltageuV);

    for (i = 0; i < Perf.status.numDomGrps; i++)
    {
        DBG_PRINTF_PERF(("PS: d: %d, v: %d\n",
                         i, Perf.status.domGrps[i].value, 0, 0));
    }

    return FLCN_OK;
}

/*!
 * Process the RM_PMU_PERF_TABLES_INFO object which describes the "perf tables"
 * (the processed RM version of the Perf Table, Clock Range Table, VF tables,
 * and VDT).  This is sent down from the RM when the PMU initially loads, and
 * whenever the the RM mutates the state of the table (i.e. overclocking, VDT
 * voltage recallwlation, etc.).
 *
 * @param[in] pInfo  RM_PMU_PERF_TABLES_INFO object to process.
 *
 * @return FLCN_OK
 *     Object succesfully processed.
 */
FLCN_STATUS
perfProcessTablesInfo
(
    RM_PMU_PERF_TABLES_INFO *pInfo
)
{
    LwU8 i;

    // Allocate the array of Pstates if not already allocated.
    if (Perf.tables.pPstates == NULL)
    {
        Perf.tables.pPstates =
            lwosCallocResidentType(pInfo->numPstates, RM_PMU_PERF_PSTATE_INFO);
        if (Perf.tables.pPstates == NULL)
        {
            return FLCN_ERR_NO_FREE_MEM;
        }
    }
    else
    {
        PMU_HALT_COND(Perf.tables.numPstates == pInfo->numPstates);
    }

    // Copy in the Pstate entries
    Perf.tables.numPstates = pInfo->numPstates;
    Perf.tables.numDomGrpsDecoup = pInfo->numDomGrpsDecoup;
    for (i = 0; i < pInfo->numPstates; i++)
    {
        Perf.tables.pPstates[i].freqKHz = pInfo->pstates[i].freqKHz;
        Perf.tables.pPstates[i].voltageuV = pInfo->pstates[i].voltageuV;

        // Copy in the decoupled domain group info.
        memcpy(Perf.tables.pPstates[i].domGrpsDecoup,
                pInfo->pstates[i].domGrpsDecoup,
                pInfo->numDomGrpsDecoup *
                    sizeof(RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO));

        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_LWRRENT_AWARE))
        {
            Perf.tables.pPstates[i].msDynamicLwrrentCoefficient =
                pInfo->pstates[i].msDynamicLwrrentCoefficient;
        }
    }

    // Allocate the array of VF entries if not already allocated.
    if (Perf.tables.pVfEntries == NULL)
    {
        Perf.tables.pVfEntries =
            lwosCallocResidentType(pInfo->numVfEntries, RM_PMU_PERF_VF_ENTRY_INFO);
        if (Perf.tables.pPstates == NULL)
        {
            return FLCN_ERR_NO_FREE_MEM;
        }
    }
    else
    {
        PMU_HALT_COND(Perf.tables.numVfEntries == pInfo->numVfEntries);
    }

    // Copy in the set of VF entries.
    Perf.tables.numVfEntries = pInfo->numVfEntries;
    memcpy(Perf.tables.pVfEntries, pInfo->vfEntries,
                pInfo->numVfEntries * sizeof(RM_PMU_PERF_VF_ENTRY_INFO));

    // Allocate the array of VPstate entires if not already allocated.
    if ((Perf.tables.vPstateTbl.pVPstates == NULL) &&
        (pInfo->vPstateTbl.numVPstates > 0))
    {
        Perf.tables.vPstateTbl.pVPstates =
            lwosMallocResidentType(pInfo->vPstateTbl.numVPstates,
                                   RM_PMU_PERF_VPSTATE_INFO);
        if (Perf.tables.vPstateTbl.pVPstates == NULL)
        {
            return FLCN_ERR_NO_FREE_MEM;
        }
    }
    else
    {
        PMU_HALT_COND(Perf.tables.vPstateTbl.numVPstates ==
            pInfo->vPstateTbl.numVPstates);
    }

    // Copy in the VPstate Table.
    memcpy(&Perf.tables.vPstateTbl.vPstateIdxs, &pInfo->vPstateTbl.vPstateIdxs,
            sizeof(LwU8) * RM_PMU_PERF_VPSTATE_IDX_MAX_IDXS);
    Perf.tables.vPstateTbl.numVPstates = pInfo->vPstateTbl.numVPstates;
    memcpy(Perf.tables.vPstateTbl.pVPstates, &pInfo->vPstateTbl.vPstates,
            pInfo->vPstateTbl.numVPstates * sizeof(RM_PMU_PERF_VPSTATE_INFO));

    // Copy in the minimum non-PERF voltage.
    Perf.tables.nonPerfMilwoltageuV = pInfo->nonPerfMilwoltageuV;

    DBG_PRINTF_PERF(("TI: p: %d, d: %d, v: %d\n",
                        pInfo->numPstates,
                        pInfo->numDomGrpsDecoup,
                        pInfo->numVfEntries,
                        0));

    return FLCN_OK;
}

/*!
 * Retrieves the latest Domain Group value (pstate index for the Pstate Domain
 * Group, frequency for decoupled Domain Groups) sent from the RM.  This is the
 * current state of the Domain Group.
 *
 * @param[in] domGrpIdx  Index of the requested Domain Group.
 *
 * @return Domain Group value.
 */
LwU32
perfDomGrpGetValue
(
    LwU8 domGrpIdx
)
{
    //
    // Make sure index is valid. The number of available domain groups changes
    // with each GPU per VBIOS specification of pstates 2.0.
    //
    if (domGrpIdx < Perf.status.numDomGrps)
    {
        return Perf.status.domGrps[domGrpIdx].value;
    }

    PMU_HALT();
    return 0;
}

/*!
 * Retrieves the latest Core voltage (uV) sent from the RM.  This is the
 * current Core Voltage of the GPU.
 *
 * @return Core Voltage (uV)
 */
LwU32
perfVoltageuVGet(void)
{
    return Perf.status.voltageuV;
}

/*!
 * Get the pstate count
 *
 * @return The number of valid pstates. Pstates are identified by index and
 *         can range from zero to the pstate count (exclusive).
 */
LwU8
perfGetPstateCount(void)
{
    return Perf.status.numPstates;
}

/*!
 * Accessor for PERF Pstate information.
 *
 * @param[in] pstateIdx Index of requested Pstate.
 *
 * @return Pointer to corresponding RM_PMU_PERF_PSTATE_INFO.
 */
RM_PMU_PERF_PSTATE_INFO *
perfPstateGet
(
    LwU8 pstateIdx
)
{
    if (pstateIdx < Perf.tables.numPstates)
    {
        return &Perf.tables.pPstates[pstateIdx];
    }

    PMU_HALT();
    return NULL;
}

/*!
 * @brief   Accessor for PERF Pstate-specific Domain Group information.
 *
 * @param[in]   pstateIdx   Index of requested Pstate.
 * @param[in]   domGrpIdx   Index of requested Domain Group.
 *
 * @return Pointer to corresponding RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO.
 */
RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *
perfPstateDomGrpGet
(
    LwU8 pstateIdx,
    LwU8 domGrpIdx
)
{
    if ((pstateIdx < Perf.tables.numPstates) &&
        (domGrpIdx != RM_PMU_PERF_DOMAIN_GROUP_PSTATE) &&
        ((domGrpIdx - 1) <  Perf.tables.numDomGrpsDecoup))
    {
        return &Perf.tables.pPstates[pstateIdx].domGrpsDecoup[domGrpIdx - 1];
    }

    PMU_HALT();
    return NULL;
}

/*!
 * Helper function to compute the voltage floor corresponding to a requested
 * pstate.  This will be the lowest voltage allowed (per decoupled clock
 * changes) while in the requested pstate.
 *
 * @note This function also includes handling for any non-pstate/perf voltage
 * limits, such as various chip WARs.
 *
 * @param[in] pstateIdx Index of requested Pstate.
 *
 * @return Voltage floor corresponding to the pstate.  Units of uV.
 */
LwU32
perfPstateMilwoltageuVGet
(
    LwU8 pstateIdx
)
{
    if (pstateIdx < Perf.tables.numPstates)
    {
        return LW_MAX(Perf.tables.pPstates[pstateIdx].voltageuV,
                        Perf.tables.nonPerfMilwoltageuV);
    }

    PMU_HALT();
    return 0;
}

/*!
 * @brief   Accessor for PERF VF entry information.
 *
 * @param[in]   vfEntryIndex    Index of requested VF entry.
 *
 * @return Pointer to corresponding RM_PMU_PERF_VF_ENTRY_INFO structure.
 */
RM_PMU_PERF_VF_ENTRY_INFO *
perfVfEntryGet
(
    LwU8 vfEntryIdx
)
{
    RM_PMU_PERF_VF_ENTRY_INFO *pVfEntryInfo = PERF_VF_ENTRY_GET(vfEntryIdx);

    PMU_HALT_COND(pVfEntryInfo != NULL);
    return pVfEntryInfo;
}

/*!
 * Accessor for PERF VF entry maximum frequency (kHz).  Includes handling for
 * any OC delta values which are applied to the pstate's domain group frequency
 * range.
 *
 * @param[in] pDomGrpInfo
 *     Pointer to RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO structure representing
 *     the pstate's domain group range values which correspond to the given VF
 *     entry.
 * @param[in] pVfEntry
 *     Pointer to RM_PMU_PERF_VF_ENTRY_INFO structure representing the VF entry
 *     to evaluate.
 *
 * @return VF entry's maximum frequency (kHz) with correction for the pstate
 *     domain group's OC delta, if applicable.
 */
LwU32
perfVfEntryGetMaxFreqKHz
(
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pDomGrpInfo,
    RM_PMU_PERF_VF_ENTRY_INFO            *pVfEntry
)
{
    if ((LwS32)pVfEntry->maxFreqKHz > -pDomGrpInfo->maxFreqDeltaKHz)
    {
        return (LwU32)(pVfEntry->maxFreqKHz + pDomGrpInfo->maxFreqDeltaKHz);
    }
    else
    {
        return 0;
    }
}

/*!
 * Accessor for PERF VF entry information.  Will lookup the VF entry index
 * corresponding to a given frequency of a given decoupled domain group in a
 * given pstate.
 *
 * @param[in] pstateIdx Index of requested Pstate.
 * @param[in] domGrpIdx Index of requested Domain Group.
 * @param[in] freqKHz   Frequency for which to find the corresponding VF entry.
 *
 * @return Index of corresponding VF entry in the VBIOS VF Table.
 */
LwU8
perfVfEntryIdxGetForFreq
(
    LwU8  pstateIdx,
    LwU8  domGrpIdx,
    LwU32 freqKHz
)
{
    RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pPstateDomGrp;
    LwU8 vfIdx = 0;

    // Find the corresponding domain group information.
    pPstateDomGrp = perfPstateDomGrpGet(pstateIdx, domGrpIdx);
    if (pPstateDomGrp != NULL)
    {
        //
        // Search through the the set of VF entries (lowest to highest) to find
        // the first entry with a maximum frequency >= than the requested
        // frequency.
        //
        for (vfIdx = pPstateDomGrp->vfIdxFirst;
                vfIdx < pPstateDomGrp->vfIdxLast; vfIdx++)
        {
            if (perfVfEntryGetMaxFreqKHz(pPstateDomGrp,
                    &Perf.tables.pVfEntries[vfIdx]) >= freqKHz)
            {
                break;
            }
        }
    }

    return vfIdx;
}

/*!
 * @copydoc@ PerfPstatePackPstateCeilingToDomGrpLimits
 */
FLCN_STATUS
perfPstatePackPstateCeilingToDomGrpLimits
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    // Use the legacy VPSTATE helper fuction to get the MAX PSTATE info
    return perfVPstatePackPstateCeilingToDomGrpLimits(pDomGrpLimits);
}

/*!
 * PSTATE object doesn't exist on the PMU for _2X, so we just fail here. We need this
 * implemented because it's used in a macro in pstate.h
 *
 * @copydoc PerfPstateGetByLevel
 */
PSTATE *
perfPstateGetByLevel
(
    LwU8 level
)
{
    PMU_BREAKPOINT();
    return NULL;
}

/* ------------------------- Private Functions ------------------------------ */
