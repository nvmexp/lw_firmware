/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mgrmgr.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "device/interface/c2c.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/tests/iobandwidth.h"

class C2CBandwidth : public IOBandwidth
{
public:
    C2CBandwidth();
    virtual ~C2CBandwidth() { }

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(BwThresholdPercent,    UINT32);
    SETGET_PROP(ByteEnableToggle,      bool);
    SETGET_PROP(PixelComponentMask,    UINT32);
    SETGET_PROP(ShowPerfMonStats,      bool);

private:
    static constexpr UINT32 ALL_COMPONENTS = ~0U;

    UINT64       m_StartTimeMs = 0ULL;

    // JS Variables
    UINT32       m_BwThresholdPercent    = 97;
    bool         m_ByteEnableToggle      = false;
    UINT32       m_PixelComponentMask    = ALL_COMPONENTS;
    bool         m_ShowPerfMonStats      = false;

    UINT64 GetBandwidthThresholdPercent(IOBandwidth::TransferType tt) override
        { return m_BwThresholdPercent; }
    UINT64 GetDataBandwidthKiBps(IOBandwidth::TransferType tt) override;
    string GetDeviceString(TestDevicePtr pTestDevice) override
        { return pTestDevice->GetName(); }
    UINT32 GetPixelMask() const override;
    string GetInterfaceBwSummary() override;
    UINT64 GetRawBandwidthKiBps() override;
    RC GetRemoteTestDevice(TestDevicePtr *pRemoteTestDev) override;
    UINT64 GetStartTimeMs() const override { return m_StartTimeMs; }
    const char * GetTestedInterface() const override { return "C2C Bandwidth"; }
    void SetupDmaWrap(TransferType tt, DmaWrapper * pDmaWrap) const override;
    bool UsePhysAddrCopies() const override { return false; }
};

// -----------------------------------------------------------------------------
C2CBandwidth::C2CBandwidth()
{
    SetName("C2CBandwidth");
}

// -----------------------------------------------------------------------------
bool C2CBandwidth::IsSupported()
{
    if (!IOBandwidth::IsSupported())
        return false;

    if (!GetBoundTestDevice()->SupportsInterface<C2C>())
    {
        Printf(Tee::PriLow, "%s : C2C interface is not supported\n", GetName().c_str());
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
RC C2CBandwidth::Setup()
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

    return IOBandwidth::Setup();
}

// -----------------------------------------------------------------------------
RC C2CBandwidth::Run()
{
    RC rc;

    auto pC2C = GetBoundTestDevice()->GetInterface<C2C>();

    m_StartTimeMs = Xp::GetWallTimeMS();

    bool bDone = false;
    const UINT32 runtimeMs = GetRuntimeMs();
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
            CHECK_RC(InnerRun(TransferType::READS));
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

        if (runtimeMs && (Xp::GetWallTimeMS() - m_StartTimeMs) >= runtimeMs)
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
            CHECK_RC(InnerRun(TransferType::WRITES));
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
        bDone = !runtimeMs || (Xp::GetWallTimeMS() - m_StartTimeMs) >= runtimeMs;
    }
    return rc;
}

// -----------------------------------------------------------------------------
void C2CBandwidth::PrintJsProperties(Tee::Priority pri)
{
    IOBandwidth::PrintJsProperties(pri);

    Printf(pri, "C2CBandwidth Js Properties:\n");
    Printf(pri, "\tBwThresholdPct:                 %u%%\n", m_BwThresholdPercent);
    Printf(pri, "\tByteEnableToggle:               %s\n",   m_ByteEnableToggle ? "true" : "false");        //$
    Printf(pri, "\tPixelComponentMask:             0x%x\n", m_PixelComponentMask);
    Printf(pri, "\tShowPerfMonStats:               %s\n",   m_ShowPerfMonStats ? "true" : "false");   //$
}

// -----------------------------------------------------------------------------
UINT64 C2CBandwidth::GetDataBandwidthKiBps(IOBandwidth::TransferType tt)
{
    const UINT64 rawBwKiBps = GetRawBandwidthKiBps();

    switch (tt)
    {
        case TransferType::READS:
            return static_cast<UINT64>(static_cast<FLOAT64>(rawBwKiBps) * 0.94);
        case TransferType::WRITES:
            return static_cast<UINT64>(static_cast<FLOAT64>(rawBwKiBps) * 0.91);
        case TransferType::READS_AND_WRITES:
            return static_cast<UINT64>(static_cast<FLOAT64>(rawBwKiBps) * 0.88);
        default:
            MASSERT(!"Unknown transfer type");
            break;
    }
    return rawBwKiBps;
}

