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

#include "hopperlwlink.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_devif.h"

#include "ctrl/ctrl2080.h"

using namespace LwLinkDevIf;

//------------------------------------------------------------------------------
//! \brief Clear the error counts for the specified link id
//!
//! \param linkId  : Link id to clear counts on
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC HopperLwLink::DoClearErrorCounts(UINT32 linkId)
{
    RC rc;
    LW2080_CTRL_LWLINK_GET_REFRESH_COUNTERS_PARAMS phyRefreshParams = { };
    phyRefreshParams.linkMask = 1 << linkId;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_REFRESH_COUNTERS,
                                           &phyRefreshParams,
                                           sizeof(phyRefreshParams)));

    MASSERT(m_CachedErrorCounts.size() > linkId);
    Tasker::MutexHolder lockCache(m_ErrorCounterMutex);
    m_CachedErrorCounts[linkId] = ErrorCounts();
    return rc;
}

#include "gpu/include/testdevice.h"
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

//------------------------------------------------------------------------------
/* virtual */ RC HopperLwLink::DoGetBlockCodeMode
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
FLOAT64 HopperLwLink::DoGetDefaultErrorThreshold
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
void HopperLwLink::DoGetEomDefaultSettings
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
RC HopperLwLink::DoGetErrorCounts(UINT32 linkId, LwLink::ErrorCounts * pErrorCounts)
{
    MASSERT(pErrorCounts);
    RC rc;
    CHECK_RC(AmpereLwLink::DoGetErrorCounts(linkId, pErrorCounts));

    LW2080_CTRL_LWLINK_GET_REFRESH_COUNTERS_PARAMS phyRefreshParams = { };
    phyRefreshParams.linkMask = 1 << linkId;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_GET_REFRESH_COUNTERS,
                                           &phyRefreshParams,
                                           sizeof(phyRefreshParams)));

    CHECK_RC(pErrorCounts->SetCount(ErrorCounts::LWL_UPHY_REFRESH_FAIL_ID,
                                    phyRefreshParams.refreshCount[linkId].failCount,
                                    false));
    CHECK_RC(pErrorCounts->SetCount(ErrorCounts::LWL_UPHY_REFRESH_PASS_ID,
                                    phyRefreshParams.refreshCount[linkId].passCount,
                                    false));

    {
        Tasker::MutexHolder lockCache(m_ErrorCounterMutex);
        MASSERT(m_CachedErrorCounts.size() > linkId);
        m_CachedErrorCounts[linkId] += *pErrorCounts;
        *pErrorCounts = m_CachedErrorCounts[linkId];
    }

    return rc;
}

//------------------------------------------------------------------------------
/* virtual */ RC HopperLwLink::DoGetLineCode(UINT32 linkId, SystemLineCode *pLineCode)
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
RC HopperLwLink::DoInitialize(LibDevHandles handles)
{
    RC rc;

    CHECK_RC(AmpereLwLink::DoInitialize(handles));

    m_ErrorCounterMutex = Tasker::CreateMutex("LwLink Error Cache Mutex",
                                             (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM) ?
                                                 Tasker::mtxUnchecked : Tasker::mtxLast);
    m_CachedErrorCounts.resize(GetMaxLinks());
    return rc;
}

//------------------------------------------------------------------------------
RC HopperLwLink::DoPerLaneErrorCountsEnabled
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
/* virtual */ RC HopperLwLink::DoGetLinkPowerStateStatus
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

    // Hopper doesn't distinguish RX/TX LP states
    pLinkPowerStatus->txHwControlledPowerState = pLinkPowerStatus->rxHwControlledPowerState;
    pLinkPowerStatus->txSubLinkLwrrentPowerState = pLinkPowerStatus->rxSubLinkLwrrentPowerState;
    pLinkPowerStatus->txSubLinkConfigPowerState = pLinkPowerStatus->rxSubLinkConfigPowerState;

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */ RC HopperLwLink::DoRequestPowerState
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
/* virtual */ UINT32 HopperLwLink::DoGetLowPowerEntryTimeMs(UINT32 linkId, LwLink::SubLinkDir dir) const
{
    if (!IsLinkActive(linkId))
        return 0;

    const RegHal& regs = Regs();

    const FLOAT64 thresholdUnitUs = 100.0;
    const FLOAT64 threshold = regs.Read32(linkId, MODS_LWLIPT_LNK_PWRM_L1_ENTER_THRESHOLD_THRESHOLD);

    return static_cast<UINT32>((threshold * thresholdUnitUs) / 1000.0);
}

//------------------------------------------------------------------------------
RC HopperLwLink::DoStartPowerStateToggle(UINT32 linkId, UINT32 in, UINT32 out)
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
RC HopperLwLink::DoClearLPCounts(UINT32 linkId)
{
    RC rc;

    LW2080_CTRL_LWLINK_CLEAR_COUNTERS_PARAMS clearParams = {};
    clearParams.linkMask = (1LLU << linkId);
    clearParams.counterMask = LW2080_CTRL_LWLINK_LP_COUNTERS_DL;

    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(),
                                           LW2080_CTRL_CMD_LWLINK_CLEAR_COUNTERS,
                                           &clearParams,
                                           sizeof(clearParams)));

    return rc;
}

