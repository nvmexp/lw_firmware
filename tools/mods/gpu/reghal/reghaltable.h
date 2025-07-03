/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_REGHALTABLE_H
#define INCLUDED_REGHALTABLE_H

#include "mods_reg_hal.h"
#ifdef MATS_STANDALONE
#include "fakemods.h"
#else
#include "core/include/device.h"
#endif
#include "../../core/include/types.h"
#include <array>
#include <unordered_map>
#include <map>
#include <memory>
#include <string>
#include <vector>

//--------------------------------------------------------------------
//! Domain of a register.  Delcare this outside of RegHal so that the enum
//! can be forward declared and cirlwlar dependencies avoided during compilation
//! of MATS
enum class RegHalDomain
{
     RAW              = 0x0
    ,AFS              = 0x1
    ,AFS_PERFMON      = 0x2
    ,CLKS             = 0x3
    ,DLPL             = 0x4
    ,FUSE             = 0x5
    ,GLUE             = 0x6
    ,IOCTRL           = 0x7
    ,JTAG             = 0x8
    ,MINION           = 0x9
    ,NPG              = 0xa
    ,NPORT            = 0xb
    ,NTL              = 0xc
    ,LWLCPR           = 0xd
    ,LWLDL            = 0xe
    ,LWLIPT           = 0xf
    ,LWLIPT_LNK       = 0x10
    ,LWLPLL           = 0x11
    ,LWLTLC           = 0x12
    ,LWLW             = 0x13
    ,NXBAR            = 0x14
    ,PHY              = 0x15
    ,PMGR             = 0x16
    ,SAW              = 0x17
    ,SIOCTRL          = 0x18
    ,SWX              = 0x19
    ,SWX_PERFMON      = 0x1a
    ,XP3G             = 0x1b
    ,XVE              = 0x1c
    ,RUNLIST          = 0x1d
    ,CHRAM            = 0x1e
    ,XTL              = 0x1f
    ,XPL              = 0x20
    ,PCIE             = 0x21
    ,CLKS_P0          = 0x22

    ,MULTICAST_BIT    = 0x100
    ,NPORT_MULTI      = (NPORT | MULTICAST_BIT)
    ,LWLDL_MULTI      = (LWLDL | MULTICAST_BIT)
    ,LWLIPT_LNK_MULTI = (LWLIPT_LNK | MULTICAST_BIT)
    ,LWLTLC_MULTI     = (LWLTLC | MULTICAST_BIT)
};

//--------------------------------------------------------------------
//! \brief HAL (Hardware Abstraction Layer) for accessing registers
//!
//! This class uses a lookup table to read and write registers, so
//! that the same code can be used to control different GPUs even if
//! the registers were rearranged.
//!
//! There are three enums used to lookup entries in the HAL table:
//! - ModsGpuRegAddress: The offset of a register and its domain
//! - ModsGpuRegField:   A bit field within a register
//! - ModsGpuRegValue:   A value that can be stored within a bit field
//!
//! The enum values have the same names as the macros in
//! <branch>/drivers/common/inc/hwref, except that they start with
//! "MODS_" instead of "LW_" (e.g. MODS_PMC_BOOT_0 instead of
//! LW_PMC_BOOT_0).  See reghal.def for more details.
//!
//! Unless otherwise specified, the methods MASSERT if the requested
//! register isn't supported.  You can use IsSupported()
//! to check whether a register is supported.
//!
class RegHalTable
{
public:
    RegHalTable() : m_pCapabilityTable(std::make_unique<CapabilityTable>()) {}
    virtual ~RegHalTable() { }

    void Initialize(Device::LwDeviceId deviceId);

    enum Flags
    {
        FLAG_PHYSICAL_ONLY = 0x0,  // Only exists on physical GPU (default)
        FLAG_VIRTUAL_ONLY  = 0x1,  // Register only exists on VGPU
        FLAG_VIRTUAL_MASK  = 0x1,
    };

