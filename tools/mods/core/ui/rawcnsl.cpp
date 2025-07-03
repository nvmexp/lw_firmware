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

//! @file    rawcnsl.cpp
//! @brief   Implement RawConsole

#include "core/include/rawcnsl.h"
#include "core/include/cnslmgr.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include <stdio.h>
#include <string.h>

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
RawConsole::RawConsole()
{
}
//-----------------------------------------------------------------------------
//! \brief Destructor
//!
RawConsole::~RawConsole()
{
}
/* static */ Console *RawConsole::CreateRawConsole() { return new RawConsole; }
/* virtual */ RC RawConsole::Enable(ConsoleBuffer *pConsoleBuffer)
{
    return OK;
}
/* virtual */ RC RawConsole::Disable()
{
    return OK;
}
/* virtual */ RC RawConsole::Suspend()
{
    return Disable();
}
/* virtual */ bool RawConsole::IsEnabled() const
{
    return true;
}
/* virtual */ RC RawConsole::PopUp()
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ RC RawConsole::PopDown()
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ bool RawConsole::IsPoppedUp() const
{
    return false;
}
/* virtual */ RC RawConsole::SetDisplayDevice(GpuDevice *pDevice)
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ GpuDevice * RawConsole::GetDisplayDevice()
{
    return nullptr;
}
/* virtual */ void RawConsole::AllowYield(bool bAllowYield)
{
    m_bAllowYield = bAllowYield;
}
/* virtual */ UINT32 RawConsole::GetScreenColumns() const
{
    return 80;
}
/* virtual */  UINT32 RawConsole::GetScreenRows() const
{
    return 50;
}
/* virtual */  void RawConsole::GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const
{
    if (pRow)
        *pRow = 0;
    if (pColumn)
        *pColumn = 0;
}
/* virtual */  void RawConsole::SetLwrsorPosition(INT32 row, INT32 column)
{
}
/* virtual */  RC RawConsole::SetLwrsor(Console::Cursor Type)
{
    return OK;
}
/* virtual */  void RawConsole::ClearTextScreen()
{
}
/* virtual */  RC RawConsole::ScrollScreen(INT32 lines)
{
    return OK;
}
/* virtual */  void RawConsole::PrintStringToScreen
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    if (fwrite(str, 1, strLen, stdout) != strLen)
        return;
    fflush(stdout);
}
/* virtual */ RC RawConsole::GetString(string *pString)
{
    RC rc;

    if (m_bAllowYield)
    {
        EnterSingleCharMode(true);
        DEFER
        {
            ExitSingleCharMode();
        };
        while (!KeyboardHit())
        {
            Tasker::Yield();
        }
    }

    char buf[1024];
    CHECK_RC(PrivGetString(buf, sizeof(buf)));

    pString->assign(buf);
    return rc;
}
/* virtual */ bool RawConsole::Formatting() const
{
    return false;
}
/* virtual */ bool RawConsole::Echo() const
{
    return false;
}

//-----------------------------------------------------------------------------
//! \brief Private function to get a string from the input.
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC RawConsole::PrivGetString(char *pStr, UINT32 size)
{
    if ( !fgets(pStr, size, stdin) )
       return RC::FILE_READ_ERROR;

    // Remove trailing \n
    const size_t len = strlen(pStr);
    if ((len > 0) && (pStr[len-1] == '\n'))
    {
        pStr[len-1] = 0;
    }

    return OK;
}
