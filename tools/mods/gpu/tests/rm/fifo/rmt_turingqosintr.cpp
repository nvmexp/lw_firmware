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
//! \file rmt_TuringQosIntr.cpp
//! \brief To verify basic functionality of KEPLER_CHANNEL_GROUP_A object
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
#include "turing/tu102/dev_pbdma.h"
#include "turing/tu102/dev_fifo.h"
#include "ctrl/ctrla06f.h"
#include "ctrl/ctrla06c.h"
#include "ctrl/ctrl2080.h"

#include "class/cl0005.h"  // LW01_EVENT
#include "class/cla06c.h"
#include "class/cla06f.h"
#include "class/cla16f.h"
#include "class/cla197.h"
#include "class/cla1c0.h"
#include "class/cla0b5.h"
#include "class/cla06fsubch.h"
#include "class/cl9067.h"
#include "class/cl007d.h"

#include "lwRmApi.h"
// Must be last
#include "core/include/memcheck.h"

#define PHYSICAL        0x00000001
static bool bRunlistIdleCbFired = false;
static bool bRunlistEngIdleCbFired = false;
static bool bRunlistBlockedCbFired= false;
static bool bRunlistBlockedEngIdleCbFired= false;
static bool bTsgCallbackFired= false;
// static bool bCtxswTimeoutCbFired = false;

bool failtest = LW_FALSE;

class TuringQosIntrTest : public RmTest
{
    public:
    TuringQosIntrTest();
    virtual ~TuringQosIntrTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(ForceTest, string);

    private:
    RC BasicQosInterruptTest();
    // RC CtxSwitchTimeoutTest();
    RC AcquireTimeoutLoggingTest();

    LwU32 GetGrCopyInstance(LwU32);

    RC AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj, LwU32 engineInstance = (LwU32) -1);
    void FreeObjects(Channel *pCh);

    RC setupRunlistIdleIntrCb();
    RC setupRunlistAndEngIdleIntrCb();
    RC setupRunlistBlockedIntrCb();
    RC setupRunlistBlockedEngIdleIntrCb();
    RC setupTsgPreemptIntrCb();
    // RC setupCtxswTimeoutCb();

    RC submitBlockingAcquire(Channel *pCh, LwU32 val);
    RC submitRelease(Channel *pCh, LwU32 val);
    RC CleanObjects();

    bool bRunOnError;
    Surface2D m_semaSurf;
    Surface2D m_copySurfSrc;
    Surface2D m_copySurfDest;
    string m_ForceTest;

    map<LwRm::Handle, ThreeDAlloc> m_3dAllocs;
    map<LwRm::Handle, DmaCopyAlloc> m_ceAllocs;
    bool m_bLateChannelAlloc;

    // ChannelGroup m_chGrp1(&m_TestConfig);
    ChannelGroup m_chGrp1;
    Channel *m_pCh1;
    LwRm::Handle m_hObj1;

    ChannelGroup m_chGrp2;
    Channel *m_pCh2;
    LwRm::Handle m_hObj2;

    LWOS10_EVENT_KERNEL_CALLBACK_EX m_tsgPreemptCb;
    LWOS10_EVENT_KERNEL_CALLBACK_EX m_runlistBlkdCb;
    LWOS10_EVENT_KERNEL_CALLBACK_EX m_runlistBlkdEngIdleCb;
    LWOS10_EVENT_KERNEL_CALLBACK_EX m_runlistEngIdleCb;
    LWOS10_EVENT_KERNEL_CALLBACK_EX m_runlistIdleCb;
    // LWOS10_EVENT_KERNEL_CALLBACK_EX m_ctxswTimeoutCb;

    LwRm::Handle m_hEventRunlistBlocked;
    LwRm::Handle m_hEventRunlistBlockedEngIdle;
    LwRm::Handle m_hEventRunlistEngIdle;
    LwRm::Handle m_hEventRunlistIdle;
    LwRm::Handle m_hEventTsgPreempted;
    // LwRm::Handle m_hEventCtxswTimedout;

    LwRm * pLwRm;
    LwRm::Handle hClient;
    LwRm::Handle hSubdev;
    GpuSubdevice * pSubdev;
};


//! \brief TuringQosIntrTest constructor
//!
//! Placeholder : doesnt do much, much funcationality in Setup()
//!
//! \sa Setup
//------------------------------------------------------------------------------
TuringQosIntrTest::TuringQosIntrTest():
    bRunOnError(false),
    m_bLateChannelAlloc(false),
    m_chGrp1(&m_TestConfig),
    m_pCh1(nullptr),
    m_hObj1(0),
    m_chGrp2(&m_TestConfig),
    m_pCh2(nullptr),
    m_hObj2(0),
    m_tsgPreemptCb({}),
    m_runlistBlkdCb({}),
    m_runlistBlkdEngIdleCb({}),
    m_runlistEngIdleCb({}),
    m_runlistIdleCb({}),
    m_hEventRunlistBlocked(0),
    m_hEventRunlistBlockedEngIdle(0),
    m_hEventRunlistEngIdle(0),
    m_hEventRunlistIdle(0),
    m_hEventTsgPreempted(0),
    hClient(0),
    hSubdev(0),
    pSubdev(nullptr)
{
    pLwRm = GetBoundRmClient();
    SetName("TuringQosIntrTest");
}



