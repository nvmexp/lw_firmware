/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lagunalwlink.h"

#include "lwlink_lib_ctrl.h"
#include "ctrl_dev_lwswitch.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "lwlink/minion/minion_lwlink_defines_public_gh100.h"

// Field definitions in minion_lwlink_defines_public_gh100.h
#define LW_MINION_UCODE_CONFIGEOM_EOM_MODE_FOM             0x0
#define LW_MINION_UCODE_CONFIGEOM_FOM_MODE_EYEO            0x1
#define LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_UPPER        0x4
#define LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_MID          0x2
#define LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_LOWER        0x1
#define LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_ALL          0x7
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER        0x3c
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_MID          0x1e
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER        0xf
#define LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL          0x3f

#define LW_DRF_CS_DATALEN                    3:0
#define LW_DRF_CS_TCEN                       4:4
#define LW_DRF_EXTCS_REDUCTIONOP             7:0
#define LW_DRF_EXTCS_MIXEDPRECISION          8:8
#define LW_DRF_EXTCS_MIXEDPRECISION_DISABLE  0x0
#define LW_DRF_EXTCS_MIXEDPRECISION_ENABLE   0x1
#define LW_DRF_EXTCS_ATOMIC_CMD              3:0
#define LW_DRF_EXTCS_ATOMIC_CMD_IMIN         0x0
#define LW_DRF_EXTCS_ATOMIC_CMD_IMAX         0x1
#define LW_DRF_EXTCS_ATOMIC_CMD_IXOR         0x2
#define LW_DRF_EXTCS_ATOMIC_CMD_IAND         0x3
#define LW_DRF_EXTCS_ATOMIC_CMD_IOR          0x4
#define LW_DRF_EXTCS_ATOMIC_CMD_IADD         0x5
#define LW_DRF_EXTCS_ATOMIC_CMD_INC          0x6
#define LW_DRF_EXTCS_ATOMIC_CMD_DEC          0x7
#define LW_DRF_EXTCS_ATOMIC_CMD_CAS          0x8
#define LW_DRF_EXTCS_ATOMIC_CMD_EXCH         0x9
#define LW_DRF_EXTCS_ATOMIC_CMD_FADD         0xA
#define LW_DRF_EXTCS_ATOMIC_CMD_FMIN         0xB
#define LW_DRF_EXTCS_ATOMIC_CMD_FMAX         0xC
#define LW_DRF_EXTCS_ATOMIC_SIZE             6:4
#define LW_DRF_EXTCS_ATOMIC_SIZE_8           0x0
#define LW_DRF_EXTCS_ATOMIC_SIZE_16          0x1
#define LW_DRF_EXTCS_ATOMIC_SIZE_32          0x2
#define LW_DRF_EXTCS_ATOMIC_SIZE_64          0x3
#define LW_DRF_EXTCS_ATOMIC_SIZE_128         0x4
#define LW_DRF_EXTCS_ATOMIC_SIGN             7:7
#define LW_DRF_EXTCS_ATOMIC_SIGN_SIGNED      0x0
#define LW_DRF_EXTCS_ATOMIC_SIGN_UNSIGNED    0x1
#define LW_DRF_EXTCS_ATOMIC_RED              8:8
#define LW_DRF_EXTCS_ATOMIC_RED_DISABLE      0x0
#define LW_DRF_EXTCS_ATOMIC_RED_ENABLE       0x1
#define LW_DRF_EXTCS_MULTICAST             10:10
#define LW_DRF_EXTCS_MULTICAST_DISABLE       0x0
#define LW_DRF_EXTCS_MULTICAST_ENABLE        0x1
#define LW_DRF_EXTCS_ADDRTYPE              12:11
#define LW_DRF_EXTCS_ADDRTYPE_SPA            0x1
#define LW_DRF_EXTCS_ADDRTYPE_GPA_FLA        0x2
#define LW_DRF_REQATTR_PATTERN_RAM_IDX       4:0
#define LW_DRF_REQATTR_PATTERN_CHECK         5:5
#define LW_DRF_REQATTR_PATTERN_CHECK_DISABLE 0x0
#define LW_DRF_REQATTR_PATTERN_CHECK_ENABLE  0x1

#define NCISOC_CMD_READ  0x6
#define NCISOC_CMD_WRITE 0x16
#define NCISOC_CMD_ATOMIC 0x53
#define REQ_ADDR_ROUTE_MULTICAST (0b11 << 11)

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
RC LagunaLwLink::DoGetBlockCodeMode
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
    else if (eccMode == regs.LookupValue(MODS_LWLDL_TOP_LINK_CONFIG_ECC_MODE_ECC89))
    {
        *pBlockCodeMode = SystemBlockCodeMode::ECC89;
    }
    else
    {
        Printf(Tee::PriError, "Unknown block code mode %u for link %u\n", eccMode, linkId);
        return RC::BAD_PARAMETER;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
FLOAT64 LagunaLwLink::DoGetDefaultErrorThreshold
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

    if (errId == LwLink::ErrorCounts::LWL_UPHY_REFRESH_FAIL_ID)
        return static_cast<FLOAT64>(1U);

    return 0.0;
}

