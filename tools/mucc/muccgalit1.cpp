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

#include "mucc.h"
#include "muccgalit1.h"
#include "ampere/ga100/hwproject.h"
using namespace LwDiagUtils;

namespace Mucc
{
    //----------------------------------------------------------------
    //! \brief Allocate a Galit1Pgm object; used by Launcher::AllocProgram()
    //!
    unique_ptr<Program> Launcher::AllocGalit1Pgm() const
    {
        return make_unique<Galit1Pgm>(*this);
    }

    //----------------------------------------------------------------
    //! \brief Assemble an assembly-language input for GALIT1
    //!
    //! \param input The assembly-language input.  The caller should
    //!     make sure that any preprocessor has already been called.
    //!     This caller should pass this input with move(), because
    //!     this method moves the input into the Galit1Pgm object.
    //!
    EC Galit1Pgm::Assemble(vector<char>&& input)
    {
        // Passed to Tokenizer constructor
        static const unordered_map<string, ReservedWord> reservedWords =
        {
            { "R0",             R0             },
            { "R1",             R1             },
            { "R2",             R2             },
            { "R3",             R3             },
            { "R4",             R4             },
            { "R5",             R5             },
            { "R6",             R6             },
            { "R7",             R7             },
            { "R8",             R8             },
            { "R9",             R9             },
            { "R10",            R10            },
            { "R11",            R11            },
            { "R12",            R12            },
            { "R13",            R13            },
            { "R14",            R14            },
            { "R15",            R15            },

            { "P0",             P0             },
            { "P1",             P1             },
            { "P2",             P2             },
            { "P3",             P3             },

            { "C0",             C0             },
            { "C1",             C1             },

            { "BRA",            BRA            },
            { "BNZ",            BNZ            },
            { "BRDE",           BRDE           },

            { "LOAD",           LOAD           },
            { "LDI",            LDI            },
            { "LDPRBS",         LDPRBS         },
            { "INCR",           INCR           },

            { "CNOP",           CNOP           },
            { "RD",             RD             },
            { "RDA",            RDA            },
            { "WR",             WR             },
            { "WRA",            WRA            },
            { "MRS",            MRS            },
            { "CPD",            CPD            },

            { "RNOP",           RNOP           },
            { "ACT",            ACT            },
            { "PRE",            PRE            },
            { "PREA",           PREA           },
            { "REFSB",          REFSB          },
            { "REF",            REF            },
            { "RFMAB",          RFMAB          },
            { "RFMPB",          RFMPB          },
            { "PDE",            PDE            },
            { "SRE",            SRE            },
            { "PDX",            PDX            },
            { "SRX",            SRX            },
            { "RPD",            RPD            },

            { "HOLD",           HOLD           },
            { "TILA",           TILA           },
            { "STOP",           STOP           },
            { "PDBI",           PDBI           },
            { "CAL",            CAL            },
            { "NOP",            NOP            },
            { "HACK",           HACK           },

            { "PATRAM",         PATRAM         }
        };

        // Passed to Tokenizer constructor
        static const unordered_map<string, Symbol> symbols =
        {
            { "(",  LEFT_PAREN  },
            { ")",  RIGHT_PAREN },
            { "!",  LOGICAL_NOT },
            { "~",  BITWISE_NOT },
            { "*",  MULTIPLY    },
            { "/",  DIVIDE      },
            { "%",  MODULO      },
            { "+",  PLUS        },
            { "-",  MINUS       },
            { "<<", LEFT_SHIFT  },
            { ">>", RIGHT_SHIFT },
            { "<",  LOGICAL_LT  },
            { "<=", LOGICAL_LE  },
            { ">",  LOGICAL_GT  },
            { ">=", LOGICAL_GE  },
            { "==", LOGICAL_EQ  },
            { "!=", LOGICAL_NE  },
            { "&",  BITWISE_AND },
            { "^",  BITWISE_XOR },
            { "|",  BITWISE_OR  },
            { "&&", LOGICAL_AND },
            { "||", LOGICAL_OR  },
            { "?",  QUESTION    },
            { ":",  COLON       },
            { ".",  DOT         },
            { ";",  SEMICOLON   },
            { "\n", NEWLINE     }
        };

        // Initialize the input and tokenizer
        //
        EC ec = OK;
        m_Input = move(input);
        Tokenizer tokens(this, &m_Input, reservedWords, symbols);

        // Loop until all tokens have been read from the Tokenizer
        //
        for (bool done = false; !done;)
        {
            // Reset the Uniq() method, which ensures that some
            // errors/warnings are only printed once per statement
            //
            ResetUniq();

            // Process one statement, label, symbol, or whatnot.
            //
            EC tmpEc = OK;
            switch (tokens.GetToken(0).GetType())
            {
                case Token::ILLEGAL:
                    tmpEc = ILWALID_FILE_FORMAT;
                    break;
                case Token::RESERVED_WORD:
                    tmpEc = EatStatement(&tokens);
                    break;
                case Token::SYMBOL:
                    tmpEc = EatSymbol(&tokens);
                    break;
                case Token::IDENTIFIER:
                    tmpEc = EatLabel(&tokens);
                    break;
                case Token::INTEGER:
                    tmpEc = ReportUnexpectedInput(&tokens);
                    break;
                case Token::END_OF_FILE:
                    done = true;
                    break;
            }

            // If any errors oclwrred in the above code-block, then
            // discard tokens until we reach the next newline or EOF
            //
            if (tmpEc != OK)
            {
                while (!tokens.GetToken(0).Isa(NEWLINE) &&
                       !tokens.GetToken(0).Isa(Token::END_OF_FILE))
                {
                    tokens.Pop(1);
                }
                tokens.Pop(1);
                FIRST_EC(tmpEc);
            }
        }

        // Now that we've processed the entire input file, do any
        // final processing on the threads.
        //
        ResetUniq();
        for (Thread& thread: GetThreads())
        {
            FIRST_EC(thread.Close());
        }

        // Sort the threads by mask.  This is just to make the output
        // consistent, for the benefit of any programs that might
        // process the output.
        //
        sort(GetThreads().begin(), GetThreads().end(),
             [](const Thread& t1, const Thread& t2)
             {
                 return t1.GetThreadMask() < t2.GetThreadMask();
             });

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Store the default instruction word in a Bits object
    //!
    //! Most bits in an instruction word are 0 until explicitly set to
    //! 1, but a small handful should be 1 until explicitly set to 0.
    //! This method sets those default bits.
    //!
    void Galit1Pgm::SetDefaultInstruction
    (
        const Thread& thread,
        Bits*         pBits
    )
    const
    {
        LWDASSERT(pBits->GetSize() == GetInstructionSize());
        LWDASSERT(!pBits->AnyBitSet(GetInstructionSize() - 1, 0));

        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(0, 0)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(1, 0)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(2, 0)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(3, 0)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefault(GetUsePatramDBIBit(),
                          thread.GetUsePatramDbi());
    }

    //----------------------------------------------------------------
    //! \brief Return the maximum number of FBPAs for this litter
    //!
    unsigned Galit1Pgm::GetMaxFbpas() const
    {
        return LW_SCAL_LITTER_NUM_FBPAS;
    }

    void Galit1Pgm::AddStopAndNop(Thread* pThread)
    {
        if (!pThread->GetInstructions().size())
        {
            return;
        }
        static const Token stopInstr(Token::RESERVED_WORD, STOP, "<stdin>",
                                     "STOP", 0, 0, 4);
        static const Token nopInstr(Token::RESERVED_WORD, NOP, "<stdin>",
                                     "NOP", 0, 0, 3);

        const Token& lastOpcode =
            pThread->GetInstructions().back().GetStatements().back().GetOpcode();
        if (lastOpcode.GetReservedWord() == STOP)
        {
             return;
        }
        Instruction stopInstruction(pThread);
        Instruction nopInstruction(pThread);
        if ((lastOpcode.GetReservedWord() == BRA) || (lastOpcode.GetReservedWord() == BNZ)
            || (lastOpcode.GetReservedWord() == BRDE))
        {
            nopInstruction.AddStatement(nopInstr, false);
            stopInstruction.AddStatement(stopInstr, false);
            pThread->GetInstructions().push_back(nopInstruction);
            pThread->GetInstructions().push_back(stopInstruction);
        }
        else
        {
            stopInstruction.AddStatement(stopInstr, false);
            pThread->GetInstructions().push_back(stopInstruction);
        }

    }

    //----------------------------------------------------------------
    //! \brief Eat a channel token
    //!
    //! GALIT1 doesn't support channels; any opcode that takes a
    //! channel in GHLIT1+ should omit the channel arg and assume
    //! channel==0.
    //!
    EC Galit1Pgm::EatChannel(Tokenizer* pTokens, int* pChannel) const
    {
        *pChannel = 0;
        return OK;
    }

    //--------------------------------------------------------------
    //! \brief Update the rfm bit in instruction.
    //! Doesn't apply to GALIT1
    EC Galit1Pgm::UpdateRFMBit
    (
        const Token &token,
        int phase,
        int channel, 
        bool isRFMSet
    )
    {
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Eat a statement from pTokens
    //!
    //! Eat a statement of the form "opcode arg arg ...".  The caller
    //! must ensure that the first token is a reserved word.
    //!
    EC Galit1Pgm::EatStatement(Tokenizer* pTokens)
    {
        LWDASSERT(pTokens->GetToken(0).Isa(Token::RESERVED_WORD));
        EC ec = OK;

        // Eat the statement.  The parsing is different for each opcode.
        //
        switch (pTokens->GetToken(0).GetReservedWord())
        {
            case P0:
            case P1:
            case P2:
            case P3:
                CHECK_EC(EatPhaseStatement(pTokens));
                break;
            case C0:
            case C1:
            case PDBI:
            case CAL:
                CHECK_EC(EatChannelStatement(pTokens));
                break;
            case BRA:
                CHECK_EC(EatOpcode(pTokens, HI_LO_VAL(STMT_BRANCH_CTL, _BRA)));
                CHECK_EC(EatExpression(pTokens, HI_LO(STMT_BRANCH_TGT_ADDR)));
                break;
            case BNZ:
                CHECK_EC(EatOpcode(pTokens, HI_LO_VAL(STMT_BRANCH_CTL, _BNZ)));
                CHECK_EC(EatRegister(pTokens, HI_LO(STMT_BRANCH_COND_REG)));
                CHECK_EC(EatExpression(pTokens, HI_LO(STMT_BRANCH_TGT_ADDR)));
                break;
            case BRDE:
                CHECK_EC(EatOpcode(pTokens, HI_LO_VAL(STMT_BRANCH_CTL, _BRDE)));
                CHECK_EC(EatExpression(pTokens, HI_LO(STMT_BRANCH_TGT_ADDR)));
                break;
            case LOAD:
                CHECK_EC(EatOpcode(pTokens, HI_LO(STMT_REG_LOAD_EN), 1));
                CHECK_EC(EatRegister(pTokens, HI_LO(STMT_REG_LOAD_INDEX)));
                CHECK_EC(EatExpression(pTokens, HI_LO(STMT_REG_LOAD_VALUE)));
                CHECK_EC(CheckForLoadAndIncr());
                break;
            case LDI:
                CHECK_EC(EatOpcode(pTokens, HI_LO(STMT_REG_LOAD_EN), 1));
                CHECK_EC(EatRegister(pTokens, HI_LO(STMT_REG_LOAD_INDEX)));
                CHECK_EC(EatExpression(pTokens, HI_LO(STMT_REG_LOAD_VALUE)));
                CHECK_EC(EatExpression(pTokens,
                                       HI_LO(STMT_REG_LOAD_INC_VALUE)));
                CHECK_EC(CheckForLoadAndIncr());
                break;
            case LDPRBS:
                {
                    const Token opcode = pTokens->GetToken(0);
                    CHECK_EC(EatOpcode(pTokens, HI_LO(STMT_REG_LOAD_EN), 1));
                    CHECK_EC(EatRegister(pTokens, HI_LO(STMT_REG_LOAD_INDEX)));
                    CHECK_EC(EatExpression(pTokens,
                                           HI_LO(STMT_REG_LOAD_VALUE)));
                    CHECK_EC(UpdateStatement(opcode,
                                             HI_LO(STMT_REG_LOAD_PRBS), 1));
                    CHECK_EC(CheckForLoadAndIncr());
                }
                break;
            case INCR:
                if (IsRegister(pTokens->GetToken(1)))
                {
                    CHECK_EC(EatOpcode(pTokens, false));
                    const Token regToken = pTokens->GetToken(0);
                    unsigned    regNum   = regToken.GetReservedWord() - R0;
                    CHECK_EC(UpdateStatement(
                            regToken, HI_LO(STMT_INCR_MASK(regNum)), 1, false,
                            "Attempting to increment the same register twice"));
                    CHECK_EC(CheckForLoadAndIncr());
                    pTokens->Pop(1);
                }
                else
                {
                    Printf(PriError, "%s: Expected a register:\n",
                           pTokens->GetToken(1).GetFileLine().c_str());
                    pTokens->GetToken(1).PrintUnderlined(PriError);
                    Printf(PriError, "\n");
                    return ILWALID_FILE_FORMAT;
                }
                break;
            case CNOP:
            case RD:
            case RDA:
            case WR:
            case WRA:
            case MRS:
            case CPD:
                Printf(PriError,
                       "%s: Column opcodes must be used in P0 or P2:\n",
                       pTokens->GetToken(0).GetFileLine().c_str());
                pTokens->GetToken(0).PrintUnderlined(PriError);
                Printf(PriError, "\n");
                return ILWALID_FILE_FORMAT;
            case RNOP:
            case ACT:
            case PRE:
            case PREA:
            case REFSB:
            case REF:
            case RFMAB:
            case RFMPB:
            case PDE:
            case SRE:
            case PDX:
            case SRX:
            case RPD:
                Printf(PriError, "%s: Row opcodes must be used in P1 or P3:\n",
                       pTokens->GetToken(0).GetFileLine().c_str());
                pTokens->GetToken(0).PrintUnderlined(PriError);
                Printf(PriError, "\n");
                return ILWALID_FILE_FORMAT;
            case HOLD:
                CHECK_EC(EatOpcode(pTokens, false));
                CHECK_EC(EatExpression(pTokens, HI_LO_2(GetHoldCounterBits())));
                break;
            case TILA:
                CHECK_EC(EatOpcode(pTokens, HI_LO_2(GetIlaTriggerBits()), 1));
                break;
            case STOP:
                CHECK_EC(EatOpcode(pTokens, HI_LO_2(GetStopExelwtionBits()), 1));
                break;
            case NOP:
                CHECK_EC(EatOpcode(pTokens, false));
                break;
            case HACK:
                CHECK_EC(EatHackStatement(pTokens));
                break;
            case PATRAM:
                CHECK_EC(EatPatram(pTokens));
                break;
            default:
                CHECK_EC(ReportUnexpectedInput(pTokens));
                break;
        }

        // The next token after the statement should be a newline or
        // semicolon.  Eat the newline, let the caller handle the
        // semicolon, and return an error for anything else.
        //
        if (!pTokens->GetToken(0).Isa(SEMICOLON))
        {
            CHECK_EC(EatNewline(pTokens));
        }

        return ec;
    }

    //------------------------------------------------------------------
    //! \brief Processes the command (opcode) after the phase and channel.
    //! Right now, this function is called from wthin EatPhaseStatement.
    //i! Each LIT has its own implmentation to account for its unique
    //! reserved words.
    EC Galit1Pgm::EatPhaseCmd
    (
        const Token &token,
        int &cmd, 
        unsigned &numArgs, 
        bool &isColOp,
        bool &isRowOp,
        bool &isWrite,
        bool &powerDown,
        bool &isRFMSet
    ) const
    {
        EC ec = OK;

        switch (token.GetReservedWord())
        {
            case CNOP:
                cmd       = STMT_PHASE_CMD_CNOP;
                numArgs   = 0;
                isColOp   = true;
                break;
            case RD:
                cmd       = STMT_PHASE_CMD_RD;
                numArgs   = 3;
                isColOp   = true;
                break;
            case RDA:
                cmd       = STMT_PHASE_CMD_RDA;
                numArgs   = 3;
                isColOp   = true;
                break;
            case WR:
                cmd       = STMT_PHASE_CMD_WR;
                numArgs   = 3;
                isColOp   = true;
                isWrite   = true;
                break;
            case WRA:
                cmd       = STMT_PHASE_CMD_WRA;
                numArgs   = 3;
                isColOp   = true;
                isWrite   = true;
                break;
            case MRS:
                cmd       = STMT_PHASE_CMD_MRS;
                numArgs   = 2;
                isColOp   = true;
                break;
            case CPD:
                cmd       = STMT_PHASE_CMD_CPD;
                numArgs   = 0;
                isColOp   = true;
                powerDown = true;
                break;
            case RNOP:
                cmd       = STMT_PHASE_CMD_RNOP;
                numArgs   = 0;
                isRowOp   = true;
                break;
            case ACT:
                cmd       = STMT_PHASE_CMD_ACT;
                numArgs   = 2;
                isRowOp   = true;
                break;
            case PRE:
                cmd       = STMT_PHASE_CMD_PRE;
                numArgs   = 1;
                isRowOp   = true;
                break;
            case PREA:
                cmd       = STMT_PHASE_CMD_PREA;
                numArgs   = 1;
                isRowOp   = true;
                break;
            case REFSB:
                cmd       = STMT_PHASE_CMD_REFSB;
                numArgs   = 1;
                isRowOp   = true;
                break;
            case REF:
                cmd       = STMT_PHASE_CMD_REF;
                numArgs   = 1;
                isRowOp   = true;
                break;
            case PDE:
                cmd       = STMT_PHASE_CMD_PDE;
                numArgs   = 0;
                isRowOp   = true;
                powerDown = true;
                break;
            case SRE:
                cmd       = STMT_PHASE_CMD_SRE;
                numArgs   = 0;
                isRowOp   = true;
                powerDown = true;
                break;
            case PDX:
                cmd       = STMT_PHASE_CMD_PDX;
                numArgs   = 0;
                isRowOp   = true;
                break;
            case SRX:
                cmd       = STMT_PHASE_CMD_SRX;
                numArgs   = 0;
                isRowOp   = true;
                break;
            case RPD:
                cmd       = STMT_PHASE_CMD_RPD;
                numArgs   = 0;
                isRowOp   = true;
                powerDown = true;
                break;
            default:
                break; // We catch this error in the sanity check below
        }
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a P0, P1, P2, or P3 statement; called from EatStatement()
    //!
    EC Galit1Pgm::EatPhaseStatement(Tokenizer* pTokens)
    {
        EC ec = OK;

        // Eat the phase token
        //
        const int phase = pTokens->GetToken(0).GetReservedWord() - P0;
        pTokens->Pop(1);

        // Eat the channel token, if any.
        //
        int channel = 0;
        CHECK_EC(EatChannel(pTokens, &channel));

        // The phase statements all have pretty much the same format:
        // a phase (P0-P3), followed by an opcode, followed by up to 3
        // registers.  Abstract away most of the processing by setting
        // a group of variables to represent the opcode.
        //
        const Token opcode    = pTokens->GetToken(0);
        INT32       cmd       = 0;     // Value to write to the instruction word
        unsigned    numArgs   = 0;     // Number of arguments
        bool        isColOp   = false; // true if it's a column operation
        bool        isRowOp   = false; // true if it's a row operation
        bool        isWrite   = false; // true if it's a WR or WRA
        bool        powerDown = false; // true if the CKE bit should be 0
        bool        isRFMSet  = false; // true if the RFM bit should be 1
        if (opcode.Isa(Token::RESERVED_WORD))
        {
            CHECK_EC(EatPhaseCmd(opcode, cmd, 
                        numArgs, isColOp, isRowOp, 
                        isWrite, powerDown, isRFMSet));
        }

        // Sanity check to make sure that column operations happen in
        // P0 or P2, and row operations happen in P1 or P3.  This
        // section also catches cases in which the opcode is neither a
        // row nor column operation, or not even a reserved word.
        //
        switch (phase)
        {
            case 0:
            case 2:
                if (!isColOp)
                {
                    Printf(PriError,
                           "%s: Column opcodes must be used in P0 or P2:\n",
                           opcode.GetFileLine().c_str());
                    opcode.PrintUnderlined(PriError);
                    Printf(PriError, "\n");
                    return ILWALID_FILE_FORMAT;
                }
                break;
            case 1:
            case 3:
                if (!isRowOp)
                {
                    Printf(PriError,
                           "%s: Row opcodes must be used in P1 or P3:\n",
                           opcode.GetFileLine().c_str());
                    opcode.PrintUnderlined(PriError);
                    Printf(PriError, "\n");
                    return ILWALID_FILE_FORMAT;
                }
                break;
            default:
                LWDASSERT(!"Bad phase");
        }

        // Eat the opcode token.
        //
        CHECK_EC(EatOpcode(pTokens, HI_LO_2(GetPhaseCmdBits(phase, channel)),
                           cmd));

        // Eat the first arg (if any), which is always a bank register
        //
        if (numArgs >= 1)
        {
            CHECK_EC(EatRegister(pTokens,
                                 HI_LO_2(GetPhaseBankRegBits(phase, channel))));
        }

        // Eat the second arg (if any), which is always a row or
        // column register
        //
        if (numArgs >= 2)
        {
            CHECK_EC(EatRegister(pTokens,
                                 HI_LO_2(GetPhaseRowColRegBits(phase, channel))));
        }

        // Eat the third arg (if any), which is always a pattern
        // register.  This one is a little odd: the third arg only
        // exists for P0 & P2, the arg is stored in a different
        // bitfield depending on whether it's a read or write
        // operation, and the arg is not per-phase (meaning that P0 &
        // P2 must specify the same register if they're both reads or
        // both writes).
        //
        if (numArgs >= 3)
        {
            const unsigned hiBit = isWrite ? DRF_EXTENT(STMT_P02_WR_REG) :
                                             DRF_EXTENT(STMT_P02_RD_REG);
            const unsigned loBit = isWrite ? DRF_BASE(STMT_P02_WR_REG) :
                                             DRF_BASE(STMT_P02_RD_REG);
            const unsigned enBit = isWrite ? DRF_BASE(STMT_P02_WR_ENABLE) :
                                             DRF_BASE(STMT_P02_RD_ENABLE);
            const char* conflictMsg = isWrite ?
                "Writes in P0 & P2 must use the same pattern register" :
                "Reads in P0 & P2 must use the same pattern register";

            CHECK_EC(EatRegister(pTokens, hiBit, loBit, true, conflictMsg));
            CHECK_EC(UpdateStatement(opcode, enBit, enBit, 1,
                                     true, conflictMsg));
        }

        // Set the CKE bit
        CHECK_EC(UpdateStatement(opcode, HI_LO_2(GetPhaseCkeBits(phase, channel)),
                                 powerDown ? STMT_PHASE_CKE_PWR_DN
                                           : STMT_PHASE_CKE_PWR_UP));

        UpdateRFMBit(opcode, phase, channel, isRFMSet);

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a C0 or C1 statement; called from EatStatement()
    //!
    //! On platforms that don't support channels (i.e. GALIT1), we
    //! skip the channel and start with the opcode.  Thus, this method
    //! may be called if the first token is C0 or C1 on GHLIT1+, or if
    //! the first token is PDBI or CAL on GALIT1.
    //!
    EC Galit1Pgm::EatChannelStatement(Tokenizer* pTokens)
    {
        EC ec = OK;

        // Eat the channel token, if any.
        //
        int channel = 0;
        CHECK_EC(EatChannel(pTokens, &channel));

        // Eat & processes the rest of the statement
        //
        const Token opcode = pTokens->GetToken(0);
        bool isValidOpcode = false;

        if (opcode.Isa(Token::RESERVED_WORD))
        {
            switch (opcode.GetReservedWord())
            {
                case PDBI:
                {
                    CHECK_EC(EatOpcode(pTokens, false));
                    CHECK_EC(EatExpression(
                                    pTokens, GetUsePatramDBIBit(),
                                    GetUsePatramDBIBit()));
                    isValidOpcode = true;
                    break;
                }
                case CAL:
                {
                    unique_ptr<const Expression> pPseudochanExpr;
                    INT64 pseudochan;

                    CHECK_EC(EatOpcode(pTokens, false));
                    CHECK_EC(EatExpression(pTokens, &pPseudochanExpr));
                    CHECK_EC(pPseudochanExpr->EvaluateConst(this, &pseudochan));
                    CHECK_EC(pPseudochanExpr->CheckLimits(this, pseudochan,
                                                          0, 1));
                    CHECK_EC(UpdateStatement(
                            opcode,
                            HI_LO_2(GetCalBits(channel,
                                           static_cast<UINT32>(pseudochan))),
                            1));
                    isValidOpcode = true;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        // Error if opcode is invalid
        //
        if (!isValidOpcode)
        {
            Printf(PriError,
                   "%s: Expected a channel opcode such as PDBI or CAL:\n",
                   opcode.GetFileLine().c_str());
            opcode.PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a HACK statement; called from EatStatement()
    //!
    EC Galit1Pgm::EatHackStatement(Tokenizer* pTokens)
    {
        const Token opcode = pTokens->GetToken(0);
        unique_ptr<const Expression> pHiBit;
        unique_ptr<const Expression> pLoBit;
        EC ec = OK;

        CHECK_EC(EatOpcode(pTokens, true));
        CHECK_EC(EatExpression(pTokens, &pHiBit));
        CHECK_EC(EatExpression(pTokens, &pLoBit));

        INT64 hiBit;
        INT64 loBit;
        const unsigned maxBit = GetInstructionSize() - 1;
        CHECK_EC(pHiBit->EvaluateConst(this, &hiBit));
        CHECK_EC(pLoBit->EvaluateConst(this, &loBit));
        CHECK_EC(pHiBit->CheckLimits(this, hiBit, 0, maxBit));
        CHECK_EC(pLoBit->CheckLimits(this, loBit, 0, maxBit));

        if (hiBit < loBit)
        {
            Printf(PriError, "%s: First arg must be >= second arg:\n",
                   opcode.GetFileLine().c_str());
            Printf(PriError, "%s\n", ToString(opcode.GetLine()).c_str());
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        CHECK_EC(EatExpression(pTokens,
                               static_cast<unsigned>(hiBit),
                               static_cast<unsigned>(loBit)));
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a PATRAM statement; called from EatStatement()
    //!
    EC Galit1Pgm::EatPatram(Tokenizer* pTokens)
    {
        EC ec = OK;

        const Token& opcode = pTokens->GetToken(0);
        for (Thread* pThread: GetSelectedThreads())
        {
            FIRST_EC(pThread->EnsurePendingPatram(opcode));
            pThread->GetPendingPatram()->AppendOpcode(opcode);
        }
        pTokens->Pop(1);

        FIRST_EC(EatExpression(pTokens, 32, 0, CodeExpression::DQ));
        FIRST_EC(EatExpression(pTokens,  4, 0, CodeExpression::ECC));
        FIRST_EC(EatExpression(pTokens,  4, 0, CodeExpression::DBI));
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a programming unit that starts with a symbol
    //!
    //! This method more-or-less eats a line that starts with a
    //! symbol.
    //!
    //! I say "more-or-less" because, in some cases, there was other
    //! stuff in the line before the symbol, but the code decided to
    //! let this method handle the remainder of the line.  The most
    //! common example is a statement followed by a semicolon.
    //!
    EC Galit1Pgm::EatSymbol(Tokenizer* pTokens)
    {
        LWDASSERT(pTokens->GetToken(0).Isa(Token::SYMBOL));
        const Token& token = pTokens->GetToken(0);
        EC ec = OK;

        switch (token.GetSymbol())
        {
            case DOT:
                CHECK_EC(EatDirective(pTokens));
                break;
            case SEMICOLON:
                for (Thread* pThread: GetSelectedThreads())
                {
                    FIRST_EC(pThread->ClosePendingOperation(token, NOP));
                }
                pTokens->Pop(1);
                FIRST_EC(EatNewline(pTokens));
                break;
            case NEWLINE:
                pTokens->Pop(1);
                break;
            default:
                CHECK_EC(ReportUnexpectedInput(pTokens));
                break;
        }
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a directive
    //!
    //! A directive is parsed as a DOT (".") token followed
    //! immediately by an identifier.  This method ensures that the
    //! identifier matches one of the supported directives, processes
    //! it, and eats everything up to the terminating newline.  The
    //! caller must ensure that the next token is a DOT.
    //!
    EC Galit1Pgm::EatDirective(Tokenizer* pTokens)
    {
        LWDASSERT(pTokens->GetToken(0).Isa(DOT));
        const Token& dotToken = pTokens->GetToken(0);
        const Token& dirToken = pTokens->GetToken(1);
        EC ec = OK;

        // Find out which directive this is, if any.
        //
        enum Directive
        {
            FBPA_SUBP_MASK,
            USE_PATRAM_DBI,
            MAX_CYCLES
        };
        static const unordered_map<string, Directive> directives =
        {
            { "FBPA_SUBP_MASK", FBPA_SUBP_MASK },
            { "USE_PATRAM_DBI", USE_PATRAM_DBI },
            { "MAX_CYCLES",     MAX_CYCLES     }
        };
        const auto pDirective = directives.find(ToUpper(dirToken.GetText()));

        // Return an error if the DOT is not immediately followed by a
        // valid directive word, with no spaces in between.
        //
        if (!dirToken.Isa(Token::IDENTIFIER) ||
            !dotToken.IsAdjacent(dirToken) ||
            pDirective == directives.end())
        {
            Printf(PriError, "%s: Unexpected symbol:\n",
                   dotToken.GetFileLine().c_str());
            dotToken.PrintUnderlined(PriError);
            Printf(PriError,
                "Do you mean .FBPA_SUBP_MASK, .USE_PATRAM_DBI, or .MAXCYCLES?\n");
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        // Pop the DOT and directive identifier
        //
        pTokens->Pop(2);

        // Process the directive args
        //
        switch (pDirective->second)
        {
            case FBPA_SUBP_MASK:
            {
                // Loop through all "fbpaMask subpMask" pairs in the
                // line, and use them to construct a ThreadMask.
                //
                ThreadMask threadMask(GetThreadMaskSize());
                do
                {
                    // Eat one "fbpaMask subpMask" pair
                    //
                    const unsigned maxFbpas = GetMaxFbpas();

                    unique_ptr<const Expression> pFbpaMask;
                    unique_ptr<const Expression> pSubpMask;
                    CHECK_EC(EatExpression(pTokens, &pFbpaMask));
                    CHECK_EC(EatExpression(pTokens, &pSubpMask));

                    INT64 fbpaMask;
                    INT64 subpMask;
                    CHECK_EC(pFbpaMask->EvaluateConst(this, &fbpaMask));
                    CHECK_EC(pSubpMask->EvaluateConst(this, &subpMask));

                    // Check for overflow
                    //
                    if (Bits::IsOverflow64(maxFbpas, 1, fbpaMask))
                    {
                        const Token& firstToken = pFbpaMask->GetFirstToken();
                        const Token& lastToken  = pFbpaMask->GetLastToken();
                        Printf(PriError, "%s: FBPA mask %s:\n",
                               firstToken.GetFileLine().c_str(),
                               Bits::GetOverflow64(maxFbpas, 1,
                                                   fbpaMask).c_str());
                        firstToken.PrintUnderlined(PriError, lastToken);
                        Printf(PriError, "\n");
                        return ILWALID_FILE_FORMAT;
                    }
                    if (Bits::IsOverflow64(2, 1, subpMask))
                    {
                        const Token& firstToken = pSubpMask->GetFirstToken();
                        const Token& lastToken  = pSubpMask->GetLastToken();
                        Printf(PriError, "%s: SUBP mask %s:\n",
                               firstToken.GetFileLine().c_str(),
                               Bits::GetOverflow64(2, 1, subpMask).c_str());
                        firstToken.PrintUnderlined(PriError, lastToken);
                        Printf(PriError, "\n");
                        return ILWALID_FILE_FORMAT;
                    }

                    // Set bits in threadMask indicated by fbpaMask
                    // and subpMask.
                    //
                    for (unsigned fbpa = 0; fbpa < maxFbpas; ++fbpa)
                    {
                        if (((fbpaMask >> fbpa) & 1) != 0)
                        {
                            for (unsigned subp = 0; subp < 2; ++subp)
                            {
                                if (((subpMask >> subp) & 1) != 0)
                                {
                                    threadMask[2 * fbpa + subp] = true;
                                }
                            }
                        }
                    }
                } while (!pTokens->GetToken(0).Isa(NEWLINE) &&
                         !pTokens->GetToken(0).Isa(Token::END_OF_FILE));

                // Finally, use the combined mask to select which
                // threads are affected by subsequent statements.
                //
                SetSelectedThreads(threadMask);
                break;
            }
            case USE_PATRAM_DBI:
            {
                // Eat the boolean expression, and use it to set the
                // UsePatramDbi flag in each selected thread.
                //
                unique_ptr<const Expression> pUsePatramDbi;
                CHECK_EC(EatExpression(pTokens, &pUsePatramDbi));
                for (Thread* pThread: GetSelectedThreads())
                {
                    INT64 usePatramDbi;
                    CHECK_EC(pUsePatramDbi->Evaluate(pThread, &usePatramDbi));
                    CHECK_EC(pUsePatramDbi->CheckLimits(this, usePatramDbi,
                                                        0, 1));
                    pThread->SetUsePatramDbi(usePatramDbi != 0);
                }
                break;
            }
            case MAX_CYCLES:
            {
                // Eat the integer expression, and use it to set the
                // MaxCycles value in each selected thread.  Do not
                // set MaxCycles in a thread to two different values.
                //
                unique_ptr<const Expression> pMaxCycles;
                CHECK_EC(EatExpression(pTokens, &pMaxCycles));
                for (Thread* pThread: GetSelectedThreads())
                {
                    if (pThread->GetMaxCycles() != 0)
                    {
                        const Token& firstToken = pMaxCycles->GetFirstToken();
                        const Token& lastToken  = pMaxCycles->GetLastToken();
                        Printf(PriError,
                               "%s: .MAX_CYCLES was already set to %lld:\n",
                               firstToken.GetFileLine().c_str(),
                               pThread->GetMaxCycles());
                        firstToken.PrintUnderlined(PriError, lastToken);
                        Printf(PriError, "\n");
                        return ILWALID_FILE_FORMAT;
                    }
                    INT64 maxCycles;
                    CHECK_EC(pMaxCycles->Evaluate(pThread, &maxCycles));
                    pThread->SetMaxCycles(maxCycles);
                }
                break;
            }
        }

        // The directive should end with a newline.
        //
        CHECK_EC(EatNewline(pTokens));

        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a label of the form "identifier COLON"
    //!
    //! Note that other statements may follow the label (even other
    //! labels!), so do not try to eat a newline after the label.
    //!
    EC Galit1Pgm::EatLabel(Tokenizer* pTokens)
    {
        LWDASSERT(pTokens->GetToken(0).Isa(Token::IDENTIFIER));
        EC ec = OK;

        if (!pTokens->GetToken(1).Isa(COLON))
        {
            return ReportUnexpectedInput(pTokens);
        }

        for (Thread* pThread: GetSelectedThreads())
        {
            CHECK_EC(pThread->AddLabel(pTokens->GetToken(0)));
        }

        pTokens->Pop(2);
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a register token
    //!
    //! Store the register in the pending Statement.  The hiBit,
    //! loBit, allowSharing, and conflictMsg args are the same as for
    //! Program::UpdateStatement().
    //!
    EC Galit1Pgm::EatRegister
    (
        Tokenizer*  pTokens,
        unsigned    hiBit,
        unsigned    loBit,
        bool        allowSharing,
        const char* conflictMsg
    )
    {
        const Token& token = pTokens->GetToken(0);
        EC ec = OK;

        if (!IsRegister(token))
        {
            Printf(PriError, "%s: Expected a register:\n",
                   token.GetFileLine().c_str());
            token.PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }

        CHECK_EC(UpdateStatement(token, hiBit, loBit,
                                 token.GetReservedWord() - R0,
                                 allowSharing, conflictMsg));
        pTokens->Pop(1);
        return ec;
    }

    //----------------------------------------------------------------
    //! \brief Eat a newline token
    //!
    EC Galit1Pgm::EatNewline(Tokenizer* pTokens)
    {
        const Token& token = pTokens->GetToken(0);
        if (!token.Isa(NEWLINE))
        {
            Printf(PriError, "%s: Unexpected input at end of line:\n",
                   token.GetFileLine().c_str());
            token.PrintUnderlined(PriError);
            Printf(PriError, "\n");
            return ILWALID_FILE_FORMAT;
        }
        pTokens->Pop(1);
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Verify that LOAD and INCR do not update the same register
    //!
    //! It is illegal to load a register (LOAD, LDI, LDPRBS) in the
    //! same instruction that does an INCR on the same register.  This
    //! method checks for that.
    //!
    //! This method should be called after a register-load or INCR
    //! Statement has been added.  It prints an error message and
    //! returns ILWALID_FILE_FORMAT if the pending Statement conflicts
    //! with previous Statements.
    //!
    EC Galit1Pgm::CheckForLoadAndIncr()
    {
        // Loop through all selected threads
        //
        for (Thread* pThread: GetSelectedThreads())
        {
            // Find the register used in the register-load instruction
            // (if any)
            //
            const Instruction* pInstruction = pThread->GetPendingInstruction();
            const Bits& instBits = pInstruction->GetNonHackBits();
            const INT32 regNum   = instBits.GetBits(HI_LO(STMT_REG_LOAD_INDEX));

            // If there is no conflict over that register, then
            // continue to the next Thread
            //
            if (!instBits.GetBit(DRF_BASE(STMT_REG_LOAD_EN)) ||
                !instBits.GetBit(DRF_BASE(STMT_INCR_MASK(regNum))))
            {
                continue;
            }

            // Print the error message and return an error
            //
            if (Uniq(__FILE__, __LINE__))
            {
                // Print the first part of the error message, showing
                // the pending (most recently added) statement
                //
                const Statement& pendStmt =
                    pInstruction->GetStatements().back();
                Printf(PriError,
                    "%s: Attempting to load and increment the same register:\n",
                    pendStmt.GetOpcode().GetFileLine().c_str());
                Printf(PriError, "%s\n",
                       ToString(pendStmt.GetOpcode().GetLine()).c_str());

                // Find the previous Statement that conflicts with the
                // pending Statement, and add that to the error
                // message.  If the pending Statement was a
                // register-load, then only show a previous INCR
                // Statement, and vice-versa.
                //
                for (const Statement& prevStmt: pInstruction->GetStatements())
                {
                    if (prevStmt.IsHack())
                    {
                        continue;
                    }
                    if ((pendStmt.GetBit(DRF_BASE(STMT_REG_LOAD_EN)) &&
                         prevStmt.GetBit(DRF_BASE(STMT_INCR_MASK(regNum)))) ||
                        (pendStmt.GetBit(DRF_BASE(STMT_INCR_MASK(regNum))) &&
                         prevStmt.GetBit(DRF_BASE(STMT_REG_LOAD_EN)) &&
                         prevStmt.GetBits(HI_LO(STMT_REG_LOAD_INDEX)) == regNum))
                    {
                        Printf(PriError, "%s: Previous statement was here:\n",
                               prevStmt.GetOpcode().GetFileLine().c_str());
                        Printf(PriError, "%s\n",
                               ToString(prevStmt.GetOpcode().GetLine()).c_str());
                        break;
                    }
                }
                Printf(PriError, "\n");
            }
            return ILWALID_FILE_FORMAT;
        }

        // If we get here, then there is no conflict
        //
        return OK;
    }

    //----------------------------------------------------------------
    //! \brief Return true if the token argument is a register
    //!
    bool Galit1Pgm::IsRegister(const Token& token)
    {
        return (token.Isa(Token::RESERVED_WORD) &&
                token.GetReservedWord() >= R0 &&
                token.GetReservedWord() <= R15);
    }
}
