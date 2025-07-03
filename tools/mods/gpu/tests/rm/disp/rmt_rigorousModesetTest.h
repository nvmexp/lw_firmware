/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2014,2016-2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//!
//! \file rmt_rigorousModesetTest.h
//! \brief IMPtest to cycle through all resolution provided in the input config file
//! \and check for major IMP features functioning
//!
#ifndef INCLUDED_RMTEST_H
#include "gpu/tests/rmtest.h"
#endif

#ifndef INCLUDED_DISPLAY_H
#include "core/include/display.h"
#endif

#include "ctrl/ctrl0073.h"

#ifndef INCLUDED_DTIUTILS_H
#include "gpu/tests/rm/utility/dtiutils.h"
#endif

#include "gpu/tests/rm/clk/rmt_fermipstateswitch.h"

// define where to print the message.
// 3 possible output includes STD_OUT, .LOG file and .CSV file
#define PRINT_STDOUT             1
#define PRINT_LOG_FILE           2
#define PRINT_CSV_FILE           4
using namespace DTIUtils;

// 3DMarkVantage needs lots time to scan system info before launching
#define THREEDMARK_VANTAGE_LAUNCH_TIME_SEC   30

#define GENERIC_APPLICATION_LAUNCH_TIME_SEC   5

// align the defines with rmtest.js
#define TRISTATE_DISABLE    0
#define TRISTATE_ENABLE     1
#define TRISTATE_DEFAULT    2

#define IMPTEST3_VPSTATE_MIN       258
#define IMPTEST3_VPSTATE_MAX       256
#define IMPTEST3_VPSTATE_UNDEFINED 257

// This value is derived from experiments. 10sec leads to 94~95% asymptotic result in average to 100 sec.
#define ISOFBLATENCY_THRESHOLD 10

typedef struct _resolution
{
    LwU32 width;
    LwU32 height;
    LwU32 refreshRate;

    _resolution() : width(0), height(0), refreshRate(0)
    {
    }
}resolution;

#define APP_FLAG_POLL_COMPLETE                  1
#define APP_FLAG_SET_POSITION_AND_SIZE          2

typedef struct _Application
{
    string      name;
    string      command;
    string      parameters;
    string      directory;
    UINT32      seconds;
    UINT32      xPos;
    UINT32      yPos;
    UINT32      flags;
    UINT32      mClkUtilThreshold;
    UINT32      gpuClkUtilThreshold;

    _Application(): name(""), command(""), parameters(""), directory(""), seconds(0), xPos(1), yPos(1), flags(0), mClkUtilThreshold(0), gpuClkUtilThreshold(0)
    {
    }

    _Application( string      Name ,
        string      AppCmd         = "",
        string      AppParams      = "",
        string      AppDir         = "",
        UINT32      Seconds        = 0,
        UINT32      xPosition      = 1,
        UINT32      yPosition      = 1,
        UINT32      Flags          = 0,
        UINT32      AppMClkUtilThreshold    = 0,
        UINT32      AppGpuClkUtilThreshold   = 0
        ):name(Name), command(AppCmd), parameters(AppParams), directory(AppDir),
        seconds(Seconds), xPos(xPosition), yPos(yPosition), flags(Flags), mClkUtilThreshold(AppMClkUtilThreshold), gpuClkUtilThreshold(AppGpuClkUtilThreshold)
    {
    }

}Application;

typedef struct _ModeSettings
{
    string name;
    Display::Mode mode;
    string colorFormat;
    bool interlaced;
    UINT32 timing;
    bool scaling;
    UINT32 overlayDepth;

    _ModeSettings():name(""),mode(),colorFormat("A8R8G8B8"),
        interlaced(false),timing(Display::GTF),scaling(false),
        overlayDepth(0)
    {
    }

    _ModeSettings(string             Name,
        Display::Mode      Mode                       = Display::Mode(0,0,32,60),
        string             ColorFormat                = "A8R8G8B8",
        bool               Interlaced                 = false,
        UINT32             Timing                     = Display::GTF,
        bool               Scaling                    = false,
        UINT32             OverlayDepth               = 0
        ):name(Name),mode(Mode),colorFormat(ColorFormat),
        interlaced(Interlaced),timing(Timing),scaling(Scaling),
        overlayDepth(OverlayDepth)
    {
    }

    // 2 modes can be compared based on fetch rate it would need.
    static bool compare(_ModeSettings a, _ModeSettings b)
    {
        return ( ((UINT64)a.mode.width * a.mode.height * a.mode.refreshRate * a.mode.depth) <
                 ((UINT64)b.mode.width * b.mode.height * b.mode.refreshRate * b.mode.depth));
    }
}ModeSettings;

