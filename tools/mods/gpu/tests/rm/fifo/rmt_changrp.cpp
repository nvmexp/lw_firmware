/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2014,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_changrp.cpp
//! \brief To verify basic functionality of KEPLER_CHANNEL_GROUP_A object
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
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

// Must be last
#include "core/include/memcheck.h"

class ChannelGroupTest : public RmTest
{
    public:
    ChannelGroupTest();
    virtual ~ChannelGroupTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(bRunMaxGroups, bool);
    SETGET_PROP(ForceTest, string);

    private:
    RC MaxGroupsTest();
    RC Basic3dGroupTest();
    RC BasicComputeGroupTest();
    RC BasicCombinedTest();
    RC BasicCopyTest();
    RC MaxChannelsTest();
    RC IlwalidBindScheduleTest();
    RC BasicNotifierTest();
    RC BasicSemaphoreTest();
    RC Basic3dAndComputeNotifierTest();
    RC FreeWhileRunningTest();
    RC SwChannelTest();
    RC ModifyTimesliceTest();
    RC PreemptAPITest();
    RC SomeHostOnlyTest();
    RC HostOnlyTest();
    RC SubcontextScheduleTest();

    LwU32 GetGrCopyInstance(LwU32);

    RC AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj, LwU32 engineInstance = (LwU32) -1);
    RC AllocEvent(Channel *pCh, LwRm::Handle hObj, ModsEvent **pNotifyEvent);

    RC WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU32 data, bool bAwaken = false);
    RC WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU64 offset);

    void FreeObjects(Channel *pCh);

    RC CleanObjects();

    bool bRunOnError;
    Surface2D m_semaSurf;
    Surface2D m_semaSurf2;
    bool m_bRunMaxGroups;
    bool m_bSkipMaxGroupsTest;
    string m_ForceTest;

    class EventData
    {
        public:
            EventData() : bAlloced(false) {};
            ModsEvent* pNotifyEvent;
            LwRm::Handle hNotifyEvent;
            bool bAlloced;
    };

    map<LwRm::Handle, ThreeDAlloc> m_3dAllocs;
    map<LwRm::Handle, DmaCopyAlloc> m_ceAllocs;
    map<LwRm::Handle, ComputeAlloc> m_computeAllocs;
    map<LwRm::Handle, EventData> m_objectEvents;
    bool m_bVirtCtx;
    bool m_bLateChannelAlloc;
};

//! \brief ChannelGroupTest constructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
ChannelGroupTest::ChannelGroupTest() :
    bRunOnError(false),
    m_bRunMaxGroups(false),
    m_bSkipMaxGroupsTest(false),
    m_bVirtCtx(false),
    m_bLateChannelAlloc(false)
{
    SetName("ChannelGroupTest");
}

//! \brief ChannelGroupTest destructor
//!
//! Placeholder : doesnt do much, most functionality in Setup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
ChannelGroupTest::~ChannelGroupTest()
{

}

//! \brief IsSupported(), Looks for whether test can execute in current elw.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string ChannelGroupTest::IsTestSupported()
{
    LwRmPtr    pLwRm;
    return pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()) ? RUN_RMTEST_TRUE : "Channel groups not supported";
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocating the channel and software object for use in the test
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC ChannelGroupTest::Setup()
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

    m_semaSurf2.SetForceSizeAlloc(true);
    m_semaSurf2.SetArrayPitch(1);
    m_semaSurf2.SetArraySize(0x1000);
    m_semaSurf2.SetColorFormat(ColorUtils::VOID32);
    m_semaSurf2.SetAddressModel(Memory::Paging);
    m_semaSurf2.SetLayout(Surface2D::Pitch);
    m_semaSurf2.SetLocation(Memory::Fb);
    CHECK_RC(m_semaSurf2.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_semaSurf2.Map());

    // Skip all MaxGroupsTest related testing on non-Si DVS runs until we can solve Bug 913329
    m_bSkipMaxGroupsTest = IsDVS() && (Platform::GetSimulationMode() != Platform::Hardware);
    return OK;
}

//! \brief  Cleanup():
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC ChannelGroupTest::Cleanup()
{
    m_semaSurf2.Free();
    m_semaSurf.Free();
    return OK;
}

