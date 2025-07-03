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
#include "hopperc2c.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "ctrl/ctrl2080.h"
#include "device/c2c/gh100c2cdiagdriver.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/reghal/reghal.h"

namespace
{
    constexpr UINT32 C2C_LANES_PER_LINK = 9;

    #define LW_PC2C_PHASE_PASSING_PASSING_WINDOW  16:10
    #define LW_PC2C_PHASE_PASSING_NUM_WINDOWS     29:17
}

RC HopperC2C::DoConfigurePerfMon(UINT32 pmBit, PerfMonDataType pmDataType)
{
    if (pmBit >= GetPerfMonBitCount())
    {
        Printf(Tee::PriError, "Cannont configure C2C perfMon, invalid PM bit count : %u\n", pmBit);
        return RC::SOFTWARE_ERROR;
    }
    if (!GetActivePartitionMask())
    {
        Printf(Tee::PriError, "Cannont configure C2C perfMon, C2C not active\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_bLink0Only)
    {
        Printf(Tee::PriError, "C2C perfMon not supported in link 0 only mode\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 pmBitValue = 0;
    RegHal & regs = GetDevice()->Regs();
    switch (pmDataType)
    {
        case PerfMonDataType::RxDataAllVCs:
            pmBitValue = regs.LookupValue(MODS_PC2C_C2CS0_LINK_TL_PM_SEL_BIT188);
            break;
        case PerfMonDataType::TxDataAllVCs:
            pmBitValue = regs.LookupValue(MODS_PC2C_C2CS0_LINK_TL_PM_SEL_BIT183);
            break;
        default:
            Printf(Tee::PriError, "Cannont configure C2C perfMon, unknown PM data type : %u\n",
                   static_cast<UINT32>(pmDataType));
            return RC::SOFTWARE_ERROR;
    }

    auto WriteC2CBitSel = [&] (ModsGpuRegAddress c2cBasePmAddr) -> void
    {
        const UINT32 c2cPmRegAddr =
            regs.LookupAddress(c2cBasePmAddr) + (pmBit / 4) * sizeof(UINT32);
        UINT32 pmBitReg = GetDevice()->RegRd32(c2cPmRegAddr);
        switch (pmBit %4)
        {
            case 0:
                regs.SetField(&pmBitReg, MODS_PC2C_C2CS0_LINK_TL_PM0_BIT0_SEL, pmBitValue);
                break;
            case 1:
                regs.SetField(&pmBitReg, MODS_PC2C_C2CS0_LINK_TL_PM0_BIT1_SEL, pmBitValue);
                break;
            case 2:
                regs.SetField(&pmBitReg, MODS_PC2C_C2CS0_LINK_TL_PM0_BIT2_SEL, pmBitValue);
                break;
            case 3:
                regs.SetField(&pmBitReg, MODS_PC2C_C2CS0_LINK_TL_PM0_BIT3_SEL, pmBitValue);
                break;
        }
        GetDevice()->RegWr32(c2cPmRegAddr, pmBitReg);
    };

    if (IsPartitionActive(0))
        WriteC2CBitSel(MODS_PC2C_C2CS0_LINK_TL_PM0);
    if (IsPartitionActive(1))
        WriteC2CBitSel(MODS_PC2C_C2CS1_LINK_TL_PM0);

    m_PmConfigs[pmBit] = pmDataType;

    return RC::OK;
}

RC HopperC2C::DoDumpRegs() const
{
    return RC::OK;
}

C2C::ConnectionType HopperC2C::DoGetConnectionType(UINT32 partitionId) const
{
    MASSERT(m_bInitialized);
    return m_ConnectionType;
}

UINT32 HopperC2C::DoGetActivePartitionMask() const
{
    MASSERT(m_bInitialized);
    return m_ActivePartitionMask;
}

RC HopperC2C::DoGetErrorCounts(map<UINT32, ErrorCounts> * pErrorCounts) const
{
    MASSERT(m_bInitialized);
    RC rc;

    if (!GetActivePartitionMask())
        return RC::OK;

    LW2080_CTRL_BUS_GET_C2C_ERR_INFO_PARAMS errParams = { };
    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->ControlBySubdevice(GetGpuSubdevice(),
                                       LW2080_CTRL_CMD_BUS_GET_C2C_ERR_INFO,
                                       &errParams,
                                       sizeof(errParams)));
    const UINT32 maxLinks = GetMaxLinks();
    for (UINT32 linkId = 0; linkId < maxLinks; ++linkId)
    {
        C2cDiagDriver::LinkStatus linkStatus = C2cDiagDriver::LINK_NOT_TRAINED;
        CHECK_RC(m_C2CDiagDriver->GetLinkStatus(linkId, &linkStatus));

        if (linkStatus != C2cDiagDriver::LINK_TRAINED)
            continue;

        ErrorCounts lwrCounts;
        CHECK_RC(lwrCounts.SetCount(ErrorCounts::RX_CRC_ID, errParams.errCnts[linkId].nrCrcErrIntr));
        CHECK_RC(lwrCounts.SetCount(ErrorCounts::TX_REPLAY_ID, errParams.errCnts[linkId].nrReplayErrIntr));
        CHECK_RC(lwrCounts.SetCount(ErrorCounts::TX_REPLAY_B2B_FID_ID, errParams.errCnts[linkId].nrReplayB2bErrIntr));
        pErrorCounts->emplace(linkId, lwrCounts);
    }

    return rc;
}

RC HopperC2C::DoGetFomData(UINT32 linkId, vector<FomData> *pFomData)
{
    MASSERT(pFomData);

    RegHal & regs = GetDevice()->Regs();
    if (!regs.HasRWAccess(MODS_PC2C_C2C0_PL_RX_DEBUG_CTRL))
    {
        // This would print 100% of the time on production parts so do not print at
        // PriNormal or PriError
        Printf(Tee::PriLow, "Unable to get C2C FOM values on link %u due to priv protection\n", linkId);
        return RC::PRIV_LEVEL_VIOLATION;
    }

    C2cDiagDriver::LinkStatus driverStatus = C2cDiagDriver::LINK_NOT_TRAINED;
    RC rc;
    CHECK_RC(m_C2CDiagDriver->GetLinkStatus(linkId, &driverStatus));
    if (driverStatus == C2cDiagDriver::LINK_NOT_TRAINED)
        return RC::HW_STATUS_ERROR;

    const UINT32 c2cLinkStride = regs.LookupAddress(MODS_PC2C_C2C1_LINK_TL_PM_DEBUG0) -
                                 regs.LookupAddress(MODS_PC2C_C2C0_LINK_TL_PM_DEBUG0);
    const UINT32 rxDebugSel    = regs.LookupAddress(MODS_PC2C_C2C0_PL_RX_DEBUG_CTRL) +
                                 linkId * c2cLinkStride;
    const UINT32 rxDebug0      = regs.LookupAddress(MODS_PC2C_C2C0_PL_RX_DEBUG0) +
                                 linkId * c2cLinkStride;
    const UINT32 rxDebug1      = regs.LookupAddress(MODS_PC2C_C2C0_PL_RX_DEBUG1) +
                                 linkId * c2cLinkStride;
    FomData fomData;

    pFomData->clear();

    // Each debug data request covers 2 lanes of FOM data
    for (UINT32 lwrLane = 0; lwrLane < C2C_LANES_PER_LINK; lwrLane += 2)
    {
        UINT32 selValue = 0;
        switch (lwrLane)
        {
            case 0:
                selValue = regs.LookupValue(MODS_PC2C_C2C0_PL_RX_DEBUG_CTRL_SEL_PHASE_PASSING0);
                break;
            case 2:
                selValue = regs.LookupValue(MODS_PC2C_C2C0_PL_RX_DEBUG_CTRL_SEL_PHASE_PASSING1);
                break;
            case 4:
                selValue = regs.LookupValue(MODS_PC2C_C2C0_PL_RX_DEBUG_CTRL_SEL_PHASE_PASSING2);
                break;
            case 6:
                selValue = regs.LookupValue(MODS_PC2C_C2C0_PL_RX_DEBUG_CTRL_SEL_PHASE_PASSING3);
                break;
            case 8:
                selValue = regs.LookupValue(MODS_PC2C_C2C0_PL_RX_DEBUG_CTRL_SEL_PHASE_PASSING4);
                break;
            default:
                Printf(Tee::PriError, "Invalid C2C lane\n");
                return RC::SOFTWARE_ERROR;
        }

        GetDevice()->RegWr32(rxDebugSel, selValue);
        const UINT32 debug0 = GetDevice()->RegRd32(rxDebug0);

        fomData.passingWindow = DRF_VAL(_PC2C, _PHASE_PASSING, _PASSING_WINDOW, debug0);
        fomData.numWindows = DRF_VAL(_PC2C, _PHASE_PASSING, _NUM_WINDOWS, debug0);
        pFomData->push_back(fomData);

        // There are an odd number of lanes, do no store the unused second lane info on the
        // last loop
        if (lwrLane != (C2C_LANES_PER_LINK - 1))
        {
            const UINT32 debug1 = GetDevice()->RegRd32(rxDebug1);
            fomData.passingWindow = DRF_VAL(_PC2C, _PHASE_PASSING, _PASSING_WINDOW, debug1);
            fomData.numWindows = DRF_VAL(_PC2C, _PHASE_PASSING, _NUM_WINDOWS, debug1);
            pFomData->push_back(fomData);
        }
    }

    return RC::OK;
}

RC HopperC2C::DoGetLineRateMbps(UINT32 partitionId, UINT32 *pLineRateMbps) const
{
    MASSERT(m_bInitialized);
    MASSERT(pLineRateMbps);

    *pLineRateMbps = UNKNOWN_LINE_RATE;
    FLOAT32 fLineRateMbps = 0.0;
    RC rc;
    CHECK_RC(m_C2CDiagDriver->GetLineRate(partitionId, &fLineRateMbps));
    *pLineRateMbps = static_cast<UINT32>(fLineRateMbps * 1000.0);
    return rc;
}

UINT32 HopperC2C::DoGetLinksPerPartition() const
{
    return C2C_DIAG_DRIVER_GH100_MAX_LINKS_PER_PARTITION;
}

RC HopperC2C::DoGetLinkStatus(UINT32 linkId, LinkStatus *pStatus) const
{
    MASSERT(m_bInitialized);
    MASSERT(pStatus);

    C2cDiagDriver::LinkStatus driverStatus = C2cDiagDriver::LINK_NOT_TRAINED;

    RC rc;
    CHECK_RC(m_C2CDiagDriver->GetLinkStatus(linkId, &driverStatus));

    switch (driverStatus)
    {
        case C2cDiagDriver::LINK_NOT_TRAINED:
            *pStatus = LinkStatus::Off;
            break;
        case C2cDiagDriver::LINK_TRAINED:
            *pStatus = LinkStatus::Active;
            break;
        case C2cDiagDriver::LINK_TRAINING_FAILED:
            *pStatus = LinkStatus::Fail;
            break;
        default:
            MASSERT(!"Unknown C2C link state");
            return RC::HW_STATUS_ERROR;

    }
    return rc;
}

UINT32 HopperC2C::DoGetMaxFomValue() const
{
    return 32;
}

UINT32 HopperC2C::DoGetMaxFomWindows() const
{
    return 32;
}

UINT32 HopperC2C::DoGetMaxPartitions() const
{

    return C2C_DIAG_DRIVER_GH100_MAX_C2C_PARTITIONS;
}

UINT32 HopperC2C::DoGetPartitionBandwidthKiBps(UINT32 partitionId) const
{
    MASSERT(m_bInitialized);
    return m_PartitionBwKiBps;
}

UINT32 HopperC2C::DoGetPerfMonBitCount() const
{
    return 16;
}

string HopperC2C::DoGetRemoteDeviceString(UINT32 partitionId) const
{
    MASSERT(m_bInitialized);

    if (!IsPartitionActive(partitionId))
        return "Not Connected";

    if (GetConnectionType(partitionId) == ConnectionType::GPU)
    {
        TestDevicePtr pRemoteDevice;
        if (DevMgrMgr::d_TestDeviceMgr->GetDevice(m_RemoteId, &pRemoteDevice) != RC::OK)
        {
            MASSERT(!"Unable to locate remote C2C device");
            return "Unknown GPU Device";
        }
        return pRemoteDevice->GetName();
    }
    else if (GetConnectionType(partitionId) == ConnectionType::CPU)
    {
        // TODO : Need to uniquely identify the TH500 connection
        return "Sysmem";
    }

    MASSERT(!"Unknown C2C connection type");
    return "Unknown Device";
}

Device::Id HopperC2C::DoGetRemoteId(UINT32 partitionId) const
{
    MASSERT(m_bInitialized);
    return m_RemoteId;
}

RC HopperC2C::DoInitialize()
{
    // TODO - RM has been requested to add a bSupported flag to the C2C Info control call
    // in bug 3206861 
    m_bSupported = !Platform::IsVirtFunMode() && (GetGpuSubdevice()->GetFsImpl()->C2CMask() != 0);

    UINT32 rmC2CLinks;
    if ((Registry::Read("ResourceManager", "RmOverrideC2CLinks", &rmC2CLinks) == RC::OK) &&
        (rmC2CLinks == 0))
    {
        m_bSupported = false;
    }

    if (!m_bSupported)
    {
        m_bInitialized = true;
        return RC::OK;
    }

    GpuSubdevice *pSubdev = GetDevice()->GetInterface<GpuSubdevice>();
    UINT08 * pC2CBase = static_cast<UINT08 *>(pSubdev->GetLwBase()) + DRF_BASE(LW_PC2C);

    RC rc;
    m_C2CDiagDriver = make_unique<Gh100C2cDiagDriver>();
    CHECK_RC(m_C2CDiagDriver->Init(pC2CBase,
                                   GetMaxPartitions(),
                                   GetLinksPerPartition()));

    LW2080_CTRL_CMD_BUS_GET_C2C_INFO_PARAMS c2cParams = { };
    LwRmPtr pLwRm;
    CHECK_RC(pLwRm->ControlBySubdevice(GetGpuSubdevice(),
                                       LW2080_CTRL_CMD_BUS_GET_C2C_INFO,
                                       &c2cParams,
                                       sizeof(c2cParams)));

    if (c2cParams.bIsLinkUp)
    {
        if ((c2cParams.nrLinks % GetLinksPerPartition()) != 0)
        {
            if (c2cParams.linkMask != 0x1)
            {
                Printf(Tee::PriError,
                       "C2C : When not all links in a partition are present, only link 0 is "
                       "supported, link mask 0x%x\n",
                       c2cParams.linkMask);
                return RC::UNSUPPORTED_SYSTEM_CONFIG;
            }
            m_bLink0Only = true;
            m_C2CDiagDriver->SetLink0Only(true);
        }
        if (c2cParams.nrLinks == GetMaxLinks())
        {
            m_ActivePartitionMask = (1 << GetMaxPartitions()) - 1;
        }
        else
        {
            for (UINT32 partitionId = 0; partitionId < GetMaxPartitions(); partitionId++)
            {
                const UINT32 partitionLinkMask =
                    ((1U << GetLinksPerPartition()) - 1) << (partitionId * GetLinksPerPartition());
                if (c2cParams.linkMask & partitionLinkMask)
                    m_ActivePartitionMask |= (1U << partitionId);
            }
        }

        UINT64 linkBwKBps = 0;
        // Bug 3479179 : RM returning fixed line rate for 
        if (GetDevice()->HasBug(3479179))
        {
            const UINT32 firstPartition = Utility::BitScanForward(m_ActivePartitionMask);
            FLOAT32 fLineRateMbps = 0.0;
            CHECK_RC(m_C2CDiagDriver->GetLineRate(firstPartition, &fLineRateMbps));
            const UINT32 lineRateMbps = static_cast<UINT32>(fLineRateMbps * 1000.0);
            linkBwKBps = static_cast<UINT64>(1000ULL * lineRateMbps * C2C_LANES_PER_LINK / 8);
        }
        else
        {
            linkBwKBps = static_cast<UINT64>(c2cParams.perLinkBwMBps) * 1000ULL;
        }

        // Colwert to kiBps
        m_PartitionBwKiBps = static_cast<UINT32>(linkBwKBps * 1000ULL / 1024ULL) *
                             GetLinksPerPartition();

        switch (c2cParams.remoteType)
        {
            case LW2080_CTRL_BUS_GET_C2C_INFO_REMOTE_TYPE_CPU:
                m_ConnectionType = ConnectionType::CPU;
                break;
            case LW2080_CTRL_BUS_GET_C2C_INFO_REMOTE_TYPE_GPU:
                m_ConnectionType = ConnectionType::GPU;
                break;
            default:
                Printf(Tee::PriError, "Unknown C2C remote type %d\n", c2cParams.remoteType);
                return RC::SOFTWARE_ERROR;
        }
    }

    // TODO : Need to discuss this with the RM team on whether they can return remote GPU
    // information when the type is GPU
    //
    // For now assume that if there are 2 GPUs in the system the connection is between them
    // and if there are more than 2 throw an error
    //
    // From the GPU side of the connection there are only 3 possible POR options:
    //    1. Loopback
    //    2. Connected to a single remote GPU
    //    3. Connected to a TH500
    if (m_ConnectionType == ConnectionType::GPU)
    {
        UINT32 numGpus = DevMgrMgr::d_TestDeviceMgr->NumDevicesType(TestDevice::TYPE_LWIDIA_GPU);
        if (numGpus == 1)
        {
            m_RemoteId = GetDevice()->GetId();
        }
        else if (numGpus == 2)
        {
            Device::Id id = GetDevice()->GetId();
            for (auto pTestDevice : *DevMgrMgr::d_TestDeviceMgr)
            {
                if (pTestDevice->GetId() != id)
                {
                    m_RemoteId = pTestDevice->GetId();
                    break;
                }
            }
        }
        else
        {
            Printf(Tee::PriError, "Unable to determine to which remote GPU C2C is connected");
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }
    }

    m_bInitialized = true;
    SetRegLogLinkMask((1U << (GetMaxPartitions() * GetLinksPerPartition())) - 1);

    return rc;
}

//-----------------------------------------------------------------------------
bool HopperC2C::DoIsPartitionActive(UINT32 partitionId) const
{
    MASSERT(m_bInitialized);
    MASSERT(partitionId < GetMaxPartitions());

    if (partitionId >= GetMaxPartitions())
    {
        Printf(Tee::PriError,
               "Invalid C2C partition %u when checking active status\n",
               partitionId);
        return false;
    }

    return m_ActivePartitionMask & (1 << partitionId);
}

//-----------------------------------------------------------------------------
void HopperC2C::DoPrintTopology(Tee::Priority pri)
{
    string topoString = "\n";
    for (UINT32 partitionId = 0; partitionId < GetMaxPartitions(); ++partitionId)
    {
        topoString += Utility::StrPrintf("C2C Topology for %s partition %u\n",
                                         GetDevice()->GetName().c_str(), partitionId);
        topoString += "--------------------------------------------\n";
        if (IsPartitionActive(partitionId))
        {
            topoString += "  Partition Status    : Active\n";
            topoString += Utility::StrPrintf("  Remote Device       : %s\n",
                                             GetRemoteDeviceString(partitionId).c_str());
            UINT32 lineRateBps = 0;
            if (GetLineRateMbps(partitionId, &lineRateBps) == RC::OK)
            {
                topoString += Utility::StrPrintf("  Line Rate           : %u Mbps\n",lineRateBps);
            }
            else
            {
                MASSERT(!"Failed to get C2C line rate");
                topoString += "  Line Rate           : Unknown\n";
            }
            topoString += Utility::StrPrintf("  Partition Data Rate : %u KiBps\n",
                                             GetPartitionBandwidthKiBps(partitionId));
        }
        else
        {
            topoString += "  Partition Status    : Not Active\n";
            topoString += Utility::StrPrintf("  Remote Device       : %s\n",
                                             GetRemoteDeviceString(partitionId).c_str());
            topoString += "  Line Rate           : 0 Mbps\n";
            topoString += "  Partition Data Rate : 0 KiBps\n";
        }

        for (UINT32 linkIndex = 0; linkIndex < GetLinksPerPartition(); linkIndex++)
        {
            const UINT32 linkId = (partitionId * GetLinksPerPartition()) + linkIndex;

            LinkStatus linkStatus = LinkStatus::Unknown;
            
            topoString += Utility::StrPrintf("  Link %u\n", linkId);
            if (GetLinkStatus(linkId, &linkStatus) == RC::OK)
            {
                topoString += Utility::StrPrintf("    Link Status : %s\n",
                                                 LinkStatusString(linkStatus));
            }
            else
            {
                topoString += Utility::StrPrintf("    Link Status : Unknown\n");
            }
            vector<FomData> fomData;
            RC rc = GetFomData(linkId, &fomData);
            if (rc == RC::OK)
            {
                topoString += Utility::StrPrintf("    FOM Values  : ");
                string comma;
                for (auto const & lwrFom : fomData)
                {
                    topoString += Utility::StrPrintf("%s%u", comma.c_str(), lwrFom.passingWindow);
                    if (comma.empty())
                        comma = ", ";
                }
                topoString += "\n";
                comma.clear();
                topoString += Utility::StrPrintf("    FOM Windows : ");
                for (auto const & lwrFom : fomData)
                {
                    topoString += Utility::StrPrintf("%s%u", comma.c_str(), lwrFom.numWindows);
                    if (comma.empty())
                        comma = ", ";
                }
                topoString += "\n";
            }
            else if (rc = RC::PRIV_LEVEL_VIOLATION)
            {
                topoString += Utility::StrPrintf("    FOM Values  : Unable to read\n");
                topoString += Utility::StrPrintf("    FOM Windows : Unable to read\n");
            }
            else
            {
                topoString += Utility::StrPrintf("    FOM Values  : Unknown\n");
                topoString += Utility::StrPrintf("    FOM Windows : Unknown\n");
            }
        }
        topoString += "\n";
    }
    Printf(pri, "%s\n", topoString.c_str());
}

RC HopperC2C::DoStartPerfMon()
{
    if (m_PmConfigs.empty())
    {
        Printf(Tee::PriError, "No C2C perf monitors are configured\n");
        return RC::SOFTWARE_ERROR;
    }

    RegHal & regs = GetDevice()->Regs();

    if (IsPartitionActive(0))
    {
        regs.Write32(MODS_PC2C_C2CS0_LINK_TL_PM_ENABLE, 0);
        regs.Write32(MODS_PC2C_C2CS0_LINK_PM_DIV_4_OR_DIV_8, 0);
    }
    if (IsPartitionActive(1))
    {
        regs.Write32(MODS_PC2C_C2CS1_LINK_TL_PM_ENABLE, 0);
        regs.Write32(MODS_PC2C_C2CS1_LINK_PM_DIV_4_OR_DIV_8, 0);
    }

    for (auto const & lwrConfig : m_PmConfigs)
    {
        UINT32 c2cPmRegAddr;
        if (IsPartitionActive(0))
        {
            c2cPmRegAddr = regs.LookupAddress(MODS_PC2C_C2CS0_LINK_TL_PM_DEBUG0) +
                lwrConfig.first * sizeof(UINT32);
            GetDevice()->RegWr32(c2cPmRegAddr, 0);
        }
        if (IsPartitionActive(1))
        {
            c2cPmRegAddr =
                regs.LookupAddress(MODS_PC2C_C2CS1_LINK_TL_PM_DEBUG0) +
                lwrConfig.first * sizeof(UINT32);
            GetDevice()->RegWr32(c2cPmRegAddr, 0);
        }
    }

    if (IsPartitionActive(0))
        regs.Write32(MODS_PC2C_C2CS0_LINK_TL_PM_ENABLE, 1);
    if (IsPartitionActive(1))
        regs.Write32(MODS_PC2C_C2CS1_LINK_TL_PM_ENABLE, 1);

    return RC::OK;
}

RC HopperC2C::DoStopPerfMon(map<UINT32, vector<PerfMonCount>> * pPmCounts)
{
    if (m_PmConfigs.empty())
    {
        Printf(Tee::PriError, "No C2C perf monitors are configured\n");
        return RC::SOFTWARE_ERROR;
    }

    RegHal & regs = GetDevice()->Regs();
    if (IsPartitionActive(0))
        regs.Write32(MODS_PC2C_C2CS0_LINK_TL_PM_ENABLE, 0);
    if (IsPartitionActive(1))
        regs.Write32(MODS_PC2C_C2CS1_LINK_TL_PM_ENABLE, 0);

    const UINT32 counterRegStride = regs.LookupAddress(MODS_PC2C_C2C1_LINK_TL_PM_DEBUG0) -
                                    regs.LookupAddress(MODS_PC2C_C2C0_LINK_TL_PM_DEBUG0);
    const UINT32 link0Counter0Addr = regs.LookupAddress(MODS_PC2C_C2C0_LINK_TL_PM_DEBUG0);


    for (UINT32 partitionId = 0; partitionId < GetMaxPartitions(); partitionId++)
    {
        for (UINT32 lwrLinkIdx = 0; lwrLinkIdx < GetLinksPerPartition(); lwrLinkIdx++)
        {
            const UINT32 lwrLink = (partitionId * GetLinksPerPartition()) + lwrLinkIdx;

            pPmCounts->insert(make_pair(lwrLink, vector<PerfMonCount>()));

            for (auto const & lwrConfig : m_PmConfigs)
            {
                PerfMonCount pmCount;
                const UINT32 counterRegAddr = link0Counter0Addr +
                                              (lwrLink * counterRegStride) +
                                              (lwrConfig.first * sizeof(UINT32));

                pmCount.pmDataType = lwrConfig.second;
                pmCount.pmCount    = 4ULL * GetDevice()->RegRd32(counterRegAddr);
                pPmCounts->at(lwrLink).push_back(pmCount);
                GetDevice()->RegWr32(counterRegAddr, 0);
            }
        }
    }

    return RC::OK;
}

// C2LWphy
//-----------------------------------------------------------------------------
UINT32 HopperC2C::DoGetActiveLaneMask(RegBlock regBlock) const
{
    // C2C doesnt have an equivalent CLN block
    if (regBlock == RegBlock::CLN)
        return 0;

    UINT32 activeMask = 0;
    const UINT32 partLinkMask = (1U << GetLinksPerPartition()) - 1;
    INT32 partId = Utility::BitScanForward(GetActivePartitionMask());
    for ( ; partId != -1; partId++)
    {
        activeMask |= (partLinkMask << (partId * GetLinksPerPartition()));
    }
    return activeMask;
}

//-----------------------------------------------------------------------------
UINT32 HopperC2C::DoGetMaxRegBlocks(RegBlock regBlock) const
{
    // C2C doesnt have an equivalent CLN block
    if (regBlock == RegBlock::CLN)
        return 0;
    return GetMaxPartitions() * GetLinksPerPartition();
}

//-----------------------------------------------------------------------------
UINT32 HopperC2C::DoGetMaxLanes(RegBlock regBlock) const
{
    // C2C doesnt have an equivalent CLN block
    if (regBlock == RegBlock::CLN)
        return 0;
    return 1;
}

//-----------------------------------------------------------------------------
UINT64 HopperC2C::DoGetRegLogRegBlockMask(RegBlock regBlock)
{
    // C2C doesnt have an equivalent CLN block
    if (regBlock == RegBlock::CLN)
        return 0;
    return m_UphyRegsLinkMask;
}

//-----------------------------------------------------------------------------
RC HopperC2C::DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive)
{
    // C2C doesnt have an equivalent CLN block
    if (regBlock == RegBlock::CLN)
        *pbActive = false;
    else
        *pbActive = IsPartitionActive(blockIdx / GetLinksPerPartition());
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperC2C::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
{
    if (linkId >= (GetMaxPartitions() * GetLinksPerPartition()))
    {
        Printf(Tee::PriError, "Invalid link ID %u when reading pad lane register!\n", linkId);
        return RC::BAD_PARAMETER;
    }

    if (lane > 0)
    {
        Printf(Tee::PriError, "Invalid lane %u when reading pad lane register!\n", lane);
        return RC::BAD_PARAMETER;
    }

    RegHal & regs = GetDevice()->Regs();
    const UINT32 linkStride = regs.LookupAddress(MODS_PC2C_C2C1_PLLSEC_PRIV_LEVEL_MASK) -
                              regs.LookupAddress(MODS_PC2C_C2C0_PLLSEC_PRIV_LEVEL_MASK);
    const UINT32 regAddr = regs.LookupAddress(MODS_PC2C_C2C0_PLLSEC_PRIV_LEVEL_MASK) +
                           (linkId * linkStride) +
                           addr;
    *pData = GetDevice()->RegRd32(regAddr);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperC2C::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
{
    if (linkId >= (GetMaxPartitions() * GetLinksPerPartition()))
    {
        Printf(Tee::PriError, "Invalid link ID %u when writing pad lane register!\n", linkId);
        return RC::BAD_PARAMETER;
    }

    if (lane > 0)
    {
        Printf(Tee::PriError, "Invalid lane %u when writing pad lane register!\n", lane);
        return RC::BAD_PARAMETER;
    }
    RegHal & regs = GetDevice()->Regs();
    const UINT32 linkStride = regs.LookupAddress(MODS_PC2C_C2C1_PLLSEC_PRIV_LEVEL_MASK) -
                              regs.LookupAddress(MODS_PC2C_C2C0_PLLSEC_PRIV_LEVEL_MASK);
    const UINT32 regAddr = regs.LookupAddress(MODS_PC2C_C2C0_PLLSEC_PRIV_LEVEL_MASK) +
                           (linkId * linkStride) +
                           addr;
    GetDevice()->RegWr32(regAddr, data);
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC HopperC2C::DoSetRegLogLinkMask(UINT64 linkMask)
{
    const UINT32 maxLinkMask = (1U << (GetLinksPerPartition() * GetMaxPartitions())) - 1;

    if (maxLinkMask == 0ULL)
    {
        m_UphyRegsLinkMask  = 0ULL;
        return RC::OK;
    }

    m_UphyRegsLinkMask = (linkMask & maxLinkMask);

    const UINT64 partLinkMask = (1ULL << GetLinksPerPartition()) - 1;
    for (UINT32 partId = 0; partId < GetMaxPartitions(); partId++)
    {
        if (!IsPartitionActive(partId))
            m_UphyRegsLinkMask &= ~(partLinkMask << (partId * GetLinksPerPartition()));
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
GpuSubdevice* HopperC2C::GetGpuSubdevice()
{
    return dynamic_cast<GpuSubdevice*>(GetDevice());
}

//-----------------------------------------------------------------------------
const GpuSubdevice* HopperC2C::GetGpuSubdevice() const
{
    return dynamic_cast<const GpuSubdevice*>(GetDevice());
}

