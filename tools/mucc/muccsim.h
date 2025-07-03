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
#pragma once
#ifndef INCLUDED_MUCCSIM_H
#define INCLUDED_MUCCSIM_H

#include "muccpgm.h"

// This file contains the Mucc::SimulatedThread base class, along
// with the classes used to implement Mucc::SimulatedThread.
//
namespace Mucc
{
    using namespace LwDiagUtils;
    class Launcher;
    class Thread;
    class Program;
    using ThreadMask = boost::dynamic_bitset<UINT32>;

    //----------------------------------------------------------------
    //! \brief Simulates a register file for some SimulatedThread instance
    //!
    class SimulatedRegFile
    {
    public:
        SimulatedRegFile(UINT32 numRegs, UINT32 regWidth);
        EC Read(UINT32 regNum, UINT32* target) const;
        EC Write(UINT32 regNum, UINT32 value, UINT32 increment, bool prbs);
        EC Increment(UINT32 regNum);
    private:

        struct RegInfo
        {
            UINT32 value;
            UINT32 increment;
            bool prbs;
        };

        vector<RegInfo> m_Registers;
        UINT32 m_RegMask;
    };

    // In this case, Patram is an existing type that already includes
    // state to represent the contents of a dram entry.  We reuse that
    // type to simplify testing for equality between patram entries
    // which hold the "correct" data and dram entries which might have
    // errors.
    //
    // Depending on the complexity that might be added to an entry class
    // later (such as testing for timing discipline violations, etc), a
    // standalone class might eventually be a better fit.  For now, though,
    // all we need is to test equality of the data (for checking for read
    // errors), so a typedef works for now.
    //
    // If a standalone class is created in the future, just make sure that
    // there's a constructor that acceps a Patram object.
    using SimulatedMemoryEntry = Patram;

    //----------------------------------------------------------------
    //! \brief Simulates memory for some SimulatedThread instance
    //!
    class SimulatedMemory
    {
    public:
        SimulatedMemory();

        EC Activate(UINT32 bank, UINT32 row);
        EC Read(UINT32 bank, UINT32 column, const Patram& correctData, bool* readError);
        EC Write(UINT32 bank, UINT32 column, const SimulatedMemoryEntry& data);
    private:

        struct Location
        {
            UINT32 bank;
            UINT32 row;
            UINT32 column;

            bool operator==(const Location & rhs) const
            {
                return bank   == rhs.bank &&
                       row    == rhs.row  &&
                       column == rhs.column;
            }

            struct HashFunction
            {
                size_t operator()(const Location & _) const
                {
                    constexpr size_t maxColumnSize = 6;
                    constexpr size_t maxRowSize    = 15;
                    return _.column                 ^
                           (_.row << maxColumnSize) ^
                           (_.bank << (maxColumnSize + maxRowSize));
                }
            };
        };

        UINT32 m_LwrrentRow; // Represents the row that is stored in the row buffer
        unordered_map<Location, Patram, Location::HashFunction> m_Data;
        bool IsReadError(const SimulatedMemoryEntry& entry, const Patram& patram);
    };

    //----------------------------------------------------------------
    //! \brief Abstract base class that simulates code that runs on some
    //! set of subpartitions
    //!
    //! The state that is contained here should be needed for all possible
    //! simulations, regardless of which litter is being simulated.  This
    //! would include things like a program counter, a cycle counter, etc
    //!
    //! Any additional state that might be necessary for the simulation of
    //! a specific litter should be added to a child class, and a definition
    //! of SimulatedThread::SimulateInstruction should be provided that will
    //! do the heavy lifting of actually simulating each instruction
    //!
    class SimulatedThread
    {
    public:
        SimulatedThread(const Thread& thread,
                        SimulatedRegFile&& regFile,
                        SimulatedMemory&&  memory);
        virtual ~SimulatedThread() {}
        EC Run();
    protected:
        virtual EC SimulateInstruction() = 0;
        UINT32 GetInstructionBits(UINT32 highBit, UINT32 lowBit);
        UINT32 m_PC;
        UINT64 m_LwrrentCycle = 0;
        SimulatedRegFile m_RegFile;
        SimulatedMemory m_Memory;
        const vector<Patram>& m_Patram;
        bool m_Terminate = false;
    private:
        const Thread& m_Thread;
        const Instruction* m_Instruction;
    };
}

#endif // INCLUDED_MUCCSIM_H
