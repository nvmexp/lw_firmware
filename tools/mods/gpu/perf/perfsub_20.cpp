/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ctrl/ctrl2080.h"

#include "gpu/perf/perfsub_20.h"

#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "Lwcm.h"
#include "core/include/display.h"
#include "core/include/lwrm.h"
#include "core/include/pci.h"
#include "core/include/utility.h"
#include "gpu/perf/perfutil.h"

#include <algorithm>

// Because JS_NewObject with a NULL class is unsafe in JS 1.4.
JS_CLASS(PerfDummy);

Perf20::Perf20(GpuSubdevice *pSubdevice, bool owned) :
    Perf(pSubdevice, owned)
   ,m_VdtTuningEntryModified(false)
   ,m_PStateVoltageModified(false)
{
    memset(&m_VdtInfo, 0, sizeof(LW2080_CTRL_PMGR_VOLT_VDT_GET_INFO_PARAMS));
}

Perf20::~Perf20()
{
}

//-----------------------------------------------------------------------------
RC Perf20::SetVoltage(Gpu::VoltageDomain Domain, UINT32 VoltMv, UINT32 PStateNum)
{
    if (!CanSetVoltDomailwiaPState(Domain))
    {
        Printf(Tee::PriNormal, "Voltage domain lwrrently not supported\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    const LW2080_CTRL_PERF_VOLT_DOM_INFO ctor = {0};
    vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> Voltage(1, ctor);

    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> Dummy;
    Voltage[0].domain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Domain);
    Voltage[0].type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL;
    Voltage[0].data.logical.logicalVoltageuV = VoltMv * 1000;

    CHECK_RC(SetPStateTableInternal(PStateNum, Dummy, Voltage));
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf20::Init()
{
    if (m_InitDone)
        return OK;

    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);

    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS StatesInfo = {0};
    rc = pLwRm->Control(pRmHandle,
                        LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                        &StatesInfo,
                        sizeof(StatesInfo));
    if (RC::LWRM_NOT_SUPPORTED == rc)
    {
        Printf(Tee::PriLow, "This GPU does not support p-states\n");
        return OK;
    }
    CHECK_RC(rc);

    Printf(Tee::PriLow,
           "PStates Mask = 0x%x\n"
           "VPStates Mask = 0x%x, Num = %d\n"
           "Supported Clock Domains = 0x%x\n"
           "Supported Voltage Domains = 0x%x\n",
           (unsigned)StatesInfo.pstates,
           (unsigned)StatesInfo.virtualPstates,
           (int)     StatesInfo.virtualPstatesNum,
           (unsigned)StatesInfo.perfClkDomains,
           (unsigned)StatesInfo.perfVoltageDomains);

    m_ClkDomainsMask     = StatesInfo.perfClkDomains;
    m_VoltageDomainsMask = StatesInfo.perfVoltageDomains;

    // Explicitly clear deprecated domains from m_VoltageDomainsMask
    m_VoltageDomainsMask &= ~(LW2080_CTRL_PERF_VOLTAGE_DOMAINS_FB |
                              LW2080_CTRL_PERF_VOLTAGE_DOMAINS_COLD_CORE |
                              LW2080_CTRL_PERF_VOLTAGE_DOMAINS_CORE_NOMINAL);

    UINT32 NumClkDomains = Utility::CountBits(m_ClkDomainsMask);

    LW2080_CTRL_CLK_GET_PSTATES2_INFO_V2_PARAMS PState2ClkParams = { 0 };
    if (m_ClkDomainsMask)
    {
        PState2ClkParams.clkInfoListSize = NumClkDomains;

        // setup the Clock / voltage domain in each list
        UINT32 ClkListIdx = 0;
        for (UINT32 BitNum = 0; BitNum < 32; BitNum++)
        {
            UINT32 BitMask = 1<<BitNum;
            if (m_ClkDomainsMask & BitMask)
            {
                PState2ClkParams.clkInfoList[ClkListIdx].clkDomain = BitMask;
                ClkListIdx++;
            }
        }

        // Query PState 2.0 clock domain info
        CHECK_RC(pLwRm->Control(pRmHandle,
                                LW2080_CTRL_CMD_CLK_GET_PSTATES2_INFO_V2,
                                &PState2ClkParams,
                                sizeof(PState2ClkParams)));
    }

    for (UINT32 i = 0; i < NumClkDomains; i++)
    {
        Gpu::ClkDomain Domain = PerfUtil::ClkCtrl2080BitToGpuClkDomain(PState2ClkParams.clkInfoList[i].clkDomain);
        Gpu::ClkDomain RatioDomain = PerfUtil::ClkCtrl2080BitToGpuClkDomain(PState2ClkParams.clkInfoList[i].ratioDomain);
        ClkDomainInfo NewClockDomain;
        NewClockDomain.Domain      = Domain;
        NewClockDomain.RatioDomain = RatioDomain;
        NewClockDomain.Ratio       = PState2ClkParams.clkInfoList[i].ratio;
        NewClockDomain.objectIndex = i;
        NewClockDomain.PStateFloor = FLD_TEST_DRF(2080,
                                                  _CTRL_CLK,
                                                  _PSTATES2_INFO_FLAGS_PSTATEFLOOR,
                                                  _TRUE,
                                                  PState2ClkParams.clkInfoList[i].flags);

        UINT32 ClkType = DRF_VAL(2080, _CTRL_CLK,
                                 _PSTATES2_INFO_FLAGS_USAGE, PState2ClkParams.clkInfoList[i].flags);
        switch (ClkType)
        {
            case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_FIXED:
                NewClockDomain.Type = FIXED;
                break;
            case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_PSTATE:
                NewClockDomain.Type = PSTATE;
                break;
            case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_DECOUPLED:
                NewClockDomain.Type = DECOUPLED;
                // maybe there are other better indications of PState 2.0
                m_PStateVersion = PStateVersion::PSTATE_20;
                break;
            case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_RATIO:
                NewClockDomain.Type = RATIO;
                // maybe there are other better indications of PState 2.0
                m_PStateVersion = PStateVersion::PSTATE_20;
                break;
            default:
                NewClockDomain.Type = UNKNOWN_TYPE;
                break;
        }
        NewClockDomain.IsRmClkDomain = true;
        m_ClockDomains[Domain] = NewClockDomain;

        // For any 1x/2x Clock domains, create a corresponding 2x/1x clock domain
        if (PerfUtil::Is1xClockDomain(Domain) || PerfUtil::Is2xClockDomain(Domain))
        {
            NewClockDomain.Domain = PerfUtil::ColwertToOppositeClockDomain(Domain);
            // This is not the original RM domain
            NewClockDomain.IsRmClkDomain = false;
            m_ClockDomains[NewClockDomain.Domain] = NewClockDomain;
        }
    }

    if (0 == StatesInfo.pstates || !m_pSubdevice->HasFeature(Device::GPUSUB_CAN_HAVE_PSTATE20))
    {
        m_InitDone = true;
        m_PStateVersion = PStateVersion::PSTATE_10;
        return OK;
    }

    m_VPStatesMask       = StatesInfo.virtualPstates;
    m_VPStatesNum        = StatesInfo.virtualPstatesNum;

    for (UINT32 i = 0; i < 32; i++)
    {
        if (StatesInfo.pstates & (1<<i))
        {
            PStateInfo NewPState;
            NewPState.PStateBit = PStateNumTo2080Bit(i);
            NewPState.PStateNum = i;
            NewPState.FreqOffsetKHz = 0;
            m_CachedPStates.push_back(NewPState);
        }
    }

    CHECK_RC(GetLwrrentPerfValuesNoCache(&m_CachedPStates));

    // Initializing with no clock domains at intersect unless the
    // user requests so via -XXXclk intersect, (OR) XX.intersect
    for (PStateInfos::iterator pstateIt = m_CachedPStates.begin();
         pstateIt != m_CachedPStates.end();
         ++pstateIt)
    {
        for (map<UINT32, ClkRange>::iterator clkIt = pstateIt->ClkVals.begin();
             clkIt != pstateIt->ClkVals.end();
             ++clkIt)
        {
            pstateIt->IsClkIntersect[clkIt->first] = false;
        }
    }

    CHECK_RC(ParseVfTable());
    CHECK_RC(ParseVdtTable());
    (void) DumpVPStateInfo();
    CHECK_RC(CachePStates2Data());

    m_InitDone = true;
    return rc;
}

// Apply a frequency offset to the Gpc and ratio related clocks.
// The net effect is to shift the VF Lwrve in the x direction
//
// V |               a    b
// o |              /    /
// l |             /    /
// t |            /    /
// g |           /    /
// a |a&b_______/____/
// g |
// e |
//   --------------------------------------------
//   F r e q u e n c y
// Lwrve a is the lwrve without any offset
// Lwrve b has OffsetKHz offset applied
RC Perf20::ApplyFreqTuneOffset(INT32 OffsetKHz, UINT32 PStateNum)
{
    RC rc;
    LW2080_CTRL_PERF_PSTATE20_CLK_DOM_INFO Clk = {0};
    Clk.domain = PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkGpc2);
    Clk.freqDeltakHz.value = OffsetKHz;

    UINT32 psIdx;
    GetPStateIdx(PStateNum,&psIdx);

    LW2080_CTRL_PERF_SET_PSTATES20_DATA_PARAMS Params = {0};
    Params.numPstates  = 1;
    Params.numClocks   = 1;
    Params.numVoltages = 0;
    Params.pstate[0].pstateID = m_CachedPStates[psIdx].PStateBit;
    Params.pstate[0].flags = 0;
    Params.pstate[0].perfClkDomInfoList = LW_PTR_TO_LwP64(&Clk);

    Printf(Tee::PriDebug,"Setting PState20 Data: pstate:%u domain:0x%x freqDeltakHz:%d\n",
        Params.pstate[0].pstateID, Clk.domain, Clk.freqDeltakHz.value);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
        LW2080_CTRL_CMD_PERF_SET_PSTATES20_DATA,
        &Params,
        sizeof(Params)));

    return rc;
}

