/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    task_cmdmgmt-test.c
 * @brief   Unit tests for logic in task_cmdmgmt.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "cmdmgmt/cmdmgmt_dispatch-mock.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "cmdmgmt/cmdmgmt_supersurface-mock.h"
#include "config/g_profile.h"
#include "dmemovl-mock.h"
#include "g_pmurpc.h"
#include "main_init_api.h"
#include "pmu_rtos_queues.h"
#include "lwos_dma-mock.h"
#include "lwostask-mock.h"
#include "lwrtos-mock.h"
#include "test-macros.h"

//
// SuperSurface includes. The SuperSurface header is not self-standing right
// now, so the order here is important.
//
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifi2c.h"
#include "pmu/pmuif_pg_gc6.h"
#include "pmu/pmuifspi.h"
#include "rmpmucmdif.h"
#include "rmpmusupersurfif.h"


#include "dev_pwr_csb.h"

/* ------------------------ Mocked Functions -------------------------------- */
OSTASK_OVL_DESC _overlay_id_imem_cmdmgmtMisc = 0;
LwrtosTaskHandle OsTaskCmdMgmt;
LwrtosQueueHandle UcodeQueueIdToQueueHandleMap[PMU_QUEUE_ID__COUNT] = {0U};
RM_PMU_CMD_LINE_ARGS PmuInitArgs;

/*!
 * @brief   "Mocked" implementation of the FB flush routine.
 *
 * @details This mocked copy is made local to this file because it is not used
 *          anywhere else and is unlikely to be tested soon. Note that this is
 *          just the "regular" function without a "_MOCK" suffix. This is
 *          because chip-config does not lwrrently support (as far as I know) a
 *          simple way to support "mocking" as the PMU has been doing it.
 *          Therefore, because this function is unlikely to be tested, it's
 *          mock is just defined as the "real" thing. Once this needs to be
 *          tested (or anything in its file does), "mocking" for HALs will need
 *          to be solved.
 */
void
lsfCopyBrssData_GM20X
(
    RM_PMU_BSI_RAM_SELWRE_SCRATCH_DATA *pBrssData
)
{
}

/* ------------------------ Externs/Prototypes ------------------------------ */
/*!
 * @defgroup    CmdMgmtTaskPrivateFunctions
 *
 * @details These are static variables/functions that really "should" be private
 *          to task_cmdmgmt.c. They are declared here so that they can be
 *          accessed by the tests. Ideally, the code and/or tests will be
 *          refactored in the future to not require this.
 * @{
 */
extern void s_cmdmgmtInit(void);
extern FLCN_STATUS s_cmdmgmtProcessEvent(DISPATCH_CMDMGMT *pDispatch);
/*!@}*/

/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_CMDMGMT_TASK,
                UT_SUITE_SET_COMPONENT("CmdMgmt")
                UT_SUITE_SET_DESCRIPTION("Tests Unit CmdMgmt Task of Component CmdMgmt")
                UT_SUITE_SET_OWNER("aherring"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskPreInitTaskSuccessful,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task pre-initialization")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskPreInitTaskTaskCreateFail,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task pre-initialization if OSTASK_CREATE fails")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskPreInitTaskQueueCreateFail,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task pre-initialization if lwrtosQueueCreateOvlRes fails")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskInitSuccessful,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task initialization succeeds")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskInitDmaFail,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task initialization succeeds even if SuperSurfaceFbOffset initialization fails")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessSingleCommand,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing a single command")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessMultipleCommands,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing multiple commands on the same queue")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessLpqThenHpq,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing behavior when commands queued to both high priority and low priority queues")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessCommandWraparound,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing commands until the FbQueue is exhausted")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessDispatchFailure,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing a command when dispatch returns a failure")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessSignalValidSignal,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing a valid signal for PBI")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessSignalIlwalidSignal,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing an invalid signal for PBI")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessEventIlwalid,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task processing an invalid event")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_CMDMGMT_TASK, CmdMgmtTaskInitDoneRpc,
                UT_CASE_SET_DESCRIPTION("Tests CmdMgmt Task's PMU_INIT_DONE RPC")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Initial value to be assigned to @ref OsTaskCmdMgmt; test should
 *          ensure this value changes.
 */
static const LwrtosTaskHandle
CmdMgmtTaskPreInitTaskSuccessfulTaskHandleInit = ((LwrtosTaskHandle)~0U);

/*!
 * @brief   Initial value to be assigned to @ref LWOS_QUEUE(PMU, CMDMGMT); test
 *          should ensure this value changes.
 */
static const LwrtosQueueHandle
CmdMgmtTaskPreInitTaskSuccessfulQueueHandleInit = ((LwrtosQueueHandle)~0U);

/*!
 * @brief   Sets up the state for CmdMgmtTaskPreInitTaskSuccessful test
 *
 * @details Initializes:
 *              LWOS task mocking
 *              LWRTOS mocking
 *              @ref OsTaskCmdMgmt based on @ref CmdMgmtTaskPreInitTaskSuccessfulTaskHandleInit
 *              @ref LWOS_QUEUE(PMU, CMDMGMT) based on CmdMgmtTaskPreInitTaskSuccessfulQueueHandleInit
 */
static void
CmdMgmtTaskPreInitTaskSuccessfulPreTest(void)
{
    lwosTaskMockInit();
    lwrtosMockInit();

    OsTaskCmdMgmt =  CmdMgmtTaskPreInitTaskSuccessfulTaskHandleInit;
    LWOS_QUEUE(PMU, CMDMGMT) = CmdMgmtTaskPreInitTaskSuccessfulQueueHandleInit;
}

/*!
 * @brief   Tests CmdMgmt Task pre-initialization
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskPreInitTaskSuccessful)
{
    FLCN_STATUS status;

    CmdMgmtTaskPreInitTaskSuccessfulPreTest();

    status = cmdmgmtPreInitTask_IMPL();

    UT_ASSERT(status == FLCN_OK);
    UT_ASSERT(OsTaskCmdMgmt != CmdMgmtTaskPreInitTaskSuccessfulTaskHandleInit);
    UT_ASSERT(LWOS_QUEUE(PMU, CMDMGMT) != CmdMgmtTaskPreInitTaskSuccessfulQueueHandleInit);
}

/*!
 * @brief   Sets up the state for CmdMgmtTaskPreInitTaskTaskCreateFail test
 *
 * @details Initializes:
 *              LWOS task mocking
 *              LWRTOS mocking
 *              @ref lwosTaskCreate error code (via @ref LwosTaskMockConfig)
 */
static void
CmdMgmtTaskPreInitTaskTaskCreateFailPreTest(void)
{
    lwosTaskMockInit();
    lwrtosMockInit();

    LwosTaskMockConfig.lwosTaskCreateConfig.errorCodes[0U] = FLCN_ERR_ILWALID_STATE;
}

