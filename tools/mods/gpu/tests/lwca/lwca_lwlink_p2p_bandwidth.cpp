/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "device/interface/lwlink/lwlthroughputcounters.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/tests/lwca/lwlinkbandwidth/lwlinkp2pbandwidth.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"

using namespace LwLinkP2PBandwidth;

namespace MfgTest
{
    // -----------------------------------------------------------------------------
    //! LWCA-based LwLink P2P bandwidth test.
    //!
    //! ************ DEBUG AND EXPERIMENTAL ONLY
    //!
    //! This test simply uses SM to saturate LwLink bandwidth (P2P or loopback only)
    //!
    //! The simple kernel writes static values over P2P/Loopback to the target surface
    //! based on the block/thread.
    //!
    //! NO DATA IS VALIDATED FOR CORRECTNESS
    //! BANDWIDTH IS NOT VALIDATED FOR CORRECTNESS
    //!
    //! This test is lwrrently only for debug as a sanity check for bandwidth
    //!
    class LwdaLwLinkP2PBandwidth : public LwdaStreamTest
    {
    public:
        LwdaLwLinkP2PBandwidth();
        virtual ~LwdaLwLinkP2PBandwidth() { }
        virtual bool IsSupported();
        virtual RC Setup();
        virtual RC Run();
        virtual RC Cleanup();
        void PrintJsProperties(Tee::Priority pri);

        SETGET_PROP(ThreadCount,             UINT32);
        SETGET_PROP(Uint4PerThread,          UINT32);
        SETGET_PROP(AllowPcie,               bool);
        SETGET_PROP(PreferLoopback,          bool);
        SETGET_PROP(Iterations,              UINT32);

    private:
        RC GetRemoteLwdaDevice(Lwca::Device *pRemLwdaDev);
        bool IsLoopback() const;
        RC   Synchronize();
        RC   RunBandwidth(Lwca::Function &func, UINT32 blockWidth, UINT32 gridWidth, LwLinkP2PBandwidthInput *pInputData, UINT32 linkMask, bool bReads);

        Lwca::Device       m_RemoteLwdaDev; //!< Lwca remote device target

        //! Variables set from JS
        UINT32 m_ThreadCount;
        UINT32 m_Uint4PerThread;
        bool   m_AllowPcie;
        bool   m_PreferLoopback;
        UINT32 m_Iterations;

        static const UINT32 BYTES_PER_UINT4      = 16;
    };
}

using namespace MfgTest;

