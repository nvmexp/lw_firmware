/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

namespace LwLinkP2PBandwidth
{
    struct LwLinkP2PBandwidthInput
    {
        device_ptr p2pMemory;
        device_ptr localMemory;
        UINT32 uint4PerThread;
        UINT32 iterations;
    };
};
