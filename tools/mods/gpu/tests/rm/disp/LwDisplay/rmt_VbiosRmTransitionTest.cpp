/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_VbiosRmTransitionTest.cpp
//! \brief This test calls lwdisplay library APIs to verify VBIOS/RM transition.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"         // For CLASS_PROP_READWRITE
#include "core/include/jscript.h"        // For JavaScript linkage
#include "core/include/cnslmgr.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/dispchan.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "class/clc37d.h"               // LWC37D_CORE_CHANNEL_DMA
#include "disp/v03_00/dev_disp.h"       // LW_PDISP_RG_UNDERFLOW
#include "core/include/platform.h"
#include "core/include/stdoutsink.h"
#include "core/include/utility.h"
#include "gpu/perf/powermgmtutil.h"
#include "core/include/simpleui.h"
#include "modesetlib.h"

#define DEFAULT_GPU_SUSPEND_TIME        5                                   // 5 Sec
#define DEFAULT_GPU_SUSPEND_TIME_MS     (DEFAULT_GPU_SUSPEND_TIME * 1000)
#define DEFAULT_SYSTEM_SUSPEND_TIME     15                                  // 15 Sec
#define DEFAULT_SYSTEM_HIBERNATE_TIME   30                                  // 30 Sec

using namespace std;
using namespace Modesetlib;

class VbiosRmTransitionTest : public RmTest
{
public:
    VbiosRmTransitionTest();
    virtual ~VbiosRmTransitionTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC ParseCmdLineArgs();

    RC GetCrcFileName(DisplayPanel *pDisplayPanel,
                       string& crcFileName,
                       string& fileDirPath);

    SETGET_PROP(subtest, string);
    SETGET_PROP(heads, string);
    SETGET_PROP(sors, string);
    SETGET_PROP(windows, string);
    SETGET_PROP(protocol, string);      // Config protocol through cmdline
    SETGET_PROP(resolution, string);    // Config raster size through cmdline
    SETGET_PROP(pixelclock, UINT32);    // Config pixel clock through cmdline
    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(legacyVrr, bool);

private:
    Display *m_pDisplay;
    PowerMgmtUtil m_powerMgmtUtil;
    DisplayPanelHelper *m_pDispPanelHelper;

    RC RunBootUpSubtest(DisplayPanel *pDisplayPanel);
    RC RunSuspendResumeSubtest(DisplayPanel *pDisplayPanel);
    RC RunDriverUnloadLoadSubtest(DisplayPanel *pDisplayPanel);
    RC RunBSODSubtest(vector<DisplayPanel *> pDisplayPanels);
    RC RunOneshotSubtest(DisplayPanel *pDisplayPanel);

    RC AllocateChannels(DisplayPanel *pDisplayPanel);
    RC DeallocateChannels(DisplayPanel *pDisplayPanel);
    RC CheckDispUnderflow(DisplayPanel *pDisplayPanel);
    RC DoModeset(DisplayPanel *pDisplayPanel);
    RC DoOneshotModeset(DisplayPanel *pDisplayPanel);
    RC DoStereoModeset(DisplayPanel *pDisplayPanel);
    RC CheckCRC(DisplayPanel *pDisplayPanel);
    RC DoSuspendResume();
    RC SwitchToVGAMode(UINT32 mode);
    RC SwitchToDriverMode();
    RC SetVgaMode(UINT32 mode);

    UINT32 m_VgaBaseOrg;
    bool m_coreAllocated;
    bool m_VgaIsEnabled;
    bool m_testBootUp;
    bool m_testSuspendResume;
    bool m_testLoadUnload;
    bool m_testBsod;
    bool m_testOneshot;
    string m_subtest;
    string m_heads;
    string m_windows;
    string m_sors;
    string m_protocol;
    string m_resolution;
    UINT32 m_pixelclock;
    bool m_onlyConnectedDisplays;
    bool m_manualVerif;
    bool m_generate_gold;
    bool m_legacyVrr;
    DisplayResolution m_dispResolution;
    vector<string> m_szSors;
    vector<string> m_szHeads;
    map<UINT32, WindowParams> m_headWindowMap;
};

#ifdef CLIENT_SIDE_RESMAN
static void RestoreStdout()
{
    Tee::GetStdoutSink()->Enable();
}
#endif

