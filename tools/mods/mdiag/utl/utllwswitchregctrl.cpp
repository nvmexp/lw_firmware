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

#include "utllwswitch.h"
#include "utllwswitchregctrl.h"
#include "utlutil.h"
#include "mdiag/IRegisterMap.h"
#include "core/include/refparse.h"

void UtlLwSwitchRegCtrl::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("LwSwitchRegCtrl.SetField", "broadcast", false, "broadcast register write");
    kwargs->RegisterKwarg("LwSwitchRegCtrl.Write32", "broadcast", false, "broadcast register write");
    kwargs->RegisterKwarg("LwSwitchRegCtrl.SetEngine", "engine", true, "target engine number");
    kwargs->RegisterKwarg("LwSwitchRegCtrl.SetEngine", "instance", true, "target instance number");

    py::class_<UtlLwSwitchRegCtrl, UtlRegCtrl> lwswitchRegCtrl(module, "LwSwitchRegCtrl",
        "UTL interface for manipulating LwSwitch registers. In addition to the standard kwargs accepted by the RegCtrl class, LwSwitchRegCtrl.SetField() and Write32() accept an optional 'broadcast' kwarg, defaulting to False if not given");
    lwswitchRegCtrl.def("SetEngine", &UtlLwSwitchRegCtrl::SetEngine,
        UTL_DOCSTRING("LwSwitchRegCtrl.SetEngine", "This function sets the engine and instance for future register operations."));
}

UtlLwSwitchRegCtrl::UtlLwSwitchRegCtrl
(
    UtlLwSwitch* lwswitch,
    UINT32 instance,
    UINT32 engine
)
    : m_Lwswitch(lwswitch),
    m_Instance(instance),
    m_Engine(engine)
{
}

UINT32 UtlLwSwitchRegCtrl::GetAddress
(
   const string& regName,
   bool ignoreDomainBase
)
{
    return m_Lwswitch->FindRegister(regName)->GetAddress();
}

string UtlLwSwitchRegCtrl::GetName
(
    UINT32 regAddress
)
{
    string regName;
    auto reg = m_Lwswitch->FindRegister(regAddress);

    if (reg)
    {
        regName = reg->GetName();
    }

    return regName;
}

unique_ptr<IRegisterClass> UtlLwSwitchRegCtrl::GetRegister
(
    const string& regName
)
{
    return m_Lwswitch->FindRegister(regName);
}

UINT32 UtlLwSwitchRegCtrl::Read32
(
    const string& regName,
    bool hasRegName, 
    UINT32 regAddress,
    UtlChannel* pUtlChannel
)
{
    if (hasRegName)
    {
        regAddress = m_Lwswitch->FindRegister(regName)->GetAddress();
    }

    return ControlRead(regAddress);
}

void UtlLwSwitchRegCtrl::Write32
(
    const string& regName,
    bool hasRegName, 
    UINT32 regAddress,
    UINT32 data,
    py::kwargs kwargs,
    UtlChannel* pUtlChannel
)
{
    bool bcast = false;
    UtlUtility::GetOptionalKwarg<bool>(&bcast, kwargs, "broadcast", "LwSwitchRegCtrl.Write32");

    UtlGil::Release gil;

    if (hasRegName)
    {
        regAddress = m_Lwswitch->FindRegister(regName)->GetAddress();
    }

    ControlWrite(regAddress, bcast, data);
}

UINT32 UtlLwSwitchRegCtrl::GetField
(
    const string& regName,
    const string& regFieldName,
    UINT32 regValue,
    bool hasRegValue
)
{

    auto reg = m_Lwswitch->FindRegister(regName);
    string absRegFieldName = reg->GetName() + regFieldName;
    auto field = std::unique_ptr<IRegisterField>(reg->FindField(absRegFieldName.c_str()));
    if (field == nullptr)
    {
        UtlUtility::PyError("Could not get Field %s value for the register %s",
            regFieldName.c_str(), regName.c_str());
    }
    UINT32 readmask = field->GetReadMask();
    if (!hasRegValue)
    {
        regValue = ControlRead(reg->GetAddress()); 
    }
    regValue &= readmask;
    regValue >>= field->GetStartBit();

    return regValue;
}

