/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <limits>
#include "core/include/jsonlog.h"
#include "core/include/script.h"
#include "core/include/version.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlinkuphy.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/lwlink/lwlink_mle_colw.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "mods_reg_hal.h"
#include "lwlinktest.h"

class LwLinkEyeDiagram : public LwLinkTest
{
public:
    LwLinkEyeDiagram();
    virtual ~LwLinkEyeDiagram() {}

    bool IsSupported() override;
    RC Setup() override;
    RC RunTest() override;
    RC InitFromJs();
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(PrintPerLoop,         bool);
    SETGET_PROP(TestTypeMask,         UINT32);
    SETGET_PROP(KeepRunning,          bool);
    SETGET_PROP(LoopDelayMs,          FLOAT64);
    SETGET_PROP(EnableUphyLogging,    bool);
    SETGET_PROP(LogUphyOnLoopFailure, bool);
    SETGET_PROP(LogUphyOnTestFailure, bool);
    SETGET_PROP(LogUphyOnTest,        bool);
    SETGET_PROP(LogUphyOnLoop,        UINT32);
    SETGET_PROP(PrintMarginalWarning, bool);
    SETGET_PROP(SkipEomCheck,         bool);
    SETGET_PROP(NumRetriesOnZeroEom,  UINT32);

    typedef vector<INT32> LinkArray;
    typedef vector<UINT32> LinkArrayU;
    typedef vector<FLOAT32> LinkArrayF;
private:
    enum LinkType
    {
        LT_ACCESS = 0x1,
        LT_TRUNK  = 0x2
    };

    UINT32             m_NumLoops       = 100;
    bool               m_PrintPerLoop   = false;
    vector<LinkArrayF> m_XPassThresholds;
    vector<LinkArrayF> m_XFailThresholds;
    vector<LinkArrayF> m_YPassThresholds;
    vector<LinkArrayF> m_YFailThresholds;
    vector<LinkArrayF> m_Pam4YPassThresholds;
    vector<LinkArrayF> m_Pam4YFailThresholds;
    LinkArrayU         m_BerNumErrors;
    LinkArrayU         m_BerNumBlocks;
    UINT32             m_TestTypeMask         = (LT_ACCESS | LT_TRUNK);
    bool               m_KeepRunning          = false;
    FLOAT64            m_LoopDelayMs          = 0.0;
    bool               m_EnableUphyLogging    = false;
    bool               m_LogUphyOnLoopFailure = false;
    bool               m_LogUphyOnTestFailure = false;
    bool               m_LogUphyOnTest        = false;
    UINT32             m_LogUphyOnLoop        = ~0U;
    bool               m_PrintMarginalWarning = false;
    bool               m_SkipEomCheck         = false;
    UINT32             m_NumRetriesOnZeroEom  = 0;

    static constexpr FLOAT64 MIN_LOOP_DELAY = 250.0;
    static constexpr FLOAT64 WARN_LOOP_DELAY = 1000.0;

    UINT32 GetMaxLanes();
    RC InitPerLaneJsArray(const char * jsPropName, vector<LinkArrayF> *pThresholds);
    RC InitPerLinkJsArray(const char * jsPropName, LinkArrayU *pThresholds);
    void PrintPerLaneJsArray
    (
        Tee::Priority pri,
        const char *  pFormattedName,
        const char *  pPrintSpec,
        vector<LinkArrayF> & perLaneData
    );
    void PrintPerLinkJsArray
    (
        Tee::Priority pri,
        const char *  pFormattedName,
        const char *  pPrintSpec,
        LinkArrayU &  perLinkData
    );

    bool ShouldTestLink(UINT32 linkId);

    class StatusVals
    {
    public:
        explicit StatusVals(TestDevicePtr pTestDevice,
                            LwLink::FomMode mode,
                            const vector<LinkArrayF> & passThresholds,
                            const vector<LinkArrayF> & failThresholds,
                            const vector<LinkArrayF> & pam4PassThresholds,
                            const vector<LinkArrayF> & pam4FailThresholds,
                            UINT64 pam4LinkMask,
                            UINT32 numLinks, UINT32 numLanes,
                            bool forcePrintAvg,
                            bool requirePam4,
                            bool verbose);
        RC AddVals(const LinkArray& vals, UINT32 link);
        RC CheckVals(const LinkArray& vals, UINT32 link, bool skipEomCheck) const;

        RC CheckMilwals(UINT32 link, bool skipEomCheck) const;
        RC CheckAvgVals(UINT32 link, bool printMarginalWarn, bool skipEomCheck) const;
        const vector<LinkArrayF> & GetAvgVals() const { return m_AvgVals; }

        void PrintSummary(Tee::Priority pri, UINT32 link) const;
        template<typename T>
        void PrintEomStatus(Tee::Priority pri,
                            const vector<T>& status,
                            UINT32 link, string index,
                            bool printLinkMinMax) const;

        LwLink::FomMode GetMode() const { return m_Mode; }
        bool GetForcePrintAvg() const { return m_ForcePrintAvg; }
        bool GetRequirePam4() const { return m_RequirePam4; }

    private:
        TestDevicePtr       m_pTestDevice;
        LwLink::FomMode     m_Mode;
        vector<LinkArrayF>  m_PassThresholds;
        vector<LinkArrayF>  m_FailThresholds;
        vector<LinkArrayF>  m_Pam4PassThresholds;
        vector<LinkArrayF>  m_Pam4FailThresholds;
        UINT64              m_Pam4LinkMask;
        UINT32              m_NumLinks;
        UINT32              m_NumLanes;
        bool                m_ForcePrintAvg;
        bool                m_RequirePam4;
        bool                m_Verbose;

        vector<LinkArray>   m_Milwals;
        vector<LinkArray>   m_MaxVals;
        vector<LinkArrayF>  m_VarVals;
        vector<LinkArrayF>  m_AvgVals;
        vector<UINT32>      m_NumVals;

