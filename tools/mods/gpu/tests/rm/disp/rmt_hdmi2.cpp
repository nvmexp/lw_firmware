/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_Hdmi2.cpp
//! \brief Hdmi Ver2 verif test to cycle through all connected
//! displays on all heads and verify Scrambling and clock mode.
//!

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/memcheck.h"
#include "core/include/modsedid.h"
#include "core/include/display.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl5070/ctrl5070chnc.h" // IsModePossible
#include "device/interface/i2c.h"
#include "disp/v02_05/dev_disp.h"       // Hdmi 2.0 state verification
#include "gpu/disp/hdmi_spec_addendum.h"
#include "gpu/display/modeset_utils.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/tests/rm/utility/hdcputils.h"

#include <time.h>

#define OBJ_MAX_SORS    4

class Hdmi2 : public RmTest
{
public:
    Hdmi2();
    virtual ~Hdmi2();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    RC VerifyDisp(DisplayID SetModeDisplay,
                    UINT32 width,
                    UINT32 height,
                    UINT32 depth,
                    UINT32 refreshRate,
                    UINT32 thisHead,
                    UINT32 sorNum,
                    bool gt240ClockSupported,
                    bool lte340ScramblingSupp);

    RC VerifyHDCP(DisplayID Display);
    RC VerifyImage(DisplayID displayID,
                   LwU32 width,
                   LwU32 height,
                   LwU32 depth,
                   LwU32 refreshRate,
                   Surface2D* CoreImage);

    RC VerifyScramblingState(DisplayID display, LwU32 I2CPort, bool bEnable);

    RC VerifyClkGte340State(DisplayID display, LwU32 I2CPort, bool bEnable);

    RC ReadWriteI2cRegister(LwU32 I2CPort,
                            UINT32 I2CAddress,
                            UINT32 I2CRegisterOFfset,
                            UINT32 *pReadData,
                            bool bReadWrite = false);

    //!
    //! \brief Helpler function to put the current thread to sleep for a given number(base on ptimer) of milliseconds,
    //!         giving up its CPU time to other threads.
    //!         Warning, this function is not for precise delays.
    //! \param miliseconds [in ]: Milliseconds to sleep
    //! \param runOnEmu [in]: It is on emulation or not
    //! \param gpuSubdev [in]: Pointer to gpu subdevice
    //!
    void Sleep(LwU32 miliseconds, LwBool runOnEmu, GpuSubdevice *gpuSubdev);

    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(generate_gold, bool);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(rastersize, string); //Config raster size through cmdline
    SETGET_PROP(WithAllHeads,bool);  //Config WithAllHeads through cmdline
    SETGET_PROP(withAllSors,bool);   //Config WithAllSors through cmdline
    SETGET_PROP(check_hdcp, bool);

private:
    Display     *m_pDisplay;
    bool        m_onlyConnectedDisplays;
    bool        m_manualVerif;
    bool        m_generate_gold;
    string      m_protocol;
    UINT32      m_pixelclock;
    string      m_rastersize;
    bool        m_WithAllHeads;
    bool        m_withAllSors;
    string      m_goldenCrcDir;
    bool        m_isHdmi2_0Capable;
    string      m_edidFileName;
    bool        m_check_hdcp;
    bool        m_ignoreHdmi2LargerModes;
};

//! \brief Hdmi2 constructor
//!
//! Does SetName which has to be done inside ever test's constructor
//!
//------------------------------------------------------------------------------
Hdmi2::Hdmi2()
{
    SetName("Hdmi2");
    m_onlyConnectedDisplays = false;
    m_manualVerif = false;
    m_generate_gold = false;
    m_protocol = "SINGLE_TMDS_A,SINGLE_TMDS_B";
    m_pixelclock = 0;
    m_rastersize = "";
    m_WithAllHeads = false;
    m_withAllSors = false;
    m_goldenCrcDir = "DTI/Golden/Hdmi2Test/";
    m_isHdmi2_0Capable = false;
    m_edidFileName = "edids/HDMI_Ver2_4k.txt";
}

//! \brief Hdmi2 destructor
//!
//! does  nothing
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Hdmi2::~Hdmi2()
{

}

//! \brief IsSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//! The test has to run of GF11X chips but can run on all elwironments
//------------------------------------------------------------------------------
string Hdmi2::IsTestSupported()
{
    if (GetBoundGpuSubdevice()->DeviceId() < Gpu::GM204)
    {
        return "HDMI 2.0 is supported from GM20x and later only";
    }
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return "HDMI 2.0 test is NOT supported on non hardware platform due to NO i2c support for monitor.";
    }
    if (GetBoundGpuSubdevice()->SupportsInterface<I2c>())
    {
        return "HDMI 2.0 test is NOT supported on current device due to lack of I2C support.";
    }

    return RUN_RMTEST_TRUE;
}

//! \setup Initialise internal state from JS
//!
//! Initial state has to be setup based on the JS setting. This function
// does the same.
//------------------------------------------------------------------------------
RC Hdmi2::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    m_pDisplay = GetDisplay();

    // manual Verif must be used
    //1) When OnlyConnectedDisplays are used
    //2) And only on Hw Platforms
    if (m_manualVerif && (!m_onlyConnectedDisplays ||
        Platform::GetSimulationMode() != Platform::Hardware))
    {
        m_manualVerif = false;
    }

    if (GetBoundGpuSubdevice()->DeviceId() >= Gpu::GM204)
    {
        m_isHdmi2_0Capable = 1;
    }

    return rc;
}

