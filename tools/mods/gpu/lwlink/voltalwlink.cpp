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

#include <limits>

#include <boost/range/algorithm/find_if.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/repetition/enum.hpp>
#include <boost/preprocessor/seq/elem.hpp>

#include "voltalwlink.h"
#include "gpu/include/gpusbdev.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghaltable.h"
#include "core/include/utility.h"
#include "core/include/tee.h"
#include "ctrl/ctrl2080.h"
#include "core/include/platform.h"

 // LWPU SDK
#include "ctrl/ctrl90cc.h"

// Expands to MODS_ ## d[0] ## n ## d[1]. BOOST_PP_CAT is used instead of ##,
// because ## doesn't work with other macros.
#define PREFIX_N_SUFFIX_CAT(z, n, d)     \
    BOOST_PP_CAT(                        \
        MODS_,                           \
        BOOST_PP_CAT(                    \
            BOOST_PP_CAT(                \
                BOOST_PP_SEQ_ELEM(0, d), \
                n                        \
            ),                           \
            BOOST_PP_SEQ_ELEM(1, d)      \
        )                                \
    )

// Expands to
// constexpr ModsGpuReg ## Type Name[] =
// {
//     Prefix0Suffix, Prefix1Suffix, Prefix2Suffix
//     Prefix3Suffix, Prefix4Suffix, Prefix5Suffix
// };
#define PER_LINK_ARRAY(Type, Name, Prefix, Suffix)              \
    constexpr ModsGpuReg ## Type Name[] =                       \
    {                                                           \
        BOOST_PP_ENUM(6, PREFIX_N_SUFFIX_CAT, (Prefix)(Suffix)) \
    }

namespace
{
    PER_LINK_ARRAY(Value,   s_RxLwrrentStateFB,    PLWLTLC_RX, _PWRM_IC_SW_CTRL_LWRRENTSTATE_FB);
    PER_LINK_ARRAY(Value,   s_TxLwrrentStateFB,    PLWLTLC_TX, _PWRM_IC_SW_CTRL_LWRRENTSTATE_FB);
    PER_LINK_ARRAY(Field,   s_RxHwLPDisable,       PLWLTLC_RX, _PWRM_IC_SW_CTRL_HARDWAREDISABLE);
    PER_LINK_ARRAY(Field,   s_TxHwLPDisable,       PLWLTLC_TX, _PWRM_IC_SW_CTRL_HARDWAREDISABLE);
    PER_LINK_ARRAY(Field,   s_RxIdleCountStart,    PLWLTLC_RX, _PWRM_IC_SW_CTRL_COUNTSTART);
    PER_LINK_ARRAY(Field,   s_TxIdleCountStart,    PLWLTLC_TX, _PWRM_IC_SW_CTRL_COUNTSTART);
    PER_LINK_ARRAY(Value,   s_RxSoftwareDesiredFB, PLWLTLC_RX, _PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB);
    PER_LINK_ARRAY(Value,   s_TxSoftwareDesiredFB, PLWLTLC_TX, _PWRM_IC_SW_CTRL_SOFTWAREDESIRED_FB);
    PER_LINK_ARRAY(Value,   s_RxSoftwareDesiredLP, PLWLTLC_RX, _PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP);
    PER_LINK_ARRAY(Value,   s_TxSoftwareDesiredLP, PLWLTLC_TX, _PWRM_IC_SW_CTRL_SOFTWAREDESIRED_LP);
    PER_LINK_ARRAY(Address, s_rxPwrmIcSwCtrl,      PLWLTLC_RX, _PWRM_IC_SW_CTRL);
    PER_LINK_ARRAY(Address, s_txPwrmIcSwCtrl,      PLWLTLC_TX, _PWRM_IC_SW_CTRL);
    PER_LINK_ARRAY(Field,   s_RxPwrmIcLpEnterThreshold, PLWLTLC_RX, _PWRM_IC_INC_LPINC);
    PER_LINK_ARRAY(Field,   s_TxPwrmIcLpEnterThreshold, PLWLTLC_TX, _PWRM_IC_INC_LPINC);
    PER_LINK_ARRAY(Field,   s_RxPwrmIcIncLpInc,         PLWLTLC_RX, _PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD);
    PER_LINK_ARRAY(Field,   s_TxPwrmIcIncLpInc,         PLWLTLC_TX, _PWRM_IC_LP_ENTER_THRESHOLD_THRESHOLD);
    PER_LINK_ARRAY(Value,   s_AcSafeEnabledOn,          PLWL,       _LINK_CONFIG_AC_SAFE_EN_ON);