//-----------------------------------------------------------------------------
void LagunaLwLink::DoGetEomDefaultSettings
(
    UINT32 link,
    EomSettings* pSettings
) const
{
    MASSERT(pSettings);
    // Only 1 line rate (106.25) is planned for LwLink4
    pSettings->numErrors = 0x32;
    pSettings->numBlocks = 0x7;
}

//------------------------------------------------------------------------------
RC LagunaLwLink::DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode)
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
    else
    {
        *pLineCode = SystemLineCode::NRZ;
    }
    return RC::OK;
}

//------------------------------------------------------------------------------
RC LagunaLwLink::DoPerLaneErrorCountsEnabled
(
    UINT32 linkId,
    bool *pbPerLaneEnabled
)
{
    MASSERT(pbPerLaneEnabled);
    *pbPerLaneEnabled = false;
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LagunaLwLink::DoGetLinkPowerStateStatus
(
    UINT32 linkId,
    LwLinkPowerState::LinkPowerStateStatus *pLinkPowerStatus
) const
{
    MASSERT(pLinkPowerStatus);
    RC rc;

    const RegHal& regs = Regs();

    pLinkPowerStatus->rxHwControlledPowerState =
        regs.Test32(linkId, MODS_LWLIPT_LNK_PWRM_CTRL_L1_HARDWARE_DISABLE_HW_MONITOR_ENABLED);
    pLinkPowerStatus->rxSubLinkLwrrentPowerState =
        regs.Test32(linkId, MODS_LWLIPT_LNK_PWRM_CTRL_L1_LWRRENT_STATE_L1)
        ? LwLinkPowerState::SLS_PWRM_LP : LwLinkPowerState::SLS_PWRM_FB;
    pLinkPowerStatus->rxSubLinkConfigPowerState =
        regs.Test32(linkId, MODS_LWLIPT_LNK_PWRM_CTRL_L1_SOFTWARE_DESIRED_L1)
        ? LwLinkPowerState::SLS_PWRM_LP : LwLinkPowerState::SLS_PWRM_FB;

    // Laguna doesn't distinguish RX/TX LP states
    pLinkPowerStatus->txHwControlledPowerState = pLinkPowerStatus->rxHwControlledPowerState;
    pLinkPowerStatus->txSubLinkLwrrentPowerState = pLinkPowerStatus->rxSubLinkLwrrentPowerState;
    pLinkPowerStatus->txSubLinkConfigPowerState = pLinkPowerStatus->rxSubLinkConfigPowerState;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LagunaLwLink::DoRequestPowerState
(
    UINT32 linkId
  , LwLinkPowerState::SubLinkPowerState state
  , bool bHwControlled
)
{
    RC rc;
    CHECK_RC(CheckPowerStateAvailable(state, bHwControlled));

    RegHal& regs = Regs();

    UINT32 icSwCtrl = regs.Read32(linkId, MODS_LWLIPT_LNK_PWRM_CTRL);
    UINT32 forceCtrl = regs.Read32(linkId, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL);

    switch (state)
    {
    case LwLinkPowerState::SLS_PWRM_LP:
        regs.SetField(&icSwCtrl, MODS_LWLIPT_LNK_PWRM_CTRL_L1_SOFTWARE_DESIRED_L1);
        if (bHwControlled)
        {
            // set the bit allowing idle counting if the HW is allowed to change the power
            // state, i.e. when true == (SOFTWAREDESIRED == 1 (LP) && HARDWAREDISABLE == 0)
            regs.SetField(&icSwCtrl, MODS_LWLIPT_LNK_PWRM_CTRL_L1_COUNT_ENABLE_ENABLE);
            regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_FORCE_FORCE_OFF);
        }
        else
        {
            regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_MODE_INFINITE);
            regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_FORCE_FORCE_ON);
        }
        break;
    case LwLinkPowerState::SLS_PWRM_FB:
        regs.SetField(&icSwCtrl, MODS_LWLIPT_LNK_PWRM_CTRL_L1_SOFTWARE_DESIRED_ACTIVE);
        regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_FORCE_FORCE_OFF);
        break;
    default:
        Printf(Tee::PriError,
               "%s : Invalid SubLinkPowerState = %d\n",
               __FUNCTION__, static_cast<UINT32>(state));
        return RC::BAD_PARAMETER;
    }

    if (bHwControlled)
        regs.SetField(&icSwCtrl, MODS_LWLIPT_LNK_PWRM_CTRL_L1_HARDWARE_DISABLE_HW_MONITOR_ENABLED);
    else
        regs.SetField(&icSwCtrl, MODS_LWLIPT_LNK_PWRM_CTRL_L1_HARDWARE_DISABLE_HW_MONITOR_DISABLE);

    regs.Write32(linkId, MODS_LWLIPT_LNK_PWRM_CTRL, icSwCtrl);
    regs.Write32(linkId, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL, forceCtrl);

    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 LagunaLwLink::DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
{
    if (!IsLinkActive(linkId))
        return 0;

    const RegHal& regs = Regs();

    const FLOAT64 thresholdUnitUs = 100.0;
    const FLOAT64 threshold = regs.Read32(linkId, MODS_LWLIPT_LNK_PWRM_L1_ENTER_THRESHOLD_THRESHOLD);

    return static_cast<UINT32>((threshold * thresholdUnitUs) / 1000.0);
}

