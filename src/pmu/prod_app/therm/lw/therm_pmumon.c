/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  therm_pmumon.c
 * @brief Therm PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "therm/therm_pmumon.h"
#include "therm/objtherm.h"
#include "dev_pwr_csb.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
OS_TMR_CALLBACK OsCBThermPmumon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief Active Period Callback Time : 200 ms
 */
#define THERM_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US                   (200000U)

/*!
 * @brief Idle Period Callback Time : 1 sec
 */
#define THERM_PMUMON_IDLE_POWER_CALLBACK_PERIOD_US                    (1000000U)

/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_thermCallbackPmumon)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "s_thermCallbackPmumon")
    GCC_ATTRIB_USED();

static FLCN_STATUS s_thermPmumonUnload (void)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "s_thermPmumonUnload");

static FLCN_STATUS s_thermPmumonLoad (void)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "s_thermPmumonLoad");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   OBJECT_LOAD RPC helper for THERM_PMUMON Sys Tsense Recovery WAR.
 *
 * @param[in]     bLoad If Load or Unload event.
 */
FLCN_STATUS
thermPmumon
(
    LwBool bLoad
)
{
    FLCN_STATUS status = FLCN_OK;

    if (bLoad)
    {
        status = s_thermPmumonLoad();
    }
    else
    {
        status = s_thermPmumonUnload();
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   OBJECT_LOAD RPC helper for THERM PMUMON.
 *
 * @return  OBJECT_LOAD RPC helper Status.
 */
static FLCN_STATUS
s_thermPmumonLoad (void)
{
    FLCN_STATUS status;

    // Register OS Callback.
    if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBThermPmumon))
    {
        PMU_HALT_OK_OR_GOTO(status,
            osTmrCallbackCreate(&OsCBThermPmumon,                         // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,   // type
                OVL_INDEX_IMEM(thermLibSensor2X),                         // ovlImem
                s_thermCallbackPmumon,                                    // pTmrCallbackFunc
                LWOS_QUEUE(PMU, THERM),                                   // queueHandle
                THERM_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US,             // periodNormalus
                THERM_PMUMON_IDLE_POWER_CALLBACK_PERIOD_US,               // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                         // bRelaxedUseSleep
                RM_PMU_TASK_ID_THERM),                                    // taskId
            s_thermPmumonLoad_exit);

        PMU_HALT_OK_OR_GOTO(status,
            osTmrCallbackSchedule(&OsCBThermPmumon),
            s_thermPmumonLoad_exit);
    }

    // Switch SENSOR_9 I2CS Source to GPU_AVG.
    PMU_HALT_OK_OR_GOTO(status,
        thermSensor9I2csSourceSet_HAL(&Therm, LW_CPWR_THERM_SENSOR_9_I2CS_GPU_AVG),
        s_thermPmumonLoad_exit);

s_thermPmumonLoad_exit:
    return status;
}

/*!
 * @brief   OBJECT_UNLOAD RPC helper for THERM PMUMON.
 *
 * @return  OBJECT_UNOAD RPC helper Status.
 */
static FLCN_STATUS
s_thermPmumonUnload(void)
{
    FLCN_STATUS status;

    // Switch SENSOR_9 I2CS Source to GPU_MAX.
    status = thermSensor9I2csSourceSet_HAL(&Therm, LW_CPWR_THERM_SENSOR_9_I2CS_GPU_MAX);

    osTmrCallbackCancel(&OsCBThermPmumon);

    // Avoid Breakpoint for Failure in Unload path.
    return status;
}

/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_thermCallbackPmumon
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS     status;
    LwTemp          gpcMungedTemp;

    // Critical Section Acquired to avoid Stale GPC_AVG Temp being override in SYS Tsense.
    appTaskCriticalEnter();
    {
        // Retrieve GPC_AVG Munged Temperature.
        status = thermGetGpuGpcCombinedTemp_HAL(&Therm,
                        LW_CPWR_THERM_TEMP_SENSOR_ID_GPC_AVG, &gpcMungedTemp);

        if (status != FLCN_OK)
        {
            // Do not add breakpoint here, above interface takes care. Just skip faking Sys Tsense.
            goto s_thermCallbackPmumon_exit;
        }

        // Override Sys Tsense with GPC_AVG MUNGED.
        thermSysTsenseTempSet_HAL(&Therm, gpcMungedTemp);

s_thermCallbackPmumon_exit:
        lwosNOP();
    }
    appTaskCriticalExit();

    // Force Status as SUCCESS for next attempt.
    return FLCN_OK;
}
