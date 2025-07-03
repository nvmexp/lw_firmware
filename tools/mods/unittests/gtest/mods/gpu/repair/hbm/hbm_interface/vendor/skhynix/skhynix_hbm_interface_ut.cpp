/*
 * NVIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by NVIDIA Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to NVIDIA
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of NVIDIA Corporation is prohibited.
 *
 * NVIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"
#include "gpu/repair/hbm/hbm_interface/vendor/skhynix/skhynix_hbm_interface.h"
#include "gtest/gtest.h"

#include <climits>

using namespace Memory::Hbm;

using HynixRowBurnedFuseMask = SkHynixHbm2::RowBurnedFuseMask;
using HynixRowFuse = SkHynixHbm2::RowFuse;

TEST(SkHynixHbm2Interface, RowBurnedFuseMask)
{
    HynixRowBurnedFuseMask fuseMask;
    UINT08 bits;
    HynixRowFuse fuse;
    constexpr UINT08 numBits = sizeof(bits) * CHAR_BIT;

    // All bits set => all fuses used
    bits = 0xff;
    fuseMask = HynixRowBurnedFuseMask(bits);
    EXPECT_EQ(fuseMask.Mask(), bits);
    EXPECT_NE(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);

    // No bit set => all fuses available
    bits = 0;
    fuseMask = HynixRowBurnedFuseMask(bits);
    EXPECT_EQ(fuseMask.Mask(), bits);
    EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
    EXPECT_EQ(fuse.Number(), 0U);

    // Single bit set
    for (UINT32 n = 0; n < numBits; ++n)
    {
        bits = static_cast<UINT08>(1) << n;
        fuseMask = HynixRowBurnedFuseMask(bits);
        EXPECT_EQ(fuseMask.Mask(), bits);
        EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
        if (n == 0)
        {
            EXPECT_EQ(fuse.Number(), 1U);
        }
        else
        {
            EXPECT_EQ(fuse.Number(), 0U);
        }
    }

    // Sequential unsetting from MSB
    //
    // ex. 0111 -> 0011 -> 0001
    //
    // Skip the all set and none set since those are already covered.
    bits = 0x7f;
    for (UINT32 n = 0; n < numBits - 1; ++n)
    {
        const UINT08 nextBit = numBits - 1 - n;
        bits &= Utility::Bitmask<UINT08>(nextBit);
        fuseMask = HynixRowBurnedFuseMask(bits);
        EXPECT_EQ(fuseMask.Mask(), bits);
        EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
        EXPECT_EQ(fuse.Number(), nextBit);
    }

    // Sequential unsetting from LSB
    //
    // ex. 1110 -> 1100 -> 1000
    //
    // Skip the all set and none set since those are already covered.
    bits = 0xfe;
    for (UINT32 n = 0; n < numBits - 1; ++n)
    {
        bits &= Utility::Bitmask<UINT08>(numBits - 1 - n) << (n + 1);
        fuseMask = HynixRowBurnedFuseMask(bits);
        EXPECT_EQ(fuseMask.Mask(), bits);
        EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
        EXPECT_EQ(fuse.Number(), 0U);
    }

    // Random bits
    //
    bits = 0b00010000;
    fuseMask = HynixRowBurnedFuseMask(bits);
    EXPECT_EQ(fuseMask.Mask(), bits);
    EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
    EXPECT_EQ(fuse.Number(), 0U);

    bits = 0b00100010;
    fuseMask = HynixRowBurnedFuseMask(bits);
    EXPECT_EQ(fuseMask.Mask(), bits);
    EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
    EXPECT_EQ(fuse.Number(), 0U);

    bits = 0b10011111;
    fuseMask = HynixRowBurnedFuseMask(bits);
    EXPECT_EQ(fuseMask.Mask(), bits);
    EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
    EXPECT_EQ(fuse.Number(), 5U);

    bits = 0b10001011;
    fuseMask = HynixRowBurnedFuseMask(bits);
    EXPECT_EQ(fuseMask.Mask(), bits);
    EXPECT_EQ(fuseMask.GetFirstAvailableFuse(&fuse), RC::OK);
    EXPECT_EQ(fuse.Number(), 2U);
}

TEST(SkHynixHbm2Interface, SoftRepairWriteData)
{
    // TODO(stewarts):
}

TEST(SkHynixHbm2Interface, HardRepairData)
{
    // TODO(stewarts):
}

TEST(SkHynixHbm2Interface, BankPair)
{
    // TODO(stewarts):
}

TEST(SkHynixHbm2Interface, PprInfoReadData)
{
    // TODO(stewarts): this is the important one
}