/*!
 * @brief   Tests CmdMgmt Task pre-initialization if OSTASK_CREATE fails
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskPreInitTaskTaskCreateFail)
{
    FLCN_STATUS status;

    CmdMgmtTaskPreInitTaskTaskCreateFailPreTest();

    status = cmdmgmtPreInitTask_IMPL();

    UT_ASSERT(status != FLCN_OK);
}

/*!
 * @brief   Sets up the state for CmdMgmtTaskPreInitTaskQueueCreateFail test
 *
 * @details Initializes:
 *              LWOS task mocking
 *              LWRTOS mocking
 *              @ref lwrtosQueueCreateOvlRes error code (via @ref LwrtosMockQueueConfig)
 */
static void
CmdMgmtTaskPreInitTaskQueueCreateFailPreTest(void)
{
    lwosTaskMockInit();
    lwrtosMockInit();

    LwrtosMockQueueConfig.lwrtosQueueCreateOvlResConfig.errorCodes[0U] = FLCN_ERR_ILWALID_STATE;
}

/*!
 * @brief   Tests CmdMgmt Task pre-initialization if lwrtosQueueCreateOvlRes fails
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskPreInitTaskQueueCreateFail)
{
    FLCN_STATUS status;

    CmdMgmtTaskPreInitTaskQueueCreateFailPreTest();

    status = cmdmgmtPreInitTask_IMPL();

    UT_ASSERT(status != FLCN_OK);
}

/*!
 * @brief   Mock value from which to DMA to @ref SuperSurfaceFbOffset
 */
static RM_FLCN_U64
CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset =
{
    .lo = 0xfeedfeed,
    .hi = 0xbeefbeef,
};

/*!
 * @brief   Mock memory region from which to DMA @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset
 */
static DMA_MOCK_MEMORY_REGION
CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffsetRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
    .sizeOfData = sizeof(CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset),
};

/*!
 * @brief   Mock driver-managed heap offset to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskInitSuccessfulRmManagedAreaOffset = 0xB7U;

/*!
 * @brief   Mock driver-managed heap size to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskInitSuccessfulRmManagedAreaSize = 0xE9U;

/*!
 * @brief   Sets up the state for CmdMgmtTaskInitSuccessful test
 *
 * @details Initializes:
 *              CmdMgmt SuperSurface mocking
 *              DMA mocking
 *                  - A mock memory region based on @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffsetRegion
 *              CmdMgmt FbQueue mocking
 *                  - The @ref cmdmgmtRMHInitMsgPopulate return values based on
 *                    @ref CmdMgmtTaskInitSuccessfulRmManagedAreaOffset and
 *                    @ref CmdMgmtTaskInitSuccessfulRmManagedAreaSize (via
 *                    @ref CmdMgmtFbqMockConfig)
 *              CmdMgmt RPC mocking
 *
 * @note    The "real" implementations of @ref pmuQueuePmuToRmInit_GMXXX, @ref pmuQueuePmuToRmHeadGet_GMXXX,
 *          @ref pmuQueuePmuToRmHeadSet_GMXXX, and @ref pmuQueuePmuToRmTailGet_GMXXX are used/referred to here.
 */
static void
CmdMgmtTaskInitSuccessfulPreTest(void)
{
    // Initialize super-surface mocking
    cmdmgmtSuperSurfaceMockInit();

    // Initialize DMA mocking
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffsetRegion);

    // Initialize RPC mocking
    cmdmgmtRpcMockInit();
}

/*!
 * @brief   Tests CmdMgmt Task initialization succeeds
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskInitSuccessful)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;

    CmdMgmtTaskInitSuccessfulPreTest();

    s_cmdmgmtInit();

    UT_ASSERT(cmdmgmtRpcMockGetSentRpc(&rpc.hdr, sizeof(rpc)) == FLCN_OK);

    // Ensure that initialization succeeded
    UT_ASSERT(rpc.status == FLCN_OK);

    // Ensure that the RPC message contains expected data
    UT_ASSERT(rpc.rmManagedAreaOffset == CmdMgmtTaskInitSuccessfulRmManagedAreaOffset);
    UT_ASSERT(rpc.rmManagedAreaSize == CmdMgmtTaskInitSuccessfulRmManagedAreaSize);

    // Check that SuperSurface has been initialized
    UT_ASSERT(cmdmgmtSuperSurfaceMockMemberDescriptorInitGetNumCalls() == 1U);
}

/*!
 * @brief   Mock value from which to DMA to @ref SuperSurfaceFbOffset
 */
static RM_FLCN_U64
CmdMgmtTaskInitDmaFailSuperSurfaceFbOffset =
{
    .lo = 0xfeedfeed,
    .hi = 0xbeefbeef,
};

/*!
 * @brief   Mock memory region from which to DMA @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset
 */
static DMA_MOCK_MEMORY_REGION
CmdMgmtTaskInitDmaFailSuperSurfaceFbOffsetRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdMgmtTaskInitDmaFailSuperSurfaceFbOffset,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
    .sizeOfData = sizeof(CmdMgmtTaskInitDmaFailSuperSurfaceFbOffset),
};

/*!
 * @brief   Mock driver-managed heap offset to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskInitDmaFailRmManagedAreaOffset = 0xB7U;

/*!
 * @brief   Mock driver-managed heap size to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskInitDmaFailRmManagedAreaSize = 0xE9U;

/*!
 * @brief   Sets up the state for CmdMgmtTaskInitDmaFail test
 *
 * @details Initializes:
 *              CmdMgmt SuperSurface mocking
 *              DMA mocking
 *                  - A mock memory region based on @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffsetRegion
 *                  - A return error code for @ref dmaRead
 *              CmdMgmt FbQueue mocking
 *                  - The @ref cmdmgmtRMHInitMsgPopulate return values based on
 *                    @ref CmdMgmtTaskInitSuccessfulRmManagedAreaOffset and
 *                    @ref CmdMgmtTaskInitSuccessfulRmManagedAreaSize (via
 *                    @ref CmdMgmtFbqMockConfig)
 *              CmdMgmt RPC mocking
 *
 * @note    The "real" implementations of @ref pmuQueuePmuToRmInit_GMXXX, @ref pmuQueuePmuToRmHeadGet_GMXXX,
 *          @ref pmuQueuePmuToRmHeadSet_GMXXX, and @ref pmuQueuePmuToRmTailGet_GMXXX are used/referred to here.
 */
