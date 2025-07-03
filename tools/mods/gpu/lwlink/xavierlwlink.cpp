/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "xavierlwlink.h"
#include "lwmisc.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_libif.h"

#define __EXPORTED_HEADERS__
#define UNDEF_EXPORTED_HEADERS
#undef BIT_IDX_32
#include <linux/types.h>
#include <linux/ioctl.h>
#include "linux/cheetah-lwlink-uapi.h"
#undef UNDEF_EXPORTED_HEADERS
#undef __EXPORTED_HEADERS__

using namespace LwLinkDevIf;

namespace
{
    const FLOAT64 LWLINK_PACKET_BYTES_QUAL_ENG  = 64.0;
    const UINT32  BYTES_PER_FLIT                = 16;

    // DRF definitions for CONTROL_CONFIG_EOM
    // TODO: These should be defined in some external header file
    // See Bug 1808284
    // Field offsets and widths taken from IAS document at:
    // https://p4viewer.lwpu.com/get/dev/lwif/lwlink/mainline/doc/Arch/Internal/DLPL_Spec/asciidoc/published/LWLink_MINION_IAS.html#_gpu_specific_dl_commands
    // Field values taken from comment at:
    // https://lwbugswb.lwpu.com/LwBugs5/SWBug.aspx?bugid=1880995&cmtNo=13
    #define LW_MINION_EOM_NERRS        3:0
    #define LW_MINION_EOM_NBLKS        7:4
    #define LW_MINION_EOM_MODE         11:8
    #define LW_MINION_EOM_MODE_X       0x5
    #define LW_MINION_EOM_MODE_XL      0x0
    #define LW_MINION_EOM_MODE_XH      0x1
    #define LW_MINION_EOM_MODE_Y       0xb
    #define LW_MINION_EOM_MODE_YL      0x6
    #define LW_MINION_EOM_MODE_YH      0x7
    #define LW_MINION_EOM_TYPE         17:16
    #define LW_MINION_EOM_TYPE_ODD     0x1
    #define LW_MINION_EOM_TYPE_EVEN    0x2
    #define LW_MINION_EOM_TYPE_ODDEVEN 0x3
}

//------------------------------------------------------------------------------
//! \brief Save the current per-lane CRC setting
//!
//! \return OK if successful, not OK otherwise
RC XavierLwLink::SavePerLaneCrcSetting()
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

RC XavierLwLink::DoClearHwErrorCounts(UINT32 linkId)
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError,
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    tegra_lwlink_clear_counters clearParams = { };
    clearParams.link_mask = (1 << linkId);
    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));
    clearParams.counter_mask = GetErrorCounterControlMask(bPerLaneEnabled);
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_CLEAR_COUNTERS,
                                            &clearParams,
                                            sizeof(clearParams)));

    // SW Recoveries are self clearing on read
    tegra_lwlink_get_error_recoveries recoveryParams = {};
    recoveryParams.link_mask = 1 << linkId;
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_TEGRA_GET_ERROR_RECOVERIES,
                                            &recoveryParams,
                                            sizeof(recoveryParams)));

    return OK;
}

RC XavierLwLink::DoDetectTopology()
{
    RC rc;

    CHECK_RC(InitializePostDiscovery());
    CHECK_RC(SavePerLaneCrcSetting());

    vector<EndpointInfo> endpointInfo;
    CHECK_RC(GetDetectedEndpointInfo(&endpointInfo));
    SetEndpointInfo(endpointInfo);

    return rc;
}

RC XavierLwLink::DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
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
/* virtual */ RC XavierLwLink::DoGetLinkPowerStateStatus
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
/* virtual */ RC XavierLwLink::DoRequestPowerState
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
UINT32 XavierLwLink::DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
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

LwLinkImpl::CeWidth XavierLwLink::DoGetCeCopyWidth(CeTarget target) const
{
    // Note: iGPU CE cannot be used with LwLink anyway
    return CEW_256_BYTE;
}

