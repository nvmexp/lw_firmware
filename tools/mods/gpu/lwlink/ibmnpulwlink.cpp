/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tee.h"
#include "core/include/platform.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "ibmnpulwlink.h"
#include "ctrl/ctrl2080.h"
#include <boost/range/algorithm/find_if.hpp>

namespace
{
    const UINT32 ILWALID_OFFSET = ~0U;
    const UINT32 ILWALID_BAR = ~0U;
    const UINT32 ILWALID_DOMAIN = ~0U;
    const UINT32 PHY_OFFSET     = 0;
    const UINT32 DLPL_OFFSET    = 0;
    const UINT32 NTL_OFFSET     = 0x10000;

    #define LW_NPU_LOW_POWER                      0xE0
    #define LW_NPU_LOW_POWER_LP_MODE_ENABLE       0:0
    #define LW_NPU_LOW_POWER_LP_MODE_ENABLE_FALSE 0x0
    #define LW_NPU_LOW_POWER_LP_MODE_ENABLE_TRUE  0x1
    #define LW_NPU_LOW_POWER_LP_ONLY_MODE         1:1
    #define LW_NPU_LOW_POWER_LP_ONLY_MODE_FALSE   0x0
    #define LW_NPU_LOW_POWER_LP_ONLY_MODE_TRUE    0x1

    UINT08 ReverseBitsInByte(UINT08 b)
    {
        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
        return b;
    }

    void ReverseBitsPerByte(UINT08 *pData, UINT08 byteCount)
    {
        for (;byteCount > 0; --byteCount, ++pData)
            *pData = ReverseBitsInByte(*pData);
    }

    const vector<UINT32> s_DefaultLineRateByVersion =
    {
        19200,
        25000,
        25000
    };
}

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
//! \brief Clear the error counts for the specified link id
//!
//! \param linkId  : Link id to clear counts on
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoClearHwErrorCounts(UINT32 linkId)
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_LWLINK_CLEAR_COUNTERS_PARAMS clearParams;
    clearParams.linkMask = (1 << GetLibLinkId(linkId));

    RC rc;
    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));
    clearParams.counterMask = GetErrorCounterControlMask(bPerLaneEnabled);

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(linkId),
                                               LibInterface::CONTROL_CLEAR_COUNTERS,
                                               &clearParams,
                                               sizeof(clearParams)));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize the device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC IbmNpuLwLink::DoDetectTopology()
{
    RC rc;
    CHECK_RC(InitializePostDiscovery());
    CHECK_RC(SavePerLaneCrcSetting());
    return rc;
}

//--------------------------------------------------------------------------
//! \brief Return whether the domain requires a VBIOS
//!
//! \param domain  : one of the known hw domains (DLPL/NTL/GLUE/PHY)
//!
//! \return true if the domain requires a byte swap, false otherwise
//!
bool IbmNpuLwLink::DomainRequiresByteSwap(RegHalDomain domain) const
{
    return (domain == RegHalDomain::DLPL);
}

