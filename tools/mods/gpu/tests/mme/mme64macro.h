/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/bitfield.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include <map>

namespace Mme64
{
    typedef UINT08 AluNum;
    typedef UINT08 OutNum;

    // TODO(stewarts): Temporary until the test is working for Groups
    // with two ALUs. Design under the assumption that Groups are
    // arbitrary sizes and that these defines do not exist.
    // Alu:Output is 1:1
    constexpr UINT32 s_NumGroupAlus = 2;
    constexpr UINT32 s_NumGroupOuts = s_NumGroupAlus;

    namespace Desc
    {
        //!
        //! \brief Instruction field description.
        //!
        struct FieldDesc
        {
            UINT32 offset;          //!< Bit offset from start of section
            UINT32 size;            //!< Size of the field in bits
        };

        // Global section description
        namespace Global
        {
            constexpr UINT32    SectionSize = 10;       //!< Section size in bits
            constexpr FieldDesc EndNext     = {0, 1};
            constexpr FieldDesc PredMode    = {1, 4};
            constexpr FieldDesc PredReg     = {5, 5};
        }

        // Alu section description
        namespace Alu
        {
            constexpr UINT32    SectionSize  = 36;      //!< Section size in bits

            constexpr FieldDesc Op           = {0,  5};
            constexpr FieldDesc Dst          = {5,  5};
            constexpr FieldDesc Src0         = {10, 5};
            constexpr FieldDesc Src1         = {15, 5};

            constexpr FieldDesc Immed        = {20, 16};

            constexpr FieldDesc Subop        = {20, 3};

            constexpr FieldDesc SrcBit       = {20, 5};
            constexpr FieldDesc Width        = {25, 5};
            constexpr FieldDesc DstBit       = {30, 5};

            constexpr FieldDesc Offset       = {20, 13};

            constexpr FieldDesc DelaySlot    = {33, 1};
            constexpr FieldDesc AddSrc0      = {34, 1};
            constexpr FieldDesc AddPc        = {35, 1};

            constexpr FieldDesc PredictTaken = {34, 1};
            constexpr FieldDesc BranchTrue   = {35, 1};
        };

        // Output section description
        namespace Output
        {
            constexpr UINT32    SectionSize = 7;        //!< Section size in bits

            constexpr FieldDesc Method      = {0, 3};
            constexpr FieldDesc Emit        = {3, 4};
        };
    };

    //!
    //! \brief ALU op codes.
    //!
    //! ALU operation code values (ie. alu[n].op).
    //!
    //! Values taken from the 'ALU Opcodes' table from the ISA document.
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    enum class Op : UINT32
    {
        /*** ALU ***/
        // Arithmetic
        Add       = 0x00    //!< Add
        ,Addc     = 0x01    //!< Add with carry
        ,Sub      = 0x02    //!< Subtract
        ,Subb     = 0x03    //!< Subtract with Borrow
        ,Mul      = 0x04    //!< Multiply, save Low word (Signed)
        ,Mulh     = 0x05    //!< Multiply, save High word
        ,Mulu     = 0x06    //!< Multiply, save Low word (Unsiged)
        ,Clz      = 0x08    //!< Count Leading Zeros
        ,Sll      = 0x09    //!< Shift Left Logical
        ,Srl      = 0x0A    //!< Shift Right Logical
        ,Sra      = 0x0B    //!< Shift Right Arithmetic
        // System
        ,Extended = 0x07    //!< Extended opcode
        // Logic
        ,And      = 0x0C    //!< Bitwise And
        ,Nand     = 0x0D    //!< Bitwise Nand
        ,Or       = 0x0E    //!< Bitwise Or
        ,Xor      = 0x0F    //!< Bitwise Exclusive Or
        ,Merge    = 0x10    //!< Merge field
        // Comparison
        ,Slt      = 0x11    //!< Set if Less Than (Signed)
        ,Sltu     = 0x12    //!< Set if Less Than (Unsigned)
        ,Sle      = 0x13    //!< Set if Less Than or Equal (Signed)
        ,Sleu     = 0x14    //!< Set if Less Than or Equal (Unsigned)
        ,Seq      = 0x15    //!< Set if Equal
        // State (Shadow Ram I/O)
        ,State    = 0x16    //!< State read from Shadow ram
        // Flow control
        ,Loop     = 0x17    //!< Loop Initialization
        ,Jal      = 0x18    //!< Jump and Link
        ,Blt      = 0x19    //!< Branch Less Than (Signed)
        ,Bltu     = 0x1A    //!< Branch Less Than (Unsigned)
        ,Ble      = 0x1B    //!< Branch Less Than or Equal (Signed)
        ,Bleu     = 0x1C    //!< Branch Less Than or Equal (Unsigned)
        ,Beq      = 0x1D    //!< Branch Equal
        // I/O (Data Ram or Frame buffer via DMA I/O)
        ,Dread    = 0x1E    //!< Data Ram Read
        ,Dwrite   = 0x1F    //!< Data Ram Write
    };

    //!
    //! \brief Possible predicate mode values for each part of the predicate
    //! mode.
    //!
    //! \see Mme64::PredMode
    //!
    enum class PredVal : UINT32
    {
        U   //!< Unconditional - ignores predicate conditional
        ,T  //!< True - predicate conditional is true
        ,F  //!< False - predicate conditional is false
    };

    //!
    //! \brief Predicate mode values.
    //!
    //! Predicate mode field values (ie. global.predMode).
    //!
    //! Predicate mode defines a predicate conditon based on
    //! 'predReg == 0' for each ALU and Output.
    //! The values are specified four at a time corresponding to the
    //! following controls: [ALU0][ALU1][OUT0][OUT1]
    //!
    //! Example: TFUU  => ALU0=T, ALU1=F, OUT0=U, OUT1=U
    //!
    //! The ALU values control the ALU write outputs. The OUT values
    //! control the current method update and the final emit of the
    //! output pipes.
    //!
    //! Values taken from the 'Predicate mode enum' table from the ISA document.
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    //! \see Mme64::PredVal
    //! \see ::s_PredModeName
    //! \see ::s_PredModeEncoding
    //!
    enum class PredMode : UINT32
    {
        UUUU  = 0x0
        ,TTTT = 0x1
        ,FFFF = 0x2
        ,TTUU = 0x3
        ,FFUU = 0x4
        ,TFUU = 0x5
        ,TUUU = 0x6
        ,FUUU = 0x7
        ,UUTT = 0x8
        ,UUTF = 0x9
        ,UUTU = 0xA
        ,UUFT = 0xB
        ,UUFF = 0xC
        ,UUFU = 0xD
        ,UUUT = 0xE
        ,UUUF = 0xF
    };

    //!
    //! \brief ALU source/destination field values.
    //!
    //! ALU source/destination field values (ie. alu[n].dst, alu[n].src[m]).
    //!
    //! Values taken from the 'ALU Source/Dest Fields' table from the ISA document.
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    enum class SrcDst : UINT32
    {
        R0         = 0x00 //!< Register
        ,R1        = 0x01 //!< Register
        ,R2        = 0x02 //!< Register
        ,R3        = 0x03 //!< Register
        ,R4        = 0x04 //!< Register
        ,R5        = 0x05 //!< Register
        ,R6        = 0x06 //!< Register
        ,R7        = 0x07 //!< Register
        ,R8        = 0x08 //!< Register
        ,R9        = 0x09 //!< Register
        ,R10       = 0x0A //!< Register
        ,R11       = 0x0B //!< Register
        ,R12       = 0x0C //!< Register
        ,R13       = 0x0D //!< Register
        ,R14       = 0x0E //!< Register
        ,R15       = 0x0F //!< Register
        ,R16       = 0x10 //!< Register
        ,R17       = 0x11 //!< Register
        ,R18       = 0x12 //!< Register
        ,R19       = 0x13 //!< Register
        ,R20       = 0x14 //!< Register
        ,R21       = 0x15 //!< Register
        ,R22       = 0x16 //!< Register
        ,R23       = 0x17 //!< Register
        ,Zero      = 0x18 //!< Zero value
        ,Immed     = 0x19 //!< Immediate from alu, sign extended to form 32-bit value
        ,ImmedPair = 0x1A //!< Immediate from alu^1 (read alu# xor 1), sign extended to
                          //!< form 32-bit value
        ,Immed32   = 0x1B //!< 32-bit immediate formed from the immediate from ALU0 in
                          //!< the high bits and ALU1 in the low bits (in scalable form,
                          //!< ALU 2*floor(alu/2) in the high bits and ALU 2*floor(alu/2)+1
                          //!< in the low bits)
        ,Load0     = 0x1C //!< Load top instruction from the input fifo
        ,Load1     = 0x1D //!< Load second instruction instruction from the input fifo. Must
                          //!< be used in conjuction with Load0
    };