//-----------------------------------------------------------------------------
// VF table is 2 sets of tables. The first table is VF Index table, and the
// second table is VF entry table.
//
// VF Index table tells us, given PState, the entry idex of the VF entry table.
//  - The Indexes map from: {pstate, domain} to a range of VF Entries
// Each VF Entry gives a range of clocks and also some PLL flags.
//  - MODS builds the list of VFPoints (points that real driver would set) from
//    this VF table. If an VF Entry has a step size > 0,
//  Note: all the VF points in one VF Entry have the same voltage (VDT entry)
//  Lwrrently we don't have information on VDT table at the moment.
RC Perf20::ParseVfTable()
{
    RC rc;
    if (m_PStateVersion < PStateVersion::PSTATE_20)
    {
        return OK;
    }
    // retrieve information about the VF entries
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_PERF_VF_TABLES_GET_INFO_PARAMS VfTbl = {0};
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PERF_VF_TABLES_GET_INFO,
                            &VfTbl,
                            sizeof(VfTbl)));

    const Tee::Priority pri = m_DumpVfTable ? Tee::PriNormal : Tee::PriSecret;

    Printf(pri, "num VF Index = %u, num VF entries = %u\n",
           VfTbl.numIndexes,
           VfTbl.numEntries);

    // actually getting the VF entries
    typedef vector<LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO> VfIndexTableInfos;
    const LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO ctorVfi = {0};
    VfIndexTableInfos VfIndexes(VfTbl.numIndexes, ctorVfi);
    for (UINT32 i = 0; i < VfIndexes.size(); i++)
    {
        VfIndexes[i].index = i;
    }
    const LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO ctorVfe = {0};
    vector<LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO> VfEntries(VfTbl.numEntries, ctorVfe);
    for (UINT32 i = 0; i < VfEntries.size(); i++)
    {
        VfEntries[i].index = i;
        VfEntries[i].voltageInfo.type = LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VDT;
        VfEntries[i].voltageInfo.domain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Gpu::CoreV);
    }
    LW2080_CTRL_PERF_VF_TABLES_ENTRIES_INFO_PARAMS VfTblEntryParams = {0};
    VfTblEntryParams.vfIndexesInfoListSize = VfTbl.numIndexes;
    VfTblEntryParams.vfIndexesInfoList = LW_PTR_TO_LwP64(&VfIndexes[0]);
    VfTblEntryParams.vfEntriesInfoListSize = VfTbl.numEntries;
    VfTblEntryParams.vfEntriesInfoList = LW_PTR_TO_LwP64(&VfEntries[0]);
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PERF_VF_TABLES_ENTRIES_GET_INFO,
                            &VfTblEntryParams,
                            sizeof(VfTblEntryParams)));

    // Parse each VF_ENTRIES_TABLE_INFO into a mods VfEntryTbl object.

    // Check for no-display VBIOS
    UINT32 connectors = 0;
    CHECK_RC(m_pSubdevice->GetParentDevice()->GetDisplay()->GetConnectors(&connectors));

    m_VfEntryTbls.clear();
    for (size_t ii = 0; ii < VfIndexes.size(); ++ii)
    {
        VfEntryTbl vfe;
        Gpu::ClkDomain domain =
            PerfUtil::ClkCtrl2080BitToGpuClkDomain(VfIndexes[ii].domain);
        // Don't add ClkDisp domains on no-display VBIOS
        if (domain == Gpu::ClkDisp && connectors == 0)
        {
            continue;
        }

        ParseVfEntryTbl(VfIndexes[ii],
                        VfEntries,
                        pri,
                        &vfe);
        if (vfe.Domain != Gpu::ClkUnkUnsp)
            m_VfEntryTbls.push_back(vfe);
    }
    return rc;
}

