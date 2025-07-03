/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_timer.c
 * @brief  Hdcp22 OS timer wrapper Functions
 *
 * Implement helper functions to support Hdcp22 OS timer usages.
 */
/* ------------------------ System includes --------------------------------- */
#include "lwrtos.h"
#include "lwostimer.h"
#include "lwostask.h"
#include "lwoslayer.h"

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_timer.h"
#include "lib_intfchdcp22wired.h"

/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
#if !(defined(GSPLITE_RTOS) && (FLCNLIBCFG_FEATURE_ENABLED(HDCP22WIRED_OS_CALLBACK_CENTRALISED)))
extern OS_TIMER     Hdcp22WiredOsTimer;
#endif
extern LwrtosQueueHandle Hdcp22WiredQueue;

/* ------------------------ Static variables -------------------------------- */
#if defined(GSPLITE_RTOS) && (FLCNLIBCFG_FEATURE_ENABLED(HDCP22WIRED_OS_CALLBACK_CENTRALISED))
static OS_TMR_CALLBACK Hdcp22WiredOsTmrCb;
#endif
//
// Vars to cross check internal hddcp22 timer usage, and LW_PDISP_AUDIT is secure
// timer can be trusted.
//
static LwU32 gHdcp22TimerStartStampUs;
static LwU32 gHdcp22TimerIntervalUs;

/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Prototypes  ------------------------------------- */
// Timer callback should be put to resident.
#if defined(GSPLITE_RTOS) && (FLCNLIBCFG_FEATURE_ENABLED(HDCP22WIRED_OS_CALLBACK_CENTRALISED))
static void _hdcp22wiredTimerCallback(OS_TMR_CALLBACK *pCallback)
#else
static void _hdcp22wiredTimerCallback(void *pParam, LwU32 ticksExpired, LwU32 skipped)
#endif
    GCC_ATTRIB_SECTION("imem_resident", "_hdcp22wiredTimerCallback")
    GCC_ATTRIB_USED();

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @brief This function is callback handler of hdcp22 timer that
 * sends timeout event to hdcp22 task.
 *
 * @param[in]   pParam          Parameter provided at callback scheduling time.
 * @param[in]   ticksExpired    OS ticks time when callback expired.
 * @param[in]   skipped         Number of skipped callbacks due to @ref
 *      LW_OS_TIMER_FLAGS_RELWRRING == _YES_MISSED_SKIP (otherwise zero).
 */