static void
CmdMgmtTaskInitDmaFailPreTest(void)
{
    // Initialize super-surface mocking
    cmdmgmtSuperSurfaceMockInit();

    // Initialize DMA mocking
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdMgmtTaskInitDmaFailSuperSurfaceFbOffsetRegion);
    dmaMockReadAddEntry(0U, FLCN_ERR_ILWALID_STATE);

    // Initialize RPC mocking
    cmdmgmtRpcMockInit();
}

/*!
 * @brief   Tests CmdMgmt Task initialization succeeds even if SuperSurfaceFbOffset initialization fails
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskInitDmaFail)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;

    CmdMgmtTaskInitDmaFailPreTest();

    s_cmdmgmtInit();

    UT_ASSERT(cmdmgmtRpcMockGetSentRpc(&rpc.hdr, sizeof(rpc)) == FLCN_OK);

    // Ensure that initialization succeeded
    UT_ASSERT(rpc.status == FLCN_OK);

    // Ensure that the RPC message contains expected data
    UT_ASSERT(rpc.rmManagedAreaOffset == CmdMgmtTaskInitDmaFailRmManagedAreaOffset);
    UT_ASSERT(rpc.rmManagedAreaSize == CmdMgmtTaskInitDmaFailRmManagedAreaSize);

    // Check that SuperSurface has been initialized
    if (PMUCFG_FEATURE_ENABLED(PMU_CMDMGMT_SUPER_SURFACE_MEMBER_DESCRIPTORS))
    {
        UT_ASSERT(cmdmgmtSuperSurfaceMockMemberDescriptorInitGetNumCalls() == 1U);
    }
}

/*!
 * @brief   Mock value from which to DMA to @ref SuperSurfaceFbOffset
 */
static RM_FLCN_U64
CmdMgmtTaskProcessSingleCommandSuperSurfaceFbOffset =
{
    .lo = 0xfeedfeed,
    .hi = 0xbeefbeef,
};

/*!
 * @brief   Mock memory region from which to DMA @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset
 */
static DMA_MOCK_MEMORY_REGION
CmdMgmtTaskProcessSingleCommandSuperSurfaceFbOffsetRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdMgmtTaskProcessSingleCommandSuperSurfaceFbOffset,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
    .sizeOfData = sizeof(CmdMgmtTaskProcessSingleCommandSuperSurfaceFbOffset),
};

/*!
 * @brief   Mock driver-managed heap offset to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessSingleCommandRmManagedAreaOffset = 0xB7U;

/*!
 * @brief   Mock driver-managed heap size to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessSingleCommandRmManagedAreaSize = 0xE9U;

/*!
 * @brief   Physical ID of the queue being sent to; initialized during pre-test
 */
static LwU8
CmdMgmtTaskProcessSingleCommandPhysicalQueue;

/*!
 * @brief   Mock @ref RM_FLCN_QUEUE_HDR to return from @ref osCmdmgmtFbqCmdCopyIn
 */
static const RM_FLCN_QUEUE_HDR
CmdMgmtTaskProcessSingleCommandFlcnQueueHeader =
{
    .unitId = RM_PMU_UNIT_CMDMGMT,
    .size = 0x10,
    .ctrlFlags = 0U,
    .seqNumId = 0U,
};

/*!
 * @brief   Sets up the state for CmdMgmtTaskProcessSingleCommand test
 *
 * @details Initializes:
 *              CmdMgmt SuperSurface mocking
 *              DMA mocking
 *                  - A mock memory region based on @ref CmdMgmtTaskProcessSingleCommandSuperSurfaceFbOffsetRegion
 *              CmdMgmt FbQueue mocking
 *                  - The @ref cmdmgmtRMHInitMsgPopulate return values based on
 *                    @ref CmdMgmtTaskProcessSingleCommandRmManagedAreaOffset and
 *                    @ref CmdMgmtTaskProcessSingleCommandRmManagedAreaSize (via
 *                    @ref CmdMgmtFbqMockConfig)
 *              CmdMgmt RPC mocking
 *              CmdMgmt Dispatch mocking
 *              The CmdMgmt Task itself by calling @ref s_cmdmgmtInit directly
 *              @ref CmdMgmtTaskProcessSingleCommandPhysicalQueue using the RPC
 *                  sent by @ref s_cmdmgmtInit
 *              The LW_CPWR_PMU_CMD_HEAD_INTRSTAT register to indicate an interrupt for
 *                  @ref CmdMgmtTaskProcessSingleCommandPhysicalQueue
 *              The LW_CPWR_PMU_QUEUE_HEAD register to indicate one new message
 *                  is available
 *              One mock FbQueue element based on @ref CmdMgmtTaskProcessSingleCommandFlcnQueueHeader
 *                  and at queue @ref CmdMgmtTaskProcessSingleCommandPhysicalQueue
 *
 * @note    The "real" implementations of @ref pmuQueuePmuToRmInit_GMXXX, @ref pmuQueuePmuToRmHeadGet_GMXXX,
 *          @ref pmuQueuePmuToRmHeadSet_GMXXX, and @ref pmuQueuePmuToRmTailGet_GMXXX are used/referred to here.
 */
static void
CmdMgmtTaskProcessSingleCommandPreTest(void)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;
    LwU32 queueHead;
    FLCN_STATUS status;

    // Initialize super-surface mocking
    cmdmgmtSuperSurfaceMockInit();

    // Initialize DMA mocking
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdMgmtTaskProcessSingleCommandSuperSurfaceFbOffsetRegion);

    // Initialize RPC mocking
    cmdmgmtRpcMockInit();

    // Initialize dispatch mocking
    cmdmgmtDispatchMockInit();

    //
    // Initialize the task itself and retrieve initialization data out of the
    // RPC.
    //
    s_cmdmgmtInit();
    UT_ASSERT(cmdmgmtRpcMockGetSentRpc(&rpc.hdr, sizeof(rpc)) == FLCN_OK);
    UT_ASSERT(rpc.status == FLCN_OK);

    CmdMgmtTaskProcessSingleCommandPhysicalQueue = RM_PMU_COMMAND_QUEUE_HPQ;

    // Mark the interrupt pending for this queue
    REG_WR32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT, LWBIT32(CmdMgmtTaskProcessSingleCommandPhysicalQueue));

    // Get what the initialization says the head is
    queueHead = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessSingleCommandPhysicalQueue));

    // Add the mock FbQueue element for this queue/position
    status = cmdmgmtFbqMockCmdAddElement(&CmdMgmtTaskProcessSingleCommandFlcnQueueHeader,
                                         CmdMgmtTaskProcessSingleCommandPhysicalQueue,
                                         queueHead);
    UT_ASSERT(status == FLCN_OK);

    // Increment the head
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessSingleCommandPhysicalQueue), queueHead + 1U);
}

