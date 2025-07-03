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
#pragma once
#ifndef INCLUDED_MUCCPGM_H
#define INCLUDED_MUCCPGM_H

#include "mucccore.h"
#include "preproc.h"
#include <boost/dynamic_bitset.hpp>
#include <set>
#include <unordered_set>

// This file contains the Mucc::Program base class, along with the
// classes used to implement Mucc::Program.
//
// A Mucc::Program can assemble a Mucc assembly file, and holds the
// compiled program.  Different litters are likely to assemble
// slightly differently, so there will generally be one subclass of
// Mucc::Program for each litter that assembles programs differently.
//
// The microcontroller is capable of running different code on each
// subpartition, so a Program consists of "Threads".  Each Thread
// contains the code for some mask of FBPAs & subpartitions.  A Thread
// consists of Instructions which represents an instruction word, and
// Patrams which represent a pattern-ram entry.  An Instruction
// consists of Statements, each of which represents one statement like
// "LD R1 0" that sets a handful of bits in the Instruction.
//
namespace Mucc
{
    using namespace LwDiagUtils;
    class Launcher;
    class Thread;
    class Program;
    using ThreadMask = boost::dynamic_bitset<UINT32>;

    //----------------------------------------------------------------
    //! \brief Class which represents a statement with opcode and args
    //!
    //! This class represents one parsed statement such as "LD R0 50",
    //! consisting of an opcode plus some arguments.  It inherits from
    //! Bits, since the primary purpose of this class is to represent
    //! the bits that it sets in the surrounding instruction word, but
    //! it also contains an opcode Token with debugging info.
    //!
    //! HACK statements can override all other statements, so there is
    //! a flag identifying them.
    //!
    class Statement : public Bits
    {
    public:
        Statement(unsigned size, const Token& opcode, bool isHack) :
            Bits(size), m_Opcode(opcode), m_IsHack(isHack) {}
        Statement(const Statement&)     = default;
        Statement(Statement&&) noexcept = default;
        const Token& GetOpcode() const { return m_Opcode; }
        bool         IsHack()    const { return m_IsHack; }
    private:
        Token m_Opcode;  //!< The opcode in the statement
        bool  m_IsHack;  //!< True if this is a HACK statment
    };

    //----------------------------------------------------------------
    //! \brief Class which represents an instruction word
    //!
    //! In the source code, an instruction consists of a series of
    //! statements terminated in a semicolon.  This class contains
    //! all Statements that went into this instruction.
    //!
    class Instruction
    {
    public:
        explicit Instruction(Thread* pThread);
        Instruction(const Instruction&)     = default;
        Instruction(Instruction&&) noexcept = default;
        void AddStatement(const Token& opcode, bool isHack);
        EC   UpdateStatement(const Token& token, unsigned hiBit,
                             unsigned loBit, INT32 value,
                             bool allowSharing, const char* conflictMsg);
        void ResolveExpression(unsigned index, unsigned hiBit,
                               unsigned loBit, INT32 value);
        EC   Close(const Token& semicolon, ReservedWord nop);

        unsigned    GetNumStatements() const;
        Bits        GetBits()          const;
        const Bits& GetNonHackBits()   const { return m_AllNonHacks; }
        const vector<Statement>& GetStatements() const { return m_Statements; }

    private:
        Program*           m_pProgram;      //!< Program that this is part of
        vector<Statement>  m_Statements;    //!< All Statements in instruction
        Bits               m_AllNonHacks;   //!< Combined bits from non-HACKs
        Bits               m_AllHacks;      //!< Combined bits from HACK stmts
    };

