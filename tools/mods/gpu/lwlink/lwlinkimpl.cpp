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

#include "gpu/lwlink/lwlinkimpl.h"
#include "core/include/errormap.h"
#include "core/include/mle_protobuf.h"
#include "core/utility/obfuscate_string.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080lwlink.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlsleepstate.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_lwlink_libif.h"
#include "lwlink.h"
#include "lwlink_lib_ctrl.h"
#include "lwlink_mle_colw.h"

#include <climits>
#include <numeric>

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
/* virtual */ bool LwLinkImpl::DoAreLanesReversed(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return false;
    }

    return m_LinkInfo[linkId].bLanesReversed;
}

//------------------------------------------------------------------------------
//! \brief Check that the link mask is valid
//!
//! \param linkMask : Link mask to check
//! \param pFunc    : Function doing the checking
//!
//! \return OK if successful, not OK otherwise
RC LwLinkImpl::CheckLinkMask(UINT64 linkMask, const char *pFunc) const
{
    StickyRC rc;
    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    while (lwrLink != -1)
    {
        if (!IsLinkValid(lwrLink))
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unknown linkId %u on LwLink device %s\n",
                   pFunc, lwrLink, GetDevice()->GetName().c_str());
            rc = RC::BAD_PARAMETER;
        }

        if (!IsLinkActive(lwrLink))
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Inactive linkId %u on LwLink device %s\n",
                   pFunc, lwrLink, GetDevice()->GetName().c_str());
            rc = RC::SOFTWARE_ERROR;
        }
        lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1);
    }
    return OK;
}

