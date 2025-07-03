/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2016,2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//!
//! \file rmt_rigorousModesetTest.cpp
//! \brief IMPtest to cycle through all resolution provided in the input config file
//! \and check for major IMP features functioning
//!
#include "rmt_rigorousModesetTest.h"
#include "core/include/utility.h"
#include "gpu/tests/rm/utility/crccomparison.h"
#include "core/include/modsedid.h"
#include "ctrl/ctrl5070.h"
#include "kepler/gk104/dev_fbpa.h"
#include <string>
#include "core/include/platform.h"
#include "gpu/tests/rm/dispstatecheck/dispstate.h"
#include "pgstats.h"
#include "gpu/utility/rmclkutil.h"
#include <algorithm>
#include <time.h>
#ifdef INCLUDE_MDIAG
    #include "core/include/mrmtestinfra.h"
#endif

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    #include "gpu/display/win32disp.h"
    #include "gpu/display/win32cctx.h"
    #include "Windows.h"
    #include "lwapi.h"
#endif
#ifndef INCLUDED_MODESET_UTILS_H
#include "gpu/display/modeset_utils.h"
#endif

#include "core/include/memcheck.h"

string allKnownResStr = "640x480@60, 800x600@60, 1024x768@60, 1280x1024@60, 1280x800@60,"
                         "1280x960@60, 1360x768@60, 1440x900@60, 1600x1200@60, 1920x1080@60,"
                         "1920x1200@60, 2560x1600@60";
bool appLaunchCreatedUnderflow = false;
bool appLaunchError = false;
bool appNotRun = false;
bool appRunCheck = false;
UINT32 baselineMClkUtil = 0;

using namespace DTIUtils;

#define BUFFER_SIZE_LWRRENT_DIR  256

//
// configNo is set to NO_CONFIG_NUMBER if we are not running configs specified
// in the config file.
//
#define NO_CONFIG_NUMBER    0xFFFF

// Number of loops to probe the ASR/MSCG efficiency.
#define ASR_MSCG_EFFICIENCY_CHECK_LOOPS 10

//
// Disable mempool compression may make the actual efficiency be very close to predicted one
// and sometimes be lower than predicted efficiency. So that we need this tolerance when checking
// MSCG efficiency.
//
#define MSCG_EFFICIENCY_TOLERANCE 10

//
// Whether mempool compression is disabled or not, MSCG efficiency sometimes report quite low
// compare to other samples during desktop idle. So far no clear reason about this situation,
// so just raise warning if failed cases within 2 out of 10 samples (ASR_MSCG_EFFICIENCY_CHECK_LOOPS)
//
#define MSCG_EFFICIENCY_CHECK_FAILED_TOLERANCE 2

//
// This macro is copied from "fermi/fermi_fb.h" to access register in different partition.
// Manually copy here for now since include "fermi/fermi_fb.h" will cause compile error
// when building winMods.
//
// The Fermi manuals define broadcast registers at 0x0010Fxxx and corresponding
// per-partition registers at 0x0011pxxx where p is the partition number.
// Since the manuals don't have an indexed symbol for the per-partition regs,
// we have this macro to colwert a broadcast or per-partition address into
// the corresponding per-partition address.
#define FB_PER_FBPA(reg,part) (((reg) & 0xfff00fff) | 0x00010000| ((part) << 12))

map<string,Application>     g_application;
map<string,ModeSettings>    g_modeSettings;
IMPMarketingParameters      g_IMPMarketingParameters;

//! Retrieves the application object from the pLwrrApplicationObj and
//! if it doesn't exist in g_application then it adds it there
//! and returns a reference for use to caller function..
//!
//! params pLwrrApplicationObj  [in]    : reference application object.
//! params retrievedAppObj      [out]   : retrieved application info from first parameter.
RC addAppObjHelper(JSObject        *pLwrrApplicationObj,
                   Application     &retrievedAppObj);

//! Retrieves the ModeSettings object from the pLwrrModeSettingsObj and
//! if it doesn't exist in g_modeSettings then it adds it there
//! and returns a reference for use to caller function.
//!
//! params pLwrrModeSettingsObj  [in]    : reference mode setting object.
//! params retrievedModeSettings [out]   : retrieved mode settings from first parameter.
RC addModeStgsObjHelper(JSObject            *pLwrrModeSettingsObj,
                        ModeSettings        &retrievedModeSettings);

RC HandleAero(bool bHandleAero);

//! LaunchApplication()
//! Helper function used to launch each process in separate threads and kill it after specified seconds
//! params app2Launch [in]    : application info for launching.
void    LaunchApplication(void *app2Launch);

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

    //! LaunchApplicationStartProcess()
    //! Helper function used to start process used in LaunchApplication()
    //!
    //! params launchAppParams [in]    : application parameters for launching it.
    //! params pProcessHandle  [out]   : process handle of new started process.
    static RC LaunchApplicationStartProcess(LaunchApplicationParams *launchAppParams,
                                            HANDLE* pProcessHandle);

    //! LaunchApplicationStopProcess()
    //! Helper function used to stop process used in LaunchApplication()
    //!
    //! params processName [in]     : process name string used for output log.
    //! params hProcess    [in]     : target process handle to be closed.
    static void LaunchApplicationStopProcess(string processName,
                                             HANDLE hProcess);

#endif

//! function GetDisplayStartPosAndSize()
//!          This function returns the startX, startY position of the displayID
//! along with the current mode on it
//! params displayId [in]       : displayID whose details are to be retreived.
//! params startXPos [out]      : starting X co-ordinate of this Display
//! params startYPos [out]      : starting Y co-ordinate of this Display
//! params width     [out]      : width  of mode on this Display
//! params height    [out]      : height of mode on this Display
RC  Modeset::GetDisplayStartPosAndSize(DisplayID displayId,
                                       LwS32 *startXPos, LwS32 *startYPos,
                                       LwU32 *width, LwU32 *height)
{
    RC rc;

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    UINT32 lwrPathCount                         = 0;
    LW_DISPLAYCONFIG_PATH_INFO *pLwrPathInfo    = NULL;
    LwAPI_Status lwApiStatus;

    // Get and save our gpu ID
    UINT32 ourGpuId = m_pDisplay->GetOwningDisplaySubdevice()->GetGpuId();

    // Initialize LwApi
    CHECK_RC(m_pDisplay->GetWin32Display()->InitializeLwApi());

    // Now, first get the current display arrangement
    CHECK_RC(m_pDisplay->GetWin32Display()->AllocateAndGetDisplayConfig(
                                &lwrPathCount,
                                (void **)&pLwrPathInfo));

    for (UINT32 lwrPathLoopCount = 0; lwrPathLoopCount < lwrPathCount; lwrPathLoopCount++)
    {
        // Scan through all targets and find which ones will remain active
        for (UINT32 lwrTargetLoopCount = 0;
            lwrTargetLoopCount < pLwrPathInfo[lwrPathLoopCount].targetInfoCount;
            lwrTargetLoopCount++)
        {
            UINT32 thisDisplayOutputID = 0;
            UINT32 gpuIdThisDisplay    = 0;
            LwPhysicalGpuHandle gpuHandleThisDisplay;

            // Get the display mask of this display from the lwapi version of its displayId.
            lwApiStatus = LwAPI_SYS_GetGpuAndOutputIdFromDisplayId(
                              pLwrPathInfo[lwrPathLoopCount].targetInfo[lwrTargetLoopCount].displayId,
                             &gpuHandleThisDisplay,
                             &thisDisplayOutputID);
            if (lwApiStatus != LWAPI_OK)
            {
                Printf(Tee::PriNormal,
                    "\n %s: LwAPI_SYS_GetGpuAndOutputIdFromDisplayId() failed\n",
                    __FUNCTION__);
                rc = RC::SOFTWARE_ERROR;
                goto Cleanup;
            }

            // Get the gpuId of this display
            lwApiStatus = LwAPI_GetGPUIDfromPhysicalGPU(gpuHandleThisDisplay, &gpuIdThisDisplay);
            if (lwApiStatus != LWAPI_OK)
            {
                Printf(Tee::PriNormal, "\n %s: LwAPI_GetGPUIDfromPhysicalGPU() failed.",
                    __FUNCTION__);
                rc = RC::SOFTWARE_ERROR;
                goto Cleanup;
            }

            //
            //  Check if we have got the display we wanted to retrieve settings
            //  Condition 1: This display's GPU ID should match with our class object instance's GPU ID
            //  Condition 2: This displayID for this targetInfo must match displayID passed by user
            //
            bool foundDisplay = ((ourGpuId == gpuIdThisDisplay ) &&
                                 (thisDisplayOutputID == static_cast<UINT32>(displayId))
                                );

            // We have to found displayId whose properties we wanted to retrieve
            if (foundDisplay)
            {
                *width      = pLwrPathInfo[lwrPathLoopCount].sourceModeInfo->resolution.width;
                *height     = pLwrPathInfo[lwrPathLoopCount].sourceModeInfo->resolution.height;
                *startXPos  = pLwrPathInfo[lwrPathLoopCount].sourceModeInfo->position.x;
                *startYPos  = pLwrPathInfo[lwrPathLoopCount].sourceModeInfo->position.y;
                goto Cleanup;
            }
        }
    }

    *width      = 0;
    *height     = 0;
    *startXPos  = 0;
    *startYPos  = 0;

    m_reportStr = Utility::StrPrintf(
        "\n INFO: %s: DisplayID %X is not active. RC = NO_DISPLAY_DEVICE_CONNECTED",
        __FUNCTION__, (UINT32)displayId);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    rc = RC::NO_DISPLAY_DEVICE_CONNECTED;
Cleanup:
    Win32Display::DeAllocateDisplayConfig(lwrPathCount, (void **)&pLwrPathInfo);
#else
    Printf(Tee::PriNormal, "%s: is supported only on winmods platform", __FUNCTION__);
#endif
    return rc;
}

//! \brief Modeset constructor
//!
//! Sets the name of the test
//!
//------------------------------------------------------------------------------
Modeset::Modeset() :
    m_appName3DMarkV("3DMarkVantageCmd.exe"),
    m_appConfigFile3DMarkV("dispinfradata/imptest3/configs/IMPTest_3DMarkVantageConfig_Template.txt")
{
    SetName("Modeset");
    m_onlyConnectedDisplays = false;
    m_SwitchPStates = true;
    m_IgnoreUnderflow = false;
    m_manualVerif = false;
    m_enableASRTest = false;
    m_enableMSCGTest = false;
    m_enableImmedFlipTest = false;
    m_mempoolCompressionTristate = TRISTATE_DEFAULT;
    m_minMempoolTristate = TRISTATE_DEFAULT;
    m_immedFlipTestCheckMSCGSecs = 60;
    m_protocol = "CRT,TV,LVDS_LWSTOM,SINGLE_TMDS_A,SINGLE_TMDS_B,DUAL_TMDS,DP_A,DP_B,EXT_TMDS_ENC";
    m_repro = "";
    m_runfrominputfile = false;
    m_inputconfigfile  = "";
    m_runOnlyConfigs = "";
    m_pixelclock = 0;
    m_pstateSwitchCount = 10;
    m_useCRTFakeEdid = "";
    m_useDFPFakeEdid = "";
    // Note this is EDID v1.4 with DisplayID v1.3 based extension block.
    // For now we hardcode EDID name here and
    // logic for EDID auto generate will concentrate only on Edid v1.4.
    m_referenceEdidFileName_DFP = "DFP_LWD_5kx3k_x4.txt";
    m_autoGenerateEdid = false;
    m_isoFbLatencyTest = 0;
    m_freqTolerance100x = FREQ_TOLERANCE_100X;
    m_ignoreClockDiffError = false;
    m_disableRandomPStateTest = false;
    m_clockDiffFound = false;
    m_useWindowsResList = false;
    m_forceLargeLwrsor = false;
    m_forcePStateBeforeModeset = false;
    if (m_useAppRunCheck)
        appRunCheck = true;
    m_runOnEmu = false;
    m_PStateSupport = true;
    if (GetBoundGpuSubdevice()->GetPerf()->IsPState30Supported())
    {
        m_bIsPS30Supported = true;
        m_PerfStatePrint = "vP-State";
        m_PerfStatePrintHelper = "IMP vP";
        m_lwrrImpMinPState = 256;
    }
    else
    {
        m_bIsPS30Supported = false;
        m_PerfStatePrint = "PState";
        m_PerfStatePrintHelper = "P";
        m_lwrrImpMinPState = LW2080_CTRL_PERF_PSTATES_P0;
    }
    m_bMclkSwitchPossible       = false;
    m_bOnetimeSetupDone         = false;
    m_bModesetEventSent         = false;
    m_bClockSwitchEventSent     = false;
    m_bClockSwitchDoneEventSent = false;
    m_TraceDoneEventWaitCount   = 0;

    m_pDisplay = GetDisplay();
    m_numHeads = m_pDisplay->GetHeadCount();
    m_channelImage = new Surface2D[(Display::NUM_CHANNEL_IDS - 1) * m_numHeads];
    m_pLwrsChnImg = NULL;
    m_pOvlyChnImg = NULL;

    char lwrrTime[30] = "";
    DTIUtils::ReportUtils::GetLwrrTime("%Y-%m-%d-%H-%M-%S", lwrrTime, sizeof(lwrrTime));

    m_testStartTime = string(lwrrTime);
    m_CSVFileName = "IMPTest_" + m_testStartTime + "_CSV.csv";
    m_ReportFileName = "IMPTest_" + m_testStartTime + "_Report.log";
    m_pCSVFile = NULL ;
    m_pReportFile = NULL;
}

//! \brief Modeset destructor
//!
//! deallocates the memory before leaving the test
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
Modeset::~Modeset()
{
    UINT32 count = 0;
    UINT32 totalSurf2DObj = (Display::NUM_CHANNEL_IDS - 1) * m_numHeads;

    // Freeing m_channelImage & m_pOvlyChnImg.
    for(count = 0; count < totalSurf2DObj; count++)
    {
        if(m_channelImage[count].GetMemHandle() != 0)
        {
            m_channelImage[count].Free();
        }

        if (count < m_numHeads && m_pOvlyChnImg &&
            m_pOvlyChnImg[count].GetMemHandle() != 0)
        {
            m_pOvlyChnImg[count].Free();
        }
    }

    // Freeing m_pLwrsChnImg.
    if (m_pLwrsChnImg &&
        m_pLwrsChnImg[0].GetMemHandle() != 0)
    {
        m_pLwrsChnImg[0].Free();
    }

    delete []m_pLwrsChnImg;
    delete []m_pOvlyChnImg;
    delete []m_channelImage;
    m_channelImage = NULL;
}

//! \brief runReproConfig
//!
//! This method reproduces the config provided to it as input
//! It sets Image / removes image from a given Channel. It also detaches display.
//!
//! \sa getConfigFromString()
RC Modeset::runReproConfig(SetImageConfig *pReproConfig)
{
    RC rc;
    ImageUtils imageArray;
    vector<SetImageConfig>  setImageConfig;
    string                  reportString;
    EvoDisplay *pDisplay = GetDisplay()->GetEvoDisplay();

    if (pReproConfig->head == Display::ILWALID_HEAD)
    {
        if (pDisplay->GetHeadByDisplayID(pReproConfig->displayId, &pReproConfig->head) == RC::ILWALID_HEAD)
        {
            m_reportStr = Utility::StrPrintf(
                    "%s: Failed to getHead for displayID = %x. RC = %d\n",
                    __FUNCTION__, (UINT32)pReproConfig->displayId, (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::ILWALID_HEAD;
        }
    }

    // Remove Image from channel / Detach Display
    if((pReproConfig->mode.width == static_cast<UINT32>(-1) ) || (pReproConfig->mode.height ==  static_cast<UINT32>(-1) ))
    {
        CHECK_RC(pDisplay->SetImage(pReproConfig->displayId, NULL, pReproConfig->channelId));
        if (m_channelImage[pReproConfig->channelId * m_numHeads + pReproConfig->head].GetMemHandle())
            m_channelImage[pReproConfig->channelId * m_numHeads + pReproConfig->head].Free();

        // Detach Display if its Core Channel
        if(pReproConfig->displayId == Display::CORE_CHANNEL_ID)
        {
            rc = pDisplay->DetachDisplay(DisplayIDs(1, pReproConfig->displayId));
            m_reportStr = Utility::StrPrintf("Detaching DisplayID 0x%08X ",
                                             (UINT32)pReproConfig->displayId);

            setImageConfig.push_back(SetImageConfig(pReproConfig->displayId,
                                      Display::Mode(pReproConfig->mode.width, pReproConfig->mode.height, pReproConfig->mode.depth, pReproConfig->mode.refreshRate),
                                      pReproConfig->head,
                                      Display::CORE_CHANNEL_ID));

            WriteCSVAndReport(setImageConfig, ColorUtils::LWFMT_NONE,
                            true, rc == OK, m_reportStr.c_str(), reportString);
        }
        return rc;
    }

    //Setting image on channel
    m_reportStr = Utility::StrPrintf("Setting up image of res: %d x %d\n",
            pReproConfig->mode.width, pReproConfig->mode.height);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    imageArray = ImageUtils::SelectImage(pReproConfig->mode.width, pReproConfig->mode.height);

    if(m_channelImage[pReproConfig->channelId * m_numHeads + pReproConfig->head].GetMemHandle() != 0)
    {
        m_channelImage[pReproConfig->channelId * m_numHeads + pReproConfig->head].Free();
    }

    rc = pDisplay->SetupChannelImage(pReproConfig->displayId,
                    pReproConfig->mode.width,
                    pReproConfig->mode.height,
                    pReproConfig->mode.depth,
                    pReproConfig->channelId,
                    &m_channelImage[pReproConfig->channelId * m_numHeads + pReproConfig->head],
                    imageArray.GetImageName(),
                    0,
                    pReproConfig->head);

    if (rc != OK )
    {
        m_reportStr = Utility::StrPrintf(
                "%s: SetupChannelImage() Failed. RC = %d\n ",
                __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    if (pReproConfig->channelId == Display::CORE_CHANNEL_ID)
    {
        rc = pDisplay->SetMode(pReproConfig->displayId,
             pReproConfig->mode.width,
             pReproConfig->mode.height,
             pReproConfig->mode.depth,
             pReproConfig->mode.refreshRate,
             pReproConfig->head);
        m_reportStr = Utility::StrPrintf(
            "%s: SetMode(). Width= %d, Height= %d, depth=%d, refresh Rate = %d\n ",
            __FUNCTION__, pReproConfig->mode.width, pReproConfig->mode.height,
            pReproConfig->mode.depth, pReproConfig->mode.refreshRate);
    }
    else
    {
        rc = pDisplay->SetImage(pReproConfig->displayId,
                &m_channelImage[pReproConfig->channelId * m_numHeads + pReproConfig->head],
                pReproConfig->channelId);
        m_reportStr = Utility::StrPrintf(
                    "%s: SetImage() on channel %d. Width= %d, Height= %d, depth=%d, refresh Rate = %d\n ",
                    __FUNCTION__, pReproConfig->channelId, pReproConfig->mode.width, pReproConfig->mode.height,
                    pReproConfig->mode.depth, pReproConfig->mode.refreshRate);
    }

    setImageConfig.clear();
    setImageConfig.push_back(SetImageConfig(pReproConfig->displayId,
        Display::Mode(pReproConfig->mode.width, pReproConfig->mode.height, pReproConfig->mode.depth, pReproConfig->mode.refreshRate),
        pReproConfig->head,
        pReproConfig->channelId));

    WriteCSVAndReport(setImageConfig, ColorUtils::LWFMT_NONE,
                          true, rc == OK, m_reportStr.c_str(), reportString);

    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: SetMode() / SetImage() Failed. RC = %d\n ",
                __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    return rc;
}

//! \brief getConfigFromString
//!
//! This method validates the config string passed to it as the input
//! and populates the correesponding values in "lwrrReproConfig"
//! If input string has any invalid data then it throws RC::LWRM_ILWALID_ARGUMENT error
//!
//! \sa runReproConfig()
RC Modeset::getConfigFromString(string reproSring, SetImageConfig *lwrrReproConfig)
{
    RC rc;
    vector <string> configFields;
    vector <string> fieldTokens;
    configFields = DTIUtils::VerifUtils::Tokenize(reproSring, ",");

    for (UINT32 i = 0; i < configFields.size(); i++)
    {

        configFields[i] = DTIUtils::Mislwtils::trim(configFields[i]);
        fieldTokens = DTIUtils::VerifUtils::Tokenize(configFields[i], "=");

        if(fieldTokens.size() != 2)
        {
            m_reportStr = Utility::StrPrintf("%s: Invalid commandline. Each -repro field must be of form \"fieldName1=Value1,fieldName2=Value2;\"\n ", __FUNCTION__ );
            PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::LWRM_ILWALID_ARGUMENT;
        }

        fieldTokens[0] = DTIUtils::Mislwtils::trim(fieldTokens[0]);
        fieldTokens[1] = DTIUtils::Mislwtils::trim(fieldTokens[1]);

        // Compare for valid text fields
        if(fieldTokens[0].compare("DISPLAYID") == 0)
        {
            lwrrReproConfig->displayId = strtoul(fieldTokens[1].c_str(),NULL,16);
        }
        else if(fieldTokens[0].compare("WIDTH") == 0)
        {
            lwrrReproConfig->mode.width = strtoul(fieldTokens[1].c_str(),NULL,0);
        }
        else if(fieldTokens[0].compare("HEIGHT") == 0)
        {
            lwrrReproConfig->mode.height = strtoul(fieldTokens[1].c_str(),NULL,0);
        }
        else if(fieldTokens[0].compare("REFRESHRATE") == 0)
        {
            lwrrReproConfig->mode.refreshRate = strtoul(fieldTokens[1].c_str(),NULL,0);
        }
        else if(fieldTokens[0].compare("DEPTH") == 0)
        {
            lwrrReproConfig->mode.depth = strtoul(fieldTokens[1].c_str(),NULL,0);
        }
        else if(fieldTokens[0].compare("HEAD") == 0)
        {
            UINT32 head = strtoul(fieldTokens[1].c_str(),NULL,0);
            lwrrReproConfig->head = ( head < GetDisplay()->GetHeadCount()) ? head : (UINT32)Display::ILWALID_HEAD;
        }
        else if(fieldTokens[0].compare("CHANNELID") == 0)
        {
            if(fieldTokens[1].compare("CORE") == 0)
            {
                lwrrReproConfig->channelId = Display::CORE_CHANNEL_ID;
            }
            else if(fieldTokens[1].compare("BASE") == 0)
            {
                lwrrReproConfig->channelId = Display::BASE_CHANNEL_ID;
            }
            else if(fieldTokens[1].compare("OVERLAY") == 0)
            {
                lwrrReproConfig->channelId = Display::OVERLAY_CHANNEL_ID;
            }
            else if(fieldTokens[1].compare("CURSOR") == 0)
            {
                lwrrReproConfig->channelId = Display::LWRSOR_IMM_CHANNEL_ID;
            }
            else
            {
                m_reportStr = Utility::StrPrintf("%s: Invalid commandline. \"CHANNELID\" must have value \"CORE/BASE/OVERLAY/CURSOR\"\n", __FUNCTION__ );
                PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return RC::LWRM_ILWALID_ARGUMENT;
            }
        }
        else
        {
            m_reportStr = Utility::StrPrintf("%s: Invalid commandline. Field before = must be \"DISPLAYID / WIDTH / HEIGHT / REFRESHRATE / DEPTH / HEAD / CHANNELID\"\n", __FUNCTION__ );
            PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::LWRM_ILWALID_ARGUMENT;
        }
    }

    return rc;
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         other string for failed reason otherwise
//! The test has to run of GF11X chips but can run on all elwironments
//------------------------------------------------------------------------------
string Modeset::IsTestSupported()
{
    RC rc;
    FermiPStateSwitchTest       ObjSwitchRandomPState;
    vector<LwU32> supportedPerfStates;
    if (m_enableMSCGTest)
    {
        if (m_bIsPS30Supported)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: MSCG test is lwrrently not supported on "
                    "PState 3.0\n",
                    __FUNCTION__);
            Printf(Tee::PriNormal, "%s", m_reportStr.c_str());
            return m_reportStr;
        }
    }

    rc = ObjSwitchRandomPState.Setup();
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: ObjSwitchRandomPState.Setup() failed with RC = %d\n",
                __FUNCTION__, (UINT32)rc);
        Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

        ObjSwitchRandomPState.Cleanup();
        return m_reportStr;
    }

    if (!m_bIsPS30Supported)
    {
        LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS PStatesInfoParams = {0};
        rc = ObjSwitchRandomPState.GetSupportedPStatesInfo(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
            PStatesInfoParams,
            supportedPerfStates);

        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetSupportedPStatesInfo failed with RC = %d\n",
                    __FUNCTION__, (UINT32)rc);
            Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

            ObjSwitchRandomPState.Cleanup();
            return m_reportStr;
        }
    }
    else
    {
        LW2080_CTRL_PERF_VPSTATES_INFO virtualPStatesInfo;
        vector<LwU32> virtualPStates;
        rc = ObjSwitchRandomPState.GetSupportedIMPVPStatesInfo(
            pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
            &virtualPStatesInfo,
            &virtualPStates,
            &supportedPerfStates);

        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetSupportedIMPVPStatesInfo failed with RC ="
                    " %d\n", __FUNCTION__, (UINT32)rc);
            Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

            ObjSwitchRandomPState.Cleanup();
            return m_reportStr;
        }
    }

    ObjSwitchRandomPState.Cleanup();
    if (static_cast<UINT32>(supportedPerfStates.size()) < 1)
    {
        if (!m_disableRandomPStateTest)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Must set disableRandomPStateTest = true when "
                    "using non-pstate vbios. Exiting Test.\n",
                    __FUNCTION__);
            Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

            return m_reportStr;
        }

        m_PStateSupport = false;
        m_reportStr = Utility::StrPrintf(
                "Warning: %s: Current card does not support any PState. "
                "It will skip all the tests which require PState like "
                "ASR, MSCG and RandomPState test.\n",
                __FUNCTION__);
        Printf(Tee::PriNormal, "%s", m_reportStr.c_str());
    }
    if (GetDisplay()->GetIgnoreIMP())
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Ignore IMP = true. Exiting Test.\n",
                __FUNCTION__);
        Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

        return m_reportStr;
    }

    if (!GetDisplay()->IsIMPEnabled())
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: IMP is disabled. Exiting Test.\n",
                __FUNCTION__);
        Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

        return m_reportStr;
    }

    if (!checkAsrMscgTestVbiosSettings())
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Enable subtest failed due to VBIOS setting. "
                "Exiting Test.\n",
                __FUNCTION__);
        Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

        return m_reportStr;
    }

    if (m_enableImmedFlipTest && !check3DMarkVantageInstallation())
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Enable immediate flip test failed."
                "Please install 3DMarkVantage first. Exiting Test.\n",
                __FUNCTION__);
        Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

        return m_reportStr;
    }

    return RUN_RMTEST_TRUE;
}

RC addToKnownRes(string res, vector<resolution*> &knownRes)
{
    RC rc = OK;
    resolution *current = new resolution;
    size_t rpos = res.find("@"), xpos = 0;
    string sstr = res.substr(rpos+1, res.size());
    current->refreshRate = atoi(sstr.c_str());

    xpos = res.find("x");
    if(xpos && (xpos != string::npos))
    {
        sstr = res.substr(0, xpos);
        current->width= atoi(sstr.c_str());
    }

    sstr = res.substr(xpos + 1, rpos - xpos);
    current->height = atoi(sstr.c_str());
    knownRes.push_back(current);
    return rc;
}

