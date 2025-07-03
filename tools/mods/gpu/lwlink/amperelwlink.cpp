/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "amperelwlink.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghaltable.h"
#include "core/include/utility.h"
#include "core/include/tee.h"
#include "ctrl/ctrl2080.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "lwlink/minion/minion_lwlink_defines_public_ga100.h"

using namespace LwLinkDevIf;

namespace
{
    const UINT32 BLOCK_SIZE  = 16;
    const UINT32 FOOTER_BITS = 2;

    // Field definitions in minion_lwlink_defines_public_ga100.h
    #define LW_MINION_UCODE_CONFIGEOM_EOMMODE_Y                0x2
    #define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_NONE       0x0
    #define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_N    0x1
    #define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_P    0x2
    #define LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_BOTH 0x3
    #define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER        0x1
    #define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_MID          0x100
    #define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER        0x1
    #define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL          0xf

    const FLOAT64 BW_PCT_MAX                 = 0.99;
    const FLOAT64 BW_PCT_4_SITE              = 0.92;
    const FLOAT64 BW_PCT_WRITE_GA100         = 0.80;
    const FLOAT64 BW_PCT_WRITE_COLLAPSED_RSP = 0.93;
    const FLOAT64 BW_PCT_UNSUPPORTED_CFG     = 0.60;
}

