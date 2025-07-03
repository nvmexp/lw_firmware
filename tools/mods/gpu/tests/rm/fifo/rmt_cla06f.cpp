/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_clA06f.cpp
//! \brief KEPLER_CHANNEL_GPFIFO_A schedule Test
//!

#include "lwos.h"
#include "core/include/jscript.h"
#include "core/include/channel.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C
#include "class/clb06f.h" // MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A
#include "ctrl/ctrla06f.h"
#include "ctrl/ctrl0080/ctrl0080fifo.h"
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "gpu/utility/surf2d.h"
#include "gpu/tests/rmtest.h"
#include "core/include/tasker.h"
#include "gpu/include/gralloc.h"
#include "core/include/memcheck.h"

#define SW_OBJ_SUBCH 5

static bool
pollForZero(void* pArgs)
{
    return MEM_RD32(pArgs) == 0;
}

class ClassA06fTest: public RmTest
{
public:
    ClassA06fTest();
    virtual ~ClassA06fTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

private:
    RC               HostScheduleTest(bool);
    RC               HostBindTest();
    RC               MaxChannelsTest();
    RC               NonStallingInterruptTest();
    RC               IlwalidSubchannelTest();
    RC               RunlistPreemptTest();
    RC               PbTimesliceTest();

    Surface2D        m_SemaSurf;
    UINT32 *         m_cpuAddr;

};

//!
//! \brief PollFunc
//!
//! Polling on the semaphore release condition, timeout condition
//! in case the sema isn't released.
//!
//! \return TRUE if the sema released else false.
//!
//! \sa Run
//-----------------------------------------------------------------------------
static bool PollFunc(void *pArg)
{
    UINT32 data = MEM_RD32(pArg);

    if(data == ((UINT32)0x12345678))
    {
        Printf(Tee::PriLow, "Sema exit Success \n");
        return true;
    }
    else
    {
        return false;
    }

}

//! \brief ClassA06fTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
ClassA06fTest::ClassA06fTest() :
    m_cpuAddr(NULL)
{
    SetName("ClassA06fTest");
}

