/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.
 * All rights reserved. All information contained herein is proprietary and
 * confidential to LWPU Corporation. Any use, reproduction, or disclosure
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/abort.h"
#include "core/include/registry.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/mle_protobuf.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "gpu/perf/avfssub.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/thermsub.h"
#include "gpu/perf/voltsub.h"

class CheckAVFS : public GpuTest
{
public:
    CheckAVFS();

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(VoltageStride, UINT08);
    SETGET_PROP(AdcMargin, FLOAT32);
    SETGET_PROP(FreqMargin, FLOAT32);
    SETGET_PROP(ForceAdcAcc, bool);
    SETGET_PROP(MilwoltuV, UINT32);
    SETGET_PROP(MaxVoltuV, UINT32);
    SETGET_PROP(CheckEffectiveFrequency, bool);
    SETGET_PROP(ShowWorstAdcDelta, bool);
    SETGET_PROP(SkipRegkeyCheck, bool);
    SETGET_JSARRAY_PROP_LWSTOM(PerAdcMargins)
    SETGET_PROP(AllowAnyVmin, bool);
    SETGET_PROP(SimulateTemp, bool);
    SETGET_PROP(TempDegC, FLOAT32);
    SETGET_PROP(VoltSettleMs, UINT32);
    SETGET_PROP(LowTempLwtoff, FLOAT32);
    SETGET_PROP(LowTempAdcMargin, FLOAT32);
    SETGET_PROP(NegAdcMargin, FLOAT32);
    SETGET_PROP(NegLowTempAdcMargin, FLOAT32);

    // TODO: Remove these in 2022 or later...
    RC SetSkipAdcCalCheck(bool val);
    bool GetSkipAdcCalCheck() const;

private:
    PStateOwner m_PStateOwner;
    Perf::PerfPoint m_OrigPerfPoint;
    map<Gpu::SplitVoltageDomain, UINT32> m_AdcDevMasks;

    struct VoltageInfo
    {
        UINT32 milwoltuV;
        UINT32 maxVoltuV;
        UINT32 stepSizeuV;
    };
    map<Gpu::SplitVoltageDomain, VoltageInfo> m_VoltInfos;

    Perf::PStateLockType m_OrigPStateLock = Perf::NotLocked;
    Perf* m_pPerf = nullptr;
    Avfs* m_pAvfs = nullptr;
    Volt3x* m_pVolt3x = nullptr;
    bool m_RunOnError = false;

    // User-configurable arguments
    FLOAT32 m_AdcMargin = 3.0f;
    FLOAT32 m_FreqMargin = 3.0f;
    UINT08 m_VoltageStride = 8; // 8 * 6.25mV = 50mV steps for Pascal/Volta

    vector<Gpu::ClkDomain> m_ClfcClkDoms;
    set<Gpu::SplitVoltageDomain> m_ClvcVoltDoms = {};

    bool m_ForceAdcAcc = false;

    UINT32 m_MilwoltuV = 0;
    UINT32 m_MaxVoltuV = 0;
    bool m_CheckEffectiveFrequency = false;
    bool m_SkipRegkeyCheck = false;

    bool m_ShowWorstAdcDelta = false;
    struct AdcErrorInfo
    {
        FLOAT32 delta = 0.0f;
        Avfs::AdcId adcId = Avfs::AdcId_NUM;
        UINT32 voltuV = 0;
    };
    AdcErrorInfo m_WorstAdcCodeDelta = {};

    map<Avfs::AdcId, FLOAT32> m_PerAdcMarginMap = {};
    JsArray m_PerAdcMargins;

    bool m_AllowAnyVmin = false;

    bool m_SimulateTemp = true;
    static constexpr FLOAT32 ILWALID_TEMP = -1234.0f;
    FLOAT32 m_TempDegC = ILWALID_TEMP;
    UINT32 m_VoltSettleMs = 0;
    FLOAT32 m_LowTempLwtoff = ILWALID_TEMP;
    FLOAT32 m_LowTempAdcMargin = 5.0f;
    FLOAT32 m_InitTemp = ILWALID_TEMP;

    struct AdcMargin
    {
        FLOAT32 positive;
        FLOAT32 negative;
    };
    FLOAT32 m_NegAdcMargin = 0.0f;
    FLOAT32 m_NegLowTempAdcMargin = 0.0f;

    RC InnerRun();
    RC QueryVoltageInfo(Gpu::SplitVoltageDomain voltDom, VoltageInfo* pVoltInfo);
    RC SetVoltageuV
    (
        Gpu::SplitVoltageDomain voltDom,
        UINT32 targetVoltuV,
        UINT32* pObservedVoltuV
    );
    RC CollectAdcCodes
    (
        UINT32 adcDevMask,
        map<Avfs::AdcId, vector<UINT08>>* pSampledCodes,
        UINT32 numSamples
    );
    struct AdcStat
    {
        Avfs::AdcId adcId;
        float avgSample;
        UINT08 minSample;
        UINT08 maxSample;
    };
    vector<AdcStat> ParseAdcCodes
    (
        UINT32 adcDevMask,
        const map<Avfs::AdcId, vector<UINT08>>& sampledCodes
    );
    RC CheckAdcCodes
    (
        Gpu::SplitVoltageDomain voltDom,
        UINT32 lwrrVoltuV,
        const vector<AdcStat>& adcStats,
        bool isFinalCheck
    );
    RC CheckNafllFreqs
    (
        Gpu::SplitVoltageDomain voltDom,
        UINT32 lwrrVoltuV,
        const VoltageInfo& voltInfo
    );

    RC SetClfcEnable(bool bEnable) const;

    RC LockSimulatedTemp(FLOAT32 degC) const;
    RC UnlockSimulatedTemp() const;
    AdcMargin LookupAdcMargin(Avfs::AdcId adcId) const;
};