/* static */ const AmpereLwLink::ErrorFlagDefinition AmpereLwLink::s_ErrorFlags =
{
    {
        TLCRX0,
        {
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXDLHDRPARITYERR,                  "Receive DL Header Parity Error"                                               }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXDLDATAPARITYERR,                 "Receive DL Data Parity Error"                                                 }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXDLCTRLPARITYERR,                 "Receive DL Control Parity Error"                                              }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXILWALIDAEERR,                    "Receive Invalid AE Flit Received Error"                                       }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXILWALIDBEERR,                    "Receive Invalid BE Flit Received Error"                                       }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXILWALIDADDRALIGNERR,             "Receive Invalid Address Alignment Error"                                      }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXPKTLENERR,                       "Receive Packet Length Error"                                                  }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RSVCMDENCERR,                      "Receive Reserved Command Encoding Error"                                      }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RSVDATLENENCERR,                   "Receive Reserved Data Length Encoding Error"                                  }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RSVPKTSTATUSERR,                   "Receive Reserved Packet Status Encoding Error"                                }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RSVCACHEATTRPROBEREQERR,           "Receive Reserved Cache Attribute Encoding in Probe Request Error"             }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RSVCACHEATTRPROBERSPERR,           "Receive Reserved Cache Attribute Encoding in Probe Response Error"            }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_DATLENGTRMWREQMAXERR,              "Receive DatLen is greater than the RMW Max size (64B) Error"                  }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_DATLENLTATRRSPMINERR,              "Receive DatLen is less than the ATR response size (8B) Error"                 }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_ILWALIDCACHEATTRPOERR,             "Receive The CacheAttr field and PO field do not agree Error"                  }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_ILWALIDCRERR,                      "Receive Invalid compressed response Error"                                    }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXRSPSTATUS_HW_ERR,                "Receive hardware error response status"                                       }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXRSPSTATUS_UR_ERR,                "Receive unsupported request response status"                                  }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXRSPSTATUS_PRIV_ERR,              "Receive priv error response status"                                           }, //$                                                                                                                                                                   
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_ILWALID_COLLAPSED_RESPONSE_ERR,    "Receive invalid collapsed response"                                           }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXRSPSTATUS_DATA_HW_ERR,           "Receive hardware error in response status in response with data"              }, // Hopper //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXRSPSTATUS_DATA_UR_ERR,           "Receive unsupported request in response status in response with data"         }, // Hopper //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_RXRSPSTATUS_DATA_PRIV_ERR,         "Receive priveledge errir in response status in response with data"            }, // Hopper //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_0_DATLEN192PACKETERR,                "Receive unexpected 192 byte packet"                                           }  // Hopper //$
        }
    },
    {
        TLCRX1,
        {
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_1_AN1_HEARTBEAT_TIMEOUT_ERR, "Receive AN1 heartbeat timeout"                     }, // Ampere //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_1_HEARTBEAT_TIMEOUT_ERR,     "Receive heartbeat timeout"                         }, // Hopper //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_1_ILLEGALPRIWRITE,           "Receive Illegal Pri write"                         }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_1_RXDATAOVFERR,              "Receive Data Overflow Error"                       }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_1_RXHDROVFERR,               "Receive Header Overflow Error"                     }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_1_RXPOISONERR,               "Receive Data Poisoned Packet Received Error"       }, //$
            { MODS_LWLTLC_RX_LNK_ERR_STATUS_1_STOMPDETERR,               "Receive Stomped Packet Received Error"             }  //$
        }
    },
    {
        TLCRXSYS0,
        {
            { MODS_LWLTLC_RX_SYS_ERR_STATUS_0_NCISOC_PARITY_ERR,      "Receive NCISOC parity Error"         }, //$
            { MODS_LWLTLC_RX_SYS_ERR_STATUS_0_HDR_RAM_ECC_DBE_ERR,    "Receive Header RAM ECC DBE Error"    }, //$
            { MODS_LWLTLC_RX_SYS_ERR_STATUS_0_HDR_RAM_ECC_LIMIT_ERR,  "Receive Header RAM ECC Limit Error"  }, //$
            { MODS_LWLTLC_RX_SYS_ERR_STATUS_0_DAT0_RAM_ECC_DBE_ERR,   "Receive Data0 RAM ECC DBE Error"     }, //$
            { MODS_LWLTLC_RX_SYS_ERR_STATUS_0_DAT0_RAM_ECC_LIMIT_ERR, "Receive Data0 RAM ECC Limit Error"   }, //$
            { MODS_LWLTLC_RX_SYS_ERR_STATUS_0_DAT1_RAM_ECC_DBE_ERR,   "Receive Data1 RAM ECC DBE Error"     }, //$
            { MODS_LWLTLC_RX_SYS_ERR_STATUS_0_DAT1_RAM_ECC_LIMIT_ERR, "Receive Data1 RAM ECC Limit Error"   }  //$
        }
    },
    {
        TLCTX0,
        {
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_CREQ_RAM_HDR_ECC_DBE_ERR, "Transmit request RAM header ECC double bit error"   }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_CREQ_RAM_DAT_ECC_DBE_ERR, "Transmit request RAM data ECC double bit error"     }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_CREQ_RAM_ECC_LIMIT_ERR,   "Transmit request RAM ECC limit error"               }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_RSP_RAM_HDR_ECC_DBE_ERR,  "Transmit response RAM header ECC double bit error"  }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_RSP_RAM_DAT_ECC_DBE_ERR,  "Transmit response RAM data ECC double bit error"    }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_RSP_RAM_ECC_LIMIT_ERR,    "Transmit response RAM ECC limit error"              }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_COM_RAM_HDR_ECC_DBE_ERR,  "Transmit command RAM header ECC double bit error"   }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_COM_RAM_DAT_ECC_DBE_ERR,  "Transmit command RAM data ECC double bit error"     }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_COM_RAM_ECC_LIMIT_ERR,    "Transmit command RAM ECC limit error"               }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_ILLEGALPRIWRITE,          "Transmit Illegal Pri write"                         }, //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_0_TXDLCREDITPARITYERR,      "Transmit DL Flow Control Interface Parity Error"    }  //$
        }
    },
    {
        TLCTX1,
        {
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC0, "Transmit VC0 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC1, "Transmit VC1 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC2, "Transmit VC2 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC3, "Transmit VC3 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC4, "Transmit VC4 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC5, "Transmit VC5 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC6, "Transmit VC6 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_AN1_TIMEOUT_VC7, "Transmit VC7 AN1 Timeout"  }, // Ampere //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC0,     "Transmit VC0 Timeout"      }, // Hopper //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC1,     "Transmit VC1 Timeout"      }, // Hopper //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC2,     "Transmit VC2 Timeout"      }, // Hopper //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC3,     "Transmit VC3 Timeout"      }, // Hopper //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC4,     "Transmit VC4 Timeout"      }, // Hopper //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC5,     "Transmit VC5 Timeout"      }, // Hopper //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC6,     "Transmit VC6 Timeout"      }, // Hopper //$
            { MODS_LWLTLC_TX_LNK_ERR_STATUS_1_TIMEOUT_VC7,     "Transmit VC7 Timeout"      }  // Hopper //$
        }
    },
    {
        TLCTXSYS0,
        {
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_NCISOC_PARITY_ERR,      "Transmit NCISOC Parity Error"                        }, //$
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_NCISOC_HDR_ECC_DBE_ERR, "Transmit NCISOC Header ECC DBE Error"                }, //$
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_NCISOC_DAT_ECC_DBE_ERR, "Transmit NCISOC Data ECC DBE Error"                  }, //$
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_NCISOC_ECC_LIMIT_ERR,   "Transmit NCISOC ECC Limit Error"                     }, //$
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_TXPOISONDET,            "Transmit Poison Detect"                              }, //$
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_TXRSPSTATUS_HW_ERR,     "Transmit Response Status HW Error"                   }, //$
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_TXRSPSTATUS_UR_ERR,     "Transmit Response Status Unsupported Request Error"  }, //$
            { MODS_LWLTLC_TX_SYS_ERR_STATUS_0_TXRSPSTATUS_PRIV_ERR,   "Transmit Response Status Priv Error"                 }  //$
        }
    },
    {
        LWLIPT,
        {
            { MODS_LWLIPT_COMMON_ERR_STATUS_0_CLKCTL_ILLEGAL_REQUEST, "LWLIPT Illegal Clock Control Request"     }, //$
            { MODS_LWLIPT_COMMON_ERR_STATUS_0_RSTSEQ_PLL_TIMEOUT,     "LWLIPT Reset Sequencer PLL Timeout"       }, //$
            { MODS_LWLIPT_COMMON_ERR_STATUS_0_RSTSEQ_PHYARB_TIMEOUT,  "LWLIPT Reset Sequencer PHY ARB Timeout"   }, //$
        }
    },
    {
        LWLIPT_LNK,
        {
            { MODS_LWLIPT_LNK_ERR_STATUS_0_SLEEPWHILEACTIVELINK,           "LWLIPT Sleep While Active Link"                }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_ILLEGALLINKSTATEREQUEST,        "LWLIPT Illegal Link State Request"             }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_FAILEDMINIONREQUEST,            "LWLIPT Failed Minion Request"                  }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_RESERVEDREQUESTVALUE,           "LWLIPT Reserved Request Value"                 }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_LINKSTATEWRITEWHILEBUSY,        "LWLIPT Link State Write While Busy"            }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_LINK_STATE_REQUEST_TIMEOUT,     "LWLIPT Link State Request Timeout"             }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_WRITE_TO_LOCKED_SYSTEM_REG_ERR, "LWLIPT Write to Locked System Register"        }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_RSTSEQ_PHYCTL_TIMEOUT,          "LWLIPT Reset Sequencer PHY Control Timeout"    }, //$
            { MODS_LWLIPT_LNK_ERR_STATUS_0_RSTSEQ_CLKCTL_TIMEOUT,          "LWLIPT Reset Sequencer Clock Control Timeout"  }  //$
        }
    }
};

