/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_thrmintr.c
 * @brief  Thermal Interrupt Control Task
 *
 * The following is a thermal/power control module that aims to control the
 * thermal interrupts on PMU.
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmu_oslayer.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_objic.h"
#include "therm/objtherm.h"
#include "therm/thrmintr.h"
#include "task_pmgr.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
static void s_thermEventTaskNotify(LwU32 maskRising)
    // Called from ISR -> resident code.
    GCC_ATTRIB_SECTION("imem_resident", "s_thermEventTaskNotify");

static void s_thermEventViolationUpdate(LwU32 maskRising, LwU32 maskFalling)
    // Called from ISR -> resident code.
    GCC_ATTRIB_SECTION("imem_resident", "s_thermEventViolationUpdate");

/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Function Prototypes  --------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * @brief   Initialize static Event Violation data
 */
void
thermEventViolationPreInit(void)
{
    //
    // Structure is cleared by default.
    //
    // Therm.evt.maskIntrLevelHiHw    = 0;
    // Therm.evt.maskStateLwrrent     = RM_PMU_THERM_EVENT_MASK_NONE;
    // Therm.evt.MaskStateUsedLwrrent = RM_PMU_THERM_EVENT_MASK_NONE;
    // Therm.evt.maskRecentlyAsserted = RM_PMU_THERM_EVENT_MASK_NONE;
    // Therm.evt.bNotifyRM            = LW_FALSE;
    //

    thermEventViolationPreInit_HAL();

    Therm.evt.maskSupported =
        thermEventMapHwToGenericMask_HAL(&Therm, Therm.evt.maskSupportedHw);

    // Allow tracking of derived(combined) THERM events.
    Therm.evt.maskSupported |= BIT(RM_PMU_THERM_EVENT_META_ANY_HW_FS);

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_META_EVENT_ANY_EDP))
    {
        Therm.evt.maskSupported |= BIT(RM_PMU_THERM_EVENT_META_ANY_EDP);
    }

    //
    // Initialize GPIO and thermal interrupts (BA is enabled separately).
    // Note for new event interrupts - Please ensure to update
    // thermSlctHwFsEventMapInternalToRmctrl_HAL in RM when new event interrupts
    // will be enabled (for e.g. BA). Otherwise, it will lead to failures/asserts
    // in clients who have subscribed for THERM event notifications.
    //
    thermEventIntrPreInit_HAL(&Therm);
}

/*!
 * @brief   Process ISR notification of THERM event assertion.
 */
void
thermEventProcess(void)
{
    LwU32   maskAsserted;
    LwU32   maskAssertedUsed;

    appTaskCriticalEnter();
    {
        // Cache recently triggered HW failsafe slowdown interrupt mask
        maskAsserted = Therm.evt.maskRecentlyAsserted;

        // Clear mask
        Therm.evt.maskRecentlyAsserted = RM_PMU_THERM_EVENT_MASK_NONE;
    }
    appTaskCriticalExit();

    //
    // Compute the mask of asserted and used events - these are events which are
    // actually driving a slowdown.  This is what *most* of the code below
    // actually needs to care about (rare exception with the
    // PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN feature).
    //
    // If when all this code is fixed to only need used events - can simplify
    // @ref thermEventService() to only pass used events to THERM task.
    //
    maskAssertedUsed = maskAsserted & thermEventUseMaskGet_HAL(&Therm);

    // Manually handle power supply hot unplug shutdown event
    if ( PMUCFG_FEATURE_ENABLED(PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN) &&
        !PMUCFG_FEATURE_ENABLED(PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN_AUTO_CONFIG) &&
        (FLD_TEST_DRF(_RM_PMU_THERM, _FEATURES, _PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN,
                        _ENABLED, ThermConfig.thermFeatures)))
    {
        //
        // Unfortunately, we've shipped boards which rely on _EXT_POWER event,
        // but which do not enable it via USE bits.  As a temporary WAR, PMU
        // will continue to look at raw event assertion state.  A long-term RM
        // WAR should enable the _EXT_POWER event via the USE bits.
        //
        if (RM_PMU_THERM_IS_BIT_SET(RM_PMU_THERM_EVENT_EXT_POWER, maskAsserted))
        {
            thermPwrSupplyHotUnplugShutdownManualTrigger_HAL(&Therm);
        }
    }

    // If RM/PMU requested notifications, send them.
    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_HW_FS_SLOW_NOTIFICATION) &&
        Therm.evt.bNotifyRM &&
        (RM_PMU_THERM_EVENT_MASK_NONE != maskAssertedUsed))
    {
        thermSlowCtrlHwFsHandleTriggeredSlowIntr(maskAssertedUsed);
    }
}

