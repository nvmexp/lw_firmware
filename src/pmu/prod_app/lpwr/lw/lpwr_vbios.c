/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_vbios.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35)
LPWR_VBIOS LpwrVbios
    GCC_ATTRIB_SECTION("dmem_lpwr", "LpwrVbios");
#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35)

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Handles PMU PG VBIOS init operations
 */
void
lpwrVbiosPreInit()
{
#if PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35)
        // Allocate pVbiosData pointer to VBIOS data cache
        Lpwr.pVbiosData = &LpwrVbios;
        PMU_HALT_COND(Lpwr.pVbiosData != NULL);

        // Allocate the memory for DIFR VBIOS Data
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_DIFR))
        {
            Lpwr.pVbiosData->pDifr = lwosCallocType(OVL_INDEX_DMEM(lpwr), 1,
                                                    RM_PMU_LPWR_VBIOS_DIFR);
            if (Lpwr.pVbiosData->pDifr == NULL)
            {
                PMU_BREAKPOINT();
            }
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35)
}

/*!
 * @brief : Initialize VBIOS data structure with default values
 */
void
lpwrVbiosDataInit()
{
    LwU32 idx;

    // Initialize RM_PMU_LPWR_VBIOS_IDX_DATA structure
    Lpwr.pVbiosData->idx.basePeriodMs = RM_PMU_LPWR_VBIOS_CALLBACK_BASE_PERIOD_DEFAULT_MS;

    for (idx = 0; idx < RM_PMU_LPWR_VBIOS_IDX_ENTRY_COUNT_MAX; idx++)
    {
        Lpwr.pVbiosData->idx.entry[idx].grIdx   = RM_PMU_LPWR_VBIOS_GR_ENTRY_RSVD;
        Lpwr.pVbiosData->idx.entry[idx].msIdx   = RM_PMU_LPWR_VBIOS_MS_ENTRY_RSVD;
        Lpwr.pVbiosData->idx.entry[idx].diIdx   = RM_PMU_LPWR_VBIOS_DI_ENTRY_RSVD;
        Lpwr.pVbiosData->idx.entry[idx].grRgIdx = RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_RSVD;
        Lpwr.pVbiosData->idx.entry[idx].eiIdx   = RM_PMU_LPWR_VBIOS_EI_ENTRY_RSVD;
    }

    // Initialize RM_PMU_LPWR_VBIOS_GR_DATA structure
    Lpwr.pVbiosData->gr.defaultEntryIdx          = RM_PMU_LPWR_VBIOS_GR_ENTRY_RSVD;
    Lpwr.pVbiosData->gr.idleThresholdUs          = RM_PMU_PG_CTRL_IDLE_THRESHOLD_DEFAULT_US;
    Lpwr.pVbiosData->gr.adaptiveGrBaseMultiplier = RM_PMU_LPWR_VBIOS_CALLBACK_BASE_MULTIPLIER_DEFAULT;

    for (idx = 0; idx < RM_PMU_LPWR_VBIOS_GR_ENTRY_COUNT_MAX; idx++)
    {
        Lpwr.pVbiosData->gr.entry[idx].psiRailMask = BIT(RM_PMU_PSI_RAIL_ID_LWVDD);
    }

    // Initialize RM_PMU_LPWR_VBIOS_MS_DATA structure
    Lpwr.pVbiosData->ms.defaultEntryIdx            = RM_PMU_LPWR_VBIOS_MS_ENTRY_RSVD;
    Lpwr.pVbiosData->ms.idleThresholdUs            = RM_PMU_PG_CTRL_IDLE_THRESHOLD_AGGRESSIVE_US;
    Lpwr.pVbiosData->ms.adaptiveMscgBaseMultiplier = RM_PMU_LPWR_VBIOS_CALLBACK_BASE_MULTIPLIER_DEFAULT;
    Lpwr.pVbiosData->ms.adaptiveMscgIndexLwrrent   = RM_PMU_LPWR_VBIOS_AP_ENTRY_RSVD;
    Lpwr.pVbiosData->ms.adaptiveMscgIndexAc        = RM_PMU_LPWR_VBIOS_AP_ENTRY_RSVD;
    Lpwr.pVbiosData->ms.adaptiveMscgIndexDc        = RM_PMU_LPWR_VBIOS_AP_ENTRY_RSVD;

    for (idx = 0; idx < RM_PMU_LPWR_VBIOS_MS_ENTRY_COUNT_MAX; idx++)
    {
        Lpwr.pVbiosData->ms.entry[idx].dynamicLwrrentLogic = LW_U16_MAX;
        Lpwr.pVbiosData->ms.entry[idx].dynamicLwrrentSram  = LW_U16_MAX;
        Lpwr.pVbiosData->ms.entry[idx].psiRailMask         = BIT(RM_PMU_PSI_RAIL_ID_LWVDD);
    }

    // Initialize RM_PMU_LPWR_VBIOS_DI_DATA structure
    Lpwr.pVbiosData->di.defaultEntryIdx          = RM_PMU_LPWR_VBIOS_DI_ENTRY_RSVD;
    Lpwr.pVbiosData->di.idleThresholdUs          = RM_PMU_PG_CTRL_IDLE_THRESHOLD_AGGRESSIVE_US;
    Lpwr.pVbiosData->di.sleepVfeIdxLogic         = PMU_PERF_VFE_EQU_INDEX_ILWALID;
    Lpwr.pVbiosData->di.sleepVfeIdxSram          = PMU_PERF_VFE_EQU_INDEX_ILWALID;
    Lpwr.pVbiosData->di.sleepVoltDevIdxLogic     = PG_VOLT_RAIL_VOLTDEV_IDX_ILWALID;
    Lpwr.pVbiosData->di.sleepVoltDevIdxSram      = PG_VOLT_RAIL_VOLTDEV_IDX_ILWALID;
    Lpwr.pVbiosData->di.voltageSettleTimeUs      = RM_PMU_LPWR_VBIOS_VOLTAGE_SETTLE_TIME_ILWALID;
    Lpwr.pVbiosData->di.adaptiveDiBaseMultiplier = RM_PMU_LPWR_VBIOS_CALLBACK_BASE_MULTIPLIER_DEFAULT;

    for (idx = 0; idx < RM_PMU_LPWR_VBIOS_DI_ENTRY_COUNT_MAX; idx++)
    {
        Lpwr.pVbiosData->di.entry[idx].psiRailMask = BIT(RM_PMU_PSI_RAIL_ID_LWVDD);
    }

    // Initialize RM_PMU_LPWR_VBIOS_GR_RG_DATA structure
    Lpwr.pVbiosData->grRg.defaultEntryIdx      = RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_RSVD;
    Lpwr.pVbiosData->grRg.idleThresholdUs      = RM_PMU_LPWR_CTRL_GR_RG_IDLE_THRESHOLD_DEFAULT_US;
    Lpwr.pVbiosData->grRg.autoWakeupIntervalMs = RM_PMU_LPWR_VBIOS_GR_RG_AUTO_WAKEUP_INTERVAL_DEFAULT_MS;

    // Initialize RM_PMU_LPWR_VBIOS_EI_DATA structure
    Lpwr.pVbiosData->ei.defaultEntryIdx      = RM_PMU_LPWR_VBIOS_EI_ENTRY_RSVD;
    Lpwr.pVbiosData->ei.idleThresholdUs      = RM_PMU_LPWR_CTRL_EI_IDLE_THRESHOLD_DEFAULT_US;
    Lpwr.pVbiosData->ei.clientCallbackBaseMs = RM_PMU_LPWR_VBIOS_EI_CALLBACK_BASE_PERIOD_DEFAULT_MS;

    // Initialize RM_PMU_LPWR_VBIOS_DIFR_DATA structure
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_DIFR))
    {
        Lpwr.pVbiosData->pDifr->defaultEntryIdx            = RM_PMU_LPWR_VBIOS_DIFR_ENTRY_RSVD;
        Lpwr.pVbiosData->pDifr->prefetchFsmIdleThresholdUs = RM_PMU_LPWR_CTRL_DFPR_IDLE_THRESHOLD_DEFAULT_US;
        Lpwr.pVbiosData->pDifr->swAsrFsmIdleThresholdUs    = RM_PMU_LPWR_CTRL_MS_DIFR_SW_ASR_IDLE_THRESHOLD_DEFAULT_US;
        Lpwr.pVbiosData->pDifr->cgFsmIdleThresholdUs       = RM_PMU_LPWR_CTRL_MS_DIFR_CG_IDLE_THRESHOLD_DEFAULT_US;

        for (idx = 0; idx < RM_PMU_LPWR_VBIOS_DIFR_ENTRY_COUNT_MAX; idx++)
        {
            Lpwr.pVbiosData->pDifr->entry[idx].psiRailMask = BIT(RM_PMU_PSI_RAIL_ID_LWVDD);
        }
    }
}

