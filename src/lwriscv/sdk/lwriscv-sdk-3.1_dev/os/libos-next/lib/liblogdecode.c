/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * ----------------------------------------------------------------------
 * Copyright ??? 2005-2014 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ----------------------------------------------------------------------
 */

#ifdef LWRM

#    include <core/core.h>
#    include <stddef.h> // size_t

#    define printf(fmt, ...) portDbgExPrintfLevel(LEVEL_ERROR, fmt, ##__VA_ARGS__)
#    define snprintf         lwDbgSnprintf

#else // LWRM

#    include <stdio.h>
#    include <stdlib.h>
#    include <ctype.h>
#    include <string.h>

#    define portStringCopy(d, ld, s, ls) strncpy(d, s, ld)
#    define portStringLength(s)          strlen(s)
#    define portStringLengthSafe(s, l)   strlen(s)

#    define portMemCopy(d, ld, s, ls) memcpy(d, s, ld)
#    define portMemSet(d, v, l)       memset(d, v, l)
#    define portMemCmp(d, s, l)       memcmp(d, s, l)
#    define portMemAllocNonPaged(l)   malloc(l);
#    define portMemFree(p)            free(p);

#    ifdef LWSYM_STANDALONE
#        define printf(fmt, ...) fprintf(logDecode->fout, fmt, ##__VA_ARGS__)
#    endif

#endif // LWRM

#include "lwtypes.h"
#include "logdecode.h"

#if LIBOS_LOG_DECODE_ENABLE

#    define SYM_DECODED_LINE_MAX_SIZE 1024

// These defines assume RISCV with -mabi=lp64/lp64f/lp64d
#    define LOG_INT_MAX     LW_S32_MAX
#    define LOG_UINT_MAX    LW_U32_MAX
#    define LOG_LONG_MAX    LW_S64_MAX
#    define LOG_ULONG_MAX   LW_U64_MAX
#    define LOG_INTMAX_MAX  LW_S64_MAX
#    define LOG_UINTMAX_MAX LW_U64_MAX
#    define LOG_LLONG_MAX   LW_S64_MAX
#    define LOG_ULLONG_MAX  LW_U64_MAX
#    define LOG_SIZE_MAX    LW_U64_MAX
#    define NL_ARGMAX  32

/* Some useful macros */
#    define MAX(a, b) ((int)(a) > (int)(b) ? (int)(a) : (int)(b))
#    define MIN(a, b) ((int)(a) < (int)(b) ? (int)(a) : (int)(b))

#    define IS_DIGIT(c) (((c) >= '0') && ((c) <= '9'))

/* Colwenient bit representation for modifier flags, which all fall
 * within 31 codepoints of the space character. */
#    define ALT_FORM (1U << ('#' - ' '))
#    define ZERO_PAD (1U << ('0' - ' '))
#    define LEFT_ADJ (1U << ('-' - ' '))
#    define PAD_POS  (1U << (' ' - ' '))
#    define MARK_POS (1U << ('+' - ' '))
#    define GROUPED  (1U << ('\'' - ' '))
#    define FLAGMASK (ALT_FORM | ZERO_PAD | LEFT_ADJ | PAD_POS | MARK_POS | GROUPED)
#    if LOG_UINT_MAX == LOG_ULONG_MAX
#        define LOG_LONG_IS_INT
#    endif
#    if LOG_SIZE_MAX != LOG_ULONG_MAX || LOG_UINTMAX_MAX != LOG_ULLONG_MAX
#        define LOG_ODD_TYPES
#    endif

#if defined(LOG_LONG_IS_INT) || defined(LOG_ODD_TYPES)
#error "Type sizes don't match RISCV lp64 ABI!"
#endif // defined(LOG_LONG_IS_INT) || defined(LOG_ODD_TYPES)

/* State machine to accept length modifiers + colwersion specifiers.
 * Result is 0 on failure, or an argument type to pop on success. */
