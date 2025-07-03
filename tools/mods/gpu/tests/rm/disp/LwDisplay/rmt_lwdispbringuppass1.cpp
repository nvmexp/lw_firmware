/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_lwdispbringuppass1
//!       This test does the functional verification of snooze fmodel release, desktop color fmodel release.

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/statecache_C3.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "class/clc370.h"
#include "class/clc570.h"
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "class/clc57d.h" // LWC57D_CORE_CHANNEL_DMA
#include "hwref/disp/v03_00/dev_disp.h" // LW_PDISP_FE_DEBUG_CTL
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/dispchan.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/utility/zbccolwertutil.h"
using namespace std;

#define OBJ_MAX_SORS    4

class IDisplayChannel
{
public:
    virtual RC Write(LwU32 method_offset, LwU32 method_data) = 0;
    virtual RC Flush() {return OK;}

};

class PushBufferChannel : public IDisplayChannel
{
public:

    DisplayChannel *m_pCh;

    PushBufferChannel(DisplayChannel *pCh);

    virtual RC Write(LwU32 method_offset, LwU32 method_data);
    virtual RC Flush();
};

class DebugPortChannel : public IDisplayChannel
{
private:
        Display *m_pDisplay;
        LwU32    m_channelNum;
public:

    DebugPortChannel(Display *pDisplay, LwU32 channelNum);
    virtual RC Write(LwU32 method_offset, LwU32 method_data);
};

struct LwDispBringupPass1NotifierSettings
{
    bool Dirty;
    NotifierMode mode;
    void* CompletionAddress;
    UINT32 NumNotifiers;
    UINT32 NotifierSize;
    bool (*PollNotifierDone)(void *);
    LwDispBringupPass1NotifierSettings();
};

LwDispBringupPass1NotifierSettings::LwDispBringupPass1NotifierSettings()
{
    Dirty = false;
    CompletionAddress = NULL;
    NumNotifiers = 0;
    NotifierSize = 0;
}

PushBufferChannel::PushBufferChannel(DisplayChannel *pCh)
{
    m_pCh = pCh;
}

RC PushBufferChannel::Write(LwU32 method_offset, LwU32 method_data)
{
    return m_pCh->Write(method_offset, method_data);
}

RC PushBufferChannel::Flush()
{
    return m_pCh->Flush();
}

class LwDispBringupPass1 : public RmTest
{
public:
    LwDispBringupPass1();
    virtual ~LwDispBringupPass1();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC SetupOR
    (
        IDisplayChannel *pCh,
        DisplayID displayID,
        UINT32 head,
        bool   bDetach,
        LwDispORSettings *pORSettings
    );

    RC SetupRaster
    (
        IDisplayChannel *pCh,
        UINT32 head,
        LwDispRasterSettings *pRasterSettings
    );

    RC SetupPixelClock
    (
        IDisplayChannel *pCh,
        UINT32 head,
        LwDispPixelClockSettings *pPixelClkSettings
    );

    RC SetupViewportOut
    (
        IDisplayChannel *pCh,
        UINT32 head,
        LwDispViewPortSettings *pViewPortSettings
    );

    RC SetupViewportIn
    (
        IDisplayChannel *pCh,
        UINT32 head,
        LwDispViewPortSettings *pViewPortSettings
    );

    RC SetupDesktopColor
    (
        IDisplayChannel *pCh,
        UINT32 head,
        LwDispColor desktopColor
    );

    RC SetupWindowOwner
    (
        IDisplayChannel *pCh,
        UINT32 winInstance,
        UINT32 head
     );

    RC InitNotifierSettings
    (
        LwDispCoreChnContext* pChnContext,
        LwU32                 offset = 0
    );

    RC SetupCompletionNotifier
    (
        IDisplayChannel      *pCh,
        LwDispCoreChnContext* pChnContext,
        LwU32                 offset = 0
     );

    static bool PollNotifierComplete
    (
        void *Args
    );

    RC TriggerUpdate(IDisplayChannel *pCh, UINT32 head);

    SETGET_PROP(UseDebugPortChannel, bool);
    SETGET_PROP(TestDesktopColor, bool);
    SETGET_PROP(rastersize, string); //Config raster size through cmdline

private:
    LwDispBringupPass1NotifierSettings m_NotifierSettings;
    Display *m_pDisplay;
    bool m_UseDebugPortChannel;
    bool m_TestDesktopColor;
    string m_rastersize;
};

