/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_LIBC_STRING_H
#define LWRISCV_LIBC_STRING_H

#include <stddef.h>

void   *memcpy(void *dst, const void *src, size_t count);
int     memcmp(const void *s1, const void *s2, size_t n);
void   *memset(void *s, int c, size_t n);
size_t  strnlen(const char *s, size_t n);

#endif // LWRISCV_LIBC_STRING_H
