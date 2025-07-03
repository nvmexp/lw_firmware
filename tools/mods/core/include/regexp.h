/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file  regexp.h
//! \brief Defines regular expression class

#ifndef INCLUDED_REGEXP_H
#define INCLUDED_REGEXP_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#include <regex>

#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

//! \brief RegExp class manages regular expressions.
//!
//! This is intended to consolidate a lot of the repeated regular
//! expression code into one place so that other classes can use this
//! class as a member object.
//!
//! RegExp is a general wrapper around std::regex. RegExp uses the
//! ECMAScript grammar (default for std::regex). It can do matching
//! or ilwerse matching (based on the ILWERT flag).
//!
//! Typical usage:
//! \code
//!     RegExp regexp;
//!     CHECK_RC(regexp.Set("^[Rr]egular expression.*"));
//!     if (regexp.Matches(someString)) ...
//! \endcode
//!
class RegExp
{
public:
    RegExp();
    RegExp(const RegExp& rhs);
    ~RegExp();
    RegExp &operator=(const RegExp &rhs);
    RC      Set(const string &pattern, UINT32 flags = 0);
    bool    Matches(const string &str) const;
    bool    Matches(const string &str);
    string  GetSubstring(UINT32 index) const;
    string  ToString()                 const { return m_Pattern; }
    UINT32  GetFlags()                 const { return m_Flags; }

    enum Flags
    {
        ILWERT = 0x01,  //!< Do ilwerse matching
        FULL   = 0x02,  //!< Regular expression must match entire string
        ICASE  = 0x04,  //!< Ignore case when matching (same as REG_ICASE)
        SUB    = 0x08,  //!< Support substring matching (opposite of REG_NOSUB)
    };

private:
    RC ApplyRegex(const string &pattern, UINT32 flags);

    string m_Pattern;            //!< regex pattern string
    UINT32 m_Flags;              //!< regex flags
    regex m_Regex;               //!< C++ stdlib regex object
    vector<string> m_Substrings; //!< substrings created by Matches()
};

#endif
