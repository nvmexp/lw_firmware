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

#include "class/clc6b5.h" // AMPERE_DMA_COPY_A
#include "core/include/mgrmgr.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "device/interface/pcie.h"
#include "device/interface/c2c.h"
#include "device/interface/lwlink.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/tests/iobandwidth.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"

// -----------------------------------------------------------------------------
// TODO : Using CEs in Physical mode isnt actually the correct method for this test on
// hopper.  The test needs to just use the normal mapping mechanisms and RM will choose
// which mapping type is used depending on the connection between the GPUs with
// C2C > LwLink > BAR1 P2P > PCIE Mailbox
//
// Leave physical mode as an option however so the test could be run on non-hopper if
// desired
//
// https://lwbugs/3331356 needs to be solved first and then this test updated
//
// This test drives P2P traffic between GPUs (or loopback) using copy engines setup to
// transfer data between GPUs via physical address copies that target the BAR1 region
// of the remote GPU rather than through the normal P2P mailbox system.
//
// In addition, in a multi-GPU system bi-directional traffic will be generated
//
// The test verifies bandwidth and data integrity

// -----------------------------------------------------------------------------
class Bar1P2PBandwidth : public IOBandwidth
{
public:
    Bar1P2PBandwidth();
    virtual ~Bar1P2PBandwidth() { }

    SETGET_PROP(ReadBwThresholdPercent,  UINT32);
    SETGET_PROP(WriteBwThresholdPercent, UINT32);
    SETGET_PROP(IgnoreBridgeRequirement, bool);
    SETGET_PROP(UsePhysicalMode,         bool);

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    void PrintJsProperties(Tee::Priority pri) override;
private:
    UINT64 GetBandwidthThresholdPercent(IOBandwidth::TransferType tt) override;
    UINT64 GetDataBandwidthKiBps(IOBandwidth::TransferType tt) override;
    string GetDeviceString(TestDevicePtr pTestDevice) override;
    UINT64 GetRawBandwidthKiBps() override;
    string GetInterfaceBwSummary() override { return ""; }
    UINT32 GetPixelMask() const override { return ~0U; }
    RC GetRemoteTestDevice(TestDevicePtr *pRemoteTestDev) override;
    UINT64 GetStartTimeMs() const override { return m_StartTimeMs; }
    const char * GetTestedInterface() const override { return "BAR1 P2P Bandwidth"; }
    void SetupDmaWrap(TransferType tt, DmaWrapper * pDmaWrap) const override;
    bool UsePhysAddrCopies() const override { return m_UsePhysicalMode; }

    RC CallwlateDataBandwidth
    (
        UINT64               rawBandwidth,
        Pci::PcieLinkSpeed   linkSpeed,
        UINT64             * pReadDataBandwidth,
        UINT64             * pWriteDataBandwidth
    );
    bool ConnectedViaPcie(TestDevicePtr pRemDevice);
    RC GetMinimumBandwidth
    (
        TestDevicePtr        pTestDev,
        PexDevice          * pCommonBridgeDev,
        UINT64             * pMinRawBw,
        UINT64             * pMinReadDataBw,
        UINT64             * pMinWriteDataBw
    );
    RC InitializeBandwidthData(TestDevicePtr pRemoteDev, PexDevice * pCommonBridgeDev);

    UINT64 m_RawBandwidthKiBps       = 0ULL;
    UINT64 m_ReadDataBandwidthKiBps  = 0ULL;
    UINT64 m_WriteDataBandwidthKiBps = 0ULL;
    UINT64 m_StartTimeMs             = 0ULL;

    // JS variables
    UINT32 m_ReadBwThresholdPercent  = 85;
    UINT32 m_WriteBwThresholdPercent = 95;
    bool   m_IgnoreBridgeRequirement = false;
    bool   m_UsePhysicalMode         = false;
};

// -----------------------------------------------------------------------------
Bar1P2PBandwidth::Bar1P2PBandwidth()
{
    SetName("Bar1P2PBandwidth");
}

