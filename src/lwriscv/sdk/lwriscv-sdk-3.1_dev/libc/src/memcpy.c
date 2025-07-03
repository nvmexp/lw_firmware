/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stddef.h>
#include <stdint.h>
#include <libc/string.h>

void *memcpy(void *dst, const void *src, size_t count)
{
    char *d=dst;
    const char *s = src;

    while (count)
    {
        *d++ = *s++;
        count--;
    }

    return dst;
}
