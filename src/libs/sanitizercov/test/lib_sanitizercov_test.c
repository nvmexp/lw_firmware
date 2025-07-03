/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file lib_sanitizercov_test.c
 *
 * @brief Unit testing (in Linux userspace, independent of ucode/RTOS)
 *        apparatus for lib_sanitizercov.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "lib_sanitizercov.h"
#include "lib_sanitizercov_test.h"
#include "lib_sanitizercov_decode.h"

static LwU64 buf[SANITIZER_COV_BUF_NUM_ELEMENTS];

void assertStatus(LwU32 used, LwU32 missed, LwBool bEnabled)
{
    FLCN_STATUS           r;
    SANITIZER_COV_STATUS  s;
    r = sanitizerCovGetStatus(&s);
    assert(r == FLCN_OK);
    assert(s.used == used);
    assert(s.missed == missed);
    assert(s.bEnabled == bEnabled);
}

void setStatus(LwU32 used, LwU32 missed, LwBool bEnabled)
{
    FLCN_STATUS           r;
    SANITIZER_COV_STATUS  s;

    s.used = used;
    s.missed = missed;
    s.bEnabled = bEnabled;
    r = sanitizerCovSetStatus(&s);
    assert(r == FLCN_OK);
}

uint64_t assertFirstAndGetPC(uint64_t qword, uint8_t typ, uint8_t psz)
{
    uint8_t   dtyp;
    uint8_t   dpsz;
    uint64_t  dpc;
    bool      valid;

    valid = sanitizerCovDecodeFirst(qword, &dtyp, &dpsz, &dpc);
    assert(valid);
    assert(dtyp == typ);
    assert(dpsz == psz);
    return dpc;
}

int main(void)
{
    FLCN_STATUS  r;
    LwU32        nread;
    LwBool       done;
    uint64_t     pc;

    // init should result in disabled status
    sanitizerCovInit();
    assertStatus(0, 0, LW_FALSE);

    // callbacks should have no effect (because disabled)
    __sanitizer_cov_trace_pc();
    assertStatus(0, 0, LW_FALSE);

    // setting status with enable should enable
    setStatus(1, 1, LW_TRUE);
    assertStatus(0, 0, LW_TRUE);

    // callbacks should now have an effect (because enabled)
    __sanitizer_cov_trace_pc();
    assertStatus(1, 0, LW_TRUE);

    // we had one trace-pc callback, should have 1 element to read
    nread = 1;
    r = sanitizerCovCopyData(buf, &nread, &done);
    assert(r == FLCN_OK);
    assert(done);
    assert(nread == 1);
    assertStatus(0, 0, LW_TRUE);

    // what we read should be valid
    pc = assertFirstAndGetPC(buf[0], SANITIZER_COV_TYP_TRACE_PC, 0);

    // filling buffer should result in all used
    for (int i = 0; i < SANITIZER_COV_BUF_NUM_ELEMENTS; i++)
    {
        __sanitizer_cov_trace_pc();
    }
    assertStatus(SANITIZER_COV_BUF_NUM_ELEMENTS, 0, LW_TRUE);

    // setting status with used = 0 should clear buffer
    setStatus(0, 1, LW_TRUE);
    assertStatus(0, 0, LW_TRUE);

    // over fill buffer (each cmp2 callback takes two elements)
    // should result in full buffer and half missed
    for (uint16_t i = 0; i < SANITIZER_COV_BUF_NUM_ELEMENTS; i++) {
        __sanitizer_cov_trace_cmp2(i, i * 2);
    }
    assertStatus(SANITIZER_COV_BUF_NUM_ELEMENTS, SANITIZER_COV_BUF_NUM_ELEMENTS / 2, LW_TRUE);

    // try reading half the buffer, should not be done yet
    nread = SANITIZER_COV_BUF_NUM_ELEMENTS / 2;
    r = sanitizerCovCopyData(buf, &nread, &done);
    assert(r == FLCN_OK);
    assert(!done);
    assert(nread == SANITIZER_COV_BUF_NUM_ELEMENTS / 2);

    // try reading the other half of the buffer, should be done
    // buffer should also not have any used (should be empty)
    nread = SANITIZER_COV_BUF_NUM_ELEMENTS;
    r = sanitizerCovCopyData(&buf[SANITIZER_COV_BUF_NUM_ELEMENTS / 2], &nread, &done);
    assert(r == FLCN_OK);
    assert(done);
    assert(nread == SANITIZER_COV_BUF_NUM_ELEMENTS / 2);
    assertStatus(0, SANITIZER_COV_BUF_NUM_ELEMENTS / 2, LW_TRUE);

    // verify what we read is correct
    for (uint16_t i = 0; i < SANITIZER_COV_BUF_NUM_ELEMENTS; i += 2) {
        pc = assertFirstAndGetPC(buf[i], SANITIZER_COV_TYP_TRACE_CMP, 1);
        uint64_t arg1, arg2;
        bool valid = sanitizerCovDecodeCmpParams(buf[i + 1], 1, &arg1, &arg2);
        assert(valid);
        assert(arg1 == i / 2);
        assert(arg2 == i);
    }

    // each cmp8 callback takes 3 elements
    __sanitizer_cov_trace_cmp8(12345, 67890);
    assertStatus(3, SANITIZER_COV_BUF_NUM_ELEMENTS / 2, LW_TRUE);

    // setting status with missed = 0 should reset missed counter
    setStatus(1, 0, LW_TRUE);
    assertStatus(3, 0, LW_TRUE);
}