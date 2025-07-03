
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

//! @file    outcwrap.h
//! @brief   Declare OutputConsoleWrapper -- A wrapper for console classes
//! that cannot accept input only .

#pragma once

#include "rc.h"
#include "console.h"
#include <string>

//!
//! @class(OutputConsoleWrapper)  OutputConsoleWrapper.
//!
//! This class wraps a console to prevent the console from performing
//! input.
//!
class OutputConsoleWrapper : public Console
{
public:
    OutputConsoleWrapper();
    virtual ~OutputConsoleWrapper();
    void SetWrappedConsole(Console *pConsole) { m_pWrappedConsole = pConsole; }
    virtual RC Enable(ConsoleBuffer *pConsoleBuffer);
    virtual RC Disable();
    virtual RC Suspend();
    virtual bool IsEnabled() const;
    virtual RC PopUp();
    virtual RC PopDown();
    virtual bool IsPoppedUp() const;
    virtual RC SetDisplayDevice(GpuDevice *pDevice);
    virtual GpuDevice * GetDisplayDevice();
    virtual void AllowYield(bool bAllowYield);
    virtual UINT32 GetScreenColumns() const;
    virtual UINT32 GetScreenRows() const;
    virtual void GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const;
    virtual void SetLwrsorPosition(INT32 row, INT32 column);
    virtual void ClearTextScreen();
    virtual RC SetLwrsor(Cursor Type);
    virtual RC ScrollScreen(INT32 lines);
    virtual void PrintStringToScreen(const char*           str,
                                     size_t                strLen,
                                     Tee::ScreenPrintState state);
    virtual VirtualKey GetKey();
    virtual RC GetString(string *pString);
    virtual bool KeyboardHit();
    virtual bool Formatting() const;
    virtual bool Echo() const;
private:
    Console *m_pWrappedConsole; //!< Pointer to the wrapped console
};
