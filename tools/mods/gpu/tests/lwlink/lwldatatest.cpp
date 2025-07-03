/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */


#include "lwldatatest.h"

#include "core/include/errormap.h"
#include "core/include/fpicker.h"
#include "core/include/golden.h"
#include "core/include/mgrmgr.h"
#include "core/include/mle_protobuf.h"
#include "core/include/tar.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl2080.h"

#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlsleepstate.h"

#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/lwlink/lwlinkimpl.h"

#include "gpu/tests/gputestc.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_alloc_mgr.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_test_mode.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_test_route.h"

#include "gpu/uphy/uphyreglogger.h"

#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_impl.h"
#include "gpu/utility/thermalslowdown.h"
#include "gpu/utility/thermalthrottling.h"

#include "jsinterp.h"

#if defined(INCLUDE_LWSWITCH)
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_protobuf.h"
#endif

#include <array>
#include <vector>

#include <boost/range/algorithm.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/spirit/include/qi.hpp>

using namespace boost::spirit;

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Function for sorting the test modes by direction then by number
    //! of routes
    //!
    //! \param m1 : Test mode 1
    //! \param m1 : Test mode 2
    //!
    //! \return true if test mode 1 is less than test mode 2, false otherwise
    bool SortTestModes
    (
         LwLinkDataTestHelper::TestModePtr m1
        ,LwLinkDataTestHelper::TestModePtr m2
    )
    {
        if (m1->GetTransferDir() != m2->GetTransferDir())
            return m1->GetTransferDir() < m2->GetTransferDir();
        if (m1->GetSysmemTransferDir() != m2->GetSysmemTransferDir())
            return m1->GetSysmemTransferDir() < m2->GetSysmemTransferDir();
        return m1->GetNumRoutes() < m2->GetNumRoutes();
    }

    //--------------------------------------------------------------------------
    //! \brief Function for sorting the test routes in the way that they should
    //! be used given no other restrictions.  Sysmem routes get priority
    //! followed by routes with longer lengths to test more connections
    //!
    //! \param m1 : Test route 1
    //! \param m1 : Test route 2
    //!
    //! \return true if test route 1 is less than test route 2, false otherwise
    bool SortRoutes
    (
        LwLinkRoutePtr pRoute1,
        LwLinkRoutePtr pRoute2
    )
    {
        if (pRoute1->IsSysmem() != pRoute2->IsSysmem())
            return pRoute1->IsSysmem();

        return pRoute1->GetMaxLength(pRoute1->GetEndpointDevices().first,
                                     LwLink::DT_REQUEST) >
               pRoute2->GetMaxLength(pRoute2->GetEndpointDevices().first,
                                     LwLink::DT_REQUEST);
    }

    RC IsHwLowPowerModeEnabled(TestDevicePtr pTestDevice, bool *pbSupported)
    {
        StickyRC rc;
        auto pLwLink = pTestDevice->GetInterface<LwLink>();
        auto pPowerState = pLwLink->GetInterface<LwLinkPowerState>();

        *pbSupported = true;
        for (UINT32 lwrLink = 0; lwrLink < pLwLink->GetMaxLinks(); lwrLink++)
        {
            LwLinkPowerState::LinkPowerStateStatus lpws = { };
            RC lwrRc = pPowerState->GetLinkPowerStateStatus(lwrLink, &lpws);
            if (OK == lwrRc)
            {
                *pbSupported = lpws.rxHwControlledPowerState &&
                               lpws.txHwControlledPowerState;
            }
            rc = lwrRc;
        }

        return rc;
    }

    void GetConnectionsOnRoute
    (
        LwLinkRoutePtr            pRoute,
        TestDevicePtr             pFromEndpoint,
        LwLink::DataType          dt,
        set<LwLinkConnectionPtr> *pCons
    )
    {
        MASSERT(pCons);

        pCons->clear();

        auto pathIt = pRoute->PathsBegin(pFromEndpoint, dt);
        auto pathEnd = pRoute->PathsEnd(pFromEndpoint, dt);

        for (; pathIt != pathEnd; ++pathIt)
        {
            auto peIt = pathIt->PathEntryBegin(pFromEndpoint);
            auto peEnd = LwLinkPath::PathEntryIterator();
            for (; peIt != peEnd; ++peIt)
            {
                pCons->insert(peIt->pCon);
            }
        }
    }
}

using namespace MfgTest;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
MfgTest::LwLinkDataTest::LwLinkDataTest()
 :  m_NumErrorsToPrint(10)
   ,m_BwThresholdPct(1.0)
   ,m_TransferDirMask(LwLinkDataTestHelper::TD_IN_OUT)
   ,m_TransferTypeMask(LwLinkDataTestHelper::TT_ALL)
   ,m_MaxSimulRoutes(ALL_ROUTES)
   ,m_MinSimulRoutes(ALL_ROUTES)
   ,m_LwLinkIdMask(ALL_LINKS)
   ,m_ShowTestModes(false)
   ,m_ShowBandwidthData(false)
   ,m_CopyLoopCount(1)
   ,m_DumpSurfaceOnMiscompare(false)
   ,m_SkipBandwidthCheck(false)
   ,m_SkipSurfaceCheck(false)
   ,m_SysmemLoopback(false)
   ,m_PauseTestMask(LwLinkDataTestHelper::TestMode::PAUSE_NONE)
   ,m_PerModeRuntimeSec(0)
   ,m_RuntimeMs(0)
   ,m_TestAllGpus(false)
   ,m_ShowTestProgress(false)
   ,m_SkipLinkStateCheck(false)
   ,m_FailOnSysmemBw(false)
   ,m_AddCpuTraffic(false)
   ,m_NumCpuTrafficThreads(0)
   ,m_TogglePowerState(false)
   ,m_TogglePowerStateIntMs(0.1)
   ,m_LPCountCheckTol(1)
   ,m_LPCountCheckTolPct(-1.0)
   ,m_SkipLPCountCheck(false)
   ,m_MinLPCount(0)
   ,m_LowPowerBandwidthCheck(false)
   ,m_CheckHwPowerStateSwitch(false)
   ,m_ShowLowPowerCheck(false)
   ,m_IdleAfterCopyMs(100)
   ,m_ThermalThrottling(false)
   ,m_ThermalThrottlingOnCount(0)
   ,m_ThermalThrottlingOffCount(0)
   ,m_ThermalSlowdown(false)
   ,m_ThermalSlowdownPeriodUs(500000)
   ,m_LoopbackTestType(LBT_READ_WRITE)
   ,m_ShowPortStats(false)
   ,m_SkipPortStatsCheck(true)
   ,m_LowLatencyThresholdNs(10)
   ,m_MidLatencyThresholdNs(195)
   ,m_HighLatencyThresholdNs(1014)
   ,m_IsBgTest(false)
   ,m_KeepRunning(false)
   ,m_JSConstructHook(JavaScriptPtr()->Runtime(), this)
{
    SetName("LwLinkDataTest");
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
}

