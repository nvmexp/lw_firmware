/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/utility/char_utility.h"
#include "gtest/gtest.h"

#include <cctype>
#include <limits>

using namespace CharUtility;

TEST(CharUtilityTest, IsLowerCase)
{
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c)
    {
        EXPECT_EQ(IsLowerCase(c), !!std::islower(c));
    }
}

TEST(CharUtilityTest, IsUpperCase)
{
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c)
    {
        EXPECT_EQ(IsUpperCase(c), !!std::isupper(c));
    }

}

TEST(CharUtilityTest, IsAlpha)
{
    for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c)
    {
        EXPECT_EQ(IsAlpha(c), !!std::isalpha(c));
    }

}

TEST(CharUtilityTest, ToUpperCase)
{
    for (int c = 0; c <= std::numeric_limits<char>::max(); ++c)
    {
        EXPECT_EQ(static_cast<int>(ToUpperCase(c)), std::toupper(c));
    }

}
