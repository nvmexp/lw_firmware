/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include <stdio.h>

/**
 *------------------------------------------------------------------------------
 * @function(ModsAssertFail)
 *
 * Print a warning message to screen, then try to bring up the debugger,
 * then halt the program.
 *------------------------------------------------------------------------------
 */
void ModsAssertFail(const char *file, int line, const char *function, const char *cond)
{
    static bool lwrsed = false;

    if (lwrsed)
        return;  // No re-lwrsing.

    lwrsed = true;

    //
    // Say what happened.
    //
    printf("%s: MASSERT(%s) failed at %s line %d\n", function, cond, file, line);
    fflush(stdout);

    Printf(Tee::PriError, "%s: MASSERT(%s) failed at %s line %d\n", function, cond, file, line);

    //
    // Try to bring up the debugger.  Crashes the application if no debugger.
    //
    Platform::BreakPoint(RC::ASSERT_FAILURE_DETECTED, file, line);

    lwrsed = false;
}

/**
 *------------------------------------------------------------------------------
 * @function(printchar)
 *
 * Debug function called by the regex library, when compile with DEBUG defined,
 * and when the extern int Debug is set to 1.
 *------------------------------------------------------------------------------
 */
extern "C" {

// including this prototype fixes a compile warning on the Mac.
void printchar ( char c);

void
printchar ( char c)
{
    if (c < 040 || c >= 0177)
    {
        putchar ('\\');
        putchar (((c >> 6) & 3) + '0');
        putchar (((c >> 3) & 7) + '0');
        putchar ((c & 7) + '0');
    }
    else
        putchar (c);
}

}
