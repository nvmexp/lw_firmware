/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <ut/ut.h>
#include <ut/ut_case.h>
#include <ut/ut_assert.h>
#include <ut/ut_fake_mem.h>

#include <test-macros.h>
#include <regmock.h>
#include LWRISCV64_MANUAL_ADDRESS_MAP

#if defined(UTF_USE_LIBLWRISCV)
    #include <liblwriscv/io.h>
#elif defined(UTF_USE_LIBFSP)
    #include <cpu/io.h>
    #include <cpu/mmio-access.h>
#else
    #error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined.
#endif

/**
 * IO Driver Unit Tests
 *
 * Tests ilwolving register reads and writes.
 *
 * Link to IO Driver Documentation: https://confluence.lwpu.com/display/LW/liblwriscv+IO+driver
 * Link to FSP Jama Requirements: TODO
 */

/** --------------------------------- TEST SET UP --------------------------------- */
/*
 * Generic test setup.
 */
static void *ioTestSetup(void)
{
    UTF_IO_SET_MOCKS_ENABLED(LW_TRUE);
}

/**
 * \file io-test.c
 *
 * @testgroup{IO}
 * @testgrouppurpose{Test suite to test the io driver}
 * @testgroupsetup{
 *      Test suite setup includes the following:
 *      1. Enable register mocking
 * }
 * @testgroupteardown{
 *      There is no teardown process.
 * }
 */
UT_SUITE_DEFINE(IO,
                UT_SUITE_SET_COMPONENT("IO")
                UT_SUITE_SET_DESCRIPTION("IO driver tests")
                UT_SUITE_SET_OWNER("aadranly")
                UT_SUITE_SETUP_HOOK(ioTestSetup)
                UT_SUITE_TEARDOWN_HOOK(NULL))

#define PRINT_REG_VAL(a,v) utf_printf("%s(%d) : addr = 0x%x, value = 0x%x\n", \
                                                __FUNCTION__, __LINE__, a, v)

#define PRINT_REG_VAL64(a,v) utf_printf("%s(%d) : addr = 0x%x, value = 0x%llx\n", \
                                                __FUNCTION__, __LINE__, a, v)

/** ------------------------------ COMMON TEST CASES ------------------------------ */
/**
 * @testcasetitle{IO_local_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value to a local address
 *      2) Read from the same local address
 *      3) Compare and confirm that the value read from the local address is
 *         equivalent to the value written previously.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(IO, local_test,
    UT_CASE_SET_DESCRIPTION("Testing local register read/write through io interface")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO"))

UT_CASE_RUN(IO, local_test)
{
    LwU32 addr = 0x5050;
    LwU32 reg32;

    localWrite(addr, 0xABCD);

    reg32 = localRead(addr);
    PRINT_REG_VAL(addr, reg32);

    UT_ASSERT_EQUAL_UINT32(reg32, 0xABCD);
}

#if LWRISCV_HAS_CSB_MMIO || (LWRISCV_HAS_PRI && LWRISCV_HAS_CSB_OVER_PRI)
/**
 * @testcasetitle{IO_csb_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value to a csb address
 *      2) Read from the same csb address
 *      3) Compare and confirm that the value read from the csb address is
 *         equivalent to the value written previously.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(IO, csb_test,
    UT_CASE_SET_DESCRIPTION("Testing csb register read/write through io interface")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO"))

UT_CASE_RUN(IO, csb_test)
{
    LwU32 addr = 0x5050;
    LwU32 reg32;

    csbWrite(addr, 0xABCD);

    reg32 = csbRead(addr);
    PRINT_REG_VAL(addr, reg32);

    UT_ASSERT_EQUAL_UINT32(reg32, 0xABCD);
}
#endif

#if LWRISCV_HAS_PRI
/**
 * @testcasetitle{IO_pri_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value to a pri address
 *      2) Read from the same pri address
 *      3) Compare and confirm that the value read from the pri address is
 *         equivalent to the value written previously.
 *      4) Follow steps 1-3 for both falcon read/write and riscv read/write
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(IO, pri_test,
    UT_CASE_SET_DESCRIPTION("Testing pri register read/write through io interface")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO"))

UT_CASE_RUN(IO, pri_test)
{
    LwU32 addr = 0x5050;
    LwU32 reg32;

    // pri - read/write
    priWrite(addr, 0xABCD);

    reg32 = priRead(addr);
    PRINT_REG_VAL(addr, reg32);
    UT_ASSERT_EQUAL_UINT32(reg32, 0xABCD);

    // falcon - read/write
    falconWrite(addr, 0xABCD);

    reg32 = falconRead(addr);
    PRINT_REG_VAL(addr, reg32);
    UT_ASSERT_EQUAL_UINT32(reg32, 0xABCD);

    // riscv - read/write
    riscvWrite(addr, 0xABCD);

    reg32 = riscvRead(addr);
    PRINT_REG_VAL(addr, reg32);
    UT_ASSERT_EQUAL_UINT32(reg32, 0xABCD);
}
#endif

#if defined(UTF_USE_LIBLWRISCV)
/** ---------------------------- LIBLWRISCV TEST CASES ---------------------------- */
    // unique liblwriscv test cases
#elif defined(UTF_USE_LIBFSP)
/** ------------------------------ LIBFSP TEST CASES ------------------------------ */
    // unique libfsp test cases
    /**
     * @testcasetitle{IO_mmio64_test}
     * @testdependencies{}
     * @testdesign{
     *      1) Write a known 64-bit value to a memory address with iowrite64_offset
     *      2) Read from the same 64-bit address from the same memory address
     *         ioread64_offset
     *      3) Verify that the value read from the memory address is the same as
     *         the value written into that address.
     * }
     * @testmethod{Requirements-Based}
     * @casederiv{Analysis of requirements}
     */
    UT_CASE_DEFINE(IO, mmio64_test,
        UT_CASE_SET_DESCRIPTION("Testing 64-bit read/write to mmio")
        UT_CASE_SET_TYPE(REQUIREMENTS)
        UT_CASE_LINK_REQS("TODO"))

    UT_CASE_RUN(IO, mmio64_test)
    {
        uintptr_t base = LW_RISCV_AMAP_DMEM_START;
        LwU32 offset = 0x00000440UL;
        LwU64 reg64;

        iowrite64_offset(base, offset, 0xFFFFFFFFFFFFFFFFULL);

        reg64 = ioread64_offset(base, offset);
        PRINT_REG_VAL64(base+offset, reg64);

        UT_ASSERT_EQUAL_UINT64(reg64, 0xFFFFFFFFFFFFFFFFULL);
    }
#endif
