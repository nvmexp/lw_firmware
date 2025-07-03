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

//! @file    modscnsl.h
//! @brief   Declare ModsConsole -- MODS Console Class.

#pragma once

#include "surf2d.h"
#include "core/include/display.h"
#include "core/include/rawcnsl.h"
#include "gpu/include/dispmgr.h"
#include "gpu/utility/gpuutils.h"
#include <atomic>
#include <vector>

class GpuDevice;
class Display;
class EventThread;
struct LwDispLutSettings;

//!
//! @class ModsConsole
//! MODS Console.
//!
//! This defines the MODS Console which uses the MODS surface and display
//! classes to implement a console display.  The MODS console has two
//! modes of operation - Yielding and NonYielding mode.
//!
//! Yielding mode is much more efficient in terms of draw speed when
//! scrolling.  The following ASCII art with accompanying text
//! describes the surface management for a yielding console.
//!
//! |--------------------------------------|
//! |                                      |
//! |              Top Screen              |
//! |                                      |
//! |--------------------------------------|
//! |                                      |
//! |                                      |
//! |**************************************| <-- Cursor positioning reference
//! |                                      |
//! |                                      | Current draw screen
//! |                                      |
//! |**************************************| <-- Current cursor location
//! |             Scroll Buffer            | <-- Earliest history point
//! |                                      |
//! |                                      |
//! |######################################| <-- Display start location
//! |                                      |
//! |                                      | Current displayed screen
//! |                                      |
//! |######################################|
//! |--------------------------------------|
//! |                                      |
//! |             Bottom Screen            |
//! |                                      |
//! |--------------------------------------|
//!
//! The surface is divided up into three sections:  A region at the top of
//! the surface which is the exact height of the screen, a region which
//! contains the scrollback, and a region at the bottom of the surface
//! identical to to top screen region.
//!
//! In Yielding mode, any writes within either the top or bottom screen
//! region are mirrored into the opposing region.  This eliminates the need
//! to actually copy any data during scrollback and allows scrolling the
//! console by simply updating the display position within the surface.
//!
//! The current displayed screen is a moving window within the surface in
//! yielding mode with control of the surface managed like a cirlwlar
//! buffer
//!
//! In non-yielding mode, it is not possible to update the display position
//! so in this mode the displayed portion of the surface is always fixed.
//! This means that any scrolling that is performed required entire screen
//! refreshes
//!
//! In both modes, the MODS console required input from the console to occur
//! immediately after a keypress oclwrs.  This means that there are some
//! platform specific implementation portions of this class which reside in
//! the appropriate platform directory in order to configure input from
//! the keyboard appropriately
//!
//! For detailed comments on each of the public interfaces, see console.h
class ModsConsole : public RawConsole
{
public:
    ModsConsole();
    virtual ~ModsConsole();

    // Virtual functions implemented here defined in the Console class
    RC Enable(ConsoleBuffer *pConsoleBuffer) override;
    RC Disable() override;
    bool IsEnabled() const override;
    RC Suspend() override;
    RC PopUp() override;
    RC PopDown() override;
    bool IsPoppedUp() const override;
    RC SetDisplayDevice(GpuDevice *pDevice) override;
    GpuDevice * GetDisplayDevice() override { return m_pGpuDevice; }
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
    bool Formatting() const override;
    bool Echo() const override;

    // Accessor functions used by JS interfaces to the Mods Console
    bool IsInitialized() const { return m_bInitialized; }
    void SetWidth(UINT32 Width) { m_JsWidth = Width; }
    UINT32 GetWidth() const { return m_JsWidth; }
    void SetHeight(UINT32 Height) { m_JsHeight = Height; }
    UINT32 GetHeight() const { return m_JsHeight; }
    void SetRefresh(UINT32 Refresh) { m_JsRefresh = Refresh; }
    UINT32 GetRefresh() const { return m_JsRefresh; }
    void SetScrollSize(UINT32 ScrollSize) { m_JsScrollSize = ScrollSize; }
    UINT32 GetScrollSize() const { return m_ScrollSize; }
    void SetDevice(UINT32 DevInst) { m_JsDevInst = DevInst; }
    UINT32 GetDevice() const { return m_JsDevInst; }