    struct OnOffRegField
    {
        ModsGpuRegValue on;
        ModsGpuRegValue off;
    };
    #define ON_OFF_FIELD(field)\
    {\
        field##_ON,\
        field##_OFF\
    }

    struct EyeDiagramRegs
    {
        OnOffRegField   lwlRxEomEn;
        OnOffRegField   lwlRxEomOvrd;
        ModsGpuRegField   lwlRxEomDone;
        ModsGpuRegField   lwlRxEomStatus;
    };

    const EyeDiagramRegs s_EyeDiagramRegs[] =
    {
        {
            ON_OFF_FIELD(MODS_PLWL0_BR0_PAD_CTL_8_RX_EOM_EN)
           ,ON_OFF_FIELD(MODS_PLWL0_BR0_PAD_CTL_8_RX_EOM_OVRD)
           ,MODS_PLWL0_BR0_PAD_CTL_4_RX_EOM_DONE
           ,MODS_PLWL0_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
            ON_OFF_FIELD(MODS_PLWL1_BR0_PAD_CTL_8_RX_EOM_EN)
           ,ON_OFF_FIELD(MODS_PLWL1_BR0_PAD_CTL_8_RX_EOM_OVRD)
           ,MODS_PLWL1_BR0_PAD_CTL_4_RX_EOM_DONE
           ,MODS_PLWL1_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
            ON_OFF_FIELD(MODS_PLWL2_BR0_PAD_CTL_8_RX_EOM_EN)
           ,ON_OFF_FIELD(MODS_PLWL2_BR0_PAD_CTL_8_RX_EOM_OVRD)
           ,MODS_PLWL2_BR0_PAD_CTL_4_RX_EOM_DONE
           ,MODS_PLWL2_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
            ON_OFF_FIELD(MODS_PLWL3_BR0_PAD_CTL_8_RX_EOM_EN)
           ,ON_OFF_FIELD(MODS_PLWL3_BR0_PAD_CTL_8_RX_EOM_OVRD)
           ,MODS_PLWL3_BR0_PAD_CTL_4_RX_EOM_DONE
           ,MODS_PLWL3_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
            ON_OFF_FIELD(MODS_PLWL4_BR0_PAD_CTL_8_RX_EOM_EN)
           ,ON_OFF_FIELD(MODS_PLWL4_BR0_PAD_CTL_8_RX_EOM_OVRD)
           ,MODS_PLWL4_BR0_PAD_CTL_4_RX_EOM_DONE
           ,MODS_PLWL4_BR0_PAD_CTL_4_RX_EOM_STATUS
        },
        {
            ON_OFF_FIELD(MODS_PLWL5_BR0_PAD_CTL_8_RX_EOM_EN)
           ,ON_OFF_FIELD(MODS_PLWL5_BR0_PAD_CTL_8_RX_EOM_OVRD)
           ,MODS_PLWL5_BR0_PAD_CTL_4_RX_EOM_DONE
           ,MODS_PLWL5_BR0_PAD_CTL_4_RX_EOM_STATUS
        }
    };

    // DRF definitions for LW2080_CTRL_CMD_LWLINK_SETUP_EOM_PARAMS
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
RC VoltaLwLink::DoDetectTopology()
{
    RC rc;
    CHECK_RC(PascalLwLink::DoDetectTopology());
    SetRegLogLinkMask(GetMaxLinkMask());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the EOM experiment and return the status value for each lane
//!        on this link
//!
//! \param mode      : The mode to run the EOM in, eg. EYEO_X
//! \param type      : The type of EOM to run, eg. ODDEVEN
//! \param link      : The individual link to run the EOM on
//! \param timeoutMs : How long to wait for the EOM to finish
//! \param status    : Output array that will contain the status for each lane
//!
//! \return OK if successful, not OK otherwise
//!
RC VoltaLwLink::DoGetEomStatus(FomMode mode,
                               UINT32 link,
                               UINT32 numErrors,
                               UINT32 numBlocks,
                               FLOAT64 timeoutMs,
                               vector<INT32>* status)
{
    MASSERT(status);
    RC rc;

    GpuSubdevice* pGpuSubdev = GetGpuSubdevice();
    RegHal& regs = Regs();

    if (link > NUMELEMS(s_EyeDiagramRegs))
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
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomEn.off, numLanes);

    UINT32 doneStatus = 0x0;
    auto pollEomDone = [&]()->bool
        {
            for (UINT32 lane = 0; lane < numLanes; lane++)
            {
                if (!regs.Test32(s_EyeDiagramRegs[link].lwlRxEomDone, lane, doneStatus))
                    return false;
            }
            return true;
        };

    // Check that EOM_DONE is reset
    CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));

    // Set EOM programmable
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomOvrd.on, numLanes);

    // Set EOM enabled
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomEn.on, numLanes);

    // Check if EOM is done
    doneStatus = 0x1;
    CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));

    // Set EOM disabled
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomEn.off, numLanes);

    // Set EOM not programmable
    regs.Write32(s_EyeDiagramRegs[link].lwlRxEomOvrd.off, numLanes);

    // Get EOM status
    for (UINT32 i = 0; i < numLanes; i++)
    {
        status->at(i) = static_cast<INT16>(regs.Read32(s_EyeDiagramRegs[link].lwlRxEomStatus, i));
    }

    return OK;
}