/*!
 * @brief   Tests CmdMgmt Task processing a single command
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessSingleCommand)
{
    DISP2UNIT_RM_RPC cmd;

    CmdMgmtTaskProcessSingleCommandPreTest();

    s_cmdmgmtProcessEvent(
        &(DISPATCH_CMDMGMT){ .disp2unitEvt = DISP2UNIT_EVT_RM_RPC, });

    // Ensure that interrupts have been re-enabled
    UT_ASSERT(REG_RD32(CSB, LW_CPWR_PMU_CMD_INTREN) == 7U);

    // Ensure that dispatching happened and get the dispatched item
    UT_ASSERT(cmdmgmtDispatchMockGetDispatched(&cmd) == FLCN_OK);

    // Ensure that the command we retrieved has the expected values
    UT_ASSERT(cmd.hdr.eventType == DISP2UNIT_EVT_RM_RPC);
    UT_ASSERT(cmd.hdr.unitId == CmdMgmtTaskProcessSingleCommandFlcnQueueHeader.unitId);
    UT_ASSERT(cmd.cmdQueueId == CmdMgmtTaskProcessSingleCommandPhysicalQueue);
    UT_ASSERT(memcmp(&cmd.pRpc->hdr,
                     &CmdMgmtTaskProcessSingleCommandFlcnQueueHeader,
                     sizeof(CmdMgmtTaskProcessSingleCommandFlcnQueueHeader)) == 0);
}

/*!
 * @brief   Mock value from which to DMA to @ref SuperSurfaceFbOffset
 */
static RM_FLCN_U64
CmdMgmtTaskProcessMultipleCommandsSuperSurfaceFbOffset =
{
    .lo = 0xfeedfeed,
    .hi = 0xbeefbeef,
};

/*!
 * @brief   Mock memory region from which to DMA @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset
 */
static DMA_MOCK_MEMORY_REGION
CmdMgmtTaskProcessMultipleCommandsSuperSurfaceFbOffsetRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdMgmtTaskProcessMultipleCommandsSuperSurfaceFbOffset,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
    .sizeOfData = sizeof(CmdMgmtTaskProcessMultipleCommandsSuperSurfaceFbOffset),
};

/*!
 * @brief   Mock driver-managed heap offset to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessMultipleCommandsRmManagedAreaOffset = 0xB7U;

/*!
 * @brief   Mock driver-managed heap size to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessMultipleCommandsRmManagedAreaSize = 0xE9U;

/*!
 * @brief   Physical ID of the queue being sent to; initialized during pre-test
 */
static LwU8
CmdMgmtTaskProcessMultipleCommandsPhysicalQueue;

/*!
 * @brief   Mock @ref RM_FLCN_QUEUE_HDR to return from @ref osCmdmgmtFbqCmdCopyIn
 */
static const RM_FLCN_QUEUE_HDR
CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders[] =
{
    {
        .unitId = RM_PMU_UNIT_CMDMGMT,
        .size = 0x10,
        .ctrlFlags = 0U,
        .seqNumId = 0U,
    },
    {
        .unitId = RM_PMU_UNIT_THERM,
        .size = 0x38,
        .ctrlFlags = 9U,
        .seqNumId = 7U,
    },
    {
        .unitId = RM_PMU_UNIT_PERF,
        .size = 0x99,
        .ctrlFlags = 4U,
        .seqNumId = 2U,
    },
};

/*!
 * @brief   Sets up the state for CmdMgmtTaskProcessMultipleCommands test
 *
 * @details Initializes:
 *              CmdMgmt SuperSurface mocking
 *              DMA mocking
 *                  - A mock memory region based on @ref CmdMgmtTaskProcessMultipleCommandsSuperSurfaceFbOffsetRegion
 *              CmdMgmt FbQueue mocking
 *                  - The @ref cmdmgmtRMHInitMsgPopulate return values based on
 *                    @ref CmdMgmtTaskProcessMultipleCommandsRmManagedAreaOffset and
 *                    @ref CmdMgmtTaskProcessMultipleCommandsRmManagedAreaSize (via
 *                    @ref CmdMgmtFbqMockConfig)
 *              CmdMgmt RPC mocking
 *              CmdMgmt Dispatch mocking
 *              The CmdMgmt Task itself by calling @ref s_cmdmgmtInit directly
 *              @ref CmdMgmtTaskProcessMultipleCommandsPhysicalQueue using the RPC
 *                  sent by @ref s_cmdmgmtInit
 *              The LW_CPWR_PMU_CMD_HEAD_INTRSTAT register to indicate an interrupt for
 *                  @ref CmdMgmtTaskProcessMultipleCommandsPhysicalQueue
 *              The LW_CPWR_PMU_QUEUE_HEAD register to indicate multiple new messages
 *                  are available
 *              Multiple mock FbQueue elements based on @ref CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders
 *                  and at queue @ref CmdMgmtTaskProcessMultipleCommandsPhysicalQueue
 *
 * @note    The "real" implementations of @ref pmuQueuePmuToRmInit_GMXXX, @ref pmuQueuePmuToRmHeadGet_GMXXX,
 *          @ref pmuQueuePmuToRmHeadSet_GMXXX, and @ref pmuQueuePmuToRmTailGet_GMXXX are used/referred to here.
 */
static void
CmdMgmtTaskProcessMultipleCommandsPreTest(void)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;
    LwU32 queueHead;
    FLCN_STATUS status;

    // Initialize super-surface mocking
    cmdmgmtSuperSurfaceMockInit();

    // Initialize DMA mocking
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdMgmtTaskProcessMultipleCommandsSuperSurfaceFbOffsetRegion);

    // Initialize RPC mocking
    cmdmgmtRpcMockInit();

    // Initialize dispatch mocking
    cmdmgmtDispatchMockInit();

    //
    // Initialize the task itself and retrieve initialization data out of the
    // RPC.
    //
    s_cmdmgmtInit();
    UT_ASSERT(cmdmgmtRpcMockGetSentRpc(&rpc.hdr, sizeof(rpc)) == FLCN_OK);
    UT_ASSERT(rpc.status == FLCN_OK);

    CmdMgmtTaskProcessMultipleCommandsPhysicalQueue = RM_PMU_COMMAND_QUEUE_HPQ;

    // Mark the interrupt pending for this queue
    REG_WR32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT, LWBIT32(CmdMgmtTaskProcessMultipleCommandsPhysicalQueue));

    // Get what the initialization says the head is
    queueHead = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessMultipleCommandsPhysicalQueue));
    for (queueHead = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessMultipleCommandsPhysicalQueue));
         queueHead < LW_ARRAY_ELEMENTS(CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders);
         queueHead++)
    {
        // Add the mock FbQueue element for this queue/position
        status = cmdmgmtFbqMockCmdAddElement(&CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders[queueHead],
                                             CmdMgmtTaskProcessMultipleCommandsPhysicalQueue,
                                             queueHead);
        UT_ASSERT(status == FLCN_OK);
    }

    // Increment the head
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessMultipleCommandsPhysicalQueue), queueHead);
}