//-----------------------------------------------------------------------------
RC HopperLwLink::DoGetLPEntryOrExitCount
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

    LW2080_CTRL_LWLINK_GET_LP_COUNTERS_PARAMS lpParams = {};
    lpParams.linkId = linkId;
    lpParams.counterValidMask = (1 << LW2080_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_ENTER) |
                                (1 << LW2080_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_EXIT);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetGpuSubdevice(), LW2080_CTRL_CMD_LWLINK_GET_LP_COUNTERS,
                                           &lpParams, sizeof(lpParams)));

    if (entry)
    {
        MASSERT(lpParams.counterValidMask & (1 << LW2080_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_ENTER));
        *pCount = lpParams.counterValues[LW2080_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_ENTER];
        *pbOverflow = (*pCount == ~0U);
    }
    else
    {
        MASSERT(lpParams.counterValidMask & (1 << LW2080_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_EXIT));
        *pCount = lpParams.counterValues[LW2080_CTRL_LWLINK_GET_LP_COUNTERS_NUM_TX_LP_EXIT];
        *pbOverflow = (*pCount == ~0U);
    }

    return rc;
}

//-----------------------------------------------------------------------------
UINT32 HopperLwLink::DoGetRecalStartTimeUs(UINT32 linkId) const
{
    const RegHal& regs = Regs();
    return regs.Read32(linkId, MODS_LWLDL_RX_PHYCFG_RX_1_RECAL_START) << regs.Read32(linkId, MODS_LWLDL_RX_PHYCFG_RX_1_RECAL_START_SCALE);
}

UINT32 HopperLwLink::DoGetRecalWakeTimeUs(UINT32 linkId) const
{
    const RegHal& regs = Regs();
    return regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_L1WAKE) << regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_L1WAKE_SCL);
}

//-----------------------------------------------------------------------------
UINT32 HopperLwLink::DoGetMinRecalTimeUs(UINT32 linkId) const
{
    const RegHal& regs = Regs();
    return regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_MINCAL) << regs.Read32(linkId, MODS_LWLDL_RX_RX_L1RECAL_CTRL2_MINCAL_SCL);
}

// LwLinkIobist implementation
//------------------------------------------------------------------------------
void HopperLwLink::DoGetIobistErrorFlags
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
void HopperLwLink::DoSetIobistInitiator(UINT64 linkMask, bool bInitiator)
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
RC HopperLwLink::DoSetIobistTime(UINT64 linkMask, IobistTime iobistTime)
{
    MASSERT(linkMask);

    RC rc;
    RegHal& regs = Regs();

    INT32 lwrLinkId = Utility::BitScanForward(linkMask);
    for ( ; lwrLinkId != -1; lwrLinkId = Utility::BitScanForward(linkMask, lwrLinkId + 1))
    {
        if (!IsLinkAcCoupled(lwrLinkId) &&
            ((iobistTime == IobistTime::Iobt1s) || (iobistTime == IobistTime::Iobt10s)))
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
void HopperLwLink::DoSetIobistType(UINT64 linkMask, IobistType iobistType)
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
UINT32 HopperLwLink::DoGetMaxRegBlocks(RegBlock regBlock) const
{
    // PLLs are no longer associated with IOCTRLs starting on GH100.  There are 2 PLLs shared
    // across all IOCTRLs
    if (regBlock == RegBlock::CLN)
        return 2;

    return GetMaxLinks();
}

//------------------------------------------------------------------------------
RC HopperLwLink::DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive)
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
        // Block index 0 covers links 0 through 11, block index 1 covers links
        // 12 through 17, this is static and there are no HW defines that can be
        // used to get this information
        const UINT32 firstLink = blockIdx ? 12 : 0;
        const UINT32 lastLink = blockIdx ? 17 : 11;

        *pbActive = false;
        for (UINT32 lwrLink = firstLink; lwrLink <= lastLink; lwrLink++)
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
RC HopperLwLink::DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
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
RC HopperLwLink::DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
{
    RegHal &regs = Regs();

    if (!regs.HasRWAccess(ioctl, MODS_PTRIM_LWLINK_UPHY0_PLL1_CTRL4))
    {
        Printf(Tee::PriWarn,
               "LwLink UPHY PLL registers may not be read due to priv protection\n");
        return RC::PRIV_LEVEL_VIOLATION;
    }

    ModsGpuRegAddress pllCtrlReg = ioctl ? MODS_PTRIM_LWLINK_UPHY0_PLL1_CTRL4 :
                                           MODS_PTRIM_LWLINK_UPHY0_PLL0_CTRL4;
    ModsGpuRegField pllData = ioctl ? MODS_PTRIM_LWLINK_UPHY0_PLL1_CTRL5_CFG_RDATA :
                                      MODS_PTRIM_LWLINK_UPHY0_PLL0_CTRL5_CFG_RDATA;

    UINT32 pllCtl4 = regs.Read32(pllCtrlReg);
    regs.SetField(&pllCtl4, MODS_PTRIM_LWLINK_UPHY0_PLL1_CTRL4_CFG_ADDR, addr);
    regs.SetField(&pllCtl4, MODS_PTRIM_LWLINK_UPHY0_PLL1_CTRL4_CFG_RDS_ON);
    regs.Write32(pllCtrlReg, pllCtl4);
    *pData = static_cast<UINT16>(regs.Read32(pllData));
    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ bool HopperLwLink::DoSupportsFomMode(FomMode mode) const
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
RC HopperLwLink::GetEomConfigValue
(
    FomMode mode,
    UINT32 numErrors,
    UINT32 numBlocks,
    UINT32 *pConfigVal
) const
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

//-----------------------------------------------------------------------------
RC HopperLwLink::DoGetEomStatus
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
UINT32 HopperLwLink::GetErrorCounterControlMask(bool bIncludePerLane)
{
    UINT32 counterMask = LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_FLIT    |
                         LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_REPLAY      |
                         LW2080_CTRL_LWLINK_COUNTER_DL_TX_ERR_RECOVERY    |
                         LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_REPLAY      |
                         LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_MASKED;
    if (bIncludePerLane)
    {
        counterMask |= LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L0 |
                       LW2080_CTRL_LWLINK_COUNTER_DL_RX_ERR_CRC_LANE_L1;
    }
    return counterMask;
}


