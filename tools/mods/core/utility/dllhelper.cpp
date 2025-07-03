/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/dllhelper.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include <map>

namespace
{
    //! This map indicates how many times we called Xp::LoadDLL() to
    //! load each module with unloadOnExit=true.  On exit, we need to
    //! unload those modules the same number of times.
    //!
    map<void *, UINT32> s_DllModulesToUnloadOnExit;

    //! This mutex should be used around Xp::LoadDLL() and
    //! Xp::UnloadDLL(), partially because the operations we do on
    //! s_DllModulesToUnloadOnExit aren't thread-safe, and partially
    //! because loading DLLs in two parallel threads isn't thread-safe
    //! on some operating systems.
    //!
    void *s_DllMutex;
}

//--------------------------------------------------------------------
//! \brief Should be called in Xp::OnEntry after initializing Tasker
//!
RC DllHelper::Initialize()
{
    s_DllMutex = Tasker::AllocMutex("DllMutex", Tasker::mtxLast);
    return OK;
}

//--------------------------------------------------------------------
//! \brief Should be called in Xp::OnExit before destroying Tasker
//!
RC DllHelper::Shutdown()
{
    StickyRC firstRc;

    // Unload DLLs that were loaded with unloadOnExit=true
    //
    while (!s_DllModulesToUnloadOnExit.empty())
    {
        firstRc = Xp::UnloadDLL(s_DllModulesToUnloadOnExit.begin()->first);
    }

    // Free the mutex
    //
    if (s_DllMutex)
    {
        Tasker::FreeMutex(s_DllMutex);
        s_DllMutex = nullptr;
    }

    return firstRc;
}

//--------------------------------------------------------------------
//! \brief Hold this mutex during Xp::LoadDLL() and Xp::UnloadDLL()
//!
void *DllHelper::GetMutex()
{
    MASSERT(s_DllMutex);
    return s_DllMutex;
}

//--------------------------------------------------------------------
//! \brief Append a missing suffix, if any, in Xp::LoadDLL()
//!
string DllHelper::AppendMissingSuffix(const string &fileName)
{
    string fileNameWithSuffix = fileName;
    if (fileName.find('.') == string::npos)
    {
        Printf(Tee::PriDebug, "Xp::LoadDLL: Guessing %s as a suffix for %s\n",
               Xp::GetDLLSuffix().c_str(), fileName.c_str());
        fileNameWithSuffix += Xp::GetDLLSuffix();
    }
    return fileNameWithSuffix;
}

//--------------------------------------------------------------------
//! \brief Call this from Xp::LoadDLL() after loading the DLL
//!
RC DllHelper::RegisterModuleHandle(void *moduleHandle, bool unloadOnExit)
{
    // Increment the entry in s_DllModulesToUnloadOnExit, creating
    // entry if needed.
    //
    if (unloadOnExit)
    {
        if (s_DllModulesToUnloadOnExit.count(moduleHandle))
        {
            ++s_DllModulesToUnloadOnExit[moduleHandle];
        }
        else
        {
            s_DllModulesToUnloadOnExit[moduleHandle] = 1;
        }
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Call this from Xp::UnloadDLL() before unloading the DLL
//!
RC DllHelper::UnregisterModuleHandle(void *moduleHandle)
{
    // Decrement the entry in s_DllModulesToUnloadOnExit, if any.
    // Erase entry when it reaches 0.
    //
    map<void*, UINT32>::iterator iter =
        s_DllModulesToUnloadOnExit.find(moduleHandle);
    if (iter != s_DllModulesToUnloadOnExit.end())
    {
        MASSERT(iter->second != 0);
        if (iter->second > 1)
        {
            --iter->second;
        }
        else
        {
            s_DllModulesToUnloadOnExit.erase(iter);
        }
    }
    return OK;
}