//------------------------------------------------------------------------------
void AmpereLwLink::SetLinkInfo
(
    UINT32                                      linkId,
    const LW2080_CTRL_LWLINK_LINK_STATUS_INFO & rmLinkInfo,
    LinkInfo *                                  pLinkInfo
) const
{
    pLinkInfo->bValid =
        (LW2080_CTRL_LWLINK_GET_CAP(((const LwU8*)(&rmLinkInfo.capsTbl)),
                                    LW2080_CTRL_LWLINK_CAPS_VALID) != 0);

    pLinkInfo->version = rmLinkInfo.lwlinkVersion;

    if (!pLinkInfo->bValid)
        return;

    const LW2080_CTRL_LWLINK_DEVICE_INFO & remoteInfo = rmLinkInfo.remoteDeviceInfo;
    pLinkInfo->bActive = (remoteInfo.deviceType !=
                       LW2080_CTRL_LWLINK_DEVICE_INFO_DEVICE_TYPE_NONE);

    // Ensure that refClk is unknown on Ampere (instead of just 0).  There is only
    // one ref clock on Ampere+ and RM isnt required to report it
    pLinkInfo->refClkSpeedMhz    = UNKNOWN_RATE;
    pLinkInfo->sublinkWidth      = rmLinkInfo.subLinkWidth;
    pLinkInfo->bLanesReversed    = (rmLinkInfo.bLaneReversal == LW_TRUE);
    pLinkInfo->linkClockMHz      = rmLinkInfo.lwlinkLinkClockMhz;
    pLinkInfo->rxdetLaneMask     = rmLinkInfo.laneRxdetStatusMask;

    if (!pLinkInfo->bActive)
        return;

    // Only capture line/data rate if there is an active remote partner
    pLinkInfo->lineRateMbps      = rmLinkInfo.lwlinkLineRateMbps;

    // RM reported link data rate KiBps and per lane rxdet status mask is only valid on Ampere
    pLinkInfo->maxLinkDataRateKiBps = rmLinkInfo.lwlinkLinkDataRateKiBps;
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoClearHwErrorCounts(UINT32 linkId)
{
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
    // GA100 minion ucode has a bug and does not actually clear RX replays
    if (GetDevice()->GetDeviceId() == Device::GA100)
    {
        Regs().Write32(linkId, MODS_LWLDL_RX_ERROR_COUNT_CTRL_CLEAR_REPLAY_CLEAR);
    }
#endif

    // Do not use turing clear (turing clears LP counters with errors while
    // Ampere/Volta do not)
    return VoltaLwLink::DoClearHwErrorCounts(linkId);
}

//------------------------------------------------------------------------------
//! \brief Enable per lane error counts
//!
//! \param bEnable : True to enable per-lane counts, false to disable
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC AmpereLwLink::DoEnablePerLaneErrorCounts(UINT32 linkId, bool bEnable)
{
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;
    }
    Regs().Write32(linkId, MODS_LWLDL_TX_TXLANECRC_ENABLE, bEnable ? 1 : 0);
    return OK;
}

//------------------------------------------------------------------------------
FLOAT64 AmpereLwLink::DoGetDefaultErrorThreshold
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

//-----------------------------------------------------------------------------
void AmpereLwLink::DoGetEomDefaultSettings
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