RC Modeset::WriteCSVAndReport(vector<SetImageConfig> setImageConfig, ColorUtils::Format colorFormat,
                        bool bIMPpassed, bool bSetModeStatus, const string &comment, string &reportString)
{
    RC rc;

    // Let's get formatted string to print to .csv and .log
    CHECK_RC(DTIUtils::ReportUtils::CreateCSVReportString(m_pDisplay, setImageConfig, colorFormat,
                        bIMPpassed, bSetModeStatus, comment, reportString));

    // write to .csv file
    CHECK_RC(DTIUtils::ReportUtils::write2File(m_pCSVFile,reportString));

    //write to stdout and .log file
    PRINT_STDOUT_N_REPORT(Tee::PriHigh, reportString.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    return rc;
}

RC Modeset::populateKnownRes(string allRes, vector<resolution*>&knownRes)
{
    RC rc;
    size_t position, ppos = 0;
    position = allRes.find(",", ppos+1);
    while(position!=string::npos)
    {
        CHECK_RC(addToKnownRes(allRes.substr(ppos, position - ppos), knownRes));
        ppos= position+1;
        position = allRes.find(",", ppos);
    }
    CHECK_RC(addToKnownRes(allRes.substr(ppos, allRes.size()), knownRes));
    return OK;
}

//!
//! \brief Sets a mode, runs applications/stress test, and checks for underflow.
//!
//! \param configNo       : Specifies which config from the config file we are
//!                         testing; can be set to NO_CONFIG_NUMBER if the mode
//!                         is not specified in a config file
//! \param pDisplayIds    : The display IDs for all displays that will be active
//!                         after the modeset (including displays that were
//!                         active before, and that are remaining active)
//! \param pDisplayModes  : Describes the modes for all displays that will be
//!                         active after the modeset (including any displays
//!                         that were active before, that are not changing)
//! \param pOverlayDepth  : Overlay Pixel Depth vector, used for the IMP check;
//!                         note that the overlay will not be activated by
//!                         ImpTest3's modeset, but rather by applications that
//!                         ImpTest3 ilwokes that make use of overlays
//! \param pModesetDisplayIds : Specifies the IDs for all displays whose heads
//!                         will have a modeset performed
//! \param pModesToSet    : Describes the modes on heads that are changing
//! \param pModesetHeads  : Specifies the head number for all heads that are
//!                         having a modeset performed
//! \param viewMode       : Specifies how the desktop should be arranged across
//!                         multiple displays (may not be applicable for non-
//!                         Windows OS's)
//! \param porPState      : PState that the mode must be able to operate at;
//!                         a failure is flagged if IMP says the mode is not
//!                         possible at that PState; can be set to
//!                         LW2080_CTRL_PERF_PSTATES_UNDEFINED if none is to be
//!                         specified
//! \param porVPState     : vP-State that the mode must be able to operate at;
//!                         a failure if flagged if IMP says the mode is not
//!                         possible at the vP-State; can be set to
//!                         IMPTEST3_VPSTATE_UNDEFINED
//! \param uiDisplay      : Indexes the display to be used for manual test
//!                         output
//! \param pUiSurface     : Surface to be used to display manual test messages
//! \param pDispHeadMapParams : Routing information that allows the head to be
//!                         determined from the display ID
//!
//! \return RC status
//!
RC Modeset::TestDisplayConfig
(
    UINT32                      configNo,
    const DisplayIDs            *pDisplayIds,
    const vector<Display::Mode> *pDisplayModes,
    const vector<UINT32>        *pOverlayDepth,
    const DisplayIDs            *pModesetDisplayIds,
    const vector<Display::Mode> *pModesToSet,
    const vector<UINT32>        *pModesetHeads,
    Display::VIEW_MODE          viewMode,
    UINT32                      porPState,
    UINT32                      porVPState,
    UINT32                      uiDisplay,
    Surface2D                   *pUiSurface,
    const LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *pDispHeadMapParams
)
{
    StickyRC                    stickyRC;
    StickyRC                    cleanupRC;
    RC                          rc;
    LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS impParams;
    vector<Tasker::ThreadID>    threadIdAppLaunch;
    vector<LwU32>               supportedIMPvPStates;
    string                      commentStr;
    string                      errorStr = "";
    UINT32                      underflowHead;
    UINT32                      lwrX = 0;
    UINT32                      lwrY = 0;
    UINT32                      overlayCount;
#ifdef INCLUDE_MDIAG
    bool                        TestRunningOnMRMI = false;
    bool                        bMrmTracesStarted = false;
#endif

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    UINT32                      disp;
    LaunchApplicationParams     *launchAppParams = NULL;
    vector<LaunchApplicationParams *> ptrsLaunchAppParams;
    LW_GPU_DYNAMIC_PSTATES_INFO pstatesInfo = {0};
#else
    UINT32                      ovlyNo = 0;
    UINT32                      ovlyWidth = 0;
    UINT32                      ovlyHeight = 0;
    UINT32                      ovlyDepth = 0;
    ImageUtils                  oimageArray;
    m_pLwrsChnImg =             new Surface2D[1];
    m_pOvlyChnImg =             new Surface2D[m_numHeads];
#endif

    overlayCount = static_cast<UINT32>(pDisplayIds->size());
    if (NO_CONFIG_NUMBER != configNo)
    {
        if (overlayCount > g_IMPMarketingParameters.configList[configNo].overlayCount)
        {
            overlayCount = g_IMPMarketingParameters.configList[configNo].overlayCount;
        }
        if (overlayCount > g_IMPMarketingParameters.configList[configNo].activeDisplaysCount[0])
        {
            overlayCount = g_IMPMarketingParameters.configList[configNo].activeDisplaysCount[0];
        }
    }

    m_reportStr = "\n############################################\n";
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    memset(&impParams, 0, sizeof(LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS));
    rc = m_pDisplay->IMPQuery
        (
            *pDisplayIds,
            *pDisplayModes,
            *pOverlayDepth,
            &impParams
        );

    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: IMP Query Failed with RC =%d.\n ",
                __FUNCTION__, (UINT32)rc);

        // Print to CSV report
        if(rc == RC::MODE_IS_NOT_POSSIBLE)
        {
            commentStr = Utility::StrPrintf(
                    "MODE_IS_NOT_POSSIBLE,Not Attempted,FAIL,%s",
                    m_reportStr.c_str());
        }
        else
        {
            commentStr = Utility::StrPrintf(
                    "IMP_QUERY_FAILED_WITH_RC,Not Attempted,FAIL,%s",
                    m_reportStr.c_str());
        }
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, commentStr.c_str(), PRINT_CSV_FILE );
        goto Cleanup;
    }

    // m_lwrrMinPstate represents the min vP-State/PState supplied by IMP.
    if (!m_bIsPS30Supported)
    {
        m_lwrrImpMinPState = static_cast<UINT32>(impParams.MinPState);
        if (m_lwrrImpMinPState == LW2080_CTRL_PERF_PSTATES_UNDEFINED)
        {
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: IMP Query Passed.\n ", __FUNCTION__);
        }
        else
        {
            if (m_pDisplay->GetIgnoreIMP())
            {
                m_lwrrImpMinPState = LW2080_CTRL_PERF_PSTATES_P0;
            }

            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: MinPState = P%u   Extra margin = %d%% (%s)\n",
                    __FUNCTION__,
                    DTIUtils::Mislwtils::getLogBase2(m_lwrrImpMinPState),
                    (UINT32) (((UINT64) impParams.worstCaseMargin * 100) /
                              LW5070_CTRL_IMP_MARGIN_MULTIPLIER) - 100,
                    impParams.worstCaseDomain);
        }
    }
    else
    {
        supportedIMPvPStates = m_objSwitchRandomPState.getIMPVirtualPStates();
        // the min vP-State supported by IMP
        m_lwrrImpMinPState = static_cast<UINT32>(impParams.minPerfLevel);

        if (m_lwrrImpMinPState == LW2080_CTRL_PERF_VPSTATE_INDEX_ILWALID)
        {
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: %d: The Min vP-State fetched from the IMP Query"
                    " is not valid.\n ",
                    __FUNCTION__, __LINE__);
        }
        else
        {
            if (m_pDisplay->GetIgnoreIMP())
            {
                // Setting fastest IMP vP-State supported.
                m_lwrrImpMinPState = static_cast<UINT32>(supportedIMPvPStates.size() - 1);
            }

            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: IMP Query Passed.\n ", __FUNCTION__);
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: MilwPState = vP%u  Extra margin = %d%% (%s)\n",
                    __FUNCTION__,
                    m_lwrrImpMinPState,
                    (UINT32) (((UINT64) impParams.worstCaseMargin * 100) /
                          LW5070_CTRL_IMP_MARGIN_MULTIPLIER ) - 100,
                    impParams.worstCaseDomain);
        }
    }

    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    commentStr = "MODE_IS_POSSIBLE,";
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, commentStr.c_str(), PRINT_CSV_FILE );

    UINT32 porMinPState;
    if (porPState == (UINT32)MinPState)
    {
        porMinPState = m_lwrrImpMinPState;
    }
    else if (porPState == (UINT32)MaxPState)
    {
        porMinPState = static_cast<UINT32>(MaxPState);
    }
    else
    {
        porMinPState = porPState;
    }

    if (!m_PStateSupport)
    {
        if (porMinPState != LW2080_CTRL_PERF_PSTATES_UNDEFINED)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Specify POR min pstate - P%u in config file but system doesn't support PState. "
                    "Please specify 'PState.Undefined' in config file when using non-pstate vbios.\n",
                    __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(porMinPState));
            outputCommentStr("Not Attempted,FAIL,");

            rc = RC::LWRM_ILWALID_STATE;
            goto Cleanup;
        }

        if (porVPState != IMPTEST3_VPSTATE_UNDEFINED)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Specified POR min vP-State in config file "
                    "but the system doesn't support vP-States. "
                    "Set porVPState to IMPTEST3_VPSTATE_UNDEFINED (257) on "
                    "non pstate vbios\n",
                    __FUNCTION__);
            outputCommentStr("Not Attempted,FAIL,");

            rc = RC::LWRM_ILWALID_STATE;
            goto Cleanup;
        }
    }

    if (m_bIsPS30Supported)
    {
        if (porVPState == IMPTEST3_VPSTATE_MIN)
        {
            porMinPState = m_lwrrImpMinPState;
        }
        else if (porVPState == IMPTEST3_VPSTATE_MAX)
        {
            porMinPState = static_cast<UINT32>(supportedIMPvPStates.size() - 1);
        }
        else if (porVPState == IMPTEST3_VPSTATE_UNDEFINED)
        {
            //
            // TODO: Need to get the slowest vP-State nearest to the
            // PState if mentioned in the porPState. Lwrrently setting
            // the fastest IMP vP-State.
            //
            porMinPState = static_cast<UINT32>(supportedIMPvPStates.size() - 1);
        }
        else
        {
            //
            // TODO: Need to do this validation while
            // retrieving the config file.
            //
            if (porVPState < supportedIMPvPStates.size())
            {
                porMinPState = porVPState;
            }
            else
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Specified POR vP-State - vP%u in "
                        "config file, but system doesn't support the "
                        "vP-State. Please specify a valid IMP vP-State.\n",
                        __FUNCTION__, porVPState);
                outputCommentStr("Not Attempted,FAIL,");

                rc = RC::LWRM_ILWALID_STATE;
                goto Cleanup;
            }
        }
    }

    // ARCH wants us to force lock to the PORMinPState prior to the SetMode.
    // This if block takes care of it
    if (m_PStateSupport)
    {
        commentStr = Utility::StrPrintf(
            "\nINFO: %s: Checking if por(V)PState = %s specified "
            "in config file is possible for this config with IMP Min "
            "(V)PState = %s\n",
            __FUNCTION__,
            printPerfStateHelper(porMinPState).c_str(),
            printPerfStateHelper(m_lwrrImpMinPState).c_str());
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, commentStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        // if this mode is not possible @ Config.porMinPState specified
        // in config file then raise an ASSERT for ARCH.
        if ((porMinPState > m_lwrrImpMinPState && !m_bIsPS30Supported) ||
            (porMinPState < m_lwrrImpMinPState && m_bIsPS30Supported))
        {
            // Please specify 'PState.MinPState'/IMPTEST3_VPSTATE_MIN in
            // config file so test will automatically select appropriate
            // pstate based on RM IMP returned MinPstate.

            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: %s: Config's perf state = %s"
                    " in config file but RM IMP MinPstate = %s. "
                    "So can't support this config at Perf State.\n",
                    __FUNCTION__,
                    (m_bIsPS30Supported) ? "PState 3.0" : "PState 2.0",
                    printPerfStateHelper(porMinPState).c_str(),
                    printPerfStateHelper(m_lwrrImpMinPState).c_str());
            outputCommentStr("Not Attempted,FAIL,");

            //this debug assert is needed for ARCH team
            MASSERT(0);
            rc = RC::MODE_IS_NOT_POSSIBLE;
            goto Cleanup;
        }

        // Arch wants to lock pstate to Config.porMinPState
        // To be used only on EVO display and not on winmods.
        if (m_forcePStateBeforeModeset)
        {
            m_reportStr = Utility::StrPrintf(
                    "\nINFO: %s: Starting %s Transition to %s",
                    __FUNCTION__,
                    m_PerfStatePrint,
                    printPerfStateHelper(porMinPState).c_str());
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            if ((rc = ProgramAndVerifyPState(porMinPState)) != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s Transition to %s failed\n",
                        m_PerfStatePrint,
                        printPerfStateHelper(porMinPState).c_str());
                outputCommentStr("Not Attempted,FAIL,");

                //this debug assert is needed for ARCH team
                MASSERT(0);
                goto Cleanup;
            }
        }
    }

    Printf(Tee::PriHigh, "\nINFO: %s: Doing SetMode on DisplayIDs=\n", __FUNCTION__);
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, *pDisplayIds);

    // Do SetMode
    rc = m_pDisplay->SetMode(*pModesetDisplayIds,
                             *pModesetHeads,
                             *pModesToSet,
                             false,
                             viewMode);

    if (rc != OK)
    {
        if(rc == RC::ILWALID_DISPLAY_MASK)
        {
            m_reportStr = Utility::StrPrintf(
                    "SetMode not attempted due to invalid dispId combination. RC = %d\n",
                    (UINT32)rc);
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "SetMode Failed. RC = %d\n", (UINT32)rc);
        }

        outputCommentStr("FAIL,FAIL,");
        goto Cleanup;
    }

    m_reportStr = Utility::StrPrintf("%s: SetMode done",__FUNCTION__);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    commentStr = "PASS,";
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, commentStr.c_str(), PRINT_CSV_FILE );

    // Check Underflow
    if (!m_IgnoreUnderflow &&
        ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR))
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Display underflow was detected on head %u after SetMode\n",
                __FUNCTION__, underflowHead);

        outputCommentStr("FAIL,");
        goto Cleanup;
    }

    // Lets verify if the ARCH request PORPState is still existing.
    // This if block takes care of it
    if (m_PStateSupport)
    {
        if (!m_forcePStateBeforeModeset)
        {
            m_reportStr = Utility::StrPrintf(
                    "\nINFO: %s: Starting %s Transition to %s",
                    __FUNCTION__,
                    m_PerfStatePrint,
                    printPerfStateHelper(porMinPState).c_str());
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            if ((rc = ProgramAndVerifyPState(porMinPState)) != OK)
            {
                string reportStrTmp = " ";
                // PState switch fails due to underflow too. So let's check underflow status here.
                if (DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)
                    == RC::LWRM_ERROR)
                {
                    if (!m_IgnoreUnderflow)
                    {
                        rc = RC::LWRM_ERROR;
                        reportStrTmp = Utility::StrPrintf(
                                " Display underflow detected on head %u.",
                                underflowHead);
                    }
                }

                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s Transition to %s after SetMode() failed. %s\n",
                        m_PerfStatePrint,
                        printPerfStateHelper(porMinPState).c_str(),
                        reportStrTmp.c_str());
                outputCommentStr("FAIL,");

                goto Cleanup;
            }

            if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR)
            {
                if (!m_IgnoreUnderflow)
                {
                    m_reportStr = Utility::StrPrintf(
                            "ERROR: %s: Display underflow was detected on head %u before SetLwrsorPos\n",
                            __FUNCTION__, underflowHead);

                    outputCommentStr("FAIL,");
                    goto Cleanup;
                }
            }
        }
    }

    // Force large cursor.
    // Note: LwAPI for changing cursor settings needs to be ilwoked after SetMode is done
    if (m_forceLargeLwrsor)
    {
        rc = SetLargeLwrsor(pModesetDisplayIds, m_pLwrsChnImg);
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: SetLargeLwrsor() Failed with RC = %d\n",
                    __FUNCTION__, (UINT32)rc);

            outputCommentStr("FAIL,");
            goto Cleanup;
        }

        if (!m_IgnoreUnderflow &&
            ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR))
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Display underflow was detected on head %u after forcing large cursor.\n",
                    __FUNCTION__, underflowHead);

            outputCommentStr("FAIL,");
            goto Cleanup;
        }
    }

    //Setting Cursor To 0,0 Pos
    m_reportStr = Utility::StrPrintf(
            "\nINFO: %s: Moving the Cursor on DisplayId = %X to (x,y):(%d,%d)\n",
            __FUNCTION__, (UINT32)(*pModesetDisplayIds)[0], lwrX, lwrY);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    rc = m_pDisplay->SetLwrsorPos((*pModesetDisplayIds)[0], lwrX, lwrY);

    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: SetLwrsorPos() Failed with RC = %d\n",
                __FUNCTION__, (UINT32)rc);

        outputCommentStr("FAIL,");
        goto Cleanup;
    }

    if (!m_IgnoreUnderflow &&
        ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR))
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Display underflow was detected on head %u after SetLwrsorPos.\n",
                __FUNCTION__, underflowHead);

        outputCommentStr("FAIL,");
        goto Cleanup;
    }

    // Let's re- verify if the ARCH request PORPState is still existing after Large cursor changes.
    // This if block takes care of it
    if (m_PStateSupport && m_forcePStateBeforeModeset)
    {
        UINT32 lwrrPState = ~0;
        rc = GetBoundGpuSubdevice()->GetPerf()->GetLwrrentPState(&lwrrPState);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetLwrrentPState() failed with RC = %d\n",
                    __FUNCTION__, (UINT32)rc);
            outputCommentStr("FAIL,");
            goto Cleanup;
        }
        if (!m_bIsPS30Supported)
        {
            if (DTIUtils::Mislwtils::getLogBase2(porMinPState) != lwrrPState)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Set Large Cursor Changed the POR Pstate from P%u to new pstate P%u.\n",
                        __FUNCTION__,
                        DTIUtils::Mislwtils::getLogBase2(porMinPState),
                        lwrrPState);
                outputCommentStr("FAIL,");
                rc = RC::UNEXPECTED_RESULT;
                goto Cleanup;
            }
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: SUCCESS: POR %s %s%u is still retained after Set Large Cursor.\n",
                        __FUNCTION__,
                        m_PerfStatePrint,
                        m_PerfStatePrintHelper,
                        lwrrPState);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
        else
        {
            // TODO: Need to find an alternative to checking if the vP-State is set?
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: The PState after Set Large Cursor change: P%d."
                    "While requested POR vP-State: IMP vP%d\n",
                    __FUNCTION__,
                    lwrrPState,
                    porMinPState);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }

#ifdef INCLUDE_MDIAG
    // Placing deliberately after the setmode. If Setmode isnt possible at all
    // then we shouldnt be running the traces.
    TestRunningOnMRMI = MrmCppInfra::TestsRunningOnMRMI();

    if (TestRunningOnMRMI)
    {
        if (!m_bOnetimeSetupDone)
        {
            m_reportStr = Utility::StrPrintf(
                "INFO: %s: RM Setup is done. Waiting for MRMI setup completion.\n ",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            m_bOnetimeSetupDone = true;
            bMrmTracesStarted   = true;

            if ((rc = MrmCppInfra::SetupDoneWaitingForOthers("RM")) != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "INFO: %s: SetupDoneWaitingForOthers(RM) failed with RC = %d. Skipping setmode and going to next config.\n ",
                    __FUNCTION__, (UINT32)rc);
                outputCommentStr("FAIL,");
                goto Cleanup;
            }
        }

        m_reportStr = Utility::StrPrintf(
                "INFO: %s: Sending SetEvent(Modeset).\n ",
                __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        rc = MrmCppInfra::SetEvent("Modeset");
        m_bModesetEventSent = true;
        bMrmTracesStarted = true;

        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: MrmCppInfra::SetEvent(Modeset) failed with RC=%d.\n"
                    "Skipping this mode\n",
                    __FUNCTION__, (UINT32)rc);

            outputCommentStr("FAIL,");
            goto Cleanup;
        }
    }
#endif

    m_reportStr = Utility::StrPrintf(
            "%s: Sleeping for Setmode Delay:%d seconds\n", __FUNCTION__,
            g_IMPMarketingParameters.testLevelConfig.setModeDelay);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    Sleep(g_IMPMarketingParameters.testLevelConfig.setModeDelay * 1000, m_runOnEmu, GetBoundGpuSubdevice());

    //
    // Manual verif is only supported on EVO, because that is the only
    // infrastructure that supports writing messages to a surface.
    //
    if (m_manualVerif &&
        (m_pDisplay->GetDisplayClassFamily() == Display::EVO))
    {
        rc = DTIUtils::VerifUtils::manualVerification(m_pDisplay,
            (*pDisplayIds)[uiDisplay],
            pUiSurface,
            "Is image OK?(y/n)",
            (*pDisplayModes)[uiDisplay],
            DTI_MANUALVERIF_TIMEOUT_MS,
            DTI_MANUALVERIF_SLEEP_MS);

        if (rc != OK)
        {
            if(rc == RC::LWRM_INSUFFICIENT_RESOURCES)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Manual verification failed with insufficient resources.Bug 736351. RC=%d\n",__FUNCTION__, (UINT32)rc);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                rc.Clear();
            }
            else
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Manual Verif Failed with RC = %d\n",
                        __FUNCTION__, (UINT32)rc);

                outputCommentStr("FAIL,");
                goto Cleanup;
            }
        }
    }

    // checkSubTestResults will update comment field in CSV file if any error occur
    if ((rc = checkSubTestResults(pDispHeadMapParams, pDisplayIds, porMinPState)) != OK)
    {
        goto Cleanup;
    }

    if (!m_IgnoreUnderflow &&
        ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR))
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Display underflow was detected on head %u after checkSubTestResults\n",
                __FUNCTION__, underflowHead);

        outputCommentStr("FAIL,");
        goto Cleanup;
    }

#ifdef INCLUDE_MDIAG
    m_bClockSwitchEventSent = false;
    if (TestRunningOnMRMI)
    {
        m_reportStr = Utility::StrPrintf(
                "INFO: %s: Sending SetEvent(ClockSwitch).\n ",
                __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        rc = MrmCppInfra::SetEvent("ClockSwitch");
        m_bClockSwitchEventSent = true;
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: MrmCppInfra::SetEvent(ClockSwitch) Failed with RC = %d. "
                    "Skipping this mode\n",
                    __FUNCTION__, (UINT32)rc);

            outputCommentStr("FAIL,");
            goto Cleanup;
        }
    }
#endif

    // Launch Apps. This is supported only on win32 mods
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    UINT32 maxLaunchTimeSec         = 0;
    LwPhysicalGpuHandle physicalGpuHandle;
    appLaunchCreatedUnderflow = false;
    appLaunchError = false;
    appNotRun                       = false;
    UINT32  ourGpuId                = m_pDisplay->GetOwningDisplaySubdevice()->GetGpuId();
    LwAPI_GetPhysicalGPUFromGPUID(ourGpuId, &physicalGpuHandle);
    UINT32 maxAppSec = 0;

    // Modeset has passed, aero has been disabled.
    // Callwlate baseline MClk utilization before running any app.
    pstatesInfo.version = LW_GPU_DYNAMIC_PSTATES_INFO_VER;
    LwAPI_GPU_GetDynamicPstatesInfo(physicalGpuHandle, &pstatesInfo);

    if (pstatesInfo.utilization[LWAPI_GPU_UTILIZATION_DOMAIN_FB].bIsPresent)
    {
        baselineMClkUtil = pstatesInfo.utilization[LWAPI_GPU_UTILIZATION_DOMAIN_FB].percentage;
        m_reportStr = Utility::StrPrintf(
                "INFO: %s: BaselineMClkUtil = %d\n",
                __FUNCTION__, baselineMClkUtil);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    m_reportStr = Utility::StrPrintf(
            "INFO: %s: Launching Apps:\n", __FUNCTION__);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    if (NO_CONFIG_NUMBER != configNo &&
        g_IMPMarketingParameters.configList[configNo].useInlineApp)
    {
        for (UINT32 modeNo = 0; modeNo < g_IMPMarketingParameters.configList[configNo].appList.size() &&
             modeNo < static_cast<UINT32>(pDisplayIds->size()); modeNo++)
        {
            for (UINT32 appNo = 0; appNo < g_IMPMarketingParameters.configList[configNo].appList[modeNo].size(); appNo++)
            {
                launchAppParams = new LaunchApplicationParams();

                rc = setupAppParams((*pDisplayIds)[modeNo], launchAppParams,
                                    g_application[g_IMPMarketingParameters.configList[configNo].appList[modeNo][appNo]]);
                if (rc != OK)
                {
                    delete launchAppParams;
                    goto Cleanup;
                }

                sprintf(launchAppParams->threadName, "Launch Application %u", appNo);

                ptrsLaunchAppParams.push_back(launchAppParams);
            }
        }
    }
    else
    {
        // Let's use apps specified in the TestLevelConfig
        // Launch the non ovly app list on all display IDs
        // TODO: need to check for moving apps accross the screens.
        for (disp = 0; disp < pDisplayIds->size(); disp++)
        {
            for (UINT32 appNo = 0; appNo < g_IMPMarketingParameters.testLevelConfig.appsToLaunchNoOverlay.size(); appNo++)
            {
                launchAppParams = new LaunchApplicationParams();

                rc = setupAppParams((*pDisplayIds)[disp], launchAppParams,
                                    g_application[g_IMPMarketingParameters.testLevelConfig.appsToLaunchNoOverlay[appNo]]);
                if (rc != OK)
                {
                    delete launchAppParams;
                    goto Cleanup;
                }

                sprintf(launchAppParams->threadName, "Launch Application %u", appNo);

                ptrsLaunchAppParams.push_back(launchAppParams);
            }
        }

        m_reportStr = Utility::StrPrintf(
                "INFO: %s: Launching Overlay Apps:\n",
                __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        // Launch the overlay applist on first noOfOverlayCount DispID
        // TODO: need to check for moving apps accross the screens.
        for (disp = 0; disp < overlayCount; disp++)
        {
            for (UINT32 appNo = 0; appNo < g_IMPMarketingParameters.testLevelConfig.appsToLaunchOverlay.size(); appNo++)
            {
                launchAppParams = new LaunchApplicationParams();

                rc = setupAppParams((*pDisplayIds)[disp], launchAppParams,
                                    g_application[g_IMPMarketingParameters.testLevelConfig.appsToLaunchOverlay[appNo]]);
                if (rc != OK)
                {
                    delete launchAppParams;
                    goto Cleanup;
                }

                sprintf(launchAppParams->threadName, "Launch Overlay Application %u", appNo);

                ptrsLaunchAppParams.push_back(launchAppParams);
            }
        }
    }

    for (UINT32 appNo = 0; appNo < ptrsLaunchAppParams.size(); appNo++)
    {
        if (ptrsLaunchAppParams[appNo]->app.seconds > maxAppSec)
        {
            maxAppSec = ptrsLaunchAppParams[appNo]->app.seconds;
        }

        if (ptrsLaunchAppParams[appNo]->launchTimeSec > maxLaunchTimeSec)
        {
            maxLaunchTimeSec = ptrsLaunchAppParams[appNo]->launchTimeSec;
        }

        threadIdAppLaunch.push_back(
            Tasker::CreateThread
            (
                LaunchApplication,
                ptrsLaunchAppParams[appNo],
                Tasker::MIN_STACK_SIZE,
                ptrsLaunchAppParams[appNo]->threadName
            )
        );
    }

    //
    // Calling sleep here will ilwoke other ready threads we created above for applications.
    // And waiting one more second longer than application launch latency to ensure post launch operations
    // such as setting the applications intended window size and location have finished before entering
    // Random PState Test.
    //
    Sleep((maxLaunchTimeSec + 1) * 1000, m_runOnEmu, GetBoundGpuSubdevice());

    if (appLaunchError)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Application launch failed.\n", __FUNCTION__);

        outputCommentStr("FAIL,");
        rc = RC::SOFTWARE_ERROR;
        goto Cleanup;
    }

    //
    // Run Random PState Test for half of max application running time. And then stay min PState
    // until all application threads finished so that applications run on min PState for at least half of
    // max running time since min PState is more stressful.
    //
    // And don't poll underflow state inside RandomPStateTransitions due to 2 reasons below -
    // 1. It will keep polling underflow state in running app thread and app running time should be longer than Random PState Test,
    //    so don't poll it inside RandomPStateTransitions and rely on app thread for checking underflow.
    // 2. I found it may cause system hangs in very rare case if we poll display underflow state in different threads.
    //
    bool bRunMinPStateTest = false;
    bool bRunIsoFbLatencyTest = false;
    UINT32 randomPStateTestDuration = (m_PStateSupport) ? (maxAppSec / 2) : 0;
    UINT32 minPStateTestDuration = maxAppSec - randomPStateTestDuration;
    if (m_PStateSupport)
    {
        if (m_bMclkSwitchPossible)
        {
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: Starting Random PState Transitions:\n", __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            rc = RandomPStateTransitions(m_lwrrImpMinPState, m_pstateSwitchCount, randomPStateTestDuration, false);

            if (rc == OK && !appLaunchCreatedUnderflow)
            {
                bRunMinPStateTest = true;
                rc = ProgramAndVerifyPState(m_lwrrImpMinPState);
            }
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "WARNING: %s: MCLK Switch is NOT possible."
                    " Hence ignoring RandomPStateTransitions( ).\n",
                    __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }

    //
    // Enable ISO FB Latency Measurement
    // Lwrrently, we perform Iso Fb Latency Measurement with only min p-state.
    // If m_PStateSupport is not true, the system is running at fixed clocks.
    // And that would represent the min pstate.
    //
    if (m_isoFbLatencyTest > 0 && minPStateTestDuration >= m_isoFbLatencyTest)
    {
        if (StartIsoFbLatencyTest() == OK)
        {
            bRunIsoFbLatencyTest = true;
        }
    }

    // Wait until the application thread terminates.
    Tasker::Join(threadIdAppLaunch);

    // Disable and get Iso Fb Latency Measurement result
    if (bRunIsoFbLatencyTest)
    {
        rc = EndIsoFbLatencyTest();
        if (OK != rc)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Iso FB Latency Test failed.\n", __FUNCTION__);

            outputCommentStr("FAIL,");
            goto Cleanup;
        }
    }

    if (rc != OK || appLaunchCreatedUnderflow)
    {
        errorStr = "ERROR: Display underflow was detected";
        if (m_PStateSupport)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: %s PState Test failed",
                    __FUNCTION__, (bRunMinPStateTest ? "Min" : "Random"));

            errorStr = m_reportStr;
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(" with RC = %d", (UINT32)rc);
                errorStr += m_reportStr;
            }
            if (appLaunchCreatedUnderflow)
            {
                errorStr += " and Display underflow was detected";
            }
        }

        m_reportStr = Utility::StrPrintf(
                "%s during App running\n", errorStr.c_str());

        outputCommentStr("FAIL,");
        goto Cleanup;
    }

    if (appNotRun)
    {
        outputCommentStr("FAIL,");
        rc = RC::SOFTWARE_ERROR;
        goto Cleanup;
    }

    // runStressTestOnPrimaryDisplay will output the error(if any) to log and csv files
    rc = runStressTestOnPrimaryDisplay((*pDisplayIds)[0], m_lwrrImpMinPState);
    if (rc != OK)
    {
        goto Cleanup;
    }
#else // not LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

    // Set overlay image on first OverlayCount DispIDs.
    for (ovlyNo = 0; ovlyNo < overlayCount; ovlyNo ++)
    {
        ovlyWidth        = (*pDisplayModes)[ovlyNo].width;
        ovlyHeight       = (*pDisplayModes)[ovlyNo].height;
        ovlyDepth        = (*pDisplayModes)[ovlyNo].depth;

        //Setting windowed overlay
        m_reportStr = Utility::StrPrintf(
                "INFO: %s: Setting up overlay channel image on DispId: 0x%X, of %dx%d\n", __FUNCTION__,
            (UINT32)(*pDisplayIds)[ovlyNo], (UINT32)ovlyWidth, (UINT32)ovlyHeight);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        oimageArray = ImageUtils::SelectImage(ovlyWidth, ovlyHeight);

        //Free the core image else SetupChannelImage will throw RC error
        if(m_pOvlyChnImg[ovlyNo].GetMemHandle() != 0)
        {
            m_pOvlyChnImg[ovlyNo].Free();
        }

        rc = m_pDisplay->SetupChannelImage((*pDisplayIds)[ovlyNo],
            ovlyWidth, ovlyHeight,
            ovlyDepth,
            Display::OVERLAY_CHANNEL_ID,
            &m_pOvlyChnImg[ovlyNo],
            oimageArray.GetImageName(),
            0,
            (*pModesetHeads)[ovlyNo],
            0,
            0,
            Surface2D::BlockLinear);
        if (OK != rc)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: SetupChannelImage(OVERLAY_CHANNEL) failed for DispId:0x%X; head %u.\n",
                    __FUNCTION__, (UINT32)(*pDisplayIds)[ovlyNo], (*pModesetHeads)[ovlyNo]);

            outputCommentStr("FAIL,");
            goto Cleanup;
        }

        rc = m_pDisplay->SetImage((*pDisplayIds)[ovlyNo], &m_pOvlyChnImg[ovlyNo], Display::OVERLAY_CHANNEL_ID);
        if (OK != rc)
        {
            m_reportStr = Utility::StrPrintf(
                "ERROR: %s: SetImage(OVERLAY_CHANNEL) failed for DispId:0x%X; head %u.\n",
                __FUNCTION__, (UINT32)(*pDisplayIds)[ovlyNo], (*pModesetHeads)[ovlyNo]);

            outputCommentStr("FAIL,");
            goto Cleanup;
        }

        if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR)
        {
            if (!m_IgnoreUnderflow)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Display underflow detected. Doing SetOvlyImage(0x%x) on head %u.\n",
                    __FUNCTION__, (UINT32)(*pDisplayIds)[ovlyNo], underflowHead);

                outputCommentStr("FAIL,");
                goto Cleanup;
            }
        }
    }

    if (m_PStateSupport)
    {
        if (m_bMclkSwitchPossible)
        {
            m_reportStr = Utility::StrPrintf(
                "INFO: %s: Starting RandomPStateTransitions( ).\n ",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            rc = RandomPStateTransitions(m_lwrrImpMinPState, m_pstateSwitchCount);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Random PState Test failed with RC = %d\n",
                        __FUNCTION__, (UINT32)rc);
                outputCommentStr("FAIL,");
                goto Cleanup;
            }
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                "WARNING: %s: MCLK Switch is NOT possible. Hence ignoring RandomPStateTransitions( ).\n",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }
#endif

Cleanup:

    m_reportStr = Utility::StrPrintf(
            "INFO: %s: Starting config related cleanup now.\n", __FUNCTION__);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    errorStr = " ";
    if (appLaunchCreatedUnderflow)
    {
        m_reportStr = Utility::StrPrintf(
                "%s", "Display underflow was detected on app launch.");
        outputCommentStr("");
        m_reportStr = " ";
    }

    // stickyRC keeps a record of the first error encountered
    stickyRC = rc;

#ifdef INCLUDE_MDIAG
    if (TestRunningOnMRMI)
    {
        // MRMI needs all events to be sent in sequence:
        // Send( Modeset => ClockSwitch => ClockSwitchDone ) => Wait(TraceDone)
        // So if in case we have exited above config abruptly then let's send event now.

        // Send all other events only if the first event of "Modeset" was sent for this config.
        // If it was not sent then it means that the config exited much before modeset.
        // So no need to send other events here.
        if (!bMrmTracesStarted)
        {
            m_reportStr = Utility::StrPrintf(
                "INFO: %s: No traces started for this thread. Hence skipping MRM cleanup.\n",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            goto endOfMrmTraces;
        }

        if (!m_bModesetEventSent)
        {
            m_reportStr = Utility::StrPrintf(
                "INFO: %s: MRMI: RM->MRMI: Sending Modeset event.\n",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            rc = MrmCppInfra::SetEvent("Modeset");
            m_bModesetEventSent = true;
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: %s MrmCppInfra::SetEvent(Modeset) Failed with RC = %d.",
                    __FUNCTION__, errorStr.c_str(), (UINT32)rc);
                outputCommentStr("");
                cleanupRC = rc;
            }
        }
        if (!m_bClockSwitchEventSent)
        {
            m_reportStr = Utility::StrPrintf(
                "INFO: %s: MRMI: RM->MRMI: Sending ClockSwitch event.\n",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            rc = MrmCppInfra::SetEvent("ClockSwitch");
            m_bClockSwitchEventSent = true;
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: %s MrmCppInfra::SetEvent(ClockSwitch) Failed with RC = %d.",
                    __FUNCTION__, errorStr.c_str(), (UINT32)rc);
                outputCommentStr("");
                cleanupRC = rc;
            }
        }

        if (!m_bClockSwitchDoneEventSent)
        {
            m_reportStr = Utility::StrPrintf(
                "INFO: %s: MRMI: RM-> MRMI: Sending ClockSwitchDone event.\n",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            rc = MrmCppInfra::SetEvent("ClockSwitchDone");
            m_bClockSwitchDoneEventSent = true;
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: %s MrmCppInfra::SetEvent(ClockSwitchDone) Failed with RC = %d.",
                    __FUNCTION__, errorStr.c_str(), (UINT32)rc);
                outputCommentStr("");
                cleanupRC = rc;
                goto endOfMrmTraces;
            }
        }
        if (!m_TraceDoneEventWaitCount)
        {
            m_TraceDoneEventWaitCount++;
            rc = MrmCppInfra::WaitOnEvent("TraceDone");
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: %s MrmCppInfra::WaitOnEvent(TraceDone) Failed with RC = %d.",
                    __FUNCTION__, errorStr.c_str(), (UINT32)rc);
                outputCommentStr("");
                cleanupRC = rc;
                goto endOfMrmTraces;
            }

            rc = MrmCppInfra::ResetEvent("TraceDone");
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: %s MrmCppInfra::ResetEvent(TraceDone) Failed with RC = %d.",
                    __FUNCTION__, errorStr.c_str(), (UINT32)rc);
                outputCommentStr("");
                cleanupRC = rc;
                goto endOfMrmTraces;
            }
        }
    }
endOfMrmTraces:
#endif

//Do cleanup once TraceDone event is received as it might still be using the Display
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    // Free the objects allocated dynamically
    // We needed to allocate object on heap to keep the object alive when it is used in global LaunchApplication() method.
    m_reportStr = Utility::StrPrintf(
        "INFO: %s: Freeing App Launch Ptrs.\n",
        __FUNCTION__);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    for (LwS32 appNo = static_cast<LwS32>(ptrsLaunchAppParams.size()) - 1; appNo >= 0 ; appNo--)
    {
        delete ptrsLaunchAppParams[appNo];
        ptrsLaunchAppParams.erase(ptrsLaunchAppParams.begin()+appNo);
    }

    // TODO: To Detach all monitors on windows too.
    // this is needed since HW team wants us to force the PORPState at start of each config.
    // So making active heads as 1 [windows always needs atleast 1 active head].