/*!
 * @brief THERM event servicing.  Handles changes in THERM event state,
 * including updating violation statistics and notifying THERM task.
 */
void
thermEventService(void)
{
    LwU32 maskStateNew;
    LwU32 maskRising;
    LwU32 maskStateUsedNew;
    LwU32 maskRisingUsed;
    LwU32 maskFallingUsed;

    //
    // Retrieve mask of asserted events - these are all THERM events which are
    // lwrrently engaged, regardless of whether they are actually enabled to
    // drive slowdown via USE bits.
    //
    // @Note: Once bug regarding @ref PMU_THERM_PWR_SUPPLY_HOT_UNPLUG_SHUTDOWN
    // is fixed, separate @ref maskStateNew variable can be removed and can
    // consolidate down to only enabled/used events.
    //
    maskStateNew = thermEventAssertedMaskGet_HAL(&Therm);

    //
    // Compute the new mask of asserted events which are actually driving a
    // slowdown via USE bits.  Events may assert/deassert while not enabled via
    // USE, or USE bits may toggle while an event is asserted.  This check
    // handles all possibilities.
    //
    // @Note: Interrupt bits are tied only to event assertion state (irrespective
    // to whether events are enabled), this leads to the following two
    // interesting cases:
    //
    // 1. Even when ISR receives event assertion rising edge interrupt, this
    // interface may not detect any new state if the event is disabled via USE
    // bits.
    //
    // 2. No interrupt is generated when an already-asserted event is enabled or
    // disabled via USE bits.  SW must manually refresh state via this interface
    // whenever the USE bits are toggled.
    //
    maskStateUsedNew = maskStateNew & thermEventUseMaskGet_HAL(&Therm);

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_WAR_BUG_1357298))
    {
        maskStateUsedNew &= ~BIT(RM_PMU_THERM_EVENT_ALERT_NEG1H);
    }

    //
    // Track derived(combined) event: if any HW failsafe event has triggered.
    // Since maskStateUsedNew contains only valid HW events we do not have to
    // clear this bit (just set it if any HW event was detected).
    //
    if (RM_PMU_THERM_EVENT_MASK_NONE !=
        (maskStateUsedNew & RM_PMU_THERM_EVENT_MASK_ANY_HW_FS))
    {
        maskStateUsedNew |= BIT(RM_PMU_THERM_EVENT_META_ANY_HW_FS);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_META_EVENT_ANY_EDP) &&
        (RM_PMU_THERM_EVENT_MASK_NONE !=
         (maskStateUsedNew & RM_PMU_THERM_EVENT_MASK_ANY_EDP)))
    {
        maskStateUsedNew |= BIT(RM_PMU_THERM_EVENT_META_ANY_EDP);
    }

    //
    // Compute mask of rising and falling edges by comparing new state and
    // previous state.
    //
    // @Note:  This allows ignoring glitches 0->1->0 & 1->0->1 that happen
    // within the time required to process event interrupt.  Such cases
    // should be very rare, so it should be safe to ignore them.
    //
    // 1. Only need rising edges for events regardless if used or not.
    //
    maskRising = ~Therm.evt.maskStateLwrrent & maskStateNew; // Rising 0->1 transition

    // 2. Need rising and falling edges for used events.
    maskRisingUsed  = ~Therm.evt.maskStateUsedLwrrent & maskStateUsedNew; // Rising 0->1 transition
    maskFallingUsed = Therm.evt.maskStateUsedLwrrent & ~maskStateUsedNew; // Falling 1->0 transition

    // Update violation counters and timers.
    s_thermEventViolationUpdate(maskRisingUsed, maskFallingUsed);

    // Save new state as current state.
    Therm.evt.maskStateLwrrent     = maskStateNew;
    Therm.evt.maskStateUsedLwrrent = maskStateUsedNew;

    // Notify THERM task of events that just started.
    if (RM_PMU_THERM_EVENT_MASK_NONE != maskRising)
    {
        s_thermEventTaskNotify(maskRising);
    }
}

/*!
 * THERM task entry point to refresh THERM event state for cases where THERM
 * event state may change without generating an interrupt. This is effectively
 * a wrapper to @ref thermEventService() to be called from the THERM task.
 */
