/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2013,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_sampleMonitorOff.cpp
//! \brief Sample test to demonstrate use of these monitor off APIs:
//!         1. TurnScreenOff
//!         2. DeactivateDisplays
//!         3. ActivateDisplays
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/display.h"
#include "core/include/xp.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/evo_disp.h"
#include "core/include/memcheck.h"

#define FINDMIN(a, b)          ((a) < (b) ? (a): (b))

class SampleMonitorOffTest : public RmTest
{
public:
    SampleMonitorOffTest();
    virtual ~SampleMonitorOffTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(rastersize, string); //Config raster size through cmdline
    SETGET_PROP(onlyConnectedDisplays,bool);

private:
    DisplayID m_displayId;
    Display*  m_pDisplay;
    bool      m_RunningOnWin32Disp;
    bool      m_onlyConnectedDisplays;
    string    m_protocol;
    string    m_rastersize;

    RC MonitorOffTest();
    RC DeactivateDisplaysTest();
    RC ActivateAllCombinations();
    bool Cnr
    (
        vector <UINT32> *s,  // Subset
        UINT32           n,  // N-set
        UINT32           r   // R-Subset
    );
    RC VerifyActiveDisplays(DisplayIDs displaysToCheck);
};

//!
//! @brief SampleMonitorOffTest default constructor.
//!
//! Sets the test name, that't it.
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
SampleMonitorOffTest::SampleMonitorOffTest() : m_displayId(0)
{
    SetName("SampleMonitorOffTest");
    m_pDisplay = GetDisplay();
    m_RunningOnWin32Disp = false;
    m_onlyConnectedDisplays = false;
    m_protocol = "CRT";
    m_rastersize = "";
}

//!
//! @brief SampleMonitorOffTest default destructor.
//!
//! Placeholder : Doesn't do anything specific.
//!
//! @param[in]  NONE
//! @param[out] NONE
//-----------------------------------------------------------------------------
SampleMonitorOffTest::~SampleMonitorOffTest()
{

}

//!
//! @brief IsTestSupported(), Looks for whether test can execute in current elw.
//!
//! Checks whether we're running in Win32Display infra. This test lwrrently
//! does not support Win32Display infra.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    True if the test can be run in the current environment,
//!             false otherwise.
//-----------------------------------------------------------------------------
string SampleMonitorOffTest::IsTestSupported()
{
    DisplayIDs    Detected;
    RC            rc = OK;

    if (m_protocol.compare("DUAL_TMDS") == 0)
    {
        return "Dual TMDS is not yet supported since the test uses a low BE timing and PCLK for using DUAL_TMDS must be greater than 165MHz";
    }

    if (m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS) != OK)
    {
        return "InitializeDisplayHW failed. Can not continue with the test";
    }

    m_RunningOnWin32Disp = (m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP);

    if (m_RunningOnWin32Disp)
    {
        return "Can not run on WIN32DISP";
    }

    if (m_pDisplay->GetDetected(&Detected) != OK)
    {
        return "GetDetected failed. Can not continue with the test";
    }

    Printf(Tee::PriHigh, "%s: Requested protocol for running the test: %s\n", __FUNCTION__, m_protocol.c_str());

    for (UINT32 i = 0; i < Detected.size(); i++)
    {
        string Protocol = "";

        if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, Detected[i], m_protocol))
        {
            m_displayId = Detected[i];
            break;
        }
    }

    //If !m_onlyConnectedDisplays then consider fake displays too
    if((!m_onlyConnectedDisplays) && (m_displayId == 0))
    {
        DisplayIDs supportedDisplays;

        CHECK_RC_CLEANUP(m_pDisplay->GetConnectors(&supportedDisplays));

        for (LwU32 i = 0; i < supportedDisplays.size(); i++)
        {
            if (!m_pDisplay->IsDispAvailInList(supportedDisplays[i], Detected))
            {
                if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supportedDisplays[i], m_protocol))
                {
                    m_displayId = supportedDisplays[i];
                    CHECK_RC_CLEANUP(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, m_displayId, false));
                    break;
                }
            }
        }
    }
    if (!m_displayId)
        return "Could not find any attached display device that supports requested protocol";
    return RUN_RMTEST_TRUE;

   Cleanup:
       if (rc == OK)
           return RUN_RMTEST_TRUE;
       return rc.Message();
}

//!
//! @brief Setup(): Just doing Init from Js, nothing else.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if init passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SampleMonitorOffTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    return rc;
}

