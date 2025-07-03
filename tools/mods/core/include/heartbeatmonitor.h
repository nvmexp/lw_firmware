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

#pragma once

#include "core/include/rc.h"
#include "core/include/tasker.h"

namespace HeartBeatMonitor
{
    using MonitorHandle = INT64;
    RC SendUpdate(MonitorHandle regId);
    MonitorHandle RegisterTest(INT32 testId, INT32 devInst, UINT64 heartBeatPeriodSeconds);
    RC UnRegisterTest(MonitorHandle regId);
    RC InitMonitor();
    void StopHeartBeat();
};
