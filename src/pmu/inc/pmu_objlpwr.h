/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJLPWR_H
#define PMU_OBJLPWR_H

/*!
 * @file pmu_objlpwr.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "pmu/pmuif_lpwr_vbios.h"
#include "pmu_objpg.h"
#include "pmu_lpwr_test.h"

/* ---------------------- Forward Declarations ----------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config/g_lpwr_hal.h"

/* ------------------------ Macros & Defines ------------------------------- */
#if (PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV)
#define PRIV_BLOCKER_GR_PROFILE_WL_RANGES_INIT(_OP_)  _OP_(0, 0)
#define PRIV_BLOCKER_GR_PROFILE_BL_RANGES_INIT(_OP_)  _OP_(0, 0)
#define PRIV_BLOCKER_MS_PROFILE_WL_RANGES_INIT(_OP_)  _OP_(0, 0)
#define PRIV_BLOCKER_MS_PROFILE_BL_RANGES_INIT(_OP_)  _OP_(0, 0)
#endif // (PMU_PROFILE_GH20X || PMU_PROFILE_GH20X_RISCV)
/*!
 * @brief Macro to check the support for Sub features of LPWR Ctrl
 */
#define LPWR_CTRL_IS_SF_SUPPORTED(pPgState, featureName, sfName)                   \
            (LPWR_IS_SF_MASK_SET(featureName, sfName, pPgState->supportMask))

/*!
 * @brief Macro to check the enable state for Sub features of LPWR Ctrl
 */
#define LPWR_CTRL_IS_SF_ENABLED(pPgState, featureName, sfName)                     \
            (LPWR_IS_SF_MASK_SET(featureName, sfName, pPgState->enabledMask))

/*!
 * @brief Debug macros for Perf change
 */
#if PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)
// Generic Lpwr Debug macros
#define LPWR_DEBUG_INFO_UPDATE(idx, data)            (LpwrDebugInfo[idx] = data)
// Perf Debug macros
#define LPWR_PERF_DEBUG_CNTR_UPDATE(perfEvent)       ((LpwrPerfCntr##perfEvent)++)
#define LPWR_PERF_DEBUG_LAST_IDX_UPDATE(perfIdx)     (LpwrPerfLastIdx = perfIdx)
#define LPWR_PERF_DEBUG_LWRR_IDX_UPDATE(perfIdx)     (LpwrPerfLwrrIdx = perfIdx)
// Event Debug macros
#define LPWR_EVENT_DEBUG_INFO_UPDATE(ctrlId, eventId, data)             \
do {                                                                    \
    LpwrEventInfo[LpwrLwrrEventIdx] = ((ctrlId << 24) | eventId);       \
    LpwrEventData[LpwrLwrrEventIdx] = (data);                           \
    LpwrLwrrEventIdx = (LpwrLwrrEventIdx + 1) % LPWR_EVENT_DEBUG_COUNT; \
} while (LW_FALSE)
#else
#define LPWR_DEBUG_INFO_UPDATE(idx, data)
#define LPWR_PERF_DEBUG_CNTR_UPDATE(perfEvent)
#define LPWR_PERF_DEBUG_LAST_IDX_UPDATE(perfIdx)
#define LPWR_PERF_DEBUG_LWRR_IDX_UPDATE(perfIdx)
#define LPWR_EVENT_DEBUG_INFO_UPDATE(ctrlId, eventId, data)
#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)

/*!
 * @brief Defines for LPWR debug info array index
 */
enum
{
    LPWR_DEBUG_INFO_IDX_0  ,
    LPWR_DEBUG_INFO_IDX_1  ,
    LPWR_DEBUG_INFO_IDX_2  ,
    LPWR_DEBUG_INFO_IDX_3  ,
    LPWR_DEBUG_INFO_IDX_4  ,
    LPWR_DEBUG_INFO_IDX_MAX,
};

/*!
 * @brief Counts of LPWR events to be logged
 */
#define LPWR_EVENT_DEBUG_COUNT             (30)

/*!
 * @brief Sleep Aware Callback clients
 *
 * _GR and _GR_PSTATE:
 *      They are controlling GR coupled sleep aware callback. GR coupled sleep
 *      aware callback will be engaged when both of these clients are activating
 *      sleep mode.
 *
 * _EI and _EI_PSTATE:
 *      They are controlling EI coupled sleep aware callback. EI coupled sleep
 *      aware callback will be engaged when both of these clients are activating
 *      sleep mode.
 *
 * _RM:
 *      This is legacy client used by version 1 of sleep aware callback.
 */
