/*
 * Copyright (c) 2013-2020, LWPU Corporation.  All rights reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from LWPU Corporation is strictly prohibited.
 */

/*
 * This file originates from CheetAh code base:
 * Repo: git-master.lwpu.com:12001/cheetah/bootloader/lwtboot
 * Branch: Master
 * Commit: 5dc8a5123649165aa9adca7d976e5df79d6bb176
 * File: common/lib/debug/lwtboot_debug.c
 * File MD5: 9e36645a615840f2248bb9c195cd1d90
 * This code was mostly ported by Sukanto Ghosh (form previous CheetAh bootloaders).
 *
 * liblwriscv version is branched off chips_a:
 * //sw/dev/gpu_drv/chips_a/uproc/lwriscv/shlib/src/shlib/lw_vprintfmt.c#3
 */

/* Debug printf utilities, semihosting, assertion, etc */
#include <stdarg.h>
#include <stddef.h>

#include "common.h"

#define PRINTNUM_MAX_LEN 64

static int s_printNum(void (*putch)(int, void*), void *putdat,
                    unsigned long long n, int sign, int pad,
                    int alternate_form, unsigned radix, char padchar)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    int len = 0;
    int total;
    unsigned long long x = n;

    // callwlate length
    do
    {
        x /= radix;
        len++;
    } while (x > 0);

    if (sign < 0)
    {
        len++;
    }

    if (alternate_form)
    {
        if (radix == 8)
        {
            len++;
        } else if (radix == 16)
        {
            len += 2;
        }
    }

    if (len > pad)
    {
        total = len;
        pad = 0;
    }
    else
    {
        total = pad;
        pad -= len;
    }

    // pad
    while (pad--)
    {
        putch(padchar, putdat);
    }

    // write sign
    if (sign < 0)
    {
        putch('-', putdat);
        len--;
    }

    // prefix '0x' (hex-values) or '0' (octal-values)
    if (alternate_form)
    {
        if (radix == 8)
        {
            putch('0', putdat);
            len--;
        } else if (radix == 16)
        {
            putch('0', putdat);
            putch('x', putdat);
            len -= 2;
        }
    }

    // write integer
    if (len > PRINTNUM_MAX_LEN)
    {
        len = PRINTNUM_MAX_LEN;
    }

    {
        char num[PRINTNUM_MAX_LEN];
        int i;

        x = n;
        i = len;
        while (i--)
        {
            num[i] = digits[x % radix];
            x /= radix;
        }
        for (i=0; i<len; ++i)
        {
            putch(num[i], putdat);
        }
    }

    return total;
}

int vprintfmt(void (*putch)(int, void*), void *putdat,
              const char* pFormat, va_list ap)
{
    const char* f = pFormat;
    int total = 0; // total chars written
    char padchar = ' ';

    while (1)
    {
        int wrote = 0;
        char lwr = *(f++);

        // end of pFormat
        if (lwr == '\0')
        {
            break;
        }

        // pFormatted argument
        if (lwr != '%')
        {
            // print string literals
            putch(lwr, putdat);
            wrote = 1;
        }
        else
        {
            const char* arg = f;
            int pad = 0;
            int longint = 0;
            int sign = 0;
            int base = 0;
            int alternate_form = 0;

            if (*arg == '#')
            {
                alternate_form = 1;
                arg++;
            }

            // support for %0N zero-padding
            if (*arg == '0')
            {
                padchar = '0';
                arg++;
            }

            while ((*arg >= '0') && (*arg <= '9'))
            {
                pad *= 10;
                pad += (*(arg++)) - '0';
            }

            if (*arg == 'p')
            {
                // %p == %#lx
                alternate_form = 1;
                longint = 1;
            }

            if (*arg == 'l')
            {
                arg++;
                longint = 1;
                if (*(arg) == 'l')
                {
                    arg++;
                    longint = 2;
                }
            }

            switch (*(arg++))
            {
            case 'i':
                sign = 1;
            __attribute__ ((fallthrough));
            case 'u':
                base = 10; __attribute__ ((fallthrough));
            case 'o':
                if (!base)
                {
                    base = 8;
                } __attribute__ ((fallthrough));
            case 'p':
            case 'x':
            case 'X':
                {
                    unsigned long long val;

                    if (!base)
                        base = 16;

                    if (longint == 2)
                    {
                        long long int tmp = va_arg(ap, long long int);
                        if ((sign) && (tmp < 0))
                        {
                            sign = -1;
                            tmp = -tmp;
                        }
                        val = (unsigned long long int)tmp;
                    } else if (longint == 1)
                    {
                        long int tmp = va_arg(ap, long int);
                        if ((sign) && (tmp < 0))
                        {
                            sign = -1;
                            tmp = -tmp;
                        }
                        val = (unsigned long int)tmp;
                    } else
                    {
                        int tmp = va_arg(ap, int);
                        if ((sign) && (tmp < 0))
                        {
                            sign = -1;
                            tmp = -tmp;
                        }
                        val = (unsigned int)tmp;
                    }

                    wrote = s_printNum(putch, putdat, val, sign, pad,
                                       alternate_form, (unsigned)base, padchar);
                }
                break;
            case 's':
                {
                    char* val = va_arg(ap, char *);

                    wrote = 0;
                    while (val[wrote])
                    {
                        putch(val[wrote], putdat);
                        wrote++;
                    }

                    if (wrote == 0)
                    {
                        f = arg;
                        wrote = -2;
                    }
                }
                break;
            case 'c':
                {
                    char val = (char)va_arg(ap, unsigned);

                    putch(val, putdat);
                    wrote = 1;
                }
                break;
            case '%':
                putch('%', putdat);
                wrote = 1;
                break;
            default:
                // unsupported, just return with whatever already printed
                wrote = -1;
                break;
            }

            if (wrote >= 0)
                f = arg;
        }

        if (wrote == -1)
            break;

        // put this after the previous if statement (wrote == 0)
        if (wrote == -2)
        {
            wrote = 0;
        }

        total += wrote;
    }

    return total;
}
