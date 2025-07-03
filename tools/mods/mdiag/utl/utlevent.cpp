/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlchannel.h"
#include "utlevent.h"
#include "utltest.h"
#include "utlsurface.h"
#include "utlvftest.h"
#include "class/cl0005.h"
#include "class/clc369.h"

vector<void (*)()> BaseUtlEvent::s_UtlEventFreeFuncs;
vector<unique_ptr<RmEventData>> RmEventData::s_RmEventDatas;

void BaseUtlEvent::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("BeforeSendPushBufMethods.AddCallback", "func", true, "function to be called when event triggers");
    kwargs->RegisterKwarg("BeforeSendPushBufMethods.AddCallback", "pushBufName", false, "faulting push buffer name");
    kwargs->RegisterKwarg("BeforeSendPushBufMethods.AddCallback", "pushBufOffset", false, "faulting push buffer offset");
    kwargs->RegisterKwarg("NonReplayableFaultOverflowEvent.ForceBufferOverflow", "gpu", false, "The GPU on which to force an overflow. Defaults to GPU 0.");
    kwargs->RegisterKwarg("ReplayableFaultEvent.UpdateGetIndex", "index", true, "The new get index for device fault buffer.");
    kwargs->RegisterKwarg("ReplayableFaultEvent.SetUpdateFaultBufferGetIndex", "value", true, "Specifies if fault buffer get index needs to be updated.");

    UTL_BIND_EVENT(InitEvent, "This class represents the event that triggers after UTL has initialized the Python interpreter, but before UTL scripts are imported.")

    UTL_BIND_EVENT(StartEvent, "This class represents the event that triggers after UTL has initialized the Python interpreter and has imported all of the UTL scripts specified on the MODS command-line.  UTL scripts will typically create and run Test objects when this event triggers. If a callback is registered to this event, then the UTL script is responsible for creating and running the tests.")

    UTL_BIND_EVENT(SmcPartitionCreatedEvent, "This class represents an event that triggers after a GPU partition is created.")
    SmcPartitionCreatedEvent.def_readonly("partition", &UtlSmcPartitionCreatedEvent::m_Partition, "The gpu partition associated with this event.");

    UTL_BIND_EVENT(EndEvent, "This class represents the event that triggers just before MODS shuts down the Python interpreter and does final UTL cleanup.  This event can be used to override the final status of the MODS run.  By default, MODS looks at the status of each Test object that was run.")
    EndEvent.def_property("status",
        &UtlEndEvent::GetStatus,
        &UtlEndEvent::SetStatus,
        "An enum property which represents the status of current MODS run- set to Status.Succeeded if all tests are successful else set to the status of the first failing test). UTL scripts are allowed to change the status to any state except Status.Succeeded.");

    UTL_BIND_EVENT(MethodWriteEvent, "This class represents an event that can trigger whenever MODS writes a method to a channel.")
    MethodWriteEvent.def_readonly("channel", &UtlMethodWriteEvent::m_Channel,
        "The Channel object to which a method is being written");
    MethodWriteEvent.def_readonly("method", &UtlMethodWriteEvent::m_Method,
        "The method that is being written to the channel");
    MethodWriteEvent.def_readonly("data", &UtlMethodWriteEvent::m_Data,
        "The method data that is being written to the channel");
    MethodWriteEvent.def_readonly("hwClass", &UtlMethodWriteEvent::m_HwClass,
        "The number representing the HW class of the channel");
    MethodWriteEvent.def_readonly("methodsWritten", &UtlMethodWriteEvent::m_MethodsWritten,
        "The number of methods that have been written to the channel before this event.  This number will include both MODS methods and test methods.");
    MethodWriteEvent.def_readonly("testMethodsWritten", &UtlMethodWriteEvent::m_TestMethodsWritten,
        "The number of test methods that have been written to the channel before this event.  This will not include MODS methods.  Note that lwrrently only methods from a trace3d test will be counted.");
    MethodWriteEvent.def_readonly("isTestMethod", &UtlMethodWriteEvent::m_IsTestMethod,
        "This property is a bool that will equal True if the method being written comes from a test.  If the method instead comes from MODS, the property will equal False.  Note that lwrrently this property only counts methods from a trace3d test.");
    MethodWriteEvent.def_property_readonly("groupInfo",
        &UtlMethodWriteEvent::GetGroupInfo,
        "Methods can be sent in groups (e.g., an incrementing method) and each member of the group will result in a method write event.  This property is a tuple that represents group information about the method for this event.  There are two values in the tuple; the first value is the position of the method within the group (starting from 1) and the second value is the group size.");
    MethodWriteEvent.def_readwrite("skipWrite",
        &UtlMethodWriteEvent::m_SkipWrite,
        "Setting this property to True tells MODS to skip writing the method to the pushbuffer.  For method groups that trigger multiple events (e.g. an incrementing method with multiple data entries), all methods in the group will be skipped if any of the event objects have skipWrite = True.  Note that the methodsWritten and testMethodsWritten values will still be updated even when method writes are skipped.");

    UTL_BIND_EVENT(ChannelResetEvent, "This class represents an event that can trigger whenever a channel is reset.")
    ChannelResetEvent.def_readonly("channel", &UtlChannelResetEvent::m_Channel,
        "The channel object being reset");

    UTL_BIND_EVENT(TestCreatedEvent, "This class represents an event that triggers when a UTL or Mdiag test is created.")
    TestCreatedEvent.def_readonly("test", &UtlTestCreatedEvent::m_Test);

    UTL_BIND_EVENT(TestCleanupEvent, "This class represents an event that triggers before a test is cleaned up.")
    TestCleanupEvent.def_readonly("test", &UtlTestCleanupEvent::m_Test);

    UTL_BIND_EVENT(TestMethodsDoneEvent, "This class represents an event that triggers after test methods have exelwted.")
    TestMethodsDoneEvent.def_readonly("test", &UtlTestMethodsDoneEvent::m_Test);

    UTL_BIND_EVENT(Trace3dEvent, "This class represents an event that triggers when a Trace3d event triggers.")
    Trace3dEvent.def_readonly("test", &UtlTrace3dEvent::m_Test);
    Trace3dEvent.def_readonly("eventName", &UtlTrace3dEvent::m_EventName);

    UTL_BIND_EVENT(EngineNonStallIntEvent, "This class represents an event that triggers when a nonstalling interrupt oclwrs.")
    UTL_BIND_EVENT(NonFatalPoisonErrorEvent, "This class represents an event that triggers when a non fatal poison error oclwrs.")
    UTL_BIND_EVENT(FatalPoisonErrorEvent, "This class represents an event that triggers when a fatal poison error oclwrs.")
    UTL_BIND_EVENT(AccessCounterNotificationEvent, "This class represents an event that triggers when a access counter notification oclwrs")
    UTL_BIND_EVENT(NonReplayableFaultOverflowEvent, "This class represents the event that oclwrs when the capacity of the buffer of nonreplayable faults is exceeded.")
    NonReplayableFaultOverflowEvent.def("ForceBufferOverflow", &UtlNonReplayableFaultOverflowEvent::ForceBufferOverflow, "ForceBufferOverflow");
    NonReplayableFaultOverflowEvent.def_readonly("gpu", &UtlNonReplayableFaultOverflowEvent::m_pGpu, "The GPU on which the fault oclwred.");
    UTL_BIND_EVENT(NonReplayableFaultEvent, "This class represents the event for non-replayable fault.")
    NonReplayableFaultEvent.def("IsFaultBufferEmpty", &UtlNonReplayableFaultEvent::IsFaultBufferEmpty, "Indicates if the shadow fault buffer is empty.");
    NonReplayableFaultEvent.def_property_readonly("data", &UtlNonReplayableFaultEvent::GetData, "Returns the fault buffer data.");
    NonReplayableFaultEvent.def_readonly("gpu", &UtlNonReplayableFaultEvent::m_pGpu, "The GPU on which the fault oclwred.");
    UTL_BIND_EVENT(ReplayableFaultEvent, "This class represents the event for replayable fault.");
    ReplayableFaultEvent.def("IsFaultBufferOverflowed", &UtlReplayableFaultEvent::IsFaultBufferOverflowed, "Checks if there is overflow in subdevice fault buffer.");
    ReplayableFaultEvent.def("UpdateGetIndex", &UtlReplayableFaultEvent::UpdateGetIndex, "Updates the replayable fault buffer's get index.");
    ReplayableFaultEvent.def("FetchPutIndex", &UtlReplayableFaultEvent::FetchPutIndex, "Makes an RM call to get the put index.");
    ReplayableFaultEvent.def("EnableNotifications", &UtlReplayableFaultEvent::EnableNotifications, "Enable the notifications on device fault buffer.");
    ReplayableFaultEvent.def_property("getIndex", &UtlReplayableFaultEvent::GetGetIndex, &UtlReplayableFaultEvent::SetGetIndex, "Represents the get index of subdevice fault buffer.");
    ReplayableFaultEvent.def_property("putIndex", &UtlReplayableFaultEvent::GetPutIndex, &UtlReplayableFaultEvent::SetPutIndex, "Represents the put index of subdevice fault buffer.");
    ReplayableFaultEvent.def_property("data", &UtlReplayableFaultEvent::GetData, &UtlReplayableFaultEvent::SetData, "Represents the fault entry data of subdevice fault buffer.");
    ReplayableFaultEvent.def_property_readonly("maxFaultBufferEntries",
            &UtlReplayableFaultEvent::GetMaxFaultBufferEntries,
            "Represents the put index of subdevice fault buffer.");
    ReplayableFaultEvent.def_readonly("gpu",
            &UtlReplayableFaultEvent::m_pGpu,
            "The GPU on which the fault oclwred.");
    ReplayableFaultEvent.def_static("GetUpdateFaultBufferGetIndex", &UtlReplayableFaultEvent::GetUpdateFaultBufferGetIndex, "Returns true if get index of fault buffer needs to be updated.");
    ReplayableFaultEvent.def_static("SetUpdateFaultBufferGetIndex", &UtlReplayableFaultEvent::SetUpdateFaultBufferGetIndex, "Specifies if get index of fault buffer needs to be updated.");

    UTL_BIND_EVENT(NonReplayableFaultInPrivsEvent, "This class represents a nonreplayable fault event that doesn't have a corresponding fault buffer entry, but with data in device registers.");
    NonReplayableFaultInPrivsEvent.def_readonly("gpu",
        &UtlNonReplayableFaultInPrivsEvent::m_pGpu,
        "The GPU on which the fault oclwred.");
    UTL_BIND_EVENT(SecFaultEvent, "This class represents an event that triggers when a sec fault oclwrs.")
    UTL_BIND_EVENT(HangEvent, "This class represents the event that triggers after hang is detected.");
    UTL_BIND_EVENT(TablePrintEvent, "This class represents the event that triggers when table print is needed.");
    TablePrintEvent.def_readonly("header", &UtlTablePrintEvent::m_Header,
        "Table header to be printed");
    TablePrintEvent.def_readonly("data", &UtlTablePrintEvent::m_Data,
        "Table data to be printed");
    TablePrintEvent.def_readonly("isDebug", &UtlTablePrintEvent::m_IsDebug,
        "Specifies if the table will be printed only when debug messages are enabled");
    UTL_BIND_EVENT(UserEvent, "This class represents the event which can be triggered by the user (typically via plugin/policy manager).");
    UserEvent.def_readonly("userData", &UtlUserEvent::m_UserData,
        "Data (map<arg, argData>) passed by the user for this event");
    UTL_BIND_EVENT(TraceFileReadEvent, "This class represents the event that triggers when a trace file is read.");
    TraceFileReadEvent.def("GetDataSeq", &UtlTraceFileReadEvent::GetDataSeq,
        "GetDataSeq",
        py::return_value_policy::reference);
    TraceFileReadEvent.def_readonly("fileName", &UtlTraceFileReadEvent::m_FileName,
        "Name of the trace file being read");
    TraceFileReadEvent.def_readonly("test", &UtlTraceFileReadEvent::m_Test,
        "Test which ilwoked this event");

    UTL_BIND_EVENT(BeforeSendPushBufHeaderEvent, "This class represents an event that triggers before the first method in METHODS")
    BeforeSendPushBufHeaderEvent.def_readonly("pushBufName", &UtlBeforeSendPushBufHeaderEvent::m_PushBufName);
    BeforeSendPushBufHeaderEvent.def_readonly("pushBufOffset", &UtlBeforeSendPushBufHeaderEvent::m_PushBufOffset);
    BeforeSendPushBufHeaderEvent.def_readonly("test", &UtlBeforeSendPushBufHeaderEvent::m_Test);

    UTL_BIND_EVENT(SurfaceAllocationEvent,
        "This class represents an event that triggers when a surface is about to be allocated.");
    SurfaceAllocationEvent.def_readonly("surface",
        &UtlSurfaceAllocationEvent::m_Surface,
        "This member points to a utl.Surface object representing the surface that is about to be allocated.");
    SurfaceAllocationEvent.def_readonly("test",
        &UtlSurfaceAllocationEvent::m_Test,
        "If the event surface belongs to a test, this member will point to the corresponding utl.Test object."
        "  Otherwise, this member will be None.");

    UTL_BIND_EVENT(VfLaunchedEvent, "This class represents the event that triggers after vf is launched.")
    VfLaunchedEvent.def_readonly("vfTest", &UtlVfLaunchedEvent::m_VfTest,
        "The member points to a utl.VfTest object representing the vf test that is launched.");

    UtlTraceOpEvent::RegisterAll(module);
}