//! \brief  Run():
//!
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC ChannelGroupTest::Run()
{
    RC rc = OK;
    RC errorRc = OK;

#define RUN_TEST(name)\
    if (!m_ForceTest.length() || m_ForceTest.compare(#name) == 0)\
    {\
        Printf(Tee::PriLow, "%s: %s running \n", __FUNCTION__, #name);\
        rc = name(); \
        if (bRunOnError && rc != OK)\
        {\
            if (errorRc == OK)\
                errorRc = rc;\
            Printf(Tee::PriHigh, "%s: %s failed with status: %s.\n", __FUNCTION__, #name, rc.Message());\
            rc.Clear();\
        }\
        else if (rc == OK)\
            Printf(Tee::PriLow, "%s: %s passed\n", __FUNCTION__, #name);\
        else \
            CHECK_RC(rc);\
        if(CleanObjects() != OK)\
            Printf(Tee::PriHigh, "%s: CleanObjects had to clean state after %s!\n", __FUNCTION__, #name);\
    }\
    else\
    {\
        Printf(Tee::PriLow, "%s: Not running %s\n", __FUNCTION__, #name);\
    }

    if (m_ForceTest.length())
        Printf(Tee::PriHigh, "Running only %s\n", m_ForceTest.c_str());

    // Do this subset of tests w/both normal and virtual contexts
    m_bVirtCtx = false;
    do
    {
        // Basic functionality, allocate and send methods ensuring exelwtion order.
        RUN_TEST(Basic3dGroupTest);
        RUN_TEST(BasicComputeGroupTest);
        RUN_TEST(BasicCopyTest);

        m_bLateChannelAlloc = false;
        RUN_TEST(BasicCombinedTest);

        m_bLateChannelAlloc = true;
        RUN_TEST(BasicCombinedTest);

        // Test notifiers
        RUN_TEST(BasicNotifierTest);
        RUN_TEST(Basic3dAndComputeNotifierTest);

        // Test semaphores
        RUN_TEST(BasicSemaphoreTest);

        RUN_TEST(FreeWhileRunningTest);

        // SW methods on channel group
        RUN_TEST(SwChannelTest);

        RUN_TEST(SubcontextScheduleTest);

        m_bVirtCtx = !m_bVirtCtx;
    } while (m_bVirtCtx);

    RUN_TEST(ModifyTimesliceTest);

    RUN_TEST(PreemptAPITest);

    if(!m_bSkipMaxGroupsTest)
    {
        // Max groups in system
        RUN_TEST(MaxGroupsTest);
    }

    // Max channels in group
    RUN_TEST(MaxChannelsTest);

    // TODO BIND allows calls on channels in TSGs
    // Negative testing of _BIND channels in group
    // Negative testing of _SCHEDULE channels in group
    //RUN_TEST(IlwalidBindScheduleTest);

    RUN_TEST(SomeHostOnlyTest);
    RUN_TEST(HostOnlyTest);

    CHECK_RC(errorRc);
    return rc;
}

static bool
pollForZero(void* pArgs)
{
    return MEM_RD32(pArgs) == 0;
}

/**
 * @brief Tests multiple graphics channels in a TSG are properly sharing a context.
 *
 * @return
 */
RC ChannelGroupTest::Basic3dGroupTest()
{
    RC rc;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = 2;

    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        Channel *pCh;
        LwRm::Handle hObj;
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
    }
    CHECK_RC_CLEANUP(chGrp.Schedule());

    //
    // Temporary change to WAR bug 1761880, we need to make sure the TSG is not
    // on a PBDMA when we promote a ctx
    //
    if (m_bVirtCtx)
    {
        //
        // We need to either disable TSG or wait for idle before preempting.
        // Otherwise scheduler may pick it again right after we preempted,
        // or it may get parked on PBDMA (preemption will fail in that case).
        //
        CHECK_RC_CLEANUP(chGrp.Flush());
        CHECK_RC_CLEANUP(chGrp.WaitIdle());
        CHECK_RC_CLEANUP(chGrp.Preempt());
    }

    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_3D, 0));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
        rc = RC::DATA_MISMATCH;

Cleanup:
    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

/**
 * @brief Tests multiple compute channels in a TSG are properly sharing a context.
 *
 * @return
 */
RC ChannelGroupTest::BasicComputeGroupTest()
{
    RC rc;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = 2;

    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        Channel *pCh;
        LwRm::Handle hObj;
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COMPUTE, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObj));
    }
    CHECK_RC_CLEANUP(chGrp.Schedule());

    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_COMPUTE, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_COMPUTE, 0));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
        rc = RC::DATA_MISMATCH;

Cleanup:
    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

/**
 * @brief Tests multiple copy channels in a TSG.
 *
 * @return
 */
RC ChannelGroupTest::BasicCopyTest()
{
    RC rc;
    ChannelGroup chGrp(&m_TestConfig);
    const LwU32 grpSz = 2;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_COPY(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        Channel *pCh;
        LwRm::Handle hObj;
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COPY_ENGINE, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, hObj));
        MEM_WR32((LwU8*)m_semaSurf.GetAddress() + i*0x10, 0xdeadbeef);
    }

    CHECK_RC_CLEANUP(chGrp.Schedule());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        Channel *pCh = chGrp.GetChannel(i);
        ChannelGroup::SplitMethodStream chStream(pCh);
        CHECK_RC_CLEANUP(WriteBackendOffset(&chStream, LWA06F_SUBCHANNEL_COPY_ENGINE, m_semaSurf.GetCtxDmaOffsetGpu() + 0x10*i));
        CHECK_RC_CLEANUP(WriteBackendRelease(&chStream, LWA06F_SUBCHANNEL_COPY_ENGINE, 0));
    }

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        if (MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10*i) != 0)
        {
            rc = RC::DATA_MISMATCH;
            break;
        }
    }

Cleanup:
    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();
    return rc;
}

/**
 * @brief Tests that compute, graphics, and CE can coexist in the same TSG.
 *
 *        If m_bLateChannelAlloc is set then also test whether channels
 *        allocated after the TSG has been scheduled are themselves scheduled.
 *
 * @return
 */
RC ChannelGroupTest::BasicCombinedTest()
{
    RC rc;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = 2;

    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);
    MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0xdeadbeef);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        Channel *pCh;
        LwRm::Handle hObj;
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COMPUTE, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObj));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COPY_ENGINE, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, hObj));

        MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x100 + i*0x10, 0xdeadbeef);

        // Channels allocated to the TSG after the TSG is scheduled should also be scheduled
        if (m_bLateChannelAlloc && (i == 0))
            CHECK_RC_CLEANUP(chGrp.Schedule());
    }

    if (!m_bLateChannelAlloc)
        CHECK_RC_CLEANUP(chGrp.Schedule());

    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_COMPUTE, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_COMPUTE, 0));

    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu() + 0x10));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_3D, 0));

    for (LwU32 i = 0; i < grpSz; i++)
    {
        Channel *pCh = chGrp.GetChannel(i);
        ChannelGroup::SplitMethodStream chStream(pCh);
        CHECK_RC_CLEANUP(WriteBackendOffset(&chStream, LWA06F_SUBCHANNEL_COPY_ENGINE, m_semaSurf.GetCtxDmaOffsetGpu() + 0x100 + 0x10*i));
        CHECK_RC_CLEANUP(WriteBackendRelease(&chStream, LWA06F_SUBCHANNEL_COPY_ENGINE, 0));
    }

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0 ||
            MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10) != 0)
        rc = RC::DATA_MISMATCH;

    for (LwU32 i = 0; i < grpSz; i++)
    {
        if (MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x100 + 0x10*i) != 0)
        {
            rc.Clear();
            rc = RC::DATA_MISMATCH;
            break;
        }
    }

Cleanup:
    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

/**
 * @brief Tests maximum number of TSGs can be allocated
 *
 * @return
 */