enum
{
    LPWR_SAC_CLIENT_ID_GR = 0x0 ,
    LPWR_SAC_CLIENT_ID_GR_PSTATE,
    LPWR_SAC_CLIENT_ID_EI       ,
    LPWR_SAC_CLIENT_ID_EI_PSTATE,
    LPWR_SAC_CLIENT_ID_RM       ,
    LPWR_SAC_CLIENT_ID__COUNT   ,
};

/*!
 * @Brief Available Priv Blocker Modes
 *
 * BLOCK_NONE:     Blocker will not block any Priv access
 * BLOCK_EQUATION: Blocker will only block the specific
 *                 Priv access as described by the PROFILEs
 * BLOCK_ALL:      Blocker will block every Priv access.
 *                 For this block to work, atleast one PRFOILE
 *                 must be enabled.
*/
enum
{
    LPWR_PRIV_BLOCKER_MODE_BLOCK_NONE = 0,
    LPWR_PRIV_BLOCKER_MODE_BLOCK_EQUATION,
    LPWR_PRIV_BLOCKER_MODE_BLOCK_ALL     ,
};

/*!
 *  @Brief Available Priv Blocker Profiles
 *
 *  PROFILE_NONE: Blocker will not block any Priv access
 *  PROFILE_GR:   Blocker will block only GR Specific Ranges
 *                i.e. Super set of all applicable Graphics Ranges
 *  PROFILE_MS:   BLocker will block only MS specific Ranges
 *                i.e. Super set of all applicable Memory Subsystem Ranges.
 *
 */
enum
{
    LPWR_PRIV_BLOCKER_PROFILE_NONE = 0,
    LPWR_PRIV_BLOCKER_PROFILE_GR      ,
    LPWR_PRIV_BLOCKER_PROFILE_MS      ,
};

/*!
 *  @Brief Available Priv Blocker Range Type
 *
 *  RANGE_NONE: No ranges which will be blocked by Priv Blockers
 *  RANGE_DL  : Disallow range Type
 *  RANGE_AL  : Allow Range Type
 */
enum
{
    LPWR_PRIV_BLOCKER_RANGE_NONE =   0,
    LPWR_PRIV_BLOCKER_RANGE_DISALLOW  ,
    LPWR_PRIV_BLOCKER_RANGE_ALLOW     ,
};

/*!
 * @brief Macro to extract the Disallow and Allow Ranges and
 * filling up the structure LPWR_PRIV_BLOCKER_AL_DL_RANGE
 */
#define LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGES_GET(startRange, endRange) \
        {                                                                 \
            .startAddr = startRange,                                      \
            .endAddr   = endRange                                         \
        },

/*!
 * @brief XVE Blocker timeout value for EMULATION
 */
#define LPWR_PRIV_BLOCKERS_XVE_TIMEOUT_EMU_US         0xFFFF

/*!
 * @brief Defines: LPWR_SEQ Ctrl Ids for LOGIC PG
 */
enum
{
    LPWR_SEQ_LOGIC_CTRL_ID_GR = 0x0,
    LPWR_SEQ_LOGIC_CTRL_ID__COUNT  ,
};

/*!
 * @brief Defines: LPWR_SEQ Ctrl Ids for SRAM PG
 */
enum
{
    LPWR_SEQ_SRAM_CTRL_ID_GR = 0x0,
    LPWR_SEQ_SRAM_CTRL_ID_MS      ,
    LPWR_SEQ_SRAM_CTRL_ID_DI      ,
    LPWR_SEQ_SRAM_CTRL_ID__COUNT  ,
};

/*!
* @brief Hepler macros define start and end for LPWR_SEQ_SRAM_CTRL_ID_xyz
*/
#define LPWR_SEQ_SRAM_CTRL_ID__START  LPWR_SEQ_SRAM_CTRL_ID_GR
#define LPWR_SEQ_SRAM_CTRL_ID__END    (LPWR_SEQ_SRAM_CTRL_ID__COUNT - 1)

/*!
 * @brief LPWR SEQUENCER States
 *
 * These stats defines the various mode of Sequencer
 * in which HW state machine of the sequencer can do
 * transition.
 *
 * PWR_ON    - No Power Gating in LOGIC or SRAMs
 * LOGIC_PG  - Logic part is Power Gated
 * SRAM_RPG  - SRAMs are in RPG Mode (Ram Power Gating)
 * SRAM_RPPG - SRAMs are in RPPG Mode (Ram Periphery Power Gating)
 *
 * States are defined here as well:
 * https://confluence.lwpu.com/display/LS/LPWR+Sequencer%3A+Design#LPWRSequencer:Design-Sequencerstates
 */
