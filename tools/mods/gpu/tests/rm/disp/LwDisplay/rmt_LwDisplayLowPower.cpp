/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_LwDisplayLowPower
//!       This test calls lwdisplay library APIs to verify window(s) on head(s)

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwmisc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/display/evo_dp.h"
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "gpu/tests/rm/utility/dtiutils.h"
#include "modesetlib.h"
#include "LwDpUtils.h"
#include "core/include/simpleui.h"
#include "random.h"
#include "gpu/tests/rm/disp/pgstats.h"

using namespace std;
using namespace Modesetlib;

// Number of loops to probe the ASR/MSCG efficiency.
#define MSCG_EFFICIENCY_CHECK_LOOPS 10

class LwDisplayLowPower : public RmTest
{
public:
    LwDisplayLowPower();
    virtual ~LwDisplayLowPower();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC TestDisplayPanel(DisplayPanel *pDisplayPanel, UINT32 configNumber);

    RC GetCrcFileName(DisplayPanel *pDisplayPanel,
                       string& crcFileName,
                       string& fileDirPath,
                       UINT32 configNumber);

    vector<string> Tokenize(const string &str,
        const string &delimiters);

    RC ParseCmdLineArgs();
    RC InitializeTestDispPanel(DisplayPanel *pTestDispPanel);
    RC checkDispUnderflow(UINT32 head);
    UINT32 getLoadVCounter(UINT32 head);
    UINT32 getMscgEfficiency();
    void Sleep(LwU32 miliseconds, LwBool runOnEmu, GpuSubdevice *gpuSubdev);

    SETGET_PROP(heads, string);
    SETGET_PROP(sors, string);
    SETGET_PROP(windows, string);
    SETGET_PROP(headwin, string);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(resolution, string); //Config raster size through cmdline
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(force_DSC_BitsPerPixelX16, UINT32);
    SETGET_PROP(enable_mode, string);
    SETGET_PROP(inputLutLwrve, UINT32);
    SETGET_PROP(outputLutLwrve, UINT32);
    SETGET_PROP(tmoLutLwrve, UINT32);
    SETGET_PROP(colorgamut, UINT32);
    SETGET_PROP(semiplanar, bool);
    SETGET_PROP(enableVRR, bool);
    SETGET_PROP(numFrames, UINT32);
    SETGET_PROP(immFlip, bool);
    SETGET_PROP(osmMclkSwitch, bool);

private:
    Display *m_pDisplay;
    string m_config;
    string m_heads;
    string m_windows;
    string m_headwin;
    LwDisplay *m_pLwDisplay;
    string m_sors;
    string m_protocol;
    string m_resolution;
    UINT32 m_pixelclock;
    UINT32 m_linkRate;
    UINT32 m_laneCount;
    bool m_manualVerif;
    bool m_onlyConnectedDisplays;
    bool m_generate_gold;
    DisplayResolution m_dispResolution;
    UINT32 m_force_DSC_BitsPerPixelX16;
    vector<string>m_szConfig;
    vector<string>m_szSors;
    vector<string>m_szHeads;
    map<UINT32, WindowParams>m_headWindowMap;
    UINT32 m_inputLutLwrve;
    UINT32 m_outputLutLwrve;
    UINT32 m_tmoLutLwrve;
    UINT32 m_colorgamut;
    string m_enable_mode;
    DisplayMode displayMode;
    UINT32 maxHeads;
    UINT32 maxWindows;
    UINT32 m_dpTestMode;
    bool m_semiplanar;
    bool m_enableVRR;
    bool m_immFlip;
    bool m_osmMclkSwitch;
    UINT32 m_numFrames;
    bool m_runOnEmu;
};

