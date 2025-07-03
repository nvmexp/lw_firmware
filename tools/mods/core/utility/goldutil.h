/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  goldutil.h
 * @brief Utilities for golden.cpp, should ease sharing between GPU and WMP
 *
 * For now, this will be implemented in gpuutils.cpp, wmputil.cpp, and
 * gld_stub.cpp (MCP only build)
 */

#ifndef INCLUDED_GOLDEN_UTILITIES_H
#define INCLUDED_GOLDEN_UTILITIES_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif

namespace GoldenUtils
{
    void SendTrigger(UINT32 subdev);
};

#endif // INCLUDED_GOLDEN_UTILITIES_H

