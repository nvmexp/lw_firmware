/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/tasker.h"

#include "mdiag/sysspec.h"
#include "mdiag/thread.h"

#include "utl.h"
#include "utlthread.h"
#include "utlutil.h"

vector<unique_ptr<UtlThread>> UtlThread::s_Threads;

void UtlThread::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Thread.Fork", "func", true, "function to run in new thread");
    kwargs->RegisterKwarg("Thread.Fork", "args", false, "optional keyword args for new function");
    kwargs->RegisterKwarg("Thread.Poll", "func", true, "function to poll");
    
    py::class_<UtlThread> thread(module, "Thread",
        "The Thread class is used for objects that represent MODS threads.  UTL scripts that wish to run functions in parallel should use the Thread class rather than the the Python threading module."
        "\n"
        "MODS threads are cooperative, which means that threads only yield when they explicitly call a yielding function.  UTL scripts can use the Thread.Yield function to yield to other threads.  In addition, some long running functions have implicit yields within them (e.g. Test.Run).");
    thread.def_static("Fork", &UtlThread::Fork,
        UTL_DOCSTRING("Thread.Fork", "This function forks a cooperative MODS thread."),
        py::return_value_policy::reference);
    thread.def_static("Yield", &UtlThread::Yield,
        "This function yields control to another thread.");
    thread.def("IsFinished", &UtlThread::IsFinished,
        "This function returns True if the main thread function has completed.");
    thread.def_static("WaitAll", &UtlThread::WaitAll,
        "This function will pause the current thread and wait for any child threads to finish.");
    thread.def_static("Poll", &UtlThread::Poll,
        UTL_DOCSTRING("Thread.Poll", "This function takes a Python function that returns True or False and polls on it until it returns True.  Each time the polled function returns False, the current thread will yield.  The Thread.Poll function will also timeout if the polling time exceeds the default timeout value, which can be defined with the -timeout_ms MODS command-line argument."));
}

// This function is called from a python script to create a new thread,
// which will run the user-specified Python function.
//
UtlThread* UtlThread::Fork(py::kwargs kwargs)
{
    UtlUtility::RequirePhase(Utl::InitEventPhase | Utl::RunPhase,
        "Thread.Fork");

    py::function func = UtlUtility::GetRequiredKwarg<py::function>(
        kwargs, "func", "Thread.Fork");

    if (func.is_none())
    {
        UtlUtility::PyError("func parameter is not a valid Python function");
    }

    DebugPrintf("UTL: UtlThread::Fork called by thread id %d\n", Tasker::LwrrentThread());
    unique_ptr<UtlThread> utlThread(new UtlThread(func));

    py::object args;
    if (UtlUtility::GetOptionalKwarg<py::object>(&args, kwargs, "args", "Thread.Fork"))
    {
        utlThread->m_Args = args;
        utlThread->m_HasArgs = true;
    }

    UtlGil::Release gil;

    utlThread->m_MdiagThread = Thread::Fork(ThreadMainFunction,
        0, (const char**) utlThread.get(), 2 * MEGABYTE, false,
        "UtlThread::Fork");

    s_Threads.push_back(move(utlThread));

    return s_Threads.back().get();
}

UtlThread::UtlThread(py::function func)
    : m_PythonFunction(func)
{
}

// This function is used by UtlThread::Fork to create a new thread that calls
// a user-specified function from a UTL script.
//
int UtlThread::ThreadMainFunction(int argc, const char** argv)
{
    DebugPrintf("UTL: UtlThread::ThreadMainFunction thread id = %d\n", Tasker::LwrrentThread());
    UtlThread* utlThread = (UtlThread*) argv;

    // Each thread needs to catch exceptions from the Python interpreter.
    try
    {
        UtlGil::Acquire gil;

        if (utlThread->m_HasArgs)
        {
            bool argIsDict = py::isinstance<py::dict>(utlThread->m_Args);
            py::object codeObject = utlThread->m_PythonFunction.attr("__code__");
            int funcArgCount = codeObject.attr("co_argcount").cast<int>();
            UINT32 funcFlags = codeObject.attr("co_flags").cast<UINT32>();

            // If the argument object is a Python dictionary and the user's
            // function is only expecting kwargs, the user is expecting the
            // legacy version of utl.Thread.Fork, which means the args should
            // be passed as kwargs instead of a positional parameter.
            // (See bug 3319233.)
            if (argIsDict && (funcArgCount == 0) && (funcFlags & CO_VARKEYWORDS))
            {
                utlThread->m_PythonFunction(**utlThread->m_Args);
            }
            else
            {
                utlThread->m_PythonFunction(utlThread->m_Args);
            }
        }
        else
        {
            utlThread->m_PythonFunction();
        }
    }
    catch (const py::error_already_set& e)
    {
        UtlUtility::HandlePythonException(e.what());
    }

    utlThread->m_IsFinished = true;

    return 1;
}

void UtlThread::FreeThreads()
{
    s_Threads.clear();
}

// This function can be called from a UTL script to yield to another thread.
//
void UtlThread::Yield()
{
    if (Utl::Instance()->IsAborting())
    {
        throw std::runtime_error("aborting due to earlier errors");
    }
    else
    {
        UtlGil::Release gil;
        Tasker::Yield();
    }
}

bool UtlThread::IsFinished() const
{
    return m_IsFinished;
}

// This function can be called from a UTL script to wait for all child threads
// to finish.
//
void UtlThread::WaitAll()
{
    UtlGil::Release gil;
    Thread::JoinAll();
}

void UtlThread::Poll(py::kwargs kwargs)
{
    UtlUtility::RequirePhase(Utl::InitEventPhase | Utl::RunPhase,
        "Thread.Poll");

    UtlThread::PollArgs pollArgs;

    pollArgs.m_Func = UtlUtility::GetRequiredKwarg<py::function>(
        kwargs, "func", "Thread.Poll");

    if (pollArgs.m_Func.is_none())
    {
        UtlUtility::PyError("func parameter is not a valid Python function");
    }

    UtlGil::Release gil;

    RC rc = POLLWRAP_HW(&UtlThread::PollFunc, &pollArgs,
        Tasker::GetDefaultTimeoutMs());

    if (rc == RC::TIMEOUT_ERROR)
    {
        UtlUtility::PyError("Thread.Poll function timed out");
    }
    else
    {
        MASSERT(rc == OK);
    }
}

bool UtlThread::PollFunc(void* args)
{
    if (Utl::Instance()->IsAborting())
    {
        throw std::runtime_error("aborting due to earlier errors");
    }

    UtlThread::PollArgs* pollArgs = (UtlThread::PollArgs*) args;
    UtlGil::Acquire gil;
    return pollArgs->m_Func().cast<bool>();
}
