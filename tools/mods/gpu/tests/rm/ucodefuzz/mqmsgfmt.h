/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCODEFUZZ_MQMSGFMT_H
#define UCODEFUZZ_MQMSGFMT_H

//!
//! \file mqmsgfmt.h
//! \brief Message queue (MQ) message formats used for the ucode fuzzer
//!
//! Message formats used between the libFuzzer and MODS components of the
//! ucode fuzzer (for use with POSIX message queues).
//!
//! Note: this file is duplicated between the two components and the two
//! copies must agree.
//!
//! libFuzzer side:  <UCODE_FUZZER_REPO>/src/mqmsgfmt.h
//! MODS side:  //sw/<branch>/diag/mods/gpu/tests/rm/ucodefuzz/mqmsgfmt.h
//!

#include <stdint.h>

//
// Queue names and lengths (length being number of messages)
//

// libfuzzer -> MODS queue for test cases (one per MODS instance)
#define UCODEFUZZ_FUZZ_QUEUE_PREFIX    "/ucodefuzz_fuzz_queue_"
#define UCODEFUZZ_FUZZ_QUEUE_LENGTH    16

// MODS -> libfuzzer queue for initialization and results (only one)
#define UCODEFUZZ_SERVER_QUEUE_NAME    "/ucodefuzz_server_listen_queue"
#define UCODEFUZZ_SERVER_QUEUE_LENGTH  16

//
// Message types (the tag used for the tagged union UcodeFuzzMsg)
//

// libfuzzer -> MODS message with test case data for ucode
#define UCODEFUZZ_MSG_TESTCASE_DATA     1
// libfuzzer -> MODS message with fuzz data for ucode (e.g. RPC params)
#define UCODEFUZZ_MSG_FUZZ_DATA         2
// libfuzzer -> MODS message to stop fuzzing
#define UCODEFUZZ_MSG_STOP              3

// MODS -> libfuzzer message with initialization parameters for selected ucode
#define UCODEFUZZ_MSG_PARAMS_FROM_MODS  4
// MODS -> libfuzzer message with result of test case
#define UCODEFUZZ_MSG_TESTCASE_RESULT   5

// placeholder message/coverage type for indicating no coverage
#define UCODEFUZZ_MSG_COV_NONE               100
// MODS -> libfuzzer message with SanitizerCoverage data
#define UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE 101
// MODS -> libfuzzer message with BullseyeCoverage data
#define UCODEFUZZ_MSG_COV_BULLSEYE_COVERAGE  102

// typedef for message types
typedef uint32_t UcodeFuzzMsgType;
// use the MSG_COV_* defines for a "coverage type" typedef as well
typedef UcodeFuzzMsgType UcodeFuzzCovType;

// message body for UCODEFUZZ_MSG_TESTCASE_DATA
typedef struct
{
    uint32_t  fuzzDataMsgCount;    // number of FUZZ_DATA messages to follow
    uint32_t  _pad;                // padding so data is 8-byte aligned
} UcodeFuzzMsgBodyTestcaseData;

// message body for UCODEFUZZ_MSG_FUZZ_DATA
typedef struct
{
    uint32_t  size;        // number of bytes of test case data contained
    uint32_t  _pad;        // padding so data is 8-byte aligned
    uint8_t   data[4080];  // test case data (raw bytes)
} UcodeFuzzMsgBodyFuzzData;

// message body for UCODEFUZZ_MSG_PARAMS_FROM_MODS
typedef struct
{
    uint32_t  maxLen;   // maximum allowable size of fuzz data
    int32_t   modsPid;  // PID of MODS process
} UcodeFuzzMsgBodyParamsFromMods;

// message body for UCODEFUZZ_MSG_TESTCASE_RESULT
typedef struct
{
    uint32_t          rc;           // MODS RC from this fuzzer run
    uint32_t          covMsgCount;  // number of COV_* messages to follow
    UcodeFuzzCovType  covType;      // type of COV_* messages to follow
} UcodeFuzzMsgBodyTestcaseResult;

// message body for UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE
typedef struct
{
    uint32_t  numElements;  // number of elements of data contained
    uint32_t  _pad;         // padding so data is 8-byte aligned
    uint64_t  data[510];    // SanitizerCoverage data elements
} UcodeFuzzMsgBodyCovSanitizerCoverage;

// message body for UCODEFUZZ_MSG_COV_BULLSEYE_COVERAGE
typedef struct
{
    uint32_t  numElements;   // number of elements of data contained
    uint32_t  _pad;          // padding so data is 8-byte aligned
    uint8_t   data[4080];    // SanitizerCoverage data elements
} UcodeFuzzMsgBodyCovBullseyeCoverage;

//
// Tagged union of all posible messages and message bodies
//
typedef struct
{
    UcodeFuzzMsgType  msgType;  // message type (tag)
    uint32_t          _pad;     // padding so bodies are 8-byte aligned
    union {
        UcodeFuzzMsgBodyTestcaseData          testcaseData;
        UcodeFuzzMsgBodyFuzzData              fuzzData;
        UcodeFuzzMsgBodyParamsFromMods        paramsFromMods;
        UcodeFuzzMsgBodyTestcaseResult        testcaseResult;
        UcodeFuzzMsgBodyCovSanitizerCoverage  covSanitizerCoverage;
        UcodeFuzzMsgBodyCovBullseyeCoverage   covBullseyeCoverage;
    };
} UcodeFuzzMsg;

// number of bytes for msgType and _pad (i.e. the header) in UcodeFuzzMsg
#define UCODEFUZZ_MSG_HEADER_LEN 8

// ensure UcodeFuzzMsg fits on one page
#if defined(ct_assert)
ct_assert(sizeof(UcodeFuzzMsg) <= 4096);
#elif defined(__cplusplus)
static_assert(sizeof(UcodeFuzzMsg) <= 4096);
#else
_Static_assert(sizeof(UcodeFuzzMsg) <= 4096, "sizeof(UcodeFuzzMsg) exceeds 4096!");
#endif

#endif  // UCODEFUZZ_MQMSGFMT_H
