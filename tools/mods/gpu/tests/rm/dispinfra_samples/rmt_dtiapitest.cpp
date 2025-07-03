/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2009 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//!
//! \file rmt_dtiapitester.cpp
//! \brief Sample test to demonstrate use of DTI APIs.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/display/evo_disp.h"
#include "gpu/display/evo_chns.h"
#include "core/include/platform.h"
#include "core/include/xp.h"

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    #include "gpu/display/win32disp.h"
    #include "gpu/display/win32cctx.h"
    #include "lwapi.h"
#endif

class DTIApiTest : public RmTest
{
public:
    DTIApiTest();
    virtual ~DTIApiTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC LwstomTimingsExample(DisplayIDs displayIds);
    SETGET_PROP(cloneView, bool);              //Config CloneView through cmdline
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    RC GetDisplayIdFromGpuAndOutputId(UINT32        gpuId,
        DisplayID     modsDisplayID,
        UINT32       *lwApiDisplayId);
#endif
private:
    bool m_cloneView;

};

//! \brief DTIApiTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
DTIApiTest::DTIApiTest()
{
    SetName("DTIApiTest");
    m_cloneView = false;
}

//! \brief DTIApiTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
DTIApiTest::~DTIApiTest()
{

}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment. Restricting this
//! test to Win32Display Class for now, i.e. only in Win32 MODS. This test is
//! DisplayID based, and checks API of WIN32DISP class
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string DTIApiTest::IsTestSupported()
{
    Display *pDisplay = GetDisplay();

    if (pDisplay->GetDisplayClassFamily() == Display::WIN32DISP || pDisplay->GetDisplayClassFamily() == Display::EVO )
        return RUN_RMTEST_TRUE;
    return "EVO and WIN32DISP Display classes are not supported on current platform";
}

