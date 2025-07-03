/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/fpicker.h"
#include "core/include/massert.h"
#include "core/include/trie.h"
#include "core/include/types.h"
#include "mme64macro.h"

#include <memory>
#include <type_traits>

namespace Mme64
{
    //!
    //! \brief Creates predicate modes.
    //!
    //! \see Mme64::PredMode
    //!
    template <AluNum AluCount>
    class PredModeBuilder
    {
    public:
        //! Number of predicate values in the predicate mode.
        static constexpr UINT32 s_NumPredVals = AluCount * 2;

        using iter = typename vector<PredVal>::iterator;
        using revIter = typename vector<PredVal>::reverse_iterator;
        using constIter = typename vector<PredVal>::const_iterator;
        using constRevIter = typename vector<PredVal>::const_reverse_iterator;

        PredModeBuilder();

        void Reset();

        vector<PredVal> NextOptions() const;
        void            SetNext(PredVal predVal);
        UINT32          NumSetPredVals() const;

        PredVal         Get(UINT32 index) const;
        PredVal         GetAlu(AluNum alu) const;
        PredVal         GetOutput(OutNum out) const;

        bool AllAlusSet() const;
        bool AllOutputsSet() const;
        bool Ready() const;
        RC   Build(PredMode* pPredMode) const;

        void SetAllUncond();
        void SetUncondAluFalseOutput();

        iter         begin()         { return m_PredMode.begin();   }
        constIter    begin()   const { return m_PredMode.begin();   }
        constIter    cbegin()  const { return m_PredMode.cbegin();  }
        iter         end()           { return m_PredMode.end();     }
        constIter    end()     const { return m_PredMode.end();     }
        constIter    cend()    const { return m_PredMode.cend();    }
        revIter      rbegin()        { return m_PredMode.rbegin();  }
        constRevIter rbegin()  const { return m_PredMode.rbegin();  }
        constRevIter crbegin() const { return m_PredMode.crbegin(); }
        revIter      rend()          { return m_PredMode.rend();    }
        constRevIter rend()    const { return m_PredMode.rend();    }
        constRevIter crend()   const { return m_PredMode.crend();   }

        string ToString() const;

    private:
        //!< Predicate mode being built.
        vector<PredVal> m_PredMode;
        //!< Contains the valid predicate mode configurations.
        Trie<vector<PredVal>> m_ValidPredModeTrie;
    };

    //!
    //! \brief LOADn instruction picker.
    //!
    //! LOADn instruction refers to the actual instructions LOAD0, LOAD1, and so
    //! on.
    //!
    //! Manages the MME64 restrictions on LOADn instructions. Specifically:
    //! - LOADn will not be picked until LOADn-1 has been picked.
    //! - Tracks the number of unique load instructions to know the amount of
    //!   input fifo data required. When two LOADn instructions of the same n
    //!   are used in a single group, it counts as a single fifo read; both
    //!   instructions refer to the same data.
    //!
    //! These restrictions are tracked since the last reset. This means if a
    //! reset is done for every group, the values will be applied to a single
    //! group. Note that this is the intended use case.
    //!
    // TODO(stewarts): Is it a problem that LOAD0 will always occur in an
    // earlier ALU than LOAD1?
    class LoadInstrPicker
    {
    public:
        LoadInstrPicker(FancyPicker::FpContext* m_pFpCtx);

        RC Reset();
        void Rand(Src* pSrc);
        void Rand(Method* pMethod);
        void Rand(Emit* pEmit);

        UINT32 NumUniqueLoads() const;

    private:
        FancyPicker::FpContext* m_pFpCtx;
        UINT32 m_MaxN;         //!< Maximum supported N.
        UINT32 m_NextHighestN; //!< Next highest 'n' value for the LOADn
                               //!< instructions since the last reset.

        UINT32 PickN();

        /* Possible LOADn values */
        // NOTE: The LOADn instructions must be in increasing 'n' order starting
        // at index 0.
        static vector<Src> s_SrcLoads;
        static vector<Method> s_MethodLoads;
        static vector<Emit> s_EmitLoads;
    };