    //----------------------------------------------------------------
    //! \brief Class which represents a patram (pattern-ram) entry
    //!
    //! Each patram entry contains DQ, ECC, and DBI bits.  In GA100,
    //! an entry contains 256 DQ bits, 32 ECC bits, and 32 DBI bits.
    //!
    class Patram
    {
    public:
        explicit Patram(Program* pProgram);
        Patram(const Patram&)     = default;
        Patram(Patram&&) noexcept = default;
        void AppendOpcode(const Token& opcode) { m_Opcodes.push_back(opcode); }
        void AppendDq(unsigned numBits, INT32 value);
        void AppendEcc(unsigned numBits, INT32 value);
        void AppendDbi(unsigned numBits, INT32 value);
        void UpdateDq(unsigned hiBit, unsigned loBit, INT32 value);
        void UpdateEcc(unsigned hiBit, unsigned loBit, INT32 value);
        void UpdateDbi(unsigned hiBit, unsigned loBit, INT32 value);
        const vector<Token>& GetOpcodes() const { return m_Opcodes;  }
        const Bits&          GetDq()      const { return m_Dq;  }
        const Bits&          GetEcc()     const { return m_Ecc; }
        const Bits&          GetDbi()     const { return m_Dbi; }
        EC Close(const Token& semicolon);
    private:
        Program*      m_pProgram;
        vector<Token> m_Opcodes;
        Bits          m_Dq;
        Bits          m_Ecc;
        Bits          m_Dbi;
    };

    //----------------------------------------------------------------
    //! \brief Base class for holding an arithmetic expression
    //!
    //! This base class stores and evaluates an arithmetic expression
    //! such as "2*(5+startAddress)".  Most of the work is done in
    //! subclasses such as UnaryExpression and BinaryExpression.
    //!
    class Expression
    {
    public:
        virtual ~Expression() {}
        EC Evaluate(Thread* pThread, INT64* pValue) const;
        EC EvaluateConst(Program* pProgram, INT64* pValue) const;
        EC CheckLimits(Program* pProgram, INT64 value,
                       INT64 milwalue, INT64 maxValue) const;
        virtual const Token& GetFirstToken() const = 0;
        virtual const Token& GetLastToken() const = 0;

        // Each Program subclass (e.g.  Galit1Program) should use the
        // following values in its Symbol enum.
        // Each subclass is free to define its own Symbol enum, but
        // this convention ensures that they all use the same enum
        // values for the arithmetic operators that we need to use at
        // this base-class level.
        //
        enum Operator: unsigned
        {
            LEFT_PAREN,
            RIGHT_PAREN,
            LOGICAL_NOT,
            BITWISE_NOT,
            MULTIPLY,
            DIVIDE,
            MODULO,
            PLUS,
            MINUS,
            LEFT_SHIFT,
            RIGHT_SHIFT,
            LOGICAL_LT,
            LOGICAL_LE,
            LOGICAL_GT,
            LOGICAL_GE,
            LOGICAL_EQ,
            LOGICAL_NE,
            BITWISE_AND,
            BITWISE_XOR,
            BITWISE_OR,
            LOGICAL_AND,
            LOGICAL_OR,
            QUESTION,
            COLON,

            ILLEGAL,
            FIRST_OP = LEFT_PAREN,
            LAST_OP  = ILLEGAL
        };
        static Operator TokenToOperator(const Token& token);

    protected:
        virtual EC EvaluateImpl(Program* pProgram, const Thread* pThread,
                                INT64* pValue) const = 0;
        static EC EvaluateArg(const unique_ptr<const Expression>& pArg,
                              Program* pProgram, const Thread* pThread,
                              INT64* pValue);
    };

    //----------------------------------------------------------------
    //! \brief Subclass for an expression consisting of a single Token
    //!
    //! This Expression contains a single INTEGER or IDENTIFIER
    //! (label) token
    //!
    class TokenExpression: public Expression
    {
    public:
        explicit TokenExpression(const Token& token): m_Token(token) {}
        const Token& GetFirstToken() const override { return m_Token; }
        const Token& GetLastToken() const override { return m_Token; }
    protected:
        EC EvaluateImpl(Program* pProgram, const Thread* pThread,
                       INT64* pValue) const override;
    private:
        Token m_Token;
    };

