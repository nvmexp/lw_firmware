/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "limerocklwlink.h"
#include "core/include/errormap.h"
#include "core/include/tasker.h"
#include "core/include/mgrmgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr_impl.h"
#include "gpu/utility/js_testdevice.h"
#include "gpu/include/testdevicemgr.h"
#include "gpu/include/testdevice.h"
#include "gpu/i2c/lwswitchi2c.h"
#include "lwlink_lib_ctrl.h"
#include "ctrl_dev_lwswitch.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "lwlink/minion/minion_lwlink_defines_public_ga100.h"
#include "lwlink_mle_colw.h"
#include "core/include/mle_protobuf.h"

#include <climits> // for CHAR_BIT

using namespace LwLinkDevIf;

// Field definitions in minion_lwlink_defines_public_ga100.h
#define LW_MINION_UCODE_CONFIGEOM_EOMMODE_Y                0b010
#define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_NONE       0b00
#define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_N    0b01
#define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_P    0b10
#define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_BOTH 0b11
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER        0b0001
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_MID          0b0100
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER        0b0001
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL          0b1111

#define LW_RID_RMOD_USES_RLAN 6:6

#define CMIS_SERIAL_NUMBER_OFFSET   166
#define CMIS_SERIAL_NUMBER_SIZE      16
#define CMIS_PART_NUMBER_OFFSET     148
#define CMIS_PART_NUMBER_SIZE        16
#define CMIS_HW_REVISION_OFFSET     164
#define CMIS_HW_REVISION_SIZE         2

//-----------------------------------------------------------------------------
RC LimerockLwLink::ChangeLinkState
(
    SubLinkState fromState,
    SubLinkState toState,
    const vector<pair<EndpointInfo, EndpointInfo>>& epPairs
)
{
    StickyRC rc;
    UINT64 lwlinkMask = 0;

    if (epPairs.size() == 1)
    {
        return LwLinkImpl::ChangeLinkState(fromState, toState,
                                           epPairs[0].first, epPairs[0].second);
    }
    else if (epPairs.size() > LWLINK_MAX_DEVICE_CONN)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Too many endpoints specified!\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    lwlink_train_intranode_conns_parallel trainParams = {};
    trainParams.status = LWL_SUCCESS;
    trainParams.endPointPairsCount = epPairs.size();

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

    for (UINT32 idx = 0; idx < epPairs.size(); idx++)
    {
        const EndpointInfo& locEp = epPairs[idx].first;
        const EndpointInfo& remEp = epPairs[idx].second;
        lwlink_endpoint_pair& epPair = trainParams.endPointPairs[idx];

        epPair.src.nodeId             = 0;
        epPair.src.pciInfo.domain     = static_cast<LwU16>(locEp.pciDomain);
        epPair.src.pciInfo.bus        = static_cast<LwU16>(locEp.pciBus);
        epPair.src.pciInfo.device     = static_cast<LwU16>(locEp.pciDevice);
        epPair.src.pciInfo.function   = static_cast<LwU16>(locEp.pciFunction);
        epPair.src.linkIndex          = locEp.linkNumber;

        epPair.dst.nodeId             = 0;
        epPair.dst.pciInfo.domain     = static_cast<LwU16>(remEp.pciDomain);
        epPair.dst.pciInfo.bus        = static_cast<LwU16>(remEp.pciBus);
        epPair.dst.pciInfo.device     = static_cast<LwU16>(remEp.pciDevice);
        epPair.dst.pciInfo.function   = static_cast<LwU16>(remEp.pciFunction);
        epPair.dst.linkIndex          = remEp.linkNumber;
    }

    CHECK_RC(LwLinkDevIf::GetLwLinkLibIf()->Control(CTRL_LWLINK_TRAIN_INTRANODE_CONNS_PARALLEL,
                                                    &trainParams,
                                                    sizeof(trainParams)));

    if (trainParams.status != LWL_SUCCESS)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : LwLink driver training failed %u\n",
               __FUNCTION__, trainParams.status);
        rc = RC::CANNOT_SET_STATE;
    }

    for (UINT32 idx = 0; idx < trainParams.endPointPairsCount; idx++)
    {
        StickyRC validRC;
        validRC = ValidateLinkState(toState, trainParams.endpointPairsStates[idx].srcEnd);
        validRC = ValidateLinkState(toState, trainParams.endpointPairsStates[idx].dstEnd);

        if (validRC == RC::OK)
            continue;

        lwlinkMask |= (1ULL << trainParams.endPointPairs[idx].src.linkIndex);

        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Failed to train %s, link %u from %s to %s\n",
               __FUNCTION__, GetDevice()->GetName().c_str(),
               trainParams.endPointPairs[idx].src.linkIndex,
               SubLinkStateToString(fromState).c_str(),
               SubLinkStateToString(toState).c_str());
        rc = validRC;
    }

    if (rc == RC::CANNOT_SET_STATE)
    {
        ErrorMap::ScopedDevInst setDevInst(static_cast<INT32>(GetDevice()->DevInst()));

        Mle::Print(Mle::Entry::lwlink_state_change)
            .lwlink_mask(lwlinkMask)
            .fromstate(Mle::ToSubLinkState(fromState))
            .tostate(Mle::ToSubLinkState(toState));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoAssertBufferReady(UINT32 linkId, bool ready)
{
    RegHal& regs = Regs();

    regs.Write32(linkId, MODS_LWLTLC_RX_SYS_CTRL_BUFFER_READY_BUFFERRDY, (ready)?0x1:0x0);
    regs.Write32(linkId, MODS_LWLTLC_TX_SYS_CTRL_BUFFER_READY_BUFFERRDY, (ready)?0x1:0x0);

    regs.Write32(linkId, MODS_NPORT_CTRL_BUFFER_READY_BUFFERRDY, (ready)?0x1:0x0);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoConfigurePorts(UINT32 topologyId)
{
    RC rc;

    auto pFMLibIf = TopologyManager::GetFMLibIf();
    MASSERT(pFMLibIf);

    UINT64 neaLoopbackMask = GetNeaLoopbackLinkMask();
    const UINT64 nedLoopbackMask = GetNedLoopbackLinkMask();
    for (UINT32 link = 0; link < GetMaxLinks(); link++)
    {
        FMLibInterface::PortInfo portInfo;
        portInfo.type = TestDevice::TYPE_LWIDIA_LWSWITCH;
        portInfo.nodeId = 0;
        portInfo.physicalId = topologyId;
        portInfo.portNum = link;

        FMLibInterface::PortInfo remInfo;
        CHECK_RC(pFMLibIf->GetRemotePortInfo(portInfo, &remInfo));

        const bool isTrexPort = ((TopologyManager::GetTopologyFlags() & TF_TREX)
                                && remInfo.type == TestDevice::TYPE_LWIDIA_GPU);

        if (isTrexPort)
        {
            LwLinkTrex::NcisocSrc src = GetInterface<LwLinkTrex>()->GetTrexSrc();
            LwLinkTrex::NcisocDst dst = GetInterface<LwLinkTrex>()->GetTrexDst();
            if ((src == LwLinkTrex::SRC_RX ||
                 dst == LwLinkTrex::DST_TX) &&
                !(nedLoopbackMask & (1ULL << link)))
            {
                neaLoopbackMask |= (1ULL << link);
            }

            CHECK_RC(InitializeTrex(link));
        }
    }

    if (neaLoopbackMask)
    {
        CHECK_RC(SetNearEndLoopbackMode(neaLoopbackMask,  NearEndLoopMode::NEA));
    }
    if (nedLoopbackMask)
    {
        CHECK_RC(SetNearEndLoopbackMode(nedLoopbackMask,  NearEndLoopMode::NED));
    }

    SetTopologyId(topologyId);

    if (GetDevice()->HasBug(2597550) &&
        TopologyManager::GetTopologyFlags() & TF_TREX)
    {
        // The TREXs need to be primed once before they will function correctly
        // This is a WAR for a hardware bug
        RegHal& regs = Regs();

        UINT32 data = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_ADDRESS, 0) |
                      regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ0_RAM);
        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_TREX_DYNAMIC_MODE_REQ_RAM, data);

        regs.SetField(&data, MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ1_RAM);
        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_TREX_DYNAMIC_MODE_REQ_RAM, data);

        CHECK_RC(StartTrex());
        CHECK_RC(StopTrex());
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::InitializeTrex(UINT32 link)
{
    RC rc;
    RegHal& regs = Regs();

    LwLinkTrex* pTrex = GetInterface<LwLinkTrex>();
    MASSERT(pTrex);

    UINT32 data = regs.SetField(MODS_TREX_CTRL_MODE_DYNAMIC);
    switch (pTrex->GetTrexDst())
    {
        case LwLinkTrex::DST_TX:
            data |= regs.SetField(MODS_TREX_CTRL_NCISOC_DST_TLC_TX);
            break;
        case LwLinkTrex::DST_INGRESS:
            data |= regs.SetField(MODS_TREX_CTRL_NCISOC_DST_NPG_INGRESS);
            break;
        default:
            Printf(Tee::PriError, "Unknown TREX DST = %d\n", static_cast<UINT32>(GetTrexDst()));
            return RC::ILWALID_ARGUMENT;
    }
    switch (pTrex->GetTrexSrc())
    {
        case LwLinkTrex::SRC_RX:
            data |= regs.SetField(MODS_TREX_CTRL_NCISOC_SRC_TLC_RX);
            break;
        case LwLinkTrex::SRC_EGRESS:
            data |= regs.SetField(MODS_TREX_CTRL_NCISOC_SRC_NPG_EGRESS);
            break;
        default:
            Printf(Tee::PriError, "Unknown TREX SRC = %d\n", static_cast<UINT32>(GetTrexSrc()));
            return RC::ILWALID_ARGUMENT;
    }
    regs.Write32(link, MODS_TREX_CTRL, data);

    switch (GetTrexPattern())
    {
        case LwLinkTrex::DP_ALL_ZEROS:
            data = regs.SetField(MODS_TREX_QUEUE_CTRL_DATA_POLICY_ALL_ZEROS);
            break;
        case LwLinkTrex::DP_ALL_ONES:
            data = regs.SetField(MODS_TREX_QUEUE_CTRL_DATA_POLICY_ALL_ONES);
            break;
        case LwLinkTrex::DP_0_F:
            data = regs.SetField(MODS_TREX_QUEUE_CTRL_DATA_POLICY_0_F);
            break;
        case LwLinkTrex::DP_A_5:
            data = regs.SetField(MODS_TREX_QUEUE_CTRL_DATA_POLICY_A_5);
            break;
        default:
            Printf(Tee::PriError, "Unknown TREX Data Pattern = %d\n", static_cast<UINT32>(GetTrexPattern()));
            return RC::ILWALID_ARGUMENT;
    }
    regs.Write32(link, MODS_TREX_QUEUE_CTRL, data);

    data = regs.SetField(MODS_TREX_INITIALIZATION_REQRAM_INIT_HWINIT) |
           regs.SetField(MODS_TREX_INITIALIZATION_TAGPOOL_INIT_HWINIT) |
           regs.SetField(MODS_TREX_INITIALIZATION_DATAPATTERN_INIT_HWINIT);
    regs.Write32(link, MODS_TREX_INITIALIZATION, data);

    CHECK_RC(Tasker::PollHw(Tasker::GetDefaultTimeoutMs(),
             [&]() -> bool
             {
                 data = regs.Read32(link, MODS_TREX_INITIALIZATION);
                 return (regs.TestField(data, MODS_TREX_INITIALIZATION_REQRAM_INIT_HWINIT) &&
                         regs.TestField(data, MODS_TREX_INITIALIZATION_TAGPOOL_INIT_HWINIT) &&
                         regs.TestField(data, MODS_TREX_INITIALIZATION_DATAPATTERN_INIT_HWINIT));
             }, __FUNCTION__));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
{
    if (!IsLinkActive(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (bEnable)
        Regs().Write32(linkId, MODS_LWLDL_TX_TXLANECRC_ENABLE_EN);
    else
        Regs().Write32(linkId, MODS_LWLDL_TX_TXLANECRC_ENABLE_DIS);

    return OK;
}

//-----------------------------------------------------------------------------
void LimerockLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    MASSERT(pSettings);
    const UINT32 lineRateMbps = GetLineRateMbps(link);
    switch (lineRateMbps)
    {
        case 50000:
            pSettings->numErrors = 0x4;
            pSettings->numBlocks = 0x8;
            break;
        case 53125:
            // 53G is only PAM4 (and PAM4 is only 53G) and PAM4 requires a different target BER
            pSettings->numErrors = 0xF;
            pSettings->numBlocks = 0x5;
            break;
        default:
            pSettings->numErrors = 0x2;
            pSettings->numBlocks = 0x8;
            break;
    }
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoGetEomStatus
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

    LWSWITCH_CTRL_CONFIG_EOM comfigEomParams = {};
    comfigEomParams.link = link;
    CHECK_RC(GetEomConfigValue(mode, numErrors, numBlocks, &comfigEomParams.params));
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_CONFIG_EOM,
                                              &comfigEomParams,
                                              sizeof(comfigEomParams)));

    // Set EOM disabled
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_RX_EOM_EN_OFF, numLanes);

    UINT32 doneStatus = 0x0;
    auto pollEomDone = [&]()->bool
        {
            for (UINT32 lane = 0; lane < numLanes; lane++)
            {
                if (!regs.Test32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_4_RX_EOM_DONE, lane, doneStatus))
                    return false;
            }
            return true;
        };

    // Check that EOM_DONE is reset
    CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));

    // Set EOM programmable
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_RX_EOM_OVRD_ON, numLanes);

    // Set EOM enabled
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_RX_EOM_EN_ON, numLanes);

    // Check if EOM is done
    doneStatus = 0x1;
    CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));

    // Set EOM disabled
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_RX_EOM_EN_OFF, numLanes);

    // Set EOM not programmable
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_RX_EOM_OVRD_OFF, numLanes);

    // Get EOM status
    for (UINT32 i = 0; i < numLanes; i++)
    {
        // Only the lower 8 bits are actually status values, the upper 8 bits are
        // status flags
        status->at(i) = static_cast<UINT08>(regs.Read32(link,
                                                      MODS_LWLPHYCTL_LANE_PAD_CTL_4_RX_EOM_STATUS,
                                                      i));
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode)
{
    RegHal &regs = GetDevice()->Regs();
    MASSERT(pLineCode);
    if (!pLineCode)
        return RC::BAD_PARAMETER;

    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "Invalid linkId %u while querying line code mode\n", linkId);
        return RC::BAD_PARAMETER;
    }

    // TODO Use RMAPI to query this info once available (Bug 2824388)

    if (regs.Test32(linkId, MODS_LWLDL_TOP_LINK_CONFIG_PAM4_EN_ON))
    {
        *pLineCode = SystemLineCode::PAM4;
    }
    else if (regs.Test32(linkId, MODS_LWLDL_TOP_LINK_CONFIG_BLKLEN_128))
    {
        *pLineCode = SystemLineCode::NRZ_128B130;
    }
    else
    {
        *pLineCode = SystemLineCode::NRZ;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoGetBlockCodeMode
(
    UINT32 linkId,
    SystemBlockCodeMode *pBlockCodeMode
)
{
    RegHal &regs = GetDevice()->Regs();
    MASSERT(pBlockCodeMode);

    if (!pBlockCodeMode)
        return RC::BAD_PARAMETER;

    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "Invalid linkId %u while querying block code mode\n", linkId);
        return RC::BAD_PARAMETER;
    }

    // TODO Use RMAPI to query this info once available (Bug 2824388)
    const UINT32 eccMode = regs.Read32(linkId, MODS_LWLDL_TOP_LINK_CONFIG_ECC_MODE);
    if (eccMode == regs.LookupValue(MODS_LWLDL_TOP_LINK_CONFIG_ECC_MODE_OFF))
    {
        *pBlockCodeMode = SystemBlockCodeMode::OFF;
    }
    else if (eccMode == regs.LookupValue(MODS_LWLDL_TOP_LINK_CONFIG_ECC_MODE_ECC88))
    {
        *pBlockCodeMode = SystemBlockCodeMode::ECC88;
    }
    else if (eccMode == regs.LookupValue(MODS_LWLDL_TOP_LINK_CONFIG_ECC_MODE_ECC96))
    {
        *pBlockCodeMode = SystemBlockCodeMode::ECC96;
    }
    else
    {
        Printf(Tee::PriError, "Unknown block code mode %u for link %u\n", eccMode, linkId);
        return RC::BAD_PARAMETER;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    MASSERT(pErrorCounts);
    RC rc;

    if (GetDevice()->HasBug(3342771) && (GetCciIdFromLink(linkId) != ILWALID_CCI_ID))
    {
        return GetErrorCountsDirect(linkId, pErrorCounts);
    }

    CHECK_RC(LwSwitchLwLink::DoGetErrorCounts(linkId, pErrorCounts));

    bool bEccEnabled = false;
    CHECK_RC(IsPerLaneEccEnabled(linkId, &bEccEnabled));

    if (bEccEnabled)
    {
        LWSWITCH_GET_LWLINK_ECC_ERRORS_PARAMS eccParams = { };
        eccParams.linkMask = 1ULL << linkId;
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_GET_LWLINK_ECC_ERRORS,
                                                  &eccParams,
                                                  sizeof(eccParams)));

        LWSWITCH_LINK_ECC_ERROR & linkErrors = eccParams.errorLink[linkId];
        LwU32 maxErrors = 0;
        bool  bAnyEccLaneCountFound = false;
        bool  bAnyEccLaneOverflowed = false;

        ErrorCounts * pCachedErrorCounts = GetCachedErrorCounts(linkId);

        Tasker::MutexHolder lockCache(GetErrorCountMutex());
        for (UINT32 lane = 0; lane < LWSWITCH_LWLINK_MAX_LANES; lane++)
        {
            if (!linkErrors.errorLane[lane].valid)
                continue;

            bAnyEccLaneCountFound = true;

            LwLink::ErrorCounts::ErrId errId =
                static_cast<LwLink::ErrorCounts::ErrId>(LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE0_ID + lane);

            const UINT32 lwrCount = pCachedErrorCounts->GetCount(errId) +
                                    linkErrors.errorLane[lane].eccErrorValue;
            const bool bOverflowed = (linkErrors.errorLane[lane].overflowed == LW_TRUE);
            if (bOverflowed)
                bAnyEccLaneOverflowed = true;

            if (lwrCount > maxErrors)
                maxErrors = lwrCount;
            CHECK_RC(pCachedErrorCounts->SetCount(errId, lwrCount, bOverflowed));
            CHECK_RC(pErrorCounts->SetCount(errId, lwrCount, bOverflowed));
        }
        if (bAnyEccLaneCountFound)
        {
            CHECK_RC(pCachedErrorCounts->SetCount(LwLink::ErrorCounts::LWL_PRE_FEC_ID,
                                                  maxErrors,
                                                  bAnyEccLaneOverflowed));
            CHECK_RC(pErrorCounts->SetCount(LwLink::ErrorCounts::LWL_PRE_FEC_ID,
                                            maxErrors,
                                            bAnyEccLaneOverflowed));
        }
        CHECK_RC(pErrorCounts->SetCount(LwLink::ErrorCounts::LWL_ECC_DEC_FAILED_ID,
                                        eccParams.errorLink[linkId].eccDecFailed,
                                        eccParams.errorLink[linkId].eccDecFailedOverflowed));
    }
    return rc;
}