/*!
 * @brief   Tests CmdMgmt Task processing multiple commands on the same queue
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessMultipleCommands)
{
    LwLength i;

    CmdMgmtTaskProcessMultipleCommandsPreTest();

    s_cmdmgmtProcessEvent(
        &(DISPATCH_CMDMGMT){ .disp2unitEvt = DISP2UNIT_EVT_RM_RPC, });

    // Ensure that interrupts have been re-enabled
    UT_ASSERT(REG_RD32(CSB, LW_CPWR_PMU_CMD_INTREN) == 7U);

    for (i = 0U;
         i < LW_ARRAY_ELEMENTS(CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders);
         i++)
    {
        DISP2UNIT_RM_RPC cmd;

        // Ensure that dispatching happened and get the dispatched item
        UT_ASSERT(cmdmgmtDispatchMockGetDispatched(&cmd) == FLCN_OK);

        // Ensure that the command we retrieved has the expected values
        UT_ASSERT(cmd.hdr.eventType == DISP2UNIT_EVT_RM_RPC);
        UT_ASSERT(cmd.hdr.unitId == CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders[i].unitId);
        UT_ASSERT(cmd.cmdQueueId == CmdMgmtTaskProcessMultipleCommandsPhysicalQueue);
        UT_ASSERT(memcmp(&cmd.pRpc->hdr,
                         &CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders[i],
                         sizeof(CmdMgmtTaskProcessMultipleCommandsFlcnQueueHeaders[i])) == 0);
    }
}

/*!
 * @brief   Mock value from which to DMA to @ref SuperSurfaceFbOffset
 */
static RM_FLCN_U64
CmdMgmtTaskProcessLpqThenHpqSuperSurfaceFbOffset =
{
    .lo = 0xfeedfeed,
    .hi = 0xbeefbeef,
};

/*!
 * @brief   Mock memory region from which to DMA @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset
 */
static DMA_MOCK_MEMORY_REGION
CmdMgmtTaskProcessLpqThenHpqSuperSurfaceFbOffsetRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdMgmtTaskProcessLpqThenHpqSuperSurfaceFbOffset,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
    .sizeOfData = sizeof(CmdMgmtTaskProcessLpqThenHpqSuperSurfaceFbOffset),
};

/*!
 * @brief   Mock driver-managed heap offset to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessLpqThenHpqRmManagedAreaOffset = 0xB7U;

/*!
 * @brief   Mock driver-managed heap size to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessLpqThenHpqRmManagedAreaSize = 0xE9U;

/*!
 * @brief   Logical ID of the queues being sent to
 */
static const LwU8
CmdMgmtTaskProcessLpqThenHpqLogicalQueues[] =
{
    RM_PMU_COMMAND_QUEUE_LPQ,
    RM_PMU_COMMAND_QUEUE_HPQ,
};

/*!
 * @brief   Physical ID of the queues being sent to; initialized during pre-test
 */
static LwU8
CmdMgmtTaskProcessLpqThenHpqPhysicalQueues[
    LW_ARRAY_ELEMENTS(CmdMgmtTaskProcessLpqThenHpqLogicalQueues)];

/*!
 * @brief   Mock @ref RM_FLCN_QUEUE_HDR to return from @ref osCmdmgmtFbqCmdCopyIn
 */
static const RM_FLCN_QUEUE_HDR
CmdMgmtTaskProcessLpqThenHpqFlcnQueueHeaders[] =
{
    {
        .unitId = RM_PMU_UNIT_THERM,
        .size = 0x38,
        .ctrlFlags = 9U,
        .seqNumId = 7U,
    },
    {
        .unitId = RM_PMU_UNIT_PERF,
        .size = 0x99,
        .ctrlFlags = 4U,
        .seqNumId = 2U,
    },
};

/*!
 * @brief   Sets up the state for CmdMgmtTaskProcessLpqThenHpq test
 *
 * @details Initializes:
 *              CmdMgmt SuperSurface mocking
 *              DMA mocking
 *                  - A mock memory region based on @ref CmdMgmtTaskProcessLpqThenHpqSuperSurfaceFbOffsetRegion
 *              CmdMgmt FbQueue mocking
 *                  - The @ref cmdmgmtRMHInitMsgPopulate return values based on
 *                    @ref CmdMgmtTaskProcessLpqThenHpqRmManagedAreaOffset and
 *                    @ref CmdMgmtTaskProcessLpqThenHpqRmManagedAreaSize (via
 *                    @ref CmdMgmtFbqMockConfig)
 *              CmdMgmt RPC mocking
 *              CmdMgmt Dispatch mocking
 *              The CmdMgmt Task itself by calling @ref s_cmdmgmtInit directly
 *              @ref CmdMgmtTaskProcessLpqThenHpqPhysicalQueues using the RPC
 *                  sent by @ref s_cmdmgmtInit
 *              The LW_CPWR_PMU_CMD_HEAD_INTRSTAT register to indicate interrupts for
 *                  @ref CmdMgmtTaskProcessLpqThenHpqPhysicalQueues
 *              The LW_CPWR_PMU_QUEUE_HEAD registers to indicate multiple new messages
 *                  are available
 *              Multiple mock FbQueue elements based on @ref CmdMgmtTaskProcessLpqThenHpqFlcnQueueHeaders
 *                  and @ref CmdMgmtTaskProcessLpqThenHpqPhysicalQueues
 *
 * @note    The "real" implementations of @ref pmuQueuePmuToRmInit_GMXXX, @ref pmuQueuePmuToRmHeadGet_GMXXX,
 *          @ref pmuQueuePmuToRmHeadSet_GMXXX, and @ref pmuQueuePmuToRmTailGet_GMXXX are used/referred to here.
*/
static void
CmdMgmtTaskProcessLpqThenHpqPreTest(void)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;
    FLCN_STATUS status;
    LwLength i;

    // Initialize super-surface mocking
    cmdmgmtSuperSurfaceMockInit();

    // Initialize DMA mocking
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdMgmtTaskProcessLpqThenHpqSuperSurfaceFbOffsetRegion);

    // Initialize RPC mocking
    cmdmgmtRpcMockInit();

    // Initialize dispatch mocking
    cmdmgmtDispatchMockInit();

    //
    // Initialize the task itself and retrieve initialization data out of the
    // RPC.
    //
    s_cmdmgmtInit();
    UT_ASSERT(cmdmgmtRpcMockGetSentRpc(&rpc.hdr, sizeof(rpc)) == FLCN_OK);
    UT_ASSERT(rpc.status == FLCN_OK);

    REG_WR32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT, 0U);
    for (i = 0U;
         i < LW_ARRAY_ELEMENTS(CmdMgmtTaskProcessLpqThenHpqPhysicalQueues);
         i++)
    {
        const LwU32 intrStat = REG_RD32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT);
        const LwU8 logicalId = CmdMgmtTaskProcessLpqThenHpqLogicalQueues[i];

        CmdMgmtTaskProcessLpqThenHpqPhysicalQueues[i] = logicalId;

        // Mark the interrupt pending for this queue
        REG_WR32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT, intrStat | LWBIT32(CmdMgmtTaskProcessLpqThenHpqPhysicalQueues[i]));
    }

    for (i = 0U;
         i < LW_ARRAY_ELEMENTS(CmdMgmtTaskProcessLpqThenHpqPhysicalQueues);
         i++)
    {
        const LwU32 queueHead = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessLpqThenHpqPhysicalQueues[i]));

        status = cmdmgmtFbqMockCmdAddElement(&CmdMgmtTaskProcessLpqThenHpqFlcnQueueHeaders[i],
                                              CmdMgmtTaskProcessLpqThenHpqPhysicalQueues[i],
                                              queueHead);
        UT_ASSERT(status ==  FLCN_OK);

        // Increment the head
        REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessLpqThenHpqPhysicalQueues[i]), queueHead + 1U);
    }
}