//! \brief ClassA06fTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ClassA06fTest::~ClassA06fTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True if KEPLER_CHANNEL_GPFIFO_A is supported on the current chip and
//!         it is also supported in MODS as first class.
//-----------------------------------------------------------------------------
string ClassA06fTest::IsTestSupported()
{
    if( IsClassSupported(KEPLER_CHANNEL_GPFIFO_A) || IsClassSupported(KEPLER_CHANNEL_GPFIFO_B)  ||
        IsClassSupported(KEPLER_CHANNEL_GPFIFO_C) || IsClassSupported(MAXWELL_CHANNEL_GPFIFO_A) ||
        IsClassSupported(PASCAL_CHANNEL_GPFIFO_A) || IsClassSupported(VOLTA_CHANNEL_GPFIFO_A)   ||
        IsClassSupported(TURING_CHANNEL_GPFIFO_A) || IsClassSupported(AMPERE_CHANNEL_GPFIFO_A)  ||
        IsClassSupported(HOPPER_CHANNEL_GPFIFO_A))
        return RUN_RMTEST_TRUE;
    return "KEPLER_CHANNEL_GPFIFO_A, KEPLER_CHANNEL_GPFIFO_B, KEPLER_CHANNEL_GPFIFO_C, MAXWELL_CHANNEL_GPFIFO_A, PASCAL_CHANNEL_GPFIFO_A, VOLTA_CHANNEL_GPFIFO_A, TURING_CHANNEL_GPFIFO_A, AMPERE_CHANNEL_GPFIFO_A, HOPPER_CHANNEL_GPFIFO_A classes are not supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC ClassA06fTest::Setup()
{
    const UINT32 memSize = 0x1000;
    RC           rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    m_TestConfig.SetAllowMultipleChannels(true);

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    Printf(Tee::PriHigh, "%s: Allocated channel class 0x%x\n", __FUNCTION__, m_pCh->GetClass());

    m_SemaSurf.SetForceSizeAlloc(true);
    m_SemaSurf.SetArrayPitch(1);
    m_SemaSurf.SetArraySize(memSize);
    m_SemaSurf.SetColorFormat(ColorUtils::VOID32);
    m_SemaSurf.SetAddressModel(Memory::Paging);
    m_SemaSurf.SetLayout(Surface2D::Pitch);
    m_SemaSurf.SetLocation(Memory::NonCoherent);

    CHECK_RC(m_SemaSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SemaSurf.Map());

    m_SemaSurf.Fill(0);

    m_cpuAddr = (UINT32*)m_SemaSurf.GetAddress();

    return rc;
}

//! \brief Run() : The body of this test.
//!
//! In this function, it calls several  sub-tests.
//!
//! \return RC -> OK if the test is run successfully, test-specific RC if the
//!               test failed
//------------------------------------------------------------------------------
RC ClassA06fTest::Run()
{
    RC rc;

    // We should be able to free a channel no matter it is scheduled or not
    // CHECK_RC(m_TestConfig.FreeChannel(m_pCh));

    CHECK_RC(HostScheduleTest(false));

    CHECK_RC(m_TestConfig.FreeChannel(m_pCh));
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC(m_SemaSurf.BindGpuChannel(m_hCh));

    CHECK_RC(HostScheduleTest(true));

    CHECK_RC(m_TestConfig.FreeChannel(m_pCh));

    CHECK_RC(HostBindTest());

    //
    // This test can take hours if not days on simulation so only running on
    // hardware.
    //
    if(Platform::GetSimulationMode() == Platform::Hardware)
        CHECK_RC(MaxChannelsTest());

    CHECK_RC(NonStallingInterruptTest());

    CHECK_RC(RunlistPreemptTest());

    CHECK_RC(IlwalidSubchannelTest());

    CHECK_RC(PbTimesliceTest());

    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------

RC ClassA06fTest::HostScheduleTest(bool bEnable)
{
    RC rc = OK;
    LwRmPtr pLwRm;
    const UINT32 semExp = 0x12345678;
    UINT32  status = 0;
    LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};

    gpFifoSchedulParams.bEnable = true;

    // Set the semaphore location to zeros
    MEM_WR32( ((char *)m_cpuAddr + 0x0), 0x00000000);

    if (bEnable == false)
    {
        gpFifoSchedulParams.bEnable = false;

        // Schedule channel first before
        status = pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                    &gpFifoSchedulParams, sizeof(gpFifoSchedulParams));

        if (status != LW_OK)
        {
            Printf(Tee::PriHigh,
                "ClassA06fTest: failed to schedule channel, err:0x%x!\n", status);
            return RC::SOFTWARE_ERROR;
        }

        gpFifoSchedulParams.bEnable = true;

        status = pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                    &gpFifoSchedulParams, sizeof(gpFifoSchedulParams));
    }
    else {
        status = pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                    &gpFifoSchedulParams, sizeof(gpFifoSchedulParams));
    }

    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "ClassA06fTest: failed to schedule channel, err:0x%x!\n", status);
        return RC::SOFTWARE_ERROR;
    }

    m_pCh->SetAutoSchedule(false);

    m_pCh->SetSemaphoreOffset(m_SemaSurf.GetCtxDmaOffsetGpu());
    m_pCh->SemaphoreRelease(semExp);

    m_pCh->Flush();

    // poll on event notification: semaphore release
    CHECK_RC(POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs()));

    Printf(Tee::PriHigh,
        "ClassA06fTest: HostScheduleTest(%s) test passed!!\n", (bEnable?"true":"false"));

    return rc;
}

