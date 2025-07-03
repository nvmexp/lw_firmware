/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/fuse/downbin/fsmask.h"

#include "gtest/gtest.h"

TEST(DownbinFsMask, SetBit)
{
    FsMask mask1(5);

    EXPECT_EQ(mask1.SetBit(0), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x1U);
    EXPECT_EQ(mask1.SetBit(2), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x5U);

    EXPECT_EQ(mask1.SetBit(5), RC::BAD_PARAMETER);
}

TEST(DownbinFsMask, ClearBit)
{
    FsMask mask1(5);

    EXPECT_EQ(mask1.SetMask(0xF), RC::OK);
    EXPECT_EQ(mask1.ClearBit(0), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0xEU);
    EXPECT_EQ(mask1.ClearBit(2), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0xAU);
    EXPECT_EQ(mask1.ClearBit(4), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0xAU);

    EXPECT_EQ(mask1.ClearBit(5), RC::BAD_PARAMETER);
}

TEST(DownbinFsMask, SetBits)
{
    FsMask mask1(5);

    EXPECT_EQ(mask1.SetBits(0x1), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x1U);
    EXPECT_EQ(mask1.SetBits(0x4), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x5U);
    EXPECT_EQ(mask1.SetBits(0x0), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x5U);
    EXPECT_EQ(mask1.SetBits(0x1F), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x1FU);

    EXPECT_EQ(mask1.SetBits(0x20), RC::BAD_PARAMETER);
}

TEST(DownbinFsMask, ClearBits)
{
    FsMask mask1(5);

    EXPECT_EQ(mask1.ClearBits(0x2), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x0U);

    EXPECT_EQ(mask1.SetMask(0xF), RC::OK);
    EXPECT_EQ(mask1.ClearBits(0x5), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0xAU);
    EXPECT_EQ(mask1.ClearBits(0x0), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0xAU);

    EXPECT_EQ(mask1.SetMask(0xF), RC::OK);
    EXPECT_EQ(mask1.ClearBits(0xF), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x0U);
}

TEST(DownbinFsMask, GetNumSetBits)
{
    FsMask mask1(5);

    EXPECT_EQ(mask1.SetMask(0x0), RC::OK);
    EXPECT_EQ(mask1.GetNumSetBits(), 0U);

    EXPECT_EQ(mask1.SetMask(0x5), RC::OK);
    EXPECT_EQ(mask1.GetNumSetBits(), 2U);

    EXPECT_EQ(mask1.SetMask(0x1F), RC::OK);
    EXPECT_EQ(mask1.GetNumSetBits(), 5U);
}

TEST(DownbinFsMask, GetNumUnsetBits)
{
    FsMask mask1(5);

    EXPECT_EQ(mask1.SetMask(0x0), RC::OK);
    EXPECT_EQ(mask1.GetNumUnsetBits(), 5U);

    EXPECT_EQ(mask1.SetMask(0x5), RC::OK);
    EXPECT_EQ(mask1.GetNumUnsetBits(), 3U);

    EXPECT_EQ(mask1.SetMask(0x1F), RC::OK);
    EXPECT_EQ(mask1.GetNumUnsetBits(), 0U);
}

TEST(DownbinFsMask, SetIlwertedMask)
{
    FsMask mask1(5);
    FsMask disMask(5), refMask(5);

    EXPECT_EQ(mask1.SetMask(0x0), RC::OK);
    EXPECT_EQ(disMask.SetMask(0x0), RC::OK);
    EXPECT_EQ(refMask.SetMask(0x0), RC::OK);
    EXPECT_EQ(mask1.SetIlwertedMask(disMask, refMask), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x0U);

    EXPECT_EQ(disMask.SetMask(0x5), RC::OK);
    EXPECT_EQ(mask1.SetIlwertedMask(disMask, refMask), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x0U);

    EXPECT_EQ(refMask.SetMask(0x1B), RC::OK);
    EXPECT_EQ(mask1.SetIlwertedMask(disMask, refMask), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x1AU);

    EXPECT_EQ(mask1.SetIlwertedMask(disMask), RC::OK);
    EXPECT_EQ(mask1.GetMask(), 0x1AU);
}
