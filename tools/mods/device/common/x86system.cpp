/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cheetah/include/sysutil.h"
#include "cheetah/include/devid.h"
#include "core/include/platform.h"
#include "core/include/pci.h"

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

namespace
{
    typedef map<UINT32, vector<SysUtil::PciInfo> > Container;
    Container m_DeviceIdToPciMap;
};

RC DevIdToClassCode(UINT32 DevId, UINT32& ClassCode)
{
    RC rc = OK;

    // Use the DeviceId for each controller to associate it with a classcode on x86 systems
    switch (DevId)
    {
        case LW_DEVID_SATA:
            ClassCode = CLASS_CODE_AHCI;
            break;
        case LW_DEVID_HDA:
            ClassCode = CLASS_CODE_AZA;
            break;
        case LW_DEVID_USB:
            ClassCode = CLASS_CODE_USB2;
            break;
        case LW_DEVID_XUSB_HOST:
            ClassCode = CLASS_CODE_XHCI;
            break;
        case LW_DEVID_XUSB_DEV:
            ClassCode = CLASS_CODE_XUSB;
            break;
        default:
            Printf(Tee::PriError, "Invalid DevId: 0x%08x\n", DevId);
            return RC::BAD_PARAMETER;
    }

    return rc;
}

RC GetBar0Size(UINT32 DevId, UINT32 &BarSize)
{

    RC rc = OK;

    // Use the DeviceId for each controller to associate it with a the bar0 size on x86 systems
    switch (DevId)
    {
        case LW_DEVID_SATA:
            BarSize = 0x100 + 0x80 * 32;
            break;
        case LW_DEVID_HDA:
            BarSize = 0x3000;
            break;
        case LW_DEVID_USB:
            BarSize = 0x100;
            break;
        case LW_DEVID_XUSB_HOST:
            BarSize = 0x10000;
            break;
        case LW_DEVID_XUSB_DEV:
            BarSize = 0x10000;
            break;
        default:
            Printf(Tee::PriError, "Invalid DevId: 0x%08x\n", DevId);
            return RC::BAD_PARAMETER;
    }

    return rc;
}

RC SysUtil::CfgRd08(const UINT32 DeviceId, const UINT32 Index,
                    const UINT32 Offset, UINT08 *pData,
                    const void* virtBase)
{
    RC rc = OK;

    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC(Platform::PciRead08(findIt->second[Index].DomainNumber,
                                     findIt->second[Index].BusNumber,
                                     findIt->second[Index].DeviceNumber,
                                     findIt->second[Index].FunctionNumber,
                                     Offset, pData));
    }

    return rc;
}

RC SysUtil::CfgRd16(const UINT32 DeviceId, const UINT32 Index,
                    const UINT32 Offset, UINT16 *pData,
                    const void* virtBase)
{
    RC rc = OK;

    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC(Platform::PciRead16(findIt->second[Index].DomainNumber,
                                     findIt->second[Index].BusNumber,
                                     findIt->second[Index].DeviceNumber,
                                     findIt->second[Index].FunctionNumber,
                                     Offset, pData));
    }

    return rc;
}

RC SysUtil::CfgRd32(const UINT32 DeviceId, const UINT32 Index,
                    const UINT32 Offset, UINT32 *pData,
                    const void* virtBase)
{
    RC rc = OK;

    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC(Platform::PciRead32(findIt->second[Index].DomainNumber,
                                     findIt->second[Index].BusNumber,
                                     findIt->second[Index].DeviceNumber,
                                     findIt->second[Index].FunctionNumber,
                                     Offset, pData));
    }

    return rc;
}

RC SysUtil::CfgWr08(const UINT32 DeviceId, const UINT32 Index,
                    const UINT32 Offset, const UINT08 Data,
                    void* virtBase)
{
    RC rc = OK;

    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC(Platform::PciWrite08(findIt->second[Index].DomainNumber,
                                      findIt->second[Index].BusNumber,
                                      findIt->second[Index].DeviceNumber,
                                      findIt->second[Index].FunctionNumber,
                                      Offset, Data));
    }

    return rc;
}

RC SysUtil::CfgWr16(const UINT32 DeviceId, const UINT32 Index,
                    const UINT32 Offset, const UINT16 Data,
                    void* virtBase)
{
    RC rc = OK;

    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC(Platform::PciWrite16(findIt->second[Index].DomainNumber,
                                      findIt->second[Index].BusNumber,
                                      findIt->second[Index].DeviceNumber,
                                      findIt->second[Index].FunctionNumber,
                                      Offset, Data));
    }

    return rc;
}

RC SysUtil::CfgWr32(const UINT32 DeviceId, const UINT32 Index,
                    const UINT32 Offset, const UINT32 Data,
                    void* virtBase)
{
    RC rc = OK;

    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        return RC::SOFTWARE_ERROR;
    }
    else
    {
        CHECK_RC(Platform::PciWrite32(findIt->second[Index].DomainNumber,
                                      findIt->second[Index].BusNumber,
                                      findIt->second[Index].DeviceNumber,
                                      findIt->second[Index].FunctionNumber,
                                      Offset, Data));
    }

    return rc;
}

RC SysUtil::GetControllerInfo(const UINT32 DeviceId,
                              ControllerInfo* const ControllerData,
                              UINT32 *pNumControllers)

