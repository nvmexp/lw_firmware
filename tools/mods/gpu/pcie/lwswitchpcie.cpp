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

#include "lwswitchpcie.h"

#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "ctrl_dev_lwswitch.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/include/testdevice.h"
#include "lwmisc.h"
#include "lwswitch/svnp01/dev_lw_xve.h"
#include "rm.h"
#include <numeric>

namespace
{
    map<PexErrorCounts::PexErrCountIdx, UINT32> s_PexErrorMap =
    {
        { PexErrorCounts::DETAILED_NAKS_S_ID         , LWSWITCH_PEX_COUNTER_NAKS_SENT_COUNT          },
        { PexErrorCounts::REPLAY_ID                  , LWSWITCH_PEX_COUNTER_REPLAY_COUNT             },
        { PexErrorCounts::RECEIVER_ID                , LWSWITCH_PEX_COUNTER_RECEIVER_ERRORS          },
        { PexErrorCounts::LANE_ID                    , LWSWITCH_PEX_COUNTER_LANE_ERRORS              },
        { PexErrorCounts::BAD_DLLP_ID                , LWSWITCH_PEX_COUNTER_BAD_DLLP_COUNT           },
        { PexErrorCounts::REPLAY_ROLLOVER_ID         , LWSWITCH_PEX_COUNTER_REPLAY_ROLLOVER_COUNT    },
        { PexErrorCounts::BAD_TLP_ID                 , LWSWITCH_PEX_COUNTER_BAD_TLP_COUNT            },
        { PexErrorCounts::E_8B10B_ID                 , LWSWITCH_PEX_COUNTER_8B10B_ERRORS_COUNT       },
        { PexErrorCounts::SYNC_HEADER_ID             , LWSWITCH_PEX_COUNTER_SYNC_HEADER_ERRORS_COUNT },
        { PexErrorCounts::DETAILED_CRC_ID            , LWSWITCH_PEX_COUNTER_LCRC_ERRORS_COUNT        },
        { PexErrorCounts::DETAILED_NAKS_R_ID         , LWSWITCH_PEX_COUNTER_NAKS_RCVD_COUNT          },
        { PexErrorCounts::DETAILED_FAILEDL0SEXITS_ID , LWSWITCH_PEX_COUNTER_FAILED_L0S_EXITS_COUNT   },
        { PexErrorCounts::CORR_ID                    , LWSWITCH_PEX_COUNTER_CORR_ERROR_COUNT         },
        { PexErrorCounts::NON_FATAL_ID               , LWSWITCH_PEX_COUNTER_NON_FATAL_ERROR_COUNT    },
        { PexErrorCounts::FATAL_ID                   , LWSWITCH_PEX_COUNTER_FATAL_ERROR_COUNT        },
        { PexErrorCounts::UNSUP_REQ_ID               , LWSWITCH_PEX_COUNTER_UNSUPP_REQ_COUNT         }
    };

    UINT32 GetPexErrorMask()
    {
        return std::accumulate(s_PexErrorMap.begin(), s_PexErrorMap.end(), 0,
                       [](UINT32 a, const pair<PexErrorCounts::PexErrCountIdx,UINT32>& b) -> UINT32
                       {
                           return a | b.second;
                       });
    }

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
}

//-----------------------------------------------------------------------------
//! \brief Return whether Advanced Error Reporting (AER) is enabled.
//!
//! \return true if AER is enabled, false otherwise
/* virtual */ bool LwSwitchPcie::DoAerEnabled() const
{
    return GetDevice()->GetInterface<LwLinkRegs>()->Regs().Test32(0, MODS_XVE_PRIV_MISC_1_CYA_AER_ENABLE);
}

//-----------------------------------------------------------------------------
UINT32 LwSwitchPcie::DoAspmCapability()
{
    return GetDevice()->GetInterface<LwLinkRegs>()->Regs().Read32(MODS_EP_PCFG_GPU_LINK_CAPABILITIES_ASPM_SUPPORT);
}

