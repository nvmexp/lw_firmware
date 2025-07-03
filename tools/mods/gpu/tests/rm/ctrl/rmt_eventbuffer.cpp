/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_eventbuffer.cpp
//! \brief Test for covering rm event buffer functionality
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

#include "class/cl2080.h"
#include "class/cl90cd.h"
#include "ctrl/ctrl90cd.h"

#define RECORD_SIZE                 32
#define RECORD_COUNT                100
#define RECORD_BUFFER_SIZE          (RECORD_SIZE * RECORD_COUNT)
#define VARDATA_BUFFER_SIZE         6400

class EventBufferTest : public RmTest
{
public:
    EventBufferTest();
    virtual ~EventBufferTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    RC AllocBuffer();
    RC BindEvent();
    RC UpdateGet(LwU32 recordBufferGet, LwU32 vardataBufferGet);
    RC EnableEvents();

    LwRmPtr pLwRm;
    LwRm::Handle            bufferHandle         = 0;
    LwRm::Handle            hNotifyEvent         = 0;
    ModsEvent               *pNotifyEvent        = nullptr;
    LwP64                   pRecordBuffer        = 0;
    LwP64                   pVardadtaBuffer      = 0;
    LW_EVENT_BUFFER_HEADER  *pRecordBufferHeader = nullptr;

    LwRm::Handle            bindHandle           = 0;
};

//! \brief EventBufferTest basic constructor
//!
//! This is just the basic constructor for the EventBufferTest class.
//!
//! \sa Setup
//------------------------------------------------------------------------------
EventBufferTest::EventBufferTest()
{
    SetName("EventBufferTest");
}

//! \brief EventBufferTest destructor
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
EventBufferTest::~EventBufferTest()
{
}

//! \brief IsSupported
//!
//! This test is supported on all elwironments.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string EventBufferTest::IsTestSupported()
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
RC EventBufferTest::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run entry point
//!
//! Kick off system control command tests.
//!
//! \return OK if test passes, error code indicating nature of failure
//!         otherwise
//-----------------------------------------------------------------------------
RC EventBufferTest::Run()
{
    RC    rc = OK;
    LwU32 i;

    CHECK_RC(AllocBuffer());
    CHECK_RC(BindEvent());
    CHECK_RC(EnableEvents());

    CHECK_RC(UpdateGet(RECORD_COUNT / 2, VARDATA_BUFFER_SIZE / 2));
    CHECK_RC(UpdateGet(RECORD_COUNT / 2, VARDATA_BUFFER_SIZE / 2));

    for (i = 0; i < 5; i++)
    {
        CHECK_RC(UpdateGet(i, i));
    }

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

    if (UpdateGet(RECORD_COUNT * 2, VARDATA_BUFFER_SIZE / 2) == OK ||
        UpdateGet(RECORD_COUNT / 2, VARDATA_BUFFER_SIZE * 2) == OK ||
        UpdateGet(RECORD_COUNT, VARDATA_BUFFER_SIZE / 2) == OK ||
        UpdateGet(RECORD_COUNT / 2, VARDATA_BUFFER_SIZE) == OK)
    {
        rc = 1;
    }

    return rc;
}

//! \brief Cleanup entry point
//!
//! Release outstanding resource.
//!
//! \return No status returned; always passes.
//-----------------------------------------------------------------------------
RC EventBufferTest::Cleanup()
{
    RC rc;

    pLwRm->Free(bufferHandle);

    if (pNotifyEvent != NULL)
        Tasker::FreeEvent(pNotifyEvent);

    return OK;
}

RC EventBufferTest::AllocBuffer()
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
    params.vardataBufferSize = VARDATA_BUFFER_SIZE;
    params.recordCount = RECORD_COUNT;
    params.recordsFreeThreshold = RECORD_COUNT / 2;
    params.vardataFreeThreshold = VARDATA_BUFFER_SIZE / 2;
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
    pVardadtaBuffer = params.vardataBuffer;
    pRecordBufferHeader = (LW_EVENT_BUFFER_HEADER*)params.bufferHeader;

    return OK;
}

RC EventBufferTest::BindEvent()
{
    RC rc;
    LW_EVENT_BUFFER_BIND_PARAMETERS params = { 0 };
    UINT32 status = 0;

    // Allocate Event Buffer
    params.bufferHandle = bufferHandle;
    params.eventType = LW2080_NOTIFIERS_RC_ERROR;
    params.hClientTarget = pLwRm->GetClientHandle();
    params.hSrcResource = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    status = pLwRm->Alloc(bufferHandle,
        &bindHandle,
        LW_EVENT_BUFFER_BIND, &params);

    if (status != LWOS_STATUS_SUCCESS)
    {
        Printf(Tee::PriNormal,
            "Event buffer object allocation failed: 0x%x\n", status);
        return RmApiStatusToModsRC(status);
    }
    return OK;
}

RC EventBufferTest::UpdateGet(LwU32 recordBufferGet, LwU32 vardataBufferGet)
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

RC EventBufferTest::EnableEvents()
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

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ EventBufferTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(EventBufferTest, RmTest,
                 "EventBufferTest RM Test.");