RC ChannelGroupTest::MaxGroupsTest()
{
    LwRmPtr pLwRm;
    RC rc;
    LwU32 channelSize = m_TestConfig.ChannelSize();
    vector<ChannelGroup*> chanGroups;
    Surface2D *pSemaSurf = NULL;
    LwU32 i = 0;
    LwRm::Handle hNotifyEvent = 0;
    ModsEvent *pNotifyEvent=NULL;
    void *pEventAddr=NULL;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams = {0};
    LW2080_CTRL_FIFO_GET_INFO_PARAMS fifoInfoParams = {0};
    LwU32 maxGroups;
    LwU32 isPerRunlistChannelRamSupported;

    fifoInfoParams.fifoInfoTbl[0].index = LW2080_CTRL_FIFO_INFO_INDEX_IS_PER_RUNLIST_CHANNEL_RAM_SUPPORTED;
    fifoInfoParams.fifoInfoTblSize = 1;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_FIFO_GET_INFO,
                (void*)&fifoInfoParams, sizeof(fifoInfoParams)));

    isPerRunlistChannelRamSupported = fifoInfoParams.fifoInfoTbl[0].data;

    if (!isPerRunlistChannelRamSupported)
    {
        fifoInfoParams.fifoInfoTbl[0].index = LW2080_CTRL_FIFO_INFO_INDEX_MAX_CHANNEL_GROUPS;
        fifoInfoParams.fifoInfoTbl[1].index = LW2080_CTRL_FIFO_INFO_INDEX_CHANNEL_GROUPS_IN_USE;
    }
    else
    {
        fifoInfoParams.fifoInfoTbl[0].index = LW2080_CTRL_FIFO_INFO_INDEX_MAX_CHANNEL_GROUPS_PER_ENGINE;
        fifoInfoParams.fifoInfoTbl[1].index = LW2080_CTRL_FIFO_INFO_INDEX_CHANNEL_GROUPS_IN_USE_PER_ENGINE;
        fifoInfoParams.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
    }
    fifoInfoParams.fifoInfoTblSize = 2;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_FIFO_GET_INFO,
                (void*)&fifoInfoParams, sizeof(fifoInfoParams)));

    maxGroups = fifoInfoParams.fifoInfoTbl[0].data - fifoInfoParams.fifoInfoTbl[1].data;

    pNotifyEvent = Tasker::AllocEvent();
    pEventAddr = Tasker::GetOsEvent(
            pNotifyEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    //Associate Event to Object
    CHECK_RC_CLEANUP(pLwRm->AllocEvent(
        pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
        &hNotifyEvent,
        LW01_EVENT_OS_EVENT,
        LW2080_NOTIFIERS_FIFO_EVENT_MTHD | LW01_EVENT_NONSTALL_INTR,
        pEventAddr));

    eventParams.event = LW2080_NOTIFIERS_FIFO_EVENT_MTHD;
    eventParams.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_SINGLE;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                &eventParams, sizeof(eventParams)));

    m_TestConfig.SetChannelSize(1024);

    while (i < maxGroups)
    {
        ChannelGroup *pNewGrp = new ChannelGroup(&m_TestConfig);

        pNewGrp->SetUseVirtualContext(m_bVirtCtx);
        DISABLE_BREAK_COND(nobp, true);
        pNewGrp->SetEngineId(LW2080_ENGINE_TYPE_GR(0));
        rc = pNewGrp->Alloc();
        DISABLE_BREAK_END(nobp);

        if (rc != OK)
            goto Cleanup;

        chanGroups.push_back(pNewGrp);

        if (chanGroups.size() && (chanGroups.size() % 100 == 0))
            Printf(Tee::PriLow, "%s: Allocated %d channel groups\n",
                    __FUNCTION__, (int) chanGroups.size());
        i++;
    }

    rc.Clear();

    Printf(Tee::PriLow, "%s: Successfully allocated %d channel groups\n",
            __FUNCTION__, (int) chanGroups.size());

    if (!m_bRunMaxGroups)
       goto Cleanup;

    for (i = 0; i < chanGroups.size(); ++i)
    {
        CHECK_RC_CLEANUP(chanGroups[i]->AllocChannel());
        if (i && (i % 100 == 0))
            Printf(Tee::PriLow, "%s: Allocated %d channels\n",
                    __FUNCTION__, i);
    }

    Printf(Tee::PriLow, "%s: Successfully allocated %d channels\n",
            __FUNCTION__, (int) chanGroups.size());

    // Now that we have all the channels, allocate semaphore memory for all of them.
    pSemaSurf = new Surface2D;
    pSemaSurf->SetForceSizeAlloc(true);
    pSemaSurf->SetArrayPitch(1);
    pSemaSurf->SetArraySize(4 * (LwU32) chanGroups.size());
    pSemaSurf->SetColorFormat(ColorUtils::VOID32);
    pSemaSurf->SetAddressModel(Memory::Paging);
    pSemaSurf->SetLayout(Surface2D::Pitch);
    pSemaSurf->SetLocation(Memory::Coherent);
    CHECK_RC_CLEANUP(pSemaSurf->Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(pSemaSurf->Map());
    pSemaSurf->Fill(0xdeadbeef);

    for (i = 0; i < chanGroups.size(); ++i)
    {
        Channel *pCh;

        CHECK_RC_CLEANUP(chanGroups[i]->Bind(LW2080_ENGINE_TYPE_GRAPHICS));
        CHECK_RC_CLEANUP(chanGroups[i]->Schedule());

        MASSERT(chanGroups[i]->GetNumChannels() == 1);
        pCh = chanGroups[i]->GetChannel(0);

        CHECK_RC_CLEANUP(pCh->SetSemaphoreOffset(pSemaSurf->GetCtxDmaOffsetGpu() + (i*4)));
        pCh->SetSemaphoreReleaseFlags(0);
        pCh->SetSemaphorePayloadSize(Channel::SEM_PAYLOAD_SIZE_32BIT);
        CHECK_RC_CLEANUP(pCh->SemaphoreRelease(0));
        CHECK_RC_CLEANUP(pCh->Flush());

        if (i && (i % 100 == 0))
            Printf(Tee::PriLow, "%s: Written %d semaphores\n",
                    __FUNCTION__, i);
    }

    // Wait for all semaphores to be read
    for (i = 0; i < chanGroups.size(); i++)
    {
        CHECK_RC_CLEANUP(POLLWRAP(pollForZero,
                                  (LwU8*)pSemaSurf->GetAddress() + (i*4),
                                  m_TestConfig.TimeoutMs()));
        if (i % 100 == 0)
            Printf(Tee::PriLow, "%s: Waiting on %d semaphores \n",
                    __FUNCTION__, (LwU32)(chanGroups.size()-i));
    }

    Printf(Tee::PriLow, "%s: All semaphores released.\n", __FUNCTION__);

Cleanup:
    if (pSemaSurf)
    {
        pSemaSurf->Free();
        delete pSemaSurf;
    }
    if(pNotifyEvent != NULL)
        Tasker::FreeEvent(pNotifyEvent);

    while (!chanGroups.empty())
    {
        ChannelGroup *pGrp = chanGroups.back();
        chanGroups.pop_back();
        pGrp->Free();
        delete pGrp;
    }

    m_TestConfig.SetChannelSize(channelSize);
    return rc;
}