LwDisplayLowPower::LwDisplayLowPower()
{
    m_pDisplay = nullptr;
    SetName("LwDisplayLowPower");
    m_windows = "0-800-600";
    m_headwin = "0-0-800-600";
    m_protocol = "SINGLE_TMDS_A";
    m_heads = "0";
    m_sors = "0";
    m_config = "";
    m_generate_gold = false;
    m_onlyConnectedDisplays = false;
    m_manualVerif = false;
    m_pixelclock = 0;
    m_force_DSC_BitsPerPixelX16 = 0;
    m_linkRate = 0;
    m_laneCount = laneCount_0;
    m_enable_mode = "";
    displayMode = ENABLE_BYPASS_SETTINGS;
    maxHeads = 0;
    maxWindows = 0;
    m_dpTestMode = LwDispPanelMgr::LWD_PANEL_TYPE_NONE;
    m_outputLutLwrve = 0; 
    m_inputLutLwrve = 0;
    m_tmoLutLwrve = 0;
    m_colorgamut = 0;
    m_semiplanar = false;
    m_enableVRR = false;
    m_numFrames = 3;
    m_immFlip = false;
    m_osmMclkSwitch = false;
    m_runOnEmu = false;
}

//! \brief LwDispBringupPass1 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDisplayLowPower::~LwDisplayLowPower()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDisplayLowPower::IsTestSupported()
{
    m_pDisplay = GetDisplay();

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
RC LwDisplayLowPower::Setup()
{
    RC rc = OK;

    m_pDisplay = GetDisplay();
    m_pLwDisplay = static_cast<LwDisplay *>(GetDisplay());

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    if (GetBoundGpuSubdevice()->IsEmulation())
    {
        m_runOnEmu = true;
    }

    return rc;
}

RC LwDisplayLowPower::Run()
{
    RC rc;

    // allocate core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // Enumerate Display Panels first
    vector <DisplayPanel *>pDisplayPanels;
    vector<pair<DisplayPanel *, DisplayPanel *>> dualSSTPairs;
    DisplayPanelHelper *pDispPanelHelper = nullptr;

    pDispPanelHelper = new DisplayPanelHelper(m_pDisplay);
    CHECK_RC(pDispPanelHelper->EnumerateDisplayPanels(m_protocol, pDisplayPanels, dualSSTPairs, !m_onlyConnectedDisplays));

    // index 0 - DisplayPanel
    // index 1 - DisplayPanel
    // index 2 - DualSSTPair
    // so on ..
    UINT32 numPanels = 1;

    if (m_manualVerif)
    {
        UINT32 panelIndex = 0;
        do
        {
            bool bExit = false;
            CHECK_RC_CLEANUP(pDispPanelHelper->ListAndSelectDisplayPanel(panelIndex, bExit));
            if (bExit)
            {
                Printf(Tee::PriHigh,
                    " EXIT \n");
                break;
            }
            if (panelIndex > numPanels)
            {
                Printf(Tee::PriError,
                    " %s Wrong index, EXIT \n", __FUNCTION__);
                break;
            }
            DisplayPanel *pTestDispPanel;

            pTestDispPanel = pDisplayPanels[panelIndex];

            if (m_outputLutLwrve != 0)
            {
                pTestDispPanel->outputLutLwrve = 
                    (OetfLutGenerator::OetfLwrves)m_outputLutLwrve;
            }

            if(pTestDispPanel->isFakeDisplay)
            {
                pTestDispPanel->fakeDpProperties.pEdid = (UINT08 *)malloc(FAKE_EDID_SIZE * sizeof(UINT08));
                memcpy(pTestDispPanel->fakeDpProperties.pEdid, EdidForDisplayLowPower, FAKE_EDID_SIZE);
                pTestDispPanel->fakeDpProperties.edidSize = FAKE_EDID_SIZE;
            }

            CHECK_RC_CLEANUP(pTestDispPanel->pModeset->Initialize());

            rc = pTestDispPanel->pModeset->SelectUserInputs(pTestDispPanel, bExit);
            if ((rc != OK) || bExit)
            {
                Printf(Tee::PriHigh,
                    " EXIT \n");
                break;
            }
            // Test Display panel
            CHECK_RC_CLEANUP(TestDisplayPanel(pTestDispPanel, 0));
        }while (true);
    }
    else // Auto verification
    {
        CHECK_RC_CLEANUP(ParseCmdLineArgs());
        for (UINT32 iPanel = 0; iPanel< numPanels; iPanel++)
        {
            for (UINT32 iHead=0; iHead < m_szHeads.size(); iHead++)
            {
                DisplayPanel *pTestDispPanel = NULL;

                pTestDispPanel =  pDisplayPanels[iPanel];

                if (pTestDispPanel->pModeset->IsPanelTypeDP())
                {
                    Modeset_DP* pDpModeset = (Modeset_DP*)pTestDispPanel->pModeset;
                    if(m_dpTestMode && m_dpTestMode != pDpModeset->GetPanelType())
                    {
                        continue;
                    }
                }

                pTestDispPanel->head = atoi(m_szHeads[iHead].c_str());
                pTestDispPanel->resolution = m_dispResolution;

                if (m_outputLutLwrve != 0)
                {
                    pTestDispPanel->outputLutLwrve = 
                        (OetfLutGenerator::OetfLwrves)m_outputLutLwrve;
                }

                if (m_headWindowMap.find(pTestDispPanel->head) != m_headWindowMap.end())
                {
                    pTestDispPanel->winParams = m_headWindowMap[pTestDispPanel->head];
                }
                if (!pTestDispPanel->winParams.size())
                {
                    Printf(Tee::PriError,
                        " Invalid window-head combination Head - %d\n", pTestDispPanel->head);
                    return RC::SOFTWARE_ERROR;
                }

                if(pTestDispPanel->isFakeDisplay)
                {
                    pTestDispPanel->fakeDpProperties.pEdid = (UINT08 *)malloc(FAKE_EDID_SIZE * sizeof(UINT08));
                    memcpy(pTestDispPanel->fakeDpProperties.pEdid, EdidForDisplayLowPower, FAKE_EDID_SIZE);
                    pTestDispPanel->fakeDpProperties.edidSize = FAKE_EDID_SIZE;
                }

                CHECK_RC_CLEANUP(pTestDispPanel->pModeset->Initialize());
                for( UINT32 iSor = 0; iSor < m_szSors.size(); iSor++)
                {
                    pTestDispPanel->sor = atoi(m_szSors[iSor].c_str());
                    CHECK_RC_CLEANUP(TestDisplayPanel(pTestDispPanel, 0));
                }
            }
        }
    }

Cleanup:
    if (pDispPanelHelper)
    {
        delete pDispPanelHelper;
    }
    return rc;
}

RC LwDisplayLowPower::ParseCmdLineArgs()
{
    RC rc;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();

    if (m_pixelclock != 0)
    {
        m_dispResolution.pixelClockHz = m_pixelclock;
    }

    if (m_enable_mode == "SDR")
    {
        displayMode = ENABLE_SDR_SETTINGS;
    }
    else if (m_enable_mode == "HDR")
    {
        displayMode = ENABLE_HDR_SETTINGS;
    }

    vector<string>windowStrs;
    vector<string>windowTokens;

    if (!pLwDisplay->m_DispCap.flexibleWinMapSupport)
    {
        // "windowNum0-width-height, windowNum1-width-height"
        if (m_windows != "")
        {
            windowStrs = Tokenize(m_windows, ",");
        }
    }
    else
    {
        if (m_headwin != "")
        {
            windowStrs = Tokenize(m_headwin, ",");
        }
    }

    for (UINT32 i=0; i < windowStrs.size(); i++)
    {
        windowTokens = Tokenize(windowStrs[i], "-");
        WindowParam windowParam;
        if (windowTokens.size() == 3)
        {
            windowParam.windowNum = atoi(windowTokens[0].c_str());
            windowParam.width     = atoi(windowTokens[1].c_str());
            windowParam.height    = atoi(windowTokens[2].c_str());
        }
        if (windowTokens.size() == 4)
        {
            windowParam.headNum   = atoi(windowTokens[0].c_str());
            windowParam.windowNum = atoi(windowTokens[1].c_str());
            windowParam.width     = atoi(windowTokens[2].c_str());
            windowParam.height    = atoi(windowTokens[3].c_str());
        }
        windowParam.mode      = displayMode;
        if (m_inputLutLwrve != 0)
        {
            windowParam.inputLutLwrve =
                (EotfLutGenerator::EotfLwrves)m_inputLutLwrve;
        }

        if (m_tmoLutLwrve !=0)
        {
            windowParam.tmoLutLwrve =
                (EetfLutGenerator::EetfLwrves)m_tmoLutLwrve;
        }

        if (windowParam.windowNum < 
                    (unsigned)Utility::CountBits(m_pLwDisplay->m_UsableWindowsMask))
        {
            UINT32 index = 0;

            if (!pLwDisplay->m_DispCap.flexibleWinMapSupport)
            {
                // If flexible windows mapping is not enabled, then apply
                // default GV100 static mapping.
                // If flexible windows mapping is enabled, then lets assign
                // all the defined windows in "-windows" to m_headWindowMap[0]
                // we'll then refer to that index to when enabling displayPanel.
                index = windowParam.windowNum/2;
            }
            else
            {
                if (windowParam.headNum < 
                            (unsigned)Utility::CountBits(m_pLwDisplay->m_UsableHeadsMask))
                {
                    index = windowParam.headNum;
                }
            }

            WindowParams mapParams = m_headWindowMap[index];
            mapParams.push_back(windowParam);
            m_headWindowMap[index] = mapParams;
        }
    }

    bool m_withAllSors = false;
    bool m_WithAllHeads = false;

    if (m_WithAllHeads)
    {
        m_heads = "0,1,2,3";
    }
    if (m_withAllSors)
    {
        m_sors = "0,1,2,3";
    }

    if (m_heads != "")
    {
        m_szHeads = Tokenize(m_heads, ",");
    }
    if (m_sors != "")
    {
        m_szSors = Tokenize(m_sors, ",");
    }

    vector<string>szResolution;
    if (m_resolution != "")
    {
       szResolution = Tokenize(m_resolution, "x");
       if (szResolution.size() != 4)
       {
           Printf(Tee::PriHigh,
               " Invalid input resolution \n");
           return RC::SOFTWARE_ERROR;
       }
       m_dispResolution.width = atoi(szResolution[0].c_str());
       m_dispResolution.height = atoi(szResolution[1].c_str());
       m_dispResolution.depth = atoi(szResolution[2].c_str());
       m_dispResolution.refreshrate = atoi(szResolution[3].c_str());
    }

    return rc;
}

RC LwDisplayLowPower::TestDisplayPanel(DisplayPanel *pDisplayPanel, UINT32 configNumber)
{
    StickyRC rc;
    UINT32 prevLoadVCount = 0;
    UINT32 newLoadVCount = 0;
    UINT32 counter, i = 0;
    UINT32 lwrPstate = 0;
    UINT32 nextPstate = 0;
    UINT32 oldPState = 0;
    UINT32 delayTimeMs;
    bool bDidFailTest = false;
    Random m_Random;
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    Perf::PerfPoint perfPt;

    pDisplayPanel->pModeset->setDispMode(displayMode);

    if (ENABLE_HDR_SETTINGS == displayMode &&
        !m_pDisplay->GetOwningDisplaySubdevice()->IsEmOrSim() &&
        !pDisplayPanel->HDRCapable(m_pDisplay)
        )
    {
        return RC::ILWALID_DISPLAY_TYPE;
    }
    else 
    {
        if (m_colorgamut)
        {
            pDisplayPanel->pModeset->m_color = (enum ColorGamut)m_colorgamut;
        }
    }

    if (m_semiplanar)
    {
        pDisplayPanel->pModeset->m_semiPlanar = m_semiplanar;
    }

    if (!pDisplayPanel->bActiveModeset)
    {
        Printf(Tee::PriHigh," %s : Calling allocate Window and Surface \n",__FUNCTION__);
        CHECK_RC(pDisplayPanel->pModeset->AllocateWindowsAndSurface());
        Printf(Tee::PriHigh," %s : Calling SetLwstom Settings \n",__FUNCTION__);
        CHECK_RC(pDisplayPanel->pModeset->SetLwstomSettings());
        Printf(Tee::PriHigh," %s : Calling Setup channel image \n",__FUNCTION__);
        CHECK_RC(pDisplayPanel->pModeset->SetupChannelImage());
    }

/*    if (m_enableVRR)
    {
        // On fmodel we can't run this test due to lack of fmodel support, neither can A-Model
        if (!(Platform::GetSimulationMode() == Platform::Fmodel))
        {
            UINT32 retries = 10;
            do
            {
                rc = m_pDisplay->GetDisplayPortMgr()->StartVrrAuthentication(pDisplayPanel->displayId);
                if (rc == OK)
                {
                    Printf(Tee::PriHigh, "LwVRRDispTest::%s VRR authentication success \n", __FUNCTION__);
                    break;
                }
                Printf(Tee::PriHigh, "LwVRRDispTest::%s VRR authentication failed .retrying \n", __FUNCTION__);

                m_pDisplay->GetDisplayPortMgr()->ResetVrrAuthentication(pDisplayPanel->displayId);
            } while (--retries);
        }
    }*/

    if (pDisplayPanel->pModeset->IsPanelTypeDP())
    {
        Modeset_DP* pDpModeset = (Modeset_DP*)(pDisplayPanel->pModeset);

        // Force custom link config if requested by test params
        CHECK_RC(pDpModeset->DoDpLinkTraining());
    }

    pDisplayPanel->pModeset->ForceDSCBitsPerPixel(m_force_DSC_BitsPerPixelX16);
    Printf(Tee::PriHigh," %s : Doing SetMode\n",__FUNCTION__);
    CHECK_RC(pDisplayPanel->pModeset->SetMode());
    pDisplayPanel->pModeset->m_modesetDone = TRUE;
    
    if (m_immFlip)
    {
        // Enable/Disable Immediate flip if requested
        for (UINT32 i = 0; i < pDisplayPanel->winParams.size(); i++)
        {
            pDisplayPanel->windowSettings[i].winSettings.beginMode = 
                LwDispWindowSettings::BEGIN_MODE::IMMEDIATE;

            pDisplayPanel->windowSettings[i].winSettings.minPresentInt = 0;
        }
        CHECK_RC(pDisplayPanel->pModeset->SendInterlockedUpdates(pDisplayPanel));
    }

    if (m_enableVRR)
    {
        pDisplayPanel->bEnableVRR = TRUE;

        pDisplayPanel->pModeset->EnableVRR(pDisplayPanel, false /* m_legacyVrr */);

        pDisplayPanel->pModeset->SetVRRSettings(pDisplayPanel->bEnableVRR, false /* m_legacyVrr */);

        CHECK_RC(pDisplayPanel->pModeset->SetWindowImage(pDisplayPanel));
        prevLoadVCount = getLoadVCounter(pDisplayPanel->head);

        Printf(Tee::PriHigh, "LwVRRDispTest::%s number of frames set to %d\n", __FUNCTION__, m_numFrames);
        counter = m_numFrames;

        if (m_osmMclkSwitch)
        {
            // Cache previous P-state if P-state is supported
            if (!pPerf->HasPStates())
            {
                Printf(Tee::PriLow,
                    "P-state not supported\n");
                m_osmMclkSwitch = false;
            }
            else
            {
                Printf(Tee::PriHigh, "LwVRRDispTest::%s Verifying One Shot Mode mclk switch\n", __FUNCTION__);
                CHECK_RC(pPerf->GetLwrrentPState(&oldPState));
                lwrPstate = oldPState;

                if (counter < 50)
                {
                    // Increase number of frames to 50 to atleast do 5 mclk switches during test
                    Printf(Tee::PriHigh, "LwVRRDispTest::%s Increasing number of frames to %d\n", __FUNCTION__, 50);
                    counter = 50;
                }
            }
        }

        while (counter)
        {
            Printf(Tee::PriHigh, "*************** [LwVRRDispTest SetImage] - Frame #%d Start.********************\n", ++i);
            CHECK_RC(pDisplayPanel->pModeset->SendInterlockedUpdates(pDisplayPanel));

            newLoadVCount = getLoadVCounter(pDisplayPanel->head);
            if ((newLoadVCount - prevLoadVCount) > 1)
            {
                Printf(Tee::PriHigh, "############### TEST FAILED ################\n");
                bDidFailTest |= true;
            }

            CHECK_RC(checkDispUnderflow(pDisplayPanel->head));
            prevLoadVCount = newLoadVCount;
            counter--;
            
            // Printf(Tee::PriError,
            //    "MSCG: Frame %d: actual efficiency %2d%%\n",  counter, getMscgEfficiency());

            if (m_osmMclkSwitch)
            {
                // Do Pstate switch only after 10 frames
                if ((counter % 10) == 0)
                {
                    // Toggle between P0 and P8
                    nextPstate = (lwrPstate == 8) ? 0 : 8;

                    CHECK_RC(pPerf->ForcePState(nextPstate,
                                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));

                    lwrPstate = nextPstate;

                    if (OK != pPerf->GetLwrrentPerfPoint(&perfPt))
                    {
                        Printf(Tee::PriLow,
                            "Getting current perf point failed\n");
                    }

                    if (perfPt.PStateNum != nextPstate)
                    {
                        Printf(Tee::PriLow,
                            "Failed to set perf state\n");
                    }

                    // Check for underflow after P-state switch
                    CHECK_RC(checkDispUnderflow(pDisplayPanel->head));
                }
            }

            // wait for random time between 16-100ms
            delayTimeMs = m_Random.GetRandom(17,100);
            Printf(Tee::PriHigh, "*************** [LwVRRDispTest Frame] - Delay After frame = %dms ********************\n", delayTimeMs);
            Utility::Delay(delayTimeMs * 1000);
        }

        // Restore P-state to original value if changed
        if (m_osmMclkSwitch)
        {
            CHECK_RC(pPerf->ForcePState(oldPState,
                        LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
        }
    }
    else
    {
        LwU32 elapsedMs = m_runOnEmu ? 10 : 2000; // 2 sec. for silicon sleep time; 10 ms for emulator sleep time

        // Measure and report efficiency over 10 sample periods
        for (LwU32 i = 1; i <= MSCG_EFFICIENCY_CHECK_LOOPS; ++i)
        {
            Sleep(elapsedMs, m_runOnEmu, GetBoundGpuSubdevice());

            Printf(Tee::PriError,
                "MSCG: Probe %d: actual efficiency %2d%%\n",  i, getMscgEfficiency());
        }
    }

    if (m_manualVerif)
    {
        Printf(Tee::PriHigh,
            "\n WOULD you like to DETACH type YES\n");
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        string inputNumStr = pInterface->GetLine();
        if (inputNumStr.find("YES") != string::npos)
        {
            // Free Window Channel and Clear Custom Settings
            rc = pDisplayPanel->pModeset->FreeWindowsAndSurface();
            rc = pDisplayPanel->pModeset->Detach();
            pDisplayPanel->pModeset->ClearLwstomSettings();
            pDisplayPanel->pModeset->m_modesetDone = FALSE;
            pDisplayPanel->winParams.clear();
        }
        delete pInterface;
        return rc;
    }
    else
    {
        pDisplayPanel->pModeset->ClearLwstomSettings();
        Printf(Tee::PriHigh," %s : Doing Detach Display\n",__FUNCTION__);
        rc = pDisplayPanel->pModeset->Detach();
        if (rc != OK)
        {
            Printf(Tee::PriHigh," %s : Detach Display Failed rc = %d\n",__FUNCTION__, (UINT32) rc);
        }
        // Free Window Channel and clear custom Settings
        Printf(Tee::PriHigh," %s : Calling Free WindowSurface \n",__FUNCTION__);
        rc = pDisplayPanel->pModeset->FreeWindowsAndSurface();
        pDisplayPanel->pModeset->m_modesetDone = FALSE;
    }

    return rc;
}

UINT32 LwDisplayLowPower::getLoadVCounter(UINT32 head)
{
     RC rc;
     UINT32    data32;
     UINT32 loadv = 0;

     data32 = GetBoundGpuSubdevice()->RegRd32(LW_PDISP_RG_IN_LOADV_COUNTER(head));
     loadv = DRF_VAL(_PDISP, _RG_IN_LOADV_COUNTER, _VALUE, data32);
     Printf(Tee::PriHigh,"LwDisplayLowPower::%s LOADV_COUNTER on head(%d) is %d\n", __FUNCTION__, head, loadv);

     return loadv;
}

RC LwDisplayLowPower::checkDispUnderflow(UINT32 head)
{
    RC status;
    UINT32    data32;

    data32 = GetBoundGpuSubdevice()->RegRd32(LW_PDISP_RG_UNDERFLOW(head));
    if (DRF_VAL( _PDISP, _RG_UNDERFLOW, _UNDERFLOWED, data32))
    {
        Printf(Tee::PriHigh,"LwDisplayLowPower::%s Underflow Detected. LW_PDISP_RG_UNDERFLOW(%d)_UNDERFLOWED_YES\n", __FUNCTION__, head);
        status = RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,"LwDisplayLowPower::%s NO underflow detected on head(%d)\n", __FUNCTION__, head);
    }

    return status;
}

RC LwDisplayLowPower::InitializeTestDispPanel(DisplayPanel *pTestDispPanel)
{
	RC rc;
	pTestDispPanel->head = atoi(m_heads.c_str());
    pTestDispPanel->sor = atoi(m_sors.c_str());
    pTestDispPanel->resolution = m_dispResolution;

    // Setting Window Parameters
    if (m_headWindowMap.find(pTestDispPanel->head) != m_headWindowMap.end())
    {
        pTestDispPanel->winParams = m_headWindowMap[pTestDispPanel->head];
    }
    if (!pTestDispPanel->winParams.size())
    {
        Printf(Tee::PriError,
            " Invalid window-head combination Head - %d\n", pTestDispPanel->head);
        return RC::SOFTWARE_ERROR;
    }
    CHECK_RC(pTestDispPanel->pModeset->Initialize());
	return rc;
}	

vector<string> LwDisplayLowPower::Tokenize(const string &str,
                                      const string &delimiters)
{
    vector<string> tokens;
    // Skip delimiters at beginning.
    string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    string::size_type pos     = str.find_first_of(delimiters, lastPos);

    while (string::npos != pos || string::npos != lastPos)
    {
        // Found a token, add it to the vector.
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        // Skip delimiters.  Note the "not_of"
        lastPos = str.find_first_not_of(delimiters, pos);
        // Find next "non-delimiter"
        pos = str.find_first_of(delimiters, lastPos);
    }

    return tokens;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDisplayLowPower::Cleanup()
{
    return OK;
}

UINT32 LwDisplayLowPower::getMscgEfficiency()
{
    RC rc;
    PgStats objPgStats(GetBoundGpuSubdevice());
    UINT32 actualEfficiency = 0;
    UINT32 gatingCount = 0;
    UINT32 avgGatingUs = 0;
    UINT32 avgEntryUs = 0;
    UINT32 avgExitUs = 0;

    CHECK_RC(objPgStats.getPGPercent(LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS, m_runOnEmu, &actualEfficiency,
                                        &gatingCount, &avgGatingUs, &avgEntryUs, &avgExitUs));

    return actualEfficiency;
}

//! \brief Helpler function to put the current thread to sleep for a given number(base on ptimer) of milliseconds,
//!         giving up its CPU time to other threads.
//!         Warning, this function is not for precise delays.
//! \param miliseconds [in ]: Milliseconds to sleep
//! \param runOnEmu [in]: It is on emulation or not
//! \param gpuSubdev [in]: Pointer to gpu subdevice
//!
void LwDisplayLowPower::Sleep(LwU32 miliseconds, LwBool runOnEmu, GpuSubdevice *gpuSubdev)
{
    if(runOnEmu)
        GpuUtility::SleepOnPTimer(miliseconds, gpuSubdev);
    else
        Tasker::Sleep(miliseconds);
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwDisplayLowPower, RmTest,
    "Simple test to demonstrate usage of SetMode() API");

CLASS_PROP_READWRITE(LwDisplayLowPower, heads, string,
                     "head number to be used in the test");
CLASS_PROP_READWRITE(LwDisplayLowPower, windows, string,
                     "-windows windowNum-width-height, windowNum-width-height");
CLASS_PROP_READWRITE(LwDisplayLowPower, headwin, string,
                     "-headwin headnum-windowNum-width-height, headnum-windowNum-width-height");
CLASS_PROP_READWRITE(LwDisplayLowPower, sors, string,
                     "sor number to be used in the test");
CLASS_PROP_READWRITE(LwDisplayLowPower, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(LwDisplayLowPower, resolution, string,
                     "Desired raster size ex : 800x600x32x60");
CLASS_PROP_READWRITE(LwDisplayLowPower, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(LwDisplayLowPower,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(LwDisplayLowPower,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(LwDisplayLowPower, force_DSC_BitsPerPixelX16, UINT32,
                      "Force DSC with specified bits per pixel. This is actual bits per pixel multiplied by 16, default = 0");                  
CLASS_PROP_READWRITE(LwDisplayLowPower, enable_mode, string,
                      "enables SDR/HDR in Pre&Post Comp, default = bypass");
CLASS_PROP_READWRITE(LwDisplayLowPower, inputLutLwrve, UINT32,
                     "Eotf Lwrve required for the image");
CLASS_PROP_READWRITE(LwDisplayLowPower, outputLutLwrve, UINT32,
                     "Oetf Lwrve required for the monitor");
CLASS_PROP_READWRITE(LwDisplayLowPower, tmoLutLwrve, UINT32,
                     "Eetf Lwrve required TMO Block");
CLASS_PROP_READWRITE(LwDisplayLowPower, colorgamut, UINT32,
                     "REC709, DCI-P3, BT2020, ADOBE");
CLASS_PROP_READWRITE(LwDisplayLowPower, semiplanar, bool,
                     "Y- Plane and UV- Planes are saperated out for the image");
CLASS_PROP_READWRITE(LwDisplayLowPower, enableVRR, bool,
                     "Enable VRR");
CLASS_PROP_READWRITE(LwDisplayLowPower, numFrames, UINT32,
                     "number of frames to go through, default = 3");
CLASS_PROP_READWRITE(LwDisplayLowPower, immFlip, bool,
                     "use immediate flip(Tearing), default = false");
CLASS_PROP_READWRITE(LwDisplayLowPower, osmMclkSwitch, bool,
                     "Test One shot mode Mclk switch, default = false");
