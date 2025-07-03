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

#include "reghal.h"
#include "lwmisc.h"
#ifdef MATS_STANDALONE
#include "fakemods.h"
#else
#include "gpu/include/testdevice.h"
#endif
#include <unordered_set>
#include <string>

// Assume the convention that bits 3:0 of a LW_*_PRIV_LEVEL_MASK
// register give read access, and bits 7:4 give write access
//
#define PRIV_LEVEL_MASK_READ_PROTECTION                     3:0
#define PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED  0xf
#define PRIV_LEVEL_MASK_WRITE_PROTECTION                    7:4
#define PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED 0xf

// This struct holds a set of priv-protected registers, including
// their domain and array indexes.  It's used during debugging to
// keep track of which registers we've passed to HasReadAccess() and
// HasRWAccess(), so that MODS can MASSERT if we try to access a
// register without checking its priv level first.
//
struct RegHal::PrivRegSet
{
    struct Key
    {
        UINT32            domainIndex;
        ModsGpuRegAddress address;
        ArrayIndexes_t    regIndexes;
        bool operator==(const Key& rhs) const;
    };
    struct Hash
    {
        size_t operator()(const Key& key) const noexcept;
    };
    unordered_set<Key, Hash> m_Set;
};

enum PrivCheckModes
{
     READ               = 1 
    ,READWRITE          = 3
    ,IS_SUPPORTED       = 4
    ,ASSERT_ON_ERROR    = 0x8000
};

//--------------------------------------------------------------------
RegHal::RegHal() = default;

UINT32 RegHal::s_DebugPrivCheckMode = 0;

//--------------------------------------------------------------------
//! \brief Constructor
//!
//! The RegHal will not be usable until the Initialize() is called.
//!
//! \param pTestDevice TestDevice used to read/write registers
//!
RegHal::RegHal(TestDevice* pTestDevice)
    : m_pTestDevice(pTestDevice)
{
    m_CheckedReadAccess  = make_unique<PrivRegSet>();
    m_CheckedWriteAccess = make_unique<PrivRegSet>();
    m_CheckedIsSupported = make_unique<PrivRegSet>();
}

//-----------------------------------------------------------------------------
RegHal::~RegHal()
{
}

//-----------------------------------------------------------------------------
RC RegHal::Initialize()
{
    Initialize(GetTestDevice()->GetDeviceId());
    return OK;
}

//-----------------------------------------------------------------------------
RC RegHal::Initialize(TestDevice* pTestDevice)
{
    m_pTestDevice = pTestDevice;
    return Initialize();
}

//--------------------------------------------------------------------
//! \brief Read a register
UINT32 RegHal::Read32
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    if (s_DebugPrivCheckMode) 
    {
        VerifyIsSupported(domainIndex, address, arrayIndexes);
        VerifyReadAccessCheck(domainIndex, address, arrayIndexes);
    }
    return GetTestDevice()->RegRd32(domainIndex,
                                    LookupDomain(address, arrayIndexes),
                                    GetLwRmPtr(),
                                    LookupAddress(address, arrayIndexes));        
}

//--------------------------------------------------------------------
//! \brief Read a register after checking its priv level
RC RegHal::Read32Priv
(
    ModsGpuRegAddress address,
    UINT32            regIndex,
    UINT32*           pValue
) const
{
    MASSERT(pValue);

    if (!IsSupported(address, regIndex))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!HasReadAccess(address, regIndex))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    *pValue = Read32(address, regIndex);
    return RC::OK;
}

//! \brief Read a register field after checking its priv level
RC RegHal::Read32Priv(ModsGpuRegField field, UINT32* pValue) const
{
    MASSERT(pValue);

    if (!IsSupported(field))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!HasReadAccess(ColwertToAddress(field)))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    *pValue = Read32(field);
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Write a register
void RegHal::Write32
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT32            value
)
{
    if (s_DebugPrivCheckMode) 
    {
        VerifyIsSupported(domainIndex, address, arrayIndexes);
        VerifyWriteAccessCheck(domainIndex, address, arrayIndexes);
    }
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);;

    GetTestDevice()->RegWr32(domainIndex,
                             regDomain,
                             GetLwRmPtr(),
                             LookupAddress(address, arrayIndexes),
                             value);
}

