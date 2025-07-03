/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  utility/pexdev.cpp
 * @brief Manage Bridge/chipset devices.
 *
 *
 */

#include "pexdev.h"
#include "br04/br04_ref.h"
#include "core/include/cpu.h"
#include "core/include/deprecat.h"
#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/version.h"
#include "core/utility/obfuscate_string.h"
#include "ctrl/ctrl2080/ctrl2080bus.h"
#include "device/interface/pcie.h"
#include "device/interface/lwlink/lwlregs.h"
#include "device/interface/portpolicyctrl.h"
#include "device/interface/xusbhostctrl.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/perf/perfsub.h"
#include "gpu/utility/js_testdevice.h"
#include "jsapi.h"
#include "Lwcm.h"
#include "lwdevid.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#if defined(INCLUDE_AZALIA)
#include "device/azalia/azactrl.h"
#endif

#include <algorithm>

/* static */ PexDevice::OnAerLogFull PexDevice::s_OnAerLogFull     = PexDevice::AER_LOG_DELETE_OLD; //$
/* static */ size_t                  PexDevice::s_AerLogMaxEntries = 10;
/* static */ size_t                  PexDevice::s_AerUEStatusMaxClearCount = 32;
/* static */ bool                    PexDevice::s_SkipHwCounterInit = false;
/* static */ UINT32 PexDevice::RP_LINK_POWER = 0;
/* static */ UINT32 PexDevice::RP_LINK_POWER_L2_BIT = 0;
/* static */ UINT32 PexDevice::RP_LINK_POWER_L0_BIT = 0;
/* static */ UINT32 PexDevice::RP_LINK_POWER_MAGIC_BIT = 0;

namespace
{
    const UINT32 ILWALID_PORT                  = ~0U;
    const UINT32 QEMU_DUPLICATE_PORT           = (1U << 31);
    const UINT32 PLX88000_DEV_VENDOR           = 0xC0101000;
    const UINT32 PLX88000S_DEV_VENDOR          = 0xC0121000;
    const UINT32 PLX880XX_RCVR_ERR_COUNT_OFFSET = 0xBF4;

    const UINT64 PEX880XX_PORT_REGISTER_OFFSET  = 8*1024*1024;
    const UINT64 PEX87XX_PORT_REGISTER_OFFSET   = 0;

    const UINT64 PLX_PORT_REGISTER_STRIDE  = 4*1024;

    const set<UINT32> s_PlxDevVendorIds  =
    {
        0x874710B5,
        0x876410B5,
        0x878010B5,
        0x872510B5,
        0x874910B5,
        0x879610B5,
        0x974910B5,
        0x976510B5,
        0x978110B5,
        0x979710B5
    };
    const set<UINT32> s_Plx880XXDevVendorIds  =
    {
        PLX88000_DEV_VENDOR,
        PLX88000S_DEV_VENDOR,
    };

    UINT32 s_VerboseFlags = 0;

    Tee::Priority GetVerbosePri()
    {
        return (s_VerboseFlags & (1 << PexDevMgr::PEX_VERBOSE_ENABLE)) ? Tee::PriNormal :
                                                                         Tee::PriLow;
    }
};

//-----------------------------------------------------------------------------
//! \brief Override the "+=" operator in order to facilitate error aclwmulation
//!
//! \param rhs : Object to add to the current one.
//!
//! \return Reference to the current object
//!
PexErrorCounts & PexErrorCounts::operator+=(const PexErrorCounts &rhs)
{
    for (UINT32 countIdx = 0; countIdx < NUM_ERR_COUNTERS; countIdx++)
    {
        m_IsCounterValidMask |= rhs.m_IsCounterValidMask;
        m_IsHwCounterMask |= rhs.m_IsHwCounterMask;
        m_ErrCounts[countIdx] += rhs.m_ErrCounts[countIdx];
    }
    return *this;
}

//-----------------------------------------------------------------------------
//! \brief Override the ">" operator in order to facilitate error checking
//!
//! \param rhs : Object to check if less than the current object.
//!
//! \return True if the provided object is less than the current one
//!
bool PexErrorCounts::operator > (const PexErrorCounts &rhs) const
{
    for (UINT32 countIdx = 0; countIdx < NUM_ERR_COUNTERS; countIdx++)
    {
        if ((m_IsThresholdMask & (1 << countIdx)) &&
            (m_IsCounterValidMask & (1 << countIdx)) &&
            (rhs.m_IsCounterValidMask & (1 << countIdx)) &&
            (m_ErrCounts[countIdx] > rhs.m_ErrCounts[countIdx]))
        {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Get the error count associated with the specified index
//!
//! \param CountIdx : Count index to retrieve the count for
//!
//! \return Value for the specified count
//!
UINT32 PexErrorCounts::GetCount(const UINT32 CountIdx) const
{
    MASSERT(CountIdx < NUM_ERR_COUNTERS);
    return m_ErrCounts[CountIdx];
}

//-----------------------------------------------------------------------------
//! \brief Static function to get a string associated with the index
//!
//! \param CountIdx : Count index to retrieve the string for
//!
//! \return String representing the error index
//!
string PexErrorCounts::GetString(const UINT32 CountIdx)
{
    static const string s_ErrStrings[] =
    {
         "Correctable"
        ,"Non-Fatal"
        ,"Fatal"
        ,"Unsupported Request"
        ,"LineErrors"
        ,"CRCErrors"
        ,"NAKs Received"
        ,"FailedL0sExits"
        ,"NAKs Sent"
        ,"Replays"
        ,"ReceiverErrors"
        ,"LaneErrorsSum"
        ,"BadDLLPCount"
        ,"ReplayRolloverCount"
        ,"8B10BErrors"
        ,"SyncHeaderErrors"
        ,"BadTLPCount"
    };

    MASSERT(CountIdx < NUM_ERR_COUNTERS);
    if (CountIdx >= NUM_ERR_COUNTERS)
    {
        Printf(Tee::PriError,
               "PexErrorCounts::GetString : Unknown counter index %d\n",
               CountIdx);
        return "Unknown";
    }
    if (static_cast<UINT32>(CountIdx) > (sizeof(s_ErrStrings)/sizeof(string)))
    {
        Printf(Tee::PriError,
               "PexErrorCounts::GetString : Please update counter to "
               "string array\n");
        return "Unknown";
    }

    return s_ErrStrings[CountIdx];
}

//-----------------------------------------------------------------------------
//! \brief Static function to get whether a count is for internal use only
//!
//! \param CountIdx : Count index to check internal only
//!
//! \return true if the count is for internal use only, false otherwise
//!
bool PexErrorCounts::IsInternalOnly(const UINT32 CountIdx)
{
    return CountIdx >= FIRST_INTERNAL_ONLY_ID;
}

//-----------------------------------------------------------------------------
//! \brief Set a specified error count
//!
//! \param CountIdx : Count index to set the count on
//! \param bHwCount : True if the provided count is from a hardware counter,
//!                   false if from a hardware flag
//! \param Count    : Actual count
//!
//! \return OK if successful, not OK otherwise
//!
RC PexErrorCounts::SetCount
(
    const UINT32 CountIdx,
    const bool   bHwCount,
    const UINT32 Count
)
{
    if (CountIdx >= NUM_ERR_COUNTERS)
    {
        Printf(Tee::PriError,
               "Invalid PEX counter index : 0x%08x\n",
               CountIdx);
        return RC::BAD_PARAMETER;
    }

    m_IsCounterValidMask |= (1 << CountIdx);
    if (bHwCount)
        m_IsHwCounterMask |= (1 << CountIdx);
    m_ErrCounts[CountIdx] = Count;

    return OK;
}

RC PexErrorCounts::SetPerLaneCounts(UINT08 *perLaneCounts)
{
    memcpy(m_PerLaneErrCounts, perLaneCounts, sizeof(UINT08) * LW2080_CTRL_PEX_MAX_LANES);
    UINT32 sum = 0;
    for (UINT32 i = 0; i < LW2080_CTRL_PEX_MAX_LANES; ++i)
    {
        sum += m_PerLaneErrCounts[i];
    }
    return SetCount(LANE_ID, true, sum);
}

UINT08 PexErrorCounts::GetSingleLaneErrorCount(UINT32 laneIdx)
{
    return m_PerLaneErrCounts[laneIdx];
}

//-----------------------------------------------------------------------------
//! \brief Return if a specified count is valid
//!
//! \param CountIdx   : Count index to check validity on
//!
//! \return true if valid, false if invalid
//!
bool PexErrorCounts::IsValid(const UINT32 CountIdx) const
{
    MASSERT(CountIdx < NUM_ERR_COUNTERS);
    return (m_IsCounterValidMask & (1 << CountIdx)) != 0;
}

//-----------------------------------------------------------------------------
//! \brief Return if a specified count is a threshold counter
//!
//! \param CountIdx   : Count index to check threshold counter on
//!
//! \return true if a threshold, false if non threshold
//!
bool PexErrorCounts::IsThreshold(const UINT32 CountIdx) const
{
    MASSERT(CountIdx < NUM_ERR_COUNTERS);
    return (m_IsThresholdMask & (1 << CountIdx)) != 0;
}

//-----------------------------------------------------------------------------
//! \brief Reset the data in the counter
//!
void PexErrorCounts::Reset()
{
    for (UINT32 countIdx = 0; countIdx < NUM_ERR_COUNTERS; countIdx++)
    {
        m_ErrCounts[countIdx] = 0;
    }

    memset((void *)m_PerLaneErrCounts, 0, sizeof(UINT08) * LW2080_CTRL_PEX_MAX_LANES);

    m_IsCounterValidMask = 0;
    m_IsHwCounterMask = 0;
    m_IsThresholdMask = (1 << FIRST_NON_THRESHOLD_COUNTER) - 1;
}

RC PexErrorCounts::TranslatePexCounterTypeToPexErrCountIdx(UINT32 lw2080PexCounterType,
                                                           PexErrCountIdx *outIdx)
{
    MASSERT(outIdx);
    map<UINT32, PexErrCountIdx>::iterator it;
    it = lw2080PexCounterTypeToPexErrCountIdxMap.find(lw2080PexCounterType);
    if (it == lw2080PexCounterTypeToPexErrCountIdxMap.end())
    {
        Printf(Tee::PriError, "Pex counter type not found in translation map\n");
        return RC::SOFTWARE_ERROR;
    }

    *outIdx = it->second;

    return OK;
}

map<UINT32, PexErrorCounts::PexErrCountIdx>
PexErrorCounts::CreateLw2080PexCounterTypeToPexErrCountIdxMap()
{
    map<UINT32, PexErrorCounts::PexErrCountIdx> translationMap;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_NAKS_SENT_COUNT] =       PexErrorCounts::DETAILED_NAKS_S_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_REPLAY_COUNT] =          PexErrorCounts::REPLAY_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_RECEIVER_ERRORS] =       PexErrorCounts::RECEIVER_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_LANE_ERRORS] =           PexErrorCounts::LANE_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_BAD_DLLP_COUNT] =        PexErrorCounts::BAD_DLLP_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_REPLAY_ROLLOVER_COUNT] = PexErrorCounts::REPLAY_ROLLOVER_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_8B10B_ERRORS_COUNT] =    PexErrorCounts::E_8B10B_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_SYNC_HEADER_ERRORS_COUNT] = PexErrorCounts::SYNC_HEADER_ID;
    translationMap[LW2080_CTRL_BUS_PEX_COUNTER_BAD_TLP_COUNT] =         PexErrorCounts::BAD_TLP_ID;
    return translationMap;
}

map<UINT32, PexErrorCounts::PexErrCountIdx> PexErrorCounts::lw2080PexCounterTypeToPexErrCountIdxMap(
    PexErrorCounts::CreateLw2080PexCounterTypeToPexErrCountIdxMap());

// Constructor: call pass in all the PciPorts detected on the PexDevice
// Note: UpStreamPort can be null for the top most device
// DownStreamPorts is a list of connections to downstream devices. The number
// of downstream ports can vary (especially the chipset's PCIE controller)
PexDevice::PexDevice(bool IsRoot,
                     PciDevice* UpStreamPort,
                     vector<PciDevice*> DownStreamPorts)
{
    m_IsRoot = IsRoot;
    m_Depth = DEPTH_UNINITIALIZED;
    m_UpStreamPort = UpStreamPort;
    if (m_UpStreamPort)
    {
        m_UpStreamPort->pBar0 = NULL;
    }

    if (IsRoot)
    {
        m_Type = CHIPSET;
    }
    else
    {
        m_Type = UpStreamPort->Type;
    }

    UINT16 extCapOffset;
    UINT16 extCapSize;
    for (UINT32 i = 0; i < DownStreamPorts.size(); i++)
    {
        m_DownStreamPort.push_back(DownStreamPorts[i]);
        m_DownStreamPort[i]->pBar0 = NULL;
        if (IsRoot)
        {
            m_DownStreamPort[i]->Type = CHIPSET;
        }
        Pci::FindCapBase(m_DownStreamPort[i]->Domain,
                         m_DownStreamPort[i]->Bus,
                         m_DownStreamPort[i]->Dev,
                         m_DownStreamPort[i]->Func,
                         PCI_CAP_ID_PCIE,
                         &(m_DownStreamPort[i]->PcieCapPtr));

        // 0 is invalid offset and can be reliably used to indicate that the
        // L1 extended cap block is not present
        m_DownStreamPort[i]->PcieL1ExtCapPtr = 0;
        if (OK == Pci::GetExtendedCapInfo(m_DownStreamPort[i]->Domain,
                                          m_DownStreamPort[i]->Bus,
                                          m_DownStreamPort[i]->Dev,
                                          m_DownStreamPort[i]->Func,
                                          Pci::L1SS_ECI,
                                          m_DownStreamPort[i]->PcieCapPtr,
                                          &extCapOffset,
                                          &extCapSize))
        {
            m_DownStreamPort[i]->PcieL1ExtCapPtr = extCapOffset;
        }

        // 0 is invalid offset and can be reliably used to indicate that the
        // DPC extended cap block is not present
        m_DownStreamPort[i]->PcieDpcExtCapPtr = 0;
        if (OK == Pci::GetExtendedCapInfo(m_DownStreamPort[i]->Domain,
                                          m_DownStreamPort[i]->Bus,
                                          m_DownStreamPort[i]->Dev,
                                          m_DownStreamPort[i]->Func,
                                          Pci::DPC_ECI,
                                          m_DownStreamPort[i]->PcieCapPtr,
                                          &extCapOffset,
                                          &extCapSize))
        {
            m_DownStreamPort[i]->PcieDpcExtCapPtr = extCapOffset;
        }

        m_DownStreamPort[i]->PcieAerExtCapPtr = 0;
        m_DownStreamPort[i]->PcieAerExtCapSize = 0;
        if (OK == Pci::GetExtendedCapInfo(m_DownStreamPort[i]->Domain,
                                          m_DownStreamPort[i]->Bus,
                                          m_DownStreamPort[i]->Dev,
                                          m_DownStreamPort[i]->Func,
                                          Pci::AER_ECI,
                                          m_DownStreamPort[i]->PcieCapPtr,
                                          &extCapOffset,
                                          &extCapSize))
        {
            m_DownStreamPort[i]->PcieAerExtCapPtr  = extCapOffset;
            m_DownStreamPort[i]->PcieAerExtCapSize = extCapSize;
        }
    }

    if ((m_Type == UNKNOWN_PEXDEV) || (m_Type == QEMU_BRIDGE) || (m_Type == PLX))
    {
        Pci::FindCapBase(m_UpStreamPort->Domain,
                         m_UpStreamPort->Bus,
                         m_UpStreamPort->Dev,
                         m_UpStreamPort->Func,
                         PCI_CAP_ID_PCIE,
                         &(m_UpStreamPort->PcieCapPtr));

        // 0 is invalid offset and can be reliably used to indicate that the
        // L1 extended cap block is not present
        m_UpStreamPort->PcieL1ExtCapPtr = 0;
        if (OK == Pci::GetExtendedCapInfo(m_UpStreamPort->Domain,
                                          m_UpStreamPort->Bus,
                                          m_UpStreamPort->Dev,
                                          m_UpStreamPort->Func,
                                          Pci::L1SS_ECI,
                                          m_UpStreamPort->PcieCapPtr,
                                          &extCapOffset,
                                          &extCapSize))
        {
            m_UpStreamPort->PcieL1ExtCapPtr = extCapOffset;
        }

        // DPC is only valid on downstream ports
        m_UpStreamPort->PcieDpcExtCapPtr = 0;

        m_UpStreamPort->PcieAerExtCapPtr = 0;
        m_UpStreamPort->PcieAerExtCapSize = 0;
        if (OK == Pci::GetExtendedCapInfo(m_UpStreamPort->Domain,
                                          m_UpStreamPort->Bus,
                                          m_UpStreamPort->Dev,
                                          m_UpStreamPort->Func,
                                          Pci::AER_ECI,
                                          m_UpStreamPort->PcieCapPtr,
                                          &extCapOffset,
                                          &extCapSize))
        {
            m_UpStreamPort->PcieAerExtCapPtr  = extCapOffset;
            m_UpStreamPort->PcieAerExtCapSize = extCapSize;
        }
    }

    m_Initialized     = false;
    m_AspmL0sAllowed  = false;
    m_AspmL1Allowed   = false;
    m_HasAspmL1Substates = false;
    m_AspmL11Allowed  = false;
    m_AspmL12Allowed  = false;
    m_DpcCap = Pci::DPC_CAP_NONE;
    m_UsePollPcieStatus = false;
    m_UpStreamIndex = 0;
}
//-----------------------------------------------------------------------------
RC PexDevice::Initialize()
{
    RC rc;
    if (m_Initialized)
    {
        Printf(GetVerbosePri(),
               "Pex device %d already initialized.  Nothing to do!\n",
               (m_UpStreamPort) ? m_UpStreamPort->Bus : 0);
        return OK;
    }

    if (m_Type == BR04)
    {
        // attempt to MAP Bar0. otherwise fall back to reading PCI config space
        MASSERT(m_UpStreamPort);
        LW2080_CTRL_BUS_HWBC_GET_UPSTREAM_BAR0_PARAMS Params = { };
        Params.primaryBus = m_UpStreamPort->Bus;
        LwRmPtr pLwRm;
        LwRm::Handle LwRmHandle = GetPexDevHandle();
        if (OK == pLwRm->Control(LwRmHandle,
                               LW2080_CTRL_CMD_BUS_HWBC_GET_UPSTREAM_BAR0,
                               &Params,
                               sizeof(Params)))
        {
            CHECK_RC(Platform::MapDeviceMemory(&m_UpStreamPort->pBar0,
                                               Params.physBAR0,
                                               LW_BR04_XVU_CONFIG_SIZE,
                                               Memory::UC,
                                               Memory::ReadWrite));

            for (UINT32 i = 0; i < m_DownStreamPort.size(); i++)
            {
                CHECK_RC(Platform::MapDeviceMemory(&m_DownStreamPort[i]->pBar0,
                                                   Params.physBAR0 +
                                                   LW_BR04_XVD_OFFSET(m_DownStreamPort[i]->Dev),
                                                   LW_BR04_XVD_CONFIG_SIZE,
                                                   Memory::UC,
                                                   Memory::ReadWrite));
            }
        }

    }
    else if(m_Type == PLX && !s_SkipHwCounterInit)
    {
        m_pErrorCounters = make_unique<PlxErrorCounters>();

        RC tempRC = m_pErrorCounters->Initialize(this);
        if (tempRC != RC::UNSUPPORTED_FUNCTION)
        {
            CHECK_RC(tempRC);
        }
    }

    CHECK_RC(SetupCapabilities());

    m_Initialized = true;
    return OK;
}
//-----------------------------------------------------------------------------
RC PexDevice::Shutdown()
{
    if (!m_Initialized)
    {
        Printf(GetVerbosePri(),
               "PexDevice not initialized or already shutdown.  "
               "Nothing to do.\n");
        return OK;
    }
    if (m_Type == BR04)
    {
        if (m_UpStreamPort->pBar0)
        {
            Platform::UnMapDeviceMemory(m_UpStreamPort->pBar0);
        }
        for (UINT32 i = 0; i < m_DownStreamPort.size(); i++)
        {
            if (m_DownStreamPort[i]->pBar0)
            {
                Platform::UnMapDeviceMemory(m_DownStreamPort[i]->pBar0);
            }
        }
    }

    if (m_pErrorCounters)
    {
        m_pErrorCounters->Shutdown();
    }

    m_Initialized = false;
    return OK;
}
//-----------------------------------------------------------------------------
RC PexDevice::SetDepth(UINT32 Depth)
{
    if (m_Depth != DEPTH_UNINITIALIZED)
    {
        Printf(Tee::PriNormal, "Depth already set\n");
        return RC::SOFTWARE_ERROR;
    }
    m_Depth = Depth;
    return OK;
}
//-----------------------------------------------------------------------------
PexDevice* PexDevice::GetUpStreamDev()
{
    if (m_UpStreamPort)
        return (PexDevice*)(m_UpStreamPort->pDevice);
    else
        return NULL;
}
//-----------------------------------------------------------------------------
PexDevice::PciDevice* PexDevice::GetDownStreamPort(UINT32 Index)
{
    if (GetNumDPorts() <= Index)
    {
        Printf(Tee::PriNormal, "Invalid port index\n");
        return NULL;
    }
    return m_DownStreamPort[Index];
}
//-----------------------------------------------------------------------------
bool PexDevice::IsDownStreamPexDevice(UINT32 Index)
{
    if (GetNumDPorts() <= Index)
    {
        Printf(Tee::PriNormal, "Invalid port index\n");
        return false;
    }
    return m_DownStreamPort[Index]->IsPexDevice;
}
//-----------------------------------------------------------------------------
PexDevice* PexDevice::GetDownStreamPexDevice(UINT32 Index)
{
    if (GetNumDPorts() <= Index)
    {
        Printf(Tee::PriNormal, "Invalid port index\n");
        return NULL;
    }
    return (PexDevice*)(m_DownStreamPort[Index]->pDevice);
}
//-----------------------------------------------------------------------------
// Resets the downstream port. Does not save config space or verify success.
void PexDevice::ResetDownstreamPort(UINT32 port)
{
    UINT32 bridgeDomain   = GetDownStreamPort(port)->Domain;
    UINT32 bridgeBus      = GetDownStreamPort(port)->Bus;
    UINT32 bridgeDevice   = GetDownStreamPort(port)->Dev;
    UINT32 bridgeFunction = GetDownStreamPort(port)->Func;

    Printf(Tee::PriDebug,
           "Resetting using bridge %04x:%02x:%02x.%x\n",
           bridgeDomain,
           bridgeBus,
           bridgeDevice,
           bridgeFunction);

    // Read the bridge's control register
    UINT16 data = 0;
    Platform::PciRead16(bridgeDomain,
                        bridgeBus,
                        bridgeDevice,
                        bridgeFunction,
                        PCI_HEADER_TYPE1_BRIDGE_CONTROL,
                        &data);
    UINT16 temp = data | BIT(6);

    // Set the secondary bus reset bit, triggering the hot reset
    Printf(Tee::PriDebug, "Asserting secondary bus reset...\n");
    Platform::PciWrite16(bridgeDomain,
                         bridgeBus,
                         bridgeDevice,
                         bridgeFunction,
                         PCI_HEADER_TYPE1_BRIDGE_CONTROL,
                         temp);

    // Wait for 1ms for the reset as defined in the PCIE spec
    Tasker::Sleep(1);

    // Clear secondary bus reset bit
    temp = data & ~BIT(6);

    Printf(Tee::PriDebug, "Deasserting secondary bus reset...\n");
    Platform::PciWrite16(bridgeDomain,
                         bridgeBus,
                         bridgeDevice,
                         bridgeFunction,
                         PCI_HEADER_TYPE1_BRIDGE_CONTROL,
                         temp);
}
//-----------------------------------------------------------------------------
GpuSubdevice* PexDevice::GetDownStreamGpu(UINT32 Index)
{
    if (GetNumDPorts() <= Index)
    {
        Printf(Tee::PriNormal, "Invalid port index\n");
        return NULL;
    }
    return (GpuSubdevice*)(m_DownStreamPort[Index]->pDevice);
}
//-----------------------------------------------------------------------------
UINT32 PexDevice::GetNumActiveDPorts()
{
    UINT32 ActivePorts = 0;
    for (UINT32 i = 0; i < m_DownStreamPort.size(); i++)
    {
        PciDevice* pDPort = m_DownStreamPort[i];
        if (pDPort->pDevice)
            ActivePorts++;
    }
    return ActivePorts;
}
//-----------------------------------------------------------------------------
//! This is called from PexDev manager so that we can connect the current
// PexDev with the device above (another bridge or chipset)
RC PexDevice::SetUpStreamDevice(PexDevice* pUpStream, UINT32 Index)
{
    MASSERT(pUpStream);
    RC rc;
    if (pUpStream->GetNumDPorts() <= Index)
    {
        Printf(Tee::PriNormal, "Invalid port index\n");
        return RC::BAD_PARAMETER;
    }
    if (m_UpStreamPort->pDevice)
    {
        Printf(Tee::PriError, "Upstream port's device Ptr already set\n");
        return RC::SOFTWARE_ERROR;
    }

    m_UpStreamPort->pDevice = pUpStream;
    m_UpStreamIndex = Index;
    return rc;
}
// ---------- these functions require RM control API calls --------------------
// Get the RmHandle
UINT32 PexDevice::GetPexDevHandle()
{
// WAR: The current RM API accepts handle to GPU; however, this handle is not
//      used internally by the API. Ideally, the handle for bridge should be
//      independent of GPU.
// tracking in bug 385198
    GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    MASSERT(pGpuDevMgr);
    GpuSubdevice* pSubdev = pGpuDevMgr->GetFirstGpu();
    LwRmPtr pLwRm;
    return pLwRm->GetSubdeviceHandle(pSubdev);
}
//-----------------------------------------------------------------------------
RC PexDevice::ChangeUpStreamSpeed(Pci::PcieLinkSpeed NewSpeed)
{
    RC rc;
    if (NewSpeed == GetUpStreamSpeed())
    {
        Printf(Tee::PriDebug, "no change in speed required\n");
        return OK;
    }

    if (m_Type != BR04)
    {
        Printf(Tee::PriNormal, "Up stream speed change is only supported in BR04\n");
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_BUS_SET_HWBC_UPSTREAM_PCIE_SPEED_PARAMS Params = { };
    switch (NewSpeed)
    {
        case Pci::Speed2500MBPS:
            Params.busSpeed = LW2080_CTRL_BUS_SET_HWBC_UPSTREAM_PCIE_SPEED_2500MBPS;
            break;
        case Pci::Speed5000MBPS:
            Params.busSpeed = LW2080_CTRL_BUS_SET_HWBC_UPSTREAM_PCIE_SPEED_5000MBPS;
            break;
        default:
            return RC::BAD_PARAMETER;
    }
    MASSERT(m_UpStreamPort);
    Params.primaryBus = m_UpStreamPort->Bus;
    LwRmPtr pLwRm;
    LwRm::Handle LwRmHandle = GetPexDevHandle();
    CHECK_RC(pLwRm->Control(LwRmHandle,
                   LW2080_CTRL_CMD_BUS_SET_HWBC_UPSTREAM_PCIE_SPEED,
                   &Params,
                   sizeof(Params)));
    return OK;
}
//-----------------------------------------------------------------------------
Pci::PcieLinkSpeed PexDevice::GetUpStreamSpeed()
{
    if (m_Type == BR03)
    {
        return Pci::Speed2500MBPS;
    }
    else if (m_Type == CHIPSET)
    {
        return Pci::SpeedUnknown;
    }
    else if ((m_Type == UNKNOWN_PEXDEV) || (m_Type == QEMU_BRIDGE) || (m_Type == PLX))
    {
        UINT16 StatReg16 = 0;
        const UINT32 Offset = LW_PCI_CAP_PCIE_LINK_STS +
                              m_UpStreamPort->PcieCapPtr;
        Platform::PciRead16(m_UpStreamPort->Domain,
                            m_UpStreamPort->Bus,
                            m_UpStreamPort->Dev,
                            m_UpStreamPort->Func,
                            Offset,
                            &StatReg16);
        const UINT32 SpeedInReg = REF_VAL(LW_PCI_CAP_PCIE_LINK_STS_LINK_SPEED,
                                          StatReg16);
        switch (SpeedInReg)
        {
            case 1:
                return Pci::Speed2500MBPS;
            case 2:
                return Pci::Speed5000MBPS;
            case 3:
                return Pci::Speed8000MBPS;
            case 4:
                return Pci::Speed16000MBPS;
            case 5:
                return Pci::Speed32000MBPS;
            default:
                return Pci::SpeedUnknown;
        }
    }

    // BR04
    LW2080_CTRL_BUS_GET_HWBC_UPSTREAM_PCIE_SPEED_PARAMS Params = { };
    MASSERT(m_UpStreamPort);
    Params.primaryBus = m_UpStreamPort->Bus;

    LwRmPtr pLwRm;
    LwRm::Handle LwRmHandle = GetPexDevHandle();
    if (OK != pLwRm->Control(LwRmHandle,
                   LW2080_CTRL_CMD_BUS_GET_HWBC_UPSTREAM_PCIE_SPEED,
                   &Params,
                   sizeof(Params)))
    {
        Printf(Tee::PriNormal, "Rm call failed in GetUpStreamSpeed\n");
        return Pci::SpeedUnknown;
    }

    Pci::PcieLinkSpeed RetVal;
    switch (Params.busSpeed)
    {
        case LW2080_CTRL_BUS_GET_HWBC_UPSTREAM_PCIE_SPEED_2500MBPS:
            RetVal = Pci::Speed2500MBPS;
            break;
        case LW2080_CTRL_BUS_GET_HWBC_UPSTREAM_PCIE_SPEED_5000MBPS:
            RetVal = Pci::Speed5000MBPS;
            break;
        default:
            MASSERT(!"Invalid link speed!\n");
            RetVal = Pci::SpeedUnknown;
    }
    Printf(Tee::PriDebug, "UpStream speed at bus %02x is %d\n",
           m_UpStreamPort->Bus, RetVal);
    return RetVal;
}

Pci::PcieLinkSpeed PexDevice::GetDownStreamSpeed(UINT32 port)
{
    if (m_Type == BR03)
    {
        return Pci::Speed2500MBPS;
    }

    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
        return Pci::SpeedUnknown;

    UINT16 statReg16 = 0;
    const UINT32 offset = LW_PCI_CAP_PCIE_LINK_STS +
                          pPciDev->PcieCapPtr;
    Platform::PciRead16(pPciDev->Domain,
                        pPciDev->Bus,
                        pPciDev->Dev,
                        pPciDev->Func,
                        offset,
                        &statReg16);
    const UINT32 speedInReg = REF_VAL(LW_PCI_CAP_PCIE_LINK_STS_LINK_SPEED,
                                      statReg16);
    switch (speedInReg)
    {
        case 1:
            return Pci::Speed2500MBPS;
        case 2:
            return Pci::Speed5000MBPS;
        case 3:
            return Pci::Speed8000MBPS;
        case 4:
            return Pci::Speed16000MBPS;
        case 5:
            return Pci::Speed32000MBPS;
        default:
            break;
    }
    return Pci::SpeedUnknown;
}

//-----------------------------------------------------------------------------
RC PexDevice::SetDownStreamSpeed(UINT32 port, Pci::PcieLinkSpeed speed)
{
    if (GetDownStreamLinkSpeedCap(port) < speed)
    {
        Printf(Tee::PriWarn, "Unable to set down stream speed\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    UINT16 statReg16 = 0;
    const UINT32 offset = LW_PCI_CAP_PCIE_LINK_CTRL2 +
                          pPciDev->PcieCapPtr;
    Platform::PciRead16(pPciDev->Domain,
                        pPciDev->Bus,
                        pPciDev->Dev,
                        pPciDev->Func,
                        offset,
                        &statReg16);
    switch (speed)
    {
        case Pci::Speed2500MBPS:
            statReg16 = FLD_SET_DRF(_PCI, _CAP_PCIE_LINK_CTRL2, _TARGET_LINK_SPEED, _GEN1, statReg16);
            break;
        case Pci::Speed5000MBPS:
            statReg16 = FLD_SET_DRF(_PCI, _CAP_PCIE_LINK_CTRL2, _TARGET_LINK_SPEED, _GEN2, statReg16);
            break;
        case Pci::Speed8000MBPS:
            statReg16 = FLD_SET_DRF(_PCI, _CAP_PCIE_LINK_CTRL2, _TARGET_LINK_SPEED, _GEN3, statReg16);
            break;
        case Pci::Speed16000MBPS:
            statReg16 = FLD_SET_DRF(_PCI, _CAP_PCIE_LINK_CTRL2, _TARGET_LINK_SPEED, _GEN4, statReg16);
            break;
        case Pci::Speed32000MBPS:
            statReg16 = FLD_SET_DRF(_PCI, _CAP_PCIE_LINK_CTRL2, _TARGET_LINK_SPEED, _GEN5, statReg16);
            break;
        default:
            MASSERT(!"Unknown PCIE Link Speed");
            return RC::SOFTWARE_ERROR;;
    }
    Platform::PciWrite16(pPciDev->Domain,
                         pPciDev->Bus,
                         pPciDev->Dev,
                         pPciDev->Func,
                         offset,
                         statReg16);
    return RC::OK;

}

//-----------------------------------------------------------------------------
// internal
RC PexDevice::GetAspmStateInt(PciDevice *pPciDev,
                                 UINT32 Offset,
                                 UINT32 *pState)
{
    RC rc;
    UINT32 Ctrl = 0;
    CHECK_RC(Read(pPciDev, Offset, &Ctrl));
    // this is common between BR04 and root completx
    *pState = REF_VAL(LW_BR04_XVU_LINK_CTRLSTAT_ASPM_CTRL,
                                      Ctrl);
    return rc;
}

//-----------------------------------------------------------------------------
// internal
RC PexDevice::GetAspmL1SubStateInt(PciDevice *pPciDev, UINT32 *pState)
{
    MASSERT(pPciDev);
    MASSERT(pState);

    *pState = 0;
    if (!HasAspmL1Substates())
        return RC::UNSUPPORTED_HARDWARE_FEATURE;

    UINT32 l1ssControl = 0;
    RC rc;
    CHECK_RC(Read(pPciDev, pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL, &l1ssControl));

    if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_ASPM, 1, l1ssControl))
    {
        *pState |= Pci::PM_SUB_L12;
    }
    if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L11_ASPM, 1, l1ssControl))
    {
        *pState |= Pci::PM_SUB_L11;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// internal
RC PexDevice::EnableBusMasterInt(PciDevice *pPciDev)
{
    RC rc;
    UINT32 offset;

    if ((m_Type == BR04) || m_Type == BR03)
        offset = LW_BR04_XVD_DEV_CMD;
    else
        offset = LW_PCI_COMMAND;

    UINT32 cmd = 0;
    CHECK_RC(Read(pPciDev, offset, &cmd));

    cmd = FLD_SET_DRF(_BR04_XVD, _DEV_CMD, _MEMORY_SPACE, _ENABLED, cmd);
    cmd = FLD_SET_DRF(_BR04_XVD, _DEV_CMD, _BUS_MASTER, _ENABLED, cmd);

    CHECK_RC(Write(pPciDev, offset, cmd));

    return OK;
}

//-----------------------------------------------------------------------------
RC PexDevice::GetDownstreamPortHotplugEnabled(UINT32 port, bool *pEnable)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        return RC::BAD_PARAMETER;
    }

    UINT16 value = 0;
    CHECK_RC(Platform::PciRead16(pPciDev->Domain,
                        pPciDev->Bus,
                        pPciDev->Dev,
                        pPciDev->Func,
                        pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_SLOT_CTRL,
                        &value));

    *pEnable = FLD_TEST_DRF(_PCI_CAP, _PCIE_SLOT_CTRL, _HOTPLUG_INT, _ENABLED, value);
    return rc;
}