/*!
 * @brief Gets the GR table entry index corresponding to perfIdx
 *
 * @param[in]  perfIdx  Perf Index
 *
 * @return     grIdx    Index to Gr table entry
 */
LwU8
lpwrVbiosGrEntryIdxGet
(
    LwU8 perfIdx
)
{
    LwU8     lpwrIdx = RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT;
    LwU8     grIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X))
    // Get idx in the LPWR Index Table corresponding to PERF Table Entry
    lpwrIdx = PSTATE_GET_LPWR_ENTRY_IDX(perfIdx);
#endif

    if (lpwrIdx == RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_GR_ENTRY_RSVD;
    }
    else if (lpwrIdx >= RM_PMU_LPWR_VBIOS_IDX_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_GR_ENTRY_RSVD;
    }

    // Get idx in the LPWR GR Table corresponding to IDX Table Entry
    grIdx = Lpwr.pVbiosData->idx.entry[lpwrIdx].grIdx;

    if (grIdx == RM_PMU_LPWR_VBIOS_GR_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_GR_ENTRY_RSVD;
    }
    else if (grIdx >= RM_PMU_LPWR_VBIOS_GR_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_GR_ENTRY_RSVD;
    }

    return grIdx;
}

/*!
 * @brief Gets the MS table entry index corresponding to perfIdx
 *
 * @param[in]  perfIdx  Perf Index
 *
 * @return     msIdx    Index to MS table entry
 */