    enum RegSpace  // Must match genreghalimpl.py REGSPACE_ constants
    {
        UNICAST   = 0,
        MULTICAST = 1
    };

    static const size_t MAX_INDEXES = 4;
    struct Element
    {
        UINT32                          regIndex;
        std::array<UINT16, MAX_INDEXES> arrayIndexes;
        RegHalDomain                    domain;
        UINT32                          capabilityId;
        UINT32                          flags;
        UINT32                          regValue;
        // regValue needs to be last because some registers
        // don't actually define a register value, eg. LW_MMU_PTE
    };
    struct Table
    {
        const Element *pElements;
        size_t         size;
    };

    using ArrayIndexes_t = std::vector<UINT32>;
    using ArrayIndexes   = const ArrayIndexes_t &;  // Used in args
    static const ArrayIndexes_t NO_INDEXES;         // Same as "{}"

    // ***************************************************
    // * Methods for looking up entries in the HAL table *
    // ***************************************************

    // Look up register address.
    UINT32 LookupAddress(ModsGpuRegAddress)               const;
    UINT32 LookupAddress(ModsGpuRegAddress, UINT32 index) const;
    UINT32 LookupAddress(ModsGpuRegAddress, ArrayIndexes) const;

    // Look up domain of address.
    RegHalDomain LookupDomain(ModsGpuRegAddress)               const;
    RegHalDomain LookupDomain(ModsGpuRegAddress, UINT32 index) const;
    RegHalDomain LookupDomain(ModsGpuRegAddress, ArrayIndexes) const;

    // Look up size of register array (dimension must be 1 or 2).
    UINT32 LookupArraySize(ModsGpuRegAddress, UINT32 dimension) const;
    UINT32 LookupArraySize(ModsGpuRegField,   UINT32 dimension) const;
    UINT32 LookupArraySize(ModsGpuRegValue,   UINT32 dimension) const;

    // Look up the address of the corresponding priv register,
    // or 0 if the register has no corresponding priv register
    UINT32 LookupPrivAddress(ModsGpuRegAddress)               const;
    UINT32 LookupPrivAddress(ModsGpuRegAddress, UINT32 index) const;
    UINT32 LookupPrivAddress(ModsGpuRegAddress, ArrayIndexes) const;

    // Look up the high & low bits of a bitfield.
    void LookupField(ModsGpuRegField, UINT32* pHiBit, UINT32* pLoBit) const;
    void LookupField(ModsGpuRegField, UINT32 index, UINT32* pHiBit, UINT32* pLoBit) const;
    void LookupField(ModsGpuRegField, ArrayIndexes, UINT32* pHiBit, UINT32* pLoBit) const;

    // Look up the shifted mask corresponding to a register field
    // (i.e., equivalent to the old DRF_SHIFTMASK macro)
    UINT32 LookupMask(ModsGpuRegField)               const;
    UINT32 LookupMask(ModsGpuRegField, UINT32 index) const;
    UINT32 LookupMask(ModsGpuRegField, ArrayIndexes) const;

    // Look up the mask corresponding to a register field
    // (i.e., equivalent to the old DRF_MASK macro)
    UINT32 LookupFieldValueMask(ModsGpuRegField)               const;
    UINT32 LookupFieldValueMask(ModsGpuRegField, UINT32 index) const;
    UINT32 LookupFieldValueMask(ModsGpuRegField, ArrayIndexes) const;

    // Look up the size of a mask corresponding to a register field
    UINT32 LookupMaskSize(ModsGpuRegField)               const;
    UINT32 LookupMaskSize(ModsGpuRegField, UINT32 index) const;
    UINT32 LookupMaskSize(ModsGpuRegField, ArrayIndexes) const;

    // Look up register field value.
    UINT32 LookupValue(ModsGpuRegValue)               const;
    UINT32 LookupValue(ModsGpuRegValue, UINT32 index) const;
    UINT32 LookupValue(ModsGpuRegValue, ArrayIndexes) const;