VbiosRmTransitionTest::VbiosRmTransitionTest()
{
    m_pDisplay = GetDisplay();
    m_coreAllocated = false;
    m_VgaIsEnabled = false;
    m_testBootUp = false;
    m_testSuspendResume = false;
    m_testLoadUnload = false;
    m_testBsod = false;
    m_testOneshot = false;
    m_subtest = "bootup";
    m_windows = "0,1";
    m_protocol = "SINGLE_TMDS_A";
    m_heads = "0";
    m_sors = "0";
    m_generate_gold = false;
    m_onlyConnectedDisplays = false;
    m_manualVerif = false;

    SetName("VbiosRmTransitionTest");
}

//! \brief VbiosRmTransitionTest destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
VbiosRmTransitionTest::~VbiosRmTransitionTest()
{
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string VbiosRmTransitionTest::IsTestSupported()
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
RC VbiosRmTransitionTest::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings.
    CHECK_RC(InitFromJs());

    return rc;
}

//! \brief Run the test
//!
//! It will init LwDisplay HW and allocate a core and window/windowImm channels.
//!
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::Run()
{
    RC rc = OK;
    vector<UINT32> headList;

    // Disable VGA mode
    CHECK_RC(Xp::DisableUserInterface());

    // Alocate Core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    m_coreAllocated = true;

    // Parse command line arguments
    CHECK_RC(ParseCmdLineArgs());

    // Enumerate Display Panels first
    vector <DisplayPanel *>pDisplayPanels;
    vector<pair<DisplayPanel *, DisplayPanel *>> dualSSTPairs;
    m_pDispPanelHelper = new DisplayPanelHelper(m_pDisplay);
    CHECK_RC(m_pDispPanelHelper->EnumerateDisplayPanels(m_protocol, pDisplayPanels, dualSSTPairs, !m_onlyConnectedDisplays));
    // index 0 - DisplayPanel
    // index 1 - DisplayPanel
    // index 2 - DualSSTPair
    // so on ..
    UINT32 nSinglePanels = (UINT32) pDisplayPanels.size();
    UINT32 nDualSSTPanels = (UINT32) dualSSTPairs.size();
    UINT32 numPanels = nSinglePanels + nDualSSTPanels;

    if (m_manualVerif)
    {
        UINT32 panelIndex = 0;
        do
        {
            bool bExit = false;
            CHECK_RC(m_pDispPanelHelper->ListAndSelectDisplayPanel(panelIndex, bExit));
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
            if (m_testBootUp)
            {
                // Run the Boot Up subtest.
                CHECK_RC(RunBootUpSubtest(pTestDispPanel));
            }
            else if (m_testSuspendResume)
            {
                // Run the Suspend/Resume subtest.
                CHECK_RC(RunSuspendResumeSubtest(pTestDispPanel));
            }
            else if (m_testLoadUnload)
            {
                // Run the Driver Unload/Load subtest.
                CHECK_RC(RunDriverUnloadLoadSubtest(pTestDispPanel));
            }
            else if (m_testOneshot)
            {
                // Run the Oneshot (legacy mode) subtest.
                CHECK_RC(RunOneshotSubtest(pTestDispPanel));
            }
            else if (m_testBsod)
            {
                // Run the BSOD subtest.
                CHECK_RC(RunBSODSubtest(pDisplayPanels));
            }
        }while (true);
    }
    else // Auto verification
    {
        // Parse command line arguments
        //CHECK_RC(ParseCmdLineArgs());

        for (UINT32 iPanel = 0; iPanel< pDisplayPanels.size(); iPanel++)
        {
            for (UINT32 iHead = 0; iHead < m_szHeads.size(); iHead++)
            {
                pDisplayPanels[iPanel]->head = atoi(m_szHeads[iHead].c_str());
                pDisplayPanels[iPanel]->resolution = m_dispResolution;

                if (m_headWindowMap.find(pDisplayPanels[iPanel]->head) != m_headWindowMap.end())
                {
                    pDisplayPanels[iPanel]->winParams = m_headWindowMap[pDisplayPanels[iPanel]->head];
                }

                if (!pDisplayPanels[iPanel]->winParams.size())
                {
                    Printf(Tee::PriHigh,
                        " Invalid window-head combination Head - %d\n", pDisplayPanels[iPanel]->head);
                    return RC::SOFTWARE_ERROR;
                }

                for (UINT32 iSor = 0; iSor < m_szSors.size(); iSor++)
                {
                    pDisplayPanels[iPanel]->sor = atoi(m_szSors[iSor].c_str());

                    if (m_testBootUp)
                    {
                        // Run the Boot Up subtest.
                        CHECK_RC(RunBootUpSubtest(pDisplayPanels[iPanel]));
                    }
                    else if (m_testSuspendResume)
                    {
                        // Run the Suspend/Resume subtest.
                        CHECK_RC(RunSuspendResumeSubtest(pDisplayPanels[iPanel]));
                    }
                    else if (m_testLoadUnload)
                    {
                        // Run the Driver Unload/Load subtest.
                        CHECK_RC(RunDriverUnloadLoadSubtest(pDisplayPanels[iPanel]));
                    }
                    else if (m_testOneshot)
                    {
                        // Run the Oneshot (legacy mode) subtest.
                        CHECK_RC(RunOneshotSubtest(pDisplayPanels[iPanel]));
                    }
                }
            }
        }

        if (m_testBsod)
        {
            for (UINT32 iPanel = 0; iPanel< pDisplayPanels.size(); iPanel++)
            {
                if ((iPanel < m_szHeads.size()) && (iPanel < m_szSors.size()))
                {
                    pDisplayPanels[iPanel]->head = atoi(m_szHeads[iPanel].c_str());
                    pDisplayPanels[iPanel]->sor = atoi(m_szSors[iPanel].c_str());
                    pDisplayPanels[iPanel]->resolution = m_dispResolution;

                    if (m_headWindowMap.find(pDisplayPanels[iPanel]->head) != m_headWindowMap.end())
                    {
                        pDisplayPanels[iPanel]->winParams = m_headWindowMap[pDisplayPanels[iPanel]->head];
                    }

                    if (!pDisplayPanels[iPanel]->winParams.size())
                    {
                        Printf(Tee::PriHigh,
                            " Invalid window-head combination Head - %d\n", pDisplayPanels[iPanel]->head);
                        return RC::SOFTWARE_ERROR;
                    }
                }
                else
                {
                    pDisplayPanels[iPanel]->head = DEFAULT_PANEL_VALUE;
                    pDisplayPanels[iPanel]->sor = DEFAULT_PANEL_VALUE;
                }
            }

            // Run the BSOD subtest.
            CHECK_RC(RunBSODSubtest(pDisplayPanels));
        }
    }

    if (m_pDispPanelHelper != NULL)
    {
        delete m_pDispPanelHelper;
        m_pDispPanelHelper = NULL;
    }

    CHECK_RC(Xp::EnableUserInterface());

    return rc;
}