//------------------------------------------------------------------------------
FLOAT64 LimerockLwLink::DoGetDefaultErrorThreshold
(
    LwLink::ErrorCounts::ErrId errId,
    bool bRateErrors
) const
{
    MASSERT(LwLink::ErrorCounts::IsThreshold(errId));
    if (bRateErrors)
    {
        if (errId == LwLink::ErrorCounts::LWL_RX_CRC_FLIT_ID)
            return 1e-14;
        if (errId == LwLink::ErrorCounts::LWL_PRE_FEC_ID)
            return 1e-5;
    }
    return 0.0;
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoGetFomStatus(vector<PerLaneFomData> * pFomData)
{
    MASSERT(pFomData);
    pFomData->clear();
    pFomData->resize(GetMaxLinks());

    RC rc;

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        if (!IsLinkActive(lwrLink))
            continue;

        LWSWITCH_GET_FOM_VALUES_PARAMS fomParams = {};
        fomParams.linkId = lwrLink;
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_GET_FOM_VALUES,
                                                  &fomParams,
                                                  sizeof(fomParams)));

        if (fomParams.numLanes == 0)
            continue;

        for (UINT08 lwrLane = 0; lwrLane < fomParams.numLanes; lwrLane++)
            pFomData->at(lwrLink).push_back(fomParams.figureOfMeritValues[lwrLane] & 0xFF);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetOutputLinks
(
    UINT32 inputLink
   ,UINT64 fabricBase
   ,DataType dt
   ,set<UINT32>* pPorts
) const
{
    RC rc;

    FMLibInterface::PortInfo portInfo = {};
    portInfo.type = GetDevice()->GetType();
    portInfo.nodeId = 0;
    portInfo.physicalId = GetTopologyId();
    portInfo.portNum = inputLink;

    UINT32 index = static_cast<UINT32>(fabricBase >> GetFabricRegionBits());

    CHECK_RC(TopologyManager::GetFMLibIf()->GetRidPorts(portInfo, index, pPorts));

    return rc;
}

//------------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetLinkDataRateKiBps(UINT32 linkId) const
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid linkId %u\n", linkId);
        return UNKNOWN_RATE;
    }

    LWSWITCH_GET_LWLINK_STATUS_PARAMS statusParams = {};
    RC rc = GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                             LibInterface::CONTROL_GET_LINK_STATUS,
                                             &statusParams,
                                             sizeof(statusParams));
    if (rc != RC::OK)
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Failed to get LwLink status from the driver - %s\n", rc.Message());
        return UNKNOWN_RATE;
    }

    return statusParams.linkInfo[linkId].lwlinkLinkDataRateKiBps;
}

//------------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoGetLinkInfo(vector<LinkInfo> *pLinkInfo)
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
        lwrInfo.linkClockMHz      = pLinkStat->lwlinkLinkClockMhz;
        lwrInfo.sublinkWidth      = pLinkStat->subLinkWidth;
        lwrInfo.bLanesReversed    = (pLinkStat->bLaneReversal == LW_TRUE);
        lwrInfo.rxdetLaneMask     = pLinkStat->laneRxdetStatusMask;

        if (!lwrInfo.bActive)
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        lwrInfo.lineRateMbps         = pLinkStat->lwlinkLineRateMbps;
        lwrInfo.maxLinkDataRateKiBps = pLinkStat->lwlinkLinkDataRateKiBps;

        pLinkInfo->push_back(lwrInfo);
    }

    return rc;
}