//-----------------------------------------------------------------------------
RC PexDevice::SetDownstreamPortHotplugEnabled(UINT32 port, bool enable)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        return RC::BAD_PARAMETER;
    }

    UINT32 slotCtrlOffset = LW_PCI_CAP_PCIE_SLOT_CTRL + pPciDev->PcieCapPtr;
    UINT16 slotCtrl = 0;
    CHECK_RC(Platform::PciRead16(pPciDev->Domain,
                        pPciDev->Bus,
                        pPciDev->Dev,
                        pPciDev->Func,
                        slotCtrlOffset,
                        &slotCtrl));

    if (enable == FLD_TEST_DRF(_PCI_CAP, _PCIE_SLOT_CTRL, _HOTPLUG_INT, _ENABLED, slotCtrl))
    {
        // skip if field already matches what we want
        return rc;
    }

    if (enable)
    {
        // clear pending interrupts. all the hotplug event status bits are cleared by writing 1
        UINT32 slotStatusOffset = LW_PCI_CAP_PCIE_SLOT_STS + pPciDev->PcieCapPtr;
        UINT16 slotStatus = 0;
        CHECK_RC(Platform::PciRead16(pPciDev->Domain,
                            pPciDev->Bus,
                            pPciDev->Dev,
                            pPciDev->Func,
                            slotStatusOffset,
                            &slotStatus));
        CHECK_RC(Platform::PciWrite16(pPciDev->Domain,
                            pPciDev->Bus,
                            pPciDev->Dev,
                            pPciDev->Func,
                            slotStatusOffset,
                            slotStatus));

        slotCtrl = FLD_SET_DRF(_PCI_CAP, _PCIE_SLOT_CTRL, _HOTPLUG_INT, _ENABLED, slotCtrl);
    }
    else
    {
        slotCtrl = FLD_SET_DRF(_PCI_CAP, _PCIE_SLOT_CTRL, _HOTPLUG_INT, _DISABLED, slotCtrl);
    }

    CHECK_RC(Platform::PciWrite16(pPciDev->Domain,
                        pPciDev->Bus,
                        pPciDev->Dev,
                        pPciDev->Func,
                        slotCtrlOffset,
                        slotCtrl));

    UINT32 slotCap = 0;
    CHECK_RC(Platform::PciRead32(pPciDev->Domain,
                        pPciDev->Bus,
                        pPciDev->Dev,
                        pPciDev->Func,
                        LW_PCI_CAP_PCIE_SLOT_CAP + pPciDev->PcieCapPtr,
                        &slotCap));

    // poll on the command completed bit for 1sec, if supported.
    // skip for intel bridges due to errata CF118.
    // skip for broadcom (including plx)
    if (!DRF_VAL(_PCI_CAP, _PCIE_SLOT_CAP, _NO_CMD_COMPLETE, slotCap) &&
        ((pPciDev->VendorDeviceId & 0xFFFF) != Pci::Intel) &&
        ((pPciDev->VendorDeviceId & 0xFFFF) != Pci::Plx) &&
        ((pPciDev->VendorDeviceId & 0xFFFF) != Pci::Broadcom))
    {
        rc = Tasker::PollHw(1000, [&]()->bool
            {
                UINT16 slotStatus = 0;
                Platform::PciRead16(pPciDev->Domain,
                                    pPciDev->Bus,
                                    pPciDev->Dev,
                                    pPciDev->Func,
                                    LW_PCI_CAP_PCIE_SLOT_STS + pPciDev->PcieCapPtr,
                                    &slotStatus);
                return DRF_VAL(_PCI_CAP, _PCIE_SLOT_STS, _CMD_COMPLETE, slotStatus);
            }, __FUNCTION__);

        if (rc == RC::TIMEOUT_ERROR)
        {
            Printf(Tee::PriLow, "Timed out waiting for pcie command complete event.\n");
            rc.Clear();
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC PexDevice::GetUpStreamAspmState(UINT32 *pState)
{
    if (IsRoot())
        return RC::SOFTWARE_ERROR;

    PciDevice *pPciDev = GetUpStreamPort();
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    UINT32 Offset;
    if ((m_Type == BR04) || m_Type == BR03)
    {
        Offset = LW_BR04_XVD_LINK_CTRLSTAT;
    }
    else
    {
        Offset = LW_PCI_CAP_PCIE_LINK_CTRL + pPciDev->PcieCapPtr;
    }

    return GetAspmStateInt(pPciDev, Offset, pState);
}
//-----------------------------------------------------------------------------

RC PexDevice::GetDownStreamAspmState(UINT32 *pState, UINT32 Port)
{
    MASSERT(pState);

    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    UINT32 Offset;
    // bug 571832 will get rid of this 'if' check.
    if (!IsRoot() && ((m_Type == BR04) || m_Type == BR03))
    {
        Offset = LW_BR04_XVD_LINK_CTRLSTAT;
    }
    else
    {
        Offset = LW_PCI_CAP_PCIE_LINK_CTRL + pPciDev->PcieCapPtr;
    }

    CHECK_RC(GetAspmStateInt(pPciDev, Offset, pState));
    return OK;
}

RC PexDevice::ValidateAspmState(bool bForce, UINT32 port, UINT32 *pState)
{
    if (!bForce)
    {
        UINT32 newState = *pState & GetAspmCap();
        if (newState != *pState)
        {
            Printf(GetVerbosePri(),
                   "%s : Restricting ASPM to 0x%x (requested 0x%x) due to ASPM Cap\n",
                   __FUNCTION__, newState, *pState);
            *pState = newState;
        }
    }

    PciDevice *pPciDev = (port != ILWALID_PORT) ? GetDownStreamPort(port) : GetUpStreamPort();
    if ((m_Type != PLX) || s_Plx880XXDevVendorIds.count(pPciDev->VendorDeviceId))
        return OK;

    // PLX errata (only applicable to non 880xx PLX devices):
    // https://p4viewer.lwpu.com/getfile///hw/brdsoln/Infrastructure/Projects/PLX/8747_gen3_Bridge/Docs/Errata/PEX87xx_48lanefamily_Errata_v1.7_29Aug13.pdf
    //
    // Item #10 on page 4, L0S broken at all speeds
    // Item #9 on page 7, L1 broken at Gen3
    //
    // MODS cannot error when L1 is enabled but must simply print a message
    // since POR was set to L1 on Gemini boards with PLX even though it
    // contradicts the errata
    if (*pState & Pci::ASPM_L0S)
    {
        if (!bForce)
        {
            Printf(Tee::PriError, "ASPM L0S not supported on PLX chips\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
        else
        {
            Printf(Tee::PriWarn, "ASPM L0S force enabled on PLX, errors may occur\n");
        }
    }

    Pci::PcieLinkSpeed speed = (port != ILWALID_PORT) ? GetDownStreamSpeed(port) :
                                                        GetUpStreamSpeed();
    if ((*pState & Pci::ASPM_L1) && (speed == Pci::Speed8000MBPS || speed == Pci::Speed16000MBPS))
    {
        Printf(Tee::PriWarn,
               "ASPM L1 not supported on PLX in Gen3 or Gen4 but is enabled, errors may occur\n");
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC PexDevice::ReadSpeedAndWidthCap
(
    PciDevice *pPciDev,
    Pci::PcieLinkSpeed *pLinkSpeed,
    UINT32 *pLinkWidth
)
{
    MASSERT(pPciDev);

    // based on table 7-14 in PCI Express Rev2 document
    UINT32 LinkCapRegister;
    const UINT32 LinkCapOffset = LW_PCI_CAP_PCIE_LINK_CAP +
                                 pPciDev->PcieCapPtr;
    RC rc;
    CHECK_RC(Read(pPciDev, LinkCapOffset, &LinkCapRegister));

    const UINT32 maxSpeed = REF_VAL(LW_PCI_CAP_PCIE_LINK_CAP_LINK_SPEED,
                                    LinkCapRegister);
    const UINT32 maxWidth = REF_VAL(LW_PCI_CAP_PCIE_LINK_CAP_LINK_WIDTH,
                                    LinkCapRegister);
    // According to the Gen3 spec the speed bits specify a bit location
    // in the Supported Link Speeds Vector (in the Link Capabilities 2
    // register) that corresponds to the maximum Link speed.  This
    // effectively means that (1 == 2500, 2 = 5000, 3 = 8000)
    //
    // Re-reading the Gen2 and Gen1 specs, this also should work with
    // with them (i.e. this only represents the *max* speed, not a
    // not a bitmask of all supported speeds)
    //
    // Note : Previous implementations marked a value of 3 as Gen2
    // on the assumption that this was a bitmask of all supported speeds
    // (e.g. 0x2 indicates only support for Gen2 and 0x3 indicates
    // support for both Gen1 and Gen2).  Reading the Gen2 spec this
    // appears to be an incorrect assumption. It is possible that this
    // was implemented through empirical testing on multiple Gen2
    // motherboards.  However until a motherboard is found that breaks
    // the spec the following implementation is based on the current
    // language in the Gen1/Gen2/Gen3 specs.
    //
    if (pLinkSpeed)
    {
        *pLinkSpeed = Pci::Speed2500MBPS;
        switch (maxSpeed)
        {
            case 1:
                break;
            case 2:
                *pLinkSpeed = Pci::Speed5000MBPS;
                break;
            case 3:
                *pLinkSpeed = Pci::Speed8000MBPS;
                break;
            case 4:
                *pLinkSpeed = Pci::Speed16000MBPS;
                break;
            case 5:
                *pLinkSpeed = Pci::Speed32000MBPS;
                break;
            default:
                Printf(Tee::PriNormal, " Unknown PCIE speed cap 0x%x\n", maxSpeed);
                break;
        }
    }
    if (pLinkWidth)
        *pLinkWidth = maxWidth;
    return rc;
}

//-----------------------------------------------------------------------------
// Not sure how valuable this function is
RC PexDevice::SetUpStreamAspmState(UINT32 State, bool bForce)
{
    if (IsRoot())
        return RC::SOFTWARE_ERROR;

    PciDevice *pPciDev = GetUpStreamPort();
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    RC rc;
    CHECK_RC(ValidateAspmState(bForce, ILWALID_PORT, &State));

    UINT32 Offset;
    if ((m_Type == BR04) || m_Type == BR03)
    {
        Offset = LW_BR04_XVD_LINK_CTRLSTAT;
    }
    else
    {
        Offset = LW_PCI_CAP_PCIE_LINK_CTRL + pPciDev->PcieCapPtr;
    }
    UINT32 Ctrl = 0;
    CHECK_RC(Read(pPciDev, Offset, &Ctrl));
    Ctrl = FLD_SET_DRF_NUM(_BR04_XVD_LINK,
                           _CTRLSTAT,
                           _ASPM_CTRL,
                           State,
                           Ctrl);
    CHECK_RC(Write(pPciDev, Offset, Ctrl));
    return OK;
}

//-----------------------------------------------------------------------------
RC PexDevice::SetDownStreamAspmState(UINT32 State, UINT32 Port, bool bForce)
{
    PciDevice *pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    RC rc;
    CHECK_RC(ValidateAspmState(bForce, Port, &State));

    UINT32 Offset;

    if (!IsRoot() && ((m_Type == BR04) || m_Type == BR03))
    {
        Offset = LW_BR04_XVD_LINK_CTRLSTAT;
    }
    else
    {
        Offset = LW_PCI_CAP_PCIE_LINK_CTRL + pPciDev->PcieCapPtr;
    }

    UINT32 Ctrl = 0;
    CHECK_RC(Read(pPciDev, Offset, &Ctrl));
    Ctrl = FLD_SET_DRF_NUM(_BR04_XVD_LINK,
                           _CTRLSTAT,
                           _ASPM_CTRL,
                           State,
                           Ctrl);

    Printf(GetVerbosePri(), "PEX: set downstream ASPM=0x%x\n", Ctrl);
    CHECK_RC(Write(pPciDev, Offset, Ctrl));
    return OK;
}

//! Gets the current L1 substate of ASPM (disabled, L11, L12, L11/L12)
RC PexDevice::GetDownStreamAspmL1SubState(UINT32 *pState, UINT32 Port)
{
    PciDevice *pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;
    return GetAspmL1SubStateInt(pPciDev, pState);
}

RC PexDevice::GetUpStreamAspmL1SubState(UINT32 *pState)
{
    if (IsRoot())
        return RC::SOFTWARE_ERROR;

    PciDevice *pPciDev = GetUpStreamPort();
    if (!pPciDev)
        return RC::BAD_PARAMETER;
    return GetAspmL1SubStateInt(pPciDev, pState);
}

//! Get the current DPC state
RC PexDevice::GetDownStreamDpcState(Pci::DpcState *pState, UINT32 Port)
{
    MASSERT(pState);

    PciDevice *pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    *pState = Pci::DPC_STATE_DISABLED;
    if (m_DpcCap == Pci::DPC_CAP_NONE)
        return OK;

    UINT32 dpcCapCtrl;
    RC rc;
    CHECK_RC(Read(pPciDev, pPciDev->PcieDpcExtCapPtr + LW_PCI_DPC_CAP_CTRL, &dpcCapCtrl));

    switch (DRF_VAL(_PCI, _DPC_CAP_CTRL, _TRIG_ENABLE, dpcCapCtrl))
    {
        case LW_PCI_DPC_CAP_CTRL_TRIG_ENABLE_DISABLED:
            *pState = Pci::DPC_STATE_DISABLED;
            break;
        case LW_PCI_DPC_CAP_CTRL_TRIG_ENABLE_FATAL:
            *pState = Pci::DPC_STATE_FATAL;
            break;
        case LW_PCI_DPC_CAP_CTRL_TRIG_ENABLE_NON_FATAL:
            *pState = Pci::DPC_STATE_NON_FATAL;
            break;
        default:
            Printf(Tee::PriError, "Unknown DPC trigger state!\n");
            return RC::HW_STATUS_ERROR;
    }

    return rc;
}

//! Set the current DPC state
RC PexDevice::SetDownStreamDpcState(Pci::DpcState state, UINT32 Port)
{
    PciDevice *pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    if (m_DpcCap == Pci::DPC_CAP_NONE)
        return OK;

    UINT32 dpcCapCtrl;
    RC rc;
    CHECK_RC(Read(pPciDev, pPciDev->PcieDpcExtCapPtr + LW_PCI_DPC_CAP_CTRL, &dpcCapCtrl));
    switch (state)
    {
        case Pci::DPC_STATE_DISABLED:
            dpcCapCtrl = FLD_SET_DRF(_PCI, _DPC_CAP_CTRL, _TRIG_ENABLE, _DISABLED, dpcCapCtrl);
            break;
        case Pci::DPC_STATE_FATAL:
            dpcCapCtrl = FLD_SET_DRF(_PCI, _DPC_CAP_CTRL, _TRIG_ENABLE, _FATAL, dpcCapCtrl);
            break;
        case Pci::DPC_STATE_NON_FATAL:
            dpcCapCtrl = FLD_SET_DRF(_PCI, _DPC_CAP_CTRL, _TRIG_ENABLE, _NON_FATAL, dpcCapCtrl);
            break;
        default:
            Printf(Tee::PriError, "Unknown DPC trigger state %d!\n", state);
            return RC::HW_STATUS_ERROR;
    }
    CHECK_RC(Write(pPciDev, pPciDev->PcieDpcExtCapPtr + LW_PCI_DPC_CAP_CTRL, dpcCapCtrl));
    return rc;
}

UINT32 PexDevice::GetAspmCap()
{
    UINT32 Cap = Pci::ASPM_DISABLED;
    if (m_AspmL0sAllowed)
    {
        Cap |= Pci::ASPM_L0S;
    }
    if (m_AspmL1Allowed)
    {
        Cap |= Pci::ASPM_L1;
    }
    return Cap;
}

UINT32 PexDevice::GetAspmL1SSCap()
{
    UINT32 Cap = Pci::PM_SUB_DISABLED;
    if (m_AspmL11Allowed)
    {
        Cap |= Pci::PM_SUB_L11;
    }
    if (m_AspmL12Allowed)
    {
        Cap |= Pci::PM_SUB_L12;
    }
    return Cap;
}

// Each 'Link' has two ends. Each end of the link may have different width
RC PexDevice::GetUpStreamLinkWidths(UINT32 *pLocWidth, UINT32 *pHostWidth)
{
    MASSERT(pLocWidth);
    MASSERT(pHostWidth);
    if (!m_UpStreamPort || m_IsRoot)
    {
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    //
    CHECK_RC(GetLinkWidth(pLocWidth, m_UpStreamPort));
    PexDevice* pHost = (PexDevice*)m_UpStreamPort->pDevice;
    // note: m_UpStreamIndex = the port index that the current bridge is connected
    //                         to the device above
    PciDevice* pHostDP = pHost->GetDownStreamPort(m_UpStreamIndex);
    CHECK_RC(GetLinkWidth(pHostWidth, pHostDP));
    return rc;
}

RC PexDevice::GetDownStreamLinkWidth(UINT32* pWidth, UINT32 Port)
{
    MASSERT(pWidth);
    RC rc;
    PciDevice* pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
    {
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(GetLinkWidth(pWidth, pPciDev));
    return rc;
}

namespace
{
    Pci::PcieErrorsMask GetErrorMask(const PexDevice::DevErrors& errCounts)
    {
        Pci::PcieErrorsMask mask = 0U;
        for (int i=0; i <= PexErrorCounts::UNSUP_REQ_ID; ++i)
        {
            if (errCounts[i])
            {
                mask |= 1U << i;
            }
        }
        return mask;
    }
}

// !GetUpStreamErrors:
//
// a host can be the chipset's PCIE ctrl or another bridge
//             |--------------|
//             |    Host      |
//             |-------|------|   Host Errors
//                     |
//             |-------|------|   Loc Errors
//             | This Bridge  |
//             |--|--|--|--|--|
//
// pLocErrors: Errors read at the upstream port of current bridge device
// pHostErrors: errors read from the device above -> could be chipset or
//              another bridge
RC PexDevice::GetUpStreamErrors(Pci::PcieErrorsMask* pLocErrors,
                                   Pci::PcieErrorsMask* pHostErrors)
{
    MASSERT(pLocErrors);
    MASSERT(pHostErrors);
    // get errors on the local Upstream port
    // Root has no upstream port
    if (!m_UpStreamPort || m_IsRoot)
    {
        return RC::SOFTWARE_ERROR;
    }
    RC rc;
    DevErrors locErrCount;
    CHECK_RC(GetPcieErrors(m_UpStreamPort, &locErrCount));

    // Get errors on downstream port of the Bridge/Chipset above:
    // 1) Get the Upstream device:
    PexDevice* pHost = (PexDevice*)m_UpStreamPort->pDevice;
    // note: m_UpStreamIndex = the port index that the current bridge is connected
    //                         to the device above
    PciDevice* pHostDP = pHost->GetDownStreamPort(m_UpStreamIndex);
    DevErrors hostErrCount;
    CHECK_RC(GetPcieErrors(pHostDP, &hostErrCount));

    *pLocErrors  = GetErrorMask(locErrCount);
    *pHostErrors = GetErrorMask(hostErrCount);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get local and host error counts
//!
//! a host can be the chipset's PCIE ctrl or another bridge
//!             |--------------|
//!             |    Host      |
//!             |-------|------|   Host Errors
//!                     |
//!             |-------|------|   Loc Errors
//!             | This Bridge  |
//!             |--|--|--|--|--|
//!
//! \param pLocErrors : Errors read at the upstream port of current bridge device
//! \param pHostErrors: Errors read from the donstream port of thedevice above
//!                     (could be chipset or another bridge)
//! \param CountType  : Type of errors to retrieve (all, hw counter only, hw
//!                     flag only)
//!
//! \return OK if successful, not OK otherwise
//!
RC PexDevice::GetUpStreamErrorCounts
(
    PexErrorCounts *pLocErrors,
    PexErrorCounts *pHostErrors,
    PexCounterType  CountType
)
{
    MASSERT(pLocErrors);
    MASSERT(pHostErrors);
    // get errors on the local Upstream port
    // Root has no upstream port
    if (!m_UpStreamPort || m_IsRoot)
    {
        return RC::SOFTWARE_ERROR;
    }
    RC rc;
    CHECK_RC(GetPcieErrorCounts(pLocErrors,
                                m_UpStreamPort,
                                m_UsePollPcieStatus,
                                CountType));

    // Get errors on downstream port of the Bridge/Chipset above:
    // 1) Get the Upstream device:
    PexDevice* pHost = (PexDevice*)m_UpStreamPort->pDevice;
    // note: m_UpStreamIndex = the port index that the current bridge is connected
    //                         to the device above
    PciDevice* pHostDP = pHost->GetDownStreamPort(m_UpStreamIndex);
    CHECK_RC(GetPcieErrorCounts(pHostErrors,
                                pHostDP,
                                pHost->UsePollPcieStatus(),
                                CountType));
    return rc;
}

RC PexDevice::GetDownStreamErrors(Pci::PcieErrorsMask* pDownErrors,
                                     UINT32 Port)
{
    MASSERT(pDownErrors);
    RC rc;
    PciDevice* pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
    {
        return RC::BAD_PARAMETER;
    }
    //pPciDev->Bus
    // for now - until RM's API is ready:
    DevErrors errCount;
    CHECK_RC(GetPcieErrors(pPciDev, &errCount));

    *pDownErrors = GetErrorMask(errCount);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get errors on the specified downstream port
//!
//! \param pDownErrors : Errors read at the downstream port of current bridge
//!                      device
//! \param Port        : Port to read errors from
//! \param CountType   : Type of errors to retrieve (all, hw counter only, hw
//!                      flag only)
//!
//! \return OK if successful, not OK otherwise
//!
RC PexDevice::GetDownStreamErrorCounts
(
    PexErrorCounts* pDownErrors,
    UINT32          Port,
    PexCounterType  CountType
)
{
    MASSERT(pDownErrors);
    RC rc;
    PciDevice* pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
    {
        return RC::BAD_PARAMETER;
    }
    //pPciDev->Bus
    // for now - until RM's API is ready:
    CHECK_RC(GetPcieErrorCounts(pDownErrors,
                                pPciDev,
                                m_UsePollPcieStatus,
                                CountType));
    return rc;
}

RC PexDevice::GetDownStreamAerLog(UINT32 Port, vector<AERLogEntry> *pLogData)
{
    PciDevice* pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    pLogData->clear();
    pLogData->reserve(pPciDev->AerLog.size());
    Tasker::MutexHolder grabMutex(pPciDev->Mutex);
    pLogData->insert(pLogData->begin(),
                     pPciDev->AerLog.begin(),
                     pPciDev->AerLog.end());
    return OK;
}

RC PexDevice::GetUpStreamAerLog(vector<AERLogEntry> *pLogData)
{
    if (!m_UpStreamPort || m_IsRoot)
        return RC::SOFTWARE_ERROR;

    pLogData->clear();
    pLogData->reserve(m_UpStreamPort->AerLog.size());
    Tasker::MutexHolder grabMutex(m_UpStreamPort->Mutex);
    pLogData->insert(pLogData->begin(),
                     m_UpStreamPort->AerLog.begin(),
                     m_UpStreamPort->AerLog.end());
    return OK;
}

RC PexDevice::ClearDownStreamAerLog(UINT32 Port)
{
    PciDevice* pPciDev = GetDownStreamPort(Port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;
    Tasker::MutexHolder grabMutex(pPciDev->Mutex);
    pPciDev->AerLog.clear();
    return OK;
}

RC PexDevice::ClearUpStreamAerLog()
{
    if (!m_UpStreamPort || m_IsRoot)
        return RC::SOFTWARE_ERROR;
    Tasker::MutexHolder grabMutex(m_UpStreamPort->Mutex);
    m_UpStreamPort->AerLog.clear();
    return OK;
}

//-----------------------------------------------------------------------------
RC PexDevice::GetDownstreamAtomicRoutingCapable(UINT32 port, bool *pCapable)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    UINT32 value;
    CHECK_RC(Read(pPciDev, pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CAP2, &value));
    *pCapable = FLD_TEST_DRF(_PCI_CAP, _PCIE_CAP2, _ATOMICS_ROUTE_SUPPORTED, _YES, value);
    return rc;
}

//-----------------------------------------------------------------------------
RC PexDevice::GetUpstreamAtomicRoutingCapable(bool *pCapable)
{
    RC rc;
    PciDevice *pPciDev = GetUpStreamPort();
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    UINT32 value;
    CHECK_RC(Read(pPciDev, pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CAP2, &value));
    *pCapable = FLD_TEST_DRF(_PCI_CAP, _PCIE_CAP2, _ATOMICS_ROUTE_SUPPORTED, _YES, value);
    return rc;
}

//-----------------------------------------------------------------------------
RC PexDevice::GetUpstreamAtomicEgressBlocked(bool *pEnable)
{
    RC rc;
    PciDevice *pPciDev = GetUpStreamPort();
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    UINT32 value;
    CHECK_RC(Read(pPciDev, pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CTRL2, &value));
    *pEnable = FLD_TEST_DRF(_PCI_CAP, _PCIE_CTRL2, _ATOMIC_EGRESS_BLOCK, _ENABLE, value);
    return rc;
}

//-----------------------------------------------------------------------------
RC PexDevice::GetDownstreamAtomicsEnabledMask(UINT32 port, UINT32 *pAtomicMask)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    UINT32 value;
    CHECK_RC(Read(pPciDev, pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CAP2, &value));

    if (FLD_TEST_DRF(_PCI_CAP, _PCIE_CAP2, _32BIT_ATOMICS_SUPPORTED, _YES, value))
    {
        *pAtomicMask |= Pci::ATOMIC_32BIT;
    }
    if (FLD_TEST_DRF(_PCI_CAP, _PCIE_CAP2, _64BIT_ATOMICS_SUPPORTED, _YES, value))
    {
        *pAtomicMask |= Pci::ATOMIC_64BIT;
    }
    if (FLD_TEST_DRF(_PCI_CAP, _PCIE_CAP2, _128BIT_ATOMICS_SUPPORTED, _YES, value))
    {
        *pAtomicMask |= Pci::ATOMIC_128BIT;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Resets all errors on the specified downstream port
//!        as well as the upstream port of the current bridge device
//!
//! \param DownPort : Downstream port to reset errors on
//!
//! \return OK if successful, not OK otherwise
//!
RC PexDevice::ResetErrors(UINT32 DownPort)
{
    PciDevice* pPciDev = GetDownStreamPort(DownPort);
    if (!pPciDev)
    {
        return RC::BAD_PARAMETER;
    }
    RC rc;
    CHECK_RC(ResetPcieErrors(pPciDev, PEX_COUNTER_ALL));

    if (m_UpStreamPort && !m_IsRoot)
    {
        CHECK_RC(ResetPcieErrors(m_UpStreamPort, PEX_COUNTER_ALL));
    }
    return rc;
}

//------------------------------------------------------------------------------
//! Get the downstream port's Latency Tolerance Reporting mechanism if it is supported by
//! the downstream port. Refer to PCI Express Base Spec Revision 3.0 sections 7.8.15 & 7.8.16
//!
//! \return RC::UNSUPPORTED_HARDWARE_FEATURE if LTR is not supported
//!         OK on success
RC PexDevice::GetDownstreamPortLTR(UINT32 port, bool *pEnable)
{
    RC rc;
    UINT32 cap2 = 0;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(Read(pPciDev,
                  pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CAP2,
                  &cap2));

    if (FLD_TEST_DRF(_PCI_CAP, _PCIE_CAP2, _LTR_SUPPORTED, _YES, cap2))
    {
        UINT32 ctrl2;
        CHECK_RC(Read(pPciDev,
                      pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CTRL2,
                      &ctrl2));
        *pEnable = DRF_VAL(_PCI_CAP, _PCIE_CTRL2, _LTR, ctrl2);
        return OK;
    }
    else
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
}

//------------------------------------------------------------------------------
//! Enable/disable the downstream port's Latency Tolerance Reporting mechanism if it is supported by
//! the downstream port. Refer to PCI Express Base Spec Revision 3.0 sections 7.8.15 & 7.8.16
//!
//! \return RC::UNSUPPORTED_HARDWARE_FEATURE if LTR is not supported
//!         OK on success
RC PexDevice::SetDownstreamPortLTR(UINT32 port, bool enable)
{
    RC rc;
    UINT32 cap2 = 0;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(Read(pPciDev,
                  pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CAP2,
                  &cap2));
    if (FLD_TEST_DRF(_PCI_CAP, _PCIE_CAP2, _LTR_SUPPORTED, _YES, cap2))
    {
        UINT32 ctrl2;
        CHECK_RC(Read(pPciDev,
                      pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CTRL2,
                      &ctrl2));
        ctrl2= FLD_SET_DRF_NUM(_PCI_CAP, _PCIE_CTRL2, _LTR, enable, ctrl2);
        CHECK_RC(Write(pPciDev,
                       pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CTRL2,
                       ctrl2));

        CHECK_RC(Read(pPciDev,
                      pPciDev->PcieCapPtr + LW_PCI_CAP_PCIE_CTRL2,
                      &ctrl2));
        if (!FLD_TEST_DRF_NUM(_PCI_CAP, _PCIE_CTRL2, _LTR, enable, ctrl2))
        {
            return RC::REGISTER_READ_WRITE_FAILED;
        }
        return OK;
    }
    else
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
}

//------------------------------------------------------------------------------
RC PexDevice::DisableDownstreamPort(UINT32 port)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
        return RC::BAD_PARAMETER;
    }

    UINT32 Offset;
    if ((m_Type == BR04) || m_Type == BR03)
    {
        Offset = LW_BR04_XVD_LINK_CTRLSTAT;
    }
    else
    {
        Offset = LW_PCI_CAP_PCIE_LINK_CTRL + pPciDev->PcieCapPtr;
    }

    UINT32 LinkCtrl = 0;
    CHECK_RC(Read(pPciDev, Offset, &LinkCtrl));

    LinkCtrl = FLD_SET_DRF_NUM(_BR04_XVD, _LINK_CTRLSTAT, _LINK_DISABLE, 1, LinkCtrl);
    CHECK_RC(Write(pPciDev, Offset, LinkCtrl));

    return OK;
}

//------------------------------------------------------------------------------
RC PexDevice::EnableDownstreamPort(UINT32 port)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
        return RC::BAD_PARAMETER;
    }

    UINT32 Offset;
    if ((m_Type == BR04) || m_Type == BR03)
    {
        Offset = LW_BR04_XVD_LINK_CTRLSTAT;
    }
    else
    {
        Offset = LW_PCI_CAP_PCIE_LINK_CTRL + pPciDev->PcieCapPtr;
    }

    UINT32 LinkCtrl = 0;
    CHECK_RC(Read(pPciDev, Offset, &LinkCtrl));

    LinkCtrl = FLD_SET_DRF_NUM(_BR04_XVD, _LINK_CTRLSTAT, _LINK_DISABLE, 0, LinkCtrl);
    CHECK_RC(Write(pPciDev, Offset, LinkCtrl));

    return OK;
}

// TODO: Make this function automatically detect platform type
// and set L2 bits itself
void PexDevice::InitializeRootPortPowerOffsets(PlatformType type)
{
    if (type == PlatformType::TGL)
    {
        // See http://lwbugs/2869417/81
        RP_LINK_POWER = 0xE0; // offset
        RP_LINK_POWER_L2_BIT = 0x40000; // Bit 18
        RP_LINK_POWER_L0_BIT = 0x80000; // Bit 19
        RP_LINK_POWER_MAGIC_BIT = 0x80; //Bit 7
    }
    else // CFL as default
    {
        RP_LINK_POWER = 0x248; // offset
        RP_LINK_POWER_L2_BIT = 0x80; // Bit 7
        RP_LINK_POWER_L0_BIT = 0x100; // Bit 8
    }
    m_PlatformType = type;
}

//--------------------------------------------------------------------
// Typical steps to put link from L0 to L2 (full power to low power):
//  1. Write 0x1 to RP_LINK_POWER[7] (L2 bit)
//  2. Wait till L2 bit gets reset by HW after operation is complete
//  3. Write 0x1 to P0RM offset (for physical link power management)
//  4. Write 0x3 to P0AP offset (for physical link power management)
//--------------------------------------------------------------------
// Typical steps to put link from L2 to L0 (low power to full power):
//  1. Write 0x1 to RP_LINK_POWER[8] (L0 bit)
//  2. Wait till L0 bit gets reset by HW after operation is complete
//  3. Write 0x0 to P0RM offset (for physical link power management)
//  4. Write 0x0 to P0AP offset (for physical link power management)
//--------------------------------------------------------------------
// This method perform step 1 mentioned above, step 2-4 are handled in
// PollDownstreamPortLinkStatus
RC PexDevice::SetLinkPowerState(UINT32 port, LinkPowerState state)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        return RC::SOFTWARE_ERROR;
    }

    UINT32 linkCtrl = 0;
    CHECK_RC(Read(pPciDev, RP_LINK_POWER, &linkCtrl));
    if (state == L2)
    {
        linkCtrl |= RP_LINK_POWER_L2_BIT;
    }
    else if (state == L0)
    {
        linkCtrl |= RP_LINK_POWER_L0_BIT;
    }
    else
    {
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(Write(pPciDev, RP_LINK_POWER, linkCtrl));
    Platform::Delay(16000); // 16ms as per SBIOS
    return rc;
}

RC PexDevice::SetL2MagicBit(UINT32 port, LinkPowerState linkState)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        return RC::SOFTWARE_ERROR;
    }

    UINT32 linkCtrl = 0;
    CHECK_RC(Read(pPciDev, RP_LINK_POWER, &linkCtrl));

    if (linkState == L2)
    {
        linkCtrl |= RP_LINK_POWER_MAGIC_BIT;
    }
    else
    {
        linkCtrl &= (~RP_LINK_POWER_MAGIC_BIT);
    }
    CHECK_RC(Write(pPciDev, RP_LINK_POWER, linkCtrl));

    if (linkState == L0)
    {
        Platform::Delay(16000); // As per SBIOS
    }
    return rc;
}

//------------------------------------------------------------------------------
// Read the Link Capabilities Register to determine it "Data Link Layer Active"
// reporting is supported.
// Return OK if supported, error otherwise
RC PexDevice::IsDLLAReportingSupported(UINT32 port, bool *pSupported)
{
    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
        return RC::BAD_PARAMETER;
    }

    // Intel devices that utilize unique polling don't advertise that they
    // support DLLAR but actually they do via their own magic register.
    // see PollLinkStatus().
    if (pPciDev->UniqueLinkPolling)
    {
        *pSupported = true;
        return OK;
    }

    UINT32 Offset;
    if (m_Type == BR04 || m_Type == BR03)
    {
        Offset = LW_BR04_XVD_LINK_CAP;
    }
    else
    {
        Offset = LW_PCI_CAP_PCIE_LINK_CAP + pPciDev->PcieCapPtr;
    }

    UINT32 LinkCap = 0;
    CHECK_RC(Read(pPciDev, Offset, &LinkCap));

    *pSupported =(bool) DRF_VAL(_BR04_XVD_LINK, _CAP_DLL, _ACTIVE_RPT, LinkCap);
    if ( ! *pSupported)
    {
        Printf(Tee::PriError,
               "DataLinkLayerActive reporting is not supported on this Pex device.\n");
    }
    return OK;
}

//------------------------------------------------------------------------------
// Check whether the downstream port will forward P2P requests upstream when the
// P2P target is connected to a different downstream port on the same device
RC PexDevice::GetP2PForwardingEnabled(UINT32 port, bool *pbEnabled)
{ 
    MASSERT(pbEnabled);

    RC rc;
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        return RC::SOFTWARE_ERROR;
    }

    UINT16 acsCapOffset;
    UINT16 acsCapSize;
    if (RC::OK != Pci::GetExtendedCapInfo(pPciDev->Domain,
                                          pPciDev->Bus,
                                          pPciDev->Dev,
                                          pPciDev->Func,
                                          Pci::ACS_ECI,
                                          pPciDev->PcieCapPtr,
                                          &acsCapOffset,
                                          &acsCapSize))
    {
        // If the ACS capability is not present then P2P packets should not
        // be forwarded upstream
        *pbEnabled = false;
        return RC::OK;
    }

    UINT16 acsCtrl = 0;
    Platform::PciRead16(pPciDev->Domain,
                        pPciDev->Bus,
                        pPciDev->Dev,
                        pPciDev->Func,
                        acsCapOffset + LW_PCI_CAP_ACS_CTRL,
                        &acsCtrl);

    *pbEnabled = FLD_TEST_DRF(_PCI_CAP, _ACS_CTRL, _P2P_REDIRECT, _ENABLE, acsCtrl) ||
                 FLD_TEST_DRF(_PCI_CAP, _ACS_CTRL, _P2P_COMP_REDIRECT, _ENABLE, acsCtrl) ||
                 FLD_TEST_DRF(_PCI_CAP, _ACS_CTRL, _UPSTREAM_FORWARD, _ENABLE, acsCtrl);
    return RC::OK;

}

//------------------------------------------------------------------------------
//! \brief UpStream device could not always be root port device on setups which
//! involve switches(e.g. servers). This function iterate up to device which
//! is actually root
//!
//! \param ppPexDev acts as both input and output argument of PexDevice
//! \param pPort is a port on which input ppPexDev is connected to root
//! \return OK if successful
//!
RC PexDevice::GetRootPortDevice(PexDevice **ppPexDev, UINT32 *pPort)
{
    RC rc;

    if (*ppPexDev && (*ppPexDev)->IsRoot())
    {
        return RC::OK;
    }

    while (*ppPexDev && !(*ppPexDev)->IsRoot())
    {
        *pPort = (*ppPexDev)->GetUpStreamIndex();
        *ppPexDev = (*ppPexDev)->GetUpStreamDev();
    }

    if (*ppPexDev == nullptr)
    {
        Printf(Tee::PriError, "Root port device not found\n");
        return RC::PCI_DEVICE_NOT_FOUND;
    }
    return rc;
}

//------------------------------------------------------------------------------
// Wait for upto timeoutMs to see if the link becomes active
RC PexDevice::PollDownstreamPortLinkStatus
(
    UINT32 port,    // downstream port to check
    UINT32 timeoutMs, // how long to wait for link active
    LinkPowerState linkState
)
{
    StickyRC rc;
    bool bSupported = false;
    CHECK_RC(IsDLLAReportingSupported(port, &bSupported));
    if ( ! bSupported )
        return RC::UNSUPPORTED_FUNCTION;

    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
        return RC::BAD_PARAMETER;
    }

    PollArgs args = { pPciDev, linkState };
    if (linkState == LINK_ENABLE || linkState == LINK_DISABLE)
    {
        rc = POLLWRAP(PollLinkStatus, &args, timeoutMs);
    }
    else if (linkState == L0 || linkState == L2)
    {
        rc = POLLWRAP(PollL0orL2Status, &args, timeoutMs);
        if (m_PlatformType == PlatformType::TGL)
        {
            rc = SetL2MagicBit(port, linkState);
        }
        else
        {
            rc = UpdatePLPMOffsetForL2(pPciDev, linkState);
        }
    }

    // Error condition
    if (OK != rc)
    {
        Printf(Tee::PriError, "Transition to %s wasn't successful after %d msec\n",
            GetLinkStateString(linkState), timeoutMs);
    }
    return rc;
}

//------------------------------------------------------------------------------
/*static*/
// Poll for LINK_ENABLE or LINK_DISABLE status
bool PexDevice::PollLinkStatus(void *pArgs)
{
    PollArgs * pPollArgs = static_cast<PollArgs*>(pArgs);
    PciDevice *pPciDev = pPollArgs->pPciDev;
    bool bUniquePoll = pPollArgs->pPciDev->UniqueLinkPolling;
    // Wait for the link active bit to get set
    UINT32 data = 0;
    Tasker::Sleep(1);

    if (bUniquePoll)
    {
        MASSERT((pPciDev->Type != BR04) && (pPciDev->Type != BR03));
        Platform::PciRead16(pPciDev->Domain,
                            pPciDev->Bus,
                            pPciDev->Dev,
                            pPciDev->Func,
                            0x216, // offset, magic value for Intel CRB
                            (UINT16*)&data);
        if (pPollArgs->linkState == LINK_ENABLE)
        {
            if ((data & 0xf) >= 7)
            {
                return true;
            }
        }
        else if (pPollArgs->linkState == LINK_DISABLE)
        {
            if ((data & 0xf) == 0)
            {
                return true;
            }
        }
    }
    else // standard algorithm for checking link state
    {
        UINT32 Offset;
        if (pPciDev->Type == BR04)
        {
            Offset = LW_BR04_XVD_LINK_CTRLSTAT;
        }
        else
        {
            Offset = LW_PCI_CAP_PCIE_LINK_CTRL + pPciDev->PcieCapPtr;
        }

        RC rc = Read(pPciDev, Offset, &data);
        if (OK != rc)
            return false;

        return (FLD_TEST_DRF(_BR04_XVD, _LINK_CTRLSTAT, _DLL_LINK_SM, _ACTIVE, data) ==
                (pPollArgs->linkState == LINK_ENABLE ? true : false));
    }
    return false;
}

// Poll for L0 or L2 status
bool PexDevice::PollL0orL2Status(void *pArgs)
{
    PollArgs * pPollArgs = static_cast<PollArgs*>(pArgs);
    PciDevice *pPciDev = pPollArgs->pPciDev;
    // Wait for the L0/L2 bit to get reset
    Tasker::Sleep(1);

    UINT32 linkCtrl = 0;
    RC rc = Read(pPciDev, RP_LINK_POWER, &linkCtrl);
    if (rc != OK)
        return false;
    if (pPollArgs->linkState == L2)
    {
        return (linkCtrl & RP_LINK_POWER_L2_BIT) ? false : true;
    }
    else if (pPollArgs->linkState == L0)
    {
        return (linkCtrl & RP_LINK_POWER_L0_BIT) ? false : true;
    }
    return false;
}

// PLPM=Physical link power management, these offset need to be set after entry to L2/L0
RC PexDevice::UpdatePLPMOffsetForL2(PciDevice *pPciDev, LinkPowerState linkState)
{
    RC rc;
    UINT32 linkCtrl = 0;
    if (linkState == L2)
    {
        CHECK_RC(Read(pPciDev, RP_P0RM, &linkCtrl));
        CHECK_RC(Write(pPciDev, RP_P0RM, linkCtrl | RP_P0RM_SET_BIT));
        CHECK_RC(Read(pPciDev, RP_P0AP, &linkCtrl));
        CHECK_RC(Write(pPciDev, RP_P0AP, linkCtrl | RP_P0AP_SET_BIT));
    }
    else if (linkState == L0)
    {
        CHECK_RC(Read(pPciDev, RP_P0RM, &linkCtrl));
        CHECK_RC(Write(pPciDev, RP_P0RM, linkCtrl & (~RP_P0RM_SET_BIT)));
        CHECK_RC(Read(pPciDev, RP_P0AP, &linkCtrl));
        CHECK_RC(Write(pPciDev, RP_P0AP, linkCtrl & (~RP_P0AP_SET_BIT)));
    }
    return rc;
}

const char *PexDevice::GetLinkStateString(LinkPowerState state)
{
    const char *linkStr[4] = {"L0", "L2", "LINK_ENABLE", "LINK_DISABLE"};
    return linkStr[state];
}

//------------------------------------------------------------------------------
// Enable bus mastering on the upstream port
RC PexDevice::EnableUpstreamBusMastering()
{
    if (IsRoot())
        return RC::SOFTWARE_ERROR;

    PciDevice *pPciDev = GetUpStreamPort();
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    return EnableBusMasterInt(pPciDev);
}

//------------------------------------------------------------------------------
// Enable bus mastering on the downstream port
RC PexDevice::EnableDownstreamBusMastering(UINT32 port)
{
    PciDevice *pPciDev = GetDownStreamPort(port);
    if (!pPciDev)
        return RC::BAD_PARAMETER;

    return EnableBusMasterInt(pPciDev);
}

//-----------------------Private functions------------------------
// Use RM to figure out the chipset type.
// We don't query the chipset/bridge capability bits because sometimes the
// products mark themselves as 'capable'
RC PexDevice::SetupCapabilities()
{
    RC rc;
    // Do PCIE query
    // case where we don't have prior knowledge about the chipset
    // just query the ASPM bits - using first downstream PCI port
// TODO: here we are assumping that all the ports on the PCIE device has
// the same ASPM, ASPM L1SS and DPC capability. This is true for almost all cases..
// but we should really change this
    PciDevice *pPciDev = GetDownStreamPort(0);
    if (!pPciDev)
    {
        Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, __FILE__, __LINE__);
        return RC::BAD_PARAMETER;
    }

    const UINT32 Offset = LW_PCI_CAP_PCIE_LINK_CAP + pPciDev->PcieCapPtr;

    UINT32 Cap;
    CHECK_RC(Read(pPciDev, Offset, &Cap));
    // this is common between BR04 and root completx
    Cap = REF_VAL(LW_BR04_XVD_LINK_CAP_ASPM_SUPPORT, Cap);
    m_AspmL0sAllowed = ((Cap & Pci::ASPM_L0S) != 0);
    m_AspmL1Allowed  = ((Cap & Pci::ASPM_L1) != 0);

    if (pPciDev->PcieL1ExtCapPtr != 0)
    {
        m_HasAspmL1Substates = true;

        UINT32 l1ssCaps;
        CHECK_RC(Read(pPciDev, pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CAPS, &l1ssCaps));
        if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CAPS, _L1_PM_SS, 1, l1ssCaps))
        {
            if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CAPS, _L12_ASPM, 1, l1ssCaps))
            {
                m_AspmL12Allowed = true;
            }
            if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CAPS, _L11_ASPM, 1, l1ssCaps))
            {
                m_AspmL11Allowed = true;
            }
        }
    }

    if (pPciDev->PcieDpcExtCapPtr != 0)
    {
        UINT32 dpcCaps;
        CHECK_RC(Read(pPciDev, pPciDev->PcieDpcExtCapPtr + LW_PCI_DPC_CAP_CTRL, &dpcCaps));
        m_DpcCap = Pci::DPC_CAP_BASIC;
        if (FLD_TEST_DRF(_PCI, _DPC_CAP_CTRL, _RP_EXT, _ENABLED, dpcCaps))
            m_DpcCap |= Pci::DPC_CAP_RP_EXT;
        if (FLD_TEST_DRF(_PCI, _DPC_CAP_CTRL, _POISONED_TLP, _ENABLED, dpcCaps))
            m_DpcCap |= Pci::DPC_CAP_POISONED_TLP;
        if (FLD_TEST_DRF(_PCI, _DPC_CAP_CTRL, _SW_TRIG, _ENABLED, dpcCaps))
            m_DpcCap |= Pci::DPC_CAP_SW_TRIG;
    }

    m_LinkSpeedCap.resize(GetNumDPorts());
    m_LinkWidthCap.resize(GetNumDPorts());
    for (UINT32 i = 0; i < GetNumDPorts(); i++)
    {
        pPciDev = GetDownStreamPort(i);
        CHECK_RC(ReadSpeedAndWidthCap(pPciDev, &m_LinkSpeedCap[i], &m_LinkWidthCap[i]));
    }

    // Overrides
    if (m_Type == BR04)
    {
        m_AspmL0sAllowed = true;
        m_AspmL1Allowed  = false;
    }
    if (m_Type == CHIPSET)
    {
        //Can't call ConfigGetEx() because GpuDevMgr is not full initialized.
        UINT32 VendorDeviceId;
        CHECK_RC(Read(pPciDev, 0, &VendorDeviceId));
        // check for IntelX58 platform
        if ((VendorDeviceId == (0x34000000 | PCI_VENDOR_ID_INTEL )) ||
            (VendorDeviceId == (0x34030000 | PCI_VENDOR_ID_INTEL )) ||
            (VendorDeviceId == (0x34050000 | PCI_VENDOR_ID_INTEL )))
        {
            m_AspmL0sAllowed = false;
            m_AspmL1Allowed  = true;
        }
    }
    else if (m_Type == PLX)
    {
        // Per PLX errata L0s is broken at all speeds and L1 is broken at Gen3
        // See ValidateAspmState() for more information
        m_AspmL0sAllowed = false;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the control status register offset
//!
//! \param pPciDev : Pci device to get the control status register offset
//!
//! \return Status register offset
//!
UINT32 PexDevice::GetPcieStatusOffset(PciDevice* pPciDev)
{
    if (!pPciDev->Root && ((pPciDev->Type == BR04) || pPciDev->Type == BR03))
    {
        return LW_BR04_XVU_DEV_CTRLSTAT;
    }
    MASSERT((pPciDev->Type == CHIPSET) ||
            (pPciDev->Type == UNKNOWN_PEXDEV) ||
            (pPciDev->Type == PLX) ||
            (pPciDev->Type == QEMU_BRIDGE));
    return LW_PCI_CAP_DEVICE_CTL + pPciDev->PcieCapPtr;
}

//-----------------------------------------------------------------------------
//! Brief: GetPcieErrors
//! Given the PCI port on the bridge device, read the status register
//! This will clear the status register after the read
RC PexDevice::GetPcieErrors(PciDevice* pPciDev,
                            DevErrors* pErrors)
{
    MASSERT(pErrors);
    MASSERT(pPciDev);
    MASSERT(pPciDev->Located);

    RC rc;
    CHECK_RC(ReadPcieErrors(pPciDev));

    for (int i=0; i <= PexErrorCounts::UNSUP_REQ_ID; ++i)
    {
        const UINT32 count = Cpu::AtomicXchg32(reinterpret_cast<volatile UINT32*>(
                                               &pPciDev->ErrCount[i]), 0U);
        (*pErrors)[i] = count;

        if (count)
        {
            Printf(Tee::PriLow, "PEX port %04x:%02x:%02x.%x - %u %s errors\n",
                static_cast<unsigned>(pPciDev->Domain),
                static_cast<unsigned>(pPciDev->Bus),
                static_cast<unsigned>(pPciDev->Dev),
                static_cast<unsigned>(pPciDev->Func),
                count,
                PexErrorCounts::GetString(i).c_str());
        }
    }

    return rc;
}

RC PexDevice::ReadPcieErrors(PciDevice* pPciDev)
{
    MASSERT(pPciDev);
    MASSERT(pPciDev->Located);
    MASSERT(pPciDev->Mutex);
    Tasker::MutexHolder grabMutex(pPciDev->Mutex);
    RC rc;
    UINT32 StatusReg;
    UINT32 Offset = GetPcieStatusOffset(pPciDev);

    CHECK_RC(Read(pPciDev, Offset, &StatusReg));

    const UINT32 ILWALID_STATUS_REG = 0xFFFFFFFFU;
    if (StatusReg == ILWALID_STATUS_REG)
        return OK;

    const Pci::PcieErrorsMask errMask = REF_VAL(LW_PCI_CAP_DEVICE_CTL_ERROR_BITS, StatusReg);

    // Need to dump the AER block (at least) when an error oclwrs if dumping is enabled
    // because important information in the AER block will be cleared when the status
    // flags are cleared
    if ((errMask != 0) &&
        !(s_VerboseFlags & (1 << PexDevMgr::PEX_OFFICIAL_MODS_VERBOSITY)) &&
         (s_VerboseFlags & ((1 << PexDevMgr::PEX_DUMP_ON_TEST_ERROR) |
                            (1 << PexDevMgr::PEX_LOG_AER_DATA))))
    {
        AERLogEntry aerEntry;
        aerEntry.errMask = errMask;
        CHECK_RC(GetExtCapIdBlock(pPciDev, Pci::AER_ECI, &aerEntry.aerData));

        if (s_VerboseFlags & (1 << PexDevMgr::PEX_LOG_AER_DATA))
        {
            if ((pPciDev->AerLog.size() == s_AerLogMaxEntries) &&
                (s_OnAerLogFull == AER_LOG_DELETE_OLD))
            {
                pPciDev->AerLog.pop_front();
            }

            if (pPciDev->AerLog.size() < s_AerLogMaxEntries)
            {
                pPciDev->AerLog.push_back(aerEntry);
            }
        }

        if (s_VerboseFlags & (1 << PexDevMgr::PEX_DUMP_ON_TEST_ERROR))
        {
            Printf(Tee::PriNormal,
                   "\nDetected PEX error(s) on Bridge %04x:%02x:%02x.%x\n",
                   pPciDev->Domain, pPciDev->Bus, pPciDev->Dev, pPciDev->Func);
            Printf(Tee::PriNormal, "  Status Mask : 0x%x\n", errMask);
            PrintExtCapIdBlock(pPciDev,
                               Tee::PriNormal,
                               Pci::AER_ECI,
                               aerEntry.aerData);
        }
    }

    if (errMask)
    {
        StatusReg = StatusReg | DRF_SHIFTMASK(LW_PCI_CAP_DEVICE_CTL_ERROR_BITS);
        CHECK_RC(Write(pPciDev, Offset, StatusReg));

        ClearAerErrorStatus(pPciDev, true);
    }

    for (int i=0; i <= PexErrorCounts::UNSUP_REQ_ID; ++i)
    {
        if (errMask & (1 << i))
        {
            Cpu::AtomicAdd(&pPciDev->ErrCount[i], 1);
        }
    }

    return rc;
}

RC PexDevice::ClearAerErrorStatus(PciDevice* pPciDev, bool printErrors)
{
    RC rc;

    MASSERT(pPciDev);
    if (pPciDev->PcieAerExtCapPtr == 0)
    {
        return OK;
    }

    UINT32 aerCtrlReg;
    CHECK_RC(Read(pPciDev,
                  pPciDev->PcieAerExtCapPtr + LW_PCI_CAP_AER_CTRL_OFFSET,
                  &aerCtrlReg));
    bool multipleHeaderRecCapable = REF_VAL(LW_PCI_CAP_AER_CTRL_MUTIPLE_HEADER_REC_SUPP,
                                            aerCtrlReg);
    bool multipleHeaderRecEnabled = REF_VAL(LW_PCI_CAP_AER_CTRL_MUTIPLE_HEADER_REC_EN,
                                            aerCtrlReg);

    // Clear uncorrectable error status bits
    if (multipleHeaderRecCapable && multipleHeaderRecEnabled)
    {
        /* From PCIe Base Spec Rev 3.1, Sec 6.2.4.2 :
         * If multiple error handling is supported and enabled,
         * clear error pointed by First Error Pointer (FEP)
         * successively to get the sequence of uncorrectable errors.
         * All errors are cleared when FEP points to a reserved status bit
         */

        const UINT32 validStatusBitMask =
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_DATA_LINK_PROTOCOL)   |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_SURPRISE_DOWN)        |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_POISONED_TLP)         |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_FLOW_CTRL_PROTOCOL)   |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_COMPLETION_TIMEOUT)   |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_COMPLETER_ABORT)      |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UNEXPECTED_COMPLETION)|
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_RECEIVER_OVERFLOW)    |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_MALFORMED_TLP)        |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ECRC)                 |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UNSUPPORTED_REQ)      |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ACS_VIOLATION)        |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_UCORR_INTERNAL)       |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_MC_BLOCKED_TLP)       |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_ATOMICOP_BLOCKED)     |
           DRF_SHIFTMASK(LW_PCI_CAP_AER_UNCORR_ERR_STATUS_TLP_PREFIX_BLOCKED);

        UINT32 fepStatusBit;
        UINT32 clearCount = static_cast<UINT32>(s_AerUEStatusMaxClearCount);

        UINT32 corrErrStatusReg;
        CHECK_RC(Read(pPciDev,
                      pPciDev->PcieAerExtCapPtr + LW_PCI_CAP_AER_CORR_ERR_STATUS_OFFSET,
                      &corrErrStatusReg));

        string uePrint = "Sequence of UE error status bits on Bridge Port(Dom:Bus:Dev.Func)";
        uePrint += Utility::StrPrintf(" = (%04x:%02x:%02x.%x)\n",
                                        pPciDev->Domain,
                                        pPciDev->Bus,
                                        pPciDev->Dev,
                                        pPciDev->Func);
        if (REF_VAL(LW_PCI_CAP_AER_CORR_HEADER_LOG_OVERFLOW, corrErrStatusReg))
        {
            uePrint += "WARNING: AER Header Log Overflow\n";
        }
        while (true)
        {
            CHECK_RC(Read(pPciDev,
                          pPciDev->PcieAerExtCapPtr + LW_PCI_CAP_AER_CTRL_OFFSET,
                          &aerCtrlReg));
            fepStatusBit = REF_VAL(LW_PCI_CAP_AER_CTRL_FEP_BITS, aerCtrlReg);
            if (!(validStatusBitMask & (1U << fepStatusBit)))
            {
                break;
            }

            uePrint += Utility::StrPrintf("%u ", fepStatusBit);
            CHECK_RC(Write(pPciDev,
                           pPciDev->PcieAerExtCapPtr + LW_PCI_CAP_AER_UNCORR_ERR_STATUS_OFFSET,
                           1U << fepStatusBit));

            if (clearCount--)
            {
                // If we run into a situation where the HW keeps logging errors,
                // we have to stop from endlessly clearing errors
                break;
            }
        }

        // If any UE errors were cleared
        if ((clearCount != s_AerUEStatusMaxClearCount) &&
             printErrors && (s_VerboseFlags & (1 << PexDevMgr::PEX_LOG_AER_DATA)))
        {
            Printf(Tee::PriNormal, "%s\n", uePrint.c_str());
        }
    }
    else
    {
        // Writing 1s to all the status bits clears them
        // Reserved bits are hardwired to 0, and are unaffected by this
        CHECK_RC(Write(pPciDev,
                       pPciDev->PcieAerExtCapPtr + LW_PCI_CAP_AER_UNCORR_ERR_STATUS_OFFSET,
                       ~0U));
    }

    // Clear correctable error status bits by writing 1
    // Reserved bits are hardwired to 0, so are unaffected
    CHECK_RC(Write(pPciDev,
                   pPciDev->PcieAerExtCapPtr + LW_PCI_CAP_AER_CORR_ERR_STATUS_OFFSET,
                   ~0U));

    return rc;
}

