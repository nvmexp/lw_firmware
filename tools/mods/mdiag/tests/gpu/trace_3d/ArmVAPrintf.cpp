/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// vsprintf.c
//
// Print formatting routines
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROLWREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <map>
#include "mdiag/sysspec.h"

#ifdef KERNEL
#define NOFLOAT
#endif

#define ZEROPAD 1               // Pad with zero
#define SIGN    2               // Unsigned/signed long
#define PLUS    4               // Show plus
#define SPACE   8               // Space if plus
#define LEFT    16              // Left justified
#define SPECIAL 32              // 0x
#define LARGE   64              // Use 'ABCDEF' instead of 'abcdef'

#define is_digit(c) ((c) >= '0' && (c) <= '9')

static const char *digits = "0123456789abcdefghijklmnopqrstuvwxyz";
static const char *upper_digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static size_t StrNLen(const char *s, size_t count)
{
  const char *sc;
  for (sc = s; *sc != '\0' && count--; ++sc);
  return sc - s;
}

static int skip_atoi(const char **s)
{
  int i = 0;
  while (is_digit(**s)) i = i*10 + *((*s)++) - '0';
  return i;
}

static char *number(char *str, long num, int base, int size, int precision, int type)
{
  char c, sign, tmp[66];
  const char *dig = digits;
  int i;

  if (type & LARGE)  dig = upper_digits;
  if (type & LEFT) type &= ~ZEROPAD;
  if (base < 2 || base > 36) return 0;

  c = (type & ZEROPAD) ? '0' : ' ';
  sign = 0;
  if (type & SIGN)
  {
    if (num < 0)
    {
      sign = '-';
      num = -num;
      size--;
    }
    else if (type & PLUS)
    {
      sign = '+';
      size--;
    }
    else if (type & SPACE)
    {
      sign = ' ';
      size--;
    }
  }

  if (type & SPECIAL)
  {
    if (base == 16)
      size -= 2;
    else if (base == 8)
      size--;
  }

  i = 0;

  if (num == 0)
    tmp[i++] = '0';
  else
  {
    while (num != 0)
    {
      tmp[i++] = dig[((unsigned long) num) % (unsigned) base];
      num = ((unsigned long) num) / (unsigned) base;
    }
  }

  if (i > precision) precision = i;
  size -= precision;
  if (!(type & (ZEROPAD | LEFT))) while (size-- > 0) *str++ = ' ';
  if (sign) *str++ = sign;

  if (type & SPECIAL)
  {
    if (base == 8)
      *str++ = '0';
    else if (base == 16)
    {
      *str++ = '0';
      *str++ = digits[33];
    }
  }

  if (!(type & LEFT)) while (size-- > 0) *str++ = c;
  while (i < precision--) *str++ = '0';
  while (i-- > 0) *str++ = tmp[i];
  while (size-- > 0) *str++ = ' ';

  return str;
}

static char *eaddr(char *str, unsigned char *addr, int size, int precision, int type)
{
  char tmp[24];
  const char *dig = digits;
  int i, len;

  if (type & LARGE)  dig = upper_digits;
  len = 0;
  for (i = 0; i < 6; i++)
  {
    if (i != 0) tmp[len++] = ':';
    tmp[len++] = dig[addr[i] >> 4];
    tmp[len++] = dig[addr[i] & 0x0F];
  }

  if (!(type & LEFT)) while (len < size--) *str++ = ' ';
  for (i = 0; i < len; ++i) *str++ = tmp[i];
  while (len < size--) *str++ = ' ';

  return str;
}

static char *iaddr(char *str, unsigned char *addr, int size, int precision, int type)
{
  char tmp[24];
  int i, n, len;

  len = 0;
  for (i = 0; i < 4; i++)
  {
    if (i != 0) tmp[len++] = '.';
    n = addr[i];

    if (n == 0)
      tmp[len++] = digits[0];
    else
    {
      if (n >= 100)
      {
        tmp[len++] = digits[n / 100];
        n = n % 100;
        tmp[len++] = digits[n / 10];
        n = n % 10;
      }
      else if (n >= 10)
      {
        tmp[len++] = digits[n / 10];
        n = n % 10;
      }

      tmp[len++] = digits[n];
    }
  }

  if (!(type & LEFT)) while (len < size--) *str++ = ' ';
  for (i = 0; i < len; ++i) *str++ = tmp[i];
  while (len < size--) *str++ = ' ';

  return str;
}

#ifndef NOFLOAT

#include <math.h>

#define CVTBUFSIZE        (309 + 43)

