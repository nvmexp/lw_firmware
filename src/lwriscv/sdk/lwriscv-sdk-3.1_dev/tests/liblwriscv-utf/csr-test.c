/*Copyright (c) 2020-2021, LWPU CORPORATION.  All Rights Reserved.
*
* LWPU Corporation and its licensors retain all intellectual property and
* proprietary rights in and to this software and related documentation.  Any
* use, reproduction, disclosure or distribution of this software and related
* documentation without an express license agreement from LWPU Corporation
* is strictly prohibited.
*
*/

#include <stdint.h>

#include <ut/ut.h>
#include <ut/ut_case.h>
#include <ut/ut_assert.h>
#include "ut/ut_fake_mem.h"

#include <test-macros.h>
#include <regmock.h>
#include <riscv-init.h>
#include LWRISCV64_MANUAL_ADDRESS_MAP

#if defined(UTF_USE_LIBLWRISCV)
    #include <lwriscv/csr.h>
#elif defined(UTF_USE_LIBFSP)
    #include <cpu/csr.h>
#else
    #error UTF_USE_LIBLWRISCV or UTF_USE_LIBFSP must be defined.
#endif

#define DEFAULT_VALUE    0xAAAABBBBUL
#define SCRATCH_REGISTER LW_RISCV_CSR_SSCRATCH2

/** Test Setup */
static void *csrSetup(void)
{
    UTF_IO_SET_MOCKS_ENABLED(LW_FALSE);
}

/**
 * \file csr-test.c
 *
 * @testgroup{CSR}
 * @testgrouppurpose{Test suite to test the csr macros}
 * @testgroupsetup{
 *      Test suite setup includes the following:
 *      1. Disable register mocking
 * }
 * @testgroupteardown{
 *      There is no teardown process.
 * }
 */
UT_SUITE_DEFINE(CSR,
                UT_SUITE_SET_COMPONENT("CSR")
                UT_SUITE_SET_DESCRIPTION("Testing the CSR functions")
                UT_SUITE_SET_OWNER("aadranly")
                UT_SUITE_SETUP_HOOK(csrSetup)
                UT_SUITE_TEARDOWN_HOOK(NULL))

#define PRINT_REG_VAL(a,v) utf_printf("%s(%d) : addr = 0x%x, value = 0x%x\n", \
                                                __FUNCTION__, __LINE__, a, v)

