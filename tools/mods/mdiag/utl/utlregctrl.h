/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLREGCTRL_H
#define INCLUDED_UTLREGCTRL_H

#include "utlpython.h"

class UtlChannel;
class IRegisterClass;

// This class represents a UTL register manipulation interface.
//
class UtlRegCtrl
{
public:
    static void Register(py::module module);

    virtual ~UtlRegCtrl() { }

    virtual UINT32 GetAddress
    (
        const string& regName,
        bool ignoreDomainBase
    ) = 0;
    virtual string GetName
    (
        UINT32 regAddress
    ) = 0;
    virtual unique_ptr<IRegisterClass> GetRegister
    (
        const string& regName
    ) = 0;
    virtual UINT32 Read32
    (
        const string& regName,
        bool hasRegName, 
        UINT32 regAddress,
        UtlChannel* pUtlChannel
    ) = 0;
    virtual void Write32
    (
        const string& regName,
        bool hasRegName, 
        UINT32 regAddress,
        UINT32 data,
        py::kwargs kwargs,
        UtlChannel* pUtlChannel
    ) = 0;
    virtual UINT32 GetField
    (
        const string& regName,
        const string& regFieldName,
        UINT32 regValue,
        bool hasRegValue
    ) = 0;
    virtual UINT32 SetField
    (
        const string& regName,
        const string& regFieldName,
        const string& regFieldValueName,
        bool hasRegFieldValueName,
        UINT32 regFieldValueNum,
        UINT32 regValue,
        bool hasRegValue,
        py::kwargs kwargs
    ) = 0;
    virtual bool TestField
    (
        const string& regName,
        const string& regFieldName,
        const string& regFieldValueName,
        bool hasRegFieldValueName,
        UINT32 regFieldValueNum,
        UINT32 regValue,
        bool hasRegValue
    ) = 0;
    virtual UINT32 GetFieldDef
    (
        const string& regName,
        const string& regFieldName,
        const string& regFieldValueDef
    ) = 0;

    UINT32 GetAddressPy(py::kwargs kwargs);
    string GetNamePy(py::kwargs kwargs);
    vector<UINT32> GetArraySizesPy(py::kwargs kwargs);
    vector<UINT32> GetArrayStridesPy(py::kwargs kwargs);
    UINT32 Read32Py(py::kwargs kwargs);
    void Write32Py(py::kwargs kwargs);
    UINT32 GetFieldPy(py::kwargs kwargs);
    UINT32 SetFieldPy(py::kwargs kwargs);
    bool TestFieldPy(py::kwargs kwargs);
    UINT32 GetFieldDefPy(py::kwargs kwargs);
    UINT32 GetHwInitValuePy(py::kwargs kwargs);
    UINT32 GetHwInitColdValuePy(py::kwargs kwargs);
    UINT32 GetHwInitWarmValuePy(py::kwargs kwargs);
};

#endif
