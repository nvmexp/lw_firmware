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
 * @file lwosreg-test.c
 *
 * @details    This file contains all the unit tests for the Unit Registers lwosreg functions.
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
#include "lwosreg.h"

/* -------------------------------------------------------- */
/*!
 * Definition of the Unit Math LwF32 functions Suite.
 */
UT_SUITE_DEFINE(LWOSREG,
                UT_SUITE_SET_COMPONENT("Unit Registers Lwosreg functions")
                UT_SUITE_SET_DESCRIPTION("Unit tests for the Unit Registers Lwosreg functions")
                UT_SUITE_SET_OWNER("ashenfield")
                UT_SUITE_SETUP_HOOK(NULL)
                UT_SUITE_TEARDOWN_HOOK(NULL))


UT_CASE_DEFINE(LWOSREG, osRegRd32,
  UT_CASE_SET_DESCRIPTION("Ensure osRegRd32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSREG_osRegRd32]"))

/*!
 * @brief      Test the osRegRd32 macro
 *
 * @details    Write three different values to the same register address on 3 different busses.
 *             read them back with osRegRd32 and make sure that they return the correct values and have not trampled each other.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOSREG, osRegRd32)
{
    LwU32 read_csb;
    LwU32 read_bar0;
    LwU32 read_fecs;

    CSB_REG_WR32(0x0000abcd, 0xcccccccc);
    BAR0_REG_WR32(0x0000abcd, 0xbbbbbbbb);
    FECS_REG_WR32(0x0000abcd, 0xfec5fec5);

    read_csb = osRegRd32(REG_BUS_CSB,  0x0000abcd);
    read_bar0 = osRegRd32(REG_BUS_BAR0,  0x0000abcd);
    read_fecs = osRegRd32(REG_BUS_FECS,  0x0000abcd);

    UT_ASSERT_EQUAL_UINT(read_csb, 0xcccccccc);
    UT_ASSERT_EQUAL_UINT(read_bar0, 0xbbbbbbbb);
    UT_ASSERT_EQUAL_UINT(read_fecs, 0xfec5fec5);
}

UT_CASE_DEFINE(LWOSREG, osRegRd32_badbus,
  UT_CASE_SET_DESCRIPTION("Ensure osRegRd32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSREG_osRegRd32_badbus]"))

/*!
 * @brief      Test the osRegRd32 macro with a bad bus argument.
 *
 * @details    Call osRegRd32 with an invalid value for the bus argument and ensure that it does not crash
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOSREG, osRegRd32_badbus)
{
    LwU32 readval = osRegRd32(0xff, 0x0000abcd);

    UT_ASSERT_EQUAL_UINT(readval, LW_U32_MAX);
}

UT_CASE_DEFINE(LWOSREG, osRegWr32,
  UT_CASE_SET_DESCRIPTION("Ensure osRegWr32 works correctly.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSREG_osRegWr32]"))

/*!
 * @brief      Test the osRegWr32 macro
 *
 * @details    Use osRegWr32 to write three different values to the same register address on 3 different busses.
 *             read them back with and make sure that they return the correct values and have not trampled each other.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOSREG, osRegWr32)
{
    LwU32 read_csb;
    LwU32 read_bar0;
    LwU32 read_fecs;

    osRegWr32(REG_BUS_CSB, 0x0000abcd, 0xcccccccc);
    osRegWr32(REG_BUS_BAR0, 0x0000abcd, 0xbbbbbbbb);
    osRegWr32(REG_BUS_FECS, 0x0000abcd, 0xfec5fec5);

    read_csb = CSB_REG_RD32(0x0000abcd);
    read_bar0 = BAR0_REG_RD32(0x0000abcd);
    read_fecs = FECS_REG_RD32(0x0000abcd);

    UT_ASSERT_EQUAL_UINT(read_csb, 0xcccccccc);
    UT_ASSERT_EQUAL_UINT(read_bar0, 0xbbbbbbbb);
    UT_ASSERT_EQUAL_UINT(read_fecs, 0xfec5fec5);
}

UT_CASE_DEFINE(LWOSREG, osRegWr32_badbus,
  UT_CASE_SET_DESCRIPTION("Ensure osRegWr32 doesn't crash when give na bad bus argument.")
  UT_CASE_SET_TYPE(REQUIREMENTS)
  UT_CASE_LINK_REQS("TODO")
  UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[LWOSREG_osRegWr32_badbus]"))

/*!
 * @brief      Test the osRegWr32 macro
 *
 * @details    Call osRegWr32 and give it a bad bus argument. Ensure that it does not crash.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(LWOSREG, osRegWr32_badbus)
{
    osRegWr32(0xff, 0x0000abcd, 0xcccccccc);
}
