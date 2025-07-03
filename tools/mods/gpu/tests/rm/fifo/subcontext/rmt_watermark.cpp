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
//! \file rmt_watermark.cpp
//! \brief To verify Watermark SKED feature with subcontexts
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

class WatermarkTest : public RmTest
{
public:
    WatermarkTest();
    virtual ~WatermarkTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    // functions
    RC WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken = false);
    RC WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset);
    RC ChangePartitionMode(LW0080_CTRL_GR_TPC_PARTITION_MODE nextMode);
    RC DoSomeWork(ChannelGroup *pChGrp);
    RC BasicWatermarkTest(ChannelGroup *pChGrp);
    RC BasicDefaultWatermarkTest(ChannelGroup *pChGrp);
    RC NegativeWatermarkTest(ChannelGroup *pChGrp);

    // vars
    Surface2D                        m_semaSurf;
    Random                           m_randomGenerator;
    LwRm::Handle                     hChGrp;
    LwRm::Handle                     hCh3D, hChCompute;
    LwRm::Handle                     hSubctx0, hSubctx1;
    map<LwRm::Handle, ThreeDAlloc>   m_3dAllocs;
    map<LwRm::Handle, ComputeAlloc>  m_computeAllocs;
};

//! \brief WatermarkTest constructor
//!
//------------------------------------------------------------------------------
WatermarkTest::WatermarkTest()
{
    SetName("WatermarkTest");
}

//! \brief WatermarkTest destructor
//!
//------------------------------------------------------------------------------
WatermarkTest::~WatermarkTest()
{
    // has been taken care of in cleanup()
}

//! \brief Check if channel groups are supported
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         false otherwise with reason
//------------------------------------------------------------------------------
string WatermarkTest::IsTestSupported()
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
            RUN_RMTEST_TRUE : "Watermark test not supported");
}

//! \brief Setup 2D semaphore surface
//
//! \return OK if surface alloc and map succeeds
//------------------------------------------------------------------------------
RC WatermarkTest::Setup()
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

    Printf(Tee::PriDebug, "WatermarkTest::Setup() completed\n");
    return rc;
}

//! \brief Run() funtion for WatermarkTest
//!
//! Do basic subctx, channel and object allocations and trigger tests
//!
//! \return corresponding RC if any allocation or test fails
//------------------------------------------------------------------------------
RC WatermarkTest::Run()
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

    // Allocate subcontexts on default VAspace
    CHECK_RC_CLEANUP(chGrp.AllocSubcontext(&pSubctx0, 0, Subcontext::SYNC));
    CHECK_RC_CLEANUP(chGrp.AllocSubcontext(&pSubctx1, 0, Subcontext::ASYNC));
    hSubctx0 = pSubctx0->GetHandle();
    hSubctx1 = pSubctx1->GetHandle();

    // Check if default value watermark value is set before GR alloc
    CHECK_RC_CLEANUP(BasicDefaultWatermarkTest(&chGrp));

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

    // Check if default value watermark value is set after GR alloc
    CHECK_RC_CLEANUP(BasicDefaultWatermarkTest(&chGrp));

    // Schedule
    CHECK_RC_CLEANUP(chGrp.Schedule());
    CHECK_RC_CLEANUP(DoSomeWork(&chGrp));

    // Run basic Watermark test a couple of times to verify transitions
    CHECK_RC_CLEANUP(BasicWatermarkTest(&chGrp));
    CHECK_RC_CLEANUP(BasicWatermarkTest(&chGrp));

    // Run negative tests
    CHECK_RC_CLEANUP(NegativeWatermarkTest(&chGrp));

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

    Printf(Tee::PriDebug, "WatermarkTest::Run() completed\n");
    return rc;
}

//! \brief Cleanup() funtion for WatermarkTest
//!
//------------------------------------------------------------------------------
RC WatermarkTest::Cleanup()
{
    Printf(Tee::PriDebug, "WatermarkTest::Cleanup() completed\n");
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

RC WatermarkTest::WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset)
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

RC WatermarkTest::WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken)
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

RC WatermarkTest::DoSomeWork(ChannelGroup *pChGrp)
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

