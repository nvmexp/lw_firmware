/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrdev.c
 * @brief Power-Device Object Server
 *
 * This module is the primary interface to power-device objects. Each PWR_DEVICE
 * interface is provided here as a public funtion responsible for relaying the
 * request to the device-specific interface function.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmu_oslayer.h"
#include "pmgr/pwrdev.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
OS_TMR_CALLBACK OsCBPwrDev;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static OsTimerCallback      (s_pwrDevCallback)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "s_pwrDevCallback")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc    (s_pwrDevOsCallback)
    GCC_ATTRIB_SECTION("imem_pmgrPwrDeviceStateSync", "s_pwrDevOsCallback")
    GCC_ATTRIB_USED();
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
static void _pwrDevCheckAndScheduleLowSamplingCallback(LwBool bLowSampling);
#endif
static LwU8 s_pwrDevProvNumGet(PWR_DEVICE *pDev)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "s_pwrDevProvNumGet");
static LwU8 s_pwrDevThresholdNumGet(PWR_DEVICE *pDev)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "s_pwrDevThresholdNumGet");

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Relays the 'load' request to the appropriate PWR_DEVICE implementation.
 *
 * @copydoc PwrDevLoad
 */
FLCN_STATUS
pwrDevLoad
(
    PWR_DEVICE  *pDev
)
{
    switch (pDev->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA219:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA219);
            return pwrDevLoad_INA219(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            return pwrDevLoad_INA3221(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_NCT3933U);
            return pwrDevLoad_NCT3933U(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevLoad_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevLoad_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevLoad_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevLoad_BA15HW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA16HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA16);
            return pwrDevLoad_BA16HW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA20:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA20);
            return pwrDevLoad_BA20(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevLoad_GPUADC10(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevLoad_GPUADC11(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevLoad_GPUADC13(pDev);
    }

    return FLCN_OK;
}

/*!
 * Schedule power device callback routine.
 * Lwrrently we are using this callback function for updating BA devices.
 *
 * @param[in]   ticksNow        OS ticks timestamp to synchronize all load() code
 * @param[in]   bLowSampling
 *      Boolean indicating if this is scheduling Low Sampling callback
 */
void
pwrDevScheduleCallback
(
    LwU32   ticksNow,
    LwBool  bLowSampling
)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    {
        // Skip all rescheduling requests
        if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBPwrDev))
        {
            osTmrCallbackCreate(&OsCBPwrDev,                                // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,     // type
                OVL_INDEX_IMEM(pmgrPwrDeviceStateSync),                     // ovlImem
                s_pwrDevOsCallback,                                         // pTmrCallbackFunc
                LWOS_QUEUE(PMU, PMGR),                                      // queueHandle
                OS_TIMER_DELAY_1_MS * 200,                                  // periodNormalus
                OS_TIMER_DELAY_1_S,                                         // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                           // bRelaxedUseSleep
                RM_PMU_TASK_ID_PMGR);                                       // taskId

                osTmrCallbackSchedule(&OsCBPwrDev);
        }
        else
        {
            osTmrCallbackSchedule(&OsCBPwrDev);

            // Update callback periods
            osTmrCallbackUpdate(&OsCBPwrDev,
                                OS_TIMER_DELAY_1_MS * 200,
                                OS_TIMER_DELAY_1_S,
                                OS_TIMER_RELAXED_MODE_USE_NORMAL,
                                OS_TIMER_UPDATE_USE_BASE_LWRRENT);
        }
    }
#else
    {
        LwU32 samplePeriodus = OS_TIMER_DELAY_1_MS * 200;

        if (bLowSampling)
        {
            samplePeriodus = OS_TIMER_DELAY_1_S;
        }

        osTimerScheduleCallback(
                &PmgrOsTimer,
                PMGR_OS_TIMER_ENTRY_PWR_DEV_CALLBACK,
                ticksNow,
                samplePeriodus,
                DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_SKIP),
                s_pwrDevCallback,
                NULL,
                OVL_INDEX_IMEM(pmgrPwrDeviceStateSync));
    }
