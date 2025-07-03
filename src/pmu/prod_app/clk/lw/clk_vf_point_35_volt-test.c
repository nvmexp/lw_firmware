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
 * @file    clk_vf_point_35_volt-test.c
 * @brief   Unit tests for logic in clk_vf_point_35_volt.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "pmusw.h"

#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobj-mock.h"
#include "boardobj/boardobjgrpmask.h"
#include "perf/changeseq-mock.h" // for CALLOC_MOCK_EXPECTED_VALUE

#include "pmu_objclk.h"
#include "clk/clk_vf_point_35-mock.h"
#include "clk/clk_vf_point_35_volt.h"

#include "volt/objvolt.h"
#include "volt/voltrail-mock.h"

static void *testSetup(void)
{
    callocMockInit();
    boardObjConstructMockInit();
    //boardObjDynamicCastMockInit();
    clkVfPointConstruct_35MockInit();
    clkVfPointVoltageuVGet_35_MockInit();

    return NULL;
}

UT_SUITE_DEFINE(VF_POINT_35_VOLT,
                UT_SUITE_SET_COMPONENT("Clocks Software")
                UT_SUITE_SET_DESCRIPTION("Unit tests for VF Point 3.5 Volt")
                UT_SUITE_SET_OWNER("Jeff Hutchins")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(VF_POINT_35_VOLT, FirstConstructNoFreeMem,
                UT_CASE_SET_DESCRIPTION("Encounter no free memory during construction.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(VF_POINT_35_VOLT, FirstConstructNoFreeMem)
{
    RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET_UNION desc = { 0 };
    BOARDOBJGRP boardObjGrp;
    BOARDOBJ   *pBoardObj = NULL;

    callocMockAddEntry(0, NULL, NULL);
    boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
    clkVfPointConstruct_35_StubWithCallback(0, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);

    desc.v35volt.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
    desc.v35volt.super.super.super.grpIdx = 0;
    desc.v35volt.sourceVoltageuV = 500000;
    desc.v35volt.freqDelta.data.staticOffset.deltakHz = 0;

    UT_ASSERT_EQUAL_UINT(
        clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
            &boardObjGrp,
            &pBoardObj,
            sizeof(CLK_VF_POINT_35_VOLT),
            (RM_PMU_BOARDOBJ*)&desc
        ),
        FLCN_ERR_NO_FREE_MEM
    );
    UT_ASSERT_EQUAL_PTR(pBoardObj, NULL);
}

// UT_CASE_DEFINE(VF_POINT_35_VOLT, FirstConstructBadCast,
//                 UT_CASE_SET_DESCRIPTION("Attempting to use the wrong constructor.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, FirstConstructBadCast)
// {
//     RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET_UNION desc = { 0 };
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     BOARDOBJGRP boardObjGrp;
//     BOARDOBJ   *pBoardObj = NULL;

//     callocMockAddEntry(0, &vfPoint, NULL);
//     boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(0, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(1, NULL);

//     desc.v35volt.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC;
//     desc.v35volt.super.super.super.grpIdx = 0;
//     desc.v35volt.sourceVoltageuV = 500000;
//     desc.v35volt.freqDelta.data.staticOffset.deltakHz = 0;

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT_IMPL(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_ERR_ILWALID_CAST
//     );
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, FirstConstructSuccess,
//                 UT_CASE_SET_DESCRIPTION("Success initially constructing clock domain.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, FirstConstructSuccess)
// {
//     RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET_UNION desc = { 0 };
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     BOARDOBJGRP boardObjGrp;
//     BOARDOBJ   *pBoardObj = NULL;

//     callocMockAddEntry(0, &vfPoint, NULL);
//     boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(0, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint.super.super.super);
//     boardObjDynamicCastMockAddEntry(1, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(2, (void*)&vfPoint);

//     desc.v35volt.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     desc.v35volt.super.super.super.grpIdx = 0;
//     desc.v35volt.sourceVoltageuV = 500000;
//     desc.v35volt.freqDelta.data.staticOffset.deltakHz = 0;

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_OK
//     );

//     UT_ASSERT_EQUAL_UINT(vfPoint.super.super.super.type, LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI);
//     UT_ASSERT_EQUAL_UINT(vfPoint.super.super.super.grpIdx, 0);
//     UT_ASSERT_EQUAL_UINT(vfPoint.sourceVoltageuV, 500000);
//     UT_ASSERT_EQUAL_UINT(vfPoint.freqDelta.data.staticOffset.deltakHz, 0);
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, SubsequentConstructIlwalidType,
//                 UT_CASE_SET_DESCRIPTION("Attempting to update a different type")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, SubsequentConstructIlwalidType)
// {
//     RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET_UNION desc = { 0 };
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     BOARDOBJGRP boardObjGrp;
//     BOARDOBJ   *pBoardObj = NULL;

//     callocMockAddEntry(0, &vfPoint, NULL);
//     boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
//     boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(0, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(1, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint.super.super.super);
//     boardObjDynamicCastMockAddEntry(1, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(2, (void*)&vfPoint);
//     boardObjDynamicCastMockAddEntry(3, (void*)&vfPoint.super.super.super);
//     boardObjDynamicCastMockAddEntry(4, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(5, (void*)&vfPoint);

//     desc.v35volt.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     desc.v35volt.super.super.super.grpIdx = 0;
//     desc.v35volt.sourceVoltageuV = 500000;
//     desc.v35volt.freqDelta.data.staticOffset.deltakHz = 0;

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_OK
//     );
//     desc.v35volt.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_FREQ;
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_ERROR
//     );
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, SubsequentConstructIlwalidElement,
//                 UT_CASE_SET_DESCRIPTION("Attempting to update with an invalid index")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, SubsequentConstructIlwalidElement)
// {
//     RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET_UNION desc = { 0 };
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     BOARDOBJGRP boardObjGrp;
//     BOARDOBJ   *pBoardObj = NULL;

//     callocMockAddEntry(0, &vfPoint, NULL);
//     boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
//     boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(0, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(1, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint.super.super.super);
//     boardObjDynamicCastMockAddEntry(1, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(2, (void*)&vfPoint);
//     boardObjDynamicCastMockAddEntry(3, (void*)&vfPoint.super.super.super);
//     boardObjDynamicCastMockAddEntry(4, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(5, (void*)&vfPoint);

//     desc.v35volt.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     desc.v35volt.super.super.super.grpIdx = 0;
//     desc.v35volt.sourceVoltageuV = 500000;
//     desc.v35volt.freqDelta.data.staticOffset.deltakHz = 0;

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_OK
//     );
//     desc.v35volt.super.super.super.grpIdx = 20;
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_ERR_ILWALID_ARGUMENT
//     );
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, SubsequentConstructSuccess,
//                 UT_CASE_SET_DESCRIPTION("Successful update to VF point.")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, SubsequentConstructSuccess)
// {
//     RM_PMU_CLK_CLK_VF_POINT_BOARDOBJ_SET_UNION desc = { 0 };
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     BOARDOBJGRP boardObjGrp;
//     BOARDOBJ   *pBoardObj = NULL;

//     callocMockAddEntry(0, &vfPoint, NULL);
//     boardObjConstruct_StubWithCallback(0, boardObjGrpIfaceModel10ObjSet_IMPL);
//     boardObjConstruct_StubWithCallback(1, boardObjGrpIfaceModel10ObjSet_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(0, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     clkVfPointConstruct_35_StubWithCallback(1, clkVfPointGrpIfaceModel10ObjSet_35_IMPL);
//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint.super.super.super);
//     boardObjDynamicCastMockAddEntry(1, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(2, (void*)&vfPoint);
//     boardObjDynamicCastMockAddEntry(3, (void*)&vfPoint.super.super.super);
//     boardObjDynamicCastMockAddEntry(4, (void*)&vfPoint.super);
//     boardObjDynamicCastMockAddEntry(5, (void*)&vfPoint);

//     desc.v35volt.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     desc.v35volt.super.super.super.grpIdx = 0;
//     desc.v35volt.sourceVoltageuV = 500000;
//     desc.v35volt.freqDelta.data.staticOffset.deltakHz = 0;

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_OK
//     );
//     desc.v35volt.sourceVoltageuV = 200000;
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointGrpIfaceModel10ObjSet_35_VOLT(
//             &boardObjGrp,
//             &pBoardObj,
//             sizeof(CLK_VF_POINT_35_VOLT),
//             (RM_PMU_BOARDOBJ*)&desc
//         ),
//         FLCN_OK
//     );
//     UT_ASSERT_EQUAL_UINT(vfPoint.sourceVoltageuV, 200000);
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, GetVoltageBadCast,
//                 UT_CASE_SET_DESCRIPTION("Incorrect VF point class was passed in")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, GetVoltageBadCast)
// {
//     CLK_VF_POINT_40_VOLT vfPoint = { 0 };
//     LwU32 voltage;

//     boardObjDynamicCastMockAddEntry(0, NULL);
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointVoltageuVGet_35_VOLT_IMPL(
//             (CLK_VF_POINT*)&vfPoint,
//             LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
//             &voltage
//         ),
//         FLCN_ERR_ILWALID_CAST
//     );
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, GetVoltageIlwalidSource,
//                 UT_CASE_SET_DESCRIPTION("Invalid source for the VF point")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, GetVoltageIlwalidSource)
// {
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     LwU32 voltage;

//     vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     vfPoint.super.super.super.grpIdx = 0;
//     vfPoint.sourceVoltageuV = 500000;
//     vfPoint.freqDelta.data.staticOffset.deltakHz = 0;

//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint);
//     clkVfPointVoltageuVGet_35_MockAddEntry(0, FLCN_ERR_ILWALID_ARGUMENT);

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointVoltageuVGet_35_VOLT_IMPL(
//             (CLK_VF_POINT*)&vfPoint,
//             LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
//             &voltage
//         ),
//         FLCN_ERR_NOT_SUPPORTED
//     );
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, GetVoltageValidSource,
//                 UT_CASE_SET_DESCRIPTION("Valid source for the VF point")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, GetVoltageValidSource)
// {
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     LwU32 voltage;

//     vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     vfPoint.super.super.super.grpIdx = 0;
//     vfPoint.sourceVoltageuV = 500000;
//     vfPoint.freqDelta.data.staticOffset.deltakHz = 0;

//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint);
//     clkVfPointVoltageuVGet_35_MockAddEntry(0, FLCN_ERR_ILWALID_ARGUMENT);

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointVoltageuVGet_35_VOLT_IMPL(
//             (CLK_VF_POINT*)&vfPoint,
//             LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
//             &voltage
//         ),
//         FLCN_OK
//     );
//     UT_ASSERT_EQUAL_UINT(voltage, 500000);
// }

// static FLCN_STATUS
// clkVfPointVoltageuVGet_35_STUB
// (
//     CLK_VF_POINT  *pVfPoint,
//     LwU8           voltageType,
//     LwU32         *pVoltageuV
// )
// {
//     *pVoltageuV = 750000;
//     return FLCN_OK;
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, ValueFromBaseClass,
//                 UT_CASE_SET_DESCRIPTION("The base class was able to obtain the voltage")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))

// UT_CASE_RUN(VF_POINT_35_VOLT, ValueFromBaseClass)
// {
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//     LwU32 voltage;

//     vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     vfPoint.super.super.super.grpIdx = 0;
//     vfPoint.sourceVoltageuV = 500000;
//     vfPoint.freqDelta.data.staticOffset.deltakHz = 0;

//     boardObjDynamicCastMockAddEntry(0, (void*)&vfPoint);
//     clkVfPointVoltageuVGet_35_StubWithCallback(0, clkVfPointVoltageuVGet_35_STUB);

//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointVoltageuVGet_35_VOLT_IMPL(
//             (CLK_VF_POINT*)&vfPoint,
//             LW2080_CTRL_CLK_VOLTAGE_TYPE_SOURCE,
//             &voltage
//         ),
//         FLCN_OK
//     );
//     UT_ASSERT_EQUAL_UINT(voltage, 750000);
// }

// UT_CASE_DEFINE(VF_POINT_35_VOLT, LoadNullBaseTuple,
//                 UT_CASE_SET_DESCRIPTION("The base VF tuple is not present")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))
//
// UT_CASE_RUN(VF_POINT_35_VOLT, LoadNullBaseTuple)
// {
//     CLK_VF_POINT_35_VOLT vfPoint = { 0 };
//
//     vfPoint.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_40_VOLT_PRI;
//     vfPoint.super.super.super.grpIdx = 0;
//     vfPoint.sourceVoltageuV = 500000;
//     vfPoint.freqDelta.data.staticOffset.deltakHz = 0;
//
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointLoad_35_VOLT(
//             (CLK_VF_POINT*)&vfPoint,
//             NULL,
//             NULL,
//             0,
//             0
//         ),
//         FLCN_ERR_ILWALID_ARGUMENT
//     );
// }
//
// UT_CASE_DEFINE(VF_POINT_35_VOLT, LoadIlwalidVoltRail,
//                 UT_CASE_SET_DESCRIPTION("The volt rail is invalid")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))
//
// UT_CASE_RUN(VF_POINT_35_VOLT, LoadIlwalidVoltRail)
// {
//     CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
//     CLK_PROG_3X_PRIMARY clkProg = { 0 };
//     CLK_DOMAIN_3X_PRIMARY clkDomain = { 0 };
//
//     vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     vfPoint.super.super.super.super.grpIdx = 0;
//     vfPoint.super.sourceVoltageuV = 500000;
//     vfPoint.super.freqDelta.data.staticOffset.deltakHz = 0;
//
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointLoad_35_VOLT(
//             (CLK_VF_POINT*)&vfPoint,
//             NULL,
//             NULL,
//             0,
//             0
//         ),
//         FLCN_ERR_NOT_SUPPORTED
//     );
// }
//
// extern OBJVOLT Volt;
//
// UT_CASE_DEFINE(VF_POINT_35_VOLT, LoadBadRounding,
//                 UT_CASE_SET_DESCRIPTION("Error rounding voltage")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))
//
// UT_CASE_RUN(VF_POINT_35_VOLT, LoadBadRounding)
// {
//     CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
//     CLK_PROG_3X_PRIMARY clkProg = { 0 };
//     CLK_DOMAIN_3X_PRIMARY clkDomain = { 0 };
//     BOARDOBJ *pBoardObj[32] = { 0 };
//     VOLT_RAIL voltRail;
//
//     vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     vfPoint.super.super.super.super.grpIdx = 0;
//     vfPoint.super.sourceVoltageuV = 500000;
//     vfPoint.super.freqDelta.data.staticOffset.deltakHz = 0;
//
//     memset(&Volt.railMetadata, 0x00, sizeof(Volt.railMetadata));
//     Volt.railMetadata.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
//     Volt.railMetadata.super.super.classId = 0;
//     Volt.railMetadata.super.super.objSlots = 32;
//     Volt.railMetadata.super.super.ovlIdxDmem = 0;
//     Volt.railMetadata.super.super.bConstructed = LW_TRUE;
//     Volt.railMetadata.super.super.ppObjects = pBoardObj;
//     pBoardObj[0] = (BOARDOBJ*)&voltRail;
//     boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 0);
//
//     voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV = 506025;
//     voltRailRoundVoltage_MOCK_CONFIG.status = FLCN_ERR_ILLEGAL_OPERATION;
//
//
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointLoad_35_VOLT(
//             (CLK_VF_POINT*)&vfPoint,
//             NULL,
//             NULL,
//             0,
//             0
//         ),
//         FLCN_ERR_ILLEGAL_OPERATION
//     );
// }
//
// UT_CASE_DEFINE(VF_POINT_35_VOLT, LoadSuccess,
//                 UT_CASE_SET_DESCRIPTION("Successful loading")
//                 UT_CASE_SET_TYPE(REQUIREMENTS)
//                 UT_CASE_LINK_REQS("TODO")
//                 UT_CASE_LINK_SPECS("TODO"))
//
// UT_CASE_RUN(VF_POINT_35_VOLT, LoadSuccess)
// {
//     CLK_VF_POINT_35_VOLT_PRI vfPoint = { 0 };
//     CLK_PROG_3X_PRIMARY clkProg = { 0 };
//     CLK_DOMAIN_3X_PRIMARY clkDomain = { 0 };
//     BOARDOBJ *pBoardObj[32] = { 0 };
//     VOLT_RAIL voltRail;
//
//     vfPoint.super.super.super.super.type = LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_PRI;
//     vfPoint.super.super.super.super.grpIdx = 0;
//     vfPoint.super.sourceVoltageuV = 500000;
//     vfPoint.super.freqDelta.data.staticOffset.deltakHz = 0;
//
//     memset(&Volt.railMetadata, 0x00, sizeof(Volt.railMetadata));
//     Volt.railMetadata.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
//     Volt.railMetadata.super.super.classId = 0;
//     Volt.railMetadata.super.super.objSlots = 32;
//     Volt.railMetadata.super.super.ovlIdxDmem = 0;
//     Volt.railMetadata.super.super.bConstructed = LW_TRUE;
//     Volt.railMetadata.super.super.ppObjects = pBoardObj;
//     pBoardObj[0] = (BOARDOBJ*)&voltRail;
//     boardObjGrpMaskInit_E32(&Volt.railMetadata.super.objMask);
//     boardObjGrpMaskBitSet(&Volt.railMetadata.super.objMask, 0);
//
//     voltRailRoundVoltage_MOCK_CONFIG.roundedVoltageuV = 506025;
//     voltRailRoundVoltage_MOCK_CONFIG.status = FLCN_OK;
//
//
//     UT_ASSERT_EQUAL_UINT(
//         clkVfPointLoad_35_VOLT(
//             (CLK_VF_POINT*)&vfPoint,
//             NULL,
//             NULL,
//             0,
//             0
//         ),
//         FLCN_OK
//     );
//     UT_ASSERT_EQUAL_UINT(vfPoint.baseVFTuple.voltageuV, 506025);
// }
