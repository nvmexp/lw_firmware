/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2015,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  perfsub_20.h
 * @brief APIs that maintain various performance settings
 *
 */

#pragma once
#ifndef INCLUDED_PERFSUB_20_H
#define INCLUDED_PERFSUB_20_H

#include "ctrl/ctrl2080.h"

#include "gpu/perf/perfsub.h"

class GpuSubdevice;
struct JSObject;
struct JSContext;

class Perf20 : public Perf
{
public:
    Perf20(GpuSubdevice* pSubdevice, bool owned);
    ~Perf20() override;

    RC SetVoltage(Gpu::VoltageDomain Domain, UINT32 VoltMv, UINT32 PState) override;

    //! for each available pstate print out the clock and voltage settings
    RC PrintAllPStateInfo() override;
    RC GetClockAtPStateNoCache(UINT32 PStateNum,
                               Gpu::ClkDomain Domain,
                               UINT64 *pFreqHz) override;

    RC GetPexSpeedAtPState(UINT32 PStateNum, UINT32 *pLinkSpeed) override;
    RC GetLinkWidthAtPState(UINT32 PStateNum, UINT32 *pLinkWidth) override;

    RC GetVdtToJs(JSContext *pCtx, JSObject *pJsObj) override;

    // PStates 2.0 only - offset the pstate minimum LWVDD required by mclk.
    // Actual LWVDD is max(pstate, VDT) + arbiter_offset.
    // Possibly obsolete, given SetClientOffsetMv.
    RC SetPStateOffsetMv(UINT32 PStateNum, INT32 OffsetMv) override;

    // PStates 2.0 only - offset the VDT tuning entry for callwlating the
    // required LWVDD of the current gpc2clk.
    // Requires vbios setup so that the VDT chains to a "tuning entry".
    // Actual LWVDD is max(pstate, VDT) + arbiter_offset.
    // Possibly obsolete, given SetClientOffsetMv.
    RC SetVdtTuningOffsetMv(INT32 OffsetMv) override;

    //! Call super and restore the original "tuning" VDT table entry
    RC ClearVoltTuningOffsets() override;

    RC ClearForcedPState() override;

protected:
    LW2080_CTRL_PMGR_VOLT_VDT_GET_INFO_PARAMS    m_VdtInfo;
    vector<LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_INFO> m_VdtTable;

    //! Call LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO to obtain the current
    //! voltage for the given domain
    RC GetVoltageAtPStateNoCache(UINT32 PStateNum,
                                 Gpu::VoltageDomain Domain,
                                 UINT32 *pVoltageUv) override;
    //! Call LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO and use the results to
    //! update the corresponding dynamic info in pstatesInfo
    RC GetLwrrentPerfValuesNoCache(PStateInfos *pPstatesInfo) override;

private:
    PStates2Data m_PStates2Data;
    bool  m_VdtTuningEntryModified;
    bool  m_PStateVoltageModified;
    RC CachePStates2Data();
    const PStates2Data & GetCachedPStates2Data() const
    {
        return m_PStates2Data;
    }
    RC PrintPStates2Data(Tee::Priority pri, const PStates2Data & p2d);

    RC ApplyFreqTuneOffset(INT32 FreqOffsetKhz, UINT32 PStateNum) override;

    RC ParseVfTable() override;
    void ParseVfEntryTbl
    (
        const LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO & tableInfo,
        const vector<LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO> & entries,
        Tee::Priority pri,
        VfEntryTbl * pVfEntryTbl
    );

    RC ParseVdtTable();

    RC GetVirtualPerfPoints(PerfPoints *points) override;

    //! Brief: What's retrieved in init
    // 1) Number of PStates - each of the PState bits
    // 2) The supported clock and voltage domains
    // 3) PState 2.0 related info:
    //    * the type of clock (fixed/pstate/ratio)
    //    * the master domain (if it is ratio)
    //    * Min/Max (if it is pstate/ratio)
    RC Init() override;
    RC GenerateStandardPerfPoints
    (
        UINT64 fastestSafeGpc2Hz,
        const PStateInfos &pstateInfo,
        PerfPoints *pPoints
    ) override;

    //! Lock to either the intersect point, or to the nominal pstate frequencies
    //! Intersect point uses limits SET_STATUS RMCTRL to achieve its point,
    //! Non-intersect points use the FORCE_PSTATE_EX RMCTRL.
    RC InnerSetPStateLock
    (
        const PerfPoint &Setting,
        PStateLockType Lock
    ) override;

    //! Next 4 functions set up the call to ForcePStateInternal
    RC InnerSuggestPState(UINT32 TargetState, UINT32 FallBack);
    RC InnerFastSuggestPState(UINT32 TargetState, UINT32 FallBack);
    RC InnerForcePState(UINT32 Target, UINT32 FallBack);
    RC InnerFastForcePState(UINT32 Target, UINT32 FallBack);

    //! Sets up call to ForceRmPStateLock and handles errors
    RC ForcePStateInternal
    (
        UINT32 pstateNum,
        UINT32 fallback,
        PStateSwitchType switchType,
        PStateLockType newLockType
    );

    //! Use the FORCE_PSTATE_EX RMCTRL to lock to the pstate
    //! Parameters determine how switch oclwrs (fast/normal, hard/soft, fallback)
    RC DoRmPStateLock
    (
        UINT32 pstateNum,
        UINT32 fallback,
        PStateSwitchType switchType,
        PStateLockType lockType
    );

    //! Undo DoRmPStateLock by clearing the previously forced pstate
    RC ClearForcePStateEx(UINT32 Flags) override;
};

#endif
