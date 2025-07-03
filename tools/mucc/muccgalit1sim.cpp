/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mucc.h"
#include "muccgalit1sim.h"
#include "muccgalit1.h"
#include "ampere/ga100/hwproject.h"
using namespace LwDiagUtils;

namespace Mucc
{
    //----------------------------------------------------------------
    //! \brief Allocate a Galit1SimThread object; used by Launcher::AllocProgram()
    //!
    unique_ptr<SimulatedThread> Launcher::AllocGalit1SimulatedThread(const Thread& thread) const
    {
        constexpr UINT32 numRegs  = STMT_INCR_MASK__SIZE_1;
        constexpr UINT32 regWidth = 15;
        SimulatedRegFile regFile(numRegs, regWidth);
        SimulatedMemory memory {};
        return make_unique<Galit1SimThread>(thread, move(regFile), move(memory));
    }

    //----------------------------------------------------------------
    //! \brief Detect and simulate HOLD and STOP
    //!
    EC Galit1SimThread::HandleHoldAndStop()
    {
        EC ec = EC::OK;

        // Detect value for HOLD
        const INT32 holdValue = GetInstructionBits(HI_LO_2(m_pProgram->GetHoldCounterBits()));
        if (holdValue != 0 && !m_Holding)
        {
            m_Holding = true;
            m_HoldCyclesRemaining = holdValue;
        }

        // STOP
        if (GetInstructionBits(HI_LO_2(m_pProgram->GetStopExelwtionBits())))
        {
            m_Terminate = true;
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Increment registers specified in the increment mask of an instruction
    //!
    EC Galit1SimThread::HandleIncrements()
    {
        EC ec = EC::OK;

        for (int i = 0; i < STMT_INCR_MASK__SIZE_1; ++i)
        {
            const INT32 incrementRegister = GetInstructionBits(HI_LO(STMT_INCR_MASK(i)));
            if (incrementRegister)
            {
                CHECK_EC(m_RegFile.Increment(i));
            }
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Simulate one column operation on simulated dram
    //!
    EC Galit1SimThread::HandleColumnOperation(UINT32 command, UINT32 bank, UINT32 adr, bool cke)
    {
        EC ec = EC::OK;

        // Note: the following values are not present in the switch statement, because
        // they alias to another command value (causing the compiler to complain about
        // duplicate case statements).  If one of these values ever obtain unique vaules,
        // add them to the switch statement so that simulation logic can be easily added.
        //
        // STMT_PHASE_CMD_CPD aliases to STMT_PHASE_CMD_CNOP

        switch (command)
        {
        // Read
        case STMT_PHASE_CMD_RD:
        case STMT_PHASE_CMD_RDA:
        {
            const UINT32 patramReg = GetInstructionBits(HI_LO(STMT_P02_RD_REG));
            UINT32 patramEntry;
            m_RegFile.Read(patramReg, &patramEntry);

            LWDASSERT(patramEntry < m_Patram.size());
            const Patram& patram = m_Patram[patramEntry];

            m_Memory.Read(bank, adr, patram, &m_ReadError);
            break;
        }
        // Write
        case STMT_PHASE_CMD_WR:
        case STMT_PHASE_CMD_WRA:
        {
            const UINT32 patramReg = GetInstructionBits(HI_LO(STMT_P02_WR_REG));
            UINT32 patramEntry;
            m_RegFile.Read(patramReg, &patramEntry);

            LWDASSERT(patramEntry < m_Patram.size());
            const Patram& patram = m_Patram[patramEntry];

            m_Memory.Write(bank, adr, SimulatedMemoryEntry(patram));
            break;
        }
        // Lwrrently not simulated
        case STMT_PHASE_CMD_CNOP:
        case STMT_PHASE_CMD_MRS:
            break;

        default:
            LWDASSERT(!"Invalid value for column command");
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Simulate one column operation on simulated dram
    //!
    EC Galit1SimThread::HandleRowOperation(UINT32 command, UINT32 bank, UINT32 adr, bool cke)
    {
        EC ec = EC::OK;

        // Note: the following values are not present in the switch statement, because
        // they alias to another command value (causing the compiler to complain about
        // duplicate case statements).  If one of these values ever obtain unique vaules,
        // add them to the switch statement so that simulation logic can be easily added.
        //
        // STMT_PHASE_CMD_PDE aliases to STMT_PHASE_CMD_RNOP
        // STMT_PHASE_CMD_SRE aliases to STMT_PHASE_CMD_REF
        // STMT_PHASE_CMD_PDX aliases to STMT_PHASE_CMD_RNOP
        // STMT_PHASE_CMD_SRX aliases to STMT_PHASE_CMD_RNOP
        // STMT_PHASE_CMD_RPD aliases to STMT_PHASE_CMD_RNOP

        switch (command)
        {
        // Lwrrently not simulated
        case STMT_PHASE_CMD_RNOP:
        case STMT_PHASE_CMD_PRE:
        case STMT_PHASE_CMD_PREA:
        case STMT_PHASE_CMD_REFSB:
        case STMT_PHASE_CMD_REF:
            break;

        case STMT_PHASE_CMD_ACT:
            m_Memory.Activate(bank, adr);
            break;

        default:
            LWDASSERT(!"Invalid value for row command");
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Simulate all four phases of the dram operations in an instruction
    //!
    EC Galit1SimThread::HandleDramOperations()
    {
        EC ec = EC::OK;

        for (int phase = 0; phase < STMT_PHASE__SIZE_1; ++phase)
        {
            // Get dram command
            UINT32 command = GetInstructionBits(
                    HI_LO_2(m_pProgram->GetPhaseCmdBits(phase, 0)));

            // Get location info
            //
            const UINT32 bankReg = GetInstructionBits(
                    HI_LO_2(m_pProgram->GetPhaseBankRegBits(phase, 0)));
            UINT32 bank;
            m_RegFile.Read(bankReg, &bank);
            //
            const UINT32 adrReg = GetInstructionBits(
                    HI_LO_2(m_pProgram->GetPhaseRowColRegBits(phase, 0)));
            UINT32 adr;
            m_RegFile.Read(adrReg, &adr);

            // Get clock enable info
            const bool cke = GetInstructionBits(
                    HI_LO_2(m_pProgram->GetPhaseCkeBits(phase, 0)));

            if (phase % 2 == 0)
            {
                CHECK_EC(HandleColumnOperation(command, bank, adr, cke));
            }
            else
            {
                CHECK_EC(HandleRowOperation(command, bank, adr, cke));
            }
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Detect and simulate all branch instructions
    //!
    EC Galit1SimThread::HandleBranches()
    {
        EC ec = EC::OK;

        const INT32 branchCtl = GetInstructionBits(HI_LO(STMT_BRANCH_CTL));
        if (branchCtl == 0)
        {
            return ec;
        }

        bool branch = false;

        switch (branchCtl)
        {
            // Unconditional branch
            case STMT_BRANCH_CTL_BRA:
            {
                branch = true;
                break;
            }
            // Branch if not zero
            case STMT_BRANCH_CTL_BNZ:
            {
                INT32 regIndex = GetInstructionBits(HI_LO(STMT_BRANCH_COND_REG));
                UINT32 regValue;
                m_RegFile.Read(regIndex, &regValue);

                if (regValue != 0)
                {
                    branch = true;
                }

                break;
            }
            // Branch if read-data error
            case STMT_BRANCH_CTL_BRDE:
            {
                branch = m_ReadError;
                break;
            }
            default:
            {
                LWDASSERT(!"Unexpected value for branch_ctl in instruction");
            }
        }

        // Only enqueue a branch if a branch isn't already enqueued
        if (branch && !m_PendingBranch)
        {
            // Branches take effect after the next cycle
            const UINT64 branchTrigger = m_LwrrentCycle + 1;
            const INT32 branchTarget = GetInstructionBits(HI_LO(STMT_BRANCH_TGT_ADDR));

            Printf(PriDebug, "Enqueuing branch from PC = %d -> PC = %d\n",
                   m_PC,
                   branchTarget);

            m_PendingBranch = true;
            m_PendingBranchTrigger = branchTrigger;
            m_PendingBranchTarget = branchTarget;
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Handle load instructions
    //!
    EC Galit1SimThread::HandleLoads()
    {
        EC ec = EC::OK;

        INT32 loadEnable = GetInstructionBits(HI_LO(STMT_REG_LOAD_EN));
        if (loadEnable)
        {
            INT32 regNum    = GetInstructionBits(HI_LO(STMT_REG_LOAD_INDEX));
            INT32 value     = GetInstructionBits(HI_LO(STMT_REG_LOAD_VALUE));
            INT32 increment = GetInstructionBits(HI_LO(STMT_REG_LOAD_INC_VALUE));
            INT32 prbs      = GetInstructionBits(HI_LO(STMT_REG_LOAD_PRBS));

            Printf(PriDebug, "Writing {val = %d, inc = %d, prbs = %d} to register %d\n",
                   value, increment, prbs, regNum);

            CHECK_EC(m_RegFile.Write(regNum, value, increment, prbs));
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Write new value to pc, considering pending branches and holds
    //!
    EC Galit1SimThread::UpdatePC()
    {
        EC ec = EC::OK;

        // There are three possible ways PC is updated:
        //
        // 1. Branching to a target instruction
        //
        if (m_PendingBranch &&
            m_PendingBranchTrigger == m_LwrrentCycle)
        {
            Printf(PriDebug, "Taking branch from PC = %d -> PC = %d\n",
                   m_PC,
                   m_PendingBranchTarget);

            // Take the branch
            m_PC = m_PendingBranchTarget;

            // Clear a pending branch
            m_PendingBranch = false;

            // Clear hold state
            m_Holding = false;
            m_HoldCyclesRemaining = 0;
        }
        //
        // 2. Holding on the current instruction
        //
        else if (m_Holding)
        {
            Printf(PriDebug, "Holding at PC = %d for %d cycles\n",
                   m_PC,
                   m_HoldCyclesRemaining);

            if (m_HoldCyclesRemaining == 0)
            {
                ++m_PC;
                m_Holding = false;
            }
            else
            {
                --m_HoldCyclesRemaining;
            }
        }
        //
        // 3. Normal advancement by one instruction
        //
        else
        {
            Printf(PriDebug, "Incrementing from PC = %d -> PC = %d\n",
                   m_PC,
                   m_PC + 1);

            ++m_PC;
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Simulate a single instruction on a collection of state
    //!
    EC Galit1SimThread::SimulateInstruction()
    {
        EC ec = EC::OK;

        // HOLD, STOP, etc
        CHECK_EC(HandleHoldAndStop());

        // Handle dram operations
        CHECK_EC(HandleDramOperations());

        // Places taken branches within a queue to be exelwted next cycle
        CHECK_EC(HandleBranches());

        // Increment each register mentioned in the increment mask
        CHECK_EC(HandleIncrements());

        // Handle loads to a register
        CHECK_EC(HandleLoads());

        // Update program counter
        CHECK_EC(UpdatePC());

        return ec;
    }
}
