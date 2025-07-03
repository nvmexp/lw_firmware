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

//! @file    linxrawc.h
//! @brief   Declare LinuxRawConsole -- Raw Console Class for Linux.

#pragma once

#include "core/include/rawcnsl.h"
#include "core/include/rc.h"
#include <termios.h>

//!
//! @class(LinuxRawConsole)  Linux Raw Console.
//!
//! This defines the default raw console with specific implementation for
//! Linux.  The raw console interacts with the default console present on
//! the platform and is only available (on GPU builds) when the user
//! interface is enabled.
//!
//! For detailed comments on each of the public interfaces, see console.h
class LinuxRawConsole : public RawConsole
{
public:
    LinuxRawConsole()
    : m_Enabled(false),
      m_ColorTerm(DetectColorTerm())
    {
    }
    virtual ~LinuxRawConsole() { }
    virtual RC Enable(ConsoleBuffer* pConsoleBuffer);
    virtual RC Disable();
    virtual bool IsEnabled() const;
    virtual void PrintStringToScreen(const char*           str,
                                     size_t                strLen,
                                     Tee::ScreenPrintState state);
    bool IsColorTerm() const { return m_ColorTerm; }

private:
    static bool DetectColorTerm();

    bool m_Enabled;
    bool m_ColorTerm;
};
