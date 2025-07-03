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

#ifndef INCLUDED_UTLTHREAD_H
#define INCLUDED_UTLTHREAD_H

class Thread;

#include "utlpython.h"

// This class represents the C++ backing for a UTL Python thread object
// and other thread-related functions.
//
class UtlThread
{
public:
    static void Register(py::module module);

    static void FreeThreads();

    // The following functions can be called from a UTL script.
    static UtlThread* Fork(py::kwargs kwargs);
    static void Yield();
    bool IsFinished() const;
    static void WaitAll();
    static void Poll(py::kwargs kwargs);

    // Delete all unnecessary constructors/assignment operators because
    // UtlThread objects should only be created with the Fork function.
    UtlThread() = delete;
    UtlThread(UtlThread&) = delete;
    UtlThread &operator=(UtlThread&) = delete;

private:
    struct PollArgs
    {
        py::function m_Func;
    };

    // Private constructor called by Fork.
    UtlThread(py::function func);

    py::function m_PythonFunction;
    py::object m_Args;
    bool m_HasArgs = false;
    Thread* m_MdiagThread;
    bool m_IsFinished = false;

    static int ThreadMainFunction(int argc, const char** argv);
    static bool PollFunc(void* args);

    // This is a list of threads that have been created from a UTL script.
    static vector<unique_ptr<UtlThread>> s_Threads;
};

#endif