    // Check whether a register is supported on this GPU
    bool IsSupported(ModsGpuRegAddress)               const;
    bool IsSupported(ModsGpuRegAddress, UINT32 index) const;
    bool IsSupported(ModsGpuRegAddress, ArrayIndexes) const;
    bool IsSupported(ModsGpuRegField)                 const;
    bool IsSupported(ModsGpuRegField, UINT32 index)   const;
    bool IsSupported(ModsGpuRegField, ArrayIndexes)   const;
    bool IsSupported(ModsGpuRegValue)                 const;
    bool IsSupported(ModsGpuRegValue, UINT32 index)   const;
    bool IsSupported(ModsGpuRegValue, ArrayIndexes)   const;

    // ***********************************************************************
    // * Methods for reading and writing fields in a pre-read register value *
    // ***********************************************************************

    // Get a field from a pre-read register value.
    UINT32 GetField(UINT32 regValue, ModsGpuRegField)               const;
    UINT32 GetField(UINT32 regValue, ModsGpuRegField, UINT32 index) const;
    UINT32 GetField(UINT32 regValue, ModsGpuRegField, ArrayIndexes) const;

    // Set a field in a pre-read register value
    void SetField(UINT32 *pRegValue, ModsGpuRegField,               UINT32 fieldVal) const;
    void SetField(UINT32 *pRegValue, ModsGpuRegField, UINT32 index, UINT32 fieldVal) const;
    void SetField(UINT32 *pRegValue, ModsGpuRegField, ArrayIndexes, UINT32 fieldVal) const;
    void SetField(UINT32 *pRegValue, ModsGpuRegValue)                                const;
    void SetField(UINT32 *pRegValue, ModsGpuRegValue, UINT32 index)                  const;
    void SetField(UINT32 *pRegValue, ModsGpuRegValue, ArrayIndexes)                  const;

    // Return a register value with one field set.  Replaces DRF_DEF & DRF_NUM.
    UINT32 SetField(ModsGpuRegField,               UINT32 fieldVal) const;
    UINT32 SetField(ModsGpuRegField, UINT32 index, UINT32 fieldVal) const;
    UINT32 SetField(ModsGpuRegField, ArrayIndexes, UINT32 fieldVal) const;
    UINT32 SetField(ModsGpuRegValue)                                const;
    UINT32 SetField(ModsGpuRegValue, UINT32 index)                  const;
    UINT32 SetField(ModsGpuRegValue, ArrayIndexes)                  const;

    // Check whether a field has the specified value in a pre-read register.
    // Return false if the field or value isn't supported on this GPU.
    bool TestField(UINT32 regValue, ModsGpuRegField,               UINT32 fieldVal) const;
    bool TestField(UINT32 regValue, ModsGpuRegField, UINT32 index, UINT32 fieldVal) const;
    bool TestField(UINT32 regValue, ModsGpuRegField, ArrayIndexes, UINT32 fieldVal) const;
    bool TestField(UINT32 regValue, ModsGpuRegValue)                                const;
    bool TestField(UINT32 regValue, ModsGpuRegValue, UINT32 index)                  const;
    bool TestField(UINT32 regValue, ModsGpuRegValue, ArrayIndexes)                  const;

    // **************************
    // * 64 bit implementations *
    // **************************

    // Get a field from a pre-read register value.
    UINT64 GetField64(UINT64 regValue, ModsGpuRegField)               const;
    UINT64 GetField64(UINT64 regValue, ModsGpuRegField, UINT32 index) const;
    UINT64 GetField64(UINT64 regValue, ModsGpuRegField, ArrayIndexes) const;

