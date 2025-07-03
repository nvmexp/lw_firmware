/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009, 2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   mmeigen.cpp
 * @brief  Implemetation of a Method Macro Expander instruction
 *         generator
 *
 */

#include "lwmisc.h"
#include "mmeigen.h"
#include "core/include/utility.h"

// Op code field defines.  Unfortunately a generic header file does not
// exist that can be included to pick up these defines

// Generic op code fields used by multiple instructions
#define  LW_FE_MME_OP              2:0
#define  LW_FE_MME_MUX             6:4
#define  LW_FE_MME_ENDING_NEXT     7:7
#define  LW_FE_MME_DST            10:8
#define  LW_FE_MME_SRC0          13:11
#define  LW_FE_MME_SRC1          16:14

// Bin specific op code fields
#define  LW_FE_MME_SUBOP         21:17

// Immed specific op code fields
#define  LW_FE_MME_IMMED         31:14

// Insert specific op code fields
#define  LW_FE_MME_SRC_BIT       21:17
#define  LW_FE_MME_WIDTH         26:22
#define  LW_FE_MME_DST_BIT       31:27

// Branch specific opcode fields
#define  LW_FE_MME_FLAG_NZ                    4:4
#define  LW_FE_MME_FLAG_NZ_TRUE               0x0
#define  LW_FE_MME_FLAG_NO_DELAY_SLOT         5:5
#define  LW_FE_MME_FLAG_NO_DELAY_SLOT_TRUE    0x0
#define  LW_FE_MME_TARGET                   31:14

// Method fields
#define  LW_FE_MME_METHOD                    11:0
#define  LW_FE_MME_METHOD_INC               17:12

bool MmeInstGenerator::m_bInitialized = false;
map<MmeInstGenerator::OpCode, string> MmeInstGenerator::s_OpNames;
map<MmeInstGenerator::BinSubOp, string> MmeInstGenerator::s_SubOpNames;
map<MmeInstGenerator::MuxOp, string> MmeInstGenerator::s_MuxNames;

//! Static OpCode array used to generate random instructions
const MmeInstGenerator::OpCode MmeInstGenerator::s_OpCodes[] =
{
    OP_BIN,
    OP_ADDI,
    OP_INSERT,
    OP_INSERTSB0,
    OP_INSERTDB0,
    OP_STATEI,
    OP_BRA
};
#define NUM_OPCODES (sizeof(s_OpCodes) / sizeof(UINT32))

//! Static BinSubOp array used to generate random instructions
const MmeInstGenerator::BinSubOp MmeInstGenerator::s_BinSubOps[] =
{
    BOP_ADD,
    BOP_ADDC,
    BOP_SUB,
    BOP_SUBB,
    BOP_XOR,
    BOP_OR,
    BOP_AND,
    BOP_ANDNOT,
    BOP_NAND
};
#define NUM_SUBOPS (sizeof(s_BinSubOps) / sizeof(UINT32))

//! Static MuxOp array used to generate random instructions
const MmeInstGenerator::MuxOp MmeInstGenerator::s_MuxOps[] =
{
    MUX_LNN,
    MUX_RNN,
    MUX_RNR,
    MUX_LRN,
    MUX_RRN,
    MUX_LNR,
    MUX_RLR,
    MUX_RRR
};
#define NUM_MUXOPS (sizeof(s_MuxOps) / sizeof(UINT32))

//! Static MuxOp array (release muxes only) used to generate
//! random instructions
const MmeInstGenerator::MuxOp MmeInstGenerator::s_ReleaseMuxOps[] =
{
    MUX_LRN,
    MUX_RRN,
    MUX_RLR,
    MUX_RRR
};
#define NUM_RELEASE_MUXOPS (sizeof(s_ReleaseMuxOps) / sizeof(UINT32))