//! \brief Just doing Init from Js, nothing else.
//!
//! \return if InitFromJs passed, else corresponding rc
//------------------------------------------------------------------------------
RC DTIApiTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//!
//! Start overlay, move overlay and free overlay.
//!
//! \return OK if the overlay APIs succeeded, corresponding RC otherwise
//------------------------------------------------------------------------------
RC DTIApiTest::Run()
{
    RC rc;
    Display *pDisplay = GetDisplay();

    DisplayIDs selectedDisplays;
    DisplayID overlayDisplay;
    DisplayIDs allConnectedDisplays;
    DisplayIDs mininumDisplays;
    vector<UINT32>                Heads;
    vector<Display::Mode>         mode;
    //bool       lwstomSettings = true;

    // Used for Intializing Display HW
    if (pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
        CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    }
    // First see if some displays are selected
    CHECK_RC(pDisplay->Selected(&selectedDisplays));

    // if no displays are selected, lets get all the connected displays,
    // then and start overlay on one of them.
    if (selectedDisplays.size() == 0)
    {
        Printf(Tee::PriHigh, "No displays selected lwrrently, checking all connected displays\n");

        DisplayIDs allConnectedDisplays;
        CHECK_RC(pDisplay->GetDetected(&allConnectedDisplays));

        if (allConnectedDisplays.size() == 0)
        {
            Printf(Tee::PriHigh, "Strange.., no displays detected\n");
            return RC::LWRM_ILWALID_STATE;
        }

        // some displays are present, lets pick one of them,
        // and start overlay on that.
        overlayDisplay = allConnectedDisplays[0];
    }
    else
    {
        overlayDisplay = selectedDisplays[0];
    }

    UINT32 Width,Height,Depth,RefreshRate;

    UINT32 posX         = 0;
    UINT32 posY         = 0;

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        Width    = 160;
        Height   = 120;
        Depth = 32;
        RefreshRate  = 60;
    }
    else
    {
        Width    = 800;
        Height   = 600;
        Depth = 32;
        RefreshRate  = 60;
    }

    Surface2D OvlyImage;
    Surface2D CoreImage;
    string OvlyImageName = "dispinfradata/images/ovlyimage640x480.PNG";
    string CoreImageName = "dispinfradata/images/coreimage640x480.PNG";

    CHECK_RC(pDisplay->GetDetected(&allConnectedDisplays));

    CHECK_RC(pDisplay->SetMode(allConnectedDisplays[0],Width,Height,Depth,RefreshRate));

    //************************************************
    // Custom Timings specifying (Works for Evo and Win32 both)
    //************************************************
    CHECK_RC(LwstomTimingsExample(allConnectedDisplays));

    CHECK_RC(pDisplay->SetupChannelImage(overlayDisplay,
        Width,
        Height,
        32,
        Display::OVERLAY_CHANNEL_ID,
        &OvlyImage,
        OvlyImageName));

    Printf(Tee::PriHigh,"Creating a New OverlayImage\n");
    CHECK_RC(pDisplay->SetImage(overlayDisplay,
        &OvlyImage,
        Display::OVERLAY_CHANNEL_ID));

    //************************************************
    // Now move this overlay
    //************************************************
    CHECK_RC_CLEANUP(pDisplay->SetOverlayPos(overlayDisplay,
        posX + 100,
        posY + 100));

    //************************************************
    // Free Overlay
    //************************************************
    Printf(Tee::PriHigh,"Destroy: Overlay\n");
    CHECK_RC(pDisplay->SetImage(overlayDisplay,
        NULL,
        Display::OVERLAY_CHANNEL_ID));

    //************************************************
    // Setting up Dual View
    // For Evo it will return Unsupported Function for now...
    //************************************************
    for(UINT32 i=0; i<allConnectedDisplays.size() && i < 2;i++)
    {
        mode.push_back(Display::Mode(Width,Height,Depth,RefreshRate));
        mininumDisplays.push_back(allConnectedDisplays[i]);
    }

    if (!m_cloneView)
    {
        CHECK_RC(pDisplay->SetMode
          (
          mininumDisplays,
          Heads,
          mode,
          false,
          Display::DUALVIEW
          ));
   }
    mode.clear();

    //************************************************
    // Setting up Clone View
    // For Evo it will return Unsupported Function for now...
    //************************************************
    if (m_cloneView)
    {
      for(UINT32 i=0; i < allConnectedDisplays.size() && i < 2;i++)
          mode.push_back(Display::Mode(1024,768,32,60));

      CHECK_RC(pDisplay->SetMode
          (
          mininumDisplays,
          Heads,
          mode,
          false,
          Display::CLONE
          ));
    }

    //************************************************
    //example of other win32 specific advanced custom settings
    //************************************************

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    UINT32     lwApiDisplayId;
    if(pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
    {
        Win32Context** ppWin32Context  = (Win32Context **)pDisplay->GetLwstomContext();
        Win32CoreDisplaySettings *LwstomSettings = &(*ppWin32Context)->DispLwstomSettings;

        CHECK_RC(GetDisplayIdFromGpuAndOutputId(pDisplay->GetOwningDisplaySubdevice()->GetGpuId(),allConnectedDisplays[0],&lwApiDisplayId));

        //LwstomSettings->Reset();
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->displayId = lwApiDisplayId;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->rotation = (LW_ROTATE)LW_ROTATE_90;
        LwstomSettings->pathSettings.dirty->targetInfo->details->rotation = true;

        CHECK_RC(pDisplay->SetMode(allConnectedDisplays[0],Width,Height,Depth,RefreshRate));
    }
#endif

Cleanup:

    CHECK_RC(pDisplay->DetachDisplay(mininumDisplays));
    if(OvlyImage.GetMemHandle() != 0)
    {
        OvlyImage.Free();
    }

    return rc;
}