/* static */ const VoltaLwLink::ErrorFlagDefinition VoltaLwLink::s_ErrorFlags =
{
    {
        TLCRX0,
        {
            { MODS_LWLTLC_RX_ERR_STATUS_0_DATLENGTATOMICREQMAXERR,           "Receive DatLen is greater than the Atomic Max size (128B, 64B for CAS) Error" }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_DATLENGTRMWREQMAXERR,              "Receive DatLen is greater than the RMW Max size (64B) Error"                  }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_DATLENLTATRRSPMINERR,              "Receive DatLen is less than the ATR response size (8B) Error"                 }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_ILWALIDCACHEATTRPOERR,             "Receive The CacheAttr field and PO field do not agree Error"                  }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_ILWALIDCRERR,                      "Receive Invalid compressed response Error"                                    }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RSVADDRTYPEERR,                    "Receive Reserved Address Type Encoding Error"                                 }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RSVCACHEATTRPROBEREQERR,           "Receive Reserved Cache Attribute Encoding in Probe Request Error"             }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RSVCACHEATTRPROBERSPERR,           "Receive Reserved Cache Attribute Encoding in Probe Response Error"            }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RSVCMDENCERR,                      "Receive Reserved Command Encoding Error"                                      }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RSVDATLENENCERR,                   "Receive Reserved Data Length Encoding Error"                                  }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RSVPKTSTATUSERR,                   "Receive Reserved Packet Status Encoding Error"                                }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RSVRSPSTATUSERR,                   "Receive Reserved RspStatus Encoding Error"                                    }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXDLCTRLPARITYERR,                 "Receive DL Control Parity Error"                                              }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXDLDATAPARITYERR,                 "Receive DL Data Parity Error"                                                 }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXDLHDRPARITYERR,                  "Receive DL Header Parity Error"                                               }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXILWALIDADDRALIGNERR,             "Receive Invalid Address Alignment Error"                                      }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXILWALIDAEERR,                    "Receive Invalid AE Flit Received Error"                                       }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXILWALIDBEERR,                    "Receive Invalid BE Flit Received Error"                                       }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXPKTLENERR,                       "Receive Packet Length Error"                                                  }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXRAMDATAPARITYERR,                "Receive RAM Data Parity Error"                                                }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXRAMHDRPARITYERR,                 "Receive RAM Header Parity Error"                                              }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXRESPSTATUSTARGETERR,             "Receive Receive TE Error in the RspStatus field"                              }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_0_RXRESPSTATUSUNSUPPORTEDREQUESTERR, "Receive UR Error in the RspStatus field"                                      }  //$
        }
    },
    {
        TLCRX1,
        {
            { MODS_LWLTLC_RX_ERR_STATUS_1_CORRECTABLEINTERNALERR,    "Receive Correctable Internal Error"              }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_1_RXDATAOVFERR,              "Receive Data Overflow Error"                     }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_1_RXHDROVFERR,               "Receive Header Overflow Error"                   }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_1_RXPOISONERR,               "Receive Data Poisoned Packet Received Error"     }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_1_RXUNSUPNCISOCCREDITRELERR, "Receive Unsupported NCISOC Credit Release Error" }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_1_RXUNSUPLWLINKCREDITRELERR, "Receive Unsupported LWLink Credit Release Error" }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_1_RXUNSUPVCOVFERR,           "Receive Unsupported VC Overflow Error"           }, //$
            { MODS_LWLTLC_RX_ERR_STATUS_1_STOMPDETERR,               "Receive Stomped Packet Received Error"           }  //$
        }
    },
    {
        TLCTX0,
        {
            { MODS_LWLTLC_TX_ERR_STATUS_0_TARGETERR,             "Transmit Target Error detected in RspStatus"                              }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXDATACREDITOVFERR,    "Transmit Data Credit Overflow Error"                                      }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXDLCREDITOVFERR,      "Transmit DL Replay Credit Overflow Error"                                 }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXDLCREDITPARITYERR,   "Transmit DL Flow Control Interface Parity Error"                          }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXHDRCREDITOVFERR,     "Transmit Header Credit Overflow Error"                                    }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXPOISONDET,           "Transmit Data Poisoned Packet detected on transmit from NCISOC to LWLink" }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXRAMDATAPARITYERR,    "Transmit RAM Data Parity Error"                                           }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXRAMHDRPARITYERR,     "Transmit RAM Header Parity Error"                                         }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXSTOMPDET,            "Transmit Stomped Packet Detected on Transmit from NCISOC to LWLink"       }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_TXUNSUPVCOVFERR,       "Transmit Unsupported VC Overflow Error"                                   }, //$
            { MODS_LWLTLC_TX_ERR_STATUS_0_UNSUPPORTEDREQUESTERR, "Transmit Unsupported Request detected in RspStatus"                       }  //$
        }
    },
    {
        MIFRX0,
        {
            { MODS_IOCTRLMIF_RX_ERR_STATUS_0_RXRAMDATAPARITYERR, "MIF Receive RAM Data Parity Error"   }, //$
            { MODS_IOCTRLMIF_RX_ERR_STATUS_0_RXRAMHDRPARITYERR,  "MIF Receive RAM Header Parity Error" }  //$
        }
    },
    {
        MIFTX0,
        {
            { MODS_IOCTRLMIF_TX_ERR_STATUS_0_TXRAMDATAPARITYERR, "MIF Transmit RAM Data Parity Error"   }, //$
            { MODS_IOCTRLMIF_TX_ERR_STATUS_0_TXRAMHDRPARITYERR,  "MIF Transmit RAM Header Parity Error" }  //$
        }
    }
};