#endif
}

/*!
 * Cancel power device callback for updating BAxx devices
 */
void pwrDevCancelCallback(void)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    {
        osTmrCallbackCancel(&OsCBPwrDev);
    }
#else
    {
        osTimerDeScheduleCallback(&PmgrOsTimer,
                PMGR_OS_TIMER_ENTRY_PWR_DEV_CALLBACK);
    }
#endif
}

/*!
 * @copydoc PwrDevProcessPerfStatus
 */
void
pwrDevProcessPerfStatus(RM_PMU_PERF_STATUS *pStatus)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits;
    LwBool                   bLowSampling = LW_FALSE;

    //
    // Switch to Low Sampling callback, if in the new PerfStatus we are in
    // lowest Pstate, and lwrrently PMU is not requesting to cap at lowest.
    // For all other cases, continue to use normal sampling.
    //
    pwrPolicyDomGrpLimitsGetMin(&domGrpLimits);

    if ((domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE] != 0) &&
        (pStatus->domGrps[RM_PMU_DOMAIN_GROUP_PSTATE].value == 0))
    {
        bLowSampling = LW_TRUE;
    }

    _pwrDevCheckAndScheduleLowSamplingCallback(bLowSampling);
#endif
}

/*!
 * Relays the 'getLwrrent' request to the appropriate power-device.
 *
 * @copydoc PwrDevGetLwrrent
 */