    // Set a field in a pre-read register value
    void SetField64(UINT64 *pRegValue, ModsGpuRegField,               UINT64 fieldVal) const;
    void SetField64(UINT64 *pRegValue, ModsGpuRegField, UINT32 index, UINT64 fieldVal) const;
    void SetField64(UINT64 *pRegValue, ModsGpuRegField, ArrayIndexes, UINT64 fieldVal) const;
    void SetField64(UINT64 *pRegValue, ModsGpuRegValue)                                const;
    void SetField64(UINT64 *pRegValue, ModsGpuRegValue, UINT32 index)                  const;
    void SetField64(UINT64 *pRegValue, ModsGpuRegValue, ArrayIndexes)                  const;

    // Return a register value with one field set.  Replaces DRF_DEF & DRF_NUM.
    UINT64 SetField64(ModsGpuRegField,               UINT64 fieldVal) const;
    UINT64 SetField64(ModsGpuRegField, UINT32 index, UINT64 fieldVal) const;
    UINT64 SetField64(ModsGpuRegField, ArrayIndexes, UINT64 fieldVal) const;
    UINT64 SetField64(ModsGpuRegValue)                                const;
    UINT64 SetField64(ModsGpuRegValue, UINT32 index)                  const;
    UINT64 SetField64(ModsGpuRegValue, ArrayIndexes)                  const;

    // Check whether a field has the specified value in a pre-read register.
    // Return false if the field or value isn't supported on this GPU.
    bool TestField64(UINT64 regValue, ModsGpuRegField,               UINT64 fieldVal) const;
    bool TestField64(UINT64 regValue, ModsGpuRegField, UINT32 index, UINT64 fieldVal) const;
    bool TestField64(UINT64 regValue, ModsGpuRegField, ArrayIndexes, UINT64 fieldVal) const;
    bool TestField64(UINT64 regValue, ModsGpuRegValue)                                const;
    bool TestField64(UINT64 regValue, ModsGpuRegValue, UINT32 index)                  const;
    bool TestField64(UINT64 regValue, ModsGpuRegValue, ArrayIndexes)                  const;

    // *********************************************************
    // * Methods for colwerting between different RegHal enums *
    // *********************************************************

    // Colwert a register value to the field that contains it
    static ModsGpuRegField  ColwertToField(ModsGpuRegValue);

    // Colwert a register value or field to the address that contains it
    static ModsGpuRegAddress ColwertToAddress(ModsGpuRegValue);
    static ModsGpuRegAddress ColwertToAddress(ModsGpuRegField);

    // Colwert a register domain to multicast
    static RegHalDomain ColwertToMulticast(RegHalDomain);

    // Colwert a reghal enum to a string, and vice-versa
    static ModsGpuRegAddress ColwertToAddress(const std::string &str);
    static ModsGpuRegField   ColwertToField(const std::string &str);
    static ModsGpuRegValue   ColwertToValue(const std::string &str);
    static const char       *ColwertToString(ModsGpuRegAddress);
    static const char       *ColwertToString(ModsGpuRegField);
    static const char       *ColwertToString(ModsGpuRegValue);
    static std::string       ColwertToString(RegHalDomain);

    void LookupArrayIndexes
    (
        ModsGpuRegField,
        ArrayIndexes,
        ArrayIndexes_t* pAddressIndexes,
        ArrayIndexes_t* pFieldIndexes
    ) const;
    void LookupArrayIndexes
    (
        ModsGpuRegValue,
        ArrayIndexes,
        ArrayIndexes_t* pAddressIndexes,
        ArrayIndexes_t* pFieldIndexes,
        ArrayIndexes_t* pValueIndexes
    ) const;

protected:
    struct CapabilityData
    {
        UINT32 hwrefAddress;  // Nominal address of capability header from hwref
        UINT32 actualAddress; // Actual address found by walking linked list
    };
    const CapabilityData* LookupCapabilityId(ModsGpuRegValue) const;
    virtual UINT32 ReadCapability32(RegHalDomain domain, UINT32 addr) const;

private:

    // Define the key/value structs that form the HAL table
    struct HalKey
    {
        UINT32 regIndex;
        std::array<UINT16, MAX_INDEXES> arrayIndexes;