//! \brief Parse command line arguments.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::ParseCmdLineArgs(void)
{
    RC rc = OK;
    LwDisplay *m_pLwDisplay = static_cast<LwDisplay *>(GetDisplay());

    if (m_subtest.compare("bootup") == 0)
    {
        m_testBootUp = true;
    }
    else if (m_subtest.compare("suspendresume") == 0)
    {
        m_testSuspendResume = true;
    }
    else if (m_subtest.compare("loadunload") == 0)
    {
        m_testLoadUnload = true;
    }
    else if (m_subtest.compare("bsod") == 0)
    {
        m_testBsod = true;
    }
    else if (m_subtest.compare("oneshot") == 0)
    {
        m_testOneshot = true;
    }
    else
    {
        Printf(Tee::PriHigh,
               " Invalid subtest specified (-subtest %s).\n", m_subtest.c_str());
        return RC::SOFTWARE_ERROR;
    }

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
        windowStrs = DTIUtils::VerifUtils::Tokenize(m_windows, ",");
    }

    for (UINT32 i=0; i < windowStrs.size(); i++)
    {
        windowTokens = DTIUtils::VerifUtils::Tokenize(windowStrs[i], "-");
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
        m_szHeads = DTIUtils::VerifUtils::Tokenize(m_heads, ",");
    }
    if (m_sors != "")
    {
        m_szSors = DTIUtils::VerifUtils::Tokenize(m_sors, ",");
    }

    vector<string>szResolution;
    if (m_resolution != "")
    {
       szResolution = DTIUtils::VerifUtils::Tokenize(m_resolution, "x");
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

RC VbiosRmTransitionTest::GetCrcFileName(DisplayPanel *pDisplayPanel,
                                         string& szCrcFileName,
                                         string& fileDirPath)
{
    RC rc;
    char        crcFileName[50];
    string      lwrProtocol = "";
    UINT32      displayClass = m_pDisplay->GetClass();
    string      dispClassName = "";
    string      goldenCrcDir = "dispinfradata/testgoldens/VbiosRmTransition";

    CHECK_RC(DTIUtils::Mislwtils:: ColwertDisplayClassToString(displayClass,
        dispClassName));
    fileDirPath = goldenCrcDir + dispClassName + "/";
    sprintf(crcFileName, "VbiosRmTransitionTest_%s_%dx%d.xml",
        pDisplayPanel->orProtocol.c_str(),
        pDisplayPanel->resolution.width,
        pDisplayPanel->resolution.height);

    szCrcFileName = crcFileName;

    return rc;
}

//! \brief Boot Up subtest.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::RunBootUpSubtest(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;

    Printf(Tee::PriHigh, "Running Boot Up subtest...\n");

    // Switch to VGA mode.
    CHECK_RC(SwitchToVGAMode(0x3));

    // Switch to driver mode
    CHECK_RC(SwitchToDriverMode());

    // Allocate Core/Window channels.
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    m_coreAllocated = true;
    CHECK_RC(AllocateChannels(pDisplayPanel));

    // Allocate Do Modeset on head.
    CHECK_RC(DoModeset(pDisplayPanel));

    // Check underflow.
    CHECK_RC(CheckDispUnderflow(pDisplayPanel));

    // Check CRC.
    CHECK_RC(CheckCRC(pDisplayPanel));

    return rc;
}