RC PexDevice::GetOnAerLogFull(UINT32 * pOnFull)
{
    MASSERT(pOnFull);
    *pOnFull = static_cast<UINT32>(s_OnAerLogFull);
    return OK;
}

RC PexDevice::SetOnAerLogFull(UINT32 onFull)
{
    s_OnAerLogFull = static_cast<PexDevice::OnAerLogFull>(onFull);
    return OK;
}

RC PexDevice::GetAerLogMaxEntries(UINT32 * pMaxEntries)
{
    *pMaxEntries = static_cast<UINT32>(s_AerLogMaxEntries);
    return OK;
}

RC PexDevice::SetAerLogMaxEntries(UINT32 maxEntries)
{
    s_AerLogMaxEntries = static_cast<size_t>(maxEntries);
    return OK;
}

RC PexDevice::GetAerUEStatusMaxClearCount(UINT32 * pMaxCount)
{
    *pMaxCount = static_cast<UINT32>(s_AerUEStatusMaxClearCount);
    return OK;
}

RC PexDevice::SetAerUEStatusMaxClearCount(UINT32 maxCount)
{
    s_AerUEStatusMaxClearCount = static_cast<size_t>(maxCount);
    return OK;
}

RC PexDevice::GetSkipHwCounterInit(bool* pSkipInit)
{
    MASSERT(pSkipInit);
    *pSkipInit = s_SkipHwCounterInit;
    return RC::OK;
}

