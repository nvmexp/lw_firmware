/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_CycleLwDisp
//!       This test calls lwdisplay library APIs to verify window(s) on head(s)

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "hwref/disp/v03_00/dev_disp.h" // LW_PDISP_FE_DEBUG_CTL
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/display/evo_dp.h"
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "gpu/tests/rm/utility/dtiutils.h"

using namespace std;

class CycleLwDisp : public RmTest
{
public:
    CycleLwDisp();
    virtual ~CycleLwDisp();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(head, UINT32);
    SETGET_PROP(windows, string);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(manualVerif, bool);

    RC InitDPDisplays(DisplayIDs dpRootDisplays,
                     vector<DPConnector*> &pConnectors,
                     vector<DisplayPort::Group*> &pDpGroups,
                     DisplayIDs& monitorIDs);

private:
    Display *m_pDisplay;
    DPDisplay* m_pDpDisplay;
    UINT32 m_head;
    string m_windows;
    bool m_manualVerif;
    string m_protocol;
};

CycleLwDisp::CycleLwDisp()
{
    m_pDisplay = GetDisplay();
    m_pDpDisplay = nullptr;
    SetName("CycleLwDisp");
    m_head = 0;
    m_windows = "0,1";
    m_protocol = "SINGLE_TMDS_A,SINGLE_TMDS_B,DP_A,DP_B";
    m_manualVerif = false;
}

//! \brief LwDispBringupPass1 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CycleLwDisp::~CycleLwDisp()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CycleLwDisp::IsTestSupported()
{
    if (m_pDisplay->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
            return "The test is supported only on LwDisplay class!";
    }

    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state setup based on the JS setting
//------------------------------------------------------------------------------
RC CycleLwDisp::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

RC CycleLwDisp::Run()
{
    RC rc = OK;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    vector<WindowIndex> hWin;
    DisplayIDs detectedDisps;
    DisplayID  setModeDisplay;
    UINT32 width, height, refreshRate;
    vector <LwDispWindowSettings> winSettings;
    vector<string> windowNumString;
    UINT32 windowNum;
    UINT32 sorNum = 2;

    // allocate core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        width    = 320;
        height   = 320;
        refreshRate  = 60;
    }
    else
    {
        width    = 800;
        height   = 600;
        refreshRate  = 60;
    }

    //
    // As of now, we just have SINGLE_TMDS_A support on fmodel and this is always
    // forcefully detected on emu/fmodel so we dont need to fake the TMDS display.
    //
    CHECK_RC(m_pDisplay->GetDetected(&detectedDisps));
    Printf(Tee::PriHigh, "Detected displays  = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, detectedDisps);

    if (detectedDisps.size() == 0)
    {
        Printf(Tee::PriHigh, "There is no display detected!\n");
        return RC::SOFTWARE_ERROR;
    }

    DisplayIDs dpDetectedDisps;
    DisplayIDs tmdsDetectedDisps;
    for (UINT32 i = 0; i < detectedDisps.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, detectedDisps[i], m_protocol))
        {
            if (m_protocol.find("DP") != std::string::npos)
            {
                dpDetectedDisps.push_back(detectedDisps[i]);
            }
            else if(m_protocol.find("TMDS") != std::string::npos)
            {
                tmdsDetectedDisps.push_back(detectedDisps[i]);
            }
        }
    }

    if (dpDetectedDisps.size() == 0 && tmdsDetectedDisps.size() == 0)
    {
        Printf(Tee::PriHigh, "There is no TMDS/DP display detected! Check VBIOS...\n");
        return RC::SOFTWARE_ERROR;
    }

    bool isDPDisplayDisplay = false;
    if (dpDetectedDisps.size() > 0)
    {
        isDPDisplayDisplay =  true;
        Printf(Tee::PriHigh, "Doing DP Modeset...\n");
    }
    else
    {
        Printf(Tee::PriHigh, "Doing TMDS Modeset...\n");
    }

    LwDispCoreChnContext *pCoreChCtx;

    CHECK_RC(pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
        (LwDispChnContext **)&pCoreChCtx,
        LwDisplay::ILWALID_WINDOW_INSTANCE,
        m_head));

    LwDispHeadSettings &headSettings = (pCoreChCtx->DispLwstomSettings.HeadSettings[m_head]);

    vector<DPConnector*>        pConnectors(dpDetectedDisps.size());
    vector<DisplayPort::Group*> pDpGroups(dpDetectedDisps.size());
    DisplayIDs setModeDisplays;

    if (isDPDisplayDisplay)
    {
        m_pDpDisplay = new DPDisplay(m_pDisplay);
        m_pDpDisplay->getDisplay()->SetUseDPLibrary(true);

        m_pDpDisplay->setDpMode(SST);

        CHECK_RC(InitDPDisplays(dpDetectedDisps,
                       pConnectors,
                       pDpGroups,
                       setModeDisplays)); // SetMode Displays is output param
        m_head = 0;
        sorNum =0;
    }
    else
    {
        m_head = 2;
        setModeDisplays = tmdsDetectedDisps;
        sorNum = 2;

        LwDispSORSettings& sorSettings = pCoreChCtx->DispLwstomSettings.SorSettings[sorNum];
        sorSettings.ORNumber  = sorNum;
        sorSettings.OwnerMask = 0;
        sorSettings.protocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A;
        sorSettings.CrcActiveRasterOnly = false;
        sorSettings.HSyncPolarityNegative = false;
        sorSettings.VSyncPolarityNegative = false;
        sorSettings.Dirty = true;
        headSettings.pORSettings = &sorSettings;
    }

    windowNumString = DTIUtils::VerifUtils::Tokenize(m_windows,",");
    hWin.resize(windowNumString.size());
    winSettings.resize(windowNumString.size());

    vector<UINT32> winWidths;
    vector<UINT32> winHeights;
    winWidths.resize(windowNumString.size());
    winHeights.resize(windowNumString.size());