//!
//! @brief Run(): Used generally for placing all the testing stuff.
//!
//! Runs two tests:
//!     1. MonitorOffTest
//!     2. DeactivateDisplaysTest
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all tests passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SampleMonitorOffTest::Run()
{
    RC rc;

    CHECK_RC(MonitorOffTest());

    // commenting for now .. will fix later
    // Once Win32 side of Display is stable
    //CHECK_RC(DeactivateDisplaysTest());

    return rc;
}

//!
//! @brief Cleanup(): does nothing for this simple test.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK
//-----------------------------------------------------------------------------
RC SampleMonitorOffTest::Cleanup()
{
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//!
//! @brief MonitorOffTest(): Demonstates the use of TurnScreenOff() API.
//!
//! Turns monitor off and then on using TurnScreenOff() API.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all tests passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SampleMonitorOffTest::MonitorOffTest()
{

    // This is XP Style Mon Off/On Test ..

    RC            rc    = OK;
    UINT32        BaseWidth;
    UINT32        BaseHeight;
    UINT32        Depth = 32;
    UINT32        RefreshRate;
    CrcComparison crcCompObj;
    UINT32        CompCRC[3];
    UINT32        PriCRC[3];
    UINT32        SecCRC[3];
    string        crcFileName = "MonitorOffTest-";
    char          crcFileNameSuffix[16];
    bool          useSmallRaster = false;

    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        Platform::EscapeWrite("Disp_AllOrs_EnableImageOutput",0,4,1);
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
        BaseWidth    = 160;
        BaseHeight   = 120;
        RefreshRate  = 60;
    }
    else
    {
        BaseWidth    = 1024;
        BaseHeight   = 768;
        RefreshRate  = 60;
    }
    sprintf(crcFileNameSuffix, "-%dx%d.xml", BaseWidth, BaseHeight);

    Printf(Tee::PriHigh, "Running subtest %s on displayId 0x%08X\n", __FUNCTION__, (UINT32)m_displayId);

    crcFileName += m_protocol;
    crcFileName += crcFileNameSuffix;

    Surface2D CoreImage;
    CHECK_RC(m_pDisplay->SetupChannelImage(m_displayId,
                                         FINDMIN(BaseWidth, 640),
                                         FINDMIN(BaseHeight, 480),
                                         Depth,
                                         Display::CORE_CHANNEL_ID,
                                         &CoreImage,
                                         "dispinfradata/images/baseimage640x480.PNG"));
    Printf(Tee::PriHigh, "Doing Modeset On DisplayID %0x",(UINT32)m_displayId);

    // do modeset on the current displayId
    CHECK_RC(m_pDisplay->SetMode(m_displayId,
                               BaseWidth,
                               BaseHeight,
                               Depth,
                               RefreshRate));

    EvoCRCSettings crcSettings;
    crcSettings.ControlChannel = EvoCRCSettings::CORE;

    m_pDisplay->SetupCrcBuffer(m_displayId, &crcSettings);

    // Get CRC before turning monitor off
    CHECK_RC(m_pDisplay->GetCrcValues(m_displayId, &crcSettings, &CompCRC[0], &PriCRC[0], &SecCRC[0]));
    CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), crcFileName.c_str(), &crcSettings, true));
    //CHECK_RC(m_pDisplay->StartRunningCrc(m_displayId, &crcSettings));

    // turn monitor off
    CHECK_RC_MSG(m_pDisplay->SetMonitorPower(m_displayId , true), "MonitorOffTest failed, error = ");

    // Get CRC after turning monitor off
    CHECK_RC(m_pDisplay->GetCrcValues(m_displayId, &crcSettings, &CompCRC[1], &PriCRC[1], &SecCRC[1]));
    CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), crcFileName.c_str(), &crcSettings, false));

    // turn monitor on
    CHECK_RC_MSG(m_pDisplay->SetMonitorPower(m_displayId, false), "MonitorOffTest failed, error = ");

    // Get CRC after turning monitor back on
    CHECK_RC(m_pDisplay->GetCrcValues(m_displayId, &crcSettings, &CompCRC[2], &PriCRC[2], &SecCRC[2]));
    CHECK_RC(crcCompObj.DumpCrcToXml(GetBoundGpuDevice(), crcFileName.c_str(), &crcSettings, false));

    //CHECK_RC(m_pDisplay->StopRunningCrc(m_displayId, &crcSettings));
    Printf(Tee::PriHigh, "----------------------------------------------\n");
    Printf(Tee::PriHigh, "                     Compositor     Primary\n");
    Printf(Tee::PriHigh, "Before Monitor Off   0x%08X     0x%08X\n",CompCRC[0], PriCRC[0]);
    Printf(Tee::PriHigh, "After  Monitor Off   0x%08X     0x%08X\n",CompCRC[1], PriCRC[1]);
    Printf(Tee::PriHigh, "After  Monitor On    0x%08X     0x%08X\n",CompCRC[2], PriCRC[2]);
    Printf(Tee::PriHigh, "----------------------------------------------\n");

    RC tempRc = OK;

    // CRC sanity check#1: Comp CRC should be same for all cases
    if ( (CompCRC[0] != CompCRC[1] || CompCRC[1] != CompCRC[2]) )
    {
        Printf(Tee::PriHigh, "All three Comp CRCs (0x%X, 0x%X, 0x%X) should match but they aren't matching\n", CompCRC[0], CompCRC[1], CompCRC[2]);
        tempRc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    // CRC sanity check#2: 1'st and 3'rd Primary CRC should be same
    if (PriCRC[0] != PriCRC[2])
    {
        Printf(Tee::PriHigh, "First and third Primary CRCs (0x%X, 0x%X) should match but they aren't matching\n", PriCRC[0], PriCRC[2]);
        tempRc = RC::GOLDEN_VALUE_MISCOMPARE;
    }
    // CRC sanity check#2: 2'nd primary CRC must NOT match 1'st or 3'rd
    if (PriCRC[0] == PriCRC[1])
    {
        Printf(Tee::PriHigh, "First Primary CRC (0x%X) shouldn't match second (0x%X) but it is matching\n", PriCRC[0], PriCRC[1]);
        tempRc = RC::GOLDEN_VALUE_MISCOMPARE;
    }

    // Vista Style Monitor Off
    CHECK_RC(m_pDisplay->DetachDisplay(DisplayIDs(1, m_displayId)));

    if (CoreImage.GetMemHandle())
    {
        CoreImage.Free();
    }

    if ( (OK != tempRc) && (OK == rc) )
    {
        rc = tempRc;
    }
    return rc;
}