//----------------------------------------------------------------------------
//! \brief Constructor
//!
MmeInstGenerator::MmeInstGenerator(FancyPicker::FpContext *pFpCtx)
:   m_pFpCtx(pFpCtx)
{
    // Initialize all the string maps used for disassembly
    if (!m_bInitialized)
    {
        s_OpNames[OP_BIN] = "BIN";
        s_OpNames[OP_ADDI] = "ADDI";
        s_OpNames[OP_INSERT] = "INSERT";
        s_OpNames[OP_INSERTSB0] = "INSERTSB0";
        s_OpNames[OP_INSERTDB0] = "INSERTDB0";
        s_OpNames[OP_STATEI] = "STATEI";
        s_OpNames[OP_BRA] = "BRA";

        s_SubOpNames[BOP_ADD] = "ADD";
        s_SubOpNames[BOP_ADDC] = "ADDC";
        s_SubOpNames[BOP_SUB] = "SUB";
        s_SubOpNames[BOP_SUBB] = "SUBB";
        s_SubOpNames[BOP_XOR] = "XOR";
        s_SubOpNames[BOP_OR] = "OR";
        s_SubOpNames[BOP_AND] = "AND";
        s_SubOpNames[BOP_ANDNOT] = "ANDNOT";
        s_SubOpNames[BOP_NAND] = "NAND";

        s_MuxNames[MUX_LNN] = "LNN";
        s_MuxNames[MUX_RNN] = "RNN";
        s_MuxNames[MUX_RNR] = "RNR";
        s_MuxNames[MUX_LRN] = "LRN";
        s_MuxNames[MUX_RRN] = "RRN";
        s_MuxNames[MUX_LNR] = "LNR";
        s_MuxNames[MUX_RLR] = "RLR";
        s_MuxNames[MUX_RRR] = "RRR";

        m_bInitialized = true;
    }
}

