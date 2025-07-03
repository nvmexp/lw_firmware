/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlmutex.h"
#include "core/include/rc.h"
#include "mdiag/sysspec.h"
#include "utlutil.h"
#include "mdiag/advschd/policymn.h"
map<string, unique_ptr<UtlMutex> > UtlMutex::s_MutexLists;

void UtlMutex::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Mutex.CreateMutex", "name", true, "name of mutex");
    kwargs->RegisterKwarg("Mutex.RemoveMutex", "name", true, "name of mutex");

    py::class_<UtlMutex> utlMutex(module, "Mutex");
    utlMutex.def_static("CreateMutex", &UtlMutex::CreateMutex,
        UTL_DOCSTRING("Mutex.CreateMutex", "This function create a single mutex."),
        py::return_value_policy::reference);
    utlMutex.def_static("RemoveMutex", &UtlMutex::RemoveMutex,
        UTL_DOCSTRING("Mutex.RemoveMutex", "This function remove a single mutex."));
    utlMutex.def("Acquire", &UtlMutex::Acquire,
        "This function acquire this mutex between UTL and PolicyManager.");
    utlMutex.def("Release", &UtlMutex::Release,
        "This function release this mutex between UTL and PolicyManager.");
    utlMutex.def_property_readonly("name", &UtlMutex::GetName,
        "This read-only data member specified the mutex name between UTL and PolicyManager");
}

UtlMutex::UtlMutex
(
    string name
): m_Name(name)
{
    m_pMutex = Tasker::AllocMutex(name.c_str(), Tasker::mtxUnchecked);
}

void UtlMutex::FreeMutexes()
{
    for (auto it = s_MutexLists.begin();
            it != s_MutexLists.end(); ++it)
    {
        Tasker::FreeMutex(it->second->GetMutex());
    }
 
    s_MutexLists.clear();
}

/*static*/UtlMutex * UtlMutex::CreateMutex
(
    py::kwargs kwargs
)
{
    string name = UtlUtility::GetRequiredKwarg<string>(kwargs,
                        "name", "Utl.CreateMutex");
    UtlGil::Release gil;
 
    if (s_MutexLists.find(name) != s_MutexLists.end())
    {
        UtlUtility::PyError("Duplicated mutex name %s.", name.c_str());
    }
    unique_ptr<UtlMutex> utlMutex(new UtlMutex(name));
    s_MutexLists[name] = move(utlMutex);
    RegisterToPolicyManager(s_MutexLists[name].get());

    return s_MutexLists[name].get();
}

/*static*/ void UtlMutex::RemoveMutex
(
    py::kwargs kwargs
)
{
    string name = UtlUtility::GetRequiredKwarg<string>(kwargs,
                        "name", "Utl.RemoveMutex");
    UtlGil::Release gil;
    
    if (s_MutexLists.find(name) == s_MutexLists.end())
    {
        UtlUtility::PyError("Illegal mutex name %s.", name.c_str());
    }

    s_MutexLists.erase(name);
    PolicyManager * pPolicyManager = PolicyManager::Instance();
    RC rc = pPolicyManager->DeleteUtlMutex(name);
    UtlUtility::PyErrorRC(rc, "Mutex.RemoveMutex failed.");
}

/*static*/ void UtlMutex::RegisterToPolicyManager(UtlMutex * pUtlMutex)
{
    PolicyManager * pPolicyManager = PolicyManager::Instance();
    pPolicyManager->AddUtlMutex(pUtlMutex->GetMutex(), pUtlMutex->GetName());
}

void UtlMutex::Acquire()
{
    UtlGil::Release gil;
    Tasker::AcquireMutex(m_pMutex);
}

void UtlMutex::Release()
{
    UtlGil::Release gil;
    Tasker::ReleaseMutex(m_pMutex);
}

