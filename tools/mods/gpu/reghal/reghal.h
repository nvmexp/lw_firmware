/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_REGHAL_H
#define INCLUDED_REGHAL_H

#include "mods_reg_hal.h"
#include "reghaltable.h"
#include "core/include/types.h"
#ifndef MATS_STANDALONE
#include "core/include/lwrm.h"
#endif

// Uncomment in order to detect whether mods is accessing any
// priv-protected registers without checking its priv level first.

class TestDevice;

//--------------------------------------------------------------------
//! \brief HAL (Hardware Abstraction Layer) for accessing registers
//!
//! This class uses a lookup table to read and write registers, so
//! that the same code can be used to control different devices even if
//! the registers were rearranged.  Each device has a different HAL table.
//!
//! There are three enums used to lookup entries in the HAL table:
//! - ModsGpuRegAddress: The offset of a register in BAR0
//! - ModsGpuRegField:   A bit field within a register
//! - ModsGpuRegValue:   A value that can be stored within a bit field
//!
//! The enum values have the same names as the macros in
//! <branch>/drivers/common/inc/hwref, except that they start with
//! "MODS_" instead of "LW_" (e.g. MODS_PMC_BOOT_0 instead of
//! LW_PMC_BOOT_0).  See reghal.def for more details.
//!
//! Unless otherwise specified, the methods MASSERT if the requested
//! register isn't supported on this GPU.  You can use IsSupported()
//! to check whether a register is supported.
//!
class RegHal : public RegHalTable
{
public:
    RegHal();
    RegHal(TestDevice* pTestDevice);
    ~RegHal();

    // Non-copyable
    RegHal(const RegHal& rhs)            = delete;
    RegHal& operator=(const RegHal& rhs) = delete;

    RC Initialize();
    RC Initialize(TestDevice* pTestDevice);
    using RegHalTable::Initialize;

    // *********************************************
    // * Methods for reading and writing registers *
    // *********************************************
    // Interface to get/set privCheck mode
    static UINT32 GetDebugPrivCheckMode() { return s_DebugPrivCheckMode; };
    static RC SetDebugPrivCheckMode(UINT32 mode) { s_DebugPrivCheckMode = mode; return RC::OK;};
    
    // Read a register.
    UINT32 Read32(ModsGpuRegAddress)                          const;
    UINT32 Read32(ModsGpuRegAddress, UINT32 regIndex)         const;
    UINT32 Read32(ModsGpuRegAddress, ArrayIndexes regIndexes) const;

    UINT32 Read32(UINT32 domainIndex, ModsGpuRegAddress)                          const;
    UINT32 Read32(UINT32 domainIndex, ModsGpuRegAddress, UINT32 regIndex)         const;
    virtual UINT32 Read32(UINT32 domainIndex, ModsGpuRegAddress, ArrayIndexes regIndexes) const;

    RC Read32Priv(ModsGpuRegAddress, UINT32* pValue)                  const;
    RC Read32Priv(ModsGpuRegAddress, UINT32 regIndex, UINT32* pValue) const;

    // Read a register field.
    UINT32 Read32(ModsGpuRegField)                          const;
    UINT32 Read32(ModsGpuRegField, UINT32 regIndex)         const;
    UINT32 Read32(ModsGpuRegField, ArrayIndexes regIndexes) const;

    UINT32 Read32(UINT32 domainIndex, ModsGpuRegField)                          const;
    UINT32 Read32(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex)         const;
    UINT32 Read32(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes) const;

    RC Read32Priv(ModsGpuRegField, UINT32* pValue) const;

    // Write a register.
    void Write32(ModsGpuRegAddress,                          UINT32 value);
    void Write32(ModsGpuRegAddress, UINT32 regIndex,         UINT32 value);
    void Write32(ModsGpuRegAddress, ArrayIndexes regIndexes, UINT32 value);

    void Write32(UINT32 domainIndex, ModsGpuRegAddress,                          UINT32 value);
    void Write32(UINT32 domainIndex, ModsGpuRegAddress, UINT32 regIndex,         UINT32 value);
    void Write32(UINT32 domainIndex, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT32 value);

