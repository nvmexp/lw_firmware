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
 * @file    boardobjgrp-test.c
 * @brief   Unit tests for BOARDOBJGRP.
 */

/* ------------------------ Includes ---------------------------------------- */
#include "test-macros.h"
#include "dma-mock.h"
#include "perf/changeseq-mock.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrpmask-mock.h"
#include "boardobj/boardobjgrp_iface_model_10.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static void* testSetup(void);
static BoardObjGrpIfaceModel10CmdHandler        (s_unitClassOneBoardObjGrpIfaceModel10Set);
static BoardObjGrpIfaceModel10CmdHandler        (s_unitClassOneBoardObjGrpIfaceModel10GetStatus);
static BoardObjGrpIfaceModel10CmdHandler        (s_unitClassTwoBoardObjGrpIfaceModel10Set);
static BoardObjGrpIfaceModel10CmdHandler        (s_unitClassTwoBoardObjGrpIfaceModel10GetStatus);
static BoardObjGrpIfaceModel10CmdHandler        (s_unitClassFourBoardObjGrpIfaceModel10Set);

static BoardObjGrpIfaceModel10SetHeader   (s_mockGrpIfaceModel10SetHeader);
static BoardObjGrpIfaceModel10SetEntry    (s_mockGrpIfaceModel10SetEntry);
static BoardObjGrpIfaceModel10SetHeader   (s_mockGrpIfaceModel10SetHeaderFail);
static BoardObjGrpIfaceModel10SetEntry    (s_mockGrpIfaceModel10SetEntryFail);

static BoardObjGrpIfaceModel10GetStatusHeader   (s_mockGrpIfaceModel10GetStatusHeader);
static BoardObjGrpGetStatusEntry    (s_mockGrpGetStatusEntry);
static BoardObjGrpIfaceModel10GetStatusHeader   (s_mockGrpIfaceModel10GetStatusHeaderFail);
static BoardObjGrpGetStatusEntry    (s_mockGrpGetStatusEntryFail);

/* ------------------------ Defines and Macros ------------------------------ */
#define LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_CLASS_ONE     (0U)
#define LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_CLASS_TWO     (1U)
#define LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_CLASS_THREE   (2U)
#define LW2080_CTRL_BOARDOBJGRP_CLASS_ID_UNIT_CLASS_FOUR    (3U)

#define BOARDOBJGRP_DATA_LOCATION_UT    \
    (&unitTestBoardObjgrpE32.super.super)

/* ------------------------ Type Definitions -------------------------------- */
/*!
 * @brief   Unit Testing BOARDOBJGRP base class.
 *
 * @extends BOARDOBJGRP_E32
 */
typedef struct UT_GRP_E32
{
    /*!
     * @brief BOARDOBJGRP_E32 super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    BOARDOBJGRP_E32 super;

    /*!
     * { item_description }
     */
    LwU32           dummySetData;

    /*!
     * { item_description }
     */
    LwU32           dummyStatusData;
} UT_GRP_E32;

/*!
 * @brief   Unit Testing BOARDOBJGRP base class.
 *
 * @extends BOARDOBJGRP_E32
 */
typedef struct UT_GRP_E255
{
    /*!
     * @brief BOARDOBJGRP_E255 super class.
     *
     * Must always be the first element in the structure.
     *
     * @protected
     */
    BOARDOBJGRP_E255    super;

    /*!
     * { item_description }
     */
    LwU32               dummySetData;

    /*!
     * { item_description }
     */
    LwU32               dummyStatusData;
} UT_GRP_E255;

typedef struct UT
{
    /*!
     * { item_description }
     */
    BOARDOBJ    super;

    /*!
     * { item_description }
     */
    LwU32       dummySetData;

    /*!
     * { item_description }
     */
    LwU32       dummyStatusData;
} UT;

typedef UT UT_BASE;

typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class.  Must always be first element in structure.
     */
    RM_PMU_BOARDOBJGRP_E32  super;

    /*!
     * Implementation specific group set data.
     */
    LwU32                   groupSetData;
} RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER;

typedef struct
{
    /*!
     * Super class. This should always be the first member!
     */
    RM_PMU_BOARDOBJ super;

    /*!
     * Implementation specific object set data.
     */
    LwU32           objectSetData;
} RM_PMU_UNIT_UT_SET;

typedef union
{
    RM_PMU_BOARDOBJ     super;
    RM_PMU_UNIT_UT_SET  unitUT;
} RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION;

RM_PMU_BOARDOBJ_GRP_SET_MAKE_E32(UNIT, UT);

typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class.  Must always be first element in structure.
     */
    RM_PMU_BOARDOBJGRP_E32  super;

    /*!
     * Implementation specific group status data.
     */
    LwU32                   groupStatusData;
} RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER;