//--------------------------------------------------------------------
//! \brief Write a register after checking the priv level
RC RegHal::Write32Priv
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT32            value
)
{
    if (!IsSupported(address, arrayIndexes))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!HasRWAccess(address, arrayIndexes))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }
    Write32(domainIndex, space, address, arrayIndexes, value);

    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Write a register field after checking the priv level
RC RegHal::Write32Priv
(
    UINT32          domainIndex,
    RegSpace        space,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
)
{
    if (!IsSupported(ColwertToAddress(field), arrayIndexes))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!HasRWAccess(ColwertToAddress(field), arrayIndexes))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }
    Write32(domainIndex, space, field, arrayIndexes, value);
    
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Write a register
void RegHal::Write32Broadcast
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT32            value
)
{
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    GetTestDevice()->RegWrBroadcast32(domainIndex,
                                      regDomain,
                                      GetLwRmPtr(),
                                      LookupAddress(address, arrayIndexes),
                                      value);
}

//--------------------------------------------------------------------
//! \brief Write a register
void RegHal::Write32Sync
(
    UINT32            domainIndex,
    ModsGpuRegAddress  address,
    ArrayIndexes       arrayIndexes,
    UINT32             value
)
{
    GetTestDevice()->RegWrSync32(domainIndex,
                                 LookupDomain(address, arrayIndexes),
                                 GetLwRmPtr(),
                                 LookupAddress(address, arrayIndexes),
                                 value);
}

//--------------------------------------------------------------------
//! \brief Write a register after checking its priv level
RC RegHal::Write32PrivSync
(
    ModsGpuRegAddress  address,
    UINT32             arrayIndex,
    UINT32             value
)
{
    if (!IsSupported(address, arrayIndex))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!HasRWAccess(address, arrayIndex))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    Write32Sync(address, arrayIndex, value);
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Write a register field
void RegHal::Write32
(
    UINT32          domainIndex,
    RegSpace        space,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
)
{
    const ModsGpuRegAddress address = ColwertToAddress(field);
    if (s_DebugPrivCheckMode) 
    {
        VerifyIsSupported(domainIndex, address, arrayIndexes);
        VerifyWriteAccessCheck(domainIndex, ColwertToAddress(field), arrayIndexes);
    }
    
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    const UINT32 regAddress = LookupAddress(ColwertToAddress(field), addressIndexes);

    RegHalDomain regDomain = LookupDomain(ColwertToAddress(field), addressIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);;

    UINT32 regValue = GetTestDevice()->RegRd32(domainIndex,
                                               regDomain,
                                               GetLwRmPtr(),
                                               regAddress);
    SetField(&regValue, field, fieldIndexes, value);
    GetTestDevice()->RegWr32(domainIndex,
                             regDomain,
                             GetLwRmPtr(),
                             regAddress,
                             regValue);
}

//--------------------------------------------------------------------
//! \brief Write a register field
void RegHal::Write32Sync
(
    UINT32          domainIndex,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
)
{
    RC rc;
    const ModsGpuRegAddress address = ColwertToAddress(field);
    if (s_DebugPrivCheckMode) 
    {
        VerifyIsSupported(domainIndex, address, arrayIndexes);
        VerifyWriteAccessCheck(domainIndex, ColwertToAddress(field), arrayIndexes);   
        VerifyReadAccessCheck(domainIndex, ColwertToAddress(field), arrayIndexes);
    }    
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    const UINT32 regAddress      = LookupAddress(ColwertToAddress(field), addressIndexes);
    const RegHalDomain regDomain = LookupDomain(ColwertToAddress(field), addressIndexes);

    UINT32 regValue = GetTestDevice()->RegRd32(domainIndex,
                                               regDomain,
                                               GetLwRmPtr(),
                                               regAddress);
    SetField(&regValue, field, fieldIndexes, value);
    GetTestDevice()->RegWrSync32(domainIndex,
                                 regDomain,
                                 GetLwRmPtr(),
                                 regAddress,
                                 regValue);
}

//--------------------------------------------------------------------
//! \brief Write a register field after checking its priv level
RC RegHal::Write32PrivSync(ModsGpuRegField field, UINT32 value)
{
    if (!IsSupported(field))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    if (!HasRWAccess(ColwertToAddress(field)))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    Write32Sync(field, value);
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
bool RegHal::Test32
(
    UINT32          domainIndex,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT32          value
) const
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    ModsGpuRegAddress regAddress = ColwertToAddress(field);
    if (!IsSupported(regAddress, addressIndexes))
        return false;

    const UINT32 regVal = Read32(domainIndex, regAddress, addressIndexes);
    return TestField(regVal, field, arrayIndexes, value);
}