static void
_hdcp22wiredTimerCallback
(
#if defined(GSPLITE_RTOS) && (FLCNLIBCFG_FEATURE_ENABLED(HDCP22WIRED_OS_CALLBACK_CENTRALISED))
    OS_TMR_CALLBACK *pCallback
#else
    void    *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
#endif
)
{
    DISPATCH_HDCP22WIRED disp2unitEvt;
    LwU32 lwrrentTimeUs;

    if (FLCN_OK == hdcp22wiredReadAuditTimer_HAL(&Hdcp22wired, &lwrrentTimeUs))
    {
        LwU32 timeElapsedUs;

        if (lwrrentTimeUs > gHdcp22TimerStartStampUs)
        {
            timeElapsedUs = (LwU32)(lwrrentTimeUs - gHdcp22TimerStartStampUs);
        }
        else
        {
            timeElapsedUs = (LwU32)(gHdcp22TimerStartStampUs - lwrrentTimeUs + 1);
        }

        if (((timeElapsedUs >= gHdcp22TimerIntervalUs) &&
                (LwU32)(timeElapsedUs - gHdcp22TimerIntervalUs) >
                    HDCP22_TIMER_TOLERANCE(gHdcp22TimerIntervalUs)) ||
            ((timeElapsedUs < gHdcp22TimerIntervalUs) &&
                (LwU32)(gHdcp22TimerIntervalUs - timeElapsedUs) >
                    HDCP22_TIMER_TOLERANCE(gHdcp22TimerIntervalUs)))
        {
            disp2unitEvt.hdcp22Evt.eventInfo.timerEvtType = HDCP22_TIMER_ILLEGAL_EVT;
        }
        else
        {
            disp2unitEvt.hdcp22Evt.eventInfo.timerEvtType = HDCP22_TIMER_LEGAL_EVT;
        }
    }
    else
    {
        // If LW_PDISP_AUDIT not supported, always return legal event.
        disp2unitEvt.hdcp22Evt.eventInfo.timerEvtType = HDCP22_TIMER_LEGAL_EVT;
    }

    disp2unitEvt.eventType = DISP2UNIT_EVT_SIGNAL;
    disp2unitEvt.hdcp22Evt.subType = HDCP22_TIMER0_INTR;

    // Ignore return value purposely because timerCallback no return value constraint.
    (void)lwrtosQueueSendBlocking(Hdcp22WiredQueue, &disp2unitEvt,
                                  sizeof(disp2unitEvt));
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief     This function start the timer function
 * @param[in] intervalUs   timer interval in usec
 * @return    FLCN_STATUS  Returns FLCN_OK if succeed else error code.
 */
FLCN_STATUS
hdcp22wiredStartTimer
(
    LwU32 intervalUs
)
{
    FLCN_STATUS status = FLCN_OK;

    status = hdcp22wiredReadAuditTimer_HAL(&Hdcp22wired,
                                           &gHdcp22TimerStartStampUs);
    if (status == FLCN_OK)
    {
        gHdcp22TimerIntervalUs = intervalUs;
    }
    else if (status == FLCN_ERR_NOT_SUPPORTED)
    {
        //
        // If audit timer not supported, continue with overriding status.
        // Meanwhile, no need to save gHdcp22TimerIntervalUs to check at
        // timer callback handler.
        //
        status = FLCN_OK;
    }
    else
    {
        // Otherwise, return with error code.
        goto label_return;
    }

#if defined(GSPLITE_RTOS) && (FLCNLIBCFG_FEATURE_ENABLED(HDCP22WIRED_OS_CALLBACK_CENTRALISED))
    if (!OS_TMR_CALLBACK_WAS_CREATED(&Hdcp22WiredOsTmrCb))
    {
        CHECK_STATUS(osTmrCallbackCreate(&Hdcp22WiredOsTmrCb,                                   // pCallback
                                         OS_TMR_CALLBACK_TYPE_ONE_SHOT,                         // type
                                         OVL_INDEX_ILWALID,                                     // ovlImem
                                         (OsTmrCallbackFunc (*))(_hdcp22wiredTimerCallback),    // pTmrCallbackFunc
                                         Hdcp22WiredQueue,                                      // queueHandle
                                         intervalUs,                                            // periodNormalus
                                         intervalUs,                                            // periodSleepus
                                         OS_TIMER_RELAXED_MODE_USE_NORMAL,                      // bRelaxedUseSleep
                                         RM_PMU_TASK_ID_HDCP));                                 // taskId
        status = osTmrCallbackSchedule(&Hdcp22WiredOsTmrCb);
    }
    else
    {
        //
        // stopTimer to cancel timerCallback may change state _CREATED and need
        // put back to schedule list.
        //
        CHECK_STATUS(osTmrCallbackUpdate(&Hdcp22WiredOsTmrCb,
                                         intervalUs,
                                         intervalUs,
                                         OS_TIMER_RELAXED_MODE_USE_NORMAL,
                                         OS_TIMER_UPDATE_USE_BASE_LWRRENT));
        status = osTmrCallbackSchedule(&Hdcp22WiredOsTmrCb);
    }
#else
    {
        LwU8 index;

        index = osTimerScheduleCallback(&Hdcp22WiredOsTimer,                            // pOsTimer
                                        HDCP22WIRED_OS_TIMER_ENTRY_CALLBACKS,           // index
                                        lwrtosTaskGetTickCount32(),                     // ticksPrev
                                        intervalUs,                                     // usDelay
                                        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _NO),    // flags
                                        _hdcp22wiredTimerCallback,                      // pCallback
                                        NULL,                                           // pParam
                                        OVL_INDEX_ILWALID);                             // ovlIdx
        if (index == OS_TIMER_MAX_ENTRIES)
        {
            status = FLCN_ERR_ILWALID_INDEX;
        }
    }
#endif

label_return:
    return status;
}

/*!
 * @brief  This function stops timer function
 * @return  FLCN_OK                 If succeed.
 *          FLCN_ERR_NOT_SUPPORTED  If fail to cancel timer.
 *          FLCN_ERR_ILWALID_INDEX  If the requested index exceeds the number
 *                                  of entries supported by this OS_TIMER.
 */
FLCN_STATUS
hdcp22wiredStopTimer(void)
{
    FLCN_STATUS status = FLCN_OK;

#if defined(GSPLITE_RTOS) && (FLCNLIBCFG_FEATURE_ENABLED(HDCP22WIRED_OS_CALLBACK_CENTRALISED))
    if (!osTmrCallbackCancel(&Hdcp22WiredOsTmrCb))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
    }
#else
    {
        LwU8 index;

        index = osTimerDeScheduleCallback(&Hdcp22WiredOsTimer,
                                          HDCP22WIRED_OS_TIMER_ENTRY_CALLBACKS);
        if (index == OS_TIMER_MAX_ENTRIES)
        {
            status = FLCN_ERR_ILWALID_INDEX;
        }
    }
#endif

    return status;
}

/*!
 * @brief This is a wrapper function to measure the current time.
 *
 * @param[out]  ptime  Pointer to get the current time
 */
void
hdcp22wiredMeasureLwrrentTime
(
    PFLCN_TIMESTAMP pTime
)
{
    osPTimerTimeNsLwrrentGet(pTime);
}

/*!
 * @brief This function is used to measure if the elapsed time is within a limit.
 * @param[in]  pStartTime     Start Time
 * @param[in]  maxTimeLimit   Maximum Limit for the elapsed time.
 * @returns    LW_TRUE        If the elapsed time is within the limit.
 *             LW_FALSE       If the elapsed time is not within the limit.
 */
LwBool
hdcp22wiredCheckElapsedTimeWithMaxLimit
(
    PFLCN_TIMESTAMP pStartTime,
    LwU32 maxTimeLimit
)
{
    FLCN_TIMESTAMP endTime;
    osPTimerTimeNsLwrrentGet(&endTime);

    if ((endTime.parts.hi - pStartTime->parts.hi > 0) ||
        (endTime.parts.lo - pStartTime->parts.lo >= maxTimeLimit))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}
