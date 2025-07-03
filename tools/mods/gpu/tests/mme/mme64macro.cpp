/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mme64macro.h"

#include <algorithm>
#include <cstdio>

using namespace Mme64;

//------------------------------------------------------------------------------
//!
//! \brief Reset all the group data and metadata.
//!
//! NOTE: Still need to free the macro in the MME if it has been loaded.
//!
void Macro::Reset()
{
    groups.clear();
    inputData.clear();
    numEmissions  = NUM_EMISSIONS_UNKNOWN;
    cycleCount    = CYCLE_COUNT_UNKNOWN;
    mmeTableEntry = NOT_LOADED;
}

//!
//! \brief Print out the contents of the macro.
//!
void Macro::Print(Tee::Priority printLevel) const
{
    string fmt = "Macro:\n"
        "\tNumber of groups: %lu\n"
        "\tInput data size: %lu DWORDs\n"
        "\tNumber of emissions: ";
    fmt += (numEmissions == NUM_EMISSIONS_UNKNOWN
            ? "unknown" : Utility::StrPrintf("%u", numEmissions));
    fmt += "\n\tCycle count: ";
    fmt += (cycleCount == CYCLE_COUNT_UNKNOWN
            ? "unknown" : Utility::StrPrintf("%u", cycleCount));
    fmt += "\n\tMME table entry: ";
    fmt += (mmeTableEntry == NOT_LOADED
            ? "not loaded" : Utility::StrPrintf("%u", mmeTableEntry));
    fmt += "\nDumping Groups...\n";

    Printf(printLevel, fmt.c_str(), groups.size(), inputData.size());

    const UINT32 macroSize = static_cast<UINT32>(groups.size());
    MASSERT(macroSize == groups.size()); // Truncation check
    const INT32 macroSizeStrLen = snprintf(nullptr, 0, "%u", macroSize);

    for (UINT32 i = 0; i < groups.size(); ++i)
    {
        const auto& group = groups[i];
        const INT32 groupNumStrLen = snprintf(nullptr, 0, "%u", i);

        Printf(printLevel, "Group %u,%*s emissions %u: %s\n",
               i, macroSizeStrLen - groupNumStrLen, " ", group.GetNumEmissions(),
               group.Disassemble().c_str());
    }
}

//!
//! \brief True if the macro is loaded into the MME instruction RAM.
//!
bool Macro::IsLoaded() const
{
    return mmeTableEntry != NOT_LOADED;
}

//------------------------------------------------------------------------------
Emission::Emission(UINT16 method, UINT32 data)
    : method(method)
    , data(data)
{
    MASSERT(!(method & ~0x0fff)); // Method is 12-bits
}

//!
//! \brief Get method. Same as method address.
//!
//! \see GetMethodAddr
//!
UINT16 Emission::GetMethod() const
{
    return GetMethodAddr();
}

//!
//! \brief Get method address. Same as method.
//!
//! \see GetMethod
//!
UINT16 Emission::GetMethodAddr() const
{
    return method;
}

string Emission::ToString() const
{
    MASSERT(GetMethod() == GetMethodAddr()); // Represent the same value
    return Utility::StrPrintf("Method(0x%03x), Data(0x%08x)",
                              GetMethod(), GetData());
}

bool Emission::operator==(const Emission& other)
{
    // Method and method address represent the same value
    MASSERT(this->GetMethod() == this->GetMethodAddr());
    MASSERT(other.GetMethod() == other.GetMethodAddr());

    if (this == &other)
    {
        return true;
    }

    return this->GetMethod() == other.GetMethod()
        && this->GetData() == other.GetData();
}

bool Emission::operator==(const I2mEmission& other)
{
    // Method and method address represent the same value
    MASSERT(this->GetMethod() == this->GetMethodAddr());
    MASSERT(other.GetMethod() == other.GetMethodAddr());

    return other.IsValid()
        && this->GetMethod() == other.GetMethod()
        && this->GetData() == other.GetData();
}

bool Emission::operator!=(const Emission& other)
{
    return !(*this == other);
}

bool Emission::operator!=(const I2mEmission& other)
{
    return !(*this == other);
}

//------------------------------------------------------------------------------
//!
//! \brief Get method. Same as method address.
//!
//! \see GetMethodAddr
//!
UINT16 I2mEmission::GetMethod() const
{
    return GetMethodAddr();
}

//!
//! \brief Get method address. Same as method.
//!
//! \see GetMethod
//!
UINT16 I2mEmission::GetMethodAddr() const
{
    return upper & 0x0fff;
}

string I2mEmission::ToString() const
{
    return Utility::StrPrintf("Method(0x%03x), Data(0x%08x), PC(0x%08x), "
                              "Subch(%u)", GetMethod(), GetData(),
                              GetPC(), GetSubchannel());
}

//!
//! \brief Colwert the inline-to-memory emission into a generic emission.
//!
Emission I2mEmission::ToEmission() const
{
    return Emission(GetMethod(), GetData());
}

bool I2mEmission::operator==(const I2mEmission& other)
{
    // Method and method address represent the same value
    MASSERT(this->GetMethod() == this->GetMethodAddr());
    MASSERT(other.GetMethod() == other.GetMethodAddr());

    if (this == &other)
    {
        return true;
    }

    return this->IsValid() && other.IsValid()
        && this->GetMethod() == other.GetMethod()
        && this->GetData() == other.GetData()
        && this->GetSubchannel() == other.GetSubchannel()
        && this->GetPC() == other.GetPC();
}

bool I2mEmission::operator==(const Emission& other)
{
    // Method and method address represent the same value
    MASSERT(this->GetMethod() == this->GetMethodAddr());
    MASSERT(other.GetMethod() == other.GetMethodAddr());

    return this->IsValid()
        && this->GetMethod() == other.GetMethod()
        && this->GetData() == other.GetData();
}

bool I2mEmission::operator!=(const I2mEmission& other)
{
    return !(*this == other);
}

bool I2mEmission::operator!=(const Emission& other)
{
    return !(*this == other);
}