/**
 * @brief Tests maximum number of channels in a TSG can be allocated
 *
 * @return
 */
RC ChannelGroupTest::MaxChannelsTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 i = 0;
    ChannelGroup chGrp(&m_TestConfig);
    Surface2D semaSurf;
    LW2080_CTRL_FIFO_GET_INFO_PARAMS fifoInfoParams = {0};
    LwU32 maxChannels;

    fifoInfoParams.fifoInfoTbl[0].index = LW2080_CTRL_FIFO_INFO_INDEX_MAX_CHANNELS_PER_GROUP;
    fifoInfoParams.fifoInfoTbl[1].index = LW2080_CTRL_FIFO_INFO_INDEX_CHANNEL_GROUPS_IN_USE;
    fifoInfoParams.fifoInfoTblSize = 2;

    CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_FIFO_GET_INFO,
                (void*)&fifoInfoParams, sizeof(fifoInfoParams)));

    maxChannels = fifoInfoParams.fifoInfoTbl[0].data - fifoInfoParams.fifoInfoTbl[1].data;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    DISABLE_BREAK_COND(nobp, true);
    while (i < maxChannels)
    {
        rc = chGrp.AllocChannel();
        if (rc != OK)
            break;

        i++;
    }    
    DISABLE_BREAK_END(nobp);

    rc.Clear();

    if (i < maxChannels)
        goto Cleanup;

    Printf(Tee::PriLow, "%s: Successfully allocated %d channels\n",
                __FUNCTION__, chGrp.GetNumChannels());

    semaSurf.SetForceSizeAlloc(true);
    semaSurf.SetArrayPitch(1);
    semaSurf.SetArraySize(0x10 * i);
    semaSurf.SetColorFormat(ColorUtils::VOID32);
    semaSurf.SetAddressModel(Memory::Paging);
    semaSurf.SetLayout(Surface2D::Pitch);
    semaSurf.SetLocation(Memory::Fb);
    CHECK_RC_CLEANUP(semaSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(semaSurf.Map());
    semaSurf.Fill(0xdeadbeef);

    CHECK_RC_CLEANUP(chGrp.Bind(LW2080_ENGINE_TYPE_GRAPHICS));
    CHECK_RC_CLEANUP(chGrp.Schedule());

    for (i = 0; i < chGrp.GetNumChannels(); i++)
    {
        Channel *pCh = chGrp.GetChannel(i);

        CHECK_RC_CLEANUP(pCh->SetSemaphoreOffset(semaSurf.GetCtxDmaOffsetGpu() + (i*0x10)));
        CHECK_RC_CLEANUP(pCh->SemaphoreAcquire(0xdeadbeef));
        CHECK_RC_CLEANUP(pCh->SemaphoreRelease(0));
    }

    CHECK_RC_CLEANUP(chGrp.Flush());

    for (i = 0; i < chGrp.GetNumChannels(); i++)
    {
        CHECK_RC_CLEANUP(POLLWRAP(pollForZero,
                                  (LwU8*)semaSurf.GetAddress() + (i*0x10),
                                  m_TestConfig.TimeoutMs()));
    }

Cleanup:
    semaSurf.Free();

    chGrp.Free();
    return rc;
}

/**
 * @brief Negative testing for SCHEUDLE and BIND control calls
 *
 * @return
 */
RC ChannelGroupTest::IlwalidBindScheduleTest()
{
    LwRmPtr pLwRm;
    RC rc;
    ChannelGroup chGrp(&m_TestConfig);
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams = {0};
    LwU32 i;
    LwU32 parentClass;

    CHECK_RC_CLEANUP(chGrp.AllocChannel());
    CHECK_RC_CLEANUP(chGrp.AllocChannel());
    CHECK_RC_CLEANUP(chGrp.AllocChannel());

    for (i = 0; i < chGrp.GetNumChannels(); i++)
    {
        LWA06F_CTRL_BIND_PARAMS bindParams = {0};
        LWA06C_CTRL_GPFIFO_SCHEDULE_PARAMS schedParams = {0};

        Channel *pCh = chGrp.GetChannel(i);

        schedParams.bEnable = LW_TRUE;
        rc = pLwRm->Control(pCh->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                    &schedParams, sizeof(schedParams));
        if (rc == OK)
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        rc.Clear();

        bindParams.engineType = LW2080_ENGINE_TYPE_GRAPHICS;

        rc = pLwRm->Control(pCh->GetHandle(), LWA06F_CTRL_CMD_BIND,
                    &bindParams, sizeof(bindParams));

        if (rc == OK)
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        rc.Clear();

        bindParams.engineType = LW2080_ENGINE_TYPE_COPY0;

        rc = pLwRm->Control(pCh->GetHandle(), LWA06F_CTRL_CMD_BIND,
                    &bindParams, sizeof(bindParams));

        if (rc == OK)
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        rc.Clear();
    }

    //
    // Get a list of supported engines
    //
    CHECK_RC_CLEANUP( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                               LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                               &engineParams,
                               sizeof (engineParams)) );

    MASSERT(chGrp.GetNumChannels() > 0);
    parentClass = chGrp.GetChannel(0)->GetClass();
    for (LwU32 i = 0; i < engineParams.engineCount; i++ )
    {
        if (engineParams.engineList[i] == LW2080_ENGINE_TYPE_GRAPHICS ||
                engineParams.engineList[i] == GetGrCopyInstance(parentClass))
            continue;

        rc.Clear();
        rc = chGrp.Bind(engineParams.engineList[i]);
        if (rc == OK)
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

Cleanup:
    chGrp.Free();

    return rc;

}

/**
 * @brief Tests that a notify event on one channel is sent to all channels in the TSG.
 *
 * @return
 */
