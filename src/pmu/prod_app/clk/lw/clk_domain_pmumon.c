/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  clk_domain_pmumon.c
 * @brief CLK Clk Domain PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_pmumon.h"
#include "lwos_ustreamer.h"
#include "pmu_objclk.h"
#include "clk/clk_domain_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @todo Move to some CLK structure
 */
static CLK_DOMAINS_PMUMON s_clkDomainsPmumon
    GCC_ATTRIB_SECTION("dmem_perf", "s_clkDomainsPmumon");

static OS_TMR_CALLBACK OsCBClkDomainsPmumon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_clkDomainsPmumonCallback)
    GCC_ATTRIB_SECTION("imem_perf", "s_clkDomainsPmumonCallback")
    GCC_ATTRIB_USED();
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc clkClkDomainsPmumonConstruct
 */
FLCN_STATUS
clkClkDomainsPmumonConstruct(void)
{
    FLCN_STATUS status;

    status = pmumonQueueConstruct(
        &s_clkDomainsPmumon.queueDescriptor,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.clk.clkDomainsPmumonQueue.header),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.clk.clkDomainsPmumonQueue.buffer),
        sizeof(LW2080_CTRL_CLK_PMUMON_CLK_DOMAINS_SAMPLE),
        LW2080_CTRL_CLK_PMUMON_CLK_DOMAINS_SAMPLE_COUNT);
    if (status != FLCN_OK)
    {
        goto clkClkDomainsPmumonConstruct_exit;
    }

clkClkDomainsPmumonConstruct_exit:
    return FLCN_OK;
}

/*!
 * @copydoc clkClkDomainsPmumonLoad
 */
FLCN_STATUS
clkClkDomainsPmumonLoad(void)
{
    FLCN_STATUS status;
    LwU32 index;

    status = clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_GPCCLK, &index);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkClkDomainsPmumonLoad_exit;
    }

    s_clkDomainsPmumon.gpcClkIdx = (LwBoardObjIdx)index;

    status = clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_MCLK, &index);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto clkClkDomainsPmumonLoad_exit;
    }

    s_clkDomainsPmumon.dramClkIdx = (LwBoardObjIdx)index;

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBClkDomainsPmumon)))
    {
        status = osTmrCallbackCreate(&(OsCBClkDomainsPmumon),               // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(perf),                                   // ovlImem
                    s_clkDomainsPmumonCallback,                             // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                  // queueHandle
                    CLK_DOMAINS_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(),   // periodNormalus
                    CLK_DOMAINS_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),      // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                   // taskId
        if (status != FLCN_OK)
        {
            goto clkClkDomainsPmumonLoad_exit;
        }

        status = osTmrCallbackSchedule(&(OsCBClkDomainsPmumon));
        if (status != FLCN_OK)
        {
            goto clkClkDomainsPmumonLoad_exit;
        }
    }

clkClkDomainsPmumonLoad_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_clkDomainsPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status;

    s_clkDomainsPmumon.sample.gpcClkFreqKHz  =
        perfChangeSeqChangeLastClkFreqkHzGet(s_clkDomainsPmumon.gpcClkIdx);
    s_clkDomainsPmumon.sample.dramClkFreqKHz =
        perfChangeSeqChangeLastClkFreqkHzGet(s_clkDomainsPmumon.dramClkIdx);

    osPTimerTimeNsLwrrentGetAsLwU64(&s_clkDomainsPmumon.sample.super.timestamp);

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = lwosUstreamerLog
        (
            pmumonUstreamerQueueId,
            LW2080_CTRL_PMUMON_USTREAMER_EVENT_CLK_DOMAINS,
            (LwU8*)&s_clkDomainsPmumon.sample,
            sizeof(LW2080_CTRL_CLK_PMUMON_CLK_DOMAINS_SAMPLE)
        );
    lwosUstreamerFlush(pmumonUstreamerQueueId);
#else // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = pmumonQueueWrite(&s_clkDomainsPmumon.queueDescriptor, &s_clkDomainsPmumon.sample);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (status != FLCN_OK)
    {
        goto s_clkDomainsPmumonCallback_exit;
    }

s_clkDomainsPmumonCallback_exit:
    return FLCN_OK; // NJ-TODO-CB
}