//! \brief Suspend/Resume subtest.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::RunSuspendResumeSubtest(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;

    Printf(Tee::PriHigh, "Running Suspend/Resume subtest...\n");

    // Allocate Core/Windows channels.
    CHECK_RC(AllocateChannels(pDisplayPanel));

    // Do Modeset on any head.
    CHECK_RC(DoModeset(pDisplayPanel));

    // Check underflow.
    CHECK_RC(CheckDispUnderflow(pDisplayPanel));

    // Check CRC.
    CHECK_RC(CheckCRC(pDisplayPanel));

    // Detach all displays (null modeset).
    CHECK_RC(pDisplayPanel->pModeset->Detach());

    // Do Suspend/Resume.
    CHECK_RC(DoSuspendResume());

    // Do Modeset on head.
    CHECK_RC(DoModeset(pDisplayPanel));

    // Check underflow.
    CHECK_RC(CheckDispUnderflow(pDisplayPanel));

    // Check CRC.
    CHECK_RC(CheckCRC(pDisplayPanel));

    return rc;
}

//! \brief Driver Unload/Load subtest.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::RunDriverUnloadLoadSubtest(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;

    Printf(Tee::PriHigh, "Running Dirver Unload/Load subtest...\n");

    // Allocate Core/Window channels.
    CHECK_RC(AllocateChannels(pDisplayPanel));

    // Do Modeset on head.
    CHECK_RC(DoModeset(pDisplayPanel));

    // Check underflow.
    CHECK_RC(CheckDispUnderflow(pDisplayPanel));

    // Check CRC.
    CHECK_RC(CheckCRC(pDisplayPanel));

    // Deallocate all channels and resources
    CHECK_RC(DeallocateChannels(pDisplayPanel));

    // Switch to VGA mode.
    CHECK_RC(SwitchToVGAMode(0x4118));

    // Switch to driver mode.
    CHECK_RC(SwitchToDriverMode());

    // Allocate Core/Window channels.
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    m_coreAllocated = true;
    CHECK_RC(AllocateChannels(pDisplayPanel));

    // Do Modeset on head.
    CHECK_RC(DoModeset(pDisplayPanel));

    // Check underflow.
    CHECK_RC(CheckDispUnderflow(pDisplayPanel));

    // Check CRC.
    CHECK_RC(CheckCRC(pDisplayPanel));

    return rc;
}

