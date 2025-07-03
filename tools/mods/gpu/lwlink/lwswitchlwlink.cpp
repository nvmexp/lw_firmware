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

#include "lwswitchlwlink.h"

#include "core/include/platform.h"
#include "core/include/registry.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "ctrl_dev_lwswitch.h"
#include "device/interface/pcie.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_impl.h"
#include "lwswitch/svnp01/dev_nport_ip_addendum.h"
#include <boost/range/algorithm/find_if.hpp>

namespace
{
    const UINT32 ILWALID_DOMAIN = ~0U;

    // TODO: These should be defined in some external header file
    // LwSwitch uses the same PHY as GV100, so it uses the same values
    // See Bug 1808284
    // See Bug 1860968
    // Register addresses and field values take from comment at:
    // https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=1880995&cmtNo=13
    #define LW_EOM_CTRL1              0xAB
    #define LW_EOM_CTRL1_TYPE         1:0
    #define LW_EOM_CTRL1_TYPE_ODD     0x1
    #define LW_EOM_CTRL1_TYPE_EVEN    0x2
    #define LW_EOM_CTRL1_TYPE_ODDEVEN 0x3
    #define LW_EOM_CTRL2              0xAF
    #define LW_EOM_CTRL2_NERRS        3:0
    #define LW_EOM_CTRL2_NBLKS        7:4
    #define LW_EOM_CTRL2_MODE         11:8
    #define LW_EOM_CTRL2_MODE_X       0x5
    #define LW_EOM_CTRL2_MODE_XL      0x0
    #define LW_EOM_CTRL2_MODE_XH      0x1
    #define LW_EOM_CTRL2_MODE_Y       0xb
    #define LW_EOM_CTRL2_MODE_YL      0x6
    #define LW_EOM_CTRL2_MODE_YH      0x7
    #define LW_EOM_CTRL2_TYPE         17:16
    #define LW_EOM_CTRL2_TYPE_ODD     0x1
    #define LW_EOM_CTRL2_TYPE_EVEN    0x2
    #define LW_EOM_CTRL2_TYPE_ODDEVEN 0x3

    const UINT32 TL_COUNTER_MASK = LWSWITCH_LWLINK_COUNTER_TL_TX0 |
                                   LWSWITCH_LWLINK_COUNTER_TL_TX1 |
                                   LWSWITCH_LWLINK_COUNTER_TL_RX0 |
                                   LWSWITCH_LWLINK_COUNTER_TL_RX1;

    const UINT32 LATENCY_BIN_PPC_VC_MASK = (1 << LW_NPORT_VC_MAPPING_CREQ0) |
                                           (1 << LW_NPORT_VC_MAPPING_DNGRD) |
                                           (1 << LW_NPORT_VC_MAPPING_ATR)   |
                                           (1 << LW_NPORT_VC_MAPPING_ATSD)  |
                                           (1 << LW_NPORT_VC_MAPPING_PROBE) |
                                           (1 << LW_NPORT_VC_MAPPING_RSP0)  |
                                           (1 << LW_NPORT_VC_MAPPING_CREQ1) |
                                           (1 << LW_NPORT_VC_MAPPING_RSP1);
    const UINT32 LATENCY_BIN_VC_MASK = (1 << LW_NPORT_VC_MAPPING_CREQ0) |
                                       (1 << LW_NPORT_VC_MAPPING_RSP0);
    UINT32 GetLatencyBilwcMask()
    {
        return Platform::IsPPC() ? LATENCY_BIN_PPC_VC_MASK : LATENCY_BIN_VC_MASK;
    }

