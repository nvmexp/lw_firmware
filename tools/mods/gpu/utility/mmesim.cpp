/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011,2015-2016,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   mmesim.cpp
 * @brief  Implemetation of Method Macro Expander simulator and
 *         Shadow RAM simulation
 *
 */

#include "mmesim.h"
#include "core/include/rc.h"
#include "mmeigen.h"
#include "core/include/massert.h"
#include <memory>

#define LW_MME_RELEASE_METHOD       11:0
#define LW_MME_RELEASE_SUBCH       14:12
#define LW_MME_RELEASE_PC          26:16
#define LW_MME_RELEASE_MME_GEN     27:27

// Upgraded defines for pascal
#define LW_MME_RELEASE_PC_V2       27:16
#define LW_MME_RELEASE_MME_GEN_V2  28:28
//----------------------------------------------------------------------------
//! \brief Constructor
//!
MMEShadowSimulator::MMEShadowSimulator(const GpuSubdevice::MmeMethods mmeMethods)
{
    GpuSubdevice::MmeMethods::const_iterator pMmeMethod = mmeMethods.begin();
    ShadowEntry initialEntry = { false, 0 };

    // Initialize the shadow data depending upon the shadowed methods
    for ( ; pMmeMethod != mmeMethods.end(); pMmeMethod++)
    {
        // If the method is a shared method
        if (pMmeMethod->second)
        {
            map<GpuSubdevice::MmeMethod, UINT32>::iterator pSharedMethod;
            bool bFoundShared = false;

            // Search through all current shared methods to see if this method
            // has already been shared
            pSharedMethod = m_SharedMethodIndexes.begin();
            while ((!bFoundShared) &&
                   (pSharedMethod != m_SharedMethodIndexes.end()))
            {
                if (pSharedMethod->first.ShadowMethod ==
                    pMmeMethod->first.ShadowMethod)
                {
                    bFoundShared = true;
                }
                else
                {
                    pSharedMethod++;
                }
            }

            // If the method has already been shared, create a shared method
            // index entry that points to the data location, otherwise create
            // a new shared data entry and assign it
            if (bFoundShared)
            {
                m_SharedMethodIndexes[pMmeMethod->first] =
                    pSharedMethod->second;
            }
            else
            {
                m_SharedMethodIndexes[pMmeMethod->first] =
                    (UINT32)m_SharedData.size();
                m_SharedData.push_back(initialEntry);
            }
        }
        else
        {
            // Create a non shared entry
            m_MethodData[pMmeMethod->first] = initialEntry;
        }
    }
}

//----------------------------------------------------------------------------
//! \brief Destructor
//!
MMEShadowSimulator::~MMEShadowSimulator()
{
    m_MethodData.clear();
    m_SharedData.clear();
    m_SharedMethodIndexes.clear();
    m_RollbackData.clear();
    m_RollbackSharedData.clear();
    m_ValidRollbackTokens.clear();
}

//----------------------------------------------------------------------------
//! \brief Set the specified shadowed method to the specific value
//!
//! \param Method : Method to set
//! \param Data   : Data to use
//!
void MMEShadowSimulator::SetValue(const GpuSubdevice::MmeMethod Method,
                                  const UINT32 Data)
{
    // If the method is not shared, then this function is a NOP
    if (m_MethodData.count(Method))
    {
        if (m_RollbackData.size() && !m_RollbackData.back().count(Method))
        {
            m_RollbackData.back()[Method] = m_MethodData[Method];
        }
        m_MethodData[Method].Value = Data;
        m_MethodData[Method].bWritten = true;
    }
    else if (m_SharedMethodIndexes.count(Method))
    {
        if (m_RollbackSharedData.size() &&
            !m_RollbackSharedData.back().count(m_SharedMethodIndexes[Method]))
        {
            m_RollbackSharedData.back()[m_SharedMethodIndexes[Method]] =
                m_SharedData[m_SharedMethodIndexes[Method]];
        }
        m_SharedData[m_SharedMethodIndexes[Method]].Value = Data;
        m_SharedData[m_SharedMethodIndexes[Method]].bWritten = true;
    }
}

