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

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *d = s1;
    const unsigned char *s = s2;
    while (n--)
    {
        int x = (*s) - (*d);
        if (x)
        {
            return x;
        }
        d++;
        s++;
    }
    return 0;
}
