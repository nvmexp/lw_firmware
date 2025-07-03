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

#ifndef _IINTERRUPTMGR_H_
#define _IINTERRUPTMGR_H_

#include "ITypes.h"
#include "IIface.h"

class IInterruptMgr : public IIfaceObject {
public:
    /// Hooks the specified interrupt line to the CPU.
    virtual int HookInterrupt(LwU032 irqNumber) = 0;
    /// Unhooks the specified interrupt line from the CPU.
    virtual int UnhookInterrupt(LwU032 irqNumber) = 0;
    /// Checks if there were any interrupts.
    ///
    /// If any interrupts have olwrred since the last call,
    /// calls IInterrupt3::HandleSpecificInterrupt.
    /// If there were no interrupts, returns immediately.
    /// This function is periodically ilwoked from client (MODS).
    virtual void PollInterrupts() = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