enum
{
    LOG_BARE,
    LOG_LPRE,
    LOG_LLPRE,
    LOG_HPRE,
    LOG_HHPRE,
    LOG_BIGLPRE,
    LOG_ZTPRE,
    LOG_JPRE,
    LOG_STOP,
    LOG_PTR,
    LOG_INT,
    LOG_UINT,
    LOG_ULLONG,
#    ifndef LOG_LONG_IS_INT
    LOG_LONG,
    LOG_ULONG,
#    else
#        define LOG_LONG  LOG_INT
#        define LOG_ULONG LOG_UINT
#    endif
    LOG_SHORT,
    LOG_USHORT,
    LOG_CHAR,
    LOG_UCHAR,
#    ifdef LOG_ODD_TYPES
    LOG_LLONG,
    LOG_SIZET,
    LOG_IMAX,
    LOG_UMAX,
    LOG_PDIFF,
    LOG_UIPTR,
#    else
#        define LOG_LLONG LOG_ULLONG
#        define LOG_SIZET LOG_ULONG
#        define LOG_IMAX  LOG_LLONG
#        define LOG_UMAX  LOG_ULLONG
#        define LOG_PDIFF LOG_LONG
#        define LOG_UIPTR LOG_ULONG
#    endif
    LOG_NOARG,
    LOG_MAXSTATE
};
#    define S(x) [(x) - 'A']
static const unsigned char states[]['z' - 'A' + 1] = {
    {
        /* 0: bare types */
        S('d') = LOG_INT,   S('i') = LOG_INT,   S('o') = LOG_UINT,  S('u') = LOG_UINT,    S('x') = LOG_UINT,
        S('X') = LOG_UINT,  S('c') = LOG_CHAR,  S('C') = LOG_INT,   S('s') = LOG_PTR,     S('S') = LOG_PTR,
        S('p') = LOG_UIPTR, S('n') = LOG_PTR,   S('a') = LOG_UIPTR, /* LWPU decoded address extension */
        S('m') = LOG_NOARG, S('l') = LOG_LPRE,  S('h') = LOG_HPRE,  S('L') = LOG_BIGLPRE, S('z') = LOG_ZTPRE,
        S('j') = LOG_JPRE,  S('t') = LOG_ZTPRE,
    },
    {
        /* 1: l-prefixed */
        S('d') = LOG_LONG,
        S('i') = LOG_LONG,
        S('o') = LOG_ULONG,
        S('u') = LOG_ULONG,
        S('x') = LOG_ULONG,
        S('X') = LOG_ULONG,
        S('c') = LOG_INT,
        S('s') = LOG_PTR,
        S('n') = LOG_PTR,
        S('l') = LOG_LLPRE,
    },
    {
        /* 2: ll-prefixed */
        S('d') = LOG_LLONG,
        S('i') = LOG_LLONG,
        S('o') = LOG_ULLONG,
        S('u') = LOG_ULLONG,
        S('x') = LOG_ULLONG,
        S('X') = LOG_ULLONG,
        S('n') = LOG_PTR,
    },
    {
        /* 3: h-prefixed */
        S('d') = LOG_SHORT,
        S('i') = LOG_SHORT,
        S('o') = LOG_USHORT,
        S('u') = LOG_USHORT,
        S('x') = LOG_USHORT,
        S('X') = LOG_USHORT,
        S('n') = LOG_PTR,
        S('h') = LOG_HHPRE,
    },
    {
        /* 4: hh-prefixed */
        S('d') = LOG_CHAR,
        S('i') = LOG_CHAR,
        S('o') = LOG_UCHAR,
        S('u') = LOG_UCHAR,
        S('x') = LOG_UCHAR,
        S('X') = LOG_UCHAR,
        S('n') = LOG_PTR,
    },
    {
        /* 5: L-prefixed */
        S('n') = LOG_PTR,
    },
    {
        /* 6: z- or t-prefixed (assumed to be same size) */
        S('d') = LOG_PDIFF,
        S('i') = LOG_PDIFF,
        S('o') = LOG_SIZET,
        S('u') = LOG_SIZET,
        S('x') = LOG_SIZET,
        S('X') = LOG_SIZET,
        S('n') = LOG_PTR,
    },
    {
        /* 7: j-prefixed */
        S('d') = LOG_IMAX,
        S('i') = LOG_IMAX,
        S('o') = LOG_UMAX,
        S('u') = LOG_UMAX,
        S('x') = LOG_UMAX,
        S('X') = LOG_UMAX,
        S('n') = LOG_PTR,
    }};
#    define OOB(x) ((unsigned)(x) - 'A' > 'z' - 'A')
union arg
{
    LwU64 i;
    void *p;
};

/**
 *
 * @brief Print out the line buffer and reset the current line buffer pointer.
 *
 * @param[in/out] logDecode
 *   Structure used to decode log.  Contains both data set at init and working fields.
 */
static void flush_line_buffer(LIBOS_LOG_DECODE *logDecode)
{
    if (logDecode->lwrLineBufPtr != logDecode->lineBuffer)
    {
        /* Make sure line is NULL terminated */
        *logDecode->lwrLineBufPtr = 0;
        printf("%s", logDecode->lineBuffer);
        logDecode->lwrLineBufPtr = logDecode->lineBuffer;
    }
}

/**
 *
 * @brief Copy string to the line buffer.
 *
 * Copy string until 0 encountered up to maximum length l.
 * Flush the line buffer if it gets full.
 *
 * @param[in] s
 *   String to copy.  May be zero-terminated.
 * @param[in] l
 *   Maximum length to copy, if zero is not encountered first.
 * @param[in/out] logDecode
 *   Structure used to decode log.  Contains both data set at init and working fields.
 */
static void emit_string(const char *s, int l, LIBOS_LOG_DECODE *logDecode)
{
    char *line_buffer_end = logDecode->lineBuffer + LIBOS_LOG_LINE_BUFFER_SIZE - 1;
    for (; (l > 0) && (*s != 0); s++)
    {
        if (logDecode->lwrLineBufPtr >= line_buffer_end)
            flush_line_buffer(logDecode);

        *logDecode->lwrLineBufPtr++ = *s;
        l--;
    }
}

static void s_getSymbolDataStr(LIBOS_LOG_DECODE *logDecode, char *decodedLine, LwLength decodedLineSize, LwUPtr addr)
{
    const char *directory;
    const char *filename;
    const char *name;
    LwU64 offset;
    LwU64 outputLine;
    LwU64 outputColumn;
    LwU64 matchedAddress;
    if (!libosDebugResolveSymbolToName(&logDecode->resolver, addr, &name, &offset))
    {
        name = 0;
    }
    decodedLine[decodedLineSize - 1U] = '\0';

    if (libosDwarfResolveLine(
            &logDecode->resolver, addr, &directory, &filename, &outputLine, &outputColumn,
            &matchedAddress))
    {
        if (name)
        {
            snprintf(
                decodedLine, decodedLineSize - 1, "%s+%lld (%s:%lld)", name, offset, filename,
                outputLine);
        }
        else
        {
            snprintf(decodedLine, decodedLineSize - 1, "??? (%s:%lld)", filename, outputLine);
        }
    }
    else
    {
        snprintf(decodedLine, decodedLineSize - 1, "%s+%lld", name, offset);
    }
}

/**
 *
 * @brief Pad a field with ' ' or '0'.
 *
 * This routine is called with different options for left-justified and
 * right-justified fields.
 *
 * @param[in] c
 *   Pad with this character.  Usually '0' or ' '.
 * @param[in] w
 *   Desired width after padding.
 * @param[in] l
 *   Length of field so far.  Pad for w - l.
 * @param[in] fl
 *   Modifier flags.  See FLAGMASK above.
 * @param[in/out] logDecode
 *   Structure used to decode log.  Contains both data set at init and working fields.
 */
