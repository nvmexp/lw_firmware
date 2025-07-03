/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Preemption test.
//

#include <string>          // Only use <> for built-in C++ stuff
#include <vector>
#include <algorithm>
#include <map>

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"

#include "gpu/tests/gputestc.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"

#include "ctrl/ctrlc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "ctrl/ctrla06c.h" // KEPLER_CHANNEL_GROUP_A
#include "ctrl/ctrl2080.h" // LW20_SUBDEVICE_XX

#include "class/clc097.h" // PASCAL_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc0c0.h" // PASCAL_COMPUTE_A
#include "class/clc397.h" // VOLTA_A
#include "class/clc597.h" // TURING_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc997.h" // ADA_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clcd97.h" // BLACKWELL_A
#include "class/cl9097sw.h"
#include "class/cla06fsubch.h"
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define NUM_TEST_CHANNELS           2

class PreemptionTest: public RmTest
{
public:
    PreemptionTest();
    virtual ~PreemptionTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual RC SimpleModeTests();
    virtual RC TsgModeTests();

private:
    Channel *       m_pCh[NUM_TEST_CHANNELS] = {};

    LwRm::Handle    m_hCh[NUM_TEST_CHANNELS] = {};

    ThreeDAlloc     m_3dAlloc[NUM_TEST_CHANNELS];
    FLOAT64         m_TimeoutMs = Tasker::NO_TIMEOUT;

    ComputeAlloc    m_computeAlloc[NUM_TEST_CHANNELS];

    Surface2D       m_semaSurf;
    bool            m_bVirtCtx  = false;
    LwBool          m_bUnbind   = LW_FALSE;

    RC AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj);
    void FreeObjects(Channel *pCh);

    RC WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken = false);
    RC WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset);

    map<LwRm::Handle, ThreeDAlloc> m_3dAllocs;
    map<LwRm::Handle, ComputeAlloc> m_computeAllocs;

    RC Basic3dGroupTest(LwBool bSetGfx, LwU32 gfxMode, LwBool bSetComp, LwU32 compMode, LwBool bShouldPass, LwBool bEnableScg);
    RC BasicGfxpTest(LwBool bSetGfx, LwU32 gfxMode, LwBool bSetComp, LwU32 compMode, LwBool bShouldPass);
    RC BasicComputeCilpTest();
};

//!
//! \brief PreemptionTest (GfxP/CILP preemption test) constructor
//!
//! PreemptionTest (GfxP/CILP preemption test) constructor does not do much.
//!
//! \sa Setup
//------------------------------------------------------------------------------
PreemptionTest::PreemptionTest()
{
    SetName("PreemptionTest");
}

//!
//! \brief PreemptionTest (GfxP/CILP preemption test) destructor
//!
//! PreemptionTest (GfxP/CILP preemption test) destructor does not do much.  Functionality
//! mostly lies in Cleanup().
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
PreemptionTest::~PreemptionTest()
{
}