RC ClassA06fTest::HostBindTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LWA06F_CTRL_BIND_PARAMS bindParams = {0};
    LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS enginesParams =  {0};
    const UINT32 semExp = 0x12345678;
    LwRm::Handle hSwObj = 0;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                &enginesParams, sizeof(enginesParams)));

    for (LwU32 i = 0; i < enginesParams.engineCount; ++i)
    {
        LwU32 engine = enginesParams.engineList[i];
        Printf(Tee::PriLow, "%s: Testing engine %d\n", __FUNCTION__, engine);

        CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, engine));
        CHECK_RC_CLEANUP(m_SemaSurf.BindGpuChannel(m_hCh));

        bindParams.engineType = engine;
        CHECK_RC_CLEANUP(pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_BIND,
                    &bindParams, sizeof(bindParams)));

        gpFifoSchedulParams.bEnable = true;

        CHECK_RC_CLEANUP(pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                    &gpFifoSchedulParams, sizeof(gpFifoSchedulParams)));

        CHECK_RC_CLEANUP(pLwRm->Alloc(m_hCh, &hSwObj, LW04_SOFTWARE_TEST, NULL));

        MEM_WR32(((char *)m_cpuAddr + 0x0), 0x00000000);

        m_pCh->SetObject(SW_OBJ_SUBCH, hSwObj);
        m_pCh->SetSemaphoreOffset(m_SemaSurf.GetCtxDmaOffsetGpu());
        m_pCh->SemaphoreRelease(semExp);
        m_pCh->Flush();

        CHECK_RC_CLEANUP(POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs()));

        pLwRm->Free(hSwObj);
        hSwObj = 0;
        m_TestConfig.FreeChannel(m_pCh);
        m_pCh = NULL;
    }

    Printf(Tee::PriLow, "%s Passed\n", __FUNCTION__);
Cleanup:
    if (hSwObj)
        pLwRm->Free(hSwObj);
    if (m_pCh)
        m_TestConfig.FreeChannel(m_pCh);

    return rc;
}

RC ClassA06fTest::MaxChannelsTest()
{
    LwRmPtr pLwRm;
    RC rc;
    vector<Channel *> channels;
    LwRm::Handle hCh;
    Channel *pCh;
    LwU32 channelSize = m_TestConfig.ChannelSize();
    LwRm::Handle hObj007d;
    Surface2D *pSemaSurf = NULL;
    LwU32 offset;
    LwU32 i = 0;
    LW2080_CTRL_FIFO_GET_PHYSICAL_CHANNEL_COUNT_PARAMS params;

    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                LW2080_CTRL_CMD_FIFO_GET_PHYSICAL_CHANNEL_COUNT,
                &params, sizeof(params)));

    m_TestConfig.SetChannelSize(1024);

    while (i < (params.physChannelCount - params.physChannelCountInUse))
    {
        CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&pCh, &hCh, LW2080_ENGINE_TYPE_GR(0)));
        channels.push_back(pCh);
        i++;
    }

    Printf (Tee::PriLow, "%s: Successfully allocated %d channels\n",
            __FUNCTION__, (int)channels.size());

    // Now that we have all the channels, allocate semaphore memory for all of them.
    pSemaSurf = new Surface2D;
    pSemaSurf->SetForceSizeAlloc(true);
    pSemaSurf->SetArrayPitch(1);
    pSemaSurf->SetArraySize(4 * (LwU32) channels.size());
    pSemaSurf->SetColorFormat(ColorUtils::VOID32);
    pSemaSurf->SetAddressModel(Memory::Paging);
    pSemaSurf->SetLayout(Surface2D::Pitch);
    pSemaSurf->SetLocation(Memory::NonCoherent);
    CHECK_RC_CLEANUP(pSemaSurf->Alloc(GetBoundGpuDevice()));
    CHECK_RC_CLEANUP(pSemaSurf->Map());
    pSemaSurf->Fill(0);

    for (i = 0; i < channels.size(); ++i, pCh = channels[i])
    {
        offset = i*4;
        pSemaSurf->BindGpuChannel(pCh->GetHandle());

        MEM_WR32((LwU32*)pSemaSurf->GetAddress() + i, 0xdeadbeef);

        CHECK_RC_CLEANUP(pLwRm->Alloc(pCh->GetHandle(), &hObj007d, LW04_SOFTWARE_TEST, NULL));
        CHECK_RC_CLEANUP(pCh->SetObject(SW_OBJ_SUBCH, hObj007d));
        CHECK_RC_CLEANUP(pCh->SetSemaphoreOffset(pSemaSurf->GetCtxDmaOffsetGpu() + offset));
        pCh->SetSemaphoreReleaseFlags(0); // no time
        pCh->SetSemaphorePayloadSize(Channel::SEM_PAYLOAD_SIZE_32BIT);
        CHECK_RC_CLEANUP(pCh->SemaphoreRelease(0));
        CHECK_RC_CLEANUP(pCh->Flush());

        if (i && (i % 100) == 0)
        {
            Printf (Tee::PriLow, "%s: Semaphores written for channels 0-%d\n",
                     __FUNCTION__, i);
        }
    }

    for (i = 0; i < channels.size(); ++i, pCh = channels[i])
    {
        offset = i;
        CHECK_RC_CLEANUP(POLLWRAP(pollForZero,
                                  (LwU32*)pSemaSurf->GetAddress() + offset,
                                  m_TestConfig.TimeoutMs()));
        if (i && (i % 100) == 0)
        {
            Printf (Tee::PriLow, "%s: Semaphores released for channels 0-%d\n",
                     __FUNCTION__, i);
        }
    }

