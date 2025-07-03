/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "mme64igen.h"

#include <algorithm>
#include <cmath>
#include <memory>

using namespace Mme64;

#define ASSERT_NOT_EMPTY(vector) MASSERT(!((vector).empty()))

namespace
{
    //------------------------------------------------------------------------------
    // Lists of all the options for each field with set values. Used for random
    // field value selection.
    //
    // NOTE: These must be kept in sync with all the possible enum values, otherwise
    // new enum values will not be picked.
    //
    const vector<Op> s_ArithmeticOps =
    {
        Op::Add
        ,Op::Addc
        ,Op::Sub
        ,Op::Subb
        ,Op::Mul
        ,Op::Mulh
        ,Op::Mulu
        ,Op::Clz
        ,Op::Sll
        ,Op::Srl
        ,Op::Sra
    };

    const vector<Op> s_LogicOps =
    {
        Op::And
        ,Op::Nand
        ,Op::Or
        ,Op::Xor
        ,Op::Merge
    };

    const vector<Op> s_ComparisonOps =
    {
        Op::Slt
        ,Op::Sltu
        ,Op::Sle
        ,Op::Sleu
        ,Op::Seq
    };

    const vector<Op> s_FlowControlOps =
    {
        Op::Loop
        ,Op::Jal
        ,Op::Blt
        ,Op::Bltu
        ,Op::Ble
        ,Op::Bleu
        ,Op::Beq
    };

    const vector<Op> s_ExtendedOps =
    {
        Op::Extended
    };

    const vector<Op> s_StateOps =
    {
        Op::State
    };

    const vector<Op> s_DataRamOps =
    {
        Op::Dread
        ,Op::Dwrite
    };

    const vector<PredVal> s_PredVals =
    {
        PredVal::U
        ,PredVal::T
        ,PredVal::F
    };

    const vector<SpecialReg> s_SpecialRegs =
    {
        SpecialReg::LoadSource
        ,SpecialReg::MmeConfig
        ,SpecialReg::PTimerHigh
        ,SpecialReg::PTimerLow
    };

    const vector<Subop> s_Subops =
    {
        Subop::ReadFromSpecial
        ,Subop::WriteToSpecial
    };

    const vector<Src> s_AllSrcs =
    {
        Src::R0
        ,Src::R1
        ,Src::R2
        ,Src::R3
        ,Src::R4
        ,Src::R5
        ,Src::R6
        ,Src::R7
        ,Src::R8
        ,Src::R9
        ,Src::R10
        ,Src::R11
        ,Src::R12
        ,Src::R13
        ,Src::R14
        ,Src::R15
        ,Src::R16
        ,Src::R17
        ,Src::R18
        ,Src::R19
        ,Src::R20
        ,Src::R21
        ,Src::R22
        ,Src::R23
        ,Src::Zero
        ,Src::Immed
        ,Src::ImmedPair
        ,Src::Immed32
        ,Src::Load0
        ,Src::Load1
    };

    const vector<Dst> s_AllDsts =
    {
        Dst::R0
        ,Dst::R1
        ,Dst::R2
        ,Dst::R3
        ,Dst::R4
        ,Dst::R5
        ,Dst::R6
        ,Dst::R7
        ,Dst::R8
        ,Dst::R9
        ,Dst::R10
        ,Dst::R11
        ,Dst::R12
        ,Dst::R13
        ,Dst::R14
        ,Dst::R15
        ,Dst::R16
        ,Dst::R17
        ,Dst::R18
        ,Dst::R19
        ,Dst::R20
        ,Dst::R21
        ,Dst::R22
        ,Dst::R23
        ,Dst::Zero
    };

    const vector<PredReg> s_AllPredRegs =
    {
        PredReg::R0
        ,PredReg::R1
        ,PredReg::R2
        ,PredReg::R3
        ,PredReg::R4
        ,PredReg::R5
        ,PredReg::R6
        ,PredReg::R7
        ,PredReg::R8
        ,PredReg::R9
        ,PredReg::R10
        ,PredReg::R11
        ,PredReg::R12
        ,PredReg::R13
        ,PredReg::R14
        ,PredReg::R15
        ,PredReg::R16
        ,PredReg::R17
        ,PredReg::R18
        ,PredReg::R19
        ,PredReg::R20
        ,PredReg::R21
        ,PredReg::R22
        ,PredReg::R23
        ,PredReg::Zero
    };

    const vector<Method> s_AllMethods =
    {
        Method::None
        ,Method::Alu0
        ,Method::Alu1
        ,Method::Load0
        ,Method::Load1
        ,Method::Immed0
        ,Method::Immed1
    };

    const vector<Emit> s_AllEmits =
    {
        Emit::None
        ,Emit::Alu0
        ,Emit::Alu1
        ,Emit::Load0
        ,Emit::Load1
        ,Emit::Immed0
        ,Emit::Immed1
        ,Emit::ImmedHigh0
        ,Emit::ImmedHigh1
        ,Emit::Immed32
    };

    const map<AluNum, OutputField> s_AluToOutputField =
    {
        {0, OutputField::Alu0}
        ,{1, OutputField::Alu1}
    };

    //!
    //! \brief Comparator for Mme64 namespace enums.
    //!
    //! NOTE: For use with std::set_difference on the sets of possible
    //! selections.
    //!
    template <class T>
    bool EnumCmp(const T& a, const T& b)
    {
        return static_cast<UINT32>(a) < static_cast<UINT32>(b);
    }
};

//------------------------------------------------------------------------------
LoadInstrPicker::LoadInstrPicker(FancyPicker::FpContext* pFpCtx)
    : m_pFpCtx(pFpCtx), m_MaxN(static_cast<UINT32>(s_SrcLoads.size()) - 1)
{
    MASSERT(m_MaxN == s_SrcLoads.size() - 1); // Truncation check

    // Assumes that all load locations have the same maximum n (as in LOADn
    // instructions).
    MASSERT(s_SrcLoads.size() == s_MethodLoads.size()
            && s_MethodLoads.size() == s_EmitLoads.size());

    Reset();
}

//!
//! \brief Reset internal counters.
//!
RC LoadInstrPicker::Reset()
{
    m_NextHighestN = 0;

    return RC::OK;
}

void LoadInstrPicker::Rand(Src* pSrc)
{
    *pSrc = s_SrcLoads.at(PickN());
}

void LoadInstrPicker::Rand(Method* pMethod)
{
    *pMethod = s_MethodLoads.at(PickN());
}

void LoadInstrPicker::Rand(Emit* pEmit)
{
    *pEmit = s_EmitLoads.at(PickN());
}

//!
//! \brief Number of unique loads picked since the last reset.
//!
UINT32 LoadInstrPicker::NumUniqueLoads() const
{
    return m_NextHighestN;
}

UINT32 LoadInstrPicker::PickN()
{
    UINT32 n = m_pFpCtx->Rand.GetRandom(0, std::min(m_NextHighestN, m_MaxN));

    if (n == m_NextHighestN && m_NextHighestN <= m_MaxN)
    {
        // If we have used a LOADn, it is now valid to use a LOADn+1
        ++m_NextHighestN;
    }

    return n;
}

/* static */ vector<Src> LoadInstrPicker::s_SrcLoads =
{
    Src::Load0
    ,Src::Load1
};

/* static */ vector<Method> LoadInstrPicker::s_MethodLoads =
{
    Method::Load0
    ,Method::Load1
};

/* static */ vector<Emit> LoadInstrPicker::s_EmitLoads =
{
    Emit::Load0
    ,Emit::Load1
};

//------------------------------------------------------------------------------
UINT32 MacroGenerator::s_MaxGenIterationsPerAlu = 10;

MacroGenerator::MacroGenerator(FancyPicker::FpContext* pFpCtx)
    : m_pFpCtx(pFpCtx)
    , m_LoadInstrPicker(pFpCtx)
{
    MASSERT(pFpCtx);

    // The macro generator depends on registers being sequential
    SequentialRegisterEncodingCheck();
}

