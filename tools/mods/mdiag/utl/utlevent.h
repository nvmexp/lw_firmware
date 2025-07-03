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

#ifndef INCLUDED_UTLEVENT_H
#define INCLUDED_UTLEVENT_H

#include <algorithm>

#include "mdiag/tests/test.h"
#include "mdiag/thread.h"
#include "class/clc365.h"
#include "class/clc369.h"
#include "ctrl/ctrlc365.h"

#include "utlutil.h"
#include "utlpython.h"
#include "utlchannel.h"
#include "mdiag/lwgpu.h"
#include "utlgpupartition.h"
#include "core/include/evntthrd.h"
#include "utlgpu.h"
#include "utlkwargsmgr.h"
#include "mdiag/tests/gpu/trace_3d/traceop.h"
#include "mdiag/utils/mmufaultbuffers.h"
#include "utltest.h"
#include "utldebug.h"

class UtlChannel;
class UtlTest;
class UtlVfTest;

// Macro to create and expose a UTL event to Python API
//  event : Python-side name of the event
//  desc  : description of the event for documentation
#define UTL_BIND_EVENT(event, desc)                                             \
    Utl ## event::Register(module);                                             \
    py::class_<Utl ## event> event(module, #event, desc);                       \
    event.def_static("AddCallback", &Utl ## event::AddCallback,                 \
        UTL_DOCSTRING(#event".AddCallback",                                     \
            "This static function registers an existing UTL script              \
            function to be called when the event triggers"));                   \
    event.def_readonly("args", &Utl ## event::m_Args);                          \
    event.def_static("CallbackCount", &Utl ## event::CallbackCount,             \
        UTL_DOCSTRING(#event".CallbackCount",                                   \
            "This static function returns the number of callbacks registered    \
            to the event"));                                                    \
    BaseUtlEvent::s_UtlEventFreeFuncs.push_back(Utl ## event::FreeCallbacks);   \


// Base class to track RM events associated with a given device/subdevice
struct RmEventData
{
    using CallbackFunc = void (*)(void*, void*, UINT32, UINT32, UINT32);

    RmEventData
    (
        LwRm* pLwRm,
        LwRm::Handle parent,
        GpuSubdevice* pGpuSubdev,
        UINT32 index,
        void* pData,
        UINT32 size
    );

    virtual ~RmEventData();

    static RmEventData* GetRmThreadEventData
    (
        LwRm* pLwRm,
        LwRm::Handle parent,
        GpuDevice* pGpuDevice,
        GpuSubdevice* pGpuSubdev,
        UINT32 index,
        EventThread::HandlerFunc handler,
        void* pData,
        UINT32 size
    );

    static RmEventData* GetRmCallbackEventData
    (
        LwRm* pLwRm,
        LwRm::Handle parent,
        GpuSubdevice* pGpuSubdev,
        UINT32 index,
        CallbackFunc handler,
        void* arg,
        void *pData,
        UINT32 size
    );

    static void FreeAllEventData();
    static void FreeLwRmEventData(LwRm* pLwRm);

    LwRm::Handle m_Handle;
    LwRm::Handle m_Parent;
    LwRm* m_pLwRm;
    GpuSubdevice* m_pGpuSubdev;
    UINT32 m_Index;
    LwRm::Handle m_SubdevHandle;

    // Keep it for RmThreadEventData
    unique_ptr<EventThread> m_pEventThread;
    vector<UINT08> m_Data;

    static vector<unique_ptr<RmEventData>> s_RmEventDatas;

protected:
    enum EventNotificationActionType
    {
        REPEAT = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
        SINGLE = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_SINGLE,
        DISABLE = LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE
    };

    RC SetEventNotification(EventNotificationActionType action);

private:
    static RmEventData* MatchExistingRmEventData
    (
        LwRm::Handle parent,
        GpuSubdevice* pGpuSubdev,
        UINT32 index
    );
};

// Tracks RM events associated with a given device/subdevice, including the
// thread that will create and trigger UTL event associated with the RM event.
struct RmThreadEventData : public RmEventData
{
    RmThreadEventData
    (
        LwRm* pLwRm,
        LwRm::Handle parent,
        GpuDevice* pGpuDevice,
        GpuSubdevice* pGpuSubdev,
        UINT32 index,
        void* pData,
        UINT32 size
    );

    ~RmThreadEventData();

    GpuDevice* m_pGpuDevice;
};

// Tracks RM events associated with a given subdevice,
// including the callback function
struct RmCallbackEventData : public RmEventData
{
    RmCallbackEventData
    (
        LwRm* pLwRm,
        LwRm::Handle parent,
        GpuSubdevice* pGpuSubdev,
        UINT32 index,
        CallbackFunc handler,
        void* arg, // reserved for arg checking in the future
        void *pData,
        UINT32 size
    );

    ~RmCallbackEventData();

    LWOS10_EVENT_KERNEL_CALLBACK_EX m_CallbackParams;
};

class BaseUtlEvent
{
public:
    static void Register(py::module module);
    static void FreeCallbacks();

protected:
    static vector<void (*)()> s_UtlEventFreeFuncs;
};

// This template class is used as the base class for all UTL events.
// Every type of event that can happen in UTL will be represented with a
// class that is derived from the UtlEvent template.
//
// Users can register callback functions to event types through the
// AddCallback function.  In addition to registering a Python function,
// some event types also allow the user to specify filters so that the
// registered Python function is only called for some instances of the event
// type.  For example, the event utl.MethodWriteEvent can be filtered by
// channel list so that a function passed to AddCallback is only called when
// the event happens on one of the channels in the list.
//
// The EventClass template parameter is the class that represents the event
// type.  This class must be derived from the UtlEvent template.  The
// CallbackFilters template parameter represents a list of zero or more classes
// that have functionality for filtering the callback function based on event
// data.  In the above utl.MethodWriteEvent example, the channel list filtering
// is handled by the UtlCallbackFilter::ChannelList class below.
//
template <class EventClass, class ...CallbackFilters>
class UtlEvent : public BaseUtlEvent
{
public:
    UtlEvent() = default;
    virtual ~UtlEvent() {}

    static void Register(py::module module)
    {
        auto unused = {0, (CallbackFilters::Register(module), 0)...};
        UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
        kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "func", true, "function to be called when event triggers");
        kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "args", false, "args to be passed to the callback function as event.args");

        (void)unused;
    }

    // This function can be used to add any event specific setup as part of 
    // AddCallback
    static void AddCallbackInit() {}

    // Save the callback information, which will include a callback function
    // plus optional filtering information depending on the CallbackFilters
    // template parameter.
    static void AddCallback(py::kwargs kwargs)
    {
        EventClass::AddCallbackInit();
        CallbackInfo info;
        info.ProcessKwargs(kwargs);
        s_Callbacks.push_back(std::move(info));
    }

    // This function triggers the event.  Every callback on the list will be
    // tested against any potential filter for that particular event.
    // All callbacks that are not filtered out will be called sequentially.
    UINT32 Trigger()
    {
        UINT32 count = 0;

        try
        {
            UtlGil::Acquire gil;

            for (const auto& info : s_Callbacks)
            {
                // Only trigger the callback function if the callback data
                // matches the event data.
                if (info.MatchesEvent(dynamic_cast<EventClass*>(this)))
                {
                    m_Args = info.m_Args;

                    // Temporarily register the callback function's script ID
                    // to this thread.  This ensures that any UTL API used by
                    // the callback function will operate in the correct scope.
                    Utl::Instance()->RegisterScriptIdToThread(info.m_ScriptId);

                    // Call the callback function.
                    info.m_Func(dynamic_cast<EventClass*>(this));

                    // Restore the previous script ID to the thread.
                    Utl::Instance()->UnregisterScriptIdFromThread();
                }
                ++count;
            }
        }

        // The Python interpreter can only return errors via exception.
        catch (const py::error_already_set& e)
        {
            UtlUtility::HandlePythonException(e.what());
        }

        // Return the number of function calls that were made.
        return count;
    }

    // Return the number of callback functions registered to this event.
    static UINT32 CallbackCount() { return s_Callbacks.size(); }

    // Free the memory associated with the callback info list.  This may
    // need to be done before the Python interpreter goes out of scope if
    // there is any Python memory used in the CallbackInfoClass.
    static void FreeCallbacks()
    {
        for (auto it = s_Callbacks.begin(); it != s_Callbacks.end(); ++it)
        {
            (*it).Free();
        }
        s_Callbacks.clear();
    }

    // Derived Event classes must implement this static function
    // for the purposes of keyword argument registration and tracking
    //      static string GetClassName() { return "Event"; }

    // All events have an optional argument for allowing the user to pass
    // additional data to the callback function when an event triggers.
    py::object m_Args;

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlEvent(UtlEvent&) = delete;
    UtlEvent& operator=(UtlEvent&) = delete;

    // The CallbackInfo class is used to store the information passed to the
    // AddCallback function.  This will include a Python function, optional
    // user arguments, and any optional filtering information.
    class CallbackInfo : public CallbackFilters...
    {
    public:
        CallbackInfo() = default;

        // This function is used to process all of the arguments to the
        // AddCallback function in the UtlEvent base class.
        void ProcessKwargs(py::kwargs kwargs)
        {
            // Every event must have a Python callback function.
            m_Func = UtlUtility::GetRequiredKwarg<py::function>(kwargs, "func", (EventClass::GetClassName()+".AddCallback").c_str());

            if (m_Func.is_none())
            {
                UtlUtility::PyError("func parameter is not a valid Python function");
            }

            // Every event has an optional Python object which can be used
            // to pass extra data to the callback function.
            UtlUtility::GetOptionalKwarg<py::object>(&m_Args, kwargs, "args", (EventClass::GetClassName()+".AddCallback").c_str());

            // Each class passed to the CallbackFilters template parameter
            // defines a ProcessKwargs function which is used to extract one
            // piece of filter information the user may have passed to the
            // AddCallback function.  In order to call each ProcessKwargs
            // function, we construct an unused list which calls the
            // ProcessKwargs function of every class in the CallbackFilters.
            // Note that the first zero is needed because CallbackFilters can
            // be empty.  The second zero is needed because ProcessKwargs
            // does not return a value.
            // (This initializer list trick is used because the current gcc
            // version of MODS does not support C++17 fold expressions.)
            auto unused = {0, (CallbackFilters::ProcessKwargs(kwargs), 0)...};

            // The above list is unused, so cast to void to prevent a warning.
            (void)unused;

            // Save the current script id so that when the callback function
            // is triggered any UTL APIs used will have the correct scope.
            m_ScriptId = Utl::Instance()->GetScriptId();
        }

        // This function is used to check the optional callback filters to see
        // if the event matches the filters.
        bool MatchesEvent(const EventClass* event) const
        {
            // Each class passed to the CallbackFilters template parameter
            // needs to be checked to make sure the event is compatible with
            // the filter data.  The following creates a list of boolean values
            // based on the results of each MatchesEvent call.  If any of the
            // values are false, then the callback function should not be
            // called.  Note that a true value is added at the beginning of the
            // list because CallbackFilters can be empty.
            // (This initializer list trick is used because the current gcc
            // version of MODS does not support C++17 fold expressions.)
            auto results = { true, CallbackFilters::MatchesEvent(event)... };
            return (std::count(results.begin(), results.end(), false) == 0);
        }

        void Free()
        {
            m_Func = py::none();
            m_Args = py::none();
            auto unused = {0, (CallbackFilters::Free(), 0)...};

            (void)unused;
        }

        py::function m_Func;
        py::object m_Args = py::none();
        UINT32 m_ScriptId;
    };

private:
    // Stores all callback information registered by AddCallback.
    // There will be one list for each event type.
    static vector<CallbackInfo> s_Callbacks;
};

