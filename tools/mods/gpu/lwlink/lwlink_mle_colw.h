/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/mle_protobuf.h"
#include "device/interface/lwlink.h"

namespace Mle
{
    inline Mle::LwlinkStateChange::SubLinkState ToSubLinkState(LwLink::SubLinkState state)
    {
        switch (state)
        {
            case LwLink::SubLinkState::SLS_OFF:         return Mle::LwlinkStateChange::sls_off;
            case LwLink::SubLinkState::SLS_SAFE_MODE:   return Mle::LwlinkStateChange::sls_safe_mode;
            case LwLink::SubLinkState::SLS_TRAINING:    return Mle::LwlinkStateChange::sls_training;
            case LwLink::SubLinkState::SLS_SINGLE_LANE: return Mle::LwlinkStateChange::sls_single_lane;
            case LwLink::SubLinkState::SLS_HIGH_SPEED:  return Mle::LwlinkStateChange::sls_high_speed;
            case LwLink::SubLinkState::SLS_ILWALID:     return Mle::LwlinkStateChange::sls_ilwalid;
            default:                                    break;
        }
        MASSERT("Unknown sub link state colwersion");
        return Mle::LwlinkStateChange::sls_ilwalid;
    };

    inline Mle::UphyRegLog::FomMode ToEomFomMode(LwLink::FomMode fomMode)
    {
        switch (fomMode)
        {
            case LwLink::FomMode::EYEO_X:       return Mle::UphyRegLog::FomMode::x;
            case LwLink::FomMode::EYEO_XL_O:    return Mle::UphyRegLog::FomMode::xl_o;
            case LwLink::FomMode::EYEO_XL_E:    return Mle::UphyRegLog::FomMode::xl_e;
            case LwLink::FomMode::EYEO_XH_O:    return Mle::UphyRegLog::FomMode::xh_o;
            case LwLink::FomMode::EYEO_XH_E:    return Mle::UphyRegLog::FomMode::xh_e;
            case LwLink::FomMode::EYEO_Y:       return Mle::UphyRegLog::FomMode::y;
            case LwLink::FomMode::EYEO_Y_U:     return Mle::UphyRegLog::FomMode::y_u;
            case LwLink::FomMode::EYEO_Y_M:     return Mle::UphyRegLog::FomMode::y_m;
            case LwLink::FomMode::EYEO_Y_L:     return Mle::UphyRegLog::FomMode::y_l;
            case LwLink::FomMode::EYEO_YL_O:    return Mle::UphyRegLog::FomMode::yl_0;
            case LwLink::FomMode::EYEO_YL_E:    return Mle::UphyRegLog::FomMode::yl_e;
            case LwLink::FomMode::EYEO_YH_O:    return Mle::UphyRegLog::FomMode::yh_o;
            case LwLink::FomMode::EYEO_YH_E:    return Mle::UphyRegLog::FomMode::yh_e;
            default:                            break;
        }
        MASSERT("Unknown fom mode colwersion");
        return Mle::UphyRegLog::FomMode::unknown_fm;
    };
}