        bool operator<(const HalKey &rhs) const
        {
            return (regIndex != rhs.regIndex ?
                    regIndex < rhs.regIndex :
                    arrayIndexes < rhs.arrayIndexes);
        }
    };
    struct HalData
    {
        UINT32 regData;  // Address, field, or value
        RegHalDomain domain;
        ModsGpuRegValue capabilityId;
    };

    typedef std::map<HalKey, HalData> HalTable;
    static std::map<Device::LwDeviceId, std::unique_ptr<HalTable>> s_HalTables;
    HalTable * m_pHalTable = nullptr;
    bool m_Initialized = false;

    struct CapabilityHash
    {
        size_t operator()(ModsGpuRegValue val) const
            { return static_cast<size_t>(val); }
    };
    using CapabilityTable =
        std::unordered_map<ModsGpuRegValue, CapabilityData, CapabilityHash>;
    std::unique_ptr<CapabilityTable> m_pCapabilityTable;

    // Internal functions for looking up entries in the HAL table
    const HalData *LookupOrDie(ModsGpuRegAddress, ArrayIndexes) const;
    void    LookupOrDie(ModsGpuRegField, ArrayIndexes, UINT32 *pHiBit, UINT32 *pLoBit) const;
    UINT32  LookupOrDie(ModsGpuRegValue, ArrayIndexes) const;
    bool    TryLookup(ModsGpuRegAddress, ArrayIndexes, const HalData**) const;
    bool    TryLookup(ModsGpuRegField, ArrayIndexes, UINT32 *pHiBit, UINT32 *pLoBit) const;
    bool    TryLookup(ModsGpuRegValue, ArrayIndexes, UINT32 *pValue) const;
};

// ********************************************************
// * Inline methods with trivial one-line implementations *
// ********************************************************

//! \brief Look up register address in the HAL table
inline UINT32 RegHalTable::LookupAddress(ModsGpuRegAddress address) const
{
    return LookupAddress(address, NO_INDEXES);
}

//! \brief Look up register address in the HAL table
inline UINT32 RegHalTable::LookupAddress
(
    ModsGpuRegAddress address,
    UINT32            index
) const
{
    return LookupAddress(address, ArrayIndexes_t{index});
}

//! \brief Look up domain of address in the HAL table
inline RegHalDomain RegHalTable::LookupDomain
(
    ModsGpuRegAddress address
) const
{
    return LookupDomain(address, NO_INDEXES);
}

//! \brief Look up domain of address in the HAL table
inline RegHalDomain RegHalTable::LookupDomain
(
    ModsGpuRegAddress address,
    UINT32            index
) const
{
    return LookupDomain(address, ArrayIndexes_t{index});
}

//! \brief Look up domain of address in the HAL table
inline RegHalDomain RegHalTable::LookupDomain
(
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    return LookupOrDie(address, arrayIndexes)->domain;
}

inline UINT32 RegHalTable::LookupPrivAddress(ModsGpuRegAddress address) const
{
    return LookupPrivAddress(address, NO_INDEXES);
}

inline UINT32 RegHalTable::LookupPrivAddress
(
    ModsGpuRegAddress address,
    UINT32            index
) const
{
    return LookupPrivAddress(address, ArrayIndexes_t{index});
}

//! \brief Look up the high and low bits of a bitfield.
inline void RegHalTable::LookupField
(
    ModsGpuRegField field,
    UINT32* pHiBit,
    UINT32* pLoBit
) const
{
    return LookupField(field, NO_INDEXES, pHiBit, pLoBit);
}

//! \brief Look up the high and low bits of a bitfield.
inline void RegHalTable::LookupField
(
    ModsGpuRegField field,
    UINT32 index,
    UINT32* pHiBit,
    UINT32* pLoBit
) const
{
    return LookupField(field, ArrayIndexes_t{index}, pHiBit, pLoBit);
}

