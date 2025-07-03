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

//! @file    outcwrap.cpp
//! @brief   Implement OutputConsoleWrapper

#include "core/include/outcwrap.h"
#include "core/include/massert.h"

//-----------------------------------------------------------------------------
//! \brief Constructor
OutputConsoleWrapper::OutputConsoleWrapper() :
    m_pWrappedConsole(NULL)
{
}
//-----------------------------------------------------------------------------
//! \brief Destructor
//!
OutputConsoleWrapper::~OutputConsoleWrapper()
{
    m_pWrappedConsole = NULL;
}
/* virtual */ RC OutputConsoleWrapper::Enable(ConsoleBuffer *pConsoleBuffer)
{
    return m_pWrappedConsole->Enable(pConsoleBuffer);
}
/* virtual */ RC OutputConsoleWrapper::Disable()
{
    return m_pWrappedConsole->Disable();
}
/* virtual */ RC OutputConsoleWrapper::Suspend()
{
    return m_pWrappedConsole->Suspend();
}
/* virtual */ bool OutputConsoleWrapper::IsEnabled() const
{
    return m_pWrappedConsole->IsEnabled();
}
/* virtual */ RC OutputConsoleWrapper::PopUp()
{
    return m_pWrappedConsole->PopUp();
}
/* virtual */ RC OutputConsoleWrapper::PopDown()
{
    return m_pWrappedConsole->PopDown();
}
/* virtual */ bool OutputConsoleWrapper::IsPoppedUp() const
{
    return m_pWrappedConsole->IsPoppedUp();
}
/* virtual */ RC OutputConsoleWrapper::SetDisplayDevice(GpuDevice *pDevice)
{
    return m_pWrappedConsole->SetDisplayDevice(pDevice);
}
/* virtual */ GpuDevice * OutputConsoleWrapper::GetDisplayDevice()
{
    return m_pWrappedConsole->GetDisplayDevice();
}
/* virtual */ void OutputConsoleWrapper::AllowYield(bool bAllowYield)
{
    m_pWrappedConsole->AllowYield(bAllowYield);
}
/* virtual */ UINT32 OutputConsoleWrapper::GetScreenColumns() const
{
    return m_pWrappedConsole->GetScreenColumns();
}
/* virtual */  UINT32 OutputConsoleWrapper::GetScreenRows() const
{
    return m_pWrappedConsole->GetScreenRows();
}
/* virtual */  void OutputConsoleWrapper::GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const
{
    m_pWrappedConsole->GetLwrsorPosition(pRow, pColumn);
}
/* virtual */  void OutputConsoleWrapper::SetLwrsorPosition(INT32 row, INT32 column)
{
    m_pWrappedConsole->SetLwrsorPosition(row, column);
}
/* virtual */  void OutputConsoleWrapper::ClearTextScreen()
{
    m_pWrappedConsole->ClearTextScreen();
}
/* virtual */  RC OutputConsoleWrapper::SetLwrsor(Console::Cursor Type)
{
    return m_pWrappedConsole->SetLwrsor(Type);
}
/* virtual */  RC OutputConsoleWrapper::ScrollScreen(INT32 lines)
{
    return m_pWrappedConsole->ScrollScreen(lines);
}
/* virtual */  void OutputConsoleWrapper::PrintStringToScreen
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    m_pWrappedConsole->PrintStringToScreen(str, strLen, state);
}
/* virtual */ Console::VirtualKey OutputConsoleWrapper::GetKey()
{
    MASSERT(!"GetKey() not supported on output only consoles");
    return Console::VKC_NONE;
}
/* virtual */ RC OutputConsoleWrapper::GetString(string *pString)
{
    MASSERT(!"GetString() not supported on output only consoles");
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ bool OutputConsoleWrapper::KeyboardHit()
{
    MASSERT(!"KeyboardHit() not supported on output only consoles");
    return false;
}
/* virtual */ bool OutputConsoleWrapper::Formatting() const
{
    return m_pWrappedConsole->Formatting();
}
/* virtual */ bool OutputConsoleWrapper::Echo() const
{
    return m_pWrappedConsole->Echo();
}