//------------------------------------------------------------------------------
//! \brief Run the EOM experiment and return the status value for each lane
//!        on this link
//!
//! \param mode      : The mode to run the EOM in, eg. EYEO_X
//! \param type      : The type of EOM to run, eg. ODDEVEN
//! \param link      : The individual link to run the EOM on
//! \param numErrors : Number of errors to use in the EOM
//! \param numBlocks : Number of blocks to use in the EOM
//! \param timeoutMs : How long to wait for the EOM to finish
//! \param status    : Output array that will contain the status for each lane
//!
//! \return OK if successful, not OK otherwise
//!
RC AmpereLwLink::DoGetEomStatus
(
    FomMode         mode,
    UINT32          link,
    UINT32          numErrors,
    UINT32          numBlocks,
    FLOAT64         timeoutMs,
    vector<INT32> * status
)
{
    MASSERT(status);
    RC rc;

    GpuSubdevice* pGpuSubdev = GetGpuSubdevice();
    RegHal& regs = Regs();

    if (link >= GetMaxLinks())
    {
        Printf(Tee::PriError,
               "%s : Invalid link = %d. Maybe eye diagram registers need to be updated.\n",
               __FUNCTION__, link);
        return RC::BAD_PARAMETER;
    }

    if (!SupportsFomMode(mode))
    {
        Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
        return RC::BAD_PARAMETER;
    }

    // PHY registers are not implemented in C models, return static non-zero data
    // in that case so the test can actually be run
    if ((Platform::GetSimulationMode() == Platform::Fmodel) ||
        (Platform::GetSimulationMode() == Platform::Amodel))
    {
        fill(status->begin(), status->end(), 1);
        return OK;
    }

    const UINT32 numLanes = static_cast<UINT32>(status->size());
    MASSERT(numLanes > 0);

    LW2080_CTRL_CMD_LWLINK_SETUP_EOM_PARAMS eomSetupParams = {};
    eomSetupParams.linkId = link;
    CHECK_RC(GetEomConfigValue(mode, numErrors, numBlocks, &eomSetupParams.params));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(pGpuSubdev, LW2080_CTRL_CMD_LWLINK_SETUP_EOM,
                                           &eomSetupParams, sizeof(eomSetupParams)));

    // Set EOM disabled
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_RX_EOM_EN_OFF, numLanes);

    UINT32 doneStatus = 0x0;
    auto pollEomDone = [&]()->bool
        {
            for (UINT32 lane = 0; lane < numLanes; lane++)
            {
                if (!regs.Test32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_4_RX_EOM_DONE, lane, doneStatus))
                {
                    return false;
                }
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

    return OK;
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoGetFomStatus(vector<PerLaneFomData> * pFomData)
{
    MASSERT(pFomData);
    pFomData->clear();
    pFomData->resize(GetMaxLinks());

    RC rc;
    for (UINT32 lwrLink = 0; lwrLink < GetMaxLinks(); lwrLink++)
    {
        if (!IsLinkActive(lwrLink))
            continue;

        LW2080_CTRL_CMD_LWLINK_GET_LINK_FOM_VALUES_PARAMS fomParams = { };
        fomParams.linkId = lwrLink;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                               LW2080_CTRL_CMD_LWLINK_GET_LINK_FOM_VALUES,
                                               &fomParams,
                                               sizeof(fomParams)));

        if (fomParams.numLanes == 0)
            continue;

        // Only the lower 8 bits are actually fom values, the upper 8 bits are
        // status flags
        for (UINT08 lwrLane = 0; lwrLane < fomParams.numLanes; lwrLane++)
            pFomData->at(lwrLink).push_back(fomParams.figureOfMeritValues[lwrLane] & 0xFF);
    }

    return rc;
}