//-----------------------------------------------------------------------------
template <>
RC LwLinkImpl::ValidateLinkState(LwLink::SubLinkState state, const lwlink_link_state &linkState)
{
    switch (state)
    {
        case LwLink::SLS_OFF:
            // Transitioning to off after initial training (pseudo-clean shutdown) leaves
            // the link mode in reset
            if ((linkState.linkMode != lwlink_link_mode_reset) &&
                (linkState.linkMode != lwlink_link_mode_off) &&
                (linkState.linkMode != lwlink_link_mode_swcfg))
            {
                return RC::HW_STATUS_ERROR;
            }
            // If top level linkMode is off/reset there is no need to check sublink states
            //
            // Transitioning to off from safe (without a previous HS transition) will leave
            // the link in swcfg because lwlipt is not brought out of reset until HS mode
            if ((linkState.linkMode == lwlink_link_mode_swcfg) &&
                ((linkState.txSubLinkMode != lwlink_tx_sublink_mode_off) ||
                 (linkState.rxSubLinkMode != lwlink_rx_sublink_mode_off)))
            {
                return RC::HW_STATUS_ERROR;
            }
            break;
        case LwLink::SLS_SAFE_MODE:
            if ((linkState.linkMode != lwlink_link_mode_swcfg) ||
                (linkState.txSubLinkMode != lwlink_tx_sublink_mode_safe) ||
                (linkState.rxSubLinkMode != lwlink_rx_sublink_mode_safe))
            {
                return RC::HW_STATUS_ERROR;
            }
            break;
        case LwLink::SLS_HIGH_SPEED:
            if ((linkState.linkMode != lwlink_link_mode_active) ||
                ((linkState.txSubLinkMode != lwlink_tx_sublink_mode_hs) &&
                 (linkState.txSubLinkMode != lwlink_tx_sublink_mode_single_lane)) ||
                ((linkState.rxSubLinkMode != lwlink_rx_sublink_mode_hs) &&
                 (linkState.rxSubLinkMode != lwlink_rx_sublink_mode_single_lane)))
            {
                return RC::HW_STATUS_ERROR;
            }
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//------------------------------------------------------------------------------
/*virtual*/ RC LwLinkImpl::ChangeLinkState
(
    SubLinkState fromState
   ,SubLinkState toState
   ,const EndpointInfo &locEp
   ,const EndpointInfo &remEp
)
{
    StickyRC rc;
    lwlink_train_intranode_conn trainParams = { };
    UINT64 lwlinkMask = 0;

    trainParams.status                         = LWL_SUCCESS;
    trainParams.srcEndPoint.nodeId             = 0;
    trainParams.srcEndPoint.pciInfo.domain     = static_cast<LwU16>(locEp.pciDomain);
    trainParams.srcEndPoint.pciInfo.bus        = static_cast<LwU16>(locEp.pciBus);
    trainParams.srcEndPoint.pciInfo.device     = static_cast<LwU16>(locEp.pciDevice);
    trainParams.srcEndPoint.pciInfo.function   = static_cast<LwU16>(locEp.pciFunction);
    trainParams.srcEndPoint.linkIndex          = locEp.linkNumber;
    lwlinkMask                                 |= (1ULL << locEp.linkNumber);

    trainParams.dstEndPoint.nodeId             = 0;
    trainParams.dstEndPoint.pciInfo.domain     = static_cast<LwU16>(remEp.pciDomain);
    trainParams.dstEndPoint.pciInfo.bus        = static_cast<LwU16>(remEp.pciBus);
    trainParams.dstEndPoint.pciInfo.device     = static_cast<LwU16>(remEp.pciDevice);
    trainParams.dstEndPoint.pciInfo.function   = static_cast<LwU16>(remEp.pciFunction);
    trainParams.dstEndPoint.linkIndex          = remEp.linkNumber;

    switch (toState)
    {
        case SLS_OFF:
            trainParams.trainTo = lwlink_train_conn_to_off;
            break;
        case SLS_SAFE_MODE:
            trainParams.trainTo = (fromState == SLS_HIGH_SPEED) ?
                                      lwlink_train_conn_active_to_swcfg :
                                      lwlink_train_conn_off_to_swcfg;
            break;
        case SLS_HIGH_SPEED:
            MASSERT(fromState == SLS_SAFE_MODE);
            trainParams.trainTo = lwlink_train_conn_swcfg_to_active;
            break;

        default:
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unsupported training destination link state %s\n",
                   __FUNCTION__,
                   SubLinkStateToString(toState).c_str());
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(LwLinkDevIf::GetLwLinkLibIf()->Control(CTRL_LWLINK_TRAIN_INTRANODE_CONN,
                                                    &trainParams,
                                                    sizeof(trainParams)));

    if (trainParams.status != LWL_SUCCESS)
    {
        ErrorMap::ScopedDevInst setDevInst(static_cast<INT32>(GetDevice()->DevInst()));

        Mle::Print(Mle::Entry::lwlink_state_change)
                .lwlink_mask(lwlinkMask)
                .fromstate(Mle::ToSubLinkState(fromState))
                .tostate(Mle::ToSubLinkState(toState));

        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : LwLink driver training failed %u\n",
               __FUNCTION__, trainParams.status);
        return RC::CANNOT_SET_STATE;
    }

    rc = ValidateLinkState(toState, trainParams.srcEndState);
    rc = ValidateLinkState(toState, trainParams.dstEndState);

    return rc;
}

RC LwLinkImpl::ChangeLinkState
(
    SubLinkState fromState,
    SubLinkState toState,
    const vector<pair<EndpointInfo, EndpointInfo>>& epPairs
)
{
    StickyRC rc;

    for (const auto& epPair : epPairs)
    {
        rc = ChangeLinkState(fromState, toState,
                             epPair.first, epPair.second);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkImpl::DoConfigurePorts(UINT32 topologyId)
{
    // Only switches support port configuration.  On all other devices it is
    // lwrrently a no-op
    return OK;
}

//-----------------------------------------------------------------------------
RC LwLinkImpl::DoDumpRegs(UINT32 linkId) const
{
    // Create static linked list of registers to dump
    //
    struct RegDesc
    {
        ModsGpuRegAddress          addr;
        const Obfuscate::RtString* pName;
        RegDesc*                   pNext;
    };

#define ADD_REG_DESC(name)                                               \
    {                                                                    \
        using CtString = Obfuscate::CtString<                            \
                    static_cast<UINT32>(MODS_ ## name), sizeof(#name)>;  \
        static constexpr CtString obfuscatedName(#name);                 \
        static RegDesc regDesc =                                         \
            { MODS_ ## name, &obfuscatedName, nullptr };                 \
        *ppTail = &regDesc;                                              \
        ppTail = &regDesc.pNext;                                         \
    }

    static RegDesc* pLwLinkVersion1Regs = nullptr;
    if (pLwLinkVersion1Regs == nullptr)
    {
        RegDesc** ppTail = &pLwLinkVersion1Regs;
        ADD_REG_DESC(PLWL_LINK_STATE);
        ADD_REG_DESC(PLWL_SL0_SLSM_STATUS_TX);
        ADD_REG_DESC(PLWL_SL0_TXSLSM_ERR_CNTL);
        ADD_REG_DESC(PLWL_LINK_CONFIG);
        ADD_REG_DESC(PLWL_SL1_PHYCFG_RX_0);
        ADD_REG_DESC(PLWL_SL1_SLSM_STATUS_RX);
        ADD_REG_DESC(PLWL_INTR);
        ADD_REG_DESC(PLWL_ERROR_COUNT1);
        ADD_REG_DESC(PLWL_SL1_ERROR_COUNT1);
        ADD_REG_DESC(PLWL_SL0_ERROR_COUNT4);
        ADD_REG_DESC(PLWL_SL1_RXSLSM_ERR_CNTL);
        ADD_REG_DESC(PLWL_SL1_BF_ERRLOG_1);
        ADD_REG_DESC(PLWL_SL1_BF_ERRLOG_2);
        ADD_REG_DESC(PLWL_SL1_RXSLSM_TIMEOUT_0_LOG);

        ADD_REG_DESC(LWLDL_TOP_LINK_STATE);
        ADD_REG_DESC(LWLDL_TX_SLSM_STATUS_TX);
        ADD_REG_DESC(LWLDL_TX_TXSLSM_ERR_CNTL);
        ADD_REG_DESC(LWLDL_TOP_LINK_CONFIG);
        ADD_REG_DESC(LWLDL_RX_PHYCFG_RX_0);
        ADD_REG_DESC(LWLDL_RX_SLSM_STATUS_RX);
        ADD_REG_DESC(LWLDL_TOP_INTR);
        ADD_REG_DESC(LWLDL_TOP_ERROR_COUNT1);
        ADD_REG_DESC(LWLDL_RX_ERROR_COUNT1);
        ADD_REG_DESC(LWLDL_TX_ERROR_COUNT4);
        ADD_REG_DESC(LWLDL_RX_RXSLSM_ERR_CNTL);
        ADD_REG_DESC(LWLDL_RX_BF_ERRLOG_0);
        ADD_REG_DESC(LWLDL_RX_BF_ERRLOG_2);
    }
#undef ADD_REG_DESC

    // Dump the registers
    //
    const RegHal& regs = Regs();
    string regString;

    regString += Utility::StrPrintf("LwLink register dump on %s Link %u:\n",
                                    GetDevice()->GetName().c_str(), linkId);
    regString += "  Registers:\n";

    for (RegDesc* pRegDesc = pLwLinkVersion1Regs;
         pRegDesc != nullptr; pRegDesc = pRegDesc->pNext)
    {
        if (regs.IsSupported(pRegDesc->addr))
        {
            const UINT32 seed = static_cast<UINT32>(pRegDesc->addr);
            regString += Utility::StrPrintf(
                    "    LW_%-32s: 0x%0x\n",
                    pRegDesc->pName->Decode(seed).c_str(),
                    regs.Read32(linkId, pRegDesc->addr));
        }
    }

    Printf(Tee::PriNormal, "%s\n", regString.c_str());
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkImpl::DoDumpAllRegs(UINT32 linkId)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
FLOAT64 LwLinkImpl::DoGetDefaultErrorThreshold
(
    ErrorCounts::ErrId errId,
    bool bRateErrors
) const
{
    MASSERT(ErrorCounts::IsThreshold(errId));
    if (bRateErrors)
    {
        if (errId == ErrorCounts::LWL_RX_CRC_FLIT_ID)
            return 1e-15;
    }
    return 0.0;
}

//------------------------------------------------------------------------------
RC LwLinkImpl::DoGetFomStatus(vector<PerLaneFomData> * pFomData)
{
    MASSERT(pFomData);
    pFomData->clear();
    pFomData->resize(GetMaxLinks());
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Returns the rate at bits toggle on lwlink lines
//!
//! \param linkId : Link id to get the rate for
//!
//! \return Rate of the link or UNKNOWN_RATE if not known
//!
/*virtual*/ UINT32 LwLinkImpl::DoGetLineRateMbps(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return UNKNOWN_RATE;
    }

    return m_LinkInfo[linkId].lineRateMbps;
}

//------------------------------------------------------------------------------
//! \brief Returns the rate at which the lwlink link clock is running (divider of line rate)
//!
//! \param linkId : Link id to get the link clock for
//!
//! \return Rate of the link clock or UNKNOWN_RATE if not known
//!
/*virtual*/ UINT32 LwLinkImpl::DoGetLinkClockRateMhz(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return UNKNOWN_RATE;
    }

    return m_LinkInfo[linkId].linkClockMHz;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 LwLinkImpl::DoGetLinkDataRateKiBps(UINT32 linkId) const
{
    UINT32 linkDataRate = DoGetMaxLinkDataRateKiBps(linkId);

    auto pPowerImpl = GetInterface<LwLinkPowerState>();

    if ((linkDataRate != UNKNOWN_RATE) && pPowerImpl && pPowerImpl->IsSupported())
    {
        LwLinkPowerState::LinkPowerStateStatus lwrStatus;
        if (RC::OK != pPowerImpl->GetLinkPowerStateStatus(linkId, &lwrStatus))
        {
            Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
                   "%s : Unable to get power status on linkId %u on LwLink device %s\n",
                   __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        }
        else if ((lwrStatus.rxSubLinkLwrrentPowerState != LwLinkPowerState::SLS_PWRM_FB) ||
                 (lwrStatus.txSubLinkLwrrentPowerState != LwLinkPowerState::SLS_PWRM_FB))
        {
            linkDataRate /= m_LinkInfo[linkId].sublinkWidth;
        }
    }

    return linkDataRate;
}

//------------------------------------------------------------------------------
//! \brief Get the LwLink version for the specified link
//!
//! \return LwLink version for the specified link
//!
/* virtual */ UINT32 LwLinkImpl::DoGetLinkVersion(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return 0;
    }
    return m_LinkInfo[linkId].version;
}

//------------------------------------------------------------------------------
UINT32 LwLinkImpl::DoGetMaxLinkDataRateKiBps(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return UNKNOWN_RATE;
    }

    return m_LinkInfo[linkId].maxLinkDataRateKiBps;
}

//------------------------------------------------------------------------------
//! \brief Get maximum per-link per-direction bandwidthfor this device in KiBps
//!
//! \param lineRateMbps : Rate that the link lines are toggling in Mbps
//!
//! \return Per link BW in KiBps
//!
/* virtual */ UINT32 LwLinkImpl::DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const
{
    if (lineRateMbps == UNKNOWN_RATE)
    {
        switch (GetLinkVersion(0))
        {
            case 1:
                lineRateMbps = 20000;
                break;
            case 2:
                lineRateMbps = 25782;
                break;
            case 3:
                lineRateMbps = 50000;
                break;
            default:
                Printf(Tee::PriError, GetTeeModule()->GetCode(),
                       "%s : Unknown link version %u\n",
                       __FUNCTION__, GetLinkVersion(0));
                MASSERT(0);
                break;
        }
    }

    if (lineRateMbps == UNKNOWN_RATE)
        return 0;

    MASSERT(!m_LinkInfo.empty());

    UINT32 sublinkWidth = 0;
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        const UINT32 lwrSublinkWidth = GetSublinkWidth(linkId);
        if (lwrSublinkWidth != 0)
        {
            sublinkWidth = lwrSublinkWidth;
            break;
        }
    }

    return CallwlateLinkBwKiBps(lineRateMbps, sublinkWidth);
}

//------------------------------------------------------------------------------
//! \brief Get maximum total bandwidth for this device in KiBps
//!
//! \param lineRateMbps : Rate that the link lines are toggling in Mbps
//!
//! \return Total bandwidth in KiBps
//!
/* virtual */ UINT32 LwLinkImpl::DoGetMaxTotalBwKiBps(UINT32 lineRateMbps) const
{
    return GetMaxPerLinkPerDirBwKiBps(lineRateMbps) * GetMaxLinks();
}

//-----------------------------------------------------------------------------
RC LwLinkImpl::DoGetPciInfo
(
    UINT32  linkId,
    UINT32 *pDomain,
    UINT32 *pBus,
    UINT32 *pDev,
    UINT32 *pFunc
)
{
    UINT16 domain, bus, device, function;
    GetDevice()->GetId().GetPciDBDF(&domain, &bus, &device, &function);
    if (pDomain)
        *pDomain = static_cast<UINT32>(domain);
    if (pBus)
        *pBus = static_cast<UINT32>(bus);
    if (pDev)
        *pDev = static_cast<UINT32>(device);
    if (pFunc)
        *pFunc = static_cast<UINT32>(function);
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Returns the reference clock speed in MHz for the link
//!
//! \param linkId : Link id to get the rate for
//!
//! \return Reference clock speed or UNKNOWN_RATE if not known
//!
/*virtual*/ UINT32 LwLinkImpl::DoGetRefClkSpeedMhz(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return UNKNOWN_RATE;
    }

    return m_LinkInfo[linkId].refClkSpeedMhz;
}

//------------------------------------------------------------------------------
UINT32 LwLinkImpl::DoGetRxdetLaneMask(UINT32 linkId) const
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid linkId %u when retrieving RxDet lane mask\n", linkId);
        return ILWALID_LANE_MASK;
    }
    return m_LinkInfo[linkId].rxdetLaneMask;
}

//------------------------------------------------------------------------------
//! \brief Get the endpoint on the remote device connected to the specified
//! local link
//!
//! \param locLinkId         : Link ID on this device to query
//! \param pRemoteEndpoint   : Pointer to returned remote endpoint information
//!
//! \return OK if a remote device was found, not OK otherwise
//!
/* virtual */ RC LwLinkImpl::DoGetRemoteEndpoint
(
    UINT32 locLinkId,
    EndpointInfo *pRemoteEndpoint
) const
{
    RC rc;
    CHECK_RC(CheckLinkMask(1LLU << locLinkId, __FUNCTION__));

    *pRemoteEndpoint  = m_EndpointInfo[locLinkId];
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Get the sublink width for the specified link
//!
//! \param linkId : Link ID on this device to query
//!
//! \return sublink width for the specified link
//!
/* virtual */ UINT32 LwLinkImpl::DoGetSublinkWidth(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return 0;
    }

    return m_LinkInfo[linkId].sublinkWidth;
}

//------------------------------------------------------------------------------
//! \brief Initialize the lwlink
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkImpl::DoInitialize(LibDevHandles handles)
{
    RC rc;

    m_LwLinkRegs.Initialize(GetDevice());

    m_EndpointInfo.resize(GetMaxLinks());

    if (SupportsInterface<LwLinkPowerState>())
    {
        if (m_PowerStateClaimMutex == Tasker::NoMutex())
        {
            m_PowerStateClaimMutex = Tasker::CreateMutex("LwLinkPowerState", Tasker::mtxUnchecked);
            m_OriginalPowerStates.resize(GetMaxLinks());
        }
    }

    m_Initialized = true;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize information requiring discovery to be complete
//!
//! \return OK if successful, not OK otherwise
//!
RC LwLinkImpl::DoInitializePostDiscovery()
{
    RC rc;

    // This function may be called multiple times if some links fail to train
    if (m_PostDiscovery)
        m_LinkInfo.clear();

    CHECK_RC(GetLinkInfo(&m_LinkInfo));

    m_PostDiscovery = true;

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Check if the specified link ID is active on this device
//!
//! \param linkId : Link ID on this device to query
//!
//! \return true if active, false otherwise
//!
/* virtual */ bool LwLinkImpl::DoIsLinkActive(UINT32 linkId) const
{
    MASSERT(IsInitialized());

    if (linkId >= m_EndpointInfo.size())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return false;
    }

    return m_EndpointInfo[linkId].devType != TestDevice::TYPE_UNKNOWN;
}

//------------------------------------------------------------------------------
//! \brief Check if the specified link ID is valid on this device
//!
//! \param linkId : Link ID on this device to query
//!
//! \return true if valid, false otherwise
//!
/* virtual */ bool LwLinkImpl::DoIsLinkValid(UINT32 linkId) const
{
    MASSERT(IsPostDiscovery());

    if (linkId >= m_LinkInfo.size())
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return false;
    }

    return m_LinkInfo[linkId].bValid;
}

//------------------------------------------------------------------------------
RC LwLinkImpl::DoIsPerLaneEccEnabled(UINT32 linkId, bool *pbPerLaneEccEnabled) const
{
    MASSERT(pbPerLaneEccEnabled);
    *pbPerLaneEccEnabled = false;
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkImpl::DoSetLinkState(UINT64 linkMask, SubLinkState state)
{
    RC rc;

    if ((state != SLS_OFF) &&
        (state != SLS_SAFE_MODE) &&
        (state != SLS_HIGH_SPEED))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid link state to set (%d), only OFF, SAFE and HIGH_SPEED are valid\n",
               __FUNCTION__,
               state);
        return RC::SOFTWARE_ERROR;
    }

    vector<LinkStatus> linkStatus;
    CHECK_RC(GetLinkStatus(&linkStatus));

    typedef pair<EndpointInfo, EndpointInfo> EpPair;
    vector<EpPair> epPairs;

    SubLinkState fromState = SLS_ILWALID;

    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (!(linkMask & (1ULL << linkId)) || !IsLinkActive(linkId))
            continue;

        EndpointInfo remEp;
        rc = GetRemoteEndpoint(linkId, &remEp);
        if (OK != rc)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Link training not supported from devices %s!\n",
                   __FUNCTION__, GetDevice()->GetName().c_str());
            return rc;
        }

        if ((remEp.devType != TestDevice::TYPE_IBM_NPU) &&
            (remEp.devType != TestDevice::TYPE_LWIDIA_GPU) &&
            (remEp.devType != TestDevice::TYPE_LWIDIA_LWSWITCH) &&
            (remEp.devType != TestDevice::TYPE_TEGRA_MFG) &&
            (remEp.devType != TestDevice::TYPE_TREX))
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unable to train remote device type %s for linkId %u on LwLink device %s\n",
                   __FUNCTION__,
                   TestDevice::TypeToString(remEp.devType).c_str(),
                   linkId,
                   GetDevice()->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }

        if (linkId >= linkStatus.size())
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unknown linkStatus for linkId %u on LwLink device %s\n",
                   __FUNCTION__, linkId, GetDevice()->GetName().c_str());
            return RC::SOFTWARE_ERROR;
        }

        SubLinkState rxSubLinkState = linkStatus[linkId].rxSubLinkState;
        LinkState linkState = linkStatus[linkId].linkState;
        if (rxSubLinkState == SLS_SINGLE_LANE || linkState == LS_SLEEP)
            rxSubLinkState = SLS_HIGH_SPEED;
        SubLinkState txSubLinkState = linkStatus[linkId].txSubLinkState;
        if (txSubLinkState == SLS_SINGLE_LANE || linkState == LS_SLEEP)
            txSubLinkState = SLS_HIGH_SPEED;

        if (rxSubLinkState != txSubLinkState)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Sublink state mismatch at linkId %u on LwLink device %s:\n"
                   "     RX : %s\n"
                   "     TX : %s\n",
                   __FUNCTION__, linkId, GetDevice()->GetName().c_str(),
                   SubLinkStateToString(rxSubLinkState).c_str(),
                   SubLinkStateToString(txSubLinkState).c_str());
            return RC::SOFTWARE_ERROR;
        }

        if (state == rxSubLinkState)
        {
            Printf(Tee::PriLow, "Link %u on LwLink device %s already at desired state (%s)\n",
                   linkId, GetDevice()->GetName().c_str(),
                   SubLinkStateToString(rxSubLinkState).c_str());
            continue;
        }

        if (fromState == SLS_ILWALID)
            fromState = rxSubLinkState;

        if (rxSubLinkState != fromState)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Sublink state mismatch at linkId %u on LwLink device %s:"
                   " RX state is %s, should be %s\n",
                   __FUNCTION__, linkId, GetDevice()->GetName().c_str(),
                   SubLinkStateToString(rxSubLinkState).c_str(),
                   SubLinkStateToString(fromState).c_str());
            return RC::SOFTWARE_ERROR;
        }

        EndpointInfo locEp = {};
        CHECK_RC(GetPciInfo(linkId,
                            &locEp.pciDomain,
                            &locEp.pciBus,
                            &locEp.pciDevice,
                            &locEp.pciFunction));
        locEp.linkNumber = linkId;

        if (remEp.devType == TestDevice::TYPE_TREX)
        {
            // Treat TREX links as loopback
            remEp = locEp;
        }

        if (remEp.id == GetDevice()->GetId() &&
            remEp.linkNumber != linkId)
        {
            // Only add loopout connections once
            linkMask &= ~(1ULL << remEp.linkNumber);
        }

        epPairs.emplace_back(locEp, remEp);
    }

    if (epPairs.empty())
    {
        // Nothing to do
        return RC::OK;
    }

    if (state == SLS_HIGH_SPEED)
    {
        // Relwrse and make sure all the links that are not yet in HS
        // are at least in safe mode before going to HS
        UINT64 mask = accumulate(epPairs.begin(), epPairs.end(), 0x0ULL,
                          [](UINT64 x, const auto& p)->UINT64 {return x | (1ULL << p.first.linkNumber); });
        CHECK_RC(DoSetLinkState(mask, SLS_SAFE_MODE));

        // Need to set fromState to SAFE otherwise if we are trying to train from
        // OFF directly to HS mode ChangeLinkState will fail because OFF to HS
        // directly isnt supported
        fromState = SLS_SAFE_MODE;
    }

    if (RC::OK != (rc = ChangeLinkState(fromState, state, epPairs)))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Failed to train %s from %s to %s\n",
               __FUNCTION__, GetDevice()->GetName().c_str(),
               SubLinkStateToString(fromState).c_str(),
               SubLinkStateToString(state).c_str());
        return rc;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC LwLinkImpl::DoResetTopology()
{
    m_EndpointInfo.clear();
    m_EndpointInfo.resize(GetMaxLinks());

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LwLinkImpl::DoShutdown()
{
    if (SupportsInterface<LwLinkPowerState>())
    {
        if (m_PowerStateClaimMutex != Tasker::NoMutex())
        {
            if (m_PowerStateRefCount > 0)
            {
                Printf(Tee::PriLow,
                       "LwLinkPowerState : Power state claimed on shutdown on %s\n",
                       GetDevice()->GetName().c_str());
            }
            m_PowerStateClaimMutex = Tasker::NoMutex();
            m_OriginalPowerStates.clear();
        }
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC LwLinkImpl::DoWaitForLinkInit()
{
    RC rc;
    UINT32 domain, bus, dev, func;

    set<TestDevice::Id> initializedPciDevices;

    // Need to iterate over all links here since some types of lwlink devices
    // aggregate multiple PCI devices.  Keep track of the PCI devices that have
    // been initialized on only init the links once for each PCI device
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        CHECK_RC(GetPciInfo(linkId, &domain, &bus, &dev, &func));

        TestDevice::Id lwrId;
        lwrId.SetPciDBDF(domain, bus, dev, func);
        if (initializedPciDevices.count(lwrId))
            continue;

        lwlink_device_link_init_status linkInitStatusParams    = { };
        linkInitStatusParams.devInfo.nodeId             = 0;
        linkInitStatusParams.devInfo.pciInfo.domain     = static_cast<LwU16>(domain);
        linkInitStatusParams.devInfo.pciInfo.bus        = static_cast<LwU16>(bus);
        linkInitStatusParams.devInfo.pciInfo.device     = static_cast<LwU16>(dev);
        linkInitStatusParams.devInfo.pciInfo.function   = static_cast<LwU16>(func);

        CHECK_RC(LwLinkDevIf::GetLwLinkLibIf()->Control(CTRL_LWLINK_DEVICE_LINK_INIT_STATUS,
                                                        &linkInitStatusParams,
                                                        sizeof(linkInitStatusParams)));
        if (linkInitStatusParams.status != LWL_SUCCESS)
        {
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Failed to initialize links on LwLink device %s, error %d\n",
                   __FUNCTION__, GetDevice()->GetName().c_str(), linkInitStatusParams.status);
            return RC::SOFTWARE_ERROR;
        }
        // TODO : set active based on returned initStatus?
        initializedPciDevices.insert(lwrId);
    }
    return rc;
}

//-----------------------------------------------------------------------------
UINT32 LwLinkImpl::CallwlateLinkBwKiBps(UINT32 lineRateMbps, UINT32 sublinkWidth) const
{
    UINT64 maxBwKiBps = static_cast<UINT64>(lineRateMbps) * 1000;

    // Colwert per-lane kbps to per-link kBps
    maxBwKiBps *= sublinkWidth;
    maxBwKiBps /= CHAR_BIT;

    // Colwert from kBps (base 1000) to kiBps (base 1024)
    maxBwKiBps = maxBwKiBps * 1000 / 1024;

    return static_cast<UINT32>(maxBwKiBps);
}

//-----------------------------------------------------------------------------
static map<UINT32, LwLink::ErrorCounts::ErrId> s_CounterBitToID =
{
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT,    LwLink::ErrorCounts::LWL_RX_CRC_FLIT_ID   },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0, LwLink::ErrorCounts::LWL_RX_CRC_LANE0_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1, LwLink::ErrorCounts::LWL_RX_CRC_LANE1_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L2, LwLink::ErrorCounts::LWL_RX_CRC_LANE2_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L3, LwLink::ErrorCounts::LWL_RX_CRC_LANE3_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L4, LwLink::ErrorCounts::LWL_RX_CRC_LANE4_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L5, LwLink::ErrorCounts::LWL_RX_CRC_LANE5_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L6, LwLink::ErrorCounts::LWL_RX_CRC_LANE6_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L7, LwLink::ErrorCounts::LWL_RX_CRC_LANE7_ID  },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_MASKED,  LwLink::ErrorCounts::LWL_RX_CRC_MASKED_ID },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_REPLAY,      LwLink::ErrorCounts::LWL_TX_REPLAY_ID     },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_REPLAY,      LwLink::ErrorCounts::LWL_RX_REPLAY_ID     },  //$
    { LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_RECOVERY,    LwLink::ErrorCounts::LWL_TX_RECOVERY_ID   }   //$
};

