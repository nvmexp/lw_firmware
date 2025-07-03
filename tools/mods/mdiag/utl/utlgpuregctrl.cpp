/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mdiag/lwgpu.h"
#include "mdiag/smc/gpupartition.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/utils/utils.h"

#include "utlgpu.h"
#include "utlgpuregctrl.h"
#include "utlutil.h"
#include "utlchannel.h"

void UtlGpuRegCtrl::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("GpuRegCtrl.SetRegSpace", "regSpace", true, "register space to be set up for the controller");
    kwargs->RegisterKwarg("GpuRegCtrl.Write32", "overrideCtx", false, "boolean to override context; default false");
    kwargs->RegisterKwarg("GpuRegCtrl.SetField", "overrideCtx", false, "boolean to override context; default false");
    kwargs->RegisterKwarg("GpuRegCtrl.GetFieldStartBit", "regName", "true", "the name of the register");
    kwargs->RegisterKwarg("GpuRegCtrl.GetFieldStartBit", "regFieldName", true, "the name of the register field only");
    kwargs->RegisterKwarg("GpuRegCtrl.GetFieldEndBit", "regName", "true", "the name of the register");
    kwargs->RegisterKwarg("GpuRegCtrl.GetFieldEndBit", "regFieldName", true, "the name of the register field only");
    kwargs->RegisterKwarg("GpuRegCtrl.GetFieldWidth", "regName", "true", "the name of the register");
    kwargs->RegisterKwarg("GpuRegCtrl.GetFieldWidth", "regFieldName", true, "the name of the register field only");

    py::class_<UtlGpuRegCtrl, UtlRegCtrl> gpuRegCtrl(module, "GpuRegCtrl",
        "Interface for manipulating GPU registers. Users can get the register controller object using Gpu.GetRegCtrl() or SmcEngine.GetRegCtrl()");
    gpuRegCtrl.def("SetRegSpace", &UtlGpuRegCtrl::SetRegSpace,
        UTL_DOCSTRING("GpuRegCtrl.SetRegSpace", "This function sets up the RegSpace for the register controller."));
    gpuRegCtrl.def("GetFieldStartBit", &UtlGpuRegCtrl::GetFieldStartBit,
        UTL_DOCSTRING("GpuRegCtrl.GetFieldStartBit", "This functions returns the start (LSb) bit position of the specified field in the register."));
    gpuRegCtrl.def("GetFieldEndBit", &UtlGpuRegCtrl::GetFieldEndBit,
        UTL_DOCSTRING("GpuRegCtrl.GetFieldEndBit", "This functions returns the end (MSb) bit position of the specified field in the register."));
    gpuRegCtrl.def("GetFieldWidth", &UtlGpuRegCtrl::GetFieldWidth,
        UTL_DOCSTRING("GpuRegCtrl.GetFieldWidth", "This functions returns the width of the specified field in the register."));
}

// Constructor for the GPU register interface.
//
UtlGpuRegCtrl::UtlGpuRegCtrl(UtlGpu* utlGpu)
{
    m_pGpuSubdevice = utlGpu->GetGpuSubdevice();
    m_pLwRm = LwRmPtr().Get();

    m_pGpuResource = utlGpu->GetGpuResource();
}

// Constructor for the register interface in SMC mode.
//
UtlGpuRegCtrl::UtlGpuRegCtrl(SmcEngine* smcEngine)
{
    m_pSmcEngine = smcEngine;
    GpuPartition* gpuPartition = m_pSmcEngine->GetGpuPartition();
    m_pGpuResource = gpuPartition->GetLWGpuResource();
    m_pLwRm = m_pGpuResource->GetLwRmPtr(smcEngine);

    // Can assume subdevice zero here because SMC can't be in SLI mode.
    m_pGpuSubdevice = m_pGpuResource->GetGpuSubdevice(0);
}

UINT32 UtlGpuRegCtrl::GetRegAddress
(
    const string& regName,
    const string apiName,
    bool ignoreDomainBase
)
{
    UINT32 regAddress = 0;

    if (m_pGpuResource->GetRegNum(regName.c_str(), &regAddress, 
        m_RegSpace.c_str(), m_pGpuSubdevice->GetSubdeviceInst(), m_pLwRm,
        ignoreDomainBase))
    {
        UtlUtility::PyError("Unrecognized register name %s in %s.", 
            regName.c_str(), apiName.c_str());
    }

    return regAddress;
}

UINT32 UtlGpuRegCtrl::GetAddress
(
    const string& regName,
    bool ignoreDomainBase
)
{
    return GetRegAddress(regName, "GpuRegCtrl.GetAddress", ignoreDomainBase);
}

