/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/interface/lwlink/lwlpowerstate.h"

#include "core/include/types.h"

#include <vector>

//-----------------------------------------------------------------------------
/* static */ string LwLinkPowerState::SubLinkPowerStateToString(SubLinkPowerState state)
{
    switch (state)
    {
        case SLS_PWRM_FB:
            return "FB";
        case SLS_PWRM_LP:
            return "LP";
        case SLS_PWRM_ILWALID:
            return "Invalid";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
string LwLinkPowerState::ClaimStateToString(ClaimState state)
{
    switch (state)
    {
        case ClaimState::FULL_BANDWIDTH:
            return "Full Bandwidth";
        case ClaimState::SINGLE_LANE:
            return "Single Lane";
        case ClaimState::HW_CONTROL:
            return "HW Control";
        case ClaimState::ALL_STATES:
            return "All States";
        case ClaimState::NONE:
            return "None";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
RC LwLinkPowerState::Owner::Claim(ClaimState state)
{
    RC rc;
    if (m_bClaimed)
    {
        Printf(Tee::PriError,
               "Release current LwLink power state claim before requesting another claim\n");
        return RC::SOFTWARE_ERROR;
    }
    MASSERT(m_pLwLinkPowerState);
    CHECK_RC(m_pLwLinkPowerState->ClaimPowerState(state));
    m_bClaimed = true;
    return rc;
}

//-----------------------------------------------------------------------------
RC LwLinkPowerState::Owner::Release()
{
    RC rc;
    if (m_bClaimed)
    {
        MASSERT(m_pLwLinkPowerState);
        CHECK_RC(m_pLwLinkPowerState->ReleasePowerState());
        m_bClaimed = false;
    }
    else
    {
        Printf(Tee::PriLow, "LwLink power state not claimed, release skipped\n");
    }
    return rc;
}