enum
{
    LPWR_SEQ_STATE_PWR_ON = 0x0,
    LPWR_SEQ_STATE_LOGIC_PG    ,
    LPWR_SEQ_STATE_SRAM_RPG    ,
    LPWR_SEQ_STATE_SRAM_RPPG   ,
};

/*!
 * @brief Time-out value to complete SRAM Sequencer transition
 *
 * HW has predicted 40us as timeout value. Adding 10us margin to avoid
 * any issues.
 */
#define LPWR_SEQ_SRAM_CTRL_COMPLETION_TIMEOUT_US            50

/*!
 * @brief Wait in NS used by MIN_WAIT_FOR_COMPLETION
 *
 * Minimum wait for triggering -
 * 1) Exit after non-blocking entry call
 * 2) Entry after non-blocking exit call
 */
#define LPWR_SEQ_SRAM_CTRL_MIN_WAIT_FOR_COMPLETION_NS       100

/*!
 * @brief Macro to get LPWR SEQ SRAM Ctrl Structure
 */
#define LPWR_SEQ_GET_SRAM_CTRL(ctrlId)              \
                   (Lpwr.pLpwrSeq->pSramSeqCtrl[ctrlId])

/*!
 * @brief Macro to check the support for SRAM SEQ CTRL
 */
#define LPWR_SEQ_IS_SRAM_CTRL_SUPPORTED(ctrlId)     \
                   ((Lpwr.pLpwrSeq->sramSeqCtrlSupportMask & BIT(ctrlId)) != 0)

/*!
 * LPWR Statistics Framework Data structures
 */

/*!
 * @brief LPWR STAT CTRL IDs
 *
 * MS_RPPG: MS Group Coupled RPPG
 * MS_RPG:  MS Group Coupled RPG
 */
enum
{
    LPWR_STAT_CTRL_ID_MS_RPPG = 0x0,
    LPWR_STAT_CTRL_ID_MS_RPG,
    LPWR_STAT_CTRL_ID__COUNT,
};

/*!
 * @brief LPWR STAT feature States
 *
 */
enum
{
    LPWR_STAT_ENTRY_START = 0x0,
    LPWR_STAT_ENTRY_END,
    LPWR_STAT_EXIT_START,
    LPWR_STAT_EXIT_END,
};

/*!
 * LPWR STAT Ctrl structure for each feature
 */
typedef struct
{
    // Entry Count
    LwU32 entryCount;

    // Average Entry Latency in microseconds
    LwU32 avgEntryLatencyUs;

    // Average Exit Latency in microseconds
    LwU32 avgExitLatencyUs;

    // Resident Time in microseconds - it will be commulative time
    LwU32 residentTimeUs;

    // Timer to keep track of time.
    FLCN_TIMESTAMP timerNs;

    // Feature Engage status
    LwBool  bEngaged;
} LPWR_STAT_CTRL;

/*!
 * LPWR STAT structure for managing the LPWR Statistics framework
 */
typedef struct
{
    // Structure pointer for all the LPWR_STAT_CTRLs
    LPWR_STAT_CTRL *pStatCtrl[LPWR_STAT_CTRL_ID__COUNT];

    // Timer to keep track of time.
    FLCN_TIMESTAMP  wallTimerNs;

    // Total wall clock time in uS;
    LwU32           wallTimeUs;
} LPWR_STAT;

// Get LPWR STAT and Lpwr STAT ctrl pointers from OBJLPWR
#define LPWR_GET_STAT()                  (Lpwr.pLpwrStat)
#define LPWR_GET_STAT_CTRL(ctrlId)       ((Lpwr.pLpwrStat->pStatCtrl[ctrlId]))

/*!
 * @brief Macro to get Clock Gating Structure
 */
#define LPWR_GET_CG()                       (Lpwr.pLpwrCg)

/*!
 * @brief Macro to get LPWR Priv Blocker Structure
 */
#define LPWR_GET_PRIV_BLOCKER()             (Lpwr.pPrivBlocker)

/*!
 * @brief Macro to get LPWR Priv Blocker CTRL Structure
 */
#define LPWR_GET_PRIV_BLOCKER_CTRL(blkrId)  \
                                  (&Lpwr.pPrivBlocker->privBlockerCtrl[blkrId])

/*!
 * @brief Macro to check Priv blocker Wakeup pending
 */
