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

#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "device/interface/c2c.h"
#include "device/interface/lwlink/lwlthroughputcounters.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/tests/lwca/peeratomic/peeratomic.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"

using namespace PeerAtomic;

namespace MfgTest
{
    const vector<LwLinkThroughputCount::Config> s_TpcConfig =
    {
        { LwLinkThroughputCount::CI_TX_COUNT_0, LwLinkThroughputCount::UT_BYTES,   LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_TX_COUNT_1, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_RX_COUNT_0, LwLinkThroughputCount::UT_BYTES,   LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
       ,{ LwLinkThroughputCount::CI_RX_COUNT_1, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_ALL, LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
    };

    // -----------------------------------------------------------------------------
    //! LWCA-based Peer atomics test.
    //!
    //! Runs a linux kernel on both the bound GpuDevice and a remote GpuDevice.  Both
    //! kernels target the same memory (residing on the remote) and execute atomic
    //! addition and reduction addition on the memory.  It is expected that if the
    //! operations are truly atomic, the final values in the memory location will be
    //! the sum of all operations from both devices.
    //!
    //! It is possible for the remote GpuDevice to be the same as the bound GpuDevice
    //! in that case the test will run over loopback
    class LwdaPeerAtomics : public LwdaStreamTest
    {
    public:
        LwdaPeerAtomics();
        virtual ~LwdaPeerAtomics() { }
        virtual bool IsSupported();
        virtual RC Setup();
        virtual RC Run();
        virtual RC Cleanup();
        void PrintJsProperties(Tee::Priority pri);

        enum class TransferInterface : UINT08
        {
            Pcie,
            LwLink,
            C2C,
            Count
        };

        SETGET_PROP(ThreadCount,              UINT32);
        SETGET_PROP(PerThreadIterations,      UINT32);
        SETGET_PROP(RemoteStartTimeoutMs,     UINT32);
        SETGET_PROP_LWSTOM(TransferInterface, UINT08);
        SETGET_PROP(PreferLoopback,           bool);
        SETGET_PROP(ShowPortStats,            bool);

    private:
        RC GetRemoteLwdaDevice(Lwca::Device *pRemLwdaDev, LwLinkRoutePtr *pRouteToTest);
        RC CreateAtomicKernel
        (
            Lwca::Device dev,
            Lwca::Module *pMod,
            Lwca::Function *pFunc
        );
        bool IsLoopback() const;
        RC   Synchronize();

        LwLinkRoutePtr     m_LwLinkRoute;   //!< LwLink route that will be tested
        Lwca::Device       m_RemoteLwdaDev; //!< Lwca remote device target
        Lwca::DeviceMemory m_TestMem;       //!< Test memory residing on the
                                            //!< remote device

        //! Semaphores and counters residing on both the peer and local devices.
        //!
        //! Semaphores (at location 0) used to ensure the start of atomic operations
        //! in the peer and local kernels occur simultaneously thus ensuring
        //! collisions on the test memory locations
        //!
        //! The remaining memory is a per-thread timeout counter to avoid thread
        //! hangs in case one devices kernel fails to start
        Lwca::DeviceMemory m_RemoteSem;
        Lwca::DeviceMemory m_LocalSem;

        //! Variables set from JS
        UINT32              m_ThreadCount              = MIN_THREAD_COUNT;
        UINT32              m_PerThreadIterations      = MIN_PERTHREAD_ITERATIONS;
        UINT32              m_RemoteStartTimeoutMs     = MIN_REMOTE_START_TIMEOUT_MS;
        TransferInterface   m_TransferInterface        = TransferInterface::LwLink;
        bool                m_PreferLoopback           = true;
        bool                m_ShowPortStats            = false;

        static const UINT32 MIN_THREAD_COUNT = 1024;
        static const UINT32 MIN_PERTHREAD_ITERATIONS = 1024;
        static const UINT32 MIN_REMOTE_START_TIMEOUT_MS = 1;
    };
}

using namespace MfgTest;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
MfgTest::LwdaPeerAtomics::LwdaPeerAtomics()
{
    SetName("LwdaPeerAtomics");
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
/* virtual */ bool MfgTest::LwdaPeerAtomics::IsSupported()
{
    // Check if compute is supported at all
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    if (GetBoundTestDevice()->IsSOC())
        return false;

    // The kernel is compiled only for compute 6.x+
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_60)
        return false;

    // Unsupported on SLI
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        return false;

    if ((m_TransferInterface == TransferInterface::LwLink) &&
        !GetBoundGpuSubdevice()->SupportsInterface<LwLink>())
    {
        return false;
    }

    if ((m_TransferInterface == TransferInterface::C2C) &&
        !GetBoundGpuSubdevice()->SupportsInterface<C2C>())
    {
        return false;
    }

    // If unable to get a remote lwca device then test is not supported
    if (!m_RemoteLwdaDev.IsValid() && (OK != GetRemoteLwdaDevice(&m_RemoteLwdaDev,
                                                                 &m_LwLinkRoute)))
    {
        return false;
    }

    // Lwca P2P requires unified addressing
    if (!IsLoopback() &&
        !GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING))
    {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
//! \brief Setup the test
//!
//! \return OK if setup succeeds, not OK otherwise
/* virtual */ RC MfgTest::LwdaPeerAtomics::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    // Ensure that test configuration is valid
    if (m_ThreadCount < MIN_THREAD_COUNT)
    {
        Printf(Tee::PriError, "%s : Thread count %u is less than the minimum (%u)\n",
               GetName().c_str(), m_ThreadCount, MIN_THREAD_COUNT);
        return RC::BAD_PARAMETER;
    }
    if (m_PerThreadIterations < MIN_PERTHREAD_ITERATIONS)
    {
        Printf(Tee::PriError, "%s : Per thread iterations %u are less than the minimum (%u)\n",
               GetName().c_str(), m_PerThreadIterations, MIN_PERTHREAD_ITERATIONS);
        return RC::BAD_PARAMETER;
    }

    if (!m_RemoteLwdaDev.IsValid())
    {
        CHECK_RC(GetRemoteLwdaDevice(&m_RemoteLwdaDev, &m_LwLinkRoute));
    }

    if (m_ShowPortStats && (m_TransferInterface == TransferInterface::LwLink) &&
        ((m_LwLinkRoute == nullptr) ||
         !GetBoundTestDevice()->SupportsInterface<LwLinkThroughputCounters>()))
    {
        Printf(Tee::PriWarn, "%s : LwLink port stats are not supported\n", GetName().c_str());
        m_ShowPortStats = false;
    }

    if (IsLoopback())
    {
        CHECK_RC(GetLwdaInstance().EnableLoopbackAccess(GetBoundLwdaDevice()));
    }
    else
    {
        CHECK_RC(GetLwdaInstance().InitContext(m_RemoteLwdaDev));
        CHECK_RC(GetLwdaInstance().EnablePeerAccess(GetBoundLwdaDevice(), m_RemoteLwdaDev));
        CHECK_RC(GetLwdaInstance().EnablePeerAccess(m_RemoteLwdaDev, GetBoundLwdaDevice()));
    }
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_RemoteLwdaDev,
                                              m_ThreadCount*sizeof(UINT32),
                                              &m_TestMem));
    if (IsLoopback())
    {
        CHECK_RC(m_TestMem.MapLoopback());
    }
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_RemoteLwdaDev,
                                              sizeof(UINT32),
                                              &m_RemoteSem));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(GetBoundLwdaDevice(),
                                              sizeof(UINT32),
                                              &m_LocalSem));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the test