        LinkArrayF GetStdVals(UINT32 link) const;

        string GetModeStr() const;
    };
};

LwLinkEyeDiagram::LwLinkEyeDiagram()
{
    SetName("LwLinkEyeDiagram");
}

bool LwLinkEyeDiagram::IsSupported()
{
    if (!GetBoundTestDevice()->SupportsInterface<LwLink>())
        return false;

    auto pLwLink = GetBoundTestDevice()->GetInterface<LwLink>();
    bool oneActiveLink = false;
    for (UINT32 i = 0; i < pLwLink->GetMaxLinks(); i++)
    {
        if (pLwLink->IsLinkActive(i))
        {
            oneActiveLink = true;
            break;
        }
    }
    if (!oneActiveLink)
        return false;

    if (m_TestTypeMask == LT_TRUNK
        && GetBoundTestDevice()->GetType() != TestDevice::TYPE_LWIDIA_LWSWITCH)
    {
        return false;
    }

    return LwLinkTest::IsSupported();
}

RC LwLinkEyeDiagram::Setup()
{
    RC rc;

    CHECK_RC(LwLinkTest::Setup());

    UphyIf::Version uphyVer = UphyIf::Version::UNKNOWN;
    if (GetBoundTestDevice()->SupportsInterface<LwLinkUphy>())
        uphyVer = GetBoundTestDevice()->GetInterface<LwLinkUphy>()->GetVersion();

    // For UPHY version 6.1 and later need to enforce a loop delay as per the UPHY team
    // MODS shouldnt be performing the UPHY procedure on a link more than once per
    // second
    const bool bForceLoopDelay =
        (uphyVer != UphyIf::Version::UNKNOWN) &&
        (static_cast<UINT32>(uphyVer) >= static_cast<UINT32>(UphyIf::Version::V61));
    if (bForceLoopDelay)
    {
        if (m_LoopDelayMs < MIN_LOOP_DELAY)
        {
            Printf(Tee::PriError,
                   "%s : LoopDelayMs %.1f is below the minimum of %f\n", GetName().c_str(),
                   m_LoopDelayMs, MIN_LOOP_DELAY);
            return RC::BAD_PARAMETER;
        }
        else if (m_LoopDelayMs < WARN_LOOP_DELAY)
        {
            Printf(Tee::PriWarn,
                   "%s : Setting LoopDelayMs below %.1f can skew results and increase the "
                   "possibility of failure\n", GetName().c_str(), WARN_LOOP_DELAY);
        }
    }

    m_NumLoops = GetTestConfiguration()->Loops();
    if (m_NumLoops == 0)
    {
        Printf(Tee::PriError, "Loops cannot be zero\n");
        return RC::ILWALID_ARGUMENT;
    }

    auto ValidateThresholds = [] (const vector<LinkArrayF> & passThresh,
                                  const vector<LinkArrayF> & failThresh,
                                  const char * threshType) -> RC
    {
        // This is guaranteed in InitFromJs
        MASSERT(failThresh.size() == passThresh.size());
        for (size_t linkId = 0; linkId < failThresh.size(); linkId++)
        {
            // This is guaranteed in InitFromJs
            MASSERT(failThresh[linkId].size() == passThresh[linkId].size());
            for (size_t lane = 0; lane < failThresh[linkId].size(); lane++)
            {
                if (failThresh[linkId][lane] > passThresh[linkId][lane])
                {
                    Printf(Tee::PriError,
                           "%sFailThreshold cannot be greater than %sPassThreshold\n",
                           threshType, threshType);
                    return RC::ILWALID_ARGUMENT;
                }
            }
        }
        return RC::OK;
    };
    CHECK_RC(ValidateThresholds(m_XPassThresholds, m_XFailThresholds, "X"));
    CHECK_RC(ValidateThresholds(m_YPassThresholds, m_YFailThresholds, "Y"));
    CHECK_RC(ValidateThresholds(m_Pam4YPassThresholds, m_Pam4YFailThresholds, "Pam4Y"));

    // This is guaranteed in InitFromJs
    MASSERT(m_BerNumErrors.size() == m_BerNumBlocks.size());

    if (m_TestTypeMask == 0x0)
    {
        Printf(Tee::PriError, "TestTypeMask cannot be 0. Must specify at least one type.\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        SetVerbosePrintPri(Tee::PriNone);
    }

    return rc;
}

RC LwLinkEyeDiagram::RunTest()
{
    RC rc;
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    auto pLwLink = pTestDevice->GetInterface<LwLink>();

    const bool verbose = GetTestConfiguration()->Verbose();

    const FLOAT64 timeoutMs = GetTestConfiguration()->TimeoutMs();
    const UINT32 numLinks = pLwLink->GetMaxLinks();
    UINT32 linkWidth = 0;
    vector<LwLink::EomSettings> eomSettings;
    for (UINT32 link = 0; link < numLinks; link++)
    {
        eomSettings.push_back({ m_BerNumBlocks[link], m_BerNumErrors[link] });
        if (pLwLink->IsLinkValid(link))
        {
            if (pLwLink->GetSublinkWidth(link) > linkWidth)
                linkWidth = pLwLink->GetSublinkWidth(link);
            if (((m_BerNumBlocks[link] == 0) || (m_BerNumErrors[link] == 0)) &&
                pLwLink->IsLinkActive(link))
            {
                LwLink::EomSettings settings(m_BerNumBlocks[link], m_BerNumErrors[link]);
                pLwLink->GetEomDefaultSettings(link, &settings);
                if (m_BerNumBlocks[link] == 0)
                    eomSettings[link].numBlocks = settings.numBlocks;
                if (m_BerNumErrors[link] == 0)
                    eomSettings[link].numErrors = settings.numErrors;
            }
        }
    }
    MASSERT(linkWidth);

    vector<LwLink::LinkStatus> origLinkStatus;
    CHECK_RC(pLwLink->GetLinkStatus(&origLinkStatus));
    LwLinkPowerState::Owner psOwner(pTestDevice->GetInterface<LwLinkPowerState>());
    if (pTestDevice->SupportsInterface<LwLinkPowerState>())
    {
        CHECK_RC(psOwner.Claim(LwLinkPowerState::ClaimState::FULL_BANDWIDTH));
    }

    auto restoreLinkStatus = [&]() -> RC
    {
        return psOwner.Release();
    };

    DEFERNAME(deferRestoreLinkStatus)
    {
        restoreLinkStatus();
    };

    UINT64 pam4LinkMask = 0;
    for (UINT32 link = 0; link < numLinks; link++)
    {
        if (!pLwLink->IsLinkActive(link) || !pTestDevice->SupportsInterface<LwLinkPowerState>())
            continue;
        auto powerState = pTestDevice->GetInterface<LwLinkPowerState>();
        Tasker::PollHw(timeoutMs, [&]() -> bool
        {
            LwLinkPowerState::LinkPowerStateStatus ps;
            powerState->GetLinkPowerStateStatus(link, &ps);
            return ps.rxSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB &&
                   ps.txSubLinkLwrrentPowerState == LwLinkPowerState::SLS_PWRM_FB;
        }, __FUNCTION__);

        if (ShouldTestLink(link) && pLwLink->IsLinkPam4(link))
            pam4LinkMask |= 1ULL << link;
    }

    vector<StatusVals> statusValsArray;

#define ADD_CONFIG(mode, pass, fail, pam4Pass, pam4Fail, printAvg, requiresPam4, internalOnly)    \
    if (pLwLink->SupportsFomMode(LwLink::mode)                                                    \
        && (!internalOnly || (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))) \
        statusValsArray.push_back(StatusVals(pTestDevice, LwLink::mode,                           \
                                             pass, fail, pam4Pass, pam4Fail, pam4LinkMask,        \
                                             numLinks, linkWidth, printAvg, requiresPam4,         \
                                             verbose));

    const UINT32 maxLanes = GetMaxLanes();
    vector<LinkArrayF> _FLOAT32_MIN_ARR(numLinks,
                                        LinkArrayF(maxLanes, numeric_limits<FLOAT32>::lowest()));

    if (verbose)
    {
        ADD_CONFIG(EYEO_XL_O, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_XH_O, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_XL_E, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_XH_E, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_YL_O, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_YH_O, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_YL_E, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_YH_E, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);

        ADD_CONFIG(EYEO_Y_U,  _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
        ADD_CONFIG(EYEO_Y_M,  _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, true,  true);
        ADD_CONFIG(EYEO_Y_L,  _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR,  _FLOAT32_MIN_ARR, false, false, true);
    }

    ADD_CONFIG(EYEO_X, m_XPassThresholds, m_XFailThresholds, _FLOAT32_MIN_ARR, _FLOAT32_MIN_ARR, true, false, false);
    ADD_CONFIG(EYEO_Y, m_YPassThresholds, m_YFailThresholds, m_Pam4YPassThresholds, m_Pam4YFailThresholds, true, false, false);

    if (m_KeepRunning)
        m_NumLoops = 0;

    const bool bLogUphyOnFailure =
        (m_EnableUphyLogging || m_LogUphyOnLoopFailure || m_LogUphyOnTestFailure || m_LogUphyOnTest) &&
        (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM);
    const bool bLogUphy = m_EnableUphyLogging &&
                         (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM);
    map<LwLink::FomMode, vector<LinkArray>> eomValues;
    for (UINT32 loop = 0; loop < m_NumLoops || m_KeepRunning; loop++)
    {
        eomValues.clear();
        bool bLogUphyLoopFailure = false;
        auto LogUphyPerEom = [&] () -> RC
        {
            if (bLogUphy || ((rc != RC::OK) && bLogUphyOnFailure)||
                bLogUphyLoopFailure || (m_LogUphyOnLoop == loop))
            {
                return UphyRegLogger::LogRegs(pTestDevice,
                                              UphyRegLogger::UphyInterface::LwLink,
                                              UphyRegLogger::LogPoint::EOM,
                                              rc,
                                              eomValues);
            }
            return RC::OK;
        };
        DEFERNAME(logUphy) { LogUphyPerEom(); };
        for (size_t idx = 0; idx < statusValsArray.size(); ++idx)
        {
            StatusVals& status = statusValsArray[idx];
            for (UINT32 link = 0; link < numLinks; link++)
            {
                if (!ShouldTestLink(link) ||
                    (status.GetRequirePam4() && !pLwLink->IsLinkPam4(link)))
                {
                    continue;
                }

                UINT32 numOfRetries = m_NumRetriesOnZeroEom;
                LinkArray eomStatus(linkWidth, 0);
                do
                {
                    INT32 milwal = _INT32_MAX;
                    memset(&eomStatus[0], 0, eomStatus.size() * sizeof(eomStatus[0]));
                    CHECK_RC(pLwLink->GetEomStatus(status.GetMode(),
                                                   link,
                                                   eomSettings[link].numErrors,
                                                   eomSettings[link].numBlocks,
                                                   timeoutMs,
                                                   &eomStatus));

                    for (UINT32 lane = 0; lane < linkWidth; lane++)
                    {
                        if (eomStatus[lane] < milwal)
                            milwal = eomStatus[lane];
                    }

                    if (milwal != 0)
                        break;
                } while(numOfRetries--);

                if (m_PrintPerLoop)
                {
                    Tee::Priority pri = (status.GetForcePrintAvg()) ? Tee::PriNormal : GetVerbosePrintPri();
                    status.PrintEomStatus(pri, eomStatus, link, Utility::StrPrintf("(%d)",loop), true);
                }

                if (bLogUphyOnFailure || (m_LogUphyOnLoop == loop))
                {
                    LwLink::FomMode fomMode = status.GetMode();
                    if (!eomValues.count(fomMode))
                        eomValues[fomMode] = vector<vector<INT32>>(numLinks);
                    eomValues[fomMode][link] = eomStatus;
                }

                CHECK_RC(status.AddVals(eomStatus, link));
                CHECK_RC(status.CheckMilwals(link, m_SkipEomCheck));
                if ((UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM) &&
                    m_LogUphyOnLoopFailure)
                {
                    if (status.CheckVals(eomStatus, link, m_SkipEomCheck) != RC::OK)
                        bLogUphyLoopFailure = true;
                }
            }
            if (m_KeepRunning ||
                (idx != (statusValsArray.size() - 1)) ||
                (loop != (m_NumLoops - 1)))
            {
                Tasker::Sleep(m_LoopDelayMs);
            }
        }
        logUphy.Cancel();
        CHECK_RC(LogUphyPerEom());
    }

    StickyRC checkAvgRC;
    bool linksTested = false;
    for (UINT32 link = 0; link < numLinks; link++)
    {
        if (!ShouldTestLink(link))
            continue;

        linksTested = true;

        for (const StatusVals& status : statusValsArray)
        {
            if (status.GetRequirePam4() && !pLwLink->IsLinkPam4(link))
                continue;

            status.PrintSummary(GetVerbosePrintPri(), link);
            checkAvgRC = status.CheckAvgVals(link, m_PrintMarginalWarning, m_SkipEomCheck);
        }
    }
    if (!linksTested)
    {
        Printf(Tee::PriError, "No LwLinks were tested. Check your configuration.\n");
        CHECK_RC(RC::NO_TESTS_RUN);
    }

    if ((UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM) &&
        ((m_LogUphyOnTestFailure && (checkAvgRC != RC::OK)) || m_LogUphyOnTest))
    {
        eomValues.clear();
        for (const StatusVals& status : statusValsArray)
        {
            vector<LinkArrayF> avgEomValues = status.GetAvgVals();
            if (!avgEomValues.empty())
            {
                eomValues[status.GetMode()] = vector<vector<INT32>>(avgEomValues.size());
                for (size_t lwrLink = 0; lwrLink < avgEomValues.size(); lwrLink++)
                {
                    eomValues[status.GetMode()][lwrLink].resize(avgEomValues[lwrLink].size());
                    transform(avgEomValues[lwrLink].begin(), avgEomValues[lwrLink].end(),
                              eomValues[status.GetMode()][lwrLink].begin(),
                              [] (const FLOAT32 f) -> INT32 { return static_cast<INT32>(f); });
                }
            }
        }
        CHECK_RC(UphyRegLogger::LogRegs(pTestDevice,
                                        UphyRegLogger::UphyInterface::LwLink,
                                        UphyRegLogger::LogPoint::EOM,
                                        checkAvgRC,
                                        eomValues));
    }

    deferRestoreLinkStatus.Cancel();
    CHECK_RC(restoreLinkStatus());

    return checkAvgRC;
}

RC LwLinkEyeDiagram::InitFromJs()
{
    RC rc;
    CHECK_RC(GpuTest::InitFromJs());
    CHECK_RC(InitPerLaneJsArray("XPassThreshold",     &m_XPassThresholds));
    CHECK_RC(InitPerLaneJsArray("XFailThreshold",     &m_XFailThresholds));
    CHECK_RC(InitPerLaneJsArray("YPassThreshold",     &m_YPassThresholds));
    CHECK_RC(InitPerLaneJsArray("YFailThreshold",     &m_YFailThresholds));
    CHECK_RC(InitPerLaneJsArray("Pam4YPassThreshold", &m_Pam4YPassThresholds));
    CHECK_RC(InitPerLaneJsArray("Pam4YFailThreshold", &m_Pam4YFailThresholds));
    CHECK_RC(InitPerLinkJsArray("BerNumErrors",       &m_BerNumErrors));
    CHECK_RC(InitPerLinkJsArray("BerNumBlocks",       &m_BerNumBlocks));
    return rc;
}

UINT32 LwLinkEyeDiagram::GetMaxLanes()
{
    auto pLwLink = GetBoundTestDevice()->GetInterface<LwLink>();
    UINT32 maxLanes = 0;
    const UINT32 maxLinks = pLwLink->GetMaxLinks();
    for (UINT32 linkId = 0; linkId < maxLinks; linkId++)
    {
        if (pLwLink->IsLinkValid(linkId))
        {
            maxLanes = max(maxLanes, pLwLink->GetSublinkWidth(linkId));
        }
    }
    return maxLanes;
}

RC LwLinkEyeDiagram::InitPerLaneJsArray(const char * jsPropName, vector<LinkArrayF> *pPerLinkData)
{
    RC rc;
    JavaScriptPtr pJs;
    jsval jsvPerLinkData;
    rc = pJs->GetPropertyJsval(GetJSObject(), jsPropName, &jsvPerLinkData);
    auto pLwLink = GetBoundTestDevice()->GetInterface<LwLink>();
    const UINT32 maxLinks = pLwLink->GetMaxLinks();

    pPerLinkData->clear();
    pPerLinkData->resize(maxLinks);

    const UINT32 maxLanes = GetMaxLanes();
    if (RC::OK == rc)
    {
        if (JSVAL_IS_NUMBER(jsvPerLinkData))
        {
            FLOAT32 allLinkData;
            CHECK_RC(pJs->FromJsval(jsvPerLinkData, &allLinkData));
            for (UINT32 i = 0; i < maxLinks; i++)
            {
                pPerLinkData->at(i).assign(maxLanes, allLinkData);
            }
        }
        else if (JSVAL_IS_OBJECT(jsvPerLinkData))
        {
            JsArray jsaPerLinkData;
            CHECK_RC(pJs->FromJsval(jsvPerLinkData, &jsaPerLinkData));

            if (jsaPerLinkData.size() != maxLinks)
            {
                Printf(Tee::PriError,
                    "%s : Invalid number of per-link entries for %s, expected %u, actual %u\n",
                    GetName().c_str(), jsPropName, maxLinks,
                    static_cast<UINT32>(jsaPerLinkData.size()));
                return RC::ILWALID_ARGUMENT;
            }

            for (size_t linkId = 0; linkId < jsaPerLinkData.size(); ++linkId)
            {
                FLOAT32 laneData;
                if (JSVAL_IS_NUMBER(jsaPerLinkData[linkId]))
                {
                    CHECK_RC(pJs->FromJsval(jsaPerLinkData[linkId], &laneData));
                    pPerLinkData->at(linkId).assign(maxLanes, laneData);
                }
                else if (JSVAL_IS_OBJECT(jsaPerLinkData[linkId]))
                {
                    JsArray jsaPerLaneData;
                    CHECK_RC(pJs->FromJsval(jsaPerLinkData[linkId], &jsaPerLaneData));

                    if (jsaPerLaneData.size() != maxLanes)
                    {
                        Printf(Tee::PriError,
                               "%s : Invalid number of per-lane entries for %s link %u, "
                               "expected %u, actual %u\n",
                               GetName().c_str(), jsPropName, static_cast<UINT32>(linkId),
                               maxLanes, static_cast<UINT32>(jsaPerLaneData.size()));
                        return RC::ILWALID_ARGUMENT;
                    }
                    for (size_t lane = 0; lane < jsaPerLaneData.size(); ++lane)
                    {
                        CHECK_RC(pJs->FromJsval(jsaPerLaneData[lane], &laneData));
                        pPerLinkData->at(linkId).push_back(laneData);
                    }
                }
                else
                {
                    Printf(Tee::PriError,
                           "%s : Invalid JS specification of %s",
                           GetName().c_str(),
                           jsPropName);
                    return RC::ILWALID_ARGUMENT;
                }
            }
        }
        else
        {
            Printf(Tee::PriError,
                   "%s : Invalid JS specification of %s",
                   GetName().c_str(),
                   jsPropName);
            return RC::ILWALID_ARGUMENT;
        }
    }
    else if (RC::ILWALID_OBJECT_PROPERTY == rc)
    {
        rc.Clear();
        for (UINT32 i = 0; i < maxLinks; i++)
        {
            pPerLinkData->at(i).assign(maxLanes, 0.0);
        }
    }
    return rc;
}

RC LwLinkEyeDiagram::InitPerLinkJsArray(const char * jsPropName, LinkArrayU *pPerLinkData)
{
    RC rc;
    JavaScriptPtr pJs;
    jsval jsvPerLinkData;
    rc = pJs->GetPropertyJsval(GetJSObject(), jsPropName, &jsvPerLinkData);
    auto pLwLink = GetBoundTestDevice()->GetInterface<LwLink>();
    const UINT32 maxLinks = pLwLink->GetMaxLinks();
    if (RC::OK == rc)
    {
        if (JSVAL_IS_NUMBER(jsvPerLinkData))
        {
            UINT32 allLinkData;
            CHECK_RC(pJs->FromJsval(jsvPerLinkData, &allLinkData));
            pPerLinkData->assign(maxLinks, allLinkData);
        }
        else if (JSVAL_IS_OBJECT(jsvPerLinkData))
        {
            JsArray jsaPerLinkData;
            CHECK_RC(pJs->FromJsval(jsvPerLinkData, &jsaPerLinkData));

            if (jsaPerLinkData.size() != maxLinks)
            {
                Printf(Tee::PriError,
                    "%s : Invalid number of per-link entries for %s, expected %u, actual %u\n",
                    GetName().c_str(), jsPropName, maxLinks,
                    static_cast<UINT32>(jsaPerLinkData.size()));
                return RC::ILWALID_ARGUMENT;
            }

            pPerLinkData->clear();
            for (size_t i = 0; i < jsaPerLinkData.size(); ++i)
            {
                UINT32 linkData;
                CHECK_RC(pJs->FromJsval(jsaPerLinkData[i], &linkData));
                pPerLinkData->push_back(linkData);
            }
        }
        else
        {
            Printf(Tee::PriError,
                   "%s : Invalid JS specification of %s",
                   GetName().c_str(),
                   jsPropName);
            return RC::ILWALID_ARGUMENT;
        }
    }
    else if (RC::ILWALID_OBJECT_PROPERTY == rc)
    {
        rc.Clear();
        pPerLinkData->assign(maxLinks, 0);
    }
    return rc;
}

void LwLinkEyeDiagram::PrintPerLaneJsArray
(
    Tee::Priority pri,
    const char * pFormattedName,
    const char * pPrintSpec,
    vector<LinkArrayF> &perLaneData
)
{
    string perLaneStr = Utility::StrPrintf("%s[", pFormattedName);
    const string printSpec = "%s " + string(pPrintSpec);
    const string newLinePrefix = "\n" + string(strlen(pFormattedName) + 1, ' ');
    // 8 lanes is reasonable to display on a single line
    const UINT32 linksPerLine = (GetMaxLanes() > 8) ? 1 : 8 / GetMaxLanes();

    for (size_t linkId = 0; linkId < perLaneData.size(); ++linkId)
    {
        perLaneStr += Utility::StrPrintf("%s%s [",
            (linkId == 0) ? "" : ",",
            ((linkId != 0) && ((linkId % linksPerLine) == 0)) ? newLinePrefix.c_str() : "");
        for (size_t lane = 0; lane < perLaneData[linkId].size(); ++lane)
        {
            perLaneStr += Utility::StrPrintf(printSpec.c_str(),
                                             (lane == 0) ? "" : ",",
                                             perLaneData[linkId][lane]);
        }
        perLaneStr += " ]";
    }
    Printf(pri, "%s ]\n", perLaneStr.c_str());
}

void LwLinkEyeDiagram::PrintPerLinkJsArray
(
    Tee::Priority pri,
    const char * pFormattedName,
    const char * pPrintSpec,
    LinkArrayU &perLinkData
)
{
    string perLinkStr = Utility::StrPrintf("%s[", pFormattedName);
    const string printSpec = "%s%s " + string(pPrintSpec);
    const string newLinePrefix = "\n" + string(strlen(pFormattedName) + 1, ' ');
    constexpr UINT32 linksPerLine = 16;
    for (size_t ii = 0; ii < perLinkData.size(); ++ii)
    {
        perLinkStr += Utility::StrPrintf(printSpec.c_str(),
            (ii == 0) ? "" : ",",
            ((ii != 0) && ((ii % linksPerLine) == 0)) ? newLinePrefix.c_str() : "",
            perLinkData[ii]);
    }
    Printf(pri, "%s ]\n", perLinkStr.c_str());
}

bool LwLinkEyeDiagram::ShouldTestLink(UINT32 linkId)
{
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    auto pLwLink = pTestDevice->GetInterface<LwLink>();

    if (!pLwLink->IsLinkActive(linkId))
        return false;

    LwLink::EndpointInfo endpoint;
    if (OK != pLwLink->GetRemoteEndpoint(linkId, &endpoint))
        return false;

    LinkType lt =  (pTestDevice->GetType() == TestDevice::TYPE_LWIDIA_LWSWITCH &&
                    endpoint.devType == TestDevice::TYPE_LWIDIA_LWSWITCH)
                   ? LT_TRUNK : LT_ACCESS;

    return !!(m_TestTypeMask & lt);
}

LwLinkEyeDiagram::StatusVals::StatusVals(TestDevicePtr pTestDevice,
                                         LwLink::FomMode mode,
                                         const vector<LinkArrayF> & passThresholds,
                                         const vector<LinkArrayF> & failThresholds,
                                         const vector<LinkArrayF> & pam4PassThresholds,
                                         const vector<LinkArrayF> & pam4FailThresholds,
                                         UINT64 pam4LinkMask,
                                         UINT32 numLinks, UINT32 numLanes,
                                         bool forcePrintAvg,
                                         bool requirePam4,
                                         bool verbose)
: m_pTestDevice(pTestDevice)
, m_Mode(mode)
, m_PassThresholds(passThresholds)
, m_FailThresholds(failThresholds)
, m_Pam4PassThresholds(pam4PassThresholds)
, m_Pam4FailThresholds(pam4FailThresholds)
, m_Pam4LinkMask(pam4LinkMask)
, m_NumLinks(numLinks)
, m_NumLanes(numLanes)
, m_ForcePrintAvg(forcePrintAvg)
, m_RequirePam4(requirePam4)
, m_Verbose(verbose)
{
    m_Milwals.resize(numLinks, LinkArray(numLanes, _INT32_MAX));
    m_MaxVals.resize(numLinks, LinkArray(numLanes, _INT32_MIN));
    m_VarVals.resize(numLinks, LinkArrayF(numLanes, 0.0));
    m_AvgVals.resize(numLinks, LinkArrayF(numLanes, 0.0));
    m_NumVals.resize(numLinks);
}

RC LwLinkEyeDiagram::StatusVals::AddVals(const LinkArray& vals, UINT32 link)
{
    RC rc;

    for (UINT32 lane = 0; lane < m_NumLanes; lane++)
    {
        if (vals[lane] < m_Milwals[link][lane])
            m_Milwals[link][lane] = vals[lane];
        if (vals[lane] > m_MaxVals[link][lane])
            m_MaxVals[link][lane] = vals[lane];

        if (m_NumVals[link] == 0)
        {
            m_VarVals[link][lane] = 0.0;
            m_AvgVals[link][lane] = vals[lane];
        }
        else
        {
            FLOAT32 valF = static_cast<FLOAT32>(vals[lane]);
            FLOAT32 p = (FLOAT32)(m_NumVals[link]) / (FLOAT32)(m_NumVals[link]+1);
            FLOAT32 q = 1.0 / (FLOAT32)(m_NumVals[link]+1);
            m_VarVals[link][lane] += (pow(valF - m_AvgVals[link][lane], 2)
                                      * p - m_VarVals[link][lane]) * q;
            m_AvgVals[link][lane] += (valF - m_AvgVals[link][lane]) * q;
        }
    }
    m_NumVals[link]++;

    return OK;
}

RC LwLinkEyeDiagram::StatusVals::CheckVals(const LinkArray& vals,
                                           UINT32 link,
                                           bool skipEomCheck) const
{
    const vector<LinkArrayF> & failThresholds =
        (m_Pam4LinkMask & (1ULL << link)) ? m_Pam4FailThresholds : m_FailThresholds;
    for (UINT32 lane = 0; lane < m_NumLanes; lane++)
    {
        if (vals[lane] <= failThresholds[link][lane])
        {
            Mle::Print(Mle::Entry::lwlink_eom_failure)
                .lwlink_id(link)
                .lane(lane)
                .fom_mode(Mle::ToEomFomMode(m_Mode))
                .threshold(failThresholds[link][lane])
                .eom_value(vals[lane]);
            return skipEomCheck ? RC::OK : RC::UNEXPECTED_RESULT;
        }
    }
    return RC::OK;
}

RC LwLinkEyeDiagram::StatusVals::CheckMilwals(UINT32 link, bool skipEomCheck) const
{
    StickyRC rc;
    for (UINT32 i = 0; i < m_Milwals[link].size(); i++)
    {
        INT32 val = m_Milwals[link][i];
        if (val == 0)
        {
            Printf(skipEomCheck ? Tee::PriWarn : Tee::PriError,
                   "%s : LWLink%d Physical Lane(%u) %s_STATUS = 0\n",
                   m_pTestDevice->GetName().c_str(),
                   link, i, GetModeStr().c_str());
            if (JsonLogStream::IsOpen())
            {
                JsonItem jsi;
                jsi.SetCategory(JsonItem::JSONLOG_LWLINKINFO);
                jsi.SetJsonLimitedAllowAllFields(true);
                jsi.SetTag("lwlink_unexpected_eye");
                jsi.SetField("device", m_pTestDevice->GetName().c_str());
                jsi.SetField("link", link);
                jsi.SetField("lane", i);
                jsi.SetField("mode", GetModeStr().c_str());
                jsi.SetField("actual_value", 0);
                jsi.WriteToLogfile();
            }
            Mle::Print(Mle::Entry::lwlink_eom_failure)
                .lwlink_id(link)
                .lane(i)
                .fom_mode(Mle::ToEomFomMode(m_Mode))
                .threshold(0.0f)
                .eom_value(val);
            rc = skipEomCheck ? RC::OK : RC::UNEXPECTED_RESULT;
        }
    }
    return rc;
}

RC LwLinkEyeDiagram::StatusVals::CheckAvgVals(UINT32 link,
                                              bool printMarginalWarn,
                                              bool skipEomCheck) const
{
    StickyRC rc;
    const LinkArrayF & passThresholds =
        (m_Pam4LinkMask & (1ULL << link)) ? m_Pam4PassThresholds[link] : m_PassThresholds[link];
    const LinkArrayF & failThresholds =
        (m_Pam4LinkMask & (1ULL << link)) ? m_Pam4FailThresholds[link] : m_FailThresholds[link];

    for (UINT32 i = 0; i < m_AvgVals[link].size(); i++)
    {
        FLOAT32 val = m_AvgVals[link][i];
        if (val < passThresholds[i])
        {
            if (val <= failThresholds[i])
            {
                Printf(skipEomCheck ? Tee::PriWarn : Tee::PriError,
                       "%s : LWLink%d Physical Lane(%u) %s_STATUS = %f is %s"
                       " Fail threshold = %f\n",
                       m_pTestDevice->GetName().c_str(),
                       link, i, GetModeStr().c_str(),
                       val, (val == failThresholds[i]) ? "at" : "below",
                       failThresholds[i]);
                if (JsonLogStream::IsOpen())
                {
                    JsonItem jsi;
                    jsi.SetCategory(JsonItem::JSONLOG_LWLINKINFO);
                    jsi.SetJsonLimitedAllowAllFields(true);
                    jsi.SetTag("lwlink_unexpected_eye");
                    jsi.SetField("device", m_pTestDevice->GetName().c_str());
                    jsi.SetField("link", link);
                    jsi.SetField("lane", i);
                    jsi.SetField("mode", GetModeStr().c_str());
                    jsi.SetField("actual_value", val);
                    jsi.SetField("fail_threshold", failThresholds[i]);
                    jsi.WriteToLogfile();
                }
                Mle::Print(Mle::Entry::lwlink_eom_failure)
                    .lwlink_id(link)
                    .lane(i)
                    .fom_mode(Mle::ToEomFomMode(m_Mode))
                    .threshold(failThresholds[i])
                    .eom_value(val);
                rc = skipEomCheck ? RC::OK : RC::UNEXPECTED_RESULT;
                continue;
            }

            if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) ||
                printMarginalWarn)
            {
                Printf(Tee::PriWarn,
                       "%s : LWLink%d Physical Lane(%u) could be potentially marginal. %s_STATUS"
                       "  = %f is above Fail threshold = %f but below Pass threshold = %f\n",
                       m_pTestDevice->GetName().c_str(),
                       link, i, GetModeStr().c_str(),
                       val, failThresholds[i], passThresholds[i]);
            }
        }
    }
    return rc;
}

