/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <lwmisc.h>

#include "debug.h"
#include "csr.h"
#include "engine.h"
#include "io.h"
#include <lw_riscv_address_map.h>

#define static_assert(cond) switch(0) { case 0: case !!(long)(cond): ; }

// DMESG ring buffer
typedef struct {
    uint32_t read_ofs;
    uint32_t write_ofs;
    uint32_t buffer_size;
    uint32_t magic;
} RiscvDbgDmesgHdr;

#define RISCV_DMESG_MAGIC    0xF007BA11U

volatile char *dmesg_buffer = NULL;
static volatile RiscvDbgDmesgHdr *hdr;
static int initialized = 0;

unsigned delay(unsigned cycles)
{
    uint64_t r = csr_read(LW_RISCV_CSR_MHPMCOUNTER_MCYCLE) + cycles;
    uint64_t a = 0;

    while (a < r)
    {
        a = csr_read(LW_RISCV_CSR_MHPMCOUNTER_MCYCLE);
    }

    return a - (r - cycles);
}

static void dbgPutCh(char ch, int block)
{
    if (!initialized)
    {
        LwU32 r;

        r = engineRead(LW_PFALCON_FALCON_HWCFG3);
        r = DRF_VAL(_PFALCON, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, r);
        r = r << 8; // its in pages

        dmesg_buffer = (char *)(LW_RISCV_AMAP_DMEM_START + r - 0x1000);

        hdr = (RiscvDbgDmesgHdr *)(LW_RISCV_AMAP_DMEM_START + r - sizeof(RiscvDbgDmesgHdr));

        hdr->magic = RISCV_DMESG_MAGIC;
        hdr->read_ofs = 0;
        hdr->write_ofs = 0;
        hdr->buffer_size = 0x1000 - sizeof(RiscvDbgDmesgHdr);
        lwfenceAll();

        initialized = 1;
    }

    // Block until we have place in bufffer
    if (block)
    {
        while ( ((hdr->write_ofs + 1) % hdr->buffer_size) == hdr->read_ofs );
    }

    dmesg_buffer[hdr->write_ofs++] = ch;

    hdr->write_ofs = hdr->write_ofs % hdr->buffer_size;
}

int putchar(int ch)
{
    dbgPutCh(ch, 0);
    return 0;
}


#define PRINTNUM_MAX_LEN 64

static int s_printNum(void (*putch)(int, void**), void **putdat,
                    unsigned long long n, int sign, size_t pad,
                    int alternate_form, unsigned radix, char padchar)
{
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    size_t len = 0;
    size_t total;
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
        size_t i;

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

int vprintfmt(void (*putch)(int, void**), void **putdat,
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
            case 'd':
                sign = 1;
                __attribute__ ((fallthrough));
            case 'u':
                base = 10;
                __attribute__ ((fallthrough));
            case 'o':
                if (!base)
                    base = 8;
                __attribute__ ((fallthrough));
            case 'p':
            case 'x':
            case 'X':
                {
                    unsigned long long val;

                    if (!base)
                        base = 16;

                    if (longint == 2)
                    {
                        long long int tmp = va_arg(ap, unsigned long long int);
                        if ((sign) && (tmp < 0))
                        {
                            sign = -1;
                            tmp = -tmp;
                        }
                        val = (unsigned long long int)tmp;
                    } else if (longint == 1)
                    {
                        long int tmp = va_arg(ap, unsigned long int);
                        if ((sign) && (tmp < 0))
                        {
                            sign = -1;
                            tmp = -tmp;
                        }
                        val = (unsigned long int)tmp;
                    } else
                    {
                        int tmp = va_arg(ap, unsigned int);
                        if ((sign) && (tmp < 0))
                        {
                            sign = -1;
                            tmp = -tmp;
                        }
                        val = (unsigned int)tmp;
                    }

                    wrote = s_printNum(putch, putdat, val, sign, pad,
                                       alternate_form, base, padchar);
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

int printf(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  vprintfmt((void*)putchar, 0, fmt, ap);

  va_end(ap);
  return 0; // incorrect return value, but who cares, anyway?
}

int sprintf(char* str, const char* fmt, ...)
{
  va_list ap;
  char* str0 = str;
  va_start(ap, fmt);

  void sprintf_putch(int ch, void** data)
  {
    char** pstr = (char**)data;
    **pstr = ch;
    (*pstr)++;
  }

  vprintfmt(sprintf_putch, (void**)&str, fmt, ap);
  *str = 0;

  va_end(ap);
  return str - str0;
}

void memcpy(void *dst, const void *src, unsigned count)
{
    char *d=dst;
    const char *s = src;
    while (count--)
        *d++ = *s++;
}

int memcmp(const void *s1, const void *s2, unsigned n)
{
    const unsigned char *d=s1;
    const unsigned char *s = s2;
    while (n--)
    {
        unsigned char x = (*s) - (*d);
        if (x)
            return x;
        d++;
        s++;
    }
    return 0;
}

void memset(void *s, int c, unsigned n)
{
    uint8_t *p = s;
    while (n--)
        *p++ = c;
}

static int isprint(int c)
{
    return c >= ' ' && c <= '~';
}

void dumpHex(const char *data, unsigned size, unsigned offs)
{
    unsigned i, last_start=0;

    for (i=0; i<size; ++i)
    {
        if (i % 16 == 0) {// newline
            printf("%llx: ", i+offs);
            last_start = i;
        }
        printf("%02x ", (unsigned)data[i] & 0xff);
        if (((i % 16) == 15) || ((i + 1) == size)) // end of line or data
        {
            unsigned j;
            printf(" ");
            for (j = last_start; j <= i; j+=4)
            {
                printf("%08x ", ((const uint32_t*)data)[j/4]);
            }
            printf(" ");
            for (j = last_start; j <= i; ++j)
            {
                if (isprint(data[j]))
                    printf("%c", data[j]);
                else
                    printf(".");
            }
            printf("\n");
        }
    }
    if (size % 16 != 0)
        printf("\n");
}