typedef struct _TestLevelConfig
{
    string  businessUnit;
    UINT32  reproFailingModes;
    UINT32  setModeDelay;
    UINT32  attachDelay;
    UINT32  detachDelay;
    bool    attachFakeDevices;
    bool    forceModeOnSpecificProtocol; //When this is true : we will apply the mode on the sequence specified in the "forceSequence".
    //when this is false: we will apply the mode on display that is capable of running that mode..
    // Internally any protocol can be used
    vector<string>   forceProtocolSequence;     //:["DP","TMDS","TMDS", "CRT"],

    // Applications to launch when no overlay
    vector<string>      appsToLaunchNoOverlay;
    vector<string>      appsToLaunchOverlay;
    vector<string>      appsToLaunchStressOnPrimary;

    _TestLevelConfig(): businessUnit("DESKTOP_GeForce"),
        reproFailingModes(1),
        setModeDelay(0),
        attachDelay(0),
        detachDelay(0),
        attachFakeDevices(false),
        forceModeOnSpecificProtocol(false)
    {
        forceProtocolSequence = vector<string>();
        appsToLaunchNoOverlay = vector<string>();
        appsToLaunchOverlay   = vector<string>();
        appsToLaunchStressOnPrimary = vector<string>();
    }

    _TestLevelConfig(string  BusinessUnit,
        UINT32  ReproFailingModes  = 1,
        UINT32  SetModeDelay       = 0,
        UINT32  AttachDelay        = 0,
        UINT32  DetachDelay        = 0,
        bool    AttachFakeDevices  = false,
        bool    ForceModeOnSpecificProtocol= false,
        vector<string>      ForceProtocolSequence = vector<string>(),
        vector<string>      AppsToLaunchNoOverlay = vector<string>(),
        vector<string>      AppsToLaunchOverlay   = vector<string>(),
        vector<string>      AppsToLaunchStressOnPrimary = vector<string>()
        ) : businessUnit(BusinessUnit),
        reproFailingModes(ReproFailingModes),
        setModeDelay(SetModeDelay),
        attachDelay(AttachDelay),
        detachDelay(DetachDelay),
        attachFakeDevices(AttachFakeDevices),
        forceModeOnSpecificProtocol(ForceModeOnSpecificProtocol),
        forceProtocolSequence(ForceProtocolSequence),
        appsToLaunchNoOverlay(AppsToLaunchNoOverlay),
        appsToLaunchOverlay(AppsToLaunchOverlay),
        appsToLaunchStressOnPrimary(AppsToLaunchStressOnPrimary)
    {
    }

}TestLevelConfig;