//------------------------------------------------------------------------------
RC LagunaLwLink::DoStartPowerStateToggle(UINT32 linkId, UINT32 in, UINT32 out)
{
    RegHal& regs = Regs();

    regs.Write32(linkId, MODS_LWLIPT_LNK_PWRM_CTRL_L1_SOFTWARE_DESIRED_L1);
    regs.Write32(linkId, MODS_LWLIPT_LNK_PWRM_CTRL_L1_COUNT_ENABLE_ENABLE);

    UINT32 forceCtrl = regs.Read32(linkId, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL);
    regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_MODE_TOGGLE_ALWAYS);
    regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_COUNT_IN, in / 10);
    regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_COUNT_OUT, out / 10);
    regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_COUNT_INCREMENT_10US);
    regs.SetField(&forceCtrl, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL_FORCE_FORCE_ON);
    regs.Write32(linkId, MODS_LWLIPT_LNK_PWRM_L1_FORCE_CTRL, forceCtrl);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoClearLPCounts(UINT32 linkId)
{
    RC rc;

    LWSWITCH_LWLINK_CLEAR_COUNTERS_PARAMS clearParams = {};
    clearParams.linkMask = (1LLU << linkId);
    clearParams.counterMask = LWSWITCH_LWLINK_LP_COUNTERS_DL;

    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                               LibInterface::CONTROL_CLEAR_COUNTERS,
                                               &clearParams,
                                               sizeof(clearParams)));

    return rc;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoGetLPEntryOrExitCount
(
    UINT32 linkId,
    bool entry,
    UINT32 *pCount,
    bool *pbOverflow
)
{
    MASSERT(pCount);
    MASSERT(pbOverflow);
    RC rc;

    LWSWITCH_GET_LWLINK_LP_COUNTERS_PARAMS lpParams = {};
    lpParams.linkId = linkId;
    lpParams.counterValidMask = (1U << CTRL_LWSWITCH_GET_LWLINK_LP_COUNTERS_NUM_TX_LP_ENTER) |
                                (1U << CTRL_LWSWITCH_GET_LWLINK_LP_COUNTERS_NUM_TX_LP_EXIT);
    CHECK_RC(GetDevice()->GetLibIf()->Control(GetLibHandle(),
                                              LibInterface::CONTROL_LWSWITCH_GET_LP_COUNTERS,
                                              &lpParams,
                                              sizeof(lpParams)));

    if (entry)
    {
        MASSERT(lpParams.counterValidMask & (1U << CTRL_LWSWITCH_GET_LWLINK_LP_COUNTERS_NUM_TX_LP_ENTER));
        *pCount = lpParams.counterValues[CTRL_LWSWITCH_GET_LWLINK_LP_COUNTERS_NUM_TX_LP_ENTER];
        *pbOverflow = (*pCount == _UINT16_MAX);
    }
    else
    {
        MASSERT(lpParams.counterValidMask & (1U << CTRL_LWSWITCH_GET_LWLINK_LP_COUNTERS_NUM_TX_LP_EXIT));
        *pCount = lpParams.counterValues[CTRL_LWSWITCH_GET_LWLINK_LP_COUNTERS_NUM_TX_LP_EXIT];
        *pbOverflow = (*pCount == _UINT16_MAX);
    }

    return rc;
}

//-----------------------------------------------------------------------------
UINT32 LagunaLwLink::DoGetRecalStartTimeUs(UINT32 linkId) const
{
    const RegHal& regs = Regs();
    return regs.Read32(linkId, MODS_LWLDL_RX_PHYCFG_RX_1_RECAL_START) << regs.Read32(linkId, MODS_LWLDL_RX_PHYCFG_RX_1_RECAL_START_SCALE);
}

UINT32 LagunaLwLink::DoGetRecalWakeTimeUs(UINT32 linkId) const
{
    const RegHal& regs = Regs();
    return regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_L1WAKE) << regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_L1WAKE_SCL);
}

//-----------------------------------------------------------------------------
UINT32 LagunaLwLink::DoGetMinRecalTimeUs(UINT32 linkId) const
{
    const RegHal& regs = Regs();
    return regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_MINCAL) << regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_MINCAL_SCL);
}

