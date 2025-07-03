/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "reghal.h"
#include "ctrl/ctrl2080/ctrl2080gr.h"

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief HAL (Hardware Abstraction Layer) for accessing registers in a SMC
//!
//! This class implements SMC specific functionality necessary for accessing
//! registers in a SMC where registers can be virtualized
class SmcRegHal : public RegHal
{
public:
    enum
    {
        IlwalidId = ~0U
    };

    SmcRegHal(GpuSubdevice* pSubdev, LwRm *pLwRm, UINT32 swizzId = IlwalidId, UINT32 smcEngineId = IlwalidId);

    // Non-copyable
    SmcRegHal(const SmcRegHal& rhs)            = delete;
    SmcRegHal& operator=(const SmcRegHal& rhs) = delete;

    // Ensure that RegHal functions do not get hidden by the overrides as an override of a
    // function will hide all versions in the base class even if the signature is not
    // identical
    using RegHal::Read32;
    using RegHal::Write32;
    using RegHal::Write32Sync;

    UINT32 Read32
    (
        UINT32            domainIndex,
        ModsGpuRegAddress regAddr,
        ArrayIndexes      regIndexes
    ) const override;
    void Write32
    (
        UINT32            domainIndex,
        RegSpace          space,
        ModsGpuRegAddress regAddr,
        ArrayIndexes      regIndexes,
        UINT32            value
    ) override;
    void Write32Sync
    (
        UINT32            domainIndex,
        ModsGpuRegAddress regAddr,
        ArrayIndexes      regIndexes,
        UINT32            value
    ) override;
    void Write32
    (
        UINT32          domainIndex,
        RegSpace        space,
        ModsGpuRegField regField,
        ArrayIndexes    regIndexes,
        UINT32          value
    ) override;
    void Write32Sync
    (
        UINT32          domainIndex,
        ModsGpuRegField regField,
        ArrayIndexes    regIndexes,
        UINT32          value
    ) override;

    LwRm * GetLwRmPtr() const override { return m_pLwRm; }
    UINT32 GetSwizzId() const { return m_SwizzId; }
    UINT32 GetSmcEngineId() const { return m_SmcEngineId; }
    RC PollRegValue(ModsGpuRegAddress Address, UINT32 Value, 
        UINT32 Mask, FLOAT64 TimeoutMs) const;
private:
    bool IsSmcReg(ModsGpuRegAddress, ArrayIndexes regIndexes) const;
    bool IsSmcReg(ModsGpuRegField, ArrayIndexes regIndexes) const;
    bool IsSmcReg(ModsGpuRegValue, ArrayIndexes regIndexes) const;
    UINT32 ReadPgraphRegister(UINT32 offset) const;
    void   WritePgraphRegister(UINT32 offset, UINT32 data) const;

    GpuSubdevice * m_pSubdev      = nullptr;
    LwRm         * m_pLwRm        = nullptr;
    UINT32         m_SwizzId      = 0;
    UINT32         m_SmcEngineId  = 0;
};

inline bool SmcRegHal::IsSmcReg(ModsGpuRegField field, ArrayIndexes regIndexes) const
{
    return IsSmcReg(ColwertToAddress(field), regIndexes);
}

inline bool SmcRegHal::IsSmcReg(ModsGpuRegValue value, ArrayIndexes regIndexes) const
{
    return IsSmcReg(ColwertToAddress(value), regIndexes);
}