#define lpwrPrivBlockerIsWakeupPending(ctrlId)  \
            ((pLpwrPrivBlocker->wakeUpReason[ctrlId] != 0) ? LW_TRUE : LW_FALSE)

/*!
 * @brief LPWR_LP Task events types - Event coming from RM or External Task
 */
#define LPWR_LP_EVENT_EI_FSM                (DISP2UNIT_EVT__COUNT + 0U)
#define LPWR_LP_EVENT_EI_CLIENT_REQUEST     (DISP2UNIT_EVT__COUNT + 1U)
#define LPWR_LP_EVENT_DIFR_PREFETCH_REQUEST (DISP2UNIT_EVT__COUNT + 2U)
#define LPWR_LP_LPWR_FSM_IDLE_SNAP          (DISP2UNIT_EVT__COUNT + 3U)
#define LPWR_LP_PSTATE_CHANGE_NOTIFICATION  (DISP2UNIT_EVT__COUNT + 4U)

/*!
 * @brief LPWR_LP Task Event Ids
 */
#define LPWR_LP_EVENT_ID_NONE                           0U

/*!
 * @brief EI Related Event Ids
 */
#define LPWR_LP_EVENT_ID_EI_ILWALID                     0U
#define LPWR_LP_EVENT_ID_EI_FSM_ENTRY_DONE              1U
#define LPWR_LP_EVENT_ID_EI_FSM_EXIT_DONE               2U
#define LPWR_LP_EVENT_ID_EI_CLIENT_NOTIFICATION_ENABLE  3U
#define LPWR_LP_EVENT_ID_EI_CLIENT_NOTIFICATION_DISABLE 4U

/*!
 * @brief Macro to extract the Partition profile setting and
 * filling up the structure LPWR_SRAM_PG_PROFILE_INIT
 */
#define EXTRACT_LPWR_SRAM_PG_PROFILE_CONFIG(addr, data, count) \
        {                                                      \
          .regAddr  = addr,                                    \
          .regVal   = data,                                    \
          .numWrite = count                                    \
        },

/*!
 * @brief PRIV Flush Timeout
 *
 * TODO-pankumar: Working with HW folks to get optimum timeout.
 */
#define LPWR_PRIV_FLUSH_DEFAULT_TIMEOUT_NS              (100 * 1000)

/*!
 * @brief MailBox Flush Value
 */
#define PMU_LPWR_PRIV_PATH_FLUSH_VAL                    0x1234

/*!
 * Macro to colwert the SRAM PG Mode to Profile Mapping
 * Mapping of Profile and PG Mode is present in Bug: 200525560
 * Mapping is defined as follow:
 *
 * SNo. Domain    PROFILE     Meaning  Feature Mapping
 * 1.    GR       PROFILE0     RPPG      GR-RPPG
 * 2.    GR       PROFILE1     RPG       GR-RPG
 * 3.    GR       PROFILE2     POWER_ON  NA
 * 4.    GR       PROFILE3     POWER_ON  NA
 * 5.    MS       PROFILE0     RPPG      MS-RPPG
 * 6.    MS       PROFILE1     RPPG/RPG  L2-RPG/MS-RPPG
 * 7.    MS       PROFILE2     POWER_ON  NA
 * 8.    MS       PROFILE3     POWER_ON  NA
 *
 * These mapping are comming from the file ram_profile_init_seq.h
 * This file will also have the definition for define
 * SRAM_PG_PROFILE_##Mode i.e
 * a. SRAM_PG_PROFILE_RPG
 * b. SRAM_PG_PROFILE_RPPG
 * c. SRAM_PG_PROFILE_L2RPG
 *
 * In the macro below PG_MODE can have the following values:
 * 1. RPG
 * 2. RPPG
 * 3. L2RPG
 */
#define LPWR_SRAM_PG_PROFILE_GET(PG_MODE)     SRAM_PG_PROFILE_##PG_MODE

/*!
 * Macro to colwert the LOGIC PG Mode to Profile Mapping
 * Mapping is defined as follow:
 *
 * SNo. Domain    PROFILE     Meaning  Feature Mapping
 * 1.    GR       PROFILE0     ELPG      GR-ELPG
 *
 * These mapping are comming from the file logic_profile_init_seq.h
 * This file will also have the definition for define
 * LOGIC_PG_PROFILE_##Mode i.e
 * a. LOGIC_PG_PROFILE_ELPG
 *
 */
#define LPWR_LOGIC_PG_PROFILE_GET(PG_MODE)     LOGIC_PG_PROFILE_##PG_MODE

