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
//! \file rmt_LwVRRDispTest
//!       This test calls lwdisplay library APIs to verify VRR functionality

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "lwmisc.h"
#include "gpu/display/dpmgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/display/evo_dp.h"
#include "class/clc37d.h" // LWC37D_CORE_CHANNEL_DMA
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/perf/perfsub.h"
#include "modesetlib.h"
#include "core/include/simpleui.h"
#include "random.h"

using namespace std;
using namespace Modesetlib;

class LwVRRDispTest : public RmTest
{
public:
    LwVRRDispTest();
    virtual ~LwVRRDispTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC TestDisplayPanel(DisplayPanel *pDisplayPanel);
    RC RestoreDisplayPanel(DisplayPanel *pDisplayPanel);

    RC GetCrcFileName(DisplayPanel *pDisplayPanel,
                       string& crcFileName,
                       string& fileDirPath);

    vector<string> Tokenize(const string &str,
        const string &delimiters);

    RC ParseCmdLineArgs();
    RC checkDispUnderflow(UINT32 head);
    UINT32 getLoadVCounter(DisplayPanel *pDisplayPanel);

    SETGET_PROP(heads, string);
    SETGET_PROP(sors, string);
    SETGET_PROP(windows, string);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(resolution, string); //Config raster size through cmdline
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(disableVRR, bool);
    SETGET_PROP(numFrames, UINT32);
    SETGET_PROP(legacyVrr, bool);
    SETGET_PROP(immFlip, bool);
    SETGET_PROP(osmMclkSwitch, bool);

private:
    Display *m_pDisplay;
    string m_heads;
    string m_windows;
    string m_sors;
    string m_protocol;
    string m_resolution;
    UINT32 m_pixelclock;
    bool m_manualVerif;
    bool m_onlyConnectedDisplays;
    bool m_generate_gold;
    bool m_disableVRR;
    bool m_legacyVrr;
    bool m_immFlip;
    bool m_osmMclkSwitch;
    UINT32 m_numFrames;
    DisplayResolution m_dispResolution;
    vector<string>m_szSors;
    vector<string>m_szHeads;
    map<UINT32, WindowParams>m_headWindowMap;
};

LwVRRDispTest::LwVRRDispTest()
{
    SetName("LwVRRDispTest");
    m_windows = "0,1";
    m_protocol = "SINGLE_TMDS_A,DP,DSI";
    m_heads = "0";
    m_sors = "0";
    m_generate_gold = false;
    m_onlyConnectedDisplays = false;
    m_manualVerif = false;
    m_pixelclock = 0;
    m_disableVRR = false;
    m_numFrames = 3;
    m_legacyVrr = false;
    m_immFlip = false;
    m_osmMclkSwitch = false;
}

