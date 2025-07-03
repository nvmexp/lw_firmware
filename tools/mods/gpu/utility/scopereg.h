/*
* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2015-2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SCOPEDREG_H
#define INCLUDED_SCOPEDREG_H

#include "gpu/include/gpusbdev.h"

//! Reads the register value at the specified offset, then restores the original
//! value when going out of scope. Can be treated as a UINT32 after creation.
class ScopedRegister
{
public:
    using ArrayIndexes = RegHalTable::ArrayIndexes;

    ScopedRegister(GpuSubdevice *pSubdevice, UINT32 offset)
    : m_pSubdevice(pSubdevice), m_Offset(offset),
      m_SavedValue(m_pSubdevice->RegRd32(m_Offset))
    {
    }

    ScopedRegister(GpuSubdevice *pSubdevice, ModsGpuRegAddress addr)
    : ScopedRegister(pSubdevice, pSubdevice->Regs().LookupAddress(addr))
    {
    }

    ScopedRegister(GpuSubdevice *pSubdevice, ModsGpuRegAddress addr,
                   UINT32 index)
    : ScopedRegister(pSubdevice, pSubdevice->Regs().LookupAddress(addr, index))
    {
    }

    ScopedRegister(GpuSubdevice *pSubdevice, ModsGpuRegAddress addr, ArrayIndexes indexes)
    : ScopedRegister(pSubdevice, pSubdevice->Regs().LookupAddress(addr, indexes))
    {
    }

    ScopedRegister(GpuSubdevice *pSubdevice, ModsGpuRegValue value)
    : m_pSubdevice(pSubdevice                                       ),
      m_Offset    (pSubdevice->Regs().LookupAddress(
                      pSubdevice->Regs().ColwertToAddress(value))),
      m_SavedValue(pSubdevice->RegRd32(m_Offset)                    )
    {
        UINT32 newValue = m_SavedValue;
        pSubdevice->Regs().SetField(&newValue, value);
        pSubdevice->RegWr32(m_Offset, newValue);
    }

    ~ScopedRegister()
    {
        m_pSubdevice->RegWr32(m_Offset, m_SavedValue);
    }

    operator UINT32() const
    {
        return m_SavedValue;
    }

private:
    //! Do not allow copying or assignment.
    ScopedRegister(const ScopedRegister &other);
    ScopedRegister &operator=(ScopedRegister &other);

    GpuSubdevice *m_pSubdevice;
    const UINT32 m_Offset;
    const UINT32 m_SavedValue;
};

#endif /* INCLUDED_SCOPEDREG_H */