//! \brief TuringQosIntrTest destructor
//!
//! Placeholder : doesnt do much, most functionality in Setup()
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
TuringQosIntrTest::~TuringQosIntrTest()
{

}



//! \brief IsSupported(), Looks for whether test can execute in current elw.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string TuringQosIntrTest::IsTestSupported()
{
    return pLwRm->IsClassSupported(KEPLER_CHANNEL_GROUP_A, GetBoundGpuDevice()) ? RUN_RMTEST_TRUE : "Channel groups not supported";
}

static
RC allocateCeSurface(Surface2D &surf, int size, ColorUtils::Format colorFormat, GpuDevice* pDevice)
{
    RC rc;

    surf.SetForceSizeAlloc(true);
    surf.SetArrayPitch(1);
    surf.SetArraySize(size);
    surf.SetColorFormat(colorFormat);
    surf.SetAddressModel(Memory::Paging);
    surf.SetLayout(Surface2D::Pitch);
    surf.SetLocation(Memory::Fb);
    surf.SetPhysContig(true);
    CHECK_RC(surf.Alloc(pDevice));
    CHECK_RC(surf.Map());

    return rc;
}

//! \brief  Setup(): Generally used for any test level allocation
//!
//! Allocating the channel and software object for use in the test
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC TuringQosIntrTest::Setup()
{
    RC rc;
    CHECK_RC(InitFromJs());

    pLwRm = GetBoundRmClient();
    pSubdev = GetBoundGpuSubdevice();
    hSubdev = pLwRm->GetSubdeviceHandle(pSubdev);

    m_TestConfig.SetAllowMultipleChannels(true);

    CHECK_RC(allocateCeSurface(m_semaSurf, 0x1000, ColorUtils::VOID32, GetBoundGpuDevice()));
    CHECK_RC(allocateCeSurface(m_copySurfSrc, 0x800000, ColorUtils::Y8, GetBoundGpuDevice()));
    CHECK_RC(allocateCeSurface(m_copySurfDest, 0x800000, ColorUtils::Y8, GetBoundGpuDevice()));

    CHECK_RC(setupRunlistIdleIntrCb());
    CHECK_RC(setupRunlistAndEngIdleIntrCb());
    CHECK_RC(setupRunlistBlockedIntrCb());
    CHECK_RC(setupRunlistBlockedEngIdleIntrCb());
    CHECK_RC(setupTsgPreemptIntrCb());
    // CHECK_RC(setupCtxswTimeoutCb());

    return OK;
}

//! \brief  Cleanup():
//!
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC TuringQosIntrTest::Cleanup()
{

    pLwRm->Free(m_hEventRunlistBlocked);
    pLwRm->Free(m_hEventRunlistBlockedEngIdle);
    pLwRm->Free(m_hEventRunlistEngIdle);
    pLwRm->Free(m_hEventRunlistIdle);
    pLwRm->Free(m_hEventTsgPreempted);
    // pLwRm->Free(m_hEventCtxswTimedout);

    m_copySurfSrc.Free();
    m_copySurfDest.Free();
    m_semaSurf.Free();

    return OK;
}


//! \brief  Run():
//!
//
//! \return corresponding RC if any allocation fails
//------------------------------------------------------------------------------
RC TuringQosIntrTest::Run()
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
            return rc;\
        if(CleanObjects() != OK)\
            Printf(Tee::PriHigh, "%s: CleanObjects had to clean state after %s!\n", __FUNCTION__, #name);\
    }\
    else\
    {\
        Printf(Tee::PriLow, "%s: Not running %s\n", __FUNCTION__, #name);\
    }

    // Basic functionality, allocate and send methods ensuring exelwtion order.

    RUN_TEST(BasicQosInterruptTest);
    RUN_TEST(AcquireTimeoutLoggingTest);
    // RUN_TEST(CtxSwitchTimeoutTest);

    CHECK_RC(errorRc);
    return rc;
}

void runlistIdleCallback(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
   Lw2080QosIntrNotification * notification = (Lw2080QosIntrNotification *) pData;
   Printf(Tee::PriHigh, "%s: %d fired for Engine: 0x%x \n", __FUNCTION__, bRunlistIdleCbFired, notification->engineType);
   bRunlistIdleCbFired = true;
}