#else
    // Free Overlay Image
    for (ovlyNo = 0; ovlyNo < overlayCount; ovlyNo ++)
    {
        m_reportStr = Utility::StrPrintf(
                "\nINFO: %s: Freeing overlay image on DisplayId[%X]\n", __FUNCTION__, (UINT32)(*pDisplayIds)[ovlyNo]);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        if (m_pOvlyChnImg[ovlyNo].GetMemHandle() != 0)
        {
            rc = m_pDisplay->SetImage((*pDisplayIds)[ovlyNo],
                            NULL,
                            Display::OVERLAY_CHANNEL_ID);

            if (OK != rc)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: ClearImage(OVERLAY_CHANNEL) failed for DispId:0x%X;\n",
                    __FUNCTION__, (UINT32)(*pDisplayIds)[ovlyNo]);
                cleanupRC = rc;
            }

            m_pOvlyChnImg[ovlyNo].Free();
        }
    }

    // Free Cursor channel
    if (m_pLwrsChnImg)
    {
        if ((UINT32)pDisplayIds->size() && (m_pLwrsChnImg[0].GetMemHandle() != 0))
        {
            m_reportStr = Utility::StrPrintf(
                "\nINFO: %s: Freeing cursor on DisplayId[%X]\n", __FUNCTION__, (UINT32)(*pDisplayIds)[0]);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            rc = m_pDisplay->SetImage((*pDisplayIds)[0],
                                NULL,
                                Display::LWRSOR_IMM_CHANNEL_ID);
            if (OK != rc)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: ClearImage(LWRSOR_CHANNEL) failed for DispId:0x%X;\n",
                    __FUNCTION__, (UINT32)(*pDisplayIds)[0]);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                cleanupRC = rc;
            }
            m_pLwrsChnImg[0].Free();
        }
    }

    // detach display on linux.
    for (UINT32 activeDispId = 0; activeDispId < (UINT32)pDisplayIds->size(); activeDispId ++)
    {
        // here all the images if any allocated will be Freed ..
        m_reportStr = Utility::StrPrintf(
                "\nDetaching DisplayId[%X]\n", (UINT32)(*pDisplayIds)[activeDispId]);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        rc = m_pDisplay->DetachDisplay(DisplayIDs(1, (*pDisplayIds)[activeDispId]));
        if (OK != rc)
        {
            m_reportStr = Utility::StrPrintf(
                "ERROR: %s: DetachDisplay(0x%X) failed with rc=%d.\n",
                __FUNCTION__, (UINT32)(*pDisplayIds)[0], (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            cleanupRC = rc;
        }

        // Freeing Core channel Surface2D.
        if (m_channelImage[activeDispId].GetMemHandle() != 0)
            m_channelImage[activeDispId].Free();
    }
#endif

    m_reportStr = Utility::StrPrintf(
            "\nINFO: %s: Clearing forced pstate.\n", __FUNCTION__);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    // Clear any forced PState as the last step.
    rc = m_objSwitchRandomPState.ProgramPState(LW2080_CTRL_PERF_PSTATES_CLEAR_FORCED);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: %s Clear Forced PState failed with RC = %d\n",
                __FUNCTION__, errorStr.c_str(), (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        cleanupRC = rc;
    }

    if (OK == stickyRC)
    {
        if (OK == cleanupRC)
        {
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: Config passed successfully.\n", __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            // Print to CSV report
            PRINT_STDOUT_N_REPORT(Tee::PriHigh, "PASS", PRINT_CSV_FILE );
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Error during cleanup.\n", __FUNCTION__);
            outputCommentStr("FAIL,");
        }
    }

    stickyRC = cleanupRC;

    return stickyRC;
}

//! TestLwrrCombination
//! Loops through all the edid resolutions for all the displayIds passed to this method
//!
RC Modeset::TestLwrrCombination(DisplayIDs dispIds,
                                LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams,
                                vector<ModeSettingsEx>   lwrrModesOnOtherDispIds)
{
    RC rc, rcOverall = OK;
    DisplayID SetModeDisplay = dispIds[0];
    DisplayIDs remainingDispIds;
    DisplayIDs activeDisplays;
    UINT32   Head;
    Surface2D CoreImage;
    char                        lwrrTime[30]       = "";
    string                      dateFormat         = "%Y-%m-%d-%H-%M-%S";
    vector<ModeSettingsEx>      totalModeSettingsEx;
    ModeSettingsEx              thisModeSettingsEx;
    DisplayIDs                  lwrrCombiDispIds2Use;
    vector<Display::Mode>       lwrrCombiModes2Use;
    vector<UINT32>              lwrrCombiOvlyDepth;
    Display::Mode               tempMode;
    vector<Display::Mode>       allListedResolutions;

    if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask(SetModeDisplay, dispHeadMapParams, &Head)) != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: GetHeadFromRoutingMask Failed to Get Head for DispID = 0x%08X. RC = %d\n ",
                __FUNCTION__, (UINT32)SetModeDisplay, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        m_reportStr = Utility::StrPrintf(
            "Head Routing Mask Params =\n"                  \
            "dispHeadMapParams.subDeviceInstance = %u\n"    \
            "dispHeadMapParams.displayMask       = %u\n"    \
            "dispHeadMapParams.oldDisplayMask    = %u\n"    \
            "dispHeadMapParams.oldHeadRoutingMap = %u\n"    \
            "dispHeadMapParams.headRoutingMap    = %u\n",
            (UINT32)dispHeadMapParams.subDeviceInstance,
            (UINT32)dispHeadMapParams.displayMask,
            (UINT32)dispHeadMapParams.oldDisplayMask,
            (UINT32)dispHeadMapParams.oldHeadRoutingMap,
            (UINT32)dispHeadMapParams.headRoutingMap);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::ILWALID_HEAD;
    }

    allListedResolutions.clear();

    rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                     SetModeDisplay,
                                    &allListedResolutions,
                                     m_useWindowsResList);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Failed to GetListedModes(0x%X) ",
                            __FUNCTION__,(UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        return rc;
    }

    // On win32 Platform GetListedModes() API would return resolutions of 8 bit and 16 bit depth too.
    // Deleting those resolutions since it would increase test run time exponentially
    for (LwS32 loopCount = static_cast<UINT32>(allListedResolutions.size()) - 1 ;
               loopCount >= 0; loopCount--)
    {
        if(allListedResolutions[loopCount].depth < 32)
        {
            allListedResolutions.erase(allListedResolutions.begin() + loopCount);
        }
    }

    ImageUtils imgArr;
    imgArr = ImageUtils::SelectImage(640, 480);

    for (LwS32 loopCount = static_cast<UINT32>(allListedResolutions.size()) - 1 ;
               loopCount >= 0; loopCount--)
    {

        UINT32 setmodeWidth;
        UINT32 setmodeHeight;
        UINT32 setmodeRefreshRate;
        UINT32 setmodeDepth = 32;
        bool runLwstomResolution = false;
        m_clockDiffFound = false;

        if(!runLwstomResolution )
        {
            setmodeWidth = allListedResolutions[loopCount].width;
            setmodeHeight = allListedResolutions[loopCount].height;
            setmodeRefreshRate = allListedResolutions[loopCount].refreshRate;
        }

        PRINT_STDOUT_N_REPORT(Tee::PriHigh, "\n", PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);
        m_reportStr = Utility::StrPrintf(
            "\n Testing Mode:"
            "\n ||        TimeStamp       ||   dispId  ||  width  ||  height  "
            "||  Depth  ||  RR  ||  IMP  ||  SetMode ||  Result  || Comment ||");
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        DTIUtils::ReportUtils::GetLwrrTime(dateFormat.c_str(), lwrrTime, sizeof(lwrrTime));

        //Write Mode list to the CSV Config file
        for(UINT32 modeNo = 0; modeNo < lwrrModesOnOtherDispIds.size(); modeNo++)
        {
            m_reportStr = Utility::StrPrintf(
                "\n%30s,0x%10X,%10d,%10d,%10d,%10d,",
                lwrrTime,(UINT32)lwrrModesOnOtherDispIds[modeNo].displayId,
                lwrrModesOnOtherDispIds[modeNo].modeSetting.mode.width,
                lwrrModesOnOtherDispIds[modeNo].modeSetting.mode.height,
                lwrrModesOnOtherDispIds[modeNo].modeSetting.mode.depth,
                lwrrModesOnOtherDispIds[modeNo].modeSetting.mode.refreshRate);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE  | PRINT_CSV_FILE);
        }
        m_reportStr = Utility::StrPrintf(
                "\n%30s,0x%10X,%10d,%10d,%10d,%10d,",
                lwrrTime,(UINT32)SetModeDisplay,
                setmodeWidth,
                setmodeHeight,
                setmodeDepth,
                setmodeRefreshRate);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE  | PRINT_CSV_FILE);

        // Clear previous underflow state
        rc = SetUnderflowStateOnAllHead(LW5070_CTRL_CMD_UNDERFLOW_PROP_ENABLED_YES,
            LW5070_CTRL_CMD_UNDERFLOW_PROP_CLEAR_UNDERFLOW_YES,
            LW5070_CTRL_CMD_UNDERFLOW_PROP_MODE_RED);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: SetUnderflowStateOnAllHead() Failed. RC = %d\n ", __FUNCTION__, (UINT32)rc);

            outputCommentStr("Not Attempted,Not Attempted,FAIL,");
            goto nextResolution;
        }

        //
        // Note:  The channel image function used below supports the core
        // channel only on EVO.
        //
        if (m_pDisplay->GetDisplayClassFamily() == Display::EVO)
        {
            //Free the core image else SetupChannelImage will throw RC error
            if(CoreImage.GetMemHandle() != 0)
            {
                CoreImage.Free();
            }
            rc = m_pDisplay->SetupChannelImage(SetModeDisplay,
                setmodeWidth,
                setmodeHeight,
                setmodeDepth,
                Display::CORE_CHANNEL_ID,
                &CoreImage,
                imgArr.GetImageName(),
                0,
                Head);
            if (rc != OK )
            {
                // combination is not possible, throw an error
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: SetupChannelImage() Failed. RC = %d \n ", __FUNCTION__, (UINT32)rc);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto nextResolution;
            }
        }
        totalModeSettingsEx.clear();
        totalModeSettingsEx = lwrrModesOnOtherDispIds;

        thisModeSettingsEx.displayId = dispIds[0];
        thisModeSettingsEx.modeSetting.mode = Display::Mode(setmodeWidth, setmodeHeight,
                                       setmodeDepth,setmodeRefreshRate);
        totalModeSettingsEx.push_back(thisModeSettingsEx);

        lwrrCombiDispIds2Use.clear();
        lwrrCombiModes2Use.clear();
        lwrrCombiOvlyDepth.clear();

        for(UINT32 countModeSetting = 0;
            countModeSetting < static_cast<UINT32>(totalModeSettingsEx.size());
            countModeSetting++)
        {
            lwrrCombiDispIds2Use.push_back(totalModeSettingsEx[countModeSetting].displayId);
            tempMode = Display::Mode(
                totalModeSettingsEx[countModeSetting].modeSetting.mode.width,
                totalModeSettingsEx[countModeSetting].modeSetting.mode.height,
                totalModeSettingsEx[countModeSetting].modeSetting.mode.depth,
                totalModeSettingsEx[countModeSetting].modeSetting.mode.refreshRate
               );
            lwrrCombiModes2Use.push_back(tempMode);
            lwrrCombiOvlyDepth.push_back(32);
        }

        {
            vector<Display::Mode> modesToSet(1, lwrrCombiModes2Use[static_cast<UINT32>(totalModeSettingsEx.size()) - 1]);
            DisplayIDs            modesetDisplayIds(1, SetModeDisplay);
            vector<UINT32>        modesetHeads(1, Head);

            rc = TestDisplayConfig(NO_CONFIG_NUMBER,
                                   &lwrrCombiDispIds2Use,
                                   &lwrrCombiModes2Use,
                                   &lwrrCombiOvlyDepth,
                                   &modesetDisplayIds,
                                   &modesToSet,
                                   &modesetHeads,
                                   Display::STANDARD,
                                   LW2080_CTRL_PERF_PSTATES_UNDEFINED,
                                   IMPTEST3_VPSTATE_UNDEFINED,
                                   static_cast<UINT32>(totalModeSettingsEx.size()) - 1,
                                   &CoreImage,
                                   &dispHeadMapParams);
        }

        if (rc != OK)
        {
            goto nextResolution;
        }

        activeDisplays.clear();
        rc = m_pDisplay->GetScanningOutDisplays(&activeDisplays);
        if (OK != rc)
        {
            goto nextResolution;
        }

        remainingDispIds.clear();
        remainingDispIds = dispIds;
        // if there is another displayIDs in the vector
        // Then check if free heads are available for it and then Go ahead OR
        // if it was already active displayID then too we can go ahead and
        // call same method relwrsively with this next displayID
        if ((dispIds.size() > 1) &&
            (activeDisplays.size() < m_numHeads ||
             m_pDisplay->IsDispAvailInList(dispIds[1], activeDisplays)))
        {
            remainingDispIds.erase(remainingDispIds.begin());
            rc = TestLwrrCombination(remainingDispIds, dispHeadMapParams, totalModeSettingsEx);
            if(rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: TestLwrrCombination() Failed with RC=%d\n",
                        __FUNCTION__, (UINT32)rc);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

                goto nextResolution;
            }
        }

nextResolution:

        // rcOverall keeps a record of the first error encountered
        if (rcOverall == OK)
            rcOverall = rc;
    }

    PRINT_STDOUT_N_REPORT(Tee::PriHigh, "\n", PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);

    if (m_pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
        if(CoreImage.GetMemHandle() != 0)
        {
            CoreImage.Free();
        }
    }

    return rcOverall;
}

//! \setup Initialise internal state from JS
//!
//! Initial state has to be setup based on the JS setting. This function
// does the same.
//------------------------------------------------------------------------------
RC Modeset::Setup()
{
    RC rc;
    string headerString;

    CHECK_RC(InitFromJs());

    m_pCSVFile = fopen(m_CSVFileName.c_str(), "a+");
    if (m_pCSVFile == NULL)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Failed to open File %s in a+ mode\n",
            __FUNCTION__, m_CSVFileName.c_str());
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT );
        return RC::CANNOT_OPEN_FILE;
    }

    m_pReportFile = fopen(m_ReportFileName.c_str(), "a+");
    if (m_pReportFile == NULL)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Failed to open File %s in a+ mode\n",
            __FUNCTION__, m_ReportFileName.c_str());
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT );
        return RC::CANNOT_OPEN_FILE;
    }

    if( m_inputconfigfile.compare("") != 0 &&
        g_IMPMarketingParameters.testLevelConfig.attachFakeDevices == true)
    {
        m_onlyConnectedDisplays = false;
    }

    if(!m_runfrominputfile &&
       (g_IMPMarketingParameters.testLevelConfig.forceModeOnSpecificProtocol == true))
    {
        m_protocol = "";
        for( UINT32 i = 0; i < g_IMPMarketingParameters.testLevelConfig.forceProtocolSequence.size(); i++)
        {
            m_protocol += g_IMPMarketingParameters.testLevelConfig.forceProtocolSequence[i];
            m_protocol += ",";
        }
    }

    // m_autoGenerateEdid is valid only for faked displays
    if (m_onlyConnectedDisplays)
    {
        m_autoGenerateEdid = false;
    }

    // Windows res list grabbing is valid only for winmods
    if(m_pDisplay->GetDisplayClassFamily() != Display::WIN32DISP &&
       m_useWindowsResList)
    {
        m_reportStr = Utility::StrPrintf(
            "WARNING: %s: Setting m_useWindowsResList = false for non winmods platform.\n",
            __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        m_useWindowsResList = false;
    }

    // At Windows, do not lock pstate prior to modeset as it would cause underflow
    // if the previous mode is large.
    if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP &&
       m_forcePStateBeforeModeset)
    {
        m_reportStr = Utility::StrPrintf(
            "WARNING: %s: At WinMods, we shouldn't lock pstate prior to modeset. Hence, setting m_forcePStateBeforeModeset = false.\n",
            __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        m_forcePStateBeforeModeset = false;
    }

    // Let's first get the underflow params of the lwrrect board
    // so on exit we restore the underflow reproting state back to this value.
    // Note: retrieving value from head 0 would be enough since all heads would be symetrical.
    rc =DTIUtils::VerifUtils::GetUnderflowState(m_pDisplay,
        0,
        &m_underFlowParamsOnInit);

    if (OK != rc)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: Failed to get RG underflow params\n", __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    // Enable underflow reporting
    rc = SetUnderflowStateOnAllHead(LW5070_CTRL_CMD_UNDERFLOW_PROP_ENABLED_YES,
        LW5070_CTRL_CMD_UNDERFLOW_PROP_CLEAR_UNDERFLOW_YES,
        LW5070_CTRL_CMD_UNDERFLOW_PROP_MODE_RED);
    if(rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Failed to SetUnderflowStateOnAllHead. RC = %d\n",
            __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    //
    // Manual Verif can be used only if:
    // 1) Testing is being done with physical displays (so the results can be
    //    observed visually), and
    // 2) Testing is being done on an actual HW platform, and
    // 3) Testing is being done on EVO, because that is the only environment
    //    with SW infrastructure that supports writing messages to a surface.
    //
    if (!m_onlyConnectedDisplays ||
        (Platform::GetSimulationMode() != Platform::Hardware) ||
        (m_pDisplay->GetDisplayClassFamily() != Display::EVO))
    {
        m_manualVerif = false;
    }

    rc = m_objSwitchRandomPState.Setup();

    if(rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: m_objSwitchRandomPState.Setup failed with RC =%d.",
                __FUNCTION__,(UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    // split the comma separated CRT EDID files list to vector<string>
    m_useCRTFakeEdid = DTIUtils::Mislwtils::trim(m_useCRTFakeEdid);
    if(m_useCRTFakeEdid.compare("") != 0)
    {
        m_CRTFakeEdidFileList = DTIUtils::VerifUtils::Tokenize(
            m_useCRTFakeEdid, ",");
    }

    // split the comma separated DFP EDID files list to vector<string>
    m_useDFPFakeEdid = DTIUtils::Mislwtils::trim(m_useDFPFakeEdid);
    if(m_useDFPFakeEdid.compare("") != 0)
    {
        m_DFPFakeEdidFileList = DTIUtils::VerifUtils::Tokenize(
            m_useDFPFakeEdid, ",");
    }

    // split the comma separated config nos list to vector<int>
    m_runOnlyConfigs = DTIUtils::Mislwtils::trim(m_runOnlyConfigs);
    if(m_runOnlyConfigs.compare("") != 0)
    {
        vector<string> runOnlyConfigsListStr = DTIUtils::VerifUtils::Tokenize(
            m_runOnlyConfigs, ",");
        for (LwU32 i = 0; i < runOnlyConfigsListStr.size(); i++)
            m_runOnlyConfigsList.push_back(strtol(runOnlyConfigsListStr[i].c_str(), NULL, 10));
    }

    if (GetBoundGpuSubdevice()->IsEmulation())
        m_runOnEmu = true;

    CHECK_RC(populateKnownRes(allKnownResStr, wellKnownRes));

    return rc;
}

//! \brief Run the test
//!
//! It reads the Edid of all the connected displays one by one and then
//! test the listed resolutions in the edid along with the ones where
//! where the BE timings don't match the listed timings at native resolution
//! and requires upscaling
//------------------------------------------------------------------------------
RC Modeset::Run()
{
    RC rc, cleanupRc;
    Display *pDisplay = GetDisplay();
    CHECK_RC(pDisplay->InitializeDisplayHW(Display::ALLOC_DISP_CHANNELS));
    DisplayIDs Detected;
    DisplayIDs ActiveDisplays;
    DisplayIDs workingSet;
    UINT32     modesCountInt  = 0;
    UINT32     dfpDispIdCount = 0;
    UINT32     crtDispIdCount = 0;
    UINT32     numDisps;
    UINT32     crtTotalFakeEdidCount =
                    static_cast<UINT32>(m_CRTFakeEdidFileList.size());

    UINT32     dfpTotalFakeEdidCount =
                    static_cast<UINT32>(m_DFPFakeEdidFileList.size());
    string     edidFileName   = "";
#ifdef INCLUDE_MDIAG
    bool       TestRunningOnMRMI = false;
#endif
    Display::DisplayType type;
    vector<Display::Mode> ListedModes;
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    BOOL bAeroResult = FALSE;
#endif

    //
    // To guarantee valid test results, first clear any pstate forces that may
    // have been left by other programs.
    //
    // Note: The RM Ctrl call for forced pstate clear does a pstate switch
    // internally to perfGetFastestLevel() value to restore clocks, in case
    // tools have changed any clocks directly. ClearForcedPState() clears all
    // the CLIENT* perflimits in the case of PS30.
    //

    GetBoundGpuSubdevice()->GetPerf()->ClearForcedPState();

    CHECK_RC_CLEANUP(pDisplay->GetDetected(&Detected));

    Printf(Tee::PriHigh, "\n All Detected displays =\n");
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, Detected);

    //Only the protocol passed by the user must be used
    for (UINT32 i=0; i < Detected.size(); i++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay,
                                                      Detected[i],
                                                      m_protocol))
        {
            workingSet.push_back(Detected[i]);
        }
    }

    Printf(Tee::PriHigh, "\n Detected displays which support protocol %s =\n", m_protocol.c_str());
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, workingSet);

    //
    // if the edid is not valid for disp id then SetLwstomEdid.
    // This is needed on emulation / simulation / silicon (when kvm used)
    //
    for (UINT32 dsp=0; dsp < workingSet.size(); dsp ++)
    {
        if ( DTIUtils::EDIDUtils::IsValidEdid(m_pDisplay, workingSet[dsp]) &&
             DTIUtils::EDIDUtils::EdidHasResolutions(m_pDisplay, workingSet[dsp])
           )
            continue;

        edidFileName   = "";
        rc =  m_pDisplay->GetDisplayType(workingSet[dsp], &type);
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "ERROR: %s: GetDisplayType failed with RC =%d.",
                __FUNCTION__,(UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            goto Cleanup;
        }

        if (type == Display::CRT &&
            crtTotalFakeEdidCount > 0)
        {
            edidFileName   += string("edids/");
            edidFileName   += DTIUtils::Mislwtils::trim(
                m_CRTFakeEdidFileList[
                    (crtDispIdCount++) % crtTotalFakeEdidCount]);
        }
        else if(type == Display::DFP &&
            dfpTotalFakeEdidCount > 0 )
        {
            edidFileName   += string("edids/");
            edidFileName   += DTIUtils::Mislwtils::trim(
                m_DFPFakeEdidFileList[
                    (dfpDispIdCount++) % dfpTotalFakeEdidCount]);
        }

        rc = DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, workingSet[dsp], true, edidFileName);
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "ERROR: %s: SetLwstomEdid(0x%X) failed with RC =%d.",
                __FUNCTION__, (UINT32)workingSet[dsp], (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            goto Cleanup;
        }
    }

    //If !m_onlyConnectedDisplays then consider fake displays too
    if (!m_onlyConnectedDisplays)
    {
        //get all the available displays
        DisplayIDs supported;
        CHECK_RC_CLEANUP(pDisplay->GetConnectors(&supported));
        m_fakedDispIds.clear();

        dfpDispIdCount = 0;
        crtDispIdCount = 0;

        for (UINT32 disp = 0; disp < supported.size(); disp++)
        {
            if (!pDisplay->IsDispAvailInList(supported[disp], Detected) &&
                DTIUtils::VerifUtils::IsSupportedProtocol(pDisplay, supported[disp], m_protocol))
            {
                edidFileName   = "";

                rc =  m_pDisplay->GetDisplayType(supported[disp], &type);
                if(rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                            "ERROR: %s: GetDisplayType failed with RC =%d.",
                            __FUNCTION__,(UINT32)rc);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    goto Cleanup;
                }

                if (type == Display::CRT &&
                    crtTotalFakeEdidCount > 0)
                {
                    edidFileName   += string("edids/");
                    edidFileName   += DTIUtils::Mislwtils::trim(
                                        m_CRTFakeEdidFileList[
                                        crtDispIdCount++ % crtTotalFakeEdidCount]);
                }
                else if(type == Display::DFP &&
                    dfpTotalFakeEdidCount > 0 )
                {
                    edidFileName   += string("edids/");
                    edidFileName   += DTIUtils::Mislwtils::trim(
                                        m_DFPFakeEdidFileList[
                                        dfpDispIdCount++ % dfpTotalFakeEdidCount]);
                }

                rc = DTIUtils::EDIDUtils::SetLwstomEdid(pDisplay, supported[disp],
                                                        false,              /*fake display*/
                                                        edidFileName);
                if(rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: DTIUtils::EDIDUtils::SetLwstomEdid() failed faking DisplayId 0x%X "
                        " with RC =%d. So ignoring this Display Id.",
                        __FUNCTION__,(UINT32)supported[disp],(UINT32)rc);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    rc.Clear();
                    continue;
                }

                // Lets Read back the EDID.
                rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                                         supported[disp],
                                                        &ListedModes,
                                                         false);

                for (modesCountInt = 0; modesCountInt < (UINT32)ListedModes.size(); modesCountInt++)
                {
                    m_reportStr = Utility::StrPrintf(
                            "\nMods-DisplayId [0x%X]:Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d;",
                           (UINT32)supported[disp], modesCountInt,
                           ListedModes[modesCountInt].width,ListedModes[modesCountInt].height,
                           ListedModes[modesCountInt].depth,ListedModes[modesCountInt].refreshRate);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                }

                if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
                {
                    // Lets Read back the EDID.
                    rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                                             supported[disp],
                                                            &ListedModes,
                                                             true);

                    for (modesCountInt = 0; modesCountInt < (UINT32)ListedModes.size(); modesCountInt++)
                    {
                        m_reportStr = Utility::StrPrintf(
                                "\nWindows-DisplayId [0x%X]:Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d;",
                               (UINT32)supported[disp], modesCountInt,
                               ListedModes[modesCountInt].width,ListedModes[modesCountInt].height,
                               ListedModes[modesCountInt].depth,ListedModes[modesCountInt].refreshRate);
                        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    }
                }

                m_fakedDispIds.push_back(supported[disp]);
                workingSet.push_back(supported[disp]);

                // Sleep for attach delay
                m_reportStr = Utility::StrPrintf(
                        "%s: Sleeping for Attach Delay:%d seconds\n",__FUNCTION__,
                        g_IMPMarketingParameters.testLevelConfig.attachDelay);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                Sleep(g_IMPMarketingParameters.testLevelConfig.attachDelay * 1000, m_runOnEmu, GetBoundGpuSubdevice());
            }
        }

        Printf(Tee::PriHigh, "\n Faked displays =\n");
        m_pDisplay->PrintDisplayIds(Tee::PriHigh, m_fakedDispIds);
    }

    // m_autoGenerateEdid is valid only for faked displays
    if (m_fakedDispIds.size() == 0 && m_autoGenerateEdid)
    {
        m_reportStr = Utility::StrPrintf(
            "WARNING: %s: Setting m_autoGenerateEdid = false, as there is NO fake dispId available.\n",
            __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        m_autoGenerateEdid = false;
    }

    // WAR: Let's remove those displayIds whose GetListedMode fails.
    // This is needed since @ bringup using KVM we can't read edid for KVM.
    // So we must ignore those display ids for our testing
    // else test might fail further down when trying to read edid for those dispIds.
    // Here we have delibertately kept last argument of GetListedModes() as false
    // since we want to verify GetListedModes() using MODS EDID library
    for(LwS32 disp = (LwS32)(workingSet.size()) -1; disp >=0 ; disp-- )
    {
        rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                                 workingSet[disp],
                                                &ListedModes,
                                                false);
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "ERROR: %s: DTIUtils::EDIDUtils::GetListedModes() failed for Display 0x%X "
                " with RC =%d. So ignoring this Display Id.",
                __FUNCTION__,(UINT32)workingSet[disp],(UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            workingSet.erase(workingSet.begin() + disp);
            rc.Clear();
        }
        else
        {
            // Let's retrieve and print the detected displays modes list
            m_reportStr = Utility::StrPrintf(
                "\nINFO: %s: Detected DisplayId[0x%X] supports required protocol.\nPrinting its modes list.\n",
                __FUNCTION__, (UINT32)workingSet[disp]);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            for (modesCountInt = 0; modesCountInt < (UINT32)ListedModes.size(); modesCountInt++)
            {
                m_reportStr = Utility::StrPrintf(
                    "\nMods-DisplayId [0x%X]:Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d;",
                    (UINT32)workingSet[disp], modesCountInt,
                    ListedModes[modesCountInt].width,ListedModes[modesCountInt].height,
                    ListedModes[modesCountInt].depth,ListedModes[modesCountInt].refreshRate);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            }

            if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP && !m_runOnEmu)
            {
                // Let's read back the EDID.
                rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                    workingSet[disp],
                    &ListedModes,
                    true);

                for (modesCountInt = 0; modesCountInt < (UINT32)ListedModes.size(); modesCountInt++)
                {
                    m_reportStr = Utility::StrPrintf(
                        "\nWindows-DisplayId [0x%X]:Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d;",
                        (UINT32)workingSet[disp], modesCountInt,
                        ListedModes[modesCountInt].width,ListedModes[modesCountInt].height,
                        ListedModes[modesCountInt].depth,ListedModes[modesCountInt].refreshRate);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                }
            }
        }
    }

    //get the no of displays to work with
    numDisps = static_cast<UINT32>(workingSet.size());
    if (numDisps == 0)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: No displays detected, test aborting...RC= SOFTWARE_ERROR\n",__FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        rc = RC::SOFTWARE_ERROR;
        goto Cleanup;
    }

    CHECK_RC_CLEANUP(configTestSettings());

    // if "-repro '...'" cmdline is passed then just proceed with the following and return
    if (m_repro.compare("") != 0)
    {
        vector <string> eachReproStep;
        std::transform(m_repro.begin(), m_repro.end(), m_repro.begin(), DTIUtils::VerifUtils::upper);

        eachReproStep = DTIUtils::VerifUtils::Tokenize(m_repro, ";");
        SetImageConfig *pReproConfig;

        for (UINT32 i = 0; i < eachReproStep.size(); i++)
        {
            pReproConfig = new SetImageConfig();
            CHECK_RC_CLEANUP(getConfigFromString(eachReproStep[i], pReproConfig));
            CHECK_RC_CLEANUP(runReproConfig(pReproConfig));
            delete pReproConfig;
        }

        goto Cleanup;
    }

#ifdef INCLUDE_MDIAG
    TestRunningOnMRMI = MrmCppInfra::TestsRunningOnMRMI();
    if (TestRunningOnMRMI)
    {
        CHECK_RC_CLEANUP(MrmCppInfra::AllocEvent("Modeset"));
        CHECK_RC_CLEANUP(MrmCppInfra::AllocEvent("ClockSwitch"));
        CHECK_RC_CLEANUP(MrmCppInfra::AllocEvent("TraceDone"));
        CHECK_RC_CLEANUP(MrmCppInfra::AllocEvent("ClockSwitchDone"));
        CHECK_RC_CLEANUP(MrmCppInfra::AllocEvent("ModesetTestDone"));
    }
#endif

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    HINSTANCE hDWM = LoadLibrary("dwmapi.dll");
    if( NULL != hDWM)
    {
        HRESULT (WINAPI*DwmIsCompositionEnabled)(BOOL*);
        DwmIsCompositionEnabled = (HRESULT (WINAPI*)(BOOL*))GetProcAddress(hDWM, "DwmIsCompositionEnabled");
        if (DwmIsCompositionEnabled)
        {
            if (S_OK != DwmIsCompositionEnabled(&bAeroResult))
            {
                m_reportStr = Utility::StrPrintf(
                        "INFO : %s: Call to DwmIsCompositionEnabled failed.\n",
                        __FUNCTION__);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                rc = RC::SOFTWARE_ERROR;
                goto Cleanup;
            }
        }
        FreeLibrary(hDWM);
    }

    // We measure mclk utilization before starting apps and both mclk and LwClk utilization after starting each app,
    // to verify that the app is running.  Some apps (such as 3DMarkV) disable Aero when they run.
    // We want the utilization comparison to be consistent, so we want Aero to be disabled for
    // the baseline mclk measurement (taken before starting the app), as well.
    // We disable Aero here and leave it disabled for the entire test.
    if (bAeroResult)
    {
        CHECK_RC_CLEANUP(HandleAero(false));
    }
#endif

    if(m_runfrominputfile)
    {
        rc = TestInputConfigFile(workingSet);
        if( rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: TestInputConfigFile() failed with RC = %d\n",
                __FUNCTION__, (UINT32)rc);
        }
    }
    else
    {
        rc = TestAllEdidResCombi(workingSet);
        if (rc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: TestAllEdidResCombi() failed with RC = %d\n",
                __FUNCTION__, (UINT32)rc);
        }
    }

Cleanup:
#ifdef INCLUDE_MDIAG
    if(TestRunningOnMRMI)
    {
        if (!m_bOnetimeSetupDone)
        {
                m_reportStr = Utility::StrPrintf(
                    "INFO: %s: RM Setup is done. Waiting for MRMI setup completion.\n ",
                    __FUNCTION__);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                m_bOnetimeSetupDone = true;

                if ((rc = MrmCppInfra::SetupDoneWaitingForOthers("RM")) != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                        "INFO: %s: SetupDoneWaitingForOthers(RM) failed with RC = %d. Skipping setmode and going to next config.\n ",
                        __FUNCTION__, (UINT32)rc);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                }
        }

        //Mdiag might have entered waiting for these before setting "ModesetTestDone". So setting them again to unblock mdiag.
        CHECK_RC_CLEANUP(MrmCppInfra::SetEvent("Modeset"));
        CHECK_RC_CLEANUP(MrmCppInfra::SetEvent("ClockSwitch"));
        CHECK_RC_CLEANUP(MrmCppInfra::SetEvent("ClockSwitchDone"));
        CHECK_RC_CLEANUP(MrmCppInfra::SetEvent("ModesetTestDone"));
        CHECK_RC_CLEANUP(MrmCppInfra::TestDoneWaitingForOthers("RM"));

        CHECK_RC(MrmCppInfra::FreeEvent("Modeset"));
        CHECK_RC(MrmCppInfra::FreeEvent("ClockSwitch"));
        CHECK_RC(MrmCppInfra::FreeEvent("ClockSwitchDone"));
        CHECK_RC(MrmCppInfra::FreeEvent("ModesetTestDone"));
        CHECK_RC(MrmCppInfra::FreeEvent("TraceDone"));
    }
#endif

    cleanupRc = resetIMPParameters();
    if (cleanupRc != OK)
    {
        Printf(Tee::PriHigh, "ERROR: %s: resetIMPParameters() failed with rc = %d.\n",
                __FUNCTION__, (UINT32)cleanupRc);
    }

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    // restore Aero
    // no point disabling Aero again.
    // restore it back if it was enabled in the starting.
    if (bAeroResult)
    {
        cleanupRc = HandleAero(true);
        if (cleanupRc != OK)
        {
            Printf(Tee::PriHigh, "ERROR: %s: HandleAero() failed with RC = %d\n",
                    __FUNCTION__, (UINT32)cleanupRc);
        }
    }
#endif

    // returns the first !OK rc
    return rc;
}

#define DWM_EC_ENABLECOMPOSITION  1
#define DWM_EC_DISABLECOMPOSITION 0
//! function HandleAero()
//!         This function enables/disables Aero.
//! params bHandleAero  [in]    : boolean to tell whether to enable or disable Aero
//! Returns RC = OK, or an RC error code if call to set Aero fails.
RC HandleAero(bool bHandleAero)
{
    RC rc = OK;
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    // the API is not supported pre-vista
    HINSTANCE hDWM = LoadLibrary("dwmapi.dll");
    if (NULL != hDWM)
    {
        HRESULT (WINAPI*DwmEnableComposition)(UINT);
        DwmEnableComposition = (HRESULT (WINAPI*)(UINT))GetProcAddress(hDWM, "DwmEnableComposition");
        if (DwmEnableComposition)
        {
            if (S_OK != DwmEnableComposition(bHandleAero ? DWM_EC_ENABLECOMPOSITION :
                                                           DWM_EC_DISABLECOMPOSITION))
            {
                Printf(Tee::PriHigh, "ERROR: %s: Call to DwmEnableComposition failed.\n",
                        __FUNCTION__);
                rc = RC::SOFTWARE_ERROR;
            }
        }
        FreeLibrary(hDWM);
    }

#endif
    return rc;
}

//!---------------------------------------------------------------------------
//! \brief  sortDispIdsAndFillDispParams()
//! \param   workingset[in ]       : DisplayIDs that needs to be sorted based on highest PClk they support
//! \param   sortedWorkingSet[out] : vector of DispParams that will hold the edid resolutions and the max width and height each dispId can support
//! \return  RC::OK when successful else returns the RC error code
//! \sa TestInputConfigFile()
//!---------------------------------------------------------------------------
RC  Modeset::sortDispIdsAndFillDispParams(DisplayIDs workingset,
    vector<DispParams> &sortedWorkingSet,
    bool useWindowsResList)
{
    RC rc;
    UINT64 lwrrResFetchRate;
    UINT64 maxResFetchRate;
    INT32  maxModeIndex;
    vector<Display::Mode> ListedModes;

    sortedWorkingSet.clear();

    for(UINT32 dispCount = 0; dispCount < workingset.size(); dispCount++)
    {
        ListedModes.clear();
        maxResFetchRate = 0;
        maxModeIndex    = -1;

        rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                       workingset[dispCount],
                                      &ListedModes,
                                       useWindowsResList);
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "ERROR: %s: DTIUtils::EDIDUtils::GetListedModes() failed for Display 0x%X "
                " with RC =%d. So ignoring this Display Id.",
                __FUNCTION__,(UINT32)workingset[dispCount],(UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            continue;
        }

        DispParams dispParam;
        dispParam.dispId = workingset[dispCount];
        dispParam.modes  = ListedModes;

        for(UINT32 i = 0; i < (UINT32)ListedModes.size(); i++)
        {
            lwrrResFetchRate = (UINT64)ListedModes[i].width * ListedModes[i].height *
                                 ListedModes[i].refreshRate * ListedModes[i].depth;

            if (maxResFetchRate < lwrrResFetchRate)
            {
                maxModeIndex = i;
                maxResFetchRate = lwrrResFetchRate;
            }
        }

        if (-1 != maxModeIndex)
        {
            dispParam.largestMode = ListedModes[maxModeIndex];
        }

        sortedWorkingSet.push_back(dispParam);

    }

    sort(sortedWorkingSet.begin(), sortedWorkingSet.end(), DispParams::compare);
    reverse(sortedWorkingSet.begin(), sortedWorkingSet.end());
    return rc;
}