//! \brief Run the test
//!
//! Call the SetMode() API with some default values
//!
//! \return OK if the SetMode call succeeded, else corresponding rc
//------------------------------------------------------------------------------
RC Hdmi2::Run()
{
    RC rc;
    bool isXbarSupported =  FALSE;
    bool dispIdSupportsHdmi = FALSE;
    Display::Mode smallerMode, largerMode;
    vector<Display::Mode> listedModes;

    CHECK_RC(ErrorLogger::StartingTest());
    ErrorLogger::IgnoreErrorsForThisTest();

    CHECK_RC(m_pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));

    if (m_pDisplay->GetOwningDisplaySubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_SOR_XBAR))
    {
        isXbarSupported = TRUE;
    }

    // get all the connected displays
    DisplayIDs Detected;
    CHECK_RC(m_pDisplay->GetDetected(&Detected));
    Printf(Tee::PriHigh, "All Detected displays = \n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

    DisplayIDs workingSet;

    //Only the protocol passed by the user must be used
    for (UINT32 i=0; i < Detected.size(); i++)
    {
        if(DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay,
                                                  Detected[i],
                                                  m_protocol))
        {
            //
            // if the edid is not valid for disp id then SetLwstomEdid.
            // This is needed on emulation / simulation / silicon (when kvm used)
            //
            if ((!(DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, Detected[i]) &&
                   DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, Detected[i]))) ||
                   GetBoundGpuSubdevice()->IsEmulation()) // Set Custom HDMI edid on emulation to run the test
            {
                if((rc = DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, Detected[i], true, m_edidFileName)) != OK)
                {
                    Printf(Tee::PriLow,"SetLwstomEdid failed for displayID = 0x%X.\n",
                        (UINT32)Detected[i]);
                    return rc;
                }
            }

            listedModes.clear();
            // getListedResolutions & check get 1 res above 340Mhz.
            // this is needed so as to ilwalidate old edid cached inside mode.
            rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                                 Detected[i],
                                                 &listedModes,
                                                 false);

            m_pDisplay->GetHdmiSupport((UINT32)Detected[i], &dispIdSupportsHdmi);

            if (!dispIdSupportsHdmi)
            {
                Printf(Tee::PriLow,"%s: DisplayID = 0x%X doesnt supports HDMI. Skipping it.\n",
                        __FUNCTION__, (UINT32)Detected[i]);
                continue;
            }

            LW0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS i2cPortParams = {0};
            CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&(i2cPortParams.subDeviceInstance)));
            i2cPortParams.displayId = (UINT32)Detected[i];

            CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_I2C_PORTID,
                &i2cPortParams, sizeof(i2cPortParams)));

            UINT32 clkDtflags = 0;

            //
            // On samsung panel we are loosing coverage of HDCP2.2 since
            // HDCP2.2 and HDMI2.0 are not supported together, so to get coverage
            // of HDCP2.2 ignore Hdmi2.0 larger modes testing.
            //
            m_ignoreHdmi2LargerModes = (ReadWriteI2cRegister(i2cPortParams.ddcPortId -1,
                                                             LW_HDMI_SCDC_SLAVE_ADR,
                                                             LW_HDMI_SCDC_STATUS_FLAGS_0,
                                                             &clkDtflags) == OK) ?
                                                             false : true;
            workingSet.push_back(Detected[i]);
        }
    }

    Printf(Tee::PriHigh, "\n Detected displays with protocol %s which supports HDMI are :\n",m_protocol.c_str());
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, workingSet);

    //If !m_onlyConnectedDisplays then consider fake displays too
    if(!m_onlyConnectedDisplays)
    {
        //get all the available displays
        DisplayIDs supported;
        CHECK_RC(m_pDisplay->GetConnectors(&supported));

        DisplayIDs displaysToFake;
        for(UINT32 loopCount = 0; loopCount < supported.size(); loopCount++)
        {
            if (!m_pDisplay->IsDispAvailInList(supported[loopCount], Detected) &&
                DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay, supported[loopCount], m_protocol))
            {
                displaysToFake.push_back(supported[loopCount]);
            }
        }

        for(UINT32 loopCount = 0; loopCount < displaysToFake.size(); loopCount++)
        {
            CHECK_RC(DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay,displaysToFake[loopCount], false, m_edidFileName));

            // you can check HDMI support only after u fake an EDID over it.
            m_pDisplay->GetHdmiSupport((UINT32)displaysToFake[loopCount], &dispIdSupportsHdmi);

            if (!dispIdSupportsHdmi)
            {
                Printf(Tee::PriLow,"%s: DisplayID = 0x%X doesnt supports HDMI. Skipping it.\n",
                        __FUNCTION__, (UINT32)supported[loopCount]);
                continue;
            }

            workingSet.push_back(displaysToFake[loopCount]);
        }

    }

    //get the no of displays to work with
    UINT32 numDisps = static_cast<UINT32>(workingSet.size());

    if (numDisps == 0)
    {
        Printf(Tee::PriHigh, "No displays detected, test aborting... \n");
        return RC::SOFTWARE_ERROR;
    }

    LWT_TIMING timing;
    UINT32 flag = 0;
    bool foundLargerMode = false;
    UINT08  rawEdid[EDID_SIZE_MAX];
    UINT32  rawEdidSize = EDID_SIZE_MAX;
    LWT_EDID_INFO   edidInfo;
    Edid    *pEdid = m_pDisplay->GetEdidMember();
    UINT32 emuRefreshRate, modesCountInt;
    bool   useSmallRaster = false;

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
            Printf(Tee::PriHigh, "%s: Bad raster size argument: \"%s\". Treating as \"large\".\n", __FUNCTION__, m_rastersize.c_str());
        }
    }

    if (useSmallRaster)
    {
        // This is for Fmodel and RTL which are slow in producing frames
        smallerMode.width    = 160;
        smallerMode.height   = 120;
        smallerMode.depth    = 32;
        smallerMode.refreshRate = 60;

        // 200[HTotal] * 160[VTotal] * 11000 [RR] = 352.0Mhz [pclk]
        emuRefreshRate       = 11000;
    }
    else
    {
        smallerMode.width    = 640;
        smallerMode.height   = 480;
        smallerMode.depth    = 32;
        smallerMode.refreshRate = 60;

        // 800[HTotal] * 525[VTotal] * 850 [RR] = 357.0Mhz [pclk]
        emuRefreshRate       = 850;
    }

    UINT32 sorNum  = 0;
    UINT32 disp = 0;
    vector<UINT32>sorList;

    listedModes.clear();

     // getListedResolutions & check get 1 res above 340Mhz.
    rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                             workingSet[disp],
                                             &listedModes,
                                             false);

    //retrieving the edid.
    CHECK_RC(pEdid->GetRawEdid(workingSet[disp], rawEdid, &rawEdidSize));

    // lets grab the LWT_EDID_INFO
    memset(&edidInfo, 0, sizeof(edidInfo));
    LwTiming_ParseEDIDInfo(rawEdid, rawEdidSize, &edidInfo);

    // printing the HDMI ver2 params from the EDID.
    Printf(Tee::PriHigh, "edidInfo.hdmi_2_0_info.SCDC_present =%d\n", edidInfo.hdmi_2_0_info.SCDC_present);
    Printf(Tee::PriHigh, "edidInfo.hdmi_2_0_info.rr_capable =%d\n", edidInfo.hdmi_2_0_info.rr_capable);
    Printf(Tee::PriHigh, "edidInfo.hdmi_2_0_info.lte_340Mcsc_scramble =%d\n", edidInfo.hdmi_2_0_info.lte_340Mcsc_scramble);
    Printf(Tee::PriHigh, "edidInfo.hdmi_2_0_info.max_TMDS_char_rate  =%d\n", edidInfo.hdmi_2_0_info.max_TMDS_char_rate);

    for (modesCountInt = 0; modesCountInt < (UINT32)listedModes.size(); modesCountInt++)
    {
        //retrieve backend timing using the CVT_RB
        memset(&timing, 0 , sizeof(LWT_TIMING));
        rc = ModesetUtils::CallwlateLWTTiming(m_pDisplay,
            workingSet[disp],
            listedModes[modesCountInt].width,
            listedModes[modesCountInt].height,
            listedModes[modesCountInt].refreshRate,
            Display::AUTO,
            &timing,
            flag);
        if (rc != OK)
        {
            Printf(Tee::PriHigh,
                "ERROR: %s: CallwlateLWTTiming() failed to get backend timings. RC = %d.\n",
                __FUNCTION__, (UINT32)rc);
            CHECK_RC(rc);
        }

        Printf(Tee::PriHigh, "\n DisplayId [0x%X]:Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d; PClk[10Khz] = %u.\n",
               (UINT32)workingSet[disp], modesCountInt,
               listedModes[modesCountInt].width, listedModes[modesCountInt].height,
               listedModes[modesCountInt].depth, listedModes[modesCountInt].refreshRate,
               (UINT32)timing.pclk);

        ModesetUtils::PrintLwtTiming(&timing, Tee::PriHigh);

        if ((timing.pclk *10000ULL) >= (340000000ULL) && !foundLargerMode)
        {
            largerMode.width        = listedModes[modesCountInt].width;
            largerMode.height       = listedModes[modesCountInt].height;
            largerMode.depth        = listedModes[modesCountInt].depth;
            largerMode.refreshRate  = listedModes[modesCountInt].refreshRate;

            if (Platform::GetSimulationMode() == Platform::Hardware)
            {
               // On silicon render 4kx2k mode.
                if (largerMode.width == 4096 &&
                    largerMode.height == 2160 &&
                    largerMode.refreshRate == 60)
                {
                    foundLargerMode         = true;
                }
            }
            else
            {
                foundLargerMode         = true;
            }
        }
    }

    // WAR for faster verif @ Emu and fmodel.
    // To reduce modeset time, lets use a smaller backend raster and
    // use much larger refresh rate. This trick helps verify larger pclk much faster.
    if (GetBoundGpuSubdevice()->IsEmulation() ||
        Platform::GetSimulationMode() != Platform::Hardware)
    {
            largerMode.width        = smallerMode.width;
            largerMode.height       = smallerMode.height;
            largerMode.depth        = smallerMode.depth;
            largerMode.refreshRate  = emuRefreshRate;
    }

    UINT32 numHeads = GetDisplay()->GetHeadCount();
    UINT32 HeadNum = 0;

    /*
    ==========================================================================================================================================
    | GM20x: Hdmi Ver 2: Verification summary report.                                                                                               |
    ------------------------------------------------------------------------------------------------------------------------------------------
    Case    |   PCLK    |   Scrambling LTE 340      |   Clk GTE 340 Supported   ||  Result      |   Scrambling Expected |   CLK GTE 340 Expected    |
    ==========================================================================================================================================
    0       |   Low     |   Not Supported           |   Not Supported           ||  PASS        |   disabled            |   disabled                |
    ------------------------------------------------------------------------------------------------------------------------------------------
    1       |   Low     |   Not Supported           |   Supported               ||  PASS        |   disabled            |   disabled                |
    ------------------------------------------------------------------------------------------------------------------------------------------
    2       |   Low     |   Supported               |   Not Supported           ||  PASS        |   enabled             |   disabled                |
    ------------------------------------------------------------------------------------------------------------------------------------------
    3       |   Low     |   Supported               |   Supported               ||  PASS        |   enabled             |   disabled                |
    ------------------------------------------------------------------------------------------------------------------------------------------
    4       |   High    |   Not Supported           |   Not Supported           ||  INVALID CASE|   dont care           |   dont care               |
    ------------------------------------------------------------------------------------------------------------------------------------------
    5       |   High    |   Not Supported           |   Supported               ||  NOT TESTED  |   dont care           |   dont care               |
    ------------------------------------------------------------------------------------------------------------------------------------------
    6       |   High    |   Supported               |   Not Supported           ||  INVALID CASE|   dont care           |   dont care               |
    ------------------------------------------------------------------------------------------------------------------------------------------
    7       |   High    |   Supported               |   Supported               ||  PASS        |   enabled             |   enabled                 |
    ==========================================================================================================================================
    Note: To ensure to verify most combinations are covered we are using following random sequence.

    * Verify Case 7
    * Verify Case 1
    * Verify Case 7
    * Verify Case 2
    * Verify Case 0
    * Verify Case 3
    */

    UINT32 sorExcludeMask = 0;
    UINT32 numSors = OBJ_MAX_SORS;
    while(sorNum < numSors)
    {
        if (isXbarSupported)
        {
            sorExcludeMask = (0xf)^(BIT(sorNum));

            CHECK_RC(m_pDisplay->AssignSOR(DisplayIDs(1, workingSet[disp]), sorExcludeMask,
                sorList, Display::AssignSorSetting::ONE_HEAD_MODE));

            for (UINT32 i = 0; i < OBJ_MAX_SORS; i++)
            {
                if ((workingSet[disp] & sorList[i]) == workingSet[disp])
                {
                    Printf(Tee::PriHigh, "Using SOR = %d\n", sorNum);
                    break;
                }
            }
        }

        while (HeadNum < numHeads)
        {
            DisplayID SetModeDisplay = workingSet[disp];
            Surface2D CoreImage;

            UINT32 thisHead = Display::ILWALID_HEAD;
            if (m_WithAllHeads)
            {
                thisHead = HeadNum;
            }
            else
            {
                if ((rc = GetDisplay()->GetHead(SetModeDisplay, &thisHead)) == RC::ILWALID_HEAD)
                {
                    Printf(Tee::PriHigh, "Hdmi2: Failed to Get Head.\n ");
                    return rc;
                }
            }

            if (!m_ignoreHdmi2LargerModes)
            {
                // Case 7: larger mode with scrambling and clock mode
                Printf(Tee::PriHigh, "***** Verifying Case 7: PCLK = High, lte340MhzScrambling = true, gt340MhzClockMode = true.\n ");
                rc = (VerifyDisp(SetModeDisplay,
                                 largerMode.width,
                                 largerMode.height,
                                 largerMode.depth,
                                 largerMode.refreshRate,
                                 thisHead,
                                 sorNum,
                                 true,               // gt340MhzClockMode
                                 true));             // lte340MhzScrambling
                if (rc != OK)
                {
                    Printf(Tee::PriHigh, "***** Case 7 failed: RC= %d.\n ",(UINT32)rc);
                    return rc;
                }
            }

            // Case 1: smaller mode without scrambling test but ClkGte340 set
            Printf(Tee::PriHigh, "***** Verifying Case 1: PCLK =  Low, lte340MhzScrambling = false, gt340MhzClockMode = true.\n ");
            rc = VerifyDisp(SetModeDisplay,
                        smallerMode.width,
                        smallerMode.height,
                        smallerMode.depth,
                        smallerMode.refreshRate,
                        thisHead,
                        sorNum,
                        true,              // gt340MhzClockMode
                        false);            // lte340MhzScrambling
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "***** Case 1 failed: RC= %d.\n ",(UINT32)rc);
                return rc;
            }

            if (!m_ignoreHdmi2LargerModes)
            {
                // Case 7: larger mode with scrambling and clock mode
                Printf(Tee::PriHigh, "***** Verifying Case 7: PCLK = High, lte340MhzScrambling = true, gt340MhzClockMode = true.\n ");
                rc = VerifyDisp(SetModeDisplay,
                                largerMode.width,
                                largerMode.height,
                                largerMode.depth,
                                largerMode.refreshRate,
                                thisHead,
                                sorNum,
                                true,               // gt340MhzClockMode
                                true);             // lte340MhzScrambling
                if (rc != OK)
                {
                    Printf(Tee::PriHigh, "***** Case 7 failed: RC= %d.\n ",(UINT32)rc);
                    return rc;
                }
            }

            // Case 2: small res with scrambling test.
            Printf(Tee::PriHigh, "***** Verifying Case 2: PCLK =  Low, lte340MhzScrambling = true, gt340MhzClockMode = false.\n ");
            rc = VerifyDisp(SetModeDisplay,
                        smallerMode.width,
                        smallerMode.height,
                        smallerMode.depth,
                        smallerMode.refreshRate,
                        thisHead,
                        sorNum,
                        false,              // gt340MhzClockMode
                        true);             // lte340MhzScrambling

            if (rc != OK)
            {
                Printf(Tee::PriHigh, "***** Case 2 failed: RC= %d.\n ",(UINT32)rc);
                return rc;
            }

            // Case 0: smaller mode without scrambling test
            Printf(Tee::PriHigh, "***** Verifying Case 0: PCLK =  Low, lte340MhzScrambling = false, gt340MhzClockMode = false.\n ");
            rc = VerifyDisp(SetModeDisplay,
                        smallerMode.width,
                        smallerMode.height,
                        smallerMode.depth,
                        smallerMode.refreshRate,
                        thisHead,
                        sorNum,
                        false,              // gt340MhzClockMode
                        false);            // lte340MhzScrambling
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "***** Case 0 failed: RC= %d.\n ",(UINT32)rc);
                return rc;
            }

            // Case 3: smaller mode without scrambling test but ClkGte340 set
            Printf(Tee::PriHigh, "***** Verifying Case 3: PCLK =  Low, lte340MhzScrambling = true, gt340MhzClockMode = true.\n ");
            rc = VerifyDisp(SetModeDisplay,
                        smallerMode.width,
                        smallerMode.height,
                        smallerMode.depth,
                        smallerMode.refreshRate,
                        thisHead,
                        sorNum,
                        true,              // gt340MhzClockMode
                        true);            // lte340MhzScrambling
            if (rc != OK)
            {
                Printf(Tee::PriHigh, "***** Case 3 failed: RC= %d.\n ",(UINT32)rc);
                return rc;
            }

            if (m_WithAllHeads)
            {
                HeadNum ++;
            }
            else
            {
               break;
            }
        }

        // You should not try with different SORs if the display is LVDS (as LVDS is upported only on SOR0)
        // Also, if the given display is a CRT then no point of verifying on various SORs, it'll make use of DAC.
        if (m_withAllSors)
        {
            sorNum ++;
        }
        else
        {
             break;
        }
    }

    return rc;
}

