/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  deprecat.cpp
 * @brief Used to implement deprecation mechanism in javascript functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "core/include/version.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/deprecat.h"
#include "core/include/utility.h"

set<string> Deprecation::s_allowedNames;
static const int DAYS_UNTIL_LOUD    =  60;
static const int DAYS_UNTIL_REMOVAL = 120;

//--------------------------------------------------------------------
//! \brief Construct a Deprecation for a deprecated javascript function
//!
//! \param functionName The name of the deprecated javascript function
//! or property
//! \param deprecationDate The date on which it was deprecated,
//! in m/d/y format (e.g. 3/22/2006).
//! \param fix An explanation of how to change the javascript program
//! so that it doesn't use the deprecated function.
//!
Deprecation::Deprecation
(
    const char *functionName,
    const char *deprecationDate,
    const char *fix
)
{
    m_functionName    = functionName;
    m_fix             = fix;

    UINT32 iBuildDate       = ParseDate(g_BuildDate);
    UINT32 iDeprecationDate = ParseDate(deprecationDate);
    UINT32 iLoudDate        = AddDays(iDeprecationDate, DAYS_UNTIL_LOUD);
    UINT32 iRemovalDate     = AddDays(iDeprecationDate, DAYS_UNTIL_REMOVAL);

    m_deprecationDate = FmtDate(iDeprecationDate);
    m_loudDate        = FmtDate(iLoudDate);
    m_removalDate     = FmtDate(iRemovalDate);

    if (iBuildDate < iLoudDate)
    {
        m_phase = QUIET_PHASE;
    }
    else if (iBuildDate < iRemovalDate)
    {
        m_phase = LOUD_PHASE;
    }
    else
    {
        m_phase = REMOVED_PHASE;
    }
}

//--------------------------------------------------------------------
//! \brief Test whether a deprecated javascript function can be used
//!
//! Test whether the deprecated function can be used.  Deprecated
//! functions cannot be used unless someone has called Deprecation::Allow.
//!
//! \param pContext The JavaScript context.  If non-NULL, then this
//! function prints an error message if the deprecated function is
//! not allowed, or if the deprecation has reached the "loud" phase.
//! \return true if the deprecated feature is allowed, false if not.
//!
bool Deprecation::IsAllowed(JSContext *pContext)
{
    bool allowed = false;
    bool printWarning = true;

    switch (m_phase)
    {
        case QUIET_PHASE:
            allowed = true;
            printWarning = (s_allowedNames.count(m_functionName) == 0);
            break;
        case LOUD_PHASE:
            allowed = (s_allowedNames.count(m_functionName) > 0);
            printWarning = true;
            break;
        case REMOVED_PHASE:
            allowed = false;
            printWarning = true;
            break;
        default:
            MASSERT(!"Illegal case in Deprecation::IsAllowed");
            break;
    }

    if (printWarning && (pContext != NULL))
    {
        PrintErrorMessage();
        JS_ReportWarning(pContext, "DEPRECATED FEATURE: \"%s\" is deprecated;"
                         " see above", m_functionName.c_str());
    }
    return allowed;
}

//--------------------------------------------------------------------
//! \brief Allows a deprecated feature to be used
//!
//! \param functionName The name of the deprecated function or property
//! to allow.  This must be the same as the "functionName" argument
//! passed to the constructor of the deprecated function's Deprecation
//! obect.
//!
//! This can be called from JavaScript, as "AllowDeprecation()".
//!
void Deprecation::Allow(const char *functionName)
{
    s_allowedNames.insert(functionName);
}

//! \brief Allow a deprecated function to be used
//!
//! This is a thin wrapper around Deprecation::Allow(), which a allows
//! a deprecated function to be used.
//! \param name The name of the deprecated function
//!
JS_STEST_GLOBAL
(
    AllowDeprecation,
    1,
    "Allow a deprecated function to be used."
)
{
    STEST_HEADER(1, 1, "Usage: AllowDeprecation(function_name)");
    STEST_ARG(0, string, functionName);
    Deprecation::Allow(functionName.c_str());
    RETURN_RC(OK);
}

JS_STEST_GLOBAL
(
    Deprecate,
    3,
    "Deprecate the current JS function"
)
{
    STEST_HEADER(3, 3, "Usage: Deprecate(functionName, MM/DD/YYYY, fixString)");
    STEST_ARG(0, string, functionName);
    STEST_ARG(1, string, date);
    STEST_ARG(2, string, fix);

    Deprecation depr(functionName.c_str(), date.c_str(), fix.c_str());
    if (!depr.IsAllowed(pContext))
    {
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);
    }
    RETURN_RC(OK);
}