RC PexDevice::SetSkipHwCounterInit(bool skipInit)
{
    s_SkipHwCounterInit = skipInit;
    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Static function to retrieve error counts
//!
//! \param pErrors   : Pointer to returned errors
//! \param pPciDev   : Pci device to read
//! \param UsePoll   : true if only polling method (hw flags) should be used
//!                    when retrieveing errors
//! \param CountType : Type of errors to retrieve (all, hw counter only, hw
//!                    flag only)
//!
//! \return OK if successful, not OK otherwise
//!
RC PexDevice::GetPcieErrorCounts
(
    PexErrorCounts *pErrors,
    PciDevice      *pPciDev,
    bool            UsePoll,
    PexCounterType  CountType
)
{
    MASSERT(pErrors);
    MASSERT(pPciDev);

    RC rc;

    if (CountType != PEX_COUNTER_HW)
    {
        DevErrors devErrors;
        CHECK_RC(GetPcieErrors(pPciDev, &devErrors));

        // PLX correctable errors are always read through hw counters
        // unless forced to use the polling bit
        if ((!pPciDev->pErrorCounters) || UsePoll)
        {
            CHECK_RC(pErrors->SetCount(PexErrorCounts::CORR_ID,
                                       false,
                                       devErrors[PexErrorCounts::CORR_ID]));
        }
        CHECK_RC(pErrors->SetCount(PexErrorCounts::NON_FATAL_ID,
                                   false,
                                   devErrors[PexErrorCounts::NON_FATAL_ID]));
        CHECK_RC(pErrors->SetCount(PexErrorCounts::FATAL_ID,
                                   false,
                                   devErrors[PexErrorCounts::FATAL_ID]));
        CHECK_RC(pErrors->SetCount(PexErrorCounts::UNSUP_REQ_ID,
                                   false,
                                   devErrors[PexErrorCounts::UNSUP_REQ_ID]));
    }

    if ((CountType != PEX_COUNTER_FLAG) && !UsePoll && (pPciDev->pErrorCounters))
    {
        CHECK_RC(pPciDev->pErrorCounters->GetPcieErrorCounts(pErrors, pPciDev));
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Reset errors
//!
//! \param pPciDev   : Pci device to reset on
//! \param CountType : Error counter type to reset
//!
//! \return OK if successful, not OK otherwise
//!
RC PexDevice::ResetPcieErrors(PciDevice      *pPciDev,
                              PexCounterType  CountType)
{
    RC rc;
    if (CountType != PEX_COUNTER_HW)
    {
        UINT32 Offset = GetPcieStatusOffset(pPciDev);
        UINT32 StatusReg;

        CHECK_RC(Read(pPciDev, Offset, &StatusReg));
        StatusReg = StatusReg | DRF_SHIFTMASK(LW_PCI_CAP_DEVICE_CTL_ERROR_BITS);
        CHECK_RC(Write(pPciDev, Offset, StatusReg));

        Tasker::MutexHolder grabMutex(pPciDev->Mutex);
        for (int i=0; i <= PexErrorCounts::UNSUP_REQ_ID; ++i)
        {
            Cpu::AtomicXchg32(reinterpret_cast<volatile UINT32*>(&pPciDev->ErrCount[i]), 0U);
            pPciDev->AerLog.clear();
        }

        CHECK_RC(ClearAerErrorStatus(pPciDev, false));
    }

    if ((CountType != PEX_COUNTER_FLAG) && (pPciDev->pErrorCounters))
    {
        CHECK_RC(pPciDev->pErrorCounters->ResetPcieErrors(pPciDev));
    }

    return rc;
}

//
//! brief: GetLinkWidth given the Port and device type
//  static function
//  https://ebola/hw_projs/web/Projects/BR04/Manuals/dev_pes_xvu.ref
//  See also ../../sdk/lwpu/inc/Lwcm.h
//  and ../resman/kernel/state/lw/state.c
RC PexDevice::GetLinkWidth(UINT32 *pWidth,
                           PciDevice* pPciDev)
{
    MASSERT(pWidth);
    RC rc;
    UINT32 Offset;
    if (!pPciDev->Root && ((pPciDev->Type == BR04) || pPciDev->Type == BR03))
    {
        UINT32 StatusReg;
        Offset = LW_BR04_XVU_LINK_CTRLSTAT;
        CHECK_RC(Read(pPciDev, Offset, &StatusReg));
        // for ALL cases, the bit field for link width is 20-25
        *pWidth = DRF_VAL(_BR04_XVU, _LINK_CTRLSTAT, _NEGO_LINK_WIDTH, StatusReg);
    }
    else
    {
        UINT16 StatReg16 = 0;
        MASSERT((pPciDev->Type == CHIPSET) ||
                (pPciDev->Type == UNKNOWN_PEXDEV) ||
                (pPciDev->Type == PLX) ||
                (pPciDev->Type == QEMU_BRIDGE));
        Offset = LW_PCI_CAP_PCIE_LINK_STS + pPciDev->PcieCapPtr;

        // TODO: add Read16
        CHECK_RC(Platform::PciRead16(pPciDev->Domain,
                                     pPciDev->Bus,
                                     pPciDev->Dev,
                                     pPciDev->Func,
                                     Offset,
                                     &StatReg16));
        *pWidth = DRF_VAL(_PCI, _CAP_PCIE_LINK_STS, _LINK_WIDTH, StatReg16);
    }
    return rc;
}
//-----------------------------------------------------------------------------
Pci::PcieLinkSpeed PexDevice::GetDownStreamLinkSpeedCap(UINT32 Port)
{
    if (!m_Initialized)
    {
        PciDevice *pPciDev = GetDownStreamPort(Port);
        Pci::PcieLinkSpeed linkSpeed;
        if (ReadSpeedAndWidthCap(pPciDev, &linkSpeed, nullptr) != RC::OK)
        {
            Printf(Tee::PriNormal, "Failed to read link speed capability\n");
            return Pci::Speed2500MBPS;
        }
        return linkSpeed;
    }
    if (Port >= m_LinkSpeedCap.size())
    {
        Printf(Tee::PriNormal, "Port out of range!\n");
        return Pci::Speed2500MBPS;
    }

    return m_LinkSpeedCap[Port];
}
//-----------------------------------------------------------------------------
Pci::PcieLinkSpeed PexDevice::GetUpStreamLinkSpeedCap()
{
    PexDevice* pPexDev = GetUpStreamDev();
    MASSERT(pPexDev);
    UINT32 LwrPortNum  = GetUpStreamIndex();

    return pPexDev->GetDownStreamLinkSpeedCap(LwrPortNum);
}
//-----------------------------------------------------------------------------
UINT32 PexDevice::GetDownStreamLinkWidthCap(UINT32 Port)
{
    if (!m_Initialized)
    {
        PciDevice *pPciDev = GetDownStreamPort(Port);
        UINT32 linkWidth;
        if (ReadSpeedAndWidthCap(pPciDev, nullptr, &linkWidth) != RC::OK)
        {
            Printf(Tee::PriNormal, "Failed to read link width capability\n");
            return 1;
        }
        return linkWidth;
    }
    if (Port >= m_LinkWidthCap.size())
    {
        Printf(Tee::PriNormal, "Port out of range!\n");
        return 1;
    }

    return m_LinkWidthCap[Port];
}
//-----------------------------------------------------------------------------
UINT32 PexDevice::GetUpStreamLinkWidthCap()
{
    PexDevice* pPexDev = GetUpStreamDev();
    MASSERT(pPexDev);
    UINT32 LwrPortNum  = GetUpStreamIndex();

    return pPexDev->GetDownStreamLinkWidthCap(LwrPortNum);
}

//-----------------------------------------------------------------------------
void PexDevice::PrintInfo(Tee::Priority pri)
{
    Printf(Tee::PriNormal, "------------------------------------\n");
    string buff;
    JsonItem jsi;
    jsi.SetCategory(JsonItem::JSONLOG_PEXTOPOLOGY);
    jsi.SetJsonLimitedAllowAllFields(true);
    if (m_IsRoot)
    {
        Printf(Tee::PriNormal, "Chipset\n");
        if (JsonLogStream::IsOpen())
        {
            jsi.SetTag("pcie_chipset");
        }
    }
    else
    {
        MASSERT(m_UpStreamPort);
        Printf(pri, "Bridge %04x:%02x:%02x.%x\n",
               m_UpStreamPort->Domain, m_UpStreamPort->Bus, m_UpStreamPort->Dev, m_UpStreamPort->Func);
        if (JsonLogStream::IsOpen())
        {
            string pcieTag = Utility::StrPrintf("pcie_bridge 0x%x/0x%x", m_UpStreamPort->Domain,
                                                 m_UpStreamPort->Bus);
            jsi.SetTag(pcieTag.c_str());

            buff = Utility::StrPrintf("0x%x", m_UpStreamPort->Domain);
            jsi.SetField("upstream_domain", buff.c_str());

            buff = Utility::StrPrintf("0x%x", m_UpStreamPort->Bus);
            jsi.SetField("upstream_bus", buff.c_str());

            buff = Utility::StrPrintf("0x%x", m_UpStreamPort->Dev);
            jsi.SetField("upstream_dev", buff.c_str());

            buff = Utility::StrPrintf("0x%x", m_UpStreamPort->Func);
            jsi.SetField("upstream_func", buff.c_str());
        }

        Printf(pri, "UpStream Port %04x:%02x:%02x.%x\n",
               m_UpStreamPort->Domain,
               m_UpStreamPort->Bus,
               m_UpStreamPort->Dev,
               m_UpStreamPort->Func);
        MASSERT(m_UpStreamPort->pDevice);
        PexDevice* pPexDevAbove = (PexDevice*)m_UpStreamPort->pDevice;
        if (pPexDevAbove->IsRoot())
        {
            Printf(pri, "   connected to root\n");
            if (JsonLogStream::IsOpen())
            {
                jsi.SetField("upstream_connected_to", "root");
            }

        }
        else
        {
            Printf(pri, "   connected to bridge %02x\n",
                   pPexDevAbove->m_UpStreamPort->Bus);
            if (JsonLogStream::IsOpen())
            {
                string connectedTo = Utility::StrPrintf("pcie_bridge 0x%x/0x%x",
                                                         pPexDevAbove->m_UpStreamPort->Domain,
                                                         pPexDevAbove->m_UpStreamPort->Bus);
                jsi.SetField("upstream_connected_to", connectedTo.c_str());
            }
        }
        UINT32 Lanes = 0;
        GetLinkWidth(&Lanes, m_UpStreamPort);
        Printf(pri, "   Speed: %d Mbps | Lanes: %d\n",
               GetUpStreamSpeed(), Lanes);
        if (JsonLogStream::IsOpen())
        {
            jsi.SetField("upstream_speed", GetUpStreamSpeed());
            jsi.SetField("upstream_lanes", Lanes);
        }
        UINT32 LwrrSetting;
        GetUpStreamAspmState(&LwrrSetting);
        Printf(pri, "   ASPM: State=0x%1x\n", LwrrSetting);
        if (JsonLogStream::IsOpen())
        {
            buff = Utility::StrPrintf("0x%1x", LwrrSetting);
            jsi.SetField("upstream_aspm_state", buff.c_str());
        }
        if (HasAspmL1Substates())
        {
            GetUpStreamAspmL1SubState(&LwrrSetting);
            Printf(pri, "   ASPM L1SS: State=0x%1x\n", LwrrSetting);
            if (JsonLogStream::IsOpen())
            {
                buff = Utility::StrPrintf("0x%1x", LwrrSetting);
                jsi.SetField("upstream_aspm_l1ss_state", buff.c_str());
            }
        }
        else
        {
            Printf(pri, "   ASPM L1SS: Not supported\n");
            if (JsonLogStream::IsOpen())
            {
                jsi.SetField("upstream_aspm_l1ss_state", "not_supported");
            }
        }

    }
    JsArray downStreamPorts;
    for (UINT32 i = 0; i < m_DownStreamPort.size(); i++)
    {
        JsonItem downStreamJsi;
        MASSERT(m_DownStreamPort[i]);
        Printf(pri, "DownStream Port %04x:%02x:%02x.%x\n",
               m_DownStreamPort[i]->Domain,
               m_DownStreamPort[i]->Bus,
               m_DownStreamPort[i]->Dev,
               m_DownStreamPort[i]->Func);

        if (JsonLogStream::IsOpen())
        {
            // Can't use to_string because Android doesn't support it
            string tagName = Utility::StrPrintf("downstream_%d", m_DownStreamPort[i]->Domain);
            downStreamJsi.SetTag(tagName.c_str());

            buff = Utility::StrPrintf("0x%x", m_DownStreamPort[i]->Domain);
            downStreamJsi.SetField("domain", buff.c_str());

            buff = Utility::StrPrintf("0x%x", m_DownStreamPort[i]->Bus);
            downStreamJsi.SetField("bus", buff.c_str());

            buff = Utility::StrPrintf("0x%x", m_DownStreamPort[i]->Dev);
            downStreamJsi.SetField("dev", buff.c_str());

            buff = Utility::StrPrintf("0x%x", m_DownStreamPort[i]->Func);
            downStreamJsi.SetField("func", buff.c_str());
        }

        // these can be printed even if the links are empty
        Printf(pri, "   MaxSpeed = %d\n", m_LinkSpeedCap[i]);
        Printf(pri, "   MaxWidth = %d\n", m_LinkWidthCap[i]);

        UINT32 Lanes = 0;
        GetLinkWidth(&Lanes, m_DownStreamPort[i]);
        Printf(pri, "   Lanes: %d\n", Lanes);

        if (JsonLogStream::IsOpen())
        {
            downStreamJsi.SetField("maxspeed", m_LinkSpeedCap[i]);
            downStreamJsi.SetField("maxwidth", m_LinkWidthCap[i]);
            downStreamJsi.SetField("lanes", Lanes);
        }

        UINT32 LwrrSetting;
        UINT32 Cap = GetAspmCap();
        GetDownStreamAspmState(&LwrrSetting, i);
        Printf(pri, "   ASPM: Cap=0x%1x|State=0x%1x\n", Cap, LwrrSetting);
        if (JsonLogStream::IsOpen())
        {
            buff = Utility::StrPrintf("0x%1x", Cap);
            downStreamJsi.SetField("aspm_state", buff.c_str());

            buff = Utility::StrPrintf("0x%1x", LwrrSetting);
            downStreamJsi.SetField("aspm_cap", buff.c_str());
        }
        if (HasAspmL1Substates())
        {
            Cap = GetAspmL1SSCap();
            GetDownStreamAspmL1SubState(&LwrrSetting, i);
            Printf(pri, "   ASPM L1SS: Cap=0x%1x|State=0x%1x\n", Cap, LwrrSetting);
            if (JsonLogStream::IsOpen())
            {
                buff = Utility::StrPrintf("0x%1x", Cap);
                downStreamJsi.SetField("aspm_l1ss_state", buff.c_str());

                buff = Utility::StrPrintf("0x%1x", LwrrSetting);
                downStreamJsi.SetField("aspm_l1ss_cap", buff.c_str());
            }
        }
        else
        {
            Printf(pri, "   ASPM L1SS: Not supported\n");
            downStreamJsi.SetField("aspm_l1ss_state", "not_supported");
        }

        Pci::DpcState lwrDpcState = Pci::DPC_STATE_DISABLED;
        if (GetDpcCap() != Pci::DPC_CAP_NONE)
            GetDownStreamDpcState(&lwrDpcState, i);
        Printf(pri, "   DPC: Cap=%s|State=%s\n",
               Pci::DpcCapMaskToString(GetDpcCap()).c_str(),
               Pci::DpcStateToString(lwrDpcState).c_str());

        if (JsonLogStream::IsOpen())
        {
            downStreamJsi.SetField("dpc_cap", Pci::DpcCapMaskToString(GetDpcCap()).c_str());
            downStreamJsi.SetField("dpc_state",  Pci::DpcStateToString(lwrDpcState).c_str());
        }

        if (!m_DownStreamPort[i]->pDevice)
        {
            Printf(pri, "   Not connected to anything\n");
            if (JsonLogStream::IsOpen())
            {
                downStreamJsi.SetField("connected_to", "None");
            }
            continue;
        }
        if (m_DownStreamPort[i]->IsPexDevice)
        {
            PexDevice* pBridgeBelow = (PexDevice*)m_DownStreamPort[i]->pDevice;
            Printf(pri, "   connected to bridge %04x:%02x:%02x.%x\n",
                   pBridgeBelow->m_UpStreamPort->Domain,
                   pBridgeBelow->m_UpStreamPort->Bus,
                   pBridgeBelow->m_UpStreamPort->Dev,
                   pBridgeBelow->m_UpStreamPort->Func);

            if (JsonLogStream::IsOpen())
            {
                buff = Utility::StrPrintf("pcie_bridge 0x%x/0x%x",
                                                     pBridgeBelow->m_UpStreamPort->Domain,
                                                     pBridgeBelow->m_UpStreamPort->Bus);
                downStreamJsi.SetField("connected_to", buff.c_str());
            }
        }
        else
        {
            TestDevice* pDevBelow = (TestDevice*)m_DownStreamPort[i]->pDevice;
            Printf(pri, "   connected to %s\n",
                   pDevBelow->GetName().c_str());
            if (JsonLogStream::IsOpen())
            {
                downStreamJsi.SetField("connected_to", pDevBelow->GetName().c_str());
            }
        }
        downStreamPorts.push_back(OBJECT_TO_JSVAL(downStreamJsi.GetObj()));
    }
    jsi.SetField("downstream_nodes", &downStreamPorts);
    Printf(pri, "Depth from root = %d\n", m_Depth);
    if (JsonLogStream::IsOpen())
    {
        jsi.SetField("depth_from_root", m_Depth);
        jsi.WriteToLogfile();
    }
}

RC PexDevice::PrintDownStreamExtCapIdBlock
(
    Tee::Priority      Pri,
    Pci::ExtendedCapID CapId,
    UINT32             Port
)
{
    if (Port >= m_DownStreamPort.size())
    {
        Printf(Tee::PriError,
               "PrintDownStreamExtCapIdBlock : Unknown downstream port %02x\n",
               Port);
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    vector<UINT32> capData;
    CHECK_RC(GetExtCapIdBlock(m_DownStreamPort[Port], CapId, &capData));
    PrintExtCapIdBlock(m_DownStreamPort[Port], Pri, CapId, capData);
    return rc;
}

RC PexDevice::PrintUpStreamExtCapIdBlock(Tee::Priority Pri, Pci::ExtendedCapID CapId)
{
    if (m_IsRoot)
    {
        Printf(Pri,
               "Chipset : No upstream port to print extended capabilities\n");
        return OK;
    }

    RC rc;
    vector<UINT32> capData;
    CHECK_RC(GetExtCapIdBlock(m_UpStreamPort, CapId, &capData));
    PrintExtCapIdBlock(m_UpStreamPort, Pri, CapId, capData);
    return rc;
}

//-----------------------------------------------------------------------------
RC PexDevice::GetRom(void *pInBuffer, UINT32 BufferSize)
{
    MASSERT(pInBuffer);

    RC rc;

    if (m_Type != BR04)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (BufferSize > BR04_ROMSIZE)
    {
        return RC::BAD_PARAMETER;
    }

    LW2080_CTRL_BUS_HWBC_GET_UPSTREAM_BAR0_PARAMS Params = { };
    MASSERT(m_UpStreamPort);
    Params.primaryBus = m_UpStreamPort->Bus;
    LwRmPtr pLwRm;
    LwRm::Handle LwRmHandle = GetPexDevHandle();
    CHECK_RC(pLwRm->Control(LwRmHandle,
                           LW2080_CTRL_CMD_BUS_HWBC_GET_UPSTREAM_BAR0,
                           &Params,
                           sizeof(Params)));

    void *pBar0 = NULL;
    const UINT32 OFFSET_TO_ROM = 0x1000;
    CHECK_RC(Platform::MapDeviceMemory(&pBar0,
                                       Params.physBAR0 + OFFSET_TO_ROM,
                                       BR04_ROMSIZE,
                                       Memory::UC,
                                       Memory::Readable));

    // Note : Platform::MemCopy() defaults to use a SSE copy if all GPUs
    // in the system support SSE copies.  However it appears (empirically) that
    // SSE copies from a BR04 device do not work, so force the BR04 to use the
    // the 4-byte mem copy instead.
    Platform::MemCopy4Byte(pInBuffer, pBar0, BufferSize);

    Platform::UnMapDeviceMemory(pBar0);

    return rc;
}
/* static */ RC PexDevice::Read
(
    PciDevice* pPciDev,
    UINT32     Offset,
    UINT32*    pValue
)
{
    RC rc;
    if (pPciDev->pBar0 != NULL)
    {
        *pValue = MEM_RD32((UINT08 *)pPciDev->pBar0 + Offset);
    }
    else
    {
        CHECK_RC(Platform::PciRead32(pPciDev->Domain,
                                     pPciDev->Bus,
                                     pPciDev->Dev,
                                     pPciDev->Func,
                                     Offset,
                                     pValue));
    }

    return rc;
}

/* static */ RC PexDevice::Write
(
    PciDevice* pPciDev,
    UINT32     Offset,
    UINT32     Value
)
{
    RC rc;
    if (pPciDev->pBar0 != NULL)
    {
        MEM_WR32((UINT08 *)pPciDev->pBar0 + Offset, Value);
    }
    else
    {
        CHECK_RC(Platform::PciWrite32(pPciDev->Domain,
                                      pPciDev->Bus,
                                      pPciDev->Dev,
                                      pPciDev->Func,
                                      Offset,
                                      Value));
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* static */ void PexDevice::PrintExtCapIdBlock
(
    PciDevice* pPciDev,
    Tee::Priority Pri,
    Pci::ExtendedCapID CapId,
    const vector<UINT32> & capData
)
{
    string extCapStr = "------------------------------------\n";

    MASSERT(pPciDev);

    switch (CapId)
    {
        case Pci::AER_ECI:
            extCapStr += "  Advanced Error Reporting";
            break;
        case Pci::SEC_PCIE_ECI:
            extCapStr += "  Secondary PCI Express";
            break;
        case Pci::DPC_ECI:
            extCapStr += "  Downstream Port Containment";
            break;
        default:
            extCapStr += Utility::StrPrintf("  Extended PCI Capability block %d", CapId);
    }

    extCapStr += " Extended Capability Block\n";
    extCapStr +=
        Utility::StrPrintf("    Bridge Port %04x:%02x:%02x.%x\n",
                           pPciDev->Domain,
                           pPciDev->Bus,
                           pPciDev->Dev,
                           pPciDev->Func);

    if (capData.empty())
    {
        Printf(Pri, "%s      ****** NOT FOUND ******\n", extCapStr.c_str());
        return;
    }

    for (size_t ii = 0; ii < capData.size(); ++ii)
    {
        if ((ii % 4) == 0)
        {
            if (ii != 0)
                extCapStr += "\n";
            extCapStr += Utility::StrPrintf("      0x%02x : ", static_cast<UINT32>(ii));
        }
        extCapStr += Utility::StrPrintf("0x%08x ", capData[ii]);
    }
    Printf(Pri, "%s\n", extCapStr.c_str());
}

//-----------------------------------------------------------------------------
/* static */ RC PexDevice::GetExtCapIdBlock
(
    PciDevice* pPciDev,
    Pci::ExtendedCapID CapId,
    vector<UINT32> *pExtCapData
)
{
    pExtCapData->clear();

    UINT16 capOffset = 0;
    UINT16 capSize   = 0;
    if (CapId == Pci::AER_ECI)
    {
         if (pPciDev->PcieAerExtCapPtr == 0)
             return OK;
         capOffset = pPciDev->PcieAerExtCapPtr;
         capSize   = pPciDev->PcieAerExtCapSize;
    }
    else if (OK != Pci::GetExtendedCapInfo(pPciDev->Domain,
                                      pPciDev->Bus,
                                      pPciDev->Dev,
                                      pPciDev->Func,
                                      CapId,
                                      pPciDev->PcieCapPtr,
                                      &capOffset,
                                      &capSize))
    {
        // If getting the cap offset/size fails simply return no data
        return OK;
    }

    RC rc;
    UINT32 data;
    for (UINT16 lwrOffset = 0; lwrOffset < capSize; lwrOffset += 4)
    {
        CHECK_RC(Read(pPciDev, capOffset + lwrOffset, &data));
        pExtCapData->push_back(data);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC PlxErrorCounters::Initialize(PexDevice* pPexDevice)
{
    RC rc;

    // all the port registers are mapped into the upstream port's BAR0
    PexDevice::PciDevice* pUpstreamDevice = pPexDevice->GetUpStreamPort();
    PHYSADDR barBaseAddress = 0;
    UINT64   barSize = 0;
    CHECK_RC(Pci::GetBarInfo(pUpstreamDevice->Domain,
                             pUpstreamDevice->Bus,
                             pUpstreamDevice->Dev,
                             pUpstreamDevice->Func,
                             0,
                             &barBaseAddress,
                             &barSize));

    if (!barBaseAddress)
    {
        Printf(Tee::PriLow,
               "BAR0 unassigned for pex bridge %04x:%02x:%02x.%x. Unable to use pcie error counter registers.\n",
               pUpstreamDevice->Domain,
               pUpstreamDevice->Bus,
               pUpstreamDevice->Dev,
               pUpstreamDevice->Func);
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (s_Plx880XXDevVendorIds.count(pUpstreamDevice->VendorDeviceId))
    {
        // ATLAS family
        m_RcvrErrOffset = PLX880XX_RCVR_ERR_COUNT_OFFSET;
        m_PortRegisterOffset = PEX880XX_PORT_REGISTER_OFFSET;
        m_PortRegisterStride = PLX_PORT_REGISTER_STRIDE;
        m_HasPortSelect = true;
        m_HasInternalUpstreamPorts = true;
        m_PortsPerStation = 16;
        m_StationCount = 6;
    }
    else if (s_PlxDevVendorIds.count(pUpstreamDevice->VendorDeviceId))
    {
        m_RcvrErrOffset = PexDevice::PLX_RCVR_ERR_COUNT_OFFSET;
        m_PortRegisterOffset = PEX87XX_PORT_REGISTER_OFFSET;
        m_PortRegisterStride = PLX_PORT_REGISTER_STRIDE;
        m_HasPortSelect = false;
        m_HasInternalUpstreamPorts = false;

        switch (pUpstreamDevice->VendorDeviceId)
        {
        // DRACO 1 family
        case 0x874710B5:
        case 0x874910B5:
        // DRACO 2 family
        case 0x872510B5:
            m_PortsPerStation = 8;
            m_StationCount = 3;
            break;
        // CAPELLA 1 family
        case 0x876410B5:
        case 0x878010B5:
        case 0x879610B5:
        // CAPELLA 2 family
        case 0x974910B5:
        case 0x976510B5:
        case 0x978110B5:
        case 0x979710B5:
            m_PortsPerStation = 4;
            m_StationCount = 6;
            break;
        default:
            Printf(Tee::PriError,
                   "Unsupported pex bridge %04x:%02x:%02x.%x vendor=%04x devid=%04x.  "
                   "Unable to determine max ports per station.\n",
                   pUpstreamDevice->Domain,
                   pUpstreamDevice->Bus,
                   pUpstreamDevice->Dev,
                   pUpstreamDevice->Func,
                   pUpstreamDevice->VendorDeviceId & 0xFFFFU,
                   pUpstreamDevice->VendorDeviceId >> 16);
            return RC::UNSUPPORTED_FUNCTION;
        }
    }
    else
    {
        Printf(Tee::PriLow,
               "Unsupported pex bridge %04x:%02x:%02x.%x. Unable to use pcie error counter registers.\n",
                pUpstreamDevice->Domain,
                pUpstreamDevice->Bus,
                pUpstreamDevice->Dev,
                pUpstreamDevice->Func);
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(Platform::MapDeviceMemory(&m_pBar0,
                                       barBaseAddress,
                                       barSize,
                                       Memory::UC,
                                       Memory::ReadWrite));

    // get the port number for each port
    vector<PexDevice::PciDevice*> validPorts;
    list<PexDevice*> pexDevices;
    pexDevices.push_back(pPexDevice);
    while (!pexDevices.empty())
    {
        PexDevice* pLwrPexDevice = pexDevices.front();
        pexDevices.pop_front();

        vector<PexDevice::PciDevice*> ports;
        ports.push_back(pLwrPexDevice->GetUpStreamPort());
        for (UINT32 i = 0; i < pLwrPexDevice->GetNumDPorts(); i++)
        {
            ports.push_back(pLwrPexDevice->GetDownStreamPort(i));
        }

        // initialize the port numbers
        for (PexDevice::PciDevice* pPciDev : ports)
        {
            UINT32 value = 0;
            CHECK_RC(Platform::PciRead32(pPciDev->Domain,
                                         pPciDev->Bus,
                                         pPciDev->Dev,
                                         pPciDev->Func,
                                         LW_PCI_CAP_PCIE_LINK_CAP +
                                         pPciDev->PcieCapPtr,
                                         &value));
            pPciDev->PortNum = REF_VAL(LW_PCI_CAP_PCIE_LINK_CAP_LINK_PORT_NUMBER, value);

            if (pPciDev->PortNum < m_StationCount * m_PortsPerStation)
            {
                validPorts.push_back(pPciDev);
            }
        }

        // check if we need to initialize the downstream device
        // Atlas devices have multiple layers of ports, which mods detects as multiple pex devices.
        // However, the ports for all those pex devices use the same upstream port's BAR0.
        if (m_HasInternalUpstreamPorts)
        {
            for (UINT32 i = 0; i < pLwrPexDevice->GetNumDPorts(); i++)
            {
                PexDevice::PciDevice* pPciDev  = pLwrPexDevice->GetDownStreamPort(i);

                // if the port number is valid, the port is external.
                if (pPciDev->PortNum < m_StationCount * m_PortsPerStation)
                    continue;

                PexDevice*  pNextPexDevice = pLwrPexDevice->GetDownStreamPexDevice(i);
                if (pNextPexDevice == NULL)
                    continue;

                // sanity check that the vendor/devid matches
                PexDevice::PciDevice* pNextUpstreamDevice = pNextPexDevice->GetUpStreamPort();
                if (pNextUpstreamDevice->VendorDeviceId != pUpstreamDevice->VendorDeviceId)
                    continue;

                pexDevices.push_back(pNextPexDevice);
            }
        }
    }

    m_initialized = true;

    // sanity check that we can read port registers via BAR0
    UINT32 vendorDeviceId;
    CHECK_RC(ReadPort32(pUpstreamDevice->PortNum, 0, &vendorDeviceId));
    if (!vendorDeviceId)
    {
        Printf(Tee::PriLow,
                "Unable to read port registers for pex bridge %04x:%02x:%02x.%x.  "
                "Unable to use pcie error counter registers.\n",
                pUpstreamDevice->Domain,
                pUpstreamDevice->Bus,
                pUpstreamDevice->Dev,
                pUpstreamDevice->Func);

        Platform::UnMapDeviceMemory(m_pBar0);
        m_initialized = false;
        return RC::UNSUPPORTED_FUNCTION;
    }

    // assign this error counter object to the valid ports
    for (PexDevice::PciDevice* pPciDev : validPorts)
    {
        pPciDev->pErrorCounters = this;
    }

    return rc;
}

void PlxErrorCounters::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    if (m_pBar0)
    {
        Platform::UnMapDeviceMemory(m_pBar0);
    }

    m_initialized = false;
}

RC PlxErrorCounters::GetPcieErrorCounts(PexErrorCounts *pErrors, PexDevice::PciDevice *pPciDev)
{
    RC rc;

    if (!m_initialized)
    {
        Printf(Tee::PriError, "PlxErrorCounters for %04x:%02x:%02x.%x not initialized.\n",
                            pPciDev->Domain,
                            pPciDev->Bus,
                            pPciDev->Dev,
                            pPciDev->Func);
        return RC::UNSUPPORTED_FUNCTION;
    }

    // PLX bridge devices support hardware counters for correctable errors
    // When reading counters non-flag counters on a PLX device, read the
    // necessary hardware counters and sum them
    UINT08 rcvrErrs;
    UINT32 badTlp, badDllp;

    UINT08 stationPort;
    UINT08 stationPortZero;
    GetStationPort(pPciDev->PortNum, &stationPort, &stationPortZero);

    if (m_HasPortSelect) // PEX88000
    {
        // Receiver Error Counter Register
        //     bits 31:31 - write enable
        //     bits 30:28 - reserved
        //     bits 27:24 - port select
        //     bits 23: 8 - reserved
        //     bits  7: 0 - port error counter
        CHECK_RC(WritePort32(stationPortZero, m_RcvrErrOffset, 0x80000000 | (stationPort << 24)));
        CHECK_RC(ReadPort08(stationPortZero, m_RcvrErrOffset, &rcvrErrs));
    }
    else // PEX87XX and PEX97XX
    {
        // Receiver Error Counter Register0
        //     bits 31:24 - port 3 error counter
        //     bits 23:16 - port 2 error counter
        //     bits 15: 8 - port 1 error counter
        //     bits  7: 0 - port 0 error counter
        //
        // Receiver Error Counter Register1
        //     bits 31:24 -
        //     bits 23:16 -
        //     bits 15: 8 - port 5 error counter
        //     bits  7: 0 - port 4 error counter

        MASSERT(stationPort < 8);
        CHECK_RC(ReadPort08(stationPortZero, m_RcvrErrOffset + stationPort, &rcvrErrs));
    }

    CHECK_RC(ReadPort32(pPciDev->PortNum, PexDevice::PLX_BAD_TLP_COUNT_OFFSET, &badTlp));
    CHECK_RC(ReadPort32(pPciDev->PortNum, PexDevice::PLX_BAD_DLLP_COUNT_OFFSET, &badDllp));

    CHECK_RC(ResetPcieErrors(pPciDev));

    CHECK_RC(pErrors->SetCount(PexErrorCounts::CORR_ID, true,
                               rcvrErrs + badTlp + badDllp));

    return rc;
}

RC PlxErrorCounters::ResetPcieErrors(PexDevice::PciDevice *pPciDev)
{
    RC rc;

    if (!m_initialized)
    {
        Printf(Tee::PriError, "PlxErrorCounters for %04x:%02x:%02x.%x not initialized.\n",
                            pPciDev->Domain,
                            pPciDev->Bus,
                            pPciDev->Dev,
                            pPciDev->Func);
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT08 stationPort;
    UINT08 stationPortZero;
    GetStationPort(pPciDev->PortNum, &stationPort, &stationPortZero);

    if (m_HasPortSelect) // PEX88000
    {
        // Receiver Error Counter Register
        //     bits 31:31 - write enable
        //     bits 30:28 - reserved
        //     bits 27:24 - port select
        //     bits 23: 8 - reserved
        //     bits  7: 0 - port error counter

        // Receiver errors are cleared by writing all 1's to the counter
        CHECK_RC(WritePort32(stationPortZero, m_RcvrErrOffset, 0x80000000 | (stationPort << 24) | 0xFF));
    }
    else // PEX87XX and PEX97XX
    {
        // Receiver Error Counter Register
        //     bits 31:24 - port 3 error counter
        //     bits 23:16 - port 2 error counter
        //     bits 15: 8 - port 1 error counter
        //     bits  7: 0 - port 0 error counter
        //
        // Receiver Error Counter Register1
        //     bits 31:24 -
        //     bits 23:16 -
        //     bits 15: 8 - port 5 error counter
        //     bits  7: 0 - port 4 error counter

        MASSERT(stationPort < 8);

        // Receiver errors are cleared by writing all 1's
        CHECK_RC(WritePort08(stationPortZero, m_RcvrErrOffset + stationPort, 0xFF));
    }

    CHECK_RC(WritePort32(pPciDev->PortNum, PexDevice::PLX_BAD_TLP_COUNT_OFFSET, 0));
    CHECK_RC(WritePort32(pPciDev->PortNum, PexDevice::PLX_BAD_DLLP_COUNT_OFFSET, 0));

    return rc;
}

void PlxErrorCounters::GetStationPort(UINT08 port, UINT08* stationPort, UINT08* stationPortZero)
{
    // Ports are grouped by "station". Station registers are accessed
    // through port0 of the station.

    // determine the port's station-relative port number
    if (stationPort)
        *stationPort = port % m_PortsPerStation;

    // determine the chip-relative port number of the station's port0
    if (stationPortZero)
        *stationPortZero = (port / m_PortsPerStation) * m_PortsPerStation;
}

RC PlxErrorCounters::ReadPort32(UINT08 port, UINT32 offset, UINT32 *pValue)
{
    RC rc;

    if (!m_initialized)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT64 portOffset = m_PortRegisterOffset + (port * m_PortRegisterStride);
    *pValue = MEM_RD32(static_cast<UINT08*>(m_pBar0) + portOffset + offset);

    return rc;
}

RC PlxErrorCounters::WritePort32(UINT08 port, UINT32 offset, UINT32 value)
{
    RC rc;

    if (!m_initialized)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT64 portOffset = m_PortRegisterOffset + (port * m_PortRegisterStride);
    MEM_WR32(static_cast<UINT08*>(m_pBar0) + portOffset + offset, value);

    return rc;
}

RC PlxErrorCounters::ReadPort08(UINT08 port, UINT32 offset, UINT08 *pValue)
{
    RC rc;

    if (!m_initialized)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT64 portOffset = m_PortRegisterOffset + (port * m_PortRegisterStride);
    *pValue = MEM_RD08(static_cast<UINT08*>(m_pBar0) + portOffset + offset);

    return rc;
}

RC PlxErrorCounters::WritePort08(UINT08 port, UINT32 offset, UINT08 value)
{
    RC rc;

    if (!m_initialized)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    UINT64 portOffset = m_PortRegisterOffset + (port * m_PortRegisterStride);
    MEM_WR08(static_cast<UINT08*>(m_pBar0) + portOffset + offset, value);

    return rc;
}

//-----------------------------------------------------------------------------
// PexDevMgr
//-----------------------------------------------------------------------------

PexDevMgr::PexDevMgr()
: m_ErrorCollectorMutex(Tasker::AllocMutex("PexDevMgr error collector",
                        (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM) ? Tasker::mtxUnchecked
                                                                      : Tasker::mtxLast))
{
    MASSERT(m_ErrorCollectorMutex);
}

PexDevMgr::~PexDevMgr()
{
    StopErrorCollector();
    Tasker::FreeMutex(m_ErrorCollectorMutex);
}

RC PexDevMgr::InitializeAll()
{
    RC rc;
    if (!m_Found)
    {
        Printf(Tee::PriError,
               "Cannot initialize Pex devices before calling "
               "FindDevices()!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_Initialized)
    {
        Printf(Tee::PriNormal, "PexDevMgr already initialized.  Some Pex "
                               "device initializations may do nothing.\n");
    }

    for (auto& pexDev : m_Bridges)
    {
        CHECK_RC(pexDev.Initialize());
    }

    CHECK_RC(StartErrorCollector());

    m_Initialized = true;

    return rc;
}

RC PexDevMgr::ShutdownAll()
{
    StickyRC rc;
    if (!m_Initialized)
    {
        Printf(Tee::PriNormal,
               "PexDevMgr not initialized.  "
               "Bridge device shutdowns will likely do nothing.\n");
    }

    rc = StopErrorCollector();

    for (auto& pexDev : m_Bridges)
    {
        rc = pexDev.Shutdown();
    }

    m_Initialized = false;
    return rc;
}

RC PexDevMgr::StartErrorCollector()
{
    if (!m_ErrorCollectorEvent)
    {
        m_ErrorCollectorEvent = Tasker::AllocEvent("PexDevMgr error collector", false);
    }
    if (!m_ErrorCollectorEvent)
    {
        return RC::SOFTWARE_ERROR;
    }

    Cpu::AtomicWrite(&m_ErrorCollectorEnd, 0);
    Cpu::AtomicWrite(&m_ErrorCollectorRC,  OK);

    MASSERT(m_ErrorCollectorThread == Tasker::NULL_THREAD);
    m_ErrorCollectorThread = Tasker::CreateThread(
            [](void* that) { static_cast<PexDevMgr*>(that)->ErrorCollectorThread(); },
            this, 0, "PexDevMgr error collector");
    if (m_ErrorCollectorThread == Tasker::NULL_THREAD)
    {
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC PexDevMgr::StopErrorCollector()
{
    Cpu::AtomicWrite(&m_ErrorCollectorEnd, 1);

    if (m_ErrorCollectorEvent)
    {
        Tasker::SetEvent(m_ErrorCollectorEvent);
    }

    if (m_ErrorCollectorThread != Tasker::NULL_THREAD)
    {
        Tasker::Join(m_ErrorCollectorThread);
        m_ErrorCollectorThread = Tasker::NULL_THREAD;
    }

    if (m_ErrorCollectorEvent)
    {
        Tasker::FreeEvent(m_ErrorCollectorEvent);
        m_ErrorCollectorEvent = nullptr;
    }

    return Cpu::AtomicXchg32(reinterpret_cast<volatile UINT32*>(&m_ErrorCollectorRC), OK);
}

void PexDevMgr::ErrorCollectorThread()
{
    Tasker::DetachThread detach;

    const UINT32 timeoutMs = m_ErrorPollingPeriodMs;

    while (Cpu::AtomicRead(&m_ErrorCollectorEnd) == 0)
    {
        MASSERT(m_ErrorCollectorEvent);
        const RC rc = Tasker::WaitOnEvent(m_ErrorCollectorEvent, timeoutMs);
        if (rc != OK && rc != RC::TIMEOUT_ERROR)
        {
            Cpu::AtomicWrite(&m_ErrorCollectorRC, rc.Get());
            Printf(Tee::PriError, "Event wait failed, "
                                  "PEX error collector thread is exiting\n");
            return;
        }

        if (Cpu::AtomicRead(&m_ErrorCollectorPause) != 0)
        {
            continue;
        }

        for (auto& port : m_BridgePorts)
        {
            if (port.second.Located)
            {
                const RC rc = PexDevice::ReadPcieErrors(&port.second);
                if (rc != OK)
                {
                    Cpu::AtomicWrite(&m_ErrorCollectorRC, rc.Get());
                    Printf(Tee::PriError, "Failed to read PEX errors, "
                                          "PEX error collector thread is exiting\n");
                    return;
                }
            }
        }
    }
}

UINT32 PexDevMgr::NumDevices()
{
    return static_cast<UINT32>(m_Bridges.size());
}

UINT32 PexDevMgr::ChipsetASPMState()
{
    auto pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    pTestDeviceMgr->GetDevice(0, &pTestDevice);
    MASSERT(pTestDevice.get() != nullptr);

    PexDevice* pLwrDev = nullptr;
    UINT32 portId = ~0U;
    if (OK != pTestDevice->GetInterface<Pcie>()->GetUpStreamInfo(&pLwrDev, &portId))
        return Pci::ASPM_UNKNOWN_STATE;

    // Find the root port attached to the GPU
    while (pLwrDev && !pLwrDev->IsRoot())
    {
        pLwrDev = pLwrDev->GetUpStreamDev();
    }

    UINT32 aspm = Pci::ASPM_UNKNOWN_STATE;
    if (pLwrDev &&
        OK == pLwrDev->GetDownStreamAspmState(&aspm, 0))
    {
        return aspm;
    }

    return Pci::ASPM_UNKNOWN_STATE;
}

bool PexDevMgr::ChipsetHasASPML1Substates()
{
    auto pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    pTestDeviceMgr->GetDevice(0, &pTestDevice);
    MASSERT(pTestDevice.get() != nullptr);

    PexDevice* pLwrDev = nullptr;
    UINT32 portId = ~0U;
    if (OK != pTestDevice->GetInterface<Pcie>()->GetUpStreamInfo(&pLwrDev, &portId))
        return false;

    // Find the root port attached to the GPU
    while (pLwrDev && !pLwrDev->IsRoot())
    {
        pLwrDev = pLwrDev->GetUpStreamDev();
    }

    // Assume that the all root ports support ASPM L1SS
    if (nullptr != pLwrDev)
        return pLwrDev->HasAspmL1Substates();

    return false;
}

UINT32 PexDevMgr::ChipsetASPML1SubState()
{
    auto pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    pTestDeviceMgr->GetDevice(0, &pTestDevice);
    MASSERT(pTestDevice.get() != nullptr);

    PexDevice* pLwrDev = nullptr;
    UINT32 portId = ~0U;
    if (OK != pTestDevice->GetInterface<Pcie>()->GetUpStreamInfo(&pLwrDev, &portId))
        return Pci::PM_UNKNOWN_SUB_STATE;

    // Find the root port attached to the GPU
    while (pLwrDev && !pLwrDev->IsRoot())
    {
        pLwrDev = pLwrDev->GetUpStreamDev();
    }

    // Report the chipset state from the current Gpu
    UINT32 l1ss;
    if ((nullptr != pLwrDev) &&
        (OK == pLwrDev->GetDownStreamAspmL1SubState(&l1ss, 0)))
    {
        return l1ss;
    }
    return Pci::PM_UNKNOWN_SUB_STATE;
}

//------------------------------------------------------------------------------
// Return true if this downstream port requires unique link polling algorithm.
// false otherwise.
bool PexDevMgr::RequiresUniqueLinkPolling(UINT32 vendorDeviceId)
{
    // Note this list may grow, it is determined imperically.
    static const UINT32 deviceIds[] =
    {
        // downstream port  chipset
        0x01018086,         // IntelP67 (verified)
        0x01518086,         // IntelZ77A-GD55 (verified)
        0x0C018086,         // Haswell
        0x19018086,         // Skylake-H (verified)
        0x16018086,         // Broadwell (verified)
    };
    for (const auto devId : deviceIds)
    {
        if (vendorDeviceId == devId)
            return true;
    }
    return false;
}

// PCI Config registers from: BR04/dev_pes_xvd.ref
// #define LW_PES_XVD_BUS                                   0x00000018 /* RW-4R */
// #define LW_PES_XVD_BUS_PRI_NUMBER                               7:0 /* RWIUF */
// #define LW_PES_XVD_BUS_PRI_NUMBER_INIT                   0x00000000 /* RWI-V */
// #define LW_PES_XVD_BUS_SEC_NUMBER                              15:8 /* RWIUF */
// #define LW_PES_XVD_BUS_SEC_NUMBER_INIT                   0x00000000 /* RWI-V */
// #define LW_PES_XVD_BUS_SUB_NUMBER                             23:16 /* RWIUF */
// #define LW_PES_XVD_BUS_SUB_NUMBER_INIT                   0x00000000 /* RWI-V */
RC PexDevMgr::FindDevices()
{
    RC rc;
    // Step 1: Find all the bridge ports:
    const UINT32 BR04_DEV_VENDOR_ID = 0x5B010DE;
    const UINT32 BR03_DEV_VENDOR_ID = 0x01B310DE;
    const UINT32 QEMU_BRIDGE_DEV_VENDOR_ID  = 0x00011b36;
    const INT32 VENDOR_ID_REG = 0x0;
    const INT32 BUS_NUM_REG = 0x18;
    INT32 Index = 0;
    UINT32 Domain;
    UINT32 Bus;
    UINT32 Device;
    UINT32 Function;

    if (m_Found)
    {
        Printf(Tee::PriLow, "Already found bridge devices! Doing nothing.\n");
        return OK;
    }

    if (Platform::IsTegra() && CheetAh::IsInitialized() &&
        !CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_PCIE))
    {
        Printf(Tee::PriLow, "Skipping PexDevMgr initialization, PCI bus not available\n");
        m_Found = true;
        return OK;
    }

    if (!Platform::HasClientSideResman())
    {
        Printf(Tee::PriLow, "Skipping PexDevMgr initialization, using kernel mode RM\n");
        m_Found = true;
        return OK;
    }

    const UINT32 classes[] =
    {
        Pci::CLASS_CODE_BRIDGE,
        Pci::CLASS_CODE_BRIDGE_SD
    };
    Printf(Tee::PriLow, "Find bridge ports\n");

    for (UINT32 idx = 0; idx < sizeof(classes) / sizeof(classes[0]); idx++)
    {
        while (OK == Platform::FindPciClassCode(classes[idx],
                                        Index++,
                                        &Domain,
                                        &Bus,
                                        &Device,
                                        &Function))
        {
            UINT32 VendorDeviceId;
            PexDevice::PexDevType Type = PexDevice::UNKNOWN_PEXDEV;
            const char* typeStr = "";
            Platform::PciRead32(Domain, Bus, Device, 0, VENDOR_ID_REG, &VendorDeviceId);
            if ((VendorDeviceId & 0xFFF0FFFF) == BR04_DEV_VENDOR_ID)
            {
                Type = PexDevice::BR04;
                typeStr = "BR04 ";
            }
            else if (VendorDeviceId == BR03_DEV_VENDOR_ID)
            {
                Type = PexDevice::BR03;
                typeStr = "BR03 ";
            }
            else if (s_PlxDevVendorIds.count(VendorDeviceId) ||
                     s_Plx880XXDevVendorIds.count(VendorDeviceId))
            {
                Type = PexDevice::PLX;
                typeStr = "PLX ";
            }
            else if (VendorDeviceId == QEMU_BRIDGE_DEV_VENDOR_ID)
            {
                Type = PexDevice::QEMU_BRIDGE;
                typeStr = "QEMU ";
            }

            UINT32 BusNumReg = 0;
            Platform::PciRead32(Domain, Bus, Device, Function, BUS_NUM_REG, &BusNumReg);

            PexDevice::PciDevice Port = { };
            Port.Domain = Domain;
            Port.Bus = Bus;
            Port.Dev = Device;
            Port.Func = Function;
            Port.SecBus = (BusNumReg & 0xFF00) >> 8;
            Port.SubBus = (BusNumReg & 0xFF0000) >> 16;
            Port.VendorDeviceId = VendorDeviceId;
            Port.Type = Type;
            Port.pDevice = NULL;
            Port.UniqueLinkPolling = RequiresUniqueLinkPolling(VendorDeviceId);

            if ((BusNumReg & 0xFFU) != Bus)
            {
                Printf(Tee::PriWarn, "Unexpected primary bus %02x for bridge %04x:%02x:%02x.%x\n",
                       BusNumReg & 0xFFU, Domain, Bus, Device, Function);
            }

            Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] %sbridge "
                   "sec=%02x sub=%02x vendor=%04x devid=%04x uniq_link_poll=%s\n",
                   Domain, Bus, Device, Function, typeStr,
                   Port.SecBus, Port.SubBus,
                   VendorDeviceId & 0xFFFFU, VendorDeviceId >> 16,
                   Port.UniqueLinkPolling ? "true" : "false");

                // insert new port to the collection
                UINT32 portId = (Port.Domain << 16) | Port.SecBus;
                m_BridgePorts[portId] = Port;

                // QEMU bridge devices present as a single port instead of an upstream
                // port with multiple downstream ports, put a duplicate port in the
                // bridge port list so that the QEMU device can be represented as a MODS
                // PexDevice which requires a PciDevice for both up and downstream
                if (Port.Type == PexDevice::QEMU_BRIDGE)
                    m_BridgePorts[portId | QEMU_DUPLICATE_PORT] = Port;
        }
    }

    // Step 2: Must assign the upstream port of all leaf devices we care about
    // this creates the initial PexDevices (including grouping up/down ports of
    // bridge devices) that are then scanned during ConstructTopology
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
    {
        TestDevicePtr pTestDevice;
        CHECK_RC(pTestDeviceMgr->GetDevice(devInst, &pTestDevice));
        if ((pTestDevice->GetType() == TestDevice::TYPE_IBM_NPU) ||
            pTestDevice->IsModsInternalPlaceholder())
        {
            continue;
        }

        UINT32 domainNum = pTestDevice->GetInterface<Pcie>()->DomainNumber();
        UINT32 busNum = pTestDevice->GetInterface<Pcie>()->BusNumber();

        FindPexDevice(static_cast<UINT16>(domainNum),
                      static_cast<UINT08>(busNum),
                      static_cast<UINT08>(busNum));
    }

    for (const LeafDevice& dev : m_LeafDevices)
    {
        FindPexDevice(dev.domain, dev.secBus, dev.subBus);
    }

    // Step 3:
    CHECK_RC(ConstructTopology());

#if defined(PPC64LE)
    // Step 4 (PPC only) : enable bus mastering and IO space on all bridge
    // devices starting at the GPU and going to the rootport
    CHECK_RC(EnableBusMastering());
#endif

    Printf(Tee::PriLow, "------Finish bridge detection-----\n");

    m_Found = true;

    return OK;
}

// this will be useful when we have a JS interface for bridges
RC PexDevMgr::GetDevice(UINT32 domainBus, Device **pDevice)
{
    MASSERT(pDevice);
    // Chipset has no Upstream Bus, so if the bus is the bus number of
    // chipset's downstream bus number, then return Chipset
    // Todo: fix the GetRoot and m_pRoot - should not take param
    UINT32 domain = domainBus >> 16;
    UINT32 bus = domainBus & 0xFFFF;
    PexDevice* pBridge = GetRoot(0);
    PexDevice::PciDevice* pPciDev = pBridge->GetDownStreamPort(0);
    if (domain == pPciDev->Domain &&
        bus == pPciDev->Bus)
    {
        *pDevice = pBridge;
        return OK;
    }

    for (auto& pexDev : m_Bridges)
    {
        if (pexDev.IsRoot())
            continue;
        pPciDev = pexDev.GetUpStreamPort();
        if (domain == pPciDev->Domain &&
            bus == pPciDev->Bus)
        {
            *pDevice = &pexDev;
            return OK;
        }
    }

    Printf(Tee::PriError, "Invalid domainBus number! - Get bridge failed\n");
    return RC::ILWALID_DEVICE_ID;
}

// If there's no need for GetRoot .... consider removing this
PexDevice* PexDevMgr::GetRoot(UINT32 Index)
{
    MASSERT(Index < m_pRoots.size());
    return m_pRoots[Index];
}

//-----------------------------------------------------------------------------
RC PexDevMgr::RegisterLeafDevice(UINT32 domain, UINT32 secBus, UINT32 subBus)
{
    LeafDevice dev = {domain, secBus, subBus};
    m_LeafDevices.push_back(dev);
    return RC::OK;
}

//-----------internal functions:------------

// This constructs a PexDevice object
//
// This function relies on the provided SecBus being the bus of either a leaf
// device or the upstream bus of a PCIE bridge device in order to function
// correctly.  This function identifies the entire PEX device (bridge or
// rootport) that contains the port with the specified secondary bus
//
// When implementing this function it is important to note that some lwstomers
// program the secondary bus numbers higher than any bus that actually exists
// below the current bus (in effect "reserving" bus numbers).  The previous
// implementation of this function used the subordinate bus as part of the
// identification scheme and this resulted in incorrect PEX device
// identification when a customer "reserved" a bus number
//
// The algorithm used in this function is as follows:
//
// Step1 : Find the port above using the provided SecBus. PciDevices are indexed
//         by their secondary port since that will always be unique.  The
//         secondary bus specifies the lowest bus number connected below the
//         current device.  PCI bus numbers are arranged such that there
//         cannot be a device with a higher bus number than the bus number of
//         the current device in the entire PCI device tree below the current
//         device.
//
// Step2 : The port above *must* be a downstream port (see assumption),
//         therefore locate all siblings of the downstream port (i.e. ports
//         which all share the same bus number).  Put these ports into an array
//         since they will be part of a single PEX device (bridge or rootport)
//
// Step3 : Find the upstream port from the collection of downstream ports
//         identified in step2 by looking for a port with a secondary bus that
//         matches the primary bus of the downstream ports.  If there is no
//         matching port, then the collection of downstream ports must be a
//         rootport (*important* since this is the exit case for the relwrsive
//         call exelwted in step 4 - a rootport *must* be found).  In this case
//         create the rootport PexDevice using the collection of downstream
//         ports and exit
//
// Step4 : If an upstream port was identified for the collection of downstream
//         ports in step 3, then relwrsively call this function using the bus
//         of the upstream port (since this is a bridge device) in order to
//         identify the PEX device above this one.
//
bool PexDevMgr::FindPexDevice(UINT16 Domain, UINT08 SecBus, UINT08 SubBus)
{
    const UINT32 portId = (Domain << 16) | SecBus;

    // Step 1 : Find a downstream port with a secondary bus that matches.
    if (!m_BridgePorts.count(portId))
    {
        Printf(Tee::PriLow, "found root at secondary %04x:%02x:00.0\n",
               Domain, SecBus);
        return false;
    }

    PexDevice::PciDevice& bridgePort = m_BridgePorts[portId];

    // The Subordinate bus number of the upstream port *must* be greater than
    // or equal to the subordinate bus number of the downstream port to which
    // it is connected (PCI spec)
    MASSERT(bridgePort.SubBus >= SubBus);

    if (bridgePort.Located)
    {
        Printf(Tee::PriLow,
               "Port already found: %04x:%02x:%02x.%x  sec=%02x  sub=0x%02x\n",
               bridgePort.Domain,
               bridgePort.Bus,
               bridgePort.Dev,
               bridgePort.Func,
               SecBus, bridgePort.SubBus);
        return true;
    }
    vector<PexDevice::PciDevice*> siblingPorts;
    if (bridgePort.Type == PexDevice::QEMU_BRIDGE)
    {
        bridgePort.Located = true;
        bridgePort.Mutex = m_ErrorCollectorMutex;
        m_BridgePorts[portId | QEMU_DUPLICATE_PORT].Located = true;
        m_BridgePorts[portId | QEMU_DUPLICATE_PORT].Mutex = m_ErrorCollectorMutex;

        // QEMU Bridge devices do not have sibling downstream ports or an
        // upstream port, they present as a single PexDevice
        FindPexDevice(bridgePort.Domain, bridgePort.Bus, bridgePort.SubBus);

        // create put the new bridge into the list
        Printf(Tee::PriLow, "Found QEMU bridge at %04x:%02x:%02x.%x\n",
               bridgePort.Domain,
               bridgePort.Bus,
               bridgePort.Dev,
               bridgePort.Func);
        siblingPorts.push_back(&m_BridgePorts[portId | QEMU_DUPLICATE_PORT]);
        m_Bridges.emplace_back(false, &bridgePort, siblingPorts);
        return true;
    }

    // Step 2: locate the sibling ports
    UINT16 siblingDomain = bridgePort.Domain;
    UINT08 siblingBus = bridgePort.Bus;
    map<UINT32, PexDevice::PciDevice>::iterator pPortAbove;
    Printf(Tee::PriLow, "Find siblings at %04x:%02x:xx.x\n",
           siblingDomain, siblingBus);
    for (auto& portPair : m_BridgePorts)
    {
        auto& port = portPair.second;
        if (port.Domain == siblingDomain &&
            port.Bus    == siblingBus)
        {
            Printf(Tee::PriLow, "  Sibling found: domain=%04x sec=%02x sub=%02x\n",
                   port.Domain,
                   port.SecBus,
                   port.SubBus);
            port.Located = true;
            port.Mutex = m_ErrorCollectorMutex;
            siblingPorts.push_back(&port);
        }
    }

    // Step 3: Find the UpStream port from the collection of bridge ports
    const UINT32 siblingId = (siblingDomain << 16) | siblingBus;
    auto portAboveIt = m_BridgePorts.find(siblingId);
    if (portAboveIt == m_BridgePorts.end())
    {
        // add the new bridge into the list
        Printf(Tee::PriLow, "  Found chipset controller's port at %04x:%02x:%02x.%x\n",
               siblingPorts[0]->Domain,
               siblingPorts[0]->Bus,
               siblingPorts[0]->Dev,
               siblingPorts[0]->Func);
        m_Bridges.emplace_back(true, nullptr, siblingPorts);
        return false;
    }

    auto& portAbove = portAboveIt->second;
    portAbove.Located = true;
    portAbove.Mutex = m_ErrorCollectorMutex;
    Printf(Tee::PriLow, "  Upstream port: domain=%04x sec=%02x sub=%02x\n",
           portAbove.Domain,
           portAbove.SecBus,
           portAbove.SubBus);

    // Step 4 : Look for a bridge device above: its downstream port secondary
    // bus would be the current upstream port's bus number
    FindPexDevice(portAbove.Domain, portAbove.Bus, portAbove.SubBus);

    // add the new bridge into the list
    Printf(Tee::PriLow, "  Found bridge at %04x:%02x:%02x.%x\n",
           portAbove.Domain, portAbove.Bus, portAbove.Dev, portAbove.Func);
    m_Bridges.emplace_back(false, &portAbove, siblingPorts);

    return true;
}

// From the list of Bridge devices and GpuSubdevices, connect them together
RC PexDevMgr::ConstructTopology()
{
    RC rc;

    // Step 1: Construct the Root array
    // Todo: if there's no need for m_pRoots, consider removing this
    for (auto& pexDev : m_Bridges)
    {
        if (pexDev.IsRoot())
        {
            PexDevice::PciDevice* pDownPort = pexDev.GetDownStreamPort(0);
            Printf(Tee::PriLow, "Root check: %04x:%02x:%02x.%x IsRoot=%s\n",
                   pDownPort->Domain,
                   pDownPort->Bus,
                   pDownPort->Dev,
                   pDownPort->Func,
                   pexDev.IsRoot() ? "true" : "false");
            m_pRoots.push_back(&pexDev);
        }
    }
    if (m_pRoots.size() != 1)
    {
        Printf(Tee::PriLow, "%u PEX roots found\n", static_cast<unsigned>(m_pRoots.size()));
    }

    // Step 2: for each Bridge, look for a bridge for each of its downstream ports
    for (auto& pexDev : m_Bridges)
    {
        CHECK_RC(ConnectPorts(&pexDev));
    }
    return OK;
}

RC PexDevMgr::ConnectPorts(PexDevice* pBridgeDev)
{
    MASSERT(pBridgeDev);
    RC rc;
    // for each downstream port.
    // maybe later replace 4 with pBridgeDev->NumDPorts()?
    const char* upstreamType = "";
    const PexDevice::PciDevice* pUpPort = nullptr;
    if (!pBridgeDev->IsRoot())
    {
        if (pBridgeDev->GetDepth() == PexDevice::DEPTH_UNINITIALIZED)
        {
            PexDevice* pUpStreamDev = pBridgeDev->GetUpStreamDev();
            MASSERT(pUpStreamDev);
            UINT32 UpDepth = pUpStreamDev->GetDepth();
            CHECK_RC(pBridgeDev->SetDepth(UpDepth + 1));
        }
        upstreamType = "Bridge";
        pUpPort = pBridgeDev->GetUpStreamPort();
    }
    else
    {
        CHECK_RC(pBridgeDev->SetDepth(0));
        upstreamType = "Chipset";
        // Note: chipset does not have UpStreamPort
        pUpPort = pBridgeDev->GetDownStreamPort(0);
    }
    Printf(Tee::PriLow, "Connecting ports for %s %04x:%02x:%02x.%x:\n",
           upstreamType, pUpPort->Domain, pUpPort->Bus, pUpPort->Dev, pUpPort->Func);

    for (UINT32 i = 0; i < pBridgeDev->GetNumDPorts(); i++)
    {
        PexDevice::PciDevice* pDownPort = pBridgeDev->GetDownStreamPort(i);
        MASSERT(pDownPort);

        if (pBridgeDev->IsRoot())
        {
            pDownPort->Root = true;
        }

        //look for matching Upstream port of known bridge devices
        for (auto& pexDev : m_Bridges)
        {
            if (pexDev.IsRoot())
                continue;

            PexDevice::PciDevice* pUpPort = pexDev.GetUpStreamPort();
            MASSERT(pUpPort);
            if ((pDownPort->Domain == pUpPort->Domain) &&
                (pDownPort->SecBus <= pUpPort->Bus) &&
                (pDownPort->SubBus >= pUpPort->Bus) )
            {
                // Connect the up and down stream port, then create new Link
                pDownPort->pDevice = &pexDev;
                pDownPort->IsPexDevice = true;
                CHECK_RC(pexDev.SetUpStreamDevice(pBridgeDev, i));
                Printf(Tee::PriLow, "  DP%u %04x:%02x:%02x.%x connects to Bridge %04x:%02x:%02x.%x\n",
                       i,
                       pDownPort->Domain, pDownPort->Bus, pDownPort->Dev, pDownPort->Func,
                       pUpPort->Domain, pUpPort->Bus, pUpPort->Dev, pUpPort->Func);
                break;
            }
        }

        // already found child, continue with next port
        if (pDownPort->pDevice)
            continue;

        // look to see if a TestDevice's bus number matches
        TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
        for (UINT32 devIdx = 0; devIdx < pTestDeviceMgr->NumDevices(); devIdx++)
        {
            TestDevicePtr pTestDevice;
            CHECK_RC(pTestDeviceMgr->GetDevice(devIdx, &pTestDevice));
            if ((pTestDevice->GetType() == TestDevice::TYPE_IBM_NPU) ||
                pTestDevice->IsModsInternalPlaceholder())
            {
                continue;
            }

            UINT32 BusNum = pTestDevice->GetInterface<Pcie>()->BusNumber();
            UINT32 DomainNum = pTestDevice->GetInterface<Pcie>()->DomainNumber();
            if ((pDownPort->Domain == DomainNum) &&
                (pDownPort->SecBus <= BusNum) &&
                (pDownPort->SubBus >= BusNum))
            {
                pDownPort->pDevice = pTestDevice.get();
                pDownPort->IsPexDevice = false;
                pTestDevice->GetInterface<Pcie>()->SetUpStreamDevice(pBridgeDev, i);
                Printf(Tee::PriLow, "  DP%u %04x:%02x:%02x.%x connects to test device %s\n",
                       i,
                       pDownPort->Domain, pDownPort->Bus, pDownPort->Dev, pDownPort->Func,
                       pTestDevice->GetName().c_str());
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// Starting with each GPU, enable bus mastering and memory space on all bridge
// devices all the way up to the rootport
RC PexDevMgr::EnableBusMastering()
{
    RC rc;

    GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
    for (GpuSubdevice *pSubdev = pGpuDevMgr->GetFirstGpu();
         pSubdev != NULL; pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
    {
        PexDevice *pPexDev;
        UINT32     port;
        CHECK_RC(pSubdev->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &port));

        CHECK_RC(pPexDev->EnableDownstreamBusMastering(port));

        while (!pPexDev->IsRoot())
        {
            CHECK_RC(pPexDev->EnableUpstreamBusMastering());

            port = pPexDev->GetUpStreamIndex();
            pPexDev = pPexDev->GetUpStreamDev();

            MASSERT(pPexDev);
            CHECK_RC(pPexDev->EnableDownstreamBusMastering(port));
        }
    }

    return rc;
}

bool PexDevMgr::IsPexDeviceAbove(GpuSubdevice* pGpuSubdev,
                                 PexDevice* pPexDevAbove)
{
    MASSERT(pGpuSubdev);
    MASSERT(pPexDevAbove);
    UINT32 BusNum = pGpuSubdev->GetInterface<Pcie>()->BusNumber();
    UINT32 DomainNum = pGpuSubdev->GetInterface<Pcie>()->DomainNumber();

    for (UINT32 i = 0; i < pPexDevAbove->GetNumDPorts(); i++)
    {
        PexDevice::PciDevice* pPort = pPexDevAbove->GetDownStreamPort(i);
        if ((pPort->Domain == DomainNum) &&
            (pPort->SecBus <= BusNum) &&
            (pPort->SubBus >= BusNum))
        {
            return true;
        }
    }
    return false;
}

bool PexDevMgr::IsPexDeviceAbove(PexDevice* pBridgeBelow,
                                 PexDevice* pPexDevAbove)
{
    MASSERT(pBridgeBelow);
    MASSERT(pPexDevAbove);
    UINT32 BusNum = pBridgeBelow->GetUpStreamPort()->Bus;
    UINT32 DomainNum = pBridgeBelow->GetUpStreamPort()->Domain;

    for (UINT32 i = 0; i < pPexDevAbove->GetNumDPorts(); i++)
    {
        PexDevice::PciDevice* pPort = pPexDevAbove->GetDownStreamPort(i);
        if ((pPort->Domain == DomainNum) &&
            (pPort->SecBus <= BusNum) &&
            (pPort->SubBus >= BusNum))
        {
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
void PexDevMgr::PrintPexDeviceInfo()
{
    for (auto& pexDev : m_Bridges)
    {
        pexDev.PrintInfo(Tee::PriNormal);
    }
}

//-----------------------------------------------------------------------------
// Called from DumpPexInfo() in gpudecls.js
//
RC PexDevMgr::DumpPexInfo
(
    TestDevice* pTestDevice,
    UINT32      pexVerbose,
    bool        printOnceRegs
)
{
    // Create static linked lists of registers that we might dump
    //
    struct RegDesc
    {
        ModsGpuRegAddress          addr;
        UINT32                     idx;                    
        const Obfuscate::RtString* pName;
        bool                       dumpInOfficial;
        RegDesc*                   pNext;
        bool                       bUseIdx;
    };

#define ADD_REG_DESC(name, dumpInOfficial)                               \
    {                                                                    \
        using CtString = Obfuscate::CtString<                            \
                    static_cast<UINT32>(MODS_ ## name), sizeof(#name)>;  \
        static constexpr CtString obfuscatedName(#name);                 \
        static RegDesc regDesc =                                         \
            { MODS_ ## name, 0, &obfuscatedName, dumpInOfficial, nullptr, false }; \
        *ppTail = &regDesc;                                              \
        ppTail = &regDesc.pNext;                                         \
    }

#define ADD_REG_DESC_IDX(name, idx, dumpInOfficial)                      \
    {                                                                    \
        using CtString = Obfuscate::CtString<                            \
                    static_cast<UINT32>(MODS_ ## name), sizeof(#name)>;  \
        static constexpr CtString obfuscatedName(#name#idx);             \
        static RegDesc regDesc =                                         \
            { MODS_ ## name, idx, &obfuscatedName, dumpInOfficial, nullptr, true }; \
        *ppTail = &regDesc;                                              \
        ppTail = &regDesc.pNext;                                         \
    }

    static RegDesc* pPreGen3PexRegs = nullptr;
    if (pPreGen3PexRegs == nullptr)
    {
        RegDesc** ppTail = &pPreGen3PexRegs;
        ADD_REG_DESC(XP_PEX_PAD_CTL_2,                         false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_5,                         false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_6,                         false);
        ADD_REG_DESC(XP_PEX_PLL_CTL2,                          false);
        ADD_REG_DESC(XP_GEN2_FTS,                              false);
        ADD_REG_DESC(XP_IDLE_TIMER_0,                          false);
        ADD_REG_DESC(XP_IDLE_TIMER_1,                          false);
        ADD_REG_DESC(XP_IDLE_TIMER_2,                          false);
        ADD_REG_DESC(EP_PCFG_GPU_DEVICE_CONTROL_STATUS,        false);
        ADD_REG_DESC(EP_PCFG_GPU_LINK_CONTROL_STATUS,          false);
        ADD_REG_DESC(EP_PCFG_GPU_LINK_CONTROL_STATUS_2,        false);
        ADD_REG_DESC(XVE_PRIV_XP_2,                            false);
        ADD_REG_DESC(XVE_PRIV_XV_0,                            false);
        ADD_REG_DESC(XVE_PRIV_XP_NAK_COUNT,                    false);
        ADD_REG_DESC(XVE_PRIV_XP_REPLAY_COUNT,                 false);
        ADD_REG_DESC(XVE_PRIV_XP_RECOVERY_COUNT,               false);
        ADD_REG_DESC(XVE_PRIV_XP_PAD_PWRDN,                    false);
        ADD_REG_DESC(XVE_PRIV_XP_STATS0,                       false);
        ADD_REG_DESC(XVE_PRIV_XP_STATS1,                       false);
        ADD_REG_DESC(EP_PCFG_GPU_UNCORRECTABLE_ERROR_STATUS,   false);
        ADD_REG_DESC(EP_PCFG_GPU_CORRECTABLE_ERROR_STATUS,     false);
        ADD_REG_DESC(XVE_PRIV_XP_FTS,                          false);
        ADD_REG_DESC(XVE_PRIV_XP_LCTRL_2,                      false);
        ADD_REG_DESC(XVE_PRIV_XP_CHIPSET_XMIT_L0S_ENTRY_COUNT, true);
        ADD_REG_DESC(XVE_PRIV_XP_GPU_XMIT_L0S_ENTRY_COUNT,     true);
        ADD_REG_DESC(XVE_PRIV_XP_L1_ENTRY_COUNT,               false);
        ADD_REG_DESC(XVE_PRIV_XP_L1P_ENTRY_COUNT,              false);
        ADD_REG_DESC(XVE_PRIV_XP_L1_TO_RECOVERY_COUNT,         false);
        ADD_REG_DESC(XVE_PRIV_XP_L0_TO_RECOVERY_COUNT,         false);
        ADD_REG_DESC(XVE_ERROR_COUNTER,                        false);
        ADD_REG_DESC(XVE_PRIV_XP_DEEP_L1_ENTRY_COUNT,          false);
        ADD_REG_DESC(XVE_PRIV_XP_ASLM_COUNTER,                 false);
        ADD_REG_DESC(XVE_PEX_PLL,                              false);
    };

    static RegDesc* pGen3PexRegs = nullptr;
    if (pGen3PexRegs == nullptr)
    {
        RegDesc** ppTail = &pGen3PexRegs;
        ADD_REG_DESC(EP_PCFG_GPU_LINK_CONTROL_STATUS,         false);
        ADD_REG_DESC(EP_PCFG_GPU_LINK_CONTROL_STATUS_2,       false);
        ADD_REG_DESC(XVE_PRIV_XP_2,                           false);
        ADD_REG_DESC(XVE_PRIV_XV_0,                           false);
        ADD_REG_DESC(EP_PCFG_GPU_LANE_ERROR_STATUS,           false);
        ADD_REG_DESC(XVE_ERROR_COUNTER1,                      false);
        ADD_REG_DESC(XVE_ERROR_COUNTER,                       false);
        ADD_REG_DESC(EP_PCFG_GPU_UNCORRECTABLE_ERROR_STATUS,  false);
        ADD_REG_DESC(EP_PCFG_GPU_CORRECTABLE_ERROR_STATUS,    false);
        ADD_REG_DESC(XP_PL_PAD_PWRDN,                         false);
        ADD_REG_DESC(XP_LANE_ERROR_STATUS,                    false);
        ADD_REG_DESC(XP_8B10B_ERRORS_COUNT,                   false);
        ADD_REG_DESC(XP_SYNC_HEADER_ERRORS_COUNT,             false);
        ADD_REG_DESC(XP_LANE_ERRORS_COUNT_0,                  false);
        ADD_REG_DESC(XP_LANE_ERRORS_COUNT_1,                  false);
        ADD_REG_DESC(XP_LANE_ERRORS_COUNT_2,                  false);
        ADD_REG_DESC(XP_LANE_ERRORS_COUNT_3,                  false);
        ADD_REG_DESC(XP_RECEIVER_ERRORS_COUNT,                false);
        ADD_REG_DESC(XP_LCRC_ERRORS_COUNT,                    false);
        ADD_REG_DESC(XP_FAILED_L0S_EXITS_COUNT,               false);
        ADD_REG_DESC(XP_NAKS_SENT_COUNT,                      false);
        ADD_REG_DESC(XP_NAKS_RCVD_COUNT,                      false);
        ADD_REG_DESC(XP_REPLAY_COUNT,                         false);
        ADD_REG_DESC(XP_REPLAY_ROLLOVER_COUNT,                false);
        ADD_REG_DESC(XP_L1_TO_RECOVERY_COUNT,                 false);
        ADD_REG_DESC(XP_L0_TO_RECOVERY_COUNT,                 false);
        ADD_REG_DESC(XP_RECOVERY_COUNT,                       false);
        ADD_REG_DESC(XP_BAD_DLLP_COUNT,                       false);
        ADD_REG_DESC(XP_BAD_TLP_COUNT,                        false);
        ADD_REG_DESC(XP_CHIPSET_XMIT_L0S_ENTRY_COUNT,         true);
        ADD_REG_DESC(XP_GPU_XMIT_L0S_ENTRY_COUNT,             true);
        ADD_REG_DESC(XP_L1_ENTRY_COUNT,                       true);
        ADD_REG_DESC(XP_L1P_ENTRY_COUNT,                      false);
        ADD_REG_DESC(XP_DEEP_L1_ENTRY_COUNT,                  false);
        ADD_REG_DESC(XP_L1_1_ENTRY_COUNT,                     false);
        ADD_REG_DESC(XP_L1_2_ENTRY_COUNT,                     false);
        ADD_REG_DESC(XP_L1_2_ABORT_COUNT,                     false);
        ADD_REG_DESC(XP_L1_SUBSTATE_TO_DEEP_L1_TIMEOUT_COUNT, false);
        ADD_REG_DESC(XP_L1_SHORT_DURATION_COUNT,              false);
        ADD_REG_DESC(XP_ASLM_COUNT,                           false);
        ADD_REG_DESC(XP_SKPOS_ERRORS_COUNT,                   false);
        ADD_REG_DESC(XP_PL_EQUALIZATION_0,                    false);
        ADD_REG_DESC(XP_PL_EQUALIZATION_1,                    false);
    };

    static RegDesc* pGen3PexRegsOnce = nullptr;
    if (pGen3PexRegsOnce == nullptr)
    {
        RegDesc** ppTail = &pGen3PexRegsOnce;
        ADD_REG_DESC(XP_PEX_PAD_CTL_SEL,          false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_1,            false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_2,            false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_3,            false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_4,            false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_5,            false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_6,            false);
        ADD_REG_DESC(XP_PEX_PAD_CTL_7,            false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_1_R1,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_2_R1,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_3_R1,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R1,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_1_R2,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_2_R2,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_3_R2,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R2,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_1_R3,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_2_R3,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_3_R3,        false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R3_0,      false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R3_1,      false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R3_2,      false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R3_3,      false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R3_4,      false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R3_5,      false);
        ADD_REG_DESC(XP_PEX_PAD_ECTL_4_R3_6,      false);
        ADD_REG_DESC(XP_PEX_PLL_CTL1,             false);
        ADD_REG_DESC(XP_PEX_PLL_CTL2,             false);
        ADD_REG_DESC(XP_PEX_PLL_CTL3,             false);
        ADD_REG_DESC(XP_PEX_PLL_CTL4_PLL,         false);
        ADD_REG_DESC(XP_PL_PAD_CFG,               false);
        ADD_REG_DESC(XP_PEX_PLL,                  false);
        ADD_REG_DESC(XP_PEX_PLL2,                 false);
        ADD_REG_DESC(XP_PL_N_FTS_LOCAL,           false);
        ADD_REG_DESC(XP_PL_N_FTS_REMOTE,          false);
        ADD_REG_DESC(XP_PL_FTS_DETECT_START,      false);
        ADD_REG_DESC(XP_PL_LCTRL_2,               false);
        ADD_REG_DESC(XP_PL_IDLE_TIMER_RX_L0S_DLY, false);
        ADD_REG_DESC(XP_PL_IDLE_TIMER_TX_L0S_MIN, false);
        ADD_REG_DESC(XP_PL_IDLE_TIMER_L1_MIN,     false);
    };

    // TODO: Discuss this with Lael what types of registers should be dumped?
    static RegDesc* pGen5PexRegs = nullptr;
    if (pGen5PexRegs == nullptr)
    {
        RegDesc** ppTail = &pGen5PexRegs;
        ADD_REG_DESC(EP_PCFG_GPU_LINK_CONTROL_STATUS,         false);
        ADD_REG_DESC(EP_PCFG_GPU_LINK_CONTROL_STATUS_2,       false);
        ADD_REG_DESC(XTL_EP_PRI_PRIV_XV_0,                    false); // replaces XVE_PRIV_XV_0
        ADD_REG_DESC(EP_PCFG_GPU_LANE_ERROR_STATUS,           false);
        ADD_REG_DESC(XTL_EP_PRI_ERROR_COUNTER,                false); //$ replaces XVE_ERROR_COUNTER
        ADD_REG_DESC(XTL_EP_PRI_ERROR_COUNTER1,               false); //$ replaces XVE_ERROR_COUNTER1
        ADD_REG_DESC(EP_PCFG_GPU_UNCORRECTABLE_ERROR_STATUS,  false);
        ADD_REG_DESC(EP_PCFG_GPU_CORRECTABLE_ERROR_STATUS,    false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_PAD_PWRDN,                false); //$ replaces XP_PL_PAD_PWRDN
        ADD_REG_DESC(XPL_PL_RXLANES_ERROR_STATUS,             false); //$ replaces XP_LANE_ERROR_STATUS
        ADD_REG_DESC(XPL_PL_RXLANES_8B10B_ERRORS_COUNT,       false); //$ replaces XP_8B10B_ERRORS_COUNT
        ADD_REG_DESC(XPL_PL_RXLANES_SYNC_HEADER_ERRORS_COUNT, false); //$ replaces XP_SYNC_HEADER_ERRORS_COUNT
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 0,      false); //$ replaces XP_LANE_ERRORS_COUNT_0
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 1,      false); //$ replaces XP_LANE_ERRORS_COUNT_1
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 2,      false); //$ replaces XP_LANE_ERRORS_COUNT_2
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 3,      false); //$ replaces XP_LANE_ERRORS_COUNT_3
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 4,      false); // ...
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 5,      false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 6,      false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 7,      false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 8,      false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 9,      false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 10,     false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 11,     false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 12,     false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 13,     false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 14,     false); // 
        ADD_REG_DESC_IDX(XPL_PL_RXLANES_ERRORS_COUNT, 15,     false); // 
        ADD_REG_DESC(XPL_DL_RECEIVER_ERRORS_COUNT,            false); //$ replaces XP_RECEIVER_ERRORS_COUNT
        ADD_REG_DESC(XPL_DL_LCRC_ERRORS_COUNT,                false); //$ replaces XP_LCRC_ERRORS_COUNT
        ADD_REG_DESC(XPL_DL_NAKS_SENT_COUNT,                  false); //$ replaces XP_NAKS_SENT_COUNT
        ADD_REG_DESC(XPL_DL_NAKS_RCVD_COUNT,                  false); //$ replaces XP_NAKS_RCVD_COUNT
        ADD_REG_DESC(XPL_DL_REPLAY_COUNT,                     false); //$ replaces XP_REPLAY_COUNT
        ADD_REG_DESC(XPL_DL_REPLAY_ROLLOVER_COUNT,            false); //$ replaces XP_REPLAY_ROLLOVER_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L1_TO_RECOVERY_COUNT,       false); //$ replaces XP_L1_TO_RECOVERY_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L0_TO_RECOVERY_COUNT,       false); //$ replaces XP_L0_TO_RECOVERY_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_RECOVERY_COUNT,             false); //$ replaces XP_RECOVERY_COUNT
        ADD_REG_DESC(XPL_DL_BAD_DLLP_COUNT,                   false); //$ replaces XP_BAD_DLLP_COUNT
        ADD_REG_DESC(XPL_DL_BAD_TLP_COUNT,                    false); //$ replaces XP_BAD_TLP_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L1_ENTRY_COUNT,              true); //$ replaces XP_L1_ENTRY_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L1_PLL_PD_ENTRY_COUNT,      false); //$ replaces XP_L1P_ENTRY_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L1_CPM_ENTRY_COUNT,         false); //$ replaces XP_DEEP_L1_ENTRY_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L1_1_ENTRY_COUNT,           false); //$ replaces XP_L1_1_ENTRY_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L1_2_ENTRY_COUNT,           false); //$ replaces XP_L1_2_ENTRY_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_L1_2_ABORT_COUNT,           false); //$ replaces XP_L1_2_ABORT_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_ASLM_COUNT,                 false); //$ replaces XP_ASLM_COUNT
        ADD_REG_DESC(XPL_DL_SKPOS_ERRORS_COUNT,               false); //$ replaces XP_SKPOS_ERRORS_COUNT
        ADD_REG_DESC(XPL_PL_LTSSM_EQUALIZATION_0,             false); //$ replaces XP_PL_EQUALIZATION_0
        ADD_REG_DESC(XPL_PL_LTSSM_EQUALIZATION_1,             false); //$ replaces XP_PL_EQUALIZATION_1
        ADD_REG_DESC(XPL_DL_LTSSM_RECOVERY_REASON,            false); //$ new request
        ADD_REG_DESC(XPL_PL_LTSSM_RECOVERY_REASON,            false); //$ new request
    };

    // Note1: These registers may not be available once the dev_lw_xp.h file is removed. I have
    //        an email out to Alap Petel to comment on how to handle them.
    static RegDesc* pGen5PexRegsOnce = nullptr;
    if (pGen5PexRegsOnce == nullptr)
    {
        RegDesc** ppTail = &pGen5PexRegsOnce;
        ADD_REG_DESC(UXL_LANE_REG_MISC_CTL_0,           false); // replaces XP_PEX_PAD_CTL_1
        ADD_REG_DESC(UXL_LANE_REG_MISC_CTL_1,           false); // replaces XP_PEX_PAD_CTL_2 & XP_PEX_PAD_CTL_3
        ADD_REG_DESC(UXL_LANE_REG_AUX_CTL_0,            false); // replaces XP_PEX_PAD_CTL_4
        ADD_REG_DESC(UXL_LANE_REG_MISC_CTL_2,           false); // replaces XP_PEX_PAD_CTL_5
        ADD_REG_DESC(UXL_LANE_REG_DIRECT_CTL_1,         false); // replaces XP_PEX_PAD_CTL_8
        ADD_REG_DESC(UXL_LANE_REG_DIRECT_CTL_2,         false); // replaces XP_PEX_PAD_CTL_9
        //Gen1
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R1_0,          false); // replaces XP_PEX_PAD_ECTL_1_R1
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R1_1,          false); // replaces XP_PEX_PAD_ECTL_4_R1
        // Gen2
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R2_0,          false); // replaces XP_PEX_PAD_ECTL_1_R2
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R2_1,          false); // replaces XP_PEX_PAD_ECTL_4_R2
        //Gen3
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_0,          false); // replaces XP_PEX_PAD_ECTL_1_R3
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_1,          false); // replaces XP_PEX_PAD_ECTL_4_R3_0
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_2,          false); // replaces XP_PEX_PAD_ECTL_4_R3_1
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_3,          false); // replaces XP_PEX_PAD_ECTL_4_R3_2
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_4,          false); // replaces XP_PEX_PAD_ECTL_4_R3_3
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_5,          false); // replaces XP_PEX_PAD_ECTL_4_R3_4
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_6,          false); // replaces XP_PEX_PAD_ECTL_4_R3_5
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R3_7,          false); // replaces XP_PEX_PAD_ECTL_4_R3_6
        ADD_REG_DESC(XPL_PL_PAD_CTL_TX_MARGIN_MAP_R3_0, false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_TX_MARGIN_MAP_R3_1, false);
        //Gen4
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_0,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_1,          false);// replaces XP_PEX_PAD_ECTL_4_R4_0
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_2,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_3,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_4,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_5,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_6,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R4_7,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_TX_MARGIN_MAP_R4_0, false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_TX_MARGIN_MAP_R4_1, false);
        //Gen5
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_0,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_1,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_2,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_3,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_4,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_5,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_6,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_ECTL_R5_7,          false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_TX_MARGIN_MAP_R5_0, false);
        ADD_REG_DESC(XPL_PL_PAD_CTL_TX_MARGIN_MAP_R5_1, false);
        //Misc
        ADD_REG_DESC(UXL_PLL_REG_CTL_0,                 false); // replaces XP_PEX_PLL_CTL1 & XP_PEX_PLL_CTL2
        ADD_REG_DESC(UXL_PLL_REG_CTL_1,                 false); // replaces XP_PEX_PLL_CTL3
        ADD_REG_DESC(XPL_PL_PAD_CTL_RX_CTL,             false); // replaces XP_PL_PAD_CFG
        ADD_REG_DESC(XPL_PL_PAD_CTL_GEN3_EQ,            false); // replaces XP_PL_PAD_CFG
        ADD_REG_DESC(XPL_PL_PAD_CTL_GEN4_EQ,            false); // replaces XP_PL_PAD_CFG
        ADD_REG_DESC(XPL_PL_PAD_CTL_GEN5_EQ,            false); // replaces XP_PL_PAD_CFG
        ADD_REG_DESC(XTL_EP_PRI_PWR_MGMT_CYA1,          false); // replaces XP_PEX_PLL
        ADD_REG_DESC(XTL_EP_PRI_PWR_MGMT_CYA2,          false); // replaces XP_PEX_PLL
        ADD_REG_DESC(XPL_PL_PAD_CTL_PM_TIMER_0,         false); // replaces XP_PEX_PLL2
        ADD_REG_DESC(XPL_PL_LTSSM_LCTRL_2,              false); // partially replaces XP_PL_LCTRL_2
        ADD_REG_DESC(XPL_PL_LTSSM_RECOVERY_EQ_PHASE0_TOTAL_TIME, false);
        ADD_REG_DESC(XPL_PL_LTSSM_RECOVERY_EQ_PHASE1_TOTAL_TIME, false);
        ADD_REG_DESC(XPL_PL_LTSSM_RECOVERY_EQ_PHASE2_TOTAL_TIME, false);
        ADD_REG_DESC(XPL_PL_LTSSM_RECOVERY_EQ_PHASE3_TOTAL_TIME, false);
        
    };
#undef ADD_REG_DESC

    // Get pointer to the GpuDevice or LwLink RegHal
    //
    RegHal* pRegs = nullptr;
    switch (pTestDevice->GetType())
    {
        case TestDevice::TYPE_LWIDIA_GPU:
            pRegs = &pTestDevice->Regs();
            break;
        case TestDevice::TYPE_LWIDIA_LWSWITCH:
            pRegs = &pTestDevice->GetInterface<LwLinkRegs>()->Regs();
            break;
        default:
            pRegs = nullptr;
            break;
    }

    if (pRegs)
    {
        const bool isOfficial =
            (pexVerbose & (1 << PexDevMgr::PEX_OFFICIAL_MODS_VERBOSITY)) != 0;

        // Writes 0x11111111 LW_XVE_PRIV_XP_STATS0 to clear the error counters
        //
        if (pRegs->IsSupported(MODS_XVE_PRIV_XP_STATS0))
        {
            pRegs->Write32(0, MODS_XVE_PRIV_XP_STATS0, 0x11111111);
        }

        // Choose which registers to dump
        //
        RegDesc* pPexRegs     = pPreGen3PexRegs;
        RegDesc* pPexOnceRegs = nullptr;
        if (pTestDevice->HasFeature(TestDevice::GPUSUB_SUPPORTS_GEN5_PCIE))
        {
            pPexRegs     = pGen5PexRegs;
            pPexOnceRegs = pGen5PexRegsOnce;
        }
        else if (pTestDevice->HasFeature(TestDevice::GPUSUB_SUPPORTS_GEN3_PCIE))
        {
            pPexRegs     = pGen3PexRegs;
            pPexOnceRegs = pGen3PexRegsOnce;
        }

        // Dump the registers
        //
        string pexRegStr = Utility::StrPrintf(
                "%s PEX registers:\n------------------------------------\n",
                pTestDevice->GetName().c_str());

        if (printOnceRegs)
        {
            for (RegDesc* pRegDesc = pPexOnceRegs;
                 pRegDesc != nullptr; pRegDesc = pRegDesc->pNext)
            {
                if ((!isOfficial || pRegDesc->dumpInOfficial) &&
                    pRegs->IsSupported(pRegDesc->addr))
                {
                    const UINT32 seed = static_cast<UINT32>(pRegDesc->addr);
                    if (pRegDesc->bUseIdx)
                    {
                        pexRegStr += Utility::StrPrintf(
                                " LW_%s(%d) = 0x%08x\n",
                                pRegDesc->pName->Decode(seed).c_str(),
                                pRegDesc->idx, 
                                pRegs->Read32(0, pRegDesc->addr, pRegDesc->idx));
                        
                    }
                    else
                    {
                        pexRegStr += Utility::StrPrintf(
                                " LW_%s = 0x%08x\n",
                                pRegDesc->pName->Decode(seed).c_str(),
                                pRegs->Read32(0, pRegDesc->addr));
                    }
                }
            }
        }

        for (RegDesc* pRegDesc = pPexRegs;
             pRegDesc != nullptr; pRegDesc = pRegDesc->pNext)
        {
            if ((!isOfficial || pRegDesc->dumpInOfficial) &&
                pRegs->IsSupported(pRegDesc->addr))
            {
                const UINT32 seed = static_cast<UINT32>(pRegDesc->addr);
                if (pRegDesc->bUseIdx)
                {
                    pexRegStr += Utility::StrPrintf(
                            " LW_%s(%d) = 0x%08x\n",
                            pRegDesc->pName->Decode(seed).c_str(),
                            pRegDesc->idx,
                            pRegs->Read32(0, pRegDesc->addr, pRegDesc->idx));
                }
                else
                {
                    pexRegStr += Utility::StrPrintf(
                            " LW_%s = 0x%08x\n",
                            pRegDesc->pName->Decode(seed).c_str(),
                            pRegs->Read32(0, pRegDesc->addr));
                }
            }
        }
        Printf(Tee::PriNormal, "%s", pexRegStr.c_str());
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC PexDevMgr::SavePexSetting(UINT32 *pRet, UINT32 WhatToSave)
{
    RC rc;
    SnapShot NewSnapshot;
    TestDeviceMgr* pTestDeviceMgr = (TestDeviceMgr*)DevMgrMgr::d_TestDeviceMgr;
    for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
    {
        TestDevicePtr pTestDevice;
        CHECK_RC(pTestDeviceMgr->GetDevice(devInst, &pTestDevice));

        PexDevice *pUpStreamDev;
        UINT32 PortId;
        auto pPcie = pTestDevice->GetInterface<Pcie>();
        CHECK_RC(pPcie->GetUpStreamInfo(&pUpStreamDev, &PortId));
        // don't  putting the setting into Snapshot if there's no dev above
        if (!pUpStreamDev)
            continue;

        PexSettings NewSetting;
        NewSetting.Inst = devInst;
        // get parameters for the link directly above GPU
        PexParam NewParam;
        if (WhatToSave & REST_SPEED)
        {
            NewParam.Speed = pPcie->GetLinkSpeed(Pci::SpeedUnknown);
        }

        if (WhatToSave & REST_DPC)
        {
            CHECK_RC(pUpStreamDev->GetDownStreamDpcState(&NewParam.DpcState, PortId));
        }

        if (WhatToSave & REST_ASPM)
        {
            NewParam.LocAspm = pPcie->GetAspmState();
            CHECK_RC(pUpStreamDev->GetDownStreamAspmState(&NewParam.HostAspm, PortId));

            #if defined(INCLUDE_AZALIA)
            auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
            AzaliaController* pAza = nullptr;
            if (pSubdev && (pAza = pSubdev->GetAzaliaController()))
            {
                CHECK_RC(pAza->GetAspmState(&NewParam.AzaAspm));
            }
            #endif
            #if defined(INCLUDE_XUSB)
            if (pTestDevice->SupportsInterface<XusbHostCtrl>())
            {
                auto pXusb = pTestDevice->GetInterface<XusbHostCtrl>();
                CHECK_RC(pXusb->GetAspmState(&NewParam.XusbAspm));
            }
            if (pTestDevice->SupportsInterface<PortPolicyCtrl>())
            {
                auto pPpc = pTestDevice->GetInterface<PortPolicyCtrl>();
                CHECK_RC(pPpc->GetAspmState(&NewParam.PpcAspm));
            }
            #endif
        }
        if (WhatToSave & REST_ASPM_L1SS)
        {
            if (pPcie->HasAspmL1Substates())
            {
                CHECK_RC(pPcie->GetAspmL1ssEnabled(&NewParam.LocAspmL1SS));
            }
            if (pUpStreamDev->HasAspmL1Substates())
            {
                CHECK_RC(pUpStreamDev->GetDownStreamAspmL1SubState(&NewParam.HostAspmL1SS,
                                                                   PortId));
            }
        }
        if (WhatToSave & REST_LINK_WIDTH)
        {
            NewParam.Width = pPcie->GetLinkWidth();
        }
        NewSetting.List.push_back(NewParam);

        // save PEX settings up to root
        while (pUpStreamDev && !pUpStreamDev->IsRoot())
        {
            if (WhatToSave & REST_SPEED)
            {
                NewParam.Speed = pUpStreamDev->GetUpStreamSpeed();
            }
            if (WhatToSave & REST_ASPM)
            {
                CHECK_RC(pUpStreamDev->GetUpStreamAspmState(&NewParam.LocAspm));
            }

            if ((WhatToSave & REST_ASPM_L1SS) &&
                pUpStreamDev->HasAspmL1Substates())
            {
                CHECK_RC(pUpStreamDev->GetUpStreamAspmL1SubState(&NewParam.LocAspmL1SS));
            }
            // next device
            PortId       = pUpStreamDev->GetUpStreamIndex();
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
            if (pUpStreamDev)
            {
                if (WhatToSave & REST_ASPM)
                {
                    CHECK_RC(pUpStreamDev->GetDownStreamAspmState(&NewParam.HostAspm,
                                                                  PortId));
                }
                if (WhatToSave & REST_DPC)
                {
                    CHECK_RC(pUpStreamDev->GetDownStreamDpcState(&NewParam.DpcState, PortId));
                }
                if ((WhatToSave & REST_ASPM_L1SS) &&
                    pUpStreamDev->HasAspmL1Substates())
                {
                    CHECK_RC(pUpStreamDev->GetDownStreamAspmL1SubState(&NewParam.HostAspmL1SS,
                                                                       PortId));
                }
            }

            NewSetting.List.push_back(NewParam);
        }
        // add a list of setting of a GPU to a snapshot
        NewSnapshot.GpuSettingList[NewSetting.Inst] = NewSetting;
        NewSnapshot.SessionId = m_PexSnapshotCount;
    }
    // add to snapshots
    m_PexSnapshots[m_PexSnapshotCount] = NewSnapshot;
    *pRet = m_PexSnapshotCount;
    m_PexSnapshotCount++;
    return rc;
}
//-----------------------------------------------------------------------------
bool PexDevMgr::DoesSessionIdExist(UINT32 SessionId)
{
    if (m_PexSnapshots.find(SessionId) == m_PexSnapshots.end())
    {
        return false;
    }
    return true;
}
//-----------------------------------------------------------------------------
//! restores a set of PEX parameters:
// SessionId : session ID returned in SavePexSetting
// RemoveSession : if true, forget the session
// GpuDevice : if only want to restore the PEX settings on all paths for a device
// GpuSubdevice : if only want to restore the PEX settings on one path
RC PexDevMgr::RestorePexSetting(UINT32 SessionId,
                                UINT32 WhatToRestore,
                                bool RemoveSession,
                                GpuDevice *pGpuDev, /* =0 */
                                GpuSubdevice *pSubdev /* =0 */)
{
    RC rc;
    if (m_PexSnapshots.find(SessionId) == m_PexSnapshots.end())
    {
        Printf(Tee::PriNormal, "unknown session ID. Bailing out\n");
        return RC::BAD_PARAMETER;
    }
    Printf(m_MsgLevel, "Restoring PEX setting %d\n", SessionId);
    TestDeviceMgr* pTestDeviceMgr = (TestDeviceMgr*)DevMgrMgr::d_TestDeviceMgr;
    TestDevicePtr pTestDevice;
    if (pSubdev)
    {
        CHECK_RC(pTestDeviceMgr->GetDevice(pSubdev->DevInst(), &pTestDevice));
        CHECK_RC(IntRestorePexSetting(SessionId, WhatToRestore, pTestDevice));
    }
    else if (pGpuDev)
    {
        for (UINT32 i = 0; i < pGpuDev->GetNumSubdevices(); i++)
        {
            GpuSubdevice *pSubdev = pGpuDev->GetSubdevice(i);
            CHECK_RC(pTestDeviceMgr->GetDevice(pSubdev->DevInst(), &pTestDevice));
            CHECK_RC(IntRestorePexSetting(SessionId, WhatToRestore, pTestDevice));
        }
    }
    else
    {
        // restore all
        for (UINT32 devInst = 0; devInst < pTestDeviceMgr->NumDevices(); devInst++)
        {
            CHECK_RC(pTestDeviceMgr->GetDevice(devInst, &pTestDevice));
            CHECK_RC(IntRestorePexSetting(SessionId, WhatToRestore, pTestDevice));
        }
    }
    // remove from list if requested:
    if (RemoveSession)
    {
        m_PexSnapshots.erase(SessionId);
    }
    return rc;
}
//-----------------------------------------------------------------------------
//! restores a set of PEX parameters:
// SessionId : session ID returned in SavePexSetting
// RemoveSession : if true, forget the session
// pLwLinkDev : if only want to restore the PEX settings on one path
RC PexDevMgr::RestorePexSetting(UINT32 sessionId,
                                UINT32 whatToRestore,
                                bool removeSession,
                                TestDevicePtr pTestDevice)
{
    RC rc;
    if (m_PexSnapshots.find(sessionId) == m_PexSnapshots.end())
    {
        Printf(Tee::PriNormal, "unknown session ID. Bailing out\n");
        return RC::BAD_PARAMETER;
    }
    Printf(m_MsgLevel, "Restoring PEX setting %d\n", sessionId);

    CHECK_RC(IntRestorePexSetting(sessionId, whatToRestore, pTestDevice));

    if (removeSession)
    {
        m_PexSnapshots.erase(sessionId);
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC PexDevMgr::IntRestorePexSetting(UINT32 SessionId,
                                   UINT32 WhatToRestore,
                                   GpuSubdevice *pSubdev)
{
    RC rc;
    MASSERT(pSubdev);

    SCOPED_DEV_INST(pSubdev);

    // assume session ID is valid since it was checked in RestorePexSetting
    UINT32 devInst = pSubdev->DevInst();
    map<UINT32, PexSettings>::const_iterator SettingsIter =
        m_PexSnapshots[SessionId].GpuSettingList.find(devInst);
    if ((SettingsIter == m_PexSnapshots[SessionId].GpuSettingList.end()) ||
        (pSubdev->BusType() != Gpu::PciExpress))
    {
        // nothing to restore
        Printf(m_MsgLevel, "nothing to restore on Gpu %d\n", devInst);
        return OK;
    }

    Printf(m_MsgLevel, "---restore Gpu %d PEX setting----\n", devInst);
    const vector<PexParam> &ParamList = SettingsIter->second.List;
    PexDevice *pUpStreamDev;
    UINT32 DepthFromGpu;
    LwRmPtr pLwRm;
    LwRm::Handle RmHandle = pLwRm->GetSubdeviceHandle(pSubdev);
    auto pGpuPcie = pSubdev->GetInterface<Pcie>();

    if (WhatToRestore & REST_SPEED)
    {
        // make sure we can restore GPU speed first:
        bool GpuSupportsSpeedChange = false;
        if (pGpuPcie->LinkSpeedCapability() > Pci::Speed2500MBPS)
        {
            LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS PStatesInfo = { };
            CHECK_RC(LwRmPtr()->ControlBySubdevice(pSubdev,
                                                   LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO,
                                                   &PStatesInfo,
                                                   sizeof(PStatesInfo)));
            GpuSupportsSpeedChange = FLD_TEST_DRF(2080_CTRL_PERF,
                                           _GET_PSTATES_FLAGS,
                                           _PEXSPEED_CHANGE,
                                           _ENABLE,
                                           PStatesInfo.flags);
        }

        Pci::PcieLinkSpeed NewSpeed = ParamList[0].Speed;
        if (GpuSupportsSpeedChange)
        {
            // restore GPU link speed
            Printf(m_MsgLevel, " Gpu Speed = %d\n", NewSpeed);
            if (NewSpeed != pGpuPcie->GetLinkSpeed(Pci::SpeedUnknown))
            {
                LW2080_CTRL_BUS_SET_PCIE_SPEED_PARAMS param = {};
                switch (NewSpeed)
                {
                    case Pci::Speed2500MBPS:
                        param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_2500MBPS;
                        break;
                    case Pci::Speed5000MBPS:
                        param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_5000MBPS;
                        break;
                    case Pci::Speed8000MBPS:
                        param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_8000MBPS;
                        break;
                    case Pci::Speed16000MBPS:
                        param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_16000MBPS;
                        break;
                    case Pci::Speed32000MBPS:
                        param.busSpeed = LW2080_CTRL_BUS_SET_PCIE_SPEED_32000MBPS;
                        break;
                    default:
                        return RC::BAD_PARAMETER;
                }

                // Verify that the target PCI gen speed can be set at the
                // current PState. If not, pick the highest supported GEN speed.
                Perf* pPerf = pSubdev->GetPerf();
                if (pPerf->IsPState30Supported())
                {
                    UINT32 pstateNum;
                    CHECK_RC(pPerf->GetLwrrentPState(&pstateNum));
                    Perf::ClkRange pciGenRange;
                    CHECK_RC(pPerf->GetClockRange(pstateNum, Gpu::ClkPexGen,
                                                  &pciGenRange));
                    param.busSpeed = min<LwU32>(pciGenRange.MaxKHz,
                                                param.busSpeed);
                }

                CHECK_RC(pLwRm->Control(RmHandle, LW2080_CTRL_CMD_BUS_SET_PCIE_SPEED,
                                        &param, sizeof(param)));
            }
        }
        else
        {
            Printf(Tee::PriNormal, " Gpu Speed change not supported!\n");
        }

        // set link speed for rest of the link
        CHECK_RC(pGpuPcie->GetUpStreamInfo(&pUpStreamDev));
        DepthFromGpu = 1;
        while (pUpStreamDev && !pUpStreamDev->IsRoot())
        {
            NewSpeed = ParamList[DepthFromGpu].Speed;
            Printf(m_MsgLevel, " Pex ctrl speed = %d\n", (UINT32) NewSpeed);
            CHECK_RC(pUpStreamDev->ChangeUpStreamSpeed(NewSpeed));
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
            DepthFromGpu++;
        }
    }

    UINT32 PortId;
    if (WhatToRestore & REST_ASPM)
    {
        // restore ASPM settings on the link above GPU
        UINT32 State = ParamList[0].LocAspm;
        Printf(m_MsgLevel, " Gpu loc ASPM = %d\n", (UINT32) State);
        CHECK_RC(pGpuPcie->SetAspmState(State));
        CHECK_RC(pGpuPcie->GetUpStreamInfo(&pUpStreamDev, &PortId));
        if (pUpStreamDev)
        {
            State = ParamList[0].HostAspm;
            Printf(m_MsgLevel, " Gpu host ASPM = %d\n", (UINT32) State);
            CHECK_RC(pUpStreamDev->SetDownStreamAspmState(State, PortId, true));
        }
        // restore Azalia ASPM settings
        #if defined(INCLUDE_AZALIA)
        AzaliaController* pAza = pSubdev->GetAzaliaController();
        if (pAza)
        {
            Printf(m_MsgLevel, " Gpu aza ASPM = %d\n", (UINT32) ParamList[0].AzaAspm);
            CHECK_RC(pAza->SetAspmState(ParamList[0].AzaAspm));
        }
        #endif
        #if defined(INCLUDE_XUSB)
        if (pSubdev->SupportsInterface<XusbHostCtrl>())
        {
            auto pXusb = pSubdev->GetInterface<XusbHostCtrl>();
            Printf(m_MsgLevel, " Gpu xusb ASPM = %d\n", (UINT32) ParamList[0].XusbAspm);
            CHECK_RC(pXusb->SetAspmState(ParamList[0].XusbAspm));
        }
        if (pSubdev->SupportsInterface<PortPolicyCtrl>())
        {
            auto pPpc = pSubdev->GetInterface<PortPolicyCtrl>();
            Printf(m_MsgLevel, " Gpu ppc ASPM = %d\n", (UINT32) ParamList[0].PpcAspm);
            CHECK_RC(pPpc->SetAspmState(ParamList[0].PpcAspm));
        }
        #endif

        // set aspm for the rest of the devices
        DepthFromGpu = 1;
        while (pUpStreamDev && !pUpStreamDev->IsRoot())
        {
            State = ParamList[DepthFromGpu].LocAspm;
            Printf(m_MsgLevel, " PEX ctrl loc ASPM = %d\n", State);
            CHECK_RC(pUpStreamDev->SetUpStreamAspmState(State, true));
            PortId       = pUpStreamDev->GetUpStreamIndex();
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
            if (pUpStreamDev)
            {
                State = ParamList[DepthFromGpu].HostAspm;
                Printf(m_MsgLevel, " PEX ctrl host ASPM = %d\n", State);
                CHECK_RC(pUpStreamDev->SetDownStreamAspmState(State, PortId, true));
            }
            DepthFromGpu++;
        }
    }

    if (WhatToRestore & REST_ASPM_L1SS)
    {
        // restore ASPM L1 Substate settings on the link above GPU
        PexDevice::PciDevice downStreamPciDev;
        PexDevice::PciDevice *pDownStreamPciDev = nullptr;
        PexDevice::PciDevice *pUpStreamPciDev = nullptr;

        if (pGpuPcie->HasAspmL1Substates())
        {
            downStreamPciDev.Domain = pGpuPcie->DomainNumber();
            downStreamPciDev.Bus    = pGpuPcie->BusNumber();
            downStreamPciDev.Dev    = pGpuPcie->DeviceNumber();
            downStreamPciDev.Func   = pGpuPcie->FunctionNumber();
            downStreamPciDev.PcieL1ExtCapPtr = pGpuPcie->GetL1SubstateOffset();
            pDownStreamPciDev = &downStreamPciDev;
        }
        CHECK_RC(pGpuPcie->GetUpStreamInfo(&pUpStreamDev, &PortId));
        if (pUpStreamDev && pUpStreamDev->HasAspmL1Substates())
            pUpStreamPciDev = pUpStreamDev->GetDownStreamPort(PortId);

        CHECK_RC(SetAspmL1SubStateInt(pDownStreamPciDev,
                                      pUpStreamPciDev,
                                      ParamList[0].LocAspmL1SS,
                                      ParamList[0].HostAspmL1SS));
        if (pDownStreamPciDev != nullptr)
        {
            Printf(m_MsgLevel, " Gpu loc ASPM L1SS = %d\n",
                   (UINT32) ParamList[0].LocAspmL1SS);
        }
        if (pUpStreamPciDev != nullptr)
        {
            Printf(m_MsgLevel, " PEX ctrl host ASPM L1SS = %d\n",
                   (UINT32) ParamList[0].HostAspmL1SS);
        }

        // set aspm for the rest of the devices
        DepthFromGpu = 1;
        while (pUpStreamDev && !pUpStreamDev->IsRoot())
        {
            pDownStreamPciDev = nullptr;
            pUpStreamPciDev = nullptr;
            if (pUpStreamDev->HasAspmL1Substates())
                pDownStreamPciDev = pUpStreamDev->GetUpStreamPort();

            PortId       = pUpStreamDev->GetUpStreamIndex();
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
            if (pUpStreamDev && pUpStreamDev->HasAspmL1Substates())
                pUpStreamPciDev = pUpStreamDev->GetDownStreamPort(PortId);

            CHECK_RC(SetAspmL1SubStateInt(pDownStreamPciDev,
                                          pUpStreamPciDev,
                                          ParamList[DepthFromGpu].LocAspmL1SS,
                                          ParamList[DepthFromGpu].HostAspmL1SS));
            if (pDownStreamPciDev != nullptr)
            {
                Printf(m_MsgLevel, " PEX ctrl loc ASPM L1SS = %d\n",
                       (UINT32) ParamList[DepthFromGpu].LocAspmL1SS);
            }
            if (pUpStreamPciDev != nullptr)
            {
                Printf(m_MsgLevel, " PEX ctrl host ASPM L1SS = %d\n",
                       (UINT32) ParamList[DepthFromGpu].HostAspmL1SS);
            }
            DepthFromGpu++;
        }
    }

    if (WhatToRestore & REST_LINK_WIDTH)
    {
        if (pGpuPcie->IsASLMCapable())
        {
            LW2080_CTRL_BUS_SET_PCIE_LINK_WIDTH_PARAMS LwParam = { };
            LwParam.pcieLinkWidth = ParamList[0].Width;
            Printf(m_MsgLevel, " Gpu Width  = %d\n", ParamList[0].Width);
            CHECK_RC(pLwRm->Control(RmHandle,
                                    LW2080_CTRL_CMD_BUS_SET_PCIE_LINK_WIDTH,
                                    &LwParam, sizeof(LwParam)));

            // can't really change link width for BR04s right now.. need RM call
        }
    }

    if (WhatToRestore & REST_DPC)
    {
        // restore DPC settings on the link above GPU
        CHECK_RC(pGpuPcie->GetUpStreamInfo(&pUpStreamDev, &PortId));
        if (pUpStreamDev)
        {
            Printf(m_MsgLevel, " Gpu host DPC State = %s\n",
                   Pci::DpcStateToString(ParamList[0].DpcState).c_str());
            CHECK_RC(pUpStreamDev->SetDownStreamDpcState(ParamList[0].DpcState, PortId));
        }

        DepthFromGpu = 1;
        while (pUpStreamDev && !pUpStreamDev->IsRoot())
        {
            PortId       = pUpStreamDev->GetUpStreamIndex();
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
            if (pUpStreamDev)
            {
                Printf(m_MsgLevel, " PEX ctrl host DPC State = %s\n",
                       Pci::DpcStateToString(ParamList[DepthFromGpu].DpcState).c_str());
                CHECK_RC(pUpStreamDev->SetDownStreamDpcState(ParamList[DepthFromGpu].DpcState,
                                                             PortId));
            }
            DepthFromGpu++;
        }
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC PexDevMgr::IntRestorePexSetting(UINT32 sessionId,
                                   UINT32 whatToRestore,
                                   TestDevicePtr pTestDevice)
{
    RC rc;

    SCOPED_DEV_INST(pTestDevice.get());

    if (pTestDevice.get() == nullptr)
    {
        MASSERT(!"pTestDevice is null!\n");
        return RC::BAD_PARAMETER;
    }

    auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
    if (pSubdev)
    {
        return IntRestorePexSetting(sessionId, whatToRestore, pSubdev);
    }

    const UINT32 devInst = pTestDevice->DevInst();
    // assume session ID is valid since it was checked in RestorePexSetting
    map<UINT32, PexSettings>::const_iterator settingsIter =
        m_PexSnapshots[sessionId].LwSwitchSettingList.find(devInst);
    if (settingsIter == m_PexSnapshots[sessionId].LwSwitchSettingList.end())
    {
        // nothing to restore
        Printf(m_MsgLevel, "nothing to restore on %s\n", pTestDevice->GetName().c_str());
        return OK;
    }

    Printf(m_MsgLevel, "---restore %s PEX setting----\n", pTestDevice->GetName().c_str());
    const vector<PexParam>& paramList = settingsIter->second.List;

    PexDevice *pUpStreamDev = nullptr;
    UINT32 portId = ~0U;
    if (whatToRestore & REST_ASPM)
    {
        // restore ASPM settings on the link above LwSwitch
        UINT32 state = paramList[0].LocAspm;
        Printf(m_MsgLevel, " LwSwitch loc ASPM = %d\n", (UINT32)state);
        CHECK_RC(pTestDevice->GetInterface<Pcie>()->SetAspmState(state));
        CHECK_RC(pTestDevice->GetInterface<Pcie>()->GetUpStreamInfo(&pUpStreamDev, &portId));
        if (pUpStreamDev)
        {
            state = paramList[0].HostAspm;
            Printf(m_MsgLevel, " LwSwitch host ASPM = %d\n", (UINT32)state);
            CHECK_RC(pUpStreamDev->SetDownStreamAspmState(state, portId, true));
        }

        // set aspm for the rest of the devices
        for (UINT32 depthFromDev = 1; pUpStreamDev && !pUpStreamDev->IsRoot(); depthFromDev++)
        {
            state = paramList[depthFromDev].LocAspm;
            Printf(m_MsgLevel, " PEX ctrl loc ASPM = %d\n", (UINT32)state);
            CHECK_RC(pUpStreamDev->SetUpStreamAspmState(state, true));
            portId       = pUpStreamDev->GetUpStreamIndex();
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
            if (pUpStreamDev)
            {
                state = paramList[depthFromDev].HostAspm;
                Printf(m_MsgLevel, " PEX ctrl host ASPM = %d\n", (UINT32)state);
                CHECK_RC(pUpStreamDev->SetDownStreamAspmState(state, portId, true));
            }
        }
    }

    if (whatToRestore & REST_DPC)
    {
        CHECK_RC(pTestDevice->GetInterface<Pcie>()->GetUpStreamInfo(&pUpStreamDev, &portId));
        if (pUpStreamDev)
        {
            Printf(m_MsgLevel, " LwSwitch host DPC State = %s\n",
                   Pci::DpcStateToString(paramList[0].DpcState).c_str());
            CHECK_RC(pUpStreamDev->SetDownStreamDpcState(paramList[0].DpcState, portId));
        }

        for (UINT32 depthFromDev = 1; pUpStreamDev && !pUpStreamDev->IsRoot(); depthFromDev++)
        {
            portId       = pUpStreamDev->GetUpStreamIndex();
            pUpStreamDev = pUpStreamDev->GetUpStreamDev();
            if (pUpStreamDev)
            {
                Printf(m_MsgLevel, " PEX ctrl host DPC State = %s\n",
                       Pci::DpcStateToString(paramList[depthFromDev].DpcState).c_str());
                CHECK_RC(pUpStreamDev->SetDownStreamDpcState(paramList[depthFromDev].DpcState,
                                                             portId));
            }
        }
    }

    return rc;
}
//-----------------------------------------------------------------------------
// We can always local a 'link' in the PEX device tree given a device and the
// distance (depth) from the device.
// bool ForceIt: if this is true, we'll set the ASPM setting regardless of
// capability or the current state
// Normally, we only set it if it is actually supported
RC PexDevMgr::SetAspm(TestDevicePtr pTestDevice, // depth is relative to this device
                      UINT32 Depth,   // depth = 0 is link directly above the device
                      UINT32 LocAspm,
                      UINT32 HostAspm,
                      bool   ForceIt)
{
    RC rc;
    PexDevice *pPexCtrl;
    UINT32 PortId;
    auto pPcie = pTestDevice->GetInterface<Pcie>();
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexCtrl, &PortId));
    if ((pTestDevice->BusType() != Gpu::PciExpress) &&
        (pPexCtrl == NULL))
    {
        // not applicable - eg iGPU or Mac - no access to PCI read/write
        return OK;
    }

    UINT32 LocAspmCap;
    UINT32 LwrLocAspm;
    UINT32 HostAspmCap;
    UINT32 LwrHostAspm;
    if (Depth == 0)
    {
        if (!ForceIt)
        {
            LocAspmCap = pPcie->AspmCapability();
            LwrLocAspm = pPcie->GetAspmState();
            LocAspm = LwrLocAspm|(LocAspmCap & LocAspm);
            HostAspmCap = pPexCtrl->GetAspmCap();
            CHECK_RC(pPexCtrl->GetDownStreamAspmState(&LwrHostAspm, PortId));
            HostAspm = LwrHostAspm|(HostAspmCap & HostAspm);
        }
        CHECK_RC(pPcie->SetAspmState(LocAspm));
        CHECK_RC(pPexCtrl->SetDownStreamAspmState(HostAspm, PortId, ForceIt));
    }
    else
    {
        // traverse until reaching the desired PEX conroller
        for (UINT32 LwrDepth = 1; (LwrDepth < Depth) && (pPexCtrl != NULL); LwrDepth++)
        {
            pPexCtrl = pPexCtrl->GetUpStreamDev();
        }
        if ((pPexCtrl == NULL) || pPexCtrl->IsRoot())
        {
            Printf(Tee::PriNormal, "Depth is above root - impossible\n");
            return RC::BAD_PARAMETER;
        }
        PortId = pPexCtrl->GetUpStreamIndex();
        PexDevice * pUpStreamDev = pPexCtrl->GetUpStreamDev();
        if (!ForceIt)
        {
            LocAspmCap = pPexCtrl->GetAspmCap();
            CHECK_RC(pPexCtrl->GetUpStreamAspmState(&LwrLocAspm));
            LocAspm = LwrLocAspm|(LocAspmCap & LocAspm);
            HostAspmCap = pUpStreamDev->GetAspmCap();
            CHECK_RC(pUpStreamDev->GetDownStreamAspmState(&LwrHostAspm, PortId));
            HostAspm = LwrHostAspm|(HostAspmCap & HostAspm);
        }
        CHECK_RC(pPexCtrl->SetUpStreamAspmState(LocAspm, ForceIt));
        CHECK_RC(pUpStreamDev->SetDownStreamAspmState(HostAspm, PortId, ForceIt));
    }

    Printf(m_MsgLevel, "  Set depth %d at Dev %d with Loc %d, Host %d\n",
           Depth, pTestDevice->DevInst(), LocAspm, HostAspm);
    return rc;
}

//-----------------------------------------------------------------------------
// This should be the only method used to change ASPM L1SS since ASPM L1SS needs
// to follow specific rules when enabling or disabling on a link
//
// When disabling any ASPM L1 substate bits, the downstream device must be
// disabled before the upstream device
//
// When enabling any ASPM L1 substate bits, the upstream device must be
// enabled before the downstream device
//
// Reference section 7.xx.3 of:
// https://www.pcisig.com/specifications/pciexpress/specifications/ECN_L1_PM_Substates_with_CLKREQ_23_Aug_2012.pdf
//
// Also reference http://lwbugs/1389011
//
RC PexDevMgr::SetAspmL1SS(UINT32 testDevInst, // depth is relative to this test device
                          UINT32 Depth,       // depth = 0 is link directly above test device
                          UINT32 AspmL1SS)
{
    RC rc;
    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    CHECK_RC(pTestDeviceMgr->GetDevice(testDevInst, &pTestDevice));

    PexDevice *pPexDev = nullptr;
    UINT32 PortId;
    auto pPcie = pTestDevice->GetInterface<Pcie>();
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev, &PortId));
    if (pPexDev == nullptr)
    {
        // not applicable - eg iGPU or Mac - no access to PCI read/write
        return OK;
    }

    PexDevice::PciDevice downStreamPciDev;
    PexDevice::PciDevice *pUpstreamPciDev;
    UINT32 LocAspmL1SSCap  = Pci::PM_SUB_DISABLED;
    UINT32 HostAspmL1SSCap = Pci::PM_SUB_DISABLED;

    bool bLocHasAspmL1Substates;
    bool bHostHasAspmL1Substates;
    if (Depth == 0)
    {
        bLocHasAspmL1Substates  = pPcie->HasAspmL1Substates();
        if (bLocHasAspmL1Substates)
        {
            downStreamPciDev.Domain = pPcie->DomainNumber();
            downStreamPciDev.Bus    = pPcie->BusNumber();
            downStreamPciDev.Dev    = pPcie->DeviceNumber();
            downStreamPciDev.Func   = pPcie->FunctionNumber();
            downStreamPciDev.PcieL1ExtCapPtr = pPcie->GetL1SubstateOffset();
            CHECK_RC(pPcie->GetAspmL1SSCapability(&LocAspmL1SSCap));
        }
    }
    else
    {
        // traverse until reaching the desired PEX conroller
        for (UINT32 LwrDepth = 1; (LwrDepth < Depth) && (pPexDev != NULL); LwrDepth++)
        {
            pPexDev = pPexDev->GetUpStreamDev();
        }
        if ((pPexDev == nullptr) || pPexDev->IsRoot())
        {
            Printf(Tee::PriNormal, "Depth is above root - impossible\n");
            return RC::BAD_PARAMETER;
        }

        bLocHasAspmL1Substates  = pPexDev->HasAspmL1Substates();
        LocAspmL1SSCap  = pPexDev->GetAspmL1SSCap();
        PexDevice::PciDevice *pPciDev = pPexDev->GetUpStreamPort();
        if (nullptr == pPciDev)
            return RC::BAD_PARAMETER;
        downStreamPciDev = *pPciDev;

        PortId = pPexDev->GetUpStreamIndex();
        pPexDev = pPexDev->GetUpStreamDev();
    }

    bHostHasAspmL1Substates = pPexDev->HasAspmL1Substates();
    HostAspmL1SSCap = pPexDev->GetAspmL1SSCap();
    pUpstreamPciDev = pPexDev->GetDownStreamPort(PortId);
    if (nullptr == pUpstreamPciDev)
        return RC::BAD_PARAMETER;

    if (!bLocHasAspmL1Substates || !bHostHasAspmL1Substates)
    {
        Printf(Tee::PriError,
               "%s : Unable to change ASPM L11 Substate at depth %u\n"
               "     %s Allowed %s, Upstream PEX Device Allowed %s\n",
               __FUNCTION__, Depth,
               (Depth == 0) ? "Test Device" : "Local PEX Device",
               bLocHasAspmL1Substates ? "true" : "false",
               bHostHasAspmL1Substates ? "true" : "false");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (((HostAspmL1SSCap & AspmL1SS) != AspmL1SS) ||
        ((LocAspmL1SSCap & AspmL1SS) != AspmL1SS))
    {
        Printf(Tee::PriError,
               "%s : Unable to change ASPM L11 Substate at depth %u to 0x%x\n"
               "     %s Capable 0x%x, Upstream PEX Device Capable 0x%x\n",
               __FUNCTION__, Depth, AspmL1SS,
               (Depth == 0) ? "Test Device" : "Local PEX Device",
               LocAspmL1SSCap,
               HostAspmL1SSCap);
        return RC::CANNOT_SET_STATE;
    }

    CHECK_RC(SetAspmL1SubStateInt(&downStreamPciDev,
                                  pUpstreamPciDev,
                                  AspmL1SS,
                                  AspmL1SS));

    Printf(m_MsgLevel, "  Set ASPM L1SS at depth %d from Gpu %d with %d\n",
           Depth, testDevInst, AspmL1SS);
    return rc;
}

//-----------------------------------------------------------------------------
// internal
/* static */ RC PexDevMgr::SetAspmL1SubStateInt
(
    PexDevice::PciDevice *pPciDev,
    bool bDownStream,
    UINT32 state
)
{
    UINT32 origL1SSCtrl = 0;
    RC rc;
    CHECK_RC(Platform::PciRead32(pPciDev->Domain,
                                 pPciDev->Bus,
                                 pPciDev->Dev,
                                 pPciDev->Func,
                                 pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                 &origL1SSCtrl));
    UINT32 lwrL1SSCtrl  = origL1SSCtrl;

    // Program the scale and thresh values for L1.2 vs. L1.1 if they
    // do not already have valid values and L1.2 is being enabled.
    //
    // ASSUMPTION : If this is a downstream port, this assumes that the device
    // it is connected to (if any) has already disabled ASPM L1.2 and PCIPM L1.2
    // since if we disable it here and it is not disabled on the connected
    // device this can cause problems
    if ((Pci::PM_SUB_L12 & state) &&
        FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_ASPM, 0, lwrL1SSCtrl))
    {
        // When enabling L1.2 if any threshold changes are necessary, L1.2
        // must first be disabled (including PCIPM_L1_2).
        //
        // Reference section 7.xx.3 of:
        // https://www.pcisig.com/specifications/pciexpress/specifications/ECN_L1_PM_Substates_with_CLKREQ_23_Aug_2012.pdf
        lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_PCI_PM, 0, lwrL1SSCtrl);
        CHECK_RC(Platform::PciWrite32(pPciDev->Domain,
                                      pPciDev->Bus,
                                      pPciDev->Dev,
                                      pPciDev->Func,
                                      pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                      lwrL1SSCtrl));

        // Leave PowerOnScale/Value at either the hardware default of 10us
        // or whatever it was set to if already set
        //
        // If L1.2 thres==0 then program thresh/scale with default values
        // otherwise leave them alone
        if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _LTR_L12_THRESH_VALUE, 0, lwrL1SSCtrl))
        {
            lwrL1SSCtrl = FLD_SET_DRF(_PCI,
                                      _L1SS_CONTROL,
                                      _LTR_L12_THRESH_VALUE,
                                      _DEFAULT,
                                      lwrL1SSCtrl);
            lwrL1SSCtrl = FLD_SET_DRF(_PCI,
                                      _L1SS_CONTROL,
                                      _LTR_L12_THRESH_SCALE,
                                      _DEFAULT,
                                      lwrL1SSCtrl);
        }

        // Program CommonModeRestoreTime to max if not set (only relevent on
        // downstream ports)
        if (bDownStream &&
            FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _CMN_MODE_REST_TIME, 0, lwrL1SSCtrl))
        {
            lwrL1SSCtrl = FLD_SET_DRF(_PCI,
                                      _L1SS_CONTROL,
                                      _CMN_MODE_REST_TIME,
                                      _DEFAULT,
                                      lwrL1SSCtrl);
        }
        CHECK_RC(Platform::PciWrite32(pPciDev->Domain,
                                      pPciDev->Bus,
                                      pPciDev->Dev,
                                      pPciDev->Func,
                                      pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                      lwrL1SSCtrl));
    }

    UINT32 val = (state & Pci::PM_SUB_L12) ? 1 : 0;
    lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_ASPM, val, lwrL1SSCtrl);

    val = (state & Pci::PM_SUB_L11) ? 1 : 0;
    lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L11_ASPM, val, lwrL1SSCtrl);

    // Restore PCIPM_L1_2 enable which may have been disabled when changing
    // thresholds
    val = FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_PCI_PM, 1, origL1SSCtrl) ? 1 : 0;
    lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_PCI_PM, val, lwrL1SSCtrl);

    CHECK_RC(Platform::PciWrite32(pPciDev->Domain,
                                  pPciDev->Bus,
                                  pPciDev->Dev,
                                  pPciDev->Func,
                                  pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                  lwrL1SSCtrl));
    return rc;
}

