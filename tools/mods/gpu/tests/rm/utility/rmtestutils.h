/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_RMTESTUTILS_H
#define INCLUDED_RMTESTUTILS_H

#include "core/include/utility.h"
#include "core/include/platform.h"

struct PollArguments
{
    UINT32  value;
    UINT32 *pAddr;
};

RC      PollVal(PollArguments *pArg, FLOAT64 timeoutMs);
bool    PollFunc(void *pArg);

//! Utility to disable breakpoints conditionally.
#define DISABLE_BREAK_COND(name, bDisable)                         \
{                                                                  \
    unique_ptr<Platform::DisableBreakPoints> name(                 \
            (bDisable) ? new Platform::DisableBreakPoints : NULL);

//! Resets breakpoints to previous state.
#define DISABLE_BREAK_END(name) \
    (name).reset(NULL);         \
}

#endif
