/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
 * @file  perfcfpmsensors_pmumon.c
 * @brief Perf Cf Pm Sensors PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_pm_sensor.h"
#include "perf/cf/perf_cf_pm_sensors_pmumon.h"
#include "pmu/ssurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @todo Move to some PERF_CF_PM structure
 */
static PERF_CF_PM_SENSORS_PMUMON PerfCfPmSensorsPmumon
    GCC_ATTRIB_SECTION("dmem_perfCfPmSensors", "PerfCfPmSensorsPmumon");

static OS_TMR_CALLBACK OsCBPerfCfPmSensorsPmumon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Returns whether the PMUMON queues are available for writing in the
 *          super-surface
 *
 * @details There are some situations in which the host systems are memory
 *          constrained enough that we limit the physical size of the
 *          super-surface by omitting functionality not needed on those systems,
 *          including the PM_SENSOR PMUMON queues. This function uses the
 *          super-surface size to detect those situations.
 *
 * @return  @ref LW_TRUE    Queues are supported
 * @return  @ref LW_FALSE   Otherwise
 */
static LwBool s_perfCfPmSensorsPmumonQueuesSupported(void)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsPmumonQueuesSupported");

static OsTmrCallbackFunc (s_perfCfPmSensorsPmumonCallback)
    GCC_ATTRIB_SECTION("imem_perfCfPmSensors", "s_perfCfPmSensorsPmumonCallback")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc perfCfSensorsPmumonConstruct
 */
 FLCN_STATUS
perfCfPmSensorsPmumonConstruct(void)
{
    // Reset the boolean tracking first time load.
    PerfCfPmSensorsPmumon.bIsFirstLoad = LW_TRUE;
 
    return FLCN_OK;
}

/*!
 * @copydoc perfCfSensorsPmumonLoad
 */
FLCN_STATUS
perfCfPmSensorsPmumonLoad(void)
{
    FLCN_STATUS status = FLCN_OK;

    // Check for ampere board only
    if (BOARDOBJGRP_MAX_IDX_GET(PERF_CF_PM_SENSOR) >= LW2080_CTRL_PERF_PMUMON_PERF_CF_PM_SENSORS_SAMPLE_BOARDOBJ_ENABLED_COUNT)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        PMU_BREAKPOINT();
        goto perfCfPmSensorsPmumonLoad_exit;
    }

    // If the queues can't be supported, then exit early with a success status.
    if (!s_perfCfPmSensorsPmumonQueuesSupported())
    {
        status = FLCN_OK;
        goto perfCfPmSensorsPmumonLoad_exit;
    }

    if (PerfCfPmSensorsPmumon.bIsFirstLoad )
    {
        PERF_CF_PM_SENSOR  *pPmSensor;
        LwBoardObjIdx       i;

        // Reset the boolean tracking the first load.
        PerfCfPmSensorsPmumon.bIsFirstLoad = LW_FALSE;

        // Init and copy the boardobj mask.
        boardObjGrpMaskInit_E32(&PerfCfPmSensorsPmumon.status.mask);

        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskCopy(&PerfCfPmSensorsPmumon.status.mask,
                &(PERF_CF_PM_GET_SENSORS()->super.objMask)),
            perfCfPmSensorsPmumonLoad_exit);

        BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_PM_SENSOR, pPmSensor, i, NULL)
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                pmumonQueueConstruct(
                    &PerfCfPmSensorsPmumon.queueDescriptor[i],
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(rmOnDemandMapped.perfCf.perfCfPmSensorsPmumonQueue[i].header), 
                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(rmOnDemandMapped.perfCf.perfCfPmSensorsPmumonQueue[i].buffer),
                    sizeof(LW2080_CTRL_PERF_PMUMON_PERF_CF_PM_SENSORS_SAMPLE),
                    LW2080_CTRL_PERF_PMUMON_PERF_CF_PM_SENSORS_SAMPLE_COUNT),
                perfCfPmSensorsPmumonLoad_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                boardObjGrpMaskExport_E1024(
                    &(pPmSensor->signalsSupportedMask),
                    &PerfCfPmSensorsPmumon.status.pmSensors[i].pmSensor.signalsMask),
                perfCfPmSensorsPmumonLoad_exit);
        }
        BOARDOBJGRP_ITERATOR_END;
    }
                
    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPerfCfPmSensorsPmumon)))
    {
        status = osTmrCallbackCreate(&(OsCBPerfCfPmSensorsPmumon),                 // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,        // type
                    OVL_INDEX_IMEM(perfCfPmSensors),                               // ovlImem
                    s_perfCfPmSensorsPmumonCallback,                               // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PERF),                                         // queueHandle
                    PERF_CF_PM_SENSORS_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(),   // periodNormalus
                    PERF_CF_PM_SENSORS_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),      // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                              // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PERF);                                          // taskId
        if (status != FLCN_OK)
        {
            goto perfCfPmSensorsPmumonLoad_exit;
        }

        PMU_ASSERT_OK_OR_GOTO(status,
            osTmrCallbackSchedule(&(OsCBPerfCfPmSensorsPmumon)),
            perfCfPmSensorsPmumonLoad_exit);
    }

perfCfPmSensorsPmumonLoad_exit:
    return status;
}

/*!
 * @copydoc perfCfSensorsPmumonUnload
 */
void
perfCfPmSensorsPmumonUnload(void)
{
    if (OS_TMR_CALLBACK_WAS_CREATED(&OsCBPerfCfPmSensorsPmumon))
    {
        osTmrCallbackCancel(&OsCBPerfCfPmSensorsPmumon);
    }
}

/* ------------------------- Private Functions ------------------------------ */
static LwBool
s_perfCfPmSensorsPmumonQueuesSupported(void)
{
    //
    // The queues can only be supported if the super-surface contains them in
    // their entirety.
    //
    return ssurfaceSize() >= 
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(
            rmOnDemandMapped.perfCf.perfCfPmSensorsPmumonQueue) +
        RM_PMU_SUPER_SURFACE_MEMBER_SIZE(
            rmOnDemandMapped.perfCf.perfCfPmSensorsPmumonQueue);
}

/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_perfCfPmSensorsPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS    status = FLCN_OK;
    LwBoardObjIdx  index;
    PERF_CF_PM_SENSOR *pPmSensor;
    LwBoardObjIdx      i;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPmSensorsStatusGet(PERF_CF_PM_GET_SENSORS(), &PerfCfPmSensorsPmumon.status),
        s_perfCfPmSensorsPmumonCallback_exit);
    
    osPTimerTimeNsLwrrentGetAsLwU64(&PerfCfPmSensorsPmumon.sample.super.timestamp);

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_PM_SENSOR, pPmSensor, i,NULL)
    {
        BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pPmSensor->signalsSupportedMask, index)
        {
            PerfCfPmSensorsPmumon.sample.signals[index].cntLast =  PerfCfPmSensorsPmumon.status.pmSensors[i].pmSensor.signals[index].cntLast;
            PerfCfPmSensorsPmumon.sample.signals[index].cntDiff =  PerfCfPmSensorsPmumon.status.pmSensors[i].pmSensor.signals[index].cntDiff;
        }
        BOARDOBJGRPMASK_FOR_EACH_END;

        PMU_ASSERT_OK_OR_GOTO(status,
           pmumonQueueWrite(&PerfCfPmSensorsPmumon.queueDescriptor[i], &PerfCfPmSensorsPmumon.sample),
           s_perfCfPmSensorsPmumonCallback_exit);

    }
    BOARDOBJGRP_ITERATOR_END;
    
s_perfCfPmSensorsPmumonCallback_exit:
    return status; 
}