typedef struct
{
    /*!
     * Super class. This should always be the first member!
     */
    RM_PMU_BOARDOBJ super;

    /*!
     * Implementation specific object set data.
     */
    LwU32           objectStatusData;
} RM_PMU_UNIT_UT_GET_STATUS_SUPER;

typedef union
{
    RM_PMU_BOARDOBJ                     super;
    RM_PMU_UNIT_UT_GET_STATUS_SUPER     unitUT;
} RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION;

RM_PMU_BOARDOBJ_GRP_GET_STATUS_MAKE_E32(UNIT, UT);


/* ------------------------ Globals ----------------------------------------- */
// For testing
UT_GRP_E32  unitTestBoardObjgrpE32  = { 0 };
UT_GRP_E255 unitTestBoardObjgrpE255 = { 0 };

// Required to build boardobjgrp.c - KO-TODO: remove this
RM_PMU_CMD_LINE_ARGS PmuInitArgs;

/*!
 * Array of all UNIT BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY handler entries. To be used
 * with @ref boardObjGrpIfaceModel10CmdHandlerDispatch.
 */
static BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_ENTRY utBoardObjGrpHandlers[] =
{
    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(UNIT, CLASS_ONE,
        s_unitClassOneBoardObjGrpIfaceModel10Set,
        s_unitClassOneBoardObjGrpIfaceModel10GetStatus),

    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE(UNIT, CLASS_TWO,
        s_unitClassTwoBoardObjGrpIfaceModel10Set,
        s_unitClassTwoBoardObjGrpIfaceModel10GetStatus),

    BOARDOBJGRP_IFACE_MODEL_10_CMD_HANDLER_CREATE_SET_ONLY(UNIT, CLASS_FOUR,
        s_unitClassFourBoardObjGrpIfaceModel10Set),
};
/* ------------------------ Local Data -------------------------------------- */
/* ------------------------ Test Suite Declaration--------------------------- */
UT_SUITE_DEFINE(PMU_BOARDOBJGRP,
                UT_SUITE_SET_COMPONENT("TODO")
                UT_SUITE_SET_DESCRIPTION("TODO")
                UT_SUITE_SET_OWNER("TODO")
                UT_SUITE_SETUP_HOOK(testSetup))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, CmdHandlerDispatchPositiveCases,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, CmdHandlerDispatchNullHandler,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, CmdHandlerDispatchIlwalidClassId,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, CmdHandlerDispatchIlwalidCommandId,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, GrpGet,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, GrpGetByGrpPtr,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, ObjGet,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, IsValid,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, IsEmpty,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, MaxIdxGet,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10Set,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10SetHeader,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10GetStatus_SUPER,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10ReadHeader,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10WriteHeader,
                UT_CASE_SET_DESCRIPTION("TODO")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/* ------------------------ Test Suite Implementation ----------------------- */
