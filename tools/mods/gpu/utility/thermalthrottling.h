/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/types.h"

class ThermalThrottling
{
public:
    virtual ~ThermalThrottling() = default;

    RC StartThermalThrottling(UINT32 onCount, UINT32 offCount)
    {
        return DoStartThermalThrottling(onCount, offCount);
    }
    RC StopThermalThrottling()
    {
        return DoStopThermalThrottling();
    }

private:
    virtual RC DoStartThermalThrottling(UINT32 onCount, UINT32 offCount) = 0;
    virtual RC DoStopThermalThrottling() = 0;
};