//-----------------------------------------------------------------------------
//! \brief Enable or disable Advanced Error Reporting (AER)
//!
//! \param bEnable : true to enable AER, false to disable
/* virtual */ void LwSwitchPcie::DoEnableAer(bool bEnable)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    if (!m_bRestoreAer)
    {
        m_bOrigAerEnabled = regs.Test32(0, MODS_XVE_PRIV_MISC_1_CYA_AER_ENABLE);
        m_bRestoreAer = true;
    }

    if (bEnable)
    {
        regs.Write32(0, MODS_XVE_PRIV_MISC_1_CYA_AER_ENABLE);
    }
    else
    {
        regs.Write32(0, MODS_XVE_PRIV_MISC_1_CYA_AER_DISABLE);
    }
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchPcie::DoGetAspmCyaL1SubState(UINT32 *pAspmCya)
{
    *pAspmCya = 0;

    RC rc;
    UINT16 l1pmsscya = 0;
    CHECK_RC(ReadPci(LW_XVE_L1_PM_SUBSTATES_CYA, &l1pmsscya));
    if (FLD_TEST_DRF(_XVE, _L1_PM_SUBSTATES_CYA, _ASPM_L1_2, _SUPPORTED, l1pmsscya))
    {
        *pAspmCya |= Pci::PM_SUB_L12;
    }
    if (FLD_TEST_DRF(_XVE, _L1_PM_SUBSTATES_CYA, _ASPM_L1_1, _SUPPORTED, l1pmsscya))
    {
        *pAspmCya |= Pci::PM_SUB_L11;
    }
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 LwSwitchPcie::DoGetAspmCyaState()
{
    UINT16 xvePriv0 = 0;
    if (OK != ReadPci(LW_XVE_PRIV_XV_0, &xvePriv0))
    {
        Printf(Tee::PriError, "%s : Cannot get Aspm CYA State!\n", __FUNCTION__);
        return 0;
    }

    UINT32 aspmCya = DRF_VAL(_XVE, _PRIV_XV_0, _CYA_L0S_ENABLE, xvePriv0);
    aspmCya |= DRF_VAL(_XVE, _PRIV_XV_0, _CYA_L1_ENABLE, xvePriv0) << 1;

    return aspmCya;
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::DoGetAspmDeepL1Enabled(bool *pbEnabled)
{
    MASSERT(pbEnabled);
    *pbEnabled = false;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::DoGetAspmEntryCounts(AspmEntryCounters *pCounts)
{
    MASSERT(pCounts);

    RC rc;
    LWSWITCH_PEX_GET_COUNTERS_PARAMS getCountersParams = { };
    getCountersParams.pexCounterMask = LWSWITCH_PEX_COUNTER_L1_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1_1_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1_2_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1_2_ABORT_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1P_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_CHIPSET_XMIT_L0S_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT |
                                       LWSWITCH_PEX_COUNTER_L1_SHORT_DURATION_COUNT;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_GET_COUNTERS,
         &getCountersParams,
         sizeof(getCountersParams)));

    pCounts->L1Entry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_ENTRY_COUNT)];
    pCounts->L1_1_Entry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_1_ENTRY_COUNT)];
    pCounts->L1_2_Entry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_2_ENTRY_COUNT)];
    pCounts->L1_2_Abort = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_2_ABORT_COUNT)];
    pCounts->L1PEntry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1P_ENTRY_COUNT)];
    pCounts->UpstreamL0SEntry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_CHIPSET_XMIT_L0S_ENTRY_COUNT)];
    pCounts->XmitL0SEntry = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT)];
    pCounts->L1_ShortDuration = getCountersParams.pexCounters[Utility::BitScanForward(
            LWSWITCH_PEX_COUNTER_L1_SHORT_DURATION_COUNT)];

    // These counters are not supported
    pCounts->L1ClockPmPllPdEntry = 0;
    pCounts->L1ClockPmCpmEntry = 0;
    pCounts->DeepL1Entry = 0;
    pCounts->L1SS_DeepL1Timouts = 0;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::DoGetAspmL1SSCapability(UINT32 *pCap)
{
    MASSERT(pCap);
    RC rc;

    *pCap = Pci::PM_SUB_DISABLED;

    // RTL can turn off the PCIE interface and only allows certain register accesses
    // to PCIE in this state without crashing, these CYAs are not allowed
    if (Platform::IsVirtFunMode() || (Platform::GetSimulationMode() == Platform::RTL))
    {
        return RC::OK;
    }

    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32 capsReg = regs.Read32(MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER);
    if (regs.TestField(capsReg, MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER_L1_PM_SS_SUPPORT_SUPPORTED))
    {
        if (regs.TestField(capsReg, MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER_ASPM_L11_SUPPORT_SUPPORTED))
        {
            *pCap |= Pci::PM_SUB_L11;
        }
        if (regs.TestField(capsReg, MODS_EP_PCFG_GPU_L1_PM_SS_CAP_REGISTER_ASPM_L12_SUPPORT_SUPPORTED))
        {
            *pCap |= Pci::PM_SUB_L12;
        }
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ bool LwSwitchPcie::DoSupportsFomMode(Pci::FomMode mode)
{
    switch (mode)
    {
        case Pci::EYEO_X:
        case Pci::EYEO_Y:
            return true;
        default:
            break;
    }
    return false;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchPcie::DoGetEomStatus
(
    Pci::FomMode mode,
    vector<UINT32>* status,
    FLOAT64 timeoutMs
)
{
    MASSERT(status);
    MASSERT(status->size());
    RC rc;
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    const UINT32 numLanes = static_cast<UINT32>(status->size());

    UINT32 modeVal = 0x0;
    switch (mode)
    {
        case Pci::EYEO_X:
            modeVal = LW_EOM_CTRL2_MODE_X;
            break;
        case Pci::EYEO_Y:
            modeVal = LW_EOM_CTRL2_MODE_Y;
            break;
        default:
            Printf(Tee::PriError, "Invalid Mode = %u\n", mode);
            return RC::ILWALID_ARGUMENT;
    }

    regs.Write32(0, MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    UINT32 ctl8 = regs.Read32(0, MODS_XP_PEX_PAD_CTL_8);
    regs.SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDATA, DRF_NUM(_EOM, _CTRL2, _MODE, modeVal) |
                                                          DRF_NUM(_EOM, _CTRL2, _NBLKS, 0xd) |
                                                          DRF_NUM(_EOM, _CTRL2, _NERRS, 0x7));
    regs.SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, LW_EOM_CTRL2);
    regs.SetField(&ctl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDS, 0x1);
    regs.Write32(0, MODS_XP_PEX_PAD_CTL_8, ctl8);

    regs.Write32(0, MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(0, MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);

    UINT32 doneStatus = 0x0;
    auto pollEomDone = [&]()->bool
        {
            return regs.Test32(0, MODS_XP_PEX_PAD_CTL_5_RX_EOM_DONE, doneStatus);
        };

    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(0, MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (0xffff << i) & 0xffff);
        CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));
    }

    regs.Write32(0, MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(0, MODS_XP_PEX_PAD_CTL_5_RX_EOM_OVRD_ENABLE);
    regs.Write32(0, MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_ENABLE);

    doneStatus = 0x1;
    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(0, MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (0xffff << i) & 0xffff);
        CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));
    }

    regs.Write32(0, MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT_LANES_15_0);
    regs.Write32(0, MODS_XP_PEX_PAD_CTL_5_RX_EOM_EN_DISABLE);
    regs.Write32(0, MODS_XP_PEX_PAD_CTL_5_RX_EOM_OVRD_DISABLE);

    for (UINT32 i = 0; i < numLanes; i++)
    {
        regs.Write32(0, MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (0xffff << i) & 0xffff);
        status->at(i) = regs.Read32(0, MODS_XP_PEX_PAD_CTL_5_RX_EOM_STATUS);
    }

    return OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchPcie::DoGetErrorCounts