//!
//! \brief Selects the most stressful mode possible.
//!
//! This function creates a config (within requested parameters) that is
//! intended to be as stressful for IMP testing as possible.  "Stress" is
//! measured relative to the pstate at which the config is expected to run,
//! so a config that can run at P8 with very little margin will be more
//! stressful than another config that requires slightly more bandwidth, such
//! that it is forced to run at P5 (where it will have a lot more margin).
//!
//! Possible modes are enumerated from modes.js.  (If more stress configs,
//! i.e., configs with even lower margin, are desired, then more modes should
//! be added to modes.js.
//!
//! For multihead configs, for now, only configs where all heads have the same
//! resolution are considered.
//!
//! This test option lwrrently supports only DP ports.
//!
//! \param pAvailableDispIDs : Display IDs available for our use during the
//!                         test
//! \param activeDisplaysCount : The number of displays that we want to use for
//!                         this test
//! \param overlayCount   : The number of display on which we want to have
//!                         overlay enabled for this test
//! \param porPState      : The lowest pstate at which the config needs to be
//!                         able to run
//! \param porVPState     : The lowest vP-State at which the config needs to be
//!                         able to run
//! \param pWorstCaseModeSettings : Returns the most stressful config found to
//!                         the caller
//!
//! \return RC status
//!            OK - config was found
//!            ILWALID_DISPLAY_MASK - not enough DP ports
//!            MODE_IS_NOT_POSSIBLE - no possible configs (meeting the
//!                                   specified parameters) were found
//!            (Other values returned from subfunctions are also possible.)
//!
RC Modeset::GenerateWorstCaseConfig
(
    DisplayIDs              *pAvailableDispIDs,
    UINT32                  activeDisplaysCount,
    UINT32                  overlayCount,
    UINT32                  porPState,
    UINT32                  porVPState,
    vector<ModeSettings>    *pWorstCaseModeSettings
)
{
    RC                      rc;
    Display::Mode           displayMode;
    vector<Display::Mode>   lwrrCombiModes2Use;
    DisplayIDs              dispIdsForLwrrRun;
    vector<Display::Mode>   displayModes;
    vector<UINT32>          lwrrCombiHeads2Use;
    vector<UINT32>          lwrrCombiOvlyDepth;
    LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS impParams;
    UINT32                  worstCaseMargin = 0xFFFFFFFF;
    char                    worstCaseDomain[8];
    UINT32                  overlaysLeftToCount;
    UINT32                  dpDispIDsFound = 0;
    UINT32                  dispIdIndex;
    UINT32                  impMinPState;
    UINT32                  lwrrentDisplay;
    vector<ModeSettings>    modeSettings;
    bool                    bModeSatisfiesPor;
    vector<UINT32>          supportedIMPvPStates;

    //
    // Find all of the available DP disp IDs.  (For now, only DP ports are
    // supported, for simplicity.)
    //
    for (dispIdIndex = 0;
         dispIdIndex < pAvailableDispIDs->size();
         dispIdIndex++)
    {
        if (DTIUtils::VerifUtils::IsSupportedProtocol(
                m_pDisplay,
                (*pAvailableDispIDs)[dispIdIndex],
                "DP"))
        {
            dispIdsForLwrrRun.push_back((*pAvailableDispIDs)[dispIdIndex]);
            dpDispIDsFound++;
            if (dpDispIDsFound >= activeDisplaysCount)
            {
                break;
            }
        }
    }
    if (dpDispIDsFound < activeDisplaysCount)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: Not enough available DP ports.  "
                "Skipping this config\n",
                __FUNCTION__);

        PRINT_STDOUT_N_REPORT(Tee::PriError,
                              m_reportStr.c_str(),
                              PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::ILWALID_DISPLAY_MASK;
    }

    //
    // Loop through all possible modes.  For each mode, call IMP and callwlate
    // the IMP margin.  From the modes that are possible, choose the one with
    // the lowest margin, and return it to the caller to run the test.
    //
    map<string,ModeSettings>::iterator it;
    for (it = g_modeSettings.begin(); it != g_modeSettings.end(); it++)
    {

        lwrrCombiModes2Use.clear();
        lwrrCombiOvlyDepth.clear();
        modeSettings.clear();

        displayMode.width       = it->second.mode.width;
        displayMode.height      = it->second.mode.height;
        displayMode.depth       = it->second.mode.depth;
        displayMode.refreshRate = it->second.mode.refreshRate;

        overlaysLeftToCount = overlayCount;

        for (lwrrentDisplay = 0;
             lwrrentDisplay < activeDisplaysCount;
             lwrrentDisplay++)
        {
            modeSettings.push_back(it->second);
            lwrrCombiModes2Use.push_back(displayMode);

            if (overlaysLeftToCount)
            {
                lwrrCombiOvlyDepth.push_back(32);
                overlaysLeftToCount--;
            }
            else
            {
                lwrrCombiOvlyDepth.push_back(0);
            }
        }

        memset(&impParams, 0, sizeof(LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS));

        //
        // Call IMP to see if the mode is possible, and get the min pstate and
        // the margin.
        //
        rc = m_pDisplay->IMPQuery(dispIdsForLwrrRun,
                                  lwrrCombiModes2Use,
                                  lwrrCombiOvlyDepth,
                                  &impParams);

        if (rc == OK)
        {
            if (m_bIsPS30Supported)
            {
                impMinPState = static_cast<UINT32>(impParams.minPerfLevel);
                supportedIMPvPStates = m_objSwitchRandomPState.getIMPVirtualPStates();

                if (porVPState == IMPTEST3_VPSTATE_UNDEFINED)
                {
                    // TODO: Need to get the slowest vP-State, lwrrently assigning the
                    // the fastest vP-State instead.
                    porVPState = static_cast<UINT32>(supportedIMPvPStates.size() - 1);
                }
                else if (porVPState == IMPTEST3_VPSTATE_MAX)
                {
                    porVPState = static_cast<UINT32>(supportedIMPvPStates.size() - 1);
                }
                else if (porVPState == IMPTEST3_VPSTATE_MIN)
                {
                    porVPState = impMinPState;
                }
                bModeSatisfiesPor = ((!m_PStateSupport) ||
                                     (porVPState >= impMinPState));
                // we get the nearest vP-State
            }
            else
            {
                impMinPState = static_cast<UINT32>(impParams.MinPState);
                bModeSatisfiesPor = ((!m_PStateSupport) ||
                                     (porPState == LW2080_CTRL_PERF_PSTATES_UNDEFINED) ||
                                     (porPState == (UINT32) MinPState) ||
                                     (porPState == (UINT32) MaxPState) ||
                                     (porPState <= impMinPState));
            }

            if (bModeSatisfiesPor)
            {
                //
                // The mode is possible at the required pstate.  Check for
                // worst case margin.
                //
                if (worstCaseMargin > impParams.worstCaseMargin)
                {
                    //
                    // This is the most stressful mode we've found so far.
                    // Save it for later.
                    //
                    worstCaseMargin = impParams.worstCaseMargin;
                    memcpy(worstCaseDomain,
                           impParams.worstCaseDomain,
                           sizeof(worstCaseDomain));
                    *pWorstCaseModeSettings = modeSettings;
                }
                m_reportStr = Utility::StrPrintf(
                        "%s: IMP min perf state = %s   Extra margin = %d%% (%s)\n",
                        __FUNCTION__,
                        printPerfStateHelper(impMinPState).c_str(),
                        (UINT32) (((UINT64) impParams.worstCaseMargin * 100) /
                                  LW5070_CTRL_IMP_MARGIN_MULTIPLIER) - 100,
                        impParams.worstCaseDomain);
            }
            else
            {
                m_reportStr = Utility::StrPrintf(
                        "%s: Mode is rejected because IMP min perf state (%s)"
                        " exceeds config file pstate (%s)\n",
                        __FUNCTION__,
                        printPerfStateHelper(impMinPState).c_str(),
                        printPerfStateHelper(porPState).c_str());
            }
            PRINT_STDOUT_N_REPORT(Tee::PriNormal,
                                  m_reportStr.c_str(),
                                  PRINT_STDOUT | PRINT_LOG_FILE);
        }
        else if (rc != RC::MODE_IS_NOT_POSSIBLE)
        {
            //
            // If the mode is not possible, it's OK; we'll just try the next
            // mode.  But if some other error oclwrred, fail the test.
            //
            return rc;
        }
    }

    if (worstCaseMargin == 0xFFFFFFFF)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: No possible modes found\n",
                __FUNCTION__);
        rc = RC::MODE_IS_NOT_POSSIBLE;
    }
    else
    {
        m_reportStr = Utility::StrPrintf(
                "%s: Worst case margin = %d%% (%s)\n",
                __FUNCTION__,
                (UINT32) (((UINT64) impParams.worstCaseMargin * 100) /
                          LW5070_CTRL_IMP_MARGIN_MULTIPLIER) - 100,
                worstCaseDomain);
        rc = OK;
    }

    PRINT_STDOUT_N_REPORT(Tee::PriNormal,
                          m_reportStr.c_str(),
                          PRINT_STDOUT | PRINT_LOG_FILE);
    return rc;
}

//!---------------------------------------------------------------------------
//! \brief  TestInputConfigFile() : This method loops through all the configs given in the input
//!             javascript based config file and check if any of the following
//!             fails {IMP/ SetMode/Underflow occur/Pstate Switch fails}
//! \param   workingset[in ]       : DisplayIDs that needs to be used for SetMode operations
//! \return  RC::OK when successful else returns the RC error code
//! \sa sortDispIdsAndFillDispParams()
//!---------------------------------------------------------------------------
RC Modeset::TestInputConfigFile(DisplayIDs workingSet)
{
    RC rc, rcOverall = OK;
    vector<DispParams> sortedWorkingSet;
    vector<ModeSettings>        sortedModeSettings;
    vector<Display::Mode>       lwrrCombiModes2Use;
    DisplayIDs                  dispIdsForLwrrRun;
    DisplayIDs                  ActiveDisplays;
    UINT32                      activeDisplaysCount;
    UINT32                      lwrrCombiDispMask       = 0;
    UINT32                      Head                    = Display::ILWALID_HEAD;
    LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams;
    vector<UINT32>              lwrrCombiHeads2Use;
    vector<UINT32>              lwrrCombiOvlyDepth;
    UINT32                      gpuFound = 0;
    char                        commentStr[255];
    DisplayIDs                  lwrrActiveDisplays;
    UINT32                disp = 0;
    EvoDisplay           *pEvoDisplay;
    EvoDisplayChnContext *pCoreContext     = NULL;
    EvoCoreChnContext    *pCoreDispContext = NULL;
    ColorUtils::Format    format;
    LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS Params;
    bool modeSupportedOnMonitor = false;
    bool foundDispId = false;
    string  newEdidFileName = "";
    vector<Display::Mode> ListedModes;
    bool useWindowsResList;

    memset(&Params, 0, sizeof(LW5070_CTRL_CMD_IS_MODE_POSSIBLE_PARAMS));

    Printf(Tee::PriHigh, "\nINFO: %s: Beginning Testing input file with displayIDs =\n", __FUNCTION__);
    m_pDisplay->PrintDisplayIds(Tee::PriHigh, workingSet);

    if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
    {
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
        m_pDisplay->GetWin32Display()->SetIgnorePrevModesetDispID(true);
#endif
    }

    // 1) Find the edid resolution and store max width and heigth and disp id in struct dispMaxProp.
    // If autoGenerateEdid is enabled, then disable resolution querying using windows API,
    // as it needs additional modeset.
    useWindowsResList = m_autoGenerateEdid ? false : m_useWindowsResList;
    sortDispIdsAndFillDispParams(workingSet, sortedWorkingSet, useWindowsResList);

    PRINT_STDOUT_N_REPORT(Tee::PriHigh,
        "ConfigNo,TimeStamp,Width,Height,Depth,RefreshRate,ColorFormat,Interlaced,TimingType,Scaling,IMP_Status,SetMode_Status,Result,Comment",
        PRINT_STDOUT | PRINT_LOG_FILE  | PRINT_CSV_FILE);

    for(UINT32 configNo=0; configNo < g_IMPMarketingParameters.configList.size() ; configNo++)
    {
        // Deliberately adding this as 1st check so as to make the summary report less complex with only required configs printed.
        if(m_runOnlyConfigsList.size() &&
           !std::count(m_runOnlyConfigsList.begin(), m_runOnlyConfigsList.end(), configNo))
        {
            m_reportStr = Utility::StrPrintf(
                    "Config No = %d: skipped as it is not stated in '-runOnlyConfigs' cmd line.\n", configNo);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE );
            continue;
        }

        m_clockDiffFound          = false;

#ifdef INCLUDE_MDIAG
        m_bModesetEventSent         = false;
        m_bClockSwitchEventSent     = false;
        m_bClockSwitchDoneEventSent = false;
#endif

        sprintf(commentStr," ");
        m_reportStr = Utility::StrPrintf(
                "\nINFO: %s: Config No = %d: Testing Modes\n",__FUNCTION__, configNo);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE );

        char lwrrTime[30] = "";
        string dateFormat = "%Y-%m-%d-%H-%M-%S";
        DTIUtils::ReportUtils::GetLwrrTime(dateFormat.c_str(), lwrrTime, sizeof lwrrTime);

        //Write Mode list to the CSV Config file
        for(UINT32 modeNo = 0; modeNo < g_IMPMarketingParameters.configList[configNo].modesList.size(); modeNo++)
        {
            m_reportStr = Utility::StrPrintf(
                "\n%d,%s,%d,%d,%d,%d,%s,%s,%d,%s,",
                configNo,lwrrTime,
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].mode.width,
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].mode.height,
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].mode.depth,
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].mode.refreshRate,
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].colorFormat.c_str(),
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].interlaced ? "TRUE" : "FALSE",
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].timing,
                g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]].scaling ? "TRUE" : "FALSE"
                );
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE  | PRINT_CSV_FILE);
        }

        // check if this config is for this Gpu ?
        gpuFound = (UINT32) count (g_IMPMarketingParameters.configList[configNo].validForChip.begin(),
                                   g_IMPMarketingParameters.configList[configNo].validForChip.end(),
                                   (UINT32)m_pDisplay->GetOwningDisplaySubdevice()->DeviceId());

        if(!gpuFound)
        {
            // skipping this mode since it is not specified to be run on this GPU
            m_reportStr = Utility::StrPrintf(
                    "Config No = %d: Skipping current config since \"validForChip\" field doesnt have current GPU device=> Gpu.%s\n",
                    configNo,
                    Gpu::DeviceIdToInternalString(m_pDisplay->GetOwningDisplaySubdevice()->DeviceId()).c_str());

            outputCommentStr("Not Attempted,Not Attempted,FAIL,");
            goto endOfConfig;
        }

        // Sort desc the activeDisplayCount..Can be [2,4,3] and desc sort => [4,3,2]
        sort(   g_IMPMarketingParameters.configList[configNo].activeDisplaysCount.begin(), g_IMPMarketingParameters.configList[configNo].activeDisplaysCount.end());
        reverse(g_IMPMarketingParameters.configList[configNo].activeDisplaysCount.begin(), g_IMPMarketingParameters.configList[configNo].activeDisplaysCount.end());

        activeDisplaysCount = g_IMPMarketingParameters.configList[configNo].activeDisplaysCount[0];

        //Check if no of modes to run is <= numHeads
        if (m_numHeads < activeDisplaysCount)
        {
            m_reportStr = Utility::StrPrintf(
                "Config No = %d: ERROR: Active display count { %d} > headCount { %d}\n",
                configNo, activeDisplaysCount, m_numHeads);

            outputCommentStr("Not Attempted,Not Attempted,FAIL,");
            goto endOfConfig;
        }

        // Check if activeDisplayCount > 0
        if (activeDisplaysCount < 1)
        {
            m_reportStr = Utility::StrPrintf(
                    "Config No = %d: ERROR: Active display count { %d } must be greater then 0\n",
                    configNo, activeDisplaysCount);

            outputCommentStr("Not Attempted,Not Attempted,FAIL,");
            goto endOfConfig;
        }

        // Clear previous underflow state
        rc = SetUnderflowStateOnAllHead(LW5070_CTRL_CMD_UNDERFLOW_PROP_ENABLED_YES,
            LW5070_CTRL_CMD_UNDERFLOW_PROP_CLEAR_UNDERFLOW_YES,
            LW5070_CTRL_CMD_UNDERFLOW_PROP_MODE_RED);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "Config No = %d: %s: SetUnderflowStateOnAllHead() Failed. RC = %d\n ",
                    configNo, __FUNCTION__, (UINT32)rc);

            outputCommentStr("Not Attempted,Not Attempted,FAIL,");
            goto endOfConfig;
        }

        sortedModeSettings.clear();
        if (g_IMPMarketingParameters.configList[configNo].modesList.size())
        {
            // Check if modeList size >= activeDisplaysCount
            if (static_cast<UINT32>(g_IMPMarketingParameters.configList[configNo].modesList.size()) <
                activeDisplaysCount)
            {
                m_reportStr = Utility::StrPrintf(
                        "Config No = %d: ERROR: activeDisplaysCount {%d} must be <= no of modes {%d}.\n",
                        configNo, activeDisplaysCount,
                        (UINT32)g_IMPMarketingParameters.configList[configNo].modesList.size());

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }

            // If modes are specified in the config, use them.
            for(UINT32 modeNo = 0; modeNo < g_IMPMarketingParameters.configList[configNo].modesList.size(); modeNo++)
            {
                sortedModeSettings.push_back(g_modeSettings[g_IMPMarketingParameters.configList[configNo].modesList[modeNo]]);
            }
        }
        else
        {
            //
            // If no modes are specified in the config, try to generate worst
            // case stress mode(s) automatically.
            //
            rc = GenerateWorstCaseConfig(
                &workingSet,
                activeDisplaysCount,
                g_IMPMarketingParameters.configList[configNo].overlayCount,
                g_IMPMarketingParameters.configList[configNo].porPState,
                g_IMPMarketingParameters.configList[configNo].porVPState,
                &sortedModeSettings);
            // Print the Mode Settings
            for (UINT32 i = 0; i < sortedModeSettings.size(); i++)
            {
                m_reportStr = Utility::StrPrintf(
                    "\n%d,%s,%d,%d,%d,%d,%s,%s,%d,%s,",
                    configNo,lwrrTime,
                    sortedModeSettings[i].mode.width,
                    sortedModeSettings[i].mode.height,
                    sortedModeSettings[i].mode.depth,
                    sortedModeSettings[i].mode.refreshRate,
                    sortedModeSettings[i].colorFormat.c_str(),
                    sortedModeSettings[i].interlaced ? "TRUE" : "FALSE",
                    sortedModeSettings[i].timing,
                    sortedModeSettings[i].scaling ? "TRUE" : "FALSE"
                    );
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE  | PRINT_CSV_FILE);
            }

            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "%s: ERROR: GenerateWorstCaseConfig returned %d\n",
                        __FUNCTION__,
                        (UINT32) rc);
                m_pDisplay->PrintDisplayIds(Tee::PriHigh, ActiveDisplays);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }
        }

        if(!g_IMPMarketingParameters.testLevelConfig.forceModeOnSpecificProtocol &&
           !g_IMPMarketingParameters.configList[configNo].forceModeOnSpecificProtocol )
        {
            // It is upto us to best fit the Mode to available monitors
            // so sort the modesetting vector desc by width and height and
            // then assign highest mode to highest monitor available
            sort(sortedModeSettings.begin(), sortedModeSettings.end(), ModeSettings::compare);
            reverse(sortedModeSettings.begin(), sortedModeSettings.end());
        }

        // Check if sufficient no of displays are connected
        if (sortedWorkingSet.size() < activeDisplaysCount)
        {
            Printf(Tee::PriHigh, "\nINFO: %s: SortedWorkingSet has only following DisplayIDs =\n", __FUNCTION__);
            for(UINT32 countSortedDispIDs = 0; countSortedDispIDs < sortedWorkingSet.size(); countSortedDispIDs++)
            {
                Printf(Tee::PriHigh, "\n DisplayIDs = 0x%X",
                    (UINT32)sortedWorkingSet[countSortedDispIDs].dispId);
            }

            m_reportStr = Utility::StrPrintf(
                "Config No = %d: ERROR: Pls connect %d no of displays.\n",
                configNo, activeDisplaysCount);

            outputCommentStr("Not Attempted,Not Attempted,FAIL,");
            goto endOfConfig;
        }

        // Detach all the selected display to get all the heads free for this new combination on Linux
        ActiveDisplays.clear();
        m_pDisplay->GetScanningOutDisplays(&ActiveDisplays);

        Printf(Tee::PriHigh, "\nINFO: %s: Active DisplayIDs=\n", __FUNCTION__);
        m_pDisplay->PrintDisplayIds(Tee::PriHigh, ActiveDisplays);

        if(ActiveDisplays.size() >= 1 && m_pDisplay->GetDisplayClassFamily() == Display::EVO)
        {
            Printf(Tee::PriHigh, "\nINFO: %s: Detaching DisplayIDs=\n", __FUNCTION__);
            m_pDisplay->PrintDisplayIds(Tee::PriHigh, ActiveDisplays);

            rc = m_pDisplay->DetachDisplay(ActiveDisplays);

            if(rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "%s: ERROR: DetachDisplay failed to detach displays. RC =%d\n",
                        __FUNCTION__, (UINT32)rc);
                m_pDisplay->PrintDisplayIds(Tee::PriHigh, ActiveDisplays);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }
            m_reportStr = Utility::StrPrintf(
                    "INFO: %s: Sleeping for Detach Delay:%d seconds\n", __FUNCTION__, g_IMPMarketingParameters.testLevelConfig.detachDelay);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            Sleep(g_IMPMarketingParameters.testLevelConfig.detachDelay * 1000, m_runOnEmu, GetBoundGpuSubdevice());
        }

        // Now considering only the highest activeDisplaysCount.(say 4 when activeDisplaysCount= [2,3,4])
        // In future we can respected other lower activeDisplaysCount values
        foundDispId = false;
        dispIdsForLwrrRun.clear();

        if (m_autoGenerateEdid)
        {
            char        buffer[33];
            vector<Display::Mode>  modesList;

            sprintf(buffer, "%d", configNo);
            newEdidFileName  = "EDID_" + m_testStartTime;
            newEdidFileName += string("_configNo_") + string(buffer) + ".txt";

            for (UINT32 lwrrMode = 0; lwrrMode < (UINT32)sortedModeSettings.size(); lwrrMode++)
            {
                modesList.push_back(sortedModeSettings[lwrrMode].mode);
            }

            char errorStr[256];
            rc = DTIUtils::EDIDUtils::CreateEdidFileWithModes(m_pDisplay,
                                    m_fakedDispIds[0],                  // Pass the 1st Fake disp from it.
                                    modesList,                          // Modes needed for this config.
                                    m_referenceEdidFileName_DFP,        // EDID file to use for reference.
                                    newEdidFileName,
                                    errorStr);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "Config No = %d: ERROR: while auto generating EDID for this config.",
                    configNo);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }

            for (disp = 0; disp < m_fakedDispIds.size(); disp++)
            {
                rc = DTIUtils::EDIDUtils::SetLwstomEdid(m_pDisplay, m_fakedDispIds[disp],
                                                        true,              /*overwrite EDID*/
                                                        newEdidFileName);
                if(rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: DTIUtils::EDIDUtils::SetLwstomEdid() failed overwriting new EDID on DisplayId 0x%X "
                        " with RC =%d. So ignoring this Display Id.",
                        __FUNCTION__,(UINT32)m_fakedDispIds[disp],(UINT32)rc);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    rc.Clear();
                    continue;
                }

                // Let's read back the EDID.
                m_reportStr = Utility::StrPrintf(
                        "INFO: %s: AUTO Generated EDID applied on DisplayId 0x%X."
                        " Printing new modes list.\n",
                        __FUNCTION__, (UINT32)m_fakedDispIds[disp]);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

                rc = DTIUtils::EDIDUtils::GetListedModes(m_pDisplay,
                                                         m_fakedDispIds[disp],
                                                         &ListedModes,
                                                         false);
                if (rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                        "ERROR: GetListedModes(0x%X) failed to retrieve resolution. RC=%d.",
                        (UINT32)m_fakedDispIds[disp], (UINT32)rc);

                    outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                    goto endOfConfig;
                }

                for (UINT32 modesCountInt = 0; modesCountInt < (UINT32)ListedModes.size(); modesCountInt++)
                {
                    m_reportStr = Utility::StrPrintf(
                            "Mods-DisplayId [0x%X]:Mode[%d]: Width=%d; Height=%d; Depth=%d; RefreshRate=%d;\n",
                           (UINT32)m_fakedDispIds[disp], modesCountInt,
                           ListedModes[modesCountInt].width, ListedModes[modesCountInt].height,
                           ListedModes[modesCountInt].depth, ListedModes[modesCountInt].refreshRate);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                }

                // Sleep for attach delay
                m_reportStr = Utility::StrPrintf(
                    "\nINFO: %s: Sleeping for Attach Delay: %d seconds\n",__FUNCTION__,
                    g_IMPMarketingParameters.testLevelConfig.attachDelay);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                Tasker::Sleep(g_IMPMarketingParameters.testLevelConfig.attachDelay * 1000);
            }
        }

        if(g_IMPMarketingParameters.testLevelConfig.forceModeOnSpecificProtocol ||
           g_IMPMarketingParameters.configList[configNo].forceModeOnSpecificProtocol )
        {
            vector<string> forceProtocolSequence =
                g_IMPMarketingParameters.configList[configNo].forceModeOnSpecificProtocol ?
                g_IMPMarketingParameters.configList[configNo].forceProtocolSequence :
                g_IMPMarketingParameters.testLevelConfig.forceProtocolSequence;

            // Check if forceProtocolSequence > = activeDisplaysCount
            if( static_cast<UINT32>(forceProtocolSequence.size()) <
                activeDisplaysCount)
            {
                m_reportStr = Utility::StrPrintf(
                    "Config No = %d: ERROR: forceProtocolSequence {%d} in input file "
                    "must be >= no of activeDisplaysCount {%d} in input file.\n",
                    configNo, (UINT32)forceProtocolSequence.size(),
                    activeDisplaysCount);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }

            for (UINT32 pushDispIds = 0; pushDispIds < activeDisplaysCount; pushDispIds++)
            {
                foundDispId = false;
                for(UINT32 dispIDLooper=0; dispIDLooper < workingSet.size(); dispIDLooper++)
                {
                    bool DoesDispIDSupportReqProtocol = DTIUtils::VerifUtils::IsSupportedProtocol(m_pDisplay,
                        workingSet[dispIDLooper],
                        forceProtocolSequence[pushDispIds]);
                    if(DoesDispIDSupportReqProtocol &&
                        !m_pDisplay->IsDispAvailInList(workingSet[dispIDLooper], dispIdsForLwrrRun))
                    {
                        dispIdsForLwrrRun.push_back(workingSet[dispIDLooper]);
                        foundDispId = true;
                        break;
                    }
                }
                if(!foundDispId)
                {
                    m_reportStr = Utility::StrPrintf(
                            "%s: ERROR: Failed to find a unused dispID having protocol= %s. skipping this mode\n",
                            __FUNCTION__,
                            forceProtocolSequence[pushDispIds].c_str());

                    outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                    goto endOfConfig;
                }
            }
        }
        else
        {
            for (UINT32 pushDispIds = 0; pushDispIds < activeDisplaysCount; pushDispIds++)
                dispIdsForLwrrRun.push_back(sortedWorkingSet[pushDispIds].dispId);
        }

        // Check if the monitor supports that modes
        for (UINT32 dispIds = 0; dispIds < activeDisplaysCount; dispIds++)
        {
            modeSupportedOnMonitor = false;

            // When using -autoGenerateEdid cmdline, re-read modes from the EDID
            // as we have just recently overwritten the EDID.
            // Else refer to cached array stored in sortedWorkingSet.
            // This is needed as we can't use windows API to get windows modeslist unless
            // that displayID is active. This operation needs additional modeset, which delays emulation.
            if (m_autoGenerateEdid)
            {
                rc = DTIUtils::VerifUtils::IsModeSupportedOnMonitor(m_pDisplay,
                                        sortedModeSettings[dispIds].mode,
                                        dispIdsForLwrrRun[dispIds],
                                        &modeSupportedOnMonitor,
                                        false);
            }
            else
            {
                rc = IsModeSupported(sortedModeSettings[dispIds].mode,
                                          dispIdsForLwrrRun[dispIds],
                                          sortedWorkingSet,
                                          &modeSupportedOnMonitor);
            }

            if(rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: IsModeSupported() failed with RC =%d.",
                       __FUNCTION__,(UINT32)rc);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }

            if(!modeSupportedOnMonitor)
            {
                m_reportStr = Utility::StrPrintf(
                    "%s: ERROR: Monitor 0x%X doesnot support mode {Width:%d; Height: %d; Depth:%d; RefreshRate:%d}. Skipping this Config\n",
                    __FUNCTION__,
                    (UINT32)dispIdsForLwrrRun[dispIds],
                    sortedModeSettings[dispIds].mode.width,
                    sortedModeSettings[dispIds].mode.height,
                    sortedModeSettings[dispIds].mode.depth,
                    sortedModeSettings[dispIds].mode.refreshRate
                    );
                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }
        }

        // Find Heads for this new Combination.Call the GetHeadRoutingMask.
        m_pDisplay->DispIDToDispMask(dispIdsForLwrrRun, lwrrCombiDispMask);
        memset(&dispHeadMapParams, 0,
               sizeof (LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS));
        dispHeadMapParams.subDeviceInstance     = 0;
        dispHeadMapParams.displayMask           = lwrrCombiDispMask;
        dispHeadMapParams.oldDisplayMask        = 0;
        dispHeadMapParams.oldHeadRoutingMap     = 0;
        dispHeadMapParams.headRoutingMap        = 0;
        rc = DTIUtils::Mislwtils::GetHeadRoutingMask(m_pDisplay, &dispHeadMapParams);

        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetHeadRoutingMask(0x%X) Failed with RC = %d."
                    "Skipping this mode\n",
                    __FUNCTION__, lwrrCombiDispMask, (UINT32)rc);

            outputCommentStr("Not Attempted,Not Attempted,FAIL,");
            goto endOfConfig;
        }

        m_reportStr = "\n############################################\n";
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE );
        m_reportStr = Utility::StrPrintf(
                "INFO: %s: Testing Display Combinations DisplayIDs:\n", __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE );

        lwrrCombiHeads2Use.clear();
        lwrrCombiModes2Use.clear();
        lwrrCombiOvlyDepth.clear();

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
       Win32Context** ppWin32Context  = (Win32Context **)m_pDisplay->GetLwstomContext();
       PathSettings *pPathSettings    = &((*ppWin32Context)->DispLwstomSettings.pathSettings);
       Win32Display::ResetPathSettings(pPathSettings, m_numHeads);
#endif

        // Respect other Modesettings Parameters and Prepare SetMode Params
        for (disp = 0; disp < activeDisplaysCount; disp ++)
        {
            m_reportStr = Utility::StrPrintf(
                    "   0x%08x    ", (UINT32)dispIdsForLwrrRun[disp]);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            lwrrCombiModes2Use.push_back(sortedModeSettings[disp].mode);

            if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask(dispIdsForLwrrRun[disp], dispHeadMapParams, &Head)) != OK)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetHeadFromRoutingMask Failed to Get Head for DispID = 0x%08X. RC = %d\n ",
                    __FUNCTION__, (UINT32)dispIdsForLwrrRun[disp], (UINT32)rc);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }
            lwrrCombiHeads2Use.push_back(Head);

            // respect the overlayCount settings from the input file
            if(disp < g_IMPMarketingParameters.configList[configNo].overlayCount)
            {
                lwrrCombiOvlyDepth.push_back(sortedModeSettings[disp].overlayDepth);
            }
            else
            {
                lwrrCombiOvlyDepth.push_back(0);
            }

            // respect timing parameter
            EvoRasterSettings RasterSettings;
            rc = m_pDisplay->SetupEvoLwstomTimings
                (dispIdsForLwrrRun[disp],
                sortedModeSettings[disp].mode.width,
                sortedModeSettings[disp].mode.height,
                sortedModeSettings[disp].mode.refreshRate,
                (Display::LwstomTimingType)sortedModeSettings[disp].timing,
                &RasterSettings,
                Head,
                0,
                NULL);

            if(rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: GetHeadRoutingMask(0x%X) Failed with RC = %d."
                        "Skipping this mode\n",
                        __FUNCTION__, lwrrCombiDispMask, (UINT32)rc);

                outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                goto endOfConfig;
            }

            if (sortedModeSettings[disp].colorFormat.compare("") == 0 ||
                sortedModeSettings[disp].colorFormat.compare("LWFMT_NONE") == 0 )
            {
                format = ColorUtils::ColorDepthToFormat(sortedModeSettings[disp].mode.depth);
            }
            else
            {
                format = ColorUtils::StringToFormat(sortedModeSettings[disp].colorFormat);
            }

            if (m_pDisplay->GetDisplayClassFamily() == Display::EVO)
            {
                ImageUtils imgArr;
                pEvoDisplay                = m_pDisplay->GetEvoDisplay();
                pCoreContext     = pEvoDisplay->GetEvoDisplayChnContext(dispIdsForLwrrRun[disp], Display::CORE_CHANNEL_ID);
                pCoreDispContext = (EvoCoreChnContext *)pCoreContext;
                EvoCoreDisplaySettings &LwstomSettings = pCoreDispContext->DispLwstomSettings;

                // respect interlaced parameter
                LwstomSettings.HeadSettings[Head].RasterSettings.Interlaced = sortedModeSettings[disp].interlaced;
                LwstomSettings.HeadSettings[Head].RasterSettings.Dirty      = true;

                // respect scaling parameter
                if(sortedModeSettings[disp].scaling)
                {
                    LwstomSettings.HeadSettings[Head].ScalerSettings.ScalingActive   = sortedModeSettings[disp].scaling;
                    LwstomSettings.HeadSettings[Head].ScalerSettings.HorizontalTaps  = EvoScalerSettings::HTAPS_2;
                    LwstomSettings.HeadSettings[Head].ScalerSettings.VeriticalTaps   = EvoScalerSettings::VTAPS_3;
                    LwstomSettings.HeadSettings[Head].ScalerSettings.Dirty           = true;
                }

                imgArr = ImageUtils::SelectImage(sortedModeSettings[disp].mode.width,
                    sortedModeSettings[disp].mode.height);

                //Free the core image else SetupChannelImage will throw RC error
                if(m_channelImage[disp].GetMemHandle() != 0)
                {
                    m_channelImage[disp].Free();
                }

                // respect colorFormat and SetupChannelImage
                rc = m_pDisplay->SetupChannelImage(dispIdsForLwrrRun[disp],
                    sortedModeSettings[disp].mode.width,
                    sortedModeSettings[disp].mode.height,
                    sortedModeSettings[disp].mode.depth,
                    Display::CORE_CHANNEL_ID,
                    &m_channelImage[disp],
                    imgArr.GetImageName(),
                    0,
                    lwrrCombiHeads2Use[disp],
                    0,
                    0,
                    Surface2D::BlockLinear,
                    format);

                if (rc != OK )
                {
                    if(rc == RC::ILWALID_HEAD)
                    {
                        m_reportStr = Utility::StrPrintf(
                                "ERROR: %s: SetupChannelImage() Failed for displayID=%X. RC = ILWALID_HEAD\n ",
                               __FUNCTION__, (UINT32)dispIdsForLwrrRun[disp]);
                    }
                    else
                    {
                        m_reportStr = Utility::StrPrintf(
                                "ERROR: %s: SetupChannelImage() Failed for displayID=%X. RC = %d\n ",
                                __FUNCTION__, (UINT32)dispIdsForLwrrRun[disp], (UINT32)rc);
                    }

                    outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                    goto endOfConfig;
                }
            }
            else if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
            {
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
                UINT32 lwApiDisplayId;
                UINT32 foundPathAt = 0;
                UINT32 foundTargetPathAt = 0;
                bool foundTarget = false;
                Win32Display *pWin32Display = m_pDisplay->GetWin32Display();

                //Get the index of path and targetInfo
                rc = pWin32Display->GetPathAndTargetIndexFromOutputID(dispIdsForLwrrRun[disp],
                    (void *)pPathSettings->pPathInfo,
                    pPathSettings->pathCount,
                    &foundPathAt,
                    &foundTargetPathAt,
                    &foundTarget);

                if(rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                            "ERROR: %s: GetPathAndTargetIndexFromOutputID(0x%X) Failed with RC = %d."
                            "Skipping this mode\n",
                            __FUNCTION__, dispIdsForLwrrRun[disp], (UINT32)rc);

                    outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                    goto endOfConfig;
                }

                rc = GetDisplayIdFromGpuAndOutputId(m_pDisplay->GetOwningDisplaySubdevice()->GetGpuId(),
                    dispIdsForLwrrRun[disp],&lwApiDisplayId);

                if(rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                            "ERROR: %s: GetDisplayIdFromGpuAndOutputId(0x%X) Failed with RC = %d."
                            "Skipping this mode\n",
                            __FUNCTION__, dispIdsForLwrrRun[disp], (UINT32)rc);

                    outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                    goto endOfConfig;
                }

                pPathSettings->pPathInfo[foundPathAt].targetInfo[foundTargetPathAt].displayId           = lwApiDisplayId;
                pPathSettings->dirty[foundPathAt].targetInfo[foundTargetPathAt].displayId               = true;

                // respect interlaced parameter
                pPathSettings->pPathInfo[foundPathAt].targetInfo[foundTargetPathAt].details->interlaced = sortedModeSettings[disp].interlaced;
                pPathSettings->dirty[foundPathAt].targetInfo[foundTargetPathAt].details->interlaced     = true;

                // respect scaling parameter
                if (sortedModeSettings[disp].scaling)
                {
                    pPathSettings->pPathInfo[foundPathAt].targetInfo[foundTargetPathAt].details->scaling = LW_SCALING_ADAPTER_SCALING;
                }
                else
                {
                    pPathSettings->pPathInfo[foundPathAt].targetInfo[foundTargetPathAt].details->scaling = LW_SCALING_MONITOR_SCALING;
                }

                if ((rc = pWin32Display->CheckScalingCaps(dispIdsForLwrrRun[disp],
                                            pPathSettings->pPathInfo[foundPathAt].targetInfo[foundTargetPathAt].details->scaling)) != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                            "ERROR: %s: CheckScalingCaps for displayId = 0x%X failed with RC = %d.\n"
                            "The display may not support the specified scaling type. Skipping this mode.\n",
                            __FUNCTION__, dispIdsForLwrrRun[disp], (UINT32)rc);

                    outputCommentStr("Not Attempted,Not Attempted,FAIL,");
                    goto endOfConfig;
                }
                pPathSettings->dirty[foundPathAt].targetInfo[foundTargetPathAt].details->scaling         = true;
#endif
            }
        }

        rc = TestDisplayConfig(configNo,
                               &dispIdsForLwrrRun,
                               &lwrrCombiModes2Use,
                               &lwrrCombiOvlyDepth,
                               &dispIdsForLwrrRun,
                               &lwrrCombiModes2Use,
                               &lwrrCombiHeads2Use,
                               g_IMPMarketingParameters.configList[configNo].viewMode,
                               g_IMPMarketingParameters.configList[configNo].porPState,
                               g_IMPMarketingParameters.configList[configNo].porVPState,
                               0,
                               &m_channelImage[0],
                               &dispHeadMapParams);