void runlistAndEngIdleCallback(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
   Lw2080QosIntrNotification * notification = (Lw2080QosIntrNotification *) pData;
   Printf(Tee::PriHigh, "%s: %d fired for Engine: 0x%x \n", __FUNCTION__, bRunlistEngIdleCbFired,
          notification->engineType);
   bRunlistEngIdleCbFired = true;
}

static void runlistBlockedCallback(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
   Lw2080QosIntrNotification * notification = (Lw2080QosIntrNotification *) pData;
   Printf(Tee::PriHigh, "%s: %d fired for Engine: 0x%x \n", __FUNCTION__, bRunlistBlockedCbFired,
          notification->engineType);
   bRunlistBlockedCbFired = true;
}

static void runlistBlockedEngIdleCallback(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
   Lw2080QosIntrNotification * notification = (Lw2080QosIntrNotification *) pData;
   Printf(Tee::PriHigh, "%s: %d for Engine: 0x%x \n", __FUNCTION__, bRunlistBlockedEngIdleCbFired,
          notification->engineType);
   bRunlistBlockedEngIdleCbFired = true;
}

void tsgPreemptCallback(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{

   Printf(Tee::PriHigh, "%s: %d\n", __FUNCTION__, bTsgCallbackFired);
   bTsgCallbackFired = true;
}

#if 0
void ctxswTimeoutCallback(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
   Printf(Tee::PriHigh, "%s: %d\n", __FUNCTION__, bCtxswTimeoutCbFired);
   bCtxswTimeoutCbFired = true;
}
#endif

RC CePhysicalCopy(Channel *pCh, LwRm::Handle hLwrCeObj, LwU32 locSubChannel,
   LwU64 source, Memory::Location srcLoc, LwU32 srcAddrType,
   LwU64 dest, Memory::Location destLoc, LwU32 destAddrType,
   LwU64 blockSize)
{

    LwU32 addrTypeFlags = 0;

    // step 1: set the src and destination addresses
    pCh->SetObject(locSubChannel, hLwrCeObj);
    pCh->Write(locSubChannel, LWA0B5_OFFSET_IN_UPPER, LwU64_HI32(source));
    pCh->Write(locSubChannel, LWA0B5_OFFSET_IN_LOWER, LwU64_LO32(source));

    pCh->Write(locSubChannel, LWA0B5_OFFSET_OUT_UPPER, LwU64_HI32(dest));
    pCh->Write(locSubChannel, LWA0B5_OFFSET_OUT_LOWER, LwU64_LO32(dest));

    //
    // step 2: set where the addresses point to (either FB, non-coherent sysmem,
    // or coherent sysmem
    //
    switch (srcLoc)
    {
        case Memory::Fb:
            pCh->Write(locSubChannel, LWA0B5_SET_SRC_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB));
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    switch (destLoc)
    {
        case Memory::Fb:
            pCh->Write(locSubChannel, LWA0B5_SET_DST_PHYS_MODE,
                        DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB));
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    //
    // step 3: set the necessary surface/block parameters based on what kind of
    // address was passed in. address passed in can either be VIRTUAL or PHYSICAL
    //
    switch (srcAddrType)
    {
        case PHYSICAL:
            pCh->Write(locSubChannel, LWA0B5_LINE_LENGTH_IN, LwU64_LO32(blockSize));
            addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _PHYSICAL);
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    switch (destAddrType)
    {
        case PHYSICAL:
            addrTypeFlags |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL);
            break;
        default:
            return RC::BAD_PARAMETER;
    }

    // step 4: set how big the block of memory is
    pCh->Write(locSubChannel, LWA0B5_LINE_COUNT, 1);
    // step 6: launch the CE
    pCh->Write(locSubChannel, LWA0B5_LAUNCH_DMA,
            DRF_DEF(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _BLOCKING) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH) |
            DRF_DEF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED) |
            addrTypeFlags);

    return OK;

}
#if 0
RC
TuringQosIntrTest::setupCtxswTimeoutCb()
{

    RC rc;
    LW0005_ALLOC_PARAMETERS allocParams;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS enableNotify;

    memset(&m_ctxswTimeoutCb, 0, sizeof(m_ctxswTimeoutCb));
    m_ctxswTimeoutCb.func = ctxswTimeoutCallback;
    m_ctxswTimeoutCb.arg  = NULL;

    // TODO change it to EVENT
    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_CTXSW_TIMEOUT;
    allocParams.data          = LW_PTR_TO_LwP64(&m_ctxswTimeoutCb);

    CHECK_RC(pLwRm->Alloc(hSubdev,
                          &m_hEventCtxswTimedout,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&enableNotify, 0, sizeof(enableNotify));
    enableNotify.event  = LW2080_NOTIFIERS_CTXSW_TIMEOUT;
    enableNotify.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(hSubdev,
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &enableNotify, sizeof(enableNotify)));
    return rc;
}
#endif