(
    PexErrorCounts            *pLocCounts,
    PexErrorCounts            *pHostCounts,
    PexDevice::PexCounterType  countType
)
{
    RC rc;

    if (pHostCounts)
    {
        CHECK_RC(GetHostErrorCounts(pHostCounts, countType));
    }

    if (!pLocCounts)
    {
        return rc;
    }

    // If only retrieving flag based counters, just return immediately.
    // The call to ResetPcieErrorCounters() is purposefully skipped below
    // since retrieving flag based counters should not affect hw counter
    // based counters
    if (countType == PexDevice::PEX_COUNTER_FLAG)
    {
        return rc;
    }

    CHECK_RC(GetLocalErrorCounts(pLocCounts));
    CHECK_RC(UpdateAerLog());
    CHECK_RC(ResetErrorCounters());

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 LwSwitchPcie::DoGetL1SubstateOffset()
{
    return GetDevice()->GetInterface<LwLinkRegs>()->Regs().LookupAddress(MODS_EP_PCFG_GPU_L1_PM_SS_HEADER);
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::DoResetAspmEntryCounters()
{
    RC rc;
    LWSWITCH_PEX_CLEAR_COUNTERS_PARAMS clearCountersParams = { };
    clearCountersParams.pexCounterMask = LWSWITCH_PEX_COUNTER_L1_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1_1_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1_2_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1_2_ABORT_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1P_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_CHIPSET_XMIT_L0S_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_GPU_XMIT_L0S_ENTRY_COUNT |
                                         LWSWITCH_PEX_COUNTER_L1_SHORT_DURATION_COUNT;
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_CLEAR_COUNTERS,
         &clearCountersParams,
         sizeof(clearCountersParams)));
    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchPcie::DoResetErrorCounters()
{
    RC rc;

    LWSWITCH_PEX_CLEAR_COUNTERS_PARAMS pexCounters = {};
    pexCounters.pexCounterMask = GetPexErrorMask();

    CHECK_RC(GetDevice()->GetLibIf()->Control(m_LibHandle,
         LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_CLEAR_COUNTERS,
         &pexCounters,
         sizeof(pexCounters)));

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchPcie::DoSetAspmState(UINT32 aspm)
{
    RC rc;

    GetDevice()->GetInterface<LwLinkRegs>()->Regs().Write32(MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS_ASPM_CTRL, aspm);

    return OK;
}

//-----------------------------------------------------------------------------
Pci::PcieLinkSpeed LwSwitchPcie::DoGetLinkSpeed(Pci::PcieLinkSpeed expectedSpeed)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    const UINT32 status = regs.Read32(MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS);

    if (regs.TestField(status, MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_2P5))
    {
        return Pci::Speed2500MBPS;
    }
    else if (regs.TestField(status, MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_5P0))
    {
        return Pci::Speed5000MBPS;
    }
    else if (regs.TestField(status, MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_8P0))
    {
        return Pci::Speed8000MBPS;
    }
    else if (regs.TestField(status, MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_16P0))
    {
        return Pci::Speed16000MBPS;
    }
    else if (regs.TestField(status, MODS_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_32P0))
    {
        return Pci::Speed32000MBPS;
    }
    else
    {
        return Pci::SpeedUnknown;
    }
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::DoSetLinkSpeed(Pci::PcieLinkSpeed newSpeed)
{
    RC rc;
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32 data = regs.Read32(MODS_XP_PL_LINK_CONFIG_0);
    regs.SetField(&data, MODS_XP_PL_LINK_CONFIG_0_LTSSM_DIRECTIVE_CHANGE_SPEED);

    switch (newSpeed)
    {
        case Pci::Speed2500MBPS:
            regs.SetField(&data, MODS_XP_PL_LINK_CONFIG_0_MAX_LINK_RATE_2500_MTPS);
            break;
        case Pci::Speed5000MBPS:
            regs.SetField(&data, MODS_XP_PL_LINK_CONFIG_0_MAX_LINK_RATE_5000_MTPS);
            break;
        case Pci::Speed8000MBPS:
            regs.SetField(&data, MODS_XP_PL_LINK_CONFIG_0_MAX_LINK_RATE_8000_MTPS);
            break;
        case Pci::Speed16000MBPS:
            regs.SetField(&data, MODS_XP_PL_LINK_CONFIG_0_MAX_LINK_RATE_16000_MTPS);
            break;
        default:
            Printf(Tee::PriError, "Invalid PCIE Link Speed = %d\n", static_cast<UINT32>(newSpeed));
            return RC::ILWALID_ARGUMENT;
    }

    regs.Write32(MODS_XP_PL_LINK_CONFIG_0, data);

    CHECK_RC_MSG(Tasker::PollHw(Tasker::GetDefaultTimeoutMs(), [&]() -> bool
    {
        return (GetLinkSpeed(newSpeed) == newSpeed);
    }, __FUNCTION__),
    "Failed to set PCIE Link Speed = %d\n", static_cast<UINT32>(newSpeed));

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::DoGetPolledErrors
(
    Pci::PcieErrorsMask* pDevErrors,
    Pci::PcieErrorsMask *pHostErrors
)
{
    RC rc;

    if (pDevErrors)
    {
        *pDevErrors = Pci::NO_ERROR;

        RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
        UINT32 ctrlStatus = regs.Read32(MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS);

        if (regs.TestField(ctrlStatus, MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_CORR_ERROR_DETECTED, 1))
        {
            *pDevErrors |= Pci::CORRECTABLE_ERROR;
        }
        if (regs.TestField(ctrlStatus, MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_NON_FATAL_ERROR_DETECTED, 1))
        {
            *pDevErrors |= Pci::NON_FATAL_ERROR;
        }
        if (regs.TestField(ctrlStatus, MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_FATAL_ERROR_DETECTED, 1))
        {
            *pDevErrors |= Pci::FATAL_ERROR;
        }
        if (regs.TestField(ctrlStatus, MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS_UNSUPP_REQUEST_DETECTED, 1))
        {
            *pDevErrors |= Pci::UNSUPPORTED_REQ;
        }

        // Clear Errors
        regs.Write32(MODS_EP_PCFG_GPU_DEVICE_CONTROL_STATUS, ctrlStatus);
    }

    if (pHostErrors)
    {
        *pHostErrors = Pci::NO_ERROR;

        PexDevice *pUpStreamDev = nullptr;
        UINT32 parentPort = 0;
        CHECK_RC(GetUpStreamInfo(&pUpStreamDev, &parentPort));

        CHECK_RC(pUpStreamDev->GetDownStreamErrors(pHostErrors,
                                                   parentPort));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::GetHostErrorCounts
(
    PexErrorCounts* pCounts,
    PexDevice::PexCounterType countType
)
{
    RC rc;
    PexDevice* pUpStreamDev = nullptr;
    UINT32 parentPort = ~0U;

    CHECK_RC(GetUpStreamInfo(&pUpStreamDev, &parentPort));

    if (pUpStreamDev)
    {
        CHECK_RC(pUpStreamDev->GetDownStreamErrorCounts(pCounts,
                                                        parentPort,
                                                        countType));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::DoHasAspmDeepL1(bool *pbHasDeepL1)
{
    MASSERT(pbHasDeepL1);
    *pbHasDeepL1 = false;
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchPcie::DoInitialize
(
    LwLinkDevIf::LibInterface::LibDeviceHandle libHandle
)
{
    m_LibHandle = libHandle;

    return PcieImpl::DoInitialize(libHandle);
}

//-----------------------------------------------------------------------------
/* virtual */ RC LwSwitchPcie::DoShutdown()
{
    if (m_bRestoreAer)
    {
        EnableAer(m_bOrigAerEnabled);
        m_bRestoreAer = false;
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC LwSwitchPcie::GetLocalErrorCounts(PexErrorCounts* pCounts)
{
    RC rc;
    LwLinkDevIf::LibIfPtr libIf = GetDevice()->GetLibIf();

    LWSWITCH_PEX_GET_COUNTERS_PARAMS pexCounters = {};
    pexCounters.pexCounterMask = GetPexErrorMask();

    CHECK_RC(libIf->Control(m_LibHandle,
                            LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_GET_COUNTERS,
                            &pexCounters,
                            sizeof(pexCounters)));

    for (const auto& errorType : s_PexErrorMap)
    {
        CHECK_RC(pCounts->SetCount(errorType.first, true,
                                   pexCounters.pexCounters[Utility::Log2i(errorType.second)]));
        if (errorType.first == PexErrorCounts::E_8B10B_ID ||
            errorType.first == PexErrorCounts::SYNC_HEADER_ID)
        {
            const UINT32 e8B10BIdx = Utility::Log2i(s_PexErrorMap[PexErrorCounts::E_8B10B_ID]);
            const UINT32 syncIdx   = Utility::Log2i(s_PexErrorMap[PexErrorCounts::SYNC_HEADER_ID]);
            UINT32 lineErrorCounts = pexCounters.pexCounters[e8B10BIdx] + pexCounters.pexCounters[syncIdx];
            CHECK_RC(pCounts->SetCount(PexErrorCounts::DETAILED_LINEERROR_ID, true, lineErrorCounts));
        }
    }

    LWSWITCH_PEX_GET_LANE_COUNTERS_PARAMS perLaneParams = {};
    CHECK_RC(libIf->Control(m_LibHandle,
                            LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_PEX_GET_LANE_COUNTERS,
                            &perLaneParams,
                            sizeof(perLaneParams)));

    pCounts->SetPerLaneCounts(&perLaneParams.pexLaneCounter[0]);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Helper function to get the PCIE lanes for Rx or Tx
//!
//! \param isTx : Set to 'true' for Tx, 'false' for Rx
//!
//! \return PCIE lanes (16 bits, each one corresponding to a lane)
/* virtual */ UINT32 LwSwitchPcie::GetRegLanes(bool isTx) const
{
    UINT32 val = 0;
    const RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();

    UINT32 regVal = regs.Read32(0, MODS_XP_PL_LINK_STATUS_0);

    // Map the 5 bit register field value to the corresponding lanes
    if (isTx)
    {
        if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_NULL))
        {
            val = 0x0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_00_00))
        {
            val = 0x1;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_01_01))
        {
            val = 0x2;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_02_02))
        {
            val = 0x4;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_03_03))
        {
            val = 0x8;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_04_04))
        {
            val = 0x10;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_05_05))
        {
            val = 0x20;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_06_06))
        {
            val = 0x40;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_07))
        {
            val = 0x80;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_08_08))
        {
            val = 0x100;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_09_09))
        {
            val = 0x200;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_10_10))
        {
            val = 0x400;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_11_11))
        {
            val = 0x800;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_12_12))
        {
            val = 0x1000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_13_13))
        {
            val = 0x2000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_14_14))
        {
            val = 0x4000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_15))
        {
            val = 0x8000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_01_00))
        {
            val = 0x3;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_03_02))
        {
            val = 0xC;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_05_04))
        {
            val = 0x30;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_06))
        {
            val = 0xC0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_09_08))
        {
            val = 0x300;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_11_10))
        {
            val = 0xC00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_13_12))
        {
            val = 0x3000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_14))
        {
            val = 0xC000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_03_00))
        {
            val = 0xF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_04))
        {
            val = 0xF0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_11_08))
        {
            val = 0xF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_12))
        {
            val = 0xF000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_07_00))
        {
            val = 0xFF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_08))
        {
            val = 0xFF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_TX_LANES_15_00))
        {
            val = 0xFFFF;
        }
    }
    else
    {
        if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_NULL))
        {
            val = 0x0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_00_00))
        {
            val = 0x1;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_01))
        {
            val = 0x2;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_02_02))
        {
            val = 0x4;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_03))
        {
            val = 0x8;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_04_04))
        {
            val = 0x10;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_05))
        {
            val = 0x20;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_06_06))
        {
            val = 0x40;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_07))
        {
            val = 0x80;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_08_08))
        {
            val = 0x100;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_09))
        {
            val = 0x200;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_10_10))
        {
            val = 0x400;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_11))
        {
            val = 0x800;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_12_12))
        {
            val = 0x1000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_13))
        {
            val = 0x2000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_14_14))
        {
            val = 0x4000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_15))
        {
            val = 0x8000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_01_00))
        {
            val = 0x3;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_02))
        {
            val = 0xC;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_05_04))
        {
            val = 0x30;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_06))
        {
            val = 0xC0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_09_08))
        {
            val = 0x300;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_10))
        {
            val = 0xC00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_13_12))
        {
            val = 0x3000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_14))
        {
            val = 0xC000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_03_00))
        {
            val = 0xF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_04))
        {
            val = 0xF0;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_11_08))
        {
            val = 0xF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_12))
        {
            val = 0xF000;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_07_00))
        {
            val = 0xFF;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_08))
        {
            val = 0xFF00;
        }
        else if (regs.TestField(regVal, MODS_XP_PL_LINK_STATUS_0_ACTIVE_RX_LANES_15_00))
        {
            val = 0xFFFF;
        }
    }

    return val;
}

