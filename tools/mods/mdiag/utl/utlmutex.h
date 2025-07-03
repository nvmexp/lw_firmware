/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLMUTEX_H
#define INCLUDED_UTLMUTEX_H

#include "core/include/types.h"
#include "utlpython.h"

// This class represents the C++ backing for the UTL Python Mutex object.
// This class provide the block and unblock ability to communicate between UTL and other
// plugin(lwrrently support PolicyManager only).
//
class UtlMutex 
{
public:
    static void Register(py::module module);
    
    string GetName() const { return m_Name; }
    void * GetMutex() { return m_pMutex; }

    // Explore to python
    void Acquire();
    void Release();
    static UtlMutex * CreateMutex(py::kwargs kwargs);    
    static void RemoveMutex(py::kwargs kwargs);
    static void FreeMutexes();
private:
    UtlMutex(string name);
    static void RegisterToPolicyManager(UtlMutex * utlMutex);

    string m_Name;
    void * m_pMutex;
    static map<string, unique_ptr<UtlMutex> > s_MutexLists;
};

#endif
