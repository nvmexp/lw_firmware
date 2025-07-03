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

#include "utlutil.h"
#include "utlregctrl.h"
#include "utlchannel.h"
#include "mdiag/IRegisterMap.h"
#include "mdiag/sysspec.h"

void UtlRegCtrl::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("RegCtrl.GetAddress", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.GetAddress", "ignoreDomainBase", false, "ignore adding domain base. default is false (domain base will be added).");
    kwargs->RegisterKwarg("RegCtrl.GetName", "regAddress", true, "the address of the register");
    kwargs->RegisterKwarg("RegCtrl.GetArraySizes", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.GetArrayStrides", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.Read32", "regName", false, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.Read32", "regAddress", false, "the address of the register");
    // channel is used by UtlGpuRegCtrl only but adding/parsing this argument 
    // in the base class (UtlRegCtrl). It is done this way since UTL 
    // documentation does not list args defined in derived class when the API 
    // is defined in the base class. 
    kwargs->RegisterKwarg("RegCtrl.Read32", "channel", false, "signifies that context register is being accessed and channel's context will be used");
    kwargs->RegisterKwarg("RegCtrl.Write32", "regName", false, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.Write32", "regAddress", false, "the address of the register");
    kwargs->RegisterKwarg("RegCtrl.Write32", "data", true, "the value to be written");
    // channel is used by UtlGpuRegCtrl only but adding/parsing this argument 
    // in the base class (UtlRegCtrl). It is done this way since UTL 
    // documentation does not list args defined in derived class when the API 
    // is defined in the base class.
    kwargs->RegisterKwarg("RegCtrl.Write32", "channel", false, "signifies that context register is being accessed and channel's context will be used");
    kwargs->RegisterKwarg("RegCtrl.GetField", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.GetField", "regFieldName", true, "the name of the register field only");
    kwargs->RegisterKwarg("RegCtrl.GetField", "regValue", false, "the value to be used for reading instead of directly reading the register");
    kwargs->RegisterKwarg("RegCtrl.SetField", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.SetField", "regFieldName", true, "the name of the register field only");
    kwargs->RegisterKwarg("RegCtrl.SetField", "regFieldValueName", false, "the definition of the register field to be set");
    kwargs->RegisterKwarg("RegCtrl.SetField", "regFieldValueNum", false, "the value of the register field to be set");
    kwargs->RegisterKwarg("RegCtrl.SetField", "regValue", false, "the value to be used for setting instead of directly setting the register");
    kwargs->RegisterKwarg("RegCtrl.TestField", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.TestField", "regFieldName", true, "the name of the register field only");
    kwargs->RegisterKwarg("RegCtrl.TestField", "regFieldValueName", false, "the definition of the register field to be compared");
    kwargs->RegisterKwarg("RegCtrl.TestField", "regFieldValueNum", false, "the value of the register field to be compared");
    kwargs->RegisterKwarg("RegCtrl.TestField", "regValue", false, "the value to be used for reading instead of directly setting the register");
    kwargs->RegisterKwarg("RegCtrl.GetFieldDef", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.GetFieldDef", "regFieldName", true, "the name of the register field only");
    kwargs->RegisterKwarg("RegCtrl.GetFieldDef", "regFieldValueDef", true, "the name of the register field defined value");
    kwargs->RegisterKwarg("RegCtrl.GetHwInitValue", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.GetHwInitColdValue", "regName", true, "the name of the register");
    kwargs->RegisterKwarg("RegCtrl.GetHwInitWarmValue", "regName", true, "the name of the register");

    py::class_<UtlRegCtrl> regCtrl(module, "RegCtrl",
        "Generic UTL interface to manipulate registers.");
    regCtrl.def("GetAddress", &UtlRegCtrl::GetAddressPy,
        UTL_DOCSTRING("RegCtrl.GetAddress", "This function will return the address of the register specified by name"));
    regCtrl.def("GetName", &UtlRegCtrl::GetNamePy,
        UTL_DOCSTRING("RegCtrl.GetName", "This function will return the name of the register specified by the address"));
    regCtrl.def("GetArraySizes", &UtlRegCtrl::GetArraySizesPy,
        UTL_DOCSTRING("RegCtrl.GetArraySizes", "This function will return a list of array sizes for each dimension. It will return an empty list for non-array registers."));
    regCtrl.def("GetArrayStrides", &UtlRegCtrl::GetArrayStridesPy,
        UTL_DOCSTRING("RegCtrl.GetArrayStrides", "This function will return a list of array strides for each dimension. It will return an empty list for non-array registers."));
    regCtrl.def("Read32", &UtlRegCtrl::Read32Py,
        UTL_DOCSTRING("RegCtrl.Read32", "This function will read and return the value of a register. The register can be specified by name or an address. Either the name or the address needs to be specified (not both)"));
    regCtrl.def("Write32", &UtlRegCtrl::Write32Py,
        UTL_DOCSTRING("RegCtrl.Write32", "This function will write data to a register. The register can be specified by name or an address. Either the name or the address needs to be specified (not both)"));
    regCtrl.def("GetField", &UtlRegCtrl::GetFieldPy,
        UTL_DOCSTRING("RegCtrl.GetField", "This function will read and return the value of the specified field in the register. If the optional regValue parameter is specified then the parameter's value will be used (instead of reading from the register) to get the field value."));
    regCtrl.def("SetField", &UtlRegCtrl::SetFieldPy,
        UTL_DOCSTRING("RegCtrl.SetField", "This function will set the field in a register to the specified field definition or a value. The field value to be set can be specified either as a string (regFieldValueName) or as a number (regFieldValueNum). Only one of the field value types should be specified. If the optional regValue parameter is specified then the parameter's value will be used (instead of writing to the register) to set the field value and the new regValue would be returned."));
    regCtrl.def("TestField", &UtlRegCtrl::TestFieldPy,
        UTL_DOCSTRING("RegCtrl.TestField", "This function will test if the field in a register is equal to the specified field definition or a value. The field value to be specified can be specified either as a string (regFieldValueName) or as a number (regFieldValueNum). Only one of the field value types should be specified. If the optional regValue parameter is specified then the parameter's value will be used (instead of reading the register) to test the field value."));
    regCtrl.def("GetFieldDef", &UtlRegCtrl::GetFieldDefPy,
        UTL_DOCSTRING("RegCtrl.GetFieldDef", "This function will read and return the defined value of the specified field in the register."));
    regCtrl.def("GetHwInitValue", &UtlRegCtrl::GetHwInitValuePy,
        UTL_DOCSTRING("RegCtrl.GetHwInitValue", "This function will return the hw init value of the register."));
    regCtrl.def("GetHwInitColdValue", &UtlRegCtrl::GetHwInitColdValuePy,
        UTL_DOCSTRING("RegCtrl.GetHwInitColdValue", "This function will return the hw init cold value of the register."));
    regCtrl.def("GetHwInitWarmValue", &UtlRegCtrl::GetHwInitWarmValuePy,
        UTL_DOCSTRING("RegCtrl.GetHwInitWarmValue", "This function will return the hw init warm value of the register."));
}

UINT32 UtlRegCtrl::GetAddressPy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetAddress");
    bool ignoreDomainBase = false;
    UtlUtility::GetOptionalKwarg<bool>(
        &ignoreDomainBase, kwargs, "ignoreDomainBase", "RegCtrl.GetAddress");

    return GetAddress(regName, ignoreDomainBase);
}

string UtlRegCtrl::GetNamePy(py::kwargs kwargs)
{
    UINT32 regAddress = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "regAddress",
        "RegCtrl.GetName");

    UtlGil::Release gil;

    return GetName(regAddress);
}

vector<UINT32> UtlRegCtrl::GetArraySizesPy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetArraySizes");

    UtlGil::Release gil;

    vector<UINT32> arraySizes;
    UINT32 sizeI = 0, sizeJ = 0, strideI = 0, strideJ = 0;
    auto reg = GetRegister(regName);

    if (reg)
    {
        reg->GetFormula2(sizeI, sizeJ, strideI, strideJ);
        if (reg->GetArrayDimensions() == 2)
        {
            arraySizes = {sizeI, sizeJ};
        }
        else if (reg->GetArrayDimensions() == 1)
        {
            arraySizes = {sizeI};
        }
    }
    else
    {
        UtlUtility::PyError("Unrecognized register name %s in RegCtrl.GetArraySizes.", 
            regName.c_str());
    }
    
    return arraySizes;
}

vector<UINT32> UtlRegCtrl::GetArrayStridesPy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetArrayStrides");

    UtlGil::Release gil;

    vector<UINT32> arrayStrides;
    UINT32 sizeI = 0, sizeJ = 0, strideI = 0, strideJ = 0;
    auto reg = GetRegister(regName);

    if (reg)
    {
        reg->GetFormula2(sizeI, sizeJ, strideI, strideJ);
        if (reg->GetArrayDimensions() == 2)
        {
            arrayStrides = {strideI, strideJ};
        }
        else if (reg->GetArrayDimensions() == 1)
        {
            arrayStrides = {strideI};
        }
    }
    else
    {
        UtlUtility::PyError("Unrecognized register name %s in RegCtrl.GetArrayStrides.", 
            regName.c_str());
    }
    
    return arrayStrides;
}

UINT32 UtlRegCtrl::Read32Py(py::kwargs kwargs)
{
    string regName;
    bool hasRegName = UtlUtility::GetOptionalKwarg<string>(
        &regName, kwargs, "regName", "RegCtrl.Read32");

    UINT32 regAddress = ~0U;
    bool hasRegAddress = UtlUtility::GetOptionalKwarg<UINT32>(
        &regAddress, kwargs, "regAddress", "RegCtrl.Read32");

    if ((hasRegName && hasRegAddress) ||
        (!hasRegName && !hasRegAddress))
    {
        UtlUtility::PyError("RegCtrl.Read32 requires exactly one of the following arguments: regName or regAddress.");
    }

    UtlChannel* pUtlChannel = nullptr;
    UtlUtility::GetOptionalKwarg<UtlChannel*>(&pUtlChannel, kwargs, "channel",
        "RegCtrl.Read32");

    UtlGil::Release gil;

    return Read32(regName, hasRegName, regAddress, pUtlChannel);
}

void UtlRegCtrl::Write32Py(py::kwargs kwargs)
{
    string regName;
    bool hasRegName = UtlUtility::GetOptionalKwarg<string>(
        &regName, kwargs, "regName", "RegCtrl.Write32");

    UINT32 regAddress = ~0U;
    bool hasRegAddress = UtlUtility::GetOptionalKwarg<UINT32>(
        &regAddress, kwargs, "regAddress", "RegCtrl.Write32");

    if ((hasRegName && hasRegAddress) ||
        (!hasRegName && !hasRegAddress))
    {
        UtlUtility::PyError("RegCtrl.Write32 requires exactly one of the following arguments: regName or regAddress.");
    }

    UINT32 data = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "data",
        "RegCtrl.Write32");

    UtlChannel* pUtlChannel = nullptr;
    UtlUtility::GetOptionalKwarg<UtlChannel*>(&pUtlChannel, kwargs, "channel",
        "RegCtrl.Write32");

    Write32(regName, hasRegName, regAddress, data, kwargs, pUtlChannel);
}

UINT32 UtlRegCtrl::GetFieldPy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetField");
    string regFieldName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldName",
        "RegCtrl.GetField");

    UINT32 regValue = ~0U;
    bool hasRegValue = UtlUtility::GetOptionalKwarg<UINT32>(
        &regValue, kwargs, "regValue", "RegCtrl.GetField");

    UtlGil::Release gil;

    return GetField(regName, regFieldName, regValue, hasRegValue);
}

