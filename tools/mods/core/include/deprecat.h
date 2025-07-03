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

/**
 * @file  deprecat.h
 * @brief Used to implement deprecation mechanism in javascript functions
 */

#pragma once

#include <set>
#include <string>

struct JSContext;

//! \brief Class used to support deprecated javascript functions
//!
//! This class should be used within a deprecated javascript back-end
//! function.  It provides a mechanism so that the user gets an error
//! message when the function is called telling how to fix the script,
//! and takes the deprecated function through 3 stages of deprecation:
//!
//! -# Initially, the function will fail with a notice saying, "deprecated
//!    function, here's how to fix it" message.  The user can turn
//!    off the error message and enable the function by calling
//!    Deprecation::Allow (in C++) or AllowDeprecation (in JavaScript)
//!    on the function name.
//! -# After 60 days, the user can still use the function as described
//!    above, but the error message can't be turned off any more.
//!    Also, the error message turns into a great big 50-line spew
//!    with lots of uppercase letters and exclamation points.
//! -# After 120 days, the function stops working.  Period.
//!    The error message still tells how to fix the script, of course.
//!
//! Here is a typical example of how this class would be used in
//! a JavaScript function named "Foo", which was deprecated on 3/22/2006.
//! It will start generating "loud" error messages 60 days later (5/21/2006),
//! and stop working 120 days later (7/20/2006):
//!
//! \verbatim
//! C_(Global_Foo)
//! {
//!     MASSERT(pContent != NULL);
//!
//!     static Deprecation depr("Foo", "3/22/2006",
//!                             "Use ImprovedFoo instead of Foo");
//!     if (!depr.IsAllowed(pContent))
//!     {
//!         RETURN_RC(RC::UNSUPPORTED_FUNCTION);
//!     }
//!
//!     /* Rest of function goes here */
//! }
//! \endverbatim
//!
//! Note: The deprecation phases are based on the build date, not the
//! computer clock.  (I don't trust the clocks on DOS computers.)
//! The build date is updated whenever version.c recompiles.
//!
class Deprecation
{
public:
    Deprecation
    (
        const char *functionName,
        const char *deprecationDate,
        const char *fix
    );
    bool        IsAllowed(JSContext *pContext);
    static void Allow(const char *functionName);
    void        PrintErrorMessage();
private:
    static UINT32 ParseDate(const char *date);
    static UINT32 AddDays(UINT32 date, UINT32 daysToAdd);
    static string FmtDate(UINT32 date);
    enum Phase { QUIET_PHASE, LOUD_PHASE, REMOVED_PHASE };
    string m_functionName;    //< Name of the deprecated javascript function
    string m_deprecationDate; //< Date on which the feature was deprecated
    string m_loudDate;        //< Date on which the warning messages get loud
    string m_removalDate;     //< Date on which the function stops working
    string m_fix;             //< How to fix the script to not use the function
    Phase  m_phase;           //< What phase we are in, based on the date
    static set<string> s_allowedNames;
                              //< Names of deprecated functions that may be used
};
