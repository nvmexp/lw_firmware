/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   jstasker.cpp
 *
 * This file contains JavaScript linkage for the Tasker.
 */

#include "core/include/tasker.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "jsapi.h"
#include "core/include/tee.h"
#include "core/include/cpu.h"
#include "core/include/utility.h"
#include <memory>

// Structure definition used to create a new thread from JS
struct JSThreadStruct
{
    JSFunction * jsFunc; // The JS function to run
    JsArray args; // An array of arguments to pass to the function
    volatile INT32 refCount; // Reference count to the structure
};

//------------------------------------------------------------------------------
//
// JavaScript linkage for Tasker:
//
//------------------------------------------------------------------------------

//! Controls threads created through the JavaScript interface.
/**
 * JSCaller creates a new JavaScript context, exelwtes a JSFunction within
 * that context, destroys that context, and returns.
 *
 * @param myargs A pointer to the JSThread structure which contains the
 *               JSFunction and any args to pass to the function.
 */
static void JSCaller(void *args)
{
    MASSERT(args != 0);

    class AutoReleaseJsThreadStruct
    {
    public:
        explicit AutoReleaseJsThreadStruct(void* arg)
        : m_pTS(static_cast<JSThreadStruct*>(arg))
        {
        }
        ~AutoReleaseJsThreadStruct()
        {
            if (Cpu::AtomicAdd(&m_pTS->refCount, -1) == 1)
            {
                if (!m_pTS->args.empty())
                {
                    JavaScriptPtr pJavaScript;
                    JSContext* threadCtx;
                    if (OK == pJavaScript->GetContext(&threadCtx))
                    {
                        for (size_t ii = 0; ii < m_pTS->args.size(); ii++)
                        {
                            JS_RemoveRoot(threadCtx, &m_pTS->args[ii]);
                        }
                    }
                    else
                    {
                       Printf(Tee::PriHigh, "Unable to RemoveRoot due to invalid context!!\n");
                    }
                }
                delete m_pTS;
            }
        }
        JSThreadStruct* operator->() const { return m_pTS; }
    private:
        JSThreadStruct* m_pTS;
    };

    AutoReleaseJsThreadStruct pJsThread(args);
    JavaScriptPtr pJavaScript;
    JSContext *threadCtx;

    if (OK == pJavaScript->GetContext(&threadCtx))
    {
       pJavaScript->CallMethod(threadCtx, pJsThread->jsFunc, pJsThread->args);
    }
    else
    {
       Printf(Tee::PriHigh, "Unable to run JS thread due to invalid context!!\n");
    }
}

JS_CLASS(Tasker);

//! JavaScript class for tasker/thread control.
/**
 * This class provides a JavaScript interface for basic thread control.
 * Only the most simple operations (new thread, join thread, and yield) are
 * lwrrently supported.
 */
static SObject Tasker_Object
(
    "Tasker",
    TaskerClass,
    0,
    0,
    "Thread manager."
);

//
// Script methods
//

C_(Tasker_CreateThread);
//! Create a new thread.
/**
 * @param arg1 The name of the new thread.
 * @param arg2 The JS function to control this thread.
 * @param arg3 (optional) Desired size of the new thread's stack, in bytes.
 * @param arg4 (optional) A JS array of arguments to be passed to the function
 *
 * @return Thread ID of newly created thread.
 */
SMethod Tasker_CreateThread
(
    Tasker_Object,
    "CreateThread",
    C_Tasker_CreateThread,
    4,
    "Create and start a new thread."
);

C_(Tasker_CreateThreadGroup);
//! Create a new thread group.
/**
 * @param arg1 The name for the new threads (thread index will be appended).
 * @param arg2 The JS function to control this thread.
 * @param arg3 Number of threads to create.
 * @param arg4 (optional) A JS array of arguments to be passed to the function
 *
 * @return Array of thread IDs of newly created threads.
 *
 * @sa Tasker::CreateThreadGroup
 */