//! ReadWriteI2cRegister
//!     Reads & Writes Display's DDC registers
//! dispId       : display whose register is to be read.
//! I2CAddress   : I2CAddress of the device.
//! I2CRegisterOFfset : offset of register to be read.
//! pReadData    : pointer specifying data to be written or stores data read from register.
//! bReadWrite   : false = read; true = write
RC Hdmi2::ReadWriteI2cRegister(LwU32 I2CPort,
                              UINT32 I2CAddress,
                              UINT32 I2CRegisterOFfset,
                              UINT32 *pReadData,
                              bool bReadWrite)
{
    RC rc;
    UINT32 subdev=0;
    GpuSubdevice *pSubdev = GetBoundGpuDevice()->GetSubdevice(subdev);

    I2c::Dev i2cDev = pSubdev->GetInterface<I2c>()->I2cDev(I2CPort, I2CAddress);

    if (bReadWrite)
    {
        rc = i2cDev.Write(I2CRegisterOFfset, *pReadData);
    }
    else
    {
        *pReadData = 0;
        rc = i2cDev.Read(I2CRegisterOFfset, pReadData);
    }

    Printf(Tee::PriHigh,
               "%s: I2C Operation:%s, [DDCPort] = %d, [I2CAddress] = %d, [I2CRegisterOFfset] = %d, Data = %0x, RC= %d\n",
                __FUNCTION__, bReadWrite ? "Write" : "Read",  I2CPort, I2CAddress, I2CRegisterOFfset,
                *pReadData, (UINT32)rc);

    return rc;
}