static char *cvt(double arg, int ndigits, int *decpt, int *sign, char *buf, int eflag) {
  int r2;
  double fi, fj;
  char *p, *p1;

  if (ndigits < 0) ndigits = 0;
  if (ndigits >= CVTBUFSIZE - 1) ndigits = CVTBUFSIZE - 2;
  r2 = 0;
  *sign = 0;
  p = &buf[0];
  if (arg < 0) {
    *sign = 1;
    arg = -arg;
  }
  arg = modf(arg, &fi);
  p1 = &buf[CVTBUFSIZE];

  if (fi != 0) {
    p1 = &buf[CVTBUFSIZE];
    while (fi != 0) {
      fj = modf(fi / 10, &fi);
      *--p1 = (int)((fj + .03) * 10) + '0';
      r2++;
    }
    while (p1 < &buf[CVTBUFSIZE]) *p++ = *p1++;
  } else if (arg > 0) {
    while ((fj = arg * 10) < 1) {
      arg = fj;
      r2--;
    }
  }
  p1 = &buf[ndigits];
  if (eflag == 0) p1 += r2;
  *decpt = r2;
  if (p1 < &buf[0]) {
    buf[0] = '\0';
    return buf;
  }
  while (p <= p1 && p < &buf[CVTBUFSIZE]) {
    arg *= 10;
    arg = modf(arg, &fj);
    *p++ = (int) fj + '0';
  }
  if (p1 >= &buf[CVTBUFSIZE]) {
    buf[CVTBUFSIZE - 1] = '\0';
    return buf;
  }
  p = p1;
  *p1 += 5;
  while (*p1 > '9') {
    *p1 = '0';
    if (p1 > buf) {
      ++*--p1;
    } else {
      *p1 = '1';
      (*decpt)++;
      if (eflag == 0) {
        if (p > buf) *p = '0';
        p++;
      }
    }
  }
  *p = '\0';
  return buf;
}

char *ecvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf) {
  return cvt(arg, ndigits, decpt, sign, buf, 1);
}

char *fcvtbuf(double arg, int ndigits, int *decpt, int *sign, char *buf) {
  return cvt(arg, ndigits, decpt, sign, buf, 0);
}

static void cfltcvt(double value, char *buffer, char fmt, int precision)
{
  int decpt, sign, exp, pos;
  char *digits = NULL;
  char cvtbuf[80];
  int capexp = 0;
  int magnitude;

  if (fmt == 'G' || fmt == 'E')
  {
    capexp = 1;
    fmt += 'a' - 'A';
  }

  if (fmt == 'g')
  {
    digits = ecvtbuf(value, precision, &decpt, &sign, cvtbuf);
    magnitude = decpt - 1;
    if (magnitude < -4  ||  magnitude > precision - 1)
    {
      fmt = 'e';
      precision -= 1;
    }
    else
    {
      fmt = 'f';
      precision -= decpt;
    }
  }

  if (fmt == 'e')
  {
    digits = ecvtbuf(value, precision + 1, &decpt, &sign, cvtbuf);

    if (sign) *buffer++ = '-';
    *buffer++ = *digits;
    if (precision > 0) *buffer++ = '.';
    memcpy(buffer, digits + 1, precision);
    buffer += precision;
    *buffer++ = capexp ? 'E' : 'e';

    if (decpt == 0)
    {
      if (value == 0.0)
        exp = 0;
      else
        exp = -1;
    }
    else
      exp = decpt - 1;

    if (exp < 0)
    {
      *buffer++ = '-';
      exp = -exp;
    }
    else
      *buffer++ = '+';

    buffer[2] = (exp % 10) + '0';
    exp = exp / 10;
    buffer[1] = (exp % 10) + '0';
    exp = exp / 10;
    buffer[0] = (exp % 10) + '0';
    buffer += 3;
  }
  else if (fmt == 'f')
  {
    digits = fcvtbuf(value, precision, &decpt, &sign, cvtbuf);
    if (sign) *buffer++ = '-';
    if (*digits)
    {
      if (decpt <= 0)
      {
        *buffer++ = '0';
        *buffer++ = '.';
        for (pos = 0; pos < -decpt; pos++) *buffer++ = '0';
        while (*digits) *buffer++ = *digits++;
      }
      else
      {
        pos = 0;
        while (*digits)
        {
          if (pos++ == decpt) *buffer++ = '.';
          *buffer++ = *digits++;
        }
      }
    }
    else
    {
      *buffer++ = '0';
      if (precision > 0)
      {
        *buffer++ = '.';
        for (pos = 0; pos < precision; pos++) *buffer++ = '0';
      }
    }
  }

  *buffer = '\0';
}

static void forcdecpt(char *buffer)
{
  while (*buffer)
  {
    if (*buffer == '.') return;
    if (*buffer == 'e' || *buffer == 'E') break;
    buffer++;
  }

  if (*buffer)
  {
    int n = strlen(buffer);
    while (n > 0)
    {
      buffer[n + 1] = buffer[n];
      n--;
    }

    *buffer = '.';
  }
  else
  {
    *buffer++ = '.';
    *buffer = '\0';
  }
}