static void pad(char c, int w, int l, int fl, LIBOS_LOG_DECODE *logDecode)
{
    char *line_buffer_end = logDecode->lineBuffer + LIBOS_LOG_LINE_BUFFER_SIZE - 1;
    if (fl & (LEFT_ADJ | ZERO_PAD) || l >= w)
        return;
    l = w - l;
    for (; l > 0; l--)
    {
        if (logDecode->lwrLineBufPtr >= line_buffer_end)
            flush_line_buffer(logDecode);
        *logDecode->lwrLineBufPtr++ = c;
    }
}
static char *fmt_x(LwU64 x, char *s, int lower)
{
    static const char xdigits[16] = {"0123456789ABCDEF"};
    for (; x; x >>= 4)
        *--s = xdigits[(x & 15)] | lower;
    return s;
}
static char *fmt_o(LwU64 x, char *s)
{
    for (; x; x >>= 3)
        *--s = '0' + (x & 7);
    return s;
}
static char *fmt_u(LwU64 x, char *s)
{
    unsigned long y;
    for (; x > LOG_ULONG_MAX; x /= 10)
        *--s = '0' + x % 10;
    for (y = x; y; y /= 10)
        *--s = '0' + y % 10;
    return s;
}
static int getint(char **s)
{
    int i;
    for (i = 0; IS_DIGIT(**s); (*s)++)
        i = 10 * i + (**s - '0');
    return i;
}
static int libos_printf_a(
    LIBOS_LOG_DECODE *logDecode, LIBOS_LOG_DECODE_RECORD *pRec, const char *fmt, const char *filename)
{
    LwU64 *args                     = pRec->args;
    LwU64 arg_count                 = pRec->meta->argumentCount;
    union arg nl_arg[NL_ARGMAX + 1] = {{0}};
    char *a, *z, *s = (char *)fmt;
    unsigned l10n = 0, fl;
    int w, p;
    union arg arg = {0};
    int argpos;
    unsigned st, ps;
    int cnt = 0, l = 0;
    char buf[sizeof(LwU64) * 3 + 3];
    const char *prefix;
    int t, pl;
    LwU64 arg_index = 0;
    char wc[2];
    char *line_buffer_end = logDecode->lineBuffer + LIBOS_LOG_LINE_BUFFER_SIZE - 1;
    LwBool bResolvePtrVal = LW_FALSE;

    for (;;)
    {
        /* Update output count, end loop when fmt is exhausted */
        if (cnt >= 0)
        {
            if (l > LOG_INT_MAX - cnt)
            {
                cnt = -1;
            }
            else
                cnt += l;
        }

#    if defined(LWRM)
        if (logDecode->lwrLineBufPtr == logDecode->lineBuffer)
        {
            // Prefix every line with LWRM GPUn Ucode-task: filename(lineNumber):
            snprintf(
                logDecode->lwrLineBufPtr, LIBOS_LOG_LINE_BUFFER_SIZE - 1,
                "LWRM GPU%u %s-%s: %s(%u): ", pRec->log->gpuInstance,
                logDecode->sourceName, pRec->log->taskPrefix, filename, pRec->meta->lineNumber);
            logDecode->lwrLineBufPtr += portStringLength(logDecode->lwrLineBufPtr);
        }
#    elif defined(LWSYM_STANDALONE)
        if (logDecode->lwrLineBufPtr == logDecode->lineBuffer)
        {
            // Prefix every line with GPUn Ucode-task: filename(lineNumber):
            snprintf(
                logDecode->lwrLineBufPtr, LIBOS_LOG_LINE_BUFFER_SIZE - 1,
                "T:%llu GPU%u %s-%s: %s(%u): ", pRec->timeStamp,
                pRec->log->gpuInstance, logDecode->sourceName, pRec->log->taskPrefix,
                filename, pRec->meta->lineNumber);
            logDecode->lwrLineBufPtr += portStringLength(logDecode->lwrLineBufPtr);
        }
#    endif

        /* Handle literal text and %% format specifiers */
        for (; *s; s++)
        {
            if (logDecode->lwrLineBufPtr >= line_buffer_end)
                flush_line_buffer(logDecode);

            if (*s == '%')
            {
                if (s[1] == '%')
                    s++;
                else
                    break;
            }

            *logDecode->lwrLineBufPtr++ = *s;

            if (*s == '\n')
                break;
        }

        if (*s == '\n')
        {
            flush_line_buffer(logDecode);
            s++;

            if (!*s)
                break;
            else
                continue;
        }
        if (!*s)
            break;

        a = s;
        z = s;
        l = z - a;
        if (IS_DIGIT(s[1]) && s[2] == '$')
        {
            l10n   = 1;
            argpos = s[1] - '0';
            s += 3;
        }
        else
        {
            argpos = -1;
            s++;
        }
        /* Read modifier flags */
        for (fl = 0; (unsigned)*s - ' ' < 32 && (FLAGMASK & (1U << (*s - ' '))); s++)
            fl |= 1U << (*s - ' ');
        /* Read field width */
        if (*s == '*')
        {
            if (IS_DIGIT(s[1]) && s[2] == '$')
            {
                l10n = 1;
                w    = nl_arg[s[1] - '0'].i;
                s += 3;
            }
            else if (!l10n)
            {
                if (arg_index >= arg_count)
                    return 0;
                w = args[arg_index++];
                s++;
            }
            else
                return -1;
            if (w < 0)
                fl |= LEFT_ADJ, w = -w;
        }
        else if ((w = getint(&s)) < 0)
            return -1;
        /* Read precision */
        if (*s == '.' && s[1] == '*')
        {
            if (IS_DIGIT(s[2]) && s[3] == '$')
            {
                p = nl_arg[s[2] - '0'].i;
                s += 4;
            }
            else if (!l10n)
            {
                if (arg_index >= arg_count)
                    return 0;
                p = args[arg_index++];
                s += 2;
            }
            else
                return -1;
        }
        else if (*s == '.')
        {
            s++;
            p = getint(&s);
        }
        else
            p = -1;
        /* Format specifier state machine */
        st = 0;
        do
        {
            if (OOB(*s))
                return -1;
            ps = st;
            st = states[st] S(*s++);
        } while (st - 1 < LOG_STOP);
        if (!st)
            return -1;
        /* Check validity of argument type (nl/normal) */
        if (st == LOG_NOARG)
        {
            if (argpos >= 0)
                return -1;
        }
        else
        {
            if (argpos < 0)
            {
                if (arg_index >= arg_count)
                    return 0;
                arg.i = args[arg_index++];
            }
        }
        z      = buf + sizeof(buf);
        prefix = "-+   0X0x";
        pl     = 0;
        t      = s[-1];
        /* Transform ls,lc -> S,C */
        if (ps && (t & 15) == 3)
            t &= ~32;
        /* - and 0 flags are mutually exclusive */
        if (fl & LEFT_ADJ)
            fl &= ~ZERO_PAD;

        bResolvePtrVal = LW_FALSE;
        switch (t)
        {
        case 'n':
#if !LIBOS_LOG_DECODE_ENABLE
            // We can't support %n when decoding, these pointers do not exist here!
            switch (ps)
            {
            case LOG_BARE:
                *(int *)arg.p = cnt;
                break;
            case LOG_LPRE:
                *(long *)arg.p = cnt;
                break;
            case LOG_LLPRE:
                *(long long *)arg.p = cnt;
                break;
            case LOG_HPRE:
                *(unsigned short *)arg.p = cnt;
                break;
            case LOG_HHPRE:
                *(unsigned char *)arg.p = cnt;
                break;
            case LOG_ZTPRE:
                *(size_t *)arg.p = cnt;
                break;
            case LOG_JPRE:
                *(LwU64 *)arg.p = cnt;
                break;
            }
#endif // !LIBOS_LOG_DECODE_ENABLE
            continue;
        case 'p':
            t = 'x';
            fl |= ALT_FORM;

            if (logDecode->bPtrSymbolResolve)
            {
                bResolvePtrVal = LW_TRUE;
            }
        case 'x':
        case 'X':
            a = fmt_x(arg.i, z, t & 32);
            if (arg.i && (fl & ALT_FORM))
                prefix += (t >> 4), pl = 2;
            if (0)
            {
            case 'o':
                a = fmt_o(arg.i, z);
                if ((fl & ALT_FORM) && arg.i)
                    prefix += 5, pl = 1;
            }
            if (0)
            {
            case 'd':
            case 'i':
                pl = 1;
                if (arg.i > LOG_INTMAX_MAX)
                {
                    arg.i = -(LwS64)arg.i;
                }
                else if (fl & MARK_POS)
                {
                    prefix++;
                }
                else if (fl & PAD_POS)
                {
                    prefix += 2;
                }
                else
                    pl = 0;
            case 'u':
                a = fmt_u(arg.i, z);
            }
            if (p >= 0)
                fl &= ~ZERO_PAD;
            if (!arg.i && !p)
            {
                a = z;
                break;
            }
            p = MAX(p, z - a + !arg.i);
            break;
        case 'c':
            *(a = z - (p = 1)) = arg.i;
            fl &= ~ZERO_PAD;
            break;
        case 'a':
            {
                static char symDecodedLine[SYM_DECODED_LINE_MAX_SIZE];

                s_getSymbolDataStr(logDecode, symDecodedLine, sizeof(symDecodedLine), (LwUPtr)arg.i);

                // Set common vars
                a = &symDecodedLine[0];
                z = &symDecodedLine[sizeof(symDecodedLine) - 1];
            }
            goto print_string;
        case 's':
            a = (char *)libosElfReadStringVirtual(logDecode->elf, (LwUPtr)arg.p);
            if (!a)
                a = "(bad-pointer)";
        print_string:
            p = portStringLengthSafe(a, p);
            z = a + p;
            fl &= ~ZERO_PAD;
            break;
        case 'C':
            wc[0] = arg.i;
            wc[1] = 0;
            arg.p = wc;
            p     = -1;
        }
        if (p < z - a)
            p = z - a;
        if (w < pl + p)
            w = pl + p;
        pad(' ', w, pl + p, fl, logDecode);
        emit_string(prefix, pl, logDecode);
        pad('0', w, pl + p, fl ^ ZERO_PAD, logDecode);
        pad('0', p, z - a, 0, logDecode);
        emit_string(a, z - a, logDecode);

        if (bResolvePtrVal)
        {
            // Append symbol info to ptr addr value in the following format: 0x123 <sym+data>

            static char symDecodedLine[SYM_DECODED_LINE_MAX_SIZE];

            symDecodedLine[0] = ' ';
            symDecodedLine[1] = '<';

            s_getSymbolDataStr(logDecode, symDecodedLine + 2, sizeof(symDecodedLine) - 2, (LwUPtr)arg.i);

            LwLength symDecodedLineLen = portStringLength(symDecodedLine);
            symDecodedLineLen = MIN(symDecodedLineLen, sizeof(symDecodedLine) - 1);
            symDecodedLine[symDecodedLineLen] = '>';
            symDecodedLine[symDecodedLineLen + 1] = '\0';

            // Set common vars
            a = &symDecodedLine[0];
            z = &symDecodedLine[sizeof(symDecodedLine) - 1];

            emit_string(a, z - a, logDecode);
        }

        pad(' ', w, pl + p, fl ^ LEFT_ADJ, logDecode);
        l = w;
    }
    return cnt;
}