string UtlGpuRegCtrl::GetName
(
    UINT32 regAddress
)
{
    string regName;
    auto reg = m_pGpuResource->GetRegister(regAddress);

    if (reg)
    {
        regName = reg->GetName();
    }

    return regName;
}

unique_ptr<IRegisterClass> UtlGpuRegCtrl::GetRegister
(
    const string& regName
)
{
    return m_pGpuResource->GetRegister(regName.c_str());
}

UINT32 UtlGpuRegCtrl::Read32
(
    const string& regName,
    bool hasRegName, 
    UINT32 regAddress,
    UtlChannel* pUtlChannel
)
{
    if (hasRegName)
    {
        regAddress = GetRegAddress(regName, "GpuRegCtrl.Read32");
    }

    UINT32 data = ~0U;
    // If UtlChannel is specified then it is a context reg access
    if (pUtlChannel)
    {
        RC rc = m_pGpuSubdevice->CtxRegRd32(pUtlChannel->GetHandle(), 
            regAddress, &data, m_pLwRm);
        UtlUtility::PyErrorRC(rc, "GpuRegCtrl.Read32 failed for context register %s.",
            m_pGpuResource->GetRegister(regAddress)->GetName());
    }
    else
    {
        data = m_pGpuResource->RegRd32(regAddress, 
            m_pGpuSubdevice->GetSubdeviceInst(), m_pSmcEngine);
    }
    return data;
}

void UtlGpuRegCtrl::Write32
(
    const string& regName,
    bool hasRegName, 
    UINT32 regAddress,
    UINT32 data,
    py::kwargs kwargs,
    UtlChannel* pUtlChannel
)
{
    bool overrideCtx = false;
    UtlUtility::GetOptionalKwarg<bool>(&overrideCtx, kwargs, "overrideCtx", "GpuRegCtrl.Write32");

    UtlGil::Release gil;

    if (hasRegName)
    {
        regAddress = GetRegAddress(regName, "GpuRegCtrl.Write32");
    }

    if (overrideCtx)
    {
        RC rc = m_pGpuResource->SetContextOverride(regAddress, 0xffffffff, data, m_pSmcEngine);
        UtlUtility::PyErrorRC(rc, "GpuRegCtrl.Write32 failed for context register %s.",
            m_pGpuResource->GetRegister(regAddress)->GetName());
    }

    // If UtlChannel is specified then it is a context reg access
    if (pUtlChannel)
    {
        RC rc = m_pGpuSubdevice->CtxRegWr32(pUtlChannel->GetHandle(), 
            regAddress, data, m_pLwRm);
        UtlUtility::PyErrorRC(rc, "GpuRegCtrl.Write32 failed for context register %s.",
            m_pGpuResource->GetRegister(regAddress)->GetName());
    }
    else
    {
        m_pGpuResource->RegWr32(regAddress, data,
            m_pGpuSubdevice->GetSubdeviceInst(), m_pSmcEngine);
    }
}

UINT32 UtlGpuRegCtrl::GetField
(
    const string& regName,
    const string& regFieldName,
    UINT32 regValue,
    bool hasRegValue
)
{
    UINT32* regValuePtr = nullptr;
    UINT32 regFieldValue = ~0U;
    if (hasRegValue)
    {
        regValuePtr = &regValue; 
    }

    if (m_pGpuResource->GetRegFldNum(
            regName.c_str(),
            regFieldName.c_str(),
            &regFieldValue,
            regValuePtr,
            m_RegSpace.c_str(),
            m_pGpuSubdevice->GetSubdeviceInst(),
            m_pSmcEngine))
    {
        UtlUtility::PyError("Could not get Field %s value for the register %s", 
            regFieldName.c_str(), regName.c_str());
    }

    return regFieldValue;
}

UINT32 UtlGpuRegCtrl::SetField
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
    UINT32* regValuePtr = nullptr;
    bool overrideCtx = false;

    UtlUtility::GetOptionalKwarg<bool>(&overrideCtx, kwargs, "overrideCtx", "GpuRegCtrl.SetField");
    
    if (hasRegValue)
    {
        regValuePtr = &regValue; 
    }

    UtlGil::Release gil;

    if (hasRegFieldValueName)
    {
        if (m_pGpuResource->SetRegFldDef(
                regName.c_str(),
                regFieldName.c_str(),
                regFieldValueName.c_str(),
                regValuePtr,
                m_RegSpace.c_str(),
                m_pGpuSubdevice->GetSubdeviceInst(),
                m_pSmcEngine,
                overrideCtx))
        {
            UtlUtility::PyError("Could not Set Field %s value for the register %s", 
                regFieldName.c_str(), regName.c_str());
        }
    }
    else
    {
        if (m_pGpuResource->SetRegFldNum(
                regName.c_str(),
                regFieldName.c_str(),
                regFieldValueNum,
                regValuePtr,
                m_RegSpace.c_str(),
                m_pGpuSubdevice->GetSubdeviceInst(),
                m_pSmcEngine,
                overrideCtx))
        {
            UtlUtility::PyError("Could not Set Field %s value for the register %s", 
                regFieldName.c_str(), regName.c_str());
        }
    }

    return regValue;

}