static void cropzeros(char *buffer)
{
  char *stop;

  while (*buffer && *buffer != '.') buffer++;
  if (*buffer++)
  {
    while (*buffer && *buffer != 'e' && *buffer != 'E') buffer++;
    stop = buffer--;
    while (*buffer == '0') buffer--;
    if (*buffer == '.') buffer--;
    while ((*++buffer = *stop++));
  }
}

static char *flt(char *str, double num, int size, int precision, char fmt, int flags)
{
  char tmp[80];
  char c, sign;
  int n, i;

  // Left align means no zero padding
  if (flags & LEFT) flags &= ~ZEROPAD;

  // Determine padding and sign char
  c = (flags & ZEROPAD) ? '0' : ' ';
  sign = 0;
  if (flags & SIGN)
  {
    if (num < 0.0)
    {
      sign = '-';
      num = -num;
      size--;
    }
    else if (flags & PLUS)
    {
      sign = '+';
      size--;
    }
    else if (flags & SPACE)
    {
      sign = ' ';
      size--;
    }
  }

  // Compute the precision value
  if (precision < 0)
    precision = 6; // Default precision: 6
  else if (precision == 0 && fmt == 'g')
    precision = 1; // ANSI specified

  // Colwert floating point number to text
  cfltcvt(num, tmp, fmt, precision);

  // '#' and precision == 0 means force a decimal point
  if ((flags & SPECIAL) && precision == 0) forcdecpt(tmp);

  // 'g' format means crop zero unless '#' given
  if (fmt == 'g' && !(flags & SPECIAL)) cropzeros(tmp);

  n = strlen(tmp);

  // Output number with alignment and padding
  size -= n;
  if (!(flags & (ZEROPAD | LEFT))) while (size-- > 0) *str++ = ' ';
  if (sign) *str++ = sign;
  if (!(flags & LEFT)) while (size-- > 0) *str++ = c;
  for (i = 0; i < n; i++) *str++ = tmp[i];
  while (size-- > 0) *str++ = ' ';

  return str;
}

#endif

size_t ArmSizeOf(char) { return 1; }
size_t ArmSizeOf(short) { return 2; }
size_t ArmSizeOf(int) { return 4; }
size_t ArmSizeOf(long) { return 4; }
size_t ArmSizeOf(long long) { return 8; }
size_t ArmSizeOf(float) { return 4; }
size_t ArmSizeOf(double) { return 8; }
size_t ArmSizeOf(long double) { return 8; }
size_t ArmSizeOf(void *) { return 4; }
size_t ArmSizeOf(long long unsigned int) { return 8;}
size_t ArmSizeOf(long unsigned int) { return 4;}
size_t ArmSizeOf(unsigned int) { return 4;}

template <class T>
T ArmVaArg(char *&ap, int &addrVaList)
{
    // ARM platform will alignment to type.
    T tmp = T();
    int alignment = ((int)addrVaList + ArmSizeOf(tmp) - 1) & ~(ArmSizeOf(tmp) - 1);
    ap += (alignment - addrVaList);
    addrVaList = alignment + ArmSizeOf(tmp);
    return *(T *)((ap += ArmSizeOf(tmp)) - ArmSizeOf(tmp));
}

#define CHECK_ARGS if (args >= args_end) return str - buf