//! \brief Look up the shifted mask corresponding to a register field
inline UINT32 RegHalTable::LookupMask(ModsGpuRegField field) const
{
    return LookupMask(field, NO_INDEXES);
}

//! \brief Look up the shifted mask corresponding to a register field
inline UINT32 RegHalTable::LookupMask(ModsGpuRegField field, UINT32 index) const
{
    return LookupMask(field, ArrayIndexes_t{index});
}

//! \brief Look up the field value mask corresponding to a register field
inline UINT32 RegHalTable::LookupFieldValueMask(ModsGpuRegField field) const
{
    return LookupFieldValueMask(field, NO_INDEXES);
}

//! \brief Look up the field value mask corresponding to a register field
inline UINT32 RegHalTable::LookupFieldValueMask(ModsGpuRegField field, UINT32 index) const
{
    return LookupFieldValueMask(field, ArrayIndexes_t{index});
}

//! \brief Look up the size of a mask corresponding to a register field
inline UINT32 RegHalTable::LookupMaskSize(ModsGpuRegField field) const
{
    return LookupMaskSize(field, NO_INDEXES);
}

//! \brief Look up the size of a mask corresponding to a register field
inline UINT32 RegHalTable::LookupMaskSize(ModsGpuRegField field, UINT32 index) const
{
    return LookupMaskSize(field, ArrayIndexes_t{index});
}

//! \brief Look up register field value.
inline UINT32 RegHalTable::LookupValue(ModsGpuRegValue value) const
{
    return LookupOrDie(value, NO_INDEXES);
}

//! \brief Look up register field value.
inline UINT32 RegHalTable::LookupValue(ModsGpuRegValue value, UINT32 index) const
{
    return LookupOrDie(value, ArrayIndexes_t{index});
}

//! \brief Look up register field value.
inline UINT32 RegHalTable::LookupValue(ModsGpuRegValue value, ArrayIndexes indexes) const
{
    return LookupOrDie(value, indexes);
}

//! \brief Check whether a register is supported on this GPU
inline bool RegHalTable::IsSupported(ModsGpuRegAddress address) const
{
    return IsSupported(address, NO_INDEXES);
}

//! \brief Check whether a register is supported on this GPU
inline bool RegHalTable::IsSupported
(
    ModsGpuRegAddress address,
    UINT32            index
) const
{
    return IsSupported(address, ArrayIndexes_t{index});
}

//! \brief Check whether a register is supported on this GPU
inline bool RegHalTable::IsSupported
(
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    const HalData *pHalData;
    return TryLookup(address, arrayIndexes, &pHalData) &&
        (pHalData->capabilityId == MODS_REGISTER_VALUE_NULL ||
         LookupCapabilityId(pHalData->capabilityId) != nullptr);
}

//! \brief Check whether a register field is supported on this GPU
inline bool RegHalTable::IsSupported(ModsGpuRegField field) const
{
    UINT32 dummy[2];
    return TryLookup(field, NO_INDEXES, &dummy[0], &dummy[1]);
}

//! \brief Check whether a register field is supported on this GPU
inline bool RegHalTable::IsSupported(ModsGpuRegField field, UINT32 index) const
{
    UINT32 dummy[2];
    return TryLookup(field, ArrayIndexes_t{index}, &dummy[0], &dummy[1]);
}

//! \brief Check whether a register field is supported on this GPU
inline bool RegHalTable::IsSupported(ModsGpuRegField field, ArrayIndexes indexes) const
{
    UINT32 dummy[2];
    return TryLookup(field, indexes, &dummy[0], &dummy[1]);
}

//! \brief Check whether a register value is supported on this GPU
inline bool RegHalTable::IsSupported(ModsGpuRegValue value) const
{
    UINT32 dummy;
    return TryLookup(value, NO_INDEXES, &dummy);
}

//! \brief Check whether a register value is supported on this GPU
inline bool RegHalTable::IsSupported(ModsGpuRegValue value, UINT32 index) const
{
    UINT32 dummy;
    return TryLookup(value, ArrayIndexes_t{index}, &dummy);
}

