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

/**
 * @file   nativmtx.h
 * @brief  Native non-relwrsive mutex implementation.
 */

#ifndef INCLUDED_NATIVETHREAD_H
#define INCLUDED_NATIVETHREAD_H

#if defined(_MSC_VER)
#include "win32/winthread.h"

// Other (POSIX) implementation
#else
#include "linux/posixthread.h"
#endif

#endif // !INCLUDED_NATIVETHREAD_H