//------------------------------------------------------------------------------
UINT32 AmpereLwLink::DoGetLinkDataRateKiBps(UINT32 linkId) const
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return UNKNOWN_RATE;
    }

    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
    RC rc = LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                          LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                                          &statusParams,
                                          sizeof(statusParams));
    if (rc != RC::OK)
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Failed to get LwLink status from RM - %s\n", rc.Message());
        return UNKNOWN_RATE;
    }

    return statusParams.linkInfo[linkId].lwlinkLinkDataRateKiBps;
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    MASSERT(pErrorCounts);
    RC rc;

    CHECK_RC(VoltaLwLink::DoGetErrorCounts(linkId, pErrorCounts));

    bool bEccEnabled = false;
    CHECK_RC(IsPerLaneEccEnabled(linkId, &bEccEnabled));

    if (bEccEnabled)
    {
        LW2080_CTRL_LWLINK_GET_LWLINK_ECC_ERRORS_PARAMS eccParams = { };
        eccParams.linkMask = 1 << linkId;
        CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                               LW2080_CTRL_CMD_LWLINK_GET_LWLINK_ECC_ERRORS,
                                               &eccParams,
                                               sizeof(eccParams)));

        LW2080_CTRL_LWLINK_LINK_ECC_ERROR & linkErrors = eccParams.errorLink[linkId];
        LwU32 maxErrors = 0;
        bool  bAnyEccLaneCountFound = false;
        bool  bAnyEccLaneOverflowed = false;
        for (UINT32 lane = 0; lane < LW2080_CTRL_LWLINK_MAX_LANES; lane++)
        {
            if (!linkErrors.errorLane[lane].bValid)
                continue;

            bAnyEccLaneCountFound = true;

            if (linkErrors.errorLane[lane].eccErrorValue > maxErrors)
                maxErrors = linkErrors.errorLane[lane].eccErrorValue;
            if (linkErrors.errorLane[lane].overflowed == LW_TRUE)
                bAnyEccLaneOverflowed = true;

            LwLink::ErrorCounts::ErrId errId =
                static_cast<LwLink::ErrorCounts::ErrId>(LwLink::ErrorCounts::LWL_FEC_CORRECTIONS_LANE0_ID + lane);
            CHECK_RC(pErrorCounts->SetCount(errId, linkErrors.errorLane[lane].eccErrorValue,
                                            (linkErrors.errorLane[lane].overflowed == LW_TRUE)));
        }
        if (bAnyEccLaneCountFound)
        {
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
RC AmpereLwLink::DoGetErrorFlags(LwLink::ErrorFlags * pErrorFlags)
{
    return VoltaLwLink::GetErrorFlagsInternal(s_ErrorFlags, pErrorFlags);
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoInitialize(LibDevHandles handles)
{
    RC rc;
    CHECK_RC(TuringLwLink::DoInitialize(handles));

    // FLA implementation in RM changes in a way that is not supported in MFG MODS
    // when LW_VERIF_FEATURES is enabled
#ifdef LW_VERIF_FEATURES
    m_bFlaEnabled = false;
#else
    CHECK_RC(GetGpuSubdevice()->GetFlaCapable(&m_bFlaEnabled));
#endif
    return rc;
}

//------------------------------------------------------------------------------
bool AmpereLwLink::DoIsLinkAcCoupled(UINT32 linkId) const
{
    if (!IsLinkActive(linkId))
        return false;
    return Regs().Test32(linkId, MODS_LWLDL_TOP_LINK_CONFIG_AC_SAFE_EN_ON);
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoIsPerLaneEccEnabled(UINT32 linkId, bool *pbPerLaneEccEnabled) const
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
UINT32 AmpereLwLink::DoGetLinksPerGroup() const
{
    return Regs().LookupAddress(MODS_LWL_LWLIPT_NUM_LINKS);
}

//------------------------------------------------------------------------------
UINT32 AmpereLwLink::DoGetMaxLinkDataRateKiBps(UINT32 linkId) const
{
    if (!IsLinkValid(linkId))
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return UNKNOWN_RATE;
    }

    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS statusParams = {};
    RC rc = LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                          LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                                          &statusParams,
                                          sizeof(statusParams));
    if (rc != RC::OK)
    {
        Printf(Tee::PriWarn, GetTeeModule()->GetCode(),
               "Failed to get LwLink status from RM - %s\n", rc.Message());
        return UNKNOWN_RATE;
    }

    auto linkInfo = statusParams.linkInfo[linkId];
    if ((linkInfo.rxSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_SINGLE_LANE) ||
        (linkInfo.txSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_TX_STATE_SINGLE_LANE))
    {
        return linkInfo.lwlinkLinkDataRateKiBps * GetSublinkWidth(linkId);
    }
    return linkInfo.lwlinkLinkDataRateKiBps;
}

//------------------------------------------------------------------------------
UINT32 AmpereLwLink::DoGetMaxLinkGroups() const
{
    return Regs().LookupAddress(MODS_SCAL_LITTER_NUM_IOCTRL);
}

//------------------------------------------------------------------------------
FLOAT64 AmpereLwLink::DoGetMaxObservableBwPct(LwLink::TransferType tt)
{
    FrameBuffer * pFb = GetGpuSubdevice()->GetFB();

    if (pFb == nullptr)
        return BW_PCT_MAX;

    FLOAT64 bwPct = BW_PCT_MAX;

    if (false
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
        || (GetDevice()->GetDeviceId() == Device::GA100)
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
        || (GetDevice()->GetDeviceId() == Device::GH100)
#endif
       )
    {
        const UINT32 hbmSiteCount = pFb->GetFbpCount() / pFb->GetFbpsPerHbmSite();

        // At the moment characterization of write bandwidth on GA100 is unknown, use a lower
        // observable percentage
        if ((tt == TT_UNIDIR_WRITE) || (tt == TT_BIDIR_WRITE) || (tt == TT_READ_WRITE))
        {
            bwPct = BW_PCT_WRITE_GA100;
        }
        else if (hbmSiteCount < 5)
        {
            bwPct = min(bwPct, (hbmSiteCount == 4) ? BW_PCT_4_SITE : BW_PCT_UNSUPPORTED_CFG);

            // 4-site parts are now POR and to avoid having lwstomers report issues
            // from lwqual due to this print, lower the priority on 4 site parts
            Printf((hbmSiteCount == 4) ? Tee::PriLow : Tee::PriNormal,
                   "%s : Unsupported HBM configuration (%u sites).\n"
                   "%s   Unable to determine observable bandwidth percent, using %2.1f%%!\n",
                   GetGpuSubdevice()->GpuIdentStr().c_str(),
                   hbmSiteCount,
                   string(GetGpuSubdevice()->GpuIdentStr().size(), ' ').c_str(),
                   bwPct * 100.0);
        }
        return bwPct;
    }
    else
    // For non-GA100 it is diffilwlt to estimate the effect of collapsed responses
    // on write bandwidth, lwrrently MODS assumes 16:1 (could be up to 64:1) but it
    // could also be far less - but still not as bad as on GA100
    if (HasCollapsedResponses() &&
        ((tt == TT_UNIDIR_WRITE) || (tt == TT_BIDIR_WRITE) || (tt == TT_READ_WRITE)))
    {
        bwPct = BW_PCT_WRITE_COLLAPSED_RSP;
    }

    return bwPct;
}


//------------------------------------------------------------------------------
//! \brief Get whether per lane counts are enabled
//!
//! \param linkId : Link ID to get per-lane error count enable state
//! \param pbPerLaneEnabled : Return status of per lane enabled
//!
//! \return true if per lane counts are enabled, false otherwise
/* virtual */ RC AmpereLwLink::DoPerLaneErrorCountsEnabled
(
    UINT32 linkId,
    bool *pbPerLaneEnabled
)
{
    MASSERT(pbPerLaneEnabled);
    *pbPerLaneEnabled = false;

    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return RC::BAD_PARAMETER;;
    }

    *pbPerLaneEnabled = Regs().Read32(linkId, MODS_LWLDL_TX_TXLANECRC_ENABLE) ? true : false;

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC AmpereLwLink::DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode)
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
    if (!regs.HasReadAccess(linkId, MODS_LWLDL_TOP_LINK_CONFIG))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }
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

