/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DEEPIDLE_H
#define DEEPIDLE_H

/*!
 * @file pmu_didle.h
 */

#include "lwtypes.h"
#include "unit_api.h"
#include "pmu_objtimer.h"
#include "main.h"

#define DIDLE_SIGNAL_EVTID  (DISP2UNIT_EVT__COUNT + 0U)
enum
{
    DIDLE_SIGNAL_DEEP_L1_ENTRY         ,
    DIDLE_SIGNAL_DEEP_L1_EXIT          ,
    DIDLE_SIGNAL_ABORT                 ,
    DIDLE_SIGNAL_ELPG_ENTRY            ,
    DIDLE_SIGNAL_ELPG_EXIT             ,
    DIDLE_SIGNAL_MSCG_ENGAGED          ,
    DIDLE_SIGNAL_MSCG_ABORTED          ,
    DIDLE_SIGNAL_MSCG_DISENGAGED       ,
    DIDLE_SIGNAL_DIOS_ATTEMPT_TO_ENGAGE
};

typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    LwU8            signalType;
} DISPATCH_SIGNAL_DIDLE_GENERIC;

/*!
 * @brief Structure to send deepL1/L1.1/L1.2 entry/exit event
 */
typedef struct
{
    // Must be first element of the structure
    DISP2UNIT_HDR           hdr;

    // Signal Type in DeepIdle
    LwU8                    signalType;

    // PEX sleep state - DI_PEX_SLEEP_STATE_xyz
    LwU32                   pexSleepState;
} DISPATCH_SIGNAL_DIDLE_DEEP_L1;

/*!
 * @brief Dispatch structure for GCX based DeepIdle OR Legacy DeepIdle
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    DISPATCH_SIGNAL_DIDLE_GENERIC   sigGen;
    DISPATCH_SIGNAL_DIDLE_DEEP_L1   sigDeepL1;
} DISPATCH_DIDLE;

/*!
 * @brief Enumeration to represent each deepidle state
 *
 * The lwrrently active state is held in DidleLwrState
 * OS = Opportunistic Sleep, Enters whenever the latency constraints are satisfied
 *      usually when there are no heads.
 *
 * DIDLE_STATE_INACTIVE     The gpu is not in deepidle.  The PMU cannot enter
 *                          deepidle automatically.
 *
 * DIDLE_STATE_ARMED_OS     The PMU will enter/exit deepidle autonomously based
 *                          on PCI link idleness, indicated by Deep-L1
 *                          entry/exit signals.
 *
 * DIDLE_STATE_ENTERING_OS  The PMU is in the process of putting the GPU into
 *                          deepidle as a result of a Deep L1 entry signal.
 *
 * DIDLE_STATE_EXITING_OS   The PMU is in the process of pulling the GPU out of
 *                          deepidle NH as a result of a Deep L1 exit signal.
 *                          This is a transitional state only; commands cannot
 *                          be processed while in this state.
 *
 * DIDLE_STATE_IN_OS        The GPU is in deepidle NH as a result of a Deep L1
 *                          entry signal.
 */

/*
 * @brief extension of enumeration for SSC
 * Didle state Machine is most suitable for SSC implementation. So it is extended
 * to handle SSC as well.
 *
 * DIDLE_STATE_IN_SSC        In this state PMU is halted. At the end of SSC
 *                           entry sequence  in ENTERING_SSC state, after giving
 *                           control to SCI and just before halting the PMU
 *                           falcon(itself), it will change state to SSC
 */

enum DIDLE_STATE
{
    DIDLE_STATE_INACTIVE,
    DIDLE_STATE_ARMED_OS,
    DIDLE_STATE_ENTERING_OS,
    DIDLE_STATE_EXITING_OS,
    DIDLE_STATE_IN_OS,
    DIDLE_STATE_IN_SSC
};

/*!
 * @brief Enumeration to represent each deepidle substate during entry into and
 *        exit from DIDLE_STATE_ACTIVE
 *
 * The current entry/exit substate is held in DidleDidleSubstate
 *
 * GC4_SUBSTATE_START                   No actions have been taken to begin
 *                                      entry/exit of deepidle.  The entry
 *                                      states start from here and go forward.
 *
 * GC4_SUBSTATE_DMI_COUNTER             The DMI counter has been stopped if
 *                                      entering or restarted if exiting.
 *
 * GC4_SUBSTATE_ACPD                    ACPD has been disabled if entering
 *                                      or restored if exiting.
 *
 * GC4_SUBSTATE_ASR                     ASR has been disabled if entering
 *                                      or restored if exiting.
 *
 * GC4_SUBSTATE_SELF_REFRESH            DRAM self-refresh has been enabled
 *                                      if entering or disabled if exiting.
 *
 * GC4_SUBSTATE_CML_CLOCKS              The CML clocks have been disabled
 *                                      if entering or restored if exiting.
 *
 * GC4_SUBSTATE_ONESRC_CLOCKS           The one source clocks have been disabled
 *                                      if entering or restored if exiting.
 *
 * GC4_SUBSTATE_XTAL_CLOCKS             The clocks that must remain on have been
 *                                      switched to xtal or xtal4x if entering
 *                                      or restored to their original sources
 *                                      if exiting.
 *
 * GC4_SUBSTATE_SPPLLS                  The spplls have been disabled
 *                                      if entering or restored if exiting.
 *
 * GC4_SUBSTATE_VOLTAGE                 The voltage has been lowered
 *                                      if entering or restored if exiting.
 *
 * GC4_SUBSTATE_END                     Number of entry/exit substates.  The
 *                                      states for exiting start from here and
 *                                      go backward.
 */
enum GC4_SUBSTATE
{
    GC4_SUBSTATE_START,
    GC4_SUBSTATE_DMI_COUNTER,
    GC4_SUBSTATE_ACPD,
    GC4_SUBSTATE_ASR,
    GC4_SUBSTATE_SELF_REFRESH,
    GC4_SUBSTATE_CML_CLOCKS,
    GC4_SUBSTATE_ONESRC_CLOCKS,
    GC4_SUBSTATE_XTAL_CLOCKS,
    GC4_SUBSTATE_SPPLLS,
    GC4_SUBSTATE_VOLTAGE,
    GC4_SUBSTATE_PMU_STOP_CLOCKS_TRIGGER,
    GC4_SUBSTATE_END,
};

/*!
 * This is the lwrrently active deepidle state.  The possible values are defined
 * in the DIDLE_STATE enum.
 */
extern LwU8 DidleLwrState;

// Marco for checking if deep idle is initialized
#define DIDLE_IS_INITIALIZED(di)        \
    PMUCFG_FEATURE_ENABLED(PMUTASK_GCX)?(di.bInitialized):LW_FALSE

#endif // DEEPIDLE_H