RC XavierLwLink::DoGetErrorCounts
(
    UINT32 linkId,
    LwLink::ErrorCounts * pErrorCounts
)
{
    // Counter registers are not implemented on simulation platforms
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return OK;

    MASSERT(pErrorCounts);
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriError,
               "%s : Unknown linkId %u on LwLink device %s\n",
               __FUNCTION__, linkId, GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    tegra_lwlink_get_counters countParams = {};
    bool bPerLaneEnabled = false;
    CHECK_RC(PerLaneErrorCountsEnabled(linkId, &bPerLaneEnabled));

    countParams.link_id = linkId;
    countParams.counter_mask = GetErrorCounterControlMask(bPerLaneEnabled);
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_GET_COUNTERS,
                                            &countParams,
                                            sizeof(countParams)));

    INT32 lwrCounterIdx = Utility::BitScanForward(countParams.counter_mask, 0);
    while (lwrCounterIdx != -1)
    {
        LwLink::ErrorCounts::ErrId errId = ErrorCounterBitToId(1U << lwrCounterIdx);
        if (errId == LwLink::ErrorCounts::LWL_NUM_ERR_COUNTERS)
        {
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(pErrorCounts->SetCount(errId,
                                 static_cast<UINT32>(countParams.lwlink_counters[lwrCounterIdx]),
                                 false));
        lwrCounterIdx = Utility::BitScanForward(countParams.counter_mask,
                                                lwrCounterIdx + 1);
    }

    // SW Recoveries are self clearing on read
    tegra_lwlink_get_error_recoveries recoveryParams = {};
    recoveryParams.link_mask = 1 << linkId;
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_TEGRA_GET_ERROR_RECOVERIES,
                                            &recoveryParams,
                                            sizeof(recoveryParams)));
    CHECK_RC(pErrorCounts->SetCount(LwLink::ErrorCounts::LWL_SW_RECOVERY_ID,
                                    static_cast<UINT32>(recoveryParams.num_recoveries),
                                    false));
    return rc;
}

namespace
{
    pair<ModsGpuRegField, string> rxErr0Fields[] =
    {
        { MODS_LWLTLC_RX_ERR_STATUS_0_DATLENGTATOMICREQMAXERR,           "Receive DatLen is greater than the Atomic Max size (128B, 64B for CAS) Error" },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_DATLENGTRMWREQMAXERR,              "Receive DatLen is greater than the RMW Max size (64B) Error"                  },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_DATLENLTATRRSPMINERR,              "Receive DatLen is less than the ATR response size (8B) Error"                 },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_ILWALIDCACHEATTRPOERR,             "Receive The CacheAttr field and PO field do not agree Error"                  },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_ILWALIDCRERR,                      "Receive Invalid compressed response Error"                                    },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RSVADDRTYPEERR,                    "Receive Reserved Address Type Encoding Error"                                 },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RSVCACHEATTRPROBEREQERR,           "Receive Reserved Cache Attribute Encoding in Probe Request Error"             },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RSVCACHEATTRPROBERSPERR,           "Receive Reserved Cache Attribute Encoding in Probe Response Error"            },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RSVCMDENCERR,                      "Receive Reserved Command Encoding Error"                                      },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RSVDATLENENCERR,                   "Receive Reserved Data Length Encoding Error"                                  },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RSVPKTSTATUSERR,                   "Receive Reserved Packet Status Encoding Error"                                },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RSVRSPSTATUSERR,                   "Receive Reserved RspStatus Encoding Error"                                    },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXDLCTRLPARITYERR,                 "Receive DL Control Parity Error"                                              },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXDLDATAPARITYERR,                 "Receive DL Data Parity Error"                                                 },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXDLHDRPARITYERR,                  "Receive DL Header Parity Error"                                               },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXILWALIDADDRALIGNERR,             "Receive Invalid Address Alignment Error"                                      },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXILWALIDAEERR,                    "Receive Invalid AE Flit Received Error"                                       },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXILWALIDBEERR,                    "Receive Invalid BE Flit Received Error"                                       },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXPKTLENERR,                       "Receive Packet Length Error"                                                  },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXRAMDATAPARITYERR,                "Receive RAM Data Parity Error"                                                },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXRAMHDRPARITYERR,                 "Receive RAM Header Parity Error"                                              },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXRESPSTATUSTARGETERR,             "Receive Receive TE Error in the RspStatus field"                              },//$
        { MODS_LWLTLC_RX_ERR_STATUS_0_RXRESPSTATUSUNSUPPORTEDREQUESTERR, "Receive UR Error in the RspStatus field"                                      }//$
    };

    pair<ModsGpuRegField, string> rxErr1Fields[] =
    {
        { MODS_LWLTLC_RX_ERR_STATUS_1_CORRECTABLEINTERNALERR,    "Receive Correctable Internal Error"              },//$
        { MODS_LWLTLC_RX_ERR_STATUS_1_RXDATAOVFERR,              "Receive Data Overflow Error"                     },//$
        { MODS_LWLTLC_RX_ERR_STATUS_1_RXHDROVFERR,               "Receive Header Overflow Error"                   },//$
        { MODS_LWLTLC_RX_ERR_STATUS_1_RXPOISONERR,               "Receive Data Poisoned Packet Received Error"     },//$
        { MODS_LWLTLC_RX_ERR_STATUS_1_RXUNSUPNCISOCCREDITRELERR, "Receive Unsupported NCISOC Credit Release Error" },//$
        { MODS_LWLTLC_RX_ERR_STATUS_1_RXUNSUPLWLINKCREDITRELERR, "Receive Unsupported LWLink Credit Release Error" },//$
        { MODS_LWLTLC_RX_ERR_STATUS_1_RXUNSUPVCOVFERR,           "Receive Unsupported VC Overflow Error"           },//$
        { MODS_LWLTLC_RX_ERR_STATUS_1_STOMPDETERR,               "Receive Stomped Packet Received Error"           }//$
    };

    pair<ModsGpuRegField, string> txErr0Fields[] =
    {
        { MODS_LWLTLC_TX_ERR_STATUS_0_TARGETERR,             "Transmit Target Error detected in RspStatus"                              },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXDATACREDITOVFERR,    "Transmit Data Credit Overflow Error"                                      },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXDLCREDITOVFERR,      "Transmit DL Replay Credit Overflow Error"                                 },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXDLCREDITPARITYERR,   "Transmit DL Flow Control Interface Parity Error"                          },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXHDRCREDITOVFERR,     "Transmit Header Credit Overflow Error"                                    },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXPOISONDET,           "Transmit Data Poisoned Packet detected on transmit from NCISOC to LWLink" },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXRAMDATAPARITYERR,    "Transmit RAM Data Parity Error"                                           },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXRAMHDRPARITYERR,     "Transmit RAM Header Parity Error"                                         },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXSTOMPDET,            "Transmit Stomped Packet Detected on Transmit from NCISOC to LWLink"       },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_TXUNSUPVCOVFERR,       "Transmit Unsupported VC Overflow Error"                                   },//$
        { MODS_LWLTLC_TX_ERR_STATUS_0_UNSUPPORTEDREQUESTERR, "Transmit Unsupported Request detected in RspStatus"                       }//$
    };
}

