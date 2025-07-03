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

//! @file    modscnsl.cpp
//! @brief   Implement ModsConsole
//!
//! For detailed comments on each of the public interfaces, see console.h

#include "modscnsl.h"
#include "core/include/color.h"
#include "gpu/include/dispmgr.h"
#include "core/include/display.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "gpuutils.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/evntthrd.h"
#include "core/include/cnslbuf.h"
#include "core/include/memoryif.h"
#include "core/include/cnslmgr.h"
#include "core/include/cpu.h"

#include "class/clc370.h"

#include <algorithm>
#include <ctype.h>

//! Width of a character in pixels
/* static */ const UINT32 ModsConsole::s_CharHeight = 8;

//! Height of a character in pixels
/* static */ const UINT32 ModsConsole::s_CharWidth  = 8;

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
ModsConsole::ModsConsole()
:   m_Width(0),
    m_Height(0),
    m_Refresh(0),
    m_ColorFormat(ColorUtils::LWFMT_NONE),
    m_ScrollSize(500),
    m_CharWidth(s_CharWidth),
    m_CharHeight(s_CharHeight),
    m_CharScale(1),
    m_JsWidth(0),
    m_JsHeight(0),
    m_JsRefresh(0),
    m_JsScrollSize(500),
    m_JsDevInst(Gpu::MaxNumDevices),
    m_pGpuDevice(nullptr),
    m_bInitialized(false),
    m_bEnabled(false),
    m_bStolen(false),
    m_bAllowYield(true),
    m_LwrsorType(Console::LWRSOR_NORMAL),
    m_bUpdatingConsole(false),
    m_bEnablePrints(true),
    m_bHeaderPrinted(false),
    m_pTestContext(nullptr),
    m_pUpdateDisplayThread(nullptr),
    m_pUpdateDisplayMutex(nullptr),
    m_pLutSettings(nullptr),
    m_pConsoleBuffer(nullptr),
    m_bUserInterfaceDisabled(false)
{
    ResetDisplayVars();

    // Each LUT entry consists of 2 32bit values.  The first 32bit values
    // contains the green (31:16) and red (15:0) components, while the
    // second 32bit value contains the blue (15:0) component.  There are
    // 257 possible entries in the LUT.  The value of each component
    // is callwlated by ((8-bit color value) << 6) and then placing it in
    // the appropriate position within the correct 32bit value
    m_ConsoleLut.push_back(0);            // Black    (G, R)
    m_ConsoleLut.push_back(0);            // Black    (B)
    m_ConsoleLut.push_back(0x20002000);   // Grey     (G, R)
    m_ConsoleLut.push_back(0x2000);       // Grey     (B)
    m_ConsoleLut.push_back(0x3ff83ff8);   // White    (G, R)
    m_ConsoleLut.push_back(0x3ff8);       // White    (B)
    m_ConsoleLut.push_back(0x0);          // Blue     (G, R)
    m_ConsoleLut.push_back(0x3ff8);       // Blue     (B)
    m_ConsoleLut.push_back(0x3ff80000);   // Green    (G, R)
    m_ConsoleLut.push_back(0x0);          // Green    (B)
    m_ConsoleLut.push_back(0x3ff8);       // Red      (G, R)
    m_ConsoleLut.push_back(0x0);          // Red      (B)
    m_ConsoleLut.push_back(0x3ff83ff8);   // Yellow   (G, R)
    m_ConsoleLut.push_back(0x0);          // Yellow   (B)
    m_ConsoleLut.push_back(0x3ff80000);   // Cyan     (G, R)
    m_ConsoleLut.push_back(0x3ff8);       // Cyan     (B)
    m_ConsoleLut.push_back(0x3ff8);       // Magenta  (G, R)
    m_ConsoleLut.push_back(0x3ff8);       // Magenta  (B)
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
ModsConsole::~ModsConsole()
{
    ModsConsole::Disable();
}
//-----------------------------------------------------------------------------
/* virtual */ RC ModsConsole::Enable(ConsoleBuffer *pConsoleBuffer)
{
    MASSERT(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pConsoleBuffer != NULL);

    RC                   rc;
    GpuDevMgr *pDevMgr = static_cast<GpuDevMgr *>(DevMgrMgr::d_GraphDevMgr);

    // Capture the console device from JS if it has been specified
    if ((m_JsDevInst != Gpu::MaxNumDevices) &&
        ((m_pGpuDevice == nullptr) ||
         (m_pGpuDevice->GetDeviceInst() != m_JsDevInst)))
    {
        Device *pDevice;
        CHECK_RC(pDevMgr->GetDevice(m_JsDevInst, &pDevice));
        m_pGpuDevice = static_cast<GpuDevice *>(pDevice);
    }
    else if (m_pGpuDevice == nullptr)
    {
        m_pGpuDevice = pDevMgr->GetPrimaryDevice();

        // A valid lwpu GPU may or may not be the primary subdevice (and even
        // if it is it may not support the MODS console).  If the device
        // containing the primary subdevice can be used for the MODS console
        // then use it, otherwise default to the first device that actually
        // supports the MODS console
        if (!m_pGpuDevice ||
            !m_pGpuDevice->GetSubdevice(0)->HasFeature(
                                        Device::GPUSUB_SUPPORTS_MODS_CONSOLE))
        {
            // If the console device is not specified, find the first device
            // that supports the MODS console
            m_pGpuDevice = pDevMgr->GetFirstGpuDevice();
            while (m_pGpuDevice &&
                   !m_pGpuDevice->GetSubdevice(0)->HasFeature(
                                        Device::GPUSUB_SUPPORTS_MODS_CONSOLE))
            {
                m_pGpuDevice = pDevMgr->GetNextGpuDevice(m_pGpuDevice);
            }
        }
    }

    // Ensure that the GPU supports the MODS console before enabling
    if (!m_pGpuDevice ||
        !m_pGpuDevice->GetSubdevice(0)->HasFeature(
                                    Device::GPUSUB_SUPPORTS_MODS_CONSOLE))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // GPU with null display (e.g. display-less) doesn't support MODS console
    if (m_pGpuDevice->GetDisplay()->GetDisplayClassFamily() ==
            Display::NULLDISP)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_pConsoleBuffer = pConsoleBuffer;
    m_pTestContext =
        new DisplayMgr::TestContext(m_pGpuDevice->GetDisplayMgr());

    m_pTestContext->SetContent(DisplayMgr::CONTENT_CONSOLE);

    CHECK_RC(m_pTestContext->AllocDisplay(DisplayMgr::RealDisplayOnIdle));

    Display *pDisplay = m_pTestContext->GetDisplay();

    // If the MODS console was unable to get the real display, then flag the
    // display as "stolen" so that when the MODS console gets the real display
    // when it becomes available, the remainder of initialization is performed
    if ((!m_pTestContext->IsCompositionEnabled()) &&
        (pDisplay == m_pGpuDevice->GetDisplayMgr()->GetNullDisplay()))
    {
        m_bEnabled = true;
        m_bStolen = true;
        m_bInitialized = false;
        m_pTestContext->UnlockDisplay(DisplayStolenCB,
                                      DisplayRestoredCB,
                                      this);
        return rc;
    }

    m_bStolen = false;
    CHECK_RC(Initialize());
    CHECK_RC(Show());
    m_bEnabled = true;

    // Display position updates are performed by a thread (since it is not
    // possible to update the display when printing from an interrupt
    Tasker::SetEvent(m_pUpdateDisplayThread->GetEvent());

    // Print a nice fancy header for the MODS Console to let the user know how
    // to interact
    PrintHeader();

    // Allow other tests to "steal" the real display from the MODS console
    m_pTestContext->UnlockDisplay(DisplayStolenCB,
                                  DisplayRestoredCB,
                                  this);
    return rc;
}
//-----------------------------------------------------------------------------
/* virtual */ RC ModsConsole::Disable()
{
    return DisableModsConsole(false);
}
//-----------------------------------------------------------------------------
/* virtual */ RC ModsConsole::Suspend()
{
    return DisableModsConsole(true);
}
//-----------------------------------------------------------------------------
//! \brief Private function to disable MODS consoles
//!
RC ModsConsole::DisableModsConsole(bool bResume)
{
    RC rc;

    if (m_bEnabled)
    {
        m_bEnabled = false;

        if (!m_bStolen)
        {
            CHECK_RC(Hide());
        }
    }

    if (m_pUpdateDisplayThread != nullptr)
    {
        m_pUpdateDisplayThread->SetHandler(nullptr);
        delete m_pUpdateDisplayThread;
        m_pUpdateDisplayThread = nullptr;
    }

    // Initialized must be set to false before freeing the surface, otherwise
    // when mods is run with verbose printing and the surface is freed, the
    // RM prints generated after the surface is unmapped will cause
    // segmentation faults
    m_bInitialized = false;

    ResetDisplayVars();

    m_bStolen = false;
    m_bHeaderPrinted = false;
    m_LwrsorType = Console::LWRSOR_NORMAL;
    m_bAllowYield = true;
    m_bUpdatingConsole = false;
    if (!bResume)
        m_pGpuDevice = nullptr;

    delete m_pTestContext;
    m_pTestContext = nullptr;

    m_ConsoleSurf.Free();

#if !defined(TEGRA_MODS)
    delete m_pLutSettings; // Has to be used as a pointer as Android doesn't
                           // compile the LwDisplay code
    m_pLutSettings = nullptr;
#endif

    if (m_bUserInterfaceDisabled)
    {
        CHECK_RC(Platform::EnableUserInterface());
        m_bUserInterfaceDisabled = false;
    }
    if (m_pUpdateDisplayMutex != nullptr)
    {
        Tasker::FreeMutex(m_pUpdateDisplayMutex);
        m_pUpdateDisplayMutex = nullptr;
    }
    if (m_PrintLock)
    {
        Tasker::FreeMutex(m_PrintLock);
        m_PrintLock = nullptr;
    }

    return rc;
}
//-----------------------------------------------------------------------------
/* virtual */ bool ModsConsole::IsEnabled() const
{
    return (m_bEnabled && !m_bStolen);
}
//-----------------------------------------------------------------------------
/* virtual */ RC ModsConsole::PopUp()
{
    // This API is a placehoder for the ability to "pop-up" the mods console
    // in the middle of a test.  For instance, when a golden value miscompares
    // the pop-up feature could be used to take over the real display and show
    // the mods console and allow user interaction to debug the miscompare.
    //
    // When the console is popped-up, it would be necessary to save the current
    // display state (because the console will appropriate the display) and
    // then restore the original display state when the console is popped-
    // down
    return RC::UNSUPPORTED_FUNCTION;
}
//-----------------------------------------------------------------------------
/* virtual */ RC ModsConsole::PopDown()
{
    return RC::UNSUPPORTED_FUNCTION;
}
//-----------------------------------------------------------------------------
/* virtual */ bool ModsConsole::IsPoppedUp() const
{
    return false;
}
//-----------------------------------------------------------------------------
/* virtual */ RC ModsConsole::SetDisplayDevice(GpuDevice *pDevice)
{
    RC rc;
    GpuDevice *pGpuDevice = pDevice;
    if (!pGpuDevice->GetSubdevice(0)->HasFeature(
                                    Device::GPUSUB_SUPPORTS_MODS_CONSOLE))
    {
        Printf(Tee::PriHigh, "Device %d does not support the ModsConsole\n",
               pGpuDevice->GetDeviceInst());
        return RC::UNSUPPORTED_FUNCTION;
    }
    CHECK_RC(Disable());
    m_pGpuDevice = pGpuDevice;
    m_JsDevInst = m_pGpuDevice->GetDeviceInst();
    m_bHeaderPrinted = true;
    return Enable(m_pConsoleBuffer);
}
//-----------------------------------------------------------------------------
/* virtual */ void ModsConsole::AllowYield(bool bAllowYield)
{
    if (bAllowYield && !m_bAllowYield)
    {
        UINT32 lwrX = m_LwrX;
        UINT32 lwrY = m_LwrY;

        // The Mods Console can yield for console updates, reset the console
        // state, clear the display and repaint the history
        m_bAllowYield = true;
        ResetDisplayVars();
        m_pUpdateDisplayThread->SetHandler(DoUpdateDisplayPosition, this);
        UpdateDisplayYielding();
        SetLwrsorPosition(lwrY, lwrX);
    }
    else if (!bAllowYield && m_bAllowYield)
    {
        UINT32 *pAddress;

        m_bAllowYield = false;
        m_pUpdateDisplayThread->SetHandler(nullptr);

        // The draw start row cannot be updated when not yielding so it will
        // remain at whatever spot it is lwrrently displaying
        m_DrawStartRow = m_DisplayStartRow;

        m_bIncrementValidRow = false;
        m_bDuplicateWrites = false;

        // When not yielding, the display start row indicates where within the
        // console buffer text is being displayed.  I.e. the first line of text
        // on the screen is at index m_DisplayStartRow within m_pConsoleBuffer
        m_DisplayStartRow = 0;
        m_PendingDisplayStartRow = 0;
        if (m_pConsoleBuffer->ValidLines() > GetScreenRows())
        {
            m_DisplayStartRow = m_pConsoleBuffer->ValidLines() -
                                GetScreenRows();
        }

        // Clear the current screen before redraw
        pAddress = static_cast<UINT32 *>(m_ConsoleSurf.GetAddress()) +
                             (m_ConsoleSurf.GetPitch() *
                              m_DrawStartRow * m_CharHeight / 4);
        Memory::FillConstant(pAddress,
                             0,
                             m_Width / 4, // Doing a 32bit fill on 8bit surface
                             m_Height,
                             32,
                             m_ConsoleSurf.GetPitch());

        // Redraw the entire screen
        UpdateDisplayNonYielding();
        static const char cannotYield[] =
            "MODS Console cannot yield!!! Scrolling will be SLOW!!\n";
        PrintStringToScreen(cannotYield, sizeof(cannotYield) - 1, Tee::SPS_WARNING);
    }
}
//-----------------------------------------------------------------------------
/* virtual */ UINT32 ModsConsole::GetScreenColumns() const
{
    return (m_Width / m_CharWidth);
}
//-----------------------------------------------------------------------------
/* virtual */ UINT32 ModsConsole::GetScreenRows() const
{
    return (m_Height / m_CharHeight);
}
//-----------------------------------------------------------------------------
/* virtual */ void ModsConsole::GetLwrsorPosition
(
    INT32 * pRow,
    INT32 * pColumn
) const
{
    if (pRow)
        *pRow = m_LwrY;
    if (pColumn)
        *pColumn = m_LwrX;
}
//-----------------------------------------------------------------------------
/* virtual */ void ModsConsole::SetLwrsorPosition(INT32 row, INT32 column)
{
    UINT32 bufferRow = 0;

    // The cursor set point is always referenced from the bottom of the buffer
    // less the number of screen rows (or from zero if there aren't enough rows
    // yet in the buffer
    if (m_pConsoleBuffer->ValidLines() > GetScreenRows())
    {
        bufferRow = m_pConsoleBuffer->ValidLines() -
                            GetScreenRows();
    }
    m_pConsoleBuffer->SetWritePosition(bufferRow + row, column);
    m_LwrX = column;
    m_LwrY = row;

    // If the cursor position changes, then it may have changed to a point
    // that requires duplicating any further writes
    SetDuplicateWrites();
}
/* virtual */ RC ModsConsole::SetLwrsor(Console::Cursor Type)
{
    m_LwrsorType = Type;
    return UpdateLwrsor();
}
/* virtual */ void ModsConsole::ClearTextScreen()
{
    if (!m_bEnabled)
        return;

    string clearText(GetScreenRows(), '\n');
    if (m_bAllowYield)
    {
        // Print a screens worth of blank lines and move the cursor back to
        // the origin of the screen (create a blank screen without losing any
        // history) - necessary in yielding mode to handle wrapping within the
        // console surface
        PrintStringToScreen(clearText.data(), clearText.size(), Tee::SPS_NORMAL);
    }
    else
    {
        UINT32 dirtyRow;
        m_pConsoleBuffer->AddString(clearText.data(), clearText.size(), &dirtyRow);
        // Clear the current screen before redraw
        UINT32 *pAddress = static_cast<UINT32 *>(m_ConsoleSurf.GetAddress()) +
                                    (m_ConsoleSurf.GetPitch() *
                                     m_DrawStartRow * m_CharHeight / 4);
        Memory::FillConstant(pAddress,
                             0,
                             m_Width / 4, // Doing a 32bit fill on 8bit surface
                             m_Height,
                             32,
                             m_ConsoleSurf.GetPitch());
    }
    SetLwrsorPosition(0,0);
}
//-----------------------------------------------------------------------------
/* virtual */ RC ModsConsole::ScrollScreen(INT32 lines)
{
    if (m_bAllowYield)
    {
        return ScrollScreenYielding(lines);
    }
    else
    {
        return ScrollScreenNonYielding(lines);
    }
}
//-----------------------------------------------------------------------------
/* virtual */ void ModsConsole::PrintStringToScreen
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    UINT32 LinesToUpdate;
    UINT32 StartUpdateLine;

    if (!m_bInitialized)
        return;

    if (!m_bEnablePrints)
        return;

    Tasker::MutexHolder lock(m_PrintLock);

    m_pConsoleBuffer->SetCharAttributes(state);

    // Add the string to the buffer.  This takes care of creating all necessary
    // lines (since a single string can create multiple lines) and adding them
    // to the buffer.  The number of lines requiring an update as well as the
    // starting line within the console buffer to begin updating are returned
    LinesToUpdate = m_pConsoleBuffer->AddString(str, strLen, &StartUpdateLine);

    m_LwrX = 0;
    for (UINT32 lwrLine = 0; lwrLine < LinesToUpdate; lwrLine++)
    {
        WriteLine(StartUpdateLine + lwrLine);

        if (lwrLine != (LinesToUpdate - 1))
        {
            // Erase the cursor if it is present before continuing to the
            // next line
            if (m_LwrX < GetScreenRows())
            {
                GpuUtility::PutChar(m_LwrX * m_CharWidth,
                                    (m_LwrY + m_DrawStartRow) * m_CharHeight,
                                    ' ',
                                    m_CharScale,
                                    LUT_BLACK,
                                    LUT_BLACK,
                                    false,
                                    &m_ConsoleSurf);
            }
            NextLine();
        }
    }
    m_pConsoleBuffer->GetWritePosition(nullptr, &m_LwrX);
}
//-----------------------------------------------------------------------------
/* virtual */ bool ModsConsole::Formatting() const
{
    return true;
}
//-----------------------------------------------------------------------------
/* virtual */ bool ModsConsole::Echo() const
{
    return true;
}

