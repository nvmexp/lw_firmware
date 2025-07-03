/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_LIBC_H
#define LIBLWRISCV_LIBC_H
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
// Libc-replace interface (TODO: to be split perhaps?)

// memops.c
void *memcpy(void *dst, const void *src, size_t count);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *s, int c, size_t n);

// sleep.c
uint64_t delay(unsigned cycles);
void usleep(unsigned uSec);

// string.c
size_t strnlen(const char *s, size_t n);

// vprintfmt.c
int vprintfmt(void (*putch)(int, void*), void *putdat, const char* pFormat, va_list ap);

// freestanding things
__attribute__( ( always_inline ) ) static inline int isprint(int c)
{
    return c >= ' ' && c <= '~';
}

#endif // LIBLWRISCV_LIBC_H