UINT32 UtlRegCtrl::SetFieldPy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.SetField");
    string regFieldName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldName",
        "RegCtrl.SetField");

    string regFieldValueName;
    bool hasRegFieldValueName = UtlUtility::GetOptionalKwarg<string>(
        &regFieldValueName, kwargs, "regFieldValueName", "RegCtrl.SetField");

    UINT32 regFieldValueNum = ~0U;
    bool hasRegFieldValueNum = UtlUtility::GetOptionalKwarg<UINT32>(
        &regFieldValueNum, kwargs, "regFieldValueNum", "RegCtrl.SetField");

    if ((hasRegFieldValueName && hasRegFieldValueNum) ||
        (!hasRegFieldValueName && !hasRegFieldValueNum))
    {
        UtlUtility::PyError("RegCtrl.SetField requires exactly one of the following arguments: regFieldValueName or regFieldValueNum.");
    }

    UINT32 regValue = ~0U;
    bool hasRegValue = UtlUtility::GetOptionalKwarg<UINT32>(
        &regValue, kwargs, "regValue", "RegCtrl.SetField");

    return SetField(regName, regFieldName,
        regFieldValueName, hasRegFieldValueName,
        regFieldValueNum, regValue, hasRegValue, kwargs);
}

bool UtlRegCtrl::TestFieldPy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.TestField");
    string regFieldName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldName",
        "RegCtrl.TestField");

    string regFieldValueName;
    bool hasRegFieldValueName = UtlUtility::GetOptionalKwarg<string>(
        &regFieldValueName, kwargs, "regFieldValueName", "RegCtrl.TestField");

    UINT32 regFieldValueNum = ~0U;
    bool hasRegFieldValueNum = UtlUtility::GetOptionalKwarg<UINT32>(
        &regFieldValueNum, kwargs, "regFieldValueNum", "RegCtrl.TestField");

    if ((hasRegFieldValueName && hasRegFieldValueNum) ||
        (!hasRegFieldValueName && !hasRegFieldValueNum))
    {
        UtlUtility::PyError("RegCtrl.TestField requires exactly one of the following arguments: regFieldValueName or regFieldValueNum.");
    }

    UINT32 regValue = ~0U;
    bool hasRegValue = UtlUtility::GetOptionalKwarg<UINT32>(
        &regValue, kwargs, "regValue", "RegCtrl.TestField");

    UtlGil::Release gil;

    return TestField(regName, regFieldName, regFieldValueName,
        hasRegFieldValueName, regFieldValueNum, regValue, hasRegValue);
}

