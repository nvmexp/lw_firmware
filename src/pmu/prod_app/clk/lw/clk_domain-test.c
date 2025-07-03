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
 * @file    clk_domain-test.c
 * @brief   Unit tests for logic in clk_domain.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobjgrpmask.h"

#include "pmu_objclk.h"
#include "clk/clk_domain.h"
#include "clk/clk_domain-mock.h"

// external variables that need to get their own header
extern OBJCLK Clk;

static void* testSetup(void)
{
    memset(&Clk, 0x00, sizeof(Clk));

    return NULL;
}

UT_SUITE_DEFINE(CLK_DOMAIN,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for clock domains")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(CLK_DOMAIN, IsFixedNullParameter,
                UT_CASE_SET_DESCRIPTION("Pass a null pointer to the function.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

// Non-null test cases are handled by class-specific tests.
UT_CASE_RUN(CLK_DOMAIN, IsFixedNullParameter)
{
    UT_ASSERT_FALSE(clkDomainIsFixed_IMPL(NULL));
}


UT_CASE_DEFINE(CLK_DOMAIN, LoadDomainsError,
                UT_CASE_SET_DESCRIPTION("Error while loading the clock domains.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN, LoadDomainsError)
{
    CLK_DOMAIN clkDomain = { 0 };
    BOARDOBJ *pObjects[32] = { 0 };

    pObjects[0] = (BOARDOBJ*)&clkDomain;

    Clk.domains.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.domains.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
    Clk.domains.super.super.objSlots = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
    Clk.domains.super.super.ovlIdxDmem = 0;
    Clk.domains.super.super.bConstructed = LW_TRUE;
    Clk.domains.super.super.ppObjects = pObjects;
    boardObjGrpMaskInit_E32(&Clk.domains.super.objMask);
    boardObjGrpMaskBitSet(&Clk.domains.super.objMask, 0);

    boardObjGrpMaskInit_E32(&Clk.domains.progDomainsMask);
    boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 0);

    clkDomainLoadMockAddEntry(0, FLCN_ERR_ILWALID_ARGUMENT);

    UT_ASSERT_EQUAL_UINT(clkDomainsLoad(), FLCN_ERR_ILWALID_ARGUMENT);
    UT_ASSERT_FALSE(Clk.domains.bLoaded);
}

UT_CASE_DEFINE(CLK_DOMAIN, LoadDomainsSuccess,
                UT_CASE_SET_DESCRIPTION("Success while loading the clock domains.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_DOMAIN, LoadDomainsSuccess)
{
    CLK_DOMAIN clkDomain = { 0 };
    BOARDOBJ *pObjects[32] = { 0 };

    pObjects[0] = (BOARDOBJ*)&clkDomain;

    Clk.domains.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.domains.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
    Clk.domains.super.super.objSlots = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
    Clk.domains.super.super.ovlIdxDmem = 0;
    Clk.domains.super.super.bConstructed = LW_TRUE;
    Clk.domains.super.super.ppObjects = pObjects;
    boardObjGrpMaskInit_E32(&Clk.domains.super.objMask);
    boardObjGrpMaskBitSet(&Clk.domains.super.objMask, 0);

    boardObjGrpMaskInit_E32(&Clk.domains.progDomainsMask);
    boardObjGrpMaskBitSet(&Clk.domains.progDomainsMask, 0);

    clkDomainLoadMockAddEntry(0, FLCN_OK);

    UT_ASSERT_EQUAL_UINT(clkDomainsLoad(), FLCN_OK);
    UT_ASSERT_TRUE(Clk.domains.bLoaded);
}