void BaseUtlEvent::FreeCallbacks()
{
    for (const auto& func : s_UtlEventFreeFuncs)
    {
        func();
    }

    RmEventData::FreeAllEventData();
}

void UtlEndEvent::SetStatus(Test::TestStatus status)
{
    if (status == Test::TEST_SUCCEEDED)
    {
        UtlUtility::PyError("can't change EndEvent.status to utl.Status.Succeeded");
    }

    m_Status = status;
}

UtlTestCreatedEvent::UtlTestCreatedEvent
(
    UtlTest* pTest
) :
    UtlEvent(),
    m_Test(pTest)
{
    MASSERT(pTest);
}

UtlTestMethodsDoneEvent::UtlTestMethodsDoneEvent
(
    UtlTest* pTest
) :
    UtlEvent(),
    m_Test(pTest)
{
    MASSERT(pTest);
}

UtlTestCleanupEvent::UtlTestCleanupEvent
(
    UtlTest* pTest
) :
    UtlEvent(),
    m_Test(pTest)
{
    MASSERT(pTest);
}

UtlSmcPartitionCreatedEvent::UtlSmcPartitionCreatedEvent
(
    UtlGpuPartition* pPartition
) :
    UtlEvent(),
    m_Partition(pPartition)
{
    MASSERT(pPartition);
}