RC
TuringQosIntrTest::setupRunlistIdleIntrCb()
{
    RC rc;
    LW0005_ALLOC_PARAMETERS allocParams;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS enableNotify;

    memset(&m_runlistIdleCb, 0, sizeof(m_runlistIdleCb));
    m_runlistIdleCb.func = runlistIdleCallback;
    m_runlistIdleCb.arg  = NULL;

    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_RUNLIST_IDLE;
    allocParams.data          = LW_PTR_TO_LwP64(&m_runlistIdleCb);

    CHECK_RC(pLwRm->Alloc(hSubdev,
                          &m_hEventRunlistIdle,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&enableNotify, 0, sizeof(enableNotify));
    enableNotify.event = LW2080_NOTIFIERS_RUNLIST_IDLE;
    enableNotify.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(hSubdev,
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &enableNotify, sizeof(enableNotify)));

    return rc;
}

RC
TuringQosIntrTest::setupRunlistBlockedEngIdleIntrCb()
{
    RC rc;
    LW0005_ALLOC_PARAMETERS allocParams;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS enableNotify;

    memset(&m_runlistBlkdEngIdleCb, 0, sizeof(m_runlistBlkdEngIdleCb));
    m_runlistBlkdEngIdleCb.func = runlistBlockedEngIdleCallback;
    m_runlistBlkdEngIdleCb.arg  = NULL;

    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_RUNLIST_ACQUIRE_AND_ENG_IDLE;
    allocParams.data          = LW_PTR_TO_LwP64(&m_runlistBlkdEngIdleCb);

    CHECK_RC(pLwRm->Alloc(hSubdev,
                          &m_hEventRunlistBlockedEngIdle,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&enableNotify, 0, sizeof(enableNotify));
    enableNotify.event = LW2080_NOTIFIERS_RUNLIST_ACQUIRE_AND_ENG_IDLE;
    enableNotify.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(hSubdev,
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &enableNotify, sizeof(enableNotify)));

    return rc;
}

RC
TuringQosIntrTest::setupRunlistBlockedIntrCb()
{
    RC rc;
    LW0005_ALLOC_PARAMETERS allocParams;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS enableNotify;

    memset(&m_runlistBlkdCb, 0, sizeof(m_runlistBlkdCb));
    m_runlistBlkdCb.func = runlistBlockedCallback;
    m_runlistBlkdCb.arg  = NULL;

    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_RUNLIST_ACQUIRE;
    allocParams.data          = LW_PTR_TO_LwP64(&m_runlistBlkdCb);

    CHECK_RC(pLwRm->Alloc(hSubdev,
                          &m_hEventRunlistBlocked,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&enableNotify, 0, sizeof(enableNotify));
    enableNotify.event = LW2080_NOTIFIERS_RUNLIST_ACQUIRE;
    enableNotify.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(hSubdev,
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &enableNotify, sizeof(enableNotify)));


    return rc;
}

RC
TuringQosIntrTest::setupRunlistAndEngIdleIntrCb()
{

    RC rc;
    LW0005_ALLOC_PARAMETERS allocParams;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS enableNotify;

    memset(&m_runlistEngIdleCb, 0, sizeof(m_runlistEngIdleCb));
    m_runlistEngIdleCb.func = runlistAndEngIdleCallback;
    m_runlistEngIdleCb.arg  = NULL;

    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_RUNLIST_AND_ENG_IDLE;
    allocParams.data          = LW_PTR_TO_LwP64(&m_runlistEngIdleCb);

    CHECK_RC(pLwRm->Alloc(hSubdev,
                          &m_hEventRunlistEngIdle,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&enableNotify, 0, sizeof(enableNotify));
    enableNotify.event = LW2080_NOTIFIERS_RUNLIST_AND_ENG_IDLE;
    enableNotify.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(hSubdev,
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &enableNotify, sizeof(enableNotify)));
    return rc;
}

