/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IMULTIHEAP2_H_
#define _IMULTIHEAP2_H_

#include "ITypes.h"
#include "IIface.h"

class IMultiHeap2 : public IIfaceObject {
public:
    virtual int DeviceAllocSysMem64(LwPciDev dev, LwU064 sz,
                                    LwU032 t, LwU064 align, LwU064*) = 0;
    virtual int AllocSysMem64(LwU064 sz, LwU032 t, LwU064 align, LwU064 *) = 0;
    virtual void FreeSysMem64(LwU064) = 0;

    virtual int DeviceAllocSysMem32(LwPciDev dev, LwU032 sz,
                                    LwU032 t, LwU032 align, LwU032*) = 0;
    virtual int AllocSysMem32(LwU032 sz, LwU032 t, LwU032 align, LwU032 *) = 0;
    virtual void FreeSysMem32(LwU032) = 0;

    virtual bool Support64() = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
