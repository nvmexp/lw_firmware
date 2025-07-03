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

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;

    while (n--)
    {
        *p++ = (uint8_t)c;
    }

    return s;
}