//------------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetMaxLinkDataRateKiBps(UINT32 linkId) const
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return UNKNOWN_RATE;
    }

    if (IsLinkActive(linkId))
    {
        EndpointInfo ep;
        auto * pTrex = GetInterface<LwLinkTrex>();
        if ((RC::OK == GetRemoteEndpoint(linkId, &ep)) &&
            (ep.devType == TestDevice::TYPE_TREX) &&
            (pTrex->GetTrexDst() == LwLinkTrex::DST_INGRESS) &&
            (pTrex->GetTrexSrc() == LwLinkTrex::SRC_EGRESS))

        {
            // Return unknown so that the connection rate can be solely determined by the TREX device
            return UNKNOWN_RATE;
        }
    }

    LWSWITCH_GET_LWLINK_STATUS_PARAMS statusParams = {};
    RC rc = GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                             LibInterface::CONTROL_GET_LINK_STATUS,
                                             &statusParams,
                                             sizeof(statusParams));
    if (rc != RC::OK)
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Failed to get LwLink status from Driver - %s\n", rc.Message());
        return UNKNOWN_RATE;
    }

    auto linkInfo = statusParams.linkInfo[linkId];
    if ((linkInfo.rxSublinkStatus == LWSWITCH_LWLINK_STATUS_SUBLINK_RX_STATE_SINGLE_LANE) ||
        (linkInfo.txSublinkStatus == LWSWITCH_LWLINK_STATUS_SUBLINK_TX_STATE_SINGLE_LANE))
    {
        return linkInfo.lwlinkLinkDataRateKiBps * GetSublinkWidth(linkId);
    }
    return linkInfo.lwlinkLinkDataRateKiBps;
}

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetMaxPerLinkPerDirBwKiBps(UINT32 lineRateMbps) const
{
    auto * pTrex = GetInterface<LwLinkTrex>();
    if ((TopologyManager::GetTopologyFlags() & TF_TREX) &&
        (pTrex->GetTrexDst() == LwLinkTrex::DST_INGRESS) &&
        (pTrex->GetTrexSrc() == LwLinkTrex::SRC_EGRESS))

    {
        // Return unknown so that the connection rate can be solely determined by the TREX device
        return UNKNOWN_RATE;
    }
    return LwSwitchLwLink::DoGetMaxPerLinkPerDirBwKiBps(lineRateMbps);
}

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetMaxTotalBwKiBps(UINT32 lineRateMbps) const
{
    auto * pTrex = GetInterface<LwLinkTrex>();
    if ((TopologyManager::GetTopologyFlags() & TF_TREX) &&
        (pTrex->GetTrexDst() == LwLinkTrex::DST_INGRESS) &&
        (pTrex->GetTrexSrc() == LwLinkTrex::SRC_EGRESS))

    {
        // Return unknown so that the connection rate can be solely determined by the TREX device
        return UNKNOWN_RATE;
    }
    return LwSwitchLwLink::DoGetMaxTotalBwKiBps(lineRateMbps);
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoGetLinkPowerStateStatus
(
    UINT32 linkId,
    LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
) const
{
    MASSERT(nullptr != pLinkPowerStatus);

    RC rc;

    const RegHal& regs = Regs();

    pLinkPowerStatus->rxHwControlledPowerState =
        regs.Test32(linkId, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE, 0);
    pLinkPowerStatus->rxSubLinkLwrrentPowerState =
        regs.Test32(linkId, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_LWRRENTSTATE_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->rxSubLinkConfigPowerState =
        regs.Test32(linkId, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txHwControlledPowerState =
        regs.Test32(linkId, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE, 0);
    pLinkPowerStatus->txSubLinkLwrrentPowerState =
        regs.Test32(linkId, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_LWRRENTSTATE_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txSubLinkConfigPowerState =
        regs.Test32(linkId, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoRequestPowerState
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
        regs.SetField(&rxSwCtrl, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP);
        regs.SetField(&txSwCtrl, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP);
        if (bHwControlled)
        {
            // set the bit allowing idle counting if the HW is allowed to change the power
            // state, i.e. when true == (SOFTWAREDESIRED == 1 (LP) && HARDWAREDISABLE == 0)
            regs.SetField(&rxSwCtrl, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_COUNTSTART, 1);
            regs.SetField(&txSwCtrl, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_COUNTSTART, 1);
        }
        break;
    case LwLinkPowerState::SLS_PWRM_FB:
        regs.SetField(&rxSwCtrl, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB);
        regs.SetField(&txSwCtrl, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB);
        break;
    default:
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid SubLinkPowerState = %d\n",
               __FUNCTION__, static_cast<UINT32>(state));
        return RC::BAD_PARAMETER;
    }
    regs.SetField(&rxSwCtrl,
                  MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE,
                  bHwControlled ? 0 : 1);
    regs.SetField(&txSwCtrl,
                  MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE,
                  bHwControlled ? 0 : 1);
    regs.Write32(linkId, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL, rxSwCtrl);
    regs.Write32(linkId, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL, txSwCtrl);

    return RC::OK;
}

//------------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
{
    if (!IsLinkActive(linkId))
        return 0;

    const auto pwrmIcLpEnterThresholdReg =
        RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD :
                    MODS_LWLTLC_TX_LNK_PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD;
    const auto pwrmIcIncLpIncReg = RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_INC_LPINC :
                                               MODS_LWLTLC_TX_LNK_PWRM_IC_INC_LPINC;
    const RegHal& regs = Regs();

    const FLOAT64 linkClock = GetLinkClockRateMhz(linkId);
    const FLOAT64 threshold = regs.Read32(linkId, pwrmIcLpEnterThresholdReg);
    const FLOAT64 lpInc = regs.Read32(linkId, pwrmIcIncLpIncReg);

    return static_cast<UINT32>(threshold / linkClock / 1000.0 / lpInc);
}


//-----------------------------------------------------------------------------
RC LimerockLwLink::DoClearLPCounts(UINT32 linkId)
{
    RC rc;

    Regs().Write32(linkId, MODS_LWLDL_TOP_STATS_CTRL_CLEAR_ALL_CLEAR);

    return rc;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
)
{
    RC rc;

    MASSERT(nullptr != pCount);

    if (entry)
    {
        *pCount = Regs().Read32(linkId, MODS_LWLDL_TOP_STATS_D_NUM_TX_LP_ENTER);
        *pbOverflow = *pCount == Regs().LookupMask(MODS_LWLDL_TOP_STATS_D_NUM_TX_LP_ENTER);
    }
    else
    {
        *pCount = Regs().Read32(linkId, MODS_LWLDL_TOP_STATS_D_NUM_TX_LP_EXIT);
        *pbOverflow = *pCount == Regs().LookupMask(MODS_LWLDL_TOP_STATS_D_NUM_TX_LP_EXIT);
    }

    return rc;
}

//------------------------------------------------------------------------------
// LwLinkUphy
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC LimerockLwLink::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
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
    if (!regs.Test32(linkId, MODS_LWLPHYCTL_LANE_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE, lane))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PAD registers may not be written due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    UINT32 wData = regs.SetField(MODS_LWLPHYCTL_LANE_PAD_CTL_6_CFG_WDATA, data);
    regs.SetField(&wData, MODS_LWLPHYCTL_LANE_PAD_CTL_6_CFG_ADDR, addr);
    regs.SetField(&wData, MODS_LWLPHYCTL_LANE_PAD_CTL_6_CFG_WDS_ON);
    regs.Write32(linkId, MODS_LWLPHYCTL_LANE_PAD_CTL_6, lane, wData);
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    RegHal &regs = Regs();
    // Priv protection registers are backwards.  ENABLE does not mean write protection
    // is enabled for the register, but rather that level is allowed to write the regiter
    if (!regs.Test32(ioctl, MODS_LWLPLLCTL_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PLL registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }
    UINT32 pllCtl4 = regs.Read32(ioctl, MODS_LWLPLLCTL_PLL_CTL_4);
    regs.SetField(&pllCtl4, MODS_LWLPLLCTL_PLL_CTL_4_CFG_ADDR, addr);
    regs.SetField(&pllCtl4, MODS_LWLPLLCTL_PLL_CTL_4_CFG_RDS_ON);
    regs.Write32(ioctl, MODS_LWLPLLCTL_PLL_CTL_4, pllCtl4);
    *pData = static_cast<UINT16>(regs.Read32(ioctl, MODS_LWLPLLCTL_PLL_CTL_5_CFG_RDATA));
    return RC::OK;
}

//------------------------------------------------------------------------------
// LwLinkUphy
//------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetCCCount() const
{
    return static_cast<UINT32>(m_CableControllerInfo.size());
}

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetCciIdFromLink(UINT32 linkId) const
{
    for (size_t lwrCciId = 0; lwrCciId < m_CableControllerInfo.size(); lwrCciId++)
    {
        if (m_CableControllerInfo[lwrCciId].linkMask & (1 << linkId))
        {
            return static_cast<UINT32>(lwrCciId);
        }
    }
    return ILWALID_CCI_ID;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetCCTempC(UINT32 cciId, FLOAT32 *pTemp) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller temperature\n", cciId);
        return UNKNOWN_RATE;
    }

    RC rc;
    LWSWITCH_CCI_GET_TEMPERATURE tempParams = {};
    const UINT32 linkId =
        static_cast<UINT32>(Utility::BitScanForward(m_CableControllerInfo[cciId].linkMask));
    tempParams.linkId = linkId;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_LWSWITCH_CCI_GET_TEMPERATURE,
                                              &tempParams,
                                              sizeof(tempParams)));
    *pTemp = static_cast<FLOAT32>(LW_TYPES_LW_TEMP_TO_CELSIUS_FLOAT(tempParams.temperature));
    return rc;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetCCErrorFlags(CCErrorFlags * pErrorFlags) const
{
    RC rc;
    for (size_t lwrCciId = 0; lwrCciId < m_CableControllerInfo.size(); ++lwrCciId)
    {
        const UINT08 cciPhysId = static_cast<UINT08>(GetPhysicalId(lwrCciId));

        LWSWITCH_CCI_GET_MODULE_FLAGS ccModuleFlags = { };
        ccModuleFlags.linkId =
            static_cast<LwU32>(Utility::BitScanForward(m_CableControllerInfo[lwrCciId].linkMask));
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                               LibInterface::CONTROL_LWSWITCH_CCI_GET_MODULE_FLAGS,
                                               &ccModuleFlags,
                                               sizeof(ccModuleFlags)));

        // Fatal
        if (ccModuleFlags.flags.bLModuleFirmwareFault)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::ModuleFirmwareFault });
        if (ccModuleFlags.flags.bDatapathFirmwareFault)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::DataPathFirmwareFault });
        if (ccModuleFlags.flags.bLTempHighAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::HighTempAlarm });
        if (ccModuleFlags.flags.bLTempLowAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::LowTempAlarm });
        if (ccModuleFlags.flags.bLVccHighAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::HighVoltageAlarm });
        if (ccModuleFlags.flags.bLVccLowAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::LowVoltageAlarm });
        if (ccModuleFlags.flags.bLAux1HighAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1HighAlarm });
        if (ccModuleFlags.flags.bLAux1LowAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1LowAlarm });
        if (ccModuleFlags.flags.bLAux2HighAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1HighAlarm });
        if (ccModuleFlags.flags.bLAux2LowAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1LowAlarm });
        if (ccModuleFlags.flags.bLAux3HighAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux3HighAlarm });
        if (ccModuleFlags.flags.bLAux3LowAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux3LowAlarm });
        if (ccModuleFlags.flags.bLVendorHighAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::VendorHighAlarm });
        if (ccModuleFlags.flags.bLVendorLowAlarm)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::VendorLowAlarm });

        // Non-Fatal
        if (ccModuleFlags.flags.bLTempHighWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::HighTempWarning });
        if (ccModuleFlags.flags.bLTempLowWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::LowTempWarning });
        if (ccModuleFlags.flags.bLVccHighWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::HighVoltageWarning });
        if (ccModuleFlags.flags.bLVccLowWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::LowVoltageWarning });
        if (ccModuleFlags.flags.bLAux1HighWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1HighWarning });
        if (ccModuleFlags.flags.bLAux1LowWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1LowWarning });
        if (ccModuleFlags.flags.bLAux2HighWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1HighWarning });
        if (ccModuleFlags.flags.bLAux2LowWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux1LowWarning });
        if (ccModuleFlags.flags.bLAux3HighWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux3HighWarning });
        if (ccModuleFlags.flags.bLAux3LowWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::Aux3LowWarning });
        if (ccModuleFlags.flags.bLVendorHighWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::VendorHighWarning });
        if (ccModuleFlags.flags.bLVendorLowWarn)
            pErrorFlags->push_back({ cciPhysId, CCI_ILWALID, ErrorFlagType::VendorLowWarning });

        LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ_PARAMS cmisParams = {};
        cmisParams.cageIndex = cciPhysId;
        cmisParams.bank      = 0;
        cmisParams.page      = 0x11;
        cmisParams.address   = 134;
        cmisParams.count     = 19;
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ,
                                                  &cmisParams,
                                                  sizeof(cmisParams)));
        for (UINT32 lwrrFlag = 0; lwrrFlag < cmisParams.count; lwrrFlag++)
        {
            if (cmisParams.data[lwrrFlag] == 0)
                continue;

            ErrorFlagType errFlag = ErrorFlagType::Unknown;
            switch (lwrrFlag)
            {
                case 0: errFlag  = ErrorFlagType::LatchedDataPathStateChange; break;
                case 1: errFlag  = ErrorFlagType::TxFault;                    break;
                case 2: errFlag  = ErrorFlagType::TxLossOfSignal;             break;
                case 3: errFlag  = ErrorFlagType::TxCDRLossOfLock;            break;
                case 4: errFlag  = ErrorFlagType::TxAdaptiveInputEqFault;     break;
                case 5: errFlag  = ErrorFlagType::TxOutputPowerHighAlarm;     break;
                case 6: errFlag  = ErrorFlagType::TxOutputPowerLowAlarm;      break;
                case 7: errFlag  = ErrorFlagType::TxOutputPowerHighWarning;   break;
                case 8: errFlag  = ErrorFlagType::TxOutputPowerLowWarning;    break;
                case 9: errFlag  = ErrorFlagType::TxBiasHighAlarm;            break;
                case 10: errFlag = ErrorFlagType::TxBiasLowAlarm;             break;
                case 11: errFlag = ErrorFlagType::TxBiasHighWarning;          break;
                case 12: errFlag = ErrorFlagType::TxBiasLowWarning;           break;
                case 13: errFlag = ErrorFlagType::RxLatchedLossOfSignal;      break;
                case 14: errFlag = ErrorFlagType::RxLatchedCDRLossOfLock;     break;
                case 15: errFlag = ErrorFlagType::RxInputPowerHighAlarm;      break;
                case 16: errFlag = ErrorFlagType::RxInputPowerLowAlarm;       break;
                case 17: errFlag = ErrorFlagType::RxInputPowerHighWarning;    break;
                case 18: errFlag = ErrorFlagType::RxInputPowerLowWarning;     break;
                default: break;
            }
            for (UINT08 lwrrLane = 0; lwrrLane < CHAR_BIT; lwrrLane++)
            {
                if ((1 << lwrrLane) & cmisParams.data[lwrrFlag])
                {
                    pErrorFlags->push_back({ cciPhysId, lwrrLane, errFlag });
                }
            }
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
const LwLinkCableController::CCFirmwareInfo & LimerockLwLink::DoGetCCFirmwareInfo
(
    UINT32 cciId,
    CCFirmwareImage image
) const
{
    static CCFirmwareInfo noFwInfo;
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller firmware info\n", cciId);
        return noFwInfo;
    }

    switch (image)
    {
        case CCFirmwareImage::Factory: return m_CableControllerInfo[cciId].factoryFwInfo;
        case CCFirmwareImage::ImageA:  return m_CableControllerInfo[cciId].imageAFwInfo;
        case CCFirmwareImage::ImageB:  return m_CableControllerInfo[cciId].imageBFwInfo;
        default:
            MASSERT(!"Unknown CC firmware image type");
            break;
    }

    return noFwInfo;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetCCGradingValues(UINT32 cciId, vector<GradingValues> * pGradingValues) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller grading values\n", cciId);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc;
    INT32 linkId = Utility::BitScanForward(m_CableControllerInfo[cciId].linkMask);
    while (linkId != -1)
    {
        LWSWITCH_CCI_GET_GRADING_VALUES_PARAMS gradingParams = { };
        gradingParams.linkId = linkId;
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_CCI_GET_GRADING_VALUES,
                                                  &gradingParams,
                                                  sizeof(gradingParams)));
        GradingValues lwrGrading;
        lwrGrading.linkId   = linkId;
        lwrGrading.laneMask = gradingParams.laneMask;
        lwrGrading.rxInit.assign(gradingParams.grading.rx_init,
                                 gradingParams.grading.rx_init + LWSWITCH_CCI_XVCR_LANES);
        lwrGrading.txInit.assign(gradingParams.grading.tx_init,
                                 gradingParams.grading.tx_init + LWSWITCH_CCI_XVCR_LANES);
        lwrGrading.rxMaint.assign(gradingParams.grading.rx_maint,
                                  gradingParams.grading.rx_maint + LWSWITCH_CCI_XVCR_LANES);
        lwrGrading.txMaint.assign(gradingParams.grading.tx_maint,
                                  gradingParams.grading.tx_maint + LWSWITCH_CCI_XVCR_LANES);
        pGradingValues->push_back(lwrGrading);
        linkId = Utility::BitScanForward(m_CableControllerInfo[cciId].linkMask, linkId + 1);
    }
    return rc;
}

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetCCHostLaneCount(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller host lane count\n", cciId);
        return ILWALID_LANE_MASK;
    }

    return m_CableControllerInfo[cciId].hostLaneCount;
}

//-----------------------------------------------------------------------------
string LimerockLwLink::DoGetCCHwRevision(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller HW revision\n", cciId);
        return "Unknown";
    }

    return m_CableControllerInfo[cciId].hwRev;
}