//!
//! \brief Create a macro that sets the MME register file. Can add to existing
//! macros.
//!
//! Sets the values for all registers, including the method register.
//!
//! NOTE: Only randomizes the value of the first 16 bits of each register. The
//! upper 16 bits will be 0.
//! NOTE: Does not abide by the generation configuration settings.
//!
//! \param randValues If true, randomize the values in the regfile. Otherwise
//! zero the regfile.
//! \param[out] pMacro Macro to populate.
//!
//! \see GetNumGroupsInRefgileSetter Number of groups that will be generated.
//!
// TODO(stewarts): Can use LOAD or DREAD/DWRITE to set these values and improve
// the bit range to 32bits.
RC MacroGenerator::GenRegfileSetter(bool randValues, Macro* pMacro)
{
    // NOTE(stewarts): Assumes that the registers have sequential encodings.

    RC rc;
    MASSERT(pMacro);

    Macro::Groups& groups = pMacro->groups;
    PredModeBuilder<s_NumGroupAlus> predModeBuilder;

    const UINT32 originalMacroSize = static_cast<UINT32>(pMacro->groups.size());
    MASSERT(originalMacroSize == pMacro->groups.size()); // Truncation check
    const AluNum numAlus = s_NumGroupAlus;
    const UINT32 numGroups = MacroGenerator::GetNumGroupsInRegfileSetter(numAlus);

    // Prepare for adding groups
    CHECK_RC(PrepareMacroForGroupAddition(pMacro));
    groups.reserve(groups.size() + numGroups);

    // There should be no emissions from generated groups
    PredMode predMode;
    predModeBuilder.SetUncondAluFalseOutput();
    CHECK_RC(predModeBuilder.Build(&predMode));

    const UINT32 regsPerGroup = numAlus; // Number of regs that can be reset per
                                         // group
    const UINT32 lookaheadAmt = 2;
    bool endNext = false;

    for (UINT32 groupBaseReg = s_LowestRegEncoding;
         groupBaseReg <= s_HighestRegEncoding;
         groupBaseReg += regsPerGroup)
    {
        // Create a new MME64 Group to set part of remaining registers to zero
        groups.emplace_back();
        auto& group = groups.back();

        // Set the end next on the second last group
        const UINT32 lookaheadTwoLoops
            = groupBaseReg + (regsPerGroup * lookaheadAmt);
        endNext
            // If next Group could zero the remaining registers, set ENDNEXT
            = lookaheadTwoLoops >= s_HighestRegEncoding
            // Second last group has ENDNEXT, last group shouldn't.
            && !endNext;

        group.SetGlobal(endNext, predMode, PredReg::Zero);

        for (AluNum alu = 0; alu < numAlus; ++alu)
        {
            const UINT32 reg = groupBaseReg + alu; // current register

            if (reg > s_HighestRegEncoding)
            {
                // More ALUs available than we need to zero all registers, fill
                // with nops
                group.InstrAdd(alu, Dst::Zero, Src::Zero, Src::Zero);
            }
            else
            {
                // Set register[reg]
                UINT16 regVal = 0;
                if (randValues)
                {
                    regVal = m_pFpCtx->Rand.GetRandom(0, _UINT16_MAX); // 16-bit reg
                }

                group.SetAluImmed(alu, regVal);
                group.InstrAdd(alu,
                               static_cast<Dst>(reg), Src::Immed, Src::Zero);
            }

            // No emissions
            group.SetOutput(alu, Method::None, Emit::None);
            group.SetNumEmissions(0);
        }
    }

    // Number of groups generated should match the estimated number of groups.
    if (groups.size() - originalMacroSize != numGroups)
    {
        Printf(Tee::PriError,
               "Estimated number of groups does not match number of groups "
               "generated\n"
               "\tEstimated(%u), Generated(%u)\n", numGroups,
               static_cast<UINT32>(groups.size() - originalMacroSize));
        MASSERT(!"See last error");
        return RC::SOFTWARE_ERROR;
    }

    // Set method register to known state
    //
    // Abritrarily use the 0th output of the last group to set the method
    // register using immediate from ALU0.
    auto& lastGroup = groups.back();

    // Set 0th output to run uncoditionally, allowing the method register to be
    // set.
    lastGroup.SetGlobalPredMode(PredMode::UUUF); // @Robust

    UINT16 methRegVal = 0;
    if (randValues)
    {
        methRegVal = m_pFpCtx->Rand.GetRandom(0, _UINT16_MAX); // 16-bit reg
    }

    lastGroup.SetAluImmed(0, methRegVal);
    lastGroup.SetOutputMethod(0, Method::Immed0);

    return rc;
}