//!
//! \brief Is PreemptionTest (GfxP/CILP preemption test) supported?
//!
//! Verify if PreemptionTest (GfxP/CILP preemption test) is supported in the current
//! environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------
string PreemptionTest::IsTestSupported()
{
    LwRmPtr    pLwRm;

    if (!pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()))
        return "Channel groups not supported";
    else if (!pLwRm->IsClassSupported(PASCAL_A,    GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(PASCAL_B,    GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(VOLTA_A,     GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(TURING_A,    GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(AMPERE_A,    GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(AMPERE_B,    GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(ADA_A,       GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(HOPPER_A,    GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(HOPPER_B,    GetBoundGpuDevice()) &&
             !pLwRm->IsClassSupported(BLACKWELL_A, GetBoundGpuDevice()))
        return "PASCAL_A or PASCAL_B or VOLTA_A class not supported";

    return RUN_RMTEST_TRUE;
}

//!
//! \brief PreemptionTest (GfxP/CILP preemption test) Setup
//!
//! \return RC OK if all's well.
//------------------------------------------------------------------------------
RC PreemptionTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    LW0080_CTRL_FIFO_GET_CAPS_PARAMS fifoCapsParams = {0};
    LwU8 *pFifoCaps = NULL;

    CHECK_RC(InitFromJs());

    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    // We will be using multiple channels in this test
    m_TestConfig.SetAllowMultipleChannels(true);

    pFifoCaps = new LwU8[LW0080_CTRL_FIFO_CAPS_TBL_SIZE];
    if (pFifoCaps == NULL)
    {
        return RC::LWRM_INSUFFICIENT_RESOURCES;
    }
    fifoCapsParams.capsTblSize = LW0080_CTRL_FIFO_CAPS_TBL_SIZE;
    fifoCapsParams.capsTbl = (LwP64) pFifoCaps;
    if (pLwRm->ControlByDevice(GetBoundGpuDevice(), LW0080_CTRL_CMD_FIFO_GET_CAPS,
                                                    &fifoCapsParams, sizeof(fifoCapsParams)))
    {
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC PreemptionTest::Run()
{
    RC rc;

    m_bUnbind = LW_TRUE;
    CHECK_RC(BasicGfxpTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP, LW_FALSE, 0xffffffff, LW_TRUE));
    m_bUnbind = LW_FALSE;

    CHECK_RC(SimpleModeTests());
    CHECK_RC(TsgModeTests());

    CHECK_RC(BasicComputeCilpTest());

    return rc;
}

RC PreemptionTest::SimpleModeTests()
{
    RC rc;

    CHECK_RC(BasicGfxpTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI, LW_FALSE, 0xffffffff, LW_TRUE));
    CHECK_RC(BasicGfxpTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP, LW_FALSE, 0xffffffff, LW_TRUE));
    CHECK_RC(BasicGfxpTest(LW_FALSE, 0, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI, LW_TRUE));
    CHECK_RC(BasicGfxpTest(LW_FALSE, 0, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA, LW_TRUE));

    CHECK_RC(BasicGfxpTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI, LW_TRUE));
    CHECK_RC(BasicGfxpTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA, LW_TRUE));

    CHECK_RC(BasicGfxpTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI, LW_TRUE));
    CHECK_RC(BasicGfxpTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA, LW_TRUE));

    CHECK_RC(BasicGfxpTest(LW_TRUE, 0xf, LW_FALSE, 0xffffffff, LW_FALSE));
    CHECK_RC(BasicGfxpTest(LW_FALSE, 0xffffffff, LW_TRUE, 0xff, LW_FALSE));

    return rc;
}

RC PreemptionTest::TsgModeTests()
{
    RC rc;

    // run series of tests against channel group
    CHECK_RC(Basic3dGroupTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI, LW_FALSE, 0xffffffff, LW_TRUE, LW_FALSE));

    CHECK_RC(Basic3dGroupTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP, LW_FALSE, 0xffffffff, LW_TRUE, LW_FALSE));
    CHECK_RC(Basic3dGroupTest(LW_FALSE, 0, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI, LW_TRUE, LW_FALSE));
    CHECK_RC(Basic3dGroupTest(LW_FALSE, 0, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA, LW_TRUE, LW_FALSE));

    CHECK_RC(Basic3dGroupTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI, LW_TRUE, LW_FALSE));
    CHECK_RC(Basic3dGroupTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA, LW_TRUE, LW_FALSE));

    CHECK_RC(Basic3dGroupTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI, LW_TRUE, LW_FALSE));
    CHECK_RC(Basic3dGroupTest(LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_GFXP, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA, LW_TRUE, LW_FALSE));

    CHECK_RC(Basic3dGroupTest(LW_TRUE, 0xf, LW_FALSE, 0xffffffff, LW_FALSE, LW_FALSE));
    CHECK_RC(Basic3dGroupTest(LW_FALSE, 0xffffffff, LW_TRUE, 0xff, LW_FALSE, LW_FALSE));

    CHECK_RC(Basic3dGroupTest(LW_FALSE, 0, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI, LW_TRUE, LW_TRUE));
    CHECK_RC(Basic3dGroupTest(LW_FALSE, 0, LW_TRUE, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA, LW_TRUE, LW_TRUE));

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
RC PreemptionTest::Cleanup()
{
    return OK;
}

/**
 * @brief Tests setting the requested GfxP/CIP mode on a new channel, checks against pass/fail as expected
 *
 * @return
 */