//-----------------------------------------------------------------------------
/* virtual */ RC LagunaLwLink::InitializeTrex(UINT32 link)
{
    RC rc;
    RegHal& regs = Regs();

    CHECK_RC(LimerockLwLink::InitializeTrex(link));

    regs.Write32(link, MODS_TREX_BUFFER_READY_BUFFERRDY_ENABLE);

    regs.Write32(link, MODS_TREX_HW_CTRL_GLOBALTREXENABLE_GLOBAL_TREX_ENABLE);

    return rc;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::GetTrexReductionOpEncode(LwLinkTrex::ReductionOp op, UINT32* pCode) const
{
    MASSERT(pCode);
    switch (op)
    {
        case RO_S16_FADD_MP:
        case RO_S16_FADD: *pCode = 0x1A; break;
        case RO_S16_FMIN: *pCode = 0x1B; break;
        case RO_S16_FMAX: *pCode = 0x1C; break;
        case RO_S32_IMIN: *pCode = 0x20; break;
        case RO_S32_IMAX: *pCode = 0x21; break;
        case RO_S32_IXOR: *pCode = 0x22; break;
        case RO_S32_IAND: *pCode = 0x23; break;
        case RO_S32_IOR:  *pCode = 0x24; break;
        case RO_S32_IADD: *pCode = 0x25; break;
        case RO_S32_FADD: *pCode = 0x2A; break;
        case RO_S64_IMIN: *pCode = 0x30; break;
        case RO_S64_IMAX: *pCode = 0x31; break;
        case RO_U16_FADD_MP:
        case RO_U16_FADD: *pCode = 0x9A; break;
        case RO_U16_FMIN: *pCode = 0x9B; break;
        case RO_U16_FMAX: *pCode = 0x9C; break;
        case RO_U32_IMIN: *pCode = 0xA0; break;
        case RO_U32_IMAX: *pCode = 0xA1; break;
        case RO_U32_IXOR: *pCode = 0xA2; break;
        case RO_U32_IAND: *pCode = 0xA3; break;
        case RO_U32_IOR:  *pCode = 0xA4; break;
        case RO_U32_IADD: *pCode = 0xA5; break;
        case RO_U64_IMIN: *pCode = 0xB0; break;
        case RO_U64_IMAX: *pCode = 0xB1; break;
        case RO_U64_IXOR: *pCode = 0xB2; break;
        case RO_U64_IAND: *pCode = 0xB3; break;
        case RO_U64_IOR:  *pCode = 0xB4; break;
        case RO_U64_IADD: *pCode = 0xB5; break;
        case RO_U64_FADD: *pCode = 0xBA; break;
        default:
            Printf(Tee::PriError, "Invalid Trex Reduction Op\n");
            return RC::ILWALID_ARGUMENT;
    }
    return RC::OK;
}


//------------------------------------------------------------------------------
UINT32 LagunaLwLink::DoGetMaxRegBlocks(RegBlock regBlock) const
{
    // PLLs are no longer associated with IOCTRLs starting on GH100.  There are 2 PLLs shared
    // across all IOCTRLs
    if (regBlock == RegBlock::CLN)
        return 4;

    return GetMaxLinks();
}

//------------------------------------------------------------------------------
RC LagunaLwLink::DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive)
{
    MASSERT(pbActive);

    if (blockIdx >= GetMaxRegBlocks(regBlock))
    {
        Printf(Tee::PriError, "Invalid reg block index %u when checking reg block active\n",
               blockIdx);
        MASSERT(!"Invalid register block index");
        return RC::BAD_PARAMETER;
    }

    RC rc;
    vector<LinkStatus> linkStatus;
    CHECK_RC(GetLinkStatus(&linkStatus));

    if (regBlock == RegBlock::CLN)
    {
        static const UINT32 linksPerPll = 16;
        const UINT32 firstLink = blockIdx * linksPerPll;

        *pbActive = false;
        for (UINT32 lwrLink = firstLink; lwrLink < (firstLink + linksPerPll); lwrLink++)
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
RC LagunaLwLink::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
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

    if (!regs.HasRWAccess(linkId, MODS_LWLPHYCTL_LANE_PAD_CTL_6, lane))
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
RC LagunaLwLink::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    RegHal &regs = Regs();

    if (!regs.HasRWAccess(ioctl, MODS_PTRIM_LWLINK_UPHY0_PLL1_CTRL4))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PLL registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    UINT32 pllCtl4 = regs.Read32(ioctl, MODS_CLOCK_LWSW_PRT_LWLINK_UPHY0_PLL0_CTRL4);
    regs.SetField(&pllCtl4, MODS_CLOCK_LWSW_PRT_LWLINK_UPHY0_PLL0_CTRL4_CFG_ADDR, addr);
    regs.SetField(&pllCtl4, MODS_CLOCK_LWSW_PRT_LWLINK_UPHY0_PLL0_CTRL4_CFG_RDS_ON);
    regs.Write32(ioctl, MODS_CLOCK_LWSW_PRT_LWLINK_UPHY0_PLL0_CTRL4, pllCtl4);
    *pData =
        static_cast<UINT16>(regs.Read32(ioctl,
                                        MODS_CLOCK_LWSW_PRT_LWLINK_UPHY0_PLL0_CTRL5_CFG_RDATA));
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoCreateRamEntry
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

    if ((type == LwLinkTrex::ET_MC_ATOMIC || type == LwLinkTrex::ET_ATOMIC) &&
        dataLen > 128)
    {
        Printf(Tee::PriError, "TREX Atomic does not support >128B data length");
        return RC::BAD_PARAMETER;
    }

    UINT32 dataLenCode = 0;
    CHECK_RC(GetTrexDataLenEncode(dataLen, &dataLenCode));
    UINT08 cs = DRF_NUM(_DRF, _CS, _DATALEN, dataLenCode);

    UINT16 extcs = DRF_DEF(_DRF, _EXTCS, _ADDRTYPE, _GPA_FLA);

    UINT32 reqAddrRoute = route;

    UINT08 reqAttr = 0;

    if (type == LwLinkTrex::ET_MC_READ || type == LwLinkTrex::ET_MC_WRITE || type == LwLinkTrex::ET_MC_ATOMIC)
    {
        extcs |= DRF_DEF(_DRF, _EXTCS, _MULTICAST, _ENABLE);

        reqAddrRoute |= REQ_ADDR_ROUTE_MULTICAST;

        reqAttr |= DRF_DEF(_DRF, _REQATTR, _PATTERN_CHECK, _DISABLE);
    }

    if (type == LwLinkTrex::ET_MC_READ || type == LwLinkTrex::ET_MC_ATOMIC || type == LwLinkTrex::ET_ATOMIC)
    {
        UINT32 opCode = 0;
        CHECK_RC(GetTrexReductionOpEncode(op, &opCode));
        extcs |= DRF_NUM(_DRF, _EXTCS, _REDUCTIONOP, opCode);
    }

    if (type == LwLinkTrex::ET_MC_READ && (op == RO_S16_FADD_MP || op == RO_U16_FADD_MP))
        extcs |= DRF_DEF(_DRF, _EXTCS, _MIXEDPRECISION, _ENABLE);

    if (type == LwLinkTrex::ET_MC_ATOMIC || type == LwLinkTrex::ET_ATOMIC)
    {
        extcs |= DRF_DEF(_DRF, _EXTCS, _ATOMIC_RED, _ENABLE);
    }

    UINT08 cmd = 0;
    switch (type)
    {
        case LwLinkTrex::ET_MC_READ:
        case LwLinkTrex::ET_READ:
            cmd = NCISOC_CMD_READ;
            break;
        case LwLinkTrex::ET_MC_WRITE:
        case LwLinkTrex::ET_WRITE:
            cmd = NCISOC_CMD_WRITE;
            break;
        case LwLinkTrex::ET_ATOMIC:
        case LwLinkTrex::ET_MC_ATOMIC:
            cmd = NCISOC_CMD_ATOMIC;
            break;
        default:
            Printf(Tee::PriError, "Unknown RamEntry Type: %d\n", static_cast<UINT32>(type));
            return RC::ILWALID_ARGUMENT;
    }

    MASSERT(route < regs.LookupFieldValueMask(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_REQ_ADDR_ROUTE));

    pEntry->data0 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_REPEAT_COUNT,     0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_NCISOC_VC,        0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_NCISOC_REQ_CMD, cmd) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA0_NCISOC_REQ_CS,   cs);

    pEntry->data1 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA1_NCISOC_REQATTR, reqAttr);

    pEntry->data2 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA2_NCISOC_REQ_EXTCS, extcs) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA2_NCISOC_REQCONTEXT,    0);

    pEntry->data3 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_PKTDEBUG,                  0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_REQLAN,                    0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA3_NCISOC_REQ_ADDR_ROUTE, reqAddrRoute);

    pEntry->data4 = regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_NCISOC_REQID, 0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_NCISOC_TGTID, 0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_SLEEP,        0) |
                    regs.SetField(MODS_TREX_DYNAMIC_MODE_REQ_RAMDATA4_DBI_EN,       0);

    return RC::OK;
}