    //! Class for temporarily pausing printing to the MODS console. Used
    //! primarily during ECC tests since MODS console writes can cause
    //! ECC errors if they happen to occur while checkbits are disabled
    //!
    //! NOTE : Instead of pausing on DOS platforms (since printing can
    //! happen from a real interrupt and pausing in the print routine
    //! ilwolves yielding) prints are dropped while paused
    void Pause(bool bPause);
    class PauseConsoleUpdates
    {
    public:
        PauseConsoleUpdates() : m_bPaused(false) { }
        ~PauseConsoleUpdates();
        RC Pause();
    private:
        // Non-copyable
        PauseConsoleUpdates(const PauseConsoleUpdates&);
        PauseConsoleUpdates& operator=(const PauseConsoleUpdates&);

        bool  m_bPaused;
    };

private:
    void EnterSingleCharMode(bool echo) override;
    void ExitSingleCharMode() override;

    //! Enumeration of LUT indexes for the Mods Console LUT
    enum LutIndexes
    {
        LUT_BLACK   = 0,
        LUT_GREY    = 1,
        LUT_WHITE   = 2,
        LUT_BLUE    = 3,
        LUT_GREEN   = 4,
        LUT_RED     = 5,
        LUT_YELLOW  = 6,
        LUT_CYAN    = 7,
        LUT_MAGENTA = 8
    };

    //! 32-bit colors
    enum Colors32
    {
        COL_BLACK   = 0xFF000000,
        COL_GREY    = 0xFFA0A0A0,
        COL_WHITE   = 0xFFFFFFFF,
        COL_BLUE    = 0xFF0000A0,
        COL_GREEN   = 0xFF00A000,
        COL_RED     = 0xFFA00000,
        COL_YELLOW  = 0xFFA0A000,
        COL_CYAN    = 0xFF00A0A0,
        COL_MAGENTA = 0xFFA000A0
    };

    void ResetDisplayVars();
    RC   Initialize();
    RC   Show();
    RC   Hide();
    void ClearLine() const;
    void WriteLine(UINT32 bufferLine);
    void NextLine();
    RC   UpdateLwrsor();
    RC   UpdateDisplayPosition();
    void UpdateDisplayYielding();
    void UpdateDisplayNonYielding();
    RC   ScrollScreenYielding(INT32 lines);
    RC   ScrollScreenNonYielding(INT32 lines);
    void GetColor
    (
        Tee::ScreenPrintState state,
        UINT32 *pFGColor,
        UINT32 *pBGColor
    ) const;
    void PrintHeader();
    void SetDuplicateWrites();

    static RC   DisplayStolenCB(void *pvArgs);
    static RC   DisplayRestoredCB(void *pvArgs);
    static void DoUpdateDisplayPosition(void *pvThis);
    RC DisableModsConsole(bool bResume);

    UINT32 m_Width;       //!< Width in pixels of the console
    UINT32 m_Height;      //!< Height in pixels of the console
    UINT32 m_Refresh;     //!< Refresh rate of the console
    ColorUtils::Format m_ColorFormat; //!< Color format of the console surface
    UINT32 m_ScrollSize;  //!< Scrollback history size in text rows for the
                          //!< console
    UINT32 m_CharWidth;   //!< Character width on screen after scaling.
    UINT32 m_CharHeight;  //!< Character height on screen after scaling.
    UINT32 m_CharScale;   //!< Character scaling factor.
    UINT32 m_JsWidth;     //!< Width in pixels of the console (set from JS)
    UINT32 m_JsHeight;    //!< Height in pixels of the console (set from JS)
    UINT32 m_JsRefresh;   //!< Refresh rate of the console (set from JS)
    UINT32 m_JsScrollSize;//!< Scrollback history size in text rows for the
                          //!< console (set from JS)
    UINT32 m_JsDevInst;   //!< Device instance to display the console on
                          //!< (set from JS)

