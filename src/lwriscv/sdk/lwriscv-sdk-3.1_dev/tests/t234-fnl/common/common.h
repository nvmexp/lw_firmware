/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef COMMON_H
#define COMMON_H
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

int vprintfmt(void (*putch)(int, void*), void *putdat,
              const char* pFormat, va_list ap);
bool printInit(uint16_t bufferSize);
int putchar(int c);
int puts(const char *pStr);
void putHex(int count, unsigned long value);
int printf(const char *pFmt, ...);

#endif // COMMON_H
