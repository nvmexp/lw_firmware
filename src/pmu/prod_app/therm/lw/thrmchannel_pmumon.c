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
 * @file  thrmchannel_pmumon.c
 * @brief Therm Channel PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"   // for PmuInitArgs
#include "lwos_ustreamer.h"
#include "therm/objtherm.h"
#include "therm/thrmchannel_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static THERM_CHANNEL_PMUMON s_thermChannelPmumon
    GCC_ATTRIB_SECTION("dmem_therm", "s_thermChannelPmumon");

static OS_TMR_CALLBACK OsCBThermChannelPmuMon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_thermChannelPmumonCallback)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "s_thermChannelPmumonCallback")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc thermChannelPmumonConstruct
 */
FLCN_STATUS
thermChannelPmumonConstruct(void)
{
    FLCN_STATUS status;

    status = pmumonQueueConstruct(
        &s_thermChannelPmumon.queueDescriptor,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.therm.thermChannelPmumonQueue.header),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.therm.thermChannelPmumonQueue.buffer),
        sizeof(LW2080_CTRL_THERM_PMUMON_THERM_CHANNEL_SAMPLE),
        LW2080_CTRL_THERM_PMUMON_THERM_CHANNEL_SAMPLE_COUNT);
    if (status != FLCN_OK)
    {
        goto thermChannelPmumonConstruct_exit;
    }

thermChannelPmumonConstruct_exit:
    return FLCN_OK;
}

/*!
 * @copydoc thermChannelPmumonLoad
 */
FLCN_STATUS
thermChannelPmumonLoad(void)
{
    FLCN_STATUS status;

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBThermChannelPmuMon)))
    {
        status = osTmrCallbackCreate(&(OsCBThermChannelPmuMon),             // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(thermLibSensor2X),                       // ovlImem
                    s_thermChannelPmumonCallback,                           // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, THERM),                                 // queueHandle
                    THERM_CHANNEL_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(), // periodNormalus
                    THERM_CHANNEL_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),    // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_THERM);                                  // taskId
        if (status != FLCN_OK)
        {
            goto thermChannelPmumonLoad_exit;
        }
    }

    status = osTmrCallbackSchedule(&(OsCBThermChannelPmuMon));
    if (status != FLCN_OK)
    {
        goto thermChannelPmumonLoad_exit;
    }

thermChannelPmumonLoad_exit:
    return status;
}

/*!
 * @copydoc thermChannelPmumonUnload
 */
void
thermChannelPmumonUnload(void)
{
    osTmrCallbackCancel(&OsCBThermChannelPmuMon);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_thermChannelPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    THERM_CHANNEL *pThermChannel;
    FLCN_STATUS    status;
    LwBoardObjIdx  i;

    osPTimerTimeNsLwrrentGetAsLwU64(&s_thermChannelPmumon.sample.super.timestamp);

    BOARDOBJGRP_ITERATOR_BEGIN(THERM_CHANNEL, pThermChannel, i, NULL)
    {
        (void)thermChannelTempValueGet(pThermChannel,
            &s_thermChannelPmumon.sample.temperature[i]);
    }
    BOARDOBJGRP_ITERATOR_END;

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (LWOS_USTREAMER_IS_ENABLED())
    {
        status = lwosUstreamerLog
        (
            pmumonUstreamerQueueId,
            LW2080_CTRL_PMUMON_USTREAMER_EVENT_THERM_CHANNEL,
            (LwU8*)&s_thermChannelPmumon.sample,
            sizeof(s_thermChannelPmumon.sample)
        );
        lwosUstreamerFlush(pmumonUstreamerQueueId);
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = pmumonQueueWrite(&s_thermChannelPmumon.queueDescriptor, &s_thermChannelPmumon.sample);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (status != FLCN_OK)
    {
        goto s_thermChannelPmumonCallback_exit;
    }

s_thermChannelPmumonCallback_exit:
    return FLCN_OK;
}
