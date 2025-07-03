/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_callback.c
 * @brief  Implements centralized LPWR callback
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objap.h"
#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED)
OS_TMR_CALLBACK OsCBLpwr;
#endif // PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED)

/* ------------------------- Prototypes ------------------------------------- */
static void s_lpwrCallbackSchedule(void);
static void s_lpwrCallbackDeschedule(void);
static void s_lpwrCallbackCtrlExelwte(LwU8 ctrlId);

static void s_lpwrCallbackSacArbiter_v1(void);
static void s_lpwrCallbackSacArbiter_v2(void);

static OsTmrCallbackFunc (s_lpwrHandleOsCallback)
                         GCC_ATTRIB_USED();
static OsTimerCallback   (s_lpwrHandleCallback)
                         GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Construct centralized PG callback
 *
 * @return  FLCN_OK                 On success
 * @return  FLCN_ERR_NO_FREE_MEM    If malloc() fails
 */
FLCN_STATUS
lpwrCallbackConstruct(void)
{
    LwU32 ctrlId;
    FLCN_STATUS status = FLCN_OK;

    Pg.pCallback = lwosCallocResident(sizeof(LPWR_CALLBACK));
    if (Pg.pCallback == NULL)
    {
        status = FLCN_ERR_NO_FREE_MEM;
        goto lpwrCallbackConstruct_exit;
    }

    // Allocate one legacy callback
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTimerInitTracked(OSTASK_TCB(LPWR), &Pg.osTimer, 1);
#endif

    // Default initialization
    Pg.pCallback->basePeriodUs =
            RM_PMU_LPWR_VBIOS_CALLBACK_BASE_PERIOD_DEFAULT_MS * 1000;

    for (ctrlId = 0; ctrlId < LPWR_CALLBACK_ID__COUNT; ctrlId++)
    {
        Pg.pCallback->callbackCtrl[ctrlId].baseMultiplier =
                    RM_PMU_LPWR_VBIOS_CALLBACK_BASE_MULTIPLIER_DEFAULT;
    }

lpwrCallbackConstruct_exit:
    return status;
}

/*!
 * @brief Schedule controller callback
 *
 * @param[in] ctrlId    LPWR callback controller ID
 */
void
lpwrCallbackCtrlSchedule
(
    LwU8 ctrlId
)
{
    LPWR_CALLBACK *pCallback = LPWR_GET_CALLBACK();

    PMU_HALT_COND(ctrlId < LPWR_CALLBACK_ID__COUNT);

    // Schedule centralized callback
    if (pCallback->scheduledMask == 0)
    {
        s_lpwrCallbackSchedule();
    }

    // Set the bit in scheduledMask
    pCallback->scheduledMask |= BIT(ctrlId);
}

/*!
 * @brief Deschedule controller callback
 *
 * @param[in] ctrlId    LPWR callback controller ID
 */
void
lpwrCallbackCtrlDeschedule
(
    LwU8 ctrlId
)
{
    LPWR_CALLBACK *pCallback = LPWR_GET_CALLBACK();

    PMU_HALT_COND(ctrlId < LPWR_CALLBACK_ID__COUNT);

    // Centralised callback is scheduled when scheduledMask is non-zero
    if (pCallback->scheduledMask != 0)
    {
        // Clear the bit in scheduledMask
        pCallback->scheduledMask &= (~BIT(ctrlId));

        // Deschedule centralised callback
        if (pCallback->scheduledMask == 0)
        {
            s_lpwrCallbackDeschedule();
        }
    }
}

/*!
 * @brief Get callback period in usec
 *
 * @param[in] ctrlId    LPWR callback controller ID
 *
 * @return    LwU32     Callback period in usec
 */
LwU32
lpwrCallbackCtrlGetPeriodUs
(
    LwU8 ctrlId
)
{
    LPWR_CALLBACK     *pCallback     = LPWR_GET_CALLBACK();
    LPWR_CALLBACKCTRL *pCallbackCtrl = LPWR_GET_CALLBACKCTRL(ctrlId);

    PMU_HALT_COND(ctrlId < LPWR_CALLBACK_ID__COUNT);

    return (pCallback->basePeriodUs * pCallbackCtrl->baseMultiplier);
}

/*!
 * @brief Notification when MS goes in Full Power mode
 *
 * This function exelwtes the pending callback on MS wake-up
 */