RC
TuringQosIntrTest::setupTsgPreemptIntrCb()
{

    RC rc;
    LW0005_ALLOC_PARAMETERS allocParams;
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS enableNotify;

    memset(&m_tsgPreemptCb, 0, sizeof(m_tsgPreemptCb));
    m_tsgPreemptCb.func = tsgPreemptCallback;
    m_tsgPreemptCb.arg  = NULL;

    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_TSG_PREEMPT_COMPLETE;
    allocParams.data          = LW_PTR_TO_LwP64(&m_tsgPreemptCb);

    CHECK_RC(pLwRm->Alloc(hSubdev,
                          &m_hEventTsgPreempted,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&enableNotify, 0, sizeof(enableNotify));
    enableNotify.event = LW2080_NOTIFIERS_TSG_PREEMPT_COMPLETE;
    enableNotify.action = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT;

    CHECK_RC(pLwRm->Control(hSubdev,
                            LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                            &enableNotify, sizeof(enableNotify)));
    return rc;
}

static bool
testForRunlistIdleAndEngIdle(void *pArg)
{
   return (bRunlistEngIdleCbFired && bRunlistIdleCbFired);
}

static bool
testForRunlistBlkdEngIdleIntr(void *pArg)
{
   return (bRunlistBlockedEngIdleCbFired && bRunlistBlockedCbFired);
}

static bool
testForRunlistBlkdIntr(void *pArg)
{
   return bRunlistBlockedCbFired;
}

static bool
testForTsgPreemptIntr(void *pArg)
{
   return bTsgCallbackFired;
}

#if 0
static bool
testForContextSwitchTimeout(void *pArg)
{
   return bCtxswTimeoutCbFired;
}
#endif
RC
TuringQosIntrTest::submitRelease(Channel *pCh, LwU32 val)
{

    RC rc = OK;

    MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0xdeadbeef);

    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SEMAPHORE_A,LwU64_HI32(m_semaSurf.GetCtxDmaOffsetGpu() + 0x10)));
    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA0B5_SET_SEMAPHORE_B,LwU64_LO32(m_semaSurf.GetCtxDmaOffsetGpu() + 0x10)));
    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_SET_SEMAPHORE_PAYLOAD, val));// 0x00dead00));
    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE, LWA0B5_LAUNCH_DMA,
                        DRF_DEF(A0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_ONE_WORD_SEMAPHORE) |
                        DRF_DEF(A0B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                        DRF_DEF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                        DRF_DEF(A0B5, _LAUNCH_DMA,_DATA_TRANSFER_TYPE,_NONE)));

Cleanup:
     return rc;
}

RC
TuringQosIntrTest::submitBlockingAcquire(Channel *pCh, LwU32 val)
{

    RC rc = OK;

    MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0xdeadbeef);

    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHOREA,LwU64_HI32(m_semaSurf.GetCtxDmaOffsetGpu()+0x10)));
    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHOREB,LwU64_LO32(m_semaSurf.GetCtxDmaOffsetGpu()+0x10)));
    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHOREC, val)); // 0x99dead99));

    CHECK_RC_CLEANUP(pCh->Write(LWA06F_SUBCHANNEL_COPY_ENGINE,LWA06F_SEMAPHORED,
                           DRF_DEF(A06F, _SEMAPHORED,_OPERATION,_ACQUIRE)|
                           DRF_DEF(A06F, _SEMAPHORED,_ACQUIRE_SWITCH,_ENABLED)));

Cleanup:
     return rc;
}

RC
TuringQosIntrTest::BasicQosInterruptTest()
{
    RC rc = OK;

    LWA06C_CTRL_TIMESLICE_PARAMS tsParams = {0};
    // Set one and make sure it sticks
    tsParams.timesliceUs = 2000000;
    // initCeEngineId(m_pCh1->getClass());
    m_chGrp1.SetEngineId(LW2080_ENGINE_TYPE_COPY0);
    CHECK_RC_CLEANUP(m_chGrp1.Alloc());
    CHECK_RC_CLEANUP(m_chGrp1.AllocChannel(&m_pCh1));

    CHECK_RC_CLEANUP(pLwRm->Control(m_chGrp1.GetHandle(),LWA06C_CTRL_CMD_SET_TIMESLICE, &tsParams, sizeof(tsParams)));
    CHECK_RC_CLEANUP(AllocObject(m_pCh1, LWA06F_SUBCHANNEL_COPY_ENGINE, &m_hObj1));
    CHECK_RC_CLEANUP(m_chGrp1.Schedule(true));

    Printf(Tee::PriHigh, "%s: Begin Test1 %s \n", __FUNCTION__, "SUBMIT A BLOCKING ACQUIRE" );

    bRunlistBlockedEngIdleCbFired= false;
    bRunlistBlockedCbFired= false;

    CHECK_RC_CLEANUP(m_pCh1->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, m_hObj1));
    CHECK_RC_CLEANUP(submitBlockingAcquire(m_pCh1, 0x99dead99));
    CHECK_RC_CLEANUP(m_chGrp1.Flush());

    rc = POLLWRAP(&testForRunlistBlkdEngIdleIntr, NULL, 10000);
    if (OK != rc)
    {
        failtest = LW_TRUE;
        Printf(Tee::PriHigh, "Test1: RUNLIST_ACQUIRE_AND_ENG_IDLE INTR missed ACQUIRE_ENG_IDLE %d \
                              RUNLIST_ACQUIRE %d \n",bRunlistBlockedEngIdleCbFired, bRunlistBlockedCbFired);
        rc.Clear();
    }
    else
    {
        Printf(Tee::PriHigh, "%s: Test1: RUNLIST_ACQUIRE_AND_ENG_IDLE INTR received  \n", __FUNCTION__);
        Printf(Tee::PriHigh, "%s: Test1: Verifed LW_PFIFO_INTR_0_RUNLIST_ACQUIRE_AND_ENG_IDLE\n", __FUNCTION__);
        Printf(Tee::PriHigh, "%s: Test1: Verifed LW_PFIFO_INTR_0_RUNLIST_ACQUIRE when ENG is IDLE\n", __FUNCTION__);
        Printf(Tee::PriHigh, "%s: End Test 1: Success\n", __FUNCTION__);
    }


    bTsgCallbackFired= false;

