/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    nlwrcnsl.h
//! @brief   Declare NlwrsesConsole -- Nlwrses Console Class.

#pragma once

#include "rawcnsl.h"
#include "rc.h"
#include <deque>

//!
//! @class(NlwrsesConsole)  Nlwrses Console.
//!
//! This defines a console using the Nlwrses library and is only available
//! on platforms which support that library.
//!
//! For detailed comments on each of the public interfaces, see console.h
class NlwrsesConsole : public RawConsole
{
public:
    NlwrsesConsole();
    virtual ~NlwrsesConsole();
    virtual RC Enable(ConsoleBuffer *pConsoleBuffer);
    virtual RC Disable();
    virtual bool IsEnabled() const;
    virtual UINT32 GetScreenColumns() const;
    virtual UINT32 GetScreenRows() const;
    virtual void GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const;
    virtual void SetLwrsorPosition(INT32 row, INT32 column);
    virtual RC SetLwrsor(Console::Cursor Type);
    virtual void ClearTextScreen();
    virtual void PrintStringToScreen(const char*           str,
                                     size_t                strLen,
                                     Tee::ScreenPrintState state);
    virtual bool Formatting() const;
    virtual bool Echo() const;
protected:
    virtual INT32 GetChar(bool bYield);
    virtual RC    PrivGetString(char *pStr, UINT32 size);
    virtual void  FlushChars(deque<Console::VirtualKey> *pKeyQueue,
                             const Console::VirtualKey  *RawKeyMap);
    virtual bool  PrivKeyboardHit();
private:
    bool m_bEnabled;  //!< true if the console is enabled

    struct WINDOW;
    WINDOW *stdscr = nullptr;

    typedef int (*fn_cbreak)();
    typedef int (*fn_clear)();
    typedef int (*fn_lwrs_set)(int);
    typedef int (*fn_endwin)();
    typedef WINDOW* (*fn_initscr)();
    typedef int (*fn_move)(int, int);
    typedef int (*fn_nodelay)(WINDOW*, bool);
    typedef int (*fn_noecho)();
    typedef int (*fn_nonl)();
    typedef int (*fn_printw)(const char *, ...);
    typedef int (*fn_refresh)();
    typedef int (*fn_scrollok)(WINDOW*, bool);
    typedef int (*fn_ungetch)(int);
    typedef int (*fn_wgetch)(WINDOW*);
    typedef int (*fn_wgetnstr)(WINDOW*, char*, int);

    fn_cbreak cbreak = nullptr;
    fn_cbreak clear = nullptr;
    fn_lwrs_set lwrs_set = nullptr;
    fn_endwin endwin = nullptr;
    fn_initscr initscr = nullptr;
    fn_move move = nullptr;
    fn_nodelay nodelay = nullptr;
    fn_noecho noecho = nullptr;
    fn_nonl nonl = nullptr;
    fn_printw printw = nullptr;
    fn_refresh refresh = nullptr;
    fn_scrollok scrollok = nullptr;
    fn_ungetch ungetch = nullptr;
    fn_wgetch wgetch = nullptr;
    fn_wgetnstr wgetnstr = nullptr;
};