    // Taken from the switch driver and provided only when setting up virtual channels
    // that are not going to be read
    const LWSWITCH_LATENCY_BIN s_DefaultLatencyBins = { 120, 200, 1000 };
}

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
//! \brief Clear the error counts for the specified link id
//!
//! \param linkId  : Link id to clear counts on
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoClearErrorCounts(UINT32 linkId)
{
    MASSERT(m_CachedErrorCounts.size() > linkId);
    Tasker::MutexHolder lockCache(m_ErrorCounterMutex);
    m_CachedErrorCounts[linkId] = ErrorCounts();
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoClearHwErrorCounts(UINT32 linkId)
{
    RC rc;

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));

    LWSWITCH_LWLINK_CLEAR_COUNTERS_PARAMS clearParams = {};
    clearParams.linkMask = (1LLU << linkId);
    clearParams.counterMask = GetErrorCounterControlMask(bPerLaneEnabled);

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                               LibInterface::CONTROL_CLEAR_COUNTERS,
                                               &clearParams,
                                               sizeof(clearParams)));
    return rc;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoConfigurePorts(UINT32 topologyId)
{
    RC rc;

    m_TopologyId = topologyId;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Detect topology for the lwswitch device
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwSwitchLwLink::DoDetectTopology()
{
    // Nothing to do here but setup the link info since by default lwswitch
    // devices do not detect topology (detecting topology is GPU centric)
    return InitializePostDiscovery();
}

//------------------------------------------------------------------------------
//! \brief Get Copy Engine copy width through lwlink
//!
//! \param target : Copy engine target to get the width for
//!
//! \return Copy Engine copy width through lwlink
/* virtual */ LwLinkImpl::CeWidth LwSwitchLwLink::DoGetCeCopyWidth(CeTarget target) const
{
    //TODO
    return CEW_256_BYTE;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
{
    if (!IsLinkActive(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (bEnable)
        Regs().Write32(linkId, MODS_PLWL_SL0_TXLANECRC_ENABLE_EN);
    else
        Regs().Write32(linkId, MODS_PLWL_SL0_TXLANECRC_ENABLE_DIS);

    return OK;
}

//-----------------------------------------------------------------------------
UINT32 LwSwitchLwLink::GetDomainId(RegHalDomain domain)
{
    switch (domain)
    {
    case RegHalDomain::AFS:
        return REGISTER_RW_ENGINE_AFS;
    case RegHalDomain::AFS_PERFMON:
        return REGISTER_RW_ENGINE_AFS_PERFMON;
    case RegHalDomain::CLKS:
        return REGISTER_RW_ENGINE_CLKS;
    case RegHalDomain::DLPL:
        return REGISTER_RW_ENGINE_DLPL;
    case RegHalDomain::FUSE:
        return REGISTER_RW_ENGINE_FUSE;
    case RegHalDomain::JTAG:
        return REGISTER_RW_ENGINE_JTAG;
    case RegHalDomain::MINION:
        return REGISTER_RW_ENGINE_MINION;
    case RegHalDomain::NPG:
        return REGISTER_RW_ENGINE_NPG;
    case RegHalDomain::NPORT:
        return REGISTER_RW_ENGINE_NPORT;
    case RegHalDomain::NPORT_MULTI:
        return REGISTER_RW_ENGINE_NPORT_MULTICAST;
    case RegHalDomain::LWLDL:
        return REGISTER_RW_ENGINE_LWLDL;
    case RegHalDomain::LWLIPT:
        return REGISTER_RW_ENGINE_LWLIPT;
    case RegHalDomain::LWLIPT_LNK:
        return REGISTER_RW_ENGINE_LWLIPT_LNK;
    case RegHalDomain::LWLTLC:
        return REGISTER_RW_ENGINE_LWLTLC;
    case RegHalDomain::LWLTLC_MULTI:
        return REGISTER_RW_ENGINE_LWLTLC_MULTICAST;
    case RegHalDomain::NXBAR:
        return REGISTER_RW_ENGINE_NXBAR;
    case RegHalDomain::PMGR:
        return REGISTER_RW_ENGINE_PMGR;
    case RegHalDomain::RAW:
        return REGISTER_RW_ENGINE_RAW;
    case RegHalDomain::SAW:
        return REGISTER_RW_ENGINE_SAW;
    case RegHalDomain::SIOCTRL:
        return REGISTER_RW_ENGINE_SIOCTRL;
    case RegHalDomain::SWX:
        return REGISTER_RW_ENGINE_SWX;
    case RegHalDomain::SWX_PERFMON:
        return REGISTER_RW_ENGINE_SWX_PERFMON;
    case RegHalDomain::XP3G:
        return REGISTER_RW_ENGINE_XP3G;
    case RegHalDomain::XVE:
        return REGISTER_RW_ENGINE_XVE;
    case RegHalDomain::LWLPLL:
        return REGISTER_RW_ENGINE_PLL;
    case RegHalDomain::CLKS_P0:
        return REGISTER_RW_ENGINE_CLKS_P0;
    case RegHalDomain::XPL:
        return REGISTER_RW_ENGINE_XPL;
    case RegHalDomain::XTL:
        return REGISTER_RW_ENGINE_XTL;
    default:
        Printf(Tee::PriError,
               "%s : Unknown domain %u\n",
               __FUNCTION__,
               static_cast<UINT32>(domain));
        return ILWALID_DOMAIN;
    }
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error counters for the specified link
//!
//! \param linkId       : Link ID to get errors on
//! \param pErrorCounts : Pointer to returned error counts
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    MASSERT(pErrorCounts);
    RC rc;

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));

    LWSWITCH_LWLINK_GET_COUNTERS_PARAMS countParams = {};
    countParams.linkId = linkId;
    countParams.counterMask = GetErrorCounterControlMask(bPerLaneEnabled);

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_GET_COUNTERS,
                                              &countParams,
                                              sizeof(countParams)));

    for (INT32 lwrCounterIdx = Utility::BitScanForward(countParams.counterMask, 0);
         lwrCounterIdx != -1;
         lwrCounterIdx = Utility::BitScanForward(countParams.counterMask,
                                                 lwrCounterIdx + 1))
    {
        LwLink::ErrorCounts::ErrId errId = ErrorCounterBitToId(1U << lwrCounterIdx);
        if (errId == LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS)
        {
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(pErrorCounts->SetCount(errId,
                                  static_cast<UINT32>(countParams.lwlinkCounters[lwrCounterIdx]),
                                  false));
    }

    CHECK_RC(pErrorCounts->SetCount(LwLink::ErrorCounts::LWL_SW_RECOVERY_ID, 0, false));

    {
        Tasker::MutexHolder lockCache(m_ErrorCounterMutex);
        MASSERT(m_CachedErrorCounts.size() > linkId);
        m_CachedErrorCounts[linkId] += *pErrorCounts;
        *pErrorCounts = m_CachedErrorCounts[linkId];
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error flags
//!
//! \param pErrorFlags : Pointer to returned error flags
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoGetErrorFlags(LwLink::ErrorFlags * pErrorFlags)
{
    MASSERT(pErrorFlags);
    pErrorFlags->linkErrorFlags.clear();
    pErrorFlags->linkGroupErrorFlags.clear();
    // All of the error flags defined in the GPU are represented as full LwSwitch errors
    // and will be reported with that API.
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get the fabric aperture for this device
//!
//! \param pFas : Pointer to returned fabric apertures
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoGetFabricApertures(vector<FabricAperture> *pFas)
{
    // Return an empty set of fabric addresses.  This will allow switch only fabric
    // configurations
    pFas->clear();
    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ void LwSwitchLwLink::DoGetFlitInfo
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
//! \brief Get Link ID associated with the specified endpoint
//!
//! \param endpointInfo : Endpoint info to get link ID for
//! \param pLinkId      : Pointer to returned Link ID
//!
//! \return OK if successful, not OK otherwise
RC LwSwitchLwLink::DoGetLinkId(const EndpointInfo &endpointInfo, UINT32 *pLinkId) const
{
    if (GetDevice()->GetId() != endpointInfo.id)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Endpoint not present on %s\n", __FUNCTION__,
               GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }
    *pLinkId = endpointInfo.linkNumber;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get information about the LwLinks on the associated subdevice
//!
//! \param pLinkInfo  : Pointer to returned link info
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoGetLinkInfo(vector<LinkInfo> *pLinkInfo)
{
    RC rc;
    LWSWITCH_GET_LWLINK_STATUS_PARAMS statusParams = {};
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_GET_LINK_STATUS,
                                              &statusParams,
                                              sizeof(statusParams)));
    pLinkInfo->clear();

    const UINT64 linkMask = statusParams.enabledLinkMask;

    for (size_t lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        LinkInfo lwrInfo;

        if (!(linkMask & (1LLU << lwrLink)))
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        // There can be holes in the link mask from the library and the library
        // will not virtualize the linkIds (i.e. bit x always corresponds
        // to index x and always refers to physical lwlink x)
        LWSWITCH_LWLINK_LINK_STATUS_INFO *pLinkStat = &statusParams.linkInfo[lwrLink];

        lwrInfo.bValid =
            (LWSWITCH_LWLINK_GET_CAP(((LwU8*)(&pLinkStat->capsTbl)),
                                        LWSWITCH_LWLINK_CAPS_VALID) != 0);

        lwrInfo.version = pLinkStat->lwlinkVersion;
        if (!lwrInfo.bValid)
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        if (LwLinkDevIf::TopologyManager::GetTopologyMatch() != LwLinkDevIf::TM_DEFAULT)
        {
            lwrInfo.bActive = true;
        }
        else
        {
            LWSWITCH_LWLINK_DEVICE_INFO *pRemoteInfo = &pLinkStat->remoteDeviceInfo;
            lwrInfo.bActive = (pRemoteInfo->deviceType !=
                               LWSWITCH_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE);
        }

        lwrInfo.refClkSpeedMhz    = pLinkStat->lwlinkRefClkSpeedMhz;
        lwrInfo.linkClockMHz      = pLinkStat->lwlinkCommonClockSpeedKHz / 1000;
        lwrInfo.sublinkWidth      = pLinkStat->subLinkWidth;
        lwrInfo.bLanesReversed    = (pLinkStat->bLaneReversal == LW_TRUE);

        if (!lwrInfo.bActive)
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        lwrInfo.lineRateMbps      = pLinkStat->lwlinkLinkClockMhz;
        lwrInfo.maxLinkDataRateKiBps =
            static_cast<UINT32>(GetLinkDataRateEfficiency(lwrLink) *
                                CallwlateLinkBwKiBps(lwrInfo.lineRateMbps,
                                                     lwrInfo.sublinkWidth));

        pLinkInfo->push_back(lwrInfo);
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get link status for all links
//!
//! \param pLinkStatus  : Pointer to returned per-link status
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoGetLinkStatus(vector<LinkStatus> *pLinkStatus)
{
    MASSERT(pLinkStatus);
    RC rc;
    LWSWITCH_GET_LWLINK_STATUS_PARAMS statusParams = { };
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_GET_LINK_STATUS,
                                              &statusParams,
                                              sizeof(statusParams)));
    pLinkStatus->clear();

    const UINT64 linkMask = statusParams.enabledLinkMask;

    for (size_t lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        LinkStatus lwrState =
        {
            LS_ILWALID,
            SLS_ILWALID,
            SLS_ILWALID
        };

        if (!(linkMask & (1LLU << lwrLink)))
        {
            pLinkStatus->push_back(lwrState);
            continue;
        }

        else
        {
            LWSWITCH_LWLINK_LINK_STATUS_INFO *pLinkStat = &statusParams.linkInfo[lwrLink];
            lwrState.linkState      = RmLinkStateToLinkState(pLinkStat->linkState);
            lwrState.rxSubLinkState =
                RmSubLinkStateToLinkState(pLinkStat->rxSublinkStatus);
            lwrState.txSubLinkState =
                RmSubLinkStateToLinkState(pLinkStat->txSublinkStatus);
            pLinkStatus->push_back(lwrState);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
void LwSwitchLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    MASSERT(pSettings);
    pSettings->numErrors = 0x7;
    pSettings->numBlocks = 0xd;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoGetEomStatus
(
    FomMode mode
   ,UINT32 link
   ,UINT32 numErrors
   ,UINT32 numBlocks
   ,FLOAT64 timeoutMs
   ,vector<INT32>* status
)
{
    MASSERT(status);
    RC rc;

    const UINT32 numLanes = static_cast<UINT32>(status->size());
    MASSERT(numLanes > 0);

    RegHal& regs = Regs();

    UINT32 typeVal = 0x0;
    UINT32 modeVal = 0x0;
    switch (mode)
    {
        case EYEO_X:
            typeVal = LW_EOM_CTRL2_TYPE_ODDEVEN;
            modeVal = LW_EOM_CTRL2_MODE_X;
            break;
        case EYEO_XL_O:
            typeVal = LW_EOM_CTRL2_TYPE_ODD;
            modeVal = LW_EOM_CTRL2_MODE_XL;
            break;
        case EYEO_XL_E:
            typeVal = LW_EOM_CTRL2_TYPE_EVEN;
            modeVal = LW_EOM_CTRL2_MODE_XL;
            break;
        case EYEO_XH_O:
            typeVal = LW_EOM_CTRL2_TYPE_ODD;
            modeVal = LW_EOM_CTRL2_MODE_XH;
            break;
        case EYEO_XH_E:
            typeVal = LW_EOM_CTRL2_TYPE_EVEN;
            modeVal = LW_EOM_CTRL2_MODE_XH;
            break;
        case EYEO_Y:
            typeVal = LW_EOM_CTRL2_TYPE_ODDEVEN;
            modeVal = LW_EOM_CTRL2_MODE_Y;
            break;
        case EYEO_YL_O:
            typeVal = LW_EOM_CTRL2_TYPE_ODD;
            modeVal = LW_EOM_CTRL2_MODE_YL;
            break;
        case EYEO_YL_E:
            typeVal = LW_EOM_CTRL2_TYPE_EVEN;
            modeVal = LW_EOM_CTRL2_MODE_YL;
            break;
        case EYEO_YH_O:
            typeVal = LW_EOM_CTRL2_TYPE_ODD;
            modeVal = LW_EOM_CTRL2_MODE_YH;
            break;
        case EYEO_YH_E:
            typeVal = LW_EOM_CTRL2_TYPE_EVEN;
            modeVal = LW_EOM_CTRL2_MODE_YH;
            break;
        default:
            Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
            return RC::BAD_PARAMETER;
    }

    LWSWITCH_CTRL_CONFIG_EOM comfigEomParams = {};
    comfigEomParams.link = link;
    comfigEomParams.params = DRF_NUM(_EOM, _CTRL2, _TYPE, typeVal) |
                             DRF_NUM(_EOM, _CTRL2, _MODE, modeVal) |
                             DRF_NUM(_EOM, _CTRL2, _NBLKS, numBlocks) |
                             DRF_NUM(_EOM, _CTRL2, _NERRS, numErrors);
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_CONFIG_EOM,
                                              &comfigEomParams,
                                              sizeof(comfigEomParams)));

    // Set EOM disabled
    regs.Write32(link, MODS_PLWL_BR0_PAD_CTL_8_RX_EOM_EN_OFF, numLanes);

    UINT32 doneStatus = 0x0;
    auto pollEomDone = [&]()->bool
        {
            for (UINT32 lane = 0; lane < numLanes; lane++)
            {
                if (!regs.Test32(link, MODS_PLWL_BR0_PAD_CTL_4_RX_EOM_DONE, lane, doneStatus))
                    return false;
            }
            return true;
        };

    // Check that EOM_DONE is reset
    CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));

    // Set EOM programmable
    regs.Write32(link, MODS_PLWL_BR0_PAD_CTL_8_RX_EOM_OVRD_ON, numLanes);

    // Set EOM enabled
    regs.Write32(link, MODS_PLWL_BR0_PAD_CTL_8_RX_EOM_EN_ON, numLanes);

    // Check if EOM is done
    doneStatus = 0x1;
    CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));

    // Set EOM disabled
    regs.Write32(link, MODS_PLWL_BR0_PAD_CTL_8_RX_EOM_EN_OFF, numLanes);

    // Set EOM not programmable
    regs.Write32(link, MODS_PLWL_BR0_PAD_CTL_8_RX_EOM_OVRD_OFF, numLanes);

    // Get EOM status
    for (UINT32 i = 0; i < numLanes; i++)
    {
        status->at(i) =
            static_cast<INT16>(regs.Read32(link, MODS_PLWL_BR0_PAD_CTL_4_RX_EOM_STATUS, i));
    }

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ FLOAT64 LwSwitchLwLink::DoGetMaxObservableBwPct(LwLink::TransferType tt)
{
    //TODO
    return 0.0;
}

//-----------------------------------------------------------------------------
/* virtual */ FLOAT64 LwSwitchLwLink::DoGetMaxTransferEfficiency() const
{
    //TODO
    return 0.0;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoGetDetectedEndpointInfo(vector<EndpointInfo> *pEndpointInfo)
{
    RC rc;
    MASSERT(IsPostDiscovery());

    if (m_CachedDetectedEndpointInfo.size() > 0)
    {
        *pEndpointInfo = m_CachedDetectedEndpointInfo;
        return OK;
    }

    LWSWITCH_GET_LWLINK_STATUS_PARAMS statusParams = { };
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_GET_LINK_STATUS,
                                              &statusParams,
                                              sizeof(statusParams)));
    pEndpointInfo->clear();
    pEndpointInfo->resize(GetMaxLinks());

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        if (!(statusParams.enabledLinkMask & (1LLU << lwrLink)))
            continue;

        // There can be holes in the link mask from RM and RM will not
        // virtualize the linkIds (i.e. bit x always corresponds
        // to index x and always refers to physical lwlink x)
        LWSWITCH_LWLINK_LINK_STATUS_INFO *pLinkStat = &statusParams.linkInfo[lwrLink];

        if (LWSWITCH_LWLINK_GET_CAP(((LwU8*)(&pLinkStat->capsTbl)),
                                        LWSWITCH_LWLINK_CAPS_VALID) == 0)
        {
            continue;
        }

        if (!pLinkStat->connected)
            continue;

        LWSWITCH_LWLINK_DEVICE_INFO *pRemoteInfo = &pLinkStat->remoteDeviceInfo;

        if (pRemoteInfo->deviceType == LWSWITCH_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE)
        {
            Printf(Tee::PriError,
                   "%s : Switch driver reported connected link with no remote device type "
                   "at link %u!\n",
                   __FUNCTION__, lwrLink);
            return RC::SOFTWARE_ERROR;
        }

        if (!DRF_VAL(SWITCH_LWLINK,
                     _DEVICE_INFO_DEVICE_ID,
                     _FLAGS,
                     pRemoteInfo->deviceIdFlags) &
                LWSWITCH_LWLINK_DEVICE_INFO_DEVICE_ID_FLAGS_PCI)
        {
            Printf(Tee::PriError, "%s : PCI info missing endpoint at link %u!\n",
                   __FUNCTION__, lwrLink);
            return RC::SOFTWARE_ERROR;
        }

        EndpointInfo lwrInfo;
        switch (pRemoteInfo->deviceType)
        {
            case LWSWITCH_LWLINK_DEVICE_INFO_DEVICE_TYPE_GPU:
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                      pRemoteInfo->bus,
                                      pRemoteInfo->device,
                                      pRemoteInfo->function);
                lwrInfo.devType = TestDevice::TYPE_LWIDIA_GPU;
                break;
            case LWSWITCH_LWLINK_DEVICE_INFO_DEVICE_TYPE_EBRIDGE:
                // Ebridge devices are only differentiated by their Domain/Bus/Device
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                      pRemoteInfo->bus,
                                      pRemoteInfo->device);
                lwrInfo.devType = TestDevice::TYPE_EBRIDGE;
                break;
            case LWSWITCH_LWLINK_DEVICE_INFO_DEVICE_TYPE_NPU:
                // IBMNPU devices are only differentiated by their Domain
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain);
                lwrInfo.devType = TestDevice::TYPE_IBM_NPU;
                break;
            case LWSWITCH_LWLINK_DEVICE_INFO_DEVICE_TYPE_SWITCH:
                // Switch devices are not differentiated by function
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                      pRemoteInfo->bus,
                                      pRemoteInfo->device);
                lwrInfo.devType = TestDevice::TYPE_LWIDIA_LWSWITCH;
                break;
            default:
                Printf(Tee::PriError, "%s : Unknown remote device type %u\n",
                       __FUNCTION__, static_cast<UINT32>(pRemoteInfo->deviceType));
                return RC::SOFTWARE_ERROR;
        }

        lwrInfo.linkNumber = pLinkStat->remoteDeviceLinkNumber;
        lwrInfo.pciDomain   = pRemoteInfo->domain;
        lwrInfo.pciBus      = pRemoteInfo->bus;
        lwrInfo.pciDevice   = pRemoteInfo->device;
        lwrInfo.pciFunction = pRemoteInfo->function;

        pEndpointInfo->at(lwrLink) = lwrInfo;
    }

    m_CachedDetectedEndpointInfo = *pEndpointInfo;

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoGetOutputLinks
(
    UINT32 inputLink
   ,UINT64 fabricBase
   ,DataType dt
   ,set<UINT32>* pPorts
) const
{
    MASSERT((dt == DT_REQUEST) || (dt == DT_RESPONSE));
    RC rc;

    FMLibInterface::PortInfo portInfo = {};
    portInfo.type = GetDevice()->GetType();
    portInfo.nodeId = 0;
    portInfo.physicalId = GetTopologyId();
    portInfo.portNum = inputLink;

    if (dt == DT_REQUEST)
    {
        UINT32 index = static_cast<UINT32>(fabricBase >> GetFabricRegionBits());
        CHECK_RC(TopologyManager::GetFMLibIf()->GetIngressRequestPorts(portInfo, index, pPorts));
    }
    else if (dt == DT_RESPONSE)
    {
        CHECK_RC(TopologyManager::GetFMLibIf()->GetIngressResponsePorts(portInfo, fabricBase, pPorts));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchLwLink::DoGetPciInfo
(
    UINT32  linkId,
    UINT32 *pDomain,
    UINT32 *pBus,
    UINT32 *pDev,
    UINT32 *pFunc
)
{
    return GetDevice()->GetLibIf()->GetPciInfo(GetLibHandle(),
                                               pDomain,
                                               pBus,
                                               pDev,
                                               pFunc);
}

/* virtual */ RC LwSwitchLwLink::DoInitialize(LibDevHandles handles)
{
    MASSERT(handles.size() == 1);
    RC rc;

    m_LibHandle = handles[0];

    CHECK_RC(LwLinkImpl::DoInitialize(handles));

    m_ErrorCounterMutex = Tasker::CreateMutex("LwLink Error Cache Mutex",
                                             (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM) ?
                                                 Tasker::mtxUnchecked : Tasker::mtxLast);
    m_CachedErrorCounts.resize(GetMaxLinks());

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoInitializePostDiscovery()
{
    RC rc;
    CHECK_RC(LwLinkImpl::DoInitializePostDiscovery());
    SetRegLogLinkMask(GetMaxLinkMask());
    return OK;
}

//------------------------------------------------------------------------------
bool LwSwitchLwLink::DoIsLinkAcCoupled(UINT32 linkId) const
{
    if (!IsLinkActive(linkId))
        return false;
    return Regs().Test32(linkId, MODS_PLWL_LINK_CONFIG_AC_SAFE_EN_ON);
}

//------------------------------------------------------------------------------
//! \brief Get whether per lane counts are enabled
//!
//! \param linkId           : Link ID to get per-lane error count enable state
//! \param pbPerLaneEnabled : Return status of per lane enabled
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoPerLaneErrorCountsEnabled
(
    UINT32 linkId,
    bool *pbPerLaneEnabled
)
{
    MASSERT(pbPerLaneEnabled);
    *pbPerLaneEnabled = false;

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    *pbPerLaneEnabled = Regs().Test32(linkId, MODS_PLWL_SL0_TXLANECRC_ENABLE_EN);

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoRegRd
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64*      pData
)
{
    RC rc;

    if (domain == RegHalDomain::PCIE)
    {
        MASSERT(linkId == 0);
        auto pPcie = GetDevice()->GetInterface<Pcie>();
        UINT32 data = 0;
        CHECK_RC(Platform::PciRead32(pPcie->DomainNumber(),
                                     pPcie->BusNumber(),
                                     pPcie->DeviceNumber(),
                                     pPcie->FunctionNumber(),
                                     offset,
                                     &data));
        *pData = static_cast<UINT64>(data);
        return RC::OK;
    }

    UINT32 domainId = GetDomainId(domain);
    if (domainId == ILWALID_DOMAIN)
        return RC::BAD_PARAMETER;

    LWSWITCH_REGISTER_READ readParams = {};
    readParams.engine   = domainId;
    readParams.instance = linkId;
    readParams.offset   = offset;
    readParams.val      = 0x0;

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
        LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_REGISTER_READ,
        &readParams,
        sizeof(readParams)));

    *pData = static_cast<UINT64>(readParams.val);

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoRegWr
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64       data
)
{
    RC rc;

    MASSERT(data <= _UINT32_MAX);

    if (domain == RegHalDomain::PCIE)
    {
        MASSERT(linkId == 0);
        auto pPcie = GetDevice()->GetInterface<Pcie>();
        CHECK_RC(Platform::PciWrite32(pPcie->DomainNumber(),
                                      pPcie->BusNumber(),
                                      pPcie->DeviceNumber(),
                                      pPcie->FunctionNumber(),
                                      offset,
                                      static_cast<UINT32>(data)));
        return RC::OK;
    }

    UINT32 domainId = GetDomainId(domain);
    if (domainId == ILWALID_DOMAIN)
        return RC::BAD_PARAMETER;

    LWSWITCH_REGISTER_WRITE writeParams = {};
    writeParams.engine   = domainId;
    writeParams.instance = linkId;
    writeParams.bcast    = false;
    writeParams.offset   = offset;
    writeParams.val      = static_cast<UINT32>(data);

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
        LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_REGISTER_WRITE,
        &writeParams,
        sizeof(writeParams)));

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoRegWrBroadcast
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64       data
)
{
    RC rc;

    MASSERT(data <= _UINT32_MAX);

    UINT32 domainId = GetDomainId(domain);
    if (domainId == ILWALID_DOMAIN)
        return RC::BAD_PARAMETER;

    LWSWITCH_REGISTER_WRITE writeParams = {};
    writeParams.engine   = domainId;
    writeParams.instance = linkId;
    writeParams.bcast    = true;
    writeParams.offset   = offset;
    writeParams.val      = static_cast<UINT32>(data);

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
        LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_REGISTER_WRITE,
        &writeParams,
        sizeof(writeParams)));

    return OK;
}

