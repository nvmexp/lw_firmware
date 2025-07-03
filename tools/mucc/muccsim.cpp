/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "muccsim.h"
#include "lwmisc.h"
using namespace LwDiagUtils;

namespace Mucc
{
    //----------------------------------------------------------------
    //! \brief SimulatedRegFile constructor
    //!
    //! \param numRegs the number of registers that should be in the regfile
    //! \param regWidth the number of bits in each register
    //!
    SimulatedRegFile::SimulatedRegFile(UINT32 numRegs, UINT32 regWidth) :
        m_Registers(numRegs),
        m_RegMask((1U << regWidth) - 1U)
    {
    }

    //----------------------------------------------------------------
    //! \brief Read a value from the register file
    //!
    EC SimulatedRegFile::Read(UINT32 regNum, UINT32* target) const
    {
        EC ec = EC::OK;

        if (regNum >= m_Registers.size())
        {
            return BAD_PARAMETER;
        }

        const UINT32 value = m_Registers[regNum].value;

        // Ensure that value fits within the register size
        //
        // All methods that write to the register file must zero out
        // all bits that aren't included in m_RegMask; this assertion
        // checks for a failure to do so
        LWDASSERT(!(value & ~m_RegMask));

        *target = value;

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Write a value to the register file
    //!
    EC SimulatedRegFile::Write(UINT32 regNum, UINT32 value,
                               UINT32 increment, bool prbs)
    {
        EC ec = EC::OK;

        if (regNum >= m_Registers.size())
        {
            return BAD_PARAMETER;
        }

        m_Registers[regNum] = {value & m_RegMask, increment, prbs};

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Generate next element in a pseudo-random binary sequence
    //!
    //! Used in SimulatedRegFile::Increment
    //!
    static unsigned GenerateNextPRBS(unsigned prev)
    {
        unsigned next = prev << 1;
        next |= ((prev >> 14) ^ (prev >> 13)) & 0x1;
        return next & 0x7fff;
    }

    //----------------------------------------------------------------
    //! \brief Write a value to the register file
    //!
    EC SimulatedRegFile::Increment(UINT32 regNum)
    {
        EC ec = EC::OK;

        if (regNum >= m_Registers.size())
        {
            return BAD_PARAMETER;
        }

        RegInfo & regInfo = m_Registers[regNum];

        if (regInfo.prbs)
        {
            regInfo.value = (GenerateNextPRBS(regInfo.value)) & m_RegMask;
        }
        else
        {
            regInfo.value = (regInfo.value + regInfo.increment) & m_RegMask;
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief SimulatedMemory constructor
    //!
    SimulatedMemory::SimulatedMemory() :
        m_LwrrentRow(0)
    {
    }

    //----------------------------------------------------------------
    //! \brief Select a row for an upcoming read or write command
    //!
    EC SimulatedMemory::Activate(UINT32 bank, UINT32 row)
    {
        EC ec = EC::OK;

        m_LwrrentRow = row;

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Read a piece of data from the simulated memory
    //!
    EC SimulatedMemory::Read(UINT32 bank, UINT32 column,
                             const Patram& correctData, bool* readError)
    {
        EC ec = EC::OK;

        const Location location = {bank, m_LwrrentRow, column};
        auto entryItr = m_Data.find(location);

        if (entryItr == m_Data.end())
        {
            *readError = true;
        }
        else
        {
            *readError = IsReadError(entryItr->second, correctData);
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Read a piece of data from the simulated memory
    //!
    EC SimulatedMemory::Write(UINT32 bank, UINT32 column, const SimulatedMemoryEntry& data)
    {
        EC ec = EC::OK;

        const Location location = {bank, m_LwrrentRow, column};
        m_Data.insert({location, data});

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Check if an entry read from dram matches a patram entry
    //!
    bool SimulatedMemory::IsReadError(const SimulatedMemoryEntry& entry,
                                      const Patram& patram)
    {
        return entry.GetDq()  != patram.GetDq()  ||
               entry.GetEcc() != patram.GetEcc() ||
               entry.GetDbi() != patram.GetDbi();
    }

    //----------------------------------------------------------------
    //! \brief SimulatedThread constructor
    //!
    //! A copy on the thread is performed so that we can use the mutable
    //! interface of Thread without modifying the original object
    //!
    SimulatedThread::SimulatedThread(const Thread& thread,
                                     SimulatedRegFile&& regFile,
                                     SimulatedMemory&& memory) :
        m_PC(thread.FindLabel("start")->second),
        m_RegFile(regFile),
        m_Memory(memory),
        m_Patram(thread.GetPatrams()),
        m_Thread(thread),
        m_Instruction(nullptr)
    {
    }

    //----------------------------------------------------------------
    //! \brief Run the simulation on a SimulatedThread
    //!
    EC SimulatedThread::Run()
    {
        EC ec = EC::OK;

        const UINT64 maxCycles = m_Thread.GetMaxCycles();
        const UINT64 endLocation = m_Thread.GetInstructions().size();

        // Termination condition: program counter has reached end, we've
        // reached the max number of cycles, or we received a termination
        // signal from the subclass
        //
        // In GA100, the terminate signal is used to terminate when the
        // "stop_exelwtion" bit is set in an instruction. In general,
        // the terminate signal should be given whenever any litter-specific
        // termination event happens that the abstract interface couldn't
        // know about (by virtue of it being abstract and not knowing specifics)
        while (m_PC != endLocation &&
              (maxCycles == 0 || m_LwrrentCycle < maxCycles) &&
              !m_Terminate)
        {
            // Get instruction
            if (m_PC >= endLocation)
            {
                Printf(PriError, "Invalid value for PC: %u", m_PC);
                return BAD_COMMAND_LINE_ARGUMENT;
            }
            m_Instruction = &m_Thread.GetInstructions()[m_PC];

            EC ec = SimulateInstruction();

            ++m_LwrrentCycle;

            if (ec != EC::OK)
            {
                Printf(PriError, "Program error encountered at PC = %d\n", m_PC);
                return BAD_COMMAND_LINE_ARGUMENT;
            }
        }

        try
        {
            Printf(PriNormal, "Finished exelwtion of thread with mask 0x%lx in %llu cycles.\n",
                   m_Thread.GetThreadMask().to_ulong(),
                   m_LwrrentCycle);
        }
        catch (...)
        {
            return SOFTWARE_ERROR; // to_ulong threw an exception
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Harvest some bits from the current instruction
    //!
    UINT32 SimulatedThread::GetInstructionBits(UINT32 highBit, UINT32 lowBit)
    {
        LWDASSERT(m_Instruction);
        return m_Instruction->GetBits().GetBits(highBit, lowBit);
    }
}
