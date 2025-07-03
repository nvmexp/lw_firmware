/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file regmacros-test.c
 *
 * @details    This file contains all the unit tests for the Unit Registers regmacros functions.
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include <ut/ut.h>
#include "ut/ut_case.h"
#include "ut/ut_assert.h"
#include "ut/ut_fake_mem.h"
#include "regmock.h"
#include "csb.h"
#include "pmu_bar0.h"

/* ------------------------ Application Includes ---------------------------- */
#include "regmacros.h"

/* -------------------------------------------------------- */
/*!
 * Definition of the Unit Math LwF32 functions Suite.
 */
UT_SUITE_DEFINE(REGMACROS,
                UT_SUITE_SET_COMPONENT("Unit Registers regmacros functions")
                UT_SUITE_SET_DESCRIPTION("Unit tests for the Unit Registers regmacros functions")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))


#define LW_DUMMYDEVICE                                           0x005fffff:0x00400000 /* RW--D */
#define LW_DUMMYDEVICE_DUMMYREG                                             0x00400080 /* RW-4R */
#define LW_DUMMYDEVICE_DUMMYREG_DUMMYFIELD                                       19:16 /* RWEVF */
#define LW_DUMMYDEVICE_DUMMYREG_DUMMYFIELD_DUMMYVALUE                       0x00000009 /* RWE-V */
#define LW_DUMMYDEVICE_DUMMYREG2                                            0x00400084 /* RW-4R */
#define LW_DUMMYDEVICE_DUMMYREG2_DUMMYFIELD                                      19:16 /* RWEVF */
#define LW_DUMMYDEVICE_DUMMYREG2_DUMMYFIELD_DUMMYVALUE                      0x00000009 /* RWE-V */

#define LW_DUMMYDEVICE_DUMMYIDXREG(i)                             (0x00404200+((i)*4)) /* RW-4A */
#define LW_DUMMYDEVICE_DUMMYIDXREG_DUMMYFIELD                                    19:16 /* RWEVF */
#define LW_DUMMYDEVICE_DUMMYIDXREG_DUMMYFIELD_DUMMYVALUE                    0x00000009 /* RWE-V */