    void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegAddress,                          UINT32 value);
    void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, UINT32 regIndex,         UINT32 value);
    virtual void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT32 value);

    RC Write32Priv(ModsGpuRegAddress,                  UINT32 value);
    RC Write32Priv(ModsGpuRegAddress, UINT32 regIndex, UINT32 value);

    // Write a register value after checking the priv level
    RC Write32Priv(ModsGpuRegValue);
    RC Write32Priv(ModsGpuRegValue, UINT32 regIndex);
    RC Write32Priv(ModsGpuRegValue, ArrayIndexes regIndexes);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegValue);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegValue, UINT32 regIndex);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegValue, ArrayIndexes regIndexes);
    RC Write32Priv(UINT32 domainIndex, RegSpace, ModsGpuRegValue value, ArrayIndexes arrayIndexes);
    
    // Write a register field.
    RC Write32Priv(ModsGpuRegField,                          UINT32 value);
    RC Write32Priv(ModsGpuRegField, UINT32 regIndex,         UINT32 value);
    RC Write32Priv(ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegField,                          UINT32 value);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex,         UINT32 value);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value);
    RC Write32Priv(UINT32 domainIndex, RegSpace, ModsGpuRegField,   ArrayIndexes arrayIndexes, UINT32 value);
    
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegAddress,                          UINT32 value);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegAddress, UINT32 regIndex,         UINT32 value);
    RC Write32Priv(UINT32 domainIndex, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT32 value);
    RC Write32Priv(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, ArrayIndexes arrayIndexes, UINT32 value);

    void Write32Broadcast(UINT32 domainIndex, RegSpace, ModsGpuRegAddress,                          UINT32 value);
    void Write32Broadcast(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, UINT32 regIndex,         UINT32 value);
    virtual void Write32Broadcast(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT32 value);

    void Write32Sync(ModsGpuRegAddress,                          UINT32 value);
    void Write32Sync(ModsGpuRegAddress, UINT32 regIndex,         UINT32 value);
    void Write32Sync(ModsGpuRegAddress, ArrayIndexes regIndexes, UINT32 value);

    void Write32Sync(UINT32 domainIndex, ModsGpuRegAddress,                          UINT32 value);
    void Write32Sync(UINT32 domainIndex, ModsGpuRegAddress, UINT32 regIndex,         UINT32 value);
    virtual void Write32Sync(UINT32 domainIndex, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT32 value);

    RC Write32PrivSync(ModsGpuRegAddress,                  UINT32 value);
    RC Write32PrivSync(ModsGpuRegAddress, UINT32 regIndex, UINT32 value);

    // Write a register field.
    void Write32(ModsGpuRegField,                          UINT32 value);
    void Write32(ModsGpuRegField, UINT32 regIndex,         UINT32 value);
    void Write32(ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value);

    void Write32(UINT32 domainIndex, ModsGpuRegField,                          UINT32 value);
    void Write32(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex,         UINT32 value);
    void Write32(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value);

    void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegField,                          UINT32 value);
    void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegField, UINT32 regIndex,         UINT32 value);
    virtual void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value);

    void Write32Sync(ModsGpuRegField,                          UINT32 value);
    void Write32Sync(ModsGpuRegField, UINT32 regIndex,         UINT32 value);
    void Write32Sync(ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value);

    void Write32Sync(UINT32 domainIndex, ModsGpuRegField,                          UINT32 value);
    void Write32Sync(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex,         UINT32 value);
    virtual void Write32Sync(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value);

    RC Write32PrivSync(ModsGpuRegField, UINT32 value);

    // Write a register value
    void Write32(ModsGpuRegValue);
    void Write32(ModsGpuRegValue, UINT32 regIndex);
    void Write32(ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write32(UINT32 domainIndex, ModsGpuRegValue);
    void Write32(UINT32 domainIndex, ModsGpuRegValue, UINT32 regIndex);
    void Write32(UINT32 domainIndex, ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegValue);
    void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegValue, UINT32 regIndex);
    void Write32(UINT32 domainIndex, RegSpace, ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write32Sync(ModsGpuRegValue);
    void Write32Sync(ModsGpuRegValue, UINT32 regIndex);
    void Write32Sync(ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write32Sync(UINT32 domainIndex, ModsGpuRegValue);
    void Write32Sync(UINT32 domainIndex, ModsGpuRegValue, UINT32 regIndex);
    void Write32Sync(UINT32 domainIndex, ModsGpuRegValue, ArrayIndexes regIndexes);

    // Check whether a register field has the specified value.
    // Return false if the register/field/value isn't supported on this GPU.
    bool Test32(ModsGpuRegField,                          UINT32 value) const;
    bool Test32(ModsGpuRegField, UINT32 regIndex,         UINT32 value) const;
    bool Test32(ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value) const;

    bool Test32(UINT32 domainIndex, ModsGpuRegField,                          UINT32 value) const;
    bool Test32(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex,         UINT32 value) const;
    bool Test32(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes, UINT32 value) const;

    bool Test32(ModsGpuRegValue)                          const;
    bool Test32(ModsGpuRegValue, UINT32 regIndex)         const;
    bool Test32(ModsGpuRegValue, ArrayIndexes regIndexes) const;

    bool Test32(UINT32 domainIndex, ModsGpuRegValue)                          const;
    bool Test32(UINT32 domainIndex, ModsGpuRegValue, UINT32 regIndex)         const;
    bool Test32(UINT32 domainIndex, ModsGpuRegValue, ArrayIndexes regIndexes) const;

    // **************************
    // * 64 bit implementations *
    // **************************

    // Read a register.
    UINT64 Read64(ModsGpuRegAddress)                          const;
    UINT64 Read64(ModsGpuRegAddress, UINT32 regIndex)         const;
    UINT64 Read64(ModsGpuRegAddress, ArrayIndexes regIndexes) const;

    UINT64 Read64(UINT32 domainIndex, ModsGpuRegAddress)                          const;
    UINT64 Read64(UINT32 domainIndex, ModsGpuRegAddress, UINT32 regIndex)         const;
    virtual UINT64 Read64(UINT32 domainIndex, ModsGpuRegAddress, ArrayIndexes regIndexes) const;

    // Read a register field.
    UINT64 Read64(ModsGpuRegField)                          const;
    UINT64 Read64(ModsGpuRegField, UINT32 regIndex)         const;
    UINT64 Read64(ModsGpuRegField, ArrayIndexes regIndexes) const;

    UINT64 Read64(UINT32 domainIndex, ModsGpuRegField)                          const;
    UINT64 Read64(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex)         const;
    UINT64 Read64(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes) const;

    // Write a register.
    void Write64(ModsGpuRegAddress,                          UINT64 value);
    void Write64(ModsGpuRegAddress, UINT32 regIndex,         UINT64 value);
    void Write64(ModsGpuRegAddress, ArrayIndexes regIndexes, UINT64 value);

    void Write64(UINT32 domainIndex, ModsGpuRegAddress,                          UINT64 value);
    void Write64(UINT32 domainIndex, ModsGpuRegAddress, UINT32 regIndex,         UINT64 value);
    void Write64(UINT32 domainIndex, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT64 value);

    void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegAddress,                          UINT64 value);
    void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, UINT32 regIndex,         UINT64 value);
    virtual void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT64 value);

    void Write64Broadcast(UINT32 domainIndex, RegSpace, ModsGpuRegAddress,                          UINT64 value);
    void Write64Broadcast(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, UINT32 regIndex,         UINT64 value);
    virtual void Write64Broadcast(UINT32 domainIndex, RegSpace, ModsGpuRegAddress, ArrayIndexes regIndexes, UINT64 value);

    void Write64Sync(ModsGpuRegAddress,                          UINT64 value);
    void Write64Sync(ModsGpuRegAddress, UINT32 regIndex,         UINT64 value);
    void Write64Sync(ModsGpuRegAddress, ArrayIndexes regIndexes, UINT64 value);

    void Write64Sync(UINT32 domainIndex, ModsGpuRegAddress,                          UINT64 value);
    void Write64Sync(UINT32 domainIndex, ModsGpuRegAddress, UINT32 regIndex,         UINT64 value);
    virtual void Write64Sync
    (
        UINT32            domainIndex,
        ModsGpuRegAddress regAddr,
        ArrayIndexes      regIndexes,
        UINT64            value
    );

    // Determine whether a priv-protected register is readable or read/writable
    bool HasReadAccess(ModsGpuRegAddress)                          const;
    bool HasReadAccess(ModsGpuRegAddress, UINT32 regIndex)         const;
    bool HasReadAccess(ModsGpuRegAddress, ArrayIndexes regIndexes) const;
    bool HasReadAccess(UINT32 domainIndex,
                       ModsGpuRegAddress)                          const;
    bool HasReadAccess(UINT32 domainIndex,
                       ModsGpuRegAddress, UINT32 regIndex)         const;
    bool HasReadAccess(UINT32 domainIndex,
                       ModsGpuRegAddress, ArrayIndexes regIndexes) const;

    bool HasRWAccess(ModsGpuRegAddress)                          const;
    bool HasRWAccess(ModsGpuRegAddress, UINT32 regIndex)         const;
    bool HasRWAccess(ModsGpuRegAddress, ArrayIndexes regIndexes) const;
    bool HasRWAccess(UINT32 domainIndex,
                     ModsGpuRegAddress)                          const;
    bool HasRWAccess(UINT32 domainIndex,
                     ModsGpuRegAddress, UINT32 regIndex)         const;
    bool HasRWAccess(UINT32 domainIndex,
                     ModsGpuRegAddress, ArrayIndexes regIndexes) const;

    // Write a register field.
    void Write64(ModsGpuRegField,                          UINT64 value);
    void Write64(ModsGpuRegField, UINT32 regIndex,         UINT64 value);
    void Write64(ModsGpuRegField, ArrayIndexes regIndexes, UINT64 value);

    void Write64(UINT32 domainIndex, ModsGpuRegField,                          UINT64 value);
    void Write64(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex,         UINT64 value);
    void Write64(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes, UINT64 value);

    void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegField,                          UINT64 value);
    void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegField, UINT32 regIndex,         UINT64 value);
    virtual void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegField, ArrayIndexes regIndexes, UINT64 value);

    void Write64Sync(ModsGpuRegField,                          UINT64 value);
    void Write64Sync(ModsGpuRegField, UINT32 regIndex,         UINT64 value);
    void Write64Sync(ModsGpuRegField, ArrayIndexes regIndexes, UINT64 value);

    void Write64Sync(UINT32 domainIndex, ModsGpuRegField,                          UINT64 value);
    void Write64Sync(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex,         UINT64 value);
    virtual void Write64Sync
    (
        UINT32          domainIndex,
        ModsGpuRegField regField,
        ArrayIndexes    regIndexes,
        UINT64          value
    );

    void Write64(ModsGpuRegValue);
    void Write64(ModsGpuRegValue, UINT32 regIndex);
    void Write64(ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write64(UINT32 domainIndex, ModsGpuRegValue);
    void Write64(UINT32 domainIndex, ModsGpuRegValue, UINT32 regIndex);
    void Write64(UINT32 domainIndex, ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegValue);
    void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegValue, UINT32 regIndex);
    void Write64(UINT32 domainIndex, RegSpace, ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write64Sync(ModsGpuRegValue);
    void Write64Sync(ModsGpuRegValue, UINT32 regIndex);
    void Write64Sync(ModsGpuRegValue, ArrayIndexes regIndexes);

    void Write64Sync(UINT32 domainIndex, ModsGpuRegValue);
    void Write64Sync(UINT32 domainIndex, ModsGpuRegValue, UINT32 regIndex);
    void Write64Sync(UINT32 domainIndex, ModsGpuRegValue, ArrayIndexes regIndexes);

    // Check whether a register field has the specified value.
    // Return false if the register/field/value isn't supported on this GPU.
    bool Test64(ModsGpuRegField,                          UINT64 value) const;
    bool Test64(ModsGpuRegField, UINT32 regIndex,         UINT64 value) const;
    bool Test64(ModsGpuRegField, ArrayIndexes regIndexes, UINT64 value) const;

    bool Test64(UINT32 domainIndex, ModsGpuRegField,                          UINT64 value) const;
    bool Test64(UINT32 domainIndex, ModsGpuRegField, UINT32 regIndex,         UINT64 value) const;
    bool Test64(UINT32 domainIndex, ModsGpuRegField, ArrayIndexes regIndexes, UINT64 value) const;

    bool Test64(ModsGpuRegValue)                          const;
    bool Test64(ModsGpuRegValue, UINT32 regIndex)         const;
    bool Test64(ModsGpuRegValue, ArrayIndexes regIndexes) const;

    bool Test64(UINT32 domainIndex, ModsGpuRegValue)                          const;
    bool Test64(UINT32 domainIndex, ModsGpuRegValue, UINT32 regIndex)         const;
    bool Test64(UINT32 domainIndex, ModsGpuRegValue, ArrayIndexes regIndexes) const;

    virtual LwRm * GetLwRmPtr() const { return LwRmPtr().Get(); }

protected:
    TestDevice * GetTestDevice() const { MASSERT(m_pTestDevice); return m_pTestDevice; }
    UINT32 ReadCapability32(RegHalDomain domain, UINT32 addr) const override;

private:
    TestDevice* m_pTestDevice = nullptr;
    static UINT32 s_DebugPrivCheckMode;
    struct PrivRegSet;
    unique_ptr<PrivRegSet> m_CheckedReadAccess;
    unique_ptr<PrivRegSet> m_CheckedWriteAccess;
    unique_ptr<PrivRegSet> m_CheckedIsSupported;
    void VerifyReadAccessCheck(UINT32 domainIndex, ModsGpuRegAddress,
                               ArrayIndexes) const;
    void VerifyWriteAccessCheck(UINT32 domainIndex, ModsGpuRegAddress,
                                ArrayIndexes) const;
    void VerifyIsSupported(UINT32 domainIndex, ModsGpuRegAddress,
                           ArrayIndexes) const;
    void PrintErrorMsg(UINT32 domainIndex, ModsGpuRegAddress,
                       ArrayIndexes, const char *) const;
};

