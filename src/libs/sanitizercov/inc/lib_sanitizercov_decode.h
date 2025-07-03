/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SANITIZERCOV_DECODE_H
#define LIB_SANITIZERCOV_DECODE_H

/*
 * lib_sanitizercov_decode.h
 *
 * Decode utilities for the encodings used by the lib_sanitizercov uproc
 * library to encode SanitizerCoverage run-time coverage collection data for
 * the ucode fuzzer.
 *
 * Note: this file is duplicated between the ucode fuzzer (libFuzzer component)
 * and the lib_sanitizercov uproc library; the two copies must agree.
 *
 * ucode fuzzer (libFuzzer) side:  <UCODE_FUZZER_REPO>/src/lib_sanitizercov_decode.h
 * lib_sanitizercov side:  //sw/<branch>/uproc/libs/sanitizercov/inc/lib_sanitizercov_decode.h
 */

#include <stdbool.h>
#include <stdint.h>

#include "lib_sanitizercov_common.h"

// returns true if valid (i.e. magic is correct for first qword)
static inline bool sanitizerCovDecodeFirst(uint64_t qword, uint8_t *typ, uint8_t *psz, uint64_t *pc) {
    uint64_t magic = qword >> 54;
    if (magic != SANITIZER_COV_MAGIC) {
        return false;
    }
    *typ = (qword >> 50) & 0xf;
    *psz = (qword >> 48) & 0x3;
    *pc = (qword & 0xffffffffffff);
    return true;
}

// returns true if valid (i.e. zero padding is correct for size of parameter)
static inline bool sanitizerCovDecodeCmpParams(uint64_t qword, uint8_t psz, uint64_t *arg1, uint64_t *arg2) {
    switch (psz) {
        case 0:
            *arg1 = (qword & 0xff);
            *arg2 = (qword >> 8);
            return !(qword >> 16);
        case 1:
            *arg1 = (qword & 0xffff);
            *arg2 = (qword >> 16);
            return !(qword >> 32);
        case 2:
            *arg1 = (qword & 0xffffffff);
            *arg2 = (qword >> 32);
            return true;
        case 3:
            *arg1 = qword;
            return true;
        default:
            return false;
    }
}

#endif  // LIB_SANITIZERCOV_DECODE_H