RC ChannelGroupTest::BasicNotifierTest()
{
    RC rc;
    const LwU32 grpSz = 2;
    ChannelGroup chGrp(&m_TestConfig);
    ModsEvent *pNotifyEvent[grpSz+1];
    Channel *pCh = NULL;
    LwRm::Handle hCh;
    LwRm::Handle hObj;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
        CHECK_RC_CLEANUP(AllocEvent(pCh, hObj, &pNotifyEvent[i]));
    }

    // Allocate an extra channel to make sure we don't wake it up.
    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&pCh, &hCh, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
    CHECK_RC_CLEANUP(AllocEvent(pCh, hObj, &pNotifyEvent[grpSz]));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_NOTIFY_A, LwU64_HI32(m_semaSurf.GetCtxDmaOffsetGpu())));
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_NOTIFY_B, LwU64_LO32(m_semaSurf.GetCtxDmaOffsetGpu())));
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_NOTIFY, DRF_DEF(A197, _NOTIFY, _TYPE, _WRITE_THEN_AWAKEN)));
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        if (!Tasker::IsEventSet(pNotifyEvent[i]))
        {
            Printf(Tee::PriHigh, "%s: Notifier %d not set\n", __FUNCTION__, i);
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        }
    }

    if (Tasker::IsEventSet(pNotifyEvent[grpSz]))
    {
        Printf(Tee::PriHigh, "%s: Single Channel's notifier IS set\n", __FUNCTION__);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);

    }

Cleanup:

    for (LwU32 i = 0; i < grpSz; i++)
    {
        FreeObjects(chGrp.GetChannel(i));
        Tasker::FreeEvent(pNotifyEvent[i]);
    }

    chGrp.Free();

    FreeObjects(pCh);
    Tasker::FreeEvent(pNotifyEvent[grpSz]);
    m_TestConfig.FreeChannel(pCh);

    return rc;
}

/**
 * @brief Tests that a sempahore aweken event on one channel is sent to all channels in the TSG.
 *
 * @return
 */
RC ChannelGroupTest::BasicSemaphoreTest()
{
    RC rc;
    const LwU32 grpSz = 2;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    ModsEvent *pNotifyEvent[grpSz+1];
    Channel *pCh = NULL;
    LwRm::Handle hCh;
    LwRm::Handle hObj;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
        CHECK_RC_CLEANUP(AllocEvent(pCh, hObj, &pNotifyEvent[i]));
    }

    // Allocate an extra channel to make sure we don't wake it up.
    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&pCh, &hCh, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
    CHECK_RC_CLEANUP(AllocEvent(pCh, hObj, &pNotifyEvent[grpSz]));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_3D, 0, true));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    for (LwU32 i = 0; i < grpSz; i++)
    {
        if (!Tasker::IsEventSet(pNotifyEvent[i]))
        {
            Printf(Tee::PriHigh, "%s: Event %d not set\n", __FUNCTION__, i);
            CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
        }
    }

    if (Tasker::IsEventSet(pNotifyEvent[grpSz]))
    {
        Printf(Tee::PriHigh, "%s: Single Channel's semaphore Event IS set\n", __FUNCTION__);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);

    }

Cleanup:

    for (LwU32 i = 0; i < grpSz; i++)
    {
        FreeObjects(chGrp.GetChannel(i));
        Tasker::FreeEvent(pNotifyEvent[i]);
    }

    chGrp.Free();

    FreeObjects(pCh);
    Tasker::FreeEvent(pNotifyEvent[grpSz]);
    m_TestConfig.FreeChannel(pCh);

    return rc;
}

/**
 * @brief Checks that with graphics and compute objects allocated that only the right class of objects gets notification.
 *
 * @return
 */
