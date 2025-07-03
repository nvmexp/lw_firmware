/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJPG_H
#define PMU_OBJPG_H

/*!
 * @file pmu_objpg.h
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"
#include "unit_api.h"
#include "main.h"
#include "lwostimer.h"
#include "pmu_objpsi.h"
#include "pmu_objpmu.h"
#include "pmu_cg.h"
#include "task_perf_daemon.h"

/* ------------------------ Forward Declartion ----------------------------- */
// PG State Object
typedef struct OBJPGSTATE              OBJPGSTATE;

typedef union  DISPATCH_LPWR           DISPATCH_LPWR;
typedef struct DISPATCH_LPWR_FSM_EVENT DISPATCH_LPWR_FSM_EVENT;
typedef struct DISPATCH_LPWR_EXT_EVENT DISPATCH_LPWR_EXT_EVENT;
typedef struct DISPATCH_LPWR_INTR_ELPG DISPATCH_LPWR_INTR_ELPG;
typedef struct DISPATCH_LPWR_FUNCS     DISPATCH_LPWR_FUNCS;
typedef struct PG_LOGIC_STATE          PG_LOGIC_STATE;

/* ------------------------ Application includes --------------------------- */
#include "config/g_pg_hal.h"
#include "pg/pg_hal-mock.h"
#include "perf/3x/vfe.h"

/* ------------------------ Macros & Defines ------------------------------- */
#define GET_OBJPGSTATE(ctrlId)           (Pg.pPgStates[ctrlId])
#define PG_GET_ENG_IDX(ctrlId)           (GET_OBJPGSTATE(ctrlId)->hwEngIdx)

/*!
 * @brief Macros to get pointer to LPWR_CALLBACK and LPWR_CALLBACK_CTRL.
 */
#define LPWR_GET_CALLBACK()                (Pg.pCallback)
#define LPWR_GET_CALLBACKCTRL(ctrlId)      (&Pg.pCallback->callbackCtrl[ctrlId])

/*!
 * @brief Macro to check if PgCtrl is supported
 */
#define pgIsCtrlSupported(ctrlId)           ((Pg.supportedMask & BIT(ctrlId)) != 0U)

/*!
 * @brief Macro to check the support for Sub features of PG Eng
 */
#define PG_IS_SF_SUPPORTED(featureObj, featureName, sfName)                   \
        (LPWR_IS_SF_MASK_SET(featureName, sfName, featureObj.supportMask))

/*!
 * @brief Macro to check the enable state for Sub features of PG Eng
 */
#define PG_IS_SF_ENABLED(featureObj, featureName, sfName)                     \
        (LPWR_IS_SF_MASK_SET(featureName, sfName, featureObj.enabledMask))

/*!
 * @brief Macro to check support for LPWR ARCH sub-features
 */
#define LPWR_ARCH_IS_SF_SUPPORTED(sfName)                                       \
        (LPWR_IS_SF_MASK_SET(ARCH, sfName, Pg.archSfSupportMask))
//
// Defines for Buffer that is used by SSC/GC6 to copy data
// from FB/SYS MEM to DMEM
//
#define PG_BUFFER_SIZE_BYTES            256U
#define PG_BUFFER_SIZE_DWORDS             \
        (PG_BUFFER_SIZE_BYTES >> 2)
/*!
 * @brief LPWR task events
 */
#define LPWR_EVENT_ID_PMU                           (DISP2UNIT_EVT__COUNT + 0U)
#define LPWR_EVENT_ID_PMU_EXT                       (DISP2UNIT_EVT__COUNT + 1U)
#define LPWR_EVENT_ID_PG_INTR                       (DISP2UNIT_EVT__COUNT + 2U)
#define LPWR_EVENT_ID_PG_HOLDOFF_INTR               (DISP2UNIT_EVT__COUNT + 3U)
#define LPWR_EVENT_ID_MS_WAKEUP_INTR                (DISP2UNIT_EVT__COUNT + 4U)
#define LPWR_EVENT_ID_MS_DISP_INTR                  (DISP2UNIT_EVT__COUNT + 5U)
#define LPWR_EVENT_ID_MS_ABORT_DMA_SUSP             (DISP2UNIT_EVT__COUNT + 6U)

#define LPWR_EVENT_ID_MS_OSM_NISO_START             (DISP2UNIT_EVT__COUNT + 7U)
#define LPWR_EVENT_ID_MS_OSM_NISO_END               (DISP2UNIT_EVT__COUNT + 8U)

#define LPWR_EVENT_ID_PRIV_BLOCKER_WAKEUP_INTR      (DISP2UNIT_EVT__COUNT + 9U)

#define LPWR_EVENT_ID_PERF_CHANGE_NOTIFICATION      (DISP2UNIT_EVT__COUNT + 10U)
#define LPWR_EVENT_ID_EI_NOTIFICATION               (DISP2UNIT_EVT__COUNT + 11U)
#define LPWR_EVENT_ID_GR_RG_PERF_SCRIPT_COMPLETION  (DISP2UNIT_EVT__COUNT + 12U)

/*!
 * @brief   PG State Machine States
 *
 * Define internal states of PMU PG State Machine. Enums here are somewhat
 * redundant to other enums defined above, but we define a new set of states
 * that will be used by the new PMU PG architecture for easy migration to the
 * PMU in future.
 */

#define PMU_PG_STATE_PWR_ON   BIT(0U)
#define PMU_PG_STATE_DISALLOW BIT(1U)
#define PMU_PG_STATE_ON2OFF   BIT(2U)
#define PMU_PG_STATE_PWR_OFF  BIT(3U)
#define PMU_PG_STATE_OFF2ON   BIT(4U)

/*!
 * @brief   Macros to check the full-power state for PgCtrl
 *
 * PgCtrl is fully powered ON if its in PWR_ON or DISALLOW state.
 *
 * @Note    PG_IS_MS_FULL_POWER() macro has one critical use-case in RTOS code
 *          handling callbacks so all future changes to these macros should
 *          assess impact on callback performance.
 */
#define PMU_PG_STATE_MASK_FULL_PWR          \
    (PMU_PG_STATE_PWR_ON |                  \
     PMU_PG_STATE_DISALLOW)

#define PG_IS_FULL_POWER(ctrlId)            \
    ((Pg.pPgStates[ctrlId]->state & PMU_PG_STATE_MASK_FULL_PWR) != 0U)

/*!
 * @brief   Macros to check the Transition state for PgCtrl
 *
 * PgCtrl is fully in transition if its in ON2OFF or OFF2ON state.
 */
#define PMU_PG_STATE_MASK_TRANSITION        \
    (PMU_PG_STATE_ON2OFF |                  \
     PMU_PG_STATE_OFF2ON)

#define LPWR_CTRL_IS_IN_TRANSITION(ctrlId)  \
            ((Pg.pPgStates[ctrlId]->state & PMU_PG_STATE_MASK_TRANSITION) != 0U)

