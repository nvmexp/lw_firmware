/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWMODSLOG_H
#define INCLUDED_LWMODSLOG_H

// TODO: inttypes.h won't define what we need if we're using C++ without this define. Is there a better way?
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#if LW_KEXT_LOG
#define _PLOG_ENT(p) kprintf("+ enter %s[%p]::%s\n", (p)->getName(), (p), __FUNCTION__)
#define _PLOG_EXT(p) kprintf("- leave %s[%p]::%s\n", (p)->getName(), (p), __FUNCTION__)
#define _PLOG_ERR_EXT(p, format, args...) \
    kprintf("- leave %s[%p]::%s ERROR - " format "\n", \
            (p)->getName(), (p), __FUNCTION__, ##args)
#else
#define _PLOG_ENT(p)
#define _PLOG_EXT(p)
#define _PLOG_ERR_EXT(p, format, args...) kprintf("modskext: %s: error - " format "\n", __FUNCTION__, ##args)
#endif

#define LOG_ENT() _PLOG_ENT(this)
#define LOG_EXT() _PLOG_EXT(this)
#define LOG_ERR_EXT(format, args...) _PLOG_ERR_EXT(this, format, ##args)

#define SLOG_ENT() _PLOG_ENT(target)
#define SLOG_EXT() _PLOG_EXT(target)
#define SLOG_ERR_EXT(format, args...) _PLOG_ERR_EXT(target, format, ##args)
#define SLOG_ERR(format, args...) kprintf("%s ERROR - " format "\n", __FUNCTION__, ##args)

#endif // INCLUDED_LWMODSLOG_H
