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

class ThermalSlowdown
{
public:
    virtual ~ThermalSlowdown() = default;

    RC StartThermalSlowdown(UINT32 periodUs)
    {
        return DoStartThermalSlowdown(periodUs);
    }
    RC StopThermalSlowdown()
    {
        return DoStopThermalSlowdown();
    }

private:
    virtual RC DoStartThermalSlowdown(UINT32 periodUs) = 0;
    virtual RC DoStopThermalSlowdown() = 0;
};