/*!
 * @brief Macro to check whether PgCtrl is engaged
 */
#define PG_IS_ENGAGED(ctrlId)               \
            ((GET_OBJPGSTATE(ctrlId))->state == PMU_PG_STATE_PWR_OFF)

/*!
 * @brief State of RTOS notification to PG task
 *
 * _DISABLED : Notification is disabled
 * _ENABLED  : Notification is enabled
 * _QUEUED   : IDLE task queued the idle event to PG Queue
 */
enum
{
    PG_RTOS_IDLE_EVENT_STATE_DISABLED = 0x0,
    PG_RTOS_IDLE_EVENT_STATE_ENABLED       ,
    PG_RTOS_IDLE_EVENT_STATE_QUEUED        ,
};

/*!
 * @brief Task Management events used to updates statistics
 *
 * _DETECTION              : Task thrashing/starvation detected.
 * _PREVENTION_ACTIVATED   : Task thrashing/starvation prevention
 *                           policy activated.
 * _PREVENTION_DEACTIVATED : Task thrashing/starvation prevention
 *                           policy deactivated.
 */
enum
{
    PG_STAT_TASK_MGMT_EVENT_ID_DETECTION = 0x0       ,
    PG_STAT_TASK_MGMT_EVENT_ID_PREVENTION_ACTIVATED  ,
    PG_STAT_TASK_MGMT_EVENT_ID_PREVENTION_DEACTIVATED,
};

/*!
 * @brief LPWR FSM Events
 *
 * These events are process by LPWR FSM.
 */
#define PMU_PG_EVENT_NONE              0U
#define PMU_PG_EVENT_CHECK_STATE       1U
#define PMU_PG_EVENT_PG_ON             2U
#define PMU_PG_EVENT_PG_ON_DONE        3U
#define PMU_PG_EVENT_ENG_RST           4U
#define PMU_PG_EVENT_CTX_RESTORE       5U
#define PMU_PG_EVENT_DENY_PG_ON        6U
#define PMU_PG_EVENT_POWERED_DOWN      7U
#define PMU_PG_EVENT_POWERING_UP       8U
#define PMU_PG_EVENT_POWERED_UP        9U
#define PMU_PG_EVENT_WAKEUP            10U
#define PMU_PG_EVENT_DISALLOW          11U
#define PMU_PG_EVENT_DISALLOW_ACK      12U
#define PMU_PG_EVENT_ALLOW             13U

#define PMU_PG_EVENT_ALL_MASK           \
   (BIT(PMU_PG_EVENT_NONE)             |\
    BIT(PMU_PG_EVENT_CHECK_STATE)      |\
    BIT(PMU_PG_EVENT_PG_ON)            |\
    BIT(PMU_PG_EVENT_PG_ON_DONE)       |\
    BIT(PMU_PG_EVENT_ENG_RST)          |\
    BIT(PMU_PG_EVENT_CTX_RESTORE)      |\
    BIT(PMU_PG_EVENT_DENY_PG_ON)       |\
    BIT(PMU_PG_EVENT_WAKEUP)           |\
    BIT(PMU_PG_EVENT_DISALLOW)         |\
    BIT(PMU_PG_EVENT_DISALLOW_ACK)     |\
    BIT(PMU_PG_EVENT_ALLOW)            |\
    BIT(PMU_PG_EVENT_POWERING_UP)      |\
    BIT(PMU_PG_EVENT_POWERED_UP)       |\
    BIT(PMU_PG_EVENT_POWERED_DOWN))

/*!
 * @brief PMU PG External events
 *
 * Possible values of LPWR_EVENT_ID_PMU_EXT.
 * These are events that other tasks will send to PG.
 */
enum
{
    PMU_PG_EXT_EVENT_RTOS_IDLE = 0x1,
    PMU_PG_EXT_EVENT_SELF_ALLOW     ,
};

/*!
 * @brief Action done by SM
 *
 * _PG_OFF_START: Trigger PG_OFF. It will wake HW SM.
 * _PG_OFF_DONE : Notify completion of exit sequence.
 */
enum
{
    PMU_PG_SM_ACTION_PG_OFF_START = 0x0,
    PMU_PG_SM_ACTION_PG_OFF_DONE       ,
};

/*!
 * @brief Overlay IDs for all LPWR features.
 *
 * IDs are defined in form of Bit Mask.
 *
 * _GR          : All GR overlays i.e. libGr, libFifo
 * _MS          : All MS overlays i.e. libMs, libSwAsr
 * _LPWR_LOADIN : lpwrLoadin overlay
 * _FOREVER     : Overlays that will be permanently attach to LPWR task
 */
enum
{
    PG_OVL_ID_MASK_GR          = BIT(0),
    PG_OVL_ID_MASK_MS          = BIT(1),
    PG_OVL_ID_MASK_LPWR_LOADIN = BIT(2),
    PG_OVL_ID_MASK_FOREVER     = BIT(3)
};

/*!
 * @brief Index for PG_ENG and/or LPWR_ENG
 */
enum
{
    PG_ENG_IDX_ENG_0 = 0x0,
    PG_ENG_IDX_ENG_1      ,
    PG_ENG_IDX_ENG_2      ,
    PG_ENG_IDX_ENG_3      ,
    PG_ENG_IDX_ENG_4      ,
    PG_ENG_IDX_ENG_5      ,
    PG_ENG_IDX_ENG_6      ,
    PG_ENG_IDX_ENG_7      ,
    PG_ENG_IDX_ENG_8      ,
    PG_ENG_IDX_ENG_9      ,
    PG_ENG_IDX__COUNT     ,
};

/*!
 * @brief Map PG_ENG/LPWR_ENG HW IDX to SW feature
 */
#define PG_ENG_IDX_ENG_GR            PG_ENG_IDX_ENG_0
#define PG_ENG_IDX_ENG_GR_PASSIVE    PG_ENG_IDX_ENG_1
#define PG_ENG_IDX_ENG_GR_RG         PG_ENG_IDX_ENG_2
#define PG_ENG_IDX_ENG_MS            PG_ENG_IDX_ENG_4
#define PG_ENG_IDX_ENG_MS_LTC        PG_ENG_IDX_ENG_4
#define PG_ENG_IDX_ENG_MS_PASSIVE    PG_ENG_IDX_ENG_4
#define PG_ENG_IDX_ENG_EI            PG_ENG_IDX_ENG_3
#define PG_ENG_IDX_ENG_EI_PASSIVE    PG_ENG_IDX_ENG_3
#define PG_ENG_IDX_ENG_DIFR_PREFETCH PG_ENG_IDX_ENG_5
#define PG_ENG_IDX_ENG_DIFR_SW_ASR   PG_ENG_IDX_ENG_6
#define PG_ENG_IDX_ENG_DIFR_CG       PG_ENG_IDX_ENG_7