//-----------------------------------------------------------------------------
// internal
/* static */ RC PexDevMgr::GetPciPmL1SubStateInt
(
    PexDevice::PciDevice *pPciDev,
    UINT32 *pState
)
{
    UINT32 l1SSCtrl = 0;
    RC rc;
    CHECK_RC(Platform::PciRead32(pPciDev->Domain,
                                 pPciDev->Bus,
                                 pPciDev->Dev,
                                 pPciDev->Func,
                                 pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                 &l1SSCtrl));
    if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_PCI_PM, 1, l1SSCtrl))
    {
        *pState |= Pci::PM_SUB_L12;
    }
    if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L11_PCI_PM, 1, l1SSCtrl))
    {
        *pState |= Pci::PM_SUB_L11;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// internal
/* static */ RC PexDevMgr::SetPciPmL1SubStateInt
(
    PexDevice::PciDevice *pPciDev,
    bool bDownStream,
    UINT32 state
)
{
    UINT32 origL1SSCtrl = 0;
    RC rc;
    CHECK_RC(Platform::PciRead32(pPciDev->Domain,
                                 pPciDev->Bus,
                                 pPciDev->Dev,
                                 pPciDev->Func,
                                 pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                 &origL1SSCtrl));
    UINT32 lwrL1SSCtrl  = origL1SSCtrl;

    // Program the scale and thresh values for L1.2 vs. L1.1 if they
    // do not already have valid values and L1.2 is being enabled.
    //
    // ASSUMPTION : If this is a downstream port, this assumes that the device
    // it is connected to (if any) has already disabled ASPM L1.2 and PCIPM L1.2
    // since if we disable it here and it is not disabled on the connected
    // device this can cause problems
    if ((Pci::PM_SUB_L12 & state) &&
        FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_PCI_PM, 0, lwrL1SSCtrl))
    {
        // When enabling L1.2 if any threshold changes are necessary, L1.2
        // must first be disabled (including ASPM_L1_2).
        //
        // Reference section 7.xx.3 of:
        // https://www.pcisig.com/specifications/pciexpress/specifications/ECN_L1_PM_Substates_with_CLKREQ_23_Aug_2012.pdf
        lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_ASPM, 0, lwrL1SSCtrl);
        CHECK_RC(Platform::PciWrite32(pPciDev->Domain,
                                      pPciDev->Bus,
                                      pPciDev->Dev,
                                      pPciDev->Func,
                                      pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                      lwrL1SSCtrl));

        // Leave PowerOnScale/Value at either the hardware default of 10us
        // or whatever it was set to if already set
        //
        // If L1.2 thres==0 then program thresh/scale with default values
        // otherwise leave them alone
        if (FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _LTR_L12_THRESH_VALUE, 0, lwrL1SSCtrl))
        {
            lwrL1SSCtrl = FLD_SET_DRF(_PCI,
                                      _L1SS_CONTROL,
                                      _LTR_L12_THRESH_VALUE,
                                      _DEFAULT,
                                      lwrL1SSCtrl);
            lwrL1SSCtrl = FLD_SET_DRF(_PCI,
                                      _L1SS_CONTROL,
                                      _LTR_L12_THRESH_SCALE,
                                      _DEFAULT,
                                      lwrL1SSCtrl);
        }

        // Program CommonModeRestoreTime to max if not set (only relevent on
        // downstream ports)
        if (bDownStream &&
            FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _CMN_MODE_REST_TIME, 0, lwrL1SSCtrl))
        {
            lwrL1SSCtrl = FLD_SET_DRF(_PCI,
                                      _L1SS_CONTROL,
                                      _CMN_MODE_REST_TIME,
                                      _DEFAULT,
                                      lwrL1SSCtrl);
        }
        CHECK_RC(Platform::PciWrite32(pPciDev->Domain,
                                      pPciDev->Bus,
                                      pPciDev->Dev,
                                      pPciDev->Func,
                                      pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                      lwrL1SSCtrl));
    }

    UINT32 val = (state & Pci::PM_SUB_L12) ? 1 : 0;
    lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_PCI_PM, val, lwrL1SSCtrl);

    val = (state & Pci::PM_SUB_L11) ? 1 : 0;
    lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L11_PCI_PM, val, lwrL1SSCtrl);

    // Restore ASPM_L1_2 enable which may have been disabled when changing
    // thresholds
    val = FLD_TEST_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_ASPM, 1, origL1SSCtrl) ? 1 : 0;
    lwrL1SSCtrl = FLD_SET_DRF_NUM(_PCI, _L1SS_CONTROL, _L12_ASPM, val, lwrL1SSCtrl);
    CHECK_RC(Platform::PciWrite32(pPciDev->Domain,
                                  pPciDev->Bus,
                                  pPciDev->Dev,
                                  pPciDev->Func,
                                  pPciDev->PcieL1ExtCapPtr + LW_PCI_L1SS_CONTROL,
                                  lwrL1SSCtrl));
    return rc;
}

