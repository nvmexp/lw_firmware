/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_LwDispBasicWindowSanity
//!       This test does the functional verification of window fmodel release

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/statecache_C3.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "class/clc37e.h" // LWC37E_WINDOW_CHANNEL_DMA
#include "class/clc37a.h" // LWC37A_LWRSOR_IMM_CHANNEL_PIO
#include "hwref/disp/v03_00/dev_disp.h" // LW_PDISP_FE_DEBUG_CTL
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/dispchan.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/modeset_utils.h"
#include "class/clc37dcrcnotif.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "ctrl/ctrl0073.h"

using namespace std;

#define MAX_WINDOWS 32
#define OBJ_MAX_SORS    4
#define LWD_BASICWIN_DEFAULT_SIZE 0x1000

const UINT32 TransferFunctionEntrySize = 8;
const UINT32 TransferFunctionNumEntries = 1025;

struct LwDispBasicWindowSanityNotifierSettings
{
    bool Dirty;
    NotifierMode mode;
    void* CompletionAddress;
    UINT32 NumNotifiers;
    UINT32 NotifierSize;
    bool (*PollNotifierDone)(void *);
    LwDispBasicWindowSanityNotifierSettings();
};

LwDispBasicWindowSanityNotifierSettings::LwDispBasicWindowSanityNotifierSettings()
{
    Dirty = false;
    CompletionAddress = NULL;
    NumNotifiers = 0;
    NotifierSize = 0;
}

class LwDispBasicWindowSanity : public RmTest
{
public:
    LwDispBasicWindowSanity();
    virtual ~LwDispBasicWindowSanity();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC SetupViewportOut
    (
        DisplayChannel *pCh,
        UINT32 head,
        LwDispViewPortSettings *pViewPortSettings
    );

    RC SetupViewportIn
    (
        DisplayChannel *pCh,
        UINT32 head,
        LwDispViewPortSettings *pViewPortSettings
    );

    RC SetupSurface
    (
        DisplayID display,
        LwDispWindowSettings *winSettings,
        DisplayChannel *pCh
    );

    RC SetupLwrsorSurface
    (
        DisplayID display,
        LwDispLwrsorSettings *pLwrsorSettings,
        DisplayChannel *pCh,
        UINT32 head
    );

    RC VerifyLwrsorSupport
    (
        DisplayID setModeDisplay,
        UINT32 head,
        DisplayChannel *pCoreCh,
        DisplayChannel *pLwrsorCh,
        LwDispLwrsorSettings *pLwrsorSettings
    );

    RC SetTimestampLo
    (
        DisplayChannel *pCh,
        UINT32 value
    );

    RC SetCompositionCtrl
    (
        DisplayChannel *pCh,
        UINT32 depth
    );

    RC SetWinSizeIn
    (
        DisplayChannel *pCh,
        LwDispWindowSettings *winSettings
    );

    RC SetWinSize
    (
        DisplayChannel *pCh,
        LwDispWindowSettings *winSettings
    );

    RC SetWinSizeOut
    (
        DisplayChannel *pCh,
        LwDispWindowSettings *winSettings
    );

    RC SetupDesktopColor
    (
        DisplayChannel *pCh,
        UINT32 head,
        LwDispColor desktopColor
    );

    RC SetupOR
    (
        DisplayChannel *pCh,
        DisplayID displayID,
        UINT32 head,
        bool   bDetach,
        LwDispORSettings *pORSettings
    );

    RC InitCoreNotifierSettings
    (
        LwDispChnContext* pChnContext,
        LwU32 offset = 0
    );

    RC InitWindowNotifierSettings
    (
        LwDispChnContext* pChnContext,
        int               windowId,
        LwU32             offset = 0
    );

    RC SetupCoreCompletionNotifier
    (
        DisplayChannel        *pCh,
        LwDispChnContext* pChnContext,
        LwU32                 offset = 0
     );

    RC SetupWindowCompletionNotifier
    (
        DisplayChannel          *pCh,
        LwDispChnContext* pChnContext,
        int                     windowId,
        LwU32                   offset = 0
     );

    static bool PollNotifierComplete
    (
        void *Args
    );

    static bool PollCrcNotifierComplete
    (
        void *Args
    );

    RC WindowSetOwner
    (
        DisplayChannel *pCh,
        UINT32 winInstance,
        UINT32 head
    );

    RC SetupRaster
    (
        DisplayChannel *pCh,
        UINT32 head,
        LwDispRasterSettings *pRasterSettings
    );

    RC SetupPixelClock
    (
        DisplayChannel *pCh,
        UINT32 head,
        LwDispPixelClockSettings *pPixelClkSettings
    );

    RC VerifyLut
    (
        DisplayID display,
        LwDispChnContext *pChnCtx,
        UINT32 head,
        LwDispLutSettings *pLutSettings
    );

    RC SetOutputLUT
    (
        DisplayID Display,
        UINT32 Head,
        DisplayChannel *pCh,
        LwDispLutSettings& pLutSettings
    );

    RC SetInputLUT
    (
        DisplayID Display,
        UINT32 Head,
        DisplayChannel *pCh,
        LwDispLutSettings& pLutSettings
    );

    RC SetupLUTBuffer
    (
        DisplayID Display,
        UINT32  Head,
        LwDispChnContext *pChnCtx,
        LwDispLutSettings *pLutSettings,
        UINT32 *pData,
        UINT32 Size
    );

    RC AllocSetupLutBuffer
    (
        LwDispChnContext *pChnCtx,
        UINT32 Head,
        LwDispLutSettings *pLutSettings
    );

    // CRC functions
    RC StartRunningCrc
    (
        LwDispChnContext *m_pCoreChCtx,
        DisplayChannel *m_pCoreCh,
        UINT32 Head,
        LwDispCRCSettings *pCrcSettings
    );

    RC PrepPollCrcNotifier
    (
        DisplayChannel *m_pCoreCh,
        DisplayChannel * pWinCh,
        UINT32 Head,
        Display::ChannelID chID
    );

    RC GetCrcValues
    (
        UINT32 Head,
        LwDispCRCSettings *pCrcSettings
    );

    RC SetupCrcBuffer
    (
        LwDispChnContext *m_pCoreChCtx,
        UINT32 Head,
        LwDispCRCSettings *pCrcSettings
    );

    RC PollCrcCompletion
    (
        LwDispCRCSettings *pCrcSettings
    );

    RC GetProtocol
    (
        UINT32 orProtocol,
        UINT32 *pProtocol
    );

    SETGET_PROP(UseDebugPortChannel, bool);
    SETGET_PROP(TestLwrsor, bool);
    SETGET_PROP(UseWindowNotifier, bool);
    SETGET_PROP(TestOutputLut, bool);
    SETGET_PROP(TestInputLut, bool);
    SETGET_PROP(IncludeCrc, bool);
    SETGET_PROP(rastersize, string); //Config raster size through cmdline

    enum
    {
        HEADS_SIZE = 4,
        WINDOWS_SIZE = 8
    };
private:
    Display *m_pDisplay;
    LwDisplay *pLwDisplay;
    bool m_UseDebugPortChannel;
    bool m_TestLwrsor;
    bool m_UseWindowNotifier;
    bool m_TestOutputLut;
    bool m_TestInputLut;
    bool m_IncludeCrc;
    LwDispCoreChnContext *m_pCoreContext;
    LwDispBasicWindowSanityNotifierSettings m_CoreNotifierSettings;
    LwDispBasicWindowSanityNotifierSettings m_WindowNotifierSettings[MAX_WINDOWS];
    LwDispChnContext *m_pCoreChCtx;
    vector <LwDispChnContext *>m_pWinChCtx;
    vector <LwDispChnContext *>m_pLwrsorChCtx;
    vector<DisplayChannel *> m_pWinChs;
    DisplayChannel *m_pCoreCh;
    vector<DisplayChannel *> m_pLwrsorChs;
    UINT32 m_TimeoutMultiplier;
    string m_rastersize;
};

LwDispBasicWindowSanity::LwDispBasicWindowSanity()
{
    m_pDisplay = GetDisplay();
    pLwDisplay = m_pDisplay->GetLwDisplay();
    m_UseDebugPortChannel = false;
    m_TestLwrsor = false;
    m_UseWindowNotifier = false;
    m_TestOutputLut = false;
    m_TestInputLut = false;
    m_IncludeCrc = false;
    m_pCoreContext = NULL;
    m_rastersize = "";
    SetName("LwDispBasicWindowSanity");
    m_TimeoutMultiplier = 2;
    m_pWinChCtx.resize(LwDispBasicWindowSanity::HEADS_SIZE);
    m_pLwrsorChCtx.resize(LwDispBasicWindowSanity::HEADS_SIZE);
    m_pWinChs.resize(LwDispBasicWindowSanity::WINDOWS_SIZE);
    m_pLwrsorChs.resize(LwDispBasicWindowSanity::HEADS_SIZE);
}

//! \brief LwDispBringupPass1 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDispBasicWindowSanity::~LwDispBasicWindowSanity()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDispBasicWindowSanity::IsTestSupported()
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
RC LwDispBasicWindowSanity::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

