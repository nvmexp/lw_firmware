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

#include "mme64sim.h"

#include <array>
#include <limits.h>

using namespace Mme64;

//!
//! \brief Called when an error oclwrs during simulation.
//!
//! Required to be defined by the MME64 Simulator.
//!
void mmeError(string s, const ScanOp* op)
{
    Printf(Tee::PriError, "[mme64sim] %s\n", s.c_str());
}

//!
//! \brief Called when a warning oclwrs during simulation.
//!
//! Required to be defined by the MME64 Simulator.
//!
void mmeWarning(string category, string msg)
{
    Printf(Tee::PriWarn, "[mme64sim] %s: %s\n", category.c_str(), msg.c_str());
}

//!
//! \brief Call whena trace print oclwrs during simulation.
//!
//! Required to be defined by the MME64 Simulator.
//!
void mmeTrace(const char* msg)
{
    Printf(Tee::PriLow, "[mme64sim]: %s\n", msg);
}

//!
//! Required to be defined by the MME64 Simulator.
//!
vector<ScanOp> parse(string text, vector<Pragma>& outPragmas)
{
    MASSERT(!"mme64sim parse should not be called");
    outPragmas.clear();
    return vector<ScanOp>();
}

//------------------------------------------------------------------------------
//!
//! \brief Constructor. Creates given number of entries starting at the given address.
//!
//! The address of each entry starts at the given address and is added
//! sequentially in the range [start_addr, start_addr + num_entries - 1]. Values
//! are initialized to zero.
//!
//! \param startAddr Starting address.
//! \param numEntries Number of entries to create.
//!
Simulator::Memory::Memory(UINT32 startAddr, UINT32 numEntries)
{
    Reserve(numEntries);

    for (UINT32 n = 0; n < numEntries; ++n)
    {
        Set(startAddr + n, 0);
    }
}

Simulator::Memory::Memory(Simulator::Memory&& o)
    : m_Memory(std::move(o.m_Memory))
{}

Simulator::Memory::Memory(const Simulator::Memory& o)
    : m_Memory(o.m_Memory)
{}

//!
//! \brief Set the given address to the given value.
//!
//! NOTE: To avoid performance conerns (O(n) lookup), it is up to
//! the caller to make sure the same address is not written to twice.
//!
//! \param addr Memory address to write.
//! \param value Value the given address should be set to.
//!
void Simulator::Memory::Set(Address addr, Value value)
{
    const unsigned int uintAddr = static_cast<unsigned int>(addr);
    MASSERT(uintAddr == addr); // Truncation check
    const unsigned int uintValue = static_cast<unsigned int>(value);
    MASSERT(uintValue == value); // Truncation check

    m_Memory.push_back(uintAddr);
    m_Memory.push_back(uintValue);
}

//!
//! \brief Get the nth set entry, where n is the given entry index.
//!
//! \param entryIndex Added entry index.
//!
std::pair<Simulator::Memory::Address, Simulator::Memory::Value>
Simulator::Memory::GetEntry(UINT32 entryIndex) const
{
    // Each entry consists of two adjacent elements in the vector
    const UINT32 index = entryIndex * 2;
    MASSERT(index + 1 < m_Memory.size());

    const Address addr = static_cast<Address>(m_Memory.at(index));
    const Value val = static_cast<Value>(m_Memory.at(index + 1));
    return std::make_pair(addr, val);
}

//!
//! \brief Set the nth entry that was set, where n is the given entry index.
//!
//! \param entryIndex Added entry index.
//! \param addr Memory address to write.
//! \param value Value the given address should be set to.
//!
void Simulator::Memory::SetEntry(UINT32 entryIndex, Address addr, Value value)
{
    // Each entry consists of two adjacent elements in the vector
    const UINT32 index = entryIndex * 2;
    MASSERT(index + 1 < m_Memory.size());

    const unsigned int uintAddr = static_cast<unsigned int>(addr);
    MASSERT(uintAddr == addr); // Truncation check
    const unsigned int uintValue = static_cast<unsigned int>(value);
    MASSERT(uintValue == value); // Truncation check

    m_Memory[index] = uintAddr;
    m_Memory[index + 1] = uintValue;
}

//!
//! \brief clears all previously set entries.
//!
void Simulator::Memory::Clear()
{
    m_Memory.clear();
}

//!
//! \brief Reserves space for the given number of entries.
//!
//! \param numEntries Number of memory addresses to be written.
//!
void Simulator::Memory::Reserve(UINT32 numEntries)
{
    m_Memory.reserve(GetSimMemArraySize(numEntries));
}

//!
//! \brief Return true if empty.
//!
bool Simulator::Memory::Empty() const
{
    return m_Memory.empty();
}