RC XavierLwLink::DoGetErrorFlags(LwLink::ErrorFlags * pErrorFlags)
{
    // Put this MASSERT in any function that assumes only one link is present
    // For this function the kernel API always gets flags on all links so if there
    // was more than one link the links we are not interested in would need to be
    // cached just like on dGPU
    MASSERT(GetMaxLinks() < 2);

    MASSERT(pErrorFlags);
    pErrorFlags->linkErrorFlags.clear();
    pErrorFlags->linkGroupErrorFlags.clear();

    // Flag registers are not implemented on simulation platforms
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return OK;

    RC rc;
    tegra_lwlink_get_err_info errorInfoParams = {};
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_GET_ERR_INFO,
                                            &errorInfoParams,
                                            sizeof(errorInfoParams)));

    RegHal& regs = Regs();
    set<string> newFlags;
    for (const auto& errFlag : rxErr0Fields)
    {
        if (regs.GetField(errorInfoParams.link_err_info.tlc_rx_err_status0, errFlag.first))
        {
            newFlags.insert(errFlag.second);
        }
    }
    for (const auto& errFlag : rxErr1Fields)
    {
        if (regs.GetField(errorInfoParams.link_err_info.tlc_rx_err_status1, errFlag.first))
        {
            newFlags.insert(errFlag.second);
        }
    }
    for (const auto& errFlag : txErr0Fields)
    {
        if (regs.GetField(errorInfoParams.link_err_info.tlc_tx_err_status0, errFlag.first))
        {
            newFlags.insert(errFlag.second);
        }
    }

    if (errorInfoParams.link_err_info.bExcess_error_dl)
    {
        newFlags.insert("Excess DL Errors");
    }

    // Kernel interface only supports one link and that link better match the link
    // we are querying
    MASSERT(errorInfoParams.link_mask == 0x1);
    pErrorFlags->linkErrorFlags[0] = { newFlags.begin(), newFlags.end() };

    return rc;
}

RC XavierLwLink::DoGetLinkId
(
    const EndpointInfo &endpointInfo,
    UINT32                        *pLinkId
) const
{
    if (GetDevice()->GetId() != endpointInfo.id)
    {
        Printf(Tee::PriError,
               "%s : Endpoint not present on %s\n", __FUNCTION__,
               GetDevice()->GetName().c_str());
        return RC::SOFTWARE_ERROR;
    }

    *pLinkId = endpointInfo.linkNumber;
    return OK;
}