void *MfgTest::LwLinkDataTest::JSConstructHook::Hook
(
    JSContext *cx
  , JSStackFrame *fp
  , JSBool before
  , JSBool *ok
)
{
    if (m_This->GetJSObject() == fp->thisp && JS_FALSE == before && JS_TRUE == *ok)
    {
        // Create a JS object for m_DataFlow. We need it, because if a user specified the
        // correspondent object on command line or in a spec file, TestListCopyObject will try to
        // copy all properties from the test description property DataFlow to this test DataFlow
        // object. We need an object to copy the properties to.
        auto dataFlowObj = JS_NewObject(cx, nullptr, nullptr, nullptr);
        auto dataFlowObjJval = OBJECT_TO_JSVAL(dataFlowObj);

        // Set m_DataFlow via the JS API instead of assigning directly, so the engine knows the new
        // object is reachable and cannot be garbage collected.
        JS_SetProperty(cx, m_This->GetJSObject(), "DataFlow", &dataFlowObjJval);

        ShutDownHook();
    }
    else if (m_This->GetJSObject() == fp->thisp && JS_FALSE == before && JS_FALSE == *ok)
    {
        // there was an error in the constructor, just shutdown the hook
        ShutDownHook();
    }

    return GetHead();
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
MfgTest::LwLinkDataTest::~LwLinkDataTest()
{
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
/* virtual */ bool MfgTest::LwLinkDataTest::IsSupported()
{
    if (!LwLinkTest::IsSupported())
        return false;

    // Unsupported on SLI
    if (GetBoundGpuDevice() && GetBoundGpuDevice()->GetNumSubdevices() > 1)
        return false;

    TestDevicePtr pTestDevice = GetBoundTestDevice();

    if (m_ThermalThrottling || m_ThermalSlowdown)
    {
        TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
        if (pTestDeviceMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH) == 0)
            return false;

        TestDevicePtr pDev;
        pTestDeviceMgr->GetDevice(TestDevice::TYPE_LWIDIA_LWSWITCH, 0, &pDev);
        if (m_ThermalThrottling && !pDev->SupportsInterface<ThermalThrottling>())
            return false;
        if (m_ThermalSlowdown && !pDev->SupportsInterface<ThermalSlowdown>())
            return false;
    }

    const bool hasLPFeature = GetBoundTestDevice()->HasFeature(Device::GPUSUB_LWLINK_LP_SLM_MODE_POR) ||
                              GetBoundTestDevice()->HasFeature(Device::GPUSUB_LWLINK_L1_MODE_POR);
    if ((!pTestDevice->SupportsInterface<LwLinkPowerState>() || !hasLPFeature) &&
        (m_TogglePowerState || m_CheckHwPowerStateSwitch))
    {
        return false;
    }
    
    if (!pTestDevice->HasFeature(Device::GPUSUB_LWLINK_LP_SLM_MODE_POR) &&
        m_LowPowerBandwidthCheck)
    {
        // LowPowerBandwidthCheck requires SLM
        return false;
    }

    if (m_L2AfterCopy)
    {
        if (!pTestDevice->SupportsInterface<LwLinkSleepState>())
        {
            VerbosePrintf("%s : Skipping because L2 is not supported!\n", GetName().c_str());

            return false;
        }

        if (GCxPwrWait::GCxModeDisabled != m_GCxInL2)
        {
            GCxBubble gcxBubble(GetBoundGpuSubdevice());
            if (!gcxBubble.IsGCxSupported(m_GCxInL2, Perf::ILWALID_PSTATE))
            {
                VerbosePrintf
                (
                    "%s : Skipping because GCx mode %u is not supported!\n",
                    GetName().c_str(),
                    m_GCxInL2
                );
                return false;
            }
        }
    }

    // SW driven tests depend on RM correctly sending Minion commands which is only done if
    // HW driven mode is actually enabled in the VBIOS.
    if (m_CheckHwPowerStateSwitch)
    {
        bool bHwPowerStateSwitchSupported = true;
        RC rc = IsHwLowPowerModeEnabled(pTestDevice, &bHwPowerStateSwitchSupported);

        if (!bHwPowerStateSwitchSupported)
        {
            VerbosePrintf("%s : Skipping because not all links have HW LP mode enabled!\n",
                          GetName().c_str());
            return false;
        }

        if (OK != rc)
        {
            Printf(Tee::PriWarn,
                   "%s : Unable to determine if low power mode is enabled on all links, "
                   "test may fail\n",
                   GetName().c_str());
        }
    }

    return true;
}

//------------------------------------------------------------------------------
//! \brief Setup the test
//!
//! \return OK if setup succeeds, not OK otherwise
/* virtual */ RC MfgTest::LwLinkDataTest::Setup()
{
    RC rc;

    // Ensure that there are at least some routes where the remote device
    // is not part of an SLI device
    vector<LwLinkRoutePtr> routes;
    CHECK_RC(GetLwLinkRoutes(GetBoundTestDevice(), &routes));

    // This check was moved from LwLinkDataTest::IsSupported to avoid a test
    // escape when we don't have supported test routes, but they should be. See
    // bug 2188798.
    bool bAtLeastOneNonSLIRoute = false;
    TestDevicePtr pTestDevice = GetBoundTestDevice();
    for (size_t ii = 0; ii < routes.size(); ii++)
    {
        if (routes[ii]->IsP2P() && pTestDevice->GetType() == TestDevice::TYPE_LWIDIA_GPU)
        {
            TestDevicePtr pRemLwLinkDev = routes[ii]->GetRemoteEndpoint(pTestDevice);
            GpuSubdevice *pRemDev = pRemLwLinkDev->GetInterface<GpuSubdevice>();

            if (pRemDev->GetParentDevice()->GetNumSubdevices() == 1)
                bAtLeastOneNonSLIRoute = true;
        }
        else
        {
            bAtLeastOneNonSLIRoute = true;
        }
    }

    bool bSysmem = false;
    if (m_AddCpuTraffic && (OK != SysmemUsesLwLink(&bSysmem) || !bSysmem))
    {
        return RC::HW_ERROR;
    }

    // If all remote devices are SLI devices, fail the test
    if (!bAtLeastOneNonSLIRoute)
    {
        Printf
        (
            Tee::PriError,
            "%s failed get LwLink routes that connect not SLI devices.\n",
            GetName().c_str()
        );
    }


    if (GetUseDataFlowObj())
    {
        if (!m_DataFlowFile.empty())
        {
            // If both DataFlow and DataFlowFile are specified, a warning will
            // be printed by `SetupLwLinkDataTestCommon` in gpudecls.js, ignore
            // it here.
            JavaScriptPtr js;
            string jsonStr;
            // If UseOptimal is true, the correspondent JSON file is located
            // next to the topology file.
            if (m_UseOptimal)
            {
                vector<char> jsonData;
                if (Xp::DoesFileExist(m_DataFlowFile))
                {
                    CHECK_RC(Utility::ReadPossiblyEncryptedFile(m_DataFlowFile, &jsonData, nullptr));
                }
                else
                {
                    const bool bOnLwNetwork =
                        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK);
                    if (bOnLwNetwork)
                    {
                        UINT32 size = 0;
                        if (RC::OK == (rc = Utility::ReadTarFile("lwlinktopofiles.INTERNAL.bin", m_DataFlowFile.c_str(), &size, nullptr)))
                        {
                            jsonData.resize(size);
                            rc = Utility::ReadTarFile("lwlinktopofiles.INTERNAL.bin", m_DataFlowFile.c_str(), &size, &jsonData[0]);
                        }
                    }

                    if ((rc == RC::FILE_DOES_NOT_EXIST) || !bOnLwNetwork)
                    {
                        rc.Clear();
                        UINT32 size = 0;
                        if (RC::OK == (rc = Utility::ReadTarFile("lwlinktopofiles.bin", m_DataFlowFile.c_str(), &size, nullptr)))
                        {
                            jsonData.resize(size);
                            rc = Utility::ReadTarFile("lwlinktopofiles.bin", m_DataFlowFile.c_str(), &size, &jsonData[0]);
                        }
                    }

                    if (rc == RC::FILE_DOES_NOT_EXIST)
                    {
                        Printf(Tee::PriError,
                               "Data flow file %s not found in lwlinktopofiles.bin%s!\n",
                               m_DataFlowFile.c_str(),
                               bOnLwNetwork ? " or lwlinktopofiles.INTERNAL.bin" : "");
                    }
                    else if (rc != RC::OK)
                    {
                        Printf(Tee::PriError,
                               "Unable to read data flow file %s from archive!\n",
                               m_DataFlowFile.c_str());
                    }
                    CHECK_RC(rc);
                }
                jsonStr.assign(jsonData.cbegin(), jsonData.cend());
            }
            else
            {
                vector<char> jsonData;
                CHECK_RC(Utility::ReadPossiblyEncryptedFile(m_DataFlowFile, &jsonData, nullptr));
                jsonStr.assign(jsonData.cbegin(), jsonData.cend());
            }

            // Call JSON.parse
            JSObject *jsonObj;
            jsval jsonJsval;
            jsval parseResult;
            CHECK_RC(js->ToJsval(jsonStr, &jsonJsval));
            CHECK_RC(js->GetProperty(js->GetGlobalObject(), "JSON", &jsonObj));
            // JSON.parse can throw a syntax error exception, it will be
            // processed by `JS_CallFunction`, don't bother.
            CHECK_RC(js->CallMethod(jsonObj, "parse", {jsonJsval}, &parseResult));
            // Use JS to set m_DataFlow, so the engine knows about our object.
            CHECK_RC(js->SetPropertyJsval(GetJSObject(), "DataFlow", parseResult));
        }
        // Read it first, so it is reflected in PrintJsProperties
        CHECK_RC(ReadDataFlowFromJSObj());
    }

    CHECK_RC(LwLinkTest::Setup());

    if (m_TestAllGpus)
    {
        // LwLink P2P (and loopback) links are not trained to HS mode and LCE/PCE
        // mapping is not configured correctly for the lwlink topology until a P2P
        // connection is made by allocating a peer mapping.  Force allocate a peer
        // mapping to ensure P2P links are in HS mode.  Peer mappings for the bound
        // device were already created in LwLinkTest::Setup() when TestAllGpus is
        // selected, ensure that P2P mappings are created between all devices.
        //
        // Creating mappings between GPUs that already have a mapping is a No-op
        vector<LwLinkRoutePtr> routes;
        GpuDevMgr * pMgr = (GpuDevMgr*) DevMgrMgr::d_GraphDevMgr;

        CHECK_RC(GetLwLinkRoutes(nullptr, &routes));

        TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
        const bool bLogUphy =
            ((pTestDeviceMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH) == 0)&&
             (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::PostLinkTraining));
        set<TestDevicePtr> uphyLogUntrainedLinksDevices;

        auto LogUphyOnUntrainedLinks = [&] () -> RC
        {
            StickyRC uphyLogRc;
            if (bLogUphy)
            {
                for (auto const & lwrDevice : uphyLogUntrainedLinksDevices)
                {
                    uphyLogRc = UphyRegLogger::LogRegs(lwrDevice,
                                                       UphyRegLogger::UphyInterface::LwLink,
                                                       UphyRegLogger::PostLinkTraining,
                                                       rc);
                }
            }
            return uphyLogRc;
        };
        DEFERNAME(logUphy)
        {
            LogUphyOnUntrainedLinks();
        };
        for (size_t ii = 0; ii < routes.size(); ii++)
        {
            if (routes[ii]->IsP2P())
            {
                LwLinkConnection::EndpointDevices endpoints =
                    routes[ii]->GetEndpointDevices();
                GpuSubdevice *pSubdev1 = endpoints.first->GetInterface<GpuSubdevice>();
                GpuSubdevice *pSubdev2 = endpoints.second->GetInterface<GpuSubdevice>();

                if (bLogUphy)
                {
                    CHECK_RC(AddUphyLogUntrainedLinksDevice(endpoints.first,
                                                            &uphyLogUntrainedLinksDevices));
                    CHECK_RC(AddUphyLogUntrainedLinksDevice(endpoints.second,
                                                            &uphyLogUntrainedLinksDevices));
                }

                Printf(Tee::PriLow, "Creating P2P mapping between GPUs %s and %s\n",
                       pSubdev1->GpuIdentStr().c_str(), pSubdev2->GpuIdentStr().c_str());

                // Peer mapping is a training point, part of the UPHY purpose is to diagnose
                // failures, so do not immediately exit on peer mapping failure and allow the
                // uphy dump to occur
                CHECK_RC(pMgr->AddPeerMapping(GetBoundRmClient(),
                                              pSubdev1->GetInterface<GpuSubdevice>(),
                                              pSubdev2->GetInterface<GpuSubdevice>()));
            }
        }

        logUphy.Cancel();
        CHECK_RC(LogUphyOnUntrainedLinks());
    }

    CHECK_RC(ValidateTestSetup());

    m_pAllocMgr = CreateAllocMgr();
    MASSERT(m_pAllocMgr);
    if (!m_pAllocMgr)
        return RC::CANNOT_ALLOCATE_MEMORY;

    if (GCxPwrWait::GCxModeDisabled != m_GCxInL2)
    {
        if (m_GCxInL2 > GCxPwrWait::GCxModeRTD3)
        {
            Printf
            (
                Tee::PriError,
                "GCxInL2 = %d is invalid, only %d-%d are valid.\n",
                m_GCxInL2,
                GCxPwrWait::GCxModeDisabled,
                GCxPwrWait::GCxModeRTD3
            );
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        CHECK_RC(GpuTest::AllocDisplayAndSurface(true));
    }

    CHECK_RC(InitializeAllocMgr());
    CHECK_RC(SetupTestRoutes());
    if (GetUseDataFlowObj())
    {
        CHECK_RC(SetupDataFlowTestModes());
    }
    else
    {
        CHECK_RC(SetupTestModes());
    }

    LwLinkPowerState::ClaimState desiredState =
        LwLinkPowerState::ClaimState::FULL_BANDWIDTH;
    if (m_TogglePowerState || m_L2AfterCopy)
        desiredState = LwLinkPowerState::ClaimState::ALL_STATES;
    else if (m_LowPowerBandwidthCheck)
        desiredState = LwLinkPowerState::ClaimState::SINGLE_LANE;
    else if (m_CheckHwPowerStateSwitch)
        desiredState = LwLinkPowerState::ClaimState::HW_CONTROL;

    set<TestDevicePtr> powerStateDevices;
    for (auto & lwrTestMode : m_TestModes)
    {
        CHECK_RC(lwrTestMode->ForEachDevice(
            [&] (TestDevicePtr pDev) -> RC
            {
                if (pDev->SupportsInterface<LwLinkPowerState>() && !powerStateDevices.count(pDev))
                {
                    powerStateDevices.insert(pDev);
                }
                return RC::OK;
            }
        ));
    }
    // Explicitly resize the power state owners array to avoid vector resizes calling the
    // Owner destructor and releasing the lock
    m_PowerStateOwners.resize(powerStateDevices.size());
    auto ppLwrDev = powerStateDevices.begin();
    for (size_t lwrIdx = 0; lwrIdx < m_PowerStateOwners.size(); lwrIdx++, ppLwrDev++)
    {
        m_PowerStateOwners[lwrIdx] =
            LwLinkPowerState::Owner((*ppLwrDev)->GetInterface<LwLinkPowerState>());
        CHECK_RC(m_PowerStateOwners[lwrIdx].Claim(desiredState));
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Cleanup the test
//!
//! \return OK if cleanup succeeds, not OK otherwise
/* virtual */ RC MfgTest::LwLinkDataTest::Cleanup()
{
    StickyRC rc;

    for (auto & lwrrentPsOwner : m_PowerStateOwners)
    {
        rc = lwrrentPsOwner.Release();
    }
    m_PowerStateOwners.clear();

    m_PowerStateOwners.clear();
    m_TestModes.clear();
    rc = CleanupTestRoutes();
    if (m_pAllocMgr.get() != nullptr)
    {
        rc = m_pAllocMgr->Shutdown();
        m_pAllocMgr.reset();
    }
    rc = LwLinkTest::Cleanup();
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the test
//!
//! \return OK if Run succeeds, not OK otherwise
/* virtual */ RC MfgTest::LwLinkDataTest::RunTest()
{
    StickyRC rc;
    Tee::Priority pri = GetVerbosePrintPri();

    if (m_TestModes.empty())
    {
        Printf(Tee::PriError, "Empty test modes.\n");
        return RC::SOFTWARE_ERROR;
    }

    const UINT64 runStartMs = Xp::GetWallTimeMS();
    size_t ii = 0;
    do {
        if (ii >= m_TestModes.size())
            ii = 0; // Reset for KeepRunning

        bool bTestModeShown = false;
        Printf(m_ShowTestModes ? Tee::PriNormal : pri,
               "%s : Test Mode %u\n", GetName().c_str(), static_cast<UINT32>(ii));
        ShowTestMode(ii, m_ShowTestModes ? Tee::PriNormal : pri, &bTestModeShown);

        CHECK_RC(m_TestModes[ii]->AcquireResources());

        DEFERNAME(releaseResources)
        {
            m_TestModes[ii]->ReleaseResources();
        };
        if (!m_SkipLinkStateCheck)
        {
            rc = m_TestModes[ii]->CheckLinkState();
            if (OK != rc)
            {
                // If the test mode wasnt dumped above, dump it now
                const string spacingStr(GetName().size(), ' ');
                Printf(Tee::PriNormal, "%s : Link state failure in test mode %u!!\n",
                       GetName().c_str(), static_cast<UINT32>(ii));

                ShowTestMode(ii, Tee::PriNormal, &bTestModeShown);
                if ((rc == RC::LWLINK_BUS_ERROR) && !m_pGolden->GetStopOnError())
                {
                    SetDeferredRc(rc);
                    rc.Clear();
                }
            }
            CHECK_RC(rc);
        }

        const double TicksPerSec = (double)Xp::QueryPerformanceFrequency();
        double elapsedModeRuntimeSec = 0.0;
        double start;
        const UINT32 endLoop = GetTestConfiguration()->Loops();

        if (m_ShowPortStats || !m_SkipPortStatsCheck)
        {
            CHECK_RC(m_TestModes[ii]->StartPortStats());
        }

        const bool bLogUphy = m_EnableUphyLogging ||
               (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::BandwidthTest);
        bool bDone = false;
        UINT32 loop = 0;
        auto LogUphyOnCopy = [&] () -> RC
        {
            StickyRC uphyLogRc;
            if (bLogUphy)
            {
                uphyLogRc = m_TestModes[ii]->ForEachDevice(
                    [&] (TestDevicePtr pDev) -> RC
                    {
                        return UphyRegLogger::LogRegs(pDev,
                                                      UphyRegLogger::UphyInterface::LwLink,
                                                      UphyRegLogger::LogPoint::BandwidthTest,
                                                      rc);
                    }
                );
            }
            return uphyLogRc;
        };
        while (!bDone)
        {
            DEFERNAME(logUphy)
            {
                LogUphyOnCopy();
            };
            start = (double)Xp::QueryPerformanceCounter();
            if (m_ShowTestProgress)
                Printf(Tee::PriNormal, "%s : Starting loop %u\n", GetName().c_str(), loop);
            CHECK_RC(m_TestModes[ii]->DoOneCopy(m_pTestConfig->TimeoutMs()));
            elapsedModeRuntimeSec += ((double)Xp::QueryPerformanceCounter() - start)/TicksPerSec;

            logUphy.Cancel();
            CHECK_RC(LogUphyOnCopy());

            loop++;
            bDone = ((m_PerModeRuntimeSec != 0) &&
                     (elapsedModeRuntimeSec >= m_PerModeRuntimeSec)) ||
                    ((m_PerModeRuntimeSec == 0) && (loop >= endLoop)) ||
                    (m_IsBgTest && !m_KeepRunning);

            if (m_RuntimeMs && !m_IsBgTest &&
                (Xp::GetWallTimeMS() - runStartMs) >= m_RuntimeMs)
            {
                bDone = true;
            }
        }

        if (m_ShowPortStats || !m_SkipPortStatsCheck)
        {
            CHECK_RC(m_TestModes[ii]->StopPortStats());

            rc = m_TestModes[ii]->CheckPortStats();
            if (rc == RC::UNEXPECTED_RESULT)
            {
                // If the test mode wasnt dumped above, dump it now
                const string spacingStr(GetName().size(), ' ');
                Printf(Tee::PriNormal, "%s : Port Stat check failure in test mode %u!!\n",
                       GetName().c_str(), static_cast<UINT32>(ii));

                ShowTestMode(ii, Tee::PriNormal, &bTestModeShown);
                if (m_SkipPortStatsCheck)
                {
                    Printf(Tee::PriWarn,
                           "%s : Ignoring port stat check failure in test mode %u "
                           "due to SkipPortStatsCheck!!\n",
                           GetName().c_str(), static_cast<UINT32>(ii));
                    rc.Clear();
                }

                if (!m_pGolden->GetStopOnError())
                {
                    SetDeferredRc(rc);
                    rc.Clear();
                }
            }

            if (m_ShowPortStats)
            {
                Printf(Tee::PriNormal,
                       "%s : Port statistics for test mode %u\n",
                       GetName().c_str(), static_cast<UINT32>(ii));
                ShowTestMode(ii, Tee::PriNormal, &bTestModeShown);
                CHECK_RC(m_TestModes[ii]->ShowPortStats());
            }
        }

        if (!m_SkipSurfaceCheck)
        {
            rc = m_TestModes[ii]->CheckSurfaces();
            if (OK != rc)
            {
                // If the test mode wasnt dumped above, dump it now
                Printf(Tee::PriNormal, "%s : Surface check failure in test mode %u!!\n",
                       GetName().c_str(), static_cast<UINT32>(ii));

                ShowTestMode(ii, Tee::PriNormal, &bTestModeShown);

                if ((rc == RC::BUFFER_MISMATCH) && !m_pGolden->GetStopOnError())
                {
                    SetDeferredRc(rc);
                    rc.Clear();
                }

                if (m_DumpSurfaceOnMiscompare)
                {
                    string fname = Utility::StrPrintf("%s_%u",
                                                      GetName().c_str(),
                                                      static_cast<UINT32>(ii));
                    CHECK_RC(m_TestModes[ii]->DumpSurfaces(fname));
                }
            }
            CHECK_RC(rc);
        }

        if (m_CheckHwPowerStateSwitch || m_TogglePowerState || m_ThermalSlowdown || m_ShowLowPowerCheck)
        {
            rc = m_TestModes[ii]->CheckLPCount(m_LPCountCheckTol, m_LPCountCheckTolPct);
            if (OK != rc)
            {
                Printf(Tee::PriNormal, "%s : LP entry counters check failure in test mode %u!!\n",
                       GetName().c_str(), static_cast<UINT32>(ii));
                ShowTestMode(ii, Tee::PriNormal, &bTestModeShown);
                if (m_SkipLPCountCheck)
                {
                    Printf(Tee::PriWarn,
                           "%s : Ignoring LP counter check failure in test mode %u "
                           "due to SkipLPCountCheck!!\n",
                           GetName().c_str(), static_cast<UINT32>(ii));
                    rc.Clear();
                }

                if (rc == RC::LWLINK_BUS_ERROR && !m_pGolden->GetStopOnError())
                {
                    SetDeferredRc(rc);
                    rc.Clear();
                }
            }
            CHECK_RC(rc);
        }

        if (!m_SkipBandwidthCheck || m_ShowBandwidthData)
        {
            vector<LwLinkDataTestHelper::TestMode::BandwidthInfo> bwFailureInfos;
            rc = m_TestModes[ii]->CheckBandwidth(&bwFailureInfos);
            if (rc == RC::BANDWIDTH_OUT_OF_RANGE)
            {
                // If the test mode wasnt dumped above, dump it now
                const string spacingStr(GetName().size(), ' ');
                Printf(Tee::PriNormal, "%s : Bandwidth check failure in test mode %u!!\n",
                       GetName().c_str(), static_cast<UINT32>(ii));
                Printf(Tee::PriNormal,
                       "%s   Please verify the LwLink HW and check FB configuration and\n"
                       "%s   SYSCLK frequency with the HW team!!\n",
                       spacingStr.c_str(), spacingStr.c_str());

                ShowTestMode(ii, Tee::PriNormal, &bTestModeShown);
                if (m_SkipBandwidthCheck)
                {
                    Printf(Tee::PriWarn,
                           "%s : Ignoring bandwidth check failure in test mode %u "
                           "due to SkipBandwidthCheck!!\n",
                           GetName().c_str(), static_cast<UINT32>(ii));
                    rc.Clear();
                }
                else
                {
                    for (const auto bwInfo : bwFailureInfos)
                    {
                        using ProtobufTransferType = Mle::BwFailure::TransferType;
                        ProtobufTransferType tt = ProtobufTransferType::tt_unknown;
                        switch (bwInfo.transferType)
                        {
                            case LwLink::TT_UNIDIR_READ:     tt = ProtobufTransferType::tt_unidir_read; break;
                            case LwLink::TT_UNIDIR_WRITE:    tt = ProtobufTransferType::tt_unidir_write; break;
                            case LwLink::TT_BIDIR_READ:      tt = ProtobufTransferType::tt_bidir_read; break;
                            case LwLink::TT_BIDIR_WRITE:     tt = ProtobufTransferType::tt_bidir_write; break;
                            case LwLink::TT_READ_WRITE:      tt = ProtobufTransferType::tt_read_write; break;
                        }
                        using FailureCauseType = Mle::BwFailure::FailureCause;
                        FailureCauseType failureCause = bwInfo.tooLow ? FailureCauseType::too_low : FailureCauseType::too_high;

                        ErrorMap::ScopedDevInst setDevInst(bwInfo.srcDevId);

                        Mle::Print(Mle::Entry::bw_failure)
                            .remote_dev_id(bwInfo.remoteDevId)
                            .src_link_mask(bwInfo.srcLinkMask)
                            .remote_link_mask(bwInfo.remoteLinkMask)
                            .transfer_type(tt)
                            .bandwidth(bwInfo.bandwidth)
                            .threshold(bwInfo.threshold)
                            .failure_cause(failureCause)
                            .interface(Mle::UphyRegLog::Interface::lwlink)
                            .bw_units(Mle::BwFailure::BwUnits::kibi_bytes_per_sec);
                    }
                }

                if (!m_pGolden->GetStopOnError())
                {
                    SetDeferredRc(rc);
                    rc.Clear();
                }
            }
            CHECK_RC(rc);
        }

        releaseResources.Cancel();
        CHECK_RC(m_TestModes[ii]->ReleaseResources());

        ii++;
        if (m_IsBgTest)
        {
            // Exit from the background test only upon going through
            // all the testmodes:
            if (ii >= m_TestModes.size() && !m_KeepRunning)
            {
                break;
            }
        }
        else if (m_RuntimeMs)
        {
            if ( (Xp::GetWallTimeMS() - runStartMs) >= m_RuntimeMs)
            {
                break;
            }
        }
        else
        {
            if (ii >= m_TestModes.size())
            {
                break;
            }
        }
    } while (true);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
void MfgTest::LwLinkDataTest::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "LwLinkDataTest Js Properties:\n");
    Printf(pri, "\tNumErrorsToPrint:          %u\n",     m_NumErrorsToPrint);
    Printf(pri, "\tBwThresholdPct:            %-8.3f\n",  m_BwThresholdPct);
    Printf(pri, "\tTransferDirMask:           %s\n",
           LwLinkDataTestHelper::TransferDirMaskStr(m_TransferDirMask).c_str());
    Printf(pri, "\tTransferTypeMask:          %s\n",
           LwLinkDataTestHelper::TransferTypeMaskStr(m_TransferTypeMask).c_str());

    string tempStr = "ALL";
    if (m_MinSimulRoutes != ALL_ROUTES)
        tempStr = Utility::StrPrintf("%u", m_MinSimulRoutes);
    Printf(pri, "\tMinSimulRoutes:            %s\n",     tempStr.c_str());

    tempStr = "ALL";
    if (m_MaxSimulRoutes != ALL_ROUTES)
        tempStr = Utility::StrPrintf("%u", m_MaxSimulRoutes);
    Printf(pri, "\tMaxSimulRoutes:            %s\n",     tempStr.c_str());

    tempStr = "ALL";
    if (m_LwLinkIdMask != ALL_LINKS)
        tempStr = Utility::StrPrintf("0x%02x", m_LwLinkIdMask);
    Printf(pri, "\tLwLinkIdMask:              %s\n", tempStr.c_str());

    Printf(pri, "\tShowTestModes:             %s\n", m_ShowTestModes ? "true" : "false");
    Printf(pri, "\tShowBandwidthData:         %s\n", m_ShowBandwidthData ? "true" : "false");
    Printf(pri, "\tCopyLoopCount:             %u\n", m_CopyLoopCount);
    Printf(pri, "\tDumpSurfaceOnMiscompare:   %s\n", m_DumpSurfaceOnMiscompare ? "true" : "false");
    Printf(pri, "\tSkipBandwidthCheck:        %s\n", m_SkipBandwidthCheck ? "true" : "false");
    Printf(pri, "\tSkipSurfaceCheck:          %s\n", m_SkipSurfaceCheck ? "true" : "false");
    Printf(pri, "\tSysmemLoopback:            %s\n", m_SysmemLoopback ? "true" : "false");
    Printf(pri, "\tPauseTestMask:             0x%02x\n", m_PauseTestMask);
    Printf(pri, "\tPerModeRuntimeSec:         %u\n", m_PerModeRuntimeSec);
    Printf(pri, "\tRuntimeMs:                 %u\n", m_RuntimeMs);
    Printf(pri, "\tTestAllGpus:               %s\n", m_TestAllGpus ? "true" : "false");
    Printf(pri, "\tShowTestProgress:          %s\n", m_ShowTestProgress ? "true" : "false");
    Printf(pri, "\tSkipLinkStateCheck:        %s\n", m_SkipLinkStateCheck ? "true" : "false");
    Printf(pri, "\tFailOnSysmemBw:            %s\n", m_FailOnSysmemBw ? "true" : "false");
    Printf(pri, "\tAddCpuTraffic:             %s\n", m_AddCpuTraffic ? "true" : "false");
    Printf(pri, "\tNumCpuTrafficThreads:      %u\n", m_NumCpuTrafficThreads);
    Printf(pri, "\tTogglePowerState:          %s\n", m_TogglePowerState ? "true" : "false");
    Printf(pri, "\tTogglePowerStateIntMs:     %lf ms\n", m_TogglePowerStateIntMs);
    Printf(pri, "\tLPCountCheckTol:           %u\n", m_LPCountCheckTol);
    Printf(pri, "\tLPCountCheckTolPct:        %-8.3f\n", m_LPCountCheckTolPct);
    Printf(pri, "\tSkipLPCountCheck:          %s\n", m_SkipLPCountCheck ? "true" : "false");
    Printf(pri, "\tMinLPCount:                %u\n", m_MinLPCount);
    Printf(pri, "\tLowPowerBandwidthCheck:    %s\n", m_LowPowerBandwidthCheck ? "true" : "false");
    Printf(pri, "\tCheckHwPowerStateSwitch:   %s\n", m_CheckHwPowerStateSwitch ? "true" : "false");
    Printf(pri, "\tIdleAfterCopyMs:           %u ms\n", m_IdleAfterCopyMs);
    Printf(pri, "\tThermalThrottling:         %s\n", m_ThermalThrottling ? "true" : "false");
    Printf(pri, "\tThermalThrottlingOnCount:  %u\n", m_ThermalThrottlingOnCount);
    Printf(pri, "\tThermalThrottlingOffCount: %u\n", m_ThermalThrottlingOffCount);
    Printf(pri, "\tThermalSlowdown:           %s\n", m_ThermalSlowdown ? "true" : "false");
    Printf(pri, "\tThermalSlowdownPeriodUs:   %u\n", m_ThermalSlowdownPeriodUs);
    tempStr = "UNKNOWN";
    switch (m_LoopbackTestType)
    {
        case LBT_READ:
            tempStr = "Read Only";
            break;
        case LBT_WRITE:
            tempStr = "Write Only";
            break;
        case LBT_READ_WRITE:
            tempStr = "Read/Write";
            break;
        default:
            break;
    }
    Printf(pri, "\tLoopbackTestType:          %s\n", tempStr.c_str());
    Printf(pri, "\tShowPortStats:             %s\n", m_ShowPortStats ? "true" : "false");
    Printf(pri, "\tSkipPortStatsCheck:        %s\n", m_SkipPortStatsCheck ? "true" : "false");
    Printf(pri, "\tLowLatencyThresholdNs:     %u\n", m_LowLatencyThresholdNs);
    Printf(pri, "\tMidLatencyThresholdNs:     %u\n", m_MidLatencyThresholdNs);
    Printf(pri, "\tHighLatencyThresholdNs:    %u\n", m_HighLatencyThresholdNs);
    if (m_RespDataFlow.empty() && m_ReqDataFlow.empty())
    {
        Printf(pri, "\tDataFlow:                  empty\n");
    }
    else
    {
        string dataFlowStr = "\tDataFlow:\n";
        if (!m_RespDataFlow.empty())
        {
            dataFlowStr +="\t    response:\n";
            for (auto respPair : m_RespDataFlow)
            {
                dataFlowStr += Utility::StrPrintf
                (
                    "\t        %s -> %s\n",
                    get<1>(respPair)->GetName().c_str(), // sink device first, see definition
                                                         // of RespSrcSinkDevs
                    get<0>(respPair)->GetName().c_str()
                );
            }
        }
        if (!m_ReqDataFlow.empty())
        {
            dataFlowStr += "\t    request:\n";
            for (auto reqPair : m_ReqDataFlow)
            {
                dataFlowStr += Utility::StrPrintf
                (
                    "\t        %s -> %s\n",
                    get<0>(reqPair)->GetName().c_str(), // src device first, see definition
                                                        // of ReqSrcSinkDevs
                    get<1>(reqPair)->GetName().c_str()
                );
            }
        }
        Printf(pri, "%s", dataFlowStr.c_str());
    }
    Printf(pri, "\tL2AfterCopy:               %s\n", m_L2AfterCopy ? "yes" : "no");
    Printf(pri, "\tL2DurationMs:              %lf\n", m_L2DurationMs);
    Printf(pri, "\tGCxInL2:                   %u\n", m_GCxInL2);
    Printf(pri, "\tEnableUphyLogging:         %s\n", m_EnableUphyLogging ? "true" : "false");
    LwLinkTest::PrintJsProperties(pri);
}

//----------------------------------------------------------------------------
// Configure the test mode based on test parameters
/* virtual */ void MfgTest::LwLinkDataTest::ConfigureTestMode
(
    LwLinkDataTestHelper::TestModePtr pTm,
    LwLinkDataTestHelper::TransferDir td,
    LwLinkDataTestHelper::TransferDir sysmemTd,
    bool                              bLoopbackInput
)
{
    pTm->SetSysmemLoopback(m_SysmemLoopback);
    pTm->SetShowCopyProgress(m_ShowTestProgress);
    pTm->SetFailOnSysmemBw(m_FailOnSysmemBw);
    pTm->SetAddCpuTraffic(m_AddCpuTraffic);
    pTm->SetNumCpuTrafficThreads(m_NumCpuTrafficThreads);
    pTm->SetLoopbackInput(bLoopbackInput);
    pTm->SetShowLowPowerCheck(m_ShowLowPowerCheck);

    if (m_TogglePowerState)
    {
        pTm->SetPowerStateToggle(true);
        pTm->SetPSToggleIntervalMs(m_TogglePowerStateIntMs);
    }

    if (m_LowPowerBandwidthCheck)
    {
        pTm->SetMakeLowPowerCopies(true);
    }

    if (m_CheckHwPowerStateSwitch)
    {
        pTm->SetInsertIdleAfterCopy(true);
        pTm->SetIdleAfterCopyMs(m_IdleAfterCopyMs);
    }

    if (m_TogglePowerState || m_CheckHwPowerStateSwitch)
    {
        pTm->SetMinLPCount(m_MinLPCount);
    }

    if (m_L2AfterCopy)
    {
        pTm->SetInsertL2AfterCopy(true);
        pTm->SetL2IntervalMs(m_L2DurationMs);
        if (GCxPwrWait::GCxModeDisabled != m_GCxInL2)
        {
            auto gcxBubble = make_unique<GCxBubble>(GetBoundGpuSubdevice());
            gcxBubble->SetGCxParams(
                GetBoundRmClient(),
                GetDisplay(),
                GetDisplayMgrTestContext(),
                Perf::ILWALID_PSTATE,
                GetTestConfiguration()->Verbose(),
                GetTestConfiguration()->TimeoutMs(),
                m_GCxInL2);
            pTm->SetGCxBubble(move(gcxBubble));
        }
    }

    pTm->SetThermalThrottling(m_ThermalThrottling);
    pTm->SetThermalThrottlingOnCount(m_ThermalThrottlingOnCount);
    pTm->SetThermalThrottlingOffCount(m_ThermalThrottlingOffCount);

    pTm->SetThermalSlowdown(m_ThermalSlowdown);
    pTm->SetThermalSlowdownPeriodUs(m_ThermalSlowdownPeriodUs);

    // If there is a GPU<->CPU connection and not testing sysmem loopback
    // mode, then sysmem links should be tested with Input and Output
    // separately.  Simultaneous bandwidth is severely limited by system
    // memory constraints.  This means that p2p/loopback may have one transfer
    // direction and sysmem may have a different one
    pTm->SetTransferDir(td);
    pTm->SetSysmemTransferDir(sysmemTd);

    pTm->SetLatencyBins(m_LowLatencyThresholdNs,
                        m_MidLatencyThresholdNs,
                        m_HighLatencyThresholdNs);
}

//----------------------------------------------------------------------------
// Check whether the route is valid for testing or whether it should be filtered
// out due to test parameters.  Returns true if the route is valid for testing
// false if the route should be skipped
/* virtual */ bool MfgTest::LwLinkDataTest::IsRouteValid(LwLinkRoutePtr pRoute)
{
    Tee::Priority pri = GetVerbosePrintPri();
    LwLinkConnection::EndpointDevices endpointDevs = pRoute->GetEndpointDevices();

    // Skip any routes with devices that are part of a SLI device
    if (pRoute->IsP2P())
    {
        GpuSubdevice *pSubdev1 = endpointDevs.first->GetInterface<GpuSubdevice>();
        GpuSubdevice *pSubdev2 = endpointDevs.second->GetInterface<GpuSubdevice>();

        if ((pSubdev1 && pSubdev1->GetParentDevice()->GetNumSubdevices() > 1) ||
            (pSubdev2 && pSubdev2->GetParentDevice()->GetNumSubdevices() > 1))
        {
            Printf(pri,
                   "%s : Skipping route since an endpoint is part of a SLI device:\n%s",
                   GetName().c_str(),
                   pRoute->GetString(endpointDevs.first,
                                     LwLink::DT_REQUEST,
                                     "    ").c_str());
            return false;
        }
    }

    // If a link ID filter is applied, then filter any routes
    // that do not contain a link that needs to be tested
    if (m_LwLinkIdMask != ALL_LINKS)
    {
        vector<LwLinkConnection::EndpointIds> eIds;
        eIds = pRoute->GetEndpointLinks(endpointDevs.first, LwLink::DT_REQUEST);
        bool bCountainsTestedLink = false;
        for (size_t ei = 0; (ei < eIds.size()) && !bCountainsTestedLink; ei++)
        {
            if (eIds[ei].first != LwLink::ILWALID_LINK_ID)
                bCountainsTestedLink = !!(m_LwLinkIdMask & (1 << eIds[ei].first));
            if (!bCountainsTestedLink && (eIds[ei].second != LwLink::ILWALID_LINK_ID))
                bCountainsTestedLink = !!(m_LwLinkIdMask & (1 << eIds[ei].second));
        }
        if (!bCountainsTestedLink)
        {
            Printf(pri,
                   "%s : Connection contains no tested links, skipping:\n%s\n",
                   GetName().c_str(),
                   pRoute->GetString(endpointDevs.first,
                                     LwLink::DT_REQUEST,
                                     "    ").c_str());
            return false;
        }
    }

    // If the route is of a type not being tested, skip it
    if ((pRoute->IsSysmem() && !(m_TransferTypeMask & LwLinkDataTestHelper::TT_SYSMEM)) ||
        (pRoute->IsLoop() && !(m_TransferTypeMask & LwLinkDataTestHelper::TT_LOOPBACK)) ||
        (pRoute->IsP2P() && !(m_TransferTypeMask & LwLinkDataTestHelper::TT_P2P)))
    {
        Printf(pri, "%s : Skipping route due to transfer type:\n%s\n",
               GetName().c_str(),
               pRoute->GetString(endpointDevs.first,
                                 LwLink::DT_REQUEST,
                                 "    ").c_str());
        return false;
    }

    // Loopback is only supported for In/Out testing since it by definition
    // tests both directions.  If In/Out transfers are not being tested
    // skip all loopback routes
    if ((pRoute->IsLoop() || (pRoute->IsSysmem() && m_SysmemLoopback)) &&
        (!(m_TransferDirMask & LwLinkDataTestHelper::TD_IN_OUT)))
    {
        Printf(pri,
               "%s : Loopback transfers only supported for in/out, skipping"
               " route\n%s\n",
               GetName().c_str(),
               pRoute->GetString(endpointDevs.first,
                                 LwLink::DT_REQUEST,
                                 "    ").c_str());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
// Check for validity of the test mode
// Returns true if the test mode is valid and can be tested, false otherwise
/* virtual */ bool MfgTest::LwLinkDataTest::IsTestModeValid(LwLinkDataTestHelper::TestModePtr pTm)
{
    // Only allow loopback testmodes when the directions is also In/Out
    if (pTm->HasLoopback() && (pTm->GetTransferDir() != LwLinkDataTestHelper::TD_IN_OUT))
        return false;
    return true;
}

//----------------------------------------------------------------------------
// Ensure that the arguments passed to the test are valid (i.e. no conflicting
// test arguments)
/* virtual */ RC MfgTest::LwLinkDataTest::ValidateTestSetup()
{
    if (GetBoundGpuSubdevice())
    {
        // Unsupported on SLI (to get here on a SLI device IsSupported would have
        // had to have been bypassed)
        if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        {
            Printf(Tee::PriError, "%s : Not supported on SLI!!\n", GetName().c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
        if (pPerf->HasPStates())
        {
            Perf::PerfPoint perfPt(0, Perf::GpcPerf_MAX);
            if (OK != pPerf->GetLwrrentPerfPoint(&perfPt))
            {
                Printf(Tee::PriWarn,
                       "%s : Getting current perf point failed, assuming sufficient "
                       "clock speeds\n",
                       GetName().c_str());
            }

            if ((perfPt.PStateNum != 0) ||
                (perfPt.SpecialPt != Perf::GpcPerf_MAX && !GetBoundGpuSubdevice()->IsSOC()))
            {
                // If we are not skipping the bandwidth check then the test is only
                // supported in pstates where we can expect to meet the bandwidth
                // requirements
                if (!m_SkipBandwidthCheck)
                {
                    Printf(Tee::PriWarn, "%s with bandwidth checking is only supported in 0.max\n"
                                         "    Skipping bandwidth check!\n",
                                         GetName().c_str());
                    m_SkipBandwidthCheck = true;
                }
            }
        }
    }

    if (m_ThermalThrottling &&
        m_ThermalThrottlingOnCount == 0 &&
        m_ThermalThrottlingOffCount == 0)
    {
        Printf(Tee::PriError, "%s : ThermalThrottling OnCount and OffCount cannot both be 0!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (m_AddCpuTraffic && m_NumCpuTrafficThreads == 0)
    {
        Printf(Tee::PriError, "%s : NumCpuTrafficThreads cannot be 0!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    auto powerStateTests = boost::count(array<bool, 3>
        {{ m_TogglePowerState, m_LowPowerBandwidthCheck, m_CheckHwPowerStateSwitch }}, true);
    if (1 < powerStateTests)
    {
        Printf(Tee::PriError,
            "Low power tests are incompatible between each other\n");
        return RC::BAD_PARAMETER;
    }

    if (m_L2AfterCopy && (m_TogglePowerState || m_CheckHwPowerStateSwitch))
    {
        Printf(Tee::PriError,
            "L2 test is incompatible with LP/FB switching tests\n");
        return RC::BAD_PARAMETER;
    }

    if (m_CheckHwPowerStateSwitch)
    {
        TestDevicePtr pTestDevice = GetBoundTestDevice();
        bool bHwPowerStateSwitchSupported = true;
        if (OK != IsHwLowPowerModeEnabled(pTestDevice, &bHwPowerStateSwitchSupported))
        {
            Printf(Tee::PriError, "%s : Unable to check HW power state support on %s!\n",
                   GetName().c_str(),
                   pTestDevice->GetName().c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }
        else if (!bHwPowerStateSwitchSupported)
        {
            Printf(Tee::PriError, "%s : HW power state switching not supported on %s!\n",
                   GetName().c_str(),
                   pTestDevice->GetName().c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }
    }

    if ((m_LoopbackTestType > static_cast<UINT32>(LBT_READ_WRITE)) || (m_LoopbackTestType == 0))
    {
        Printf(Tee::PriError,
               "%s : Invalid loopback test type %d\n", GetName().c_str(), m_LoopbackTestType);
        return RC::BAD_PARAMETER;
    }
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Setup all test routes
//!
//! \return OK if setup succeeds, not OK otherwise
RC MfgTest::LwLinkDataTest::SetupTestRoutes()
{
    RC rc;

    if (m_LwLinkIdMask == 0)
    {
        Printf(Tee::PriError,
               "%s : LwLinkIdMask may not be zero!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    if (m_MaxSimulRoutes == 0)
    {
        Printf(Tee::PriError,
               "%s : MaxSimulRoutes may not be zero!\n",
               GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    const bool useTrex = (GetBoundTestDevice()->GetType() == TestDevice::TYPE_LWIDIA_LWSWITCH);

    vector<LwLinkRoutePtr> routes;
    CHECK_RC(GetLwLinkRoutes(m_TestAllGpus || m_UseDataFlowObj || useTrex ? TestDevicePtr() : GetBoundTestDevice(),
                             &routes));

    sort(routes.begin(), routes.end(), SortRoutes);
    FilterTestRoutes(&routes);

    if (GetUseDataFlowObj())
    {
        SetupDataFlowRoutes(m_RespDataFlow, routes, back_inserter(m_RespTestRoutes));
        SetupDataFlowRoutes(m_ReqDataFlow, routes, back_inserter(m_ReqTestRoutes));
    }
    else
    {
        map< TestDevice*, vector<FLOAT32> > deviceLinkUsage;
        if (useTrex)
        {
            vector<TestDevicePtr> topoDevs;
            CHECK_RC(LwLinkDevIf::TopologyManager::GetDevices(&topoDevs));
            for (UINT32 idx = 0; idx < topoDevs.size(); idx++)
            {
                if (topoDevs[idx]->GetType() != TestDevice::TYPE_TREX)
                    continue;

                CHECK_RC(SetupTestRoutesOnGpu(topoDevs[idx], &routes, &deviceLinkUsage));
            }
        }
        else if (m_TestAllGpus)
        {
            TestDeviceMgr * pMgr = (TestDeviceMgr*) DevMgrMgr::d_TestDeviceMgr;
            for (UINT32 idx = 0; idx < pMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_GPU); idx++)
            {
                TestDevicePtr pTestDevice;
                CHECK_RC(pMgr->GetDevice(TestDevice::TYPE_LWIDIA_GPU, idx, &pTestDevice));
                if (!pTestDevice->SupportsInterface<LwLink>())
                {
                    continue;
                }
                CHECK_RC(SetupTestRoutesOnGpu(pTestDevice, &routes, &deviceLinkUsage));
            }
        }
        else
        {
            CHECK_RC(SetupTestRoutesOnGpu(GetBoundTestDevice(), &routes, &deviceLinkUsage));
        }
    }

    // Enable error injection mode on the subdevice if necessary.
    if (!GetBoundTestDevice()->IsSOC())
    {
        LwRmPtr pLwRm;
        for (auto const & pDev : m_TestedDevs)
        {
            if (m_TogglePowerState || m_ThermalSlowdown || m_LowPowerBandwidthCheck || m_CheckHwPowerStateSwitch)
            {
                CHECK_RC(pDev->GetInterface<LwLink>()->ReserveCounters());
                m_CounterReservedDevs.insert(pDev);
            }
        }
    }

    // Ensure that min routes is sane
    if ((m_MinSimulRoutes != ALL_ROUTES) &&
        (m_MinSimulRoutes > m_TestRoutes.size()))
    {
        Printf(Tee::PriError,
               "%s : MinSimulRoutes (%u) set higher than available routes (%u)!\n",
               GetName().c_str(),
               m_MinSimulRoutes,
               static_cast<UINT32>(m_TestRoutes.size()));
        return RC::BAD_PARAMETER;
    }

    if (m_TestRoutes.empty() && m_RespTestRoutes.empty() && m_ReqTestRoutes.empty())
    {
        Printf(Tee::PriError, "%s : No routes found to test!\n",
               GetName().c_str());
        return RC::NO_TESTS_RUN;
    }

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Cleanup all test routes
//!
//! \return OK if cleanup succeeds, not OK otherwise
RC MfgTest::LwLinkDataTest::CleanupTestRoutes()
{
    StickyRC rc;
    LwRmPtr pLwRm;

    if (!GetBoundTestDevice()->IsSOC())
    {
        for (auto const & pDev : m_CounterReservedDevs)
        {
            rc = pDev->GetInterface<LwLink>()->ReleaseCounters();
        }
        m_CounterReservedDevs.clear();
    }

    m_TestRoutes.clear();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Filter out and discard any routes that would be unusable based upon
//! the test configuration
//!
//! \param pRoutes : List of lwlink routes to filter
//!
void MfgTest::LwLinkDataTest::FilterTestRoutes
(
    vector<LwLinkRoutePtr> *pRoutes
)
{
    vector<LwLinkRoutePtr>::iterator ppRoute = pRoutes->begin();

    while (ppRoute != pRoutes->end())
    {
        LwLinkRoutePtr pRoute = *ppRoute;

        if (!IsRouteValid(pRoute))
        {
            ppRoute = pRoutes->erase(ppRoute);
            continue;
        }
        ppRoute++;
    }
}

//------------------------------------------------------------------------------
//! \brief Setup test routes associated with a particular GPU
//!
//! \param pTestDevice : TestDevicePtr of GPU to setup test routes on
//! \param pRoutes     : Full list of lwlink routes
//! \param pLinkUsage  : Current per device per-link usage
//!
//! \return OK if setup succeeds, not OK otherwise
//!
//! TODO : Handle more cases
//!    Sysmem : Ensure sysmem links get saturated first but allow all links to
//!             be saturated in a switch based system that has sysmem links.
//!             This may involve creating round robin configurations (i.e. 1 GPU
//!             saturates sysmem, the other 3 are configured to write or read
//!             1->2->3->1 to saturate all links on all GPUs (idea for
//!             implementation - keep track of saturation per-direction)
//!    Switch : Ensure that all active links on all switches are saturated
//!    Route  : Validate that route combinations do not over-saturate any links
//!             that are part of the route set
//!    Route  : Saturation on non-bidirectional routes
RC MfgTest::LwLinkDataTest::SetupTestRoutesOnGpu
(
    TestDevicePtr pTestDevice,
    vector<LwLinkRoutePtr> *pRoutes,
    map< TestDevice*, vector<FLOAT32> > *pLinkUsage
)
{
    RC rc;
    auto pLwLink = pTestDevice->GetInterface<LwLink>();

    vector<LwLinkRoutePtr> possibleRoutes;
    std::copy_if(pRoutes->begin(), pRoutes->end(), std::back_inserter(possibleRoutes),
                 [&pTestDevice](auto pRoute){ return pRoute->UsesDevice(pTestDevice); });

    vector<FLOAT32> noUsage(pLwLink->GetMaxLinks(), 0.0);
    if (pLinkUsage->find(pTestDevice.get()) == pLinkUsage->end())
        (*pLinkUsage)[pTestDevice.get()] = noUsage;

    vector<bool> bestRouteSet;
    UINT32 bestScore = 0;
    // Start by checking single routes and gradually increase until all routes
    // on the device are activated simultaneously
    for (size_t lwrNumRoutes = 1; (lwrNumRoutes <= possibleRoutes.size()); lwrNumRoutes++)
    {
        // Create a vector of bools that is permuted.  Using a vector of
        // bools actually ends up creating combinations rather than
        // permutations since link permutation (0, 2) would be
        // (true, false, true) which is the same as the link permutation
        // (2, 0)
        vector<bool> v(possibleRoutes.size());

        // Initialize the bool vector with a number of true values equal to
        // the number of routes that should be selected for the current
        // combination run
        fill(v.begin(), v.end() - possibleRoutes.size() + lwrNumRoutes, true);

        do
        {
            vector<FLOAT32> linkUsage = (*pLinkUsage)[pTestDevice.get()];
            bool routesValid          = true;
            UINT32 totalRouteLength   = 0;
            bool bContainsSysmemRoute = false;

            for (size_t i = 0; (i < possibleRoutes.size()) && routesValid; ++i)
            {
                if (v[i])
                {
                    // Need to determine the current usage, theoretically the
                    // number of endpoint links can differ from the actual width
                    // of the route if there is an intermediate connection
                    // that is narrower than the connection at the GPU.
                    vector<LwLinkConnection::EndpointIds> eIds =
                        possibleRoutes[i]->GetEndpointLinks(pTestDevice, LwLink::DT_REQUEST);
                    UINT32 epWidth = possibleRoutes[i]->GetWidthAtEndpoint(pTestDevice);
                    MASSERT(epWidth);
                    FLOAT32 lwrUsage =
                        possibleRoutes[i]->GetWidth(pTestDevice, LwLink::DT_REQUEST) / 
                        static_cast<FLOAT32>(epWidth);

                    TestDevicePtr pRemoteDev =
                        possibleRoutes[i]->GetRemoteEndpoint(pTestDevice);
                    epWidth = possibleRoutes[i]->GetWidthAtEndpoint(pRemoteDev);
                    MASSERT(epWidth);
                    FLOAT32 remUsage =
                        possibleRoutes[i]->GetWidth(pRemoteDev, LwLink::DT_REQUEST) /
                        static_cast<FLOAT32>(epWidth);

                    if (pLinkUsage->find(pRemoteDev.get()) == pLinkUsage->end())
                        (*pLinkUsage)[pRemoteDev.get()] = noUsage;

                    // while the route is valid, loop through the endpoints
                    for (auto const & lwrEp : eIds)
                    {
                        if (lwrEp.first != LwLink::ILWALID_LINK_ID)
                        {
                            linkUsage[lwrEp.first] += lwrUsage;
                            routesValid = (linkUsage[lwrEp.first] <= 1.0);

                            if (!routesValid)
                                break;
                        }

                        // Ensure the link usage on loop out connections is
                        // correctly updated
                        if (possibleRoutes[i]->IsLoop())
                        {
                            if ((lwrEp.first != LwLink::ILWALID_LINK_ID) &&
                                (lwrEp.second != LwLink::ILWALID_LINK_ID) &&
                                (lwrEp.first != lwrEp.second))
                            {
                                linkUsage[lwrEp.second] += lwrUsage;
                                routesValid = (linkUsage[lwrEp.second] <= 1.0);
                            }
                        }
                        else if (lwrEp.second != LwLink::ILWALID_LINK_ID)
                        {
                            // Validate the remote end can also handle the
                            // additional bandwidth
                            if (((*pLinkUsage)[pRemoteDev.get()][lwrEp.second] + remUsage) > 1.0)
                                break;
                        }
                    }
                    totalRouteLength += possibleRoutes[i]->GetMaxLength(pTestDevice,
                                                                        LwLink::DT_REQUEST);
                    if (possibleRoutes[i]->IsSysmem())
                        bContainsSysmemRoute = true;
                }
            }
            // Check the routes to see if this is a better route set than the
            // existing route set
            if (routesValid)
            {
                bool   bAllLinksSaturated = true;
                UINT32 linksUsed = 0;
                for (size_t lwrLink = 0; lwrLink < linkUsage.size(); lwrLink++)
                {
                    if (pLwLink->IsLinkActive(lwrLink))
                    {
                        if (linkUsage[lwrLink] < 1.0)
                            bAllLinksSaturated = false;
                        if (linkUsage[lwrLink] > (*pLinkUsage)[pTestDevice.get()][lwrLink])
                            linksUsed++;
                    }
                }

                // Create a score for the route set with the following priority
                //  1. All links being saturated
                //  2. System memory route as part of the route set
                //  3. Number of links used of the route set on the DUT
                //  4. Total length of all the routes
                UINT32 score = 0;
                if (bAllLinksSaturated)
                    score = (1 << 17);
                if (bContainsSysmemRoute)
                    score |= (1 << 16);
                score |= (linksUsed << 8);
                score |= totalRouteLength;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestRouteSet = v;

                    VerbosePrintf("New best route set on LwLink device %s\n",
                                  pTestDevice->GetName().c_str());
                    for (size_t lwrRoute = 0; lwrRoute < bestRouteSet.size(); lwrRoute++)
                    {
                        if (!bestRouteSet[lwrRoute])
                            continue;
                        TestDevicePtr pRemDev =
                            possibleRoutes[lwrRoute]->GetRemoteEndpoint(pTestDevice);
                        const UINT32 width =
                            possibleRoutes[lwrRoute]->GetWidth(pTestDevice, LwLink::DT_REQUEST);
                        const UINT32 length =
                            possibleRoutes[lwrRoute]->GetMaxLength(pTestDevice,
                                                                   LwLink::DT_REQUEST);
                        VerbosePrintf("  %s : Width %u, Length %u\n",
                                      pRemDev->GetName().c_str(),
                                      width,
                                      length);
                    }
                }
            }
        } while (prev_permutation(v.begin(), v.end()));
    }

    // Setup the test routes for the best route set
    for (size_t lwrRoute = 0; lwrRoute < bestRouteSet.size(); lwrRoute++)
    {
        if (!bestRouteSet[lwrRoute])
            continue;

        LwLinkDataTestHelper::TestRoutePtr pTr = CreateTestRoute(pTestDevice,
                                                                 possibleRoutes[lwrRoute]);
        if (!pTr)
            return RC::CANNOT_ALLOCATE_MEMORY;

        if (!AreRouteConnectionsUniform(possibleRoutes[lwrRoute]))
        {
            LwLinkConnection::EndpointDevices epDevs = possibleRoutes[lwrRoute]->GetEndpointDevices();
            Printf(Tee::PriWarn,
                   "%s : %s"
                   "         LwLink connections are not uniform in both directions for all data types\n"
                   "         without providing an optimal JSON, test may fail to exercise all links\n",
                   GetName().c_str(),
                   possibleRoutes[lwrRoute]->GetString(epDevs.first, LwLink::DT_REQUEST, "").c_str());
        }
        CHECK_RC(pTr->Initialize());

        // Update the link usage on the remote device for the route being tested
        TestDevicePtr pRemoteDev = possibleRoutes[lwrRoute]->GetRemoteEndpoint(pTestDevice);
        vector<LwLinkConnection::EndpointIds> eIds =
            possibleRoutes[lwrRoute]->GetEndpointLinks(pRemoteDev, LwLink::DT_REQUEST);
        const UINT32 remEpWidth = possibleRoutes[lwrRoute]->GetWidthAtEndpoint(pRemoteDev);
        MASSERT(remEpWidth);
        FLOAT32 remUsage = possibleRoutes[lwrRoute]->GetWidth(pRemoteDev, LwLink::DT_REQUEST) /
                           static_cast<FLOAT32>(remEpWidth);
        for (size_t lwrEp = 0; lwrEp < eIds.size(); lwrEp++)
        {
            if (eIds[lwrEp].first == LwLink::ILWALID_LINK_ID)
                continue;

            if (!possibleRoutes[lwrRoute]->IsLoop())
                (*pLinkUsage)[pRemoteDev.get()][eIds[lwrEp].first] += remUsage;
        }

        m_TestRoutes.push_back(pTr);

        m_TestedDevs.insert(pTestDevice);
        if (possibleRoutes[lwrRoute]->IsP2P() && !possibleRoutes[lwrRoute]->IsLoop())
        {
            m_TestedDevs.insert(pRemoteDev);
        }
    }

    pRoutes->erase(std::remove_if(pRoutes->begin(), pRoutes->end(),
                                  [&pTestDevice](auto pRoute){ return pRoute->UsesDevice(pTestDevice); }),
                   pRoutes->end());

    return rc;
}

//------------------------------------------------------------------------------
// Return true if the connections used in a route are uniform across all directions
// and data types
bool MfgTest::LwLinkDataTest::AreRouteConnectionsUniform(LwLinkRoutePtr pRoute)
{
    LwLinkConnection::EndpointDevices epDevs = pRoute->GetEndpointDevices();

    if (pRoute->AllPathsBidirectional() &&
        pRoute->AllDataFlowIdentical(epDevs.first) &&
        ((epDevs.first == epDevs.second) || pRoute->AllDataFlowIdentical(epDevs.second)))
    {
        return true;
    }

    set<LwLinkConnectionPtr> baseConnections;
    set<LwLinkConnectionPtr> lwrConnections;
    GetConnectionsOnRoute(pRoute, epDevs.first, LwLink::DT_REQUEST, &baseConnections);
    if (!pRoute->AllDataFlowIdentical(epDevs.first))
    {
        GetConnectionsOnRoute(pRoute, epDevs.first, LwLink::DT_RESPONSE, &lwrConnections);
        if (lwrConnections != baseConnections)
            return false;
    }

    if ((epDevs.first != epDevs.second) || pRoute->AllPathsBidirectional())
    {
        GetConnectionsOnRoute(pRoute, epDevs.second, LwLink::DT_REQUEST, &lwrConnections);
        if (lwrConnections != baseConnections)
            return false;
        if (!pRoute->AllDataFlowIdentical(epDevs.second))
        {
            GetConnectionsOnRoute(pRoute, epDevs.second, LwLink::DT_RESPONSE, &lwrConnections);
            if (lwrConnections != baseConnections)
                return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------
template <typename SrcSinkList, typename RoutesList, typename OutIterator>
RC MfgTest::LwLinkDataTest::SetupDataFlowRoutes
(
    const SrcSinkList& srcSinkList,
    const RoutesList& routes,
    OutIterator outIt
)
{
    RC rc;

    vector<LwLinkRoutePtr> neededRoutes;
    for (const auto &srcSink : srcSinkList)
    {
        auto it = boost::find_if(routes, [&srcSink](auto r)
        {
            auto ed = r->GetEndpointDevices();
            return (get<0>(ed) == get<0>(srcSink) && get<1>(ed) == get<1>(srcSink)) ||
                   (get<0>(ed) == get<1>(srcSink) && get<1>(ed) == get<0>(srcSink));
        });
        TestDevicePtr actDev = get<0>(srcSink);
        TestDevicePtr inactDev = get<1>(srcSink);
        if (routes.end() != it)
        {
            // We create routes with the "local" device always the one that
            // generates the traffic. The descendants of this class have to use
            // this assumption to setup the copy engines accordingly.
            auto tr = CreateTestRoute(actDev, *it);
            if (!tr)
            {
                CHECK_RC(RC::CANNOT_ALLOCATE_MEMORY);
            }
            tr->Initialize();
            *outIt++ = tr;
            m_TestedDevs.insert(actDev);
            m_TestedDevs.insert(inactDev);
        }
        else
        {
            Printf(Tee::PriError,
                   "Cannot find a route for device %s and %s\n",
                   actDev->GetName().c_str(), inactDev->GetName().c_str());
            CHECK_RC(RC::BAD_PARAMETER);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC MfgTest::LwLinkDataTest::SetupDataFlowTestModes()
{
    RC rc;

    if (!m_RespTestRoutes.empty() &&
        ((m_TransferDirMask & LwLinkDataTestHelper::TD_IN_ONLY) ||
         (m_TransferDirMask & LwLinkDataTestHelper::TD_IN_OUT)))
    {
        auto respTm = CreateTestMode();
        if (!respTm)
            return RC::CANNOT_ALLOCATE_MEMORY;
        ConfigureTestMode(
            respTm,
            LwLinkDataTestHelper::TD_IN_ONLY,
            LwLinkDataTestHelper::TD_ILWALID,
            false
        );
        respTm->Initialize(m_RespTestRoutes, ~static_cast<UINT32>(0));
        CHECK_RC(AssignTestTransferHw(respTm));
        m_TestModes.push_back(respTm);
    }

    if (!m_ReqTestRoutes.empty() &&
        ((m_TransferDirMask & LwLinkDataTestHelper::TD_OUT_ONLY) ||
         (m_TransferDirMask & LwLinkDataTestHelper::TD_IN_OUT)))
    {
        auto reqTm = CreateTestMode();
        if (!reqTm)
            return RC::CANNOT_ALLOCATE_MEMORY;
        ConfigureTestMode(
            reqTm,
            LwLinkDataTestHelper::TD_OUT_ONLY,
            LwLinkDataTestHelper::TD_ILWALID,
            false
        );
        reqTm->Initialize(m_ReqTestRoutes, ~static_cast<UINT32>(0));
        CHECK_RC(AssignTestTransferHw(reqTm));
        m_TestModes.push_back(reqTm);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Setup all test modes
//!
//! \return OK if setup succeeds, not OK otherwise
RC MfgTest::LwLinkDataTest::SetupTestModes()
{
    const UINT32 allRouteMask = (1 << static_cast<UINT32>(m_TestRoutes.size())) - 1;
    for (UINT32 routeMask = 1; routeMask <= allRouteMask; routeMask++)
    {
        UINT32 routeCount = static_cast<UINT32>(Utility::CountBits(routeMask));

        // Skip if there are too many routes
        if ((m_MaxSimulRoutes != ALL_ROUTES) &&
            (routeCount > m_MaxSimulRoutes))
        {
            continue;
        }

        // Skip if there are not enough routes
        if (((m_MinSimulRoutes == ALL_ROUTES) && (routeMask != allRouteMask)) ||
            ((m_MinSimulRoutes != ALL_ROUTES) && (routeCount < m_MinSimulRoutes)))
        {
            continue;
        }

        UINT32 transferDirs = m_TransferDirMask;
        while (transferDirs)
        {
            UINT32 lwrDir = transferDirs & ~(transferDirs - 1);
            transferDirs ^= lwrDir;

            LwLinkDataTestHelper::TransferDir lwrTd;
            lwrTd = static_cast<LwLinkDataTestHelper::TransferDir>(lwrDir);
            LwLinkDataTestHelper::TestModePtr pTm = CreateTestMode();
            if (!pTm)
                return RC::CANNOT_ALLOCATE_MEMORY;

            LwLinkDataTestHelper::TransferDir sysmemTd = lwrTd;
            if (sysmemTd == LwLinkDataTestHelper::TD_IN_OUT)
                sysmemTd = LwLinkDataTestHelper::TD_IN_ONLY;

            // If testing loopback by write, or read/write, then the first mode
            // tested will be with writes
            ConfigureTestMode(pTm, lwrTd, sysmemTd, (m_LoopbackTestType == LBT_READ));

            pTm->Initialize(m_TestRoutes, routeMask);

            if (!IsTestModeValid(pTm) || (OK != AssignTestTransferHw(pTm)))
                continue;

            m_TestModes.push_back(pTm);

            const bool bSingleDirSysmem =
                (pTm->GetTransferTypes() & LwLinkDataTestHelper::TT_SYSMEM) &&
                (lwrTd == LwLinkDataTestHelper::TD_IN_OUT) &&
                !m_SysmemLoopback;

            // If there is a GPU<->GPU loopback route, add the test mode
            // a second time to test writes through loopback in addition to
            // reads if both reads and writes are being tested through loopback
            //
            // If there is a GPU<->CPU connection and not testing sysmem loopback
            // mode, then also add a second test mode to test sysmem connections
            // in output mode
            if (((pTm->GetTransferTypes() & LwLinkDataTestHelper::TT_LOOPBACK) &&
                 (m_LoopbackTestType == LBT_READ_WRITE)) ||
                 bSingleDirSysmem)
            {
                LwLinkDataTestHelper::TestModePtr pTm = CreateTestMode();
                if (!pTm)
                    return RC::CANNOT_ALLOCATE_MEMORY;

                ConfigureTestMode(pTm, lwrTd, LwLinkDataTestHelper::TD_OUT_ONLY, true);

                pTm->Initialize(m_TestRoutes, routeMask);

                if (!IsTestModeValid(pTm) || (OK != AssignTestTransferHw(pTm)))
                    continue;

                m_TestModes.push_back(pTm);
            }
        }
    }

    if (m_TestModes.empty())
    {
        Printf(Tee::PriError, "%s : No modes found to test!\n",
               GetName().c_str());
        return RC::NO_TESTS_RUN;
    }

    // Sort Test modes by number of routes tested
    sort(m_TestModes.begin(), m_TestModes.end(), SortTestModes);

    if (m_CheckHwPowerStateSwitch && (m_IdleAfterCopyMs == 0))
    {
        for (size_t ii = 0; ii < m_TestModes.size(); ii++)
        {
            UINT32 idleAfterCopy = 0;
            m_TestModes[ii]->ForEachLink(
                [&](TestDevicePtr dev, UINT32 linkId)->RC
                {
                    if (dev->SupportsInterface<LwLinkPowerState>())
                    {
                        auto pPwrStateImpl = dev->GetInterface<LwLinkPowerState>();
                        idleAfterCopy = max(idleAfterCopy,
                                            pPwrStateImpl->GetLowPowerEntryTimeMs(linkId,
                                                                                LwLink::TX));
                    }
                    return OK;
                },
                [&](TestDevicePtr dev, UINT32 linkId)->RC
                {
                    if (dev->SupportsInterface<LwLinkPowerState>())
                    {
                        auto pPwrStateImpl = dev->GetInterface<LwLinkPowerState>();
                        idleAfterCopy = max(idleAfterCopy,
                                            pPwrStateImpl->GetLowPowerEntryTimeMs(linkId,
                                                                                LwLink::RX));
                    }
                    return OK;
                }
            );

            // Add an extra buffer of 1ms to account for any potential error
            idleAfterCopy += 1;
            m_TestModes[ii]->SetIdleAfterCopyMs(idleAfterCopy);
            VerbosePrintf("%s : IdleAfterCopyMs auto detected as %u on test mode %zu\n",
                          GetName().c_str(),
                          idleAfterCopy,
                          ii);
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC MfgTest::LwLinkDataTest::ReadDataFlowFromJSObj()
{
    typedef tuple<TestDevice::Type, unsigned int> DataFlowDevId;
    typedef tuple<DataFlowDevId, DataFlowDevId> RespSrcAndSink;
    typedef tuple<DataFlowDevId, DataFlowDevId, unsigned int> ReqSrcAndSink;

    RC rc;

    auto GetDevByTopologyId = [](DataFlowDevId id, TestDevicePtr *outDev) -> RC
    {
        MASSERT(nullptr != outDev);

        RC rc;

        outDev->reset();

        if (TestDevice::TYPE_LWIDIA_GPU != get<0>(id) && TestDevice::TYPE_IBM_NPU != get<0>(id))
        {
            Printf(
                Tee::PriError,
                "Unsupported device type %d in the data flow specification\n",
                static_cast<int>(get<0>(id)));
            CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }

        auto topoManager = LwLinkDevIf::TopologyManager::GetImpl();
        *outDev = topoManager->GetDevice(get<0>(id), get<1>(id));

        if (!*outDev)
        {
            Printf(
                Tee::PriError
              , "Cannot find %s from topology with id %d\n"
              , TestDevice::TYPE_LWIDIA_GPU == get<0>(id) ? "GPU" : "NPU"
              , get<1>(id)
            );
            CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }

        return rc;
    };

    JavaScriptPtr js;

    // Parses {"gpuN" | "npuN"} and generates a tuple of TestDevice::Type and a
    // number. "gpu", "npu" are case insensitive. qi::attr generates the
    // first part of the tuple, i.e. TestDevice::Type, qi::uint_ the second.
    // Use `qi::copy`, because by default grammar operands are held by
    // reference, they will be discarded before the expression is used in
    // `qi::parse`. `qi::copy` will copy all operands by value.
    auto const devIdGrammar = qi::copy((
            qi::no_case["gpu"] >> qi::attr(TestDevice::TYPE_LWIDIA_GPU) |
            qi::no_case["npu"] >> qi::attr(TestDevice::TYPE_IBM_NPU)
        ) >> qi::uint_);

    vector<JSObject *> respNodes;
    vector<JSObject *> reqNodes;
    CHECK_RC(js->GetProperty(m_DataFlow, DataFlowProps::RESPONSE, &respNodes));
    CHECK_RC(js->GetProperty(m_DataFlow, DataFlowProps::REQUEST, &reqNodes));

    vector<RespSrcAndSink> respDataFlow;
    for (auto respObj : respNodes)
    {
        string srcStr;
        string sinkStr;

        CHECK_RC(js->GetProperty(respObj, DataFlowProps::SOURCE, &srcStr));
        CHECK_RC(js->GetProperty(respObj, DataFlowProps::SINK, &sinkStr));

        DataFlowDevId srcDevId;
        DataFlowDevId sincDevId;
        if (!qi::parse(srcStr.begin(), srcStr.end(), devIdGrammar, srcDevId))
        {
            Printf(Tee::PriError, "Cannot recognize device and id: %s\n", srcStr.c_str());
            CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }
        if (!qi::parse(sinkStr.begin(), sinkStr.end(), devIdGrammar, sincDevId))
        {
            Printf(Tee::PriError, "Cannot recognize device and id: %s\n", sinkStr.c_str());
            CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }
        respDataFlow.emplace_back(srcDevId, sincDevId);
    }

    vector<ReqSrcAndSink> reqDataFlow;
    for (auto reqObj : reqNodes)
    {
        JSObject *sinkObj;
        string srcStr;
        string sinkStr;
        unsigned int addrRegion;

        CHECK_RC(js->GetProperty(reqObj, DataFlowProps::SOURCE, &srcStr));

        // sink property can be just a string, e.g. "gpu1" or an object, e.g.
        // { node : "gpu7", addr_region : 0 }
        if (RC::CANNOT_COLWERT_JSVAL_TO_OJBECT ==
                js->GetProperty(reqObj, DataFlowProps::SINK, &sinkObj))
        {
            CHECK_RC(js->GetProperty(reqObj, DataFlowProps::SINK, &sinkStr));
            addrRegion = 0;
        }
        else
        {
            CHECK_RC(js->GetProperty(sinkObj, DataFlowProps::NODE, &sinkStr));
            if (RC::ILWALID_OBJECT_PROPERTY ==
                    js->GetProperty(sinkObj, DataFlowProps::ADDR_REGION, &addrRegion))
            {
                addrRegion = 0;
            }
        }

        DataFlowDevId srcDevId;
        DataFlowDevId sincDevId;
        if (!qi::parse(srcStr.begin(), srcStr.end(), devIdGrammar, srcDevId))
        {
            Printf(Tee::PriError, "Cannot recognize device and id: %s\n", srcStr.c_str());
            CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }
        if (!qi::parse(sinkStr.begin(), sinkStr.end(), devIdGrammar, sincDevId))
        {
            Printf(Tee::PriError, "Cannot recognize device and id: %s\n", sinkStr.c_str());
            CHECK_RC(RC::BAD_COMMAND_LINE_ARGUMENT);
        }
        reqDataFlow.emplace_back(srcDevId, sincDevId, addrRegion);
    }

    for (const auto &respDevs : respDataFlow)
    {
        TestDevicePtr src;
        TestDevicePtr sink;

        CHECK_RC(GetDevByTopologyId(get<0>(respDevs), &src));
        CHECK_RC(GetDevByTopologyId(get<1>(respDevs), &sink));

        // put the sink device first, since it is the one that is active in the
        // response traffic
        m_RespDataFlow.emplace_back(sink, src);
    }

    for (const auto &reqDevs : reqDataFlow)
    {
        TestDevicePtr src;
        TestDevicePtr sink;

        CHECK_RC(GetDevByTopologyId(get<0>(reqDevs), &src));
        CHECK_RC(GetDevByTopologyId(get<1>(reqDevs), &sink));

        // put the source device first, since it is the one that is active in
        // the request traffic
        m_ReqDataFlow.emplace_back(src, sink, get<2>(reqDevs));
    }

    return rc;
}

//-----------------------------------------------------------------------------
void MfgTest::LwLinkDataTest::ShowTestMode
(
    size_t modeIdx
   ,Tee::Priority pri
   ,bool *pbTestModeShown
)
{
    if (!(*pbTestModeShown))
    {
        m_TestModes[modeIdx]->Print(pri, "   ");
        *pbTestModeShown = Tee::GetFileLevel() <= static_cast<int>(pri);
    }
}

//-----------------------------------------------------------------------------
// Exposed JS properties
JS_CLASS_VIRTUAL_INHERIT(LwLinkDataTest, LwLinkTest, "LwLink data transfer test");

CLASS_PROP_READWRITE(LwLinkDataTest, NumErrorsToPrint, UINT32,
                     "Number of errors printed on miscompares (default = 10)");
CLASS_PROP_READWRITE(LwLinkDataTest, BwThresholdPct, FLOAT64,
                     "Percent of max bandwidth that is still considered a pass (default = 1.0)");
CLASS_PROP_READWRITE(LwLinkDataTest, TransferDirMask, UINT32,
                     "Mask indicating transfer directions to perform  (default = TD_IN_OUT)");
CLASS_PROP_READWRITE(LwLinkDataTest, TransferTypeMask, UINT32,
                     "Mask indicating transfer types to perform  (default = TT_ALL)");
CLASS_PROP_READWRITE(LwLinkDataTest, MaxSimulRoutes, UINT32,
                     "Maximum number of simultaneous routes to test (default = ALL_ROUTES)");
CLASS_PROP_READWRITE(LwLinkDataTest, MinSimulRoutes, UINT32,
                     "Minimum number of simultaneous routes to test (default = ALL_ROUTES)");
CLASS_PROP_READWRITE(LwLinkDataTest, LwLinkIdMask, UINT32,
                     "Mask of LwLink IDs to test (default = ALL_LINKS)");
CLASS_PROP_READWRITE(LwLinkDataTest, ShowTestModes, bool,
                     "Print out test modes (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, ShowBandwidthData, bool,
                     "Print out detailed bandwidth data (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, CopyLoopCount, UINT32,
                     "Number of times each copy is repeated internally (default = 1)");
CLASS_PROP_READWRITE(LwLinkDataTest, DumpSurfaceOnMiscompare, bool,
                     "Dump all surfaces on a miscompare (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, SkipBandwidthCheck, bool,
                     "Skip the lwlink bandwidth check (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, SkipSurfaceCheck, bool,
                     "Skip the surface check (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, SysmemLoopback, bool,
    "Use sysmem loopback for sysmem routes (sys<->sys transfers) (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, PauseTestMask, UINT32,
                     "Mask of when test should be paused (default = PAUSE_NONE)");
CLASS_PROP_READWRITE(LwLinkDataTest, PerModeRuntimeSec, UINT32,
                     "Number of seconds to run each test mode, 0=use loops (default = 0)");
CLASS_PROP_READWRITE(LwLinkDataTest, RuntimeMs, UINT32,
                     "Requested time to run the active portion of the test in ms");
CLASS_PROP_READWRITE(LwLinkDataTest, TestAllGpus, bool,
                     "Test routes from all GPUs rather than just the bound GPU (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, ShowTestProgress, bool,
                     "True to show test progress (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, SkipLinkStateCheck, bool,
                     "Skip the link state check");
CLASS_PROP_READWRITE(LwLinkDataTest, FailOnSysmemBw, bool,
                     "Fail if system memory bandwidth is low (default=false)");
CLASS_PROP_READWRITE(LwLinkDataTest, AddCpuTraffic, bool,
                     "Add CPU generated Lwlink traffic to run simultaneously with GPU transfers (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, NumCpuTrafficThreads, UINT32,
                     "Number of simultaneous threads to run CPU traffic when using AddCpuTraffic (default = 0)");
CLASS_PROP_READWRITE(LwLinkDataTest, TogglePowerState, bool,
                     "Switch between Low Power (LP) and Full Bandwindth (FB) power states in the "
                     "background (default=false)");
CLASS_PROP_READWRITE(LwLinkDataTest, TogglePowerStateIntMs, FLOAT64,
                     "Switching time interval between LP and FB in ms (default=0.1)");
CLASS_PROP_READWRITE(LwLinkDataTest, LPCountCheckTol, UINT32,
                     "Ignore HW LP entry counters differ from the SW by this value (default=1)");
CLASS_PROP_READWRITE(LwLinkDataTest, LPCountCheckTolPct, FLOAT64,
                     "Ignore HW LP entry counters differ from the SW by the specified percent (default=-1.0 - disabled)");
CLASS_PROP_READWRITE(LwLinkDataTest, SkipLPCountCheck, bool,
                     "Skip the counter of LP power state entry check (default=false)");
CLASS_PROP_READWRITE(LwLinkDataTest, MinLPCount, UINT32,
                     "If above zero, it sets the minimum amount of LP entries the power state switch test expects, (default=0)");
CLASS_PROP_READWRITE(LwLinkDataTest, LowPowerBandwidthCheck, bool,
                     "Check bandwidth in low power consumption power state (default=false)");
CLASS_PROP_READWRITE(LwLinkDataTest, CheckHwPowerStateSwitch, bool,
                     "Insert idle period after copy to check HW switch to LP (default=false)");
CLASS_PROP_READWRITE(LwLinkDataTest, ShowLowPowerCheck, bool,
                     "Print the Low Power stats regardless of whether they were explicitly tested");
CLASS_PROP_READWRITE(LwLinkDataTest, IdleAfterCopyMs, UINT32,
                     "Idle interval in ms after copy for checking HW switch to LP (default=1)");
CLASS_PROP_READWRITE(LwLinkDataTest, ThermalThrottling, bool,
                     "Simulate thermal throttling on the LwSwitch devices (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, ThermalThrottlingOnCount, UINT32,
                     "Number of sysclk cycles to simulate overtemp during thermal throttling (default = 8000)"); //$
CLASS_PROP_READWRITE(LwLinkDataTest, ThermalThrottlingOffCount, UINT32,
                     "Number of sysclk cycles to disable overtemp simulation during thermal throttling (default = 16000)"); //$
CLASS_PROP_READWRITE(LwLinkDataTest, ThermalSlowdown, bool,
                     "Simulate thermal slowdown on the LwSwitch devices (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, ThermalSlowdownPeriodUs, UINT32,
                     "Minimum number of microseconds that the LwSwitch should remain in thermal slowdown once requested");
CLASS_PROP_READWRITE(LwLinkDataTest, LoopbackTestType, UINT32,
                     "Test type for loopback connections - read, write, or read/write (default = read/write)"); //$
CLASS_PROP_READWRITE(LwLinkDataTest, ShowPortStats, bool,
                     "Print detailed port statistics (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, SkipPortStatsCheck, bool,
                     "Skip checking port statistics (default = false)");
CLASS_PROP_READWRITE(LwLinkDataTest, LowLatencyThresholdNs, UINT32,
                     "Port statistics low latency threshold (default = 10)");
CLASS_PROP_READWRITE(LwLinkDataTest, MidLatencyThresholdNs, UINT32,
                     "Port statistics mid latency threshold (default = 195)");
CLASS_PROP_READWRITE(LwLinkDataTest, HighLatencyThresholdNs, UINT32,
                     "Port statistics high latency threshold (default = 1014)");
CLASS_PROP_READWRITE(LwLinkDataTest, DataFlow, JSObject *,
                     "A set of source and sink GPUs to set up data transfer");
CLASS_PROP_READWRITE(LwLinkDataTest, UseDataFlowObj, bool,
                     "Use sources and sinks specified in the DataFlow JS object (default = false)"); //$
CLASS_PROP_READWRITE(LwLinkDataTest, DataFlowFile, string,
                     "The name of a file with JSON definition of the traffic flow. Note that MODS "
                     "parses test arguments as JS expressions, therefore the filename has to "
                     "contain quotes after being processed by the command line processor. For "
                     "example: -testarg 246 DataFlowfile '\"explorer-optimal.json\"'.");
CLASS_PROP_READWRITE(LwLinkDataTest, UseOptimal, bool,
                     "The test will try to find a JSON file with the traffic definition by adding "
                     "-optimal.json to the topology filename without .topo[.bin] extensions.");
CLASS_PROP_READWRITE(LwLinkDataTest, L2AfterCopy, bool, "Place links in L2 sleep state between copies"); //$
CLASS_PROP_READWRITE(LwLinkDataTest, L2DurationMs, double, "Duration of LP sleep state between copies"); //$
CLASS_PROP_READWRITE(LwLinkDataTest, GCxInL2, UINT32, "Request GCx after L2");
CLASS_PROP_READWRITE(LwLinkDataTest, EnableUphyLogging, bool,
                     "Enable UPHY register logging after each copy.");
CLASS_PROP_READWRITE_FULL(LwLinkDataTest, IsBgTest, bool, // This should not be marked as a valid testarg
                          "Indicates this test is running as a bgtest.", 0, 0);
CLASS_PROP_READWRITE_FULL(LwLinkDataTest, KeepRunning, bool, // This should not be marked as a valid testarg
                          "Tell the test to keep running. To be used with bgtest.", 0, 0);

PROP_CONST(LwLinkDataTest, TD_IN_ONLY,  LwLinkDataTestHelper::TD_IN_ONLY);
PROP_CONST(LwLinkDataTest, TD_OUT_ONLY, LwLinkDataTestHelper::TD_OUT_ONLY);
PROP_CONST(LwLinkDataTest, TD_IN_OUT,   LwLinkDataTestHelper::TD_IN_OUT);
PROP_CONST(LwLinkDataTest, TD_ALL,      LwLinkDataTestHelper::TD_ALL);
PROP_CONST(LwLinkDataTest, TT_SYSMEM,   LwLinkDataTestHelper::TT_SYSMEM);
PROP_CONST(LwLinkDataTest, TT_P2P,      LwLinkDataTestHelper::TT_P2P);
PROP_CONST(LwLinkDataTest, TT_LOOPBACK, LwLinkDataTestHelper::TT_LOOPBACK);
PROP_CONST(LwLinkDataTest, TT_ALL,      LwLinkDataTestHelper::TT_ALL);
PROP_CONST(LwLinkDataTest, ALL_ROUTES,  LwLinkDataTest::ALL_ROUTES);
PROP_CONST(LwLinkDataTest, ALL_LINKS,   LwLinkDataTest::ALL_LINKS);
PROP_CONST(LwLinkDataTest, PAUSE_BEFORE_COPIES,  LwLinkDataTestHelper::TestMode::PAUSE_BEFORE_COPIES);  //$
PROP_CONST(LwLinkDataTest, PAUSE_AFTER_COPIES,   LwLinkDataTestHelper::TestMode::PAUSE_AFTER_COPIES);   //$
PROP_CONST(LwLinkDataTest, PAUSE_NONE,           LwLinkDataTestHelper::TestMode::PAUSE_NONE);
PROP_CONST(LwLinkDataTest, LBT_READ,        LwLinkDataTest::LBT_READ);
PROP_CONST(LwLinkDataTest, LBT_WRITE,       LwLinkDataTest::LBT_WRITE);
PROP_CONST(LwLinkDataTest, LBT_READ_WRITE, LwLinkDataTest::LBT_READ_WRITE);