// This namespace is used to store all of the classes that can be passed to the
// CallbackFilters parameter of the UtlEvent class above.
namespace UtlCallbackFilter
{
    template <class EventClass>
    struct Test
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "test", false, "user test of interest");
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            if (UtlUtility::GetOptionalKwarg<UtlTest*>(&m_Test, kwargs, "test", (EventClass::GetClassName() + ".AddCallback").c_str()))
            {
                if (m_Test == nullptr)
                {
                    UtlUtility::PyError("test parameter is not a valid utl.Test object");
                }
            }
            else
            {
                // Get a test pointer based on the current script ID.  Any
                // scripts with test-only scope will automatically be filtered
                // by the test associated with that script.
                m_Test = Utl::Instance()->GetTestFromScriptId();
            }
        }

        bool MatchesEvent(const EventClass* event) const
        {
            // If the user registered the callback function for a specific test,
            // don't call the function if the current test doesn't match.
            return ((m_Test == nullptr) || (m_Test == event->m_Test));
        }

        void Free() {};
    private:
        UtlTest* m_Test = nullptr;
    };

    template <class EventClass>
    struct FileName
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "fileName", false, "fileName of interest");
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            if (UtlUtility::GetOptionalKwarg<string>(&m_FileName, kwargs, "fileName", (EventClass::GetClassName() + ".AddCallback").c_str()) &&
                m_FileName.empty())
            {
                UtlUtility::PyError("fileName parameter is not a valid string");
            }
        }

        bool MatchesEvent(const EventClass* event) const
        {
            // If the user registered the callback function for a specific
            // fileName, don't call the function if the fileName doesn't match.
            if (!m_FileName.empty() && (m_FileName != event->m_FileName))
            {
                return false;
            }
            return true;
        }

        void Free() {};
    private:
        string m_FileName;
    };

    template <class EventClass>
    struct EventName
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "eventName", false, "name of event to respond to");
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            if (UtlUtility::GetOptionalKwarg<string>(&m_EventName, kwargs,
                    "eventName", (EventClass::GetClassName() + ".AddCallback").c_str())
                    && m_EventName.empty())
            {
                UtlUtility::PyError("eventName parameter is not a valid string");
            }
        }

        bool MatchesEvent(const EventClass* event) const
        {
            // If the user registered the callback function for a specific event
            // name, don't call the function if the event name doesn't match.
            if (!m_EventName.empty() && (m_EventName != event->m_EventName))
            {
                return false;
            }
            return true;
        }

        void Free() {};

        string m_EventName;
    };

    template <class EventClass>
    struct ChannelList
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "channelList", false, "list of affected channel/s");
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            if (UtlUtility::GetOptionalKwarg<vector<UtlChannel*>>(
                    &m_ChannelList, kwargs, "channelList",
                    (EventClass::GetClassName() + ".AddCallback").c_str()) &&
                (m_ChannelList.size() == 0))
            {
                UtlUtility::PyError("channelList parameter is not a valid list of utl.Channel objects");
            }

            // If the current script has test-only scope, save the test
            // pointer so that channels from other tests will not trigger
            // this callback function.
            m_ScriptIdTest = Utl::Instance()->GetTestFromScriptId();
        }

        bool MatchesEvent(const EventClass* event) const
        {
            if ((m_ChannelList.size() > 0) &&
                (find(m_ChannelList.begin(), m_ChannelList.end(),
                    event->m_Channel) == m_ChannelList.end()))
            {
                return false;
            }

            // If the callback function was from a script with test-only scope,
            // only call the function for channels associated with that test.
            if (m_ScriptIdTest != nullptr)
            {
                MASSERT(m_ScriptIdTest->GetChannels().size() > 0);
                if (find(m_ScriptIdTest->GetChannels().begin(),
                    m_ScriptIdTest->GetChannels().end(),
                    event->m_Channel) == m_ScriptIdTest->GetChannels().end())
                {
                    return false;
                }
            }
            return true;
        }

        void Free() {};

        vector<UtlChannel*> m_ChannelList;
        UtlTest* m_ScriptIdTest;
    };

    template <class EventClass>
    struct AfterWrite
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "afterWrite", false, "if set to True, callback will be triggered after writing the method , else before the method is written");
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            UtlUtility::GetOptionalKwarg<bool>(&m_AfterWrite, kwargs, "afterWrite",
                (EventClass::GetClassName() + ".AddCallback").c_str());
        }

        bool MatchesEvent(const EventClass* event) const
        {
            // If the user registered the callback function for a specific test,
            // don't call the function if the current test doesn't match.
            return (m_AfterWrite == event->m_AfterWrite);
        }

        void Free() {};

        bool m_AfterWrite = false;
    };

    // Used as a parent class for setting up callbacks on RM interrupts. The
    // assocaiated EventClass must provide a constructor that takes a
    // RmEventData pointer.
    template <class EventClass>
    struct RmIntEvent
    {
        static void Register(py::module module) {}

        void Free()
        {
            m_pRmEventData = nullptr;
        }

        RmIntEvent()
        {
            m_pRmEventData = nullptr;
        }

        RmIntEvent(RmIntEvent && rmIntEvent)
        {
            m_pRmEventData = rmIntEvent.m_pRmEventData;
        }

        static void EventHandler(void *data)
        {
            try 
            {
                RmEventData* pRmEventData = static_cast<RmEventData*>(data);
                EventClass event(pRmEventData);
                event.Trigger();
            }
            catch (const py::error_already_set& e)
            {
                UtlUtility::HandlePythonException(e.what());
            }
        }

        static void CallbackFunction(void *pArg, void *pData, UINT32 eventHandle, UINT32 data, UINT32 status)
        {
            try 
            {
                EventClass event(eventHandle);
                event.Trigger();
            }
            catch (const py::error_already_set& e)
            {
                UtlUtility::HandlePythonException(e.what());
            }
        }

        bool MatchesEvent(const EventClass* event) const
        {
            return (event->m_Handle == m_pRmEventData->m_Handle);
        }

        RmEventData* m_pRmEventData;
    };

    template <class EventClass>
    struct EngineNonStallInt : public RmIntEvent<EventClass>
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "channel", true, "target channel");
        }

        EngineNonStallInt() : RmIntEvent<EventClass>()
        {
            m_Channel = nullptr;
        }

        void Free()
        {
            RmIntEvent<EventClass>::Free();
        }

        EngineNonStallInt(EngineNonStallInt &&engineNonStallInt)
            : RmIntEvent<EventClass>(move(engineNonStallInt))
        {
            m_Channel = engineNonStallInt.m_Channel;
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            m_Channel = UtlUtility::GetRequiredKwarg<UtlChannel*>(kwargs, "channel", 
                (EventClass::GetClassName() + ".AddCallback").c_str());
            LwRm* pLwRm = m_Channel->GetLwRmPtr();
            LWGpuResource * pGpuRes = m_Channel->GetLwGpuChannel()->GetLWGpu();
            GpuSubdevice * pGpuSubDev = pGpuRes->GetGpuSubdevice();
            LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(pGpuSubDev);
            UINT32 notifier = MDiagUtils::GetRmNotifierByEngineType(
                m_Channel->GetEngineId(), pGpuSubDev, pLwRm);
            UINT32 index = notifier | LW01_EVENT_NONSTALL_INTR;
            GpuDevice * pGpuDev = pGpuRes->GetGpuDevice();
            RmIntEvent<EventClass>::m_pRmEventData = RmEventData::GetRmThreadEventData(
                pLwRm, subdevHandle, pGpuDev, pGpuSubDev, index,
                RmIntEvent<EventClass>::EventHandler, this, 0);
        }

        UtlChannel* m_Channel;
    };

    template <class EventClass>
    struct PoisonErrorEvent : public RmIntEvent<EventClass>
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "channel", true, "target channel");
        }

        PoisonErrorEvent() : RmIntEvent<EventClass>()
        {
            m_Channel = nullptr;
        }

        void Free()
        {
            RmIntEvent<EventClass>::Free();
        }

        PoisonErrorEvent(PoisonErrorEvent &&poisonErrorEvent)
            : RmIntEvent<EventClass>(move(poisonErrorEvent))
        {
            m_Channel = poisonErrorEvent.m_Channel;
        }

        virtual UINT32 GetIndex() = 0;

        void ProcessKwargs(py::kwargs kwargs)
        {
            m_Channel = UtlUtility::GetRequiredKwarg<UtlChannel*>(kwargs, "channel", 
                (EventClass::GetClassName() + ".AddCallback").c_str());
            LwRm* pLwRm = m_Channel->GetLwRmPtr();
            LWGpuResource * pGpuRes = m_Channel->GetLwGpuChannel()->GetLWGpu();
            GpuSubdevice * pGpuSubDev = pGpuRes->GetGpuSubdevice();
            LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(pGpuSubDev);
            GpuDevice * pGpuDev = pGpuRes->GetGpuDevice();
            RmIntEvent<EventClass>::m_pRmEventData = RmEventData::GetRmThreadEventData(
                pLwRm, subdevHandle, pGpuDev, pGpuSubDev, GetIndex(),
                RmIntEvent<EventClass>::EventHandler, this, 0);
        }

        UtlChannel* m_Channel;
    };

    template <class EventClass>
    struct NonFatalPoisonError : public PoisonErrorEvent<EventClass>
    {
        /* virtual */ UINT32 GetIndex()
        {
            return LW2080_NOTIFIERS_POISON_ERROR_NON_FATAL;
        }
    };

    template <class EventClass>
    struct FatalPoisonError : public PoisonErrorEvent<EventClass>
    {
        /* virtual */ UINT32 GetIndex()
        {
            return LW2080_NOTIFIERS_POISON_ERROR_FATAL;
        }
    };

    template <class EventClass>
    struct SecFault : public RmIntEvent<EventClass>
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName() + ".AddCallback").c_str(),
                "gpu", false, "The GPU on which the fault oclwrs. Defaults to GPU 0.");
        }

        SecFault() : RmIntEvent<EventClass>()
        {
            m_Gpu = nullptr;
        }

        void Free()
        {
            RmIntEvent<EventClass>::Free();
        }

        SecFault(SecFault &&secFault)
            : RmIntEvent<EventClass>(move(secFault))
        {
            m_Gpu = secFault.m_Gpu;
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&m_Gpu, kwargs, "gpu",
                (EventClass::GetClassName() + ".AddCallback").c_str()))
            {
                m_Gpu = UtlGpu::GetGpus()[0];
            }
            LwRm* pLwRm = LwRmPtr().Get();
            GpuSubdevice* pGpuSubDev = m_Gpu->GetGpuSubdevice();
            LwRm::Handle subdevHandle = pLwRm->GetSubdeviceHandle(pGpuSubDev);
            RmIntEvent<EventClass>::m_pRmEventData = RmEventData::GetRmCallbackEventData(
                pLwRm, subdevHandle, pGpuSubDev,
                LW2080_NOTIFIERS_SEC_FAULT_ERROR,
                RmIntEvent<EventClass>::CallbackFunction,
                nullptr, this, 0);
        }

        UtlGpu* m_Gpu;
    };

    // Responsible for obtaining the UtlGpu argument for any UTL fault event.
    template <class EventClass>
    struct UtlFault : public RmIntEvent<EventClass>
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName() + ".AddCallback").c_str(),
                "gpu", false, "The GPU on which the fault oclwrs. Defaults to GPU 0.");
        }

        UtlFault() : RmIntEvent<EventClass>()
        {
            m_Gpu = nullptr;
        }

        UtlFault(UtlFault &&other)
            : RmIntEvent<EventClass>(move(other))
        {
            m_Gpu = other.m_Gpu;
        }

        void Free()
        {
            RmIntEvent<EventClass>::Free();
        }

        virtual UINT32 GetIndex() = 0;

        virtual void ProcessKwargs(py::kwargs kwargs)
        {
            if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&m_Gpu, kwargs, "gpu",
                (EventClass::GetClassName() + ".AddCallback").c_str()))
            {
                m_Gpu = UtlGpu::GetGpus()[0];
            }
            LwRm* pLwRm = LwRmPtr().Get();
            GpuSubdevice* pGpuSubdevice = m_Gpu->GetGpuSubdevice();
            GpuDevice* pGpuDevice = m_Gpu->GetGpudevice();

            SubdeviceShadowFaultBuffer* pShadowBuffer = SubdeviceShadowFaultBuffer::GetShadowFaultBuffer(pGpuSubdevice);
            MASSERT(pShadowBuffer);

            SubdeviceFaultBuffer* pSubdeviceFaultBuffer = pShadowBuffer->GetFaultBuffer();
            MASSERT(pSubdeviceFaultBuffer);

            RmIntEvent<EventClass>::m_pRmEventData = RmEventData::GetRmThreadEventData(
                pLwRm, pSubdeviceFaultBuffer->GetFaultBufferHandle(),
                pGpuDevice, pGpuSubdevice, GetIndex(),
                RmIntEvent<EventClass>::EventHandler, this, 0);

            MDiagUtils::RegisterFaultIndexInUtl(GetIndex());
        }

        UtlGpu* m_Gpu;
    };

    template <class EventClass>
    struct NonReplayableFaultOverflow : public UtlFault<EventClass>
    {
        /* virtual */ UINT32 GetIndex()
        {
            return LWC369_NOTIFIER_MMU_FAULT_ERROR;
        }
    };

    template <class EventClass>
    struct NonReplayableFault : public UtlFault<EventClass>
    {
        /* virtual */ UINT32 GetIndex()
        {
            return LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE;
        }
    };

    template <class EventClass>
    struct ReplayableFault : public UtlFault<EventClass>
    {
        UINT32 m_FaultBufferSize;
        UINT32 m_MaxFaultBufferEntries;
        UINT32 *m_FaultBufferEntries;
        UINT32 m_GetIndex;
        UINT32 m_PutIndex;

        /* virtual */ UINT32 GetIndex()
        {
            return LWC369_NOTIFIER_MMU_FAULT_REPLAYABLE;
        }

        /* virtual */ void ProcessKwargs(py::kwargs kwargs)
        {
            if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&(UtlFault<EventClass>::m_Gpu), kwargs, "gpu",
                (EventClass::GetClassName() + ".AddCallback").c_str()))
            {
                UtlFault<EventClass>::m_Gpu = UtlGpu::GetGpus()[0];
            }

            LwRm* pLwRm = LwRmPtr().Get();
            GpuSubdevice* pGpuSubdevice = UtlFault<EventClass>::m_Gpu->GetGpuSubdevice();
            GpuDevice* pGpuDevice = UtlFault<EventClass>::m_Gpu->GetGpudevice();

            SubdeviceFaultBuffer* pSubdeviceFaultBuffer = SubdeviceFaultBuffer::GetFaultBuffer(pGpuSubdevice);
            MASSERT(pSubdeviceFaultBuffer);

            UINT32 faultBufferSize = LWC369_BUF_SIZE;
            pSubdeviceFaultBuffer->GetFaultBufferSize(&m_FaultBufferSize);
            m_MaxFaultBufferEntries = m_FaultBufferSize / faultBufferSize;
            pSubdeviceFaultBuffer->MapFaultBuffer(0, m_FaultBufferSize, (void**)&m_FaultBufferEntries, 0, pGpuSubdevice);
            pSubdeviceFaultBuffer->GetFaultBufferGetIndex(&m_GetIndex);

            RmIntEvent<EventClass>::m_pRmEventData = RmEventData::GetRmThreadEventData(
                pLwRm, pSubdeviceFaultBuffer->GetFaultBufferHandle(),
                pGpuDevice, pGpuSubdevice, GetIndex(),
                RmIntEvent<EventClass>::EventHandler, this, sizeof(ReplayableFault));

            MDiagUtils::RegisterFaultIndexInUtl(GetIndex());
        }
    };

    template <class EventClass>
    struct NonReplayableFaultInPrivs : public UtlFault<EventClass>
    {
        /* virtual */ UINT32 GetIndex()
        {
            return LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE_IN_PRIV;
        }
    };

    template <class EventClass>
    struct AccessCounterNotification : public UtlFault<EventClass>
    {
        /* virtual */ UINT32 GetIndex()
        {
            return LWC365_NOTIFIERS_ACCESS_COUNTER;
        }

        /* virtual */ void ProcessKwargs(py::kwargs kwargs)
        {
            RC rc;

            if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&(UtlFault<EventClass>::m_Gpu), kwargs, "gpu",
                (EventClass::GetClassName() + ".AddCallback").c_str()))
            {
                UtlFault<EventClass>::m_Gpu = UtlGpu::GetGpus()[0];
            }

            LwRm* pLwRm = LwRmPtr().Get();
            GpuSubdevice* pGpuSubdevice = UtlFault<EventClass>::m_Gpu->GetGpuSubdevice();
            GpuDevice* pGpuDevice = UtlFault<EventClass>::m_Gpu->GetGpudevice();

            UINT32 accessCounterBufferAllocParams = 0;
            LwRm::Handle accessCounterBufferHandle;

            // Allocate the access counter buffer object.
            rc = pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pGpuSubdevice),
                        &accessCounterBufferHandle,
                        ACCESS_COUNTER_NOTIFY_BUFFER,
                        &accessCounterBufferAllocParams);
            UtlUtility::PyErrorRC(rc, "AddCallback failed : Can't allocate the access counter buffer object.");

            RmIntEvent<EventClass>::m_pRmEventData = RmEventData::GetRmThreadEventData(
                    pLwRm, accessCounterBufferHandle,
                    pGpuDevice, pGpuSubdevice, GetIndex(),
                    RmIntEvent<EventClass>::EventHandler, this, 0);

            LWC365_CTRL_ACCESS_CNTR_BUFFER_ENABLE_PARAMS notificationParams = {};
            notificationParams.intrOwnership = LWC365_CTRL_ACCESS_COUNTER_INTERRUPT_OWNERSHIP_NO_CHANGE;
            notificationParams.enable = true;

            rc = pLwRm->Control(accessCounterBufferHandle,
                        LWC365_CTRL_CMD_ACCESS_CNTR_BUFFER_ENABLE,
                        (void*)&notificationParams,
                        sizeof(notificationParams));
            UtlUtility::PyErrorRC(rc, "AddCallback failed : Can't enable access counter notification.");
        }
    };

    template <class EventClass>
    struct TraceOpList
    {
        static void Register(py::module module)
        {
            UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "type", false, "TraceOp type need be handled");
            kwargs->RegisterKwarg((EventClass::GetClassName()+".AddCallback").c_str(), "point", false, "TraceOp trigger point to handle");
        }

        void ProcessKwargs(py::kwargs kwargs)
        {
            UtlUtility::GetOptionalKwarg<TraceOpType>(&m_Type, kwargs,
                "type", (EventClass::GetClassName()+".AddCallback").c_str());
            UtlUtility::GetOptionalKwarg<UINT32>(&m_Point, kwargs,
                "point", (EventClass::GetClassName()+".AddCallback").c_str());
        }

        bool MatchesEvent(const EventClass* event) const
        {
            if ((m_Type != TraceOpType::Unknown) &&
                (event->GetType() != m_Type))
            {
                return false;
            }
            else if ((m_Point != static_cast<UINT32>(TraceOpTriggerPoint::None)) &&
                !TraceOp::HasTriggerPoint(m_Point, event->GetTriggerPoint()))
            {
                return false;
            }
            return true;
        }

        void Free() {}

    private:
        TraceOpType m_Type = TraceOpType::Unknown;
        UINT32 m_Point = UINT32(TraceOpTriggerPoint::None);
    };
};