//------------------------------------------------------------------------------
void Perf20::ParseVfEntryTbl
(
    const LW2080_CTRL_PERF_VF_INDEXES_TABLE_INFO & tableInfo,
    const vector<LW2080_CTRL_PERF_VF_ENTRIES_TABLE_INFO> & entries,
    Tee::Priority pri,
    VfEntryTbl * pVfEntryTbl
)
{
    pVfEntryTbl->Domain =
        PerfUtil::ClkCtrl2080BitToGpuClkDomain(tableInfo.domain);
    pVfEntryTbl->PStateMask = tableInfo.pstate;

    Printf(pri, "Parsing VfEntryTable: pstates 0x%x domain %s\n",
           pVfEntryTbl->PStateMask,
           PerfUtil::ClkDomainToStr(pVfEntryTbl->Domain));

    if (pVfEntryTbl->Domain == Gpu::ClkUnkUnsp)
        return;

    // Find the min clock frequency for this domain in any of its pstates.
    UINT32 floorKHz = 0xffffffff;
    for (PStateInfos::iterator pps = m_CachedPStates.begin();
         pps != m_CachedPStates.end();
         ++pps)
    {
        const UINT32 pstateMinKHz = pps->ClkVals[tableInfo.domain].MinKHz;
        if ((pVfEntryTbl->PStateMask & (1 << pps->PStateNum)) &&
            (pstateMinKHz < floorKHz))
        {
            floorKHz = pstateMinKHz;
        }
    }

    // go from the last table of each Index Table and traverse down
    // algorighm: start from the last index (highest freq). At each index,
    // create VF points at each step size until reaching the maxFreqKHz
    // of the next (lower) index.
    // At the very first Index, traverse go until we hit 0)
    for (int ii = tableInfo.entryIndexLast;
         ii >= tableInfo.entryIndexFirst;
         ii--)
    {
        if (0 == entries[ii].freqStepSizeKHz)
        {
            VfPoint NewPt = {0};
            NewPt.ClkFreqKHz = entries[ii].maxFreqKHz;
            NewPt.Flags      = entries[ii].flags;
            NewPt.VdtIdx     = entries[ii].voltageInfo.data.vdt.vdtIndex;
            NewPt.VfIdx      = ii;
            NewPt.IsDerived  = false;
            NewPt.StepSizeKHz = entries[ii].freqStepSizeKHz;
            pVfEntryTbl->Points.push_back(NewPt);
            Printf(pri,
                   " %dKHz, VDT idx=%d, ErrorVF=%d, VF=%d, Flags=0x%x, StepSizeKHz=%d\n",
                   NewPt.ClkFreqKHz,
                   NewPt.VdtIdx,
                   NewPt.VfIdx + 1, // 0 is reserved for no VF point
                   NewPt.VfIdx,
                   NewPt.Flags,
                   NewPt.StepSizeKHz);
            continue;
        }

        const bool FirstEntry = (ii == tableInfo.entryIndexFirst);
        UINT32 FloorFreq = FirstEntry ? floorKHz : entries[ii-1].maxFreqKHz;
        Printf(pri, "FloorFreq = %d\n", FloorFreq);

        const UINT32 freqStepKHz = entries[ii].freqStepSizeKHz;

        if (1 == freqStepKHz)
        {
            // for Fermi+ chips, One Source clocks.
            // see bug 900315: annoying that RM refuses to parse this for us
            UINT32 Divider         = 1;
            const UINT32 RefClkKhz = 1620000;
            UINT32 FreqKHz         = 1620000;
            // step 1: find the starting point of the division
            while (FreqKHz > entries[ii].maxFreqKHz)
            {
                FreqKHz = RefClkKhz / Divider;
                Divider++;
            }

            Printf(pri,
                   "FreqKhz = %d, Divider = %d, FloorFreq = %d\n",
                   FreqKHz, Divider, FloorFreq);

            // step 2: add point that are integer divides of 1620 until
            // we hit the floor frequency
            while (FreqKHz && FreqKHz >= FloorFreq)
            {
                VfPoint NewPt = {0};
                NewPt.ClkFreqKHz = FreqKHz;
                NewPt.Flags      = entries[ii].flags;
                NewPt.VdtIdx     = entries[ii].voltageInfo.data.vdt.vdtIndex;
                NewPt.VfIdx      = ii;
                NewPt.IsDerived  = (FreqKHz != entries[ii].maxFreqKHz);
                NewPt.StepSizeKHz = freqStepKHz;
                pVfEntryTbl->Points.push_back(NewPt);
                Printf(pri,
                       "Osc %dKHz, VDT idx=%d, ErrorVf=%d, VF=%d, Flags=0x%x\n",
                       NewPt.ClkFreqKHz,
                       NewPt.VdtIdx,
                       NewPt.VfIdx + 1, // 0 is reserved for no VF point
                       NewPt.VfIdx,
                       NewPt.Flags);

                FreqKHz = RefClkKhz / Divider;
                Divider++;
            }
        }
        else
        {
            for (int Freq = entries[ii].maxFreqKHz;
                 Freq >= static_cast<int>(FloorFreq) &&
                     Freq >= static_cast<int>(freqStepKHz);
                 Freq -= freqStepKHz)
            {
                VfPoint NewPt = {0};
                NewPt.ClkFreqKHz = Freq;
                NewPt.Flags      = entries[ii].flags;
                NewPt.VdtIdx     = entries[ii].voltageInfo.data.vdt.vdtIndex;
                NewPt.VfIdx      = ii;
                NewPt.IsDerived  = ((UINT32)Freq != entries[ii].maxFreqKHz);
                NewPt.StepSizeKHz = freqStepKHz;
                pVfEntryTbl->Points.push_back(NewPt);
                Printf(pri,
                       "step %dKHz, VDT idx=%d, ErrorVF=%d, VF=%d, Flags=0x%x\n",
                       NewPt.ClkFreqKHz,
                       NewPt.VdtIdx,
                       NewPt.VfIdx + 1, // 0 is reserved for no VF point
                       NewPt.VfIdx,
                       NewPt.Flags);
            }
        }
    }
}

