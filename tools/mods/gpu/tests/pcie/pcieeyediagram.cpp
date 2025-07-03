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

#include "core/include/version.h"
#include "core/include/jsonlog.h"
#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "device/interface/pcie.h"
#include "device/interface/pcie/pcieuphy.h"
#include "device/interface/uphyif.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/tests/gputest.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/pcie/pexdev.h"
#include "mods_reg_hal.h"

#include <cstring>

namespace
{
    struct JsPropToLinkSpeed
    {
        const char *       prop;
        Pci::PcieLinkSpeed linkSpeed;
    };
    const JsPropToLinkSpeed s_JsPropToLinkSpeed[] =
    {
         { "Default", Pci::SpeedUnknown   }
        ,{ "Gen1",    Pci::Speed2500MBPS  }
        ,{ "Gen2",    Pci::Speed5000MBPS  }
        ,{ "Gen3",    Pci::Speed8000MBPS  }
        ,{ "Gen4",    Pci::Speed16000MBPS }
        ,{ "Gen5",    Pci::Speed32000MBPS }
    };
}

class PcieEyeDiagram : public GpuTest
{
public:
    PcieEyeDiagram();
    virtual ~PcieEyeDiagram() {}

    RC InitFromJs() override;
    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(PrintPerLoop,         bool);
    SETGET_PROP(EnableUphyLogging,    bool);
    SETGET_PROP(LogUphyOnLoopFailure, bool);
    SETGET_PROP(LogUphyOnTestFailure, bool);
    SETGET_PROP(LogUphyOnTest,        bool);
    SETGET_PROP(LogUphyOnLoop,        UINT32);
    SETGET_PROP(PrintMarginalWarning, bool);
    SETGET_PROP(SkipEomCheck,         bool);
    SETGET_PROP(LoopDelayMs,          FLOAT64);

private:
    struct EyeThreshold
    {
        Pci::PcieLinkSpeed linkSpeed;
        vector<UINT32>     laneThresholds;
    };

    UINT32 m_NumLoops             = 100;
    bool   m_PrintPerLoop         = false;
    vector<EyeThreshold> m_XPassThresholds;
    vector<EyeThreshold> m_XFailThresholds;
    vector<EyeThreshold> m_YPassThresholds;
    vector<EyeThreshold> m_YFailThresholds;
    bool    m_EnableUphyLogging    = false;
    bool    m_LogUphyOnLoopFailure = false;
    bool    m_LogUphyOnTestFailure = false;
    bool    m_LogUphyOnTest        = false;
    UINT32  m_LogUphyOnLoop        = ~0U;
    bool    m_PrintMarginalWarning = false;
    bool    m_SkipEomCheck         = false;
    FLOAT64 m_LoopDelayMs          = 0.0;

    static constexpr FLOAT64 MIN_LOOP_DELAY = 250.0;
    static constexpr FLOAT64 WARN_LOOP_DELAY = 1000.0;

    RC GetFirstLane(UINT32* pLane);
    RC GetLinkSpeedThresholds
    (
        const vector<EyeThreshold> & perGenSpeedData,
        Pci::PcieLinkSpeed           linkSpeed,
        const vector<UINT32> **      ppLaneThresholds
    );
    RC InitPerGenSpeedJsArray(const char * jsPropName, vector<EyeThreshold> *pPerGenSpeedData);
    RC InitPerLaneJsArray
    (
        JSObject * pThreshObject,
        const string & prop,
        vector<UINT32> *pPerLaneData
    );

    template<typename T> void LogUphyData
    (
        Pci::FomMode fomMode,
        UINT32 firstLane,
        const vector<T>& vals,
        map<Pci::FomMode, vector<vector<INT32>>> * pEomValues
    );
    void PrintEomStatus(Tee::Priority pri, string mode, string type,
                        const vector<UINT32>& status,
                        bool printLinkMinMax);
    void PrintPerGenSpeedJsArray
    (
        Tee::Priority pri,
        const char * pFormattedName,
        const vector<EyeThreshold> &perGenSpeedData
    );
    RC ValidateThresholds
    (
        const vector<EyeThreshold> & passThresholds,
        const vector<EyeThreshold> & failThresholds
    );

    RC DisableAspm();

    class StatusVals
    {
    public:
        explicit StatusVals(TestDevicePtr pTestDevice,
                            Pci::FomMode mode,
                            const vector<UINT32> & passThresholds,
                            const vector<UINT32> & failThresholds,
                            UINT32 firstLane,
                            UINT32 numLanes,
                            bool skipEomCheck,
                            bool printMarginalWarn,
                            bool forcePrintAvg,
                            bool requireGen5,
                            bool verbose);
        RC AddVals(const vector<UINT32> & vals);
        RC CheckVals(const vector<UINT32> & vals) const;

