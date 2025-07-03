/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlink.h"
#include "ctrl/ctrl2080/ctrl2080lwlink.h"

const LwLink::LibDevHandles_t LwLink::NO_HANDLES = {};

//-----------------------------------------------------------------------------
/* static */ LwLink::LinkState LwLink::RmLinkStateToLinkState(UINT32 rmState)
{
    switch (rmState)
    {
        case LW2080_CTRL_LWLINK_STATUS_LINK_STATE_INIT:
            return LS_INIT;
        case LW2080_CTRL_LWLINK_STATUS_LINK_STATE_HWCFG:
            return LS_HWCFG;
        case LW2080_CTRL_LWLINK_STATUS_LINK_STATE_SWCFG:
            return LS_SWCFG;
        case LW2080_CTRL_LWLINK_STATUS_LINK_STATE_ACTIVE:
            return LS_ACTIVE;
        case LW2080_CTRL_LWLINK_STATUS_LINK_STATE_FAULT:
            return LS_FAULT;
        case 0x5: // TODO: Update with the actual RM define
            return LS_SLEEP;
        case LW2080_CTRL_LWLINK_STATUS_LINK_STATE_RECOVERY:
            return LS_RECOVERY;
        case LW2080_CTRL_LWLINK_STATUS_LINK_STATE_ILWALID:
            return LS_ILWALID;
        default:
            break;
    }
    Printf(Tee::PriError, "Unknown rm link state %u\n", rmState);
    return LS_ILWALID;
}

//-----------------------------------------------------------------------------
/* static */ LwLink::SubLinkState LwLink::RmSubLinkStateToLinkState(UINT32 rmState)
{
    switch (rmState)
    {
        case LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_HIGH_SPEED_1:
            return SLS_HIGH_SPEED;
        case LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_SINGLE_LANE:
            return SLS_SINGLE_LANE;
        case LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_TRAINING:
            return SLS_TRAINING;
        case LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_SAFE_MODE:
            return SLS_SAFE_MODE;
        case LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_OFF:
            return SLS_OFF;
        case LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_ILWALID:
            return SLS_ILWALID;
        default:
            break;
    }
    Printf(Tee::PriError, "Unknown rm sublink state %u\n", rmState);
    return SLS_ILWALID;
}