SMethod Tasker_CreateThreadGroup
(
    Tasker_Object,
    "CreateThreadGroup",
    C_Tasker_CreateThreadGroup,
    4,
    "Create and start a group of new threads."
);

C_(Tasker_SetCpuAffinity);
//! Sets the CPU affinity for a tasker thread
STest Tasker_SetCpuAffinity
(
    Tasker_Object,
    "SetCpuAffinity",
    C_Tasker_SetCpuAffinity,
    2,
    "Sets the CPU affinity for a tasker thread"
 );

C_(Tasker_EnableThreadStats);
//! Enable thread statistics.
STest Tasker_EnableThreadStats
(
    Tasker_Object,
    "EnableThreadStats",
    C_Tasker_EnableThreadStats,
    0,
    "Enable collecting of thread statistics."
);

C_(Tasker_Yield);
//! Cause the current thread to yield the processor.
STest Tasker_Yield
(
    Tasker_Object,
    "Yield",
    C_Tasker_Yield,
    0,
    "Cause the current thread to yield the processor."
 );

C_(Tasker_WaitOnBarrier);
//! Cause the current thread to yield the processor.
STest Tasker_WaitOnBarrier
(
    Tasker_Object,
    "WaitOnBarrier",
    C_Tasker_WaitOnBarrier,
    0,
    "Block the current thread until all threads in its group call WaitOnBarrier."
 );

C_(Tasker_Join);
//! Cause the current thread to block until another thread dies.
/**
 * @param arg1 Thread ID or an array of Thread IDs of threads to join.
 */
STest Tasker_Join
(
    Tasker_Object,
    "Join",
    C_Tasker_Join,
    1,
    "Remove the current thread from the ready queue until another thread dies."
);

C_(Tasker_PrintAllThreads);
//! Print all threads that haven't yet died.
/**
 * @note If a thread has just died and has not yet
 *       had its resources reclaimed, it will appear
 *       in the printed list also.
 */
SMethod Tasker_PrintAllThreads
(
 Tasker_Object,
 "PrintAllThreads",
 C_Tasker_PrintAllThreads,
 0,
 "Print all living threads.  Could also print a dead thread yet to be collected."
);

C_(Tasker_StackBytesFree);
//! Return number of bytes free on stack of current thread.
SMethod Tasker_StackBytesFree
(
 Tasker_Object,
 "StackBytesFree",
 C_Tasker_StackBytesFree,
 0,
 "Return number of bytes free on stack of current thread."
);

P_(Tasker_Get_ThreadId);
static SProperty Tasker_ThreadId
(
    Tasker_Object,
    "ThreadId",
    0,
    Tasker_Get_ThreadId,
    0,
    JSPROP_READONLY,
    "Thread ID of the current running thread."
);

P_(Tasker_Get_ThreadIdx);
static SProperty Tasker_ThreadIdx
(
    Tasker_Object,
    "ThreadIdx",
    0,
    Tasker_Get_ThreadIdx,
    0,
    JSPROP_READONLY,
    "Thread index of the current thread in its group."
);

P_(Tasker_Get_GroupSize);
static SProperty Tasker_GroupSize
(
    Tasker_Object,
    "GroupSize",
    0,
    Tasker_Get_GroupSize,
    0,
    JSPROP_READONLY,
    "Number of threads in the group to which the current thread belongs."
);

P_(Tasker_Get_NumThreadsAliveInGroup);
static SProperty Tasker_NumThreadsAliveInGroup
(
    Tasker_Object,
    "NumThreadsAlive",
    0,
    Tasker_Get_NumThreadsAliveInGroup,
    0,
    JSPROP_READONLY,
    "Number of threads alive in the tasker group to which the current thread belongs."
);

P_(Tasker_Get_MaxHwPollHz);
P_(Tasker_Set_MaxHwPollHz);
static SProperty Tasker_MaxHwPollHz
(
    Tasker_Object,
    "MaxHwPollHz",
    0,
    0.0,
    Tasker_Get_MaxHwPollHz,
    Tasker_Set_MaxHwPollHz,
    0,
    "Max frequency for polling HW in Hertz (registers or device memory)."
);

