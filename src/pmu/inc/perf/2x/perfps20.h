/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PERFPS20_H
#define PERFPS20_H

/*!
 * @file    perfps20.h
 * @copydoc perfps20.c
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "lib/lib_perf.h"
/* ------------------------ Types Definitions ------------------------------ */

/*!
 * Structure representing the VBIOS Virtual Pstate Table.
 */
typedef struct
{
    /*!
     * Array of semantic VPstate indexes indexed by VPstate name.  These are the
     * indexes as specified in the vpstate table header.
     */
    LwU8 vPstateIdxs[RM_PMU_PERF_VPSTATE_IDX_MAX_IDXS];

    /*!
     * Number of Virtual Pstates described in the VBIOS Virutal Pstate Table,
     * which are being described by the RM for the PMU.
     *
     * @note The Virtual Pstate table can describe more vpstates than the PMU
     * needs - this number will be limited to the maximum index needed by the
     * @ref vPstateIdxs array above.
     */
    LwU8                     numVPstates;
    /*!
     * Array of virtual pstates as specified in the VBIOS Virutal Pstate Table.
     */
    RM_PMU_PERF_VPSTATE_INFO *pVPstates;
} PERF_VPSTATE_TABLE;

/*!
 * This structure contains information specified by the VBIOS Perf Tables in a
 * format specific to the PMU.
 */
typedef struct
{
    /*!
     * Number of Pstates supported on this GPU.
     */
    LwU8  numPstates;
    /*!
     * Number of decoupled Domain Groups supported on this GPU.
     */
    LwU8  numDomGrpsDecoup;
    /*!
     * Number of VF entries on this GPU.
     */
    LwU8  numVfEntries;
    /*!
     * Minimum voltage limit set by non-PERF VOLTAGE_REQUEST_CLIENTs.  Units of
     * uV.
     */
    LwU32 nonPerfMilwoltageuV;
    /*!
     * Array of RM_PMU_PERF_PSTATE_INFO structures describing the pstates on the
     * GPU.  Has valid indexes form 0 to @ref numPstates - 1.
     */
    RM_PMU_PERF_PSTATE_INFO   *pPstates;
    /*!
     * Array of RM_PMU_PERF_VF_ENTRY_INFO structures describing the VF entries
     * on the GPU.  Has valid indexes form 0 to @ref numPstates - 1.
     */
    RM_PMU_PERF_VF_ENTRY_INFO *pVfEntries;
    /*!
     * Structure representing the VBIOS Virtual Pstate Table.
     */
    PERF_VPSTATE_TABLE vPstateTbl;
} PERF_TABLES;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */

/*!
 * An invalid vPstate index.
 *
 * NG-TODO: Share a common definition with the RM
 */
#define PERF_VPSTATE_ILWALID_IDX                                           0xFE

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_2X)
/*!
 * Gets the MSCG dynamic current co-eff for the current pstate index
 *
 * Required for current aware PSI
 */
#define perfMsDynLwrrentCoeffGet(pstateIdx)                                    \
 (Perf.tables.pPstates[(pstateIdx)].msDynamicLwrrentCoefficient)

/*!
 * Macro required to check the validity of latest PERF state since Domain Group
 * specific values are updated only after the first pstate callback oclwrs.
 */
#define perfPstatesAreSupported() (Perf.status.numDomGrps != 0)
#else
#define perfMsDynLwrrentCoeffGet(pstateIdx) (0x0)
#define perfPstatesAreSupported() (LW_FALSE)
#endif

/*!
 * @brief   Macro accessor for PERF VF entry information.
 *
 * @param[in]   vfEntryIndex    Index of requested VF entry.
 *
 * @return Pointer to corresponding RM_PMU_PERF_VF_ENTRY_INFO structure.
 *         NULL is returned if vfEntryIdx is invalid.
 *
 * @note    To conserve IMEM it should be expanded only once in each task.
 *          Create respective fucntion wrappers if used more than once.
 */
#define PERF_VF_ENTRY_GET(vfEntryIdx)                                          \
    (((vfEntryIdx) < Perf.tables.numVfEntries) ?                               \
    (&Perf.tables.pVfEntries[(vfEntryIdx)]) :                                  \
    (NULL))

/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS perfConstructPs20(void)
    GCC_ATTRIB_SECTION("imem_init", "perfConstructPs20");

FLCN_STATUS perfProcessPerfStatus(RM_PMU_PERF_STATUS *pStatus);
FLCN_STATUS perfProcessTablesInfo(RM_PMU_PERF_TABLES_INFO *pInfo);
FLCN_STATUS perfVPstatePackPstateCeilingToDomGrpLimits(PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits)
    GCC_ATTRIB_SECTION("imem_perfVf", "perfVPstatePackPstateCeilingToDomGrpLimits");
LwU32      perfVoltageuVGet(void);
RM_PMU_PERF_PSTATE_INFO              *perfPstateGet(LwU8 pstateIdx);
RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *perfPstateDomGrpGet(LwU8 pstateIdx, LwU8 domGrpIdx);
LwU32                                 perfPstateMilwoltageuVGet(LwU8 pstateIdx);
RM_PMU_PERF_VF_ENTRY_INFO            *perfVfEntryGet(LwU8 vfEntryIdx);
LwU32                                 perfVfEntryGetMaxFreqKHz(RM_PMU_PERF_PSTATE_DOMAIN_GROUP_INFO *pDomGrpInfo, RM_PMU_PERF_VF_ENTRY_INFO *pVfEntry);
LwU8                                  perfVfEntryIdxGetForFreq(LwU8 pstateIdx, LwU8 domGrpIdx, LwU32 freqKHz);

#endif // PERFPS20_H