//!
//! @brief DeactivateDisplaysTest(): Runs deactivation test.
//! Saves lwrrently active displays, and calls ActivateAllCombinations which
//! will activate all combinations of lwrrently detected displays.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all tests passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SampleMonitorOffTest::DeactivateDisplaysTest()
{
    RC rc;
    RC cleanupRc;

    DisplayIDs activeDisplays, gdiPrimary;
    bool gdiPrimaryBelongsToThisSubdevice = false;

    // Save lwrrently active displays so we can restore them after the test.
    CHECK_RC_CLEANUP(m_pDisplay->GetScanningOutDisplays(&activeDisplays));

    if (m_RunningOnWin32Disp)
    {
        // Save current GDI primary display
        CHECK_RC_CLEANUP(m_pDisplay->GetSystemPrimaryDisplay(&gdiPrimary, &gdiPrimaryBelongsToThisSubdevice));
    }

    // DeactivateDisplaysTest comprises of three tests, we'll run them one by one
    CHECK_RC_CLEANUP(ActivateAllCombinations());

Cleanup:
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "Test %s failed, error = %s. This failure could be due to "
            "bug 624099, please check if this bug is fixed.\n", __FUNCTION__, rc.Message());
    }

    if (!m_RunningOnWin32Disp)
    {
        // Do modeset
        cleanupRc = m_pDisplay->SetMode(activeDisplays, 640, 480, 32, 60);
        if (OK != cleanupRc)
        {
            Printf(Tee::PriHigh, "Test %s: Failure while cleaning up, while activating the "
                "displays that were originally active, error = %s\n", __FUNCTION__, rc.Message());
        }
    }
    // Set the GDI primary to what it was.
    if (m_RunningOnWin32Disp && gdiPrimaryBelongsToThisSubdevice)
    {
        cleanupRc = m_pDisplay->SetSystemPrimaryDisplay(&gdiPrimary[0]);
        if (OK != cleanupRc)
        {
            Printf(Tee::PriHigh, "Test %s: Failure while cleaning up, while setting the "
                "GDI primary to what it was before, error = %s\n", __FUNCTION__, rc.Message());
        }
    }
    // return first error
    return (OK != rc)? rc : cleanupRc;
}