/*!
 * @brief Idle supplementary counter IDX
 */
enum
{
    PG_IDLE_SUPP_CNTR_IDX_0 = 0x0,
    PG_IDLE_SUPP_CNTR_IDX_1      ,
    PG_IDLE_SUPP_CNTR_IDX_2      ,
    PG_IDLE_SUPP_CNTR_IDX_3      ,
    PG_IDLE_SUPP_CNTR_IDX_4      ,
    PG_IDLE_SUPP_CNTR_IDX_5      ,
    PG_IDLE_SUPP_CNTR_IDX_6      ,
    PG_IDLE_SUPP_CNTR_IDX_7      ,
    PG_IDLE_SUPP_CNTR_IDX__COUNT ,
};

/*!
 * @brief Define invalid supp counter index
 */
#define PG_IDLE_SUPP_CNTR_IDX__ILWALID  PG_IDLE_SUPP_CNTR_IDX__COUNT

/*!
 * @brief Working mode of Supplemental Idle Counters
 *
 * 1) _FORCE_IDLE:
 * This is equivalent to HW mode LW_CPWR_PMU_IDLE_CTRL_SUPP_VALUE_ALWAYS.
 * The idle signal is overridden to 1. Means signal is forcefully set to IDLE.
 *
 * 2) _FORCE_BUSY
 * This is equivalent to HW mode LW_CPWR_PMU_IDLE_CTRL_SUPP_VALUE_NEVER.
 * The idle signal is overridden to 0. Means signal is forcefully set to BUSY.
 *
 * 3) _AUTO_IDLE
 * This is equivalent to HW mode LW_CPWR_PMU_IDLE_CTRL_SUPP_VALUE_IDLE.
 * The idle signal is get collected from different units and ORed mask of these
 * signal is feed to counters.
 */
#define PG_SUPP_IDLE_COUNTER_MODE_FORCE_IDLE          (0U)
#define PG_SUPP_IDLE_COUNTER_MODE_FORCE_BUSY          (1U)
#define PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE           (2U)

/*
 * SW Index for Voltage Rails in SCI
 */
#define PG_ISLAND_SCI_RAIL_IDX_LWVDDL             0U
#define PG_ISLAND_SCI_RAIL_IDX_LWVDDS             1U

/*!
 *  @brief : Macros to check whether Volt parameters are valid or not.
 */
#define PG_VOLT_IS_RAIL_SUPPORTED(railIdx)          (Pg.voltRail[railIdx].voltRailIdx != PG_VOLT_RAIL_IDX_ILWALID)
#define PG_VOLT_IS_VFE_IDX_VALID(vfeIdx)            ((vfeIdx) != PMU_PERF_VFE_EQU_INDEX_ILWALID)
#define PG_VOLT_IS_VOLTDEV_IDX_VALID(voltDevIdx)    ((voltDevIdx) != PG_VOLT_RAIL_VOLTDEV_IDX_ILWALID)

/*!
 *  @brief : Macros to get volt parameters based on rail Index
 */
#define PG_VOLT_GET_VOLT_RAIL_IDX(railIdx)        (Pg.voltRail[railIdx].voltRailIdx)
#define PG_VOLT_GET_SLEEP_VFE_IDX(railIdx)        (VFE_GET_PMU_IDX_FROM_BOARDOBJIDX(Pg.voltRail[railIdx].sleepVfeIdx))
#define PG_VOLT_GET_SLEEP_VOLTDEV_IDX(railIdx)    (Pg.voltRail[railIdx].sleepVoltDevIdx)
#define PG_VOLT_GET_SLEEP_VOLTAGE_UV(railIdx)     (Pg.voltRail[railIdx].sleepVoltageuV)
#define PG_VOLT_GET_THERM_VID0_CACHE(railIdx)     (Pg.voltRail[railIdx].thermVid0Cache)
#define PG_VOLT_GET_THERM_VID1_CACHE(railIdx)     (Pg.voltRail[railIdx].thermVid1Cache)

/*!
 *  @brief : Macros to set volt params for individual rails
 */
#define PG_VOLT_SET_SLEEP_VOLTAGE_UV(railIdx, value)     (Pg.voltRail[railIdx].sleepVoltageuV = (value))
#define PG_VOLT_SET_THERM_VID0_CACHE(railIdx, value)     (Pg.voltRail[railIdx].thermVid0Cache = (value))
#define PG_VOLT_SET_THERM_VID1_CACHE(railIdx, value)     (Pg.voltRail[railIdx].thermVid1Cache = (value))

/* ------------------------ Types definitions ------------------------------ */

/*!
 * @brief Powergatable Interface Definition
 *
 * A powergatable entity (or powergatable) is composed of one or more of host
 * engines or subsystems, which can be powergated by using some or all of the
 * following generic powergating operations.
 *
 * o Primitive PG operations
 * -------------------------
 * lpwrEntry()
 *      Manages PG_ENG and LPWR_ENG entry sequence.
 * lpwrExit()
 *      Manages PG_ENG and LPWR_ENG exit sequence.
 * lpwrReset()
 *      Manages reset sequence for PG_ENG. This API is not used by LPWR_ENG.
 */
// Primitve Powergatable Operation Type Definition
typedef FLCN_STATUS (*PgOps)     (void);

// Powergatable Interface
typedef struct POWERGATABLE
{
    PgOps   lpwrEntry;
    PgOps   lpwrExit;
    PgOps   lpwrReset;
} POWERGATABLE;

/*!
 * @brief Structure to get delta time from last sample
 */
typedef struct
{
    LwU32 prevTimeUs;
    LwU32 deltaTimeUs;
} PGSTATE_STAT_SAMPLE;

/*!
 * @brief Task management related state of PgCtrl
 *
 * State variables used by PG task to detect the starvation of non-LPWR task
 * because of LPWR feature. After detection PG task will attempt to prevent
 * the thrashing/starvation.
 */
typedef struct
{
    // Sleep and powered up samples used by thrashing logic
    PGSTATE_STAT_SAMPLE   sleepInfo;
    PGSTATE_STAT_SAMPLE   poweredUpInfo;

    // Counts back-to-back aborts. It gets reset on successful entry.
    LwU8   b2bAbortCount;

    //
    // Counts back-to-back wakeup. It gets reset when
    // - Resident time is greater than PG_CTRL_TASK_MGMT_MIN_RESIDENT_TIME_DEFAULT_US
    // OR
    // - Powered ON time is greater than PG_CTRL_TASK_MGMT_POWERED_UP_TIME_DEFAULT_US
    //
    LwU8   b2bWakeupCount;

    // Set LW_TRUE when PgCtrl is disabled to avoid thrashing or starvation
    LwBool bDisallow;
} PG_STATE_TASK_MGMT;