void
lpwrCallbackNotifyMsFullPower(void)
{
    LwU32          callbackCtrlId = 0;
    LPWR_CALLBACK *pCallback      = LPWR_GET_CALLBACK();

    // Execute the pending callbacks
    FOR_EACH_INDEX_IN_MASK(32, callbackCtrlId, pCallback->exelwtionPendingMask)
    {
        s_lpwrCallbackCtrlExelwte(callbackCtrlId);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Reset the pending mask
    pCallback->exelwtionPendingMask = 0;
    pCallback->bWakeMsInProgress    = LW_FALSE;
}

/*!
 * @brief Sleep Aware Callback (SAC) arbiter
 *
 * Arbiter sets callback mode based on request coming from different clients.
 * We have two versions of arbiter.
 * Version 1: Doc @ s_lpwrCallbackSacArbiter_v1()
 * Version 2: Doc @ s_lpwrCallbackSacArbiter_v2()
 *
 * @param[in] clientId  Sleep Aware Callback client ID LPWR_SAC_CLIENT_ID_xyz
 * @param[in] bSleep    LW_TRUE if client is requesting sleep mode
 *                      LW_FALSE otherwise
 */
void
lpwrCallbackSacArbiter
(
    LwU32  clientId,
    LwBool bSleep
)
{
    if (clientId >= LPWR_SAC_CLIENT_ID__COUNT)
    {
        PMU_BREAKPOINT();
        return;
    }

    // Cache the client request
    Lpwr.bSleepClientReq[clientId] = bSleep;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SAC_V1))
    {
        s_lpwrCallbackSacArbiter_v1();
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SAC_V2))
    {
        s_lpwrCallbackSacArbiter_v2();
    }
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Execute the callback for given controller
 *
  * @param[in] ctrlId    LPWR callback controller ID
 */
static void
s_lpwrCallbackCtrlExelwte
(
    LwU8 ctrlId
)
{

    if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR) &&
        ctrlId == LPWR_CALLBACK_ID_AP_GR)
    {
        apCtrlExelwte(RM_PMU_AP_CTRL_ID_GR);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_DI_DEEP_L1) &&
             ctrlId == LPWR_CALLBACK_ID_AP_DI)
    {
        apCtrlExelwte(RM_PMU_AP_CTRL_ID_DI_DEEP_L1);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG) &&
             ctrlId == LPWR_CALLBACK_ID_AP_MSCG)
    {
        apCtrlExelwte(RM_PMU_AP_CTRL_ID_MSCG);
    }
    else
    {
        PMU_HALT();
    }
}

/*!
 * @ref OsTmrCallback
 *
 * @brief   Handles centralised LPWR callback
 *
 * @param[in] pCallback    Pointer to OS_TMR_CALLBACK
 */