/*!
 * @brief list of clients requesting holdoff mask update
 *
 * _GR_PG       : Holdoff Mask engaged by GR-PG Features
 * _GR_RG       : Holdoff Mask engaged by GR-RG Feature
 * _MS_MSCG     : Holdoff Mask engaged by MSCG Feature
 * _MS_LTC      : Holdoff Mask engaged by MS_LTC Feature
 * _DIFR_SW_ASR : Holdoff Mask engaged by DIFR_SW_ASR Feature
 * _SEC2_BLK_WAR: On pascal, SEC2 used combined FB and priv blocker.
 *                Blocker requires engaging holdoff as part of SW WAR 200113462
 * _DFPR        : Holdoff Mask engaged by DIFR Layer 1
 */
enum
{
    LPWR_HOLDOFF_CLIENT_ID_GR_PG    = 0x0,
    LPWR_HOLDOFF_CLIENT_ID_GR_RG         ,
    LPWR_HOLDOFF_CLIENT_ID_MS_MSCG       ,
    LPWR_HOLDOFF_CLIENT_ID_MS_LTC        ,
    LPWR_HOLDOFF_CLIENT_ID_MS_DIFR_SW_ASR,
    LPWR_HOLDOFF_CLIENT_ID_SEC2_BLK_WAR  ,
    LPWR_HOLDOFF_CLIENT_ID_DFPR          ,
    LPWR_HOLDOFF_CLIENT_ID__COUNT        ,
};

/*!
 * @brief Client IDs for LPWR Group CTRL mutual exclusion policy
 */
enum
{
    LPWR_GRP_CTRL_CLIENT_ID_RM = 0x0,
    LPWR_GRP_CTRL_CLIENT_ID_EI      ,
    LPWR_GRP_CTRL_CLIENT_ID_DFPR    ,
    LPWR_GRP_CTRL_CLIENT_ID__COUNT  ,
};

/*!
 * @brief Heler macro to define holdoff mask as zero
 */
#define LPWR_HOLDOFF_MASK_NONE          (0x0)

// Get LPWR group and Lpwr group ctrl pointers from OBJLPWR
#define LPWR_GET_GRP()                  (&(Lpwr.lpwrGrp))
#define LPWR_GET_GRP_CTRL(ctrlId)       (&(Lpwr.lpwrGrp.grpCtrl[ctrlId]))

// Get LPWR test info pointer.
#define LPWR_GET_TEST()                 (Lpwr.pLpwrTest)

/* ------------------------ Types definitions ------------------------------ */

/*!
 * @brief Structure for SRAM Partition PG Profile Init
 */
typedef struct
{
    LwU32 regAddr;
    LwU32 regVal;
    LwU32 numWrite;
} LPWR_SRAM_PG_PROFILE_INIT;

/*!
 * @brief Structure for Priv Blocker BL/WL Ranges
 */
typedef struct
{
    LwU32 startAddr;
    LwU32 endAddr;
}
LPWR_PRIV_BLOCKER_ALLOW_DISALLOW_RANGE;

/*!
 * @brief Structure for LPWR VBIOS DATA
 */
typedef struct
{
    RM_PMU_LPWR_VBIOS_IDX    idx;
    RM_PMU_LPWR_VBIOS_GR     gr;
    RM_PMU_LPWR_VBIOS_MS     ms;
    RM_PMU_LPWR_VBIOS_DI     di;
    RM_PMU_LPWR_VBIOS_AP     ap;
    RM_PMU_LPWR_VBIOS_GR_RG  grRg;
    RM_PMU_LPWR_VBIOS_EI     ei;
    RM_PMU_LPWR_VBIOS_DIFR  *pDifr;
} LPWR_VBIOS;

/*!
 * @brief Structure for LPWR GROUP CTRL
 */
typedef struct
{
    // Mask of LPWR_CTRLs that are part of current LPWR group
    LwU32    lpwrCtrlMask;

    //
    // Cache disallow request from all clients LPWR_GRP_CTRL_CLIENT_ID_xyz
    // as part of mutual exclusion policy.
    //
    LwU32    clientDisallowMask[LPWR_GRP_CTRL_CLIENT_ID__COUNT];

    //
    // Only group owner is enabled at given time rest all members are disabled
    // by mutual exclusion policy.
    //
    LwU32    ownerMask;
} LPWR_GRP_CTRL;

/*!
 * @brief Structure for LPWR GROUP info
 */
typedef struct
{
    LPWR_GRP_CTRL   grpCtrl[RM_PMU_LPWR_GRP_CTRL_ID__COUNT];
} LPWR_GRP;

