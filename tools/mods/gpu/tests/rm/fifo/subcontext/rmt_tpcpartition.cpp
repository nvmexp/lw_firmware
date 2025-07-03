/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_tpcpartition.cpp
//! \brief To verify basic functionality of TPC partitioning feature with subcontexts
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"

#include <string>
#include <map>
#include <ctime>

#include "core/include/memcheck.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "random.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "gpu/tests/rm/utility/rmtestutils.h"

#include "class/cl9067.h"       // FERMI_CONTEXT_SHARE_A
#include "class/cl90f1.h"       // FERMI_VASPACE_A
#include "class/clc397.h"       // VOLTA_A
#include "class/cla06fsubch.h"  // LWA06F_SUBCHANNEL_*
#include "class/cla197.h"       // KEPLER_B
#include "class/cla1c0.h"       // KEPLER_COMPUTE_B

#include "ctrl/ctrl9067.h"
#include "ctrl/ctrl2080/ctrl2080gr.h"

class TpcPartitionTest : public RmTest
{
public:
    TpcPartitionTest();
    virtual ~TpcPartitionTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    static const LW0080_CTRL_GR_TPC_PARTITION_MODE NONE    = LW0080_CTRL_GR_TPC_PARTITION_MODE_NONE;
    static const LW0080_CTRL_GR_TPC_PARTITION_MODE STATIC  = LW0080_CTRL_GR_TPC_PARTITION_MODE_STATIC;
    static const LW0080_CTRL_GR_TPC_PARTITION_MODE DYNAMIC = LW0080_CTRL_GR_TPC_PARTITION_MODE_DYNAMIC;

private:
    // functions
    RC WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken = false);
    RC WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset);
    RC ChangePartitionMode(LW0080_CTRL_GR_TPC_PARTITION_MODE nextMode, bool bEnableAllTpcs = false);
    RC ChangePartitionTable(LW0080_CTRL_GR_TPC_PARTITION_MODE mode);
    RC DoSomeWork(ChannelGroup *pChGrp);
    RC BasicTpcEnableFlagTest(ChannelGroup *pChGrp);
    RC BasicPartitionModeTransitionTest(ChannelGroup *pChGrp);
    RC BasicPartitionTableTransitionTest(LW0080_CTRL_GR_TPC_PARTITION_MODE mode, ChannelGroup *pChGrp);
    RC NegativeTpcPartitionTests(ChannelGroup *pChGrp);

    // vars
    Surface2D                        m_semaSurf;
    Random                           m_randomGenerator;
    LwRm::Handle                     hChGrp;
    LwRm::Handle                     hCh3D, hChCompute;
    LwRm::Handle                     hSubctx0, hSubctx1;
    map<LwRm::Handle, ThreeDAlloc>   m_3dAllocs;
    map<LwRm::Handle, ComputeAlloc>  m_computeAllocs;
    LwU32                            m_maxTpcCount;
};

//! \brief TpcPartitionTest constructor
//!
//------------------------------------------------------------------------------
TpcPartitionTest::TpcPartitionTest()
{
    SetName("TpcPartitionTest");
}

//! \brief TpcPartitionTest destructor
//!
//------------------------------------------------------------------------------
TpcPartitionTest::~TpcPartitionTest()
{
    // has been taken care of in cleanup()
}

//! \brief Check if channel groups are supported
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         false otherwise with reason
//------------------------------------------------------------------------------
string TpcPartitionTest::IsTestSupported()
{
    LwRmPtr pLwRm;
    LwU8    fifoCaps[LW0080_CTRL_FIFO_CAPS_TBL_SIZE];
    LW0080_CTRL_FIFO_GET_CAPS_PARAMS fifoCapsParams = {0};

    memset(fifoCaps, 0, LW0080_CTRL_FIFO_CAPS_TBL_SIZE);
    fifoCapsParams.capsTblSize = LW0080_CTRL_FIFO_CAPS_TBL_SIZE;
    fifoCapsParams.capsTbl     = (LwP64)fifoCaps;

    pLwRm->ControlByDevice(GetBoundGpuDevice(), LW0080_CTRL_CMD_FIFO_GET_CAPS,
        &fifoCapsParams, sizeof(fifoCapsParams));

    return (pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()) &&
            LW0080_CTRL_FIFO_GET_CAP(fifoCaps, LW0080_CTRL_FIFO_CAPS_MULTI_VAS_PER_CHANGRP)?
            RUN_RMTEST_TRUE : "Tpc partition test not supported");
}