//-----------------------------------------------------------------------------
LwLink::ErrorCounts::ErrId LwLinkImpl::ErrorCounterBitToId(UINT32 counterBit)
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
UINT32 LwLinkImpl::GetErrorCounterControlMask(bool bIncludePerLane)
{
    UINT32 counterMask = LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT    |
                         LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_REPLAY      |
                         LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_RECOVERY;
    if (bIncludePerLane)
    {
        counterMask |= LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L2 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L3 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L4 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L5 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L6 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L7;
    }
    return counterMask;
}

//-----------------------------------------------------------------------------
// Static LwLink Implementations
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/* static */ ModsGpuRegValue LwLinkImpl::SystemLineRateToRegVal(SystemLineRate lineRate)
{
    switch (lineRate)
    {
        case SystemLineRate::GBPS_50_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_50_00000_GBPS;
        case SystemLineRate::GBPS_16_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_16_00000_GBPS;
        case SystemLineRate::GBPS_20_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_20_00000_GBPS;
        case SystemLineRate::GBPS_25_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_25_00000_GBPS;
        case SystemLineRate::GBPS_25_7:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_25_78125_GBPS;
        case SystemLineRate::GBPS_32_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_32_00000_GBPS;
        case SystemLineRate::GBPS_40_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_40_00000_GBPS;
        case SystemLineRate::GBPS_53_1:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_53_12500_GBPS;
        case SystemLineRate::GBPS_28_1:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_28_12500_GBPS;
        case SystemLineRate::GBPS_100_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_100_00000_GBPS;
        case SystemLineRate::GBPS_106_2:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_106_25000_GBPS;
        default:
            MASSERT(!"Unknown system line rate");
            break;
    }
    return MODS_REGISTER_VALUE_NULL;
}