//------------------------------------------------------------------------------
/* virtual */ RC AmpereLwLink::DoGetBlockCodeMode
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
RC AmpereLwLink::GetEomConfigValue
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
            eyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL;
            break;
        case EYEO_Y_L:
            modeVal = LW_MINION_UCODE_CONFIGEOM_EOMMODE_Y;
            spareSel = LW_MINION_UCODE_CONFIGEOM_EOM_SPARE_SEL_SPARE_N;
            eyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL;
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

//------------------------------------------------------------------------------
void AmpereLwLink::DoSetCeCopyWidth(CeTarget target, CeWidth width)
{
    ModsGpuRegValue wrValue = MODS_REGISTER_VALUE_NULL;
    if (target == CET_SYSMEM)
    {
        if (width == CEW_256_BYTE)
        {
            wrValue = MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_SYSMEM_EN_ON;
        }
        else
        {
            wrValue = MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_SYSMEM_EN_OFF;
        }
    }
    else if (target == CET_PEERMEM)
    {
        if (width == CEW_256_BYTE)
        {
            wrValue = MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_PEERMEM_EN_ON;
        }
        else
        {
            wrValue = MODS_PFB_HSHUB_IG_CE2HSH_CFG_LWL_256B_PEERMEM_EN_OFF;
        }
    }
    MASSERT(wrValue != MODS_REGISTER_VALUE_NULL);
    RegHal &regs = Regs();
    const UINT32 hshubSize = regs.LookupArraySize(wrValue, 1);
    for (UINT32 hshubIdx = 0; hshubIdx < hshubSize; hshubIdx++)
        regs.Write32(wrValue, hshubIdx);
}