void LwLinkEyeDiagram::StatusVals::PrintSummary(Tee::Priority pri, UINT32 link) const
{
    PrintEomStatus(pri, m_Milwals[link], link, "MIN", false);
    PrintEomStatus(pri, m_MaxVals[link], link, "MAX", false);
    PrintEomStatus(pri, GetStdVals(link), link, "STD", false);

    PrintEomStatus((m_ForcePrintAvg) ? Tee::PriNormal : pri,
                   m_AvgVals[link], link,
                   (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) ? "" : "AVG",
                   false);
}

template<typename T>
void LwLinkEyeDiagram::StatusVals::PrintEomStatus(Tee::Priority pri,
                                                  const vector<T>& status,
                                                  UINT32 link,
                                                  string index,
                                                  bool printLinkMinMax) const
{
    JavaScriptPtr pJs;
    JsArray vals;

    string statusLabel = Utility::StrPrintf("%s_STATUS",
                                            GetModeStr().c_str());
    string linkStatus = Utility::StrPrintf("%s : LWLink%d Physical Lane(0-%u) %12s %s%s= ",
                                            m_pTestDevice->GetName().c_str(),
                                            link,
                                            static_cast<UINT32>(status.size())-1,
                                            statusLabel.c_str(),
                                            index.c_str(),
                                            (index.length())?" ":"");

    T linkMin = numeric_limits<T>::max();
    T linkMax = numeric_limits<T>::min();
    for (UINT32 lane = 0; lane < status.size(); lane++)
    {
        const T& statusT = status[lane];
        if (statusT < linkMin)
            linkMin = statusT;
        if (statusT > linkMax)
            linkMax = statusT;

        jsval statusVal;
        pJs->ToJsval(statusT, &statusVal);
        vals.push_back(statusVal);

        linkStatus += Utility::StrPrintf("%*.*f", m_Verbose?5:3, m_Verbose?1:0,
                                         static_cast<FLOAT64>(statusT));
        if (lane < status.size() - 1)
        {
            linkStatus += ", ";
        }
    }
    if (printLinkMinMax)
    {
        linkStatus += Utility::StrPrintf("; MIN:%5.1f, MAX:%5.1f",
                                         static_cast<FLOAT64>(linkMin),
                                         static_cast<FLOAT64>(linkMax));
    }

    Printf(pri, "%s\n", linkStatus.c_str());

    if (JsonLogStream::IsOpen())
    {
        JsonItem jsi;
        jsi.SetTag("lwlink_eye_measurement");
        jsi.SetField("device", m_pTestDevice->GetName().c_str());
        jsi.SetField("linkid", link);
        jsi.SetField("mode", GetModeStr().c_str());
        jsi.SetField("index", index.c_str());
        jsi.SetField("data", &vals);
        jsi.SetField("linkMin", linkMin);
        jsi.SetField("linkMax", linkMax);

        jsi.WriteToLogfile();
    }
}