//! \brief BSOD subtest.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::RunBSODSubtest(vector<DisplayPanel *> pDisplayPanels)
{
    RC rc = OK;

    Printf(Tee::PriHigh, "Running BSOD subtest...\n");

    for (UINT32 iPanel = 0; iPanel < pDisplayPanels.size(); iPanel++)
    {
        if ((pDisplayPanels[iPanel]->head == DEFAULT_PANEL_VALUE) ||
            (pDisplayPanels[iPanel]->sor == DEFAULT_PANEL_VALUE))
            continue;

        // Allocate Window channels.
        CHECK_RC(AllocateChannels(pDisplayPanels[iPanel]));

        // Do Modeset on head.
        CHECK_RC(DoModeset(pDisplayPanels[iPanel]));

        // Check underflow.
        CHECK_RC(CheckDispUnderflow(pDisplayPanels[iPanel]));
    }

    // Delete allocated resources before switching to VGA modes.
    if (m_pDispPanelHelper != NULL)
    {
        delete m_pDispPanelHelper;
        m_pDispPanelHelper = NULL;
    }

    // Switch to VGA mode.
    CHECK_RC(SwitchToVGAMode(0x12));

    return rc;
}

//! \brief Oneshot mode subtest.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::RunOneshotSubtest(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;

    if (m_legacyVrr)
        Printf(Tee::PriHigh, "Running legacy Oneshot mode subtest...\n");
    else
        Printf(Tee::PriHigh, "Running new Oneshot mode subtest...\n");

    // Allocate Core/Window channels.
    CHECK_RC(AllocateChannels(pDisplayPanel));

    // Do Modeset on head.
    CHECK_RC(DoOneshotModeset(pDisplayPanel));

    // Check underflow.
    CHECK_RC(CheckDispUnderflow(pDisplayPanel));

    // Delete allocated resources before switching to VGA modes.
    if (m_pDispPanelHelper != NULL)
    {
        delete m_pDispPanelHelper;
        m_pDispPanelHelper = NULL;
    }

    // Switch to VGA mode.
    CHECK_RC(SwitchToVGAMode(0x12));

    return rc;
}

//! \brief Allocate Core & Window Channels.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::AllocateChannels(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;

    if (!pDisplayPanel->bActiveModeset)
    {
        CHECK_RC(pDisplayPanel->pModeset->AllocateWindowsAndSurface());
    }

    return rc;
}

//! \brief Deallocate Core & Window Channels.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::DeallocateChannels(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;
    GpuDevice *pDevice = GetBoundGpuDevice();
    UINT32 deviceInst = pDevice->GetDeviceInst();

    //
    // Deallocate all window instances
    // Free Window Channel and Clear Custom Settings
    //
    CHECK_RC(pDisplayPanel->pModeset->ClearLwstomSettings());
    CHECK_RC(pDisplayPanel->pModeset->FreeWindowsAndSurface());
    CHECK_RC(pDisplayPanel->pModeset->Detach());

    // Deallocate core channel and release GPU resources
    if (m_coreAllocated)
    {
        m_pDisplay->FreeGpuResources(deviceInst);
        m_pDisplay->FreeRmResources(deviceInst);
        m_coreAllocated = false;
    }

    return rc;
}

//! \brief Check the underflow bit.
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::CheckDispUnderflow(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;
    UINT32 data32;
    UINT32 head = pDisplayPanel->head;

    data32 = GetBoundGpuSubdevice()->RegRd32(LW_PDISP_RG_UNDERFLOW(head));
    if (DRF_VAL(_PDISP, _RG_UNDERFLOW, _UNDERFLOWED, data32))
    {
        Printf(Tee::PriHigh, "ERROR: Underflow Dected (head = %d).\n", head);
        rc = RC::SET_MODE_FAILED;
    }

    return rc;
}