RC Hdmi2::VerifyDisp(DisplayID SetModeDisplay,
                     UINT32 width,
                     UINT32 height,
                     UINT32 depth,
                     UINT32 refreshRate,
                     UINT32 thisHead,
                     UINT32 sorNum,
                     bool gt340ClockSupported,
                     bool lte340ScramblingSupp)
{
    RC rc = OK;
    Surface2D CoreImage;

    bool bLwstomTimings = false;
    if (bLwstomTimings)
    {
       //
       // This code block is helpful incase if custom raster timings
       // needs to get inputted for debug, TBD Need to execute this
       // code by considering input from command line.
       //
        EvoCoreChnContext *pCoreDispContext = NULL;

        pCoreDispContext = (EvoCoreChnContext *)m_pDisplay->GetEvoDisplay()->
            GetEvoDisplayChnContext(SetModeDisplay, Display::CORE_CHANNEL_ID);

        EvoHeadSettings &HeadSettings = (pCoreDispContext->DispLwstomSettings.HeadSettings[thisHead]);
        EvoRasterSettings &lwstomRasterSettings = HeadSettings.RasterSettings;

        EvoRasterSettings RasterSettings;
        rc = m_pDisplay->SetupEvoLwstomTimings
            (
            SetModeDisplay,
            width,
            height,
            refreshRate,
            Display::AUTO,
            &RasterSettings,
            thisHead,
            0,
            NULL);

        lwstomRasterSettings.PixelClockHz = 594000000;
        lwstomRasterSettings.Dirty = true;
    }

    if (m_pixelclock)
    {
        EvoCoreChnContext *pCoreDispContext = NULL;
        pCoreDispContext = (EvoCoreChnContext *)m_pDisplay->GetEvoDisplay()->
                            GetEvoDisplayChnContext(SetModeDisplay, Display::CORE_CHANNEL_ID);

        EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

        Printf(Tee::PriHigh, "In %s() Using PixelClock = %d \n", __FUNCTION__, m_pixelclock);

        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.PixelClkFreqHz = m_pixelclock;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.Adj1000Div1001 = false;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.ClkMode = EvoPixelClockSettings::CLK_LWSTOM;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.NotDriver = false;
        LwstomSettings.HeadSettings[thisHead].PixelClkSettings.Dirty = true;
        LwstomSettings.HeadSettings[thisHead].Dirty = true;
    }

    if (!m_ignoreHdmi2LargerModes)
    {
        CHECK_RC(m_pDisplay->SetHdmi2_0Caps((UINT32)SetModeDisplay,
            true,
            gt340ClockSupported,
            lte340ScramblingSupp));
    }
    // Call IMP
    vector<Display::Mode> modes;
    vector<UINT32> overlayDepths;
    LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS IMPParams;

    memset(&IMPParams, 0, sizeof(LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS));

    modes.push_back(Display::Mode(width, height, depth, refreshRate));
    overlayDepths.push_back(0);

    rc = m_pDisplay->IMPQuery(DisplayIDs(1, SetModeDisplay),
                            modes,
                            overlayDepths,
                            &IMPParams);

    if(rc != OK)
    {
        if(rc == RC::MODE_IS_NOT_POSSIBLE)
        {
            Printf(Tee::PriHigh, "ERROR: %s: IMP Query Failed as MODE_IS_NOT_POSSIBLE. RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
        }
        else
        {
            Printf(Tee::PriHigh, "ERROR: %s: IMP Query Failed with RC =%d.\n ",
                __FUNCTION__, (UINT32)rc);
        }
        return rc;
    }

    // For any given resolution select image of that resolution
    // else select default image
    DTIUtils::ImageUtils imgArr = DTIUtils::ImageUtils::SelectImage(width, height);

    CHECK_RC(m_pDisplay->SetupChannelImage(SetModeDisplay,
                                                width,
                                                height,
                                                depth,
                                                Display::CORE_CHANNEL_ID,
                                                &CoreImage,
                                                imgArr.GetImageName(),
                                                0,/*color*/
                                                thisHead));

    CHECK_RC(m_pDisplay->SetMode(SetModeDisplay,
                                        width,
                                        height,
                                        depth,
                                        refreshRate,
                                        thisHead));

    if (!m_ignoreHdmi2LargerModes)
    {
        // Get the I2C port.
        LW0073_CTRL_SPECIFIC_GET_I2C_PORTID_PARAMS i2cPortParams = {0};
        CHECK_RC(m_pDisplay->GetDisplaySubdeviceInstance(&(i2cPortParams.subDeviceInstance)));
        i2cPortParams.displayId = (UINT32)SetModeDisplay;

        CHECK_RC(m_pDisplay->RmControl(LW0073_CTRL_CMD_SPECIFIC_GET_I2C_PORTID,
            &i2cPortParams, sizeof(i2cPortParams)));

        LwU32 I2CPort = i2cPortParams.ddcPortId -1;

        // Lets verify the scrambling state.
        rc = VerifyScramblingState(SetModeDisplay, I2CPort, lte340ScramblingSupp);
        if(rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: VerifyScramblingState() Failed with RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
            return rc;
        }

        // Lets verify the Clk mode state.
        rc = VerifyClkGte340State(SetModeDisplay, I2CPort, gt340ClockSupported);
        if(rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: VerifyClkGte340State() Failed with RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
            return rc;
        }
    }
    CHECK_RC(VerifyImage(SetModeDisplay, width, height, depth, refreshRate, &CoreImage));

    if (m_check_hdcp)
    {
        Printf(Tee::PriHigh, "\n ===== HDCP verification in progress ===> \n");
        rc = VerifyHDCP(SetModeDisplay);
        if(rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: VerifyHDCP Failed with RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
            return rc;
        }
    }

    CHECK_RC(VerifyImage(SetModeDisplay, width, height, depth, refreshRate, &CoreImage));

    //
    // Here compare CRC's which will let know whether HDCP working.
    //
    //

    rc = m_pDisplay->DetachDisplay(DisplayIDs(1, SetModeDisplay));
    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: DetachDisplay() Failed with RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
        return rc;
    }

    if(CoreImage.GetMemHandle() != 0)
        CoreImage.Free();

    return rc;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Hdmi2::Cleanup()
{
    return OK;
}

//! \brief sets hdmi 2.0 capabilities for this display id.
//------------------------------------------------------------------------------
RC Hdmi2::VerifyScramblingState(DisplayID dispId, LwU32 I2CPort, bool bEnable)
{
    RC rc = OK;
    GpuSubdevice *pSubdev =  GetDisplay()->GetOwningDisplaySubdevice();
    UINT32 hdmi2Ctrl = 0, sleepMs = 1;
    UINT32 orIndex = 0;
    UINT32 config = 0, status = 0, clkDtflags = 0;

    if (!m_isHdmi2_0Capable)
    {
        Printf(Tee::PriHigh, "%s: Scrambling feature is not supported on this GPU.\n",__FUNCTION__);
        return OK;
    }

    // sleep needed here as the status register in the i2c will update only after scrambled data is detected.
    if (GetBoundGpuSubdevice()->IsEmulation())
    {
        Printf(Tee::PriHigh, "%s: Sleeping for %d mSec / emu ptimer cycles.\n",__FUNCTION__, sleepMs);
        Sleep(sleepMs, GetBoundGpuSubdevice()->IsEmulation(), GetBoundGpuSubdevice());
    }

    CHECK_RC(m_pDisplay->GetORProtocol((UINT32)dispId, &orIndex, 0, 0));

    hdmi2Ctrl = pSubdev->RegRd32(LW_PDISP_SOR_HDMI2_CTRL(orIndex));
    Printf(Tee::PriHigh, "%s: LW_PDISP_SOR_HDMI2_CTRL[%d] = %X\n", __FUNCTION__, orIndex, hdmi2Ctrl);

    rc = ReadWriteI2cRegister(I2CPort, LW_HDMI_SCDC_SLAVE_ADR, LW_HDMI_SCDC_TMDS_CONFIGURATION, &config);
    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: I2C Read error while reading sink TMDS configuration state. RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
        return rc;
    }
    Printf(Tee::PriHigh, "%s: I2CRead () => LW_HDMI_SCDC_SLAVE_ADR [LW_HDMI_SCDC_TMDS_CONFIGURATION] = 0x%X.\n", __FUNCTION__, (UINT32) config);

    rc = ReadWriteI2cRegister(I2CPort, LW_HDMI_SCDC_SLAVE_ADR, LW_HDMI_SCDC_SCRAMBLER_STATUS, &status);
    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: I2C Read error while reading sink scrambling status. RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
        return rc;
    }
    Printf(Tee::PriHigh, "%s: I2CRead () => LW_HDMI_SCDC_SLAVE_ADR [LW_HDMI_SCDC_SCRAMBLER_STATUS] = 0x%X.\n", __FUNCTION__, (UINT32) status);

    rc = ReadWriteI2cRegister(I2CPort, LW_HDMI_SCDC_SLAVE_ADR, LW_HDMI_SCDC_STATUS_FLAGS_0, &clkDtflags);
    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: I2C Read error while reading sink clock detection status. RC =0x%X.\n",
                __FUNCTION__, (UINT32)rc);
        return rc;
    }

    Printf(Tee::PriHigh, "%s: I2CRead () => LW_HDMI_SCDC_SLAVE_ADR [LW_HDMI_SCDC_STATUS_FLAGS_0] = 0x%X\n", __FUNCTION__, (UINT32)clkDtflags);

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _SCRAMBLE, _ENABLE, hdmi2Ctrl) = 0x%X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _SCRAMBLE, _ENABLE, hdmi2Ctrl));

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _SCRAMBLE, _ENABLE, config) = 0x%X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _SCRAMBLE, _ENABLE, config));

    if (!((FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _SCRAMBLE, _ENABLE, hdmi2Ctrl) &&
         FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _SCRAMBLE, _ENABLE, config)) ||
        (FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _SCRAMBLE, _DISABLE, hdmi2Ctrl) &&
         FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _SCRAMBLE, _DISABLE, config))))
    {
        Printf(Tee::PriHigh, "%s:ERROR: Mismatch between the source and sink TMDS scrambling state.\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _SCRAMBLE, _ENABLE, hdmi2Ctrl) = 0x%X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _SCRAMBLE, _ENABLE, hdmi2Ctrl));

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_SCRAMBLER_STATUS, _SCRAMBLING, _DISABLED = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_SCRAMBLER_STATUS, _SCRAMBLING, _DISABLED, status));

    if (FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _SCRAMBLE, _ENABLE, hdmi2Ctrl) &&
        FLD_TEST_DRF(_HDMI, _SCDC_SCRAMBLER_STATUS, _SCRAMBLING, _DISABLED, status))
    {
        Printf(Tee::PriHigh, "%s: Sink device fails to enable scrambling as requested by source.\n", __FUNCTION__);
        MASSERT(0);
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! \brief verify hdmi 2.0 Clock mode capabilities for this display id.
//------------------------------------------------------------------------------
RC Hdmi2::VerifyClkGte340State(DisplayID dispId, LwU32 I2CPort, bool bEnable)
{
    RC rc;
    GpuSubdevice *pSubdev =  GetDisplay()->GetOwningDisplaySubdevice();
    UINT32 hdmi2Ctrl;
    UINT32 config = 0, status = 0, clkDtflags = 0;
    UINT32 orIndex = 0;

    if (!m_isHdmi2_0Capable)
    {
        Printf(Tee::PriHigh, "%s: Clock gte 340 feature is not supported on this GPU.\n",__FUNCTION__);
        return OK;
    }

    CHECK_RC(m_pDisplay->GetORProtocol((UINT32)dispId, &orIndex, 0, 0));

    hdmi2Ctrl = pSubdev->RegRd32(LW_PDISP_SOR_HDMI2_CTRL(orIndex));
    Printf(Tee::PriHigh, "LW_PDISP_SOR_HDMI2_CTRL %d\n", orIndex);

    rc = ReadWriteI2cRegister(I2CPort, LW_HDMI_SCDC_SLAVE_ADR, LW_HDMI_SCDC_TMDS_CONFIGURATION, &config);
    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: I2C Read error while reading sink TMDS configuration state. RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
        return rc;
    }
    Printf(Tee::PriHigh, "%s: I2CRead () => LW_HDMI_SCDC_SLAVE_ADR [LW_HDMI_SCDC_TMDS_CONFIGURATION] = %X\n",__FUNCTION__, (UINT32) config);

    rc = ReadWriteI2cRegister(I2CPort, LW_HDMI_SCDC_SLAVE_ADR, LW_HDMI_SCDC_SCRAMBLER_STATUS, &status);
    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: I2C Read error while reading sink scrambling status. RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
        return rc;
    }
    Printf(Tee::PriHigh, "%s: I2CRead () => LW_HDMI_SCDC_SLAVE_ADR [LW_HDMI_SCDC_SCRAMBLER_STATUS] = %X.\n", __FUNCTION__, (UINT32)status);

    rc = ReadWriteI2cRegister(I2CPort, LW_HDMI_SCDC_SLAVE_ADR, LW_HDMI_SCDC_STATUS_FLAGS_0, &clkDtflags);
    if(rc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: I2C Read error while reading sink clock detection status. RC =%d.\n",
                __FUNCTION__, (UINT32)rc);
        return rc;
    }
    Printf(Tee::PriHigh, "%s: I2CRead () => LW_HDMI_SCDC_SLAVE_ADR [LW_HDMI_SCDC_STATUS_FLAGS_0] = %X\n",__FUNCTION__, (UINT32) clkDtflags);

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _DIV_BY_4, hdmi2Ctrl) = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _DIV_BY_4, hdmi2Ctrl));

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _TMDS_BIT_CLOCK_RATIO, _ENABLE, config) = %X\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _TMDS_BIT_CLOCK_RATIO, _ENABLE, config));

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _NORMAL, hdmi2Ctrl) = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _NORMAL, hdmi2Ctrl));

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _TMDS_BIT_CLOCK_RATIO, _DISABLE, config) = %X\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _TMDS_BIT_CLOCK_RATIO, _DISABLE, config));

    if (!((FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _DIV_BY_4, hdmi2Ctrl) &&
                    FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _TMDS_BIT_CLOCK_RATIO, _ENABLE, config)) ||
                   (FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _NORMAL, hdmi2Ctrl) &&
                    FLD_TEST_DRF(_HDMI, _SCDC_TMDS_CONFIGURATION, _TMDS_BIT_CLOCK_RATIO, _DISABLE, config))))
    {
        Printf(Tee::PriHigh, "%s:ERROR: Mismatch between the source and sink TMDS clock mode state.\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _DIV_BY_4, hdmi2Ctrl) = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _DIV_BY_4, hdmi2Ctrl));

    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CLOCK, _DETECTED, _FALSE, clkDtflags) = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CLOCK, _DETECTED, _FALSE, clkDtflags));
    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH0, _LOCKED, _FALSE, clkDtflags) = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH0, _LOCKED, _FALSE, clkDtflags));
    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH1, _LOCKED, _FALSE, clkDtflags) = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH1, _LOCKED, _FALSE, clkDtflags));
    Printf(Tee::PriHigh, "%s: FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH2, _LOCKED, _FALSE, clkDtflags) = %X.\n",
        __FUNCTION__, (UINT32)FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH2, _LOCKED, _FALSE, clkDtflags));

    if (FLD_TEST_DRF(_PDISP, _SOR_HDMI2_CTRL, _CLOCK_MODE, _DIV_BY_4, hdmi2Ctrl) &&
        (FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CLOCK, _DETECTED, _FALSE, clkDtflags) ||
         FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH0, _LOCKED, _FALSE, clkDtflags) ||
         FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH1, _LOCKED, _FALSE, clkDtflags) ||
         FLD_TEST_DRF(_HDMI, _SCDC_STATUS_FLAGS_0_CH2, _LOCKED, _FALSE, clkDtflags)))
    {
        //
        // On samsung panel and emulation, sink state didn't get update as locked,
        // So ignore this error for now.
        Printf(Tee::PriHigh, "ERROR: Sink device is unable to lock to 1/4 clock.\n");
    }

    return rc;
}