#if 1
    Printf(Tee::PriHigh, "%s: Begin Test 2  %s \n", __FUNCTION__, "ISSUE TSG_PREEMPT" );
    CHECK_RC_CLEANUP(m_chGrp1.Preempt(false));


    rc = POLLWRAP(&testForTsgPreemptIntr, NULL, 10000);
    if (OK != rc)
    {
        failtest = LW_TRUE;
        Printf(Tee::PriHigh, "Test2: TSG_PREEMPT_COMPLETE INTR missed\n %d", bTsgCallbackFired);
        rc.Clear();
    }
    else
    {
        Printf(Tee::PriHigh, "%s: Test2: TSG_PREEMPT_COMPLETE INTR received\n", __FUNCTION__);
        Printf(Tee::PriHigh, "%s: End Test 2: Success\n", __FUNCTION__);
    }

#endif
    Printf(Tee::PriHigh, "%s: Begin test 3  %s \n", __FUNCTION__, "Cpu write semaphore and wait for idle" );

    bRunlistEngIdleCbFired = false;
    bRunlistIdleCbFired = false;

    MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0x99dead99);
    CHECK_RC_CLEANUP(m_chGrp1.Flush());

    rc = POLLWRAP(&testForRunlistIdleAndEngIdle, NULL, 10000);
    if (OK != rc)
    {
        failtest = LW_TRUE;
        Printf(Tee::PriHigh, "%s: Test3: RUNLIST_IDLE and/or RUNLIST_AND_ENG_IDLE INTR missed\n", __FUNCTION__);
        rc.Clear();
        rc = RC::TIMEOUT_ERROR;
        goto Cleanup;
    }
    else
    {
        Printf(Tee::PriHigh, "%s: Test3: RUNLIST_IDLE and RUNLIST_AND_ENG_IDLE INTR received\n", __FUNCTION__);
        Printf(Tee::PriHigh, "%s: End Test 3: Success\n", __FUNCTION__);
    }

