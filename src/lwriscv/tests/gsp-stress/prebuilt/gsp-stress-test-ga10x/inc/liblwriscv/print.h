/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBNRISCV_PRINT_H
#define LIBNRISCV_PRINT_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

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

#define LWRISCV_DEBUG_PRINT_ENABLED (LWRISCV_CONFIG_DEBUG_PRINT_LEVEL > LEVEL_NONE)

#if LWRISCV_DEBUG_PRINT_ENABLED
int putchar(int c);
int puts(const char *pStr);
void putHex(int count, unsigned long value);
int printf(const char *pFmt, ...);
#else
static inline int putchar(int c) { return 0; }
static inline int puts(const char *pStr) { return 0; }
static inline void putHex(int count, unsigned long value) { }
static inline int printf(const char *pFmt, ...) { return 0; }
#endif

#define dbgPutchar(level, ch) do { if ((level) <= LWRISCV_CONFIG_DEBUG_PRINT_LEVEL) putchar(ch); } while(0)
#define dbgPuts(level, str) do { if ((level) <= LWRISCV_CONFIG_DEBUG_PRINT_LEVEL) puts(str); } while (0)
#define dbgPutHex(level, count, value) do { if ((level) <= LWRISCV_CONFIG_DEBUG_PRINT_LEVEL) putHex(count, value); } while (0)
#define dbgPrintf(level, fmt, ...) do { if ((level) <= LWRISCV_CONFIG_DEBUG_PRINT_LEVEL) printf((fmt), ##__VA_ARGS__); } while (0)

// String wrapper for non-standard print functions, string arrays etc.
#define dbgStr(level, str) ((level) <= LWRISCV_CONFIG_DEBUG_PRINT_LEVEL ? str : "")

#if LWRISCV_DEBUG_PRINT_ENABLED
bool printInit(void *pBuffer, uint16_t bufferSize, uint8_t queueNo, uint8_t swgenNo);
bool printInitEx(void *pBuffer, uint16_t bufferSize, uint32_t queueHeadPri, uint32_t queueTailPri, uint8_t swgenNo);
#else
static inline bool printInit(void *pBuffer, uint16_t bufferSize, uint8_t queueNo, uint8_t swgenNo) { return true; }
static inline bool printInitEx(void *pBuffer, uint16_t bufferSize, uint32_t queueHeadPri, uint32_t queueTailPri, uint8_t swgenNo) { return true; }
#endif // LWRISCV_DEBUG_PRINT_ENABLED

#endif // LIBLWRISCV_PRINT_H