//--------------------------------------------------------------------
//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
bool RegHal::Test32
(
    UINT32          domainIndex,
    ModsGpuRegValue value,
    ArrayIndexes    arrayIndexes
) const
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    ArrayIndexes_t valueIndexes;
    LookupArrayIndexes(value, arrayIndexes, &addressIndexes, &fieldIndexes, &valueIndexes);

    ModsGpuRegAddress regAddress = ColwertToAddress(value);
    if (!IsSupported(regAddress, addressIndexes))
        return false;

    const UINT32 regVal = Read32(domainIndex, regAddress, addressIndexes);
    return TestField(regVal, value, arrayIndexes);
}

// **************************
// * 64 bit implementations *
// **************************

//--------------------------------------------------------------------
//! \brief Read a register
UINT64 RegHal::Read64
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    return GetTestDevice()->RegRd64(domainIndex,
                                    LookupDomain(address, arrayIndexes),
                                    GetLwRmPtr(),
                                    LookupAddress(address, arrayIndexes));
}

//--------------------------------------------------------------------
//! \brief Write a register
void RegHal::Write64
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT64            value
)
{
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    GetTestDevice()->RegWr64(domainIndex,
                             regDomain,
                             GetLwRmPtr(),
                             LookupAddress(address, arrayIndexes),
                             value);
}

//--------------------------------------------------------------------
//! \brief Write a register
void RegHal::Write64Broadcast
(
    UINT32            domainIndex,
    RegSpace          space,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    UINT64            value
)
{
    RegHalDomain regDomain = LookupDomain(address, arrayIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    GetTestDevice()->RegWrBroadcast64(domainIndex,
                                      regDomain,
                                      GetLwRmPtr(),
                                      LookupAddress(address, arrayIndexes),
                                      value);
}

//--------------------------------------------------------------------
//! \brief Write a register
void RegHal::Write64Sync
(
    UINT32            domainIndex,
    ModsGpuRegAddress  address,
    ArrayIndexes       arrayIndexes,
    UINT64             value
)
{
    GetTestDevice()->RegWrSync64(domainIndex,
                                 LookupDomain(address, arrayIndexes),
                                 GetLwRmPtr(),
                                 LookupAddress(address, arrayIndexes),
                                 value);
}

//--------------------------------------------------------------------
//! \brief Return true if a register is readable
//!
//! \brief Return true if a priv-protected register is readable, or if
//! the register is not priv-protected (i.e. readable by default)
//!
bool RegHal::HasReadAccess
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    // Get the address of the priv register; return "true" if none exists
    const UINT32 privRegister = LookupPrivAddress(address, arrayIndexes);
    if (privRegister == 0)
    {
        return true;
    }
    if (s_DebugPrivCheckMode & READ)
    {
        PrivRegSet::Key privKey = { domainIndex, address, arrayIndexes };
        m_CheckedReadAccess->m_Set.insert(privKey);
    }   

    // Read the priv register, and check for read access
    //
    const RegHalDomain domain = LookupDomain(address, arrayIndexes);
    const UINT32 privValue = GetTestDevice()->RegRd32(domainIndex, domain,
                                                      GetLwRmPtr(),
                                                      privRegister);
    return FLD_TEST_REF(PRIV_LEVEL_MASK_READ_PROTECTION,
                        _ALL_LEVELS_ENABLED, privValue);
}

//--------------------------------------------------------------------
//! \brief Return true if a register has readable and writable
//!
//! \brief Return true if a priv-protected register has read/write
//! access, or if the register is not priv-protected (i.e. readable
//! and writable by default)
//!
bool RegHal::HasRWAccess
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    // Get the address of the priv register; return "true" if none exists
    const UINT32 privRegister = LookupPrivAddress(address, arrayIndexes);
    if (privRegister == 0)
    {
        return true;
    }

    if (s_DebugPrivCheckMode & READWRITE)
    {
        PrivRegSet::Key privKey = { domainIndex, address, arrayIndexes };
        m_CheckedReadAccess->m_Set.insert(privKey);
        m_CheckedWriteAccess->m_Set.insert(privKey);
    }   
    // Read the priv register, and check for read & write access
    const RegHalDomain domain = LookupDomain(address, arrayIndexes);
    const UINT32 privValue = GetTestDevice()->RegRd32(domainIndex, domain,
                                                      GetLwRmPtr(),
                                                      privRegister);
    return (FLD_TEST_REF(PRIV_LEVEL_MASK_READ_PROTECTION,
                         _ALL_LEVELS_ENABLED, privValue) &&
            FLD_TEST_REF(PRIV_LEVEL_MASK_WRITE_PROTECTION,
                         _ALL_LEVELS_ENABLED, privValue));
}