PROP_CONST( Tasker, MIN_STACK_SIZE, Tasker::MIN_STACK_SIZE );
PROP_CONST( Tasker, NO_TIMEOUT, Tasker::NO_TIMEOUT );
PROP_READWRITE_NAMESPACE(Tasker, DefaultTimeoutMs, FLOAT64,
                         "Default mods timeout.");
PROP_READWRITE_NAMESPACE(Tasker, IgnoreCpuAffinityFailure, bool,
                         "Ignore errors when setting Cpu affinity fails.");

//
// Implementation of script methods
//
C_(Tasker_CreateThread)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    string threadName;
    JSFunction *threadFunc;
    JsArray args;
    unique_ptr<JSThreadStruct> pJsThread(new JSThreadStruct);
    pJsThread->refCount = 1;
    UINT32 stackSize = Tasker::MIN_STACK_SIZE;

    const char usage[] =
             "Usage: threadID = Tasker.CreateThread(threadName, threadFunc, "
             "[stackSize], [args])";

    if ( ((NumArguments != 2) && (NumArguments != 3) && (NumArguments != 4))
         || (OK != pJavaScript->FromJsval(pArguments[0], &threadName))
         || (OK != pJavaScript->FromJsval(pArguments[1], &threadFunc)) )
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Save off the JS function in a structure passed to our new thread
    pJsThread->jsFunc = threadFunc;

    if ( (3 <=  NumArguments)
         && (OK != pJavaScript->FromJsval(pArguments[2], &stackSize)) )
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    if ( (4 == NumArguments)
         && (OK != pJavaScript->FromJsval(pArguments[3], &args)) )
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Save off the JS args (if any) in a structure passed to our new thread
    pJsThread->args = args;

    //Protect the thread arguments from the Javascript Garbage Collector
    for (size_t ii = 0; ii < args.size(); ii++)
    {
        if (JS_TRUE != JS_AddRoot(pContext, &pJsThread->args[ii]))
        {
            JS_ReportError(pContext, "Could not AddRoot on args"
                                     " in Tasker.CreateThread()");
            return JS_FALSE;
        }
    }

    Tasker::ThreadID pid = Tasker::CreateThread(JSCaller,
                                               pJsThread.get(),
                                               stackSize,
                                               threadName.c_str());

    // Let the thread destroy the structure with its arguments
    if (pid != Tasker::NULL_THREAD)
    {
       pJsThread.release();
    }

    if ((pid == Tasker::NULL_THREAD) ||
        (OK != pJavaScript->ToJsval(pid, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error oclwred in Tasker.CreateThread().");
        *pReturlwalue = 0;
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(Tasker_CreateThreadGroup)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    string threadName;
    JSFunction* threadFunc;
    JsArray args;
    JsArray threadObjs;
    unique_ptr<JSThreadStruct> pJsThread(new JSThreadStruct);
    const char usage[] =
        "Usage: threadIDs = Tasker.CreateThreadGroup(threadName, threadFunc, "
        "[threadObjs], [args])";

    if (((NumArguments != 3) && (NumArguments != 4))
        || (OK != pJavaScript->FromJsval(pArguments[0], &threadName))
        || (OK != pJavaScript->FromJsval(pArguments[1], &threadFunc))
        || (OK != pJavaScript->FromJsval(pArguments[2], &threadObjs))
        || (threadObjs.size() == 0))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Save off the JS function in a structure passed to our new thread
    pJsThread->jsFunc = threadFunc;

    // Set reference count to the number of threads that will get this structure
    pJsThread->refCount = static_cast<INT32>(threadObjs.size());

    if ((4 == NumArguments)
        && (OK != pJavaScript->FromJsval(pArguments[3], &args)))
    {
        JS_ReportError(pContext, usage);
        return JS_FALSE;
    }

    // Save off the JS args (if any) in a structure passed to our new thread
    pJsThread->args = args;

    //Protect the thread arguments from the Javascript Garbage Collector
    for (size_t ii = 0; ii < args.size(); ii++)
    {
        if (JS_TRUE != JS_AddRoot(pContext, &pJsThread->args[ii]))
        {
            JS_ReportError(pContext, "Could not AddRoot on args"
                                     " in Tasker.CreateThreadGroup()");
            return JS_FALSE;
        }
    }

    void *pvThreadGroup;
    vector<Tasker::ThreadID> pids = Tasker::CreateThreadGroup
        (
                JSCaller,
                pJsThread.get(),
                static_cast<UINT32>(threadObjs.size()),
                threadName.c_str(),
                false,
                &pvThreadGroup
        );

    // Let the threads destroy the structure with its arguments
    if (!pids.empty())
    {
        pJsThread.release();
    }

    Tasker::StartOrDestroyThreadGroup sdtg(pvThreadGroup);
    // If all the threads were started as expected, then copy back out the
    // thread IDs onto the JS thread object.  This prevents the case where JS
    // can attempt to access a thread ID from a different thread before the
    // thread that created it can update the JS structure with the ID
    if (pids.size() == threadObjs.size())
    {
        for (size_t pidIdx = 0; pidIdx < pids.size(); pidIdx++)
        {
            JSObject *pThreadObj;
            if (OK != pJavaScript->FromJsval(threadObjs[pidIdx], &pThreadObj))
            {
                JS_ReportError(pContext, "Could not read thread object"
                                         " in Tasker.CreateThreadGroup()");
                return JS_FALSE;
            }
            if (OK != pJavaScript->SetProperty(pThreadObj, "Id",
                                               static_cast<INT32>(pids[pidIdx])))
            {
                JS_ReportError(pContext, "Could not set thread ID"
                                         " in Tasker.CreateThreadGroup()");
                return JS_FALSE;
            }
            if (OK != pJavaScript->ToJsval(pThreadObj, &threadObjs[pidIdx]))
            {
                JS_ReportError(pContext, "Could not write thread object"
                                         " in Tasker.CreateThreadGroup()");
                return JS_FALSE;
            }
        }
    }
    sdtg.SetStart(true);

    JsArray array;
    array.resize(pids.size());
    for (UINT32 i=0; i < array.size(); i++)
    {
        if (OK != pJavaScript->ToJsval(pids[i], &array[i]))
        {
                array.clear();
                break;
        }
    }

    if (pids.empty() || array.empty() ||
        (OK != pJavaScript->ToJsval(&array, pReturlwalue)))
    {
        JS_ReportError(pContext, "Error oclwred in Tasker.CreateThreadGroup().");
        *pReturlwalue = 0;
        return JS_FALSE;
    }

    return JS_TRUE;
}

C_(Tasker_SetCpuAffinity)
{
    JavaScriptPtr pJavaScript;
    JsArray jsCpuMasks;
    Tasker::ThreadID pid = Tasker::NULL_THREAD;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &pid)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &jsCpuMasks)))
    {
        JS_ReportError(pContext, "Usage: Tasker.SetCpuAffinity(pid, cpuMasks[])");
        return JS_FALSE;
    }

    RC rc;
    vector<UINT32> cpuMasks;
    C_CHECK_RC(pJavaScript->FromJsArray(jsCpuMasks, &cpuMasks));
    RETURN_RC(Tasker::SetCpuAffinity(pid, cpuMasks));
}

