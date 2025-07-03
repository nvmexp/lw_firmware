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
#else
    #error This test is only compatible with liblwriscv
#endif

#define ILWALID_MPU_HANDLE ((mpu_handle_t)-1)

// one MMU page
#define DMEM_BUF_SIZE  0x400
#define DMEM_BUF_ALIGN 0x400

static uint32_t*  dmem_buf;
static mpu_context_t mpu_ctx;

static void *hashmpu_test_setup(void)
{
    RET_TYPE ret;

    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);

    ret = mpu_init(&mpu_ctx);
    UT_ASSERT_EQUAL_UINT(retval_to_error_t(ret), E_SUCCESS);

    dmem_buf = (uint32_t *) utfMallocAligned(DMEM_BUF_SIZE, DMEM_BUF_ALIGN);
    UT_ASSERT_NOT_NULL(dmem_buf);

    for (int i = 0; i < DMEM_BUF_SIZE/sizeof(uint32_t); i++)
    {
        dmem_buf[i] = i;
    }
}

UT_SUITE_DEFINE(HASHMPU,
                UT_SUITE_SET_COMPONENT("MPU")
                UT_SUITE_SET_DESCRIPTION("mpu tests")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(hashmpu_test_setup)
                UT_SUITE_TEARDOWN_HOOK(NULL))
/*
 * The Following code tests the Hash MPU HW feature.
 * There is no liblwriscv driver support for this feature
 * so the tests are directly writing hardware registers.
 */

__attribute__((section(".imem_resident")))
static uint8_t hash(uint64_t address, uint16_t param)
{
    uint16_t s0 = param & 0x3f;
    uint16_t s1 = (param >> 8) & 0x3f;
    return ((address >> s0) ^ (address >> s1)) & 0x1f;
}

__attribute__((section(".imem_resident")))
static uint16_t create_hash_param(uint16_t s0, uint16_t s1)
{
    return (uint16_t)(s0 | (s1 << 8));
}

__attribute__((section(".imem_resident")))
static mpu_handle_t reserve_and_write_hashed_entry(
    mpu_context_t *ctx,
    uint32_t block_start,
    uint64_t va,
    uint64_t pa,
    uint64_t page_size,
    uint64_t attr)
{
    mpu_handle_t hndl;
    uint64_t page_mask;

    LWRV_STATUS ret = mpu_reserve_entry(ctx, block_start, &hndl);
    //reserved index should be no more than 4 ahead of the requested block_start
    if ((ret != LWRV_OK) || (hndl-block_start > 3))
    {
        return ILWALID_MPU_HANDLE;
    }

    page_mask = (page_size - 1) >> 3;
    pa = (pa >> 2) | page_mask;

    ret = mpu_write_entry(ctx, hndl, va, pa, 0, attr);
    if (ret != LWRV_OK)
    {
        return ILWALID_MPU_HANDLE;
    }

    return hndl;
}

__attribute__((section(".imem_resident")))
static void disable_and_free_all_mpu_entries(mpu_context_t *ctx)
{
    mpu_handle_t hndl;
    for (hndl = 0; hndl < LW_RISCV_CSR_MPU_ENTRY_COUNT; hndl++)
    {
        mpu_disable_entry(ctx, hndl);
        mpu_free_entry(ctx, hndl);
    }
}

