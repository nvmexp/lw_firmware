/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/abort.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/pcie/pciemargin.h"
#include "pextest.h"

class PexMargin : public PexTest
{
public:
    PexMargin() = default;
    virtual ~PexMargin() = default;

    RC Setup() override;
    RC RunTest() override;
    RC Cleanup() override;
    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(TimingThreshold, UINT32);
    SETGET_PROP(VoltageThreshold, UINT32);
    SETGET_PROP(MaxErrors, UINT32);
    SETGET_PROP(VerifyMs, FLOAT64);

private:
    UINT32 m_TimingThreshold = 0;
    UINT32 m_VoltageThreshold = 0;
    UINT32 m_MaxErrors = 4;
    FLOAT64 m_VerifyMs = 100.0;
};

bool PexMargin::IsSupported()
{
    if (!PexTest::IsSupported())
        return false;

    if (!GetBoundTestDevice()->SupportsInterface<Pcie>() ||
        !GetBoundTestDevice()->SupportsInterface<PcieMargin>())
        return false;

    if (GetBoundTestDevice()->GetInterface<Pcie>()->GetLinkSpeed(Pci::Speed16000MBPS) < Pci::Speed16000MBPS)
        return false;

    return true;
}

RC PexMargin::Setup()
{
    RC rc;

    CHECK_RC(PexTest::Setup());

    PcieMargin* pPcieMargin = GetBoundTestDevice()->GetInterface<PcieMargin>();
    CHECK_RC(pPcieMargin->EnableMargining());

    PcieMargin::CtrlCaps caps;
    CHECK_RC(pPcieMargin->GetMarginCtrlCaps(0, &caps));
    if (m_VoltageThreshold > 0 && !caps.bVoltageSupported)
    {
        Printf(Tee::PriError, "VoltageThreshold greater than 0, but voltage stepping not supported\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return RC::OK;
}

RC PexMargin::RunTest()
{
    RC rc;
    Pcie* pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    PcieMargin* pPcieMargin = GetBoundTestDevice()->GetInterface<PcieMargin>();

    UINT32 origPexId = ~0U;
    PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    CHECK_RC(pPexDevMgr->SavePexSetting(&origPexId, PexDevMgr::REST_ASPM));
    auto RestorePexSettings = [&origPexId](TestDevicePtr pDev) -> RC
    {
        PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
        return pPexDevMgr->RestorePexSetting(origPexId, PexDevMgr::REST_ASPM, true, pDev);
    };
    DEFERNAME(restoreAspm)
    {
        RestorePexSettings(GetBoundTestDevice());
    };
    CHECK_RC(DisableAspm());

    DEFERNAME(resetPexErrors)
    {
        pPcie->ResetErrorCounters();
    };

    StickyRC resultRC;

    for (UINT32 lane = 0; lane < pPcie->GetLinkWidth(); lane++)
    {
        VerbosePrintf("Lane %d\n", lane);

        CHECK_RC(pPcieMargin->ClearMarginErrorLog(lane));
        CHECK_RC(pPcieMargin->SetMarginNormalSettings(lane));

        PcieMargin::CtrlCaps caps;
        CHECK_RC(pPcieMargin->GetMarginCtrlCaps(lane, &caps));

        UINT32 numTimingSteps = 0;
        CHECK_RC(pPcieMargin->GetMarginNumTimingSteps(lane, &numTimingSteps));

        CHECK_RC(pPcieMargin->SetMarginErrorCountLimit(lane, m_MaxErrors));

        vector<PcieMargin::TimingMarginDir> timingDirs = {PcieMargin::TMD_UNIDIR};
        if (caps.bIndLeftRightTiming)
        {
            timingDirs.clear();
            timingDirs.push_back(PcieMargin::TMD_RIGHT);
            timingDirs.push_back(PcieMargin::TMD_LEFT);
        }
        for (auto dir : timingDirs)
        {
            UINT32 lwrTimingStep;
            for (lwrTimingStep = 0; lwrTimingStep <= numTimingSteps; lwrTimingStep++)
            {
                CHECK_RC(Abort::Check());

                VerbosePrintf("Timing Offset = %d\n", lwrTimingStep);

                CHECK_RC(pPcieMargin->ClearMarginErrorLog(lane));

                bool bVerified = false;
                CHECK_RC(pPcieMargin->SetMarginTimingOffset(lane, dir, lwrTimingStep, m_VerifyMs, &bVerified));
                if (!bVerified)
                {
                    break;
                }
            }

            string laneMarginStr = "LINK UNSTABLE";
            if (lwrTimingStep > 0)
            {
                // Current timing step is the first step where there were errors greater than
                // the desired count.  Per the pex group, MODS should report the last offset
                // where errors *did not* exceed the threshold
                laneMarginStr = Utility::StrPrintf("%u", lwrTimingStep - 1);
            }
            // MAX is (numTimingSteps + 1) which indicates that there were no error counts
            // higher than the threshold at any timing offset
            Printf(Tee::PriNormal, "Lane %d %sTiming Offset = %s%s\n",
                    lane,
                   (dir == PcieMargin::TMD_UNIDIR) ? ""
                       : Utility::StrPrintf("%s", (dir==PcieMargin::TMD_RIGHT)?"Right ":"Left ").c_str(),
                   laneMarginStr.c_str(),
                   (lwrTimingStep == (numTimingSteps + 1))?" MAX":"");

            if (lwrTimingStep < m_TimingThreshold)
            {
                Printf(Tee::PriError, "Lane %d Timing Offset below Threshold = %d\n",
                       lane, m_TimingThreshold);
                resultRC = RC::UNEXPECTED_RESULT;
            }
        }

        CHECK_RC(pPcieMargin->ClearMarginErrorLog(lane));
        CHECK_RC(pPcieMargin->SetMarginNormalSettings(lane));

        if (caps.bVoltageSupported)
        {
            UINT32 numVoltageSteps = 0;
            CHECK_RC(pPcieMargin->GetMarginNumVoltageSteps(lane, &numVoltageSteps));

            vector<PcieMargin::VoltageMarginDir> voltageDirs = {PcieMargin::VMD_UNIDIR};
            if (caps.bIndUpDowlwoltage)
            {
                voltageDirs.clear();
                voltageDirs.push_back(PcieMargin::VMD_UP);
                voltageDirs.push_back(PcieMargin::VMD_DOWN);
            }
            for (auto dir : voltageDirs)
            {
                UINT32 lwrVoltageStep;
                for (lwrVoltageStep = 0; lwrVoltageStep <= numVoltageSteps; lwrVoltageStep++)
                {
                    CHECK_RC(Abort::Check());

                    VerbosePrintf("Voltage Offset = %d\n", lwrVoltageStep);

                    CHECK_RC(pPcieMargin->ClearMarginErrorLog(lane));

                    bool bVerified = false;
                    CHECK_RC(pPcieMargin->SetMargilwoltageOffset(lane, dir, lwrVoltageStep, m_VerifyMs, &bVerified));
                    if (!bVerified)
                    {
                        // This was the first iteration to fail after increasing the steps, we're done
                        break;
                    }
                }

                string laneMarginStr = "LINK UNSTABLE";
                if (lwrVoltageStep > 0)
                {
                    // Current timing step is the first step where there were errors greater than
                    // the desired count.  Per the pex group, MODS should report the last offset
                    // where errors *did not* exceed the threshold
                    laneMarginStr = Utility::StrPrintf("%u", lwrVoltageStep - 1);
                }
                // MAX is (numVoltageSteps + 1) which indicates that there were no error counts
                // higher than the threshold at any voltage offset
                Printf(Tee::PriNormal, "Lane %d %sVoltage Offset = %s%s\n",
                        lane,
                       (dir == PcieMargin::VMD_UNIDIR) ? ""
                           : Utility::StrPrintf("%s", (dir==PcieMargin::VMD_UP)?"Up ":"Down ").c_str(),
                       laneMarginStr.c_str(),
                       (lwrVoltageStep == (numVoltageSteps + 1))?" MAX":"");

                if (lwrVoltageStep < m_VoltageThreshold)
                {
                    Printf(Tee::PriError, "Lane %d Voltage Offset below Threshold = %d\n",
                           lane, m_VoltageThreshold);
                    resultRC = RC::UNEXPECTED_RESULT;
                }
            }

            CHECK_RC(pPcieMargin->ClearMarginErrorLog(lane));
            CHECK_RC(pPcieMargin->SetMarginNormalSettings(lane));
        }
    }

    CHECK_RC(resultRC);

    resetPexErrors.Cancel();
    CHECK_RC(pPcie->ResetErrorCounters());

    return RC::OK;
}

RC PexMargin::Cleanup()
{
    StickyRC rc;
    PcieMargin* pPcieMargin = GetBoundTestDevice()->GetInterface<PcieMargin>();

    for (UINT32 lane = 0; lane < GetBoundTestDevice()->GetInterface<Pcie>()->GetLinkWidth(); lane++)
    {
        rc = pPcieMargin->ClearMarginErrorLog(lane);
        rc = pPcieMargin->SetMarginNormalSettings(lane);
    }

    rc = pPcieMargin->DisableMargining();

    rc = PexTest::Cleanup();

    return rc;
}

void PexMargin::PrintJsProperties(Tee::Priority pri)
{
    PexTest::PrintJsProperties(pri);

    Printf(pri, "PexMargin Js Properties:\n");
    Printf(pri, "\tTimingThreshold: %u\n", m_TimingThreshold);
    Printf(pri, "\tVoltageThreshold: %u\n", m_VoltageThreshold);
    Printf(pri, "\tMaxErrors: %u\n", m_MaxErrors);
    Printf(pri, "\tVerifyMs:  %f\n", m_VerifyMs);
}

JS_CLASS_INHERIT(PexMargin, PexTest, "Pcie margin test");
CLASS_PROP_READWRITE(PexMargin, TimingThreshold, UINT32, "The timing offset for a particular lane must be greater than or equal to this threshold");
CLASS_PROP_READWRITE(PexMargin, VoltageThreshold, UINT32, "The voltage offset for a particular lane must be greater than or equal to this threshold");
CLASS_PROP_READWRITE(PexMargin, MaxErrors, UINT32, "Maximum number of errors before declaring an offset has failed");
CLASS_PROP_READWRITE(PexMargin, VerifyMs, FLOAT64, "The length of time in milliseconds to wait for errors to accumulate before declaring an offset has passed");
