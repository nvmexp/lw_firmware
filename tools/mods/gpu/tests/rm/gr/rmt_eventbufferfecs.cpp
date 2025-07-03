/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_eventbufferfecs.cpp
//! \brief Test for covering rm event buffer functionality with FECS
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"

#include "core/include/xp.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/massert.h"
#include "gpu/tests/rm/utility/dtiutils.h"

#include <string>
#include "core/include/memcheck.h"

#include "class/cl2080.h"
#include "class/cl90cd.h"
#include "class/cl90cdtypes.h"
#include "class/cl90cdfecs.h"
#include "ctrl/ctrl90cd.h"

#include "class/clc06f.h"
#include "class/cla06f.h"
#include "class/cla06fsubch.h"
#include "class/cla16f.h"
#include "class/cla26f.h"
#include "class/clb06f.h"
#include "ctrl/ctrl90cc.h"
#include "ctrl/ctrl2080.h"
#include "gpu/include/gralloc.h"
#include "core/include/threadpool.h"

#define RECORD_SIZE                       32 // size of record header plus our payload
#define RECORD_COUNT                      500
#define RECORD_BUFFER_SIZE                (RECORD_SIZE * RECORD_COUNT)
#define VARDATA_BUFFER_SIZE               6400
#define LW_EVENT_BUFFER_FECS_CTXSWTAG_ALL 63
#define NUM_NOP_PUSH_TIMES                10
#define NUM_YIELD_NOP_PUSH_ITERATIONS     100000
#define NUM_YIELD_EVENT_BUFFER_ITERATIONS 500000 // usually 500k
#define NUM_TEST_CHANNELS (4) // Avoid 64 or more otherwise system might run out of memory
#define NUM_NOPS          (64)
#define LW_ERR_ILWALID_ARGUMENT 0x2a7 // LWOS_STATUS_ERROR_ILWALID_ARGUMENT fails to work for some reason

class PushNopsTask
{
public:
    PushNopsTask(Channel *pCh[NUM_TEST_CHANNELS],
        LwRm::Handle hCh[NUM_TEST_CHANNELS],
        LwRm::Handle hComputeObj[NUM_TEST_CHANNELS]) :
        pCh(pCh), hCh(hCh), hComputeObj(hComputeObj)
    {
    }

    RC operator()()
    {
        return RunFecs();
    }

private:
    RC RunFecs();
    Channel       **pCh;
    LwRm::Handle   *hCh;
    LwRm::Handle   *hComputeObj;
};

class EventBufferFecsTest : public RmTest
{
public:
    EventBufferFecsTest();
    virtual ~EventBufferFecsTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC AllocZeroBuffer();
    RC AllocHugeBuffer();
    RC AllocBuffer(LwBool fecsOnly);
    RC BindEvent();
    RC UpdateGet(LwU32 recordBufferGet, LwU32 vardataBufferGet);
    RC PerformBadUpdateGets();
    RC PerformBadUpdateGetsNoVardataBuffer();
    RC EnableEvents();
    RC DumpBufferContents();
    RC FlushBufferContents();
    RC RestartFecsEvent();
    RC CleanFecs();

    LwRmPtr pLwRm;
    LwRm::Handle                      bufferHandle        = 0;
    LwRm::Handle                      hNotifyEvent        = 0;
    ModsEvent                        *pNotifyEvent        = nullptr;
    LwP64                             pRecordBuffer       = 0;
    LwP64                             pVardataBuffer      = 0;
    LW_EVENT_BUFFER_HEADER           *pRecordBufferHeader = nullptr;
    PushNopsTask                     *pPushNopsTask       = nullptr;
    Tasker::ThreadPool<PushNopsTask> *pThreadPool         = nullptr;
    LwBool                            isOnSecondFecsBind  = 0;

    LwRm::Handle fecsBindHandle                           = 0;
    LwRm::Handle rcBindHandle                             = 0;

    Channel      *pCh[NUM_TEST_CHANNELS]                  = {};
    LwRm::Handle  hCh[NUM_TEST_CHANNELS]                  = {};
    LwRm::Handle  hComputeObj[NUM_TEST_CHANNELS]          = {};
};

