/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "ctrl/ctrl2080.h"

#include "core/include/rc.h"
#include "core/include/types.h"

class LwLinkSleepState
{
public:
    enum SleepState
    {
        L0 = LW2080_CTRL_LWLINK_POWER_STATE_L0
      , L1 = LW2080_CTRL_LWLINK_POWER_STATE_L1
      , L2 = LW2080_CTRL_LWLINK_POWER_STATE_L2
      , L3 = LW2080_CTRL_LWLINK_POWER_STATE_L3
    };

    bool IsSupported() const { return DoIsSleepStateSupported(); }
    RC Lock() { return DoSleepStateLock(); }
    RC UnLock() { return DoSleepStateUnlock(); }
    RC Get(UINT32 linkId, SleepState *state) const { return DoGetSleepState(linkId, state); }
    RC Set(UINT32 mask, SleepState state) { return DoSetSleepState(mask, state); }

private:
    virtual bool DoIsSleepStateSupported() const = 0;
    virtual RC DoSleepStateLock() = 0;
    virtual RC DoSleepStateUnlock() = 0;
    virtual RC DoGetSleepState(UINT32 linkId, SleepState *state) const = 0;
    virtual RC DoSetSleepState(UINT32 mask, SleepState state) = 0;
};