typedef struct _ConfigList
{
    Display::VIEW_MODE  viewMode;
    UINT32              porPState;
    UINT32              porVPState;
    vector<UINT32>      activeDisplaysCount;
    UINT32              overlayCount;
    vector<UINT32>      validForChip;
    vector<string>      modesList;       // stores the Keys of map modeSettings

    // When this is true : we will apply the mode on the sequence specified in the "forceProtocolSequence".
    // when this is false: we will apply the forceModeOnSpecificProtocol settings provided in the testLevelConfig if that is true
    // else : run mode on display that is capable of running that mode wherein internally any protocol can be used
    bool    forceModeOnSpecificProtocol;

    // this means force modes on DisplayIDs corresponding to the protocols specified here.
    // Mode1 on SINGLE_TMDS displayID, mode2 on SINGLE_TMDS displayID, Mode3 on "DP", Mode4 on "CRT" displayID
    // Fails if corresponding displayID not found
    vector<string>   forceProtocolSequence;     //:["SINGLE_TMDS","SINGLE_TMDS","DP", "CRT"],

    bool                    useInlineApp;
    vector<vector<string> >  appList;

    _ConfigList():viewMode(Display::STANDARD),
        porPState(0),
        porVPState(0),
        activeDisplaysCount(),
        overlayCount(0),
        validForChip(),
        modesList(),
        forceModeOnSpecificProtocol(false),
        useInlineApp(false),
        appList()
    {
        forceProtocolSequence = vector<string>();
    }

    _ConfigList( Display::VIEW_MODE  ViewMode,
        UINT32              PorPState     = 0,
        UINT32              PorVPState    = 0,
        vector<UINT32>      ActiveDisplaysCount = vector<UINT32>(),
        UINT32              OverlayCount        = 0,
        vector<UINT32>      ValidForChip        = vector<UINT32>(),
        vector<string>      Modelist            = vector<string>(),
        bool                ForceModeOnSpecificProtocol = false,
        vector<string>      ForceProtocolSequence       = vector<string>(),
        bool                UseInlineApp        = false,
        vector< vector<string> >  AppList         = vector<vector<string> >()
        ):viewMode(ViewMode),
        porPState(PorPState),
        activeDisplaysCount(ActiveDisplaysCount),
        overlayCount(OverlayCount),
        validForChip(ValidForChip),
        modesList(Modelist),
        forceModeOnSpecificProtocol(ForceModeOnSpecificProtocol),
        forceProtocolSequence(ForceProtocolSequence),
        useInlineApp(UseInlineApp),
        appList(AppList)
    {
    }
}ConfigList;

typedef struct _IMPMarketingParameters
{
    TestLevelConfig         testLevelConfig;
    vector<ConfigList>      configList;

    _IMPMarketingParameters():testLevelConfig(),configList()
    {
    }

}IMPMarketingParameters;

typedef enum
{
    Undefined       = 0x00000000,
    P0              = 0x00000001,
    P1              = 0x00000002,
    P2              = 0x00000004,
    P3              = 0x00000008,
    P4              = 0x00000010,
    P5              = 0x00000020,
    P6              = 0x00000040,
    P7              = 0x00000080,
    P8              = 0x00000100,
    P9              = 0x00000200,
    P10             = 0x00000400,
    P11             = 0x00000800,
    P12             = 0x00001000,
    P13             = 0x00002000,
    P14             = 0x00004000,
    P15             = 0x00008000,
    MaxPState       = 0x00008000,
    MinPState       = 0x000000FF
}PState;

typedef struct _DispParams
{
    DisplayID dispId;
    vector<Display::Mode> modes;
    Display::Mode largestMode;

    _DispParams():dispId(0),modes(),largestMode()
    {
    }

    static bool compare(_DispParams a, _DispParams b)
    {
        // 2 modes can be compared based on fetch rate it would need.
        return ( ((UINT64)a.largestMode.width * a.largestMode.height * a.largestMode.refreshRate * a.largestMode.depth) <
                 ((UINT64)b.largestMode.width * b.largestMode.height * b.largestMode.refreshRate * b.largestMode.depth));
    }
} DispParams;

typedef struct _ModeSettingsEx
{
    DisplayID       displayId;
    ModeSettings    modeSetting;

    _ModeSettingsEx() : displayId(0), modeSetting()
    {
    }
} ModeSettingsEx;

//! struct LaunchApplicationParams
//!     This structure is needed for the LaunchApplication()
typedef struct _LaunchApplicationParams
{
    Display        *pDisplay;      // Needed to ilwoke checkDispUnderflow
    UINT32          numHeads;       // Needed to ilwoke checkDispUnderflow
    Application     app;
    LwS32           startXPosOfDisplay;
    LwS32           startYPosOfDisplay;
    LwS32           appWidth;
    LwS32           appHeight;
    char            threadName[128];
    UINT32          launchTimeSec;  // Default launch time is 5 secs, it may be overridden to 30 secs when building app parameters for 3DMarkV
    bool            runOnEmu;
    GpuSubdevice   *gpuSubdev;
    _LaunchApplicationParams() :app(),
                                startXPosOfDisplay(0),
                                startYPosOfDisplay(0),
                                appWidth(0),
                                appHeight(0),
                                launchTimeSec(GENERIC_APPLICATION_LAUNCH_TIME_SEC),
                                runOnEmu(false),
                                gpuSubdev(NULL)
    {
    }
} LaunchApplicationParams;

