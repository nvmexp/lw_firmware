/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mgrmgr.h"
#include "core/include/js_uint64.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/testdevicemgr.h"
#include "device/interface/lwlink.h"
#include "device/interface/lwlink/lwliobist.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "gpu/tests/lwlink/lwlinktest.h"

class GpuTestConfiguration;
class Goldelwalues;
class GpuSubdevice;

//-----------------------------------------------------------------------------
// TODO :
//     - Update link training call if necessary once bug 3355691 is complete
//     - Update NEA/NED as necessary once RM/Switch driver fixes are in
//
// https://lwbugs/3356807 : RM/Switch driver handling of LTSSM TEST
// https://lwbugs/3355691 : ALI hookup to train intranode connection
// https://lwbugs/3356681 : LwSwitch NEA/NED DLCMD support
//
// This test impelements the IOBIST_ECC sequence and does not require a HULK
// Links need to be divided into AC coupled and DC coupled links because AC
// coupled links selections for IOBIST time are different than DC coupled links
//
// Reference section 12.1 of the following document for the core of the procedure
//     https://p4viewer.lwpu.com/get/hw/lwgpu_gh100_lwlink/ip/lwif/lwlink/4.0/doc/arch/lwlipt/publish/released/lwlipt_4P0_Full.html
//-----------------------------------------------------------------------------
class LwLinkIobistTest : public LwLinkTest
{
public:
    LwLinkIobistTest();
    virtual ~LwLinkIobistTest() { }
    RC Setup() override;
    RC Cleanup() override;
    bool IsSupported() override;

    RC RunTest() override;

    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(AllDevices,            bool);
    SETGET_PROP(IsBgTest,              bool);
    SETGET_PROP(KeepRunning,           bool);
    SETGET_PROP(RuntimeMs,             UINT32);
    SETGET_PROP(ExtraIobistTimeoutMs,  UINT32);
    SETGET_PROP_ENUM(NeaMode,    UINT08, LwLink::NearEndLoopMode,  LwLink::NearEndLoopMode::NED);
    SETGET_PROP_ENUM(IobistType, UINT08, LwLinkIobist::IobistType, LwLinkIobist::IobistType::PostTrain);
    SETGET_PROP_ENUM(IobistTimeAc, UINT08, LwLinkIobist::IobistTime, LwLinkIobist::IobistTime::Iobt10s);
    SETGET_PROP_ENUM(IobistTimeDc, UINT08, LwLinkIobist::IobistTime, LwLinkIobist::IobistTime::Iobt800us);
    SETGET_PROP(PreFecRateThresh,       FLOAT64);
    SETGET_PROP(EccDecFailedRateThresh, FLOAT64);

    void SetLinkMask(UINT64 linkMask) { m_LinkMask = linkMask; }

private:
    // These timeouts are taken from:
    // https://confluence.lwpu.com/pages/viewpage.action?pageId=477793434
    static constexpr UINT32 IOBIST_ECC_PRETRAIN_TIMEOUT_MS  = 1015;
    static constexpr UINT32 IOBIST_ECC_POSTTRAIN_TIMEOUT_MS = 20000;

    enum class CouplingType : UINT08
    {
        AC,
        DC,
        Mixed,
        None
    };

    RC AclwmulateErrorCounts(const map<TestDevicePtr, UINT64> & deviceTestMasks, UINT32 loop);
    RC CacheRemoteConnectionStrings();
    RC CheckErrorRates(UINT32 totalLoops);
    RC CheckErrorFlags(const map<TestDevicePtr, UINT64> & deviceTestMasks, UINT32 loop);
    RC CheckLinkState(const map<TestDevicePtr, UINT64> & deviceTestMasks, UINT32 loop);
    RC ClearErrorCounts(const map<TestDevicePtr, UINT64> & deviceTestMasks);
    FLOAT64 GetIobistTimeMs(LwLinkIobist::IobistTime iobt) const;
    RC RunIobist
    (
        const map<TestDevicePtr, UINT64> & deviceTestMasks,
        CouplingType                       couplingType,
        UINT32                             loop
    );
    RC SetLinkState
    (
        const map<TestDevicePtr, UINT64> & deviceLinkMasks,
        LwLink::SubLinkState               state,
        bool                               bAllDevices
    );
    RC SetupIobist();
    RC SetupLinkMasks();

    map<TestDevicePtr, UINT64>      m_DeviceActiveLinkMasks;
    map<TestDevicePtr, UINT64>      m_DeviceAcLinkMasks;
    map<TestDevicePtr, UINT64>      m_DeviceDcLinkMasks;

    struct NearEndLinkMasks
    {
        UINT64 neaLinkMask = 0ULL;
        UINT64 nedLinkMask = 0ULL;
    };
    map<TestDevicePtr, NearEndLinkMasks>    m_DeviceNelbLinkMasks;
    map<TestDevicePtr, vector<LwLink::ErrorCounts>> m_DeviceErrorCounts;    
    map<TestDevicePtr, vector<string>> m_DeviceRemoteConnections;    

    vector<LwLinkPowerState::Owner> m_PowerStateOwners;
    bool                            m_bSwitchFabric   = false;
    CouplingType                    m_CouplingType    = CouplingType::None;
    StickyRC                        m_DeferredRc;

