/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Common include file for all MODS source files.

#ifndef INCLUDED_MODSINC_H
#define INCLUDED_MODSINC_H

#if defined(__GNUC__)
#   if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ > 3))
#       define PRAGMA_ONCE_SUPPORTED
#       pragma once
#   endif
#endif

#include "cpuopsys.h"

// Take care of Microsoft Visual C++ / Microsoft Windows specifics

#define DLLEXPORT
#define DLLIMPORT

#include <stdlib.h>

char* itoa( int n, char* str, int base );

#define stricmp(x,y) (strcasecmp((x), (y)))

#include "../core/include/macros.h"

// We want to include this from all source files
#include "../core/include/onetime.h"

#endif // !INCLUDED_MODSINC_H
