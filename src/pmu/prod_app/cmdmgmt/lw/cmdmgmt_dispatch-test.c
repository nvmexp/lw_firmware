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
 * @file    cmdmgmt_dispatch-test.c
 * @brief   Unit tests for logic in cmdmgmt_dispatch.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "config/pmu-config.h"
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_dispatch.h"
#include "cmdmgmt/cmdmgmt_rpc_impl-mock.h"
#include "pmu_rtos_queues.h"
#include "lwrtos-mock.h"
#include "lwostask.h"

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @breif   cmdmgmt_dispatch.c tries to attach this overlay, but overlays aren't
 *          defined for the test build, so we define it here to a meaningless
 *          value
 *
 * @note    TODO: Find a general way to solve this issue for all PMU ucode
 */
OSTASK_OVL_DESC _overlay_id_imem_cmdmgmtRpc = 0;

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/*!
 * @breif   Declares an RM_FLCN_CMD_PMU and DISP2UNIT_RM_RPC structure for
 *          a generic RPC with the UNIT set to the passed unit.
 *
 * @param[in]   command_name    Name of the RM_FLCN_CMD_PMU struct to declare
 * @param[in]   request_name    Name of the DISP2UNIT_RM_RPC struct to declare
 * @param[in]   unit            RM_PMU_UNIT_* to declare as target for struct
 */
#define DECLARE_COMMAND_AND_REQUEST(command_name, request_name, unit)          \
    RM_FLCN_CMD_PMU (command_name) =                                           \
    {                                                                          \
        .cmd =                                                                 \
        {                                                                      \
            .rpc =                                                             \
            {                                                                  \
                /* Note: this isn't actually a valid pointer to anything */    \
                .rpcDmemPtr =                                                  \
                    ((LwU32)&(command_name)) +                                 \
                    RM_FLCN_QUEUE_HDR_SIZE +                                   \
                    sizeof(RM_PMU_RPC_CMD),                                    \
            },                                                                 \
        },                                                                     \
    };                                                                         \
                                                                               \
    DISP2UNIT_RM_RPC (request_name) =                                          \
    {                                                                          \
        .hdr =                                                                 \
        {                                                                      \
            .unitId = (unit),                                                  \
        },                                                                     \
        .pRpc = &(command_name),                                               \
    };

/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_CMDMGMT_DISPATCH,
                UT_SUITE_SET_COMPONENT("CmdMgmt")
                UT_SUITE_SET_DESCRIPTION("Tests Unit CmdMgmt Dispatch of Component CmdMgmt")
                UT_SUITE_SET_OWNER("aherring"))

UT_CASE_DEFINE(PMU_CMDMGMT_DISPATCH, DispatchOfCmdMgmt,
                UT_CASE_SET_DESCRIPTION("Tests dispatching to RM_PMU_UNIT_CMDMGMT")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_DISPATCH, DispatchOfValidUnit,
                UT_CASE_SET_DESCRIPTION("Tests dispatching to a valid non-CMDMGMT RM_PMU_UNIT")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_DISPATCH, DispatchOfIlwalidUnit,
                UT_CASE_SET_DESCRIPTION("Tests dispatching to an invalid RM_PMU_UNIT")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_DISPATCH, DispatchIlwalidDmemPtr,
                UT_CASE_SET_DESCRIPTION("Tests dispatching with an invalid PMU RPC structure")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_DISPATCH, DispatchNullPointer,
                UT_CASE_SET_DESCRIPTION("Tests dispatching when a NULL pointer is provided")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Tests dispatching to RM_PMU_UNIT_CMDMGMT
 *
 * @details Ensures that the CmdMgmt task will process commands sent to
 *          @ref RM_PMU_UNIT_CMDMGMT
 */
UT_CASE_RUN(PMU_CMDMGMT_DISPATCH, DispatchOfCmdMgmt)
{
    FLCN_STATUS status;
    DECLARE_COMMAND_AND_REQUEST(command, request, RM_PMU_UNIT_CMDMGMT);

    cmdmgmtRpcMockInit();
    pmuRpcProcessUnitCmdmgmt_MOCK_fake.return_val = FLCN_OK;

    status = cmdmgmtExtCmdDispatch_IMPL(&request);

    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_RPC))
    {
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
        UT_ASSERT_EQUAL_UINT(pmuRpcProcessUnitCmdmgmt_MOCK_fake.call_count, 1U);
        UT_ASSERT_EQUAL_PTR(pmuRpcProcessUnitCmdmgmt_MOCK_fake.arg0_val, &request);
    }
    else
    {
        UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
        UT_ASSERT_EQUAL_UINT(pmuRpcProcessUnitCmdmgmt_MOCK_fake.call_count, 0U);
    }
}

/*!
 * @brief   Tests dispatching to a valid non-CMDMGMT @ref RM_PMU_UNIT
 *
 * @detils  Uses a valid @ref RM_PMU_UNIT to test the general dispatching code
 */