#if LWOS_IS_WINDOWS && !defined(SIM_BUILD) && !defined(WIN_MFG)

    //! struct _ProcessWindowsInfo
    //! This structure is needed to communicate with the callback
    //! function EnumProcessWindowsProc()which returns the
    //! window handle for the processID passed to it.
    typedef struct _ProcessWindowsInfo
    {
        DWORD ProcessID;
        std::vector<HWND> Windows;

        _ProcessWindowsInfo( DWORD const AProcessID )
            : ProcessID( AProcessID )
        {
        }
    }ProcessWindowsInfo;

    //! function EnumProcessWindowsProc()
    //!     Used to find the window handle for the processID passed to it.
    //! This function is an callback function which retrieves
    //! the ProcessID for the window handle passed to it.
    //! If the ProcessID retrieved is same as the one we are looking for
    //! then it stores this HWND into ProcessWindowsInfo=>Windows vector.
    BOOL __stdcall EnumProcessWindowsProc(HWND hwnd, LPARAM lParam );

    //! function MoveAndSize()
    //!     This function moves the application whose windows handle is passed
    //! to xPos, yPos position. It also resizes the application to the width
    //! and height provided to it.
    //! params m_Hwnd    [in]      : windows handle of process to be moved
    //! params xPos      [in]      : X co-ordinate where this process it to be moved
    //! params yPos      [in]      : Y co-ordinate where this process it to be moved
    //! params width     [in]      : resize the process window to this width
    //! params height    [in]      : resize the process window to this width
    void MoveAndSize(HWND       m_Hwnd,
                     LwS32        xPos,
                     LwS32        yPos,
                     LwS32        width,
                     LwS32        height);
#endif

class Modeset : public RmTest
{
public:
    Modeset();
    virtual ~Modeset();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    RC populateKnownRes(string , vector<resolution*>&);
    RC TestLwrrCombination(DisplayIDs dispIds,
        LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS dispHeadMapParams,
        vector<ModeSettingsEx>   lwrrModesOnOtherDispIds);
    RC      TestAllEdidResCombi(DisplayIDs workingSet);
    RC      GenerateWorstCaseConfig(DisplayIDs   *pAvailableDispIDs,
                                    UINT32   activeDisplaysCount,
                                    UINT32   overlayCount,
                                    UINT32   porPState,
                                    UINT32   porVPState,
                                    vector<ModeSettings> *pWorstCaseModeSettings);
    RC      TestInputConfigFile(DisplayIDs workingSet);
    RC      sortDispIdsAndFillDispParams(DisplayIDs workingset,
        vector<DispParams> &sortedWorkingSet,
        bool useWindowsResList);
    RC      ProgramAndVerifyPState(UINT32 PStateMask);
    RC      RandomPStateTransitions(UINT32 MinPStateMask, UINT32 iterNum, UINT32 durationSecs = 0, bool bCheckUnderflow = true);

    //! function SetUnderflowStateOnAllHead
    //! Set Underflow state on all heads.
    //! Function paramters are to allow have more control over the function.
    //! When no argument specified it clears underflow and enables underflow reporting
    RC      SetUnderflowStateOnAllHead(
        UINT32 enable           = LW5070_CTRL_CMD_UNDERFLOW_PROP_ENABLED_YES,
        UINT32 clearUnderflow   = LW5070_CTRL_CMD_UNDERFLOW_PROP_CLEAR_UNDERFLOW_YES,
        UINT32 mode             = LW5070_CTRL_CMD_UNDERFLOW_PROP_MODE_RED);

    //! function SetLargeLwrsor
    //! sets large cursor on the disp ids specified as argument.
    RC SetLargeLwrsor(const DisplayIDs  *pSetLargeLwrsorDispIds,
                      Surface2D         *pLwrsChnImg);