    enum OpSelectionFlags
    {
        OPS_ARITHMETIC  = 1 << 0
        ,OPS_LOGIC      = 1 << 1
        ,OPS_COMP       = 1 << 2
        ,OPS_FLOW_CTRL  = 1 << 3
        ,OPS_EXTENDED   = 1 << 4
        ,OPS_STATE      = 1 << 5
        ,OPS_DATA_RAM   = 1 << 6
    };

    //!
    //! \brief Provides an interface for generating MME64 (Method Macro Expander
    //! 64) groups, which make up an MME64 macro.
    //!
    //! ISA: http://p4viewer/get/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx
    //!
    class MacroGenerator
    {
    public:
        //!
        //! \brief Group generation configuration.
        //!
        struct Config
        {
            enum
            {
                MAX_NUM_REGISTERS_ALL = 0,
                MAX_DATA_RAM_ENTRY_ALL = 0
            };

            //! Select which instruction types to generate.
            OpSelectionFlags flags;
            //! Percent of flow control instructions (between 0 and 1).
            FLOAT32 percentFlowControlOps;
            //! If true, no load ops in source, destination, or output.
            bool noLoad = false;
            //! If true, no Groups will send methods down pipeline.
            bool noSendingMethods = false;
            //! If true, all predicate modes will be unconditional.
            bool unconditionalPredMode = false;
            //! Limits the registers used during generation to the first n
            //! registers. If 0, uses all the available registers.
            UINT32 maxNumRegisters = MAX_NUM_REGISTERS_ALL;
            //! Limits the accessed Data RAM to the first n entries. If 0, uses
            //! the full Data RAM. Values above the max size of the data RAM
            //! will use the full Data RAM.
            UINT32 maxDataRamEntry = MAX_DATA_RAM_ENTRY_ALL;

            Config() = default;
        };

        static constexpr UINT32 GetNumGroupsInRegfileSetter(UINT32 numAlus);
        static constexpr UINT32 GetNumGroupsInMmeConfigSetup(UINT32 numAlus);
        static constexpr UINT32 GetNumGroupsInDataRamSetter
        (
            UINT32 numAlus
            ,UINT32 numDataRamEntries
        );

        MacroGenerator(FancyPicker::FpContext* pFpCtx);

        RC GenRegfileSetter(bool randValues, Macro* pMacro);
        RC GenMmeConfigSetup(Macro* pMacro);
        RC GenDataRamSetter
        (
            UINT32 startDataRamEntry
            ,UINT32 numEntriesToSet
            ,bool randValues
            ,Macro* pMacro
        );

        RC SetGenConfig(unique_ptr<Config> genConfig);
        RC GenMacro(UINT32 numGroups, UINT32 dataRamSize, Macro* pOutMacro);

    private:
        //! Maximum attempts to create a valid ALU op.
        static UINT32 s_MaxGenIterationsPerAlu;

        FancyPicker::FpContext* m_pFpCtx;
        PredModeBuilder<s_NumGroupAlus> m_PredModeBuilder;
        LoadInstrPicker m_LoadInstrPicker;

        unique_ptr<Config> m_pGenConfig;
        vector<Op> m_StandardOps;

        vector<Src> m_Srcs;
        vector<Dst> m_Dsts;
        vector<Method> m_Methods;
        vector<Emit> m_Emits;

        void SequentialRegisterEncodingCheck() const;

        template <class T>
        bool FilterLoad(const T& slot) const;
        template <class T>
        bool FilterRegs(const T& slot) const;

        RC GenGroup(Macro* const pMacro, bool endNext, UINT32 dataRamSize);
        RC PostAluUpdate
        (
            AluNum alu
            ,vector<Method>* pIlwalidOutMethods
            ,vector<Emit>* pIlwalidOutEmits
            ,bool* pGroupHasBranch
            ,Group<s_NumGroupAlus>* pGroup
        );
        RC GenOutputSection
        (
            vector<Method>* pIlwalidOutMethods
            ,vector<Emit>* pIlwalidOutEmits
            ,Group<s_NumGroupAlus>* pGroup
        );
        RC UpdateMacroMetadata
        (
            Group<s_NumGroupAlus>* pGroup,
            Macro* pMacro
        ) const;
        RC PrepareMacroForGroupAddition(Macro* pMacro) const;

