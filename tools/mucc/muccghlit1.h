/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once
#ifndef INCLUDED_MUCCGHLIT1_H
#define INCLUDED_MUCCGHLIT1_H

#include "muccgalit1.h"

namespace Mucc
{
    //----------------------------------------------------------------
    //! \brief Subclass of Mucc::Program for assembling for GHLIT1
    //!
    class Ghlit1Pgm: public Galit1Pgm
    {
    public:
        explicit Ghlit1Pgm(const Launcher& launcher) : Galit1Pgm(launcher) {}
        void     SetDefaultInstruction(const Thread& thread,
                                       Bits* pBits) const override;
        unsigned GetInstructionSize() const override { return 256; }
        unsigned GetMaxFbpas() const override;

        pair<int, int> GetPhaseCmdBits(int phase, int channel) const override
        {
            const int base = (17 * phase) + (71 * channel);
            const int highBit = 98 + base;
            const int lowBit = 92 + base;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetPhaseRowColRegBits(int phase, int channel) const override
        {
            const int base = (17 * phase) + (71 * channel);
            const int highBit = 102 + base;
            const int lowBit = 99 + base;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetPhaseBankRegBits(int phase, int channel) const override
        {
            const int base = (17 * phase) + (71 * channel);
            const int highBit = 106 + base;
            const int lowBit = 103 + base;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetPhaseCkeBits(int phase, int channel) const override
        {
            const int base = (17 * phase) + (71 * channel);
            const int highBit = 107 + base;

            return std::make_pair(highBit, highBit);
        }

        pair<int, int> GetPhaseRfmBits(int phase, int channel) const
        {
            const int base = (17 * phase) + (71 * channel);
            const int highBit = 108 + base;

            return std::make_pair(highBit, highBit);
        }

        int GetUsePatramDBIBit() const override
        {
            return 91;
        }

        pair<int, int> GetCalBits(int pc, int channel) const
        {
            const int base = pc + (71 * channel);
            const int highBit = 160 + base;
         
            return std::make_pair(highBit, highBit);
        }

        pair<int, int> GetHoldCounterBits() const override
        {
            const int lowBit = 75;
            const int highBit = 88;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetIlaTriggerBits() const override
        {
            const int lowBit = 89;
            const int highBit = 89;
            
            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetStopExelwtionBits() const override
        {
            const int lowBit = 90;
            const int highBit = 90;

            return std::make_pair(highBit, lowBit);
        }

    protected:
        EC EatChannel(Tokenizer* pTokens, int* pChannel) const override;
        EC EatPhaseCmd
        (   
            const Token &token,
            int &cmd,
            unsigned &numArgs,
            bool &isColOp,
            bool &isRowOp,
            bool &isWrite,
            bool &powerDown,
            bool &isRFMSet
        ) const override;
        EC UpdateRFMBit
        (
            const Token &token,
            int phase,
            int channel,
            bool isRFMSet
        ) override;
    };
}

#endif // INCLUDED_MUCCGHLIT1_H
