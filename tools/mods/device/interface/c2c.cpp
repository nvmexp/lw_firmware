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

#include "c2c.h"
#include "device/c2c/c2cdiagdriver.h"

const char * C2C::LinkStatusString(LinkStatus status)
{
    switch (status)
    {
        case LinkStatus::Active:
            return "Active";
        case LinkStatus::Off:
            return "Off";
        case LinkStatus::Fail:
            return "Fail";
        default:
            break;
    }
    return "Unknown";
}

C2C::ErrorCounts::ErrorCounts()
{
    m_ErrorCounts.resize(ErrId::NUM_ERR_COUNTERS);
}

UINT32 C2C::ErrorCounts::GetCount(const ErrId errId) const
{
    MASSERT(static_cast<size_t>(errId) < m_ErrorCounts.size());
    return m_ErrorCounts[errId].countValue;
}

bool C2C::ErrorCounts::IsValid(const ErrId errId) const
{
    MASSERT(static_cast<size_t>(errId) < m_ErrorCounts.size());
    return m_ErrorCounts[errId].bValid;
}

RC C2C::ErrorCounts::SetCount(const ErrId errId, const UINT32 val)
{
    if (static_cast<size_t>(errId) >= m_ErrorCounts.size())
    {
        Printf(Tee::PriError,
               "Invalid C2C counter Id : %d\n",
               errId);
        return RC::BAD_PARAMETER;
    }
    m_ErrorCounts[errId].countValue = val;
    m_ErrorCounts[errId].bValid     = true;

    return RC::OK;
}

string C2C::ErrorCounts::GetName(const ErrId errId)
{
    switch (errId)
    {
        case RX_CRC_ID:
            return "RxCrc";
        case TX_REPLAY_ID:
            return "TxReplay";
        case TX_REPLAY_B2B_FID_ID:
            return "TxReplayB2BFid";
        default:
            break;
    }
    return "Unknown";
}

string C2C::ErrorCounts::GetJsonTag(const ErrId errId)
{
    switch (errId)
    {
        case RX_CRC_ID:
            return "rx_crc";
        case TX_REPLAY_ID:
            return "tx_replay";
        case TX_REPLAY_B2B_FID_ID:
            return "tx_replay_b2b_fid";
        default:
            break;
    }
    return "unknown";
}
