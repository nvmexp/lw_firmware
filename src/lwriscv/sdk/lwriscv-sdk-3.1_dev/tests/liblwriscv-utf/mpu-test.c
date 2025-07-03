/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <stdbool.h>
#include <test-macros.h>
#include <regmock.h>
#include <ut/ut.h>
#include <ut/ut_case.h>
#include <ut/ut_assert.h>
#include "status-shim.h"
#include LWRISCV64_MANUAL_ADDRESS_MAP

// Get the MPU functions either from liblwriscv or libfsp.
// they are named the same in both places, but have different return types,
// hence this if/else. Once we drop support for liblwriscv, remove this.
#if defined(UTF_USE_LIBLWRISCV)
    #include "lwmisc.h"                // Non-safety DRF macros
    #include "liblwriscv/mpu.h"
#elif defined(UTF_USE_LIBFSP)
    #include <misc/lwmisc_drf.h>       // safety macros, needed for libfsp
    #include <cpu/riscv-mpu.h>
#else
    #error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined
#endif

// one MMU page
#define DMEM_BUF_SIZE  0x400
#define DMEM_BUF_ALIGN 0x400

static uint32_t*  dmem_buf;
static mpu_context_t mpu_ctx;
static mpu_context_t pre_test_mpu_ctx;

static void *mpu_test_setup(void)
{
    RET_TYPE ret;

    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    ret = mpu_init(&mpu_ctx);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    pre_test_mpu_ctx = mpu_ctx;

    dmem_buf = (uint32_t *) utfMallocAligned(DMEM_BUF_SIZE, DMEM_BUF_ALIGN);
    UT_ASSERT_NOT_NULL(dmem_buf);

    for (int i = 0; i < DMEM_BUF_SIZE/sizeof(uint32_t); i++)
    {
        dmem_buf[i] = i;
    }

    return NULL;
}

static void *mpu_test_teardown(void)
{
    RET_TYPE ret;
    mpu_handle_t handle;
    uint32_t i;

    // Restore the pre-test state - Disable all MPU entries not in pre_test_mpu_ctx
    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    for (i = 0; i < pre_test_mpu_ctx.mpu_entry_count; i++)
    {
        ret = mpu_reserve_entry(&pre_test_mpu_ctx, i, &handle);
        if ((retval_to_error_t(ret) == E_SUCCESS) &&
            (i == handle))
        {
            mpu_disable_entry(&pre_test_mpu_ctx, handle);
            // Don't bother freeing it since pre_test_mpu_ctx won't be used after this.
        }
        else
        {
            mpu_free_entry(&pre_test_mpu_ctx, handle);
        }
    }
}

/** \file
 * @testgroup{MPU}
 * @testgrouppurpose{Testgroup purpose is to test the MPU driver in mpu.c}
 */
UT_SUITE_DEFINE(MPU,
                UT_SUITE_SET_COMPONENT("MPU")
                UT_SUITE_SET_DESCRIPTION("mpu tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(mpu_test_setup)
                UT_SUITE_TEARDOWN_HOOK(mpu_test_teardown))

UT_CASE_DEFINE(MPU, access_va,
    UT_CASE_SET_DESCRIPTION("create an MPU mapping and access its memory")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542850")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_access_va}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry and mpu_write_entry to create a mapping
 *   2) Access the memory through the mapping and verify that it matches
 *      what we read through the identity mapping.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, access_va)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    ret = mpu_write_entry(&mpu_ctx, handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    UT_ASSERT_EQUAL_INT(memcmp(dmem_buf, (void *) va, DMEM_BUF_SIZE), 0);
}