LwLinkEyeDiagram::LinkArrayF LwLinkEyeDiagram::StatusVals::GetStdVals(UINT32 link) const
{
    LinkArrayF stdVals = m_VarVals[link];
    if (m_NumVals[link] != 0)
    {
        for (UINT32 lane = 0; lane < m_NumLanes; lane++)
            stdVals[lane] = sqrt(stdVals[lane]);
    }
    return stdVals;
}

string LwLinkEyeDiagram::StatusVals::GetModeStr() const
{
    switch (m_Mode)
    {
        case LwLink::EYEO_X:
            return "X";
        case LwLink::EYEO_XL_O:
            return "XL_O";
        case LwLink::EYEO_XL_E:
            return "XL_E";
        case LwLink::EYEO_XH_O:
            return "XH_O";
        case LwLink::EYEO_XH_E:
            return "XH_E";
        case LwLink::EYEO_Y:
            return "Y";
        case LwLink::EYEO_Y_U:
            return "Y_U";
        case LwLink::EYEO_Y_M:
            return "Y_M";
        case LwLink::EYEO_Y_L:
            return "Y_L";
        case LwLink::EYEO_YL_O:
            return "YL_O";
        case LwLink::EYEO_YL_E:
            return "YL_E";
        case LwLink::EYEO_YH_O:
            return "YH_O";
        case LwLink::EYEO_YH_E:
            return "YH_E";
        default:
            return "";
    }
}