endOfConfig:
        // rcOverall keeps a record of the first error encountered
        if (rcOverall == OK)
            rcOverall = rc;

        // Clear the displayIds list
        dispIdsForLwrrRun.clear();

        // Config ends here.
        m_reportStr = Utility::StrPrintf(
                "INFO: %s: Config No = %d: Ends\n", __FUNCTION__, configNo);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    return rcOverall;
}

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

    //! function EnumProcessWindowsProc()
    //!          Used to find the window handle for the processID passed to it.
    //! This callback function retrieves the ProcessID for the window handle passed to it.
    //! If the ProcessID retrieved is same as the one we are looking for
    //! then it stores this HWND into ProcessWindowsInfo=>Windows vector.
    BOOL __stdcall EnumProcessWindowsProc( HWND hwnd, LPARAM lParam )
    {
        ProcessWindowsInfo *Info = reinterpret_cast<ProcessWindowsInfo*>( lParam );
        DWORD WindowProcessID;

        GetWindowThreadProcessId( hwnd, &WindowProcessID );

        if( WindowProcessID == Info->ProcessID )
            Info->Windows.push_back( hwnd );

        return true;
    }

    //! function MoveAndSize()
    //!          This function moves the application whose windows handle is passed to xPos, yPos position.
    //!          It also resizes the application to the width and height provided to it.
    //! params m_Hwnd    [in]      : windows handle of process to be moved
    //! params xPos      [in]      : X co-ordinate where this process it to be moved
    //! params yPos      [in]      : Y co-ordinate where this process it to be moved
    //! params width     [in]      : resize the process window to this width
    //! params height    [in]      : resize the process window to this width
    void MoveAndSize
    (
        HWND    m_Hwnd,
        LwS32   xPos,
        LwS32   yPos,
        LwS32   width,
        LwS32   height
    )
    {
        if (m_Hwnd)
        {
            MoveWindow(m_Hwnd, xPos, yPos, width, height, TRUE);
        }
        return;
    }
#endif

//! function SetLargeLwrsor
//! sets large cursor on the disp ids specified as argument.
RC Modeset::SetLargeLwrsor(const DisplayIDs *pSetLargeLwrsorDispIds,
                           Surface2D  *pLwrsChnImg)
{
    RC rc;
    char    lwrsorImgName[255];
    string  LwrsorImageName;
    UINT32  LwrsorWidth  = 64;
    UINT32  LwrsorHeight = 64;

    //Changing cursor to Max Size
    if (GetBoundGpuSubdevice()->DeviceId() >= Gpu::GM107)
    {
        LwrsorWidth  = 256;
        LwrsorHeight = 256;
    }

    sprintf(lwrsorImgName,"dispinfradata/images/image%dx%d.png",
        LwrsorWidth,
        LwrsorHeight);
    LwrsorImageName = string(lwrsorImgName);

    m_reportStr = Utility::StrPrintf(
            "\nCreating Cursor Image of width =%d, height = %d, Depth = 32 ",LwrsorWidth,LwrsorHeight);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    if(m_pDisplay->GetDisplayClassFamily() == Display::EVO)
    {
        // if the cursor image is preallocated then free it
        // else SetupChannelImage will throw RC error
        if(pLwrsChnImg[0].GetMemHandle() != 0)
        {
            pLwrsChnImg[0].Free();
        }

        rc = m_pDisplay->SetupChannelImage((*pSetLargeLwrsorDispIds)[0],
            LwrsorWidth,
            LwrsorHeight,
            32,
            Display::LWRSOR_IMM_CHANNEL_ID,
            &pLwrsChnImg[0],
            LwrsorImageName);

        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "\n %s: SetupChannelImage() failed with rc = %d",
                __FUNCTION__, (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::SOFTWARE_ERROR;
        }

        rc = m_pDisplay->SetImage((*pSetLargeLwrsorDispIds)[0],
            &pLwrsChnImg[0],
            Display::LWRSOR_IMM_CHANNEL_ID);
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "\n %s: SetupImage(Display::LWRSOR_IMM_CHANNEL_ID) failed with rc = %d",
                __FUNCTION__, (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::SOFTWARE_ERROR;
        }
    }
    else if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
    {
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

        LwU32               targetId          = 0;
        LwAPI_Status        lwApiStatus       = LWAPI_OK;
        LwDisplayHandle     displayHandle     = 0;
        LwPhysicalGpuHandle physicalGpuHandle = 0;
        LW_LWRSOR_STATE     lwapiLwrsorState  = {0};
        UINT32              ourGpuId          = m_pDisplay->GetOwningDisplaySubdevice()->GetGpuId();
        LwAPI_ShortString errorString;

        lwApiStatus = LwAPI_GetPhysicalGPUFromGPUID(ourGpuId, &physicalGpuHandle);
        if (lwApiStatus != LWAPI_OK)
        {
            LwAPI_GetErrorMessage(lwApiStatus, errorString);
            m_reportStr = Utility::StrPrintf(
                "\n %s: LwAPI_GetPhysicalGPUFromGPUID() failed with LwAPI error = %s",
                __FUNCTION__, errorString);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::SOFTWARE_ERROR;
        }

        for(UINT32 lwrsorId = 0; lwrsorId < static_cast<UINT32>(pSetLargeLwrsorDispIds->size()); lwrsorId++)
        {
            memset(&lwapiLwrsorState, 0, sizeof(LW_LWRSOR_STATE));

            lwApiStatus = LwAPI_GPU_GetTargetID(physicalGpuHandle, (*pSetLargeLwrsorDispIds)[lwrsorId], &targetId);
            if (lwApiStatus != LWAPI_OK)
            {
                LwAPI_GetErrorMessage(lwApiStatus, errorString);

                m_reportStr = Utility::StrPrintf(
                    "\n %s: LwAPI_GPU_GetTargetID() failed with LwAPI error = %s",
                    __FUNCTION__, errorString);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return RC::SOFTWARE_ERROR;
            }

            lwapiLwrsorState.version = LW_LWRSOR_STATE_VER;
            lwapiLwrsorState.hwLwrsorEnable = 1;
            lwapiLwrsorState.targetId = targetId;

            if (LwrsorWidth == 256)
            {
                lwapiLwrsorState.lwrsorSize = LW_LWRSOR_SIZE_256;
            }
            else
            {
                lwapiLwrsorState.lwrsorSize = LW_LWRSOR_SIZE_64;
            }

            lwApiStatus = LwAPI_GetAssociatedDisplayFromOutputId(physicalGpuHandle, (*pSetLargeLwrsorDispIds)[lwrsorId], &displayHandle);
            if (lwApiStatus != LWAPI_OK)
            {
                LwAPI_GetErrorMessage(lwApiStatus, errorString);
                m_reportStr = Utility::StrPrintf(
                    "\n %s: LwAPI_GetAssociatedDisplayFromOutputId() failed with LwAPI error = %s",
                    __FUNCTION__, errorString);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return RC::SOFTWARE_ERROR;
            }

            lwApiStatus = LwAPI_DISP_SetLwrsorState(displayHandle, &lwapiLwrsorState);
            if (lwApiStatus != LWAPI_OK)
            {
                LwAPI_GetErrorMessage(lwApiStatus, errorString);
                m_reportStr = Utility::StrPrintf(
                    "\n %s: LwAPI_DISP_SetLwrsorState() failed with LwAPI error = %s",
                    __FUNCTION__, errorString);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return RC::SOFTWARE_ERROR;
            }
        }

#endif
    }
    return rc;
}

void LaunchApplication(void *app2Launch)
{

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    LaunchApplicationParams *launchAppParams = (LaunchApplicationParams *) app2Launch;

    if(appLaunchCreatedUnderflow)
    {
        Printf(Tee::PriHigh,
                "Seems system is already in underflowed state..Skipping launching Process: %s",
                launchAppParams->app.command.c_str());
        return;
    }
    RC rc;
    char        reportStr[255];
    UINT32      underflowHead;
    HANDLE      processHandle;
    UINT64      startTimeMS     = 0;
    UINT64      elapseTimeSec   = 0;
    bool        bPollComplete   = false;
    UINT32      gpuUtilPerSec   = 0;
    UINT32      gpuUtilTot      = 0;
    UINT32      memUtilPerSec   = 0;
    UINT32      memUtilTot      = 0;
    UINT32      count           = 0;
    UINT32      perSecCount     = 0;
    UINT64      prevTimeElapsedInSecs = 0;
    LW_GPU_DYNAMIC_PSTATES_INFO pstatesInfo = {0};
    LwPhysicalGpuHandle physicalGpuHandle;
    DWORD exitCode;
    UINT32      ourGpuId        = (launchAppParams->pDisplay)->GetOwningDisplaySubdevice()->GetGpuId();
    LwAPI_GetPhysicalGPUFromGPUID(ourGpuId, &physicalGpuHandle);

    if ((rc = LaunchApplicationStartProcess(launchAppParams, &processHandle)) != OK )
    {
        appLaunchError = true;
        return;
    }

    // Check Underflow
    if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(launchAppParams->pDisplay, launchAppParams->numHeads, &underflowHead))
         == RC::LWRM_ERROR)
    {
        sprintf(reportStr,"ERROR: %s: Display underflow was detected on head %u after launching Process Name = %s\n",
                __FUNCTION__, underflowHead, launchAppParams->app.command.c_str());
        Printf(Tee::PriHigh, reportStr);
        appLaunchCreatedUnderflow = true;

        // kill the Process
        goto Cleanup;
    }

    elapseTimeSec = prevTimeElapsedInSecs = startTimeMS = Platform::GetTimeMS();

    sprintf(reportStr,
            "Process Name = %s, Process Handle = %u, running time =%d seconds, flags = %u\n",
            launchAppParams->app.command.c_str(), processHandle,
            launchAppParams->app.seconds, launchAppParams->app.flags);
    Printf(Tee::PriHigh, reportStr);

    do
    {
        if (GetExitCodeProcess(processHandle, &exitCode))
        {
            if (exitCode != STILL_ACTIVE)
            {
                if (appRunCheck)
                {
                    if (!perSecCount)
                        perSecCount = 1;
                    gpuUtilTot = gpuUtilTot/perSecCount;
                    memUtilTot = memUtilTot/perSecCount;

                    if (!((gpuUtilTot >= (launchAppParams->app).gpuClkUtilThreshold) &&
                         (memUtilTot >= ((launchAppParams->app).mClkUtilThreshold) + baselineMClkUtil)))
                    {
                        sprintf(reportStr,"%s: gpuClkUtilThreshold = %d, mClkUtilThreshold = %d\n",
                                        __FUNCTION__, gpuUtilTot, memUtilTot);
                        Printf(Tee::PriHigh, reportStr);
                        sprintf(reportStr, "ERROR: %s: Failed to run application = %s\n",
                                __FUNCTION__, launchAppParams->app.command.c_str());
                        Printf(Tee::PriHigh, reportStr);
                        appNotRun = true;
                        goto Cleanup;
                    }
                }
                if ((UINT32)elapseTimeSec < launchAppParams->app.seconds)
                {
                    // re-launch the app
                    Printf(Tee::PriHigh, "Process Name = %s re-launch\n", launchAppParams->app.command.c_str());

                    // call launchApplicationStopProcess to close process handle
                    LaunchApplicationStopProcess(launchAppParams->app.command, processHandle);
                    // printing the values before re-launching app
                    // this is the case when app has exited before time but its mclk and
                    // gpu clk utilizations are higher than the threshold values
                    sprintf(reportStr,"%s: gpuClkUtilThreshold = %d, mClkUtilThreshold = %d\n",
                                        __FUNCTION__, gpuUtilTot, memUtilTot);
                    Printf(Tee::PriHigh, reportStr);
                    if ((rc = LaunchApplicationStartProcess(launchAppParams, &processHandle)) != OK)
                    {
                        sprintf(reportStr, "ERROR: %s: Re-launch Process Name = %s failed\n",
                                __FUNCTION__, launchAppParams->app.command.c_str());
                        Printf(Tee::PriHigh, reportStr);
                        appLaunchError = true;
                        // kill the Process
                        goto Cleanup;
                    }
                    perSecCount = 0;
                    count = 0;
                    gpuUtilPerSec = 0;
                    memUtilPerSec = 0;
                }
                else if (launchAppParams->app.flags & APP_FLAG_POLL_COMPLETE)
                {
                    bPollComplete = true;
                }
            }
            else
            {
                if (appRunCheck)
                {
                    pstatesInfo.version = LW_GPU_DYNAMIC_PSTATES_INFO_VER;
                    LwAPI_GPU_GetDynamicPstatesInfo(physicalGpuHandle, &pstatesInfo);

                    if ((pstatesInfo.utilization[LWAPI_GPU_UTILIZATION_DOMAIN_GPU].bIsPresent)&&
                        (pstatesInfo.utilization[LWAPI_GPU_UTILIZATION_DOMAIN_FB].bIsPresent))
                    {
                        gpuUtilPerSec += pstatesInfo.utilization[LWAPI_GPU_UTILIZATION_DOMAIN_GPU].percentage;
                        memUtilPerSec += pstatesInfo.utilization[LWAPI_GPU_UTILIZATION_DOMAIN_FB].percentage;
                        count++;
                    }
                    else
                    {
                        sprintf(reportStr, "ERROR: %s: Utilization can't be measured as one of the domains is missing.\n",
                                __FUNCTION__);
                        Printf(Tee::PriHigh, reportStr);
                        break;
                    }

                    if (prevTimeElapsedInSecs != elapseTimeSec)
                    {
                        perSecCount++;
                        // take the average of clock utils per second
                        if (count)
                        {
                            gpuUtilTot += gpuUtilPerSec/count;
                            memUtilTot += memUtilPerSec/count;
                        }
                        count = 0;
                        gpuUtilPerSec = 0;
                        memUtilPerSec = 0;
                    }

                    prevTimeElapsedInSecs = elapseTimeSec;
                }
            }
        }
        else
        {
            sprintf(reportStr, "ERROR: %s: GetExitCodeProcess failed, can't query process status.\n",
                    __FUNCTION__);
            Printf(Tee::PriHigh, reportStr);
            // kill the Process
            goto Cleanup;
        }

        Modeset::Sleep(1000, launchAppParams->runOnEmu, launchAppParams->gpuSubdev);

        if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(launchAppParams->pDisplay, launchAppParams->numHeads, &underflowHead))
            == RC::LWRM_ERROR)
        {
            sprintf(reportStr, "ERROR: %s: Display underflow was detected on head %u while running Process Name = %s\n",
                    __FUNCTION__, underflowHead, launchAppParams->app.command.c_str());
            Printf(Tee::PriHigh, reportStr);
            appLaunchCreatedUnderflow = true;

            // kill the Process
            goto Cleanup;
        }

        elapseTimeSec = (Platform::GetTimeMS() - startTimeMS) / 1000;
    }
    while (((launchAppParams->app.flags & APP_FLAG_POLL_COMPLETE) && !bPollComplete) ||
           (UINT32)elapseTimeSec < launchAppParams->app.seconds);

    // print mclk and gpu clock utilization values
    // this is the case when app ran without any need for the re-launch
    if (!perSecCount)
        perSecCount = 1;
    gpuUtilTot = gpuUtilTot/perSecCount;
    memUtilTot = memUtilTot/perSecCount;

    sprintf(reportStr,"%s: gpuClkUtilThreshold = %d, mClkUtilThreshold = %d\n",
                        __FUNCTION__, gpuUtilTot, memUtilTot);
    Printf(Tee::PriHigh, reportStr);

    // Let's re-check for Underflow after the sleep.
    if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(launchAppParams->pDisplay, launchAppParams->numHeads, &underflowHead))
        == RC::LWRM_ERROR)
    {
        sprintf(reportStr,"ERROR: %s: Display underflow was detected on head %u after running Process Name = %s \n",
                __FUNCTION__, underflowHead, launchAppParams->app.command.c_str());
        Printf(Tee::PriHigh, reportStr);
        appLaunchCreatedUnderflow = true;
    }

Cleanup:
    // kill the Process with exitcode 0
    LaunchApplicationStopProcess(launchAppParams->app.command, processHandle);

#else
    Printf(Tee::PriHigh, "Application launching is not supported on non winmods platform");
#endif
    return;
}

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

static RC LaunchApplicationStartProcess(LaunchApplicationParams *launchAppParams, HANDLE* pProcessHandle)
{
    char        reportStr[255];
    SHELLEXELWTEINFO ShExecInfo;

    ShExecInfo.cbSize       = sizeof(SHELLEXELWTEINFO);
    ShExecInfo.fMask        = NULL;
    ShExecInfo.hwnd         = NULL;
    ShExecInfo.lpVerb       = "open";
    ShExecInfo.lpFile       = launchAppParams->app.command.c_str();
    ShExecInfo.lpParameters = launchAppParams->app.parameters.c_str();
    ShExecInfo.lpDirectory  = launchAppParams->app.directory.c_str();
    ShExecInfo.nShow        = SW_MAXIMIZE;
    ShExecInfo.hInstApp     = NULL;
    ShExecInfo.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_HMONITOR | SEE_MASK_FLAG_NO_UI;

    if (!ShellExelwteEx(&ShExecInfo))
    {
        sprintf(reportStr, "Launching Process Name = %s Failed.\n",
                launchAppParams->app.command.c_str());
        Printf(Tee::PriHigh, reportStr);
        return RC::SOFTWARE_ERROR;
    }

    WaitForInputIdle( ShExecInfo.hProcess, INFINITE );
    sprintf(reportStr, "Launching Process Name = %s, Process Handle = %u\n",
            launchAppParams->app.command.c_str(), ShExecInfo.hProcess);
    Printf(Tee::PriHigh, reportStr);

    // This sleep is required for application to
    // initialize properly after which app handle retrieving and moving works fine
    Modeset::Sleep(launchAppParams->launchTimeSec * 1000, launchAppParams->runOnEmu, launchAppParams->gpuSubdev);

    if (launchAppParams->app.flags & APP_FLAG_SET_POSITION_AND_SIZE)
    {
        // Let's get the window handle from the process handle
        ProcessWindowsInfo Info( GetProcessId(ShExecInfo.hProcess) );

        EnumWindows( (WNDENUMPROC)EnumProcessWindowsProc,
                     reinterpret_cast<LPARAM>( &Info ) );

        // Move to required display Id and x,y position
        if (static_cast<UINT32>(Info.Windows.size()) > 0)
        {
            MoveAndSize(Info.Windows[0],
                    launchAppParams->startXPosOfDisplay + launchAppParams->app.xPos,
                    launchAppParams->startYPosOfDisplay + launchAppParams->app.yPos,
                    launchAppParams->appWidth,
                    launchAppParams->appHeight);
        }
    }

    *pProcessHandle = ShExecInfo.hProcess;

    return OK;
}

static void LaunchApplicationStopProcess(string processName, HANDLE hProcess)
{
    char        reportStr[255];

    // kill the Process with exitcode 0
    if (hProcess)
    {
        sprintf(reportStr, "Process Name = %s, Process Handle = %u, Killing\n",
                processName.c_str(), hProcess);
        Printf(Tee::PriHigh, reportStr);
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }

    return;
}

#endif

// Set Underflow state on all heads.
// Function paramters are to allow have more control over the function.
// When no argument specified it clears underflow and enables underflow reporting
RC  Modeset::SetUnderflowStateOnAllHead(UINT32 enable,
                                        UINT32 clearUnderflow,
                                        UINT32 mode)
{
    RC rc;

    for(UINT32 head = 0; head < m_numHeads; head++)
    {
        rc = DTIUtils::VerifUtils::SetUnderflowState(m_pDisplay,
            head,
            enable,
            clearUnderflow,
            mode);
        if(rc != OK)
        {
            Printf(Tee::PriHigh, "\n %s: Failed to SetUnderflowState() on head %d with RC = %d.",
                __FUNCTION__, head, (UINT32)rc);
            return rc;
        }
    }

    return rc;
}

RC Modeset::ProgramAndVerifyPState(UINT32 perfLevel)
{
    RC rc;
    vector<LW2080_CTRL_CLK_EXTENDED_INFO> MismatchedClks;
    string errorCause = " ";
    UINT32 underflowHead;

    m_reportStr = Utility::StrPrintf(
            "Starting %s Transition to %s ",
            m_PerfStatePrint,
            printPerfStateHelper(perfLevel).c_str());
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    if (!m_bIsPS30Supported)
    {
        rc = m_objSwitchRandomPState.ProgramAndVerifyPStates(perfLevel,
            false,
            &MismatchedClks,
            m_freqTolerance100x,
            false);
    }
    else
    {
        rc = m_objSwitchRandomPState.ProgramAndVerifyIMPVPStates(perfLevel,
            false,
            &MismatchedClks,
            m_freqTolerance100x,
            false);
    }

    if(rc != OK)
    {
        if (rc == RC::DATA_MISMATCH || rc == RC::CLOCKS_TOO_LOW)
        {
            RmtClockUtil clkUtil;
            string clkErrorString;
            for(UINT32 countMismatch = 0; countMismatch < MismatchedClks.size(); countMismatch++)
            {
                double fPercentDiff = ((double)MismatchedClks[countMismatch].effectiveFreq  -
                    (double)MismatchedClks[countMismatch].clkInfo.targetFreq) * 100.0 /
                    (double)(MismatchedClks[countMismatch].clkInfo.targetFreq);

                clkErrorString = Utility::StrPrintf("\nClock Difference Found =>\n"
                    "Clk Name: %13s,\n"
                    "Source: %10s,\n"
                    "Flag: 0x%x,\n"
                    "Expected Freq: %7d KHz,\n"
                    "Measured Freq (SW): %7d KHz,\n"
                    "Measured Freq (HW): %7d KHz,\n"
                    "Freq Diff: %+5.4f %%,\n",
                    clkUtil.GetClkDomainName(MismatchedClks[countMismatch].clkInfo.clkDomain),
                    clkUtil.GetClkSourceName(MismatchedClks[countMismatch].clkInfo.clkSource),
                    (UINT32)(MismatchedClks[countMismatch].clkInfo.flags),
                    (UINT32)(MismatchedClks[countMismatch].clkInfo.targetFreq),
                    (UINT32)(MismatchedClks[countMismatch].clkInfo.actualFreq),
                    (UINT32)(MismatchedClks[countMismatch].effectiveFreq),
                    fPercentDiff);

                errorCause += string(clkErrorString);

                PRINT_STDOUT_N_REPORT(Tee::PriHigh, clkErrorString.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            }

            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Clock Diff Found during %s Transition to %s\n",
                    __FUNCTION__, m_PerfStatePrint,
                    printPerfStateHelper(perfLevel).c_str());

            m_clockDiffFound = true;

            if (m_ignoreClockDiffError)
                rc.Clear();
            MismatchedClks.clear();
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: ProgramAndVerifyPStates() failed."
                    " %s Transition to %s failed with RC = %d\n",
                    __FUNCTION__,
                    m_PerfStatePrint,
                    printPerfStateHelper(perfLevel).c_str(),
                    (UINT32)rc);
        }

        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        if (rc != OK)
        {
            return rc;
        }
    }

    // Check underflow
    if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR)
    {
        if (!m_IgnoreUnderflow)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Display underflow was detected on head %u after %s Changed to %s\n",
                    __FUNCTION__, underflowHead,
                     m_PerfStatePrint,
                     printPerfStateHelper(perfLevel).c_str());
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
    }
    return rc;
}

//! TestAllEdidResCombi: loops through all the edid resolutions for all the combinations of displayIds.
//! workingSet[in]  :   DisplayIDs to use for the testing
RC Modeset::TestAllEdidResCombi(DisplayIDs workingSet)
{
    RC                   rc;
    Display             *pDisplay           = GetDisplay();
    vector<UINT32>       validCombi;
    DisplayIDs           ActiveDisplays;
    DisplayIDs           lwrrCombinationDispIDs;
    UINT32               lwrrCombiDispMask  = 0;
    LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams;
    vector<ModeSettingsEx>    lwrrModesOnOtherDispIds;

    // Report file header
    m_reportStr = Utility::StrPrintf(
            "\nTimeStamp,Display ID,Width,Height,"
            "Depth,Refresh Rate,IMP Status,SetMode Done,Result,Comment,");
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_CSV_FILE);

    //get the no of displays to work with
    UINT32 numDisps = static_cast<UINT32>(workingSet.size());
    if (numDisps == 0)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: No displays detected, test aborting...RC= SOFTWARE_ERROR\n",__FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }
    UINT32 allCombinations = static_cast<UINT32>(1<<(numDisps));
    validCombi.resize(numDisps);

    for (UINT32 trialcombi = allCombinations - 1; trialcombi > 0; trialcombi--)
    {
        // if its a valid combination the try the one
        if (!combinationGenerator(validCombi, trialcombi, m_numHeads, numDisps))
            continue;

        //Detach all the selected display to get all the heads free for this new combination
        ActiveDisplays.clear();
        pDisplay->GetScanningOutDisplays(&ActiveDisplays);

        lwrrCombinationDispIDs.clear();
        //Get the Vector of DisplayID of current combination
        for(UINT32 loopCount = 0; loopCount < numDisps; loopCount++)
        {
            if(validCombi[loopCount])
                lwrrCombinationDispIDs.push_back(workingSet[loopCount]);
        }

        // Detach all current display Ids so we get free head for next combination
        // For winmods do modeset on first display Id will detach all active scanning display IDs
        if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
        {
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

            m_pDisplay->GetWin32Display()->SetIgnorePrevModesetDispID(true);
            if(static_cast<UINT32>(lwrrCombinationDispIDs.size()) > 0)
            {
                vector<UINT32>            heads;
                heads.push_back(Display::ILWALID_HEAD);
                vector<Display::Mode>              modes;
                modes.push_back( Display::Mode(1024, 768, 32, 60));

                rc = m_pDisplay->SetMode(DisplayIDs(1, lwrrCombinationDispIDs[0]),
                        heads,
                        modes,
                        false,
                        Display::STANDARD);

                if (rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: SetMode Failed with RC=%d\n",
                        __FUNCTION__,(UINT32)rc);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    continue;
                }
            }
            m_pDisplay->GetWin32Display()->SetIgnorePrevModesetDispID(false);
#endif
        }
        else if(m_pDisplay->GetDisplayClassFamily() == Display::EVO)
        {
            //detach All scanning out displays to get free heads
            m_pDisplay->DetachDisplay(ActiveDisplays);
        }

        pDisplay->DispIDToDispMask(lwrrCombinationDispIDs, lwrrCombiDispMask);

        // zero out param structs
        memset(&dispHeadMapParams, 0,
               sizeof (LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS));

        // Get Heads for this new Combination
        // Call the GetHeadRouting
        dispHeadMapParams.subDeviceInstance = 0;
        dispHeadMapParams.displayMask = lwrrCombiDispMask;
        dispHeadMapParams.oldDisplayMask = 0;
        dispHeadMapParams.oldHeadRoutingMap = 0;
        dispHeadMapParams.headRoutingMap = 0;

        rc = DTIUtils::Mislwtils::GetHeadRoutingMask(pDisplay, &dispHeadMapParams);
        if(rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "\nError: %s GetHeadRoutingMask() failed with RC = %d\n"  \
                "dispHeadMapParams.subDeviceInstance = %u\n"     \
                "dispHeadMapParams.displayMask       = %u\n"     \
                "dispHeadMapParams.oldDisplayMask    = %u\n"     \
                "dispHeadMapParams.oldHeadRoutingMap = %u\n"     \
                "dispHeadMapParams.headRoutingMap    = %u\n",
                __FUNCTION__, (UINT32)rc,
                (UINT32)dispHeadMapParams.subDeviceInstance,
                (UINT32)dispHeadMapParams.displayMask,
                (UINT32)dispHeadMapParams.oldDisplayMask,
                (UINT32)dispHeadMapParams.oldHeadRoutingMap,
                (UINT32)dispHeadMapParams.headRoutingMap);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            PRINT_STDOUT_N_REPORT(Tee::PriHigh, "Current Combination Head Routing Mask is not possible."
                "Skipping this combination.",
                PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);
            continue;
        }

        m_reportStr = "\n############################################\n";
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);
        m_reportStr = "Testing Display Combinations DisplayIDs:\n";
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);

        for (UINT32 disp = 0; disp < lwrrCombinationDispIDs.size(); disp ++)
        {
            m_reportStr = Utility::StrPrintf(
                    "   0x%08x    ", (UINT32)lwrrCombinationDispIDs[disp]);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);
        }
        m_reportStr = "\n############################################\n";
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);

        UINT32 Head = 0;
        // Let's check if head mapping is valid
        if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask(lwrrCombinationDispIDs[0], dispHeadMapParams, &Head)) != OK)
        {
            m_reportStr = Utility::StrPrintf(
                "%s: Current combination head mapping is not supported.Skipping this config\n ",
                __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            m_reportStr = Utility::StrPrintf(
                "Head Routing Mask Params =\n"
                "dispHeadMapParams.subDeviceInstance = %u\n"
                "dispHeadMapParams.displayMask       = %u\n"
                "dispHeadMapParams.oldDisplayMask    = %u\n"
                "dispHeadMapParams.oldHeadRoutingMap = %u\n"
                "dispHeadMapParams.headRoutingMap    = %u\n",
                (UINT32)dispHeadMapParams.subDeviceInstance,
                (UINT32)dispHeadMapParams.displayMask,
                (UINT32)dispHeadMapParams.oldDisplayMask,
                (UINT32)dispHeadMapParams.oldHeadRoutingMap,
                (UINT32)dispHeadMapParams.headRoutingMap);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            PRINT_STDOUT_N_REPORT(Tee::PriHigh, "Current Combination Head Routing Mask is not possible."
                "Skipping this combination.",
                PRINT_STDOUT | PRINT_LOG_FILE | PRINT_CSV_FILE);

            continue;
        }

        lwrrModesOnOtherDispIds.clear();
        // start the Modeset on current display combination and repeat for each EDID resolution
        rc = TestLwrrCombination(lwrrCombinationDispIDs,
                                 dispHeadMapParams,
                                 lwrrModesOnOtherDispIds);
        if(rc != OK)
        {
            if (rc == RC::ILWALID_EDID)
            {
                // WAR: invalid edid displays must be ignored since @ bringup
                // using KVM returns invalid edid for that displayID.
                Printf(Tee::PriHigh, "%s: TestLwrrCombination() failed with RC::ILWALID_EDID."
                    "Ignoring this and moving to next combination\n",
                    __FUNCTION__);
                rc.Clear();
            }
            else if (rc == RC::ILWALID_HEAD)
            {
                Printf(Tee::PriHigh, "%s: Head Routing Map failed ILWALID_HEAD."
                    "Ignoring this and moving to next combination\n",
                    __FUNCTION__);
                rc.Clear();
            }
            else
            {
                Printf(Tee::PriHigh, "%s: TestLwrrCombination() failed with RC = %d.",
                    __FUNCTION__, (UINT32)rc);
            }
            CHECK_RC(rc);
        }
    }

    return rc;

}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC Modeset::Cleanup()
{
    RC  rc;
    UINT32 i;
    for (i = 0; i < wellKnownRes.size(); i++)
    {
        delete wellKnownRes[i];
        wellKnownRes[i] = NULL;
    }

    // Let's restore undertflow reporting state of card
    rc = SetUnderflowStateOnAllHead(
        m_underFlowParamsOnInit.underflowParams.enable,       // enable,
        m_underFlowParamsOnInit.underflowParams.underflow,    // clearUnderflow,
        m_underFlowParamsOnInit.underflowParams.mode);        // mode
    if(rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Failed to restore UnderflowStateOnAllHead. RC = %d\n",
            __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE );
        return rc;
    }

    rc = m_objSwitchRandomPState.Cleanup();
    if(rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "ERROR: %s: ObjSwitchRandomPState.Cleanup() Failed with RC=%d\n",
            __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    if(m_pDisplay->GetDisplayClassFamily() == Display::WIN32DISP)
    {
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
        m_pDisplay->GetWin32Display()->SetIgnorePrevModesetDispID(true);
#endif
    }

    wellKnownRes.clear();

    fclose(m_pCSVFile);
    fclose(m_pReportFile);

    m_pCSVFile = NULL ;
    m_pReportFile = NULL;
    return OK;
}

RC Modeset::PRINT_STDOUT_N_REPORT(UINT32 priority, const char *message, UINT32 printWhere)
{
        RC rc;
        if(printWhere & PRINT_STDOUT)
            Printf(priority, "%s", message);
        if(printWhere & PRINT_LOG_FILE)
            rc = DTIUtils::ReportUtils::write2File(m_pReportFile, message);
        if(printWhere & PRINT_CSV_FILE)
            rc = DTIUtils::ReportUtils::write2File(m_pCSVFile, message);
        return rc;
}

RC Modeset::GetDisplayIdFromGpuAndOutputId(UINT32        gpuId,
                                               DisplayID     modsDisplayID,
                                               UINT32       *lwApiDisplayId)
{
    RC rc;
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    LwAPI_Status            lwApiStatus;
    LwPhysicalGpuHandle     gpuHandleThisDisplay;

    lwApiStatus = LwAPI_Initialize();
    if (lwApiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%s: LwAPI_Initialize failed\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Get the Gpuhandle of our GPU ID
    lwApiStatus = LwAPI_GetPhysicalGPUFromGPUID(gpuId, &gpuHandleThisDisplay);
    if (lwApiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%s: LwAPI_GetPhysicalGPUFromGPUID failed\n",
            __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Now get the lwapi style of DisplayId for thisDisplay
    lwApiStatus = LwAPI_SYS_GetDisplayIdFromGpuAndOutputId(gpuHandleThisDisplay,
                            (UINT32)modsDisplayID, lwApiDisplayId);
    if (lwApiStatus != LWAPI_OK)
    {
        Printf(Tee::PriHigh, "%s: LwAPI_SYS_GetDisplayIdFromGpuAndOutputId failed\n",
            __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

#endif
    return rc;
}

//! \brief combinationGenerator, does
//! takes a integer as input colwerts  it to a combination. Returns true if
//! the combination is valid else returns false
//------------------------------------------------------------------------------
bool Modeset::combinationGenerator(vector<UINT32>& validCombi,
                                   UINT32          trialCombi,
                                   UINT32          numOfHeads,
                                   UINT32          numOfDisps)
{
    UINT32 limit = 0;

    for(UINT32 loopCount = 0; loopCount < numOfDisps; loopCount++)
    {
        validCombi[loopCount] = (trialCombi & 1);
        if (validCombi[loopCount])
            limit++;
        trialCombi = (trialCombi >> 1);
    }

    if(limit <= numOfHeads && limit >= 1)
        return true;
    else
        return false;
}

//! \brief Prepare a user-friendly string for the perf level
//!
//! For example, if the perfState == 8 on a PS30 System, then the return string
//! might be something similar to "IMP vP8". If the perfState ==
//! LW2080_CTRL_PERF_PSTATES_P5 on a PS20 system, then the return string might
//! be "P5".
//!
//! perfState: PS30: a 0-based perf level
//!            PS20: a LW2080_CTRL_PERF_PSTATES_xx pstate specifier
string Modeset::printPerfStateHelper(UINT32 perfState)
{
    string reportStr = Utility::StrPrintf("%s%u",
        m_PerfStatePrintHelper,
        (m_bIsPS30Supported) ? perfState :
        DTIUtils::Mislwtils::getLogBase2(perfState));
    return reportStr;
}

//! \brief check the VBIOS settings of ASR/MSCG test, return false if anything wrong
//------------------------------------------------------------------------------
bool Modeset::checkAsrMscgTestVbiosSettings()
{
    RC rc;
    FermiPStateSwitchTest ObjSwitchRandomPState;
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS PStatesInfoParams = {0};
    vector<LwU32> supportedPStateMasks;
    bool isFeatureEnabled = false;
    LwU32 isFeatureEnabledPerPState;

    // check what pstates are supported
    rc = ObjSwitchRandomPState.GetSupportedPStatesInfo(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                                       PStatesInfoParams,
                                                       supportedPStateMasks);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "GetSupportedPStatesInfo failed with RC = %d\n", (UINT32) rc);
        Printf(Tee::PriNormal, "%s", m_reportStr.c_str());
        return false;
    }

    //
    // Check the VBIOS settings for ASR/MSCG
    // 1. There must one pstate which set ASR/MSCG to true.
    // 2. For ASR test, need to check MSCG is disabled for any pstate since the priority of MSCG is higher than ASR
    //    if both set to enable and it will cause abnormal result later when test running.
    //
    if (m_enableASRTest)
    {
        for (LwU32 i = 0; i < supportedPStateMasks.size(); i++)
        {
            rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_MSCG_ALLOWED_PER_PSTATE,
                                 supportedPStateMasks[i], &isFeatureEnabledPerPState);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Query MSCG enable state for pstate P%u failed with RC = %d\n",
                        __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]), (UINT32)rc);
                Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

                return false;
            }

            if (!!isFeatureEnabledPerPState)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: enable ASR test failed since pstate P%u set MSCG to enable in VBIOS\n",
                        __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]));
                Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

                return false;
            }

            rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_ASR_ALLOWED_PER_PSTATE,
                                 supportedPStateMasks[i], &isFeatureEnabledPerPState);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Query ASR enable state for pstate P%u failed with RC = %d\n",
                        __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]), (UINT32)rc);
                Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

                return false;
            }

            if (!!isFeatureEnabledPerPState)
            {
                isFeatureEnabled = true;
            }
        }

        if (!isFeatureEnabled)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: enable ASR test failed since there is no pstate set ASR to enable in VBIOS\n",
                    __FUNCTION__);
            Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

            return false;
        }
    }

    if (m_enableMSCGTest && !m_bIsPS30Supported)
    {
        for (LwU32 i = 0; i < supportedPStateMasks.size(); i++)
        {
            rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_MSCG_ALLOWED_PER_PSTATE,
                                 supportedPStateMasks[i], &isFeatureEnabledPerPState);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: Query MSCG enable state for pstate P%u failed with RC = %d\n",
                        __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]), (UINT32)rc);
                Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

                return false;
            }

            if (!!isFeatureEnabledPerPState)
            {
                isFeatureEnabled = true;
                break;
            }
        }

        if (!isFeatureEnabled)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: enable MSCG test failed since there is no pstate set MSCG to enable in VBIOS\n",
                    __FUNCTION__);
            Printf(Tee::PriNormal, "%s", m_reportStr.c_str());

            return false;
        }
    }

    return true;
}