    //----------------------------------------------------------------
    //! \brief Subclass for a unary expression
    //!
    class UnaryExpression: public Expression
    {
    public:
        UnaryExpression(const Token& op, unique_ptr<const Expression> pArg):
            m_Op(op), m_pArg(move(pArg)) {}
        const Token& GetFirstToken() const override { return m_Op; }
        const Token& GetLastToken() const override { return m_pArg->GetLastToken(); }
    protected:
        EC EvaluateImpl(Program* pProgram, const Thread* pThread,
                        INT64* pValue) const override;
    private:
        Token m_Op;
        unique_ptr<const Expression> m_pArg;
    };

    //----------------------------------------------------------------
    //! \brief Subclass for a binary expression
    //!
    class BinaryExpression: public Expression
    {
    public:
        BinaryExpression(unique_ptr<const Expression> pArg1, const Token& op,
                         unique_ptr<const Expression> pArg2):
            m_pArg1(move(pArg1)), m_Op(op), m_pArg2(move(pArg2)) {}
        const Token& GetFirstToken() const override { return m_pArg1->GetFirstToken(); }
        const Token& GetLastToken() const override { return m_pArg2->GetLastToken(); }
    protected:
        EC EvaluateImpl(Program* pProgram, const Thread* pThread,
                        INT64* pValue) const override;
    private:
        unique_ptr<const Expression> m_pArg1;
        Token m_Op;
        unique_ptr<const Expression> m_pArg2;
    };

    //----------------------------------------------------------------
    //! \brief Subclass for a ternary expression
    //!
    class TernaryExpression: public Expression
    {
    public:
        TernaryExpression(unique_ptr<const Expression> pArg1,
                          unique_ptr<const Expression> pArg2,
                          unique_ptr<const Expression> pArg3):
            m_pArg1(move(pArg1)), m_pArg2(move(pArg2)), m_pArg3(move(pArg3)) {}
        const Token& GetFirstToken() const override { return m_pArg1->GetFirstToken(); }
        const Token& GetLastToken() const override { return m_pArg3->GetLastToken(); }
    protected:
        EC EvaluateImpl(Program* pProgram, const Thread* pThread,
                        INT64* pValue) const override;
    private:
        unique_ptr<const Expression> m_pArg1;
        unique_ptr<const Expression> m_pArg2;
        unique_ptr<const Expression> m_pArg3;
    };

    //----------------------------------------------------------------
    //! \brief Class that inserts an arithmetic expression into code
    //!
    //! Some statements take a numeric argument: a branch statement
    //! takes the address it will branch to, a register-load statement
    //! takes the value it will load into the register, etc.  This
    //! argument can be an arithmetic expression that might include a
    //! label, in which case we can't evaluate the expression right
    //! away if we haven't parsed the label yet.
    //!
    //! This class holds an expression, and indicates where we should
    //! store the value in the code once we evaluate it.  Once we've
    //! parsed the entire program (including labels), we "resolve" the
    //! CodeExpression by evaluating the expression and storing the
    //! result in the code.
    //!
    class CodeExpression
    {
    public:
        // The Type indicates where we should eventually store the
        // value of this expression
        enum Type
        {
            STATEMENT, // In m_Instructions[index1].m_Statements[index2]
            DQ,        // In m_Patram[index1].m_Dq
            ECC,       // In m_Patram[index1].m_Ecc
            DBI        // In m_Patram[index1].m_Dbi
        };
        CodeExpression(const Expression* pExpression,
                       unsigned index1, unsigned index2,
                       unsigned hiBit, unsigned loBit, Type type);
        EC Resolve(Thread* pThread) const;

    private:
        const Expression* m_pExpression; // Arithmetic expression to evaluate
        unsigned m_Index1; // Index of instruction or patram in thread
        unsigned m_Index2; // Index of statement in instruction
        unsigned m_HiBit;  // High bit in statement, dq, ecc, or dbi
        unsigned m_LoBit;  // Low bit in statement, dq, ecc, or dbi
        Type     m_Type;
    };

