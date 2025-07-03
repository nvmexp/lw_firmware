/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MMEIGEN_H
#define INCLUDED_MMEIGEN_H

#ifndef INCLUDED_FPICKER_H
#include "core/include/fpicker.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_SET
#include <set>
#define INCLUDED_STL_SET
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

//--------------------------------------------------------------------
//! \brief Provides an interface for generating MME (Method Macro
//!        Expander) instructions
//!
//! https://fermi/p4/hw/doc/gpu/fermi/Ilwestigation_phase/FE/MME_instruction_set.doc
//!
class MmeInstGenerator
{
public:
    //! This enum describes the different OP codes available to the MME
    enum OpCode
    {
        OP_BIN       = 0,  //!< Binary op (SubOp contains real op)
        OP_ADDI      = 1,  //!< Add immediate
        OP_INSERT    = 2,  //!< Insert specified bits from Src1 register
                           //!< into Src0 register at the specified bit and
                           //!< store the result in Dst
        OP_INSERTSB0 = 3,  //!< Extract the specified bits from Src1
                           //!< register using Src0 register as the
                           //!< starting bit to extract from Src1, storing
                           //!< the result in Dst
        OP_INSERTDB0 = 4,  //!< Extract the specified bits from Src1
                           //!< register using Src0 register as the
                           //!< destination bit to extract into, storing
                           //!< the result in Dst
        OP_STATEI    = 5,  //!< Extract a value from shadow RAM using
                           //!< Src0 register + immediate value as the
                           //!< address to extract
        OP_BRA       = 7,  //!< Branch instruction
    };

    //! This enum describes the different SubOp codes available for the OP_BIN
    //! instruction
    enum BinSubOp
    {
        BOP_ADD    = 0,
        BOP_ADDC   = 1,
        BOP_SUB    = 2,
        BOP_SUBB   = 3,
        BOP_XOR    = 8,
        BOP_OR     = 9,
        BOP_AND    = 10,
        BOP_ANDNOT = 11,
        BOP_NAND   = 12
    };

    //! This enum describes the different MUX Ops available to the MME
    //! Mux Ops control whether data is loaded for an instruction (i.e.
    //! requires a CALL_MME_DATA method) or whether data is generated
    //! when the instrction completes (method/data pair output by the
    //! MME to the rest of the frontend)
    //!
    //! The semantics are L = Load, R = Result, N = None.  Load means
    //! that data is loaded from the macro data input.  Result meand
    //! that the result of the operation is used.  There are three
    //! relevant fields: Dst which is the destination of the operation,
    //! Release which when not None will cause a method/data pair to
    //! be released, and Method which constrols which value is loaded
    //! into the method register
    enum MuxOp
    {
        MUX_LNN,  //!< Dst : Load, Release : None, Method : None
        MUX_RNN,  //!< Dst : Result, Release : None, Method : None
        MUX_RNR,  //!< Dst : Result, Release : None, Method : Result
        MUX_LRN,  //!< Dst : Load, Release : Result, Method : None
        MUX_RRN,  //!< Dst : Result, Release : Result, Method : None
        MUX_LNR,  //!< Dst : Load, Release : None, Method : Result
        MUX_RLR,  //!< Dst : Result, Release : Load, Method : Result
        MUX_RRR,  //!< Dst : Result, Release : Result[17:12], Method : Result
    };

    MmeInstGenerator(FancyPicker::FpContext *pFpCtx);

    // Binary operations
    UINT32 ADD(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
               const MuxOp Mux, const bool bEndingNext);
    UINT32 ADDC(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
                const MuxOp Mux, const bool bEndingNext);
    UINT32 SUB(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
               const MuxOp Mux, const bool bEndingNext);
    UINT32 SUBB(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
                const MuxOp Mux, bool const bEndingNext);
    UINT32 XOR(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
               const MuxOp Mux, const bool bEndingNext);
    UINT32 OR(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
              const MuxOp Mux, const bool bEndingNext);
    UINT32 AND(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
               const MuxOp Mux, const bool bEndingNext);
    UINT32 ANDNOT(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
                  const MuxOp  Mux, const bool bEndingNext);
    UINT32 NAND(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
                const MuxOp  Mux, const bool bEndingNext);
    UINT32 NOP(const bool bEndingNext);

