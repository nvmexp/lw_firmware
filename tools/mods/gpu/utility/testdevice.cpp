/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/gpu.h"
#include "core/include/errormap.h"
#include "gpu/include/testdevice.h"

//-----------------------------------------------------------------------------
TestDevice::TestDevice(Id i)
: m_Regs(this)
, m_Id(i)
{
}

//-----------------------------------------------------------------------------
TestDevice::TestDevice(Id i, Device::LwDeviceId deviceId)
: TestDevice(i)
{
    m_Regs.Initialize(deviceId);
}

//-----------------------------------------------------------------------------
UINT32 TestDevice::BusType()
{
    return Gpu::PciExpress;
}

//-----------------------------------------------------------------------------
RC TestDevice::EndOfLoopCheck(bool captureReference)
{
    Xp::DriverStats driverStats = {};
    RC rc = Xp::GetDriverStats(&driverStats);
    if (rc == RC::UNSUPPORTED_FUNCTION)
    {
        return RC::OK;
    }
    CHECK_RC(rc);

    Printf(Tee::PriLow,
        "EndOfLoopCheck:\n"
        "    AllocationCount = %llu\n"
        "    MappingCount    = %llu\n"
        "    PagesCount      = %llu\n",
        driverStats.allocationCount,
        driverStats.mappingCount,
        driverStats.pagesCount);

    if (captureReference)
    {
        m_ReferenceDriverStats = driverStats;
        return RC::OK;
    }

    if (driverStats.allocationCount > m_ReferenceDriverStats.allocationCount)
    {
        Printf(Tee::PriError, "AllocationCount = %llu > %llu\n",
            driverStats.allocationCount, m_ReferenceDriverStats.allocationCount);
        return RC::MEMORY_LEAK;
    }

    if (driverStats.mappingCount > m_ReferenceDriverStats.mappingCount)
    {
        Printf(Tee::PriError, "MappingCount = %llu > %llu\n",
            driverStats.mappingCount, m_ReferenceDriverStats.mappingCount);
        return RC::MEMORY_LEAK;
    }

    if (driverStats.pagesCount > m_ReferenceDriverStats.pagesCount)
    {
        Printf(Tee::PriError, "PagesCount = %llu > %llu\n",
            driverStats.pagesCount, m_ReferenceDriverStats.pagesCount);
        return RC::MEMORY_LEAK;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC TestDevice::GetTestPhase(UINT32* pPhase)
{
    MASSERT(pPhase);
    *pPhase = 0;
    return OK;
}

//-----------------------------------------------------------------------------
RC TestDevice::Initialize()
{
    return m_Regs.Initialize();
}

//-----------------------------------------------------------------------------
string TestDevice::GetTypeName() const
{
    return TypeToString(GetType());
}

//-----------------------------------------------------------------------------
/* static */ string TestDevice::TypeToString(Type t)
{
    switch (t)
    {
        case TYPE_LWIDIA_GPU:
            return "GPU";
        case TYPE_TEGRA_MFG:
            return "TegraMfg";
        case TYPE_IBM_NPU:
            return "NPU";
        case TYPE_EBRIDGE:
            return "Ebridge";
        case TYPE_LWIDIA_LWSWITCH:
            return "LwSwitch";
        case TYPE_TREX:
            return "TREX";
        default:
            break;
    }
    return "Unknown";
}

//-----------------------------------------------------------------------------
/* virtual */ RC TestDevice::GetChipSkuNumberModifier(string *pStr)
{
    MASSERT(pStr);
    RC rc;

    // Get Chip Sku from vbios
    CHECK_RC(GetChipSkuNumber(pStr));

    // Append SKU modifier, with dash, if one exists
    string mod;
    rc = GetChipSkuModifier(&mod);
    if (rc == RC::OK)
    {
        *pStr += "-" + mod;
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* static */ bool TestDevice::IsPciDeviceInAllowedList(const Id& id)
{
    UINT16 domain = 0;
    UINT16 bus    = 0;
    UINT16 dev    = 0;
    UINT16 func   = 0;
    id.GetPciDBDF(&domain, &bus, &dev, &func);
    return GpuPtr()->IsPciDeviceInAllowedList(domain, bus, dev, func);
}

//-----------------------------------------------------------------------------
TestDevice::ScopedDevInst::ScopedDevInst(const TestDevice* pDev)
{
    const INT32 devId = static_cast<INT32>(pDev->DevInst());
    m_OldId = ErrorMap::DevInst();
    ErrorMap::SetDevInst(devId);
}

//-----------------------------------------------------------------------------
TestDevice::ScopedDevInst::~ScopedDevInst()
{
    ErrorMap::SetDevInst(m_OldId);
}