//! \brief Setup 2D semaphore surface
//
//! \return OK if surface alloc and map succeeds
//------------------------------------------------------------------------------
RC TpcPartitionTest::Setup()
{
    RC rc = OK;
    LwU32 seed;

    CHECK_RC(InitFromJs());
    m_TestConfig.SetAllowMultipleChannels(true);

    // Setup 2D semaphore surface
    m_semaSurf.SetForceSizeAlloc(true);
    m_semaSurf.SetArrayPitch(1);
    m_semaSurf.SetArraySize(0x1000);
    m_semaSurf.SetColorFormat(ColorUtils::VOID32);
    m_semaSurf.SetAddressModel(Memory::Paging);
    m_semaSurf.SetLayout(Surface2D::Pitch);
    m_semaSurf.SetLocation(Memory::Fb);
    CHECK_RC(m_semaSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_semaSurf.Map());

    seed = (LwU32)time(NULL);
    m_randomGenerator.SeedRandom(seed);

    Printf(Tee::PriDebug, "TpcPartitionTest::Setup() completed\n");
    return rc;
}

//! \brief Run() funtion for TpcPartitionTest
//!
//! Do basic subctx, channel and object allocations and trigger tests
//!
//! \return corresponding RC if any allocation or test fails
//------------------------------------------------------------------------------
RC TpcPartitionTest::Run()
{
    RC               rc = OK;
    LwU32            flags;
    Subcontext      *pSubctx0  = NULL, *pSubctx1  = NULL;
    Channel         *pChCompute, *pCh3D;
    LwRm::Handle     hObjCompute, hObj3D;
    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS channelGroupParams = {0};
    channelGroupParams.engineType = LW2080_ENGINE_TYPE_GR(0);

    // Allocate a TSG and a method stream to split work between channels in TSG
    ChannelGroup chGrp(&m_TestConfig, &channelGroupParams);
    chGrp.SetUseVirtualContext(false);
    CHECK_RC_CLEANUP(chGrp.Alloc());
    hChGrp = chGrp.GetHandle();

    // Enable all TPCs in dynamic mode before allocating a subcontext
    CHECK_RC_CLEANUP(ChangePartitionMode(TpcPartitionTest::DYNAMIC, true));

    // Allocate subcontexts on default VAspace
    CHECK_RC_CLEANUP(chGrp.AllocSubcontext(&pSubctx0, 0, Subcontext::SYNC));
    CHECK_RC_CLEANUP(chGrp.AllocSubcontext(&pSubctx1, 0, Subcontext::ASYNC));
    hSubctx0 = pSubctx0->GetHandle();
    hSubctx1 = pSubctx1->GetHandle();

    // Allocate channels on the subcontexts
    flags = 0;
    flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, 0, flags);
    CHECK_RC_CLEANUP(chGrp.AllocChannelWithSubcontext(&pCh3D, pSubctx0, flags));

    flags = 0;
    flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, 1, flags);
    CHECK_RC_CLEANUP(chGrp.AllocChannelWithSubcontext(&pChCompute, pSubctx1, flags));

    // Allocate gr objects on the channels
    hCh3D = pCh3D->GetHandle();
    CHECK_RC_CLEANUP(m_3dAllocs[hCh3D].Alloc(hCh3D, GetBoundGpuDevice()));
    hObj3D = m_3dAllocs[hCh3D].GetHandle();
    CHECK_RC_CLEANUP(pCh3D->SetObject(LWA06F_SUBCHANNEL_3D, hObj3D));

    hChCompute = pChCompute->GetHandle();
    CHECK_RC_CLEANUP(m_computeAllocs[hChCompute].Alloc(hChCompute, GetBoundGpuDevice()));
    hObjCompute = m_computeAllocs[hChCompute].GetHandle();
    CHECK_RC_CLEANUP(pChCompute->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObjCompute));

    // Schedule
    CHECK_RC_CLEANUP(chGrp.Schedule());
    CHECK_RC(DoSomeWork(&chGrp));

    // When the 'bEnableAllTpcs' flag is set, partition table cannot be programmed.
    CHECK_RC_CLEANUP(BasicTpcEnableFlagTest(&chGrp));

    // Run basic mode transition test
    CHECK_RC_CLEANUP(BasicPartitionModeTransitionTest(&chGrp));

    // Run basic mode table transition test in STATIC and DYNAMIC mode
    CHECK_RC_CLEANUP(BasicPartitionTableTransitionTest(TpcPartitionTest::STATIC, &chGrp));
    CHECK_RC_CLEANUP(BasicPartitionTableTransitionTest(TpcPartitionTest::DYNAMIC, &chGrp));

    // Run negative test cases that should fail
    CHECK_RC_CLEANUP(NegativeTpcPartitionTests(&chGrp));