//! \brief Helpler function to put the current thread to sleep for a given number(base on ptimer) of milliseconds,
//!         giving up its CPU time to other threads.
//!         Warning, this function is not for precise delays.
//! \param miliseconds [in ]: Milliseconds to sleep
//! \param runOnEmu [in]: It is on emulation or not
//! \param gpuSubdev [in]: Pointer to gpu subdevice
//!
void Hdmi2::Sleep(LwU32 miliseconds, LwBool runOnEmu, GpuSubdevice *gpuSubdev)
{
    if(runOnEmu)
        GpuUtility::SleepOnPTimer(miliseconds, gpuSubdev);
    else
        Tasker::Sleep(miliseconds);
}

RC Hdmi2::VerifyHDCP(DisplayID Display)
{
    RC rc;
    HDCPUtils hdcpUtils;

    CHECK_RC(hdcpUtils.Init(m_pDisplay, Display));

    CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::RENEGOTIATE_LINK));

    // Sleep(2000, GetBoundGpuSubdevice()->IsEmulation(), GetBoundGpuSubdevice());
    // Emulation sleep is taking more time
    Sleep(2000, false, GetBoundGpuSubdevice());

    bool            isHdcp1xEnabled = false;
    bool            isGpuCapable = false;
    bool            isHdcp22Capable = false;
    bool            isHdcp22Enabled = false;
    bool            isRepeater = false;
    bool            gethdcpStatusCached = true;

    CHECK_RC(hdcpUtils.GetHDCPInfo(Display, gethdcpStatusCached, isHdcp1xEnabled, isGpuCapable,
                isGpuCapable, isHdcp22Capable, isHdcp22Enabled, isRepeater));

    if (isHdcp1xEnabled || isHdcp22Enabled)
    {
        // Check for upstream
        CHECK_RC(hdcpUtils.PerformJob(HDCPUtils::READ_LINK_STATUS));
    }

    return rc;
}

