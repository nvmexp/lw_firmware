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
    TestId_KMEM_Config,
    TestId_KMEM_DUMP,
    TestId_KDF,
    TestId_DICE,
    TestId_KMEM_PROT,
    TestIdCount,
} TestId;

typedef uint64_t (*TestCallback)(void);

#endif // TESTS_H