//!
//! \brief Return the number of memory entries.
//!
UINT32 Simulator::Memory::GetNumEntries() const
{
    // There are two elements for each memory each: [addr, data].
    const UINT32 numEntries = static_cast<UINT32>(m_Memory.size() / 2);
    MASSERT(numEntries == (m_Memory.size() / 2)); // Truncation check
    return numEntries;
}

//!
//! \brief Return the underlying simulator formatted array.
//!
unsigned int* Simulator::Memory::GetSimArray()
{
    return m_Memory.data();
}

//!
//! \brief Return the size of the underlying simulator formatted array.
//!
int Simulator::Memory::GetSimArraySize() const
{
    const int size = static_cast<int>(m_Memory.size());
    MASSERT(static_cast<UINT32>(size) == m_Memory.size()); // Truncation check

    return size;
}

//!
//! \brief Return the size of the array needed to convey the
//! information for the given number of memory entries to the
//! simulator.
//!
//! \param numEntries Number of memory addresses to be written.
//!
UINT32 Simulator::Memory::GetSimMemArraySize(UINT32 numEntries) const
{
    // Reserve double the RAM size: each RAM entry is represented
    // by a address, value pair.
    return numEntries * 2;
}

Simulator::Memory&
Simulator::Memory::operator=(Simulator::Memory&& o)
{
    if (this != &o)
    {
        m_Memory = std::move(o.m_Memory);
    }

    return *this;
}

Simulator::Memory&
Simulator::Memory::operator=(const Simulator::Memory& o)
{
    if (this != &o)
    {
        m_Memory = o.m_Memory;
    }

    return *this;
}

//------------------------------------------------------------------------------
//!
//! \brief Run the given macro program on the simulator.
//!
//! \param[in,out] pMacro Macro to run. Cycle count is set on run completion.
//! \param config Run configuration.
//!
RC Simulator::Run
(
    Macro* const pMacro
    ,RunConfiguration& config // TODO: const attribute will fail because the
                              // simulator expects a non-const value
)
{
    RC rc;
    MASSERT(pMacro);
    const Macro::Groups& groups = pMacro->groups;

    // Callback must be set
    MASSERT(config.callback);
    // Simulator accepts an 'int' number of instructions, make sure we don't overflow
    MASSERT(groups.size() < static_cast<UINT32>(INT_MAX));

    // Colwert the instructions into the simulator format
    //
    vector<UINT32> instructionData;
    instructionData.reserve(groups.size() * Group<2>::GetElementCount());

    // For each group
    for (auto groupIter = groups.begin();
         groupIter != groups.end();
         ++groupIter)
    {
        // For each data element in group
        for (auto instrIter = groupIter->begin();
             instrIter != groupIter->end();
             ++instrIter)
        {
            instructionData.push_back(*instrIter);
        }
    }

    // Create a MME64 Simulator program
    //
    // Check overflow
    MASSERT(instructionData.size() <=
            static_cast<vector<UINT32>::size_type>(std::numeric_limits<int>::max()));

    MME2Program prog(instructionData.data(),
                     static_cast<int>(instructionData.size()),
                     config.callback, config.traceEnabled,
                     config.disableMemoryUnit);

    // Execute the simulator program
    //
    vector<UINT32>& inputData = pMacro->inputData;
    const int inputDataSize = static_cast<int>(inputData.size());
    MASSERT(static_cast<UINT32>(inputDataSize) == inputData.size()); // Truncation check
    Memory* pShadowRamState = config.pShadowRamState;
    Memory* pDataRamState = config.pDataRamState;
    Memory* pMemoryState = config.pMemoryState;

    int cycleCount = prog.run(config.pCallbackData,
                              inputData.data(), inputDataSize,
                              pShadowRamState->GetSimArray(), pShadowRamState->GetSimArraySize(),
                              // The arg methodTriggerState is only used for the handshake
                              // with FECS falcon for PRI RM, can be safely ignored.
                              nullptr, 0,
                              pDataRamState->GetSimArray(), pDataRamState->GetSimArraySize(),
                              pMemoryState->GetSimArray(), pMemoryState->GetSimArraySize());

    // Check for error
    if (cycleCount == RUN_ERROR)
    {
        Printf(Tee::PriError, "MME64 Simulator failed to run with:\n\t%s\n",
               prog.getError());
        return RC::SOFTWARE_ERROR;
    }
    else if (cycleCount < 0)
    {
        Printf(Tee::PriError, "Simulator encounted unknown error.\n"
               "\tError report: %s\n", prog.getError());
        return RC::SOFTWARE_ERROR;
    }

    // Check for warning
    char* warnCategory = prog.getWarningCat();
    char* warnMsg = prog.getWarningMsg();
    if (warnCategory[0] && warnMsg[0])
    {
        mmeWarning(warnCategory, warnMsg);
    }

    pMacro->cycleCount = static_cast<UINT32>(cycleCount);

    return rc;
}
