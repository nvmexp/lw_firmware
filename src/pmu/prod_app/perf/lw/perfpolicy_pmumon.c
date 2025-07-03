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
 * @file  perfpolicy_pmumon.c
 * @brief Perf Policy PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"   // for PmuInitArgs
#include "pmu_objperf.h"
#include "perf/perfpolicy.h"
#include "perf/perfpolicy_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static PERF_POLICY_PMUMON s_perfPolicyPmumon
    GCC_ATTRIB_SECTION("dmem_perf", "s_perfPolicyPmumon");

static OS_TMR_CALLBACK OsCBPerfPolicyPmuMon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_perfPolicyPmumonCallback)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "s_perfPolicyPmumonCallback")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc perfPolicyPmumonConstruct
 */
FLCN_STATUS
perfPolicyPmumonConstruct(void)
{
    FLCN_STATUS status;

    status = pmumonQueueConstruct(
        &s_perfPolicyPmumon.queueDescriptor,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.perf.perfPolicyPmumonQueue.header),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.perf.perfPolicyPmumonQueue.buffer),
        sizeof(RM_PMU_PERF_PMUMON_PERF_POLICIES_SAMPLE),
        LW2080_CTRL_PERF_PMUMON_PERF_POLICIES_SAMPLE_COUNT);
    if (status != FLCN_OK)
    {
        goto perfPolicyPmumonConstruct_exit;
    }

perfPolicyPmumonConstruct_exit:
    return FLCN_OK;
}

/*!
 * @copydoc perfPolicyPmumonLoad
 */
FLCN_STATUS
perfPolicyPmumonLoad(void)
{
    FLCN_STATUS status;

    s_perfPolicyPmumon.sample.data.hdr.data.super.super.type     = Perf.pPolicies->super.super.type;
    s_perfPolicyPmumon.sample.data.hdr.data.super.super.classId  = Perf.pPolicies->super.super.classId;
    s_perfPolicyPmumon.sample.data.hdr.data.super.super.objSlots = Perf.pPolicies->super.super.objSlots;
    s_perfPolicyPmumon.sample.data.hdr.data.super.super.flags    = FLD_SET_DRF(_RM_PMU_BOARDOBJGRP_SUPER,
                                                                    _FLAGS, _GET_STATUS_COPY_IN, _FALSE,
                                                                    s_perfPolicyPmumon.sample.data.hdr.data.super.super.flags);

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_INIT(&s_perfPolicyPmumon.sample.data.hdr.data.super.objMask.super);

    status = boardObjGrpMaskExport_E32(&Perf.pPolicies->super.objMask,
                                       &s_perfPolicyPmumon.sample.data.hdr.data.super.objMask);
    if (status != FLCN_OK)
    {
        goto perfPolicyPmumonLoad_exit;
    }

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfPolicyPmuMon)))
    {
        status = osTmrCallbackCreate(&(OsCBPerfPolicyPmuMon),               // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(libPerfPolicyBoardObj),                  // ovlImem
                    s_perfPolicyPmumonCallback,                             // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                  // queueHandle
                    PERF_POLICY_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(),   // periodNormalus
                    PERF_POLICY_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),      // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                   // taskId
        if (status != FLCN_OK)
        {
            goto perfPolicyPmumonLoad_exit;
        }
    }

    status = osTmrCallbackSchedule(&(OsCBPerfPolicyPmuMon));
    if (status != FLCN_OK)
    {
        goto perfPolicyPmumonLoad_exit;
    }

perfPolicyPmumonLoad_exit:
    return status;
}

/*!
 * @copydoc perfPolicyPmumonUnload
 */
void
perfPolicyPmumonUnload(void)
{
    osTmrCallbackCancel(&OsCBPerfPolicyPmuMon);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_perfPolicyPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status;

    osPTimerTimeNsLwrrentGetAsLwU64(&s_perfPolicyPmumon.sample.super.timestamp);

    status = perfPolicyPmumonGetSample(Perf.pPolicies, &s_perfPolicyPmumon.sample.data);
    if (status != FLCN_OK)
    {
        goto s_perfPolicyPmumonCallback_exit;
    }

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (LWOS_USTREAMER_IS_ENABLED())
    {
        status = lwosUstreamerLog
        (
            pmumonUstreamerQueueId,
            LW2080_CTRL_PMUMON_USTREAMER_EVENT_PERF_POLICY,
            (LwU8*)&s_perfPolicyPmumon.sample,
            sizeof(RM_PMU_PERF_PMUMON_PERF_POLICIES_SAMPLE)
        );
        lwosUstreamerFlush(pmumonUstreamerQueueId);
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = pmumonQueueWrite(&s_perfPolicyPmumon.queueDescriptor, &s_perfPolicyPmumon.sample);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (status != FLCN_OK)
    {
        goto s_perfPolicyPmumonCallback_exit;
    }

s_perfPolicyPmumonCallback_exit:
    return FLCN_OK;
}