/*!
 * @brief PRIV BLOCKER CTRL Structure
 */
typedef struct
{
    // Current Mode of the Blocker
    LwU32   lwrrentMode;

    // Bit mask of the profiles which are enabled
    LwU32   profileEnabledMask;
} LPWR_PRIV_BLOCKER_CTRL;

/*!
 * @brief PRIV BLOCKER Management Structure
 */
typedef struct
{
    // Support mask of all the supported Priv blockers
    LwU32                  supportMask;

    // Interrupt enabledMasddk of all priv blockers
    LwU32                  gpio1IntrEnMask;

    // Per LPWR_CTRL Wakeup Reason
    LwU32                  wakeUpReason[RM_PMU_LPWR_CTRL_ID_MAX];

    // Structure to control the Priv Blocker Mode
    LPWR_PRIV_BLOCKER_CTRL privBlockerCtrl[RM_PMU_PRIV_BLOCKER_ID_MAX];
} LPWR_PRIV_BLOCKER;

/*!
 * @brief LPWR_SEQUENCER Ctrl Structure
 */
typedef struct
{
    // State refers to LPWR_SEQ_STATE_xyz
    LwU32                        state;

    // Timeout value for the sequencer state change.
    LwU32                        timeoutUs;

    // Pointer to statistics for LPWR SEQ Ctrl
    RM_PMU_LPWR_SEQ_CTRL_STATS  *pStats;
} LPWR_SEQ_CTRL;

/*!
 * @brief LPWR_SEQUENCER Management Structure
 */
typedef struct
{
    // Support mask for LOGIC Seq Ctrls
    LwU32   logicSeqCtrlSupportMask;

    // Support mask for SRAM Seq Ctrls
    LwU32   sramSeqCtrlSupportMask;

    // LOGIC Sequencer CTRL Structures
    LPWR_SEQ_CTRL  *pLogicSeqCtrl[LPWR_SEQ_LOGIC_CTRL_ID__COUNT];

    // SRAM Sequencer CTRL Structure
    LPWR_SEQ_CTRL  *pSramSeqCtrl[LPWR_SEQ_SRAM_CTRL_ID__COUNT];
} LPWR_SEQ;

/*!
 * LPWR object Definition
 */
typedef struct
{
    // Pointer to VBIOS data cache
    LPWR_VBIOS        *pVbiosData;

    // Object to manage Lpwr Clock Gating
    LPWR_CG           *pLpwrCg;

    // Object to manage the PRIV Blockers
    LPWR_PRIV_BLOCKER *pPrivBlocker;

    //Object to manage the LPWR SEQUENCER
    LPWR_SEQ          *pLpwrSeq;

    // Structure for Lpwr Stat Info
    LPWR_STAT         *pLpwrStat;

    // Structure for Lpwr Group info
    LPWR_GRP           lpwrGrp;

    // Structure for LPWR test info.
    LPWR_TEST         *pLpwrTest;

    // Cache for client holdoff mask
    LwU32              clientHoldoffMask[LPWR_HOLDOFF_CLIENT_ID__COUNT];

    //
    // Cache value of GR-ELCG state as part of GR-ELCG <-> OSM CG WAR.
    // Refer @pmu_pgcg.c for details.
    //
    LwU32              grElcgState;

    //
    // Disable reason mask for GR-ELCG.
    // This is mask of LPWR_CG_ELCG_CTRL_REASON_xyz
    //
    LwU32              grElcgDisableReasonMask;

    //
    // Mask of LpwrCtrl which we need to disable/enable
    // during perf change
    //
    LwU32              lpwrCtrlPerfMask;

    // Cache Sleep Aware Callback client request
    LwBool             bSleepClientReq[LPWR_SAC_CLIENT_ID__COUNT];

    // Perf Index that is in effect at the start of a perf transition
    LwU8               lastPerfIdx;

    // Current Perf Index
    LwU8               targetPerfIdx;

    // Next step for Perf change handling
    LwU8               nextPerfChangeStep;

    // Set when GPU running on battery
    LwBool             bPwrSrcDc;
} OBJLPWR;

/*!
 * Below definitions are for new task: LPWR_LP
 */

/*!
 * @brief Structure used to send EI Client (PMU Clients) request to
 *  EI Framework(Task LPWR_LP)
 */
typedef struct
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR hdr;

    // EI CLient event ID - LPWR_LP_EVENT_ID_EI_CLIENT_xyz
    LwU8    eventId;

    // EI Client Id
    LwU8    clientId;
} DISPATCH_LPWR_LP_EI_CLIENT_REQUEST;