RC LwDispBasicWindowSanity::SetupOR
(
    DisplayChannel *pCh,
    DisplayID displayId,
    UINT32 head,
    bool   bDetach,
    LwDispORSettings *pORSettings
)
{
    RC rc;
    MASSERT(pORSettings);

    LwDispSORSettings *pSORSettings = (LwDispSORSettings *)pORSettings;

    UINT32 CrcMode =
        pORSettings->CrcActiveRasterOnly ?
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_CRC_MODE_ACTIVE_RASTER :
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_CRC_MODE_COMPLETE_RASTER;

    // Setting Default for now need to find out how to determine the PixelDepth
    UINT32 PixelDepth = LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_24_444;

    UINT32 HSyncPolNeg =
         pORSettings->HSyncPolarityNegative ?
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_HSYNC_POLARITY_NEGATIVE_TRUE :
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_HSYNC_POLARITY_POSITIVE_TRUE;

    UINT32 VSyncPolNeg =
         pORSettings->VSyncPolarityNegative ?
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_VSYNC_POLARITY_NEGATIVE_TRUE :
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_VSYNC_POLARITY_POSITIVE_TRUE;

    if (!((pORSettings->protocol == LWC37D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A) ||
        (pORSettings->protocol == LWC37D_SOR_SET_CONTROL_PROTOCOL_DUAL_TMDS)))
    {
        Printf(Tee::PriHigh, " ERROR : Aborting test as this test only supports SINGLE_TMDS_A \n");
        return RC::SOFTWARE_ERROR;
    }

    // SO this OR is attached to One of the head
    if (bDetach)
    {
        pSORSettings->OwnerMask ^= (1 << head);
        Printf(Tee::PriHigh, "Detaching  SOR: %d from head : %d, OwnerMask : %d \n ",
            pORSettings->ORNumber, head, pSORSettings->OwnerMask);
    }
    else // Attach
    {
        pSORSettings->OwnerMask  = 1 << head;
        Printf(Tee::PriHigh, "Attaching  SOR: %d to head : %d, OwnerMask : %d \n ",
            pORSettings->ORNumber, head, pSORSettings->OwnerMask);
    }

    UINT32 data = 0;

    data = FLD_SET_DRF_NUM(C37D, _SOR_SET_CONTROL, _OWNER_MASK, pSORSettings->OwnerMask, data);
    data = FLD_SET_DRF_NUM(C37D, _SOR_SET_CONTROL, _PROTOCOL, pORSettings->protocol, data);

    if (!bDetach)
    {
        data = FLD_SET_DRF(C37D, _SOR_SET_CONTROL, _DE_SYNC_POLARITY,
                           _POSITIVE_TRUE, data);
        data = FLD_SET_DRF(C37D, _SOR_SET_CONTROL, _PIXEL_REPLICATE_MODE,
                           _OFF, data);
    }

    CHECK_RC(pCh->Write(LWC37D_SOR_SET_CONTROL(pORSettings->ORNumber), data));

    if (!bDetach)
    {
        CHECK_RC(pCh->Write(
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE(head),
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _CRC_MODE, CrcMode) |
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _HSYNC_POLARITY, HSyncPolNeg) |
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _VSYNC_POLARITY, VSyncPolNeg) |
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _PIXEL_DEPTH, PixelDepth) ));

        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_DISPLAY_ID(head, 0), (UINT32) displayId));
        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_DISPLAY_ID(head, 1), 0));
    }
    else
    {
        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_DISPLAY_ID(head, 0), 0));
        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_DISPLAY_ID(head, 1), 0));
    }

    return rc;
}

RC LwDispBasicWindowSanity::SetupRaster
(
    DisplayChannel *pCh,
    UINT32 head,
    LwDispRasterSettings *pRasterSettings
)
{
    MASSERT(pRasterSettings);
    RC rc;

    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_RASTER_SIZE(head),
                        DRF_NUM(C37D, _HEAD_SET_RASTER_SIZE, _WIDTH, pRasterSettings->RasterWidth) |
                        DRF_NUM(C37D, _HEAD_SET_RASTER_SIZE, _HEIGHT, pRasterSettings->RasterHeight)));

    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_RASTER_SYNC_END(head),
                        DRF_NUM(C37D, _HEAD_SET_RASTER_SYNC_END, _X, pRasterSettings->SyncEndX) |
                        DRF_NUM(C37D, _HEAD_SET_RASTER_SYNC_END, _Y, pRasterSettings->SyncEndY)));

    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_RASTER_BLANK_START(head),
                        DRF_NUM(C37D, _HEAD_SET_RASTER_BLANK_START, _X, pRasterSettings->BlankStartX) |
                        DRF_NUM(C37D, _HEAD_SET_RASTER_BLANK_START, _Y, pRasterSettings->BlankStartY)));

    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_RASTER_BLANK_END(head),
                        DRF_NUM(C37D, _HEAD_SET_RASTER_BLANK_END, _X, pRasterSettings->BlankEndX) |
                        DRF_NUM(C37D, _HEAD_SET_RASTER_BLANK_END, _Y, pRasterSettings->BlankEndY)));

    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_RASTER_VERT_BLANK2(head),
                        DRF_NUM(C37D, _HEAD_SET_RASTER_VERT_BLANK2, _YSTART, pRasterSettings->Blank2StartY) |
                        DRF_NUM(C37D, _HEAD_SET_RASTER_VERT_BLANK2, _YEND, pRasterSettings->Blank2EndY)));

    return rc;
}

RC LwDispBasicWindowSanity::SetupPixelClock
(
    DisplayChannel *pCh,
    UINT32 head,
    LwDispPixelClockSettings *pPixelClkSettings
)
{
    MASSERT(pPixelClkSettings);

    RC rc;
    UINT32  Adj1000Div1001 = 0;
    Adj1000Div1001 = pPixelClkSettings-> bAdj1000Div1001 ?
                     LWC37D_HEAD_SET_PIXEL_CLOCK_FREQUENCY_ADJ1000DIV1001_FALSE:
                     LWC37D_HEAD_SET_PIXEL_CLOCK_FREQUENCY_ADJ1000DIV1001_TRUE;

    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_PIXEL_CLOCK_FREQUENCY(head),
        DRF_NUM(C37D, _HEAD_SET_PIXEL_CLOCK_FREQUENCY, _HERTZ, pPixelClkSettings->pixelClkFreqHz) |
        DRF_NUM(C37D, _HEAD_SET_PIXEL_CLOCK_FREQUENCY, _ADJ1000DIV1001, Adj1000Div1001)));

    return rc;
}