        RC CollectStandardOpSelections();
        bool IsBranchInstruction(Op op) const;
        bool IsDeferredWriteInstruction(Op op) const;
        RC AssertBranchAndPredModeAgree
        (
            const Group<s_NumGroupAlus>& group
        ) const;
        bool ValidatedDataRamOp
        (
            UINT32 dataRamSize
            ,UINT32* pNumGroupDataRamOps
            ,UINT32* pLastDataRamAddr
            ,UINT32* pDataRamAddr
        ) const;

        //!
        //! \brief True if the given slot value is LOADn.
        //!
        template <class T,
                  typename std::enable_if_t<std::is_same<T, Src>::value
                                            || std::is_same<T, Method>::value
                                            || std::is_same<T, Emit>::value>* = nullptr>
        constexpr
        bool IsLoad(T slot) const
        {
            return slot == T::Load0 || slot == T::Load1;
        }

        //!
        //! \brief True if the given slot is a register.
        //!
        //! NOTE: Relies on registers having sequential encodings.
        //!
        template <class T,
                  typename std::enable_if_t<std::is_same<T, SrcDst>::value
                                            || std::is_same<T, Src>::value
                                            || std::is_same<T, Dst>::value>* = nullptr>
        constexpr
        bool IsRegister(T slot) const
        {
            return s_LowestRegEncoding <= static_cast<UINT32>(slot)
                && static_cast<UINT32>(slot) <= s_HighestRegEncoding;
        }

        RC GetAluOutputField(AluNum alu, OutputField* pField) const;
        RC GetPredModeExecAlusNoEmissions(PredMode* pPredMode) const;

        /* Random field value pickers */
        Op RandOp() const;
        PredVal RandPredVal(const vector<PredVal>& selection) const;
        SpecialReg RandSpecialReg() const;
        Subop RandSubop() const;
        Src RandSrc();
        Dst RandDst(vector<Dst>* selection) const;
        PredReg RandPredReg() const;
        Method RandMethod(const vector<Method>& selection);
        Emit RandEmit(const vector<Emit>& selection);
        UINT16 RandImmed() const;
        bool RandBool() const;
        UINT16 RandOffset() const;
        UINT32 RandDataRamAddr(UINT32 dataRamSize) const;
    };

    //--------------------------------------------------------------------------

    //!
    //! \brief Constructor.
    //!
    //! NOTE: Defaults the predicate mode to unconditional predicate values.
    //!
    template <AluNum AluCount>
    PredModeBuilder<AluCount>::PredModeBuilder()
    {
        m_PredMode.reserve(s_NumPredVals);

        // Populate predicate mode trie with all valid predicate modes
        vector<PredVal> validPredMode;
        validPredMode.reserve(s_NumPredVals);

        for (const auto& iter : s_PredModeName)
        {
            validPredMode.clear();
            for (const char c : iter.second)
            {
                validPredMode.push_back(s_PredValEncoding.at(c));
            }

            m_ValidPredModeTrie.Insert(validPredMode);
        }

        // Prepare to build a new predicate mode
        Reset();
    }

    //!
    //! \brief Start building a new predicate mode.
    //!
    template <AluNum AluCount>
    void PredModeBuilder<AluCount>::Reset()
    {
        m_PredMode.clear();
        MASSERT(m_PredMode.empty());
    }

    //!
    //! \brief Return all valid predicate values for the next predicate value.
    //!
    //! Possible values are based on the lwrrently selected predicate values.
    //!
    template <AluNum AluCount>
    vector<PredVal> PredModeBuilder<AluCount>::NextOptions() const
    {
        vector<PredVal> possiblePredVals
            = m_ValidPredModeTrie.Follows(m_PredMode.begin(), m_PredMode.end());

        // Either we should have possible values, or be ready to build a
        // predicate mode.
        MASSERT(!possiblePredVals.empty()
                || (possiblePredVals.empty() && Ready()));

        return possiblePredVals;
    }

    //!
    //! \brief Set the next predicate value in the predicate mode.
    //!
    //! The predicate must be valid based on the previously set predicate
    //! values.
    //!
    //! \see NextOptions
    //!
    template <AluNum AluCount>
    void PredModeBuilder<AluCount>::SetNext(PredVal predVal)
    {
        MASSERT(m_PredMode.size() < s_NumPredVals);

        // PredVal must be a valid predicate value
        vector<PredVal> possiblePredVals = NextOptions();
        if (possiblePredVals.end() == std::find(possiblePredVals.begin(),
                                                possiblePredVals.end(),
                                                predVal))
        {
            MASSERT(!"Attempting to set an invalid predicate value");
            (void)possiblePredVals;
        }

        m_PredMode.push_back(predVal);
    }

