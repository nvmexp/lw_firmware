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
 * @file  perf_cf_topology_pmumon.c
 * @brief Perf CF Topologies PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "main.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_pmumon.h"
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_topology.h"
#include "perf/cf/perf_cf_topology_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @todo Move to some PERF_CF structure
 */
static PERF_CF_TOPOLOGIES_PMUMON s_perfCfTopologiesPmumon
    GCC_ATTRIB_SECTION("dmem_perfCfTopology", "s_perfCfTopologiesPmumon");

static OS_TMR_CALLBACK OsCBPerfCfTopologiesPmumon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_perfCfTopologiesPmumonCallback)
    GCC_ATTRIB_SECTION("imem_perfCfTopology", "s_perfCfTopologiesPmumonCallback")
    GCC_ATTRIB_USED();
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc perfCfTopologiesPmumonConstruct
 */
FLCN_STATUS
perfCfTopologiesPmumonConstruct(void)
{
    FLCN_STATUS status;

    status = pmumonQueueConstruct(
        &s_perfCfTopologiesPmumon.queueDescriptor,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.perfCf.perfCfTopologiesPmumonQueue.header),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.perfCf.perfCfTopologiesPmumonQueue.buffer),
        sizeof(LW2080_CTRL_PERF_PMUMON_PERF_CF_TOPOLOGIES_SAMPLE),
        LW2080_CTRL_PERF_PMUMON_PERF_CF_TOPOLOGIES_SAMPLE_COUNT);
    if (status != FLCN_OK)
    {
        goto perfCfTopologiesPmumonConstruct_exit;
    }

    boardObjGrpMaskInit_E32(&s_perfCfTopologiesPmumon.status.mask);

perfCfTopologiesPmumonConstruct_exit:
    return FLCN_OK;
}

/*!
 * @copydoc perfCfTopologiesPmumonLoad
 */
FLCN_STATUS
perfCfTopologiesPmumonLoad(void)
{
    FLCN_STATUS status = FLCN_OK;

    status = boardObjGrpMaskCopy(&s_perfCfTopologiesPmumon.status.mask,
                                 &(PERF_CF_GET_TOPOLOGYS()->super.objMask));
    if (status != FLCN_OK)
    {
        goto perfCfTopologiesPmumonLoad_exit;
    }

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfCfTopologiesPmumon)))
    {
        status = osTmrCallbackCreate(&(OsCBPerfCfTopologiesPmumon),                 // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,         // type
                    OVL_INDEX_IMEM(perfCfTopology),                                 // ovlImem
                    s_perfCfTopologiesPmumonCallback,                               // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                          // queueHandle
                    PERF_CF_TOPOLOGIES_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(),    // periodNormalus
                    PERF_CF_TOPOLOGIES_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),       // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                               // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                           // taskId
        if (status != FLCN_OK)
        {
            goto perfCfTopologiesPmumonLoad_exit;
        }

        status = osTmrCallbackSchedule(&(OsCBPerfCfTopologiesPmumon));
        if (status != FLCN_OK)
        {
            goto perfCfTopologiesPmumonLoad_exit;
        }
    }

perfCfTopologiesPmumonLoad_exit:
    return status;
}


/*!
 * @copydoc perfCfTopologiesPmumonUnload
 */
void
perfCfTopologiesPmumonUnload(void)
{
    if (OS_TMR_CALLBACK_WAS_CREATED(&OsCBPerfCfTopologiesPmumon))
    {
        osTmrCallbackCancel(&OsCBPerfCfTopologiesPmumon);
    }
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_perfCfTopologiesPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS       status;
    PERF_CF_TOPOLOGY *pTopology;
    LwU64             util64      = 0;
    LwU64             pct100      = 10000;
    LwU32             util        = 0;
    LwU32             lwenlwtil   = 0;
    LwU32             lwencCount  = 0;
    LwU32             lwdelwtil   = 0;
    LwU32             lwdecCount  = 0;
    LwBoardObjIdx     i           = 0;

    status = perfCfTopologysStatusGet(PERF_CF_GET_TOPOLOGYS(),
                                      &s_perfCfTopologiesPmumon.status);
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologiesPmumonCallback_exit;
    }

    osPTimerTimeNsLwrrentGetAsLwU64(&s_perfCfTopologiesPmumon.sample.super.timestamp);

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_TOPOLOGY, pTopology, i, NULL)
    {
        if ((LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_NONE ==
                                            pTopology->gpumonTag)   ||
            pTopology->bNotAvailable)
        {
            continue;
        }

        LwU64_ALIGN32_UNPACK(&util64,
            &(s_perfCfTopologiesPmumon.status.topologys[i].topology.reading));

        // Scale 1 (FXP40.24) to 0.01% (LwU32).
        lwU64Mul(&util64, &util64, &pct100);
        lw64Lsr(&util64, &util64, 24);
        util = (LwU32)util64;

        // Clamp utilization to 100%.
        util = LW_MIN(util, (LwU32)pct100);

        //
        // KO-TODO: Switch to labels instead of using gpumon tags. Export
        //          utilization for all labeled topology entries.
        //
        switch (pTopology->gpumonTag)
        {
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_FB:
                s_perfCfTopologiesPmumon.sample.fbUtil = util;
                break;
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_GR:
                s_perfCfTopologiesPmumon.sample.grUtil = util;
                break;
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_LWENC:
                lwenlwtil += util;
                lwencCount++;
                break;
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_LWDEC:
                lwdelwtil += util;
                lwdecCount++;
                break;
            case LW2080_CTRL_PERF_PERF_CF_TOPOLOGY_GPUMON_TAG_VID:
                s_perfCfTopologiesPmumon.sample.vidUtil = util;
                break;
            default:
                PMU_BREAKPOINT();
                break;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Callwlate avarage if LWENC/LWDEC engines are present.
    if (lwencCount > 0)
    {
         s_perfCfTopologiesPmumon.sample.lwenlwtil = lwenlwtil / lwencCount;
    }
    if (lwdecCount > 0)
    {
         s_perfCfTopologiesPmumon.sample.lwdelwtil = lwdelwtil / lwdecCount;
    }

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (LWOS_USTREAMER_IS_ENABLED())
    {
        status = lwosUstreamerLog
        (
            pmumonUstreamerQueueId,
            LW2080_CTRL_PMUMON_USTREAMER_EVENT_PERF_CF_TOPOLOGIES,
            (LwU8*)&s_perfCfTopologiesPmumon.sample,
            sizeof(LW2080_CTRL_PERF_PMUMON_PERF_CF_TOPOLOGIES_SAMPLE)
        );
        lwosUstreamerFlush(pmumonUstreamerQueueId);
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = pmumonQueueWrite(&s_perfCfTopologiesPmumon.queueDescriptor, &s_perfCfTopologiesPmumon.sample);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (status != FLCN_OK)
    {
        goto s_perfCfTopologiesPmumonCallback_exit;
    }

s_perfCfTopologiesPmumonCallback_exit:
    return FLCN_OK; // NJ-TODO-CB
}