// ********************************************************
// * Inline methods with trivial one-line implementations *
// ********************************************************

//! \brief Read a register
inline UINT32 RegHal::Read32(ModsGpuRegAddress address) const
{
    return Read32(0, address, NO_INDEXES);
}

//! \brief Read a register
inline UINT32 RegHal::Read32(ModsGpuRegAddress address, UINT32 regIndex) const
{
    return Read32(0, address, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register
inline UINT32 RegHal::Read32(ModsGpuRegAddress address, ArrayIndexes regIndexes) const
{
    return Read32(0, address, regIndexes);
}

//! \brief Read a register
inline UINT32 RegHal::Read32(UINT32 domainIndex, ModsGpuRegAddress address) const
{
    return Read32(domainIndex, address, NO_INDEXES);
}

//! \brief Read a register
inline UINT32 RegHal::Read32(UINT32 domainIndex, ModsGpuRegAddress address, UINT32 regIndex) const
{
    return Read32(domainIndex, address, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register after checking its priv level
inline RC RegHal::Read32Priv(ModsGpuRegAddress address, UINT32* pValue) const
{
    return Read32Priv(address, 0, pValue);
}

//! \brief Read a register field
inline UINT32 RegHal::Read32(ModsGpuRegField field) const
{
    return Read32(0, field, NO_INDEXES);
}

//! \brief Read a register field
inline UINT32 RegHal::Read32(ModsGpuRegField field, UINT32 regIndex) const
{
    return Read32(0, field, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register field
inline UINT32 RegHal::Read32
(
    ModsGpuRegField field,
    ArrayIndexes    regArrayIndexes
) const
{
    return GetField(Read32(0, ColwertToAddress(field), regArrayIndexes), field);
}

//! \brief Read a register field
inline UINT32 RegHal::Read32(UINT32 domainIndex, ModsGpuRegField field) const
{
    return Read32(domainIndex, field, NO_INDEXES);
}

//! \brief Read a register field
inline UINT32 RegHal::Read32(UINT32 domainIndex, ModsGpuRegField field, UINT32 regIndex) const
{
    return Read32(domainIndex, field, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register field
inline UINT32 RegHal::Read32
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    ArrayIndexes regArrayIndexes
) const
{
    return GetField(Read32(domainIndex, ColwertToAddress(field), regArrayIndexes), field);
}

//! \brief Write a register
inline void RegHal::Write32(ModsGpuRegAddress address, UINT32 value)
{
    Write32(0, UNICAST, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write32
(
    ModsGpuRegAddress address,
    UINT32            regIndex,
    UINT32            value
)
{
    Write32(0, UNICAST, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write32
(
    ModsGpuRegAddress address,
    ArrayIndexes      regArrayIndexes,
    UINT32            value
)
{
    Write32(0, UNICAST, address, regArrayIndexes, value);
}

//! \brief Write a register
inline void RegHal::Write32(UINT32 domainIndex, ModsGpuRegAddress address, UINT32 value)
{
    Write32(domainIndex, UNICAST, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write32
(
    UINT32 domainIndex,
    ModsGpuRegAddress address,
    UINT32 regIndex,
    UINT32 value
)
{
    Write32(domainIndex, UNICAST, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write32
(
    UINT32 domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes regArrayIndexes,
    UINT32 value
)
{
    Write32(domainIndex, UNICAST, address, regArrayIndexes, value);
}

//! \brief Write a register
inline void RegHal::Write32
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT32 value
)
{
    Write32(domainIndex, space, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write32
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT32 regIndex,
    UINT32 value
)
{
    Write32(domainIndex, space, address, ArrayIndexes_t{ regIndex }, value);
}

inline RC RegHal::Write32Priv(ModsGpuRegValue value)
{
    return Write32Priv(0, UNICAST, value, NO_INDEXES);
}

inline RC RegHal::Write32Priv(ModsGpuRegValue value, UINT32 regIndex)
{
    return Write32Priv(0, UNICAST, value, ArrayIndexes_t{ regIndex });
}

inline RC RegHal::Write32Priv
(
    ModsGpuRegAddress  address,
    UINT32             regIndex,
    UINT32             value
)
{   //see reghal.cpp
    return Write32Priv(0, UNICAST, address, ArrayIndexes_t{ regIndex }, value);
}

inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegValue value)
{
    return Write32Priv(domainIndex, UNICAST, value, NO_INDEXES);
}

inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegValue value, UINT32 regIndex)
{
    return Write32Priv(domainIndex, UNICAST, value, ArrayIndexes_t{ regIndex });
}

inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegValue value, ArrayIndexes regArrayIndexes)
{
    return Write32Priv(domainIndex, UNICAST, value, regArrayIndexes);
}

inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegAddress address, UINT32 value)
{
    return Write32Priv(domainIndex, UNICAST, address, NO_INDEXES, value);
}

inline RC RegHal::Write32Priv
(
    UINT32 domainIndex, 
    ModsGpuRegAddress  address,
    UINT32             regIndex,
    UINT32             value
)
{   //see reghal.cpp
    return Write32Priv(domainIndex, UNICAST, address, ArrayIndexes_t{ regIndex }, value);
}

inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegAddress address, ArrayIndexes regArrayIndexes, UINT32 value)
{
    return Write32Priv(domainIndex, UNICAST, address, regArrayIndexes, value);
}

//! \brief Write a register field after check the priv level
inline RC RegHal::Write32Priv
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegValue value,
    ArrayIndexes regArrayIndexes
)
{   //see reghal.cpp
    return Write32Priv(domainIndex, space, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

inline RC RegHal::Write32Priv(ModsGpuRegValue value, ArrayIndexes regIndexes)
{   // see reghal.cpp
    return Write32Priv(0, UNICAST, ColwertToField(value), regIndexes, LookupValue(value));
}

//! \brief Write a register after checking its priv level
inline RC RegHal::Write32Priv(ModsGpuRegAddress address, UINT32 value)
{   //see reghal.cpp
    return Write32Priv(address, 0, value);
}

//! \brief Write a register field
inline RC RegHal::Write32Priv(ModsGpuRegField field, UINT32 value)
{   //see reghal.cpp
    return Write32Priv(0, UNICAST, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline RC RegHal::Write32Priv(ModsGpuRegField field, UINT32 regIndex, UINT32 value)
{   //see reghal.cpp
    return Write32Priv(0, UNICAST, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline RC RegHal::Write32Priv(ModsGpuRegField field, ArrayIndexes regArrayIndexes, UINT32 value)
{   //see reghal.cpp
    return Write32Priv(0, UNICAST, field, regArrayIndexes, value);
}

inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegField field, UINT32 value)
{   //see reghal.cpp
    return Write32Priv(domainIndex, UNICAST, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegField field, UINT32 regIndex, UINT32 value)
{   //see reghal.cpp
    return Write32Priv(domainIndex, UNICAST, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline RC RegHal::Write32Priv(UINT32 domainIndex, ModsGpuRegField field, ArrayIndexes regArrayIndexes, UINT32 value)
{   //see reghal.cpp
    return Write32Priv(domainIndex, UNICAST, field, regArrayIndexes, value);
}


//! \brief Write a register
inline void RegHal::Write32Broadcast
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT32 value
)
{
    Write32Broadcast(domainIndex, space, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write32Broadcast
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT32 regIndex,
    UINT32 value
)
{
    Write32Broadcast(domainIndex, space, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write32Sync(ModsGpuRegAddress address, UINT32 value)
{
    Write32Sync(0, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write32Sync
(
    ModsGpuRegAddress address,
    UINT32            regIndex,
    UINT32            value
)
{
    Write32Sync(0, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write32Sync
(
    ModsGpuRegAddress address,
    ArrayIndexes      regArrayIndexes,
    UINT32            value
)
{
    Write32Sync(0, address, regArrayIndexes, value);
}

//! \brief Write a register
inline void RegHal::Write32Sync(UINT32 domainIndex, ModsGpuRegAddress address, UINT32 value)
{
    Write32Sync(domainIndex, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write32Sync
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    UINT32            regIndex,
    UINT32            value
)
{
    Write32Sync(domainIndex, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register after checking its priv level
inline RC RegHal::Write32PrivSync(ModsGpuRegAddress address, UINT32 value)
{
    return Write32PrivSync(address, 0, value);
}

//! \brief Write a register field
inline void RegHal::Write32(ModsGpuRegField field, UINT32 value)
{
    Write32(0, UNICAST, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write32(ModsGpuRegField field, UINT32 regIndex, UINT32 value)
{
    Write32(0, UNICAST, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write32(ModsGpuRegField field, ArrayIndexes regArrayIndexes, UINT32 value)
{
    Write32(0, UNICAST, field, regArrayIndexes, value);
}

//! \brief Write a register field
inline void RegHal::Write32(UINT32 domainIndex, ModsGpuRegField field, UINT32 value)
{
    Write32(domainIndex, UNICAST, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write32
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT32 value
)
{
    Write32(domainIndex, UNICAST, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write32
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    ArrayIndexes regArrayIndexes,
    UINT32 value
)
{
    Write32(domainIndex, UNICAST, field, regArrayIndexes, value);
}

//! \brief Write a register field
inline void RegHal::Write32
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegField field,
    UINT32 value
)
{
    Write32(domainIndex, space, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write32
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT32 value
)
{
    Write32(domainIndex, space, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write32Sync(ModsGpuRegField field, UINT32 value)
{
    Write32Sync(0, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write32Sync
(
    ModsGpuRegField field,
    UINT32          regIndex,
    UINT32          value
)
{
    Write32Sync(0, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write32Sync
(
    ModsGpuRegField field,
    ArrayIndexes    regArrayIndexes,
    UINT32          value
)
{
    Write32Sync(0, field, regArrayIndexes, value);
}

//! \brief Write a register field
inline void RegHal::Write32Sync(UINT32 domainIndex, ModsGpuRegField field, UINT32 value)
{
    Write32Sync(domainIndex, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write32Sync
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32          regIndex,
    UINT32          value
)
{
    Write32Sync(domainIndex, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write32(ModsGpuRegValue value)
{
    Write32(0, UNICAST, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write32(ModsGpuRegValue value, UINT32 regIndex)
{
    Write32(0, UNICAST, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write32(ModsGpuRegValue value, ArrayIndexes regArrayIndexes)
{
    Write32(0, UNICAST, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write32(UINT32 domainIndex, ModsGpuRegValue value)
{
    Write32(domainIndex, UNICAST, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write32(UINT32 domainIndex, ModsGpuRegValue value, UINT32 regIndex)
{
    Write32(domainIndex, UNICAST, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write32
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    ArrayIndexes regArrayIndexes
)
{
    Write32(domainIndex, UNICAST, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write32(UINT32 domainIndex, RegSpace space, ModsGpuRegValue value)
{
    Write32(domainIndex, space, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write32(UINT32 domainIndex, RegSpace space, ModsGpuRegValue value, UINT32 regIndex)
{
    Write32(domainIndex, space, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write32
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegValue value,
    ArrayIndexes regArrayIndexes
)
{
    Write32(domainIndex, space, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write32Sync(ModsGpuRegValue value)
{
    Write32Sync(0, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write32Sync(ModsGpuRegValue value, UINT32 regIndex)
{
    Write32Sync(0, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write32Sync
(
    ModsGpuRegValue value,
    ArrayIndexes    regArrayIndexes
)
{
    Write32Sync(0, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write32Sync(UINT32 domainIndex, ModsGpuRegValue value)
{
    Write32Sync(domainIndex, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write32Sync(UINT32 domainIndex, ModsGpuRegValue value, UINT32 regIndex)
{
    Write32Sync(domainIndex, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write32Sync
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    ArrayIndexes regArrayIndexes
)
{
    Write32Sync(domainIndex, ColwertToField(value), regArrayIndexes, LookupValue(value));
}


//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32(ModsGpuRegField field, UINT32 value) const
{
    return Test32(0, field, NO_INDEXES, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32
(
    ModsGpuRegField field,
    UINT32          regIndex,
    UINT32          value
) const
{
    return Test32(0, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32
(
    ModsGpuRegField field,
    ArrayIndexes    regArrayIndexes,
    UINT32          value
) const
{
    return Test32(0, field, regArrayIndexes, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32(UINT32 domainIndex, ModsGpuRegField field, UINT32 value) const
{
    return Test32(domainIndex, field, NO_INDEXES, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT32 value
) const
{
    return Test32(domainIndex, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32(ModsGpuRegValue value) const
{
    return Test32(0, value, NO_INDEXES);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32(ModsGpuRegValue value, UINT32 regIndex) const
{
    return Test32(0, value, ArrayIndexes_t{ regIndex });
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32(ModsGpuRegValue value, ArrayIndexes regArrayIndexes) const
{
    return Test32(0, value, regArrayIndexes);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32(UINT32 domainIndex, ModsGpuRegValue value) const
{
    return Test32(domainIndex, value, NO_INDEXES);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test32(UINT32 domainIndex, ModsGpuRegValue value, UINT32 regIndex) const
{
    return Test32(domainIndex, value, ArrayIndexes_t{ regIndex });
}

//*************************
// 64 bit implementations *
//*************************
//! \brief Read a register
inline UINT64 RegHal::Read64(ModsGpuRegAddress address) const
{
    return Read64(0, address, NO_INDEXES);
}

//! \brief Read a register
inline UINT64 RegHal::Read64(ModsGpuRegAddress address, UINT32 regIndex) const
{
    return Read64(0, address, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register
inline UINT64 RegHal::Read64(ModsGpuRegAddress address, ArrayIndexes regIndexes) const
{
    return Read64(0, address, regIndexes);
}

//! \brief Read a register
inline UINT64 RegHal::Read64(UINT32 domainIndex, ModsGpuRegAddress address) const
{
    return Read64(domainIndex, address, NO_INDEXES);
}

//! \brief Read a register
inline UINT64 RegHal::Read64(UINT32 domainIndex, ModsGpuRegAddress address, UINT32 regIndex) const
{
    return Read64(domainIndex, address, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register field
inline UINT64 RegHal::Read64(ModsGpuRegField field) const
{
    return Read64(0, field, NO_INDEXES);
}

//! \brief Read a register field
inline UINT64 RegHal::Read64(ModsGpuRegField field, UINT32 regIndex) const
{
    return Read64(0, field, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register field
inline UINT64 RegHal::Read64
(
    ModsGpuRegField field,
    ArrayIndexes    regArrayIndexes
) const
{
    return GetField64(Read64(0, ColwertToAddress(field), regArrayIndexes), field);
}

//! \brief Read a register field
inline UINT64 RegHal::Read64(UINT32 domainIndex, ModsGpuRegField field) const
{
    return Read64(domainIndex, field, NO_INDEXES);
}

//! \brief Read a register field
inline UINT64 RegHal::Read64(UINT32 domainIndex, ModsGpuRegField field, UINT32 regIndex) const
{
    return Read64(domainIndex, field, ArrayIndexes_t{ regIndex });
}

//! \brief Read a register field
inline UINT64 RegHal::Read64
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    ArrayIndexes regArrayIndexes
) const
{
    return GetField64(Read64(domainIndex, ColwertToAddress(field), regArrayIndexes), field);
}

//! \brief Write a register
inline void RegHal::Write64(ModsGpuRegAddress address, UINT64 value)
{
    Write64(0, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write64
(
    ModsGpuRegAddress address,
    UINT32            regIndex,
    UINT64            value
)
{
    Write64(0, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write64
(
    ModsGpuRegAddress address,
    ArrayIndexes      regArrayIndexes,
    UINT64            value
)
{
    Write64(0, address, regArrayIndexes, value);
}

//! \brief Write a register
inline void RegHal::Write64(UINT32 domainIndex, ModsGpuRegAddress address, UINT64 value)
{
    Write64(domainIndex, UNICAST, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write64
(
    UINT32 domainIndex,
    ModsGpuRegAddress address,
    UINT32 regIndex,
    UINT64 value
)
{
    Write64(domainIndex, UNICAST, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write64
(
    UINT32 domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes regArrayIndexes,
    UINT64 value
)
{
    Write64(domainIndex, UNICAST, address, regArrayIndexes, value);
}

//! \brief Write a register
inline void RegHal::Write64
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT64 value
)
{
    Write64(domainIndex, space, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write64
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT32 regIndex,
    UINT64 value
)
{
    Write64(domainIndex, space, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write64Broadcast
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT64 value
)
{
    Write64Broadcast(domainIndex, space, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write64Broadcast
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegAddress address,
    UINT32 regIndex,
    UINT64 value
)
{
    Write64Broadcast(domainIndex, space, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write64Sync(ModsGpuRegAddress address, UINT64 value)
{
    Write64Sync(0, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write64Sync
(
    ModsGpuRegAddress address,
    UINT32            regIndex,
    UINT64            value
)
{
    Write64Sync(0, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register
inline void RegHal::Write64Sync
(
    ModsGpuRegAddress address,
    ArrayIndexes      regArrayIndexes,
    UINT64            value
)
{
    Write64Sync(0, address, regArrayIndexes, value);
}

//! \brief Write a register
inline void RegHal::Write64Sync(UINT32 domainIndex, ModsGpuRegAddress address, UINT64 value)
{
    Write64Sync(domainIndex, address, NO_INDEXES, value);
}

//! \brief Write a register
inline void RegHal::Write64Sync
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    UINT32            regIndex,
    UINT64            value
)
{
    Write64Sync(domainIndex, address, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write64(ModsGpuRegField field, UINT64 value)
{
    Write64(0, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write64(ModsGpuRegField field, UINT32 regIndex, UINT64 value)
{
    Write64(0, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write64(ModsGpuRegField field, ArrayIndexes regArrayIndexes, UINT64 value)
{
    Write64(0, field, regArrayIndexes, value);
}

//! \brief Write a register field
inline void RegHal::Write64(UINT32 domainIndex, ModsGpuRegField field, UINT64 value)
{
    Write64(domainIndex, UNICAST, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write64
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT64 value
)
{
    Write64(domainIndex, UNICAST, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write64
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    ArrayIndexes regArrayIndexes,
    UINT64 value
)
{
    Write64(domainIndex, UNICAST, field, regArrayIndexes, value);
}

//! \brief Write a register field
inline void RegHal::Write64
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegField field,
    UINT64 value
)
{
    Write64(domainIndex, space, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write64
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT64 value
)
{
    Write64(domainIndex, space, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write64Sync(ModsGpuRegField field, UINT64 value)
{
    Write64Sync(0, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write64Sync
(
    ModsGpuRegField field,
    UINT32          regIndex,
    UINT64          value
)
{
    Write64Sync(0, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write64Sync
(
    ModsGpuRegField field,
    ArrayIndexes    regArrayIndexes,
    UINT64          value
)
{
    Write64Sync(0, field, regArrayIndexes, value);
}

//! \brief Write a register field
inline void RegHal::Write64Sync(UINT32 domainIndex, ModsGpuRegField field, UINT64 value)
{
    Write64Sync(domainIndex, field, NO_INDEXES, value);
}

//! \brief Write a register field
inline void RegHal::Write64Sync
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32          regIndex,
    UINT64          value
)
{
    Write64Sync(domainIndex, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Write a register field
inline void RegHal::Write64(ModsGpuRegValue value)
{
    Write64(0, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write64(ModsGpuRegValue value, UINT32 regIndex)
{
    Write64(0, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write64(ModsGpuRegValue value, ArrayIndexes regArrayIndexes)
{
    Write64(0, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write64(UINT32 domainIndex, ModsGpuRegValue value)
{
    Write64(domainIndex, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write64(UINT32 domainIndex, ModsGpuRegValue value, UINT32 regIndex)
{
    Write64(domainIndex, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write64
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    ArrayIndexes regArrayIndexes
)
{
    Write64(domainIndex, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write64(UINT32 domainIndex, RegSpace space, ModsGpuRegValue value)
{
    Write64(domainIndex, space, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write64(UINT32 domainIndex, RegSpace space, ModsGpuRegValue value, UINT32 regIndex)
{
    Write64(domainIndex, space, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write64
(
    UINT32 domainIndex,
    RegSpace space,
    ModsGpuRegValue value,
    ArrayIndexes regArrayIndexes
)
{
    Write64(domainIndex, space, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write64Sync(ModsGpuRegValue value)
{
    Write64Sync(0, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write64Sync(ModsGpuRegValue value, UINT32 regIndex)
{
    Write64Sync(0, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write64Sync
(
    ModsGpuRegValue value,
    ArrayIndexes    regArrayIndexes
)
{
    Write64Sync(0, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Write a register field
inline void RegHal::Write64Sync(UINT32 domainIndex, ModsGpuRegValue value)
{
    Write64Sync(domainIndex, value, NO_INDEXES);
}

//! \brief Write a register field
inline void RegHal::Write64Sync(UINT32 domainIndex, ModsGpuRegValue value, UINT32 regIndex)
{
    Write64Sync(domainIndex, value, ArrayIndexes_t{ regIndex });
}

//! \brief Write a register field
inline void RegHal::Write64Sync
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    ArrayIndexes regArrayIndexes
)
{
    Write64Sync(domainIndex, ColwertToField(value), regArrayIndexes, LookupValue(value));
}

//! \brief Return true if a priv-protected register is readable, or if
//! the register is not priv-protected
inline bool RegHal::HasReadAccess(ModsGpuRegAddress address) const
{
    return HasReadAccess(0, address, NO_INDEXES);
}

//! \brief Return true if a priv-protected register is readable, or if
//! the register is not priv-protected
inline bool RegHal::HasReadAccess
(
    ModsGpuRegAddress address,
    UINT32            regIndex
) const
{
    return HasReadAccess(0, address, ArrayIndexes_t{ regIndex });
}

//! \brief Return true if a priv-protected register is readable, or if
//! the register is not priv-protected
inline bool RegHal::HasReadAccess
(
    ModsGpuRegAddress address,
    ArrayIndexes      regIndexes
) const
{
    return HasReadAccess(0, address, regIndexes);
}

//! \brief Return true if a priv-protected register is readable, or if
//! the register is not priv-protected
inline bool RegHal::HasReadAccess
(
    UINT32            domainIndex,
    ModsGpuRegAddress address
) const
{
    return HasReadAccess(domainIndex, address, NO_INDEXES);
}

//! \brief Return true if a priv-protected register is readable, or if
//! the register is not priv-protected
inline bool RegHal::HasReadAccess
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    UINT32            regIndex
) const
{
    return HasReadAccess(domainIndex, address, ArrayIndexes_t{ regIndex });
}


//! \brief Return true if a priv-protected register has read/write
//! access, or if the register is not priv-protected
inline bool RegHal::HasRWAccess(ModsGpuRegAddress address) const
{
    return HasRWAccess(0, address, NO_INDEXES);
}

//! \brief Return true if a priv-protected register has read/write
//! access, or if the register is not priv-protected
inline bool RegHal::HasRWAccess
(
    ModsGpuRegAddress address,
    UINT32            regIndex
) const
{
    return HasRWAccess(0, address, ArrayIndexes_t{ regIndex });
}

//! \brief Return true if a priv-protected register has read/write
//! access, or if the register is not priv-protected
inline bool RegHal::HasRWAccess
(
    ModsGpuRegAddress address,
    ArrayIndexes      regIndexes
) const
{
    return HasRWAccess(0, address, regIndexes);
}

//! \brief Return true if a priv-protected register has read/write
//! access, or if the register is not priv-protected
inline bool RegHal::HasRWAccess
(
    UINT32            domainIndex,
    ModsGpuRegAddress address
) const
{
    return HasRWAccess(domainIndex, address, NO_INDEXES);
}

//! \brief Return true if a priv-protected register has read/write
//! access, or if the register is not priv-protected
inline bool RegHal::HasRWAccess
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    UINT32            regIndex
) const
{
    return HasRWAccess(domainIndex, address, ArrayIndexes_t{ regIndex });
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64(ModsGpuRegField field, UINT64 value) const
{
    return Test64(0, field, NO_INDEXES, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64
(
    ModsGpuRegField field,
    UINT32          regIndex,
    UINT64          value
) const
{
    return Test64(0, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64
(
    ModsGpuRegField field,
    ArrayIndexes    regArrayIndexes,
    UINT64          value
) const
{
    return Test64(0, field, regArrayIndexes, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64(UINT32 domainIndex, ModsGpuRegField field, UINT64 value) const
{
    return Test64(domainIndex, field, NO_INDEXES, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    UINT32 regIndex,
    UINT64 value
) const
{
    return Test64(domainIndex, field, ArrayIndexes_t{ regIndex }, value);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64(ModsGpuRegValue value) const
{
    return Test64(0, value, NO_INDEXES);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64(ModsGpuRegValue value, UINT32 regIndex) const
{
    return Test64(0, value, ArrayIndexes_t{ regIndex });
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64(ModsGpuRegValue value, ArrayIndexes regArrayIndexes) const
{
    return Test64(0, value, regArrayIndexes);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64(UINT32 domainIndex, ModsGpuRegValue value) const
{
    return Test64(domainIndex, value, NO_INDEXES);
}

//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
inline bool RegHal::Test64(UINT32 domainIndex, ModsGpuRegValue value, UINT32 regIndex) const
{
    return Test64(domainIndex, value, ArrayIndexes_t{ regIndex });
}

#endif // INCLUDED_REGHAL_H