//! \brief Run the test
//!
//! It will Init LwDisplay HW and allocate a core and window channel.
//------------------------------------------------------------------------------
RC LwDispBasicWindowSanity::Run()
{
    RC rc = OK;
    vector <WindowIndex>hWin;
    DisplayIDs workingSet;
    DisplayIDs DisplaysToUse;
    DisplayID  setModeDisplay;
    vector <LwDispWindowSettings> winSettings;
    LwDispViewPortSettings viewPortSettings;
    UINT32 ctxDmaHandle0;
    UINT32 offset0;
    LwDispCRCSettings testCRCSettings;
    LwDispLutSettings InputLutSettings;
    LwDispLutSettings OutputLutSettings;
    LwU32 headUsageBound = 0;
    LwU32 windowFormatUsageBound = 0;
    LwU32 windowUsageBound = 0;

    winSettings.resize(LwDispBasicWindowSanity::WINDOWS_SIZE);
    hWin.resize(LwDispBasicWindowSanity::WINDOWS_SIZE);

    // allocate core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    //
    // As of now, we just have SINGLE_TMDS_A support on fmodel and this is always
    // forcefully detected on emu/fmodel so we dont need to fake the TMDS display.
    //
    CHECK_RC(m_pDisplay->GetDetected(&DisplaysToUse));
    Printf(Tee::PriHigh, "Detected displays  = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, DisplaysToUse);

    if (DisplaysToUse.size() == 0)
    {
        Printf(Tee::PriHigh, "There is no display detected!\n");
        return RC::SOFTWARE_ERROR;
    }

    for (UINT32 i = 0; i < DisplaysToUse.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, DisplaysToUse[i], "SINGLE_TMDS_A"))
        {
            //
            // if the edid is not valid for disp id then SetLwstomEdid.
            // This is needed on emulation / simulation / silicon (when kvm used)
            //
            if (! (DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, DisplaysToUse[i]) &&
                   DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, DisplaysToUse[i]))
               )
            {
                if((rc = DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, DisplaysToUse[i], true)) != OK)
                {
                    Printf(Tee::PriLow,"SetLwstomEdid failed for displayID = 0x%X \n",
                        (UINT32)DisplaysToUse[i]);
                    return rc;
                }
            }
            workingSet.push_back(DisplaysToUse[i]);
            break;
        }
    }

    if (workingSet.size() == 0)
    {
        Printf(Tee::PriHigh, "There is no TMDS display detected! Check VBIOS...\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 width, height, refreshRate;
    LwDispRasterSettings rasterSettings;
    UINT32 windowNum = 0;
    UINT32 lwrsorNum = 0;
    UINT32 head = 0;
    bool   useSmallRaster = false;

    if (GetBoundGpuSubdevice()->IsDFPGA())
    {
        // Sepcial Configuration for TMDS_A
        windowNum = 4;
        head = 2;
        lwrsorNum = 2;
    }

    if (m_rastersize.compare("") == 0)
    {
        useSmallRaster = (Platform::GetSimulationMode() != Platform::Hardware);
    }
    else
    {
        useSmallRaster = (m_rastersize.compare("small") == 0);
        if (!useSmallRaster && m_rastersize.compare("large") != 0)
        {
            // Raster size param must be either "small" or "large".
            Printf(Tee::PriHigh, "%s: Bad raster size argument: \"%s\". Treating as \"large\"\n", __FUNCTION__, m_rastersize.c_str());
        }
    }

    if (useSmallRaster)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        width    = 160;
        height   = 120;
        refreshRate  = 60;
    }
    else
    {
        width    = 800;
        height   = 600;
        refreshRate = 60;
    }

    if (width == 160 && height == 120)
    {
        // Use special debug raster
        // these are self Determined Raster Settings  Useful for Debug Purpose
        // which will make CRC and other captures fast ..
        //
        rasterSettings.RasterWidth  = width + 26;
        rasterSettings.ActiveX      = width;
        rasterSettings.SyncEndX     = 15;
        rasterSettings.BlankStartX  = rasterSettings.RasterWidth - 5;
        rasterSettings.BlankEndX    = rasterSettings.SyncEndX + 4;
        rasterSettings.PolarityX    = false;
        rasterSettings.RasterHeight = height + 30;
        rasterSettings.ActiveY      = height;
        rasterSettings.SyncEndY     = 0;
        rasterSettings.BlankStartY  = rasterSettings.RasterHeight - 2;
        rasterSettings.BlankEndY    = rasterSettings.SyncEndY + 24;
        rasterSettings.Blank2StartY = 1;
        rasterSettings.Blank2EndY   = 0;
        rasterSettings.PolarityY    = false;
        rasterSettings.Interlaced   = false;
        // 165 MHZ
        rasterSettings.PixelClockHz = 165000000;
        rasterSettings.Dirty = true;
    }
    else
    {
        CHECK_RC(ModesetUtils::GetDispLwstomTiming(m_pDisplay, workingSet[0], width, height, refreshRate,
            Display::AUTO, &rasterSettings, 0, NULL));
        if (GetBoundGpuSubdevice()->IsDFPGA())
        {
            // 38 MHZ
            rasterSettings.PixelClockHz = 38000000;
        }
    }

    if (!GetBoundGpuSubdevice()->IsDFPGA())
    {
        // Update RM settings for SOR XBAR
        vector<UINT32>sorList;
        UINT32 sorExcludeMask = 0;
        CHECK_RC(m_pDisplay->AssignSOR(DisplayIDs(1, workingSet[0]),
            sorExcludeMask, sorList, Display::AssignSorSetting::ONE_HEAD_MODE));
        for (UINT32 i = 0; i < OBJ_MAX_SORS; i++)
        {
            if ((workingSet[0] & sorList[i]) == workingSet[0])
            {
                Printf(Tee::PriHigh, "Using SOR = %d\n", i);
                break;
            }
        }
    }

    // allocate window instance 0
    CHECK_RC(pLwDisplay->AllocateWinAndWinImm(&hWin[windowNum], head, windowNum));
    if (m_TestLwrsor)
    {
        CHECK_RC(pLwDisplay->AllocateLwrsorImm(lwrsorNum));
    }

    if (m_UseDebugPortChannel)
    {
        m_pCoreCh = new DebugPortDisplayChannelC3(LW_PDISP_CHN_NUM_CORE, m_pDisplay->GetOwningDevice());
        m_pWinChs[windowNum] = new DebugPortDisplayChannelC3(LW_PDISP_CHN_NUM_WIN(windowNum), m_pDisplay->GetOwningDevice());
        if (m_TestLwrsor)
            m_pLwrsorChs[lwrsorNum] = new DebugPortDisplayChannelC3(LW_PDISP_CHN_NUM_LWRS(lwrsorNum), m_pDisplay->GetOwningDevice());
    }
    else
    {
        // get display channel for window instance 0 and core channel
        CHECK_RC(pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID, &m_pCoreChCtx, LwDisplay::ILWALID_WINDOW_INSTANCE, head));
        CHECK_RC(pLwDisplay->GetLwDisplayChnContext(Display::WINDOW_CHANNEL_ID, &m_pWinChCtx[windowNum], windowNum, head));
        if (m_TestLwrsor)
        {
            CHECK_RC(pLwDisplay->GetLwDisplayChnContext(Display::LWRSOR_IMM_CHANNEL_ID,
                                    &m_pLwrsorChCtx[lwrsorNum],
                                    0,
                                    head));
            m_pLwrsorChs[lwrsorNum] = m_pLwrsorChCtx[lwrsorNum]->GetChannel();
        }
        m_pCoreCh = m_pCoreChCtx->GetChannel();
        m_pWinChs[windowNum] = m_pWinChCtx[windowNum]->GetChannel();
    }

    winSettings[0].winIndex = hWin[windowNum];
    winSettings[0].image.pkd.pWindowImage = new Surface2D;
    winSettings[0].SetSizeOut(width, height);

    CHECK_RC(SetupSurface(setModeDisplay, &winSettings[0], m_pWinChs[windowNum]));
    ctxDmaHandle0 = winSettings[0].image.pkd.pWindowImage->GetCtxDmaHandle();
    offset0 = LwU64_LO32(winSettings[0].image.pkd.pWindowImage->GetCtxDmaOffsetIso() >> 8);

    // Send 0x8BADF00D to win4 LWD_WINDOW_SET_UPDATE_TIMESTAMP_LO__ADDR,
    CHECK_RC(SetTimestampLo(m_pWinChs[windowNum], 0x8BADF00D));

    // Send depth=0 to win4 LWD_WINDOW_SET_COMPOSITION_CONTROL__ADDR,
    CHECK_RC(SetCompositionCtrl(m_pWinChs[windowNum], 0));

    // Configure win0 size and pos so that they partially overlap.
    winSettings[0].SetPointSizeIlwalues(0, 0, width, height);
    winSettings[0].SetSizeOut(width, height);

    CHECK_RC(SetWinSize(m_pWinChs[windowNum], &winSettings[0]));
    CHECK_RC(SetWinSizeIn(m_pWinChs[windowNum], &winSettings[0]));
    CHECK_RC(SetWinSizeOut(m_pWinChs[windowNum], &winSettings[0]));
    windowUsageBound = FLD_SET_DRF_NUM(C37D, _WINDOW_SET_WINDOW_USAGE_BOUNDS, _MAX_PIXELS_FETCHED_PER_LINE, width, windowUsageBound);

    //  Send the ContextDma value in SetupChannelImage to win4 LWD_WINDOW_SET_CONTEXT_DMA_ISO__ADDR(0)
    CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_SET_OFFSET(0),
            DRF_NUM(C37E, _SET_OFFSET, _ORIGIN, offset0)));

    CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_SET_CONTEXT_DMA_ISO(0),
            DRF_NUM(C37E, _SET_CONTEXT_DMA_ISO, _HANDLE, ctxDmaHandle0)));

    CHECK_RC(SetupRaster(m_pCoreCh, head, &rasterSettings));

    //Set Pixel Clock value
    LwDispPixelClockSettings pixelClockSettings;
    pixelClockSettings.pixelClkFreqHz = rasterSettings.PixelClockHz;
    pixelClockSettings.bAdj1000Div1001 = false;
    CHECK_RC(SetupPixelClock(m_pCoreCh, head, &pixelClockSettings));

    // Send core raster parameter controlling methods,
    viewPortSettings.SetViewportIn(0, 0, width, height);
    viewPortSettings.SetViewportOut(0, 0, width, height);

    CHECK_RC(SetupViewportIn(m_pCoreCh, head, &viewPortSettings));

    CHECK_RC(SetupViewportOut(m_pCoreCh, head, &viewPortSettings));

    LwDispColor desktopColor;

    desktopColor.Alpha = 0xff;
    desktopColor.Red = 0;
    desktopColor.Green = 0;
    desktopColor.Blue = 0;

    // Set Core.Head[0].SetDefaultColor on head 0
    CHECK_RC(SetupDesktopColor(m_pCoreCh, head, desktopColor));

    // Send Core.Window[0].SetControl.OwnerMask to attach head0,
    CHECK_RC(WindowSetOwner(m_pCoreCh, hWin[windowNum], head));

     // Ask RM which OR should be used (and protocol) for this Display
    UINT32 orIndex    = 0;
    UINT32 orType     = 0;
    UINT32 orProtocol = 0;

    CHECK_RC(m_pDisplay->GetORProtocol(workingSet[0], &orIndex, &orType, &orProtocol));
    UINT32 protocol = 0;
    CHECK_RC(GetProtocol(orProtocol, &protocol));

    LwDispSORSettings sorSettings;
    sorSettings.ORNumber  = orIndex;
    sorSettings.OwnerMask = 0;
    sorSettings.protocol = protocol;
    sorSettings.CrcActiveRasterOnly = false;
    sorSettings.HSyncPolarityNegative = false;
    sorSettings.VSyncPolarityNegative = false;

    CHECK_RC(SetupOR(m_pCoreCh, workingSet[0], head, false, &sorSettings));

    if (m_TestLwrsor)
    {
        LwDispLwrsorSettings lwrsorSettings;
        lwrsorSettings.hotSpotX = 0;
        lwrsorSettings.hotSpotY = 0;

        // Setting the cursor position to (0,0) for now. Change this once HW bug is resolved
        lwrsorSettings.hotSpotPointOutX = width/2;
        lwrsorSettings.hotSpotPointOutY = height/2;
        lwrsorSettings.pLwrsorSurface = new Surface2D;

        CHECK_RC(VerifyLwrsorSupport(setModeDisplay, head, m_pCoreCh, m_pLwrsorChs[lwrsorNum], &lwrsorSettings));
    }

    /* The sequence followed for notifier is :
     * 1. Setup OR and other parameters
     * 2. Init/Setup Core Notifier
     * 3. Send core and window update; Flush updates
     * 4. Poll for core notifier FINISHED
     * 5. Set notifier to NULL
     * 6. Send update Flush
     * 7. poll for window notifier FINISHED
     *
     * 8. Dettach head
     * 9. Setup core and window notifier with different offset
     * 10. Clear notifier state
     * 11. Send core + window update; Flush updates
     * 12. Poll for core completeion FINISHED
     * 13. Set notifier to NULL
     * 14. Send update Flush
     * 15  Poll for window notifier FINISHED
     */

    if (!m_UseDebugPortChannel)
    {
        CHECK_RC(InitCoreNotifierSettings(m_pCoreChCtx));
        CHECK_RC(InitWindowNotifierSettings(m_pWinChCtx[windowNum], windowNum, 0));

        CHECK_RC(SetupCoreCompletionNotifier(m_pCoreCh, m_pCoreChCtx));
        CHECK_RC(SetupWindowCompletionNotifier(m_pWinChCtx[windowNum]->GetChannel(), m_pWinChCtx[windowNum], windowNum));
    }

    if (m_TestInputLut)
    {
        CHECK_RC(VerifyLut(workingSet[0], m_pWinChCtx[windowNum], head, &InputLutSettings));
        CHECK_RC(SetInputLUT(workingSet[0], head, m_pWinChCtx[windowNum]->GetChannel(), InputLutSettings));
        windowUsageBound |= DRF_DEF(C37D, _WINDOW_SET_WINDOW_USAGE_BOUNDS, _INPUT_LUT, _USAGE_257);
    }

    if (m_TestOutputLut)
    {
        CHECK_RC(VerifyLut(workingSet[0], m_pCoreChCtx, head, &OutputLutSettings));
        CHECK_RC(SetOutputLUT(workingSet[0], head, m_pCoreCh, OutputLutSettings));
        headUsageBound |= DRF_DEF(C37D, _HEAD_SET_HEAD_USAGE_BOUNDS, _OUTPUT_LUT, _USAGE_257);
    }

    // Setup the usage bounds
    if (m_TestLwrsor)
        headUsageBound |= DRF_DEF(C37D, _HEAD_SET_HEAD_USAGE_BOUNDS, _LWRSOR, _USAGE_W32_H32);

    /* CRC */
    if(m_IncludeCrc)
    {
        //Before 1st update do -
        // 1. Setup CRC buffer
        // 2. Start running CRC which does -
        //      - Write LWC37D_HEAD_SET_CONTEXT_DMA_CRC
        //      - Fill memory with 0
        //      - Write LWC37D_HEAD_SET_CRC_CONTROL
        CHECK_RC(SetupCrcBuffer(m_pCoreChCtx, head, &testCRCSettings));
        CHECK_RC(StartRunningCrc(m_pCoreChCtx, m_pCoreCh, head, &testCRCSettings));
    }

    // Setup all the usage-bounds
    if (headUsageBound)
        CHECK_RC(m_pCoreCh->Write(LWC37D_HEAD_SET_HEAD_USAGE_BOUNDS(head), headUsageBound));
    if (windowUsageBound)
    {
        // Force scalar taps to 2 always
        windowUsageBound |= DRF_DEF(C37D, _WINDOW_SET_WINDOW_USAGE_BOUNDS, _INPUT_SCALER_TAPS, _TAPS_2);
        CHECK_RC(m_pCoreCh->Write(LWC37D_WINDOW_SET_WINDOW_USAGE_BOUNDS(windowNum), windowUsageBound));
    }
    windowFormatUsageBound |= DRF_DEF(C37D, _WINDOW_SET_WINDOW_FORMAT_USAGE_BOUNDS, _RGB_PACKED4BPP, _TRUE);
    if (windowFormatUsageBound)
        CHECK_RC(m_pCoreCh->Write(LWC37D_WINDOW_SET_WINDOW_FORMAT_USAGE_BOUNDS(windowNum), windowFormatUsageBound));

    // Send Core.Update
    m_pCoreCh->Write(LWC37D_UPDATE, DRF_DEF(C37D, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE));

    // Send win0.upd
    CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_UPDATE, DRF_DEF(C37E, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE)));

    if (m_TestLwrsor)
    {
        m_pLwrsorChs[lwrsorNum]->Write(LWC37A_UPDATE, DRF_DEF(C37A, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE));
    }

    CHECK_RC(m_pCoreCh->Flush());

    if (m_TestLwrsor)
    {
        CHECK_RC(m_pLwrsorChs[lwrsorNum]->Flush());
    }

    if (!m_UseDebugPortChannel)
    {
        CHECK_RC(POLLWRAP(
            m_CoreNotifierSettings.PollNotifierDone,
            &m_CoreNotifierSettings,
            3 * m_pCoreChCtx->GetChannel()->GetDefaultTimeoutMs()));
    }
    else
    {
        Tasker::Sleep(100*1000);
    }
    //
    // Make sure window flush is done after core-notifier completes to ensure that usage-bounds are set
    // and no cross channel error checks trigger
    //
    if (m_UseWindowNotifier)
    {
        CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_SET_CONTEXT_DMA_NOTIFIER, 0));

        // Send win0.upd
        CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_UPDATE, DRF_DEF(C37E, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE)));
        CHECK_RC(m_pWinChs[windowNum]->Flush());

        // Window notifiers are not supported in fmodel yet
        CHECK_RC(POLLWRAP(
            m_WindowNotifierSettings[windowNum].PollNotifierDone,
            &m_WindowNotifierSettings[windowNum],
            3 * m_pWinChs[windowNum]->GetDefaultTimeoutMs()));
    }
    else
    {
        CHECK_RC(m_pWinChs[windowNum]->Flush());
        Tasker::Sleep(100*1000);
    }

    /* CRC */
    if(m_IncludeCrc)
    {
        // After 1st update and before 2nd update do -
        // 1. Setup LWC37D_HEAD_SET_CONTEXT_DMA_CRC to 0
        CHECK_RC(PrepPollCrcNotifier(m_pCoreCh, m_pWinChs[windowNum], head, Display::WINDOW_CHANNEL_ID));
    }

    //************* Detach Display ******************************//

    CHECK_RC(SetupOR(m_pCoreCh, workingSet[0], head, true, &sorSettings));

    if (!m_UseDebugPortChannel)
    {
        /*Setup notifier with different offset */
        CHECK_RC(InitCoreNotifierSettings(m_pCoreChCtx, 1/*new_offset*/));
        CHECK_RC(InitWindowNotifierSettings(m_pWinChCtx[windowNum], windowNum, 1/*new_offset*/));

        CHECK_RC(SetupCoreCompletionNotifier(m_pCoreCh, m_pCoreChCtx, 1/*new_offset*/));
        CHECK_RC(SetupWindowCompletionNotifier(m_pWinChs[windowNum], m_pWinChCtx[windowNum], windowNum, 1/*new_offset*/));

        /* Clear the current state*/
        MEM_WR32(m_CoreNotifierSettings.CompletionAddress, 0);
        MEM_WR32(m_WindowNotifierSettings[windowNum].CompletionAddress, 0);
    }

    /* Send Core.Update */
    m_pCoreCh->Write(LWC37D_UPDATE, DRF_DEF(C37D, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE));

    /* Send win0.upd */
    CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_UPDATE, DRF_DEF(C37E, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE)));

    CHECK_RC(m_pCoreCh->Flush());
    CHECK_RC(m_pWinChs[windowNum]->Flush());

    if (!m_UseDebugPortChannel)
    {
        /*Poll for completion */
        CHECK_RC(POLLWRAP(
            m_CoreNotifierSettings.PollNotifierDone,
            &m_CoreNotifierSettings,
            3 * m_pCoreChCtx->GetChannel()->GetDefaultTimeoutMs()));

        if (m_UseWindowNotifier)
        {
           CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_SET_CONTEXT_DMA_NOTIFIER, 0));

           // Send win0.upd
           CHECK_RC(m_pWinChs[windowNum]->Write(LWC37E_UPDATE, DRF_DEF(C37E, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE)));
           CHECK_RC(m_pWinChs[windowNum]->Flush());

            CHECK_RC(POLLWRAP(
                m_WindowNotifierSettings[windowNum].PollNotifierDone,
                &m_WindowNotifierSettings[windowNum],
                3 * m_pWinChCtx[windowNum]->GetChannel()->GetDefaultTimeoutMs()));
        }
    }
    else
    {
      Tasker::Sleep(100*1000);
    }

    if(m_IncludeCrc)
    {
        CrcComparison crcCompObj;
        char crcFileName[80];
        string prefix = "LwDispTest_CRC";

        sprintf(crcFileName, "%s_%dx%d", prefix.c_str(), width, height);
        if(m_TestLwrsor)
        {
            sprintf(crcFileName, "%s_lwrsor", crcFileName);
        }
        if(m_TestOutputLut)
        {
            sprintf(crcFileName, "%s_outputLUT", crcFileName);
        }
        if(m_TestInputLut)
        {
            sprintf(crcFileName, "%s_inputLUT", crcFileName);
        }

        string goldenCRC    = "dispinfradata/LwDispTest/CRC/" + string(crcFileName) + ".xml";
        string crcOutputLog = string(crcFileName) + "_output.log";
        sprintf(crcFileName, "%s.xml", crcFileName);

        // After 2nd update do -
        // 1. Poll for CRC completion
        CHECK_RC(PollCrcCompletion(&testCRCSettings));
        CHECK_RC(GetCrcValues(head, &testCRCSettings));

        CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), crcFileName, &testCRCSettings, true));
        CHECK_RC(crcCompObj.CompareCrcFiles(goldenCRC.c_str(), crcFileName, crcOutputLog.c_str()));
    }

    if (m_TestLwrsor)
    {
        CHECK_RC(pLwDisplay->DestroyLwrsorImm(head));
    }
    CHECK_RC(pLwDisplay->DestroyWinAndWinImm(hWin[windowNum]));

    return rc;
}

