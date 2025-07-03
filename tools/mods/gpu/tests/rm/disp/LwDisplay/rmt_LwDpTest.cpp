/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_LwDpTest.cpp
//!       This test calls lwdisplay library APIs to verify DP functionality

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/display/modeset_utils.h"
#include "modesetlib.h"
#include "LwDpUtils.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/simpleui.h"

using namespace std;
using namespace Modesetlib;

UINT32 getPreDefinedMode(LINK_RATE linkRate, DP_LANE_COUNT laneCount);

struct TestPanels
{
    vector <pair<DisplayPanel *, DisplayPanel *>> sHMPairs;
    vector <DisplayPanel *> pDisplayPanels;
};

typedef std::map<UINT32, UINT32> mstSorMap;

class LwDpTest : public RmTest
{
public:
    LwDpTest();
    virtual ~LwDpTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    /* Test the given panel pair for link configurations specified*/
    RC TestDisplays
    (
        TestPanels *testPanels,
        vector<LINK_RATE> linkRatesToTest,
        vector<DP_LANE_COUNT> laneCountToTest,
        UINT32 testDispCnt
    );

    /* Get the resolution that best fits the given link configuration */
    RC getResForLinkConfig
    (
        DisplayPanel* panel,
        DisplayResolution *res,
        LINK_RATE linkRatesToTest,
        DP_LANE_COUNT laneCountToTest,
        LwDispCoreChnContext* m_pCoreChCtx
    );

    /* Populate the link configuration based on parameters */
    RC populateLinkConfig
    (
        vector <LINK_RATE> *linkRatesToTest,
        vector <DP_LANE_COUNT> *laneCountToTest
    );

    RC getSorForConnector
    (
        UINT32 connectorID,
        UINT32* SorNum
    );

    bool isConnectorInMSTMap
    (
        mstSorMap &mstSorInfo,
        UINT32 connectorID,
        UINT32* SorNum
    );

    UINT32 getHeadForDisplay
    (
        DisplayID &display
    );

    bool isSHMMode();

    RC setTestMode
    (
        bool &bIsMST
    );

    RC setMSTFakeDisplayEdid
    (
        UINT08** testEdid,
        bool &bIsMST
    );

    void freeMSTFakeDisplayEdid
    (
        UINT08** testEdid
    );

    RC populateDisplayPanelSettings
    (
        TestPanels *testPanels,
        UINT32 *testDispCnt
    );

    void doLinkTraining
    (
        DisplayIDs displayIdVec,
        UINT32 linkRate,
        UINT32 laneCount
    );

    void PopulateLaneCount
    (
        UINT32 laneCount,
        UINT32 minLaneCount,
        vector <DP_LANE_COUNT> *laneCountsToTest
    );

    void PopulateLinkRate
    (
        UINT32 linkRate,
        UINT32 minLinkRate,
        vector <LINK_RATE> *linkRatesToTest
    );

    SETGET_PROP(testName, string);
    SETGET_PROP(dpMode, string);
    SETGET_PROP(minLinkRate, UINT32);
    SETGET_PROP(minLaneCount, UINT32);
    SETGET_PROP(linkRate, UINT32);
    SETGET_PROP(laneCount, UINT32);
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(numOfSinks, UINT32);
    SETGET_PROP(protocol, string);
    SETGET_PROP(force_DSC_BitsPerPixelX16, UINT32);

private:
    Display   *m_pDisplay;
    LwDisplay *m_pLwDisplay;
    bool   m_testFlushMode;
    bool   m_onlyConnectedDisplays;
    UINT32 m_linkRate = 0;
    UINT32 m_laneCount = 0;
    UINT32 m_minLinkRate = 0;
    UINT32 m_minLaneCount = 0;
    bool m_manualVerif;
    bool m_generate_gold;
    LwDpTestModes m_mode;
    string m_testName;
    string m_dpMode;
    UINT32 m_numOfSinks;
    string m_protocol;
    UINT32 m_force_DSC_BitsPerPixelX16;
};

LwDpTest::LwDpTest()
{
    SetName("LwDpTest");

    m_generate_gold = false;
    m_manualVerif = false;
    m_onlyConnectedDisplays = false;
    m_testName = "";
    m_dpMode = "";
    m_mode = DP_MODE_NONE;
    m_linkRate = 0;
    m_laneCount = 0;
    m_minLinkRate = LINK_RATE::RBR;
    m_minLaneCount = laneCount_1;
    m_numOfSinks = 4;
    m_protocol = "DP_A,DP_B";
    m_force_DSC_BitsPerPixelX16 = 0;
}

