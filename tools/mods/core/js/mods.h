/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MODS_H
#define INCLUDED_MODS_H

var g_ModsHId = "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/core/js/mods.h#25 $";

//------------------------------------------------------------------------------
// Macros
//

function CastRc(rc)
{
    switch (typeof(rc))
    {
        case "number":
            return rc | 0;  // bitwise ops coerce to UINT32
        case "undefined":
            return OK;      // treat uninitialized vars as OK
        default:
            return RC.CANNOT_COLWERT_JSVAL_TO_INTEGER;
    }
}
Log.Never("CastRc");

//#define DEBUG_CHECK_RC 1
#if defined (DEBUG_CHECK_RC)
    function CheckRcFail(rc, failFuncArguments)
    {
        Out.Printf(Out.PriError, "CHECK_RC failed (%d) in %s(%s)\n",
                   rc,
                   failFuncArguments.callee.name,
                   Array.prototype.join.call(failFuncArguments));

        // Breakpoint here if -jsd was used.
        debugger;
    }
#else
    function CheckRcFail(rc, arguments) { }
#endif

#define CHECK_RC(f)                     \
    do                                  \
    {                                   \
        if (OK != (rc = CastRc(f)))     \
        {                               \
            CheckRcFail(rc, arguments); \
            return rc;                  \
        }                               \
    } while (false)

#define CHECK_RC_CLEANUP(f)             \
    do                                  \
    {                                   \
        if (OK != (rc = CastRc(f)))     \
        {                               \
            CheckRcFail(rc, arguments); \
            break Cleanup;              \
        }                               \
    } while (false)

#define FIRST_RC(f)                     \
    do                                  \
    {                                   \
        rc = CastRc(f);                 \
        firstRc = CastRc(firstRc);      \
        if (rc)                         \
        {                               \
            CheckRcFail(rc, arguments); \
            if (OK == firstRc)          \
                firstRc = rc;           \
        }                               \
    } while (false);

#define THROW_RC(f)                     \
    do                                  \
    {                                   \
        if (OK != (rc = CastRc(f)))     \
        {                               \
            CheckRcFail(rc, arguments); \
            throw rc;                   \
        }                               \
    } while (false)

function HandleUnknownException(e)
{
    if (!(e instanceof JsException) && (typeof(e) !== "number"))
        throw e;
}
#define CATCH_RC(statement)                                              \
    try                                                                  \
    {                                                                    \
        statement ;                                                      \
    }                                                                    \
    catch (e)                                                            \
    {                                                                    \
        HandleUnknownException(e);                                       \
        if (typeof(e) === "number")                                      \
            return CastRc(e);                                            \
        else if (e instanceof JsException)                               \
            return CastRc(e.rc);                                         \
    }

//------------------------------------------------------------------------------
// Test Mode is a global mode that selects:
//  - a standard list of tests (that can be overridden with -test or -skip)
//  - duration of tests (i.e. number of loops, etc.)
//  - strictness of error-checks (for tests that have fuzzy pass/fail checks)
//
// A test mode is exclusive; we must be in just one mode.
//
// TM_ENGR is for engineering experiments inside lwpu.
//   Disallowed in Official/customer mods packages.
//
// TM_COMPUTE is like TM_ENGR, except it is allowed in Official/customer
//   packages and is allowed only on the compute/lwca products.
//
// TM_MFG_CHIP is for functional chip-screening with a socketted board,
//   also known as SLT, inside and outside lwpu.
//
// TM_MFG_BOARD is for outside board-manufacturing partners to test
//   motherboards (mcptest.js) or gpu add-in cards (gputest.js).
//
// TM_OQA is for an Outgoing Quality Assurance test at OEM lwstomers after
//   system assembly and burn-in.
//   This is a faster, less-strict test than TM_MFG_BOARD or TM_MFG_CHIP.
//
// TM_END_USER is for an end-user test.
//   It uses a "bound" js file and self-generated CRCs so we can distribute
//   a single mods.exe that takes no arguments and requires no setup.
//
// TM_DVS is for automated regression testing of mods itself.