/* static */ RC PexDevMgr::SetAspmL1SubStateInt
(
    PexDevice::PciDevice *pDownPciDev,
    PexDevice::PciDevice *pUpPciDev,
    UINT32 downState,
    UINT32 upState
)
{
    RC rc;

    // To meet enable/disable requirements, simply disable ASPM on the
    // downstream device before making any changes
    UINT32 downPciPmState = 0;
    if (pDownPciDev != nullptr)
    {
        CHECK_RC(GetPciPmL1SubStateInt(pDownPciDev, &downPciPmState));
        CHECK_RC(SetPciPmL1SubStateInt(pDownPciDev, false, Pci::PM_SUB_DISABLED));
        CHECK_RC(SetAspmL1SubStateInt(pDownPciDev, false, Pci::PM_SUB_DISABLED));
    }

    // Set the real values
    if (pUpPciDev != nullptr)
    {
        CHECK_RC(SetAspmL1SubStateInt(pUpPciDev, true, upState));
    }

    if (pDownPciDev != nullptr)
    {
        CHECK_RC(SetAspmL1SubStateInt(pDownPciDev, false, downState));
        CHECK_RC(SetPciPmL1SubStateInt(pDownPciDev, false, downPciPmState));
    }

    return rc;
}

PexDevMgr::PauseErrorCollectorThread::PauseErrorCollectorThread(PexDevMgr* pPexDevMgr)
: m_pPexDevMgr(pPexDevMgr)
{
    Cpu::AtomicAdd(&pPexDevMgr->m_ErrorCollectorPause, 1);
}

