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

struct RegFileParams
{
    device_ptr patternsPtr;
    device_ptr errorLogPtr;
    device_ptr errorCountPtr;
    UINT32 numPatterns;
    UINT32 numInnerIterations;
    UINT32 errorLogLen;
};

struct RegFileError
{
    UINT32 readVal;
    UINT32 expectedVal;
    UINT32 iteration;
    UINT32 smid;
    UINT32 warpid;
    UINT32 laneid;
    UINT32 registerid;
};