//! \brief Check whether a register value is supported on this GPU
inline bool RegHalTable::IsSupported(ModsGpuRegValue value, ArrayIndexes indexes) const
{
    UINT32 dummy;
    return TryLookup(value, indexes, &dummy);
}

//! \brief Get a field from a pre-read register value.
inline UINT32 RegHalTable::GetField(UINT32 regValue, ModsGpuRegField field) const
{
    return GetField(regValue, field, NO_INDEXES);
}

//! \brief Get a field from a pre-read register value.
inline UINT32 RegHalTable::GetField
(
    UINT32 regValue,
    ModsGpuRegField field,
    UINT32 index
) const
{
    return GetField(regValue, field, ArrayIndexes_t{index});
}

//! \brief Set a field in a pre-read register value.
inline void RegHalTable::SetField
(
    UINT32 *pRegValue,
    ModsGpuRegField field,
    UINT32 fieldVal
) const
{
    return SetField(pRegValue, field, NO_INDEXES, fieldVal);
}

//! \brief Set a field in a pre-read register value.
inline void RegHalTable::SetField
(
    UINT32 *pRegValue,
    ModsGpuRegField field,
    UINT32 index,
    UINT32 fieldVal
) const
{
    return SetField(pRegValue, field, ArrayIndexes_t{index}, fieldVal);
}

//! \brief Set a field value in a pre-read register value.
inline void RegHalTable::SetField
(
    UINT32 *pRegValue,
    ModsGpuRegValue value
) const
{
    return SetField(pRegValue, value, NO_INDEXES);
}

//! \brief Set a field value in a pre-read register value.
inline void RegHalTable::SetField
(
    UINT32 *pRegValue,
    ModsGpuRegValue value,
    UINT32 index
) const
{
    return SetField(pRegValue, value, ArrayIndexes_t{index});
}

//! \brief Return a register value with one field set; replaces DRF_NUM
inline UINT32 RegHalTable::SetField
(
    ModsGpuRegField field,
    UINT32 fieldVal
) const
{
    return SetField(field, NO_INDEXES, fieldVal);
}

//! \brief Return a register value with one field set; replaces DRF_NUM
inline UINT32 RegHalTable::SetField
(
    ModsGpuRegField field,
    UINT32 index,
    UINT32 fieldVal
) const
{
    return SetField(field, ArrayIndexes_t{index}, fieldVal);
}

//! \brief Return a register value with one field value set; replaces DRF_DEF
inline UINT32 RegHalTable::SetField(ModsGpuRegValue value) const
{
    return SetField(value, NO_INDEXES);
}