//!
//! \brief Create start of a macro that sets the starting configuration of the
//! MME.
//!
//! NOTE: The simulator has no concept of external configuration, simply macro
//! input and memory state. By setting the MME state at the beginning of each
//! macro, it ensures that both the HW MME and the simulator will be in sync.
//! This was discovered when the simulator was processing the MME related
//! emitted methods and the HW MME wasn't.
//! NOTE: Does not abide by the generation configuration settings.
//!
//! \param[out] pMacro Macro to populate.
//!
//! \see GetNumGroupsInMmeConfigSetup Number of groups that will be generated.
//!
RC MacroGenerator::GenMmeConfigSetup(Macro* pMacro)
{
    RC rc;
    MASSERT(pMacro);
    // This must be the first thing in all the macros so the MME state is set
    // upfront.
    MASSERT(pMacro->groups.empty());
    pMacro->numEmissions = 0; // Initialize emission count
    // Must have at least two ALUs
    MASSERT(s_NumGroupAlus >= 2);

    const UINT32 originalMacroSize = static_cast<UINT32>(pMacro->groups.size());
    const UINT32 numAlus = s_NumGroupAlus;
    const UINT32 numGroups = MacroGenerator::GetNumGroupsInMmeConfigSetup(numAlus);
    Macro::Groups& groups = pMacro->groups;
    groups.reserve(numGroups);
    PredModeBuilder<s_NumGroupAlus> predModeBuilder;

    // Generated groups should not emit
    PredMode predMode;
    predModeBuilder.SetUncondAluFalseOutput();
    CHECK_RC(predModeBuilder.Build(&predMode));

    // Set loads to come from the incoming methods to FE (default setting at
    // macro start)
    //
    // NOTE: Don't really need to set this, but the macro needs at least two
    // groups to be valid. Besides, this garantees the initial state of the MME.
    groups.emplace_back();
    auto* pGroup = &groups.back();
    pGroup->SetGlobal(true/*endNext*/, predMode, PredReg::Zero);
    pGroup->InstrExtendedWriteToLoadSource(0, LoadSource::MethodInput);
    pGroup->SetOutput(0, Method::None, Emit::None);
    pGroup->Nop(1);
    pGroup->SetOutput(1, Method::None, Emit::None);
    pGroup->SetNumEmissions(0);

    // Fill remaining ALUs with NOPs
    for (UINT32 alu = 2; alu < numAlus; ++alu)
    {
        pGroup->Nop(alu);
        pGroup->SetOutput(alu, Method::None, Emit::None);
    }

    // Set MME to consume all methods after the shadow RAM.
    //
    // NOTE: This prevents the simulator from processing MME emissions, which is
    // the same behaviour when the MME is in I2M mode.
    //
    groups.emplace_back();
    pGroup = &(groups.back());
    pGroup->SetGlobal(false/*endNext*/, predMode, PredReg::Zero);
    pGroup->InstrExtendedWriteToMmeConfig(0, MmeConfig::MethodEmitDisable_On);
    pGroup->SetOutput(0, Method::None, Emit::None);
    pGroup->Nop(1);
    pGroup->SetOutput(1, Method::None, Emit::None);
    pGroup->SetNumEmissions(0);

    // Fill remaining ALUs with NOPs
    for (UINT32 alu = 2; alu < numAlus; ++alu)
    {
        pGroup->Nop(alu);
        pGroup->SetOutput(alu, Method::None, Emit::None);
    }

    // Number of groups generated should match the estimated number of groups.
    if (groups.size() - originalMacroSize != numGroups)
    {
        Printf(Tee::PriError,
               "Estimated number of groups does not match number of groups "
               "generated\n"
               "\tEstimated(%u), Generated(%u)\n", numGroups,
               static_cast<UINT32>(groups.size() - originalMacroSize));
        MASSERT(!"See last error");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//!
//! \brief Generate a macro that sets the Data RAM to some known state.
//!
//! Generates a number of groups with DWRITE operations to set the Data RAM.
//!
//! NOTE: Does not abide by the generation configuration settings.
//!
//! \param startDataRamEntry Starting entry/address of the Data RAM to be set.
//! \param numEntriesToSet Number of entries/addresses to set starting at the
//! start entry.
//! \param randValue If true, randomize the values in the Data RAM. Otherwise
//! zero the Data RAM.
//! \param[out] pMacro Macro to populate.
//!
RC MacroGenerator::GenDataRamSetter
(
    UINT32 startDataRamEntry
    ,UINT32 numEntriesToSet
    ,bool randValues
    ,Macro* pMacro
)
{
    RC rc;
    MASSERT(pMacro);
    // Current algorithm does not support a data ram entry address greater than
    // 16-bits
    MASSERT(startDataRamEntry <= _UINT16_MAX);
    MASSERT(numEntriesToSet > 0); // Must set at least one entry.

    Macro::Groups& groups = pMacro->groups;

    const UINT32 originalMacroSize = static_cast<UINT32>(pMacro->groups.size());
    MASSERT(originalMacroSize == pMacro->groups.size()); // Truncation check
    const AluNum numAlus = s_NumGroupAlus;
    const UINT32 numGroups
        = MacroGenerator::GetNumGroupsInDataRamSetter(numAlus, numEntriesToSet);

    // Prepare for adding groups
    CHECK_RC(PrepareMacroForGroupAddition(pMacro));
    groups.reserve(groups.size() + numGroups);

    // Generated groups should not emit
    PredModeBuilder<s_NumGroupAlus> predModeBuilder;
    PredMode predMode;
    predModeBuilder.SetUncondAluFalseOutput();
    CHECK_RC(predModeBuilder.Build(&predMode));

    // Generate groups
    //
    MASSERT(numAlus >= 2); // Population algorithms require at least two ALUs
    bool endNext = false;

    // Need to track the data ram addresses from a register:
    // - R0 will track the current data ram address
    // - R1 will contain the value 1 for incrementing R0
    groups.emplace_back();
    auto& regSetupGroup = groups.back();

    // Need at least one more group to have the minimum macro size of two.
    MASSERT(numEntriesToSet > 0);

    // If there is only one entry to set, only one group will be generated after
    // the register setup group. This makes the register setup group the 2nd
    // last group, and therefore it should have ENDNEXT set.
    if (numEntriesToSet == 1)
    {
        endNext = true;
    }

    regSetupGroup.SetGlobal(endNext, predMode, PredReg::Zero);
    regSetupGroup.SetImmed(0, static_cast<UINT16>(startDataRamEntry));
    regSetupGroup.InstrAdd(0, Dst::R0, Src::Zero, Src::Immed); // R0 = start entry
    regSetupGroup.SetOutput(0, Method::None, Emit::None);
    regSetupGroup.SetImmed(1, 0x1);
    regSetupGroup.InstrAdd(1, Dst::R1, Src::Zero, Src::Immed); // R1 = 1
    regSetupGroup.SetOutput(0, Method::None, Emit::None);
    for (UINT32 alu = 2; alu < numAlus; ++alu)
    {
        regSetupGroup.Nop(alu);
        regSetupGroup.SetOutput(alu, Method::None, Emit::None);
    }
    regSetupGroup.SetNumEmissions(0);

    // Generate the DWRITES to set the data ram. One data ram entry set per
    // group.
    //
    // NOTE: There can only be two DWRITE instructions per group: one for an
    // even address, and one for an odd address.
    //
    // Ref: https://p4viewer.lwpu.com/get/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    // Section "Group Descriptions":
    //   "Two DREAD or DWRITE ops can only be in a group if SW can guarantee
    //   they are to even/odd addresses so that they can be serviced by 2 data
    //   ram banks in parallel."
    const UINT32 lastRamAddr = startDataRamEntry + numEntriesToSet - 1;

    for (UINT32 ramAddr = startDataRamEntry;
         ramAddr <= lastRamAddr;
         ++ramAddr)
    {
        groups.emplace_back();
        auto& group = groups.back();

        // Set the end next on the second last group only
        endNext = (ramAddr + 1 == lastRamAddr);

        group.SetGlobal(endNext, predMode, PredReg::Zero);

        // Get value
        UINT32 val = 0;
        if (randValues)
        {
            val = m_pFpCtx->Rand.GetRandom(0, _UINT32_MAX);
        }

        // Set the full 32-bit value by using two 16-bit immediates. One ALU for
        // the DWRITE, another ALU to increment the data ram address.
        group.SetImmed(0, static_cast<UINT16>(val >> 16)); // High bits
        group.SetImmed(1, static_cast<UINT16>(val));       // Low bits
        group.InstrDwrite(0, Src::R0, Src::Immed32);  // Set data ram entry
        group.SetOutput(0, Method::None, Emit::None);
        group.InstrAdd(1, Dst::R0, Src::R0, Src::R1); // Increment data ram addr
        group.SetOutput(1, Method::None, Emit::None);
        for (UINT32 alu = 2; alu < numAlus; ++alu)
        {
            group.Nop(alu);
            group.SetOutput(alu, Method::None, Emit::None);
        }

        // No emissions
        group.SetNumEmissions(0);
    }

    // Number of groups generated should match the estimated number of groups.
    if (groups.size() - originalMacroSize != numGroups)
    {
        Printf(Tee::PriError,
               "Estimated number of groups does not match number of groups "
               "generated\n"
               "\tEstimated(%u), Generated(%u)\n", numGroups,
               static_cast<UINT32>(groups.size() - originalMacroSize));
        MASSERT(!"See last error");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow, "Generated %u groups to set Data RAM addr %u to %u\n",
           numGroups, startDataRamEntry, lastRamAddr);

    return rc;
}

//!
//! \brief Set the macro generation configuration.
//!
//! The configuration is used to influence macro/group generation results,
//! unless otherwise stated in the generation method's documentation.
//!
//! \param pGenConfig Generation configuration. Object takes ownership of the
//! config.
//!
RC MacroGenerator::SetGenConfig(unique_ptr<Config> pGenConfig)
{
    // NOTE: LOADn instruction will only be added once per applicable selection
    // and is denoted by the LOAD0 instruction. Even though there are n load
    // instructions and it is valid to include all of them, it is only valid to
    // pick LOADn if LOADn-1 has already been picked. When LOAD0 instruction is
    // picked for a group by the Rand* methods, an appropriately selected random
    // load value will be used in place of LOAD0. This is handled by the
    // LoadInstrPicker.

    RC rc;
    MASSERT(0 <= pGenConfig->percentFlowControlOps
            && pGenConfig->percentFlowControlOps <= 1);

    // Take ownership of the config
    m_pGenConfig = std::move(pGenConfig);

    // Reset selections
    m_StandardOps.clear();
    m_Srcs.clear();
    m_Dsts.clear();
    m_Methods.clear();
    m_Emits.clear();

    // Collect ops selections
    //
    CHECK_RC(CollectStandardOpSelections());

    // TODO(stewarts):
    // OPS_FLOW_CTRL
    // OPS_EXTENDED
    // OPS_STATE
    MASSERT(!(m_pGenConfig->flags & OPS_FLOW_CTRL));
    MASSERT(!(m_pGenConfig->flags & OPS_EXTENDED));
    MASSERT(!(m_pGenConfig->flags & OPS_STATE));

    // Collect instruction slot slections
    //
    // Collect Src selections
    m_Srcs.reserve(s_AllSrcs.size());
    for (Src src : s_AllSrcs)
    {
        if (!FilterLoad(src) && !FilterRegs(src))
        {
            m_Srcs.push_back(src);
        }
    }
    m_Srcs.shrink_to_fit();

    // Collect Dst selections
    m_Dsts.reserve(s_AllDsts.size());
    for (Dst dst : s_AllDsts)
    {
        if (!FilterRegs(dst))
        {
            m_Dsts.push_back(dst);
        }
    }
    m_Dsts.shrink_to_fit();

    // Collect Method selections
    m_Methods.reserve(s_AllMethods.size());
    for (Method method : s_AllMethods)
    {
        if (!FilterLoad(method))
        {
            m_Methods.push_back(method);
        }
    }
    m_Methods.shrink_to_fit();

    // Collect Emit selections
    m_Emits.reserve(s_AllEmits.size());
    for (Emit emit : s_AllEmits)
    {
        if (!FilterLoad(emit)
            // Filter sending methods
            && !(m_pGenConfig->noSendingMethods && emit == Emit::None))
        {
            m_Emits.push_back(emit);
        }
    }
    m_Emits.shrink_to_fit();

    // No collection should be empty
    ASSERT_NOT_EMPTY(m_StandardOps);
    ASSERT_NOT_EMPTY(m_Srcs);
    ASSERT_NOT_EMPTY(m_Dsts);
    ASSERT_NOT_EMPTY(m_Methods);
    ASSERT_NOT_EMPTY(m_Emits);

    // Sort Method and Emit selection for use with std::set_difference
    std::sort(m_Methods.begin(), m_Methods.end(), &::EnumCmp<Method>);
    std::sort(m_Emits.begin(), m_Emits.end(), &::EnumCmp<Emit>);

    return rc;
}

//!
//! \brief Randomly generate a macro based on the current generation settings.
//!
//! Supports adding to existing generated Macros.
//!
//! \param numGroup Number of groups to generate.
//! \param dataRamSize Size of the usable/addressable MME Data RAM. Value is
//! ignored if Data RAM operations are not being generated.
//! \param[out] pOutMacro Macro that is populated with the generated groups.
//! Metadata regarding the groups is populated accordingly.
//!
//! \see SetGenConfig Set generation configuration.
//!
RC MacroGenerator::GenMacro
(
    UINT32 numGroups
    ,UINT32 dataRamSize
    ,Macro* pOutMacro
)
{
    RC rc;
    MASSERT(pOutMacro);
    MASSERT(numGroups > 0); // Must have at least one group per pack
    MASSERT(m_pGenConfig);

    UINT32 numUniqueLoads = 0;

    // Prepare the macro
    CHECK_RC(PrepareMacroForGroupAddition(pOutMacro));
    pOutMacro->groups.reserve(pOutMacro->groups.size() + numGroups);

    const UINT32 numFlowControl
        = static_cast<UINT32>(numGroups * m_pGenConfig->percentFlowControlOps);
    const UINT32 numNonFlowControl = numGroups - numFlowControl;

    // Generate non-flow control instructions
    //
    for (UINT32 n = 0; n < numNonFlowControl; n++)
    {
        bool endNext = false;

        // Set 'end next' if this is the second last group.
        if (n == numNonFlowControl - 2) { endNext = true; }

        // Reset the load instruction picker before each group.
        // This allows:
        // - The number of unique loads reported by the picker to be the number
        //   of unique loads for the generated group.
        // - Resets the LOADn restrictions. LOADn restrictions apply on a group
        //   by group basis.
        CHECK_RC(m_LoadInstrPicker.Reset());

        // Gen group
        CHECK_RC(GenGroup(pOutMacro, endNext, dataRamSize));

        // Get unique load count in generated group
        numUniqueLoads += m_LoadInstrPicker.NumUniqueLoads();

    }

    // Generate flow control instructions
    // for (UINT32 n = 0; n < numFlowControl; n++)
    // {
    //     // TODO(stewarts):
    // }

    // Generate the input data values (comsumed in LOADs)
    //
    pOutMacro->inputData.reserve(numUniqueLoads);
    for (UINT32 n = 0; n < numUniqueLoads; ++n)
    {
        pOutMacro->inputData.push_back(m_pFpCtx->Rand.GetRandom(0,
                                                                _UINT32_MAX));
    }

    return rc;
}

//!
//! \brief Checks that register encodings are sequential. Asserts if check
//! fails.
//!
//! Sequential register encodings are an assumption made by the
//! MacroGenerator's implementation. If this assumption fails, an
//! iterator/generator will have to be made that produces R0 to Rn in ascending
//! order. This must then replace the existing use of lowest encoding to highest
//! encoding to loop through registers.
//!
void MacroGenerator::SequentialRegisterEncodingCheck() const
{
    // Using s_AllSrcs as an example of valid register encodings. Requires it to
    // be sorted. This is not a functional requirement of s_AllSrcs, but a
    // requirement for it to be used in the sequential encoding check.
    MASSERT(s_NumRegisters == s_HighestRegEncoding - s_LowestRegEncoding + 1);
    MASSERT(s_NumRegisters <= s_AllSrcs.size());
    MASSERT(static_cast<UINT32>(s_AllSrcs.at(0)) == s_LowestRegEncoding);
    MASSERT(static_cast<UINT32>(s_AllSrcs.at(s_NumRegisters - 1))
            == s_HighestRegEncoding);

    UINT32 lwrEncoding = s_LowestRegEncoding;

    // For each register
    for (UINT32 i = 0; i < s_NumRegisters; ++i)
    {
        const bool isSequential
            = !(i + 1 < s_NumRegisters
                && (static_cast<UINT32>(s_AllSrcs.at(i))
                    != static_cast<UINT32>(s_AllSrcs.at(i + 1)) - 1));
        const bool isCorrectValue
            = (lwrEncoding == static_cast<UINT32>(s_AllSrcs.at(i)));

        if (!isSequential || !isCorrectValue)
        {
            Printf(Tee::PriError, "Register encodings are not sequential. "
                   "Required by MacroGenerator.\n");
            MASSERT(!"Register encodings are not sequential");
            break;
        }

        ++lwrEncoding;
    }

    MASSERT(lwrEncoding - 1 == s_HighestRegEncoding);
}

//!
//! \brief Generates a random Group.
//!
//! \param[in,out] pMacro Macro the group will be added to.
//! \param endNext True if the ENDNEXT field in the group global section should
//! be set, false otherwise.
//! \param dataRamSize Size of the usable/addressable MME Data RAM. Value is
//! ignored if Data RAM operations are not being generated.
//!
//! \return RC::ILWALID_INPUT if a valid group could not be generated.
//!
//! \see SetGenConfig Set generation configuration.
//!
RC MacroGenerator::GenGroup
(
    Macro* const pMacro
    ,bool endNext
    ,UINT32 dataRamSize
)
{
    // Group Restrictions
    //
    // - [x] Branch ops (BRT, BLTU, BLE, BLEU, BEQ, JAL, LOOP) cannot
    //   be present in a group with a predicate mode other than UUUU.
    // - [ ] There can only be one branch op in a group and it must be
    //   in ALU0.  Note, any operation in ALU1 is considered before
    //   the branch and always performed.
    // - [x] There can only be one STATE read in a group
    // - [x] Two DREAD or DWRITE ops can only be in a group if SW can
    //   guarantee they are to even/odd addresses so that they can be
    //   serviced by 2 data ram banks in parallel.
    // - [x] LOADn can only be used as a source if LOADn-1 is also used
    // - [x] If an ALU is exelwting a deferred write instruction
    //   (STATE, MUL, MULH, MULU, or MULHU), or a branch instruction
    //   (BLT, BLTU, BLE, BLEU, BEQ, JAL, LOOP), or a DWRITE
    //   instruction, then its ALU output cannot be used as a method
    //   or emit.
    // - [x] All ALU destinations must be either unique within the group or ZERO
    // - [x] ADDC and SUBB may not be in ALU0 and must be paired with
    //   ADD and SUB respectively in the next lower ALU
    // - [x] MULH and MULHU may not be in ALU0 and must be paired with
    //   MUL and MULU respectively in the next lower ALU
    //
    // Output limitations:
    //
    // - [x] STATE                  : ALU cannot be used as method/emit
    // - [x] DWRITE                 : ALU cannot be used as method/emit
    // - [x] EXTENDED WriteToSpecial: ALU cannot be used as method/emit
    //

    RC rc;
    MASSERT(pMacro);

    pMacro->groups.emplace_back();
    Group<s_NumGroupAlus>* pGroup = &pMacro->groups.back();

    m_PredModeBuilder.Reset();

    vector<Dst> validDsts(m_Dsts);

    // Invalid output values for this Group
    vector<Method> ilwalidOutMethods;
    vector<Emit> ilwalidOutEmits;

    const UINT32 aluCount = pGroup->GetAluCount();
    bool skipOp = false;            // Skip the next op code generation
    bool groupHasBranch = false;    // True if Group contains a branch
    bool groupHasStateRd = false;   // True if Group conatins STATE instruction
    UINT32 numGroupDataRamOps = 0;  // Num of group DREAD/DWRITE ops
    // The Data RAM address of the last DREAD/DWRITE. Only valid if
    // numGroupDataRamOps > 0
    UINT32 lastDataRamAddr = _UINT32_MAX;

    // Find ops for each ALU
    //

    // True if at least one ALU was generated within allowed number of attempts.
    bool aluGenerated = false;
    UINT32 numIterationsForAlu = 0;
    AluNum alu = 0;
    while (alu < aluCount)
    {
        Op op;

        // Ensure we don't spend too much time per ALU
        ++numIterationsForAlu;
        if (numIterationsForAlu >= s_MaxGenIterationsPerAlu)
        {
            Printf(Tee::PriLow, "Maximum number of attempts reached while "
                   "generating a valid ALU instruction. Forcing ALU%u to be a "
                   "null op.\n", alu);
            pGroup->Nop(alu);

            CHECK_RC(PostAluUpdate(alu, &ilwalidOutMethods, &ilwalidOutEmits,
                                   &groupHasBranch, pGroup));

            // Move to next ALU
            ++alu;
            numIterationsForAlu = 0;
            continue;
        }

        // Set ALU op fields
        if (!skipOp)
        {
            // All ops could have an immediate ilwolved, since another ALU may use
            // it in its calulwlation. Set it here and overwrite as necessary.
            pGroup->SetAluImmed(alu, RandImmed());

            op = RandOp();
            pGroup->SetAluOp(alu, op);

            switch (op)
            {
            case Op::Addc:
            case Op::Subb:
            case Op::Mulh:
            {
                const bool isLastAlu = (alu == (aluCount - 1));
                if (aluCount == 1 || isLastAlu)
                {
                    // Skip, these instructions must be paired with the next ALU
                    continue; // Pick diff op
                }
                else
                {
                    // Set this alu to non-carry op, and the next alu to the carry op
                    //
                    skipOp = true; // Skip gen for the next alu, setting it here

                    switch (op)
                    {
                    case Op::Addc:
                        pGroup->InstrAdd(alu, RandDst(&validDsts),
                                         RandSrc(), RandSrc());
                        pGroup->InstrAddc(alu + 1, RandDst(&validDsts),
                                          RandSrc(), RandSrc());
                        break;

                    case Op::Subb:
                        pGroup->InstrSub(alu, RandDst(&validDsts),
                                         RandSrc(), RandSrc());
                        pGroup->InstrSubb(alu + 1, RandDst(&validDsts),
                                          RandSrc(), RandSrc());
                        break;

                    case Op::Mulh:
                        // MULH has two forms:
                        // - MULH if the previous ALU is MUL
                        // - MULHU if the previous ALU is MULU
                        if (RandBool())
                        {
                            pGroup->InstrMul(alu, RandDst(&validDsts),
                                             RandSrc(), RandSrc());
                        }
                        else
                        {
                            pGroup->InstrMulu(alu, RandDst(&validDsts),
                                              RandSrc(), RandSrc());
                        }
                        pGroup->InstrMulh(alu + 1, RandDst(&validDsts));
                        break;
                    default:
                        MASSERT(!"Invalid op code");
                    }
                }
            } break;

            case Op::Add:
            case Op::Sub:
            case Op::Mul:
            case Op::Mulu:
            case Op::Sll:
            case Op::Srl:
            case Op::Sra:
            case Op::And:
            case Op::Nand:
            case Op::Or:
            case Op::Xor:
            case Op::Slt:
            case Op::Sltu:
            case Op::Sle:
            case Op::Sleu:
            case Op::Seq:
                pGroup->SetAluDst(alu, RandDst(&validDsts));
                pGroup->SetAluSrc0(alu, RandSrc());
                pGroup->SetAluSrc1(alu, RandSrc());
                break;

            case Op::Clz:
                pGroup->SetAluDst(alu, RandDst(&validDsts));
                pGroup->SetAluSrc0(alu, RandSrc());
                pGroup->SetAluSrc1(alu, Src::Zero);

                // src1 must be ZERO
                MASSERT(pGroup->GetAluSrc1(alu) == Src::Zero);
                break;

            case Op::Extended:
            {
                Subop subop = RandSubop();
                pGroup->SetAluSubop(alu, subop);

                switch (subop)
                {
                case Subop::ReadFromSpecial:
                    pGroup->SetAluDst(alu, RandDst(&validDsts));
                    pGroup->SetAluSrc0(alu, RandSrc());
                    pGroup->SetAluSrc1(alu, Src::Zero);

                    // src1 must be ZERO
                    MASSERT(pGroup->GetAluSrc1(alu) == Src::Zero);
                    break;

                case Subop::WriteToSpecial:
                    pGroup->SetAluDst(alu, Dst::Zero);
                    pGroup->SetAluSrc0(alu, RandSrc());
                    pGroup->SetAluSrc1(alu, RandSrc());

                    // ALU is invalid for output method/emit
                    OutputField field;
                    CHECK_RC(GetAluOutputField(alu, &field));
                    ilwalidOutMethods.push_back(static_cast<Method>(field));
                    ilwalidOutEmits.push_back(static_cast<Emit>(field));

                    // dst must be ZERO
                    MASSERT(pGroup->GetAluDst(alu) == Dst::Zero);
                    break;

                default:
                    MASSERT(!"Unknown subop code during instruction generation");
                    break;
                }
            } break;

            case Op::Merge:
            {
                pGroup->SetAluDst(alu, RandDst(&validDsts));

                const UINT32 maxSrcBitVal = static_cast<UINT32>(std::pow(2, Desc::Alu::SrcBit.size)) - 1;
                MASSERT(maxSrcBitVal <= _UINT08_MAX);
                const UINT32 maxWidthVal = static_cast<UINT32>(std::pow(2, Desc::Alu::Width.size)) - 1;
                MASSERT(maxWidthVal <= _UINT08_MAX);
                const UINT32 maxDstBitVal = static_cast<UINT32>(std::pow(2, Desc::Alu::DstBit.size)) - 1;
                MASSERT(maxDstBitVal <= _UINT08_MAX);

                pGroup->SetAluSrcBit(alu, static_cast<UINT08>(m_pFpCtx->Rand.GetRandom(0, maxSrcBitVal)));
                pGroup->SetAluWidth(alu, static_cast<UINT08>(m_pFpCtx->Rand.GetRandom(0, maxWidthVal)));
                pGroup->SetAluDstBit(alu, static_cast<UINT08>(m_pFpCtx->Rand.GetRandom(0, maxDstBitVal)));
            } break;

            case Op::State:
            {
                // Cannot have two STATE instructions in the same group
                if (groupHasStateRd)
                {
                    continue; // Pick diff op
                }
                groupHasStateRd = true;

                pGroup->SetAluDst(alu, RandDst(&validDsts));
                pGroup->SetAluSrc0(alu, RandSrc());
                pGroup->SetAluSrc1(alu, RandSrc());

                // ALU is invalid for output method/emit
                OutputField field;
                CHECK_RC(GetAluOutputField(alu, &field));
                ilwalidOutMethods.push_back(static_cast<Method>(field));
                ilwalidOutEmits.push_back(static_cast<Emit>(field));
            } break;

            case Op::Loop:
                // NOTE: Remember, offset is interpreted as signed.
                pGroup->SetAluDst(alu, Dst::Zero);
                pGroup->SetAluSrc0(alu, RandSrc()); // Loop count
                pGroup->SetAluSrc1(alu, Src::Zero);
                pGroup->SetAluOffset(alu, RandOffset());

                // dst and src1 must be ZERO
                MASSERT(pGroup->GetAluDst(alu) == Dst::Zero);
                MASSERT(pGroup->GetAluSrc1(alu) == Src::Zero);
                break;

            case Op::Jal:
                pGroup->SetAluDst(alu, RandDst(&validDsts));
                pGroup->SetAluSrc0(alu, RandSrc());
                pGroup->SetAluSrc1(alu, Src::Zero);
                pGroup->SetAluAddPc(alu, RandBool());
                pGroup->SetAluAddSrc0(alu, RandBool());
                pGroup->SetAluDelaySlot(alu, RandBool());
                pGroup->SetAluOffset(alu, RandOffset());

                // src1 must be ZERO
                MASSERT(pGroup->GetAluSrc1(alu) == Src::Zero);
                break;

            case Op::Blt:
            case Op::Bltu:
            case Op::Ble:
            case Op::Bleu:
            case Op::Beq:
                pGroup->SetAluDst(alu, Dst::Zero);
                pGroup->SetAluSrc0(alu, RandSrc());
                pGroup->SetAluBranchTrue(alu, RandBool());
                pGroup->SetAluPredictTaken(alu, RandBool());
                pGroup->SetAluDelaySlot(alu, RandBool());
                pGroup->SetAluOffset(alu, RandOffset());

                // dst must be ZERO
                MASSERT(pGroup->GetAluDst(alu) == Dst::Zero);
                break;

            case Op::Dread:
            {
                UINT32 dataRamAddr;
                if (!ValidatedDataRamOp(dataRamSize, &numGroupDataRamOps,
                                        &lastDataRamAddr, &dataRamAddr))
                {
                    continue; // Pick different op
                }

                pGroup->SetImmed(alu, dataRamAddr);
                pGroup->SetAluDst(alu, RandDst(&validDsts));
                pGroup->SetAluSrc0(alu, Src::Immed); // Index
                pGroup->SetAluSrc1(alu, Src::Zero);

                // src1 must be ZERO
                MASSERT(pGroup->GetAluSrc1(alu) == Src::Zero);
            } break;

            case Op::Dwrite:
            {
                UINT32 dataRamAddr;
                if (!ValidatedDataRamOp(dataRamSize, &numGroupDataRamOps,
                                        &lastDataRamAddr, &dataRamAddr))
                {
                    continue; // Pick different op
                }

                pGroup->SetImmed(alu, dataRamAddr);
                pGroup->SetAluDst(alu, Dst::Zero);
                pGroup->SetAluSrc0(alu, Src::Immed); // Index
                pGroup->SetAluSrc1(alu, RandSrc());  // Data

                // ALU is invalid for output method/emit
                OutputField field;
                CHECK_RC(GetAluOutputField(alu, &field));
                ilwalidOutMethods.push_back(static_cast<Method>(field));
                ilwalidOutEmits.push_back(static_cast<Emit>(field));

                // dst must be ZERO
                MASSERT(pGroup->GetAluDst(alu) == Dst::Zero);
            } break;

            default:
                MASSERT(!"Unknown op code during instruction generation");
                break;
            }
        }
        else
        {
            // Skipped one op, don't skip the next one
            skipOp = false;
        }

        CHECK_RC(PostAluUpdate(alu, &ilwalidOutMethods, &ilwalidOutEmits,
                               &groupHasBranch, pGroup));

        // Successfully generated an ALU
        aluGenerated = true;

        // Move to next ALU
        //
        ++alu;
        numIterationsForAlu = 0;
    }

    // Check that at least one ALU was generated.
    if (!aluGenerated)
    {
        Printf(Tee::PriWarn, "Unable to generate a valid ALU op\n");
        return RC::ILWALID_INPUT;
    }

    // Can't be more than 2 DREAD/DWRITE ops per group.
    MASSERT(numGroupDataRamOps <= 2);

    // Set output section
    //
    // Set output predicate values
    MASSERT(m_PredModeBuilder.AllAlusSet());
    for (UINT32 out = 0; out < s_NumGroupAlus; ++out)
    {
        // Groups that contain branch ops must have fully unconditional pred mode
        if (groupHasBranch || m_pGenConfig->unconditionalPredMode)
        {
            m_PredModeBuilder.SetNext(PredVal::U);
        }
        else
        {
            // Pick a random valid predicate value
            m_PredModeBuilder.SetNext(RandPredVal(m_PredModeBuilder.NextOptions()));
        }
    }
    MASSERT(m_PredModeBuilder.AllOutputsSet());

    // Set method and emit slot
    std::sort(ilwalidOutMethods.begin(), ilwalidOutMethods.end(),
              &::EnumCmp<Method>);
    std::sort(ilwalidOutEmits.begin(), ilwalidOutEmits.end(),
              &::EnumCmp<Emit>);
    CHECK_RC(GenOutputSection(&ilwalidOutMethods, &ilwalidOutEmits, pGroup));

    // Set global section
    //
    // Get the predicate mode
    PredMode predMode;
    rc = m_PredModeBuilder.Build(&predMode);
    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Unable to generate valid predicate mode\n"
               "\tIlwalid value: %s\n", m_PredModeBuilder.ToString().c_str());
        CHECK_RC(rc);
    }

    pGroup->SetGlobalEndNext(endNext);
    pGroup->SetGlobalPredMode(predMode);
    // TODO(stewarts): Make the predicate value predictable, for now
    // - To have a random predicate mode, it means that we:
    //   - Can't predict emissions during generation
    //     - The only source of the emission count would be what the simulator
    //       determines. Since the simulator does not expose what group released
    //       each emission, there is no way of associating an emission with its
    //       group.
    //   - Can't tell if an ALU is going to be exelwted
    //     - from my understanding, when an ALU is used as an output but not
    //       exelwted, its value is replaced with 0. This could lead to a number
    //       of emissions that contain 0. This may not be an issue.
    pGroup->SetGlobalPredReg(PredReg::Zero/*RandPredReg()*/);

    // Final validation checks
    //
    CHECK_RC(AssertBranchAndPredModeAgree(*pGroup));

    // Record group information
    //
    CHECK_RC(UpdateMacroMetadata(pGroup, pMacro));

    return rc;
}