DebugPortChannel::DebugPortChannel(Display *pDisplay, LwU32 channelNum)
{
    m_pDisplay = pDisplay;
    m_channelNum = channelNum;
}

LwDispBringupPass1::LwDispBringupPass1()
{
    m_pDisplay = GetDisplay();
    m_UseDebugPortChannel = false;
    m_TestDesktopColor = false;
    m_rastersize = "";
    SetName("LwDispBringupPass1");
}

//! \brief LwDispBringupPass1 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDispBringupPass1::~LwDispBringupPass1()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDispBringupPass1::IsTestSupported()
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
RC LwDispBringupPass1::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//!
//! It will Init LwDisplay HW and allocate a core and window channel.
//------------------------------------------------------------------------------
RC LwDispBringupPass1::Run()
{
    RC rc = OK;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    WindowIndex hWin;
    GpuSubdevice *pSubdev = m_pDisplay->GetOwningDisplaySubdevice();

    CHECK_RC(pLwDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    DisplayIDs DisplaysToUse;
    DisplayIDs workingSet;

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
    UINT32 windowInstance = 0;
    UINT32 head = 0;
    UINT32 sorNum = 0;
    bool   useSmallRaster = false;

    if (GetBoundGpuSubdevice()->IsDFPGA())
    {
        // Sepcial Configuration for TMDS_A
        windowInstance = 4;
        head = 2;
        sorNum = 2;
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
                Printf(Tee::PriHigh, "Using SOR = %d\n", sorNum);
                break;
            }
        }
    }

    CHECK_RC(pLwDisplay->AllocateWinAndWinImm(&hWin, head, windowInstance));

    IDisplayChannel *pCh = NULL;
    LwDispChnContext* pChnContext = NULL;

    // Use Core Display Channel
    if (!m_UseDebugPortChannel)
    {
        CHECK_RC(pLwDisplay->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
                                                    &pChnContext,
                                                    LwDisplay::ILWALID_WINDOW_INSTANCE,
                                                    head));
        pCh = new PushBufferChannel(pChnContext->GetChannel());
    }
    else
    {
        pCh = new DebugPortChannel(m_pDisplay, LW_PDISP_CHN_NUM_CORE);
    }

    LwDispSORSettings sorSettings;
    sorSettings.ORNumber  = sorNum;
    sorSettings.OwnerMask = 0;
    sorSettings.protocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A;
    sorSettings.CrcActiveRasterOnly = false;
    sorSettings.HSyncPolarityNegative = false;
    sorSettings.VSyncPolarityNegative = false;

    CHECK_RC(SetupOR(pCh, workingSet[0], head, false, &sorSettings));

    if (m_TestDesktopColor)
    {
      //
      // This flag is needed to pass through as2 snooze frame test protection
      // as with this method as2 is failing becasue of window method integration issues.
      CHECK_RC(SetupWindowOwner(pCh, hWin, head));
    }

    CHECK_RC(SetupRaster(pCh, head, &rasterSettings));

    //Set Pixel Clock value
    LwDispPixelClockSettings pixelClockSettings;
    pixelClockSettings.pixelClkFreqHz = rasterSettings.PixelClockHz;
    pixelClockSettings.bAdj1000Div1001 = false;
    CHECK_RC(SetupPixelClock(pCh, head, &pixelClockSettings));

    LwDispViewPortSettings viewPortSettings;

    viewPortSettings.SetViewportIn(0, 0, width, height);
    viewPortSettings.SetViewportOut(0, 0, width, height);

    CHECK_RC(SetupViewportIn(pCh, head, &viewPortSettings));

    CHECK_RC(SetupViewportOut(pCh, head, &viewPortSettings));

    LwDispColor desktopColor;

    desktopColor.Alpha = 0xf;
    desktopColor.Red = 0;
    desktopColor.Green = 255;
    desktopColor.Blue = 0;

    // Set desktop color method with above values.
    CHECK_RC(SetupDesktopColor(pCh, head, desktopColor));

    // dump state cache
    StateCache_C3 stateCache;
    stateCache.ReadStateCache(pSubdev,
                              Display::CORE_CHANNEL_ID,
                              0,
                              StateCache::BOTH);

    LwDispCoreChnContext *pCoreChnCtx = (LwDispCoreChnContext *)pChnContext;

    if (!m_UseDebugPortChannel)
    {
        CHECK_RC(InitNotifierSettings(pCoreChnCtx));

        CHECK_RC(SetupCompletionNotifier(pCh, pCoreChnCtx));
    }

    CHECK_RC(TriggerUpdate(pCh, head));

    pCh->Flush();

    if (!m_UseDebugPortChannel)
    {
        Printf(Tee::PriHigh, "Polling notifier\n");

        CHECK_RC(POLLWRAP(
            m_NotifierSettings.PollNotifierDone,
            &m_NotifierSettings,
            3 * pCoreChnCtx->GetChannel()->GetDefaultTimeoutMs()));
    }
    else
    {
        Tasker::Sleep(100 * 1000);
    }

    Printf(Tee::PriHigh, "Disconnecting OR\n");

    CHECK_RC(SetupOR(pCh, workingSet[0], head, true, &sorSettings));

    if (!m_UseDebugPortChannel)
    {
        /*Setup notifier with different offset */
        CHECK_RC(InitNotifierSettings(pCoreChnCtx, 1/*new_offset*/));
        CHECK_RC(SetupCompletionNotifier(pCh, pCoreChnCtx, 1/*new_offset*/));

        /* Clear the current state*/
        MEM_WR32(m_NotifierSettings.CompletionAddress, 0);
    }

    CHECK_RC(TriggerUpdate(pCh, head));
    pCh->Flush();

    if (!m_UseDebugPortChannel)
    {
        /*Poll for completion*/
        CHECK_RC(POLLWRAP(
            m_NotifierSettings.PollNotifierDone,
            &m_NotifierSettings,
            3 * pCoreChnCtx->GetChannel()->GetDefaultTimeoutMs()));
    }
    else
    {
       Tasker::Sleep(100 * 1000);
    }

    stateCache.ReadStateCache(pSubdev,
        Display::CORE_CHANNEL_ID,
        0,
        StateCache::BOTH);

    Printf(Tee::PriHigh, "Destroy Win and WinImm\n");

    CHECK_RC(pLwDisplay->DestroyWinAndWinImm(hWin));

    return rc;
}

