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

#pragma once

namespace CharUtility
{
    //!
    //! \brief Return true if the given character is a lower case alpha
    //! character. A constexpr std::islower.
    //!
    constexpr bool IsLowerCase(char c) { return 'a' <= c && c <= 'z'; }

    //!
    //! \brief Return true if the given character is an upper case alpha
    //! character. A constexpr std::isupper.
    //!
    constexpr bool IsUpperCase(char c) { return 'A' <= c && c <= 'Z'; }

    //!
    //! \brief Return true if the given character is an alpha character
    //! ([a-zA-Z]). A constexpr std::isalpha.
    //!
    constexpr bool IsAlpha(char c) { return IsLowerCase(c) || IsUpperCase(c); }

    //!
    //! \brief Return the upper case of the given character. Returned as-is if
    //! non-numeric. A constexpr std::toupper.
    //!
    constexpr char ToUpperCase(char c)
    {
        return ((!IsAlpha(c) || IsUpperCase(c))
                ? c
                : c - ('a' - 'A'));
    }
}
