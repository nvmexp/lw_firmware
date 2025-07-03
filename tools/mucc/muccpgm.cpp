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

#include "mucc.h"
#include "lwmisc.h"
using namespace LwDiagUtils;

namespace Mucc
{
    //----------------------------------------------------------------
    //! \brief Construct an empty Instruction
    //!
    Instruction::Instruction(Thread* pThread) :
        m_pProgram(pThread->GetProgram()),
        m_AllNonHacks(pThread->GetProgram()->GetInstructionSize()),
        m_AllHacks(pThread->GetProgram()->GetInstructionSize())
    {
        // The program typically has some instruction bits that are
        // set to a default value, unless overridden by a statement.
        //
        m_pProgram->SetDefaultInstruction(*pThread, &m_AllNonHacks);
    }

    //----------------------------------------------------------------
    //! \brief Add a non-HACK Statement to the instruction
    //!
    //! Add a Statement with the indicated opcode.  The statement is
    //! created without any bits set; use UpdateStatement() to do
    //! that.
    //!
    void Instruction::AddStatement(const Token& opcode, bool isHack)
    {
        m_Statements.emplace_back(m_AllNonHacks.GetSize(), opcode, isHack);
    }

    //----------------------------------------------------------------
    //! \brief Update the bits in the most recently-added statement
    //!
    //! Update the bits in the most recent Statement created by
    //! AddStatement().
    //!
    //! \param token The Token in the statement that caused these bits
    //!     to be set; typically the opcode or one of the args.  Used
    //!     for error messages.
    //! \param hiBit The high bit number being set
    //! \param loBit The low bit number being set
    //! \param value The value to store in the hiBit:loBit bitfield
    //! \param allowSharing If false, then it is an error to set any
    //!     bits that were already set by a previous UpdateStatement().
    //!     If true, then it is legal as long as the bits are getting
    //!     set to the same value as before.
    //! \param If there is a conflict with a previous UpdateStatement
    //!     (see allowSharing arg), then print this message.  If this
    //!     arg is nullptr, then print a default message instead.
    //!
    EC Instruction::UpdateStatement
    (
        const Token& token,
        unsigned     hiBit,
        unsigned     loBit,
        INT32        value,
        bool         allowSharing,
        const char*  conflictMsg
    )
    {
        LWDASSERT(!m_Statements.empty());
        const bool isHack   = m_Statements.back().IsHack();
        Bits*      pAllBits = isHack ? &m_AllHacks : &m_AllNonHacks;
        static const char* defaultConflictMsg =
            "Overriding a previous statement (missing semicolon?)";

        // Check for conflict with a previous UpdateStatement()
        //
        if (pAllBits->AnyBitSet(hiBit, loBit) &&
            !(allowSharing && pAllBits->Equals(hiBit, loBit, value)))
        {
            for (const Statement& statement: m_Statements)
            {
                if (statement.AnyBitSet(hiBit, loBit) &&
                    statement.IsHack() == isHack &&
                    !(allowSharing && statement.Equals(hiBit, loBit, value)))
                {
                    const Token& oldOpcode = statement.GetOpcode();
                    Printf(PriError, "%s: %s:\n",
                           token.GetFileLine().c_str(),
                           conflictMsg ? conflictMsg : defaultConflictMsg);
                    Printf(PriError, "%s\n",
                           ToString(token.GetLine()).c_str());
                    Printf(PriError, "%s: Previous statement was here:\n",
                           oldOpcode.GetFileLine().c_str());
                    Printf(PriError, "%s\n",
                           ToString(oldOpcode.GetLine()).c_str());
                    Printf(PriError, "\n");
                    return ILWALID_FILE_FORMAT;
                }
            }
        }

        // Update the bits
        //
        m_Statements.back().SetBits(hiBit, loBit, value);
        pAllBits->SetBits(hiBit, loBit, value);
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Update the bits in a statement that were set by a CodeExpression
    //!
    void Instruction::ResolveExpression
    (
        unsigned index,
        unsigned hiBit,
        unsigned loBit,
        INT32 value
    )
    {
        LWDASSERT(index < m_Statements.size());
        LWDASSERT(hiBit < m_Statements[index].GetSize());

        m_Statements[index].SetBits(hiBit, loBit, value);
        if (m_Statements[index].IsHack())
        {
            m_AllHacks.SetBits(hiBit, loBit, value);
        }
        else
        {
            m_AllNonHacks.SetBits(hiBit, loBit, value);
        }
    }

    //----------------------------------------------------------------
    //! \brief Close an Instruction when we parse a semicolon
    //!
    //! \param semicolon The semicolon token that caused this
    //!     statement to be closed
    //! \param nop The ReservedWord enum corresponding to the NOP
    //!     opcode.  (The ReservedWord enum can be defined differently
    //!     in each Program subclass, which is why this value is
    //!     passed as an arg.)
    //!
    EC Instruction::Close(const Token& semicolon, ReservedWord nop)
    {
        EC ec = OK;

        // Print a warning if this instruction has a NOP mixed with
        // other statements, and the WARN_NOP flag was enabled.
        //
        if (m_pProgram->GetFlag(Flag::WARN_NOP) && m_Statements.size() > 1)
        {
            for (const Statement& statement: m_Statements)
            {
                const Token& opcode = statement.GetOpcode();
                if (opcode.GetReservedWord() == nop)
                {
                    if (m_pProgram->Uniq(__FILE__, __LINE__))
                    {
                        Printf(PriWarn,
                               "%s: Mixing NOP with other statements"
                               " (missing semicolon?):\n",
                               semicolon.GetFileLine().c_str());
                        opcode.PrintUnderlined(PriWarn);
                        CHECK_EC(m_pProgram->ReportWarning());
                    }
                }
            }
        }
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Return the number of Statements in the Instruction
    //!
    unsigned Instruction::GetNumStatements() const
    {
        return static_cast<unsigned>(m_Statements.size());
    }

    //----------------------------------------------------------------
    //! \brief Return the bits from all Statements combined
    //!
    Bits Instruction::GetBits() const
    {
        Bits output = m_AllNonHacks;
        output.SetBits(m_AllHacks);
        return output;
    }

    //----------------------------------------------------------------
    //! \brief Create an empty Patram entry
    //!
    Patram::Patram(Program* pProgram) :
        m_pProgram(pProgram)
    {
        // Reserve the number of expected bits
        //
        m_Dq.Reserve(m_pProgram->GetPatramDqBits());
        m_Ecc.Reserve(m_pProgram->GetPatramEccBits());
        m_Dbi.Reserve(m_pProgram->GetPatramDbiBits());
    }

    //----------------------------------------------------------------
    //! \brief Append DQ bits to the Patram entry
    //!
    void Patram::AppendDq(unsigned numBits, INT32 value)
    {
        m_Dq.AppendBits(numBits, value);
    }

    //----------------------------------------------------------------
    //! \brief Append ECC bits to the Patram entry
    //!
    void Patram::AppendEcc(unsigned numBits, INT32 value)
    {
        m_Ecc.AppendBits(numBits, value);
    }

    //----------------------------------------------------------------
    //! \brief Append DBI bits to the Patram entry
    //!
    void Patram::AppendDbi(unsigned numBits, INT32 value)
    {
        m_Dbi.AppendBits(numBits, value);
    }

    //----------------------------------------------------------------
    //! \brief Update DQ bits to the Patram entry
    //!
    void Patram::UpdateDq(unsigned hiBit, unsigned loBit, INT32 value)
    {
        LWDASSERT(hiBit < m_Dq.GetSize());
        m_Dq.SetBits(hiBit, loBit, value);
    }

    //----------------------------------------------------------------
    //! \brief Update ECC bits to the Patram entry
    //!
    void Patram::UpdateEcc(unsigned hiBit, unsigned loBit, INT32 value)
    {
        LWDASSERT(hiBit < m_Ecc.GetSize());
        m_Ecc.SetBits(hiBit, loBit, value);
    }

    //----------------------------------------------------------------
    //! \brief Update DBI bits to the Patram entry
    //!
    void Patram::UpdateDbi(unsigned hiBit, unsigned loBit, INT32 value)
    {
        LWDASSERT(hiBit < m_Dbi.GetSize());
        m_Dbi.SetBits(hiBit, loBit, value);
    }

    //----------------------------------------------------------------
    //! \brief Close a Patram entry when we parse a semicolon
    //!
    //! \param semicolon The semicolon token that caused this patram
    //!     entry to be closed
    //!
    EC Patram::Close(const Token& semicolon)
    {
        // Check whether DQ, ECC, or DBI has the wrong number of bits
        //
        const char* badType  = nullptr;
        unsigned    actual   = 0;
        unsigned    expected = 0;

        if (m_Dq.GetSize() != m_pProgram->GetPatramDqBits())
        {
            badType  = "DQ";
            actual   = m_Dq.GetSize();
            expected = m_pProgram->GetPatramDqBits();
        }
        else if (m_Ecc.GetSize() != m_pProgram->GetPatramEccBits())
        {
            badType  = "ECC";
            actual   = m_Ecc.GetSize();
            expected = m_pProgram->GetPatramEccBits();
        }
        else if (m_Dbi.GetSize() != m_pProgram->GetPatramDbiBits())
        {
            badType  = "DBI";
            actual   = m_Dbi.GetSize();
            expected = m_pProgram->GetPatramDbiBits();
        }

        // If DQ, ECC, or DBI has the wrong number of bits, then print
        // an error message and return ILWALID_FILE_FORMAT
        //
        if (badType)
        {
            if (m_pProgram->Uniq(__FILE__, __LINE__))
            {
                Printf(PriError,
                       "%s: Expected %u bits of %s data in patram entry,"
                       " but got %u\n",
                       semicolon.GetFileLine().c_str(),
                       expected, badType, actual);
                Printf(PriError, "\n");
            }
            return ILWALID_FILE_FORMAT;
        }
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Find the value of the expression & store it in *pValue
    //!
    EC Expression::Evaluate(Thread* pThread, INT64* pValue) const
    {
        EC ec = OK;
        CHECK_EC(EvaluateImpl(pThread->GetProgram(), pThread, pValue));
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Find the value of the expression & store it in *pValue
    //!
    //! The expression must be "Const" in that it must not contain any
    //! identifiers (labels) that may be thread-dependant.  It must
    //! consist of literals and operators such as "2+2" that can be
    //! evaluated without any external info.
    //!
    EC Expression::EvaluateConst(Program* pProgram, INT64* pValue) const
    {
        EC ec = OK;
        CHECK_EC(EvaluateImpl(pProgram, nullptr, pValue));
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Check a value to ensure milwalue <= value <= maxValue
    //!
    //! This should be called after Evaluate() or EvaluateConst() to
    //! make sure the evaluated value falls within the indicated
    //! range.  Print an error and return non-OK if it does not.
    //!
    EC Expression::CheckLimits
    (
        Program* pProgram,
        INT64    value,
        INT64    milwalue,
        INT64    maxValue
    ) const
    {
        if (value < milwalue || value > maxValue)
        {
            if (pProgram->Uniq(__FILE__, __LINE__))
            {
                const Token& firstToken = GetFirstToken();
                const Token& lastToken  = GetLastToken();
                Printf(PriError,
                    "%s: Illegal value %lld; should be between %lld & %lld:\n",
                    firstToken.GetFileLine().c_str(),
                    value, milwalue, maxValue);
                firstToken.PrintUnderlined(PriError, lastToken);
                Printf(PriError, "\n");
            }
            return ILWALID_FILE_FORMAT;
        }
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Colwert a Token to an Operator
    //!
    //! If this Token represents an Operator such as "+", then return
    //! the operator.  Otherwise, return ILLEGAL.
    //!
    //! This method assumes that the Tokenizer was configured to use
    //! the Operator enums as Symbol values.
    //!
    Expression::Operator Expression::TokenToOperator(const Token& token)
    {
        if (token.GetType() != Token::SYMBOL)
        {
            return Expression::ILLEGAL;
        }
        Operator op = static_cast<Operator>(token.GetSymbol());
        return (op >= Expression::LEFT_PAREN && op < Expression::ILLEGAL
                ? op : Expression::ILLEGAL);
    }

    //----------------------------------------------------------------
    //! \brief Thin wrapper around EvaluateImpl
    //!
    //! Allow subclasses of Expression to call EvaluateImpl() on other
    //! Expression objects, without making EvaluateImpl() public.
    //!
    EC Expression::EvaluateArg
    (
        const unique_ptr<const Expression>& pArg,
        Program* pProgram,
        const Thread* pThread,
        INT64* pValue
    )
    {
        return pArg->EvaluateImpl(pProgram, pThread, pValue);
    }

    //----------------------------------------------------------------
    //! \brief Find the value of the expression & store it in *pValue
    //!
    EC TokenExpression::EvaluateImpl
    (
        Program*      pProgram,
        const Thread* pThread,  // May be nullptr for EvaluateConst()
        INT64*        pValue
    ) const
    {
        LWDASSERT (pProgram != nullptr && pValue != nullptr);
        switch (m_Token.GetType())
        {
            case Token::INTEGER:
                *pValue = m_Token.GetInteger();
                break;
            case Token::IDENTIFIER:
            {
                if (pThread == nullptr)
                {
                    if (pProgram->Uniq(__FILE__, __LINE__))
                    {
                        Printf(PriError,
                            "%s: Labels are not allowed in this expression:\n",
                            m_Token.GetFileLine().c_str());
                        m_Token.PrintUnderlined(PriError);
                        Printf(PriError, "\n");
                    }
                    return ILWALID_FILE_FORMAT;
                }

                const auto& labels = pThread->GetLabels();
                const auto  iter   = labels.find(m_Token);
                if (iter == labels.end())
                {
                    if (pProgram->Uniq(__FILE__, __LINE__))
                    {
                        Printf(PriError, "%s: Cannot find label:\n",
                               m_Token.GetFileLine().c_str());
                        m_Token.PrintUnderlined(PriError);
                        Printf(PriError, "\n");
                    }
                    return ILWALID_FILE_FORMAT;
                }
                *pValue = iter->second;
                break;
            }
            default:
                LWDASSERT(!"Unknown token type");
                break;
        }
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Find the value of the expression & store it in *pValue
    //!
    EC UnaryExpression::EvaluateImpl
    (
        Program*      pProgram,
        const Thread* pThread,  // May be nullptr for EvaluateConst()
        INT64*        pValue
    ) const
    {
        LWDASSERT (pProgram != nullptr && pValue != nullptr);
        EC ec = OK;

        INT64 arg;
        CHECK_EC(EvaluateArg(m_pArg, pProgram, pThread, &arg));

        switch (TokenToOperator(m_Op))
        {
            case Expression::PLUS:
                *pValue = arg;
                break;
            case Expression::MINUS:
                *pValue = -arg;
                break;
            case Expression::LOGICAL_NOT:
                *pValue = !arg;
                break;
            case Expression::BITWISE_NOT:
                *pValue = ~arg;
                break;
            default:
                LWDASSERT(!"Unknown unary operator");
                return SOFTWARE_ERROR;
        }
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Find the value of the expression & store it in *pValue
    //!
    EC BinaryExpression::EvaluateImpl
    (
        Program*      pProgram,
        const Thread* pThread,  // May be nullptr for EvaluateConst()
        INT64*        pValue
    ) const
    {
        LWDASSERT (pProgram != nullptr && pValue != nullptr);
        EC ec = OK;

        INT64 arg1;
        INT64 arg2;
        CHECK_EC(EvaluateArg(m_pArg1, pProgram, pThread, &arg1));
        CHECK_EC(EvaluateArg(m_pArg2, pProgram, pThread, &arg2));

        switch (TokenToOperator(m_Op))
        {
            case Expression::DIVIDE:
            case Expression::MODULO:
                if (arg2 == 0)
                {
                    if (pProgram->Uniq(__FILE__, __LINE__))
                    {
                        Printf(PriError, "%s: Divide by zero:\n",
                               m_Op.GetFileLine().c_str());
                        m_Op.PrintUnderlined(PriError);
                        Printf(PriError, "\n");
                    }
                    return ILWALID_FILE_FORMAT;
                }
                break;
            default:
                break;
        }

        switch (TokenToOperator(m_Op))
        {
            case Expression::MULTIPLY:
                *pValue = arg1 * arg2;
                break;
            case Expression::DIVIDE:
                *pValue = arg1 / arg2;
                break;
            case Expression::MODULO:
                *pValue = arg1 % arg2;
                break;
            case Expression::PLUS:
                *pValue = arg1 + arg2;
                break;
            case Expression::MINUS:
                *pValue = arg1 - arg2;
                break;
            case Expression::LEFT_SHIFT:
                *pValue = (arg2 >= 0) ? (arg1 << arg2) : (arg1 >> -arg2);
                break;
            case Expression::RIGHT_SHIFT:
                *pValue = (arg2 >= 0) ? (arg1 >> arg2) : (arg1 << -arg2);
                break;
            case Expression::LOGICAL_LT:
                *pValue = (arg1 < arg2);
                break;
            case Expression::LOGICAL_LE:
                *pValue = (arg1 <= arg2);
                break;
            case Expression::LOGICAL_GT:
                *pValue = (arg1 > arg2);
                break;
            case Expression::LOGICAL_GE:
                *pValue = (arg1 >= arg2);
                break;
            case Expression::LOGICAL_EQ:
                *pValue = (arg1 == arg2);
                break;
            case Expression::LOGICAL_NE:
                *pValue = (arg1 != arg2);
                break;
            case Expression::BITWISE_AND:
                *pValue = arg1 & arg2;
                break;
            case Expression::BITWISE_XOR:
                *pValue = arg1 ^ arg2;
                break;
            case Expression::BITWISE_OR:
                *pValue = arg1 | arg2;
                break;
            case Expression::LOGICAL_AND:
                *pValue = (arg1 && arg2);
                break;
            case Expression::LOGICAL_OR:
                *pValue = (arg1 || arg2);
                break;
            default:
                LWDASSERT(!"Unknown unary operator");
                return SOFTWARE_ERROR;
        }
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Find the value of the expression & store it in *pValue
    //!
    EC TernaryExpression::EvaluateImpl
    (
        Program*      pProgram,
        const Thread* pThread,  // May be nullptr for EvaluateConst()
        INT64*        pValue
    ) const
    {
        LWDASSERT (pProgram != nullptr && pValue != nullptr);
        EC ec = OK;
        INT64 arg1;
        CHECK_EC(EvaluateArg(m_pArg1, pProgram, pThread, &arg1));
        CHECK_EC(EvaluateArg(arg1 ? m_pArg2 : m_pArg3,
                            pProgram, pThread, pValue));
        return OK;
    }

    //----------------------------------------------------------------
    CodeExpression::CodeExpression
    (
        const Expression* pExpression,
        unsigned          index1,
        unsigned          index2,
        unsigned          hiBit,
        unsigned          loBit,
        Type              type
    ) :
        m_pExpression(pExpression),
        m_Index1(index1),
        m_Index2(index2),
        m_HiBit(hiBit),
        m_LoBit(loBit),
        m_Type(type)
    {
    }

    //----------------------------------------------------------------
    //! \brief Evaluate the expression, and store the value.
    //!
    //! Evaluate the expression, and store the value in the bits
    //! specified in the constructor.
    //!
    EC CodeExpression::Resolve(Thread* pThread) const
    {
        Program* pProgram = pThread->GetProgram();
        EC ec = OK;

        // Find the value of the expression
        //
        INT64 value64 = 0;
        CHECK_EC(m_pExpression->Evaluate(pThread, &value64));

        // Make sure the value doesn't overflow
        //
        if (Bits::IsOverflow64(m_HiBit, m_LoBit, value64))
        {
            const Token& firstToken = m_pExpression->GetFirstToken();
            const Token& lastToken = m_pExpression->GetLastToken();
            if (pProgram->Uniq(__FILE__, __LINE__))
            {
                Printf(PriError, "%s: This value (%lld) is %s\n",
                       firstToken.GetFileLine().c_str(), value64,
                       Bits::GetOverflow64(m_HiBit, m_LoBit, value64).c_str());
                firstToken.PrintUnderlined(PriError, lastToken);
                Printf(PriError, "\n");
            }
            return ILWALID_FILE_FORMAT;
        }

        // Store the value
        //
        LWDASSERT(m_HiBit - m_LoBit < 32);
        INT32 value32 = static_cast<INT32>(value64);
        switch (m_Type)
        {
            case CodeExpression::STATEMENT:
                pThread->GetInstruction(m_Index1)->ResolveExpression(
                        m_Index2, m_HiBit, m_LoBit, value32);
                break;
            case CodeExpression::DQ:
                pThread->GetPatram(m_Index1)->UpdateDq(m_HiBit, m_LoBit,
                                                       value32);
                break;
            case CodeExpression::ECC:
                pThread->GetPatram(m_Index1)->UpdateEcc(m_HiBit, m_LoBit,
                                                        value32);
                break;
            case CodeExpression::DBI:
                pThread->GetPatram(m_Index1)->UpdateDbi(m_HiBit, m_LoBit,
                                                        value32);
                break;
        }
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Colwenience function to find a label given a string
    //!
    Thread::Labels::const_iterator Thread::FindLabel(const string& label) const
    {
        Token token(Token::IDENTIFIER, 0, "", label, 0, 0, label.size());
        return m_Labels.find(token);
    }

    //----------------------------------------------------------------
    //! \brief Return a ThreadMask as a decimal number
    //!
    //! There's a pretty good chance that the ThreadMask will have
    //! more than 64 bits in the future, so this function does the
    //! decimal colwersion manually.
    //!
    /* static */ string Thread::ToJson(const ThreadMask& mask)
    {
        // Used to divide large number stored in a vector<UINT32> by 10.
        // 1 << 32 == 10 * QUOTIENT32 + REMAINDER32
        //
        static const UINT32 QUOTIENT32  = 429496729;
        static const UINT32 REMAINDER32 = 6;

        // Colwert the ThreadMask into a vector<UINT32>
        //
        vector<UINT32> data(mask.num_blocks());
        boost::to_block_range(mask, data.begin());
        while (!data.empty() && data.back() == 0)
        {
            data.pop_back();
        }

        // Colwert the vector<UINT32> to decimal by repeatedly
        // dividing by 10, and forming the digits from the remainders.
        //
        string result = (data.empty() ? "0" : "");
        while (!data.empty())
        {
            UINT32 remainder = 0;
            for (auto pData = data.rbegin(); pData != data.rend(); ++pData)
            {
                const UINT32 myQuotient  = (*pData / 10 +
                                            remainder * QUOTIENT32);
                const UINT32 myRemainder = (*pData % 10 +
                                            remainder * REMAINDER32);
                *pData    = myQuotient + myRemainder / 10;
                remainder = myRemainder % 10;
            }
            result = static_cast<char>('0' + remainder) + result;
            if (data.back() == 0)
            {
                data.pop_back();
            }
        }
        return result;
    }

    //----------------------------------------------------------------
    //! \brief Return the Instruction that we are lwrrently parsing
    //!
    //! It is up to the caller to make sure that we're lwrrently
    //! parsing an instruction.
    //!
    Instruction* Thread::GetPendingInstruction()
    {
        LWDASSERT(m_PendingOperation == INSTRUCTION);
        return &m_Instructions.back();
    }

    //----------------------------------------------------------------
    //! \brief Return the Patram entry that we are lwrrently parsing
    //!
    //! It is up to the caller to make sure that we're lwrrently
    //! parsing a Patram entry.
    //!
    Patram* Thread::GetPendingPatram()
    {
        LWDASSERT(m_PendingOperation == PATRAM);
        return &m_Patrams.back();
    }

    //----------------------------------------------------------------
    //! \brief Make sure that GetPendingInstruction() will return a valid value
    //!
    //! This method should be called when we parse a statement that
    //! should be part of an Instruction object.  If we're lwrrently
    //! parsing an instruction, then do nothing; the statement is part
    //! of the current instruction.  If not, then add a new empty
    //! instruction.
    //!
    //! If this method finds an error (e.g. we were in the middle of a
    //! patram entry), then print the error and return an error code,
    //! but still make sure that GetPendingInstruction() will work so
    //! that we don't get any core-dumps.
    //!
    EC Thread::EnsurePendingInstruction(const Token& opcode)
    {
        EC ec = OK;

        // If we're in the middle of a Patram entry, then print an
        // error and close it as if we had parsed a semicolon.  Keep
        // running, but return an error later.
        //
        if (m_PendingOperation == PATRAM)
        {
            if (m_pProgram->Uniq(__FILE__, __LINE__))
            {
                Printf(PriError,
                       "%s: Unexpected statement in the middle of PATRAM:\n",
                       opcode.GetFileLine().c_str());
                Printf(PriError, "%s\n", ToString(opcode.GetLine()).c_str());
                Printf(PriError, "\n");
            }
            ec = ILWALID_FILE_FORMAT;
            m_PendingOperation = NONE;
        }

        // If we're not in the middle of anything (e.g. start of file
        // or after a semicolon), then create a new empty Instruction
        // and set pendingOperation = INSTRUCTION.
        //
        if (m_PendingOperation == NONE)
        {
            for (const Token& label: m_PendingLabels)
            {
                m_Labels[label] = static_cast<INT32>(m_Instructions.size());
            }
            m_PendingLabels.clear();
            m_Instructions.emplace_back(this);
            m_PendingOperation = INSTRUCTION;
        }

        // Verify that we're now in the correct state.
        //
        LWDASSERT(m_PendingOperation == INSTRUCTION);
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Make sure that GetPendingPatram() will return a valid value
    //!
    //! This method should be called when we parse a statement that
    //! should be part of a Patram entry.  If we're lwrrently parsing
    //! a Patram entry, then do nothing; the statement will be
    //! appended to the current entry.  current instruction.  If not,
    //! then add a new empty Patram object.
    //!
    //! If this method finds an error (e.g. we were in the middle of
    //! an Instruction), then print the error and return an error
    //! code, but still make sure that GetPendingPatram() will
    //! work so that we don't get any core-dumps.
    //!
    EC Thread::EnsurePendingPatram(const Token& opcode)
    {
        EC ec = OK;

        // If we're in the middle of an Instruction, then print an
        // error and close it as if we had parsed a semicolon.  Keep
        // running, but return an error later.
        //
        if (m_PendingOperation == INSTRUCTION)
        {
            if (m_pProgram->Uniq(__FILE__, __LINE__))
            {
                Printf(PriError,
                       "%s: Unexpected PATRAM in the middle of instruction:\n",
                       opcode.GetFileLine().c_str());
                Printf(PriError, "%s\n", ToString(opcode.GetLine()).c_str());
                Printf(PriError, "\n");
            }
            ec = ILWALID_FILE_FORMAT;
            m_PendingOperation = NONE;
        }

        // If we're not in the middle of anything (e.g. start of file
        // or after a semicolon), then create a new empty Patram and
        // set pendingOperation = PATRAM.
        //
        if (m_PendingOperation == NONE)
        {
            for (const Token& label: m_PendingLabels)
            {
                m_Labels[label] = static_cast<INT32>(m_Patrams.size());
            }
            m_PendingLabels.clear();
            m_Patrams.emplace_back(m_pProgram);
            m_PendingOperation = PATRAM;
        }

        // Verify that we're now in the correct state.
        //
        LWDASSERT(m_PendingOperation == PATRAM);
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Close the pending Instruction or Patram due to a semicolon
    //!
    //! \param semicolon The semicolon token that caused this
    //!     Instruction or Patram entry to be closed
    //! \param nop See Instruction::Close()
    //!
    EC Thread::ClosePendingOperation(const Token& semicolon, ReservedWord nop)
    {
        EC ec = OK;

        // Close the pending operation.  If there isn't one, assume an
        // empty Instruction, but print a warning if the WARN_NOP flag
        // is active.
        //
        switch (m_PendingOperation)
        {
            case INSTRUCTION:
                FIRST_EC(m_Instructions.back().Close(semicolon, nop));
                break;
            case PATRAM:
                FIRST_EC(m_Patrams.back().Close(semicolon));
                break;
            case NONE:
                if (m_pProgram->GetFlag(Flag::WARN_NOP) &&
                    m_pProgram->Uniq(__FILE__, __LINE__))
                {
                    Printf(PriWarn, "%s: Empty statement; assuming NOP\n",
                           semicolon.GetFileLine().c_str());
                    semicolon.PrintUnderlined(PriWarn);
                    FIRST_EC(m_pProgram->ReportWarning());
                }
                FIRST_EC(EnsurePendingInstruction(semicolon));
        }

        // Indicate that there is now no pending operation
        //
        m_PendingOperation = Thread::NONE;
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Add a label to the Thread
    //!
    //! This method should be called when we parse a "identifier :"
    //! sequence that defines a label.
    //!
    //! \param label An IDENTIFIER token containing the label
    //!
    EC Thread::AddLabel(const Token& label)
    {
        EC ec = OK;

        // Make sure the label is not a duplicate
        //
        if (m_Labels.count(label) || m_PendingLabels.count(label))
        {
            const Token& oldLabel = (m_Labels.count(label) ?
                                     m_Labels.find(label)->first :
                                     *m_PendingLabels.find(label));
            Printf(PriError, "%s: Duplicate label:\n",
                   label.GetFileLine().c_str());
            label.PrintUnderlined(PriError);
            Printf(PriError, "%s: Previous definition was here:\n",
                   oldLabel.GetFileLine().c_str());
            oldLabel.PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        // If we're in the middle of parsing an Instruction or Patram,
        // then the value of the label is the offset of the current
        // Instruction or Patram.  If not, then add the label to
        // m_PendingLabels so that we can assign it a value when the
        // next Instruction or Patram starts.
        //
        // The WARN_MIDLABEL flag indicates that labels should *only*
        // appear before the start of the next Instruction or Patram,
        // so print a warning if we violate that.
        //
        switch (m_PendingOperation)
        {
            case INSTRUCTION:
                if (m_pProgram->GetFlag(Flag::WARN_MIDLABEL) &&
                    m_pProgram->Uniq(__FILE__, __LINE__))
                {
                    Printf(PriWarn,
                           "%s: Label in middle of instruction"
                           " (missing semicolon?):\n",
                           label.GetFileLine().c_str());
                    label.PrintUnderlined(PriWarn);
                    CHECK_EC(m_pProgram->ReportWarning());
                }
                m_Labels[label] = static_cast<INT32>(m_Instructions.size() - 1);
                break;
            case PATRAM:
                if (m_pProgram->GetFlag(Flag::WARN_MIDLABEL) &&
                    m_pProgram->Uniq(__FILE__, __LINE__))
                {
                    Printf(PriWarn, "%s: Label in middle of patram"
                           " (missing semicolon?):\n",
                           label.GetFileLine().c_str());
                    label.PrintUnderlined(PriWarn);
                    CHECK_EC(m_pProgram->ReportWarning());
                }
                m_Labels[label] = static_cast<INT32>(m_Patrams.size() - 1);
                break;
            case NONE:
                m_PendingLabels.insert(label);
                break;
        }
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Add an Expression to the thread
    //!
    //! Add an arithmetic expression to the thread, which will be
    //! resolved later to set some bits in one of the Statement or
    //! Patram objects.
    //!
    //! \param pExpression Pointer to an arithmetic expression.
    //! \param hiBit For a STATEMENT Expression, indicates the high
    //!     bit of the bitfield where we will store Expression's
    //!     value.  For a DQ, ECC, or DBI Expression, indicates the
    //!     number of bits.
    //! \param loBit For a STATEMENT Expression, indicates the low bit
    //!     of the bitfield where we will store Expression's value.
    //!     For a DQ, ECC, or DBI Expression, must be 0.
    //! \param type Indicates where we should store this value of this
    //!     expression: In a Statement, patram DQ bits, etc.
    //!
    EC Thread::AddExpression
    (
        const Expression*    pExpression,
        unsigned             hiBit,
        unsigned             loBit,
        CodeExpression::Type type
    )
    {
        unsigned index1 = 0;
        unsigned index2 = 0;
        EC ec = OK;

        // Callwlate index1, index2, hiBit, and loBit depending on
        // what type of Expression this is.
        //
        switch (type)
        {
            case CodeExpression::STATEMENT:
                index1 = static_cast<unsigned>(m_Instructions.size()) - 1;
                index2 = m_Instructions.back().GetNumStatements() - 1;
                CHECK_EC(m_Instructions.back().UpdateStatement(
                                pExpression->GetFirstToken(),
                                hiBit, loBit, 0, false, nullptr));
                break;
            case CodeExpression::DQ:
                LWDASSERT(loBit == 0);
                index1 = static_cast<unsigned>(m_Patrams.size()) - 1;
                loBit  = m_Patrams.back().GetDq().GetSize();
                m_Patrams.back().AppendDq(hiBit, 0);
                hiBit += loBit - 1;
                break;
            case CodeExpression::ECC:
                LWDASSERT(loBit == 0);
                index1 = static_cast<unsigned>(m_Patrams.size()) - 1;
                loBit  = m_Patrams.back().GetEcc().GetSize();
                m_Patrams.back().AppendEcc(hiBit, 0);
                hiBit += loBit - 1;
                break;
            case CodeExpression::DBI:
                LWDASSERT(loBit == 0);
                index1 = static_cast<unsigned>(m_Patrams.size()) - 1;
                loBit  = m_Patrams.back().GetDbi().GetSize();
                m_Patrams.back().AppendDbi(hiBit, 0);
                hiBit += loBit - 1;
                break;
            default:
                LWDASSERT(!"Unknown expression type");
        }

        // Sanity check: make sure we didn't encounter any empty
        // vectors when we callwlated index1 and index2 to be the last
        // element of a vector, and make sure the bitfield hiBit:loBit
        // contains sane values.
        //
        LWDASSERT(index1 != static_cast<unsigned>(-1));
        LWDASSERT(index2 != static_cast<unsigned>(-1));
        LWDASSERT(hiBit >= loBit);

        // Add the Expression object
        //
        m_CodeExpressions.emplace_back(pExpression, index1, index2,
                                       hiBit, loBit, type);
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Close the thread at EOF
    //!
    EC Thread::Close()
    {
        EC ec = OK;
        static const Token startLabel(Token::IDENTIFIER, 0, "<stdin>",
                                      "start", 0, 0, 5);
        static const Token endLabel(Token::IDENTIFIER, 0, "<stdin>",
                                    "end", 0, 0, 3);
        static const Token endpLabel(Token::IDENTIFIER, 0, "<stdin>",
                                     "endp", 0, 0, 4);

        // Print an error if there was a pending operation.
        //
        switch (m_PendingOperation)
        {
            case NONE:
                break;    // This is the success case
            case INSTRUCTION:
                if (m_pProgram->Uniq(__FILE__, __LINE__))
                {
                    const Statement& lastStatement =
                        GetPendingInstruction()->GetStatements().back();
                    const Token& lastOpcode = lastStatement.GetOpcode();
                    Printf(PriError,
                           "%s: Unexpected EOF in the middle of instruction"
                           " (missing semicolon?):\n",
                           lastOpcode.GetFileLine().c_str());
                    Printf(PriError, "%s\n",
                           ToString(lastOpcode.GetLine()).c_str());
                    Printf(PriError, "\n");
                }
                FIRST_EC(ILWALID_FILE_FORMAT);
                break;
            case PATRAM:
            {
                if (m_pProgram->Uniq(__FILE__, __LINE__))
                {
                    const Token& lastOpcode =
                        m_Patrams.back().GetOpcodes().back();
                    Printf(PriError,
                           "%s: Unexpected EOF in the middle of PATRAM"
                           " (missing semicolon?):\n",
                           lastOpcode.GetFileLine().c_str());
                    Printf(PriError, "%s\n",
                           ToString(lastOpcode.GetLine()).c_str());
                    Printf(PriError, "\n");
                }
                FIRST_EC(ILWALID_FILE_FORMAT);
                break;
            }
        }
      
        // Print an error if there were trailing labels.  We can't
        // tell whether they were supposed to be instruction or patram
        // offsets.
        //
        if (!m_PendingLabels.empty())
        {
            if (m_pProgram->Uniq(__FILE__, __LINE__))
            {
                Printf(PriError, "Trailing label(s) at the end of program:\n");
                for (const Token& label: m_PendingLabels)
                {
                    label.PrintUnderlined(PriError);
                }
                Printf(PriError,
                       "Use the special label \"%s\" for the end of code.\n",
                       ToString(endLabel.GetText()).c_str());
                Printf(PriError,
                       "Use the special label \"%s\" for the end of patram.\n",
                       ToString(endpLabel.GetText()).c_str());
                Printf(PriError, "\n");
            }
            FIRST_EC(ILWALID_FILE_FORMAT);
        }

        m_pProgram->AddStopAndNop(this);

        // Add special labels
        //
        if (!m_Labels.count(startLabel))
        {
            m_Labels[startLabel] = 0;
        }
        if (!m_Labels.count(endLabel))
        {
            m_Labels[endLabel] = static_cast<INT32>(m_Instructions.size());
        }
        if (!m_Labels.count(endpLabel))
        {
            m_Labels[endpLabel] = static_cast<INT32>(m_Patrams.size());
        }

        // Resolve all CodeExpressions
        //
        for (const CodeExpression& expression: m_CodeExpressions)
        {
            FIRST_EC(expression.Resolve(this));
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Program constructor
    //!
    //! Create a Program with build Flags inherited from the Launcher.
    //! Note: The design allows a subclass to call Flags::SetDefault()
    //! to choose different defaults than Launcher, if someone says,
    //! "We want this build flag to be different after GA100, but
    //! leave it as-is on GA100 for back-compatibility."
    //!
    Program::Program(const Launcher& launcher) :
        m_pLauncher(&launcher),
        m_Flags(launcher.GetFlags())
    {
    }

    //----------------------------------------------------------------
    //! \brief Initialize the Program object
    //!
    //! Initialize the program with a single Thread, in which all bits
    //! in the ThreadMask are set.  See the Thread class for more
    //! details.
    //!
    //! This code would ideally be in the constructor, except that it
    //! relies on virtual methods that can't be called from the
    //! constructor.
    //!
    void Program::Initialize()
    {
        LWDASSERT(m_Threads.empty());
        m_SelectedThreadsMask.resize(GetThreadMaskSize());
        m_SelectedThreadsMask.set();
        m_Threads.emplace_back(this);
        m_Threads[0].SetThreadMask(m_SelectedThreadsMask);
    }

    //----------------------------------------------------------------
    //! \brief Preprocess and assemble a program
    //!
    //! Run the preprocessor, and pass the output of the preprocessor
    //! to the input of the virtual Program::Assemble() method.
    //!
    //! \param pPreprocessor A Preprocessor in which the caller has
    //!     already called LoadFile(), SetSearchPath(), etc to define
    //!     how the program will be preprocessed.
    //!
    EC Program::Assemble(Preprocessor* pPreprocessor)
    {
        LWDASSERT(pPreprocessor != nullptr);
        EC ec = OK;

        pPreprocessor->SetLineCommandMode(Preprocessor::LineCommandHash);
        vector<char> input;
        CHECK_EC(pPreprocessor->Process(&input));
        CHECK_EC(Assemble(move(input)));
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Print the program to a JSON file
    //!
    void Program::PrintJson(FILE* fh) const
    {
#define INDENT2  "  "
#define INDENT4  "    "
#define INDENT6  "      "
#define INDENT8  "        "
#define INDENT10 "          "
#define INDENT12 "            "

        // Print header
        //
        fprintf(fh, "{\n");
        fprintf(fh, INDENT2 "\"litter\": \"%s\",\n",
                LitterToName(m_pLauncher->GetLitter()));

        // Print Threads
        //
        fprintf(fh, INDENT2 "\"threads\":\n");
        fprintf(fh, INDENT2 "[\n");
        const char* threadSep = "";
        for (const Thread& thread: m_Threads)
        {
            fprintf(fh, "%s", threadSep);
            fprintf(fh, INDENT4 "{\n");

            // Print Thread header
            //
            fprintf(fh, INDENT6 "\"mask\": %s,\n",
                    Thread::ToJson(thread.GetThreadMask()).c_str());
            fprintf(fh, INDENT6 "\"maxCycles\": %lld,\n",
                    thread.GetMaxCycles());

            // Print Instructions
            //
            fprintf(fh, INDENT6 "\"code\":\n");
            fprintf(fh, INDENT6 "[\n");
            const char* instSep = "";
            for (const Instruction& inst: thread.GetInstructions())
            {
                fprintf(fh, "%s", instSep);
                fprintf(fh, INDENT8 "{\n");
                fprintf(fh, INDENT10 "\"source\":\n");
                fprintf(fh, INDENT10 "[\n");
                const char* sourceSep = "";
                for (const Statement& statement: inst.GetStatements())
                {
                    fprintf(fh, "%s", sourceSep);
                    const Token& opcode = statement.GetOpcode();
                    const string source = (opcode.GetFileLine() + ": " +
                                           ToString(opcode.GetLine()));
                    fprintf(fh, INDENT12 "%s", ToJson(source).c_str());
                    sourceSep = ",\n";
                }
                fprintf(fh, "\n");
                fprintf(fh, INDENT10 "],\n");
                fprintf(fh, INDENT10 "\"data\":\n");
                fprintf(fh, INDENT10 "[\n");
                inst.GetBits().PrintJson(fh, 12);
                fprintf(fh, INDENT10 "]\n");
                fprintf(fh, INDENT8 "}");
                instSep = ",\n";
            }
            fprintf(fh, "\n");
            fprintf(fh, INDENT6 "],\n");

            // Print Patrams
            //
            fprintf(fh, INDENT6 "\"patram\":\n");
            fprintf(fh, INDENT6 "[\n");
            const char* patramSep = "";
            for (const Patram& patram: thread.GetPatrams())
            {
                fprintf(fh, "%s", patramSep);
                fprintf(fh, INDENT8 "{\n");
                fprintf(fh, INDENT10 "\"source\":\n");
                fprintf(fh, INDENT10 "[\n");
                const char* sourceSep = "";
                for (const Token& opcode: patram.GetOpcodes())
                {
                    fprintf(fh, "%s", sourceSep);
                    const string source = (opcode.GetFileLine() + ": " +
                                           ToString(opcode.GetLine()));
                    fprintf(fh, INDENT12 "%s", ToJson(source).c_str());
                    sourceSep = ",\n";
                }
                fprintf(fh, "\n");
                fprintf(fh, INDENT10 "],\n");
                fprintf(fh, INDENT10 "\"dq\":\n");
                fprintf(fh, INDENT10 "[\n");
                patram.GetDq().PrintJson(fh, 12);
                fprintf(fh, INDENT10 "],\n");
                fprintf(fh, INDENT10 "\"ecc\":\n");
                fprintf(fh, INDENT10 "[\n");
                patram.GetEcc().PrintJson(fh, 12);
                fprintf(fh, INDENT10 "],\n");
                fprintf(fh, INDENT10 "\"dbi\":\n");
                fprintf(fh, INDENT10 "[\n");
                patram.GetDbi().PrintJson(fh, 12);
                fprintf(fh, INDENT10 "]\n");
                fprintf(fh, INDENT8 "}");
                patramSep = ",\n";
            }
            fprintf(fh, "\n");
            fprintf(fh, INDENT6 "],\n");

            // Print labels
            //
            map<string_view, INT32> sortedLabels;
            for (const auto& label: thread.GetLabels())
            {
                sortedLabels[label.first.GetText()] = label.second;
            }
            fprintf(fh, INDENT6 "\"labels\":\n");
            fprintf(fh, INDENT6 "{\n");
            const char* labelSep = "";
            for (const auto& label: sortedLabels)
            {
                fprintf(fh, "%s", labelSep);
                fprintf(fh, INDENT8 "\"%s\": %d",
                        ToString(label.first).c_str(), label.second);
                labelSep = ",\n";
            }
            fprintf(fh, INDENT6 "\n");
            fprintf(fh, INDENT6 "}\n");

            fprintf(fh, INDENT4 "}");
            threadSep = ",\n";
        }
        fprintf(fh, "\n");
        fprintf(fh, INDENT2 "]\n");
        fprintf(fh, "}\n");
#undef INDENT2
#undef INDENT4
#undef INDENT6
#undef INDENT8
#undef INDENT10
#undef INDENT12
    }

    //----------------------------------------------------------------
    //! \brief Simulate the exelwtion of the program
    //!
    EC Program::Simulate() const
    {
        EC ec = OK;

        for (const auto& muccThread : m_Threads)
        {
            unique_ptr<SimulatedThread> simulatedThread;
            simulatedThread = m_pLauncher->AllocSimulatedThread(muccThread);
            CHECK_EC(simulatedThread->Run());
        }

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Helper function for TREAT_WARNINGS_AS_ERRORS flag
    //!
    //! This method should be called after printing a PriWarn message.
    //! If TREAT_WARNINGS_AS_ERRORS is enabled, it prints a "Treating
    //! warnings as errors" messages and returns an error, otherwise
    //! it returns OK.
    //!
    //! Either way, this method prints a blank line at PriWarn.  The
    //! convention in Mucc is to print a blank PriError or PiWarn line
    //! after each error or warning, because most errors/warnings are
    //! several lines long, so the blank line helps separate them.
    //! Since we always print a blank line after a warning, and always
    //! call this function after a warning, it makes things easier to
    //! combine them.
    //!
    //! Typical usage:
    //!     Printf(PriWarn, "some warning\n");
    //!     CHECK_EC(program.ReportWarning());
    //!
    EC Program::ReportWarning()
    {
        EC ec = OK;

        if (GetFlag(Flag::TREAT_WARNINGS_AS_ERRORS))
        {
            ec = ILWALID_FILE_FORMAT;
            if (!m_ReportedWarning)
            {
                // Print this message after the first warning
                Printf(PriError, "Treating warnings as errors\n");
                m_ReportedWarning = true;
            }
        }

        Printf(PriWarn, "\n");
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Used to eliminate duplicate error/warning messages
    //!
    //! In most cases, when we find a syntax error, we return a bad EC
    //! and stop parsing the line.  But sometimes we need to continue
    //! processing, in which case we might print the same
    //! error/warning message N times if we are generating N threads.
    //! This method helps prevent that redundancy by checking whether
    //! we've already exelwted a Printf at a particular (file,
    //! lineNumber) tuple, and suppressing it afterwards.
    //!
    //! Typical usage:
    //!     if (program.Uniq(__FILE__, __LINE__)) { Printf(PriError, ...
    //!
    //! Call ResetUniq() to re-enable all Printfs that are suppressed
    //! by Uniq.  This should be done when we start parsing a new
    //! line/statement/whatever.
    //!
    bool Program::Uniq(const char* file, unsigned line)
    {
        if (m_UniqLog[file].count(line))
        {
            return false;
        }
        m_UniqLog[file].insert(line);
        return true;
    }

    //----------------------------------------------------------------
    //! \brief Set the bitmask that says which threads are selected
    //!
    //! This should be used after the .FBPA_SUBP_MASK directive, to
    //! determine which Threads are selected.  The next time we call
    //! GetSelectedThreads(), it will return the new selected threads.
    //!
    //! This method should not be called while the code is using the
    //! return value of GetSelectedThreads(), since it will corrupt
    //! the data.
    //!
    //! The new selected threads are not callwlated until the next
    //! time GetSelectedThreads() is called, just in case
    //! SetSelectedThreads() gets called several times in a row for
    //! whatever reason.  The mask may cause new Thread objects to get
    //! created, which will slow down the assembly, so we don't want
    //! to create Threads unnecessarily for intermediate masks.
    //!
    //! See the Threads class for an overview of Thread masks.
    //!
    void Program::SetSelectedThreads(const ThreadMask& mask)
    {
        m_SelectedThreadsMask = mask;
        m_SelectedThreads.clear(); // Tells GetSelectedThreads() to re-create
    }

    //----------------------------------------------------------------
    //! \brief Return the Threads selected by SetSelectedThreads()
    //!
    //! Do not call SetSelectedThreads() while using the return
    //! value of this method, or it will corrupt the data.
    //!
    const vector<Thread*>& Program::GetSelectedThreads()
    {
        // If SetSelectedThreads() was called, then it cleared the
        // m_SelectedThreads vector.  Recreate it now.
        //
        if (m_SelectedThreads.empty())
        {
            const ThreadMask unselectedThreadsMask = ~m_SelectedThreadsMask;

            // Loop through m_Threads to find any whose threadMask is
            // partially inside m_SelectedThreadsMask, and partially
            // outside.  Split each such thread into two identical
            // threads, one with the part of the threadMask that was
            // inside, and one with the part that was outside.
            //
            for (size_t ii = 0; ii < m_Threads.size(); ++ii)
            {
                const ThreadMask& mask          = m_Threads[ii].GetThreadMask();
                const ThreadMask  selectedPart   = mask & m_SelectedThreadsMask;
                const ThreadMask  unselectedPart = mask & unselectedThreadsMask;
                if (selectedPart.any() && unselectedPart.any())
                {
                    m_Threads.emplace_back(m_Threads[ii]);
                    m_Threads[ii].SetThreadMask(selectedPart);
                    m_Threads.back().SetThreadMask(unselectedPart);
                }
            }

            // Fill m_SelectedThreads with pointers to the elements of
            // m_Threads with threadMasks inside m_SelectedThreadsMask.
            //
            for (Thread& thread: m_Threads)
            {
                if (thread.GetThreadMask().intersects(m_SelectedThreadsMask))
                {
                    m_SelectedThreads.push_back(&thread);
                }
            }
        }

        return m_SelectedThreads;
    }

    //----------------------------------------------------------------
    //! \brief Pop an opcode from Tokenizer and add a new Statement
    //!
    //! Pop an opcode token from the start of pTokens.  Use it to add
    //! a new Statement to the pending Instruction in each selected
    //! thread.  If there is no pending Instruction, create one.
    //!
    //! On error, return ILWALID_FILE_FORMAT but create the new
    //! Instruction anyways, since subsequent code may rely on the new
    //! Instruction.
    //!
    EC Program::EatOpcode(Tokenizer* pTokens, bool isHack)
    {
        const Token& opcode = pTokens->GetToken(0);
        EC ec = OK;

        LWDASSERT(opcode.Isa(Token::RESERVED_WORD));
        for (Thread* pThread: GetSelectedThreads())
        {
            FIRST_EC(pThread->EnsurePendingInstruction(opcode));
            Instruction* pInstruction = pThread->GetPendingInstruction();
            pInstruction->AddStatement(opcode, isHack);
        }
        pTokens->Pop(1);
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Pop an opcode from Tokenizer and add a new Statement
    //!
    //! This method acts like EatOpcode(pTokens, false) followed by
    //! UpdateStatement(opcode, hiBit, loBit, value).  It pops an
    //! opcode token from the start of pTokens, uses it to add a new
    //! Statement to the pending Instruction in each selected thread,
    //! and sets the bits hiBit:loBit to "value" in the new
    //! Statements.
    //!
    //! This method is useful for the 90% case in which setting bits
    //! hiBit:loBit to "value" indicates that we're doing the
    //! operation specified by the opcode.  Presumably, the caller
    //! already peeked at the first Token in pTokens to see which
    //! opcode it was.
    //!
    EC Program::EatOpcode
    (
        Tokenizer* pTokens,
        unsigned   hiBit,
        unsigned   loBit,
        INT32      value
    )
    {
        const Token& opcode = pTokens->GetToken(0);
        EC ec = OK;

        LWDASSERT(opcode.Isa(Token::RESERVED_WORD));
        for (Thread* pThread: GetSelectedThreads())
        {
            FIRST_EC(pThread->EnsurePendingInstruction(opcode));
            Instruction* pInstruction = pThread->GetPendingInstruction();
            pInstruction->AddStatement(opcode, false);
            FIRST_EC(pInstruction->UpdateStatement(opcode, hiBit, loBit,
                                                   value, false, nullptr));
        }
        pTokens->Pop(1);
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Pop an arithmetic expression from pTokens that affects the code
    //!
    //! Pop an arithmetic expression from the start of pTokens
    //! pTokens, and use it to create a CodeExpression object that
    //! modifies the indicated bits in the pending Statement or Patram
    //! (depending on "type") in each selected thread.
    //!
    EC Program::EatExpression
    (
        Tokenizer*           pTokens,
        unsigned             hiBit,
        unsigned             loBit,
        CodeExpression::Type type
    )
    {
        EC ec = OK;

        unique_ptr<const Expression> pExpression;
        CHECK_EC(EatExpression(pTokens, &pExpression));
        for (Thread* pThread: GetSelectedThreads())
        {
            CHECK_EC(pThread->AddExpression(pExpression.get(),
                                            hiBit, loBit, type));
        }
        m_Expressions.push_back(move(pExpression));
        return OK;
    }

    namespace
    {
        //------------------------------------------------------------
        //! \brief Support class for Dijkstra's "Shunting Yard" algorithm
        //!
        //! This class is used by Program::EatExpression(), which
        //! takes a series of Tokens representing an arithmetic
        //! expression (e.g. "3 * 2 + ~7"), and colwerts them into a
        //! tree of unary, binary, and ternary Expression objects.
        //!
        //! The biggest challenge with that colwersion is to properly
        //! handle operator precedence, parentheses, left-associative
        //! binary operators, and right-associative unary & ternary
        //! operators.  This class implements all that, using a
        //! modified form of Dijkstra's shunting-yard algorithm; see
        //! https://en.wikipedia.org/wiki/Shunting-yard_algorithm or
        //! https://www.geeksforgeeks.org/expression-evaluation.
        //!
        //! The classic shunting-yard algorithm handles
        //! left-associative binary operators and parentheses:
        //!
        //! 1  VAR valueStack
        //! 2  VAR operatorStack
        //! 3
        //! 4  SUBROUTINE SHUNT():
        //! 5      POP operator from operatorStack
        //! 6      POP arg2 and arg1 from valueStack
        //! 7      PUSH (arg1 operator arg2) onto valueStack
        //! 8
        //! 9  FOR EACH token:
        //! 10     IF token is a value:
        //! 11         PUSH token on valueStack
        //! 12     ELSE IF token is left-parenthesis:
        //! 13         PUSH token on operatorStack
        //! 14     ELSE IF token is right-parenthesis:
        //! 15         WHILE top of operatorStack is not left-parenthesis
        //! 16             SHUNT()
        //! 17         POP left-parethesis from operatorStack
        //! 18     ELSE IF token is any other operator:
        //! 19         WHILE top of operatorStack is higher or equal precedence:
        //! 20             SHUNT()
        //! 21         PUSH token on operatorStack
        //! 22 WHILE operatorStack is not empty:
        //! 23     SHUNT()
        //!
        //! I mostly took the precedence numbers from
        //! https://en.cppreference.com/w/cpp/language/operator_precedence,
        //! which uses low numbers for high precedence.  So line (19)
        //! above is more like "WHILE token_precedence >=
        //! top_of_operatorStack_precedence":
        //!
        //!     PRECEDENCE OPERATORS    ASSOCIATIVITY
        //!     ---------- -----------  -------------
        //!     3          +a -a !a ~a  Right-to-left
        //!     5          a*b a/b a%b  Left-to-right
        //!     6          a+b a-b      Left-to-right
        //!     7          << >>        Left-to-right
        //!     9          < <= > >=    Left-to-right
        //!     10         == !=        Left-to-right
        //!     11         &            Left-to-right
        //!     12         ^            Left-to-right
        //!     13         |            Left-to-right
        //!     14         &&           Left-to-right
        //!     15         ||           Left-to-right
        //!     16         a?b:c        Right-to-left
        //!
        //! I made a few tweaks to the algorithm to minimize special
        //! cases and handle unary operators, the ternary (a?b:c)
        //! operator, and error handling:
        //!
        //! * The algorithm doesn't check for out-of-order tokens; it
        //!   treats "3 3 3 + +" the same as "3 + 3 + 3".  I dealt
        //!   with that by adding an "expectingValue" flag, which is
        //!   true if we expect the next token to be a value (e.g.
        //!   "5"), and false if we expect the next token to be an
        //!   operator (e.g. "*").  The order requirements were
        //!   satisfied by simply checking the precondition and
        //!   postcondition values of this flag for each token:
        //!
        //!   token type(s)                   precondition postcondition
        //!   -------------                   ------------ -------------
        //!   values                          true         false
        //!   binary and ternary operators    false        true
        //!   left-paren and unary operators  true         true
        //!   right-paren                     false        false
        //!
        //!   The whole expression starts with expectingValue=true,
        //!   and should end with expectingValue=false.
        //!
        //! * To cleanly handle right-associative operators, I gave
        //!   each operator a shuntPrecedence in addition to its
        //!   regular precedence.  The shuntPrecedence is used in
        //!   lines 19-20 of the above pseudocode, to determine which
        //!   operators on operatorStack should get shunted off.  So
        //!   line 19 is really "WHILE token_shuntPrecedence >=
        //!   top_of_operatorStack_precedence".
        //!
        //!   For typical left-associative binary operators such as
        //!   "*", shuntPrecedence == precedence, which means that a
        //!   new "*" operator causes previous "*" operators to get
        //!   shunted.  But right-associative unary operators have
        //!   shuntPrecedence == precedence-1, which means that a
        //!   series of unary operaters will accumulate on the
        //!   operatorStack.
        //!
        //! * I also (ab)used "shuntPrecedence" to collase a few steps
        //!   of the algorithm.  For example, I gave left-paren a low
        //!   shuntPrecedence and high precedence, so that it won't
        //!   shunt anything when pushed on operatorStack, but nothing
        //!   can shunt it.  That let me treat left-paren mostly the
        //!   same as other operators.
        //!
        //!   The biggest abuse was the ternary operator. Alternating
        //!   question marks and colons are right-associative, so that
        //!   "a ? b : c ? d : e ? f : g" is treated as
        //!   "a ? b : (c ? d : (e ? f : g))".  But two colons in a
        //!   row cause the first colon to be shunted, so that
        //!   "a ? b ? c ? d : e : f : g" is treated as
        //!   "a ? (b ? (c ? d : e) : f) : g".  That mess gets handled
        //!   automatically by setting shuntPrecedence[?] <
        //!   precedence[:] <= shuntPrecedence[:] < precedence[?].
        //!
        class ShuntingYard
        {
        public:
            static bool IsLegalToken(const Token& token);
            EC AppendToken(const Token& token);
            EC EndExpression(unique_ptr<const Expression>* ppExpression);
            unsigned GetOpenParens() const { return m_OpenParens; }

        private:
            EC AppendValue(const Token& token);
            EC AppendOperator(const Token& token);
            EC Shunt();
            EC Shunt(unsigned shuntPrecedence);

            struct OperatorProperties
            {
                unsigned precedence;            // See above
                unsigned shuntPrecedence;       // See above
                unsigned numArgs;               // 1=unary 2=binary 3=ternary
                bool     expectingValueBefore;  // Precondition; see above
                bool     expectValueAfter;      // Postcondition; see above
            };
            using OperatorPropertiesTable =
                multimap<Expression::Operator, OperatorProperties>;
            static const OperatorPropertiesTable s_OperatorProperties;
            static const unsigned s_ValuePrecedence;
            static const unsigned s_EndPrecedence;

            struct OperatorStackEntry
            {
                Token token;
                Expression::Operator op;
                const OperatorProperties* pProps;
                OperatorStackEntry(const Token& t, Expression::Operator o,
                                   const OperatorProperties* p) :
                    token(t), op(o), pProps(p) {}
            };
            using OperatorStack = vector<OperatorStackEntry>;
            using ValueStack = vector<unique_ptr<const Expression>>;

            ValueStack    m_ValueStack;            // See above
            OperatorStack m_OperatorStack;         // See above
            bool          m_ExpectingValue = true; // See above
            unsigned      m_OpenParens = 0;        // Num unclosed left parens
        };
    }

    // This table dumps most of the messy details of the shunting-yard
    // algorithm into a table lookup.  See the documentation above the
    // "ShuntingYard" class for more info.
    //
    // Note that PLUS and MINUS appear in this table twice: once as
    // unary operators, and once as binary operators.  The expectValue
    // precondition tells us which one to use.  The table is a
    // multimap to allow those keys to be used twice.
    //
    const ShuntingYard::OperatorPropertiesTable
        ShuntingYard::s_OperatorProperties =
    {
        //                                           expect expect
        //                                shunt num  value  value
        // Operator                  prec prec  args before after
        // --------                  ---- ----- ---- ------ ------
        { Expression::LEFT_PAREN,  {  99,   0,   0,  true,  true  } },
        { Expression::RIGHT_PAREN, {  99,  98,   0,  false, false } },

        { Expression::PLUS,        {   3,   2,   1,  true,  true  } },
        { Expression::MINUS,       {   3,   2,   1,  true,  true  } },
        { Expression::LOGICAL_NOT, {   3,   2,   1,  true,  true  } },
        { Expression::BITWISE_NOT, {   3,   2,   1,  true,  true  } },

        { Expression::MULTIPLY,    {   5,   5,   2,  false, true  } },
        { Expression::DIVIDE,      {   5,   5,   2,  false, true  } },
        { Expression::MODULO,      {   5,   5,   2,  false, true  } },
        { Expression::PLUS,        {   6,   6,   2,  false, true  } },
        { Expression::MINUS,       {   6,   6,   2,  false, true  } },
        { Expression::LEFT_SHIFT,  {   7,   7,   2,  false, true  } },
        { Expression::RIGHT_SHIFT, {   7,   7,   2,  false, true  } },
        { Expression::LOGICAL_LT,  {   9,   9,   2,  false, true  } },
        { Expression::LOGICAL_LE,  {   9,   9,   2,  false, true  } },
        { Expression::LOGICAL_GT,  {   9,   9,   2,  false, true  } },
        { Expression::LOGICAL_GE,  {   9,   9,   2,  false, true  } },
        { Expression::LOGICAL_EQ,  {  10,  10,   2,  false, true  } },
        { Expression::LOGICAL_NE,  {  10,  10,   2,  false, true  } },
        { Expression::BITWISE_AND, {  11,  11,   2,  false, true  } },
        { Expression::BITWISE_XOR, {  12,  12,   2,  false, true  } },
        { Expression::BITWISE_OR,  {  13,  13,   2,  false, true  } },
        { Expression::LOGICAL_AND, {  14,  14,   2,  false, true  } },
        { Expression::LOGICAL_OR,  {  15,  15,   2,  false, true  } },

        { Expression::QUESTION,    {  17,  15,   3,  false, true  } },
        { Expression::COLON,       {  16,  16,   3,  false, true  } }
    };
    const unsigned ShuntingYard::s_ValuePrecedence =  3; // Can shunt unary
    const unsigned ShuntingYard::s_EndPrecedence   = 98; // Shunt all but lparen

    //----------------------------------------------------------------
    //! \brief Shunt the top operator in operatorStack
    //!
    //! Pop the operator from the top of operatorStack, and the top
    //! 1-3 values from valueStack, depending on whether the operator
    //! is unary, binary, or ternary.  Push the combined Expression
    //! onto valueStack.
    //!
    EC ShuntingYard::Shunt()
    {
        LWDASSERT(!m_OperatorStack.empty());
        const OperatorStackEntry op = m_OperatorStack.back();
        m_OperatorStack.pop_back();

        switch (op.pProps->numArgs)
        {
            case 1:
            {
                LWDASSERT(m_ValueStack.size() >= 1);
                unique_ptr<const Expression>& arg = m_ValueStack.back();
                arg = make_unique<UnaryExpression>(op.token, move(arg));
                break;
            }
            case 2:
            {
                LWDASSERT(m_ValueStack.size() >= 2);
                const size_t sz = m_ValueStack.size();
                unique_ptr<const Expression>& arg1 = m_ValueStack[sz - 2];
                unique_ptr<const Expression>& arg2 = m_ValueStack[sz - 1];
                arg1 = make_unique<BinaryExpression>(move(arg1), op.token,
                                                     move(arg2));
                m_ValueStack.pop_back(); // Pop arg2
                break;
            }
            case 3:
            {
                if (op.op != Expression::COLON ||
                    m_OperatorStack.empty() ||
                    m_OperatorStack.back().op != Expression::QUESTION)
                {
                    Printf(PriError, "%s: Malformed ternary operator:\n",
                           op.token.GetFileLine().c_str());
                    op.token.PrintUnderlined(PriError);
                    Printf(PriError, "\n");
                    return ILWALID_FILE_FORMAT;
                }
                LWDASSERT(m_ValueStack.size() >= 2);
                const size_t sz = m_ValueStack.size();
                unique_ptr<const Expression>& arg1 = m_ValueStack[sz - 3];
                unique_ptr<const Expression>& arg2 = m_ValueStack[sz - 2];
                unique_ptr<const Expression>& arg3 = m_ValueStack[sz - 1];
                arg1 = make_unique<TernaryExpression>(move(arg1), move(arg2),
                                                      move(arg3));
                m_ValueStack.pop_back();    // Pop arg3
                m_ValueStack.pop_back();    // Pop arg2
                m_OperatorStack.pop_back(); // Pop question mark
                break;
            }
        }
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Shunt multiple operators in operatorStack
    //!
    //! Keep shunting as long as the top of operatorStack has
    //! precedence <= shuntPrecedence, or until the stack is empty.
    //!
    EC ShuntingYard::Shunt(unsigned shuntPrecedence)
    {
        EC ec = OK;
        while (!m_OperatorStack.empty() &&
               shuntPrecedence >= m_OperatorStack.back().pProps->precedence)
        {
            CHECK_EC(Shunt());
        }
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Return "true" if this is a legal expression token
    //!
    //! This method is mostly used to determine when we've reached the
    //! end of the expression.
    //!
    bool ShuntingYard::IsLegalToken(const Token& token)
    {
        switch (token.GetType())
        {
            case Token::INTEGER:
            case Token::IDENTIFIER:
                return true;
            case Token::SYMBOL:
                return (Expression::TokenToOperator(token) !=
                        Expression::ILLEGAL);
            default:
                return false;
        }
    }

    //----------------------------------------------------------------
    //! \brief Append one Token to the end of the expression
    //!
    EC ShuntingYard::AppendToken(const Token& token)
    {
        EC ec = OK;

        switch (token.GetType())
        {
            case Token::INTEGER:
            case Token::IDENTIFIER:
                CHECK_EC(AppendValue(token));
                return ec;
            case Token::SYMBOL:
                CHECK_EC(AppendOperator(token));
                return ec;
            default:
                LWDASSERT(!"Should not get here after calling IsLegalToken");
                return SOFTWARE_ERROR;
        }
    }

    //----------------------------------------------------------------
    //! \brief This method should be called at the end of the expression
    //!
    //! \param On success, *ppExpression contains an Expression object
    //! that represents the entire expression.
    //!
    EC ShuntingYard::EndExpression
    (
        unique_ptr<const Expression>* ppExpression
    )
    {
        EC ec = OK;
        if (m_OpenParens != 0)
        {
            for (auto pOperator = m_OperatorStack.rbegin();
                 pOperator != m_OperatorStack.rend(); ++pOperator)
            {
                if (pOperator->op == Expression::LEFT_PAREN)
                {
                    Printf(PriError, "%s: Unmatched left parenthesis:\n",
                           pOperator->token.GetFileLine().c_str());
                    pOperator->token.PrintUnderlined(PriError);
                    Printf(PriError, "\n");
                    return ILWALID_FILE_FORMAT;
                }
            }
            LWDASSERT(!"Counted an unmatched left paren, but cannot find it");
            return SOFTWARE_ERROR;
        }

        CHECK_EC(Shunt(s_EndPrecedence));
        LWDASSERT(m_OperatorStack.empty());
        LWDASSERT(m_ValueStack.size() == 1);
        *ppExpression = move(m_ValueStack.back());
        m_ValueStack.pop_back();
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Append a value Token (INTEGER or IDENTIFIER) to the expression
    //!
    EC ShuntingYard::AppendValue(const Token& token)
    {
        EC ec = OK;

        if (!m_ExpectingValue)
        {
            Printf(PriError,
                "%s: Two values in a row without an operator between them:\n",
                token.GetFileLine().c_str());
            token.PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        m_ValueStack.push_back(make_unique<TokenExpression>(token));
        CHECK_EC(Shunt(s_ValuePrecedence));
        m_ExpectingValue = false;
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Append an operator Token to the expression
    //!
    EC ShuntingYard::AppendOperator(const Token& token)
    {
        const Expression::Operator op = Expression::TokenToOperator(token);
        EC ec = OK;

        const OperatorProperties* pProps = nullptr;
        for (auto iter = s_OperatorProperties.lower_bound(op);
             iter != s_OperatorProperties.upper_bound(op);
             ++iter)
        {
            if (iter->second.expectingValueBefore == m_ExpectingValue)
            {
                pProps = &iter->second;
                break;
            }
        }
        if (pProps == nullptr)
        {
            Printf(PriError, "%s: Illegal use of \"%s\" operator:\n",
                   token.GetFileLine().c_str(),
                   ToString(token.GetText()).c_str());
            token.PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        CHECK_EC(Shunt(pProps->shuntPrecedence));
        switch (op)
        {
            case Expression::LEFT_PAREN:
                m_OperatorStack.emplace_back(token, op, pProps);
                ++m_OpenParens;
                break;
            case Expression::RIGHT_PAREN:
                if (m_OpenParens == 0)
                {
                    Printf(PriError, "%s: Unmatched right parenthesis:\n",
                           token.GetFileLine().c_str());
                    token.PrintUnderlined(PriError);
                    Printf(PriError, "\n");
                    return ILWALID_FILE_FORMAT;
                }
                LWDASSERT(!m_OperatorStack.empty());
                LWDASSERT(m_OperatorStack.back().op == Expression::LEFT_PAREN);
                m_OperatorStack.pop_back();
                --m_OpenParens;
                CHECK_EC(Shunt(s_ValuePrecedence));
                break;
            default:
                m_OperatorStack.emplace_back(token, op, pProps);
                break;
        }
        m_ExpectingValue = pProps->expectValueAfter;

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Pop an arithmetic expression from pTokens
    //!
    //! Pop an arithmetic expression the start of pTokens, and use it
    //! to create an Expression object.
    //!
    EC Program::EatExpression
    (
        Tokenizer*         pTokens,
        unique_ptr<const Expression>* ppExpression
    )
    {
        ShuntingYard shuntingYard;
        EC ec = OK;

        // Parse the first token
        //
        if (!ShuntingYard::IsLegalToken(pTokens->GetToken(0)))
        {
            Printf(PriError,
                   "%s: Expected an integer, identifier, or expression:\n",
                   pTokens->GetToken(0).GetFileLine().c_str());
            pTokens->GetToken(0).PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        // Continue parsing until we either hit an non-expression
        // token (typically a newline), or until the next token is
        // separated by whitespace AND we are not inside parentheses.
        //
        // The whitespace rule is because the assembler uses
        // space-separated args, so "FOO +5 +10" would be the mnemonic
        // "FOO" followed by two args.  The parenthesis rule allows
        // "FOO (+5 +6 +7) +10" to be interpreted as "FOO 18 10".
        //
        CHECK_EC(shuntingYard.AppendToken(pTokens->GetToken(0)));
        while (ShuntingYard::IsLegalToken(pTokens->GetToken(1)) &&
               (shuntingYard.GetOpenParens() > 0 ||
                pTokens->GetToken(0).IsAdjacent(pTokens->GetToken(1))))
        {
            pTokens->Pop(1);
            CHECK_EC(shuntingYard.AppendToken(pTokens->GetToken(0)));
        }
        pTokens->Pop(1);

        CHECK_EC(shuntingYard.EndExpression(ppExpression));
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Print a generic error for an unexpected token
    //!
    //! This method is mostly useful for when an unexpected token
    //! appears at the start of a line.  We can usually print a more
    //! specific message after parsing the first token or two.
    //!
    EC Program::ReportUnexpectedInput(Tokenizer* pTokens)
    {
        const Token& token = pTokens->GetToken(0);
        if (token.GetText() == "\n")
        {
            Printf(PriError, "%s: Unexpected end of line:\n",
                   token.GetFileLine().c_str());
            Printf(PriError, "%s\n", ToString(token.GetLine()).c_str());
        }
        else
        {
            Printf(PriError, "%s: Unexpected input:\n",
                   token.GetFileLine().c_str());
            token.PrintUnderlined(PriError);
        }
        Printf(PriError, "\n");
        return ILWALID_FILE_FORMAT;
    }

    //----------------------------------------------------------------
    //! \brief Update the pending statement in all Threads
    //!
    //! See Instruction::UpdateStatement() for a description of the
    //! arguments.  The caller must make sure that the value would not
    //! overflow the bitfield.
    //!
    EC Program::UpdateStatement
    (
        const Token& token,
        unsigned     hiBit,
        unsigned     loBit,
        INT32        value,
        bool         allowSharing,
        const char*  conflictMsg
    )
    {
        EC ec = OK;

        // Make sure the value doesn't overflow the hiBit:loBit bitfield
        //
        if (Bits::IsOverflow(hiBit, loBit, value))
        {
            Printf(PriError, "%s: This value (%d) is %s\n",
                   token.GetFileLine().c_str(), value,
                   Bits::GetOverflow(hiBit, loBit, value).c_str());
            token.PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        // Update the pending statement in each Thread
        //
        for (Thread* pThread: GetSelectedThreads())
        {
            Instruction* pInstruction = pThread->GetPendingInstruction();
            CHECK_EC(pInstruction->UpdateStatement(token, hiBit, loBit, value,
                                                   allowSharing, conflictMsg));
        }
        return ec;
    }
}