//----------------------------------------------------------------------------
//! \brief Return a ADD instruction (Dst = Src0 + Src1).  Carry will be
//!        updated
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::ADD(const UINT32 Dst, const UINT32 Src0,
                             const UINT32 Src1, const MuxOp Mux,
                             const bool bEndingNext)
{
    return MmeBin(BOP_ADD, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a ADDC instruction (Dst = Src0 + Src1 + Carry).  Carry will
//!        be updated
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::ADDC(const UINT32 Dst, const UINT32 Src0,
                              const UINT32 Src1, const MuxOp Mux,
                              const bool bEndingNext)
{
    return MmeBin(BOP_ADDC, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a SUB instruction (Dst = Src0 + ~Src1 + 1).  Carry will
//!        be updated
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::SUB(const UINT32 Dst, const UINT32 Src0,
                             const UINT32 Src1, const MuxOp Mux,
                             const bool bEndingNext)
{
    return MmeBin(BOP_SUB, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a SUBB instruction (Dst = Src0 + ~Src1 + Carry).  Carry will
//!        be updated
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::SUBB(const UINT32 Dst, const UINT32 Src0,
                              const UINT32 Src1, const MuxOp Mux,
                              const bool bEndingNext)
{
    return MmeBin(BOP_SUBB, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a XOR instruction (Dst = Src0 ^ Src1).  Carry will *not* be
//!        changed
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::XOR(const UINT32 Dst, const UINT32 Src0,
                             const UINT32 Src1, const MuxOp Mux,
                             const bool bEndingNext)
{
    return MmeBin(BOP_XOR, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a OR instruction (Dst = Src0 | Src1).  Carry will *not* be
//!        changed
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::OR(const UINT32 Dst, const UINT32 Src0,
                            const UINT32 Src1, const MuxOp Mux,
                            const bool bEndingNext)
{
    return MmeBin(BOP_OR, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a AND instruction (Dst = Src0 & Src1).  Carry will *not* be
//!        changed
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::AND(const UINT32 Dst, const UINT32 Src0,
                             const UINT32 Src1, const MuxOp Mux,
                             const bool bEndingNext)
{
    return MmeBin(BOP_AND, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a ANDNOT instruction (Dst = Src0 & ~Src1).  Carry will *not*
//!        be changed
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::ANDNOT(const UINT32 Dst, const UINT32 Src0,
                                const UINT32 Src1, const MuxOp Mux,
                                const bool bEndingNext)
{
    return MmeBin(BOP_ANDNOT, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a NAND instruction (Dst = ~(Src0 & Src1).  Carry will *not*
//!        be changed
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::NAND(const UINT32 Dst, const UINT32 Src0,
                              const UINT32 Src1, const MuxOp Mux,
                              const bool bEndingNext)
{
    return MmeBin(BOP_NAND, Dst, Src0, Src1, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a NOP instruction.  Carry will *not* be changed
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::NOP(const bool bEndingNext)
{
    return MmeImmed(OP_ADDI, 0, 0, 0, MUX_RNN, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a ADDI (Dst = Src0 + sign_extend(Immed)) instruction.
//!        Carry will *not* be changed
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Immed       : Immediate value
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::ADDI(const UINT32 Dst, const UINT32 Src0,
                              const UINT32 Immed, const MuxOp Mux,
                              const bool bEndingNext)
{
    return MmeImmed(OP_ADDI, Dst, Src0, Immed, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a STATEI instruction.  Carry will *not* be changed
//!           addr = Src0 + sign_extend(Immed)
//!           Dst = shadow[addr]
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Immed       : Immediate value
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::STATEI(const UINT32 Dst, const UINT32 Src0,
                                const UINT32 Immed, const bool bEndingNext)
{
    return MmeImmed(OP_STATEI, Dst, Src0, Immed, MUX_RNN, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a INSERT instruction.  Carry will *not* be changed
//!    Temp = Src0
//!    Temp[DestBit+Width-1 : DestBit] = Src1[SourceBit+Width-1 : SourceBit]
//!    Dst = Temp
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param DestBit     : Destination bit for the bit copy
//! \param SourceBit   : Source bit for the bit copy
//! \param Width       : Number of bits to copy
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::INSERT(const UINT32 Dst, const UINT32 Src0,
                                const UINT32 Src1, const UINT32 DestBit,
                                const UINT32 SourceBit, const UINT32 Width,
                                const MuxOp Mux, const bool bEndingNext)
{
    return MmeInsert(OP_INSERT, Dst, Src0, Src1, DestBit, SourceBit, Width, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a INSERTSB0 instruction.  Carry will *not* be changed
//!    Temp = 0
//!    Temp[DestBit+Width-1 : DestBit] = Src1[Src0+Width-1 : Src0]
//!    Dst = Temp
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param DestBit     : Destination bit for the bit copy
//! \param Width       : Number of bits to copy
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::INSERTSB0(const UINT32 Dst, const UINT32 Src0,
                                   const UINT32 Src1, const UINT32 DestBit,
                                   const UINT32 Width, const MuxOp Mux,
                                   const bool bEndingNext)
{
    return MmeInsert(OP_INSERTSB0, Dst, Src0, Src1, DestBit, 0, Width, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a INSERTDB0 instruction.  Carry will *not* be changed
//!    Temp = 0
//!    Temp[Src0+Width-1 : Src0] = Src1[SourceBit+Width-1 : SourceBit]
//!    Dst = Temp
//!
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param SourceBit   : Source bit for the bit copy
//! \param Width       : Number of bits to copy
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::INSERTDB0(const UINT32 Dst, const UINT32 Src0,
                                   const UINT32 Src1, const UINT32 SourceBit,
                                   const UINT32 Width, const MuxOp Mux,
                                   const bool bEndingNext)
{
    return MmeInsert(OP_INSERTDB0, Dst, Src0, Src1, 0, SourceBit, Width, Mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Return a BRA instruction.  Carry will *not* be changed
//!     cond = bNotZero ? (src0 != 0) : (src0 == 0)
//!
//!     if (!cond && bEndingNext)
//!        prepareToEnd
//!
//!     if (!bNoDelaySlot)
//!        exelwteDelaySlot
//!
//!     if (cond)
//!        pc = pc + sign_extend(Target)
//!
//! \param Src0         : Source register 0
//! \param Target       : Offset to branch from current PC (in instructions)
//! \param bNotZero     : True if the branch is taken when Src0 is not zero
//! \param bNoDelaySlot : True if there is *no* delay slot in the branch
//! \param bEndingNext  : True if this is the next to last instruction in a
//!                       macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::BRA(const UINT32 Src0, const UINT32 Target,
                             const bool bNotZero, const bool bNoDelaySlot,
                             const bool bEndingNext)
{
    return DRF_NUM(_FE, _MME, _OP, OP_BRA) |
           DRF_NUM(_FE, _MME, _FLAG_NZ, bNotZero) |
           DRF_NUM(_FE, _MME, _FLAG_NO_DELAY_SLOT, bNoDelaySlot) |
           DRF_NUM(_FE, _MME, _ENDING_NEXT, bEndingNext) |
           DRF_NUM(_FE, _MME, _SRC0, Src0) |
           DRF_NUM(_FE, _MME, _TARGET, Target);
}

//----------------------------------------------------------------------------
//! \brief Return the OpCode for a specific instruction
//!
//! \param Instruction  : Instruction to extract the OpCode from
//!
//! \return The OpCode for the specified instruction
//!
MmeInstGenerator::OpCode MmeInstGenerator::GetOpCode(const UINT32 Instruction)
{
    OpCode op = (OpCode)DRF_VAL(_FE, _MME, _OP, Instruction);
    MASSERT(s_OpNames.count(op));
    return op;
}

//----------------------------------------------------------------------------
//! \brief Return the BinSubOp for a specific instruction
//!
//! \param Instruction  : Instruction to extract the BinSubOp from
//!
//! \return The BinSubOp for the specified instruction
//!
MmeInstGenerator::BinSubOp MmeInstGenerator::GetBinSubOp(const UINT32 Instruction)
{
    BinSubOp binSubOp = (BinSubOp)DRF_VAL(_FE, _MME, _SUBOP, Instruction);
    MASSERT(GetOpCode(Instruction) == OP_BIN);
    MASSERT(s_SubOpNames.count(binSubOp));
    return binSubOp;
}

//----------------------------------------------------------------------------
//! \brief Return the MuxOp for a specific instruction
//!
//! \param Instruction  : Instruction to extract the MuxOp from
//!
//! \return The MuxOp for the specified instruction
//!
MmeInstGenerator::MuxOp MmeInstGenerator::GetMux(const UINT32 Instruction)
{
    MuxOp muxOp = (MuxOp)DRF_VAL(_FE, _MME, _MUX, Instruction);
    MASSERT(GetOpCode(Instruction) != OP_BRA);
    MASSERT(s_MuxNames.count(muxOp));
    return muxOp;
}

//----------------------------------------------------------------------------
//! \brief Return the Src0 register for a specific instruction
//!
//! \param Instruction  : Instruction to extract the Src0 register from
//!
//! \return The Src0 register for the specified instruction
//!
UINT32 MmeInstGenerator::GetSrc0Reg(const UINT32 Instruction)
{
    UINT32 reg = DRF_VAL(_FE, _MME, _SRC0, Instruction);
    MASSERT(reg < GetRegCount());
    return reg;
}

//----------------------------------------------------------------------------
//! \brief Return the Src1 register for a specific instruction
//!
//! \param Instruction  : Instruction to extract the Src1 register from
//!
//! \return The Src1 register for the specified instruction
//!
UINT32 MmeInstGenerator::GetSrc1Reg(const UINT32 Instruction)
{
    UINT32 reg = DRF_VAL(_FE, _MME, _SRC1, Instruction);
    MASSERT_DECL(OpCode op = GetOpCode(Instruction));

    // Remove compiler warning in release builds and when compile with Clang
    //op = OpCode(op + 0);

    MASSERT((op == OP_BIN)       || (op == OP_INSERT) ||
            (op == OP_INSERTSB0) || (op == OP_INSERTDB0));
    MASSERT(reg < GetRegCount());
    return reg;
}

//----------------------------------------------------------------------------
//! \brief Return the Dst register for a specific instruction
//!
//! \param Instruction  : Instruction to extract the Dst register from
//!
//! \return The Dst register for the specified instruction
//!
UINT32 MmeInstGenerator::GetDstReg(const UINT32 Instruction)
{
    UINT32 reg = DRF_VAL(_FE, _MME, _DST, Instruction);
    MASSERT(GetOpCode(Instruction) != OP_BRA);
    MASSERT(reg < GetRegCount());
    return reg;
}

//----------------------------------------------------------------------------
//! \brief Return the immediate value for a specific instruction
//!
//! \param Instruction  : Instruction to extract the immediate value from
//!
//! \return The immediate value for the specified instruction
//!
INT32 MmeInstGenerator::GetImmed(const UINT32 Instruction)
{
    // Make sure the immediate value is sign extended
    INT32 immed = (INT32)Instruction >> DRF_BASE(LW_FE_MME_IMMED);
    //OpCode op = GetOpCode(Instruction);
    MASSERT_DECL(OpCode op = GetOpCode(Instruction));

    // Remove compiler warning in release builds and when compile with Clang
   // op = OpCode(op + 0);

    MASSERT((op == OP_ADDI) || (op == OP_STATEI));
    return immed;
}

//----------------------------------------------------------------------------
//! \brief Return the source bit for a specific instruction
//!
//! \param Instruction  : Instruction to extract the source bit from
//!
//! \return The source bit for the specified instruction
//!
UINT32 MmeInstGenerator::GetSrcBit(const UINT32 Instruction)
{
    UINT32 srcBit = DRF_VAL(_FE, _MME, _SRC_BIT, Instruction);
    //OpCode op = GetOpCode(Instruction);
    MASSERT_DECL(OpCode op = GetOpCode(Instruction));
    // Remove compiler warning in release builds and when compile with Clang
   // op = OpCode(op + 0);

    MASSERT((op == OP_INSERT) || (op == OP_INSERTSB0) || (op == OP_INSERTDB0));
    return srcBit;
}

//----------------------------------------------------------------------------
//! \brief Return the destination bit for a specific instruction
//!
//! \param Instruction  : Instruction to extract the destination bit from
//!
//! \return The destination bit for the specified instruction
//!
UINT32 MmeInstGenerator::GetDstBit(const UINT32 Instruction)
{
    UINT32 destBit = DRF_VAL(_FE, _MME, _DST_BIT, Instruction);
    //OpCode op = GetOpCode(Instruction);
    MASSERT_DECL(OpCode op = GetOpCode(Instruction));
    // Remove compiler warning in release builds and when compile with Clang
    //op = OpCode(op + 0);

    MASSERT((op == OP_INSERT) || (op == OP_INSERTSB0) || (op == OP_INSERTDB0));
    return destBit;
}

//----------------------------------------------------------------------------
//! \brief Return the width for a specific instruction
//!
//! \param Instruction  : Instruction to extract the width from
//!
//! \return The width for the specified instruction
//!
UINT32 MmeInstGenerator::GetWidth(const UINT32 Instruction)
{
    UINT32 width = DRF_VAL(_FE, _MME, _WIDTH, Instruction);
    //OpCode op = GetOpCode(Instruction);
    MASSERT_DECL(OpCode op = GetOpCode(Instruction));
    // Remove compiler warning in release builds and when compile with Clang
    //op = OpCode(op + 0);

    MASSERT((op == OP_INSERT) || (op == OP_INSERTSB0) || (op == OP_INSERTDB0));
    return width;
}

//----------------------------------------------------------------------------
//! \brief Return the no delay slot flag for a branch instruction
//!
//! \param Instruction  : Instruction to extract the no delay slot flag from
//!
//! \return The no delay slot flag for the specified instruction
//!
bool MmeInstGenerator::GetBranchNoDelaySlot(const UINT32 Instruction)
{
    bool bNoDelaySlot = (DRF_VAL(_FE, _MME,
                                 _FLAG_NO_DELAY_SLOT, Instruction) != 0);
    MASSERT(GetOpCode(Instruction) == OP_BRA);
    return bNoDelaySlot;
}

//----------------------------------------------------------------------------
//! \brief Return the not zero flag for a branch instruction
//!
//! \param Instruction  : Instruction to extract the not zero flag from
//!
//! \return The not zero flag for the specified instruction
//!
bool MmeInstGenerator::GetBranchNotZero(const UINT32 Instruction)
{
    bool bNoDelaySlot = (DRF_VAL(_FE, _MME, _FLAG_NZ, Instruction) != 0);
    MASSERT(GetOpCode(Instruction) == OP_BRA);
    return bNoDelaySlot;
}

//----------------------------------------------------------------------------
//! \brief Return the branch target for a branch instruction
//!
//! \param Instruction  : Instruction to extract the branch target from
//!
//! \return The branch target for the specified instruction
//!
INT32 MmeInstGenerator::GetBranchTarget(const UINT32 Instruction)
{
    INT32 target = (INT32)Instruction >> DRF_BASE(LW_FE_MME_TARGET);
    MASSERT(GetOpCode(Instruction) == OP_BRA);
    return target;
}

//----------------------------------------------------------------------------
//! \brief Return the ending next flag for a instruction
//!
//! \param Instruction  : Instruction to extract the ending next flag from
//!
//! \return The ending next flag for the specified instruction
//!
bool MmeInstGenerator::GetEndingNext(const UINT32 Instruction)
{
    return (DRF_VAL(_FE, _MME, _ENDING_NEXT, Instruction) != 0);
}

//----------------------------------------------------------------------------
//! \brief Extract the method from a result data
//!
//! \param Data  : Data to extract the method from from
//!
//! \return The method for the specified data
//!
UINT32 MmeInstGenerator::GetMethod(const UINT32 Data)
{
    return DRF_VAL(_FE, _MME, _METHOD, Data);
}

//----------------------------------------------------------------------------
//! \brief Extract the method increment from a result data
//!
//! \param Data  : Data to extract the method increment from from
//!
//! \return The method increment for the specified data
//!
UINT32 MmeInstGenerator::GetMethodInc(const UINT32 Data)
{
    return DRF_VAL(_FE, _MME, _METHOD_INC, Data);
}

//----------------------------------------------------------------------------
//! \brief Return whether the instruction is a load instruction
//!
//! \param Instruction  : Instruction to check for load on
//!
//! \return true if the instruction is a load instruction, false otherwise
//!
bool MmeInstGenerator::IsLoadInstruction(const UINT32 Instruction)
{
    MuxOp mux = (MuxOp)DRF_VAL(_FE, _MME, _MUX, Instruction);
    MASSERT(s_MuxNames.count(mux));
    return ((mux == MUX_LNN) || (mux == MUX_LRN) ||
            (mux == MUX_LNR) || (mux == MUX_RLR));
}

//----------------------------------------------------------------------------
//! \brief Return whether the instruction is a release instruction
//!
//! \param Instruction  : Instruction to check for release on
//!
//! \return true if the instruction is a release instruction, false otherwise
//!
bool MmeInstGenerator::IsReleaseInstruction(const UINT32 Instruction)
{
    MuxOp mux = (MuxOp)DRF_VAL(_FE, _MME, _MUX, Instruction);
    MASSERT(s_MuxNames.count(mux));
    return ((mux == MUX_LRN) || (mux == MUX_RRN) ||
            (mux == MUX_RRR) || (mux == MUX_RLR));
}

//----------------------------------------------------------------------------
//! \brief Clear the EndingNext bit of an instruction
//!
//! \param Instruction  : Instruction to clear the ending next bit on
//!
//! \return Instruction with the ending next bit cleared
//!
UINT32 MmeInstGenerator::ClearEndingNext(const UINT32 Instruction)
{
    return FLD_SET_DRF_NUM(_FE, _MME, _ENDING_NEXT, 0, Instruction);
}

//----------------------------------------------------------------------------
//! \brief Set the EndingNext bit of an instruction
//!
//! \param Instruction  : Instruction to set the ending next bit on
//!
//! \return Instruction with the ending next bit set
//!
UINT32 MmeInstGenerator::SetEndingNext(const UINT32 Instruction)
{
    return FLD_SET_DRF_NUM(_FE, _MME, _ENDING_NEXT, 1, Instruction);
}

//----------------------------------------------------------------------------
//! \brief Get the maximum value for an immediate field of an instruction
//!
//! \return The maximum value for an immediate field of an instruction
//!
UINT32 MmeInstGenerator::GetMaxImmedValue()
{
    return DRF_MASK(LW_FE_MME_IMMED);
}

//----------------------------------------------------------------------------
//! \brief Get the maximum value for an source bit field of an instruction
//!
//! \return The maximum value for a source bit field of an instruction
//!
UINT32 MmeInstGenerator::GetMaxSrcBitValue()
{
    return DRF_MASK(LW_FE_MME_SRC_BIT);
}

//----------------------------------------------------------------------------
//! \brief Generate a random instruction
//!
//! \param bReleaseOnly    : true to generate only instructions that release
//!                          method/data pairs
//! \param bAllowBranches  : true to allow randomized branch instructions
//! \param ValidOutputRegs : Set of valid registers that may be used for output
//! \param bEndingNext     : true if this is the next to last instruction in a
//!                          macro
//!
//! \return Binary representation of the instruction
//!
UINT32 MmeInstGenerator::GenRandInstruction(const bool bReleaseOnly,
                                            const bool bAllowBranches,
                                            const set<UINT32> &ValidOutputRegs,
                                            const bool bEndingNext)
{
    OpCode op        = s_OpCodes[m_pFpCtx->Rand.GetRandom(0, NUM_OPCODES - 1)];
    BinSubOp subOp   = BOP_ADD;
    UINT32 src0      = 0;
    UINT32 src1      = 0;
    UINT32 dst       = 0;
    MuxOp  mux       = MUX_RNN;
    UINT32 immed     = 0;
    UINT32 destBit   = 0;
    UINT32 sourceBit = 0;
    UINT32 width     = 0;

    // Change branch instructions to binary instructions if branches are
    // disallowed, if release only and the op is one which cannot generate
    // a release, chage the operation to binary as well
    if ((!bAllowBranches && (op == OP_BRA)) ||
        (bReleaseOnly && ((op == OP_BRA) || (op == OP_STATEI))))
    {
        op = OP_BIN;
    }

    // Src0 register used in all instructions
    src0 = m_pFpCtx->Rand.GetRandom(0, GetRegCount() - 1);

    // Generate the branch instruction data, note that no validation is done
    // for an appropriate target range, this must be done at a higher level
    if (op == OP_BRA)
    {
        UINT32 target   = m_pFpCtx->Rand.GetRandom(0,
                                                   DRF_MASK(LW_FE_MME_TARGET));
        bool   bNotZero = (m_pFpCtx->Rand.GetRandom(0, 1) != 0);
        bool   bNoDelaySlot = (m_pFpCtx->Rand.GetRandom(0, 1) != 0);
        return BRA(src0, target, bNotZero, bNoDelaySlot, bEndingNext);
    }

    // Choose a destination register from the list of valid output registers
    UINT32 dstRegIndex = m_pFpCtx->Rand.GetRandom(0,
                                           (UINT32)ValidOutputRegs.size() - 1);
    set<UINT32>::const_iterator pDstReg = ValidOutputRegs.begin();
    while (dstRegIndex)
    {
        pDstReg++;
        dstRegIndex--;
    }
    dst  = *pDstReg;

    // Choose a mux operation appropriately
    if (bReleaseOnly)
    {
        mux = s_ReleaseMuxOps[m_pFpCtx->Rand.GetRandom(0,
                                                   NUM_RELEASE_MUXOPS - 1)];
    }
    else if (op == OP_STATEI)
    {
        mux = MUX_RNN;
    }
    else
    {
        mux = s_MuxOps[m_pFpCtx->Rand.GetRandom(0, NUM_MUXOPS - 1)];
    }

    // Pick the source 1 register if necessary
    if ((op == OP_BIN)       || (op == OP_INSERT) ||
        (op == OP_INSERTSB0) || (op == OP_INSERTDB0))
    {
        //used in BIN, INSERTs.
        src1 = m_pFpCtx->Rand.GetRandom(0, GetRegCount() - 1);
    }

    // Pick the the sub operation for binary operation
    if (op == OP_BIN)
    {
        subOp = s_BinSubOps[m_pFpCtx->Rand.GetRandom(0, NUM_SUBOPS - 1)];
    }

    // Pick the the immediate value for required operations
    if ((op == OP_ADDI) || (op == OP_STATEI))
    {
        immed = m_pFpCtx->Rand.GetRandom(0, DRF_MASK(LW_FE_MME_IMMED));
    }

    // Pick the the additional values for inserts
    if ((op == OP_INSERT) ||
        (op == OP_INSERTSB0) ||
        (op == OP_INSERTDB0))
    {
        //used in INSERTs
        destBit   = m_pFpCtx->Rand.GetRandom(0, DRF_MASK(LW_FE_MME_DST_BIT));
        sourceBit = m_pFpCtx->Rand.GetRandom(0, DRF_MASK(LW_FE_MME_SRC_BIT));
        width     = m_pFpCtx->Rand.GetRandom(0, DRF_MASK(LW_FE_MME_WIDTH));
    }

    if (op == OP_BIN)
    {
        return MmeBin(subOp, dst, src0, src1, mux, bEndingNext);
    }
    else if ((op == OP_ADDI) || (op == OP_STATEI))
    {
        return MmeImmed(op, dst, src0, immed, mux, bEndingNext);
    }

    // Otherwise the instruction must be an insert
    return MmeInsert(op, dst, src0, src1, destBit, sourceBit,
                     width, mux, bEndingNext);
}

//----------------------------------------------------------------------------
//! \brief Disassemble the instruction into a string representation
//!
//! \param Instruction    : Instruction to disassemble
//!
//! \return String representation of the instruction
//!
string MmeInstGenerator::Disassemble(UINT32 Instruction)
{
    string instructionString = "";
    OpCode op = GetOpCode(Instruction);
    MuxOp  muxOp = (MuxOp)DRF_VAL(_FE, _MME, _MUX, Instruction);
    BinSubOp binSubOp = (BinSubOp)DRF_VAL(_FE, _MME, _SUBOP, Instruction);

    if (!s_OpNames.count(op))
    {
        MASSERT(!"Unknown op code");
        return "***UNKNOWN INSTRUCTION***";
    }
    if ((op != OP_BRA) && !s_MuxNames.count(muxOp))
    {
        MASSERT(!"Unknown Mux");
        return "***UNKNOWN INSTRUCTION***";
    }
    if ((op == OP_BIN) && !s_SubOpNames.count(binSubOp))
    {
        MASSERT(!"Unknown Binary Sub Operation");
        return "***UNKNOWN INSTRUCTION***";
    }

    switch (DRF_VAL(_FE, _MME, _OP, Instruction))
    {
        case OP_BIN:
            instructionString = Utility::StrPrintf("%s r%d, r%d, r%d, %s",
                                        s_SubOpNames[binSubOp].c_str(),
                                        DRF_VAL(_FE, _MME, _DST, Instruction),
                                        DRF_VAL(_FE, _MME, _SRC0, Instruction),
                                        DRF_VAL(_FE, _MME, _SRC1, Instruction),
                                        s_MuxNames[muxOp].c_str());
            break;
        case OP_ADDI:
        case OP_STATEI:
            instructionString = Utility::StrPrintf("%s r%d, r%d, 0x%05x, %s",
                                       s_OpNames[op].c_str(),
                                       DRF_VAL(_FE, _MME, _DST, Instruction),
                                       DRF_VAL(_FE, _MME, _SRC0, Instruction),
                                       DRF_VAL(_FE, _MME, _IMMED, Instruction),
                                       s_MuxNames[muxOp].c_str());
            break;
        case OP_INSERT:
        case OP_INSERTSB0:
        case OP_INSERTDB0:
            instructionString = Utility::StrPrintf(
                                     "%s r%d, r%d, r%d, %d, %d, %d, %s",
                                     s_OpNames[op].c_str(),
                                     DRF_VAL(_FE, _MME, _DST, Instruction),
                                     DRF_VAL(_FE, _MME, _SRC0, Instruction),
                                     DRF_VAL(_FE, _MME, _SRC1, Instruction),
                                     DRF_VAL(_FE, _MME, _DST_BIT, Instruction),
                                     DRF_VAL(_FE, _MME, _SRC_BIT, Instruction),
                                     DRF_VAL(_FE, _MME, _WIDTH, Instruction),
                                     s_MuxNames[muxOp].c_str());
            break;
        case OP_BRA:
            instructionString = Utility::StrPrintf("%s r%d, %d, %d, %d",
                         s_OpNames[op].c_str(),
                         DRF_VAL(_FE, _MME, _SRC0, Instruction),
                         DRF_VAL(_FE, _MME, _TARGET, Instruction),
                         DRF_VAL(_FE, _MME, _FLAG_NZ, Instruction),
                         DRF_VAL(_FE, _MME, _FLAG_NO_DELAY_SLOT, Instruction));
            break;
        default:
            MASSERT(!"Unknown op code");
            return "***UNKNOWN INSTRUCTION***";
    }

    if (DRF_VAL(_FE, _MME, _ENDING_NEXT, Instruction))
    {
        instructionString += Utility::StrPrintf("  (EN)");
    }

    return instructionString;
}

//----------------------------------------------------------------------------
//! \brief Private function to create a binary instruction
//!
//! \param SubOp       : Binary sub operation
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return String representation of the instruction
//!
UINT32 MmeInstGenerator::MmeBin(const BinSubOp SubOp, const UINT32 Dst,
                                const UINT32 Src0, const UINT32 Src1,
                                const MuxOp Mux, const bool   bEndingNext)
{
    return DRF_NUM(_FE, _MME, _OP, OP_BIN) |
           DRF_NUM(_FE, _MME, _MUX, Mux) |
           DRF_NUM(_FE, _MME, _ENDING_NEXT, bEndingNext) |
           DRF_NUM(_FE, _MME, _DST, Dst) |
           DRF_NUM(_FE, _MME, _SRC0, Src0) |
           DRF_NUM(_FE, _MME, _SRC1, Src1) |
           DRF_NUM(_FE, _MME, _SUBOP, SubOp);
}

//----------------------------------------------------------------------------
//! \brief Private function to create an instruction using an immediate value
//!
//! \param Op          : Operation
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Immed       : Immediate value
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return String representation of the instruction
//!
UINT32 MmeInstGenerator::MmeImmed(const OpCode Op, const UINT32 Dst,
                                  const UINT32 Src0, const UINT32 Immed,
                                  const MuxOp Mux, const bool bEndingNext)
{
    return DRF_NUM(_FE, _MME, _OP, Op) |
           DRF_NUM(_FE, _MME, _MUX, Mux) |
           DRF_NUM(_FE, _MME, _ENDING_NEXT, bEndingNext) |
           DRF_NUM(_FE, _MME, _DST, Dst) |
           DRF_NUM(_FE, _MME, _SRC0, Src0) |
           DRF_NUM(_FE, _MME, _IMMED, Immed);
}

//----------------------------------------------------------------------------
//! \brief Private function to create an insert instruction
//!
//! \param Op          : Operation
//! \param Dst         : Destination register
//! \param Src0        : Source register 0
//! \param Src1        : Source register 1
//! \param DestBit     : Destination bit for the insert
//! \param SourceBit   : Source bit for the insert
//! \param Width       : Number of bits to insert
//! \param Mux         : Mux value
//! \param bEndingNext : true if this is the next to last instruction in a
//!                      macro
//!
//! \return String representation of the instruction
//!
UINT32 MmeInstGenerator::MmeInsert(const OpCode Op, const UINT32 Dst,
                                   const UINT32 Src0, const UINT32 Src1,
                                   const UINT32 DestBit,
                                   const UINT32 SourceBit,
                                   const UINT32 Width, const MuxOp Mux,
                                   const bool bEndingNext)
{
    MASSERT(DestBit <= 32);
    MASSERT(SourceBit <= 32);
    MASSERT(Width <= 32);

    return DRF_NUM(_FE, _MME, _OP, Op) |
           DRF_NUM(_FE, _MME, _MUX, Mux) |
           DRF_NUM(_FE, _MME, _ENDING_NEXT, bEndingNext) |
           DRF_NUM(_FE, _MME, _DST, Dst) |
           DRF_NUM(_FE, _MME, _SRC0, Src0) |
           DRF_NUM(_FE, _MME, _SRC1, Src1) |
           DRF_NUM(_FE, _MME, _SRC_BIT, SourceBit) |
           DRF_NUM(_FE, _MME, _WIDTH, Width) |
           DRF_NUM(_FE, _MME, _DST_BIT, DestBit);
}