//------------------------------------------------------------------------------
RC VoltaLwLink::DoGetErrorFlags(ErrorFlags * pErrorFlags)
{
    return GetErrorFlagsInternal(s_ErrorFlags, pErrorFlags);
}

//-----------------------------------------------------------------------------
RC VoltaLwLink::DoInitializePostDiscovery()
{
    RC rc;
    CHECK_RC(PascalLwLink::DoInitializePostDiscovery());
    SetRegLogLinkMask(GetMaxLinkMask());
    return rc;
}

//------------------------------------------------------------------------------
bool VoltaLwLink::DoIsLinkAcCoupled(UINT32 linkId) const
{
    if (!IsLinkActive(linkId))
        return false;
    return Regs().Test32(s_AcSafeEnabledOn[linkId]);
}

//------------------------------------------------------------------------------
//! \brief Get the current HW LwLink error flags
//!
//! \param flags       : Error flag definition
//! \param pErrorFlags : Pointer to returned error flags
//!
//! \return OK if successful, not OK otherwise
RC VoltaLwLink::GetErrorFlagsInternal
(
    const ErrorFlagDefinition & flags,
    LwLink::ErrorFlags        * pErrorFlags
)
{
    MASSERT(pErrorFlags);
    pErrorFlags->linkErrorFlags.clear();
    pErrorFlags->linkGroupErrorFlags.clear();

    // Flag registers are not implemented on simulation platforms
    if (Platform::GetSimulationMode() != Platform::Hardware)
        return OK;

    RC rc;
    RegHal& regs = Regs();

    LW2080_CTRL_LWLINK_GET_ERR_INFO_PARAMS flagParams = {};
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_ERR_INFO,
                                           &flagParams,
                                           sizeof(flagParams)));

    for (INT32 lwrLink = Utility::BitScanForward(flagParams.linkMask, 0);
         lwrLink != -1;
         lwrLink = Utility::BitScanForward(flagParams.linkMask, lwrLink + 1))
    {
        LW2080_CTRL_LWLINK_ERR_INFO& linkErrInfo = flagParams.linkErrInfo[lwrLink];
        set<string> newFlags;

        for (const auto &errFlagBlock : flags)
        {
            // Lwlipt errors are per link group rather than per link
            if (errFlagBlock.first == LWLIPT)
                continue;

            UINT32 errFlagData = 0;
            switch (errFlagBlock.first)
            {
                case TLCRX0:      errFlagData = linkErrInfo.TLCRxErrStatus0;     break;
                case TLCRX1:      errFlagData = linkErrInfo.TLCRxErrStatus1;     break;
                case TLCRXSYS0:   errFlagData = linkErrInfo.TLCRxSysErrStatus0;  break;
                case TLCTX0:      errFlagData = linkErrInfo.TLCTxErrStatus0;     break;
                case TLCTX1:      errFlagData = linkErrInfo.TLCTxErrStatus1;     break;
                case TLCTXSYS0:   errFlagData = linkErrInfo.TLCTxSysErrStatus0;  break;
                case MIFRX0:      errFlagData = linkErrInfo.MIFRxErrStatus0;     break;
                case MIFTX0:      errFlagData = linkErrInfo.MIFTxErrStatus0;     break;
                case LWLIPT_LNK:  errFlagData = linkErrInfo.LWLIPTLnkErrStatus0; break;
                default:
                    Printf(Tee::PriError, "%s : Unknown Error Flag Type %d\n",
                           __FUNCTION__,
                           errFlagBlock.first);
                    return RC::SOFTWARE_ERROR;
            }
            for (const auto & errFlag : errFlagBlock.second)
            {
                if (regs.IsSupported(errFlag.first) && regs.GetField(errFlagData, errFlag.first))
                {
                    newFlags.insert(errFlag.second);
                }
            }
        }
        if (flagParams.linkErrInfo[lwrLink].bExcessErrorDL)
        {
            newFlags.insert("Excess DL Errors");
        }

        pErrorFlags->linkErrorFlags[lwrLink] = { newFlags.begin(), newFlags.end() };
    }


    for (INT32 lwrLinkGroup = Utility::BitScanForward(flagParams.ioctrlMask, 0);
         lwrLinkGroup != -1;
         lwrLinkGroup = Utility::BitScanForward(flagParams.ioctrlMask, lwrLinkGroup + 1))
    {
        LW2080_CTRL_LWLINK_COMMON_ERR_INFO& linkGroupErrInfo =
            flagParams.commonErrInfo[lwrLinkGroup];
        set<string> newFlags;

        for (const auto &errFlagBlock : flags)
        {
            // Only LWLIPT errors are per link group rather than per link
            if (errFlagBlock.first != LWLIPT)
                continue;

            for (const auto & errFlag : errFlagBlock.second)
            {
                if (regs.IsSupported(errFlag.first) &&
                    regs.GetField(linkGroupErrInfo.LWLIPTErrStatus0, errFlag.first))
                {
                    newFlags.insert(errFlag.second);
                }
            }
        }

        pErrorFlags->linkGroupErrorFlags[lwrLinkGroup] = { newFlags.begin(), newFlags.end() };
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Get the base for the register domain and linkId
//!
//! \param domain : Domain to get the base for
//! \param linkId : Link Id to get the base for
//!
//! \return Base for the register domain / link if one exists,
//!         ILWALID_DOMAIN_BASE if not
UINT32 VoltaLwLink::GetDomainBase(RegHalDomain domain, UINT32 linkId)
{
    static const ModsGpuRegAddress s_LwlBaseRegs[] =
    {
        MODS_PLWL0_LINK_STATE
        ,MODS_PLWL1_LINK_STATE
        ,MODS_PLWL2_LINK_STATE
        ,MODS_PLWL3_LINK_STATE
        ,MODS_PLWL4_LINK_STATE
        ,MODS_PLWL5_LINK_STATE
    };

    if (linkId >= NUMELEMS(s_LwlBaseRegs))
    {
        Printf(Tee::PriHigh, "%s : Invalid linkId %u\n", __FUNCTION__, linkId);
        return ILWALID_DOMAIN_BASE;
    }

    switch (domain)
    {
        case RegHalDomain::DLPL:
            return Regs().LookupAddress(s_LwlBaseRegs[linkId]);
        case RegHalDomain::RAW:
            return 0x0;
        default:
            Printf(Tee::PriError,
                   "%s : Invalid domain %u\n",
                   __FUNCTION__,
                   static_cast<UINT32>(domain));
            break;
    }
    return ILWALID_DOMAIN_BASE;
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaLwLink::DoGetLinkPowerStateStatus
(
    UINT32 linkId,
    LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
) const
{
    MASSERT(pLinkPowerStatus);

    RC rc;

    const RegHal& regHal = Regs();

    pLinkPowerStatus->rxHwControlledPowerState =
        0 == regHal.Read32(s_RxHwLPDisable[linkId]);
    pLinkPowerStatus->rxSubLinkLwrrentPowerState =
        regHal.Test32(s_RxLwrrentStateFB[linkId])
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->rxSubLinkConfigPowerState =
        regHal.Test32(s_RxSoftwareDesiredFB[linkId])
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txHwControlledPowerState =
        0 == regHal.Read32(s_TxHwLPDisable[linkId]);
    pLinkPowerStatus->txSubLinkLwrrentPowerState =
        regHal.Test32(s_TxLwrrentStateFB[linkId])
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;
    pLinkPowerStatus->txSubLinkConfigPowerState =
        regHal.Test32(s_TxSoftwareDesiredFB[linkId])
        ? LwLinkPowerState::SLS_PWRM_FB : LwLinkPowerState::SLS_PWRM_LP;

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaLwLink::DoRequestPowerState
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
        const auto pwrmIcSwCtrlRegs = RX == dir ? s_rxPwrmIcSwCtrl : s_txPwrmIcSwCtrl;
        UINT32 pwrmIcSwCtrlRegValue = 0;
    
        const auto fb = RX == dir ? s_RxSoftwareDesiredFB : s_TxSoftwareDesiredFB;
        const auto lp = RX == dir ? s_RxSoftwareDesiredLP : s_TxSoftwareDesiredLP;
        const auto swDesiredVal = LwLinkPowerState::SLS_PWRM_FB == state ? fb : lp;
    
        const auto hwDisField = RX == dir ? s_RxHwLPDisable : s_TxHwLPDisable;
    
        regHal.SetField(&pwrmIcSwCtrlRegValue, swDesiredVal[linkId]);
        regHal.SetField(&pwrmIcSwCtrlRegValue, hwDisField[linkId], bHwControlled ? 0 : 1);
        if (bHwControlled && LwLinkPowerState::SLS_PWRM_LP == state)
        {
            // set the bit allowing idle counting if the HW is allowed to change the power state,
            // i.e. when true == (SOFTWAREDESIRED == 1 (LP) && HARDWAREDISABLE == 0)
            const auto idleCountStart = RX == dir ? s_RxIdleCountStart : s_TxIdleCountStart;
            regHal.SetField(&pwrmIcSwCtrlRegValue, idleCountStart[linkId], 1);
        }
    
        // Set both HW disable and SW desired bits at once due to a bug in fmodel
        // that increments LP entry counter even if we write LP to SW desired power
        // state bit that already was set to LP.
        regHal.Write32(pwrmIcSwCtrlRegs[linkId], pwrmIcSwCtrlRegValue);
    }

    return rc;
}

//------------------------------------------------------------------------------
UINT32 VoltaLwLink::DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
{
    if (!IsLinkActive(linkId))
        return 0;

    const auto pwrmIcLpEnterThresholdRegs =
        RX == dir ? s_RxPwrmIcLpEnterThreshold : s_TxPwrmIcLpEnterThreshold;
    const auto pwrmIcIncLpIncRegs = RX == dir ? s_RxPwrmIcIncLpInc : s_TxPwrmIcIncLpInc;
    const RegHal& regs = Regs();

    const FLOAT64 linkClock = GetLinkClockRateMhz(linkId);
    const FLOAT64 threshold = regs.Read32(pwrmIcLpEnterThresholdRegs[linkId]);
    const FLOAT64 lpInc = regs.Read32(pwrmIcIncLpIncRegs[linkId]);

    return static_cast<UINT32>(threshold / linkClock / 1000.0 / lpInc);
}

//------------------------------------------------------------------------------
RC VoltaLwLink::GetEomConfigValue
(
    FomMode mode,
    UINT32 numErrors,
    UINT32 numBlocks,
    UINT32 *pConfigVal
) const
{
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
            Printf(Tee::PriError, "Invalid or unsupported FOM mode : %d\n", mode);
            return RC::ILWALID_ARGUMENT;
    }

    *pConfigVal = DRF_NUM(_MINION, _EOM, _TYPE, typeVal) |
                  DRF_NUM(_MINION, _EOM, _MODE, modeVal) |
                  DRF_NUM(_MINION, _EOM, _NBLKS, numBlocks) |
                  DRF_NUM(_MINION, _EOM, _NERRS, numErrors);
    return RC::OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaLwLink::DoClearLPCounts(UINT32 linkId)
{
    RC rc;

    ReserveLwLinkCounters reserveLwlCounters(this);
    CHECK_RC(reserveLwlCounters.Reserve());

    LW90CC_CTRL_LWLINK_CLEAR_LP_COUNTERS_PARAMS clearParams{ linkId };

    CHECK_RC(LwRmPtr()->Control(
        GetProfilerHandle(),
        LW90CC_CTRL_CMD_LWLINK_CLEAR_LP_COUNTERS,
        &clearParams,
        sizeof(clearParams)
    ));
    CHECK_RC(reserveLwlCounters.Release());

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC VoltaLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
) 
{
    MASSERT(nullptr != pCount);

    RC rc;

    *pCount = 0;

    ReserveLwLinkCounters reserveLwlCounters(this);
    CHECK_RC(reserveLwlCounters.Reserve());

    auto cntNum = entry ? LW90CC_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_ENTER :
                          LW90CC_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_EXIT;

    LW90CC_CTRL_LWLINK_GET_LP_COUNTERS_PARAMS getCountersParams
    {
        linkId,
        LWBIT32(cntNum)
    };

    CHECK_RC(LwRmPtr()->Control(
        GetProfilerHandle(),
        LW90CC_CTRL_CMD_LWLINK_GET_LP_COUNTERS,
        &getCountersParams,
        sizeof(getCountersParams)
    ));
    CHECK_RC(reserveLwlCounters.Release());

    *pCount = getCountersParams.counterValues[cntNum];
    *pbOverflow = HasLPCountOverflowed(entry, *pCount);

    return rc;
}

//------------------------------------------------------------------------------
bool VoltaLwLink::HasLPCountOverflowed(bool bEntry, UINT32 lpCount) const
{
    auto cntReg = bEntry ? MODS_PLWL_STATS_D_NUM_TX_LP_ENTER :
                           MODS_PLWL_STATS_D_NUM_TX_LP_EXIT;
    return (lpCount == Regs().LookupMask(cntReg));
}

//------------------------------------------------------------------------------
// LwLinkUphy
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
UINT32 VoltaLwLink::DoGetActiveLaneMask(RegBlock regBlock) const
{
    if (regBlock == RegBlock::CLN)
        return 1;

    return (1 << DoGetMaxLanes(regBlock)) - 1;
}

//------------------------------------------------------------------------------
UINT32 VoltaLwLink::DoGetMaxLanes(RegBlock regBlock) const 
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
UINT32 VoltaLwLink::DoGetMaxRegBlocks(RegBlock regBlock) const
{
    if (regBlock == RegBlock::CLN)
        return GetMaxLinkGroups();

    return GetMaxLinks();
}

//------------------------------------------------------------------------------
UINT64 VoltaLwLink::DoGetRegLogRegBlockMask(RegBlock regBlock)
{
    if (regBlock == RegBlock::CLN)
        return m_UphyRegsIoctlMask;
    return m_UphyRegsLinkMask;
}

//------------------------------------------------------------------------------
RC VoltaLwLink::DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive)
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
RC VoltaLwLink::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
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
    LW2080_CTRL_LWLINK_READ_UPHY_PAD_LANE_REG_PARAMS readUphyParams = { };
    readUphyParams.linkId = linkId;
    readUphyParams.lane = lane;
    readUphyParams.addr = addr;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_READ_UPHY_PAD_LANE_REG,
                                           &readUphyParams,
                                           sizeof(readUphyParams)));
    *pData = static_cast<UINT16>(readUphyParams.phyConfigData);
    return rc;
}

