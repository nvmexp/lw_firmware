/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_changrp.cpp
//! \brief To verify basic functionality of SCG group channel tagging
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string>          // Only use <> for built-in C++ stuff
#include <vector>
#include <algorithm>
#include <map>
#include "core/utility/errloggr.h"
#include "lwos.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/notifier.h"
#include "lwRmApi.h"
#include "core/include/utility.h"
#include "random.h"
#include "gpu/include/gralloc.h"
#include "core/include/tasker.h"
#include "gpu/include/notifier.h"
#include "gpu/tests/rm/utility/changrp.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"

#include "ctrl/ctrla06f.h"
#include "ctrl/ctrla06c.h"
#include "ctrl/ctrl2080.h"

#include "class/cla06c.h"
#include "class/cla06f.h"
#include "class/cla16f.h"
#include "class/cla197.h"
#include "class/cla1c0.h"
#include "class/cla0b5.h"
#include "class/cla06fsubch.h"
#include "class/cl9067.h"
#include "class/cl007d.h"
#include "class/clb197.h" // MAXWELL_B
#include "class/clc097.h" // PASCAL_A
#include "class/clc197.h" // PASCAL_B
#include "class/clc397.h" // VOLTA_A
#include "class/clc597.h" // TURING_A
#include "class/clc697.h" // AMPERE_A
#include "class/clc797.h" // AMPERE_B
#include "class/clc997.h" // ADA_A
#include "class/clcb97.h" // HOPPER_A
#include "class/clcc97.h" // HOPPER_B
#include "class/clcd97.h" // BLACKWELL_A

// Must be last
#include "core/include/memcheck.h"

class ChannelSCGTest : public RmTest
{
    public:
    ChannelSCGTest();
    virtual ~ChannelSCGTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    private:
    RC BasicCombinedTest();

    RC AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj);
    RC AllocEvent(Channel *pCh, LwRm::Handle hObj, ModsEvent **pNotifyEvent);

    RC WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken = false);
    RC WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset);

    void FreeObjects(Channel *pCh);

    bool bRunOnError;
    Surface2D m_semaSurf;

    class EventData
    {
        public:
            EventData() :
                pNotifyEvent(nullptr), hNotifyEvent(0), bAlloced(false) {}
            ModsEvent* pNotifyEvent;
            LwRm::Handle hNotifyEvent;
            bool bAlloced;
    };

    map<LwRm::Handle, ThreeDAlloc> m_3dAllocs;
    map<LwRm::Handle, ComputeAlloc> m_computeAllocs;
    bool m_bVirtCtx;
};

//! \brief ChannelSCGTest constructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
ChannelSCGTest::ChannelSCGTest() :
    bRunOnError(false),
    m_bVirtCtx(false)
{
    SetName("ChannelSCGTest");
}

//! \brief ChannelSCGTest destructor
//!
//! Placeholder : doesnt do much, most functionality in Setup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ChannelSCGTest::~ChannelSCGTest()
{

}