RC LwDispBasicWindowSanity::SetupCrcBuffer
(
    LwDispChnContext  *m_pCoreChCtx,
    UINT32             Head,
    LwDispCRCSettings *pCrcSettings
)
{
    RC     rc;

    pCrcSettings->pCrcBuffer = new Surface2D();
    pCrcSettings->pCrcBuffer->SetWidth(LWD_BASICWIN_DEFAULT_SIZE);
    pCrcSettings->pCrcBuffer->SetPitch(LWD_BASICWIN_DEFAULT_SIZE);
    pCrcSettings->pCrcBuffer->SetHeight(1);
    pCrcSettings->pCrcBuffer->SetColorFormat(ColorUtils::Y8);
    pCrcSettings->pCrcBuffer->SetProtect(Memory::ReadWrite);
    pCrcSettings->pCrcBuffer->SetDisplayable(true);
    pCrcSettings->pCrcBuffer->SetPhysContig(true);

    const Memory::Location finalLoc =
        m_pDisplay->GetOwningDisplaySubdevice()->FixOptimalLocation(
            pCrcSettings->MemLocation);

    if ((finalLoc == Memory::Fb) ||
        (finalLoc == Memory::NonCoherent))
    {
        pCrcSettings->pCrcBuffer->SetLocation(Memory::Fb);
    }
    else
    {
        pCrcSettings->pCrcBuffer->SetLocation(Memory::Coherent);
        pCrcSettings->pCrcBuffer->SetAddressModel(Memory::Segmentation);

        CHECK_RC(m_pCoreChCtx->GetDMAChanInfo()->DispChanCfg.ApplySystemModelFlags(
                    Memory::Coherent, pCrcSettings->pCrcBuffer));
    }

    CHECK_RC(pCrcSettings->pCrcBuffer->Alloc(m_pDisplay->GetOwningDevice()));
    CHECK_RC(pCrcSettings->pCrcBuffer->Map(0));

    pCrcSettings->NotifierSettings.CompletionAddress =
            (UINT08*)(pCrcSettings->pCrcBuffer->GetAddress()) +
             LWC37D_NOTIFIER_CRC_STATUS_0;

    pCrcSettings->NotifierSettings.PollNotifierDone = LwDispBasicWindowSanity::PollCrcNotifierComplete;
    pCrcSettings->NotifierSettings.NumNotifiers = 1;

    return rc;
}