//------------------------------------------------------------------------------
RC VoltaLwLink::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
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

    static const ModsGpuRegAddress s_PadWdataRegs[] =
    {
        MODS_PLWL0_BR0_PAD_CTL_6
       ,MODS_PLWL1_BR0_PAD_CTL_6
       ,MODS_PLWL2_BR0_PAD_CTL_6
       ,MODS_PLWL3_BR0_PAD_CTL_6
       ,MODS_PLWL4_BR0_PAD_CTL_6
       ,MODS_PLWL5_BR0_PAD_CTL_6
    };
    static const ModsGpuRegValue s_PadPrivEnabledVal[] =
    {
        MODS_PLWL0_BR0_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE
       ,MODS_PLWL1_BR0_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE
       ,MODS_PLWL2_BR0_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE
       ,MODS_PLWL3_BR0_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE
       ,MODS_PLWL4_BR0_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE
       ,MODS_PLWL5_BR0_PRIV_LEVEL_MASK0_WRITE_PROTECTION_LEVEL0_ENABLE
    };
    MASSERT(linkId < (sizeof(s_PadWdataRegs) / sizeof(ModsGpuRegField)));

    // On GV100 there are effectively 3 IOCTL blocks, one for each pair of links
    RegHal &regs = Regs();

    // Priv protection registers are backwards.  ENABLE does not mean write protection
    // is enabled for the register, but rather that level is allowed to write the regiter
    if (!regs.Test32(s_PadPrivEnabledVal[linkId], lane))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PAD registers may not be written due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    UINT32 wData = regs.SetField(MODS_PLWL0_BR0_PAD_CTL_6_CFG_WDATA, data);
    regs.SetField(&wData, MODS_PLWL0_BR0_PAD_CTL_6_CFG_ADDR, addr);
    regs.SetField(&wData, MODS_PLWL0_BR0_PAD_CTL_6_CFG_WDS_ON);
    regs.Write32(s_PadWdataRegs[linkId], lane, wData);
    return RC::OK;
}