//-----------------------------------------------------------------------------
RC Perf20::ParseVdtTable()
{
    if (m_PStateVersion < PStateVersion::PSTATE_20)
        return OK;

    RC rc;
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PMGR_VOLT_VDT_GET_INFO,
                            &m_VdtInfo,
                            sizeof(m_VdtInfo)));

    m_VdtTable.clear();
    m_VdtTable.resize(m_VdtInfo.numEntries);
    for (size_t i = 0; i < m_VdtTable.size(); i++)
    {
        m_VdtTable[i].index = (LwU8)i;
    }
    LW2080_CTRL_PMGR_VOLT_VDT_ENTRIES_INFO_PARAMS TblParams = {0};
    TblParams.vdtEntryInfoListSize = static_cast<UINT32>(m_VdtTable.size());
    TblParams.vdtEntryInfoList = LW_PTR_TO_LwP64(&m_VdtTable[0]);
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PMGR_VOLT_VDT_ENTRIES_GET_INFO,
                            &TblParams,
                            sizeof(TblParams)));
    return rc;
}
//-----------------------------------------------------------------------------
RC Perf20::GetVdtToJs(JSContext *pCtx, JSObject *pJsObj)
{
    RC rc;
    CHECK_RC(GetPStatesInfo());

    JavaScriptPtr pJs;
    CHECK_RC(pJs->SetProperty(pJsObj, "hwSpeedo",
                              (UINT32)m_VdtInfo.hwSpeedo));
    CHECK_RC(pJs->SetProperty(pJsObj, "hwSpeedoVersion",
                              (UINT32)m_VdtInfo.hwSpeedoVersion));
    CHECK_RC(pJs->SetProperty(pJsObj, "speedoVersion",
                              (UINT32)m_VdtInfo.speedoVersion));
    CHECK_RC(pJs->SetProperty(pJsObj, "tempPollingPeriodms",
                              m_VdtInfo.tempPollingPeriodms));
    CHECK_RC(pJs->SetProperty(pJsObj, "nominalP0VdtEntry",
                              m_VdtInfo.nominalP0VdtEntry));
    CHECK_RC(pJs->SetProperty(pJsObj, "reliabilityLimitEntry",
                              m_VdtInfo.reliabilityLimitEntry));
    CHECK_RC(pJs->SetProperty(pJsObj, "overVoltageLimitEntry",
                              m_VdtInfo.overVoltageLimitEntry));
    CHECK_RC(pJs->SetProperty(pJsObj, "voltageTuningEntry",
                              m_VdtInfo.voltageTuningEntry));

    JSObject *pVdtEntries = JS_NewArrayObject(pCtx, 0, NULL);
    for (UINT32 i = 0; i < m_VdtTable.size(); i++)
    {
        JSObject *pVdt = JS_NewObject(pCtx, &PerfDummyClass, NULL, NULL);
        CHECK_RC(pJs->SetProperty(pVdt, "lwrrTargetVoltageuV",
                                  (UINT32)m_VdtTable[i].lwrrTargetVoltageuV));
        CHECK_RC(pJs->SetProperty(pVdt, "localUnboundVoltageuV",
                                  m_VdtTable[i].localUnboundVoltageuV));
        CHECK_RC(pJs->SetProperty(pVdt, "type",
                                  m_VdtTable[i].type));
        CHECK_RC(pJs->SetProperty(pVdt, "nextEntry",
                                  m_VdtTable[i].nextEntry));
        CHECK_RC(pJs->SetProperty(pVdt, "voltageMinuV",
                                  m_VdtTable[i].voltageMinuV));
        CHECK_RC(pJs->SetProperty(pVdt, "voltageMaxuV",
                                  m_VdtTable[i].voltageMaxuV));
        if (m_VdtTable[i].type == LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_TYPE_CVB10)
        {
            // RM LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_DATA_CVB10 coefficients
            // they are stored as 64 bit integer, but they really are 32 bit
            // signed integers
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient0",
                                      (INT32)m_VdtTable[i].data.cvb10.coefficient0));
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient1",
                                      (INT32)m_VdtTable[i].data.cvb10.coefficient1));
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient2",
                                      (INT32)m_VdtTable[i].data.cvb10.coefficient2));
        }
        else if (m_VdtTable[i].type == LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_TYPE_CVB20)
        {
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient0",
                                      m_VdtTable[i].data.cvb20.coefficient0));
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient1",
                                      m_VdtTable[i].data.cvb20.coefficient1));
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient2",
                                      m_VdtTable[i].data.cvb20.coefficient2));
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient3",
                                      m_VdtTable[i].data.cvb20.coefficient3));
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient4",
                                      m_VdtTable[i].data.cvb20.coefficient4));
            CHECK_RC(pJs->SetProperty(pVdt, "coefficient5",
                                      m_VdtTable[i].data.cvb20.coefficient5));
        }
        jsval VdtJsval;
        CHECK_RC(pJs->ToJsval(pVdt, &VdtJsval));
        CHECK_RC(pJs->SetElement(pVdtEntries, i, VdtJsval));
    }
    jsval VdtArrayJsval;
    CHECK_RC(pJs->ToJsval(pVdtEntries, &VdtArrayJsval));
    CHECK_RC(pJs->SetPropertyJsval(pJsObj, "VdtEntries", VdtArrayJsval));
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf20::SetPStateOffsetMv(UINT32 PStateNum, INT32 OffsetMv)
{
    RC rc;
    PStates2Data p2d = GetCachedPStates2Data();
    const UINT32 rmDomain = LW2080_CTRL_PERF_VOLTAGE_DOMAINS_CORE;
    const UINT32 numPStates = p2d.Params.numPstates;

    UINT32 psIdx;
    for (psIdx = 0; psIdx < numPStates; psIdx++)
    {
        if (p2d.Params.pstate[psIdx].pstateID == (1U<<PStateNum))
            break;
    }
    if (psIdx >= numPStates)
    {
        Printf(Tee::PriError, "PState %d not supported!\n", PStateNum);
        return RC::UNSUPPORTED_FUNCTION;
    }

    vector<LW2080_CTRL_PERF_PSTATE20_VOLT_DOM_INFO>::iterator i;
    for (i = p2d.Volts[psIdx].begin(); i != p2d.Volts[psIdx].end(); /**/)
    {
        if ((*i).domain == rmDomain)
        {
            // Modify and retain this item.
            (*i).voltageDeltauV.value = OffsetMv * 1000;
            ++i;
        }
        else
        {
            // Delete this item.
            i = p2d.Volts[psIdx].erase(i);
        }
    }

    LW2080_CTRL_PERF_SET_PSTATES20_DATA_PARAMS params = {0};

    // Set a "mods only" flag to disable a bunch of voltage-limit sanity
    // checks in the RM.  We want to be allowed to set P8 vmin above
    // P0's vmin for example.  See bug 1330815 for more details.
    params.flags = DRF_DEF(2080_CTRL_PERF_SET_PSTATES20_DATA_PARAMS,
                           _FLAGS, _MODE, _INTERNAL_TEST);
    params.numClocks = 0;
    params.numPstates  = 1;
    params.numVoltages = static_cast<LwU32>(p2d.Volts[psIdx].size());
    params.pstate[0].pstateID = p2d.Params.pstate[psIdx].pstateID;
    params.pstate[0].perfVoltDomInfoList =
            LW_PTR_TO_LwP64(&(p2d.Volts[psIdx][0]));

    CHECK_RC(LwRmPtr()->ControlBySubdevice(m_pSubdevice,
        LW2080_CTRL_CMD_PERF_SET_PSTATES20_DATA,
        &params,
        sizeof(params)));
    m_PStateVoltageModified = true;

    return rc;
}

//-----------------------------------------------------------------------------
RC Perf20::SetVdtTuningOffsetMv(INT32 offsetMv)
{
    RC rc;
    CHECK_RC(GetPStatesInfo());
    if (m_PStateVersion < PStateVersion::PSTATE_20)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // In addition, only support this if the VDT entry is actually in VBIOS
    if (LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_INDEX_ILWALID ==
        m_VdtInfo.voltageTuningEntry)
    {
        Printf(Tee::PriLow, "VBIOS does not have voltage tuning entry!\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    const INT32 offsetuV = offsetMv * 1000;

    if ((m_VdtTable[m_VdtInfo.voltageTuningEntry].voltageMinuV == offsetuV)&&
        (m_VdtTable[m_VdtInfo.voltageTuningEntry].voltageMaxuV == offsetuV))
    {
        return OK;
    }

    m_VdtTable[m_VdtInfo.voltageTuningEntry].voltageMinuV = offsetuV;
    m_VdtTable[m_VdtInfo.voltageTuningEntry].voltageMaxuV = offsetuV;

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    LW2080_CTRL_PMGR_VOLT_VDT_ENTRIES_INFO_PARAMS TblParams = {0};
    TblParams.vdtEntryInfoListSize = (LwU32) m_VdtTable.size();
    TblParams.vdtEntryInfoList = LW_PTR_TO_LwP64(&m_VdtTable[0]);
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PMGR_VOLT_VDT_ENTRIES_SET_INFO,
                            &TblParams,
                            sizeof(TblParams)));

    m_VdtTuningEntryModified = true;

    // refresh the VDT entries - lwrrentVoltage can be updated.
    CHECK_RC(ParseVdtTable());
    return rc;
}

//------------------------------------------------------------------------------
RC Perf20::CachePStates2Data()
{
    if (m_PStates2Data.Initialized)
        return OK;

    return GetPStates2Data(&m_PStates2Data);
}