// first Image will be of 800x600
    winWidths[0] = width;
    winHeights[0] = height;

// Second Image is of 640 x 480
    winWidths[1] = 640;
    winHeights[1] = 480;

    for (UINT32 i = 0; i < windowNumString.size(); i++)
    {
        // Allocate the window channel for the given window instance
        windowNum = std::atoi(windowNumString[i].c_str());
        CHECK_RC(pLwDisplay->AllocateWinAndWinImm(&hWin[i], m_head, windowNum));

        LwDispSurfaceParams surface;

        // Allocate default surface to the given window
        winSettings[i].winIndex = hWin[i];
        winSettings[i].image.pkd.pWindowImage = new Surface2D;
        winSettings[i].SetSizeOut(winWidths[i], winHeights[i]);

        surface.imageWidth = winSettings[i].sizeOut.width;;
        surface.imageHeight = winSettings[i].sizeOut.height;
        surface.colorDepthBpp = 32;
        surface.layout = Surface2D::Pitch;
        DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(surface.imageWidth, surface.imageHeight);
        surface.imageFileName = imgArr.GetImageName();

        CHECK_RC(SurfaceUtils::SurfaceHelper::SetupSurface(
           m_pDisplay->GetOwningDisplaySubdevice(),
           &surface,
           winSettings[i].image.pkd.pWindowImage,
           Display::WINDOW_CHANNEL_ID));

        CHECK_RC(pLwDisplay->SetupChannelImage(setModeDisplays[0], &winSettings[i]));
    }

    if (!(DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, setModeDisplays[0]) &&
        DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, setModeDisplays[0])))
    {
        if((rc = DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, setModeDisplays[0], true)) != OK)
        {
            Printf(Tee::PriHigh,"SetLwstomEdid failed for displayID = 0x%X \n",
                (UINT32)setModeDisplays[0]);
            return rc;
        }
    }

    if (GetBoundGpuSubdevice()->IsDFPGA())
    {
        // 38 MHZ of pixel clock
        UINT32 pixelclock = 38000000;
        Printf(Tee::PriHigh, "In %s() Using PixelClock = %d \n", __FUNCTION__, pixelclock);
        headSettings.pixelClkSettings.pixelClkFreqHz = pixelclock;
        headSettings.pixelClkSettings.bAdj1000Div1001 = false;
        headSettings.pixelClkSettings.bNotDriver = false;
        headSettings.pixelClkSettings.bHopping = false;
        headSettings.pixelClkSettings.HoppingMode = LwDispPixelClockSettings::VBLANK;
        headSettings.pixelClkSettings.dirty = true;
    }

    if (!isDPDisplayDisplay)
    {
        // SetMode on the display with the given windows
        CHECK_RC(pLwDisplay->SetMode(setModeDisplays[0], width, height, 32, refreshRate, m_head));
    }
    else
    {

        pConnectors[0]->notifyDetachBegin(NULL);
        pConnectors[0]->notifyDetachEnd();

        // dcbDisplayID  is connecterId
        CHECK_RC(m_pDpDisplay->SetMode(dpDetectedDisps[0], setModeDisplays[0],
                                     pDpGroups[0],
                                     width,
                                     height,
                                     32,
                                     refreshRate,
                                     m_head));
    }

    if (m_manualVerif)
    {
        if (isDPDisplayDisplay)
        {
            MASSERT( 0 && "See DP monitor and Check Image");
        }
        else
        {
            MASSERT( 0 && "See TMDS monitor and Check Image");
        }
    }

    // Now Detach Display here
    if (!isDPDisplayDisplay)
    {
        CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, setModeDisplays[0])));
    }
    else
    {
        CHECK_RC(m_pDpDisplay->DetachDisplay(dpDetectedDisps[0], setModeDisplays[0], pDpGroups[0]));
    }

    for (UINT32 i = 0; i < windowNumString.size(); i++)
    {
        CHECK_RC(pLwDisplay->DestroyWinAndWinImm(hWin[i]));
        if (winSettings[i].image.pkd.pWindowImage && winSettings[i].image.pkd.pWindowImage->GetMemHandle())
        {
            winSettings[i].image.pkd.pWindowImage->Free();
            delete winSettings[i].image.pkd.pWindowImage;
            winSettings[i].image.pkd.pWindowImage = NULL;
        }
    }

    return rc;
}