/*!
 * @brief Structure used to send EI FSM Events to LPWR_LP Task (EI Framework)
 */
typedef struct
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR hdr;

    // EI event ID - LPWR_LP_EVENT_ID_xyz
    LwU8          eventId;
} DISPATCH_LPWR_LP_EI_FSM_IDLE_EVENT;

/*!
 * @brief Structure used to send LPWR FSM Idle Snap realted events to LPWR_LP task
 */
typedef struct
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR hdr;

    // LPWR Ctrl ID - RM_PMU_LPWR_CTRL_ID_xyz
    LwU8          ctrlId;
} DISPATCH_LPWR_LP_LPWR_FSM_IDLE_SNAP_EVENT;

/*!
 * @brief Structure used to send Pstate Change Notification to LPWR_LP task
 */
typedef struct
{
    // Header - Must be first element of the structure
    DISP2UNIT_HDR hdr;

    // RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA    data;
} DISPATCH_LPWR_LP_PSTATE_CHANGE_NOTIFICATION;

/*!
 * @brief Union defining all the command to LPWR_LP task
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    DISPATCH_LPWR_LP_EI_FSM_IDLE_EVENT          eiFsmEvent;
    DISPATCH_LPWR_LP_EI_CLIENT_REQUEST          eiClientRequest;
    DISPATCH_LPWR_LP_LPWR_FSM_IDLE_SNAP_EVENT   idleSnapEvent;
    DISPATCH_LPWR_LP_PSTATE_CHANGE_NOTIFICATION pstateChangeNotification;
} DISPATCH_LPWR_LP;

/* ------------------------ External definitions --------------------------- */
extern OBJLPWR Lpwr;

// Queue to receive the I2C response
LwrtosQueueHandle LpwrI2cQueue;

/* ------------------------ Function Prototypes ---------------------------- */
// Lpwr core functions
void        lpwrPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "lpwrPreInit");
FLCN_STATUS lpwrPostInit(void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrPostInit");
void        lpwrEventHandler(DISPATCH_LPWR *pLpwrEvt, PG_LOGIC_STATE *pPgLogicState);

// LPWR_LP core Function
FLCN_STATUS lpwrLpEventHandler(DISPATCH_LPWR_LP *pLpwrLpEvt)
    GCC_ATTRIB_SECTION("imem_lpwrLp", "lpwrLpEventHandler");

// Sleep aware task management functions
LwBool  lpwrNotifyRtosIdle         (void)
    GCC_ATTRIB_SECTION("imem_resident", "lpwrNotifyRtosIdle");
LwBool  lpwrIsNonLpwrTaskIdle(void);

// Sleep aware callback arbiter
void lpwrCallbackSacArbiter(LwU32 clientId, LwBool bSleep);

// Lpwr VBIOS functions
void    lpwrVbiosPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "lpwrVbiosPreInit");
void    lpwrVbiosDataInit   (void)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrVbiosDataInit");
LwU8    lpwrVbiosGrEntryIdxGet(LwU8 perfIdx);
LwU8    lpwrVbiosMsEntryIdxGet(LwU8 perfIdx);
LwU8    lpwrVbiosDiEntryIdxGet(LwU8 perfIdx);
LwU8    lpwrVbiosGrRgEntryIdxGet(LwU8 perfIdx);
LwU8    lpwrVbiosEiEntryIdxGet(LwU8 perfIdx);
LwU8    lpwrVbiosDifrEntryIdxGet(LwU8 perfIdx);

// Lpwr Perf change functions
void        lpwrPerfChangePreSeq(void);
void        lpwrPerfChangePostSeq(void);
void        lpwrProcessPstateChange(RM_PMU_RPC_STRUCT_LPWR_PSTATE_CHANGE *);
FLCN_STATUS lpwrPerfChangeEvtHandler(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA perfNotificationData);
FLCN_STATUS lpwrPerfPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPerfPostInit");

// Centralised LPWR callback prototypes
FLCN_STATUS lpwrCallbackConstruct(void)
    GCC_ATTRIB_SECTION("imem_init", "lpwrCallbackConstruct");
void        lpwrCallbackCtrlSchedule(LwU8 ctrlId);
void        lpwrCallbackCtrlDeschedule(LwU8 ctrlId);
LwU32       lpwrCallbackCtrlGetPeriodUs(LwU8 ctrlId);
void        lpwrCallbackNotifyMsFullPower(void);