//-----------------------------------------------------------------------------
/* static */ ModsGpuRegValue LwLinkImpl::SystemLineCodeToRegVal(SystemLineCode lineCode)
{
    switch (lineCode)
    {
        case SystemLineCode::NRZ:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LINE_CODE_MODE_NRZ;
        case SystemLineCode::NRZ_128B130:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LINE_CODE_MODE_NRZ_128B130;
        case SystemLineCode::PAM4:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LINE_CODE_MODE_PAM4;
        default:
            MASSERT(!"Unknown system line code");
            break;
    }
    return MODS_REGISTER_VALUE_NULL;
}

//-----------------------------------------------------------------------------
/* static */ ModsGpuRegValue LwLinkImpl::SystemTxTrainAlgToRegVal(SystemTxTrainAlg txTrainAlg)
{
    switch (txTrainAlg)
    {
        case SystemTxTrainAlg::A0_SINGLE_PRESET:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_TXTRAIN_OPTIMIZATION_ALGORITHM_A0;
        case SystemTxTrainAlg::A1_PRESET_ARRAY:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_TXTRAIN_OPTIMIZATION_ALGORITHM_A1;
        case SystemTxTrainAlg::A2_FINE_GRAINED_EXHAUSTIVE:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_TXTRAIN_OPTIMIZATION_ALGORITHM_A2;
        case SystemTxTrainAlg::A4_FOM_CENTROID:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_TXTRAIN_OPTIMIZATION_ALGORITHM_A4;
        default:
            MASSERT(!"Unknown system tx train alg");
            break;
    }
    return MODS_REGISTER_VALUE_NULL;
}

