/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef TESTS_H
#define TESTS_H

#include <stdint.h>
#include <stdbool.h>
#include <lwmisc.h>
#include <lwriscv/csr.h>

#define MAKE_DEBUG_FULL(TEST_ID, SUBTEST, IDX, EXPECTVAL, GOTVAL) (\
  ((uint64_t)(TEST_ID & 0xFF) << 56) | \
  ((uint64_t)((SUBTEST) & 0xFF) << 48) | \
  ((uint64_t)((IDX) & 0xFF) << 40) | \
  ((uint64_t)((EXPECTVAL) & 0xFFFF) << 16) | \
  ((uint64_t)((GOTVAL) & 0xFFFF)) \
)

#define MAKE_DEBUG_NOVAL(TEST_ID, SUBTEST, IDX) MAKE_DEBUG_FULL(TEST_ID, SUBTEST, IDX, 0, 0)
#define MAKE_DEBUG_NOSUB(TEST_ID, IDX, EXPECTVAL, GOTVAL) MAKE_DEBUG_FULL(TEST_ID, 0, IDX, EXPECTVAL, GOTVAL)
#define MAKE_DEBUG(TEST_ID, IDX) MAKE_DEBUG_FULL(TEST_ID, 0, IDX, 0, 0)

#define PASSING_DEBUG_VALUE 0xCAFECAFECAFECAFEULL

typedef enum {
    TestId_Generic=1,
    TestId_Msgq,
    TestId_LWDEC,
    TestIdCount,
} TestId;

typedef uint64_t (*TestCallback)(void);

#define MANIFEST_AD_TEST_MAILBOX0 0x42424242
#define MANIFEST_AD_TEST_MAILBOX1 0x43434343

uint64_t test_msgq(void);
uint64_t test_lwdec(void);

#endif // TESTS_H