    // JS variables
    bool   m_AllDevices                     = false;
    bool   m_IsBgTest                       = false;
    bool   m_KeepRunning                    = false;
    UINT32 m_RuntimeMs                      = 0U;
    UINT32 m_ExtraIobistTimeoutMs           = 0U;
    LwLink::NearEndLoopMode  m_NeaMode      = LwLink::NearEndLoopMode::None;
    LwLinkIobist::IobistType m_IobistType   = LwLinkIobist::IobistType::PreTrain;
    LwLinkIobist::IobistTime m_IobistTimeAc = LwLinkIobist::IobistTime::Iobt800us;
    LwLinkIobist::IobistTime m_IobistTimeDc = LwLinkIobist::IobistTime::Iobt800us;
    FLOAT64 m_PreFecRateThresh              = -1.0;
    FLOAT64 m_EccDecFailedRateThresh        = -1.0;
    UINT64 m_LinkMask                       = ~0ULL;  

    static vector<LwLink::ErrorCounts::ErrId> s_ErrorIdsToCheck;
};

// TODO Add ECC_DEC_FAILED when available
vector<LwLink::ErrorCounts::ErrId> LwLinkIobistTest::s_ErrorIdsToCheck =
{
    LwLink::ErrorCounts::LWL_ECC_DEC_FAILED_ID,
    LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE0_ID,
    LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE1_ID
};

//-----------------------------------------------------------------------------
LwLinkIobistTest::LwLinkIobistTest()
{
    SetName("LwLinkIobistTest");
}

