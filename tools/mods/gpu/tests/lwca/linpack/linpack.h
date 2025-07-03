/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

#define ERROR_LOG_LEN 8192
struct LinpackError
{
    UINT64 failureIdx;
};

#define ERROR_LOG_SIZE_BYTES (ERROR_LOG_LEN * sizeof(LinpackError))

#define OCLWPY_FUNC_POLLING_INTERVAL_NS 1000000