    //! function GetDisplayStartPosAndSize()
    //!     This function returns the startX, startY position of the displayID
    //! along with the current mode on it
    //! params displayId [in]       : displayID whose details are to be retreived.
    //! params startXPos [out]      : starting X co-ordinate of this Display
    //! params startYPos [out]      : starting Y co-ordinate of this Display
    //! params width     [out]      : width  of mode on this Display
    //! params height    [out]      : height of mode on this Display
    RC GetDisplayStartPosAndSize(DisplayID displayId,
                                 LwS32 *startXPos, LwS32 *startYPos,
                                 LwU32 *width, LwU32 *height);

    //! function Sleep()
    //! Helpler function to put the current thread to sleep for a given number(base on ptimer) of milliseconds,
    //!     giving up its CPU time to other threads.
    //!     Warning, this function is not for precise delays.
    static void Sleep(LwU32 miliseconds, LwBool runOnEmu, GpuSubdevice *gpuSubdev);

    //! function IsModeSupported()
    //! Helpler function to check whether mode is supported on given displayID.
    RC IsModeSupported(Display::Mode      mode,
                       DisplayID          dispId,
                       vector<DispParams> &sortedWorkingSet,
                       bool               *pModeSupportedOnMonitor);

    bool combinationGenerator(vector<UINT32>& validCombi,
        UINT32 trialCombi,
        UINT32 numOfHeads,
        UINT32 numOfDisps);
    RC getConfigFromString(string reproSring, SetImageConfig *lwrrReproConfig);
    RC runReproConfig(SetImageConfig *pReproConfig);
    RC WriteCSVAndReport(vector<SetImageConfig> setImageConfig, ColorUtils::Format colorFormat,
        bool bIMPpassed, bool bSetModeStatus, const string &comment, string &reportString);

    RC PRINT_STDOUT_N_REPORT(UINT32 priority, const char *message, UINT32 printWhere);
    RC GetDisplayIdFromGpuAndOutputId(UINT32        gpuId,
        DisplayID     modsDisplayID,
        UINT32       *lwApiDisplayId);