//!
//! \return OK if setup succeeds, not OK otherwise
RC MfgTest::LwdaPeerAtomics::Run()
{
    StickyRC rc;

    typedef vector<UINT32,Lwca::HostMemoryAllocator<UINT32> > VelwINT32;
    VelwINT32 hostData(m_ThreadCount + 1,
                       UINT32(),
                       Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(),
                                                         GetBoundLwdaDevice()));
    for (UINT32 i = 0; i < (m_ThreadCount + 1); i++)
        hostData[i] = 0;
    CHECK_RC(m_TestMem.Set(&hostData[0], m_ThreadCount*sizeof(UINT32)));
    CHECK_RC(m_RemoteSem.Set(&hostData[0], sizeof(UINT32)));
    CHECK_RC(m_LocalSem.Set(&hostData[0], sizeof(UINT32)));
    CHECK_RC(Synchronize());

    // When running on non-loopback mode, UVA makes the device pointer access
    // transparent (i.e. the same device pointer is used for local/remote and
    // UVA causes access to be P2P as necessary).  For loopback the device
    // pointer is explicitly created and is different.  In order to force the
    // "local" instance of the kernel to send atomics over loopback, it needs
    // to be explicitly provided with the loopback device pointer to the
    // test memory
    UINT64 locTestMemoryPtr = m_TestMem.GetDevicePtr();
    if (IsLoopback())
        locTestMemoryPtr = m_TestMem.GetLoopbackDevicePtr();

    // Local kernel input, the test memory pointer needs to go to memory on the
    // "remote" end of the link.  For loopback this means the test memory device
    // pointer should be the loopback device pointer
    const PeerAtomicInput localInput =
    {
        locTestMemoryPtr,
        m_LocalSem.GetDevicePtr(),  // Control structure local to the kernel run
                                        // location (local since this structure is the
                                        // local kernel input)
        m_RemoteSem.GetDevicePtr(), // Control struction on the remote kernel run
                                        // location (remote since this structure is the
                                        // local kernel input)
        m_PerThreadIterations,
        static_cast<UINT64>(m_RemoteStartTimeoutMs) * 1000000ULL
    };
    const PeerAtomicInput remoteInput =
    {
        m_TestMem.GetDevicePtr(),
        m_RemoteSem.GetDevicePtr(), // Control structure local to the kernel run
                                        // location (remote since this structure is the
                                        // remote kernel input)
        m_LocalSem.GetDevicePtr(),  // Control structure on the remote kernel run
                                        // location (local since this structure is the
                                        // remote kernel input)
        m_PerThreadIterations,
        static_cast<UINT64>(m_RemoteStartTimeoutMs) * 1000000ULL
    };

    Lwca::Module localModule;
    Lwca::Function localPeerAtomic;
    Lwca::Module remoteModule;
    Lwca::Function remotePeerAtomic;
    CHECK_RC(CreateAtomicKernel(GetBoundLwdaDevice(), &localModule, &localPeerAtomic));
    CHECK_RC(CreateAtomicKernel(m_RemoteLwdaDev, &remoteModule, &remotePeerAtomic));

    // When running loopback it is required to create a second stream in order
    // to run multiple kernels simultaneously on the same device
    //
    // Create the stream before starting the local kernel so that time for
    // stream creation isn't part of the time between launching the local/remote
    // kernels when loopback testing
    Lwca::Stream remoteStream;
    if (IsLoopback())
        remoteStream = GetLwdaInstance().CreateStream(m_RemoteLwdaDev);

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    UINT64 linkMask = 0;

    // Support for port stats was validated during Setup
    LwLinkThroughputCounters * pTpc = nullptr;
    auto * pC2C = GetBoundTestDevice()->GetInterface<C2C>();
    if (m_ShowPortStats)
    {
        if ((m_TransferInterface == TransferInterface::LwLink) &&
            pTestDevice->SupportsInterface<LwLinkThroughputCounters>())
        {
            pTpc = pTestDevice->GetInterface<LwLinkThroughputCounters>();
            vector<LwLinkConnection::EndpointIds> eids =
                m_LwLinkRoute->GetEndpointLinks(pTestDevice, LwLink::DT_REQUEST);

            for (auto const & lwrEid : eids) 
                linkMask |= (1ULL << lwrEid.first);

            CHECK_RC(pTpc->SetupThroughputCounters(linkMask, s_TpcConfig));
            CHECK_RC(pTpc->StartThroughputCounters(linkMask));
            CHECK_RC(pTpc->ClearThroughputCounters(linkMask));
        }
        else if (m_TransferInterface == TransferInterface::C2C)
        {
            CHECK_RC(pC2C->ConfigurePerfMon(0, C2C::PerfMonDataType::RxDataAllVCs));
            CHECK_RC(pC2C->ConfigurePerfMon(1, C2C::PerfMonDataType::TxDataAllVCs));
            CHECK_RC(pC2C->StartPerfMon());
        }
    }

    CHECK_RC(localPeerAtomic.Launch(localInput));

    if (IsLoopback())
    {
        CHECK_RC(remotePeerAtomic.Launch(remoteStream, remoteInput));
    }
    else
    {
        CHECK_RC(remotePeerAtomic.Launch(remoteInput));
    }

    CHECK_RC(Synchronize());

    if (m_ShowPortStats)
    {
        if ((m_TransferInterface == TransferInterface::LwLink) &&
            pTestDevice->SupportsInterface<LwLinkThroughputCounters>())
        {
            CHECK_RC(pTpc->StopThroughputCounters(linkMask));

            map<UINT32, vector<LwLinkThroughputCount>> counts;
            rc = pTpc->GetThroughputCounters(linkMask, &counts);
            if (rc != RC::OK)
            {
                Printf(Tee::PriError, "Failed to get throughput counters on %s\n",
                       pTestDevice->GetName().c_str());
            }
            else
            {
                Printf(Tee::PriNormal, "%s throughput counters :\n",
                       pTestDevice->GetName().c_str());
                for (auto const &lwrLinkCounts : counts)
                {
                    Printf(Tee::PriNormal, "  Link %u :\n", lwrLinkCounts.first);

                    for (auto const &lwrCount : lwrLinkCounts.second)
                    {
                        Printf(Tee::PriNormal, "%-15s : %llu\n",
                               LwLinkThroughputCount::GetString(lwrCount.config, "    ").c_str(),
                               lwrCount.countValue);
                    }
                }
            }
        }
        else if (m_TransferInterface == TransferInterface::C2C)
        {
            map<UINT32, vector<C2C::PerfMonCount>> pmCounts;
            CHECK_RC(pC2C->StopPerfMon(&pmCounts));
            UINT64 totalReadFlits = 0;
            UINT64 totalWriteFlits = 0;
            for (auto const & lwrLinkCounts : pmCounts)
            {
                for (size_t lwrPmBit = 0; lwrPmBit < lwrLinkCounts.second.size(); lwrPmBit++)
                {
                    const UINT64 pmCount = lwrLinkCounts.second[lwrPmBit].pmCount;
                    Printf(Tee::PriNormal, "C2C link %u total %s data flits : %llu\n",
                           lwrLinkCounts.first,
                           (lwrPmBit == 0) ? "rx" : "tx",
                           pmCount);
                    if (lwrPmBit == 0)
                        totalReadFlits += pmCount;
                    else
                        totalWriteFlits += pmCount;
                }
            }
            Printf(Tee::PriNormal, "C2C total rx data flits  : %llu\n", totalReadFlits);
            Printf(Tee::PriNormal, "C2C total tx data flits : %llu\n", totalWriteFlits);
        }
    }

    CHECK_RC(m_TestMem.Get(&hostData[0], m_ThreadCount*sizeof(UINT32)));
    CHECK_RC(GetLwdaInstance().SynchronizeContext(m_RemoteLwdaDev));

    // Both local and remote threads are running the same kernel same iterations on
    // the same memory
    const UINT32 expectedVal = m_PerThreadIterations * INCREMENTS_PER_ITERATION * 2;
    for (UINT32 i = 0; i < m_ThreadCount; i++)
    {
        if (hostData[i] != expectedVal)
        {
            Printf(Tee::PriError, "%s : Miscompare at thread %u, expected 0x%08x, actual 0x%08x\n",
                   GetName().c_str(), i, expectedVal, hostData[i]);
            rc = RC::GPU_COMPUTE_MISCOMPARE;
        }
    }

    rc = m_RemoteSem.Get(&hostData[0], sizeof(UINT32));
    VerbosePrintf("%s : Remote semaphore : 0x%08x\n", GetName().c_str(), hostData[0]);
    rc = m_LocalSem.Get(&hostData[0], sizeof(UINT32));
    VerbosePrintf("%s : Local semaphore  : 0x%08x\n", GetName().c_str(), hostData[0]);

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Cleanup the test
//!
//! \return OK if cleanup succeeds, not OK otherwise
RC MfgTest::LwdaPeerAtomics::Cleanup()
{
    StickyRC rc;

    m_LocalSem.Free();
    m_RemoteSem.Free();
    m_TestMem.Free();

    if (IsLoopback())
    {
        rc = GetLwdaInstance().DisableLoopbackAccess(GetBoundLwdaDevice());
    }
    else
    {
        rc = GetLwdaInstance().DisablePeerAccess(m_RemoteLwdaDev, GetBoundLwdaDevice());
        rc = GetLwdaInstance().DisablePeerAccess(GetBoundLwdaDevice(), m_RemoteLwdaDev);
    }
    rc = LwdaStreamTest::Cleanup();
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \param pri : Priority to print the test properties at
void MfgTest::LwdaPeerAtomics::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tThreadCount:             %u\n", m_ThreadCount);
    Printf(pri, "\tPerThreadIterations:     %u\n", m_PerThreadIterations);
    Printf(pri, "\tRemoteStartTimeoutMs:    %u\n", m_RemoteStartTimeoutMs);
    string tiStr = "Unknown";
    switch (m_TransferInterface)
    {
        case TransferInterface::Pcie:   tiStr = "Pcie";   break;
        case TransferInterface::LwLink: tiStr = "LwLink"; break;
        case TransferInterface::C2C:    tiStr = "C2C";    break;
        default: break;
    }
    Printf(pri, "\tTransferInterface:       %s\n", tiStr.c_str());
    Printf(pri, "\tPreferLoopback:          %s\n", m_PreferLoopback ? "true" : "false");

    LwdaStreamTest::PrintJsProperties(pri);
}