//-----------------------------------------------------------------------------
RC Perf20::PrintPStates2Data(Tee::Priority pri, const Perf::PStates2Data & p2d)
{
    const LwU32 numPstates  = p2d.Params.numPstates;
    const LwU32 numClocks   = p2d.Params.numClocks;
    const LwU32 numVoltages = p2d.Params.numVoltages;

    Printf(pri, "\nPState20Data Table:\n"
                "flags:0x%x numPstates:%d numClocks:%d numVoltages:%d\n",
           p2d.Params.flags,
           numPstates,
           numClocks,
           numVoltages);

    for (LwU32 psIdx=0; psIdx < numPstates; psIdx++)
    {
        Printf(pri,
                "PState %d Info flags:0x%x\n",
                p2d.Params.pstate[psIdx].pstateID,
               p2d.Params.pstate[psIdx].flags);

        Printf(pri,"Clk Domains:\n");
        for (LwU32 clkIdx = 0; clkIdx < numClocks; clkIdx++)
        {
            const LW2080_CTRL_PERF_PSTATE20_CLK_DOM_INFO & clk =
                p2d.Clocks[psIdx][clkIdx];

            Printf(pri,
                "Domain:0x%x flags:0x%x freqDeltakHz(val:%d min:%d max:%d) type:%d(",
                clk.domain,
                clk.flags,
                clk.freqDeltakHz.value,
                clk.freqDeltakHz.valueRange.min,
                clk.freqDeltakHz.valueRange.max,
                clk.type);
            switch(clk.type)
            {
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_FIXED:
                    Printf(pri,
                        "Fixed) freqkHz:%d\n",
                        clk.data.fixed.freqkHz);
                    break;
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_PSTATE:
                    Printf(pri,
                        "Pstate) freqkHz:%d\n",
                        clk.data.pstate.freqkHz);
                    break;
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_DECOUPLED:
                    Printf(pri,
                        "decoupled) minFreqkHz:%d maxFreqkHz:%d voltageDomain:0x%x"
                        " minFreqVoltageuV:%d maxFreqVoltageuV:%d \n",
                        clk.data.decoupled.minFreqkHz,
                        clk.data.decoupled.maxFreqkHz,
                        clk.data.decoupled.voltageDomain,
                        clk.data.decoupled.minFreqVoltageuV,
                        clk.data.decoupled.maxFreqVoltageuV );
                    break;
                case LW2080_CTRL_CLK_PSTATES2_INFO_FLAGS_USAGE_RATIO:
                    Printf(pri,
                        "ratio) minFreqkHz:%d maxFreqkHz:%d voltageDomain:0x%x"
                        " minFreqVoltageuV:%d maxFreqVoltageuV:%d \n",
                        clk.data.ratio.minFreqkHz,
                        clk.data.ratio.maxFreqkHz,
                        clk.data.ratio.voltageDomain,
                        clk.data.ratio.minFreqVoltageuV,
                        clk.data.ratio.maxFreqVoltageuV );
                    break;
                default:
                    Printf(pri,"type:unknown\n");
                    break;
            }
        }

        Printf(pri,"Voltage Domains:\n");
        for (LwU32 vltIdx = 0; vltIdx < numVoltages; vltIdx++)
        {
            const LW2080_CTRL_PERF_PSTATE20_VOLT_DOM_INFO & volt =
                p2d.Volts[psIdx][vltIdx];

            Printf(pri,
                "Domain:0x%x flags:0x%x voltageDeltauV(val:%d min:%d max:%d)"
                " lwrrTragetVoltageuV:%d type:%d(",
                volt.domain,
                volt.flags,
                volt.voltageDeltauV.value,
                volt.voltageDeltauV.valueRange.min,
                volt.voltageDeltauV.valueRange.max,
                volt.lwrrTargetVoltageuV,
                volt.type);
            switch(volt.type)
            {
                case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_LOGICAL:
                    Printf(pri,"logical) logicalVoltageuV:%d\n",
                        volt.data.logical.logicalVoltageuV);
                    break;
                case LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VDT:
                    Printf(pri,"vdt) vdtIndex:%d\n",
                        volt.data.vdt.vdtIndex);
                    break;
                default:
                    Printf(pri,"type:unknown\n");
                    break;
            }
        }
    }
    return OK;
}

RC Perf20::GetVirtualPerfPoints(PerfPoints *pPoints)
{
    MASSERT(pPoints);
    RC rc;
    UINT64 tdpGpcPerfHz = 0;

    // Set up PerfPoints that track the named vpstates.
    for (int i = 0; i < NUM_GpcPerfMode; i++)
    {
        const GpcPerfMode gpcPerfMode = static_cast<GpcPerfMode>(i);

        if (0 == GetVPStateId(gpcPerfMode))
            continue;  // not a vpstate-based gpc2perfmode

        if (HasVirtualPState(gpcPerfMode))
        {
            UINT64 gpcHz;
            UINT32 pstate;
            CHECK_RC(ClockOfVPState(
                         GetVPStateId(gpcPerfMode),
                         true,
                         &gpcHz,
                         &pstate));

            PerfPoint p(pstate, gpcPerfMode, gpcHz);

            if (gpcPerfMode == GpcPerf_TDP)
            {
                tdpGpcPerfHz = gpcHz;
            }
            else
            {
                // Assumes GpcPerf_TDP comes before MIDPOINT, TURBO.
                // Assumes all vpstate vbios' support TDP.
                MASSERT(tdpGpcPerfHz != 0);
                p.SetUnsafeIfFasterThan(tdpGpcPerfHz);
            }
            pPoints->insert(p);
        }
    }
    return rc;
}