Cleanup:
    Printf (Tee::PriLow, "Freeing channels\n");
    while (!channels.empty())
    {

        m_TestConfig.FreeChannel(channels.back());
        channels.pop_back();
    }

    m_TestConfig.SetChannelSize(channelSize);
    if (pSemaSurf)
    {
        pSemaSurf->Free();
        delete pSemaSurf;
    }
    if (rc == OK)
    {
        Printf(Tee::PriLow, "%s passed\n", __FUNCTION__);
    }
    else
    {
        // Remove this message if bug 3138070 is fixed
        Printf(Tee::PriNormal, "%s Please try -sysmem_page_size 64 due to bug 3105420\n", __FUNCTION__);
    }

    return rc;
}

//------------------------------------------------------------------------------
// Used to allow test to ignore nonblocking interrupts.
static bool ErrorLogFilter(const char *errMsg)
{
    const char * patterns[] =
    {
         "LWRM: processing * nonstall interrupt\n"
    };
    for (size_t i = 0; i < NUMELEMS(patterns); i++)
    {
        if (Utility::MatchWildCard(errMsg, patterns[i]))
        {
            Printf(Tee::PriHigh,
                "Ignoring error message: %s\n",
                errMsg);
            return false;
        }
    }
    return true; // log this error, it's not one of the filtered items.
}