/*mfg date starts at byte 11 and is 3 byte long*/
string LimerockLwLink::GetCarrierBoardMfgDate(vector<UINT32>& epromData)
{
    /*Fru EEPROM returns minutes from 1/1/1996 00:00*/
    time_t baseStartEpoch = CARRIER_BASE_START_EPOCH;
    time_t offsetEpochFromStart =(
                                  epromData[CARRIER_MFG_DATE_BYTE_OFFSET] << 0 |
                                  epromData[CARRIER_MFG_DATE_BYTE_OFFSET + 1] << 8 |
                                  epromData[CARRIER_MFG_DATE_BYTE_OFFSET + 2] << 16
                                 ) * 60;
    time_t totalTimeFromEpoch = baseStartEpoch + offsetEpochFromStart;
    string boardMfgDate = asctime(localtime(&totalTimeFromEpoch));
    boardMfgDate.erase(remove(boardMfgDate.begin(), boardMfgDate.end(), '\n'), boardMfgDate.end());
    return boardMfgDate;
}

string LimerockLwLink::GetCarrierBoardMfgName(vector<UINT32>& epromData)
{
    UINT08 dataLength = epromData[CARRIER_MFG_NAME_BYTE_OFFSET]
                        & CARRIER_BOARD_DATA_LENGTH_MASK;
    UINT08 dataOffset = CARRIER_MFG_NAME_BYTE_OFFSET + 1;
    string boardMfgName;
    for (int i = dataOffset; i < dataOffset + dataLength; i++)
    {
        boardMfgName += static_cast<char>(epromData[i]);
    }
    return boardMfgName;
}

string LimerockLwLink::GetCarrierBoardProductName(vector<UINT32>& epromData)
{
    UINT08 dataLength = epromData[CARRIER_BOARD_NAME_BYTE_OFFSET]
                        & CARRIER_BOARD_DATA_LENGTH_MASK;
    UINT08 dataOffset = CARRIER_BOARD_NAME_BYTE_OFFSET + 1;
    string boardProductName;
    for (int i = dataOffset; i < dataOffset + dataLength; i++)
    {
        boardProductName += static_cast<char>(epromData[i]);
    }
    return boardProductName;
}

string LimerockLwLink::GetCarrierBoardSerial(vector<UINT32>& epromData)
{
    UINT08 dataLength = epromData[CARRIER_BOARD_SERIAL_BYTE_OFFSET]
                        & CARRIER_BOARD_DATA_LENGTH_MASK;
    UINT08 dataOffset = CARRIER_BOARD_SERIAL_BYTE_OFFSET + 1;
    string boardSerial;
    for (int i = dataOffset; i < dataOffset + dataLength; i++)
    {
        boardSerial += static_cast<char>(epromData[i]);
    }
    return boardSerial;
}

string LimerockLwLink::GetCarrierBoardPartNum(vector<UINT32>& epromData)
{
    UINT08 dataLength = epromData[CARRIER_BOARD_HWREV_PN_BYTE_OFFSET]
                        & CARRIER_BOARD_DATA_LENGTH_MASK;
    UINT08 dataOffset = CARRIER_BOARD_HWREV_PN_BYTE_OFFSET + 1;
    string boardPartNum;
    /*Last 4 bytes are fro HW rev*/
    for (int i = dataOffset; i < dataOffset + dataLength - 4; i++)
    {
        boardPartNum += static_cast<char>(epromData[i]);
    }

    return boardPartNum;
}

string LimerockLwLink::GetCarrierBoardHwRev(vector<UINT32>& epromData)
{
    UINT08 dataLength = epromData[CARRIER_BOARD_HWREV_PN_BYTE_OFFSET]
                        & CARRIER_BOARD_DATA_LENGTH_MASK;
    UINT08 dataOffset = CARRIER_BOARD_HWREV_PN_BYTE_OFFSET + 1;
    string boardHWRev;
    for (int i = (dataOffset + (dataLength - 4)); i < dataOffset + dataLength; i++)
    {
        /*We don't want to report leading hyphen in rev version bytes*/
        if (epromData.at(i) == '-')
            continue;
        boardHWRev += static_cast<char>(epromData[i]);
    }

    return boardHWRev;
}

string LimerockLwLink::GetCarrierBoardFruVer(vector<UINT32>& epromData)
{
    UINT08 dataLength = epromData[CARRIER_BOARD_FRU_BYTE_OFFSET] & CARRIER_BOARD_DATA_LENGTH_MASK;
    UINT08 dataOffset = CARRIER_BOARD_FRU_BYTE_OFFSET + 1;
    string boardFruVer;
    for (int i = dataOffset; i < dataOffset + dataLength; i++)
    {
        boardFruVer += static_cast<char>(epromData[i]);
    }
    return boardFruVer;
}

RC LimerockLwLink::GetPerPortCarrierFruEepromData(TestDevice* pDev, UINT08 portNum,
                                                    UINT32 portAddr, CarrierFruInfo* carrierFruInfo)
{
    RC rc;
    vector<UINT32> eepromData;
    I2c::Dev i2cDev = pDev->GetInterface<I2c>()->I2cDev(portNum, portAddr);
    rc = i2cDev.ReadArray(0, 256, &eepromData);
    carrierFruInfo->carrierBoardMfgDate     = GetCarrierBoardMfgDate(eepromData);
    carrierFruInfo->carrierBoardMfgName     = GetCarrierBoardMfgName(eepromData);
    carrierFruInfo->carrierBoardProductName = GetCarrierBoardProductName(eepromData);
    carrierFruInfo->carrierBoardSerialNum   = GetCarrierBoardSerial(eepromData);
    carrierFruInfo->carrierBoardFruRev      = GetCarrierBoardFruVer(eepromData);
    carrierFruInfo->carrierBoardPartNum     = GetCarrierBoardPartNum(eepromData);
    carrierFruInfo->carrierBoardHwRev       = GetCarrierBoardHwRev(eepromData);
    return rc;
}
//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetCarrierFruEepromData(CarrierFruInfos* carrierFruInfos)
{
    RC rc;
    TestDevice* pDev = GetDevice();
    I2c::I2cDcbDevInfo i2cDcbDevInfo;
    CHECK_RC(pDev->GetInterface<I2c>()->GetI2cDcbDevInfo(&i2cDcbDevInfo));
    for (UINT32 dev = 0; dev < i2cDcbDevInfo.size(); ++dev)
    {
        if (i2cDcbDevInfo[dev].Type == LWSWITCH_I2C_DEVICE_CCI_PARTITION_0_IPMI_FRU ||
            i2cDcbDevInfo[dev].Type == LWSWITCH_I2C_DEVICE_CCI_PARTITION_1_IPMI_FRU)
        {
            CarrierFruInfo carrierFruInfoPort;
            carrierFruInfoPort.carrierPortNum = to_string(i2cDcbDevInfo[dev].I2CLogicalPort);
            rc = GetPerPortCarrierFruEepromData(pDev, i2cDcbDevInfo[dev].I2CLogicalPort,
                                                    i2cDcbDevInfo[dev].I2CAddress,
                                                    &carrierFruInfoPort);
            if(rc == RC::LWRM_ERROR || rc == RC::GENERIC_I2C_ERROR)
            {
                Printf(Tee::PriError, "%s : Failed to read on carrier fru info port %u\n",
                            GetDevice()->GetName().c_str(), i2cDcbDevInfo[dev].I2CLogicalPort);
                SCOPED_DEV_INST(GetDevice());
                Mle::Print(Mle::Entry::i2c_read_error)
                    .dev_type(Mle::I2cReadError::I2cDevType::optical_carrier)
                    .optical_device_id(i2cDcbDevInfo[dev].I2CLogicalPort);
                return RC::I2C_DEVICE_NOT_FOUND;
            }
            carrierFruInfos->push_back(carrierFruInfoPort);

        }
    }
    return rc;

}

//------------------------------------------------------------------------------
LwLinkCableController::CCInitStatus LimerockLwLink::DoGetCCInitStatus(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller host init status\n", cciId);
        return LwLinkCableController::CCInitStatus::Unknown;
    }

    return m_CableControllerInfo[cciId].initStatus;
}

//------------------------------------------------------------------------------
UINT64 LimerockLwLink::DoGetCCLinkMask(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller link mask\n", cciId);
        return 0;
    }

    return m_CableControllerInfo[cciId].linkMask;
}

//------------------------------------------------------------------------------
LwLinkCableController::CCModuleIdentifier LimerockLwLink::DoGetCCModuleIdentifier(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller module id\n", cciId);
        return LwLinkCableController::CCModuleIdentifier::Unknown;
    }

    return m_CableControllerInfo[cciId].moduleIdentifier;
}

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetCCModuleLaneCount(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller module lane count\n", cciId);
        return ILWALID_LANE_MASK;
    }

    return m_CableControllerInfo[cciId].moduleLaneCount;
}

//-----------------------------------------------------------------------------
LwLinkCableController::CCModuleMediaType LimerockLwLink::DoGetCCModuleMediaType(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller module media type\n", cciId);
        return CCModuleMediaType::Unknown;
    }

    return m_CableControllerInfo[cciId].moduleMediaType;
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoGetCCModuleState(UINT32 cciId, LwLinkCableController::ModuleState * pStateValue) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller state values\n", cciId);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc;
    LWSWITCH_CCI_GET_MODULE_STATE stateParams = { };
    const UINT32 linkId =
        static_cast<UINT32>(Utility::BitScanForward(m_CableControllerInfo[cciId].linkMask));
    stateParams.linkId = linkId;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_LWSWITCH_CCI_GET_MODULE_STATE,
                                              &stateParams,
                                              sizeof(stateParams)));

    if (stateParams.info.bLReserved == LW_TRUE)
        *pStateValue = ModuleState::Reserved;
    else if (stateParams.info.bLModuleLowPwrState == LW_TRUE)
        *pStateValue = ModuleState::LowPwr;
    else if (stateParams.info.bLModulePwrUpState == LW_TRUE)
        *pStateValue = ModuleState::PwrUp;
    else if (stateParams.info.bLModuleReadyState == LW_TRUE)
        *pStateValue = ModuleState::Ready;
    else if (stateParams.info.bLModulePwrDnState == LW_TRUE)
        *pStateValue = ModuleState::PwrDn;
    else if (stateParams.info.bLFaultState == LW_TRUE)
        *pStateValue = ModuleState::Fault;

    return rc;
}

//-----------------------------------------------------------------------------
string LimerockLwLink::DoGetCCPartNumber(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller part number\n", cciId);
        return "Unknown";
    }

    return m_CableControllerInfo[cciId].partNumber;
}

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetCCPhysicalId(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller physical id\n", cciId);
        return ~0U;
    }

    return m_CableControllerInfo[cciId].physicalId;
}