UtlTrace3dEvent::UtlTrace3dEvent
(
    UtlTest* pTest,
    const string& eventName
) :
    UtlEvent(),
    m_Test(pTest),
    m_EventName(eventName)
{
    MASSERT(pTest);
    MASSERT(!eventName.empty());
}

UtlBeforeSendPushBufHeaderEvent::UtlBeforeSendPushBufHeaderEvent
(
    UtlTest* pTest
) :
    UtlEvent(),
    m_Test(pTest)
{
    MASSERT(pTest);
}

UtlEngineNonStallIntEvent::UtlEngineNonStallIntEvent
(
    RmEventData* pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
}

UtlNonFatalPoisonErrorEvent::UtlNonFatalPoisonErrorEvent
(
    RmEventData* pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
}

UtlFatalPoisonErrorEvent::UtlFatalPoisonErrorEvent
(
    RmEventData* pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
}

UtlAccessCounterNotificationEvent::UtlAccessCounterNotificationEvent
(
    RmEventData* pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
}

UtlNonReplayableFaultOverflowEvent::UtlNonReplayableFaultOverflowEvent
(
    RmEventData* pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
    m_pGpu = UtlGpu::GetFromGpuSubdevicePtr(pEventData->m_pGpuSubdev);
    MASSERT(m_pGpu);
}