// Lpwr group functions
FLCN_STATUS lpwrGrpPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrGrpPostInit");
LwBool      lpwrGrpIsCtrlSupported(LwU32 lpwrGrpId)
            GCC_ATTRIB_SECTION("imem_resident", "lpwrGrpIsCtrlSupported");
LwBool      lpwrGrpIsLpwrCtrlMember(LwU32 lpwrGrpId, LwU32 lpwrCtrlId);
void        lpwrGrpDisallowExt(LwU32 lpwrGrpId)
            GCC_ATTRIB_SECTION("imem_libLpwr", "lpwrGrpDisallowExt");
void        lpwrGrpAllowExt(LwU32 lpwrGrpId)
            GCC_ATTRIB_SECTION("imem_libLpwr", "lpwrGrpAllowExt");
LwU32       lpwrGrpCtrlMaskGet(LwU32 lpwrGrpId);
LwU32       lpwrGrpCtrlRmDisallowMaskGet(LwU32 lpwrGrpId);
LwBool      lpwrGrpCtrlIsSfSupported(LwU32 lpwrGrpId, LwU32 sfId);
LwBool      lpwrGrpCtrlIsEnaged(LwU32 lpwrGrpId)
            GCC_ATTRIB_SECTION("imem_resident", "lpwrGrpCtrlIsEnaged");
LwBool      lpwrGrpCtrlIsInTransition(LwU32 lpwrGrpId);
void        lpwrGrpCtrlMutualExclusion(LwU32 grpCtrlId, LwU32 clientId, LwU32 clientDisallowMask);
void        lpwrGrpCtrlSeqOwnerChange(LwU8 grpCtrlId);
void        lpwrGrpCtrlRmClientHandler(LwU8 grpCtrlId);

// Handle voltage specific functionality
void    lpwrProcessVfeDynamilwpdate(void)
    GCC_ATTRIB_SECTION("imem_perfVfe", "lpwrProcessVfeDynamilwpdate");

// LPWR OSM functions
void   lpwrOsmAckSend(void);

// Function to request EI notification enable/disable for LPWR based Clients
FLCN_STATUS lpwrRequestEiNotificationEnable(LwU8 clientId, LwBool bEnable);

// Function to initialize the priv blockers
FLCN_STATUS lpwrPrivBlockerInit(LwU32 blockerSupportMask)
    GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrPrivBlockerInit");

// Function to set the Wakeup CtrlId and Wakeup Reason mask
void        lpwrPrivBlockerWakeupMaskSet(LwU32 ctrlIdMask, LwU32 wakeupReason);

// Function to Clear the Wakeup CtrlId and Wakeup Reason mask
void        lpwrPrivBlockerWakeupMaskClear(LwU32 ctrlIdMask);

// Function to update the Mode of Priv Blocker
FLCN_STATUS lpwrPrivBlockerModeSet(LwU32 blockerId, LwU32 requestedMode,
                                   LwU32 requestedProfile);

// Function to update the blocker Mode by LPWR_CTRLS
FLCN_STATUS lpwrCtrlPrivBlockerModeSet(LwU32 ctrlId, LwU32 blockerMode,
                                       LwU32 profileId, LwU32 timeOutNs);

// To be called at init time as part of the PMU Engine construct sequence.
FLCN_STATUS pmuPreInitGc6Exit(void)
    GCC_ATTRIB_SECTION("imem_init", "pmuPreInitGc6Exit");

// LPWR Sequencer Functions

// Function to initialize the LPWR Sequencer State
FLCN_STATUS lpwrSeqInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrSeqInit");

// Function to initialize the SRAM Sequencer Ctrl
FLCN_STATUS lpwrSeqSramCtrlInit(LwU8 ctrlId)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrSeqSramCtrlInit");

FLCN_STATUS lpwrSeqSramStateSet(LwU8 ctrlId, LwU8 targetState, LwBool bBlocking);

// Function to register fo pstate notification
FLCN_STATUS lpwrLpPstateChangeEvtHandler(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA pstateNotificationData)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "lpwrLpPstateChangeEvtHandler");
FLCN_STATUS lpwrLpPstatePostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrLpPstatePostInit");

// LPWR Stat Functions

// Function to initialize the LPWR Stat Infrastructure
FLCN_STATUS lpwrStatInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrStatInit");

FLCN_STATUS lpwrStatCtrlInit(LwU8 ctrlId)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "lpwrStatCtrlInit");
FLCN_STATUS lpwrStatCtrlUpdate(LwU8 ctrlId, LwU8 featureState);
#endif // PMU_OBJLPWR_H