RC PreemptionTest::BasicGfxpTest(LwBool bSetGfx, LwU32 gfxMode, LwBool bSetComp, LwU32 compMode, LwBool bShouldPass)
{
    RC rc;
    LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_PARAMS grPreemptionModeParams = {0};
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LWC06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams;

    Printf(Tee::PriHigh, "%s: setGfx=%d gfxMode=%d setComp=%d compMode=%d shouldPass=%d\n", __FUNCTION__,
           bSetGfx, gfxMode, bSetComp, compMode, bShouldPass);

    m_TestConfig.SetAllowMultipleChannels(true);

    m_pCh[0] = NULL;
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[0], &m_hCh[0], LW2080_ENGINE_TYPE_GR(0)));

    // setup and make control call to trigger SMPC WAR
    grPreemptionModeParams.hChannel = m_hCh[0];

    if (bSetGfx)
    {
        grPreemptionModeParams.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _GFXP, _SET, grPreemptionModeParams.flags);
        grPreemptionModeParams.gfxpPreemptMode = gfxMode;
    }

    if(bSetComp)
    {
        grPreemptionModeParams.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _CILP, _SET, grPreemptionModeParams.flags);
        grPreemptionModeParams.cilpPreemptMode = compMode;
    }

    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_GR_SET_CTXSW_PREEMPTION_MODE,
                        (void*)&grPreemptionModeParams,
                        sizeof(grPreemptionModeParams));

    if (OK != rc)
    {
        if (bShouldPass)
        {
            Printf(Tee::PriHigh, "%s: failed to set preemption mode gr=%d comp=%d NOT expected\n", __FUNCTION__, gfxMode, compMode);
        }
        else
        {
            Printf(Tee::PriHigh, "%s: failed to set preemption mode gr=%d comp=%d AS expected\n", __FUNCTION__, gfxMode, compMode);
            rc.Clear();
        }

        goto Cleanup;
    }
    else if ((OK == rc) && !bShouldPass)
    {
        Printf(Tee::PriHigh, "%s: set preemption mode gr=%d comp=%d but should have failed\n",
            __FUNCTION__, gfxMode, compMode);
        rc = RC::LWRM_ERROR;
        goto Cleanup;
    }

    CHECK_RC(m_3dAlloc[0].Alloc(m_hCh[0], GetBoundGpuDevice()));

    CHECK_RC((m_pCh[0])->SetObject(LWA06F_SUBCHANNEL_3D, m_3dAlloc[0].GetHandle()));

    gpFifoSchedulParams.bEnable = true;

    // Schedule channel so it is properly evicted when channel is freed
    CHECK_RC(pLwRm->Control(m_pCh[0]->GetHandle(), LWC06F_CTRL_CMD_GPFIFO_SCHEDULE,
             &gpFifoSchedulParams, sizeof(gpFifoSchedulParams)));

    if (m_bUnbind)
    {
        grPreemptionModeParams.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _GFXP, _SET, grPreemptionModeParams.flags);
        grPreemptionModeParams.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _CILP, _SET, grPreemptionModeParams.flags);
        grPreemptionModeParams.gfxpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_GFX_WFI;
        grPreemptionModeParams.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI;

        rc = pLwRm->Control(hSubdevice,
                            LW2080_CTRL_CMD_GR_SET_CTXSW_PREEMPTION_MODE,
                            (void*)&grPreemptionModeParams,
                            sizeof(grPreemptionModeParams));
    }

Cleanup:

    m_3dAlloc[0].Free();

    m_TestConfig.FreeChannel(m_pCh[0]);

    return rc;
}

/**
 * @brief Tests setting the requested CIP mode on a new channel, checks against pass/fail as expected
 *
 * @return
 */
RC PreemptionTest::BasicComputeCilpTest()
{
    RC rc;
    LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_PARAMS grPreemptionModeParams = {0};
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LWC06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams;

    Printf(Tee::PriHigh, "%s: setting compMode=%d \n", __FUNCTION__,
           LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CILP);

    m_TestConfig.SetAllowMultipleChannels(true);

    m_pCh[0] = NULL;
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[0], &m_hCh[0], LW2080_ENGINE_TYPE_GR(0)));

    // setup and make control call to trigger SMPC WAR
    grPreemptionModeParams.hChannel = m_hCh[0];

    grPreemptionModeParams.flags = 
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _CILP, _SET, grPreemptionModeParams.flags);
    grPreemptionModeParams.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CILP;

    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_GR_SET_CTXSW_PREEMPTION_MODE,
                        (void*)&grPreemptionModeParams,
                        sizeof(grPreemptionModeParams));

    if (OK != rc)
    {
        Printf(Tee::PriHigh, "%s: failed to set preemption mode comp=%d NOT expected\n",
                    __FUNCTION__, LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CILP);
        goto Cleanup;
    }

    CHECK_RC(m_computeAlloc[0].Alloc(m_hCh[0], GetBoundGpuDevice()));
    CHECK_RC((m_pCh[0])->SetObject(LWA06F_SUBCHANNEL_COMPUTE, m_computeAlloc[0].GetHandle()));

    gpFifoSchedulParams.bEnable = true;

    // Schedule channel so it is properly evicted when channel is freed
    CHECK_RC(pLwRm->Control(m_pCh[0]->GetHandle(), LWC06F_CTRL_CMD_GPFIFO_SCHEDULE,
             &gpFifoSchedulParams, sizeof(gpFifoSchedulParams)));