//!
//! \brief Update generation state after setting an ALU.
//!
//! Arguments' values will be updated in accordance with the given generated
//! ALU information.
//!
//! \param alu ALU that was just generated.
//! \param[in,out] pIlwalidOutMethod Collection of invalid output.method values
//! for the given Group.
//! \param[in,out] pIlwalidOutEmit Collection of invalid output.emit values for
//! the given Group.
//! \param[in,out] pGroupHasBranch True if the group has a branch.
//! \param[in,out] pGroup Group being generated.
//!
RC MacroGenerator::PostAluUpdate
(
    AluNum alu
    ,vector<Method>* pIlwalidOutMethods
    ,vector<Emit>* pIlwalidOutEmits
    ,bool* pGroupHasBranch
    ,Group<s_NumGroupAlus>* pGroup
)
{
    RC rc;
    MASSERT(alu < s_NumGroupAlus);
    MASSERT(pIlwalidOutMethods);
    MASSERT(pIlwalidOutEmits);
    MASSERT(pGroup);

    const Op& aluOp = pGroup->GetAluOp(alu);

    // Update valid output section fields considerations
    //
    // Check ALU0 for branch to determine if the group contains a branch.
    // Branch must be in ALU0, and it is always the first ALU to be set.
    *pGroupHasBranch = IsBranchInstruction(pGroup->GetAluOp(0));

    // ALU performing a deferred write, branch, or write instruction cannot
    // be used in output
    if (IsDeferredWriteInstruction(aluOp) || IsBranchInstruction(aluOp)
        || aluOp == Op::Dwrite)
    {
        // Exclude the current value of `alu` from possible selection for
        // the Group
        OutputField field;
        CHECK_RC(GetAluOutputField(alu, &field));
        pIlwalidOutMethods->push_back(static_cast<Method>(field));
        pIlwalidOutEmits->push_back(static_cast<Emit>(field));
    }

    // Set ALU predicate value
    //
    // Groups that contain branch ops must have fully unconditional pred
    // mode
    if (*pGroupHasBranch || m_pGenConfig->unconditionalPredMode)
    {
        m_PredModeBuilder.SetNext(PredVal::U);
    }
    else
    {
        // Pick a random valid predicate value
        m_PredModeBuilder.SetNext(RandPredVal(m_PredModeBuilder.NextOptions()));
    }

    return rc;
}

