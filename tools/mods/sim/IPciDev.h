/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IPCIDEV_H_
#define _IPCIDEV_H_

#include "ITypes.h"
#include "IIface.h"

class IPciDev : public IIfaceObject {
public:
    virtual LwErr GetPciBarInfo(LwPciDev pciDevice, int index, LwU064* pAddress, LwU064* pSize) { return LW_FAIL; }
    virtual LwErr GetPciIrq(LwPciDev pciDevice, LwU032* pIrq) { return LW_FAIL; }
    virtual LwErr GetPciMappedPhysicalAddress(LwU064 address, LwU032 offset, LwU064* pMappedAddress) { return LW_FAIL; }

    virtual LwErr FindPciDevice(LwU016 vendorId, LwU016 deviceId, int index, LwPciDev* pciDev) { return LW_FAIL; }
    virtual LwErr FindPciClassCode(LwU032 classCode, int index, LwPciDev* pciDev) { return LW_FAIL; }

    virtual LwErr PciCfgRd08(LwPciDev pciDev, LwU032 address, LwU008* pData) { return LW_FAIL; }
    virtual LwErr PciCfgRd16(LwPciDev pciDev, LwU032 address, LwU016* pData) { return LW_FAIL; }
    virtual LwErr PciCfgRd32(LwPciDev pciDev, LwU032 address, LwU032* pData) { return LW_FAIL; }

    virtual LwErr PciCfgWr08(LwPciDev pciDev, LwU032 address, LwU008 data) { return LW_FAIL; }
    virtual LwErr PciCfgWr16(LwPciDev pciDev, LwU032 address, LwU016 data) { return LW_FAIL; }
    virtual LwErr PciCfgWr32(LwPciDev pciDev, LwU032 address, LwU032 data) { return LW_FAIL; }

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