//! \brief Do modeset
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::DoModeset(DisplayPanel *pDisplayPanel)
{
    RC rc;
    //LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();

    //
    // hard coding for now
    // Select Default surface of 800x600
    //
    pDisplayPanel->winParams[0].width = 800;  // pDisplayPanel->resolution.width;
    pDisplayPanel->winParams[0].height = 600; // pDisplayPanel->resolution.height;

    if (!pDisplayPanel->bActiveModeset)
    {
        CHECK_RC(pDisplayPanel->pModeset->Initialize());
        CHECK_RC(pDisplayPanel->pModeset->SetLwstomSettings());
        CHECK_RC(pDisplayPanel->pModeset->SetupChannelImage());
        //CHECK_RC(pDisplayPanel->pModeset->SetWindowImage(pDisplayPanel));
    }

    CHECK_RC(pDisplayPanel->pModeset->SetMode());

    //Tasker::Sleep(500);
    //Printf(Tee::PriHigh, "Dump state cache after modeset.\n");
    //pLwDisplay->DumpStateCache();

    return rc;
}

//! \brief Do Onsehot modeset
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::DoOneshotModeset(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();

    //
    // hard coding for now
    // Select Default surface of 800x600
    //
    pDisplayPanel->winParams[0].width = 800;  // pDisplayPanel->resolution.width;
    pDisplayPanel->winParams[0].height = 600; // pDisplayPanel->resolution.height;

    if (!pDisplayPanel->bActiveModeset)
    {
        CHECK_RC(pDisplayPanel->pModeset->SetLwstomSettings());
        CHECK_RC(pDisplayPanel->pModeset->SetupChannelImage());
        CHECK_RC(pDisplayPanel->pModeset->SetWindowImage(pDisplayPanel));
    }

    CHECK_RC(pDisplayPanel->pModeset->SetMode());

    pDisplayPanel->bEnableVRR = true;
    pDisplayPanel->pModeset->SetVRRSettings(pDisplayPanel->bEnableVRR, m_legacyVrr);

    // Enable VRR in RM for legacy VRR only
    if (m_legacyVrr)
    {
        pDisplayPanel->pModeset->EnableVRR(pDisplayPanel);
    }

    Printf(Tee::PriHigh, "VbiosRmTransitionTest::%s Dump state cache after enabling VRR.\n", __FUNCTION__);
    pLwDisplay->DumpStateCache();

    return rc;
}

//! \brief Do HDMI frame-packed Stereo modeset
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::DoStereoModeset(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;

    // TODO: add stereo modeset code here.

    return rc;
}

//! \brief Check CRC
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::CheckCRC(DisplayPanel *pDisplayPanel)
{
    RC rc = OK;
    string crcFileName;
    string crcDirPath;
    if (!m_manualVerif)
    {
        CHECK_RC(GetCrcFileName(pDisplayPanel, crcFileName, crcDirPath));
    }

    rc = pDisplayPanel->pModeset->VerifyDisp(
            m_manualVerif,
            crcFileName,
            crcDirPath,
            m_generate_gold);

    return rc;
}

//! \brief Do Suspend/Resume
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::DoSuspendResume()
{
    RC rc = OK;

    // Bind the pwrmgmt utility object to the subdevice we'll be running the test on.
    CHECK_RC(m_powerMgmtUtil.BindGpuSubdevice(GetBoundGpuSubdevice()));

    if ((Platform::Fmodel == Platform::GetSimulationMode()) ||
        (Xp::OS_LINUXSIM  == Xp::GetOperatingSystem())      ||
        (Xp::OS_LINUX     == Xp::GetOperatingSystem())      ||
        (Xp::OS_LINUXRM   == Xp::GetOperatingSystem()))
    {
        CHECK_RC(m_powerMgmtUtil.RmSetPowerState(m_powerMgmtUtil.GPU_PWR_SUSPEND));
        Tasker::Sleep(DEFAULT_GPU_SUSPEND_TIME_MS);
        CHECK_RC(m_powerMgmtUtil.RmSetPowerState(m_powerMgmtUtil.GPU_PWR_RESUME));
    }
    else
    {
        CHECK_RC(m_powerMgmtUtil.PowerStateTransition(m_powerMgmtUtil.POWER_S3,
                                                      DEFAULT_SYSTEM_SUSPEND_TIME));
        CHECK_RC(m_powerMgmtUtil.PowerStateTransition(m_powerMgmtUtil.POWER_S4,
                                                      DEFAULT_SYSTEM_HIBERNATE_TIME));
    }

    return rc;
}