RC LwDispBasicWindowSanity::PrepPollCrcNotifier
(
    DisplayChannel     *m_pCoreCh,
    DisplayChannel     *pWinCh,
    UINT32              Head,
    Display::ChannelID  chID
)
{
    RC rc;

    switch (chID)
    {
        case Display::WINDOW_CHANNEL_ID:
            CHECK_RC(pWinCh->Write(LWC37E_SET_PRESENT_CONTROL,
            DRF_NUM(C37E, _SET_PRESENT_CONTROL, _MIN_PRESENT_INTERVAL, 1)    |
            DRF_DEF(C37E, _SET_PRESENT_CONTROL, _BEGIN_MODE, _NON_TEARING)    |
            DRF_DEF(C37E, _SET_PRESENT_CONTROL, _TIMESTAMP_MODE, _DISABLE)));
            break;
        default :
            break;
    }

    // Send a method that Disconnects the notifier from the head:
    CHECK_RC(m_pCoreCh->Write(LWC37D_HEAD_SET_CONTEXT_DMA_CRC(Head), 0));

    return rc;
}

RC LwDispBasicWindowSanity::StartRunningCrc
(
    LwDispChnContext   *m_pCoreChCtx,
    DisplayChannel     *m_pCoreCh,
    UINT32              Head,
    LwDispCRCSettings  *pCrcSettings
)
{
    MASSERT(pCrcSettings);

    RC rc;

    // this will Start the CRC ..
    if (pCrcSettings->pCrcBuffer->GetLocation() == Memory::Coherent )
    {
        CHECK_RC(pCrcSettings->pCrcBuffer->
                 BindGpuChannel(m_pCoreChCtx->GetDMAChanInfo()->hDispChan));

        CHECK_RC(m_pCoreCh->Write(LWC37D_HEAD_SET_CONTEXT_DMA_CRC(Head),
                 pCrcSettings->pCrcBuffer->GetCtxDmaHandle()));
    }
    else if(pCrcSettings->pCrcBuffer->GetLocation() == Memory::Fb)
    {
         CHECK_RC(pCrcSettings->pCrcBuffer->
                  BindIsoChannel(m_pCoreChCtx->GetDMAChanInfo()->hDispChan));

         CHECK_RC( m_pCoreCh->Write(LWC37D_HEAD_SET_CONTEXT_DMA_CRC(Head),
                  pCrcSettings->pCrcBuffer->GetCtxDmaHandleIso()));
    }

    Memory::Fill32(
                  pCrcSettings->pCrcBuffer->GetAddress(),
                  0,
                  (UINT32)pCrcSettings->pCrcBuffer->GetSize() / 4);

    // Set _PRIMARY_CRC to _NONE until fmodel bug is fixed
    pCrcSettings->CrcPrimaryOutput = (LwDispCRCSettings::CRC_OUTPUT)LWC37D_HEAD_SET_CRC_CONTROL_PRIMARY_CRC_NONE;
    pCrcSettings->CrcSecondaryOutput = (LwDispCRCSettings::CRC_OUTPUT)LWC37D_HEAD_SET_CRC_CONTROL_SECONDARY_CRC_NONE;
    pCrcSettings->ExpectBufferCollapse = LWC37D_HEAD_SET_CRC_CONTROL_EXPECT_BUFFER_COLLAPSE_FALSE;
    pCrcSettings->CrcDuringSnooze = LWC37D_HEAD_SET_CRC_CONTROL_CRC_DURING_SNOOZE_DISABLE;
    pCrcSettings->ColorDepthAgnosticEnable = 0;

   CHECK_RC(m_pCoreCh->Write(LWC37D_HEAD_SET_CRC_CONTROL(Head),
        DRF_NUM(C37D, _HEAD_SET_CRC_CONTROL, _PRIMARY_CRC, pCrcSettings->CrcPrimaryOutput) |
        DRF_NUM(C37D, _HEAD_SET_CRC_CONTROL, _SECONDARY_CRC, pCrcSettings->CrcSecondaryOutput) |
        DRF_NUM(C37D, _HEAD_SET_CRC_CONTROL, _CONTROLLING_CHANNEL, 0) |
        DRF_NUM(C37D, _HEAD_SET_CRC_CONTROL, _EXPECT_BUFFER_COLLAPSE, pCrcSettings->ExpectBufferCollapse) |
        DRF_NUM(C37D, _HEAD_SET_CRC_CONTROL, _CRC_DURING_SNOOZE, pCrcSettings->CrcDuringSnooze) |
        DRF_NUM(C37D, _HEAD_SET_CRC_CONTROL, _COLOR_DEPTH_AGNOSTIC, pCrcSettings->ColorDepthAgnosticEnable)));

   return rc;
}

RC LwDispBasicWindowSanity::PollCrcCompletion
(
    LwDispCRCSettings *pCrcSettings
)
{
    RC              rc;

    MASSERT(pCrcSettings);

    CHECK_RC( POLLWRAP( pCrcSettings->NotifierSettings.PollNotifierDone,
                    &pCrcSettings->NotifierSettings,
                    m_TimeoutMultiplier * Tasker::GetDefaultTimeoutMs()));

    return rc;
}

RC LwDispBasicWindowSanity::GetCrcValues
(
   UINT32 Head,
   LwDispCRCSettings *pCrcSettings
)
{
    MASSERT(pCrcSettings);
    RC rc;

    UINT32* pCrcBuffer = (UINT32 *)pCrcSettings->pCrcBuffer->GetAddress();
    UINT32 crcAddr =  LWC37D_NOTIFIER_CRC_CRC_ENTRY0_8;
    LwU32 compCRC = 0, priCRC =0, secCRC=0, rgCRC = 0;

    UINT32 numCrcs = DRF_VAL(C37D, _NOTIFIER_CRC, _STATUS_0_COUNT, MEM_RD32(pCrcBuffer));

    if (numCrcs < 1)
    {
        return RC::SOFTWARE_ERROR;
    }

    pCrcSettings->NumExpectedCRCs = numCrcs;

    // Dump CRC data for current Notifier
    for (UINT32 i = 0; i < numCrcs; i++)
    {
       compCRC = MEM_RD32(pCrcBuffer + crcAddr +
                   (LWC37D_NOTIFIER_CRC_CRC_ENTRY0_11 - LWC37D_NOTIFIER_CRC_CRC_ENTRY0_8));
       rgCRC   = MEM_RD32(pCrcBuffer + crcAddr +
                   (LWC37D_NOTIFIER_CRC_CRC_ENTRY0_12 - LWC37D_NOTIFIER_CRC_CRC_ENTRY0_8));
       priCRC  = MEM_RD32(pCrcBuffer + crcAddr +
                   (LWC37D_NOTIFIER_CRC_CRC_ENTRY0_13 - LWC37D_NOTIFIER_CRC_CRC_ENTRY0_8));
       secCRC  = MEM_RD32(pCrcBuffer + crcAddr +
                   (LWC37D_NOTIFIER_CRC_CRC_ENTRY0_14 - LWC37D_NOTIFIER_CRC_CRC_ENTRY0_8));

       crcAddr += (LWC37D_NOTIFIER_CRC_CRC_ENTRY0_15 - LWC37D_NOTIFIER_CRC_CRC_ENTRY0_8) + 1;
        Printf(Tee::PriHigh,
           "Captured LWD2.0 CRCs for Compositor=0x%08X, Rg=0x%08X, Primary=0x%08X Secondary=0x%08X\n",
           compCRC, rgCRC, priCRC, secCRC);
    }
    return rc;
}

bool LwDispBasicWindowSanity::PollCrcNotifierComplete(void *Args)
{
    LwDispNotifierSettings *pNotifierSettings = (LwDispNotifierSettings*)Args;

    UINT32 lwrVal = MEM_RD32(pNotifierSettings->CompletionAddress);

    bool done = (DRF_VAL(C37D, _NOTIFIER_CRC_STATUS_0, _DONE, lwrVal) ==
                 LWC37D_NOTIFIER_CRC_STATUS_0_DONE_TRUE);

    if (!done)
        return false;

    Printf(Tee::PriDebug, "NOTIFIER COMPLETE - value = 0x%08x \n", lwrVal);

    return true;
}

