/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef IFS_MISC_H
#define IFS_MISC_H

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "lwtypes.h"

#include <stddef.h> // NULL

// Round down (v) to a multiple of (s), where (s) is a power of 2
#define FS_ROUND_DOWN_POW2(v,s) ((v) & ~((s) - 1))
#define FS_ROUND_UP_POW2(v,s) ((v + (s) - 1) & ~((s) - 1))

#define FS_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define FS_MAX(a, b) (((a) > (b)) ? (a) : (b))

#if defined(LWRM)
#include "utils/lwprintf.h"
#include "utils/lwassert.h"

#if !defined(FS_PRINTF)
#define FS_PRINTF(level, format, ...) LW_PRINTF(LEVEL_##level, format, ##__VA_ARGS__)
#endif // !defined(FS_PRINTF)

#define FS_ASSERT(expr)                                                \
    LW_ASSERT_OR_ELSE_STR_PRECOMP(expr, #expr, /* no other action */)

#define FS_ASSERT_AND_RETURN(expr, retval)                             \
    LW_ASSERT_OR_ELSE_STR_PRECOMP(expr, #expr, return (retval))

#else // LWRM

#ifdef BUILD_DEBUG
#include <assert.h>
#endif

#if !defined(FS_PRINTF)
#define FS_PRINTF(level, format, ...) {}
#endif
#ifdef BUILD_DEBUG
#define FS_ASSERT(cond) assert(cond)
#else
#define FS_ASSERT(cond) {}
#endif // BUILD_DEBUG


#define FS_ASSERT_AND_RETURN(cond, retval)                             \
    do {                                                               \
        if (cond) {} else                                              \
        {                                                              \
            FS_PRINTF(ERROR, "assertion FAILED! %s: line %d\n",        \
                        __FILE__, __LINE__);                           \
            return (retval);                                           \
        }                                                              \
    } while(0)

#endif // defined(LWRM)

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // IFS_MISC_H
