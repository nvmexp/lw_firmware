/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_THRMINTR_H
#define PMU_THRMINTR_H

/*!
 * @file thrmintr.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "task_therm.h"

/* ------------------------ Defines ---------------------------------------- */
/*!
 * Timestamp based THERM event filtering period for RM notifications (1 second).
 */
#define THERM_PMU_EVENT_RM_NOTIFICATION_FILTER_1S_IN_NS               1000000000

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * This structure holds the violation data for single LW_THERM event.
 */
typedef struct
{
    /*!
     * Total time event was asserted (measured in nanoseconds). Without an
     * EXTENDED mode and thermEventViolationGetTimer64() interface this
     * timer wraps around after ~4.295 seconds, so clients must poll faster
     * than that.
     */
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_TIMERS_EXTENDED)
    FLCN_TIMESTAMP  timer;
#else
    LwU32           timer;
#endif
    /*!
     * Total number of times event was asserted (count). Without an EXTENDED
     * mode and thermEventViolationGetCounter64() interface this counter wraps
     * around after 4294967295 events oclwred, so client must poll reasonably
     * fast (relative to the event frequency).
     *
     * @note    These two 32-bit fields must be adjacent (same order) since we
     *          cast @ref counterLo pointer to LwU64* and work with 64-bit value.
     */
    LwU32   counterLo;
#if PMUCFG_FEATURE_ENABLED(PMU_THERM_VIOLATION_COUNTERS_EXTENDED)
    LwU32   counterHi;
#endif
} THERM_PMU_EVENT_VIOLATION_ENTRY;

/*!
 * This structure holds all THERM event related information (state, violation
 * information, slowdown notification, ...).
 */
typedef struct
{
    /*!
     * HW specific bitmask of SW supported THERM events that trigger level
     * interrupts. This mask is subset of events available in HW that are
     * supported by SW (necessary memory has been allocated for them and
     * their indexes can be packed). Subset of @see maskSupportedHw.
     */
    LwU32                               maskIntrLevelHw;
    /*!
     * HW specific bitmask specifying which THERM events that trigger level
     * interrupts are asserting IRQ on HIGH level (remaining assert at LOW).
     */
    LwU32                               maskIntrLevelHiHw;
    /*!
     * HW specific bitmask of SW supported THERM events. This mask is subset
     * of events available in HW that are supported by SW (necessary memory
     * has been allocated for them and their indexes can be packed).
     */
    LwU32                               maskSupportedHw;
    /*!
     * RM_PMU_THERM_EVENT_<xyz> bitmask of SW supported THERM events. This mask
     * is subset of events available in HW that are supported by SW (necessary
     * memory has been allocated for them and their indexes can be packed).
     */
    LwU32                               maskSupported;
    /*!
     * RM_PMU_THERM_EVENT_<xyz> bitmask representing current state of events.
     */
    LwU32                               maskStateLwrrent;
    /*!
     * RM_PMU_THERM_EVENT_<xyz> bitmask representing current state of events
     * w.r.t. slowdown - i.e. are asserted (will have a bit set in @ref
     * maskStateLwrrent) and enabled via the USE bits.
     */
    LwU32                               maskStateUsedLwrrent;
    /*!
     * RM_PMU_THERM_EVENT_<xyz> bitmask of THERM events that recently asserted.
     * ISR notifies THERM task each time when THERM event oclwrs. To avoid
     * flooding of THERM task and its message queue with event notifications
     * following approach was taken:
     * - there can be at most one notification message pending in the queue
     * - in case of new THERM event ISR will update this bitmask, and place
     *   message into THERM queue only if none was on its way
     * - uppon receving notification message, THERM task (within a critical
     *   section) caches this bitmask of all events that happened since last
     *   notification and clears it notifying ISR that message queue is empty.
     */
    LwU32                               maskRecentlyAsserted;
    /*!
     * @copydoc THERM_PMU_EVENT_VIOLATION_ENTRY.
     *
     * Pointer to the statically allocated array holding violation entries,
     * indexed by packed event ID (packed RM_PMU_THERM_EVENT_<xyz>).
     */
    THERM_PMU_EVENT_VIOLATION_ENTRY    *pViolData;
    /*!
     * @copydoc FLCN_TIMESTAMP.
     *
     * Pointer to the statically allocated array holding min timestamp for the
     * next RM->PMU notification of THERM events, indexed by packed event ID
     * (packed RM_PMU_THERM_EVENT_<xyz>).
     */
    FLCN_TIMESTAMP                     *pNextNotif;
    /*!
     * Set if RM requires notifications on THERM event assertions.
     */
    LwBool                              bNotifyRM;
} THERM_PMU_EVENT_DATA;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void        thermEventViolationPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "thermEventViolationPreInit");

void        thermEventProcess(void);

void        thermEventService(void)
    // Called from ISR -> resident code.
    GCC_ATTRIB_SECTION("imem_resident", "thermEventService");

FLCN_STATUS thermEventViolationGetCounter32(LwU8 evtIdx, LwU32 *pCounter)
    GCC_ATTRIB_SECTION("imem_resident", "thermEventViolationGetCounter32");
FLCN_STATUS thermEventViolationGetCounter64(LwU8 evtIdx, LwU64_ALIGN32 *pCounter)
    GCC_ATTRIB_SECTION("imem_resident", "thermEventViolationGetCounter64");

FLCN_STATUS thermEventViolationGetTimer32(LwU8 evtIdx, LwU32 *pTimer)
    GCC_ATTRIB_SECTION("imem_resident", "thermEventViolationGetTimer32");
FLCN_STATUS thermEventViolationGetTimer64(LwU8 evtIdx, PFLCN_TIMESTAMP pTimer)
    GCC_ATTRIB_SECTION("imem_resident", "thermEventViolationGetTimer64");

// Accessor for @ref maskStateUsedLwrrent
#define     thermEventAssertedGet(therm)    \
    ((therm)->evt.maskStateUsedLwrrent)

#endif // PMU_THRMINTR_H