JS_CLASS_INHERIT(CheckAVFS, GpuTest, "Check all NAFLLs' ADC calibration");

CLASS_PROP_READWRITE(CheckAVFS, VoltageStride, UINT08,
    "Distance between voltages in units of the voltage regulator step size");
CLASS_PROP_READWRITE(CheckAVFS, AdcMargin, FLOAT32,
    "Allowed difference between expected and observed ADC code");
CLASS_PROP_READWRITE(CheckAVFS, FreqMargin, FLOAT32,
    "Allowed difference between expected and observed frequency in terms of VR step size");
CLASS_PROP_READWRITE(CheckAVFS, ForceAdcAcc, bool,
    "Use ADC_ACLW to read NAFLL ADC codes");
CLASS_PROP_READWRITE(CheckAVFS, MilwoltuV, UINT32,
    "Lowest voltage at which ADC calibration will be verified");
CLASS_PROP_READWRITE(CheckAVFS, MaxVoltuV, UINT32,
    "Highest voltage at which ADC calibration will be verified");
CLASS_PROP_READWRITE(CheckAVFS, CheckEffectiveFrequency, bool,
    "Check each NAFLLs effective frequency at each voltage tested");
CLASS_PROP_READWRITE(CheckAVFS, ShowWorstAdcDelta, bool,
    "Write biggest difference between expected and actual code to stdout");
CLASS_PROP_READWRITE(CheckAVFS, SkipAdcCalCheck, bool,
    "Allow the test to run on parts that do not have proper ADC calibration fused");
CLASS_PROP_READWRITE(CheckAVFS, SkipRegkeyCheck, bool,
    "Allow the test to run when -adc_cal_check_ignore is used");
CLASS_PROP_READWRITE_JSARRAY(CheckAVFS, PerAdcMargins, JsArray,
    "Specify a margin for one or more ADCs");
CLASS_PROP_READWRITE(CheckAVFS, AllowAnyVmin, bool,
    "Allow the test to set MilwoltuV below VF lwrve requirements");
CLASS_PROP_READWRITE(CheckAVFS, SimulateTemp, bool,
    "Lock the VF lwrves to a simulated temperature");
CLASS_PROP_READWRITE(CheckAVFS, TempDegC, FLOAT32,
    "Specifies to which temperature to lock the VF lwrves");
CLASS_PROP_READWRITE(CheckAVFS, VoltSettleMs, UINT32,
    "Adds a delay between setting a voltage and reading the ADC codes");
CLASS_PROP_READWRITE(CheckAVFS, LowTempLwtoff, FLOAT32,
    "Temperature in Celsius at or below which the test uses LowTempAdcMargin");
CLASS_PROP_READWRITE(CheckAVFS, LowTempAdcMargin, FLOAT32,
    "Allowed ADC margin at low temperatures (see LowTempLwtoff)");
CLASS_PROP_READWRITE(CheckAVFS, NegAdcMargin, FLOAT32,
    "Allowed negative ADC margin");
CLASS_PROP_READWRITE(CheckAVFS, NegLowTempAdcMargin, FLOAT32,
    "Allowed negative ADC margin at low temperatures (see LowTempLwtoff)");

CheckAVFS::CheckAVFS()
{
    SetName("CheckAVFS");
}

bool CheckAVFS::IsSupported()
{
    GpuSubdevice* pGpuSub = GetBoundGpuSubdevice();
    m_pPerf = pGpuSub->GetPerf();
    m_pAvfs = pGpuSub->GetAvfs();
    m_pVolt3x = pGpuSub->GetVolt3x();

    if (!m_pPerf || !m_pPerf->IsPState30Supported() || !m_pPerf->HasPStates())
        return false;

    if (!m_pVolt3x || !m_pVolt3x->IsInitialized())
        return false;

    if (!m_pAvfs || !m_pAvfs->GetAdcDevMask())
        return false;

    // Do not run on parts when -adc_cal_check_ignore is used
    UINT32 adcCalCheckIgnore;
    if (OK == Registry::Read("ResourceManager", "RmClkAdcCalRevCheckIgnore", &adcCalCheckIgnore))
    {
        if (adcCalCheckIgnore && !m_SkipRegkeyCheck)
        {
            return false;
        }
    }

    const UINT32 fuseRev = pGpuSub->Regs().Read32(MODS_FUSE_OPT_ADC_CAL_FUSE_REV_DATA);
    const bool printFuseInfo = (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK);
    if (printFuseInfo)
    {
        VerbosePrintf("OPT_ADC_CAL_FUSE_REV is %u\n", fuseRev);
    }

    // If OPT_ADC_CAL_FUSE_REV is zero, then ATE never calibrated the ADCs.
    // So this test would not be valid to run. This can happen with early
    // bringup parts.
    if (!fuseRev)
    {
        if (printFuseInfo)
        {
            Printf(Tee::PriLow, "Skipping CheckAVFS because OPT_ADC_CAL_FUSE_REV is 0\n");
        }
        return false;
    }

    return true;
}