Cleanup:
   if (rc != OK)
       failtest = LW_TRUE;
   FreeObjects(m_chGrp1.GetChannel(0));
   m_chGrp1.Free();

   if (rc != OK)
   {

        Printf(Tee::PriHigh, "%s: We missed a few interrupts, final check \n", __FUNCTION__);
        // Check one last time
        rc = POLLWRAP(&testForRunlistIdleAndEngIdle, NULL, 10000);
        if (rc != OK)
            failtest = LW_TRUE;

        rc = POLLWRAP(&testForTsgPreemptIntr, NULL, 10000);
        if (rc != OK)
            failtest = LW_TRUE;


        rc = POLLWRAP(&testForRunlistBlkdEngIdleIntr, NULL, 10000);
        if (OK != rc)
            failtest = LW_TRUE;

        Printf(Tee::PriHigh, "%s: Received all interrupts in final check, returning success\n", __FUNCTION__);
   }
   return rc;
}
#if 0
RC
TuringQosIntrTest::CtxSwitchTimeoutTest()
{

    RC rc = OK;
    int i =0;
    LW2080_CTRL_FIFO_CONFIG_CTXSW_TIMEOUT_PARAMS ctxswParams = {0};
    //pthread_t preempt_thread;

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    Printf(Tee::PriHigh, "%s: Begin Test 5: LW_PFIFO_0_CTXSW_TIMEOUT INTR test\n", __FUNCTION__);
    m_chGrp1.SetEngineId(LW2080_ENGINE_TYPE_COPY0);
    CHECK_RC_CLEANUP(m_chGrp1.Alloc());
    CHECK_RC_CLEANUP(m_chGrp1.AllocChannel(&m_pCh1));
    CHECK_RC_CLEANUP(AllocObject(m_pCh1, LWA06F_SUBCHANNEL_COPY_ENGINE, &m_hObj1));

    // Call into RM, set the context switch timeout and enable context switch timeout detection using
    // a control call.
    bCtxswTimeoutCbFired = false;

    ctxswParams.timeout = 5;
    ctxswParams.bEnable = LW_TRUE;
    rc = pLwRm->Control(hSubdev,
            LW2080_CTRL_CMD_FIFO_CONFIG_CTXSW_TIMEOUT,
            &ctxswParams, sizeof(ctxswParams));
    MASSERT(rc == OK);

    // Submit a lot of copies
    CHECK_RC_CLEANUP(m_pCh1->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, m_hObj1));

    for (int z= 0; z < 10000; z++)
    {

    CHECK_RC_CLEANUP(CePhysicalCopy(m_pCh1, m_hObj1, LWA06F_SUBCHANNEL_COPY_ENGINE,
                         m_copySurfSrc.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                         m_copySurfDest.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                         m_copySurfDest.GetSize()));

    // Submit Some copy methods
    CHECK_RC_CLEANUP(CePhysicalCopy(m_pCh1, m_hObj1, LWA06F_SUBCHANNEL_COPY_ENGINE,
                     m_copySurfSrc.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                     m_copySurfDest.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                     m_copySurfDest.GetSize()));

    CHECK_RC_CLEANUP(CePhysicalCopy(m_pCh1, m_hObj1, LWA06F_SUBCHANNEL_COPY_ENGINE,
                     m_copySurfSrc.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                     m_copySurfDest.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                     m_copySurfDest.GetSize()));

    }

    // Submit the blocking acquire again
    CHECK_RC_CLEANUP(submitBlockingAcquire(m_pCh1, 0x99dead99));

    CHECK_RC_CLEANUP(m_chGrp1.Schedule(true));
    CHECK_RC_CLEANUP(m_chGrp1.Flush());

    for (i=0; i < 1000; i++)
    {
       pSubdev->RegWr32(0x00002638, 0x1);
       Tasker::Sleep(1);
       if (bCtxswTimeoutCbFired)
          break;
    }
    // DONE: Wait for context switch timeout
    rc = POLLWRAP(&testForContextSwitchTimeout, NULL, 20000);
    if (OK != rc)
    {
        // TODO: may be if test fails try the test again - at least may be thrice till we get the interrupt
        failtest = LW_TRUE;
        Printf(Tee::PriHigh, "%s Test 5 FAILED: CTXSW_TIMEOUT_ERROR INTR missed\n %d",
                         __FUNCTION__, bCtxswTimeoutCbFired);
        MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0x99dead99);
    }
    else
    {
        Printf(Tee::PriHigh, "%s: Test 5 SUCCESS: CTXSW_TIMEOUT_ERROR INTR received\n", __FUNCTION__);
        // DONE: Trigger an engine reset
        CHECK_RC_CLEANUP(m_pCh1->RmcResetChannel(LW2080_ENGINE_TYPE_COPY0));

        ctxswParams.timeout = 0;
        ctxswParams.bEnable = LW_FALSE;
        rc = pLwRm->Control(hSubdev,
            LW2080_CTRL_CMD_FIFO_CONFIG_CTXSW_TIMEOUT,
            &ctxswParams, sizeof(ctxswParams));

        MASSERT(rc == OK);

        Printf(Tee::PriHigh, "%s: Test 6 Context switch timeout recovers with reset engine\n", __FUNCTION__);
        // DONE: Allocate a channel and sanity test and wait for idle and test the semaphore
        m_chGrp2.SetEngineId(LW2080_ENGINE_TYPE_COPY0);
        CHECK_RC_CLEANUP(m_chGrp2.Alloc());
        CHECK_RC_CLEANUP(m_chGrp2.AllocChannel(&m_pCh2));
        CHECK_RC_CLEANUP(AllocObject(m_pCh2, LWA06F_SUBCHANNEL_COPY_ENGINE, &m_hObj2));
        CHECK_RC_CLEANUP(m_chGrp2.Schedule(true));

        CHECK_RC_CLEANUP(m_pCh2->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, m_hObj2));
        CHECK_RC_CLEANUP(submitRelease(m_pCh2, 0x00dead00));
        CHECK_RC_CLEANUP(m_chGrp2.Flush());

        rc = m_chGrp2.WaitIdle();
        if (rc == OK)
        {
            if (MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10) != 0x00dead00) {
                rc = RC::DATA_MISMATCH;
                Printf(Tee::PriHigh, "%s: End Test 6: Failed datamismatch\n", __FUNCTION__);
            }
            else {
                Printf(Tee::PriHigh, "%s: End Test 6: Success\n", __FUNCTION__);
            }
        }
        else
        {
            failtest = LW_TRUE;
            Printf(Tee::PriHigh, "%s: End Test 6: Failed wfi timedout\n", __FUNCTION__);
            if (MEM_RD32((LwU8*)m_semaSurf.GetAddress() + 0x10) != 0x00dead00) {
                Printf(Tee::PriHigh, "%s: End Test 6: Reset engine unsuccessful\n", __FUNCTION__);
            }
        }

        FreeObjects(m_chGrp2.GetChannel(0));
        m_chGrp2.Free();
    }

Cleanup:

   MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0x99dead99);
   if (failtest == LW_TRUE)
   {
       rc.Clear();
       rc = RC::TIMEOUT_ERROR;
   }
   FreeObjects(m_chGrp1.GetChannel(0));
   m_chGrp1.Free();
   Printf(Tee::PriHigh, "%s:Turing QOS INTR testing %s\n", __FUNCTION__, (rc!=OK) ? "FAILED" : "PASSED");

   ErrorLogger::TestCompleted();

   return rc;
}
#endif