//-----------------------------------------------------------------------------
/* static */ string LwLink::LinkStateToString(LinkState state)
{
    switch (state)
    {
        case LS_INIT:
            return "Init";
        case LS_HWCFG:
            return "Hardware Config";
        case LS_SWCFG:
            return "Software Config";
        case LS_ACTIVE:
            return "Active";
        case LS_FAULT:
            return "Fault";
        case LS_SLEEP:
            return "Sleep";
        case LS_RECOVERY:
            return "Recovery";
        case LS_ILWALID:
            return "Invalid";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ string LwLink::SubLinkStateToString(SubLinkState state)
{
    switch (state)
    {
        case SLS_OFF:
            return "Off";
        case SLS_SAFE_MODE:
            return "Safe Mode";
        case SLS_TRAINING:
            return "Training";
        case SLS_SINGLE_LANE:
            return "Single Lane";
        case SLS_HIGH_SPEED:
            return "High Speed";
        case SLS_ILWALID:
            return "Invalid";
        default:
            break;
    }
    return "Unknown";
}

//------------------------------------------------------------------------------
/* static */ string LwLink::TransferTypeString(TransferType t)
{
    switch (t)
    {
        case TT_UNIDIR_READ:
            return "Unidirectional Read";
        case TT_UNIDIR_WRITE:
            return "Unidirectional Write";
        case TT_BIDIR_READ:
            return "Bidirectional Read";
        case TT_BIDIR_WRITE:
            return "Bidirectional Write";
        case TT_READ_WRITE:
            return "Read/Write";
        default:
            break;
    }

    return "UNKNOWN";
}

//------------------------------------------------------------------------------
/* static */ string LwLink::SystemParameterString(SystemParameter sysParam)
{
    switch (sysParam)
    {
        case SystemParameter::RESTORE_PHY_PARAMS:
            return "PHY Parameter Restore";
        case SystemParameter::SL_ENABLE:
            return "Single Lane Enable";
        case SystemParameter::L2_ENABLE:
            return "L2 Enable";
        case SystemParameter::LINE_RATE:
            return "Line Rate";
        case SystemParameter::LINE_CODE_MODE:
            return "Line Code Mode";
        case SystemParameter::LINK_DISABLE:
            return "Link Disable";
        case SystemParameter::TXTRAIN_OPT_ALGO:
            return "TX Training Optimization Algorithm";
        case SystemParameter::BLOCK_CODE_MODE:
            return "Block Code Mode";
        case SystemParameter::LINK_INIT_DISABLE:
            return "Link Initialization Disable";
        case SystemParameter::ALI_ENABLE:
            return "ALI Enable";
        case SystemParameter::TXTRAIN_MIN_TRAIN_TIME_MANTISSA:
            return "TX Minimum Train Time Mantissa";
        case SystemParameter::TXTRAIN_MIN_TRAIN_TIME_EXPONENT:
            return "TX Minimum Train Time Exponent";
        case SystemParameter::L1_MIN_RECAL_TIME_MANTISSA:
            return "L1 Minimum Recal Time Mantissa";
        case SystemParameter::L1_MIN_RECAL_TIME_EXPONENT:
            return "L1 Minimum Recal Time Exponent";
        case SystemParameter::L1_MAX_RECAL_PERIOD_MANTISSA:
            return "L1 Maximum Recal Period Mantissa";
        case SystemParameter::L1_MAX_RECAL_PERIOD_EXPONENT:
            return "L1 Maximum Recal Period Exponent";
        case SystemParameter::DISABLE_UPHY_UCODE_LOAD:
            return "Disable UPHY uCode Load";
        case SystemParameter::L1_ENABLE:
            return "L1 Enable";
        default:
            break;
    }
    return "UNKNOWN";
}

//-----------------------------------------------------------------------------
/* static */ string LwLink::BlockCodeModeToString(SystemBlockCodeMode blockMode)
{
    switch (blockMode)
    {
        case SystemBlockCodeMode::OFF   :
            return "ECC OFF";
        case SystemBlockCodeMode::ECC96 :
            return "ECC96";
        case SystemBlockCodeMode::ECC88 :
            return "ECC88";
        case SystemBlockCodeMode::ECC89 :
            return "ECC89";
        default:
            MASSERT(!"Unknown system block code mode");
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ string LwLink::LineCodeToString(SystemLineCode lineCode)
{
    switch (lineCode)
    {
        case SystemLineCode::NRZ :
            return "NRZ";
        case SystemLineCode::NRZ_128B130 :
            return "NRZ_128B130";
        case SystemLineCode::PAM4 :
            return "PAM4";
        default:
            MASSERT(!"Unknown system line code");
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* static */ const char * LwLink::NearEndLoopModeToString(NearEndLoopMode nelMode)
{
    switch (nelMode)
    {
        case NearEndLoopMode::NEA :
            return "NEA";
        case NearEndLoopMode::NED :
            return "NED";
        case NearEndLoopMode::None :
            return "None";
        default:
            MASSERT(!"Unknown ");
            break;
    }
    return "Unknown";
}

//------------------------------------------------------------------------------
// LwLink::ErrorCounts Implementation
//------------------------------------------------------------------------------

/* static */ map<LwLink::ErrorCounts::ErrId, LwLink::ErrorCounts::ErrIdDescriptor> LwLink::ErrorCounts::s_ErrorDescriptors = //$
{
    { LWL_RX_CRC_FLIT_ID,           ErrIdDescriptor("RxCrcFlit",           "rx_crc_flit",       true,  false, false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_RX_CRC_LANE0_ID,          ErrIdDescriptor("PhysLane0RxCrc",      "rx_crc_lane",       false, true,  false, 0) }, //$
    { LWL_RX_CRC_LANE1_ID,          ErrIdDescriptor("PhysLane1RxCrc",      "rx_crc_lane",       false, true,  false, 1) }, //$
    { LWL_RX_CRC_LANE2_ID,          ErrIdDescriptor("PhysLane2RxCrc",      "rx_crc_lane",       false, true,  false, 2) }, //$
    { LWL_RX_CRC_LANE3_ID,          ErrIdDescriptor("PhysLane3RxCrc",      "rx_crc_lane",       false, true,  false, 3) }, //$
    { LWL_RX_CRC_LANE4_ID,          ErrIdDescriptor("PhysLane4RxCrc",      "rx_crc_lane",       false, true,  false, 4) }, //$
    { LWL_RX_CRC_LANE5_ID,          ErrIdDescriptor("PhysLane5RxCrc",      "rx_crc_lane",       false, true,  false, 5) }, //$
    { LWL_RX_CRC_LANE6_ID,          ErrIdDescriptor("PhysLane6RxCrc",      "rx_crc_lane",       false, true,  false, 6) }, //$
    { LWL_RX_CRC_LANE7_ID,          ErrIdDescriptor("PhysLane7RxCrc",      "rx_crc_lane",       false, true,  false, 7) }, //$
    { LWL_RX_CRC_MASKED_ID,         ErrIdDescriptor("RxCrcMasked",         "rx_crc_masked",     false, true,  false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_TX_REPLAY_ID,             ErrIdDescriptor("TxReplay",            "tx_replay",         false, false, false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_RX_REPLAY_ID,             ErrIdDescriptor("RxReplay",            "rx_replay",         false, false, false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_TX_RECOVERY_ID,           ErrIdDescriptor("TxRecovery",          "tx_recovery",       true,  true,  false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_SW_RECOVERY_ID,           ErrIdDescriptor("SwRecovery",          "sw_recovery",       true,  true,  false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_UPHY_REFRESH_FAIL_ID,     ErrIdDescriptor("UphyRefreshFail",     "uphy_refresh_fail", true,  true,  false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_PRE_FEC_ID,               ErrIdDescriptor("Pre-FEC",             "pre_fec",           true,  false, false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_ECC_DEC_FAILED_ID,        ErrIdDescriptor("EccDecFailed",        "ecc_dec_failed",    false, true,  false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_UPHY_REFRESH_PASS_ID,     ErrIdDescriptor("UphyRefreshPass",     "uphy_refresh_pass", false, true,  false, LwLink::ErrorCounts::LWLINK_ILWALID_LANE) }, //$
    { LWL_FEC_CORRECTIONS_LANE0_ID, ErrIdDescriptor("PhysLane0FecCorrections ", "fec_lane_corrections", false, true, false, 0) }, //$
    { LWL_FEC_CORRECTIONS_LANE1_ID, ErrIdDescriptor("PhysLane1FecCorrections ", "fec_lane_corrections", false, true, false, 1) }, //$
    { LWL_FEC_CORRECTIONS_LANE2_ID, ErrIdDescriptor("PhysLane2FecCorrections ", "fec_lane_corrections", false, true, false, 2) }, //$
    { LWL_FEC_CORRECTIONS_LANE3_ID, ErrIdDescriptor("PhysLane3FecCorrections ", "fec_lane_corrections", false, true, false, 3) }  //$
};

/* static */ const UINT32 LwLink::ErrorCounts::LWLINK_ILWALID_LANE = ~0U;

//------------------------------------------------------------------------------
//! \brief Constructor
LwLink::ErrorCounts::ErrorCounts()
{
    m_ErrorCounts.resize(LWL_NUM_ERR_COUNTERS);
}

//------------------------------------------------------------------------------
//! \brief Get the count associated with an error id
//!
//! \param errId : Error id to get the count for
//!
//! \return Error count for the specified ID
UINT64 LwLink::ErrorCounts::GetCount(const ErrId errId) const
{
    MASSERT(static_cast<size_t>(errId) < m_ErrorCounts.size());
    return m_ErrorCounts[errId].countValue;
}

//------------------------------------------------------------------------------
//! \brief Get whether the error count is valid for a specific ID
//!
//! \param errId : Error id to get validity for
//!
//! \return true if the ID has a valid count, false otherwise
bool LwLink::ErrorCounts::IsValid(const ErrId errId) const
{
    MASSERT(static_cast<size_t>(errId) < m_ErrorCounts.size());
    return m_ErrorCounts[errId].bValid;
}

//------------------------------------------------------------------------------
bool LwLink::ErrorCounts::DidCountOverflow(const ErrId errId) const
{
    MASSERT(static_cast<size_t>(errId) < m_ErrorCounts.size());
    return m_ErrorCounts[errId].bOverflowed;
}

//------------------------------------------------------------------------------
//! \brief Set the count for a specific error ID
//!
//! \param errId : Error id to set the count on
//! \param val   : Value to set the count to
//!
//! \return OK if successful, not OK otherwise
RC LwLink::ErrorCounts::SetCount(const ErrId errId, const UINT32 val, bool bOverflowed)
{
    if (static_cast<size_t>(errId) >= m_ErrorCounts.size())
    {
        Printf(Tee::PriError,
               "Invalid LwLink counter Id : %d\n",
               errId);
        return RC::BAD_PARAMETER;
    }
    m_ErrorCounts[errId].bValid      = true;
    m_ErrorCounts[errId].countValue  = val;
    m_ErrorCounts[errId].bOverflowed = bOverflowed;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Allow checking for error count objects for inequality
//!
//! \param rhs : error count object to compare against
//!
//! \return true if not equal, false otherwise
bool LwLink::ErrorCounts::operator!=(const LwLink::ErrorCounts &rhs) const
{
    MASSERT(m_ErrorCounts.size() == rhs.m_ErrorCounts.size());
    for (size_t ii = 0; ii < m_ErrorCounts.size(); ii++)
    {
        if ((m_ErrorCounts[ii].bValid != rhs.m_ErrorCounts[ii].bValid) ||
            (m_ErrorCounts[ii].countValue != rhs.m_ErrorCounts[ii].countValue) ||
            (m_ErrorCounts[ii].bOverflowed != rhs.m_ErrorCounts[ii].bOverflowed))
        {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
//! \brief Allow aclwmulation between error count objects
//!
//! \param rhs : error count object to add to the current object
//!
//! \return reference to the current object after aclwmulation
LwLink::ErrorCounts & LwLink::ErrorCounts::operator+=(const LwLink::ErrorCounts &rhs)
{
    MASSERT(m_ErrorCounts.size() == rhs.m_ErrorCounts.size());

    for (size_t ii = 0; ii < m_ErrorCounts.size(); ii++)
    {
        if (rhs.m_ErrorCounts[ii].bValid)
        {
            m_ErrorCounts[ii].bValid = true;
            m_ErrorCounts[ii].countValue += rhs.m_ErrorCounts[ii].countValue;
            if (rhs.m_ErrorCounts[ii].bOverflowed)
                m_ErrorCounts[ii].bOverflowed = true;
        }
    }
    return *this;
}

//------------------------------------------------------------------------------
//! \brief Static function to get the JSON tag for a specific error ID
//!
//! \param errId : Error id to get the JSON tag for
//!
//! \return String representing the JSON tag of the error
/* static */ string LwLink::ErrorCounts::GetJsonTag(const ErrId errId)
{
    if (s_ErrorDescriptors.find(errId) != s_ErrorDescriptors.end())
        return s_ErrorDescriptors[errId].jsonTag;
    return "Unknown";
}

//------------------------------------------------------------------------------
//! \brief Static function to get the lane for a specific error ID
//!
//! \param errId : Error id to get the lane for
//!
//! \return Lane associated with the error, ILWALID_LANE if not associated
/* static */ UINT32 LwLink::ErrorCounts::GetLane(const ErrId errId)
{
    if (s_ErrorDescriptors.find(errId) != s_ErrorDescriptors.end())
        return s_ErrorDescriptors[errId].lane;
    return LWLINK_ILWALID_LANE;
}

//------------------------------------------------------------------------------
//! \brief Static function to get the error name for a specific error ID
//!
//! \param errId : Error id to get the name for
//!
//! \return String representing the name of the error
/* static */ string LwLink::ErrorCounts::GetName(const ErrId errId)
{
    if (s_ErrorDescriptors.find(errId) != s_ErrorDescriptors.end())
        return s_ErrorDescriptors[errId].name;
    return "Unknown";
}

//------------------------------------------------------------------------------
//! \brief Static function to get whether the error ID is internal only
//!
//! \param errId : Error id to check internal only on
//!
//! \return true if the error ID is internal only, false otherwise
/* static */ bool LwLink::ErrorCounts::IsInternalOnly(const ErrId errId)
{
    if (s_ErrorDescriptors.find(errId) != s_ErrorDescriptors.end())
        return s_ErrorDescriptors[errId].bInternalOnly;
    return true;
}

//------------------------------------------------------------------------------
//! \brief Static function to get whether the error ID should always use a count
//! threshold
//!
//! \param errId : Error id to check internal only on
//!
//! \return true if the error ID should always use a count threshold, false otherwise
/* static */ bool LwLink::ErrorCounts::IsCountOnly(const ErrId errId)
{
    if (s_ErrorDescriptors.find(errId) != s_ErrorDescriptors.end())
        return s_ErrorDescriptors[errId].bCountOnly;
    return true;
}

//------------------------------------------------------------------------------
//! \brief Static function to get whether the error ID has a failure threshold
//! associated with it
//!
//! \param errId : Error id to check threshold presence on
//!
//! \return true if the error ID has a failure threshold, false otherwise
/* static */ bool LwLink::ErrorCounts::IsThreshold(const ErrId errId)
{
    if (s_ErrorDescriptors.find(errId) != s_ErrorDescriptors.end())
        return s_ErrorDescriptors[errId].bThreshold;
    return false;
}