void LwLinkEyeDiagram::PrintJsProperties(Tee::Priority pri)
{

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "        PrintPerLoop:          %s\n", m_PrintPerLoop?"true":"false");
    PrintPerLaneJsArray(pri, "        XPassThreshold:        ", "%.3f", m_XPassThresholds);
    PrintPerLaneJsArray(pri, "        XFailThreshold:        ", "%.3f", m_XFailThresholds);
    PrintPerLaneJsArray(pri, "        YPassThreshold:        ", "%.3f", m_YPassThresholds);
    PrintPerLaneJsArray(pri, "        YFailThreshold:        ", "%.3f", m_YFailThresholds);
    PrintPerLaneJsArray(pri, "        Pam4YPassThreshold:    ", "%.3f", m_Pam4YPassThresholds);
    PrintPerLaneJsArray(pri, "        Pam4YFailThreshold:    ", "%.3f", m_Pam4YFailThresholds);
    PrintPerLinkJsArray(pri, "        BerNumErrors:          ", "%u", m_BerNumErrors);
    PrintPerLinkJsArray(pri, "        BerNumBlocks:          ", "%u", m_BerNumBlocks);
    Printf(pri, "        TestTypeMask:          %u\n", m_TestTypeMask);
    Printf(pri, "        LoopDelayMs:           %f\n", m_LoopDelayMs);
    Printf(pri, "        EnableUphyLogging:     %s\n", m_EnableUphyLogging ? "true" : "false");
    Printf(pri, "        LogUphyOnLoopFailure:  %s\n", m_LogUphyOnLoopFailure ? "true" : "false");
    Printf(pri, "        LogUphyOnTestFailure:  %s\n", m_LogUphyOnTestFailure ? "true" : "false");
    Printf(pri, "        LogUphyOnTest:         %s\n", m_LogUphyOnTest ? "true" : "false");
    if (m_LogUphyOnLoop == ~0U)
        Printf(pri, "        LogUphyOnLoop:         disabled\n");
    else
        Printf(pri, "        LogUphyOnLoop:         %u\n", m_LogUphyOnLoop);
    Printf(pri, "        PrintMarginalWarning:  %s\n", m_PrintMarginalWarning ? "true" : "false");
    Printf(pri, "        SkipEomCheck:          %s\n", m_SkipEomCheck ? "true" : "false");
    Printf(pri, "        NumRetriesOnZeroEom:   %u\n", m_NumRetriesOnZeroEom);
    GpuTest::PrintJsProperties(pri);
}