//! \brief Return a register value with one field value set; replaces DRF_DEF
inline UINT32 RegHalTable::SetField
(
    ModsGpuRegValue value,
    UINT32 index
) const
{
    return SetField(value, ArrayIndexes_t{index});
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField
(
    UINT32 regValue,
    ModsGpuRegField field,
    UINT32 fieldVal
) const
{
    return TestField(regValue, field, NO_INDEXES, fieldVal);
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField
(
    UINT32 regValue,
    ModsGpuRegField field,
    UINT32 index,
    UINT32 fieldVal
) const
{
    return TestField(regValue, field, ArrayIndexes_t{index}, fieldVal);
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField
(
    UINT32 regValue,
    ModsGpuRegValue value
) const
{
    return TestField(regValue, value, NO_INDEXES);
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField
(
    UINT32 regValue,
    ModsGpuRegValue value,
    UINT32 index
) const
{
    return TestField(regValue, value, ArrayIndexes_t{index});
}

// **************************
// * 64 bit implementations *
// **************************

//! \brief Get a field from a pre-read register value.
inline UINT64 RegHalTable::GetField64(UINT64 regValue, ModsGpuRegField field) const
{
    return GetField64(regValue, field, NO_INDEXES);
}

//! \brief Get a field from a pre-read register value.
inline UINT64 RegHalTable::GetField64
(
    UINT64 regValue,
    ModsGpuRegField field,
    UINT32 index
) const
{
    return GetField64(regValue, field, ArrayIndexes_t{index});
}

//! \brief Set a field in a pre-read register value.
inline void RegHalTable::SetField64
(
    UINT64 *pRegValue,
    ModsGpuRegField field,
    UINT64 fieldVal
) const
{
    return SetField64(pRegValue, field, NO_INDEXES, fieldVal);
}

//! \brief Set a field in a pre-read register value.
inline void RegHalTable::SetField64
(
    UINT64 *pRegValue,
    ModsGpuRegField field,
    UINT32 index,
    UINT64 fieldVal
) const
{
    return SetField64(pRegValue, field, ArrayIndexes_t{index}, fieldVal);
}

//! \brief Set a field value in a pre-read register value.
inline void RegHalTable::SetField64
(
    UINT64 *pRegValue,
    ModsGpuRegValue value
) const
{
    return SetField64(pRegValue, value, NO_INDEXES);
}

//! \brief Set a field value in a pre-read register value.
inline void RegHalTable::SetField64
(
    UINT64 *pRegValue,
    ModsGpuRegValue value,
    UINT32 index
) const
{
    return SetField64(pRegValue, value, ArrayIndexes_t{index});
}

//! \brief Return a register value with one field set; replaces DRF_NUM
inline UINT64 RegHalTable::SetField64
(
    ModsGpuRegField field,
    UINT64 fieldVal
) const
{
    return SetField64(field, NO_INDEXES, fieldVal);
}

//! \brief Return a register value with one field set; replaces DRF_NUM
inline UINT64 RegHalTable::SetField64
(
    ModsGpuRegField field,
    UINT32 index,
    UINT64 fieldVal
) const
{
    return SetField64(field, ArrayIndexes_t{index}, fieldVal);
}

//! \brief Return a register value with one field value set; replaces DRF_DEF
inline UINT64 RegHalTable::SetField64(ModsGpuRegValue value) const
{
    return SetField64(value, NO_INDEXES);
}

//! \brief Return a register value with one field value set; replaces DRF_DEF
inline UINT64 RegHalTable::SetField64
(
    ModsGpuRegValue value,
    UINT32 index
) const
{
    return SetField64(value, ArrayIndexes_t{index});
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField64
(
    UINT64 regValue,
    ModsGpuRegField field,
    UINT64 fieldVal
) const
{
    return TestField64(regValue, field, NO_INDEXES, fieldVal);
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField64
(
    UINT64 regValue,
    ModsGpuRegField field,
    UINT32 index,
    UINT64 fieldVal
) const
{
    return TestField64(regValue, field, ArrayIndexes_t{index}, fieldVal);
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField64
(
    UINT64 regValue,
    ModsGpuRegValue value
) const
{
    return TestField64(regValue, value, NO_INDEXES);
}

//! \brief Check whether a field has the specified value in a pre-read register
inline bool RegHalTable::TestField64
(
    UINT64 regValue,
    ModsGpuRegValue value,
    UINT32 index
) const
{
    return TestField64(regValue, value, ArrayIndexes_t{index});
}

// ********************************************
// * Declarations for compile-time HAL tables *
// ********************************************

// Used by genreghalimpl.py to create the compile-time HAL tables
#define DECLARE_REGHAL_TABLE(GPU, ELEMENTS)                          \
    namespace RegHalTableInfo                                        \
    {                                                                \
        extern const RegHalTable::Table g_##GPU;                     \
        const RegHalTable::Table g_##GPU =                           \
        {                                                            \
            ELEMENTS,                                                \
            sizeof(ELEMENTS) / sizeof(RegHalTable::Element)          \
        };                                                           \
    }

#endif // INCLUDED_REGHALTABLE_H