    // NOTE(stewarts): Update as more registers are added.
    static constexpr UINT32 s_LowestRegEncoding = static_cast<UINT32>(SrcDst::R0);
    static constexpr UINT32 s_HighestRegEncoding = static_cast<UINT32>(SrcDst::R23);
    static constexpr UINT32 s_NumRegisters = s_HighestRegEncoding - s_LowestRegEncoding + 1;

    //!
    //! \brief Output field values.
    //!
    //! Output field values for method (output[n].method) and emit (output[n].emit) fields.
    //!
    //! Values taken from the 'Output fields' table from the ISA document.
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    enum class OutputField : UINT32
    {
        None        = 0x0 //!< Field specific
        ,Alu0       = 0x1 //!< Output of ALU0
        ,Alu1       = 0x2 //!< Output of ALU1
        ,Load0      = 0x3 //!< Value of first load
        ,Load1      = 0x4 //!< Value of second load
        ,Immed0     = 0x5 //!< 16-bit immediate from ALU0 zero extended to 32-bit
        ,Immed1     = 0x6 //!< 16-bit immediate from ALU0 zero extended to 32-bit
        ,ImmedHigh0 = 0x8 //!< Bits [15:12] of immediate from ALU0 zero extended to 32-bits
        ,ImmedHigh1 = 0x9 //!< Bits [15:12] of immediate from ALU0 zero extended to 32-bits
        ,Immed32    = 0xA //!< 32-bit immediate formed from the immediate from ALU0 in
                          //!< the high bits and ALU1 in the low bits (in scalable form,
                          //!< ALU 2*floor(alu/2) in the high bits and ALU 2*floor(alu/2)+1
                          //!< in the low bits)
    };

    //!
    //! \brief Special registers.
    //!
    //! Values used to reference special registers.
    //!
    //! Values taken from the 'Special registers' table from the ISA document.
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    //! \see LoadSource Valid values for LoadSource register.
    //!
    enum class SpecialReg : UINT32
    {
        //! Control for the source of LOAD sources (Data Ram or DMA).
        //!
        //! \see LoadSource Valid register values.
        LoadSource  = 0x0
        //! If not set, all methods are consumed after the shadow RAM (used for
        //! testing)
        //!
        //! \see MmeConfig Valid register values.
        ,MmeConfig  = 0x1
        ,PTimerHigh = 0x2  //!< Return the high 32-bits of PTIMER, writes ignored
        ,PTimerLow  = 0x3  //!< Return the low 32-bits of PTIMER, writes ignored
    };

    //!
    //! \brief Valid LoadSource special register values.
    //!
    enum class LoadSource : UINT32
    {
        MethodInput   = 0x0 //!< Load from incoming methods to FE (CallMme[].Data)
        ,DataReadFifo = 0x1 //!< Load from memory read requests issued to the data FIFO
    };

    //!
    //! \brief Valid MmeConfig special register values.
    //!
    enum class MmeConfig : UINT32
    {
        // Bit 0 is Method Emit Disable with possible values:
        MethodEmitDisable_Off = 0x0 //!< Do not consume methods
        ,MethodEmitDisable_On = 0x1 //!< Consume methods after the shadow RAM

        // Bits [1:31] are undefined
    };

    //!
    //! \brief Extended op codes (subops).
    //!
    //! Op codes used to specify the sub operation when the EXTENDED op is used.
    //!
    //! Values taken from the 'Extended opcodes' table from the ISA document.
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    enum class Subop : UINT32
    {
        ReadFromSpecial = 0x0 //!< Read from a spcial register
        ,WriteToSpecial = 0x1 //!< Write ro a special register
    };

    //!
    //! \brief Valid source encodings.
    //!
    //! NOTE: This is a subset of SrcDst.
    //!
    enum class Src : UINT32
    {
        R0         = static_cast<UINT32>(SrcDst::R0)
        ,R1        = static_cast<UINT32>(SrcDst::R1)
        ,R2        = static_cast<UINT32>(SrcDst::R2)
        ,R3        = static_cast<UINT32>(SrcDst::R3)
        ,R4        = static_cast<UINT32>(SrcDst::R4)
        ,R5        = static_cast<UINT32>(SrcDst::R5)
        ,R6        = static_cast<UINT32>(SrcDst::R6)
        ,R7        = static_cast<UINT32>(SrcDst::R7)
        ,R8        = static_cast<UINT32>(SrcDst::R8)
        ,R9        = static_cast<UINT32>(SrcDst::R9)
        ,R10       = static_cast<UINT32>(SrcDst::R10)
        ,R11       = static_cast<UINT32>(SrcDst::R11)
        ,R12       = static_cast<UINT32>(SrcDst::R12)
        ,R13       = static_cast<UINT32>(SrcDst::R13)
        ,R14       = static_cast<UINT32>(SrcDst::R14)
        ,R15       = static_cast<UINT32>(SrcDst::R15)
        ,R16       = static_cast<UINT32>(SrcDst::R16)
        ,R17       = static_cast<UINT32>(SrcDst::R17)
        ,R18       = static_cast<UINT32>(SrcDst::R18)
        ,R19       = static_cast<UINT32>(SrcDst::R19)
        ,R20       = static_cast<UINT32>(SrcDst::R20)
        ,R21       = static_cast<UINT32>(SrcDst::R21)
        ,R22       = static_cast<UINT32>(SrcDst::R22)
        ,R23       = static_cast<UINT32>(SrcDst::R23)
        ,Zero      = static_cast<UINT32>(SrcDst::Zero)
        ,Immed     = static_cast<UINT32>(SrcDst::Immed)
        ,ImmedPair = static_cast<UINT32>(SrcDst::ImmedPair)
        ,Immed32   = static_cast<UINT32>(SrcDst::Immed32)
        ,Load0     = static_cast<UINT32>(SrcDst::Load0)
        ,Load1     = static_cast<UINT32>(SrcDst::Load1)
    };

    //!
    //! \brief Valid destination encodings.
    //!
    //! NOTE: This is a subset of SrcDst.
    //!
    enum class Dst : UINT32
    {
        R0    = static_cast<UINT32>(SrcDst::R0)
        ,R1   = static_cast<UINT32>(SrcDst::R1)
        ,R2   = static_cast<UINT32>(SrcDst::R2)
        ,R3   = static_cast<UINT32>(SrcDst::R3)
        ,R4   = static_cast<UINT32>(SrcDst::R4)
        ,R5   = static_cast<UINT32>(SrcDst::R5)
        ,R6   = static_cast<UINT32>(SrcDst::R6)
        ,R7   = static_cast<UINT32>(SrcDst::R7)
        ,R8   = static_cast<UINT32>(SrcDst::R8)
        ,R9   = static_cast<UINT32>(SrcDst::R9)
        ,R10  = static_cast<UINT32>(SrcDst::R10)
        ,R11  = static_cast<UINT32>(SrcDst::R11)
        ,R12  = static_cast<UINT32>(SrcDst::R12)
        ,R13  = static_cast<UINT32>(SrcDst::R13)
        ,R14  = static_cast<UINT32>(SrcDst::R14)
        ,R15  = static_cast<UINT32>(SrcDst::R15)
        ,R16  = static_cast<UINT32>(SrcDst::R16)
        ,R17  = static_cast<UINT32>(SrcDst::R17)
        ,R18  = static_cast<UINT32>(SrcDst::R18)
        ,R19  = static_cast<UINT32>(SrcDst::R19)
        ,R20  = static_cast<UINT32>(SrcDst::R20)
        ,R21  = static_cast<UINT32>(SrcDst::R21)
        ,R22  = static_cast<UINT32>(SrcDst::R22)
        ,R23  = static_cast<UINT32>(SrcDst::R23)
        ,Zero = static_cast<UINT32>(SrcDst::Zero)
    };