//----------------------------------------------------------------------------
//! \brief Get the the value for the specified shadowed method
//!
//! \param Method : Method to get
//!
//! \return Value for the specified method
//!
UINT32 MMEShadowSimulator::GetValue(const GpuSubdevice::MmeMethod Method)
{
    UINT32 data = 0;

    // If the method is not shadowed or unwritten it will always return
    // zero (same function as hardware).
    if (m_MethodData.count(Method))
    {
        if (m_MethodData[Method].bWritten)
        {
            data = m_MethodData[Method].Value;
        }
        else
        {
            MASSERT(!"Unwritten shadow method");
        }
    }
    else if (m_SharedMethodIndexes.count(Method))
    {
        if (m_SharedData[m_SharedMethodIndexes[Method]].bWritten)
        {
            data = m_SharedData[m_SharedMethodIndexes[Method]].Value;
        }
        else
        {
            MASSERT(!"Unwritten shadow method");
        }
    }

    return data;
}

//----------------------------------------------------------------------------
//! \brief Check if a particular shadow method has ever been written
//!
//! \param Method : Method to check
//!
//! \return true if the value has been written
//!
bool MMEShadowSimulator::IsValueWritten(const GpuSubdevice::MmeMethod method)
{
    if (m_MethodData.count(method))
    {
        return m_MethodData[method].bWritten;
    }
    else if (m_SharedMethodIndexes.count(method))
    {
        return m_SharedData[m_SharedMethodIndexes[method]].bWritten;
    }

    MASSERT(!"Unknown shadow method");
    return false;
}

//----------------------------------------------------------------------------
//! \brief Start a rollback point for the shadow simulator.
//!
//! \return Token used to roll back to the specified position
INT32 MMEShadowSimulator::SetRollbackPoint()
{
    INT32 rollbackToken = static_cast<INT32>(m_RollbackData.size());
    map<GpuSubdevice::MmeMethod, ShadowEntry> m_EmptyRollbackData;
    map<UINT32, ShadowEntry> m_EmptyRollbackSharedData;

    m_RollbackData.push_back(m_EmptyRollbackData);
    m_RollbackSharedData.push_back(m_EmptyRollbackSharedData);
    m_ValidRollbackTokens.insert(rollbackToken);

    return rollbackToken;
}

