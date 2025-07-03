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

#ifndef _IPPCDEV_H_
#define _IPPCDEV_H_

#include "ITypes.h"
#include "IIface.h"

enum TCE_BYPASS_MODE
{
    TCE_BYPASS_DEFAULT
   ,TCE_BYPASS_ON
   ,TCE_BYPASS_OFF
};

class IPpc : public IIfaceObject {
public:
    virtual LwErr SetupDmaBase(LwPciDev pciDevice, TCE_BYPASS_MODE mode, LwU064 devDmaMask, LwU064 *pDmaBase) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
