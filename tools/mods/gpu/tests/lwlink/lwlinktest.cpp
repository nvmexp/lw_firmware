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
#include "lwlinktest.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwLinkTest::LwLinkTest()
{
    SetName("LwLinkTest");
}

//------------------------------------------------------------------------------
//! \brief Destructor
//!
/* virtual */ LwLinkTest::~LwLinkTest()
{
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported (any active lwlinks on bound subdevice).
//!
//! \return true if supported, false otherwise
/* virtual */ bool LwLinkTest::IsSupported()
{
    if (!GpuTest::IsSupported())
        return false;

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    // If unable to find a LwLink device then there are no links to test
    if (!pTestDevice->SupportsInterface<LwLink>())
        return false;

    return true;
}

//------------------------------------------------------------------------------
//! \brief Setup the test.
//!
//! \return OK if setup succeeds, not OK otherwise
/* virtual */ RC LwLinkTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();

    if (pSubdev != nullptr)
    {
        TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
        const bool bLogUphy =
            ((pTestDeviceMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_LWSWITCH) == 0)&&
             (UphyRegLogger::GetLogPointMask() & UphyRegLogger::LogPoint::PostLinkTraining));

        // This check was moved from LwLinkTest::IsSupported to avoid a test escape
        // when we don't have any test routes, but they should be. See bug 2188798.
        vector<LwLinkRoutePtr> routes;
        CHECK_RC(LwLinkDevIf::TopologyManager::GetRoutesOnDevice(pTestDevice, &routes));
        if (routes.empty())
        {
            Printf(
                Tee::PriError,
                "LwLink device %s is not connected!\n",
                pTestDevice->GetName().c_str());
            return RC::HW_ERROR;
        }

        set<TestDevicePtr> uphyLogUntrainedLinksDevices;
        if (bLogUphy)
        {
            CHECK_RC(AddUphyLogUntrainedLinksDevice(pTestDevice,
                                                    &uphyLogUntrainedLinksDevices));
        }

        // LwLink P2P (and loopback) links are not trained to HS mode until a P2P
        // connection is made by allocating a peer mapping.  Force allocate a peer
        // mapping to ensure P2P links are in HS mode.
        GpuDevMgr * pMgr = (GpuDevMgr*) DevMgrMgr::d_GraphDevMgr;

        auto LogUphyOnUntrainedLinks = [&] () -> RC
        {
            StickyRC uphyRc;
            if (bLogUphy)
            {
                for (auto const & lwrDevice : uphyLogUntrainedLinksDevices)
                {
                    uphyRc = UphyRegLogger::LogRegs(lwrDevice,
                                                    UphyRegLogger::UphyInterface::LwLink,
                                                    UphyRegLogger::LogPoint::PostLinkTraining,
                                                    rc);
                }
            }
            return uphyRc;
        };
        DEFERNAME(logUphy)
        {
            LogUphyOnUntrainedLinks();
        };
        for (size_t ii = 0; ii < routes.size(); ii++)
        {
            if (routes[ii]->IsP2P())
            {
                TestDevicePtr pRemDev = routes[ii]->GetRemoteEndpoint(pTestDevice);
                if (bLogUphy)
                {
                    CHECK_RC(AddUphyLogUntrainedLinksDevice(pRemDev,
                                                            &uphyLogUntrainedLinksDevices));
                }

                // Peer mapping is a training point, part of the UPHY purpose is to diagnose
                // failures, so do not immediately exit on peer mapping failure and allow the
                // uphy dump to occur
                CHECK_RC(pMgr->AddPeerMapping(GetBoundRmClient(),
                                              pSubdev,
                                              pRemDev->GetInterface<GpuSubdevice>()));
            }
        }

        logUphy.Cancel();
        CHECK_RC(LogUphyOnUntrainedLinks());
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the test.  TODO : Accumulate and check errors
//!
//! \return OK if test succeeds, not OK otherwise
/* virtual */ RC LwLinkTest::Run()
{
    RC rc;

    CHECK_RC(RunTest());
    CHECK_RC(m_DeferredRc);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Cleanup the test.
//!
//! \return OK if test cleanup succeeds, not OK otherwise
/* virtual */ RC LwLinkTest::Cleanup()
{
    return GpuTest::Cleanup();
}

//------------------------------------------------------------------------------
//! \brief Get active lwlink routes.
//!
//! \param pSubdev : If not null, gets all active lwlink routes on the
//!                  subdevice.  If null gets all active lwlink routes on
//!                  all subdevices
//! \param pRoutes : Returned vector of routes
//!
//! \return OK if successful, not OK otherwise
RC LwLinkTest::GetLwLinkRoutes
(
    TestDevicePtr pTestDevice,
    vector<LwLinkRoutePtr> *pRoutes
)
{
    RC rc;

    if (pTestDevice.get() != nullptr)
    {
        if (pTestDevice->SupportsInterface<LwLink>())
        {
            CHECK_RC(LwLinkDevIf::TopologyManager::GetRoutesOnDevice(pTestDevice, pRoutes));
        }
    }
    else
    {
        CHECK_RC(LwLinkDevIf::TopologyManager::GetRoutes(pRoutes));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkTest::AddUphyLogUntrainedLinksDevice
(
    TestDevicePtr pTestDevice,
    set<TestDevicePtr> *pUphyLogUntrainedLinksDevice
)
{
    MASSERT(pUphyLogUntrainedLinksDevice);
    RC rc;

    if (pUphyLogUntrainedLinksDevice->count(pTestDevice))
        return OK;

    if (pTestDevice->SupportsInterface<LwLink>())
    {
        bool bAnyUntrainedLinks = false;
        vector<LwLink::LinkStatus> linkStatus;
        auto pLwLink = pTestDevice->GetInterface<LwLink>();
        CHECK_RC(pLwLink->GetLinkStatus(&linkStatus));
        for (size_t ii = 0; ii < linkStatus.size() && !bAnyUntrainedLinks; ++ii)
        {
            if (!pLwLink->IsLinkActive(ii))
                continue;

            if ((linkStatus[ii].linkState != LwLink::LS_ACTIVE) ||
                ((linkStatus[ii].rxSubLinkState != LwLink::SLS_SINGLE_LANE) &&
                 (linkStatus[ii].rxSubLinkState != LwLink::SLS_HIGH_SPEED)) ||
                ((linkStatus[ii].txSubLinkState != LwLink::SLS_SINGLE_LANE) &&
                 (linkStatus[ii].txSubLinkState != LwLink::SLS_HIGH_SPEED)))
            {
                bAnyUntrainedLinks = true;
            }
        }
        if (bAnyUntrainedLinks)
            pUphyLogUntrainedLinksDevice->insert(pTestDevice);
    }
    return rc;
}

//------------------------------------------------------------------------------
JS_CLASS_VIRTUAL_INHERIT(LwLinkTest, GpuTest, "LwLink test base class");