// This event is triggered after global UTL scripts are imported and
// before the UtlStartEvent is triggered.  (In this context, global UTL scripts
// are ones specified by the -utl_script command-line argument.)  This event is
// also triggered once for each script with test-only scope.  (Scripts with
// test-only scope are specified by the -utl_test_script command-line argument.
//
// The Test callback filter is used to ensure that the InitEvent callbacks are
// only triggered once for each script, whether the script has global scope or
// test-only scope.  This test filtering is handled by MODS rather than the
// user, so the UtlCallbackFilter::Test template is specialized for the
// InitEvent class (see utlevent.cpp).
//
class UtlInitEvent :
    public UtlEvent<UtlInitEvent,
                    UtlCallbackFilter::Test<UtlInitEvent>>
{
public:
    UtlInitEvent(UtlTest* test) : m_Test(test) {};
    static string GetClassName() { return "InitEvent"; }
    UtlTest* m_Test = nullptr;
};

// This event is triggered after a global UtlInitEvent (see above) and is meant
// to represent when tests are run.  If a user registers a callback function
// for this event, the normal mdiag test flow won't be used.
//
class UtlStartEvent : public UtlEvent<UtlStartEvent>
{
public:
    static string GetClassName() { return "StartEvent"; }
};

// This event is triggered after a GPU partition is created via a UTL script
//
class UtlSmcPartitionCreatedEvent : public UtlEvent<UtlSmcPartitionCreatedEvent>
{
public:
    static string GetClassName() { return "SmcPartitionCreatedEvent"; }
    UtlSmcPartitionCreatedEvent(UtlGpuPartition* pPartition);
    UtlGpuPartition* m_Partition = nullptr;
};