RC ClassA06fTest::NonStallingInterruptTest()
{
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS eventParams = {0};
    LwRmPtr pLwRm;
    RC rc;
    LwRm::Handle hSwObj = 0;
    LwRm::Handle hNotifyEvent = 0;
    ModsEvent *pNotifyEvent=NULL;
    void *pEventAddr=NULL;

    //
    // Turn off unexpected interrupt checking because nonblocking interrupts
    // may be mis-attributed
    //
    ErrorLogger::StartingTest();
    ErrorLogger::InstallErrorLogFilterForThisTest(ErrorLogFilter);

    pNotifyEvent = Tasker::AllocEvent();
    pEventAddr = Tasker::GetOsEvent(
            pNotifyEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    //Associcate Event to Object
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

    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC_CLEANUP(pLwRm->Alloc(m_hCh, &hSwObj, LW04_SOFTWARE_TEST, NULL));

    CHECK_RC_CLEANUP(m_pCh->SetObject(SW_OBJ_SUBCH, hSwObj));
    CHECK_RC_CLEANUP(m_pCh->NonStallInterrupt());
    CHECK_RC_CLEANUP(m_pCh->Flush());

    CHECK_RC_CLEANUP(Tasker::WaitOnEvent(pNotifyEvent, m_TestConfig.TimeoutMs()));

    Printf(Tee::PriLow, "%s Pass\n", __FUNCTION__);

Cleanup:
    if (hSwObj)
        pLwRm->Free(hSwObj);

    m_TestConfig.FreeChannel(m_pCh);

    if (hNotifyEvent)
        pLwRm->Free(hNotifyEvent);

    if(pNotifyEvent != NULL)
        Tasker::FreeEvent(pNotifyEvent);

    ErrorLogger::TestCompleted();

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC ClassA06fTest::Cleanup()
{

    m_SemaSurf.Free();

    return OK;
}

// Tests sending a hw engine setObject on a sw subchannel
RC ClassA06fTest::IlwalidSubchannelTest()
{
    LwRmPtr        pLwRm;
    RC             rc;
    UINT32         ceEngineId = LW2080_ENGINE_TYPE_NULL;
    DmaCopyAlloc   ceAlloc;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams = {0};

    // Get a list of supported engines
    CHECK_RC_CLEANUP( pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                     LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                                     &engineParams,
                                     sizeof (engineParams)));

    // Find the first CE instance available on this chip
    for (unsigned int i = 0; i < engineParams.engineCount; i++)
    {
        if (LW2080_ENGINE_TYPE_IS_COPY(engineParams.engineList[i]))
        {
            ceEngineId = engineParams.engineList[i];
            break;
        }
    }

    if (ceEngineId == LW2080_ENGINE_TYPE_NULL)
    {
        Printf(Tee::PriLow, "No CE instances supported on this chip, skipping test %s\n", __FUNCTION__);
        return rc;
    }

    Printf(Tee::PriLow,
           "%s: Running test on CE%d\n",
           __FUNCTION__,
           LW2080_ENGINE_TYPE_COPY_IDX(ceEngineId));
    CHECK_RC(StartRCTest());

    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, ceEngineId));
    CHECK_RC_CLEANUP(ceAlloc.AllocOnEngine(m_hCh,
                                           ceEngineId,
                                           GetBoundGpuDevice(),
                                           LwRmPtr().Get()));
    // Issue back-to-back RC errors, also tests all
    CHECK_RC_CLEANUP(m_pCh->SetObject(5, ceAlloc.GetHandle()));
    CHECK_RC_CLEANUP(m_pCh->SetObject(6, ceAlloc.GetHandle()));
    CHECK_RC_CLEANUP(m_pCh->SetObject(7, ceAlloc.GetHandle()));
    CHECK_RC_CLEANUP(m_pCh->Flush());
    rc = m_pCh->WaitIdle(m_TestConfig.TimeoutMs());

    if (rc == RC::RM_RCH_PBDMA_ERROR)
        rc.Clear();
    else
        CHECK_RC_CLEANUP(rc);

    m_pCh->ClearError();

Cleanup:
    ceAlloc.Free();
    m_TestConfig.FreeChannel(m_pCh);

    EndRCTest();
    return rc;
}