/*!
 * @brief   Tests CmdMgmt Task processing behavior when commands queued to both high priority and low priority queues
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessLpqThenHpq)
{
    LwLength i;
    const LwLength maxIndex = LW_ARRAY_ELEMENTS(CmdMgmtTaskProcessLpqThenHpqFlcnQueueHeaders) - 1U;

    CmdMgmtTaskProcessLpqThenHpqPreTest();

    s_cmdmgmtProcessEvent(
        &(DISPATCH_CMDMGMT){ .disp2unitEvt = DISP2UNIT_EVT_RM_RPC, });

    // Ensure that interrupts have been re-enabled
    UT_ASSERT(REG_RD32(CSB, LW_CPWR_PMU_CMD_INTREN) == 7U);

    //
    // We have to check the that the headers were dispatched in reverse order.
    // This is because the HPQ should be processed before the LPQ, but the arrays
    // are arranged to put the LPQ first.
    //
    for (i = 0U;
         i <= maxIndex;
         i++)
    {
        DISP2UNIT_RM_RPC cmd;

        // Ensure that dispatching happened and get the dispatched item
        UT_ASSERT(cmdmgmtDispatchMockGetDispatched(&cmd) == FLCN_OK);

        // Ensure that the command we retrieved has the expected values
        UT_ASSERT(cmd.hdr.eventType == DISP2UNIT_EVT_RM_RPC);
        UT_ASSERT(cmd.hdr.unitId == CmdMgmtTaskProcessLpqThenHpqFlcnQueueHeaders[maxIndex - i].unitId);
        UT_ASSERT(cmd.cmdQueueId == CmdMgmtTaskProcessLpqThenHpqPhysicalQueues[maxIndex - i]);
        UT_ASSERT(memcmp(&cmd.pRpc->hdr,
                         &CmdMgmtTaskProcessLpqThenHpqFlcnQueueHeaders[maxIndex - i],
                         sizeof(CmdMgmtTaskProcessLpqThenHpqFlcnQueueHeaders[maxIndex - i])) == 0);
    }
}

/*!
 * @brief   Mock value from which to DMA to @ref SuperSurfaceFbOffset
 */
static RM_FLCN_U64
CmdMgmtTaskProcessCommandWraparoundSuperSurfaceFbOffset =
{
    .lo = 0xfeedfeed,
    .hi = 0xbeefbeef,
};

/*!
 * @brief   Mock memory region from which to DMA @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset
 */
static DMA_MOCK_MEMORY_REGION
CmdMgmtTaskProcessCommandWraparoundSuperSurfaceFbOffsetRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdMgmtTaskProcessCommandWraparoundSuperSurfaceFbOffset,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
    .sizeOfData = sizeof(CmdMgmtTaskProcessCommandWraparoundSuperSurfaceFbOffset),
};

/*!
 * @brief   Mock driver-managed heap offset to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessCommandWraparoundRmManagedAreaOffset = 0xB7U;

/*!
 * @brief   Mock driver-managed heap size to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessCommandWraparoundRmManagedAreaSize = 0xE9U;

/*!
 * @brief   Physical ID of the queue being sent to; initialized during pre-test
 */
static LwU8
CmdMgmtTaskProcessCommandWraparoundPhysicalQueue;

/*!
 * @brief   Queue head start reported by @ref s_cmdmgmtInit
 */
static LwU8
CmdMgmtTaskProcessCommandWraparoundHeadStart;

/*!
 * @brief   Mock @ref RM_FLCN_QUEUE_HDR to return from @ref osCmdmgmtFbqCmdCopyIn
 */
static const RM_FLCN_QUEUE_HDR
CmdMgmtTaskProcessCommandWraparoundFlcnQueueHeader =
{
    .unitId = RM_PMU_UNIT_CMDMGMT,
    .size = 0x10,
    .ctrlFlags = 0U,
    .seqNumId = 0U,
};

/*!
 * @brief   Sets up the state for CmdMgmtTaskProcessCommandWraparound test
 *
 * @details Initializes:
 *              CmdMgmt SuperSurface mocking
 *              DMA mocking
 *                  - A mock memory region based on @ref CmdMgmtTaskProcessCommandWraparoundSuperSurfaceFbOffsetRegion
 *              CmdMgmt FbQueue mocking
 *                  - The @ref cmdmgmtRMHInitMsgPopulate return values based on
 *                    @ref CmdMgmtTaskProcessCommandWraparoundRmManagedAreaOffset and
 *                    @ref CmdMgmtTaskProcessCommandWraparoundRmManagedAreaSize (via
 *                    @ref CmdMgmtFbqMockConfig)
 *              CmdMgmt RPC mocking
 *              CmdMgmt Dispatch mocking
 *              The CmdMgmt Task itself by calling @ref s_cmdmgmtInit directly
 *              @ref CmdMgmtTaskProcessCommandWraparoundPhysicalQueue using the RPC
 *                  sent by @ref s_cmdmgmtInit
 *              The LW_CPWR_PMU_CMD_HEAD_INTRSTAT register to indicate an interrupt for
 *                  @ref CmdMgmtTaskProcessCommandWraparoundPhysicalQueue
 *              The LW_CPWR_PMU_QUEUE_HEAD register to indicate one new message
 *                  is available
 *              One mock FbQueue element based on @ref CmdMgmtTaskProcessCommandWraparoundFlcnQueueHeader
 *                  and at queue @ref CmdMgmtTaskProcessCommandWraparoundPhysicalQueue
 *
 * @note    The "real" implementations of @ref pmuQueuePmuToRmInit_GMXXX, @ref pmuQueuePmuToRmHeadGet_GMXXX,
 *          @ref pmuQueuePmuToRmHeadSet_GMXXX, and @ref pmuQueuePmuToRmTailGet_GMXXX are used/referred to here.
 */