C_(Tasker_EnableThreadStats)
{
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Tasker.EnableThreadStats()");
        return JS_FALSE;
    }

    Tasker::EnableThreadStats();
    return JS_TRUE;
}

C_(Tasker_Yield)
{
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Tasker.Yield()");
        return JS_FALSE;
    }

    Tasker::Yield();
    return JS_TRUE;
}

C_(Tasker_WaitOnBarrier)
{
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Tasker.WaitOnBarrier()");
        return JS_FALSE;
    }

    Tasker::WaitOnBarrier();
    return JS_TRUE;
}

C_(Tasker_Join)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JsArray pids;
    Tasker::ThreadID pid = Tasker::NULL_THREAD;
    RC rc;

    if ((NumArguments != 1) ||
        ((OK != pJavaScript->FromJsval(pArguments[0], &pids)) &&
        (OK != pJavaScript->FromJsval(pArguments[0], &pid))))
    {
        JS_ReportError(pContext, "Usage: Tasker.Join(pid)");
        return JS_FALSE;
    }

    if (!pids.empty())
    {
        vector<Tasker::ThreadID> intPids;
        intPids.resize(pids.size());
        for (UINT32 i=0; i < pids.size(); i++)
        {
            C_CHECK_RC(pJavaScript->FromJsval(pids[i], &intPids[i]));
        }
        C_CHECK_RC(Tasker::Join(intPids));
    }
    else
    {
        C_CHECK_RC(Tasker::Join(pid));
    }

    RETURN_RC(OK);
}