//! \brief LwDispBringupPass1 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwVRRDispTest::~LwVRRDispTest()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwVRRDispTest::IsTestSupported()
{
     m_pDisplay = GetDisplay();   
    // On fmodel we can't run this test due to lack of fmodel support, neither can A-Model
    if ((Platform::GetSimulationMode() == Platform::Fmodel) ||
        (Platform::GetSimulationMode() == Platform::Amodel))
    {
        return "Not Supported on F-Model or A-Model";
    }

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
RC LwVRRDispTest::Setup()
{
    m_pDisplay = GetDisplay();
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

RC LwVRRDispTest::Run()
{
    RC rc;

    // allocate core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // Enumerate Display Panels first
    vector <DisplayPanel *>pDisplayPanels;
    vector<pair<DisplayPanel *, DisplayPanel *>> dualSSTPairs;
    DisplayPanelHelper dispPanelHelper(m_pDisplay);
    CHECK_RC(dispPanelHelper.EnumerateDisplayPanels(m_protocol, pDisplayPanels, dualSSTPairs, !m_onlyConnectedDisplays));
    UINT32 nSinglePanels = (UINT32) pDisplayPanels.size();
    UINT32 nDualSSTPanels = (UINT32) dualSSTPairs.size();
    UINT32 numPanels = nSinglePanels + nDualSSTPanels;

    if (m_manualVerif)
    {
        UINT32 panelIndex = 0;
        do
        {
            bool bExit = false;
            CHECK_RC(dispPanelHelper.ListAndSelectDisplayPanel(panelIndex, bExit));
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
            if (panelIndex > (nSinglePanels - 1))
            {
                pTestDispPanel = dualSSTPairs[panelIndex - nSinglePanels].first;
            }
            else
            {
                pTestDispPanel = pDisplayPanels[panelIndex];
            }

            CHECK_RC(pTestDispPanel->pModeset->Initialize());

            rc = pTestDispPanel->pModeset->SelectUserInputs(pTestDispPanel, bExit);
            if ((rc != OK) || bExit)
            {
                Printf(Tee::PriHigh,
                    " EXIT \n");
                break;
            }
            // Test Display panel
            CHECK_RC(TestDisplayPanel(pTestDispPanel));
        }while (true);
    }
    else // Auto verification
    {
        CHECK_RC(ParseCmdLineArgs());

        for (UINT32 panelIndex = 0; panelIndex < pDisplayPanels.size(); panelIndex++)
        {
            if (panelIndex >= m_szHeads.size())
            {
                Printf(Tee::PriHigh,"%s: Unspecified HEAD number argument for display panel 0x%x\n",
                    __FUNCTION__, (UINT32)pDisplayPanels[panelIndex]->displayId);
                break;
            }

            if (panelIndex >= m_szSors.size())
            {
                Printf(Tee::PriHigh,"%s: Unspecified SOR number argument for display panel 0x%x\n",
                    __FUNCTION__, (UINT32)pDisplayPanels[panelIndex]->displayId);
                break;
            }

            pDisplayPanels[panelIndex]->head = atoi(m_szHeads[panelIndex].c_str());
            pDisplayPanels[panelIndex]->resolution = m_dispResolution;
            if (m_headWindowMap.find(pDisplayPanels[panelIndex]->head) != m_headWindowMap.end())
            {
                Printf(Tee::PriHigh,"%s: m_headWindowMap.find FAILED -> set winParams to m_headWindowMap[%d]\n",
                    __FUNCTION__, (UINT32)pDisplayPanels[panelIndex]->head);
                pDisplayPanels[panelIndex]->winParams = m_headWindowMap[pDisplayPanels[panelIndex]->head];
            }

            if (!pDisplayPanels[panelIndex]->winParams.size())
            {
                Printf(Tee::PriHigh,
                    " Invalid window-head combination Head - %d\n", pDisplayPanels[panelIndex]->head);
                return RC::SOFTWARE_ERROR;
            }

            pDisplayPanels[panelIndex]->sor = atoi(m_szSors[panelIndex].c_str());
            CHECK_RC(pDisplayPanels[panelIndex]->pModeset->Initialize());
            CHECK_RC(TestDisplayPanel(pDisplayPanels[panelIndex]));
        }
    }

    // Clean up display panels
    for (UINT32 panelIndex = 0; panelIndex < pDisplayPanels.size(); panelIndex++)
    {
        if (pDisplayPanels[panelIndex]->bActiveModeset)
        {
            Printf(Tee::PriHigh,
                "%s: Disable VRR and clean up displayId: 0x%x\n", __FUNCTION__,
                (UINT32)pDisplayPanels[panelIndex]->displayId);
            CHECK_RC(RestoreDisplayPanel(pDisplayPanels[panelIndex]));
        }
    }

    return rc;
}

RC LwVRRDispTest::ParseCmdLineArgs()
{
    RC rc;
    LwDisplay *m_pLwDisplay = static_cast<LwDisplay *>(GetDisplay());
    if (m_pixelclock != 0)
    {
        m_dispResolution.pixelClockHz = m_pixelclock;
    }

    vector<string>windowStrs;
    vector<string>windowTokens;

    WindowParams winParams;
    // "windowNum0-width-height, windowNum1-width-height"
    if (m_windows != "")
    {
        windowStrs = Tokenize(m_windows, ",");
    }

    for (UINT32 i=0; i < windowStrs.size(); i++)
    {
        windowTokens = Tokenize(windowStrs[i], "-");
        WindowParam windowParam;
        windowParam.windowNum = atoi(windowTokens[0].c_str());
        windowParam.width = atoi(windowTokens[1].c_str());
        windowParam.height = atoi(windowTokens[2].c_str());
        if (windowParam.windowNum <
                (unsigned)Utility::CountBits(m_pLwDisplay->m_UsableWindowsMask))
        {
            WindowParams mapParams = m_headWindowMap[windowParam.windowNum/2];
            mapParams.push_back(windowParam);
            m_headWindowMap[windowParam.windowNum/2] = mapParams;
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

UINT32 LwVRRDispTest::getLoadVCounter(DisplayPanel *pDisplayPanel)
{
     RC rc;
     UINT32    data32;
     UINT32 loadv = 0;

#if defined(TEGRA_MODS)
     data32 = pDisplayPanel->pModeset->Disp_reg_dump (LW_PDISP_RG_IN_LOADV_COUNTER(pDisplayPanel->head) - 0x610000);
#else
     data32 = GetBoundGpuSubdevice()->RegRd32(LW_PDISP_RG_IN_LOADV_COUNTER(pDisplayPanel->head));
#endif
     loadv = DRF_VAL(_PDISP, _RG_IN_LOADV_COUNTER, _VALUE, data32);
     Printf(Tee::PriHigh,"LwVRRDispTest::%s LOADV_COUNTER on head(%d) is %d\n", __FUNCTION__, pDisplayPanel->head, loadv);

     return loadv;
}

RC LwVRRDispTest::checkDispUnderflow(UINT32 head)
{
    RC status;
    UINT32    data32;

    data32 = GetBoundGpuSubdevice()->RegRd32(LW_PDISP_RG_UNDERFLOW(head));
    if (DRF_VAL( _PDISP, _RG_UNDERFLOW, _UNDERFLOWED, data32))
    {
        Printf(Tee::PriHigh,"LwVRRDispTest::%s Underflow Detected. LW_PDISP_RG_UNDERFLOW(%d)_UNDERFLOWED_YES\n", __FUNCTION__, head);
        status = RC::SOFTWARE_ERROR;
    }
    else
    {
        Printf(Tee::PriHigh,"LwVRRDispTest::%s NO underflow detected on head(%d)\n", __FUNCTION__, head);
    }

    return status;
}

RC LwVRRDispTest::TestDisplayPanel(DisplayPanel *pDisplayPanel)
{
    RC rc;
#if !defined(TEGRA_MODS)
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
#endif
    UINT32 prevLoadVCount = 0;
    UINT32 newLoadVCount = 0;
    UINT32 counter, i = 0;
    UINT32 retries = 10;
    UINT32 lwrPstate = 0;
    UINT32 nextPstate = 0;
    UINT32 oldPState = 0;
    UINT32 delayTimeMs;
    bool bDidFailTest = false;
    Random m_Random;
    m_Random.SeedRandom(m_TestConfig.Seed());
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    Perf::PerfPoint perfPt;

    pDisplayPanel->winParams[0].width = pDisplayPanel->resolution.width;
    pDisplayPanel->winParams[0].height = pDisplayPanel->resolution.height;

    if (!pDisplayPanel->bActiveModeset)
    {
        CHECK_RC(pDisplayPanel->pModeset->AllocateWindowsAndSurface());
        CHECK_RC(pDisplayPanel->pModeset->SetLwstomSettings());
        CHECK_RC(pDisplayPanel->pModeset->SetupChannelImage());
    }

    if (pDisplayPanel->orProtocol.find("DSI") == string::npos)
    {
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

    CHECK_RC(pDisplayPanel->pModeset->SetMode());

#if !defined(TEGRA_MODS)
    pLwDisplay->DumpStateCache();
#endif

    pDisplayPanel->bEnableVRR = !m_disableVRR;

    pDisplayPanel->pModeset->EnableVRR(pDisplayPanel, m_legacyVrr);

    pDisplayPanel->pModeset->SetVRRSettings(pDisplayPanel->bEnableVRR, m_legacyVrr);

    // Enable/Disable Immediate flip if requested
    for (UINT32 i = 0; i < pDisplayPanel->winParams.size(); i++)
    {
        pDisplayPanel->windowSettings[i].winSettings.beginMode = m_immFlip ?
            LwDispWindowSettings::BEGIN_MODE::IMMEDIATE:
            LwDispWindowSettings::BEGIN_MODE::NON_TEARING;
    }

#if !defined(TEGRA_MODS)
    Printf(Tee::PriHigh, "LwVRRDispTest::%s DUMP state cache [after enable VRR]\n", __FUNCTION__);
    pLwDisplay->DumpStateCache();
#endif

    CHECK_RC(pDisplayPanel->pModeset->SetWindowImage(pDisplayPanel));
    prevLoadVCount = getLoadVCounter(pDisplayPanel);

    Printf(Tee::PriHigh, "LwVRRDispTest::%s number of frames set to %d\n", __FUNCTION__, m_numFrames);
    counter = m_numFrames;

    if (m_osmMclkSwitch)
    {
        // Enable OSM mclk switch only when method based VRR is used
        if (m_legacyVrr)
        {
            m_osmMclkSwitch = false;
        }
        else
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
    }

    while (counter)
    {
        Printf(Tee::PriHigh, "*************** [LwVRRDispTest SetImage] - Frame #%d Start.********************\n", ++i);
        CHECK_RC(pDisplayPanel->pModeset->SendInterlockedUpdates(pDisplayPanel));

        newLoadVCount = getLoadVCounter(pDisplayPanel);
        if ((newLoadVCount - prevLoadVCount) > 1)
        {
            Printf(Tee::PriHigh, "############### TEST FAILED ################\n");
            bDidFailTest |= true;
        }

        CHECK_RC(checkDispUnderflow(pDisplayPanel->head));
        prevLoadVCount = newLoadVCount;
        counter--;

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

    if (bDidFailTest)
    {
        rc = RC::UNEXPECTED_RESULT;
    }

    return rc;
}

RC LwVRRDispTest::RestoreDisplayPanel(DisplayPanel *pDisplayPanel)
{
    RC rc;
#if !defined(TEGRA_MODS)
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
#endif

    pDisplayPanel->bEnableVRR = false;

    // Disable VRR in RM for legacy VRR only
    if (m_legacyVrr)
    {
        pDisplayPanel->pModeset->DisableVRR(pDisplayPanel);
    }

    pDisplayPanel->pModeset->SetVRRSettings(pDisplayPanel->bEnableVRR, m_legacyVrr);
#if !defined(TEGRA_MODS)
    Printf(Tee::PriHigh, "%s: DUMP state cache [after disabling VRR]\n", __FUNCTION__);
    pLwDisplay->DumpStateCache();
#endif

    // Clear Custom Settings
    CHECK_RC(pDisplayPanel->pModeset->ClearLwstomSettings());

    rc = pDisplayPanel->pModeset->Detach();

    // Free Window Channel
    CHECK_RC(pDisplayPanel->pModeset->FreeWindowsAndSurface());

    return rc;
}

RC LwVRRDispTest::GetCrcFileName(DisplayPanel *pDisplayPanel,
                                  string& szCrcFileName,
                                  string& fileDirPath)
{
    RC rc;
    char        crcFileName[50];
    string      lwrProtocol = "";
    UINT32      displayClass = m_pDisplay->GetClass();
    string      dispClassName = "";
    string      goldenCrcDir = "LwDisplayLib/Golden/LwVRRDispTest";

    CHECK_RC(DTIUtils::Mislwtils:: ColwertDisplayClassToString(displayClass,
        dispClassName));
    fileDirPath = goldenCrcDir + dispClassName + "/";
    sprintf(crcFileName, "LwVRRDispTest_%s_%dx%d.xml",
        pDisplayPanel->orProtocol.c_str(),
        pDisplayPanel->resolution.width,
        pDisplayPanel->resolution.height);

    szCrcFileName = crcFileName;

    return rc;
}

vector<string> LwVRRDispTest::Tokenize(const string &str,
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
RC LwVRRDispTest::Cleanup()
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
JS_CLASS_INHERIT(LwVRRDispTest, RmTest,
    "Simple test to verify VRR methods based feature");

CLASS_PROP_READWRITE(LwVRRDispTest, heads, string,
                     "head number to be used in the test");
CLASS_PROP_READWRITE(LwVRRDispTest, windows, string,
                     "-window windowNum-width-height, windowNum-width-height");
CLASS_PROP_READWRITE(LwVRRDispTest, sors, string,
                     "sor number to be used in the test");
CLASS_PROP_READWRITE(LwVRRDispTest, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(LwVRRDispTest, resolution, string,
                     "Desired raster size (small/large)");
CLASS_PROP_READWRITE(LwVRRDispTest, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(LwVRRDispTest,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(LwVRRDispTest,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(LwVRRDispTest, generate_gold, bool,
                     "generates gold files, default = 0");
CLASS_PROP_READWRITE(LwVRRDispTest, disableVRR, bool,
                     "disable VRR (for testing)");
CLASS_PROP_READWRITE(LwVRRDispTest, numFrames, UINT32,
                     "number of frames to go through, default = 3");
CLASS_PROP_READWRITE(LwVRRDispTest, legacyVrr, bool,
                     "use Legacy VRR (for testing), default = false");
CLASS_PROP_READWRITE(LwVRRDispTest, immFlip, bool,
                     "use immediate flip(Tearing), default = false");
CLASS_PROP_READWRITE(LwVRRDispTest, osmMclkSwitch, bool,
                     "Test One shot mode Mclk switch, default = false");