//!
//! \brief Generate the output section of the given Group.
//!
//! \param ilwalidOutMethod Collection of invalid output.method values for the
//! given Group. Must be sorted.
//! \param ilwalidOutEmit Collection of invalid output.emit values for the given
//! Group. Must be sorted.
//! \param[out] pGroup Output section will be generated for this Group.
//!
RC MacroGenerator::GenOutputSection
(
    vector<Method>* pIlwalidOutMethods
    ,vector<Emit>* pIlwalidOutEmits
    ,Group<s_NumGroupAlus>* pGroup
)
{
    RC rc;
    MASSERT(pIlwalidOutMethods);
    MASSERT(pIlwalidOutEmits);
    MASSERT(pGroup);

    // Must be sorted for std::set_difference
    MASSERT(std::is_sorted(m_Methods.cbegin(), m_Methods.cend(),
                           &::EnumCmp<Method>));
    MASSERT(std::is_sorted(m_Emits.cbegin(), m_Emits.cend(),
                           &::EnumCmp<Emit>));
    MASSERT(std::is_sorted(pIlwalidOutMethods->begin(), pIlwalidOutMethods->end(),
                           &::EnumCmp<Method>));
    MASSERT(std::is_sorted(pIlwalidOutEmits->begin(), pIlwalidOutEmits->end(),
                           &::EnumCmp<Emit>));

    // Get the available valid selections for the output section
    //
    vector<Method> methodSelection;
    vector<Emit> emitSelection;

    const UINT64 methodSelectionSize
        = static_cast<UINT64>(
            std::abs(static_cast<FLOAT64>(m_Methods.size() - pIlwalidOutMethods->size())));
    const UINT64 emitSelectionSize
        = static_cast<UINT64>(
            std::abs(static_cast<FLOAT64>(m_Emits.size() - pIlwalidOutEmits->size())));
    methodSelection.reserve(methodSelectionSize);
    emitSelection.reserve(emitSelectionSize);

    std::set_difference(m_Methods.begin(), m_Methods.end(),
                        pIlwalidOutMethods->begin(), pIlwalidOutMethods->end(),
                        std::inserter(methodSelection, methodSelection.end()));
    std::set_difference(m_Emits.begin(), m_Emits.end(),
                        pIlwalidOutEmits->begin(), pIlwalidOutEmits->end(),
                        std::inserter(emitSelection, emitSelection.end()));

    // Set the output fields
    //
    for (AluNum alu = 0; alu < pGroup->GetAluCount(); ++alu)
    {
        Method method = RandMethod(methodSelection);
        Emit emit = RandEmit(emitSelection);

        pGroup->SetOutputMethod(alu, method);
        pGroup->SetOutputEmit(alu, emit);
    }

    return rc;
}

