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

#ifndef _IINTERRUPTMGR2_H_
#define _IINTERRUPTMGR2_H_

#include "ITypes.h"
#include "IIface.h"
#include "irqinfo.h"

class IInterruptMgr2 : public IIfaceObject {
public:
    /// Hooks the specified interrupt line to the CPU and provides 
    /// interrupt masking info.
    virtual int HookInterrupt(IrqInfo2 irqInfo) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