//------------------------------------------------------------------------------
//! \brief Constructor
//!
MfgTest::LwdaLwLinkP2PBandwidth::LwdaLwLinkP2PBandwidth()
  : m_ThreadCount(0)
   ,m_Uint4PerThread(8)
   ,m_AllowPcie(false)
   ,m_PreferLoopback(true)
   ,m_Iterations(256)
{
    SetName("LwdaLwLinkP2PBandwidth");
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
/* virtual */ bool MfgTest::LwdaLwLinkP2PBandwidth::IsSupported()
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

    // If unable to find a LwLink device for the GpuSubdevice then there are no
    // lwlinks to test so retport that the test is unsupported unless allowing
    // the test to run on PCIE (for testing purposes)
    if (!m_AllowPcie &&
        !GetBoundGpuSubdevice()->SupportsInterface<LwLink>())
    {
        return false;
    }

    // If unable to get a remote lwca device then test is not supported
    if (!m_RemoteLwdaDev.IsValid() && (OK != GetRemoteLwdaDevice(&m_RemoteLwdaDev)))
        return false;

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
/* virtual */ RC MfgTest::LwdaLwLinkP2PBandwidth::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    if (!m_RemoteLwdaDev.IsValid())
    {
        CHECK_RC(GetRemoteLwdaDevice(&m_RemoteLwdaDev));
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

    return rc;
}

RC MfgTest::LwdaLwLinkP2PBandwidth::RunBandwidth(Lwca::Function &func, UINT32 blockWidth, UINT32 gridWidth, LwLinkP2PBandwidthInput *pInputData, UINT32 linkMask, bool bReads)
{
    RC rc;

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLinkThroughputCounters * pTpc = nullptr;
    if (pTestDevice->SupportsInterface<LwLinkThroughputCounters>())
    {
        pTpc = pTestDevice->GetInterface<LwLinkThroughputCounters>();
    }
    else
    {
        // Should not reach here if LWLink is supported.
        // Print warning message and skip function procedure.
        Printf(Tee::PriWarn,
               "Throughput counters not supported on device. Skipping test run procedure.");
        return RC::OK;
    }

    UINT64 totalTimeNs      = 0;
    UINT64 totalBytes       = 0;
    UINT64 totalDataPackets = 0;

    INT32 bytesCounter   = -1;
    INT32 cyclesCounter  = -1;
    INT32 packetsCounter = -1;

    for (UINT32 lwrLoop = 0; lwrLoop < GetTestConfiguration()->Loops(); lwrLoop++)
    {
        CHECK_RC(pTpc->ClearThroughputCounters(linkMask));
        CHECK_RC(pTpc->StartThroughputCounters(linkMask));
        CHECK_RC(func.Launch(*pInputData));
        CHECK_RC(GetLwdaInstance().Synchronize(GetBoundLwdaDevice()));
        CHECK_RC(pTpc->StopThroughputCounters(linkMask));

        map<UINT32, vector<LwLinkThroughputCount>> counts;
        CHECK_RC(pTpc->GetThroughputCounters(linkMask, &counts));

        INT32 linkId = Utility::BitScanForward(linkMask, 0);

        if ((bytesCounter == -1) || (cyclesCounter == -1))
        {
            for (size_t ii = 0; ii < counts[linkId].size(); ii++)
            {
                switch (counts[linkId][ii].config.m_Cid)
                {
                    case LwLinkThroughputCount::CI_TX_COUNT_0:
                    case LwLinkThroughputCount::CI_TX_COUNT_1:
                    case LwLinkThroughputCount::CI_TX_COUNT_2:
                    case LwLinkThroughputCount::CI_TX_COUNT_3:
                        if (bReads)
                            continue;
                        break;
                    case LwLinkThroughputCount::CI_RX_COUNT_0:
                    case LwLinkThroughputCount::CI_RX_COUNT_1:
                    case LwLinkThroughputCount::CI_RX_COUNT_2:
                    case LwLinkThroughputCount::CI_RX_COUNT_3:
                        if (!bReads)
                            continue;
                        break;
                    default:
                        Printf(Tee::PriError, "%s : Unknown counter ID %d\n",
                                GetName().c_str(), counts[linkId][ii].config.m_Cid);
                        return RC::SOFTWARE_ERROR;
                }

                switch (counts[linkId][ii].config.m_Ut)
                {
                    case LwLinkThroughputCount::UT_CYCLES:
                        cyclesCounter = static_cast<INT32>(ii);
                        break;
                    case LwLinkThroughputCount::UT_PACKETS:
                        packetsCounter = static_cast<INT32>(ii);
                        break;
                    case LwLinkThroughputCount::UT_BYTES:
                        bytesCounter = static_cast<INT32>(ii);
                        break;
                    default:
                        Printf(Tee::PriError, "%s : Invalid counter unit type %d\n",
                                GetName().c_str(), counts[linkId][ii].config.m_Ut);
                        return RC::SOFTWARE_ERROR;
                }
            }

            if ((bytesCounter == -1) || (cyclesCounter == -1))
            {
                Printf(Tee::PriError, "%s : Required byte or cycle counter not present\n",
                        GetName().c_str());
                return RC::SOFTWARE_ERROR;
            }
        }

        auto pLwLink = pTestDevice->GetInterface<LwLink>();
        const UINT32 tpCntrRateMhz = pLwLink->GetLinkClockRateMhz(linkId);
        UINT64 activeCycles = 0;
        while (linkId != -1)
        {
            totalBytes       += counts[linkId][bytesCounter].countValue;
            if (packetsCounter != -1)
                totalDataPackets += counts[linkId][packetsCounter].countValue;
            if (counts[linkId][cyclesCounter].countValue > activeCycles)
                activeCycles = counts[linkId][cyclesCounter].countValue;
            linkId = Utility::BitScanForward(linkMask, linkId + 1);
        }
        totalTimeNs += (activeCycles * 1000ULL) / tpCntrRateMhz;
    }

    MASSERT(totalTimeNs > 0);
    FLOAT64 actualBwMBs = static_cast<FLOAT64>(totalBytes * 1000ULL) /  totalTimeNs;
    string bwSummary = Utility::StrPrintf("Grid Width              : %u\n"
                                          "Block Width             : %u\n"
                                          "Bytes PerLoop           : %llu\n"
                                          "Total Bytes Transferred : %llu\n",
                                          gridWidth,
                                          blockWidth,
                                          totalBytes / GetTestConfiguration()->Loops(),
                                          totalBytes);
    if (packetsCounter != -1)
        bwSummary += Utility::StrPrintf("Total Data Packets      : %llu\n", totalDataPackets);
    bwSummary += Utility::StrPrintf("Total Time              : %llu\n"
                                    "Total Bandwidth         : %8.3fMB/s\n",
                                    totalTimeNs,
                                    actualBwMBs);
    Printf(Tee::PriNormal, "\n%s\n", bwSummary.c_str());

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the test
//!
//! \return OK if setup succeeds, not OK otherwise
RC MfgTest::LwdaLwLinkP2PBandwidth::Run()
{
    StickyRC rc;

    // Size the block/grid to ensure 100% thread oclwpancy without violating
    // dimension requirements
    UINT32 gridWidth = 1;
    UINT32 blockWidth = m_ThreadCount;
    if (blockWidth == 0)
    {
        blockWidth = GetBoundLwdaDevice().GetShaderCount();
        blockWidth *=
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR);
        m_ThreadCount = blockWidth;
        VerbosePrintf("%s : Using %u threads\n", GetName().c_str(), m_ThreadCount);
    }
    const UINT32 maxBlockDimX =
        GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X);
    MASSERT(maxBlockDimX > 0);
    if (blockWidth > maxBlockDimX)
    {
        gridWidth  = blockWidth / maxBlockDimX;
        blockWidth = maxBlockDimX;
    }

    Lwca::DeviceMemory p2pMem;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_RemoteLwdaDev,
                                              blockWidth*gridWidth*m_Uint4PerThread*BYTES_PER_UINT4,
                                              &p2pMem));
    Lwca::DeviceMemory localMem;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_RemoteLwdaDev,
                                              blockWidth*gridWidth*m_Uint4PerThread*BYTES_PER_UINT4,
                                              &localMem));

    if (IsLoopback())
    {
        CHECK_RC(p2pMem.MapLoopback());
    }

    Lwca::Module   p2PBandwidthModule;
    Lwca::Function p2pFill;
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule(GetBoundLwdaDevice(),
                                                         "lwlinkp2pbandwidth",
                                                         &p2PBandwidthModule));

    p2pFill = p2PBandwidthModule.GetFunction("P2PFill", gridWidth, blockWidth);
    CHECK_RC(p2pFill.InitCheck());

    Lwca::Function p2pReads;
    p2pReads = p2PBandwidthModule.GetFunction("P2PReads", gridWidth, blockWidth);
    CHECK_RC(p2pReads.InitCheck());

    Lwca::Function p2pWrites;
    p2pWrites = p2PBandwidthModule.GetFunction("P2PWrites", gridWidth, blockWidth);
    CHECK_RC(p2pReads.InitCheck());

    UINT64 p2pPtr = p2pMem.GetDevicePtr();
    if (IsLoopback())
        p2pPtr = p2pMem.GetLoopbackDevicePtr();
    LwLinkP2PBandwidthInput inputData = { p2pPtr, localMem.GetDevicePtr(), m_Uint4PerThread, m_Iterations };

    TestDevicePtr pTestDevice = GetBoundTestDevice();
    LwLink * pLwLink = pTestDevice->GetInterface<LwLink>();

    // Monitor the throughput counters to ensure data is going over lwlink
    vector<LwLinkThroughputCount::Config> configs =
    {
         { LwLinkThroughputCount::CI_TX_COUNT_0, LwLinkThroughputCount::UT_BYTES,   LwLinkThroughputCount::FF_ALL,  LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
        ,{ LwLinkThroughputCount::CI_TX_COUNT_1, LwLinkThroughputCount::UT_CYCLES,  LwLinkThroughputCount::FF_ALL,  LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
        ,{ LwLinkThroughputCount::CI_RX_COUNT_0, LwLinkThroughputCount::UT_BYTES,   LwLinkThroughputCount::FF_ALL,  LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
        ,{ LwLinkThroughputCount::CI_RX_COUNT_1, LwLinkThroughputCount::UT_CYCLES,  LwLinkThroughputCount::FF_ALL,  LwLinkThroughputCount::PF_ALL_EXCEPT_NOP, LwLinkThroughputCount::PS_1 }  //$
    };

    LwLinkThroughputCounters * pTpc = nullptr;
    UINT64 linkMask = 0;
    if (pTestDevice->SupportsInterface<LwLinkThroughputCounters>())
    {
        pTpc = pTestDevice->GetInterface<LwLinkThroughputCounters>();
        if (pTpc->IsCounterSupported(LwLinkThroughputCount::CI_TX_COUNT_2))
        {
            configs.emplace_back(LwLinkThroughputCount::CI_TX_COUNT_2, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_DATA, LwLinkThroughputCount::PF_DATA_ONLY, LwLinkThroughputCount::PS_1);
        }
        if (pTpc->IsCounterSupported(LwLinkThroughputCount::CI_RX_COUNT_2))
        {
            configs.emplace_back(LwLinkThroughputCount::CI_RX_COUNT_2, LwLinkThroughputCount::UT_PACKETS, LwLinkThroughputCount::FF_DATA, LwLinkThroughputCount::PF_DATA_ONLY, LwLinkThroughputCount::PS_1);
        }
        for (UINT32 lwrLinkId = 0; lwrLinkId < pLwLink->GetMaxLinks(); lwrLinkId++)
        {
            if (!pLwLink->IsLinkActive(lwrLinkId))
                continue;
            linkMask |= (1ULL << lwrLinkId);
        }

        CHECK_RC(pTpc->SetupThroughputCounters(linkMask, configs));
    }

    CHECK_RC(RunBandwidth(p2pFill,   blockWidth, gridWidth, &inputData, linkMask, false));
    CHECK_RC(RunBandwidth(p2pWrites, blockWidth, gridWidth, &inputData, linkMask, false));
    CHECK_RC(RunBandwidth(p2pReads,  blockWidth, gridWidth, &inputData, linkMask, true));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Cleanup the test
//!
//! \return OK if cleanup succeeds, not OK otherwise
RC MfgTest::LwdaLwLinkP2PBandwidth::Cleanup()
{
    StickyRC rc;

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
void MfgTest::LwdaLwLinkP2PBandwidth::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tThreadCount:             %u\n", m_ThreadCount);
    Printf(pri, "\tUint4PerThread:          %u\n", m_Uint4PerThread);
    Printf(pri, "\tIterations:              %u\n", m_Iterations);
    Printf(pri, "\tAllowPcie:               %s\n", m_AllowPcie ? "true" : "false");
    Printf(pri, "\tPreferLoopback:          %s\n", m_PreferLoopback ? "true" : "false");

    LwdaStreamTest::PrintJsProperties(pri);
}

//-----------------------------------------------------------------------------
//! \brief Gets the remote Lwca device to run the kernel on
//!
//! \param pRemoteLwdaDev : Pointer to returned remote device
//!
//! \return OK if a remote lwca device is found, not OK otherwise
RC MfgTest::LwdaLwLinkP2PBandwidth::GetRemoteLwdaDevice(Lwca::Device *pRemoteLwdaDev)
{
    RC rc;

    // If there are LwLink routes on the current GPU, look for a remote device
    // on one of the LwLink routes first
    if (GetBoundGpuSubdevice()->SupportsInterface<LwLink>())
    {
        vector<LwLinkRoutePtr> routes;
        TestDevicePtr pTestDevice = GetBoundTestDevice();
        CHECK_RC(LwLinkDevIf::TopologyManager::GetRoutesOnDevice(pTestDevice, &routes));

        bool bLoopbackPresent = false;
        for (size_t i = 0; (i < routes.size()) && !bLoopbackPresent; i++)
            bLoopbackPresent = routes[i]->IsLoop();

        // Choose loopback as the first option for a remote device is it is
        // preferred
        if (m_PreferLoopback && bLoopbackPresent)
        {
            *pRemoteLwdaDev = GetBoundLwdaDevice();
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
                return OK;
            }
        }

        // If we get here, there were no P2P lwlink connections and either
        // loopback isnt preferred or there are no loopback lwlink connections
        // either
        if (bLoopbackPresent)
        {
            *pRemoteLwdaDev = GetBoundLwdaDevice();
            return OK;
        }
    }

    // No LwLink devices found, if PCIE devices are not allowed then fail
    if (!m_AllowPcie)
        return RC::DEVICE_NOT_FOUND;

    // There will always be a PCIE connection found (if there is no other GPU
    // in the system then loopback will be used)
    Printf(Tee::PriNormal,
           "%s : Testing LWCA lwlink P2P over PCIE, expect failures!!\n",
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
    return OK;
}

bool MfgTest::LwdaLwLinkP2PBandwidth::IsLoopback() const
{
    return m_RemoteLwdaDev.GetHandle() == GetBoundLwdaDevice().GetHandle();
}

JS_CLASS_INHERIT(LwdaLwLinkP2PBandwidth, LwdaStreamTest, "LWCA atomics test");
CLASS_PROP_READWRITE(LwdaLwLinkP2PBandwidth, ThreadCount, UINT32,
                     "Number of threads to launch, 0 is max threads (default = 0)");
CLASS_PROP_READWRITE(LwdaLwLinkP2PBandwidth, Uint4PerThread, UINT32,
                     "Number of uint4 (128bit values) to test per thread (default = 128)");
CLASS_PROP_READWRITE(LwdaLwLinkP2PBandwidth, AllowPcie, bool,
                     "Allow the test to run on PCIE connections (default = false)");
CLASS_PROP_READWRITE(LwdaLwLinkP2PBandwidth, PreferLoopback, bool,
                 "Prefer loopback connections when finding a connection to test (default = true)");
CLASS_PROP_READWRITE(LwdaLwLinkP2PBandwidth, Iterations, UINT32,
                 "Number of iterations to run (default = 16)");