//!
//! \brief Update the Group and Macro information with the data from the given
//! Group.
//!
//! \param pGroup Newly generated group.
//! \param pMacro Macro associated with the generated group.
//!
RC MacroGenerator::UpdateMacroMetadata
(
    Group<s_NumGroupAlus>* pGroup,
    Macro* pMacro
) const
{
    RC rc;
    MASSERT(pGroup);
    MASSERT(pMacro);
    // Should have either set the emission to 0 or be building off old value
    MASSERT(pMacro->numEmissions != Macro::NUM_EMISSIONS_UNKNOWN);

    // Update the number of emissions
    //
    string predMode = s_PredModeName.at(pGroup->GetGlobalPredMode());
    const UINT32 aluCount = pGroup->GetAluCount();
    MASSERT(aluCount * 2 == predMode.size()); // ALU:Output 1:1 => num(ALU) * 2

    UINT32 numGroupEmissions = 0;
    for (OutNum out = 0; out < aluCount; ++out)
    {
        // Don't consider method emissions in the emission count
        if (pGroup->GetOutputEmit(out) == Emit::None)
        {
            continue;
        }

        // Only need to consider the output predicate values to determine
        // emissions
        const UINT32 i = aluCount + static_cast<UINT32>(out);
        switch (predMode[i])
        {
        case 'U':
            // TODO(stewarts): check the emission behaviour when there is a branch
            ++numGroupEmissions;
            break;
        case 'T':
            ++numGroupEmissions;
            break;
        case 'F':
            break; // No emission
        default:
            MASSERT(!"Unhandled predicate value");
        }
    }

    // Update emission counts
    pGroup->SetNumEmissions(numGroupEmissions);
    pMacro->numEmissions += numGroupEmissions;

    return rc;
}