RC ChannelGroupTest::Basic3dAndComputeNotifierTest()
{
    RC rc;
    const LwU32 grpSz = 2;
    ChannelGroup chGrp(&m_TestConfig);
    ModsEvent *pNotifyEvent[grpSz];
    Channel *pCh = NULL;
    LwRm::Handle hObj;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
    CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
    CHECK_RC_CLEANUP(AllocEvent(pCh, hObj, &pNotifyEvent[0]));

    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COMPUTE, &hObj));
    CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObj));
    CHECK_RC_CLEANUP(AllocEvent(pCh, hObj, &pNotifyEvent[1]));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0x0, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0x4, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0x8, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0xc, 0xdeadbeef);

    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_NOTIFY_A, LwU64_HI32(m_semaSurf.GetCtxDmaOffsetGpu())));
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_NOTIFY_B, LwU64_LO32(m_semaSurf.GetCtxDmaOffsetGpu())));
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_NOTIFY, DRF_DEF(A197, _NOTIFY, _TYPE, _WRITE_THEN_AWAKEN)));
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->Write(LWA06F_SUBCHANNEL_3D, LWA197_NO_OPERATION, 0));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    //
    // After notification, HW writes a timestamp into the first 8 bytes of the
    // surface and 8-bytes of zeroes at an offset of 8 bytes into the surface.
    // Ensure that a zero exists at offsets 0x8 and 0xc of the surface.
    //
    if ((MEM_RD32(((LwU8*)m_semaSurf.GetAddress())+0x0) == 0xdeadbeef) ||
        (MEM_RD32(((LwU8*)m_semaSurf.GetAddress())+0x4) == 0xdeadbeef) ||
        (MEM_RD32(((LwU8*)m_semaSurf.GetAddress())+0x8) != 0) ||
        (MEM_RD32(((LwU8*)m_semaSurf.GetAddress())+0xc) != 0))
    {
        Printf(Tee::PriHigh, "%s: Graphics notify did not occur properly\n", __FUNCTION__);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    if (!Tasker::IsEventSet(pNotifyEvent[0]))
    {
        Printf(Tee::PriHigh, "%s: Graphics event not set after Graphics notify\n", __FUNCTION__);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    Tasker::ResetEvent(pNotifyEvent[0]);

    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0x0, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0x4, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0x8, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf.GetAddress())+0xc, 0xdeadbeef);

    MEM_WR32(((LwU8*)m_semaSurf2.GetAddress())+0x0, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf2.GetAddress())+0x4, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf2.GetAddress())+0x8, 0xdeadbeef);
    MEM_WR32(((LwU8*)m_semaSurf2.GetAddress())+0xc, 0xdeadbeef);

    CHECK_RC_CLEANUP(chGrp.GetChannel(1)->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_NOTIFY_A, LwU64_HI32(m_semaSurf2.GetCtxDmaOffsetGpu())));
    CHECK_RC_CLEANUP(chGrp.GetChannel(1)->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_NOTIFY_B, LwU64_LO32(m_semaSurf2.GetCtxDmaOffsetGpu())));
    CHECK_RC_CLEANUP(chGrp.GetChannel(1)->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_NOTIFY, DRF_DEF(A1C0, _NOTIFY, _TYPE, _WRITE_THEN_AWAKEN)));
    CHECK_RC_CLEANUP(chGrp.GetChannel(1)->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_NO_OPERATION, 0));
    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    //
    // After notification, HW writes a timestamp into the first 8 bytes of the
    // surface and 8-bytes of zeroes at an offset of 8 bytes into the surface.
    // Ensure that a zero exists at offsets 0x8 and 0xc of the surface.
    //
    if ((MEM_RD32(((LwU8*)m_semaSurf2.GetAddress())+0x0) == 0xdeadbeef) ||
        (MEM_RD32(((LwU8*)m_semaSurf2.GetAddress())+0x4) == 0xdeadbeef) ||
        (MEM_RD32(((LwU8*)m_semaSurf2.GetAddress())+0x8) != 0) ||
        (MEM_RD32(((LwU8*)m_semaSurf2.GetAddress())+0xc) != 0) ||
        (MEM_RD32(((LwU8*)m_semaSurf.GetAddress()) +0x0) != 0xdeadbeef) ||
        (MEM_RD32(((LwU8*)m_semaSurf.GetAddress()) +0x4) != 0xdeadbeef) ||
        (MEM_RD32(((LwU8*)m_semaSurf.GetAddress()) +0x8) != 0xdeadbeef) ||
        (MEM_RD32(((LwU8*)m_semaSurf.GetAddress()) +0xc) != 0xdeadbeef))
    {
        Printf(Tee::PriHigh, "%s: Compute notify did not occur properly\n", __FUNCTION__);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    if (!Tasker::IsEventSet(pNotifyEvent[1]))
    {
        Printf(Tee::PriHigh, "%s: Compute event not set after Compute notify\n", __FUNCTION__);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

Cleanup:
    for (LwU32 i = 0; i < grpSz; i++)
    {
        FreeObjects(chGrp.GetChannel(i));
        Tasker::FreeEvent(pNotifyEvent[i]);
    }

    chGrp.Free();

    return rc;
}

/**
 * @brief Tests freeing one channel while another is running in the same TSG
 *
 * @return
 */
RC ChannelGroupTest::FreeWhileRunningTest()
{
    RC rc;
    const LwU32 grpSz = 2;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream *pStream;
    Channel *pCh = NULL;
    LwRm::Handle hObj;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());
    for (LwU32 i = 0; i < grpSz; i++)
    {
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
        CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
        CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
    }

    CHECK_RC_CLEANUP(chGrp.Schedule());

    MEM_WR32(((LwU8*)m_semaSurf.GetAddress()), 0xdeadbeef);

    // Send host acquire on one channel
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->SetSemaphoreOffset(m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(chGrp.GetChannel(0)->SemaphoreAcquire(0));

    // Flush, will force channel busy until sema release below
    CHECK_RC_CLEANUP(chGrp.Flush());

    // Send backend release on the other channel
    pStream = new ChannelGroup::SplitMethodStream(chGrp.GetChannel(1));
    CHECK_RC_CLEANUP(WriteBackendOffset(pStream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(pStream, LWA06F_SUBCHANNEL_3D, 0));
    delete pStream;

    // Free should flush/WFI on channel
    chGrp.FreeChannel(chGrp.GetChannel(1));

    // Check that freed channel releases its semaphore and allows other channel to WFI
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
    {
        Printf(Tee::PriHigh, "%s: Invalid semaphore value 0x%x\n",
                __FUNCTION__, MEM_RD32(m_semaSurf.GetAddress()));
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

Cleanup:
    for (LwU32 i = 0; i < chGrp.GetNumChannels(); i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

/**
 * @brief Tests that a TSG containing only sw objects works properly.
 *
 * @return
 */
RC ChannelGroupTest::SwChannelTest()
{
    RC rc;
    LwRmPtr pLwRm;
    ChannelGroup chGrp(&m_TestConfig);
    Channel *pCh = NULL;
    LwRm::Handle hObj = 0;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_SW);
    CHECK_RC_CLEANUP(chGrp.Alloc());
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(pLwRm->Alloc(pCh->GetHandle(), &hObj, LW04_SOFTWARE_TEST, NULL));

    MEM_WR32(((LwU8*)m_semaSurf.GetAddress()), 0xdeadbeef);

    CHECK_RC_CLEANUP(chGrp.Schedule());

    pCh->SetObject(5, hObj);
    pCh->SetSemaphoreOffset(m_semaSurf.GetCtxDmaOffsetGpu());
    pCh->SemaphoreRelease(0);
    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
    {
        Printf(Tee::PriHigh, "%s: Invalid semaphore value 0x%x\n",
                __FUNCTION__, MEM_RD32(m_semaSurf.GetAddress()));
        CHECK_RC_CLEANUP(RC::DATA_MISMATCH);
    }

Cleanup:
    pLwRm->Free(hObj);
    chGrp.Free();
    return rc;
}

/**
 * @brief Tests API to get/set TSG timeslice.
 *
 * @return
 */
RC ChannelGroupTest::ModifyTimesliceTest()
{
    RC rc;
    Channel *pCh;
    LwRmPtr pLwRm;
    LWA06C_CTRL_TIMESLICE_PARAMS tsParams = {0};
    LwU64 tsTemp;
    ChannelGroup chGrp0(&m_TestConfig);
    ChannelGroup chGrp1(&m_TestConfig);
    ChannelGroup chGrp2(&m_TestConfig);

    chGrp0.SetUseVirtualContext(m_bVirtCtx);
    chGrp1.SetUseVirtualContext(m_bVirtCtx);

    chGrp0.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp0.Alloc());
    CHECK_RC_CLEANUP(chGrp0.AllocChannel(&pCh));

    chGrp1.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp1.Alloc());
    CHECK_RC_CLEANUP(chGrp1.AllocChannel(&pCh));

    CHECK_RC_CLEANUP(chGrp0.Bind(LW2080_ENGINE_TYPE_GRAPHICS));
    CHECK_RC_CLEANUP(chGrp0.Schedule());

    CHECK_RC_CLEANUP(chGrp1.Bind(LW2080_ENGINE_TYPE_GRAPHICS));
    CHECK_RC_CLEANUP(chGrp1.Schedule());

    // Make sure default is same for both.
    CHECK_RC_CLEANUP(pLwRm->Control(chGrp0.GetHandle(),
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));

    tsTemp = tsParams.timesliceUs;

    CHECK_RC_CLEANUP(pLwRm->Control(chGrp1.GetHandle(),
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));

    if (tsTemp != tsParams.timesliceUs)
    {
        Printf(Tee::PriHigh, "%s: All default timeslices should be the same (%lld != %lld)\n",
                __FUNCTION__, tsTemp, tsParams.timesliceUs);

        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    // Set one and make sure it sticks
    tsParams.timesliceUs *= 2;

    CHECK_RC_CLEANUP(pLwRm->Control(chGrp0.GetHandle(),
            LWA06C_CTRL_CMD_SET_TIMESLICE, &tsParams, sizeof(tsParams)));
    CHECK_RC_CLEANUP(pLwRm->Control(chGrp0.GetHandle(),
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));

    if (tsParams.timesliceUs != tsTemp * 2)
    {
        Printf(Tee::PriHigh, "%s: Could not program timeslice, set %lld read %lld\n",
                __FUNCTION__, tsTemp*2, tsParams.timesliceUs);
        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    // Now make sure setting chGrp0 didn't change chGrp1
    CHECK_RC_CLEANUP(pLwRm->Control(chGrp1.GetHandle(),
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));

    if (tsTemp != tsParams.timesliceUs)
    {
        Printf(Tee::PriHigh, "%s: Modifying one channel group's timeslice changed another channel group's timeslice\n",
                __FUNCTION__);

        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

    // And finally, make sure a newly allocated group still gets the default.
    chGrp2.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp2.Alloc());
    CHECK_RC_CLEANUP(pLwRm->Control(chGrp2.GetHandle(),
            LWA06C_CTRL_CMD_GET_TIMESLICE, &tsParams, sizeof(tsParams)));

    if (tsTemp != tsParams.timesliceUs)
    {
        Printf(Tee::PriHigh, "%s: All default timeslices should be the same (%lld != %lld)\n",
                __FUNCTION__, tsTemp, tsParams.timesliceUs);

        CHECK_RC_CLEANUP(RC::SOFTWARE_ERROR);
    }

Cleanup:
    chGrp0.Free();
    chGrp1.Free();
    chGrp2.Free();
    return rc;
}

/**
 * @brief Tests API to preempt a TSG
 *
 * @return
 */
RC ChannelGroupTest::PreemptAPITest()
{
    RC rc;
    Channel *pCh;
    LwRm::Handle hObj;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
    CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));

    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
    CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    // Do a semaphore release
    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);
    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_3D, 0));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
        rc = RC::DATA_MISMATCH;

    // Preempt the TSG
    CHECK_RC_CLEANUP(chGrp.Preempt());