FLCN_STATUS
pmuRpcThermSlctHwfsEventsEnabledStateRefresh
(
    RM_PMU_RPC_STRUCT_THERM_SLCT_HWFS_EVENTS_ENABLED_STATE_REFRESH *pParams
)
{
    //
    // Call into the THERM event servicing routine.  Use critical section to
    // ensure that cannot race with ISR path.
    //
    appTaskCriticalEnter();
    {
        thermEventService();
    }
    appTaskCriticalExit();

    return FLCN_OK;
}

/*!
 * @brief   Returns 32-bit violation counter for requested THERM event index
 *
 * @param[in]   evtIdx      THERM event index as RM_PMU_THERM_EVENT_<xyz>
 * @param[out]  pCounter    Buffer to store current volation counter value
 *
 * @return  FLCN_OK Always succeeds (for now, we should check evtIdx)
 */
FLCN_STATUS
thermEventViolationGetCounter32
(
    LwU8    evtIdx,
    LwU32  *pCounter
)
{
    LwU8    packed;

    // Make sure that requested THERM event is supported by this feature
    if (!RM_PMU_THERM_IS_BIT_SET(evtIdx, Therm.evt.maskSupported))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Pack generic event index (if required)
    packed = thermEventViolationPackEventIndex_HAL(&Therm, evtIdx);

    *pCounter = Therm.evt.pViolData[packed].counterLo;

    return FLCN_OK;
}

/*!
 * @brief   Returns 64-bit violation counter for requested THERM event index
 *
 * @param[in]   evtIdx      THERM event index as RM_PMU_THERM_EVENT_<xyz>
 * @param[out]  pCounter    Buffer to store current volation counter value
 *
 * @return  FLCN_OK Always succeeds (for now, we should check evtIdx)
 */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_COUNTERS_EXTENDED)
FLCN_STATUS
thermEventViolationGetCounter64
(
    LwU8            evtIdx,
    LwU64_ALIGN32  *pCounter
)
{
    LwU8    packed;

    // Make sure that requested THERM event is supported by this feature
    if (!RM_PMU_THERM_IS_BIT_SET(evtIdx, Therm.evt.maskSupported))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Pack generic event index (if required)
    packed = thermEventViolationPackEventIndex_HAL(&Therm, evtIdx);

    pCounter->lo = Therm.evt.pViolData[packed].counterLo;
    pCounter->hi = Therm.evt.pViolData[packed].counterHi;

    return FLCN_OK;
}
#endif

/*!
 * @brief   Returns the 32-bit violation timer for requested THERM event index.
 *
 * @param[in]   evtIdx  THERM event index as RM_PMU_THERM_EVENT_<xyz>
 * @param[out]  pTimer  Buffer to store current volation timer's value [ns]
 *
 * @return  FLCN_OK Always succeeds (for now, we should check evtIdx)
 */
FLCN_STATUS
thermEventViolationGetTimer32
(
    LwU8    evtIdx,
    LwU32  *pTimer
)
{
    FLCN_TIMESTAMP  now;
    LwU32           maskState;
    LwU8            packed;
    LwBool          bActive;

    // Make sure that requested THERM event is supported by this feature
    if (!RM_PMU_THERM_IS_BIT_SET(evtIdx, Therm.evt.maskSupported))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Pack generic event index (if required)
    packed = thermEventViolationPackEventIndex_HAL(&Therm, evtIdx);

    // Critical section ensures all necessary synchronization with ISR code.
    appTaskCriticalEnter();
    {
        maskState = Therm.evt.maskStateUsedLwrrent;
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
        *pTimer = Therm.evt.pViolData[packed].timer.parts.lo;
#else
        *pTimer = Therm.evt.pViolData[packed].timer;
#endif
        bActive   = RM_PMU_THERM_IS_BIT_SET(evtIdx, maskState);

        if (bActive)
        {
            osPTimerTimeNsLwrrentGet(&now);
        }
    }
    appTaskCriticalExit();

    if (bActive)
    {
        *pTimer += now.parts.lo;
    }

    return FLCN_OK;
}

/*!
 * @brief   Returns the 64-bit violation timer for requested THERM event index.
 *
 * @param[in]   evtIdx  THERM event index as RM_PMU_THERM_EVENT_<xyz>
 * @param[out]  pTimer  Buffer to store current volation timer's value [ns]
 *
 * @return  FLCN_OK Always succeeds (for now, we should check evtIdx)
 */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
