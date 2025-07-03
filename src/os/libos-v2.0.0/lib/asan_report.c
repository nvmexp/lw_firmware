/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include "libos.h"
#include "libos_log.h"
#include "libos_asan.h"
#include "libos_asan_report.h"

/**
 * @file
 * @brief Helpers for ASAN error reporting needing static memory
 * 
 * LIBOS logging requires any string logged out with %s to reside in
 * LIBOS_SECTION_LOGGING. As such, we cannout make error reporting helpers
 * header-only. This file contains the parts of ASAN error reporting helpers
 * that require LIBOS_SECTION_LOGGING.
 */

#ifndef LW_ARRAY_ELEMENTS
#define LW_ARRAY_ELEMENTS(x) ((sizeof(x) / sizeof((x)[0])))
#endif

const char *libosAsanReportGetShortDesc(LibosTaskId remote, LwUPtr badAddr)
{
    static LIBOS_SECTION_LOGGING const char BAD_str[]        = "bad";
    static LIBOS_SECTION_LOGGING const char OOB_str[]        = "out-of-bounds";
    static LIBOS_SECTION_LOGGING const char GLOBAL_OOB_str[] = "global-out-of-bounds";
    static LIBOS_SECTION_LOGGING const char STACK_OOB_str[]  = "stack-out-of-bounds";
    static LIBOS_SECTION_LOGGING const char HEAP_OOB_str[]   = "heap-out-of-bounds";
    static LIBOS_SECTION_LOGGING const char HEAP_UAF_str[]   = "heap-use-after-free";

    static const struct
    {
        LwU8 poisolwalue;
        const char *desc;
    } poisonTbl[] =
    {
        { LIBOS_ASAN_POISON_GLOBAL_RED,      GLOBAL_OOB_str },
        { LIBOS_ASAN_POISON_STACK_LEFT,      STACK_OOB_str },
        { LIBOS_ASAN_POISON_STACK_MID,       STACK_OOB_str },
        { LIBOS_ASAN_POISON_STACK_RIGHT,     STACK_OOB_str },
        { LIBOS_ASAN_POISON_STACK_PARTIAL,   STACK_OOB_str },
        { LIBOS_ASAN_POISON_HEAP_LEFT,       HEAP_OOB_str },
        { LIBOS_ASAN_POISON_HEAP_RIGHT,      HEAP_OOB_str },
        { LIBOS_ASAN_POISON_HEAP_FREED,      HEAP_UAF_str }
    };

    LwUPtr shadow = (LwUPtr) LIBOS_ASAN_MEM_TO_SHADOW(badAddr);
    LwU8 value;

    if (libosTaskMemoryRead(&value, remote, shadow, 1) != LIBOS_OK)
    {
        return BAD_str;
    }

    if (value > 0 && value < LIBOS_ASAN_SHADOW_GRAN)
    {
        // this is partially-poisoned shadow byte, try next (should be full)
        ++shadow;
        if (libosTaskMemoryRead(&value, remote, shadow, 1) != LIBOS_OK)
        {
            return OOB_str;
        }
    }

    LwU64 i;
    LwU64 n = LW_ARRAY_ELEMENTS(poisonTbl);

    for (i = 0; i < n; i++)
    {
        if (poisonTbl[i].poisolwalue == value)
        {
            return poisonTbl[i].desc;
        }
    }

    return BAD_str;
}

const char *libosAsanReportGetReadWriteDesc(LwBool bWrite)
{
    static LIBOS_SECTION_LOGGING const char WRITE_str[] = "write";
    static LIBOS_SECTION_LOGGING const char READ_str[]  = "read";

    if (bWrite)
    {
        return WRITE_str;
    }
    else
    {
        return READ_str;
    }
}