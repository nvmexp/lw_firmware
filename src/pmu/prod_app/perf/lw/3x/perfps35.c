/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   perfps35.c
 * @brief  Pstate 3.5 Interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * copydoc@ PerfDomGrpGetValue
 */
LwU32
perfDomGrpGetValue
(
    LwU8 domGrpIdx
)
{
    LwU32           value = LW_U32_MAX;
    OSTASK_OVL_DESC ovlDescList[] = {
#ifdef DMEM_VA_SUPPORTED
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)
#endif
#endif
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (domGrpIdx == RM_PMU_PERF_DOMAIN_GROUP_PSTATE)
        {
            value = perfPstateGetLevelByIndex(perfChangeSeqChangeLastPstateIndexGet());
            if (value == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
            {
                PMU_BREAKPOINT();
                goto perfDomGrpGetValue_exit;
            }
        }
        else
        {
            // Colwert the domain grout index to clock domain index
            LwU8 clkDomainIdx = clkDomainsGetIdxByDomainGrpIdx(domGrpIdx);

            if (BOARDOBJGRP_IS_VALID(CLK_DOMAIN, clkDomainIdx))
            {
                value = perfChangeSeqChangeLastClkFreqkHzGet(clkDomainIdx);
                goto perfDomGrpGetValue_exit;
            }
        }
perfDomGrpGetValue_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    if (value == LW_U32_MAX)
    {
        PMU_BREAKPOINT();
        value = LW_U8_MIN;  // Maintaing legacy behavior
    }

    return value;
}

/* ------------------------- Private Functions ------------------------------ */