Cleanup:
    chGrp.Free();
    return rc;
}

/**
 * @brief Tests if we can properly schedule a TSG with some channels w/o objects on it.
 *
 * @return
 */
RC ChannelGroupTest::SomeHostOnlyTest()
{
    RC rc;
    Channel *pCh;
    LwRm::Handle hObj;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
    CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));

    // Channel doesn't have object allocated
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    // Do a semaphore release
    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);
    CHECK_RC_CLEANUP(WriteBackendOffset(&stream, LWA06F_SUBCHANNEL_3D, m_semaSurf.GetCtxDmaOffsetGpu()));
    CHECK_RC_CLEANUP(WriteBackendRelease(&stream, LWA06F_SUBCHANNEL_3D, 0));

    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
        rc = RC::DATA_MISMATCH;

Cleanup:
    chGrp.Free();
    return rc;
}

/**
 * @brief Tests if we can properly schedule a TSG with only host channels on it.
 *
 * @return
 */
RC ChannelGroupTest::HostOnlyTest()
{
    RC rc;
    Channel *pCh;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));

    CHECK_RC_CLEANUP(chGrp.Schedule());

    // Do a semaphore release (host since we have no engine objects).
    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);
    pCh->Write(0, LWA06F_SEMAPHOREA, LwU64_HI32(m_semaSurf.GetCtxDmaOffsetGpu()));
    pCh->Write(0, LWA06F_SEMAPHOREB, LwU64_LO32(m_semaSurf.GetCtxDmaOffsetGpu()));
    pCh->Write(0, LWA06F_SEMAPHOREC, 0);
    pCh->Write(0, LWA06F_SEMAPHORED, DRF_DEF(A06F, _SEMAPHORED, _OPERATION, _RELEASE));
    pCh->Flush();

    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    if (MEM_RD32(m_semaSurf.GetAddress()) != 0)
        rc = RC::DATA_MISMATCH;

Cleanup:
    chGrp.Free();
    return rc;
}

/**
 * @brief Tests if we can add a channel to a TSG without it being scheduled.
 *
 * @return
 */
RC ChannelGroupTest::SubcontextScheduleTest()
{
    RC rc = OK;
    ChannelGroup chGrp(&m_TestConfig);
    ChannelGroup::SplitMethodStream stream(&chGrp);
    const LwU32 grpSz = 3;

    UINT32 *cpuAddr = (UINT32 *) m_semaSurf.GetAddress();
    UINT64 gpuAddr = m_semaSurf.GetCtxDmaOffsetGpu();
    UINT32 semGot, semExp = 0;

    chGrp.SetUseVirtualContext(m_bVirtCtx);
    chGrp.SetEngineId(LW2080_ENGINE_TYPE_GR(0));
    CHECK_RC_CLEANUP(chGrp.Alloc());

    Channel *pCh;
    LwRm::Handle hObj;
    CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh));
    CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COMPUTE, &hObj));
    CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObj));

    CHECK_RC(pCh->SetSemaphoreOffset(gpuAddr));
    MEM_WR32(m_semaSurf.GetAddress(), 0xdeadbeef);

    CHECK_RC_CLEANUP(chGrp.Schedule());
    CHECK_RC_CLEANUP(chGrp.Flush());
    CHECK_RC_CLEANUP(chGrp.WaitIdle());

    for (LwU32 i = 1; i < grpSz; i++)
    {
        CHECK_RC_CLEANUP(chGrp.AllocChannel(&pCh, FLD_SET_DRF(OS04, _FLAGS, _DELAY_CHANNEL_SCHEDULING, _TRUE, 0)));
        if(i == 1)
        {
            CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_COMPUTE, &hObj));
            CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hObj));
        }
        else
        {
            CHECK_RC_CLEANUP(AllocObject(pCh, LWA06F_SUBCHANNEL_3D, &hObj));
            CHECK_RC_CLEANUP(pCh->SetObject(LWA06F_SUBCHANNEL_3D, hObj));
        }

        //Positive test - check if scheduled
        pCh->RmcGpfifoSchedule(true);
        CHECK_RC_CLEANUP(chGrp.Flush());

        semExp = 0xf000 + i;
        CHECK_RC(pCh->SetSemaphoreOffset(gpuAddr));
        CHECK_RC(pCh->SemaphoreRelease(semExp));
        pCh->Flush();

        CHECK_RC_CLEANUP(chGrp.WaitIdle());
        semGot = MEM_RD32(cpuAddr);
        if(semGot != semExp)
        {
            Printf(Tee::PriHigh, "expected 0x%x, got 0x%x\n", semExp, semGot);
            rc = RC::UNEXPECTED_RESULT;
            goto Cleanup;
        }
    }