// Define the last index that the common test modes use so that other scripts
// can start where the common masks leave off
#define TM_LAST_COMMON_INDEX 7

#define TM_ILWALID       0
#define TM_NONE          0
#define TM_OQA           (1 << 0)
#define TM_MFG_CHIP      (1 << 1)
#define TM_MFG_BOARD     (1 << 2)
#define TM_MFG_BOARD2    (1 << 3)
#define TM_ENGR          (1 << 4)
#define TM_END_USER      (1 << 5)
#define TM_DVS           (1 << 6)
#define TM_COMPUTE       (1 << TM_LAST_COMMON_INDEX)
#define TM_MFG_PLUS      (TM_MFG_CHIP | TM_MFG_BOARD | TM_MFG_BOARD2 | \
                          TM_ENGR | TM_DVS | TM_COMPUTE)
#define TM_OQA_PLUS      (TM_MFG_PLUS | TM_OQA)
#define TM_ALL           (TM_OQA | TM_MFG_CHIP | TM_MFG_BOARD | TM_MFG_BOARD2 | \
                          TM_ENGR | TM_END_USER | TM_DVS | TM_COMPUTE)

// Different test categories
#define TC_PSEUDO           (1 << 0)  // Doesn't actually test anything, per se. Can be used to start some
                                      // other test or functionality. Usually paired with TC_PHASE.
#define TC_PHASE            (1 << 1)  // These tests mark the beginning or end of "phases" of the testlist -
                                      // something that will affect all tests that follow it.
                                      // For example, running something in the background or setting the PState.
#define TC_MEMORY           (1 << 2)
#define TC_DISPLAY          (1 << 3)
#define TC_LWLINK           (1 << 4)
#define TC_VIDEO            (1 << 5)
#define TC_GRAPHICS         (1 << 6)
#define TC_3D               (1 << 7)
#define TC_LWDA             (1 << 8)
#define TC_IGNORE_GOLDEN    (1 << 9)
#define TC_C2C              (1 << 10)
#define TC_NONE             0

//--------------------------------------------------------------------
// Print masks used by prntutil.js to hide (mask off) some data from
// lwstomers
//
#define PRNT_INTERNAL       1
#define PRNT_EXTERNAL       2
#define PRNT_ANY           (PRNT_INTERNAL | PRNT_EXTERNAL)
#define PRNT_OSX            4
#define PRNT_COMPUTE        8
#define PRNT_EMULATION      16

// DO NOT USE : These are only retained for legacy purposes of not breaking scripts not under
// MODS control
#define PRNT_ENG_BUILD      1
#define PRNT_OFFICIAL_BUILD 2
#define PRNT_QUAL_BUILD     2
#define PRNT_ANY_BUILD      (PRNT_INTERNAL | PRNT_EXTERNAL)

//--------------------------------------------------------------------
#ifdef DEBUG_JS
#define MASSERT(f) \
    if (!f) \
    { \
        Out.Printf(Out.PriError, "MASSERT(%s) failed\n", #f); \
        debugger; \
    }
#else
#define MASSERT(f)
#endif

// In JS 1.4 times we used HAS_OWN macro to hide the fact that JS 1.4 did not
// support hasOwnProperty().  Keep HAS_OWN as a function for now in case user
// scripts are still using it and print a message so users have time to fix
// their scripts.
function HAS_OWN(obj, prop)
{
    Out.Printf(Out.PriWarn, "HAS_OWN is deprecated and will be removed!\n");
    Out.Printf(Out.PriWarn, "Fix your script by replacing HAS_OWN with hasOwnProperty() now\n");
    return obj.hasOwnProperty(prop);
}

if (UserInterface === UI_Script)
{
    function print()
    {
        if (arguments.length)
        {
            let args = Array.prototype.slice.call(arguments);
            args[0] += "\n";
            args.unshift(Out.PriNormal);
            Out.Printf.apply(Out, args);
        }
    }
}

#endif // !INCLUDED_MODS_H
