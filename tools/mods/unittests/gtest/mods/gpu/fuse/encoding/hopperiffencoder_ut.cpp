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

#include "gpu/fuse/encoding/hopperiffencoder.h"
#include "hwiffcrcalgo.h"

#include "gtest/gtest.h"

// Tests the HW implementation and HopperIffEncoder implementation
// against some pre-computed golden values.
TEST(HopperIffEncoderCrcTest, PrecomputedValuesTest)
{
    HopperIffEncoder hencoder;
    UINT32 hopperCrcVal = 0;
    UINT32 hwCrcVal = 0;
    vector<UINT32> goldelwals = {32056, 0, 20981, 42793, 19928};
    vector<vector<UINT32>> tests =
        {
        {1234, 765655, 545345354, 55555, 554443, 8920890},
        {0, 0, 0, 0},
        {120000, 789327, 3213213, 321312, 8888, 33213, 11111, 112, 33, 90333},
        {125201, 134403, 310675, 449023, 505109, 570817, 646146, 709016, 898486, 954260, 969570},
        {14431, 29517, 149210, 159531, 162704, 173628, 325985, 398107, 695643, 927169, 968373},
        };

    for (unsigned i = 0; i < tests.size(); i++)
    {
        hencoder.CallwlateCrc(tests[i], &hopperCrcVal);
        hwCrcVal = callwlateCRC(tests[i]);
        EXPECT_EQ(hwCrcVal, goldelwals[i]);
        EXPECT_EQ(hopperCrcVal, goldelwals[i]);
    }
}

// Tests random inputs to make sure that HW implementation
// and HopperIffEncoder implementation match.
TEST(HopperIffEncoderCrcTest, SimulatedInputsTest)
{
    HopperIffEncoder hencoder;
    UINT32 hopperCrcVal = 0;
    UINT32 hwCrcVal = 0;
    int test_vector_size = 200;
    vector<UINT32> test_vector(test_vector_size, 0);
    int iterations = 100;
    for (int i = 0; i < iterations; i++)
    {
        for (int j = 0; j < test_vector_size; j++)
        {
            test_vector[j] = rand();
        }
        hwCrcVal = callwlateCRC(test_vector);
        hencoder.CallwlateCrc(test_vector, &hopperCrcVal);
        EXPECT_EQ(hopperCrcVal, hwCrcVal);
    }
}