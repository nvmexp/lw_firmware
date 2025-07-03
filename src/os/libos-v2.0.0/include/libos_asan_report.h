/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#include <sdk/lwpu/inc/lwtypes.h>
#include "libos.h"
#include "libos_asan.h"

/**
 * @file
 * @brief Helpers for ASAN error reporting
 * 
 * For example, if an ASAN-enabled task is set to panic and trap to TASK_INIT
 * on encountering an ASAN error, the trap handler in TASK_INIT may use the
 * functions and macros defined here to log the details of the error (type,
 * shadow addresses, etc).
 */

/**
 * @brief Determines memory bug type from corresponding shadow memory in remote task
 *
 * @param[in] remote
 *     Remote task to attempt to access shadow memory from
 * @param[in] badAddr
 *     Memory address in remote task to access corresponding shadow memory for
 *     (not the address of the shadow memory itself)
 *
 * @returns A short string describing the memory bug type
 */
const char *libosAsanReportGetShortDesc(LibosTaskId remote, LwUPtr badAddr);

/**
 * @brief Returns either the string "read" or "write" depending on bWrite
 *
 * @param[in] bWrite
 *     Whether the memory access being reported upon is a read/load (LW_FALSE)
 *     or a write/store (LW_TRUE).
 *
 * @returns The string "read" or the string "write"
 */
const char *libosAsanReportGetReadWriteDesc(LwBool bWrite);

/**
 * @brief Temporary buffer type used by ASAN report logging macros
 */
typedef struct {
    LwU8 s[16];
} LibosAsanReportBuffer;

/**
 * @brief Logs ASAN error summary
 *
 * @param[in] logFn
 *     Logging function/macro to use (e.g. LOG_INIT)
 * @param[in] logLevel
 *     Logging level to use (e.g. LOG_LEVEL_INFO)
 * @param[in] remote
 *     Remote task to attempt to access shadow memory from
 * @param[in] addr
 *     Memory address in remote task of the erroneous memory access
 * @param[in] size
 *     Size of the erroneous memory access
 * @param[in] bWrite
 *     Whether memory access was a write (LW_TRUE) or a read (LW_FALSE)
 * @param[in] pc
 *     PC of the erroneous memory access
 * @param[in] badAddr
 *     Memory address in remote task to access corresponding shadow memory for
 *     (not the address of the shadow memory itself)
 */
#define LIBOS_ASAN_REPORT_LOG_SUMMARY(logFn, logLevel, remote, addr, size, bWrite, pc, badAddr) \
    do { \
        logFn(logLevel, "ASAN: ERROR: %s %s of size %lu at address %p by pc %p\n", \
              libosAsanReportGetShortDesc((remote), (badAddr)), \
              libosAsanReportGetReadWriteDesc((bWrite)), (size), (addr), (pc)); \
    } while (0)

/**
 * @brief Logs corresponding shadow bytes arround given address in remote
 *
 * @param[in] logFn
 *     Logging function/macro to use (e.g. LOG_INIT)
 * @param[in] logLevel
 *     Logging level to use (e.g. LOG_LEVEL_INFO)
 * @param[in] remote
 *     Remote task to attempt to access shadow memory from
 * @param[in] pBuf
 *     Pointer to an LibosAsanReportBuffer to use
 * @param[in] badAddr
 *     Memory address in remote task to access corresponding shadow memory for
 *     (not the address of the shadow memory itself)
 */
#define LIBOS_ASAN_REPORT_LOG_SHADOW(logFn, logLevel, remote, pBuf, badAddr) \
    do { \
        if (libosTaskMemoryRead(&((pBuf)->s), (remote), (LwUPtr) LIBOS_ASAN_MEM_TO_SHADOW((badAddr)) - 7, 16) == LIBOS_OK) { \
            logFn(logLevel, "Shadow bytes around the buggy address %p:\n", (badAddr)); \
            logFn(logLevel, "%p: %02x %02x %02x %02x %02x %02x %02x [%02x] " \
                  "%02x %02x %02x %02x %02x %02x %02x %02x\n", \
                  (LwUPtr) LIBOS_ASAN_MEM_TO_SHADOW((badAddr)) - 7, \
                  (pBuf)->s[0], (pBuf)->s[1], (pBuf)->s[2], (pBuf)->s[3], (pBuf)->s[4], (pBuf)->s[5], (pBuf)->s[6], (pBuf)->s[7], \
                  (pBuf)->s[8], (pBuf)->s[9], (pBuf)->s[10], (pBuf)->s[11], (pBuf)->s[12], (pBuf)->s[13], (pBuf)->s[14], (pBuf)->s[15]); \
        } else { \
            logFn(logLevel, "Shadow bytes around the buggy address %p: (could not read shadow memory around %p)\n", \
                  (badAddr), LIBOS_ASAN_MEM_TO_SHADOW((badAddr))); \
        } \
    } while (0)