RC XavierLwLink::DoGetLinkInfo(vector<LinkInfo> *pLinkInfo)
{
    RC rc;
    LwRmPtr pLwRm;

    tegra_lwlink_status statusParams = {};
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_GET_LINK_STATUS,
                                            &statusParams,
                                            sizeof(statusParams)));
    pLinkInfo->clear();

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        LinkInfo lwrInfo;

        if (!(statusParams.enabled_link_mask & (1 << lwrLink)))
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        // There can be holes in the link mask from RM and RM will not
        // virtualize the linkIds (i.e. bit x always corresponds
        // to index x and always refers to physical lwlink x)
        tegra_lwlink_link_status_info *pLinkStat = &statusParams.link_info;

        lwrInfo.bValid = !!(pLinkStat->caps & TEGRA_CTRL_LWLINK_CAPS_VALID);

        lwrInfo.version = pLinkStat->lwlink_version;

        if (!lwrInfo.bValid)
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        tegra_lwlink_device_info *pRemoteInfo = &pLinkStat->remote_device_info;
        lwrInfo.bActive = (pRemoteInfo->device_type !=
                           TEGRA_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE);

        lwrInfo.refClkSpeedMhz   = pLinkStat->lwlink_ref_clk_speedMhz;
        lwrInfo.linkClockMHz     = pLinkStat->lwlink_common_clock_speedKHz / 1000;
        lwrInfo.sublinkWidth     = pLinkStat->sublink_width;
        lwrInfo.bLanesReversed   = (pLinkStat->bLane_reversal == LW_TRUE);

        if (!lwrInfo.bActive)
        {
            pLinkInfo->push_back(lwrInfo);
            continue;
        }

        lwrInfo.lineRateMbps     = pLinkStat->lwlink_link_clockMhz;

        FLOAT64 efficiency = 1.0;
        const UINT32 blklen = Regs().Read32(lwrLink, MODS_PLWL_LINK_CONFIG_BLKLEN);
        if (!Regs().TestField(blklen, MODS_PLWL_LINK_CONFIG_BLKLEN_OFF))
        {
            static const UINT32 BLOCK_SIZE  = 16;
            static const UINT32 FOOTER_BITS = 2;
            const FLOAT64 frameLength = (BLOCK_SIZE * blklen) + FOOTER_BITS;
            efficiency = (frameLength - FOOTER_BITS)  /  frameLength;
        }
        lwrInfo.maxLinkDataRateKiBps = efficiency *
                                       CallwlateLinkBwKiBps(lwrInfo.lineRateMbps,
                                                            lwrInfo.sublinkWidth);
        pLinkInfo->push_back(lwrInfo);
    }

    return rc;
}

RC XavierLwLink::DoGetLinkStatus(vector<LinkStatus> *pLinkStatus)
{
    StickyRC rc;
    tegra_lwlink_status statusParams = {};
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_GET_LINK_STATUS,
                                            &statusParams,
                                            sizeof(statusParams)));
    pLinkStatus->clear();

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        LinkStatus lwrState =
        {
            LS_ILWALID,
            SLS_ILWALID,
            SLS_ILWALID
        };

        if (!(statusParams.enabled_link_mask & (1 << lwrLink)))
        {
            pLinkStatus->push_back(lwrState);
            continue;
        }

        tegra_lwlink_link_status_info *pLinkStat = &statusParams.link_info;
        lwrState.linkState      = RmLinkStateToLinkState(pLinkStat->link_state);
        lwrState.rxSubLinkState =
            RmSubLinkStateToLinkState(pLinkStat->rx_sublink_status);
        lwrState.txSubLinkState =
            RmSubLinkStateToLinkState(pLinkStat->tx_sublink_status);
        pLinkStatus->push_back(lwrState);
    }

    return rc;
}

//------------------------------------------------------------------------------
void XavierLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    MASSERT(pSettings);
    pSettings->numErrors = 0x7;
    pSettings->numBlocks = 0xe;
}