C_(Tasker_PrintAllThreads)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    if ( (NumArguments != 0) )
    {
        JS_ReportError(pContext, "Usage: Tasker.PrintAllThreads()");
        return JS_FALSE;
    }

    Tasker::PrintAllThreads();

    return JS_TRUE;
}

C_(Tasker_StackBytesFree)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;

    if ( (NumArguments != 0) )
    {
        JS_ReportError(pContext, "Usage: Tasker.StackBytesFree()");
        return JS_FALSE;
    }

    pJavaScript->ToJsval(Tasker::StackBytesFree(), pReturlwalue);

    return JS_TRUE;
}

P_(Tasker_Get_ThreadId)
{
    JavaScriptPtr pJs;
    pJs->ToJsval(Tasker::LwrrentThread(), pValue);
    return JS_TRUE;
}

P_(Tasker_Get_ThreadIdx)
{
    JavaScriptPtr pJs;
    pJs->ToJsval(Tasker::GetThreadIdx(), pValue);
    return JS_TRUE;
}

P_(Tasker_Get_GroupSize)
{
    JavaScriptPtr pJs;
    pJs->ToJsval(Tasker::GetGroupSize(), pValue);
    return JS_TRUE;
}

P_(Tasker_Get_NumThreadsAliveInGroup)
{
    JavaScriptPtr pJs;
    pJs->ToJsval(Tasker::GetNumThreadsAliveInGroup(), pValue);
    return JS_TRUE;
}

P_(Tasker_Get_MaxHwPollHz)
{
    JavaScriptPtr pJs;
    pJs->ToJsval(Tasker::GetMaxHwPollHz(), pValue);
    return JS_TRUE;
}

P_(Tasker_Set_MaxHwPollHz)
{
    JavaScriptPtr pJs;
    double Hz;
    pJs->FromJsval(*pValue, &Hz);
    Tasker::SetMaxHwPollHz(Hz);
    return JS_TRUE;
}

JS_STEST_LWSTOM(Tasker,
                CreateSemaphore,
                3,
                "Create a Tasker semaphore")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 initialCount;
    JSObject * jsObj;
    string name;

    if (    (NumArguments != 3)
         || (OK != pJavaScript->FromJsval(pArguments[0], &initialCount))
         || (OK != pJavaScript->FromJsval(pArguments[1], &name))
         || (OK != pJavaScript->FromJsval(pArguments[2], &jsObj))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.CreateSemaphore(InitialCount, name, [SemId])");
        return JS_FALSE;
    }

    SemaID semId;
    semId = Tasker::CreateSemaphore(initialCount, name.c_str());

    if (JS_SetPrivate(pContext, jsObj, semId) != JS_TRUE)
    {
        JS_ReportError(pContext, "Usage: Tasker.CreateSemaphore(InitialCount, name, [SemId])");
        return JS_FALSE;
    }

    RETURN_RC(OK);
}