RC LwDispBasicWindowSanity::VerifyLut
(
    DisplayID display,
    LwDispChnContext *pChnCtx,
    UINT32 head,
    LwDispLutSettings *pLutSettings
)
{
    RC rc;
    vector<UINT32> TestLut;
    UINT32 j = 0;

    // Set palette of size 257
    for (j = 0; j < 257; j++)
    {
        UINT32 val = (256 - j) << 8;

        TestLut.push_back((val << 16) | (val));  // Green, Red
        TestLut.push_back(val);  // Blue
    }

    CHECK_RC(SetupLUTBuffer(display, head, pChnCtx, pLutSettings, &TestLut[0],(UINT32)TestLut.size()));

    return rc;
}

RC LwDispBasicWindowSanity::SetOutputLUT
(
    DisplayID Display,
    UINT32 Head,
    DisplayChannel *pCh,
    LwDispLutSettings& pLutSettings
)
{
    RC rc;

    if (pLutSettings.Disp20.dirty)
    {
        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_CONTROL_OUTPUT_LUT(Head),
            DRF_DEF(C37D, _HEAD_SET_CONTROL_OUTPUT_LUT, _SIZE, _SIZE_257) |
            DRF_DEF(C37D, _HEAD_SET_CONTROL_OUTPUT_LUT, _RANGE, _UNITY) |
            DRF_DEF(C37D, _HEAD_SET_CONTROL_OUTPUT_LUT, _OUTPUT_MODE, _INDEX)));

        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_OFFSET_OUTPUT_LUT(Head),
            DRF_NUM(C37D, _HEAD_SET_OFFSET_OUTPUT_LUT, _ORIGIN,
                    LwU64_LO32(pLutSettings.pLutBuffer->GetCtxDmaOffsetIso() >> 8))));

        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_CONTEXT_DMA_OUTPUT_LUT(Head),
            DRF_NUM(C37D, _HEAD_SET_CONTEXT_DMA_OUTPUT_LUT, _HANDLE,
                    LwU64_LO32(pLutSettings.pLutBuffer->GetCtxDmaHandle()))));
    }
    else
    {
        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_OFFSET_OUTPUT_LUT(Head),
            DRF_NUM(C37D, _HEAD_SET_OFFSET_OUTPUT_LUT, _ORIGIN, 0)));

        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_CONTEXT_DMA_OUTPUT_LUT(Head),
            DRF_NUM(C37D, _HEAD_SET_CONTEXT_DMA_OUTPUT_LUT, _HANDLE, 0)));
    }

    return rc;
}

RC LwDispBasicWindowSanity::SetInputLUT
(
    DisplayID Display,
    UINT32 Head,
    DisplayChannel *pCh,
    LwDispLutSettings& pLutSettings
)
{
    RC rc;

    if (pLutSettings.Disp20.dirty)
    {
        CHECK_RC(pCh->Write(LWC37E_SET_CONTROL_INPUT_LUT,
            DRF_DEF(C37E, _SET_CONTROL_INPUT_LUT, _SIZE, _SIZE_257) |
            DRF_DEF(C37E, _SET_CONTROL_INPUT_LUT, _RANGE, _UNITY) |
            DRF_DEF(C37E, _SET_CONTROL_INPUT_LUT, _OUTPUT_MODE, _INDEX)));

        CHECK_RC(pCh->Write(LWC37E_SET_OFFSET_INPUT_LUT,
            DRF_NUM(C37E, _SET_OFFSET_INPUT_LUT, _ORIGIN,
                    LwU64_LO32(pLutSettings.pLutBuffer->GetCtxDmaOffsetIso() >> 8))));

        CHECK_RC(pCh->Write(LWC37E_SET_CONTEXT_DMA_INPUT_LUT,
            DRF_NUM(C37E, _SET_CONTEXT_DMA_INPUT_LUT, _HANDLE,
                    LwU64_LO32(pLutSettings.pLutBuffer->GetCtxDmaHandle()))));
    }
    else
    {
        CHECK_RC(pCh->Write(LWC37E_SET_OFFSET_INPUT_LUT,
            DRF_NUM(C37E, _SET_OFFSET_INPUT_LUT, _ORIGIN, 0)));

        CHECK_RC(pCh->Write(LWC37E_SET_CONTEXT_DMA_INPUT_LUT,
            DRF_NUM(C37E, _SET_CONTEXT_DMA_INPUT_LUT, _HANDLE, 0)));
    }

    return rc;
}

RC LwDispBasicWindowSanity::SetupLUTBuffer
(
    DisplayID Display,
    UINT32  Head,
    LwDispChnContext *pChnCtx,
    LwDispLutSettings *pLutSettings,
    UINT32 *pData,
    UINT32 Size
)
{

    MASSERT(pData);
    RC rc;

    Surface2D *pLutBuffer = NULL;
    UINT32 SizeToCopy = 0;
    UINT32 *pLutBuf = NULL;

    if(!pLutSettings->pLutBuffer)
    {
        CHECK_RC(AllocSetupLutBuffer(pChnCtx, Head, pLutSettings));
    }

    pLutBuffer = pLutSettings->pLutBuffer;
    pLutBuf = (UINT32 *)pLutBuffer->GetAddress();
    Memory::Fill32(pLutBuf, 0, (UINT32)pLutBuffer->GetSize() / 4);
    SizeToCopy = min(Size, TransferFunctionEntrySize * TransferFunctionNumEntries / 4);
    for (UINT32 DataIdx = 0; DataIdx < SizeToCopy; DataIdx++)
    {
        MEM_WR32(pLutBuf, pData[DataIdx]);
        pLutBuf++;
    }
    pLutSettings->Disp20.dirty = true;

    return rc;
}

RC LwDispBasicWindowSanity::AllocSetupLutBuffer
(
    LwDispChnContext *pChnCtx,
    UINT32 Head,
    LwDispLutSettings *pLutSettings
)
{
    RC               rc;

    pLutSettings->pLutBuffer = new Surface2D();

    if (pLutSettings->pLutBuffer == NULL)
        return RC::CANNOT_ALLOCATE_MEMORY;

    Surface2D *pLutBuffer = pLutSettings->pLutBuffer;

    pLutBuffer->SetWidth(TransferFunctionNumEntries*TransferFunctionEntrySize);
    pLutBuffer->SetPitch(TransferFunctionNumEntries*TransferFunctionEntrySize);
    pLutBuffer->SetHeight(1);
    pLutBuffer->SetColorFormat(ColorUtils::Y8);
    pLutBuffer->SetLocation(Memory::Optimal);
    pLutBuffer->SetProtect(Memory::ReadWrite);
    pLutBuffer->SetDisplayable(true);
    pLutBuffer->SetPhysContig(true);

    CHECK_RC(pChnCtx->GetDMAChanInfo()->DispChanCfg.ApplySystemModelFlags(
                Memory::Optimal, pLutBuffer));

    CHECK_RC(pLutBuffer->Alloc(m_pDisplay->GetOwningDevice()));

    CHECK_RC(pLutBuffer->Map(0));
    CHECK_RC(pLutBuffer->BindIsoChannel(pChnCtx->GetDMAChanInfo()->hDispChan));

    return rc;
}

RC LwDispBasicWindowSanity::VerifyLwrsorSupport
(
    DisplayID setModeDisplay,
    UINT32 head,
    DisplayChannel *pCoreCh,
    DisplayChannel *pLwrsorCh,
    LwDispLwrsorSettings *pLwrsorSettings
)
{
    RC rc;

    UINT32 ctxDmaHandleLwrsor, offsetLwrsor;

    CHECK_RC(SetupLwrsorSurface(setModeDisplay, pLwrsorSettings, pCoreCh, head));

    ctxDmaHandleLwrsor = pLwrsorSettings->pLwrsorSurface->GetCtxDmaHandle();
    offsetLwrsor = LwU64_LO32(pLwrsorSettings->pLwrsorSurface->GetCtxDmaOffsetIso() >> 8);

    CHECK_RC(pCoreCh->Write(LWC37D_HEAD_SET_OFFSET_LWRSOR(head,0),
                DRF_NUM(C37D, _HEAD_SET_OFFSET_LWRSOR, _ORIGIN, offsetLwrsor)));

    CHECK_RC(pCoreCh->Write(LWC37D_HEAD_SET_CONTEXT_DMA_LWRSOR(head,0),
                DRF_NUM(C37D, _HEAD_SET_CONTEXT_DMA_LWRSOR, _HANDLE, ctxDmaHandleLwrsor)));

    CHECK_RC(pCoreCh->Write(LWC37D_HEAD_SET_PRESENT_CONTROL_LWRSOR(head),
                DRF_DEF(C37D, _HEAD_SET_PRESENT_CONTROL_LWRSOR, _MODE, _MONO)));

    CHECK_RC(pCoreCh->Write(LWC37D_HEAD_SET_CONTROL_LWRSOR(head),
                DRF_DEF(C37D, _HEAD_SET_CONTROL_LWRSOR, _ENABLE, _ENABLE) |
                DRF_DEF(C37D, _HEAD_SET_CONTROL_LWRSOR, _FORMAT, _A1R5G5B5) |
                DRF_DEF(C37D, _HEAD_SET_CONTROL_LWRSOR, _SIZE, _W32_H32) |
                DRF_NUM(C37D, _HEAD_SET_CONTROL_LWRSOR, _HOT_SPOT_X, pLwrsorSettings->hotSpotX) |
                DRF_NUM(C37D, _HEAD_SET_CONTROL_LWRSOR, _HOT_SPOT_Y, pLwrsorSettings->hotSpotY) |
                DRF_DEF(C37D, _HEAD_SET_CONTROL_LWRSOR, _DE_GAMMA, _NONE)));

    CHECK_RC(pCoreCh->Write(LWC37D_HEAD_SET_CONTROL_LWRSOR_COMPOSITION(head),
                            DRF_NUM(C37D, _HEAD_SET_CONTROL_LWRSOR_COMPOSITION, _K1, 255) |
                            DRF_DEF(C37D, _HEAD_SET_CONTROL_LWRSOR_COMPOSITION, _MODE, _BLEND) |
                            DRF_DEF(C37D, _HEAD_SET_CONTROL_LWRSOR_COMPOSITION, _LWRSOR_COLOR_FACTOR_SELECT, _K1_TIMES_SRC)
                 ));

    CHECK_RC(pCoreCh->Write(LWC37D_HEAD_SET_PRESENT_CONTROL_LWRSOR(head),
                DRF_DEF(C37D, _HEAD_SET_PRESENT_CONTROL_LWRSOR, _MODE, _MONO)));

    CHECK_RC(pLwrsorCh->Write(LWC37A_SET_LWRSOR_HOT_SPOT_POINT_OUT(head),
                    DRF_NUM(C37A, _SET_LWRSOR_HOT_SPOT_POINT_OUT, _X, pLwrsorSettings->hotSpotPointOutX) |
                    DRF_NUM(C37A, _SET_LWRSOR_HOT_SPOT_POINT_OUT, _Y, pLwrsorSettings->hotSpotPointOutY)));

    return rc;
}