RC CheckAVFS::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    m_pPerf = GetBoundGpuSubdevice()->GetPerf();
    m_pAvfs = GetBoundGpuSubdevice()->GetAvfs();
    m_pVolt3x = GetBoundGpuSubdevice()->GetVolt3x();

    if (!m_VoltageStride)
    {
        Printf(Tee::PriError, "VoltageStride must not be 0\n");
        return RC::ILWALID_ARGUMENT;
    }
    if (!m_FreqMargin)
    {
        Printf(Tee::PriError, "FreqMargin must not be 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    m_RunOnError = !GetGoldelwalues()->GetStopOnError();

    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));
    CHECK_RC(m_pPerf->GetLwrrentPerfPoint(&m_OrigPerfPoint));

    // Set the PerfPoint with the lowest possible clock frequencies
    vector<UINT32> pstates;
    CHECK_RC(m_pPerf->GetAvailablePStates(&pstates));
    CHECK_RC(m_pPerf->SetPerfPoint(Perf::PerfPoint(pstates.back(), Perf::GpcPerf_MIN)));

    // We must simulate a VFE temperature in order to prevent the VF lwrves
    // from re-evaluating and ilwalidating Vmin. This must be done before QueryVoltageInfo().
    // See bug 2555744 for more details.
    CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetChipTempViaPrimary(&m_InitTemp));
    if (m_SimulateTemp)
    {
        if (m_TempDegC == ILWALID_TEMP)
        {
            m_TempDegC = m_InitTemp;
        }
        CHECK_RC(LockSimulatedTemp(m_TempDegC));
    }

    m_pVolt3x->SetPrintVoltSafetyWarnings(false);
    for (const auto voltDom : m_pVolt3x->GetAvailableDomains())
    {
        CHECK_RC(QueryVoltageInfo(voltDom, &m_VoltInfos[voltDom]));
    }

    PrintProgressInit(GetTestConfiguration()->Loops());

    if (m_pVolt3x->IsPascalSplitRailSupported() || GetBoundGpuSubdevice()->HasBug(2390848))
    {
        // Set a HardLock to allow us to hit V_rel
        m_OrigPStateLock = m_pPerf->GetPStateLockType();
        CHECK_RC(m_pPerf->SetForcedPStateLockType(Perf::HardLock));
    }

    // Save the status of all CLFCs so that we can restore them correctly later
    CHECK_RC(m_pAvfs->GetEnabledFreqControllers(&m_ClfcClkDoms));
    // Disable CLFC to prevent it from interfering with our ADC readings
    CHECK_RC(SetClfcEnable(false));
    CHECK_RC(m_pAvfs->GetEnabledVoltControllers(&m_ClvcVoltDoms));
    CHECK_RC(m_pAvfs->SetVoltControllerEnable(m_ClvcVoltDoms, false));

    if (GetBoundGpuSubdevice()->DeviceId() == Gpu::GP100)
    {
        // ADC_MONITOR is unavailable on GP100
        m_ForceAdcAcc = true;
    }

    if (m_ForceAdcAcc)
    {
        m_AdcMargin = ceil(m_AdcMargin);
        m_LowTempAdcMargin = ceil(m_LowTempAdcMargin);
    }

    if (m_PerAdcMargins.size() % 2)
    {
        Printf(Tee::PriError, "Invalid PerAdcMargins\n");
        return RC::ILWALID_ARGUMENT;
    }
    JavaScriptPtr pJs;
    for (UINT32 ii = 0; ii < m_PerAdcMargins.size(); ii += 2)
    {
        UINT32 adc;
        CHECK_RC(pJs->FromJsval(m_PerAdcMargins[ii], &adc));
        const Avfs::AdcId adcId = static_cast<Avfs::AdcId>(adc);
        FLOAT32 margin;
        CHECK_RC(pJs->FromJsval(m_PerAdcMargins[ii+1], &margin));
        VerbosePrintf("Applying margin override of %.3f to %s\n",
            margin, PerfUtil::AdcIdToStr(adcId));
        m_PerAdcMarginMap[adcId] = margin;
    }

    if (m_NegAdcMargin > 0)
    {
        Printf(Tee::PriError, "NegAdcMargin must be less than or equal to 0\n");
        return RC::ILWALID_ARGUMENT;
    }
    if (m_NegLowTempAdcMargin > 0)
    {
        Printf(Tee::PriError, "NegLowTempAdcMargin must be less than or equal to 0\n");
        return RC::ILWALID_ARGUMENT;
    }

    return rc;
}

namespace
{
    Mle::AdcStats::AdcId ToAdcId(Avfs::AdcId adcId)
    {
        switch (adcId)
        {
            case Avfs::AdcId::ADC_SYS:       return Mle::AdcStats::adc_sys;
            case Avfs::AdcId::ADC_LTC:       return Mle::AdcStats::adc_ltc;
            case Avfs::AdcId::ADC_XBAR:      return Mle::AdcStats::adc_xbar;
            case Avfs::AdcId::ADC_LWD:       return Mle::AdcStats::adc_lwd;
            case Avfs::AdcId::ADC_HOST:      return Mle::AdcStats::adc_host;
            case Avfs::AdcId::ADC_GPC0:      return Mle::AdcStats::adc_gpc0;
            case Avfs::AdcId::ADC_GPC1:      return Mle::AdcStats::adc_gpc1;
            case Avfs::AdcId::ADC_GPC2:      return Mle::AdcStats::adc_gpc2;
            case Avfs::AdcId::ADC_GPC3:      return Mle::AdcStats::adc_gpc3;
            case Avfs::AdcId::ADC_GPC4:      return Mle::AdcStats::adc_gpc4;
            case Avfs::AdcId::ADC_GPC5:      return Mle::AdcStats::adc_gpc5;
            case Avfs::AdcId::ADC_GPC6:      return Mle::AdcStats::adc_gpc6;
            case Avfs::AdcId::ADC_GPC7:      return Mle::AdcStats::adc_gpc7;
            case Avfs::AdcId::ADC_GPC8:      return Mle::AdcStats::adc_gpc8;
            case Avfs::AdcId::ADC_GPC9:      return Mle::AdcStats::adc_gpc9;
            case Avfs::AdcId::ADC_GPC10:     return Mle::AdcStats::adc_gpc10;
            case Avfs::AdcId::ADC_GPC11:     return Mle::AdcStats::adc_gpc11;
            case Avfs::AdcId::ADC_GPCS:      return Mle::AdcStats::adc_gpcs;
            case Avfs::AdcId::ADC_SRAM:      return Mle::AdcStats::adc_sram;
            case Avfs::AdcId::ADC_SYS_ISINK: return Mle::AdcStats::adc_sys_isink;
            default:
                MASSERT("Unknown ADC Id colwersion");
                return Mle::AdcStats::adc_sys;
        }
    }
}