//! \brief IsSupported(), Looks for whether test can execute in current elw.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ChannelSCGTest::IsTestSupported()
{
    LwRmPtr    pLwRm;
    return (pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()) &&
           (pLwRm->IsClassSupported(MAXWELL_B,   GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(PASCAL_A,    GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(PASCAL_B,    GetBoundGpuDevice())  ||
            /* Keep running SCG tests on VOLTA until it is removed from HW */
            pLwRm->IsClassSupported(VOLTA_A,     GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(TURING_A,    GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(AMPERE_A,    GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(AMPERE_B,    GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(ADA_A,       GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(HOPPER_A,    GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(HOPPER_B,    GetBoundGpuDevice())  ||
            pLwRm->IsClassSupported(BLACKWELL_A, GetBoundGpuDevice()))) ? RUN_RMTEST_TRUE : "SCG Channel groups not supported";
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocating the channel and software object for use in the test
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC ChannelSCGTest::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());
    m_TestConfig.SetAllowMultipleChannels(true);

    m_semaSurf.SetForceSizeAlloc(true);
    m_semaSurf.SetArrayPitch(1);
    m_semaSurf.SetArraySize(0x1000);
    m_semaSurf.SetColorFormat(ColorUtils::VOID32);
    m_semaSurf.SetAddressModel(Memory::Paging);
    m_semaSurf.SetLayout(Surface2D::Pitch);
    m_semaSurf.SetLocation(Memory::Fb);
    CHECK_RC(m_semaSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_semaSurf.Map());

    return OK;
}

//! \brief  Cleanup():
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC ChannelSCGTest::Cleanup()
{
    m_semaSurf.Free();
    return OK;
}

//! \brief  Run():
//!
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC ChannelSCGTest::Run()
{
    RC rc = OK;
    RC errorRc = OK;

    // Do this subset of tests w/both normal and virtual contexts
    m_bVirtCtx = false;
    do
    {
        // Basic functionality, allocate and send methods ensuring exelwtion order.
        Printf(Tee::PriLow, "%s: BasicCombinedTest running \n", __FUNCTION__);
        rc = BasicCombinedTest();
        if (bRunOnError && rc != OK)
        {
            if (errorRc == OK)
                errorRc = rc;
            Printf(Tee::PriHigh, "%s: BasicCombinedTest failed with status: %s.\n", __FUNCTION__, rc.Message());
            rc.Clear();
        }
        else if (rc == OK)
            Printf(Tee::PriLow, "%s: BasicCombinedTest passed\n", __FUNCTION__);
        else
            CHECK_RC(rc);
        m_bVirtCtx = !m_bVirtCtx;
    } while (m_bVirtCtx);

    CHECK_RC(errorRc);
    return rc;
}

/**
 * @brief Tests that compute, graphics, can coexist in the same TSG.
 *
 * @return
 */
RC ChannelSCGTest::BasicCombinedTest()
{
    RC rc;
    LwU32 flags;
    LW_CHANNEL_GROUP_ALLOCATION_PARAMETERS chGrpParams = {0};
    chGrpParams.engineType = LW2080_ENGINE_TYPE_GR(0);
    
    ChannelGroup chGrp(&m_TestConfig, &chGrpParams);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = 2;
    Channel *pChCompute, *pCh3D;
    LwRm::Handle hObjCompute, hObj3D;
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    Gpu::LwDeviceId chip = pSubdevice->DeviceId();

    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);
    MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0xdeadbeef);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    CHECK_RC_CLEANUP(chGrp.Alloc());

    // Allocate COMPUTE in ASYNC GROUP THREAD 1
    flags = 0;
    flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_THREAD,   1, flags);
    flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, 1, flags);
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pChCompute, flags));
    CHECK_RC_CLEANUP(AllocObject(pChCompute, LWA06F_SUBCHANNEL_COMPUTE, &hObjCompute));
    CHECK_RC_CLEANUP(pChCompute->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObjCompute));

    // Allocate GRAPHICS in SYNC GROUP THREAD 0
    flags = 0;
    flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_THREAD,   0, flags);
    flags = FLD_SET_DRF_NUM(OS04, _FLAGS, _GROUP_CHANNEL_RUNQUEUE, 0, flags);
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh3D, flags));
    CHECK_RC_CLEANUP(AllocObject(pCh3D, LWA06F_SUBCHANNEL_3D, &hObj3D));
    CHECK_RC_CLEANUP(pCh3D->SetObject(LWA06F_SUBCHANNEL_3D, hObj3D));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    //
    // On GP10X with SCG, we must ensure that the methods are written to the intended channel within the group
    // rather than just round robin among all channels
    //
    // This change is only applicable for GP10X however as it is the only family with this SCG setup
    //
    if (IsGP10XorBetter(chip)
#if LWCFG(GLOBAL_CHIP_T194)
            || (chip == Gpu::T194)
#endif
       )
    {
        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, pChCompute->GetHandle(), LWA06F_SUBCHANNEL_COMPUTE, m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, pChCompute->GetHandle(), LWA06F_SUBCHANNEL_COMPUTE, 0));

        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, pCh3D->GetHandle(), LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu() + 0x10));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, pCh3D->GetHandle(), LWA06F_SUBCHANNEL_3D, 0));
    }
    else
    {
        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, 0x0, LWA06F_SUBCHANNEL_COMPUTE, m_semaSurf.GetCtxDmaOffsetGpu()));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, 0x0, LWA06F_SUBCHANNEL_COMPUTE, 0));

        CHECK_RC_CLEANUP(WriteBackendOffset(&stream, 0x0, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu() + 0x10));
        CHECK_RC_CLEANUP(WriteBackendRelease(&stream, 0x0, LWA06F_SUBCHANNEL_3D, 0));
    }

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if ((MEM_RD32(m_semaSurf.GetAddress()) != 0) ||
        (MEM_RD32((LwU8*) m_semaSurf.GetAddress()+ 0x10) != 0))
        rc = RC::DATA_MISMATCH;

Cleanup:
    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

RC ChannelSCGTest::AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj)
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

void ChannelSCGTest::FreeObjects(Channel *pCh)
{
    LwRm::Handle hCh = pCh->GetHandle();

    m_3dAllocs[hCh].Free();
    m_computeAllocs[hCh].Free();
}

RC  ChannelSCGTest::WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU64 offset)
{
    RC rc;
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

RC  ChannelSCGTest::WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwRm::Handle hCh, LwU32 subch, LwU32 data, bool bAwaken)
{
    RC rc;

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

JS_CLASS_INHERIT(ChannelSCGTest, RmTest,
    "ChannelSCGTest RMTEST that tests SCG taggingfunctionality");