    //!
    //! \brief Return the number of predicate values that have been set so far
    //! to build the predicate mode.
    //!
    template <AluNum AluCount>
    UINT32 PredModeBuilder<AluCount>::NumSetPredVals() const
    {
        return m_PredMode.size();
    }

    //!
    //! \brief Get the predicate value of the given position in the
    //! predicate mode.
    //!
    template <AluNum AluCount>
    PredVal PredModeBuilder<AluCount>::Get(UINT32 index) const
    {
        // Must be getting a previously set value
        MASSERT(index < m_PredMode.size());

        return m_PredMode[index];
    }

    //!
    //! \brief Get the given ALU's predicate value.
    //!
    template <AluNum AluCount>
    PredVal PredModeBuilder<AluCount>::GetAlu(AluNum alu) const
    {
        MASSERT(alu < s_NumPredVals / 2);
        // Must be getting a previously set value
        MASSERT(alu < m_PredMode.size());

        return m_PredMode[alu];
    }

    //!
    //! \brief Get the given Output's predicate value.
    //!
    template <AluNum AluCount>
    PredVal PredModeBuilder<AluCount>::GetOutput(OutNum out) const
    {
        MASSERT(out < s_NumPredVals / 2);
        // Must be getting a previously set value
        MASSERT((out + s_NumPredVals / 2) < m_PredMode.size());

        return m_PredMode[out + s_NumPredVals / 2];
    }

    //!
    //! \brief True if all the ALU predicate values have been set.
    //!
    template <AluNum AluCount>
    bool PredModeBuilder<AluCount>::AllAlusSet() const
    {
        // ALUs in in the first half of the predicate mode
        return m_PredMode.size() >= s_NumPredVals / 2;
    }

    //!
    //! \brief True if all the output predicate values have been set.
    //!
    template <AluNum AluCount>
    bool PredModeBuilder<AluCount>::AllOutputsSet() const
    {
        // ALUs in in the second half of the predicate mode
        return m_PredMode.size() >= s_NumPredVals;
    }

    //!
    //! \brief Return true if ready to build a predicate mode.
    //!
    template <AluNum AluCount>
    bool PredModeBuilder<AluCount>::Ready() const
    {
        MASSERT(m_PredMode.size() <= s_NumPredVals);
        MASSERT(AllAlusSet() && AllOutputsSet());
        return m_PredMode.size() == s_NumPredVals;
    }

    //!
    //! \brief Get the predicate mode corresponding to the lwrrently constructed
    //! predicate mode.
    //!
    template <AluNum AluCount>
    RC PredModeBuilder<AluCount>::Build(PredMode* pPredMode) const
    {
        RC rc;
        MASSERT(Ready());

        string s = ToString();

        auto iter = s_PredModeEncoding.find(s);
        if (iter == s_PredModeEncoding.end())
        {
            Printf(Tee::PriError,
                   "Attempting to build invalid predicate mode: %s\n",
                   s.c_str());
            MASSERT(!"See previous error");
            return RC::CANNOT_ENUMERATE_OBJECT;
        }

        *pPredMode = iter->second;

        return rc;
    }

    //!
    //! \brief Set the ALUs to uncoditional and set the outputs to false.
    //!
    template <AluNum AluCount>
    void PredModeBuilder<AluCount>::SetUncondAluFalseOutput()
    {
        Reset();

        // Set ALUs to unconditional
        for (UINT32 n = 0; n < AluCount; ++n)
        {
            SetNext(PredVal::U);
        }

        // Set outputs to false
        for (UINT32 n = 0; n < AluCount; ++n)
        {
            SetNext(PredVal::F);
        }

        MASSERT(Ready());
    }

    //!
    //! \brief Set the full predicate mode to unconditional.
    //!
    template <AluNum AluCount>
    void PredModeBuilder<AluCount>::SetAllUncond()
    {
        Reset();

        for (UINT32 n = 0; n < AluCount * 2; ++n)
        {
            SetNext(PredVal::U);
        }

        MASSERT(Ready());
    }