//------------------------------------------------------------------------------
RC VoltaLwLink::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    if (ioctl >= DoGetMaxLinkGroups())
    {
        Printf(Tee::PriError, "Invalid ioctl %u when reading pll register!\n", ioctl);
        return RC::BAD_PARAMETER;
    }
    static const ModsGpuRegAddress s_PllWdataRegs[] =
    {
        MODS_PLWL0_BR0_PLL_CTL_4
       ,MODS_PLWL2_BR0_PLL_CTL_4
       ,MODS_PLWL4_BR0_PLL_CTL_4
    };
    static const ModsGpuRegValue s_PllPrivEnabledVal[] =
    {
        MODS_PLWL0_BR0_PRIV_LEVEL_MASK_COM0_WRITE_PROTECTION_LEVEL0_ENABLE
       ,MODS_PLWL2_BR0_PRIV_LEVEL_MASK_COM0_WRITE_PROTECTION_LEVEL0_ENABLE
       ,MODS_PLWL4_BR0_PRIV_LEVEL_MASK_COM0_WRITE_PROTECTION_LEVEL0_ENABLE
    };
    static const ModsGpuRegField s_PllWRataRegs[] =
    {
        MODS_PLWL0_BR0_PLL_CTL_5_CFG_RDATA
       ,MODS_PLWL2_BR0_PLL_CTL_5_CFG_RDATA
       ,MODS_PLWL4_BR0_PLL_CTL_5_CFG_RDATA
    };

    // On GV100 there are effectively 3 IOCTL blocks, one for each pair of links
    RegHal &regs = Regs();

    // Priv protection registers are backwards.  ENABLE does not mean write protection
    // is enabled for the register, but rather that level is allowed to write the regiter
    if (!regs.Test32(s_PllPrivEnabledVal[ioctl]))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PLL registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }
    UINT32 pllCtl4 = regs.Read32(ioctl, s_PllWdataRegs[ioctl]);
    regs.SetField(&pllCtl4, MODS_LWLPLLCTL_PLL_CTL_4_CFG_ADDR, addr);
    regs.SetField(&pllCtl4, MODS_LWLPLLCTL_PLL_CTL_4_CFG_RDS_ON);
    regs.Write32(ioctl, s_PllWdataRegs[ioctl], pllCtl4);
    *pData = static_cast<UINT16>(regs.Read32(ioctl, s_PllWRataRegs[ioctl]));
    return RC::OK;
}

//------------------------------------------------------------------------------
RC VoltaLwLink::DoSetRegLogLinkMask(UINT64 linkMask)
{
    const UINT64 maxLinkMask = GetMaxLinkMask();

    if (maxLinkMask == 0ULL)
    {
        m_UphyRegsLinkMask  = 0ULL;
        m_UphyRegsIoctlMask = 0;
        return RC::OK;
    }

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
