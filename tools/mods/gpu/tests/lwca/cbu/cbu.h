/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <stdint.h>
#include "../tools/tools.h"

struct CbuParams
{
    device_ptr flagArrMem;
    device_ptr threadStepArrMem;
    UINT32 numFlagAccesses;
};