Cleanup:

    m_computeAlloc[0].Free();

    m_TestConfig.FreeChannel(m_pCh[0]);

    return rc;
}

#define GROUP_SIZE 2
#define MY_3D_CH 1
#define MY_COMPUTE_CH 0

/**
 * @brief Tests setting the requested GfxP/CIP mode on a new TSG, checks against pass/fail as expected
 *
 * @return
 */
RC PreemptionTest::Basic3dGroupTest(LwBool bSetGfx, LwU32 gfxMode, LwBool bSetComp, LwU32 compMode,
                                    LwBool bShouldPass, LwBool bEnableScg)
{
    RC rc;
    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};
    chGrpParams.engineType = LW2080_ENGINE_TYPE_GR(0);

    ChannelGroup chGrp(&m_TestConfig, &chGrpParams);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = GROUP_SIZE;
    LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_PARAMS grPreemptionModeParams = {0};
    LwRmPtr pLwRm;
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(pSubdevice);
    Gpu::LwDeviceId chip = pSubdevice->DeviceId();

    Channel *pCh[GROUP_SIZE];
    LwRm::Handle hCh[GROUP_SIZE];
    LwRm::Handle hObj[GROUP_SIZE];
    LwU32 flags;

    Printf(Tee::PriHigh, "%s: setGfx=%d gfxMode=%d setComp=%d compMode=%d shouldPass=%d enScg=%d\n",
           __FUNCTION__, bSetGfx, gfxMode, bSetComp, compMode, bShouldPass, bEnableScg);

    m_semaSurf.SetForceSizeAlloc(true);
    m_semaSurf.SetArrayPitch(1);
    m_semaSurf.SetArraySize(0x1000);
    m_semaSurf.SetColorFormat(ColorUtils::VOID32);
    m_semaSurf.SetAddressModel(Memory::Paging);
    m_semaSurf.SetLayout(Surface2D::Pitch);
    m_semaSurf.SetLocation(Memory::Fb);
    CHECK_RC(m_semaSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_semaSurf.Map());

    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);
    MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0xdeadbeef);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    CHECK_RC(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        flags = 0;

        if ((i == MY_3D_CH) && bEnableScg)
        {
            flags = FLD_SET_DRF_NUM(OS04, _FLAGS,_GROUP_CHANNEL_THREAD,   0, flags);
            flags = FLD_SET_DRF_NUM(OS04, _FLAGS,_GROUP_CHANNEL_RUNQUEUE, 0, flags);
        }
        else if (bEnableScg)
        {
            flags = FLD_SET_DRF_NUM(OS04, _FLAGS,_GROUP_CHANNEL_THREAD,   1, flags);
            flags = FLD_SET_DRF_NUM(OS04, _FLAGS,_GROUP_CHANNEL_RUNQUEUE, 1, flags);
        }

        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh[i], flags));
        hCh[i] = pCh[i]->GetHandle();
    }

    CHECK_RC_CLEANUP(chGrp.Schedule());

    // setup and make control call to trigger SMPC WAR
    grPreemptionModeParams.hChannel = chGrp.GetHandle();

    if (bSetGfx)
    {
        grPreemptionModeParams.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _GFXP, _SET, grPreemptionModeParams.flags);
        grPreemptionModeParams.gfxpPreemptMode = gfxMode;
    }

    if(bSetComp)
    {
        grPreemptionModeParams.flags =
            FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _CILP, _SET, grPreemptionModeParams.flags);
        grPreemptionModeParams.cilpPreemptMode = compMode;
    }

    rc = pLwRm->Control(hSubdevice,
                        LW2080_CTRL_CMD_GR_SET_CTXSW_PREEMPTION_MODE,
                        (void*)&grPreemptionModeParams,
                        sizeof(grPreemptionModeParams));

    if (OK != rc)
    {
        if (bShouldPass)
        {
            Printf(Tee::PriHigh, "%s: failed to set preemption mode gr=%d comp=%d NOT expected\n", __FUNCTION__, gfxMode, compMode);
        }
        else
        {
            Printf(Tee::PriHigh, "%s: failed to set preemption mode gr=%d comp=%d AS expected\n", __FUNCTION__, gfxMode, compMode);
            rc.Clear();
        }

        goto Cleanup;
    }
    else if ((OK == rc) && !bShouldPass)
    {
        Printf(Tee::PriHigh, "%s: set preemption mode gr=%d comp=%d but should have failed\n",
            __FUNCTION__, gfxMode, compMode);
        rc = RC::LWRM_ERROR;

        goto Cleanup;
    }

    for (LwU32 i = 0; i < grpSz; i++)
    {
        if(i == MY_3D_CH)
        {
            CHECK_RC_CLEANUP(AllocObject(pCh[i], LWA06F_SUBCHANNEL_3D, &hObj[i]));
            CHECK_RC_CLEANUP((pCh[i])->SetObject(LWA06F_SUBCHANNEL_3D, hObj[i]));
        }
        else
        {
            CHECK_RC_CLEANUP(AllocObject(pCh[i], LWA06F_SUBCHANNEL_COMPUTE, &hObj[i]));
            CHECK_RC_CLEANUP((pCh[i])->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObj[i]));
        }
    }

    //
    // On GP10X with SCG, we must ensure that the methods are written to the intended channel within the group
    // rather than just round robin among all channels
    //
    // This change is only applicable for GP10X however as it is the only family with this SCG setup
    //
    if ((IsGP10XorBetter(chip)
#if LWCFG(GLOBAL_CHIP_T194)
                || (chip == Gpu::T194)
#endif
        ) && bEnableScg)
    {
        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, hCh[MY_3D_CH], LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, hCh[MY_3D_CH], LWA06F_SUBCHANNEL_3D, 0));

        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, hCh[MY_COMPUTE_CH], LWA06F_SUBCHANNEL_COMPUTE, m_semaSurf.GetCtxDmaOffsetGpu() + 0x10));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, hCh[MY_COMPUTE_CH], LWA06F_SUBCHANNEL_COMPUTE, 0));
    }
    else
    {
        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, 0x0, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, 0x0, LWA06F_SUBCHANNEL_3D, 0));

        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, 0x0, LWA06F_SUBCHANNEL_COMPUTE, m_semaSurf.GetCtxDmaOffsetGpu() + 0x10));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, 0x0, LWA06F_SUBCHANNEL_COMPUTE, 0));
    }

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if ((MEM_RD32(m_semaSurf.GetAddress()) != 0) ||
        (MEM_RD32((LwU8*) m_semaSurf.GetAddress()+ 0x10) != 0))
    {
        Printf(Tee::PriHigh, "%s: data1 0x%08x data2 0x%08x\n",
               __FUNCTION__, MEM_RD32(m_semaSurf.GetAddress()), MEM_RD32((LwU8*) m_semaSurf.GetAddress()+ 0x10));

        rc = RC::DATA_MISMATCH;
    }

Cleanup:

    m_semaSurf.Free();

    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

void PreemptionTest::FreeObjects(Channel *pCh)
{
    LwRm::Handle hCh = pCh->GetHandle();

    m_3dAllocs[hCh].Free();
    m_computeAllocs[hCh].Free();
}

RC PreemptionTest::AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj)
{
    RC rc;
    LwRm::Handle hCh = pCh->GetHandle();
    MASSERT(hObj);

    switch (subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(m_3dAllocs[hCh].Alloc(hCh, GetBoundGpuDevice()));
            *hObj = m_3dAllocs[hCh].GetHandle();
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(m_computeAllocs[hCh].Alloc(hCh, GetBoundGpuDevice()));
            *hObj = m_computeAllocs[hCh].GetHandle();
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }
    return rc;
}

RC PreemptionTest::WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset)
{
    RC rc;
    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;
}

RC PreemptionTest::WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken)
{
    RC rc;

    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_3D, LWC097_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(C097, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(C097, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(hCh, LWA06F_SUBCHANNEL_COMPUTE, LWC0C0_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(C0C0, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(C0C0, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ PreemptionTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PreemptionTest, RmTest,
                 "Preemption RM test - Test Preemption mode setup");
