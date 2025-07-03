/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LOGDECODE_H_
#define LOGDECODE_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef LWRM

#    if RMCFG_FEATURE_ENABLED(GSP_LOG_DECODE) || (!RMCFG_FEATURE_ENABLED(RELEASE_BUILD) && !RMCFG_FEATURE_ENABLED(PLATFORM_GSP))
#        define LIBOS_LOG_DECODE_ENABLE 1
#    else
#        define LIBOS_LOG_DECODE_ENABLE 0
#    endif

#    if LWLOG_ENABLED
#        define LIBOS_LOG_TO_LWLOG 1
#    else
#        define LIBOS_LOG_TO_LWLOG 0
#    endif

#    define LIBOS_LOG_MAX_LOGS      5   // Max logs per GPU

#else // LWRM
#    include <stdio.h>

#    define LIBOS_LOG_DECODE_ENABLE 1
#    define LIBOS_LOG_TO_LWLOG      0

#    define LIBOS_LOG_MAX_LOGS    160   // Max logs for all GPUs for offline decoder

#endif // LWRM

#define LIBOS_LOG_ENABLE          (LIBOS_LOG_TO_LWLOG || LIBOS_LOG_DECODE_ENABLE)

#if LIBOS_LOG_DECODE_ENABLE
#    include "../include/libos_log.h"
#    include "lines.h"
#endif

// Forward declarations.
struct LIBOS_LOG_DECODE_LOG;
typedef struct LIBOS_LOG_DECODE_LOG LIBOS_LOG_DECODE_LOG;

#define LIBOS_LOG_LINE_BUFFER_SIZE 128
#define LIBOS_LOG_MAX_ARGS         20

#if LIBOS_LOG_DECODE_ENABLE

#    include "lwctassert.h"

typedef struct
{
    LW_DECLARE_ALIGNED(LIBOS_LOG_DECODE_LOG *log, 8);
    LW_DECLARE_ALIGNED(libosLogMetadata *meta, 8);
    LW_DECLARE_ALIGNED(LwU64 timeStamp, 8);
    LwU64 args[LIBOS_LOG_MAX_ARGS];
} LIBOS_LOG_DECODE_RECORD;

// Size of LIBOS_LOG_DECODE_RECORD without args, in number of LwU64 entries.
#    define LIBOS_LOG_DECODE_RECORD_BASE 3

// Ensure that the size matches up (no padding in the struct)
ct_assert((LIBOS_LOG_DECODE_RECORD_BASE * sizeof(LwU64)) == (sizeof(LIBOS_LOG_DECODE_RECORD) - sizeof(((LIBOS_LOG_DECODE_RECORD*)NULL)->args)));

#endif // LIBOS_LOG_DECODE_ENABLE

#define TASK_NAME_MAX_LENGTH   (8)
#define SOURCE_NAME_MAX_LENGTH (4)

// LwLog buffer
typedef struct
{
    LwU32 gpuArch;
    LwU32 gpuImpl;
    LwU32 rsvd1;
    LwU32 rsvd2;
    char taskPrefix[TASK_NAME_MAX_LENGTH]; // Prefix string printed before each line.
    LwU8 data[0];
} LIBOS_LOG_LWLOG_BUFFER;

struct LIBOS_LOG_DECODE_LOG
{
    volatile LwU64 *physicLogBuffer;
    LwU64 logBufferSize;             // Includes put pointer located in first 8 bytes.
    LwU64 previousPut;               // Keeps track of records already printed.
    LwU64 putCopy;                   // End pointer for this batch.
    LwU64 putIter;                   // Iterator for this batch.
    LwU32 gpuInstance;               // GPU that this log is associated with.
    char taskPrefix[TASK_NAME_MAX_LENGTH]; // Prefix string printed before each line.

#if LIBOS_LOG_TO_LWLOG
    LwU32 hLwLogNoWrap;  // No wrap buffer captures first records.
    LwU32 hLwLogWrap;    // Wrap buffer captures last records.
    LwBool bLwLogNoWrap; // LW_TRUE if no wrap buffer not full.
#endif

#if LIBOS_LOG_DECODE_ENABLE
    LIBOS_LOG_DECODE_RECORD record;
#endif
};

typedef struct
{
    char sourceName[SOURCE_NAME_MAX_LENGTH]; // GSP, PMU etc

    LwU64 numLogBuffers;
    LIBOS_LOG_DECODE_LOG log[LIBOS_LOG_MAX_LOGS];

#if LIBOS_LOG_DECODE_ENABLE
    elf64_header *elf;
    libosDebugResolver resolver;
    LwU64 *scratchBuffer;    // Sorted by timestamp.
    LwU64 scratchBufferSize; // Sum of logBufferSize.
    char *lwrLineBufPtr;     // Current position in lineBuffer.
    // Decodes into lineBuffer, then prints as a string.
    char lineBuffer[LIBOS_LOG_LINE_BUFFER_SIZE];
    LwBool bSynchronousBuffer;
    LwBool bPtrSymbolResolve;
#endif // LIBOS_LOG_DECODE_ENABLE

#ifndef LWRM
    FILE *fout;
#endif

} LIBOS_LOG_DECODE;

#ifdef LWSYM_STANDALONE
void libosLogCreate(LIBOS_LOG_DECODE *logDecode, FILE *fout);
void libosLogCreateEx(LIBOS_LOG_DECODE *logDecode, const char *pSourceName, FILE *fout);
#else
void libosLogCreate(LIBOS_LOG_DECODE *logDecode);
void libosLogCreateEx(LIBOS_LOG_DECODE *logDecode, const char *pSourceName);
#endif

void libosLogAddLogEx(LIBOS_LOG_DECODE *logDecode, void *buffer, LwU64 bufferSize, LwU32 gpuInstance, LwU32 gpuArch, LwU32 gpuImpl, const char *name);
void libosLogAddLog(LIBOS_LOG_DECODE *logDecode, void *buffer, LwU64 bufferSize, LwU32 gpuInstance, const char *name);

#if LIBOS_LOG_DECODE_ENABLE
void libosLogInit(LIBOS_LOG_DECODE *logDecode, elf64_header *elf);
void libosLogInitEx(
    LIBOS_LOG_DECODE *logDecode, elf64_header *elf, LwBool bSynchronousBuffer, LwBool bPtrSymbolResolve);
#else
void libosLogInit(LIBOS_LOG_DECODE *logDecode, void *elf);
void libosLogInitEx(
    LIBOS_LOG_DECODE *logDecode, void *elf, LwBool bSynchronousBuffer, LwBool bPtrSymbolResolve);
#endif // LIBOS_LOG_DECODE_ENABLE

void libosLogDestroy(LIBOS_LOG_DECODE *logDecode);

void libosExtractLogs(LIBOS_LOG_DECODE *logDecode, LwBool bSyncLwLog);

#ifdef __cplusplus
}
#endif

#endif // LOGDECODE_H_
