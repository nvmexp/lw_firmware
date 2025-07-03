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

//! @file    nullcnsl.h
//! @brief   Declare NullConsole

#pragma once

#include "console.h"

//!
//! @class(NullConsole)  Null Console.
//!
//! This defines a null console which provides default implementations but
//! accomplishes nothing.
//!
//! For detailed comments on each of the public interfaces, see console.h
class NullConsole : public Console
{
public:
    NullConsole();
    virtual ~NullConsole();
    virtual RC Enable(ConsoleBuffer *pConsoleBuffer);
    virtual RC Disable();
    virtual bool IsEnabled() const { return true; }
    virtual RC Suspend() { return OK; }
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
    virtual RC SetLwrsor(Console::Cursor Type);
    virtual void ClearTextScreen();
    virtual RC ScrollScreen(INT32 lines);
    virtual void PrintStringToScreen(const char*           str,
                                     size_t                strLen,
                                     Tee::ScreenPrintState state);
    virtual Console::VirtualKey GetKey();
    virtual RC GetString(string *pString);
    virtual bool KeyboardHit();
    virtual bool Formatting() const;
    virtual bool Echo() const;
};