RC XavierLwLink::DoGetEomStatus(FomMode mode,
                                UINT32 link,
                                UINT32 numErrors,
                                UINT32 numBlocks,
                                FLOAT64 timeoutMs,
                                vector<INT32>* status)
{
    MASSERT(status);
    RC rc;

    const UINT32 numLanes = static_cast<UINT32>(status->size());
    MASSERT(numLanes > 0);

    UINT32 typeVal = 0x0;
    UINT32 modeVal = 0x0;
    switch (mode)
    {
        case EYEO_X:
            typeVal = LW_MINION_EOM_TYPE_ODDEVEN;
            modeVal = LW_MINION_EOM_MODE_X;
            break;
        case EYEO_XL_O:
            typeVal = LW_MINION_EOM_TYPE_ODD;
            modeVal = LW_MINION_EOM_MODE_XL;
            break;
        case EYEO_XL_E:
            typeVal = LW_MINION_EOM_TYPE_EVEN;
            modeVal = LW_MINION_EOM_MODE_XL;
            break;
        case EYEO_XH_O:
            typeVal = LW_MINION_EOM_TYPE_ODD;
            modeVal = LW_MINION_EOM_MODE_XH;
            break;
        case EYEO_XH_E:
            typeVal = LW_MINION_EOM_TYPE_EVEN;
            modeVal = LW_MINION_EOM_MODE_XH;
            break;
        case EYEO_Y:
            typeVal = LW_MINION_EOM_TYPE_ODDEVEN;
            modeVal = LW_MINION_EOM_MODE_Y;
            break;
        case EYEO_YL_O:
            typeVal = LW_MINION_EOM_TYPE_ODD;
            modeVal = LW_MINION_EOM_MODE_YL;
            break;
        case EYEO_YL_E:
            typeVal = LW_MINION_EOM_TYPE_EVEN;
            modeVal = LW_MINION_EOM_MODE_YL;
            break;
        case EYEO_YH_O:
            typeVal = LW_MINION_EOM_TYPE_ODD;
            modeVal = LW_MINION_EOM_MODE_YH;
            break;
        case EYEO_YH_E:
            typeVal = LW_MINION_EOM_TYPE_EVEN;
            modeVal = LW_MINION_EOM_MODE_YH;
            break;
        default:
            Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
            return RC::BAD_PARAMETER;
    }

    tegra_lwlink_setup_eom eomSetupParams = {};
    eomSetupParams.link_id = link;
    eomSetupParams.params = DRF_NUM(_MINION, _EOM, _TYPE, typeVal) |
                            DRF_NUM(_MINION, _EOM, _MODE, modeVal) |
                            DRF_NUM(_MINION, _EOM, _NBLKS, numBlocks) |
                            DRF_NUM(_MINION, _EOM, _NERRS, numErrors);

    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_CONFIG_EOM,
                                            &eomSetupParams,
                                            sizeof(eomSetupParams)));

    RegHal& regs = Regs();

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

RC XavierLwLink::DoGetFabricApertures(vector<FabricAperture> *pFas)
{
    MASSERT(pFas);
    *pFas = m_FabricApertures;
    return (m_FabricApertures.empty()) ? RC::SOFTWARE_ERROR : OK;
}

void XavierLwLink::DoGetFlitInfo
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
    MASSERT(hwType == HT_QUAL_ENG);

    // Since the qual engine always coalesess to 64 byte packets, actual and total data
    // flits will always be the same
    *pReadDataFlits = LWLINK_PACKET_BYTES_QUAL_ENG / BYTES_PER_FLIT;
    *pWriteDataFlits = LWLINK_PACKET_BYTES_QUAL_ENG / BYTES_PER_FLIT;

    // Minimum of 2 data flits on read
    *pTotalReadFlits  = static_cast<UINT32>(*pReadDataFlits);
    *pTotalWriteFlits = static_cast<UINT32>(*pWriteDataFlits);
}

UINT32 XavierLwLink::DoGetMaxLinks() const
{
    MASSERT(m_bLwCapsLoaded);
    return m_MaxLinks;
}

FLOAT64 XavierLwLink::DoGetMaxObservableBwPct(LwLink::TransferType tt)
{
    return 0.99;
}

FLOAT64 XavierLwLink::DoGetMaxTransferEfficiency() const
{
    return 0.94;
}

