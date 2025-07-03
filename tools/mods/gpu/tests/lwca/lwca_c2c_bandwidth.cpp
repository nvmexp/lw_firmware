/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "device/interface/c2c.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/tests/lwca/c2cbandwidth/c2cbandwidth.h"
#include "gpu/tests/lwdastst.h"

namespace MfgTest
{
    class LwdaC2CBandwidth : public LwdaStreamTest
    {
    public:
        LwdaC2CBandwidth();
        virtual ~LwdaC2CBandwidth() { }
        bool IsSupported() override;
        RC Setup() override;
        RC Run() override;
        RC Cleanup() override;
        void PrintJsProperties(Tee::Priority pri) override;

        SETGET_PROP(ThreadCount,          UINT32);
        SETGET_PROP(Uint4PerThread,       UINT32);
        SETGET_PROP(Iterations,           UINT32);
        SETGET_PROP_LWSTOM(TransferType,  UINT08);
        SETGET_PROP(SrcMemMode,           UINT32);
        SETGET_PROP(SrcMemModePercent,    UINT32);
        SETGET_PROP(BwThresholdPercent,   UINT32);
        SETGET_PROP(ShowBandwidthData,    bool);
        SETGET_PROP(SkipBandwidthCheck,   bool);
        SETGET_PROP(SkipSurfaceCheck,     bool);
        SETGET_PROP(NumErrorsToPrint,     UINT32);
        SETGET_PROP(UseFills,             bool);
        SETGET_PROP(RuntimeMs,            UINT32);
        SETGET_PROP(ShowPerfMonStats,     bool);

    private:
        enum class TransferType : UINT08
        {
            READS,
            WRITES,
            READS_AND_WRITES
        };
        enum class DeviceMemLoc : UINT08
        {
            LOCAL,
            REMOTE
        };

        RC CheckBandwidth(TransferType tt, float elapsedTimeMs, DeviceMemLoc memLoc);
        RC GetRemoteDevice();
        RC InitializeMemory();
        RC InnerRun(TransferType tt, UINT64 startTimeMs);
        RC VerifyDestinationMemory
        (
            Lwca::DeviceMemory     * pDstMemory,
            C2CBandwidthTestMemory   srcMem,
            DeviceMemLoc             memLoc
        );

        Lwca::Module         m_LocalC2CModule;
        Lwca::Function       m_LocalBwFunc;

        TestDevicePtr        m_pRemoteDevice;
        Lwca::Device         m_RemoteLwdaDev;
        Lwca::Stream         m_RemoteStream;
        Lwca::Module         m_RemoteC2CModule;
        Lwca::Function       m_RemoteBwFunc;

        bool                 m_bLoopback  = false;

        vector<Lwca::DeviceMemory> m_LocalMemory;
        vector<Lwca::DeviceMemory> m_RemoteMemory;
        vector<Lwca::HostMemory>   m_HostMemory;
        UINT32                     m_BlockWidth = 0;
        UINT32                     m_GridWidth  = 1;

        //! Variables set from JS
        UINT32       m_ThreadCount        = 0;
        UINT32       m_Uint4PerThread     = 8;
        UINT32       m_Iterations         = 8;
        TransferType m_TransferType       = TransferType::READS_AND_WRITES;
        UINT32       m_SrcMemMode         = 4;
        UINT32       m_SrcMemModePercent  = 50;
        UINT32       m_BwThresholdPercent = 97;
        bool         m_ShowBandwidthData  = false;
        bool         m_SkipBandwidthCheck = false;
        bool         m_SkipSurfaceCheck   = false;
        UINT32       m_NumErrorsToPrint   = 10;
        bool         m_UseFills           = false;
        UINT32       m_RuntimeMs          = 0;
        bool         m_ShowPerfMonStats   = false;

        static const UINT32 BYTES_PER_UINT4      = 16;
        static const UINT32 MIN_THREADS          = 512;
    };
}

using namespace MfgTest;

//------------------------------------------------------------------------------
MfgTest::LwdaC2CBandwidth::LwdaC2CBandwidth()
{
    SetName("LwdaC2CBandwidth");
}