    SETGET_PROP(onlyConnectedDisplays, bool);
    SETGET_PROP(manualVerif, bool);
    SETGET_PROP(enableASRTest, bool);
    SETGET_PROP(enableMSCGTest, bool);
    SETGET_PROP(enableImmedFlipTest, bool);
    SETGET_PROP(mempoolCompressionTristate, UINT08);
    SETGET_PROP(minMempoolTristate, UINT08);
    SETGET_PROP(isoFbLatencyTest, UINT32);// When == 0, the Iso Fb latency test is disabled.
                                                 // When > 0, the Iso Fb latency test is enabled. And it indicates the min acceptable record time (in seconds).
    SETGET_PROP(immedFlipTestCheckMSCGSecs, UINT32);
    SETGET_PROP(protocol, string);   //Config protocol through cmdline
    SETGET_PROP(repro, string);      //Config repro through cmdline
    SETGET_PROP(runfrominputfile, bool);     //when true:Runs from input config file..False:Runs all combinations for all edid resolutions
    SETGET_PROP(inputconfigfile, string);    //input config file passed here
    SETGET_PROP(runOnlyConfigs, string);    // comma seperated list to specifying config nos to run from the input config file. It starts with 0.
    SETGET_PROP(pixelclock, UINT32); //Config pixel clock through cmdline
    SETGET_PROP(pstateSwitchCount, UINT32);
    SETGET_PROP(randomPStateSwitchIntervalMS, UINT32);
    SETGET_PROP(useCRTFakeEdid, string);    // Edid file to use for faking CRT
    SETGET_PROP(useDFPFakeEdid, string);    // Edid file to use for faking DFP
    SETGET_PROP(autoGenerateEdid, bool);    // When true: auto generate EDID file for POR mode verif.
                                            // For now only valid on Windows OS.
    SETGET_PROP(freqTolerance100x, UINT32); // allowed clock freq tolerance
    SETGET_PROP(ignoreClockDiffError, bool); // allowed ignore failure due to clock freq diff
    SETGET_PROP(disableRandomPStateTest, bool); // allowed turn off Random PState Test
    SETGET_PROP(useWindowsResList, bool); // When true use windows api to prune resolutions.
                                          // This returns more resolutions than what EDID returns.
                                          // Only valid on Windows OS.
    SETGET_PROP(forceLargeLwrsor, bool);  // Force large cursor 64x64 for upto GF119. 256x256 for kepler_And_Above
    SETGET_PROP(forcePStateBeforeModeset, bool);  // When true, forces POR Pstate prior to the modeset.
    SETGET_PROP(useAppRunCheck, bool);  // When true, checks whether an app is running or not

private:
    LwRmPtr pLwRm;
    bool m_onlyConnectedDisplays;
    bool m_enableASRTest;
    bool m_enableMSCGTest;
    bool m_enableImmedFlipTest;
    UINT08 m_mempoolCompressionTristate;
    UINT08 m_minMempoolTristate;
    UINT32 m_isoFbLatencyTest;
    UINT32 m_immedFlipTestCheckMSCGSecs;
    bool m_SwitchPStates;
    bool m_IgnoreUnderflow;
    bool m_manualVerif;
    string m_protocol;
    string m_repro;
    bool   m_runfrominputfile;
    string m_inputconfigfile;
    string m_runOnlyConfigs;
    vector<LwU32> m_runOnlyConfigsList;
    UINT32 m_pixelclock;
    UINT32 m_pstateSwitchCount;
    UINT32 m_randomPStateSwitchIntervalMS;
    string m_useCRTFakeEdid;
    string m_useDFPFakeEdid;
    string m_testStartTime;
    string         m_referenceEdidFileName_DFP;
    bool           m_autoGenerateEdid;
    const string m_appName3DMarkV;
    const string m_appConfigFile3DMarkV;
    const char    *m_PerfStatePrint;
    const char    *m_PerfStatePrintHelper;
    string m_appInstallLoc3DMarkV;
    vector<string> m_CRTFakeEdidFileList;
    vector<string> m_DFPFakeEdidFileList;
    UINT32         m_freqTolerance100x;
    UINT32         m_lwrrImpMinPState;
    bool           m_ignoreClockDiffError;
    bool           m_disableRandomPStateTest;
    bool           m_clockDiffFound;
    bool           m_useWindowsResList;
    bool           m_forceLargeLwrsor;
    bool           m_forcePStateBeforeModeset;
    bool           m_useAppRunCheck;
    bool           m_runOnEmu;
    bool           m_PStateSupport;
    bool           m_bIsPS30Supported;
    bool           m_bMclkSwitchPossible;
    bool           m_bOnetimeSetupDone;
    bool           m_bModesetEventSent;
    bool           m_bClockSwitchEventSent;
    bool           m_bClockSwitchDoneEventSent;
    UINT32         m_TraceDoneEventWaitCount;

    LW5070_CTRL_CMD_GET_RG_UNDERFLOW_PROP_PARAMS m_underFlowParamsOnInit;
    FermiPStateSwitchTest m_objSwitchRandomPState;
    UINT32 m_numHeads;
    Display *m_pDisplay;
    DisplayIDs          m_fakedDispIds;
    vector<resolution*> wellKnownRes;
    Surface2D    *m_channelImage;
    Surface2D    *m_pLwrsChnImg;
    Surface2D    *m_pOvlyChnImg;
    string      m_reportStr;
    string      m_CSVFileName;
    string      m_ReportFileName;
    FILE       *m_pCSVFile;
    FILE       *m_pReportFile;

    RC TestDisplayConfig(UINT32                    configNo,
                         const DisplayIDs          *pDisplayIds,
                         const vector<Display::Mode>    *pDisplayModes,
                         const vector<UINT32>      *pOverlayDepth,
                         const DisplayIDs          *pModesetDisplayIds,
                         const vector<Display::Mode>    *pModesToSet,
                         const vector<UINT32>      *pModesetHeads,
                         Display::VIEW_MODE        viewMode,
                         UINT32                    porPState,
                         UINT32                    porVPState,
                         UINT32                    uiDisplay,
                         Surface2D                 *pUiSurface,
                         const LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *pDispHeadMapParams);

