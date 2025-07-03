/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/lwlink/lwlinkimpl.h"

const LwLink::LibDevHandles_t LwLink::NO_HANDLES = {};

// Macro to create a function for creating a concrete GPU PCIE interface
#define CREATE_LWLINK_STUB_FUNCTION(Class)                                    \
    LwLinkImpl *Create##Class(GpuSubdevice *pGpu)                             \
    {                                                                         \
        return nullptr;                                                       \
    }

CREATE_LWLINK_STUB_FUNCTION(NullLwLink);
CREATE_LWLINK_STUB_FUNCTION(PascalLwLink);
CREATE_LWLINK_STUB_FUNCTION(VoltaLwLink);
CREATE_LWLINK_STUB_FUNCTION(TuringLwLink);