UtlNonReplayableFaultEvent::UtlNonReplayableFaultEvent
(
    RmEventData* pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
    m_pGpu = UtlGpu::GetFromGpuSubdevicePtr(pEventData->m_pGpuSubdev);
    MASSERT(m_pGpu);

    m_pShadowFaultBuffer = SubdeviceShadowFaultBuffer::GetShadowFaultBuffer(pEventData->m_pGpuSubdev);
}

bool UtlNonReplayableFaultEvent::IsFaultBufferEmpty()
{
    bool result;
    m_pShadowFaultBuffer->IsFaultBufferEmpty(&result);
    return result;
}

vector<UINT32> UtlNonReplayableFaultEvent::GetData()
{
    SubdeviceShadowFaultBuffer::FaultData faultData;
    m_pShadowFaultBuffer->PopAndCopyNextFaultEntry(&faultData);
    return vector<UINT32>(faultData.data, faultData.data + (LWC369_BUF_SIZE / sizeof(UINT32)));
}

UtlReplayableFaultEvent::UtlReplayableFaultEvent
(
    RmEventData* pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
    m_pGpu = UtlGpu::GetFromGpuSubdevicePtr(pEventData->m_pGpuSubdev);
    MASSERT(m_pGpu);

    m_pReplayableFault = static_cast<UtlCallbackFilter::ReplayableFault<UtlReplayableFaultEvent> *>((void*)&pEventData->m_Data[0]);
    m_pSubdeviceFaultBuffer = SubdeviceFaultBuffer::GetFaultBuffer(pEventData->m_pGpuSubdev);
}

/* static */ void UtlNonReplayableFaultOverflowEvent::ForceBufferOverflow
(
    py::kwargs kwargs
)
{
    RC rc;
    UINT32 size = 0, put = 0;
    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu",
        (GetClassName() + ".ForceBufferOverflow").c_str()))
    {
        gpu = UtlGpu::GetGpus()[0];
    }
    GpuSubdevice* pGpuSubdevice = gpu->GetGpuSubdevice();
    DebugPrintf("UTL: Forcing nonreplayable fault overflow on subdevice %p.\n",
        pGpuSubdevice);

    SubdeviceShadowFaultBuffer* pShadowBuffer =
        SubdeviceShadowFaultBuffer::GetShadowFaultBuffer(pGpuSubdevice);
    MASSERT(pShadowBuffer);
    rc = pShadowBuffer->GetFaultBufferSize(&size);
    UtlUtility::PyErrorRC(rc, "Couldn't get fault buffer size");
    rc = pShadowBuffer->GetFaultBufferPutPointer(&put);
    UtlUtility::PyErrorRC(rc, "Couldn't get fault buffer put pointer");
    rc = pShadowBuffer->UpdateFaultBufferGetPointer((put + 1) % size);
    UtlUtility::PyErrorRC(rc, "Couldn't update fault buffer get pointer");

    DebugPrintf("UTL: Forced nonreplayable fault buffer overflow OK.\n");
}

UtlNonReplayableFaultInPrivsEvent::UtlNonReplayableFaultInPrivsEvent
(
    RmEventData *pEventData
) :
    UtlEvent(),
    m_Handle(pEventData->m_Handle)
{
    m_pGpu = UtlGpu::GetFromGpuSubdevicePtr(pEventData->m_pGpuSubdev);
    MASSERT(m_pGpu);
}

UtlSecFaultEvent::UtlSecFaultEvent
(
    LwRm::Handle handle
) :
    UtlEvent(),
    m_Handle(handle)
{
}