//-----------------------------------------------------------------------------
bool LwLinkIobistTest::IsSupported()
{
    if (!LwLinkTest::IsSupported())
        return false;

    if (!GetBoundTestDevice()->SupportsInterface<LwLinkIobist>())
    {
        Printf(Tee::PriLow, "%s : LwLinkIobist interface not supported\n", GetName().c_str());
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::Setup()
{
    RC rc;

    if (!GetBoundTestDevice()->SupportsInterface<LwLinkIobist>())
    {
        Printf(Tee::PriError, "%s : LwLinkIobist interface not supported\n", GetName().c_str());
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if ((m_LinkMask != ~0ULL) && m_AllDevices)
    {
        Printf(Tee::PriError, "%s : Specifying link mask with AllDevices is not supported\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(LwLinkTest::Setup());

    TestDevicePtr pDev = GetBoundTestDevice();

    // Transitioning to OFF ilwalidates all peer mappings, so remove all peer mappings
    // associated with any GPU under test
    if (m_AllDevices)
    {
        for (auto pTestDevice : *DevMgrMgr::d_TestDeviceMgr)
        {
            auto * pGpuSub = pTestDevice->GetInterface<GpuSubdevice>();
            if (pGpuSub)
            {
                CHECK_RC(DevMgrMgr::d_GraphDevMgr->RemoveAllPeerMappings(pGpuSub));
            }
        }
    }
    else if (GetBoundGpuSubdevice())
    {
        CHECK_RC(DevMgrMgr::d_GraphDevMgr->RemoveAllPeerMappings(GetBoundGpuSubdevice()));
    }

    CHECK_RC(SetupLinkMasks());
    CHECK_RC(CacheRemoteConnectionStrings());

    for (auto & lwrDevMask : m_DeviceActiveLinkMasks)
    {
        m_DeviceErrorCounts[lwrDevMask.first] =
            vector<LwLink::ErrorCounts>(lwrDevMask.first->GetInterface<LwLink>()->GetMaxLinks());
        if (lwrDevMask.first->SupportsInterface<LwLinkPowerState>())
        {
            m_PowerStateOwners.push_back(LwLinkPowerState::Owner(lwrDevMask.first->GetInterface<LwLinkPowerState>()));
            CHECK_RC(m_PowerStateOwners.back().Claim(LwLinkPowerState::ClaimState::FULL_BANDWIDTH));
        }
    }

    CHECK_RC(SetLinkState(m_DeviceActiveLinkMasks, LwLink::SLS_OFF, false));

    CHECK_RC(SetupIobist());

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::RunTest()
{
    RC rc;
    StickyRC deferredRc;
    bool bDone = false;
    const UINT32 endLoop = GetTestConfiguration()->Loops();
    UINT32 loop = 0;
    const UINT64 runStartMs = Xp::GetWallTimeMS();

    // Ensure the links are in the same state as when Run starts to allow Run to potentially be
    // called multiple times without setup/cleanup
    DEFERNAME(trainToOff)
    {
        // Training to OFF disables all near end loopback modes in HW
        SetLinkState(m_DeviceActiveLinkMasks, LwLink::SLS_OFF,
                     m_NeaMode != LwLink::NearEndLoopMode::None);
    };

    while (!bDone)
    {
        // If the time to spend in IOBIST is the same for AC and DC, just run
        // IOBIST on all links at the same time
        if (m_IobistTimeAc == m_IobistTimeDc)
        {
            CHECK_RC(RunIobist(m_DeviceActiveLinkMasks, CouplingType::DC, loop));
        }
        else
        {
            // Otherwise run all DC links followed by all AC links
            if (m_CouplingType != CouplingType::AC)
            {
                CHECK_RC(RunIobist(m_DeviceDcLinkMasks, CouplingType::DC, loop));
            }
            if (m_CouplingType != CouplingType::DC)
            {
                CHECK_RC(RunIobist(m_DeviceAcLinkMasks, CouplingType::AC, loop));
            }
        }

        loop++;
        bDone = (loop >= endLoop) || (m_IsBgTest && !m_KeepRunning);

        if (m_RuntimeMs && !m_IsBgTest &&
            (Xp::GetWallTimeMS() - runStartMs) >= m_RuntimeMs)
        {
            bDone = true;
        }
    }

    m_DeferredRc = CheckErrorRates(loop);

    trainToOff.Cancel();
    CHECK_RC(SetLinkState(m_DeviceActiveLinkMasks, LwLink::SLS_OFF,
                          m_NeaMode != LwLink::NearEndLoopMode::None));

    return m_DeferredRc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::Cleanup()
{
    StickyRC rc;

    for (auto const & lwrDevMask : m_DeviceActiveLinkMasks)
    {
        auto * pLwlIobist = lwrDevMask.first->GetInterface<LwLinkIobist>();
        pLwlIobist->SetType(lwrDevMask.second, LwLinkIobist::IobistType::Off);
        pLwlIobist->SetInitiator(lwrDevMask.second, false);
        rc = pLwlIobist->SetTime(lwrDevMask.second, LwLinkIobist::IobistTime::Iobt20us);
    }
    for (auto const & lwrDevMask : m_DeviceNelbLinkMasks)
    {
        auto * pLwLink = lwrDevMask.first->GetInterface<LwLink>();
        rc = pLwLink->SetNearEndLoopbackMode(lwrDevMask.second.neaLinkMask,
                                             LwLink::NearEndLoopMode::NEA);
        rc = pLwLink->SetNearEndLoopbackMode(lwrDevMask.second.nedLinkMask,
                                             LwLink::NearEndLoopMode::NED);
    }

    rc = SetLinkState(m_DeviceActiveLinkMasks, LwLink::SLS_HIGH_SPEED, false);

    for (auto & lwrPwrOwner : m_PowerStateOwners)
    {
        rc = lwrPwrOwner.Release();
    }

    return rc;
}

//-----------------------------------------------------------------------------
void LwLinkIobistTest::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tAllDevices:             %s\n", m_AllDevices ? "true" : "false");
    Printf(pri, "\tIsBgTest:               %s\n", m_IsBgTest ? "true" : "false");
    Printf(pri, "\tKeepRunning:            %s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "\tRuntimeMs:              %u\n", m_RuntimeMs);
    Printf(pri, "\tExtraIobistTimeoutMs:   %u\n", m_ExtraIobistTimeoutMs);
    Printf(pri, "\tNeaMode:                %s\n", LwLink::NearEndLoopModeToString(m_NeaMode));
    Printf(pri, "\tIobistType:             %s\n", LwLinkIobist::IobbistTypeToString(m_IobistType));
    Printf(pri, "\tIobistType:             %s\n", LwLinkIobist::IobbistTypeToString(m_IobistType));
    Printf(pri, "\tIobistTimeAc:           %s\n", LwLinkIobist::IobbistTimeToString(m_IobistTimeAc));
    Printf(pri, "\tIobistTimeDc:           %s\n", LwLinkIobist::IobbistTimeToString(m_IobistTimeDc));
    Printf(pri, "\tPreFecRateThresh:       %.6E\n", m_PreFecRateThresh);
    Printf(pri, "\tEccDecFailedRateThresh: %.6E\n", m_EccDecFailedRateThresh);
    Printf(pri, "\tLinkMask:               0x%llx\n", m_LinkMask);    
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::AclwmulateErrorCounts
(
    const map<TestDevicePtr, UINT64> & deviceTestMasks,
    UINT32                             loop
)
{
    RC rc;

    for (auto const & lwrDevMask : deviceTestMasks)
    {
        // If only running on the bound device in a loopback mode then only
        // clear and check errors on the bound device
        if (!m_AllDevices && (m_NeaMode != LwLink::NearEndLoopMode::None) &&
            (lwrDevMask.first != GetBoundTestDevice()))
        {
            continue;
        }

        LwLink::ErrorCounts errorCounts;
        auto * pLwLink = lwrDevMask.first->GetInterface<LwLink>();
        INT32 lwrLinkId = Utility::BitScanForward(lwrDevMask.second);
        for ( ; lwrLinkId != -1;
              lwrLinkId = Utility::BitScanForward(lwrDevMask.second, lwrLinkId + 1))
        {
            CHECK_RC(pLwLink->GetErrorCounts(lwrLinkId, &errorCounts));
            m_DeviceErrorCounts[lwrDevMask.first][lwrLinkId] += errorCounts;

            bool bAnyErrors = false;
            for (auto const & lwrErrId :s_ErrorIdsToCheck)
            {
                if (errorCounts.GetCount(lwrErrId))
                    bAnyErrors = true;
            }

            if (bAnyErrors)
            {
                const bool bLanesReversed =
                    lwrDevMask.first->GetInterface<LwLink>()->AreLanesReversed(lwrLinkId);
                const UINT32 sublinkWidth =
                    lwrDevMask.first->GetInterface<LwLink>()->GetSublinkWidth(lwrLinkId);

                string errString =
                    Utility::StrPrintf("%s : Errors detected on %s, link %d at loop %u\n",
                                       GetName().c_str(),
                                       lwrDevMask.first->GetName().c_str(),
                                       lwrLinkId,
                                       loop);
                errString += Utility::StrPrintf("  Remote Connection : %s\n",
                               m_DeviceRemoteConnections[lwrDevMask.first][lwrLinkId].c_str());

                for (auto const & lwrErrId :s_ErrorIdsToCheck)
                {
                    errString += Utility::StrPrintf("  %s",
                                                    LwLink::ErrorCounts::GetName(lwrErrId).c_str());
                    const UINT32 physLane = LwLink::ErrorCounts::GetLane(lwrErrId);
                    if (physLane != LwLink::ErrorCounts::LWLINK_ILWALID_LANE)
                    {
                        errString += Utility::StrPrintf(" (Logical Lane %u)",
                            bLanesReversed ? (sublinkWidth - physLane - 1) : physLane);
                    }
                    errString += Utility::StrPrintf(" : %llu\n", errorCounts.GetCount(lwrErrId));
                }
                VerbosePrintf("%s\n", errString.c_str());
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::CacheRemoteConnectionStrings()
{
    RC rc;
    for (auto const & lwrDevMask : m_DeviceActiveLinkMasks)
    {
        m_DeviceRemoteConnections[lwrDevMask.first] =
            vector<string>(lwrDevMask.first->GetInterface<LwLink>()->GetMaxLinks());
        INT32 lwrLinkId = Utility::BitScanForward(lwrDevMask.second);
        for ( ; lwrLinkId != -1;
              lwrLinkId = Utility::BitScanForward(lwrDevMask.second, lwrLinkId + 1))
        {
            if (m_NeaMode != LwLink::NearEndLoopMode::None)
            {
                m_DeviceRemoteConnections[lwrDevMask.first][lwrLinkId] =
                    Utility::StrPrintf("%s, link %d (%s)",
                                       lwrDevMask.first->GetName().c_str(),
                                       lwrLinkId,
                                       LwLink::NearEndLoopModeToString(m_NeaMode));
            }
            else
            {
                LwLink::EndpointInfo ep;
                TestDevicePtr pRemDev;
                CHECK_RC(lwrDevMask.first->GetInterface<LwLink>()->GetRemoteEndpoint(lwrLinkId,
                                                                                     &ep));
                CHECK_RC(DevMgrMgr::d_TestDeviceMgr->GetDevice(ep.id, &pRemDev));
                m_DeviceRemoteConnections[lwrDevMask.first][lwrLinkId] =
                    Utility::StrPrintf("%s, link %d", pRemDev->GetName().c_str(), ep.linkNumber);
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::CheckErrorRates(UINT32 totalLoops)
{
    StickyRC rc;

    auto IsLinkAcCoupled = [this] (TestDevicePtr pDev, UINT32 linkId) -> bool
    {
        if (m_DeviceAcLinkMasks.find(pDev) == m_DeviceAcLinkMasks.end())
            return false;

        return m_DeviceAcLinkMasks[pDev] & (1ULL << linkId);
    };

    for (auto const & lwrDevMask : m_DeviceActiveLinkMasks)
    {
        // If only running on the bound device in a loopback mode then only
        // clear and check errors on the bound device
        if (!m_AllDevices && (m_NeaMode != LwLink::NearEndLoopMode::None) &&
            (lwrDevMask.first != GetBoundTestDevice()))
        {
            continue;
        }
        
        auto const & deviceErrors = m_DeviceErrorCounts[lwrDevMask.first];

        for (size_t linkId = 0; linkId < deviceErrors.size(); linkId++)
        {
            bool bAnyErrorsValid = false;
            for (auto const & lwrErrId :s_ErrorIdsToCheck)
            {
                if (deviceErrors[linkId].IsValid(lwrErrId))
                    bAnyErrorsValid = true;
            }

            if (!bAnyErrorsValid)
                continue;

            const bool bAcCoupled = IsLinkAcCoupled(lwrDevMask.first, static_cast<UINT32>(linkId));
            const FLOAT64 iobistTimeMs =
                GetIobistTimeMs(bAcCoupled ? m_IobistTimeAc : m_IobistTimeDc);
            const FLOAT64 elapsedTimeSec =
                (static_cast<FLOAT64>(iobistTimeMs) / 1000000.0) * totalLoops;
            const bool bLanesReversed =
                lwrDevMask.first->GetInterface<LwLink>()->AreLanesReversed(linkId);
            const UINT32 sublinkWidth =
                lwrDevMask.first->GetInterface<LwLink>()->GetSublinkWidth(linkId);

            string errorSummaryStr =
                Utility::StrPrintf("%s : Errors detected on %s, link %u:\n",
                                   GetName().c_str(),
                                   lwrDevMask.first->GetName().c_str(),
                                   static_cast<UINT32>(linkId));
            errorSummaryStr += Utility::StrPrintf("  Remote Connection : %s\n",
                           m_DeviceRemoteConnections[lwrDevMask.first][linkId].c_str());

            bool bThreshExceeded = false;
            for (auto const & lwrErrId : s_ErrorIdsToCheck)
            {
                if (!deviceErrors[linkId].IsValid(lwrErrId))
                    continue;

                errorSummaryStr += Utility::StrPrintf("  %s",
                                                   LwLink::ErrorCounts::GetName(lwrErrId).c_str());
                const UINT32 physLane = LwLink::ErrorCounts::GetLane(lwrErrId);
                if (physLane != LwLink::ErrorCounts::LWLINK_ILWALID_LANE)
                {
                    errorSummaryStr += Utility::StrPrintf(" (Logical Lane %u)",
                        bLanesReversed ? (sublinkWidth - physLane - 1) : physLane);
                }

                const FLOAT64 ber =
                    static_cast<FLOAT64>(deviceErrors[linkId].GetCount(lwrErrId)) / elapsedTimeSec;

                FLOAT64 thresh = (lwrErrId == LwLink::ErrorCounts::LWL_ECC_DEC_FAILED_ID) ?
                    m_EccDecFailedRateThresh : m_PreFecRateThresh;
                errorSummaryStr +=
                    Utility::StrPrintf(" Bit Error Rate (expected) : %.6E (%.6E)", ber, thresh);
                if (ber > thresh)
                {
                    rc = RC::LWLINK_IOBIST_ERROR;
                    bThreshExceeded = true;
                }
            }
            Printf(bThreshExceeded ? Tee::PriError : GetVerbosePrintPri(),
                   "%s\n", errorSummaryStr.c_str());
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::CheckErrorFlags
(
    const map<TestDevicePtr, UINT64> & deviceTestMasks,
    UINT32                             loop
)
{
    StickyRC rc;
    for (auto const & lwrDevMask : deviceTestMasks)
    {
        // If only running on the bound device in a loopback mode then only
        // clear and check errors on the bound device
        if (!m_AllDevices && (m_NeaMode != LwLink::NearEndLoopMode::None) &&
            (lwrDevMask.first != GetBoundTestDevice()))
        {
            continue;
        }

        auto *pLwlIobist = lwrDevMask.first->GetInterface<LwLinkIobist>();

        set<LwLinkIobist::ErrorFlag> errorFlags;
        pLwlIobist->GetErrorFlags(lwrDevMask.second, &errorFlags);
        if (!errorFlags.empty())
        {
            rc = RC::LWLINK_IOBIST_ERROR;
            for (auto const & lwrErr : errorFlags)
            {
                Printf(Tee::PriError,
                       "%s : %s error detected during IOBIST on %s, link %u, loop %u\n",
                       GetName().c_str(),
                       LwLinkIobist::ErrorFlagTypeToString(lwrErr.errFlag),
                       lwrDevMask.first->GetName().c_str(),
                       lwrErr.linkId,
                       loop);
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::CheckLinkState
(
    const map<TestDevicePtr, UINT64> & deviceTestMasks,
    UINT32 loop
)
{
    RC rc;
    StickyRC linkStatusRc;;
    for (auto const & lwrDevMask : deviceTestMasks)
    {
        // If only running on the bound device in a loopback mode then only
        // clear and check errors on the bound device
        if (!m_AllDevices && (m_NeaMode != LwLink::NearEndLoopMode::None) &&
            (lwrDevMask.first != GetBoundTestDevice()))
        {
            continue;
        }

        // If the IOBIST sequence was successful, the links will end up in SWCFG/SAFE
        vector<LwLink::LinkStatus> linkStatus;
        CHECK_RC(lwrDevMask.first->GetInterface<LwLink>()->GetLinkStatus(&linkStatus));
        INT32 lwrLinkId = Utility::BitScanForward(lwrDevMask.second);
        for ( ; lwrLinkId != -1;
              lwrLinkId = Utility::BitScanForward(lwrDevMask.second, lwrLinkId + 1))
        {
            if ((linkStatus[lwrLinkId].linkState != LwLink::LS_SWCFG) ||
                (linkStatus[lwrLinkId].rxSubLinkState != LwLink::SLS_SAFE_MODE) ||
                (linkStatus[lwrLinkId].txSubLinkState != LwLink::SLS_SAFE_MODE))
            {
                Printf(Tee::PriError,
                       "%s : Unexpected link status during IOBIST on %s, link %u, loop %u"
                       "  Link State (expected)       : %s (%s)\n"
                       "  RX Sublink State (expected) : %s (%s)\n"
                       "  TX Sublink State (expected) : %s (%s)\n",
                       GetName().c_str(),
                       lwrDevMask.first->GetName().c_str(),
                       lwrLinkId,
                       loop,
                       LwLink::LinkStateToString(linkStatus[lwrLinkId].linkState).c_str(),
                       LwLink::LinkStateToString(LwLink::LS_SWCFG).c_str(),
                       LwLink::SubLinkStateToString(linkStatus[lwrLinkId].rxSubLinkState).c_str(),
                       LwLink::SubLinkStateToString(LwLink::SLS_SAFE_MODE).c_str(),
                       LwLink::SubLinkStateToString(linkStatus[lwrLinkId].txSubLinkState).c_str(),
                       LwLink::SubLinkStateToString(LwLink::SLS_SAFE_MODE).c_str());
                linkStatusRc = RC::LWLINK_TRAINING_ERROR;
            }
        }
    }
    return linkStatusRc;
}

//-----------------------------------------------------------------------------
FLOAT64 LwLinkIobistTest::GetIobistTimeMs(LwLinkIobist::IobistTime iobt) const
{
    switch (iobt)
    {
        case LwLinkIobist::IobistTime::Iobt20us:
            return 0.00002;
        case LwLinkIobist::IobistTime::Iobt800us:
            return 0.0008;
        case LwLinkIobist::IobistTime::Iobt1s:
            return 1000.0;
        case LwLinkIobist::IobistTime::Iobt10s:
            return 10000.0;
        default:
            MASSERT(!"Unknown IOBIST time");
            break;
    }
    return 0.00002;
}

//-----------------------------------------------------------------------------
// Reference section 12.1 of the following document for the core of the procedure
//     https://p4viewer.lwpu.com/get/hw/lwgpu_gh100_lwlink/ip/lwif/lwlink/4.0/doc/arch/lwlipt/publish/released/lwlipt_4P0_Full.html
RC LwLinkIobistTest::RunIobist
(
    const map<TestDevicePtr, UINT64> & deviceTestMasks,
    CouplingType                       couplingType,
    UINT32                             loop
)
{
    RC rc;
    const LwLinkIobist::IobistTime iobt = (couplingType == CouplingType::AC) ? m_IobistTimeAc :
                                                                               m_IobistTimeDc;

    // First clear all the error counts - for the purposes of this test we only want to
    // accumulate errors during the IOBIST sequence
    const bool bStopOnError = GetGoldelwalues()->GetStopOnError();
    for (auto const &lwrDevMask : deviceTestMasks)
    {
        // If only running on the bound device in a loopback mode then only
        // clear and check errors on the bound device
        if (!m_AllDevices && (m_NeaMode != LwLink::NearEndLoopMode::None) &&
            (lwrDevMask.first != GetBoundTestDevice()))
        {
            continue;
        }

        auto * pLwLink = GetBoundTestDevice()->GetInterface<LwLink>();
        INT32 lwrLinkId = Utility::BitScanForward(lwrDevMask.second);
        for ( ; lwrLinkId != -1;
              lwrLinkId = Utility::BitScanForward(lwrDevMask.second, lwrLinkId + 1))
        {
            CHECK_RC(pLwLink->ClearErrorCounts(lwrLinkId));
        }
    }

    // IOBIST has already been configured, triggering the HS mode transition will start the
    // IOBIST process
    CHECK_RC(SetLinkState(deviceTestMasks, LwLink::SLS_HIGH_SPEED,
                          m_NeaMode != LwLink::NearEndLoopMode::None));

    const UINT32 iobistTimeMs = GetIobistTimeMs(iobt) + m_ExtraIobistTimeoutMs +
        ((m_IobistType == LwLinkIobist::IobistType::PreTrain) ? IOBIST_ECC_PRETRAIN_TIMEOUT_MS :
                                                                IOBIST_ECC_POSTTRAIN_TIMEOUT_MS);

    Tasker::Sleep(iobistTimeMs);

    // Validate that the link state is correct and no IOBIST error flags are present
    m_DeferredRc = CheckLinkState(deviceTestMasks, loop);
    if (bStopOnError)
    {
        CHECK_RC(m_DeferredRc);
    }

    m_DeferredRc = CheckErrorFlags(deviceTestMasks, loop);
    if (bStopOnError)
    {
        CHECK_RC(m_DeferredRc);
    }
    CHECK_RC(AclwmulateErrorCounts(deviceTestMasks, loop));

    // Train back to off in preparation for the next loop
    CHECK_RC(SetLinkState(deviceTestMasks, LwLink::SLS_OFF,
                          m_NeaMode != LwLink::NearEndLoopMode::None));

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::SetLinkState
(
    const map<TestDevicePtr, UINT64> & deviceLinkMasks,
    LwLink::SubLinkState               state,
    bool                               bAllDevices                
)
{
    StickyRC rc;
    if (m_AllDevices)
    {
        for (auto & lwrDevMask : deviceLinkMasks)
        {
            if (!bAllDevices && m_bSwitchFabric &&
                (lwrDevMask.first->GetType() != Device::TYPE_LWIDIA_LWSWITCH))
            {
                continue;
            }

            auto * pLwLink = lwrDevMask.first->GetInterface<LwLink>();
            rc = pLwLink->SetLinkState(lwrDevMask.second, state);
        }
    }
    else
    {
        auto * pLwLink = GetBoundTestDevice()->GetInterface<LwLink>();
        rc = pLwLink->SetLinkState(deviceLinkMasks.at(GetBoundTestDevice()), state);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkIobistTest::SetupIobist()
{
    RC rc;

    auto IsLinkAcCoupled = [this] (TestDevicePtr pDev, UINT32 linkId) -> bool
    {
        if (m_DeviceAcLinkMasks.find(pDev) == m_DeviceAcLinkMasks.end())
            return false;

        return m_DeviceAcLinkMasks[pDev] & (1ULL << linkId);
    };

    map<TestDevicePtr, set<UINT32>> iobistLinksConfigured;
    for (auto const & lwrDevMask : m_DeviceActiveLinkMasks)
    {
        auto * pLwLink = lwrDevMask.first->GetInterface<LwLink>();
        CHECK_RC(pLwLink->SetNearEndLoopbackMode(lwrDevMask.second, m_NeaMode));

        auto *pLwlIobist = lwrDevMask.first->GetInterface<LwLinkIobist>();

        // If there is only one type of coupling configure the IOBIST time for all links at once,
        // othewise defer until the per-link loops
        pLwlIobist->SetType(lwrDevMask.second, m_IobistType);
        if (m_CouplingType == CouplingType::DC)
        {
            CHECK_RC(pLwlIobist->SetTime(lwrDevMask.second, m_IobistTimeDc));
        }
        else if (m_CouplingType == CouplingType::AC)
        {
            CHECK_RC(pLwlIobist->SetTime(lwrDevMask.second, m_IobistTimeAc));
        }

        if (m_AllDevices)
        {
            INT32 lwrLinkId = Utility::BitScanForward(lwrDevMask.second);
            for ( ; lwrLinkId != -1;
                  lwrLinkId = Utility::BitScanForward(lwrDevMask.second, lwrLinkId + 1))
            {
                if (m_CouplingType == CouplingType::Mixed)
                {
                    const bool bAcCoupled = IsLinkAcCoupled(lwrDevMask.first, lwrLinkId);
                    CHECK_RC(pLwlIobist->SetTime((1ULL << lwrLinkId),
                                                 bAcCoupled ? m_IobistTimeAc : m_IobistTimeDc));
                }

                // Initiator needs to be configured differently on each end of the
                // link so keep track of which links have already had the initiator
                // configured, skipping any links that are already configured
                if (iobistLinksConfigured.count(lwrDevMask.first) &&
                    iobistLinksConfigured[lwrDevMask.first].count(lwrLinkId))
                {
                    continue;
                }
                if (!iobistLinksConfigured.count(lwrDevMask.first))
                {
                    iobistLinksConfigured[lwrDevMask.first] = set<UINT32>();
                }
                iobistLinksConfigured[lwrDevMask.first].insert(lwrLinkId);
                pLwlIobist->SetInitiator((1ULL << lwrLinkId), true);

                LwLink::EndpointInfo ep;
                TestDevicePtr pRemDev;
                CHECK_RC(pLwLink->GetRemoteEndpoint(lwrLinkId, &ep));
                CHECK_RC(DevMgrMgr::d_TestDeviceMgr->GetDevice(ep.id, &pRemDev));

                // If this is loopback no need to set the initiator
                if ((pRemDev == lwrDevMask.first) && (ep.linkNumber = lwrLinkId))
                    continue;

                auto *pRemLwLinkIobist = pRemDev->GetInterface<LwLinkIobist>();
                if (!iobistLinksConfigured.count(pRemDev))
                {
                    iobistLinksConfigured[pRemDev] = set<UINT32>();
                }
                iobistLinksConfigured[pRemDev].insert(ep.linkNumber);
                pRemLwLinkIobist->SetInitiator((1ULL << ep.linkNumber), false);
            }
        }
        else
        {
            if (m_CouplingType == CouplingType::Mixed)
            {
                INT32 lwrLinkId = Utility::BitScanForward(lwrDevMask.second);
                for ( ; lwrLinkId != -1;
                      lwrLinkId = Utility::BitScanForward(lwrDevMask.second, lwrLinkId + 1))
                {
                    const bool bAcCoupled = IsLinkAcCoupled(lwrDevMask.first, lwrLinkId);
                    CHECK_RC(pLwlIobist->SetTime((1ULL << lwrLinkId),
                                                 bAcCoupled ? m_IobistTimeAc : m_IobistTimeDc));
                }
            }
            pLwlIobist->SetInitiator(lwrDevMask.second, lwrDevMask.first == GetBoundTestDevice());
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
// Create the link masks that will be tested
RC LwLinkIobistTest::SetupLinkMasks()
{
    RC rc;
    TestDevicePtr pDev = GetBoundTestDevice();

    if (m_AllDevices)
    {
        for (auto pTestDevice : *DevMgrMgr::d_TestDeviceMgr)
        {
            auto * pLwLink = pTestDevice->GetInterface<LwLink>();
            if (pLwLink != nullptr)
            {
                if (!m_bSwitchFabric && (pTestDevice->GetType() == Device::TYPE_LWIDIA_LWSWITCH))
                    m_bSwitchFabric = true;

                UINT64 acLinkMask = 0;
                UINT64 dcLinkMask = 0;
                for (size_t lwrId = 0; lwrId < pLwLink->GetMaxLinks(); lwrId++)
                {
                    if (pLwLink->IsLinkActive(lwrId))
                    {
                        if (pLwLink->IsLinkAcCoupled(lwrId))
                            acLinkMask |= (1ULL << lwrId);
                        else
                            dcLinkMask |= (1ULL << lwrId);
                    }
                }

                // If the coupling type is already Mixed, no need to perform further checking
                if (m_CouplingType != CouplingType::Mixed)
                {
                    if (acLinkMask && dcLinkMask)
                    {
                        m_CouplingType = CouplingType::Mixed;
                    }
                    else if (acLinkMask)
                    {
                        m_CouplingType = (m_CouplingType == CouplingType::DC) ?
                                             CouplingType::Mixed : CouplingType::AC;
                    }
                    else if (dcLinkMask)
                    {
                        m_CouplingType = (m_CouplingType == CouplingType::AC) ?
                                             CouplingType::Mixed : CouplingType::DC;
                    }
                }

                m_DeviceActiveLinkMasks[pTestDevice] = (acLinkMask | dcLinkMask);
                m_DeviceAcLinkMasks[pTestDevice] = acLinkMask;
                m_DeviceDcLinkMasks[pTestDevice] = dcLinkMask;

                // Save off the state of the current near end settings for the links
                // so that they can be restored after the test
                if (m_NeaMode != LwLink::NearEndLoopMode::None)
                {
                    m_DeviceNelbLinkMasks[pTestDevice] =
                    {
                        pLwLink->GetNeaLoopbackLinkMask(),
                        pLwLink->GetNedLoopbackLinkMask()
                    };
                }
            }
        }
    }
    else
    {
        m_DeviceActiveLinkMasks[pDev] = 0;
        m_bSwitchFabric = (pDev->GetType() == Device::TYPE_LWIDIA_LWSWITCH);

        auto * pLwLink = pDev->GetInterface<LwLink>();
        UINT64 acLinkMask = 0;
        UINT64 dcLinkMask = 0;
        for (size_t lwrId = 0; lwrId < pLwLink->GetMaxLinks(); lwrId++)
        {
            if (!(m_LinkMask & (1ULL << lwrId)))
                continue;

            if (pLwLink->IsLinkActive(lwrId))
            {
                m_DeviceActiveLinkMasks[pDev] |= (1ULL << lwrId);

                const bool bAcCoupled = pLwLink->IsLinkAcCoupled(lwrId);
                if (bAcCoupled)
                    acLinkMask |= (1ULL << lwrId);
                else
                    dcLinkMask |= (1ULL << lwrId);

                LwLink::EndpointInfo ep;
                TestDevicePtr pRemDev;
                CHECK_RC(pLwLink->GetRemoteEndpoint(lwrId, &ep));
                CHECK_RC(DevMgrMgr::d_TestDeviceMgr->GetDevice(ep.id, &pRemDev));
                if (pRemDev != pDev)
                {
                    if (!m_bSwitchFabric &&
                        (pRemDev->GetType() == Device::TYPE_LWIDIA_LWSWITCH))
                    {
                        m_bSwitchFabric = true;
                    }

                    const UINT64 lwrLinkMask = (1ULL << ep.linkNumber);
                    if (m_DeviceActiveLinkMasks.count(pRemDev))
                    {
                        if (bAcCoupled)
                            m_DeviceAcLinkMasks[pRemDev] |= lwrLinkMask;
                        else
                            m_DeviceDcLinkMasks[pRemDev] |= lwrLinkMask;

                        m_DeviceActiveLinkMasks[pRemDev] |= lwrLinkMask;
                    }
                    else
                    {
                        if (bAcCoupled)
                            m_DeviceAcLinkMasks[pRemDev] = lwrLinkMask;
                        else
                            m_DeviceDcLinkMasks[pRemDev] = lwrLinkMask;

                        m_DeviceActiveLinkMasks[pRemDev] = lwrLinkMask;
                    }
                }
            }
        }
        m_DeviceAcLinkMasks[pDev] = acLinkMask;
        m_DeviceDcLinkMasks[pDev] = dcLinkMask;
        if (acLinkMask && dcLinkMask)
            m_CouplingType = CouplingType::Mixed;
        else if (acLinkMask)
            m_CouplingType = CouplingType::AC;
        else if (dcLinkMask)
            m_CouplingType = CouplingType::DC;
        if (m_NeaMode != LwLink::NearEndLoopMode::None)
        {
            m_DeviceNelbLinkMasks[pDev] =
            {
                pLwLink->GetNeaLoopbackLinkMask(),
                pLwLink->GetNedLoopbackLinkMask()
            };
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_INHERIT(LwLinkIobistTest, LwLinkTest, "LwLink IOBIST test");
CLASS_PROP_READWRITE(LwLinkIobistTest, AllDevices, bool,
                     "Run the test across all devices (default = false)");
CLASS_PROP_READWRITE(LwLinkIobistTest, IsBgTest, bool,
                     "Whether the test is being run in the background (default = false)");
CLASS_PROP_READWRITE(LwLinkIobistTest, KeepRunning, bool,
                     "Whether to keep the test running (default = false)");
CLASS_PROP_READWRITE(LwLinkIobistTest, RuntimeMs, UINT32,
                     "Time to remain in IOBIST mode (default = 5000)");
CLASS_PROP_READWRITE(LwLinkIobistTest, NeaMode, UINT08,
                     "Near end loopback mode to use, 0=None, 1=NEA, 2=NED (default = None)");
CLASS_PROP_READWRITE(LwLinkIobistTest, IobistType, UINT08,
                     "IOBIST type to perform, 1=PreTrain, 2=PostTrain (default = 1)");
CLASS_PROP_READWRITE(LwLinkIobistTest, ExtraIobistTimeoutMs, UINT32,
                     "Extra time in ms to wait for IOBIST to complete (default = 0)");
CLASS_PROP_READWRITE(LwLinkIobistTest, IobistTimeAc, UINT08,
    "Time to use for IOBIST on AC coupled links (0=20us, 1=800us, 2=1s, 3=10s) (default = auto detect)");
CLASS_PROP_READWRITE(LwLinkIobistTest, IobistTimeDc, UINT08,
    "Time to use for IOBIST on AC coupled links (0=20us, 1=800us) (default = auto detect)");
CLASS_PROP_READWRITE(LwLinkIobistTest, PreFecRateThresh, FLOAT64,
    "Threshold to apply for PreFEC error rate (default = device default)");
CLASS_PROP_READWRITE(LwLinkIobistTest, EccDecFailedRateThresh, FLOAT64,
    "Threshold to apply for ECC DEC Failed error rate (default = device default)");

JS_STEST_LWSTOM(LwLinkIobistTest, SetLinkMask, 1,
                "Set the link mask for iobist (default = all links)")
{
    STEST_HEADER(1, 1, "Usage: LwLinkState.SetLinkMask(linkMask)");
    STEST_PRIVATE(LwLinkIobistTest, pObj, nullptr);
    STEST_ARG(0, JSObject *, pJsLinkMaskObj);
    JSClass* pJsClass = JS_GetClass(pJsLinkMaskObj);
    if (!(pJsClass->flags & JSCLASS_HAS_PRIVATE) || strcmp(pJsClass->name, "UINT64") != 0)
    {
        Printf(Tee::PriError, "SetLinkMask expects a UINT64 JS object as a parameter\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }
    JsUINT64* pJsUINT64 = static_cast<JsUINT64*>(JS_GetPrivate(pContext, pJsLinkMaskObj));
    pObj->SetLinkMask(pJsUINT64->GetValue());
    RETURN_RC(RC::OK);
}
