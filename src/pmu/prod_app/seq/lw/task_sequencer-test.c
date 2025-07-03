/*
 * Copyright (c) 2019-2020, LWPU CORPORATION. All rights reserved.
 *
 * LWPU CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from LWPU CORPORATION is strictly prohibited.
 */

/* ------------------------- System Includes -------------------------------- */
/*
 * ****************************** TEST INFO ************************************
 * This test is lwrrently used to test out s_seqScriptRunSuperSurface funcionality
 * Things done to make this test work(for tu10x):
 * 1. Edit make-unittest.lwmk present in pmu_sw/prod_app
 *      a) Change config profile to pmu-tu10x
 *      b) Comment out all test sources and unit sources, except for pmu_objpmu-test.c
 *      c) Uncomment task_sequence-test.c dependencies
 * 2. Remove LWOS tests from UTF build ( found in chips_a/tools/UTF/ucode/common/tests/testlist.mk)
 */

#include "test-macros.h"
#include <ut/ut.h>
#include "ut/ut_case.h"
#include "ut/ut_assert.h"
#include "regmock.h"
#include "lwrtos.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objseq.h"
#include "pmu/pmuifpmu.h"
#include "pmu/pmuifseq.h"
#include "pmu_objfifo.h"
#include "lwos_dma-mock.h"
/* ------------------------ Mocked Function Prototypes ---------------------- */
/* ------------------------  Mocked functions ------------------------------- */
void appTaskCriticalEnter();
void appTaskCriticalExit();

/* ------------------------ Mocked Global Variables ------------------------- */
/*!
 * @brief Fake PmuPrivLevelCtrlRegAddr for PMU_BAR0_WR32_SAFE
 *
 * @note This variable is initialized to an 2^32 - 1 because
 *       there should be no chance for that address to be hit.
 *       This variable is just used for definitin purposes, because
 *       some files that this test depends on depend on existence of this
 *       variable.
 */
LwU32 PmuPrivLevelCtrlRegAddr = 0xffffffff;

/*!
 * @brief      Files we are dependent on require this variable to be defined.
 */
OBJFIFO Fifo;

/*!
 * @brief      Fake @ref RM_PMU_CMD_LINE_ARGS because this file and other files
 *             depend on definition of this variable.
 */
RM_PMU_CMD_LINE_ARGS PmuInitArgs;

/* ------------------------ Unit-Under-Test --------------------------------- */
#include "task_sequencer.c"

/* ------------------------ Local Variables --------------------------------- */

/*!
 * @brief Seq script buffer that will be used for testing purposes.
 */
static RM_PMU_SEQ_SCRIPT_BUFFER seqScriptBuffer;
static DMA_MOCK_MEMORY_REGION   superSurfaceMemoryRegion;

#define LW_PMU_SEQ_INSTR_OPC_BYTE_IDX  (0x0)
#define LW_PMU_SEQ_INSTR_SIZE_BYTE_IDX (0x2)

/* ------------------------- Prototypes ------------------------------------- */
static void testSetupHook();
static void testTeardownHook();
static void copyToSequencerBufferSuperSurface(LwU8 *pData, LwU32 size, LwU32 offset);

/* ------------------------------- UTF Tests -------------------------------- */
UT_SUITE_DEFINE(PMU_TASK_SEQUENCER,
                UT_SUITE_SET_COMPONENT("PMU_SEQ")
                UT_SUITE_SET_DESCRIPTION("Tests for PMU SEQ Engine.")
                UT_SUITE_SET_OWNER("pmu")
                UT_SUITE_SETUP_HOOK(testSetupHook)
                UT_SUITE_TEARDOWN_HOOK(testTeardownHook))

UT_CASE_DEFINE(PMU_TASK_SEQUENCER, s_seqScriptRunSuperSurface_BufferIdx_Ilwalid,
    UT_CASE_SET_DESCRIPTION("Test whether s_seqScriptRunSuperSurface will report error for invalid buffer")
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_TASK_SEQUENCER, s_seqScriptRunSuperSurface_RunScript,
    UT_CASE_SET_DESCRIPTION("Testing whether script successfuly returns with SEQ_EXIT operation.")
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_TASK_SEQUENCER, s_seqScriptRunSuperSurface_RunScript_Ilwalid,
    UT_CASE_SET_DESCRIPTION("Test case should fail because SEQ_EXIT operation is not present.")
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("TODO"))