/*!
 * @brief default value for TASK_MGMT feature
 * B2B_ABORT_THRESHOLD_DEFAULT :
 * PgCtrl is disabled when back-to-back abort count reaches
 * B2B_ABORT_THRESHOLD_DEFAULT.
 *
 * WAKEUP_THRESHOLD_DEFAULT    :
 * PgCtrl is disabled when back-to-back wake-ups having residency less than
 * MIN_RESIDENT_TIME_DEFAULT_US
 *
 * MIN_RESIDENT_TIME_DEFAULT_US:
 * Minimum residency used by wake-up policy.
 *
 * POWERED_UP_TIME_DEFAULT_US  :
 * Back-to-back wake-up policy will be reset if PgCtrl power on time is greater
 * than POWERED_UP_TIME_DEFAULT_US.
 *
 * Idle task re-enables PgCtrl disabled because of any of above policy. Idle
 * task is schedule means PMU do not have any active work.
 */
#define PG_CTRL_TASK_MGMT_B2B_ABORT_THRESHOLD_DEFAULT       5U
#define PG_CTRL_TASK_MGMT_B2B_WAKEUP_THRESHOLD_DEFAULT      5U
#define PG_CTRL_TASK_MGMT_MIN_RESIDENT_TIME_DEFAULT_US      200U
#define PG_CTRL_TASK_MGMT_POWERED_UP_TIME_DEFAULT_US        5000U

/*!
 * @brief Subfeature Enable and Disable clients for PG ENG
 *
 * Each of the clients can request to enable or disable a sub-features of PG_ENG.
 * The final enabled mask will be determined based on the cached requests of
 * all the clients.
 *
 * Details:
 * 1) _RM              :  RM client will update the enabled mask based on RM
 *                        request
 * 2) _OSM             :  OSM (MSCG One Shot Mode) will enable disable iso stutter
 *                        sub feature for MS.
 * 3) _PERF            :  PERF client will update the sub-feature mask based on
 *                        current Pstate
 * 4) _MCLK            :  MCLK Client will update the sub-feature mask based on
 *                        current mclk value
 * 5) _EI_NOTIFICATION :  EI Notification will update the sub-feature mask
 */
enum
{
    LPWR_CTRL_SFM_CLIENT_ID_RM = 0x0       ,
    LPWR_CTRL_SFM_CLIENT_ID_OSM            ,
    LPWR_CTRL_SFM_CLIENT_ID_PERF           ,
    LPWR_CTRL_SFM_CLIENT_ID_MCLK           ,
    LPWR_CTRL_SFM_CLIENT_ID_EI_NOTIFICATION,
    LPWR_CTRL_SFM_CLIENT_ID__COUNT         ,
};

/*!
 * @brief Idle snap reason
 *
 * Idle snap representing errors:
 * SW covert idle snap interrupt to DISALLOW event and disables PgCtrl forever.
 *
 * ERR_IDLE_FLIP_POWERING_DOWN  : Idle signal reported busy in POWERING_DOWN
 *                                state. This means idle-flip was asserted
 *                                after point of no-return.
 * ERR_IDLE_FLIP_PWR_OFF        : Idle signal reported busy in PWR_OFF state.
 *                                It was not expected to change the state of
 *                                signal in PWR_OFF state.
 * ERR_UNKNOWN                  : SW is not able to find the valid reason for
 *                                idle snap.
 *
 * Idle snap representing wakeups:
 * SW covert idle snap interrupt to WAKEUP event. Error reason gets priority
 * over wakeup reason.
 *
 * WAKEUP_IDLE_FLIP_PWR_OFF     : Idle snap generated by expected changes in
 *                                idle signal state.
 * WAKEUP_SW_CLIENT             : Idle snap generated by SW client.
 * WAKEUP_TIMER                 : Idle snap generated by wakeup timer.
 */
#define PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_POWERING_DOWN     BIT(0)
#define PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_PWR_OFF           BIT(1)
#define PG_IDLE_SNAP_REASON_ERR_UNKNOWN                     BIT(2)

#define PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF        BIT(16)
#define PG_IDLE_SNAP_REASON_WAKEUP_SW_CLIENT                BIT(17)
#define PG_IDLE_SNAP_REASON_WAKEUP_TIMER                    BIT(18)

/*!
 * @brief Define for mask of all idle snap error reasons
 */
#define PG_IDLE_SNAP_REASON_MASK_ERR                    \
(                                                       \
    PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_POWERING_DOWN    |\
    PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_PWR_OFF          |\
    PG_IDLE_SNAP_REASON_ERR_UNKNOWN                     \
)

/*!
 * @brief Helper define to initialize idle-snap reason
 */
#define PG_IDLE_SNAP_REASON__INIT   0x0

/*!
 * @brief Macro to check whether idle snap is generated because of error
 */
#define PG_CTRL_IS_IDLE_SNAP_ERR(reasonMask)            \
    ((reasonMask & PG_IDLE_SNAP_REASON_MASK_ERR) != 0)

/*!
 * @PG Logic state
 *
 * PG logic state is defined by PG logic event and PgCtrl Mask. Each PG logic
 * does processing based on current state.
 */
struct PG_LOGIC_STATE
{
    // PG Ctrl ID - RM_PMU_LPWR_CTRL_ID_xyz
    LwU32 ctrlId;

    // PG logic event - PMU_PG_EVENT_xyz
    LwU32 eventId;

    // Additional data
    LwU32 data;
};

/*!
 * @brief Centralized LPWR callback IDs
 *
 * AP_GR:   Adaptive GR callback
 * AP_DI:   Adaptive DI callback
 * AP_MSCG: Adaptive MSCG callback
 */
enum
{
    LPWR_CALLBACK_ID_AP_GR = 0x0,
    LPWR_CALLBACK_ID_AP_DI      ,
    LPWR_CALLBACK_ID_AP_MSCG    ,
    LPWR_CALLBACK_ID__COUNT     ,
};

/*!
 * @brief helper macro to define INVALID
 */
#define LPWR_CALLBACK_ID_AP__ILWALID    LPWR_CALLBACK_ID__COUNT

/*!
 * @brief LPWR callback controller manages individual callbacks
 */
typedef struct
{
    //
    // Number of times base callback exelwted without triggering controller
    // callback. Framework exelwtes controller callback when baseCount
    // reaches to baseMultiplier.
    //
    LwU8   baseCount;

    //
    // Helps to define callback period for this controller
    // Callback period = baseMultiplier * basePeriodUs
    //
    // Ref @LPWR_CALLBACK for basePeriodUs.
    //
    LwU8   baseMultiplier;

    //
    // Immediately execute the callback.
    //
    // ISO MSCG has tight restrictions on exit latency. Violation of exit
    // latency timing can lead to underflow. Its likely possible that display
    // HW trigger the wake-up for ISO MSCG but LPWR task is not able to service
    // that wake-up in time as task was exelwting the callback. This will lead
    // to underflow. To avoid violation of exit latency, callback exelwtion is
    // aligned with ISO-MSCG exit.
    //
    // This is the behaviour -
    // LW_TRUE : Immediately execute callback independent of ISO MSCG state
    // LW_FALSE: Wake ISO MSCG, execute the callback on MSCG exit/abort.
    //
    LwBool bImmediateExelwtion;
} LPWR_CALLBACKCTRL;