RC LwDispBringupPass1::InitNotifierSettings
(
    LwDispCoreChnContext* pChnContext,
    LwU32                 offset
)
{
    RC rc;

    MASSERT((offset>=0) && (((offset+1)*LW_DISP_NOTIFIER_SIZEOF) < 0x1000/*notifier_size*/));

    m_NotifierSettings.NotifierSize = LW_DISP_NOTIFIER_SIZEOF;
    m_NotifierSettings.CompletionAddress =
        (UINT08*)pChnContext->GetDMAChanInfo()->ChannelNotifier.GetAddress() +
        LW_DISP_NOTIFIER__0 +
        (offset * LW_DISP_NOTIFIER_SIZEOF);

    m_NotifierSettings.PollNotifierDone = LwDispBringupPass1::PollNotifierComplete;
    m_NotifierSettings.Dirty = true;

    return rc;
}

RC LwDispBringupPass1 ::SetupCompletionNotifier
(
    IDisplayChannel      *pCh,
    LwDispCoreChnContext* pChnContext,
    LwU32                 offset
)
{
    RC     rc;

    MASSERT((offset>=0) && (((offset+1)*LW_DISP_NOTIFIER_SIZEOF) < 0x1000/*notifier_size*/));

    CHECK_RC(pCh->Write(LWC37D_SET_CONTEXT_DMA_NOTIFIER, pChnContext->GetDMAChanInfo()->ChannelNotifier.GetCtxDmaHandleGpu()));

    //TODO: Add support for multi-GPU
    CHECK_RC(pCh->Write(LWC37D_SET_NOTIFIER_CONTROL,
            DRF_DEF(C37D, _SET_NOTIFIER_CONTROL, _MODE, _WRITE) |
            DRF_NUM(C37D, _SET_NOTIFIER_CONTROL, _OFFSET, offset) |
            DRF_DEF(C37D, _SET_NOTIFIER_CONTROL, _NOTIFY, _ENABLE)));

    return rc;
}