LwU8
lpwrVbiosMsEntryIdxGet
(
    LwU8 perfIdx
)
{
    LwU8     lpwrIdx = RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT;
    LwU8     msIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X))
    // Get idx in the LPWR Index Table corresponding to PERF Table Entry
    lpwrIdx = PSTATE_GET_LPWR_ENTRY_IDX(perfIdx);
#endif

    if (lpwrIdx == RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_MS_ENTRY_RSVD;
    }
    else if (lpwrIdx >= RM_PMU_LPWR_VBIOS_IDX_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_MS_ENTRY_RSVD;
    }

    // Get idx in the LPWR MS Table corresponding to IDX Table Entry
    msIdx = Lpwr.pVbiosData->idx.entry[lpwrIdx].msIdx;

    if (msIdx == RM_PMU_LPWR_VBIOS_MS_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_MS_ENTRY_RSVD;
    }
    else if (msIdx >= RM_PMU_LPWR_VBIOS_MS_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_MS_ENTRY_RSVD;
    }

    return msIdx;
}

/*!
 * @brief Gets the DI table entry index corresponding to perfIdx
 *
 * @param[in]  perfIdx  Perf Index
 *
 * @return     msIdx    Index to MS table entry
 */
LwU8
lpwrVbiosDiEntryIdxGet
(
    LwU8 perfIdx
)
{
    LwU8     lpwrIdx = RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT;
    LwU8     diIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X))
    // Get idx in the LPWR Index Table corresponding to PERF Table Entry
    lpwrIdx = PSTATE_GET_LPWR_ENTRY_IDX(perfIdx);
#endif

    if (lpwrIdx == RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_DI_ENTRY_RSVD;
    }
    else if (lpwrIdx >= RM_PMU_LPWR_VBIOS_IDX_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_DI_ENTRY_RSVD;
    }

    // Get idx in the LPWR DI Table corresponding to IDX Table Entry
    diIdx = Lpwr.pVbiosData->idx.entry[lpwrIdx].diIdx;

    if (diIdx == RM_PMU_LPWR_VBIOS_DI_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_DI_ENTRY_RSVD;
    }
    else if (diIdx >= RM_PMU_LPWR_VBIOS_DI_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_DI_ENTRY_RSVD;
    }

    return diIdx;
}