static void
CmdMgmtTaskProcessCommandWraparoundPreTest(void)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;
    LwU32 queueHead;
    FLCN_STATUS status;

    // Initialize super-surface mocking
    cmdmgmtSuperSurfaceMockInit();

    // Initialize DMA mocking
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdMgmtTaskProcessCommandWraparoundSuperSurfaceFbOffsetRegion);

    // Initialize RPC mocking
    cmdmgmtRpcMockInit();

    // Initialize dispatch mocking
    cmdmgmtDispatchMockInit();

    //
    // Initialize the task itself and retrieve initialization data out of the
    // RPC.
    //
    s_cmdmgmtInit();
    UT_ASSERT(cmdmgmtRpcMockGetSentRpc(&rpc.hdr, sizeof(rpc)) == FLCN_OK);
    UT_ASSERT(rpc.status == FLCN_OK);

    CmdMgmtTaskProcessCommandWraparoundPhysicalQueue = RM_PMU_COMMAND_QUEUE_LPQ;

    // Get what the initialization says the head is
    CmdMgmtTaskProcessCommandWraparoundHeadStart =
        REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessCommandWraparoundPhysicalQueue));
}

/*!
 * @brief   Tests CmdMgmt Task processing commands until the FbQueue is
 *          exhausted
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessCommandWraparound)
{
    DISP2UNIT_RM_RPC cmd;
    LwLength queueHead;
    LwU32 i;

    CmdMgmtTaskProcessCommandWraparoundPreTest();

    for (i = 0U, queueHead = CmdMgmtTaskProcessCommandWraparoundHeadStart;
         i < RM_PMU_FBQ_CMD_NUM_ELEMENTS + 1U;
         i++, queueHead = (queueHead + 1U) % RM_PMU_FBQ_CMD_NUM_ELEMENTS)
    {
        // Add the mock FbQueue element for this queue/position
        FLCN_STATUS status = cmdmgmtFbqMockCmdAddElement(&CmdMgmtTaskProcessCommandWraparoundFlcnQueueHeader,
                                                         CmdMgmtTaskProcessCommandWraparoundPhysicalQueue,
                                                         queueHead);
        UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

        // Increment the head
        REG_WR32(CSB,
                 LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessCommandWraparoundPhysicalQueue),
                 (queueHead + 1U) % RM_PMU_FBQ_CMD_NUM_ELEMENTS);

        // Mark the interrupt pending for this queue
        REG_WR32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT, LWBIT32(CmdMgmtTaskProcessCommandWraparoundPhysicalQueue));

        // Process the new command
        s_cmdmgmtProcessEvent(
            &(DISPATCH_CMDMGMT){ .disp2unitEvt = DISP2UNIT_EVT_RM_RPC, });

        // Ensure that dispatching happened
        UT_ASSERT_EQUAL_UINT(cmdmgmtDispatchMockGetDispatched(&(DISP2UNIT_RM_RPC){0U}), FLCN_OK);
    }
}

/*!
 * @brief   Mock value from which to DMA to @ref SuperSurfaceFbOffset
 */
static RM_FLCN_U64
CmdMgmtTaskProcessDispatchFailureSuperSurfaceFbOffset =
{
    .lo = 0xfeedfeed,
    .hi = 0xbeefbeef,
};

/*!
 * @brief   Mock memory region from which to DMA @ref CmdMgmtTaskInitSuccessfulSuperSurfaceFbOffset
 */
static DMA_MOCK_MEMORY_REGION
CmdMgmtTaskProcessDispatchFailureSuperSurfaceFbOffsetRegion =
{
    .pMemDesc = &PmuInitArgs.superSurface,
    .pData = &CmdMgmtTaskProcessDispatchFailureSuperSurfaceFbOffset,
    .offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(hdr.data.superSurfaceFbOffset),
    .sizeOfData = sizeof(CmdMgmtTaskProcessDispatchFailureSuperSurfaceFbOffset),
};

/*!
 * @brief   Mock driver-managed heap offset to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessDispatchFailureRmManagedAreaOffset = 0xB7U;

/*!
 * @brief   Mock driver-managed heap size to return from @ref cmdmgmtRMHInitMsgPopulate
 */
static const LwU16
CmdMgmtTaskProcessDispatchFailureRmManagedAreaSize = 0xE9U;

/*!
 * @brief   Physical ID of the queue being sent to; initialized during pre-test
 */
static LwU8
CmdMgmtTaskProcessDispatchFailurePhysicalQueue;

/*!
 * @brief   Mock @ref RM_FLCN_QUEUE_HDR to return from @ref osCmdmgmtFbqCmdCopyIn
 */
static const RM_FLCN_QUEUE_HDR
CmdMgmtTaskProcessDispatchFailureFlcnQueueHeader =
{
    .unitId = RM_PMU_UNIT_CMDMGMT,
    .size = 0x10,
    .ctrlFlags = 0U,
    .seqNumId = 0U,
};

/*!
 *
 */
static const FLCN_STATUS
CmdMgmtTaskProcessDispatchFailureErrorCode = FLCN_ERR_QUEUE_MGMT_ILWALID_UNIT_ID;