    //!
    //! \brief Valid predicate register encodings.
    //!
    //! NOTE: This is a subset of SrcDst.
    //!
    enum class PredReg : UINT32
    {
        R0    = static_cast<UINT32>(SrcDst::R0)
        ,R1   = static_cast<UINT32>(SrcDst::R1)
        ,R2   = static_cast<UINT32>(SrcDst::R2)
        ,R3   = static_cast<UINT32>(SrcDst::R3)
        ,R4   = static_cast<UINT32>(SrcDst::R4)
        ,R5   = static_cast<UINT32>(SrcDst::R5)
        ,R6   = static_cast<UINT32>(SrcDst::R6)
        ,R7   = static_cast<UINT32>(SrcDst::R7)
        ,R8   = static_cast<UINT32>(SrcDst::R8)
        ,R9   = static_cast<UINT32>(SrcDst::R9)
        ,R10  = static_cast<UINT32>(SrcDst::R10)
        ,R11  = static_cast<UINT32>(SrcDst::R11)
        ,R12  = static_cast<UINT32>(SrcDst::R12)
        ,R13  = static_cast<UINT32>(SrcDst::R13)
        ,R14  = static_cast<UINT32>(SrcDst::R14)
        ,R15  = static_cast<UINT32>(SrcDst::R15)
        ,R16  = static_cast<UINT32>(SrcDst::R16)
        ,R17  = static_cast<UINT32>(SrcDst::R17)
        ,R18  = static_cast<UINT32>(SrcDst::R18)
        ,R19  = static_cast<UINT32>(SrcDst::R19)
        ,R20  = static_cast<UINT32>(SrcDst::R20)
        ,R21  = static_cast<UINT32>(SrcDst::R21)
        ,R22  = static_cast<UINT32>(SrcDst::R22)
        ,R23  = static_cast<UINT32>(SrcDst::R23)
        ,Zero = static_cast<UINT32>(SrcDst::Zero)
    };

    //!
    //! \brief Valid method encodings.
    //!
    //! These method sources set the method register as follows:
    //! - Bits [11: 0] are the method value
    //! - Bits [15:12] are the method register increment value
    //!
    //! NOTE: This is a subset of OutputField.
    //!
    //! \see OutputField
    //!
    enum class Method : UINT32
    {
        None    = static_cast<UINT32>(OutputField::None)    //!< Make no change to the current method
        ,Alu0   = static_cast<UINT32>(OutputField::Alu0)
        ,Alu1   = static_cast<UINT32>(OutputField::Alu1)
        ,Load0  = static_cast<UINT32>(OutputField::Load0)
        ,Load1  = static_cast<UINT32>(OutputField::Load1)
        ,Immed0 = static_cast<UINT32>(OutputField::Immed0)
        ,Immed1 = static_cast<UINT32>(OutputField::Immed1)
    };

    //!
    //! \brief Breakdown of the value sent to a method slot.
    //!
    //! Data sent to a method slot 16 bits.
    //!
    //! The data from the source defined in the Output.Method field are
    //! interpretted as a method addr part and a method increment part.
    //!
    struct MethodSlot
    {
        UINT16 bits;

        //!
        //! \brief Constructor from method slot value.
        //!
        MethodSlot(UINT16 bits) : bits(bits) {}

        //!
        //! \brief Constructor from each field's value.
        //!
        //! \param methodAddr Method address portion of the method slot (bits
        //! [11:0]).
        //! \param increment Method increment for the method slot (bits
        //! [15:12]).
        //!
        MethodSlot(UINT16 methodAddr, UINT16 methodInc)
        {
            // data is max 12 bits [11:0]
            MASSERT(!(methodAddr & ~static_cast<UINT16>(0x0fff)));
            // increment is max 4 bits [15:12]
            MASSERT(!(methodInc & ~static_cast<UINT16>(0x000f)));

            bits = (methodInc << 12) | methodAddr;
        }

        UINT16 GetMethodAddr() const { return bits & 0xfff; }
        UINT16 GetMethodInc() const  { return bits >> 12; }
        UINT16 GetValue() const      { return bits; }
    };

    //!
    //! \brief Valid emit encodings.
    //!
    //! NOTE: This is a subset of OutputField.
    //!
    //! If emit is not none, then it triggers a method to be sent down the
    //! pipeline with 32-bits of data corresponding to the emit encoding.
    //!
    //! \see OutputField
    //!
    enum class Emit : UINT32
    {
        None        = static_cast<UINT32>(OutputField::None)       //!< No emission will occur.
        ,Alu0       = static_cast<UINT32>(OutputField::Alu0)
        ,Alu1       = static_cast<UINT32>(OutputField::Alu1)
        ,Load0      = static_cast<UINT32>(OutputField::Load0)
        ,Load1      = static_cast<UINT32>(OutputField::Load1)
        ,Immed0     = static_cast<UINT32>(OutputField::Immed0)
        ,Immed1     = static_cast<UINT32>(OutputField::Immed1)
        ,ImmedHigh0 = static_cast<UINT32>(OutputField::ImmedHigh0)
        ,ImmedHigh1 = static_cast<UINT32>(OutputField::ImmedHigh1)
        ,Immed32    = static_cast<UINT32>(OutputField::Immed32)
    };

    //!
    //! \brief Return the number of bits in an MME64 Group given the number of
    //! ALUs.
    //!
    static constexpr UINT32 GroupBitCount(AluNum aluCount)
    {
        return Desc::Global::SectionSize + (Desc::Alu::SectionSize + Desc::Output::SectionSize)
            * static_cast<UINT32>(aluCount);
    }

    //!
    //! \brief Return given ALU's paired ALU.
    //!
    //! Definition of pair is found in the MME64 ISA document.
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    static constexpr AluNum GetAluPair(AluNum aluNum)
    {
        return aluNum ^ 1;
    }

    //!
    //! \brief A single MME64 group.
    //!
    //! MME64 groups are Very Long Instruction Words (VLIW), and consist of
    //! three sections: a Global part, n ALU parts (designed to be expandable to
    //! 4), and n Output parts (one-to-one with ALU parts). Each group is
    //! represented as a bitfield. A single MME64 group is exelwted per clock.
    //!
    template <AluNum AluCount>
    class Group : public Bitfield<UINT32, GroupBitCount(AluCount)>
    {
        static_assert(AluCount > 0, "AluCount must be greater than 0");

    public:
        enum
        {
            NUM_EMISSIONS_UNKNOWN = _UINT32_MAX
        };

        Group()
            : Bitfield<UINT32, GroupBitCount(AluCount)>()
            , m_AluCount(AluCount)
            , m_NumEmissions(NUM_EMISSIONS_UNKNOWN)
        {}

        string DisassembleGlobal() const;
        string DisassembleAlu(AluNum aluNum) const;
        string DisassembleOutput(OutNum outNum) const;
        string Disassemble() const;

        // NOTE: Inline to WAR erroneous MSVC unreferenced function warning with
        // virtual templates.
        //   Ref: https://stackoverflow.com/questions/3051992/compiler-warning-at-c-template-base-class?utm_medium=organic&utm_source=google_rich_qa&utm_campaign=google_rich_qa
        inline void Reset() override;