/*!
 * @brief Cetralized PG callback
 */
typedef struct
{
    // Base sampling period in usec
    LwU32               basePeriodUs;

    // Scheduled callback mask. Its a mask of LPWR_CALLBACK_ID_xyz.
    LwU32               scheduledMask;

    //
    // Mask of pending callback exelwtion. Pending callback exelwted on MS exit.
    // Refer doc @LPWR_CALLBACKCTRL.bImmediateExelwtion
    //
    LwU32               exelwtionPendingMask;

    // MS wake in progress
    LwBool              bWakeMsInProgress;

    // Callback controller
    LPWR_CALLBACKCTRL   callbackCtrl[LPWR_CALLBACK_ID__COUNT];
} LPWR_CALLBACK;

/*!
  * Structure for 64-bit ns PG stats via the PMU_PG_STATS_64 feature.  This
  * structure is used 64-bit ns counter values which can be consumed from other
  * PMU tasks which need them.  Scaling the existing 32-bit us values from @ref
  * RM_PMU_PG_STATISTICS is not a viable option.
  *
  * @note In follow-up, PG teams could decide to move RM_PMU_PG_STATISTICS to
  * 64-bit ns values and we can remove this separate structure/feature.
 */
typedef struct
{
    /*!
     * Resident time (ns) in the PG feature during the latest iteration.
     */
    LwU64 residentTimeNs;
    /*!
     * Total sleep time (ns) in the PG feature.
     */
    LwU64 totalSleepTimeNs;
} PGSTATE_STATS_64;

/*!
 * PG STATE Object consists of per-powergatable SW states and HW interface
 * needed by PG State Machine Logic.
 *
 * NOTE: This object is only applicable to powergatable entities that share the
 * same PG powergating attributes and algorithms and it only represents one
 * type (ELPG) of PG mechanisms that PMU may possibly support in future.
 */
struct OBJPGSTATE
{
    // PG controller Id: GR, MS, DI etc
    LwU32                id;

    // Powergatable Interface
    POWERGATABLE         pgIf;

    // Timestamp used for statistics computations
    FLCN_TIMESTAMP       timeStampCacheNs;

#if PMUCFG_FEATURE_ENABLED(PMU_PG_STATS_64)
    PGSTATE_STATS_64     stats64;
#endif

    // Current PG-SM's State - PMU_PG_STATE_xyz
    volatile LwU32       state;

    // Bit Mask of all the lwrrently pending states/operations
    LwU32                statePending;

    // Engine holdoff mask for PgCtrl
    LwU32                holdoffMask;

    // Idle Mask for PgCtrl
    LwU32                idleMask[RM_PMU_PG_IDLE_BANK_SIZE];

    // Support Mask for PgCtrl
    LwU32                supportMask;

    // Enabled Mask for PgCtrl
    LwU32                enabledMask;

    //
    // Disallow reason mask - PG_CTRL_ALLOW_DISALLOW_REASON_MASK_xyz
    //
    // Mask of all clients/reasons that has requested to disable PgCtrl.
    // FSM set PG_CTRL_ALLOW_DISALLOW_REASON_MASK_xyz bit when it starts
    // processing disable request from given client.
    //
    volatile LwU32       disallowReasonMask;

    //
    // Its mask of PG_CTRL_ALLOW_DISALLOW_REASON_MASK_xyz. It notifies pending
    // DISALLOW requests.
    //
    // DISALLOW request wakes PgCtrl and locks it to DISALLOW state. Its likely
    // possible that FSM is servicing DISALLOW request for client_x and client_y
    // requested DISALLOW. In such scenario, FSM continue the processing of
    // client_x request and marks client_y as pending for DISALLOW_ACK i.e.
    //  cache the client_y request. FSM uses this cache to send DISALLOW_ACK
    // after completion of DISALLOW (i.e. FSM moved to DISALLOW state).
    //
    LwU32                disallowAckPendingMask;

    // Avoid the duplicate requests for pgCtrlAllow(), pgCtrlDisallow() APIs
    LwU32                disallowReentrancyMask;

    // List of parents PgCtrl that have disabled current PgCtrl
    LwU32                parentDisallowMask;

    // Disallow reason mask for HW allow/disallow APIs
    LwU32                hwDisallowReasonMask;

    // Cahce to store current state of idle signals.
    LwU32                idleStatusCache[RM_PMU_PG_IDLE_BANK_SIZE];

    //
    // Array to hold the latest value of subfeature masks as requested
    // by each client that can enable/disable a subfeature
    //
    LwU32                enabledMaskClient[LPWR_CTRL_SFM_CLIENT_ID__COUNT];

    //
    // Variable to hold the values of new enabledMask till we update
    // the actual enabledMask in GR/MS/DI
    //
    LwU32                requestedMask;

    //
    // Variable to hold the mask of enabled priv blockers.
    // Priv Blocker can be
    // a. XVE Priv Blocker
    // b. SEC2 Priv Blocker
    // c. GSP Priv Blocker
    //
    // Every time there is a change in SFM bit for these priv blockers,
    // this mask will be updated.
    //
    LwU32               privBlockerEnabledMask;

    // Idle snap reason mask - PG_IDLE_SNAP_REASON_xyz
    LwU32               idleSnapReasonMask;

    // Auto-wakeup interval in ms
    LwU32               autoWakeupIntervalMs;

    //
    // Structure for holding pg stats
    //
    RM_PMU_PG_STATISTICS stats;

    // PgCtrl idle and PPU thresholds
    PG_CTRL_THRESHOLD    thresholdCycles;

    // Set when PgCtrl disallowed by RM
    LwBool               bRmDisallowed;

    //
    // When set, FSM logic will automatically DISALLOW PgCtrl on exit.
    // This feature is only supported by LPWR_ENG.
    //
    LwBool               bSelfDisallow;

    // Disallow count for external requests
    volatile LwU8        extDisallowCnt;

    // PG_ENG/LPWR_ENG HW IDX
    LwU8                 hwEngIdx;

    // IDX for IDLE_SUPP Mask
    LwU8                 idleSuppIdx;

    // Wakeup Reason Type (Normal/Cumulative)
    LwBool               bLwmulativeWakeupMask;

    //
    // Task state used to prevent task thrashing and thus starvation of
    // non-LPWR task because of LPWR features.
    //
    PG_STATE_TASK_MGMT   taskMgmt;
};

