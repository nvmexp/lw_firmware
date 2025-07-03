/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    clk_prog_3x-test.c
 * @brief   Unit tests for logic in clk_prog_3x.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "pmu_objclk.h"
#include "clk/clk_prog_3x.h"

#include "clk/pmu_clknafll-mock.h"

static void *testSetup(void)
{
    clkNafllFreqMHzQuantizeMockInit();

    return NULL;
}

UT_SUITE_DEFINE(CLK_PROG_3X,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for clock prog 3.x")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(CLK_PROG_3X, QuantizeNonNafllSource,
                UT_CASE_SET_DESCRIPTION("Clock prog uses a non-NAFLL source.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_3X, QuantizeNonNafllSource)
{
    CLK_PROG_3X clkProg = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };
    LwU16 freqMHz = LW_U16_MAX;

    clkProg.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_ONE_SOURCE;
    UT_ASSERT_EQUAL_UINT(
        clkProg3XFreqMHzQuantize(
            &clkProg,
            &clkDomainProg,
            &freqMHz,
            LW_FALSE
        ),
        FLCN_OK // Shouldn't this return an error?
    );
    UT_ASSERT_EQUAL_UINT(freqMHz, LW_U16_MAX);
}

UT_CASE_DEFINE(CLK_PROG_3X, QuantizeNafllSourceWithError,
                UT_CASE_SET_DESCRIPTION("An error oclwrs while quantizing a NAFLL source.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_3X, QuantizeNafllSourceWithError)
{
    CLK_PROG_3X clkProg = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };
    LwU16 freqMHz = 0;

    clkNafllFreqMHzQuantizeMockAddEntry(0, FLCN_ERR_ILWALID_SOURCE);

    clkProg.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
    UT_ASSERT_EQUAL_UINT(
        clkProg3XFreqMHzQuantize(
            &clkProg,
            &clkDomainProg,
            &freqMHz,
            LW_FALSE
        ),
        FLCN_ERR_ILWALID_SOURCE
    );
    UT_ASSERT_EQUAL_UINT(freqMHz, 0);
}

static FLCN_STATUS
clkNafllFreqMHzQuantize_STUB
(
    LwU32   clkDomain,
    LwU16  *pFreqMHz,
    LwBool  bFloor
)
{
    *pFreqMHz = 5000;
    return FLCN_OK;
}

UT_CASE_DEFINE(CLK_PROG_3X, QuantizeNafllSource,
                UT_CASE_SET_DESCRIPTION("An error oclwrs while quantizing a NAFLL source.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_PROG_3X, QuantizeNafllSource)
{
    CLK_PROG_3X clkProg = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };
    LwU16 freqMHz;

    clkNafllFreqMHzQuantize_StubWithCallback(0, clkNafllFreqMHzQuantize_STUB);

    clkProg.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
    UT_ASSERT_EQUAL_UINT(
        clkProg3XFreqMHzQuantize(
            &clkProg,
            &clkDomainProg,
            &freqMHz,
            LW_FALSE
        ),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT(freqMHz, 5000);
}