/**
 * @testcasetitle{CSR_write_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value into scratch CSR.
 *      2) Read from the CSR scratch.
 *      3) Assert that the value written into the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, write_test,
               UT_CASE_SET_DESCRIPTION("Testing the write function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22481770"))

UT_CASE_RUN(CSR, write_test)
{
    LwU32 reg_write;

    csr_write(SCRATCH_REGISTER, DEFAULT_VALUE);
    reg_write = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(reg_write, DEFAULT_VALUE);
}

/**
 * @testcasetitle{CSR_read_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value into scratch CSR.
 *      2) Read from the CSR scratch.
 *      3) Assert that the value written into the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, read_test,
               UT_CASE_SET_DESCRIPTION("Testing the read function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482025"))

UT_CASE_RUN(CSR, read_test)
{
    LwU32 reg_read;

    csr_write(SCRATCH_REGISTER, DEFAULT_VALUE);
    reg_read = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(reg_read, DEFAULT_VALUE);
}

/**
 * @testcasetitle{CSR_set_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Set bits of a scratch CSR using a known mask.
 *      2) Read from the CSR scratch.
 *      3) Assert that the bits set in the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, set_test,
               UT_CASE_SET_DESCRIPTION("Testing the set function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482031"))

UT_CASE_RUN(CSR, set_test)
{
    LwU32 reg_set;

    csr_write(SCRATCH_REGISTER, 0x00000000UL);
    csr_set(SCRATCH_REGISTER, DEFAULT_VALUE);
    reg_set = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(reg_set, DEFAULT_VALUE);
}

/**
 * @testcasetitle{CSR_clear_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known initial value into the scratch CSR.
 *      2) Clear bits of the scratch CSR using a known mask.
 *      3) Read from the CSR scratch.
 *      4) Assert that the bits cleared from the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, clear_test,
               UT_CASE_SET_DESCRIPTION("Testing the clear function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482034"))

UT_CASE_RUN(CSR, clear_test)
{
    LwU32 reg_clear;

    csr_write(SCRATCH_REGISTER, 0x00000000UL);
    csr_clear(SCRATCH_REGISTER, DEFAULT_VALUE);

    reg_clear = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(reg_clear, 0x00000000UL);
}

/**
 * @testcasetitle{CSR_read_clear_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value into CSR scratch.
 *      2) Call the function under test (csr_read_and_clear).
 *      3) Assert that the value read matches the known value
 *         initially written to the CSR.
 *      4) Assert that the bits cleared in the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, read_clear_test,
               UT_CASE_SET_DESCRIPTION("Testing the read and clear function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482178"))

UT_CASE_RUN(CSR, read_clear_test)
{
    LwU32 current;
    LwU32 reg_read_clear;

    csr_write(SCRATCH_REGISTER, DEFAULT_VALUE);

    current = csr_read_and_clear(SCRATCH_REGISTER, DEFAULT_VALUE);
    reg_read_clear = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(current, DEFAULT_VALUE);
    UT_ASSERT_EQUAL_UINT(reg_read_clear, 0x00000000UL);
}

/**
 * @testcasetitle{CSR_read_set_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value into CSR scratch.
 *      2) Call the function under test (csr_read_and_set).
 *      3) Assert that the value read matches the known value
 *         initially written to the CSR.
 *      4) Assert that the bits set in the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, read_set_test,
               UT_CASE_SET_DESCRIPTION("Testing the read and set function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482188"))

UT_CASE_RUN(CSR, read_set_test)
{
    LwU64 current;
    LwU32 reg_read_set;

    csr_write(SCRATCH_REGISTER, 0x00000000UL);

    current = csr_read_and_set(SCRATCH_REGISTER, DEFAULT_VALUE);
    reg_read_set = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(current, 0x00000000UL);
    UT_ASSERT_EQUAL_UINT(reg_read_set, DEFAULT_VALUE);
}

/**
 * @testcasetitle{CSR_read_clear_imm_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value into CSR scratch.
 *      2) Call the function under test (csr_read_and_clear_imm).
 *      3) Assert that the value read matches the known value
 *         initially written to the CSR.
 *      4) Assert that the bits cleared in the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, read_clear_imm_test,
               UT_CASE_SET_DESCRIPTION("Testing the read and clear immediate function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482241"))

UT_CASE_RUN(CSR, read_clear_imm_test)
{
    LwU64 current;
    LwU32 reg_read_clear_imm;
    LwU32 reg_temp;

    csr_write(SCRATCH_REGISTER, DEFAULT_VALUE);
    reg_temp = csr_read_and_clear_imm(SCRATCH_REGISTER, 0xF);
    reg_read_clear_imm = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(reg_temp, DEFAULT_VALUE);
    UT_ASSERT_EQUAL_UINT(reg_read_clear_imm, 0xAAAABBB0UL);
}

/**
 * @testcasetitle{CSR_read_set_imm_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value into CSR scratch.
 *      2) Call the function under test (csr_read_and_set_imm).
 *      3) Assert that the value read matches the known value
 *         initially written to the CSR.
 *      4) Assert that the bits set in the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, read_set_imm_test,
               UT_CASE_SET_DESCRIPTION("Testing the read and set immediate function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482210"))

UT_CASE_RUN(CSR, read_set_imm_test)
{
    LwU64 current;
    LwU32 reg_read_set_imm;
    LwU32 reg_temp;

    csr_write(SCRATCH_REGISTER, 0x00000000UL);
    reg_temp = csr_read_and_set_imm(SCRATCH_REGISTER, 0xFUL);
    reg_read_set_imm = csr_read(SCRATCH_REGISTER);

    UT_ASSERT_EQUAL_UINT(reg_temp, 0x00000000UL);
    UT_ASSERT_EQUAL_UINT(reg_read_set_imm, 0xFUL);
}

/**
 * @testcasetitle{CSR_read_write_imm_test}
 * @testdependencies{}
 * @testdesign{
 *      1) Write a known value into CSR scratch.
 *      2) Call the function under test (csr_read_and_write_imm).
 *      3) Assert that the value read matches the known value
 *         initially written to the CSR.
 *      4) Assert that the value written in the scratch register
 *         matches what is expected.
 * }
 * @testmethod{Requirements-Based}
 * @casederiv{Analysis of requirements}
 */
UT_CASE_DEFINE(CSR, read_write_imm_test,
               UT_CASE_SET_DESCRIPTION("Testing the read and write immediate function")
               UT_CASE_SET_TYPE(REQUIREMENTS)
               UT_CASE_LINK_REQS("GID-REQ-22482226"))

UT_CASE_RUN(CSR, read_write_imm_test)
{
    LwU32 reg_read_write_imm;

    csr_write(SCRATCH_REGISTER, 0x00000000UL);
    csr_read_and_write_imm(SCRATCH_REGISTER, 0xFUL);
    reg_read_write_imm = csr_read(SCRATCH_REGISTER);

	UT_ASSERT_EQUAL_UINT(reg_read_write_imm, 0xFUL);
}