// This event represents the end of the UTL flow.  It is triggered after
// all UtlStartEvent callbacks are finished.  This event can also be used
// to allow a UTL script to change the final test status, allowing
// a UTL script to perform its own checks in addition to the usual end-of-test
// checking that MODS does (CRC checking, interrupt checking, etc.)
//
class UtlEndEvent : public UtlEvent<UtlEndEvent>
{
public:
    static string GetClassName() { return "EndEvent"; }
    Test::TestStatus GetStatus() const { return m_Status; }
    void SetStatus(Test::TestStatus status);

    Test::TestStatus m_Status;
};

// This event is used to report method writes to channels.  The AddCallback
// function allows optional arguments to 1) restrict the event to a list
// of channels, and 2) specify whether to trigger the event before or after
// the actual method write.
//
class UtlMethodWriteEvent :
    public UtlEvent<UtlMethodWriteEvent,
                    UtlCallbackFilter::ChannelList<UtlMethodWriteEvent>,
                    UtlCallbackFilter::AfterWrite<UtlMethodWriteEvent>>
{
public:
    UtlMethodWriteEvent() = default;
    static string GetClassName() { return "MethodWriteEvent"; }

    py::tuple GetGroupInfo();

    // These fields are set on the event object that is passed to the
    // callback function that was registered by the UTL script.
    UtlChannel* m_Channel = nullptr;
    UINT32 m_Method;
    UINT32 m_Data;
    UINT32 m_HwClass;
    UINT32 m_MethodsWritten;
    UINT32 m_TestMethodsWritten;
    bool m_AfterWrite;
    bool m_IsTestMethod;
    UINT32 m_GroupIndex;
    UINT32 m_GroupCount;
    bool m_SkipWrite = false;
};