Cleanup:
    // Free allocated objects
    m_3dAllocs[hCh3D].Free();
    m_computeAllocs[hChCompute].Free();

    // Free channels
    if (pChCompute)
        chGrp.FreeChannel(pChCompute);

    if (pCh3D)
        chGrp.FreeChannel(pCh3D);

    // Free subcontexts
    if (pSubctx0)
    {
        chGrp.FreeSubcontext(pSubctx0);
        pSubctx0 = NULL;
    }

    if (pSubctx1)
    {
        chGrp.FreeSubcontext(pSubctx1);
        pSubctx1 = NULL;
    }

    // Free channel group
    chGrp.Free();

    Printf(Tee::PriDebug, "TpcPartitionTest::Run() completed\n");
    return rc;
}

//! \brief Cleanup() funtion for TpcPartitionTest
//!
//------------------------------------------------------------------------------
RC TpcPartitionTest::Cleanup()
{
    Printf(Tee::PriDebug, "TpcPartitionTest::Cleanup() completed\n");
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

RC TpcPartitionTest::ChangePartitionMode(LW0080_CTRL_GR_TPC_PARTITION_MODE nextMode, bool bEnableAllTpcs)
{
    RC        rc = OK;
    LwRmPtr   pLwRm;
    LW0080_CTRL_GR_TPC_PARTITION_MODE_PARAMS modeParams={0};
    modeParams.hChannelGroup = hChGrp;

    // Check if lwrrentMode == nextMode, if yes return early
    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                        LW0080_CTRL_CMD_GR_GET_TPC_PARTITION_MODE,
                        (void *)&modeParams,
                        sizeof(modeParams)));

    if(modeParams.mode == nextMode)
    {
        Printf(Tee::PriDebug, "Already in requested mode, returning early success...\n");
        return rc;
    }
    Printf(Tee::PriDebug, "Previous partition mode was %d\n", (LwU32)modeParams.mode);

    // Do the requested mode change
    modeParams.mode = nextMode;
    modeParams.bEnableAllTpcs = bEnableAllTpcs;
    CHECK_RC(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                        LW0080_CTRL_CMD_GR_SET_TPC_PARTITION_MODE,
                        (void *)&modeParams,
                        sizeof(modeParams)));

    Printf(Tee::PriDebug, "Current partition mode is %d\n", (LwU32)modeParams.mode);

    return rc;
}

