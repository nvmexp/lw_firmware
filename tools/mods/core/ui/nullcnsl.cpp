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

//! @file    nlwrcnsl.cpp
//! @brief   Implement NullConsole

#include "core/include/nullcnsl.h"
#include "core/include/cnslmgr.h"
#include <deque>

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
NullConsole::NullConsole()
{
}
//-----------------------------------------------------------------------------
//! \brief Destructor
//!
NullConsole::~NullConsole()
{
}
/* virtual */ RC NullConsole::Enable(ConsoleBuffer *pConsoleBuffer)
{
    return OK;
}
/* virtual */ RC NullConsole::Disable()
{
    return OK;
}
/* virtual */ RC NullConsole::PopUp()
{
    return OK;
}
/* virtual */ RC NullConsole::PopDown()
{
    return OK;
}
/* virtual */ bool NullConsole::IsPoppedUp() const
{
    return false;
}
/* virtual */ RC NullConsole::SetDisplayDevice(GpuDevice *pDevice)
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ GpuDevice * NullConsole::GetDisplayDevice()
{
    return NULL;
}
/* virtual */ void NullConsole::AllowYield(bool bAllowYield)
{
}
/* virtual */ UINT32 NullConsole::GetScreenColumns() const
{
    return 80;
}
/* virtual */  UINT32 NullConsole::GetScreenRows() const
{
    return 50;
}
/* virtual */  void NullConsole::GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const
{
    if (pRow)
        *pRow = 0;
    if (pColumn)
        *pColumn = 0;
}
/* virtual */  void NullConsole::SetLwrsorPosition(INT32 row, INT32 column)
{
}
/* virtual */  RC NullConsole::SetLwrsor(Console::Cursor Type)
{
    return OK;
}
/* virtual */  void NullConsole::ClearTextScreen()
{
}
/* virtual */  RC NullConsole::ScrollScreen(INT32 lines)
{
    return OK;
}
/* virtual */  void NullConsole::PrintStringToScreen
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
}
/* virtual */  Console::VirtualKey NullConsole::GetKey()
{
    return VKC_NONE;
}
/* virtual */ RC NullConsole::GetString(string *pString)
{
    return RC::FILE_READ_ERROR;
}
/* virtual */  bool NullConsole::KeyboardHit()
{
    return false;
}
/* virtual */ bool NullConsole::Formatting() const
{
    return false;
}
/* virtual */ bool NullConsole::Echo() const
{
    return false;
}
