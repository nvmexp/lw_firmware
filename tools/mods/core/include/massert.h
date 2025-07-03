/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013,2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_MASSERT_H
#define INCLUDED_MASSERT_H

//
// Declare the function to be called when an assertion is not true.
//
// Prints a message about what file/line this happened at,
// then tries to bring up the debugger (using Xp::BreakPoint()),
// then halts the program.
//
// see core/main/massert.cpp
//
extern void ModsAssertFail(const char *file, int line, const char *function, const char *cond);

#ifdef DEBUG
   #define MASSERT(test) ((void)((test)||(::ModsAssertFail(__FILE__,__LINE__,MODS_FUNCTION, #test),0)))
#else
   #define MASSERT(test) ((void)0)
#endif

#ifdef DEBUG
    #define MASSERT_DECL(x) x
#else
    #define MASSERT_DECL(x)
#endif

//added to help Coverity Prevent handle MASSERTs
#ifdef __COVERITY__
#undef MASSERT
#define MASSERT(test) assert(test)
#endif

#define MASSERT_FAIL() \
    MASSERT(!"Generic assertion failure<refer to previous message>.")

// Compile Time assert
// -------------------
// Use ct_assert(b) instead of assert(b) whenever the condition 'b' is
// constant, i.e. when 'b' can be determined at compile time.
//
#define ct_assert(test) static_assert(test, #test)

#endif // INCLUDED_MASSERT_H
