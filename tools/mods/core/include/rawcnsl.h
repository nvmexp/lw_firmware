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

//! @file    rawcnsl.h
//! @brief   Declare RawConsole -- Raw Console Class.

#pragma once

#include "console.h"
#include "rc.h"
#include <deque>

//!
//! @class RawConsole
//! Raw Console.
//!
//! This defines the default raw console available on all platforms.
//! The raw console interacts with the default console present on the
//! platform and is only available (on GPU builds) when the user interface
//! is enabled.  For most platforms, this console is implemented using
//! default POSIX interfaces (getc, fgets, fputs, fflush)
//!
//! Note that there are some platform specific implementation portions
//! of this class which reside in the appropriate platform directory.
//! It is important that the input functions (which will typically remain
//! constant across all consoles on a particular platform) be implemented
//! within the RawConsole class rather than a subclass.  This ensures that
//! any subclasses which require input may derive from the RawConsole class
//! rather than being required to implement one version per subclass.
//!
//! For detailed comments on each of the public interfaces, see console.h
class RawConsole : public Console
{
public:
    RawConsole();
    virtual ~RawConsole();
    static  Console *CreateRawConsole();
    RC Enable(ConsoleBuffer *pConsoleBuffer) override;
    RC Disable() override;
    RC Suspend() override;
    bool IsEnabled() const override;
    RC PopUp() override;
    RC PopDown() override;
    bool IsPoppedUp() const override;
    RC SetDisplayDevice(GpuDevice *pDevice) override;
    GpuDevice * GetDisplayDevice() override;
    void AllowYield(bool bAllowYield) override;
    UINT32 GetScreenColumns() const override;
    UINT32 GetScreenRows() const override;
    void GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const override;
    void SetLwrsorPosition(INT32 row, INT32 column) override;
    RC SetLwrsor(Console::Cursor Type) override;
    void ClearTextScreen() override;
    RC ScrollScreen(INT32 lines) override;
    void PrintStringToScreen
    (
        const char*           str,
        size_t                strLen,
        Tee::ScreenPrintState state
    ) override;
    Console::VirtualKey GetKey() override;
    RC GetString(string *pString) override;
    bool KeyboardHit() override;
    bool Formatting() const override;
    bool Echo() const override;

protected:
    virtual INT32 GetChar(bool bYield);
    virtual RC    PrivGetString(char *pStr, UINT32 size);
    virtual void  FlushChars(deque<Console::VirtualKey> *pKeyQueue,
                             const Console::VirtualKey  *RawKeyMap);
    virtual Console::VirtualKey HandleEscapeSequence();
    virtual bool                PrivKeyboardHit();
    virtual bool                GetAllowYield() { return m_bAllowYield; }
    virtual void EnterSingleCharMode(bool echo);
    virtual void ExitSingleCharMode();

private:
    bool m_bAllowYield = false;;
};
