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

#include "utl.h"
#include "utlutil.h"
#include "utlkwargsmgr.h"
#include "mdiag/sysspec.h"
#include <sstream>

unique_ptr<UtlKwargsMgr> UtlKwargsMgr::s_KwargsMgr;

UtlKwargsMgr::KwargEntry::KwargEntry
(
    const string& keyword,
    bool required,
    const string& description
)
:   m_Keyword(keyword),
    m_Required(required),
    m_Description(description)
{
}

UtlKwargsMgr::UtlKwargsMgr()
{
    m_KwargsMap = unordered_map<string, unordered_map<string, KwargEntry> >();
}

void UtlKwargsMgr::CreateInstance()
{
    MASSERT(!s_KwargsMgr.get());
    s_KwargsMgr.reset(new UtlKwargsMgr());
}

UtlKwargsMgr* UtlKwargsMgr::Instance()
{
    if (!s_KwargsMgr.get())
    {
        CreateInstance();
    }
    return s_KwargsMgr.get();
}

void UtlKwargsMgr::RegisterKwarg
(
    const char* functionName,
    const char* keyword,
    bool required,
    const char* description
)
{
    KwargEntry entry(keyword, required, description);
    m_KwargsMap[functionName].insert({keyword, entry});
}

void UtlKwargsMgr::CheckUserKwargsOrDie(const char* functionName, py::kwargs kwargs) const
{
    auto fmap = m_KwargsMap.find(functionName);
    // Check that the function has been registered
    // Not all functions have kwargs
    if (fmap == m_KwargsMap.end())
    {
        return;
    }
    for (auto kwarg : kwargs)
    {
        auto entry = fmap->second.find(kwarg.first.cast<string>());
        if (entry == fmap->second.end())
        {
            UtlUtility::PyError("Unexpected keyword argument passed to function %s: %s",
                functionName, kwarg.first.cast<string>().c_str());
        }
    }
}

bool UtlKwargsMgr::VerifyKwarg(const char* functionName, const char* kwarg, bool required) const
{
    auto fmap = m_KwargsMap.find(functionName);
    // Check that the function has been registered
    if (fmap == m_KwargsMap.end())
    {
        UtlUtility::PyError("Function %s not found in kwargsMgr table", functionName);
    }
    auto entry = fmap->second.find(string(kwarg));
    if (entry == fmap->second.end())
    {
        return false;
    }
    return entry->second.m_Required == required;
}

string UtlKwargsMgr::GetDocstring(const char* functionName, const char* functionDesc) const
{
    auto fmap = m_KwargsMap.find(functionName);
    if (fmap == m_KwargsMap.end())
    {
        // No kwargs associated with this function
        return "";
    }
    vector<KwargEntry> reqEntries = vector<KwargEntry>();
    vector<KwargEntry> optEntries = vector<KwargEntry>();
    for (auto& entry : fmap->second)
    {
        if (entry.second.m_Required)
        {
            reqEntries.push_back(entry.second);
        }
        else
        {
            optEntries.push_back(entry.second);
        }
    }

    // Populate and format docstring
    std::stringstream ss;
    if (!reqEntries.empty())
    {
        ss << "\n\n";
        ss << "Required kwargs:\n\n";
        for (auto& entry : reqEntries)
        {
            ss << ":param " << entry.m_Keyword << ": " << entry.m_Description << "\n";
        }
    }
    if (!optEntries.empty())
    {
        if (ss.rdbuf()->in_avail() == 0)
        {
            ss << "\n";
        }
        ss << "\nOptional kwargs:\n\n";
        for (auto& entry : optEntries)
        {
            ss << ":param " << entry.m_Keyword << ": " << entry.m_Description << "\n";
        }
    }
    return ss.str();
}