        RC CheckMilwals() const;
        RC CheckAvgVals() const;
        const vector<FLOAT32> & GetAvgVals() const { return m_AvgVals; }

        void PrintSummary(Tee::Priority pri) const;
        template<typename T>
        void PrintEomStatus(Tee::Priority pri,
                            const vector<T>& status,
                            string index,
                            bool printLinkMinMax) const;

        Pci::FomMode GetMode() const { return m_Mode; }
        bool GetForcePrintAvg() const { return m_ForcePrintAvg; }
        bool GetRequireGen5() const { return m_RequireGen5; }

    private:
        TestDevicePtr       m_pTestDevice;
        Pci::FomMode        m_Mode               = Pci::EYEO_Y;
        vector<UINT32>      m_PassThresholds;
        vector<UINT32>      m_FailThresholds;
        UINT32              m_FirstLane          = 0;
        UINT32              m_NumLanes           = 0;
        bool                m_SkipEomCheck       = false;
        bool                m_PrintMarginalWarn  = false;
        bool                m_ForcePrintAvg      = false;
        bool                m_RequireGen5        = false;
        bool                m_Verbose            = false;

        vector<UINT32>   m_Milwals;
        vector<UINT32>   m_MaxVals;
        vector<FLOAT32>  m_VarVals;
        vector<FLOAT32>  m_AvgVals;
        UINT32           m_NumVals = 0;

        vector<FLOAT32>  GetStdVals() const;

        string GetModeStr() const;
    };
};

PcieEyeDiagram::PcieEyeDiagram()
{
    SetName("PcieEyeDiagram");
}

RC PcieEyeDiagram::InitPerLaneJsArray
(
    JSObject * pThreshObject,
    const string & prop,
    vector<UINT32> *pPerLaneData
)
{
    RC rc;
    JavaScriptPtr pJs;
    jsval jsvPerLaneData;
    rc = pJs->GetPropertyJsval(pThreshObject, prop.c_str(), &jsvPerLaneData);
    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    const UINT32 maxLanes = pPcie->GetLinkWidth();

    if (RC::OK == rc)
    {
        if (JSVAL_IS_NUMBER(jsvPerLaneData))
        {
            UINT32 allLaneData;
            CHECK_RC(pJs->FromJsval(jsvPerLaneData, &allLaneData));
            pPerLaneData->assign(maxLanes, allLaneData);
        }
        else if (JSVAL_IS_OBJECT(jsvPerLaneData))
        {
            JsArray jsaPerLaneData;
            CHECK_RC(pJs->FromJsval(jsvPerLaneData, &jsaPerLaneData));

            if (jsaPerLaneData.size() < maxLanes)
            {
                Printf(Tee::PriError,
                    "%s : Too few of per-lane entries for %s, minimum expected %u, actual %u\n",
                    GetName().c_str(), prop.c_str(), maxLanes,
                    static_cast<UINT32>(jsaPerLaneData.size()));
                return RC::ILWALID_ARGUMENT;
            }

            pPerLaneData->clear();
            for (size_t i = 0; i < maxLanes; ++i)
            {
                UINT32 laneData;
                CHECK_RC(pJs->FromJsval(jsaPerLaneData[i], &laneData));
                pPerLaneData->push_back(laneData);
            }
        }
        else
        {
            Printf(Tee::PriError,
                   "%s : Invalid JS specification of %s",
                   GetName().c_str(),
                   prop.c_str());
            return RC::ILWALID_ARGUMENT;
        }
    }
    else if (RC::ILWALID_OBJECT_PROPERTY == rc)
    {
        rc.Clear();
        pPerLaneData->assign(maxLanes, 0U);
    }
    return rc;
}

bool PcieEyeDiagram::IsSupported()
{
    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    return (pPcie && pPcie->SupportsFom());
}

RC PcieEyeDiagram::ValidateThresholds
(
    const vector<EyeThreshold> & passThresholds,
    const vector<EyeThreshold> & failThresholds
)
{
    RC rc;

    set<Pci::PcieLinkSpeed> speedsToVerify;
    for (const auto &  lwrThresh : passThresholds)
        speedsToVerify.insert(lwrThresh.linkSpeed);
    for (const auto & lwrThresh : failThresholds)
        speedsToVerify.insert(lwrThresh.linkSpeed);

    const vector<UINT32> *pPassThresholds;
    const vector<UINT32> *pFailThresholds;
    for (const auto speed : speedsToVerify)
    {
        CHECK_RC(GetLinkSpeedThresholds(passThresholds, speed, &pPassThresholds));
        CHECK_RC(GetLinkSpeedThresholds(failThresholds, speed, &pFailThresholds));
        if (pPassThresholds->size() != pPassThresholds->size())
        {
            Printf(Tee::PriError,
                   "Pass and fail thresholds for PCIE Speed %d are not the same size\n",
                   speed);
            return RC::ILWALID_ARGUMENT;
        }

        for (size_t lane = 0; lane < pPassThresholds->size(); ++lane)
        {
            if ((*pFailThresholds)[lane] > (*pPassThresholds)[lane])
            {
                Printf(Tee::PriError,
                       "FailThreshold cannot be greater than PassThreshold (speed %d)\n",
                       speed);
                return RC::ILWALID_ARGUMENT;
            }
        }
    }
    return RC::OK;
}