RC PushNopsTask::RunFecs()
{
    RC rc;
    LwRmPtr pLwRm;
    LwU32 ch;
    LwU32 nops;
    LwU32 i;
    LwU32 j;

    for (i = 0; i < NUM_NOP_PUSH_TIMES; ++i)
    {
        // push no-ops to all allocated channels
        for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
        {
            CHECK_RC(pCh[ch]->SetObject(LWA06F_SUBCHANNEL_COMPUTE, hComputeObj[ch]));
            for (nops = 0; nops < NUM_NOPS; nops++)
            {
                CHECK_RC(pCh[ch]->WriteNop()); // TODO is this right? should it be a write to the subchannel?
            }
        }

        // launch the channels by flushing them
        for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
        {
            CHECK_RC(pCh[ch]->Flush());
        }

        for (j = 0; j < NUM_YIELD_NOP_PUSH_ITERATIONS; ++j)
        {
            Tasker::Yield();
        }
    }

    return rc;
}

//! \brief EventBufferFecsTest basic constructor
//!
//! This is just the basic constructor for the EventBufferFecsTest class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
EventBufferFecsTest::EventBufferFecsTest()
{
    SetName("EventBufferFecsTest");
}

//! \brief EventBufferFecsTest destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
EventBufferFecsTest::~EventBufferFecsTest()
{
}

//! \brief IsSupported
//!
//! This test is supported on all elwironments.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string EventBufferFecsTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup entry point
//!
//! Allocate hClient (LW01_ROOT) object.
//!
//! \return OK if resource allocations succeed, error code indicating
//!         nature of failure otherwise
//-----------------------------------------------------------------------------
RC EventBufferFecsTest::Setup()
{
    RC rc;
    LwU32 ch;
    LwRmPtr pLwRm;
    ComputeAlloc computeAllocator;

    CHECK_RC(InitFromJs());

    // enable multiple channels
    m_TestConfig.SetAllowMultipleChannels(true);

    // allocate all the channels and a compute object for each channel.
    // we arbitrarily choose compute objects to push no-ops to, however
    // any object that uses the GR engines will work. Note that there is
    // a dedicated subchannel for each class for kepler GPUs onward.
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        pCh[ch] = NULL;
        CHECK_RC(m_TestConfig.AllocateChannel(&pCh[ch], &hCh[ch], LW2080_ENGINE_TYPE_GR(0)));
        CHECK_RC(computeAllocator.Alloc(hCh[ch], GetBoundGpuDevice()));
        hComputeObj[ch] = computeAllocator.GetHandle();
    }

    pPushNopsTask = new PushNopsTask(pCh, hCh, hComputeObj);
    pThreadPool = new Tasker::ThreadPool<PushNopsTask>();
    pThreadPool->Initialize(9, true);
    pThreadPool->EnqueueTask(*pPushNopsTask);

    return rc;
}

//! \brief Run entry point
//!
//! Kick off system control command tests.
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC EventBufferFecsTest::Run()
{
    RC    rc = OK;
    LwU32 i;

    isOnSecondFecsBind = LW_FALSE;
    CHECK_RC(AllocBuffer(LW_FALSE));
    CHECK_RC(BindEvent());
    CHECK_RC(EnableEvents());
    
    // These two tests pass only if the allocation fails on RM side
    // and returns LW_ERR_ILWALID_ARGUMENT
    CHECK_RC(AllocZeroBuffer());
    CHECK_RC(AllocHugeBuffer());

    for (i = 0; i < NUM_YIELD_EVENT_BUFFER_ITERATIONS; i++)
    {
        Tasker::Yield();
    }
    CHECK_RC(DumpBufferContents());

    // If the count is set to a small number, this call should allow for
    // new records to be written to the first few indices, i.e. 0 to 10
    for (i = 0; i < NUM_YIELD_EVENT_BUFFER_ITERATIONS / 3; i++)
    {
        Tasker::Yield();
    }
    if (RECORD_COUNT >= 3)
    {
        // small wrap-around test for DumpBufferContents itself
        CHECK_RC(UpdateGet(RECORD_COUNT - 3, 0));
    }
    CHECK_RC(DumpBufferContents());

    // Update Get mini stress test
    for (i = 0; i < RECORD_COUNT; i++)
    {
        CHECK_RC(UpdateGet(i, 0));
    }

    for (i = 0; i < VARDATA_BUFFER_SIZE; i++)
    {
        CHECK_RC(UpdateGet(0, VARDATA_BUFFER_SIZE - i - 1));
    }

    CHECK_RC(UpdateGet(RECORD_COUNT - 1, 0));
    CHECK_RC(UpdateGet(0, VARDATA_BUFFER_SIZE - 1));
    CHECK_RC(UpdateGet(RECORD_COUNT - 1, VARDATA_BUFFER_SIZE - 1));
    CHECK_RC(UpdateGet(RECORD_COUNT / 2, VARDATA_BUFFER_SIZE / 2));
    CHECK_RC(PerformBadUpdateGets());

    // Second test run for FECS, where the bind is unbound and then rebound.
    // vardataBuffer is not initialized this time, so any non-zero value
    // passed to the second parameter of UpdateGet should fail.
    CHECK_RC(RestartFecsEvent());
    CHECK_RC(PerformBadUpdateGetsNoVardataBuffer());

    CHECK_RC(UpdateGet(0, 0));
    for (i = 0; i < NUM_YIELD_EVENT_BUFFER_ITERATIONS; i++)
    {
        Tasker::Yield();
    }
    CHECK_RC(DumpBufferContents());

    // Cleanup
    CHECK_RC(CleanFecs());

    return rc;
}