        /* Set section value. */
        void SetGlobal(bool endNext, PredMode predMode, PredReg predReg);
        void InstrAdd(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrAddc(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrSub(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrSubb(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrMul(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrMulh(AluNum alu, Dst dst);
        void InstrMulu(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrClz(AluNum alu, Dst dst, Src src0);
        void InstrSll(AluNum alu, Dst dst, Src src0, Src shift);
        void InstrSrl(AluNum alu, Dst dst, Src src0, Src shift);
        void InstrSra(AluNum alu, Dst dst, Src src0, Src shift);
        void InstrExtendedReadFromSpecial
        (
            AluNum alu
            ,Dst dst
            ,SpecialReg specialReg
        );
        void InstrExtendedWriteToLoadSource(AluNum alu, LoadSource val);
        void InstrExtendedWriteToMmeConfig(AluNum alu, MmeConfig val);
        void InstrAnd(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrNand(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrOr(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrXor(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrMerge
        (
            AluNum alu
            ,Dst dst
            ,Src src0
            ,Src src1
            ,UINT08 srcBit
            ,UINT08 width
            ,UINT08 dstBit
        );
        void InstrSlt(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrSltu(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrSle(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrSleu(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrSeq(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrState(AluNum alu, Dst dst, Src src0, Src src1);
        void InstrLoop(AluNum alu, Src loopCount, UINT16 offset);
        void InstrJal
        (
            AluNum alu
            ,Dst dst
            ,Src src0
            ,bool appPc
            ,bool addSrc0
            ,bool delaySlot
            ,UINT16 offset
        );
        void InstrBlt
        (
            AluNum alu
            ,Src src0
            ,Src src1
            ,bool branchTrue
            ,bool predictTaken
            ,bool delaySlot
            ,UINT16 offset
        );
        void InstrBltu
        (
            AluNum alu
            ,Src src0
            ,Src src1
            ,bool branchTrue
            ,bool predictTaken
            ,bool delaySlot
            ,UINT16 offset
        );
        void InstrBle
        (
            AluNum alu
            ,Src src0
            ,Src src1
            ,bool branchTrue
            ,bool predictTaken
            ,bool delaySlot
            ,UINT16 offset
        );
        void InstrBleu
        (
            AluNum alu
            ,Src src0
            ,Src src1
            ,bool branchTrue
            ,bool predictTaken
            ,bool delaySlot
            ,UINT16 offset
        );
        void InstrBeq
        (
            AluNum alu
            ,Src src0
            ,Src src1
            ,bool branchTrue
            ,bool predictTaken
            ,bool delaySlot
            ,UINT16 offset
        );
        void InstrDread(AluNum alu, Dst dst, Src index);
        void InstrDwrite(AluNum alu, Src index, Src data);
        void SetOutput(OutNum out, Method method, Emit emit);
        void Nop(AluNum alu);

        /*** Set individual field. ***/
        void SetGlobalEndNext(bool value);
        void SetGlobalPredMode(PredMode mode);
        void SetGlobalPredReg(PredReg reg);
        void SetAluImmed(AluNum alu, UINT16 immed);
        void SetAluOp(AluNum alu, Op operation);
        void SetAluDst(AluNum alu, Dst dst);
        void SetAluSrc0(AluNum alu, Src src);
        void SetAluSrc1(AluNum alu, Src src);
        void SetAluSubop(AluNum alu, Subop op);
        void SetAluSrcBit(AluNum alu, UINT08 srcBit);
        void SetAluWidth(AluNum alu, UINT08 width);
        void SetAluDstBit(AluNum alu, UINT08 dstBit);
        void SetAluAddPc(AluNum alu, bool value);
        void SetAluAddSrc0(AluNum alu, bool value);
        void SetAluDelaySlot(AluNum alu, bool value);
        void SetAluOffset(AluNum alu, UINT16 offset);
        void SetAluBranchTrue(AluNum alu, bool value);
        void SetAluPredictTaken(AluNum alu, bool value);
        void SetOutputMethod(OutNum out, Method method);
        void SetOutputEmit(OutNum out, Emit emit);
        void SetImmed(AluNum alu, UINT16 immed);
        void SetNumEmissions(UINT32 numEmissions) { m_NumEmissions = numEmissions; }

        /*** Get individual field. ***/
        bool GetGlobalEndNext() const;
        PredMode GetGlobalPredMode() const;
        PredReg GetGlobalPredReg() const;
        Op GetAluOp(AluNum alu) const;
        Dst GetAluDst(AluNum alu) const;
        Src GetAluSrc0(AluNum alu) const;
        Src GetAluSrc1(AluNum alu) const;
        Subop GetAluSubop(AluNum alu) const;
        UINT08 GetAluSrcBit(AluNum alu) const;
        UINT08 GetAluWidth(AluNum alu) const;
        UINT08 GetAluDstBit(AluNum alu) const;
        bool GetAluAddPc(AluNum alu) const;
        bool GetAluAddSrc0(AluNum alu) const;
        bool GetAluDelaySlot(AluNum alu) const;
        UINT16 GetAluOffset(AluNum alu) const;
        bool GetAluBranchTrue(AluNum alu) const;
        bool GetAluPredictTaken(AluNum alu) const;
        Method GetOutputMethod(OutNum out) const;
        Emit GetOutputEmit(OutNum out) const;
        UINT16 GetAluImmed(AluNum alu) const;

        AluNum GetAluCount() const { return m_AluCount; }
        UINT32 GetNumEmissions() const { return m_NumEmissions; }

    private:
        const AluNum m_AluCount;
        UINT32 m_NumEmissions; //!< Number of emissions produced by this group.
        //! Index i is true if ALU i's full op code is tied to it's immediate
        //! value. For example, op EXTENDED uses the immediate to select the
        //! suboperation.
        array<bool, AluCount> m_ImmedTiedToOp = {};

        //!
        //! \brief Return the specific Alu section's offset from the 0th bit of
        //! the Group.
        //!
        UINT32 GetAluGroupOffset(AluNum alu) const
        { return Desc::Global::SectionSize + Desc::Alu::SectionSize * alu; }

        //!
        //! \brief Return the specific Output section's offset from the 0th bit
        //! of the Group.
        //!
        UINT32 GetOutputGroupOffset(OutNum out) const
        { return Desc::Global::SectionSize + Desc::Alu::SectionSize * m_AluCount
                + Desc::Output::SectionSize * out; }

        void InstrExtendedWriteToSpecialReg
        (
            AluNum alu
            ,UINT16 specialReg
            ,UINT16 val
        );
    };

    //!
    //! \brief MME64 macro program.
    //!
    struct Macro
    {
        using Groups = vector<Group<s_NumGroupAlus>>;
        using StartAddrPtr = UINT32;    //!< Pointer into the mme_start_addr table
        enum
        {
            //! Not loaded in the mme_start_addr table (ie. MME64 macro table)
            NOT_LOADED = _UINT32_MAX,
            //! Number of emissions is unknown
            NUM_EMISSIONS_UNKNOWN = _UINT32_MAX,
            //! Cycle count is unknown
            CYCLE_COUNT_UNKNOWN = _UINT32_MAX
        };

        //! Groups that make up the macro.
        Groups groups;
        //! MME input data for this macro. // TODO(stewarts): better type
        vector<UINT32> inputData;
        //! Number of emissions expected from running the macro.
        UINT32 numEmissions        = NUM_EMISSIONS_UNKNOWN;
        //! Number of cycles to complete the macro.
        UINT32 cycleCount          = CYCLE_COUNT_UNKNOWN;
        //! MME64 table entry where this macro is loaded.
        StartAddrPtr mmeTableEntry = NOT_LOADED;

        void Reset();
        void Print(Tee::Priority printLevel) const;
        bool IsLoaded() const;
    };

    class I2mEmission;

    //!
    //! \brief A generic MME emission.
    //!
    class Emission
    {
    public:
        Emission() {};
        Emission(UINT16 method, UINT32 data);
        Emission(const Emission& other) = default;
        Emission(Emission&& other) = default;

        UINT16 GetMethod() const;
        UINT16 GetMethodAddr() const;
        UINT32 GetData() const { return data; }

        string ToString() const;

        bool operator==(const Emission& other);
        bool operator==(const I2mEmission& other);
        bool operator!=(const Emission& other);
        bool operator!=(const I2mEmission& other);
        Emission& operator=(const Emission&) = default;
        Emission& operator=(Emission&&) = default;

    private:
        //! Method, bits [11:0].
        //!
        //! The method and method address refer to the same thing. It is called
        //! a method in the context of the pipeline, and a method address when
        //! the value is sourced from the method register. The bit positioning
        //! and the value is the same in either case. A method register is made
        //! up of a method address in bits [11:0] and a method increment in bits
        //! [15:12].
        UINT16 method;
        UINT32 data;    //!< Data, 32-bits
    };

    //!
    //! \brief Data emitted from the MME64 when MME_TO_M2M_INLINE mode is
    //! enabled.
    //!
    //! Format:
    //! 8B      | 4B      | Description
    //! --------|---------|---------------------------------
    //! 31:0    | 31:0    | MME method 0 data               | 32-bits
    //! ----------------------------------------------------
    //! 43:32   | 11:0    | MME method 0 method address     | 32-bits
    //! 46:44   | 14:12   | Subchannel                      |
    //! 47      | 15      | 0                               |
    //! 59:48   | 27:16   | MME program counter (PC)        |
    //! 62:60   | 30:28   | 0                               |
    //! 63      | 31      | MME method 0 valid              |
    //!
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/IAS/FE/Hardware MicroArch/TU10x_FE_MME64_IAS.docx //$
    //!
    class I2mEmission
    {
    public:
        static constexpr UINT32 s_SizeInDwords = 2; //!< Size in DWORDs of output emitted from MME64
        I2mEmission() : lower(0), upper(0) {}
        I2mEmission(UINT32 low, UINT32 up) : lower(low), upper(up) {}
        I2mEmission(const I2mEmission& other) = default;
        I2mEmission(I2mEmission&& other) = default;

        UINT32 GetUpper() const { return upper; }
        UINT32 GetLower() const { return lower; }

        UINT16 GetMethod() const;
        UINT16 GetMethodAddr() const;
        UINT32 GetData() const       { return lower; }
        UINT32 GetSubchannel() const { return (upper >> 12) & 0x7; }
        //!
        //! \brief MME program counter (PC) at time of emission.
        //!
        UINT32 GetPC() const { return (upper >> 16) & 0xfff; }
        bool IsValid() const { return (upper >> 31) == 0x1; }

        string ToString() const;
        Emission ToEmission() const;

        bool operator==(const I2mEmission& other);
        bool operator==(const Emission& other);
        bool operator!=(const I2mEmission& other);
        bool operator!=(const Emission& other);
        I2mEmission& operator=(const I2mEmission&) = default;
        I2mEmission& operator=(I2mEmission&&) = default;

    private:
        UINT32 lower; // bits [31:0]
        UINT32 upper; // bits [63:32]
    };

    //--------------------------------------------------------------------------

    namespace
    {
        // Map encoding->name for each Group field value type.
        //
        static map<Op, string> s_OpName =
        {
            {Op::Add, "ADD"}
            ,{Op::Addc, "ADDC"}
            ,{Op::Sub, "SUB"}
            ,{Op::Subb, "SUBB"}
            ,{Op::Mul, "MUL"}
            ,{Op::Mulh, "MULH"}
            ,{Op::Mulu, "MULU"}
            ,{Op::Clz, "CLZ"}
            ,{Op::Sll, "SLL"}
            ,{Op::Srl, "SRL"}
            ,{Op::Sra, "SRA"}
            ,{Op::Extended, "EXTENDED"}
            ,{Op::And, "AND"}
            ,{Op::Nand, "NAND"}
            ,{Op::Or, "OR"}
            ,{Op::Xor, "XOR"}
            ,{Op::Merge, "MERGE"}
            ,{Op::Slt, "SLT"}
            ,{Op::Sltu, "SLTU"}
            ,{Op::Sle, "SLE"}
            ,{Op::Sleu, "SLEU"}
            ,{Op::Seq, "SEQ"}
            ,{Op::State, "STATE"}
            ,{Op::Loop, "LOOP"}
            ,{Op::Jal, "JAL"}
            ,{Op::Blt, "BLT"}
            ,{Op::Bltu, "BLTU"}
            ,{Op::Ble, "BLE"}
            ,{Op::Bleu, "BLEU"}
            ,{Op::Beq, "BEQ"}
            ,{Op::Dread, "DREAD"}
            ,{Op::Dwrite, "DWRITE"}
        };

        // NOTE: Could combine s_PredValName & s_PredValEncoding with
        // boost::bimap (not available).
        static map<PredVal, char> s_PredValName =
        {
            {PredVal::U, 'U'}
            ,{PredVal::T, 'T'}
            ,{PredVal::F, 'F'}
        };

        // NOTE: Could combine s_PredValName & s_PredValEncoding with
        // boost::bimap (not available).
        static map<char, PredVal> s_PredValEncoding =
        {
            {'U', PredVal::U}
            ,{'T', PredVal::T}
            ,{'F', PredVal::F}
        };

        // NOTE: Could combine s_PredModeName & s_PredModeEncoding with
        // boost::bimap (not available).
        static map<PredMode, string> s_PredModeName =
        {
            {PredMode::UUUU, "UUUU"}
            ,{PredMode::TTTT, "TTTT"}
            ,{PredMode::FFFF, "FFFF"}
            ,{PredMode::TTUU, "TTUU"}
            ,{PredMode::FFUU, "FFUU"}
            ,{PredMode::TFUU, "TFUU"}
            ,{PredMode::TUUU, "TUUU"}
            ,{PredMode::FUUU, "FUUU"}
            ,{PredMode::UUTT, "UUTT"}
            ,{PredMode::UUTF, "UUTF"}
            ,{PredMode::UUTU, "UUTU"}
            ,{PredMode::UUFT, "UUFT"}
            ,{PredMode::UUFF, "UUFF"}
            ,{PredMode::UUFU, "UUFU"}
            ,{PredMode::UUUT, "UUUT"}
            ,{PredMode::UUUF, "UUUF"}
        };

        // NOTE: Could combine s_PredModeName & s_PredModeEncoding with
        // boost::bimap (not available).
        static map<string, PredMode> s_PredModeEncoding =
        {
            {"UUUU", PredMode::UUUU}
            ,{"TTTT", PredMode::TTTT}
            ,{"FFFF", PredMode::FFFF}
            ,{"TTUU", PredMode::TTUU}
            ,{"FFUU", PredMode::FFUU}
            ,{"TFUU", PredMode::TFUU}
            ,{"TUUU", PredMode::TUUU}
            ,{"FUUU", PredMode::FUUU}
            ,{"UUTT", PredMode::UUTT}
            ,{"UUTF", PredMode::UUTF}
            ,{"UUTU", PredMode::UUTU}
            ,{"UUFT", PredMode::UUFT}
            ,{"UUFF", PredMode::UUFF}
            ,{"UUFU", PredMode::UUFU}
            ,{"UUUT", PredMode::UUUT}
            ,{"UUUF", PredMode::UUUF}
        };

        //! Superset of registers, sources, and destinations
        static map<SrcDst, string> s_SrcDstName =
        {
            {SrcDst::R0, "R0"}
            ,{SrcDst::R1, "R1"}
            ,{SrcDst::R2, "R2"}
            ,{SrcDst::R3, "R3"}
            ,{SrcDst::R4, "R4"}
            ,{SrcDst::R5, "R5"}
            ,{SrcDst::R6, "R6"}
            ,{SrcDst::R7, "R7"}
            ,{SrcDst::R8, "R8"}
            ,{SrcDst::R9, "R9"}
            ,{SrcDst::R10, "R10"}
            ,{SrcDst::R11, "R11"}
            ,{SrcDst::R12, "R12"}
            ,{SrcDst::R13, "R13"}
            ,{SrcDst::R14, "R14"}
            ,{SrcDst::R15, "R15"}
            ,{SrcDst::R16, "R16"}
            ,{SrcDst::R17, "R17"}
            ,{SrcDst::R18, "R18"}
            ,{SrcDst::R19, "R19"}
            ,{SrcDst::R20, "R20"}
            ,{SrcDst::R21, "R21"}
            ,{SrcDst::R22, "R22"}
            ,{SrcDst::R23, "R23"}
            ,{SrcDst::Zero, "ZERO"}
            ,{SrcDst::Immed, "IMMED"}
            ,{SrcDst::ImmedPair, "IMMEDPAIR"}
            ,{SrcDst::Immed32, "IMMED32"}
            ,{SrcDst::Load0, "LOAD0"}
            ,{SrcDst::Load1, "LOAD1"}
        };

        static map<SpecialReg, string> s_SpecialRegName =
        {
            {SpecialReg::LoadSource, "LoadSource"}
            ,{SpecialReg::MmeConfig, "MmeConfig"}
            ,{SpecialReg::PTimerHigh, "PTimerHigh"}
            ,{SpecialReg::PTimerLow, "PTimerLow"}
        };

        static map<Subop, string> s_SubopName =
        {
            {Subop::ReadFromSpecial, "ReadFromSpecial"}
            ,{Subop::WriteToSpecial, "WriteToSpecial"}
        };

        static map<Emit, string> s_EmitName =
        {
            {Emit::None, "NONE"}
            ,{Emit::Alu0, "ALU0"}
            ,{Emit::Alu1, "ALU1"}
            ,{Emit::Load0, "LOAD0"}
            ,{Emit::Load1, "LOAD1"}
            ,{Emit::Immed0, "IMMED0"}
            ,{Emit::Immed1, "IMMED1"}
            ,{Emit::ImmedHigh0, "IMMEDHIGH0"}
            ,{Emit::ImmedHigh1, "IMMEDHIGH1"}
            ,{Emit::Immed32, "IMMED32"}
        };

        static map<Method, string> s_MethodName =
        {
            {Method::None, "NONE"}
            ,{Method::Alu0, "ALU0"}
            ,{Method::Alu1, "ALU1"}
            ,{Method::Load0, "LOAD0"}
            ,{Method::Load1, "LOAD1"}
            ,{Method::Immed0, "IMMED0"}
            ,{Method::Immed1, "IMMED1"}
        };
    };

    //--------------------------------------------------------------------------

    template <AluNum AluCount>
    string Group<AluCount>::DisassembleGlobal() const
    {
        string s;

        const bool endNext = GetGlobalEndNext();
        const PredMode predMode = GetGlobalPredMode();
        const string& predModeStr = s_PredModeName[predMode];

        if (endNext)
        {
            s += "ENDNEXT, ";
        }

        s += "?";
        s += predModeStr;
        if (predMode != PredMode::UUUU)
        {
            const string& predRegStr
                = s_SrcDstName[static_cast<SrcDst>(GetGlobalPredReg())];

            s += ", ";
            s += predRegStr;
        }

        return s;
    }

    template <AluNum AluCount>
    string Group<AluCount>::DisassembleAlu(AluNum alu) const
    {
        const Op      op       = GetAluOp(alu);
        const string& opName   = s_OpName[op];
        const Dst     dst      = GetAluDst(alu);
        const string& dstName  = s_SrcDstName[static_cast<SrcDst>(dst)];
        const Src     src0     = GetAluSrc0(alu);
        const string& src0Name = s_SrcDstName[static_cast<SrcDst>(src0)];
        const Src     src1     = GetAluSrc1(alu);
        const string& src1Name = s_SrcDstName[static_cast<SrcDst>(src1)];
        const UINT16  immed    = GetAluImmed(alu);

        // All ALU Ops consist of 'Op Dst, Src0, Src1, Immed', where some commands
        // can have varying representations of immed.
        string s = Utility::StrPrintf("%s %s, %s, %s",
                                      opName.c_str(), dstName.c_str(),
                                      src0Name.c_str(), src1Name.c_str());

        switch (op)
        {
        case Op::Add:
        case Op::Addc:
        case Op::Sub:
        case Op::Subb:
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
            s += Utility::StrPrintf(", 0x%04x", immed);
            break;

        case Op::Mulh:
            MASSERT(src0 == Src::Zero);
            MASSERT(src1 == Src::Zero);
            s += Utility::StrPrintf(", 0x%04x", immed);
            break;

        case Op::Clz:
            MASSERT(src1 == Src::Zero);
            s += Utility::StrPrintf(", 0x%04x", immed);
            break;

        case Op::Extended:
        {
            Subop subop = GetAluSubop(alu);

            switch (subop)
            {
            case Subop::ReadFromSpecial:
                MASSERT(src0 == Src::Zero);
                break;

            case Subop::WriteToSpecial:
                MASSERT(dst == Dst::Zero);
                break;

            default:
                MASSERT(!"Unknown subop code during disassembly");
                break;
            }

            s += Utility::StrPrintf(", 0x%04x", static_cast<UINT32>(subop));
        } break;

        case Op::Merge:
        {
            const UINT08 srcBit = GetAluSrcBit(alu);
            const UINT08 width = GetAluWidth(alu);
            const UINT08 dstBit = GetAluDstBit(alu);
            s += Utility::StrPrintf(", 0x%04x, 0x%04x, 0x%04x",
                                    srcBit, width, dstBit);
        } break;

        case Op::State:
            s += Utility::StrPrintf(", 0x%04x", immed);
            break;

        case Op::Loop:
        {
            MASSERT(src1 == Src::Zero);
            MASSERT(dst == Dst::Zero);
            UINT16 offset = GetAluOffset(alu);
            s += Utility::StrPrintf(", 0x%04x", offset);
        } break;

        case Op::Jal:
        {
            MASSERT(src1 == Src::Zero);
            const bool addPc = GetAluAddPc(alu);
            const bool addSrc0 = GetAluAddSrc0(alu);
            const bool delaySlot = GetAluDelaySlot(alu);
            const UINT16 offset = GetAluOffset(alu);

            if (addPc)
            {
                s += ", ADDPC";
            }

            if (addSrc0)
            {
                s += ", ADDSRC0";
            }

            if (delaySlot)
            {
                s += ", DS";
            }

            s += Utility::StrPrintf(", 0x%04x", offset);
        } break;

        case Op::Blt:
        case Op::Bltu:
        case Op::Ble:
        case Op::Bleu:
        case Op::Beq:
        {
            MASSERT(dst == Dst::Zero);
            const bool branchTrue = GetAluBranchTrue(alu);
            const bool predictTaken = GetAluPredictTaken(alu);
            const bool delaySlot = GetAluDelaySlot(alu);
            const UINT16 offset = GetAluOffset(alu);

            if (branchTrue)
            {
                s += ", T";
            }

            if (predictTaken)
            {
                s += ", PTAKEN";
            }

            if (delaySlot)
            {
                s += ", DS";
            }

            s += Utility::StrPrintf(", 0x%04x", offset);
        } break;

        case Op::Dread:
            MASSERT(src1 == Src::Zero);
            s += Utility::StrPrintf(", 0x%04x", immed);
            break;

        case Op::Dwrite:
            MASSERT(dst == Dst::Zero);
            s += Utility::StrPrintf(", 0x%04x", immed);
            break;

        default:
            MASSERT(!"Unknown op code during disassembly");
            break;
        }

        return s;
    }

    template <AluNum AluCount>
    string Group<AluCount>::DisassembleOutput(OutNum out) const
    {
        const string& method = s_MethodName[GetOutputMethod(out)];
        const string& emit = s_EmitName[GetOutputEmit(out)];

        return Utility::StrPrintf("METH %s, %s", method.c_str(), emit.c_str());
    }

    //!
    //! \brief Disassemble Group according to ISA text encoding
    //! specification.
    //!
    //! Ref: https://p4viewer.lwpu.com/getfile/hw/doc/gpu/turing/turing/design/Functional_Descriptions/Turing_MME64_ISA.docx //$
    //!
    template <AluNum AluCount>
    string Group<AluCount>::Disassemble() const
    {
        // TODO(stewarts): Align the columns for readability when dumping multiple
        // Groups.
        string s;

        s += DisassembleGlobal();

        for (AluNum i = 0; i < m_AluCount; i++)
        {
            s += " | " + DisassembleAlu(i);
        }

        for (AluNum i = 0; i < m_AluCount; i++)
        {
            s += " | " + DisassembleOutput(i);
        }

        return s;
    }

    //!
    //! \brief Reset the content of the group.
    //!
    template <AluNum AluCount>
    /*inline*/ void Group<AluCount>::Reset() /*override*/
    {
        Bitfield<UINT32, GroupBitCount(AluCount)>::Reset();
        m_ImmedTiedToOp.fill(false);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetGlobal
    (
        bool endNext
        ,PredMode predMode
        ,PredReg predReg
    )
    {
        SetGlobalEndNext(endNext);
        SetGlobalPredMode(predMode);
        SetGlobalPredReg(predReg);
    }

    //!
    //! \brief Set immediate for specific ALU.
    //!
    //! Can set immediate during instructions that don't require immediates
    //! and the immediates can be referenced by other ALUs for computations.
    //!
    template <AluNum AluCount>
    void Group<AluCount>::SetImmed(AluNum alu, UINT16 immed)
    {
        // Can't set the immediate if ALU operation already set it as part of
        // the op code.
        MASSERT(!m_ImmedTiedToOp[alu]);

        SetAluImmed(alu, immed);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrAdd(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Add);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrAddc(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Addc);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSub(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Sub);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSubb(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Subb);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrMul(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Mul);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrMulh(AluNum alu, Dst dst)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Mulh);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, Src::Zero);
        SetAluSrc1(alu, Src::Zero);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrMulu(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Mulu);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrClz(AluNum alu, Dst dst, Src src0)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Clz);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, Src::Zero);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSll(AluNum alu, Dst dst, Src src0, Src shift)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Sll);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, shift);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSrl(AluNum alu, Dst dst, Src src0, Src shift)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Srl);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, shift);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSra(AluNum alu, Dst dst, Src src0, Src shift)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Sra);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, shift);
    }

    //!
    //! \brief Write EXTENDED WriteToSpecial instruction to the given ALU.
    //!
    //! NOTE: Although source0 of the instruction can be any register, here it
    //! is forced to be the ALU's immediate. This is to make source0's value
    //! easily predicatable in software.
    //!
    template <AluNum AluCount>
    void Group<AluCount>::InstrExtendedReadFromSpecial
    (
        AluNum alu
        ,Dst dst
        ,SpecialReg specialReg
    )
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Extended);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, Src::Immed);
        SetAluSrc1(alu, Src::Zero);

        // Subop is set in the ALU's immediate
        const UINT16 aluImmed
            = static_cast<UINT16>(Subop::WriteToSpecial) << 12
            | static_cast<UINT16>(specialReg);

        SetImmed(alu, aluImmed);
        m_ImmedTiedToOp[alu] = true;
    }

    //!
    //! \brief Write EXTENDED WriteToSpecial instruction to the given ALU with
    //! special register set to LoadSource.
    //!
    //! NOTE: Although source0 of the instruction can be any register, here it
    //! is forced to be the ALU's immediate. This is to make source0's value
    //! easily predicatable in software.
    //! NOTE: Uses the ALU pair's immediate as the value to write.
    //!
    template <AluNum AluCount>
    void Group<AluCount>::InstrExtendedWriteToLoadSource
    (
        AluNum alu
        ,LoadSource val
    )
    {
        InstrExtendedWriteToSpecialReg(alu,
                                       static_cast<UINT16>(SpecialReg::MmeConfig),
                                       static_cast<UINT16>(val));

    }

    //!
    //! \brief Write EXTENDED WriteToSpecial instruction to the given ALU with
    //! special register set to LoadSource.
    //!
    //! NOTE: Although source0 of the instruction can be any register, here it
    //! is forced to be the ALU's immediate. This is to make source0's value
    //! easily predicatable in software.
    //! NOTE: Uses the ALU pair's immediate as the value to write.
    //!
    template <AluNum AluCount>
    void Group<AluCount>::InstrExtendedWriteToMmeConfig
    (
        AluNum alu
        ,MmeConfig val
    )
    {
        InstrExtendedWriteToSpecialReg(alu,
                                       static_cast<UINT16>(SpecialReg::MmeConfig),
                                       static_cast<UINT16>(val));
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrAnd(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::And);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrNand(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Nand);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrOr(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Or);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrXor(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Xor);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrMerge
    (
        AluNum alu
        ,Dst dst
        ,Src src0
        ,Src src1
        ,UINT08 srcBit
        ,UINT08 width
        ,UINT08 dstBit
    )
    {
        MASSERT(alu < m_AluCount);

        // These fields are only 5 bits wide, make sure we don't truncate.
        MASSERT(!(srcBit & 0xE0));
        MASSERT(!(width & 0xE0));
        MASSERT(!(dstBit & 0xE0));

        SetAluOp(alu, Op::Merge);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
        SetAluSrcBit(alu, srcBit);
        SetAluWidth(alu, width);
        SetAluDstBit(alu, dstBit);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSlt(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Slt);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSltu(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Sltu);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSle(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Sle);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSleu(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Sleu);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrSeq(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Seq);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrState(AluNum alu, Dst dst, Src src0, Src src1)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::State);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrLoop(AluNum alu, Src loopCount, UINT16 offset)
    {
        // TODO(stewarts): offset can be negative. Should we handle that through
        // bitpattern reinterpretation at the caller or callee site?
        MASSERT(alu < m_AluCount);
        MASSERT(!(offset & 0xE000)); // Offset field is only 13 bits wide

        SetAluOp(alu, Op::Loop);
        SetAluSrc0(alu, loopCount);
        SetAluOffset(alu, offset);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrJal
    (
        AluNum alu
        ,Dst dst
        ,Src src0
        ,bool addPc
        ,bool addSrc0
        ,bool delaySlot
        ,UINT16 offset
    )
    {
        MASSERT(alu < m_AluCount);
        MASSERT(!(offset & 0xE000)); // Offset field is only 13 bits wide

        SetAluOp(alu, Op::Jal);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, Src::Zero);
        SetAluAddPc(alu, addPc);
        SetAluAddSrc0(alu, addSrc0);
        SetAluDelaySlot(alu, delaySlot);
        SetAluOffset(alu, offset);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrBlt
    (
        AluNum alu
        ,Src src0
        ,Src src1
        ,bool branchTrue
        ,bool predictTaken
        ,bool delaySlot
        ,UINT16 offset
    )
    {
        MASSERT(alu < m_AluCount);
        MASSERT(!(offset & 0xE000)); // Offset field is only 13 bits wide

        SetAluOp(alu, Op::Blt);
        SetAluDst(alu, Dst::Zero);
        SetAluSrc0(alu, src0);
        SetAluSrc1(alu, src1);
        SetAluBranchTrue(alu, branchTrue);
        SetAluPredictTaken(alu, predictTaken);
        SetAluDelaySlot(alu, delaySlot);
        SetAluOffset(alu, offset);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrBltu
    (
        AluNum alu
        ,Src src0
        ,Src src1
        ,bool branchTrue
        ,bool predictTaken
        ,bool delaySlot
        ,UINT16 offset
    )
    {
        InstrBlt(alu, src0, src1, branchTrue, predictTaken, delaySlot, offset);
        SetAluOp(alu, Op::Bltu);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrBle
    (
        AluNum alu
        ,Src src0
        ,Src src1
        ,bool branchTrue
        ,bool predictTaken
        ,bool delaySlot
        ,UINT16 offset
    )
    {
        InstrBlt(alu, src0, src1, branchTrue, predictTaken, delaySlot, offset);
        SetAluOp(alu, Op::Ble);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrBleu
    (
        AluNum alu
        ,Src src0
        ,Src src1
        ,bool branchTrue
        ,bool predictTaken
        ,bool delaySlot
        ,UINT16 offset
    )
    {
        InstrBlt(alu, src0, src1, branchTrue, predictTaken, delaySlot, offset);
        SetAluOp(alu, Op::Bleu);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrBeq
    (
        AluNum alu
        ,Src src0
        ,Src src1
        ,bool branchTrue
        ,bool predictTaken
        ,bool delaySlot
        ,UINT16 offset
    )
    {
        InstrBlt(alu, src0, src1, branchTrue, predictTaken, delaySlot, offset);
        SetAluOp(alu, Op::Beq);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrDread(AluNum alu, Dst dst, Src index)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Dread);
        SetAluDst(alu, dst);
        SetAluSrc0(alu, index);
        SetAluSrc1(alu, Src::Zero);
    }

    template <AluNum AluCount>
    void Group<AluCount>::InstrDwrite(AluNum alu, Src index, Src data)
    {
        MASSERT(alu < m_AluCount);

        SetAluOp(alu, Op::Dwrite);
        SetAluDst(alu, Dst::Zero);
        SetAluSrc0(alu, index);
        SetAluSrc1(alu, data);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetOutput(OutNum out, Method method, Emit emit)
    {
        MASSERT(out < m_AluCount);

        SetOutputMethod(out, method);
        SetOutputEmit(out, emit);
    }

    template <AluNum AluCount>
    void Group<AluCount>::Nop(AluNum alu)
    {
        MASSERT(alu < m_AluCount);
        InstrAdd(alu, Dst::Zero, Src::Zero, Src::Zero);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetGlobalEndNext(bool value)
    {
        this->SetBit(Desc::Global::EndNext.offset, value);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetGlobalPredMode(PredMode mode)
    {
        this->SetBits(Desc::Global::PredMode.offset, Desc::Global::PredMode.size,
                      static_cast<UINT32>(mode));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetGlobalPredReg(PredReg reg)
    {
        this->SetBits(Desc::Global::PredReg.offset, Desc::Global::PredReg.size,
                      static_cast<UINT32>(reg));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluImmed(AluNum alu, UINT16 immed)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Immed.offset,
                      Desc::Alu::Immed.size, immed);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluOp(AluNum alu, Op operation)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Op.offset,
                      Desc::Alu::Op.size, static_cast<UINT32>(operation));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluDst(AluNum alu, Dst dst)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Dst.offset,
                      Desc::Alu::Dst.size, static_cast<UINT32>(dst));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluSrc0(AluNum alu, Src src)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Src0.offset,
                      Desc::Alu::Src0.size, static_cast<UINT32>(src));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluSrc1(AluNum alu, Src src)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Src1.offset,
                      Desc::Alu::Src1.size, static_cast<UINT32>(src));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluSubop(AluNum alu, Subop op)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Subop.offset,
                      Desc::Alu::Subop.size, static_cast<UINT32>(op));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluSrcBit(AluNum alu, UINT08 srcBit)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::SrcBit.offset,
                      Desc::Alu::SrcBit.size, srcBit);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluWidth(AluNum alu, UINT08 width)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Width.offset,
                      Desc::Alu::Width.size, width);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluDstBit(AluNum alu, UINT08 dstBit)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::DstBit.offset,
                      Desc::Alu::DstBit.size, dstBit);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluAddPc(AluNum alu, bool value)
    {
        this->SetBit(GetAluGroupOffset(alu) + Desc::Alu::AddPc.offset, value);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluAddSrc0(AluNum alu, bool value)
    {
        this->SetBit(GetAluGroupOffset(alu) + Desc::Alu::AddSrc0.offset, value);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluDelaySlot(AluNum alu, bool value)
    {
        this->SetBit(GetAluGroupOffset(alu) + Desc::Alu::DelaySlot.offset, value);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluOffset(AluNum alu, UINT16 offset)
    {
        this->SetBits(GetAluGroupOffset(alu) + Desc::Alu::Offset.offset,
                      Desc::Alu::Offset.size, offset);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluBranchTrue(AluNum alu, bool value)
    {
        this->SetBit(GetAluGroupOffset(alu) + Desc::Alu::BranchTrue.offset, value);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetAluPredictTaken(AluNum alu, bool value)
    {
        this->SetBit(GetAluGroupOffset(alu) + Desc::Alu::PredictTaken.offset, value);
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetOutputMethod(OutNum out, Method method)
    {
        this->SetBits(GetOutputGroupOffset(out) + Desc::Output::Method.offset,
                      Desc::Output::Method.size, static_cast<UINT32>(method));
    }

    template <AluNum AluCount>
    void Group<AluCount>::SetOutputEmit(OutNum out, Emit emit)
    {
        this->SetBits(GetOutputGroupOffset(out) + Desc::Output::Emit.offset,
                      Desc::Output::Emit.size, static_cast<UINT32>(emit));
    }

    template <AluNum AluCount>
    bool Group<AluCount>::GetGlobalEndNext() const
    {
        return this->GetBit(Desc::Global::EndNext.offset);
    }

    template <AluNum AluCount>
    Mme64::PredMode Group<AluCount>::GetGlobalPredMode() const
    {
        return static_cast<PredMode>(this->GetBits(Desc::Global::PredMode.offset,
                                                   Desc::Global::PredMode.size));
    }

    template <AluNum AluCount>
    PredReg Group<AluCount>::GetGlobalPredReg() const
    {
        return static_cast<PredReg>(this->GetBits(Desc::Global::PredReg.offset,
                                                  Desc::Global::PredReg.size));
    }

    template <AluNum AluCount>
    UINT16 Group<AluCount>::GetAluImmed(AluNum alu) const
    {
        const UINT32 size = Desc::Alu::Immed.size;
        MASSERT(size <= sizeof(UINT16) * CHAR_BIT); // Truncation check

        return static_cast<UINT16>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::Immed.offset, size));
    }

    template <AluNum AluCount>
    Op Group<AluCount>::GetAluOp(AluNum alu) const
    {
        return static_cast<Op>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::Op.offset,
                          Desc::Alu::Op.size));
    }

    template <AluNum AluCount>
    Dst Group<AluCount>::GetAluDst(AluNum alu) const
    {
        return static_cast<Dst>(this->GetBits(
                                    GetAluGroupOffset(alu) + Desc::Alu::Dst.offset,
                                    Desc::Alu::Dst.size));
    }

    template <AluNum AluCount>
    Src Group<AluCount>::GetAluSrc0(AluNum alu) const
    {
        return static_cast<Src>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::Src0.offset,
                          Desc::Alu::Src0.size));
    }

    template <AluNum AluCount>
    Src Group<AluCount>::GetAluSrc1(AluNum alu) const
    {
        return static_cast<Src>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::Src1.offset,
                          Desc::Alu::Src1.size));
    }

    template <AluNum AluCount>
    Subop Group<AluCount>::GetAluSubop(AluNum alu) const
    {
        return static_cast<Subop>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::Subop.offset,
                          Desc::Alu::Subop.size));
    }

