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
#ifndef INCLUDED_MUCCGALIT1SIM_H
#define INCLUDED_MUCCGALIT1SIM_H

#include "muccsim.h"
#include "lwmisc.h"

class Galit1Pgm;

namespace Mucc
{
    class Galit1SimThread: public SimulatedThread
    {
    public:
        Galit1SimThread(const Thread& thread,
                        SimulatedRegFile&& regFile,
                        SimulatedMemory&& memory) :
            SimulatedThread(thread, move(regFile), move(memory)), m_pProgram(thread.GetProgram())
        { }

        EC SimulateInstruction() override;

    private:
        Program* m_pProgram;
        // Additional simulation state

        // Holding
        bool m_Holding = false;
        INT32 m_HoldCyclesRemaining = 0;

        // Branching
        bool m_PendingBranch = false;
        UINT64 m_PendingBranchTrigger = 0; // Cycle number when branch should be taken
        INT32 m_PendingBranchTarget = 0;   // Address to branch to

        // Dram state
        bool m_ReadError = false;

        // Helper methods
        EC HandleHoldAndStop();
        EC HandleColumnOperation(UINT32 command, UINT32 bank, UINT32 adr, bool cke);
        EC HandleRowOperation(UINT32 command, UINT32 bank, UINT32 adr, bool cke);
        EC HandleDramOperations();
        EC HandleBranches();
        EC HandleLoads();
        EC HandleIncrements();
        EC UpdatePC();
    };
}

#endif // INCLUDED_MUCCGALIT1SIM_H
