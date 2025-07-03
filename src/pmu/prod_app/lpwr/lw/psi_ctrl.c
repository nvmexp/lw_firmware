/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   objpsi.c
 * @brief  PMU PG PSI related functions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostask.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpsi.h"
#include "pmu_objlpwr.h"
#include "pmu_objdi.h"
#include "pmu_objperf.h"
#include "pmu_objms.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief API to get PSICTRL rail enabledMask corresponding to PERF Entry Index
 *
 * @param[in]  ctrlId    PSI CTRL ID
 * @param[in]  perfIdx   PERF Entry Index
 *
 * @return     railMask  VBIOS rail mask for corresponding P-state
 */
LwU32
psiCtrlPstateRailMaskGet
(
    LwU32  ctrlId,
    LwU8   perfIdx
)
{
    LwU32 railMask = 0;
    LwU8  idx;

    if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_GR)) &&
        (ctrlId == RM_PMU_PSI_CTRL_ID_GR))
    {
        idx      = lpwrVbiosGrEntryIdxGet(perfIdx);
        railMask = Lpwr.pVbiosData->gr.entry[idx].psiRailMask;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_MS)) &&
             (ctrlId == RM_PMU_PSI_CTRL_ID_MS))
    {
        idx      = lpwrVbiosMsEntryIdxGet(perfIdx);
        railMask = Lpwr.pVbiosData->ms.entry[idx].psiRailMask;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_DI)) &&
             (ctrlId == RM_PMU_PSI_CTRL_ID_DI))
    {
        idx      = lpwrVbiosDiEntryIdxGet(perfIdx);
        railMask = Lpwr.pVbiosData->di.entry[idx].psiRailMask;
    }
    else if ((PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_PSTATE)) &&
             (ctrlId == RM_PMU_PSI_CTRL_ID_PSTATE))
    {
        // Get idx in the LPWR Index Table corresponding to PERF Table Entry
        idx = PSTATE_GET_LPWR_ENTRY_IDX(perfIdx);

        //
        // For PState Based PSI, we always engage the PSI on all the
        // supported rails. Hence, there is no individual rail mask
        // for each pstate
        //
        if (Lpwr.pVbiosData->idx.entry[idx].bPstateCoupledPSIEnable)
        {
            railMask = Psi.railSupportMask;
        }
    }
    else
    {
        PMU_BREAKPOINT();
    }

    return (Psi.railSupportMask & railMask);
}
/* ------------------------- Private Functions ------------------------------ */