static FLCN_STATUS
s_lpwrHandleOsCallback
(
    OS_TMR_CALLBACK *pOsCallback
)
{
    OBJPGSTATE        *pMsState      = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LPWR_CALLBACK     *pCallback     = LPWR_GET_CALLBACK();
    LPWR_CALLBACKCTRL *pCallbackCtrl = NULL;
    LwU32              ctrlId;
    LwBool             bIsoMsFullPower;

    //
    // ISO-MS is in Full power if -
    // 1) ISO-MSCG is not supported                 OR
    // 2) ISO-MSCG is supported but MSCG is not engaged
    //
    // Referring support state of ISO_STUTTER rather than enablement state.
    // Checking enablement state might lead to underflow for MSCG oneshot.
    // E.g. LPWR is exelwting the callback and display send NISO_END event.
    // NISO_END request will get defer.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_MS)              &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, ISO_STUTTER))
    {
        bIsoMsFullPower = PG_IS_FULL_POWER(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    }
    else
    {
        bIsoMsFullPower = LW_TRUE;
    }

    FOR_EACH_INDEX_IN_MASK(32, ctrlId, pCallback->scheduledMask)
    {
        pCallbackCtrl = LPWR_GET_CALLBACKCTRL(ctrlId);

        pCallbackCtrl->baseCount++;

        // Execute the callback if base count is overflowing
        if (pCallbackCtrl->baseCount >=  pCallbackCtrl->baseMultiplier)
        {
            //
            // Execute the callback if ISO-MSCG is in full power OR Immediate
            // exelwtion mode is enabled. Otherwise delay the callback execute.
            //
            if (pCallbackCtrl->bImmediateExelwtion ||
                bIsoMsFullPower)
            {
                s_lpwrCallbackCtrlExelwte(ctrlId);
            }
            else
            {
                pCallback->exelwtionPendingMask |= BIT(ctrlId);
            }

            // Reset the base counter
            pCallbackCtrl->baseCount = 0;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Wake MSCG if there are pending callback requests
    if ((PMUCFG_FEATURE_ENABLED(PMU_PG_MS)) &&
        (!pCallback->bWakeMsInProgress)     &&
        (pCallback->exelwtionPendingMask != 0))
    {
        pgCtrlEventSend(RM_PMU_LPWR_CTRL_ID_MS_MSCG, PMU_PG_EVENT_WAKEUP,
                        PG_WAKEUP_EVENT_MS_CALLBACK);

        pCallback->bWakeMsInProgress = LW_TRUE;
    }

    return FLCN_OK;
}

/*!
 * @ref     OsTimerCallback
 *
 * @brief   Handles centralised LPWR callback
 */
static void
s_lpwrHandleCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_lpwrHandleOsCallback(NULL);
}

/*!
 * @brief Schedule LPWR_CALLBACK
 */
static void
s_lpwrCallbackSchedule(void)
{
    LPWR_CALLBACK *pCallback = LPWR_GET_CALLBACK();

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    if (!OS_TMR_CALLBACK_WAS_CREATED(&OsCBLpwr))
    {
        osTmrCallbackCreate(&OsCBLpwr,                                  // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                OVL_INDEX_ILWALID,                                      // ovlImem
                s_lpwrHandleOsCallback,                                 // pTmrCallbackFunc
                LWOS_QUEUE(PMU, LPWR),                                  // queueHandle
                pCallback->basePeriodUs,                                // periodNormalus
                pCallback->basePeriodUs,                                // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                RM_PMU_TASK_ID_LPWR);                                   // taskId
    }
    osTmrCallbackSchedule(&OsCBLpwr);
#else
    osTimerScheduleCallback(
            &Pg.osTimer,                                                // pOsTimer
            0,                                                          // index
            lwrtosTaskGetTickCount32(),                                 // ticksPrev
            pCallback->basePeriodUs,                                    // usDelay
            DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_SKIP),   // flags
            s_lpwrHandleCallback,                                       // pCallback
            NULL,                                                       // pParam
            OVL_INDEX_ILWALID);                                         // ovlIdx
#endif
}

/*!
 * @brief Deschedule LPWR_CALLBACK
 */
static void
s_lpwrCallbackDeschedule(void)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCancel(&OsCBLpwr);
#else
    osTimerDeScheduleCallback(&Pg.osTimer, 0);
#endif
}

/*!
 * @brief Sleep Aware Callback (SAC) arbiter version 1
 *
 * This arbiter responds to request from following clients
 * - RM
 * - GR
 *
 * Version 1 do not consider pstate client:
 * Version 1 is enabled on Pascal and Turing GPUs. On Pascal, RM manages LPWR
 * feature interaction with pstate. Thus we decided not to consider the pstate
 * client in version 1.
 */
static void
s_lpwrCallbackSacArbiter_v1(void)
{
    if (Lpwr.bSleepClientReq[LPWR_SAC_CLIENT_ID_RM] ||
        Lpwr.bSleepClientReq[LPWR_SAC_CLIENT_ID_GR])
    {
        OS_TMR_CALLBACK_MODE_SET(SLEEP_ALWAYS);
    }
    else
    {
        OS_TMR_CALLBACK_MODE_SET(NORMAL);
    }
}

/*!
 * @brief Sleep Aware Callback (SAC) arbiter version 2
 *
 * This arbiter responds to request from following clients
 * - EI
 * - EI_PSTATE
 * - GR
 * - GR_PSTATE
 *
 * EI coupled sleep aware callback has higher priority than GR coupled sleep
 * aware callback.
 */
static void
s_lpwrCallbackSacArbiter_v2(void)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_SAC_V2))

    //
    // EI coupled sleep aware callback gets priority over GR coupled sleep
    // aware callback
    //
    if (Lpwr.bSleepClientReq[LPWR_SAC_CLIENT_ID_EI] &&
        Lpwr.bSleepClientReq[LPWR_SAC_CLIENT_ID_EI_PSTATE])
    {
        OS_TMR_CALLBACK_MODE_SET(SLEEP_ALWAYS);
    }
    else if (Lpwr.bSleepClientReq[LPWR_SAC_CLIENT_ID_GR] &&
             Lpwr.bSleepClientReq[LPWR_SAC_CLIENT_ID_GR_PSTATE])
    {
        OS_TMR_CALLBACK_MODE_SET(SLEEP_RELAXED);
    }
    else
    {
        OS_TMR_CALLBACK_MODE_SET(NORMAL);
    }

#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_SAC_V2)
}
