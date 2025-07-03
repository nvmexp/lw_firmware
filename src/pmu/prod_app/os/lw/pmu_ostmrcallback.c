/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_ostmrcallback.c
 * @brief   PMU falcon specific OS_TMR_CALLBACK hooks.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lwostmrcallback.h"
#include "pmu_objic.h"
#include "pmu_objpg.h"

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Private Functions ------------------------------- */
/* ------------------------ Implementation ---------------------------------- */
/*!
 * @brief   Notifies respective task that the callback requires servicing.
 *
 * @param[in]   pCallback   Handle of the callback that requires servicing.
 *
 * @return  Boolean if task notification has succeeded.
 *
 * @pre Caller is responsible to assure that pCallback is not NULL.
 */
LwBool
osTmrCallbackNotifyTaskISR
(
    OS_TMR_CALLBACK *pCallback
)
{
    DISP2UNIT_OS_TMR_CALLBACK callback;
    FLCN_STATUS               status;
    LwBool                    bSuccess = LW_FALSE;

    callback.hdr.eventType = DISP2UNIT_EVT_OS_TMR_CALLBACK;
    callback.pCallback = pCallback;

    status = lwrtosQueueSendFromISRWithStatus(pCallback->queueHandle, &callback,
                                              sizeof(callback));
    if (status == FLCN_OK)
    {
        LWOS_WATCHDOG_ADD_ITEM_FROM_ISR(pCallback->taskId);
        bSuccess = LW_TRUE;
    }
    else if (FLCN_ERR_TIMEOUT != status)
    {
        OS_BREAKPOINT();
        goto osTmrCallbackNotifyTaskISR_exit;
    }

osTmrCallbackNotifyTaskISR_exit:
    return bSuccess;
}

/*!
 * @brief  Returns the max number of callbacks that can be registered. This varies
 *         with profile and needs to be updated if a new callback is created
 */
LwU32
osTmrCallbackMaxCountGet()
{
    LwU32 cbCount = 0;

#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAINS_PMUMON))
    cbCount++; // OsCBClkDomainsPmumon
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_EFFECTIVE_AVG))
    cbCount++; // OsCBClkFreqEffectiveSample
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_CONTROLLER))
    cbCount++; // OsCBClkController
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_PCM_CALLBACK))
    cbCount++; // OsCBPcm
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_PERF_CALLBACK))
    cbCount++; // OsCBClkCntrSwCntr
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
    cbCount++; // OsCBClkFreqController10
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI))
    cbCount++; // OsCBEi
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CENTRALISED_CALLBACK) && PMUCFG_FEATURE_ENABLED(PMU_AP))
    cbCount++; // OsCBLpwr
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_TEST))
    cbCount++; // lpwrTestCallback
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PSI_PMGR_SLEEP_CALLBACK))
    cbCount++; // OsCBPsi
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
    cbCount++; // OsCBPwrChannels
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNELS_PMUMON))
    cbCount++; // OsCBPwrChannelsPmuMon
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_CHANNEL_PMUMON))
    cbCount++; // OsCBThermChannelPmuMon
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_MONITOR))
    cbCount++; // OsCBThermMonitor
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_PMUMON_WAR_3076546))
    cbCount++; // OsCBThermPmumon
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_RAILS_PMUMON))
    cbCount++; // OsCBVoltRailsPmumon
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_COOLER_PMUMON))
    cbCount++; // OsCBFanCoolerPmuMon
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGIES_PMUMON))
    cbCount++; // OsCBPerfCfTopologiesPmumon
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_PMUMON)
    cbCount++; // OsCBPerfPolicyPmuMon
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_POLICY_V20)
    cbCount++; // OsCBFan
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
    cbCount++; // OsCBPwrPolicy
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_FAN_ARBITER)
    cbCount++; // OsCBFanArbiter
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE)
    cbCount++; // OsCBVfeTimer.super
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_DEVICES_EXTENDED)
    cbCount++; // OsCBAdcVoltageSample
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_DISP_CHECK_VRR_HACK_BUG_1604175) && PMUCFG_FEATURE_ENABLED(PMUTASK_DISP)
    cbCount++; // OsCBDisp
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_TOPOLOGY)
    cbCount++; // OsCBPerfCfTopologys.super
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER)
    cbCount++; // OsCBPerfCfControllers.super
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
    cbCount++; // OsCBPwrDev
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_THERM_POLICY)
    cbCount++; // OsCBThermPolicy
#endif
#if PMUCFG_FEATURE_ENABLED(PMUTASK_PERFMON)
    cbCount++; // OsCBPerfMon
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_BIF_XUSB)
    cbCount++; // OsCBBifXusbIsochTimeout
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE)
    cbCount++; // OsCBPerfCfControllerMemTuneMonitor
#endif
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSORS_PMUMON)
    cbCount++; // OsCBPerfCfPmSensorsPmumon
#endif

    return cbCount;
}
