/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseqscriptstep-test.c
 * @brief   Unit tests for logic in LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "perf/changeseq.h"

/* ------------------------ Function Prototypes ----------------------------- */
static void initializeScript(CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED *pStepLwrr);
static void initializeRequest(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pRequest);
static void initializeVoltListItem(LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pVoltListItem, LwU32 railIdx, LwU32 voltageuV, LwU32 voltageMinNoiseUnawareuV, LwS32 *pVoltOffsetuV);

#define TEST_CASE_NUM_VOLT_RAILS                                            (2U)

/* ------------------------ Test Suite Declaration--------------------------- */
/*!
 * @brief The test group for the voltage-based change sequencer steps.
 *
 * @defgroup VOLT_STEP_TEST_SUITE
 */
UT_SUITE_DEFINE(CHANGESEQSCRIPTSTEPBUILD_VOLT,
                UT_SUITE_SET_COMPONENT("Change Sequencer Script VOLT Step")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("Jeff Hutchins"))

/* ---------------- perfDaemonChangeSeqScriptBuildStep_VOLT() --------------- */
/*!
 * @brief Verify the requested volt list is copied over to the step.
 *
 * @verbatim
 * Test Case ID   : CHANGESEQ_DAEMON.VerifyVoltListCopied
 * Requirement ID :
 * Dependencies   :
 * @endverbatim
 *
 * @ingroup VOLT_STEP_TEST_SUITE
 *
 * Test Setup:
 * @li Create a volt list in the lwrrentChange.
 *
 * Test Exelwtion:
 * @li Call @ref perfDaemonChangeSeqScriptBuildStep_VOLT(). Expect the function
 * to return @ref FLCN_OK.
 * @li Verify the volt list was copied over to the current script step.
 */
UT_CASE_DEFINE(CHANGESEQSCRIPTSTEPBUILD_VOLT, VerifyVoltListCopied,
                UT_CASE_SET_DESCRIPTION("Verify the volt list was copied to the step.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(CHANGESEQSCRIPTSTEPBUILD_VOLT, VerifyVoltListCopied)
{
    CHANGE_SEQ changeSeq;
    CHANGE_SEQ_SCRIPT script;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED stepLwrr;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lwrrentChange;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE lastChange;

    initializeScript(&script, &stepLwrr);
    initializeRequest(&lwrrentChange);
    initializeRequest(&lastChange);

    UT_ASSERT_EQUAL_UINT(
        perfDaemonChangeSeqScriptBuildStep_VOLT_IMPL(
            &changeSeq,
            &script,
            &lwrrentChange,
            &lastChange
        ),
        FLCN_OK
    );

    UT_ASSERT_EQUAL_UINT(script.pStepLwrr->data.volt.voltList.numRails, TEST_CASE_NUM_VOLT_RAILS);
    for (LwU8 i = 0; i < script.pStepLwrr->data.volt.voltList.numRails; ++i)
    {
        UT_ASSERT_EQUAL_UINT(script.pStepLwrr->data.volt.voltList.rails[i].railIdx, i);
        UT_ASSERT_EQUAL_UINT(script.pStepLwrr->data.volt.voltList.rails[i].voltageuV, 500000 + (i + 100000));
        UT_ASSERT_EQUAL_UINT(script.pStepLwrr->data.volt.voltList.rails[i].voltageMinNoiseUnawareuV, 400000);
        for (LwU8 j = 0; j < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++j)
        {
            UT_ASSERT_EQUAL_UINT(script.pStepLwrr->data.volt.voltList.rails[i].voltOffsetuV[i], 0);
        }
    }
}

/* ------------------------ Helper Functions -------------------------------- */
/*!
 * @brief Initializes the change sequencer script.
 *
 * Creates a change sequencer script with the current step being a voltage-based
 * one.
 *
 * @param  pScript  The change sequencer script to initialize.
 */
static void initializeScript(CHANGE_SEQ_SCRIPT *pScript, LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED *pStepLwrr)
{
    memset(pScript, 0x00, sizeof(*pScript));
    memset(pStepLwrr, 0x00, sizeof(*pStepLwrr));

    pScript->pStepLwrr = pStepLwrr;
    pScript->pStepLwrr->data.volt.super.stepId = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_VOLT;
    pScript->pStepLwrr->data.volt.voltList.numRails = TEST_CASE_NUM_VOLT_RAILS;
    for (LwU8 i = 0; i < pScript->pStepLwrr->data.volt.voltList.numRails; ++i)
    {
        LwS32 voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX];
        memset(voltOffsetuV, 0x00, sizeof(voltOffsetuV));
        initializeVoltListItem(&pScript->pStepLwrr->data.volt.voltList.rails[i], i, 0, 0, voltOffsetuV);
    }
}

/*!
 * @brief Initializes the change request.
 *
 * Initializes the change request to TEST_CASE_NUM_VOLT_RAILS and the associated
 * volt rail items.
 *
 * @param  pRequest  The change request to initialize.
 */
static void initializeRequest(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pRequest)
{
    memset(pRequest, 0x00, sizeof(*pRequest));
    pRequest->voltList.numRails = TEST_CASE_NUM_VOLT_RAILS;
    for (LwU8 i = 0; i < pRequest->voltList.numRails; ++i)
    {
        LwS32 voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX];
        memset(voltOffsetuV, 0x00, sizeof(voltOffsetuV));
        initializeVoltListItem(
            &pRequest->voltList.rails[i],
            i,
            500000 + (i + 100000),
            400000,
            voltOffsetuV
        );
    }
}

/*!
 * @brief Initializes the volt list item.
 *
 * @param  pVoltListItem             The volt list item to initialize.
 * @param  railIdx                   The index to the volt rail to use.
 * @param  voltageuV                 The voltage to set the item to.
 * @param  voltageMinNoiseUnawareuV  The minimum voltage to set the item to for
 *                                   noise unaware clocks.
 * @param  pVoltOffsetuV             Any voltage offsets to apply.
 */
static void initializeVoltListItem
(
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pVoltListItem,
    LwU32 railIdx,
    LwU32 voltageuV,
    LwU32 voltageMinNoiseUnawareuV,
    LwS32 *pVoltOffsetuV
)
{
    pVoltListItem->railIdx = railIdx;
    pVoltListItem->voltageuV = voltageuV;
    pVoltListItem->voltageMinNoiseUnawareuV = voltageMinNoiseUnawareuV;
    for (LwU8 i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; ++i)
    {
        pVoltListItem->voltOffsetuV[i] = pVoltOffsetuV[i];
    }
}