UINT32 UtlReplayableFaultEvent::GetGetIndex()
{
    return m_pReplayableFault->m_GetIndex;
}

void UtlReplayableFaultEvent::SetGetIndex(UINT32 index)
{
    m_pReplayableFault->m_GetIndex = index;
}

UINT32 UtlReplayableFaultEvent::GetPutIndex()
{
    return m_pReplayableFault->m_PutIndex;
}

void UtlReplayableFaultEvent::SetPutIndex(UINT32 index)
{
    m_pReplayableFault->m_PutIndex = index;
}

void UtlReplayableFaultEvent::UpdateGetIndex(py::kwargs kwargs)
{
    RC rc;
    UINT32 newIndex = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs,
        "index",
        "ReplayableFaultEvent.UpdateGetIndex");

    UtlGil::Release gil;
    rc = m_pSubdeviceFaultBuffer->UpdateFaultBufferGetIndex(newIndex);
    UtlUtility::PyErrorRC(rc, "Unable to update get index of subdevice fault buffer.");
}

void UtlReplayableFaultEvent::FetchPutIndex()
{
    RC rc;
    UtlGil::Release gil;
    rc = m_pSubdeviceFaultBuffer->GetFaultBufferPutIndex(&m_pReplayableFault->m_PutIndex);
    UtlUtility::PyErrorRC(rc, "Unable to fetch put index of subdevice fault buffer.");
}

vector<UINT32> UtlReplayableFaultEvent::GetData()
{
    const UINT32 entrySizeInDword = LWC369_BUF_SIZE / sizeof(UINT32);
    const UINT32 *faultEntryAddr = &(m_pReplayableFault->m_FaultBufferEntries[m_pReplayableFault->m_GetIndex * entrySizeInDword]);

    vector<UINT32> faultEntry(entrySizeInDword);
    UtlGil::Release gil;

    for (UINT32 i = 0; i < faultEntry.size(); ++i)
    {
        faultEntry[i] = Platform::VirtualRd32(faultEntryAddr + i);
    }

    DebugPrintf("Retrieved fault entry from subdevice fault buffer at index = 0x%x\n", m_pReplayableFault->m_GetIndex);
    return faultEntry;
}

bool UtlReplayableFaultEvent::IsFaultBufferOverflowed()
{
    GpuSubdevice* pGpuSubdevice = m_pGpu->GetGpuSubdevice();
    UtlGil::Release gil;

    bool overflow = pGpuSubdevice->FaultBufferOverflowed();
    DebugPrintf("Subdevice fault buffer overflowed = 0x%x\n", overflow);
    return overflow;
}

void UtlReplayableFaultEvent::SetData(py::list pyData)
{
    const UINT32 entrySizeInDword = LWC369_BUF_SIZE / sizeof(UINT32);
    volatile UINT32 *faultEntryAddr = &(m_pReplayableFault->m_FaultBufferEntries[m_pReplayableFault->m_GetIndex * entrySizeInDword]);

    vector<UINT32> data;
    UtlUtility::CastPyObjectToListOrSingleElement(pyData, &data);

    if (data.empty() || data.size() != entrySizeInDword)
    {
        UtlUtility::PyError("Invalid data passed to ReplayableFaultEvent.SetData");
    }

    DebugPrintf("Successfully colwerted fault entry object to vector<UINT32>.\n");

    UtlGil::Release gil;
    for (UINT32 i = 0; i < entrySizeInDword; ++i)
    {
        Platform::VirtualWr32(faultEntryAddr + i, data[i]);
    }

    DebugPrintf("Completed fault entry write at index = 0x%x in subdevice fault buffer.\n", m_pReplayableFault->m_GetIndex);
}

UINT32 UtlReplayableFaultEvent::GetMaxFaultBufferEntries()
{
    return m_pReplayableFault->m_MaxFaultBufferEntries;
}

void UtlReplayableFaultEvent::EnableNotifications()
{
    RC rc;
    UtlGil::Release gil;

    rc = m_pSubdeviceFaultBuffer->EnableNotificationsForFaultBuffer();
    UtlUtility::PyErrorRC(rc, "Unable to enable notifications on subdevice fault buffer.");
}

bool UtlReplayableFaultEvent::UpdateFaultBufferGetIndex = true;

bool UtlReplayableFaultEvent::GetUpdateFaultBufferGetIndex()
{
    return UtlReplayableFaultEvent::UpdateFaultBufferGetIndex;
}

void UtlReplayableFaultEvent::SetUpdateFaultBufferGetIndex(py::kwargs kwargs)
{
    UINT32 value = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs,
        "value",
        "ReplayableFaultEvent.SetUpdateFaultBufferGetIndex");
    UtlReplayableFaultEvent::UpdateFaultBufferGetIndex = value;
}

