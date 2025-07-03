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

//! @file    nlwrcnsl.cpp
//! @brief   Implement NlwrsesConsole

#include "core/include/nlwrcnsl.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/massert.h"
#include "core/include/cnslmgr.h"
#include "core/include/platform.h"

#include <dlfcn.h>
#include <nlwrses.h>

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
NlwrsesConsole::NlwrsesConsole() :
    m_bEnabled(false)
{
}
//-----------------------------------------------------------------------------
//! \brief Destructor
//!
NlwrsesConsole::~NlwrsesConsole()
{
}

#define GET_NC_FUNCTION(function_name)                             \
{                                                                  \
    function_name =                                                \
        (fn_##function_name) dlsym(libnlwrsesDLL, #function_name); \
    if (!function_name)                                            \
    {                                                              \
        errString += " - error: ";                                 \
        errString += dlerror();                                    \
        continue;                                                  \
    }                                                              \
}

/* virtual */ RC NlwrsesConsole::Enable(ConsoleBuffer *pConsoleBuffer)
{
    if (wgetnstr == nullptr)
    {
        bool missingFunction = true;
        constexpr UINT32 highestVersion = 7;
        constexpr UINT32 lowestVersion = 5;
        string errString = "Cannot load nlwrses:";
        for (UINT32 verIdx = highestVersion; verIdx >= lowestVersion; verIdx--)
        {
            const string filename = Utility::StrPrintf("libnlwrses.so.%u", verIdx);
            errString += "\n   " + filename;
            void* libnlwrsesDLL = dlopen(filename.c_str(), RTLD_NOW);
            if (libnlwrsesDLL == nullptr)
            {
                errString += " - error: ";
                errString += dlerror();
                continue;
            }

            GET_NC_FUNCTION(cbreak);
            GET_NC_FUNCTION(clear);
            GET_NC_FUNCTION(lwrs_set);
            GET_NC_FUNCTION(endwin);
            GET_NC_FUNCTION(initscr);
            GET_NC_FUNCTION(move);
            GET_NC_FUNCTION(nodelay);
            GET_NC_FUNCTION(noecho);
            GET_NC_FUNCTION(nonl);
            GET_NC_FUNCTION(printw);
            GET_NC_FUNCTION(refresh);
            GET_NC_FUNCTION(scrollok);
            GET_NC_FUNCTION(ungetch);
            GET_NC_FUNCTION(wgetch);
            GET_NC_FUNCTION(wgetnstr);
            missingFunction = false;
            break;
        }
        if (missingFunction)
        {
            wgetnstr = nullptr;
            printf("%s\n", errString.c_str());
            return RC::DLL_LOAD_FAILED;
        }
    }

    stdscr = initscr();
    MASSERT(NULL != stdscr);
    cbreak();
    nodelay(stdscr, true);
    noecho();
    scrollok(stdscr, true);
    nonl();
    m_bEnabled = true;
    return OK;
}

/* virtual */ RC NlwrsesConsole::Disable()
{
    if (endwin) endwin();
    setvbuf(stdout, NULL, _IONBF, 0);
    m_bEnabled = false;
    return OK;
}
/* virtual */ bool NlwrsesConsole::IsEnabled() const
{
    return m_bEnabled;
}
/* virtual */ UINT32 NlwrsesConsole::GetScreenColumns() const
{
    _win_st* const ws = reinterpret_cast<_win_st* const>(stdscr);
    return getmaxx(ws);
}
/* virtual */  UINT32 NlwrsesConsole::GetScreenRows() const
{
    _win_st* const ws = reinterpret_cast<_win_st* const>(stdscr);
    return getmaxy(ws);
}
/* virtual */  void NlwrsesConsole::GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const
{
    _win_st* const ws = reinterpret_cast<_win_st* const>(stdscr);
    INT32 y = 0, x = 0;
    getyx(ws, y, x);
    if (pRow)
       *pRow = y;
    if (pColumn)
       *pColumn = x;
}
/* virtual */  void NlwrsesConsole::SetLwrsorPosition(INT32 row, INT32 column)
{
    move(row, column);
    refresh();
}
/* virtual */  RC NlwrsesConsole::SetLwrsor(Console::Cursor Type)
{
    INT32 ret;

    switch (Type)
    {
        default:
        case Console::LWRSOR_NORMAL:
            ret = lwrs_set(1);
            break;
        case Console::LWRSOR_BLOCK:
            ret = lwrs_set(2);
            break;
        case Console::LWRSOR_NONE:
            ret = lwrs_set(0);
            break;
    }
    return (ret == ERR) ? RC::UNSUPPORTED_FUNCTION : OK;
}
/* virtual */  void NlwrsesConsole::ClearTextScreen()
{
    clear();
    refresh();
}
/* virtual */  void NlwrsesConsole::PrintStringToScreen
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    printw("%.*s", static_cast<int>(strLen), str);
    refresh();
}
/* virtual */ bool NlwrsesConsole::Formatting() const
{
    return true;
}
/* virtual */ bool NlwrsesConsole::Echo() const
{
    return true;
}

//-----------------------------------------------------------------------------
//! \brief Private function to get a string from the input.
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC NlwrsesConsole::PrivGetString(char *pStr, UINT32 size)
{
    if (getnstr(pStr, size) == ERR)
        return RC::FILE_READ_ERROR;

    return OK;
}

//-----------------------------------------------------------------------------
//! Create a NlwrsesConsole and register it with ConsoleManager.
//! ConsoleManager owns the pointer so the console stays around during static
//! destruction
static Console *CreateNlwrsesConsole() { return new NlwrsesConsole; }
static ConsoleFactory s_NlwrsesConsole(CreateNlwrsesConsole, "nlwrses", false);
