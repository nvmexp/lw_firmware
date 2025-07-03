/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// DO NOT EDIT
// See https://wiki.lwpu.com/engwiki/index.php/MODS/sim_linkage#How_to_change_ifspec

#ifndef _ICLOCKMGR_H_
#define _ICLOCKMGR_H_

#include "ITypes.h"
#include "IIface.h"

class IClockMgr : public IIfaceObject {
public:
    /// Returns clock handle for the given clock.
    /// All further clock manipulations are done using the returned handle.
    /// @param clkDevice Device name (input)
    /// @param clkController Controller name (input)
    /// @param pHandle Returned handle (output)
    /// @return Returns 0 on success or non-zero on failure
    virtual int GetClockHandle(const char* clkDevice, const char* clkController, LwU064* pHandle) = 0;
    /// Returns current parent of the given clock.
    /// @param handle The clock (input)
    /// @param pParentHandle Returned handle of the parent clock (output)
    /// @return Returns 0 on success or non-zero on failure
    virtual int GetClockParent(LwU064 handle, LwU064* pParentHandle) = 0;
    /// Sets new parent for the given clock.
    /// @param handle The clock (input)
    /// @param parentHandle Handle of the clock that will become the new parent (input)
    /// @return Returns 0 on success or non-zero on failure
    virtual int SetClockParent(LwU064 handle, LwU064 parentHandle) = 0;
    /// Returns current enable state of the given clock.
    /// @param handle The clock (input)
    /// @param pEnabled Returned clock enable count - 0: disabled, >0: enabled (output)
    /// @return Returns 0 on success or non-zero on failure
    virtual int GetClockEnabled(LwU064 handle, LwU032* pEnableCount) = 0;
    /// Enables or disabled the given clock.
    /// @param handle The clock (input)
    /// @param enabled New state - 1: enabled, 0: disabled (input)
    /// @return Returns 0 on success or non-zero on failure
    virtual int SetClockEnabled(LwU064 handle, int enabled) = 0;
    /// Returns current clock rate of the given clock.
    /// @param handle The clock (input)
    /// @param pRateHz Retunred rate in Hz (output)
    /// @return Returns 0 on success or non-zero on failure
    virtual int GetClockRate(LwU064 handle, LwU064* pRateHz) = 0;
    /// Sets clock rate of the given clock.
    /// @param handle The clock (input)
    /// @param rateHz New rate in Hz (input)
    /// @return Returns 0 on success or non-zero on failure
    virtual int SetClockRate(LwU064 handle, LwU064 rateHz) = 0;
    /// Asserts or deasserts reset of the given clock.
    /// @param handle The clock (input)
    /// @param assertReset 1: assert, 0: deassert (input)
    /// @return Returns 0 on success or non-zero on failure
    virtual int AssertClockReset(LwU064 handle, int assertReset) = 0;

    // IIfaceObject Interface
    virtual void AddRef() = 0;
    virtual void Release() = 0;
    virtual IIfaceObject* QueryIface(IID_TYPE id) = 0;
};

#endif