RC Perf20::GenerateStandardPerfPoints
(
    UINT64 fastestSafeGpcHz,
    const PStateInfos &pstateInfo,
    PerfPoints *pPoints
)
{
    MASSERT(pPoints);
    RC rc;
    for (UINT32 iState = 0; iState < pstateInfo.size(); iState++)
    {
        UINT32 GpcDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkGpc);

        // Make sure it's a valid RM gpc clock domain
        bool using1XGpcClk = false;
        if (!IsRmClockDomain(Gpu::ClkGpc))
        {
            using1XGpcClk = true;
            GpcDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkGpc2);
        }

        map<UINT32, ClkRange>::const_iterator clkIt = pstateInfo[iState].ClkVals.find(GpcDomain);
        if (clkIt == pstateInfo[iState].ClkVals.end())
        {
            Printf(Tee::PriError,
                   "PState %d does not have an entry for GpcClk\n",
                   pstateInfo[iState].PStateNum);
            return RC::SOFTWARE_ERROR;
        }
        const ClkRange &ClkInfo = clkIt->second;
        const UINT32 pstate = pstateInfo[iState].PStateNum;

        // Note that ClkInfo is actually gpc2clk!!!

        if ((m_PStateVersion < PStateVersion::PSTATE_20) || (ClkInfo.MinKHz == ClkInfo.MaxKHz))
        {
            // If this pstate has just one gpcperf setting, call it #.max.
            // .nom/min/mid etc would also be accurate, but we by default
            // only test .max and .intersect in gputest.js.
            PerfPoint p(pstate, GpcPerf_MAX,
                ClkInfo.LwrrKHz * 1000ULL / (using1XGpcClk ? 2ULL : 1ULL));
            p.SetUnsafeIfFasterThan(fastestSafeGpcHz);
            pPoints->insert(p);
            continue;
        }

        {
            // Call the "default gpcperf" value #.nom.  It's not clear how
            // valuable this PerfPoint is, but it's tradition.
            PerfPoint p(pstate, GpcPerf_NOM,
                ClkInfo.LwrrKHz * 1000ULL / (using1XGpcClk ? 2ULL : 1ULL));
            p.SetUnsafeIfFasterThan(fastestSafeGpcHz);
            pPoints->insert(p);
        }
        {
            PerfPoint p(pstate, GpcPerf_MAX,
                ClkInfo.MaxKHz * 1000ULL / (using1XGpcClk ? 2ULL : 1ULL));

            if (pstate == 0)
            {
                const UINT32 rmDispDomain =
                    PerfUtil::GpuClkDomainToCtrl2080Bit(Gpu::ClkDisp);
                map<UINT32, ClkRange> &orgClkInfo =
                    m_CachedPStates[iState].ClkVals;
                if (orgClkInfo.find(rmDispDomain) != orgClkInfo.end())
                {
                    const ClkSetting maxDispClk(Gpu::ClkDisp,
                        orgClkInfo[rmDispDomain].MaxKHz * 1000ULL, 0, false);
                    p.Clks[Gpu::ClkDisp] = maxDispClk;
                }
            }

            p.SetUnsafeIfFasterThan(fastestSafeGpcHz);
            pPoints->insert(p);
        }
        {
            PerfPoint p(pstate, GpcPerf_MID,
                (ClkInfo.MinKHz + ClkInfo.MaxKHz) / 2 * 1000ULL / (using1XGpcClk ? 2ULL : 1ULL));
            p.SetUnsafeIfFasterThan(fastestSafeGpcHz);
            pPoints->insert(p);
        }
        {
            PerfPoint p(pstate, GpcPerf_MIN,
                ClkInfo.MinKHz * 1000ULL / (using1XGpcClk ? 2ULL : 1ULL));
            p.SetUnsafeIfFasterThan(fastestSafeGpcHz);
            pPoints->insert(p);
        }
        {
            // Fill in the same gpcclk for .intersect as for .min.
            // Having an in-range (if incorrect) gpcclk for each standard
            // PerfPoint avoids bug 1246902 where the default pstate 0
            // gpc freq is below the legal (min,max) for pstate 0.
            PerfPoint p(pstate, GpcPerf_INTERSECT,
                ClkInfo.MinKHz * 1000ULL / (using1XGpcClk ? 2ULL : 1ULL));
            pPoints->insert(p);
        }
    }
    return rc;
}