PexDevMgr::PauseErrorCollectorThread::~PauseErrorCollectorThread()
{
    Cpu::AtomicAdd(&m_pPexDevMgr->m_ErrorCollectorPause, -1);
}

//--------------------------- JS Interface ------------------------------------
//              Bridge Mgr
//-----------------------------------------------------------------------------
static JSClass JsPexDevMgr_class =
{
    "PexDevMgr",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    JS_FinalizeStub
};

//-----------------------------------------------------------------------------
static SObject JsPexDevMgr_Object
(
    "PexDevMgr",
    JsPexDevMgr_class,
    0,
    0,
    "PexDevMgr JS Object"
);
PROP_READONLY(JsPexDevMgr, ((PexDevMgr *)(DevMgrMgr::d_PexDevMgr)),
              NumDevices, UINT32, "Return the number of bridge/chipset devices");
PROP_READONLY(JsPexDevMgr, ((PexDevMgr *)(DevMgrMgr::d_PexDevMgr)),
              ChipsetASPMState, UINT32, "Return the chipset ASPM level" );
PROP_READONLY(JsPexDevMgr, ((PexDevMgr *)(DevMgrMgr::d_PexDevMgr)),
              ChipsetHasASPML1Substates, bool, "Return whether chipset has aspm l1 substates" );
PROP_READONLY(JsPexDevMgr, ((PexDevMgr *)(DevMgrMgr::d_PexDevMgr)),
              ChipsetASPML1SubState, UINT32, "Return the chipset ASPM l1 substate level" );
PROP_READWRITE(JsPexDevMgr, ((PexDevMgr *)(DevMgrMgr::d_PexDevMgr)),
               VerboseFlags, UINT32, "Pex verbosity flags");
PROP_READWRITE(JsPexDevMgr, ((PexDevMgr *)(DevMgrMgr::d_PexDevMgr)),
               ErrorPollingPeriodMs, UINT32, "How often PCIE errors are polled, in ms");
PROP_CONST(JsPexDevMgr, REST_SPEED, PexDevMgr::REST_SPEED);
PROP_CONST(JsPexDevMgr, REST_LINK_WIDTH, PexDevMgr::REST_LINK_WIDTH);
PROP_CONST(JsPexDevMgr, REST_ASPM, PexDevMgr::REST_ASPM);
PROP_CONST(JsPexDevMgr, REST_ASPM_L1SS, PexDevMgr::REST_ASPM_L1SS);
PROP_CONST(JsPexDevMgr, REST_DPC, PexDevMgr::REST_DPC);
PROP_CONST(JsPexDevMgr, REST_ALL, PexDevMgr::REST_ALL);

UINT32 PexDevMgr::GetVerboseFlags() const
{
    return s_VerboseFlags;
}