//! \brief Switch to VGA Mode
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::SwitchToVGAMode(UINT32 mode)
{
#ifdef CLIENT_SIDE_RESMAN
    const UINT32 VGA_BASE_ALIGNMENT_BITS = 18;
    const UINT32 VGA_BASE_ALIGNMENT = 1 << VGA_BASE_ALIGNMENT_BITS;

    RC rc = OK;
    GpuDevice *pDevice = GetBoundGpuDevice();
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    UINT32 deviceInst = pDevice->GetDeviceInst();

    if (!m_VgaIsEnabled)
    {
        CHECK_RC(ConsoleManager::Instance()->PrepareForSuspend(pDevice));
        if (m_coreAllocated)
        {
            CHECK_RC(m_pDisplay->DetachAllDisplays());
            m_pDisplay->FreeGpuResources(deviceInst);
            m_pDisplay->FreeRmResources(deviceInst);
            m_coreAllocated = false;
        }

        // Backup the original MODS_PDISP_VGA_BASE value
        m_VgaBaseOrg = GetBoundGpuSubdevice()->Regs().Read32(MODS_PDISP_VGA_BASE);

        const bool restoreDefaultDisplay = true;
        if (!RmEnableVga(deviceInst, restoreDefaultDisplay))
        {
            Printf(Tee::PriError, "Error: Unable to switch to VGA.");
            return RC::SET_MODE_FAILED;
        }

        Surface2D vgaSurface;
        vgaSurface.SetWidth(1024);
        vgaSurface.SetHeight(768);
        vgaSurface.SetColorFormat(ColorUtils::A8R8G8B8);
        vgaSurface.SetPitch(4*1024);
        vgaSurface.SetType(LWOS32_TYPE_PRIMARY);
        vgaSurface.SetLocation(Memory::Fb);
        vgaSurface.SetProtect(Memory::ReadWrite);
        vgaSurface.SetLayout(Surface2D::Pitch);
        vgaSurface.SetDisplayable(true);
        vgaSurface.SetAlignment(VGA_BASE_ALIGNMENT);
        CHECK_RC(vgaSurface.Alloc(pDevice));
        CHECK_RC(vgaSurface.Map(m_pDisplay->GetMasterSubdevice()));
        UINT08 *vgaSurfaceBuf = (UINT08*)vgaSurface.GetAddress();

        UINT32 vgaSurfaceBaseValue = 0;
        pSubdevice->Regs().SetField(&vgaSurfaceBaseValue, MODS_PDISP_VGA_BASE_ADDR,
            static_cast<UINT32>(vgaSurface.GetVidMemOffset() >> VGA_BASE_ALIGNMENT_BITS));
        pSubdevice->Regs().SetField(&vgaSurfaceBaseValue,
            MODS_PDISP_VGA_BASE_TARGET_PHYS_LWM);
        pSubdevice->Regs().SetField(&vgaSurfaceBaseValue,
            MODS_PDISP_VGA_BASE_STATUS_VALID);
        pSubdevice->Regs().Write32(MODS_PDISP_VGA_BASE, vgaSurfaceBaseValue);

        Utility::CleanupOnReturn<void> restoreStdout(&RestoreStdout);
        if (!Tee::GetStdoutSink()->IsEnabled())
        {
            restoreStdout.Cancel();
        }
        else
        {
            Tee::GetStdoutSink()->Disable();
        }

        switch (mode)
        {
            case 0x3:
            {
                CHECK_RC(SetVgaMode(mode));
                UINT08 charValue = '0';
                UINT08 attributeValue = 0x8;
                const UINT32 NUM_COL = 80;
                const UINT32 NUM_ROW = 25;
                const UINT32 CELL_SIZE = 8;
                for (UINT32 textY = 0; textY < NUM_ROW; textY++)
                {
                    for (UINT32 textX = 0; textX < NUM_COL; textX++)
                    {
                        vgaSurfaceBuf[(NUM_COL*textY + textX)*CELL_SIZE + 0] = charValue++;
                        vgaSurfaceBuf[(NUM_COL*textY + textX)*CELL_SIZE + 1] =
                            (attributeValue++) & 0x7F; // Don't enable blinking as this generates
                                                       // even more CRC beyond blinking cursor
                    }
                    attributeValue += 11; // To test more attibute/char combinations
                }
                break;
            }
            case 0x12:
            {
                CHECK_RC(SetVgaMode(mode));
                CHECK_RC(vgaSurface.FillPattern(1, 1, "gradient", "mfgdiag", "horizontal"));
                break;
            }
            case 0x4118:
            {
                CHECK_RC(SetVgaMode(mode));
                CHECK_RC(vgaSurface.FillPattern(1, 1, "gradient", "mfgdiag", "horizontal"));
                break;
            }
            default:
            {
                break;
            }
        }
        m_VgaIsEnabled = true;
    }

    if (m_manualVerif)
    {
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        Printf(Tee::PriHigh,
            " Type YES if you see image correctly \n");
        string inputNumStr = pInterface->GetLine();
        if (inputNumStr.find("YES") != string::npos)
        {
            rc = OK;
        }
        else
        {
            rc = RC::SOFTWARE_ERROR;
        }
        delete pInterface;
    }
    return rc;
#else
    return RC::SOFTWARE_ERROR;
#endif
}