bool UtlGpuRegCtrl::TestField
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
    UINT32* regValuePtr = nullptr;
    if (hasRegValue)
    {
        regValuePtr = &regValue; 
    }

    if (hasRegFieldValueName)
    {
        return (m_pGpuResource->TestRegFldDef(
                    regName.c_str(),
                    regFieldName.c_str(),
                    regFieldValueName.c_str(),
                    regValuePtr,
                    m_RegSpace.c_str(),
                    m_pGpuSubdevice->GetSubdeviceInst(),
                    m_pSmcEngine));
    }
    else
    {
        return (m_pGpuResource->TestRegFldNum(
                    regName.c_str(),
                    regFieldName.c_str(),
                    regFieldValueNum,
                    regValuePtr,
                    m_RegSpace.c_str(),
                    m_pGpuSubdevice->GetSubdeviceInst(),
                    m_pSmcEngine));
    }
}

UINT32 UtlGpuRegCtrl::GetFieldDef
(
    const string& regName,
    const string& regFieldName,
    const string& regFieldValueDef
)
{
    UINT32 val = 0;
    int ret = m_pGpuResource->GetRegFldDef(regName.c_str(), 
             regFieldName.c_str(), 
             regFieldValueDef.c_str(),
             &val,
             Gpu::UNSPECIFIED_SUBDEV);
    if (ret != 0)
    {
        UtlUtility::PyError("Can't get field %s value def %s for the register %s", 
            regFieldName.c_str(),
            regFieldValueDef.c_str(),
            regName.c_str());
    }
    return val;
}

void UtlGpuRegCtrl::SetRegSpace(py::kwargs kwargs)
{
    m_RegSpace = UtlUtility::GetRequiredKwarg<string>(kwargs, "regSpace",
        "GpuRegCtrl.SetRegSpace");
}

UINT32 UtlGpuRegCtrl::GetFieldStartBit(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "GpuRegCtrl.GetFieldStartBit");
    string regFieldName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldName",
        "GpuRegCtrl.GetFieldStartBit");

    UINT32 startBit = ~0U;

    RC rc = m_pGpuResource->GetRegFieldBitPositions(regName.c_str(), regFieldName.c_str(),
                &startBit, nullptr, m_pGpuSubdevice->GetSubdeviceInst());
   
    UtlUtility::PyErrorRC(rc, "Unable to get startBit for the field %s within the register %s.",
        regFieldName.c_str(), regName.c_str());

    return startBit;
}

UINT32 UtlGpuRegCtrl::GetFieldEndBit(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "GpuRegCtrl.GetFieldEndBit");
    string regFieldName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldName",
        "GpuRegCtrl.GetFieldEndBit");

    UINT32 endBit = ~0U;

    RC rc = m_pGpuResource->GetRegFieldBitPositions(regName.c_str(), regFieldName.c_str(),
                nullptr, &endBit, m_pGpuSubdevice->GetSubdeviceInst());
  
    UtlUtility::PyErrorRC(rc, "Unable to get endBit for the field %s within the register %s.",
        regFieldName.c_str(), regName.c_str());

    return endBit;
}

UINT32 UtlGpuRegCtrl::GetFieldWidth(py::kwargs kwargs)
{
    string regName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regName",
        "GpuRegCtrl.GetFieldWidth");
    string regFieldName = UtlUtility::GetRequiredKwarg<string>(kwargs, "regFieldName",
        "GpuRegCtrl.GetFieldWidth");

    UINT32 startBit = ~0U;
    UINT32 endBit = ~0U;

    RC rc = m_pGpuResource->GetRegFieldBitPositions(regName.c_str(), regFieldName.c_str(),
                &startBit, &endBit, m_pGpuSubdevice->GetSubdeviceInst());
  
    UtlUtility::PyErrorRC(rc, "Unable to get width for the field %s within the register %s.",
        regFieldName.c_str(), regName.c_str());

    // FIELD widths are inclusive- add 1 here
    return (endBit - startBit + 1);
}