//! \brief setup the test settings before setMode
//------------------------------------------------------------------------------
RC Modeset::configTestSettings()
{
    RC rc;
    FermiPStateSwitchTest ObjSwitchRandomPState;
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS PStatesInfoParams = {0};
    vector<LwU32> supportedPStateMasks;
    if (!m_bIsPS30Supported)
    {
        // check what pstates are supported
        rc = ObjSwitchRandomPState.GetSupportedPStatesInfo(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                                           PStatesInfoParams,
                                                           supportedPStateMasks);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "GetSupportedPStatesInfo failed with RC = %d\n", (UINT32) rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }

    }

    if (m_enableASRTest)
    {
        // force enable asr
        rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_ASR_ALLOWED,
                             LW2080_CTRL_PERF_PSTATES_UNDEFINED, TRUE);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "Force allowing ASR failed with RC = %d\n", (UINT32) rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
        for (LwU32 i = 0; i < supportedPStateMasks.size(); i++)
        {
            // force disable mscg for pstate
            rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_MSCG_ALLOWED_PER_PSTATE,
                                 supportedPStateMasks[i], FALSE);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "Force disallowing MSCG for pstate P%u failed with RC = %d\n",
                        DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]), (UINT32)rc);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return rc;
            }

            // force asr for pstate
            rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_ASR_ALLOWED_PER_PSTATE,
                                 supportedPStateMasks[i], TRUE);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "Force allowing ASR for pstate P%u failed with RC = %d\n",
                        DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]), (UINT32) rc);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return rc;
            }
            m_reportStr = Utility::StrPrintf(
                    "INFO: force allowing ASR and force disallowing MSCG for pstate P%u\n",
                    DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]));
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }

    if (m_enableMSCGTest && !m_bIsPS30Supported)
    {
        //
        // To setup MSCG, the flow is very simliar as ASR, only different is we just set ASR to disallowed,
        // then it will be disabled regardless of the settings in each pstate.
        //
        rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_ASR_ALLOWED,
                             LW2080_CTRL_PERF_PSTATES_UNDEFINED, FALSE);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "Force disallowing ASR failed with RC = %d\n", (UINT32) rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
        for (LwU32 i = 0; i < supportedPStateMasks.size(); i++)
        {
            rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_IS_MSCG_ALLOWED_PER_PSTATE,
                                 supportedPStateMasks[i], TRUE);
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "Force allowing MSCG for pstate P%u failed with RC = %d\n",
                        DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]), (UINT32) rc);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return rc;
            }
            m_reportStr = Utility::StrPrintf(
                    "INFO: force allowing MSCG for pstate P%u\n",
                    DTIUtils::Mislwtils::getLogBase2(supportedPStateMasks[i]));
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }

    if (m_mempoolCompressionTristate == TRISTATE_DISABLE || m_mempoolCompressionTristate == TRISTATE_ENABLE)
    {
        rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_MEMPOOL_COMPRESSION,
                             LW2080_CTRL_PERF_PSTATES_UNDEFINED,
                             (m_mempoolCompressionTristate == TRISTATE_ENABLE));
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "%s: force %s mempool compression failed with RC = %d\n",
                    __FUNCTION__, m_mempoolCompressionTristate == TRISTATE_ENABLE ? "enabling" : "disabling", (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }

        m_reportStr = Utility::StrPrintf(
                "INFO: force %s mempool compression\n",
                m_mempoolCompressionTristate == TRISTATE_ENABLE ? "enabling" : "disabling");
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    if (m_minMempoolTristate == TRISTATE_DISABLE || m_minMempoolTristate == TRISTATE_ENABLE)
    {
        rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_FORCE_MIN_MEMPOOL,
                             LW2080_CTRL_PERF_PSTATES_UNDEFINED,
                             (m_minMempoolTristate == TRISTATE_ENABLE));
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "%s: force %s min mempool failed with RC = %d\n",
                    __FUNCTION__, m_minMempoolTristate == TRISTATE_ENABLE ? "enabling" : "disabling", (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
        m_reportStr = Utility::StrPrintf(
                "INFO: force %s min mempool\n",
                m_minMempoolTristate == TRISTATE_ENABLE ? "enabling" : "disabling");
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    return OK;
}

//! \brief check the result of ASR/MSCG test after setMode
//------------------------------------------------------------------------------
RC Modeset::checkAsrMscgTestResults()
{
    RC rc;
    GpuSubdevice* pSubdevice = GetBoundGpuSubdevice();
    PgStats objPgStats(pSubdevice);
    LwU32 stutterFeature;
    LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS PStatesInfoParams = {0};
    vector<LwU32> supportedPStateMasks;

    // check what pstates are supported
    rc = m_objSwitchRandomPState.GetSupportedPStatesInfo(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                                         PStatesInfoParams,
                                                         supportedPStateMasks);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "GetSupportedPStatesInfo failed with RC = %d\n", (UINT32) rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    // loop through from RM IMP minPState to maxPState(P0) and test ASR/MSCG
    for (UINT32 pState = m_lwrrImpMinPState; pState >= (UINT32)P0; pState = pState >> 1)
    {
        if (std::count(supportedPStateMasks.begin(), supportedPStateMasks.end(), (UINT32)pState))
        {
            m_reportStr = Utility::StrPrintf(
                    "Starting PState Transition to P%u",
                    DTIUtils::Mislwtils::getLogBase2(pState));
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            CHECK_RC(ProgramAndVerifyPState(pState));

            //
            // MSCG and ASR subtests have the similar flow to check its status, as below
            //
            // 1. Check if ASR/MSCG is running
            // 2. If yes, means IMP consider ASR/MSCG is possible and go step#3.
            //    If no, just skip since IMP think ASR/MSCG is not possible.
            // 3. (MSCG only) Check if MSCG is actually running, if yes, go step#4
            // 4. Check efficiency 10 times and raise error if any actual efficiency < predicted efficiency
            //
            if (m_enableASRTest)
            {
                // confirm ASR is running
                rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_STUTTER_FEATURE_PER_PSTATE, pState, &stutterFeature);
                if (rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                            "Get stutter feature for pstate %u failed with rc = %d\n",
                            DTIUtils::Mislwtils::getLogBase2(pState), (UINT32)rc);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    return rc;
                }
                if (stutterFeature != LW5070_CTRL_IMP_STUTTER_FEATURE_ASR)
                {
                    m_reportStr = Utility::StrPrintf(
                            "INFO: %s: IMP deemed ASR not possible for this mode... skipping ASR check. (Running is %s)\n",
                            __FUNCTION__,
                            (stutterFeature == LW5070_CTRL_IMP_STUTTER_FEATURE_MSCG ? "MSCG" :
                             (stutterFeature == LW5070_CTRL_IMP_STUTTER_FEATURE_NONE ? "none" :
                              "<invalid value>")));
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                }
                else
                {
                    rc = checkAsrEfficiency(pState);
                    if ( rc != OK)
                    {
                        m_reportStr = Utility::StrPrintf(
                                "ASR efficiency check failed for pstate %u with RC = %d\n",
                                DTIUtils::Mislwtils::getLogBase2(pState),
                                (UINT32)rc);
                        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                        return rc;
                    }
                }
            }

            if (m_enableMSCGTest)
            {
                // confirm MSCG is running
                rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_STUTTER_FEATURE_PER_PSTATE, pState, &stutterFeature);
                if (rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                            "Get stutter feature for pstate %u failed with rc = %d\n",
                            DTIUtils::Mislwtils::getLogBase2(pState), (UINT32)rc);
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    return rc;
                }
                if (stutterFeature != LW5070_CTRL_IMP_STUTTER_FEATURE_MSCG)
                {
                    m_reportStr = Utility::StrPrintf(
                            "INFO: %s: IMP deemed MSCG not possible for this mode... skipping MSCG check. (Running is %s)\n",
                            __FUNCTION__,
                            (stutterFeature == LW5070_CTRL_IMP_STUTTER_FEATURE_ASR ? "ASR" :
                             (stutterFeature == LW5070_CTRL_IMP_STUTTER_FEATURE_NONE ? "none" :
                              "<invalid value>")));
                    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                }
                else
                {
                    bool mscgEnabled = false;

                    CHECK_RC(objPgStats.isPGSupported(LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS, &mscgEnabled));

                    // check if MSCG has been disabled by reg key
                    if (!mscgEnabled)
                    {
                        m_reportStr = Utility::StrPrintf(
                                "INFO: %s: MSCG has been disabled by reg key\n",
                                __FUNCTION__);
                        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                    }
                    else
                    {
                        CHECK_RC(objPgStats.isPGEnabled(LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS, &mscgEnabled));
                        if (!mscgEnabled)
                        {
                            m_reportStr = Utility::StrPrintf(
                                    "INFO: %s: IMP considers MSCG possible, but MSCG is not enabled in HW\n",
                                    __FUNCTION__);
                            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                        }
                        else
                        {
                            rc = checkMscgEfficiency(pState);
                            if (rc != OK)
                            {
                                m_reportStr = Utility::StrPrintf(
                                        "MSCG efficiency check failed for pstate %u with RC = %d\n",
                                        DTIUtils::Mislwtils::getLogBase2(pState),
                                        (UINT32)rc);
                                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                                return rc;
                            }
                        }
                    }
                }
            }
        }
    }

    return OK;
}

//! \brief check computed ASR efficiency & compare it with actual efficiency
//------------------------------------------------------------------------------
RC Modeset::checkAsrEfficiency
(
    LwU32 pState
)
{
    RC rc;
    LwU32 predictedEfficiency  = 0;
    LwU32 actualEfficiency = 0;
    LwU32 clkFreq = 0;
    LwU32 fbPartMask = 0;
    LwU32 asrCycCnt = 0;
    LwU32 regOffset = 0;
    LwU32 channelIndex = 0;
    UINT32 partition = 0, channel = 0;
    LwU32 avgPerCycle = 0;
    vector<LwU32> avgPerChannel;
    const LwU32 elapsedMs = 2000; // 2 sec. for sleep time
    vector<string> resultStrsError;
    string separatorStr;
    LwU32 separatorCharNum = 12; // default value is the sum of left-most and right-most fields

    //
    // This const value is copied from "lw50/lw5x_mem_link_training.h" to indicate the max number of partition.
    // And the header file should be included along with "fermi/fermi_fb.h". Manually copy here for now since
    // include "fermi/fermi_fb.h" will cause compile error when building winMods.
    //
    const LwU32 MAX_NUM_PARTITIONS = 10;

    // get callwlated efficiency
    rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_STUTTER_FEATURE_PREDICTED_EFFICIENCY_PER_PSTATE,
                         pState, &predictedEfficiency);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: Get ASR predicted efficiency of stutter feature for pstate %u failed with rc = %d\n",
                __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(pState), (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(getFbPartitionMask(fbPartMask));

    // get ltc2clk which is needed for getting actual efficiency
    CHECK_RC(getClkDomainFreq(LW2080_CTRL_CLK_DOMAIN_LTC2CLK, clkFreq));

    // On some GPUs (GF108) LTC2CLK runs off XBAR2CLK, so ltc2clk might be reported as zero.
    if (clkFreq <= 200)
    {
        // get xbar2clk which is needed for getting actual efficiency
        CHECK_RC(getClkDomainFreq(LW2080_CTRL_CLK_DOMAIN_XBAR2CLK, clkFreq));
        if (clkFreq <= 200)
        {
            m_reportStr = Utility::StrPrintf(
                    "%s: Failed to get valid reference CLK to callwlate actual ASR efficiency\n", __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::SOFTWARE_ERROR;
        }
    }

    m_reportStr = Utility::StrPrintf(
            "ASR: predicted efficiency %2d%% for pstate %u\n"
            "ASR: The output format as \"Iteration index | (Parition ID)-(Channel ID) | ... | Avg |\":\n",
            predictedEfficiency, DTIUtils::Mislwtils::getLogBase2(pState));
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    PRINT_STDOUT_N_REPORT(Tee::PriHigh, "  #  |", PRINT_STDOUT | PRINT_LOG_FILE);
    // prepare header and avg efficiency array for each channel
    for (partition = 0; partition < MAX_NUM_PARTITIONS; ++partition)
    {
        if (!(fbPartMask & (1 << partition)))
        {
            continue;
        }

        for (channel = 0; channel < LW_PFB_FBPA_PM_ASR_CYCCNT__SIZE_1; ++channel)
        {
            m_reportStr = Utility::StrPrintf(" %1u-%1u |", partition, channel);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            separatorCharNum += 6;
            avgPerChannel.push_back(0);
        }
    }
    avgPerChannel.push_back(0); // add one more in the last for the overall avg
    PRINT_STDOUT_N_REPORT(Tee::PriHigh, " Avg |\n", PRINT_STDOUT | PRINT_LOG_FILE);

    separatorStr = std::string(separatorCharNum, '=');
    PRINT_STDOUT_N_REPORT(Tee::PriHigh, separatorStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    // get subdevice
    GpuSubdevice *pSubdev =  m_pDisplay->GetOwningDisplaySubdevice();

    // measure and report efficiency over 10 sample periods
    for (LwU32 i = 1; i <= ASR_MSCG_EFFICIENCY_CHECK_LOOPS; ++i)
    {
        m_reportStr = Utility::StrPrintf("\n %2u  |", i);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        // reset the counter registers
        for (partition = 0; partition < MAX_NUM_PARTITIONS; ++partition)
        {
            if (!(fbPartMask & (1 << partition)))
            {
                continue;
            }

            for (channel = 0; channel < LW_PFB_FBPA_PM_ASR_CYCCNT__SIZE_1; ++channel)
            {
                regOffset = FB_PER_FBPA(LW_PFB_FBPA_PM_ASR_CYCCNT(channel), partition);

                // reset reg value to 0
                pSubdev->RegWr32(regOffset, 0);
            }
        }

        Sleep(elapsedMs, m_runOnEmu, GetBoundGpuSubdevice());

        avgPerCycle = 0;
        channelIndex = 0;
        for (partition = 0; partition < MAX_NUM_PARTITIONS; ++partition)
        {
            if (!(fbPartMask & (1 << partition)))
            {
                continue;
            }

            for (channel = 0; channel < LW_PFB_FBPA_PM_ASR_CYCCNT__SIZE_1; ++channel)
            {
                regOffset = FB_PER_FBPA(LW_PFB_FBPA_PM_ASR_CYCCNT(channel), partition);
                asrCycCnt = pSubdev->RegRd32(regOffset);

                //
                // reference to the callwlation of ASRCheck tool - //hw/notebook/tools/ASRCheck/...
                // Callwlation equation as below and the percentage result from 0 to 100 -
                //
                actualEfficiency = (LwU64)asrCycCnt * 200 / (elapsedMs * (LwU64)clkFreq);

                m_reportStr = Utility::StrPrintf(" %02d%% |", actualEfficiency);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

                avgPerCycle += actualEfficiency;
                avgPerChannel[channelIndex++] += actualEfficiency;
                if (predictedEfficiency > actualEfficiency)
                {
                    m_reportStr = Utility::StrPrintf(
                            "ERROR: ASR Channel %u-%u: probe %u: predicted efficiency %2u%% > actual efficiency %2u%%\n",
                            partition, channel, (unsigned int) i,
                            (unsigned int) predictedEfficiency,
                            (unsigned int) actualEfficiency
                            );
                    resultStrsError.push_back(m_reportStr);
                    rc = RC::SOFTWARE_ERROR;
                }
            }
        }
        avgPerCycle /= channelIndex;
        m_reportStr = Utility::StrPrintf(" %02d%% |", avgPerCycle);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        avgPerChannel[channelIndex] += avgPerCycle;
    }

    PRINT_STDOUT_N_REPORT(Tee::PriHigh, "\n", PRINT_STDOUT | PRINT_LOG_FILE);
    PRINT_STDOUT_N_REPORT(Tee::PriHigh, separatorStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    PRINT_STDOUT_N_REPORT(Tee::PriHigh, "\n Avg |", PRINT_STDOUT | PRINT_LOG_FILE);
    // print out overall avg
    for (LwU32 i = 0; i < avgPerChannel.size(); ++i)
    {
        m_reportStr = Utility::StrPrintf(
                " %02d%% |", (avgPerChannel[i] / ASR_MSCG_EFFICIENCY_CHECK_LOOPS));
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    // print out all the error results from probing
    PRINT_STDOUT_N_REPORT(Tee::PriHigh, "\n", PRINT_STDOUT | PRINT_LOG_FILE);
    for (LwU32 i = 0; i < resultStrsError.size(); ++i)
    {
        PRINT_STDOUT_N_REPORT(Tee::PriHigh, resultStrsError[i].c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    return rc;
}

//! \brief check computed MSCG efficiency & compare it with actual efficiency
//------------------------------------------------------------------------------
RC Modeset::checkMscgEfficiency
(
    LwU32 pState
)
{
    RC rc;
    PgStats objPgStats(GetBoundGpuSubdevice());
    LwU32 predictedEfficiency = 0;
    UINT32 actualEfficiency = 0;
    const LwU32 elapsedMs = m_runOnEmu ? 10 : 2000; // 2 sec. for silicon sleep time; 10 ms for emulator sleep time
    UINT32 gatingCount = 0;
    UINT32 avgGatingUs = 0;
    UINT32 avgEntryUs = 0;
    UINT32 avgExitUs = 0;
    vector<string> resultStrsError;

    // get predicted efficiency
    rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_STUTTER_FEATURE_PREDICTED_EFFICIENCY_PER_PSTATE,
                         pState, &predictedEfficiency);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "Get MSCG predicted efficiency of stutter feature for pstate %u failed with rc = %d\n",
                DTIUtils::Mislwtils::getLogBase2(pState), (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }

    if (!m_bIsPS30Supported)
    {
        m_reportStr = Utility::StrPrintf(
                "MSCG: predicted efficiency %2d%% for pstate %u\n", predictedEfficiency,
                DTIUtils::Mislwtils::getLogBase2(pState));
    }
    else
    {
        m_reportStr = Utility::StrPrintf(
                "MSCG: predicted efficiency for lowest IMP vP-State\n");
    }
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    // measure and report efficiency over 10 sample periods
    for (LwU32 i = 1; i <= ASR_MSCG_EFFICIENCY_CHECK_LOOPS; ++i)
    {
        Sleep(elapsedMs, m_runOnEmu, GetBoundGpuSubdevice());

        CHECK_RC(objPgStats.getPGPercent(LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS, m_runOnEmu, &actualEfficiency,
                                         &gatingCount, &avgGatingUs, &avgEntryUs, &avgExitUs));
        if (predictedEfficiency > (actualEfficiency + MSCG_EFFICIENCY_TOLERANCE))
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: probe %u: predicted efficiency %2u%% > actual efficiency %2d%% + tolerance(%2d%%)\n",
                    (unsigned int) i,
                    (unsigned int) predictedEfficiency,
                    actualEfficiency,
                    MSCG_EFFICIENCY_TOLERANCE
                    );
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            resultStrsError.push_back(m_reportStr);

            rc = RC::SOFTWARE_ERROR;
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "MSCG: probe %d: actual efficiency %2d%%\n", i, actualEfficiency);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }

    // print out all the error results from probing
    PRINT_STDOUT_N_REPORT(Tee::PriHigh, "\n", PRINT_STDOUT | PRINT_LOG_FILE);
    for (LwU32 i = 0; i < resultStrsError.size(); ++i)
    {
        PRINT_STDOUT_N_REPORT(Tee::PriHigh, resultStrsError[i].c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    if (resultStrsError.size() <= MSCG_EFFICIENCY_CHECK_FAILED_TOLERANCE)
    {
        rc.Clear();
    }

    return rc;
}

//! \brief Reset IMP related features to original settings
//------------------------------------------------------------------------------
RC Modeset::resetIMPParameters()
{
    LW5070_CTRL_IMP_SET_GET_PARAMETER_PARAMS params = {{0}, 0};

    params.base.subdeviceIndex = m_pDisplay->GetMasterSubdevice();
    params.index = LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_NONE;
    params.pstateApi = LW2080_CTRL_PERF_PSTATES_UNDEFINED;
    params.operation = LW5070_CTRL_IMP_SET_GET_PARAMETER_OPERATION_RESET;

    return m_pDisplay->RmControl5070(LW5070_CTRL_CMD_IMP_SET_GET_PARAMETER,
                                     &params, sizeof(params));
}

//! \brief Set parameter of IMP features via LW5070_CTRL_CMD_IMP_SET_GET_PARAMETER interface
//
//  \param index        LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_xxx
//  \param pstateApi    pstateApi. One of LW2080_CTRL_PERF_PSTATES_Pxxx. For
//                      indices not needing a pstateApi use LW2080_CTRL_PERF_PSTATES_UNDEFINED
//  \param value        the value to set
//
RC Modeset::setIMPParameter(LwU32 index, LwU32 pstateApi, LwU32 value)
{
    LW5070_CTRL_IMP_SET_GET_PARAMETER_PARAMS params = {{0}, 0};

    params.base.subdeviceIndex = m_pDisplay->GetMasterSubdevice();
    params.index = index;
    params.value = value;
    params.pstateApi = pstateApi;
    params.operation = LW5070_CTRL_IMP_SET_GET_PARAMETER_OPERATION_SET;

    return m_pDisplay->RmControl5070(LW5070_CTRL_CMD_IMP_SET_GET_PARAMETER,
                                     &params, sizeof(params));
}

//! \brief Get parameter of IMP features via LW5070_CTRL_CMD_IMP_SET_GET_PARAMETER interface
//
//  \param index        LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_xxx
//  \param pstateApi    pstateApi. One of LW2080_CTRL_PERF_PSTATES_Pxxx defines. For
//                      indices not needing a pstateApi use LW2080_CTRL_PERF_PSTATES_UNDEFINED
//  \param value[out]   extracted value of the specified parameter
//  \param head         Head index, which is required when querying Mclk switch feature
//
RC Modeset::getIMPParameter(LwU32 index, LwU32 pstateApi, LwU32 *value, LwU32 head)
{
    LW5070_CTRL_IMP_SET_GET_PARAMETER_PARAMS params = {{0}, 0};
    RC rc;

    params.base.subdeviceIndex = m_pDisplay->GetMasterSubdevice();
    params.index = index;
    params.pstateApi = pstateApi;
    params.head = head;
    params.operation = LW5070_CTRL_IMP_SET_GET_PARAMETER_OPERATION_GET;

    rc = m_pDisplay->RmControl5070(LW5070_CTRL_CMD_IMP_SET_GET_PARAMETER,
                                   &params, sizeof(params));

    if (rc == OK)
    {
        *value = params.value;
    }
    return rc;
}

//! \brief gets FB partition mask
//------------------------------------------------------------------------------
RC Modeset::getFbPartitionMask(LwU32 &partMask)
{
    RC rc;
    LW2080_CTRL_FB_INFO fbInfo = {0};
    LW2080_CTRL_FB_GET_INFO_PARAMS fbGetInfoParams = {0};

    // get FB partition info
    fbInfo.index = LW2080_CTRL_FB_INFO_INDEX_PARTITION_MASK;
    fbGetInfoParams.fbInfoList     = LW_PTR_TO_LwP64(&fbInfo);
    fbGetInfoParams.fbInfoListSize = 1;
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_FB_GET_INFO,
                        &fbGetInfoParams, sizeof(fbGetInfoParams));
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: LW2080_CTRL_CMD_FB_GET_INFO failed with rc = %d\n", __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }
    partMask = fbInfo.data;
    return OK;
}

//! \brief gets freq of specified clk domain
//------------------------------------------------------------------------------
RC Modeset::getClkDomainFreq(LwU32 clkDomain, LwU32 &clkFreq)
{
    RC rc;
    LW2080_CTRL_CLK_INFO clkInfo = {0};
    LW2080_CTRL_CLK_GET_INFO_PARAMS clkGetInfoParams = {0};

    clkGetInfoParams.flags = 0;
    clkGetInfoParams.clkInfoListSize = 1;
    clkGetInfoParams.clkInfoList = LW_PTR_TO_LwP64(&clkInfo);
    clkInfo.flags = 0;
    clkInfo.clkDomain = clkDomain;
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_CLK_GET_INFO,
                        &clkGetInfoParams, sizeof(clkGetInfoParams));
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: LW2080_CTRL_CLK_GET_INFO failed with rc = %d\n", __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }
    clkFreq = clkInfo.actualFreq;
    return OK;
}

RC Modeset::RandomPStateTransitions(UINT32 MinPStateMask, UINT32 iterNum, UINT32 durationSecs, bool bCheckUnderflow)
{
    RC rc;
    vector<LW2080_CTRL_CLK_EXTENDED_INFO> MismatchedClks;
    string errorCause = " ";

    if (m_disableRandomPStateTest)
    {
        return OK;
    }

    rc = m_objSwitchRandomPState.RandomPStateTransitionsTest(iterNum,
                               m_randomPStateSwitchIntervalMS,
                               MinPStateMask,
                               false,
                              &MismatchedClks,
                               m_freqTolerance100x,
                               durationSecs,
                               bCheckUnderflow,
                               m_ignoreClockDiffError);

    if (rc != OK)
    {
        if (rc == RC::DATA_MISMATCH || rc == RC::CLOCKS_TOO_LOW)
        {
            RmtClockUtil clkUtil;
            string clkErrorString;
            for (UINT32 countMismatch = 0; countMismatch < MismatchedClks.size(); countMismatch++)
            {
                double fPercentDiff = ((double)MismatchedClks[countMismatch].effectiveFreq  -
                    (double)MismatchedClks[countMismatch].clkInfo.targetFreq) * 100.0 /
                    (double)(MismatchedClks[countMismatch].clkInfo.targetFreq);

                clkErrorString = Utility::StrPrintf("\nClock Difference Found =>\n"
                    "Clk Name: %13s,\n"
                    "Source: %10s,\n"
                    "Flag: 0x%x,\n"
                    "Expected Freq: %7d KHz,\n"
                    "Measured Freq (SW): %7d KHz,\n"
                    "Measured Freq (HW): %7d KHz,\n"
                    "Freq Diff: %+5.4f %%,\n",
                    clkUtil.GetClkDomainName(MismatchedClks[countMismatch].clkInfo.clkDomain),
                    clkUtil.GetClkSourceName(MismatchedClks[countMismatch].clkInfo.clkSource),
                    (UINT32)(MismatchedClks[countMismatch].clkInfo.flags),
                    (UINT32)(MismatchedClks[countMismatch].clkInfo.targetFreq),
                    (UINT32)(MismatchedClks[countMismatch].clkInfo.actualFreq),
                    (UINT32)(MismatchedClks[countMismatch].effectiveFreq),
                    fPercentDiff);

                errorCause += string(clkErrorString);

                PRINT_STDOUT_N_REPORT(Tee::PriHigh, clkErrorString.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            }

            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Clock Diff Found during RandomPStateTransitions\n",
                    __FUNCTION__);

            m_clockDiffFound = true;

            if (m_ignoreClockDiffError)
                rc.Clear();
            MismatchedClks.clear();
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: RandomPStateTransitions failed with RC=%d\n",
                    __FUNCTION__, (UINT32)rc);
        }

        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }
    return rc;
}

//! \brief check all subtests results after setMode
//
// Note that it will also update the "comment" field in CSV file.
//------------------------------------------------------------------------------
RC Modeset::checkSubTestResults
(
    const LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *pDispHeadMapParams,
    const DisplayIDs *pDispIdsForLwrrRun,
    UINT32 porMinPState
)
{
    RC rc;
    UINT32 minPStateMclkSwitchPossible = LW2080_CTRL_PERF_PSTATES_UNDEFINED;
    char minIMPPStateStr[50] = "Undefined";
    char minMClkPStateStr[50] = "Undefined";
    char minPORPStateStr[50] = "Undefined";

    m_bMclkSwitchPossible = false;

    // Step 1) Let's find if MCLK Switch is possible or not for current display config.
    // Note: This needs to be found first as we should NOT do any pstate switches
    // if MCLK switch is NOT possible else it will surely underflow. Confirmed with HW folks.
    rc = isMclkSwitchPossible(pDispHeadMapParams, pDispIdsForLwrrRun,
                              m_bMclkSwitchPossible, minPStateMclkSwitchPossible);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: isMclkSwitchPossible() failed with RC = %d",
                __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    if (!m_bIsPS30Supported)
    {
        if (m_lwrrImpMinPState != LW2080_CTRL_PERF_PSTATES_UNDEFINED)
        {
            sprintf(minIMPPStateStr, "P%u", DTIUtils::Mislwtils::getLogBase2(m_lwrrImpMinPState));
        }

        if (minPStateMclkSwitchPossible != LW2080_CTRL_PERF_PSTATES_UNDEFINED)
        {
            sprintf(minMClkPStateStr, "P%u", DTIUtils::Mislwtils::getLogBase2(minPStateMclkSwitchPossible));
        }

        if (porMinPState != LW2080_CTRL_PERF_PSTATES_UNDEFINED)
        {
            sprintf(minPORPStateStr, "P%u", DTIUtils::Mislwtils::getLogBase2(porMinPState));
        }
    }
    else
    {
        if (m_lwrrImpMinPState != IMPTEST3_VPSTATE_UNDEFINED)
        {
            sprintf(minIMPPStateStr, "vP%u", m_lwrrImpMinPState);
        }

        if (minPStateMclkSwitchPossible != IMPTEST3_VPSTATE_UNDEFINED)
        {
            sprintf(minMClkPStateStr, "vP%u", minPStateMclkSwitchPossible);
        }

        if (porMinPState != IMPTEST3_VPSTATE_UNDEFINED)
        {
            sprintf(minPORPStateStr, "vP%u", porMinPState);
        }
    }

    // Step 2) Let's report the minPState for IMP, MClk Switch and POR.
    // also adjust the porMinPState
    m_reportStr = Utility::StrPrintf(
            "IMP MinPState = %s, MClk Switch MinPState = %s, POR MinPState = %s\n",
            minIMPPStateStr, minMClkPStateStr, minPORPStateStr);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    // update the POR MinPState based on IMP and display glitch
    if (!m_bIsPS30Supported)
    {
        if (m_bMclkSwitchPossible &&
            minPStateMclkSwitchPossible != LW2080_CTRL_PERF_PSTATES_UNDEFINED &&
            porMinPState != LW2080_CTRL_PERF_PSTATES_UNDEFINED &&
            porMinPState > minPStateMclkSwitchPossible)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: POR min pstate: P%u is lower than minPStateMclkSwitchPossible: P%u allowed by RM. Using minPStateMclkSwitchPossible value for this ASR/MSCG verification.",
                    __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(porMinPState), DTIUtils::Mislwtils::getLogBase2(minPStateMclkSwitchPossible));
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            porMinPState = minPStateMclkSwitchPossible;
        }

        if (porMinPState != LW2080_CTRL_PERF_PSTATES_UNDEFINED &&
            porMinPState > m_lwrrImpMinPState)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: POR min pstate - P%u is lower than min pstate - P%u allowed by RM",
                    __FUNCTION__, DTIUtils::Mislwtils::getLogBase2(porMinPState), DTIUtils::Mislwtils::getLogBase2(m_lwrrImpMinPState));

            outputCommentStr("FAIL,");
            return RC::SOFTWARE_ERROR;
        }
    }
    else
    {
        if (m_bMclkSwitchPossible &&
            minPStateMclkSwitchPossible != IMPTEST3_VPSTATE_UNDEFINED &&
            porMinPState != IMPTEST3_VPSTATE_UNDEFINED &&
            porMinPState < minPStateMclkSwitchPossible)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: POR min vPState: vP%u is lower than "
                    "minPStateMclkSwitchPossible: vP%u allowed by RM. "
                    "Using minPStateMclkSwitchPossible value for this "
                    "ASR/MSCG verification.\n",
                    __FUNCTION__, porMinPState, minPStateMclkSwitchPossible);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            porMinPState = minPStateMclkSwitchPossible;
        }

        if (porMinPState != IMPTEST3_VPSTATE_UNDEFINED &&
            porMinPState < m_lwrrImpMinPState)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: POR min vPState - vP%u is lower than min "
                    "vPState - vP%u allowed by RM\n",
                    __FUNCTION__, porMinPState, m_lwrrImpMinPState);

            outputCommentStr("FAIL,");
            return RC::SOFTWARE_ERROR;
        }
    }

    // Step 3: ImmedFlipTest: does critical watermark testing/immediate flip mode test
    // Valid only for winmods as it launches Apps like 3DMark for verif.
    // Note: Pstate switches in below checkImmediateFlip( ) must check m_bMclkSwitchPossible status
    if (m_enableImmedFlipTest)
    {
        rc = checkImmediateFlip((*pDispIdsForLwrrRun)[0], porMinPState);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Immediate Flip Test failed with RC = %d",
                    __FUNCTION__, (UINT32)rc);

            outputCommentStr("FAIL,");
            return rc;
        }
    }

    if (!m_PStateSupport)
    {
        // return early since below subtests need PState support
        return rc;
    }

    // Step 4: Check ASR/MSCG :
    // TODO: Decide clock switch strategy in checkAsrMscgTestResults( ) if m_bMclkSwitchPossible = false.
    if (m_enableASRTest || m_enableMSCGTest)
    {
        rc = checkAsrMscgTestResults();
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: ASR/MSCG Test failed with RC = %d",
                    __FUNCTION__, (UINT32)rc);

            outputCommentStr("FAIL,");
            return rc;
        }
    }

    // Step 5: checkMclkSwitchGlitch
    if (m_bMclkSwitchPossible)
    {
        // TODO: Discuss to find if we should pass RM IMP or POR MinPstate here ?
        rc = checkMclkSwitchGlitch(pDispIdsForLwrrRun, porMinPState);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: MCLK Switch Glitch Test failed with RC = %d",
                    __FUNCTION__, (UINT32)rc);

            outputCommentStr("FAIL,");
            return rc;
        }
    }
    else
    {
        m_reportStr = Utility::StrPrintf(
                "INFO: %s: MCLK Switch is NOT possible. "
                "Hence ignoring checkMclkSwitchGlitch( ).\n",
                __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    return OK;
}

//! \brief check if system had installed 3DMarkVantage and initialize app
//         name and its install location, return false if not installed
//
//------------------------------------------------------------------------------
bool Modeset::check3DMarkVantageInstallation()
{
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    HKEY hKey, hSubKey;
    DWORD dwRtn, dwIndex, dwSubKeyLength;
    char subKey[256] = {0};
    DWORD type = REG_SZ;
    char  appName[256] = {0};
    DWORD appNameSize;
    char  appInstallLoc[256] = {0};
    DWORD appInstallLocSize;

    //
    // Query the regkey to find the 3DMark application.  Specify KEY_WOW64_32KEY
    // to make sure we search the 32-bit reg keys (since 3DMark Vantage is a
    // 32-bit application), even though ImpTest3 may be compiled as a 64-bit
    // application and running on a 64-bit OS.
    //
    dwRtn = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                         "SOFTWARE\\Microsoft\\Windows\\LwrrentVersion\\Uninstall",
                         0,
                         KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_32KEY,
                         &hKey);
    if (dwRtn == ERROR_SUCCESS)
    {
        dwIndex = 0;
        while (dwRtn == ERROR_SUCCESS)
        {
            dwSubKeyLength = 256;
            dwRtn = RegEnumKeyEx(hKey, dwIndex++, subKey, &dwSubKeyLength, NULL, NULL, NULL, NULL);

            if (RegOpenKeyEx(hKey, subKey, 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &hSubKey) == ERROR_SUCCESS)
            {
                appNameSize = sizeof(appName);
                if (RegQueryValueEx(hSubKey, "DisplayName", NULL, &type, (LPBYTE)appName, &appNameSize) == ERROR_SUCCESS)
                {
                    if ((strcmp(appName, "3DMark Vantage") == 0))
                    {
                        appInstallLocSize = sizeof(appInstallLoc);
                        if (RegQueryValueEx(hSubKey, "InstallLocation", NULL, &type,
                                            (LPBYTE)appInstallLoc, &appInstallLocSize) == ERROR_SUCCESS)
                        {
                            m_appInstallLoc3DMarkV = appInstallLoc;
                            return true;
                        }
                    }
                }
            }
        }
    }
#else
    Printf(Tee::PriHigh, "%s not supported on non winmods platform", __FUNCTION__);
#endif
    return false;
}

