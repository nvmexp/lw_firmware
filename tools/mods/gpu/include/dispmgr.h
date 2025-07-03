/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2012,2016-2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    dispmgr.h
//! @brief   Declare DisplayMgr -- Display Manager.

#ifndef INCLUDED_DISPMGR_H
#define INCLUDED_DISPMGR_H

#ifndef INCLUDED_FSAA_H
#include "core/include/fsaa.h"
#endif
#ifndef INCLUDED_COLOR_H
#include "core/include/color.h"
#endif
#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#include <map>
#include <memory>
#include <set>
class Display;
class GpuDevice;
class Surface2D;
struct LwDispWindowSettings;
struct LwDispLutSettings;
namespace GpuUtility
{
    struct DisplayImageDescription;
    struct MemoryChunkDesc;
}

//!
//! @class DisplayMgr
//! @brief Display manager.
//!
//! This class arbitrates among multiple tests for access to the
//! Display object, and handles alloc/free of primary Surface2Ds.
//!
//! Only one test can have the real HW Display object, the others share a
//! NullDisplay object instead.
//!
//! Each GpuDevice owns one DisplayMgr.
//! Each DisplayMgr owns one real Display object and a NullDisplay object.
//!
class DisplayMgr
{
public:
    DisplayMgr(GpuDevice * pGpuDevice);
    virtual ~DisplayMgr();

    Display * GetRealDisplay() const;
    Display * GetNullDisplay() const;

    RC Activate();
    RC ActivateComposition();
    RC DeactivateComposition();

    RC SetDisplayClones(UINT32 DisplaysMask);

    enum Requirements
    {
        RequireNullDisplay,     //!< Test prefers to run offscreen
        PreferRealDisplay,      //!< Test prefers to run onscreen, offscreen OK
        RequireRealDisplay,     //!< Test must run onscreen
        RealDisplayOnIdle,      //!< Test contexts requests the real display
                                //!< when no other contexts have the real
                                //!< display
    };

    enum Content
    {
        CONTENT_UNKNOWN = 0,
        CONTENT_CONSOLE,
        CONTENT_TEST
    };

    enum WindowLocation
    {
        LOCATION_TOP_LEFT = 0,
        LOCATION_TOP_RIGHT,
        LOCATION_BOTTOM_RIGHT,
        LOCATION_BOTTOM_LEFT,
        LOCATION_CENTER,
        NUM_LOCATIONS
    };

    struct WindowPosition
    {
        INT32 offsetX = 0;
        INT32 offsetY = 0;
        WindowLocation location = LOCATION_CENTER;
    };

    enum WaitForUpdate
    {
        WAIT_FOR_UPDATE,
        DONT_WAIT_FOR_UPDATE
    };

    //! Callback function called to notify real display context if the display
    //! is stolen or to notify a context that the display has been restored
    //! when the display was allocaed with RealDisplayOnIdle
    typedef RC (* DisplayCallback)(void *pvArgs);
    class TestContext;          // defined below

    //! Test setup (full service version):
    //! - AllocDisplay.
    //! - Alloc a primary surface.
    //! - SetMode and set Image if the HW display was allocated.
    //! - If !requireHwDisplay, UnlockDisplay.
    //
    RC SetupTest
    (
        Requirements           reqs,
        UINT32                 displayWidth,
        UINT32                 displayHeight,
        UINT32                 surfaceWidth,
        UINT32                 surfaceHeight,
        ColorUtils::Format     colorFormat,
        FSAAMode               fsaaMode,
        UINT32                 refreshRate,
        bool                   isCompressed,
        bool                   isBlockLinear,
        DisplayCallback        pStolenCB,
        DisplayCallback        pRestoredCB,
        void                  *pCBArgs,
        TestContext**          ppReturnedTestContext
    );