UT_CASE_RUN(PMU_TASK_SEQUENCER, s_seqScriptRunSuperSurface_BufferIdx_Ilwalid)
{
    UT_ASSERT(FLCN_OK != s_seqScriptRunSuperSurface(RM_PMU_SEQ_SCRIPT_BUFFER_COUNT));
}

UT_CASE_RUN(PMU_TASK_SEQUENCER, s_seqScriptRunSuperSurface_RunScript)
{
    //
    // Input payload, just execute the EXIT intruction.
    //
    LwU8  inPayload[8]   = {0U};
    LwU32 instrSizeBytes = LW_PMU_SEQ_OPC_SIZE_BYTES(EXIT);

    inPayload[LW_PMU_SEQ_INSTR_OPC_BYTE_IDX]  = LW_PMU_SEQ_EXIT_OPC;
    inPayload[LW_PMU_SEQ_INSTR_SIZE_BYTE_IDX] = LW_PMU_SEQ_OPC_SIZE_WORDS(EXIT);

    copyToSequencerBufferSuperSurface(inPayload, instrSizeBytes, 0U);

    UT_ASSERT(s_seqScriptRunSuperSurface(0U) == FLCN_OK);
}

UT_CASE_RUN(PMU_TASK_SEQUENCER, s_seqScriptRunSuperSurface_RunScript_Ilwalid)
{
    //
    // Input payload for LOAD ACC instruction.
    //
    LwU8  inPayload[8]   = {0U};
    LwU32 instrSizeWords = LW_PMU_SEQ_OPC_SIZE_WORDS(LOAD_ACC);
    LwU32 instrSizeBytes = LW_PMU_SEQ_OPC_SIZE_BYTES(LOAD_ACC);
    LwU8  i;

    inPayload[LW_PMU_SEQ_INSTR_OPC_BYTE_IDX]  = LW_PMU_SEQ_LOAD_ACC_OPC;
    inPayload[LW_PMU_SEQ_INSTR_SIZE_BYTE_IDX] = instrSizeWords;

    //
    // We need to have a block of instructions, in order to see whether this will fail.
    // If we don't do that, we'll get stuck in an infinite loop.
    //
    for (i = 0U; i < LW_PMU_SEQ_DMA_BLOCK_SIZE_BYTES / instrSizeBytes; ++i)
    {
        copyToSequencerBufferSuperSurface(inPayload, instrSizeBytes, i * instrSizeBytes);
    }

    UT_ASSERT(FLCN_OK != s_seqScriptRunSuperSurface(0U));
}

/* ------------------------  Mocked functions ------------------------------- */
void
appTaskCriticalEnter()
{

}

void
appTaskCriticalExit()
{

}

/* ------------------------ Static Functions  ------------------------------- */
static void testSetupHook()
{
    superSurfaceMemoryRegion.pMemDesc     = &PmuInitArgs.superSurface;
    superSurfaceMemoryRegion.pData        = &seqScriptBuffer;
    superSurfaceMemoryRegion.offsetOfData = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(
                            all.seq.seqScriptSurface.seqScriptBuffers[0]);
    superSurfaceMemoryRegion.sizeOfData   = sizeof(seqScriptBuffer);

    dmaMockConfigMemoryRegionInsert(&superSurfaceMemoryRegion);

    utf_printf("Successfully set up test\n");
}

static void testTeardownHook()
{
    dmaMockConfigMemoryRegionRemove(&superSurfaceMemoryRegion);

    utf_printf("Successfully performed teardown\n");
}

static void copyToSequencerBufferSuperSurface(LwU8 *pData, LwU32 size, LwU32 offset)
{
    LwU8 *pSuperSurfaceBuffer = (LwU8*)&seqScriptBuffer;

    memcpy(pSuperSurfaceBuffer + offset, pData, size);
}
