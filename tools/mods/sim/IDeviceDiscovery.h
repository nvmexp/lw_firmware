/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec
// Note IDeviceDiscovery is deprecated and was never used for anything, keeping it just because it is defined in the HW tree as well.

#ifndef _IDEVICEDISCOVERY_H_
#define _IDEVICEDISCOVERY_H_

#include "ITypes.h"
#include "IIface.h"

class IDeviceDiscovery : public IIfaceObject {
public:
    virtual int HasPciBus() = 0;
    virtual int GetNumDevices() = 0;
    virtual int GetNumBARs(int devIdx) = 0;
    virtual int GetDeviceBAR(int devIdx, int barIdx, LwU064* pPhysAddr, LwU064* pSize) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