RmEventData::RmEventData
(
    LwRm* pLwRm,
    LwRm::Handle parent,
    GpuSubdevice* pGpuSubdev,
    UINT32 index,
    void* pData,
    UINT32 size
) :
    m_Handle(0),
    m_Parent(parent),
    m_pLwRm(pLwRm),
    m_pGpuSubdev(pGpuSubdev),
    m_Index(index),
    m_Data((UINT08*)pData, (UINT08*)pData + size)
{
    m_SubdevHandle = m_pLwRm->GetSubdeviceHandle(m_pGpuSubdev);
}

RmEventData::~RmEventData()
{
    if (OK != SetEventNotification(DISABLE))
    {
        ErrPrintf("Failed to disable the event notification for index %d.", m_Index);
        MASSERT(0);
    }

    if (m_Handle != 0)
    {
        m_pLwRm->Free(m_Handle);
        m_Handle = 0;
    }
}

RC RmEventData::SetEventNotification
(
    RmEventData::EventNotificationActionType action
)
{
    RC rc;

    if (m_SubdevHandle == m_Parent)
    {
        // Set event notification action
        LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS notificationParams = {};
        notificationParams.event  = (m_Index & LW01_EVENT_NONSTALL_INTR) ? 
            (m_Index & (~LW01_EVENT_NONSTALL_INTR)) : m_Index;
        notificationParams.action = action;
        CHECK_RC(m_pLwRm->ControlBySubdevice(m_pGpuSubdev,
                                             LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                                             &notificationParams,
                                             sizeof(notificationParams)));
    }

    return rc;
}

/* static */ RmEventData* RmEventData::MatchExistingRmEventData
(
    LwRm::Handle parent,
    GpuSubdevice* pGpuSubdev,
    UINT32 index
)
{
    for (auto const & pRmEventData : s_RmEventDatas)
    {
        if (pRmEventData->m_Parent == parent &&
            pRmEventData->m_pGpuSubdev == pGpuSubdev &&
            pRmEventData->m_Index == index)
        {
            return pRmEventData.get();
        }
    }

    return nullptr;
}

/* static */ RmEventData* RmEventData::GetRmThreadEventData
(
    LwRm* pLwRm,
    LwRm::Handle parent,
    GpuDevice* pGpuDevice,
    GpuSubdevice* pGpuSubdev,
    UINT32 index,
    EventThread::HandlerFunc handler,
    void* pData,
    UINT32 size
)
{
    auto pRmEventData = MatchExistingRmEventData(parent, pGpuSubdev, index);
    if (pRmEventData == nullptr)
    {
        s_RmEventDatas.push_back(make_unique<RmThreadEventData>(pLwRm, parent,
            pGpuDevice, pGpuSubdev, index, pData, size));
        pRmEventData = s_RmEventDatas.back().get();
        pRmEventData->m_pEventThread->SetHandler(handler, pRmEventData);
    }

    return pRmEventData;
}

/* static */ RmEventData* RmEventData::GetRmCallbackEventData
(
    LwRm* pLwRm,
    LwRm::Handle parent,
    GpuSubdevice* pGpuSubdev,
    UINT32 index,
    RmEventData::CallbackFunc handler,
    void* arg,
    void* pData,
    UINT32 size
)
{
    auto pRmEventData = MatchExistingRmEventData(parent, pGpuSubdev, index);
    if (pRmEventData == nullptr)
    {
        s_RmEventDatas.push_back(make_unique<RmCallbackEventData>(pLwRm, parent,
            pGpuSubdev, index, handler, arg, pData, size));
        pRmEventData = s_RmEventDatas.back().get();
    }
    return pRmEventData;
}

/* static */ void RmEventData::FreeLwRmEventData(LwRm* pLwRm)
{
    s_RmEventDatas.erase(remove_if(s_RmEventDatas.begin(), s_RmEventDatas.end(),
        [&](unique_ptr<RmEventData> const & data) { return data->m_pLwRm == pLwRm; }), 
        s_RmEventDatas.end()
    );
}

/* static */ void RmEventData::FreeAllEventData()
{
    s_RmEventDatas.clear();
}

RmThreadEventData::RmThreadEventData
(
    LwRm* pLwRm,
    LwRm::Handle parent,
    GpuDevice* pGpuDevice,
    GpuSubdevice* pGpuSubdev,
    UINT32 index,
    void* pData,
    UINT32 size
) :
    RmEventData(pLwRm, parent, pGpuSubdev, index, pData, size),
    m_pGpuDevice(pGpuDevice)
{
    m_pEventThread.reset(new EventThread());

    void* const osEvent = m_pEventThread->GetOsEvent(
        m_pLwRm->GetClientHandle(),
        m_pLwRm->GetDeviceHandle(m_pGpuDevice));
    MASSERT(osEvent);
    RC rc = m_pLwRm->AllocEvent(m_Parent,
        &m_Handle, LW01_EVENT_OS_EVENT, m_Index, osEvent);

    if (rc != OK)
    {
        m_pEventThread.reset(nullptr);
        UtlUtility::PyErrorRC(rc, "AddCallback failed to allocate an RM event.");
    }

    rc = SetEventNotification(REPEAT);
    if (OK != rc)
    {
        m_pEventThread.reset(nullptr);
        UtlUtility::PyErrorRC(rc, "AddCallback failed to enable event notification for index %d.", m_Index);
    }
}

