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

#ifndef INCLUDED_UTLHELP_H
#define INCLUDED_UTLHELP_H

#include <queue>
#include "core/include/utility.h"
#include "utlpython.h"

// This class is used to print UTL module documentation to the MODS log file.
//
class UtlHelp
{
public:
    UtlHelp() {};
    void Print();

private:
    // This structure represents information about a single type from
    // the UTL module that is exported to Python.  It is only used by UtlHelp.
    //
    struct UtlType
    {
        UtlType
        (
            const string& name,
            py::handle handle,
            const string& fullName
        );

        bool IsEnum();
        bool IsClass();
        bool IsModuleFunction();

        string m_Name;
        py::handle m_Handle;
        string m_FullName;
    };

    static bool AttrIsValid(const string& name);
    void PrintHeader();
    void PrintSubheader(int indentLevel);

    void PrintDocString
    (
        py::handle handle,
        int indentLevel,
        bool isFunction = false
    );

    void PrintModuleFunction(const UtlType& utlFunction);
    void PrintClass(const UtlType& utlClass);

    void PrintClassAttributes
    (
        const string& headerText,
        const vector<UtlType>& attrList,
        bool isFunction = false
    );

    void PrintClassConstants
    (
        const string& headerText,
        const vector<UtlType>& attrList
    );

    void PrintEnum(const UtlType& utlEnum);

    queue<UtlType> m_ModuleFunctions;
    queue<UtlType> m_ClassTypes;
    queue<UtlType> m_EnumTypes;
};

#endif