RC Perf20::GetLwrrentPerfValuesNoCache(PStateInfos *pPstatesInfo)
{
    MASSERT(pPstatesInfo);
    RC rc;

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);

    PStateInfos &pstatesInfo = *pPstatesInfo;

    vector<LW2080_CTRL_PERF_CLK_DOM_INFO> ClkDomBuffer;
    vector<LW2080_CTRL_PERF_CLK_DOM2_INFO> ClkDom2Buffer;
    vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> VoltDomBuffer;

    // setup the clock / voltage domain in each list
    AssignDomainsToTypeWithDomainBuffer(m_ClkDomainsMask, &ClkDomBuffer);
    AssignDomainsToTypeWithDomainBuffer(m_ClkDomainsMask, &ClkDom2Buffer);
    AssignDomainsToTypeWithDomainBuffer(m_VoltageDomainsMask, &VoltDomBuffer);

    LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS Params = {};

    if (0 != m_ClkDomainsMask)
    {
        Params.perfClkDomInfoListSize = Utility::CountBits(m_ClkDomainsMask);
        Params.perfClkDomInfoList = LW_PTR_TO_LwP64(&ClkDomBuffer[0]);
        Params.perfClkDom2InfoList = LW_PTR_TO_LwP64(&ClkDom2Buffer[0]);
    }

    if (0 != m_VoltageDomainsMask)
    {
        Params.perfVoltDomInfoListSize = Utility::CountBits(m_VoltageDomainsMask);
        Params.perfVoltDomInfoList = LW_PTR_TO_LwP64(&VoltDomBuffer[0]);
    }

    // Iterate through all pstates, get info for each one
    for (UINT32 i = 0; i < pstatesInfo.size(); i++)
    {
        Params.pstate = pstatesInfo[i].PStateBit;
        CHECK_RC(pLwRm->Control(pRmHandle,
                                LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
                                &Params,
                                sizeof(Params)));

        // get the clock settings for each clock domain of this pstate
        for (UINT32 idx = 0; idx < Params.perfClkDomInfoListSize; idx++)
        {
            LW2080_CTRL_PERF_CLK_DOM_INFO &ClkInfo = ClkDomBuffer[idx];
            pstatesInfo[i].ClkVals[ClkInfo.domain].LwrrKHz = ClkInfo.freq;
            pstatesInfo[i].ClkFlags[ClkInfo.domain] = ClkInfo.flags;

            LW2080_CTRL_PERF_CLK_DOM2_INFO &Clk2Info = ClkDom2Buffer[idx];
            pstatesInfo[i].ClkVals[ClkInfo.domain].MinKHz = Clk2Info.minFreq;
            pstatesInfo[i].ClkVals[ClkInfo.domain].MaxKHz = Clk2Info.maxFreq;
        }
        // get the voltage settings for each voltage domain of this pstate
        for (UINT32 idx = 0; idx < Params.perfVoltDomInfoListSize; idx++)
        {
            LW2080_CTRL_PERF_VOLT_DOM_INFO &VoltDomInfo =  VoltDomBuffer[idx];
            VoltInfo newVoltInfo = {};
            if (VoltDomInfo.type == LW2080_CTRL_PERF_VOLT_DOM_INFO_TYPE_VDT)
            {
                newVoltInfo.VdtIdx = VoltDomInfo.data.vdt.vdtIndex;
            }
            else
            {
                newVoltInfo.VdtIdx = LW2080_CTRL_PMGR_VOLT_VDT_ENTRY_INDEX_ILWALID;
                newVoltInfo.LogicalMv = VoltDomInfo.data.logical.logicalVoltageuV / 1000;
            }
            newVoltInfo.LwrrentTargetMv = VoltDomInfo.lwrrTargetVoltageuV / 1000;
            pstatesInfo[i].VoltageVals[VoltDomInfo.domain] = newVoltInfo;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf20::PrintAllPStateInfo()
{
    RC rc;
    CHECK_RC(GetPStatesInfo());
    for (UINT32 i = 0; i < m_CachedPStates.size(); i++)
    {
        Printf(Tee::PriNormal, "PState %d (0x%08x)\n",
               m_CachedPStates[i].PStateNum, m_CachedPStates[i].PStateBit);

        map<UINT32, ClkRange>::const_iterator ClkIter =
            m_CachedPStates[i].ClkVals.begin();
        for (; ClkIter != m_CachedPStates[i].ClkVals.end(); ++ClkIter)
        {
            Gpu::ClkDomain Domain = PerfUtil::ClkCtrl2080BitToGpuClkDomain(ClkIter->first);

            Printf(Tee::PriNormal, "  %10s: %5d MHz| Flags: 0x%x",
                   PerfUtil::ClkDomainToStr(Domain),
                   ClkIter->second.LwrrKHz/1000,
                   m_CachedPStates[i].ClkFlags[ClkIter->first]);

            if (m_ClockDomains.find(Domain) == m_ClockDomains.end())
            {
                Printf(Tee::PriNormal, "\n");
                continue;
            }

            Printf(Tee::PriNormal, "| Type:%d ", m_ClockDomains[Domain].Type);

            if (ClkIter->second.MinKHz != ClkIter->second.MaxKHz)
            {
                Printf(Tee::PriNormal, "| MinFreq: %d MHz| MaxFreq: %d MHz",
                       ClkIter->second.MinKHz/1000,
                       ClkIter->second.MaxKHz/1000);
            }

            if (m_ClockDomains[Domain].RatioDomain != Gpu::ClkUnkUnsp)
            {
                Printf(Tee::PriNormal, "| Master: %10s| Ratio: %d",
                       PerfUtil::ClkDomainToStr(m_ClockDomains[Domain].RatioDomain),
                       m_ClockDomains[Domain].Ratio);
            }

            Printf(Tee::PriNormal, "\n");
        }
        map<UINT32, VoltInfo>::const_iterator VolIter =
            m_CachedPStates[i].VoltageVals.begin();
        for (; VolIter != m_CachedPStates[i].VoltageVals.end(); VolIter++)
        {
            Gpu::VoltageDomain Domain = PerfUtil::VoltCtrl2080BitToGpuVoltDomain(VolIter->first);
            if (Domain == Gpu::ILWALID_VoltageDomain)
            {
                continue;
            }
            Printf(Tee::PriNormal, "Volt Domain %10s (0x%08x): %d mV (%d)\n",
                   PerfUtil::VoltDomainToStr(Domain),
                   VolIter->first,
                   VolIter->second.LogicalMv,
                   VolIter->second.VdtIdx);
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC Perf20::GetClockAtPStateNoCache(UINT32 PStateNum,
                                 Gpu::ClkDomain Domain,
                                 UINT64 *pFreqHz)
{
    MASSERT(pFreqHz);
    RC rc;

    // Make sure it's a valid RM clock domain
    FLOAT32 freqRatio = 1.0f;
    if (!IsRmClockDomain(Domain))
    {
        freqRatio = PerfUtil::GetRmToModsFreqRatio(Domain);
        Domain = PerfUtil::ColwertToOppositeClockDomain(Domain);
        if (Domain == Gpu::ClkUnkUnsp)
        {
            Printf(Tee::PriError, "Specified clock domain doesn't exist.\n");
            return RC::ILWALID_CLOCK_DOMAIN;
        }
    }

    UINT32 RmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(Domain);

    LW2080_CTRL_PERF_CLK_DOM_INFO ClkInfo  = {0};
    ClkInfo.domain = RmDomain;
    LW2080_CTRL_PERF_CLK_DOM2_INFO Clk2Info = {0};
    Clk2Info.domain = RmDomain;

    if (PStateNum == ILWALID_PSTATE)
    {
        CHECK_RC(GetLwrrentPState(&PStateNum));
    }

    LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS Params   = {0};
    Params.pstate                  = PStateNumTo2080Bit(PStateNum);
    Params.perfVoltDomInfoListSize = 0;
    Params.perfClkDomInfoListSize  = 1;
    Params.perfClkDomInfoList      = LW_PTR_TO_LwP64(&ClkInfo);
    Params.perfClkDom2InfoList     = LW_PTR_TO_LwP64(&Clk2Info);
    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
                            &Params,
                            sizeof(Params)));
    *pFreqHz = static_cast<UINT64>(ClkInfo.freq * freqRatio * 1000ULL);
    return rc;
}

RC Perf20::GetVoltageAtPStateNoCache(UINT32 PStateNum,
                             Gpu::VoltageDomain Domain,
                             UINT32 *pVoltageUv)
{
    MASSERT(pVoltageUv);
    RC rc;
    LW2080_CTRL_PERF_VOLT_DOM_INFO VoltInfo = {0};
    VoltInfo.domain = PerfUtil::GpuVoltageDomainToCtrl2080Bit(Domain);

    LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS Params  = {0};
    Params.pstate                  = PStateNumTo2080Bit(PStateNum);
    Params.perfVoltDomInfoListSize = 1;
    Params.perfClkDomInfoListSize  = 0;
    Params.perfVoltDomInfoList     = LW_PTR_TO_LwP64(&VoltInfo);

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
                            &Params,
                            sizeof(Params)));
    *pVoltageUv = VoltInfo.lwrrTargetVoltageuV;
    return rc;
}

//-----------------------------------------------------------------------------
//! Note: this might be obsolte with future PState 2.0 plan
RC Perf20::GetPexSpeedAtPState(UINT32 PStateNum, UINT32 *pLinkSpeed)
{
    RC rc;
    LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS Params = {0};
    Params.pstate                  = PStateNumTo2080Bit(PStateNum);
    Params.perfVoltDomInfoListSize = 0;
    Params.perfClkDomInfoListSize  = 0;

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
                            LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
                            &Params,
                            sizeof(Params)));

    UINT32 PexSpeed = DRF_VAL(2080_CTRL,_GET_PSTATE,_FLAG_PCIELINKSPEED, Params.flags);
    switch (PexSpeed)
    {
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKSPEED_GEN3:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed8000MBPS);
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKSPEED_GEN2:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed5000MBPS);
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKSPEED_GEN1:
            *pLinkSpeed = static_cast<UINT32>(Pci::Speed2500MBPS);
            break;
        default:
            *pLinkSpeed = static_cast<UINT32>(Pci::SpeedUnknown);
    }
    return rc;
}
//-----------------------------------------------------------------------------
//! Note: this might be obsolte with future PState 2.0 plan
RC Perf20::GetLinkWidthAtPState(UINT32 PStateNum, UINT32 *pLinkWidth)
{
    RC rc;
    *pLinkWidth = 0;

    LW2080_CTRL_PERF_GET_PSTATE2_INFO_PARAMS params = {0};
    params.pstate = PStateNumTo2080Bit(PStateNum);

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    CHECK_RC(pLwRm->Control(pRmHandle,
           LW2080_CTRL_CMD_PERF_GET_PSTATE2_INFO,
           &params,
           sizeof(params)));

    UINT32 widthFlag = DRF_VAL(2080_CTRL,_GET_PSTATE,_FLAG_PCIELINKWIDTH, params.flags);
    switch(widthFlag)
    {
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_UNDEFINED:
            *pLinkWidth = 0;
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_1:
            *pLinkWidth = 1;
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_2:
            *pLinkWidth = 2;
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_4:
            *pLinkWidth = 4;
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_8:
            *pLinkWidth = 8;
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_12:
            *pLinkWidth = 12;
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_16:
            *pLinkWidth = 16;
            break;
        case LW2080_CTRL_GET_PSTATE_FLAG_PCIELINKWIDTH_32:
            *pLinkWidth = 32;
            break;
        default:
            *pLinkWidth = 0;
    }

    return OK;
}

RC Perf20::ClearVoltTuningOffsets()
{
    RC rc;
    CHECK_RC(Perf::ClearVoltTuningOffsets());
    if (m_VdtTuningEntryModified)
    {
        rc = SetVdtTuningOffsetMv(0);
        m_VdtTuningEntryModified = false;
    }
    if (m_PStateVoltageModified)
    {
        // set pstate voltage offset to 0 for all pstates
        for (UINT32 i = 0 ; i < m_CachedPStates.size(); i++)
        {
            rc = SetPStateOffsetMv(m_CachedPStates[i].PStateNum, 0);
        }
        m_PStateVoltageModified = false;
    }
    return rc;
}