//-----------------------------------------------------------------------------
/* static */ ModsGpuRegValue LwLinkImpl::SystemBlockCodeToRegVal(SystemBlockCodeMode blockCode)
{
    switch (blockCode)
    {
        case SystemBlockCodeMode::OFF:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_BLOCK_CODE_MODE_OFF;
        case SystemBlockCodeMode::ECC96:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_BLOCK_CODE_MODE_ECC96_ENABLED;
        case SystemBlockCodeMode::ECC88:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_BLOCK_CODE_MODE_ECC88_ENABLED;
        case SystemBlockCodeMode::ECC89:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_BLOCK_CODE_MODE_ECC89_ENABLED;
        default:
            MASSERT(!"Unknown system block code");
            break;
    }
    return MODS_REGISTER_VALUE_NULL;
}

//-----------------------------------------------------------------------------
/* static */ ModsGpuRegValue LwLinkImpl::SystemRefClockModeToRegVal(SystemRefClockMode refClockMode)
{
    switch (refClockMode)
    {
        case SystemRefClockMode::COMMON:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_REFERENCE_CLOCK_MODE_COMMON;
        case SystemRefClockMode::RESERVED:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_REFERENCE_CLOCK_MODE_RESERVED;
        case SystemRefClockMode::NON_COMMON_NO_SS:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_REFERENCE_CLOCK_MODE_NON_COMMON_NO_SS;
        case SystemRefClockMode::NON_COMMON_SS:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_REFERENCE_CLOCK_MODE_NON_COMMON_SS;
        default:
            MASSERT(!"Unknown system ref clock mode");
            break;
    }
    return MODS_REGISTER_VALUE_NULL;
}


