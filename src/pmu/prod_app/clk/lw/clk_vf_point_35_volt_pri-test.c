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
 * @file    clk_vf_point_35_volt_pri-test.c
 * @brief   Unit tests for logic in clk_vf_point_35_volt_pri.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "pmu_objclk.h"
#include "clk/clk_vf_point_35_volt_pri.h"
#include "clk/clk_vf_point_35-mock.h"
#include "clk/clk_vf_point_35_volt-mock.h"

extern OBJCLK Clk;

static void *testSetup(void)
{
    clkVfPointGetStatus_35_MockInit();
    //boardObjDynamicCastMockInit();

    return NULL;
}

UT_SUITE_DEFINE(VF_POINT_35_VOLT_PRI,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for VF Point 3.5 Volt (Primary)")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))


static void constructVfPoint(CLK_VF_POINT_35_VOLT_PRI *pVfPoint)
{
    pVfPoint->baseVFTuple.cpmMaxFreqOffsetMHz = 100;
    for (LwU8 i = 0; i < CLK_CLK_VF_POINT_35_VOLT_PRI_FREQ_TUPLE_MAX_SIZE; ++i)
    {
        pVfPoint->baseVFTuple.freqTuple[i].freqMHz = 2000 - (i * 150);
        pVfPoint->offsetedVFTuple[i].freqMHz = 200 * (i + 1);
        pVfPoint->offsetedVFTuple[i].voltageuV = 100000 * (i + 1);
    }
}

// UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, GetStatusBadCast,
//                 UT_CASE_SET_DESCRIPTION("Bad dynamic cast.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT_PRI, GetStatusBadCast)
// {
//     RM_PMU_CLK_CLK_VF_POINT_35_VOLT_PRI_BOARDOBJ_GET_STATUS boardObj = { 0 };
//     CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };

//     constructVfPoint(&vfPoint);
//     boardObjDynamicCastMockAddEntry(0, NULL);

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointIfaceModel10GetStatus_35_VOLT_PRI_IMPL(
//             (BOARDOBJ*)&vfPoint,
//             (RM_PMU_BOARDOBJ*)&boardObj
//         ),
//         FLCN_ERR_ILWALID_CAST
//     );
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, GetStatusBaseClassError,
//                 UT_CASE_SET_DESCRIPTION("Base class returned error.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT_PRI, GetStatusBaseClassError)
// {
//     RM_PMU_CLK_CLK_VF_POINT_35_VOLT_PRI_BOARDOBJ_GET_STATUS boardObj = { 0 };
//     CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };

//     constructVfPoint(&vfPoint);
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint);
//     clkVfPointGetStatus_35_MockAddEntry(0, FLCN_ERR_OUT_OF_RANGE);

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointIfaceModel10GetStatus_35_VOLT_PRI(
//             (BOARDOBJ*)&vfPoint,
//             (RM_PMU_BOARDOBJ*)&boardObj
//         ),
//         FLCN_ERR_OUT_OF_RANGE
//     );
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, GetStatusSuccess,
//                 UT_CASE_SET_DESCRIPTION("Successful call.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT_PRI, GetStatusSuccess)
// {
//     RM_PMU_CLK_CLK_VF_POINT_35_VOLT_PRI_BOARDOBJ_GET_STATUS boardObj = { 0 };
//     CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };

//     constructVfPoint(&vfPoint);
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint);
//     clkVfPointGetStatus_35_MockAddEntry(0, FLCN_OK);

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointIfaceModel10GetStatus_35_VOLT_PRI(
//             (BOARDOBJ*)&vfPoint,
//             (RM_PMU_BOARDOBJ*)&boardObj
//         ),
//         FLCN_OK
//     );

//     UT_ASSERT_EQUAL_UINT(boardObj.baseVFTuple.cpmMaxFreqOffsetMHz, 100);
//     for (LwU8 i = 0; i < CLK_CLK_VF_POINT_35_VOLT_PRI_FREQ_TUPLE_MAX_SIZE; ++i)
//     {
//         UT_ASSERT_EQUAL_UINT(boardObj.baseVFTuple.freqTuple[i].freqMHz, 2000 - (i * 150));
//         UT_ASSERT_EQUAL_UINT(boardObj.offsetedVFTuple[i].freqMHz, 200 * (i + 1));
//         UT_ASSERT_EQUAL_UINT(boardObj.offsetedVFTuple[i].voltageuV, 100000 * (i + 1));
//     }
// }

UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, AccessorIlwalidLwrve,
                UT_CASE_SET_DESCRIPTION("Lwrve is not supported.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(VF_POINT_35_VOLT_PRI, AccessorIlwalidLwrve)
{
    CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
    CLK_DOMAIN_3X_PRIMARY clkDomain = { 0 };
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE tuple = { 0 };

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35FreqTupleAccessor_VOLT_PRI(
            (CLK_VF_POINT_35*)&vfPoint,
            &clkDomain,
            0,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_PRI,
            LW_TRUE,
            &tuple
        ),
        FLCN_ERR_NOT_SUPPORTED
    );
}

UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, AccessorNullTuple,
                UT_CASE_SET_DESCRIPTION("Could not find the base tuple.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(VF_POINT_35_VOLT_PRI, AccessorNullTuple)
{
    CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
    CLK_DOMAIN_3X_PRIMARY clkDomain = { 0 };
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE tuple = { 0 };

    vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI;
    vfPoint.super.super.super.super.grpIdx = 0;

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35FreqTupleAccessor_VOLT_PRI(
            (CLK_VF_POINT_35*)&vfPoint,
            &clkDomain,
            0,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0,
            LW_TRUE,
            &tuple
        ),
        FLCN_ERR_ILWALID_ARGUMENT
    );
}

UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, AccessorSuccess,
                UT_CASE_SET_DESCRIPTION("Could not find the base tuple.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(VF_POINT_35_VOLT_PRI, AccessorSuccess)
{
    CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
    CLK_DOMAIN_3X_PRIMARY clkDomain = { 0 };
    LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE tuple = { 0 };

    vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
    vfPoint.super.super.super.super.grpIdx = 0;

    UT_ASSERT_EQUAL_UINT(
        clkVfPoint35FreqTupleAccessor_VOLT_PRI(
            (CLK_VF_POINT_35*)&vfPoint,
            &clkDomain,
            0,
            LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0,
            LW_TRUE,
            &tuple
        ),
        FLCN_OK
    );
}

// UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, LoadBaseError,
//                 UT_CASE_SET_DESCRIPTION("Base class returned an error while loading.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))
//
// UT_CASE_RUN(VF_POINT_35_VOLT_PRI, LoadBaseError)
// {
//     CLK_VF_POINT vfPoint = { 0 };
//     CLK_PROG_3X_PRIMARY clkProg = { 0 };
//     CLK_DOMAIN_3X_PRIMARY clkDomain = { 0 };
//
//     clkVfPointLoad_35_VOLTMockAddEntry(0, FLCN_ERR_ILWALID_CAST);
//
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointLoad_35_VOLT_PRI(
//             &vfPoint,
//             &clkProg,
//             &clkDomain,
//             0,
//             0
//         ),
//         FLCN_ERR_ILWALID_CAST
//     );
// }

UT_CASE_DEFINE(VF_POINT_35_VOLT_PRI, LoadIlwalidClkPos,
                UT_CASE_SET_DESCRIPTION("An invalid clock position was returned.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(VF_POINT_35_VOLT_PRI, LoadIlwalidClkPos)
{
    CLK_VF_POINT vfPoint = { 0 };
    CLK_PROG_3X_PRIMARY clkProg = { 0 };
    CLK_DOMAIN_35_PROG clkDomain = { 0 };
    CLK_DOMAIN_35_PRIMARY clkDomainPrimary = { 0 };
    BOARDOBJ *pObjects[LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS];

    clkVfPointLoad_35_VOLTMockAddEntry(0, FLCN_OK);
    clkDomain.clkVFLwrveCount = 1;
    pObjects[0] = (BOARDOBJ*)&clkDomain;

    Clk.vfPoints.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    Clk.vfPoints.super.super.classId = LW2080_CTRL_BOARDOBJGRP_CLASS_ID_CLK_CLK_DOMAIN;
    Clk.vfPoints.super.super.objSlots = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
    Clk.vfPoints.super.super.ovlIdxDmem = 0;
    Clk.vfPoints.super.super.bConstructed = LW_TRUE;
    Clk.vfPoints.super.super.ppObjects = &pObjects;
    boardObjGrpMaskInit_E255(&Clk.vfPoints.super.objMask);
    boardObjGrpMaskBitSet(&Clk.vfPoints.super.objMask, 0);

    boardObjGrpMaskInit_E32(&clkDomainPrimary.primarySecondaryDomainsGrpMask);
    boardObjGrpMaskBitSet(&clkDomainPrimary.primarySecondaryDomainsGrpMask, 0);

    UT_ASSERT_EQUAL_UINT(
        clkVfPointLoad_35_VOLT_PRI(
            &vfPoint,
            &clkProg,
            (CLK_DOMAIN_3X_PRIMARY*)&clkDomain,
            0,
            1
        ),
        FLCN_OK
    );
}