//! \brief Cleanup entry point
//!
//! Release outstanding resource.
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC EventBufferFecsTest::Cleanup()
{
    RC rc;

    pLwRm->Free(fecsBindHandle);
    pLwRm->Free(bufferHandle);

    // Only free RC handle if RestartFecsEvent has not been called
    if (!isOnSecondFecsBind)
        pLwRm->Free(rcBindHandle);

    if (pNotifyEvent != NULL)
        Tasker::FreeEvent(pNotifyEvent);

    delete pThreadPool;
    delete pPushNopsTask;

    return OK;
}

RC EventBufferFecsTest::AllocBuffer(LwBool fecsOnly)
{
    RC rc;
    LW_EVENT_BUFFER_ALLOC_PARAMETERS params = { 0 };
    UINT32 status = 0;
    void *pEventAddr = NULL;
    pNotifyEvent = NULL;

    // Allocate an OS event handle
    pNotifyEvent = Tasker::AllocEvent();

    pEventAddr = Tasker::GetOsEvent(
            pNotifyEvent,
            pLwRm->GetClientHandle(),
            pLwRm->GetDeviceHandle(GetBoundGpuDevice()));

    // Allocate Event Buffer
    params.recordSize = RECORD_SIZE;
    params.recordCount = RECORD_COUNT;
    params.recordsFreeThreshold = RECORD_COUNT / 2;
    if (!fecsOnly)
    {
        params.vardataBufferSize = VARDATA_BUFFER_SIZE;
        params.vardataFreeThreshold = VARDATA_BUFFER_SIZE / 2;
    }
    params.hSubDevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    params.notificationHandle = (LwU64)pEventAddr;

    status = pLwRm->Alloc(pLwRm->GetClientHandle(),
        &bufferHandle,
        LW_EVENT_BUFFER, &params);

    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
            "Event buffer object allocation failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    pRecordBuffer = params.recordBuffer;
    pVardataBuffer = params.vardataBuffer;
    pRecordBufferHeader = (LW_EVENT_BUFFER_HEADER*)params.bufferHeader;

    return OK;
}