// This event is used to notify a UTL script that a given channel is being 
// reset. 
// 
class UtlChannelResetEvent :
    public UtlEvent<UtlChannelResetEvent,
                    UtlCallbackFilter::ChannelList<UtlChannelResetEvent>>
{
public:
    UtlChannelResetEvent(UtlChannel* pUtlChannel);
    static string GetClassName() { return "ChannelResetEvent"; }
    UtlChannel* m_Channel = nullptr;
};

// This event is used to notify a UTL script that a given test has just
// been created.  This can be used by scripts that don't register a callback
// function for the StartEvent because those scripts won't be creating Test
// objects directly.
//
class UtlTestCreatedEvent :
    public UtlEvent<UtlTestCreatedEvent,
                    UtlCallbackFilter::Test<UtlTestCreatedEvent>>
{
public:
    UtlTestCreatedEvent(UtlTest* pTest);
    static string GetClassName() { return "TestCreatedEvent"; }
    UtlTest* m_Test = nullptr;
};

// This event is used to notify a UTL script that a given test has finished
// exelwting all the methods and is just before the CRC-checkign stage.
//
class UtlTestMethodsDoneEvent :
    public UtlEvent<UtlTestMethodsDoneEvent,
                    UtlCallbackFilter::Test<UtlTestMethodsDoneEvent>>
{
public:
    UtlTestMethodsDoneEvent(UtlTest* pTest);
    static string GetClassName() { return "TestMethodsDoneEvent"; }
    UtlTest* m_Test = nullptr;
};