//! \brief Switch to Driver Mode
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::SwitchToDriverMode()
{
#ifdef CLIENT_SIDE_RESMAN
    GpuDevice *pDevice = GetBoundGpuDevice();
    UINT32 deviceInst = pDevice->GetDeviceInst();

    if (m_VgaIsEnabled)
    {
        RmDisableVga(deviceInst);
        m_VgaIsEnabled = false;
    }

    return OK;
#else
    return RC::SOFTWARE_ERROR;
#endif
}

//! \brief Set VGA Mode
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::SetVgaMode(UINT32 mode)
{
#ifdef CLIENT_SIDE_RESMAN
    const bool windowed = false;
    const bool clearMemory = true;
    UINT32 eax = 0x4F02;
    UINT32 ebx =
          (mode & 0x00003FFF)
        | (windowed ? 0 : 1 << 14)
        | (clearMemory ? 0 : 1 << 15);
    UINT32 ecx = 0, edx = 0;

    RmCallVideoBIOS(GetBoundGpuDevice()->GetDeviceInst(), false,
        &eax, &ebx, &ecx, &edx, NULL);

    // Command completed successfully if AX=0x004F.
    if (0x004F != (eax & 0x0000FFFF))
    {
        Printf(Tee::PriError, "Unable to set VESA mode 0x%x, ax=0x%04x\n",
            mode, eax&0xFFFF);
        return RC::SET_MODE_FAILED;
    }

    return OK;
#else
    return RC::SOFTWARE_ERROR;
#endif
}

//! \brief Cleanup
//------------------------------------------------------------------------------
RC VbiosRmTransitionTest::Cleanup()
{
    RC rc = OK;
    GpuDevice *pDevice = GetBoundGpuDevice();

#ifdef CLIENT_SIDE_RESMAN
    if (m_VgaIsEnabled)
    {
        RmDisableVga(pDevice->GetDeviceInst());
        m_VgaIsEnabled = false;
    }
#endif
    rc = m_pDisplay->AllocGpuResources();

    GetBoundGpuSubdevice()->Regs().Write32(MODS_PDISP_VGA_BASE, m_VgaBaseOrg);

    rc = ConsoleManager::Instance()->ResumeFromSuspend(pDevice);

    return rc;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(VbiosRmTransitionTest, RmTest,
    "Simple test to verify transition between VBIOS and RM.");

CLASS_PROP_READWRITE(VbiosRmTransitionTest, subtest, string,
                     "Subtest to be run (bootup/suspendresume/loadunload/bsod)");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, heads, string,
                     "head number to be used in the test");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, windows, string,
                     "-window windowNum-width-height, windowNum-width-height");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, sors, string,
                     "head number to be used in the test");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, resolution, string,
                     "desired raster size (small/large)");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, pixelclock, UINT32,
                     "desired pixel clock in Hz");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, onlyConnectedDisplays, bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, manualVerif, bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, generate_gold, bool,
                     "generates gold files, default = 0");
CLASS_PROP_READWRITE(VbiosRmTransitionTest, legacyVrr, bool,
                     "use Legacy VRR (for testing)");
