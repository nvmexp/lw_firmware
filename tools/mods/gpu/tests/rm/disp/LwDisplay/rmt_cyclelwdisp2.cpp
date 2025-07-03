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
//! \file rmt_CycleLwDisp2
//!       This test calls lwdisplay library APIs to verify window(s) on head(s)

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "gpu/display/lwdisplay/lw_disp_c6.h"
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
#include "core/include/simpleui.h"
#include "cyclelwdisp2_configlist.h"

using namespace std;
using namespace Modesetlib;

class CycleLwDisp2 : public RmTest
{
public:
    CycleLwDisp2();
    virtual ~CycleLwDisp2();

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
    RC LoopThroughConfig( vector <DisplayPanel *>pDisplayPanels);
    RC GetConfig(UINT32 lwrrConfigNum);
    RC PrintConfig(UINT32 lwrrConfigNum);

    SETGET_PROP(heads, string);
    SETGET_PROP(sors, string);
    SETGET_PROP(windows, string);
    SETGET_PROP(headwin, string);
    SETGET_PROP(config, string);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(resolution, string); //Config raster size through cmdline
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(check_hdcp, bool);
    SETGET_PROP(force_DSC_BitsPerPixelX16, UINT32);
    SETGET_PROP(linkRate, UINT32);
    SETGET_PROP(laneCount, UINT32);
    SETGET_PROP(enable_mode, string);
    SETGET_PROP(inputLutLwrve, UINT32);
    SETGET_PROP(outputLutLwrve, UINT32);
    SETGET_PROP(tmoLutLwrve, UINT32);
    SETGET_PROP(colorgamut, UINT32);
    SETGET_PROP(dpMode, string);
    SETGET_PROP(dwa, bool);
    SETGET_PROP(semiplanar, bool);
    SETGET_PROP(forceYUV, string);
    SETGET_PROP(hdmiFrl, bool);
    SETGET_PROP(hdmiFrlLinkRate, UINT32);
    SETGET_PROP(hdmiFrlLaneCount, UINT32);
    SETGET_PROP(region_crcs, string);

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
    bool m_check_hdcp;
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
    string m_dpMode;
    UINT32 m_dpTestMode;
    bool m_dwa;
    bool m_semiplanar;
    string m_forceYUV;
    UINT32 m_outputColorSpace;
    bool m_hdmiFrl;
    UINT32 m_hdmiFrlLinkRate;
    UINT32 m_hdmiFrlLaneCount;
    string m_region_crcs;
    RC DynamicWindowMover(vector <DisplayPanel *>pDisplayPanels);
};

CycleLwDisp2::CycleLwDisp2()
{
    m_pDisplay = nullptr;
    SetName("CycleLwDisp2");
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
    m_check_hdcp = false;
    m_force_DSC_BitsPerPixelX16 = 0;
    m_linkRate = 0;
    m_laneCount = laneCount_0;
    m_enable_mode = "";
    displayMode = ENABLE_BYPASS_SETTINGS;
    maxHeads = 0;
    maxWindows = 0;
    m_dpMode = "";
    m_dpTestMode = LwDispPanelMgr::LWD_PANEL_TYPE_NONE;
    m_outputLutLwrve = 0; 
    m_inputLutLwrve = 0;
    m_tmoLutLwrve = 0;
    m_colorgamut = 0;
    m_dwa = false;
    m_semiplanar = false;
    m_forceYUV = "";
    m_outputColorSpace = 0;
    m_hdmiFrl = false;
    m_hdmiFrlLinkRate = 0;
    m_hdmiFrlLaneCount = 0;
    m_region_crcs = "";
}

