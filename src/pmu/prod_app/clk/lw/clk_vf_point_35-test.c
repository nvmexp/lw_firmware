/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    clk_vf_point-test.c
 * @brief   Unit tests for logic in clk_vf_point.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobjgrp_e32.h"
#include "boardobj/boardobjgrpmask.h"
#include "boardobj/boardobj-mock.h"
#include "pmu_objclk.h"
#include "clk/clk_vf_point_35.h"
#include "clk/clk_vf_point_35_freq.h"
#include "clk/clk_vf_point_35_volt-mock.h"
#include "ctrl/ctrl2080/ctrl2080clk.h"

extern OBJCLK Clk;

/* ------------------------ Test Suite Declaration -------------------------- */
static void initializeClockDomainGrp()
{
    static BOARDOBJ *pClockDomainObjs[32];
    memset(pClockDomainObjs, 0x00, sizeof(pClockDomainObjs));

    Clk.domains.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.domains.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
    Clk.domains.super.super.objSlots = 32;
    Clk.domains.super.super.ovlIdxDmem = 0;
    Clk.domains.super.super.bConstructed = LW_TRUE;
    Clk.domains.super.super.ppObjects = pClockDomainObjs;
}

static void addClockDomain(CLK_DOMAIN *pDomain, LwU8 index)
{
    UT_ASSERT_TRUE(index < LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
    Clk.domains.super.super.ppObjects[index] = (BOARDOBJ*)pDomain;
}

static void initializeClockProgGrp()
{
    static BOARDOBJ *pClockProgObjs[32];
    memset(pClockProgObjs, 0x00, sizeof(pClockProgObjs));

    Clk.progs.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E255;
    Clk.progs.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_PROG;
    Clk.progs.super.super.objSlots = 255;
    Clk.progs.super.super.ovlIdxDmem = 0;
    Clk.progs.super.super.bConstructed = LW_TRUE;
    Clk.progs.super.super.ppObjects = pClockProgObjs;
}

static void addClockProg(CLK_PROG *pProg, LwU8 index)
{
    UT_ASSERT_TRUE(index < LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
    Clk.progs.super.super.ppObjects[index] = (BOARDOBJ*)pProg;
}

static void initializeObjClk()
{
    memset(&Clk, 0x00, sizeof(Clk));
    initializeClockDomainGrp();
    initializeClockProgGrp();
}

static void *testCaseSetup(void)
{
    initializeObjClk();

    boardObjConstructMockInit();
    clkVfPointVoltageuVGet_35_VOLTMockInit();
}

UT_SUITE_DEFINE(CLK_VF_POINT_35,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for VF points 3.5")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testCaseSetup))

/* ------------------------ clkVfPointGrpIfaceModel10ObjSet_SUPER() --------------------- */
UT_CASE_DEFINE(CLK_VF_POINT_35, Trim_NullTuple,
                UT_CASE_SET_DESCRIPTION("Offset tuple is not present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, Trim_NullTuple)
{
    CLK_VF_POINT_35 vfPoint = { 0 };
    CLK_DOMAIN_35_PRIMARY domainPrimary = { 0 };
    CLK_PROG_35_PRIMARY progPrimary = { 0 };

    vfPoint.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_UNKNOWN;

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35Trim(&vfPoint, &domainPrimary, &progPrimary, 0),
        FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(CLK_VF_POINT_35, Trim_NoClockDomains,
                UT_CASE_SET_DESCRIPTION("Base tuple is not present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, Trim_NoClockDomains)
{
    CLK_VF_POINT_35_FREQ vfPoint = { 0 };
    CLK_DOMAIN_35_PRIMARY domainPrimary = { 0 };
    CLK_PROG_35_PRIMARY progPrimary = { 0 };

    memset(&Clk, 0x00, sizeof(Clk));
    vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ;

    boardObjGrpMaskInit_E32(&domainPrimary.primary.secondaryIdxsMask);

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35Trim((CLK_VF_POINT_35*)&vfPoint, &domainPrimary, &progPrimary, 0),
        FLCN_OK
    );
}

// UT_CASE_DEFINE(CLK_VF_POINT_35, Trim_Success,
//                 UT_CASE_SET_DESCRIPTION("Success trimming the VF point.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

//
// Unit test case Trim_Success is failing. Not able to determine why the
// function believes an invalid clock domain index is present.
//
// UT_CASE_RUN(CLK_VF_POINT_35, Trim_Success)
// {
//     CLK_VF_POINT_35_FREQ vfPoint = { 0 };
//     CLK_DOMAIN_35_PRIMARY domainPrimary = { 0 };
//     CLK_PROG_35_PRIMARY progPrimary = { 0 };
//     CLK_DOMAIN_35_PROG clkDomain[2];
//     BOARDOBJ *pClockDomains[32];
//     CLK_PROG_35 clkProg[2];
//     BOARDOBJ *pClockProgs[255];

//     // setup CLK_DOMAIN board object group
//     memset(&Clk, 0x00, sizeof(Clk));
//     Clk.domains.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
//     Clk.domains.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
//     Clk.domains.super.super.objSlots = 32;
//     Clk.domains.super.super.ovlIdxDmem = 0;
//     Clk.domains.super.super.bConstructed = LW_TRUE;
//     Clk.domains.super.super.ppObjects = pClockDomains;
//     memset(pClockDomains, 0x00, sizeof(pClockDomains));
//     pClockDomains[2] = (BOARDOBJ*)&clkDomain[0];
//     pClockDomains[5] = (BOARDOBJ*)&clkDomain[1];

//     clkDomain[0].super.clkProgIdxLast = 19;
//     clkDomain[0].clkVFLwrveCount = 1;
//     clkDomain[0].clkPos = 0;
//     clkDomain[1].super.clkProgIdxLast = 129;
//     clkDomain[1].clkVFLwrveCount = 1;
//     clkDomain[1].clkPos = 1;

//     // setup CLK_PROG board object group
//     Clk.progs.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E255;
//     Clk.progs.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_PROG;
//     Clk.progs.super.super.objSlots = 255;
//     Clk.progs.super.super.ovlIdxDmem = 0;
//     Clk.progs.super.super.bConstructed = LW_TRUE;
//     Clk.progs.super.super.ppObjects = pClockProgs;
//     memset(pClockProgs, 0x00, sizeof(pClockProgs));
//     pClockProgs[19] = (BOARDOBJ*)&clkProg[0];
//     pClockProgs[129] = (BOARDOBJ*)&clkProg[1];

//     clkProg[0].offsettedFreqMaxMHz = 5;
//     clkProg[0].super.freqMaxMHz = 400;
//     clkProg[1].offsettedFreqMaxMHz = 15;
//     clkProg[1].super.freqMaxMHz = 600;

//     for (LwU8 i = 0; i < CLK_CLK_VF_POINT_35_FREQ_FREQ_TUPLE_MAX_SIZE; ++i)
//     {
//         vfPoint.baseVFTuple.freqTuple[i].freqMHz = 500;
//         vfPoint.offsetedVFTuple[i].freqMHz = 10;
//     }

//     boardObjGrpMaskInit_E32(&domainPrimary.primary.secondaryIdxsMask);
//     boardObjGrpMaskBitSet(&domainPrimary.primary.secondaryIdxsMask, 2);
//     boardObjGrpMaskBitSet(&domainPrimary.primary.secondaryIdxsMask, 5);

//     UT_ASSERT_EQUAL_INT(
//         clkVfPoint35Trim(
//             (CLK_VF_POINT_35*)&vfPoint,
//             &domainPrimary,
//             &progPrimary,
//             LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI),
//         FLCN_ERROR
//     );
//     UT_ASSERT_EQUAL_UINT(vfPoint.baseVFTuple.freqTuple[0].freqMHz, 500);
//     UT_ASSERT_EQUAL_UINT(vfPoint.offsetedVFTuple[0].freqMHz, 10);
//     UT_ASSERT_EQUAL_UINT(vfPoint.baseVFTuple.freqTuple[1].freqMHz, 600);
//     UT_ASSERT_EQUAL_UINT(vfPoint.offsetedVFTuple[1].freqMHz, 15);

//     UT_ASSERT_TRUE(LW_FALSE);
// }

UT_CASE_DEFINE(CLK_VF_POINT_35, Accessor_NullTuple,
                UT_CASE_SET_DESCRIPTION("Base tuple is not present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, Accessor_NullTuple)
{
    CLK_VF_POINT_35 vfPoint = { 0 };
    CLK_PROG_3X_PRIMARY progPrimary = { 0 };
    CLK_DOMAIN_3X_PRIMARY domainPrimary = { 0 };
    LW2080_CTRL_CLK_VF_PAIR vfPair = { 0 };
    CLK_DOMAIN_35_PROG clkDomain = { 0 };

    vfPoint.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_UNKNOWN;

    clkDomain.clkVFLwrveCount = 1;
    addClockDomain((CLK_DOMAIN*)&clkDomain, 0);

    UT_ASSERT_EQUAL_UINT(
        clkVfPointAccessor_35(
            (CLK_VF_POINT*)&vfPoint,
            &progPrimary,
            &domainPrimary,
            0,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            LW_FALSE,
            &vfPair),
        FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(CLK_VF_POINT_35, Accessor_IlwalidPosition,
                UT_CASE_SET_DESCRIPTION("Invalid clock position.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, Accessor_IlwalidPosition)
{
    CLK_VF_POINT_35 vfPoint = { 0 };
    CLK_PROG_3X_PRIMARY progPrimary = { 0 };
    CLK_DOMAIN_3X_PRIMARY domainPrimary = { 0 };
    LW2080_CTRL_CLK_VF_PAIR vfPair = { 0 };
    CLK_DOMAIN_35_PROG clkDomain = { 0 };

    vfPoint.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;

    clkDomain.clkVFLwrveCount = 0;
    addClockDomain((CLK_DOMAIN*)&clkDomain, 0);

    UT_ASSERT_EQUAL_UINT(
        clkVfPointAccessor_35(
            (CLK_VF_POINT*)&vfPoint,
            &progPrimary,
            &domainPrimary,
            0,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            LW_FALSE,
            &vfPair),
        FLCN_ERR_ILWALID_ARGUMENT);
}

UT_CASE_DEFINE(CLK_VF_POINT_35, Accessor_BasePOR,
                UT_CASE_SET_DESCRIPTION("Access base with POR voltage type.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, Accessor_BasePOR)
{
    CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
    CLK_PROG_3X_PRIMARY progPrimary = { 0 };
    CLK_DOMAIN_3X_PRIMARY domainPrimary = { 0 };
    LW2080_CTRL_CLK_VF_PAIR vfPair = { 0 };
    CLK_DOMAIN_35_PROG clkDomain = { 0 };

    vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
    vfPoint.baseVFTuple.cpmMaxFreqOffsetMHz = 10;
    for (LwU8 i = 0; i < LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE; ++i)
    {
        vfPoint.baseVFTuple.freqTuple[i].freqMHz = 1200;
    }
    vfPoint.baseVFTuple.voltageuV = 700;
    for (LwU8 j = 0; j < CLK_CLK_VF_POINT_35_VOLT_PRI_FREQ_TUPLE_MAX_SIZE; ++j)
    {
        vfPoint.offsetedVFTuple[j].freqMHz = 650;
        vfPoint.offsetedVFTuple[j].voltageuV = 900;
    }

    clkDomain.clkVFLwrveCount = 1;
    clkDomain.clkPos = 1;
    addClockDomain((CLK_DOMAIN*)&clkDomain, 0);

    clkVfPointVoltageuVGet_35_VOLTMockAddEntry(0, FLCN_OK, 800);
    UT_ASSERT_EQUAL_UINT(
        clkVfPointAccessor_35(
            (CLK_VF_POINT*)&vfPoint,
            &progPrimary,
            &domainPrimary,
            0,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
            LW_FALSE,
            &vfPair),
        FLCN_OK);
    UT_ASSERT_EQUAL_UINT(vfPair.voltageuV, 700);
    UT_ASSERT_EQUAL_UINT(vfPair.freqMHz, 1200);
}

UT_CASE_DEFINE(CLK_VF_POINT_35, Accessor_OffsetSource,
                UT_CASE_SET_DESCRIPTION("Access offset with source voltage type.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, Accessor_OffsetSource)
{
    CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
    CLK_PROG_3X_PRIMARY progPrimary = { 0 };
    CLK_DOMAIN_3X_PRIMARY domainPrimary = { 0 };
    LW2080_CTRL_CLK_VF_PAIR vfPair = { 0 };
    CLK_DOMAIN_35_PROG clkDomain = { 0 };

    vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
    vfPoint.baseVFTuple.cpmMaxFreqOffsetMHz = 10;
    for (LwU8 i = 0; i < LW2080_CTRL_CLK_CLK_VF_POINT_FREQ_TUPLE_MAX_SIZE; ++i)
    {
        vfPoint.baseVFTuple.freqTuple[i].freqMHz = 1200;
    }
    vfPoint.baseVFTuple.voltageuV = 700;
    for (LwU8 j = 0; j < CLK_CLK_VF_POINT_35_VOLT_PRI_FREQ_TUPLE_MAX_SIZE; ++j)
    {
        vfPoint.offsetedVFTuple[j].freqMHz = 650;
        vfPoint.offsetedVFTuple[j].voltageuV = 900;
    }

    clkDomain.clkVFLwrveCount = 1;
    clkDomain.clkPos = 1;
    addClockDomain((CLK_DOMAIN*)&clkDomain, 0);

    clkVfPointVoltageuVGet_35_VOLTMockAddEntry(0, FLCN_OK, 800);
    UT_ASSERT_EQUAL_UINT(
        clkVfPointAccessor_35(
            (CLK_VF_POINT*)&vfPoint,
            &progPrimary,
            &domainPrimary,
            0,
            0,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            LW_TRUE,
            &vfPair),
        FLCN_OK);
    UT_ASSERT_EQUAL_UINT(vfPair.voltageuV, 800);
    UT_ASSERT_EQUAL_UINT(vfPair.freqMHz, 650);
}

UT_CASE_DEFINE(CLK_VF_POINT_35, VoltageGet_NullBase,
                UT_CASE_SET_DESCRIPTION("Base tuple not present.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, VoltageGet_NullBase)
{
    CLK_VF_POINT vfPoint = { 0 };
    LwU32 voltage = 0;

    vfPoint.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_UNKNOWN;

    UT_ASSERT_EQUAL_UINT(
        clkVfPointVoltageuVGet_35(
            &vfPoint,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            &voltage
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(CLK_VF_POINT_35, VoltageGet_SourceVoltageType,
                UT_CASE_SET_DESCRIPTION("Source voltage type.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, VoltageGet_SourceVoltageType)
{
    CLK_VF_POINT vfPoint = { 0 };
    LwU32 voltage = 0;

    vfPoint.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;

    UT_ASSERT_EQUAL_UINT(
        clkVfPointVoltageuVGet_35(
            &vfPoint,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
            &voltage
        ),
        FLCN_ERR_NOT_SUPPORTED
    );
}

UT_CASE_DEFINE(CLK_VF_POINT_35, VoltageGet_PORVoltageType,
                UT_CASE_SET_DESCRIPTION("Source voltage type.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CLK_VF_POINT_35, VoltageGet_PORVoltageType)
{
    CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
    LwU32 voltage = 0;

    vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
    vfPoint.baseVFTuple.voltageuV = 4000;

    UT_ASSERT_EQUAL_UINT(
        clkVfPointVoltageuVGet_35(
            &vfPoint,
            LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
            &voltage
        ),
        FLCN_OK
    );
    UT_ASSERT_EQUAL_UINT32(voltage, 4000);
}