/*!
 * Accessor macro to retrieve a pointer to the @ref PGSTATE_STATS_64 structure.  Used
 * to encapsulate the PMU_PG_STATS_64 feature enablement (and avoiding DMEM
 * waste) without needing to scatter "#if" throughout code.
 *
 * @param[in] _pPgState
 *     Pointer to @ref OBJPGSTATE structure
 *
 * @return Pointer to corresponding @ref PGSTATE_STATS_64 structure.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PG_STATS_64)
#define PGSTATE_STATS_64_GET(_pPgState) \
    (&((_pPgState)->stats64))
#else
#define PGSTATE_STATS_64_GET(_pPgState) \
    ((PGSTATE_STATS_64 *)NULL)
#endif // PMUCFG_FEATURE_ENABLED(PMU_PG_STATS_64)

/*!
 * @brief Structure used to send LPWR FSM realted events to LPWR task
 */
struct DISPATCH_LPWR_FSM_EVENT
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR hdr;

    // PG Ctrl ID - RM_PMU_LPWR_CTRL_ID_xyz
    LwU8          ctrlId;

    // FSM event ID - PMU_PG_EVENT_xyz
    LwU8          eventId;

    // Additional data
    LwU32         data;
};

/*!
 * @brief Structure used to send external events
 */
struct DISPATCH_LPWR_EXT_EVENT
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR   hdr;

    // PgCtrl ID
    LwU8            ctrlId;

    // Event ID
    LwU8            eventId;
};

/*!
 * @brief Structure used to send PG_ENG/LPWR_ENG intterrupts
 */
struct DISPATCH_LPWR_INTR_ELPG
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    // PgCtrl ID
    LwU8            ctrlId;

    // Interrupt value/stat
    LwU32           intrStat;
};

/*!
 * @brief Structure used to send EI Notification to Clients
 */
typedef struct
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR hdr;

    // EI notification data for clients
    RM_PMU_LPWR_EI_NOTIFICATION_DATA data;
} DISPATCH_LPWR_LP_EI_NOTIFICATION;

/*!
 * @brief Structure used to receive Gr Rg perf script completion.
 */
typedef struct
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR   hdr;
} DISPATCH_LPWR_GR_RG_PERF_SCRIPT_COMPLETION;

/*!
 * @brief Uninon defining one block in LPWR task queue
 */
union DISPATCH_LPWR
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    DISPATCH_LPWR_INTR_ELPG                     intr;
    DISPATCH_LPWR_FSM_EVENT                     fsmEvent;
    DISPATCH_LPWR_EXT_EVENT                     extEvent;
    DISPATCH_PERF_CHANGE_SEQ_NOTIFICATION       perfChange;
    DISPATCH_LPWR_LP_EI_NOTIFICATION            eiNotification;
    DISPATCH_LPWR_GR_RG_PERF_SCRIPT_COMPLETION  grRgPerfScriptCompletion;
};

/*!
 * @brief ALLOW/DISALLOW reason mask
 *
 * This mask is used to identify client that allow/disallow PgCtrl
 * _RM             : RM RPC
 * _PMU_API        : PMU API exposed to non-LPWR task
 * _PARENT         : Parent of given PgCtrl
 * _SV_INTR        : Supervisor interrupt
 * _THRASHING      : Task thrashing detection
 * _SFM            : Sub-feature mask
 * _THRESHOLD      : Idle and/or PPU threshold update
 * _IDLE_SNAP      : Idle snap interrupt detected error
 * _PERF           : Perf module
 * _SELF           : FSM self allow/disallow
 * _LPWR_GRP       : LPWR group mutual exclusion policy
 * _GR_RG          : Disallow due to GR_RG exit
 * _DFPR           : Disallow due to DFPR Entry/Exit
 */
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_RM              BIT(0)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PMU_API         BIT(1)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PARENT          BIT(2)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_SV_INTR         BIT(3)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_THRASHING       BIT(4)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_SFM             BIT(5)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_THRESHOLD       BIT(6)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_IDLE_SNAP       BIT(7)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_PERF            BIT(8)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_SELF            BIT(9)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_LPWR_GRP        BIT(10)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_GR_RG           BIT(11)
#define PG_CTRL_ALLOW_DISALLOW_REASON_MASK_DFPR            BIT(12)

/*!
 * @brief Helper macros to get ALLOW and DISALLOW reason mask
 */
