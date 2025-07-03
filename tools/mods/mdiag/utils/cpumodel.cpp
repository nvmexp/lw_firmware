/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <stdio.h>
#include "types.h"
#include "core/include/memtypes.h"
#include "cpumodel.h"
#include "core/include/platform.h"

static bool cpuModelEnabled = false;

// returns true if CPU Model is enabled in chiplib, false otherwise
bool CPUModel::Enabled()
{
    UINT32 data;

    if (0 == Platform::EscapeRead("CPU_MODEL|CM_ENABLED", 0, 4, &data))
    {
        cpuModelEnabled = data != 0;
    }

    return cpuModelEnabled;
}
