/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/utility/modscnsl.h"
#include <termios.h>
#include <unistd.h>

//! @file    linxmodc.cpp
//! @brief   Implement cross platform portion of ModsConsole
//!

namespace
{
    atomic<UINT32> s_EchoDisableCount{0};
    struct termios s_SavedStdinSettings;
}

void ModsConsole::EnterSingleCharMode(bool echo)
{
    if ((s_EchoDisableCount++ == 0) && isatty(fileno(stdin)))
    {
        // Disable canonical mode and echo
        struct termios newSettings;
        tcgetattr(fileno(stdin), &s_SavedStdinSettings);
        newSettings = s_SavedStdinSettings;
        newSettings.c_iflag &= ~ICRNL;
        newSettings.c_lflag &= ~ICANON;
        if (!echo)
        {
            newSettings.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ECHOPRT | ECHOKE);
        }
        tcsetattr(fileno(stdin), TCSANOW, &newSettings);
    }
}

void ModsConsole::ExitSingleCharMode()
{
    if ((--s_EchoDisableCount == 0) && isatty(fileno(stdin)))
    {
        tcsetattr(fileno(stdin), TCSANOW, &s_SavedStdinSettings);
    }
}