UT_CASE_DEFINE(MPU, manage_handles,
    UT_CASE_SET_DESCRIPTION("MPU handle management")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542852")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_manage_handles}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry
 *   2) Call mpu_write_entry with the created handle and verify that it succeeded
 *   3) Call mpu_write_entry with an invalid handle and verify that an error was returned
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, manage_handles)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // call an api with a valid handle
    ret = mpu_write_entry(&mpu_ctx, handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // call an api with an invalid handle
    ret = mpu_write_entry(&mpu_ctx, 0xFFFFFFFF,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}

UT_CASE_DEFINE(MPU, mpu_init,
    UT_CASE_SET_DESCRIPTION("Test mpu_init")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542856")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_init}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_init
 *   2) verify that it returns success
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_init)
{
    RET_TYPE ret;

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);
    csr_write(LW_RISCV_CSR_SMPUCTL,
        DRF_NUM(_RISCV_CSR, _SMPUCTL, _ENTRY_COUNT, 128));
    csr_write(LW_RISCV_CSR_SMPUVA,
        DRF_NUM(_RISCV_CSR, _SMPUVA, _VLD, 1));

    ret = mpu_init(&mpu_ctx);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
}

UT_CASE_DEFINE(MPU, mpu_init_preserve_mappings,
    UT_CASE_SET_DESCRIPTION("Test mpu_init and preserve the existing mappings")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542857")
    UT_CASE_LINK_SPECS("TODO"))

const uint32_t pre_reserved_index = 0;

static uint32_t smpuva_mock_read(uint32_t addr, uint8_t size)
{
    uint32_t idx = csr_read(LW_RISCV_CSR_SMPUIDX);
    idx = DRF_VAL64(_RISCV, _CSR_XMPUIDX, _INDEX, idx);

    // Simulate idx 1 being valid and the others being disabled
    if (idx == pre_reserved_index)
    {
        return DRF_NUM(_RISCV_CSR, _XMPUVA, _VLD, 1);
    }
    else
    {
        return 0;
    }
}