    //! function checkSubTestResults()
    //!     This function checks each subtest result and output proper error to CSV file.
    //! params dispHeadMapParams [in]  : Mapping between displayID and head num.
    //! params dispIdsForLwrrRun [in]  : current active displayIDs
    //! params porMinPState      [in]  : min (V)PState for current running POR mode
    RC checkSubTestResults(const LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *pDispHeadMapParams,
                           const DisplayIDs *pDispIdsForLwrrRun,
                           UINT32 porMinPState = LW2080_CTRL_PERF_PSTATES_UNDEFINED);

    //! function checkAsrMscgTestVbiosSettings()
    //!     This function checks at least one pstate set ASR/MSCG to enable in VBIOS.
    bool checkAsrMscgTestVbiosSettings();

    //! function check3DMarkVantageInstallation()
    //!     This function checks if the 3DMarkVantage application is installed on the system.
    //!     Note that this function can only support on windows platform.
    bool check3DMarkVantageInstallation();

    //! function create3DMarkVantageConfig()
    //!     Used to create 3DMarkVantage config file for running it via cmdline.
    //!     Note that this function can only support on windows platform.
    //! params fileName [in]  : Config file name
    //! params width    [in]  : mode width
    //! params height   [in]  : mode height
    RC create3DMarkVantageConfig(string fileName, UINT32 width, UINT32 height);

    //! function configTestSettings()
    //!     Used to config settings for ASR/MSCG, mempool compression, and min mempool.
    RC configTestSettings();

    //! function checkAsrMscgTestResults()
    //!     This function checks result of ASR/MSCG test.
    RC checkAsrMscgTestResults();

    //! function printPerfStateHelper(UINT32 perfState)
    //!     Helper function to print the the pstate/vP-State depending
    //! on Perf System (PS20/PS30)
    string printPerfStateHelper(UINT32 perfState);
    //! function checkAsrEfficiency()
    //!     This function checks ASR efficiency and raise error if the actual efficiency is lower than redicted one.
    //! params pState [in]  : Current PState
    RC checkAsrEfficiency(LwU32 pState);

    //! function checkMscgEfficiency()
    //!     This function checks ASR efficiency and raise error if the actual efficiency is lower than redicted one.
    //! params pState [in]  : Current PState
    RC checkMscgEfficiency(LwU32 PState);

    //! function isMclkSwitchPossible()
    //!     Output the mclk switch settings and return if mclk switch is possible based on current mode.
    //! params dispHeadMapParams           [in]  : Mapping between displayID and head num
    //! params dispIdsForLwrrRun           [in]  : current active displayIDs
    //! params bMclkSwitchPossible         [out] : return value to indicate if mclk switch is possible
    //! params minPStateMclkSwitchPossible [out] : PState mask of the min-PState for mclk switch
    RC isMclkSwitchPossible(const LW0073_CTRL_SYSTEM_GET_HEAD_ROUTING_MAP_PARAMS *pDispHeadMapParams,
                            const DisplayIDs *pDispIdsForLwrrRun,
                            bool &bMclkSwitchPossible,
                            UINT32 &minPStateMclkSwitchPossible);

    //! function checkMclkSwitchGlitch()
    //!     This function checks if there is any CRC mis-match or underflow during pstate switch on current mode.
    //! params dispIdsForLwrrRun   [in]  : current active displayIDs
    //! params porMinPState        [in]  : min (v)pstate passed by the end user.Can be any (v)pstate and not just RM IMP Minpstate
    RC checkMclkSwitchGlitch(const DisplayIDs *pDispIdsForLwrrRun,
                             UINT32           porMinPState);

    //! function checkCRC()
    //!     This function is a helper function which checks if there is any CRC mis-match.
    //! params dispIdsForLwrrRun   [in] : current active displayIDs
    //! params perfState           [in] : the (v)PState transition against which CRC is checked.
    RC checkCRC(const DisplayIDs *pDispIdsForLwrrRun,
                UINT32 perfState);
    //! function StartIsoFbLatencyTest()
    //!     This function ask IMP to start iso fb latency measurement and maintain the max latency record.
    //!     We can later use EndIsoFbLatencyTest() to stop the measurement and get the callwlated result.
    RC StartIsoFbLatencyTest();

    //! function EndIsoFbLatencyTest()
    //!     This function ends iso fb latency measurement and get the callwlated result for this test.
    RC EndIsoFbLatencyTest();