RC ClassA06fTest::RunlistPreemptTest()
{
    RC rc;
    LwRmPtr pLwRm;
    LWA06F_CTRL_BIND_PARAMS bindParams = {0};
    LWA06F_CTRL_GPFIFO_SCHEDULE_PARAMS gpFifoSchedulParams = {0};
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS enginesParams =  {0};
    LW0080_CTRL_FIFO_STOP_RUNLIST_PARAMS stopRunlistParams = {0};
    LW0080_CTRL_FIFO_START_RUNLIST_PARAMS startRunlistParams = {0};
    const UINT32 semExp = 0x12345678;

    Printf(Tee::PriLow, "%s Running\n", __FUNCTION__);
    CHECK_RC(pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()), LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
                &enginesParams, sizeof(enginesParams)));

    for (LwU32 i = 0; i < enginesParams.engineCount; ++i)
    {
        LwU32 engine = enginesParams.engineList[i];
        Printf(Tee::PriLow, "%s: Testing engine %d\n", __FUNCTION__, engine);

        CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, engine));
        CHECK_RC_CLEANUP(m_SemaSurf.BindGpuChannel(m_hCh));

        bindParams.engineType = engine;
        CHECK_RC_CLEANUP(pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_BIND,
                    &bindParams, sizeof(bindParams)));

        gpFifoSchedulParams.bEnable = true;

        CHECK_RC_CLEANUP(pLwRm->Control(m_pCh->GetHandle(), LWA06F_CTRL_CMD_GPFIFO_SCHEDULE,
                    &gpFifoSchedulParams, sizeof(gpFifoSchedulParams)));

        MEM_WR32(((char *)m_cpuAddr + 0x0), 0x00000000);

        m_pCh->SetSemaphoreOffset(m_SemaSurf.GetCtxDmaOffsetGpu());
        m_pCh->SemaphoreRelease(semExp);
        m_pCh->SemaphoreAcquire(0);
        m_pCh->SemaphoreRelease(semExp);
        m_pCh->Flush();

        CHECK_RC_CLEANUP(POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs()));
        stopRunlistParams.engineID = engine;

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()), LW0080_CTRL_CMD_FIFO_STOP_RUNLIST,
                    &stopRunlistParams, sizeof(stopRunlistParams)));

        MEM_WR32(((char *)m_cpuAddr + 0x0), 0x00000000);

        //
        // TODO we should detect that the channel isn't running until the runlist is restarted.
        //

        startRunlistParams.engineID = engine;

        CHECK_RC_CLEANUP(pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()), LW0080_CTRL_CMD_FIFO_START_RUNLIST,
                    &startRunlistParams, sizeof(startRunlistParams)));

        // Now the channel should complete.
        CHECK_RC_CLEANUP(POLLWRAP(&PollFunc, m_cpuAddr, m_TestConfig.TimeoutMs()));

        m_TestConfig.FreeChannel(m_pCh);
        m_pCh = NULL;
    }

    Printf(Tee::PriLow, "%s Passed\n", __FUNCTION__);
Cleanup:
    if (m_pCh)
        m_TestConfig.FreeChannel(m_pCh);

    return rc;
}

//! \brief Make sure PB timeslice ctrls don't work on Maxwell
RC ClassA06fTest::PbTimesliceTest()
{
    LwRmPtr pLwRm;
    RC rc;
    LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PARAMS params = {0};
    // RM breaks on failure, disable BPs for negative testing.
    Platform::DisableBreakPoints nobp;

    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));

    // Make sure this call succeeds on Kepler and Fails on Maxwell
    params.hChannel = m_pCh->GetHandle();
    params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PBDMATIMESLICEINMICROSECONDS;
    params.value    = 0x100ULL;
    rc = pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                            &params,
                            sizeof(params));

    switch (GetBoundGpuDevice()->GetFamily())
    {
        case GpuDevice::Kepler:
            CHECK_RC_CLEANUP(rc);
            break;

        default:
        rc.Clear();
    }

    // Make sure this call succeeds on Kepler and Fails on Maxwell
    params.hChannel = m_pCh->GetHandle();
    params.property = LW0080_CTRL_FIFO_SET_CHANNEL_PROPERTIES_PBDMATIMESLICEDISABLE;
    rc = pLwRm->Control(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                            LW0080_CTRL_CMD_FIFO_SET_CHANNEL_PROPERTIES,
                            &params,
                            sizeof(params));

    switch (GetBoundGpuDevice()->GetFamily())
    {
        case GpuDevice::Kepler:
            CHECK_RC(rc);
            break;

        default:
        rc.Clear();
    }
Cleanup:
    m_TestConfig.FreeChannel(m_pCh);
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ClassA06fTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ClassA06fTest, RmTest,
                 "ClassA06fTest RM test.");