/**
 *
 * @brief Print log records from scratch buffer.
 *
 * Prints log records from the scratch buffer scanning forwards.
 *
 * @param[in/out] logDecode
 *   Pointer to LIBOS_LOG_DECODE structure.
 * @param[in] scratchBuffer
 *   Pointer to first byte in scratch buffer that contains valid data.
 * @param[int] valid_elements
 *   Number of valid elements in the scratch buffer.
 */
static void libosPrintLogRecords(LIBOS_LOG_DECODE *logDecode, LwU64 *scratchBuffer, LwU64 valid_elements)
{
    LwU64 index;
    LwU64 i;

    if (valid_elements == 0)
        return;

    for (index = 0; index < valid_elements;)
    {
        LIBOS_LOG_DECODE_RECORD *pRec = (LIBOS_LOG_DECODE_RECORD *)&scratchBuffer[index];

        if (pRec->meta == NULL)
        {
            printf(
                "**** Bad metadata.  Lost %lld entries from %s ****\n", valid_elements - index,
                logDecode->sourceName);
            return;
        }

        // Locate format string
        const char *format = libosElfReadStringVirtual(logDecode->elf, (LwU64)(LwUPtr)pRec->meta->format);
        if (!format)
            break;

        // Locate filename
        const char *filename = libosElfReadStringVirtual(logDecode->elf, (LwU64)(LwUPtr)pRec->meta->filename);
        if (!filename || !filename[0])
            filename = "unknown";

        // Strip off path
        for (i = portStringLength(filename) - 1; i > 0; i--)
        {
            if (filename[i] == '/')
            {
                i++;
                break;
            }
        }
        filename = &filename[i];

        // Format
        libos_printf_a(logDecode, pRec, format, filename);

        // Advance
        index += pRec->meta->argumentCount + LIBOS_LOG_DECODE_RECORD_BASE;
    }
}