/*!
 * @brief Gets the GR-RG table entry index corresponding to perfIdx
 *
 * @param[in]  perfIdx  Perf Index
 *
 * @return     grRgIdx    Index to GR-RG table entry
 */
LwU8
lpwrVbiosGrRgEntryIdxGet
(
    LwU8 perfIdx
)
{
    LwU8     lpwrIdx = RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT;
    LwU8     grRgIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X))
    // Get idx in the LPWR Index Table corresponding to PERF Table Entry
    lpwrIdx = PSTATE_GET_LPWR_ENTRY_IDX(perfIdx);
#endif

    if (lpwrIdx == RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_RSVD;
    }
    else if (lpwrIdx >= RM_PMU_LPWR_VBIOS_IDX_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_RSVD;
    }

    // Get idx in the LPWR GR Table corresponding to IDX Table Entry
    grRgIdx = Lpwr.pVbiosData->idx.entry[lpwrIdx].grRgIdx;

    if (grRgIdx == RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_RSVD;
    }
    else if (grRgIdx >= RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_GR_RG_ENTRY_RSVD;
    }

    return grRgIdx;
}

/*!
 * @brief Gets the EI table entry index corresponding to perfIdx
 *
 * @param[in]  perfIdx  Perf Index
 *
 * @return     eiIdx    Index to Ei table entry
 */
LwU8
lpwrVbiosEiEntryIdxGet
(
    LwU8 perfIdx
)
{
    LwU8     lpwrIdx = RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT;
    LwU8     eiIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X))
    // Get idx in the LPWR Index Table corresponding to PERF Table Entry
    lpwrIdx = PSTATE_GET_LPWR_ENTRY_IDX(perfIdx);
#endif

    if (lpwrIdx == RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_EI_ENTRY_RSVD;
    }
    else if (lpwrIdx >= RM_PMU_LPWR_VBIOS_IDX_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_EI_ENTRY_RSVD;
    }

    // Get idx in the LPWR EI Table corresponding to IDX Table Entry
    eiIdx = Lpwr.pVbiosData->idx.entry[lpwrIdx].eiIdx;

    if (eiIdx == RM_PMU_LPWR_VBIOS_EI_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_EI_ENTRY_RSVD;
    }
    else if (eiIdx >= RM_PMU_LPWR_VBIOS_EI_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_EI_ENTRY_RSVD;
    }

    return eiIdx;
}

/*!
 * @brief Gets the DIFR table entry index corresponding to perfIdx
 *
 * @param[in]  perfIdx  Perf Index
 *
 * @return     difrIdx    Index to Difr table entry
 */
LwU8
lpwrVbiosDifrEntryIdxGet
(
    LwU8 perfIdx
)
{
    LwU8     lpwrIdx = RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT;
    LwU8     difrIdx;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_3X))
    // Get idx in the LPWR Index Table corresponding to PERF Table Entry
    lpwrIdx = PSTATE_GET_LPWR_ENTRY_IDX(perfIdx);
#endif

    if (lpwrIdx == RM_PMU_LPWR_VBIOS_IDX_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_DIFR_ENTRY_RSVD;
    }
    else if (lpwrIdx >= RM_PMU_LPWR_VBIOS_IDX_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_DIFR_ENTRY_RSVD;
    }

    // Get idx in the LPWR DIFR Table corresponding to IDX Table Entry
    difrIdx = Lpwr.pVbiosData->idx.entry[lpwrIdx].difrIdx;

    if (difrIdx == RM_PMU_LPWR_VBIOS_DIFR_ENTRY_DEFAULT)
    {
        return RM_PMU_LPWR_VBIOS_DIFR_ENTRY_RSVD;
    }
    else if (difrIdx >= RM_PMU_LPWR_VBIOS_DIFR_ENTRY_COUNT_MAX)
    {
        PMU_BREAKPOINT();
        return RM_PMU_LPWR_VBIOS_DIFR_ENTRY_RSVD;
    }

    return difrIdx;
}

/* ------------------------ Private Functions ------------------------------- */
