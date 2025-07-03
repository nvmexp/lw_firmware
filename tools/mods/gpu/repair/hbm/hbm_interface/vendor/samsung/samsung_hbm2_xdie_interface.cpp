/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_interface.h"

#include "core/include/utility.h"
#include "gpu/repair/repair_util.h"

using namespace Memory;
using namespace Memory::Hbm;
using namespace Repair;

using WirF = Wir::Flags;
using WirT = Wir::Type;
using RegT = Wir::RegType;

namespace
{
    //! HBM2 Samsung B-Die WIRs
    vector<Wir> s_SamsungHbm2XDieWirs =
    {
        { "HARD_REPAIR", WirT::HARD_REPAIR, 0x08, RegT::Write, 21,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::Samsung},
                {Die::X},
                {StackHeight::All},
                {Revision::A1, Revision::A2}),
          WirF::SingleChannelOnly
        },

        { "SOFT_REPAIR", WirT::SOFT_REPAIR, 0x08, RegT::Write, 21,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::Samsung},
                {Die::X},
                {StackHeight::All},
                {Revision::A1, Revision::A2}),
          WirF::SingleChannelOnly
        },

        { "TEST_MODE_REGISTER_SET", WirT::TEST_MODE_REGISTER_SET, 0xb0, RegT::Write, 37,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::Samsung},
                {Die::X},
                {StackHeight::All},
                {Revision::A1, Revision::A2})
        },

        { "ENABLE_FUSE_SCAN", WirT::ENABLE_FUSE_SCAN, 0xc0, RegT::Read | RegT::Write, 26,
          SupportedHbmModels(
                {SpecVersion::V2},
                {Vendor::Samsung},
                {Die::X},
                {StackHeight::All},
                {Revision::A1, Revision::A2})
        }
    };
}