//-----------------------------------------------------------------------------
bool LagunaLwLink::DoIsTrexDone(UINT32 linkId)
{
    RegHal& regs = Regs();

    if (!regs.Test32(linkId, MODS_TREX_HW_CTRL_REQSEQUENCEBUSY_DONE))
        return false;

    return true;
}

RC LagunaLwLink::DoSetLfsrEnabled(UINT32 linkId, bool bEnabled)
{
    RegHal& regs = Regs();

    if (bEnabled)
    {
        regs.Write32(linkId, MODS_TREX_CTRL_DATA_LFSR_EN_ENABLE);
        regs.Write32(linkId, MODS_TREX_DATA_LFSR_MASK_MASK, 0xBFFFBFFF);
    }
    else
    {
        regs.Write32(linkId, MODS_TREX_CTRL_DATA_LFSR_EN_DISABLE);
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoSetTrexRun(bool bEnabled)
{
    RC rc;

    RegHal& regs = Regs();

    if (bEnabled)
    {
        UINT32 data = regs.SetField(MODS_TREX_HW_CTRL_ENABLE_ENABLE_HW_READ);

        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_TREX_HW_CTRL, data);

        for (UINT32 linkId = 0; linkId < GetMaxLinks(); linkId++)
        {
            if (!IsLinkActive(linkId))
                continue;
            // Flush the previous writes
            regs.Read32(linkId, MODS_TREX_HW_CTRL);
        }
    }
    else
    {
        UINT32 data = regs.SetField(MODS_TREX_HW_CTRL_ENABLE_DISABLE_HW_READ);
        regs.Write32Broadcast(0, RegHal::MULTICAST, MODS_TREX_HW_CTRL, data);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoWriteTrexRam
(
    UINT32 linkId,
    LwLinkTrex::TrexRam ram,
    const vector<LwLinkTrex::RamEntry>& entries
)
{
    RC rc;
    RegHal& regs = Regs();

    if (entries.size() == 1)
    {
        Printf(Tee::PriError, "TREX RAM must specify at least 2 entries\n");
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(LimerockLwLink::DoWriteTrexRam(linkId, ram, entries));

    if (ram == LwLinkTrex::TR_REQ0)
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAM_VLD_RAM0, entries.size()-1);
    else
        regs.Write32(linkId, MODS_TREX_DYNAMIC_MODE_REQ_RAM_VLD_RAM1, entries.size()-1);

    return rc;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoSetQueueRepeatCount(UINT32 linkId, UINT32 repeatCount)
{
    MASSERT(repeatCount > 0);
    RegHal& regs = Regs();

    // Minus 1 because repeatCount is 0-based
    regs.Write32(linkId, MODS_TREX_QUEUE_CTRL_REPEAT_COUNT, repeatCount - 1);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoGetResidencyCounts(UINT32 linkId, UINT32 mcId, UINT64* pMcCount, UINT64* pRedCount)
{
    MASSERT(pMcCount);
    MASSERT(pRedCount);
    RegHal& regs = Regs();

    UINT32 index = mcId * regs.LookupValue(MODS_MULTICASTTSTATE_STAT_RESIDENCY_COUNT_CTRL_INDEX_MCID_STRIDE);

    UINT32 data = 0;
    data = regs.SetField(MODS_MULTICASTTSTATE_STAT_RESIDENCY_COUNT_CTRL_INDEX, index) |
           regs.SetField(MODS_MULTICASTTSTATE_STAT_RESIDENCY_COUNT_CTRL_AUTOINCR_ON);
    regs.Write32(linkId, MODS_MULTICASTTSTATE_STAT_RESIDENCY_COUNT_CTRL, data);

    *pMcCount = static_cast<UINT64>(regs.Read32(linkId, MODS_MULTICASTTSTATE_STAT_RESIDENCY_COUNT_DATA_COUNT));
    *pMcCount |= (static_cast<UINT64>(regs.Read32(linkId, MODS_MULTICASTTSTATE_STAT_RESIDENCY_COUNT_DATA_COUNT)) << 32);

    data = regs.SetField(MODS_REDUCTIONTSTATE_STAT_RESIDENCY_COUNT_CTRL_INDEX, index) |
           regs.SetField(MODS_REDUCTIONTSTATE_STAT_RESIDENCY_COUNT_CTRL_AUTOINCR_ON);
    regs.Write32(linkId, MODS_REDUCTIONTSTATE_STAT_RESIDENCY_COUNT_CTRL, data);

    *pRedCount = static_cast<UINT64>(regs.Read32(linkId, MODS_REDUCTIONTSTATE_STAT_RESIDENCY_COUNT_DATA_COUNT));
    *pRedCount |= (static_cast<UINT64>(regs.Read32(linkId, MODS_REDUCTIONTSTATE_STAT_RESIDENCY_COUNT_DATA_COUNT)) << 32);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoGetResponseCheckCounts(UINT32 linkId, UINT64* pPassCount, UINT64* pFailCount)
{
    MASSERT(pPassCount);
    MASSERT(pFailCount);
    RegHal& regs = Regs();

    *pPassCount = static_cast<UINT64>(regs.Read32(linkId, MODS_TREX_SW_RSP_CHECK_PASS_CNT_LOW_COUNT)) |
                 (static_cast<UINT64>(regs.Read32(linkId, MODS_TREX_SW_RSP_CHECK_PASS_CNT_HIGH_COUNT)) << 32);

    *pFailCount = static_cast<UINT64>(regs.Read32(linkId, MODS_TREX_SW_RSP_CHECK_FAIL_CNT_LOW_COUNT)) |
                 (static_cast<UINT64>(regs.Read32(linkId, MODS_TREX_SW_RSP_CHECK_FAIL_CNT_HIGH_COUNT)) << 32);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoResetResponseCheckCounts(UINT32 linkId)
{
    RC rc;
    RegHal& regs = Regs();

    CHECK_RC(regs.Write32Priv(linkId, MODS_TREX_SW_RSP_CHECK_PASS_CNT_LOW_COUNT, 0));
    CHECK_RC(regs.Write32Priv(linkId, MODS_TREX_SW_RSP_CHECK_PASS_CNT_HIGH_COUNT, 0));
    CHECK_RC(regs.Write32Priv(linkId, MODS_TREX_SW_RSP_CHECK_FAIL_CNT_LOW_COUNT, 0));
    CHECK_RC(regs.Write32Priv(linkId, MODS_TREX_SW_RSP_CHECK_FAIL_CNT_HIGH_COUNT, 0));

    return rc;
}

 //-----------------------------------------------------------------------------
RC LagunaLwLink::DoStartResidencyTimers(UINT32 linkId)
{
    RegHal& regs = Regs();

    regs.Write32(linkId, MODS_MULTICASTTSTATE_STAT_RESIDENCY_BIN_CTRL_LOW_LIMIT, 0xFFFFFF);
    regs.Write32(linkId, MODS_MULTICASTTSTATE_STAT_RESIDENCY_BIN_CTRL_HIGH_LIMIT, 0xFFFFFF);
    regs.Write32(linkId, MODS_REDUCTIONTSTATE_STAT_RESIDENCY_BIN_CTRL_LOW_LIMIT, 0xFFFFFF);
    regs.Write32(linkId, MODS_REDUCTIONTSTATE_STAT_RESIDENCY_BIN_CTRL_HIGH_LIMIT, 0xFFFFFF);

    UINT32 data = 0;
    data = regs.SetField(MODS_MULTICASTTSTATE_STAT_RESIDENCY_CONTROL_ENABLE_TIMER_ENABLE) |
           regs.SetField(MODS_MULTICASTTSTATE_STAT_RESIDENCY_CONTROL_SNAP_ON_DEMAND_DISABLE);
    regs.Write32(linkId, MODS_MULTICASTTSTATE_STAT_RESIDENCY_CONTROL, data);

    data = regs.SetField(MODS_REDUCTIONTSTATE_STAT_RESIDENCY_CONTROL_ENABLE_TIMER_ENABLE) |
           regs.SetField(MODS_REDUCTIONTSTATE_STAT_RESIDENCY_CONTROL_SNAP_ON_DEMAND_DISABLE);
    regs.Write32(linkId, MODS_REDUCTIONTSTATE_STAT_RESIDENCY_CONTROL, data);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LagunaLwLink::DoStopResidencyTimers(UINT32 linkId)
{
    RegHal& regs = Regs();

    UINT32 data = 0;
    data = regs.SetField(MODS_MULTICASTTSTATE_STAT_RESIDENCY_CONTROL_ENABLE_TIMER_DISABLE) |
           regs.SetField(MODS_MULTICASTTSTATE_STAT_RESIDENCY_CONTROL_SNAP_ON_DEMAND_ENABLE);
    regs.Write32(linkId, MODS_MULTICASTTSTATE_STAT_RESIDENCY_CONTROL, data);

    data = regs.SetField(MODS_REDUCTIONTSTATE_STAT_RESIDENCY_CONTROL_ENABLE_TIMER_DISABLE) |
           regs.SetField(MODS_REDUCTIONTSTATE_STAT_RESIDENCY_CONTROL_SNAP_ON_DEMAND_ENABLE);
    regs.Write32(linkId, MODS_REDUCTIONTSTATE_STAT_RESIDENCY_CONTROL, data);

    return RC::OK;
}

// LwLinkIobist implementation
//------------------------------------------------------------------------------
void LagunaLwLink::DoGetIobistErrorFlags
(
    UINT64 linkMask,
    set<LwLinkIobist::ErrorFlag> * pErrorFlags
) const
{
    const RegHal& regs = Regs();
    INT32 lwrLinkId = Utility::BitScanForward(linkMask);
    for ( ; lwrLinkId != -1; lwrLinkId = Utility::BitScanForward(linkMask, lwrLinkId + 1))
    {
        UINT32 errCtrl = regs.Read32(lwrLinkId, MODS_LWLDL_RX_RXSLSM_ERR_CNTL);
        if (regs.TestField(errCtrl, MODS_LWLDL_RX_RXSLSM_ERR_CNTL_IOBIST_ECC_ALIGN_DONE_ERR, 1))
        {
            pErrorFlags->insert({ static_cast<UINT32>(lwrLinkId),
                                  LwLinkIobist::ErrorFlagType::AlignDone });
        }
        if (regs.TestField(errCtrl, MODS_LWLDL_RX_RXSLSM_ERR_CNTL_IOBIST_ECC_ALIGN_LOCK_ERR, 1))
        {
            pErrorFlags->insert({ static_cast<UINT32>(lwrLinkId),
                                  LwLinkIobist::ErrorFlagType::AlignLock });
        }
        if (regs.TestField(errCtrl, MODS_LWLDL_RX_RXSLSM_ERR_CNTL_IOBIST_ECC_SCRAM_LOCK_ERR, 1))
        {
            pErrorFlags->insert({ static_cast<UINT32>(lwrLinkId),
                                  LwLinkIobist::ErrorFlagType::ScramLock });
        }
    }
}

//------------------------------------------------------------------------------
void LagunaLwLink::DoSetIobistInitiator(UINT64 linkMask, bool bInitiator)
{
    RegHal& regs = Regs();
    INT32 lwrLinkId = Utility::BitScanForward(linkMask);
    for ( ; lwrLinkId != -1; lwrLinkId = Utility::BitScanForward(linkMask, lwrLinkId + 1))
    {
        UINT32 linkModeCtrl = regs.Read32(lwrLinkId, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL);
        regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_5,
                      bInitiator ? 1 : 0);
        regs.Write32(lwrLinkId, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL, linkModeCtrl);
    }
}

//------------------------------------------------------------------------------
RC LagunaLwLink::DoSetIobistTime(UINT64 linkMask, IobistTime iobistTime)
{
    MASSERT(linkMask);

    RC rc;
    RegHal& regs = Regs();

    INT32 lwrLinkId = Utility::BitScanForward(linkMask);
    for ( ; lwrLinkId != -1; lwrLinkId = Utility::BitScanForward(linkMask, lwrLinkId + 1))
    {
        if (!IsLinkAcCoupled(lwrLinkId) &&
            ((iobistTime == IobistTime::Iobt1s) || ((iobistTime == IobistTime::Iobt10s))))
        {
            Printf(Tee::PriError,
                   "IOBIST times of 1s or 10s not supported on link %d because it is not AC "
                   "coupled\n",
                   lwrLinkId);
            return RC::BAD_PARAMETER;
        }

        UINT32 linkModeCtrl = regs.Read32(lwrLinkId, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL);
        switch (iobistTime)
        {
            default:
            case IobistTime::Iobt20us:
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_6, 0);
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_7, 0);
                break;
            case IobistTime::Iobt800us:
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_6, 1);
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_7, 0);
                break;
            case IobistTime::Iobt1s:
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_6, 0);
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_7, 1);
                break;
            case IobistTime::Iobt10s:
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_6, 1);
                regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_7, 1);
                break;
        }
        regs.Write32(lwrLinkId, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL, linkModeCtrl);
    }
    return rc;
}

