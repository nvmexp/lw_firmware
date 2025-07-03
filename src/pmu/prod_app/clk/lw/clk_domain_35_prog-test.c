/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    clk_domain_35_prog-test.c
 * @brief   Unit tests for logic in clk_domain_35_prog.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"

#include "pmu_objclk.h"
#include "clk/clk_domain_35_prog.h"
#include "clk/clk_domain_35_primary-mock.h"
#include "clk/clk_domain_35_secondary-mock.h"

// external variables that need to get their own header
extern OBJCLK Clk;

static void* testSetup(void)
{
    clkDomainProgVoltToFreqTuple_35_PRIMARYMockInit();
    clkDomainProgVoltToFreqTuple_35_SECONDARYMockInit();

    return NULL;
}

UT_SUITE_DEFINE(CLK_DOMAIN_35_PROG,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for clock domain 3.5 progs")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))


static void constructClkDomainProg(CLK_DOMAIN_35_PROG *pClkDomainProg, INTERFACE_VIRTUAL_TABLE *pVtable, LwU8 classType)
{
    pClkDomainProg  // CLK_DOMAIN_35_PROG
        ->super     // CLK_DOMAIN_3X_PROG
        .super      // CLK_DOMAIN_3X
        .super      // CLK_DOMAIN
        .super      // BOARDOBJ_VTABLE
        .super      // BOARDOBJ
        .type = classType;
    pClkDomainProg  // CLK_DOMAIN_35_PROG
        ->super     // CLK_DOMAIN_3X_PROG
        .prog       // CLK_DOMAIN_PROG
        .super      // BOARDOJB_INTERFACE
        .pVirtualTable = pVtable;
    pClkDomainProg  // CLK_DOMAIN_35_PROG
        ->super     // CLK_DOMAIN_3X_PROG
        .prog       // CLK_DOMAIN_PROG
        .super      // BOARDOJB_INTERFACE
        .pVirtualTable->offset = (LwS16)((LwS32)&pClkDomainProg->super.prog - (LwS32)pClkDomainProg);
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PROG, VoltToFreqTupleNullIter,
                UT_CASE_SET_DESCRIPTION("Pass a null pointer as the iterator.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PROG, VoltToFreqTupleNullIter)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_35_PROG clkDomainProg = { 0 };
    LW2080_CTRL_CLK_VF_INPUT input = { 0 };
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE output = { 0 };

    constructClkDomainProg(&clkDomainProg, &vtable, LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY);

    UT_ASSERT_EQUAL_UINT(
        clkDomainProgVoltToFreqTuple_35(
            &clkDomainProg.super.prog,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            &input,
            &output,
            NULL
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}


UT_CASE_DEFINE(CLK_DOMAIN_35_PROG, VoltToFreqTupleIlwalidDomainType,
                UT_CASE_SET_DESCRIPTION("Pass a domain not supported by the function.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PROG, VoltToFreqTupleIlwalidDomainType)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_35_PROG clkDomainProg = { 0 };
    LW2080_CTRL_CLK_VF_INPUT input = { 0 };
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE output = { 0 };
    LW2080_CTRL_CLK_VF_ITERATION_STATE iter = { 0 };

    constructClkDomainProg(&clkDomainProg, &vtable, LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED);

    UT_ASSERT_EQUAL_UINT(
        clkDomainProgVoltToFreqTuple_35(
            &clkDomainProg.super.prog,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            &input,
            &output,
            &iter
        ),
        FLCN_ERR_NOT_SUPPORTED
    );
}


UT_CASE_DEFINE(CLK_DOMAIN_35_PROG, VoltToFreqTuplePrimaryDomainFailed,
                UT_CASE_SET_DESCRIPTION("Calling clock domain specific code failed.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PROG, VoltToFreqTuplePrimaryDomainFailed)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_35_PROG clkDomainProg = { 0 };
    LW2080_CTRL_CLK_VF_INPUT input = { 0 };
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE output = { 0 };
    LW2080_CTRL_CLK_VF_ITERATION_STATE iter = { 0 };

    constructClkDomainProg(&clkDomainProg, &vtable, LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_PRIMARY);
    clkDomainProgVoltToFreqTuple_35_PRIMARYMockAddEntry(0, FLCN_ERR_MISMATCHED_TARGET);

    UT_ASSERT_EQUAL_UINT(
        clkDomainProgVoltToFreqTuple_35(
            &clkDomainProg.super.prog,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            &input,
            &output,
            &iter
        ),
        FLCN_ERR_MISMATCHED_TARGET
    );
}

UT_CASE_DEFINE(CLK_DOMAIN_35_PROG, VoltToFreqTupleSuccess,
                UT_CASE_SET_DESCRIPTION("Calling clock domain specific code failed.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_35_PROG, VoltToFreqTupleSuccess)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_35_PROG clkDomainProg = { 0 };
    LW2080_CTRL_CLK_VF_INPUT input = { 0 };
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE output = { 0 };
    LW2080_CTRL_CLK_VF_ITERATION_STATE iter = { 0 };

    constructClkDomainProg(&clkDomainProg, &vtable, LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_35_SECONDARY);
    clkDomainProgVoltToFreqTuple_35_SECONDARYMockAddEntry(0, FLCN_OK);

    UT_ASSERT_EQUAL_UINT(
        clkDomainProgVoltToFreqTuple_35(
            &clkDomainProg.super.prog,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            &input,
            &output,
            &iter
        ),
        FLCN_OK
    );
}