bool LwDispBringupPass1::PollNotifierComplete(void *Args)
{
    LwDispBringupPass1NotifierSettings *pNotifierSettings = (LwDispBringupPass1NotifierSettings*)Args;

    UINT32 lwrVal = MEM_RD32(pNotifierSettings->CompletionAddress);

    bool done = (DRF_VAL(_DISP_NOTIFIER, __0, _STATUS, lwrVal) ==
                 LW_DISP_NOTIFIER__0_STATUS_FINISHED);

    if (!done)
        return false;

    Printf(Tee::PriDebug, " 0x%08x \n", lwrVal);

    return true;
}

RC LwDispBringupPass1::SetupDesktopColor
(
    IDisplayChannel *pCh,
    UINT32 head,
    LwDispColor desktopColor
)
{
    RC rc;

    if (LWC370_DISPLAY == m_pDisplay->GetClass())
    {
        CHECK_RC(pCh->Write(LWC37D_HEAD_SET_DESKTOP_COLOR(head),
            DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_ALPHA, desktopColor.Alpha)  |
            DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_RED, desktopColor.Red)      |
            DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_GREEN, desktopColor.Green)  |
            DRF_NUM(C37D, _HEAD, _SET_DESKTOP_COLOR_BLUE, desktopColor.Blue)));
    }
    else if (LWC570_DISPLAY == m_pDisplay->GetClass())
    {
        CHECK_RC(pCh->Write(LWC57D_HEAD_SET_DESKTOP_COLOR_ALPHA_RED(head),
                DRF_NUM(C57D, _HEAD, _SET_DESKTOP_COLOR_ALPHA_RED_ALPHA,
                    desktopColor.Alpha) |
                DRF_NUM(C57D, _HEAD, _SET_DESKTOP_COLOR_ALPHA_RED_RED,
                    zbcColwert::uint32ToFP16(desktopColor.Red))));

        CHECK_RC(pCh->Write(LWC57D_HEAD_SET_DESKTOP_COLOR_GREEN_BLUE(head),
                DRF_NUM(C57D, _HEAD, _SET_DESKTOP_COLOR_GREEN_BLUE_GREEN,
                    zbcColwert::uint32ToFP16(desktopColor.Green)) |
                DRF_NUM(C57D, _HEAD, _SET_DESKTOP_COLOR_GREEN_BLUE_BLUE,
                    zbcColwert::uint32ToFP16(desktopColor.Blue))));
    }
    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDispBringupPass1::Cleanup()
{
    return OK;
}

RC LwDispBringupPass1::TriggerUpdate(IDisplayChannel *pCh, UINT32 head)
{
    pCh->Write(LWC37D_UPDATE, DRF_DEF(C37D, _UPDATE, _FLIP_LOCK_PIN, _LOCK_PIN_NONE));
    return OK;
}