//------------------------------------------------------------------------------
void LagunaLwLink::DoSetIobistType(UINT64 linkMask, IobistType iobistType)
{
    RegHal& regs = Regs();
    INT32 lwrLinkId = Utility::BitScanForward(linkMask);
    for ( ; lwrLinkId != -1; lwrLinkId = Utility::BitScanForward(linkMask, lwrLinkId + 1))
    {
        UINT32 linkModeCtrl = regs.Read32(lwrLinkId, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL);
        if (iobistType == IobistType::PreTrain)
        {
            regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_3, 1);
            regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_4, 0);
        }
        else if (iobistType == IobistType::PostTrain)
        {
            regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_3, 0);
            regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_4, 1);
        }
        else
        {
            regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_3, 0);
            regs.SetField(&linkModeCtrl, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL_RESERVED_4, 0);
        }
        regs.Write32(lwrLinkId, MODS_LWLIPT_LNK_CTRL_SW_LINK_MODE_CTRL, linkModeCtrl);
    }
}

//------------------------------------------------------------------------------
RC LagunaLwLink::GetEomConfigValue(FomMode mode, UINT32 numErrors, UINT32 numBlocks, UINT32 *pConfigVal) const
{
    UINT32 pamEyeSel = 0x0;
    UINT32 berEyeSel = 0x0;
    switch (mode)
    {
        case EYEO_Y:
            pamEyeSel = LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_ALL;
            berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_ALL;
            break;
        case EYEO_Y_U:
            pamEyeSel = LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_UPPER;
            berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_UPPER;
            break;
        case EYEO_Y_M:
            pamEyeSel = LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_MID;
            berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_MID;
            break;
        case EYEO_Y_L:
            pamEyeSel = LW_MINION_UCODE_CONFIGEOM_PAM_EYE_SEL_LOWER;
            berEyeSel = LW_MINION_UCODE_CONFIGEOM_BER_EYE_SEL_LOWER;
            break;
        default:
            Printf(Tee::PriError, "Invalid or unsupported FOM mode : %d\n", mode);
            return RC::ILWALID_ARGUMENT;
    }

    *pConfigVal = DRF_NUM(_MINION_UCODE, _CONFIGEOM, _EOM_MODE,    LW_MINION_UCODE_CONFIGEOM_EOM_MODE_FOM)  |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _FOM_MODE,    LW_MINION_UCODE_CONFIGEOM_FOM_MODE_EYEO) |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _NUM_BLKS,    numBlocks) |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _NUM_ERRS,    numErrors) |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _PAM_EYE_SEL, pamEyeSel) |
                  DRF_NUM(_MINION_UCODE, _CONFIGEOM, _BER_EYE_SEL, berEyeSel);
    return RC::OK;
}