// This event is used to notify a UTL script that a given test is about
// to enter its cleanup phase.
//
class UtlTestCleanupEvent :
    public UtlEvent<UtlTestCleanupEvent,
                    UtlCallbackFilter::Test<UtlTestCleanupEvent>>
{
public:
    UtlTestCleanupEvent(UtlTest* pTest);
    static string GetClassName() { return "TestCleanupEvent"; }
    UtlTest* m_Test = nullptr;
};

// This event is used to notify a UTL script that a named trace3d event
// has oclwred.
//
//
class UtlTrace3dEvent :
    public UtlEvent<UtlTrace3dEvent,
                    UtlCallbackFilter::Test<UtlTrace3dEvent>,
                    UtlCallbackFilter::EventName<UtlTrace3dEvent>>
{
public:
    UtlTrace3dEvent(UtlTest* pTest, const string& eventName);
    static string GetClassName() { return "Trace3dEvent"; }

    UtlTest* m_Test = nullptr;
    string m_EventName;
};

// This event is used to notify a UTL script that a Methods traceOp 
// would go to execute.
//
class UtlBeforeSendPushBufHeaderEvent :
    public UtlEvent<UtlBeforeSendPushBufHeaderEvent,
                    UtlCallbackFilter::Test<UtlBeforeSendPushBufHeaderEvent>>
{
public:
    UtlBeforeSendPushBufHeaderEvent(UtlTest * pTest);
    static string GetClassName() { return "BeforeSendPushBufHeaderEvent"; }

    UtlTest* m_Test = nullptr;
    string m_PushBufName;
    UINT32 m_PushBufOffset;
};