RC XavierLwLink::DoGetDetectedEndpointInfo
(
    vector<EndpointInfo> *pEndpointInfo
)
{
    RC rc;
    LwRmPtr pLwRm;

    tegra_lwlink_status statusParams = {};
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_GET_LINK_STATUS,
                                            &statusParams,
                                            sizeof(statusParams)));
    pEndpointInfo->clear();

    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        pEndpointInfo->push_back(EndpointInfo());

        if (!(statusParams.enabled_link_mask & (1 << lwrLink)))
            continue;

        // There can be holes in the link mask from RM and RM will not
        // virtualize the linkIds (i.e. bit x always corresponds
        // to index x and always refers to physical lwlink x)
        tegra_lwlink_link_status_info *pLinkStat = &statusParams.link_info;

        if ((pLinkStat->caps & TEGRA_CTRL_LWLINK_CAPS_VALID) == 0)
        {
            continue;
        }

        if (!pLinkStat->connected)
            continue;

        tegra_lwlink_device_info *pRemoteInfo = &pLinkStat->remote_device_info;

        if (pRemoteInfo->device_type == TEGRA_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE)
        {
            const Tee::Priority pri =
                (Platform::GetSimulationMode() != Platform::Hardware) ? Tee::PriWarn :
                                                                        Tee::PriError;
            Printf(pri,
                   "%s : RM reported connected link with no remote device type at link %u!\n",
                   __FUNCTION__, lwrLink);

            // SIM platforms do not always lwrrently return valid data for status calls
            // and failing will regress arch tests.  Simply flag the link as unconnected
            // and continue when this happens
            if (Platform::GetSimulationMode() != Platform::Hardware)
            {
                pLinkStat->connected = 0;
                continue;
            }
            return RC::SOFTWARE_ERROR;
        }

        EndpointInfo lwrInfo;
        switch (pRemoteInfo->device_type)
        {
            case TEGRA_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_TEGRA:
                lwrInfo.devType = TestDevice::TYPE_LWIDIA_GPU;
                // CheetAh devices are only differentiated by their Domain
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain, 0xffff, 0xffff, 0xffff);
                break;

            case TEGRA_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_GPU:
                lwrInfo.devType = TestDevice::TYPE_LWIDIA_GPU;
                lwrInfo.id.SetPciDBDF(pRemoteInfo->domain,
                                      pRemoteInfo->bus,
                                      pRemoteInfo->device,
                                      pRemoteInfo->function);
                break;
            case TEGRA_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_SWITCH:
            case TEGRA_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NPU:
            case TEGRA_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_EBRIDGE:
                Printf(Tee::PriError, "%s : Unsupported remote device type %u\n",
                       __FUNCTION__, static_cast<UINT32>(pRemoteInfo->device_type));
                return RC::SOFTWARE_ERROR;

            default:
                Printf(Tee::PriError, "%s : Unknown remote device type %u\n",
                       __FUNCTION__, static_cast<UINT32>(pRemoteInfo->device_type));
                return RC::SOFTWARE_ERROR;
        }
        lwrInfo.linkNumber  = pLinkStat->remote_device_link_number;
        lwrInfo.pciDomain   = pRemoteInfo->domain;
        lwrInfo.pciBus      = pRemoteInfo->bus;
        lwrInfo.pciDevice   = pRemoteInfo->device;
        lwrInfo.pciFunction = pRemoteInfo->function;
        pEndpointInfo->at(lwrLink) = lwrInfo;
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC XavierLwLink::DoClearLPCounts(UINT32 linkId)
{
    // Put this MASSERT in any function that assumes only one link is present
    MASSERT(GetMaxLinks() < 2);

    tegra_lwlink_clear_lp_counters clearParams{ linkId };
    return GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                          LibInterface::CONTROL_TEGRA_CLEAR_LP_COUNTERS,
                                          &clearParams,
                                          sizeof(clearParams));
}

//------------------------------------------------------------------------------
/* virtual */ RC XavierLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
) 
{
    // Put this MASSERT in any function that assumes only one link is present
    MASSERT(GetMaxLinks() < 2);
    MASSERT(nullptr != pCount);

    *pCount = 0;
    auto cntNum = entry ? TEGRA_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_ENTER :
                          TEGRA_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_EXIT;

    tegra_lwlink_get_lp_counters getCountersParams
    {
        linkId,
        LWBIT32(cntNum)
    };

    RC rc;
    CHECK_RC(GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                            LibInterface::CONTROL_TEGRA_GET_LP_COUNTERS,
                                            &getCountersParams,
                                            sizeof(getCountersParams)));

    *pCount = getCountersParams.counter_values[cntNum];

    auto cntReg = entry ? MODS_PLWL_STATS_D_NUM_TX_LP_ENTER :
                          MODS_PLWL_STATS_D_NUM_TX_LP_EXIT;
    *pbOverflow = *pCount == Regs().LookupMask(cntReg);
    return rc;
}

RC XavierLwLink::DoPerLaneErrorCountsEnabled
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

RC XavierLwLink::DoRegRd
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64*      pData
)
{
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid link ID %u!\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    RC rc;
    UINT08* pRegs;
    CHECK_RC(GetDomainPtr(domain, &pRegs));
    *pData = static_cast<UINT64>(MEM_RD32(pRegs + offset));
    return rc;
}

RC XavierLwLink::DoRegWr
(
    UINT32       linkId,
    RegHalDomain domain,
    UINT32       offset,
    UINT64       data
)
{
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid link ID %u!\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }

    RC rc;
    UINT08* pRegs;
    CHECK_RC(GetDomainPtr(domain, &pRegs));
    MEM_WR32(pRegs + offset, static_cast<UINT32>(data));
    return rc;
}

void XavierLwLink::DoSetCeCopyWidth(CeTarget target, CeWidth width)
{
}

RC XavierLwLink::DoSetFabricApertures(const vector<FabricAperture> &fas)
{
    m_FabricApertures = fas;
    return OK;
}

RC XavierLwLink::DoSetupTopology
(
    const vector<EndpointInfo> &endpointInfo,
    UINT32 topologyId
)
{
    RC rc;

    Printf(Tee::PriLow,
           "%s : Setting up topology on %s\n",
           __FUNCTION__, GetDevice()->GetName().c_str());

    SetEndpointInfo(endpointInfo);

    CHECK_RC(SavePerLaneCrcSetting());

    m_TopologyId = topologyId;

    return rc;
}