RC TpcPartitionTest::ChangePartitionTable(LW0080_CTRL_GR_TPC_PARTITION_MODE mode)
{
    RC            rc = OK;
    LwRmPtr       pLwRm;
    LwU32         i;
    LwU32         tpcCnt_subctx0, tpcCnt_subctx1;
    LW9067_CTRL_TPC_PARTITION_TABLE_PARAMS     tableParams = {0};
    LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO   tpcLmemIndexInfo[LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX] = {{0, 0}};
    LW2080_CTRL_GR_GET_GLOBAL_SM_ORDER_PARAMS  smOrderParams = {0};

    // Find the max number of TPCs enabled
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_GR_GET_GLOBAL_SM_ORDER,
                        (void *)&smOrderParams,
                        sizeof(smOrderParams)));

    m_maxTpcCount = (LwU32)smOrderParams.numTpc;

    // Lets (randomly) choose number of TPCs on which subctx0 and subctx1 can run
    tpcCnt_subctx0 = m_randomGenerator.GetRandom(1, (LwU32)smOrderParams.numTpc);
    tpcCnt_subctx1 = m_randomGenerator.GetRandom(1, (LwU32)smOrderParams.numTpc);
    Printf(Tee::PriDebug, "tpcCnt val0 = %d\ntpcCnt val1 = %d\n", tpcCnt_subctx0, tpcCnt_subctx1);

    // Configure the partition table for subcontext-0
    tableParams.numUsedTpc = tpcCnt_subctx0;
    for (i = 0; i < tableParams.numUsedTpc; i++)
    {
        tpcLmemIndexInfo[i].globalTpcIndex = i;
        if(mode != TpcPartitionTest::DYNAMIC)
        {
            tpcLmemIndexInfo[i].lmemBlockIndex = i;
        }
        else
        {
            tpcLmemIndexInfo[i].lmemBlockIndex = 0;   // lmem index is not used in DYNAMIC mode
        }
    }

    memcpy(tableParams.tpcList, tpcLmemIndexInfo, LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX * sizeof(LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO));
    CHECK_RC(pLwRm->Control(hSubctx0,
                        LW9067_CTRL_CMD_SET_TPC_PARTITION_TABLE,
                        (void *)&tableParams,
                        sizeof(tableParams)));

    // Configure the partition table for subcontext-1
    memset(&tableParams, 0, sizeof(LW9067_CTRL_TPC_PARTITION_TABLE_PARAMS));
    memset(tpcLmemIndexInfo, 0, LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX * sizeof(LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO));

    tableParams.numUsedTpc = tpcCnt_subctx1;
    for (i = 0; i < tableParams.numUsedTpc; i++)
    {
        tpcLmemIndexInfo[i].globalTpcIndex = i;
        if(mode != TpcPartitionTest::DYNAMIC)
        {
            tpcLmemIndexInfo[i].lmemBlockIndex = i;
        }
        else
        {
            tpcLmemIndexInfo[i].lmemBlockIndex = 0;   // lmem index is not used in DYNAMIC mode
        }
    }

    memcpy(tableParams.tpcList, tpcLmemIndexInfo, LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX * sizeof(LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO));
    CHECK_RC(pLwRm->Control(hSubctx1,
                        LW9067_CTRL_CMD_SET_TPC_PARTITION_TABLE,
                        (void *)&tableParams,
                        sizeof(tableParams)));
    return rc;
}

RC TpcPartitionTest::WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset)
{
    RC rc = OK;
    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;
}