    GpuDevice               *m_pGpuDevice;  //!< GpuDevice where the console is
                                            //!< displayed

    bool                     m_bInitialized;//!< true if the console in init'd
    bool                     m_bEnabled;    //!< true if the console in enabled
    bool                     m_bStolen;     //!< true if the display manager
                                            //!< has stolen the real display
    bool                     m_bAllowYield; //!< true if the console can yield

    //! Current cursor position (in text rows) referenced from the draw start
    //! row
    UINT32                   m_LwrX;
    UINT32                   m_LwrY;
    Console::Cursor          m_LwrsorType;  //!< Current cursor type

    //! true if writes should be duplicated (either from the top screen portion
    //! of the surface to the bottom - or vice-versa)
    bool                     m_bDuplicateWrites;

    //! The current drawing start row, in text rows, within the console surface
    INT32                    m_DrawStartRow;

    //! The current valid start row, in text rows, (start of back history in
    //! the console surface) within the console surface
    INT32                    m_ValidStartRow;

    //! The current display start row, in text rows, within the console surface
    INT32                    m_DisplayStartRow;

    //! The pending display start row, in text rows, within the console surface,
    //! used only for yielding consoles
    INT32                    m_PendingDisplayStartRow;

    //! true if the valid row should be incremented when the draw start row is
    //! incremented
    bool                     m_bIncrementValidRow;

    //! true if the console is in the middle of being updated and further
    //! updates should be disallowed
    bool                     m_bUpdatingConsole;

    //! true if the console should allow prints to be processed (set to false
    //! usually during printing/updating the console in order to prevent
    //! relwrsive prints)
    bool                     m_bEnablePrints;

    bool                     m_bHeaderPrinted;  //!< true if the console header
                                                //!< help text has printed

    Surface2D                m_ConsoleSurf;     //!< Surface for the console
    DisplayMgr::TestContext *m_pTestContext;    //!< Context for the console
    vector<UINT32>           m_ConsoleLut;      //!< LUT for the console

    //! Thread and mutex for updating the display position
    EventThread             *m_pUpdateDisplayThread;
    void                    *m_pUpdateDisplayMutex;

    //! Display description for setting the MODS console display images
    GpuUtility::DisplayImageDescription m_DisplayDesc;

    LwDispLutSettings               *m_pLutSettings;

    //! Pointer to a console buffer that maintains the console history
    ConsoleBuffer                   *m_pConsoleBuffer;

    //! Line buffer to speed up drawing to the surface
    string                           m_LineBuffer;

    //! true if the mods console disabled the user interface
    bool m_bUserInterfaceDisabled;

    //! Variables for pausing MODS printing
    atomic<INT32> m_PauseCount{0};
    void*         m_PrintLock = nullptr;

    //! Character width/height size
    static const UINT32 s_CharHeight;
    static const UINT32 s_CharWidth;

    //! Class for automatically flagging the console as being in the middle of
    //! an update and potentially disabling the screen prints during the
    //! update.
    class UpdatingConsole
    {
    public:
        UpdatingConsole(ModsConsole *pConsole, bool bDisableScreenOutput);
        ~UpdatingConsole();
    private:
        ModsConsole *m_pConsole;             //!< Pointer to the ModsConsole
        bool         m_bDisableScreenOutput; //!< true if output needs to be
                                             //!< disabled during update
        bool         m_bSavedEnablePrints;   //!< Saved value for the print
                                             //!< enable flag
        bool         m_bSavedUpdatingConsole;//!< Saved value for the console
                                             //!< update flag
    };
    friend class UpdatingConsole;
};