    //----------------------------------------------------------------
    //! \brief Code that runs on some subpartitions
    //!
    //! This class represents the thread that runs on some bitmask of
    //! FBPAs & subpartitions.  When assembly starts, we have just one
    //! Thread with a mask of all ones, to indicate that all FBPAs &
    //! subpartitions run the same code (the 95% case).  But if the
    //! code contains .FBPA_SUBP_MASK directives, which indicate that
    //! subsequent lines apply to some subset of FBPAs and/or
    //! subpartitions, then the thread "splits" into threads with
    //! non-overlapping masks that all add up to the original all-ones
    //! mask.  After that point, every statement is appended to each
    //! lwrrently-selected thread.
    //!
    //! Each Thread contains all the data to run a microcontroller
    //! program: the Instructions, Patram entries, the thread-specific
    //! labels, etc.
    //!
    class Thread
    {
    public:
        explicit Thread(Program* pProgram) : m_pProgram(pProgram) {}
        Thread(const Thread&)            = default;
        Thread(Thread&&) noexcept        = default;
        Thread& operator=(const Thread&) = default;
        Thread& operator=(Thread&&)      = default;

        Program*                   GetProgram()     const { return m_pProgram; }
        const vector<Instruction>& GetInstructions() const { return m_Instructions; }
        const vector<Patram>&      GetPatrams()      const { return m_Patrams; }
        using Labels = unordered_map<Token, INT32>;
        const Labels&              GetLabels()       const { return m_Labels; }
        Labels::const_iterator     FindLabel(const string& label) const;

        const ThreadMask& GetThreadMask()       const { return m_ThreadMask; }
        void          SetThreadMask(const ThreadMask& m)  { m_ThreadMask = m; }
        static string ToJson(const ThreadMask& mask);
        Instruction*  GetPendingInstruction();
        Patram*       GetPendingPatram();
        Instruction*  GetInstruction(unsigned ii) { return &m_Instructions[ii]; }
        Patram*       GetPatram(unsigned idx)     { return &m_Patrams[idx]; }
        bool          GetUsePatramDbi()     const { return m_UsePatramDbi; }
        UINT64        GetMaxCycles()        const { return m_MaxCycles; }
        void          SetUsePatramDbi(bool pdbi)  { m_UsePatramDbi = pdbi; }
        void          SetMaxCycles(UINT64 cycles) { m_MaxCycles = cycles; }
        EC            EnsurePendingInstruction(const Token& opcode);
        EC            EnsurePendingPatram(const Token& opcode);
        EC            ClosePendingOperation(const Token& semicolon,
                                            ReservedWord nop);
        EC            AddLabel(const Token& label);
        EC            AddExpression(const Expression* pExpression,
                                    unsigned hiBit, unsigned loBit,
                                    CodeExpression::Type type);
        EC            Close();
        vector<Instruction>& GetInstructions() { return m_Instructions; }

    private:
        // Data that describes the thread once assembly is done
        //
        Program*            m_pProgram;       //!< Program that owns this Thread
        ThreadMask          m_ThreadMask;
        vector<Instruction> m_Instructions;   //!< Code Instructions
        vector<Patram>      m_Patrams;        //!< Patram entries
        Labels              m_Labels;         //!< All resolved labels
        bool                m_UsePatramDbi = true; //!< Default instruction bit
        UINT64              m_MaxCycles = 0;  //!< Max clock cycles to run pgm

        // Data that is used during assembly, and ignored afterwards
        //
        enum PendingOperation
        {
            NONE,         //!< Nothing pending (start of file or after a ';')
            INSTRUCTION,  //!< We are lwrrently parsing m_Instructions.back()
            PATRAM        //!< We are lwrrently parsing m_Patrams.back()
        };
        PendingOperation       m_PendingOperation = NONE;
        unordered_set<Token>   m_PendingLabels; //!< Labels found while op=NONE
        vector<CodeExpression> m_CodeExpressions; //!< Unresolved expressions
    };