RC PcieEyeDiagram::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    UphyIf::Version uphyVer = UphyIf::Version::UNKNOWN;
    if (GetBoundTestDevice()->SupportsInterface<PcieUphy>())
        uphyVer = GetBoundTestDevice()->GetInterface<PcieUphy>()->GetVersion();

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

    // This is guaranteed in InitFromJs
    rc = ValidateThresholds(m_XPassThresholds, m_XFailThresholds);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Invalid X thresholds!\n");
        return RC::ILWALID_ARGUMENT;
    }
    rc = ValidateThresholds(m_YPassThresholds, m_YFailThresholds);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "Invalid Y thresholds!\n");
        return RC::ILWALID_ARGUMENT;
    }

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        SetVerbosePrintPri(Tee::PriNone);
    }

    return rc;
}

RC PcieEyeDiagram::GetFirstLane(UINT32* pLane)
{
    MASSERT(pLane);
    RC rc;
    UINT32 rxLanes = 0x0;
    UINT32 txLanes = 0x0; // unused
    CHECK_RC(GetBoundTestDevice()->GetInterface<Pcie>()->GetPcieLanes(&rxLanes, &txLanes));
    MASSERT(rxLanes);
    *pLane = (rxLanes) ? static_cast<UINT32>(Utility::BitScanForward(rxLanes)) : 0;
    return OK;
}

RC PcieEyeDiagram::InitFromJs()
{
    RC rc;
    CHECK_RC(GpuTest::InitFromJs());
    CHECK_RC(InitPerGenSpeedJsArray("XPassThreshold",     &m_XPassThresholds));
    CHECK_RC(InitPerGenSpeedJsArray("XFailThreshold",     &m_XFailThresholds));
    CHECK_RC(InitPerGenSpeedJsArray("YPassThreshold",     &m_YPassThresholds));
    CHECK_RC(InitPerGenSpeedJsArray("YFailThreshold",     &m_YFailThresholds));
    return rc;
}

RC PcieEyeDiagram::GetLinkSpeedThresholds
(
    const vector<EyeThreshold> & perGenSpeedData,
    Pci::PcieLinkSpeed           linkSpeed,
    const vector<UINT32> **      ppLaneThresholds
)
{
    auto pSpeedThreshold = find_if(perGenSpeedData.begin(), perGenSpeedData.end(),
                                   [&linkSpeed] (const EyeThreshold & et) -> bool
                                   {
                                       return linkSpeed == et.linkSpeed;
                                   });
    if (pSpeedThreshold != perGenSpeedData.end())
    {
        if (ppLaneThresholds != nullptr)
        {
            *ppLaneThresholds = &(pSpeedThreshold->laneThresholds);
        }
        return RC::OK;
    }
    if ((pSpeedThreshold == perGenSpeedData.end()) && (linkSpeed != Pci::SpeedUnknown))
    {
        return GetLinkSpeedThresholds(perGenSpeedData, Pci::SpeedUnknown, ppLaneThresholds);
    }

    // No default link speed provided to the test
    return RC::BAD_PARAMETER;
}