//--------------------------------------------------------------------
//! \brief Write a register field
void RegHal::Write64
(
    UINT32          domainIndex,
    RegSpace        space,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT64          value
)
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    const UINT32 regAddress = LookupAddress(ColwertToAddress(field), addressIndexes);
    RegHalDomain regDomain = LookupDomain(ColwertToAddress(field), addressIndexes);
    if (space == MULTICAST)
        regDomain = ColwertToMulticast(regDomain);

    UINT64 regValue = GetTestDevice()->RegRd64(domainIndex,
                                               regDomain,
                                               GetLwRmPtr(),
                                               regAddress);
    SetField64(&regValue, field, fieldIndexes, value);
    GetTestDevice()->RegWr64(domainIndex,
                             regDomain,
                             GetLwRmPtr(),
                             regAddress,
                             regValue);
}

//--------------------------------------------------------------------
//! \brief Write a register field
void RegHal::Write64Sync
(
    UINT32          domainIndex,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT64          value
)
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    const UINT32 regAddress      = LookupAddress(ColwertToAddress(field), addressIndexes);
    const RegHalDomain regDomain = LookupDomain(ColwertToAddress(field), addressIndexes);

    UINT64 regValue = GetTestDevice()->RegRd64(domainIndex,
                                               regDomain,
                                               GetLwRmPtr(),
                                               regAddress);
    SetField64(&regValue, field, fieldIndexes, value);
    GetTestDevice()->RegWrSync64(domainIndex,
                                 regDomain,
                                 GetLwRmPtr(),
                                 regAddress,
                                 regValue);
}

//--------------------------------------------------------------------
//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
bool RegHal::Test64
(
    UINT32          domainIndex,
    ModsGpuRegField field,
    ArrayIndexes    arrayIndexes,
    UINT64          value
) const
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    LookupArrayIndexes(field, arrayIndexes, &addressIndexes, &fieldIndexes);

    ModsGpuRegAddress regAddress = ColwertToAddress(field);
    if (!IsSupported(regAddress, addressIndexes))
        return false;

    const UINT64 regVal = Read64(domainIndex, regAddress, addressIndexes);
    return TestField64(regVal, field, arrayIndexes, value);
}

//--------------------------------------------------------------------
//! \brief Check whether a register field has the specified value
//!
//! Return false if the register, field, or value isn't supported.
bool RegHal::Test64
(
    UINT32          domainIndex,
    ModsGpuRegValue value,
    ArrayIndexes    arrayIndexes
) const
{
    ArrayIndexes_t addressIndexes;
    ArrayIndexes_t fieldIndexes;
    ArrayIndexes_t valueIndexes;
    LookupArrayIndexes(value, arrayIndexes, &addressIndexes, &fieldIndexes, &valueIndexes);

    ModsGpuRegAddress regAddress = ColwertToAddress(value);
    if (!IsSupported(regAddress, addressIndexes))
        return false;

    const UINT64 regVal = Read64(domainIndex, regAddress, addressIndexes);
    return TestField64(regVal, value, arrayIndexes);
}

//--------------------------------------------------------------------
//! Read a register with the indicated domain and address
//!
//! This is used to walk the capability structs, presumably in PCIe
//! config space.  It mostly does the same thing as Read32(), except
//! the args are too different to merge into one function.
//!
UINT32 RegHal::ReadCapability32(RegHalDomain domain, UINT32 addr) const
{
    return GetTestDevice()->RegRd32(0, domain, GetLwRmPtr(), addr);
}


//--------------------------------------------------------------------
// Equality operator for PrivRegSet::Key, which is needed to make an
// unordered_set<Key>
//
bool RegHal::PrivRegSet::Key::operator==(const Key& rhs) const
{
    return (domainIndex == rhs.domainIndex &&
            address     == rhs.address     &&
            regIndexes  == rhs.regIndexes);
}

