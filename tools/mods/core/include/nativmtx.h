/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   nativmtx.h
 * @brief  Native non-relwrsive mutex implementation.
 */

#ifndef INCLUDED_NATIVE_MUTEX_H
#define INCLUDED_NATIVE_MUTEX_H

namespace Tasker
{
    class NativeMutex;
    class NativeMutexHolder;
}

// Windows implementation
#if defined(_MSC_VER)
#include "win32/winmutex.h"

// Other (POSIX) implementation
#else
#include "linux/linuxmutex.h"
#endif

#endif // !INCLUDED_NATIVE_MUTEX_H