RmThreadEventData::~RmThreadEventData()
{
    RC rc = m_pEventThread->SetHandler(nullptr);
    if (rc != OK)
    {
        MASSERT(!"EventThread SetHandler to nullptr failed");
    }
}

RmCallbackEventData::RmCallbackEventData
(
    LwRm* pLwRm,
    LwRm::Handle parent,
    GpuSubdevice* pGpuSubdev,
    UINT32 index,
    CallbackFunc handler,
    void* arg,
    void *pData,
    UINT32 size
) :
    RmEventData(pLwRm, parent, pGpuSubdev, index, pData, size),
    m_CallbackParams({0})
{
    LwRm::Handle subdevHandle = m_pLwRm->GetSubdeviceHandle(m_pGpuSubdev);
    LW0005_ALLOC_PARAMETERS eventParams = {};

    m_CallbackParams.func = handler;
    m_CallbackParams.arg = arg;

    eventParams.hParentClient = m_pLwRm->GetClientHandle();
    eventParams.hClass = LW01_EVENT_KERNEL_CALLBACK_EX;
    eventParams.notifyIndex = m_Index;
    eventParams.data = LW_PTR_TO_LwP64(&m_CallbackParams);

    RC rc = m_pLwRm->Alloc(subdevHandle,
                           &m_Handle,
                           LW01_EVENT_KERNEL_CALLBACK_EX,
                           &eventParams);
    UtlUtility::PyErrorRC(rc, "AddCallback failed to allocate an RM callback event.");


    rc = SetEventNotification(REPEAT);
    UtlUtility::PyErrorRC(rc, "AddCallback failed to enable event notification for index %d.", m_Index);
}

RmCallbackEventData::~RmCallbackEventData()
{
}

py::tuple UtlMethodWriteEvent::GetGroupInfo()
{
    return py::make_tuple(m_GroupIndex, m_GroupCount);
}

UtlTablePrintEvent::UtlTablePrintEvent
(
    string&& tableHeader,
    vector<vector<string>>&& tableData,
    bool isDebug
) :
    UtlEvent(),
    m_Header(move(tableHeader)),
    m_Data(move(tableData)),
    m_IsDebug(isDebug)
{
}

UtlUserEvent::UtlUserEvent
(
    map<string, string>&& userData
) :
    UtlEvent(),
    m_UserData(move(userData))
{
}

UtlTraceFileReadEvent::UtlTraceFileReadEvent
(
    UtlTest* pUtlTest,
    string fileName,
    void* data,
    size_t size
) :
    UtlEvent(),
    m_Test(pUtlTest),
    m_FileName(fileName)
{
    m_SeqData = make_unique<UtlSequence>(data, size);
}

UtlSequence* UtlTraceFileReadEvent::GetDataSeq()
{
    MASSERT(m_SeqData);
    return m_SeqData.get();
}

// UTL TraceOp event handle

UtlTraceOpEvent::UtlTraceOpEvent(UtlTest* pTest, TraceOp* pOp)
{
    m_Test = pTest;
    SetOp(pOp);
}

void UtlTraceOpEvent::SetOp(TraceOp* pOp)
{
    MASSERT(pOp);
    m_pOp = pOp;
    TraceOpType type = pOp->GetType();
    TraceOpAllocSurface* pAlloc = nullptr;
    TraceOpUpdateReloc64* pReloc64TraceOp = nullptr;
    switch (type)
    {
        case TraceOpType::AllocSurface:
        case TraceOpType::AllocVirtual:
        case TraceOpType::AllocPhysical:
        case TraceOpType::Map:
        case TraceOpType::SurfaceView:
            pAlloc = dynamic_cast<TraceOpAllocSurface*>(pOp);
            MASSERT(pAlloc);
            MASSERT(pAlloc->GetParameterSurface());
            m_ParamSurf = UtlSurface::GetFromMdiagSurf(pAlloc->GetParameterSurface());
            MASSERT(m_ParamSurf);
            break;
        case TraceOpType::Reloc64:
            pReloc64TraceOp = dynamic_cast<TraceOpUpdateReloc64*>(pOp);
            MASSERT(pReloc64TraceOp);
            m_ParamSurf = m_Test->GetSurface(pReloc64TraceOp->GetSurfaceName());
            break;
        default:
            break;
    }
    InitProperties();
}

UINT32 UtlTraceOpEvent::GetLineNumber() const
{
    MASSERT(m_pOp);
    return m_pOp->GetTraceOpId();
}

TraceOpType UtlTraceOpEvent::GetType() const
{
    MASSERT(m_pOp);
    return m_pOp->GetType();
}

TraceOpTriggerPoint UtlTraceOpEvent::GetTriggerPoint() const
{
    MASSERT(m_pOp);
    return m_pOp->GetTriggerPoint();
}