Cleanup:
    for (LwU32 i = 0; i < grpSz; i++)
        FreeObjects(chGrp.GetChannel(i));
    chGrp.Free();

    return rc;
}

RC ChannelGroupTest::AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj, LwU32 engineInstance)
{
    RC rc;
    LwRm::Handle hCh = pCh->GetHandle();
    MASSERT(hObj);

    switch (subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            MASSERT(engineInstance == (LwU32) -1);
            CHECK_RC(m_3dAllocs[hCh].Alloc(hCh, GetBoundGpuDevice()));
            *hObj = m_3dAllocs[hCh].GetHandle();
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            MASSERT(engineInstance == (LwU32) -1);
            CHECK_RC(m_computeAllocs[hCh].Alloc(hCh, GetBoundGpuDevice()));
            *hObj = m_computeAllocs[hCh].GetHandle();
            break;
        case LWA06F_SUBCHANNEL_COPY_ENGINE:
            if (engineInstance == (LwU32) -1)
                engineInstance = GetGrCopyInstance(pCh->GetClass());
            CHECK_RC(m_ceAllocs[hCh].AllocOnEngine(hCh,
                                                   LW2080_ENGINE_TYPE_COPY(engineInstance),
                                                   GetBoundGpuDevice(),
                                                   LwRmPtr().Get()));
            *hObj = m_ceAllocs[hCh].GetHandle();
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }
    return rc;
}

RC ChannelGroupTest::AllocEvent(Channel *pCh, LwRm::Handle hObj, ModsEvent **pNotifyEvent)
{
    LwRmPtr pLwRm;
    RC rc;
    void *pEventAddr;
    LwRm::Handle hNotifyEvent;

    *pNotifyEvent = Tasker::AllocEvent();
    pEventAddr = Tasker::GetOsEvent(
            *pNotifyEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    rc = pLwRm->AllocEvent(hObj, &hNotifyEvent, LW01_EVENT_OS_EVENT, 0, pEventAddr);
    if (rc == OK)
    {
        m_objectEvents[hObj].hNotifyEvent = hNotifyEvent;
        m_objectEvents[hObj].pNotifyEvent = *pNotifyEvent;
        m_objectEvents[hObj].bAlloced = true;
    }

    return rc;
}

void ChannelGroupTest::FreeObjects(Channel *pCh)
{
    LwRm::Handle hCh = pCh->GetHandle();

    if (m_ceAllocs.count(hCh)){ m_ceAllocs[hCh].Free(); m_ceAllocs.erase(hCh); }
    if (m_3dAllocs.count(hCh)){ m_3dAllocs[hCh].Free(); m_3dAllocs.erase(hCh); }
    if (m_computeAllocs.count(hCh)){ m_computeAllocs[hCh].Free(); m_computeAllocs.erase(hCh); }
}

RC ChannelGroupTest::CleanObjects()
{
    RC rc = OK;

    if (!m_ceAllocs.empty()){ m_ceAllocs.clear(); rc = -1; }
    if (!m_3dAllocs.empty()){ m_3dAllocs.clear(); rc.Clear(); rc = -1; }
    if (!m_computeAllocs.empty()){ m_computeAllocs.clear(); rc.Clear(); rc = -1; }

    return rc;
}

LwU32 ChannelGroupTest::GetGrCopyInstance(LwU32 parentClass)
{
    LwRmPtr pLwRm;
    RC rc;
    LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS partnerParams = {0};

    partnerParams.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
    partnerParams.partnershipClassId = parentClass;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
            LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST,
            &partnerParams, sizeof(partnerParams));
    MASSERT(rc == OK);
    MASSERT(partnerParams.numPartners == 1);

    return partnerParams.partnerList[0] - LW2080_ENGINE_TYPE_COPY0;
}

RC  ChannelGroupTest::WriteBackendOffset(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU64 offset)
{
    RC rc;
    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_A, LwU64_HI32(offset)));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_B, LwU64_LO32(offset)));
            break;
        case LWA06F_SUBCHANNEL_COPY_ENGINE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SEMAPHORE_A,
                    LwU64_HI32(offset)));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SEMAPHORE_B,
                    LwU64_LO32(offset)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;

}

RC  ChannelGroupTest::WriteBackendRelease(ChannelGroup::SplitMethodStream *stream, LwU32 subch, LwU32 data, bool bAwaken)
{
    RC rc;

    switch(subch)
    {
        case LWA06F_SUBCHANNEL_3D:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_3D, LWA197_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(A197, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(A197, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        case LWA06F_SUBCHANNEL_COMPUTE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_C, data));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COMPUTE, LWA1C0_SET_REPORT_SEMAPHORE_D,
                        DRF_DEF(A1C0, _SET_REPORT_SEMAPHORE_D, _OPERATION, _RELEASE) |
                        (bAwaken ? DRF_DEF(A1C0, _SET_REPORT_SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) : 0)));
            break;
        case LWA06F_SUBCHANNEL_COPY_ENGINE:
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SEMAPHORE_PAYLOAD, data));
            CHECK_RC(stream->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_LAUNCH_DMA,
                         DRF_DEF(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                         (bAwaken ? DRF_DEF(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _BLOCKING) : DRF_DEF(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE)  )|
                         DRF_DEF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                         DRF_DEF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NONE)));
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }

    return rc;
}

JS_CLASS_INHERIT(ChannelGroupTest, RmTest,
    "ChannelGroupTest RMTEST that tests KEPLER_CHANNEL_GROUP_A functionality");
CLASS_PROP_READWRITE(ChannelGroupTest, bRunMaxGroups, bool,
        "Forces running of max channel groups tests (usually runs only on silicon, very slow)");
CLASS_PROP_READWRITE(ChannelGroupTest, ForceTest, string,
        "Run specific test");