//!
//! \brief Prepare the given macro for additional groups being appended to it.
//!
//! Keeps the emission count if it is a non-empty macro, otherwise sets it to
//! zero.
//!
//! \param[in,out] pMacro Macro that will be added to.
//!
RC MacroGenerator::PrepareMacroForGroupAddition(Macro* pMacro) const
{
    RC rc;
    MASSERT(pMacro);

    Macro::Groups& groups = pMacro->groups;

    if (!groups.empty()) // Adding to existing macro
    {
        MASSERT(groups.size() >= 2); // Need at least two groups to make a macro
        MASSERT(pMacro->numEmissions != Macro::NUM_EMISSIONS_UNKNOWN);

        // If appending to an existing macro, must remove ENDNEXT of the
        // previous macro
        MASSERT(groups.size() >= 2); // Need at least two groups to make a macro
        MASSERT(pMacro->numEmissions != Macro::NUM_EMISSIONS_UNKNOWN);

        const UINT32 endNextIndex = static_cast<UINT32>(groups.size()) - 2;
        MASSERT(endNextIndex == static_cast<UINT32>(groups.size()) - 2); // Truncation check
        auto& endNextGroup = groups[endNextIndex];
        endNextGroup.SetGlobal(false,
                               endNextGroup.GetGlobalPredMode(),
                               endNextGroup.GetGlobalPredReg());
    }
    else // New macro
    {
        pMacro->numEmissions = 0;
    }

    // Macro is going to be changed, so it can be considered an entirely new
    // macro
    pMacro->cycleCount = Macro::CYCLE_COUNT_UNKNOWN; // Has not been run
    pMacro->mmeTableEntry = Macro::NOT_LOADED;       // Has not been loaded

    return rc;
}

//!
//! \brief Collect the possible values for standard operations based on the
//! generation configuration settings.
//!
RC MacroGenerator::CollectStandardOpSelections()
{
    RC rc;

    const OpSelectionFlags flags = m_pGenConfig->flags;
    vector<const vector<Op>*> toAdd;
    UINT32 sizeToAdd = 0;

    if (flags & OPS_ARITHMETIC)
    {
        toAdd.push_back(&s_ArithmeticOps);
        sizeToAdd += static_cast<UINT32>(s_ArithmeticOps.size());
        MASSERT(sizeToAdd >= s_ArithmeticOps.size()); // Truncation check
    }

    if (flags & OPS_LOGIC)
    {
        toAdd.push_back(&s_LogicOps);
        sizeToAdd += static_cast<UINT32>(s_LogicOps.size());
        MASSERT(sizeToAdd >= s_LogicOps.size()); // Truncation check
    }

    if (flags & OPS_COMP)
    {
        toAdd.push_back(&s_ComparisonOps);
        sizeToAdd += static_cast<UINT32>(s_ComparisonOps.size());
        MASSERT(sizeToAdd >= s_ComparisonOps.size()); // Truncation check
    }

    if (flags & OPS_DATA_RAM)
    {
        toAdd.push_back(&s_DataRamOps);
        sizeToAdd += static_cast<UINT32>(s_DataRamOps.size());
        MASSERT(sizeToAdd >= s_DataRamOps.size()); // Truncation check
    }

    // Add the selections
    m_StandardOps.reserve(sizeToAdd);
    for (const vector<Op>* p: toAdd)
    {
        m_StandardOps.insert(m_StandardOps.end(),
                             p->begin(), p->end());
    }

    return rc;
}

bool MacroGenerator::IsBranchInstruction(Op op) const
{
    switch (op)
    {
    case Op::Loop:
    case Op::Jal:
    case Op::Blt:
    case Op::Bltu:
    case Op::Ble:
    case Op::Bleu:
    case Op::Beq:
        return true;
    default:
        return false;
    }
}

bool MacroGenerator::IsDeferredWriteInstruction(Op op) const
{
    switch (op)
    {
    case Op::State:
    case Op::Mul:
    case Op::Mulu:
    case Op::Mulh: // MULH & MULHU
        return true;
    default:
        return false;
    }
}

//!
//! \brief Assert groups that contain a branch must have a fully unconditional
//! predicate mode.
//!
//! Assumes that the branch op has already been validated with respect to the
//! other ALUs.
//!
RC MacroGenerator::AssertBranchAndPredModeAgree
(
    const Group<s_NumGroupAlus>& group
) const
{
    RC rc;

    // Groups that contain a branch must have fully unconditional pred mode
    if (IsBranchInstruction(group.GetAluOp(0))) // Branch must be in ALU 0
    {
        // Must already be a valid pred mode
        string predMode = s_PredModeName.at(group.GetGlobalPredMode());
        char unconditional = s_PredValName.at(PredVal::U);

        const bool allUncond = std::find_if(predMode.cbegin(), predMode.cend(),
                                            [=] (const char& predVal)
                                            {
                                                return predVal != unconditional;
                                            }) == predMode.cend();

        if (!allUncond)
        {
            Printf(Tee::PriError, "Branch without fully unconditional predicate"
                   " mode: %s\n", predMode.c_str());
            MASSERT(!"Branch without fully unconditional predicate mode");
        }
    }

    return rc;
}