UINT32 UtlLwSwitchRegCtrl::SetField
(
    const string& regName,
    const string& regFieldName,
    const string& regFieldValueName,
    bool hasRegFieldValueName,
    UINT32 regFieldValueNum,
    UINT32 regValue,
    bool hasRegValue,
    py::kwargs kwargs
)
{
    bool bcast = false;
    UtlUtility::GetOptionalKwarg<bool>(&bcast, kwargs, "broadcast", "LwSwitchRegCtrl.SetField");
    
    // SetField() will operate on regValue if given (and will not write to the register)
    // This is here to allow setting multiple fields in a register with a single write,
    //  avoiding on-write triggered effects
    UtlGil::Release gil;

    auto reg = m_Lwswitch->FindRegister(regName);
    string absRegFieldName = reg->GetName() + regFieldName;
    auto regfield = std::unique_ptr<IRegisterField>(reg->FindField(absRegFieldName.c_str()));
    if (regfield == nullptr)
    {
        UtlUtility::PyError("Could not get Field %s value for the register %s",
            regFieldName.c_str(), regName.c_str());
    }
    if (hasRegFieldValueName)
    {
        string absRegFieldValueName = absRegFieldName + regFieldValueName;
        auto regFieldVal = std::unique_ptr<IRegisterValue>(
            regfield->FindValue(absRegFieldValueName.c_str())
        );
        if (regFieldVal == nullptr)
        {
            UtlUtility::PyError(
                "Could not get value constant %s for field %s in register %s.",
                regFieldValueName.c_str(), regFieldName.c_str(), regName.c_str());
        }
        else
        {
            regFieldValueNum = regFieldVal->GetValue();
        }
    }
    UINT32 writemask = regfield->GetWriteMask();
    UINT32 startbit = regfield->GetStartBit();

    if (!hasRegValue)
    {
        regValue = ControlRead(reg->GetAddress());
    }

    // overwrite only the specified field
    regValue = (regValue & ~writemask) | (writemask & (regFieldValueNum<<startbit));

    if (!hasRegValue)
    {
        ControlWrite(reg->GetAddress(), bcast, regValue);
    }

    return regValue;
}

bool UtlLwSwitchRegCtrl::TestField
(
    const string& regName,
    const string& regFieldName,
    const string& regFieldValueName,
    bool hasRegFieldValueName,
    UINT32 regFieldValueNum,
    UINT32 regValue,
    bool hasRegValue
)
{
    auto reg =  m_Lwswitch->FindRegister(regName);
    string absRegFieldName = reg->GetName() + regFieldName;
    auto regfield =  std::unique_ptr<IRegisterField>(reg->FindField(absRegFieldName.c_str()));
    if (regfield == nullptr)
    {
        UtlUtility::PyError("Could not get Field %s value for the register %s",
            regFieldName.c_str(), regName.c_str());
    }
    if (hasRegFieldValueName)
    {
        string absRegFieldValueName = absRegFieldName + regFieldValueName;
        auto regval = std::unique_ptr<IRegisterValue>(
            regfield->FindValue(absRegFieldValueName.c_str())
        );
        if (regval == nullptr)
        {
            UtlUtility::PyError(
                "Could not get value constant %s for field %s in register %s.",
                regFieldValueName.c_str(), regFieldName.c_str(), regName.c_str());
        }
        else
        {
            regFieldValueNum = regval->GetValue();
        }
    }

    if (!hasRegValue)
    {
        regValue = ControlRead(reg->GetAddress());
    }

    return ((regValue & regfield->GetReadMask()) >> regfield->GetStartBit())
        == regFieldValueNum;
}

UINT32 UtlLwSwitchRegCtrl::GetFieldDef
(
    const string& regName,
    const string& regFieldName,
    const string& regFieldValueDef
)
{
    UtlUtility::PyError("GetFieldDef() not implelemented for LwSwitchRegCtrl.");

    return 0;
}

UINT32 UtlLwSwitchRegCtrl::ControlRead(UINT32 addr) const
{
#if defined(INCLUDE_LWSWITCH)
    LWSWITCH_REGISTER_READ readParams = {};
    readParams.engine = m_Engine;
    readParams.instance = m_Instance;
    readParams.offset = addr;
    m_Lwswitch->Control(
        LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_REGISTER_READ,
        &readParams,
        sizeof(LWSWITCH_REGISTER_READ)
    );
    return readParams.val;
#else
    return 0;
#endif
}

void UtlLwSwitchRegCtrl::ControlWrite(UINT32 addr, bool bcast, UINT32 val) const
{
#if defined(INCLUDE_LWSWITCH)
    // unwritable fields are silently ignored
    LWSWITCH_REGISTER_WRITE writeParams = {};
    writeParams.engine = m_Engine;
    writeParams.instance = m_Instance;
    writeParams.bcast = bcast;
    writeParams.offset = addr;
    writeParams.val = val;
    m_Lwswitch->Control(
        LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_REGISTER_WRITE,
        &writeParams,
        sizeof(LWSWITCH_REGISTER_WRITE)
    );
#endif
}

void UtlLwSwitchRegCtrl::SetEngine(py::kwargs kwargs)
{
    m_Engine = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs, "engine", "LwSwitchRegCtrl.SetEngine");
    m_Instance = UtlUtility::GetRequiredKwarg<UINT32>(
        kwargs, "instance", "LwSwitchRegCtrl.SetEngine");
}