RC Hdmi2::VerifyImage(DisplayID displayID, LwU32 width, LwU32 height, LwU32 depth, LwU32 refreshRate, Surface2D* CoreImage)
{
    RC rc;

    if (!m_manualVerif)
    {
        char        crcFileName[50];
        string      lwrProtocol = "";

        if (m_pDisplay->GetProtocolForDisplayId(displayID, lwrProtocol) != OK)
        {
            Printf(Tee::PriHigh, "\n Error in getting Protocol for DisplayId = 0x%08X",(UINT32)displayID);
            return RC::SOFTWARE_ERROR;
        }

        sprintf(crcFileName, "Hdmi2_%s_%dx%d.xml",lwrProtocol.c_str(), (int) width, (int) height);

        CHECK_RC(DTIUtils::VerifUtils::autoVerification(m_pDisplay,
            GetBoundGpuDevice(),
            displayID,
            width,
            height,
            depth,
            string("DTI/Golden/Hdmi2/"),
            crcFileName,
            CoreImage));
    }
    else
    {
        rc = DTIUtils::VerifUtils::manualVerification(m_pDisplay,
            displayID,
            CoreImage,
            "Is image OK?(y/n)",
            Display::Mode(width, height, depth, refreshRate),
            DTI_MANUALVERIF_TIMEOUT_MS,
            DTI_MANUALVERIF_SLEEP_MS);

        if (rc == RC::LWRM_INSUFFICIENT_RESOURCES)
        {
            Printf(Tee::PriHigh, "Manual verification failed with insufficient resources ignore it for now  please see bug 736351 \n");
            rc.Clear();
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
JS_CLASS_INHERIT(Hdmi2, RmTest,
    "HDMI ver2 test to cycle through the displays and verify scrambling and clock mode");

CLASS_PROP_READWRITE(Hdmi2,onlyConnectedDisplays,bool,
                     "run on only connected displays, default = 0");
CLASS_PROP_READWRITE(Hdmi2,manualVerif,bool,
                     "do manual verification, default = 0");
CLASS_PROP_READWRITE(Hdmi2, generate_gold, bool,
                     "generates gold files, default = 0");
CLASS_PROP_READWRITE(Hdmi2, protocol, string,
                     "OR protocol to use for the test");
CLASS_PROP_READWRITE(Hdmi2, pixelclock, UINT32,
                     "Desired pixel clock in Hz");
CLASS_PROP_READWRITE(Hdmi2, rastersize, string,
                     "Desired raster size (small/large)");
CLASS_PROP_READWRITE(Hdmi2, WithAllHeads, bool,
                     "Cycle through displays with all heads");
CLASS_PROP_READWRITE(Hdmi2, withAllSors, bool,
                     "Cycle through displays on all SORs");
CLASS_PROP_READWRITE(Hdmi2,check_hdcp,bool,
                    "verifies hdcp on given monitor");