//-----------------------------------------------------------------------------
string LimerockLwLink::DoGetCCSerialNumber(UINT32 cciId) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller serial number\n", cciId);
        return "Unknown";
    }

    return m_CableControllerInfo[cciId].serialNumber;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetCCVoltage(UINT32 cciId, UINT16 * pVoltageMv) const
{
    if (cciId >= static_cast<UINT32>(m_CableControllerInfo.size()))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Invalid cciId %u when getting cable controller voltage\n", cciId);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    RC rc;
    LWSWITCH_CCI_GET_VOLTAGE voltageParams = { };
    const UINT32 linkId =
        static_cast<UINT32>(Utility::BitScanForward(m_CableControllerInfo[cciId].linkMask));
    voltageParams.linkId = linkId;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_LWSWITCH_CCI_GET_VOLTAGE,
                                              &voltageParams,
                                              sizeof(voltageParams)));

    *pVoltageMv = voltageParams.voltage.voltage_mV;

    return rc;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::InitializeCableController()
{
    RC rc;

    // This function can be called more than once if some links fail to train.  None of
    // the data initialized here depends on link training though so return if it has already
    // been called once
    if (!m_CableControllerInfo.empty())
    {
        return RC::OK;
    }
    LWSWITCH_CCI_CMIS_PRESENCE_PARAMS presenceParams = { };
    // The switch driver incorrectly returns not supported when no modules are present
    // or when CCI is not enabled instead of simply returning 0 for the masks.
    // Bug 3353324 has been filed to address this
    rc = GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                          LibInterface::CONTROL_LWSWITCH_CCI_CMIS_PRESENCE,
                                          &presenceParams,
                                          sizeof(presenceParams));
    if (rc == RC::LWRM_NOT_SUPPORTED)
        return RC::OK;

    if (presenceParams.cagesMask != presenceParams.modulesMask)
    {
        Printf(Tee::PriError,
               "%s : Missing cable controllers, expected CC mask 0x%x, actual CC mask 0x%x\n",
               GetDevice()->GetName().c_str(), presenceParams.cagesMask, presenceParams.modulesMask);
        if (!TopologyManager::GetIgnoreMissingCC())
            return RC::LWLINK_DEVICE_ERROR;
    }

    INT32 lwrCage = Utility::BitScanForward(presenceParams.modulesMask);
    for ( ; lwrCage != -1; lwrCage = Utility::BitScanForward(presenceParams.modulesMask, lwrCage + 1))
    {
        CableControllerInfo ccInfo;
        if (!((1 << lwrCage) & presenceParams.modulesMask))
        {
            ccInfo.physicalId = lwrCage;
            ccInfo.initStatus = CCInitStatus::NotFound;
            continue;
        }

        LWSWITCH_CCI_CMIS_LWLINK_MAPPING_PARAMS linkParams = { };
        linkParams.cageIndex = lwrCage;
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_CCI_CMIS_LWLINK_MAPPING,
                                                  &linkParams,
                                                  sizeof(linkParams)));

        INT32 lwrLink = Utility::BitScanForward(linkParams.linkMask);
        if (lwrLink == -1)
        {
            Printf(Tee::PriError,
                   "%s : Cable controller %u not connected to any links\n",
                   GetDevice()->GetName().c_str(), lwrCage);
            return RC::HW_STATUS_ERROR;
        }

        string ilwalidLinkStr;
        for ( ; lwrLink != -1; lwrLink = Utility::BitScanForward(linkParams.linkMask, lwrLink + 1))
        {
            if (!IsLinkValid(lwrLink))
            {
                ilwalidLinkStr += Utility::StrPrintf("%s%d",
                                                     ilwalidLinkStr.empty() ? "" : ", ", lwrLink);
            }
        }
        if (!ilwalidLinkStr.empty())
        {
            Printf(Tee::PriError,
                   "%s : Cable controller %u connected to invalid link(s) %s\n",
                   GetDevice()->GetName().c_str(), lwrCage, ilwalidLinkStr.c_str());
            return RC::HW_STATUS_ERROR;
        }

        LWSWITCH_CCI_GET_CAPABILITIES_PARAMS cciCaps = { };
        cciCaps.linkId = Utility::BitScanForward(linkParams.linkMask);
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_CCI_GET_CAPABILITIES,
                                                  &cciCaps,
                                                  sizeof(cciCaps)));
        ccInfo.initStatus      = CCInitStatus::Success;
        ccInfo.linkMask        = linkParams.linkMask;
        ccInfo.physicalId      = lwrCage;
        ccInfo.hostLaneCount   = cciCaps.capabilities.host_lane_count;
        ccInfo.moduleLaneCount = cciCaps.capabilities.module_lane_count;

        switch (cciCaps.capabilities.module_interface_id)
        {
            // TODO : Switch driver will eventually expose defines that MODS can use
            // instead of hard coding numbers
            case 0: ccInfo.moduleMediaType = CCModuleMediaType::Undefined;     break;
            case 1: ccInfo.moduleMediaType = CCModuleMediaType::OpticalMmf;    break;
            case 2: ccInfo.moduleMediaType = CCModuleMediaType::OpticalSmf;    break;
            case 3: ccInfo.moduleMediaType = CCModuleMediaType::PassiveCopper; break;
            case 4: ccInfo.moduleMediaType = CCModuleMediaType::ActiveCables;  break;
            case 5: ccInfo.moduleMediaType = CCModuleMediaType::BaseT;         break;
            default:
                if ((cciCaps.capabilities.module_interface_id >= 0x40) &&
                    (cciCaps.capabilities.module_interface_id <= 0x8F))
                {
                    ccInfo.moduleMediaType = CCModuleMediaType::Custom;
                }
                else
                {
                    ccInfo.moduleMediaType = CCModuleMediaType::Reserved;
                }
                break;
        }

        switch (cciCaps.capabilities.identifier)
        {
            // TODO : Switch driver will eventually expose defines that MODS can use
            // instead of hard coding numbers
            case  0: ccInfo.moduleIdentifier = CCModuleIdentifier::Unknown;          break;
            case  1: ccInfo.moduleIdentifier = CCModuleIdentifier::GBIC;             break;
            case  2: ccInfo.moduleIdentifier = CCModuleIdentifier::Soldered;         break;
            case  3: ccInfo.moduleIdentifier = CCModuleIdentifier::SFP;              break;
            case  4: ccInfo.moduleIdentifier = CCModuleIdentifier::XBI;              break;
            case  5: ccInfo.moduleIdentifier = CCModuleIdentifier::XENPAK;           break;
            case  6: ccInfo.moduleIdentifier = CCModuleIdentifier::XFP;              break;
            case  7: ccInfo.moduleIdentifier = CCModuleIdentifier::XFF;              break;
            case  8: ccInfo.moduleIdentifier = CCModuleIdentifier::XFP_E;            break;
            case  9: ccInfo.moduleIdentifier = CCModuleIdentifier::XPAK;             break;
            case 10: ccInfo.moduleIdentifier = CCModuleIdentifier::X2;               break;
            case 11: ccInfo.moduleIdentifier = CCModuleIdentifier::DWDM_SFP;         break;
            case 12: ccInfo.moduleIdentifier = CCModuleIdentifier::QSFP;             break;
            case 13: ccInfo.moduleIdentifier = CCModuleIdentifier::QSFPPlus;         break;
            case 14: ccInfo.moduleIdentifier = CCModuleIdentifier::CXP;              break;
            case 15: ccInfo.moduleIdentifier = CCModuleIdentifier::SMMHD4X;          break;
            case 16: ccInfo.moduleIdentifier = CCModuleIdentifier::SMMHD8X;          break;
            case 17: ccInfo.moduleIdentifier = CCModuleIdentifier::QSFP28;           break;
            case 18: ccInfo.moduleIdentifier = CCModuleIdentifier::CXP2;             break;
            case 19: ccInfo.moduleIdentifier = CCModuleIdentifier::CDFP_1_2;         break;
            case 20: ccInfo.moduleIdentifier = CCModuleIdentifier::SMMHD4XFanout;    break;
            case 21: ccInfo.moduleIdentifier = CCModuleIdentifier::SMMHD8XFanout;    break;
            case 22: ccInfo.moduleIdentifier = CCModuleIdentifier::CDFP_3;           break;
            case 23: ccInfo.moduleIdentifier = CCModuleIdentifier::MicroQSFP;        break;
            case 24: ccInfo.moduleIdentifier = CCModuleIdentifier::QSFP_DD;          break;
            case 25: ccInfo.moduleIdentifier = CCModuleIdentifier::OSFP8X;           break;
            case 26: ccInfo.moduleIdentifier = CCModuleIdentifier::SFP_DD;           break;
            case 27: ccInfo.moduleIdentifier = CCModuleIdentifier::DFSP;             break;
            case 28: ccInfo.moduleIdentifier = CCModuleIdentifier::MiniLinkX4;       break;
            case 29: ccInfo.moduleIdentifier = CCModuleIdentifier::MiniLinkX8;       break;
            case 30: ccInfo.moduleIdentifier = CCModuleIdentifier::QSFPPlusWithCMIS; break;
            default:
                if (cciCaps.capabilities.identifier < 0x80)
                {
                    ccInfo.moduleIdentifier = CCModuleIdentifier::Reserved;
                }
                else
                {
                    ccInfo.moduleIdentifier = CCModuleIdentifier::VendorSpecific;
                }
                break;
        }

        LWSWITCH_CCI_GET_FW_REVISION_PARAMS fwParams = {};
        fwParams.linkId = Utility::BitScanForward(linkParams.linkMask);
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_CCI_GET_FW_REVISIONS,
                                                  &fwParams,
                                                  sizeof(fwParams)));

        auto SaveFwVersionInfo = [] (const LWSWITCH_CCI_GET_FW_REVISIONS &fwInfo, CCFirmwareInfo *pFwInfo)
        {
            pFwInfo->majorVersion = fwInfo.major;
            pFwInfo->minorVersion = fwInfo.minor;
            pFwInfo->build        = fwInfo.build;

            pFwInfo->status = CCFirmwareStatus::NotPresent;
            if (FLD_TEST_DRF(SWITCH, _CCI_FW_FLAGS, _ACTIVE, _YES, fwInfo.flags))
            {
                pFwInfo->status = CCFirmwareStatus::Running;
            }
            else if (FLD_TEST_DRF(SWITCH, _CCI_FW_FLAGS, _PRESENT, _YES, fwInfo.flags))
            {
                pFwInfo->status = CCFirmwareStatus::Present;
            }
            if (FLD_TEST_DRF(SWITCH, _CCI_FW_FLAGS, _EMPTY, _YES, fwInfo.flags))
            {
                pFwInfo->status = CCFirmwareStatus::Empty;
            }
            if (FLD_TEST_DRF(SWITCH, _CCI_FW_FLAGS, _COMMITED, _YES, fwInfo.flags))
            {
                pFwInfo->bIsBoot = true;
            }
        };

        SaveFwVersionInfo(fwParams.revisions[LWSWITCH_CCI_FW_IMAGE_FACTORY], &ccInfo.factoryFwInfo);
        SaveFwVersionInfo(fwParams.revisions[LWSWITCH_CCI_FW_IMAGE_A],       &ccInfo.imageAFwInfo);
        SaveFwVersionInfo(fwParams.revisions[LWSWITCH_CCI_FW_IMAGE_B],       &ccInfo.imageBFwInfo);

        auto StoreCmisString = [this] (UINT32 cciId, UINT32 address, UINT32 numBytes, const char * dataType, string * pCmisString) -> RC
        {
            RC rc;
            LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ_PARAMS cmisParams = {};
            cmisParams.cageIndex = cciId;
            cmisParams.bank      = 0;
            cmisParams.page      = 0;
            cmisParams.address   = address;
            cmisParams.count     = numBytes;
            rc = GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                  LibInterface::CONTROL_LWSWITCH_CCI_CMIS_MEMORY_ACCESS_READ,
                                                  &cmisParams,
                                                  sizeof(cmisParams));
            if (rc != RC::OK)
            {
                Printf(Tee::PriError, "%s : Failed to read %s on cable controller %u\n",
                       GetDevice()->GetName().c_str(), dataType, cciId);
                SCOPED_DEV_INST(GetDevice());
                Mle::Print(Mle::Entry::i2c_read_error)
                    .dev_type(Mle::I2cReadError::I2cDevType::osfp)
                    .optical_device_id(cciId);
                if (rc == RC::LWRM_ERROR || rc == RC::GENERIC_I2C_ERROR)
                {
                    return RC::I2C_DEVICE_NOT_FOUND;
                }
                return rc;
            }
            for (UINT08 lwrByte = 0; lwrByte < numBytes; lwrByte++)
            {
                char c = '.';
                if ((cmisParams.data[lwrByte] > 31) && (cmisParams.data[lwrByte] < 128))
                    c = static_cast<char>(cmisParams.data[lwrByte]);
                pCmisString->append(1, c);
            }
            return RC::OK;
        };

        CHECK_RC(StoreCmisString(lwrCage,
                                 CMIS_SERIAL_NUMBER_OFFSET,
                                 CMIS_SERIAL_NUMBER_SIZE,
                                 "serial number",
                                 &ccInfo.serialNumber));
        CHECK_RC(StoreCmisString(lwrCage,
                                 CMIS_PART_NUMBER_OFFSET,
                                 CMIS_PART_NUMBER_SIZE,
                                 "part number",
                                 &ccInfo.partNumber));
        CHECK_RC(StoreCmisString(lwrCage,
                                 CMIS_HW_REVISION_OFFSET,
                                 CMIS_HW_REVISION_SIZE,
                                 "HW revision",
                                 &ccInfo.hwRev));

        m_CableControllerInfo.push_back(ccInfo);
    }
    CarrierFruInfos carrierFruInfos;
    CHECK_RC(GetCarrierFruEepromData(&carrierFruInfos));
    m_CarrierInfo = carrierFruInfos;

    LWSWITCH_GET_LWLINK_CAPS_PARAMS capsParams = {};
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_GET_LINK_CAPS,
                                              &capsParams,
                                              sizeof(capsParams)));
    m_CableControllerBiosLinkMask = capsParams.activeRepeaterMask;

    return rc;
}

//------------------------------------------------------------------------------
const LimerockLwLink::CableControllerInfo * LimerockLwLink::GetCableControllerInfo(UINT32 linkId) const
{
    if (!IsLinkValid(linkId))
        return nullptr;

    auto pCcInfo =  find_if(m_CableControllerInfo.begin(), m_CableControllerInfo.end(),
                            [linkId] (const CableControllerInfo & cci) -> bool
                            {
                                return (cci.linkMask & (1ULL << linkId)) != 0;
                            });
    if (pCcInfo == m_CableControllerInfo.end())
        return nullptr;
    return &*pCcInfo;
}

//------------------------------------------------------------------------------
LwLink::SystemLineRate LimerockLwLink::DoGetSystemLineRate(UINT32 linkId) const
{
    const RegHal& regs = Regs();
    const UINT32 systemLineRate =
        regs.Read32(linkId, MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE);
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_16_00000_GBPS))
        return SystemLineRate::GBPS_16_0;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_20_00000_GBPS))
        return SystemLineRate::GBPS_20_0;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_25_00000_GBPS))
        return SystemLineRate::GBPS_25_0;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_25_78125_GBPS))
        return SystemLineRate::GBPS_25_7;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_28_12500_GBPS))
        return SystemLineRate::GBPS_28_1;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_32_00000_GBPS))
        return SystemLineRate::GBPS_32_0;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_40_00000_GBPS))
        return SystemLineRate::GBPS_40_0;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_50_00000_GBPS))
        return SystemLineRate::GBPS_50_0;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_53_12500_GBPS))
        return SystemLineRate::GBPS_53_1;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_100_00000_GBPS))
        return SystemLineRate::GBPS_100_0;
    if (systemLineRate == regs.LookupValue(MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_106_25000_GBPS))
        return SystemLineRate::GBPS_106_2;
    MASSERT(!"Unknown system line rate register value");
    return SystemLineRate::GBPS_UNKNOWN;
}

//------------------------------------------------------------------------------
bool LimerockLwLink::DoHasCollapsedResponses() const
{
    const RegHal& regs = Regs();

    bool has = true;
    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        // Disable_disabled means enabled
        has = has && regs.Test32(linkId, MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_DISABLED);
    }

    return has;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LimerockLwLink::DoInitialize