//------------------------------------------------------------------------------
UINT32 LagunaLwLink::GetErrorCounterControlMask(bool bIncludePerLane)
{
    UINT32 counterMask = LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT    |
                         LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_REPLAY      |
                         LWSWITCH_LWLINK_COUNTER_DL_TX_ERR_RECOVERY    |
                         LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_REPLAY |
                         LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_MASKED |
                         LWSWITCH_LWLINK_COUNTER_PHY_REFRESH_PASS |
                         LWSWITCH_LWLINK_COUNTER_PHY_REFRESH_FAIL;
    if (bIncludePerLane)
    {
        counterMask |= LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0 |
                       LWSWITCH_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1;
    }
    return counterMask;
}

//-----------------------------------------------------------------------------
/* virtual */ RC LagunaLwLink::DoGetEomStatus
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
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_MCAST_RX_EOM_EN_OFF);

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
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_MCAST_RX_EOM_OVRD_ON);

    // Set EOM enabled
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_MCAST_RX_EOM_EN_ON);

    // Check if EOM is done
    doneStatus = 0x1;
    CHECK_RC(Tasker::PollHw(timeoutMs, pollEomDone, __FUNCTION__));

    // Set EOM disabled
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_MCAST_RX_EOM_EN_OFF);

    // Set EOM not programmable
    regs.Write32(link, MODS_LWLPHYCTL_LANE_PAD_CTL_8_MCAST_RX_EOM_OVRD_OFF);

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

//------------------------------------------------------------------------------
RC LagunaLwLink::DoSetCollapsedResponses(UINT32 mask)
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