UT_CASE_DEFINE(REGMACROS, REG_FLD_WR_DRF_DEF,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_WR_DRF_DEF works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pretest for the REG_FLD_WR_DRF_DEF, REG_FLD_WR_DRF_DEF_STALL, REG_FLD_WR_DRF_NUM, REG_WR_NUM_largevalue, REG_FLD_WR_DRF_NUM_STALL, and REG_FLD_RD_DRF tests
 * 
 * @details    Seed LW_DUMMYDEVICE_DUMMYREG with an initial value.
 */
static void REG_WR_DEF_pretest(LwU32 initial)
{
    CSB_REG_WR32(LW_DUMMYDEVICE_DUMMYREG, initial);
}

/*!
 * @brief      Test the REG_FLD_WR_DRF_DEF macro
 *
 * @details    Write an initial value into the register. Then use REG_FLD_WR_DRF_DEF to write a field in this register
 *             Read back the register value to ensure that the field has been written and the remainder of the register
 *             is unchanged.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_WR_DRF_DEF)
{
    LwU32 result;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x12395678;

    REG_WR_DEF_pretest(initial);

    REG_FLD_WR_DRF_DEF(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD, _DUMMYVALUE);

    result = CSB_REG_RD32(LW_DUMMYDEVICE_DUMMYREG);

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_WR_DRF_DEF_STALL,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_WR_DRF_DEF_STALL works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_FLD_WR_DRF_DEF_STALL macro
 *
 * @details    Write an initial value into the register. Then use REG_FLD_WR_DRF_DEF_STALL to write a field in this register
 *             Read back the register value to ensure that the field has been written and the remainder of the register
 *             is unchanged.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_WR_DRF_DEF_STALL)
{
    LwU32 result;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x12395678;

    REG_WR_DEF_pretest(initial);

    REG_FLD_WR_DRF_DEF_STALL(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD, _DUMMYVALUE);

    result = CSB_REG_RD32(LW_DUMMYDEVICE_DUMMYREG);

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_WR_DRF_NUM,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_WR_DRF_NUM works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_FLD_WR_DRF_NUM macro
 *
 * @details    Write an initial value into the register. Then use REG_FLD_WR_DRF_NUM to write a field in this register
 *             Read back the register value to ensure that the field has been written and the remainder of the register
 *             is unchanged.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_WR_DRF_NUM)
{
    LwU32 result;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x12395678;

    REG_WR_DEF_pretest(initial);

    REG_FLD_WR_DRF_NUM(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD, 0x9);

    result = CSB_REG_RD32(LW_DUMMYDEVICE_DUMMYREG);

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_WR_NUM_largevalue,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_WR_DRF_NUM works correctly when receiving a value larger than the field.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_FLD_WR_DRF_NUM macro when receiving a value larger than the field.
 *
 * @details    Write an initial value into the register. Then use REG_FLD_WR_DRF_NUM to write a field in this register.
 *             Write in a value that is larger than the field it is intended for.
 *             Read back the register value to ensure that the field has been written with the portion of the value
 *             that could fit in the field, and that the remainder of the register is unchanged.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_WR_NUM_largevalue)
{
    LwU32 result;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x12395678;

    REG_WR_DEF_pretest(initial);

    REG_FLD_WR_DRF_NUM(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD, 0xA9);

    result = CSB_REG_RD32(LW_DUMMYDEVICE_DUMMYREG);

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_WR_DRF_NUM_STALL,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_WR_DRF_DEF_STALL works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_FLD_WR_DRF_NUM_STALL macro
 *
 * @details    Write an initial value into the register. Then use REG_FLD_WR_DRF_NUM_STALL to write a field in this register
 *             Read back the register value to ensure that the field has been written and the remainder of the register
 *             is unchanged.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_WR_DRF_NUM_STALL)
{
    LwU32 result;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x12395678;

    REG_WR_DEF_pretest(initial);

    REG_FLD_WR_DRF_NUM_STALL(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD, 0x9);

    result = CSB_REG_RD32(LW_DUMMYDEVICE_DUMMYREG);

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_RD_DRF,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_RD_DRF works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_FLD_RD_DRF macro
 *
 * @details    Write an initial value into the register. 
 *             Then read it with REG_FLD_RD_DRF and ensure that it returns the correct value for the specified field.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_RD_DRF)
{
    LwU32 result;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x4;

    REG_WR_DEF_pretest(initial);

    result = REG_FLD_RD_DRF(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD);

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_TEST_DRF_DEF,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_TEST_DRF_DEF works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pretest for the REG_FLD_TEST_DRF_DEF and REG_FLD_TEST_DRF_NUM test
 * 
 * @details    - seed LW_DUMMYDEVICE_DUMMYREG with a value which matches the constant we are testing against.
 *             - seed LW_DUMMYDEVICE_DUMMYREG2 with a value which doesn't match the constant we are testing against.
 */
static void REG_TEST_pretest(LwU32 seed1, LwU32 seed2)
{
    CSB_REG_WR32(LW_DUMMYDEVICE_DUMMYREG, seed1);
    CSB_REG_WR32(LW_DUMMYDEVICE_DUMMYREG2, seed2);
}

/*!
 * @brief      Test the REG_FLD_TEST_DRF_DEF macro
 *
 * @details    call REG_FLD_TEST_DRF_DEF in two scenarios:
 *              - LW_DUMMYDEVICE_DUMMYREG has been seeded with a value which matches the constant we are testing against.
 *                We expect a return value of LW_TRUE
 *              - LW_DUMMYDEVICE_DUMMYREG2 has been seeded with a value which doesn't match the constant we are testing against.
 *                We expect a return value of LW_TRUE
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_TEST_DRF_DEF)
{
    REG_TEST_pretest(0x12395678, 0x12345678);

    UT_ASSERT_TRUE(REG_FLD_TEST_DRF_DEF(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD, _DUMMYVALUE));
    UT_ASSERT_FALSE(REG_FLD_TEST_DRF_DEF(CSB, _DUMMYDEVICE, _DUMMYREG2, _DUMMYFIELD, _DUMMYVALUE));
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_TEST_DRF_NUM,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_TEST_DRF_NUM works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_FLD_TEST_DRF_NUM macro
 *
 * @details    call REG_FLD_TEST_DRF_NUM in two scenarios:
 *              - the register has been seeded with a value which matches the constant we are testing against.
 *                We expect a return value of LW_TRUE
 *              - the register has been seeded with a value which doesn't match the constant we are testing against.
 *                We expect a return value of LW_TRUE
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_TEST_DRF_NUM)
{
    REG_TEST_pretest(0x12395678, 0x12345678);
    UT_ASSERT_TRUE(REG_FLD_TEST_DRF_NUM(CSB, _DUMMYDEVICE, _DUMMYREG, _DUMMYFIELD, 0x9));
    UT_ASSERT_FALSE(REG_FLD_TEST_DRF_NUM(CSB, _DUMMYDEVICE, _DUMMYREG2, _DUMMYFIELD, 0x9));
}

UT_CASE_DEFINE(REGMACROS, REG_FLC_IDX_TEST_DRF_DEF,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLC_IDX_TEST_DRF_DEF works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pretest for the REG_FLC_IDX_TEST_DRF_DEF test
 * 
 * @details    - seed LW_DUMMYDEVICE_DUMMYIDXREG(0) with a value which matches the constant we are testing against.
 *             - seed LW_DUMMYDEVICE_DUMMYIDXREG(1) with a value which doesn't match the constant we are testing against.
 */
static void REG_IDX_TEST_DEF_pretest(LwU32 seed1, LwU32 seed2)
{
    CSB_REG_WR32(LW_DUMMYDEVICE_DUMMYIDXREG(0), seed1);
    CSB_REG_WR32(LW_DUMMYDEVICE_DUMMYIDXREG(1), seed2);
}

/*!
 * @brief      Test the REG_FLC_IDX_TEST_DRF_DEF macro
 *
 * @details    call REG_FLC_IDX_TEST_DRF_DEF in two scenarios:
 *              - LW_DUMMYDEVICE_DUMMYIDXREG(0) has been seeded with a value which matches the constant we are testing against.
 *                We expect a return value of LW_TRUE
 *              - LW_DUMMYDEVICE_DUMMYIDXREG(1) has been seeded with a value which doesn't match the constant we are testing against.
 *                We expect a return value of LW_TRUE
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLC_IDX_TEST_DRF_DEF)
{
    REG_IDX_TEST_DEF_pretest(0x12395678, 0x12345678);

    UT_ASSERT_TRUE(REG_FLD_IDX_TEST_DRF_DEF(CSB, _DUMMYDEVICE, _DUMMYIDXREG, 0, _DUMMYFIELD, _DUMMYVALUE));
    UT_ASSERT_FALSE(REG_FLD_IDX_TEST_DRF_DEF(CSB, _DUMMYDEVICE, _DUMMYIDXREG, 1, _DUMMYFIELD, _DUMMYVALUE));
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_IDX_RD_DRF,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_IDX_RD_DRF works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pretest for the REG_FLD_IDX_RD_DRF and REG_FLD_IDX_WR_DRF_DEF tests
 * 
 * @details    Seed LW_DUMMYDEVICE_DUMMYIDXREG(index) with a value which matches the constant we are testing against.
 */
static void REG_IDX_RD_DRF_pretest(LwU32 index, LwU32 value)
{
    CSB_REG_WR32(LW_DUMMYDEVICE_DUMMYIDXREG(index), value);
}

/*!
 * @brief      Test the REG_FLD_IDX_RD_DRF macro
 *
 * @details    Seed an index register with a known seed value. Read a field in that register with REG_FLD_IDX_RD_DRF.
 *             ensure that the returned value is equal to the value of that field in the seed value.
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_IDX_RD_DRF)
{
    LwU32 result;
    LwU32 index = 2;
    LwU32 expected = 0x9;
    LwU32 seed = 0x12395678;

    REG_IDX_RD_DRF_pretest(index, seed);

    result = REG_FLD_IDX_RD_DRF(CSB, _DUMMYDEVICE, _DUMMYIDXREG, index, _DUMMYFIELD);

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_IDX_WR_DRF_DEF,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_IDX_WR_DRF_DEF works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_FLD_IDX_WR_DRF_DEF macro
 *
 * @details    Write an initial value into the register. Then use REG_FLD_IDX_WR_DRF_DEF to write a field in this register
 *             Read back the register value to ensure that the field has been written and the remainder of the register
 *             is unchanged.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_IDX_WR_DRF_DEF)
{
    LwU32 result;
    LwU32 index = 3;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x12395678;

    REG_IDX_RD_DRF_pretest(index, initial);

    REG_FLD_IDX_WR_DRF_DEF(CSB, _DUMMYDEVICE, _DUMMYIDXREG, index, _DUMMYFIELD, _DUMMYVALUE);

    result = CSB_REG_RD32(LW_DUMMYDEVICE_DUMMYIDXREG(index));

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_FLD_IDX_WR_DRF_NUM,
  UT_CASE_SET_DESCRIPTION("Ensure REG_FLD_IDX_WR_DRF_NUM works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pretest for the REG_FLD_IDX_WR_DRF_DEF test
 * 
 * @details    Seed LW_DUMMYDEVICE_DUMMYIDXREG(index) with a value which matches the constant we are testing against.
 */
static void REG_IDX_WR_DRF_NUM_pretest(LwU32 index, LwU32 value)
{
    CSB_REG_WR32(LW_DUMMYDEVICE_DUMMYIDXREG(index), value);
}

/*!
 * @brief      Test the REG_FLD_IDX_WR_DRF_NUM macro
 *
 * @details    Write an initial value into the register. Then use REG_FLD_IDX_WR_DRF_NUM to write a field in this register
 *             Read back the register value to ensure that the field has been written and the remainder of the register
 *             is unchanged.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_FLD_IDX_WR_DRF_NUM)
{
    LwU32 result;
    LwU32 index = 2;
    LwU32 initial = 0x12345678;
    LwU32 expected = 0x12395678;

    REG_IDX_WR_DRF_NUM_pretest(index, initial);

    REG_FLD_IDX_WR_DRF_NUM(CSB, _DUMMYDEVICE, _DUMMYIDXREG, index, _DUMMYFIELD, 0x9);

    result = CSB_REG_RD32(LW_DUMMYDEVICE_DUMMYIDXREG(index));

    UT_ASSERT_EQUAL_UINT(result, expected);
}

UT_CASE_DEFINE(REGMACROS, REG_RD32,
  UT_CASE_SET_DESCRIPTION("Ensure REG_RD32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Pretest for the REG_RD32 test
 * 
 * @details    Seed three different known values to the same register address on 3 different busses.
 */
static void REG_RD32_pretest(LwU32 reg, LwU32 value_csb, LwU32 value_bar0, LwU32 value_fecs)
{
    CSB_REG_WR32(reg, value_csb);
    BAR0_REG_WR32(reg, value_bar0);
    FECS_REG_WR32(reg, value_fecs);
}

/*!
 * @brief      Test the REG_RD32 macros
 *
 * @details    Seed three different known values to the same register address on 3 different busses.
 *             read them back with REG_RD32 and make sure that they return the correct values and have not trampled each other.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_RD32)
{
    LwU32 value_csb = 0xcccccccc;
    LwU32 value_bar0 = 0xbbbbbbbb;
    LwU32 value_fecs = 0xfec5fec5;
    LwU32 reg = 0x0000abcd;

    LwU32 read_csb;
    LwU32 read_bar0;
    LwU32 read_fecs;

    REG_RD32_pretest(reg, value_csb, value_bar0, value_fecs);

    read_csb = REG_RD32(CSB,  reg);
    read_bar0 = REG_RD32(BAR0,  reg);
    read_fecs = REG_RD32(FECS,  reg);

    UT_ASSERT_EQUAL_UINT(read_csb, value_csb);
    UT_ASSERT_EQUAL_UINT(read_bar0, value_bar0);
    UT_ASSERT_EQUAL_UINT(read_fecs, value_fecs);
}

UT_CASE_DEFINE(REGMACROS, REG_WR32,
  UT_CASE_SET_DESCRIPTION("Ensure REG_WR32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_WR32 macros
 *
 * @details    Use REG_WR32 to write three different known values to the same register address on 3 different busses.
 *             read them back and make sure that they return the correct values and have not trampled each other.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_WR32)
{
    LwU32 read_csb;
    LwU32 read_bar0;
    LwU32 read_fecs;

    //Seed the same register on 3 busseswith known value
    REG_WR32(CSB, 0x0000abcd, 0xcccccccc);
    REG_WR32(BAR0, 0x0000abcd, 0xbbbbbbbb);
    REG_WR32(FECS, 0x0000abcd, 0xfec5fec5);

    read_csb = CSB_REG_RD32(0x0000abcd);
    read_bar0 = BAR0_REG_RD32(0x0000abcd);
    read_fecs = FECS_REG_RD32(0x0000abcd);

    UT_ASSERT_EQUAL_UINT(read_csb, 0xcccccccc);
    UT_ASSERT_EQUAL_UINT(read_bar0, 0xbbbbbbbb);
    UT_ASSERT_EQUAL_UINT(read_fecs, 0xfec5fec5);
}

UT_CASE_DEFINE(REGMACROS, REG_RD32_STALL,
  UT_CASE_SET_DESCRIPTION("Ensure REG_RD32_STALL works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_RD32_STALL macros
 *
 * @details    Seed three different known values to the same register address on 3 different busses.
 *             read back the CSB value with REG_RD32_STALL and make sure that it returns the correct values and has not been trampled.
 * 
 * @note       There is no REG_RD32_STALL for the BAR0 and FECS busses
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_RD32_STALL)
{
    LwU32 value_csb = 0xcccccccc;
    LwU32 value_bar0 = 0xbbbbbbbb;
    LwU32 value_fecs = 0xfec5fec5;
    LwU32 reg = 0x0000abcd;
    LwU32 read_csb;

    REG_RD32_pretest(reg, value_csb, value_bar0, value_fecs);

    read_csb = REG_RD32_STALL(CSB, reg);

    UT_ASSERT_EQUAL_UINT(read_csb, value_csb);
}

UT_CASE_DEFINE(REGMACROS, REG_WR32_STALL,
  UT_CASE_SET_DESCRIPTION("Ensure REG_WR32_STALL works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief      Test the REG_WR32_STALL macros
 *
 * @details    Use REG_WR32_STALL to write different known values to the same register address on the CSB and FECS busses.
 *             read them back and make sure that they return the correct values and have not trampled each other.
 *
 * @note       There is no REG_WR32_STALL for the BAR0 and FECS buses
 * 
 * No external unit dependencies are used.
 */
UT_CASE_RUN(REGMACROS, REG_WR32_STALL)
{
    LwU32 read_csb;

    //Seed the same register on 3 busses with known value
    REG_WR32_STALL(CSB, 0x0000abcd, 0xcccccccc);

    read_csb = CSB_REG_RD32_STALL(0x0000abcd);

    UT_ASSERT_EQUAL_UINT(read_csb, 0xcccccccc);
}