//-----------------------------------------------------------------------------
RC LwSwitchLwLink::DoResetTopology()
{
    StickyRC rc;

    m_TopologyId = ~0U;

    rc = LwLinkImpl::DoResetTopology();

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ void LwSwitchLwLink::DoSetCeCopyWidth(CeTarget target, CeWidth width)
{
    //TODO
}

//-----------------------------------------------------------------------------
RC LwSwitchLwLink::DoSetNearEndLoopbackMode(UINT64 linkMask, NearEndLoopMode loopMode)
{
    // Default to NEA for TREX unless NED was specifically requested instead.
    LWSWITCH_SET_PORT_TEST_MODE portTestMode = { };
    portTestMode.nea = (loopMode == NearEndLoopMode::NEA);
    portTestMode.ned = (loopMode == NearEndLoopMode::NED);

    UINT64 lwrNeaMask = GetNeaLoopbackLinkMask();
    UINT64 lwrNedMask = GetNedLoopbackLinkMask();
    RC rc;
    for (INT32 lwrLinkId = Utility::BitScanForward(linkMask, 0);
         lwrLinkId != -1;
         lwrLinkId = Utility::BitScanForward(linkMask, lwrLinkId + 1))
    {
        portTestMode.portNum = lwrLinkId;
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                    LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_SET_PORT_TEST_MODE,
                                    &portTestMode,
                                    sizeof(portTestMode)));
        switch (loopMode)
        {
            case NearEndLoopMode::NEA:
                lwrNeaMask |= (1ULL << lwrLinkId);
                lwrNedMask &= ~(1ULL << lwrLinkId);
                break;
            case NearEndLoopMode::NED:
                lwrNeaMask &= ~(1ULL << lwrLinkId);
                lwrNedMask |= (1ULL << lwrLinkId);
                break;
            case NearEndLoopMode::None:
            default:
                lwrNeaMask &= ~(1ULL << lwrLinkId);
                lwrNedMask &= ~(1ULL << lwrLinkId);
                break;
        }
        SetNeaLoopbackLinkMask(lwrNeaMask);
        SetNedLoopbackLinkMask(lwrNedMask);
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Setup topology related functionality on the device
//!
//! \param endpointInfo : Per link endpoint info for the device
//! \param topologyId   : Id in the topology file for the device
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwSwitchLwLink::DoSetupTopology
(
    const vector<EndpointInfo> &endpointInfo
   ,UINT32                      topologyId
)
{
    RC rc;
    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Setting up topology on %s\n",
           __FUNCTION__, GetDevice()->GetName().c_str());

    LwLink::SetEndpointInfo(endpointInfo);

    CHECK_RC(SavePerLaneCrcSetting());

    if ((m_TopologyId != ~0U) && (m_TopologyId != topologyId))
    {
        Printf(Tee::PriError,
               "%s already configured with different toplogy ID, old ID %u, new ID %u\n",
               GetDevice()->GetName().c_str(), m_TopologyId, topologyId);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(ConfigurePorts(topologyId));
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoShutdown()
{
    StickyRC rc;

    if (m_SavedPerLaneEnable != ~0U)
        rc = EnablePerLaneErrorCounts(m_SavedPerLaneEnable ? true : false);

    rc = LwLinkImpl::DoShutdown();

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ bool LwSwitchLwLink::DoSupportsFomMode(FomMode mode) const
{
    switch (mode)
    {
        case EYEO_X:
        case EYEO_XL_O:
        case EYEO_XL_E:
        case EYEO_XH_O:
        case EYEO_XH_E:
        case EYEO_Y:
        case EYEO_YL_O:
        case EYEO_YL_E:
        case EYEO_YH_O:
        case EYEO_YH_E:
            return true;
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
//! \brief Save the current per-lane CRC setting
//!
//! \return OK if successful, not OK otherwise
RC LwSwitchLwLink::SavePerLaneCrcSetting()
{
    RC rc;
    // Need to read the actual state on an active link.
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (IsLinkValid(linkId))
        {
            bool enabled = false;
            CHECK_RC(PerLaneErrorCountsEnabled(linkId, &enabled));
            m_SavedPerLaneEnable = (enabled) ? 1 : 0;
            break;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
static map<UINT32, LwLink::ErrorCounts::ErrId> s_CounterBitToID =
{
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT,    LwLink::ErrorCounts::LWL_RX_CRC_FLIT_ID        },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0, LwLink::ErrorCounts::LWL_RX_CRC_LANE0_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1, LwLink::ErrorCounts::LWL_RX_CRC_LANE1_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L2, LwLink::ErrorCounts::LWL_RX_CRC_LANE2_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L3, LwLink::ErrorCounts::LWL_RX_CRC_LANE3_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L4, LwLink::ErrorCounts::LWL_RX_CRC_LANE4_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L5, LwLink::ErrorCounts::LWL_RX_CRC_LANE5_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L6, LwLink::ErrorCounts::LWL_RX_CRC_LANE6_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L7, LwLink::ErrorCounts::LWL_RX_CRC_LANE7_ID       },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_MASKED,  LwLink::ErrorCounts::LWL_RX_CRC_MASKED_ID      },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_REPLAY,      LwLink::ErrorCounts::LWL_TX_REPLAY_ID          },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_REPLAY,      LwLink::ErrorCounts::LWL_RX_REPLAY_ID          },      //$
    { LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_RECOVERY,    LwLink::ErrorCounts::LWL_TX_RECOVERY_ID        },      //$
    { LWSWITCH_LWLINK_COUNTER_PHY_REFRESH_PASS,      LwLink::ErrorCounts::LWL_UPHY_REFRESH_PASS_ID  },      //$
    { LWSWITCH_LWLINK_COUNTER_PHY_REFRESH_FAIL,      LwLink::ErrorCounts::LWL_UPHY_REFRESH_FAIL_ID  }       //$
};

//-----------------------------------------------------------------------------
LwLink::ErrorCounts::ErrId LwSwitchLwLink::ErrorCounterBitToId(UINT32 counterBit)
{
    if (s_CounterBitToID.find(counterBit) == s_CounterBitToID.end())
    {
        Printf(Tee::PriError,
               "%s : Unknown counter bit 0x%08x\n", __FUNCTION__, counterBit);
        return LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS;
    }
    return s_CounterBitToID[counterBit];
}

//-----------------------------------------------------------------------------
UINT32 LwSwitchLwLink::GetErrorCounterControlMask(bool bIncludePerLane)
{
    UINT32 counterMask = LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT    |
                         LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_REPLAY      |
                         LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_RECOVERY;
    if (bIncludePerLane)
    {
        counterMask |= LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L2 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L3 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L4 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L5 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L6 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L7;
    }
    return counterMask;
}

//-----------------------------------------------------------------------------
// Interface Implementations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoGetLinkPowerStateStatus
(
    UINT32 linkId,
    LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
) const
{
    MASSERT(nullptr != pLinkPowerStatus);

    RC rc;

    const RegHal& regs = Regs();

    pLinkPowerStatus->rxHwControlledPowerState =
        regs.Test32(linkId, MODS_LWLTLC_RX_PWRM_IC_SW_CTRL_HARDWAREDISABLE, 0);
    pLinkPowerStatus->rxSubLinkLwrrentPowerState =
        regs.Test32(linkId, MODS_LWLTLC_RX_PWRM_IC_SW_CTRL_LWRRENTSTATE_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->rxSubLinkConfigPowerState =
        regs.Test32(linkId, MODS_LWLTLC_RX_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txHwControlledPowerState =
        regs.Test32(linkId, MODS_LWLTLC_TX_PWRM_IC_SW_CTRL_HARDWAREDISABLE, 0);
    pLinkPowerStatus->txSubLinkLwrrentPowerState =
        regs.Test32(linkId, MODS_LWLTLC_TX_PWRM_IC_SW_CTRL_LWRRENTSTATE_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txSubLinkConfigPowerState =
        regs.Test32(linkId, MODS_LWLTLC_TX_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoRequestPowerState
(
    UINT32 linkId
  , LwLinkPowerState::SubLinkPowerState state
  , bool bHwControlled
)
{
    RC rc;
    CHECK_RC(CheckPowerStateAvailable(state, bHwControlled));

    RegHal& regs = Regs();

    UINT32 rxSwCtrl = 0;
    UINT32 txSwCtrl = 0;
    switch (state)
    {
    case LwLinkPowerState::SLS_PWRM_LP:
        regs.SetField(&rxSwCtrl, MODS_LWLTLC_RX_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP);
        regs.SetField(&txSwCtrl, MODS_LWLTLC_TX_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP);
        if (bHwControlled)
        {
            // set the bit allowing idle counting if the HW is allowed to change the power
            // state, i.e. when true == (SOFTWAREDESIRED == 1 (LP) && HARDWAREDISABLE == 0)
            regs.SetField(&rxSwCtrl, MODS_LWLTLC_RX_PWRM_IC_SW_CTRL_COUNTSTART, 1);
            regs.SetField(&txSwCtrl, MODS_LWLTLC_TX_PWRM_IC_SW_CTRL_COUNTSTART, 1);
        }
        break;
    case LwLinkPowerState::SLS_PWRM_FB:
        regs.SetField(&rxSwCtrl, MODS_LWLTLC_RX_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB);
        regs.SetField(&txSwCtrl, MODS_LWLTLC_TX_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB);
        break;
    default:
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid SubLinkPowerState = %d\n",
               __FUNCTION__, static_cast<UINT32>(state));
        return RC::BAD_PARAMETER;
    }
    regs.SetField(&rxSwCtrl,
                  MODS_LWLTLC_RX_PWRM_IC_SW_CTRL_HARDWAREDISABLE,
                  bHwControlled ? 0 : 1);
    regs.SetField(&txSwCtrl,
                  MODS_LWLTLC_TX_PWRM_IC_SW_CTRL_HARDWAREDISABLE,
                  bHwControlled ? 0 : 1);
    regs.Write32(linkId, MODS_LWLTLC_RX_PWRM_IC_SW_CTRL, rxSwCtrl);
    regs.Write32(linkId, MODS_LWLTLC_TX_PWRM_IC_SW_CTRL, txSwCtrl);

    return OK;
}

//------------------------------------------------------------------------------
UINT32 LwSwitchLwLink::DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
{
    if (!IsLinkActive(linkId))
        return 0;

    const auto pwrmIcLpEnterThresholdReg =
        RX == dir ? MODS_LWLTLC_RX_PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD :
                    MODS_LWLTLC_TX_PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD;
    const auto pwrmIcIncLpIncReg = RX == dir ? MODS_LWLTLC_RX_PWRM_IC_INC_LPINC :
                                               MODS_LWLTLC_TX_PWRM_IC_INC_LPINC;
    const RegHal& regs = Regs();

    const FLOAT64 linkClock = GetLinkClockRateMhz(linkId);
    const FLOAT64 threshold = regs.Read32(linkId, pwrmIcLpEnterThresholdReg);
    const FLOAT64 lpInc = regs.Read32(linkId, pwrmIcIncLpIncReg);

    return static_cast<UINT32>(threshold / linkClock / 1000.0 / lpInc);
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoClearLPCounts(UINT32 linkId)
{
    Regs().Write32(linkId, MODS_PLWL_STATS_CTRL_CLEAR_ALL_CLEAR);
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchLwLink::DoGetLPEntryOrExitCount
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
        *pbOverflow = *pCount == Regs().LookupMask(MODS_PLWL_STATS_D_NUM_TX_LP_ENTER);
    }
    else
    {
        *pCount = Regs().Read32(linkId, MODS_PLWL_STATS_D_NUM_TX_LP_EXIT);
        *pbOverflow = *pCount == Regs().LookupMask(MODS_PLWL_STATS_D_NUM_TX_LP_EXIT);
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
// LwLinkUphy
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
UINT32 LwSwitchLwLink::DoGetActiveLaneMask(RegBlock regBlock) const
{
    if (regBlock == RegBlock::CLN)
        return 1;

    return (1 << DoGetMaxLanes(regBlock)) - 1;
}

//------------------------------------------------------------------------------
UINT32 LwSwitchLwLink::DoGetMaxLanes(RegBlock regBlock) const
{
    if (regBlock == RegBlock::CLN)
        return 1;

    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        const UINT32 sublinkWidth = GetSublinkWidth(linkId);
        if (sublinkWidth)
            return sublinkWidth;
    }
    return 0;
}

//------------------------------------------------------------------------------
UINT32 LwSwitchLwLink::DoGetMaxRegBlocks(RegBlock regBlock) const
{
    if (regBlock == RegBlock::CLN)
        return GetMaxLinkGroups();

    return GetMaxLinks();
}

//------------------------------------------------------------------------------
UINT64 LwSwitchLwLink::DoGetRegLogRegBlockMask(RegBlock regBlock)
{
    if (regBlock == RegBlock::CLN)
        return m_UphyRegsIoctlMask;
    return m_UphyRegsLinkMask;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive)
{
    MASSERT(pbActive);

    if (blockIdx >= GetMaxRegBlocks(regBlock))
    {
        MASSERT(!"Invalid register block index");
        return false;
    }

    RC rc;
    vector<LinkStatus> linkStatus;
    CHECK_RC(GetLinkStatus(&linkStatus));

    if (regBlock == RegBlock::CLN)
    {
        const UINT32 linksPerGroup = GetMaxLinks() / GetMaxLinkGroups();
        const UINT32 firstLink = blockIdx * linksPerGroup;

        *pbActive = false;
        for (UINT32 lwrLink = firstLink; lwrLink < (firstLink + linksPerGroup); lwrLink++)
        {
            if ((linkStatus[lwrLink].rxSubLinkState != SLS_OFF) &&
                (linkStatus[lwrLink].rxSubLinkState != SLS_ILWALID) &&
                (linkStatus[lwrLink].txSubLinkState != SLS_OFF) &&
                (linkStatus[lwrLink].txSubLinkState != SLS_ILWALID))
            {
                *pbActive = true;
                return RC::OK;
            }
        }
        return RC::OK;
    }
    else
    {
        *pbActive = ((linkStatus[blockIdx].rxSubLinkState != SLS_OFF) &&
                     (linkStatus[blockIdx].rxSubLinkState != SLS_ILWALID) &&
                     (linkStatus[blockIdx].txSubLinkState != SLS_OFF) &&
                     (linkStatus[blockIdx].txSubLinkState != SLS_ILWALID));
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
{
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "Invalid link ID %u when reading pad lane register!\n", linkId);
        return RC::BAD_PARAMETER;
    }

    if (lane >= GetSublinkWidth(linkId))
    {
        Printf(Tee::PriError, "Invalid lane %u when reading pad lane register!\n", lane);
        return RC::BAD_PARAMETER;
    }

    RC rc;
    LWSWITCH_CTRL_READ_UPHY_PAD_LANE_REG readUphyParams = { };
    readUphyParams.link = linkId;
    readUphyParams.lane = lane;
    readUphyParams.addr = addr;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                           LibInterface::CONTROL_LWSWITCH_READ_UPHY_PAD_LANE_REG,
                                           &readUphyParams,
                                           sizeof(readUphyParams)));

    *pData = static_cast<UINT16>(readUphyParams.phy_config_data);
    return rc;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
{
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "Invalid link ID %u when writing pad lane register!\n", linkId);
        return RC::BAD_PARAMETER;
    }

    if (lane >= GetSublinkWidth(linkId))
    {
        Printf(Tee::PriError, "Invalid lane %u when writing pad lane register!\n", lane);
        return RC::BAD_PARAMETER;
    }

    RegHal &regs = Regs();

    // Priv protection registers are backwards.  ENABLE does not mean write protection
    // is enabled for the register, but rather that level is allowed to write the regiter
    if (!regs.Test32(linkId, MODS_PLWL_BR0_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE, lane))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PAD registers may not be written due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    UINT32 wData = regs.SetField(MODS_PLWL_BR0_PAD_CTL_6_CFG_WDATA, data);
    regs.SetField(&wData, MODS_PLWL_BR0_PAD_CTL_6_CFG_ADDR, addr);
    regs.SetField(&wData, MODS_PLWL_BR0_PAD_CTL_6_CFG_WDS_ON);
    regs.Write32(linkId, MODS_PLWL_BR0_PAD_CTL_6, lane, wData);
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    if (ioctl >= DoGetMaxLinkGroups())
    {
        Printf(Tee::PriError, "Invalid ioctl %u when reading pll register!\n", ioctl);
        return RC::BAD_PARAMETER;
    }

    const UINT32 linksPerIoctl = GetMaxLinks() / GetMaxLinkGroups();
    const UINT32 linkId = ioctl * linksPerIoctl;

    RegHal &regs = Regs();

    // Priv protection registers are backwards.  ENABLE does not mean write protection
    // is enabled for the register, but rather that level is allowed to write the regiter
    if (!regs.Test32(linkId, MODS_PLWL_BR0_PRIV_LEVEL_MASK_COM0_WRITE_PROTECTION_LEVEL0_ENABLE))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PLL registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }
    UINT32 pllCtl4 = regs.Read32(linkId, MODS_PLWL_BR0_PLL_CTL_4);
    regs.SetField(&pllCtl4, MODS_PLWL_BR0_PLL_CTL_4_CFG_ADDR, addr);
    regs.SetField(&pllCtl4, MODS_PLWL_BR0_PLL_CTL_4_CFG_RDS_ON);
    regs.Write32(linkId, MODS_PLWL_BR0_PLL_CTL_4, pllCtl4);
    *pData = static_cast<UINT16>(regs.Read32(linkId, MODS_PLWL_BR0_PLL_CTL_5_CFG_RDATA));
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoSetRegLogLinkMask(UINT64 linkMask)
{
    const UINT64 maxLinkMask = GetMaxLinkMask();
    m_UphyRegsLinkMask = (linkMask & maxLinkMask);
    m_UphyRegsIoctlMask = 0;

    RC rc;
    vector<LinkStatus> linkStatus;
    CHECK_RC(GetLinkStatus(&linkStatus));

    const UINT32 linksPerGroup = GetMaxLinks() / GetMaxLinkGroups();
    for (UINT32 lwrLink = 0; lwrLink < static_cast<UINT32>(linkStatus.size()); lwrLink++)
    {
        if ((linkStatus[lwrLink].rxSubLinkState == SLS_OFF) ||
            (linkStatus[lwrLink].rxSubLinkState == SLS_ILWALID) ||
            (linkStatus[lwrLink].txSubLinkState == SLS_OFF) ||
            (linkStatus[lwrLink].txSubLinkState == SLS_ILWALID))
        {
            m_UphyRegsLinkMask &= ~(1ULL << lwrLink);
        }
        else
        {
            m_UphyRegsIoctlMask |= 1U << (lwrLink / linksPerGroup);
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoSetupLatencyBins(const LatencyBins &latencyBins) const
{
    LWSWITCH_SET_LATENCY_BINS params = {};
    for (UINT32 lwrVc = 0; lwrVc < LWSWITCH_MAX_VCS; lwrVc++)
    {
        if (!(GetLatencyBilwcMask() & (1 << lwrVc)))
        {
            params.bin[lwrVc] = s_DefaultLatencyBins;
            continue;
        }

        params.bin[lwrVc].lowThreshold = latencyBins.lowThresholdNs;
        params.bin[lwrVc].medThreshold = latencyBins.midThresholdNs;
        params.bin[lwrVc].hiThreshold  = latencyBins.highThresholdNs;
    }
    return GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_LWSWITCH_SET_LATENCY_BINS,
                                            &params,
                                            sizeof(params));
}

//------------------------------------------------------------------------------
RC LwSwitchLwLink::DoClearLatencyBinCounts() const
{
    StickyRC rc;
    for (UINT32 lwrVc = 0; lwrVc < LWSWITCH_MAX_VCS; lwrVc++)
    {
        if (!(GetLatencyBilwcMask() & (1 << lwrVc)))
            continue;

        LWSWITCH_GET_INTERNAL_LATENCY params = { };
        params.vc_selector = lwrVc;
        rc = GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_LWSWITCH_GET_INTERNAL_LATENCY,
                                              &params,
                                              sizeof(params));

    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchLwLink::DoGetLatencyBinCounts
(
    vector<vector<LatencyCounts>> *pLatencyCounts
) const
{
    MASSERT(pLatencyCounts);
    pLatencyCounts->clear();

    StickyRC rc;
    for (UINT32 lwrVc = 0; lwrVc < LWSWITCH_MAX_VCS; lwrVc++)
    {
        if (!(GetLatencyBilwcMask() & (1 << lwrVc)))
            continue;

        LWSWITCH_GET_INTERNAL_LATENCY params = { };
        params.vc_selector = lwrVc;
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
            LibInterface::CONTROL_LWSWITCH_GET_INTERNAL_LATENCY,
            &params,
            sizeof(params)));

        pLatencyCounts->emplace_back();
        for (UINT32 lwrPort = 0; lwrPort < LWSWITCH_MAX_PORTS; lwrPort++)
        {
            pLatencyCounts->back().push_back({ lwrVc,
                                               params.elapsed_time_msec,
                                               params.egressHistogram[lwrPort].low,
                                               params.egressHistogram[lwrPort].medium,
                                               params.egressHistogram[lwrPort].high,
                                               params.egressHistogram[lwrPort].panic });
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
FLOAT64 LwSwitchLwLink::GetLinkDataRateEfficiency(UINT32 linkId) const
{
    FLOAT64 efficiency = 1.0;
    const UINT32 blklen = Regs().Read32(linkId, MODS_PLWL_LINK_CONFIG_BLKLEN);
    if (!Regs().TestField(blklen, MODS_PLWL_LINK_CONFIG_BLKLEN_OFF))
    {
        static const UINT32 BLOCK_SIZE  = 16;
        static const UINT32 FOOTER_BITS = 2;
        const FLOAT64 frameLength = (BLOCK_SIZE * blklen) + FOOTER_BITS;
        efficiency = (frameLength - FOOTER_BITS)  /  frameLength;
    }
    return efficiency;
}

//-----------------------------------------------------------------------------
LwLink::ErrorCounts * LwSwitchLwLink::GetCachedErrorCounts(UINT32 linkId)
{
    MASSERT(m_CachedErrorCounts.size() > linkId);
    return &(m_CachedErrorCounts[linkId]);
}
