 /*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbmgr.cpp
//! \brief Implementation of class for Smbus manager
//!
//! Contains class implementation for Smbus manager
//!

#include "smbmgr.h"
#include "core/include/platform.h"
#include "core/include/pci.h"
#include "smbdev.h"
#include "device/include/chipset.h"
#include "cheetah/include/logmacros.h"
#include "cheetah/include/tegrareg.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME  "SmbMgr"

namespace SmbMgr
{
    SmbManager d_Manager;
}

SmbManager* SmbMgr::Mgr()
{
    return &SmbMgr::d_Manager;
}

RC SmbManager::PrivateFindDevices()
{
    LOG_ENT();

    RC rc;
    UINT32 domain = 0, bus = 0, dev = 0, fun = 0;
    UINT16 vendorId = 0;
    SmbusMask SmbMask;
    m_ClassCode = CLASS_CODE_SMB2;

    for(UINT32 i=0 ;; i++)
    {
        if(OK !=
           (rc=Platform::FindPciClassCode(m_ClassCode, i, &domain, &bus, &dev, &fun)))
        {
            // only check the first index
            if (i == 0)
            {
                CHECK_RC(Platform::FindPciClassCode(CLASS_CODE_LPC, 0,
                                                    &domain, &bus, &dev, &fun));
                CHECK_RC(Platform::PciRead16(domain, bus, dev, fun,
                                             LW_PCI_VENDOR_ID, &vendorId));
                if (vendorId != Pci::VendorIds::Intel)
                    break;
                // We have an Intel chipset, so unmask smbus
                CHECK_RC(SmbMask.ToggleOn());
                if(OK != (rc=Platform::FindPciClassCode(
                              m_ClassCode, i, &domain, &bus, &dev, &fun)))
                {
                    // Couldn't find smbus on Intel, so restore mask
                    //SmbMask.ToggleOff();
                    break;
                }
            }
            else
                break;
        }
        UINT32 chipVersion;
        if(OK==(rc=PrivateValidateDevice(domain,bus,dev,fun,&chipVersion)))
        {
            //create device
            m_vpDevices.push_back(new SmbDevice(domain,bus,dev,fun,chipVersion));
            //only find the first SMB device
            break;
        }
    }
    for (UINT32 i=0; i < m_vpDevices.size(); i++)
    {
        CHECK_RC(m_vpDevices[i]->ReadInCap());
    }
    if(!m_vpDevices.size())
    {
        Printf(Tee::PriLow, "SMBus controller Not found\n");
        return rc;
    }

    LOG_EXT();
    return OK;
}

RC SmbManager::PrivateValidateDevice
(
    UINT32 DomainNum,
    UINT32 BusNum,
    UINT32 DeviceNum,
    UINT32 FunctionNum,
    UINT32 * ChipVersion
)
{
    LOG_ENT();
    RC rc;
    UINT16 vendorId = 0;
    UINT16 deviceId = 0;

    CHECK_RC(Platform::PciRead16(DomainNum, BusNum, DeviceNum, FunctionNum,
                                 LW_PCI_VENDOR_ID, &vendorId));
    CHECK_RC(Platform::PciRead16(DomainNum, BusNum, DeviceNum, FunctionNum,
                                 LW_PCI_DEVICE_ID, &deviceId));
    Printf(Tee::PriLow,
           "Found (%d, %d, %d, %d) with cc = 0x%0x, vid = 0x%04x, did = 0x%04x\n",
           DomainNum, BusNum, DeviceNum, FunctionNum, m_ClassCode, vendorId, deviceId);

    if (vendorId == Pci::VendorIds::Amd &&
        deviceId == Pci::AmdDeviceIds::KernczSmbusHostController)
    {
        *ChipVersion = Chipset::AMD;
        Printf(Tee::PriLow, "Found AMD KernCZ SMBus Host Controller");
    }
    else if (vendorId == Pci::VendorIds::Intel)
    {
        *ChipVersion = Chipset::INTEL;
        Printf(Tee::PriLow, "INTEL - SMBus: DeviceId: 0x%04x.\n", deviceId);
    }
    else
    {
        Printf(Tee::PriLow,
               "--- Unknown chip (%d, %d, %d, %d) vid = 0x%04x did = 0x%04x---\n",
               DomainNum, BusNum, DeviceNum, FunctionNum, vendorId, deviceId);
        return RC::NOT_LW_DEVICE;
    }
    LOG_EXT();
    return OK;
}