// -----------------------------------------------------------------------------
UINT08 MfgTest::LwdaPeerAtomics::GetTransferInterface() const
{
    return static_cast<UINT08>(m_TransferInterface);
}

// -----------------------------------------------------------------------------
RC MfgTest::LwdaPeerAtomics::SetTransferInterface(UINT08 transferInterface)
{
    if (transferInterface >= static_cast<UINT08>(TransferInterface::Count))
        return RC::BAD_PARAMETER;
    m_TransferInterface = static_cast<TransferInterface>(transferInterface);
    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Gets the remote Lwca device to run the kernel on
//!
//! \param pRemoteLwdaDev : Pointer to returned remote device
//! \param pRouteToTest   : Pointer to returned lwlink route to test
//!
//! \return OK if a remote lwca device is found, not OK otherwise
RC MfgTest::LwdaPeerAtomics::GetRemoteLwdaDevice
(
    Lwca::Device   *pRemoteLwdaDev,
    LwLinkRoutePtr *pRouteToTest
)
{
    RC rc;

    if (m_TransferInterface == TransferInterface::LwLink)
    {
        vector<LwLinkRoutePtr> routes;
        TestDevicePtr pTestDevice = GetBoundTestDevice();
        CHECK_RC(LwLinkDevIf::TopologyManager::GetRoutesOnDevice(pTestDevice, &routes));

        bool bLoopbackPresent = false;
        LwLinkRoutePtr pLoopbackRoute;
        for (size_t i = 0; (i < routes.size()) && !bLoopbackPresent; i++)
        {
            bLoopbackPresent = routes[i]->IsLoop();
            if (routes[i]->IsLoop())
            {
                bLoopbackPresent = true;
                pLoopbackRoute = routes[i];
            }
        }

        // Choose loopback as the first option for a remote device is it is
        // preferred
        if (m_PreferLoopback && bLoopbackPresent)
        {
            *pRemoteLwdaDev = GetBoundLwdaDevice();
            *pRouteToTest   = pLoopbackRoute;
            return OK;
        }

        for (size_t i = 0; i < routes.size(); i++)
        {
            // Loopback connections were checked above.  At this point we only
            // care about P2P connections
            //
            // Only P2P/Loopback connections are valid since atomics are only
            // supported on P2P/Loopback (lwrrently).  Atomics to sysmem are not
            // supported until Volta+
            if (!routes[i]->IsP2P())
                continue;

            TestDevicePtr pRemLwLinkDev = routes[i]->GetRemoteEndpoint(pTestDevice);
            GpuSubdevice *pRemSubdev = pRemLwLinkDev->GetInterface<GpuSubdevice>();
            GpuDevice *pRemGpuDev = pRemSubdev->GetParentDevice();

            // SLI not supported
            if (pRemGpuDev->GetNumSubdevices() != 1)
                continue;

            Lwca::Device remLwdaDev;
            CHECK_RC(GetLwdaInstance().FindDevice(*pRemGpuDev, &remLwdaDev));
            if (remLwdaDev.GetAttribute(LW_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING))
            {
                *pRemoteLwdaDev = remLwdaDev;
                *pRouteToTest   = routes[i];
                return OK;
            }
        }

        // If we get here, there were no P2P lwlink connections and either
        // loopback isnt preferred or there are no loopback lwlink connections
        // either
        if (bLoopbackPresent)
        {
            *pRemoteLwdaDev = GetBoundLwdaDevice();
            *pRouteToTest   = pLoopbackRoute;
            return OK;
        }
        return RC::DEVICE_NOT_FOUND;
    }
    else if (m_TransferInterface == TransferInterface::C2C)
    {
        auto pC2C = GetBoundTestDevice()->GetInterface<C2C>();

        for (UINT32 c2cPartition = 0; c2cPartition < pC2C->GetMaxPartitions(); ++c2cPartition)
        {
            if (pC2C->GetActivePartitionMask() & (1U << c2cPartition))
            {
                Device::Id devId = pC2C->GetRemoteId(c2cPartition);

                // Check for the loopback case
                if (devId == GetBoundTestDevice()->GetId())
                {
                    *pRemoteLwdaDev = GetBoundLwdaDevice();
                    return RC::OK;
                }

                TestDeviceMgr *pTestDeviceMgr =
                    static_cast<TestDeviceMgr *>(DevMgrMgr::d_TestDeviceMgr);

                TestDevicePtr pRemTestDev;
                CHECK_RC(pTestDeviceMgr->GetDevice(devId, &pRemTestDev));
                GpuSubdevice *pRemSubdev = pRemTestDev->GetInterface<GpuSubdevice>();
                GpuDevice *pRemGpuDev = pRemSubdev->GetParentDevice();

                if (pRemGpuDev->GetNumSubdevices() != 1)
                    return RC::DEVICE_NOT_FOUND;

                Lwca::Device remLwdaDev;
                CHECK_RC(GetLwdaInstance().FindDevice(*pRemGpuDev, &remLwdaDev));
                if (remLwdaDev.GetAttribute(LW_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING))
                {
                    *pRemoteLwdaDev = remLwdaDev;
                    return RC::OK;
                }
            }
        }
        return RC::DEVICE_NOT_FOUND;
    }
    else
    {
        MASSERT(m_TransferInterface == TransferInterface::Pcie);

        // There will always be a PCIE connection found (if there is no other GPU
        // in the system then loopback will be used)
        Printf(Tee::PriWarn,
               "%s : Testing LWCA atomics over PCIE, expect failures!!\n",
               GetName().c_str());

        // Simply do loopback over PCIE
        if (m_PreferLoopback)
        {
            *pRemoteLwdaDev = GetBoundLwdaDevice();
            return OK;
        }

        GpuDevMgr *const pGpuMgr = static_cast<GpuDevMgr *>(DevMgrMgr::d_GraphDevMgr);
        GpuDevice *pRemGpuDev = pGpuMgr->GetFirstGpuDevice();
        while (pRemGpuDev && !pRemoteLwdaDev->IsValid())
        {
            if (pRemGpuDev == GetBoundGpuDevice())
            {
                pRemGpuDev = pGpuMgr->GetNextGpuDevice(pRemGpuDev);
                continue;
            }
            Lwca::Device remLwdaDev;
            CHECK_RC(GetLwdaInstance().FindDevice(*pRemGpuDev, &remLwdaDev));
            if (remLwdaDev.GetAttribute(LW_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING))
            {
                *pRemoteLwdaDev = remLwdaDev;
                return OK;
            }
            pRemGpuDev = pGpuMgr->GetNextGpuDevice(pRemGpuDev);
        }

        *pRemoteLwdaDev = GetBoundLwdaDevice();
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Starts the atomic kernel on the specified device
//!
//! \param dev   : Device to run the kernel on
//! \param pMod  : Pointer to returned Lwca module
//! \param pFunc : Pointer to returned Lwca function
//!
//! \return OK if starting the kernel is successful, not OK otherwise
RC MfgTest::LwdaPeerAtomics::CreateAtomicKernel
(
    Lwca::Device dev,
    Lwca::Module *pMod,
    Lwca::Function *pFunc
)
{
    RC rc;

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule(dev, "peeratomic", pMod));

    *pFunc = pMod->GetFunction("PeerAtomicInc", 1, m_ThreadCount);
    CHECK_RC(pFunc->InitCheck());
    return rc;
}

bool MfgTest::LwdaPeerAtomics::IsLoopback() const
{
    return m_RemoteLwdaDev.GetHandle() == GetBoundLwdaDevice().GetHandle();
}

RC MfgTest::LwdaPeerAtomics::Synchronize()
{
    RC rc;
    if (!IsLoopback())
    {
        CHECK_RC(GetLwdaInstance().SynchronizeContext(m_RemoteLwdaDev));
    }
    CHECK_RC(GetLwdaInstance().SynchronizeContext(GetBoundLwdaDevice()));
    return rc;
}

JS_CLASS_INHERIT(LwdaPeerAtomics, LwdaStreamTest, "LWCA atomics test");
CLASS_PROP_READWRITE(LwdaPeerAtomics, ThreadCount, UINT32,
                     "Number of threads to launch (default = 1024, minimum = 1024)");
CLASS_PROP_READWRITE(LwdaPeerAtomics, PerThreadIterations, UINT32,
                     "Number of iterations per thread (default = 1024, minimum = 1024)");
CLASS_PROP_READWRITE(LwdaPeerAtomics, TransferInterface, UINT08,
                     "Transfer interface to use, 0 = pcie, 1 = lwlink, 2 = c2c (default = 1)");
CLASS_PROP_READWRITE(LwdaPeerAtomics, PreferLoopback, bool,
                     "Prefer loopback connections when finding a connection to test (default = true)");
CLASS_PROP_READWRITE(LwdaPeerAtomics, RemoteStartTimeoutMs, UINT32,
                     "Timeout in ms for the local thread to wait for the remote thread to timeout (default = 1)");
CLASS_PROP_READWRITE(LwdaPeerAtomics, ShowPortStats, bool,
                     "Show the bytes/packets (lwlink) of flits (c2c) transmitted (default = false)");
PROP_CONST(LwdaPeerAtomics, TI_PCIE,   static_cast<UINT32>(MfgTest::LwdaPeerAtomics::TransferInterface::Pcie));
PROP_CONST(LwdaPeerAtomics, TI_LWLINK, static_cast<UINT32>(MfgTest::LwdaPeerAtomics::TransferInterface::LwLink));
PROP_CONST(LwdaPeerAtomics, TI_C2C,    static_cast<UINT32>(MfgTest::LwdaPeerAtomics::TransferInterface::C2C));
