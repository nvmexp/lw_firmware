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

#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2e_interface.h"

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
    //! HBM2E Samsung WIRs
    vector<Wir> s_SamsungHbm2eWirs =
    {
        { "HARD_REPAIR", WirT::HARD_REPAIR, 0x08, RegT::Write, 22,
          SupportedHbmModels(
                { SpecVersion::V2e },
                { Vendor::Samsung },
                { Die::Unknown },
                { StackHeight::All },
                { Revision::Unknown }),
          WirF::SingleChannelOnly
        },

        { "SOFT_REPAIR", WirT::SOFT_REPAIR, 0x07, RegT::Write, 22,
          SupportedHbmModels(
                  { SpecVersion::V2e },
                  { Vendor::Samsung },
                  { Die::Unknown },
                  { StackHeight::All },
                  { Revision::Unknown }),
          WirF::SingleChannelOnly
        },

        { "ENABLE_FUSE_SCAN", WirT::ENABLE_FUSE_SCAN, 0xc0, RegT::Read | RegT::Write, 26,
          SupportedHbmModels(
                  { SpecVersion::V2e },
                  { Vendor::Samsung },
                  { Die::Unknown },
                  { StackHeight::All },
                  { Revision::Unknown }),
        }
    };
}

RC SamsungHbm2e::Setup()
{
    RC rc;

    CHECK_RC(Hbm2Interface::Setup()); // Adds vendor independent WIRs

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

    if (m_HbmModel.spec != SpecVersion::V2e)
    {
        Printf(Tee::PriError, "Unsupported Samsung HBM specification:\n  %s\n",
                m_HbmModel.ToString().c_str());
        MASSERT(!"Attempt to setup Samsung interface with a unsupported HBM specification");
        return RC::UNSUPPORTED_FUNCTION;
    }

    CHECK_RC(AddWirs(s_SamsungHbm2eWirs));

    return rc;
}

/******************************************************************************/
// RowRepairWriteData

SamsungHbm2e::RowRepairWriteData::RowRepairWriteData
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

    // bit [21]: repair enable
    switch (repairType)
    {
        case RepairType::HARD:
        case RepairType::SOFT:
            data |= 1 << 21;
            break;

        default:
            Printf(Tee::PriError, "Unsupported repair type '%s'. Defaulting to "
                   "soft repair\n", Memory::ToString(repairType).c_str());
            MASSERT(!"Unsupported repair type");
    }

    AssertWithinBitWidth(stack.Number(), 1, "stack ID");
    data |= stack.Number() << 20;

    AssertWithinBitWidth(hbmPseudoChan.Number(), 1, "pseudo channel");
    m_Data.back() |= hbmPseudoChan.Number() << 19;

    AssertWithinBitWidth(stack.Number(), 4, "bank");
    data |= bank.Number() << 15;

    AssertWithinBitWidth(row, 15, "row address");
    data |= row;
}

/******************************************************************************/
// EnableFuseScanData

Hbm::SamsungHbm2e::EnableFuseScanData::EnableFuseScanData
(
    UINT32 fuseScanMaskValue
)
{
    MASSERT(m_Data.empty());
    m_Data.push_back(fuseScanMaskValue);
}

bool Hbm::SamsungHbm2e::EnableFuseScanData::IsRowAvailable() const
{
    // Bit 0 is set if used, unset if available
    return (m_Data.back() & 1U) == 0;
}

/******************************************************************************/
// EnableFuseScanLookupTable

Hbm::SamsungHbm2e::EnableFuseScanData
Hbm::SamsungHbm2e::EnableFuseScanLookupTable::GetWdrData
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

    constexpr UINT32 pseudoChannelOffset        = s_NumLookupTableRows / s_NumPseudoChannels;
    constexpr UINT32 bankOffset                 = pseudoChannelOffset / s_NumBanksPerPseudoChannel;
    UINT32 rowIndex = 0; // Row index into the lookup table
    // Note: s_FuseMaskLookupTable is common for all the channels in HBM2E.

    // Apply pseudo channel offset
    rowIndex += hbmPseudoChan.Number() * pseudoChannelOffset;

    // Apply pseudo channel bank offset
    rowIndex += bank.Number() * bankOffset;

    // Apply spare row offset
    rowIndex += pseudoChannelSpareRow;

    Printf(Tee::PriDebug, "SpareRowIndex = %d\n", rowIndex);

    MASSERT(rowIndex < s_NumLookupTableRows);
    UINT32 mask = s_FuseMaskLookupTable[rowIndex];

    // Set the stack bits [23:24]
    AssertWithinBitWidth(stack.Number(), 2, "stack ID");
    mask |= (1 << stack.Number()) << 23;

    Printf(Tee::PriDebug, "SpareRowIndex Mask = 0x%X\n", mask);

    return EnableFuseScanData(mask);
}

