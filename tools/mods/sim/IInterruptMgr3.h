/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _IINTERRUPTMGR3_H_
#define _IINTERRUPTMGR3_H_

#include "ITypes.h"
#include "IIface.h"
#include "irqinfo.h"

class IInterruptMgr3 : public IIfaceObject
{
public:
    /// Allocate lwecs IRQ's for a PCI function returns 0 on success
    virtual int AllocateIRQs(
            LwPciDev pciDev,    // PCI's domain, bus, device, function values
            LwU032 lwecs,       // Number of vectors requested
            LwU032 *irqs,       // Returns array of allocated irq's
            LwU032 flags) = 0;

    /// Hooks the specified interrupt line to the CPU and provides
    /// interrupt masking info.
    virtual int HookInterrupt3(IrqInfo2 irqInfo) = 0;

    /// Free any allocated IRQ's
    virtual void FreeIRQs(LwPciDev pciDev) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