/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, CmdHandlerDispatchPositiveCases)
{
    FLCN_STATUS     status;
    PMU_DMEM_BUFFER buffer;

    // Positive case 1
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_ONE),  // classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD_SET);                        // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_TIMEOUT);

    // Positive case 2
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_ONE),  // classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD_GET_STATUS);                 // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_NO_FREE_MEM);

    // Positive case 3
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_TWO),  // classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD_SET);                        // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_HDCP_ILWALID_SRM);

    // Positive case 4
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_TWO),  // classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD_GET_STATUS);                 // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_HDCP_RECV_REVOKED);

    // Positive case 5
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_FOUR), // classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD_SET);                        // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_RPC_ILWALID_INPUT);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, CmdHandlerDispatchNullHandler)
{
    FLCN_STATUS     status;
    PMU_DMEM_BUFFER buffer;

    // Negative case 1 - handler is NULL
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_FOUR), // classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD_GET_STATUS);                 // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_NOT_SUPPORTED);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, CmdHandlerDispatchIlwalidClassId)
{
    FLCN_STATUS     status;
    PMU_DMEM_BUFFER buffer;

    // Negative case 2 - classId not found
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_THREE),// classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD_SET);                        // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_NOT_SUPPORTED);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, CmdHandlerDispatchIlwalidCommandId)
{
    FLCN_STATUS     status;
    PMU_DMEM_BUFFER buffer;

    // Negative case 3 - commandId is not valid
    status = boardObjGrpIfaceModel10CmdHandlerDispatch_IMPL(
        LW2080_CTRL_BOARDOBJGRP_CLASS_ID(UNIT, CLASS_ONE),  // classId
        &buffer,                                            // pBuffer
        LW_ARRAY_ELEMENTS(utBoardObjGrpHandlers),           // numEntries
        utBoardObjGrpHandlers,                              // pEntries
        RM_PMU_BOARDOBJGRP_CMD__COUNT);                     // commandId
    UT_ASSERT_TRUE(status == FLCN_ERR_ILWALID_ARGUMENT);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, GrpGet)
{
    UT_ASSERT_TRUE(BOARDOBJGRP_GRP_GET(UT) == &unitTestBoardObjgrpE32.super.super);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, GrpGetByGrpPtr)
{
    PBOARDOBJ pObjects[2];
    unitTestBoardObjgrpE32.super.super.ppObjects = pObjects;
    unitTestBoardObjgrpE32.super.super.objSlots = 2;

    UT_ASSERT_TRUE(BOARDOBJGRP_OBJ_GET_BY_GRP_PTR(BOARDOBJGRP_GRP_GET(UT), 0) == pObjects[0]);
    UT_ASSERT_TRUE(BOARDOBJGRP_OBJ_GET_BY_GRP_PTR(BOARDOBJGRP_GRP_GET(UT), 1) == pObjects[1]);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, ObjGet)
{
    PBOARDOBJ pObjects[2];
    unitTestBoardObjgrpE32.super.super.ppObjects = pObjects;
    unitTestBoardObjgrpE32.super.super.objSlots = 2;

    UT_ASSERT_TRUE(BOARDOBJGRP_OBJ_GET(UT, 0) == pObjects[0]);
    UT_ASSERT_TRUE(BOARDOBJGRP_OBJ_GET(UT, 1) == pObjects[1]);
    UT_ASSERT_TRUE(BOARDOBJGRP_OBJ_GET(UT, 2) == NULL);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, IsValid)
{
    PBOARDOBJ pObjects[2];
    pObjects[0] = (PBOARDOBJ)1; // Non-NULL pointer
    pObjects[0] = (PBOARDOBJ)2; // Non-NULL pointer
    unitTestBoardObjgrpE32.super.super.ppObjects = pObjects;
    unitTestBoardObjgrpE32.super.super.objSlots = 2;

    UT_ASSERT_TRUE(BOARDOBJGRP_IS_VALID(UT, 0));
    UT_ASSERT_TRUE(BOARDOBJGRP_IS_VALID(UT, 1));
    UT_ASSERT_FALSE(BOARDOBJGRP_IS_VALID(UT, 2));
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, IsEmpty)
{
    PBOARDOBJ pObjects[2];
    unitTestBoardObjgrpE32.super.super.ppObjects = NULL;
    unitTestBoardObjgrpE32.super.super.objSlots = 0;

    UT_ASSERT_TRUE(BOARDOBJGRP_IS_EMPTY(UT));

    unitTestBoardObjgrpE32.super.super.ppObjects = pObjects;
    unitTestBoardObjgrpE32.super.super.objSlots = 2;

    UT_ASSERT_FALSE(BOARDOBJGRP_IS_EMPTY(UT));
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, MaxIdxGet)
{
    PBOARDOBJ   pObjects[2];
    LwU8        index;
    unitTestBoardObjgrpE32.super.super.ppObjects = NULL;
    unitTestBoardObjgrpE32.super.super.objSlots = 0;
    index = BOARDOBJGRP_MAX_IDX_GET(UT);

    UT_ASSERT_EQUAL_UINT(index, LW2080_CTRL_BOARDOBJ_IDX_ILWALID);

    unitTestBoardObjgrpE32.super.super.ppObjects = pObjects;
    unitTestBoardObjgrpE32.super.super.objSlots = 2;
    index = BOARDOBJGRP_MAX_IDX_GET(UT);

    UT_ASSERT_EQUAL_UINT(index, 1);
}

