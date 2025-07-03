/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SANITIZERCOV_COMMON_H
#define LIB_SANITIZERCOV_COMMON_H

/*
 * lib_sanitizercov_common.h
 *
 * Encodings used by the lib_sanitizercov uproc library to encode
 * SanitizerCoverage run-time coverage collection data for the ucode fuzzer.
 *
 * Note: this file is duplicated between the ucode fuzzer (libFuzzer component)
 * and the lib_sanitizercov uproc library; the two copies must agree.
 *
 * ucode fuzzer (libFuzzer) side:  <UCODE_FUZZER_REPO>/src/lib_sanitizercov_common.h
 * lib_sanitizercov side:  //sw/<branch>/uproc/libs/sanitizercov/inc/lib_sanitizercov_common.h
 */

// magic number used to identify the start of a DMEM buffer entry, see DMEM buffer format details at
// https://confluence.lwpu.com/display/GPUSysSwSec/SanitizerCoverage+DMEM+buffer+format
#define SANITIZER_COV_MAGIC 0x279

// callback types
#define SANITIZER_COV_TYP_TRACE_PC 1
#define SANITIZER_COV_TYP_TRACE_CMP 2
#define SANITIZER_COV_TYP_TRACE_CONST_CMP 3
#define SANITIZER_COV_TYP_TRACE_SWITCH 4

#endif  // LIB_SANITIZERCOV_COMMON_H