JS_SMETHOD_LWSTOM(Tasker,
                DestroySemaphore,
                1,
                "Destroy a Tasker semaphore")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * jsObj;

    if (    (NumArguments != 1)
         || (OK != pJavaScript->FromJsval(pArguments[0], &jsObj))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.DestroySemaphore([SemId])");
        return JS_FALSE;
    }

    SemaID sem = (SemaID)JS_GetPrivate(pContext, jsObj);

    if (sem == NULL)
    {
        JS_ReportError(pContext, "Usage: Tasker.DestroySemaphore([SemId])");
        return JS_FALSE;
    }

    Tasker::DestroySemaphore(sem);

    JS_SetPrivate(pContext, jsObj, NULL);

    return JS_TRUE;
}

JS_STEST_LWSTOM(Tasker,
                AcquireSemaphore,
                2,
                "Acquire a semaphore")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    RC rc;
    JavaScriptPtr pJavaScript;
    JSObject * jsObj;
    FLOAT64 timeoutSecs;

    if (    (NumArguments != 2)
         || (OK != pJavaScript->FromJsval(pArguments[0], &jsObj))
         || (OK != pJavaScript->FromJsval(pArguments[1], &timeoutSecs))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.AcquireSemaphore(SemId, timeoutSecs)");
        return JS_FALSE;
    }

    SemaID sem = (SemaID)JS_GetPrivate(pContext, jsObj);

    if (sem == NULL)
    {
        JS_ReportError(pContext, "Usage: Tasker.AcquireSemaphore(SemId, timeoutSecs)");
        return JS_FALSE;
    }

    C_CHECK_RC(Tasker::AcquireSemaphore(sem, timeoutSecs * 1000));

    RETURN_RC(OK);
}

JS_SMETHOD_LWSTOM(Tasker,
                  ReleaseSemaphore,
                  1,
                  "Release a semaphore")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * jsObj;

    if (    (NumArguments != 1)
         || (OK != pJavaScript->FromJsval(pArguments[0], &jsObj))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.ReleaseSemaphore(SemId)");
        return JS_FALSE;
    }

    SemaID sem = (SemaID)JS_GetPrivate(pContext, jsObj);

    if (sem == NULL)
    {
        JS_ReportError(pContext, "Usage: Tasker.ReleaseSemaphore(SemId)");
        return JS_FALSE;
    }

    Tasker::ReleaseSemaphore(sem);

    return JS_TRUE;
}

JS_STEST_LWSTOM(Tasker,
                AllocMutex,
                1,
                "Create a Tasker Mutex")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * jsObj;

    if (    (NumArguments != 1)
         || (OK != pJavaScript->FromJsval(pArguments[0], &jsObj))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.AllocMutex([mutex])");
        return JS_FALSE;
    }

    void * mutex;
    mutex = Tasker::AllocMutex("JS Tasker.AllocMutex", Tasker::mtxUnchecked);

    if (JS_SetPrivate(pContext, jsObj, mutex) != JS_TRUE)
    {
        JS_ReportError(pContext, "Usage: Tasker.AllocMutex([mutex])");
        return JS_FALSE;
    }

    RETURN_RC(OK);
}

JS_SMETHOD_LWSTOM(Tasker,
                  FreeMutex,
                  1,
                  "Free a Tasker Mutex")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * jsObj;

    if (    (NumArguments != 1)
         || (OK != pJavaScript->FromJsval(pArguments[0], &jsObj))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.FreeMutex([mutex])");
        return JS_FALSE;
    }

    void * mutex = (SemaID)JS_GetPrivate(pContext, jsObj);

    if (mutex == NULL)
    {
        JS_ReportError(pContext, "Usage: Tasker.FreeMutex([mutex])");
        return JS_FALSE;
    }

    Tasker::FreeMutex(mutex);

    JS_SetPrivate(pContext, jsObj, NULL);

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(Tasker,
                  AcquireMutex,
                  1,
                  "Acquire a Mutex")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * jsObj;

    if (    (NumArguments != 1)
         || (OK != pJavaScript->FromJsval(pArguments[0], &jsObj))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.AcquireMutex(mutex)");
        return JS_FALSE;
    }

    void * mutex = (void *)JS_GetPrivate(pContext, jsObj);

    if (mutex == NULL)
    {
        JS_ReportError(pContext, "Usage: Tasker.AcquireMutex(mutex)");
        return JS_FALSE;
    }

    Tasker::AcquireMutex(mutex);

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM(Tasker,
                  ReleaseMutex,
                  1,
                  "Release a Mutex")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JSObject * jsObj;

    if (    (NumArguments != 1)
         || (OK != pJavaScript->FromJsval(pArguments[0], &jsObj))
       )
    {
        JS_ReportError(pContext, "Usage: Tasker.ReleaseMutex(mutex)");
        return JS_FALSE;
    }

    void * mutex = (void *)JS_GetPrivate(pContext, jsObj);

    if (mutex == NULL)
    {
        JS_ReportError(pContext, "Usage: Tasker.ReleaseMutex(mutex)");
        return JS_FALSE;
    }

    Tasker::ReleaseMutex(mutex);

    return JS_TRUE;
}