//! \brief create config file for running 3DMarkVantage via command line
RC Modeset::create3DMarkVantageConfig(string fileName, UINT32 width, UINT32 height)
{
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    RC rc;
    ifstream configFileTemplate;
    ofstream configFile;
    string configLine;
    size_t tokenPos;

    configFileTemplate.open(m_appConfigFile3DMarkV.c_str());

    if (!configFileTemplate.is_open())
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Failed to open file %s.\n",
                __FUNCTION__, m_appConfigFile3DMarkV.c_str());
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT);
        rc = RC::CANNOT_OPEN_FILE;
        goto Cleanup;
    }

    configFile.open(fileName.c_str(), ios::trunc);
    if (!configFile.is_open())
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Failed to open file %s.\n",
                __FUNCTION__, fileName.c_str());
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT);
        rc = RC::CANNOT_OPEN_FILE;
        goto Cleanup;
    }

    while (getline(configFileTemplate, configLine))
    {
        tokenPos = configLine.find("// resolution <width>x<height>");
        if (tokenPos != string::npos)
        {
            configFile << "        resolution " << width << "x" << height << std::endl;
        }
        else
        {
            configFile << configLine << std::endl;
        }
    }

    configFile.flush();

Cleanup:
    configFile.close();
    configFileTemplate.close();
    return rc;
#else
    Printf(Tee::PriHigh, "%s not supported on non winmods platform", __FUNCTION__);
    return RC::LWRM_NOT_SUPPORTED;
#endif
}

//! \brief return if mclk switch is possible for current mode
RC Modeset::isMclkSwitchPossible
(
    const LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *pDispHeadMapParams,
    const DisplayIDs *pDispIdsForLwrrRun,
    bool &bMclkSwitchPossible,
    UINT32 &minPStateMclkSwitchPossible
)
{
    RC rc;
    UINT32 head = RC::ILWALID_HEAD;
    LwU32 mclkSwitchSetting;
    bMclkSwitchPossible = false;
    minPStateMclkSwitchPossible =
        (m_bIsPS30Supported) ? IMPTEST3_VPSTATE_UNDEFINED : LW2080_CTRL_PERF_PSTATES_UNDEFINED;

    if (pDispIdsForLwrrRun->size() == 0)
    {
        return RC::SOFTWARE_ERROR;
    }

    if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask((*pDispIdsForLwrrRun)[0], *pDispHeadMapParams, &head)) != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: GetHeadFromRoutingMask Failed to Get Head for DispID = 0x%08X. RC = %d\n ",
                __FUNCTION__, (UINT32)(*pDispIdsForLwrrRun)[0], (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    // get state for mclk switch possible
    rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_MCLK_SWITCH_FEATURE_OUTPUTS,
                         LW2080_CTRL_PERF_PSTATES_UNDEFINED, &mclkSwitchSetting, head);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
                "Get mclk switch output for head %u failed with rc = %d\n",
                head, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }

    m_reportStr = "------------------ IMP: MCLK Switch Possibility Analysis ------------------\n";
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    bMclkSwitchPossible = FLD_TEST_DRF(5070_CTRL_IMP_SET_GET_PARAMETER_INDEX, _MCLK_SWITCH_FEATURE_OUTPUTS,
                                       _VALUE_POSSIBLE, _YES, mclkSwitchSetting);
    m_reportStr = Utility::StrPrintf(
            "IMP: MCLK Switch: Possible: %s\n", (bMclkSwitchPossible ? "YES" : "NO"));
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    // get the min pstate if mclk switch is possible
    if (bMclkSwitchPossible)
    {
        char minPStateStr[] = "No limit";
        if (!m_bIsPS30Supported)
        {
            LW2080_CTRL_PERF_GET_ACTIVE_LIMITS_PARAMS getActiveLimitsParams = {0};

            rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_PERF_GET_ACTIVE_LIMITS,
                                &getActiveLimitsParams,
                                sizeof(getActiveLimitsParams));
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "LW2080_CTRL_CMD_PERF_GET_ACTIVE_LIMITS failed with RC = %d",
                        (UINT32)rc);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return rc;
            }

            // look through the active limits and see if PERFCTL_SUSPEND_DISPLAY_GLITCH in the list
            for (LwU32 i = 0; i < getActiveLimitsParams.perfNumActiveLimits; i++)
            {
                if (getActiveLimitsParams.perfLimitStatusList[i].limitId == LW2080_CTRL_PERF_LIMIT_DISPLAY_GLITCH)
                {
                    if (getActiveLimitsParams.perfLimitStatusList[i].pstate != LW2080_CTRL_PERF_LIMIT_ILWALID)
                    {
                        minPStateMclkSwitchPossible = BIT(getActiveLimitsParams.perfLimitStatusList[i].pstate);
                        sprintf(minPStateStr, "P%u", getActiveLimitsParams.perfLimitStatusList[i].pstate);
                    }
                    break;
                }
            }
        }
        else
        {
            // Reading if display glitch
            LW2080_CTRL_PERF_LIMIT_GET_STATUS limitGet = {0};
            LW2080_CTRL_PERF_LIMITS_GET_STATUS_PARAMS getLimitsParams = {0};
            limitGet.limitId = LW2080_CTRL_PERF_LIMIT_DISPLAY_GLITCH;
            getLimitsParams.numLimits = 1;
            getLimitsParams.pLimits = LW_PTR_TO_LwP64(&limitGet);
            rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                LW2080_CTRL_CMD_PERF_LIMITS_GET_STATUS,
                                &getLimitsParams,
                                sizeof(getLimitsParams));
            if (rc != OK)
            {
                m_reportStr = Utility::StrPrintf(
                        "LW2080_CTRL_CMD_PERF_LIMITS_GET_STATUS failed with RC = %d\n",
                        (UINT32)rc);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                return rc;
            }
            if (limitGet.output.bEnabled)
            {
                minPStateMclkSwitchPossible = limitGet.output.value;
                sprintf(minPStateStr, "vP%u", minPStateMclkSwitchPossible);
            }

        }

        m_reportStr = Utility::StrPrintf(
                "IMP: MCLK Switch: Min PState: %s\n", minPStateStr);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    // whether mclk switch is possible or not, print out all related settings
    m_reportStr = Utility::StrPrintf(
            "IMP: MCLK Switch: Override Mempool: %s\n",
            (FLD_TEST_DRF(5070_CTRL_IMP_SET_GET_PARAMETER_INDEX, _MCLK_SWITCH_FEATURE_OUTPUTS,
                          _VALUE_OVERRIDE_MEMPOOL, _YES, mclkSwitchSetting) ? "YES" : "NO"));
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    for (LwU32 i = 0; i < pDispIdsForLwrrRun->size(); i++)
    {
        if ((rc = DTIUtils::Mislwtils::GetHeadFromRoutingMask((*pDispIdsForLwrrRun)[i], *pDispHeadMapParams, &head)) != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetHeadFromRoutingMask Failed to Get Head for DispID = 0x%08x. rc = %d\n ",
                    __FUNCTION__, (UINT32)(*pDispIdsForLwrrRun)[i], (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }

        rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_MCLK_SWITCH_FEATURE_OUTPUTS,
                             LW2080_CTRL_PERF_PSTATES_UNDEFINED, &mclkSwitchSetting, head);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "Get mclk switch output for head %u failed with rc = %d\n",
                    head, (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::SOFTWARE_ERROR;
        }

        m_reportStr = Utility::StrPrintf(
                "IMP: MCLK Switch: Mid Watermark for head %u (DisplayID = 0x%08x): %s\n",
                head, (UINT32)(*pDispIdsForLwrrRun)[i],
                (FLD_TEST_DRF(5070_CTRL_IMP_SET_GET_PARAMETER_INDEX, _MCLK_SWITCH_FEATURE_OUTPUTS,
                              _VALUE_MID_WATERMARK, _YES, mclkSwitchSetting) ? "YES" : "NO"));
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        m_reportStr = Utility::StrPrintf(
                "IMP: MCLK Switch: DWCF for head %u (DisplayID = 0x%08x): %s\n",
                head, (UINT32)(*pDispIdsForLwrrRun)[i],
                (FLD_TEST_DRF(5070_CTRL_IMP_SET_GET_PARAMETER_INDEX, _MCLK_SWITCH_FEATURE_OUTPUTS,
                              _VALUE_DWCF, _YES, mclkSwitchSetting) ? "YES" : "NO"));
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }
    m_reportStr = "--------------- IMP: MCLK Switch Possibility Analysis Ends ---------------\n";
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    return OK;
}

//! \brief mclk switch glitch test
//------------------------------------------------------------------------------
RC Modeset::checkMclkSwitchGlitch
(
    const DisplayIDs        *pDispIdsForLwrrRun,
    UINT32                  porMinPState
)
{
    RC rc;
    vector<LwU32> supportedPerfStates;
    if (!m_bIsPS30Supported)
    {
        LW2080_CTRL_PERF_GET_PSTATES_INFO_PARAMS PStatesInfoParams = {0};
        // check what pstates are supported
        rc = m_objSwitchRandomPState.GetSupportedPStatesInfo(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                                                             PStatesInfoParams,
                                                             supportedPerfStates);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "GetSupportedPStatesInfo failed with RC = %d\n", (UINT32) rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
        //
        // loop through from POR minPState to maxPState (P0) and see if any CRC mismatch & underflow
        // occur during pstate switch.
        //
        for (UINT32 i = porMinPState; i >= (UINT32)P0; i = i >> 1)
        {
            if (std::count(supportedPerfStates.begin(), supportedPerfStates.end(), (UINT32)i))
            {
                CHECK_RC(checkCRC(pDispIdsForLwrrRun, i));
            }
        }
    }
    else
    {
        LW2080_CTRL_PERF_VPSTATES_INFO virtualPStatesInfo;
        vector<LwU32> virtualPStates;
        rc = m_objSwitchRandomPState.GetSupportedIMPVPStatesInfo(
            pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
            &virtualPStatesInfo,
            &virtualPStates,
            &supportedPerfStates);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "GetSupportedIMPVPStatesInfo failed with RC = %d\n",
                    (UINT32) rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
        for (UINT32 i = porMinPState; i < supportedPerfStates.size(); i++)
        {
            CHECK_RC(checkCRC(pDispIdsForLwrrRun, i));
        }
    }

    return OK;
}
//! \brief check CRC when we transition a vP/P-State.
RC Modeset::checkCRC
(
    const DisplayIDs        *pDispIdsForLwrrRun,
    UINT32                  perfState
)
{
    RC rc;
    UINT32 compCRC;
    UINT32 priCRC;
    UINT32 secCRC;
    vector<UINT32> compGoldenCRCs;
    vector<UINT32> priGoldenCRCs;
    vector<UINT32> secGoldenCRCs;
    // extract CRCs before pstate switch
    compGoldenCRCs.clear();
    priGoldenCRCs.clear();
    secGoldenCRCs.clear();
    for (LwU32 disp = 0; disp < pDispIdsForLwrrRun->size(); disp++)
    {
        rc = getCRCValues((*pDispIdsForLwrrRun)[disp], compCRC, priCRC, secCRC);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Get CRC values failed with rc = %d\n",
                    __FUNCTION__, (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
        compGoldenCRCs.push_back(compCRC);
        priGoldenCRCs.push_back(priCRC);
        secGoldenCRCs.push_back(secCRC);
    }
    m_reportStr = Utility::StrPrintf(
            "Starting %s Transition to %s",
            m_PerfStatePrint,
            printPerfStateHelper(perfState).c_str());
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    CHECK_RC(ProgramAndVerifyPState(perfState));

    for (LwU32 disp = 0; disp < pDispIdsForLwrrRun->size(); disp++)
    {
        rc = getCRCValues((*pDispIdsForLwrrRun)[disp], compCRC, priCRC, secCRC);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Get CRC values failed with rc = %d\n",
                    __FUNCTION__, (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }

        if ((compCRC != compGoldenCRCs[disp]) ||
            (priCRC != priGoldenCRCs[disp]) ||
            (secCRC != secGoldenCRCs[disp]))
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: CRC mismatch with golden ones\n",
                    __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return RC::SOFTWARE_ERROR;
        }
    }
    return OK;
}
//! \brief Ask IMP to start iso fb latency measurement and maintain the max latency record.
RC Modeset::StartIsoFbLatencyTest()
{
    RC rc;

    m_reportStr = "Starting Iso Fb Latency Test\n";
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    if (m_isoFbLatencyTest < ISOFBLATENCY_THRESHOLD)
    {
        m_reportStr = Utility::StrPrintf(
            "Warning: %s: isoFbLatencyTest %d is less than %d\n."
            "isoFbLatencyTest min record time should be large enough to reveal a problem.\n",
            __FUNCTION__, m_isoFbLatencyTest, ISOFBLATENCY_THRESHOLD);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    // Enable ISO FB Latency Measurement
    rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_ISOFBLATENCY_TEST_ENABLE,
                      LW2080_CTRL_PERF_PSTATES_UNDEFINED, TRUE);

    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Enable Iso Fb Latency Measurement failed with RC = %d\n",
             __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
    }

    return rc;
}

//! \brief Ends iso fb latency measurement and get the callwlated result for this test.
RC Modeset::EndIsoFbLatencyTest()
{
    RC rc;

    m_reportStr = "Stopping Iso Fb Latency Test\n";
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    // Disable ISO FB Latency Measurement
    rc = setIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_ISOFBLATENCY_TEST_ENABLE,
                      LW2080_CTRL_PERF_PSTATES_UNDEFINED, FALSE);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Disable Iso Fb Latency Measurement failed with RC = %d\n",
             __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    // Verify ISO FB Latency Measurement Result
    rc = VerifyIsoFbLatencyTestResult();
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Get Iso Fb Latency Measurement result failed with RC = %d\n",
             __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }

    return rc;
}

//! \brief Verify iso fb latency measurement data and returns the final result.
RC Modeset::VerifyIsoFbLatencyTestResult()
{
    RC rc;
    LwU32 wcTotalLatency;
    LwU32 maxLatency;
    LwU32 maxTestPeriod;

    // Get ISO FB Latency Test worst case total latency
    rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_ISOFBLATENCY_TEST_WC_TOTAL_LATENCY,
                      LW2080_CTRL_PERF_PSTATES_UNDEFINED, &wcTotalLatency, 0);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Get ISO FB Latency Test worst case total latency failed with RC = %d\n",
             __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }
    // Get ISO FB Latency Test max latency
    rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_ISOFBLATENCY_TEST_MAX_LATENCY,
                      LW2080_CTRL_PERF_PSTATES_UNDEFINED, &maxLatency, 0);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
            "%s: Get ISO FB Latency Test max latency failed with RC = %d\n",
             __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }
    // Get ISO FB Latency Test max test period
    rc = getIMPParameter(LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_ISOFBLATENCY_TEST_MAX_TEST_PERIOD,
                      LW2080_CTRL_PERF_PSTATES_UNDEFINED, &maxTestPeriod, 0);
    if (rc != OK)
    {
        m_reportStr = Utility::StrPrintf(
              "%s: Get ISO FB Latency Test max test period failed with RC = %d\n",
              __FUNCTION__, (UINT32)rc);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return rc;
    }
    m_reportStr = Utility::StrPrintf(
          "Iso FB Latency Test Result: WcTotalLatency = %dns, MaxLatency = %dns, MaxTestPeriod = %ds\n",
          wcTotalLatency, maxLatency, maxTestPeriod);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    if (maxLatency > wcTotalLatency)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: ISO FB Latency Test failed because the max latency (%dns) we recorded"
                         "exceeds the worst case total latency (%dns) callwlated by IMP\n",
             __FUNCTION__, maxLatency, wcTotalLatency);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }
    if (maxTestPeriod < m_isoFbLatencyTest)
    {
        m_reportStr = Utility::StrPrintf(
                "%s: ISO FB Latency Test failed because the max test period (%ds)"
                         " does not satisfy the -isoFbLatencyTest threshold (%ds)\n",
              __FUNCTION__, maxTestPeriod, m_isoFbLatencyTest);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

//! \brief critical watermark testing/immediate flip mode test
//------------------------------------------------------------------------------
RC Modeset::checkImmediateFlip
(
    DisplayID primaryDisplay,
    UINT32 porMinPState
)
{
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    RC rc;
    LwS32   startXPos       = 0;
    LwS32   startYPos       = 0;
    LwU32   width           = 0;
    LwU32   height          = 0;
    char appParams[300];
    char lwrrDir[BUFFER_SIZE_LWRRENT_DIR] = {0};
    PgStats objPgStats(GetBoundGpuSubdevice());
    SHELLEXELWTEINFO ShExecInfo;
    UINT32 actualEfficiency = 0;
    UINT32 gatingCount = 0;
    UINT32 avgGatingUs = 0;
    UINT32 avgEntryUs = 0;
    UINT32 avgExitUs = 0;
    UINT32 underflowHead;

    //
    // Critical watermark/immediate flip test flow
    //
    // 1. Force pstate to minPstate (including display glitch)
    // 2. Launch 3DMarkVantage
    // 3. Check for MSCG efficiency == 0 & underflow for a while
    // 4. Random loop through MinPstate to MaxPstate for few iterations if MCLK switch is possible.
    // 5. During #4, Check for underflow
    // 6. Close 3DMarkVantage
    //

    rc =  GetDisplayStartPosAndSize(primaryDisplay,
                                    &startXPos, &startYPos,
                                    &width, &height);

    if (rc != OK)
    {
        if (rc == RC::NO_DISPLAY_DEVICE_CONNECTED)
        {
            rc = OK;
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetDisplayStartPosAndSize() Failed with RC = %d."
                    " Skipping immediate flip test on display id =0x%08x\n",
                    __FUNCTION__, (UINT32)rc, primaryDisplay);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            return rc;
        }
    }

    if (m_PStateSupport)
    {
        if (m_bMclkSwitchPossible)
        {
            m_reportStr = Utility::StrPrintf(
                    "Starting %s Transition to (POR) %s \n",
                    m_PerfStatePrint,
                    printPerfStateHelper(porMinPState).c_str());
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            CHECK_RC(ProgramAndVerifyPState(porMinPState));
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "WARNING: %s: MCLK Switch is NOT possible. So skipping %s Switch.",
                    __FUNCTION__, m_PerfStatePrint);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }

    // Need to pass in the config file when launching 3DMarkVantage via command line.
    CHECK_RC(create3DMarkVantageConfig("3DMarkVConfig.txt", width, height));
    if (!GetLwrrentDirectory(BUFFER_SIZE_LWRRENT_DIR, lwrrDir))
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: GetLwrrentDirectory() failed.\n", __FUNCTION__);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }

    sprintf(appParams, "--config_file=\"%s\\%s\"", lwrrDir, "3DMarkVConfig.txt");

    ShExecInfo.cbSize       = sizeof(SHELLEXELWTEINFO);
    ShExecInfo.fMask        = NULL;
    ShExecInfo.hwnd         = NULL;
    ShExecInfo.lpVerb       = "open";
    ShExecInfo.lpFile       = m_appName3DMarkV.c_str();
    ShExecInfo.lpParameters = appParams;
    ShExecInfo.lpDirectory  = m_appInstallLoc3DMarkV.c_str();
    ShExecInfo.nShow        = SW_MAXIMIZE;
    ShExecInfo.hInstApp     = NULL;
    ShExecInfo.fMask        = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_HMONITOR;

    m_reportStr = Utility::StrPrintf(
            "Launching Process Name = %s\n", m_appName3DMarkV.c_str());
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    if (!ShellExelwteEx(&ShExecInfo))
    {
        m_reportStr = Utility::StrPrintf(
            "Launching Process Name = %s Failed.\n", m_appName3DMarkV.c_str());
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        return RC::SOFTWARE_ERROR;
    }
    WaitForInputIdle(ShExecInfo.hProcess, INFINITE);

    m_reportStr = Utility::StrPrintf(
            "Process Handle = %u", ShExecInfo.hProcess);
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

    //
    // This sleep [approx value 30sec] is required for 3DMarkVantage to initialize properly and start.
    // 3DMarkVantage need to scan system info before launching benchmark thread so that it takes much longer.
    //
    Sleep(THREEDMARK_VANTAGE_LAUNCH_TIME_SEC * 1000, m_runOnEmu, GetBoundGpuSubdevice());

    // Check Underflow
    if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR)
    {
        m_reportStr = Utility::StrPrintf(
                "ERROR: %s: Display underflow was detected on head %u after launching Process Name = %s\n",
                __FUNCTION__, underflowHead, m_appName3DMarkV.c_str());
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        // kill the Process
        goto exit;
    }

    UINT64 startTimeMS = Platform::GetTimeMS();
    UINT64 elapseTimeSec = 0;
    do{
        // query and test every sec.
        Sleep(1000, m_runOnEmu, GetBoundGpuSubdevice());

        rc = objPgStats.getPGPercent(LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS, m_runOnEmu, &actualEfficiency,
                                     &gatingCount, &avgGatingUs, &avgEntryUs, &avgExitUs);
        if (rc != OK)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Query MSCG stats data failed, rc = %d\n",
                    __FUNCTION__, (UINT32)rc);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            goto exit;
        }

        Printf(Tee::PriDebug, "Polling MSCG efficiency = %2d%%\n", actualEfficiency);

        if (actualEfficiency != 0)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: MSCG actual efficiency is not equal to zero on immediate flip mode\n",
                    __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            goto exit;
        }

        if ((rc = DTIUtils::VerifUtils::checkDispUnderflow(m_pDisplay, m_numHeads, &underflowHead)) == RC::LWRM_ERROR)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Display underflow was detected on head %u after launching Process Name = %s\n",
                    __FUNCTION__, underflowHead, m_appName3DMarkV.c_str());
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

            goto exit;
        }

        elapseTimeSec = (Platform::GetTimeMS() - startTimeMS) / 1000;
    } while(elapseTimeSec < m_immedFlipTestCheckMSCGSecs);

    if (m_PStateSupport)
    {
        if (m_bMclkSwitchPossible)
        {
            // Random pstate switch from user provide min Pstate value.
            rc = RandomPStateTransitions(porMinPState, m_pstateSwitchCount);
            if (rc != OK)
            {
                goto exit;
            }
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "WARNING: %s: MCLK Switch is NOT possible. So skipping Random PState Switch.",
                    __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
        }
    }

exit:
    // kill the Process with exitcode 0
    if (ShExecInfo.hProcess)
    {
        m_reportStr = Utility::StrPrintf(
                "Process Name = %s, Process Handle = %u, Killing\n",
                m_appName3DMarkV.c_str(), ShExecInfo.hProcess);
        PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);

        TerminateProcess (ShExecInfo.hProcess, 0);
        CloseHandle(ShExecInfo.hProcess);
    }

    return rc;
#else
    Printf(Tee::PriHigh, "Application launching is not supported on non winmods platform");
    return OK;
#endif
}

//! \brief run applications specified in appsToLaunchStressOnPrimary sequentially
//         along with Random PState Test.
//------------------------------------------------------------------------------
RC Modeset::runStressTestOnPrimaryDisplay
(
    DisplayID primaryDisplay,
    UINT32    porMinPState
)
{
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

    RC rc;
    Tasker::ThreadID appThread;
    LaunchApplicationParams launchAppParams;

    appLaunchCreatedUnderflow = false;
    appLaunchError = false;
    appNotRun  = false;

    // loop applications for stress test on primary display
    for (UINT32 appNo = 0; appNo < g_IMPMarketingParameters.testLevelConfig.appsToLaunchStressOnPrimary.size(); appNo++)
    {
        rc = setupAppParams(primaryDisplay, &launchAppParams,
                            g_application[g_IMPMarketingParameters.testLevelConfig.appsToLaunchStressOnPrimary[appNo]]);
        if (rc != OK)
        {
            return rc;
        }

        sprintf(launchAppParams.threadName, "Launch Application %u", appNo);

        appThread = Tasker::CreateThread
                    (
                        LaunchApplication,
                        &launchAppParams,
                        Tasker::MIN_STACK_SIZE,
                        launchAppParams.threadName
                    );

        //
        // Calling sleep here will ilwoke other ready threads we created above for applications.
        // And waiting one more second longer than application launch latency to ensure post launch operations
        // such as setting the applications intended window size and location have finished before entering
        // Random PState Test.
        //
        Sleep((launchAppParams.launchTimeSec + 1) * 1000, m_runOnEmu, GetBoundGpuSubdevice());

        if (appLaunchError)
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Application launch failed.\n", __FUNCTION__);

            outputCommentStr("FAIL,");
            return RC::SOFTWARE_ERROR;
        }

        if (appNotRun)
        {
            outputCommentStr("FAIL,");
            return RC::SOFTWARE_ERROR;
        }

        //
        // Run Random PState Test for half of max application running time. And then stay min PState
        // until all application threads finished so that applications run on min PState for at least half of
        // max running time since min PState is more stressful.
        //
        // And don't poll underflow state inside RandomPStateTransitions due to 2 reasons below -
        // 1. It will keep polling underflow state in running app thread and app running time should be longer than Random PState Test,
        //    so don't poll it inside RandomPStateTransitions and rely on app thread for checking underflow.
        // 2. I found it may cause system hangs in very rare case if we poll display underflow state in different threads.
        //
        bool bRunMinPStateTest = false;
        bool bRunIsoFbLatencyTest = false;
        UINT32 randomPStateTestDuration = (m_PStateSupport) ? (launchAppParams.app.seconds / 2) : 0;
        UINT32 minPStateTestDuration = launchAppParams.app.seconds - randomPStateTestDuration;
        if (m_PStateSupport)
        {
            rc = RandomPStateTransitions(porMinPState, m_pstateSwitchCount, randomPStateTestDuration, false);

            if (rc == OK && !appLaunchCreatedUnderflow)
            {
                bRunMinPStateTest = true;
                rc = ProgramAndVerifyPState(porMinPState);
            }
        }

        //
        // Enable ISO FB Latency Measurement
        // Lwrrently, we perform Iso Fb Latency Measurement with only min p-state.
        // If m_PStateSupport is not true, the system is running at fixed clocks.
        // And that would represent the min pstate.
        //
        if (m_isoFbLatencyTest > 0 && minPStateTestDuration >= m_isoFbLatencyTest)
        {
            if (StartIsoFbLatencyTest() == OK)
            {
                bRunIsoFbLatencyTest = true;
            }
        }

        // Wait until the application thread terminates.
        Tasker::Join(appThread);

        // Disable and get Iso Fb Latency Measurement result
        if (bRunIsoFbLatencyTest)
        {
            rc = EndIsoFbLatencyTest();
            if (OK != rc)
            {
                m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: Iso FB Latency Test failed.\n", __FUNCTION__);

                outputCommentStr("FAIL,");
                return rc;
            }
        }

        if (rc != OK || appLaunchCreatedUnderflow)
        {
            string errorStr = "ERROR: Display underflow was detected";
            if (m_PStateSupport)
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: %s PState Test failed",
                        __FUNCTION__, (bRunMinPStateTest ? "Min" : "Random"));

                errorStr = m_reportStr;
                if (rc != OK)
                {
                    m_reportStr = Utility::StrPrintf(
                            " with RC = %d", (UINT32)rc);
                    errorStr += m_reportStr;
                }
                if (appLaunchCreatedUnderflow)
                {
                    errorStr += " and Display underflow was detected";
                }
            }

            m_reportStr = Utility::StrPrintf(
                    "%s while %s running\n", errorStr.c_str(),
                    g_application[g_IMPMarketingParameters.testLevelConfig.appsToLaunchStressOnPrimary[appNo]].name.c_str());

            outputCommentStr("FAIL,");
            return RC::SOFTWARE_ERROR;
        }

    }

    return OK;
#else
    Printf(Tee::PriHigh, "runStressTestOnPrimaryDisplay is not supported on non winmods platform");
    return OK;
#endif
}

//! \brief helper function to setup application parameters
//------------------------------------------------------------------------------
RC Modeset::setupAppParams
(
    DisplayID displayId,
    LaunchApplicationParams *launchAppParams,
    const Application &application
)
{
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    RC rc;
    LwS32   startXPos       = 0;
    LwS32   startYPos       = 0;
    LwU32   width           = 0;
    LwU32   height          = 0;

    rc =  GetDisplayStartPosAndSize(displayId,
                &startXPos, &startYPos,
                &width, &height);

    if (rc != OK)
    {
        if (rc == RC::NO_DISPLAY_DEVICE_CONNECTED)
        {
            rc = OK;
        }
        else
        {
            m_reportStr = Utility::StrPrintf(
                    "ERROR: %s: GetDisplayStartPosAndSize() Failed with RC = %d.\n"
                    "Skipping Overlay application launch on display id =0x%X\n",
                    __FUNCTION__, (UINT32)rc, displayId);

            outputCommentStr("FAIL,");
            return rc;
        }
    }

    launchAppParams->pDisplay            = m_pDisplay;
    launchAppParams->numHeads            = m_numHeads;
    launchAppParams->app                 = application;
    launchAppParams->startXPosOfDisplay  = startXPos;
    launchAppParams->startYPosOfDisplay  = startYPos;
    launchAppParams->appWidth            = width;
    launchAppParams->appHeight           = height;
    launchAppParams->runOnEmu            = m_runOnEmu;
    launchAppParams->gpuSubdev           = GetBoundGpuSubdevice();

    // special handle for 3DMarkVantage
    if (launchAppParams->app.command == m_appName3DMarkV)
    {
        char appParams[300];
        char lwrrDir[BUFFER_SIZE_LWRRENT_DIR] = {0};

        // check if 3DMarkVantage has been installed on test system
        if (m_appInstallLoc3DMarkV == "")
        {
            if (!check3DMarkVantageInstallation())
            {
                m_reportStr = Utility::StrPrintf(
                        "ERROR: %s: 3DMarkVantage has not been installed yet.\n", __FUNCTION__);
                PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
                goto Failed;
            }
        }

        CHECK_RC(create3DMarkVantageConfig("3DMarkVConfig.txt", width, height));
        if (!GetLwrrentDirectory(BUFFER_SIZE_LWRRENT_DIR, lwrrDir))
        {
            m_reportStr = Utility::StrPrintf(
                    "%s: GetLwrrentDirectory() failed.\n", __FUNCTION__);
            PRINT_STDOUT_N_REPORT(Tee::PriError, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE);
            goto Failed;
        }

        sprintf(appParams, "--config_file=\"%s\\%s\"", lwrrDir, "3DMarkVConfig.txt");

        // override the setting
        launchAppParams->app.parameters = appParams;
        launchAppParams->app.directory = m_appInstallLoc3DMarkV;
        launchAppParams->launchTimeSec = THREEDMARK_VANTAGE_LAUNCH_TIME_SEC;
    }

    return OK;

Failed:
    m_reportStr = Utility::StrPrintf(
            "%s: Fail to setup application parameters of %s.\n",
            __FUNCTION__,
            launchAppParams->app.name.c_str());

    outputCommentStr("FAIL,");

    return RC::SOFTWARE_ERROR;
#else
    Printf(Tee::PriHigh, "setupAppParams is not supported on non winmods platform");
    return RC::LWRM_ILWALID_FUNCTION;
#endif
}

//! \brief get CRC values for a specified display
//------------------------------------------------------------------------------
RC Modeset::getCRCValues
(
    DisplayID               Display,
    UINT32                 &CompCRC,
    UINT32                 &PriCRC,
    UINT32                 &SecCRC
)
{
#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)
    //
    // !!CAUTION!!
    // CRC mechanism is not ready for winmods
    //
    CompCRC = PriCRC = SecCRC = 0;
    return OK;
#else

    RC              rc;

    EvoCRCSettings CrcStgs;
    CrcStgs.ControlChannel = EvoCRCSettings::CORE;
    CrcStgs.CrcPrimaryOutput = EvoCRCSettings::CRC_RG; // Test doesn't care about the OR type

    CHECK_RC(m_pDisplay->SetupCrcBuffer(Display, &CrcStgs));
    CHECK_RC(m_pDisplay->GetCrcValues(Display, &CrcStgs, &CompCRC, &PriCRC, &SecCRC));

    return rc;
#endif
}

//! \brief hepler function to output to log & csv file.
//
//! \param  leadingStr[in ] : String to fill the CSV fields before comment field,
//                            e.g. "Not Attempted,Not Attempted,"
//
// Note that it's caller's resposibility to put comment string in m_reportStr before
// calling it.
//
//------------------------------------------------------------------------------
void Modeset::outputCommentStr(string leadingStr)
{
    string commentStr = leadingStr;
    string reportStrStripped = m_reportStr;
    reportStrStripped.erase(remove(reportStrStripped.begin(),
                                   reportStrStripped.end(), '\n'),
                            reportStrStripped.end());
    commentStr += reportStrStripped;

    PRINT_STDOUT_N_REPORT(Tee::PriNormal, m_reportStr.c_str(), PRINT_STDOUT | PRINT_LOG_FILE );

    // Print commentStr to CSV report
    PRINT_STDOUT_N_REPORT(Tee::PriNormal, commentStr.c_str(), PRINT_CSV_FILE );
}

//! \brief Helpler function to put the current thread to sleep for a given number(base on ptimer) of milliseconds,
//!         giving up its CPU time to other threads.
//!         Warning, this function is not for precise delays.
//! \param miliseconds [in ]: Milliseconds to sleep
//! \param runOnEmu [in]: It is on emulation or not
//! \param gpuSubdev [in]: Pointer to gpu subdevice
//!
void Modeset::Sleep(LwU32 miliseconds, LwBool runOnEmu, GpuSubdevice *gpuSubdev)
{
    if(runOnEmu)
        GpuUtility::SleepOnPTimer(miliseconds, gpuSubdev);
    else
        Tasker::Sleep(miliseconds);
}