RC EventBufferFecsTest::AllocZeroBuffer()
{
    RC rc = OK;
    LW_EVENT_BUFFER_ALLOC_PARAMETERS params = { 0 };
    UINT32 status = 0;
    LwRm::Handle bogusHandle;

    // Allocate Event Buffer
    params.hSubDevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    status = pLwRm->Alloc(pLwRm->GetClientHandle(),
        &bogusHandle,
        LW_EVENT_BUFFER, &params);

    if (status == LWOS_STATUS_SUCCESS)
    {
        rc = 1;
    }
    else if (status != LW_ERR_ILWALID_ARGUMENT)
    {
        Printf(Tee::PriNormal,
            "Event buffer object allocation failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    return rc;
}

RC EventBufferFecsTest::AllocHugeBuffer()
{
    RC rc = OK;
    LW_EVENT_BUFFER_ALLOC_PARAMETERS params = { 0 };
    UINT32 status = 0;
    LwRm::Handle bogusHandle;

    // Allocate Event Buffer
    params.recordSize = 1 << 10;
    params.recordCount = 1 << 30;
    params.recordsFreeThreshold = 1 << 29;
    params.hSubDevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    status = pLwRm->Alloc(pLwRm->GetClientHandle(),
        &bogusHandle,
        LW_EVENT_BUFFER, &params);

    if (status == LWOS_STATUS_SUCCESS)
    {
        rc = 1;
    }
    else if (status != LW_ERR_ILWALID_ARGUMENT)
    {
        Printf(Tee::PriNormal,
            "Event buffer object allocation failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    return rc;
}

RC EventBufferFecsTest::BindEvent()
{
    RC rc;
    LW2080_CTRL_GR_FECS_BIND_EVTBUF_FOR_UID_V2_PARAMS uidParams = { 0 };
    UINT32 status = 0;

    // Create FECS bind points
    uidParams.hEventBuffer = bufferHandle;
    uidParams.bAllUsers = false;
    uidParams.levelOfDetail = LW2080_CTRL_GR_FECS_BIND_EVTBUF_LOD_FULL;

    status = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_GR_FECS_BIND_EVTBUF_FOR_UID_V2,
                            &uidParams, sizeof(uidParams));
    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
            "First FECS binding event buffer failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    return OK;
}

RC EventBufferFecsTest::EnableEvents()
{
    RC rc;
    LW_EVENT_BUFFER_CTRL_CMD_ENABLE_EVENTS_PARAMS enableParams = { 0 };
    enableParams.enable = LW_TRUE;
    enableParams.flags = LW_EVENT_BUFFER_FLAG_OVERFLOW_POLICY_KEEP_OLDEST;
    UINT32 status;

    status = pLwRm->Control(bufferHandle, LW_EVENT_BUFFER_CTRL_CMD_ENABLE_EVENTS,
        &enableParams, sizeof(enableParams));
    if (status != LWOS_STATUS_SUCCESS)
    {
        return RmApiStatusToModsRC(status);
    }
    return OK;
}

RC EventBufferFecsTest::UpdateGet(LwU32 recordBufferGet, LwU32 vardataBufferGet)
{
    RC rc;
    LW_EVENT_BUFFER_CTRL_CMD_UPDATE_GET_PARAMS updateParams = { 0 };
    updateParams.recordBufferGet = recordBufferGet;
    updateParams.varDataBufferGet = vardataBufferGet;
    UINT32 status;

    status = pLwRm->Control(bufferHandle, LW_EVENT_BUFFER_CTRL_CMD_UPDATE_GET,
        &updateParams, sizeof(updateParams));
    if (status != LWOS_STATUS_SUCCESS)
    {
        return RmApiStatusToModsRC(status);
    }
    return OK;
}

RC EventBufferFecsTest::PerformBadUpdateGets()
{
    // TODO: separate these tests into different cases for easier debugging
    // These cases should not pass
    if (UpdateGet(RECORD_COUNT * 2, VARDATA_BUFFER_SIZE / 2) == OK ||
        UpdateGet(RECORD_COUNT / 2, VARDATA_BUFFER_SIZE * 2) == OK ||
        UpdateGet(RECORD_COUNT, VARDATA_BUFFER_SIZE / 2) == OK ||
        UpdateGet(RECORD_COUNT / 2, VARDATA_BUFFER_SIZE) == OK)
    {
        return RC::LWRM_ILWALID_ARGUMENT;
    }

    return OK;
}

RC EventBufferFecsTest::PerformBadUpdateGetsNoVardataBuffer()
{
    // These cases should not pass, either
    if (UpdateGet(RECORD_COUNT * 2, VARDATA_BUFFER_SIZE / 2) == OK ||
        UpdateGet(RECORD_COUNT / 2, 1) == OK ||
        UpdateGet(0, 1) == OK)
    {
        return RC::LWRM_ILWALID_ARGUMENT;
    }

    return OK;
}

RC EventBufferFecsTest::FlushBufferContents()
{
    UINT32 status;

    status = pLwRm->Control(bufferHandle, LW_EVENT_BUFFER_CTRL_CMD_FLUSH,
        NULL, 0);
    if (status != LWOS_STATUS_SUCCESS)
    {
        return RmApiStatusToModsRC(status);
    }
    return OK;
}

RC EventBufferFecsTest::DumpBufferContents()
{
    RC rc;
    LwU32 recordGet;
    LwU32 recordPut;

    FlushBufferContents();

    recordGet = pRecordBufferHeader->recordGet;
    recordPut = pRecordBufferHeader->recordPut;

    //
    // Manually add RECORD_SIZE instead of using LW_EVENT_BUFFER_RECORD* since sizeof(LW_EVENT_BUFFER_RECORD) is not RECORD_SIZE
    // In particular, even though we can specify how large we want each record to be, the size of the struct does not change, but the EventBuffer memory is allocated correctly
    //
    LwU64 headPtrAddr  = (LwU64)pRecordBuffer;
    LwU64 tailPtrAddr  = headPtrAddr + RECORD_COUNT * RECORD_SIZE;
    LwU64 putPtrAddr   = headPtrAddr + recordPut * RECORD_SIZE;
    LwU64 getPtrAddr   = headPtrAddr + recordGet * RECORD_SIZE;

    Printf(Tee::PriNormal, "Dumping Event Buffer Contents...\n");
    Printf(Tee::PriNormal, "For reference, %llu events have been dropped\n", pRecordBufferHeader->recordDropcount);

    while (getPtrAddr != putPtrAddr)
    {
        LW_EVENT_BUFFER_RECORD *getPtrContent = KERNEL_POINTER_FROM_LwP64(LW_EVENT_BUFFER_RECORD*, (LwP64)getPtrAddr);
        LwU16 eventType = getPtrContent->recordHeader.type;
        LwU16 eventSubtype = getPtrContent->recordHeader.subtype;

        Printf(Tee::PriNormal, "Type: %u, Subtype: %u\n", eventType, eventSubtype);

        if (eventType == LW_EVENT_BUFFER_RECORD_TYPE_FECS_CTX_SWITCH_V2)
        {
            LW_EVENT_BUFFER_FECS_RECORD_V2 *lwrrentRecord = KERNEL_POINTER_FROM_LwP64(LW_EVENT_BUFFER_FECS_RECORD_V2*, getPtrContent->inlinePayload);
            Printf(Tee::PriNormal, "Payload: seqno %u, tag %u, timestamp 0x%llx \n", lwrrentRecord->seqno, lwrrentRecord->tag, lwrrentRecord->timestamp);
            Printf(Tee::PriNormal, "Payload: vmid %u, pid %u, context_id %u \n", lwrrentRecord->vmid, lwrrentRecord->pid, lwrrentRecord->context_id);
            Printf(Tee::PriNormal, "Payload: migGpuInstanceId %u migComputeInstanceId %u \n\n", lwrrentRecord->migGpuInstanceId, lwrrentRecord->migComputeInstanceId);
        }

        getPtrAddr += RECORD_SIZE;
        if (getPtrAddr == tailPtrAddr)
        {
            getPtrAddr = headPtrAddr;
        }
    }

    return OK;
}

RC EventBufferFecsTest::RestartFecsEvent()
{
    RC rc;
    LW2080_CTRL_GR_FECS_BIND_EVTBUF_FOR_UID_V2_PARAMS uidParams = { 0 };
    UINT32 status;

    // Unbind the FECS event
    pLwRm->Free(fecsBindHandle);
    pLwRm->Free(rcBindHandle);
    pLwRm->Free(bufferHandle);

    if (pNotifyEvent != NULL)
        Tasker::FreeEvent(pNotifyEvent);

    // Allocate the EventBuffer again
    CHECK_RC(AllocBuffer(LW_TRUE));

    // Event Buffer Bind for FECS, again
    uidParams.hEventBuffer = bufferHandle;
    uidParams.bAllUsers = true;
    uidParams.levelOfDetail = LW2080_CTRL_GR_FECS_BIND_EVTBUF_LOD_FULL;

    status = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                            LW2080_CTRL_CMD_GR_FECS_BIND_EVTBUF_FOR_UID_V2,
                            &uidParams, sizeof(uidParams));

    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
            "FECS event buffer re-bind failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }

    CHECK_RC(EnableEvents());
    isOnSecondFecsBind = LW_TRUE;

    return OK;
}

RC EventBufferFecsTest::CleanFecs()
{
    LwU32 ch;
    RC rc;

    pThreadPool->ShutDown(); // stop the thread from pushing more events
    for (ch = 0; ch < NUM_TEST_CHANNELS; ch++)
    {
        if (pCh[ch] != NULL)
        {
            CHECK_RC(m_TestConfig.FreeChannel(pCh[ch]));
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ EventBufferFecsTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(EventBufferFecsTest, RmTest,
                 "EventBufferFecsTest RM Test.");