FLCN_STATUS
pwrDevGetLwrrent
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pLwrrentmA
)
{
    switch (pDev->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA219:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA219);
            return pwrDevGetLwrrent_INA219(pDev, provIdx, pLwrrentmA);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            return pwrDevGetLwrrent_INA3221(pDev, provIdx, pLwrrentmA);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevGetLwrrent_BA1XHW(pDev, provIdx, pLwrrentmA);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevGetLwrrent_BA1XHW(pDev, provIdx, pLwrrentmA);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevGetLwrrent_BA1XHW(pDev, provIdx, pLwrrentmA);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevGetLwrrent_BA1XHW(pDev, provIdx, pLwrrentmA);
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Relays the 'getVoltage' request to the appropriate power-device.
 *
 * @copydoc PwrDevGetVoltage
 */
FLCN_STATUS
pwrDevGetVoltage
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pVoltageuV
)
{
    switch (pDev->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA219:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA219);
            return pwrDevGetVoltage_INA219(pDev, provIdx, pVoltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            return pwrDevGetVoltage_INA3221(pDev, provIdx, pVoltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevGetVoltage_BA1XHW(pDev, provIdx, pVoltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevGetVoltage_BA1XHW(pDev, provIdx, pVoltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevGetVoltage_BA1XHW(pDev, provIdx, pVoltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevGetVoltage_BA1XHW(pDev, provIdx, pVoltageuV);
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Relays the 'getPower' request to the appropriate power-device.
 *
 * @copydoc PwrDevGetPower
 */
FLCN_STATUS
pwrDevGetPower
(
    PWR_DEVICE  *pDev,
    LwU8         provIdx,
    LwU32       *pPowermW
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pDev->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA219:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA219);
            status = pwrDevGetPower_INA219(pDev, provIdx, pPowermW);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            status = pwrDevGetPower_INA3221(pDev, provIdx, pPowermW);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            status = pwrDevGetPower_BA1XHW(pDev, provIdx, pPowermW);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            status = pwrDevGetPower_BA1XHW(pDev, provIdx, pPowermW);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            status = pwrDevGetPower_BA1XHW(pDev, provIdx, pPowermW);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            status = pwrDevGetPower_BA1XHW(pDev, provIdx, pPowermW);
            break;
    }

    // If correction factor != 1, apply power correction.
    if ((status == FLCN_OK) &&
        (pDev->powerCorrFactor != LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1)))
    {
        //
        // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
        // because this code segment is tied to the @ref PMU_PMGR_PWR_MONITOR_1X
        // feature and is not used on AD10X and later chips.
        //
        *pPowermW = LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
                      ((*pPowermW) * pDev->powerCorrFactor));
    }

    return status;
}

/*!
 * Relays the 'tupleGet' request to the appropriate power-device.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleGet
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;
    memset(pTuple, 0, sizeof(LW2080_CTRL_PMGR_PWR_TUPLE));

    switch (pDev->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            status = pwrDevTupleGet_INA3221(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA219:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA219);
            status = pwrDevTupleGet_INA219(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            status = pwrDevTupleGet_BA1XHW(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            status = pwrDevTupleGet_BA13HW(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            status = pwrDevTupleGet_BA14HW(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            status = pwrDevTupleGet_BA1XHW(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA16HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA16);
            status = pwrDevTupleGet_BA16HW(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA20:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA20);
            status = pwrDevTupleGet_BA20(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            status = pwrDevTupleGet_GPUADC10(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            status = pwrDevTupleGet_GPUADC11(pDev, provIdx, pTuple);
            break;

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            status = pwrDevTupleGet_GPUADC13(pDev, provIdx, pTuple);
            break;
    }

    return status;
}

/*!
 * Relays the 'tupleFullRangeGet' request to the appropriate power-device.
 *
 * @copydoc PwrDevTupleGet
 */
FLCN_STATUS
pwrDevTupleFullRangeGet
(
    PWR_DEVICE                 *pDev,
    LwU8                        provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    switch (BOARDOBJ_GET_TYPE(pDev))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevTupleFullRangeGet_GPUADC10(pDev, provIdx, pTuple);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevTupleFullRangeGet_GPUADC11(pDev, provIdx, pTuple);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevTupleFullRangeGet_GPUADC13(pDev, provIdx, pTuple);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Relays the 'tupleAccGet' request to the appropriate power-device.
 *
 * @copydoc PwrDevTupleAccGet
 */
FLCN_STATUS
pwrDevTupleAccGet
(
    PWR_DEVICE                     *pDev,
    LwU8                            provIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE_ACC *pTupleAcc
)
{
    switch (BOARDOBJ_GET_TYPE(pDev))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevTupleAccGet_GPUADC10(pDev, provIdx, pTupleAcc);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevTupleAccGet_GPUADC11(pDev, provIdx, pTupleAcc);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevTupleAccGet_GPUADC13(pDev, provIdx, pTupleAcc);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Relays the 'voltageSetChanged' request to the appropriate power-device.
 *
 * @copydoc PwrDevVoltageSetChanged
 */
FLCN_STATUS
pwrDevVoltageSetChanged
(
    BOARDOBJ *pBoardObj,
    LwU32     voltageuV
)
{
    switch (pBoardObj->type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevVoltageSetChanged_BA1XHW(pBoardObj, voltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevVoltageSetChanged_BA1XHW(pBoardObj, voltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevVoltageSetChanged_BA1XHW(pBoardObj, voltageuV);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevVoltageSetChanged_BA1XHW(pBoardObj, voltageuV);
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Set a power/current limit for the device to apply to the rail(s)
 * it is connected to.
 *
 * @copydoc PwrDevSetLimit
 */
FLCN_STATUS
pwrDevSetLimit
(
    PWR_DEVICE *pDev,
    LwU8        provIdx,
    LwU8        limitIdx,
    LwU8        limitUnit,
    LwU32       limitValue
)
{
    switch (pDev->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            return pwrDevSetLimit_INA3221(pDev, provIdx, limitIdx, limitUnit,
                                          limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_NCT3933U);
            return pwrDevSetLimit_NCT3933U(pDev, provIdx, limitIdx, limitUnit,
                                           limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevSetLimit_BA1XHW(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevSetLimit_BA1XHW(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevSetLimit_BA1XHW(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevSetLimit_BA15HW(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA16HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA16);
            return pwrDevSetLimit_BA16HW(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA20:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA20);
            return pwrDevSetLimit_BA20(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevSetLimit_GPUADC10(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevSetLimit_GPUADC11(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevSetLimit_GPUADC13(pDev, provIdx, limitIdx, limitUnit,
                                         limitValue);
    }
    PMU_HALT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Performs periodic updates of the power device's internal state.
 *
 * @copydoc PwrDevStateSync
 */
FLCN_STATUS
pwrDevStateSync
(
    PWR_DEVICE *pDev
)
{
    if (pDev == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_INDEX;
    }

    switch (BOARDOBJ_GET_TYPE(pDev))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevStateSync_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevStateSync_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevStateSync_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevStateSync_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA16HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA16);
            return pwrDevStateSync_BA16HW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA20:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA20);
            return pwrDevStateSync_BA20(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevStateSync_GPUADC10(pDev);
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrDevGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PWR_DEVICE                                  *pDev;
    FLCN_STATUS                                  status;
    RM_PMU_PMGR_PWR_DEVICE_DESC_BOARDOBJ_SET    *pSet =
        (RM_PMU_PMGR_PWR_DEVICE_DESC_BOARDOBJ_SET *)pBoardObjDesc;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }

    pDev = (PWR_DEVICE *)*ppBoardObj;

    pDev->powerCorrFactor = pSet->powerCorrFactor;

    return FLCN_OK;
}

/*!
 * PWR_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
pwrDeviceBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET_AUTO_DMA(E32,      // _grpType
        PWR_DEVICE,                                 // _class
        pBuffer,                                    // _pBuffer
        pmgrPwrDevIfaceModel10SetHeader,                  // _hdrFunc
        pmgrPwrDevIfaceModel10SetEntry,                   // _entryFunc
        all.pmgr.pwrDeviceGrpSet,                   // _ssElement
        OVL_INDEX_DMEM(pmgr),                       // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * PWR_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_GET_STATUS interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
pwrDeviceBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, perfVf)
        OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PWR_MONITOR_SEMAPHORE_TAKE;
        {
            status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS_AUTO_DMA(E32,   // _grpType
                PWR_DEVICE,                                 // _class
                pBuffer,                                    // _pBuffer
                NULL,                                       // _hdrFunc
                pwrDevIfaceModel10GetStatus,                            // _entryFunc
                all.pmgr.pwrDeviceGrpGetStatus);            // _ssElement
        }
        PWR_MONITOR_SEMAPHORE_GIVE;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
pmgrPwrDevIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    FLCN_STATUS status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto pmgrPwrDevIfaceModel10SetHeader_exit;
    }

    // Copy in BA info.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_BA))
    {
        Pmgr.baInfo.bInitializedAndUsed =
            ((RM_PMU_PMGR_PWR_DEVICE_BOARDOBJGRP_SET_HEADER *)pHdrDesc)->baInfo.bInitializedAndUsed;
        Pmgr.baInfo.utilsClkkHz =
            ((RM_PMU_PMGR_PWR_DEVICE_BOARDOBJGRP_SET_HEADER *)pHdrDesc)->baInfo.utilsClkkHz;
    }

pmgrPwrDevIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * Relays the 'construct' request to the appropriate PWR_DEVICE implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
pmgrPwrDevIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA219:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA219);
            return pwrDevGrpIfaceModel10ObjSetImpl_INA219(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_INA219), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            return pwrDevGrpIfaceModel10ObjSetImpl_INA3221(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_INA3221), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_NCT3933U);
            return pwrDevGrpIfaceModel10ObjSetImpl_NCT3933U(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_NCT3933U), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevGrpIfaceModel10ObjSetImpl_BA1XHW(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_BA1XHW), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevIfaceModel10SetImpl_BA13HW(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_BA13HW), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevGrpIfaceModel10ObjSetImpl_BA14HW(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_BA14HW), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevGrpIfaceModel10ObjSetImpl_BA15HW(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_BA15HW), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA16HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA16);
            return pwrDevGrpIfaceModel10ObjSetImpl_BA16HW(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_BA16HW), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA20:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA20);
            return pwrDevGrpIfaceModel10ObjSetImpl_BA20(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_BA20), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGrpIfaceModel10ObjSetImpl_GPUADC10(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_GPUADC10), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGrpIfaceModel10ObjSetImpl_GPUADC11(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_GPUADC11), pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGrpIfaceModel10ObjSetImpl_GPUADC13(pModel10, ppBoardObj,
                sizeof(PWR_DEVICE_GPUADC13), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrDevIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    return pwrDevIfaceModel10GetStatus_SUPER(pModel10, pPayload);
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrDevIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *pPwrDevStatus =
        (RM_PMU_PMGR_PWR_DEVICE_BOARDOBJ_GET_STATUS *)pPayload;
    PWR_DEVICE                 *pDev   = (PWR_DEVICE *)pBoardObj;
    FLCN_STATUS                 status = FLCN_OK;
    LW2080_CTRL_PMGR_PWR_TUPLE *pStatus;
    LwU8                        provIdx;
    LwU8                        limitIdx;

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrDevIfaceModel10GetStatus_SUPER_exit;
    }

    pPwrDevStatus->numProviders = s_pwrDevProvNumGet(pDev);

    for (provIdx = 0; provIdx < pPwrDevStatus->numProviders; provIdx++)
    {
        // Get the tuple value for requested provider of PWR_DEVICE
        pStatus = &(pPwrDevStatus->providers[provIdx].tuple);
        status = pwrDevTupleGet(pDev, provIdx, pStatus);

        //
        // If a provider is not supported, do not throw error, continue to next
        // provider instead.
        //
        if (status == FLCN_ERR_NOT_SUPPORTED)
        {
            status = FLCN_OK;
            continue;
        }
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrDevIfaceModel10GetStatus_SUPER_exit;
        }

        // Get the number of thresholds and get limit value for each threshold
        pPwrDevStatus->providers[provIdx].numThresholds =
            s_pwrDevThresholdNumGet(pDev);
        for (limitIdx = 0;
             limitIdx < pPwrDevStatus->providers[provIdx].numThresholds;
             limitIdx++)
        {
            status = pwrDevGetLimit(pDev, provIdx, limitIdx, pPayload);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrDevIfaceModel10GetStatus_SUPER_exit;
            }
        }
    }

pwrDevIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * PowerDev Update routine
 *
 * This function updates all power devices for voltage change.
 */
void pwrDevVoltStateSync(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_DEVICE_VOLT_STATE_SYNC))
    {
        s_pwrDevOsCallback(NULL);
    }
}

/*!
 * @copydoc PwrDevGetLimit
 */
FLCN_STATUS
pwrDevGetLimit
(
    PWR_DEVICE            *pDev,
    LwU8                   provIdx,
    LwU8                   limitIdx,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    switch (pDev->super.type)
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_NCT3933U);
            return pwrDevGetLimit_NCT3933U(pDev, provIdx, limitIdx, pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevGetLimit_GPUADC10(pDev, provIdx, limitIdx, pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevGetLimit_GPUADC11(pDev, provIdx, limitIdx, pBuf);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevGetLimit_GPUADC13(pDev, provIdx, limitIdx, pBuf);
    }

    return FLCN_OK;
}

/* ------------------------- Static Functions ------------------------------- */

/*!
 * @ref     OsTmrCallbackFunc
 *
 * PowerDev Update routine.
 */
static FLCN_STATUS
s_pwrDevOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwU8            d;
    OSTASK_OVL_DESC ovlDescList[]  = {
        OSTASK_OVL_DESC_DEFINE_PWR_DEVICE_STATE_SYNC
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        FOR_EACH_INDEX_IN_MASK(32, d, Pmgr.pwr.devices.objMask.super.pData[0])
        {
            (void)pwrDevStateSync(PWR_DEVICE_GET(d));
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK; // NJ-TODO-CB
}

/*!
 * @ref     OsTimerCallback
 *
 * PowerDev Update routine.
 */
static void
s_pwrDevCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_pwrDevOsCallback(NULL);
}

#if (!PMUCFG_FEATURE_ENABLED(PMU_PMGR_CALLBACKS_CENTRALIZED))
/*!
 * Check the request from @ref pwrDevProcessPerfStatus and schedule new
 * evaluation callback accordingly.
 *
 * @param[in] bLowSampling  Boolean indicating request type
 */
static void
_pwrDevCheckAndScheduleLowSamplingCallback
(
    LwBool bLowSampling
)
{
    OS_TIMER_ENTRY *pEntry     =
        &PmgrOsTimer.pEntries[PMGR_OS_TIMER_ENTRY_PWR_DEV_CALLBACK];
    LwU32           ticksDelay = pEntry->ticksDelay;

    //
    // If requesting to use low sampling and current sampling is normal, switch
    // to use low sampling frequency.
    //
    if (bLowSampling && (ticksDelay == (OS_TIMER_DELAY_1_MS * 200)))
    {
        pwrDevScheduleCallback(pEntry->ticksPrev, LW_TRUE);
    }

    //
    // On the other side, if we are requesting to use normal sampling and
    // current sampling is low, switch to normal sampling.
    //
    if (!bLowSampling && (ticksDelay > (OS_TIMER_DELAY_1_MS * 200)))
    {
        pwrDevScheduleCallback(pEntry->ticksPrev, LW_FALSE);
    }
}
#endif

/*!
 * Helper function to return the number of supported providers.
 *
 * @param[in] pDev Pointer to the PWR_DEVICE object
 *
 * @return Number of PWR_DEVICE providers supported by this PWR_DEVICE
 *     class/object.
 */
static LwU8
s_pwrDevProvNumGet
(
    PWR_DEVICE *pDev
)
{
    switch (BOARDOBJ_GET_TYPE(pDev))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevProvNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevProvNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevProvNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevProvNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA16HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA16);
            return pwrDevProvNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA20:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA20);
            return pwrDevProvNumGet_BA20(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            return pwrDevProvNumGet_INA3221(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_NCT3933U);
            return pwrDevProvNumGet_NCT3933U(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevProvNumGet_GPUADC10(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevProvNumGet_GPUADC11(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevProvNumGet_GPUADC13(pDev);
    }

    return RM_PMU_PMGR_PWR_DEVICE_DEFAULT_PROV_NUM;
}

/*!
 * Helper function to return the number of supported thresholds.
 *
 * @param[in] pDev    Pointer to the PWR_DEVICE object
 *
 * @return Number of thresholds supported by the PWR_DEVICE.
 */
static LwU8
s_pwrDevThresholdNumGet
(
    PWR_DEVICE *pDev
)
{
    switch (BOARDOBJ_GET_TYPE(pDev))
    {
        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA12HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA12X);
            return pwrDevThresholdNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA13HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA13X);
            return pwrDevThresholdNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA14HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA14);
            return pwrDevThresholdNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA15HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA15);
            return pwrDevThresholdNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA16HW:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA16);
            return pwrDevThresholdNumGet_BA1XHW(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_BA20:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_BA20);
            return pwrDevThresholdNumGet_BA20(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_INA3221:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_INA3221);
            return pwrDevThresholdNumGet_INA3221(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_NCT3933U:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_NCT3933U);
            return pwrDevThresholdNumGet_NCT3933U(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC10:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC10);
            return pwrDevThresholdNumGet_GPUADC10(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC11:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC11);
            return pwrDevThresholdNumGet_GPUADC11(pDev);

        case LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_GPUADC13:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_DEVICE_GPUADC13);
            return pwrDevThresholdNumGet_GPUADC13(pDev);
    }

    return RM_PMU_PMGR_PWR_DEVICE_DEFAULT_THRESHOLD_NUM;
}
