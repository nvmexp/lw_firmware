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

#ifndef INCLUDED_UTLKWARGSMGR_H
#define INCLUDED_UTLKWARGSMGR_H

#include <unordered_map>
#include "utl.h"
#include "utlpython.h"

// This is a singleton class 
class UtlKwargsMgr
{
public:
    UtlKwargsMgr(const UtlKwargsMgr&) = delete;
    UtlKwargsMgr& operator=(const UtlKwargsMgr&) = delete;

    static UtlKwargsMgr* Instance();
    static bool HasInstance() { return (s_KwargsMgr.get() != nullptr); };

    struct KwargEntry
    {
        KwargEntry
        (
            const string& keyword,
            bool required,
            const string& description
        );

        string m_Keyword;
        bool m_Required;
        string m_Description;
    };

    // All UTL script functions which call into C++ pass their parameters
    // with keyword-arguments. (For information about keyword-arguments, see
    // https://docs.python.org/3/tutorial/controlflow.html#keyword-arguments).
    // These keyword arguments are passed to C++ with a pybind11 class
    // called py::kwargs. The py::kwargs class is similar to a STL map and
    // each argument is represented as a key-value pair in that map.
    //
    // This function is used to create a registry of accepted
    // keyword-arguments for each UTL function. The registry is checked when
    // parsing keyword-arguments at runtime for error-checking purposes.
    // It is also used to generate user documentation for UTL.
    void RegisterKwarg
    (
        const char* functionName,
        const char* keyword,
        bool required,
        const char* description
    );
    
    // CheckUserKwargsOrDie() is used to verify that all user-supplied keyword
    // arguments are expected, erroring out if an unexpected kwarg is found.
    static void CheckUserKwargs(const char* functionName, py::kwargs kwargs)
    {
        UtlKwargsMgr* kwargsmgr = UtlKwargsMgr::Instance();
        kwargsmgr->CheckUserKwargsOrDie(functionName, kwargs);
    }
    void CheckUserKwargsOrDie(const char* functionName, py::kwargs kwargs) const;
    bool VerifyKwarg(const char* functionName, const char* kwarg, bool required) const;
    string GetDocstring(const char* functionName, const char* functionDesc) const;

private:
    UtlKwargsMgr();
    
    static void CreateInstance();
    static unique_ptr<UtlKwargsMgr> s_KwargsMgr;

    unordered_map<string, unordered_map<string, KwargEntry> > m_KwargsMap;
};

#endif