#    define LIBOS_LOG_TIMESTAMP_END  0
#    define LIBOS_LOG_TIMESTAMP_MAX  LW_U64_MAX

/**
 *
 * @brief Extract a single log record from one log buffer.
 *
 * This routine is designed to scan backwards from the put pointer.  It changes
 * the order of the parameters from backward scanning order {args, meta data,
 * timestamp} to forward scanning order {pLog, meta data, timestamp args}.
 * It also decodes meta data into a pointer.
 *
 * pLog->putIter points to the start of the entry last successfully extracted.
 *
 * @param[in/out] logDecode
 *   Pointer to LIBOS_LOG_DECODE structure.
 * @param[in/out] pLog
 *   Pointer to LIBOS_LOG_DECODE_LOG structure for the log to extract from.
 *
 * timeStamp is set to LIBOS_LOG_TIMESTAMP_END (0) when there is an error or we
 *   run out of records.
 */
static void libosExtractLog_ReadRecord(LIBOS_LOG_DECODE *logDecode, LIBOS_LOG_DECODE_LOG *pLog)
{
    if (pLog->putIter == pLog->previousPut)
    {
        pLog->record.timeStamp = LIBOS_LOG_TIMESTAMP_END;
        return;
    }

    LwU64 log_entries = pLog->logBufferSize / sizeof(LwU64) - 1 /* -1 for PUT pointer */;
    LwU64 previousPut = pLog->previousPut;
    LwU64 i           = pLog->putIter;
    LwU64 argCount;
    LwU64 j;

    // If we wrapped, adjust local copy of previousPut.
    if (previousPut + log_entries < pLog->putCopy)
        previousPut = pLog->putCopy - log_entries;

    pLog->record.log = pLog;

    if (logDecode->bSynchronousBuffer)
    {
        // Fake timestamp for sync buffer, marked as different from the "end" timestamp
        pLog->record.timeStamp = LIBOS_LOG_TIMESTAMP_MAX;
    }
    else
    {
        // Check whether record goes past previous put (buffer wrapped).
        if (i < previousPut + 1)
            goto buffer_wrapped;

        pLog->record.timeStamp = pLog->physicLogBuffer[1 + (--i % log_entries)];
    }

    // Check whether record goes past previous put (buffer wrapped).
    if (i < previousPut + 1)
        goto buffer_wrapped;

    pLog->record.meta = (libosLogMetadata *)libosElfReadVirtual(
        logDecode->elf, pLog->physicLogBuffer[1 + (--i % log_entries)], sizeof(libosLogMetadata));

    // Sanity check meta data.
    if (pLog->record.meta == NULL || pLog->record.meta->argumentCount > LIBOS_LOG_MAX_ARGS)
    {
        printf(
            "**** Bad metadata.  Lost %lld entries from %s-%s ****\n", pLog->putIter - previousPut,
            logDecode->sourceName, pLog->taskPrefix);
        goto error_ret;
    }

    argCount = pLog->record.meta->argumentCount;

    // Check whether record goes past previous put (buffer wrapped).
    if (i < previousPut + argCount)
        goto buffer_wrapped;

    for (j = argCount; j > 0; j--)
    {
        pLog->record.args[j - 1] = pLog->physicLogBuffer[1 + (--i % log_entries)];
    }

    pLog->putIter = i;
    return;

buffer_wrapped:
    // Put pointer wrapped and caught up to us.  This means we lost entries.
    printf(
        "**** Buffer wrapped. Lost %lld entries from %s-%s ****\n", pLog->putIter - pLog->previousPut,
        logDecode->sourceName, pLog->taskPrefix);

error_ret:
    pLog->record.timeStamp = LIBOS_LOG_TIMESTAMP_END;
    return;
}

/**
 *
 * @brief Extract all log records from all log buffers.
 *
 * Copy log records from all buffers to the scratch buffer in order of time stamp.
 *
 * @param[in/out] logDecode
 *   Pointer to LIBOS_LOG_DECODE structure.
 */