RC PcieEyeDiagram::InitPerGenSpeedJsArray
(
    const char *          jsPropName,
    vector<EyeThreshold> *pPerGenSpeedData
)
{
    RC rc;
    JavaScriptPtr pJs;
    jsval jsvPerGenSpeedData;
    rc = pJs->GetPropertyJsval(GetJSObject(), jsPropName, &jsvPerGenSpeedData);

    const UINT32 maxLinks = GetBoundTestDevice()->GetInterface<Pcie>()->GetLinkWidth();
    vector<UINT32> zeroThresholds(maxLinks, 0);

    if (RC::OK == rc)
    {
        pPerGenSpeedData->clear();
        if (JSVAL_IS_NUMBER(jsvPerGenSpeedData))
        {
            UINT32 allGenSpeedData;
            CHECK_RC(pJs->FromJsval(jsvPerGenSpeedData, &allGenSpeedData));
            pPerGenSpeedData->push_back({Pci::SpeedUnknown, vector<UINT32>(maxLinks, allGenSpeedData)});
        }
        else if (JSVAL_IS_OBJECT(jsvPerGenSpeedData))
        {
            JSObject * pJsaPerGenSpeedData;
            CHECK_RC(pJs->FromJsval(jsvPerGenSpeedData, &pJsaPerGenSpeedData));

            vector<string> properties;
            CHECK_RC(pJs->GetProperties(pJsaPerGenSpeedData, &properties));

            vector<UINT32> tempThresholds;
            auto SetGenSpeedThreshold = [&] (const string &prop, const string &forcedCaseProp) -> RC
            {
                auto pPropToLinkSpeed = find_if(begin(s_JsPropToLinkSpeed),
                    end(s_JsPropToLinkSpeed),
                    [&forcedCaseProp] (const JsPropToLinkSpeed & jsp) -> bool
                    {
                        return !strcmp(forcedCaseProp.c_str(), jsp.prop);
                    });

                if (pPropToLinkSpeed != end(s_JsPropToLinkSpeed))
                {
                    rc = InitPerLaneJsArray(pJsaPerGenSpeedData, prop, &tempThresholds);
                    if (rc == RC::OK)
                    {
                        pPerGenSpeedData->push_back({ pPropToLinkSpeed->linkSpeed,
                            tempThresholds });
                    }
                }
                else
                {
                    VerbosePrintf("%s : Unknown %s property %s\n",
                                  GetName().c_str(), jsPropName, prop.c_str());
                }
                return rc;
            };

            for (auto const & prop : properties)
            {
                string forcedCaseProp = prop;
                forcedCaseProp[0] = ::toupper(forcedCaseProp[0]);
                transform(forcedCaseProp.begin() + 1, forcedCaseProp.end(),
                          forcedCaseProp.begin() + 1, ::tolower);
                CHECK_RC(SetGenSpeedThreshold(prop, forcedCaseProp));
            }

            if (GetLinkSpeedThresholds(*pPerGenSpeedData, Pci::SpeedUnknown, nullptr) != RC::OK)
            {
                Printf(Tee::PriWarn, "%s : No default %s threshold specified, using 0\n",
                       GetName().c_str(), jsPropName);
                pPerGenSpeedData->push_back({ Pci::SpeedUnknown, zeroThresholds });
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
        pPerGenSpeedData->push_back({ Pci::SpeedUnknown, zeroThresholds });
    }
    return rc;
}

template<typename T> void PcieEyeDiagram::LogUphyData
(
    Pci::FomMode fomMode,
    UINT32 firstLane,
    const vector<T>& vals,
    map<Pci::FomMode, vector<vector<INT32>>> * pEomValues
)
{
    if (!GetBoundTestDevice()->SupportsInterface<PcieUphy>())
        return;

    auto pPcieUphy = GetBoundTestDevice()->GetInterface<PcieUphy>();
    const UINT32 maxLanes = pPcieUphy->GetMaxLanes(UphyIf::RegBlock::DLN);

    if (!pEomValues->count(fomMode))
       (*pEomValues)[fomMode] = vector<vector<INT32>>(1);
    for (UINT32 lwrLane = 0; lwrLane < maxLanes; lwrLane++)
    {
        if ((lwrLane < firstLane) || (lwrLane >= (firstLane + vals.size())))
        {
            (*pEomValues)[fomMode][0].push_back(_INT32_MAX);
        }
        else
        {
            (*pEomValues)[fomMode][0].push_back(static_cast<INT32>(vals[lwrLane - firstLane]));
        }
    }
}

RC PcieEyeDiagram::DisableAspm()
{
    RC rc;

    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();

    CHECK_RC(pPcie->SetAspmState(0));

    UINT32 portId = 0;
    PexDevice* pUpStreamDev = nullptr;
    CHECK_RC(pPcie->GetUpStreamInfo(&pUpStreamDev, &portId));
    if (pUpStreamDev)
    {
        CHECK_RC(pUpStreamDev->SetDownStreamAspmState(0, portId, true));
    }

    // set aspm for the rest of the devices
    while (pUpStreamDev && !pUpStreamDev->IsRoot())
    {
        CHECK_RC(pUpStreamDev->SetUpStreamAspmState(0, true));
        portId       = pUpStreamDev->GetUpStreamIndex();
        pUpStreamDev = pUpStreamDev->GetUpStreamDev();
        if (pUpStreamDev)
        {
            CHECK_RC(pUpStreamDev->SetDownStreamAspmState(0, portId, true));
        }
    }

    return rc;
}

RC PcieEyeDiagram::Run()
{
    RC rc;

    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    const FLOAT64 timeoutMs = GetTestConfiguration()->TimeoutMs();

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

    UINT32 linkWidth = pPcie->GetLinkWidth();
    MASSERT(linkWidth);

    UINT32 firstLane;
    CHECK_RC(GetFirstLane(&firstLane));

    const bool bLogUphyOnFailure =
        (m_EnableUphyLogging || m_LogUphyOnLoopFailure || m_LogUphyOnTestFailure || m_LogUphyOnTest) &&
        (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM);
    const bool bLogUphy = m_EnableUphyLogging &&
                         (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM);

    const Pci::PcieLinkSpeed pciLinkSpeed = pPcie->GetLinkSpeed(Pci::SpeedUnknown);
    const vector<UINT32> * pXPassThresholds;
    const vector<UINT32> * pXFailThresholds;
    const vector<UINT32> * pYPassThresholds;
    const vector<UINT32> * pYFailThresholds;

    CHECK_RC(GetLinkSpeedThresholds(m_XPassThresholds, pciLinkSpeed, &pXPassThresholds));
    CHECK_RC(GetLinkSpeedThresholds(m_XFailThresholds, pciLinkSpeed, &pXFailThresholds));
    CHECK_RC(GetLinkSpeedThresholds(m_YPassThresholds, pciLinkSpeed, &pYPassThresholds));
    CHECK_RC(GetLinkSpeedThresholds(m_YFailThresholds, pciLinkSpeed, &pYFailThresholds));

    vector<StatusVals> statusValsArray;
    const bool verbose = GetTestConfiguration()->Verbose();
#define ADD_CONFIG(mode, pass, fail, printAvg, requiresGen5, internalOnly)                        \
    if (pPcie->SupportsFomMode(Pci::mode)                                                         \
        && (!internalOnly || (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))) \
        statusValsArray.push_back(StatusVals(GetBoundTestDevice(),                                \
                                             Pci::mode,                                           \
                                             pass,                                                \
                                             fail,                                                \
                                             firstLane,                                           \
                                             linkWidth,                                           \
                                             m_SkipEomCheck,                                      \
                                             m_PrintMarginalWarning,                              \
                                             printAvg,                                            \
                                             requiresGen5,                                        \
                                             verbose));

    vector<UINT32> _UINT32_MIN_ARR(linkWidth, 0);
    if (verbose)
    {
        ADD_CONFIG(EYEO_Y_U,  _UINT32_MIN_ARR,  _UINT32_MIN_ARR, false, true, true);
        ADD_CONFIG(EYEO_Y_L,  _UINT32_MIN_ARR,  _UINT32_MIN_ARR, false, true, true);
    }

    ADD_CONFIG(EYEO_X, *pXPassThresholds, *pXFailThresholds, true, false, false);
    ADD_CONFIG(EYEO_Y, *pYPassThresholds, *pYFailThresholds, true, false, false);

    map<Pci::FomMode, vector<vector<INT32>>> eomValues;
    for (UINT32 loops = 0; loops < m_NumLoops; loops++)
    {
        string loopIndexStr = Utility::StrPrintf("(%d)", loops);

        eomValues.clear();
        bool bLogUphyLoopFailure = false;
        auto LogUphyPerEom = [&] () -> RC
        {
            if (bLogUphy || ((rc != RC::OK) && bLogUphyOnFailure) ||
                bLogUphyLoopFailure || (m_LogUphyOnLoop == loops))
            {
                return UphyRegLogger::LogRegs(GetBoundTestDevice(),
                                              UphyRegLogger::UphyInterface::Pcie,
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
            if (status.GetRequireGen5() && (pciLinkSpeed < Pci::Speed32000MBPS))
                continue;

            vector<UINT32> eomStatus(linkWidth, 0);
            CHECK_RC(pPcie->GetEomStatus(status.GetMode(), &eomStatus, timeoutMs));

            if (m_PrintPerLoop)
            {
                Tee::Priority pri = (status.GetForcePrintAvg()) ? Tee::PriNormal : GetVerbosePrintPri();
                status.PrintEomStatus(pri, eomStatus, loopIndexStr, true);
            }

            if ((UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM) &&
                m_LogUphyOnLoopFailure &&
                status.CheckVals(eomStatus) != RC::OK)
            {
                bLogUphyLoopFailure = true;
            }

            if (bLogUphyOnFailure || (m_LogUphyOnLoop == loops))
                LogUphyData(status.GetMode(), firstLane, eomStatus, &eomValues);

            CHECK_RC(status.AddVals(eomStatus));
            CHECK_RC(status.CheckMilwals());
            if ((idx != (statusValsArray.size() - 1)) || (loops != (m_NumLoops - 1)))
            {
                Tasker::Sleep(m_LoopDelayMs);
            }
        }
        logUphy.Cancel();
        CHECK_RC(LogUphyPerEom());
    }

    StickyRC checkValsRc;
    for (const StatusVals& status : statusValsArray)
    {
        if (status.GetRequireGen5() && (pciLinkSpeed < Pci::Speed32000MBPS))
            continue;

        status.PrintSummary(GetVerbosePrintPri());
        checkValsRc = status.CheckAvgVals();
    }

    if ((UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::EOM) &&
        ((m_LogUphyOnTestFailure && (checkValsRc != RC::OK)) || m_LogUphyOnTest))
    {
        eomValues.clear();
        for (const StatusVals& status : statusValsArray)
        {
            const vector<FLOAT32> & avgEomValues = status.GetAvgVals();
            if (!avgEomValues.empty())
            {
                LogUphyData(status.GetMode(), firstLane, avgEomValues, &eomValues);
            }
        }
        CHECK_RC(UphyRegLogger::LogRegs(GetBoundTestDevice(),
                                        UphyRegLogger::UphyInterface::Pcie,
                                        UphyRegLogger::LogPoint::EOM,
                                        checkValsRc,
                                        eomValues));
    }
    CHECK_RC(checkValsRc);

    restoreAspm.Cancel();
    CHECK_RC(RestorePexSettings(GetBoundTestDevice()));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \return OK if successful, not OK otherwise
void PcieEyeDiagram::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "        PrintPerLoop:         %s\n", m_PrintPerLoop ? "true" : "false");
    PrintPerGenSpeedJsArray(pri, "        XPassThreshold:       ", m_XPassThresholds);
    PrintPerGenSpeedJsArray(pri, "        XFailThreshold:       ", m_XFailThresholds);
    PrintPerGenSpeedJsArray(pri, "        YPassThreshold:       ", m_YPassThresholds);
    PrintPerGenSpeedJsArray(pri, "        YFailThreshold:       ", m_YFailThresholds);
    Printf(pri, "        EnableUphyLogging:    %s\n", m_EnableUphyLogging ? "true" : "false");
    Printf(pri, "        LogUphyOnLoopFailure: %s\n", m_LogUphyOnLoopFailure ? "true" : "false");
    Printf(pri, "        LogUphyOnTestFailure: %s\n", m_LogUphyOnTestFailure ? "true" : "false");
    Printf(pri, "        LogUphyOnTest:        %s\n", m_LogUphyOnTest ? "true" : "false");
    if (m_LogUphyOnLoop == ~0U)
        Printf(pri, "        LogUphyOnLoop:        disabled\n");
    else
        Printf(pri, "        LogUphyOnLoop:        %u\n", m_LogUphyOnLoop);
    Printf(pri, "        PrintMarginalWarning: %s\n", m_PrintMarginalWarning ? "true" : "false");
    Printf(pri, "        SkipEomCheck:         %s\n", m_SkipEomCheck ? "true" : "false");
    Printf(pri, "        LoopDelayMs:          %f\n", m_LoopDelayMs);
    GpuTest::PrintJsProperties(pri);
}

void PcieEyeDiagram::PrintPerGenSpeedJsArray
(
    Tee::Priority pri,
    const char * pFormattedName,
    const vector<EyeThreshold> &perGenSpeedData
)
{
    auto GetPerLaneString = [] (const vector<UINT32> & perLaneData) -> string
    {
        string perLaneStr = "[";
        for (size_t ii = 0; ii < perLaneData.size(); ++ii)
        {
            perLaneStr += Utility::StrPrintf("%s %u", (ii == 0) ? "" : ",", perLaneData[ii]);
        }
        perLaneStr += " ]";
        return perLaneStr;
    };

    string paddingStr(strlen(pFormattedName) + 1, ' ');
    string perGenSpeedStr = Utility::StrPrintf("%s[", pFormattedName);
    for (size_t ii = 0; ii < perGenSpeedData.size(); ++ii)
    {
        string genStr = "UNKNOWN";

        auto pLinkSpeedToStr = find_if(begin(s_JsPropToLinkSpeed),
            end(s_JsPropToLinkSpeed),
            [&perGenSpeedData, ii] (const JsPropToLinkSpeed & jsp) -> bool
            {
                return perGenSpeedData[ii].linkSpeed == jsp.linkSpeed;
            });
        if (pLinkSpeedToStr != end(s_JsPropToLinkSpeed))
            genStr = pLinkSpeedToStr->prop;

        perGenSpeedStr += Utility::StrPrintf("%s%s %s%s : ",
                                             (ii == 0) ? "" : ",\n",
                                             (ii == 0) ? "" : paddingStr.c_str(),                                             
                                             genStr.c_str(),
                                             (genStr == "Default") ? "" : "   ");
        perGenSpeedStr += GetPerLaneString(perGenSpeedData[ii].laneThresholds);
    }
    Printf(pri, "%s ]\n", perGenSpeedStr.c_str());
}

PcieEyeDiagram::StatusVals::StatusVals
(
    TestDevicePtr pTestDevice,
    Pci::FomMode mode,
    const vector<UINT32> & passThresholds,
    const vector<UINT32> & failThresholds,
    UINT32 firstLane,
    UINT32 numLanes,
    bool skipEomCheck,
    bool printMarginalWarn,
    bool forcePrintAvg,
    bool requireGen5,
    bool verbose
)
: m_pTestDevice(pTestDevice)
, m_Mode(mode)
, m_PassThresholds(passThresholds)
, m_FailThresholds(failThresholds)
, m_FirstLane(firstLane)
, m_NumLanes(numLanes)
, m_SkipEomCheck(skipEomCheck)
, m_PrintMarginalWarn(printMarginalWarn)
, m_ForcePrintAvg(forcePrintAvg)
, m_RequireGen5(requireGen5)
, m_Verbose(verbose)
{
    m_Milwals.resize(numLanes, _UINT32_MAX);
    m_MaxVals.resize(numLanes, 0);
    m_VarVals.resize(numLanes, 0.0);
    m_AvgVals.resize(numLanes, 0.0);
}

RC PcieEyeDiagram::StatusVals::AddVals(const vector<UINT32>& vals)
{
    RC rc;

    for (UINT32 lane = 0; lane < m_NumLanes; lane++)
    {
        if (vals[lane] < m_Milwals[lane])
            m_Milwals[lane] = vals[lane];
        if (vals[lane] > m_MaxVals[lane])
            m_MaxVals[lane] = vals[lane];

        if (m_NumVals == 0)
        {
            m_VarVals[lane] = 0.0;
            m_AvgVals[lane] = static_cast<FLOAT32>(vals[lane]);
        }
        else
        {
            FLOAT32 valF = static_cast<FLOAT32>(vals[lane]);
            FLOAT32 p = static_cast<FLOAT32>(m_NumVals) / static_cast<FLOAT32>(m_NumVals+1);
            FLOAT32 q = 1.0f / static_cast<FLOAT32>(m_NumVals+1);
            m_VarVals[lane] += (pow(valF - m_AvgVals[lane], 2)
                                      * p - m_VarVals[lane]) * q;
            m_AvgVals[lane] += (valF - m_AvgVals[lane]) * q;
        }
    }
    m_NumVals++;

    return RC::OK;
}

RC PcieEyeDiagram::StatusVals::CheckVals(const vector<UINT32> & vals) const
{
    for (UINT32 lane = 0; lane < m_NumLanes; lane++)
    {
        if (vals[lane] <= m_FailThresholds[lane])
        {
            return m_SkipEomCheck ? RC::OK : RC::UNEXPECTED_RESULT;
        }
    }
    return RC::OK;
}

RC PcieEyeDiagram::StatusVals::CheckMilwals() const
{
    StickyRC rc;

    for (UINT32 lane = 0; lane < m_Milwals.size(); lane++)
    {
        INT32 val = m_Milwals[lane];
        if (val == 0)
        {
            Printf(Tee::PriError, "%s : PEX Physical Lane(%u) %s_STATUS = 0\n",
                   m_pTestDevice->GetName().c_str(),
                   m_FirstLane + lane, GetModeStr().c_str());
            rc = m_SkipEomCheck ? RC::OK : RC::UNEXPECTED_RESULT;
        }
    }
    return rc;
}

RC PcieEyeDiagram::StatusVals::CheckAvgVals() const
{
    StickyRC rc;
    for (UINT32 lane = 0; lane < m_AvgVals.size(); lane++)
    {
        FLOAT32 val = m_AvgVals[lane];
        if (val < m_PassThresholds[lane])
        {
            if (val <= m_FailThresholds[lane])
            {
                Printf(m_SkipEomCheck ? Tee::PriWarn : Tee::PriError,
                       "%s : PEX Physical Lane(%u) %s_STATUS = %f is %s Fail threshold = %u\n",
                       m_pTestDevice->GetName().c_str(),
                       m_FirstLane + lane,
                       GetModeStr().c_str(),
                       val,
                       (val == m_FailThresholds[lane]) ? "at" : "below",
                       m_FailThresholds[lane]);
                rc = m_SkipEomCheck ? RC::OK : RC::UNEXPECTED_RESULT;
                continue;
            }

            if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) ||
                m_PrintMarginalWarn)
            {
                Printf(Tee::PriWarn,
                       "%s : PEX Physical Lane(%u) could be potentially marginal %s_STATUS = %f "
                       "is above Fail threshold = %u but below Pass threshold = %u\n",
                       m_pTestDevice->GetName().c_str(),
                       m_FirstLane + lane, GetModeStr().c_str(),
                       val,
                       m_FailThresholds[lane],
                       m_PassThresholds[lane]);
            }
        }
    }
    return rc;
}

void PcieEyeDiagram::StatusVals::PrintSummary(Tee::Priority pri) const
{
    PrintEomStatus(pri, m_Milwals, "MIN", false);
    PrintEomStatus(pri, m_MaxVals, "MAX", false);
    PrintEomStatus(pri, GetStdVals(), "STD", false);

    PrintEomStatus((m_ForcePrintAvg) ? Tee::PriNormal : pri,
                   m_AvgVals,
                   (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) ? "" : "AVG",
                   false);
}

template<typename T>
void PcieEyeDiagram::StatusVals::PrintEomStatus
(
    Tee::Priority pri,
    const vector<T>& status,
    string index,
    bool printLinkMinMax
) const
{
    JavaScriptPtr pJs;
    JsArray vals;

    string statusStr = Utility::StrPrintf("%s : PEX Physical Lane(%u-%u) %s_STATUS %s%s= ",
                                          m_pTestDevice->GetName().c_str(),
                                          m_FirstLane,
                                          m_FirstLane + static_cast<UINT32>(status.size())-1,
                                          GetModeStr().c_str(),
                                          index.c_str(),
                                          index.length() ? " " : "");

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

        statusStr += Utility::StrPrintf("%*.*f", m_Verbose ? 5:3, m_Verbose ? 1:0,
                                         static_cast<FLOAT64>(statusT));
        if (lane < status.size() - 1)
        {
            statusStr += ", ";
        }
    }
    if (printLinkMinMax)
    {
        statusStr += Utility::StrPrintf("; MIN:%5.1f, MAX:%5.1f",
                                         static_cast<FLOAT64>(linkMin),
                                         static_cast<FLOAT64>(linkMax));
    }

    Printf(pri, "%s\n", statusStr.c_str());

    if (JsonLogStream::IsOpen())
    {
        JsonItem jsi;
        jsi.SetTag("pex_eye_measurement");
        jsi.SetField("device", m_pTestDevice->GetName().c_str());
        jsi.SetField("mode", GetModeStr().c_str());
        jsi.SetField("index", index.c_str());
        jsi.SetField("data", &vals);
        jsi.SetField("linkMin", linkMin);
        jsi.SetField("linkMax", linkMax);

        jsi.WriteToLogfile();
    }
}