RC
TuringQosIntrTest::AcquireTimeoutLoggingTest()
{

    RC rc = OK;


    Printf(Tee::PriHigh, "%s: Begin Test 4  %s \n", __FUNCTION__, "TEST RUNLIST_ACQUIRE when ENG is not idle" );
    m_chGrp1.SetEngineId(LW2080_ENGINE_TYPE_COPY0);
    CHECK_RC_CLEANUP(m_chGrp1.Alloc());
    CHECK_RC_CLEANUP(m_chGrp1.AllocChannel(&m_pCh1));
    CHECK_RC_CLEANUP(AllocObject(m_pCh1, LWA06F_SUBCHANNEL_COPY_ENGINE, &m_hObj1));

    CHECK_RC_CLEANUP(m_pCh1->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, m_hObj1));

    CHECK_RC_CLEANUP(CePhysicalCopy(m_pCh1, m_hObj1, LWA06F_SUBCHANNEL_COPY_ENGINE,
                     m_copySurfSrc.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                     m_copySurfDest.GetVidMemOffset(), Memory::Fb, PHYSICAL,
                     0x4000));

    bRunlistBlockedEngIdleCbFired= false;
    bRunlistBlockedCbFired= false;

    // Submit the blocking acquire again
    CHECK_RC_CLEANUP(submitBlockingAcquire(m_pCh1, 0x99dead99));

    CHECK_RC_CLEANUP(m_chGrp1.Schedule(true));
    CHECK_RC_CLEANUP(m_chGrp1.Flush());


    rc = POLLWRAP(&testForRunlistBlkdIntr, NULL, 10000);


    if (OK != rc)
    {

        failtest = LW_TRUE;
        Printf(Tee::PriHigh, "%s Test FAILED: LW_PFIFO_INTR_0_RUNLIST_ACQUIRE INTR missed %d\n",
                         __FUNCTION__, bRunlistBlockedCbFired);
    }
    else
    {
        Printf(Tee::PriHigh, "%s Test SUCCESS: Verifed LW_PFIFO_INTR_0_RUNLIST_ACQUIRE when ENG not IDLE\n", __FUNCTION__);
    }

    MEM_WR32((LwU8*)m_semaSurf.GetAddress() + 0x10, 0x99dead99);

Cleanup:
   if (rc != OK)
      failtest = LW_TRUE;

   FreeObjects(m_chGrp1.GetChannel(0));
   m_chGrp1.Free();

   return OK;
}

RC TuringQosIntrTest::AllocObject(Channel *pCh, LwU32 subch, LwRm::Handle *hObj, LwU32 engineInstance)
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
        case LWA06F_SUBCHANNEL_COPY_ENGINE:
            if (engineInstance == (LwU32) -1)
                engineInstance = GetGrCopyInstance(pCh->GetClass());
            (void) engineInstance; // undefined reference
            CHECK_RC(m_ceAllocs[hCh].AllocOnEngine(hCh, LW2080_ENGINE_TYPE_COPY(0), GetBoundGpuDevice(), LwRmPtr().Get()));
            *hObj = m_ceAllocs[hCh].GetHandle();
            break;
        default:
            CHECK_RC(RC::SOFTWARE_ERROR);
    }
    return rc;
}

void TuringQosIntrTest::FreeObjects(Channel *pCh)
{
    LwRm::Handle hCh = pCh->GetHandle();

    if (m_ceAllocs.count(hCh)){ m_ceAllocs[hCh].Free(); m_ceAllocs.erase(hCh); }
    if (m_3dAllocs.count(hCh)){ m_3dAllocs[hCh].Free(); m_3dAllocs.erase(hCh); }
}

RC TuringQosIntrTest::CleanObjects()
{
    RC rc = OK;
    if (!m_ceAllocs.empty()){ m_ceAllocs.clear(); rc = -1; }
    if (!m_3dAllocs.empty()){ m_3dAllocs.clear(); rc.Clear(); rc = -1; }
    return rc;
}

LwU32 TuringQosIntrTest::GetGrCopyInstance(LwU32 parentClass)
{
    RC rc;
    LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS partnerParams = {0};

    partnerParams.engineType = LW2080_ENGINE_TYPE_GRAPHICS;
    partnerParams.partnershipClassId = parentClass;

    rc = pLwRm->Control(hSubdev,
            LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST,
            &partnerParams, sizeof(partnerParams));
    MASSERT(rc == OK);
    MASSERT(partnerParams.numPartners == 1);

    return partnerParams.partnerList[0] - LW2080_ENGINE_TYPE_COPY0;
}


JS_CLASS_INHERIT(TuringQosIntrTest, RmTest,
    "TuringQosIntrTest RMTEST that helps test qos interrupt functionality");
CLASS_PROP_READWRITE(TuringQosIntrTest, ForceTest, string,
        "Run specific test");

