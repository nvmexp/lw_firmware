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
 * @file  voltrail_pmumon.c
 * @brief Volt Rails PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_pmumon.h"
#include "volt/objvolt.h"
#include "volt/voltrail_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @todo Move to some VOLT structure
 */
static VOLT_RAILS_PMUMON s_voltRailsPmumon
    GCC_ATTRIB_SECTION("dmem_perf", "s_voltRailsPmumon");

static OS_TMR_CALLBACK OsCBVoltRailsPmumon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_voltRailsPmumonCallback)
    GCC_ATTRIB_SECTION("imem_libVolt", "s_voltRailsPmumonCallback")
    GCC_ATTRIB_USED();
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc voltVoltRailsPmumonConstruct
 */
FLCN_STATUS
voltVoltRailsPmumonConstruct(void)
{
    FLCN_STATUS status;

    status = pmumonQueueConstruct(
        &s_voltRailsPmumon.queueDescriptor,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.volt.voltRailsPmumonQueue.header),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.volt.voltRailsPmumonQueue.buffer),
        sizeof(LW2080_CTRL_VOLT_PMUMON_VOLT_RAILS_SAMPLE),
        LW2080_CTRL_VOLT_PMUMON_VOLT_RAILS_SAMPLE_COUNT);
    if (status != FLCN_OK)
    {
        goto voltVoltRailsPmumonConstruct_exit;
    }

voltVoltRailsPmumonConstruct_exit:
    return FLCN_OK;
}

/*!
 * @copydoc voltVoltRailsPmumonLoad
 */
FLCN_STATUS
voltVoltRailsPmumonLoad(void)
{
    FLCN_STATUS status = FLCN_OK;

    s_voltRailsPmumon.lwvddIdx =
        voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC);

    s_voltRailsPmumon.msvddIdx =
        voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_MSVDD);

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBVoltRailsPmumon)))
    {
        status = osTmrCallbackCreate(&(OsCBVoltRailsPmumon),                // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(libVolt),                                // ovlImem
                    s_voltRailsPmumonCallback,                              // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                  // queueHandle
                    VOLT_RAILS_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(),    // periodNormalus
                    VOLT_RAILS_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),       // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                   // taskId
        if (status != FLCN_OK)
        {
            goto voltVoltRailsPmumonLoad_exit;
        }

        status = osTmrCallbackSchedule(&(OsCBVoltRailsPmumon));
        if (status != FLCN_OK)
        {
            goto voltVoltRailsPmumonLoad_exit;
        }
    }

voltVoltRailsPmumonLoad_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_voltRailsPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status;

    if (s_voltRailsPmumon.lwvddIdx != LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
    {
        s_voltRailsPmumon.sample.lwvddVoltageuV =
            perfChangeSeqChangeLastVoltageuVGet(s_voltRailsPmumon.lwvddIdx);
    }
    else
    {
        s_voltRailsPmumon.sample.lwvddVoltageuV =
            LW2080_CTRL_VOLT_PMUMON_VOLT_RAILS_SAMPLE_ILWALID;
    }

    if (s_voltRailsPmumon.msvddIdx != LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
    {
        s_voltRailsPmumon.sample.msvddVoltageuV =
            perfChangeSeqChangeLastVoltageuVGet(s_voltRailsPmumon.msvddIdx);
    }
    else
    {
        s_voltRailsPmumon.sample.msvddVoltageuV =
            LW2080_CTRL_VOLT_PMUMON_VOLT_RAILS_SAMPLE_ILWALID;
    }

    osPTimerTimeNsLwrrentGetAsLwU64(&s_voltRailsPmumon.sample.super.timestamp);

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = lwosUstreamerLog
    (
        pmumonUstreamerQueueId,
        LW2080_CTRL_PMUMON_USTREAMER_EVENT_VOLT_RAILS,
        (LwU8*)&s_voltRailsPmumon.sample,
        sizeof(LW2080_CTRL_VOLT_PMUMON_VOLT_RAILS_SAMPLE)
    );
    lwosUstreamerFlush(pmumonUstreamerQueueId);
#else // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = pmumonQueueWrite(&s_voltRailsPmumon.queueDescriptor, &s_voltRailsPmumon.sample);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (status != FLCN_OK)
    {
        goto s_voltRailsPmumonCallback_exit;
    }

s_voltRailsPmumonCallback_exit:
    return FLCN_OK; // NJ-TODO-CB
}
