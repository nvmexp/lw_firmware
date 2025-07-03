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

#ifndef _IINTERRUPTMASK_H_
#define _IINTERRUPTMASK_H_

#include "ITypes.h"
#include "IIface.h"
#include "irqinfo.h"

class IInterruptMask : public IIfaceObject {
public:
    /// Specify how to mask device specific interrupts
    virtual int SetInterruptMask(LwU032 irqNumber,
        LwU064 barAddr,
        LwU032 barSize,
        LwU032 regOffset,
        LwU064 andMask,
        LwU064 orMask,
        LwU008 irqType,
        LwU008 maskType,
        LwU016 domain,
        LwU016 bus,
        LwU016 device,
        LwU016 function)=0;

    virtual int SetInterruptMultiMask(
        LwU032 irqNumber,
        LwU008 irqType,
        LwU064 barAddr,
        LwU032 barSize,
        const PciInfo* pciInfo,
        LwU032 maskInfoCount,
        const MaskInfo* pMaskInfoList)=0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