//-----------------------------------------------------------------------------
/* static */ ModsGpuRegValue LwLinkImpl::SystemChannelTypeToRegVal(SystemChannelType channelType)
{
    switch (channelType)
    {
        case SystemChannelType::GENERIC:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_GENERIC;
        case SystemChannelType::ACTIVE_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_ACTIVE_0;
        case SystemChannelType::ACTIVE_1:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_ACTIVE_1;
        case SystemChannelType::ACTIVE_CABLE_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_ACTIVE_CABLE_0;
        case SystemChannelType::ACTIVE_CABLE_1:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_ACTIVE_CABLE_1;
        case SystemChannelType::OPTICAL_CABLE_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_OPTICAL_CABLE_0;
        case SystemChannelType::OPTICAL_CABLE_1:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_OPTICAL_CABLE_1;
        case SystemChannelType::DIRECT_0:
            return MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE_DIRECT_0;
        default:
            MASSERT(!"Unknown system ref clock mode");
            break;
    }
    return MODS_REGISTER_VALUE_NULL;
}

//-----------------------------------------------------------------------------
RC LwLinkImpl::DoCheckPowerStateAvailable
(
    LwLinkPowerState::SubLinkPowerState sls,
    bool bHwControl
)
{
    Tasker::MutexHolder mh(m_PowerStateClaimMutex);

    if (m_ClaimedPowerState == ClaimState::NONE)
    {
        // Warn but do not fail if the state is not properly claimed (ARCH/RM tests may
        // not be claiming states)
        Printf(Tee::PriWarn,
               "Changing LwLink power state on %s without claiming power state ownership\n",
               GetDevice()->GetName().c_str());
        return RC::OK;
    }
    else if ((m_ClaimedPowerState == ClaimState::ALL_STATES) ||
             (m_PowerStateRefCount == 0))
    {
        // If all states are claimed then all states are available, similarly if the
        // refCount is zero then the claim is in the process of restoring the original
        // values so allow any state
        return RC::OK;
    }
    const ClaimState desiredState = (sls == LwLinkPowerState::SLS_PWRM_FB) ?
         ClaimState::FULL_BANDWIDTH :
        (bHwControl ? ClaimState::HW_CONTROL : ClaimState::SINGLE_LANE);

    return (desiredState == m_ClaimedPowerState) ? RC::OK : RC::RESOURCE_IN_USE;
}