static void libosExtractLogs_decode(LIBOS_LOG_DECODE *logDecode)
{
    LIBOS_LOG_DECODE_LOG *pLog;
    LIBOS_LOG_DECODE_RECORD *pPrevRec = NULL;
    LwU64 timeStamp;
    LwU64 scratchSize = logDecode->scratchBufferSize / sizeof(LwU64);
    LwU64 dst         = scratchSize;
    LwU64 recSize;
    LwU64 i;

    // Initialize iterators and prime the pump.
    for (i = 0; i < logDecode->numLogBuffers; i++)
    {
        pLog = &logDecode->log[i];

        if (!pLog->physicLogBuffer)
        {
            printf("logDecode->physicLogBuffer is NULL\n");
            return;
        }

        pLog->putCopy = pLog->physicLogBuffer[0];
        pLog->putIter = pLog->putCopy;
        libosExtractLog_ReadRecord(logDecode, pLog);
    }

    // Copy records in order of highest time stamp.
    for (;;)
    {
        timeStamp = LIBOS_LOG_TIMESTAMP_END;
        pLog      = NULL; // for debugging.

        // Find log with the highest timestamp.
        for (i = 0; i < logDecode->numLogBuffers; i++)
        {
            if (timeStamp < logDecode->log[i].record.timeStamp)
            {
                pLog      = &logDecode->log[i];
                timeStamp = pLog->record.timeStamp;
            }
        }

        if (timeStamp == LIBOS_LOG_TIMESTAMP_END)
            break;

        // Copy records with highest timestamp.
        recSize = pLog->record.meta->argumentCount + LIBOS_LOG_DECODE_RECORD_BASE;

        // Skip duplicate records.  The same record can be in both wrap and nowrap buffers.
        if ((pPrevRec == NULL) ||
            (pPrevRec->log->gpuInstance != pLog->gpuInstance) ||
            (portMemCmp(&pPrevRec->meta, &pLog->record.meta, (recSize - 1) * sizeof(LwU64)) != 0))
        {
            // Record is not identical to previous record.
            if (dst < recSize)
            {
                printf("**** scratch buffer overflow.  lost entries ****\n");
                break;
            }

            dst -= recSize;
            portMemCopy(
                &logDecode->scratchBuffer[dst], recSize * sizeof(LwU64), &pLog->record,
                recSize * sizeof(LwU64));

            pPrevRec = (LIBOS_LOG_DECODE_RECORD *)&logDecode->scratchBuffer[dst];
        }

        // Read in the next record from the log we just copied.
        libosExtractLog_ReadRecord(logDecode, pLog);
    }

    // Update the previous put pointers.
    for (i = 0; i < logDecode->numLogBuffers; i++)
        logDecode->log[i].previousPut = logDecode->log[i].putCopy;

    // Print out the copied records.
    if (dst != scratchSize)
        libosPrintLogRecords(logDecode, &logDecode->scratchBuffer[dst], scratchSize - dst);
}

#endif // LIBOS_LOG_DECODE_ENABLE

#if LIBOS_LOG_TO_LWLOG

#    define LIBOS_LOG_LWLOG_BUFFER_TAG(_name, _i) LwU32_BUILD((_name)[2], (_name)[1], (_name)[0], (LwU8)('1' + _i))

static LwBool libosCopyLogToLwlog_nowrap(LIBOS_LOG_DECODE_LOG *pLog)
{
    LWLOG_BUFFER *pLwLogBuffer = LwLogLogger.pBuffers[pLog->hLwLogNoWrap];
    LW_ASSERT_OR_RETURN((pLog->hLwLogNoWrap != 0) && (pLwLogBuffer != NULL), LW_FALSE);

    LIBOS_LOG_LWLOG_BUFFER *pNoWrapBuf = (LIBOS_LOG_LWLOG_BUFFER *)pLwLogBuffer->data;
    LwU64 putCopy                      = pLog->physicLogBuffer[0];
    LwU64 putOffset                    = putCopy * sizeof(LwU64) + sizeof(LwU64);

    if (putOffset == pLwLogBuffer->pos)
    {
        // No new data
        return LW_TRUE;
    }

    if (putOffset > pLwLogBuffer->size - sizeof(LwU64))
    {
        // Are we done filling nowrap?
        return LW_FALSE;
    }

    LwU64 len  = putOffset - pLwLogBuffer->pos;
    LwU8 *pSrc = ((LwU8 *)pLog->physicLogBuffer) + pLwLogBuffer->pos;
    LwU8 *pDst = pNoWrapBuf->data + pLwLogBuffer->pos;
    portMemCopy(pDst, len, pSrc, len);
    pLwLogBuffer->pos            = putOffset; // TODO: usage of LWLOG_BUFFER::pos is sus here, reconsider?
    *(LwU64 *)(pNoWrapBuf->data) = putCopy;
    return LW_TRUE;
}

static LwBool libosCopyLogToLwlog_wrap(LIBOS_LOG_DECODE_LOG *pLog)
{
    LWLOG_BUFFER *pLwLogBuffer = LwLogLogger.pBuffers[pLog->hLwLogWrap];
    LW_ASSERT_OR_RETURN((pLog->hLwLogWrap != 0) && (pLwLogBuffer != NULL), LW_FALSE);

    LIBOS_LOG_LWLOG_BUFFER *pWrapBuf = (LIBOS_LOG_LWLOG_BUFFER *)pLwLogBuffer->data;
    LwU64 putCopy                    = pLog->physicLogBuffer[0];
    LwU64 putOffset                  = putCopy * sizeof(LwU64) + sizeof(LwU64);

    portMemCopy(pWrapBuf->data, pLog->logBufferSize, (void *)pLog->physicLogBuffer, pLog->logBufferSize);
    pLwLogBuffer->pos = putOffset; // TODO: usage of LWLOG_BUFFER::pos is sus here, reconsider?
    return LW_TRUE;
}

static void libosExtractLogs_lwlog(LIBOS_LOG_DECODE *logDecode, LwBool bSyncLwLog)
{
    LwU64 i;
    for (i = 0; i < logDecode->numLogBuffers; i++)
    {
        LIBOS_LOG_DECODE_LOG *pLog = &logDecode->log[i];

        if (pLog->bLwLogNoWrap)
        {
            pLog->bLwLogNoWrap = libosCopyLogToLwlog_nowrap(pLog);
        }

        if (bSyncLwLog)
        {
            libosCopyLogToLwlog_wrap(pLog);
        }
    }
}