const array<UINT32, Hbm::SamsungHbm2e::EnableFuseScanLookupTable::s_NumLookupTableRows>
Hbm::SamsungHbm2e::EnableFuseScanLookupTable::s_FuseMaskLookupTable =
{
    {
        0b00001001000000001100000000,
        0b00001001000000001110000000,
        0b00001001000001001100000000,
        0b00001001000001001110000000,
        0b00001001000010001100000000,
        0b00001001000010001110000000,
        0b00001001000011001100000000,
        0b00001001000011001110000000,
        0b00001001000000010000000000,
        0b00001001000000010010000000,
        0b00001001000001010000000000,
        0b00001001000001010010000000,
        0b00001001000010010000000000,
        0b00001001000010010010000000,
        0b00001001000011010000000000,
        0b00001001000011010010000000,
        0b00001001000000101100000000,
        0b00001001000000101110000000,
        0b00001001000001101100000000,
        0b00001001000001101110000000,
        0b00001001000010101100000000,
        0b00001001000010101110000000,
        0b00001001000011101100000000,
        0b00001001000011101110000000,
        0b00001001000000110000000000,
        0b00001001000000110010000000,
        0b00001001000001110000000000,
        0b00001001000001110010000000,
        0b00001001000010110000000000,
        0b00001001000010110010000000,
        0b00001001000011110000000000,
        0b00001001000011110010000000,
        0b00001010000000001100000000,
        0b00001010000000001110000000,
        0b00001010000001001100000000,
        0b00001010000001001110000000,
        0b00001010000010001100000000,
        0b00001010000010001110000000,
        0b00001010000011001100000000,
        0b00001010000011001110000000,
        0b00001010000000010000000000,
        0b00001010000000010010000000,
        0b00001010000001010000000000,
        0b00001010000001010010000000,
        0b00001010000010010000000000,
        0b00001010000010010010000000,
        0b00001010000011010000000000,
        0b00001010000011010010000000,
        0b00001010000000101100000000,
        0b00001010000000101110000000,
        0b00001010000001101100000000,
        0b00001010000001101110000000,
        0b00001010000010101100000000,
        0b00001010000010101110000000,
        0b00001010000011101100000000,
        0b00001010000011101110000000,
        0b00001010000000110000000000,
        0b00001010000000110010000000,
        0b00001010000001110000000000,
        0b00001010000001110010000000,
        0b00001010000010110000000000,
        0b00001010000010110010000000,
        0b00001010000011110000000000,
        0b00001010000011110010000000,
        0b00001001010000001100000000,
        0b00001001010000001110000000,
        0b00001001010001001100000000,
        0b00001001010001001110000000,
        0b00001001010010001100000000,
        0b00001001010010001110000000,
        0b00001001010011001100000000,
        0b00001001010011001110000000,
        0b00001001010000010000000000,
        0b00001001010000010010000000,
        0b00001001010001010000000000,
        0b00001001010001010010000000,
        0b00001001010010010000000000,
        0b00001001010010010010000000,
        0b00001001010011010000000000,
        0b00001001010011010010000000,
        0b00001001010000101100000000,
        0b00001001010000101110000000,
        0b00001001010001101100000000,
        0b00001001010001101110000000,
        0b00001001010010101100000000,
        0b00001001010010101110000000,
        0b00001001010011101100000000,
        0b00001001010011101110000000,
        0b00001001010000110000000000,
        0b00001001010000110010000000,
        0b00001001010001110000000000,
        0b00001001010001110010000000,
        0b00001001010010110000000000,
        0b00001001010010110010000000,
        0b00001001010011110000000000,
        0b00001001010011110010000000,
        0b00001010010000001100000000,
        0b00001010010000001110000000,
        0b00001010010001001100000000,
        0b00001010010001001110000000,
        0b00001010010010001100000000,
        0b00001010010010001110000000,
        0b00001010010011001100000000,
        0b00001010010011001110000000,
        0b00001010010000010000000000,
        0b00001010010000010010000000,
        0b00001010010001010000000000,
        0b00001010010001010010000000,
        0b00001010010010010000000000,
        0b00001010010010010010000000,
        0b00001010010011010000000000,
        0b00001010010011010010000000,
        0b00001010010000101100000000,
        0b00001010010000101110000000,
        0b00001010010001101100000000,
        0b00001010010001101110000000,
        0b00001010010010101100000000,
        0b00001010010010101110000000,
        0b00001010010011101100000000,
        0b00001010010011101110000000,
        0b00001010010000110000000000,
        0b00001010010000110010000000,
        0b00001010010001110000000000,
        0b00001010010001110010000000,
        0b00001010010010110000000000,
        0b00001010010010110010000000,
        0b00001010010011110000000000,
        0b00001010010011110010000000,
        0b00001011000000001100000000,
        0b00001011000000001110000000,
        0b00001011000001001100000000,
        0b00001011000001001110000000,
        0b00001011000010001100000000,
        0b00001011000010001110000000,
        0b00001011000011001100000000,
        0b00001011000011001110000000,
        0b00001011000000010000000000,
        0b00001011000000010010000000,
        0b00001011000001010000000000,
        0b00001011000001010010000000,
        0b00001011000010010000000000,
        0b00001011000010010010000000,
        0b00001011000011010000000000,
        0b00001011000011010010000000,
        0b00001011000000101100000000,
        0b00001011000000101110000000,
        0b00001011000001101100000000,
        0b00001011000001101110000000,
        0b00001011000010101100000000,
        0b00001011000010101110000000,
        0b00001011000011101100000000,
        0b00001011000011101110000000,
        0b00001011000000110000000000,
        0b00001011000000110010000000,
        0b00001011000001110000000000,
        0b00001011000001110010000000,
        0b00001011000010110000000000,
        0b00001011000010110010000000,
        0b00001011000011110000000000,
        0b00001011000011110010000000,
        0b00001100000000001100000000,
        0b00001100000000001110000000,
        0b00001100000001001100000000,
        0b00001100000001001110000000,
        0b00001100000010001100000000,
        0b00001100000010001110000000,
        0b00001100000011001100000000,
        0b00001100000011001110000000,
        0b00001100000000010000000000,
        0b00001100000000010010000000,
        0b00001100000001010000000000,
        0b00001100000001010010000000,
        0b00001100000010010000000000,
        0b00001100000010010010000000,
        0b00001100000011010000000000,
        0b00001100000011010010000000,
        0b00001100000000101100000000,
        0b00001100000000101110000000,
        0b00001100000001101100000000,
        0b00001100000001101110000000,
        0b00001100000010101100000000,
        0b00001100000010101110000000,
        0b00001100000011101100000000,
        0b00001100000011101110000000,
        0b00001100000000110000000000,
        0b00001100000000110010000000,
        0b00001100000001110000000000,
        0b00001100000001110010000000,
        0b00001100000010110000000000,
        0b00001100000010110010000000,
        0b00001100000011110000000000,
        0b00001100000011110010000000,
        0b00001011010000001100000000,
        0b00001011010000001110000000,
        0b00001011010001001100000000,
        0b00001011010001001110000000,
        0b00001011010010001100000000,
        0b00001011010010001110000000,
        0b00001011010011001100000000,
        0b00001011010011001110000000,
        0b00001011010000010000000000,
        0b00001011010000010010000000,
        0b00001011010001010000000000,
        0b00001011010001010010000000,
        0b00001011010010010000000000,
        0b00001011010010010010000000,
        0b00001011010011010000000000,
        0b00001011010011010010000000,
        0b00001011010000101100000000,
        0b00001011010000101110000000,
        0b00001011010001101100000000,
        0b00001011010001101110000000,
        0b00001011010010101100000000,
        0b00001011010010101110000000,
        0b00001011010011101100000000,
        0b00001011010011101110000000,
        0b00001011010000110000000000,
        0b00001011010000110010000000,
        0b00001011010001110000000000,
        0b00001011010001110010000000,
        0b00001011010010110000000000,
        0b00001011010010110010000000,
        0b00001011010011110000000000,
        0b00001011010011110010000000,
        0b00001100010000001100000000,
        0b00001100010000001110000000,
        0b00001100010001001100000000,
        0b00001100010001001110000000,
        0b00001100010010001100000000,
        0b00001100010010001110000000,
        0b00001100010011001100000000,
        0b00001100010011001110000000,
        0b00001100010000010000000000,
        0b00001100010000010010000000,
        0b00001100010001010000000000,
        0b00001100010001010010000000,
        0b00001100010010010000000000,
        0b00001100010010010010000000,
        0b00001100010011010000000000,
        0b00001100010011010010000000,
        0b00001100010000101100000000,
        0b00001100010000101110000000,
        0b00001100010001101100000000,
        0b00001100010001101110000000,
        0b00001100010010101100000000,
        0b00001100010010101110000000,
        0b00001100010011101100000000,
        0b00001100010011101110000000,
        0b00001100010000110000000000,
        0b00001100010000110010000000,
        0b00001100010001110000000000,
        0b00001100010001110010000000,
        0b00001100010010110000000000,
        0b00001100010010110010000000,
        0b00001100010011110000000000,
        0b00001100010011110010000000
    }
};
