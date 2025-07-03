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

#ifndef INCLUDED_UTLGPUREGCTRL_H
#define INCLUDED_UTLGPUREGCTRL_H

#include "mdiag/sysspec.h"

#include "utlregctrl.h"

class LwRm;
class LWGpuResource;
class GpuSubdevice;
class SmcEngine;
class UtlGpu;
class IRegisterClass;

// This class represents a UTL GPU register manipulation interface.
//
class UtlGpuRegCtrl: public UtlRegCtrl
{
public:
    static void Register(py::module module);

    UtlGpuRegCtrl(UtlGpu* gpu);
    UtlGpuRegCtrl(SmcEngine* smcEngine);

    UtlGpuRegCtrl() = delete;
    UtlGpuRegCtrl(UtlGpuRegCtrl&) = delete;
    UtlGpuRegCtrl &operator=(UtlGpuRegCtrl&) = delete;

    UINT32 GetAddress
    (
       const string& regName,
       bool ignoreDomainBase
    );
    string GetName
    (
        UINT32 regAddress
    );
    unique_ptr<IRegisterClass> GetRegister
    (
        const string& regName
    );
    UINT32 Read32
    (
        const string& regName,
        bool hasRegName, 
        UINT32 regAddress,
        UtlChannel* pUtlChannel
    );
    void Write32
    (
        const string& regName,
        bool hasRegName, 
        UINT32 regAddress,
        UINT32 data,
        py::kwargs kwargs,
        UtlChannel* pUtlChannel
    );
    UINT32 GetField
    (
        const string& regName,
        const string& regFieldName,
        UINT32 regValue,
        bool hasRegValue
    );
    UINT32 SetField
    (
        const string& regName,
        const string& regFieldName,
        const string& regFieldValueName,
        bool hasRegFieldValueName,
        UINT32 regFieldValueNum,
        UINT32 regValue,
        bool hasRegValue,
        py::kwargs kwargs
    );
    bool TestField
    (
        const string& regName,
        const string& regFieldName,
        const string& regFieldValueName,
        bool hasRegFieldValueName,
        UINT32 regFieldValueNum,
        UINT32 regValue,
        bool hasRegValue
    );
    UINT32 GetFieldDef
    (
        const string& regName,
        const string& regFieldName,
        const string& regFieldValueDef
    );
    void SetRegSpace(py::kwargs kwargs);
    UINT32 GetFieldStartBit(py::kwargs kwargs);
    UINT32 GetFieldEndBit(py::kwargs kwargs);
    UINT32 GetFieldWidth(py::kwargs kwargs);

private:
    UINT32 GetRegAddress
    (
        const string& regName, 
        const string apiName,
        bool ignoreDomainBase = false
    );

    LWGpuResource* m_pGpuResource = nullptr;
    GpuSubdevice* m_pGpuSubdevice = nullptr;
    LwRm* m_pLwRm = nullptr;
    SmcEngine* m_pSmcEngine = nullptr;
    string m_RegSpace;
};

#endif