#endif // LIBOS_LOG_TO_LWLOG

/**
 *
 * @brief Helper functions for creating and destroying log buffers.
 *
 * Call the functions in this order:
 *  libosLogCreate - Just clears the LIBOS_LOG_DECODE structure.
 *  libosLogAddLog - Call once for each log buffer.
 *  libosLogInit   - Sizes/allocates scratch buffer, and inits resolver.
 *  ...
 *  libosLogDestroy - Destroys resolver and frees scratch buffer. *
 */

#ifdef LWSYM_STANDALONE
void libosLogCreate(LIBOS_LOG_DECODE *logDecode, FILE *fout)
{
    portMemSet(logDecode, 0, sizeof *logDecode);
    logDecode->fout = fout;

    // Default name value: GSP
    portStringCopy(logDecode->sourceName, sizeof(logDecode->sourceName), "GSP", sizeof(logDecode->sourceName));
}
void libosLogCreateEx(LIBOS_LOG_DECODE *logDecode, const char *pSourceName, FILE *fout)
{
    portMemSet(logDecode, 0, sizeof *logDecode);
    logDecode->fout = fout;

    // Extended args - set name value
    portStringCopy(logDecode->sourceName, sizeof(logDecode->sourceName), pSourceName, sizeof(logDecode->sourceName));
}
#else
void libosLogCreate(LIBOS_LOG_DECODE *logDecode)
{
    portMemSet(logDecode, 0, sizeof *logDecode);

    // Default name value: GSP
    portStringCopy(logDecode->sourceName, sizeof(logDecode->sourceName), "GSP", sizeof(logDecode->sourceName));
}
void libosLogCreateEx(LIBOS_LOG_DECODE *logDecode, const char *pSourceName)
{
    portMemSet(logDecode, 0, sizeof *logDecode);

    // Extended args - set name value
    portStringCopy(logDecode->sourceName, sizeof(logDecode->sourceName), pSourceName, sizeof(logDecode->sourceName));
}
#endif

void libosLogAddLogEx(LIBOS_LOG_DECODE *logDecode, void *buffer, LwU64 bufferSize, LwU32 gpuInstance, LwU32 gpuArch, LwU32 gpuImpl, const char *name)
{

    if (logDecode->numLogBuffers >= LIBOS_LOG_MAX_LOGS)
    {
        printf("LIBOS_LOG_DECODE::log array is too small. Increase LIBOS_LOG_MAX_LOGS.\n");
        return;
    }

    LwU32 i                    = logDecode->numLogBuffers++;
    LIBOS_LOG_DECODE_LOG *pLog = &logDecode->log[i];
    pLog->physicLogBuffer      = (volatile LwU64 *)buffer;
    pLog->logBufferSize        = bufferSize;
    pLog->previousPut          = 0;
    pLog->putCopy              = 0;
    pLog->putIter              = 0;

    pLog->gpuInstance  = gpuInstance;

    if (name)
        portStringCopy(pLog->taskPrefix, sizeof(pLog->taskPrefix), name, sizeof(pLog->taskPrefix));


#if LIBOS_LOG_TO_LWLOG
    LW_STATUS status;

    const LwU32 libosNoWrapBufferFlags =
        DRF_DEF(LOG, _BUFFER_FLAGS, _DISABLED, _NO)      | DRF_DEF(LOG, _BUFFER_FLAGS, _TYPE, _NOWRAP)  |
        DRF_DEF(LOG, _BUFFER_FLAGS, _EXPANDABLE, _NO)    | DRF_DEF(LOG, _BUFFER_FLAGS, _NONPAGED, _YES) |
        DRF_DEF(LOG, _BUFFER_FLAGS, _LOCKING, _NONE)     | DRF_DEF(LOG, _BUFFER_FLAGS, _OCA, _YES)      |
        DRF_DEF(LOG, _BUFFER_FLAGS, _FORMAT, _LIBOS_LOG) |
        DRF_NUM(LOG, _BUFFER_FLAGS, _GPU_INSTANCE, gpuInstance);

    const LwU32 libosWrapBufferFlags =
        DRF_DEF(LOG, _BUFFER_FLAGS, _DISABLED, _NO)      | DRF_DEF(LOG, _BUFFER_FLAGS, _TYPE, _RING)    |
        DRF_DEF(LOG, _BUFFER_FLAGS, _EXPANDABLE, _NO)    | DRF_DEF(LOG, _BUFFER_FLAGS, _NONPAGED, _YES) |
        DRF_DEF(LOG, _BUFFER_FLAGS, _LOCKING, _NONE)     | DRF_DEF(LOG, _BUFFER_FLAGS, _OCA, _YES)      |
        DRF_DEF(LOG, _BUFFER_FLAGS, _FORMAT, _LIBOS_LOG) |
        DRF_NUM(LOG, _BUFFER_FLAGS, _GPU_INSTANCE, gpuInstance);

    pLog->hLwLogNoWrap = 0;
    pLog->hLwLogWrap   = 0;
    pLog->bLwLogNoWrap = LW_FALSE;

    LIBOS_LOG_LWLOG_BUFFER *pNoWrapBuf;

    status = lwlogAllocBuffer(
        bufferSize + LW_OFFSETOF(LIBOS_LOG_LWLOG_BUFFER, data), libosNoWrapBufferFlags,
        LIBOS_LOG_LWLOG_BUFFER_TAG(logDecode->sourceName, i * 2),
        &pLog->hLwLogNoWrap);

    if (status == LW_OK)
    {
        pNoWrapBuf = (LIBOS_LOG_LWLOG_BUFFER *)LwLogLogger.pBuffers[pLog->hLwLogNoWrap]->data;
        if (name)
        {
            portStringCopy(
                pNoWrapBuf->taskPrefix, sizeof pNoWrapBuf->taskPrefix, name, sizeof pNoWrapBuf->taskPrefix);
        }

        pNoWrapBuf->gpuArch = gpuArch;
        pNoWrapBuf->gpuImpl = gpuImpl;

        LwLogLogger.pBuffers[pLog->hLwLogNoWrap]->pos = sizeof(LwU64); // offset to account for put pointer
        pLog->bLwLogNoWrap                            = LW_TRUE;
    }
    else
    {
        printf("lwlogAllocBuffer nowrap failed\n");
    }

    LIBOS_LOG_LWLOG_BUFFER *pWrapBuf;

    status = lwlogAllocBuffer(
        bufferSize + LW_OFFSETOF(LIBOS_LOG_LWLOG_BUFFER, data), libosWrapBufferFlags,
        LIBOS_LOG_LWLOG_BUFFER_TAG(logDecode->sourceName, i * 2 + 1),
        &pLog->hLwLogWrap);

    if (status == LW_OK)
    {
        pWrapBuf = (LIBOS_LOG_LWLOG_BUFFER *)LwLogLogger.pBuffers[pLog->hLwLogWrap]->data;
        if (name)
        {
            portStringCopy(
                pWrapBuf->taskPrefix, sizeof pWrapBuf->taskPrefix, name, sizeof pWrapBuf->taskPrefix);
        }

        pWrapBuf->gpuArch = gpuArch;
        pWrapBuf->gpuImpl = gpuImpl;
    }
    else
    {
        printf("lwlogAllocBuffer wrap failed\n");
    }
#endif // LIBOS_LOG_TO_LWLOG
}