    template <AluNum AluCount>
    UINT08 Group<AluCount>::GetAluSrcBit(AluNum alu) const
    {
        const UINT32 size = Desc::Alu::SrcBit.size;
        MASSERT(size <= (sizeof(UINT08) * CHAR_BIT)); // Truncation check

        return static_cast<UINT08>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::SrcBit.offset, size));
    }

    template <AluNum AluCount>
    UINT08 Group<AluCount>::GetAluWidth(AluNum alu) const
    {
        const UINT32 size = Desc::Alu::Width.size;
        MASSERT(size <= (sizeof(UINT08) * CHAR_BIT)); // Truncation check

        return static_cast<UINT08>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::Width.offset, size));
    }

    template <AluNum AluCount>
    UINT08 Group<AluCount>::GetAluDstBit(AluNum alu) const
    {
        const UINT32 size = Desc::Alu::DstBit.size;
        MASSERT(size <= (sizeof(UINT08) * CHAR_BIT)); // Truncation check

        return static_cast<UINT08>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::DstBit.offset, size));
    }

    template <AluNum AluCount>
    bool Group<AluCount>::GetAluAddPc(AluNum alu) const
    {
        return this->GetBit(GetAluGroupOffset(alu) + Desc::Alu::AddPc.offset);
    }

    template <AluNum AluCount>
    bool Group<AluCount>::GetAluAddSrc0(AluNum alu) const
    {
        return this->GetBit(GetAluGroupOffset(alu) + Desc::Alu::AddSrc0.offset);
    }

    template <AluNum AluCount>
    bool Group<AluCount>::GetAluDelaySlot(AluNum alu) const
    {
        return this->GetBit(GetAluGroupOffset(alu) + Desc::Alu::DelaySlot.offset);
    }

    template <AluNum AluCount>
    UINT16 Group<AluCount>::GetAluOffset(AluNum alu) const
    {
        const UINT32 size = Desc::Alu::Offset.size;
        MASSERT(size <= (sizeof(UINT16) * CHAR_BIT)); // Truncation check

        return static_cast<UINT16>(
            this->GetBits(GetAluGroupOffset(alu) + Desc::Alu::Offset.offset, size));
    }

    template <AluNum AluCount>
    bool Group<AluCount>::GetAluBranchTrue(AluNum alu) const
    {
        return this->GetBit(GetAluGroupOffset(alu) + Desc::Alu::BranchTrue.offset);
    }

    template <AluNum AluCount>
    bool Group<AluCount>::GetAluPredictTaken(AluNum alu) const
    {
        return this->GetBit(GetAluGroupOffset(alu) + Desc::Alu::PredictTaken.offset);
    }

    template <AluNum AluCount>
    Method Group<AluCount>::GetOutputMethod(OutNum out) const
    {
        return static_cast<Method>(
            this->GetBits(GetOutputGroupOffset(out) + Desc::Output::Method.offset,
                          Desc::Output::Method.size));
    }

    template <AluNum AluCount>
    Emit Group<AluCount>::GetOutputEmit(OutNum out) const
    {
        return static_cast<Emit>(
            this->GetBits(GetOutputGroupOffset(out) + Desc::Output::Emit.offset,
                          Desc::Output::Emit.size));
    }

    //!
    //! \brief Write EXTENDED WriteToSpecial instruction to the given ALU.
    //!
    //! NOTE: Although source0 of the instruction can be any register, here it
    //! is forced to be the ALU's immediate. This is to make source0's value
    //! easily predicatable in software.
    //! NOTE: Uses the ALU pair's immediate as the value to write.
    //!
    template <AluNum AluCount>
    void Group<AluCount>::InstrExtendedWriteToSpecialReg
    (
        AluNum alu
        ,UINT16 specialReg
        ,UINT16 val
    )
    {
        MASSERT(alu < m_AluCount);
        MASSERT(!(specialReg & ~0xfff)); // Special reg field is 12 bits

        // subop = Immed[15:12]
        // specialreg = Immed[11:0]
        // val = ImmedPair

        SetAluOp(alu, Op::Extended);
        SetAluDst(alu, Dst::Zero);
        SetAluSrc0(alu, Src::Immed);
        SetAluSrc1(alu, Src::ImmedPair);

        const AluNum aluPair = GetAluPair(alu);
        const UINT16 aluImmed
            = (static_cast<UINT16>(Subop::WriteToSpecial) << 12) | specialReg;

        SetImmed(alu, aluImmed);
        SetImmed(aluPair, val);
        m_ImmedTiedToOp[alu] = true;
        m_ImmedTiedToOp[aluPair] = true;
    }
};