    //! Test setup (lite version, don't alloc surface):
    //! - AllocDisplay
    //! - SetMode if the HW display was allocated.
    //! - If !requireHwDisplay, UnlockDisplay.
    //!
    //! This version doesn't support FOS FSAA modes.
    //
    RC SetupTest
    (
        Requirements    reqs,
        UINT32          width,
        UINT32          height,
        UINT32          depth,
        UINT32          refreshRate,
        TestContext**   ppReturnedTestContext
    );

    // Really hardcore tests can bypass both versions of TestSetup above
    // and just construct a TestContext, call AllocDisplay, and make direct
    // calls to the Display object, etc.

    //! Per-test resources.
    class TestContext
    {
    public:
        TestContext (DisplayMgr * pDisplayMgr);
        ~TestContext ();

        //! Alloc the "real" or "null" Display object.
        //!
        //! if real display is in use or RequireNullDisplay
        //!     alloc null display
        //! else if real display is not in use
        //!     alloc real display
        //! else if RequireRealDisplay and current owner has unlocked display
        //!     steal real display from current owner
        //! else
        //!     fail
        //!
        //! Stealing is allowed only after the owning TestContext permits it
        //! by calling UnlockDisplay.
        //!
        RC AllocDisplay (Requirements reqs);

        //! Allow other TestContexts to "steal" the real display.
        //!
        //! Normally called by a test that doesn't require real display, after
        //! it has completed its SetMode and SetImage operations.
        //!
        //! Remember to call GetDisplay again after unlocking, to get your new
        //! Display*, which might now be the NullDisplay object instead.
        //!
        void UnlockDisplay(DisplayCallback  pDispStolenCB,
                           DisplayCallback  pDispRestoredCB,
                           void            *pvArgs);

        Display *   GetDisplay () const    { return m_pDisplay; }
        Surface2D * GetSurface2D () const  { return m_pSurface2D; }
        UINT32      GetId () const         { return m_Id; }
        UINT32      GetXBloat () const     { return m_XBloat; }
        UINT32      GetYBloat () const     { return m_YBloat; }
        UINT32      GetDacFilterTaps() const { return m_DacFilterTaps; }
        UINT32      GetDacColorCompress() const;
        GpuDevice * GetOwningDev() const;

        bool IsCompositionEnabled() const;
        bool IsCompositionActive() const;
        UINT32 GetPrimaryDisplayID() const;
        void SetContent(Content content);
        LwDispWindowSettings* GetWindowSettings(UINT32 testWindowIdx = 0) const;
        RC SetWindowPointIn(UINT32 pointInX, UINT32 pointInY) const;
        RC UpdateWindowPosition(UINT32 testWindowIdx = 0) const;

        RC DisplayImage
        (
            Surface2D *image,
            WaitForUpdate waitForUpdate,
            UINT32 testWindowIdx = 0
        );
        RC DisplayImage
        (
            GpuUtility::DisplayImageDescription *imageDescription,
            WaitForUpdate waitForUpdate,
            UINT32 testWindowIdx = 0
        );
        RC DisplayMemoryChunk
        (
            GpuUtility::MemoryChunkDesc *pChunk,
            UINT64 chunkOffset,
            UINT32 width,
            UINT32 height,
            UINT32 depth,
            UINT32 pitch,
            UINT32 testWindowIdx = 0
        );

        RC SetModeAndImage
        (
            UINT32        displayWidth,
            UINT32        displayHeight,
            UINT32        refreshRate
        );

        RC GetPitch(UINT32 *pitch) const;
        RC AdjustPitchForScanout(UINT32 inPitch, UINT32* pOutPitch) const;
        UINT32 GetPrimaryDisplay() const;

    private:
        DisplayMgr * m_pDisplayMgr;

        //! This Id is unique per mods run, across all GpuDevices.
        //! Used by the GL driver interface to determine when we need to
        //! update the DglDesktop structures.
        UINT32      m_Id;

        Content m_Content;