RC DTIApiTest::LwstomTimingsExample(DisplayIDs displayIds)
{
    RC rc;
    bool commonEvoWin32_LwstomTiming = true;
    UINT32 width = 800;
    UINT32 height = 600;
    UINT32 depth   = 32;
    UINT32 refreshRate = 60;
    Display *pDisplay = GetDisplay();

    //This will set custom timings on both "EVO" and "win32" platform..
    //enduser must not worry about the platform on which he is running
    if(commonEvoWin32_LwstomTiming)
    {
        EvoRasterSettings RasterSettings;
        RasterSettings.RasterWidth  = width + 26;
        RasterSettings.ActiveX      = width;
        RasterSettings.SyncEndX     = 15;
        RasterSettings.BlankStartX  = RasterSettings.RasterWidth - 5;
        RasterSettings.BlankEndX    = RasterSettings.SyncEndX + 4;
        RasterSettings.PolarityX    = false;
        RasterSettings.RasterHeight = height + 30;
        RasterSettings.ActiveY      = height;
        RasterSettings.SyncEndY     = 0;
        RasterSettings.BlankStartY  = RasterSettings.RasterHeight - 2;
        RasterSettings.BlankEndY    = RasterSettings.SyncEndY + 24;
        RasterSettings.Blank2StartY = 1;
        RasterSettings.Blank2EndY   = 0;
        RasterSettings.PolarityY    = false;
        RasterSettings.Interlaced   = false;

        Display::DisplayType DispType;
        CHECK_RC(pDisplay->GetDisplayType(displayIds[0], &DispType));
        if(DispType == Display::CRT)
        {
            RasterSettings.PixelClockHz = 350000000;
        }
        else
        {
            RasterSettings.PixelClockHz = 165000000;
        }

        RasterSettings.Dirty = true;

        CHECK_RC(pDisplay->SetupEvoLwstomTimings
            (displayIds[0],
            width,
            height,
            refreshRate,
            Display::MANUAL,
            &RasterSettings,
            Display::ILWALID_HEAD,
            0,
            NULL));
    }
    CHECK_RC(pDisplay->SetMode(displayIds[0],width,height,depth,refreshRate));

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    UINT32     lwApiDisplayId;
    bool lwstomTimingsWin32Specific = false;

    // Custom Timing using only Win32 data structures is also possible
    if (lwstomTimingsWin32Specific)
    {
        Win32Context** ppWin32Context  = (Win32Context **)pDisplay->GetLwstomContext();
        Win32CoreDisplaySettings *LwstomSettings = &(*ppWin32Context)->DispLwstomSettings;
        //LwstomSettings->Reset();
        CHECK_RC(GetDisplayIdFromGpuAndOutputId(pDisplay->GetOwningDisplaySubdevice()->GetGpuId(),displayIds[0],&lwApiDisplayId));

        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->displayId = lwApiDisplayId;

        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.HVisible     = 800;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.HBorder      = 0;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.HFrontPorch  = 40;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.HSyncWidth   = 128;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.HTotal       = 1056;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.HSyncPol     = 0;

        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.VVisible     = 600;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.VBorder      = 0;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.VFrontPorch  = 1;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.VSyncWidth   = 4;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.VTotal       = 628;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.VSyncPol     = 0;

        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.interlaced   = 0;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.pclk         = 4000;

        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.etc.flag         = 0;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.etc.rr           = 60;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.etc.rrx1k        = 60317;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.etc.aspect       = 0;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.etc.rep          = 1;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timing.etc.status       = 2821;
        LwstomSettings->pathSettings.pPathInfo[0].targetInfo->details->timingOverride = (LW_TIMING_OVERRIDE)LW_TIMING_OVERRIDE_LWST;
        LwstomSettings->pathSettings.dirty[0].targetInfo->details->timing = true;
    }
#endif

    CHECK_RC(pDisplay->SetMode(displayIds[0],width,height,depth,refreshRate));
    return rc;
}

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
RC DTIApiTest::GetDisplayIdFromGpuAndOutputId(UINT32        gpuId,
                                               DisplayID     modsDisplayID,
                                               UINT32       *lwApiDisplayId)
{
    RC rc;
    LwAPI_Status            lwApiStatus;
    LwPhysicalGpuHandle     gpuHandleThisDisplay;

    lwApiStatus = LwAPI_Initialize();
    if (lwApiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%s : LwAPI_Initialize failed \n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Get the Gpuhandle of our GPU ID
    lwApiStatus = LwAPI_GetPhysicalGPUFromGPUID(gpuId, &gpuHandleThisDisplay);
    if (lwApiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%s : LwAPI_GetPhysicalGPUFromGPUID failed \n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Now get the lwapi style of DisplayId for thisDisplay
    lwApiStatus = LwAPI_SYS_GetDisplayIdFromGpuAndOutputId(gpuHandleThisDisplay, (UINT32)modsDisplayID, lwApiDisplayId);
    if (lwApiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%s : LwAPI_SYS_GetDisplayIdFromGpuAndOutputId failed \n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}
#endif

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC DTIApiTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(DTIApiTest, RmTest,
                 "Test to demonstrate use of overlay APIs");
CLASS_PROP_READWRITE(DTIApiTest, cloneView, bool,
                 "Select Clone view");