static JSBool C_ModsEvent_constructor
(
    JSContext *cx,
    JSObject *obj,
    uintN argc,
    jsval *argv,
    jsval *rval
)
{
    JavaScriptPtr pJavaScript;
    string eventName;
    const char * usage = "Usage : new ModsEvent(name, [bManualReset])";

    if (((argc != 1) && (argc != 2)) ||
        (OK != pJavaScript->FromJsval(argv[0], &eventName)))
    {
       JS_ReportError(cx, usage);
       return JS_FALSE;
    }

    ModsEvent * pEvent;
    if (argc == 2)
    {
        bool bManualReset;
        if (OK != pJavaScript->FromJsval(argv[1], &bManualReset))
        {
           JS_ReportError(cx, usage);
           return JS_FALSE;
        }
        pEvent = Tasker::AllocEvent(eventName.c_str(), bManualReset);
    }
    else
    {
        pEvent = Tasker::AllocEvent(eventName.c_str());
    }

    MASSERT(pEvent);

    return JS_SetPrivate(cx, obj, pEvent);
}

static void C_ModsEvent_finalize
(
    JSContext *cx,
    JSObject *obj
)
{
    void *pvEvent = JS_GetPrivate(cx, obj);
    if(pvEvent)
    {
        ModsEvent * pEvent = (ModsEvent *)pvEvent;
        Tasker::FreeEvent(pEvent);
    }
}

static JSClass ModsEvent_class =
{
    "ModsEvent",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_PropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ColwertStub,
    C_ModsEvent_finalize
};

static SObject ModsEvent_Object
(
    "ModsEvent",
    ModsEvent_class,
    0,
    0,
    "JS wrapper for a ModsEvent object",
    C_ModsEvent_constructor
);

JS_STEST_LWSTOM(ModsEvent,
                Wait,
                1,
                "Wait on an event")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    FLOAT64 timeoutMs;

    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &timeoutMs)))
    {
        JS_ReportError(pContext, "Usage: ModsEvent.Wait(timeoutMs)");
        return JS_FALSE;
    }

    ModsEvent * pEvent = JS_GET_PRIVATE(ModsEvent, pContext,
                                        pObject, "ModsEvent");
    if (pEvent != NULL)
    {
        RETURN_RC(Tasker::WaitOnEvent(pEvent, timeoutMs));
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(ModsEvent,
                  Set,
                  0,
                  "Set the event")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: ModsEvent.Set()");
        return JS_FALSE;
    }

    ModsEvent * pEvent = JS_GET_PRIVATE(ModsEvent, pContext,
                                        pObject, "ModsEvent");
    if (pEvent != NULL)
    {
        Tasker::SetEvent(pEvent);
        return JS_TRUE;
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM(ModsEvent,
                  Reset,
                  0,
                  "Reset an event")
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: ModsEvent.Reset()");
        return JS_FALSE;
    }

    ModsEvent * pEvent = JS_GET_PRIVATE(ModsEvent, pContext,
                                        pObject, "ModsEvent");
    if (pEvent != NULL)
    {
        Tasker::ResetEvent(pEvent);
        return JS_TRUE;
    }

    return JS_FALSE;
}