        //! This TestContext has the "real" display but doesn't need it,
        //! and is willing to give it up to another TestContext.
        bool        m_StealingAllowed;

        //! 0 if no display is allocated, else points to either the real
        //! or the NullDisplay object.
        Display *   m_pDisplay;

        //! True if DisplayMgr allocated the Surface2D (and must free it).
        bool        m_IsMySurface;

        //! True if TestContext has called DisableUserInterface.
        //! If true at destruction time, we must call EnableUserInterface.
        bool        m_DisabledUI;

        Surface2D * m_pSurface2D;

        //! Stuff we need to keep around for FSAA FOS modesets.
        //!   FSAA == Full Scene Antialiasing
        //!   FOS  == Filter On Scanout
        UINT32      m_XBloat;
        UINT32      m_YBloat;
        UINT32      m_DacFilterTaps;

        //! Original value of Display.Selected before enabling head cloning
        UINT32      m_OriginalSelect;

        DisplayCallback m_pStolenCB;   //!< Callback when the display is
                                       //!< stolen
        DisplayCallback m_pRestoredCB; //!< Callback when the display is
                                       //!< restored
        void           *m_pCBArgs;     //!< Args to use with the callbacks
        bool            m_bRealOnIdle; //!< true if this context should get
                                       //!< the real display when no one
                                       //!< else has it

        map<UINT32, UINT32> m_OwnedTestWindows; // Local ID, Global ID

        static UINT32   s_NextId;      //!< For assigning unique IDs.

        friend class DisplayMgr;

        //! Private function, called by DisplayMgr::SetupTest (full version)
        RC AllocSurface
        (
            UINT32             width,
            UINT32             height,
            ColorUtils::Format colorFormat,
            FSAAMode           fsaaMode,
            bool               isCompressed,
            bool               isBlockLinear
        );
        RC DisableUserInterface();
        void FreeDisplay();
        UINT32 SelectDisplayMask(Display *pDisp);
        UINT32 GetDisplayWindowIndex(UINT32 testWindowIdx = 0) const;
        UINT32 GetOrAcquireDisplayWindowIndex(UINT32 testWindowIdx);
    };

private:
    GpuDevice *   m_pGpuDevice;       //!< Points to the owning GpuDevice object.
    void *        m_Mutex = nullptr;            //!< Mutex for alloc/free operations.
    bool          m_RealDisplayInUse = false; //!< Some TestContext is using real display.
    Display *     m_pRealDisplay = nullptr;     //!< Points to our "hw" Display object.
    Display *     m_pNullDisplay = nullptr;     //!< Points to our NullDisplay object.
    UINT32        m_OriginalClones = 0;

    bool          m_CompositionEnabled = false;
    bool          m_CompositionActive = false;
    bool          m_CompositionSupported = false;
    UINT32        m_PrimaryDisplayID = 0;
    UINT32        m_CompositionDesktopWidth = 0;
    UINT32        m_CompositionDesktopHeight = 0;
    UINT32        m_CompositionDesktopRefreshRate = 0;
    UINT32        m_CompositionHead;
    map<UINT32, unique_ptr<LwDispWindowSettings>> m_WindowSettings;
    map<UINT32, WindowPosition> m_WindowPositions;
    map<UINT32, unique_ptr<LwDispLutSettings>> m_ILutSettings;
    unique_ptr<LwDispLutSettings> m_pCompositionOLutSettings;

    UINT32       m_ConsoleWinIndex;
    set<UINT32>  m_AvailableTestWindowIndexes;

    list<TestContext*>  m_Contexts;   //!< List of allocated TestContexts

    typedef list<TestContext*>::iterator CtxIter;

    friend class DisplayMgr::TestContext;

    bool AcquireTestWindow(UINT32 *pAcquiredDisplayWindowIdx);
    void ReleaseTestWindow(UINT32 displayWindowIdx);
};

#endif // INCLUDED_DISPMGR_H