//!
//! @brief This test will try to activate all combinations of lwrrently
//! detected displays. For e.g. if 0x1 and 0x2 displays are detected, these
//! combinations will be tried: 0x1, 0x2, 0x1 & 0x2.
//!
//! @param[in]  NONE
//! @param[out] NONE
//!
//! @returns    OK if all tests passed, specific RC if failed.
//-----------------------------------------------------------------------------
RC SampleMonitorOffTest::ActivateAllCombinations()
{
    RC          rc;
    DisplayIDs  detectedDisplays;
    UINT32      loopCount;
    // Get the detected displays, we'll run the test on all the detected displays
    CHECK_RC(m_pDisplay->GetDetected(&detectedDisplays));

    UINT32 numDetectedDisplays = (UINT32)detectedDisplays.size();

    // Get number of heads on this subdevice. We can only activate this many no. of displays at a time.
    UINT32 numHeads = m_pDisplay->GetHeadCount();

    for (UINT32 numKeepActive = 1 ; numKeepActive <= numHeads && numKeepActive <= numDetectedDisplays; ++numKeepActive)
    {
        Printf(Tee::PriLow, "%s Test: Trying all combinations with %u displays"
            " active at a time\n", __FUNCTION__, numKeepActive);
        vector <UINT32> lwrCombination(numKeepActive, 0xffffffff);
        // The first combination
        for(loopCount=0; loopCount < numKeepActive; ++loopCount) lwrCombination[loopCount] = loopCount;
        do
        {
            // Setmode will make display Active.
            DisplayIDs makeActive;
            for(loopCount = 0; loopCount < numKeepActive; ++loopCount)
            {
                makeActive.push_back(detectedDisplays[lwrCombination[loopCount]]);
            }

            Printf(Tee::PriLow, "Trying to activate this combination:\n");
            m_pDisplay->PrintDisplayIds(Tee::PriLow, makeActive);

            if (!m_RunningOnWin32Disp)
            {
                // Do a modeset
                CHECK_RC_CLEANUP(m_pDisplay->SetMode(makeActive, 640, 480, 32, 60));
            }
            // Verify that only these displays are active
            CHECK_RC_CLEANUP(VerifyActiveDisplays(makeActive));
        } while( Cnr(&lwrCombination,numDetectedDisplays,numKeepActive)) ;
    }
    Printf(Tee::PriLow, "%s Test success\n", __FUNCTION__);
    return rc;

Cleanup:
    Printf(Tee::PriHigh, "%s Test failed, rc = %s\n", __FUNCTION__, rc.Message());
    return rc;
}

//!
//! @brief Generates the next combination for specific values of N, R.
//! For e.g. nCr where n = 3, r = 2 is: (1,2) (1,3) and (2,3)
//!
//! @param[in, out] pS : The current combination.
//! @param[in]      n  : N value.
//! @param[in]      r  : R value.
//! @param[out]     pS : The next combination is returned in pS.
//!
//! @returns true if the next combination exists, false otherwise.
//-----------------------------------------------------------------------------
bool SampleMonitorOffTest::Cnr
(
    vector <UINT32> *pS,  // Subset
    UINT32           n,   // N-set
    UINT32           r    // R-Subset
)
{
    vector <UINT32> s(*pS);
    if(s[0] == (n-r))
        return false;

    UINT32 index = n - 1;
    UINT32 i     = r - 1;

    while(s[i] == index && i > 0)
    {
        --i;
        --index;
    }

    s[i] += 1;
    ++i;

    while(i < r)
    {
        s[i] = s[i-1] + 1;
        ++i;
    }
    *pS = s;
    return true;
}

//!
//! @brief Verifies that the displays sent by the caller in displaysToCheck
//! param are indeed active and no other displays apart from these are active.
//!
//! @param[in]  displaysToCheck
//!             The displays which will be checked for active state.
//! @param[out] NONE
//!
//! @returns    OK if all displays to check are active and no other displays
//!             apart from these are active, LWRM_ERROR if not.
//-----------------------------------------------------------------------------
RC SampleMonitorOffTest::VerifyActiveDisplays(DisplayIDs displaysToCheck)
{
    RC          rc;
    DisplayIDs  lwrActiveDisplays;

    CHECK_RC(m_pDisplay->GetScanningOutDisplays(&lwrActiveDisplays));

    // Verify that the only displays we are checking show up in the active list.
    if (!m_pDisplay->IsDispAvailInList(displaysToCheck, lwrActiveDisplays) ||
        !m_pDisplay->IsDispAvailInList(lwrActiveDisplays, displaysToCheck))
    {
        Printf(Tee::PriHigh, "Function %s: Display ctive verification failed.\n", __FUNCTION__);
        return RC::LWRM_ERROR;
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
JS_CLASS_INHERIT(SampleMonitorOffTest, RmTest,
    "Simple test to demonstrate usage of monitor off APIs");

CLASS_PROP_READWRITE(SampleMonitorOffTest, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(SampleMonitorOffTest, rastersize, string,
                     "Desired raster size (small/large)");
CLASS_PROP_READWRITE(SampleMonitorOffTest, onlyConnectedDisplays, bool,
                     "run on only connected displays, default = 0");