//--------------------------------------------------------------------
//! \brief Print a "this feature is deprecated" error message.
//!
//! Print a "this feature is deprecated" error message, which tells
//! how to fix the script.  This message gets more insistent at later
//! phases of the deprecation.
//!
void Deprecation::PrintErrorMessage()
{
    INT32 pri = Tee::PriHigh;
    int ii;

    Printf(pri, "\n");
    switch (m_phase)
    {
        case QUIET_PHASE:
            Printf(pri, "DEPRECATED FEATURE:\n");
            Printf(pri, "\"%s\" is deprecated as of %s."
                   "  It will be removed soon.\n",
                   m_functionName.c_str(), m_deprecationDate.c_str());
            Printf(pri, "For now, you can use it if you add"
                   " AllowDeprecation(\"%s\") to\n", m_functionName.c_str());
            Printf(pri, "your JavaScript program, or by using"
                   " \"-allow_deprecation %s\".\n", m_functionName.c_str());
            Printf(pri, "Before this feature is removed,"
                   " you must rewrite your script as follows:\n");
            break;
        case LOUD_PHASE:
            for (ii = 0; ii < 50; ii++)
            {
                Printf(pri, "DEPRECATED FEATURE!\n");
            }
            Printf(pri, "\n");
            Printf(pri, "\"%s\" has been DEPRECATED since %s!"
                  "  IT WILL STOP WORKING\n",
                  m_functionName.c_str(), m_deprecationDate.c_str());
            Printf(pri, "ON %s!!!  Make the following change to your"
                   " JavaScript program NOW:\n", m_removalDate.c_str());
            break;
        case REMOVED_PHASE:
            for (ii = 0; ii < 50; ii++)
            {
                Printf(pri, "DEPRECATED FEATURE!\n");
            }
            Printf(pri, "\n");
            Printf(pri, "\"%s\" has been deprecated since %s."
                   "  It was removed on\n",
                   m_functionName.c_str(), m_deprecationDate.c_str());
            Printf(pri, "%s.  You must make the following change to your"
                   " JavaScript program:\n", m_removalDate.c_str());
            break;
    }
    Printf(pri, "\n");
    Printf(pri, "vvvvvvvvvvvvvvvvvvvv\n");
    Printf(pri, "%s\n", m_fix.c_str());
    Printf(pri, "^^^^^^^^^^^^^^^^^^^^\n");
}

//--------------------------------------------------------------------
//! \brief Colwerts a string date into a UINT32.
//!
//! Colwerts a date from several different string formats into a
//! UINT32 of the form 0xyyyymmdd.  This can be easily compared,
//! or processed with AddDays or FmtDate.
//!
//! The input string must be of one of the following forms, or else an
//! assertion will occur:
//!
//! - "m/d/y" (e.g. "3/22/2006")  This is the required format for
//!   the date arguments passed to the constructor.
//! - "Mmm dd yyyy" (e.g. "Mar 22 2006")  This is the ANSI-regulated
//!   format of the date generated by the __DATE__ preprocessor directive.
//!
//! This method is used by the constructor.
//!
//! Note:  It might have been easier to return a time_t, but the
//! functions that manipulate time/date info are notorious for
//! platforms that "improve" on the standard.
//!
UINT32 Deprecation::ParseDate(const char *date)
{
    UINT32 year  = 0;
    UINT32 month = 0;
    UINT32 day   = 0;
    char *endp;

    if (isalpha(date[0]))
    {
        // Must be of form "Mmm dd yyyy"
        //
        const char *months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        for (int ii = 0; ii < 12; ii++)
        {
            if (strncmp(date, months[ii], 3) == 0)
            {
                month = ii + 1;
                break;
            }
        }
        endp = const_cast<char*>(&date[3]);
        MASSERT(*endp == ' ');
        endp += strspn(endp, " ");
        day = strtoul(endp, &endp, 10);
        MASSERT(*endp == ' ');
        endp += strspn(endp, " ");
        year = strtoul(endp, &endp, 10);
        MASSERT(*endp == '\0');
    }
    else
    {
        // Must be of form "m/d/y"
        //
        month = strtoul(date, &endp, 10);
        MASSERT(*endp == '/');
        day = strtoul(&endp[1], &endp, 10);
        MASSERT(*endp == '/');
        year = strtoul(&endp[1], &endp, 10);
        MASSERT(*endp == '\0');
    }

    MASSERT(year  >= 2000 && year  <= 9999);
    MASSERT(month >= 1    && month <= 12);
    MASSERT(day   >= 1    && day   <= 31);
    return (year << 16) + (month << 8) + (day);
}

//--------------------------------------------------------------------
//! \brief Add the specified number of days to a date returned by ParseDate.
//!
//! Add the specified number of days to a date returned by ParseDate.
//!
//! Note:  This method doesn't process leap years correctly, so it can
//! add an extra day during leap years.  But since it's only used in
//! the constructor to find a date that's *roughly* two months away, who
//! cares if it's not 100% precise?
//!
//! \param date A 32-bit date of the form 0xyyyymmdd, as returned by
//! ParseDate.
//! \param daysToAdd Number of days to add to date.
//! \return date + daysToAdd, in 0xyyyymmdd format.
//!
UINT32 Deprecation::AddDays(UINT32 date, UINT32 daysToAdd)
{
    UINT32 daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    UINT32 year  = (date >> 16) & 0x0ffff;
    UINT32 month = (date >> 8) & 0x0ff;
    UINT32 day   = (date & 0x0ff) + daysToAdd;

    while (day > daysInMonth[month])
    {
        day -= daysInMonth[month];
        month++;
        if (month > 12)
        {
            month -= 12;
            year++;
        }
    }
    return (year << 16) + (month << 8) + (day);
}

//--------------------------------------------------------------------
//! \brief Colwert a UINT32 into a string date of the form "m/d/y".
//!
//! Colwert a 32-bit date of the form 0xyyyymmdd into a string of
//! the form "m/d/y".  This method is used by the constructor.
//!
string Deprecation::FmtDate(UINT32 date)
{
    return Utility::StrPrintf("%d/%d/%04d",
             static_cast<int>((date >> 8) & 0x0ff),
             static_cast<int>(date & 0x0ff),
             static_cast<int>((date >> 16) & 0x0ffff) );
}