{
    RC rc = OK;

    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        CHECK_RC(FindControllers(DeviceId));
        findIt = m_DeviceIdToPciMap.find(DeviceId);
    }

    if (findIt->second.size() > MAX_NUM_CONTROLLERS_OF_ONE_TYPE)
    {
        Printf(Tee::PriError, "Software doesn't support %u controllers", (UINT32)findIt->second.size());
        return RC::SOFTWARE_ERROR;
    }

    SysUtil::ControllerInfo ctrlInfo = {0};
    for (*pNumControllers = 0; *pNumControllers < findIt->second.size(); ++(*pNumControllers))
    {
        ctrlInfo.DeviceId = DeviceId;
        ctrlInfo.DeviceIndex = *pNumControllers;

        Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
        if (findIt == m_DeviceIdToPciMap.end())
        {
            return RC::SOFTWARE_ERROR;
        }
        UINT64 baseAddress = 0;
        UINT64 barSize = 0;
        CHECK_RC(Platform::PciGetBarInfo(
                 findIt->second[*pNumControllers].DomainNumber,
                 findIt->second[*pNumControllers].BusNumber,
                 findIt->second[*pNumControllers].DeviceNumber,
                 findIt->second[*pNumControllers].FunctionNumber,
                 0,
                 &baseAddress,
                 &barSize));
        ctrlInfo.RegisterPhysBase = baseAddress;
        ctrlInfo.RegisterMapSize  = static_cast<UINT32>(barSize);
        ctrlInfo.RegisterPhysBase &= ~0xf;
        // see also Figure 6-5 of PCI Local Bus Specification

        CHECK_RC(Platform::PciGetIRQ(
                 findIt->second[*pNumControllers].DomainNumber,
                 findIt->second[*pNumControllers].BusNumber,
                 findIt->second[*pNumControllers].DeviceNumber,
                 findIt->second[*pNumControllers].FunctionNumber,
                 &ctrlInfo.InterruptNum));

        ControllerData[*pNumControllers] = ctrlInfo;
    }

    return rc;
}

RC SysUtil::FindControllers(UINT32 DeviceId)
{
    RC rc = OK;

    UINT32 ClassCode = 0;
    CHECK_RC(DevIdToClassCode(DeviceId, ClassCode));

    UINT32 Domain, Bus, Device, Function;
    for (UINT32 idx = 0; idx < MAX_NUM_CONTROLLERS_OF_ONE_TYPE; ++idx)
    {
        rc = Platform::FindPciClassCode(ClassCode, idx, &Domain, &Bus, &Device, &Function);
        if (rc != OK)
        {
            break;
        }

        // TODO FindPciClassCode should return UINT32, not INT32;
        // for now, cast to UINT32 to avoid "narrowing colwersion" error in
        // C++11 mode, in the future we should update FindPciClassCode
        // interface.
        SysUtil::PciInfo pciInfo = {
            static_cast<UINT32>(Domain),
            static_cast<UINT32>(Bus),
            static_cast<UINT32>(Device),
            static_cast<UINT32>(Function)
        };
        m_DeviceIdToPciMap[DeviceId].push_back(pciInfo);
    }

    if (m_DeviceIdToPciMap[DeviceId].size() == 0)
    {
        Printf(Tee::PriDebug, "Could not find controller with device id %u\n", DeviceId);
        return RC::DEVICE_NOT_FOUND;
    }
    else
    {
        return OK;
    }
}

RC SysUtil::GetBarOffset(const UINT32 DeviceId, const UINT08 BarIndex,
                         UINT32 *pBarOffset)
{
    *pBarOffset = 0;
    return OK;
}

RC SysUtil::PrintSystemInfo()
{
    RC rc = OK;

    Printf(Tee::PriHigh, "ClassCode\tDomain/Bus/Dev/Func\n");
    Printf(Tee::PriHigh, "---------\t------------\n");

    for (Container::const_iterator itr = m_DeviceIdToPciMap.begin();
         itr != m_DeviceIdToPciMap.end();
         ++itr)
    {
        for (UINT32 idx = 0; idx < itr->second.size(); ++idx)
        {
            Printf(Tee::PriNormal, "0x%08x\t%d/%d/%d/%d\n",
                   itr->first,
                   itr->second[idx].DomainNumber,
                   itr->second[idx].BusNumber,
                   itr->second[idx].DeviceNumber,
                   itr->second[idx].FunctionNumber);
        }
    }

    return rc;
}

RC SysUtil::GetPciInfo(const UINT32 DeviceId, const UINT32 Index,
                       struct PciInfo *pPciInfo)
{
    Container::const_iterator findIt = m_DeviceIdToPciMap.find(DeviceId);
    if (findIt == m_DeviceIdToPciMap.end())
    {
        Printf(Tee::PriError, "Device (0x%08x) has not been found", DeviceId);
        return RC::SOFTWARE_ERROR;
    }

    pPciInfo->DomainNumber = findIt->second[Index].DomainNumber;
    pPciInfo->BusNumber = findIt->second[Index].BusNumber;
    pPciInfo->DeviceNumber = findIt->second[Index].DeviceNumber;
    pPciInfo->FunctionNumber = findIt->second[Index].FunctionNumber;

    return OK;
}