// -----------------------------------------------------------------------------
string C2CBandwidth::GetInterfaceBwSummary()
{
    auto pC2C = GetBoundGpuSubdevice()->GetInterface<C2C>();
    const UINT32 partitionId = Utility::BitScanForward(pC2C->GetActivePartitionMask());
    return Utility::StrPrintf("C2C Per-Partition Raw Bandwidth = %u KiBps\n"
                              "C2C Total Partitions            = %u\n",
                              pC2C->GetPartitionBandwidthKiBps(partitionId),
                              Utility::CountBits(pC2C->GetActivePartitionMask()));
}

// -----------------------------------------------------------------------------
UINT32 C2CBandwidth::GetPixelMask() const
{
    return m_PixelComponentMask;
}

// -----------------------------------------------------------------------------
UINT64 C2CBandwidth::GetRawBandwidthKiBps()
{
    auto pC2C = GetBoundGpuSubdevice()->GetInterface<C2C>();
    const UINT32 partitionId = Utility::BitScanForward(pC2C->GetActivePartitionMask());
    return static_cast<UINT64>(pC2C->GetPartitionBandwidthKiBps(partitionId)) *
                               Utility::CountBits(pC2C->GetActivePartitionMask());
}

// -----------------------------------------------------------------------------
RC C2CBandwidth::GetRemoteTestDevice(TestDevicePtr *pRemoteTestDev)
{
    RC rc;
    auto pTestDev = GetBoundTestDevice();

    auto pC2C = pTestDev->GetInterface<C2C>();

    const UINT32 activePartitions = pC2C->GetActivePartitionMask();

    if (!activePartitions)
    {
        Printf(Tee::PriError, "%s : C2C interface not initialized on %s\n",
               GetName().c_str(), pTestDev->GetName().c_str());
        return RC::WAS_NOT_INITIALIZED;
    }

    for (UINT32 partitionId = 0; partitionId < pC2C->GetMaxPartitions(); ++partitionId)
    {
        if (!(activePartitions & (1 << partitionId)))
            continue;

        if (pC2C->GetConnectionType(partitionId) == C2C::ConnectionType::GPU)
        {
            // GPUs can only have one remote end
            MASSERT((pRemoteTestDev->get() == nullptr) ||
                    ((*pRemoteTestDev)->GetId() == pC2C->GetRemoteId(partitionId)));

            if (pC2C->GetRemoteId(partitionId) != pTestDev->GetId())
            {
                TestDeviceMgr *pTestDeviceMgr =
                    static_cast<TestDeviceMgr *>(DevMgrMgr::d_TestDeviceMgr);
                CHECK_RC(pTestDeviceMgr->GetDevice(pC2C->GetRemoteId(partitionId),
                                                   pRemoteTestDev));
            }
            else
            {
                *pRemoteTestDev = GetBoundTestDevice();
            }
        }
    }
    return rc;
}

// -----------------------------------------------------------------------------
void C2CBandwidth::SetupDmaWrap(TransferType tt, DmaWrapper * pDmaWrap) const
{
    if (m_ByteEnableToggle)
         pDmaWrap->SetCopyMask(m_PixelComponentMask);
}

// -----------------------------------------------------------------------------
JS_CLASS_INHERIT(C2CBandwidth, IOBandwidth, "C2C Bandwidth Test");

CLASS_PROP_READWRITE(C2CBandwidth, BwThresholdPercent, UINT32,
                     "Percent of max bandwidth that is still considered a pass (default = 97)");
CLASS_PROP_READWRITE(C2CBandwidth, ByteEnableToggle, bool,
                     "Whether to enable byte enables using the PixelComponentMask"
                     " (default = false)");
CLASS_PROP_READWRITE(C2CBandwidth, PixelComponentMask, UINT32,
                     "Mask of pixel components to copy (default = ~0)");
CLASS_PROP_READWRITE(C2CBandwidth, ShowPerfMonStats, UINT32,
                     "Show the perfmon stats  (default = false)");