void libosLogAddLog(LIBOS_LOG_DECODE *logDecode, void *buffer, LwU64 bufferSize, LwU32 gpuInstance, const char *name)
{
    // Use defaults for gpuArch and gpuImpl
    libosLogAddLogEx(logDecode, buffer, bufferSize, gpuInstance, 0, 0, name);
}

#if LIBOS_LOG_DECODE_ENABLE

void libosLogInit(LIBOS_LOG_DECODE *logDecode, elf64_header *elf)
{
    LwU64 scratchBufferSize = 0;
    LwU64 i;

    //
    // The scratch buffer holds the sorted records in flight from all logs.
    // If we overflow it, we lose records.
    //

    //
    // First, callwlate the smallest possible length (in 64-bit words)
    // of a log buffer entry for the current log (0 args).
    // This will allow us to callwlate for max possible number of log entries,
    // i.e. if none of them have args and are thus the smallest size possible.
    //
    LwU64 minLogBufferEntryLength = 0;
    minLogBufferEntryLength++; // account for metadata pointer
    if (!logDecode->bSynchronousBuffer)
    {
        minLogBufferEntryLength++; // account for timestamp
    }

    for (i = 0; i < logDecode->numLogBuffers; i++)
    {
        scratchBufferSize += logDecode->log[i].logBufferSize;
    }

    // The scratch buffer is sized to handle worst-case overhead
    scratchBufferSize = (scratchBufferSize * LIBOS_LOG_DECODE_RECORD_BASE) / minLogBufferEntryLength;


    logDecode->elf               = elf;
    logDecode->scratchBuffer     = portMemAllocNonPaged(scratchBufferSize);
    logDecode->scratchBufferSize = scratchBufferSize;
    logDecode->lwrLineBufPtr     = logDecode->lineBuffer;

    libosDebugResolverConstruct(&logDecode->resolver, elf);
}

void libosLogInitEx(
    LIBOS_LOG_DECODE *logDecode, elf64_header *elf, LwBool bSynchronousBuffer, LwBool bPtrSymbolResolve)
{
    // Set extended config
    logDecode->bSynchronousBuffer = bSynchronousBuffer;
    logDecode->bPtrSymbolResolve = bPtrSymbolResolve;

    // Complete init
    libosLogInit(logDecode, elf);
}

#else // LIBOS_LOG_DECODE_ENABLE

void libosLogInit(LIBOS_LOG_DECODE *logDecode, void *elf) {}

void libosLogInitEx(
    LIBOS_LOG_DECODE *logDecode, void *elf,
    LwBool bSynchronousBuffer, LwBool bPtrSymbolResolve)
{
    // No extended config to set when decode is disabled
}

#endif // LIBOS_LOG_DECODE_ENABLE

void libosLogDestroy(LIBOS_LOG_DECODE *logDecode)
{
#if LIBOS_LOG_TO_LWLOG
    LwU64 i;
    for (i = 0; i < logDecode->numLogBuffers; i++)
    {
        LIBOS_LOG_DECODE_LOG *pLog = &logDecode->log[i];

        if (pLog->hLwLogNoWrap != 0)
        {
            lwlogDeallocBuffer(pLog->hLwLogNoWrap);
            pLog->hLwLogNoWrap = 0;
        }

        if (pLog->hLwLogWrap != 0)
        {
            lwlogDeallocBuffer(pLog->hLwLogWrap);
            pLog->hLwLogWrap = 0;
        }
    }
#endif // LIBOS_LOG_TO_LWLOG

#if LIBOS_LOG_DECODE_ENABLE
    libosDebugResolverDestroy(&logDecode->resolver);

    if (logDecode->scratchBuffer)
    {
        portMemFree(logDecode->scratchBuffer);
        logDecode->scratchBuffer = NULL;
    }
#endif // LIBOS_LOG_DECODE_ENABLE
}

void libosExtractLogs(LIBOS_LOG_DECODE *logDecode, LwBool bSyncLwLog)
{
#if LIBOS_LOG_DECODE_ENABLE
    if (logDecode->elf != NULL)
        libosExtractLogs_decode(logDecode);
#endif

#if LIBOS_LOG_TO_LWLOG
    libosExtractLogs_lwlog(logDecode, bSyncLwLog);
#endif
}