//------------------------------------------------------------------------------
bool MfgTest::LwdaC2CBandwidth::IsSupported()
{
    // Check if compute is supported at all
    if (!LwdaStreamTest::IsSupported())
    {
        Printf(Tee::PriLow, "%s : LwdaStreamTest is not supported\n", GetName().c_str());
        return false;
    }

    if (GetBoundTestDevice()->IsSOC())
    {
        Printf(Tee::PriLow, "%s : Not supported on SOC\n", GetName().c_str());
        return false;
    }

    if (!GetBoundTestDevice()->SupportsInterface<C2C>())
    {
        Printf(Tee::PriLow, "%s : C2C interface is not supported\n", GetName().c_str());
        return false;
    }

    // The kernel is compiled only for compute 6.x+
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_90)
    {
        Printf(Tee::PriLow, "%s : Not supported on current SM architecture\n", GetName().c_str());
        return false;
    }

    // Unsupported on SLI
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
    {
        Printf(Tee::PriLow, "%s : Not supported on SLI devices\n", GetName().c_str());
        return false;
    }

    // If unable to get a remote lwca device then test is not supported
    if (!m_RemoteLwdaDev.IsValid() && (RC::OK != GetRemoteDevice()))
    {
        Printf(Tee::PriLow,
               "%s : Not supported due to no remote device found\n", GetName().c_str());
        return false;
    }

    // Lwca P2P requires unified addressing
    if (!m_bLoopback &&
        !GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING))
    {
        Printf(Tee::PriLow, "%s : P2P not supported because UVA is disabled\n", GetName().c_str());
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------
RC MfgTest::LwdaC2CBandwidth::Setup()
{
    // Fail here in case the test was attempted to be forced on the command line, it will fail
    // naturally for any other IsSupported conditions, skipping this check would result in a
    // seg fault
    if (!GetBoundTestDevice()->SupportsInterface<C2C>())
    {
        Printf(Tee::PriError, "%s : C2C interface not supported on %s\n",
               GetName().c_str(), GetBoundTestDevice()->GetName().c_str());
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!GetBoundTestDevice()->GetInterface<C2C>()->GetActivePartitionMask())
    {
        Printf(Tee::PriError, "%s : C2C interface not initialized on %s\n",
               GetName().c_str(), GetBoundTestDevice()->GetName().c_str());
        return RC::WAS_NOT_INITIALIZED;
    }

    if ((m_ThreadCount != 0) && (m_ThreadCount < MIN_THREADS))
    {
        Printf(Tee::PriError, "%s : A minimum of %u threads is required\n",
               GetName().c_str(), MIN_THREADS);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_UseFills)
    {
        if (m_TransferType != TransferType::WRITES)
        {
            Printf(Tee::PriError, "%s : Bandwidth via fills only supported with writes\n",
                   GetName().c_str());
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        if (!m_SkipSurfaceCheck)
        {
            Printf(Tee::PriWarn,
                   "%s : Surface checking not supported with fill bandwidth, disabling\n",
                   GetName().c_str());
            m_SkipSurfaceCheck = false;
        }
    }

    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    if (!m_RemoteLwdaDev.IsValid())
    {
        CHECK_RC(GetRemoteDevice());
    }

    // Size the block/grid to ensure 100% thread oclwpancy without violating
    // dimension requirements
    m_GridWidth = 1;
    m_BlockWidth = m_ThreadCount;
    if (m_BlockWidth == 0)
    {
        m_BlockWidth = GetBoundLwdaDevice().GetShaderCount();
        m_BlockWidth *=
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR);
        m_ThreadCount = m_BlockWidth;
        VerbosePrintf("%s : Using %u threads\n", GetName().c_str(), m_ThreadCount);
    }
    const UINT32 maxBlockDimX =
        GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X);
    MASSERT(maxBlockDimX > 0);
    if (m_BlockWidth > maxBlockDimX)
    {
        m_GridWidth  = m_BlockWidth / maxBlockDimX;
        m_BlockWidth = maxBlockDimX;
    }

    m_HostMemory.resize(MEM_COUNT);
    m_LocalMemory.resize(MEM_COUNT);

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule(GetBoundLwdaDevice(),
                                                         "c2cbandwidth",
                                                         &m_LocalC2CModule));
    m_LocalBwFunc = m_LocalC2CModule.GetFunction(m_UseFills ? "C2CFill" : "C2CMemCopy",
                                                 m_GridWidth,
                                                 m_BlockWidth);
    CHECK_RC(m_LocalBwFunc.InitCheck());

    if (m_bLoopback)
    {
        CHECK_RC(GetLwdaInstance().EnableLoopbackAccess(GetBoundLwdaDevice()));
    }
    else if (m_RemoteLwdaDev.IsValid())
    {
        CHECK_RC(GetLwdaInstance().InitContext(m_RemoteLwdaDev));
        CHECK_RC(GetLwdaInstance().EnablePeerAccess(GetBoundLwdaDevice(), m_RemoteLwdaDev));
        CHECK_RC(GetLwdaInstance().EnablePeerAccess(m_RemoteLwdaDev, GetBoundLwdaDevice()));
        m_RemoteMemory.resize(MEM_COUNT);
        m_RemoteStream = GetLwdaInstance().CreateStream(m_RemoteLwdaDev);

        CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule(m_RemoteLwdaDev,
                                                             "c2cbandwidth",
                                                             &m_RemoteC2CModule));
        m_RemoteBwFunc = m_RemoteC2CModule.GetFunction(m_UseFills ? "C2CFill" : "C2CMemCopy",
                                                       m_GridWidth,
                                                       m_BlockWidth);
        CHECK_RC(m_RemoteBwFunc.InitCheck());
    }
    CHECK_RC(InitializeMemory());

    return rc;
}
//------------------------------------------------------------------------------
RC MfgTest::LwdaC2CBandwidth::Run()
{
    RC rc;
    const UINT64 startTimeMs = Xp::GetWallTimeMS();
    bool bDone = false;
    auto pC2C = GetBoundTestDevice()->GetInterface<C2C>();
    while (!bDone)
    {
        if ((GetTransferType() == static_cast<UINT08>(TransferType::READS)) ||
            (GetTransferType() == static_cast<UINT08>(TransferType::READS_AND_WRITES)))
        {
            if (m_ShowPerfMonStats)
            {
                CHECK_RC(pC2C->ConfigurePerfMon(0, C2C::PerfMonDataType::RxDataAllVCs));
                CHECK_RC(pC2C->StartPerfMon());
            }
            CHECK_RC(InnerRun(TransferType::READS, startTimeMs));
            if (m_ShowPerfMonStats)
            {
                map<UINT32, vector<C2C::PerfMonCount>> pmCounts;
                CHECK_RC(pC2C->StopPerfMon(&pmCounts));
                UINT64 totalFlits = 0;
                for (auto const & lwrLinkCounts : pmCounts)
                {
                    for (auto const & lwrCount : lwrLinkCounts.second)
                    {
                        Printf(Tee::PriNormal, "C2C link %u total read data flits : %llu\n",
                               lwrLinkCounts.first, lwrCount.pmCount);
                        totalFlits += lwrCount.pmCount;
                    }
                }
                Printf(Tee::PriNormal, "C2C total read data flits : %llu\n", totalFlits);
            }
        }
        if (m_RuntimeMs && (Xp::GetWallTimeMS() - startTimeMs) >= m_RuntimeMs)
        {
            break;
        }
        if ((GetTransferType() == static_cast<UINT08>(TransferType::WRITES)) ||
            (GetTransferType() == static_cast<UINT08>(TransferType::READS_AND_WRITES)))
        {
            if (m_ShowPerfMonStats)
            {
                CHECK_RC(pC2C->ConfigurePerfMon(0, C2C::PerfMonDataType::TxDataAllVCs));
                CHECK_RC(pC2C->StartPerfMon());
            }
            CHECK_RC(InnerRun(TransferType::WRITES, startTimeMs));
            if (m_ShowPerfMonStats)
            {
                map<UINT32, vector<C2C::PerfMonCount>> pmCounts;
                CHECK_RC(pC2C->StopPerfMon(&pmCounts));
                UINT64 totalFlits = 0;
                for (auto const & lwrLinkCounts : pmCounts)
                {
                    for (auto const & lwrCount : lwrLinkCounts.second)
                    {
                        Printf(Tee::PriNormal, "C2C link %u total write data flits : %llu\n",
                               lwrLinkCounts.first, lwrCount.pmCount);
                        totalFlits += lwrCount.pmCount;
                    }
                }
                Printf(Tee::PriNormal, "C2C total write data flits : %llu\n", totalFlits);
            }
        }
        bDone = !m_RuntimeMs || (Xp::GetWallTimeMS() - startTimeMs) >= m_RuntimeMs;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC MfgTest::LwdaC2CBandwidth::Cleanup()
{
    StickyRC rc;

    if (m_LocalC2CModule.IsValid())
        m_LocalC2CModule.Unload();
    if (m_RemoteC2CModule.IsValid())
        m_RemoteC2CModule.Unload();
    if (m_RemoteStream.IsValid())
        m_RemoteStream.Free();

    for (UINT32 lwrMemIdx = 0; lwrMemIdx < MEM_COUNT; lwrMemIdx++)
    {
        if (m_LocalMemory[lwrMemIdx].IsValid())
        {
            if (m_bLoopback)
                rc = m_LocalMemory[lwrMemIdx].UnmapLoopback();
            m_LocalMemory[lwrMemIdx].Free();
        }
        if ((lwrMemIdx < m_RemoteMemory.size()) && m_RemoteMemory[lwrMemIdx].IsValid())
            m_RemoteMemory[lwrMemIdx].Free();
        m_HostMemory[lwrMemIdx].Free();
    }

    if (m_bLoopback)
    {
        rc = GetLwdaInstance().DisableLoopbackAccess(GetBoundLwdaDevice());
    }
    else if (m_RemoteLwdaDev.IsValid())
    {
        rc = GetLwdaInstance().DisablePeerAccess(m_RemoteLwdaDev, GetBoundLwdaDevice());
        rc = GetLwdaInstance().DisablePeerAccess(GetBoundLwdaDevice(), m_RemoteLwdaDev);
    }
    rc = LwdaStreamTest::Cleanup();
    return rc;
}

//-----------------------------------------------------------------------------
void MfgTest::LwdaC2CBandwidth::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tThreadCount:             %u\n", m_ThreadCount);
    Printf(pri, "\tUint4PerThread:          %u\n", m_Uint4PerThread);
    Printf(pri, "\tIterations:              %u\n", m_Iterations);
    string ttStr;
    switch (m_TransferType)
    {
        case TransferType::READS:            ttStr = "Reads"; break;
        case TransferType::WRITES:           ttStr = "Writes"; break;
        case TransferType::READS_AND_WRITES: ttStr = "Reads/Writes"; break;
        default:
            MASSERT(!"Unknown transfer type");
    }
    Printf(pri, "\tTransferType:                   %s\n",   ttStr.c_str());
    Printf(pri, "\tSrcMemMode:                     %u\n",   m_SrcMemMode);
    Printf(pri, "\tSrcMemModePercent:              %u\n",   m_SrcMemModePercent);
    Printf(pri, "\tBwThresholdPercent:             %u\n",   m_BwThresholdPercent);
    Printf(pri, "\tShowBandwidthData:              %s\n",   m_ShowBandwidthData ? "true" : "false");  //$
    Printf(pri, "\tSkipBandwidthCheck:             %s\n",   m_SkipBandwidthCheck ? "true" : "false"); //$
    Printf(pri, "\tSkipSurfaceCheck:               %s\n",   m_SkipSurfaceCheck ? "true" : "false");   //$
    Printf(pri, "\tNumErrorsToPrint:               %u\n",   m_NumErrorsToPrint);
    Printf(pri, "\tUseFills:                       %s\n",   m_UseFills ? "true" : "false");
    Printf(pri, "\tRuntimeMs:                      %u\n",   m_RuntimeMs);
    Printf(pri, "\tShowPerfMonStats:               %s\n",   m_ShowPerfMonStats ? "true" : "false");   //$

    LwdaStreamTest::PrintJsProperties(pri);
}

// -----------------------------------------------------------------------------
UINT08 LwdaC2CBandwidth::GetTransferType() const
{
    return static_cast<UINT08>(m_TransferType);
}

// -----------------------------------------------------------------------------
RC LwdaC2CBandwidth::SetTransferType(UINT08 transferType)
{
    if (transferType > static_cast<UINT08>(TransferType::READS_AND_WRITES))
        return RC::BAD_PARAMETER;
    m_TransferType = static_cast<TransferType>(transferType);
    return RC::OK;
}

// -----------------------------------------------------------------------------
RC LwdaC2CBandwidth::CheckBandwidth(TransferType tt, float elapsedTimeMs, DeviceMemLoc memLoc)
{
    RC rc;

    auto pC2C = GetBoundGpuSubdevice()->GetInterface<C2C>();

    const UINT32 partitionId = Utility::BitScanForward(pC2C->GetActivePartitionMask());
    const UINT32 partitionBwKiBps = pC2C->GetPartitionBandwidthKiBps(partitionId);
    const UINT64 rawBwKiBps  = static_cast<UINT64>(partitionBwKiBps) *
                               Utility::CountBits(pC2C->GetActivePartitionMask());

    const UINT64 elapsedTimeUs = static_cast<UINT64>(1000.0 * elapsedTimeMs);
    const UINT32 bytesPerIteration =
        m_BlockWidth * m_GridWidth * m_Uint4PerThread * BYTES_PER_UINT4;
    const UINT64 totalBytes =
        static_cast<UINT64>(bytesPerIteration) * GetTestConfiguration()->Loops() * m_Iterations;
    const UINT64 actualBwKiBps = static_cast<UINT64>(totalBytes * 1e6 / 1024ULL / elapsedTimeUs);

    string bwSummaryString =
        Utility::StrPrintf("Testing C2C via SM on %s\n"
                           "---------------------------------------------------\n",
                           GetBoundTestDevice()->GetName().c_str());

    TransferType bwTransferType = tt;
    if (m_bLoopback)
    {
        // Loopback and remote GPU (bidirectional traffic) are considered R/W
        bwTransferType = TransferType::READS_AND_WRITES;
        bwSummaryString += Utility::StrPrintf("Remote Device                    = %s\n",
                                              GetBoundTestDevice()->GetName().c_str());
        bwSummaryString +=
            Utility::StrPrintf("Transfer Method                  = Loopback Unidirectional %s\n",
                               (tt == TransferType::READS) ? "Read" : "Write");
    }
    else if (m_RemoteLwdaDev.IsValid())
    {
        // Loopback and remote GPU (bidirectional traffic) are considered R/W
        bwTransferType = TransferType::READS_AND_WRITES;
        bwSummaryString += Utility::StrPrintf("Remote Device                    = %s\n",
                                              m_pRemoteDevice->GetName().c_str());
        bwSummaryString +=
            Utility::StrPrintf("Transfer Method                  = Bidirectional %s\n",
                               (tt == TransferType::READS) ? "Read" : "Write");
        bwSummaryString +=
            Utility::StrPrintf("Transfer Source                  = %s\n",
                               (memLoc == DeviceMemLoc::LOCAL) ?
                                   GetBoundTestDevice()->GetName().c_str() :
                                   m_pRemoteDevice->GetName().c_str());
    }
    else
    {
        bwSummaryString += Utility::StrPrintf("Remote Device                    = Sysmem\n");
        bwSummaryString +=
            Utility::StrPrintf("Transfer Method                  = Unidirectional %s\n",
                               (tt == TransferType::READS) ? "Read" : "Write");
    }

    UINT64 dataBwKiBps = rawBwKiBps;
    switch (bwTransferType)
    {
        case TransferType::READS:
            dataBwKiBps = static_cast<UINT64>((static_cast<FLOAT64>(dataBwKiBps) * 0.91));
            break;
        case TransferType::WRITES:
            dataBwKiBps = static_cast<UINT64>((static_cast<FLOAT64>(dataBwKiBps) * 0.85));
            break;
        case TransferType::READS_AND_WRITES:
            dataBwKiBps = static_cast<UINT64>((static_cast<FLOAT64>(dataBwKiBps) * 0.76));
            break;
        default:
            MASSERT(!"Unknown transfer type");
            return RC::BAD_PARAMETER;

    }
    const UINT64 minimumBwKiBps = dataBwKiBps * m_BwThresholdPercent / 100ULL;
    const FLOAT64 percentRawBw = static_cast<FLOAT64>(actualBwKiBps) * 100.0 / dataBwKiBps;

    if (!m_RemoteLwdaDev.IsValid())
    {
        bwSummaryString += Utility::StrPrintf("Transfer Source                  = %s\n",
                                              GetBoundTestDevice()->GetName().c_str());
    }

    if (m_bLoopback || m_RemoteLwdaDev.IsValid())
    {
        bwSummaryString += Utility::StrPrintf("Transfer Direction               = I/O\n");
    }
    else
    {
        bwSummaryString += Utility::StrPrintf("Transfer Direction               = %s\n",
                                              (tt == TransferType::READS) ? "I" : "O");
    }
    if (tt == TransferType::READS)
    {
        bwSummaryString += Utility::StrPrintf("Measurement Direction            = %s\n",
                                              (memLoc == DeviceMemLoc::LOCAL) ? "I" : "O");
    }
    else
    {
        bwSummaryString += Utility::StrPrintf("Measurement Direction            = %s\n",
                                              (memLoc == DeviceMemLoc::LOCAL) ? "O" : "I");
    }
    bwSummaryString += Utility::StrPrintf("Total Threads                    = %u\n"
                                          "Bytes Per Iteration              = %u\n"
                                          "Iterations                       = %u\n"
                                          "Loops                            = %u\n"
                                          "Total Bytes Transferred          = %llu\n"
                                          "Elapsed Time                     = %.3f ms\n"
                                          "C2C Per-Partition Raw Bandwidth  = %u KiBps\n"
                                          "C2C Total Partitions             = %u\n"
                                          "Total Raw Bandwidth              = %llu KiBps\n"
                                          "Maximum Data Bandwidth           = %llu KiBps\n"
                                          "Bandwidth Threshold              = %.1f%%\n"
                                          "Minimum Bandwidth                = %llu KiBps\n"
                                          "Actual Bandwidth                 = %llu KiBps\n"
                                          "Percent Raw Bandwidth            = %.1f%%\n",
                                          m_GridWidth * m_BlockWidth,
                                          bytesPerIteration,
                                          m_Iterations,
                                          GetTestConfiguration()->Loops(),
                                          totalBytes,
                                          elapsedTimeMs,
                                          partitionBwKiBps,
                                          Utility::CountBits(pC2C->GetActivePartitionMask()),
                                          rawBwKiBps,
                                          dataBwKiBps,
                                          static_cast<FLOAT32>(m_BwThresholdPercent),
                                          minimumBwKiBps,
                                          actualBwKiBps,
                                          percentRawBw);
    if (actualBwKiBps < minimumBwKiBps)
        bwSummaryString += "\n******* Bandwidth too low! *******\n";

    const bool bBwFail = ((actualBwKiBps < minimumBwKiBps) && !m_SkipBandwidthCheck);

    Tee::Priority pri = m_ShowBandwidthData ? Tee::PriNormal : GetVerbosePrintPri();
    if (bBwFail)
        pri = m_SkipBandwidthCheck ? Tee::PriWarn : Tee::PriError;

    Printf(pri, "%s\n", bwSummaryString.c_str());
    return bBwFail ? RC::BANDWIDTH_OUT_OF_RANGE : RC::OK;
}

//-----------------------------------------------------------------------------
RC MfgTest::LwdaC2CBandwidth::GetRemoteDevice()
{
    RC rc;

    auto pC2C = GetBoundGpuSubdevice()->GetInterface<C2C>();
    for (UINT32 c2cPartition = 0; c2cPartition < pC2C->GetMaxPartitions(); ++c2cPartition)
    {
        if (pC2C->GetConnectionType(c2cPartition) == C2C::ConnectionType::GPU)
        {
            if (pC2C->GetRemoteId(c2cPartition) == GetBoundTestDevice()->GetId())
            {
                m_bLoopback = true;
                return RC::OK;
            }

            TestDeviceMgr *pTestDeviceMgr =
                static_cast<TestDeviceMgr *>(DevMgrMgr::d_TestDeviceMgr);

            TestDevicePtr pTestDevice;
            CHECK_RC(pTestDeviceMgr->GetDevice(pC2C->GetRemoteId(c2cPartition), &pTestDevice));

            Lwca::Device remLwdaDev;
            GpuDevice *pRemoteDevice =
                pTestDevice->GetInterface<GpuSubdevice>()->GetParentDevice();
            CHECK_RC(GetLwdaInstance().FindDevice(*pRemoteDevice, &remLwdaDev));
            if (remLwdaDev.GetAttribute(LW_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING))
            {
                m_RemoteLwdaDev = remLwdaDev;
                m_pRemoteDevice = pTestDevice;
                return RC::OK;
            }
            else
            {
                Printf(Tee::PriError, "%s : Remote device does not support UVA\n",
                       pTestDevice->GetName().c_str());
                return RC::UNSUPPORTED_SYSTEM_CONFIG;
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC MfgTest::LwdaC2CBandwidth::InitializeMemory()
{
    // Always initialize the host memory even if C2C is not connected to sysmem since
    // the host memory will be used to initialize device memory
    RC rc;

    const UINT32 memSize = m_BlockWidth * m_GridWidth * m_Uint4PerThread * BYTES_PER_UINT4;
    const bool bSrcBInUse = (m_SrcMemMode == 0) || (m_SrcMemMode > 4);
    for (UINT32 lwrMem = 0; lwrMem < MEM_COUNT; lwrMem++)
    {
        if (m_UseFills && (lwrMem != DST_MEM))
            continue;

        if (!bSrcBInUse && (lwrMem == SRC_MEM_B))
            continue;

        CHECK_RC(GetLwdaInstance().AllocHostMem(GetBoundLwdaDevice(),
                                                memSize,
                                                &m_HostMemory[lwrMem]));
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(GetBoundLwdaDevice(),
                                                  memSize,
                                                  &m_LocalMemory[lwrMem]));
        if (m_RemoteLwdaDev.IsValid())
        {
            CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_RemoteLwdaDev,
                                                      memSize,
                                                      &m_RemoteMemory[lwrMem]));
        }
        if (m_bLoopback)
        {
            CHECK_RC(m_LocalMemory[lwrMem].MapLoopback());
        }
    }

    // When using fills, no source surfaces need initialization
    if (m_UseFills)
        return rc;

    {
        Tasker::DetachThread detachThread;

        vector<UINT32> pattern;

        const UINT32 topSize = ALIGN_DOWN(memSize * m_SrcMemModePercent / m_SrcMemModePercent, 4U);
        const UINT32 bottomSize = memSize - topSize;
        UINT08 * pMemA = static_cast<UINT08 *>(m_HostMemory[SRC_MEM_A].GetPtr());
        UINT08 * pMemB = static_cast<UINT08 *>(m_HostMemory[SRC_MEM_B].GetPtr());
        switch (m_SrcMemMode)
        {
            case 0:
                pattern.reserve(1024);
                pattern.insert(pattern.end(), 128, 0xFFFFFFFF);
                pattern.insert(pattern.end(), 128, 0xFFFFFC00);
                pattern.insert(pattern.end(), 128, 0xC00FFFFF);
                pattern.insert(pattern.end(), 128, 0xC00FFC00);
                pattern.insert(pattern.end(), 128, 0xFFF003FF);
                pattern.insert(pattern.end(), 128, 0xFFF00000);
                pattern.insert(pattern.end(), 128, 0xC00003FF);
                pattern.insert(pattern.end(), 128, 0xC0000000);

                CHECK_RC(Memory::FillRandom32(pMemA, GetTestConfiguration()->Seed(), topSize));
                CHECK_RC(Memory::FillPattern(pMemA + topSize, &pattern, bottomSize, 0));

                CHECK_RC(Memory::FillPattern(pMemB, &pattern, topSize, 0));
                CHECK_RC(Memory::FillRandom32(pMemB + topSize,
                                              GetTestConfiguration()->Seed(),
                                              bottomSize));
                break;
            case 1:
                CHECK_RC(Memory::FillRandom32(pMemA, GetTestConfiguration()->Seed(), memSize));
                break;
            case 2:
                m_HostMemory[SRC_MEM_A].Clear();
                break;
            case 3:
                pattern.push_back(0x00000000);
                pattern.push_back(0xFFFFFFFF);
                CHECK_RC(Memory::FillPattern(pMemA, &pattern, memSize, 0));
                break;
            case 4:
                pattern.push_back(0xAAAAAAAA);
                pattern.push_back(0x55555555);
                CHECK_RC(Memory::FillPattern(pMemA, &pattern, memSize, 0));
                break;
            case 5:
                CHECK_RC(Memory::FillRandom32(pMemA, GetTestConfiguration()->Seed(), topSize));
                Memory::Fill32(pMemA + topSize, 0, bottomSize / sizeof(UINT32));

                Memory::Fill32(pMemB, 0, topSize / sizeof(UINT32));
                CHECK_RC(Memory::FillRandom32(pMemB + topSize,
                                              GetTestConfiguration()->Seed(),
                                              bottomSize));
                break;
            case 6:
                {
                    const UINT32 lineSize  = 1024;
                    const UINT32 linePitch = lineSize * sizeof(UINT32);
                    const UINT32 topLines  = (topSize + linePitch - 1) / linePitch;
                    const UINT32 bottomLines  = ((memSize + linePitch - 1) / linePitch) - topLines;

                    CHECK_RC(Memory::FillRgbBars(pMemA,
                                                 lineSize,
                                                 topLines,
                                                 ColorUtils::A8R8G8B8,
                                                 linePitch));
                    CHECK_RC(Memory::FillAddress(pMemA + (topLines * linePitch),
                                                 lineSize,
                                                 bottomLines,
                                                 ColorUtils::A8R8G8B8,
                                                 linePitch));

                    CHECK_RC(Memory::FillAddress(pMemB,
                                                 lineSize,
                                                 topLines,
                                                 ColorUtils::A8R8G8B8,
                                                 linePitch));
                    CHECK_RC(Memory::FillRgbBars(pMemB + (topLines * linePitch),
                                                 lineSize,
                                                 bottomLines,
                                                 ColorUtils::A8R8G8B8,
                                                 linePitch));
                    if (memSize % 4096)
                    {
                        pMemA = pMemA + (topLines + bottomLines) * linePitch;
                        pMemB = pMemA + (topLines + bottomLines) * linePitch;;
                        for (UINT32 lwrIdx = (topLines + bottomLines) * linePitch;
                              lwrIdx < memSize; ++lwrIdx, ++pMemA, ++pMemB)
                        {
                            MEM_WR08(pMemA, 0xFF);
                            MEM_WR08(pMemB, 0xFF);
                        }
                    }
                }
                break;
            default:
                Printf(Tee::PriError, "Invalid SrcMemMode = %u\n", m_SrcMemMode);
                return RC::ILWALID_ARGUMENT;
        }
    }

    CHECK_RC(m_LocalMemory[SRC_MEM_A].Set(m_HostMemory[SRC_MEM_A]));
    if (bSrcBInUse)
    {
        CHECK_RC(m_LocalMemory[SRC_MEM_B].Set(m_HostMemory[SRC_MEM_B]));
    }
    CHECK_RC(GetLwdaInstance().Synchronize(GetBoundLwdaDevice()));
    if (m_RemoteLwdaDev.IsValid())
    {
        CHECK_RC(m_RemoteMemory[SRC_MEM_A].Set(m_RemoteStream, m_HostMemory[SRC_MEM_A]));
        if (bSrcBInUse)
        {
            CHECK_RC(m_RemoteMemory[SRC_MEM_B].Set(m_RemoteStream, m_HostMemory[SRC_MEM_B]));
        }
        CHECK_RC(GetLwdaInstance().Synchronize(m_RemoteLwdaDev));
    }

    return rc;
}

// -----------------------------------------------------------------------------
RC MfgTest::LwdaC2CBandwidth::InnerRun(TransferType tt, UINT64 startTimeMs)
{
    RC rc;
    C2CBandwidthParams localParams  = { };
    C2CBandwidthParams remoteParams = { };

    localParams.uint4PerThread  = m_Uint4PerThread;
    localParams.iterations      = m_Iterations;
    localParams.firstSrcMem     = SRC_MEM_A;

    remoteParams.uint4PerThread = m_Uint4PerThread;
    remoteParams.iterations     = m_Iterations;
    remoteParams.firstSrcMem    = SRC_MEM_A;

    Lwca::DeviceMemory *pLocalDstMemory  = nullptr;
    Lwca::DeviceMemory *pRemoteDstMemory = nullptr;

    Lwca::Device boundDev = GetBoundLwdaDevice();

    if (tt == TransferType::WRITES)
    {
        localParams.srcAMemory = m_LocalMemory[SRC_MEM_A].GetDevicePtr();
        if (m_LocalMemory[SRC_MEM_B].IsValid())
            localParams.srcBMemory = m_LocalMemory[SRC_MEM_B].GetDevicePtr();
        else
            localParams.srcBMemory = m_LocalMemory[SRC_MEM_A].GetDevicePtr();

        if (m_bLoopback)
        {
            localParams.dstMemory = m_LocalMemory[DST_MEM].GetLoopbackDevicePtr();
            pLocalDstMemory = &m_LocalMemory[DST_MEM];
        }
        else if (m_RemoteLwdaDev.IsValid())
        {
            localParams.dstMemory = m_RemoteMemory[DST_MEM].GetDevicePtr();
            pLocalDstMemory = &m_RemoteMemory[DST_MEM];

            remoteParams.srcAMemory = m_RemoteMemory[SRC_MEM_A].GetDevicePtr();
            if (m_RemoteMemory[SRC_MEM_B].IsValid())
                remoteParams.srcBMemory = m_RemoteMemory[SRC_MEM_B].GetDevicePtr();
            else
                remoteParams.srcBMemory = m_RemoteMemory[SRC_MEM_A].GetDevicePtr();

            remoteParams.dstMemory  = m_LocalMemory[DST_MEM].GetDevicePtr();
            pRemoteDstMemory = &m_LocalMemory[DST_MEM];
        }
        else
        {
            localParams.dstMemory = m_HostMemory[DST_MEM].GetDevicePtr(boundDev);
        }
    }
    else
    {
        localParams.dstMemory = m_LocalMemory[DST_MEM].GetDevicePtr();
        pLocalDstMemory = &m_LocalMemory[DST_MEM];

        if (m_bLoopback)
        {
            localParams.srcAMemory = m_LocalMemory[SRC_MEM_A].GetLoopbackDevicePtr();
            if (m_LocalMemory[SRC_MEM_B].IsValid())
                localParams.srcBMemory = m_LocalMemory[SRC_MEM_B].GetLoopbackDevicePtr();
            else
                localParams.srcBMemory = m_LocalMemory[SRC_MEM_A].GetLoopbackDevicePtr();
        }
        else if (m_RemoteLwdaDev.IsValid())
        {
            localParams.srcAMemory = m_RemoteMemory[SRC_MEM_A].GetDevicePtr();
            if (m_RemoteMemory[SRC_MEM_B].IsValid())
                localParams.srcBMemory = m_RemoteMemory[SRC_MEM_B].GetDevicePtr();
            else
                localParams.srcBMemory = m_RemoteMemory[SRC_MEM_A].GetDevicePtr();

            remoteParams.srcAMemory = m_LocalMemory[SRC_MEM_A].GetDevicePtr();
            if (m_LocalMemory[SRC_MEM_B].IsValid())
                remoteParams.srcBMemory = m_LocalMemory[SRC_MEM_B].GetDevicePtr();
            else
                remoteParams.srcBMemory = m_LocalMemory[SRC_MEM_A].GetDevicePtr();

            remoteParams.dstMemory  = m_RemoteMemory[DST_MEM].GetDevicePtr();
            pRemoteDstMemory = &m_RemoteMemory[DST_MEM];
        }
        else
        {
            localParams.srcAMemory = m_HostMemory[SRC_MEM_A].GetDevicePtr(boundDev);
            if (m_LocalMemory[SRC_MEM_B].IsValid())
                localParams.srcBMemory = m_HostMemory[SRC_MEM_B].GetDevicePtr(boundDev);
            else
                localParams.srcBMemory = m_HostMemory[SRC_MEM_A].GetDevicePtr(boundDev);
        }
    }
    if (pLocalDstMemory)
    {
        CHECK_RC(pLocalDstMemory->Clear());
    }
    else
    {
        CHECK_RC(m_HostMemory[DST_MEM].Clear());
    }

    if (pRemoteDstMemory)
    {
        CHECK_RC(pRemoteDstMemory->Clear());
    }

    CHECK_RC(GetLwdaInstance().Synchronize(boundDev));
    if (m_RemoteLwdaDev.IsValid())
    {
        CHECK_RC(GetLwdaInstance().Synchronize(m_RemoteLwdaDev));
    }

    TestDevicePtr pTestDevice = GetBoundTestDevice();

    float localElapsedTimeMs = 0.0;
    float remoteElapsedTimeMs = 0.0;
    bool bDone = false;
    UINT32 loop = 0;
    const UINT32 endLoop = GetTestConfiguration()->Loops();
    while (!bDone)
    {
        Lwca::Event localStartEvent(GetLwdaInstance().CreateEvent(boundDev));
        Lwca::Event localStopEvent(GetLwdaInstance().CreateEvent(boundDev));

        if (m_Iterations % 2)
        {
            localParams.firstSrcMem  =
                (localParams.firstSrcMem == SRC_MEM_A) ? SRC_MEM_B : SRC_MEM_A;
            remoteParams.firstSrcMem =
                (localParams.firstSrcMem == SRC_MEM_A) ? SRC_MEM_B : SRC_MEM_A;
        }
        unique_ptr<Lwca::Event> remoteStartEvent;
        unique_ptr<Lwca::Event> remoteStopEvent;
        if (m_RemoteLwdaDev.IsValid())
        {
            remoteStartEvent =
                make_unique<Lwca::Event>(GetLwdaInstance().CreateEvent(m_RemoteLwdaDev));
            remoteStopEvent =
                make_unique<Lwca::Event>(GetLwdaInstance().CreateEvent(m_RemoteLwdaDev));
        }

        CHECK_RC(localStartEvent.Record());
        CHECK_RC(m_LocalBwFunc.Launch(localParams));
        CHECK_RC(localStopEvent.Record());

        if (m_RemoteLwdaDev.IsValid())
        {
            CHECK_RC(remoteStartEvent->Record());
            CHECK_RC(m_RemoteBwFunc.Launch(remoteParams));
            CHECK_RC(remoteStopEvent->Record());
        }
        CHECK_RC(GetLwdaInstance().Synchronize(boundDev));
        localElapsedTimeMs += localStopEvent.TimeMsElapsedSinceF(localStartEvent);
        if (m_RemoteLwdaDev.IsValid())
        {
            CHECK_RC(GetLwdaInstance().Synchronize(m_RemoteLwdaDev));
            remoteElapsedTimeMs += remoteStopEvent->TimeMsElapsedSinceF(*remoteStartEvent);
        }

        loop++;
        bDone = (loop >= endLoop);

        if (m_RuntimeMs && (Xp::GetWallTimeMS() - startTimeMs) >= m_RuntimeMs)
        {
            bDone = true;
        }
    }

    if (!m_SkipSurfaceCheck)
    {
        C2CBandwidthTestMemory srcMem = SRC_MEM_A;
        if (m_LocalMemory[SRC_MEM_B].IsValid())
        {
            srcMem = ((m_Iterations * GetTestConfiguration()->Loops()) % 2) ? SRC_MEM_A :
                                                                              SRC_MEM_B;
        }
        CHECK_RC(VerifyDestinationMemory(pLocalDstMemory, srcMem, DeviceMemLoc::LOCAL));
        if (m_RemoteLwdaDev.IsValid())
        {
            CHECK_RC(VerifyDestinationMemory(pRemoteDstMemory, srcMem, DeviceMemLoc::REMOTE));
        }
    }

    if (!m_SkipBandwidthCheck || m_ShowBandwidthData)
    {
        MASSERT(localElapsedTimeMs != 0.0);
        CHECK_RC(CheckBandwidth(tt, localElapsedTimeMs, DeviceMemLoc::LOCAL));
        if (m_RemoteLwdaDev.IsValid())
        {
            MASSERT(remoteElapsedTimeMs != 0.0);
            CHECK_RC(CheckBandwidth(tt, remoteElapsedTimeMs, DeviceMemLoc::REMOTE));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC MfgTest::LwdaC2CBandwidth::VerifyDestinationMemory
(
    Lwca::DeviceMemory     * pDstMemory,
    C2CBandwidthTestMemory   srcMem,
    DeviceMemLoc             memLoc
)
{
    RC rc;

    if (pDstMemory)
    {
        if (memLoc == DeviceMemLoc::LOCAL)
        {
            CHECK_RC(pDstMemory->Get(&m_HostMemory[DST_MEM]));
            CHECK_RC(GetLwdaInstance().Synchronize(GetBoundLwdaDevice()));
        }
        else
        {
            CHECK_RC(pDstMemory->Get(m_RemoteStream, &m_HostMemory[DST_MEM]));
            CHECK_RC(GetLwdaInstance().Synchronize(m_RemoteLwdaDev));
        }
    }

    const UINT32 *pSrcMem = static_cast<const UINT32 *>(m_HostMemory[srcMem].GetPtr());
    const UINT32 *pDstMem = static_cast<const UINT32 *>(m_HostMemory[DST_MEM].GetPtr());

    {
        Tasker::DetachThread detachThread;
        UINT32 errorCount = 0;
        const UINT32 memSize = m_BlockWidth * m_GridWidth * m_Uint4PerThread * BYTES_PER_UINT4;

        for (UINT32 lwrOffset = 0;
              (lwrOffset < memSize) && (errorCount < m_NumErrorsToPrint);
              lwrOffset += sizeof(UINT32), ++pSrcMem, ++pDstMem)
        {
            if (*pSrcMem != *pDstMem)
            {
                Printf(Tee::PriError,
                       "Mismatch at offset %u on destination surface, got 0x%x,"
                       " expected 0x%x\n",
                       lwrOffset,
                       *pDstMem,
                       *pSrcMem);
                rc = RC::BUFFER_MISMATCH;
                errorCount++;
            }
        }
    }
    return rc;
}

JS_CLASS_INHERIT(LwdaC2CBandwidth, LwdaStreamTest, "LWCA C2C bandwidth test");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, ThreadCount, UINT32,
                     "Number of threads to launch, 0 is max threads (default = 0)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, Uint4PerThread, UINT32,
                     "Number of uint4 (128bit values) to test per thread (default = 128)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, Iterations, UINT32,
                     "Number of iterations to run (default = 16)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, TransferType, UINT08,
                     "Transfer types to perform, 0 = read, 1 = write,"
                     " 2 = reads and writes (default = 2)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, SrcMemMode, UINT32,
                     "Source memory mode (0 = default = random & bars, 1 = all random,"
                     " 2 = all zeroes, 3 = alternate 0x0 & 0xFFFFFFFF,"
                     " 4 = alternate 0xAAAAAAAA & 0x55555555, 5 = random & zeroes,"
                     " 6 = default = RGB bars & address");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, SrcMemModePercent, UINT32,
                     "For modes with two types of data, what percent of the source "
                     "memory is filled with the first type. (default = 50)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, BwThresholdPercent, UINT32,
                     "Percent of max bandwidth that is still considered a pass (default = 97)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, ShowBandwidthData, bool,
                     "Print out detailed bandwidth data (default = false)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, SkipBandwidthCheck, bool,
                     "Skip the bandwidth check (default = false)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, SkipSurfaceCheck, bool,
                     "Skip the surface check (default = false)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, NumErrorsToPrint, UINT32,
                     "Number of errors printed on miscompares"
                     " (default = 10)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, UseFills, bool,
                     "Use memory fills instead of copies to generate bandwidth"
                     " (default = false)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, RuntimeMs, UINT32,
                     "Runtime for the test in milliseconds (default = 0)");
CLASS_PROP_READWRITE(LwdaC2CBandwidth, ShowPerfMonStats, UINT32,
                     "Show the perfmon stats  (default = false)");