/* virtual */ bool XavierLwLink::DoSupportsFomMode(FomMode mode) const
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

RC XavierLwLink::DoShutdown()
{
    StickyRC rc;

    if (m_SavedPerLaneEnable != ~0U)
        rc = EnablePerLaneErrorCounts(m_SavedPerLaneEnable ? true : false);

    rc = LwLinkImpl::DoShutdown();

    return rc;
}

RC XavierLwLink::DoInitialize(LibDevHandles handles)
{
    MASSERT(handles.size() == 1);
    RC rc;

    m_LibHandle = handles[0];

    LoadLwLinkCaps(m_LibHandle);
    if (RC::OK != m_LoadLwLinkCapsRc)
    {
        Printf(Tee::PriError, "%s : Unable to get lwlink caps!\n", __FUNCTION__);
        return m_LoadLwLinkCapsRc;
    }

    CHECK_RC(LwLinkImpl::DoInitialize(handles));
    return rc;
}

bool XavierLwLink::DoIsSupported(LibDevHandles handles) const
{
    MASSERT(handles.size() <= 1);
    Device::LibDeviceHandle handle = (handles.size() > 0) ? handles[0] : GetLibHandle();
    const_cast<XavierLwLink*>(this)->LoadLwLinkCaps(handle);
    return m_bSupported;
}

void XavierLwLink::LoadLwLinkCaps(Device::LibDeviceHandle handle)
{
    if (m_bLwCapsLoaded)
        return;

    tegra_lwlink_caps capParams = {};
    m_LoadLwLinkCapsRc = GetTegraLwLinkLibIf()->Control(handle,
                                                        LibInterface::CONTROL_GET_LINK_CAPS,
                                                        &capParams,
                                                        sizeof(capParams));
    if (RC::OK == m_LoadLwLinkCapsRc)
    {
        m_bSupported = !!(capParams.lwlink_caps & TEGRA_CTRL_LWLINK_CAPS_SUPPORTED);
        if (m_bSupported)
        {
            INT32 maxLinks = Utility::BitScanReverse(capParams.discovered_link_mask) + 1;
            m_MaxLinks = static_cast<UINT32>(maxLinks);

            m_bConnDetectSupported =
                !!(capParams.lwlink_caps & TEGRA_CTRL_LWLINK_CAPS_SLI_BRIDGE_SENSABLE);
        }
    }
    else
    {
        // If loading the caps actually fails report that supported is true but
        // no links are present. the RC code will be checked in initialize and
        // cause MODS to fail
        m_MaxLinks = 0;
        m_bSupported = true;
    }
    m_bLwCapsLoaded = true;
    return;
}

