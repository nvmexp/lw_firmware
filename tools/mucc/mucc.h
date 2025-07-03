/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once
#ifndef INCLUDED_MUCC_H
#define INCLUDED_MUCC_H

#include "mucccore.h"
#include "muccpgm.h"
#include "muccsim.h"

// This file is the main "entry point" to the Mucc system
// https://confluence.lwpu.com/display/GM/MuCC+Assembly+Language
//
// The caller should allocate a Mucc::Launcher, set the litter and
// possibly the flags, and then call AllocProgram() get a
// Mucc::Program subclass which can compile the program in a
// litter-specific way.
//
// See muccpgm.h for the Mucc::Program interface.  The subclasses are
// in mucc<litter>pgm.h, but we should try *NOT* to include those
// files in the core mucc files (mucc.*, mucccore.*, muccpgm.*) since
// they contain chip-specific info akin to hwref manuals, meaning that
// different litter files are likely to not "play nice" if you include
// several of them in the same .cpp file (except for cases in which
// one litter is a subclass of another).
//
namespace Mucc
{
    class Launcher
    {
    public:
        Launcher()
        {
            // Default build flags
            m_Flags.SetDefault(Flag::WARN_MIDLABEL, true);
            m_Flags.SetDefault(Flag::WARN_NOP,      true);
            m_Flags.SetDefault(Flag::WARN_OCTAL,    true);
        }

        // Set/get the litter, which determines which subclass of
        // Mucc::Program will be used to assemble the code
        //
        void       SetLitter(AmapLitter litter) { m_Litter = litter; }
        AmapLitter GetLitter()            const { return m_Litter; }

        // Set/get the build flags, which mostly determine which
        // warnings to enable
        //
        void   SetFlag(Flag fl, bool val)   { m_Flags.SetFlag(fl, val); }
        bool   GetFlag(Flag fl)       const { return m_Flags.GetFlag(fl); }
        bool   IsFlagSet(Flag fl)     const { return m_Flags.IsFlagSet(fl); }
        const Flags& GetFlags()       const { return m_Flags; }

        // Allocate a subclass of Mucc::Program, based on the litter
        //
        unique_ptr<Program> AllocProgram() const
        {
            unique_ptr<Program> pProgram = AllocProgramImpl();
            if (pProgram != nullptr)
            {
                pProgram->Initialize();
            }
            return pProgram;
        }

        // Allocate a subclass of Mucc::SimulatedThread, based on the litter
        //
        unique_ptr<SimulatedThread> AllocSimulatedThread(const Thread& thread) const
        {
            return AllocSimulatedThreadImpl(thread);
        }

    private:
        // Methods called by AllocProgramImpl() and AllocSimulatedProgramThreadImpl()
        // which should be defined in their respective subclasses in:
        //
        // mucc<litter>pgm.cpp for allocating subclasses of Program
        // mucc<litter>sim.cpp for allocating subclasses of SimulatedThread
        //
        unique_ptr<Program> AllocGalit1Pgm() const;
        unique_ptr<Program> AllocGhlit1Pgm() const;
        unique_ptr<SimulatedThread> AllocGalit1SimulatedThread(const Thread& thread) const;

        // Called by AllocProgram()
        //
        unique_ptr<Program> AllocProgramImpl() const
        {
            switch (m_Litter)
            {
                case LITTER_GALIT1:
                    return AllocGalit1Pgm();
                case LITTER_GHLIT1:
                    return AllocGhlit1Pgm();
                case LITTER_ILWALID:
                    return nullptr;
                default:
                    LWDASSERT(!"Unknown litter");
                    return nullptr;
            }
        }

        // Called by AllocSimulatedThread()
        //
        unique_ptr<SimulatedThread> AllocSimulatedThreadImpl(const Thread& thread) const
        {
            switch (m_Litter)
            {
                case LITTER_GALIT1:
                    return AllocGalit1SimulatedThread(thread);
                case LITTER_ILWALID:
                    return nullptr;
                default:
                    LWDASSERT(!"Unknown litter");
                    return nullptr;
            }
        }

        AmapLitter m_Litter = LITTER_ILWALID;
        Flags      m_Flags;
    };
};

#endif // INCLUDED_MUCC_H