RC SamsungHbm2XDie::Setup()
{
    RC rc;

    CHECK_RC(Hbm2Interface::Setup()); // Adds vendor inpedendent WIRs

    // Support checks
    //
    if (m_HbmModel.vendor != Vendor::Samsung)
    {
        Printf(Tee::PriError, "Attempt to setup Samsung HBM interface with a "
               "non-Samsung model:\n  %s\n",
               m_HbmModel.ToString().c_str());
        MASSERT(!"Attempt to setup Samsung interface with a non-Samsung "
                "HBM model");
        return RC::SOFTWARE_ERROR;
    }

    if (m_HbmModel.die != Die::X || !(m_HbmModel.revision == Revision::A1
                                      || m_HbmModel.revision == Revision::A2))
    {
        Printf(Tee::PriError, "Unsupported Samsung HBM model:\n  %s\n",
               m_HbmModel.ToString().c_str());
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(AddWirs(s_SamsungHbm2XDieWirs));

    return rc;
}

/******************************************************************************/
// RowRepairWriteData

SamsungHbm2XDie::RowRepairWriteData::RowRepairWriteData
(
    RepairType repairType,
    Stack stack,
    PseudoChannel hbmPseudoChan,
    Bank bank,
    UINT32 row
)
{
    using namespace Utility;

    MASSERT(m_Data.empty());
    m_Data.push_back(0);
    UINT32& data = m_Data.back();

    // bit [20]: hard repair enable
    switch (repairType)
    {
        case RepairType::HARD:
            data |= 1 << 20;
            break;

        case RepairType::SOFT:
            break; // Bit is 0. NOP.

        default:
            Printf(Tee::PriError, "Unsupported repair type '%s'. Defaulting to "
                   "soft repair\n", Memory::ToString(repairType).c_str());
            MASSERT(!"Unsupported repair type");
    }

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() << 19; // bit [19]: stack ID

    AssertWithinBitWidth(hbmPseudoChan.Number(), 1, "pseudo channel");
    m_Data.back() |= hbmPseudoChan.Number() << 18;

    AssertWithinBitWidth(stack.Number(), 4, "bank");
    data |= bank.Number() << 14;

    AssertWithinBitWidth(row, 14, "row address");
    data |= row;
}

/******************************************************************************/
// EnableFuseScanData

Hbm::SamsungHbm2XDie::EnableFuseScanData::EnableFuseScanData
(
    UINT32 fuseScanMaskValue
)
{
    MASSERT(m_Data.empty());
    m_Data.push_back(fuseScanMaskValue);
}

bool Hbm::SamsungHbm2XDie::EnableFuseScanData::IsRowAvailable() const
{
    // Bit 0 is set if used, unset if available
    return (m_Data.back() & 1U) == 0;
}

/******************************************************************************/
// EnableFuseScanLookupTable

Hbm::SamsungHbm2XDie::EnableFuseScanData
Hbm::SamsungHbm2XDie::EnableFuseScanLookupTable::GetWdrData
(
    const Stack& stack,
    const HbmChannel& hbmChannel,
    const PseudoChannel& hbmPseudoChan,
    const Bank& bank,
    UINT32 pseudoChannelSpareRow
)
{
    using namespace Utility;

    MASSERT(stack.Number() < 2);
    MASSERT(hbmPseudoChan.Number() < 2);
    MASSERT(bank.Number() < s_NumBanksPerChannel);
    MASSERT(pseudoChannelSpareRow < s_NumSpareRowsPerBank);

    constexpr UINT32 channelOffset               = s_NumLookupTableRows / 2;
    constexpr UINT32 upperBankOffset             = channelOffset / 2;
    constexpr UINT32 pseudoChannelOffset         = upperBankOffset / 2;
    constexpr UINT32 bankOffset                  = pseudoChannelOffset / 8;
    constexpr UINT32 pseudoChannelSpareRowOffset = bankOffset / 2;
    static_assert(pseudoChannelSpareRowOffset == 1,
                  "Invalid ENABLE_FUSE_SCAN lookup table offset callwlation");

    UINT32 rowIndex = 0; // Row index into the lookup table

    // Apply channel offset. Half the channels need it.
    switch (hbmChannel.Number())
    {
        case 0:
        case 1:
        case 4:
        case 5:
            break;

        case 2:
        case 3:
        case 6:
        case 7:
            rowIndex += channelOffset;
            break;

        default:
            Printf(Tee::PriError, "Invalid channel number\n");
            MASSERT(!"Invalid channel number");
            return RC::BAD_PARAMETER;
    }

    // Apply offset if accessing an upper bank
    if (bank.Number() >= s_NumBanksPerChannel / 2)
    {
        rowIndex += upperBankOffset;
    }

    // Apply pseudo channel offset
    rowIndex += hbmPseudoChan.Number() * pseudoChannelOffset;

    // Apply pseudo channel bank offset
    rowIndex += (bank.Number() % s_NumBanksPerPseudoChannel) * bankOffset;

    // Apply spare row offset
    rowIndex += pseudoChannelSpareRow * pseudoChannelSpareRowOffset;

    MASSERT(rowIndex < s_NumLookupTableRows);
    UINT32 mask = s_FuseMaskLookupTable[rowIndex];

    // Set the stack bits [23:24]
    AssertWithinBitWidth(stack.Number(), 2, "stack ID");
    mask |= (1 << stack.Number()) << 23;

    return EnableFuseScanData(mask);
}

const array<UINT32, Hbm::SamsungHbm2XDie::EnableFuseScanLookupTable::s_NumLookupTableRows>
Hbm::SamsungHbm2XDie::EnableFuseScanLookupTable::s_FuseMaskLookupTable =
{{
    0b00001001000000001000000000,
    0b00001001000000001010000000,
    0b00001001000000010000000000,
    0b00001001000000010010000000,
    0b00001001000000101000000000,
    0b00001001000000101010000000,
    0b00001001000000110000000000,
    0b00001001000000110010000000,
    0b00001001001000001000000000,
    0b00001001001000001010000000,
    0b00001001001000010000000000,
    0b00001001001000010010000000,
    0b00001001001000101000000000,
    0b00001001001000101010000000,
    0b00001001001000110000000000,
    0b00001001001000110010000000,
    0b00001001010000001000000000,
    0b00001001010000001010000000,
    0b00001001010000010000000000,
    0b00001001010000010010000000,
    0b00001001010000101000000000,
    0b00001001010000101010000000,
    0b00001001010000110000000000,
    0b00001001010000110010000000,
    0b00001001011000001000000000,
    0b00001001011000001010000000,
    0b00001001011000010000000000,
    0b00001001011000010010000000,
    0b00001001011000101000000000,
    0b00001001011000101010000000,
    0b00001001011000110000000000,
    0b00001001011000110010000000,
    0b00001010000000001000000000,
    0b00001010000000001010000000,
    0b00001010000000010000000000,
    0b00001010000000010010000000,
    0b00001010000000101000000000,
    0b00001010000000101010000000,
    0b00001010000000110000000000,
    0b00001010000000110010000000,
    0b00001010001000001000000000,
    0b00001010001000001010000000,
    0b00001010001000010000000000,
    0b00001010001000010010000000,
    0b00001010001000101000000000,
    0b00001010001000101010000000,
    0b00001010001000110000000000,
    0b00001010001000110010000000,
    0b00001010010000001000000000,
    0b00001010010000001010000000,
    0b00001010010000010000000000,
    0b00001010010000010010000000,
    0b00001010010000101000000000,
    0b00001010010000101010000000,
    0b00001010010000110000000000,
    0b00001010010000110010000000,
    0b00001010011000001000000000,
    0b00001010011000001010000000,
    0b00001010011000010000000000,
    0b00001010011000010010000000,
    0b00001010011000101000000000,
    0b00001010011000101010000000,
    0b00001010011000110000000000,
    0b00001010011000110010000000,
    0b00001001010000001000000000,
    0b00001001010000001010000000,
    0b00001001010000010000000000,
    0b00001001010000010010000000,
    0b00001001010000101000000000,
    0b00001001010000101010000000,
    0b00001001010000110000000000,
    0b00001001010000110010000000,
    0b00001001011000001000000000,
    0b00001001011000001010000000,
    0b00001001011000010000000000,
    0b00001001011000010010000000,
    0b00001001011000101000000000,
    0b00001001011000101010000000,
    0b00001001011000110000000000,
    0b00001001011000110010000000,
    0b00001001000000001000000000,
    0b00001001000000001010000000,
    0b00001001000000010000000000,
    0b00001001000000010010000000,
    0b00001001000000101000000000,
    0b00001001000000101010000000,
    0b00001001000000110000000000,
    0b00001001000000110010000000,
    0b00001001001000001000000000,
    0b00001001001000001010000000,
    0b00001001001000010000000000,
    0b00001001001000010010000000,
    0b00001001001000101000000000,
    0b00001001001000101010000000,
    0b00001001001000110000000000,
    0b00001001001000110010000000,
    0b00001010010000001000000000,
    0b00001010010000001010000000,
    0b00001010010000010000000000,
    0b00001010010000010010000000,
    0b00001010010000101000000000,
    0b00001010010000101010000000,
    0b00001010010000110000000000,
    0b00001010010000110010000000,
    0b00001010011000001000000000,
    0b00001010011000001010000000,
    0b00001010011000010000000000,
    0b00001010011000010010000000,
    0b00001010011000101000000000,
    0b00001010011000101010000000,
    0b00001010011000110000000000,
    0b00001010011000110010000000,
    0b00001010000000001000000000,
    0b00001010000000001010000000,
    0b00001010000000010000000000,
    0b00001010000000010010000000,
    0b00001010000000101000000000,
    0b00001010000000101010000000,
    0b00001010000000110000000000,
    0b00001010000000110010000000,
    0b00001010001000001000000000,
    0b00001010001000001010000000,
    0b00001010001000010000000000,
    0b00001010001000010010000000,
    0b00001010001000101000000000,
    0b00001010001000101010000000,
    0b00001010001000110000000000,
    0b00001010001000110010000000
}};