    //! function GetIsoFbLatencyTestResult()
    //!     This function callwlates and verifies the result of iso fb latency test.
    RC VerifyIsoFbLatencyTestResult();

    //! function checkImmediateFlip()
    //!     This function checks the result of immediate flip test.
    //!     Note that this function can only support on windows platform.
    //! params primaryDisplay  [in]  : current primary displayID of current mode
    //! params porMinPState    [in]  : min (v)pstate passed by the end user.Can be any (v)pstate and not just RM IMP Minpstate
    RC checkImmediateFlip(DisplayID primaryDisplay,
                          UINT32 porMinPState);

    //! function resetIMPParameters()
    //!     This function restore the IMP parameter settings to the original ones.
    RC resetIMPParameters();

    //! function setIMPParameter()
    //!     This function set specified IMP parameter via RM interface - LW5070_CTRL_CMD_IMP_SET_GET_PARAMETER.
    //! params index      [in] : Index of LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_*
    //! params pstateApi  [in] : One of LW2080_CTRL_PERF_PSTATES_Pxxx defines
    //! params value      [in] : value needed to be applied
    RC setIMPParameter(LwU32 index, LwU32 pstateApi, LwU32 value);

    //! function getIMPParameter()
    //!     This function get specified IMP parameter via RM interface - LW5070_CTRL_CMD_IMP_SET_GET_PARAMETER.
    //! params index      [in]  : Index of LW5070_CTRL_IMP_SET_GET_PARAMETER_INDEX_*
    //! params pstateApi  [in]  : One of LW2080_CTRL_PERF_PSTATES_Pxxx defines
    //! params value      [out] : return value from the control call
    //! params head       [in]  : head index
    RC getIMPParameter(LwU32 index, LwU32 perfApi, LwU32 *value, LwU32 head = RC::ILWALID_HEAD);

    //! function getFbPartitionMask()
    //!     This function get the FB partition mask via RM interface - LW2080_CTRL_FB_INFO_INDEX_PARTITION_MASK.
    //! params partMask   [out]  : FB partition mask
    RC getFbPartitionMask(LwU32 &partMask);

    //! function getClkDomainFreq()
    //!     This function get the clock frequency of specified clock domain via RM interface
    //!     - LW2080_CTRL_CMD_CLK_GET_INFO.
    //! params clkDomain  [in]   : clock domain
    //! params clkFreq    [out]  : clock frequency of given domain
    RC getClkDomainFreq(LwU32 clkDomain, LwU32 &clkFreq);

    //! function runStressTestOnPrimaryDisplay()
    //!     This function run applications specified in appsToLaunchStressOnPrimary sequentially.
    //!     Note that this function can only support on windows platform.
    //! params primaryDisplay  [in]  : current primary displayID of current mode
    //! params porMinPState    [in]  : min (v)pstate passed by the end user.Can be any (v)pstate and not just RM IMP Minpstate
    RC runStressTestOnPrimaryDisplay(DisplayID primaryDisplay,
                                     UINT32    porMinPState);

    //! function setupAppParams()
    //!     This function setup the application parameters for a given display.
    //! params displayId       [in]  : Display ID
    //! params launchAppParams [out] : application parameters which needs to be setup
    //! params application     [in]  : application for given display
    RC setupAppParams(DisplayID displayId, LaunchApplicationParams *launchAppParams, const Application &application);

    //! function getCRCValues()
    //!     This function get CRC values for given diaplyID.
    //! params Display  [in]   : Display ID
    //! params CompCRC  [out]  : CRC value for compositor
    //! params PriCRC   [out]  : CRC value for primary
    //! params SecCRC   [out]  : CRC value for secondary
    RC getCRCValues(DisplayID Display, UINT32 &CompCRC, UINT32 &PriCRC, UINT32 &SecCRC);

    //! function outputCommentStr()
    //!     Helper function to output to log & CSV file.
    //!     For log file, it outputs the string in m_reportStr.
    //!     For CSV file, it outputs the passed-in string(leadingStr) to the fields in front of "comment" and
    //!     m_reportStr to "comment" field.
    //! params leadingStr  [in]   : output string for the fields in front of "comment" field
    void outputCommentStr(string leadingStr);
};