UT_CASE_DEFINE(HASHMPU, mpu_hash,
    UT_CASE_SET_DESCRIPTION("Test MPU hashed mode")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

__attribute__((section(".imem_resident")))
UT_CASE_RUN(HASHMPU, mpu_hash)
{
    LWRV_STATUS ret;
    /* make stack copies of non-resident variables that we can access when hashed MPU is on */
    mpu_context_t ctx = mpu_ctx;
    uint32_t* buf = dmem_buf;
    mpu_handle_t hndl;
    uint64_t reg;
    uint8_t idx;
    uint64_t fail_location = 0;

    /* V0 will be used to identity map the entire IMEM and DMEM range 0x100000-0x200000
     * in a single page. Don't access non-resident code/data (Not properly mapped to FB).
     *
     * We will also identity map the first page of INTIO, since we may have to access csb
     */
    uint64_t v0Pagesize = 0x100000;
    /* V1 and V2 will be used for testing. Use a small size of 0x400 to ensure that buf
     * will be aligned to a page boundary.
     */
    uint64_t v1Pagesize = 0x400;
    uint64_t v2Pagesize = 0x400;

    /* Pick shifts that are >= than the page size
     * volatile so that compiler doesn't try to make these constants in some non-resident region
     */
    volatile uint16_t v0 = create_hash_param(20, 23);
    volatile uint16_t v1 = create_hash_param(11, 14);
    volatile uint16_t v2 = create_hash_param(19, 20);

    /* Pick VAs for v1 and v2 which will use to access the test buffer.
     * Ensure that they don't overlap the identity mapping or each other
     */
    uint64_t buf_va_v1 = LW_RISCV_AMAP_DMEM_END;
    uint64_t buf_va_v2 = 0x400000;
    uint32_t* buf_through_v1 = (uint32_t*) buf_va_v1;
    uint32_t* buf_through_v2 = (uint32_t*) buf_va_v2;

    /* Disable MPU. From this point on we can only access resident code/data.
     * Don't use ASSERTs, since we don't want to abort this test before restoring
     * the MPU.
     */
    csr_write( LW_RISCV_CSR_SATP, DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _BARE) |
        DRF_DEF64(_RISCV_CSR, _SATP, _ASID, _BARE)  |
        DRF_DEF64(_RISCV_CSR, _SATP, _PPN, _BARE));

    /* Get rid of our default mappings */
    disable_and_free_all_mpu_entries(&ctx);

    csr_write(LW_RISCV_CSR_SMPUHP,
        DRF_NUM64(_RISCV_CSR, _SMPUHP, _V0, v0) |
        DRF_NUM64(_RISCV_CSR, _SMPUHP, _V1, v1) |
        DRF_NUM64(_RISCV_CSR, _SMPUHP, _V2, v2));

    /* Set up a hashed MPU identity mapping for all of IMEM and DMEM */
    idx = (uint8_t)(hash(LW_RISCV_AMAP_IMEM_START, v0) * 4U);

    hndl = reserve_and_write_hashed_entry(&ctx, idx,
        LW_RISCV_AMAP_IMEM_START | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
        LW_RISCV_AMAP_IMEM_START,
        v0Pagesize,
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1) |
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XX, 1)
    );
    if (hndl == ILWALID_MPU_HANDLE)
    {
        fail_location = 1;
        goto mpu_hash_end;
    }

    /* enable hashed MPU mode */
    __asm__ volatile("sfence.vma");
    _csr_write(LW_RISCV_CSR_SATP, DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _HASH_LWMPU ) |
        DRF_DEF64(_RISCV_CSR, _SATP, _ASID, _BARE)  |
        DRF_DEF64(_RISCV_CSR, _SATP, _PPN, _BARE));


    /* Create a mapping for buf_va_v1 in each of the 4 possible MPU entries */
    idx = (uint8_t)(hash(buf_va_v1, v1) * 4U);

    /* Loop through the 4-entry block */
    for (hndl = idx; hndl <= idx+3; hndl++)
    {
        /* Map buf using a V1 page */
        hndl = reserve_and_write_hashed_entry(&ctx, hndl,
            ((uint64_t)buf_va_v1) | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
            ((uint64_t)buf),
            v1Pagesize,
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1) |
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XX, 1)
        );

        if (hndl == ILWALID_MPU_HANDLE)
        {
            fail_location = 2;
            goto mpu_hash_end;
        }

        /* Check that buffer is visible through this mapping */
        if (memcmp(buf, buf_through_v1, DMEM_BUF_SIZE))
        {
            fail_location = 3 | (hndl << 16);
            goto mpu_hash_end;
        }

        if (hndl < idx+3)
        {
            /* free all mappings except the last one. Leave it active for the next part of the test */
            mpu_disable_entry(&ctx, hndl);
            mpu_free_entry(&ctx, hndl);
        }
    }

    /* Create a mapping for buf_va_v2 */
    idx = (uint8_t)(hash(buf_va_v2, v2) * 4U);

    /* Map buf using a V2 page */
    hndl = reserve_and_write_hashed_entry(&ctx, idx,
            ((uint64_t)buf_va_v2) | DRF_NUM64(_RISCV_CSR, _XMPUVA, _VLD, 1),
            ((uint64_t)buf),
            v2Pagesize,
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XR, 1) |
        DRF_NUM64(_RISCV_CSR, _XMPUATTR, _XW, 1)
    );

    if (hndl == ILWALID_MPU_HANDLE)
    {
        fail_location = 4;
        goto mpu_hash_end;
    }

    /* Check that buffer is visible through this mapping */
    if (memcmp(buf, buf_through_v2, DMEM_BUF_SIZE))
    {
        fail_location = 5 | (hndl<<16);
        goto mpu_hash_end;
    }

mpu_hash_end:
    /* disable MPU and switch back into non-hashed mode. Restore our old mappings. */
    csr_write(LW_RISCV_CSR_SATP,
        DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _BARE) |
        DRF_DEF64(_RISCV_CSR, _SATP, _ASID, _BARE) |
        DRF_DEF64(_RISCV_CSR, _SATP, _PPN, _BARE));

    disable_and_free_all_mpu_entries(&ctx);

    /* UTF routine to set up MPU */
    initMpu();

    UT_ASSERT_EQUAL_UINT(fail_location, 0);
}