//----------------------------------------------------------------------------
//! \brief Rollback the shadow simulator data to the previous rollback point
//!
//! \param rollbackToken : Rollback to the point captured by the token
//!
//! \return OK if successful, not OK otherwise
RC MMEShadowSimulator::Rollback(INT32 rollbackToken)
{
    if (rollbackToken == ROLLBACK_NONE)
        return OK;

    if (!m_ValidRollbackTokens.count(rollbackToken))
        return RC::SOFTWARE_ERROR;

    for (INT32 idx = static_cast<INT32>(m_RollbackData.size()) - 1; idx >= rollbackToken; idx--)
    {
        map<GpuSubdevice::MmeMethod, ShadowEntry>::iterator pRollbackData;
        for (pRollbackData = m_RollbackData[idx].begin();
             pRollbackData != m_RollbackData[idx].end(); pRollbackData++)
        {
            m_MethodData[pRollbackData->first] = pRollbackData->second;
        }

        map<UINT32, ShadowEntry>::iterator pRollbackSharedData;
        for (pRollbackSharedData = m_RollbackSharedData[idx].begin();
             pRollbackSharedData != m_RollbackSharedData[idx].end();
             pRollbackSharedData++)
        {
            m_SharedData[pRollbackSharedData->first] =
                pRollbackSharedData->second;
        }
    }

    INT32 rollbackCount = (INT32)m_RollbackData.size() - rollbackToken;
    for (INT32 idx = 0; idx < rollbackCount; idx++)
    {
        m_RollbackData.pop_back();
        m_RollbackSharedData.pop_back();
        m_ValidRollbackTokens.erase(rollbackToken + idx);
    }

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Rollback the shadow simulator data to the previous rollback point
//!
//! \param rollbackToken : Rollback to the point captured by the token
//!
//! \return OK if successful, not OK otherwise
RC MMEShadowSimulator::ForgetRollbackPoint(INT32 RollbackToken)
{
    if (RollbackToken == ROLLBACK_NONE)
        return OK;

    if (!m_ValidRollbackTokens.count(RollbackToken))
        return RC::SOFTWARE_ERROR;

    INT32 mergeIndex = RollbackToken - 1;
    while ((mergeIndex >= 0) && !m_ValidRollbackTokens.count(mergeIndex))
    {
        mergeIndex--;
    }

    if (mergeIndex < 0)
    {
        m_RollbackData[RollbackToken].clear();
        m_RollbackSharedData[RollbackToken].clear();
    }
    else
    {
        map<GpuSubdevice::MmeMethod, ShadowEntry>::iterator pRollbackData;
        for (pRollbackData = m_RollbackData[RollbackToken].begin();
             pRollbackData != m_RollbackData[RollbackToken].end();
             pRollbackData++)
        {
            if (!m_RollbackData[mergeIndex].count(pRollbackData->first))
            {
                m_RollbackData[mergeIndex][pRollbackData->first] =
                    pRollbackData->second;
            }
        }

        map<UINT32, ShadowEntry>::iterator pRollbackSharedData;
        for (pRollbackSharedData = m_RollbackSharedData[RollbackToken].begin();
             pRollbackSharedData != m_RollbackSharedData[RollbackToken].end();
             pRollbackSharedData++)
        {
            if (!m_RollbackSharedData[mergeIndex].count(pRollbackSharedData->first))
            {
                m_RollbackSharedData[mergeIndex][pRollbackSharedData->first] =
                    pRollbackSharedData->second;
            }
        }
    }
    m_ValidRollbackTokens.erase(RollbackToken);

    INT32 lwrRollbackToken = (INT32)m_RollbackData.size() - 1;
    while (!m_ValidRollbackTokens.count(lwrRollbackToken) &&
           (lwrRollbackToken >= RollbackToken))
    {
        m_RollbackData.pop_back();
        m_RollbackSharedData.pop_back();
        lwrRollbackToken--;
    }

    if (m_ValidRollbackTokens.empty())
    {
        m_RollbackData.clear();
        m_RollbackSharedData.clear();
    }

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Constructor
//!
MMESimulator::MMESimulator(MmeInstGenerator * pMmeIGen, UINT32 version)
:   m_pMmeIGen(pMmeIGen),
    m_pStateInput(nullptr),
    m_TracePri(Tee::PriNone),
    m_Cycles(0),
    m_Method(0),
    m_MethodInc(0),
    m_Carry(0),
    m_LwrData(0),
    m_SubCh(0),
    m_Class(0),
    m_MethodShift(0),
    m_bIsError(false),
    m_Version(version)
{
    MASSERT(m_pMmeIGen);
    MASSERT(m_Version <= 2); // Invalid mme version
}

//----------------------------------------------------------------------------
//! \brief Run the MME program
//!
//! \param subCh              : Subchannel number where the macro will be run
//! \param subChClass         : Class of the object on the subchannel
//! \param Instructions       : Instructions for the macro
//! \param InputData          : Input data for the macro
//! \param pStateInput        : Pointer to the shadow state
//! \param bUpdateShadowState : True if the shadow state should be changed
//! \param BasePC             : PC of the first instruction of the macro
//! \param pOutputData        : Pointer to returned output data
//!
//! \return OK if the macro was successfully run, not ok otherwise
//!
RC MMESimulator::RunProgram(const UINT32 subCh,
                            const UINT32 subChClass,
                            const vector<UINT32> &Instructions,
                            const vector<UINT32> &InputData,
                            MMEShadowSimulator * const pStateInput,
                            const UINT32 BasePC,
                            vector<UINT32> *pOutputData)
{
    RC    rc;
    INT32 lwrPC = 0;
    INT32 nextPC = 1;
    UINT32 idx;

    m_SubCh = subCh;
    m_Class = subChClass;
    m_pStateInput = pStateInput;

    m_LwrData = 0;
    m_Cycles = 0;

    m_WarningCategory = "";
    m_WarningMessage = "";

    m_bIsError = false;

    m_Method = m_MethodInc = 0;

    // Init the regfile to garbage
    m_RegFile.resize(m_pMmeIGen->GetRegCount());
    for(idx = 0; idx < m_pMmeIGen->GetRegCount(); idx++)
    {
        m_RegFile[idx].Value = 0xdeadbeef;
        m_RegFile[idx].CycleReady = 0;
        m_RegFile[idx].bWritten = false;
    }

    // Reg0 is always 0 and always ready
    m_RegFile[0].Value = 0;
    m_RegFile[0].bWritten = true;
    m_RegFile[0].CycleReady = -1;

    // First load parameter is in R1
    m_RegFile[1].Value = Load(InputData);
    m_RegFile[1].bWritten = true;
    m_RegFile[1].CycleReady = -1;

    // While exectution is not complete
    while (lwrPC != END_PC)
    {
        if (lwrPC != SKIP_PC)
        {
            UINT32 inst = Instructions[lwrPC];
            UINT32 res;
            bool bHasMux = true;
            bool bTaken = false;
            bool bNoDelaySlot = false;
            INT32 target = 0;
            UINT32 writeLatency = 1;
            bool bInDelay = ((nextPC > 0) && (nextPC != (lwrPC + 1)));

            Printf(m_TracePri, "MMESimTrace: PC: %d\n", BasePC + lwrPC);
            Printf(m_TracePri, "MMESimTrace: ucode: 0x%08x\n", inst);

            switch (m_pMmeIGen->GetOpCode(inst))
            {
                default:
                    SetError("Invalid opcode");
                    return RC::SOFTWARE_ERROR;
                case MmeInstGenerator::OP_BIN:
                    res = 0;
                    CHECK_RC(HandleBinaryOp(inst, &res));
                    break;
                case MmeInstGenerator::OP_ADDI:
                    // Do an add, but don't touch the carry
                    res = Add(Reg0(inst),
                              (UINT32)m_pMmeIGen->GetImmed(inst),
                              false);
                    Printf(m_TracePri,
                           "MMESimTrace: 0x%08x + 0x%08x = 0x%08x carry %d\n",
                           Reg0(inst),
                           (UINT32)m_pMmeIGen->GetImmed(inst),
                           res,
                           m_Carry);
                    break;
                case MmeInstGenerator::OP_INSERT:
                    res = Insert(Reg0(inst), Reg1(inst),
                                 m_pMmeIGen->GetSrcBit(inst),
                                 m_pMmeIGen->GetDstBit(inst),
                                 m_pMmeIGen->GetWidth(inst));
                    break;
                case MmeInstGenerator::OP_INSERTSB0:
                    res = Insert(0, Reg1(inst), Reg0(inst),
                                 m_pMmeIGen->GetDstBit(inst),
                                 m_pMmeIGen->GetWidth(inst));
                    break;
                case MmeInstGenerator::OP_INSERTDB0:
                    res = Insert(0, Reg1(inst), m_pMmeIGen->GetSrcBit(inst),
                                 Reg0(inst), m_pMmeIGen->GetWidth(inst));
                    break;
                case MmeInstGenerator::OP_STATEI:
                    res = GetState(Reg0(inst) + m_pMmeIGen->GetImmed(inst));
                    Printf(m_TracePri,
                           "MMESimTrace: 0x%08x = 0x%08x + 0x%08x\n",
                           (INT32)Reg0(inst) + m_pMmeIGen->GetImmed(inst),
                           Reg0(inst), (UINT32)m_pMmeIGen->GetImmed(inst));
                    writeLatency = 5;
                    {
                        if (m_pMmeIGen->GetMux(inst) !=
                                MmeInstGenerator::MUX_RNN)
                        {
                            SetError("Invalid Mux in STATEI Instruction");
                            return RC::SOFTWARE_ERROR;
                        }
                    }
                    break;
                case MmeInstGenerator::OP_BRA:
                    if (bInDelay)
                    {
                        SetError("Branch not allowed in delay slot");
                        return RC::SOFTWARE_ERROR;
                    }
                    bHasMux = false;
                    bNoDelaySlot = m_pMmeIGen->GetBranchNoDelaySlot(inst);

                    bTaken = (Reg0(inst) == 0);
                    if (m_pMmeIGen->GetBranchNotZero(inst))
                        bTaken = !bTaken;

                    target = m_pMmeIGen->GetBranchTarget(inst);
                    Printf(m_TracePri,
                          "MMESimTrace: %s 0x%08x %s offset %d %sdelay slot\n",
                          m_pMmeIGen->GetBranchNotZero(inst) ? "BNZ" : "BZ",
                          Reg0(inst),
                          bTaken ? "taken":"not taken",
                          target,
                          bNoDelaySlot ? "no " : "");
                    break;
            }

            // If the instruction has a MUX
            if (bHasMux)
            {
                MmeInstGenerator::MuxOp muxOp = m_pMmeIGen->GetMux(inst);

                PrintMux(muxOp);

                // Determine what the destination value will be
                UINT32 dst = res;
                switch (muxOp)
                {
                    default:
                        SetError("Internal error - mux\n");
                        return RC::SOFTWARE_ERROR;
                    case MmeInstGenerator::MUX_LNN:
                    case MmeInstGenerator::MUX_LRN:
                    case MmeInstGenerator::MUX_LNR:
                        dst = Load(InputData);
                        break;
                    case MmeInstGenerator::MUX_RNN:
                    case MmeInstGenerator::MUX_RNR:
                    case MmeInstGenerator::MUX_RRN:
                    case MmeInstGenerator::MUX_RLR:
                    case MmeInstGenerator::MUX_RRR:
                        dst = res;
                        break;
                }

                // Update the destination value (do not change R0 since it is a
                // special register that always returns 0
                UINT32 destReg = m_pMmeIGen->GetDstReg(inst);
                Printf(m_TracePri,
                       "MMESimTrace: Writing 0x%08x to R%d\n", dst, destReg);
                if (destReg)
                {
                    m_RegFile[destReg].Value = dst;
                    m_RegFile[destReg].bWritten = true;
                    m_RegFile[destReg].CycleReady = m_Cycles + writeLatency;
                }

                // Determine how the method register will change due to the MUX
                switch (muxOp)
                {
                    default:
                        SetError("Internal error - bad mux\n");
                        return RC::SOFTWARE_ERROR;
                    case MmeInstGenerator::MUX_LNN:
                    case MmeInstGenerator::MUX_LRN:
                    case MmeInstGenerator::MUX_RNN:
                    case MmeInstGenerator::MUX_RRN:
                        break;
                    case MmeInstGenerator::MUX_LNR:
                    case MmeInstGenerator::MUX_RNR:
                    case MmeInstGenerator::MUX_RLR:
                    case MmeInstGenerator::MUX_RRR:
                        m_Method = m_pMmeIGen->GetMethod(res);
                        m_MethodInc = m_pMmeIGen->GetMethodInc(res);
                        Printf(m_TracePri,
                            "MMESimTrace: Method settings now 0x%08x 0x%04x\n",
                            m_Method, m_MethodInc);
                        break;
                }

                // Determine what (if anything) will be released
                bool bRelease = true;
                UINT32 releaseVal = res;
                switch (muxOp) {
                    default:
                        SetError("Internal error - bad mux\n");
                        return RC::SOFTWARE_ERROR;
                    case MmeInstGenerator::MUX_LNN:
                    case MmeInstGenerator::MUX_RNN:
                    case MmeInstGenerator::MUX_LNR:
                    case MmeInstGenerator::MUX_RNR:
                        bRelease = false;
                        break;
                    case MmeInstGenerator::MUX_LRN:
                    case MmeInstGenerator::MUX_RRN:
                        break;
                    case MmeInstGenerator::MUX_RRR:
                        releaseVal = m_pMmeIGen->GetMethodInc(res);
                        break;
                    case MmeInstGenerator::MUX_RLR:
                        releaseVal = Load(InputData);
                        break;
                }

                // Release data to the method stream and increment the method
                // registers as necessary
                if (bRelease && !m_bIsError)
                {
                    Printf(m_TracePri,
                           "MMESimTrace: Releasing 0x%08x to 0x%08x\n",
                           releaseVal, m_Method);
                    Release(pOutputData, BasePC + lwrPC, m_Method, releaseVal);
                    m_Method += m_MethodInc;
                    m_Method = m_pMmeIGen->GetMethod(m_Method);
                }
            }

            // Determine the next PC value
            if (bTaken)
            {
                nextPC = lwrPC + target;
                if (bNoDelaySlot)
                    lwrPC = SKIP_PC;
                else
                    lwrPC++;
            }
            else if (m_pMmeIGen->GetEndingNext(inst) && !bInDelay)
            {
                lwrPC = nextPC;
                nextPC = END_PC;
            }
            else
            {
                lwrPC = nextPC;
                nextPC++;
            }
        }
        else
        {
            lwrPC = nextPC;
            nextPC++;
        }

        m_Cycles++;

        if (m_bIsError) return RC::SOFTWARE_ERROR;
    }

    if (m_LwrData < InputData.size())
    {
        SetWarning("UnusedInput",
                   "Program didn't consume all data passed in\n");
    }

    if (m_bIsError) return RC::SOFTWARE_ERROR;

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Get the full value released by the MME for the specified method
//!
//! \param Method         : Method number to get the released value for
//! \param SubCh          : Subchannel where the method will be released
//! \param ProgramCounter : Program counter at time of release
//!
//! \return Value of the method released by the MME
//!
UINT32 MMESimulator::GetReleasedMethodValue
(
    UINT32 SubCh,
    UINT32 Method,
    UINT32 ProgramCounter
)
{
    UINT32 value;
    if (m_Version < 2)
    {
        value = (DRF_NUM(_MME, _RELEASE, _METHOD, Method)  |
                 DRF_NUM(_MME, _RELEASE, _SUBCH, SubCh)  |
                 DRF_NUM(_MME, _RELEASE, _PC, ProgramCounter)  |
                 DRF_NUM(_MME, _RELEASE, _MME_GEN, 1));
    }
    else
    {
        value = (DRF_NUM(_MME, _RELEASE, _METHOD, Method)  |
                 DRF_NUM(_MME, _RELEASE, _SUBCH, SubCh)  |
                 DRF_NUM(_MME, _RELEASE, _PC_V2, ProgramCounter)  |
                 DRF_NUM(_MME, _RELEASE, _MME_GEN_V2, 1));
    }

    return value;
}

//----------------------------------------------------------------------------
//! \brief Get the value for the specified register introducing the correct
//!        amount of latency
//!
//! \param RegNum  : Register to retrieve
//! \param Latency : Latency for the access
//!
//! \return Value of the register
//!
UINT32 MMESimulator::Reg(UINT32 RegNum, INT32 Latency)
{
    if (!m_RegFile[RegNum].bWritten)
    {
        SetError("Read unwritten register");
        return 0xcdcdcdcd;
    }
    while (m_Cycles < (m_RegFile[RegNum].CycleReady + Latency))
        m_Cycles++;

    return m_RegFile[RegNum].Value;
}

//----------------------------------------------------------------------------
//! \brief Get the value of the Src0 register in an instruction
//!
//! \param Inst    : Instruction to extract the Src0 register from
//! \param Latency : Latency for the access
//!
//! \return Value of the Src0 register
//!
UINT32 MMESimulator::Reg0(UINT32 Inst, INT32 Latency)
{
    return Reg(m_pMmeIGen->GetSrc0Reg(Inst), Latency);
}

//----------------------------------------------------------------------------
//! \brief Get the value of the Src1 register in an instruction
//!
//! \param Inst    : Instruction to extract the Src1 register from
//! \param Latency : Latency for the access
//!
//! \return Value of the Src1 register
//!
UINT32 MMESimulator::Reg1(UINT32 Inst, INT32 Latency)
{
    return Reg(m_pMmeIGen->GetSrc1Reg(Inst), Latency);
}

//----------------------------------------------------------------------------
//! \brief Simulate an insert instruction
//!
//! \param Base      : Base value to insert into
//! \param Src1      : Source1 value for the insert
//! \param SourceBit : Starting bit in source value
//! \param DestBit   : Starting bit in destination value
//! \param Width     : Width of the insertion
//!
//! \return Value after the insertion
//!
UINT32 MMESimulator::Insert(UINT32 Base, UINT32 Src1,
                            UINT32 SourceBit, UINT32 DestBit,
                            UINT32 Width)
{
    UINT32 temp = Src1;
    UINT32 oldBase = Base;
    UINT32 mask = (1 << Width) - 1;
    UINT32 xmask = mask << DestBit ;

    temp >>= SourceBit;
    temp = (SourceBit > 31) ? 0 : temp;
    temp &= mask;
    temp <<= DestBit;
    temp = (DestBit > 31) ? 0 : temp;

    xmask = (DestBit > 31) ? 0 : xmask;

    Base &= ~(xmask);
    Base |= temp;

    Printf(m_TracePri,
           "MMESimTrace: 0x%08x = Insert 0x%08x=0x%08x[%d:%d] into "
           "0x%08x at [%d:%d]\n",
           Base,
           (temp >> DestBit) << SourceBit,
           Src1,
           SourceBit + Width - 1,
           SourceBit,
           oldBase,
           DestBit + Width - 1,
           DestBit);

    return Base;
}

//----------------------------------------------------------------------------
//! \brief Simulate an ADD (or SUB) instruction
//!
//! \param a         : First value to add
//! \param b         : Second value to add
//! \param bUseCarry : true to use the carry bit
//!
//! \return Value after the add
//!
UINT32 MMESimulator::Add(UINT32 a, UINT32 b, bool bUseCarry)
{
    INT64 ia = (INT64)a;
    INT64 ib = (INT64)b;
    INT64 temp;

    temp = ia + ib + (bUseCarry ? m_Carry : 0);

    if (bUseCarry) m_Carry = (UINT32)(temp >> 32) & 1;

    return (UINT32)(temp & 0xffffffff);
}

//----------------------------------------------------------------------------
//! \brief Simulate an load from MME calling data
//!
//! \param Data : Input data array
//!
//! \return Data from the array at the current load position
//!
UINT32 MMESimulator::Load(const vector<UINT32> &Data)
{
    if (m_LwrData >= Data.size())
    {
        SetError("Too many loads for data passed in\n");
        return 0;
    }

    return Data[m_LwrData++];
}

//----------------------------------------------------------------------------
//! \brief Simulate a retrieving a value from the MME shadow RAM (STATEI)
//!
//! \param Value : The value that the STATEI instruction uses
//!
//! \return Data from the shadow RAM at the specified position
//!
UINT32 MMESimulator::GetState(UINT32 Value)
{
    GpuSubdevice::MmeMethod stateMethod;
    stateMethod.ShadowClass = m_Class;
    stateMethod.ShadowMethod = m_pMmeIGen->GetMethod(Value);

    return m_pStateInput->GetValue(stateMethod);
}

//----------------------------------------------------------------------------
//! \brief Simulate releasing a value to the output stream
//!
//! \param pOutputData    : Pointer to output data vector to store the release
//! \param ProgramCounter : Program counter at time of release
//! \param Method         : Method to release
//! \param Data           : Data to release
//!
void MMESimulator::Release(vector<UINT32> *pOutputData,
                           UINT32 ProgramCounter,
                           UINT32 Method,
                           UINT32 Data)
{
    GpuSubdevice::MmeMethod stateMethod;

    // Update the shadow RAM in case the release is to a shadowed method
    stateMethod.ShadowClass = m_Class;
    stateMethod.ShadowMethod = m_pMmeIGen->GetMethod(Method);
    m_pStateInput->SetValue(stateMethod, Data);

    // Add the release to the output data
    pOutputData->push_back(Data);
    pOutputData->push_back(GetReleasedMethodValue(m_SubCh, Method,
                                                  ProgramCounter));
}

//----------------------------------------------------------------------------
//! \brief Set the error string for the simulator
//!
//! \param ErrorString : Error string to set
//!
void MMESimulator::SetError(string ErrorString)
{
    // Only report the first error
    if (m_bIsError) return;

    m_bIsError = true;
    m_Error = "Error : " + ErrorString;
}

//----------------------------------------------------------------------------
//! \brief Set the warning string for the simulator
//!
//! \param Category : Warning categrory
//! \param Message  : Warning message
//!
void MMESimulator::SetWarning(string Category, string Message)
{
    m_WarningCategory = Category;
    m_WarningMessage = Message;
}

//----------------------------------------------------------------------------
//! \brief Print the MUX value
//!
//! \param muxOp : Mux value to print
//!
void MMESimulator::PrintMux(MmeInstGenerator::MuxOp muxOp)
{
    switch (muxOp)
    {
        case MmeInstGenerator::MUX_LNN:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_LOAD RM_NONE MM_NONE\n");
            break;
        case MmeInstGenerator::MUX_LRN:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_LOAD RM_RES MM_NONE\n");
            break;
        case MmeInstGenerator::MUX_RNN:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_RES RM_NONE MM_NONE\n");
            break;
        case MmeInstGenerator::MUX_RRN:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_RES RM_RES MM_NONE\n");
            break;
        case MmeInstGenerator::MUX_LNR:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_LOAD RM_NONE MM_RES\n");
            break;
        case MmeInstGenerator::MUX_RNR:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_RES RM_NONE MM_RES\n");
            break;
        case MmeInstGenerator::MUX_RLR:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_RES RM_LOAD MM_RES\n");
            break;
        case MmeInstGenerator::MUX_RRR:
            Printf(m_TracePri, "MMESimTrace: Mux: DM_RES RM_RESHIGH MM_RES\n");
            break;
    }
}

//----------------------------------------------------------------------------
//! \brief Handle a binary operation
//!
//! \param Inst    : Instruction containing the binary op
//! \param pResult : Pointer to the result of the operation
//!
//! \return OK if successful, not OK otherwise
//!
RC MMESimulator::HandleBinaryOp(UINT32 Inst, UINT32 *pResult)
{
    switch(m_pMmeIGen->GetBinSubOp(Inst))
    {
        default:
            SetError("Invalid Binary Subopcode");
            return RC::SOFTWARE_ERROR;
        case MmeInstGenerator::BOP_ADD:
            m_Carry = 0;
            // fallthrough
        case MmeInstGenerator::BOP_ADDC:
            *pResult = Add(Reg0(Inst), Reg1(Inst));
            Printf(m_TracePri,
                   "MMESimTrace: 0x%08x + 0x%08x = 0x%08x carry %d\n",
                   Reg0(Inst), Reg1(Inst), *pResult, m_Carry);
            break;
        case MmeInstGenerator::BOP_SUB:
            m_Carry = 1;
            // fallthrough
        case MmeInstGenerator::BOP_SUBB:
            *pResult = Add(Reg0(Inst), ~Reg1(Inst));
            Printf(m_TracePri,
                   "MMESimTrace: 0x%08x - 0x%08x = 0x%08x carry %d\n",
                   Reg0(Inst), Reg1(Inst), *pResult, m_Carry);
            break;
        case MmeInstGenerator::BOP_XOR:
            *pResult = Reg0(Inst) ^ Reg1(Inst);
            Printf(m_TracePri,
                   "MMESimTrace: 0x%08x ^ 0x%08x = 0x%08x\n",
                   Reg0(Inst), Reg1(Inst), *pResult);
            break;
        case MmeInstGenerator::BOP_OR:
            *pResult = Reg0(Inst) | Reg1(Inst);
            Printf(m_TracePri,
                   "MMESimTrace: 0x%08x | 0x%08x = 0x%08x\n",
                   Reg0(Inst), Reg1(Inst), *pResult);
            break;
        case MmeInstGenerator::BOP_AND:
            *pResult = Reg0(Inst) & Reg1(Inst);
            Printf(m_TracePri,
                   "MMESimTrace: 0x%08x & 0x%08x = 0x%08x\n",
                   Reg0(Inst), Reg1(Inst), *pResult);
            break;
        case MmeInstGenerator::BOP_ANDNOT:
            *pResult = Reg0(Inst) & ~Reg1(Inst);
            Printf(m_TracePri,
                   "MMESimTrace: 0x%08x & ~0x%08x = 0x%08x\n",
                   Reg0(Inst), Reg1(Inst), *pResult);
            break;
        case MmeInstGenerator::BOP_NAND:
            *pResult = ~(Reg0(Inst) & Reg1(Inst));
            Printf(m_TracePri,
                   "MMESimTrace: ~(0x%08x & 0x%08x) = 0x%08x\n",
                   Reg0(Inst), Reg1(Inst), *pResult);
            break;
    }
    return OK;
}