RC LwDispBringupPass1::SetupOR
(
    IDisplayChannel *pCh,
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
    UINT32 PixelDepth = LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_PIXEL_DEPTH_BPP_16_422;

    UINT32 HSyncPolNeg =
         pORSettings->HSyncPolarityNegative ?
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_HSYNC_POLARITY_NEGATIVE_TRUE :
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_HSYNC_POLARITY_POSITIVE_TRUE;

    UINT32 VSyncPolNeg =
         pORSettings->VSyncPolarityNegative ?
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_VSYNC_POLARITY_NEGATIVE_TRUE :
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE_VSYNC_POLARITY_POSITIVE_TRUE;

    // For now test with SINGLE_TMDS_A
    pORSettings->protocol = LWC37D_SOR_SET_CONTROL_PROTOCOL_SINGLE_TMDS_A;

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

    CHECK_RC(pCh->Write(
        LWC37D_SOR_SET_CONTROL(pORSettings->ORNumber),
        DRF_NUM(C37D, _SOR_SET_CONTROL, _OWNER_MASK, pSORSettings->OwnerMask ) |
        DRF_NUM(C37D, _SOR_SET_CONTROL, _PROTOCOL, pORSettings->protocol) |
        DRF_DEF(C37D, _SOR_SET_CONTROL, _DE_SYNC_POLARITY, _POSITIVE_TRUE) |
        DRF_DEF(C37D, _SOR_SET_CONTROL, _PIXEL_REPLICATE_MODE , _OFF) ));

    if (LWC370_DISPLAY == m_pDisplay->GetClass())
    {
        CHECK_RC(pCh->Write(
            LWC37D_HEAD_SET_CONTROL_OUTPUT_RESOURCE(head),
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _CRC_MODE, CrcMode) |
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _HSYNC_POLARITY, HSyncPolNeg) |
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _VSYNC_POLARITY, VSyncPolNeg) |
            DRF_NUM(C37D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _PIXEL_DEPTH, PixelDepth) ));
    }
    else if (LWC570_DISPLAY == m_pDisplay->GetClass())
    {
        CHECK_RC(pCh->Write(
            LWC57D_HEAD_SET_CONTROL_OUTPUT_RESOURCE(head),
            DRF_NUM(C57D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _CRC_MODE, CrcMode) |
            DRF_NUM(C57D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _HSYNC_POLARITY, HSyncPolNeg) |
            DRF_NUM(C57D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _VSYNC_POLARITY, VSyncPolNeg) |
            DRF_DEF(C57D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _EXT_PACKET_WIN, _NONE) |
            DRF_NUM(C57D, _HEAD_SET_CONTROL_OUTPUT_RESOURCE, _PIXEL_DEPTH, PixelDepth) ));
    }

    if (!bDetach)
    {
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

RC LwDispBringupPass1::SetupRaster
(
    IDisplayChannel *pCh,
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

RC LwDispBringupPass1::SetupWindowOwner
(
    IDisplayChannel *pCh,
    UINT32 winInstance,
    UINT32 head
)
{
    RC rc;

    CHECK_RC(pCh->Write(LWC37D_WINDOW_SET_CONTROL(winInstance),
            DRF_NUM(C37D, _WINDOW_SET_CONTROL, _OWNER, head)));

    return rc;
}

RC LwDispBringupPass1::SetupPixelClock
(
    IDisplayChannel *pCh,
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

//------------------------------------------------------------------------------
RC LwDispBringupPass1::SetupViewportIn
(
    IDisplayChannel *pCh,
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

RC LwDispBringupPass1::SetupViewportOut
(
    IDisplayChannel *pCh,
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

RC DebugPortChannel::Write(LwU32 method_offset, LwU32 method_data)
{

    //enqueue(method_offset,method_data){
    //write LW_PDISP_FE_DEBUG_DAT(method_data)
    //write LW_PDISP_FE_DEBUG_CTL(ENABLE, method_offset, NORMAL, TRIGGER)
    //poll LW_PDISP_FE_DEBUG_CTL_NEW_METHOD field until it goes from TRIGGER to DONE

    RC rc;
    UINT32 dbgCtrl = 0;
    GpuSubdevice *pSubdev;

    pSubdev = m_pDisplay->GetOwningDisplaySubdevice();
    pSubdev->RegWr32(LW_PDISP_FE_DEBUG_DAT(m_channelNum), method_data);

    // Shift the method offset by 2
    method_offset = method_offset >> 2;

    dbgCtrl = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _MODE, _ENABLE, dbgCtrl);
    dbgCtrl = FLD_SET_DRF_NUM(_PDISP, _FE_DEBUG_CTL, _METHOD_OFS, method_offset, dbgCtrl);
    dbgCtrl = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL,  _CTXDMA, _NORMAL,  dbgCtrl);
    dbgCtrl = FLD_SET_DRF(_PDISP, _FE_DEBUG_CTL, _NEW_METHOD, _TRIGGER,  dbgCtrl);
    pSubdev->RegWr32(LW_PDISP_FE_DEBUG_CTL(m_channelNum), dbgCtrl);

    while (TRUE)
    {
        if (FLD_TEST_DRF(_PDISP, _FE_DEBUG_CTL, _NEW_METHOD, _DONE,
            pSubdev->RegRd32(LW_PDISP_FE_DEBUG_CTL(m_channelNum))))
        {
            break;
        }
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
JS_CLASS_INHERIT(LwDispBringupPass1, RmTest,
    "Simple test to check verify Init of Display HW and channel allocation.");

CLASS_PROP_READWRITE(LwDispBringupPass1,UseDebugPortChannel,bool,
                     "Use DebugPort channel to inject methods and data");
CLASS_PROP_READWRITE(LwDispBringupPass1,TestDesktopColor,bool,
                     "Specify this flag to test desktop color milestone release");
CLASS_PROP_READWRITE(LwDispBringupPass1, rastersize, string,
                     "Desired raster size (small/large)");
