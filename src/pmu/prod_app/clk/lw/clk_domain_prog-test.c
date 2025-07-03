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
 * @file    clk_domain_prog-test.c
 * @brief   Unit tests for logic in clk_domain_prog.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobj-mock.h"
#include "perf/changeseq-mock.h" // for CALLOC_MOCK_EXPECTED_VALUE
#include "pmu_objclk.h"
#include "clk/clk_domain_prog.h"
#include "clk/clk_domain_3x_prog.h"
#include "clk/clk_domain_35_primary.h"
#include "clk/pmu_clknafll-mock.h"
#include "clk/clk_domain_prog-mock.h"

// external variables that need to get their own header
extern OBJCLK Clk;
extern const RM_PMU_CLK_CLK_DOMAIN_BOARDOBJ_SET_UNION UT_ClockDomains[10];

static void* testSetup(void)
{
    // callocMockInit();
    // boardObjConstructMockInit();
    // boardObjGrpMaskImportMockInit();
    clkNafllGetFreqMHzByIndexMockInit();
    clkNafllGetIndexByFreqMHzMockInit();
    clkDomainProgIsNoiseAwareMockInit();

    return NULL;
}

UT_SUITE_DEFINE(CLK_DOMAIN_PROG,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for clock domain progs")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))

static FLCN_STATUS
clkDomainProgIsNoiseAware_YesStub
(
    CLK_DOMAIN_PROG *pClkDomainProg,
    LwBool          *pbIsNoiseAware
)
{
    *pbIsNoiseAware = LW_TRUE;
    return FLCN_OK;
}

static FLCN_STATUS
clkDomainProgIsNoiseAware_NoStub
(
    CLK_DOMAIN_PROG *pClkDomainProg,
    LwBool          *pbIsNoiseAware
)
{
    *pbIsNoiseAware = LW_FALSE;
    return FLCN_OK;
}

UT_CASE_DEFINE(CLK_DOMAIN_PROG, GetFreqMHzByIndexNotNoiseAware,
                UT_CASE_SET_DESCRIPTION("Attempt to get the frequency of a non-Noise Aware clock.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_PROG, GetFreqMHzByIndexNotNoiseAware)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };

    clkDomainProg.super.super.apiDomain = LW2080_CTRL_CLK_DOMAIN_MCLK;
    clkDomainProg.prog.super.pVirtualTable = &vtable;
    clkDomainProg.prog.super.pVirtualTable->offset = (LwS16)((LwS32)&clkDomainProg.prog - (LwS32)&clkDomainProg.super);

    clkDomainProgIsNoiseAware_StubWithCallback(0, clkDomainProgIsNoiseAware_NoStub);

    UT_ASSERT_EQUAL_UINT16(clkDomainProgGetFreqMHzByIndex(&clkDomainProg.prog, 5), LW_U16_MAX);
}


UT_CASE_DEFINE(CLK_DOMAIN_PROG, GetFreqMHzByIndexNafllFailure,
                UT_CASE_SET_DESCRIPTION("Call to NAFLL function returns an error.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

static FLCN_STATUS
clkNafllGetFreqMHzByIndex_FailureStub
(
    LwU32   clkDomain,
    LwU16   idx,
    LwU16  *pFreqMHz
)
{
    UT_ASSERT_EQUAL_UINT(clkDomain, LW2080_CTRL_CLK_DOMAIN_GPCCLK);
    UT_ASSERT_EQUAL_UINT(idx, 3);
    return FLCN_ERR_ILWALID_ARGUMENT;
}

UT_CASE_RUN(CLK_DOMAIN_PROG, GetFreqMHzByIndexNafllFailure)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };

    clkDomainProg.super.super.apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    clkDomainProg.prog.super.pVirtualTable = &vtable;
    clkDomainProg.prog.super.pVirtualTable->offset = (LwS16)((LwU32)&clkDomainProg.prog - (LwU32)&clkDomainProg.super);

    clkNafllGetFreqMHzByIndex_StubWithCallback(0, clkNafllGetFreqMHzByIndex_FailureStub);
    clkDomainProgIsNoiseAware_StubWithCallback(0, clkDomainProgIsNoiseAware_YesStub);

    UT_ASSERT_EQUAL_UINT16(clkDomainProgGetFreqMHzByIndex(&clkDomainProg.prog, 3), LW_U16_MAX);
}


UT_CASE_DEFINE(CLK_DOMAIN_PROG, GetFreqByIndexNafllSuccess,
                UT_CASE_SET_DESCRIPTION("Call to NAFLL function returns success.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

static FLCN_STATUS
clkNafllGetFreqMHzByIndex_SuccessStub
(
    LwU32   clkDomain,
    LwU16   idx,
    LwU16  *pFreqMHz
)
{
    UT_ASSERT_EQUAL_UINT(clkDomain, LW2080_CTRL_CLK_DOMAIN_GPCCLK);
    UT_ASSERT_EQUAL_UINT(idx, 4);
    *pFreqMHz = 495;
    return FLCN_OK;
}

UT_CASE_RUN(CLK_DOMAIN_PROG, GetFreqByIndexNafllSuccess)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };

    clkDomainProg.super.super.apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    clkDomainProg.prog.super.pVirtualTable = &vtable;
    clkDomainProg.prog.super.pVirtualTable->offset = (LwS16)((LwU32)&clkDomainProg.prog - (LwU32)&clkDomainProg.super);

    clkNafllGetFreqMHzByIndex_StubWithCallback(0, clkNafllGetFreqMHzByIndex_SuccessStub);
    clkDomainProgIsNoiseAware_StubWithCallback(0, clkDomainProgIsNoiseAware_YesStub);

    UT_ASSERT_EQUAL_UINT16(clkDomainProgGetFreqMHzByIndex(&clkDomainProg.prog, 4), 495);
}


UT_CASE_DEFINE(CLK_DOMAIN_PROG, GetIndexByFreqNotNoiseAware,
                UT_CASE_SET_DESCRIPTION("Attempt to get the frequency of a non-Noise Aware clock.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN_PROG, GetIndexByFreqNotNoiseAware)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };

    clkDomainProg.super.super.apiDomain = LW2080_CTRL_CLK_DOMAIN_MCLK;
    clkDomainProg.prog.super.pVirtualTable = &vtable;
    clkDomainProg.prog.super.pVirtualTable->offset = (LwS16)((LwU32)&clkDomainProg.prog - (LwU32)&clkDomainProg.super);

    clkDomainProgIsNoiseAware_StubWithCallback(0, clkDomainProgIsNoiseAware_NoStub);

    UT_ASSERT_EQUAL_UINT16(clkDomainProgGetIndexByFreqMHz(&clkDomainProg.prog, 805, LW_FALSE), LW_U16_MAX);
}

UT_CASE_DEFINE(CLK_DOMAIN_PROG, GetIndexByFreqNafllFailure,
                UT_CASE_SET_DESCRIPTION("Call to NAFLL function returns an error.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

static FLCN_STATUS
clkNafllGetIndexByFreqMHz_FailureStub
(
    LwU32   clkDomain,
    LwU16   freqMHz,
    LwBool  bFloor,
    LwU16  *pIdx
)
{
    UT_ASSERT_EQUAL_UINT(clkDomain, LW2080_CTRL_CLK_DOMAIN_GPCCLK);
    UT_ASSERT_EQUAL_UINT(freqMHz, 945);
    UT_ASSERT_FALSE(bFloor);
    UT_ASSERT_TRUE(pIdx != NULL);
    return FLCN_ERR_ILWALID_ARGUMENT;
}

UT_CASE_RUN(CLK_DOMAIN_PROG, GetIndexByFreqNafllFailure)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };

    clkDomainProg.super.super.apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    clkDomainProg.prog.super.pVirtualTable = &vtable;
    clkDomainProg.prog.super.pVirtualTable->offset = (LwS16)((LwU32)&clkDomainProg.prog - (LwU32)&clkDomainProg.super);

    clkNafllGetIndexByFreqMHz_StubWithCallback(0, clkNafllGetIndexByFreqMHz_FailureStub);
    clkDomainProgIsNoiseAware_StubWithCallback(0, clkDomainProgIsNoiseAware_YesStub);

    UT_ASSERT_EQUAL_UINT16(clkDomainProgGetIndexByFreqMHz(&clkDomainProg.prog, 945, LW_FALSE), LW_U16_MAX);
}


UT_CASE_DEFINE(CLK_DOMAIN_PROG, GetIndexByFreqNafllSuccess,
                UT_CASE_SET_DESCRIPTION("Call to NAFLL function returns success.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

static FLCN_STATUS
clkNafllGetIndexByFreqMHz_SuccessStub
(
    LwU32   clkDomain,
    LwU16   freqMHz,
    LwBool  bFloor,
    LwU16  *pIdx
)
{
    UT_ASSERT_EQUAL_UINT(clkDomain, LW2080_CTRL_CLK_DOMAIN_GPCCLK);
    UT_ASSERT_EQUAL_UINT(freqMHz, 1165);
    UT_ASSERT_TRUE(bFloor);
    UT_ASSERT_TRUE(pIdx != NULL);
    *pIdx = 57;
    return FLCN_OK;
}

UT_CASE_RUN(CLK_DOMAIN_PROG, GetIndexByFreqNafllSuccess)
{
    INTERFACE_VIRTUAL_TABLE vtable = { 0 };
    CLK_DOMAIN_3X_PROG clkDomainProg = { 0 };

    clkDomainProg.super.super.apiDomain = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    clkDomainProg.prog.super.pVirtualTable = &vtable;
    clkDomainProg.prog.super.pVirtualTable->offset = (LwS16)((LwU32)&clkDomainProg.prog - (LwU32)&clkDomainProg.super);

    clkNafllGetIndexByFreqMHz_StubWithCallback(0, clkNafllGetIndexByFreqMHz_SuccessStub);
    clkDomainProgIsNoiseAware_StubWithCallback(0, clkDomainProgIsNoiseAware_YesStub);

    UT_ASSERT_EQUAL_UINT16(clkDomainProgGetIndexByFreqMHz(&clkDomainProg.prog, 1165, LW_TRUE), 57);
}