// -----------------------------------------------------------------------------
bool Bar1P2PBandwidth::IsSupported()
{
    if (!IOBandwidth::IsSupported())
        return false;

    // The test can be run on non-official supported chips as long as physical mode is used
    if (!GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_BAR1_P2P) &&
        !m_UsePhysicalMode)
    {
        Printf(Tee::PriLow, "%s : BAR1 P2P not supported on the current GPU\n", GetName().c_str());
        return false;
    }

    if (IOBandwidth::GetRemoteTestDevice() == nullptr)
    {
        Printf(Tee::PriLow, "%s : No remote test device found\n", GetName().c_str());
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
RC Bar1P2PBandwidth::Setup()
{
    RC rc;

    CHECK_RC(IOBandwidth::Setup());

    if (IOBandwidth::GetRemoteTestDevice() == nullptr)
    {
        Printf(Tee::PriError, "%s : No remote test device found\n", GetName().c_str());
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }
    return RC::OK;
}

// -----------------------------------------------------------------------------
RC Bar1P2PBandwidth::Run()
{
    RC rc;

    m_StartTimeMs = Xp::GetWallTimeMS();
    bool bDone = false;
    const UINT32 runtimeMs = GetRuntimeMs();
    while (!bDone)
    {
        if ((GetTransferType() == static_cast<UINT08>(TransferType::READS)) ||
            (GetTransferType() == static_cast<UINT08>(TransferType::READS_AND_WRITES)))
        {
            CHECK_RC(InnerRun(TransferType::READS));
        }
        if (runtimeMs && (Xp::GetWallTimeMS() - m_StartTimeMs) >= runtimeMs)
        {
            break;
        }
        if ((GetTransferType() == static_cast<UINT08>(TransferType::WRITES)) ||
            (GetTransferType() == static_cast<UINT08>(TransferType::READS_AND_WRITES)))
        {
            CHECK_RC(InnerRun(TransferType::WRITES));
        }
        bDone = !runtimeMs || (Xp::GetWallTimeMS() - m_StartTimeMs) >= runtimeMs;
    }
    return rc;
}

// -----------------------------------------------------------------------------
void Bar1P2PBandwidth::PrintJsProperties(Tee::Priority pri)
{
    IOBandwidth::PrintJsProperties(pri);

    Printf(pri, "Bar1P2PBandwidth Js Properties:\n");
    Printf(pri, "\tReadBwThresholdPct:             %u%%\n", m_ReadBwThresholdPercent);
    Printf(pri, "\tWriteBwThresholdPct:            %u%%\n", m_WriteBwThresholdPercent);
    Printf(pri, "\tIgnoreBridgeRequirement:        %s\n",   m_IgnoreBridgeRequirement ? "true" : "false"); // $
    Printf(pri, "\tUsePhysicalMode:                %s\n",   m_UsePhysicalMode ? "true" : "false");
}

// -----------------------------------------------------------------------------
UINT64 Bar1P2PBandwidth::GetBandwidthThresholdPercent(IOBandwidth::TransferType tt)
{
    return (tt == IOBandwidth::TransferType::READS) ? m_ReadBwThresholdPercent :
                                                      m_WriteBwThresholdPercent;
}

// -----------------------------------------------------------------------------
UINT64 Bar1P2PBandwidth::GetDataBandwidthKiBps(IOBandwidth::TransferType tt)
{
    return (tt == IOBandwidth::TransferType::READS) ? m_ReadDataBandwidthKiBps :
                                                      m_WriteDataBandwidthKiBps;
}

// -----------------------------------------------------------------------------
string Bar1P2PBandwidth::GetDeviceString(TestDevicePtr pTestDevice)
{
    auto pPcie = pTestDevice->GetInterface<Pcie>();
    Pci::PcieLinkSpeed linkSpeed = pPcie->GetLinkSpeed(Pci::SpeedUnknown);
    UINT32 linkWidth = pPcie->GetLinkWidth();
    return pTestDevice->GetName() + Utility::StrPrintf(" %d Gbps x%u", linkSpeed, linkWidth);

}
// -----------------------------------------------------------------------------
UINT64 Bar1P2PBandwidth::GetRawBandwidthKiBps()
{
    return m_RawBandwidthKiBps;
}

// -----------------------------------------------------------------------------
RC Bar1P2PBandwidth::GetRemoteTestDevice(TestDevicePtr *pRemoteTestDev)
{
    RC rc;
    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();

    TestDeviceMgr *pTestDeviceMgr = static_cast<TestDeviceMgr *>(DevMgrMgr::d_TestDeviceMgr);

    PexDevice *pPexDev;
    UINT32     port;
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev, &port));

    if ((pPexDev == nullptr) || pPexDev->IsRoot())
    {
        Printf(m_IgnoreBridgeRequirement ? Tee::PriWarn : Tee::PriError,
               "%s : %s is connected to the rootport\n",
               GetName().c_str(), GetBoundTestDevice()->GetName().c_str());
        if (!m_IgnoreBridgeRequirement)
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    // The requirement for running the BAR1 P2P bandwidth test is that traffic must not flow
    // through the root port as doing so severely limits bandwidth.  This is accomplished by
    //     1. Choosing pairs of GPUs that share a common PEX bridge device
    //     2. Ensuring that if the bridge device supports the ACS extended capability
    //        that it is not configured to forward P2P traffic up to the root port
    //
    // If there is only one GPU then there is no choice other than loopback
    if (pTestDeviceMgr->NumDevicesType(Device::TYPE_LWIDIA_GPU) == 1)
    {
        bool bP2PForwardEnabled = true;
        while (pPexDev && !pPexDev->IsRoot())
        {
            CHECK_RC(pPexDev->GetP2PForwardingEnabled(port, &bP2PForwardEnabled));
            if (!bP2PForwardEnabled)
                break;
            pPexDev = pPexDev->GetUpStreamDev();
        }
        if (bP2PForwardEnabled)
        {
            Printf(m_IgnoreBridgeRequirement ? Tee::PriWarn : Tee::PriError,
                   "%s : P2P forwarding enabled on downstream port above %s\n",
                   GetName().c_str(), GetBoundTestDevice()->GetName().c_str());
            if (!m_IgnoreBridgeRequirement)
                return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        if (!ConnectedViaPcie(GetBoundTestDevice()))
        {
            VerbosePrintf("%s : Loopback device not connected via PCIE\n", GetName().c_str());
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }
        VerbosePrintf("%s : Only one GPU present, running loopback\n", GetName().c_str());
        *pRemoteTestDev = GetBoundTestDevice();
        CHECK_RC(InitializeBandwidthData(GetBoundTestDevice(),
                                         pPexDev->IsRoot() ? nullptr : pPexDev));
        return RC::OK;
    }

    // Save off all bridge devices in order to locate a common bridge device with
    // the remote device
    vector<PexDevice *> nonRootPexDevices;
    while (pPexDev && !pPexDev->IsRoot())
    {
        nonRootPexDevices.push_back(pPexDev);
        pPexDev = pPexDev->GetUpStreamDev();
    }

    TestDevicePtr pLwrRemDev;
    PexDevice    *pCommonPexDev  = nullptr;
    UINT32        commonDevDepth = ~0U;

    // Find another GPU that meets the necessary requirements and prefer the following:
    //    1. A GPU with the lowest common depth (i.e. shortest route between GPUs
    //    2. A GPU that is different from the bound GPU
    for (auto pLwrTestDev : *pTestDeviceMgr)
    {
        if (pLwrTestDev->GetType() != Device::TYPE_LWIDIA_GPU)
            continue;
        auto pRemPcie = pLwrTestDev->GetInterface<Pcie>();

        if (!ConnectedViaPcie(pLwrTestDev))
            continue;

        UINT32     remPort;
        PexDevice *pRemotePexDev;
        CHECK_RC(pRemPcie->GetUpStreamInfo(&pRemotePexDev, &remPort));

        bool bFoundDevice = false;
        size_t matchingDepth = 0;
        while (pRemotePexDev && !pRemotePexDev->IsRoot() && !bFoundDevice)
        {
            for (matchingDepth = 0; matchingDepth < nonRootPexDevices.size(); matchingDepth++)
            {
                bFoundDevice = (pRemotePexDev == nonRootPexDevices[matchingDepth]);
                if (bFoundDevice)
                    break;
            }
            if (bFoundDevice)
            {
                bool bP2PForwardEnabled = false;
                CHECK_RC(pRemotePexDev->GetP2PForwardingEnabled(remPort, &bP2PForwardEnabled));
                if (bP2PForwardEnabled)
                    bFoundDevice = false;
                else
                    break;
            }
            pRemotePexDev = pRemotePexDev->GetUpStreamDev();
        }

        if (bFoundDevice &&
            (!pRemoteTestDev->get() ||
             (*pRemoteTestDev == GetBoundTestDevice()) ||
             (matchingDepth < commonDevDepth)))
        {
            *pRemoteTestDev = pLwrTestDev;
            pCommonPexDev   = pRemotePexDev;
            commonDevDepth  = static_cast<UINT32>(matchingDepth);
        }
    }

    if (pRemoteTestDev->get() != nullptr)
    {
        const PexDevice::PciDevice * pPciDev = pCommonPexDev->GetDownStreamPort(0);
        VerbosePrintf("%s : %s shares common bridge device (%04x:%02x:%02x.*) with %s "
                      "at depth %u\n",
                      GetName().c_str(),
                      GetBoundTestDevice()->GetName().c_str(),
                      pPciDev->Domain, pPciDev->Bus, pPciDev->Dev,
                      (*pRemoteTestDev)->GetName().c_str(),
                      commonDevDepth);
        CHECK_RC(InitializeBandwidthData(*pRemoteTestDev, pCommonPexDev));
        return rc;
    }
    else
    {
        Printf(m_IgnoreBridgeRequirement ? Tee::PriWarn : Tee::PriError,
               "%s : %s no other GPU devices found connected to same bridge device "
               "with P2P forwarding disabled\n",
               GetName().c_str(), GetBoundTestDevice()->GetName().c_str());
        if (!m_IgnoreBridgeRequirement)
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    // No GPU found that meets the requirements and the requirements are being ignored.
    // Choose a random GPU that is not the bound GPU
    for (auto pLwrTestDev : *pTestDeviceMgr)
    {
        if ((pLwrTestDev->GetType() != Device::TYPE_LWIDIA_GPU) ||
            (pLwrTestDev == GetBoundTestDevice()) ||
            !ConnectedViaPcie(pLwrTestDev))
        {
            continue;
        }
        *pRemoteTestDev = pLwrTestDev;
        VerbosePrintf("%s : Testing with remote device %s without common shared bridge\n",
                      GetName().c_str(),
                      (*pRemoteTestDev)->GetName().c_str());
        CHECK_RC(InitializeBandwidthData(pLwrTestDev, nullptr));
        return RC::OK;
    }

    if (pRemoteTestDev->get() == nullptr)
    {
        Printf(Tee::PriError,
               "%s : No remote device connected via PCIE found\n",
               GetName().c_str());
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    return rc;
}

// -----------------------------------------------------------------------------
void Bar1P2PBandwidth::SetupDmaWrap(IOBandwidth::TransferType tt, DmaWrapper * pDmaWrap) const
{
    if (tt == TransferType::READS)
    {
         pDmaWrap->SetUsePhysicalCopy(true, false);
         pDmaWrap->SetPhysicalCopyTarget(Memory::NonCoherent, Memory::Optimal);
    }
    else if (tt == TransferType::WRITES)
    {
         pDmaWrap->SetUsePhysicalCopy(false, true);
         pDmaWrap->SetPhysicalCopyTarget(Memory::Optimal, Memory::NonCoherent);
    }
}

// -----------------------------------------------------------------------------
RC Bar1P2PBandwidth::CallwlateDataBandwidth
(
    UINT64               rawBandwidth,
    Pci::PcieLinkSpeed   linkSpeed,
    UINT64              *pReadDataBandwidth,
    UINT64              *pWriteDataBandwidth
)
{
    switch (linkSpeed)
    {
        case Pci::Speed2500MBPS:
        case Pci::Speed5000MBPS:
            *pReadDataBandwidth = rawBandwidth * 8 / 10;
            *pWriteDataBandwidth = rawBandwidth * 8 / 10;
            break;
        case Pci::Speed8000MBPS:
        case Pci::Speed16000MBPS:
        case Pci::Speed32000MBPS:
            *pReadDataBandwidth = rawBandwidth * 128 / 130;
            *pWriteDataBandwidth = rawBandwidth * 128 / 130;
            break;
        default:
            Printf(Tee::PriError, "Unknown link speed : %d\n", linkSpeed);
            return RC::SOFTWARE_ERROR;
    }

    const UINT32 bytesPerLine = GetTestConfiguration()->SurfaceWidth() *
                                GetTestConfiguration()->DisplayDepth() / 8;
    UINT64 bytesPerPacket = 128ULL;

    // If the data being transfered is less than 64 bytes per line then PEX will
    // generate 64 byte packets, newer GPUs will generate 256 byte packets if
    // possible
    if (GetBoundTestDevice()->HasFeature(Device::GPUSUB_SUPPORTS_256B_PCIE_PACKETS) &&
        (bytesPerLine >= 256))
    {
        bytesPerPacket = 256ULL;
    }
    else if (bytesPerLine <= 64)
    {
        bytesPerPacket = 64ULL;
    }

    const UINT64 writeOverheadBytes = 24ULL;

    // This is slightly different than the number of read overhead bytes in PexBandwidth because
    // traffic in this test is truly bi-directional and needs to account for the 16 byte
    // Memory Read request packet that oclwrs from the remote device with affects the
    // data bandwidth of the local device
    const UINT64 readOverheadBytes  = 36ULL;

    // remove payload header (bytesPerPacket/(bytesPerPacket + overheadBytes)) and
    // 95% flow control overhead:
    *pReadDataBandwidth = *pReadDataBandwidth *
                           bytesPerPacket / (bytesPerPacket + readOverheadBytes) *
                           95 / 100;
    *pWriteDataBandwidth = *pWriteDataBandwidth *
                            bytesPerPacket / (bytesPerPacket + writeOverheadBytes) *
                            95 / 100;
    return RC::OK;
}

// -----------------------------------------------------------------------------
bool Bar1P2PBandwidth::ConnectedViaPcie(TestDevicePtr pRemDevice)
{
    // When using physical mode it bypasses the normal method and forces PCIE
    if (m_UsePhysicalMode)
        return true;

    auto pTestDevice = GetBoundTestDevice();

    if (pTestDevice->SupportsInterface<C2C>())
    {
        auto pC2C = pTestDevice->GetInterface<C2C>();
        for (UINT32 c2cPartition = 0; c2cPartition < pC2C->GetMaxPartitions(); ++c2cPartition)
        {
            if ((pC2C->GetConnectionType(c2cPartition) == C2C::ConnectionType::GPU) &&
                (pC2C->GetRemoteId(c2cPartition) == pRemDevice->GetId()))
            {
                return false;
            }
        }
    }
    if (pTestDevice->SupportsInterface<LwLink>())
    {
        vector<LwLinkRoutePtr> routes;
        LwLinkConnection::EndpointDevices epDevs = { pTestDevice, pRemDevice };
        if (RC::OK == LwLinkDevIf::TopologyManager::GetRoutesOnDevice(pTestDevice, &routes))
        {
            for (auto const & pLwrRoute : routes)
            {
                if (pLwrRoute->UsesEndpoints(epDevs))
                    return false;
            }
        }
    }
    return true;
}

// -----------------------------------------------------------------------------
// Find the link with the minimum bandwidth between the test device and the common
// bridge device that is shared with the remote device
RC Bar1P2PBandwidth::GetMinimumBandwidth
(
    TestDevicePtr        pTestDev,
    PexDevice          * pCommonBridgeDev,
    UINT64             * pMinRawBandwidth,
    UINT64             * pMinReadDataBandwidth,
    UINT64             * pMinWriteDataBandwidth
)
{
    MASSERT(pMinRawBandwidth);
    MASSERT(pMinReadDataBandwidth);
    MASSERT(pMinWriteDataBandwidth);

    RC rc;

    auto pPcie = pTestDev->GetInterface<Pcie>();
    Pci::PcieLinkSpeed linkSpeed = pPcie->GetLinkSpeed(Pci::SpeedUnknown);
    UINT32 linkWidth = pPcie->GetLinkWidth();
    UINT64 lwrRawBwKiBps = (static_cast<UINT64>(linkSpeed) * 1000ULL * 1000ULL * linkWidth) /
                           8ULL / 1024ULL;
    UINT64 lwrReadDataBwKiBps;
    UINT64 lwrWriteDataBwKiBps;
    CHECK_RC(CallwlateDataBandwidth(lwrRawBwKiBps,
                                    linkSpeed,
                                    &lwrReadDataBwKiBps,
                                    &lwrWriteDataBwKiBps));

    *pMinRawBandwidth       = lwrRawBwKiBps;
    *pMinReadDataBandwidth  = lwrReadDataBwKiBps;
    *pMinWriteDataBandwidth = lwrWriteDataBwKiBps;

    auto PrintDepthInfo = [this] (const string & testName,
                              TestDevicePtr pTestDevice,
                              UINT32 depth,
                              UINT32 linkWidth,
                              Pci::PcieLinkSpeed linkSpeed,
                              UINT64 rawBandwidth,
                              UINT64 readDataBandwidth,
                              UINT64 writeDataBandwidth)
    {
        VerbosePrintf("%s : At depth %u from device %s:\n"
                      "    Link Width           : %u\n"
                      "    Link Speed           : %d Gbps\n"
                      "    Raw Bandwidth        : %llu KiBps\n"
                      "    Read Data Bandwidth  : %llu KiBps\n"
                      "    Write Data Bandwidth : %llu KiBps\n",
                      testName.c_str(),
                      depth,
                      pTestDevice->GetName().c_str(),
                      linkWidth,
                      linkSpeed,
                      rawBandwidth,
                      readDataBandwidth,
                      writeDataBandwidth);
    };

    UINT32 depth = 0;
    PrintDepthInfo(GetName(),
                   pTestDev,
                   depth,
                   linkWidth,
                   linkSpeed,
                   lwrRawBwKiBps,
                   lwrReadDataBwKiBps,
                   lwrWriteDataBwKiBps);

    PexDevice *pPexDev;
    UINT32 port;
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev, &port));
    while (pPexDev && !pPexDev->IsRoot() && (pCommonBridgeDev != pPexDev))
    {
        ++depth;
        UINT32 locWidth, hostWidth;
        CHECK_RC(pPexDev->GetUpStreamLinkWidths(&locWidth, &hostWidth));
        linkWidth = min(locWidth, hostWidth);
        linkSpeed = pPexDev->GetUpStreamSpeed();
        lwrRawBwKiBps =
            (static_cast<UINT64>(linkSpeed) * 1000ULL * 1000ULL * linkWidth) / 8ULL / 1024ULL;
        CHECK_RC(CallwlateDataBandwidth(lwrRawBwKiBps,
                                        linkSpeed,
                                        &lwrReadDataBwKiBps,
                                        &lwrWriteDataBwKiBps));
        PrintDepthInfo(GetName(),
                       pTestDev,
                       depth,
                       linkWidth,
                       linkSpeed,
                       lwrRawBwKiBps,
                       lwrReadDataBwKiBps,
                       lwrWriteDataBwKiBps);

        if (lwrReadDataBwKiBps < *pMinReadDataBandwidth)
        {
            *pMinRawBandwidth       = lwrRawBwKiBps;
            *pMinReadDataBandwidth  = lwrReadDataBwKiBps;
            *pMinWriteDataBandwidth = lwrWriteDataBwKiBps;
        }
        pPexDev = pPexDev->GetUpStreamDev();
    }

    return RC::OK;
}

// -----------------------------------------------------------------------------
RC Bar1P2PBandwidth::InitializeBandwidthData(TestDevicePtr pRemoteDev, PexDevice * pCommonBridgeDev)
{
    RC rc;

    UINT64 minRawBwKiBps  = 0;
    UINT64 minReadDataBwKiBps = 0;
    UINT64 minWriteDataBwKiBps = 0;
    CHECK_RC(GetMinimumBandwidth(GetBoundTestDevice(),
                                 pCommonBridgeDev,
                                 &minRawBwKiBps,
                                 &minReadDataBwKiBps,
                                 &minWriteDataBwKiBps));

    m_RawBandwidthKiBps       = minRawBwKiBps;
    m_ReadDataBandwidthKiBps  = minReadDataBwKiBps;
    m_WriteDataBandwidthKiBps = minWriteDataBwKiBps;

    if (pRemoteDev == GetBoundTestDevice())
        return RC::OK;

    UINT64 minRemoteRawBwKiBps  = 0;
    UINT64 minRemoteReadDataBwKiBps = 0;
    UINT64 minRemoteWriteDataBwKiBps = 0;
    CHECK_RC(GetMinimumBandwidth(pRemoteDev,
                                 pCommonBridgeDev,
                                 &minRemoteRawBwKiBps,
                                 &minRemoteReadDataBwKiBps,
                                 &minRemoteWriteDataBwKiBps));

    if (minRemoteReadDataBwKiBps < m_ReadDataBandwidthKiBps)
    {
        m_RawBandwidthKiBps       = minRemoteRawBwKiBps;
        m_ReadDataBandwidthKiBps  = minRemoteReadDataBwKiBps;
        m_WriteDataBandwidthKiBps = minRemoteReadDataBwKiBps;
    }

    return rc;
}

// -----------------------------------------------------------------------------
JS_CLASS_INHERIT(Bar1P2PBandwidth, IOBandwidth, "BAR1 P2P Bandwidth Test");

CLASS_PROP_READWRITE(Bar1P2PBandwidth, ReadBwThresholdPercent, UINT32,
    "Percent of max bandwidth that is still considered a pass for reads (default = 85)");
CLASS_PROP_READWRITE(Bar1P2PBandwidth, WriteBwThresholdPercent, UINT32,
    "Percent of max bandwidth that is still considered a pass for writes (default = 95)");
CLASS_PROP_READWRITE(Bar1P2PBandwidth, IgnoreBridgeRequirement, bool,
                     "Ignore the bridge requirements for BAR1 transfers (default = false)");
CLASS_PROP_READWRITE(Bar1P2PBandwidth, UsePhysicalMode, bool,
                     "Use physical mode copies (default = false)");