//! \brief LwDispBringupPass1 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
CycleLwDisp2::~CycleLwDisp2()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string CycleLwDisp2::IsTestSupported()
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
RC CycleLwDisp2::Setup()
{
    RC rc = OK;

    m_pDisplay = GetDisplay();
    m_pLwDisplay = static_cast<LwDisplay *>(GetDisplay());

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

RC CycleLwDisp2::DynamicWindowMover(vector <DisplayPanel *>pDisplayPanels)
{
    RC rc;
    UINT32 panelIndex = 0;
    CHECK_RC(ParseCmdLineArgs());
    for (UINT32 index = 0; index < (unsigned) Utility::CountBits(m_pLwDisplay->m_UsableHeadsMask); index++)
    {
        if ((m_headWindowMap.find(index) != m_headWindowMap.end()) &&
            (panelIndex < pDisplayPanels.size()))
        {
            DisplayPanel *pTestDispPanel = pDisplayPanels[panelIndex];
            pTestDispPanel->head = atoi(m_szHeads[panelIndex].c_str());
            pTestDispPanel->resolution = m_dispResolution;
            pTestDispPanel->winParams = m_headWindowMap[pTestDispPanel->head];
            CHECK_RC(pTestDispPanel->pModeset->Initialize());
            if (pTestDispPanel->pModeset->IsPanelTypeDP())
            {
                Modeset_DP* pDpModeset = (Modeset_DP*)pTestDispPanel->pModeset;
                if(m_linkRate || m_laneCount)
                {
                    DisplayPortMgr::DP_LINK_CONFIG_DATA dpLinkConfig;
                    dpLinkConfig.linkRate = m_linkRate;
                    dpLinkConfig.laneCount = m_laneCount;
                    dpLinkConfig.bFec = false;
                    CHECK_RC(pDpModeset->SetupLinkConfig(dpLinkConfig));
                }
            }
            pTestDispPanel->sor = atoi(m_szSors[panelIndex].c_str());
            CHECK_RC(TestDisplayPanel(pTestDispPanel, 0));
            panelIndex++;
        }
    }
    return rc;
}

RC CycleLwDisp2::Run()
{
    RC rc;

    // allocate core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // Enumerate Display Panels first
    vector <DisplayPanel *>pDisplayPanels;
    vector<pair<DisplayPanel *, DisplayPanel *>> dualSSTPairs;
    DisplayPanelHelper *pDispPanelHelper = nullptr;

    pDispPanelHelper = new DisplayPanelHelper(m_pDisplay);
    // HDMI FRL uses SINGLE_TMDS_A
    if (m_protocol.compare("HDMI_FRL") == 0)
    {
        m_protocol = "SINGLE_TMDS_A";
        m_hdmiFrl = true;
    }
    CHECK_RC(pDispPanelHelper->EnumerateDisplayPanels(m_protocol, pDisplayPanels, dualSSTPairs, !m_onlyConnectedDisplays));

    // index 0 - DisplayPanel
    // index 1 - DisplayPanel
    // index 2 - DualSSTPair
    // so on ..
    UINT32 nSinglePanels = (UINT32) pDisplayPanels.size();
    UINT32 nDualSSTPanels = (UINT32) dualSSTPairs.size();
    UINT32 numPanels = nSinglePanels + nDualSSTPanels;

    if (numPanels == 0)
    {
        return RC::NO_TESTS_RUN;
    }

    if (m_dwa)
    {
        CHECK_RC_CLEANUP(DynamicWindowMover(pDisplayPanels));

        if (m_manualVerif)
        {
            CHECK_RC(pDispPanelHelper->DynamicWindowMove(pDisplayPanels));
        }
        else
        {
            CHECK_RC(pDispPanelHelper->AutomatedDynamicWindowMove(pDisplayPanels));
        }
        for (UINT32 iPanel = 0; iPanel < pDisplayPanels.size(); iPanel++)
        {
            if (!pDisplayPanels[iPanel]->pModeset->m_modesetDone)
            {
                continue;
            }
            pDisplayPanels[iPanel]->pModeset->ClearLwstomSettings();
            Printf(Tee::PriHigh," %s : Doing Detach Display\n",__FUNCTION__);
            rc = pDisplayPanels[iPanel]->pModeset->Detach();
            if (rc != OK)
            {
                Printf(Tee::PriHigh," %s : Detach Display Failed rc = %d\n",__FUNCTION__, (UINT32) rc);
            }
            // Free Window Channel and clear custom Settings
            Printf(Tee::PriHigh," %s : Calling Free WindowSurface \n",__FUNCTION__);
            rc = pDisplayPanels[iPanel]->pModeset->FreeWindowsAndSurface();
            pDisplayPanels[iPanel]->pModeset->m_modesetDone = FALSE;
        }

        return rc;
    }

    // Running through Display Configs from the Table
    if(m_config!= "")
    {
        CHECK_RC(LoopThroughConfig(pDisplayPanels));   
    }    

    else if (m_manualVerif)
    {
        CHECK_RC_CLEANUP(ParseCmdLineArgs());
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
            if (panelIndex > (nSinglePanels - 1))
            {
                pTestDispPanel = dualSSTPairs[panelIndex - nSinglePanels].first;
            }
            else
            {
                pTestDispPanel = pDisplayPanels[panelIndex];
            }

            if (m_outputLutLwrve != 0)
            {
                pTestDispPanel->outputLutLwrve = 
                    (OetfLutGenerator::OetfLwrves)m_outputLutLwrve;
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
                if (iPanel > (nSinglePanels - 1))
                {
                    // Select Primary Display panel
                    pTestDispPanel = dualSSTPairs[iPanel - nSinglePanels].first;
                }
                else
                {
                    pTestDispPanel =  pDisplayPanels[iPanel];
                }

                if (pTestDispPanel->pModeset->IsPanelTypeDP())
                {
                    Modeset_DP* pDpModeset = (Modeset_DP*)pTestDispPanel->pModeset;
                    if(m_dpTestMode && m_dpTestMode != pDpModeset->GetPanelType())
                    {
                        continue;
                    }

                    if(m_linkRate || m_laneCount)
                    {
                        DisplayPortMgr::DP_LINK_CONFIG_DATA dpLinkConfig;
                        dpLinkConfig.linkRate = m_linkRate;
                        dpLinkConfig.laneCount = m_laneCount;
                        dpLinkConfig.bFec = false;
                        CHECK_RC(pDpModeset->SetupLinkConfig(dpLinkConfig));
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

RC CycleLwDisp2::ParseCmdLineArgs()
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

    if (m_dwa && (windowStrs.size() < 3))
    {
        Printf(Tee::PriHigh, "Invalid user input. Need to have minimum 3 windows\n");
        return RC::SOFTWARE_ERROR;   
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

    if(m_dpMode != "")
    {
        if(m_dpMode == "SST")
        {
                m_dpTestMode = LwDispPanelMgr::LWD_PANEL_TYPE_DP;
        }
        else if(m_dpMode == "MST")
        {
            m_dpTestMode = LwDispPanelMgr::LWD_PANEL_TYPE_DP_MST;
        }        
        else  if(m_dpMode == "DualSST")
        {
            m_dpTestMode = LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_SST;
        }

        else if(m_dpMode == "DualMST")
        {   
            m_dpTestMode = LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_MST;
        }
        else
        {
            Printf(Tee::PriHigh, "Invalid DP test Mode. Should be SST/MST/DualSST/DualMST \n");
        }
    }
    if(m_forceYUV != "")
    {
        if(m_forceYUV == "444")
        {
            m_outputColorSpace = Display::COLORSPACE_YCBCR444;
        }
        else if(m_forceYUV == "422")
        {
            m_outputColorSpace = Display::COLORSPACE_YCBCR422;
        }
        else if(m_forceYUV == "420")
        {
            m_outputColorSpace = Display::COLORSPACE_YCBCR420;
        }
        else
        {
            Printf(Tee::PriHigh," %s : Invalid -forceYUV argument. Should be 444/422/420 \n",__FUNCTION__);
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    return rc;
}

RC CycleLwDisp2::TestDisplayPanel(DisplayPanel *pDisplayPanel, UINT32 configNumber)
{
    StickyRC rc;
    string crcFileName;
    string crcDirPath;

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

    if (pDisplayPanel->pModeset->IsPanelTypeDP())
    {
        Modeset_DP* pDpModeset = (Modeset_DP*)(pDisplayPanel->pModeset);

        // Force custom link config if requested by test params
        CHECK_RC(pDpModeset->DoDpLinkTraining());
    }

    if (m_outputColorSpace != 0)
    {
        Printf(Tee::PriHigh," %s : Forcing YUV Output\n",__FUNCTION__);
        pDisplayPanel->pModeset->ForceYUV(static_cast<Display::ColorSpace>(m_outputColorSpace));
    }

    if (pDisplayPanel->pModeset->IsPanelTypeHDMIFRL() && m_hdmiFrl)
    {
        Modeset_HDMI_FRL* pHdmiFrlModeset = (Modeset_HDMI_FRL*)(pDisplayPanel->pModeset);
        Modeset_HDMI_FRL::HDMI_FRL_LINK_CONFIG_DATA frlLinkConfig = {0};

        frlLinkConfig.linkRate = m_hdmiFrlLinkRate;
        frlLinkConfig.laneCount = m_hdmiFrlLaneCount;

        CHECK_RC(pHdmiFrlModeset->SetupLinkConfig(frlLinkConfig));
        CHECK_RC(pHdmiFrlModeset->DoFrlLinkTraining());
    }

    pDisplayPanel->pModeset->ForceDSCBitsPerPixel(m_force_DSC_BitsPerPixelX16);
    Printf(Tee::PriHigh," %s : Doing SetMode\n",__FUNCTION__);
    CHECK_RC(pDisplayPanel->pModeset->SetMode());
    pDisplayPanel->pModeset->m_modesetDone = TRUE;

    if (m_dwa)
    {
        return rc;
    }

    if (!m_manualVerif)
    {
        CHECK_RC(GetCrcFileName(pDisplayPanel, crcFileName, crcDirPath, configNumber));
    }

    rc = pDisplayPanel->pModeset->VerifyDisp(
                m_manualVerif,
                crcFileName,
                crcDirPath,
                m_generate_gold);

    if (rc == RC::OK)
    {
        if(m_config != "")
        {
            printf(" Configuration %d pass the test", configNumber);
        }
        else
        {
            printf(" Configuration set by the user pass the test" );
        }
    }

    if (rc == RC::DATA_MISMATCH)
    {
        if (GetBoundGpuSubdevice()->IsDFPGA() &&
            pDisplayPanel->pModeset->GetPanelType() == LwDispPanelMgr::LWD_PANEL_TYPE_DP)
       {
           //
           // There is CRC mismatch observed with DP panel CRC and Fmodel/Emulation CRC.
           // TODO : Fix Bug 200254662.
           Printf(Tee::PriHigh,
               "\n DP CRC got mismatched see Bug 200254662 \n");
           rc.Clear();
       }

       if (GetBoundGpuSubdevice()->IsDFPGA() &&
           pDisplayPanel->pModeset->IsPanelTypeHDMIFRL() && m_hdmiFrl)
       {
           //
           // There is CRC mismatch observed with Mstar FRL Modeset on DFPGA
           // TODO : Fix Bug 200670036.
            Printf(Tee::PriHigh,
               "\n FRL CRC got mismatched See Bug 200670036 \n");
           rc.Clear();
       }
    }

    CHECK_RC_CLEANUP(rc);

    if (m_region_crcs != "")
    {
        Printf(Tee::PriDebug," %s : Parsing regional CRC parameters \n",__FUNCTION__);
        vector<string>region_crc_strs;
        vector<string>region_crc_tokens;
        vector<LwDisplay::RegionalCrc>regionalCrcs;

        // "regionNum-width-height-xpos-ypos-goldenCrc"
        if (m_region_crcs != "")
        {
            region_crc_strs = Tokenize(m_region_crcs, ",");
        }

        for (UINT32 i=0; i < region_crc_strs.size(); i++)
        {
            region_crc_tokens = Tokenize(region_crc_strs[i], "-");
            LwDisplay::RegionalCrc regionalCrc;
            if (region_crc_tokens.size() == 5)
            {
                regionalCrc.regionNum = atoi(region_crc_tokens[0].c_str());
                regionalCrc.width = atoi(region_crc_tokens[1].c_str());
                regionalCrc.height = atoi(region_crc_tokens[2].c_str());
                regionalCrc.xPos = atoi(region_crc_tokens[3].c_str());
                regionalCrc.yPos = atoi(region_crc_tokens[4].c_str());

                // TBD set golden region CRC
                // regionalCrc.goldenCrc = atoi(region_crc_tokens[5].c_str());
                regionalCrcs.push_back(regionalCrc);
            }
        }

        LwDisplayC6 *pLwDisplayC6 = dynamic_cast<LwDisplayC6 *>(m_pLwDisplay);
        if (pLwDisplayC6 != NULL)
        {
           
            rc = pLwDisplayC6->GetRegionalCrcs(pDisplayPanel->displayId, pDisplayPanel->head, &regionalCrcs); 
            if (rc != OK)
            {
                Printf(Tee::PriError, " %s : Region CRC capture failed \n", __FUNCTION__);
            }
        }
        else
        {
            Printf(Tee::PriError, " %s : Region CRC capture failed \n", __FUNCTION__);
            return RC::UNSUPPORTED_FUNCTION;
        }

        Printf(Tee::PriHigh," %s : Captured Region CRCs\n",__FUNCTION__);
        for (UINT32 i = 0; i < regionalCrcs.size(); i++)
        {
            Printf(Tee::PriHigh,"Captured Region Number %d, CRC : 0x%x \n", regionalCrcs[i].regionNum, 
                    regionalCrcs[i].capturedCrc);

            Printf(Tee::PriHigh,"Setting same CRC as Golden Region CRC \n"); 

            // TBD set golden region CRC
            //rc = pLwDisplayC6->SetGoldenRegionalCrc(pDisplayPanel->head, regionalCrcs[i].regionNum, regionalCrcs[i].capturedCrc);
        }
    }

    if (m_check_hdcp)
    {
        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->VerifyHDCP());
    }

Cleanup:

    if (m_manualVerif)
    {
        Printf(Tee::PriHigh,
            "\n WOULD you like to DETACH type YES\n");
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        string inputNumStr = pInterface->GetLine();
        if (inputNumStr.find("YES") != string::npos)
        {
            // Clear Custom Settings
            pDisplayPanel->pModeset->ClearLwstomSettings();

            rc = pDisplayPanel->pModeset->Detach();

            // Free Window Channel
            rc = pDisplayPanel->pModeset->FreeWindowsAndSurface();

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

RC CycleLwDisp2::GetCrcFileName(DisplayPanel *pDisplayPanel,
                                  string& szCrcFileName,
                                  string& fileDirPath,
                                  UINT32 configNumber)
{
    RC rc;
    char        crcFileName[50];
    string      lwrProtocol = "";
    UINT32      displayClass = m_pDisplay->GetClass();
    string      dispClassName = "";
    string      goldenCrcDir = "dispinfradata/testgoldens/CycleLwDisp/";

    CHECK_RC(DTIUtils::Mislwtils:: ColwertDisplayClassToString(displayClass,
        dispClassName));
    fileDirPath = goldenCrcDir + dispClassName + "/";

    string orProtocol = "";
    if(m_config != "")
    {
         string fileName = "CycleLwDisp_Config%d.xml";
         sprintf(crcFileName, fileName.c_str(),configNumber);
         printf (" File Name: %s\n", crcFileName);
    }
    else
    {
        if (pDisplayPanel->pModeset->IsPanelTypeDP())
        {
            UINT32 linkRate, laneCount = 0;
            string fileName = "CycleLwDisp_%s_%dx%d_%d_%d_%d_%d.xml";
            CHECK_RC(m_pDisplay->GetDisplayPortMgr()->GetLwrrentLinkConfig(pDisplayPanel->displayId.Get(), &laneCount, &linkRate));
            LwDispPanelMgr::LwDispPanelType dpPanelType = pDisplayPanel->pModeset->GetPanelType();

            switch(dpPanelType)
            {
                case LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_SST:
                    orProtocol = "DP_DUAL_SST";
                    break;
                case LwDispPanelMgr::LWD_PANEL_TYPE_DP_DUAL_MST:
                    orProtocol = "DP_DUAL_MST";
                    break;
                case LwDispPanelMgr::LWD_PANEL_TYPE_DP_MST:
                    orProtocol = "DP_MST";
                    break;
                default:
                    orProtocol = "DP_SST";
            }

            if (displayMode == ENABLE_HDR_SETTINGS)
            {
                fileName = "CycleLwDisp_%s_%dx%d_%d_%d_%d_%d_hdr.xml";
            }
            else if (displayMode == ENABLE_SDR_SETTINGS)
            {
                fileName = "CycleLwDisp_%s_%dx%d_%d_%d_%d_%d_sdr.xml";
            }

            sprintf(crcFileName, fileName.c_str(),
                orProtocol.c_str(),
                pDisplayPanel->resolution.width,
                pDisplayPanel->resolution.height,
                pDisplayPanel->resolution.refreshrate,
                pDisplayPanel->resolution.depth,
                linkRate,
                laneCount);

        }
        else
        {
            string fileName = "CycleLwDisp_%s_%dx%d.xml";
            orProtocol = pDisplayPanel->orProtocol.c_str();
            if (displayMode == ENABLE_HDR_SETTINGS)
            {
                fileName = "CycleLwDisp_%s_%dx%d_hdr.xml";
            }
            else if (displayMode == ENABLE_SDR_SETTINGS)
            {
                fileName = "CycleLwDisp_%s_%dx%d_sdr.xml";
            }

            sprintf(crcFileName, fileName.c_str(),
                orProtocol.c_str(),
                pDisplayPanel->resolution.width,
                pDisplayPanel->resolution.height);
        }
    }

    szCrcFileName = crcFileName;

    return rc;
}

RC CycleLwDisp2::InitializeTestDispPanel(DisplayPanel *pTestDispPanel)
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

RC CycleLwDisp2::LoopThroughConfig( vector <DisplayPanel *>pDisplayPanels)
{
        RC rc;
		size_t numOfLoop;
        // Testing First Display
        DisplayPanel *pTestDispPanel = NULL;
        pTestDispPanel =  pDisplayPanels[0];

        // m_config have all the index number's of the configurations stored in configList Table that needs to be tested
        if (m_config == "All")
        {
            numOfLoop = DISP_CONFIG_ROW_MAX;
        }
        else
        {
            m_szConfig = Tokenize(m_config, ",");
            numOfLoop =  m_szConfig.size();
        }
        for(UINT32 iConfig=0; iConfig < numOfLoop; iConfig++)
        {
            // lwrrConfigNum have the index at which current Config is stored in configList Table
            UINT32 lwrrConfigNum;

            if(m_config == "All")
            {
                lwrrConfigNum = iConfig;
            }
            else
            {
                lwrrConfigNum=atoi(m_szConfig[iConfig].c_str());
            }

            // Getting configuration from configList table
            CHECK_RC(GetConfig(lwrrConfigNum));

            CHECK_RC(ParseCmdLineArgs());

            // Printing configuration
            CHECK_RC(PrintConfig(lwrrConfigNum));

			//Initialize Test Display Panels
            CHECK_RC(InitializeTestDispPanel(pTestDispPanel));

            // Test Display panel
            CHECK_RC(TestDisplayPanel(pTestDispPanel, lwrrConfigNum));

            // Clearing the headWindowMap for next iteration
            m_headWindowMap.clear();
        }
        return rc;
}

RC CycleLwDisp2::GetConfig(UINT32 lwrrConfigNum)
{
    RC rc;
    m_heads=configList[lwrrConfigNum][0];
    m_sors=configList[lwrrConfigNum][1];
    m_windows=configList[lwrrConfigNum][2];
    m_headwin=configList[lwrrConfigNum][3];
    m_resolution=configList[lwrrConfigNum][4];  
    m_pixelclock=atoi(configList[lwrrConfigNum][5].c_str());
    return rc;
}

RC CycleLwDisp2::PrintConfig(UINT32 lwrrConfigNum)
{
    RC rc;
    printf("Configuration Number: %d\n", lwrrConfigNum + 1 ) ;
    printf("Head: %s\n", m_heads.c_str());
    printf("Sor: %s\n", m_sors.c_str());
    printf("Window Number: %s\n", m_windows.c_str());
    printf("HeadWin: %s\n", m_headwin.c_str());
    printf("Display Resolution Width: %d\n", m_dispResolution.width);
    printf("Display Resolution Height: %d\n", m_dispResolution.height);
    printf("Display Resolution Depth: %d\n", m_dispResolution.depth);
    printf("Display Resolution Refreshrate: %d\n", m_dispResolution.refreshrate);
    printf("Display Resolution: %u\n", m_dispResolution.pixelClockHz);
    return rc;
}

vector<string> CycleLwDisp2::Tokenize(const string &str,
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
RC CycleLwDisp2::Cleanup()
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
JS_CLASS_INHERIT(CycleLwDisp2, RmTest,
    "Simple test to demonstrate usage of SetMode() API");

CLASS_PROP_READWRITE(CycleLwDisp2, heads, string,
                     "head number to be used in the test");
CLASS_PROP_READWRITE(CycleLwDisp2, windows, string,
                     "-windows windowNum-width-height, windowNum-width-height");
CLASS_PROP_READWRITE(CycleLwDisp2, headwin, string,
                     "-headwin headnum-windowNum-width-height, headnum-windowNum-width-height");
CLASS_PROP_READWRITE(CycleLwDisp2, region_crcs, string,
                     "-region_crcs regionNum-width-height-xPos-yPos regionNum-width-height-xPos-yPos");
CLASS_PROP_READWRITE(CycleLwDisp2, sors, string,
                     "sor number to be used in the test");
CLASS_PROP_READWRITE(CycleLwDisp2, config, string,
                     "config number to be used in the test");
CLASS_PROP_READWRITE(CycleLwDisp2, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(CycleLwDisp2, resolution, string,
                     "Desired raster size ex : 800x600x32x60");
CLASS_PROP_READWRITE(CycleLwDisp2, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(CycleLwDisp2,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(CycleLwDisp2,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(CycleLwDisp2, generate_gold, bool,
                     "generates gold files, default = 0");
CLASS_PROP_READWRITE(CycleLwDisp2, check_hdcp, bool,
                     "Validates Hdcp, default = 0");
CLASS_PROP_READWRITE(CycleLwDisp2, force_DSC_BitsPerPixelX16, UINT32,
                      "Force DSC with specified bits per pixel. This is actual bits per pixel multiplied by 16, default = 0");
CLASS_PROP_READWRITE(CycleLwDisp2, linkRate, UINT32,
                     "Desired DP lane BW ");
CLASS_PROP_READWRITE(CycleLwDisp2, laneCount, UINT32,
                     "Desired DP lane count");                     
CLASS_PROP_READWRITE(CycleLwDisp2, enable_mode, string,
                      "enables SDR/HDR in Pre&Post Comp, default = bypass");
CLASS_PROP_READWRITE(CycleLwDisp2, inputLutLwrve, UINT32,
                     "Eotf Lwrve required for the image");
CLASS_PROP_READWRITE(CycleLwDisp2, outputLutLwrve, UINT32,
                     "Oetf Lwrve required for the monitor");
CLASS_PROP_READWRITE(CycleLwDisp2, tmoLutLwrve, UINT32,
                     "Eetf Lwrve required TMO Block");
CLASS_PROP_READWRITE(CycleLwDisp2, colorgamut, UINT32,
                     "REC709, DCI-P3, BT2020, ADOBE");
CLASS_PROP_READWRITE(CycleLwDisp2, dpMode, string,
                     "Force desired DP Test mode (SST/MST/DualSST/DualMST)");
CLASS_PROP_READWRITE(CycleLwDisp2, dwa, bool,
                     "Change the window owner dynamically");
CLASS_PROP_READWRITE(CycleLwDisp2, semiplanar, bool,
                     "Y- Plane and UV- Planes are saperated out for the image");
CLASS_PROP_READWRITE(CycleLwDisp2, forceYUV, string,
                     "force YUV output");
CLASS_PROP_READWRITE(CycleLwDisp2, hdmiFrl, bool,
                     "Force HDMI FRL mode, default = 0");
CLASS_PROP_READWRITE(CycleLwDisp2, hdmiFrlLinkRate, UINT32,
                     "Desired HDMI FRL link rate");
CLASS_PROP_READWRITE(CycleLwDisp2, hdmiFrlLaneCount, UINT32,
                     "Desired HDMI FRL lane count");