    //----------------------------------------------------------------
    //! \brief Base class for all litter-specific Program subclasses
    //!
    //! A Mucc::Program can assemble a Mucc assembly file, and holds
    //! the compiled program.
    //!
    //! This base class contains generic infrastructure.  All of the
    //! architecture-specific details, such as which opcodes are
    //! available and which bitfields are defined, should go in the
    //! subclass (generally named <litter>Pgm and defined in
    //! mucc<litter>pgm.h
    //!
    class Program
    {
    public:
        // Public API
        //
        explicit Program(const Launcher& launcher);
        virtual ~Program() {}
        void       Initialize();
        EC         Assemble(Preprocessor* pPreprocessor);
        virtual EC Assemble(vector<char>&& input) = 0;
        void       PrintJson(FILE* fh) const;
        EC         Simulate() const;

        // Methods used by the Program components (Thread, Statement,
        // etc) during assembly
        //
        bool         GetFlag(Flag flag) const { return m_Flags.GetFlag(flag); }
        EC           ReportWarning();
        bool         Uniq(const char* file, unsigned line);
        void         ResetUniq()              { m_UniqLog.clear(); }
        virtual void SetDefaultInstruction(const Thread& thread,
                                           Bits* pBits) const = 0;
        vector<Thread>&       GetThreads()       { return m_Threads; }
        const vector<Thread>& GetThreads() const { return m_Threads; }

        // Architectural constants; virtualize as needed
        //
        virtual unsigned GetInstructionSize() const = 0;
        unsigned GetPatramDqBits()     const { return 256; }
        unsigned GetPatramEccBits()    const { return 32;  }
        unsigned GetPatramDbiBits()    const { return 32;  }
        unsigned GetThreadMaskSize()   const { return 2 * GetMaxFbpas(); }
        virtual unsigned GetMaxFbpas() const = 0;
        virtual void AddStopAndNop(Thread* thread) = 0;

        //BitFields related functions
        virtual pair<int, int> GetPhaseCmdBits(int phase, int channel) const = 0;
        virtual pair<int, int> GetPhaseRowColRegBits(int phase, int channel) const = 0;
        virtual pair<int, int> GetPhaseBankRegBits(int phase, int channel) const = 0;
        virtual pair<int, int> GetPhaseCkeBits(int phase, int channel) const = 0;
        virtual int GetUsePatramDBIBit() const = 0;
        virtual pair<int, int> GetCalBits(int pc, int channel) const = 0;
        virtual pair<int, int> GetHoldCounterBits() const = 0;
        virtual pair<int, int> GetIlaTriggerBits() const = 0;
        virtual pair<int, int> GetStopExelwtionBits() const = 0;

    protected:
        void SetSelectedThreads(const ThreadMask& mask);
        const vector<Thread*>& GetSelectedThreads();

        EC EatOpcode(Tokenizer* pTokens, bool isHack);
        EC EatOpcode(Tokenizer* pTokens,
                     unsigned hiBit, unsigned loBit, INT32 value);
        EC EatExpression(Tokenizer* pTokens, unsigned hiBit, unsigned loBit,
                         CodeExpression::Type type = CodeExpression::STATEMENT);
        EC EatExpression(Tokenizer* pTokens,
                         unique_ptr<const Expression>* ppExpression);
        EC ReportUnexpectedInput(Tokenizer* pTokens);
        EC UpdateStatement(const Token& token,
                           unsigned hiBit, unsigned loBit, INT32 value,
                           bool allowSharing = false,
                           const char* conflictMsg = nullptr);

        vector<char> m_Input;       //!< The input code passed to Assemble()

    private:
        const Launcher* m_pLauncher;
        Flags           m_Flags;    //!< Inherited from Launcher
        vector<Thread>  m_Threads;  //!< All threads owned by Program
        ThreadMask      m_SelectedThreadsMask;
        vector<Thread*> m_SelectedThreads; //!< Selected threads from m_Threads
        vector<unique_ptr<const Expression>> m_Expressions;
        bool            m_ReportedWarning = false; //!< Used by ReportWarning()
        map<string, set<int>> m_UniqLog;   //!< Used by Uniq() and ResetUniq()
    };
}

#endif // INCLUDED_MUCCPGM_H
