/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_PRINT_H
#define LWRISCV_PRINT_H

#include <stdarg.h>
#include <riscvifriscv.h>

// Logging levels for ucode

// Level not to be used (used to check for disabled prints)
#define LEVEL_NONE     0

// Levels below are printed if level passed is <= level from configuration

//
// Intened for messages that should be always printed. That may include
// bootup/shutdown (for every stage), fatal crash etc.
//
#define LEVEL_ALWAYS   1
// Critical (unrecoverable errors)
#define LEVEL_CRIT     3
// Ordinary errors, extra verbosity / information vs crit
#define LEVEL_ERROR    5
// Logs that may help understanding what's going on (but doesn't happen too often)
#define LEVEL_INFO     7
// Information useful for debugging
#define LEVEL_DEBUG   10
// Most verbose messages should be here (for example incoming RPC's)
#define LEVEL_TRACE   15

#ifndef LWRISCV_DEBUG_PRINT_LEVEL
# error LWRISCV_DEBUG_PRINT_LEVEL must be defined to 1 or 0 if lwriscv/print.h is included!
#endif // LWRISCV_DEBUG_PRINT_LEVEL

#ifndef LWRISCV_PRINT_RAW_MODE
# error LWRISCV_PRINT_RAW_MODE must be defined to 1 or 0 if lwriscv/print.h is included!
#endif // LWRISCV_PRINT_RAW_MODE

#define LWRISCV_DEBUG_PRINT_ENABLED (LWRISCV_DEBUG_PRINT_LEVEL > LEVEL_NONE)

#if LWRISCV_DEBUG_PRINT_ENABLED

# if LWRISCV_PRINT_RAW_MODE
// MMINTS-TODO: add libos-v2.0.0 include paths to lwriscv libs and lwriscv clients once we unify
#  include <../../../os/libos-v2.0.0/include/libos_log.h>
// #include this here since bootloader doesn't have shlib and can't support raw-mode anyway
#  include <shlib/string.h>

#  define LWRISCV_LOG_INTERNAL(_dispatcher, _level, ...)                                     \
    do                                                                                       \
    {                                                                                        \
        /* MMINTS-TODO: temporary code, change this to direct call to LIBOS_LOG_INTERNAL */  \
        /* when that is updated to do the gccFakePrintf check */                             \
        LIBOS_LOG_INTERNAL(_dispatcher, _level, __VA_ARGS__);                                \
        if (LW_FALSE)                                                                        \
        {                                                                                    \
            /* Compile-time assert that args are sane */                                     \
            gccFakePrintf(__VA_ARGS__);                                                      \
        }                                                                                    \
    } while (LW_FALSE)

void printRawModeDispatch(LwU32 numTokens, const LwUPtr *tokens);

#  define LWRISCV_SECTION_LOGGING LWRISCV_SECTION(".logging")

/* _level should match LEVEL_ERROR in RM. MMINTS-TODO: unify */
#  define PRINT_DYN_LEVEL_ERROR   (4)

#  define printf(_format, ...)    LWRISCV_LOG_INTERNAL(printRawModeDispatch, PRINT_DYN_LEVEL_ERROR, ""_format, ##__VA_ARGS__)
#  define puts(_str)              LWRISCV_LOG_INTERNAL(printRawModeDispatch, PRINT_DYN_LEVEL_ERROR, "%s\n", (const char*)(_str))
#  define putchar(_c)             LWRISCV_LOG_INTERNAL(printRawModeDispatch, PRINT_DYN_LEVEL_ERROR, "%c", (_c))

//! No-op: for raw-mode prints it exists to trick GCC into checking the types of our args for compatibility with fmt
void gccFakePrintf(const char *pFmt, ...)
    __attribute__((format(printf, 1, 2)));

# else // LWRISCV_PRINT_RAW_MODE

int putchar(int c);
int puts(const char *pStr);
void putHex(int count, unsigned long value); // TODO: implement putHex for non-BL (main) code or stub it out in main code?
int printf(const char *pFmt, ...)
    __attribute__((format(printf, 1, 2)));

# endif // LWRISCV_PRINT_RAW_MODE

#else // LWRISCV_DEBUG_PRINT_ENABLED
static inline int putchar(int c) { (void)c; return 0; }
static inline int puts(const char *pStr) { (void)pStr; return 0; }
static inline void putHex(int count, unsigned long value) { (void)count; (void)value; }
static inline __attribute__((format(printf, 1, 2))) int printf(const char *pFmt, ...) { (void)pFmt; return 0; }
#endif // LWRISCV_DEBUG_PRINT_ENABLED

#define dbgPutchar(level, ch) do { if ((level) <= LWRISCV_DEBUG_PRINT_LEVEL) putchar(ch); } while(0)
#define dbgPuts(level, str) do { if ((level) <= LWRISCV_DEBUG_PRINT_LEVEL) puts(str); } while (0)
#define dbgPutHex(level, count, value) do { if ((level) <= LWRISCV_DEBUG_PRINT_LEVEL) putHex(count, value); } while (0)
#define dbgPrintf(level, fmt, ...) do { if ((level) <= LWRISCV_DEBUG_PRINT_LEVEL) printf(""fmt, ##__VA_ARGS__); } while (0)

// String wrapper for non-standard print functions, string arrays etc.
#define dbgStr(level, str) ((level) <= LWRISCV_DEBUG_PRINT_LEVEL ? str : "")

// Don't use those functions, they're for internal use only.
void dbgPutcharUser(int c);
void dbgPutsUser(const char *pStr);
void dbgPrintfUser(const char *pFmt, va_list ap);
void dbgPutcharKernel(int c);
void dbgPutsKernel(const char *pStr);
void dbgPrintfKernel(const char *pFmt, va_list ap);
void dbgPutsKernel(const char *pStr);
void dbgDispatchRawDataKernel(LwU32 numTokens, const LwUPtr *tokens);

#endif // LWRISCV_PRINT_H