// This event is used to notify a UTL script that a NonStall interrupt 
// has been oclwred.
//
class UtlEngineNonStallIntEvent :
    public UtlEvent<UtlEngineNonStallIntEvent,
                    UtlCallbackFilter::EngineNonStallInt<UtlEngineNonStallIntEvent>>
{
public:
    UtlEngineNonStallIntEvent(RmEventData* pEventData);
    static string GetClassName() { return "EngineNonStallIntEvent"; }

    LwRm::Handle m_Handle;
};

// This event is used to notify a UTL script that a non fatal poison error 
// has been oclwred.
//
class UtlNonFatalPoisonErrorEvent :
    public UtlEvent<UtlNonFatalPoisonErrorEvent,
                    UtlCallbackFilter::NonFatalPoisonError<UtlNonFatalPoisonErrorEvent>>
{
public:
    UtlNonFatalPoisonErrorEvent(RmEventData* pEventData);
    static string GetClassName() { return "NonFatalPoisonErrorEvent"; }

    LwRm::Handle m_Handle;
};

// This event is used to notify a UTL script that a fatal poison error
// has been oclwred.
//
class UtlFatalPoisonErrorEvent :
    public UtlEvent<UtlFatalPoisonErrorEvent,
                    UtlCallbackFilter::FatalPoisonError<UtlFatalPoisonErrorEvent>>
{
public:
    UtlFatalPoisonErrorEvent(RmEventData* pEventData);
    static string GetClassName() { return "FatalPoisonErrorEvent"; }

    LwRm::Handle m_Handle;
};

// This event is triggered when a access counter notification oclwrs.
class UtlAccessCounterNotificationEvent :
    public UtlEvent<UtlAccessCounterNotificationEvent,
                    UtlCallbackFilter::AccessCounterNotification<UtlAccessCounterNotificationEvent>>
{
public:
    UtlAccessCounterNotificationEvent(RmEventData* pEventData);
    static string GetClassName() { return "AccessCounterNotificationEvent"; }
    LwRm::Handle m_Handle;
};

// This event is triggered when the capacity of the buffer of nonreplayable faults
// is exceeded.
class UtlNonReplayableFaultOverflowEvent : public UtlEvent<UtlNonReplayableFaultOverflowEvent,
        UtlCallbackFilter::NonReplayableFaultOverflow<UtlNonReplayableFaultOverflowEvent>>
{
public:
    UtlNonReplayableFaultOverflowEvent(RmEventData* pEventData);
    static string GetClassName() { return "NonReplayableFaultOverflowEvent"; }

    static void ForceBufferOverflow(py::kwargs kwargs);

    LwRm::Handle m_Handle;
    UtlGpu* m_pGpu;
};

// This event is triggered when non-replayable fault oclwrs.
class UtlNonReplayableFaultEvent : public UtlEvent<UtlNonReplayableFaultEvent,
        UtlCallbackFilter::NonReplayableFault<UtlNonReplayableFaultEvent>>
{
public:
    UtlNonReplayableFaultEvent(RmEventData* pEventData);
    static string GetClassName() { return "NonReplayableFaultEvent"; }

    LwRm::Handle m_Handle;
    UtlGpu* m_pGpu;
    SubdeviceShadowFaultBuffer* m_pShadowFaultBuffer;

    // Returns true if the fault buffer is empty.
    bool IsFaultBufferEmpty();

    // Returns the fault entry data.
    vector<UINT32> GetData();
};

// This event is triggered when replayable fault oclwrs.
class UtlReplayableFaultEvent : public UtlEvent<UtlReplayableFaultEvent,
        UtlCallbackFilter::ReplayableFault<UtlReplayableFaultEvent>>
{
public:
    UtlReplayableFaultEvent(RmEventData* pEventData);
    static string GetClassName() { return "ReplayableFaultEvent"; }

    LwRm::Handle m_Handle;
    UtlGpu* m_pGpu;
    SubdeviceFaultBuffer* m_pSubdeviceFaultBuffer;
    UtlCallbackFilter::ReplayableFault<UtlReplayableFaultEvent> *m_pReplayableFault;
    static bool UpdateFaultBufferGetIndex;

    // Returns the value of get index in replayable fault context.
    UINT32 GetGetIndex();

    // Updates the get index in replayable fault context. Note: It does NOT make an RM call update the get index.
    void SetGetIndex(UINT32 index);

    // Returns the value of put index in replayable fault context.
    UINT32 GetPutIndex();

    // Updates the put index in replayable fault context. Note: It does NOT make an RM call to update the put index.
    void SetPutIndex(UINT32 index);

    // Returns the subdevice fault buffer entry
    vector<UINT32> GetData();

    // Sets the entry in subdevice fault buffer to the specified data
    void SetData(py::list data);

    // Returns the maximum number of fault buffer entries in subdevice fault buffer
    UINT32 GetMaxFaultBufferEntries();

    // Returns true if the subdevice fault buffer has overflowed.
    bool IsFaultBufferOverflowed();

    // Updates the get index of subdevice fault buffer.
    void UpdateGetIndex(py::kwargs kwargs);

    // Enables the notifications on subdevice fault buffer.
    void EnableNotifications();

    // Makes an RM call to fetch the put index of subdevice fault buffer and update it in replayable fault context.
    void FetchPutIndex();

    // Sets if get index needs to be updated. Defaults to true.
    // Used for fault event buffer overflow testing.
    static void SetUpdateFaultBufferGetIndex(py::kwargs kwargs);

    // Returns true if get needs to be updated.
    static bool GetUpdateFaultBufferGetIndex();
};