bool UtlTraceOpEvent::NeedSkip() const
{
    MASSERT(m_pOp);
    return m_pOp->NeedSkip();
}

void UtlTraceOpEvent::SetSkip(bool bSkip)
{
    MASSERT(m_pOp);
    m_pOp->SetSkip(bSkip);
}

void UtlTraceOpEvent::RegisterAll(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();

    UTL_BIND_EVENT(TraceOpEvent,
        "This class represents an event handler object that triggers "
        "when trace.hdr parsing or before or after a TraceOp Mdiag side Run().");

    py::enum_<TraceOpType>(TraceOpEvent, "Type")
        .value("AllocSurface", TraceOpType::AllocSurface)
        .value("AllocVirtual", TraceOpType::AllocVirtual)
        .value("AllocPhysical", TraceOpType::AllocPhysical)
        .value("Map", TraceOpType::Map)
        .value("SurfaceView", TraceOpType::SurfaceView)
        .value("FreeSurface", TraceOpType::FreeSurface)
        .value("UpdateFile", TraceOpType::UpdateFile)
        .value("CheckDynamicSurface", TraceOpType::CheckDynamicSurface)
        .value("Reloc64", TraceOpType::Reloc64)
        ;

    py::enum_<TraceOpTriggerPoint>(TraceOpEvent, "TriggerPoint", py::arithmetic())
        .value("Parse", TraceOpTriggerPoint::Parse)
        .value("Before", TraceOpTriggerPoint::Before)
        .value("After", TraceOpTriggerPoint::After)
        ;

    TraceOpEvent.def_property_readonly("type", &UtlTraceOpEvent::GetType,
        "The TraceOp type of the object.");
    TraceOpEvent.def_property_readonly("lineNumber", &UtlTraceOpEvent::GetLineNumber,
        "The TraceOp line number in trace.hdr per the TraceOp object.");
    // Associated UtlTest object
    TraceOpEvent.def_readonly("test", &UtlTraceOpEvent::m_Test);

    TraceOpEvent.def_property_readonly("triggerPoint",
        &UtlTraceOpEvent::GetTriggerPoint,
        "An enum property represents TraceOp actual triggered point.");

    TraceOpEvent.def_property("skip",
        &UtlTraceOpEvent::NeedSkip,
        &UtlTraceOpEvent::SetSkip,
        "A bool property if the TraceOp Mdiag side Run() needs be skipped.");

    TraceOpEvent.def_property_readonly("props", &UtlTraceOpEvent::GetProperties,
            "This function gets properties from the TraceOp to use in script.",
        py::return_value_policy::reference);
    TraceOpEvent.def_property_readonly("hdrStrProps", &UtlTraceOpEvent::GetHdrStrProperties,
        "This function gets HdrStrProperties from the TraceOp to use in script.",
        py::return_value_policy::reference);
    TraceOpEvent.def_property_readonly("parameterSurface", &UtlTraceOpEvent::GetParameterSurface,
        "This function gets ParameterSurface of the TraceOp to use in script.");
}

void UtlTraceOpEvent::InitProperties()
{
    MASSERT(m_pOp);
    const char* opName = nullptr;
    m_pOp->GetTraceopProperties(&opName, m_Props);
    (void)opName;
}

const GpuTrace::HdrStrProps& UtlTraceOpEvent::GetHdrStrProperties() const
{
    MASSERT(m_pOp);
    return m_pOp->GetHdrStrProperties();
}

const string UtlTraceOpEvent::GetClassName()
{
    return "TraceOpEvent";
}

UtlSurfaceAllocationEvent::UtlSurfaceAllocationEvent
(
    UtlSurface* surface,
    UtlTest* test
) :
    UtlEvent(),
    m_Surface(surface),
    m_Test(test)
{
    MASSERT(surface != nullptr);
}

UtlChannelResetEvent::UtlChannelResetEvent
(
    UtlChannel* pUtlChannel
) :
    UtlEvent(),
    m_Channel(pUtlChannel)
{
    MASSERT(m_Channel != nullptr);
}

UtlVfLaunchedEvent::UtlVfLaunchedEvent
(
    UtlVfTest* vfTest
) :
    UtlEvent(),
    m_VfTest(vfTest)
{
    MASSERT(m_VfTest != nullptr);
}

// The UtlInitEvent object has a Test callback filter, but that filter is only
// used internally by MODS.  The following template specializations
// prevent the user from specifying a test object as a filter for an
// InitEvent callback.

namespace UtlCallbackFilter
{
    template <>
    void Test<UtlInitEvent>::Register(py::module module)
    {
    }

    template <>
    void Test<UtlInitEvent>::ProcessKwargs(py::kwargs kwargs)
    {
        m_Test = Utl::Instance()->GetTestFromScriptId();
    }

    template <>
    bool Test<UtlInitEvent>::MatchesEvent(const UtlInitEvent* event) const
    {
        return (m_Test == event->m_Test);
    }
}