vector<FLOAT32> PcieEyeDiagram::StatusVals::GetStdVals() const
{
    vector<FLOAT32> stdVals = m_VarVals;
    if (m_NumVals != 0)
    {
        for (UINT32 lane = 0; lane < m_NumLanes; lane++)
            stdVals[lane] = sqrt(stdVals[lane]);
    }
    return stdVals;
}

string PcieEyeDiagram::StatusVals::GetModeStr() const
{
    switch (m_Mode)
    {
        case Pci::EYEO_X:
            return "X";
        case Pci::EYEO_Y:
            return "Y";
        case Pci::EYEO_Y_U:
            return "Y_U";
        case Pci::EYEO_Y_L:
            return "Y_L";
        default:
            return "";
    }
}

JS_CLASS_INHERIT(PcieEyeDiagram, GpuTest, "PcieEyeDiagram");
CLASS_PROP_READWRITE_FULL(PcieEyeDiagram, PrintPerLoop, bool,
                          "Print the status for every loop.", MODS_INTERNAL_ONLY | SPROP_VALID_TEST_ARG, 0);
CLASS_PROP_READWRITE(PcieEyeDiagram, EnableUphyLogging, bool,
                     "Enable UPHY register logging on each capture of eom data.");
CLASS_PROP_READWRITE(PcieEyeDiagram, LogUphyOnLoopFailure, bool,
                     "Enable UPHY register logging when an individual loop has values below the threshold.");
CLASS_PROP_READWRITE(PcieEyeDiagram, LogUphyOnTestFailure, bool,
                     "Enable UPHY register logging when the full test fails.");
CLASS_PROP_READWRITE(PcieEyeDiagram, LogUphyOnTest, bool,
                     "Enable UPHY register logging when the test completes (pass or fail).");
CLASS_PROP_READWRITE(PcieEyeDiagram, LogUphyOnLoop, UINT32,
                     "Enable UPHY register logging for the specified loop");
CLASS_PROP_READWRITE(PcieEyeDiagram, PrintMarginalWarning, bool,
                     "Print warning if EOM inidicates link could potenitially be marginal.");
CLASS_PROP_READWRITE(PcieEyeDiagram, SkipEomCheck, bool,
                     "Skip PCIE EOM checks (default = false)");
CLASS_PROP_READWRITE(PcieEyeDiagram, LoopDelayMs, FLOAT64,
                     "Number of milliseconds to wait in between each loop.");