    // Other operations
    UINT32 ADDI(const UINT32 Dst, const UINT32 Src0, const UINT32 Immed,
                const MuxOp Mux, const bool bEndingNext);
    UINT32 STATEI(const UINT32 Dst, const UINT32 Src0, const UINT32 Immed,
                  const bool bEndingNext);
    UINT32 INSERT(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
                  const UINT32 DestBit, const UINT32 SourceBit,
                  const UINT32 Width, const MuxOp Mux, const bool bEndingNext);
    UINT32 INSERTSB0(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
                     const UINT32 DestBit, const UINT32 Width, const MuxOp Mux,
                     const bool bEndingNext);
    UINT32 INSERTDB0(const UINT32 Dst, const UINT32 Src0, const UINT32 Src1,
                     const UINT32 SourceBit, const UINT32 Width,
                     const MuxOp Mux, const bool bEndingNext);
    UINT32 BRA(const UINT32 Src0, const UINT32 Target, const bool bNotZero,
               const bool bNoDelaySlot, const bool bEndingNext);

    // Instruction field queries
    OpCode GetOpCode(const UINT32 Instruction);
    BinSubOp GetBinSubOp(const UINT32 Instruction);
    MuxOp  GetMux(const UINT32 Instruction);
    UINT32 GetSrc0Reg(const UINT32 Instruction);
    UINT32 GetSrc1Reg(const UINT32 Instruction);
    UINT32 GetDstReg(const UINT32 Instruction);
    INT32  GetImmed(const UINT32 Instruction);
    UINT32 GetSrcBit(const UINT32 Instruction);
    UINT32 GetDstBit(const UINT32 Instruction);
    UINT32 GetWidth(const UINT32 Instruction);
    bool   GetBranchNoDelaySlot(const UINT32 Instruction);
    bool   GetBranchNotZero(const UINT32 Instruction);
    INT32  GetBranchTarget(const UINT32 Instruction);
    bool   GetEndingNext(const UINT32 Instruction);

    UINT32 GetMethod(const UINT32 Data);
    UINT32 GetMethodInc(const UINT32 Data);

    bool IsLoadInstruction(const UINT32 Instruction);
    bool IsReleaseInstruction(const UINT32 Instruction);

    // Instruction modifiers
    UINT32 ClearEndingNext(const UINT32 Instruction);
    UINT32 SetEndingNext(const UINT32 Instruction);

    // Generic instruction range queries
    UINT32 GetRegCount() { return 8; }
    UINT32 GetMaxImmedValue();
    UINT32 GetMaxSrcBitValue();

    // Miscellaneous functions
    UINT32 GenRandInstruction(const bool bReleaseOnly,
                              const bool bAllowBranches,
                              const set<UINT32> &ValidOutputRegs,
                              const bool bEndingNext);
    string Disassemble(const UINT32 Instruction);
private:
    // Helper functions for instruction creation
    UINT32 MmeBin(const BinSubOp SubOp, const UINT32 Dst, const UINT32 Src0,
                  const UINT32 Src1, const MuxOp Mux, const bool bEndingNext);
    UINT32 MmeImmed(const OpCode Op, const UINT32 Dst, const UINT32 Src0,
                    const UINT32 Immed, const MuxOp Mux,
                    const bool bEndingNext);
    UINT32 MmeInsert(const OpCode Op, const UINT32 Dst, const UINT32 Src0,
                     const UINT32 Src1, const UINT32 DestBit,
                     const UINT32 SourceBit, const UINT32 Width,
                     const MuxOp Mux, const bool bEndingNext);

    //! Fancy Picker context for generating random instructions
    FancyPicker::FpContext *m_pFpCtx;

    // Static data

    //! Indicator for whether static data was initialized
    static bool  m_bInitialized;

    //! Operation code to string mapping
    static map<OpCode, string> s_OpNames;

    //! Binary sub-operation code to string mapping
    static map<BinSubOp, string> s_SubOpNames;

    //! Mux operation code to string mapping
    static map<MuxOp, string> s_MuxNames;

    //! Op code array, used for generating random instructions
    static const OpCode s_OpCodes[];

    //! Binary sub-operation array, used for generating random instructions
    static const BinSubOp s_BinSubOps[];

    //! Mux operation array, used for generating random instructions
    static const MuxOp s_MuxOps[];

    //! Release mux operation array (contains only mux operations that will
    //! generate a method release), used for generating random instructions
    static const MuxOp s_ReleaseMuxOps[];
};

#endif