RC CheckAVFS::Run()
{
    StickyRC sticky;

    for (UINT32 loop = 0; loop < GetTestConfiguration()->Loops(); loop++)
    {
        sticky = InnerRun();
        PrintProgressUpdate(loop + 1);
        if (!(sticky == OK || m_RunOnError) || Abort::Check() != OK)
        {
            break;
        }
    }

    const AdcMargin worstAdcMargin = LookupAdcMargin(m_WorstAdcCodeDelta.adcId);
    const FLOAT32 worstAdcMargilwal = m_WorstAdcCodeDelta.delta > 0 ?
        worstAdcMargin.positive : worstAdcMargin.negative;

    const Tee::Priority pri = (sticky != OK || m_ShowWorstAdcDelta) ?
        Tee::PriNormal : GetVerbosePrintPri();
    Printf(pri, "ADC Name             : %s\n", PerfUtil::AdcIdToStr(m_WorstAdcCodeDelta.adcId));
    Printf(pri, "Voltage              : %uuV\n", m_WorstAdcCodeDelta.voltuV);
    Printf(pri, "Worst ADC code delta : %f\n", m_WorstAdcCodeDelta.delta);
    Printf(pri, "ADC margin           : %f\n", worstAdcMargilwal);
    Mle::Print(Mle::Entry::worst_adc_delta)
        .adc_id(static_cast<Mle::WorstAdcDelta::AdcIdEnum>(ToAdcId(m_WorstAdcCodeDelta.adcId)))
        .delta(m_WorstAdcCodeDelta.delta)
        .margin(worstAdcMargilwal)
        .lwrrent_volt_uv(m_WorstAdcCodeDelta.voltuV)
        .simulating_temperature(m_TempDegC);
    return sticky;
}

RC CheckAVFS::InnerRun()
{
    RC rc;
    StickyRC firstRc;

    for (const auto voltDom : m_pVolt3x->GetAvailableDomains())
    {
        UINT32 adcDevMask = m_pAvfs->GetAdcDevMask(voltDom);

        // ADC_SRAM is broken on GP100. Avoid testing it.
        if (GetBoundGpuSubdevice()->HasBug(1667555))
        {
            UINT32 adcSramIdx;
            CHECK_RC(m_pAvfs->TranslateAdcIdToDevIndex(Avfs::ADC_SRAM, &adcSramIdx));
            adcDevMask &= ~(1 << adcSramIdx);
        }
        if (!adcDevMask)
        {
            continue;
        }

        const auto& voltInfo = m_VoltInfos[voltDom];
        UINT32 prevVoltuV = 0;

        for (UINT32 targetVoltuV = voltInfo.milwoltuV;
             targetVoltuV <= voltInfo.maxVoltuV;
             targetVoltuV += m_VoltageStride * voltInfo.stepSizeuV)
        {
            if (targetVoltuV > voltInfo.maxVoltuV)
            {
                targetVoltuV = voltInfo.maxVoltuV;
            }

            UINT32 lwrrVoltuV;
            CHECK_RC(SetVoltageuV(voltDom, targetVoltuV, &lwrrVoltuV));

            if (lwrrVoltuV != targetVoltuV)
            {
                VerbosePrintf("RM set %s %u uV\n",
                    PerfUtil::SplitVoltDomainToStr(voltDom),
                    lwrrVoltuV);
                if (lwrrVoltuV <= prevVoltuV)
                {
                    continue;
                }
            }
            prevVoltuV = lwrrVoltuV;

            RC adcRc;
            constexpr array<UINT32, 2> sampleSizes = {{ 192, 1024 }};
            UINT32 sampleSizeIdx = 0;
            do
            {
                map<Avfs::AdcId, vector<UINT08>> sampledCodes;
                CHECK_RC(CollectAdcCodes(adcDevMask, &sampledCodes, sampleSizes[sampleSizeIdx++]));
                vector<AdcStat> adcStats = ParseAdcCodes(adcDevMask, sampledCodes);
                adcRc.Clear();
                adcRc = CheckAdcCodes(voltDom, lwrrVoltuV, adcStats,
                                      sampleSizeIdx == sampleSizes.size());
            }
            while (adcRc != OK && sampleSizeIdx < sampleSizes.size() && !m_ForceAdcAcc);

            // Check calibrations errors later if -run_on_error is used
            if (adcRc == RC::AVFS_ADC_CALIBRATION_ERROR && m_RunOnError)
            {
                firstRc = adcRc;
                adcRc.Clear();
            }
            CHECK_RC(adcRc);

            if (m_CheckEffectiveFrequency)
            {
                rc = CheckNafllFreqs(voltDom, lwrrVoltuV, voltInfo);
                if ((rc == RC::CLOCKS_TOO_LOW || rc == RC::CLOCKS_TOO_HIGH) && m_RunOnError)
                {
                    firstRc = rc;
                    rc.Clear();
                }
                CHECK_RC(rc);
            }
        }
    }

    CHECK_RC(firstRc);

    return rc;
}