FLCN_STATUS
thermEventViolationGetTimer64
(
    LwU8            evtIdx,
    FLCN_TIMESTAMP *pTimer
)
{
    FLCN_TIMESTAMP  now;
    LwU32           maskState;
    LwU8            packed;
    LwBool          bActive;

    // Make sure that requested THERM event is supported by this feature
    if (!RM_PMU_THERM_IS_BIT_SET(evtIdx, Therm.evt.maskSupported))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Pack generic event index (if required)
    packed = thermEventViolationPackEventIndex_HAL(&Therm, evtIdx);

    // Critical section ensures all necessary synchronization with ISR code.
    appTaskCriticalEnter();
    {
        maskState    = Therm.evt.maskStateUsedLwrrent;
        pTimer->data = Therm.evt.pViolData[packed].timer.data;
        bActive      = RM_PMU_THERM_IS_BIT_SET(evtIdx, maskState);

        if (bActive)
        {
            osPTimerTimeNsLwrrentGet(&now);
        }
    }
    appTaskCriticalExit();

    if (bActive)
    {
        lw64Add(&(pTimer->data), &(pTimer->data), &(now.data));
    }

    return FLCN_OK;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)

/* ------------------------ Private Functions  ----------------------------- */
/*!
 * Helper which tracks slowdown/violation state of THERM events - i.e. events
 * which drive slowdowns via asserting/deasserting and enabling/disabling from
 * USE bits.
 *
 * @param[in] maskRising
 *     Mask of THERM events (@ref RM_PMU_THERM_EVENT_<xyz>) which are engaging
 *     slowdowns (i.e. asserted and enabled via USE bits).
 * @param[in] maskFalling
 *     Mask of THERM events (@ref RM_PMU_THERM_EVENT_<xyz>) which are
 *     disengaging slowdowns (i.e. either deassserted or disabled via USE bits).
 */
static void
s_thermEventViolationUpdate
(
    LwU32 maskRising,
    LwU32 maskFalling
)
{
    FLCN_TIMESTAMP  now;
    LwU32           evtIdx;
    LwU32           packed;

    // Capture lwrent time
    osPTimerTimeNsLwrrentGet(&now);

    // Merging into single loop two different actions to reduce ISR code size.
    FOR_EACH_INDEX_IN_MASK(32, evtIdx, maskRising | maskFalling)
    {
        THERM_PMU_EVENT_VIOLATION_ENTRY *pEntry;

        packed = thermEventViolationPackEventIndex_HAL(&Therm, evtIdx);
        pEntry = &Therm.evt.pViolData[packed];

        //
        // On rising edge, increment the count and start the timer for the new
        // timestamp.
        //
        if (RM_PMU_THERM_IS_BIT_SET(evtIdx, maskRising))
        {
            pEntry->counterLo++;
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_COUNTERS_EXTENDED)
            if (pEntry->counterLo == 0)
            {
                pEntry->counterHi++;
            }
#endif

#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
            lw64Sub(&(pEntry->timer.data), &(pEntry->timer.data), &(now.data));
#else
            pEntry->timer -= now.parts.lo;
#endif
        }
        // On falling edge, finish the timer with the new timestamp.
        else if (RM_PMU_THERM_IS_BIT_SET(evtIdx, maskFalling))
        {
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
            lw64Add(&(pEntry->timer.data), &(pEntry->timer.data), &(now.data));
#else
            pEntry->timer += now.parts.lo;
#endif
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief   Notify THERM task of events that just happened.
 *
 * @param[in] maskRising  Mask of THERM events that asserted
 */
static void
s_thermEventTaskNotify
(
    LwU32 maskRising
)
{
    DISP2THERM_CMD disp2Therm = {{ 0 }};

    // If bitmask is _NONE no message is in THERM queue so send one.
    if (Therm.evt.maskRecentlyAsserted == RM_PMU_THERM_EVENT_MASK_NONE)
    {
        disp2Therm.hdr.unitId = RM_PMU_UNIT_THERM;
        disp2Therm.hdr.eventType = THERM_SIGNAL_IRQ;

        lwrtosQueueIdSendFromISR(LWOS_QUEUE_ID(PMU, THERM),
                                 &disp2Therm, sizeof(disp2Therm));
    }

    // Update HW failsafe slowdown interrupts that triggered on HI level
    Therm.evt.maskRecentlyAsserted |= maskRising;
}

