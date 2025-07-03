/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_lwdispjsonconfig.cpp
//! \brief the test inits LwDisplay HW and sets up mode
//! \based on json lwdisplay configuration file
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/display/lwdisplay/lw_disp.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/rc.h"
#include "gpu/display/statecache_C3.h"
#include "sim/xp.h"
#include "parsejsonconfig.h"

using namespace std;

class LwDispJsonCfgTest : public RmTest
{
public:
    LwDispJsonCfgTest();
    virtual ~LwDispJsonCfgTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    SETGET_PROP(jsonconfigfile, string);
    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(IncludeCrc, bool);
    SETGET_PROP(protocolFilterAll, string);
private:
    string m_jsonconfigfile;
    Display *m_pDisplay;
    CParseJsonDisplaySettings *m_jsonSettings;
    bool m_onlyConnectedDisplays;
    bool m_generate_gold;
    bool m_IncludeCrc;
    string m_protocolFilterAll;
};

LwDispJsonCfgTest::LwDispJsonCfgTest()
{
    m_pDisplay = GetDisplay();
    m_onlyConnectedDisplays = false;
    m_generate_gold = false;
    m_IncludeCrc = true;
    m_jsonSettings = NULL;
    m_jsonconfigfile="dispinfradata/jsonconfigs/disp_head0_win0_pitch.json";
    m_protocolFilterAll="DUAL_TMDS,SINGLE_TMDS_A,SINGLE_TMDS_B";
    SetName("LwDispJsonCfgTest");
}

//! \brief LwDispJsonCfgTest destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
LwDispJsonCfgTest::~LwDispJsonCfgTest()
{
    if (m_jsonSettings)
        delete m_jsonSettings;
}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string LwDispJsonCfgTest::IsTestSupported()
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
RC LwDispJsonCfgTest::Setup()
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
RC LwDispJsonCfgTest::Run()
{
    RC rc;
    LwDisplay *pLwDisplay = m_pDisplay->GetLwDisplay();
    vector <DisplayPanel *>displayPanels; // enumerated display panels on a given board
    vector <DisplayPanel *>validDisplayPanels; // display panels on which mode will be set
    DisplayPanelHelper *pDispPanelHelper  = new DisplayPanelHelper(m_pDisplay);
    vector<pair<DisplayPanel *, DisplayPanel *>> dualSSTPairs;
    vector<string> PathDirectories;
    string jsonFileName= "";

    // 1) Initialize display HW
    CHECK_RC_CLEANUP(pLwDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    CHECK_RC_CLEANUP(pDispPanelHelper->EnumerateDisplayPanels(m_protocolFilterAll,
             displayPanels, dualSSTPairs, !m_onlyConnectedDisplays));

    m_jsonSettings = new CParseJsonDisplaySettings();
    if (!m_jsonSettings)
    {
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }

    PathDirectories.push_back(".");
    Utility::AppendElwSearchPaths(&PathDirectories, "LD_LIBRARY_PATH");
    Utility::AppendElwSearchPaths(&PathDirectories, "MODS_RUNSPACE");

    jsonFileName = Utility::FindFile(m_jsonconfigfile, PathDirectories);
    if (jsonFileName.size() == 0)
    {
        Printf(Tee::PriError,"\n %s Wrong file name, Please input correct file name \n",  m_jsonconfigfile.c_str());
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }

    m_jsonconfigfile = jsonFileName + m_jsonconfigfile;

    if (!m_jsonSettings->parseAndApplyDisplaySettings(m_jsonconfigfile, pLwDisplay, displayPanels, validDisplayPanels))
    {
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }

    if (validDisplayPanels.size()==0)
    {
        // Cannot find a valid configuration for modeset
        // FIXIT: Add debug print here
        rc = RC::SOFTWARE_ERROR;
        CHECK_RC_CLEANUP(rc);
    }
    for (auto & pDisplayPanel : validDisplayPanels)
    {
        //If window settings are allocated for this panel only then go ahead with modeset
        if (!pDisplayPanel->windowSettings.size())
            continue;

        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->AllocateWindowsLwstomWinSettings(pDisplayPanel));

        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->SetupChannelImage());

        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->SetLwstomSettings());

        if (pDisplayPanel->bValidLwrsorSettings)
        {
            CHECK_RC_CLEANUP(pDisplayPanel->pModeset->AllocateLwrsorImm());
            CHECK_RC_CLEANUP(pDisplayPanel->pModeset->SetupLwrsorChannelImage());
        }

        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->Initialize());
        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->SetMode());
    }

    if (m_IncludeCrc)
    {
        for (auto & pDisplayPanel : validDisplayPanels)
        {
            rc = pDisplayPanel->pModeset->AutoVerification(m_generate_gold);
            if (rc == RC::DATA_MISMATCH)
            {
                Printf(Tee::PriHigh,
                   "\n DP CRC got mismatched for head %d \n", pDisplayPanel->head);
            }
            rc.Clear();
        }
    }
    for (auto & pDisplayPanel : validDisplayPanels)
    {
        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->ClearLwstomSettings());
        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->Detach());
        CHECK_RC_CLEANUP(pDisplayPanel->pModeset->FreeWindowsAndSurface());
    }

#ifdef DUMP_STATE_CACHE
    StateCache_C3 statecache;
    statecache.CompareStateCache(m_pDisplay->GetOwningDisplaySubdevice(),
                                 Display::CORE_CHANNEL_ID, 0);

    for (UINT32 i = 0; i < 2; i++)
    {
         statecache.CompareStateCache(m_pDisplay->GetOwningDisplaySubdevice(),
                                      Display::WINDOW_CHANNEL_ID, i);
    }
#endif

Cleanup:
    delete pDispPanelHelper;

    return rc;
}

//! \brief Cleanup, does nothing for this simple test
//------------------------------------------------------------------------------
RC LwDispJsonCfgTest::Cleanup()
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
JS_CLASS_INHERIT(LwDispJsonCfgTest, RmTest,
    "Test to setup display configuration based on json configuration.");
CLASS_PROP_READWRITE(LwDispJsonCfgTest, jsonconfigfile, string,
                "lwdisplay json display mode configuration file location");
CLASS_PROP_READWRITE(LwDispJsonCfgTest, onlyConnectedDisplays, bool,
               "only use connected devices for the configuration");
CLASS_PROP_READWRITE(LwDispJsonCfgTest, generate_gold, bool,
               "just generate the golden files");
CLASS_PROP_READWRITE(LwDispJsonCfgTest, IncludeCrc, bool,
               "This has to be true to compare/generate CRC information");
CLASS_PROP_READWRITE(LwDispJsonCfgTest, protocolFilterAll, string,
               "Protocols to consider for this test");