JS_CLASS_INHERIT(LwLinkEyeDiagram, LwLinkTest, "LwLinkEyeDiagram");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, PrintPerLoop, bool,
                     "Print the status for every loop.");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, TestTypeMask, UINT32,
                     "Type of links to test. 0x1 = ACCESS, 0x2 = TRUNK. Default = 0x3 (both)");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, KeepRunning, bool,
                     "Keep the test running in the background");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, LoopDelayMs, FLOAT64,
                     "Number of milliseconds to wait in between each loop.");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, EnableUphyLogging, bool,
                     "Enable UPHY register logging on each capture of eom data.");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, LogUphyOnLoopFailure, bool,
                     "Enable UPHY register logging when an individual loop has values below the threshold.");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, LogUphyOnTestFailure, bool,
                     "Enable UPHY register logging when the test fails.");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, LogUphyOnTest, bool,
                     "Enable UPHY register logging when the test completes (pass or fail).");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, LogUphyOnLoop, UINT32,
                     "Enable UPHY register logging for the specified loop");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, PrintMarginalWarning, bool,
                     "Print warning if EOM inidicates link could potenitially be marginal.");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, SkipEomCheck, bool,
                     "Skip Lwlink EOM checks (default = false)");
CLASS_PROP_READWRITE(LwLinkEyeDiagram, NumRetriesOnZeroEom, UINT32,
                     "When EOM = 0, number of EOM retries to do before failing (default = 0)");