(
    const vector<LibInterface::LibDeviceHandle> &handles
)
{
    RC rc;
    RegHal& regs = Regs();

    CHECK_RC(LwSwitchLwLink::DoInitialize(handles));

    if (GetDevice()->HasBug(2597550))
    {
        // NOTE: Only write to valid links, but it also needs to be written before
        // link discovery, so we can't just rely on IsLinkActive or IsLinkValid.
        // CTRL_LWSWITCH_GET_LWLINK_CAPS is a lightweight RM control call that
        // includes a valid link enable mask at this point.
        LWSWITCH_GET_LWLINK_CAPS_PARAMS linkCapsParams = {};
        CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                                LibInterface::CONTROL_GET_LINK_CAPS,
                                                &linkCapsParams,
                                                sizeof(linkCapsParams)));
        for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
        {
            if (!(linkCapsParams.enabledLinkMask & (1ULL << linkId)))
            {
                // Link is not enabled, skip
                continue;
            }

            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_ERR_ECC_CTRL_NCISOC_PARITY_ENABLE_DISABLE);
            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_ERR_LOG_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_ERR_FATAL_REPORT_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_ERR_NON_FATAL_REPORT_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_ERR_CORRECTABLE_REPORT_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_ERR_CONTAIN_EN_0_NCISOC_PARITY_ERR_DISABLE);

            MASSERT(regs.Test32(linkId, MODS_INGRESS_ERR_STATUS_0_NCISOC_PARITY_ERR_NONE));

            regs.Write32(linkId, MODS_EGRESS_ERR_ECC_CTRL_NCISOC_PARITY_ENABLE_DISABLE);
            regs.Write32(linkId, MODS_EGRESS_ERR_LOG_EN_0_NCISOC_CREDIT_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_EGRESS_ERR_FATAL_REPORT_EN_0_NCISOC_CREDIT_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_EGRESS_ERR_NON_FATAL_REPORT_EN_0_NCISOC_CREDIT_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_EGRESS_ERR_CORRECTABLE_REPORT_EN_0_NCISOC_CREDIT_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_EGRESS_ERR_CONTAIN_EN_0_NCISOC_CREDIT_PARITY_ERR_DISABLE);

            regs.Write32(linkId, MODS_INGRESS_ERR_ECC_CTRL_NCISOC_PARITY_ENABLE_DISABLE);
            regs.Write32(linkId, MODS_INGRESS_ERR_LOG_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_INGRESS_ERR_FATAL_REPORT_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_INGRESS_ERR_NON_FATAL_REPORT_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_INGRESS_ERR_CORRECTABLE_REPORT_EN_0_NCISOC_PARITY_ERR_DISABLE);
            regs.Write32(linkId, MODS_INGRESS_ERR_CONTAIN_EN_0_NCISOC_PARITY_ERR_DISABLE);
        }
    }

    JavaScriptPtr pJs;
    JsArray args(1);
    JsTestDevice* pJsTestDevice = new JsTestDevice();
    TestDevicePtr pDev(GetDevice(), [](auto p){});
    pJsTestDevice->SetTestDevice(pDev);
    JSContext* pContext = nullptr;
    CHECK_RC(pJs->GetContext(&pContext));
    CHECK_RC(pJsTestDevice->CreateJSObject(pContext, pJs->GetGlobalObject()));
    CHECK_RC(pJs->ToJsval(pJsTestDevice->GetJSObject(), &args[0]));
    CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "SetLwLinkSystemParameters", args));

    return OK;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoInitializePostDiscovery()
{
    RC rc;
    CHECK_RC(LwSwitchLwLink::DoInitializePostDiscovery());
    CHECK_RC(InitializeCableController());
    return rc;
}

//------------------------------------------------------------------------------
bool LimerockLwLink::DoIsLinkAcCoupled(UINT32 linkId) const
{
    if (!IsLinkActive(linkId))
        return false;
    return Regs().Test32(linkId, MODS_LWLDL_TOP_LINK_CONFIG_AC_SAFE_EN_ON);
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoResetTopology()
{
    StickyRC rc;

    LWSWITCH_RESET_AND_DRAIN_LINKS_PARAMS resetParams = {};
    resetParams.linkMask = GetMaxLinkMask();
    rc = GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                          LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_RESET_AND_DRAIN_LINKS,
                                          &resetParams,
                                          sizeof(resetParams));

    rc = LwSwitchLwLink::DoResetTopology();

    return rc;
}

//-----------------------------------------------------------------------------
bool LimerockLwLink::DoIsLinkPam4(UINT32 linkId) const
{
    return Regs().Test32(linkId, MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LINE_CODE_MODE_PAM4);
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoIsPerLaneEccEnabled(UINT32 linkId, bool *pbPerLaneEccEnabled) const
{
    MASSERT(pbPerLaneEccEnabled);
    *pbPerLaneEccEnabled = false;

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    *pbPerLaneEccEnabled =
        !Regs().Test32(linkId, MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_BLOCK_CODE_MODE_OFF);

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Get whether per lane counts are enabled
//!
//! \param linkId           : Link ID to get per-lane error count enable state
//! \param pbPerLaneEnabled : Return status of per lane enabled
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LimerockLwLink::DoPerLaneErrorCountsEnabled
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

    *pbPerLaneEnabled = Regs().Test32(linkId, MODS_LWLDL_TX_TXLANECRC_ENABLE_EN);

    return OK;
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoSetCollapsedResponses(UINT32 mask)
{
    RC rc;
    RegHal& regs = Regs();

    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (!IsLinkActive(linkId))
            continue;

        switch (mask)
        {
            case 0x0:
                // Both tx/rx disabled
                regs.Write32(linkId, MODS_LWLTLC_RX_LNK_CTRL_LINK_CONFIG_ALLOW_COLLAPSED_RESPONSES_DISABLED);
                regs.Write32(linkId, MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_ENABLED);
                break;
            case 0x1:
                // Disabled on Tx
                regs.Write32(linkId, MODS_LWLTLC_RX_LNK_CTRL_LINK_CONFIG_ALLOW_COLLAPSED_RESPONSES_ENABLED);
                regs.Write32(linkId, MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_ENABLED);
                break;
            case 0x2:
                // Disabled on Rx
                regs.Write32(linkId, MODS_LWLTLC_RX_LNK_CTRL_LINK_CONFIG_ALLOW_COLLAPSED_RESPONSES_DISABLED);
                regs.Write32(linkId, MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_DISABLED);
                break;
            case 0x3:
                // Both tx/rx enabled
                regs.Write32(linkId, MODS_LWLTLC_RX_LNK_CTRL_LINK_CONFIG_ALLOW_COLLAPSED_RESPONSES_ENABLED);
                regs.Write32(linkId, MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_DISABLED);
                break;
            default:
                Printf(Tee::PriError, "Invalid Collapsed Responses Parameter = 0x%x\n", mask);
                return RC::ILWALID_ARGUMENT;
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoSetCompressedResponses(bool bEnable)
{
    RegHal& regs = Regs();

    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (IsLinkActive(linkId))
        {
            if (bEnable)
            {
                // Disable Disabled means enabled
                regs.Write32(linkId, MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COMPRESSED_RESPONSE_DISABLE_DISABLED);
            }
            else
            {
                regs.Write32(linkId, MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COMPRESSED_RESPONSE_DISABLE_ENABLED);
            }
        }
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
RC LimerockLwLink::DoSetDbiEnable(bool bEnable)
{
    RegHal& regs = Regs();

    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (!IsLinkActive(linkId))
            continue;

        if (bEnable)
        {
            regs.Write32(linkId, MODS_NPORT_CTRL_ENROUTEDBI_ENABLE);
            regs.Write32(linkId, MODS_NPORT_CTRL_ENEGRESSDBI_ENABLE);
            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_CTRL_LINK_CONFIG_DBI_ENABLE_ENABLED);
        }
        else
        {
            regs.Write32(linkId, MODS_NPORT_CTRL_ENROUTEDBI_DISABLE);
            regs.Write32(linkId, MODS_NPORT_CTRL_ENEGRESSDBI_DISABLE);
            regs.Write32(linkId, MODS_LWLTLC_RX_SYS_CTRL_LINK_CONFIG_DBI_ENABLE_DISABLED);
        }
    }

    const UINT32 linksPerIoctl = GetMaxLinks() / GetMaxLinkGroups();
    for (UINT32 ioctl = 0; ioctl < GetMaxLinkGroups(); ioctl++)
    {
        for (UINT32 link = 0; link < linksPerIoctl; link++)
        {
            if (regs.IsSupported(MODS_NXBAR_TC_TILEOUTx_CTRL2_DBI_MODE, ioctl))
            {
                if (bEnable)
                {
                    regs.Write32(link, MODS_NXBAR_TC_TILEOUTx_CTRL2_DBI_MODE_INIT, ioctl);
                }
                else
                {
                    regs.Write32(link, MODS_NXBAR_TC_TILEOUTx_CTRL2_DBI_MODE_BYPASS, ioctl);
                }
            }
        }
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoWaitForIdle(FLOAT64 timeoutMs)
{
    RC rc;
    RegHal& regs = Regs();

    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        if (!IsLinkActive(linkId))
            continue;

        const UINT32 domainIdx = linkId / 4; // There are 4 links per domain
        const UINT32 regIdx = linkId % 4;

        ModsGpuRegValue regVal = MODS_NPG_STATUS_NPORT0_IDLE_INIT;
        switch (regIdx)
        {
            case 0: regVal = MODS_NPG_STATUS_NPORT0_IDLE_INIT; break;
            case 1: regVal = MODS_NPG_STATUS_NPORT1_IDLE_INIT; break;
            case 2: regVal = MODS_NPG_STATUS_NPORT2_IDLE_INIT; break;
            case 3: regVal = MODS_NPG_STATUS_NPORT3_IDLE_INIT; break;
            default:
                Printf(Tee::PriError, "Invalid register index\n");
                return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(Tasker::PollHw(timeoutMs, [&]() -> bool
        {
            return regs.Test32(domainIdx, regVal);
        }, __FUNCTION__));
    }

    return RC::OK;
}

#define LW_DRF_DATA_LEN  2:0
#define LW_DRF_DATA_LEN2 3:3
#define LW_DRF_EXT_CS    15:15
#define NCISOC_CMD_READ  0x6
#define NCISOC_CMD_WRITE 0x16

//-----------------------------------------------------------------------------
RC LimerockLwLink::GetTrexDataLenEncode(UINT32 dataLen, UINT32* pCode) const
{
    MASSERT(pCode);
    switch (dataLen)
    {
        case 16 : *pCode = 0x1; return RC::OK;
        case 32 : *pCode = 0x2; return RC::OK;
        case 128: *pCode = 0x4; return RC::OK;
        case 64 : *pCode = 0x6; return RC::OK;
        case 96 : *pCode = 0x7; return RC::OK;
        case 256: *pCode = 0x5; return RC::OK;
        case 1  : *pCode = 0x8; return RC::OK;
        case 2  : *pCode = 0x9; return RC::OK;
        case 4  : *pCode = 0xA; return RC::OK;
        case 8  : *pCode = 0xB; return RC::OK;
        default:
            Printf(Tee::PriError, "Invalid Trex DataLen: %d\n", dataLen);
            return RC::ILWALID_ARGUMENT;
    }
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoCreateRamEntry
(
    LwLinkTrex::RamEntryType type,
    LwLinkTrex::ReductionOp op,
    UINT32 route,
    UINT32 dataLen,
    RamEntry* pEntry
)
{
    MASSERT(pEntry);
    RC rc;
    *pEntry = {};
    RegHal& regs = Regs();

    // The data length is not stored as-is, but rather encoded
    UINT32 dataLenCode = 0;
    CHECK_RC(GetTrexDataLenEncode(dataLen, &dataLenCode));

    // Copy bits [2:0] from dataLen into CS[2:0]
    UINT08 cs = DRF_VAL(_DRF, _DATA, _LEN, dataLenCode);

    // Copy bit[3] from dataLen into extcs[15]
    UINT16 extcs = DRF_NUM(_DRF, _EXT, _CS, DRF_VAL(_DRF, _DATA, _LEN2, dataLenCode));
    extcs |= (1 << 14);
    extcs &= ~(1 << 13);

    UINT08 cmd = 0;
    switch (type)
    {
        case LwLinkTrex::ET_READ:
            cmd = NCISOC_CMD_READ;
            break;
        case LwLinkTrex::ET_WRITE:
            cmd = NCISOC_CMD_WRITE;
            break;
        case LwLinkTrex::ET_MC_READ:
        case LwLinkTrex::ET_MC_WRITE:
            Printf(Tee::PriError, "Trex Multicast not supported\n");
            return RC::ILWALID_ARGUMENT;
        default:
            Printf(Tee::PriError, "Unknown RamEntry Type: %d\n", static_cast<UINT32>(type));
            return RC::ILWALID_ARGUMENT;
    }

    MASSERT(route < regs.LookupFieldValueMask(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_REQ_ADDR_ROUTE));

    pEntry->data0 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_REPEAT_COUNT,       0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_NCISOC_VC,          0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_NCISOC_REQ_CMD,   cmd) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_NCISOC_REQ_CS,     cs) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_NCISOC_REQATTR_LSB, 0);

    pEntry->data1 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA1_NCISOC_REQATTR_MSB, 0);

    pEntry->data2 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA2_NCISOC_REQ_EXTCS, extcs) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA2_NCISOC_REQCONTEXT,    0);

    pEntry->data3 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_PKTDEBUG,           0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_REQLAN,             0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_REQ_ADDR_MSB,       0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_REQ_ADDR_ROUTE, route);

    pEntry->data4 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_NCISOC_REQID, 0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_NCISOC_TGTID, 0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_SLEEP,        0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_DBI_EN,       0);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoGetPacketCounts(UINT32 linkId, UINT64* pReqCount, UINT64* pRspCount)
{
    MASSERT(pReqCount);
    MASSERT(pRspCount);
    RC rc;
    RegHal& regs = Regs();

    const UINT32 requestVc = 0;
    const UINT32 responseVc = 5;

    UINT32 data = regs.SetField(MODS_NPORT_PORTSTAT_SNAP_CONTROL_STARTCOUNTER_DISABLE) |
                  regs.SetField(MODS_NPORT_PORTSTAT_SNAP_CONTROL_SNAPONDEMAND_ENABLE);
    regs.Write32(linkId, MODS_NPORT_PORTSTAT_SNAP_CONTROL, data);

    *pReqCount = (static_cast<UINT64>(regs.Read32(linkId, MODS_NPORT_PORTSTAT_PACKET_COUNT_x_1_PACKETCOUNT, requestVc)) << 32)
                 | static_cast<UINT64>(regs.Read32(linkId, MODS_NPORT_PORTSTAT_PACKET_COUNT_x_0_PACKETCOUNT, requestVc));
    *pRspCount = (static_cast<UINT64>(regs.Read32(linkId, MODS_NPORT_PORTSTAT_PACKET_COUNT_x_1_PACKETCOUNT, responseVc)) << 32)
                 | static_cast<UINT64>(regs.Read32(linkId, MODS_NPORT_PORTSTAT_PACKET_COUNT_x_0_PACKETCOUNT, responseVc));

    data = regs.SetField(MODS_NPORT_PORTSTAT_SNAP_CONTROL_STARTCOUNTER_ENABLE) |
           regs.SetField(MODS_NPORT_PORTSTAT_SNAP_CONTROL_SNAPONDEMAND_DISABLE);
    regs.Write32(linkId, MODS_NPORT_PORTSTAT_SNAP_CONTROL, data);

    return rc;
}