UT_CASE_RUN(PMU_CMDMGMT_DISPATCH, DispatchOfValidUnit)
{
    /*!
     * @brief   @ref RM_PMU_UNIT to which to dispatch and expected result
     *
     * @details Ideally, we would have a @ref RM_PMU_UNIT/task that is supported
     *          on all PMU profiles. Unforunately, however, that is not the
     *          case. (CmdMgmt is supported on all profiles, but it is
     *          dispatched to differently than other tasks.) THERM
     *          comes close, but it is disabled on 'pmu-g*b' profiles.
     *          Therefore, we configure this struct to whichever one is enabled,
     *          and ct_assert if neither is to ensure that the test could be
     *          appropriately updated.
     */
    const struct
    {
        LwU8 unitId;
        LwU8 queueId;
    } expectedResult =
    {
#if PMUCFG_FEATURE_ENABLED(PMUTASK_SEQ)
        .unitId = RM_PMU_UNIT_SEQ,
        .queueId = LWOS_QUEUE_ID(PMU, SEQ),
#elif PMUCFG_FEATURE_ENABLED(PMUTASK_THERM)
        .unitId = RM_PMU_UNIT_THERM,
        .queueId = LWOS_QUEUE_ID(PMU, THERM),
#else
        //
        // If this assertion is hit, the PMU profile under test does not support
        // either RM_PMU_UNIT_SEQ or RM_PMU_UNIT_THERM, which are used as "test
        // options" here. Please replace one of the above task options with one
        // that is common to all of the profiles supporting that task *and* the
        // failing profile, or add a new #elif condition for a task supported by
        // the failing profile.
        //
        ct_assert(PMUCFG_FEATURE_ENABLED(PMUTASK_SEQ) ||
                  PMUCFG_FEATURE_ENABLED(PMUTASK_THERM));
#endif // PMUCFG_FEATURE_ENABLED(PMUTASK_SEQ)
    };

    FLCN_STATUS status;
    DECLARE_COMMAND_AND_REQUEST(command, request, expectedResult.unitId);

    lwrtosMockInit();
    lwrtosQueueIdSendBlocking_MOCK_fake.return_val = FLCN_OK;

    status = cmdmgmtExtCmdDispatch_IMPL(&request);

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(lwrtosQueueIdSendBlocking_MOCK_fake.call_count, 1U);
    UT_ASSERT_EQUAL_UINT(lwrtosQueueIdSendBlocking_MOCK_fake.arg0_val, expectedResult.queueId);
    UT_ASSERT_EQUAL_PTR(lwrtosQueueIdSendBlocking_MOCK_fake.arg1_val, &request);
    UT_ASSERT_EQUAL_UINT(lwrtosQueueIdSendBlocking_MOCK_fake.arg2_val, sizeof(request));
}

/*!
 * @brief   Tests dispatching to an invalid @ref RM_PMU_UNIT
 *
 * @details Tries dispatching a command with an invalid destination unit and
 *          ensures that the dispatch fails.
 */
UT_CASE_RUN(PMU_CMDMGMT_DISPATCH, DispatchOfIlwalidUnit)
{
    FLCN_STATUS status;
    DECLARE_COMMAND_AND_REQUEST(command, request, RM_PMU_UNIT_END);

    lwrtosMockInit();
    lwrtosQueueIdSendBlocking_MOCK_fake.return_val = FLCN_OK;

    status = cmdmgmtExtCmdDispatch_IMPL(&request);

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(lwrtosQueueIdSendBlocking_MOCK_fake.call_count, 0U);
}

/*!
 * @brief   Tests dispatching with an invalid PMU RPC structure
 *
 * @details The dispatch code is the centralized location where DMEM pointers
 *          received from RM are validated. This test ensures that dispatch
 *          fails if the DMEM pointer is invalid
 */
UT_CASE_RUN(PMU_CMDMGMT_DISPATCH, DispatchIlwalidDmemPtr)
{
    FLCN_STATUS status;
    DECLARE_COMMAND_AND_REQUEST(command, request, RM_PMU_UNIT_CMDMGMT);

    // Make the DMEM pointer invalid
    command.cmd.rpc.rpcDmemPtr++;

    status = cmdmgmtExtCmdDispatch_IMPL(&request);

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief   Tests dispatching when a NULL pointer is provided
 */
UT_CASE_RUN(PMU_CMDMGMT_DISPATCH, DispatchNullPointer)
{
    //
    // TODO: uncomment once the NULL check has been put back in
    // cmdmgmtExtCmdDispatch_IMPL. It has lwrrently been removed because it was
    // causing some failures on GK104.
    //
//    FLCN_STATUS status;
//
//    status = cmdmgmtExtCmdDispatch_IMPL(NULL);
//
//    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);
}