//-----------------------------------------------------------------------------
/* virtual */ bool AmpereLwLink::DoSupportsFomMode(FomMode mode) const
{
    switch (mode)
    {
        case EYEO_Y:
        case EYEO_Y_U:
        case EYEO_Y_L:
            return true;
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
/* virtual */ RC AmpereLwLink::DoGetLinkPowerStateStatus
(
    UINT32 linkId,
    LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
) const
{
    MASSERT(pLinkPowerStatus);

    RC rc;

    const RegHal& regHal = Regs();

    pLinkPowerStatus->rxHwControlledPowerState =
        0 == regHal.Read32(linkId, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE);
    pLinkPowerStatus->rxSubLinkLwrrentPowerState =
        regHal.Test32(linkId, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_LWRRENTSTATE_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->rxSubLinkConfigPowerState =
        regHal.Test32(linkId, MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txHwControlledPowerState =
        0 == regHal.Read32(linkId, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE);
    pLinkPowerStatus->txSubLinkLwrrentPowerState =
        regHal.Test32(linkId, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_LWRRENTSTATE_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txSubLinkConfigPowerState =
        regHal.Test32(linkId, MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB)
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC AmpereLwLink::DoRequestPowerState
(
    UINT32 linkId
   ,LwLinkPowerState::SubLinkPowerState state
   ,bool bHwControlled
)
{
    MASSERT(LwLinkPowerState::SLS_PWRM_ILWALID != state);
    RC rc;

    CHECK_RC(CheckPowerStateAvailable(state, bHwControlled));
    RegHal& regHal = Regs();

    const vector<LwLink::SubLinkDir> dirs = {LwLink::RX, LwLink::TX};
    for (auto dir : dirs)
    {
        const auto pwrmIcSwCtrlRegs = RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL :
                                                MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL;
        UINT32 pwrmIcSwCtrlRegValue = 0;
    
        const auto fb = RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB :
                                    MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB;
        const auto lp = RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP :
                                    MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP;
        const auto swDesiredVal = LwLinkPowerState::SLS_PWRM_FB == state ? fb : lp;
    
        const auto hwDisField = RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE :
                                            MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_HARDWAREDISABLE;
    
        regHal.SetField(&pwrmIcSwCtrlRegValue, swDesiredVal);
        regHal.SetField(&pwrmIcSwCtrlRegValue, hwDisField, bHwControlled ? 0 : 1);
        if (bHwControlled && LwLinkPowerState::SLS_PWRM_LP == state)
        {
            // set the bit allowing idle counting if the HW is allowed to change the power state,
            // i.e. when true == (SOFTWAREDESIRED == 1 (LP) && HARDWAREDISABLE == 0)
            const auto idleCountStart = RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_SW_CTRL_COUNTSTART :
                                                    MODS_LWLTLC_TX_LNK_PWRM_IC_SW_CTRL_COUNTSTART;
            regHal.SetField(&pwrmIcSwCtrlRegValue, idleCountStart, 1);
        }
    
        // Set both HW disable and SW desired bits at once due to a bug in fmodel
        // that increments LP entry counter even if we write LP to SW desired power
        // state bit that already was set to LP.
        regHal.Write32(linkId, pwrmIcSwCtrlRegs, pwrmIcSwCtrlRegValue);
    }

    return rc;
}

//------------------------------------------------------------------------------
UINT32 AmpereLwLink::DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
{
    if (!IsLinkActive(linkId))
        return 0;

    const auto pwrmIcLpEnterThresholdReg =
        RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD:
                    MODS_LWLTLC_TX_LNK_PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD;
    const auto pwrmIcIncLpIncReg = RX == dir ? MODS_LWLTLC_RX_LNK_PWRM_IC_INC_LPINC :
                                               MODS_LWLTLC_TX_LNK_PWRM_IC_INC_LPINC;
    const RegHal& regs = Regs();

    const FLOAT64 linkClock = GetLinkClockRateMhz(linkId);
    const FLOAT64 threshold = regs.Read32(linkId, pwrmIcLpEnterThresholdReg);
    const FLOAT64 lpInc = regs.Read32(linkId, pwrmIcIncLpIncReg);

    return static_cast<UINT32>(threshold / linkClock / 1000.0 / lpInc);
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
)
{
    return VoltaLwLink::DoGetLPEntryOrExitCount(linkId, entry, pCount, pbOverflow);
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoClearLPCounts(UINT32 linkId)
{
    return VoltaLwLink::DoClearLPCounts(linkId);
}

//------------------------------------------------------------------------------
bool AmpereLwLink::HasLPCountOverflowed(bool bEntry, UINT32 lpCount) const
{
    auto cntReg = bEntry ? MODS_LWLDL_TOP_STATS_D_NUM_TX_LP_ENTER :
                           MODS_LWLDL_TOP_STATS_D_NUM_TX_LP_EXIT;
    return (lpCount == Regs().LookupMask(cntReg));
}

//------------------------------------------------------------------------------
bool AmpereLwLink::DoGetFlaCapable()
{
    if (!GpuPtr()->IsRmInitCompleted() || m_bFlaCapableInit)
        return m_bFlaCapable;

    LwRmPtr pLwRm;
    LW2080_CTRL_GPU_GET_INFO_V2_PARAMS gpuInfo = { };
    gpuInfo.gpuInfoListSize = 1;
    gpuInfo.gpuInfoList[0].index = LW2080_CTRL_GPU_INFO_INDEX_GPU_FLA_CAPABILITY;
    if (OK == pLwRm->ControlBySubdevice(GetGpuSubdevice(),
                                        LW2080_CTRL_CMD_GPU_GET_INFO_V2,
                                        &gpuInfo,
                                        sizeof(gpuInfo)))
    {
        m_bFlaCapable =
            (gpuInfo.gpuInfoList[0].data == LW2080_CTRL_GPU_INFO_INDEX_GPU_FLA_CAPABILITY_YES);
        m_bFlaCapableInit = true;
    }

    return m_bFlaCapable;
}

//------------------------------------------------------------------------------
bool AmpereLwLink::DoGetFlaEnabled()
{
    return m_bFlaEnabled;
}

//------------------------------------------------------------------------------
LwLink::SystemLineRate AmpereLwLink::DoGetSystemLineRate(UINT32 linkId) const
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
RC AmpereLwLink::DoGetXbarBandwidth
(
    bool bWriting,
    bool bReading,
    bool bWrittenTo,
    bool bReadFrom,
    UINT32 *pXbarBwKiBps
)
{
    MASSERT(pXbarBwKiBps);

    GpuSubdevice* pGpuSubdev = GetGpuSubdevice();
    const UINT32 fbpCount = pGpuSubdev->GetFB()->GetFbpCount();

    // XBAR bandwidth is not a limiting factor on 0FB or on non-GA100
    if (
        (fbpCount == 0)
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
        || (GetDevice()->GetDeviceId() != Device::GA100)
#endif
       )
    {
        *pXbarBwKiBps = 0U;
        return RC::OK;
    }

    UINT32 xbarPackets = 0;
    // These numbers were provided in http://lwbugs/2710075/44 and are GA100 specific
    if (bWriting && bWrittenTo)
        xbarPackets = 11;
    else if (bReading && bWriting)
        xbarPackets = 10;
    else if (bReading && bReadFrom)
        xbarPackets = 9;
    else if ((bWriting && !bReadFrom) || (bReading && !bWrittenTo))
        xbarPackets = 8;
    else
    {
        MASSERT(!"Invalid GPU access pattern\n");
        Printf(Tee::PriError, "Invalid GPU access pattern\n");
        return RC::SOFTWARE_ERROR;
    }

    RC rc;
    UINT64 xbarClkHz;
    CHECK_RC(pGpuSubdev->GetClock(Gpu::ClkXbar, &xbarClkHz, nullptr, nullptr, nullptr));

    // XbarBandwidth = 256 / packets * clock * fbpcount / 1024 
    // (divide by 1024 is to colwert to KiBps), perform the callwlation in float
    // and reorder to reduce rounding effects;
    FLOAT64 fbw = xbarClkHz  * 256.0  * fbpCount / xbarPackets / 1024.0;

    // XBAR can also have a 10% ARB inefficiency so drop the bandwidth by 10%
    *pXbarBwKiBps = static_cast<UINT32>(fbw * 0.9);
    return RC::OK;
}

//------------------------------------------------------------------------------
bool AmpereLwLink::DoHasCollapsedResponses() const
{
    return Regs().Test32(MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_DISABLED);
}

//------------------------------------------------------------------------------
RC AmpereLwLink::DoSetCollapsedResponses(UINT32 mask)
{
    RC rc;
    RegHal& regs = Regs();

    // bit 0 is RX, bit 1 is TX
    const ModsGpuRegValue rxValue = (mask & 0x1) ?
        MODS_LWLTLC_RX_LNK_CTRL_LINK_CONFIG_ALLOW_COLLAPSED_RESPONSES_ENABLED :
        MODS_LWLTLC_RX_LNK_CTRL_LINK_CONFIG_ALLOW_COLLAPSED_RESPONSES_DISABLED;

    ModsGpuRegValue txValue;
    txValue = (mask & 0x2) ?
        MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_DISABLED :
        MODS_LWLTLC_TX_LNK_CTRL_LINK_CONFIG_COLLAPSED_RESPONSE_DISABLE_ENABLED;

    for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
    {
        regs.Write32(linkId, rxValue);
        regs.Write32(linkId, txValue);
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC AmpereLwLink::DoSetSystemParameter
(
    UINT64 linkMask,
    SystemParameter parm,
    SystemParamValue val
)
{
    RegHal & regs = GetDevice()->Regs();
    INT32 maxLink = Utility::BitScanReverse(linkMask);
    if (maxLink == -1)
    {
        Printf(Tee::PriError, "%s : No links specified\n", __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    if (static_cast<UINT32>(maxLink) >= regs.LookupAddress(MODS_SCAL_LITTER_NUM_LWLINK))
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

    // At the time that this is called it is likely that the lwlink hardware will
    // be in reset and therefore registers will not be accessible so MODS needs to bring
    // the links out of reset
    CHECK_RC(GetGpuSubdevice()->EnableHwDevice(GpuSubdevice::HwDevType::HDT_IOCTRL, true));

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

//------------------------------------------------------------------------------
// LwLinkUphy
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
RC AmpereLwLink::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
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
RC AmpereLwLink::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
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
//! \brief Get the base for the register domain and linkId
//!
//! \param domain : Domain to get the base for
//! \param linkId : Link Id to get the base for
//!
//! \return Base for the register domain / link if one exists,
//!         ILWALID_DOMAIN_BASE if not
UINT32 AmpereLwLink::GetDomainBase(RegHalDomain domain, UINT32 linkId)
{
    if (linkId >= GetMaxLinks())
    {
        Printf(Tee::PriError, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return ILWALID_DOMAIN_BASE;
    }
    return GetGpuSubdevice()->GetDomainBase(linkId, domain, nullptr);
}

//------------------------------------------------------------------------------
UINT32 AmpereLwLink::GetErrorCounterControlMask(bool bIncludePerLane)
{
    UINT32 counterMask = LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT    |
                         LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_REPLAY      |
                         LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_RECOVERY    |
                         LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_REPLAY;
    if (bIncludePerLane)
    {
        counterMask |= LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L2 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L3;
    }
    return counterMask;
}

//------------------------------------------------------------------------------
void AmpereLwLink::LoadLwLinkCaps()
{
    TuringLwLink::LoadLwLinkCaps();

    // Ampere is the first chip to support lwlink floorsweeping, rather than use the
    // RM discovered links, use the defines from hwproject.h so that all 12 links will
    // be printed in the lwlink summaries with missing links reporting unconnected
    //
    // This function is called before m_LwLinkRegs has been initialized so needs to
    // explicitly access GpuSubdevice::Regs which is acceptable since it is only doing
    // a LookupAddress of a non-lwlink specific register
    SetMaxLinks(GetGpuSubdevice()->Regs().LookupAddress(MODS_SCAL_LITTER_NUM_LWLINK));
}