//-----------------------------------------------------------------------------
UINT32 LimerockLwLink::DoGetRamDepth() const
{
    return Regs().LookupValue(MODS_TREX_DYNAMIC_MODE_REQ_RAM_ADDRESS_DYNAMIC_MODE_REQ_RAM_DEPTH);
}

//-----------------------------------------------------------------------------
bool LimerockLwLink::DoIsTrexDone(UINT32 linkId)
{
    RegHal& regs = Regs();

    if (!regs.Test32(linkId, MODS_TREX_HW_CTRL_ENABLE_ENABLE_HW_READ))
        return false;

    return true;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoSetRamWeight(UINT32 linkId, LwLinkTrex::TrexRam ram, UINT32 weight)
{
    RegHal& regs = Regs();

    switch (ram)
    {
        case LwLinkTrex::TR_REQ0:
            regs.Write32(linkId, MODS_TREX_ARB_WEIGHT_REQ0_ENTRY, weight);
            break;
        case LwLinkTrex::TR_REQ1:
            regs.Write32(linkId, MODS_TREX_ARB_WEIGHT_REQ1_ENTRY, weight);
            break;
        default:
            Printf(Tee::PriError, "Invalid TREX RAM %d\n", static_cast<UINT32>(ram));
            return RC::BAD_PARAMETER;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoSetTrexRun(bool bEnabled)
{
    RC rc;

    RegHal& regs = Regs();

    if (bEnabled)
    {
        for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
        {
            if (!IsLinkActive(linkId))
                continue;

            // Bug 2810820
            // The tag counter isn't being fully reset after each TREX run
            // WAR: Re-asserting Dynamic mode will reset the counter
            // We must also ensure the clock gating is disabled while doing this.
            regs.Write32(linkId, MODS_NPORT_CTRL_SLCG_DIS_CG_TREX_ENABLE);
            regs.Write32(linkId, MODS_TREX_CTRL_MODE_DISABLE);
            regs.Write32(linkId, MODS_TREX_CTRL_MODE_DYNAMIC);
            regs.Write32(linkId, MODS_NPORT_CTRL_SLCG_DIS_CG_TREX__PROD);
        }

        UINT32 data = regs.SetField(MODS_NPORT_CTRL_STOP_ROUTE_STOP_VC_ALLOWTRAFFIC) |
                      regs.SetField(MODS_NPORT_CTRL_STOP_EGRESS_STOP_STOP) |
                      regs.SetField(MODS_NPORT_CTRL_STOP_INGRESS_STOP_STOP);
        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_NPORT_CTRL_STOP, data);


        data = regs.SetField(MODS_TREX_HW_CTRL_ENABLE_ENABLE_HW_READ);

        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_TREX_HW_CTRL, data);

        for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
        {
            if (!IsLinkActive(linkId))
                continue;
            // Flush the previous writes
            regs.Read32(linkId, MODS_TREX_HW_CTRL);
        }

        data = regs.SetField(MODS_NPORT_CTRL_STOP_ROUTE_STOP_VC_ALLOWTRAFFIC) |
               regs.SetField(MODS_NPORT_CTRL_STOP_EGRESS_STOP_ALLOWTRAFFIC) |
               regs.SetField(MODS_NPORT_CTRL_STOP_INGRESS_STOP_STOP);
        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_NPORT_CTRL_STOP, data);

        data = regs.SetField(MODS_NPORT_CTRL_STOP_ROUTE_STOP_VC_ALLOWTRAFFIC) |
               regs.SetField(MODS_NPORT_CTRL_STOP_EGRESS_STOP_ALLOWTRAFFIC) |
               regs.SetField(MODS_NPORT_CTRL_STOP_INGRESS_STOP_ALLOWTRAFFIC);
        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_NPORT_CTRL_STOP, data);
    }
    else
    {
        UINT32 data = regs.SetField(MODS_TREX_HW_CTRL_ENABLE_DISABLE_HW_READ);
        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_TREX_HW_CTRL, data);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoWriteTrexRam
(
    UINT32 linkId,
    LwLinkTrex::TrexRam ram,
    const vector<LwLinkTrex::RamEntry>& entries
)
{
    RC rc;
    RegHal& regs = Regs();

    for (UINT32 i = 0; i < entries.size(); i++)
    {
        const LwLinkTrex::RamEntry& entry = entries[i];
        UINT32 data = 0;

        data = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_ADDRESS, i) |
               ((ram == LwLinkTrex::TR_REQ0) ? regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ0_RAM)
                                             : regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ1_RAM)) |
               regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_AUTO_INCR_DISABLE);
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAM, data);

        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA1, entry.data1);
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA2, entry.data2);
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3, entry.data3);
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4, entry.data4);
        // Data0 must be written last because that actually triggers the RAM write
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0, entry.data0);
    }

    // Verify that the data was actually written
#ifdef DEBUG
    for (UINT32 i = 0; i < entries.size(); i++)
    {
        const LwLinkTrex::RamEntry& entry = entries[i];
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAM,
                     regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_ADDRESS, i) |
                     ((ram == LwLinkTrex::TR_REQ0) ? regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ0_RAM)
                                                   : regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ1_RAM)) |
                     regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_AUTO_INCR_DISABLE));

        UINT32 data = 0;
        // Data0 must be read first because that actually triggers the RAM read
        data = regs.Read32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0);
        MASSERT(data == entry.data0);

        data = regs.Read32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA1);
        MASSERT(data == entry.data1);

        data = regs.Read32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA2);
        MASSERT(data == entry.data2);

        data = regs.Read32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3);
        MASSERT(data == entry.data3);

        data = regs.Read32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4);
        MASSERT(data == entry.data4);
    }