/*!
 * @brief   Sets up the state for CmdMgmtTaskProcessDispatchFailure test
 *
 * @details Initializes:
 *              CmdMgmt SuperSurface mocking
 *              DMA mocking
 *                  - A mock memory region based on @ref CmdMgmtTaskProcessDispatchFailureSuperSurfaceFbOffsetRegion
 *              CmdMgmt FbQueue mocking
 *                  - The @ref cmdmgmtRMHInitMsgPopulate return values based on
 *                    @ref CmdMgmtTaskProcessDispatchFailureRmManagedAreaOffset and
 *                    @ref CmdMgmtTaskProcessDispatchFailureRmManagedAreaSize (via
 *                    @ref CmdMgmtFbqMockConfig)
 *              CmdMgmt RPC mocking
 *              CmdMgmt Dispatch mocking
 *                  - A failure return value from @ref cmdmgmtExtCmdDispatch
 *                   (via @ref CmdMgmtDispatchMockConfig); set to
 *                   @ref CmdMgmtTaskProcessDispatchFailureErrorCode
 *              The CmdMgmt Task itself by calling @ref s_cmdmgmtInit directly
 *              @ref CmdMgmtTaskProcessDispatchFailurePhysicalQueue using the RPC
 *                  sent by @ref s_cmdmgmtInit
 *              The LW_CPWR_PMU_CMD_HEAD_INTRSTAT register to indicate an interrupt for
 *                  @ref CmdMgmtTaskProcessDispatchFailurePhysicalQueue
 *              The LW_CPWR_PMU_QUEUE_HEAD register to indicate one new message
 *                  are available
 *              One mock FbQueue element based on @ref CmdMgmtTaskProcessDispatchFailureFlcnQueueHeader
 *                  and at queue @ref CmdMgmtTaskProcessDispatchFailurePhysicalQueue
 *
 * @note    The "real" implementations of @ref pmuQueuePmuToRmInit_GMXXX, @ref pmuQueuePmuToRmHeadGet_GMXXX,
 *          @ref pmuQueuePmuToRmHeadSet_GMXXX, and @ref pmuQueuePmuToRmTailGet_GMXXX are used/referred to here.
 */
static void
CmdMgmtTaskProcessDispatchFailurePreTest(void)
{
    PMU_RM_RPC_STRUCT_CMDMGMT_INIT rpc;
    LwU32 queueHead;
    FLCN_STATUS status;

    // Initialize super-surface mocking
    cmdmgmtSuperSurfaceMockInit();

    // Initialize DMA mocking
    dmaMockInit();
    dmaMockConfigMemoryRegionInsert(&CmdMgmtTaskProcessDispatchFailureSuperSurfaceFbOffsetRegion);

    // Initialize RPC mocking
    cmdmgmtRpcMockInit();

    // Initialize dispatch mocking
    cmdmgmtDispatchMockInit();
    CmdMgmtDispatchMockConfig.cmdmgmtExtCmdDispatchConfig.errorCodes[0U] =
        CmdMgmtTaskProcessDispatchFailureErrorCode;

    // Initialize LWOS task mocking
    lwosTaskMockInit();

    //
    // Initialize the task itself and retrieve initialization data out of the
    // RPC.
    //
    s_cmdmgmtInit();
    UT_ASSERT(cmdmgmtRpcMockGetSentRpc(&rpc.hdr, sizeof(rpc)) == FLCN_OK);
    UT_ASSERT(rpc.status == FLCN_OK);

    CmdMgmtTaskProcessDispatchFailurePhysicalQueue = RM_PMU_COMMAND_QUEUE_HPQ;

    // Mark the interrupt pending for this queue
    REG_WR32(CSB, LW_CPWR_PMU_CMD_HEAD_INTRSTAT, LWBIT32(CmdMgmtTaskProcessDispatchFailurePhysicalQueue));

    // Get what the initialization says the head is
    queueHead = REG_RD32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessDispatchFailurePhysicalQueue));

    // Add the mock FbQueue element for this queue/position
    status = cmdmgmtFbqMockCmdAddElement(&CmdMgmtTaskProcessDispatchFailureFlcnQueueHeader,
                                         CmdMgmtTaskProcessDispatchFailurePhysicalQueue,
                                         queueHead);
    UT_ASSERT(status == FLCN_OK);

    // Increment the head
    queueHead++;
    REG_WR32(CSB, LW_CPWR_PMU_QUEUE_HEAD(CmdMgmtTaskProcessDispatchFailurePhysicalQueue), queueHead);
}

/*!
 * @brief   Tests CmdMgmt Task processing a command when dispatch returns a non-fatal failure
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessDispatchFailure)
{
    FLCN_STATUS status;

    CmdMgmtTaskProcessDispatchFailurePreTest();

    s_cmdmgmtProcessEvent(
        &(DISPATCH_CMDMGMT){ .disp2unitEvt = DISP2UNIT_EVT_RM_RPC, });

    UT_ASSERT_EQUAL_UINT(status, CmdMgmtTaskProcessDispatchFailureErrorCode);
}

/*!
 * @brief   Tests CmdMgmt Task processing a valid signal for PBI
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessSignalValidSignal)
{
    s_cmdmgmtProcessEvent(
        &(DISPATCH_CMDMGMT)
        {
            .signal =
            {
                .disp2unitEvt = DISP2UNIT_EVT_SIGNAL,
                .gpioSignal = DISPATCH_CMDMGMT_SIGNAL_GPIO_PBI,
            },
        });

    if (PMUCFG_FEATURE_ENABLED(PMU_PBI_SUPPORT))
    {
        // TODO on a supported profile: mock and test this
        // UT_ASSERT_EQUAL_UINT(pbiService_MOCK_fake.call_count, 1U);
    }
}

/*!
 * @brief   Tests CmdMgmt Task processing a invalid signal for PBI
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessSignalIlwalidSignal)
{
    s_cmdmgmtProcessEvent(
        &(DISPATCH_CMDMGMT)
        {
            .signal =
            {
                .disp2unitEvt = DISP2UNIT_EVT_SIGNAL,
                .gpioSignal = DISPATCH_CMDMGMT_SIGNAL_GPIO_PBI + 1U,
            },
        });

    // TODO in chips_a: mock and test these
    // UT_ASSERT_NOT_EQUAL_UINT(pbiService_MOCK_fake.call_count, 1U);
    // UT_ASSERT_NOT_EQUAL(status, FLNC_OK);
}

/*!
 * @brief   Tests CmdMgmt Task processing an invalid event
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskProcessEventIlwalid)
{
     s_cmdmgmtProcessEvent(
        &(DISPATCH_CMDMGMT){ .disp2unitEvt = DISP2UNIT_EVT__COUNT, });
}

/*!
 * @brief   Sets up the state for CmdMgmtTaskInitDoneRpc test
 *
 * @details Initializes:
 *              LWOS DMEM OVL mocking
 */
static void
CmdMgmtTaskInitDoneRpcPreTest(void)
{
    lwosDmemovlMockInit();
}

/*!
 * @brief   Tests CmdMgmt Task's PMU_INIT_DONE RPC
 */
UT_CASE_RUN(PMU_CMDMGMT_TASK, CmdMgmtTaskInitDoneRpc)
{
    RM_PMU_RPC_STRUCT_CMDMGMT_PMU_INIT_DONE rpc;
    FLCN_STATUS status;

    CmdMgmtTaskInitDoneRpcPreTest();

    status = pmuRpcCmdmgmtPmuInitDone(&rpc);

    UT_ASSERT(status == FLCN_OK);
    UT_ASSERT(lwosDmemovlMockHeapAllocationsBlockNumCalls() == 1U);
}