RC CheckAVFS::Cleanup()
{
    StickyRC rc;

    if (m_pVolt3x)
    {
        rc = m_pVolt3x->ClearAllSetVoltages();
        m_pVolt3x->SetPrintVoltSafetyWarnings(true);
    }
    if (m_pPerf)
    {
        if (m_OrigPStateLock != Perf::NotLocked)
        {
            rc = m_pPerf->SetForcedPStateLockType(m_OrigPStateLock);
        }
        rc = m_pPerf->SetPerfPoint(m_OrigPerfPoint);
        if (m_SimulateTemp)
        {
            rc = UnlockSimulatedTemp();
        }
    }
    rc = m_pAvfs->SetVoltControllerEnable(m_ClvcVoltDoms, true);
    rc = SetClfcEnable(true);
    m_PStateOwner.ReleasePStates();
    m_PerAdcMargins.clear();
    m_PerAdcMarginMap.clear();

    rc = GpuTest::Cleanup();

    return rc;
}

void CheckAVFS::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "CheckAVFS Js Properties:\n");

    Printf(pri, "\t%-32s %.3f\n", "AdcMargin:", m_AdcMargin);
    Printf(pri, "\t%-32s %.3f\n", "FreqMargin:", m_FreqMargin);
    Printf(pri, "\t%-32s %u\n", "VoltageStride:", m_VoltageStride);
    Printf(pri, "\t%-32s %s\n", "ForceAdcAcc:", m_ForceAdcAcc ? "true" : "false");
    Printf(pri, "\t%-32s %u\n", "MilwoltuV:", m_MilwoltuV);
    Printf(pri, "\t%-32s %u\n", "MaxVoltuV:", m_MaxVoltuV);
    Printf(pri, "\t%-32s %s\n", "CheckEffectiveFrequency:",
        m_CheckEffectiveFrequency ? "true" : "false");
    Printf(pri, "\t%-32s %s\n", "ShowWorstAdcDelta:", m_ShowWorstAdcDelta ? "true" : "false");
    Printf(pri, "\t%-32s %s\n", "SkipRegkeyCheck:", m_SkipRegkeyCheck ? "true" : "false");
    Printf(pri, "\t%-32s %s\n", "AllowAnyVmin:", m_AllowAnyVmin ? "true" : "false");
    Printf(pri, "\t%-32s %s\n", "SimulateTemp:", m_SimulateTemp ? "true" : "false");

    if (m_TempDegC == ILWALID_TEMP)
    {
        Printf(pri, "\t%-32s %s\n", "TempDegC:", "current");
    }
    else
    {
        Printf(pri, "\t%-32s %.1f\n", "TempDegC:", m_TempDegC);
    }
    Printf(pri, "\t%-32s %u\n", "VoltSettleMs:", m_VoltSettleMs);
    if (m_LowTempLwtoff == ILWALID_TEMP)
    {
        Printf(pri, "\t%-32s %s\n", "LowTempLwtoff:", "unset");
    }
    else
    {
        Printf(pri, "\t%-32s %.3f\n", "LowTempLwtoff:", m_LowTempLwtoff);
    }
    Printf(pri, "\t%-32s %.3f\n", "LowTempAdcMargin:", m_LowTempAdcMargin);
    Printf(pri, "\t%-32s %.3f\n", "NegAdcMargin:", m_NegAdcMargin);
    Printf(pri, "\t%-32s %.3f\n", "NegLowTempAdcMargin:", m_NegLowTempAdcMargin);
}

RC CheckAVFS::QueryVoltageInfo
(
    Gpu::SplitVoltageDomain voltDom,
    VoltageInfo* pVoltInfo
)
{
    RC rc;

    CHECK_RC(m_pVolt3x->GetVoltRailRegulatorStepSizeuV(voltDom, &pVoltInfo->stepSizeuV));
    if (!pVoltInfo->stepSizeuV)
    {
        return RC::SOFTWARE_ERROR;
    }
    const UINT32 voltStepuV = pVoltInfo->stepSizeuV * m_VoltageStride;

    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> voltLimitStatus(1);

    LW2080_CTRL_VOLT_VOLT_RAIL_STATUS railStatus = {};

    // Query RM to find rail V_min
    UINT32 lowestSafeVoltageuV;
    CHECK_RC(m_pVolt3x->GetVoltRailStatusViaId(voltDom, &railStatus));
    if (!railStatus.vminLimituV)
    {
        Printf(Tee::PriError, "%s V_min limit not set in VBIOS\n",
            PerfUtil::SplitVoltDomainToStr(voltDom));
        return RC::UNSUPPORTED_FUNCTION;
    }
    lowestSafeVoltageuV = railStatus.vminLimituV;

    // Increase milwoltuV if necessary to accomodate the clock frequencies
    // set at P8.min. Sometimes, SSG sets the rail Vmin below what is
    // required to support all clock frequencies even at P8.min. If we
    // attempt to lock such a low voltage, bad things might happen.
    lowestSafeVoltageuV = max(lowestSafeVoltageuV, railStatus.lwrrVoltDefaultuV);

    if (m_MilwoltuV)
    {
        if (m_MilwoltuV < lowestSafeVoltageuV && !m_AllowAnyVmin)
        {
            Printf(Tee::PriError,
                "MilwoltuV (%uuV) is below the lowest voltage RM will support: %uuV\n",
                m_MilwoltuV, lowestSafeVoltageuV);
            return RC::ILWALID_ARGUMENT;
        }
        pVoltInfo->milwoltuV = m_MilwoltuV;
    }
    else
    {
        // Round Vmin up to the nearest multiple of 50mV
        pVoltInfo->milwoltuV = voltStepuV * ((lowestSafeVoltageuV + voltStepuV - 1) / voltStepuV);
    }

    if (!m_MaxVoltuV)
    {
        // Query RM to find V_rel
        CHECK_RC(m_pVolt3x->GetVoltRailStatusViaId(voltDom, &railStatus));
        if (!railStatus.relLimituV)
        {
            Printf(Tee::PriError, "%s V_rel limits not set in VBIOS\n",
                PerfUtil::SplitVoltDomainToStr(voltDom));
            return RC::UNSUPPORTED_FUNCTION;
        }
        pVoltInfo->maxVoltuV = railStatus.relLimituV;

        // Round Vmax down to the nearest multiple of 50mV
        pVoltInfo->maxVoltuV = voltStepuV * (pVoltInfo->maxVoltuV / voltStepuV);
    }
    else
    {
        pVoltInfo->maxVoltuV = m_MaxVoltuV;
    }

    Tee::Priority rangePri = GetVerbosePrintPri();
    if (pVoltInfo->milwoltuV > pVoltInfo->maxVoltuV)
    {
        Printf(Tee::PriError, "Invalid voltage range\n");
        rc = RC::ILWALID_ARGUMENT;
        rangePri = Tee::PriError;
    }

    Printf(rangePri, "%s volt info:\n", PerfUtil::SplitVoltDomainToStr(voltDom));
    Printf(rangePri, "\tmilwoltuV=%u\n", pVoltInfo->milwoltuV);
    Printf(rangePri, "\tmaxVoltuV=%u\n", pVoltInfo->maxVoltuV);
    Printf(rangePri, "\tstepSizeuV=%u\n", pVoltInfo->stepSizeuV);

    return rc;
}

