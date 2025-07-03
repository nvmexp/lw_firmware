/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file lib_lw64-test.c
 *
 * @details    This file contains all the unit tests for the Unit Math LwU64 functions.
 *
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include <ut/ut.h>
#include "ut/ut_case.h"
#include "ut/ut_assert.h"
#include "ut/ut_fake_mem.h"

/* ------------------------ Application Includes ---------------------------- */
#include "flcnifcmn.h"
#include "lib_lw64.h"

/* -------------------------------------------------------- */
/*!
 * Definition of the Unit Math LwU64 functions Suite.
 */
UT_SUITE_DEFINE(MATHLW64,
                UT_SUITE_SET_COMPONENT("Unit Math LwU64 functions")
                UT_SUITE_SET_DESCRIPTION("Unit tests for the Unit Math LwU64 functions")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))


UT_CASE_DEFINE(MATHLW64, LWU64_LO,
  UT_CASE_SET_DESCRIPTION("Ensure LWU64_LO works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_LWU64_LO]"))

/*!
 * @brief      Test the LWU64_LO function
 *
 * @details    The test shall ilwoke LWU64_LO and ensure that we can successfully extract the low 32 bits of an LwU64
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, LWU64_LO)
{
    LWU64_TYPE op1;
    op1.val = 0x1234567890abcdef;

    LwU32 result = LWU64_LO(op1);
    LwU32 expectedresult = 0x90abcdef;

    UT_ASSERT_EQUAL_UINT(result, expectedresult);
}

UT_CASE_DEFINE(MATHLW64, LWU64_HI,
  UT_CASE_SET_DESCRIPTION("Ensure LWU64_HI works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_LWU64_HI]"))

/*!
 * @brief      Test the LWU64_HI function
 *
 * @details    The test shall ilwoke LWU64_LO and ensure that we can successfully extract the high 32 bits of an LwU64
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, LWU64_HI)
{
    LWU64_TYPE op1;
    op1.val = 0x1234567890abcdef;

    LwU32 result = LWU64_HI(op1);
    LwU32 expectedresult = 0x12345678;

    UT_ASSERT_EQUAL_UINT(result, expectedresult);
}

UT_CASE_DEFINE(MATHLW64, lw64CmpNE,
  UT_CASE_SET_DESCRIPTION("Ensure lw64CmpNE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64CmpNE]"))

/*!
 * @brief      Test the lw64CmpNE macro
 *
 * @details    The test shall ilwoke lw64CmpNE:
 *             - where operand1 is not equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is not equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is equal to operand2 (we expect a return value of LW_FALSE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64CmpNE)
{
    LwU64 op1 = 4;
    LwU64 op2 = 5;
    LwU64 op3 = 4;
    LwU64 op4 = 0x400000001;
    LwU64 op5 = 0x500000001;
    LwU64 op6 = 0x400000001;

    UT_ASSERT_TRUE(lw64CmpNE(&op1, &op2));
    UT_ASSERT_FALSE(lw64CmpNE(&op1, &op3));
    UT_ASSERT_TRUE(lw64CmpNE(&op4, &op5));
    UT_ASSERT_FALSE(lw64CmpNE(&op4, &op6));
}

UT_CASE_DEFINE(MATHLW64, lwU64CmpGT,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64CmpGT works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64CmpGT]"))

/*!
 * @brief      Test the lwU64CmpGT macro
 *
 * @details    The test shall ilwoke lwU64CmpGT:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is less than to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is greater than operand2 (we expect a return value of LW_TRUE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64CmpGT)
{
    LwU64 op1 = 4;
    LwU64 op2 = 5;
    LwU64 op3 = 4;
    LwU64 op4 = 3;
    LwU64 op5 = 0x400000001;
    LwU64 op6 = 0x500000001;
    LwU64 op7 = 0x400000001;
    LwU64 op8 = 0x300000001;

    UT_ASSERT_FALSE(lwU64CmpGT(&op1, &op2));
    UT_ASSERT_FALSE(lwU64CmpGT(&op1, &op3));
    UT_ASSERT_TRUE(lwU64CmpGT(&op1, &op4));
    UT_ASSERT_FALSE(lwU64CmpGT(&op5, &op6));
    UT_ASSERT_FALSE(lwU64CmpGT(&op5, &op7));
    UT_ASSERT_TRUE(lwU64CmpGT(&op5, &op8));
}

UT_CASE_DEFINE(MATHLW64, lwU64CmpGE,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64CmpGE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64CmpGE]"))
/*!
 * @brief      Test the lwU64CmpGE macro
 *
 * @details    The test shall ilwoke lwU64CmpGE:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is less than to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is greater than operand2 (we expect a return value of LW_TRUE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64CmpGE)
{
    LwU64 op1 = 4;
    LwU64 op2 = 5;
    LwU64 op3 = 4;
    LwU64 op4 = 3;
    LwU64 op5 = 0x400000001;
    LwU64 op6 = 0x500000001;
    LwU64 op7 = 0x400000001;
    LwU64 op8 = 0x300000001;

    UT_ASSERT_FALSE(lwU64CmpGE(&op1, &op2));
    UT_ASSERT_TRUE(lwU64CmpGE(&op1, &op3));
    UT_ASSERT_TRUE(lwU64CmpGE(&op1, &op4));
    UT_ASSERT_FALSE(lwU64CmpGE(&op5, &op6));
    UT_ASSERT_TRUE(lwU64CmpGE(&op5, &op7));
    UT_ASSERT_TRUE(lwU64CmpGE(&op5, &op8));
}

UT_CASE_DEFINE(MATHLW64, lwU64CmpLT,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64CmpLT works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64CmpLT]"))

/*!
 * @brief      Test the lwU64CmpLT macro
 *
 * @details    The test shall ilwoke lwU64CmpLT:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is less than to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is greater than operand2 (we expect a return value of LW_FALSE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64CmpLT)
{
    LwU64 op1 = 4;
    LwU64 op2 = 5;
    LwU64 op3 = 4;
    LwU64 op4 = 3;
    LwU64 op5 = 0x400000001;
    LwU64 op6 = 0x500000001;
    LwU64 op7 = 0x400000001;
    LwU64 op8 = 0x300000001;

    UT_ASSERT_TRUE(lwU64CmpLT(&op1, &op2));
    UT_ASSERT_FALSE(lwU64CmpLT(&op1, &op3));
    UT_ASSERT_FALSE(lwU64CmpLT(&op1, &op4));
    UT_ASSERT_TRUE(lwU64CmpLT(&op5, &op6));
    UT_ASSERT_FALSE(lwU64CmpLT(&op5, &op7));
    UT_ASSERT_FALSE(lwU64CmpLT(&op5, &op8));
}

UT_CASE_DEFINE(MATHLW64, lwU64CmpLE,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64CmpLE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64CmpLE]"))

/*!
 * @brief      Test the lwU64CmpLE macro
 *
 * @details    The test shall ilwoke lwU64CmpLE:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is less than to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is greater than operand2 (we expect a return value of LW_FALSE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64CmpLE)
{
    LwU64 op1 = 4;
    LwU64 op2 = 5;
    LwU64 op3 = 4;
    LwU64 op4 = 3;
    LwU64 op5 = 0x400000001;
    LwU64 op6 = 0x500000001;
    LwU64 op7 = 0x400000001;
    LwU64 op8 = 0x300000001;

    UT_ASSERT_TRUE(lwU64CmpLE(&op1, &op2));
    UT_ASSERT_TRUE(lwU64CmpLE(&op1, &op3));
    UT_ASSERT_FALSE(lwU64CmpLE(&op1, &op4));
    UT_ASSERT_TRUE(lwU64CmpLE(&op5, &op6));
    UT_ASSERT_TRUE(lwU64CmpLE(&op5, &op7));
    UT_ASSERT_FALSE(lwU64CmpLE(&op5, &op8));
}

UT_CASE_DEFINE(MATHLW64, LW64_FLCN_U64_PACK,
  UT_CASE_SET_DESCRIPTION("Ensure LW64_FLCN_U64_PACK works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_LW64_FLCN_U64_PACK]"))

/*!
 * @brief      Test the LW64_FLCN_U64_PACK macro
 *
 * @details    The test shall ilwoke LW64_FLCN_U64_PACK in two scenarios:
 *             - pack a RM_FLCN_U64 into an LwU64
 *             - unpack a RM_FLCN_U64 into an LwU64
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, LW64_FLCN_U64_PACK)
{
    LwU64 op1 = 0x1234567890abcdef;
    RM_FLCN_U64 expectedresult1 = {0x90abcdef, 0x12345678};
    RM_FLCN_U64 result1;

    RM_FLCN_U64 op2 = {0x90abcdef, 0x12345678};
    LwU64 expectedresult2 = 0x1234567890abcdef;
    LwU64 result2;

    LW64_FLCN_U64_PACK(&result1, &op1);
    UT_ASSERT(memcmp(&result1, &expectedresult1, sizeof(RM_FLCN_U64)) == 0);

    LW64_FLCN_U64_PACK(&result2, &op2);
    UT_ASSERT(expectedresult2 == result2);
}

UT_CASE_DEFINE(MATHLW64, lw64CmpEQ,
  UT_CASE_SET_DESCRIPTION("Ensure lw64CmpEQ works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64CmpEQ]"))

/*!
 * @brief      Test the lw64CmpEQ macro
 *
 * @details    The test shall ilwoke lw64CmpEQ:
 *             - where operand1 is not equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is not equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is a large number (>0xFFFFFFFF) and is equal to operand2 (we expect a return value of LW_TRUE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64CmpEQ)
{
    LwU64 op1 = 4;
    LwU64 op2 = 5;
    LwU64 op3 = 4;
    LwU64 op4 = 0x400000001;
    LwU64 op5 = 0x500000001;
    LwU64 op6 = 0x400000001;

    UT_ASSERT_FALSE(lw64CmpEQ(&op1, &op2));
    UT_ASSERT_TRUE(lw64CmpEQ(&op1, &op3));
    UT_ASSERT_FALSE(lw64CmpEQ(&op4, &op5));
    UT_ASSERT_TRUE(lw64CmpEQ(&op4, &op6));
}

UT_CASE_DEFINE(MATHLW64, lw64Add_SAFE,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Add_SAFE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Add_SAFE]"))

/*!
 * @brief      Test the lw64Add_SAFE function
 *
 * @details    The test shall ilwoke lw64Add_SAFE:
 *             - With numbers that don't cause an overflow as operands, to confirm that the return value matches the sum
 *             - With operands that cause an overflow from the low 32 bits into the high 32 bits
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Add_SAFE)
{
    LwU64 op1 = 5;
    LwU64 op2 = 4;
    LwU64 result1;
    LwU64 expected1 = 9;

    LwU64 op3 = 0x500000003;
    LwU64 op4 = 0x4FFFFFFFF;
    LwU64 result2;
    LwU64 expected2 = 0xA00000002;

    LwU64 op5 = 0x4FFFFFFFF;
    LwU64 op6 = 0x500000003;
    LwU64 result3;
    LwU64 expected3 = 0xA00000002;

    lw64Add_SAFE(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lw64Add_SAFE(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));

    lw64Add_SAFE(&result3, &op5, &op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&result3, &expected3));
}

UT_CASE_DEFINE(MATHLW64, lw64Add_SAFE_overflow,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Add_SAFE correctly detects overflow.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Add_SAFE_overflow]"))

/*!
 * @brief      Test the lw64Add_SAFE function in an overflow condition
 *
 * @details    The test shall ilwoke lw64Add_SAFE with large numbers as operands, to confirm that overflow is detected and halt is called
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Add_SAFE_overflow)
{
    LwU64 op1 = 0xffffffffffffffff;
    LwU64 op2 = 2;
    LwU64 result;

    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();

    lw64Add_SAFE(&result, &op1, &op2);
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

UT_CASE_DEFINE(MATHLW64, lw64Add_MOD,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Add_MOD works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Add_MOD]"))

/*!
 * @brief      Test the lw64Add_MOD function
 *
 * @details    The test shall ilwoke lw64Add_MOD:
 *             - With numbers that don't cause an overflow as operands, to confirm that the return value matches the sum
 *             - With operands that cause an overflow from the low 32 bits into the high 32 bits
 *             - With large numbers as operands, to confirm that the result wraps on overflow
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Add_MOD)
{
    LwU64 op1 = 5;
    LwU64 op2 = 4;
    LwU64 result1;
    LwU64 expected1 = 9;

    LwU64 op3 = 0x500000003;
    LwU64 op4 = 0x4FFFFFFFF;
    LwU64 result2;
    LwU64 expected2 = 0xA00000002;

    LwU64 op5 = 0x4FFFFFFFF;
    LwU64 op6 = 0x500000003;
    LwU64 result3;
    LwU64 expected3 = 0xA00000002;

    LwU64 op7 = 0xffffffffffffffff;
    LwU64 op8 = 2;
    LwU64 result4;
    LwU64 expected4 = 1;

    lw64Add_MOD(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lw64Add_MOD(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));

    lw64Add_MOD(&result3, &op5, &op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&result3, &expected3));

    lw64Add_MOD(&result4, &op7, &op8);
    UT_ASSERT_TRUE(lw64CmpEQ(&result4, &expected4));
}

UT_CASE_DEFINE(MATHLW64, lw64Add32_SAFE,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Add32_SAFE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Add32_SAFE]"))

/*!
 * @brief      Test the lw64Add32_SAFE function
 *
 * @details    The test shall ilwoke lw64Add32_SAFE:
 *             - With numbers that don't cause an overflow as operands, to confirm that the return value matches the sum
 *             - With operands that cause an overflow from the low 32 bits into the high 32 bits
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Add32_SAFE)
{
    LwU64 op1 = 5;
    LwU32 op2 = 4;
    LwU64 expected1 = 9;

    LwU64 op3 = 0x500000003;
    LwU32 op4 = 0xFFFFFFFF;
    LwU64 expected2 = 0x600000002;

    LwU64 op5 = 0x4FFFFFFFF;
    LwU32 op6 = 0x3;
    LwU64 expected3 = 0x500000002;

    lw64Add32_SAFE(&op1, op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&op1, &expected1));

    lw64Add32_SAFE(&op3, op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&op3, &expected2));

    lw64Add32_SAFE(&op5, op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&op5, &expected3));
}

UT_CASE_DEFINE(MATHLW64, lw64Add32_SAFE_overflow,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Add32_SAFE correctly detects overflow.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Add32_SAFE_overflow]"))

/*!
 * @brief      Test the lw64Add32_SAFE function in an overflow condition
 *
 * @details    The test shall ilwoke lw64Add32_SAFE with large numbers as operands, to confirm that overflow is detected and halt is called
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Add32_SAFE_overflow)
{
    LwU64 op1 = 0xffffffffffffffff;
    LwU32 op2 = 2;

    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();

    lw64Add32_SAFE(&op1, op2);
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

UT_CASE_DEFINE(MATHLW64, lw64Add32_MOD,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Add32_MOD works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Add32_MOD]"))

/*!
 * @brief      Test the lw64Add32_MOD function
 *
 * @details    The test shall ilwoke lw64Add32_MOD:
 *             - With numbers that don't cause an overflow as operands, to confirm that the return value matches the sum
 *             - With operands that cause an overflow from the low 32 bits into the high 32 bits
 *             - With large numbers as operands, to confirm that the result wraps on overflow
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Add32_MOD)
{
    LwU64 op1 = 5;
    LwU32 op2 = 4;
    LwU64 expected1 = 9;

    LwU64 op3 = 0x500000003;
    LwU32 op4 = 0xFFFFFFFF;
    LwU64 expected2 = 0x600000002;

    LwU64 op5 = 0x4FFFFFFFF;
    LwU32 op6 = 0x3;
    LwU64 expected3 = 0x500000002;

    LwU64 op7 = 0xffffffffffffffff;
    LwU32 op8 = 2;
    LwU64 expected4 = 1;

    lw64Add32_MOD(&op1, op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&op1, &expected1));

    lw64Add32_MOD(&op3, op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&op3, &expected2));

    lw64Add32_MOD(&op5, op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&op5, &expected3));

    lw64Add32_MOD(&op7, op8);
    UT_ASSERT_TRUE(lw64CmpEQ(&op7, &expected4));
}

UT_CASE_DEFINE(MATHLW64, lw64Sub_SAFE,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Sub_SAFE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Sub_SAFE]"))

/*!
 * @brief      Test the lw64Sub_SAFE function
 *
 * @details    The test shall ilwoke lw64Sub_SAFE:
 *             - With numbers that don't cause an overflow as operands, to confirm that the return value matches the difference
 *             - With operands where the low 32 bits need to "borrow" a bit from the high 32 bits
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Sub_SAFE)
{
    LwU64 op1 = 5;
    LwU64 op2 = 4;
    LwU64 result1;
    LwU64 expected1 = 1;

    LwU64 op3 = 0x500000003;
    LwU64 op4 = 0x340000002;
    LwU64 result2;
    LwU64 expected2 = 0x1C0000001;

    lw64Sub_SAFE(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lw64Sub_SAFE(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));
}

UT_CASE_DEFINE(MATHLW64, lw64Sub_SAFE_underflow,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Sub_SAFE correctly detects overflow.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Sub_SAFE_underflow]"))

/*!
 * @brief      Test the lw64Sub_SAFE function in an underflow condition
 *
 * @details    The test shall ilwoke lw64Sub_SAFE with operands that yield a negative difference, to confirm that underflow is detected and halt is called
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Sub_SAFE_underflow)
{
    LwU64 op1 = 1;
    LwU64 op2 = 2;
    LwU64 result;

    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();

    lw64Sub_SAFE(&result, &op1, &op2);
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

UT_CASE_DEFINE(MATHLW64, lw64Sub_MOD,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Sub_MOD works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Sub_MOD]"))

/*!
 * @brief      Test the lw64Sub_MOD function
 *
 * @details    The test shall ilwoke lw64Sub_MOD:
 *             - With numbers that don't cause an underflow as operands, to confirm that the return value matches the difference
 *             - With operands where the low 32 bits need to "borrow" a bit from the high 32 bits
 *             - With operands that yield a negative difference, to confirm that the result wraps on underflow
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Sub_MOD)
{
    LwU64 op1 = 5;
    LwU64 op2 = 4;
    LwU64 result1;
    LwU64 expected1 = 1;

    LwU64 op3 = 0x500000003;
    LwU64 op4 = 0x340000002;
    LwU64 result2;
    LwU64 expected2 = 0x1C0000001;

    LwU64 op5 = 1;
    LwU64 op6 = 2;
    LwU64 result3;
    LwU64 expected3 = 0xffffffffffffffff;

    lw64Sub_MOD(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lw64Sub_MOD(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));

    lw64Sub_MOD(&result3, &op5, &op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&result3, &expected3));
}

UT_CASE_DEFINE(MATHLW64, lwU64Mul_SAFE,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64Mul_SAFE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64Mul_SAFE]"))

/*!
 * @brief      Test the lwU64Mul_SAFE function
 *
 * @details    The test shall ilwoke lwU64Mul_SAFE:
 *             - With numbers that don't cause an overflow as operands, to confirm that the return value matches the product
 *             - With operands that cause an overflow from the low 32 bits into the high 32 bits
 *             - With large numbers as operands, to confirm that overflow is detected and halt is called
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64Mul_SAFE)
{
    LwU64 op1 = 5;
    LwU64 op2 = 4;
    LwU64 result1;
    LwU64 expected1 = 20;

    LwU64 op3 = 0x590000006;
    LwU64 op4 = 4;
    LwU64 result2;
    LwU64 expected2 = 0x1640000018;

    LwU64 op5 = 0x590000006;
    LwU64 op6 = 4;
    LwU64 result3;
    LwU64 expected3 = 0x1640000018;

    LwU64 op7 = 0x100000000;
    LwU64 op8 = 0x100000000;
    LwU64 result4;

    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();

    lwU64Mul_SAFE(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lwU64Mul_SAFE(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));

    lwU64Mul_SAFE(&result3, &op5, &op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&result3, &expected3));

    lwU64Mul_SAFE(&result4, &op7, &op8);
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}


UT_CASE_DEFINE(MATHLW64, lw64Mul_SAFE_overflow,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Mul_SAFE correctly detects overflow.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Mul_SAFE_overflow]"))

/*!
 * @brief      Test the lw64Mul_SAFE function in an overflow condition
 *
 * @details    The test shall ilwoke lw64Mul_SAFE with large operands, to confirm that overflow is detected and halt is called
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Mul_SAFE_overflow)
{
    LwU64 op1 = 0xffffffffffffffff;
    LwU64 op2 = 2;
    LwU64 result;

    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();

    lwU64Mul_SAFE(&result, &op1, &op2);
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

UT_CASE_DEFINE(MATHLW64, lwU64Mul_MOD,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64Mul_MOD works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64Mul_MOD]"))

/*!
 * @brief      Test the lwU64Mul_MOD function
 *
 * @details    The test shall ilwoke lwU64Mul_MOD:
 *             - With numbers that don't cause an overflow as operands, to confirm that the return value matches the product
 *             - With operands that cause an overflow from the low 32 bits into the high 32 bits
 *             - With large numbers as operands, to confirm that the result wraps on overflow
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64Mul_MOD)
{
    LwU64 op1 = 5;
    LwU64 op2 = 4;
    LwU64 result1;
    LwU64 expected1 = 20;

    LwU64 op3 = 0x590000006;
    LwU64 op4 = 4;
    LwU64 result2;
    LwU64 expected2 = 0x1640000018;

    LwU64 op5 = 0x590000006;
    LwU64 op6 = 4;
    LwU64 result3;
    LwU64 expected3 = 0x1640000018;

    LwU64 op7 = 0xffffffffffffffff;
    LwU64 op8 = 2;
    LwU64 result4;
    LwU64 expected4 = 0xfffffffffffffffe;

    lwU64Mul_MOD(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lwU64Mul_MOD(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));

    lwU64Mul_MOD(&result3, &op5, &op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&result3, &expected3));

    lwU64Mul_MOD(&result4, &op7, &op8);
    UT_ASSERT_TRUE(lw64CmpEQ(&result4, &expected4));
}

UT_CASE_DEFINE(MATHLW64, lwU64Div_FUNC,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64Div_FUNC works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64Div_FUNC]"))

/*!
 * @brief      Test the lwU64Div_FUNC function
 *
 * @details    The test shall ilwoke lwU64Div_FUNC:
 *             - With operands that yield an integer quotient, to confirm that the return value matches the quotient
 *             - With operands that yield a non-integer quotient, to confirm that truncation is handled correctly
 *             - With large operands (non-zero high 32 bits)
 *             - With a dividend of zero
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64Div_FUNC)
{
    LwU64 op1 = 6;
    LwU64 op2 = 2;
    LwU64 result1;
    LwU64 expected1 = 3;

    LwU64 op3 = 5;
    LwU64 op4 = 2;
    LwU64 result2;
    LwU64 expected2 = 2;

    LwU64 op5 = 0x1234567890abcdef;
    LwU64 op6 = 0xaabbccddee;
    LwU64 result3;
    LwU64 expected3 = 0x1b4bc4;

    LwU64 op7 = 5;
    LwU64 op8 = 0;
    LwU64 result4;

    lwU64Div_FUNC(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lwU64Div_FUNC(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));

    lwU64Div_FUNC(&result3, &op5, &op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&result3, &expected3));

    UT_ASSERT_EQUAL_UINT(0, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());

    lwU64Div_FUNC(&result4, &op7, &op8);
    UT_ASSERT_EQUAL_UINT(1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

UT_CASE_DEFINE(MATHLW64, lwU64DivRounded_FUNC,
  UT_CASE_SET_DESCRIPTION("Ensure lwU64DivRounded_FUNC works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lwU64DivRounded_FUNC]"))

/*!
 * @brief      Test the lwU64DivRounded_FUNC function
 *
 * @details    The test shall ilwoke lwU64DivRounded_FUNC:
 *             - With operands that yield an integer quotient, to confirm that the return value matches the quotient
 *             - With operands that yield a non-integer quotient, to confirm that rounding down is handled correctly
 *             - With operands that yield a non-integer quotient, to confirm that rounding up is handled correctly
 *             - With large operands (non-zero high 32 bits), where the result must be rounded down
 *             - With large operands (non-zero high 32 bits), where the result must be rounded up
 *             - With operands tat cause an overflow
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lwU64DivRounded_FUNC)
{
    LwU64 op1 = 6;
    LwU64 op2 = 2;
    LwU64 result1;
    LwU64 expected1 = 3;

    LwU64 op3 = 5;
    LwU64 op4 = 4;
    LwU64 result2;
    LwU64 expected2 = 1;

    LwU64 op5 = 5;
    LwU64 op6 = 3;
    LwU64 result3;
    LwU64 expected3 = 2;

    LwU64 op7 = 1311768467294899695LL;
    LwU64 op8 = 733295000000LL;
    LwU64 result4;
    LwU64 expected4 = 1788869LL;

    LwU64 op9 = 1311768467294899695LL;
    LwU64 op10 = 733295200000LL;
    LwU64 result5;
    LwU64 expected5 = 1788868LL;

    LwU64 op11 = 0xFFFFFFFFFFFFFFFFLL;
    LwU64 op12 = 2LL;
    LwU64 result6;
    LwU64 expected6 = 0x7FFFFFFFFFFFFFFFLL;

    lwU64DivRounded_FUNC(&result1, &op1, &op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));

    lwU64DivRounded_FUNC(&result2, &op3, &op4);
    UT_ASSERT_TRUE(lw64CmpEQ(&result2, &expected2));

    lwU64DivRounded_FUNC(&result3, &op5, &op6);
    UT_ASSERT_TRUE(lw64CmpEQ(&result3, &expected3));

    lwU64DivRounded_FUNC(&result4, &op7, &op8);
    UT_ASSERT_TRUE(lw64CmpEQ(&result4, &expected4));

    lwU64DivRounded_FUNC(&result5, &op9, &op10);
    UT_ASSERT_TRUE(lw64CmpEQ(&result5, &expected5));

    lwU64DivRounded_FUNC(&result6, &op11, &op12);
    UT_ASSERT_TRUE(lw64CmpEQ(&result6, &expected6));
}

UT_CASE_DEFINE(MATHLW64, lw64Lsl_FUNC,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Lsl_FUNC works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Lsl_FUNC]"))

/*!
 * @brief      Test the lw64Lsl_FUNC function
 *
 * @details    The test shall ilwoke lw64Lsl_FUNC and confirm that the result matches the shifted value.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Lsl_FUNC)
{
    LwU64 op1 = 0x1111111111111111;
    LwU8 op2 = 2;
    LwU64 result1;
    LwU64 expected1 = 0x4444444444444444;

    lw64Lsl_FUNC(&result1, &op1, op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));
}

UT_CASE_DEFINE(MATHLW64, lw64Lsr_FUNC,
  UT_CASE_SET_DESCRIPTION("Ensure lw64Lsr_FUNC works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLW64_lw64Lsr_FUNC]"))

/*!
 * @brief      Test the lw64Lsr_FUNC function
 *
 * @details    The test shall ilwoke lw64Lsr_FUNC and confirm that the result matches the shifted value.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLW64, lw64Lsr_FUNC)
{
    LwU64 op1 = 0x8888888888888888;
    LwU8 op2 = 2;
    LwU64 result1;
    LwU64 expected1 = 0x2222222222222222;

    lw64Lsr_FUNC(&result1, &op1, op2);
    UT_ASSERT_TRUE(lw64CmpEQ(&result1, &expected1));
}