RC LwDispBasicWindowSanity::InitCoreNotifierSettings
(
    LwDispChnContext* pChnContext,
    LwU32             offset
)
{
    RC rc;

    if((offset < 0) || (((offset+1) * LW_DISP_NOTIFIER_SIZEOF) > 0x1000/*notifier_size*/))
        return RC::SOFTWARE_ERROR;

    m_CoreNotifierSettings.NotifierSize = LW_DISP_NOTIFIER_SIZEOF;
    m_CoreNotifierSettings.CompletionAddress =
        (UINT08*)pChnContext->GetDMAChanInfo()->ChannelNotifier.GetAddress() + LW_DISP_NOTIFIER__0 +
        (offset * LW_DISP_NOTIFIER_SIZEOF);

    m_CoreNotifierSettings.PollNotifierDone = LwDispBasicWindowSanity::PollNotifierComplete;
    m_CoreNotifierSettings.Dirty = true;

    MEM_WR32(m_CoreNotifierSettings.CompletionAddress, 0);
    return rc;
}

RC LwDispBasicWindowSanity::InitWindowNotifierSettings
(
    LwDispChnContext* pChnContext,
    int               windowId,
    LwU32             offset
)
{
    RC rc;
    int i = windowId;

    if (i >= MAX_WINDOWS)
        return RC::SOFTWARE_ERROR;

    if((offset < 0) || (((offset+1) * LW_DISP_NOTIFIER_SIZEOF) > 0x1000/*notifier_size*/))
        return RC::SOFTWARE_ERROR;

    m_WindowNotifierSettings[i].NotifierSize = LW_DISP_NOTIFIER_SIZEOF;
    m_WindowNotifierSettings[i].CompletionAddress =
        (UINT08*)pChnContext->GetDMAChanInfo()->ChannelNotifier.GetAddress() + LW_DISP_NOTIFIER__0 +
        (offset * LW_DISP_NOTIFIER_SIZEOF);

    m_WindowNotifierSettings[i].PollNotifierDone = LwDispBasicWindowSanity::PollNotifierComplete;
    m_WindowNotifierSettings[i].Dirty = true;
    MEM_WR32(m_WindowNotifierSettings[i].CompletionAddress, 0);

    return rc;
}
RC LwDispBasicWindowSanity ::SetupCoreCompletionNotifier
(
    DisplayChannel   *pCh,
    LwDispChnContext* pChnContext,
    LwU32             offset
)
{
    RC     rc;
    MEM_WR32(m_CoreNotifierSettings.CompletionAddress, 0);

    if((offset < 0) || (((offset+1) * LW_DISP_NOTIFIER_SIZEOF) > 0x1000/*notifier_size*/))
        return RC::SOFTWARE_ERROR;

    CHECK_RC(pCh->Write(LWC37D_SET_CONTEXT_DMA_NOTIFIER, pChnContext->GetDMAChanInfo()->ChannelNotifier.GetCtxDmaHandleGpu()));

    //TODO: Add support for multi-GPU
    CHECK_RC(pCh->Write(LWC37D_SET_NOTIFIER_CONTROL,
            DRF_DEF(C37D, _SET_NOTIFIER_CONTROL, _MODE, _WRITE) |
            DRF_NUM(C37D, _SET_NOTIFIER_CONTROL, _OFFSET, offset) |
            DRF_DEF(C37D, _SET_NOTIFIER_CONTROL, _NOTIFY, _ENABLE)));

    return rc;
}

RC LwDispBasicWindowSanity ::SetupWindowCompletionNotifier
(
    DisplayChannel    *pCh,
    LwDispChnContext* pChnContext,
    int               windowId,
    LwU32             offset
)
{
    RC     rc;
    int i = windowId;

    if (i >= MAX_WINDOWS)
        return RC::SOFTWARE_ERROR;

    if((offset < 0) || (((offset+1) * LW_DISP_NOTIFIER_SIZEOF) > 0x1000/*notifier_size*/))
        return RC::SOFTWARE_ERROR;

    CHECK_RC(pCh->Write(LWC37E_SET_CONTEXT_DMA_NOTIFIER, pChnContext->GetDMAChanInfo()->ChannelNotifier.GetCtxDmaHandleGpu()));

    //TODO: Add support for multi-GPU
    CHECK_RC(pCh->Write(LWC37E_SET_NOTIFIER_CONTROL,
            DRF_DEF(C37E, _SET_NOTIFIER_CONTROL, _MODE, _WRITE) |
            DRF_NUM(C37E, _SET_NOTIFIER_CONTROL, _OFFSET, offset)));

    return rc;
}

bool LwDispBasicWindowSanity::PollNotifierComplete(void *Args)
{
    LwDispBasicWindowSanityNotifierSettings *pNotifierSettings = (LwDispBasicWindowSanityNotifierSettings*)Args;

    UINT32 lwrVal = MEM_RD32(pNotifierSettings->CompletionAddress);

    bool done = (DRF_VAL(_DISP_NOTIFIER, __0, _STATUS, lwrVal) ==
                 LW_DISP_NOTIFIER__0_STATUS_FINISHED);

    if (!done)
        return false;

    Printf(Tee::PriDebug, "NOTIFIER COMPLETE - value = 0x%08x \n", lwrVal);

    return true;
}

RC LwDispBasicWindowSanity::WindowSetOwner
(
    DisplayChannel *pCh,
    UINT32 winInstance,
    UINT32 head
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37D_WINDOW_SET_CONTROL(winInstance),
            DRF_NUM(C37D, _WINDOW_SET_CONTROL, _OWNER, head)));

    return rc;
}

RC LwDispBasicWindowSanity::SetupDesktopColor
(
    DisplayChannel *pCh,
    UINT32 head,
    LwDispColor desktopColor
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_DESKTOP_COLOR(head),
        DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_ALPHA, desktopColor.Alpha)  |
        DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_RED, desktopColor.Red)      |
        DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_GREEN, desktopColor.Green)  |
        DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_BLUE, desktopColor.Blue)));

    return rc;
}

RC LwDispBasicWindowSanity::SetWinSize
(
    DisplayChannel *pCh,
    LwDispWindowSettings *winSettings
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37E_SET_SIZE,
            DRF_NUM(C37E, _SET_SIZE, _WIDTH, winSettings->rectIn.width) |
            DRF_NUM(C37E, _SET_SIZE, _HEIGHT, winSettings->rectIn.height)));

    return rc;
}

RC LwDispBasicWindowSanity::SetWinSizeIn
(
    DisplayChannel *pCh,
    LwDispWindowSettings *winSettings
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37E_SET_POINT_IN(0),
            DRF_NUM(C37E, _SET_POINT_IN, _X, winSettings->rectIn.xPos) |
            DRF_NUM(C37E, _SET_POINT_IN, _Y, winSettings->rectIn.yPos)));

    CHECK_RC(pCh->Write(LWC37E_SET_SIZE_IN,
            DRF_NUM(C37E, _SET_SIZE_IN, _WIDTH, winSettings->rectIn.width) |
            DRF_NUM(C37E, _SET_SIZE_IN, _HEIGHT, winSettings->rectIn.height)));

    return rc;
}

RC LwDispBasicWindowSanity::SetWinSizeOut
(
    DisplayChannel *pCh,
    LwDispWindowSettings *winSettings
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37E_SET_SIZE_OUT,
            DRF_NUM(C37E, _SET_SIZE_OUT, _WIDTH, winSettings->sizeOut.width) |
            DRF_NUM(C37E, _SET_SIZE_OUT, _HEIGHT, winSettings->sizeOut.height)));

    return rc;
}