//-----------------------------------------------------------------------------
RC LwLinkImpl::DoClaimPowerState(ClaimState state)
{
    Tasker::MutexHolder mh(m_PowerStateClaimMutex);

    if (m_PowerStateRefCount)
    {
        // If a test has claimed all states it means that the test needs full control
        // to toggle states as it sees fit while it is running.  Therefore it needs
        // to be the only owner
        if (m_ClaimedPowerState == ClaimState::ALL_STATES)
        {
            Printf(Tee::PriError,
                   "%s LwLink power state ALL_STATES already claimed\n",
                   GetDevice()->GetName().c_str());
            return RC::RESOURCE_IN_USE;
        }
        else if (state != m_ClaimedPowerState)
        {
            Printf(Tee::PriError,
                   "LwLink power state already claimed on %s as %s, requested %s\n",
                   GetDevice()->GetName().c_str(),
                   LwLinkPowerState::ClaimStateToString(m_ClaimedPowerState).c_str(),
                   LwLinkPowerState::ClaimStateToString(state).c_str());
            return RC::RESOURCE_IN_USE;
        }
        Printf(Tee::PriLow,
               "Incrementing reference count for LwLink power state on %s as %s\n",
               GetDevice()->GetName().c_str(),
               LwLinkPowerState::ClaimStateToString(state).c_str());
        m_PowerStateRefCount++;
    }
    else
    {
        RC rc;
        Printf(Tee::PriLow,
               "Claiming LwLink power state on %s as %s\n",
               GetDevice()->GetName().c_str(),
               LwLinkPowerState::ClaimStateToString(state).c_str());

        // No one has yet claimed a power state for LwLink on this device, save the
        // current RX/TX state on all links
        m_ClaimedPowerState = state;
        m_PowerStateRefCount     = 1;
        for (UINT32 lwrLinkId = 0; lwrLinkId < GetMaxLinks(); lwrLinkId++)
        {
            if (IsLinkActive(lwrLinkId))
            {
                CHECK_RC(GetLinkPowerStateStatus(lwrLinkId, &m_OriginalPowerStates[lwrLinkId]));
            }
        }

        if (!GetDevice()->IsSOC() && SupportsInterface<LwLinkSleepState>())
        {
            // Lock the power mode in RM so that MODS can take control during testing.  Without
            // this the power mode can be changed due to background pstate changes
            auto sleepState = GetInterface<LwLinkSleepState>();
            CHECK_RC(sleepState->Lock());
        }

        // All means the owner can force any state, otherwise force the expected state
        if (m_ClaimedPowerState != ClaimState::ALL_STATES)
        {
            const LwLinkPowerState::SubLinkPowerState sls = (state == ClaimState::FULL_BANDWIDTH) ?
                LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
            for (UINT32 lwrLinkId = 0; lwrLinkId < GetMaxLinks(); lwrLinkId++)
            {
                if (IsLinkActive(lwrLinkId))
                {
                    CHECK_RC(RequestPowerState(lwrLinkId,
                                               sls,
                                               (state == ClaimState::HW_CONTROL)));
                }
            }
        }
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwLinkImpl::DoReleasePowerState()
{
    Tasker::MutexHolder mh(m_PowerStateClaimMutex);

    // If the test device is not set then Claim was never called or failed
    if (m_ClaimedPowerState == ClaimState::NONE)
        return RC::OK;

    m_PowerStateRefCount--;
    StickyRC rc;
    if (m_PowerStateRefCount == 0)
    {
        for (size_t lwrLinkId = 0; lwrLinkId < m_OriginalPowerStates.size(); lwrLinkId++)
        {
            auto savedLinkState = m_OriginalPowerStates[lwrLinkId];
            if (savedLinkState.rxSubLinkConfigPowerState != SLS_PWRM_ILWALID)
            {
                rc = RequestPowerState(static_cast<UINT32>(lwrLinkId),
                                       savedLinkState.rxSubLinkConfigPowerState,
                                       savedLinkState.rxHwControlledPowerState);
            }
            else if (savedLinkState.txSubLinkConfigPowerState != SLS_PWRM_ILWALID)
            {
                rc = RequestPowerState(static_cast<UINT32>(lwrLinkId),
                                       savedLinkState.txSubLinkConfigPowerState,
                                       savedLinkState.txHwControlledPowerState);
            }
            m_OriginalPowerStates[lwrLinkId] = LinkPowerStateStatus();
        }

        if (!GetDevice()->IsSOC() && SupportsInterface<LwLinkSleepState>())
        {
            auto sleepState = GetInterface<LwLinkSleepState>();
            rc = sleepState->UnLock();
        }
        m_ClaimedPowerState = ClaimState::NONE;
    }

    return rc;
}