//------------------------------------------------------------------------------
UINT32 LwSwitchPcie::DoGetActiveLaneMask(RegBlock regBlock) const
{
    if (regBlock == RegBlock::CLN)
        return 1;

    return GetRegLanesRx();
}

UINT32 LwSwitchPcie::DoGetMaxLanes(RegBlock regBlock) const 
{
    if (regBlock == RegBlock::CLN)
        return 1;

    return 16;
}

RC LwSwitchPcie::DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    const UINT32 laneMask = regs.LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);
    if (!((1 << lane) & laneMask))
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    UINT32 padCtl8 = regs.Read32(MODS_XP_PEX_PAD_CTL_8);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, addr);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_RDS, 1);
    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (1 << lane));
    regs.Write32(MODS_XP_PEX_PAD_CTL_8, padCtl8);
    *pData = static_cast<UINT16>(regs.Read32(MODS_XP_PEX_PAD_CTL_9_CFG_RDATA));
    return RC::OK;
}

RC LwSwitchPcie::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    const UINT32 laneMask = regs.LookupFieldValueMask(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT);
    if (!((1 << lane) & laneMask))
    {
        Printf(Tee::PriError, "Invalid lane %u specified\n", lane);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }
    UINT32 padCtl8 = regs.Read32(MODS_XP_PEX_PAD_CTL_8);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_ADDR, addr);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDATA, data);
    regs.SetField(&padCtl8, MODS_XP_PEX_PAD_CTL_8_CFG_WDS, 1);
    regs.Write32(MODS_XP_PEX_PAD_CTL_SEL_LANE_SELECT, (1 << lane));
    regs.Write32(MODS_XP_PEX_PAD_CTL_8, padCtl8);
    return RC::OK;
}

RC LwSwitchPcie::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    RegHal& regs = GetDevice()->GetInterface<LwLinkRegs>()->Regs();
    UINT32 pllCtl5 = regs.Read32(MODS_XP_PEX_PLL_CTL5);
    regs.SetField(&pllCtl5, MODS_XP_PEX_PLL_CTL5_CFG_ADDR, addr);
    regs.SetField(&pllCtl5, MODS_XP_PEX_PLL_CTL5_CFG_RDS_PENDING);
    regs.Write32(MODS_XP_PEX_PLL_CTL5, pllCtl5);
    *pData = static_cast<UINT16>(regs.Read32(MODS_XP_PEX_PLL_CTL6_CFG_RDATA));
    return RC::OK;
}
