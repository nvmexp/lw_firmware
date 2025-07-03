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

#include "mucc.h"
#include "muccghlit1.h"
#include "hopper/gh100/hwproject.h"
using namespace LwDiagUtils;

namespace Mucc
{
    //----------------------------------------------------------------
    //! \brief Allocate a Galit1Pgm object; used by Launcher::AllocProgram()
    //!
    unique_ptr<Program> Launcher::AllocGhlit1Pgm() const
    {
        return make_unique<Ghlit1Pgm>(*this);
    }

    //----------------------------------------------------------------
    //! \brief Store the default instruction word in a Bits object
    //!
    //! Most bits in an instruction word are 0 until explicitly set to
    //! 1, but a small handful should be 1 until explicitly set to 0.
    //! This method sets those default bits.
    //!
    void Ghlit1Pgm::SetDefaultInstruction
    (
        const Thread& thread,
        Bits*         pBits
    )
    const
    {
        Galit1Pgm::SetDefaultInstruction(thread, pBits);

        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(0, 1)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(1, 1)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(2, 1)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefaults(HI_LO_2(GetPhaseCkeBits(3, 1)), STMT_PHASE_CKE_PWR_UP);
        pBits->SetDefault(GetUsePatramDBIBit(),
                          thread.GetUsePatramDbi());
    }

    //----------------------------------------------------------------
    //! \brief Return the maximum number of FBPAs for this litter
    //!
    unsigned Ghlit1Pgm::GetMaxFbpas() const
    {
        return LW_SCAL_LITTER_NUM_FBPAS;
    }

    //----------------------------------------------------------------
    //! \brief Eat a channel token
    //!
    //! Store the channel in the pChannel arg.
    //!
    EC Ghlit1Pgm::EatChannel(Tokenizer* pTokens, int* pChannel) const
    {
        const Token& token = pTokens->GetToken(0);
        const bool isChannel = (token.Isa(Token::RESERVED_WORD) &&
                                token.GetReservedWord() >= C0 &&
                                token.GetReservedWord() <= C1);
        EC ec = OK;

        // if no channel is specified we'd assume to be channel 0.
        // This is done for HBM2 compatibility.
        if (!isChannel)
        {
            *pChannel = 0;
        }
        else
        {
            *pChannel = token.GetReservedWord() - C0;
            pTokens->Pop(1);
        }

        return ec;
    }

    //-----------------------------------------------------------
    //! \brief Eat the phase cmd 
    //! 
    EC Ghlit1Pgm::EatPhaseCmd
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
            case RFMAB:
                cmd     = STMT_PHASE_CMD_RFMAB;
                numArgs = 1;
                isRowOp = true;
                isRFMSet = true;
                break;
            case RFMPB:
                cmd     = STMT_PHASE_CMD_RFMPB;
                numArgs = 1;
                isRowOp = true;
                isRFMSet = true;
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
                break; 
        }

        return ec;
    }

    //---------------------------------------------------------------------------
    //! \brief update the rfm bit of instruction opcode.
    //!
    EC Ghlit1Pgm::UpdateRFMBit(const Token &token, int phase, int channel, bool isRFMSet)
    {
        EC ec = OK;
        if (isRFMSet)
        {
            CHECK_EC(UpdateStatement(token, HI_LO_2(GetPhaseRfmBits(phase, channel)),
                        STMT_PHASE_RFM_SET));
        }

        return ec;
    }
}
