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

#ifndef INCLUDED_UTLLWSWITCHREGCTRL_H
#define INCLUDED_UTLLWSWITCHREGCTRL_H

#include "utlregctrl.h"
#include "utlpython.h"

class IRegisterClass;
class UtlLwSwitch;

// This class represents a UTL LwSwitch register manipulation interface.
//
class UtlLwSwitchRegCtrl: public UtlRegCtrl
{
public:
    static void Register(py::module module);
    UtlLwSwitchRegCtrl(UtlLwSwitch* lwswitch, UINT32 instance, UINT32 engine);

    UtlLwSwitchRegCtrl() = delete;
    UtlLwSwitchRegCtrl(UtlLwSwitchRegCtrl&) = delete;
    UtlLwSwitchRegCtrl &operator=(UtlLwSwitchRegCtrl&) = delete;

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
    void SetEngine(py::kwargs kwargs);

private:
    UINT32 ControlRead(UINT32 addr) const;
    void ControlWrite(UINT32 addr, bool bcast, UINT32 val) const;

    UtlLwSwitch* m_Lwswitch;
    UINT32 m_Instance;
    UINT32 m_Engine;
};

#endif
