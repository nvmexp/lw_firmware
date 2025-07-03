/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file lib_lwf32-test.c
 *
 * @details    This file contains all the unit tests for the Unit Math LwF32 functions.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include <ut/ut.h>
#include "ut/ut_case.h"
#include "ut/ut_assert.h"
#include "ut/ut_fake_mem.h"

/* ------------------------ Application Includes ---------------------------- */
#include "lib_lwf32.h"

/* -------------------------------------------------------- */
/*!
 * Definition of the Unit Math LwF32 functions Suite.
 */
UT_SUITE_DEFINE(MATHLWF32,
                UT_SUITE_SET_COMPONENT("Unit Math LwF32 functions")
                UT_SUITE_SET_DESCRIPTION("Unit tests for the Unit Math LwF32 functions")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))


UT_CASE_DEFINE(MATHLWF32, lwF32CmpEQ,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32CmpEQ works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32CmpEQ]"))

/*!
 * @brief      Test the lwF32CmpEQ macro
 *
 * @details    The test shall ilwoke lwF32CmpEQ:
 *             - where operand1 is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is not equal to operand2 (we expect a return value of LW_FALSE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32CmpEQ)
{
    LwF32 op1 = 4.0;
    LwF32 op2 = 4.0;
    LwF32 op3 = 5.0;

    UT_ASSERT_TRUE(lwF32CmpEQ(op1, op2));
    UT_ASSERT_FALSE(lwF32CmpEQ(op1, op3));
}

UT_CASE_DEFINE(MATHLWF32, lwF32CmpNE,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32CmpNE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32CmpNE]"))

/*!
 * @brief      Test the lwF32CmpNE macro
 *
 * @details    The test shall ilwoke lwF32CmpNE:
 *             - where operand1 is not equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_FALSE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32CmpNE)
{
    LwF32 op1 = 4.0;
    LwF32 op2 = 5.0;
    LwF32 op3 = 4.0;

    UT_ASSERT_TRUE(lwF32CmpNE(op1, op2));
    UT_ASSERT_FALSE(lwF32CmpNE(op1, op3));
}

UT_CASE_DEFINE(MATHLWF32, lwF32CmpLT,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32CmpLT works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32CmpLT]"))

/*!
 * @brief      Test the lwF32CmpLT macro
 *
 * @details    The test shall ilwoke lwF32CmpLT:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_FALSE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32CmpLT)
{
    LwF32 op1 = 4.0;
    LwF32 op2 = 5.0;
    LwF32 op3 = 4.0;
    LwF32 op4 = 3.0;

    UT_ASSERT_TRUE(lwF32CmpLT(op1, op2));
    UT_ASSERT_FALSE(lwF32CmpLT(op1, op3));
    UT_ASSERT_FALSE(lwF32CmpLT(op1, op4));
}

UT_CASE_DEFINE(MATHLWF32, lwF32CmpLE,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32CmpLE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32CmpLE]"))

/*!
 * @brief      Test the lwF32CmpLE macro
 *
 * @details    The test shall ilwoke lwF32CmpLE:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_FALSE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32CmpLE)
{
    LwF32 op1 = 4.0;
    LwF32 op2 = 5.0;
    LwF32 op3 = 4.0;
    LwF32 op4 = 3.0;

    UT_ASSERT_TRUE(lwF32CmpLE(op1, op2));
    UT_ASSERT_TRUE(lwF32CmpLE(op1, op3));
    UT_ASSERT_FALSE(lwF32CmpLE(op1, op4));
}

UT_CASE_DEFINE(MATHLWF32, lwF32CmpGT,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32CmpGT works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32CmpGT]"))

/*!
 * @brief      Test the lwF32CmpGT macro
 *
 * @details    The test shall ilwoke lwF32CmpGT:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_TRUE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32CmpGT)
{
    LwF32 op1 = 4.0;
    LwF32 op2 = 5.0;
    LwF32 op3 = 4.0;
    LwF32 op4 = 3.0;

    UT_ASSERT_FALSE(lwF32CmpGT(op1, op2));
    UT_ASSERT_FALSE(lwF32CmpGT(op1, op3));
    UT_ASSERT_TRUE(lwF32CmpGT(op1, op4));
}

UT_CASE_DEFINE(MATHLWF32, lwF32CmpGE,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32CmpGE works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32CmpGE]"))
/*!
 * @brief      Test the lwF32CmpGE macro
 *
 * @details    The test shall ilwoke lwF32CmpGE:
 *             - where operand1 is less than to operand2 (we expect a return value of LW_FALSE)
 *             - where operand1 is equal to operand2 (we expect a return value of LW_TRUE)
 *             - where operand1 is greater than operand2 (we expect a return value of LW_TRUE)
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32CmpGE)
{
    LwF32 op1 = 4.0;
    LwF32 op2 = 5.0;
    LwF32 op3 = 4.0;
    LwF32 op4 = 3.0;

    UT_ASSERT_FALSE(lwF32CmpGE(op1, op2));
    UT_ASSERT_TRUE(lwF32CmpGE(op1, op3));
    UT_ASSERT_TRUE(lwF32CmpGE(op1, op4));
}


UT_CASE_DEFINE(MATHLWF32, lwF32Add,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32Add works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32Add]"))

/*!
 * @brief      Test the lwF32Add function
 *
 * @details    The test shall ilwoke lwF32Add and confirm that the result is equal to the sum of the two given operands.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32Add)
{
    LwF32 op1 = 1.2;
    LwF32 op2 = 2.3;
    LwF32 result = lwF32Add(op1, op2);
    LwF32 expectedresult = 3.5;

    UT_ASSERT(lwF32CmpEQ(result, expectedresult));
}

UT_CASE_DEFINE(MATHLWF32, lwF32Sub,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32Sub works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32Sub]"))

/*!
 * @brief      Test the lwF32Sub function
 *
 * @details    The test shall ilwoke lwF32Sub and confirm that the result is equal to the difference of the two given operands.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32Sub)
{
    LwF32 op1 = 1.2;
    LwF32 op2 = 2.4;
    LwF32 result = lwF32Sub(op1, op2);
    LwF32 expectedresult = -1.2;

    UT_ASSERT(lwF32CmpEQ(result, expectedresult));
}

UT_CASE_DEFINE(MATHLWF32, lwF32Mul,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32Mul works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32Mul]"))

/*!
 * @brief      Test the lwF32Mul function
 *
 * @details    The test shall ilwoke lwF32Mul and confirm that the result is equal to the product of the two given operands.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32Mul)
{
    LwF32 op1 = 1.2;
    LwF32 op2 = 2.3;
    LwF32 result = lwF32Mul(op1, op2);
    LwF32 expectedresult = 2.76;

    UT_ASSERT(lwF32CmpEQ(result, expectedresult));
}

UT_CASE_DEFINE(MATHLWF32, lwF32Div,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32Div works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32Div]"))

/*!
 * @brief      Test the lwF32Div function
 *
 * @details    The test shall ilwoke lwF32Div and confirm that the result is equal to the quotient of the two given operands.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32Div)
{
    LwF32 op1 = 3.0;
    LwF32 op2 = 1.2;
    LwF32 result = lwF32Div(op1, op2);
    LwF32 expectedresult = 2.5;

    UT_ASSERT(lwF32CmpEQ(result, expectedresult));
}

UT_CASE_DEFINE(MATHLWF32, LW_TYPES_F32_TO_UFXP_X_Y_ROUND,
  UT_CASE_SET_DESCRIPTION("Ensure LW_TYPES_F32_TO_UFXP_X_Y_ROUND works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_LW_TYPES_F32_TO_UFXP_X_Y_ROUND]"))

/*!
 * @brief      Test the LW_TYPES_F32_TO_UFXP_X_Y_ROUND function
 *
 * @details    The test shall ilwoke LW_TYPES_F32_TO_UFXP_X_Y_ROUND in two scenarios:
 *             - The LwF32 must be rounded up for its fixed-point colwersion
 *             - The LwF32 must be rounded down for its fixed-point colwersion
 *
 *             Both scenarios will use a 28.4 fixed-point format (4 fractional bits).
 *             In the round down scenario, we will use the floating point number 5.0626 as input.
 *             The expected output value is callwlated via the LW_TYPES_U32_TO_UFXP_X_Y_SCALED macro with the following arguments:
 *                LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4, 81, 16);
 *                This represents a fixed-point value of 81/16 = 5.0625. We expect our input (5.0626) to be rounded down to this.
 * 
 *             In the round up scenario, we will use the floating point number 5.1249 as input.
 *             The expected output value is callwlated via the LW_TYPES_U32_TO_UFXP_X_Y_SCALED macro with the following arguments:
 *                LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4, 82, 16);
 *                This represents a fixed-point value of 82/16 = 5.125. We expect our input (5.1249) to be rounded up to this.
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, LW_TYPES_F32_TO_UFXP_X_Y_ROUND)
{
    LwF32 op1 = 5.0626;
    // This should get rounded down to 5.0625 (4 fractional bits)
    LwUFXP28_4 result1 = LW_TYPES_F32_TO_UFXP_X_Y_ROUND(28, 4, op1);
    // 5.0625 = 81 / 16
    LwUFXP28_4 expectedresult1 = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4, 81, 16);

    LwF32 op2 = 5.1249;
    // This should get rounded up to 5.125 (4 fractional bits)
    LwUFXP28_4 result2 = LW_TYPES_F32_TO_UFXP_X_Y_ROUND(28, 4, op2);
    // 5.125 = 82 / 16
    LwUFXP28_4 expectedresult2 = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4, 82, 16);

    UT_ASSERT(lwF32CmpEQ(result1, expectedresult1));
    UT_ASSERT(lwF32CmpEQ(result2, expectedresult2));
}

UT_CASE_DEFINE(MATHLWF32, LW_TYPES_UFXP_X_Y_TO_F32,
  UT_CASE_SET_DESCRIPTION("Ensure LW_TYPES_UFXP_X_Y_TO_F32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_LW_TYPES_UFXP_X_Y_TO_F32]"))

/*!
 * @brief      Test the LW_TYPES_UFXP_X_Y_TO_F32 function
 *
 * @details    The test shall ilwoke LW_TYPES_UFXP_X_Y_TO_F32 and ensure that the resulting LwF32 equals its Fixed-point counterpart
 *             The test generates the input via the LW_TYPES_U32_TO_UFXP_X_Y_SCALED function:
 *                    LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4, 81, 16);
 *                    This represents a value of 81/16 = 5.0625.
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, LW_TYPES_UFXP_X_Y_TO_F32)
{
    // 5.0625 = 81 / 16
    LwUFXP28_4 op1 = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(28, 4, 81, 16);

    LwF32 result = LW_TYPES_UFXP_X_Y_TO_F32(28, 4, op1);
    LwF32 expectedresult = 5.0625;

    UT_ASSERT(lwF32CmpEQ(result, expectedresult));
}

UT_CASE_DEFINE(MATHLWF32, lwF32ColwertFromU32,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32ColwertFromU32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32ColwertFromU32]"))

/*!
 * @brief      Test the lwF32ColwertFromU32 function
 *
 * @details    The test shall ilwoke lwF32ColwertFromU32 with several inputs and ensure that the colwerted value is correct.
 *             - scenario 1: Call lwF32ColwertFromU32 with the input 5. Expected result: 5.0
 *             - scenario 2: Call lwF32ColwertFromU32 with the input 0xFFFFFFFF. Expected result: 4294967295.0
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32ColwertFromU32)
{

    LwU32 op1 = 5;
    LwF32 result1 = lwF32ColwertFromU32(op1);
    LwF32 expectedresult1 = 5.0;

    //boundary value
    LwU32 op2 = 0xFFFFFFFF;
    LwF32 result2 = lwF32ColwertFromU32(op2);
    LwF32 expectedresult2 = 4294967295.0;

    UT_ASSERT(lwF32CmpEQ(result1, expectedresult1));
    UT_ASSERT(lwF32CmpEQ(result2, expectedresult2));
}

UT_CASE_DEFINE(MATHLWF32, lwF32ColwertFromS32,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32ColwertFromS32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32ColwertFromS32]"))

/*!
 * @brief      Test the lwF32ColwertFromS32 function
 *
 * @details    The test shall ilwoke lwF32ColwertFromS32 with several inputs and ensure that the colwerted value is correct
 *             - scenario 1: Call lwF32ColwertFromS32 with the input 5. Expected result: 5.0
 *             - scenario 2: Call lwF32ColwertFromS32 with the input -2147483648. Expected result: -2147483648.0
 *             - scenario 3: Call lwF32ColwertFromS32 with the input 2147483647. Expected result: 2147483647.0
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32ColwertFromS32)
{

    LwS32 op1 = 5;
    LwF32 result1 = lwF32ColwertFromS32(op1);
    LwF32 expectedresult1 = 5.0;

    //boundary value
    LwU32 op2 = -2147483648;
    LwF32 result2 = lwF32ColwertFromS32(op2);
    LwF32 expectedresult2 = -2147483648.0;

    //boundary value
    LwU32 op3 = 2147483647;
    LwF32 result3 = lwF32ColwertFromS32(op3);
    LwF32 expectedresult3 = 2147483647.0;

    UT_ASSERT(lwF32CmpEQ(result1, expectedresult1));
    UT_ASSERT(lwF32CmpEQ(result2, expectedresult2));
    UT_ASSERT(lwF32CmpEQ(result3, expectedresult3));
}

UT_CASE_DEFINE(MATHLWF32, lwF32ColwertToU32,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32ColwertToU32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32ColwertToU32]"))

/*!
 * @brief      Test the lwF32ColwertToU32 function
 *
 * @details    The test shall ilwoke lwF32ColwertToU32 with several inputs and ensure that the colwerted value is correct
 *             - scenario 1: Call lwF32ColwertFromS32 with the input 5.0. Expected result: 5
 *             - scenario 2: Call lwF32ColwertFromS32 with the input 4294967295.0. Expected result: 4294967295
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32ColwertToU32)
{

    LwF32 op1 = 5.6;
    LwU32 result1 = lwF32ColwertToU32(op1);
    LwU32 expectedresult1 = 5;

    //boundary value
    LwF32 op2 = 4294967295.0;
    LwU32 result2 = lwF32ColwertToU32(op2);
    LwU32 expectedresult2 = 4294967295;

    UT_ASSERT_EQUAL_UINT(result1, expectedresult1);
    UT_ASSERT_EQUAL_UINT(result2, expectedresult2);
}

UT_CASE_DEFINE(MATHLWF32, lwF32ColwertToS32,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32ColwertToS32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32ColwertToS32]"))

/*!
 * @brief      Test the lwF32ColwertToS32 function
 *
 * @details    The test shall ilwoke lwF32ColwertToS32 with several inputs and ensure that the colwerted value is correct
 *             - scenario 1: Call lwF32ColwertFromS32 with the input 5.0. Expected result: 5
 *             - scenario 2: Call lwF32ColwertFromS32 with the input -2147483648.0. Expected result: -2147483648
 *             - scenario 3: Call lwF32ColwertFromS32 with the input 2147483647.0. Expected result: 2147483647
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32ColwertToS32)
{

    LwF32 op1 = -5.6;
    LwS32 result1 = lwF32ColwertToS32(op1);
    LwS32 expectedresult1 = -5;

    //boundary value
    LwF32 op2 = -2147483648.0;
    LwS32 result2 = lwF32ColwertToS32(op2);
    LwS32 expectedresult2 = -2147483648;

    //boundary value
    LwF32 op3 = 2147483647.0;
    LwS32 result3 = lwF32ColwertToS32(op3);
    LwS32 expectedresult3 = 2147483647;

    UT_ASSERT_EQUAL_INT(result1, expectedresult1);
    UT_ASSERT_EQUAL_INT(result2, expectedresult2);
    UT_ASSERT_EQUAL_INT(result3, expectedresult3);
}

UT_CASE_DEFINE(MATHLWF32, lwF32MapFromU32,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32MapFromU32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32MapFromU32]"))

/*!
 * @brief      Test the lwF32MapFromU32 function
 *
 * @details    The test shall ilwoke lwF32MapFromU32 and ensure that the mapped value is correct
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32MapFromU32)
{

    LwU32 op1 = 0x3f9e0419;
    LwF32 result1 = lwF32MapFromU32(&op1);
    LwF32 expectedresult1 = 1.2345f;

    UT_ASSERT(result1 == expectedresult1);
}

UT_CASE_DEFINE(MATHLWF32, lwF32MapToU32,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32MapFromU32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32MapToU32]"))

/*!
 * @brief      Test the lwF32MapToU32 function
 *
 * @details    The test shall ilwoke lwF32MapToU32 and ensure that the mapped value is correct
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32MapToU32)
{

    LwF32 op1 = 1.2345f;
    LwU32 result1 = lwF32MapToU32(&op1);
    LwU32 expectedresult1 = 0x3f9e0419;

    UT_ASSERT_EQUAL_UINT(result1, expectedresult1);
}


UT_CASE_DEFINE(MATHLWF32, lwF32ColwertToU64,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32ColwertToU64 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32ColwertToU64]"))

/*!
 * @brief      Test the lwF32ColwertToU64 function
 *
 * @details    The test shall ilwoke lwF32ColwertToU64 with several inputs and ensure that the colwerted value is correct
 *             - scenario 1: Call lwF32ColwertFromU64 with the input 5.0. Expected result: 5
 *             - scenario 2: Call lwF32ColwertFromU64 with the input 4294967296.0. Expected result: 4294967296
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32ColwertToU64)
{

    LwF32 op1 = 5.0;
    LwU64 result1 = lwF32ColwertToU64(op1);
    LwU64 expectedresult1 = 5;

    //boundary value
    LwF32 op2 = 4294967296.0;
    LwU64 result2 = lwF32ColwertToU64(op2);
    LwU64 expectedresult2 = 4294967296;

    UT_ASSERT_EQUAL_UINT(result1, expectedresult1);
    UT_ASSERT_EQUAL_UINT(result2, expectedresult2);
}

UT_CASE_DEFINE(MATHLWF32, lwF32ColwertFromU64,
  UT_CASE_SET_DESCRIPTION("Ensure lwF32ColwertFrom64 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[MATHLWF32_lwF32ColwertFromU64]"))

/*!
 * @brief      Test the lwF32ColwertFromU64 function
 *
 * @details    The test shall ilwoke lwF32ColwertFromU64 with several inputs and ensure that the colwerted value is correct
 *             - scenario 1: Call lwF32ColwertFromU64 with the input 5. Expected result: 5.0
 *             - scenario 2: Call lwF32ColwertFromU64 with the input 4294967296. Expected result: 4294967296.0
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(MATHLWF32, lwF32ColwertFromU64)
{

    LwU64 op1 = 5;
    LwF32 result1 = lwF32ColwertToU64(op1);
    LwF32 expectedresult1 = 5.0;

    //boundary value
    LwU64 op2 = 4294967296;
    LwF32 result2 = lwF32ColwertToU64(op2);
    LwF32 expectedresult2 = 4294967296.0;

    UT_ASSERT_EQUAL_UINT(result1, expectedresult1);
    UT_ASSERT_EQUAL_UINT(result2, expectedresult2);
}