int vsprintf(char *buf, const char *fmt, char *args, int addrVaList)
{
  int len;
  unsigned long num;
  int i, base;
  char *str;
  char *s;

  int flags;            // Flags to number()

  int field_width;      // Width of output field
  int precision;        // Min. # of digits for integers; max number of chars for from string
  int qualifier;        // 'h', 'l', or 'L' for integer fields
  char *args_end = args + BUFSIZ;

  for (str = buf; *fmt; fmt++)
  {
    if (*fmt != '%')
    {
      *str++ = *fmt;
      continue;
    }

    // Process flags
    flags = 0;
repeat:
    fmt++; // This also skips first '%'
    switch (*fmt)
    {
      case '-': flags |= LEFT; goto repeat;
      case '+': flags |= PLUS; goto repeat;
      case ' ': flags |= SPACE; goto repeat;
      case '#': flags |= SPECIAL; goto repeat;
      case '0': flags |= ZEROPAD; goto repeat;
    }

    // Get field width
    field_width = -1;
    if (is_digit(*fmt))
      field_width = skip_atoi(&fmt);
    else if (*fmt == '*')
    {
      fmt++;
      CHECK_ARGS;
      field_width = ArmVaArg<int>(args, addrVaList);//va_arg(args, int);
      if (field_width < 0)
      {
        field_width = -field_width;
        flags |= LEFT;
      }
    }

    // Get the precision
    precision = -1;
    if (*fmt == '.')
    {
      ++fmt;
      if (is_digit(*fmt))
        precision = skip_atoi(&fmt);
      else if (*fmt == '*')
      {
        ++fmt;
        CHECK_ARGS;
        precision = ArmVaArg<int>(args, addrVaList);//va_arg(args, int);
      }
      if (precision < 0) precision = 0;
    }

    // Get the colwersion qualifier
    qualifier = -1;
    if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
    {
      qualifier = *fmt;
      fmt++;
    }

    // Default base
    base = 10;

    switch (*fmt)
    {
      case 'c':
        if (!(flags & LEFT)) while (--field_width > 0) *str++ = ' ';
        CHECK_ARGS;
        *str++ = (unsigned char) ArmVaArg<int>(args, addrVaList);//va_arg(args, int);
        while (--field_width > 0) *str++ = ' ';
        continue;

      case 's':
        {
        CHECK_ARGS;
        int temp = ArmVaArg<int>(args, addrVaList);//va_arg(args, char *);
        // if (!s) s = "<NULL>";
        char buf0[BUFSIZ];
        Platform::PhysRd(temp, &buf0[0], BUFSIZ);
        buf0[BUFSIZ - 1] = 0;
        s = &buf0[0];
        len = StrNLen(buf0, precision);
        if (!(flags & LEFT)) while (len < field_width--) *str++ = ' ';
        for (i = 0; i < len; ++i) *str++ = *s++;
        while (len < field_width--) *str++ = ' ';
        continue;
        }

      case 'p':
        if (field_width == -1)
        {
          void * tmp = 0;
          field_width = 2 * ArmSizeOf(tmp);
          flags |= ZEROPAD;
        }
        CHECK_ARGS;
        str = number(str, (unsigned long) ArmVaArg<void *>(args, addrVaList), 16, field_width, precision, flags);//va_arg(args, void *), 16, field_width, precision, flags);
        continue;

      case 'n':
        CHECK_ARGS;
        if (qualifier == 'l')
        {
          long *ip = ArmVaArg<long *>(args, addrVaList);//va_arg(args, long *);
          *ip = (str - buf);
        }
        else
        {
          int *ip = ArmVaArg<int *>(args, addrVaList);//va_arg(args, int *);
          *ip = (str - buf);
        }
        continue;

      case 'A':
        flags |= LARGE;

      case 'a':
        CHECK_ARGS;
        if (qualifier == 'l')
          str = eaddr(str, ArmVaArg<unsigned char *>(args, addrVaList), field_width, precision, flags);//va_arg(args, unsigned char *), field_width, precision, flags);
        else
          str = iaddr(str, ArmVaArg<unsigned char *>(args, addrVaList), field_width, precision, flags);//va_arg(args, unsigned char *), field_width, precision, flags);
        continue;

      // Integer number formats - set up the flags and "break"
      case 'o':
        base = 8;
        break;

      case 'X':
        flags |= LARGE;

      case 'x':
        base = 16;
        break;

      case 'd':
      case 'i':
        flags |= SIGN;

      case 'u':
        break;

#ifndef NOFLOAT

      case 'E':
      case 'G':
      case 'e':
      case 'f':
      case 'g':
        CHECK_ARGS;
        str = flt(str, ArmVaArg<double>(args, addrVaList), field_width, precision, *fmt, flags | SIGN);//va_arg(args, double), field_width, precision, *fmt, flags | SIGN);
        continue;

#endif

      default:
        if (*fmt != '%') *str++ = '%';
        if (*fmt)
          *str++ = *fmt;
        else
          --fmt;
        continue;
    }

    CHECK_ARGS;
    if (qualifier == 'l')
      num = ArmVaArg<unsigned long>(args, addrVaList);//va_arg(args, unsigned long);
    else if (qualifier == 'h')
    {
      if (flags & SIGN)
        num = ArmVaArg<short>(args, addrVaList);//va_arg(args, short);
      else
        num = ArmVaArg<unsigned short>(args, addrVaList);//va_arg(args, unsigned short);
    }
    else if (flags & SIGN)
      num = ArmVaArg<int>(args, addrVaList);//va_arg(args, int);
    else
      num = ArmVaArg<unsigned int>(args, addrVaList);//va_arg(args, unsigned int);

    str = number(str, num, base, field_width, precision, flags);
  }

  *str = '\0';
  return str - buf;
}

INT32 ArmVAPrintf(char *Format, void * Args)
{
    INT32 rc;
    char buf[BUFSIZ]; // defined in stdio.h as 512.
    char ap[BUFSIZ];
    Platform::PhysRd(reinterpret_cast<PHYSADDR>(Args), &ap[0], BUFSIZ);
    rc = vsprintf(buf, Format, &ap[0], reinterpret_cast<uintptr_t>(Args));
    InfoPrintf(buf);

    return rc;
}

