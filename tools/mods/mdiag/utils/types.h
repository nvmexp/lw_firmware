/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _TYPES_H_
#define _TYPES_H_

// Include definitions of MODS types.  Much of the below will be obsoleted eventually.
#include "core/include/types.h"

#ifdef _WIN32

#include <stddef.h>

// would like to #include <stdint.h>, but old versions of MSVC (pre-2010) don't have it
typedef  INT08  int8_t;
typedef UINT08 uint8_t;
typedef  INT16  int16_t;
typedef UINT16 uint16_t;
typedef  INT32  int32_t;
typedef UINT32 uint32_t;
typedef  INT64  int64_t;
typedef UINT64 uint64_t;

// Gross disgusting hack to work-around MSVC's bogus behavior of
// moving the scope of a declarion in a 'for' statement up to the
// level outside the 'for'.
//#define for if (1) for
// Here is a version that does not silently introduce subtle bugs:
#define for if (0) ; else for
#define DIR_SEPARATOR_CHAR '\\'
#define DIR_SEPARATOR_CHAR2 '/'

extern int strcasecmp(const char* s1, const char* s2);
extern int strncasecmp(const char* s1, const char* s2, size_t n);

#define SNPRINTF    _snprintf
#define VSNPRINTF   _vsnprintf

#else

#define DIR_SEPARATOR_CHAR '/'
#define DIR_SEPARATOR_CHAR2 '\\'

#define SNPRINTF    snprintf
#define VSNPRINTF   vsnprintf

#endif

#define SizeofArr(arr)      (sizeof(arr)/sizeof(arr[0]))

#define KILOBYTE (1024)
#define MEGABYTE (KILOBYTE*KILOBYTE)
#define GIGABYTE (KILOBYTE*MEGABYTE)

#endif // _TYPES_H