RC CycleLwDisp::InitDPDisplays(DisplayIDs dpDcbDisplays,
                               vector<DPConnector*> &pConnectors,
                               vector<DisplayPort::Group*> &pDpGroups,
                               DisplayIDs& monitorIDs)
{

    //
    // Set correct mode in evoDisp as this is required for displayID allocation.
    // in MST mode we do dynamic display ID while in SST mode we use connectorID
    // Here "0" is SST Mode & "1"  is MST Mode.
    //

    RC rc;

    vector<DisplayIDs>monitorIds;

    // pick 2 from the available Dp connectros.
    for (LwU32 i = 0; i < (LwU32)dpDcbDisplays.size(); i++)
    {
        pDpGroups[i] = NULL;

        DisplayIDs monIdsPerCnctr;
        //get all detected monitors in respective connectors
        CHECK_RC(m_pDpDisplay->EnumAllocAndGetDPDisplays(dpDcbDisplays[i], &(pConnectors[i]), &monIdsPerCnctr, true));
        pDpGroups[i] = pConnectors[i]->CreateGroup(monIdsPerCnctr);
        if (pDpGroups[i] == NULL)
        {
            Printf(Tee::PriHigh, "\n ERROR: Fail to create group\n");
            return RC::SOFTWARE_ERROR;
        }
        monitorIds.push_back(monIdsPerCnctr);
    }

    for (UINT32 i = 0; i < monitorIds.size(); i++)
    {
        for (UINT32 j= 0; j < monitorIds[i].size(); j++)
        {
            monitorIDs.push_back(monitorIds[i][j]);
        }
    }
    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC CycleLwDisp::Cleanup()
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
JS_CLASS_INHERIT(CycleLwDisp, RmTest,
    "Simple test to demonstrate usage of SetMode() API");

CLASS_PROP_READWRITE(CycleLwDisp, head, UINT32,
                     "head number to be used in the test");
CLASS_PROP_READWRITE(CycleLwDisp, windows, string,
                     "window(s) to be attached to the given head");
CLASS_PROP_READWRITE(CycleLwDisp, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(CycleLwDisp,manualVerif,bool,
                     "do manual verification, default = 0");