// This event is triggered when a nonreplayable fault oclwrs, with the fault
// data held in the GPU registers rather than in the fault buffer.
class UtlNonReplayableFaultInPrivsEvent : public UtlEvent<UtlNonReplayableFaultInPrivsEvent,
        UtlCallbackFilter::NonReplayableFaultInPrivs<UtlNonReplayableFaultInPrivsEvent>>
{
public:
    UtlNonReplayableFaultInPrivsEvent(RmEventData* pEventData);
    static string GetClassName() { return "NonReplayableFaultInPrivsEvent"; }

    LwRm::Handle m_Handle;
    UtlGpu* m_pGpu;
};

// This event is used to notify a UTL script that a sec fault
// has been oclwred.
//
class UtlSecFaultEvent :
    public UtlEvent<UtlSecFaultEvent,
                    UtlCallbackFilter::SecFault<UtlSecFaultEvent>>
{
public:
    UtlSecFaultEvent(LwRm::Handle handle);
    static string GetClassName() { return "SecFaultEvent"; }

    LwRm::Handle m_Handle;
};

// This event is triggered when MODS detects test hangs somewhere
//
class UtlHangEvent : public UtlEvent<UtlHangEvent>
{
public:
    static string GetClassName() { return "HangEvent"; }
    static void AddCallbackInit() { UtlDebug::Init(); }
};

// This event is triggered when MODS wants to print data in tabular format
class UtlTablePrintEvent : public UtlEvent<UtlTablePrintEvent>
{
public:
    UtlTablePrintEvent(string&& tableHeader, vector<vector<string>>&& tableData, bool isDebug);
    static string GetClassName() { return "TablePrintEvent"; }
    string m_Header;
    vector<vector<string>> m_Data;
    bool m_IsDebug;
};

// This event is triggered when plugin/policy manager (via MODS) wants to trigger an Event 
// Plugin has an API: UtilityManager::TriggerUtlUserEvent()
// Policy Manager is unsupported right now
class UtlUserEvent : public UtlEvent<UtlUserEvent>
{
public:
    UtlUserEvent(map<string, string>&& userData);
    static string GetClassName() { return "UserEvent"; }
    map<string, string> m_UserData;
};

// This event is triggered when a trace file is read
//
class UtlTraceFileReadEvent : 
    public UtlEvent<UtlTraceFileReadEvent,
                    UtlCallbackFilter::Test<UtlTraceFileReadEvent>,
                    UtlCallbackFilter::FileName<UtlTraceFileReadEvent>>
{
public:
    UtlTraceFileReadEvent(UtlTest* pUtlTest, string fileName, void* data, size_t size);
    static string GetClassName() { return "TraceFileReadEvent"; }
    UtlSequence* GetDataSeq();
    UtlTest* m_Test;
    string m_FileName;
private:
    unique_ptr<UtlSequence> m_SeqData;
};

class UtlSurface;

// UTL TraceOp event handle
class UtlTraceOpEvent
    : public UtlEvent<UtlTraceOpEvent,
            UtlCallbackFilter::Test<UtlTraceOpEvent>,
            UtlCallbackFilter::TraceOpList<UtlTraceOpEvent>>
{
public:
    UtlTraceOpEvent(UtlTest* pTest, TraceOp* pOp);
    ~UtlTraceOpEvent() = default;
    TraceOpType GetType() const;
    UtlTest* GetTest() const { return m_Test; }
    TraceOp* GetOp() const { return m_pOp; }
    UINT32 GetLineNumber() const;
    TraceOpTriggerPoint GetTriggerPoint() const;
    UtlSurface* GetParameterSurface() const { return m_ParamSurf; }
    bool NeedSkip() const;
    void SetSkip(bool bSkip);
    const GpuTrace::HdrStrProps& GetProperties() const { return m_Props; }
    const GpuTrace::HdrStrProps& GetHdrStrProperties() const;
    static void RegisterAll(py::module module);
    static const string GetClassName();

public:
    UtlTest* m_Test = nullptr;

private:
    void SetOp(TraceOp* pOp);
    void InitProperties();

    GpuTrace::HdrStrProps m_Props;
    TraceOp* m_pOp = nullptr;
    UtlSurface* m_ParamSurf = nullptr;
};

class UtlSurfaceAllocationEvent :
    public UtlEvent<UtlSurfaceAllocationEvent,
                    UtlCallbackFilter::Test<UtlSurfaceAllocationEvent>>
{
public:
    UtlSurfaceAllocationEvent(UtlSurface* surface, UtlTest* test);
    static string GetClassName() { return "SurfaceAllocationEvent"; }
    UtlSurface* m_Surface = nullptr;
    UtlTest* m_Test = nullptr;
};

// This event is triggered when PF launches VFs. User registers a callback function
// to help PF to do some settings before VFs start to run test.
class UtlVfLaunchedEvent : public UtlEvent<UtlVfLaunchedEvent>
{
public:
    UtlVfLaunchedEvent(UtlVfTest* vfTest);
    static string GetClassName() { return "VfLaunchedEvent"; }
    UtlVfTest* m_VfTest = nullptr;
};

template<class EventClass, class ...CallbackFilters>
vector<typename UtlEvent<EventClass, CallbackFilters...>::CallbackInfo>
UtlEvent<EventClass, CallbackFilters...>::s_Callbacks;

#endif