RC CheckAVFS::SetVoltageuV
(
    Gpu::SplitVoltageDomain voltDom,
    UINT32 targetVoltuV,
    UINT32* pObservedVoltuV
)
{
    RC rc;

    VerbosePrintf("Setting %s = %u uV\n",
        PerfUtil::SplitVoltDomainToStr(voltDom), targetVoltuV);

    // For parts with multiple voltage rails, set a PerfPoint instead of
    // locking a single rail's voltage. By setting a PerfPoint, RM will
    // figure out what the voltage of all other rails will be. Otherwise,
    // this test would have to manage the voltage rail constraints. The
    // tradeoff is that RM may choose a slightly lower voltage than our
    // target because of the VF lwrve shape.
    if (m_pVolt3x->IsPascalSplitRailSupported() || GetBoundGpuSubdevice()->HasBug(2390848))
    {
        Perf::PerfPoint pp(0, Perf::GpcPerf_INTERSECT);
        Perf::IntersectSetting is;
        is.VoltageDomain = voltDom;
        is.Type = Perf::IntersectLogical;
        is.LogicalVoltageuV = targetVoltuV;
        pp.IntersectSettings.insert(is);
        CHECK_RC(m_pPerf->SetPerfPoint(pp));
    }
    else
    {
        CHECK_RC(m_pVolt3x->SetVoltagesuV({ Volt3x::VoltageSetting(voltDom, targetVoltuV) }));
    }

    Tasker::Sleep(m_VoltSettleMs);
    map<Gpu::SplitVoltageDomain, UINT32> observedVoltageuV;
    CHECK_RC(m_pVolt3x->GetLwrrentVoltagesuV(observedVoltageuV));
    const auto& voltItr = observedVoltageuV.find(voltDom);
    if (voltItr == observedVoltageuV.end())
    {
        return RC::SOFTWARE_ERROR;
    }
    *pObservedVoltuV = voltItr->second;

    return rc;
}

RC CheckAVFS::CollectAdcCodes
(
    UINT32 adcDevMask,
    map<Avfs::AdcId, vector<UINT08>>* pSampledCodes,
    UINT32 numSamples
)
{
    MASSERT(adcDevMask);
    MASSERT(pSampledCodes);

    RC rc;

    INT32 adcIdx = 0;

    // Cannot collect instantaneous ADC codes on GP100
    if (m_ForceAdcAcc)
    {
        // Wait for PMU to sample new ADC codes, which happens once a second
        Utility::SleepUS(1100000);

        LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS adcStatuses = {};
        CHECK_RC(m_pAvfs->GetAdcDeviceStatusViaMask(adcDevMask, &adcStatuses));
        UINT32 tmpDevMask = adcDevMask;
        while ((adcIdx = Utility::BitScanForward(tmpDevMask)) >= 0)
        {
            tmpDevMask ^= 1 << adcIdx;
            Avfs::AdcId adcId = m_pAvfs->TranslateDevIndexToAdcId(adcIdx);
            (*pSampledCodes)[adcId].push_back(adcStatuses.devices[adcIdx].sampledCode);
        }
        return rc;
    }

    VerbosePrintf("Collecting %u samples per ADC\n", numSamples);

    for (UINT32 sampleNum = 0; sampleNum < numSamples; sampleNum++)
    {
        LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS adcStatuses = {};
        CHECK_RC(m_pAvfs->GetAdcDeviceStatusViaMask(adcDevMask, &adcStatuses));

        UINT32 tmpDevMask = adcDevMask;
        while ((adcIdx = Utility::BitScanForward(tmpDevMask)) >= 0)
        {
            tmpDevMask ^= 1 << adcIdx;
            Avfs::AdcId adcId = m_pAvfs->TranslateDevIndexToAdcId(adcIdx);
            (*pSampledCodes)[adcId].push_back(adcStatuses.devices[adcIdx].instCode);
        }
    }

    return rc;
}

