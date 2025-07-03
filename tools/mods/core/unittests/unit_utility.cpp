/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unit_utility.cpp
 * @brief  Unit tests for Utility namespace
 */

#include "unittest.h"
#include "lwmisc.h"
#include "random.h"
#include "core/include/utility.h"
#include "core/include/xp.h"

namespace
{
    //----------------------------------------------------------------
    //! \brief Unit tests for the Utility namespace
    //!
    class UtilityTest: public UnitTest
    {
    public:
        UtilityTest();
        virtual void Run();
    private:
        Random m_Random;
    };
    ADD_UNIT_TEST(UtilityTest);
}

//--------------------------------------------------------------------
UtilityTest::UtilityTest() : UnitTest("UtilityTest")
{
    m_Random.SeedRandom(static_cast<UINT32>(Xp::GetWallTimeUS()));
}

//--------------------------------------------------------------------
//! \brief Run all Utility tests
//!
/* virtual */ void UtilityTest::Run()
{
    // BitScanForward & BitScanReverse
    //
    for (INT32 bitNum = 0; bitNum < 32; ++bitNum)
    {
        const UINT32 data  = m_Random.GetRandom() | (1 << bitNum);
        const INT32  loBit = m_Random.GetRandom(0, bitNum);
        const INT32  hiBit = m_Random.GetRandom(bitNum, 31);
        UINT32 mask;

        mask = DRF_SHIFTMASK(31:bitNum);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanForward(data & mask));

        mask = DRF_SHIFTMASK(31:bitNum) | ~DRF_SHIFTMASK(31:loBit);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanForward(data & mask, loBit));

        mask = DRF_SHIFTMASK(bitNum:0);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanReverse(data & mask));

        mask = DRF_SHIFTMASK(bitNum:0) | ~DRF_SHIFTMASK(hiBit:0);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanReverse(data & mask, hiBit));
    }
    UNIT_ASSERT_EQ(-1, Utility::BitScanForward(0));
    UNIT_ASSERT_EQ(-1, Utility::BitScanReverse(0));

    // BitScanForward64 & BitScanReverse64
    //
    for (INT32 bitNum = 0; bitNum < 64; ++bitNum)
    {
        const UINT64 data  = m_Random.GetRandom64() | (1ULL << bitNum);
        const INT32  loBit = m_Random.GetRandom(0, bitNum);
        const INT32  hiBit = m_Random.GetRandom(bitNum, 63);
        UINT64 mask;

        mask = DRF_SHIFTMASK64(63:bitNum);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanForward64(data & mask));

        mask = DRF_SHIFTMASK64(63:bitNum) | ~DRF_SHIFTMASK64(63:loBit);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanForward64(data & mask, loBit));

        mask = DRF_SHIFTMASK64(bitNum:0);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanReverse64(data & mask));

        mask = DRF_SHIFTMASK64(bitNum:0) | ~DRF_SHIFTMASK64(hiBit:0);
        UNIT_ASSERT_EQ(bitNum, Utility::BitScanReverse64(data & mask, hiBit));
    }
    UNIT_ASSERT_EQ(-1, Utility::BitScanForward64(0));
    UNIT_ASSERT_EQ(-1, Utility::BitScanReverse64(0));
}