//!
//! \brief Return true if it is valid to add a Data RAM op, and false o/w.
//!
//! The given values are updated if valid and a valid Data RAM address is
//! produced.
//!
//! \param dataRamSize Size of the Data RAM.
//! \param[in,out] pNumGroupDataRamOps Number of existing Data RAM ops in a
//! group. Incremented if valid to add a Data RAM op.
//! \param[in,out] pLastDataRamAddr Data RAM address of the last Data RAM op.
//! Updated if valid to add a Data RAM op.
//! \param[out] pDataRamAddr Random valid address for the next Data RAM op.
//!
bool MacroGenerator::ValidatedDataRamOp
(
    UINT32 dataRamSize
    ,UINT32* pNumGroupDataRamOps
    ,UINT32* pLastDataRamAddr
    ,UINT32* pDataRamAddr
) const
{
    MASSERT(pNumGroupDataRamOps);
    MASSERT(pLastDataRamAddr);
    MASSERT(pDataRamAddr);

    // Max of two DREAD/DWRITE ops in single group and one must be
    // to an even address and one must be to an odd address.
    if (*pNumGroupDataRamOps >= 2
        || (*pNumGroupDataRamOps >= m_pGenConfig->maxDataRamEntry))
    {
        return false;
    }

    // DREAD and DWRITE address must be within the Data RAM usable
    // space. To garantee this, the immediate is explicitly set and
    // used as the destination.

    // Limit the used address range to the specified number of entries
    // in the gen config.
    UINT32 maxDataRamEntry
        = (m_pGenConfig->maxDataRamEntry == Config::MAX_DATA_RAM_ENTRY_ALL
           ? dataRamSize
           : std::min(m_pGenConfig->maxDataRamEntry, dataRamSize));

    UINT32 dataRamAddr = RandDataRamAddr(maxDataRamEntry);
    if (*pNumGroupDataRamOps > 0)
    {
        // There has been a previous data ram op. The addresses must not
        // conflict: must be one even and one odd address.
        const bool lastAddrEven = *pLastDataRamAddr % 2 == 0;
        const bool lwrAddrEven = dataRamAddr % 2 == 0;

        if (lastAddrEven == lwrAddrEven)
        {
            // The addresses are both even or odd. Change the parity of our
            // current op's address.
            if (dataRamAddr == 0) // > 0
            {
                dataRamAddr += 1;
            }
            else // < max addr
            {
                dataRamAddr -= 1;
            }
        }
    }

    // Update data ram generation state info
    *pNumGroupDataRamOps += 1;
    *pLastDataRamAddr = dataRamAddr;
    *pDataRamAddr = dataRamAddr;

    return true;
}

//!
//! \brief Return the given ALU's corresponding output field encoding.
//!
//! \param alu ALU number.
//! \param[out] pField ALU's output field encoding.
//!
RC MacroGenerator::GetAluOutputField(AluNum alu, OutputField* pField) const
{
    RC rc;

    auto iter = s_AluToOutputField.find(alu);
    if (iter == s_AluToOutputField.end())
    {
        Printf(Tee::PriError,
               "Unknown method encoding for ALU%u\n", alu);
        return RC::SOFTWARE_ERROR;
    }

    *pField = iter->second;

    return rc;
}

Op MacroGenerator::RandOp() const
{
    ASSERT_NOT_EMPTY(m_StandardOps);
    const UINT32 numStdOps = static_cast<UINT32>(m_StandardOps.size());
    MASSERT(numStdOps == m_StandardOps.size()); // Truncation check

    return *std::next(m_StandardOps.begin(),
                      m_pFpCtx->Rand.GetRandom(0,  numStdOps - 1));
}

//!
//! \brief Pick a random valid predicate value from the given selection.
//!
PredVal MacroGenerator::RandPredVal
(
    const vector<PredVal>& selection
) const
{
    ASSERT_NOT_EMPTY(selection);
    const UINT32 numSelections = static_cast<UINT32>(selection.size());
    MASSERT(numSelections == selection.size()); // Truncation check

    return *std::next(selection.begin(),
                      m_pFpCtx->Rand.GetRandom(0, numSelections - 1));
}

SpecialReg MacroGenerator::RandSpecialReg() const
{
    ASSERT_NOT_EMPTY(s_SpecialRegs);
    const UINT32 numSpecialRegs = static_cast<UINT32>(s_SpecialRegs.size());
    MASSERT(numSpecialRegs == s_SpecialRegs.size()); // Truncation check

    return *std::next(s_SpecialRegs.begin(),
                      m_pFpCtx->Rand.GetRandom(0, numSpecialRegs - 1));
}

Subop MacroGenerator::RandSubop() const
{
    ASSERT_NOT_EMPTY(s_Subops);
    const UINT32 numSubops = static_cast<UINT32>(s_Subops.size());
    MASSERT(numSubops == s_Subops.size()); // Truncation check

    return *std::next(s_Subops.begin(),
                      m_pFpCtx->Rand.GetRandom(0, numSubops - 1));
}

//!
//! \brief Return a random valid source slot value.
//!
Src MacroGenerator::RandSrc()
{
    ASSERT_NOT_EMPTY(m_Srcs);
    const UINT32 numSrcs = static_cast<UINT32>(m_Srcs.size());
    MASSERT(numSrcs == m_Srcs.size()); // Truncation check

    Src src = *std::next(m_Srcs.begin(),
                         m_pFpCtx->Rand.GetRandom(0, numSrcs - 1));

    if (IsLoad(src))
    {
        // Should only have LOAD0 in the selection pool
        MASSERT(src == Src::Load0);

        m_LoadInstrPicker.Rand(&src);
    }

    return src;
}

//!
//! \brief Return a random valid destination slot value.
//!
//! \param selection Possible slot values to return.
//!
Dst MacroGenerator::RandDst(vector<Dst>* pSelection) const
{
    MASSERT(pSelection);
    MASSERT(!pSelection->empty());
    const UINT32 numSelections = static_cast<UINT32>(pSelection->size());
    MASSERT(numSelections == pSelection->size()); // Truncation check

    auto it = std::next(pSelection->begin(),
                        m_pFpCtx->Rand.GetRandom(0, numSelections - 1));

    Dst dst = *it;
    if (dst != Dst::Zero)
    {
        // A non-zero destination can't be used multiple times within
        // the same group.
        pSelection->erase(it);
    }

    return dst;
}
// TODO(stewarts):
PredReg MacroGenerator::RandPredReg() const
{
    ASSERT_NOT_EMPTY(s_AllPredRegs);
    const UINT32 numPredRegs = static_cast<UINT32>(s_AllPredRegs.size());
    MASSERT(numPredRegs == s_AllPredRegs.size()); // Truncation check

    return *std::next(s_AllPredRegs.begin(),
                      m_pFpCtx->Rand.GetRandom(0, numPredRegs - 1));
}

//!
//! \brief Return a random valid method slot value.
//!
//! \param selection Possible slot values to return.
//!
Method MacroGenerator::RandMethod
(
    const vector<Method>& selection
)
{
    ASSERT_NOT_EMPTY(selection);
    const UINT32 numSelections = static_cast<UINT32>(selection.size());
    MASSERT(numSelections == selection.size()); // Truncation check

    Method meth = *std::next(selection.begin(),
                             m_pFpCtx->Rand.GetRandom(0, numSelections - 1));

    if (IsLoad(meth))
    {
        // Should only have LOAD0 in the selection pool
        MASSERT(meth == Method::Load0);

        m_LoadInstrPicker.Rand(&meth);
    }

    return meth;
}

//!
//! \brief Return a random valid method slot value.
//!
//! \param selection Possible slot values to return.
//!
Emit MacroGenerator::RandEmit
(
    const vector<Emit>& selection
)
{
    ASSERT_NOT_EMPTY(selection);
    const UINT32 numSelections = static_cast<UINT32>(selection.size());
    MASSERT(numSelections == selection.size());

    Emit emit = *std::next(selection.begin(),
                           m_pFpCtx->Rand.GetRandom(0, numSelections - 1));

    if (IsLoad(emit))
    {
        // Should only have LOAD0 in the selection pool
        MASSERT(emit == Emit::Load0);

        m_LoadInstrPicker.Rand(&emit);
    }

    return emit;
}

UINT16 MacroGenerator::RandImmed() const
{
    const UINT32 maxImmed = static_cast<UINT32>(std::pow(2, Desc::Alu::Immed.size)) - 1;
    MASSERT(maxImmed <= _UINT16_MAX);
    return static_cast<UINT16>(m_pFpCtx->Rand.GetRandom(0, maxImmed));
}

bool MacroGenerator::RandBool() const
{
    return ((m_pFpCtx->Rand.GetRandom(0, 1) > 0) ? true : false);
}

UINT16 MacroGenerator::RandOffset() const
{
    const UINT32 maxOffset = static_cast<UINT32>(std::pow(2, Desc::Alu::Offset.size)) - 1;
    MASSERT(maxOffset <= _UINT16_MAX);
    return static_cast<UINT16>(m_pFpCtx->Rand.GetRandom(0, maxOffset));
}

//!
//! \brief Return a random Data RAM address.
//!
//! NOTE: No matter the Data FIFO configuration, the Data RAM indexing is always
//! from 0 to the usable RAM size minus one.
//!
//! \param dataRamSize The size of the usable/addressable Data RAM.
//!
UINT32 MacroGenerator::RandDataRamAddr(UINT32 dataRamSize) const
{
    return m_pFpCtx->Rand.GetRandom(0, dataRamSize - 1);
}

#undef ASSERT_NOT_EMPTY
