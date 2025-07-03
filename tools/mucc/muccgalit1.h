/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once
#ifndef INCLUDED_MUCCGALIT1_H
#define INCLUDED_MUCCGALIT1_H

#include "muccpgm.h"
#include "lwmisc.h"
#include <set>

namespace Mucc
{
    //----------------------------------------------------------------
    //! \brief Subclass of Mucc::Program for assembling for GALIT1
    //!
    class Galit1Pgm: public Program
    {
    public:
        Galit1Pgm(const Launcher& launcher) : Program(launcher) {}
        EC       Assemble(vector<char>&& input) override;
        void     SetDefaultInstruction(const Thread& thread,
                                       Bits* pBits) const override;
        unsigned GetInstructionSize() const override { return 192; }
        unsigned GetMaxFbpas() const override;
        void AddStopAndNop(Thread* thread) override;

        pair<int, int> GetPhaseCmdBits(int phase, int channel) const override
        {
            const int base = (16 * phase);
            const int highBit = 93 + base;
            const int lowBit = 87 + base;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetPhaseRowColRegBits(int phase, int channel) const override
        {
            const int base = 16 * phase;
            const int highBit = 97 + base;
            const int lowBit = 94 + base;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetPhaseBankRegBits(int phase, int channel) const override
        {
            const int base = 16 * phase;
            const int highBit = 101 + base;
            const int lowBit = 98 + base;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetPhaseCkeBits(int phase, int channel) const override
        {
            const int base = 16 * phase;
            const int highBit = 102 + base;

            return std::make_pair(highBit, highBit);
        }

        int GetUsePatramDBIBit() const override
        {
            return 151;
        }

        pair<int, int> GetCalBits(int pc, int channel) const override
        {
            const int base = pc + (71 * channel);
            const int highBit = 152 + base;

            return std::make_pair(highBit, highBit);
        }

        pair<int, int> GetHoldCounterBits() const override
        {
            const int lowBit = 75;
            const int highBit = 84;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetIlaTriggerBits() const override
        {
            const int lowBit = 85;
            const int highBit = 85;

            return std::make_pair(highBit, lowBit);
        }

        pair<int, int> GetStopExelwtionBits() const override
        {
            const int lowBit = 86;
            const int highBit = 86;

            return std::make_pair(highBit, lowBit);
        }

    protected:
        virtual EC EatChannel(Tokenizer* pTokens, int* pChannel) const;
        virtual EC EatPhaseCmd
        (   
            const Token &token,
            int &cmd, 
            unsigned &numArgs, 
            bool &isColOp,
            bool &isRowOp,
            bool &isWrite,
            bool &powerDown,
            bool &isRFMSet
        ) const;
        virtual EC UpdateRFMBit
        (
            const Token &token,
            int phase,
            int channel,
            bool isRFMSet
        );

    private:
        EC EatStatement(Tokenizer* pTokens);
        EC EatPhaseStatement(Tokenizer* pTokens);
        EC EatChannelStatement(Tokenizer* pTokens);
        EC EatHackStatement(Tokenizer* pTokens);
        EC EatPatram(Tokenizer* pTokens);
        EC EatSymbol(Tokenizer* pTokens);
        EC EatDirective(Tokenizer* pTokens);
        EC EatLabel(Tokenizer* pTokens);
        EC EatRegister(Tokenizer* pTokens, unsigned hiBit, unsigned loBit,
                       bool allowSharing = false,
                       const char* conflictMsg = nullptr);
        EC EatNewline(Tokenizer* pTokens);
        EC CheckForLoadAndIncr();
        static bool IsRegister(const Token& token);
    };

    // Reserved words for GALIT1 assembly files
    //
    // Note: This enum may be defined differently in post-GALIT1
    // litters.  It's defined in the .h file because the next litter
    // is likely to be similar enough to GALIT1 that it can re-use
    // many of the symbols.
    //
    enum ReservedWord : unsigned
    {
        // Registers
        R0,
        R1,
        R2,
        R3,
        R4,
        R5,
        R6,
        R7,
        R8,
        R9,
        R10,
        R11,
        R12,
        R13,
        R14,
        R15,

        // Phases
        P0,
        P1,
        P2,
        P3,

        // Channels (for GHLIT1)
        C0,
        C1,

        // Opcodes
        BRA,
        BNZ,
        BRDE,

        LOAD,
        LDI,
        LDPRBS,
        INCR,

        CNOP,
        RD,
        RDA,
        WR,
        WRA,
        MRS,
        CPD,

        RNOP,
        ACT,
        PRE,
        PREA,
        REFSB,
        REF,

        // 2 GHLIT1 instructions
        RFMAB,
        RFMPB,

        PDE,
        SRE,
        PDX,
        SRX,
        RPD,

        HOLD,
        TILA,
        STOP,
        PDBI,
        CAL,
        NOP,
        HACK,

        PATRAM
    };

    // Symbols for GALIT1 assembly files
    //
    enum Symbol : unsigned
    {
        // Math symbols
        //
        LEFT_PAREN  = Expression::LEFT_PAREN,
        RIGHT_PAREN = Expression::RIGHT_PAREN,
        LOGICAL_NOT = Expression::LOGICAL_NOT,
        BITWISE_NOT = Expression::BITWISE_NOT,
        MULTIPLY    = Expression::MULTIPLY,
        DIVIDE      = Expression::DIVIDE,
        MODULO      = Expression::MODULO,
        PLUS        = Expression::PLUS,
        MINUS       = Expression::MINUS,
        LEFT_SHIFT  = Expression::LEFT_SHIFT,
        RIGHT_SHIFT = Expression::RIGHT_SHIFT,
        LOGICAL_LT  = Expression::LOGICAL_LT,
        LOGICAL_LE  = Expression::LOGICAL_LE,
        LOGICAL_GT  = Expression::LOGICAL_GT,
        LOGICAL_GE  = Expression::LOGICAL_GE,
        LOGICAL_EQ  = Expression::LOGICAL_EQ,
        LOGICAL_NE  = Expression::LOGICAL_NE,
        BITWISE_AND = Expression::BITWISE_AND,
        BITWISE_XOR = Expression::BITWISE_XOR,
        BITWISE_OR  = Expression::BITWISE_OR,
        LOGICAL_AND = Expression::LOGICAL_AND,
        LOGICAL_OR  = Expression::LOGICAL_OR,
        QUESTION    = Expression::QUESTION,
        COLON       = Expression::COLON,

        // Other symbols
        //
        DOT = Expression::LAST_OP + 1,
        SEMICOLON,
        NEWLINE
    };
}

// Bitfields used for GALIT1 instruction words
//
// Note: These bitfields may be different in post-GALIT1 litters.
// They're defined in the .h file because the next litter is likely to
// be similar enough to GALIT1 that it can re-use many of the symbols.
//
#define STMT_BRANCH_CTL             1:0
#define STMT_BRANCH_CTL_BRA         0x1
#define STMT_BRANCH_CTL_BNZ         0x2
#define STMT_BRANCH_CTL_BRDE        0x3
#define STMT_BRANCH_COND_REG        5:2
#define STMT_BRANCH_TGT_ADDR        12:6
#define STMT_REG_LOAD_EN            13:13
#define STMT_REG_LOAD_INDEX         17:14
#define STMT_REG_LOAD_VALUE         32:18
#define STMT_REG_LOAD_INC_VALUE     47:33
#define STMT_REG_LOAD_PRBS          48:48
#define STMT_INCR_MASK(i)           (49 + (i)) : (49 + (i))
#define STMT_INCR_MASK__SIZE_1      16
#define STMT_P02_WR_ENABLE          65:65
#define STMT_P02_WR_REG             69:66
#define STMT_P02_RD_ENABLE          70:70
#define STMT_P02_RD_REG             74:71
#define STMT_PHASE__SIZE_1          4  // phase
#define STMT_PHASE__SIZE_2          2  // channel
// TODO bug 3115114: Update these bitfields once we know where arch
// will put the channel-1 bits
#define STMT_PHASE_CMD_CNOP         0x00
#define STMT_PHASE_CMD_RD           0x01
#define STMT_PHASE_CMD_RDA          0x05
#define STMT_PHASE_CMD_WR           0x02
#define STMT_PHASE_CMD_WRA          0x06
#define STMT_PHASE_CMD_MRS          0x48
#define STMT_PHASE_CMD_CPD          0x00
#define STMT_PHASE_CMD_RNOP         0x00
#define STMT_PHASE_CMD_ACT          0x08
#define STMT_PHASE_CMD_PRE          0x0c
#define STMT_PHASE_CMD_PREA         0x54
#define STMT_PHASE_CMD_REFSB        0x64
#define STMT_PHASE_CMD_REF          0x4c
#define STMT_PHASE_CMD_RFMAB        0x4c
#define STMT_PHASE_CMD_RFMPB        0x64
#define STMT_PHASE_CMD_PDE          0x00
#define STMT_PHASE_CMD_SRE          0x4c
#define STMT_PHASE_CMD_PDX          0x00
#define STMT_PHASE_CMD_SRX          0x00
#define STMT_PHASE_CMD_RPD          0x00
#define STMT_PHASE_CKE_PWR_UP       1
#define STMT_PHASE_CKE_PWR_DN       0
#define STMT_PHASE_RFM_SET          1
#define STMT_PHASE_RFM_CLR          0

#define STMT_PATRAM_DQ_SIZE  32
#define STMT_PATRAM_ECC_SIZE 4
#define STMT_PATRAM_DBI_SIZE 4

// Macros to let us pass the above bitfields to functions that take
// hiBit & loBit as separate args.
//
// "HI_LO(hiBit:loBit)" expands to "hiBit, loBit"
// "HI_LO_VAL(BITFIELD, _VALUE)" expands to "HI_LO(BITFIELD), BITFIELD_VALUE"
//
#define HI_LO(bits)           DRF_EXTENT(bits), DRF_BASE(bits)
#define HI_LO_VAL(bits, val)  HI_LO(bits), bits##val
#define HI_LO_2(bits)         bits.first, bits.second

#endif // INCLUDED_MUCCGALIT1_H