RC TpcPartitionTest::WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken)
{
    RC rc = OK;

    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(A197, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(A197, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(A1C0, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(A1C0, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;
}

RC TpcPartitionTest::DoSomeWork(ChannelGroup *pChGrp)
{
    RC rc = OK;
    ChannelGroup::SplitMethodStream stream(pChGrp);

    // Do some work
    CHECK_RC(WriteBackendOffset(&stream, hCh3D, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC(WriteBackendRelease(&stream, hCh3D, LWA06F_SUBCHANNEL_3D, 0));
    CHECK_RC(WriteBackendOffset(&stream, hChCompute, LWA06F_SUBCHANNEL_COMPUTE, m_semaSurf.GetCtxDmaOffsetGpu() + 0x10));
    CHECK_RC(WriteBackendRelease(&stream, hChCompute, LWA06F_SUBCHANNEL_COMPUTE, 0));

    CHECK_RC(pChGrp->Flush());
    CHECK_RC(pChGrp->WaitIdle());

    // Check if work completed as expected
    if ((MEM_RD32(m_semaSurf.GetAddress()) != 0) ||
        (MEM_RD32((LwU8*) m_semaSurf.GetAddress()+ 0x10) != 0))
        rc = RC::DATA_MISMATCH;

    return rc;
}

RC TpcPartitionTest::BasicPartitionModeTransitionTest(ChannelGroup *pChGrp)
{
    RC rc = OK;

    //
    // N=None, S=Static, D=Dynamic
    // Change partition mode in order : N-->S-->D-->S-->N-->D-->N (this covers all possible transitions)
    // Do some work in default(NONE) mode and after each mode change
    //
    CHECK_RC(ChangePartitionMode(TpcPartitionTest::STATIC));   // N-->S
    CHECK_RC(DoSomeWork(pChGrp));
    CHECK_RC(ChangePartitionMode(TpcPartitionTest::DYNAMIC));  // S-->D
    CHECK_RC(DoSomeWork(pChGrp));
    CHECK_RC(ChangePartitionMode(TpcPartitionTest::STATIC));   // D-->S
    CHECK_RC(DoSomeWork(pChGrp));
    CHECK_RC(ChangePartitionMode(TpcPartitionTest::NONE));     // S-->N
    CHECK_RC(DoSomeWork(pChGrp));
    CHECK_RC(ChangePartitionMode(TpcPartitionTest::DYNAMIC));  // N-->D
    CHECK_RC(DoSomeWork(pChGrp));
    CHECK_RC(ChangePartitionMode(TpcPartitionTest::NONE));     // D-->N
    CHECK_RC(DoSomeWork(pChGrp));

    Printf(Tee::PriDebug, "BasicPartitionModeTransitionTest() completed\n");
    return rc;
}

RC TpcPartitionTest::BasicPartitionTableTransitionTest(LW0080_CTRL_GR_TPC_PARTITION_MODE mode, ChannelGroup *pChGrp)
{
    RC            rc = OK;

    // First set the requested TPC partition mode
    CHECK_RC(ChangePartitionMode(mode));

    // Now change the TPC partition table
    CHECK_RC(ChangePartitionTable(mode));

    // Do some work with above partition table config
    CHECK_RC(DoSomeWork(pChGrp));

    Printf(Tee::PriDebug, "BasicPartitionTableTransitionTest() completed\n");
    return rc;
}

RC TpcPartitionTest::BasicTpcEnableFlagTest(ChannelGroup *pChGrp)
{
    RC            rc = OK;
    LwRmPtr       pLwRm;
    LW0080_CTRL_GR_TPC_PARTITION_MODE_PARAMS modeParams={0};

    modeParams.hChannelGroup = hChGrp;

    // Confirm that the flag is lwrrently set and mode was set correctly
    rc = pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                        LW0080_CTRL_CMD_GR_GET_TPC_PARTITION_MODE,
                        (void *)&modeParams,
                        sizeof(modeParams));

    if (modeParams.bEnableAllTpcs != LW_TRUE)
    {
        Printf(Tee::PriError, "All TPCs not enabled.. invalid state\n");
        return RC::SOFTWARE_ERROR;
    }

    if (modeParams.mode != TpcPartitionTest::DYNAMIC)
    {
        Printf(Tee::PriError, "Mode was not set correctly.. invalid state\n");
        return RC::SOFTWARE_ERROR;
    }

    // Try to change partition table. This should fail
    rc = ChangePartitionTable(TpcPartitionTest::STATIC);
    if (rc == OK)
    {
        Printf(Tee::PriError, "Partition table changed with 'bEnableAllTpcs' flag set.. invalid state\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        rc.Clear();
        // Reset the flag so that partition table can be changed after this point.
        rc = ChangePartitionMode(TpcPartitionTest::NONE, false);
    }
    Printf(Tee::PriDebug, "BasicTpcEnableTest() completed\n");
    return rc;
}

RC TpcPartitionTest::NegativeTpcPartitionTests(ChannelGroup *pChGrp)
{
    RC            rc = OK;
    LwRmPtr       pLwRm;
    LW9067_CTRL_TPC_PARTITION_TABLE_PARAMS     tableParams = {0};
    LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO   tpcLmemIndexInfo[LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX] = {{0, 0}};

    // Negative test 1 : Partition Table updates with NONE mode - this should fail
    DISABLE_BREAK_COND(nobp, true);
    rc = BasicPartitionTableTransitionTest(TpcPartitionTest::NONE, pChGrp);
    DISABLE_BREAK_END(nobp);

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "This call should have failed. Failing test..\n");
        CHECK_RC_LABEL(Exit, RC::SOFTWARE_ERROR);
    }
    rc.Clear();

    // Negative test 2 : Partition Table updates with invalid TPC count
    CHECK_RC(ChangePartitionMode(TpcPartitionTest::DYNAMIC));
    tableParams.numUsedTpc = m_maxTpcCount*2;   // invalid count (tpc > maxcount)
    memcpy(tableParams.tpcList, tpcLmemIndexInfo, LW9067_CTRL_TPC_PARTITION_TABLE_TPC_COUNT_MAX * sizeof(LW9067_CTRL_TPC_PARTITION_TABLE_TPC_INFO));

    DISABLE_BREAK_COND(nobp, true);
    rc = pLwRm->Control(hSubctx0,
                        LW9067_CTRL_CMD_SET_TPC_PARTITION_TABLE,
                        (void *)&tableParams,
                        sizeof(tableParams));
    DISABLE_BREAK_END(nobp);

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "This call should have failed. Failing test..\n");
        CHECK_RC_LABEL(Exit, RC::SOFTWARE_ERROR);
    }

    // reached here means all negative tests failed as expected
    Printf(Tee::PriHigh, "All negative tests failed as expected\n");
    rc.Clear();

Exit:
    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(TpcPartitionTest, RmTest,
                 "Subcontext TPC partitioning test.");