//-----------------------------------------------------------------------------
template <>
RC LwLinkImpl::ValidateLinkState(LwLink::SubLinkState state, const tegra_lwlink_link_state &linkState)
{
    switch (state)
    {
        case LwLink::SLS_OFF:
            if ((linkState.link_mode != TEGRA_CTRL_LWLINK_LINK_OFF) ||
                (linkState.tx_sublink_mode != TEGRA_CTRL_LWLINK_TX_OFF) ||
                (linkState.rx_sublink_mode != TEGRA_CTRL_LWLINK_RX_OFF))
            {
                return RC::HW_STATUS_ERROR;
            }
            break;
        case LwLink::SLS_SAFE_MODE:
            if ((linkState.link_mode != TEGRA_CTRL_LWLINK_LINK_SAFE) ||
                (linkState.tx_sublink_mode != TEGRA_CTRL_LWLINK_TX_SAFE) ||
                (linkState.rx_sublink_mode != TEGRA_CTRL_LWLINK_RX_SAFE))
            {
                return RC::HW_STATUS_ERROR;
            }
            break;
        case LwLink::SLS_HIGH_SPEED:
            if ((linkState.link_mode != TEGRA_CTRL_LWLINK_LINK_HS) ||
                ((linkState.tx_sublink_mode != TEGRA_CTRL_LWLINK_TX_HS) &&
                 (linkState.tx_sublink_mode != TEGRA_CTRL_LWLINK_TX_SINGLE_LANE)) ||
                ((linkState.rx_sublink_mode != TEGRA_CTRL_LWLINK_RX_HS) &&
                 (linkState.rx_sublink_mode != TEGRA_CTRL_LWLINK_RX_SINGLE_LANE)))
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
/*virtual*/ RC XavierLwLink::ChangeLinkState
(
    SubLinkState fromState
   ,SubLinkState toState
   ,const EndpointInfo &locEp
   ,const EndpointInfo &remEp
)
{
    // If the lwlink core library is present, link state changes should use it
    // instead
    if (LwLinkDevIf::GetLwLinkLibIf())
        return LwLinkImpl::ChangeLinkState(fromState, toState, locEp, remEp);

    StickyRC rc;

    tegra_lwlink_train_intranode_conn trainParams = { };

    trainParams.src_end_point.node_id             = 0;
    trainParams.src_end_point.pci_info.domain     = static_cast<__u16>(locEp.pciDomain);
    trainParams.src_end_point.pci_info.bus        = static_cast<__u8>(locEp.pciBus);
    trainParams.src_end_point.pci_info.device     = static_cast<__u8>(locEp.pciDevice);
    trainParams.src_end_point.pci_info.function   = static_cast<__u8>(locEp.pciFunction);
    trainParams.src_end_point.link_index          = locEp.linkNumber;

    trainParams.dst_end_point.node_id             = 0;
    trainParams.dst_end_point.pci_info.domain     = static_cast<__u16>(remEp.pciDomain);
    trainParams.dst_end_point.pci_info.bus        = static_cast<__u8>(remEp.pciBus);
    trainParams.dst_end_point.pci_info.device     = static_cast<__u8>(remEp.pciDevice);
    trainParams.dst_end_point.pci_info.function   = static_cast<__u8>(remEp.pciFunction);
    trainParams.dst_end_point.link_index          = remEp.linkNumber;

    switch (toState)
    {
        case SLS_OFF:
            trainParams.train_to = tegra_lwlink_train_conn_to_off;
            break;
        case SLS_SAFE_MODE:
            trainParams.train_to = (fromState == SLS_HIGH_SPEED) ?
                                      tegra_lwlink_train_conn_active_to_swcfg :
                                      tegra_lwlink_train_conn_off_to_swcfg;
            break;
        case SLS_HIGH_SPEED:
            MASSERT(fromState == SLS_SAFE_MODE);
            trainParams.train_to = tegra_lwlink_train_conn_swcfg_to_active;
            break;

        default:
            Printf(Tee::PriError, GetTeeModule()->GetCode(),
                   "%s : Unsupported training destination link state %s\n",
                   __FUNCTION__,
                   SubLinkStateToString(toState).c_str());
            return RC::SOFTWARE_ERROR;
    }

    rc = GetTegraLwLinkLibIf()->Control(GetLibHandle(),
                                        LibInterface::CONTROL_TEGRA_TRAIN_INTRANODE_CONN,
                                        &trainParams,
                                        sizeof(trainParams));

    rc = ValidateLinkState(toState, trainParams.src_end_state);
    rc = ValidateLinkState(toState, trainParams.dst_end_state);

    return rc;
}

//------------------------------------------------------------------------------
RC XavierLwLink::DoWaitForLinkInit()
{
    // Waiting for link init is only possible if the lwlink core library interface
    // is present
    if (LwLinkDevIf::GetLwLinkLibIf())
        return LwLinkImpl::DoWaitForLinkInit();
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// The LWLINK_CFG space is divided into 32K blocks.  Information provided by Ryan Abraham
// 
//     0 -  32K  : Common registers
//   32K -  64K  : LwLink Multicast registers (goes to all links)
//   64K -  96K  : LwLink 0 registers
//   96K - 128K  : LwLink 1 registers (unused on Xavier)
// ...
// 
// Within each link specific block it is further divided:
// 
//      0 - 0x3FFF : DLPL Block (covered in dev_lwl_ip.h)
// 0x4000 - 0x4FFF : MIF Block (no header)
// 0x6000 - 0x6FFF : TLC Block (covered in dev_lwltlc_ip.h)
//
// Normally all offsets would be discovered through the HW discovery table, however
// this is not yet implemented either in MODS or in the kernel driver
//
#define LWLINK_CFG_DLPL_OFFSET   0x10000
#define LWLINK_CFG_LWLTLC_OFFSET 0x16000

//------------------------------------------------------------------------------
RC XavierLwLink::GetDomainPtr(RegHalDomain domain, UINT08 **ppRegs)
{
    UINT32 domainOffset = 0;
    switch (domain)
    {
        case RegHalDomain::DLPL:
            domainOffset = LWLINK_CFG_DLPL_OFFSET;
            break;
        case RegHalDomain::LWLTLC:
            domainOffset = LWLINK_CFG_LWLTLC_OFFSET;
            break;
        default:
            Printf(Tee::PriError,
                   "%s : Invalid domain number %u!\n",
                   __FUNCTION__,
                   static_cast<UINT32>(domain));
            return RC::BAD_PARAMETER;
    }

    RC rc;

    CHECK_RC(GetRegPtr(reinterpret_cast<void **>(ppRegs)));
    *ppRegs = *ppRegs + domainOffset;
    return rc;
}