#define UT_DMEM_BUFFER_SIZE_BYTES   (256U)
#define UT_SURFACE_SIZE_BYTES       (256U)

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10Set)
{
    LwU8                    buffer[UT_DMEM_BUFFER_SIZE_BYTES]   = { 0 };
    LwU8                    fakeSurface[UT_SURFACE_SIZE_BYTES]  = { 0 };
    RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET *pSet =
        (RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET *)fakeSurface;
    PMU_DMEM_BUFFER         dmemBuffer;
    BOARDOBJGRPMASK_E32     maskE32;
    BOARDOBJGRPMASK_E255    maskE255;
    LwU32                   ssOffset    = 0;
    UT                      utBoardObj  = { 0 };
    PBOARDOBJ               pObjects[1] = { &utBoardObj.super };
    FLCN_STATUS             status;

    unitTestBoardObjgrpE32.super.super.ppObjects        = pObjects;
    unitTestBoardObjgrpE32.super.super.objSlots         = 1;
    unitTestBoardObjgrpE32.super.objMask.super.pData[0] = 1;
    unitTestBoardObjgrpE32.super.objMask.super.bitCount = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
    unitTestBoardObjgrpE32.super.objMask.super.maskDataCount = 1;

    pSet->hdr.data.super.objMask.super.pData[0] = 1;
    pSet->hdr.data.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    pSet->hdr.data.super.super.classId = 0;
    pSet->hdr.data.super.super.objSlots = 1;

    dmemBuffer.pBuf = buffer;
    dmemBuffer.size = UT_DMEM_BUFFER_SIZE_BYTES;

    DMA_MOCK_MEMORY_REGION memoryRegion =
    {
        .pMemDesc       = &PmuInitArgs.superSurface,
        .pData          = fakeSurface,
        .offsetOfData   = 0,
        .sizeOfData     = sizeof(fakeSurface),
    };

    dmaMockConfigMemoryRegionInsert(&memoryRegion);

    // 1) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE); if pBuffer->size < hdrSize
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  dmemBuffer.size + 1,
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE);

    // 2) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE); if pBuffer->size < entrySize
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  dmemBuffer.size + 1,
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE);

    // 3) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT); if (numObjectVtables == 0) && (ppObjectVtables  != NULL)
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  (PBOARDOBJ_VIRTUAL_TABLE *) 1,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    // 4) UT_ASSERT_EQUAL_UINT(status, dmaRead_error); if dmaRead fails for hdr
    dmaMockReadAddEntry(0, FLCN_ERROR);
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // 5) UT_ASSERT_EQUAL_UINT(status, hdrFunc_error); if hdrFunc fails
    dmaMockReadAddEntry(1, FLCN_OK);
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeaderFail,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);

    // 6) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if invalid boardobjgrp type is passed
    dmaMockReadAddEntry(2, FLCN_OK);
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_ILWALID,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 7) UT_ASSERT_EQUAL_UINT(status, boardObjGrpMaskImport_FUNC_error); if boardObjGrpMaskImport_FUNC fails
    dmaMockReadAddEntry(3, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(0, FLCN_ERR_TIMEOUT, NULL);
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_TIMEOUT);

    // 8) UT_ASSERT_EQUAL_UINT(status, dmaRead_error); if dmaRead fails for entry
    dmaMockReadAddEntry(4, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(1, FLCN_OK, NULL);
    dmaMockReadAddEntry(5, FLCN_ERROR);
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // 9) UT_ASSERT_EQUAL_UINT(status, entryFunc_error); if entryFunc fails
    dmaMockReadAddEntry(6, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(2, FLCN_OK, NULL);
    dmaMockReadAddEntry(7, FLCN_OK);
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntryFail,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_FUNCTION);

    // 10) Positive checks:
    dmaMockInit();
    boardObjGrpMaskImportMockInit();
    dmaMockConfigMemoryRegionInsert(&memoryRegion);
    dmaMockReadAddEntry(0, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
    dmaMockReadAddEntry(1, FLCN_OK);
    status = boardObjGrpIfaceModel10Set(&unitTestBoardObjgrpE32.super.super,
                                  LW2080_CTRL_BOARDOBJGRP_TYPE_E32,
                                  &dmemBuffer,
                                  s_mockGrpIfaceModel10SetHeader,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER_ALIGNED),
                                  s_mockGrpIfaceModel10SetEntry,
                                  sizeof(RM_PMU_UNIT_UT_BOARDOBJ_SET_UNION_ALIGNED),
                                  ssOffset,
                                  LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_SET, hdr),
                                  0,
                                  &maskE32.super,
                                  NULL,
                                  0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(pSet->hdr.data.groupSetData, unitTestBoardObjgrpE32.dummySetData);
    UT_ASSERT_EQUAL_UINT(pSet->objects[0].data.unitUT.objectSetData, utBoardObj.dummySetData);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10SetHeader)
{
    PBOARDOBJ                               pObjects[1] = { NULL };
    RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER   hdrDesc;
    FLCN_STATUS                             status;

    unitTestBoardObjgrpE32.super.super.bConstructed = LW_FALSE;
    hdrDesc.super.super.objSlots = 1;
    hdrDesc.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;

    unitTestBoardObjgrpE32.super.objMask.super.pData[0] = 1;
    unitTestBoardObjgrpE32.super.objMask.super.bitCount = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
    unitTestBoardObjgrpE32.super.objMask.super.maskDataCount = 1;

    hdrDesc.super.objMask.super.pData[0] = 1;

    // 1) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if type mismatch between group and header
    unitTestBoardObjgrpE32.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_ILWALID;
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    unitTestBoardObjgrpE32.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 2) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NO_FREE_MEM); if lwosCallocType fails when group is unconstructed
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    callocMockAddEntry(0, NULL, NULL);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NO_FREE_MEM);

    // 3) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if group has an invalid type when group is unconstructed
    callocMockAddEntry(1, pObjects, NULL);
    unitTestBoardObjgrpE32.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_ILWALID;
    hdrDesc.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_ILWALID;
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    unitTestBoardObjgrpE32.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    hdrDesc.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 4) UT_ASSERT_EQUAL_UINT(status, {boardObjGrpMaskImport_FUNC_error}); if boardObjGrpMaskImport_FUNC fails when group is unconstructed
    callocMockAddEntry(2, pObjects, NULL);
    boardObjGrpMaskImportMockAddEntry(0, FLCN_ERR_TIMEOUT, NULL);
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_TIMEOUT);

    // 5) Posivite 1st time construction
    callocMockAddEntry(3, pObjects, NULL);
    boardObjGrpMaskImportMockAddEntry(1, FLCN_OK, NULL);
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_TRUE(unitTestBoardObjgrpE32.super.super.bConstructed);

    // 6) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if group and header class ids mismatch when group is constructed
    unitTestBoardObjgrpE32.super.super.classId = 1;
    hdrDesc.super.super.classId = 0;
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    unitTestBoardObjgrpE32.super.super.classId = 1;
    hdrDesc.super.super.classId = 1;
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 7) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if group and header number of object slots mismatch when group is constructed
    unitTestBoardObjgrpE32.super.super.objSlots = 1;
    hdrDesc.super.super.objSlots = 0;
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    unitTestBoardObjgrpE32.super.super.objSlots = 1;
    hdrDesc.super.super.objSlots = 1;
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 8) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if group and header masks are not a match and SET_TYPE_INIT when group is constructed
    hdrDesc.super.super.flags = FLD_SET_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS,
                                            _SET_TYPE, _INIT, hdrDesc.super.super.flags);

    unitTestBoardObjgrpE32.super.objMask.super.pData[0] = 0x3;
    hdrDesc.super.objMask.super.pData[0] = 0x2;

    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 9) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if hdr mask is not a subset of group mask and SET_TYPE_UPDATE when group is constructed
    hdrDesc.super.super.flags = FLD_SET_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS,
                                            _SET_TYPE, _UPDATE, hdrDesc.super.super.flags);

    unitTestBoardObjgrpE32.super.objMask.super.pData[0] = 0x3;
    hdrDesc.super.objMask.super.pData[0] = 0x4;

    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 10) Positive checks:
    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    hdrDesc.super.super.flags = FLD_SET_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS,
                                            _SET_TYPE, _UPDATE, hdrDesc.super.super.flags);

    unitTestBoardObjgrpE32.super.objMask.super.pData[0] = 0x1;
    hdrDesc.super.objMask.super.pData[0] = 0x1;

    status = boardObjGrpIfaceModel10SetHeader(&unitTestBoardObjgrpE32.super.super, (RM_PMU_BOARDOBJGRP *)&hdrDesc);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10GetStatus_SUPER)
{
    LwU8                    buffer[UT_DMEM_BUFFER_SIZE_BYTES]   = { 0 };
    LwU8                    fakeSurface[UT_SURFACE_SIZE_BYTES]  = { 0 };
    RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS *pGetStatus =
        (RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS *)fakeSurface;
    PMU_DMEM_BUFFER         dmemBuffer;
    BOARDOBJGRPMASK_E32     maskE32;
    BOARDOBJGRPMASK_E255    maskE255;
    LwU32                   ssOffset    = 0;
    LwU32                   readOffset  = 0;
    UT                      utBoardObj  = { 0 };
    PBOARDOBJ               pObjects[1] = { &utBoardObj.super };
    FLCN_STATUS             status;

    unitTestBoardObjgrpE32.super.super.ppObjects        = pObjects;
    unitTestBoardObjgrpE32.super.super.objSlots         = 1;
    unitTestBoardObjgrpE32.super.objMask.super.pData[0] = 1;
    unitTestBoardObjgrpE32.super.objMask.super.bitCount = LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS;
    unitTestBoardObjgrpE32.super.objMask.super.maskDataCount = 1;

    pGetStatus->hdr.data.super.objMask.super.pData[0] = 1;
    pGetStatus->hdr.data.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    pGetStatus->hdr.data.super.super.classId = 0;
    pGetStatus->hdr.data.super.super.objSlots = 1;
    pGetStatus->hdr.data.super.super.flags = FLD_SET_DRF(_RM_PMU_BOARDOBJGRP_SUPER, _FLAGS,
        _GET_STATUS_COPY_IN, _TRUE, pGetStatus->hdr.data.super.super.flags);

    dmemBuffer.pBuf = buffer;
    dmemBuffer.size = UT_DMEM_BUFFER_SIZE_BYTES;

    DMA_MOCK_MEMORY_REGION memoryRegion =
    {
        .pMemDesc       = &PmuInitArgs.superSurface,
        .pData          = fakeSurface,
        .offsetOfData   = 0,
        .sizeOfData     = sizeof(fakeSurface),
    };
    dmaMockConfigMemoryRegionInsert(&memoryRegion);

    // 1) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE); if pBuffer->size < hdrSize
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        // sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        dmemBuffer.size + 1,
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE);

    // 2) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE); if pBuffer->size < entrySize
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        // sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        dmemBuffer.size + 1,
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_OUT_OF_RANGE);

    // 3) UT_ASSERT_EQUAL_UINT(status, {DMA_MOCK_ERROR}); if first dma read fails
    dmaMockReadAddEntry(0, FLCN_ERROR);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // 4) UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE); if pBoardObjGrp->type is invalid
    dmaMockReadAddEntry(1, FLCN_OK);
    unitTestBoardObjgrpE32.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_ILWALID;
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    unitTestBoardObjgrpE32.super.super.type = LW2080_CTRL_BOARDOBJGRP_TYPE_E32;
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_STATE);

    // 5) UT_ASSERT_EQUAL_UINT(status, {boardObjGrpMaskImport_FUNC_MOCK_ERROR}); if boardObjGrpMaskImport_FUNC fails
    dmaMockReadAddEntry(2, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(0, FLCN_ERR_TIMEOUT, NULL);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_TIMEOUT);

    // 6) UT_ASSERT_EQUAL_UINT(status, {hdrFunc_error}); if hdrFunc is supplied and fails
    dmaMockReadAddEntry(2, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(1, FLCN_OK, NULL);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeaderFail,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_NOT_SUPPORTED);

    // 7) UT_ASSERT_EQUAL_UINT(status, {DMA_MOCK_ERROR}); if hdrFunc is supplied and dma write fails
    dmaMockReadAddEntry(3, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(2, FLCN_OK, NULL);
    dmaMockWriteAddEntry(0, FLCN_ERROR);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // 8) UT_ASSERT_EQUAL_UINT(status, {DMA_MOCK_ERROR}); if dma read fails for copyIn
    dmaMockReadAddEntry(4, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(3, FLCN_OK, NULL);
    dmaMockWriteAddEntry(1, FLCN_OK);
    dmaMockReadAddEntry(5, FLCN_ERROR);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);


    dmaMockInit();
    boardObjGrpMaskImportMockInit();
    dmaMockConfigMemoryRegionInsert(&memoryRegion);


    // 9) UT_ASSERT_EQUAL_UINT(status, {entryFunc_error}); if entryFunc is supplied and fails
    dmaMockReadAddEntry(0, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(0, FLCN_OK, NULL);
    dmaMockWriteAddEntry(0, FLCN_OK);
    dmaMockReadAddEntry(1, FLCN_OK);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntryFail,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_FUNCTION);

    // 10) UT_ASSERT_EQUAL_UINT(status, {DMA_MOCK_ERROR}); if dma write fails
    dmaMockReadAddEntry(2, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(1, FLCN_OK, NULL);
    dmaMockWriteAddEntry(1, FLCN_OK);
    dmaMockReadAddEntry(3, FLCN_OK);
    dmaMockWriteAddEntry(2, FLCN_ERR_CSB_PRIV_READ_ERROR);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_CSB_PRIV_READ_ERROR);

    // 11) Positive cases:
    unitTestBoardObjgrpE32.dummyStatusData = 213;
    utBoardObj.dummyStatusData = 23;

    dmaMockReadAddEntry(4, FLCN_OK);
    boardObjGrpMaskImportMockAddEntry(2, FLCN_OK, NULL);
    dmaMockWriteAddEntry(3, FLCN_OK);
    dmaMockReadAddEntry(5, FLCN_OK);
    dmaMockWriteAddEntry(4, FLCN_OK);
    status = boardObjGrpIfaceModel10GetStatus_SUPER(&unitTestBoardObjgrpE32.super.super,
                                        &dmemBuffer,
                                        s_mockGrpIfaceModel10GetStatusHeader,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER_ALIGNED),
                                        s_mockGrpGetStatusEntry,
                                        sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED),
                                        ssOffset,
                                        LW_OFFSETOF(RM_PMU_UNIT_UT_BOARDOBJ_GRP_GET_STATUS, hdr),
                                        &maskE32.super);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
    UT_ASSERT_EQUAL_UINT(pGetStatus->hdr.data.groupStatusData, unitTestBoardObjgrpE32.dummyStatusData);
    UT_ASSERT_EQUAL_UINT(pGetStatus->objects[0].data.unitUT.objectStatusData, utBoardObj.dummyStatusData);
    UT_ASSERT_TRUE(LW_TRUE);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10ReadHeader)
{
    FLCN_STATUS status;
    LwU8        dataOut[9]      = { 0 };
    LwU8        dataSurface[9]  = { 0 };
    LwU8        in;
    LwU8        out;

    DMA_MOCK_MEMORY_REGION memoryRegion =
    {
        .pMemDesc = &PmuInitArgs.superSurface,
        .pData = dataSurface,
        .offsetOfData = 0,
        .sizeOfData = sizeof(dataSurface),
    };

    dataSurface[0] = 1;
    dataSurface[1] = 2;
    dataSurface[2] = 3;
    dataSurface[3] = 4;
    dataSurface[4] = 5;
    dataSurface[5] = 6;
    dataSurface[6] = 7;
    dataSurface[7] = 8;

    // Test result of pHeader being NULL (FLCN_ERR_ILWALID_ARGUMENT)
    status = boardObjGrpIfaceModel10ReadHeader(NULL, 1, memoryRegion.offsetOfData, 0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    // Test result of dmaWrite failing (forward dmaWrite error)
    dmaMockReadAddEntry(0, FLCN_ERROR);
    status = boardObjGrpIfaceModel10ReadHeader(dataOut, 1, memoryRegion.offsetOfData, 0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // Ensure the correct data was written to the correct location (ssOffset + writeOffset == 1 + 2)
    dmaMockReadAddEntry(1, FLCN_OK);
    dmaMockConfigMemoryRegionInsert(&memoryRegion);
    status = boardObjGrpIfaceModel10ReadHeader(dataOut, 4, 1, 2);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    UT_ASSERT_EQUAL_UINT(dataOut[0], dataSurface[3]);
    UT_ASSERT_EQUAL_UINT(dataOut[1], dataSurface[4]);
    UT_ASSERT_EQUAL_UINT(dataOut[2], dataSurface[5]);
    UT_ASSERT_EQUAL_UINT(dataOut[3], dataSurface[6]);
}

/*!
 * @brief
 *
 * @details
 */
UT_CASE_RUN(PMU_BOARDOBJGRP, boardObjGrpIfaceModel10WriteHeader)
{
    FLCN_STATUS status;
    LwU8        dataIn[9]       = { 0 };
    LwU8        dataSurface[9]  = { 0 };
    LwU8        in;
    LwU8        out;

    DMA_MOCK_MEMORY_REGION memoryRegion =
    {
        .pMemDesc = &PmuInitArgs.superSurface,
        .pData = dataSurface,
        .offsetOfData = 0,
        .sizeOfData = sizeof(dataSurface),
    };

    dataIn[0] = 1;
    dataIn[1] = 2;
    dataIn[2] = 3;
    dataIn[3] = 4;
    dataIn[4] = 5;
    dataIn[5] = 6;
    dataIn[6] = 7;
    dataIn[7] = 8;

    // Test result of pHeader being NULL (FLCN_ERR_ILWALID_ARGUMENT)
    status = boardObjGrpIfaceModel10WriteHeader(NULL, 1, memoryRegion.offsetOfData, 0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERR_ILWALID_ARGUMENT);

    // Test result of dmaWrite failing (forward dmaWrite error)
    dmaMockWriteAddEntry(0, FLCN_ERROR);
    status = boardObjGrpIfaceModel10WriteHeader(dataIn, 1, memoryRegion.offsetOfData, 0);
    UT_ASSERT_EQUAL_UINT(status, FLCN_ERROR);

    // Ensure the correct data was written to the correct location (ssOffset + writeOffset == 1 + 2)
    dmaMockWriteAddEntry(1, FLCN_OK);
    dmaMockConfigMemoryRegionInsert(&memoryRegion);
    status = boardObjGrpIfaceModel10WriteHeader(dataIn, 4, 1, 2);
    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);

    UT_ASSERT_EQUAL_UINT(dataSurface[3], dataIn[0]);
    UT_ASSERT_EQUAL_UINT(dataSurface[4], dataIn[1]);
    UT_ASSERT_EQUAL_UINT(dataSurface[5], dataIn[2]);
    UT_ASSERT_EQUAL_UINT(dataSurface[6], dataIn[3]);
}

/* ------------------------- Private Functions ------------------------------ */
static void* testSetup(void)
{
    dmaMockInit();
    boardObjGrpMaskImportMockInit();
    callocMockInit();
    memset(&unitTestBoardObjgrpE32, 0x0, sizeof(unitTestBoardObjgrpE32));
    return NULL;
}

static FLCN_STATUS
s_unitClassOneBoardObjGrpIfaceModel10Set
(
    PPMU_DMEM_BUFFER pBuffer
)
{
    return FLCN_ERR_TIMEOUT;
}

static FLCN_STATUS
s_unitClassOneBoardObjGrpIfaceModel10GetStatus
(
    PPMU_DMEM_BUFFER pBuffer
)
{
    return FLCN_ERR_NO_FREE_MEM;
}

static FLCN_STATUS
s_unitClassTwoBoardObjGrpIfaceModel10Set
(
    PPMU_DMEM_BUFFER pBuffer
)
{
    return FLCN_ERR_HDCP_ILWALID_SRM;
}

static FLCN_STATUS
s_unitClassTwoBoardObjGrpIfaceModel10GetStatus
(
    PPMU_DMEM_BUFFER pBuffer
)
{
    return FLCN_ERR_HDCP_RECV_REVOKED;
}

static FLCN_STATUS
s_unitClassFourBoardObjGrpIfaceModel10Set
(
    PPMU_DMEM_BUFFER pBuffer
)
{
    return FLCN_ERR_RPC_ILWALID_INPUT;
}

#define BoardObjGrpIfaceModel10SetHeader(fname) FLCN_STATUS (fname)(PBOARDOBJGRP pBObjGrp, PRM_PMU_BOARDOBJGRP pHdrDesc)

#define BoardObjGrpIfaceModel10SetEntry(fname) FLCN_STATUS (fname)(PBOARDOBJGRP pBObjGrp, PBOARDOBJ *ppBoardObj, PRM_PMU_BOARDOBJ pBuf)


static FLCN_STATUS
s_mockGrpIfaceModel10SetHeader
(
    PBOARDOBJGRP        pBObjGrp,
    PRM_PMU_BOARDOBJGRP pHdrDesc
)
{
    RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER *pHeader =
        (RM_PMU_UNIT_UT_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    UT_GRP_E32 *pGrp = (UT_GRP_E32 *)pBObjGrp;

    pGrp->dummySetData = pHeader->groupSetData;

    return FLCN_OK;
}

static FLCN_STATUS
s_mockGrpIfaceModel10SetEntry
(
    PBOARDOBJGRP        pBObjGrp,
    PBOARDOBJ          *ppBoardObj,
    PRM_PMU_BOARDOBJ    pBuf
)
{
    RM_PMU_UNIT_UT_SET *pEntry =
        (RM_PMU_UNIT_UT_SET *)pBuf;

    UT *pObj = (UT *)*ppBoardObj;

    pObj->dummySetData = pEntry->objectSetData;

    return FLCN_OK;
}

static FLCN_STATUS
s_mockGrpIfaceModel10SetHeaderFail
(
    PBOARDOBJGRP        pBObjGrp,
    PRM_PMU_BOARDOBJGRP pHdrDesc
)
{
    return FLCN_ERR_NOT_SUPPORTED;
}

static FLCN_STATUS
s_mockGrpIfaceModel10SetEntryFail
(
    PBOARDOBJGRP        pBObjGrp,
    PBOARDOBJ          *ppBoardObj,
    PRM_PMU_BOARDOBJ    pBuf
)
{
    return FLCN_ERR_ILWALID_FUNCTION;
}

static FLCN_STATUS
s_mockGrpIfaceModel10GetStatusHeader
(
    PBOARDOBJGRP        pBoardObjGrp,
    PRM_PMU_BOARDOBJGRP pBuf,
    PBOARDOBJGRPMASK    pMask
)
{
    RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER *pHeader =
        (RM_PMU_UNIT_UT_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;

    UT_GRP_E32 *pGrp = (UT_GRP_E32 *)pBoardObjGrp;

    pHeader->groupStatusData = pGrp->dummyStatusData;

    return FLCN_OK;
}

static FLCN_STATUS
s_mockGrpGetStatusEntry
(
    PBOARDOBJ           pBoardObj,
    PRM_PMU_BOARDOBJ    pBuf,
    LwLength            size,
    LwU8                index
)
{
    RM_PMU_UNIT_UT_GET_STATUS_SUPER *pEntry =
        (RM_PMU_UNIT_UT_GET_STATUS_SUPER *)pBuf;

    UT *pObj = (UT *)pBoardObj;

    UT_ASSERT_EQUAL_UINT(sizeof(RM_PMU_UNIT_UT_BOARDOBJ_GET_STATUS_UNION_ALIGNED), size);

    pEntry->objectStatusData = pObj->dummyStatusData;

    return FLCN_OK;
}

static FLCN_STATUS
s_mockGrpIfaceModel10GetStatusHeaderFail
(
    PBOARDOBJGRP        pBoardObjGrp,
    PRM_PMU_BOARDOBJGRP pBuf,
    PBOARDOBJGRPMASK    pMask
)
{
    return FLCN_ERR_NOT_SUPPORTED;
}

static FLCN_STATUS
s_mockGrpGetStatusEntryFail
(
    PBOARDOBJ           pBoardObj,
    PRM_PMU_BOARDOBJ    pBuf,
    LwLength            size,
    LwU8                index
)
{
    return FLCN_ERR_ILWALID_FUNCTION;
}

/* ------------------------- End of File ------------------------------------ */