RC PexDevMgr::SetVerboseFlags(UINT32 verboseFlags)
{
    s_VerboseFlags = verboseFlags;
    if (s_VerboseFlags & (1 << PEX_VERBOSE_ENABLE))
        m_MsgLevel = Tee::PriNormal;
    else
        m_MsgLevel = Tee::PriLow;

    return OK;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDevMgr,
                RegisterLeafDevice,
                3,
                "Add PEX device to topology construction")
{
    STEST_HEADER(3, 3,
        "Usage: PexDevMgr.RegisterLeafDevice(domain, secBus, subBus)");
    STEST_ARG(0, UINT32, domain);
    STEST_ARG(1, UINT32, secBus);
    STEST_ARG(2, UINT32, subBus);

    RC rc;
    PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    MASSERT(pPexDevMgr);

    C_CHECK_RC(pPexDevMgr->RegisterLeafDevice(domain, secBus, subBus));

    RETURN_RC(RC::OK);
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDevMgr,
                  GetBridgeById,
                  1,
                  "Return a JsBridge with the given Id = (domain number << 16) | bus number")
{
    JavaScriptPtr pJs;
    UINT32 domainBusNum;
    const char usage[] = "Usage: PexDevMgr.GetBridgeById(BusNumber)";

    // Check the arguments.
    if ((1 != NumArguments) || (OK != pJs->FromJsval(pArguments[0], &domainBusNum)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    if (DevMgrMgr::d_PexDevMgr == 0)
    {
        JS_ReportError(pContext,
                       "d_PexDevMgr is not initialized.");
        return JS_FALSE;
    }

    RC rc;
    PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    PexDevice* pPexDev = NULL;
    if ((pPexDevMgr->GetDevice(domainBusNum, (Device**)&pPexDev) != OK) ||
        !pPexDev)
    {
        if (pJs->ToJsval((JSObject *)NULL, pReturlwalue) != OK)
        {
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    else
    {
        JsPexDev * pJsPexDev = new JsPexDev();
        MASSERT(pJsPexDev);
        pJsPexDev->SetPexDev(pPexDev);
        C_CHECK_RC(pJsPexDev->CreateJSObject(pContext, pObject));

        if (pJs->ToJsval(pJsPexDev->GetJSObject(), pReturlwalue) != OK)
        {
            delete pJsPexDev;
            JS_ReportError(pContext, usage);
            return JS_FALSE;
        }
    }
    return JS_TRUE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDevMgr,
                PrintTopology,
                0,
                "print out all bridge/Gpu upstream/downstream info")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: PexDevMgr.PrintPexDeviceInfo()";
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    pPexDevMgr->PrintPexDeviceInfo();

    RETURN_RC(OK);
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDevMgr,
                DumpPexInfo,
                3,
                "Dump PEX register info for a device")
{
    STEST_HEADER(3, 3,
        "Usage: PexDevMgr.DumpPexInfo(testDevice, pexVerbose, printOnceRegs)");
    STEST_PRIVATE_ARG(0, JsTestDevice, pJsTestDevice, "TestDevice");
    STEST_ARG(1, UINT32,    pexVerbose);
    STEST_ARG(2, bool,      printOnceRegs);

    RC rc;
    TestDevicePtr pTestDevice = pJsTestDevice->GetTestDevice();
    if (pTestDevice == nullptr)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    C_CHECK_RC(PexDevMgr::DumpPexInfo(pTestDevice.get(),
                                      pexVerbose, printOnceRegs));
    RETURN_RC(RC::OK);
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDevMgr,
                SavePexSetting,
                0,
                "Save PEX settings")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: PexDevMgr.SavePexSetting(RetSessionId)";
    JavaScriptPtr pJs;
    JSObject * pArrayToReturn;
    *pReturlwalue = JSVAL_NULL;
    UINT32 WhatToSave;

    if ((2 != NumArguments) ||
        (OK != pJs->FromJsval(pArguments[0], &pArrayToReturn)) ||
        (OK != pJs->FromJsval(pArguments[1], &WhatToSave)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    PexDevMgr* pBrdgMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    UINT32 SessionId;
    RC rc;
    C_CHECK_RC(pBrdgMgr->SavePexSetting(&SessionId, WhatToSave));
    RETURN_RC(pJs->SetElement(pArrayToReturn, 0, SessionId));
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDevMgr,
                RestorePexSetting,
                3,
                "Restore PEX setting given session ID")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: PexDevMgr.RestorePexSetting(Session, WhatToRestore, RemoveSession)";
    UINT32 SessionId;
    UINT32 WhatToRestore;
    bool   RemoveSession;
    JavaScriptPtr pJs;
    if ((3 != NumArguments) ||
        (OK != pJs->FromJsval(pArguments[0], &SessionId)) ||
        (OK != pJs->FromJsval(pArguments[1], &WhatToRestore)) ||
        (OK != pJs->FromJsval(pArguments[2], &RemoveSession)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    PexDevMgr* pBrdgMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    RETURN_RC(pBrdgMgr->RestorePexSetting(SessionId,
                                          WhatToRestore,
                                          RemoveSession));
}
JS_SMETHOD_LWSTOM(JsPexDevMgr,
                  DoesSessionIdExist,
                  1,
                  "Does the PEX session ID Eixst")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDevMgr.DoesSessionIdExist(Id)";
    UINT32 Id;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Id)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    PexDevMgr* pBrdgMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    MASSERT(pBrdgMgr);
    bool SessionExist = pBrdgMgr->DoesSessionIdExist(Id);
    if (pJS->ToJsval(SessionExist, pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDevMgr,
                SetAspmL1SS,
                3,
                "Set ASPM L1SS at a particular depth from the GPU")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: PexDevMgr.SetAspmL1SS(TestDeviceInst, Depth, ASPML1SS)";

    UINT32 testDeviceInst;
    UINT32 depth;
    UINT32 aspmL1SS;
    JavaScriptPtr pJs;
    if ((3 != NumArguments) ||
        (OK != pJs->FromJsval(pArguments[0], &testDeviceInst)) ||
        (OK != pJs->FromJsval(pArguments[1], &depth)) ||
        (OK != pJs->FromJsval(pArguments[2], &aspmL1SS)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
    RETURN_RC(pPexDevMgr->SetAspmL1SS(testDeviceInst, depth, aspmL1SS));
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDevMgr,
                GetChipsetLTREnabled,
                1,
                "Retrieve whether LTR is enabled on the chipset")
{
    STEST_HEADER(1, 1, "Usage: PexDevMgr.GetChipsetLTREnabled(object)\n");
    STEST_ARG(0, JSObject *, pLTR);

    RC rc;

    auto pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    pTestDeviceMgr->GetDevice(0, &pTestDevice);
    MASSERT(pTestDevice.get() != nullptr);

    PexDevice* pPexDev = nullptr;
    UINT32 portId = ~0U;

    // Get root device (chipset)
    C_CHECK_RC(pTestDevice->GetInterface<Pcie>()->GetUpStreamInfo(&pPexDev, &portId));
    while (pPexDev && !pPexDev->IsRoot())
    {
        portId  = pPexDev->GetUpStreamIndex();
        pPexDev = pPexDev->GetUpStreamDev();
    }
    if (!pPexDev)
    {
        RETURN_RC(RC::PCI_DEVICE_NOT_FOUND);
    }

    // Retrieve whether LTR is enabled
    bool isEnabled;
    C_CHECK_RC(pPexDev->GetDownstreamPortLTR(portId, &isEnabled));
    C_CHECK_RC(pJavaScript->SetProperty(pLTR, "isEnabled", isEnabled));
    RETURN_RC(rc);
}

//-----------------------------------------------------------------------------
//              JS Bridge Device
//-----------------------------------------------------------------------------

static void C_JsBridge_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    MASSERT(cx != 0);
    MASSERT(obj != 0);

    JsPexDev * pJsPexDev;
    //! Delete the C++
    pJsPexDev = (JsPexDev *)JS_GetPrivate(cx, obj);
    delete pJsPexDev;
};
//-----------------------------------------------------------------------------
static JSClass JsPexDev_class =
{
    "PexDev",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_JsBridge_finalize
};

//-----------------------------------------------------------------------------
static SObject JsPexDev_Object
(
    "PexDev",
    JsPexDev_class,
    0,
    0,
    "PexDev JS Object"
);

JsPexDev::JsPexDev()
    : m_pPexDev(NULL), m_pJsPexDevObj(NULL)
{
}

//------------------------------------------------------------------------------
//! \brief Create a JS Object representation of the current associated
//! Bridge object
RC JsPexDev::CreateJSObject(JSContext *cx, JSObject *obj)
{
    //! Only create one JSObject per Perf object
    if (m_pJsPexDevObj)
    {
        Printf(Tee::PriLow,
               "A JS Object has already been created for this JsBridgeDev.\n");
        return OK;
    }

    m_pJsPexDevObj = JS_DefineObject(cx,
                                  obj, // PexDevice object
                                  "PexDev", // Property name
                                  &JsPexDev_class,
                                  JsPexDev_Object.GetJSObject(),
                                  JSPROP_READONLY);

    if (!m_pJsPexDevObj)
        return RC::COULD_NOT_CREATE_JS_OBJECT;

    //! Store the current JsBridge instance into the private area
    //! of the new JSOBject.
    if (JS_SetPrivate(cx, m_pJsPexDevObj, this) != JS_TRUE)
    {
        Printf(Tee::PriNormal,
               "Unable to set private value of JsPexDev.\n");
        return RC::COULD_NOT_CREATE_JS_OBJECT;
    }

    // Add the .Help() to this JSObject
    JS_DefineFunction(cx, m_pJsPexDevObj, "Help", &C_Global_Help, 1, 0);

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Return the private JS data - C++ Bridge object.
//!
PexDevice * JsPexDev::GetPexDev()
{
    MASSERT(m_pPexDev);
    return m_pPexDev;
}

//------------------------------------------------------------------------------
//! \brief Set the associated Perf object.
//!
//! This is called by JS Initialize
void JsPexDev::SetPexDev(PexDevice * pPexDev)
{
    MASSERT(pPexDev);
    m_pPexDev = pPexDev;
}

//------------------------------------------------------------------------------
bool JsPexDev::GetIsRoot()
{
    return GetPexDev()->IsRoot();
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetUpStreamDomain()
{
    PexDevice* pPexDev = GetPexDev();
    if (pPexDev->IsRoot())
    {
        Printf(Tee::PriNormal, "Error! Chipset has no upstream port\n");
        return 0;
    }
    PexDevice::PciDevice* pPciDev = pPexDev->GetUpStreamPort();
    MASSERT(pPciDev);
    return pPciDev->Domain;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetUpStreamBus()
{
    PexDevice* pPexDev = GetPexDev();
    if (pPexDev->IsRoot())
    {
        Printf(Tee::PriNormal, "Error! Chipset has no upstream port\n");
        return 0;
    }
    PexDevice::PciDevice* pPciDev = pPexDev->GetUpStreamPort();
    MASSERT(pPciDev);
    return pPciDev->Bus;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetUpStreamDev()
{
    PexDevice* pPexDev = GetPexDev();
    if (pPexDev->IsRoot())
    {
        Printf(Tee::PriNormal, "Error! Chipset has no upstream port\n");
        return 0;
    }
    PexDevice::PciDevice* pPciDev = pPexDev->GetUpStreamPort();
    MASSERT(pPciDev);
    return pPciDev->Dev;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetUpStreamFunc()
{
    PexDevice* pPexDev = GetPexDev();
    if (pPexDev->IsRoot())
    {
        Printf(Tee::PriNormal, "Error! Chipset has no upstream port\n");
        return 0;
    }
    PexDevice::PciDevice* pPciDev = pPexDev->GetUpStreamPort();
    MASSERT(pPciDev);
    return pPciDev->Func;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetDownStreamDomain(UINT32 Index)
{
    PexDevice::PciDevice* pPciDev = GetPexDev()->GetDownStreamPort(Index);
    if (!pPciDev)
        return 0xFFFFFFFF;

    return pPciDev->Domain;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetDownStreamBus(UINT32 Index)
{
    PexDevice::PciDevice* pPciDev = GetPexDev()->GetDownStreamPort(Index);
    if (!pPciDev)
        return 0xFFFFFFFF;

    return pPciDev->Bus;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetDownStreamDev(UINT32 Index)
{
    PexDevice::PciDevice* pPciDev = GetPexDev()->GetDownStreamPort(Index);
    if (!pPciDev)
        return 0xFFFFFFFF;

    return pPciDev->Dev;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetDownStreamFunc(UINT32 Index)
{
    PexDevice::PciDevice* pPciDev = GetPexDev()->GetDownStreamPort(Index);
    if (!pPciDev)
        return 0xFFFFFFFF;

    return pPciDev->Func;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetUpStreamSpeed()
{
    PexDevice* pPexDev = GetPexDev();
    if (pPexDev->IsRoot())
    {
        Printf(Tee::PriNormal, "Error! Chipset has no upstream port\n");
        return Pci::SpeedUnknown;
    }
    return (UINT32)pPexDev->GetUpStreamSpeed();
}

//------------------------------------------------------------------------------
UINT32 JsPexDev::GetUpStreamAspmState()
{
    PexDevice* pPexDev = GetPexDev();
    if (pPexDev->IsRoot())
    {
        Printf(Tee::PriNormal, "Chipset has no upstream port\n");
        return Pci::ASPM_UNKNOWN_STATE;
    }
    UINT32 aspmState;
    if (pPexDev->GetUpStreamAspmState(&aspmState) != RC::OK)
    {
        Printf(Tee::PriNormal, "Failed to get upstream ASPM state\n");
        return Pci::ASPM_UNKNOWN_STATE;
    }
    return aspmState;
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetNumDPorts()
{
    return GetPexDev()->GetNumDPorts();
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetNumActiveDPorts()
{
    return GetPexDev()->GetNumActiveDPorts();
}
//------------------------------------------------------------------------------
UINT32 JsPexDev::GetDepth()
{
    return GetPexDev()->GetDepth();
}
UINT32 JsPexDev::GetUpStreamIndex()
{
    return GetPexDev()->GetUpStreamIndex();
}
UINT32 JsPexDev::GetAspmCap()
{
    return GetPexDev()->GetAspmCap();
}
bool JsPexDev::GetHasAspmL1Substates()
{
    return GetPexDev()->HasAspmL1Substates();
}
Pci::PcieLinkSpeed JsPexDev::GetDownStreamSpeed(UINT32 port)
{
    return GetPexDev()->GetDownStreamSpeed(port);
}
RC JsPexDev::SetDownStreamSpeed(UINT32 port, Pci::PcieLinkSpeed speed)
{
    return GetPexDev()->SetDownStreamSpeed(port, speed);
}

//-----------------------------------------------------------------------------
//! \brief Colwert PexErrorCounts to a JS array object
//!
//! \param pJsErrorCounts : Pointer to returned JS array object
//! \param pErrors        : Pointer to error counts to colwert
//!
//! \return OK if successful, not OK otherwise
//!
RC JsPexDev::PexErrorCountsToJs(JSObject       *pJsErrorCounts,
                                PexErrorCounts *pErrors)
{
    RC rc;
    JavaScriptPtr pJavaScript;

    for (UINT32 countIdx = 0; countIdx < PexErrorCounts::NUM_ERR_COUNTERS;
          countIdx++)
    {
        PexErrorCounts::PexErrCountIdx errIdx =
            static_cast<PexErrorCounts::PexErrCountIdx>(countIdx);
        CHECK_RC(pJavaScript->SetElement(pJsErrorCounts,
                                         countIdx,
                                         pErrors->GetCount(errIdx)));
    }

    JsArray laneErrorsJsArray;
    for (UINT32 i = 0; i < LW2080_CTRL_PEX_MAX_LANES; ++i)
    {
        jsval tmp;
        CHECK_RC(pJavaScript->ToJsval(pErrors->GetSingleLaneErrorCount(i), &tmp));
        laneErrorsJsArray.push_back(tmp);
    }

    jsval laneErrorsJsVal;
    CHECK_RC(pJavaScript->ToJsval(&laneErrorsJsArray, &laneErrorsJsVal));

    CHECK_RC(pJavaScript->SetPropertyJsval(pJsErrorCounts, "LaneErrors",
                                           laneErrorsJsVal));

    CHECK_RC(pJavaScript->SetProperty(pJsErrorCounts, "ValidMask",
                                      pErrors->GetValidMask()));
    CHECK_RC(pJavaScript->SetProperty(pJsErrorCounts, "HwCounterMask",
                                      pErrors->GetHwCountMask()));
    CHECK_RC(pJavaScript->SetProperty(pJsErrorCounts, "ThresholdMask",
                                      pErrors->GetThresholdMask()));
    return rc;
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::AerLogToJs
(
    JSContext *pContext
   ,jsval *pRetVal
   ,const vector<PexDevice::AERLogEntry> &logData
)
{
    RC rc;
    JavaScriptPtr pJs;
    JsArray jsLogData;
    for (size_t logIdx = 0; logIdx < logData.size(); ++logIdx)
    {
        JSObject *pLogObj = JS_NewObject(pContext, nullptr, nullptr, nullptr);
        CHECK_RC(pJs->SetProperty(pLogObj, "ErrorMask", logData[logIdx].errMask));

        JsArray aerDataJsArray;
        for (size_t ii = 0; ii < logData[logIdx].aerData.size(); ++ii)
        {
            jsval tmp;
            CHECK_RC(pJs->ToJsval(logData[logIdx].aerData[ii], &tmp));
            aerDataJsArray.push_back(tmp);
        }

        jsval aerDataJsVal;
        CHECK_RC(pJs->ToJsval(&aerDataJsArray, &aerDataJsVal));
        CHECK_RC(pJs->SetPropertyJsval(pLogObj, "AerData", aerDataJsVal));

        jsval aerLogEntry;
        CHECK_RC(pJs->ToJsval(pLogObj, &aerLogEntry));
        jsLogData.push_back(aerLogEntry);
    }

    CHECK_RC(pJs->ToJsval(&jsLogData, pRetVal));
    return rc;
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::GetOnAerLogFull(UINT32 *pOnFull)
{
    return PexDevice::GetOnAerLogFull(pOnFull);
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::SetOnAerLogFull(UINT32 onFull)
{
    return PexDevice::SetOnAerLogFull(onFull);
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::GetAerLogMaxEntries(UINT32 *pMaxEntries)
{
    return PexDevice::GetAerLogMaxEntries(pMaxEntries);
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::SetAerLogMaxEntries(UINT32 maxEntries)
{
    return PexDevice::SetAerLogMaxEntries(maxEntries);
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::GetAerUEStatusMaxClearCount(UINT32 *pMaxCount)
{
    return PexDevice::GetAerUEStatusMaxClearCount(pMaxCount);
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::SetAerUEStatusMaxClearCount(UINT32 maxCount)
{
    return PexDevice::SetAerUEStatusMaxClearCount(maxCount);
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::GetSkipHwCounterInit(bool *pSkipInit)
{
    return PexDevice::GetSkipHwCounterInit(pSkipInit);
}

//-----------------------------------------------------------------------------
/* static */ RC JsPexDev::SetSkipHwCounterInit(bool skipInit)
{
    return PexDevice::SetSkipHwCounterInit(skipInit);
}

CLASS_PROP_READONLY(JsPexDev, IsRoot, bool,
                    "return if the device is chipset");
CLASS_PROP_READONLY(JsPexDev, UpStreamDomain, UINT32,
                    "get domain number of upstream port");
CLASS_PROP_READONLY(JsPexDev, UpStreamBus, UINT32,
                    "get bus number of upstream port");
CLASS_PROP_READONLY(JsPexDev, UpStreamDev, UINT32,
                    "get device number of upstream port");
CLASS_PROP_READONLY(JsPexDev, UpStreamFunc, UINT32,
                    "get function number of upstream port");
CLASS_PROP_READONLY(JsPexDev, NumDPorts, UINT32,
                    "get number of downstream ports");
CLASS_PROP_READONLY(JsPexDev, NumActiveDPorts, UINT32,
                    "get number of downstream ports that are connected to another dev");
CLASS_PROP_READONLY(JsPexDev, Depth, UINT32,
                    "get depth of current bridge - distance from chipset");
CLASS_PROP_READONLY(JsPexDev, UpStreamSpeed, UINT32,
                    "get linkspeed of upstream port");
CLASS_PROP_READONLY(JsPexDev, UpStreamAspmState, UINT32,
                    "get aspm state of upstream port");
CLASS_PROP_READONLY(JsPexDev, UpStreamIndex, UINT32,
                    "get upstream device's port index at which current device is connected");
CLASS_PROP_READONLY(JsPexDev, AspmCap, UINT32,
                    "get ASPM capability mask of the current device");
CLASS_PROP_READONLY(JsPexDev, HasAspmL1Substates, bool,
                    "get whether the device has ASPM substates");

// Class static member functions can be treated as a namespace
PROP_READWRITE_NAMESPACE(JsPexDev, OnAerLogFull, UINT32,
                               "AER log behavior when full (DELETE_OLD or IGNORE_NEW");
PROP_READWRITE_NAMESPACE(JsPexDev, AerLogMaxEntries, UINT32,
                               "Maximum number of entries in the AER log");
PROP_READWRITE_NAMESPACE(JsPexDev, AerUEStatusMaxClearCount, UINT32,
                               "Maximum number of AER UE status bits to be cleared at once");
PROP_READWRITE_NAMESPACE(JsPexDev, SkipHwCounterInit, bool,
                               "Skip initializing HW error counters");

//-----------------------------------------------------------------------------
// not using CLASS_PROP_READONLY here because we're creating a new JSBridge and
// JS object. We need pContext and pObject parameters
P_( JsPexDev_Get_UpStreamPexDev);
static SProperty UpStreamPexDev
(
    JsPexDev_Object,
    "UpStreamPexDev",
    0,
    0,
    JsPexDev_Get_UpStreamPexDev,
    0,
    JSPROP_READONLY,
    "acquire the Pex device above - null if nothing"
);
P_( JsPexDev_Get_UpStreamPexDev )
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    JavaScriptPtr pJs;
    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev()->GetUpStreamDev();
        if (!pPexDev)
        {
            *pValue = JSVAL_NULL;
            return JS_TRUE;
        }
        JsPexDev * pJsPexDev = new JsPexDev();
        MASSERT(pJsPexDev);
        pJsPexDev->SetPexDev(pPexDev);
        if (pJsPexDev->CreateJSObject(pContext, pObject) != OK)
        {
            delete pJsPexDev;
            JS_ReportError(pContext, "Unable to create JsPexDev JSObject");
            *pValue = JSVAL_NULL;
            return JS_FALSE;
        }

        if (pJs->ToJsval(pJsPexDev->GetJSObject(), pValue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamAspmState,
                  1,
                  "get ASPM state of a downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamAspmState(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        UINT32 State = Pci::ASPM_UNKNOWN_STATE;
        pPexDev->GetDownStreamAspmState(&State, Index);
        if (pJS->ToJsval(State, pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamAspmL1SubState,
                  1,
                  "get ASPM L1 sub state of a downstream port")
{
    STEST_HEADER(1, 1, "Usage: PexDev.GetDownStreamAspmL1SubState(DownStreamIndex)\n");
    STEST_PRIVATE(JsPexDev, pJsPexDev, "PexDev");
    STEST_ARG(0, UINT32, Index);

    PexDevice* pPexDev = pJsPexDev->GetPexDev();
    UINT32 State = Pci::PM_UNKNOWN_SUB_STATE;
    pPexDev->GetDownStreamAspmL1SubState(&State, Index);
    if (pJavaScript->ToJsval(State, pReturlwalue) == OK)
    {
        return JS_TRUE;
    }

    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetAspmCap,
                  1,
                  "get ASPM capability")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetAspmCap()";

    static Deprecation depr
    (
        "JsPexDev.GetAspmCap",
        "9/15/2019",
        "JsPexDev.GetAspmCap() is no longer supported, use JsPexDev.AspmCap instead\n"
    );

    if (NumArguments > 1)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JavaScriptPtr pJS;
    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        UINT32 Cap = pPexDev->GetAspmCap();
        if (pJS->ToJsval(Cap, pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetAspmL1SSCap,
                  0,
                  "get ASPM L1 Substate capability")
{
    STEST_HEADER(0, 0, "Usage: PexDev.GetAspmL1SSCap()\n");
    STEST_PRIVATE(JsPexDev, pJsPexDev, "PexDev");
    PexDevice* pPexDev = pJsPexDev->GetPexDev();
    if (pJavaScript->ToJsval(pPexDev->GetAspmL1SSCap(), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                SetDownStreamAspmState,
                2,
                "Set ASPM state of a downstream port")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: PexDev.SetDownStreamAspmState(Pci.ASPM_L0S, DownStreamPort [, bForce])";

    UINT32 State;
    UINT32 Port;
    bool bForce = false;
    JavaScriptPtr pJS;
    if ((NumArguments == 0) || (NumArguments > 3) ||
        (OK != pJS->FromJsval(pArguments[0], &State)) ||
        (OK != pJS->FromJsval(pArguments[1], &Port)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ((NumArguments == 3) && (OK != pJS->FromJsval(pArguments[2], &bForce)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        RETURN_RC(pPexDev->SetDownStreamAspmState(State, Port, bForce));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                SetUpStreamAspmState,
                1,
                "Set ASPM state of a upstream port")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: PexDev.SetUpStreamAspmState(Pci.ASPM_L0S [, bForce])";
    UINT32 State;
    bool bForce = false;
    JavaScriptPtr pJS;
    if ((NumArguments == 0) || (NumArguments > 2) ||
        (OK != pJS->FromJsval(pArguments[0], &State)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ((NumArguments == 2) && (OK != pJS->FromJsval(pArguments[1], &bForce)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        RETURN_RC(pPexDev->SetUpStreamAspmState(State, bForce));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                SetDownStreamDpcState,
                2,
                "Set DPC state of a downstream port")
{
    STEST_HEADER(2, 2, "Usage: PexDev.SetDownStreamDpcState(state, port)\n");
    STEST_PRIVATE(JsPexDev, pJsPexDev, "PexDev");
    STEST_ARG(0, UINT32, state);
    STEST_ARG(1, UINT32, port);

    RETURN_RC(pJsPexDev->GetPexDev()->SetDownStreamDpcState(static_cast<Pci::DpcState>(state),
                                                            port));
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamDomain,
                  1,
                  "get domain number of downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamDomain(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        if (pJS->ToJsval(pJsPexDev->GetDownStreamDomain(Index), pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamBus,
                  1,
                  "get bus number of downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamBus(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        if (pJS->ToJsval(pJsPexDev->GetDownStreamBus(Index), pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamDev,
                  1,
                  "get device number of downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamDev(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        if (pJS->ToJsval(pJsPexDev->GetDownStreamDev(Index), pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamFunc,
                  1,
                  "get function number of downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamFunc(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        if (pJS->ToJsval(pJsPexDev->GetDownStreamFunc(Index), pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                SetUpStreamSpeed,
                1,
                "Set the link speed of upstream port")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: PexDev.SetUpStreamSpeed(2500/5000)";

    UINT32 Speed;
    JavaScriptPtr pJS;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Speed)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        RETURN_RC(pPexDev->ChangeUpStreamSpeed((Pci::PcieLinkSpeed)Speed));
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                GetUpStreamPcieErrors,
                1,
                "get the PEX errors on the upstream link")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJavaScript;
    JSObject * pArrayToReturn;
    *pReturlwalue = JSVAL_NULL;
    const char usage[] = "Usage: PexDev.GetUpStreamPcieErrors(ErrorArray)";

    JavaScriptPtr pJS;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &pArrayToReturn)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        RC rc;
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        Pci::PcieErrorsMask LocErrors, HostErrors;
        C_CHECK_RC(pPexDev->GetUpStreamErrors(&LocErrors, &HostErrors));
        C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, LocErrors));
        RETURN_RC(pJavaScript->SetElement(pArrayToReturn, 1, HostErrors));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                GetUpStreamPcieErrorCounts,
                3,
                "get the PEX errors on the upstream link")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: PexDev.GetUpStreamPcieErrorCounts(LocErrors, "
                         "HostErrors, CountType)";
    JavaScriptPtr pJavaScript;
    JSObject * pRetLocErrors;
    JSObject * pRetHostErrors;
    *pReturlwalue = JSVAL_NULL;
    if (((NumArguments != 2) && (NumArguments != 3)) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pRetLocErrors) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &pRetHostErrors))))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    UINT32 temp = PexDevice::PEX_COUNTER_ALL;
    if ((NumArguments == 3) &&
        (OK != pJavaScript->FromJsval(pArguments[2], &temp)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        RC rc;
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        PexErrorCounts LocErrors;
        PexErrorCounts HostErrors;
        C_CHECK_RC(pPexDev->GetUpStreamErrorCounts(&LocErrors,
                                &HostErrors,
                                static_cast<PexDevice::PexCounterType>(temp)));
        C_CHECK_RC(JsPexDev::PexErrorCountsToJs(pRetLocErrors, &LocErrors));
        RETURN_RC(JsPexDev::PexErrorCountsToJs(pRetHostErrors, &HostErrors));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                GetUpStreamLinkWidths,
                1,
                "get the PEX link width on the upstream link")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJavaScript;
    JSObject * pArrayToReturn;
    *pReturlwalue = JSVAL_NULL;
    const char usage[] = "Usage: PexDev.GetUpStreamLinkWidth(Width Array)";

    JavaScriptPtr pJS;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &pArrayToReturn)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        RC rc;
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        UINT32 LocWidth, HostWidth;
        C_CHECK_RC(pPexDev->GetUpStreamLinkWidths(&LocWidth, &HostWidth));
        C_CHECK_RC(pJavaScript->SetElement(pArrayToReturn, 0, LocWidth));
        RETURN_RC(pJavaScript->SetElement(pArrayToReturn, 1, HostWidth));
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                PrintInfo,
                0,
                "print out PexDev upstream/downstream info")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: PexDev.PrintPexDevInfo()";
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        pJsPexDev->GetPexDev()->PrintInfo(Tee::PriNormal);
        RETURN_RC(OK);
    }
    return JS_FALSE;
}
//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                PrintRom,
                0,
                "print out contents of ROM")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    const char usage[] = "Usage: PexDev.PrintRom()";
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }
    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        RC rc;
        UINT08 *pBuffer = new UINT08[PexDevice::BR04_ROMSIZE];
        rc = pJsPexDev->GetPexDev()->GetRom(pBuffer, PexDevice::BR04_ROMSIZE);
        if (rc == OK)
        {
            Printf(Tee::PriNormal, "ROM content:");
            for (UINT32 i = 0; i < PexDevice::BR04_ROMSIZE; i++)
            {
                if (!(i % 16))
                {
                    Printf(Tee::PriNormal, "\n");
                }
                Printf(Tee::PriNormal, "0x%02x ", pBuffer[i]);
            }
            Printf(Tee::PriNormal, "\n");
        }
        delete [] pBuffer;
        RETURN_RC(rc);
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                PrintDownStreamExtCapIdBlock,
                2,
                "Print the contents of an extended capability "
                "block on a downstream port")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    UINT32 capId;
    UINT32 downStreamPort;
    *pReturlwalue = JSVAL_NULL;
    const char usage[] =
        "Usage: PexDev.PrintDownStreamExtCapIdBlock(CapId, DownStreamIndex)";

    JavaScriptPtr pJS;
    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &capId)) ||
        (OK != pJS->FromJsval(pArguments[1], &downStreamPort)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        RETURN_RC(pJsPexDev->GetPexDev()->PrintDownStreamExtCapIdBlock(
                        Tee::PriNormal,
                        static_cast<Pci::ExtendedCapID>(capId),
                        downStreamPort));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                PrintUpStreamExtCapIdBlock,
                1,
                "Print the contents of an extended capability block on the "
                "upstream port")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    UINT32 capId;
    *pReturlwalue = JSVAL_NULL;
    const char usage[] = "Usage: PexDev.PrintUpStreamExtCapIdBlock(CapId)";

    JavaScriptPtr pJS;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &capId)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        RETURN_RC(pJsPexDev->GetPexDev()->PrintUpStreamExtCapIdBlock(
                        Tee::PriNormal,
                        static_cast<Pci::ExtendedCapID>(capId)));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                ResetErrors,
                1,
                "Reset errors on the specified downstream port "
                "and upstream port")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    const char usage[] = "Usage: PexDev.ResetErrors(DownPort)";
    JavaScriptPtr pJavaScript;
    UINT32 DownPort;
    *pReturlwalue = JSVAL_NULL;
    if ((NumArguments != 1)  ||
        (OK != pJavaScript->FromJsval(pArguments[0], &DownPort)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        RETURN_RC(pJsPexDev->GetPexDev()->ResetErrors(DownPort));
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamLinkSpeedCap,
                  1,
                  "get link speed capability of a downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamLinkSpeedCap(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        Pci::PcieLinkSpeed speedCap = pPexDev->GetDownStreamLinkSpeedCap(Index);
        if (pJS->ToJsval(speedCap, pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamSpeed,
                  1,
                  "Get the link speed of a downstream port")
{
    STEST_HEADER(1, 1, "Usage: PexDev.GetDownStreamSpeed(speed)\n");
    STEST_PRIVATE(JsPexDev, pJsPexDev, "PexDev");
    STEST_ARG(0, UINT32, port);

    const UINT32 speedVal = static_cast<UINT32>(pJsPexDev->GetDownStreamSpeed(port));
    if (pJavaScript->ToJsval(speedVal, pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(JsPexDev,
                SetDownStreamSpeed,
                2,
                "Set the target link speed of a downstream port")
{
    STEST_HEADER(2, 2, "Usage: PexDev.SetDownStreamSpeed(port, speed)\n");
    STEST_PRIVATE(JsPexDev, pJsPexDev, "PexDev");
    STEST_ARG(0, UINT32, port);
    STEST_ARG(1, UINT32, speed);

    RETURN_RC(pJsPexDev->SetDownStreamSpeed(port, static_cast<Pci::PcieLinkSpeed>(speed)));
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamLinkWidthCap,
                  1,
                  "get link width capability of a downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamLinkWidthCap(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;

    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        UINT32 widthCap = pPexDev->GetDownStreamLinkWidthCap(Index);
        if (pJS->ToJsval(widthCap, pReturlwalue) == OK)
        {
            return JS_TRUE;
        }
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetDownStreamAerLog,
                  1,
                  "Get the AER log from the downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetDownStreamAerLog(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJs;

    RC rc;
    if ((NumArguments != 1) ||
        (OK != (rc = pJs->FromJsval(pArguments[0], &Index))))
    {
        pJs->Throw(pContext, rc, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        vector<PexDevice::AERLogEntry> logData;
        C_CHECK_RC_THROW(pPexDev->GetDownStreamAerLog(Index, &logData),
                         "Unable to retrieve down stream error log");
        C_CHECK_RC_THROW(JsPexDev::AerLogToJs(pContext, pReturlwalue, logData),
                                              "Unable to colwert AER data to JsVal");
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  GetUpStreamAerLog,
                  0,
                  "Get the AER log from the downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.GetUpStreamAerLog()";
    JavaScriptPtr pJs;

    RC rc;
    if (NumArguments != 0)
    {
        pJs->Throw(pContext, rc, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        vector<PexDevice::AERLogEntry> logData;

        C_CHECK_RC_THROW(pPexDev->GetUpStreamAerLog(&logData),
                         "Unable to retrieve up stream error log");
        C_CHECK_RC_THROW(JsPexDev::AerLogToJs(pContext, pReturlwalue, logData),
                                              "Unable to colwert AER data to JsVal");
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  ClearDownStreamAerLog,
                  1,
                  "Clear the AER log from the downstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.ClearDownStreamAerLog(DownStreamIndex)";
    UINT32 Index;
    JavaScriptPtr pJs;

    RC rc;
    if ((NumArguments != 1) ||
        (OK != (rc = pJs->FromJsval(pArguments[0], &Index))))
    {
        pJs->Throw(pContext, rc, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        C_CHECK_RC_THROW(pPexDev->ClearDownStreamAerLog(Index),
                         "Unable to clear down stream error log");
        *pReturlwalue = JSVAL_NULL;
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(JsPexDev,
                  ClearUpStreamAerLog,
                  0,
                  "Clear the AER log from the upstream port")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexDev.ClearUpStreamAerLog()";
    JavaScriptPtr pJs;

    RC rc;
    if (NumArguments != 0)
    {
        pJs->Throw(pContext, rc, usage);
        return JS_FALSE;
    }

    JsPexDev *pJsPexDev;
    if ((pJsPexDev = JS_GET_PRIVATE(JsPexDev, pContext, pObject, "PexDev")) != 0)
    {
        PexDevice* pPexDev = pJsPexDev->GetPexDev();
        C_CHECK_RC_THROW(pPexDev->ClearUpStreamAerLog(),
                         "Unable to clear up stream error log");
        *pReturlwalue = JSVAL_NULL;
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
//              PEX constants
//-----------------------------------------------------------------------------
JS_CLASS(PexConst);
static SObject PexConst_Object
(
    "PexConst",
    PexConstClass,
    0,
    0,
    "PexConst JS Object"
);
PROP_CONST(PexConst, DUMP_ON_TEST, (1 << PexDevMgr::PEX_DUMP_ON_TEST));
PROP_CONST(PexConst, DUMP_ON_TEST_ERROR, (1 << PexDevMgr::PEX_DUMP_ON_TEST_ERROR));
PROP_CONST(PexConst, DUMP_ON_ERROR_CB, (1 << PexDevMgr::PEX_DUMP_ON_ERROR_CB));
PROP_CONST(PexConst, PRE_CHECK_CB_ENABLE, (1 << PexDevMgr::PEX_PRE_CHECK_CB_ENABLE));
PROP_CONST(PexConst, VERBOSE_ENABLE, (1 << PexDevMgr::PEX_VERBOSE_ENABLE));
PROP_CONST(PexConst, VERBOSE_COUNTS_ONLY, (1 << PexDevMgr::PEX_VERBOSE_COUNTS_ONLY));
PROP_CONST(PexConst, OFFICIAL_MODS_VERBOSITY, (1 << PexDevMgr::PEX_OFFICIAL_MODS_VERBOSITY));
PROP_CONST(PexConst, LOG_AER_DATA, (1 << PexDevMgr::PEX_LOG_AER_DATA));
PROP_CONST(PexConst, VERBOSE_JSON_ONLY, (1 << PexDevMgr::PEX_VERBOSE_JSON_ONLY));

PROP_CONST(PexConst, CORR_ID, PexErrorCounts::CORR_ID);
PROP_CONST(PexConst, NON_FATAL_ID, PexErrorCounts::NON_FATAL_ID);
PROP_CONST(PexConst, FATAL_ID, PexErrorCounts::FATAL_ID);
PROP_CONST(PexConst, UNSUP_REQ_ID, PexErrorCounts::UNSUP_REQ_ID);
PROP_CONST(PexConst, DETAILED_LINEERROR_ID, PexErrorCounts::DETAILED_LINEERROR_ID);
PROP_CONST(PexConst, DETAILED_CRC_ID, PexErrorCounts::DETAILED_CRC_ID);
PROP_CONST(PexConst, DETAILED_NAKS_R_ID, PexErrorCounts::DETAILED_NAKS_R_ID);
PROP_CONST(PexConst, DETAILED_NAKS_S_ID, PexErrorCounts::DETAILED_NAKS_S_ID);
PROP_CONST(PexConst, DETAILED_FAILEDL0SEXITS_ID, PexErrorCounts::DETAILED_FAILEDL0SEXITS_ID);
PROP_CONST(PexConst, REPLAY_ID, PexErrorCounts::REPLAY_ID);
PROP_CONST(PexConst, RECEIVER_ID, PexErrorCounts::RECEIVER_ID);
PROP_CONST(PexConst, LANE_ID, PexErrorCounts::LANE_ID);
PROP_CONST(PexConst, BAD_DLLP_ID, PexErrorCounts::BAD_DLLP_ID);
PROP_CONST(PexConst, REPLAY_ROLLOVER_ID, PexErrorCounts::REPLAY_ROLLOVER_ID);
PROP_CONST(PexConst, E_8B10B_ID, PexErrorCounts::E_8B10B_ID);
PROP_CONST(PexConst, SYNC_HEADER_ID, PexErrorCounts::SYNC_HEADER_ID);
PROP_CONST(PexConst, BAD_TLP_ID, PexErrorCounts::BAD_TLP_ID);
PROP_CONST(PexConst, NUM_ERR_COUNTERS, PexErrorCounts::NUM_ERR_COUNTERS);

PROP_CONST(PexConst, AER_LOG_IGNORE_NEW, PexDevice::AER_LOG_IGNORE_NEW);
PROP_CONST(PexConst, AER_LOG_DELETE_OLD, PexDevice::AER_LOG_DELETE_OLD);

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(PexConst,
                  ErrorIndexToString,
                  1,
                  "Get the string associated with an error index")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexConst.ErrorIndexToString(ErrIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (pJS->ToJsval(PexErrorCounts::GetString(Index), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(PexConst,
                  ErrorIndexIsInternalOnly,
                  1,
                  "Check if the error index is for internal use only")
{
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);
    const char usage[] = "Usage: PexConst.ErrorIndexIsInternalOnly(ErrIndex)";
    UINT32 Index;
    JavaScriptPtr pJS;
    if ((NumArguments != 1) ||
        (OK != pJS->FromJsval(pArguments[0], &Index)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if (pJS->ToJsval(PexErrorCounts::IsInternalOnly(Index), pReturlwalue) == OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