RC LwDispBasicWindowSanity::SetCompositionCtrl
(
    DisplayChannel *pCh,
    UINT32 depth
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37E_SET_COMPOSITION_CONTROL,
                        DRF_DEF(C37E, _SET_COMPOSITION_CONTROL, _COLOR_KEY_SELECT, _DISABLE) |
                        DRF_NUM(C37E, _SET_COMPOSITION_CONTROL, _DEPTH, depth)));

    CHECK_RC(pCh->Write(LWC37E_SET_COMPOSITION_FACTOR_SELECT,
                        DRF_DEF(C37E, _SET_COMPOSITION_FACTOR_SELECT, _SRC_COLOR_FACTOR_MATCH_SELECT, _K1)   |
                        DRF_DEF(C37E, _SET_COMPOSITION_FACTOR_SELECT, _DST_COLOR_FACTOR_MATCH_SELECT, _ZERO) |
                        DRF_DEF(C37E, _SET_COMPOSITION_FACTOR_SELECT, _SRC_ALPHA_FACTOR_MATCH_SELECT, _K1)   |
                        DRF_DEF(C37E, _SET_COMPOSITION_FACTOR_SELECT, _DST_ALPHA_FACTOR_MATCH_SELECT, _ZERO)
                 ));

    CHECK_RC(pCh->Write(LWC37E_SET_COMPOSITION_CONSTANT_ALPHA,
                        DRF_NUM(C37E, _SET_COMPOSITION_CONSTANT_ALPHA, _K1, 255) |
                        DRF_NUM(C37E, _SET_COMPOSITION_CONSTANT_ALPHA, _K2, 255)
                 ));

    return rc;
}

RC LwDispBasicWindowSanity::SetTimestampLo
(
    DisplayChannel *pCh,
    UINT32 value
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37E_SET_UPDATE_TIMESTAMP_LO, value));

    return rc;
}

RC LwDispBasicWindowSanity::SetupLwrsorSurface
(
    DisplayID display,
    LwDispLwrsorSettings *pLwrsorSettings,
    DisplayChannel *pCh,
    UINT32 head
)
{
    RC rc;
    LwDispSurfaceParams *surface = new LwDispSurfaceParams();

    surface->imageWidth = 32;
    surface->imageHeight = 32;
    surface->colorDepthBpp = 16;
    surface->layout = Surface2D::Pitch;
    DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(surface->imageWidth, surface->imageHeight);
    surface->imageFileName = imgArr.GetImageName();

    CHECK_RC(SurfaceUtils::SurfaceHelper::SetupSurface(
        m_pDisplay->GetOwningDisplaySubdevice(),
        surface, pLwrsorSettings->pLwrsorSurface,
        Display::LWRSOR_IMM_CHANNEL_ID));

    // Setup channelImage on cursor
    CHECK_RC(pLwDisplay->SetupLwrsorChannelImage(display, pLwrsorSettings, head));

    delete surface;

    return rc;
}

RC LwDispBasicWindowSanity::SetupSurface
(
    DisplayID display,
    LwDispWindowSettings *winSettings,
    DisplayChannel *pCh
)
{
    RC rc;
    LwDispSurfaceParams *surface = new LwDispSurfaceParams();

   // fill surface and window settings for SetupChannelImage
    surface->imageWidth = winSettings->sizeOut.width;
    surface->imageHeight = winSettings->sizeOut.height;
    surface->colorDepthBpp = 32;
    surface->layout = Surface2D::Pitch;
    DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(surface->imageWidth, surface->imageHeight);
    surface->imageFileName = imgArr.GetImageName();

    // Setup channelImage on window 4
    CHECK_RC(SurfaceUtils::SurfaceHelper::SetupSurface(
       m_pDisplay->GetOwningDisplaySubdevice(),
       surface,
       winSettings->image.pkd.pWindowImage,
       Display::WINDOW_CHANNEL_ID));

    CHECK_RC(m_pDisplay->SetupChannelImage(display, winSettings));

    CHECK_RC(pCh->Write(LWC37E_SET_PLANAR_STORAGE(0),
        DRF_NUM(C37E, _SET_PLANAR_STORAGE, _PITCH, winSettings->image.GetPitch() >> 6)));

    CHECK_RC(pCh->Write(LWC37E_SET_PARAMS,
        DRF_DEF(C37E, _SET_PARAMS, _FORMAT, _A8R8G8B8)    |
        DRF_DEF(C37E, _SET_PARAMS, _COLOR_SPACE, _RGB)    |
        DRF_DEF(C37E, _SET_PARAMS, _INPUT_RANGE, _BYPASS)));

    CHECK_RC(pCh->Write(LWC37E_SET_PRESENT_CONTROL,
           DRF_NUM(C37E, _SET_PRESENT_CONTROL, _MIN_PRESENT_INTERVAL, 1)    |
        DRF_DEF(C37E, _SET_PRESENT_CONTROL, _BEGIN_MODE, _NON_TEARING)    |
        DRF_DEF(C37E, _SET_PRESENT_CONTROL, _TIMESTAMP_MODE, _DISABLE)));

    CHECK_RC(pCh->Write(LWC37E_SET_STORAGE,
        DRF_DEF(C37E, _SET_STORAGE, _BLOCK_HEIGHT, _LWD_BLOCK_HEIGHT_ONE_GOB) |
        DRF_DEF(C37E, _SET_STORAGE, _MEMORY_LAYOUT, _PITCH)));

    delete surface;

    return rc;
}

RC LwDispBasicWindowSanity::SetupViewportIn
(
    DisplayChannel *pCh,
    UINT32 head,
    LwDispViewPortSettings *pViewPortSettings
)
{
    MASSERT(pViewPortSettings);
    RC rc;

    Printf(Tee::PriLow, "SetupViewportIn:Head %d\n", head);
    Printf(Tee::PriLow, "SetupViewportIn:PointInX %d PointInY %d\n",
           pViewPortSettings->ViewportIn.xPos,
           pViewPortSettings->ViewportIn.yPos);

    Printf(Tee::PriLow, "SetupViewportIn:WidthInPixels %d SizeInLines %d\n",
           pViewPortSettings->ViewportIn.width,
           pViewPortSettings->ViewportIn.height);

    // viewport point in primary
    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_VIEWPORT_POINT_IN(head),
                        DRF_NUM(C37D, _HEAD_SET_VIEWPORT_POINT_IN, _X,
                                pViewPortSettings->ViewportIn.xPos) |
                        DRF_NUM(C37D, _HEAD_SET_VIEWPORT_POINT_IN, _Y,
                                pViewPortSettings->ViewportIn.yPos)));

    // viewport in size
    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_VIEWPORT_SIZE_IN(head),
                        DRF_NUM(C37D, _HEAD_SET_VIEWPORT_SIZE_IN, _WIDTH,
                                pViewPortSettings->ViewportIn.width) |
                        DRF_NUM(C37D, _HEAD_SET_VIEWPORT_SIZE_IN, _HEIGHT,
                                pViewPortSettings->ViewportIn.height)));

    return rc;
}

RC LwDispBasicWindowSanity::SetupViewportOut
(
    DisplayChannel *pCh,
    UINT32 head,
    LwDispViewPortSettings *pViewPortSettings
)
{
    MASSERT(pViewPortSettings);

    RC rc;

    Printf(Tee::PriLow, "SetupViewportOut:Head %d\n", head);

        Printf(Tee::PriLow, "SetupViewportOut:Width Pixels %d SizeOut Height in Lines %d\n",
            pViewPortSettings->ViewportOut.width,
            pViewPortSettings->ViewportOut.height);

    // viewport size out
    CHECK_RC(pCh->Write(LWC37D_HEAD_SET_VIEWPORT_SIZE_OUT(head),
                        DRF_NUM(C37D, _HEAD_SET_VIEWPORT_SIZE_OUT, _WIDTH,
                                pViewPortSettings->ViewportOut.width) |
                        DRF_NUM(C37D, _HEAD_SET_VIEWPORT_SIZE_OUT, _HEIGHT,
                                pViewPortSettings->ViewportOut.height)));

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDispBasicWindowSanity::Cleanup()
{
    return OK;
}

RC LwDispBasicWindowSanity::GetProtocol
(
    UINT32 orProtocol,
    UINT32 *pProtocol
)
{
    RC rc;

    //TOOD: Add PIOR protocols as well.
    switch (orProtocol)
    {
        case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A:
            *pProtocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A;
            break;
        case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_B:
            *pProtocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_B ;
            break;
        case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DUAL_TMDS:
            *pProtocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_DUAL_TMDS;
            break;
        case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_A:
            *pProtocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_DP_A;
            break;
        case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_DP_B:
            *pProtocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_DP_B;
            break;
        case LW0073_CTRL_SPECIFIC_OR_PROTOCOL_DSI:
            *pProtocol =  LW0073_CTRL_SPECIFIC_OR_PROTOCOL_DSI;
            break;
        default:
            *pProtocol = LW0073_CTRL_SPECIFIC_OR_PROTOCOL_SOR_SINGLE_TMDS_A;
            Printf(Tee::PriHigh, "Unrecognized SOR protocol %d, using TMDS_A\n",
                    orProtocol);
            return RC::BAD_PARAMETER;
    }

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwDispBasicWindowSanity, RmTest,
    "Simple test to demonstrate usage of SetMode() API");

CLASS_PROP_READWRITE(LwDispBasicWindowSanity,UseDebugPortChannel,bool,
                     "Use DebugPort channel to inject methods and data");
CLASS_PROP_READWRITE(LwDispBasicWindowSanity,UseWindowNotifier,bool,
                     "Use Windows notifier in the test");
CLASS_PROP_READWRITE(LwDispBasicWindowSanity,TestLwrsor,bool,
                     "Test Cursor support");
CLASS_PROP_READWRITE(LwDispBasicWindowSanity,TestOutputLut,bool,
                     "Test Output (Core) LUT settings");
CLASS_PROP_READWRITE(LwDispBasicWindowSanity,TestInputLut,bool,
                     "Test Input (Window) LUT settings");
CLASS_PROP_READWRITE(LwDispBasicWindowSanity,IncludeCrc,bool,
                     "Include Crc in the test");
CLASS_PROP_READWRITE(LwDispBasicWindowSanity, rastersize, string,
                     "Desired raster size (small/large)");