#define PG_CTRL_DISALLOW_REASON_MASK(reason)                                  \
                (PG_CTRL_ALLOW_DISALLOW_REASON_MASK_##reason)
#define PG_CTRL_ALLOW_REASON_MASK(reason)                                     \
                (PG_CTRL_ALLOW_DISALLOW_REASON_MASK_##reason)

/*!
 * @brief Check for duplicate DISALLOW request for pgCtrlDisallow() API
 */
#define PG_CTRL_IS_DISALLOW_REENTRANCY(pPgSt, reason)                        \
                (((pPgSt)->disallowReentrancyMask & PG_CTRL_DISALLOW_REASON_MASK(reason)) != 0)

/*!
 * @brief Helper macro to check DISALLOW_ACK reason
 */
#define PG_LOGIC_IS_DISALLOW_ACK(pPgLogicSt, ackReason)                       \
                ((PG_CTRL_ALLOW_DISALLOW_REASON_MASK_##ackReason & (pPgLogicSt->data)) != 0)

/*!
 * @brief Pending states in PG SM
 *
 * These bits are set when we would like to do some action on PG event coming
 * in near future.
 *
 * DISALLOW:
 *      DISALLOW event needs to be process when PgCtrl is in PWR_ON state.
 *      But it's quite possible that DISALLOW even came to FSM when it's not
 *      in PWR_ON state. SM sets pending states ensure that DIALLOW even will
 *      get process on next transition to PWR_ON state.
 *
 * WAKEUP:
        Processing pending for WAKEUP event.
 *
 * AP_THRESHOLD_UPDATE:
 *      Update PgCtrl thresholds based on AP settings.
 */
#define PG_STATE_PENDING_DISALLOW                   BIT(0)
#define PG_STATE_PENDING_WAKEUP                     BIT(1)
#define PG_STATE_PENDING_AP_THRESHOLD_UPDATE        BIT(2)

// Macros to set, clear and check status of Pending State Bits.
#define PG_SET_PENDING_STATE(pPgState, state)                                 \
              ((pPgState)->statePending |= PG_STATE_PENDING_##state)

#define PG_CLEAR_PENDING_STATE(pPgState, state)                               \
              ((pPgState)->statePending &= (~PG_STATE_PENDING_##state))

#define PG_IS_STATE_PENDING(pPgState, state)                                  \
              (((pPgState)->statePending & PG_STATE_PENDING_##state) != 0)

/*!
 * @brief Wake-up event reason for PgCtrl
 */
enum
{
    PG_WAKEUP_EVENT_HOLDOFF                 = 0x1,
    PG_WAKEUP_EVENT_MS_HOST_BLOCKER         = 0x2,
    PG_WAKEUP_EVENT_XVE_PRIV_BLOCKER        = 0x4,
    PG_WAKEUP_EVENT_MS_DISPLAY_BLOCKER      = 0x8,
    PG_WAKEUP_EVENT_MS_PM_BLOCKER           = 0x10,
    PG_WAKEUP_EVENT_MS_CALLBACK             = 0x20,
    PG_WAKEUP_EVENT_DFPR_PREFETCH           = 0x40,
    PG_WAKEUP_EVENT_UNUSED_1                = 0x80,
    PG_WAKEUP_EVENT_MS_ISO_BLOCKER          = 0x100,
    PG_WAKEUP_EVENT_MS_ISO_STUTTER          = 0x200,
    PG_WAKEUP_EVENT_USUSED_2                = 0x400,
    PG_WAKEUP_EVENT_MS_DMA_SUSP_ABORT       = 0x800,
    PG_WAKEUP_EVENT_PMU_WAKEUP_EXT          = 0x1000,
    PG_WAKEUP_EVENT_UNUSED_3                = 0x2000,
    PG_WAKEUP_EVENT_MS_HSHUB_BLOCKER        = 0x4000,
    PG_WAKEUP_EVENT_UNUSED_4                = 0x8000, // SSC-TODO
    PG_WAKEUP_EVENT_UNUSED_5                = 0x10000,
    PG_WAKEUP_EVENT_UNUSED_6                = 0x20000,
    PG_WAKEUP_EVENT_UNUSED_7                = 0x40000,
    PG_WAKEUP_EVENT_SEC2_FB_BLOCKER         = 0x80000,
    PG_WAKEUP_EVENT_SEC2_PRIV_BLOCKER       = 0x100000,
    PG_WAKEUP_EVENT_UNUSED_8                = 0x200000,
    PG_WAKEUP_EVENT_IDLE_SNAP               = 0x400000,
    PG_WAKEUP_EVENT_EARLY_ALLOW             = 0x800000,
    PG_WAKEUP_EVENT_GSP_FB_BLOCKER          = 0x1000000,
    PG_WAKEUP_EVENT_GSP_PRIV_BLOCKER        = 0x2000000,
};

/*!
 * The PG_LOG structure is used to help implement the PG log functionality of
 * the PMU. The PMU logs certain info about PG events as they happen to a pre-
 * reserved area in DMEM (staging record). The maximum number of events that
 * the DMEM area can hold is fixed. The PMU attempts to flush the DMEM contents
 * to the PG log buffer once the number of events in the DMEM reaches the value
 * specified by flushWaterMark or eventsPerNSI (whichever is lower). Only non-
 * zero value of eventsPerNSI is considered. If the PMU cannot flush the DMEM
 * contents right-away (due to MSCG engaged), it retries again after MSCG is
 * dis-engaged. The PMU also sends an interrupt to the RM after every flush if
 * eventsPerNSI is non-zero. The stopPolicy member of the PG_LOG struct
 * controls if and when the PMU halts exelwtion. Lwrrently only one value of
 * stopPolicy is supported (RM_PMU_PG_LOG_STOP_POLICY_FULL), meaning the PMU
 * halts if the PG log buffer is full.
 */
typedef struct
{
    LwU16       *pOffset;         //!< DMEM offset of the PG log staging record.
    LwBool       bInitialized;    //!< Is PG log init cmd received by PMU?
    LwBool       bMsDmaSuspend;   //!< Set when DMA is suspended by MS
    LwU32        recordId;        //!< The current record id.
    LwU32        putOffset;       //!< Put offset of the PG log buffer.
                                  //!< (PMU writes at this offset)
    LwBool       bFlushRequired;  //!< Set to LW_TRUE if a flush from DMEM
                                  //!< to PG log surface is required.
    RM_PMU_PG_LOG_PARAMETERS  params;   //<! operational parameters
} PG_LOG;

/*!
 * @brief Map between the PG PMU event Id to the RM PG_EVENT event Id
 */
typedef struct
{
    // Unified PG event ID - PMU_PG_EVENT_xyz
    LwU8  pmuEventId;

    // PG_LOG event ID - PMU_PG_LOG_EVENT_xyz
    LwU8  logEventId;
} PG_LOG_EVENTMAP;

/*!
 * @brief PG logging event IDs
 */
#define PMU_PG_LOG_EVENT_ILWALID           (0x0U)
#define PMU_PG_LOG_EVENT_UNUSED_1          (0x1U)
#define PMU_PG_LOG_EVENT_UNUSED_2          (0x2U)
#define PMU_PG_LOG_EVENT_PG_ON             (0x3U)
#define PMU_PG_LOG_EVENT_PG_ON_DONE        (0x4U)
#define PMU_PG_LOG_EVENT_ENG_RST           (0x5U)
#define PMU_PG_LOG_EVENT_CTX_RESTORE       (0x6U)
#define PMU_PG_LOG_EVENT_UNUSED_3          (0x7U)
#define PMU_PG_LOG_EVENT_UNUSED_4          (0x8U)
#define PMU_PG_LOG_EVENT_DENY_PG_ON        (0x9U)
#define PMU_PG_LOG_EVENT_POWERED_DOWN      (0xAU)
#define PMU_PG_LOG_EVENT_POWERING_UP       (0xBU)
#define PMU_PG_LOG_EVENT_POWERED_UP        (0xLW)
#define PMU_PG_LOG_EVENT_UNUSED_5          (0xDU)
#define PMU_PG_LOG_EVENT_UNUSED_6          (0xEU)

/*!
 * [ELCG state defines]
 * In RUN mode, the engine clock will run always ie no ELCG.
 * IN AUTO mode, the engine clock will run when the engine is non-idle ie
 * ELCG can happen.
 * IN STOP mode, the engine clocks will be stopped ie no clocks will be
 * generated for that engine.
 * Defining these values same as manual. Make sure they match to manual values
 * when in use too. you can check it by putting compile time asserts
 * using PMU_CT_ASSERT.
 */
#define PG_ELCG_STATE_ENG_CLK_RUN       0x00000000U
#define PG_ELCG_STATE_ENG_CLK_AUTO      0x00000001U
#define PG_ELCG_STATE_ENG_CLK_STOP      0x00000002U

/*!
 * @brief Macro to indicate invalid command sequence no.
 */
#define PG_CMD_SEQ_TO_ACK_ILWALID          LW_U16_MAX

/*!
 * @brief Hepler Macro to get the entry count of a PG controller
 */
#define PG_STAT_ENTRY_COUNT_GET(ctrlId)                         \
        GET_OBJPGSTATE(ctrlId)->stats.entryCount;

/* ------------------------ External definitions --------------------------- */
/*!
 * PG object Definition
 */
typedef struct
{
    // Primary Pg State Objects AKA PgCtrl object
    OBJPGSTATE    *pPgStates[RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE];

    // Manages centralized PG callbacks
    LPWR_CALLBACK *pCallback;

#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    OS_TIMER       osTimer;
#endif

    // supported pg mask
    LwU32          supportedMask;

    // LPWR ARCH sub-feature support mask
    LwU32          archSfSupportMask;

    //
    // GPIO pin mask
    // The "set" bit in this mask denotes the SCI GPIO pin number which needs
    // to be in sync with corresponding PMGR GPIO pin
    //
    // This variable falls under OBJPGISLAND. But we are keeping in OBJPG
    // as we don't have OBJPGISLAND object in PMU
    //
    LwU32          gpioPinMask;

    // Array of VOLT Rail objects
    RM_PMU_PG_VOLT_RAIL voltRail[PG_VOLT_RAIL_IDX_MAX];

    // Mask of overlays attached to PG task. Refer @PG_OVL_ID_MASK_*.
    LwU32          olvAttachedMask;

    // Buffer for GC6 related data transfer from FB/SYS MEM to DMEM
    LwU8           *dmaReadBuffer;

    // True if SYNCGPIO related overlays are attached to PG task
    LwBool         bSyncGpioAttached;

    // True if its a no pstate vbios
    LwBool         bNoPstateVbios;

    // State - PG_RTOS_IDLE_EVENT_STATE_* of RTOS idle notification to PG task
    LwU8           rtosIdleState;

    // Indicates if the OSM ack is pending or not
    LwBool         bOsmAckPending;

    // Colwerts PG_ENG/LPWR_ENG HW IDX to PgCtrl SW ID
    LwU8           hwEngIdxMap[PG_ENG_IDX__COUNT];
} OBJPG;

extern OBJPG  Pg;
extern PG_LOG PgLog;
extern RM_PMU_PG_STATISTICS *pPgStatistics[RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE];
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Send event to LPWR task
FLCN_STATUS pgCtrlEventSend(LwU32 ctrlId, LwU32 eventId, LwU32 data)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlEventSend");
FLCN_STATUS pgCtrlEventColwertOrSend(PG_LOGIC_STATE *, LwU32 ctrlId, LwU32 eventId, LwU32 data);

// PgCtrl APIs
FLCN_STATUS pgCtrlInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "pgCtrlInit");

void        pgCtrlAllow(LwU32 ctrlId, LwU32 reasonMask)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlAllow");
LwBool      pgCtrlDisallow(LwU32 ctrlId, LwU32 reasonMask)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlDisallow");
void        pgCtrlAllowExt(LwU32 ctrlId)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlAllowExt");
void        pgCtrlDisallowExt(LwU32 ctrlId)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlDisallowExt");
void        pgCtrlWakeExt(LwU32 ctrlId)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlWakeExt");

// PgCtrl HW wake-up APIs
void        pgCtrlHwDisallow(LwU32 ctrlId, LwU32 reasonMask)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlHwDisallow");
void        pgCtrlHwAllow(LwU32 ctrlId, LwU32 reasonMask)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgCtrlHwAllow");

// PG Statistic functions
void        pgStatRecordEvent            (LwU8, LwU32);
void        pgStatUpdate                 (LwU8);
void        pgStatSleepTimeUpdate        (LwU8);
void        pgStatPoweredUpTimeUpdate    (LwU8);
void        pgStatDeltaSleepTimeGetUs    (LwU8, PGSTATE_STAT_SAMPLE *);
void        pgStatDeltaPoweredUpTimeGetUs(LwU8, PGSTATE_STAT_SAMPLE *);
void        pgStatLwrrentSleepTimeGetNs  (LwU8, PFLCN_TIMESTAMP)
            GCC_ATTRIB_SECTION("imem_libLpwr", "pgStatLwrrentSleepTimeGetNs");

// PG Logging functions
void       _pgLogEvent             (LwU8, LwU32, LwU32);
void       _pgLogFlush             (void);
void       _pgLogPreInit           (void)
           GCC_ATTRIB_SECTION("imem_init", "_pgLogPreInit");
void       _pgLogNotifyMsDmaSuspend(void);
void       _pgLogNotifyMsDmaResume (void);
void       _pgLogInit              (RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_LOG_INIT *)
           GCC_ATTRIB_SECTION("imem_lpwrLoadin", "_pgLogInit");

#if PMUCFG_FEATURE_ENABLED(PMU_PG_LOG)
#define pgLogInit                     _pgLogInit
#define pgLogEvent                    _pgLogEvent
#define pgLogFlush                    _pgLogFlush
#define pgLogPreInit                  _pgLogPreInit
#define pgLogNotifyMsDmaSuspend       _pgLogNotifyMsDmaSuspend
#define pgLogNotifyMsDmaResume        _pgLogNotifyMsDmaResume
#else
#define pgLogInit(p)
#define pgLogEvent(e,t,s)             lwosVarNOP(e,t,s)
#define pgLogFlush(v)
#define pgLogPreInit()
#define pgLogNotifyMsDmaSuspend()
#define pgLogNotifyMsDmaResume()
#endif

// PG Initialization Routines
void        pgPreInit         (void)
            GCC_ATTRIB_SECTION("imem_init", "pgPreInit");

// PG Logic Interfaces
FLCN_STATUS pgLogicAll         (PG_LOGIC_STATE *);
void        pgLogicStateMachine(PG_LOGIC_STATE *);
void        pgLogicOvlPre      (PG_LOGIC_STATE *);
void        pgLogicOvlPost     (PG_LOGIC_STATE *);
FLCN_STATUS pgLogicWar         (PG_LOGIC_STATE *);

// Overlay routines
void   pgOverlayAttachAndLoad        (LwU32 ovlIdMask);
LwBool pgOverlayAttach               (LwU32 ovlIdMask);
void   pgOverlayDetach               (LwU32 ovlIdMask);
void   pgOverlaySyncGpioAttachAndLoad(void);
void   pgOverlaySyncGpioDetach       (void);

// Thrashing detection and prevention routines
void   pgCtrlThrashingDisallow  (LwU32 ctrlId);

// Thresholds related APIs
void    pgCtrlThresholdsUpdate(LwU32 ctrlId, PG_CTRL_THRESHOLD *pReqThresholdCycles);
LwBool  pgCtrlIsPpuThresholdRequired(LwU32 ctrlId);

// SFM functions
void    pgCtrlSubfeatureMaskUpdate(LwU8);
LwBool  pgCtrlSubfeatureMaskRequest(LwU8, LwU8, LwU32);
LwBool  pgCtrlIsSubFeatureSupported(LwU32, LwU32);

#endif // PMU_OBJPG_H