vector<CheckAVFS::AdcStat> CheckAVFS::ParseAdcCodes
(
    UINT32 adcDevMask,
    const map<Avfs::AdcId, vector<UINT08>>& sampledCodes
)
{
    MASSERT(adcDevMask);

    vector<AdcStat> adcStats;
    adcStats.reserve(sampledCodes.size());

    INT32 adcIdx;
    while ((adcIdx = Utility::BitScanForward(adcDevMask)) >= 0)
    {
        adcDevMask ^= 1 << adcIdx;
        Avfs::AdcId adcId = m_pAvfs->TranslateDevIndexToAdcId(adcIdx);

        UINT08 minSample = 255;
        UINT08 maxSample = 0;
        FLOAT32 avgSample = 0;
        const auto& adcSampledCodes = sampledCodes.find(adcId);
        if (adcSampledCodes == sampledCodes.end())
        {
            continue;
        }

        for (const auto sample : adcSampledCodes->second)
        {
            if (sample < minSample)
            {
                minSample = sample;
            }
            if (sample > maxSample)
            {
                maxSample = sample;
            }
            avgSample += sample;
        }
        avgSample /= adcSampledCodes->second.size();

        AdcStat adcStat;
        adcStat.avgSample = avgSample;
        adcStat.minSample = minSample;
        adcStat.maxSample = maxSample;
        adcStat.adcId = adcId;
        adcStats.push_back(adcStat);
    }

    return adcStats;
}

RC CheckAVFS::CheckAdcCodes
(
    Gpu::SplitVoltageDomain voltDom,
    UINT32 lwrrVoltuV,
    const vector<AdcStat>& adcStats,
    bool isFinalCheck
)
{
    MASSERT(lwrrVoltuV);

    RC rc;

    LwU8 expectedCode = static_cast<LwU8>(
        (lwrrVoltuV - m_pAvfs->GetLutMilwoltageuV()) /
        m_pAvfs->GetLutStepSizeuV());
    const FLOAT32 expCode = static_cast<FLOAT32>(expectedCode);

    // If any ADC has bad calibration, print the readings for all ADCs
    Tee::Priority adcPrintPri = GetVerbosePrintPri();
    if (isFinalCheck)
    {
        for (const auto& adcStat : adcStats)
        {
            const AdcMargin margin = LookupAdcMargin(adcStat.adcId);
            if (adcStat.avgSample < expCode + margin.negative ||
                adcStat.avgSample > expCode + margin.positive)
            {
                adcPrintPri = Tee::PriNormal;
            }
        }
    }

    Printf(adcPrintPri, "expectedCode=%u\n", expectedCode);

    for (const auto& adcStat : adcStats)
    {
        Printf(adcPrintPri, "%s mean sampledCode=%.3f min/max=%u/%u\n",
            PerfUtil::AdcIdToStr(adcStat.adcId),
            adcStat.avgSample, adcStat.minSample, adcStat.maxSample);

        Mle::AdcStats::RailId rail = Mle::AdcStats::unknown;
        switch (voltDom)
        {
            case Gpu::SplitVoltageDomain::VOLTAGE_LOGIC: rail = Mle::AdcStats::logic; break;
            case Gpu::SplitVoltageDomain::VOLTAGE_SRAM:  rail = Mle::AdcStats::sram; break;
            case Gpu::SplitVoltageDomain::VOLTAGE_MSVDD: rail = Mle::AdcStats::msvdd; break;
            default:
                MASSERT("Unknown SplitVoltageDomain colwersion");
        }

        Mle::Print(Mle::Entry::adc_stats)
            .avg_sample(adcStat.avgSample)
            .min_sample(adcStat.minSample)
            .max_sample(adcStat.maxSample)
            .adc_id(ToAdcId(adcStat.adcId))
            .lwrrent_volt_uv(lwrrVoltuV)
            .rail_id(rail)
            .expected_code(expectedCode);

        if (abs(adcStat.avgSample - expCode) > abs(m_WorstAdcCodeDelta.delta) ||
            (isFinalCheck && m_WorstAdcCodeDelta.adcId == adcStat.adcId &&
             m_WorstAdcCodeDelta.voltuV == lwrrVoltuV))
        {
            m_WorstAdcCodeDelta.delta = adcStat.avgSample - expCode;
            m_WorstAdcCodeDelta.adcId = adcStat.adcId;
            m_WorstAdcCodeDelta.voltuV = lwrrVoltuV;
        }

        const AdcMargin margin = LookupAdcMargin(adcStat.adcId);
        if (adcStat.avgSample < expCode + margin.negative ||
            adcStat.avgSample > expCode + margin.positive)
        {
            Tee::Priority mismatchPri = isFinalCheck ? Tee::PriError : adcPrintPri;
            Printf(mismatchPri,
                "%s mean sampled code (%.3f) differs from expected (%u) @ %s=%u uV\n",
                PerfUtil::AdcIdToStr(adcStat.adcId),
                adcStat.avgSample,
                expectedCode,
                PerfUtil::SplitVoltDomainToStr(voltDom),
                lwrrVoltuV);
            rc = RC::AVFS_ADC_CALIBRATION_ERROR;
        }
    }
    
    return rc;
}