//! \brief Helpler function to check whether mode is supported on given displayID.
//!         giving up its CPU time to other threads.
//!         Warning, this function is not for precise delays.
//! \param mode [in ]:                    mode to checked
//! \param dispId [in]:                   dispId to checked on
//! \param sortedWorkingSet [in]:         supported modes for all displayIDs
//! \param pModeSupportedOnMonitor [out]: Output value for whether given mode is supported on given displayID
//!
RC Modeset::IsModeSupported(Display::Mode        mode,
                              DisplayID          dispId,
                              vector<DispParams> &sortedWorkingSet,
                              bool               *pModeSupportedOnMonitor)
{
    RC rc = OK;
    UINT32 dispIndex;
    vector<Display::Mode>  ListedRes;

    for(dispIndex = 0; dispIndex < (UINT32)sortedWorkingSet.size(); dispIndex++)
    {
        if(sortedWorkingSet[dispIndex].dispId == dispId)
            break;
    }
    if(dispIndex == sortedWorkingSet.size())
    {
        Printf(Tee::PriHigh, "ERROR: %s: Failed to GetListedResolutions() for DispId = %X with RC=%d \n",
                            __FUNCTION__,(UINT32)dispId,(UINT32)rc);
        rc = RC::SOFTWARE_ERROR;
        return rc;
    }

    *pModeSupportedOnMonitor = false;
    for(UINT32 i = 0; i < (UINT32)sortedWorkingSet[dispIndex].modes.size(); i++)
    {
        if((UINT32)sortedWorkingSet[dispIndex].modes[i].width       == mode.width &&
           (UINT32)sortedWorkingSet[dispIndex].modes[i].height      == mode.height &&
           (UINT32)sortedWorkingSet[dispIndex].modes[i].refreshRate == mode.refreshRate
          )
        {
            *pModeSupportedOnMonitor = true;
            break;
        }
    }

    if (!(*pModeSupportedOnMonitor) && ((mode.width > 4095) || (mode.height > 4095)))
    {
        Printf(Tee::PriHigh,
               "NOTE: %s: On some SKUs, support for 4K resolution is enabled only on the debug version"
               " of the driver and mods (bugs 997511 & 1005532).\n",
               __FUNCTION__);
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
JS_CLASS_INHERIT(Modeset, RmTest, "simple cycle resolution test\n");

CLASS_PROP_READWRITE(Modeset, onlyConnectedDisplays, bool,
                     "run on only connected displays, default = 0.");
CLASS_PROP_READWRITE(Modeset, manualVerif, bool,
                     "do manual verification, default = 0.");
CLASS_PROP_READWRITE(Modeset , enableASRTest, bool,
                     "Run ASR test in each mode-switch, default = false.");
CLASS_PROP_READWRITE(Modeset , enableMSCGTest, bool,
                     "Run MSCG test in each mode-switch, default = false.");
CLASS_PROP_READWRITE(Modeset , enableImmedFlipTest, bool,
                     "Run Immediate Flip test in each mode-switch, default = false.");
CLASS_PROP_READWRITE(Modeset , disableRandomPStateTest, bool,
                     "Disable running Random PState test after each mode-switch, default = false.");
CLASS_PROP_READWRITE(Modeset , mempoolCompressionTristate, UINT08,
                     "Tristate for config mempool compression enable state, default = TRISTATE_DEFAULT.");
CLASS_PROP_READWRITE(Modeset, isoFbLatencyTest, UINT32,
                                "When == 0, the Iso Fb latency test is disabled. When > 0, the Iso Fb latency test is enabled. And it indicates the min acceptable record time (in seconds).");
CLASS_PROP_READWRITE(Modeset , minMempoolTristate, UINT08,
                     "Tristate for config minmum mempool enable state, default = TRISTATE_DEFAULT.");
CLASS_PROP_READWRITE(Modeset , immedFlipTestCheckMSCGSecs, UINT32,
                     "Duration for checking MSCG efficiency when running Immediate Flip test, default = 60 secs.");
CLASS_PROP_READWRITE(Modeset, protocol, string,
                     "OR protocol to use for the test.");
CLASS_PROP_READWRITE(Modeset, repro, string,
                     "Repro only specific thing. Used in rigorousModesetTest.");
CLASS_PROP_READWRITE(Modeset, runfrominputfile, bool,
    "when true: runs for all resolutions from input file..When false: runs for all combinations of displays for EDID resoutions.");
CLASS_PROP_READWRITE(Modeset, inputconfigfile, string,
                     "IMP input JS file to be used for the test.");
CLASS_PROP_READWRITE(Modeset, runOnlyConfigs, string,
                     "if used, it specifies the list of configNos that should be run. Remaining configs will be skipped. Note: Config Nos starts with 0 and not 1.");
CLASS_PROP_READWRITE(Modeset, pixelclock, UINT32,
                     "Desired pixel clock in Hz.");
CLASS_PROP_READWRITE(Modeset, pstateSwitchCount, UINT32,
                     "no of pstate switches.");
CLASS_PROP_READWRITE(Modeset, randomPStateSwitchIntervalMS, UINT32,
                     "Time interval (ms) between successive PState transitions.");
CLASS_PROP_READWRITE(Modeset, useCRTFakeEdid, string,
                     "Edid file to use for faking CRT.");
CLASS_PROP_READWRITE(Modeset, useDFPFakeEdid, string,
                     "Edid file to use for faking DFP.");
CLASS_PROP_READWRITE(Modeset, autoGenerateEdid, bool,
                     "Bool: When true: auto generate EDID file for POR mode verif. For now valid only on Windows OS.");
CLASS_PROP_READWRITE(Modeset, freqTolerance100x, UINT32,
                     "Clock freq difference allowed tolerance.");
CLASS_PROP_READWRITE(Modeset, ignoreClockDiffError, bool,
                     "Bool: When true runs imptest3 and just logs clk errors instead of failing it there.");
CLASS_PROP_READWRITE(Modeset, useWindowsResList, bool,
    "Bool: When true use windows api to prune resolutions. This returns more resolutions then what EDID returns.Only valid on Windows OS.");
CLASS_PROP_READWRITE(Modeset, forceLargeLwrsor, bool,
    "when true: forces large cursor. i.e. 64x64 till GF119. 256x256 @ Kepler_And_Above.");
CLASS_PROP_READWRITE(Modeset, forcePStateBeforeModeset, bool,
    "when true: forces POR Pstate prior to the modeset. This is needed for ARCH verification. Dont used this on windows as it can create underflows due to large config pending from previous modeset.");
CLASS_PROP_READWRITE(Modeset, useAppRunCheck, bool,
    "when true: makes use of GpuClk and MClk util to check whether an app is running or not.");

//
// Constant Properties
//
#define MODESET_CONST_PROPERTY(name,init,help)  \
    static SProperty Modeset_##name             \
    (                                           \
        Modeset_Object,                         \
        #name,                                  \
        0,                                      \
        init,                                   \
        0,                                      \
        0,                                      \
        JSPROP_READONLY,                        \
        "CONSTANT - " help                      \
    )

MODESET_CONST_PROPERTY(PSTATE_MINPSTATE,    MinPState,
                       "Constant indicates min PState value that IMP returns for a mode.");
MODESET_CONST_PROPERTY(PSTATE_UNDEFINED, Undefined,
                       "Constant indicates undefined PState.");
MODESET_CONST_PROPERTY(PSTATE_P0, P0,
                       "Constant indicates P0 PState.");
MODESET_CONST_PROPERTY(PSTATE_P1, P1,
                       "Constant indicates P1 PState.");
MODESET_CONST_PROPERTY(PSTATE_P2, P2,
                       "Constant indicates P2 PState.");
MODESET_CONST_PROPERTY(PSTATE_P3, P3,
                       "Constant indicates P3 PState.");
MODESET_CONST_PROPERTY(PSTATE_P4, P4,
                       "Constant indicates P4 PState.");
MODESET_CONST_PROPERTY(PSTATE_P5, P5,
                       "Constant indicates P5 PState.");
MODESET_CONST_PROPERTY(PSTATE_P6, P6,
                       "Constant indicates P6 PState.");
MODESET_CONST_PROPERTY(PSTATE_P7, P7,
                       "Constant indicates P7 PState.");
MODESET_CONST_PROPERTY(PSTATE_P8, P8,
                       "Constant indicates P8 PState.");
MODESET_CONST_PROPERTY(PSTATE_P9, P9,
                       "Constant indicates P9 PState.");
MODESET_CONST_PROPERTY(PSTATE_P10, P10,
                       "Constant indicates P10 PState.");
MODESET_CONST_PROPERTY(PSTATE_P11, P11,
                       "Constant indicates P11 PState.");
MODESET_CONST_PROPERTY(PSTATE_P12, P12,
                       "Constant indicates P12 PState.");
MODESET_CONST_PROPERTY(PSTATE_P13, P13,
                       "Constant indicates P13 PState.");
MODESET_CONST_PROPERTY(PSTATE_P14, P14,
                       "Constant indicates P14 PState.");
MODESET_CONST_PROPERTY(PSTATE_P15, P15,
                       "Constant indicates P15 PState.");
MODESET_CONST_PROPERTY(PSTATE_MAXPSTATE, MaxPState,
                       "Constant indicates numerically highest PState (having lowest clocks).");

JS_STEST_LWSTOM(Modeset,
                addApplications,
                1,
                "Reads the applications to be used to verify the marketing sheet as input from javascript file and passes it to the test.")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    JsArray  JsArrApplications;

    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &JsArrApplications))
       )
    {
        Printf(Tee::PriHigh,
                       "Usage: Modeset.addApplications(g_applicationsById)");
        return JS_FALSE;
    }

    JSObject * pLwrrApplicationObj;
    size_t applicationsCount = JsArrApplications.size();
    UINT32 i;

    for (i = 0; i < applicationsCount; i++)
    {
        if(OK != pJs->FromJsval(JsArrApplications[i], &pLwrrApplicationObj))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve application[%d]",__FUNCTION__,i);
            continue;
        }

        Application     retrievedAppObj;
        if(OK != addAppObjHelper(pLwrrApplicationObj,
                                 retrievedAppObj))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve application[%d]",__FUNCTION__,i);
            continue;
        }
    }

    Printf(Tee::PriHigh, "\n ------------------ App's List: Master Table from Config File -------------------");
    i=0;
    map<string,Application>::iterator it;
    for (it = g_application.begin(); it != g_application.end(); it++)
    {
        Printf(Tee::PriHigh,
               "\n Key = %-30s ==>> AppID = %03d, name = %-20s, Commamd = %-32s, Parameters = %-64s, Directory = %-32s,"
               "Seconds = %5d, xPos = %5d, yPos = %5d, flags = %5d, mClkThreshold = %5d, gpuClkThreshold = %5d",
               it->first.c_str(),
               i++,
               it->second.name.c_str(),
               it->second.command.c_str(),
               it->second.parameters.c_str(),
               it->second.directory.c_str(),
               it->second.seconds,
               it->second.xPos,
               it->second.yPos,
               it->second.flags,
               it->second.mClkUtilThreshold,
               it->second.gpuClkUtilThreshold
               );
    }
    RETURN_RC(OK);
}

JS_STEST_LWSTOM(Modeset,
                addModeSettings,
                1,
                "Reads the ModeSettings to be used to verify the marketing sheet as input from javascript file and passes it to the test.")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJs;
    JsArray  JsArrModeSettings;

    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &JsArrModeSettings))
       )
    {
        Printf(Tee::PriHigh,
                       "Usage: Modeset.addModeSettings(g_modeSettingsById)");
        return JS_FALSE;
    }

    JSObject * pLwrrModeSettingsObj;
    size_t modeSettingsCount = JsArrModeSettings.size();
    UINT32 i;

    for (i = 0; i < modeSettingsCount; i++)
    {
        if(OK != pJs->FromJsval(JsArrModeSettings[i], &pLwrrModeSettingsObj))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve ModeSettings[%d]",__FUNCTION__,i);
            continue;
        }

        ModeSettings        retrievedModeSettings;
        if(OK != addModeStgsObjHelper(pLwrrModeSettingsObj,
                                      retrievedModeSettings))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve ModeSettings[%d]",__FUNCTION__,i);
            continue;
        }
    }

    Printf(Tee::PriHigh, "\n ------------------ Modes List: Master table from Config File -------------------");
    i=0;
    map<string,ModeSettings>::iterator it;
    for ( it=g_modeSettings.begin(); it != g_modeSettings.end(); it++ )
    {
        Printf(Tee::PriHigh, "\n Key = %-32s ==>> ModeID =%5d , name=%-32s, width=%5d, height=%5d, depth=%3d, RefreshRate= %6d,"
                               "colorFormat=%-15s, interlaced=%3d, timing=%3d, scaling=%3d, overlayDepth =%3d",
                               it->first.c_str(),
                               i++,
                               it->second.name.c_str(),
                               it->second.mode.width,
                               it->second.mode.height,
                               it->second.mode.depth,
                               it->second.mode.refreshRate,
                               it->second.colorFormat.c_str(),
                               it->second.interlaced,
                               it->second.timing,
                               it->second.scaling,
                               it->second.overlayDepth
                               );
    }
    Printf(Tee::PriHigh, "\n");
    RETURN_RC(OK);
}

// Retrieves the application object from the pLwrrApplicationObj and
// if it doesn't exist in g_application then it adds it there
// and returns a reference for use to caller function..
RC addAppObjHelper(JSObject        *pLwrrApplicationObj,
                   Application     &retrievedAppObj)
{
    JavaScriptPtr pJs;
    UINT32 appFlags;

    if (OK != pJs->GetProperty(pLwrrApplicationObj, "name", &retrievedAppObj.name))
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].name input..\n"
               "Are you missing name Parameter? Defaulting name = \"\"\n", __FUNCTION__);
        retrievedAppObj.name = "";
    }

    if (!g_application.count(retrievedAppObj.name))
    {
        //new object added using constructor so add it to the g_application map
        if (OK != pJs->GetProperty(pLwrrApplicationObj, "command", &retrievedAppObj.command))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].command input..\n"
                   "Are you missing command Parameter? Defaulting command = \"\"\n", __FUNCTION__);
            retrievedAppObj.command = "";
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "parameters", &retrievedAppObj.parameters))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].parameters input..\n"
                   "Are you missing parameters Parameter? Defaulting parameters = \"\"\n", __FUNCTION__);
            retrievedAppObj.parameters = "";
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "directory", &retrievedAppObj.directory))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].directory input..\n"
                   "Are you missing directory Parameter? Defaulting directory = \"\"\n", __FUNCTION__);
            retrievedAppObj.directory = "";
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "seconds", &retrievedAppObj.seconds))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].seconds input..\n"
                   "Are you missing seconds Parameter? Defaulting seconds = 0\n", __FUNCTION__);
            retrievedAppObj.seconds = 0;
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "xPos", &retrievedAppObj.xPos))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].xPos input..\n"
                   "Are you missing xPos Parameter? Defaulting xPos = 0\n", __FUNCTION__);
            retrievedAppObj.xPos = 0;
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "yPos", &retrievedAppObj.yPos))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].yPos input..\n"
                   "Are you missing yPos Parameter? Defaulting yPos = 0\n", __FUNCTION__);
            retrievedAppObj.yPos = 0;
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "flags", &appFlags))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].flags input..\n"
                   "Are you missing flags Parameter? Defaulting flags = AppFlag.None\n", __FUNCTION__);
            retrievedAppObj.flags = 0;
        }
        else
        {
            retrievedAppObj.flags = appFlags;
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "mClkUtilThreshold", &retrievedAppObj.mClkUtilThreshold))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].mClkUtilThreshold input.\n"
                   "Are you missing mClkUtilThreshold Parameter? Defaulting mClkUtilThreshold = 0\n", __FUNCTION__);
            retrievedAppObj.mClkUtilThreshold = 0;
        }

        if (OK != pJs->GetProperty(pLwrrApplicationObj, "gpuClkUtilThreshold", &retrievedAppObj.gpuClkUtilThreshold))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate Application[].gpuClkUtilThreshold input.\n"
                   "Are you missing gpuClkUtilThreshold Parameter? Defaulting gpuClkUtilThreshold = 0\n", __FUNCTION__);
            retrievedAppObj.gpuClkUtilThreshold = 0;
        }

        g_application.insert(pair<string,Application>(retrievedAppObj.name, retrievedAppObj));
    }
    return OK;
}

// Retrieves the application object from the pLwrrApplicationObj and
// if it doesn't exist in g_application then it adds it there
// and returns a reference for use to caller function..
RC addModeStgsObjHelper(JSObject            *pLwrrModeSettingsObj,
                        ModeSettings        &retrievedModeSettings)
{
    JavaScriptPtr pJs;

    if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "name", &retrievedModeSettings.name))
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].name input..\n"
            "Are you missing name Parameter? Defaulting name = \"\"\n", __FUNCTION__);
        retrievedModeSettings.name = "";
    }

    if (!g_modeSettings.count(retrievedModeSettings.name))
    {
        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "width", &retrievedModeSettings.mode.width))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].width input..\n"
                "Are you missing width Parameter? Defaulting width = 0\n", __FUNCTION__);
            retrievedModeSettings.mode.width = 0;
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "height", &retrievedModeSettings.mode.height))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].height input..\n"
                "Are you missing height Parameter? Defaulting height = 0\n", __FUNCTION__);
            retrievedModeSettings.mode.height = 0;
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "depth", &retrievedModeSettings.mode.depth))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].depth input..\n"
                "Are you missing depth Parameter? Defaulting depth = 32\n", __FUNCTION__);
            retrievedModeSettings.mode.depth = 32;
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "refreshRate", &retrievedModeSettings.mode.refreshRate))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].refreshRate input..\n"
                "Are you missing refreshRate Parameter? Defaulting refreshRate = 60\n", __FUNCTION__);
            retrievedModeSettings.mode.refreshRate = 60;
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "colorFormat", &retrievedModeSettings.colorFormat))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].colorFormat input..\n"
                "Are you missing depth Parameter? Defaulting colorFormat = ""\n", __FUNCTION__);
            retrievedModeSettings.colorFormat = "";
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "interlaced", &retrievedModeSettings.interlaced))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].interlaced input..\n"
                "Are you missing refreshRate Parameter? Defaulting interlaced = false\n", __FUNCTION__);
            retrievedModeSettings.interlaced = false;
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "timing", &retrievedModeSettings.timing))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].timing input..\n"
                "Are you missing seconds Parameter? Defaulting timing = \"Display.GTF\"\n", __FUNCTION__);
            retrievedModeSettings.timing = Display::GTF;
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "scaling", &retrievedModeSettings.scaling))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].scaling input..\n"
                "Are you missing xPos Parameter? Defaulting scaling = false\n", __FUNCTION__);
            retrievedModeSettings.scaling = false;
        }

        if (OK != pJs->GetProperty(pLwrrModeSettingsObj, "overlayDepth", &retrievedModeSettings.overlayDepth))
        {
            Printf(Tee::PriHigh, "Error: %s: Couldn't translate ModeSettings[].overlayDepth input..\n"
                "Are you missing yPos Parameter? Defaulting overlayDepth = 0\n", __FUNCTION__);
            retrievedModeSettings.overlayDepth = 0;
        }
        g_modeSettings.insert(pair<string,ModeSettings>(retrievedModeSettings.name,retrievedModeSettings));
    }
    return OK;
}
// Retrieves the ConfigList from the pLwrrConfigListObj.
// and adds it to the g_IMPMarketingParameters.configList
RC addConfigList(JSObject       *pLwrrConfigListObj,
                 ConfigList     &retrievedConfigList)
{
    RC                  rc;
    JavaScriptPtr       pJs;
    JsArray             JsArrActiveDisplaysCount;
    UINT32              viewMode;
    vector<string>      modesList;

    if (OK != pJs->GetProperty(pLwrrConfigListObj, "viewMode", &viewMode))
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ConfigList[].viewMode input..\n"
              "Are you missing viewMode Parameter? Defaulting viewMode = \"STANDARD\"\n", __FUNCTION__);
        retrievedConfigList.viewMode = Display::STANDARD;
    }
    else
    {
        retrievedConfigList.viewMode = (Display::VIEW_MODE)viewMode;
    }

    UINT32      porPState = 0;

    if (OK != pJs->GetProperty(pLwrrConfigListObj, "porPState", &porPState))
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ConfigList[].porPState input..\n"
              "Are you missing porPstate Parameter? Defaulting porPstate = P0\n", __FUNCTION__);
        retrievedConfigList.porPState = P0;
    }
    else
    {
        retrievedConfigList.porPState = porPState;
    }

    UINT32 porVPState = 0;

    if (OK != pJs->GetProperty(pLwrrConfigListObj, "porVPState", &porVPState))
    {
        //
        // Defaulting it to IMPTEST3_VPSTATE_UNDEFINED to handle the case of configs before
        // PS30 on non pstate systems. (m_PStateSupport = FALSE and porPState set to
        // PState.Undefined but the vpstate entry is missing)
        //
        Printf(Tee::PriHigh,
            "Error: %s: Couldn't translate ConfigList[].porVPState input. "
            "Are you missing porVPstate Parameter? Defaulting porVPstate as Undefined (257)\n",
            __FUNCTION__);
        retrievedConfigList.porVPState = IMPTEST3_VPSTATE_UNDEFINED;
    }
    else
    {
        retrievedConfigList.porVPState = porVPState;
    }

    if (OK == pJs->GetProperty(pLwrrConfigListObj, "activeDisplaysCount", &JsArrActiveDisplaysCount))
    {
        size_t NumActiveDisp = JsArrActiveDisplaysCount.size();
        UINT32 ActiveDispCount;
        for(UINT32 j=0; j < NumActiveDisp; j++)
        {
            if(OK != pJs->FromJsval(JsArrActiveDisplaysCount[j], &ActiveDispCount))
            {
                Printf(Tee::PriHigh,
                            "Error: %s can't retrieve activeDisplaysCount[%d]",__FUNCTION__,j);
                continue;
            }
            retrievedConfigList.activeDisplaysCount.push_back(ActiveDispCount);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ConfigList[].activeDisplaysCount input..\n"
              "Are you missing activeDisplaysCount Parameter? Defaulting activeDisplaysCount = empty\n", __FUNCTION__);
    }

    if (OK != pJs->GetProperty(pLwrrConfigListObj, "overlayCount", &retrievedConfigList.overlayCount))
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ConfigList[].overlayCount input..\n"
              "Are you missing overlayCount Parameter? Defaulting overlayCount = 0\n", __FUNCTION__);
        retrievedConfigList.overlayCount = 0;
    }

    JsArray JsArrValidForChip;
    if (OK == pJs->GetProperty(pLwrrConfigListObj, "validForChip", &JsArrValidForChip))
    {
        size_t NumValidForChip = JsArrValidForChip.size();
        UINT32 ChipID;
        for(UINT32 j=0; j < NumValidForChip; j++)
        {
            if(OK != pJs->FromJsval(JsArrValidForChip[j], &ChipID))
            {
                Printf(Tee::PriHigh,
                            "Error: %s can't retrieve ValidForChip[]",__FUNCTION__);
                continue;
            }
            retrievedConfigList.validForChip.push_back(ChipID);
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ConfigList[].validForChip input..\n"
              "Are you missing validForChip Parameter? Defaulting validForChip = empty\n", __FUNCTION__);
    }

    if (OK != pJs->GetProperty(pLwrrConfigListObj, "useInlineApp", &retrievedConfigList.useInlineApp))
    {
        Printf(Tee::PriHigh, "Error: %s: Couldn't translate ConfigList[].useInlineApp input..\n"
              "Are you missing useInlineApp Parameter? Defaulting useInlineApp = false\n", __FUNCTION__);
        retrievedConfigList.useInlineApp = false;
    }

    // Retrieve App List
    JsArray JsArrAppList;
    if (OK != pJs->GetProperty(pLwrrConfigListObj, "appList",  &JsArrAppList))
    {
        Printf(Tee::PriHigh, "%s: appsToLaunchNoOverlay Array not provided in input file.."
            "Defaulting appsToLaunchNoOverlay to empty vector\n",__FUNCTION__);
    }

    vector<string> appsListPerMode;
    JSObject      *pLwrrApplicationObj;

    size_t NumAppsListForThisConfig = JsArrAppList.size();
    retrievedConfigList.appList.clear();

    for (UINT32 ModeNo = 0; ModeNo < NumAppsListForThisConfig; ModeNo++)
    {
        JsArray JsArrAppListForThisMode;
        if (OK != pJs->FromJsval(JsArrAppList[ModeNo], &JsArrAppListForThisMode))
        {
            Printf(Tee::PriHigh,
                "Error: %s can't retrieve appsToLaunchNoOverlay.application[%d]",__FUNCTION__,ModeNo);
            continue;
        }

        size_t NumAppForThisMode = JsArrAppListForThisMode.size();
        appsListPerMode.clear();
        for (UINT32 appForThisMode = 0; appForThisMode < NumAppForThisMode; appForThisMode++)
        {
            if (OK != pJs->FromJsval(JsArrAppListForThisMode[appForThisMode], &pLwrrApplicationObj))
            {
                Printf(Tee::PriHigh,
                    "Error: %s can't retrieve JsArrAppListForThisMode.application[%d]",__FUNCTION__,appForThisMode);
                continue;
            }

            Application     retrievedAppObj;
            if (OK != addAppObjHelper(pLwrrApplicationObj,
                retrievedAppObj))
            {
                Printf(Tee::PriHigh,
                    "Error: %s can't retrieve application[%d][%d] for current config",__FUNCTION__,ModeNo,appForThisMode);
                continue;
            }

            appsListPerMode.push_back(retrievedAppObj.name);
        }
        retrievedConfigList.appList.push_back(appsListPerMode);
    }

    //modesList array here needs to be parsed
    JsArray JsArrModesList;
    if (OK != pJs->GetProperty(pLwrrConfigListObj, "modesList", &JsArrModesList))
    {
        Printf(Tee::PriHigh, "Error: addConfigList() Couldn't translate modeList input..\n"
              "Are you missing modeList Parameter?\n");
        CHECK_RC(RC::BAD_PARAMETER);
    }

    size_t      NumModeList   = JsArrModesList.size();
    JSObject   *pLwrrModeListObj;
    UINT32 i = 0;

    for (i = 0; i < NumModeList; i++)
    {
        if (OK != pJs->FromJsval(JsArrModesList[i], &pLwrrModeListObj))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve ConfigList[].ModeList[%d]",__FUNCTION__,i);
            continue;
        }

        ModeSettings        lwrrModeSettings;
        if (OK != addModeStgsObjHelper(pLwrrModeListObj,
                                      lwrrModeSettings))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve ModeSettings[%d]",__FUNCTION__,i);
            continue;
        }

        retrievedConfigList.modesList.push_back(lwrrModeSettings.name);
    }

    // parse forceModeOnSpecificProtocol,forceProtocolSequence for current config
    JsArray JsArrForceProtocolSequence;
    if (OK != pJs->GetProperty(pLwrrConfigListObj, "forceModeOnSpecificProtocol",   &retrievedConfigList.forceModeOnSpecificProtocol))
    {
        retrievedConfigList.forceModeOnSpecificProtocol = false;
        Printf(Tee::PriHigh, "%s: forceModeOnSpecificProtocol field not provided in input file.."
            "Defaulting forceModeOnSpecificProtocol = false\n",__FUNCTION__);
    }

    if (OK != pJs->GetProperty(pLwrrConfigListObj, "forceProtocolSequence",  &JsArrForceProtocolSequence))
    {
        if (retrievedConfigList.forceModeOnSpecificProtocol)
        {
            Printf(Tee::PriHigh, "%s: forceProtocolSequence Array must be provided when \"forceModeOnSpecificProtocol = true\" in input file.\n",
                   __FUNCTION__);
            CHECK_RC(RC::BAD_PARAMETER);
        }
        Printf(Tee::PriHigh, "%s: forceProtocolSequence Array not provided in input file.."
            "Defaulting forceProtocolSequence to empty vector\n",__FUNCTION__);
    }

    size_t NumProtocolSequence = JsArrForceProtocolSequence.size();
    string         protocolName;

    for (i = 0; i < NumProtocolSequence; i++)
    {
        if (OK != pJs->FromJsval(JsArrForceProtocolSequence[i], &protocolName))
        {
            Printf(Tee::PriHigh,
                "\n Error: %s can't retrieve forceProtocolSequence[%d]",__FUNCTION__,i);
            continue;
        }
        if (!DTIUtils::VerifUtils::IsValidProtocolString(protocolName))
        {
            Printf(Tee::PriHigh,
                "\n Error: %s Invalid protocol string=> forceProtocolSequence[%d]=%s",__FUNCTION__,i,protocolName.c_str());
            CHECK_RC(RC::BAD_PARAMETER);
        }
        retrievedConfigList.forceProtocolSequence.push_back(protocolName);
    }

    if (retrievedConfigList.forceModeOnSpecificProtocol &&
        retrievedConfigList.forceProtocolSequence.size() < retrievedConfigList.modesList.size())
    {
        Printf(Tee::PriHigh,
            "\n Error: %s: forceProtocolSequence size [%d] must be atleast same as modeList size [%d] ",
            __FUNCTION__, (UINT32)retrievedConfigList.forceProtocolSequence.size(), (UINT32)retrievedConfigList.modesList.size());
        CHECK_RC(RC::BAD_PARAMETER);
    }

    g_IMPMarketingParameters.configList.push_back(retrievedConfigList);

    return OK;
}

JS_STEST_LWSTOM(Modeset,
                addIMPMarketingParameters,
                1,
                "Reads the marketing parameters from the input javascript file and passes it to the test.")
{
    MASSERT(pContext != 0);
    MASSERT(pObject != 0);
    MASSERT(pArguments != 0);
    MASSERT(pReturlwalue != 0);

    RC rc;
    JavaScriptPtr pJs;

    // Check the arguments.
    JSObject * pIMPParams;
    JsArray  JsArrIMPTestLevelConfig;
    JsArray  JsArrIMPConfigList;

    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &pIMPParams))
        )
    {
        Printf(Tee::PriHigh,
                       "Usage: Modeset.IMPMarketingParameters(ImpParamObj)");
        return JS_FALSE;
    }

    if (OK != pJs->GetProperty(pIMPParams, "testLevelConfig", &JsArrIMPTestLevelConfig))
    {
        Printf(Tee::PriHigh, "Error:IMPMarketingParameters(): Couldn't translate testLevelConfig input..\n"
              "Are you missing TestLevelConfig Parameter?\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    string  businessUnit;
    UINT32  reproFailingModes,setModeDelay,attachDelay,detachDelay;
    bool    attachFakeDevices;
    bool    forceModeOnSpecificProtocol;
    JsArray JsArrForceProtocolSequence;
    JsArray JsArrAppsToLaunchNoOverlay,JsArrAppsToLaunchOverlay, JsArrAppsToLaunchStressOnPrimary;
    size_t  testLevelConfigCount = JsArrIMPTestLevelConfig.size();

    // If TestLevelConfig is present then consider only the first object of TestLevelConfig
    if (testLevelConfigCount > 0)
    {
        JSObject * pJsObjTestLevelConfig;

        C_CHECK_RC(pJs->FromJsval(JsArrIMPTestLevelConfig[0], &pJsObjTestLevelConfig));

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "businessUnit",        &businessUnit))
        {
            businessUnit = "";
            Printf(Tee::PriHigh, "%s: BusinessUnit field not provided in input file.."
                "Defaulting businessUnit = \"\"\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "reproFailingModes",   &reproFailingModes))
        {
            reproFailingModes = 1;
            Printf(Tee::PriHigh, "%s: reproFailingModes field not provided in input file.."
                "Defaulting reproFailingModes = 1\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "setModeDelay",        &setModeDelay))
        {
            setModeDelay = 0;
            Printf(Tee::PriHigh, "%s: setModeDelay field not provided in input file.."
                "Defaulting setModeDelay = 0\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "attachDelay",         &attachDelay))
        {
            attachDelay = 0;
            Printf(Tee::PriHigh, "%s: attachDelay field not provided in input file.."
                "Defaulting attachDelay = 0\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "detachDelay",         &detachDelay))
        {
            detachDelay = 0;
            Printf(Tee::PriHigh, "%s: detachDelay field not provided in input file.."
                "Defaulting detachDelay = 0\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "attachFakeDevices",   &attachFakeDevices))
        {
            attachFakeDevices = false;
            Printf(Tee::PriHigh, "%s: attachFakeDevices field not provided in input file.."
                "Defaulting attachFakeDevices = false\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "forceModeOnSpecificProtocol",   &forceModeOnSpecificProtocol))
        {
            forceModeOnSpecificProtocol = false;
            Printf(Tee::PriHigh, "%s: forceModeOnSpecificProtocol field not provided in input file.."
                "Defaulting forceModeOnSpecificProtocol = false\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "forceProtocolSequence",  &JsArrForceProtocolSequence))
        {
            if (forceModeOnSpecificProtocol)
            {
                Printf(Tee::PriHigh, "%s: forceProtocolSequence Array must be provided when \"forceModeOnSpecificProtocol = true\" in input file.\n",
                        __FUNCTION__);
                return JS_FALSE;
            }
            Printf(Tee::PriHigh, "%s: forceProtocolSequence Array not provided in input file.."
                "Defaulting forceProtocolSequence to empty vector\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "appsToLaunchNoOverlay",  &JsArrAppsToLaunchNoOverlay))
        {
            Printf(Tee::PriHigh, "%s: appsToLaunchNoOverlay Array not provided in input file.."
                "Defaulting appsToLaunchNoOverlay to empty vector\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "appsToLaunchOverlay",    &JsArrAppsToLaunchOverlay))
        {
            Printf(Tee::PriHigh, "%s: appsToLaunchOverlay Array not provided in input file.."
                "Defaulting appsToLaunchNoOverlay to empty vector\n",__FUNCTION__);
        }

        if (OK != pJs->GetProperty(pJsObjTestLevelConfig, "appsToLaunchStressOnPrimary", &JsArrAppsToLaunchStressOnPrimary))
        {
            Printf(Tee::PriHigh, "%s: appsToLaunchStressOnPrimary Array not provided in input file.."
                   "Defaulting appsToLaunchStressOnPrimary to empty vector\n",__FUNCTION__);
        }
    }

    size_t NumProtocolSequence = JsArrForceProtocolSequence.size();
    size_t NumAppNoOvly = JsArrAppsToLaunchNoOverlay.size();
    size_t NumAppOvly   = JsArrAppsToLaunchOverlay.size();
    size_t NumAppStressOnPrimary   = JsArrAppsToLaunchStressOnPrimary.size();

    vector<string> forceProtocolSequence;
    vector<string> appsListNoOverlay;
    vector<string> appsListOverlay;
    vector<string> appsListStressOnPrimary;
    string         protocolName;
    JSObject      *pLwrrApplicationObj;
    UINT32 i = 0;

    for (i = 0; i < NumProtocolSequence; i++)
    {
        if (OK != pJs->FromJsval(JsArrForceProtocolSequence[i], &protocolName))
        {
            Printf(Tee::PriHigh,
                   "\n Error: %s can't retrieve forceProtocolSequence[%d]", __FUNCTION__, i);
            continue;
        }
        if (!DTIUtils::VerifUtils::IsValidProtocolString(protocolName))
        {
             Printf(Tee::PriHigh,
                    "\n Error: %s Invalid protocol string=> forceProtocolSequence[%d]=%s", __FUNCTION__, i, protocolName.c_str());
             RETURN_RC(RC::BAD_PARAMETER);
        }
        forceProtocolSequence.push_back(protocolName);
    }

    for (i = 0; i < NumAppNoOvly; i++)
    {
        if (OK != pJs->FromJsval(JsArrAppsToLaunchNoOverlay[i], &pLwrrApplicationObj))
        {
            Printf(Tee::PriHigh,
                   "Error: %s can't retrieve appsToLaunchNoOverlay.application[%d]", __FUNCTION__, i);
            continue;
        }

        Application retrievedAppObj;
        if (OK != addAppObjHelper(pLwrrApplicationObj, retrievedAppObj))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve appsToLaunchNoOverlay.application[%d]", __FUNCTION__, i);
            continue;
        }

        appsListNoOverlay.push_back(retrievedAppObj.name);
    }

    for (i = 0; i < NumAppOvly; i++)
    {
        if (OK != pJs->FromJsval(JsArrAppsToLaunchOverlay[i], &pLwrrApplicationObj))
        {
            Printf(Tee::PriHigh,
                   "Error: %s can't retrieve appsToLaunchOverlay.application[%d]", __FUNCTION__, i);
            continue;
        }

        Application retrievedAppObj;
        if (OK != addAppObjHelper(pLwrrApplicationObj, retrievedAppObj))
        {
            Printf(Tee::PriHigh,
                   "Error: %s can't retrieve appsToLaunchOverlay.application[%d]", __FUNCTION__, i);
            continue;
        }

        appsListOverlay.push_back(retrievedAppObj.name);
    }

    for (i = 0; i < NumAppStressOnPrimary; i++)
    {
        if (OK != pJs->FromJsval(JsArrAppsToLaunchStressOnPrimary[i], &pLwrrApplicationObj))
        {
            Printf(Tee::PriHigh,
                   "Error: %s can't retrieve appsToLaunchStressOnPrimary.application[%d]", __FUNCTION__, i);
            continue;
        }

        Application retrievedAppObj;
        if (OK != addAppObjHelper(pLwrrApplicationObj, retrievedAppObj))
        {
            Printf(Tee::PriHigh,
                   "Error: %s can't retrieve appsToLaunchStressOnPrimary.application[%d]", __FUNCTION__, i);
            continue;
        }

        appsListStressOnPrimary.push_back(retrievedAppObj.name);
    }

    g_IMPMarketingParameters.testLevelConfig =
                     TestLevelConfig(businessUnit,
                                     reproFailingModes,
                                     setModeDelay,
                                     attachDelay,
                                     detachDelay,
                                     attachFakeDevices,
                                     forceModeOnSpecificProtocol,
                                     forceProtocolSequence,
                                     appsListNoOverlay,
                                     appsListOverlay,
                                     appsListStressOnPrimary);

    if (OK != pJs->GetProperty(pIMPParams, "configList", &JsArrIMPConfigList))
    {
        Printf(Tee::PriHigh, "Error:IMPMarketingParameters(): Couldn't translate configList input..\n"
              "Are you missing configList Parameter?\n");
        RETURN_RC(RC::BAD_PARAMETER);
    }

    size_t      NumConfigList   = JsArrIMPConfigList.size();
    JSObject   *pLwrrConfigListObj;

    for (i = 0; i < NumConfigList; i++)
    {
        if (OK != pJs->FromJsval(JsArrIMPConfigList[i], &pLwrrConfigListObj))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve ConfigList[%d]",__FUNCTION__,i);
            continue;
        }

        ConfigList lwrrConfigList;
        if (OK != addConfigList(pLwrrConfigListObj,lwrrConfigList))
        {
            Printf(Tee::PriHigh,
                        "Error: %s can't retrieve addConfigList[%d]",__FUNCTION__,i);
            continue;
        }
    }

    RETURN_RC(OK);
}

