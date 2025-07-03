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

#include "core/include/massert.h"
#include "core/include/regexp.h"
#include "core/include/tee.h"

//--------------------------------------------------------------------
//! \brief default constructor
//!
//! The default regular expression is "", which matches any string
//! (like ".*").
//!
RegExp::RegExp() : m_Pattern(""), m_Flags(0)
{
    ApplyRegex(m_Pattern, m_Flags);
}

//--------------------------------------------------------------------
//! \brief copy constructor
//!
RegExp::RegExp(const RegExp &rhs)
{
    *this = rhs;
}

//--------------------------------------------------------------------
//! \brief destructor
//!
RegExp::~RegExp()
{
}

//--------------------------------------------------------------------
//! \brief assignment operator
//!
RegExp &RegExp::operator=(const RegExp &rhs)
{
    m_Pattern = rhs.m_Pattern;
    m_Flags  = rhs.m_Flags;
    m_Regex = rhs.m_Regex;
    m_Substrings = rhs.m_Substrings;
    return *this;
}

//--------------------------------------------------------------------
//! \brief Set the regular expression
//!
//! Set this object's members and construct the internal std::regex object.
//!
//! \param pattern The regular expression pattern string.
//! \param flags Flags to control how the regular expression is evaluated
//!
RC RegExp::ApplyRegex(const string &pattern, UINT32 flags)
{
    RC rc;
    m_Pattern = pattern;
    m_Flags = flags;
    m_Substrings.clear();

    // default grammar for std::regex
    regex_constants::syntax_option_type regexFlags = regex_constants::ECMAScript;

    if (flags & ICASE)
    {
        regexFlags |= regex_constants::icase;
    }
    if (!(flags & SUB))
    {
        regexFlags |= regex_constants::nosubs;
    }

    // If FULL is set, make sure we match the beginning and end of string.
    string regexStr = (m_Flags & FULL) ? "^(" + m_Pattern + ")$" : m_Pattern;
    try
    {
        m_Regex = regex(regexStr, regexFlags);
    }
    catch (const std::regex_error& e)
    {
        Printf(Tee::PriError, "Bad regular expression '%s': %s\n",
            m_Pattern.c_str(), e.what());
        rc = RC::BAD_PARAMETER; 
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Set the regular expression
//!
//! Set the regular expression for this object unless the new
//! pattern and flags are the same as the existing ones. This
//! method may be called multiple times.
//!
//! \param pattern The regular expression pattern string.
//! \param flags Flags to control how the regular expression is evaluated
//!
RC RegExp::Set(const string &pattern, UINT32 flags)
{
    RC rc;
    if (pattern != m_Pattern || flags != m_Flags)
    {
        rc = ApplyRegex(pattern, flags);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Check whether the regular expression matches the argument
//!
//! There are two versions of Matches() with identical arguments.
//! This one is called when the RegExp object is const.
//!
//! This method will MASSERT if you passed the SUB flag to Set(),
//! since SUB stores substring information in the RegExp.  To use SUB,
//! you must call Matches() on a non-const RegExp object.
//!
//! \param str The string to match against the regular expression
//! \return true if the RegExp matches the string, false if not
//!
bool RegExp::Matches(const string &str) const
{
    MASSERT((m_Flags & SUB) == 0);
    return const_cast<RegExp*>(this)->Matches(str);
}

//--------------------------------------------------------------------
//! \brief Check whether the regular expression matches the argument
//!
//! There are two versions of Matches() with identical arguments.
//! This one is called when the RegExp object is non-const.
//!
//! This method can be used on any valid RegExp (i.e. Set() didn't
//! return an error).  It can even be used on a freshly-constructed
//! RegExp which will use the empty regex "".
//!
//! \param str The string to match against the regular expression
//! \return true if the RegExp matches the string, false if not
//!
bool RegExp::Matches(const string &str)
{
    smatch sm;
    bool result = regex_search(str, sm, m_Regex);

    if (result && (m_Flags & SUB))
    {
        m_Substrings.clear();
        for (UINT32 i = 0; i < sm.size(); i++)
        {
            // Skip substrings[1]; it comes from the parentheses we
            // added to implement FULL
            if (i == 1 && (m_Flags & FULL))
                continue;
            m_Substrings.push_back(sm.str(i));
        }
    }

    // Return the result, ilwerting it if ILWERT was set.
    return (m_Flags & ILWERT) ? (!result) : result;
}

//--------------------------------------------------------------------
//! \brief Return the substrings generated by Matches() if SUB is used
//!
//! Passing the SUB flag to Set() will cause the RegExp to do
//! substring matching.  After calling Matches():
//!
//!   - GetSubstring(0) will return the entire string passed to Matches()
//!   - GetSubstring(1) will return the substring that matched the first
//!     pair of parentheses () in the RegExp.
//!   - GetSubstring(2) will return the substring that matched the second
//!     pair of parentheses () in the RegExp.
//!   - etc.
//!
//! This method will MASSERT if you pass an index higher than the
//! number of parentheses in the RegExp, or if the SUB flag wasn't set
//! in the RegExp.
//!
string RegExp::GetSubstring(UINT32 index) const
{
    MASSERT(m_Flags & SUB);
    MASSERT(index < m_Substrings.size());
    return m_Substrings[index];
}