/**
 * @testcasetitle{MPU_mpu_init_preserve_mappings}
 * @testdependencies{}
 * @testdesign{
 *   1) Set up register mocking so that the MPU registers report one active mapping
 *   2) Call mpu_init
 *   3) Call mpu_reserve_entry to try to reserve an entry at the same index as the pre-reserved entry
 *   4) Verify that the driver returns a different entry and does not trample the pre-reserved entry
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_init_preserve_mappings)
{
    RET_TYPE ret;
    mpu_handle_t handle;

    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    csr_write(LW_RISCV_CSR_SMPUCTL,
        DRF_NUM(_RISCV_CSR, _SMPUCTL, _ENTRY_COUNT, 128));

    csr_write(LW_RISCV_CSR_SMPUIDX, 0);

    UTF_IO_MOCK(LW_RISCV_CSR_SMPUVA,
                LW_RISCV_CSR_SMPUVA,
                smpuva_mock_read,
                NULL);

    ret = mpu_init(&mpu_ctx);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // There was one enabled entry prior to mpu_init at index 0. Ensure that it has been preserved
    ret = mpu_reserve_entry(&mpu_ctx, pre_reserved_index, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_NOT_EQUAL_UINT(handle, pre_reserved_index);
}

UT_CASE_DEFINE(MPU, mpu_enable,
    UT_CASE_SET_DESCRIPTION("Test mpu_enable")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542858")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_enable}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_enable
 *   2) verify that it returns success and wrote the correct register value
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_enable)
{
    RET_TYPE ret;
    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);

    ret = mpu_enable();
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(csr_read(LW_RISCV_CSR_SATP),
        DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _LWMPU) |
        DRF_DEF64(_RISCV_CSR, _SATP, _ASID, _BARE)  |
        DRF_DEF64(_RISCV_CSR, _SATP, _PPN, _BARE));
}

UT_CASE_DEFINE(MPU, mpu_reserve_entry,
    UT_CASE_SET_DESCRIPTION("Test mpu_reserve_entry")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542859")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_reserve_entry}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry to reserve an entry
 *   2) Verify that it returns a valid handle
 *   3) Call mpu_reserve_entry to reserve an entry
 *   4) Verify that it returns a valid handle which is different from the first handle
 *   5) Call mpu_reserve_entry with a NULL p_reserve_handle
 *   6) Verify that it returns an error
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_reserve_entry)
{
    int idx, idx2;
    mpu_handle_t handles[LW_RISCV_CSR_MPU_ENTRY_COUNT+1];
    RET_TYPE ret;

    // Reserve an entry
    ret = mpu_reserve_entry(&mpu_ctx, 0, &handles[0]);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_IN_RANGE_UINT(handles[0], 0, LW_RISCV_CSR_MPU_ENTRY_COUNT);

    // Reserve another entry. ensure that a different handle is returned
    ret = mpu_reserve_entry(&mpu_ctx, 0, &handles[1]);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_IN_RANGE_UINT(handles[1], 0, LW_RISCV_CSR_MPU_ENTRY_COUNT);
    UT_ASSERT_NOT_EQUAL_UINT(handles[0], handles[1]);

    // call mpu_reserve_entry with bad args
    ret = mpu_reserve_entry(&mpu_ctx, 0, NULL);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}

UT_CASE_DEFINE(MPU, mpu_reserve_entry_with_search_origin,
    UT_CASE_SET_DESCRIPTION("Test mpu_reserve_entry with a search_origin")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542860")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_reserve_entry}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry with a non-zero search origin
 *   2) verify that the reserved entry matches the requested search origin
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_reserve_entry_with_search_origin)
{
    int idx, idx2;
    mpu_handle_t handles[LW_RISCV_CSR_MPU_ENTRY_COUNT+1];
    RET_TYPE ret;
    // Some value that is likely to be free
    const uint32_t search_origin = 127;

    // Reserve an entry and check that we got our requested search_origin
    ret = mpu_reserve_entry(&mpu_ctx, search_origin, &handles[0]);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(handles[0], search_origin);
}

UT_CASE_DEFINE(MPU, mpu_free_entry,
    UT_CASE_SET_DESCRIPTION("Test mpu_free_entry")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542862")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_reserve_entry}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry
 *   2) Call mpu_free_entry and verify that it was freed
 *   3) Call mpu_free_entry to try to free it a second time to and verify that it fails
 *   4) Call mpu_reserve_entry and mpu_free_entry many times in a loop and verify that we don't run out of entries
 *   5) Call mpu_reserve_entry many times in a loop and verify that we run out of entries
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_free_entry)
{
    int idx, idx2;
    mpu_handle_t handles[LW_RISCV_CSR_MPU_ENTRY_COUNT+1];
    RET_TYPE ret;

    // Reserve an entry
    ret = mpu_reserve_entry(&mpu_ctx, 0, &handles[0]);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Free an entry
    ret = mpu_free_entry(&mpu_ctx, handles[0]);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Second free should fail
    ret = mpu_free_entry(&mpu_ctx, handles[0]);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // reserve and free many times and ensure we don't run out of entries
    for (idx = 0; idx < (LW_RISCV_CSR_MPU_ENTRY_COUNT + 1); idx++)
    {
        ret = mpu_reserve_entry(&mpu_ctx, 0, &handles[0]);
        UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

        ret = mpu_free_entry(&mpu_ctx, handles[0]);
        UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    }

    // reserve until we run out of entries. Ensure that reserve eventually returns an error.
    for (idx = 0; idx < (LW_RISCV_CSR_MPU_ENTRY_COUNT + 1); idx++)
    {
        ret = mpu_reserve_entry(&mpu_ctx, 0, &handles[idx]);
        if (retval_to_error_t(ret) == E_SUCCESS)
        {
            UT_ASSERT_IN_RANGE_UINT(handles[idx], 0, LW_RISCV_CSR_MPU_ENTRY_COUNT);
        }
        else
        {
            break;
        }
    }

    UT_ASSERT_NOT_EQUAL_UINT(idx, LW_RISCV_CSR_MPU_ENTRY_COUNT + 1);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_FAULT);
}

UT_CASE_DEFINE(MPU, mpu_write_entry,
    UT_CASE_SET_DESCRIPTION("Test mpu_write_entry")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542868")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_write_entry}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry
 *   2) Call mpu_write_entry on the reserved entry
 *   3) Verify that it succeeds
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_write_entry)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx, handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
}

UT_CASE_DEFINE(MPU, mpu_read_entry,
    UT_CASE_SET_DESCRIPTION("Test mpu_read_entry")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542871")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_read_entry}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry
 *   2) Call mpu_write_entry on the reserved entry
 *   3) Call mpu_read_entry on the same entry
 *   4) Verify that va, pa, rng, and attr values we read back match the ones that we wrote
 *   5) Call mpu_read_entry with NULL p_va, p_pa, p_rng, and p_attr args
 *   6) Verify that it returns success
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_read_entry)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);
    uint64_t va_rb;
    uint64_t pa_rb;
    uint64_t rng_rb;
    uint64_t attr_rb;

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx, handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // read it back
    ret = mpu_read_entry(&mpu_ctx, handle, &va_rb, &pa_rb, &rng_rb, &attr_rb);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(DRF_SHIFTMASK(LW_RISCV_CSR_SMPUVA_BASE) & va_rb, va);
    UT_ASSERT_EQUAL_UINT(pa_rb, pa);
    UT_ASSERT_EQUAL_UINT(rng_rb, rng);
    UT_ASSERT_EQUAL_UINT(attr_rb, attr);

    //it should also work with NULL args
    ret = mpu_read_entry(&mpu_ctx, handle, NULL, NULL, NULL, NULL);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
}

UT_CASE_DEFINE(MPU, mpu_enable_entry,
    UT_CASE_SET_DESCRIPTION("Test mpu_enable_entry")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542874")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_enable_entry}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry
 *   2) Call mpu_write_entry on the reserved entry, not setting the VLD bit in va
 *   3) Call mpu_enable_entry on the same entry
 *   4) Call mpu_read_entry to read back the entry
 *   5) Verify that it returns a va with a set VLD bit
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_enable_entry)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);
    uint64_t va_rb;

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx, handle, va, pa, rng, attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // enable the entry
    ret = mpu_enable_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // read it back
    ret = mpu_read_entry(&mpu_ctx, handle, &va_rb, NULL, NULL, NULL);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(DRF_VAL(_RISCV_CSR, _SMPUVA, _VLD, va_rb), 1);
}

UT_CASE_DEFINE(MPU, mpu_disable_entry,
    UT_CASE_SET_DESCRIPTION("Test mpu_disable_entry")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542876")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_disable_entry}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry
 *   2) Call mpu_write_entry on the reserved entry, not setting the VLD bit in va
 *   3) Call mpu_enable_entry on the same entry
 *   4) Call mpu_disable_entry on the same entry
 *   5) Call mpu_read_entry to read back the entry
 *   6) Verify that it returns a va with a set VLD not bit
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_disable_entry)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);
    uint64_t va_rb;

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx, handle, va, pa, rng, attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // enable the entry
    ret = mpu_enable_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // disable the entry
    ret = mpu_disable_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // read it back
    ret = mpu_read_entry(&mpu_ctx, handle, &va_rb, NULL, NULL, NULL);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(DRF_VAL(_RISCV_CSR, _SMPUVA, _VLD, va_rb), 0);

    // Free the handle
    ret = mpu_free_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
}

UT_CASE_DEFINE(MPU, mpu_retrieve_dirty_accessed,
    UT_CASE_SET_DESCRIPTION("Test the MPU dirty/accessed tracking")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542877")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_retrieve_dirty_accessed}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry and mpu_write_entry to create an entry
 *   2) Call mpu_clear_accessed_bit and mpu_clear_dirty_bit to get the A/D bits to a known state
 *   3) Read the memory of the mapping
 *   4) Call mpu_is_accessed and verify that it returns true
 *   5) Write the memory of the mapping
 *   6) Call mpu_is_dirty and verify that it returns true
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_retrieve_dirty_accessed)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    bool b_accessed;
    bool b_dirty;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);
    uint32_t * buf_through_mpu_window = (uint32_t *)va;

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx,
                        handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Clear accessed and dirty bits
    ret = mpu_clear_accessed_bit(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    ret = mpu_clear_dirty_bit(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Access it
    UT_ASSERT_EQUAL_UINT(buf_through_mpu_window[0], dmem_buf[0]);

    ret = mpu_is_accessed(&mpu_ctx, handle, &b_accessed);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_TRUE(b_accessed);

    // Make it dirty
    buf_through_mpu_window[0] = 0xDEADBEEF;

    ret = mpu_is_dirty(&mpu_ctx, handle, &b_dirty);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_TRUE(b_dirty);
}

UT_CASE_DEFINE(MPU, mpu_clear_dirty_accessed,
    UT_CASE_SET_DESCRIPTION("Test clearing of dirty/accessed")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542878")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_clear_dirty_accessed}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry and mpu_write_entry to create an entry
 *   2) read and write the memory of this mapping to make it dirty and accessed
 *   3) Call mpu_clear_accessed_bit
 *   4) Call mpu_is_accessed and verify that it returns false
 *   5) Call mpu_clear_dirty_bit
 *   6) Call mpu_is_dirty and verify that it returns false
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_clear_dirty_accessed)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    bool b_accessed;
    bool b_dirty;
    const uint64_t va = 0x1000;
    const uint64_t pa = (uint64_t) dmem_buf;
    const uint64_t rng = DMEM_BUF_SIZE;
    const uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);
    uint32_t * buf_through_mpu_window = (uint32_t *)va;

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx,
                        handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // make it accessed and dirty
    UT_ASSERT_EQUAL_UINT(buf_through_mpu_window[0], dmem_buf[0]);
    buf_through_mpu_window[0] = 0xDEADBEEF;

    // Clear accessed and dirty bits
    ret = mpu_clear_accessed_bit(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    ret = mpu_is_accessed(&mpu_ctx, handle, &b_accessed);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_FALSE(b_accessed);

    ret = mpu_clear_dirty_bit(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    ret = mpu_is_dirty(&mpu_ctx, handle, &b_dirty);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_FALSE(b_dirty);
}

UT_CASE_DEFINE(MPU, mpu_va_to_pa,
    UT_CASE_SET_DESCRIPTION("Test mpu_va_to_pa")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542879")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_va_to_pa}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry and mpu_write_entry to create a mapping
 *   2) Call mpu_va_to_pa with b_only_enabled=true to translate an address within that mapping
 *   3) Verify that the translation is successful and returns the correct pa
 *   4) Call mpu_disable_entry to disable the mapping
 *   5) Call mpu_va_to_pa with b_only_enabled=true to translate an address within that mapping
 *   6) Verify that the translation returns an error code
  *  7) Call mpu_va_to_pa with b_only_enabled=false to translate an address within that mapping
 *   8) Verify that the translation is successful and returns the correct pa
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_va_to_pa)
{
    mpu_handle_t handle;
    RET_TYPE ret;
    uint64_t va = 0x1000;
    uint64_t pa = (uint64_t) dmem_buf;
    uint64_t rng = DMEM_BUF_SIZE;
    uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);
    uint64_t pa_rb;

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx,
                        handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Do a translation
    ret = mpu_va_to_pa(&mpu_ctx, va + 100, true, &pa_rb);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(pa_rb, pa + 100);

    // disable the entry
    ret = mpu_disable_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Translation should fail now that entry is disabled
    ret = mpu_va_to_pa(&mpu_ctx, va + 100, true, &pa_rb);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // But it will work if b_only_enabled is false
    ret = mpu_va_to_pa(&mpu_ctx, va + 100, false, &pa_rb);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    UT_ASSERT_EQUAL_UINT(pa_rb, pa + 100);
}

UT_CASE_DEFINE(MPU, mpu_va_to_pa_badargs,
    UT_CASE_SET_DESCRIPTION("Test mpu_va_to_pa")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5542879")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_va_to_pa_badargs}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry and mpu_write_entry to create a mapping
 *   2) Call mpu_va_to_pa with an unmapped VA and verify that it returns an error
 *   3) Call mpu_va_to_pa with a NULL p_pa and verify that it returns an error
 *   4) Call mpu_write_entry with large va and rng values that would overflow when added
 *   5) Call mpu_va_to_pa with a va in the above range and verify that it returns an error
 *   6) Call mpu_write_entry with a large pa value
 *   7) Call mpu_va_to_pa with a va such that (va - va_base) + pa_base would overflow
 *   8) Verify that it returns an error
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_va_to_pa_badargs)
{
   mpu_handle_t handle;
    RET_TYPE ret;
    uint64_t va = 0x1000;
    uint64_t pa = (uint64_t) dmem_buf;
    uint64_t rng = DMEM_BUF_SIZE;
    uint64_t attr = DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
                          DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1);
    uint64_t pa_rb;

    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // write an entry
    ret = mpu_write_entry(&mpu_ctx,
                        handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Call it with an unmapped va
    ret = mpu_va_to_pa(&mpu_ctx, 0, true, &pa_rb);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // Call it with a NULL pa
    ret = mpu_va_to_pa(&mpu_ctx, va + 100, true, NULL);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // va_base + rng overflow
    va = 0xFFFFFFFFFFFFF000ULL;
    rng = 0xFFFFFFFFFFFFF000ULL;
    ret = mpu_write_entry(&mpu_ctx,
                        handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    ret = mpu_va_to_pa(&mpu_ctx, va + 100, true, &pa_rb);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    // overflow (va - va_base) + pa_base
    va = 0x1000ULL;
    pa = 0xFFFFFFFFFFFFF000ULL;
    rng = 0x10000ULL;
    ret = mpu_write_entry(&mpu_ctx,
                        handle,
                        va | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
                        pa,
                        rng,
                        attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);
    ret = mpu_va_to_pa(&mpu_ctx, va + 0xFFFF, true, &pa_rb);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}

UT_CASE_DEFINE(MPU, mpu_null_ctx,
    UT_CASE_SET_DESCRIPTION("Call MPU functions with NULL context argument")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5628396")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_null_ctx}
 * @testdependencies{}
 * @testdesign{
 *   1) Call all the mpy driver APIs with a NULL p_ctx arg
 *   2) Verify that the all return an error
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_null_ctx)
{
    RET_TYPE ret;
    mpu_handle_t handle;
    bool b_arg;
    uint64_t va;
    uint64_t pa;
    uint64_t rng;
    uint64_t attr;

    ret = mpu_init(NULL);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_reserve_entry(NULL, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_free_entry(NULL, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_write_entry(NULL, handle, 0, 0, 0, 0);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_read_entry(NULL, handle, &va, &pa, &rng, &attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_enable_entry(NULL, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_disable_entry(NULL, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_is_accessed(NULL, handle, &b_arg);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_is_dirty(NULL, handle, &b_arg);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_clear_accessed_bit(NULL, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_clear_dirty_bit(NULL, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_va_to_pa(NULL, va, true, &pa);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}

UT_CASE_DEFINE(MPU, mpu_ilwalid_handle,
    UT_CASE_SET_DESCRIPTION("Call MPU functions with bad handle")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("GID-REQ-5628396")
    UT_CASE_LINK_SPECS("TODO"))

/**
 * @testcasetitle{MPU_mpu_ilwalid_handle}
 * @testdependencies{}
 * @testdesign{
 *   1) Call mpu_reserve_entry to reserve a handle
 *   2) Call mpu_free_entry to free it
 *   3) Call all the MPU driver APIs, passing in the freed handle
 *   4) Verify that they all return an error
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_RUN(MPU, mpu_ilwalid_handle)
{
    RET_TYPE ret;
    mpu_handle_t handle;
    bool b_arg;
    uint64_t va;
    uint64_t pa;
    uint64_t rng;
    uint64_t attr;

    // Reserve an entry
    ret = mpu_reserve_entry(&mpu_ctx, 0, &handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // Free the handle
    ret = mpu_free_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    // It is now invalid. Call a bunch of functions and make sure they can handle it
    ret = mpu_write_entry(&mpu_ctx, handle, 0, 0, 0, 0);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_read_entry(&mpu_ctx, handle, &va, &pa, &rng, &attr);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_enable_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_disable_entry(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_is_accessed(&mpu_ctx, handle, &b_arg);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_is_dirty(&mpu_ctx, handle, &b_arg);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_clear_accessed_bit(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);

    ret = mpu_clear_dirty_bit(&mpu_ctx, handle);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_ILWALID_PARAM);
}