RC WatermarkTest::BasicDefaultWatermarkTest(ChannelGroup *pChGrp)
{
    RC       rc = OK;
    LwRmPtr  pLwRm;
    LW9067_CTRL_CWD_WATERMARK_PARAMS waterMarkParams = {0};

    // Check if the default valid watermark value is set to start with
    memset(&waterMarkParams, 0, sizeof(LW9067_CTRL_CWD_WATERMARK_PARAMS));
    CHECK_RC(pLwRm->Control(hSubctx0,
                        LW9067_CTRL_CMD_GET_CWD_WATERMARK,
                        (void *)&waterMarkParams,
                        sizeof(waterMarkParams)));

    if (waterMarkParams.watermarkValue != LW9067_CTRL_CWD_WATERMARK_VALUE_DEFAULT)
        return RC::SOFTWARE_ERROR;

    // Check if the default valid watermark value is set to start with
    memset(&waterMarkParams, 0, sizeof(LW9067_CTRL_CWD_WATERMARK_PARAMS));
    CHECK_RC(pLwRm->Control(hSubctx1,
                        LW9067_CTRL_CMD_GET_CWD_WATERMARK,
                        (void *)&waterMarkParams,
                        sizeof(waterMarkParams)));

    if (waterMarkParams.watermarkValue != LW9067_CTRL_CWD_WATERMARK_VALUE_DEFAULT)
        return RC::SOFTWARE_ERROR;

    Printf(Tee::PriHigh, "BasicDefaultWatermarkTest() completed\n");

    return rc;
}

RC WatermarkTest::BasicWatermarkTest(ChannelGroup *pChGrp)
{
    RC       rc = OK;
    LwRmPtr  pLwRm;
    LwU32    wtrmrk_val0, wtrmrk_val1;
    LW9067_CTRL_CWD_WATERMARK_PARAMS waterMarkParams = {0};

    // Get random Watermark value per-subcontext
    wtrmrk_val0 = m_randomGenerator.GetRandom(LW9067_CTRL_CWD_WATERMARK_VALUE_MIN, LW9067_CTRL_CWD_WATERMARK_VALUE_MAX);
    wtrmrk_val1 = m_randomGenerator.GetRandom(LW9067_CTRL_CWD_WATERMARK_VALUE_MIN, LW9067_CTRL_CWD_WATERMARK_VALUE_MAX);
    Printf(Tee::PriDebug, "Watermark val0 = %d\nWatermark val1 = %d\n", wtrmrk_val0, wtrmrk_val1);

    // Set watermark for subcontext-0
    waterMarkParams.watermarkValue = wtrmrk_val0;   // random choice
    CHECK_RC(pLwRm->Control(hSubctx0,
                        LW9067_CTRL_CMD_SET_CWD_WATERMARK,
                        (void *)&waterMarkParams,
                        sizeof(waterMarkParams)));

    // Set watermark for subcontext-1
    memset(&waterMarkParams, 0, sizeof(LW9067_CTRL_CWD_WATERMARK_PARAMS));
    waterMarkParams.watermarkValue = wtrmrk_val1;   // random choice
    CHECK_RC(pLwRm->Control(hSubctx1,
                        LW9067_CTRL_CMD_SET_CWD_WATERMARK,
                        (void *)&waterMarkParams,
                        sizeof(waterMarkParams)));

    // Do some work
    CHECK_RC(DoSomeWork(pChGrp));
    Printf(Tee::PriHigh, "BasicWatermarkTest() completed\n");

    return rc;
}

RC WatermarkTest::NegativeWatermarkTest(ChannelGroup *pChGrp)
{
    RC       rc = OK;
    LwRmPtr  pLwRm;
    LW9067_CTRL_CWD_WATERMARK_PARAMS waterMarkParams = {0};

    // Try setting a invalid watermark value for a subcontext, this should fail
    memset(&waterMarkParams, 0, sizeof(LW9067_CTRL_CWD_WATERMARK_PARAMS));
    waterMarkParams.watermarkValue = LW9067_CTRL_CWD_WATERMARK_VALUE_MAX + 1;

    DISABLE_BREAK_COND(nobp, true);
    rc = pLwRm->Control(hSubctx0,
                        LW9067_CTRL_CMD_SET_CWD_WATERMARK,
                        (void *)&waterMarkParams,
                        sizeof(waterMarkParams));
    DISABLE_BREAK_END(nobp);

    if (rc == OK)
    {
        Printf(Tee::PriHigh, "This call should have failed. Failing test..\n");
        rc = RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh, "Negative test failed as expected\n");
        rc.Clear();
    }

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(WatermarkTest, RmTest,
                 "Subcontext Watermark test.");