//-----------------------------------------------------------------------------
void ModsConsole::Pause(bool bPause)
{
    INT32 prevPauseCount;
    if (bPause)
    {
        prevPauseCount = m_PauseCount++;
    }
    else
    {
        MASSERT(m_PauseCount.load());
        prevPauseCount = m_PauseCount--;
    }

    if (bPause && (prevPauseCount == 0))
    {
        Tasker::AcquireMutex(m_PrintLock);
    }
    else if (!bPause && (prevPauseCount == 1))
    {
        Tasker::ReleaseMutex(m_PrintLock);
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function that resets the display/drawing variables so that
//! drawing/display occur at the top of the display surface
//!
void ModsConsole::ResetDisplayVars()
{
    m_LwrX = 0;
    m_LwrY = 0;
    m_bDuplicateWrites = false;
    m_bIncrementValidRow = false;
    m_ValidStartRow = 0;
    m_DrawStartRow = 0;
    m_DisplayStartRow = 0;
    m_PendingDisplayStartRow = 0;
}

//-----------------------------------------------------------------------------
//! \brief Private initialization function for the MODS console (allocate
//! surface for display based on the current configuration)
//!
//! \return OK if successful, not OK otherwise
RC ModsConsole::Initialize()
{
    UINT32               Width;
    UINT32               Height;
    UINT32               Refresh;
    bool                 bBufferResized = false;
    RC                   rc;
    bool                 compositionEnabled = m_pTestContext->IsCompositionEnabled();
    Display *pDisplay = compositionEnabled ?
        m_pGpuDevice->GetDisplayMgr()->GetRealDisplay() :
        m_pTestContext->GetDisplay();

    if (m_bInitialized)
        return OK;

    m_PrintLock = Tasker::AllocMutex("ModsConsole::m_PrintLock", Tasker::mtxUnchecked);

    m_pUpdateDisplayMutex = Tasker::AllocMutex(
        "ModsConsole::m_pUpdateDisplayMutex",
        Tasker::mtxUnchecked);

    CHECK_RC(Platform::DisableUserInterface());
    m_bUserInterfaceDisabled = true;

    // Capture the Js set values for the console.  When the console changes
    // display devices, it is possible the native resolution will change.
    // If this oclwrs (and the resolution has not been set from JS) then
    // the native resolution of the new display should be used
    m_Width = m_JsWidth;
    m_Height = m_JsHeight;
    m_Refresh = m_JsRefresh;
    m_ScrollSize = m_JsScrollSize;

    // When we're running on CheetAh's built-in panel:
    // - adjust character scaling
    // - use native resolution
    if (m_pGpuDevice->GetSubdevice(0)->IsSOC())
    {
        Display::Encoder encoder;
        CHECK_RC(pDisplay->GetEncoder(pDisplay->Selected(), &encoder));
        if (encoder.Signal == Display::Encoder::DSI ||
            encoder.Signal == Display::Encoder::LVDS)
        {
#if defined(TEGRA_MODS)
            Printf(Tee::PriLow, "Detected CheetAh built-in panel, forcing "
                                "MODS console dimensions to fit the screen\n");
            m_CharScale  = 2;
            m_CharWidth  = s_CharWidth  * m_CharScale;
            m_CharHeight = s_CharHeight * m_CharScale;
            m_Width      = 0;
            m_Height     = 0;
            m_Refresh    = 0;
#endif
        }
    }

    // Use the native resolution values for values that remain unconfigured
    // and override display dimensions for foreign display

    UINT32 displayID;
    if (compositionEnabled)
    {
        displayID = m_pTestContext->GetPrimaryDisplayID();
    }
    else
    {
        displayID = pDisplay->Selected();
    }

    if (!compositionEnabled || m_pTestContext->IsCompositionActive())
    {
        CHECK_RC(pDisplay->GetNativeResolution(displayID,
                                           &Width, &Height, &Refresh));
        bool IsPossible = false;
        CHECK_RC(pDisplay->IsModePossible(displayID, Width, Height,
                                      ColorUtils::PixelBits(ColorUtils::I8),
                                      Refresh, &IsPossible));
        if (!IsPossible)
        {
            CHECK_RC(pDisplay->GetMaxResolution(displayID,
                                            &Width, &Height, &Refresh));
        }
    }

    const bool foreignDisp = pDisplay->GetDisplayClassFamily() == Display::FOREIGNDISP;
    if ((m_Width == 0) || foreignDisp)
        m_Width = Width;
    if ((m_Height == 0) || foreignDisp)
        m_Height = Height;
    if (m_Refresh == 0)
        m_Refresh = Refresh;

    if ((m_pConsoleBuffer->Width() < GetScreenColumns()) ||
        (m_pConsoleBuffer->LongestLine() > GetScreenColumns()) ||
        (m_pConsoleBuffer->Depth() < (GetScreenRows() + m_ScrollSize)))

    {
        // Ensure that the console buffer is the correct size for buffering the
        // MODS console (note that if this changes the console buffer size then
        // all history is lost)
        m_pConsoleBuffer->Resize(GetScreenColumns(),
                                 GetScreenRows() + m_ScrollSize);
        bBufferResized = true;
    }
    m_pConsoleBuffer->SetConsoleWidth(GetScreenColumns());
    m_LineBuffer.reserve(GetScreenColumns());

    // Update the scroll size in case the console buffer has more scroll size
    // than is required by the Mods Console
    m_ScrollSize = m_pConsoleBuffer->Depth() - GetScreenRows();

    // Choose color format
    if (foreignDisp)
    {
        m_ColorFormat = ColorUtils::A8R8G8B8;
    }
    else
    {
        m_ColorFormat = ColorUtils::I8;
    }

    if (!compositionEnabled)
    {
        // Set the Mode that the MODS console will be using (mainly to allocate
        // the display channels)
        CHECK_RC(pDisplay->SetMode(pDisplay->Selected(),
                               m_Width,
                               m_Height,
                               ColorUtils::PixelBits(m_ColorFormat),
                               m_Refresh,
                               Display::FilterTapsNone,
                               Display::ColorCompressionNone,
                               2));
    }

    // Setup the surface for displaying the MODS console.  See modscnsl.h for
    // a detailed description on how the MODS console manages its surface
    m_ConsoleSurf.SetWidth(m_Width);
    m_ConsoleSurf.SetHeight(m_Height * 2 + m_ScrollSize * m_CharHeight);
    UINT32 adjustedPitch = 0;
    CHECK_RC(m_pTestContext->AdjustPitchForScanout(
        m_Width * ColorUtils::PixelBytes(m_ColorFormat), &adjustedPitch));
    m_ConsoleSurf.SetPitch(adjustedPitch);
    m_ConsoleSurf.SetColorFormat(m_ColorFormat);
    m_ConsoleSurf.SetLocation(Memory::Fb);
    m_ConsoleSurf.SetDisplayable (true);
    m_ConsoleSurf.SetLayout(Surface2D::Pitch);
    CHECK_RC(m_ConsoleSurf.Alloc(m_pGpuDevice));
    CHECK_RC(m_ConsoleSurf.Map(pDisplay->GetMasterSubdevice()));
    if (Platform::GetSimulationMode() == Platform::Hardware) // To save sim time
    {
        CHECK_RC(m_ConsoleSurf.Fill(LUT_BLACK, pDisplay->GetMasterSubdevice()));
    }

    CHECK_RC(pDisplay->BindToPrimaryChannels(&m_ConsoleSurf));

    // Setup the display descriptor that MODS will use for the surface
    m_DisplayDesc = { };
    m_DisplayDesc.hMem           = m_ConsoleSurf.GetMemHandle();
    m_DisplayDesc.CtxDMAHandle   = m_ConsoleSurf.GetCtxDmaHandleIso();
    m_DisplayDesc.Offset         = m_ConsoleSurf.GetCtxDmaOffsetIso() +
                                   m_ConsoleSurf.GetPitch() *
                                   (m_DisplayStartRow * m_CharHeight);
    if (foreignDisp)
        m_DisplayDesc.Height     = m_ConsoleSurf.GetHeight();
    else
        m_DisplayDesc.Height     = m_Height;
    m_DisplayDesc.Width          = m_ConsoleSurf.GetWidth();
    m_DisplayDesc.AASamples      = m_ConsoleSurf.GetAASamples();
    m_DisplayDesc.Layout         = m_ConsoleSurf.GetLayout();
    m_DisplayDesc.Pitch          = m_ConsoleSurf.GetPitch();
    m_DisplayDesc.LogBlockHeight = m_ConsoleSurf.GetLogBlockHeight();
    m_DisplayDesc.NumBlocksWidth = m_ConsoleSurf.GetBlockWidth();
    m_DisplayDesc.ColorFormat    = m_ConsoleSurf.GetColorFormat();

    // Startup the display position updating thread
    m_pUpdateDisplayThread = new EventThread(Tasker::MIN_STACK_SIZE,
                                        "ModsConsole Display Update Thread");
    m_pUpdateDisplayThread->SetHandler(DoUpdateDisplayPosition, this);

    // Repaint the display
    UpdateDisplayYielding();

    if (bBufferResized)
    {
        static const char bufResized[] =
            "Console buffer was resized, screen history was reset!!\n";
        PrintStringToScreen(bufResized, sizeof(bufResized) - 1, Tee::SPS_WARNING);
    }

    m_bInitialized = true;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function that displays the MODS console
//!
//! \return OK if successful, not OK otherwise
RC ModsConsole::Show()
{
    RC rc;

    // Disallow relwrsive show
    if (m_bUpdatingConsole)
        return OK;

    // Create an RAII class that sets the updating flag
    UpdatingConsole updateConsole(this, false);

    m_DisplayStartRow = m_PendingDisplayStartRow;

    if (m_pTestContext->IsCompositionEnabled())
    {
#if defined(TEGRA_MODS)
        return RC::SOFTWARE_ERROR;
#else
        LwDispWindowSettings *windowSettings =
            m_pTestContext->GetWindowSettings();
        LwDisplay *pLwDisplay =
            m_pGpuDevice->GetDisplayMgr()->GetRealDisplay()->GetLwDisplay();
        MASSERT(windowSettings);
        MASSERT(pLwDisplay);

        if (m_ColorFormat == ColorUtils::I8)
        {
            if (m_pLutSettings == nullptr)
                m_pLutSettings = new LwDispLutSettings;

            if (pLwDisplay->GetClass() <= LWC370_DISPLAY && m_pLutSettings->pLutBuffer == nullptr)
            {
                CHECK_RC(pLwDisplay->SetupChannelLUTBuffer(
                    m_pLutSettings,
                    &m_ConsoleLut[0],
                    static_cast<UINT32>(m_ConsoleLut.size()),
                    Display::WINDOW_CHANNEL_ID,
                    windowSettings->winIndex));
            }

            windowSettings->pInputLutSettings = m_pLutSettings;
            EotfLutGenerator::EotfLwrves eotfLwrve =
                EotfLutGenerator::EotfLwrves::EOTF_LWRVE_I8_MODS_CONSOLE;
            CHECK_RC(pLwDisplay->SetupILutLwrve(windowSettings, eotfLwrve));

            windowSettings->pInputLutSettings->Disp30.SetControlOutputLut(
                LwDispLutSettings::Disp30Settings::INTERPOLATE_DISABLE,
                LwDispLutSettings::Disp30Settings::MIRROR_DISABLE,
                LwDispLutSettings::Disp30Settings::MODE_DIRECT8);
        }

        windowSettings->image.pkd.pWindowImage = &m_ConsoleSurf;

        // Call SetWindowImage directly as we need to setup that the viewed window
        // is smaller than the total window surface/size:
        CHECK_RC(pLwDisplay->SetupChannelImage(
            m_pTestContext->GetPrimaryDisplayID(), windowSettings));

        windowSettings->rectIn.dirty = true;
        windowSettings->rectIn.xPos = 0;
        windowSettings->rectIn.yPos = m_PendingDisplayStartRow * m_CharHeight;
        windowSettings->rectIn.width = m_ConsoleSurf.GetWidth();
        windowSettings->rectIn.height = m_Height;

        windowSettings->rectValidIn.dirty = false;

        windowSettings->sizeOut.dirty = true;
        windowSettings->sizeOut.width = windowSettings->rectIn.width;
        windowSettings->sizeOut.height = windowSettings->rectIn.height;

        if (!m_pTestContext->IsCompositionActive())
            return OK;

        CHECK_RC(pLwDisplay->SetWindowImage(
            windowSettings,
            m_ConsoleSurf.GetWidth(),
            m_ConsoleSurf.GetHeight(),
            LwDisplay::SEND_UPDATE,
            LwDisplay::DONT_WAIT_FOR_NOTIFIER));

        CHECK_RC(m_pTestContext->UpdateWindowPosition());

        return OK;
#endif
    }

    Display *pDisplay = m_pTestContext->GetDisplay();

    UINT32 DisplaySubdevices = (1 << pDisplay->GetMasterSubdevice());

    CHECK_RC(pDisplay->SetDisplaySubdevices(pDisplay->Selected(), DisplaySubdevices));

    CHECK_RC(pDisplay->SetMode(pDisplay->Selected(),
                            m_Width,
                            m_Height,
                            ColorUtils::PixelBits(m_ColorFormat),
                            m_Refresh,
                            Display::FilterTapsNone,
                            Display::ColorCompressionNone,
                            2));
    CHECK_RC(pDisplay->BindToPrimaryChannels(&m_ConsoleSurf));
    CHECK_RC(pDisplay->SetDistributedMode(pDisplay->Selected(), NULL, true));
    if (m_ColorFormat == ColorUtils::I8 &&
        !pDisplay->GetOwningDisplaySubdevice()->HasBug(1854652))
    {
        CHECK_RC(pDisplay->SetBaseLUT(pDisplay->Selected(),
                                      Display::LUTMODE_LORES,
                                      &m_ConsoleLut[0],
                                      static_cast<UINT32>(m_ConsoleLut.size())));
    }
    m_DisplayDesc.Offset = m_ConsoleSurf.GetCtxDmaOffsetIso() +
                           m_ConsoleSurf.GetPitch() *
                           (m_DisplayStartRow * m_CharHeight);
    CHECK_RC(pDisplay->SetImage(pDisplay->Selected(), &m_DisplayDesc));

    CHECK_RC(pDisplay->SendUpdate
    (
        true,       // Core
        0xFFFFFFFF, // All bases
        0xFFFFFFFF, // All lwrsors
        0xFFFFFFFF, // All overlays
        0xFFFFFFFF, // All overlaysIMM
        true,       // Interlocked
        true        // Wait for notifier
    ));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function that hides the MODS console
//!
//! \return OK if successful, not OK otherwise
RC   ModsConsole::Hide()
{
    RC rc;

    // Disallow relwrsive hide
    if (m_bUpdatingConsole)
        return OK;

    UpdatingConsole updateConsole(this, false);

    if (m_pTestContext->IsCompositionEnabled())
    {
#if defined(TEGRA_MODS)
        return RC::SOFTWARE_ERROR;
#else
        return m_pTestContext->DisplayImage(static_cast<Surface2D *>(nullptr),
            DisplayMgr::WAIT_FOR_UPDATE);
#endif
    }

    Display *pDisplay = m_pTestContext->GetDisplay();
    CHECK_RC(pDisplay->SetImage(pDisplay->Selected(),
                                static_cast<GpuUtility::DisplayImageDescription *>(nullptr)));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function that displays the MODS console
//!
//! \return OK if successful, not OK otherwise
void ModsConsole::ClearLine() const
{
    UINT32 *pAddress = static_cast<UINT32 *>(m_ConsoleSurf.GetAddress()) +
                                 (m_ConsoleSurf.GetPitch() / 4 *
                                  (m_LwrY + m_DrawStartRow) *
                                  m_CharHeight);
    UINT32  byteCount = m_ConsoleSurf.GetPitch() * m_CharHeight;

    Platform::MemSet(pAddress, 0, byteCount);

    if (m_bDuplicateWrites)
    {
        UINT32 DupOffset = (m_ConsoleSurf.GetPitch() / 4 *
                            (GetScreenRows() + m_ScrollSize) * m_CharHeight);

        if ((m_LwrY + m_DrawStartRow) < GetScreenRows())
        {
            // When writing below the first screen height in the screen buffer,
            // writes need to be duplicated into the bottom portion of the
            // screen buffer
            pAddress += DupOffset;
        }
        else
        {
            // When writing within the last screen height in the screen buffer,
            // writes need to be duplicated into the top portion of the screen
            // buffer
            pAddress -= DupOffset;
        }
        Platform::MemSet(pAddress, 0, byteCount);
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function to write a line from console buffer onto the screen
//!
//! \param bufferLine is the line within the console buffer to write (at the
//!        current position within the mods console
void ModsConsole::WriteLine(UINT32 bufferLine)
{
    const ConsoleBuffer::BufferLine *pLine;
    UINT32                lwrChar = 0;
    Tee::ScreenPrintState lwrState;
    UINT32                fgColor, bgColor;
    UINT32                writeRow;
    UINT32                lwrLineChar;

    // Supress any printing when writing a line - if an error oclwrs which prints
    // prints, then this function will be called relwrsively (likely in an
    // infinite manner)
    UpdatingConsole updateConsole(this, true);

    pLine = m_pConsoleBuffer->GetLine(bufferLine);
    if (pLine == nullptr)
    {
        MASSERT(!"Invalid console buffer line\n");
        return;
    }
    writeRow = (m_LwrY + m_DrawStartRow) * m_CharHeight;
    UINT32 dupWriteRow = writeRow;
    if (m_bDuplicateWrites)
    {
        UINT32 DupOffset = (GetScreenRows() + m_ScrollSize) * m_CharHeight;

        if ((m_LwrY+m_DrawStartRow) < GetScreenRows())
        {
            dupWriteRow += DupOffset;
        }
        else
        {
            dupWriteRow -= DupOffset;
        }
    }

    // The line ends if the character is zero
    while ((lwrChar < pLine->size()) && ((*pLine)[lwrChar].c != 0))
    {
        // Group all characters on the line of similar state into a single
        // print for efficiency
        m_LineBuffer.resize(GetScreenColumns());
        lwrLineChar = 0;
        lwrState = (*pLine)[lwrChar].state;
        while ((lwrChar < pLine->size()) &&
               (lwrState == (*pLine)[lwrChar].state) &&
               ((*pLine)[lwrChar].c != 0))
        {
            if (isprint((*pLine)[lwrChar].c))
            {
                m_LineBuffer[lwrLineChar++] = (*pLine)[lwrChar].c;
            }
            lwrChar++;
        }
        m_LineBuffer.resize(lwrLineChar);
        GetColor(lwrState, &fgColor, &bgColor);
        if (OK != GpuUtility::PutChars(m_LwrX * m_CharWidth,
                                       writeRow,
                                       m_LineBuffer,
                                       m_CharScale,
                                       fgColor, bgColor,
                                       false,
                                       &m_ConsoleSurf))
        {
            MASSERT(!"Writing characters to the MODS Console failed!!!\n");
        }
        if ((m_bDuplicateWrites) &&
            (OK != GpuUtility::PutChars(m_LwrX * m_CharWidth,
                                        dupWriteRow,
                                        m_LineBuffer,
                                        m_CharScale,
                                        fgColor, bgColor,
                                        false,
                                        &m_ConsoleSurf)))
        {
            MASSERT(!"Writing duplication characters to the MODS Console "
                     "failed!!!\n");
        }
        m_LwrX += lwrLineChar;
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function to move the display of the mods console to the
//! next line
//!
void ModsConsole::NextLine()
{
    UINT32 ScreenRows = GetScreenRows();

    // Move to the start of the line.  If the cursor is not at the bottom of
    // the screen then no scrolling is necessary...simply update the current
    // write position
    m_LwrX = 0;
    if (m_LwrY < (ScreenRows - 1))
    {
        m_LwrY++;
    }
    else if (m_bAllowYield)
    {
        INT32 oldDisplayRow = m_PendingDisplayStartRow;

        // When yielding, moving to the next line ilwolves shiftin the
        // display position within the console surface
        m_DrawStartRow++;
        m_PendingDisplayStartRow++;

        // If the next line would be drawn below the end of the surface, then
        // the both display and drawing need to shift back to the beginning
        // of the surface
        if ((m_DrawStartRow + m_LwrY) == (ScreenRows * 2 + m_ScrollSize))
        {
            m_DrawStartRow = 1;
            m_PendingDisplayStartRow = 1;
        }
        else if ((m_DrawStartRow + m_LwrY) >= (ScreenRows + m_ScrollSize))
        {
            // As soon as drawing reaches the bottom portion of the screen
            // for the first time, the current valid row (which represents
            // the first line in the scrollback history) needs to increment
            // along with the drawing start row
            m_bIncrementValidRow = true;
        }

        // The valid start row needs to wrap as soon as it hits the bottom
        // display screen
        if (m_bIncrementValidRow)
        {
            m_ValidStartRow++;

            if (m_ValidStartRow == static_cast<INT32>(ScreenRows + m_ScrollSize))
            {
                m_ValidStartRow = 0;
            }
        }

        // Update the displayed position if possible and necessary
        if (m_bEnabled && !m_bStolen &&
            (oldDisplayRow != m_PendingDisplayStartRow))
        {
            Tasker::SetEvent(m_pUpdateDisplayThread->GetEvent());
        }
    }
    else
    {
        // No yielding displays should always be displaying the bottom screen
        // rows of the console buffer (if this point is reached).  Update the
        // display to reflect that
        m_DisplayStartRow = 0;
        if (m_pConsoleBuffer->ValidLines() > GetScreenRows())
        {
            m_DisplayStartRow = m_pConsoleBuffer->ValidLines() - GetScreenRows();
        }
        UpdateDisplayNonYielding();
    }

    SetDuplicateWrites();
    ClearLine();
}

//-------------------------------------------------------------------------
//! \brief Update the cursor
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC ModsConsole::UpdateLwrsor()
{
    if (!m_bInitialized)
        return OK;

    UINT32 fgColor, bgColor;

    if (m_LwrY >= GetScreenRows())
    {
        return OK;
    }

    UINT32 row, col;
    char c;
    m_pConsoleBuffer->GetWritePosition(&row, &col);
    const ConsoleBuffer::BufferLine *pRowData = m_pConsoleBuffer->GetLine(row);

    if ((*pRowData)[col].c == 0)
        c = ' ';
    else
        c = (*pRowData)[col].c;

    GetColor(Tee::SPS_NORMAL, &fgColor, &bgColor);
    if (col < GetScreenColumns())
    {
        if (m_LwrsorType == Console::LWRSOR_BLOCK)
        {
            return GpuUtility::PutChar(col * m_CharWidth,
                                   (m_LwrY + m_DrawStartRow) * m_CharHeight,
                                   c,
                                   m_CharScale,      // Scale factor
                                   bgColor, fgColor, // Ilwert FG/BG for block
                                   false,            // No underline
                                   &m_ConsoleSurf);
        }
        else
        {
            return GpuUtility::PutChar(col * m_CharWidth,
                                   (m_LwrY + m_DrawStartRow) * m_CharHeight,
                                   c,
                                   m_CharScale,      // Scale factor
                                   fgColor, bgColor, // Normal FG/BG
                                   (m_LwrsorType == Console::LWRSOR_NORMAL),
                                   &m_ConsoleSurf);
        }
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Private function to update the lwrrently displayed position to the
//! pending display position.  This is called from a seperate thread to avoid
//! updating the display position during an interrupt
//!
//! \return OK if successful, not OK otherwise
RC ModsConsole::UpdateDisplayPosition()
{
    RC rc;

    if (m_bUpdatingConsole)
        return OK;

    Tasker::MutexHolder mh(m_pUpdateDisplayMutex);

    // Turn off printing to the screen when updating the display position
    // to avoid having the display position changing while updating the display
    // position itself
    UpdatingConsole updateConsole(this, true);
    if (m_bEnabled && !m_bStolen &&
        (m_PendingDisplayStartRow != m_DisplayStartRow))
    {
        // Showing the console the first time will always cause prints which will
        // change the display row.  Update the display row and hopefully the
        // update process will not print anything out.  If it does then we cannot
        // keep the display updated to the correct position

        if (m_pTestContext->IsCompositionEnabled())
        {
#if defined(TEGRA_MODS)
            return RC::SOFTWARE_ERROR;
#else
            m_pTestContext->SetWindowPointIn(0, m_PendingDisplayStartRow * m_CharHeight);
#endif
        }
        else
        {
            Display *pDisplay = m_pTestContext->GetDisplay();

            m_DisplayDesc.Offset = m_ConsoleSurf.GetCtxDmaOffsetIso() +
                                   m_ConsoleSurf.GetPitch() *
                                   (m_PendingDisplayStartRow * m_CharHeight);

            CHECK_RC(pDisplay->SetOffsetWithinImage(pDisplay->Selected(),
                                                    m_DisplayDesc.Offset));
        }

        m_DisplayStartRow = m_PendingDisplayStartRow;
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to update the display (i.e. repaint the entire
//! display history) in a yielding fashion.  Display position updates are done
//! via a seperate thread
//!
void ModsConsole::UpdateDisplayYielding()
{
    UINT32 LineCount = m_pConsoleBuffer->ValidLines();
    for (UINT32 lwrLine = 0; lwrLine < LineCount; lwrLine++)
    {
        WriteLine(lwrLine);

        if (lwrLine != (LineCount - 1))
        {
            NextLine();
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function to update the display (i.e. repaint the entire
//! display history) in a non-yielding fashion.  Only the lwrrently displayed
//! region of the console is repainted
//!
void ModsConsole::UpdateDisplayNonYielding()
{
    UINT32 lwrY = m_LwrY;
    UINT32 lwrX = m_LwrX;
    m_LwrX = 0;
    m_LwrY = 0;

    UINT32 LineCount = m_pConsoleBuffer->ValidLines() - m_DisplayStartRow;
    if (LineCount > GetScreenRows())
    {
        LineCount = GetScreenRows();
    }
    ClearLine();
    for (UINT32 lwrLine = 0; lwrLine < LineCount; lwrLine++)
    {
        WriteLine(m_DisplayStartRow + lwrLine);

        if (lwrLine != (LineCount - 1))
        {
            m_LwrY++;
            m_LwrX=0;
            ClearLine();
        }
    }
    m_LwrX = lwrX;
    m_LwrY = lwrY;
}

//-----------------------------------------------------------------------------
//! \brief Private function to scroll the screen in a yielding fashion.  This
//! shifts the region of the console surface lwrrently being displayed via
//! display updates
//!
//! \param lines is the number of lines to scroll the screen by
//!
//! \return OK if successful, not OK otherwise
RC ModsConsole::ScrollScreenYielding(INT32 lines)
{
    INT32 ScreenRows = static_cast<INT32>(GetScreenRows());

    // NOTE : The pending display row is always used in these callwlations
    // so that multiple fast updates always end with the correct display
    // position

    // Scrolling with (lines == 0) means to scroll to the base position (i.e.
    // display the current cursor location
    if (lines == 0)
    {
        m_PendingDisplayStartRow = m_DrawStartRow;
    }
    // The draw start row is less than the valid start row.  This means that
    // there is a full history that wraps within the console surface from
    // top to bottom
    else if (m_DrawStartRow < m_ValidStartRow)
    {
        // The current pending display row is greater than the valid row.
        // This means that scrolling up (lines < 0) will never wrap in the
        // console surface, but scrolling down (lines > 0) may wrap
        if (m_PendingDisplayStartRow >= m_ValidStartRow)
        {
            // Wonderful ASCII Art depicting this case
            //     * = Lines avaliable to scroll down (later in history)
            //     + = Lines available to scroll up (earlier in history)
            //     $ = Draw screen (contains current cursor)
            //
            // |*************|(Top Screen Start, Beginning of Surface)
            // |*************|
            // |*************|(Top Screen End)
            // |*************|
            // |$$$$$$$$$$$$$|(Draw Screen Start, m_DrawStartRow)
            // |$$$$$$$$$$$$$|
            // |$$$$$$$$$$$$$|(Draw Screen End)
            // |+++++++++++++|(1st line below draw screen, m_ValidStartRow)
            // |             |(m_PendingDisplayStartRow)
            // |*************|
            // |*************|
            // |             |(Bottom Screen Start)
            // |             |
            // |-------------|(Bottom Screen End, End of Surface)
            //
            if (lines < 0)
            {
                lines = max(lines, m_ValidStartRow - m_PendingDisplayStartRow);
            }
            else
            {
                lines = min(lines, ScreenRows + static_cast<INT32>(m_ScrollSize) -
                                   m_PendingDisplayStartRow + m_DrawStartRow);
            }
        }
        else if (m_PendingDisplayStartRow <= m_DrawStartRow)
        {
            // Wonderful ASCII Art depicting this case
            //     * = Lines avaliable to scroll down (later in history)
            //     + = Lines available to scroll up (earlier in history)
            //     $ = Draw screen (contains current cursor)
            //
            // |+++++++++++++|(Top Screen Start, Beginning of Surface)
            // |+++++++++++++|
            // |+++++++++++++|(Top Screen End)
            // |             |(m_PendingDisplayStartRow)
            // |*************|
            // |$$$$$$$$$$$$$|(Draw Screen Start, m_DrawStartRow)
            // |$$$$$$$$$$$$$|
            // |$$$$$$$$$$$$$|(Draw Screen End)
            // |+++++++++++++|(1st line below draw screen, m_ValidStartRow)
            // |+++++++++++++|
            // |+++++++++++++|
            // |             |(Bottom Screen Start)
            // |             |
            // |-------------|(Bottom Screen End, End of Surface)
            //
            if (lines < 0)
            {
                lines = max(lines, m_ValidStartRow - ScreenRows -
                                   static_cast<INT32>(m_ScrollSize) - m_PendingDisplayStartRow);
            }
            else
            {
                lines = min(lines, m_DrawStartRow - m_PendingDisplayStartRow);
            }
        }
    }
    // Wonderful ASCII Art depicting the last two cases
    //     * = Lines avaliable to scroll down (later in history)
    //     + = Lines available to scroll up (earlier in history)
    //     $ = Draw screen (contains current cursor)
    //
    // |-------------|(Top Screen Start, Beginning of Surface)
    // |             |
    // |+++++++++++++|(Top Screen End, m_ValidStartRow)
    // |+++++++++++++|
    // |+++++++++++++|
    // |+++++++++++++|
    // |             |(m_PendingDisplayStartRow)
    // |*************|
    // |*************|
    // |*************|
    // |$$$$$$$$$$$$$|(Draw Screen Start, m_DrawStartRow)
    // |$$$$$$$$$$$$$|(Bottom Screen Start)
    // |$$$$$$$$$$$$$|(Draw Screen End)
    // |             |(Bottom Screen End, End of Surface)
    //
    else if (lines < 0)
    {
        lines = max(lines, m_ValidStartRow - m_PendingDisplayStartRow);
    }
    else if (lines > 0)
    {
        lines = min(lines, m_DrawStartRow - m_PendingDisplayStartRow);
    }

    if (lines != 0)
    {
        // Update the lines and wrap if necessary
        m_PendingDisplayStartRow += lines;

        if (m_PendingDisplayStartRow < 0)
        {
            m_PendingDisplayStartRow += (ScreenRows+static_cast<INT32>(m_ScrollSize));
        }
        else if (m_PendingDisplayStartRow  > (ScreenRows+static_cast<INT32>(m_ScrollSize)))
        {
            m_PendingDisplayStartRow -= (ScreenRows+static_cast<INT32>(m_ScrollSize));
        }

        // If somehow we overshot and ended up with a display row in the middle
        // of the display screen then clamp the display row appropriately based
        // on the direction of the scroll
        if ((m_PendingDisplayStartRow > static_cast<INT32>(m_DrawStartRow)) &&
            (m_PendingDisplayStartRow < static_cast<INT32>(m_ValidStartRow)))
        {
            m_PendingDisplayStartRow = (lines > 0) ? m_DrawStartRow :
                                                     m_ValidStartRow;
        }
    }

    // Finally, set the flag to perform the actual display position update
    if (m_bEnabled && !m_bStolen)
    {
        Tasker::SetEvent(m_pUpdateDisplayThread->GetEvent());
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Private function to scroll the screen in a non-yielding fashion.
//! redraws the entire screen from the console buffer based on the display
//! position
//!
//! \param lines is the number of lines to scroll the screen by
//!
//! \return OK if successful, not OK otherwise
RC ModsConsole::ScrollScreenNonYielding(INT32 lines)
{
    INT32 ScreenRows = static_cast<INT32>(GetScreenRows());
    INT32 ValidLines = static_cast<INT32>(m_pConsoleBuffer->ValidLines());
    INT32 oldDisplayRow = m_DisplayStartRow;

    if (lines == 0)
    {
        // Return to the bottom of the console buffer (or zero if there are
        // not enough rows present)
        m_DisplayStartRow = 0;
        if (ValidLines > ScreenRows)
        {
            m_DisplayStartRow = ValidLines - ScreenRows;
        }
    }
    else
    {
        // Increment the display start row, clamping as necessary
        m_DisplayStartRow += lines;
        if ((ScreenRows >= ValidLines) || (m_DisplayStartRow < 0))
        {
            m_DisplayStartRow = 0;
        }
        else if ((m_DisplayStartRow + ScreenRows) > ValidLines)
        {
            m_DisplayStartRow = ValidLines - ScreenRows;
        }
    }

    // Redraw the entire screen based on the new display start row
    if (m_bEnabled && !m_bStolen && (m_DisplayStartRow != oldDisplayRow))
    {
        m_LwrY -= (m_DisplayStartRow - oldDisplayRow);
        UpdateDisplayNonYielding();
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Private function to get the colors to use based on the provied state
//!
//! \param state is print state to get the color for
//! \param pFGColor is a pointer to the returned foreground color
//! \param pBGColor is a pointer to the returned background color
//!
void ModsConsole::GetColor
(
    Tee::ScreenPrintState state,
    UINT32 *pFGColor,
    UINT32 *pBGColor
) const
{
    if (m_ColorFormat == ColorUtils::I8)
    {
        switch (state)
        {
            case Tee::SPS_NORMAL:
                *pFGColor = LUT_GREY;
                *pBGColor = LUT_BLACK;
                break;
            case Tee::SPS_HIGH:
            case Tee::SPS_WARNING:
                *pFGColor = LUT_BLACK;
                *pBGColor = LUT_YELLOW;
                break;
            case Tee::SPS_FAIL:
                *pFGColor = LUT_WHITE;
                *pBGColor = LUT_RED;
                break;
            case Tee::SPS_PASS:
                *pFGColor = LUT_WHITE;
                *pBGColor = LUT_GREEN;
                break;
            case Tee::SPS_BOTH:
            case Tee::SPS_HIGHLIGHT:
                *pFGColor = LUT_BLACK;
                *pBGColor = LUT_CYAN;
                break;
            case Tee::SPS_LOW:
                *pFGColor = LUT_WHITE;
                *pBGColor = LUT_BLUE;
                break;
            default:
                *pFGColor = LUT_WHITE;
                *pBGColor = LUT_MAGENTA;
                break;
        }
    }
    else
    {
        switch (state)
        {
            case Tee::SPS_NORMAL:
                *pFGColor = COL_GREY;
                *pBGColor = COL_BLACK;
                break;
            case Tee::SPS_HIGH:
            case Tee::SPS_WARNING:
                *pFGColor = COL_BLACK;
                *pBGColor = COL_YELLOW;
                break;
            case Tee::SPS_FAIL:
                *pFGColor = COL_WHITE;
                *pBGColor = COL_RED;
                break;
            case Tee::SPS_PASS:
                *pFGColor = COL_WHITE;
                *pBGColor = COL_GREEN;
                break;
            case Tee::SPS_BOTH:
            case Tee::SPS_HIGHLIGHT:
                *pFGColor = COL_BLACK;
                *pBGColor = COL_CYAN;
                break;
            case Tee::SPS_LOW:
                *pFGColor = COL_WHITE;
                *pBGColor = COL_BLUE;
                break;
            default:
                *pFGColor = COL_WHITE;
                *pBGColor = COL_MAGENTA;
                break;
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function to print the mods console header if necessary
//!
void ModsConsole::PrintHeader()
{
    if (!m_bHeaderPrinted)
    {
        string HeaderStr = Utility::StrPrintf(
               "*****************************************************\n"
               "*                    MODS Console                   *\n"
               "*                     %4d x %-4d                   *\n"
               "*               %3d rows x %3d columns              *\n"
               "*             %4d lines of scroll back             *\n"
               "*                                                   *\n"
               "*         Ctrl-UpArrow : Scroll up one line         *\n"
               "*       Ctrl-DownArrow : Scroll down one line       *\n"
               "*            PageUp : Scroll up one page            *\n"
               "*          PageDown : Scroll down one page          *\n"
               "*****************************************************\n\n",
               m_Width, m_Height,
               GetScreenRows(), GetScreenColumns(), m_ScrollSize);
        PrintStringToScreen(HeaderStr.data(), HeaderStr.size(), Tee::SPS_LOW);
        m_bHeaderPrinted = true;
    }
}

//-----------------------------------------------------------------------------
//! \brief Private function to check whether the current write position is
//! within either the top screen or bottom screen region of the console surface
//! and would therefore require duplicate writes
//!
void ModsConsole::SetDuplicateWrites()
{
    if (m_bAllowYield)
    {
        if (((m_DrawStartRow + m_LwrY) >= (GetScreenRows() + m_ScrollSize)) ||
            ((m_DrawStartRow + m_LwrY) < GetScreenRows()))
        {
            m_bDuplicateWrites = true;
        }
        else
        {
            m_bDuplicateWrites = false;
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Private static function called when the display manager steals the
//! real display from the MODS console
//!
//! \param pvArgs is a pointer to the arguments (i.e. pointer to the mods
//!        console)
//!
//! \return OK if succesful, not OK if not successful
/* static */ RC ModsConsole::DisplayStolenCB(void *pvArgs)
{
    ModsConsole *pThis = static_cast<ModsConsole *>(pvArgs);

    if (pThis->m_pTestContext->IsCompositionEnabled())
        return OK;

    RC rc;

    if (!pThis->m_bStolen)
    {
        Tasker::MutexHolder mh(pThis->m_pUpdateDisplayMutex);
        CHECK_RC(pThis->Hide());
        pThis->m_bStolen = true;
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private static function called when the display manager restores the
//! real display to the MODS console
//!
//! \param pvArgs is a pointer to the arguments (i.e. pointer to the mods
//!        console)
//!
//! \return OK if succesful, not OK if not successful
/* static */ RC ModsConsole::DisplayRestoredCB(void *pvArgs)
{
    ModsConsole *pThis = static_cast<ModsConsole *>(pvArgs);
    RC rc;

    if (pThis->m_bStolen)
    {
        if (!pThis->m_bInitialized)
        {
            CHECK_RC(pThis->Initialize());
        }
        CHECK_RC(pThis->Show());
        pThis->m_bStolen = false;
        Tasker::SetEvent(pThis->m_pUpdateDisplayThread->GetEvent());
        pThis->PrintHeader();
        pThis->m_pTestContext->UnlockDisplay(DisplayStolenCB,
                                             DisplayRestoredCB,
                                             pvArgs);
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private static function called from the display position update
//! thread to update the current display position
//!
//! \param pvThis is a pointer to the arguments (i.e. pointer to the mods
//!        console)
//!
/* static */ void ModsConsole::DoUpdateDisplayPosition(void *pvThis)
{
    ModsConsole *pThis = static_cast<ModsConsole *>(pvThis);

    if (pThis->UpdateDisplayPosition() != OK)
    {
        Printf(Tee::PriHigh, "Console scrolling failed\n");
    }
}

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
//! \param pConsole is a pointer to the Mods Console.  If not NULL, then the
//!        the Mods Console is flagged as being in the middle of an update
//! \param bDisableScreenOutput is set to true if screen output should be
//!        disabled for the length of the object
//!
ModsConsole::UpdatingConsole::UpdatingConsole(ModsConsole *pConsole,
                                              bool bDisableScreenOutput)
: m_pConsole(pConsole),
  m_bDisableScreenOutput(bDisableScreenOutput),
  m_bSavedEnablePrints(false),
  m_bSavedUpdatingConsole(false)
{
    if (m_pConsole != nullptr)
    {
        m_bSavedUpdatingConsole = m_pConsole->m_bUpdatingConsole;
        m_pConsole->m_bUpdatingConsole = true;
        if (m_bDisableScreenOutput)
        {
            m_bSavedEnablePrints = m_pConsole->m_bEnablePrints;
            m_pConsole->m_bEnablePrints = false;
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Constructor - Restore the original states of all saved parameters
//!
ModsConsole::UpdatingConsole::~UpdatingConsole()
{
    if (m_pConsole != nullptr)
    {
        m_pConsole->m_bUpdatingConsole = m_bSavedUpdatingConsole;
        if (m_bDisableScreenOutput)
        {
            m_pConsole->m_bEnablePrints = m_bSavedEnablePrints;
        }
    }
}

//-----------------------------------------------------------------------------
//! Create a ModsConsole and register it with ConsoleManager.
//! ConsoleManager owns the pointer so the console stays around during static
//! destruction
static Console *CreateModsConsole() { return new ModsConsole; }
static ConsoleFactory s_ModsConsole(CreateModsConsole, "mods", false);

//! Class for temporarily pausing printing to the MODS console. Used
//! primarily during ECC tests since MODS console writes can cause
//! ECC errors if they happen to occur while checkbits are disabled
//!
//! NOTE : This has *no effect* on DOS platforms since printing can
//! happen from a real interrupt and pausing in the print routine
//! ilwolves yielding
ModsConsole::PauseConsoleUpdates::~PauseConsoleUpdates()
{
    if (m_bPaused)
    {
        ModsConsole *pModsConsole =
            static_cast<ModsConsole *>(ConsoleManager::Instance()->GetModsConsole());
        pModsConsole->Pause(false);
        m_bPaused = false;
    }
}

RC ModsConsole::PauseConsoleUpdates::Pause()
{
    ModsConsole *pModsConsole =
        static_cast<ModsConsole *>(ConsoleManager::Instance()->GetModsConsole());

    if (pModsConsole == nullptr)
        return RC::SOFTWARE_ERROR;

    if (!pModsConsole->IsEnabled())
        return OK;

    m_bPaused = true;
    pModsConsole->Pause(true);
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Javascript class for ModsConsole
//!
JS_CLASS(ModsConsole);

//-----------------------------------------------------------------------------
//! \brief Javascript object for ModsConsole
//!
SObject ModsConsole_Object
(
   "ModsConsole",
   ModsConsoleClass,
   nullptr,
   0,
   "Mods Console Interface."
);

#define MODSCONSOLE_PROP_READWRITE(propName, resulttype, helpmsg)          \
   P_(ModsConsole_Get_##propName);                                         \
   P_(ModsConsole_Set_##propName);                                         \
   P_(ModsConsole_Get_##propName)                                          \
   {                                                                       \
      MASSERT(pContext != 0);                                              \
      MASSERT(pValue   != 0);                                              \
                                                                           \
      ModsConsole *pModsConsole =                                          \
          (ModsConsole *)ConsoleManager::Instance()->GetModsConsole();     \
                                                                           \
      if (pModsConsole == NULL)                                            \
      {                                                                    \
          JS_ReportError(pContext, "Failed to get ModsConsole");           \
          *pValue = JSVAL_NULL;                                            \
          return JS_FALSE;                                                 \
      }                                                                    \
      resulttype result = pModsConsole->Get##propName();                   \
      if (OK != JavaScriptPtr()->ToJsval(result, pValue))                  \
      {                                                                    \
         JS_ReportError(pContext, "Failed to get ModsConsole." #propName );\
         *pValue = JSVAL_NULL;                                             \
         return JS_FALSE;                                                  \
      }                                                                    \
      return JS_TRUE;                                                      \
   }                                                                       \
   P_(ModsConsole_Set_##propName)                                          \
   {                                                                       \
      MASSERT(pContext != 0);                                              \
      MASSERT(pValue   != 0);                                              \
                                                                           \
      resulttype Value;                                                    \
      if (OK != JavaScriptPtr()->FromJsval(*pValue, &Value))               \
      {                                                                    \
         JS_ReportError(pContext, "Failed to set ModsConsole." #propName); \
         return JS_FALSE;                                                  \
      }                                                                    \
      ModsConsole *pModsConsole =                                          \
          (ModsConsole *)ConsoleManager::Instance()->GetModsConsole();     \
                                                                           \
      if (pModsConsole == NULL)                                            \
      {                                                                    \
          JS_ReportError(pContext, "Failed to get ModsConsole");           \
          *pValue = JSVAL_NULL;                                            \
          return JS_FALSE;                                                 \
      }                                                                    \
      if (pModsConsole->IsInitialized())                                   \
      {                                                                    \
          Printf(Tee::PriLow,                                              \
                 "ModsConsole must be disabled and re-enabled for property changes to take effect\n");\
      }                                                                    \
                                                                           \
      pModsConsole->Set##propName(Value);                                  \
      return JS_TRUE;                                                      \
   }                                                                       \
   static SProperty ModsConsole_ ## propName                               \
   (                                                                       \
      ModsConsole_Object,                                                  \
      #propName,                                                           \
      0,                                                                   \
      0,                                                                   \
      ModsConsole_Get_ ## propName,                                        \
      ModsConsole_Set_ ## propName,                                        \
      0,                                                                   \
      helpmsg                                                              \
   )

MODSCONSOLE_PROP_READWRITE(Width, UINT32,
                           "Width in pixels of the ModsConsole");
MODSCONSOLE_PROP_READWRITE(Height, UINT32,
                           "Height in pixels of the ModsConsole");
MODSCONSOLE_PROP_READWRITE(Refresh, UINT32,
                           "Refresh rate of the ModsConsole");
MODSCONSOLE_PROP_READWRITE(ScrollSize, UINT32,
                           "Scroll size in lines of text of the ModsConsole");
MODSCONSOLE_PROP_READWRITE(Device, UINT32,
                           "Device instance where of the ModsConsole");

//-----------------------------------------------------------------------------
//! \brief Clear the Mods Console screen
//!
C_(ModsConsole_Clear)
{
    MASSERT(pContext     != nullptr);
    MASSERT(pArguments   != nullptr);
    MASSERT(pReturlwalue != nullptr);
    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: ModsConsole.Clear()");
        return JS_FALSE;
    }

    ModsConsole *pModsConsole =
        static_cast<ModsConsole *>(ConsoleManager::Instance()->GetModsConsole());

    if (pModsConsole == nullptr)
    {
        JS_ReportError(pContext, "Failed to get ModsConsole");
        return JS_FALSE;
    }
    pModsConsole->ClearTextScreen();
    RETURN_RC(OK);
}
static SMethod ModsConsole_Clear
(
    ModsConsole_Object,
    "Clear",
    C_ModsConsole_Clear,
    0,
    "Clear the Mods Console Screen"
);