    template <AluNum AluCount>
    string PredModeBuilder<AluCount>::ToString() const
    {
        string s;

        for (const PredVal& predVal : m_PredMode)
        {
            auto iter = s_PredValName.find(predVal);
            if (iter == s_PredValName.end())
            {
                MASSERT(!"Unsupported PredVal found\n");
                s += '?';
            }
            else
            {
                s += iter->second;
            }
        }

        return s;
    }

    //--------------------------------------------------------------------------
    //!
    //! \brief Return number of groups in the register file setter macro.
    //!
    //! \param numAlus Number of ALUs per group.
    //!
    //! \see GenRegfileSetter Generates register file setter macro.
    //!
    /* static */ constexpr
    UINT32 MacroGenerator::GetNumGroupsInRegfileSetter(UINT32 numAlus)
    {
        return (s_HighestRegEncoding
                - s_LowestRegEncoding + 1) / numAlus;
    }

    //!
    //! \brief Return number of groups in the MME configuration setup macro.
    //!
    //! \param numAlus Number of ALUs per group.
    //!
    //! \see GenMmeStateSetup Generates MME state setup macro.
    //!
    /* static */ constexpr
    UINT32 MacroGenerator::GetNumGroupsInMmeConfigSetup(UINT32 numAlus)
    {
        // Lwrrently, the generation algorithm does not take into account the
        // number of ALUs for optimization.
        return 2;
    }

    //!
    //! \brief Return the number of groups in the Data RAM setter macro.
    //!
    //! \param numAlus Number of ALUs per group.
    //! \param numDataRamEntries Number of Data RAM entries to set in the macro.
    //!
    //! \see GenDataRamSetter Generate Data RAM setter macro.
    //!
    /* static */ constexpr
    UINT32 MacroGenerator::GetNumGroupsInDataRamSetter
    (
        UINT32 numAlus
        ,UINT32 numDataRamEntries
    )
    {
        // One group to setup tracking the current data ram address. One data
        // ram entry set per group.
        return numDataRamEntries + 1;
    }

    //!
    //! \brief Return true if the given slot is a load that shouldn't be part of
    //! the selection pool, otherwise return false.
    //!
    //! NOTE: When loads are accepted by the generation configuration, all LOADn
    //! instructions are filetered out but LOAD0. Relying on the LoadInstrPicker
    //! to use an appropriate substitute for LOAD0 during generation.
    //!
    //! \tparam T Group slot type.
    //! \param slot Group slot value (ex. Method, Emit, Src)
    //! \return True if the given value should be removed, false otherwise.
    //!
    template <class T>
    bool MacroGenerator::FilterLoad(const T& slot) const
    {
        return IsLoad(slot)
            && (m_pGenConfig->noLoad  // Ignore loads if config says so
                || slot != T::Load0); // Only add LOAD0 of the LOADn instr
    }

    //!
    //! \brief Return true if the given slot is a register that shouldn't be
    //! part of the selection pool, otherwise return false.
    //!
    //! \tparam T Group slot type.
    //! \param slot Group slot value (ex. Dst, Src)
    //! \return True if the given value should be removes, false otherwise.
    //!
    template <class T>
    bool MacroGenerator::FilterRegs(const T& slot) const
    {
        // NOTE: Assumes registers have sequential encodings

        // If it is not a register, don't filter it out
        if (!IsRegister(slot))
        {
            return false;
        }

        const UINT32 maxNumRegs = m_pGenConfig->maxNumRegisters;
        if (maxNumRegs != Config::MAX_NUM_REGISTERS_ALL)
        {
            // Gen configuration is using a limited set of registers

            const UINT32 regEncoding = static_cast<UINT32>(slot);
            // If slot is a register, it should be within valid encoding range
            MASSERT(regEncoding >= s_LowestRegEncoding
                    && regEncoding <= s_HighestRegEncoding);

            const UINT32 maxRegEncoding = s_LowestRegEncoding + maxNumRegs - 1;
            MASSERT(maxRegEncoding <= s_HighestRegEncoding);

            // Filter if reg is past the selected register range.
            return regEncoding > maxRegEncoding;
        }

        return true;
    }
}
