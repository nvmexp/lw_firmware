/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  fancooler_pmumon.c
 * @brief Fan Cooler PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"   // for PmuInitArgs
#include "therm/objtherm.h"
#include "fan/objfan.h"
#include "fan/fancooler.h"
#include "fan/fancooler_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "lwos_ustreamer.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static FAN_COOLER_PMUMON s_fanCoolerPmumon
    GCC_ATTRIB_SECTION("dmem_therm", "s_fanCoolerPmumon");

static OS_TMR_CALLBACK OsCBFanCoolerPmuMon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_fanCoolerPmumonCallback)
    GCC_ATTRIB_SECTION("imem_thermLibFanCommon", "s_fanCoolerPmumonCallback")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc fanCoolerPmumonConstruct
 */
FLCN_STATUS
fanCoolerPmumonConstruct(void)
{
    FLCN_STATUS status;

    status = pmumonQueueConstruct(
        &s_fanCoolerPmumon.queueDescriptor,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.fan.fanCoolerPmumonQueue.header),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.fan.fanCoolerPmumonQueue.buffer),
        sizeof(RM_PMU_FAN_PMUMON_FAN_COOLERS_SAMPLE),
        LW2080_CTRL_FAN_PMUMON_FAN_COOLER_SAMPLE_COUNT);
    if (status != FLCN_OK)
    {
        goto fanCoolerPmumonConstruct_exit;
    }

fanCoolerPmumonConstruct_exit:
    return status;
}

/*!
 * @copydoc fanCoolerPmumonLoad
 */
FLCN_STATUS
fanCoolerPmumonLoad(void)
{
    FLCN_STATUS status;

    s_fanCoolerPmumon.sample.data.hdr.data.super.super.type     = Fan.coolers.super.super.type;
    s_fanCoolerPmumon.sample.data.hdr.data.super.super.classId  = Fan.coolers.super.super.classId;
    s_fanCoolerPmumon.sample.data.hdr.data.super.super.objSlots = Fan.coolers.super.super.objSlots;
    s_fanCoolerPmumon.sample.data.hdr.data.super.super.flags    = FLD_SET_DRF(_RM_PMU_BOARDOBJGRP_SUPER,
                                                                    _FLAGS, _GET_STATUS_COPY_IN, _FALSE,
                                                                    s_fanCoolerPmumon.sample.data.hdr.data.super.super.flags);

    LW2080_CTRL_BOARDOBJGRP_MASK_E32_INIT(&s_fanCoolerPmumon.sample.data.hdr.data.super.objMask.super);

    status = boardObjGrpMaskExport_E32(&Fan.coolers.super.objMask,
                                       &s_fanCoolerPmumon.sample.data.hdr.data.super.objMask);
    if (status != FLCN_OK)
    {
        goto fanCoolerPmumonLoad_exit;
    }

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBFanCoolerPmuMon)))
    {
        status = osTmrCallbackCreate(&(OsCBFanCoolerPmuMon),                // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(thermLibFanCommon),                      // ovlImem
                    s_fanCoolerPmumonCallback,                              // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, THERM),                                 // queueHandle
                    FAN_COOLER_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(),    // periodNormalus
                    FAN_COOLER_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),       // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_THERM);                                  // taskId
        if (status != FLCN_OK)
        {
            goto fanCoolerPmumonLoad_exit;
        }
    }

    status = osTmrCallbackSchedule(&(OsCBFanCoolerPmuMon));
    if (status != FLCN_OK)
    {
        goto fanCoolerPmumonLoad_exit;
    }

fanCoolerPmumonLoad_exit:
    return status;
}

/*!
 * @copydoc fanCoolerPmumonUnload
 */
void
fanCoolerPmumonUnload(void)
{
    osTmrCallbackCancel(&OsCBFanCoolerPmuMon);
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_fanCoolerPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FAN_COOLER   *pCooler;
    FLCN_STATUS   status;
    LwBoardObjIdx i;

    osPTimerTimeNsLwrrentGetAsLwU64(&s_fanCoolerPmumon.sample.super.timestamp);

    BOARDOBJGRP_ITERATOR_BEGIN(FAN_COOLER, pCooler, i, NULL)
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet(&pCooler->super);

        PMU_HALT_OK_OR_GOTO(status,
            fanFanCoolerIfaceModel10GetStatus(pObjModel10,
                (RM_PMU_BOARDOBJ *)(&s_fanCoolerPmumon.sample.data.objects[i])),
            s_fanCoolerPmumonCallback_exit);
    }
    BOARDOBJGRP_ITERATOR_END;
#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = lwosUstreamerLog
    (
        pmumonUstreamerQueueId,
        LW2080_CTRL_PMUMON_USTREAMER_EVENT_FAN_COOLER,
        (LwU8*)&s_fanCoolerPmumon.sample,
        sizeof(RM_PMU_FAN_PMUMON_FAN_COOLERS_SAMPLE)
    );
    lwosUstreamerFlush(pmumonUstreamerQueueId);
#else // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = pmumonQueueWrite(&s_fanCoolerPmumon.queueDescriptor, &s_fanCoolerPmumon.sample);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (status != FLCN_OK)
    {
        goto s_fanCoolerPmumonCallback_exit;
    }

s_fanCoolerPmumonCallback_exit:
    return status;
}