RC CheckAVFS::CheckNafllFreqs
(
    Gpu::SplitVoltageDomain voltDom,
    UINT32 lwrrVoltuV,
    const VoltageInfo& voltInfo
)
{
    RC rc;
    StickyRC clockErrorRc;

    // Turn CLFC back on so that it can raise effective frequency if needed
    CHECK_RC(SetClfcEnable(true));
    DEFER
    {
        SetClfcEnable(false);
    };

    vector<Gpu::ClkDomain> nafllDomains;
    m_pAvfs->GetNafllClkDomains(&nafllDomains);

    for (const auto clkDom : nafllDomains)
    {
        // Skip this check for clock domains that do not have CLFC because we
        // cannot guarantee that the effective frequency will be accurate.
        bool clfcEnabled;
        CHECK_RC(m_pAvfs->IsFreqControllerEnabled(clkDom, &clfcEnabled));
        if (!clfcEnabled)
        {
            continue;
        }

        UINT64 targetFreqHz;
        UINT64 effectiveFreqHz;
        CHECK_RC(GetBoundGpuSubdevice()->GetClock(
            clkDom, nullptr, &targetFreqHz, &effectiveFreqHz, nullptr));
        UINT32 targetFreqMHz = static_cast<UINT32>(targetFreqHz / 1000000);
        UINT32 effectiveFreqMHz = static_cast<UINT32>(effectiveFreqHz / 1000000);
        VerbosePrintf("%s targetFreqMHz=%u effectiveFreqMHz=%u\n",
            PerfUtil::ClkDomainToStr(clkDom), targetFreqMHz, effectiveFreqMHz);

        UINT32 lowVoltuV = static_cast<UINT32>(lwrrVoltuV - m_FreqMargin * voltInfo.stepSizeuV);
        lowVoltuV = max(lowVoltuV, voltInfo.milwoltuV); // cap to V_min

        UINT32 highVoltuV = static_cast<UINT32>(lwrrVoltuV + m_FreqMargin * voltInfo.stepSizeuV);
        highVoltuV = min(highVoltuV, voltInfo.maxVoltuV); // cap to V_rel

        UINT32 lowFreqMHz;
        UINT32 highFreqMHz;
        CHECK_RC(m_pPerf->VoltFreqLookup(
            clkDom, voltDom, Perf::VoltToFreq, lowVoltuV, &lowFreqMHz));
        CHECK_RC(m_pPerf->VoltFreqLookup(
            clkDom, voltDom, Perf::VoltToFreq, highVoltuV, &highFreqMHz));

        LW2080_CTRL_CLK_NAFLL_DEVICE_STATUS nafllStatus = {};
        Avfs::NafllId nafllId = m_pAvfs->ClkDomToNafllId(clkDom);
        CHECK_RC(m_pAvfs->GetNafllDeviceStatusViaId(nafllId, &nafllStatus));

        // Skip if the target frequency is too low to guarantee an accurate
        // effective frequency or if we cannot pick reasonable bounds
        if (targetFreqMHz < nafllStatus.dvcoMinFreqMHz ||
            lowFreqMHz >= targetFreqMHz || highFreqMHz <= targetFreqHz)
        {
            continue;
        }

        bool clocksTooLow = (effectiveFreqMHz <= lowFreqMHz);
        if (clocksTooLow || effectiveFreqMHz >= highFreqMHz)
        {
            Printf(Tee::PriError,
                "%s effective freq (%u MHz) %s than target (%u MHz) with margin (%u MHz)\n",
                PerfUtil::ClkDomainToStr(clkDom),
                effectiveFreqMHz,
                clocksTooLow ? "lower" : "higher",
                targetFreqMHz,
                lowFreqMHz);

            clockErrorRc = (clocksTooLow ? RC::CLOCKS_TOO_LOW : RC::CLOCKS_TOO_HIGH);
            if (!m_RunOnError)
            {
                return clockErrorRc;
            }
        }
    }
    CHECK_RC(clockErrorRc);

    return rc;
}

RC CheckAVFS::SetClfcEnable(bool bEnable) const
{
    if (m_ClfcClkDoms.empty())
        return OK;

    VerbosePrintf("%sing CLFC(s): ",
                  bEnable ? "Enabl" : "Disabl");
    for (const auto clkDom : m_ClfcClkDoms)
    {
        VerbosePrintf("%s ", PerfUtil::ClkDomainToStr(clkDom));
    }
    VerbosePrintf("\n");

    return m_pAvfs->SetFreqControllerEnable(m_ClfcClkDoms, bEnable);
}

RC CheckAVFS::GetPerAdcMargins(JsArray *val) const
{
    MASSERT(val != NULL);
    *val = m_PerAdcMargins;
    return OK;
}

RC CheckAVFS::SetPerAdcMargins(JsArray *val)
{
    MASSERT(val != NULL);
    m_PerAdcMargins = *val;
    return OK;
}

RC CheckAVFS::LockSimulatedTemp(FLOAT32 degC) const
{
    VerbosePrintf("Simulating temperature of %.1fC\n", degC);
    return m_pPerf->OverrideVfeVar(Perf::OVERRIDE_VALUE, Perf::CLASS_TEMP, -1, degC);
}

RC CheckAVFS::UnlockSimulatedTemp() const
{
    VerbosePrintf("Removing simulated temperature\n");
    return m_pPerf->OverrideVfeVar(Perf::OVERRIDE_NONE, Perf::CLASS_TEMP, -1, 0.0f);
}

CheckAVFS::AdcMargin CheckAVFS::LookupAdcMargin(const Avfs::AdcId adcId) const
{
    const auto& perAdcMargin = m_PerAdcMarginMap.find(adcId);
    if (perAdcMargin != m_PerAdcMarginMap.end())
    {
        return { perAdcMargin->second, -perAdcMargin->second };
    }
    AdcMargin margin;
    margin.positive = m_AdcMargin;
    margin.negative = m_NegAdcMargin ? m_NegAdcMargin : -m_AdcMargin;
    if (m_LowTempLwtoff != ILWALID_TEMP && m_InitTemp <= m_LowTempLwtoff)
    {
        margin.positive = m_LowTempAdcMargin;
        margin.negative = m_NegLowTempAdcMargin ? m_NegLowTempAdcMargin : -m_LowTempAdcMargin;
    }
    return margin;
}

RC CheckAVFS::SetSkipAdcCalCheck(bool val)
{
    Printf(Tee::PriWarn, "SkipAdcCalCheck has been deprecated - use SkipRegkeyCheck instead.\n");
    m_SkipRegkeyCheck = val;
    return RC::OK;
}

bool CheckAVFS::GetSkipAdcCalCheck() const
{
    return m_SkipRegkeyCheck;
}