//--------------------------------------------------------------------
// Hash function for PrivRegSet::Key, which is needed to make an
// unordered_set<Key>
//
size_t RegHal::PrivRegSet::Hash::operator()(const Key& key) const noexcept
{
    size_t hash = key.domainIndex;
    hash = (hash * 257) + static_cast<size_t>(key.address);
    for (UINT16 regIndex: key.regIndexes)
    {
        hash = (hash * 257) + regIndex;
    }
    return hash;
}

//--------------------------------------------------------------------
// Check whether the indicated register was passed to HasReadAccess()
// and/or HasRWAccess(), and MASSERT if not.  Use m_CheckedReadAccess
// to keep track of which registers were passed to HasReadAccess
// and/or HasRWAccess.
//

void RegHal::VerifyReadAccessCheck
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      regIndexes
) const
{
    if (s_DebugPrivCheckMode & READ) 
    {
        PrivRegSet::Key key = { domainIndex, address, regIndexes };
        if (m_CheckedReadAccess->m_Set.count(key) == 0 &&
            !HasReadAccess(domainIndex, address, regIndexes))
        {
            // Report the 1st oclwrrence then log the address         
            PrintErrorMsg(domainIndex, address, regIndexes, "read without read privilege");
            m_CheckedReadAccess->m_Set.insert(key);
            if (s_DebugPrivCheckMode & ASSERT_ON_ERROR)
            {
                MASSERT(!"Trying to read register without read privilege\n");                      
            }
        }
    }
}


//--------------------------------------------------------------------
// Check whether the indicated register was passed to HasRWAccess(),
// and MASSERT if not.  Use m_CheckedWriteAccess to keep track of
// which registers were passed to HasReadAccess and/or HasRWAccess.
//
void RegHal::VerifyWriteAccessCheck
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    if (s_DebugPrivCheckMode & READWRITE)
    {
        PrivRegSet::Key key = { domainIndex, address, arrayIndexes };
        if (m_CheckedWriteAccess->m_Set.count(key) == 0 &&
            !HasRWAccess(domainIndex, address, arrayIndexes))
        {
            // Report the 1st oclwrrence then log the address   
            PrintErrorMsg(domainIndex, address, arrayIndexes, "write without write privilege");
            m_CheckedWriteAccess->m_Set.insert(key);
            if (s_DebugPrivCheckMode & ASSERT_ON_ERROR)
            {
                MASSERT(!"Trying to write register without write privilege\n");                      
            }
        }
    }
}

//--------------------------------------------------------------------
// Check whether the indicated register is supported by the hardware,
// and print error message (and potentially MASSERT) if not.  Use 
// m_CheckedIsSupported to keep track of registers which were passed 
// to IsSupported and was supported.
void RegHal::VerifyIsSupported
(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes
) const
{
    PrivRegSet::Key key = { domainIndex, address, arrayIndexes };
    if ((s_DebugPrivCheckMode & IS_SUPPORTED) &&
        (m_CheckedIsSupported->m_Set.count(key) == 0))  
    {
        if (!IsSupported(address, arrayIndexes))
        {
            PrintErrorMsg(domainIndex, address, arrayIndexes, "access unsupported");
            if (s_DebugPrivCheckMode & ASSERT_ON_ERROR)
            {
                MASSERT(!"Trying to access unsupported register\n");                      
            }
        }
        else
        {
            m_CheckedIsSupported->m_Set.insert(key);
        }
    }
}

void RegHal::PrintErrorMsg(
    UINT32            domainIndex,
    ModsGpuRegAddress address,
    ArrayIndexes      arrayIndexes,
    const char *      action
) const
{
    const char *pName = RegHalTable::ColwertToString(address);
    string desc;
    if (pName)
    {
        desc = Utility::StrPrintf(" (%s)", pName);
    }
    string strIndices;
    if (!arrayIndexes.empty()) 
    {
        for (auto it = arrayIndexes.begin(); it != arrayIndexes.end(); it++)
        {
            strIndices += (it == arrayIndexes.begin()) ? "[" : ", ";
            strIndices += to_string(*it);
        }
        strIndices += "]";
    }
    Printf(Tee::PriError, "Trying to %s register 0x%x%s%s\n",
           action, static_cast<UINT32>(address), desc.c_str(), strIndices.c_str());    
}