#endif

    UINT32 data = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_ADDRESS, entries.size()-1) |
                  ((ram == LwLinkTrex::TR_REQ0) ? regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ0_RAM)
                                                : regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAM_SEL_REQ1_RAM));
    regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAM, data);

    return rc;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoSetQueueRepeatCount(UINT32 linkId, UINT32 repeatCount)
{
    MASSERT(repeatCount > 0);
    RegHal& regs = Regs();

    UINT32 count = regs.Read32(linkId, MODS_TREX_QUEUE_CTRL_REPEAT_COUNT);
    // There is a bug in Limerock that the queue repeat idx isn't reset after traffic finishes,
    // so we just need to add to the previous count in order to run another TREX test

    // Minus 1 because repeatCount is 0-based
    count += repeatCount - 1;

    regs.Write32(linkId, MODS_TREX_QUEUE_CTRL_REPEAT_COUNT, count);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::DoSetSystemParameter
(
    UINT64 linkMask,
    SystemParameter parm,
    SystemParamValue val
)
{
    RegHal & regs = Regs();
    INT32 maxLink = Utility::BitScanReverse(linkMask);
    if (maxLink == -1)
    {
        Printf(Tee::PriError, "%s : No links specified\n", __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    if (static_cast<UINT32>(maxLink) >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid link mask specified 0x%llx\n", __FUNCTION__, linkMask);
        return RC::BAD_PARAMETER;
    }

    ModsGpuRegValue regValue = MODS_REGISTER_VALUE_NULL;
    ModsGpuRegField regField;
    ModsGpuRegValue lockedValue;
    ModsGpuRegField clearField;

    bool bRangeParameter = false;
    UINT32 rangeValue = 0;
    switch (parm)
    {
        case SystemParameter::RESTORE_PHY_PARAMS:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_RESTORE_PHY_TRAINING_PARAMS_ENABLE :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_RESTORE_PHY_TRAINING_PARAMS_DISABLE;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_RESTORE_PHY_TRAINING_PARAMS;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_RESTORE_PHY_TRAINING_PARAMS_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_RESTORE_PHY_TRAINING_PARAMS;
            break;
        case SystemParameter::SL_ENABLE:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_SL_ENABLE_ENABLE :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_SL_ENABLE_DISABLE;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_SL_ENABLE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_LOCK_PWRM_SL_ENABLE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_LOCK_CLEAR_PWRM_SL_ENABLE;
            break;
        case SystemParameter::L2_ENABLE:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_L2_ENABLE_ENABLE :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_L2_ENABLE_DISABLE;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_L2_ENABLE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_LOCK_PWRM_L2_ENABLE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_LOCK_CLEAR_PWRM_L2_ENABLE;
            break;
        case SystemParameter::LINE_RATE:
            regValue    = SystemLineRateToRegVal(val.lineRate);
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LOCK_LINE_RATE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LOCK_CLEAR_LINE_RATE;
            break;
        case SystemParameter::LINE_CODE_MODE:
            regValue    = SystemLineCodeToRegVal(val.lineCode);
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LINE_CODE_MODE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_LINE_CODE_MODE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR_LINE_CODE_MODE;
            break;
        case SystemParameter::LINK_DISABLE:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LINK_DISABLE_DISABLED :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LINK_DISABLE_ENABLED;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LINK_DISABLE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK_LINK_DISABLE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK_CLEAR_LINK_DISABLE;
            break;
        case SystemParameter::TXTRAIN_OPT_ALGO:
            regValue    = SystemTxTrainAlgToRegVal(val.txTrainAlg);
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_TXTRAIN_OPTIMIZATION_ALGORITHM;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_TXTRAIN_OPTIMIZATION_ALGORITHM_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR_TXTRAIN_OPTIMIZATION_ALGORITHM;
            break;
        case SystemParameter::BLOCK_CODE_MODE:
            regValue    = SystemBlockCodeToRegVal(val.blockCodeMode);
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_BLOCK_CODE_MODE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_BLOCK_CODE_MODE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR_BLOCK_CODE_MODE;
            break;
        case SystemParameter::REF_CLOCK_MODE:
            regValue    = SystemRefClockModeToRegVal(val.refClockMode);
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_REFERENCE_CLOCK_MODE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LOCK_REFERENCE_CLOCK_MODE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LOCK_CLEAR_REFERENCE_CLOCK_MODE;
            break;
        case SystemParameter::LINK_INIT_DISABLE:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LINK_INIT_DISABLE_LINK_INIT_DISABLED :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LINK_INIT_DISABLE_LINK_INIT_ENABLED;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LINK_INIT_DISABLE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK_LINK_INIT_DISABLE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL_LOCK_CLEAR_LINK_INIT_DISABLE;
            break;
        case SystemParameter::ALI_ENABLE:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_ALI_ENABLE_ENABLE :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_ALI_ENABLE_DISABLE;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_ALI_ENABLE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_ALI_ENABLE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_ALI_ENABLE;
            break;
        case SystemParameter::TXTRAIN_MIN_TRAIN_TIME_MANTISSA:
            bRangeParameter = true;
            rangeValue  = val.rangeValue;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_TXTRAIN_MINIMUM_TRAIN_TIME_MANTISSA;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_TXTRAIN_MINIMUM_TRAIN_TIME_MANTISSA_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR_TXTRAIN_MINIMUM_TRAIN_TIME_MANTISSA;
            break;
        case SystemParameter::TXTRAIN_MIN_TRAIN_TIME_EXPONENT:
            bRangeParameter = true;
            rangeValue  = val.rangeValue;
                regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_TXTRAIN_MINIMUM_TRAIN_TIME_EXPONENT;
                lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_TXTRAIN_MINIMUM_TRAIN_TIME_EXPONENT_LOCKED;
                clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL_LOCK_CLEAR_TXTRAIN_MINIMUM_TRAIN_TIME_EXPONENT;
            break;
        case SystemParameter::L1_MIN_RECAL_TIME_MANTISSA:
            bRangeParameter = true;
            rangeValue  = val.rangeValue;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_L1_MINIMUM_RECALIBRATION_TIME_MANTISSA;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_L1_MINIMUM_RECALIBRATION_TIME_MANTISSA_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_L1_MINIMUM_RECALIBRATION_TIME_MANTISSA;
            break;
        case SystemParameter::L1_MIN_RECAL_TIME_EXPONENT:
            bRangeParameter = true;
            rangeValue  = val.rangeValue;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_L1_MINIMUM_RECALIBRATION_TIME_EXPONENT;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_L1_MINIMUM_RECALIBRATION_TIME_EXPONENT_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_L1_MINIMUM_RECALIBRATION_TIME_EXPONENT;
            break;
        case SystemParameter::L1_MAX_RECAL_PERIOD_MANTISSA:
            bRangeParameter = true;
            rangeValue  = val.rangeValue;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_L1_MAXIMUM_RECALIBRATION_PERIOD_MANTISSA;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_L1_MAXIMUM_RECALIBRATION_PERIOD_MANTISSA_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_L1_MAXIMUM_RECALIBRATION_PERIOD_MANTISSA;
            break;
        case SystemParameter::L1_MAX_RECAL_PERIOD_EXPONENT:
            bRangeParameter = true;
            rangeValue  = val.rangeValue;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_L1_MAXIMUM_RECALIBRATION_PERIOD_EXPONENT;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_L1_MAXIMUM_RECALIBRATION_PERIOD_EXPONENT_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_L1_MAXIMUM_RECALIBRATION_PERIOD_EXPONENT;
            break;
        case SystemParameter::DISABLE_UPHY_UCODE_LOAD:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_DISABLE_UPHY_MICROCODE_LOAD_UPHY_MICROCODE_LOAD_DISABLED :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_DISABLE_UPHY_MICROCODE_LOAD_UPHY_MICROCODE_LOAD_ENABLED;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_DISABLE_UPHY_MICROCODE_LOAD;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_DISABLE_UPHY_MICROCODE_LOAD_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_DISABLE_UPHY_MICROCODE_LOAD;
            break;
        case SystemParameter::CHANNEL_TYPE:
            regValue    = SystemChannelTypeToRegVal(val.channelType);
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_CHANNEL_TYPE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CHANNEL_TYPE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_CHANNEL_CTRL2_LOCK_CLEAR_CHANNEL_TYPE;
            break;
        case SystemParameter::L1_ENABLE:
            regValue = val.bEnable ?
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_L1_ENABLE_ENABLE :
                MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_L1_ENABLE_DISABLE;
            regField    = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_PWRM_L1_ENABLE;
            lockedValue = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_LOCK_PWRM_L1_ENABLE_LOCKED;
            clearField  = MODS_LWLIPT_LNK_CTRL_SYSTEM_LINK_AN1_CTRL_LOCK_CLEAR_PWRM_L1_ENABLE;
            break;
        default:
            MASSERT(!"Unknown system setting");
            return RC::SOFTWARE_ERROR;
    }

    // We are in Ampere, these should technically always be supported, but check just in case
    if (!regs.IsSupported(regValue) ||
        !regs.IsSupported(regField) ||
        !regs.IsSupported(lockedValue) ||
        !regs.IsSupported(clearField))
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // These system settings must be setup prior to initialization and therefore
    // cannot access functionality that is only setup during initialization (GetMaxLinks,
    // link validity, link active, etc.).  In addition, because of this requirement the
    // lwlink interface may not be used because getting the interface requires LwLink::IsSupported
    // to return true which requires RM initialization
    RC rc;

    INT32 lwrLink = Utility::BitScanForward(linkMask, 0);
    bool bHulkRequired = false;
    for ( ; lwrLink != -1; lwrLink = Utility::BitScanForward(linkMask, lwrLink + 1))
    {
        const UINT32 uLwrLink = static_cast<UINT32>(lwrLink);
        if (regs.Read32(uLwrLink, regField) == regs.LookupValue(regValue))
            continue;

        // If the registers are locked and full priv protection is enabled it
        // requires a HULK to unlock them
        if (regs.Test32(uLwrLink, lockedValue))
        {
            regs.Write32(uLwrLink, clearField, 1);
            if (regs.Test32(uLwrLink, lockedValue))
            {
                bHulkRequired = true;
                break;
            }
        }
        if (bRangeParameter)
        {
            const UINT32 maxValue = regs.LookupFieldValueMask(regField);
            if (rangeValue > maxValue)
            {
                Printf(Tee::PriError,
                       "LwLink system parameter %s out of range.  Maximum = %u, Requested = %u\n",
                       SystemParameterString(parm).c_str(), maxValue, rangeValue);
                return RC::BAD_PARAMETER;
            }
            regs.Write32(uLwrLink, regField, rangeValue);
        }
        else
        {
            regs.Write32(uLwrLink, regValue);
        }

        if (regs.Read32(uLwrLink, regField) != regs.LookupValue(regValue))
        {
            bHulkRequired = true;
            break;
        }
        regs.Write32(uLwrLink, lockedValue);
    }

    if (bHulkRequired)
    {
        if (Utility::GetSelwrityUnlockLevel() >= Utility::SelwrityUnlockLevel::SUL_LWIDIA_NETWORK)
        {
            Printf(Tee::PriError, "LwLink system configuration registers are locked. "
                                  "Please use appropriate hulk license.\n");
            return RC::HULK_LICENSE_REQUIRED;
        }
        else
        {
            return RC::UNSUPPORTED_HARDWARE_FEATURE; // Hide HULK req from external clients
        }
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ bool LimerockLwLink::DoSupportsFomMode(FomMode mode) const
{
    switch (mode)
    {
        case EYEO_Y:
        case EYEO_Y_U:
        case EYEO_Y_M:
        case EYEO_Y_L:
            return true;
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
UINT32 LimerockLwLink::GetErrorCounterControlMask(bool bIncludePerLane)
{
#define LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_REPLAY           0x08000000
    UINT32 counterMask = LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT    |
                         LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_REPLAY      |
                         LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_RECOVERY    |
                         LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_REPLAY;
    if (bIncludePerLane)
    {
        counterMask |= LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L2 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L3;
    }
    return counterMask;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::GetErrorCountsDirect(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    MASSERT(pErrorCounts);

    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : Unknown linkId %u on LwLink device %s\n",
               MODS_FUNCTION, linkId, GetDevice()->GetName().c_str());
        return RC::BAD_PARAMETER;
    }

    RC rc;
    RegHal& regs = Regs();

    auto SetCount = [&] (ModsGpuRegField field, ErrorCounts::ErrId errId) -> RC
    {
        const UINT32 errCount   = regs.Read32(linkId, field);
        const bool   bOverflow  = (errCount == regs.LookupFieldValueMask(field));
        return pErrorCounts->SetCount(errId, errCount, bOverflow);
    };

    CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT1_FLIT_CRC_ERRORS, ErrorCounts::LWL_RX_CRC_FLIT_ID));
    CHECK_RC(SetCount(MODS_LWLDL_TX_ERROR_COUNT4_REPLAY_EVENTS, ErrorCounts::LWL_TX_REPLAY_ID));
    CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT6_RX_AN0_REPLAY, ErrorCounts::LWL_RX_REPLAY_ID));
    CHECK_RC(SetCount(MODS_LWLDL_TOP_ERROR_COUNT1_RECOVERY_EVENTS, ErrorCounts::LWL_TX_RECOVERY_ID));

    bool bLanesReversed = false;
    if (regs.Test32(linkId, MODS_LWLDL_RX_CONFIG_RX_REVERSAL_OVERRIDE_ON))
    {
        bLanesReversed = regs.Test32(linkId, MODS_LWLDL_RX_CONFIG_RX_LANE_REVERSE_ON);
    }
    else
    {
        bLanesReversed = regs.Test32(linkId, MODS_LWLDL_RX_CONFIG_RX_HW_LANE_REVERSE_ON);
    }
    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));
    if (bPerLaneEnabled)
    {
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT2_LANECRC_L0,
                          bLanesReversed ? ErrorCounts::LWL_RX_CRC_LANE3_ID :
                                           ErrorCounts::LWL_RX_CRC_LANE0_ID));
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT2_LANECRC_L1,
                          bLanesReversed ? ErrorCounts::LWL_RX_CRC_LANE2_ID :
                                           ErrorCounts::LWL_RX_CRC_LANE1_ID));
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT2_LANECRC_L2,
                          bLanesReversed ? ErrorCounts::LWL_RX_CRC_LANE1_ID :
                                           ErrorCounts::LWL_RX_CRC_LANE2_ID));
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT2_LANECRC_L3,
                          bLanesReversed ? ErrorCounts::LWL_RX_CRC_LANE0_ID :
                                           ErrorCounts::LWL_RX_CRC_LANE3_ID));
    }

    bool bEccEnabled = false;
    CHECK_RC(IsPerLaneEccEnabled(linkId, &bEccEnabled));

    LwU32 maxErrors = 0;
    bool  bAnyEccLaneOverflowed = false;

    Tasker::MutexHolder lockCache(GetErrorCountMutex());
    ErrorCounts * pCachedErrorCounts = GetCachedErrorCounts(linkId);
    if (bEccEnabled)
    {
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT7_ECC_CORRECTED_L0,
                          bLanesReversed ? ErrorCounts::LWL_FEC_CORRECTIONS_LANE3_ID :
                                           ErrorCounts::LWL_FEC_CORRECTIONS_LANE0_ID));
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT8_ECC_CORRECTED_L1,
                          bLanesReversed ? ErrorCounts::LWL_FEC_CORRECTIONS_LANE2_ID :
                                           ErrorCounts::LWL_FEC_CORRECTIONS_LANE1_ID));
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT9_ECC_CORRECTED_L2,
                          bLanesReversed ? ErrorCounts::LWL_FEC_CORRECTIONS_LANE1_ID :
                                           ErrorCounts::LWL_FEC_CORRECTIONS_LANE2_ID));
        CHECK_RC(SetCount(MODS_LWLDL_RX_ERROR_COUNT10_ECC_CORRECTED_L3,
                          bLanesReversed ? ErrorCounts::LWL_FEC_CORRECTIONS_LANE0_ID :
                                           ErrorCounts::LWL_FEC_CORRECTIONS_LANE3_ID));

        for (UINT32 lane = 0; lane < LWSWITCH_LWLINK_MAX_LANES; lane++)
        {
            LwLink::ErrorCounts::ErrId errId =
                static_cast<LwLink::ErrorCounts::ErrId>(LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE0_ID + lane);

            const UINT32 lwrCount = pCachedErrorCounts->GetCount(errId) +
                                    pErrorCounts->GetCount(errId);
            if (pErrorCounts->DidCountOverflow(errId))
                bAnyEccLaneOverflowed = true;

            if (lwrCount > maxErrors)
                maxErrors = lwrCount;
        }
    }

    *pCachedErrorCounts += *pErrorCounts;
    *pErrorCounts = *pCachedErrorCounts;

    if (bEccEnabled)
    {
        CHECK_RC(pCachedErrorCounts->SetCount(LwLink::ErrorCounts::LWL_PRE_FEC_ID,
                                              maxErrors,
                                              bAnyEccLaneOverflowed));
        CHECK_RC(pErrorCounts->SetCount(LwLink::ErrorCounts::LWL_PRE_FEC_ID,
                                        maxErrors,
                                        bAnyEccLaneOverflowed));
    }

    return rc;
}

//-----------------------------------------------------------------------------
FLOAT64 LimerockLwLink::GetLinkDataRateEfficiency(UINT32 linkId) const
{
    const RegHal& regs = Regs();

    if (!regs.IsSupported(MODS_LWLDL_TOP_LINK_CONFIG_BLKLEN))
        return 1.0;

    FLOAT64 efficiency = 1.0;
    const UINT32 blklen = regs.Read32(linkId, MODS_LWLDL_TOP_LINK_CONFIG_BLKLEN);
    if (!regs.TestField(blklen, MODS_LWLDL_TOP_LINK_CONFIG_BLKLEN_OFF))
    {
        static const UINT32 BLOCK_SIZE  = 16;
        static const UINT32 FOOTER_BITS = 2;
        const FLOAT64 frameLength = (BLOCK_SIZE * blklen) + FOOTER_BITS;
        efficiency = (frameLength - FOOTER_BITS)  /  frameLength;
    }
    return efficiency;
}

//-----------------------------------------------------------------------------
RC LimerockLwLink::GetEomConfigValue
(
    FomMode mode,
    UINT32 numErrors,
    UINT32 numBlocks,
    UINT32 *pConfigVal
) const
{
    UINT32 modeVal = 0x0;
    UINT32 spareSel = 0x0;
    UINT32 eyeSel = 0x0;
    switch (mode)
    {
        case EYEO_Y:
            modeVal = LW_MINION_UCODE_CONFIGEOM_EOMMODE_Y;
            spareSel = LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_BOTH;
            eyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL;
            break;
        case EYEO_Y_U:
            modeVal = LW_MINION_UCODE_CONFIGEOM_EOMMODE_Y;
            spareSel = LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_P;
            eyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER;
            break;
        case EYEO_Y_M:
            modeVal = LW_MINION_UCODE_CONFIGEOM_EOMMODE_Y;
            spareSel = LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_P;
            eyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_MID;
            break;
        case EYEO_Y_L:
            modeVal = LW_MINION_UCODE_CONFIGEOM_EOMMODE_Y;
            spareSel = LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_N;
            eyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER;
            break;
        default:
            Printf(Tee::PriError, "Invalid or unsupported FOM mode : %d\n", mode);
            return RC::ILWALID_ARGUMENT;
    }

    *pConfigVal = DRF_NUM(_MINION_UCODE, _CONFIGEOM, _EOMMODE,       modeVal)   |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _NUMBLKS,       numBlocks) |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _NUMERRS,       numErrors) |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _EOM_SPARE_SEL, spareSel)  |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _BER_EYE_SEL,   eyeSel);
    return RC::OK;
}