//! \brief LwDpTest destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDpTest::~LwDpTest()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDpTest::IsTestSupported()
{
    m_pDisplay = GetDisplay();
    if (GetDisplay()->GetDisplayClassFamily() != Display::LWDISPLAY)
    {
            return "The test is supported only on LwDisplay class!";
    }

    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state setup based on the JS setting
//------------------------------------------------------------------------------
RC LwDpTest::Setup()
{
    RC rc = OK;

    m_pDisplay = GetDisplay();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    return rc;
}

RC LwDpTest::Run()
{
    RC rc;
    UINT32 testDispCnt = 1;
    vector <LINK_RATE> linkRatesToTest;
    vector <DP_LANE_COUNT> laneCountToTest;
    vector <DisplayPanel *> pDisplayPanels;
    vector <pair<DisplayPanel *, DisplayPanel *>> sHMPairs;
    DisplayPanelHelper dispPanelHelper(m_pDisplay);
    bool bIsMST = false;
    UINT08* testEdid = NULL;

    if (m_numOfSinks > LwDisplay::GetMaxHeadNumber())
    {
        Printf(Tee::PriHigh, "%s: Overriding numOfSinks to 4\n", __FUNCTION__);
        m_numOfSinks = 4;
    }

    //
    // Function sets the test mode: SST/MST/DualSST/DualMST
    // according to the input params passed to the test.
    // If no params are passed, SST mode is assumed.
    //
    CHECK_RC(setTestMode(bIsMST));

    // In case a custom MST Edid has to be set for the test
    CHECK_RC(setMSTFakeDisplayEdid(&testEdid, bIsMST));

    // allocate core channel
    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    // Enumerate Display Panels first
    CHECK_RC(dispPanelHelper.EnumerateDisplayPanels(m_protocol,
        pDisplayPanels, sHMPairs, !m_onlyConnectedDisplays, m_numOfSinks, bIsMST, testEdid, FAKE_EDID_SIZE));

    if(bIsMST)
    {
        freeMSTFakeDisplayEdid(&testEdid);
    }

    if ((isSHMMode() && (sHMPairs.size() < testDispCnt)) ||
        (pDisplayPanels.size() < testDispCnt))
    {
        Printf(Tee::PriHigh, "There is no DP connectors available! Check VBIOS...\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 dispPairs = isSHMMode() ? (UINT32) sHMPairs.size() :
                        (UINT32) pDisplayPanels.size();
    testDispCnt = min(m_numOfSinks, dispPairs);

    TestPanels testPanels;
    testPanels.sHMPairs = sHMPairs;
    testPanels.pDisplayPanels = pDisplayPanels;

    // Populates panels settings(head/SOR/fakeEdid etc.) according to the DP mode
    CHECK_RC(populateDisplayPanelSettings(&testPanels, &testDispCnt));

    // Populate the link config based on the test selected
    CHECK_RC(populateLinkConfig(&linkRatesToTest, &laneCountToTest));

    // Run the test
    CHECK_RC(TestDisplays(&testPanels, linkRatesToTest, laneCountToTest, testDispCnt));

    return rc;
}

RC LwDpTest::TestDisplays
(
     TestPanels *testPanels,
     vector<LINK_RATE> linkRatesToTest,
     vector<DP_LANE_COUNT> laneCountToTest,
     UINT32 testDispCnt
)
{
    RC rc;
    DisplayPanel* pPrimary = NULL;
    pair<DisplayPanel *, DisplayPanel *> newPair(NULL, NULL);
    vector<Display::IMPQueryInput> impVec;
    DisplayIDs displayIdVec;
    bool bisModePossible = false;
    bool bSkipMode = false;
    bool bHeadDetached = true;
    LwDispCoreChnContext *m_pCoreChCtx;

    CHECK_RC(m_pDisplay->GetLwDisplay()->GetLwDisplayChnContext(Display::CORE_CHANNEL_ID,
        (LwDispChnContext **)&m_pCoreChCtx,
        LwDisplay::ILWALID_WINDOW_INSTANCE,
        0));

    // Loop through Link Rate
    for (INT32 lr = static_cast<INT32>(linkRatesToTest.size()-1); lr >=0; --lr)
    {
        // Loop through Lane Count
        for (INT32 lc = static_cast<INT32>(laneCountToTest.size()-1); lc >=0; --lc)
        {
            Printf(Tee::PriHigh, "Selected BW: %u LNCT: %u\n", linkRatesToTest[lr], laneCountToTest[lc] );
            bisModePossible = false;
            bSkipMode = false;
            UINT32 resolutionLoop = 0;

            while(!bisModePossible)
            {
                displayIdVec.clear();
                impVec.clear();

                // Pick the panel and run the test
                for (UINT32 iPanel = 0; iPanel < testDispCnt; iPanel++)
                {
                    DisplayResolution resolution;
                    UINT32 secondaryDisplayID;
                    Display::SingleHeadMultiStreamMode mSM;

                    if (isSHMMode())
                    {
                        pPrimary = testPanels->sHMPairs[iPanel].first;
                        newPair  = testPanels->sHMPairs[iPanel];
                    }
                    else
                    {
                        pPrimary = testPanels->pDisplayPanels[iPanel];
                    }

                    LwDispRasterSettings rasterSettings;

                    // Select resolution for the test according to linkConfig
                    getResForLinkConfig(pPrimary, &resolution, linkRatesToTest[lr],
                        laneCountToTest[lc], m_pCoreChCtx);

                    pPrimary->resolution = resolution;
                    CHECK_RC(pPrimary->pModeset->SetLwstomSettings());

                    UINT32 maxLinkRate;
                    CHECK_RC(m_pDisplay->GetDisplayPortMgr()->GetMaxLinkRateAssessed
                    (
                        pPrimary->displayId.Get(),
                        &maxLinkRate
                    ));

                    if (static_cast<UINT32>(linkRatesToTest[lr]) > maxLinkRate)
                    {
                        Printf(Tee::PriHigh, "Skipping Link Rate: %u for panel 0x%x as "
                            "highest assessed link rate on this connector is: %u\n", linkRatesToTest[lr],
                            pPrimary->displayId.Get(), maxLinkRate);
                        continue;
                    }
                    else
                    {
                        //Populate the disp IDs for link training
                        displayIdVec.push_back(pPrimary->displayId);
                    }

                    // Check for multistream mode
                    m_pDisplay->GetLwDisplay()->GetDisplayPortMgr()->GetSingleHeadMultiStreamMode
                    (
                        pPrimary->displayId.Get(),
                        &secondaryDisplayID, &mSM
                    );

                    //Get Raster timings
                    m_pDisplay->GetLwDisplay()->SelectRasterSettings
                    (
                        pPrimary->displayId,
                        pPrimary->head,
                        resolution.width,
                        resolution.height,
                        resolution.refreshrate,
                        &rasterSettings
                    );

                    Display::IMPQueryInput impInput = {pPrimary->displayId.Get(),
                                                       secondaryDisplayID,
                                                       mSM,
                                                       rasterSettings,
                                                       Display::FORCED_RASTER_ENABLED,
                                                       resolution.depth};
                    impVec.push_back(impInput);
                }

                if(impVec.size() == 0)
                {
                    Printf(Tee::PriHigh, "No Panels are capable of supporting this Link Config\n");
                    bSkipMode = true;
                    break;
                }

                // Perform link training
                doLinkTraining(displayIdVec, linkRatesToTest[lr], laneCountToTest[lc]);

                // Check with the selected resolution if mode is possible
                CHECK_RC(m_pDisplay->GetDisplayPortMgr()->IsModePossible(&impVec, &bisModePossible));
                if(!bisModePossible)
                {
                    if(m_manualVerif)
                    {
                        if(resolutionLoop == 3)
                        {
                            // If the IMP loop fails 3 times in succession, break and move to
                            // next configuration
                            bSkipMode = true;
                            Printf(Tee::PriHigh, "Moving to higher bandwidth if any\n");
                            break;
                        }
                        resolutionLoop++;
                        Printf(Tee::PriHigh, "Mode is not possible. Try a different set of resolutions\n");
                    }
                    else
                    {
                        Printf(Tee::PriHigh, "%s: Mode Not Possible\n", __FUNCTION__);
                        bSkipMode = true;
                        break;
                    }
                }
            }

            if(!bSkipMode)
            {
                // Pick the panel and run the test
                for (UINT32 iPanel = 0; iPanel < testDispCnt; iPanel++)
                {
                    WindowParam windowParam;
                    WindowParams windowParams;

                    if (isSHMMode())
                    {
                        pPrimary = testPanels->sHMPairs[iPanel].first;
                        newPair  = testPanels->sHMPairs[iPanel];
                    }
                    else
                    {
                        pPrimary = testPanels->pDisplayPanels[iPanel];
                    }

                    if (m_testFlushMode &&
                        (m_mode == DP_MODE_MST) &&
                        (bHeadDetached == false))
                    {
                        CHECK_RC(pPrimary->pModeset->Detach());
                        bHeadDetached = true;
                    }

                    UINT32 winNum = pPrimary->head;

                    // Setup Window and Custom settings
                    windowParam.windowNum = winNum;
                    windowParam.headNum   = pPrimary->head;
                    windowParams.push_back(windowParam);
                    pPrimary->winParams = windowParams;
                    pPrimary->winParams[0].width = pPrimary->resolution.width;
                    pPrimary->winParams[0].height = pPrimary->resolution.height;

                    CHECK_RC(pPrimary->pModeset->AllocateWindowsAndSurface());
                    CHECK_RC(pPrimary->pModeset->SetupChannelImage());
                    CHECK_RC(pPrimary->pModeset->SetWindowImage(pPrimary));

                    // Force DSC if requested by parameter
                    if (m_force_DSC_BitsPerPixelX16)
                    {

                        CHECK_RC(pPrimary->pModeset->ForceDSCBitsPerPixel(m_force_DSC_BitsPerPixelX16));
                    }

                    Printf(Tee::PriHigh, "Selected [%ux%u] BW: %u LCNT %u.\n",
                                        pPrimary->resolution.width,
                                        pPrimary->resolution.height,
                                        linkRatesToTest[lr],
                                        laneCountToTest[lc]);

                    // Perform modeset
                    CHECK_RC(pPrimary->pModeset->SetMode());

                    // Verify CRC
                    char crcFileName[80];
                    string crcDirPath;
                    if (!m_manualVerif)
                    {
                        crcDirPath = "dispinfradata/LwDispTest/CRC/";
                        string prefix = "LwDispTest_CRC";
                        sprintf(crcFileName, "%s_%s_%d_%d_%dx%d_%d_%d.xml",
                            prefix.c_str(),
                            m_dpMode.c_str(),
                            linkRatesToTest[lr]/1000000,
                            laneCountToTest[lc],
                            pPrimary->resolution.width,
                            pPrimary->resolution.height,
                            pPrimary->resolution.depth,
                            pPrimary->resolution.refreshrate
                            );
                    }

                    CHECK_RC(pPrimary->pModeset->VerifyDisp(m_manualVerif,
                                                            crcFileName,
                                                            crcDirPath,
                                                            m_generate_gold));
                }

                for (UINT32 iPanel = 0; iPanel < testDispCnt; iPanel++)
                {
                    if (isSHMMode())
                    {
                        pPrimary = testPanels->sHMPairs[iPanel].first;
                        newPair  = testPanels->sHMPairs[iPanel];
                    }
                    else
                    {
                        pPrimary = testPanels->pDisplayPanels[iPanel];
                    }

                    // Free Window Channel and Clear Custom Settings
                    CHECK_RC(pPrimary->pModeset->ClearLwstomSettings());
                    CHECK_RC(pPrimary->pModeset->FreeWindowsAndSurface());
                    if(!m_testFlushMode)
                    {
                        CHECK_RC(pPrimary->pModeset->Detach());
                        bHeadDetached = true;
                    }
                    else
                    {
                        bHeadDetached = false;
                    }
                }
            }
            if(displayIdVec.empty())
            {
                Printf(Tee::PriHigh, "No connectors support this LinkConfig\n");
                break;
            }
        }
    }

    if ((m_testName == "TestFlushMode") && (bHeadDetached == false))
    {
        CHECK_RC(pPrimary->pModeset->Detach());
    }

    return rc;
}

RC LwDpTest::setTestMode(bool &bIsMST)
{
    if (m_dpMode == "" || m_dpMode == "SST")
    {
        m_mode = DP_MODE_SST;
    }
    else if (m_dpMode == "MST")
    {
        m_mode = DP_MODE_MST;
        bIsMST = true;
    }
    else if(m_dpMode == "DualMST")
    {
        m_mode = DP_MODE_DUAL_MST;
        bIsMST = true;
        m_numOfSinks = 2;
    }
    else if(m_dpMode == "DualSST")
    {
        m_mode = DP_MODE_DUAL_SST;
    }

    if(m_testName.length() == 0)
    {
        m_testName = "BasicModeset";
    }

    if(m_testName == "TestFlushMode")
    {
        m_testFlushMode = true;
    }

    if(m_mode == DP_MODE_NONE)
    {
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC LwDpTest::setMSTFakeDisplayEdid(UINT08 **testEdid, bool &bIsMST)
{
    if(!bIsMST)
        return OK;

    *testEdid = (UINT08 *)malloc(FAKE_EDID_SIZE * sizeof(UINT08));

    switch(m_mode)
    {
        case DP_MODE_MST:
            memcpy(*testEdid, EdidForSSTMST, FAKE_EDID_SIZE);
            break;

        case DP_MODE_DUAL_MST:
            memcpy(*testEdid, EdidForDualSSTDualMST, FAKE_EDID_SIZE);
            break;

        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

void LwDpTest::freeMSTFakeDisplayEdid(UINT08** testEdid)
{
    if(!(*testEdid))
        return;

    free(*testEdid);
}

RC LwDpTest::getResForLinkConfig
(
    DisplayPanel* pPanel,
    DisplayResolution *resolution,
    LINK_RATE linkRate,
    DP_LANE_COUNT laneCount,
    LwDispCoreChnContext* m_pCoreChCtx
)
{
    RC rc;
    vector<Display::Mode> listedModes;
    vector<LWT_TIMING> lwtTimings;
    bool bResolutionSelected = false;
    UINT32 index;

    CHECK_RC(pPanel->pModeset->GetListedModes(&listedModes));

    for (UINT32 mCnt = 0; mCnt < (UINT32)listedModes.size(); mCnt++)
    {
        LWT_TIMING timing;
        memset(&timing, 0 , sizeof(LWT_TIMING));
        rc = ModesetUtils::CallwlateLWTTiming(m_pDisplay,
            pPanel->displayId,
            listedModes[mCnt].width,
            listedModes[mCnt].height,
            listedModes[mCnt].refreshRate,
            Display::AUTO,
            &timing,
            0);

        if (rc != OK)
        {
            Printf(Tee::PriHigh,
                "ERROR: %s: CallwlateLWTTiming() failed to get backend timings. RC = %d.\n",
                __FUNCTION__, (UINT32)rc);
            CHECK_RC(rc);
        }

        if (m_mode == DP_MODE_DUAL_SST)
        {
            // multiply Htotal and Width
            // With 2-SST timings are double.
            listedModes[mCnt].width *= 2;
            timing.pclk *= 2;
            timing.HVisible *= 2;
            timing.HBorder *= 2;
            timing.HFrontPorch *= 2;
            timing.HSyncWidth *= 2;
            timing.HTotal *= 2;
        }

        lwtTimings.push_back(timing);
        Printf(Tee::PriHigh, "Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d.\n",
            mCnt + 1,
            listedModes[mCnt].width,
            listedModes[mCnt].height,
            listedModes[mCnt].depth,
            listedModes[mCnt].refreshRate);
    }

    if(m_manualVerif)
    {
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);

        Printf(Tee::PriHigh, "\n> Input resolution index that can be accommodated in the link config\n");

        string inputNumStr = pInterface->GetLine();
        index = atoi(inputNumStr.c_str());

        if (index <= listedModes.size() && index > 0)
        {
            index --;
            resolution->width  = listedModes[index].width;
            resolution->height = listedModes[index].height;
            resolution->depth  = listedModes[index].depth;
            resolution->refreshrate = listedModes[index].refreshRate;
        }

        delete pInterface;
    }

    else
    {
        char crcFileName[80];
        string crcDirPath;
        crcDirPath = "dispinfradata/LwDispTest/CRC/";
        vector<string> searchPath;
        searchPath.push_back(crcDirPath);
        string prefix = "LwDispTest_CRC";

        for (UINT32 mCnt = 0; mCnt < (UINT32)listedModes.size(); mCnt++)
        {
            sprintf(crcFileName, "%s_%s_%d_%d_%dx%d_%d_%d.xml",
                prefix.c_str(),
                m_dpMode.c_str(),
                linkRate/1000000,
                laneCount,
                listedModes[mCnt].width,
                listedModes[mCnt].height,
                listedModes[mCnt].depth,
                listedModes[mCnt].refreshRate
                );
            if(CrcComparison::FileExists(crcFileName, &searchPath))
            {
                Printf(Tee::PriHigh, "\n%s: %s CRC file exists\n", __FUNCTION__, crcFileName);
                Printf(Tee::PriHigh, "%s: Selecting resolution: %ux%u\n",__FUNCTION__, listedModes[mCnt].width, listedModes[mCnt].height);
                resolution->width = listedModes[mCnt].width;
                resolution->height = listedModes[mCnt].height;
                resolution->depth = listedModes[mCnt].depth;
                resolution->refreshrate = listedModes[mCnt].refreshRate;
                index = mCnt;
                bResolutionSelected = true;
                break;
            }
        }

        if(!bResolutionSelected)
        {
            Printf(Tee::PriHigh, "\n**************************************************************************************\n");
            Printf(Tee::PriHigh, "No matching CRC file for verification exists for any listed modes for this link config\n");
            Printf(Tee::PriHigh, "**************************************************************************************\n");

            if(m_mode == DP_MODE_DUAL_SST || m_mode == DP_MODE_DUAL_MST)
            {
                index = 0;
            }
            else
            {
                index = getPreDefinedMode(linkRate, laneCount);
                index--;
            }

            Printf(Tee::PriHigh, "%s: Selecting predefined resolution %dx%d for the link config\n",
                __FUNCTION__, listedModes[index].width, listedModes[index].height);
            resolution->width = listedModes[index].width;
            resolution->height = listedModes[index].height;
            resolution->depth = listedModes[index].depth;
            resolution->refreshrate = listedModes[index].refreshRate;
        }
        if(m_testName == "TestFlushMode" && index != 0)
        {
            Printf(Tee::PriHigh, "%s: Overriding resolution for flush mode to 640x480\n", __FUNCTION__);
            index = 0;
            resolution->width = listedModes[index].width;
            resolution->height = listedModes[index].height;
            resolution->depth = listedModes[index].depth;
            resolution->refreshrate = listedModes[index].refreshRate;
        }
    }

    pPanel->resolution.pixelClockHz = lwtTimings[index].pclk * 10 * 1000;

    LwDispHeadSettings &headSettings = m_pCoreChCtx->DispLwstomSettings.HeadSettings[pPanel->head];
    headSettings.Reset();
    LwDispRasterSettings &rasterSettings = headSettings.rasterSettings;

    rc = ModesetUtils::ColwertLwtTiming2EvoRasterSettings
         (
            lwtTimings[index],
            &rasterSettings
         );

    if (rc != OK)
    {
        Printf(Tee::PriError, "%s failed to colwert into Raster settings index = %d",
            __FUNCTION__, (index + 1));
    }

    rasterSettings.PixelClockHz = pPanel->resolution.pixelClockHz;
    rasterSettings.Dirty = true;

    rasterSettings.Print(Tee::PriHigh);

    return rc;
}

//! This function assigns a head number to a display.
//---------------------------------------------------
UINT32 LwDpTest::getHeadForDisplay(DisplayID &display)
{
    static UINT32 headIdx = 0;
    UINT32 head = 0;
    if(m_manualVerif)
    {
        Printf(Tee::PriHigh, "Enter head for display ID 0x%x (0 to %d): \n", display.Get(), 
                           (LwDisplay::GetMaxHeadNumber() - 1));
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        string inputNumStr = pInterface->GetLine();
        head =  atoi(inputNumStr.c_str());
        delete pInterface;
    }
    else
    {
        head = headIdx++;
    }

    if (head >= LwDisplay::GetMaxHeadNumber())
    {
        Printf(Tee::PriHigh, "Invalid head number. Using default head 0\n");
        return 0;
    }
    else
        return head;
}

RC LwDpTest::populateLinkConfig
(
    vector <LINK_RATE> *linkRatesToTest,
    vector <DP_LANE_COUNT> *laneCountToTest
)
{

    if(m_testName == "BasicModeset")
    {
        if(m_linkRate == 0)
        {
            m_linkRate = LINK_RATE::RBR;
        }
        PopulateLinkRate(m_linkRate, 0, linkRatesToTest);
        if(m_laneCount == 0)
        {
            m_laneCount = laneCount_4;
        }
        PopulateLaneCount(m_laneCount, 0, laneCountToTest);
    }

    else if(m_testName == "TestLinkConfig")
    {
        PopulateLinkRate(0, m_minLinkRate, linkRatesToTest);
        PopulateLaneCount(0, m_minLaneCount, laneCountToTest);
    }

    else if(m_testName == "TestFlushMode")
    {
        if(m_minLinkRate == 0)
        {
            m_minLinkRate = HBR2;
        }
        PopulateLinkRate(m_linkRate, m_minLinkRate, linkRatesToTest);
        if(m_minLaneCount == 0)
        {
            m_minLaneCount = 4;
        }
        PopulateLaneCount(m_laneCount, m_minLaneCount, laneCountToTest);
    }

    else
    {
        Printf(Tee::PriHigh, "Test name not recognized\n");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

bool LwDpTest::isConnectorInMSTMap(mstSorMap& mstSorInfo, UINT32 connectorID, UINT32* SorNum)
{
    mstSorMap::iterator it;
    for(it = mstSorInfo.begin(); it != mstSorInfo.end(); it++)
    {
        if(it->first == connectorID)
        {
            *SorNum = it->second;
            return true;
        }
    }
    return false;
}

//! Assigns SOR for a particular DP link.
//---------------------------------------
RC LwDpTest::getSorForConnector(UINT32 connectorID, UINT32 *SorNum)
{
    RC rc;
    if(m_manualVerif)
    {
        SimpleUserInterface * pInterface = new SimpleUserInterface(true);
        Printf(Tee::PriHigh,"\n > Input SOR number for dp connector 0x%x or Type 'D' for default value \n", connectorID);
        string inputNumStr = pInterface->GetLine();
        if (!(inputNumStr.find("D") != string::npos))
        {
            *SorNum =  atoi(inputNumStr.c_str());
        }
        delete pInterface;
    }

    if (*SorNum == DEFAULT_PANEL_VALUE)
    {
        // Get From RM
        UINT32 orIndex    = 0;
        UINT32 orType     = 0;
        UINT32 orProtocol = 0;
        CHECK_RC(m_pDisplay->GetORProtocol(connectorID, &orIndex, &orType, &orProtocol));
        *SorNum = orIndex;

        if(!(orIndex >= 0 && orIndex < 4))
        {
            Printf(Tee::PriHigh, "%s: Not valid SOR for connector %u\n", __FUNCTION__, connectorID);
            return RC::SOFTWARE_ERROR;
        }

        Printf(Tee::PriHigh,
            "\n %s: Using default SOR Number: %d for dp connector 0x%x\n", __FUNCTION__, *SorNum, connectorID);
    }
    return rc;
}

bool LwDpTest::isSHMMode()
{
    return (m_mode == DP_MODE_DUAL_SST || m_mode == DP_MODE_DUAL_MST);
}

/*
The mode that this function returns are based on EdidForSSTMST.
Possible modes on this panel are given below.
For a different panel/Edid, a new function will have to be defined.

Mode[1]: Width=640; Height=480; Depth=32; RefreshRate=60.
Mode[2]: Width=800; Height=600; Depth=32; RefreshRate=60.
Mode[3]: Width=1024; Height=768; Depth=32; RefreshRate=60.
Mode[4]: Width=3840; Height=2160; Depth=32; RefreshRate=24.
Mode[5]: Width=3840; Height=2160; Depth=32; RefreshRate=30.
Mode[6]: Width=3840; Height=2160; Depth=32; RefreshRate=60.
Mode[7]: Width=3840; Height=2160; Depth=32; RefreshRate=82.
Mode[8]: Width=3840; Height=2160; Depth=32; RefreshRate=98.
Mode[9]: Width=3840; Height=2160; Depth=32; RefreshRate=120.
Mode[10]: Width=3840; Height=2160; Depth=32; RefreshRate=144.
*/

UINT32 PreDefinedModes[4][4] =
{
    // Indices here corresponds to indices of modes listed above for EdidForSSTMST panel
    {1, 1, 0, 2},
    {2, 3, 0, 4},
    {3, 4, 0, 5},
    {3, 6, 0, 7}
};

UINT32 getPreDefinedMode(LINK_RATE linkRate, DP_LANE_COUNT laneCount)
{
    UINT32 xIndex = (static_cast<UINT32>(linkRate))/HBR;
    UINT32 yIndex = (static_cast<UINT32>(laneCount)) - 1;
    return PreDefinedModes[xIndex][yIndex];
}

RC LwDpTest::populateDisplayPanelSettings(TestPanels *testPanels, UINT32 *testDispCnt)
{
    DisplayPanel* pPrimary = NULL;
    mstSorMap mstConnectors;
    DisplayIDs assignHeadForMst;
    RC rc;
    UINT32 nonMstPanels = 0;

    for (UINT32 iPanel = 0; iPanel < *testDispCnt; iPanel++)
    {
        if (m_mode == DP_MODE_DUAL_SST)
        {
            pPrimary = testPanels->sHMPairs[iPanel].first;
            CHECK_RC(getSorForConnector(testPanels->sHMPairs[iPanel].first->displayId.Get(), &(testPanels->sHMPairs[iPanel].first->sor)));
            pPrimary->head = getHeadForDisplay(pPrimary->displayId);

            if(pPrimary->isFakeDisplay)
            {
                pPrimary->fakeDpProperties.pEdid = (UINT08 *)malloc(FAKE_EDID_SIZE * sizeof(UINT08));
                memcpy(pPrimary->fakeDpProperties.pEdid, EdidForDualSSTDualMST, FAKE_EDID_SIZE);
                pPrimary->fakeDpProperties.edidSize = FAKE_EDID_SIZE;
            }
        }
        else if(m_mode == DP_MODE_SST)
        {
            pPrimary = testPanels->pDisplayPanels[iPanel];
            CHECK_RC(getSorForConnector(pPrimary->displayId.Get(), &(pPrimary->sor)));
            pPrimary->head = getHeadForDisplay(pPrimary->displayId);

            if(pPrimary->isFakeDisplay)
            {
                pPrimary->fakeDpProperties.pEdid = (UINT08 *)malloc(FAKE_EDID_SIZE * sizeof(UINT08));
                memcpy(pPrimary->fakeDpProperties.pEdid, EdidForSSTMST, FAKE_EDID_SIZE);
                pPrimary->fakeDpProperties.edidSize = FAKE_EDID_SIZE;
            }
        }

        else if(m_mode == DP_MODE_MST)
        {
            pPrimary = testPanels->pDisplayPanels[iPanel];
            if(m_pDisplay->IsMultiStreamDisplay(pPrimary->displayId.Get()))
            {
                UINT32 SorNum = DEFAULT_PANEL_VALUE;
                // check if a sor has been assigned already for the root connector
                if(isConnectorInMSTMap(mstConnectors, pPrimary->ConnectorID, &SorNum))
                {
                    pPrimary->sor = SorNum;
                }
                else
                {
                    CHECK_RC(getSorForConnector(pPrimary->ConnectorID, &SorNum));
                    mstConnectors[pPrimary->ConnectorID] = SorNum;
                    pPrimary->sor = SorNum;
                }
                assignHeadForMst.push_back(pPrimary->displayId);
            }
            else
            {
                // if the enumerated panel is not MST, remove if from the test
                Printf(Tee::PriHigh, "Skipping panel as it is not part of any MST topology\n");
                testPanels->pDisplayPanels.erase(testPanels->pDisplayPanels.begin()+iPanel);
                nonMstPanels++;
            }
        }
        else if(m_mode == DP_MODE_DUAL_MST)
        {
            pPrimary = testPanels->sHMPairs[iPanel].first;
            CHECK_RC(getSorForConnector(testPanels->sHMPairs[iPanel].first->ConnectorID, &(testPanels->sHMPairs[iPanel].first->sor)));
            pPrimary->head = getHeadForDisplay(pPrimary->displayId);
        }
        CHECK_RC(pPrimary->pModeset->Initialize());
    }

    if(m_mode == DP_MODE_MST)
    {
        if(mstConnectors.empty())
        {
            Printf(Tee::PriHigh, "No panels in MST mode for MST test\n");
            return RC::SOFTWARE_ERROR;
        }

        // Get Heads for panels in MST topology
        vector<UINT32> heads;
        CHECK_RC(m_pDisplay->GetLwDisplay()->GetHeadsForMultiDisplay(assignHeadForMst, &heads));
        for(UINT32 iPanel = 0; iPanel < assignHeadForMst.size(); iPanel++)
        {
            testPanels->pDisplayPanels[iPanel]->head = heads[iPanel];
            Printf(Tee::PriHigh, "Assigning Head for MST panel 0x%x: %d\n", testPanels->pDisplayPanels[iPanel]->displayId.Get(), heads[iPanel]);
        }
        *testDispCnt = *testDispCnt - nonMstPanels;
    }
    return OK;
}

void LwDpTest::doLinkTraining
(
    DisplayIDs displayIdVec,
    UINT32 linkRate,
    UINT32 laneCount
)
{
    for (UINT32 display = 0; display < displayIdVec.size(); display++)
    {
        //Reset old setting
        m_pDisplay->GetDisplayPortMgr()->SetLinkConfigPolicy(displayIdVec[display],
            FALSE, // commit
            TRUE,  // force
            FALSE, // m_mstMode
            DisplayPortMgr::DP_LINK_CONFIG_POLICY_DEFAULT,
            m_linkRate,
            laneCount_0);

        //Set new config
        m_pDisplay->GetDisplayPortMgr()->SetLinkConfigPolicy(displayIdVec[display],
            FALSE, // commit
            TRUE,  // force
            FALSE, // m_mstMode
            DisplayPortMgr::DP_LINK_CONFIG_POLICY_PREFERRED,
            linkRate,
            laneCount);
    }
    return;
}

void LwDpTest::PopulateLinkRate
(
    UINT32 linkRate,
    UINT32 minLinkRate,
    vector <LINK_RATE> *linkRatesToTest
)
{
    LINK_RATE linkRateToTest = (LINK_RATE)linkRate;
    LINK_RATE minLinkRateForTest = (LINK_RATE)minLinkRate;

    if (linkRate != 0)
    {
        linkRatesToTest->push_back(linkRateToTest);
    }
    else
    {
        linkRatesToTest->push_back(HBR3);
        if (minLinkRateForTest == HBR3)
            return;

        linkRatesToTest->push_back(HBR2);
        if (minLinkRateForTest == HBR2)
            return;

        linkRatesToTest->push_back(HBR);
        if (minLinkRateForTest == HBR)
            return;

        linkRatesToTest->push_back(RBR);
    }

    return;
}

void LwDpTest::PopulateLaneCount
(
    UINT32 laneCount,
    UINT32 minLaneCount,
    vector <DP_LANE_COUNT> *laneCountsToTest
)
{
    DP_LANE_COUNT laneCountToTest = (DP_LANE_COUNT)laneCount;
    DP_LANE_COUNT minLaneCountForTest = (DP_LANE_COUNT)minLaneCount;
    if (laneCount != 0)
    {
        laneCountsToTest->push_back(laneCountToTest);
    }
    else
    {
        laneCountsToTest->push_back(laneCount_4);
        if (minLaneCountForTest == laneCount_4)
            return;

        laneCountsToTest->push_back(laneCount_2);
        if (minLaneCountForTest == laneCount_2)
            return;

        laneCountsToTest->push_back(laneCount_1);
    }

    return;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDpTest::Cleanup()
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
JS_CLASS_INHERIT(LwDpTest, RmTest,
    "Simple test to demonstrate usage of SetMode() API");

CLASS_PROP_READWRITE(LwDpTest, testName, string,
                     "Name of the test to be run");
CLASS_PROP_READWRITE(LwDpTest, dpMode, string,
                     "If mode is SST or dual SST");
CLASS_PROP_READWRITE(LwDpTest, minLinkRate, UINT32,
                     "Min Link rate to be verified");
CLASS_PROP_READWRITE(LwDpTest, minLaneCount, UINT32,
                     "Min lane count to be verified");
CLASS_PROP_READWRITE(LwDpTest, linkRate, UINT32,
                     "Specific Link rate to be verified");
CLASS_PROP_READWRITE(LwDpTest, laneCount, UINT32,
                     "Specific lane count to be verified");
CLASS_PROP_READWRITE(LwDpTest, manualVerif, bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(LwDpTest, generate_gold, bool,
                     "generates gold files, default = 0");
CLASS_PROP_READWRITE(LwDpTest, onlyConnectedDisplays, bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(LwDpTest, numOfSinks, UINT32,
                     "number of sinks to be simlulated, default = 0");
CLASS_PROP_READWRITE(LwDpTest, protocol, string,
                     "protocol filter for the test, default = DP_A,DP_B");
CLASS_PROP_READWRITE(LwDpTest, force_DSC_BitsPerPixelX16, UINT32,
                      "Force DSC with specified bits per pixel. This is actual bits per pixel multiplied by 16, default = 0");