RC Perf20::InnerSetPStateLock(const PerfPoint &Setting, PStateLockType Lock)
{
    RC rc;
    UINT64 startTime = Xp::GetWallTimeUS();
    string funcName;
    switch (Lock)
    {
        case VirtualLock:
            CHECK_RC(SetIntersectPt(Setting));
            funcName = "SetPStateLock (intersect)";
            break;

        case SoftLock:
            if (Setting.FastPStateSwitch)
            {
                CHECK_RC(InnerFastSuggestPState(
                                Setting.PStateNum,
                                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
                funcName = "SetPStateLock (fast)";
            }
            else
            {
                CHECK_RC(InnerSuggestPState(
                                Setting.PStateNum,
                                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
                funcName = "SetPStateLock (normal)";
            }
            break;
        case HardLock:
        default:
            if (Setting.FastPStateSwitch)
            {
                CHECK_RC(InnerFastForcePState(
                                Setting.PStateNum,
                                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
                funcName = "SetPStateLock (fast)";
            }
            else
            {
                CHECK_RC(InnerForcePState(
                                Setting.PStateNum,
                                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
                funcName = "SetPStateLock (normal)";
            }
            break;
    }
    Printf(m_VfSwitchTimePriority,
           "%s took %lluus\n", funcName.c_str(), Xp::GetWallTimeUS() - startTime);

    m_CacheStatus = CACHE_DIRTY;
    m_PStateNumCache = CACHE_DIRTY;
    m_LwrPerfPointPStateNum = ILWALID_PSTATE;

    return rc;
}

RC Perf20::InnerSuggestPState(UINT32 TargetState, UINT32 FallBack)
{
    return ForcePStateInternal(
        TargetState, FallBack, Perf::NORMAL_SWITCH, Perf::SoftLock);
}

RC Perf20::InnerFastSuggestPState(UINT32 TargetState, UINT32 FallBack)
{
    return ForcePStateInternal(
        TargetState, FallBack, Perf::FAST_SWITCH, Perf::SoftLock);
}

RC Perf20::InnerForcePState(UINT32 TargetState, UINT32 FallBack)
{
    RC rc;
    if (m_ForcedPStateLockType != HardLock)
    {
        CHECK_RC(SuggestPState(TargetState, FallBack));
        return rc;
    }

    return ForcePStateInternal(
        TargetState, FallBack, Perf::NORMAL_SWITCH, Perf::HardLock);
}

RC Perf20::InnerFastForcePState(UINT32 TargetState, UINT32 FallBack)
{
    RC rc;

    if (m_ForcedPStateLockType != HardLock)
    {
        CHECK_RC(FastSuggestPState(TargetState, FallBack));
        return rc;
    }

    return ForcePStateInternal(
        TargetState, FallBack, Perf::FAST_SWITCH, Perf::HardLock);
}

RC Perf20::ForcePStateInternal
(
    UINT32 pstateNum,
    UINT32 fallback,
    PStateSwitchType switchType,
    PStateLockType newLockType
)
{
    RC rc;

    switch (fallback)
    {
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR:
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_HIGHER_PERF:
        case LW2080_CTRL_PERF_PSTATE_FALLBACK_LOWER_PERF:
            break;
        default:
            Printf(Tee::PriError, "Unknown fall back strategy: %d\n", fallback);
            return RC::BAD_PARAMETER;
    }

    CHECK_RC(MaybeUnlock(newLockType));

    // Update PState scope Vdt entry.
    // Ideally this would be atomic with the new pstate lock.  But we lack
    // an API for that.
    m_VoltTuningOffsetsMv[VTUNE_PSTATE] =
        m_PStateVoltTuningOffsetsMv[pstateNum];
    CHECK_RC(SetVoltTuningParams(writeIfChanged));

    UINT64 timeBefore = Xp::GetWallTimeUS();
    rc = DoRmPStateLock(pstateNum, fallback, switchType, newLockType);
    Printf(m_VfSwitchTimePriority,
           "DoRmPStateLock took %lluus\n",
           Xp::GetWallTimeUS() - timeBefore);

    if (rc != OK)
    {
        Printf(Tee::PriNormal, "set pState failed - please check if the desired"
                               " pState is available: \n");
        UINT32 NumPStates = 0;
        GetNumPStates(&NumPStates);
        if (NumPStates == 0)
        {
            Printf(Tee::PriError,
                   "No pstates detected on dev.sub = %d.%d. Check vbios!\n",
                   m_pSubdevice->GetParentDevice()->GetDeviceInst(),
                   m_pSubdevice->GetSubdeviceInst());
        }
        else
        {
            PrintAllPStateInfo();
        }
    }
    else
    {
        // We should always unlock before forcing a pstate, except for
        // a transition using the same lock type, or when transitioning
        // from X.intersect when we unlock *after*.
        MASSERT((NotLocked == m_ActivePStateLockType) ||
                (newLockType == m_ActivePStateLockType) ||
                (VirtualLock == m_ActivePStateLockType));

        m_ActivePStateLockType = newLockType;

        // Now that we've set the FORCE_PSTATE_EX lock, release any PERF_LIMIT lock
        // we might be holding.
        rc = ClearPerfLimits();
    }
    return rc;
}

RC Perf20::DoRmPStateLock
(
    UINT32 pstateNum,
    UINT32 fallback,
    PStateSwitchType switchType,
    PStateLockType lockType
)
{
    UINT32 flags = 0;
    if (switchType == FAST_SWITCH)
    {
        flags |= DRF_DEF(2080_CTRL_PERF,
                         _SET_FORCE_PSTATE_EX,
                         _FLAGS_VBLANK_WAIT,
                         _SKIP);
    }
    else
    {
        flags |= DRF_DEF(2080_CTRL_PERF,
                         _SET_FORCE_PSTATE_EX,
                         _FLAGS_VBLANK_WAIT,
                         _DEFAULT);
    }
    flags |= ForceFlag(lockType);
    LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS Params = {};
    Params.forcePstate = PStateNumTo2080Bit(pstateNum);
    Params.fallback    = fallback;
    Params.flags       = flags;

    Printf(Tee::PriLow, "set pstate: flags = 0x%x\n", flags);

    return LwRmPtr()->ControlBySubdevice(m_pSubdevice,
                        LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE_EX,
                        &Params,
                        sizeof(Params));
}

RC Perf20::ClearForcePStateEx(UINT32 Flags)
{
    RC rc;
    if (m_UsePStateCallbacks)
    {
        CallbackArguments args;
        args.Push(m_pSubdevice->GetGpuInst());
        CHECK_RC(m_Callbacks[PRE_CLEANUP].Fire(Callbacks::STOP_ON_ERROR));
    }

    LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS Params = {0};
    Params.forcePstate = LW2080_CTRL_PERF_PSTATES_CLEAR_FORCED;
    Params.fallback    = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
    Params.flags       = Flags;

    LwRmPtr pLwRm;
    LwRm::Handle pRmHandle = pLwRm->GetSubdeviceHandle(m_pSubdevice);
    MASSERT(pRmHandle);
    rc = pLwRm->Control(pRmHandle,
                        LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE_EX,
                        &Params,
                        sizeof(Params));

    if (m_ActivePStateLockType == HardLock || m_ActivePStateLockType == SoftLock)
        m_ActivePStateLockType = NotLocked;

    m_PStateNumCache = CACHE_DIRTY;
    return rc;
}

RC Perf20::ClearForcedPState()
{
    Printf(Tee::PriDebug, "Perf20::ClearForcedPState()\n");
    StickyRC rc = ClearForcePStateEx(ForceFlag(m_ActivePStateLockType));
    rc = ClearPerfLimits();
    return rc;
}