//--------------------------------------------------------------------------
bool IbmNpuLwLink::DomainRequiresBitSwap(RegHalDomain domain) const
{
    return (domain == RegHalDomain::NTL);
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
{
    if (!IsLinkActive(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    Regs().Write32(linkId, MODS_PLWL_SL0_TXLANECRC_ENABLE, bEnable ? 1 : 0);

    return OK;
}

//--------------------------------------------------------------------------
//! \brief Return the bar number where the domain is located.
//!
//! \param domain  : one of the known hw domains (DL/PL/NTL/GLUE)
//!
//! \return bar number for the domain, ILWALID_BAR if unknown
//!
UINT32 IbmNpuLwLink::GetBarNumber(RegHalDomain domain) const
{
    switch (domain)
    {
        case RegHalDomain::DLPL:
        case RegHalDomain::NTL:
            return 0;
        case RegHalDomain::PHY:
            return 1;
        default:
            Printf(Tee::PriError,
                   "%s : Unknown domain %u\n",
                   __FUNCTION__,
                   static_cast<UINT32>(domain));
            break;
    }
    return ILWALID_BAR;
}

//------------------------------------------------------------------------------
//! \brief Get Copy Engine copy width through lwlink
//!
//! \param target : Copy engine target to get the width for
//!
//! \return Copy Engine copy width through lwlink
/* virtual */ LwLinkImpl::CeWidth IbmNpuLwLink::DoGetCeCopyWidth(CeTarget target) const
{
    //TODO
    return CEW_256_BYTE;
}

//-----------------------------------------------------------------------------
void IbmNpuLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    MASSERT(!"EOM not supported on NPU devices");
    MASSERT(pSettings);
    pSettings->numErrors = 0;
    pSettings->numBlocks = 0;
}

//--------------------------------------------------------------------------
//! \brief Return the number of bytes required for data access for a given
//!        domain & offset.
//!
//! \param domain  : one of the known hw domains (DLPL/NTL/GLUE)
//! \param offset  : byte offset from within the domain.
//!
//! \return number of bytes if successful, zero otherwise
//!
UINT32 IbmNpuLwLink::GetDataSize(RegHalDomain domain) const
{

    switch (domain)
    {
        case RegHalDomain::PHY:
            return sizeof(UINT16);
        case RegHalDomain::DLPL:
            return sizeof(UINT32);
        case RegHalDomain::NTL:
            return sizeof(UINT64);
        default:
            Printf(Tee::PriError,
                   "%s : Unknown domain %u\n",
                   __FUNCTION__,
                   static_cast<UINT32>(domain));
            break;
    }
    return 0;
}

//--------------------------------------------------------------------------
//! \brief Return the base offset of the domain.
//!
//! \param domain  : one of the known hw domains (DL/PL/NTL/GLUE)
//! \param linkId  : link id to get the domain base for
//!
//! \return base offset of the domain, ILWALID_OFFSET if unknown
//!
UINT32 IbmNpuLwLink::GetDomainBase(RegHalDomain domain, UINT32 linkId) const
{
    switch (domain)
    {
        case RegHalDomain::PHY:
            return PHY_OFFSET;
        case RegHalDomain::DLPL:
            return DLPL_OFFSET;
        case RegHalDomain::NTL:
            return NTL_OFFSET;
        default:
            Printf(Tee::PriError,
                   "%s : Unknown domain %u\n",
                   __FUNCTION__,
                   static_cast<UINT32>(domain));
            break;
    }
    return ILWALID_OFFSET;
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error counters for the specified link
//!
//! \param linkId       : Link ID to get errors on
//! \param pErrorCounts : Pointer to returned error counts
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    LW2080_CTRL_LWLINK_GET_COUNTERS_PARAMS countParams = { };
    countParams.linkMask |= 1ULL<<(GetLibLinkId(linkId));

    RC rc;
    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));
    countParams.counterMask = GetErrorCounterControlMask(bPerLaneEnabled);

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(linkId),
                                              LibInterface::CONTROL_GET_COUNTERS,
                                              &countParams,
                                              sizeof(countParams)));

    INT32 lwrCounterIdx = Utility::BitScanForward(countParams.counterMask, 0);
    while (lwrCounterIdx != -1)
    {
        LwLink::ErrorCounts::ErrId errId = ErrorCounterBitToId(1U << lwrCounterIdx);
        if (errId == LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS)
        {
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(pErrorCounts->SetCount(errId,
                                  static_cast<UINT32>(countParams.counters[GetLibLinkId(linkId)].value[lwrCounterIdx]),
                                  false));
        lwrCounterIdx = Utility::BitScanForward(countParams.counterMask,
                                                lwrCounterIdx + 1);
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error flags
//!
//! \param pErrorFlags : Pointer to returned error flags
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoGetErrorFlags(LwLink::ErrorFlags * pErrorFlags)
{
    MASSERT(pErrorFlags);
    pErrorFlags->linkErrorFlags.clear();
    pErrorFlags->linkGroupErrorFlags.clear();

    set<Device::LibDeviceHandle> processedLibHandles;

    RC rc;
    for (size_t ii = 0; ii < GetMaxLinks(); ii++)
    {
        Device::LibDeviceHandle lwrHandle = GetLibHandle(ii);
        if (processedLibHandles.count(lwrHandle))
            continue;

        LW2080_CTRL_LWLINK_GET_ERR_INFO_PARAMS flagParams = { };
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(ii),
                                                  LibInterface::CONTROL_GET_ERR_INFO,
                                                  &flagParams,
                                                  sizeof(flagParams)));
        processedLibHandles.insert(lwrHandle);

        for (INT32 lwrLink = Utility::BitScanForward(flagParams.linkMask, 0);
             lwrLink != -1;
             lwrLink = Utility::BitScanForward(flagParams.linkMask, lwrLink + 1))
        {
            set<string> newFlags;
            for (UINT32 errBitIdx = 0;
                 errBitIdx < LW2080_CTRL_LWLINK_TL_ERRLOG_IDX_MAX;
                 errBitIdx++)
            {
                if (flagParams.linkErrInfo[lwrLink].TLErrlog & (1 << errBitIdx))
                {
                    LwLinkErrorFlags::LwLinkFlagBit errFlagBit =
                        LwLinkErrorFlags::GetErrorFlagBit(errBitIdx);

                    if (errFlagBit == LwLinkErrorFlags::LWL_NUM_ERR_FLAGS)
                    {
                        return RC::SOFTWARE_ERROR;
                    }

                    string errFlagStr = LwLinkErrorFlags::GetFlagString(errFlagBit);
                    newFlags.insert(errFlagStr);
                }
            }
            if (flagParams.linkErrInfo[lwrLink].bExcessErrorDL)
            {
                LwLinkErrorFlags::LwLinkFlagBit flagBit = LwLinkErrorFlags::LWL_ERR_FLAG_EXCESS_DL;
                string flagStr = LwLinkErrorFlags::GetFlagString(flagBit);
                newFlags.insert(flagStr);
            }
            pErrorFlags->linkErrorFlags[lwrLink] = { newFlags.begin(), newFlags.end() };
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the fabric apertures for this device
//!
//! \param pFas : Pointer to returned fabric apertures
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoGetFabricApertures(vector<FabricAperture> *pFas)
{
    *pFas = m_FabricApertures;
    return (m_FabricApertures.empty()) ? RC::SOFTWARE_ERROR : OK;
}

//-----------------------------------------------------------------------------
/* virtual */ void IbmNpuLwLink::DoGetFlitInfo
(
    UINT32 transferWidth
   ,HwType hwType
   ,bool bSysmem
   ,FLOAT64 *pReadDataFlits
   ,FLOAT64 *pWriteDataFlits
   ,UINT32 *pTotalReadFlits
   ,UINT32 *pTotalWriteFlits
) const
{
    //TODO
    MASSERT(pReadDataFlits);
    MASSERT(pWriteDataFlits);
    MASSERT(pTotalReadFlits);
    MASSERT(pTotalWriteFlits);
    *pReadDataFlits = 0;
    *pWriteDataFlits = 0;
    *pTotalReadFlits = 0;
    *pTotalWriteFlits = 0;
}

//------------------------------------------------------------------------------
Device::LibDeviceHandle IbmNpuLwLink::GetLibHandle
(
    UINT32 linkId
) const
{
    MASSERT(linkId < m_LibLinkInfo.size());
    if (linkId >= m_LibLinkInfo.size())
        return Device::ILWALID_LIB_HANDLE;

    return m_LibLinkInfo.at(linkId).first;
}

//------------------------------------------------------------------------------
UINT32 IbmNpuLwLink::GetLibLinkId(UINT32 linkId) const
{
    MASSERT(linkId < m_LibLinkInfo.size());
    if (linkId >= m_LibLinkInfo.size())
        return ILWALID_LINK_ID;

    return m_LibLinkInfo.at(linkId).second;
}

//------------------------------------------------------------------------------
RC IbmNpuLwLink::GetModsLinkId(UINT32 libLinkId, UINT32 *pModsLinkId) const
{
    MASSERT(pModsLinkId);
    *pModsLinkId = ILWALID_LINK_ID;
    for (size_t ii = 0; ii < m_LibLinkInfo.size(); ++ii)
    {
        if (m_LibLinkInfo[ii].second == libLinkId)
        {
            *pModsLinkId = static_cast<UINT32>(ii);
            return OK;
        }
    }
    Printf(Tee::PriError, "%s : Unknown NPU library link ID %u\n", __FUNCTION__, libLinkId);
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
//! \brief Get Link ID associated with the specified endpoint
//!
//! \param endpointInfo : Endpoint info to get link ID for
//! \param pLinkId      : Pointer to returned Link ID
//!
//! \return OK if successful, not OK otherwise
RC IbmNpuLwLink::DoGetLinkId(const EndpointInfo& endpointInfo, UINT32* pLinkId) const
{
    if (GetDevice()->GetId() != endpointInfo.id)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Endpoint not present on %s\n", __FUNCTION__,
               GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    // RM doesnt return PCI domain/bus/dev/function for ebridge endpoints
    // and forced configurations.  On forced configurations, RM uses
    // linkNumber instead.
    if (TopologyManager::GetTopologyMatch() != LwLinkDevIf::TM_DEFAULT)
    {
        if (!IsLinkValid(endpointInfo.linkNumber))
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Not enough endpoints present on %s\n",
                   __FUNCTION__, GetDevice()->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
        *pLinkId = endpointInfo.linkNumber;
        return OK;
    }

    RC rc;
    UINT32 pciDomain, pciBus, pciDev, pciFunc;
    for (size_t ii = 0; ii < GetMaxLinks(); ii++)
    {
        CHECK_RC(GetDevice()->GetLibIf()->GetPciInfo(GetLibHandle(ii),
                                                     &pciDomain,
                                                     &pciBus,
                                                     &pciDev,
                                                     &pciFunc));

        if ((pciDomain == endpointInfo.pciDomain) &&
            (pciBus == endpointInfo.pciBus) &&
            (pciDev == endpointInfo.pciDevice) &&
            (pciFunc == endpointInfo.pciFunction) &&
            (GetLibLinkId(ii) == endpointInfo.linkNumber))
        {
            // IBM NPU/Ebridge devices can eventually have multiple link per PCI D:B:D.F
            *pLinkId = static_cast<UINT32>(ii);
            return OK;
        }
    }

    Printf(Tee::PriError, GetTeeModule()->GetCode(),
           "%s : Endpoint not present on %s\n", __FUNCTION__,
           GetDevice()->GetName().c_str());
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
//! \brief Get information about the LwLinks on the associated subdevice
//!
//! \param pLinkInfo  : Pointer to returned link info
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoGetLinkInfo(vector<LinkInfo> *pLinkInfo)
{
    if (!IsInitialized())
        return OK;

    RC rc;
    set<Device::LibDeviceHandle> uniqueHandles;
    transform(m_LibLinkInfo.begin(),
              m_LibLinkInfo.end(),
              inserter(uniqueHandles, uniqueHandles.begin()),
              [] (auto & l) { return l.first; });

    pLinkInfo->clear();
    pLinkInfo->resize(GetMaxLinks());

    for (auto const & libHandle : uniqueHandles)
    {
        // This initializes any fields in the link info structure that can be
        // initialized after device init but before link connection discovery
        // (no clock speeds or remote device info)
        //
        // Simulated devices are always last, if the very first link is simulated
        // then the rest are as well so skip the control call
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
        CHECK_RC(GetDevice()->GetLibIf()->Control(libHandle,
                                                  LibInterface::CONTROL_GET_LINK_STATUS,
                                                  &statusParams,
                                                  sizeof(statusParams)));

        for (INT32 lwrLibLink = Utility::BitScanForward(statusParams.enabledLinkMask, 0);
             lwrLibLink != -1;
             lwrLibLink = Utility::BitScanForward(statusParams.enabledLinkMask, lwrLibLink + 1))
        {
            UINT32 linkId;
            CHECK_RC(GetModsLinkId(lwrLibLink, &linkId));
            if (linkId >= GetMaxLinks())
            {
                Printf(Tee::PriError, "%s : Invalid link ID %u\n", __FUNCTION__, linkId);
                return RC::SOFTWARE_ERROR;
            }

            LinkInfo & lwrInfo = pLinkInfo->at(linkId);

            // There can be holes in the link mask from the library and the library
            // will not virtualize the linkIds (i.e. bit x always corresponds
            // to index x and always refers to physical lwlink x)
            LW2080_CTRL_LWLINK_LINK_STATUS_INFO *pLinkStat = &statusParams.linkInfo[linkId];

            lwrInfo.bValid =
                (LW2080_CTRL_LWLINK_GET_CAP(((LwU8*)(&pLinkStat->capsTbl)),
                                            LW2080_CTRL_LWLINK_CAPS_VALID) != 0);

            if (lwrInfo.bValid)
            {
                lwrInfo.version           = pLinkStat->lwlinkVersion;
                lwrInfo.sublinkWidth      = pLinkStat->subLinkWidth;
                lwrInfo.bLanesReversed    = (pLinkStat->bLaneReversal == LW_TRUE);

                if (pLinkStat->lwlinkVersion >= static_cast<LwU32>(s_DefaultLineRateByVersion.size())) //$
                {
                    Printf(Tee::PriError, "%s : Unknown link version %u\n",
                           __FUNCTION__, pLinkStat->lwlinkVersion);
                    return RC::UNSUPPORTED_SYSTEM_CONFIG;
                }
                
                lwrInfo.maxLinkDataRateKiBps =
                    CallwlateLinkBwKiBps(s_DefaultLineRateByVersion[pLinkStat->lwlinkVersion],
                                         lwrInfo.sublinkWidth);
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get link status for all links
//!
//! \param pLinkStatus  : Pointer to returned per-link status
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoGetLinkStatus(vector<LinkStatus> *pLinkStatus)
{
    MASSERT(pLinkStatus);
    RC rc;

    set<Device::LibDeviceHandle> uniqueHandles;
    transform(m_LibLinkInfo.begin(),
              m_LibLinkInfo.end(),
              inserter(uniqueHandles, uniqueHandles.begin()),
              [] (auto & l) { return l.first; });

    pLinkStatus->clear();
    pLinkStatus->resize(GetMaxLinks(),
                        { LS_ILWALID, SLS_ILWALID, SLS_ILWALID });

    for (auto const & libHandle : uniqueHandles)
    {
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = { };
        CHECK_RC(GetDevice()->GetLibIf()->Control(libHandle,
                                                  LibInterface::CONTROL_GET_LINK_STATUS,
                                                  &statusParams,
                                                  sizeof(statusParams)));

        for (INT32 lwrLibLink = Utility::BitScanForward(statusParams.enabledLinkMask, 0);
             lwrLibLink != -1;
             lwrLibLink = Utility::BitScanForward(statusParams.enabledLinkMask, lwrLibLink + 1))
        {
            UINT32 linkId;
            CHECK_RC(GetModsLinkId(lwrLibLink, &linkId));
            if (linkId >= GetMaxLinks())
            {
                Printf(Tee::PriError, "%s : Invalid link ID %u\n", __FUNCTION__, linkId);
                return RC::SOFTWARE_ERROR;
            }

            LinkStatus & lwrState = pLinkStatus->at(linkId);

            LW2080_CTRL_LWLINK_LINK_STATUS_INFO *pLinkStat = &statusParams.linkInfo[linkId];
            lwrState.linkState      = RmLinkStateToLinkState(pLinkStat->linkState);
            lwrState.rxSubLinkState =
                RmSubLinkStateToLinkState(pLinkStat->rxSublinkStatus);
            lwrState.txSubLinkState =
                RmSubLinkStateToLinkState(pLinkStat->txSublinkStatus);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get Link number used by the library for the link Id
//!
//! \param linkId   : Link ID to get the link number for
//! \param pLinkNum : Pointer to returned link number
//!
//! \return OK if successful, not OK otherwise
RC IbmNpuLwLink::GetLibLinkNumber(UINT32 linkId, UINT32 *pLinkNum) const
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }
    *pLinkNum = GetLibLinkId(linkId);
    return OK;
}

UINT32 IbmNpuLwLink::DoGetMaxLinks() const
{
    return static_cast<UINT32>(m_LibLinkInfo.size());
}

//-----------------------------------------------------------------------------
/* virtual */ FLOAT64 IbmNpuLwLink::DoGetMaxObservableBwPct(LwLink::TransferType tt)
{
    //TODO
    return 0.0;
}

//------------------------------------------------------------------------------
//! \brief Get maximum per-link per-direction bandwidthfor this device in KiBps
//!
//! \param lineRateMbps : Rate that the link is running at in Mbps
//!
//! \return Per link BW in KiBps
//!
/* virtual */ UINT32 IbmNpuLwLink::DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const
{
    if (lineRateMbps == UNKNOWN_RATE)
    {
        const UINT32 lwlinkVersion = GetLinkVersion(0);
        if (lwlinkVersion >= static_cast<UINT32>(s_DefaultLineRateByVersion.size()))
        {
            Printf(Tee::PriError, "%s : Unknown link version %u\n",
                   __FUNCTION__, lwlinkVersion);
            MASSERT(0);
            return 0;
        }
        lineRateMbps = s_DefaultLineRateByVersion[lwlinkVersion];
    }

    return LwLinkImpl::DoGetMaxPerLinkPerDirBwKiBps(lineRateMbps);
}

//-----------------------------------------------------------------------------
/* virtual */ FLOAT64 IbmNpuLwLink::DoGetMaxTransferEfficiency() const
{
    //TODO
    return 0.0;
}

//------------------------------------------------------------------------------
//! \brief Get the PCI info for the specified device
//!
//! \param linkId     : Link ID to get PCI info for
//! \param pDomain    : Pointer to returned PCI domain
//! \param pBus       : Pointer to returned PCI bus
//! \param pDev       : Pointer to returned PCI device
//! \param pFunc      : Pointer to returned PCI function
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC IbmNpuLwLink::DoGetPciInfo
(
    UINT32  linkId,
    UINT32 *pDomain,
    UINT32 *pBus,
    UINT32 *pDev,
    UINT32 *pFunc
)
{
    if ((GetLibHandle(linkId) == Device::ILWALID_LIB_HANDLE) ||
        (GetLibLinkId(linkId) == ILWALID_LINK_ID))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    return GetDevice()->GetLibIf()->GetPciInfo(GetLibHandle(linkId),
                                               pDomain,
                                               pBus,
                                               pDev,
                                               pFunc);
}

//------------------------------------------------------------------------------
//! \brief Get the CPU mapped BAR pointer for the specified linkId
//!
//! \param bar    : The BAR to get the CPU mapped pointer for
//! \param linkId : Link ID to get the CPU mapped BAR pointer for
//! \param pAddr  : Pointer to returned CPU mapped BAR pointer
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::GetVirtMappedAddress
(
    UINT32 barNum
   ,UINT32 linkId
   ,void **ppAddr
) const
{
    MASSERT(ppAddr);
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    RC rc = GetDevice()->GetLibIf()->GetBarInfo(GetLibHandle(linkId), barNum, nullptr, nullptr, ppAddr);

    // Bar 1 (containing the PL registers) is only present for each pair of links
    // with only one of each adjascent pair of links actually containing BAR 1
    if ((barNum == 1) && (ppAddr == nullptr))
    {
        UINT32 libLinkNum;
        rc.Clear();
        CHECK_RC(GetLibLinkNumber(linkId, &libLinkNum));

        UINT32 partnerNum = (libLinkNum & 1) ? libLinkNum-- : libLinkNum++;
        linkId = 0;
        do
        {
            CHECK_RC(GetLibLinkNumber(linkId, &libLinkNum));
        }
        while ((linkId < GetMaxLinks()) && (libLinkNum != partnerNum));

        if (linkId == GetMaxLinks())
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unable to find BAR %u on linkId %u on LwLink device %s\n",
                   __FUNCTION__, barNum, linkId, GetDevice()->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(GetDevice()->GetLibIf()->GetBarInfo(GetLibHandle(linkId), barNum, nullptr, nullptr, ppAddr));
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoRegRd(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 *pData)
{
    MASSERT(pData);
    RC rc;

    const UINT32 domainBase = GetDomainBase(domain, linkId);
    if (domainBase == ILWALID_OFFSET)
        return RC::BAD_PARAMETER;

    const UINT32 dataSize = GetDataSize(domain);
    if (dataSize == 0)
        return RC::BAD_PARAMETER;

    const UINT32 domainBar = GetBarNumber(domain);
    if (domainBar == ILWALID_BAR)
        return RC::BAD_PARAMETER;

    if ((domainBase + offset) % dataSize)
    {
        Printf(Tee::PriError, "offset:0x%x must be a multiple of %d\n", offset, dataSize);
        return RC::BAD_PARAMETER;
    }

    void *pvAddr;
    CHECK_RC(GetVirtMappedAddress(domainBar, linkId, &pvAddr));
    if (pvAddr == nullptr)
    {
        Printf(Tee::PriError, "%s : Registers are not mapped!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    char *pAddr = static_cast<char *>(pvAddr);
    const bool bByteSwap = DomainRequiresByteSwap(domain);
    const bool bBitSwap  = DomainRequiresBitSwap(domain);
    if ( dataSize == sizeof(UINT64))
    {
        UINT64 data64 = MEM_RD64(pAddr+domainBase+offset);
        if (bByteSwap)
            Utility::ByteSwap(reinterpret_cast<UINT08 *>(&data64), dataSize, dataSize);
        if (bBitSwap)
            ReverseBitsPerByte(reinterpret_cast<UINT08 *>(&data64), sizeof(UINT64));
        *pData = data64;
        return OK;
    }
    else if(dataSize == sizeof(UINT32))
    {
        UINT32 data32 = MEM_RD32(pAddr+domainBase+offset);
        if (bByteSwap)
            Utility::ByteSwap(reinterpret_cast<UINT08 *>(&data32), dataSize, dataSize);
        if (bBitSwap)
            ReverseBitsPerByte(reinterpret_cast<UINT08 *>(&data32), sizeof(UINT32));
        *pData = static_cast<UINT64>(data32);
        return OK;
    }
    else if(dataSize == sizeof(UINT16))
    {
        UINT16 data16 = MEM_RD16(pAddr+domainBase+offset);
        if (bByteSwap)
            Utility::ByteSwap(reinterpret_cast<UINT08 *>(&data16), dataSize, dataSize);
        if (bBitSwap)
            ReverseBitsPerByte(reinterpret_cast<UINT08 *>(&data16), sizeof(UINT16));
        *pData = static_cast<UINT64>(data16);
        return OK;
    }
    else
    {
        Printf(Tee::PriError, "dataSize of %d is not supported.\n", dataSize);
        return RC::BAD_PARAMETER; // bad domain or offset value.
    }
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoRegWr(UINT32 linkId, RegHalDomain domain, UINT32 offset, UINT64 data)
{
    RC rc;

    const UINT32 domainBase = GetDomainBase(domain, linkId);
    if (domainBase == ILWALID_OFFSET)
        return RC::BAD_PARAMETER;

    const UINT32 dataSize = GetDataSize(domain);
    if (dataSize == 0)
        return RC::BAD_PARAMETER;

    const UINT32 domainBar = GetBarNumber(domain);
    if (domainBar == ILWALID_BAR)
        return RC::BAD_PARAMETER;

    if ((domainBase + offset) % dataSize)
    {
        Printf(Tee::PriError, "offset:0x%x must be a multiple of %d\n", offset, dataSize);
        return RC::BAD_PARAMETER;
    }

    void *pvAddr;
    CHECK_RC(GetVirtMappedAddress(domainBar, linkId, &pvAddr));
    if (pvAddr == nullptr)
    {
        Printf(Tee::PriError, "%s : Registers are not mapped!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    char *pAddr = static_cast<char *>(pvAddr);
    const bool bByteSwap = DomainRequiresByteSwap(domain);
    const bool bBitSwap  = DomainRequiresBitSwap(domain);

    if (dataSize == sizeof(UINT64))
    {
        if (bByteSwap)
            Utility::ByteSwap(reinterpret_cast<UINT08 *>(&data), dataSize, dataSize);
        if (bBitSwap)
            ReverseBitsPerByte(reinterpret_cast<UINT08 *>(&data), sizeof(UINT64));
        MEM_WR64(pAddr+domainBase+offset, data);
        return OK;
    }
    else if(dataSize == sizeof(UINT32))
    {
        MASSERT(data <= _UINT32_MAX);
        UINT32 data32 = static_cast<UINT32>(data);
        if (bByteSwap)
            Utility::ByteSwap(reinterpret_cast<UINT08 *>(&data32), dataSize, dataSize);
        if (bBitSwap)
            ReverseBitsPerByte(reinterpret_cast<UINT08 *>(&data32), sizeof(UINT32));
        MEM_WR32(pAddr+domainBase+offset, data32);
        return OK;
    }
    else if(dataSize == sizeof(UINT16))
    {
        MASSERT(data <= _UINT16_MAX);
        UINT16 data16 = static_cast<UINT16>(data);
        if (bByteSwap)
            Utility::ByteSwap(reinterpret_cast<UINT08 *>(&data16), dataSize, dataSize);
        if (bBitSwap)
            ReverseBitsPerByte(reinterpret_cast<UINT08 *>(&data16), sizeof(UINT16));
        MEM_WR32(pAddr+domainBase+offset, data16);
        return OK;
    }
    else
    {
        Printf(Tee::PriError, "dataSize of %d is not supported.\n", dataSize);
        return RC::BAD_PARAMETER;
    }
}

//------------------------------------------------------------------------------
//! \brief Check if the specified link ID is active on this device
//!
//! \param linkId : Link ID on this device to query
//!
//! \return true if active, false otherwise
//!
/* virtual */ bool IbmNpuLwLink::DoIsLinkActive(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return false;
    }

    return LwLinkImpl::DoIsLinkActive(linkId);
}

//------------------------------------------------------------------------------
//! \brief Check if the specified link ID is valid on this device
//!
//! \param linkId : Link ID on this device to query
//!
//! \return true if valid, false otherwise
//!
/* virtual */ bool IbmNpuLwLink::DoIsLinkValid(UINT32 linkId) const
{
    MASSERT(IsInitialized());

    if ((GetLibHandle(linkId) == Device::ILWALID_LIB_HANDLE) ||
        (GetLibLinkId(linkId) == ILWALID_LINK_ID))
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return false;
    }

    return LwLinkImpl::DoIsLinkValid(linkId);
}

//------------------------------------------------------------------------------
//! \brief Get whether per lane counts are enabled
//!
//! \param linkId           : Link ID to get per-lane error count enable state
//! \param pbPerLaneEnabled : Return status of per lane enabled
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoPerLaneErrorCountsEnabled
(
    UINT32 linkId,
    bool *pbPerLaneEnabled
)
{
    RC rc;
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    *pbPerLaneEnabled = Regs().Test32(linkId, MODS_PLWL_SL0_TXLANECRC_ENABLE, 1);
    return rc;
}


//-----------------------------------------------------------------------------
/* virtual */ void IbmNpuLwLink::DoSetCeCopyWidth(CeTarget target, CeWidth width)
{
    //TODO
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoInitialize(LibDevHandles handles)
{
    RC rc;

    for (size_t ii = 0; ii < handles.size(); ii++)
        m_LibLinkInfo.emplace_back(handles[ii], ILWALID_LINK_ID);

    for (auto pLwrInfo = m_LibLinkInfo.begin(); pLwrInfo != m_LibLinkInfo.end();)
    {
        UINT64 linkMask = 0x0ULL;
        CHECK_RC(GetDevice()->GetLibIf()->GetLinkMask(pLwrInfo->first, &linkMask));
        for (INT32 ii = 1; ii < Utility::CountBits(linkMask); ii++)
        {
            pair<Device::LibDeviceHandle, UINT32> dupData = make_pair(pLwrInfo->first, 0);
            pLwrInfo = m_LibLinkInfo.insert(pLwrInfo, dupData);
        }

        INT32 lwrLinkNum = Utility::BitScanForward(linkMask, 0);
        while ((lwrLinkNum != -1) && (pLwrInfo != m_LibLinkInfo.end()))
        {
            pLwrInfo->second = lwrLinkNum;
            pLwrInfo++;
            lwrLinkNum = Utility::BitScanForward(linkMask, lwrLinkNum + 1);
        }
        if (lwrLinkNum != -1)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unable to assign all link numbers on LwLink device %s!\n",
                   __FUNCTION__, GetDevice()->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }
    }

    CHECK_RC(LwLinkImpl::DoInitialize(handles));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoClearLPCounts(UINT32 linkId)
{
    Regs().Write32(linkId, MODS_PLWL_STATS_CTRL_CLEAR_ALL_CLEAR);
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
) 
{
    MASSERT(nullptr != pCount);

    if (entry)
    {
        *pCount = Regs().Read32(linkId, MODS_PLWL_STATS_D_NUM_TX_LP_ENTER);
    }
    else
    {
        *pCount = Regs().Read32(linkId, MODS_PLWL_STATS_D_NUM_TX_LP_EXIT);
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoGetLinkPowerStateStatus
(
    UINT32 linkId,
    LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
) const
{
    MASSERT(nullptr != pLinkPowerStatus);

    RC rc;

    UINT32 val = 0;
    CHECK_RC(const_cast<IbmNpuLwLink*>(this)->RegRd(linkId, RegHalDomain::NTL, LW_NPU_LOW_POWER, &val));

    pLinkPowerStatus->rxHwControlledPowerState =
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _TRUE, val) &&
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _FALSE, val);
    pLinkPowerStatus->rxSubLinkLwrrentPowerState =
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _FALSE, val)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->rxSubLinkConfigPowerState =
        FLD_TEST_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _FALSE, val)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;

    pLinkPowerStatus->txHwControlledPowerState = pLinkPowerStatus->rxHwControlledPowerState;
    pLinkPowerStatus->txSubLinkLwrrentPowerState = pLinkPowerStatus->rxSubLinkLwrrentPowerState;
    pLinkPowerStatus->txSubLinkConfigPowerState = pLinkPowerStatus->rxSubLinkConfigPowerState;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoRequestPowerState
(
    UINT32 linkId
  , LwLinkPowerState::SubLinkPowerState state
  , bool bHwControlled
)
{
    RC rc;
    CHECK_RC(CheckPowerStateAvailable(state, bHwControlled));

    UINT32 val = 0;
    CHECK_RC(RegRd(linkId, RegHalDomain::NTL, LW_NPU_LOW_POWER, &val));

    if (bHwControlled)
    {
        val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _TRUE, val);
        val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _FALSE, val);
    }
    else
    {
        switch (state)
        {
        case LwLinkPowerState::SLS_PWRM_LP:
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _TRUE, val);
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _TRUE, val);
            break;
        case LwLinkPowerState::SLS_PWRM_FB:
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_MODE_ENABLE, _FALSE, val);
            val = FLD_SET_DRF(_NPU, _LOW_POWER, _LP_ONLY_MODE, _FALSE, val);
            break;
        default:
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Invalid SubLinkPowerState = %d\n",
                   __FUNCTION__, static_cast<UINT32>(state));
            return RC::BAD_PARAMETER;
        }
    }

    CHECK_RC(RegWr(linkId, RegHalDomain::NTL, LW_NPU_LOW_POWER, val));

    return OK;
}

//------------------------------------------------------------------------------
//! \brief Setup the fabric apertures for this device
//!
//! \param fas : Fabric apertures to set
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoSetFabricApertures(const vector<FabricAperture> &fas)
{
    m_FabricApertures = fas;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Setup topology related functionality on the device
//!
//! \param endpointInfo : Per link endpoint info for the device
//! \param topologyId   : Id in the topology file for the device
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC IbmNpuLwLink::DoSetupTopology
(
    const vector<EndpointInfo> &endpointInfo
   ,UINT32                      topologyId
)
{
    RC rc;
    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Setting up topology on %s\n",
           __FUNCTION__, GetDevice()->GetName().c_str());

    // TODO : Parse out endpoint info from topology file

    // TODO : Validate link active status matches the topology file

    m_TopologyId = topologyId;

    CHECK_RC(SavePerLaneCrcSetting());

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC IbmNpuLwLink::DoShutdown()
{
    StickyRC rc;

    if (m_SavedPerLaneEnable != ~0U)
        rc = EnablePerLaneErrorCounts(m_SavedPerLaneEnable ? true : false);

    rc = LwLinkImpl::DoShutdown();

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Save the current per-lane CRC setting
//!
//! \return OK if successful, not OK otherwise
RC IbmNpuLwLink::SavePerLaneCrcSetting()
{
    RC rc;
    // Need to read the actual state on an active link.
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (IsLinkActive(linkId))
        {
            m_SavedPerLaneEnable = Regs().Read32(0, MODS_PLWL_SL0_TXLANECRC_ENABLE);
            break;
        }
    }

    return rc;
}