UINT32 UtlRegCtrl::GetFieldDefPy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetFieldDef");
    string regFieldName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldName",
        "RegCtrl.GetFieldDef");
    string regFieldValueDef = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldValueDef",
        "RegCtrl.GetFieldDef");

    UtlGil::Release gil;

    return GetFieldDef(regName, regFieldName, regFieldValueDef);
}

UINT32 UtlRegCtrl::GetHwInitValuePy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetHwInitValue");
        
    UtlGil::Release gil;
    
    UINT32 hwInitValue = 0U;
    auto reg = GetRegister(regName);
    
    if (reg)
    {
        if(!reg->GetHWInitValue(&hwInitValue))
            InfoPrintf("RegCtrl.GetHwInitValue: No HW init value specified for register %s. Value set to 0.\n", regName.c_str());
    }
    else
    {
        UtlUtility::PyError("Unrecognized register name %s in RegCtrl.GetHwInitValue.", 
            regName.c_str());
    }
    
    return hwInitValue;
}

UINT32 UtlRegCtrl::GetHwInitColdValuePy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetHwInitColdValue");
        
    UtlGil::Release gil;
    
    UINT32 hwInitColdValue = 0U;
    auto reg = GetRegister(regName);
    
    if (reg)
    {
        if(!reg->GetHWInitColdValue(&hwInitColdValue))
            InfoPrintf("RegCtrl.GetHwInitColdValue: No HW init cold value specified for register %s. Value set to 0.\n", regName.c_str());
    }
    else
    {
        UtlUtility::PyError("Unrecognized register name %s in RegCtrl.GetHwInitColdValue.", 
            regName.c_str());
    }
    
    return hwInitColdValue;
}
UINT32 UtlRegCtrl::GetHwInitWarmValuePy(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "RegCtrl.GetHwInitWarmValue");
        
    UtlGil::Release gil;
    
    UINT32 hwInitWarmValue = 0U;
    auto reg = GetRegister(regName);
    
    if (reg)
    {
        if(!reg->GetHWInitWarmValue(&hwInitWarmValue))
            InfoPrintf("RegCtrl.GetHwInitWarmValue: No HW init warm value specified for register %s. Value set to 0.\n", regName.c_str());
    }
    else
    {
        UtlUtility::PyError("Unrecognized register name %s in RegCtrl.GetHwInitWarmValue.", 
            regName.c_str());
    }
    
    return hwInitWarmValue;
}
