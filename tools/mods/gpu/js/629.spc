

#ifndef CHECK_RC
    #define CHECK_RC(f)                     \
        do                                  \
        {                                   \
            if (OK != (rc = CastRc(f)))     \
            {                               \
                CheckRcFail(rc, arguments); \
                return rc;                  \
            }                               \
        } while (false)
#endif

//***************************************************************************
// Global argument setup
//***************************************************************************
g_SpecArgs  = "-chipset_aspm 0 -edc_tol 100";
g_SpecArgs += " -corr_error_tol 0xFFFFFFFF -blink_lights -timeout_ms 120000";
g_SpecArgs += " -tex_err_tol correctable 100 -tex_err_tol uncorrectable 0";
g_SpecArgs += " -ecc_corr_tol fb 1000 -ecc_corr_tol l2 100 -ecc_corr_tol l1 100 -ecc_corr_tol lrf 100";
g_SpecArgs += " -ecc_uncorr_tol fb 0 -ecc_uncorr_tol l2 0 -ecc_uncorr_tol l1 0 -ecc_uncorr_tol lrf 0";
g_SpecArgs += " -ecc_verbose 0x2 -edc_verbose 0x2 -pex_verbose 0xC -extra_pexcheck";
g_SpecArgs += " -pex_line_error_tol 9000 -pex_crc_tol 9000";
g_SpecArgs += " -pex_nak_rcvd_tol 9000 -pex_nak_sent_tol 9000 -pex_l0s_tol 9000";
g_SpecArgs += " -dump_inforom -dump_stats -lwlink_verbose 0x1";
g_SpecArgs += " -feature_check_skip Ecc";
g_SpecArgs += " -pstate_direct -ignore_ot_event";
g_SpecArgs += " -disable_pex_error_rate_check -ignore_cpu_affinity_failure";

if (Platform.IsPPC && !g_SeuNoPpcHotReset && !g_SeuNoHotReset)
    g_SpecArgs += " -hot_reset";

// Note that all Gemini boards have to be added to SetBoardSpecificOptions for
// proper PEX speed check. See comments in the SetBoardSpecificOptions function
// for the details

g_SpecArgs += " -test_devid 0x1eb8"; // TU104  PG183 SKU 200

g_SpecArgs += " -nonconlwrrent_test 101"; // WAR bug 1450444
g_SpecArgs += " -blacklist_pages_on_error";
g_SpecArgs += " -blacklist_pages_dynamic";

if (g_SeuPollInterrupts)
{
    g_SpecArgs += " -poll_interrupts";
}
// Also use -poll_interrupts on Linux kernel versions older than 2.6.22,
// so far the oldest version found to work fine with -auto_interrupts.
else if (OperatingSystem === "Linux"
         && CompareVersionNumbers(LinuxKernelVer, "2.6.22") < 0)
{
    g_SpecArgs += " -poll_interrupts";
}
else
{
    g_SpecArgs += " -auto_interrupts";
}

if (0 < g_SeuLogFileLimit)
{
    // do nothing!
}
else
{
    g_SpecArgs += " -log_file_limit_mb 50";
}

if (g_SeuIgnoreUnsupPcieReqs)
{
    g_SpecArgs += " -un_supp_req_tol 0xFFFFFFFF";
}

// PPC doesn't support hot reset, which enable_ecc requires
if (!Platform.IsPPC && !g_SeuNoHotReset)
{
    g_SpecArgs += " -enable_ecc";
}

if (!g_SeuSkipGl)
{
    g_SpecArgs += " -no_shader_cache";
}

g_SpecArgs += " -bg_print_ms 10000";
g_SpecArgs += " -bg_flush";
g_SpecArgs += " -bg_core_voltage 1000";
g_SpecArgs += " -bg_clocks Gpu.ClkGpc 1000";

if (null === g_SeuDevice && null === g_SeuPciid)
{
    g_SpecArgs += " -hdmi_fft_ratio 0.40";  // WAR for bug 778991
}

//***************************************************************************
// Device specific argument setup
//***************************************************************************

var g_SpecArgsDevice = "-perlink_corr_error 0 9000 9000";

var g_SeuTestMode = 128;  // TM_COMPUTE (cannot include mods.h due to how
                          // preprocessing is done in official builds)

var g_ClockStacks = new Object();
var g_SkipThermalSanity = false;
var g_SkipPcieSpeedChange = false;
function CheckConfigPostRun(retval) {}
var g_Test1Args = { PostRunCallback : CheckConfigPostRun };
var g_Test146Args = { IgnoreBwCheck : true };
var g_ValidSkuArgs2 = {}; // ValidSkuCheck2 (T217) only
var g_PcieSpeedChangeArgs = {};

// We need to know TDP point for PState 0
var g_P0TdpHz;

var g_BoardHasDisplay = false;

var g_SkuCheckOrInitFail = false;
var g_SerialNumber = "";

function RenameLogFile()
{
    var result = "";
    if (g_SkuCheckOrInitFail || g_SeuRetestBanner)
    {
        result = "CONFIG";
    }
    else
    {
        if (OK != Log.FirstError)
        {
            result = "FAIL";
        }
        else
        {
            result = "PASS";
        }
    }

    if ("undefined" === typeof HasLogFileName || !HasLogFileName)
    {
        // the log file name wasn't set by the 'logfilename=' command line option
        var orgName = Out.FileNameOnly;
        var nameAndExt = /(.*)(\.[^.]*)/.exec(orgName)
        var name = nameAndExt ? nameAndExt[1] : orgName;
        var ext = nameAndExt ? nameAndExt[2] : "";

        Out.RenameOnClose = name + "_" + result + "_" + g_SerialNumber + ext;
    }
    else
    {
        // the log file name was set by the 'logfilename=' command line option
        var orgName = Out.FileNameOnly;
        var state = { text : 0, format : 1 };
        var lwrState = state.text;
        var formattedName = "";

        // a simple state machine to process %r and %s patterns
        for (var i = 0; i < orgName.length; ++i)
        {
            var lwrChar = orgName[i];
            switch (lwrState)
            {
                // normal state
                case state.text:
                {
                    // switch to format state
                    if ("%" == lwrChar) lwrState = state.format;
                    // just copy the character
                    else formattedName += lwrChar;
                    break;
                }
                case state.format:
                {
                    // format state to process % patterns
                    switch (lwrChar)
                    {
                        // %r - copy the result string and switch back to the normal state
                        case "r":
                            formattedName += result;
                            lwrState = state.text;
                            break;
                        // %s - copy the serial number and switch back to the normal state
                        case "s":
                            formattedName += g_SerialNumber;
                            lwrState = state.text;
                            break;
                        // cannot recognize the format symbol, copy the input as is
                        default:
                            formattedName += "%" + lwrChar;
                            lwrState = state.text;
                    }
                    break;
                }
            }
        }
        Out.RenameOnClose = formattedName;
    }
}

function SkipPerfPt(PerfPt)
{
    if (null == PerfPt)
    {
        return true;
    }

    if (0 === PerfPt.PStateNum)
    {
        if ("min" === PerfPt.LocationStr
            || "max" === PerfPt.LocationStr
            || "tdp" === PerfPt.LocationStr
            || "turbo" === PerfPt.LocationStr
            || "intersect" === PerfPt.LocationStr)
        {
            return false;
        }
    }
    else if ("max" === PerfPt.LocationStr)
    {
        return false;
    }

    return true;
}

function addModsGpuTests(testList, PerfPtList)
{
    return Diag629Spec(testList, PerfPtList);
}

//***************************************************************************
// Test specification as of 4/16 @9am
//
// To run this test spec, execute the following command:
//         "mods gputest.js -readspec 629.spc -spec Diag629Spec"
//***************************************************************************

//------------------------------------------------------------------------------
function Diag629Spec(testList, PerfPtList)
{
    // TM_COMPUTE (cannot include mods.h due to how preprocessing is done
    // in official builds)
    testList.TestMode = 128;
    var p0OnlyPerfPt;

    if (Golden.Store == Golden.Action)
    {
        // Do just one pstate for gpugen.js.
        PerfPtList = PerfPtList.slice(0,1);
    }

    if (g_SeuP0Only)
    {
        // Go through all Perf points to figure out if '0.tdp' or '0.max' exist
        for (var j = 0; j < PerfPtList.length; j++)
        {
            if (PerfPtList[j].PStateNum == 0)
            {
                if ("tdp" === PerfPtList[j].LocationStr)
                {
                    p0OnlyPerfPt = PerfPtList[j];
                    break;
                }
                else if ("max" === PerfPtList[j].LocationStr)
                {
                    p0OnlyPerfPt = PerfPtList[j];
                }
            }
        }

        if ("undefined" === typeof p0OnlyPerfPt)
        {
            g_SeuValidationErrorStrings.push(Out.Sprintf("Error: Cannot find P0.max or P0.tdp points\n"));
            return;
        }
    }

    for (var i = 0, PerfPtIdx = 0; i < PerfPtList.length; i++)
    {
        var PerfPt = PerfPtList[i];

        if (SkipPerfPt(PerfPt))
            continue;

        var PStateNum = 0;
        if (PerfPt != null)
            PStateNum = PerfPt.PStateNum;

        if (g_SeuP0Only && Golden.Store != Golden.Action)
        {
            if(!DeepCompare(p0OnlyPerfPt, PerfPt))
            {
                continue;
            }
        }

        testList.AddTest("SetPState", { InfPts : PerfPt });

        if (g_SeuOnlySkuCheck)
        {
            Diag629SpecSkuCheckTests(testList, PerfPtIdx, PStateNum);
        }
        else if (g_SeuBasicTestsOnly)
        {
            Diag629SpecBasicSystemTests(testList, PerfPtIdx++, PStateNum);
        }
        else
        {
            if (!g_SeuSkipLwLinkTests)
            {
                // Note that if g_SeuOnlyLwLinkTests is true,
                // Diag629SpecLwLinkTests will also add CheckConfig test
                Diag629SpecLwLinkTests(testList, PerfPtIdx, PStateNum);
            }
            if (!g_SeuOnlyLwLinkTests)
            {
                Diag629SpecTestsOnePState(testList, PerfPtIdx, PStateNum);
            }
            PerfPtIdx++;
        }
    }

    testList.AddTest("CheckInfoROM", { DprPBLTest : true, PBLCapacity : 63,
                                       L1EccDbeTest : true, L2EccDbeTest : true,
                                       SmEccDbeTest : true, TexUncorrTest : true });
}

#if defined(DVS_BUILD)
// this will redefine Diag629Spec
#include "629dvs.spc"
#endif

function Diag629SpecSkuCheckTests
(
    testList       // (TestList)
  , PerfPtIdx      // (integer) how many PerfPoints we've tested already
  , pstate         // pstate being tested - not used
)
{
    if (0 === PerfPtIdx)
    {
        testList.AddTests(
        [
            [ "CheckConfig", g_Test1Args]
          ,   "CheckInfoROM"
          , [ "ValidSkuCheck2", g_ValidSkuArgs2]
        ]);
    }
}

function Diag629SpecBasicSystemTests
(
    testList       // (TestList)
  , PerfPtIdx      // (integer) how many PerfPoints we've tested already
  , pstate         // pstate being tested - not used
)
{
    Diag629SpecSkuCheckTests(testList, PerfPtIdx, pstate);

    testList.AddTest("NewLwdaMatsTest");

    if (!Platform.IsPPC)
    {
       testList.AddTest("CheckDma", { SkipMissingPstates : true, NoP2P : true });
    }

    testList.AddTests(
    [
        [ "DPStressTest", {} ]
      , [ "LwdaLinpackDgemm", {} ]
    ]);
}

function Diag629SpecLwLinkTests
(
    testList       // (TestList)
  , PerfPtIdx      // (integer) how many PerfPoints we've tested already
  , pstate         // pstate being tested - not used
)
{
    if (0 == PerfPtIdx)
    {
        if (g_SeuOnlyLwLinkTests)
        {
            testList.AddTests(
            [
                [ "CheckConfig", g_Test1Args]
            ]);
        }

        testList.AddTests(
        [
             [ "LwLinkBwStress", { TestAllGpus: true, SkipBandwidthCheck: true } ]
            ,[ "LwlbwsBeHammer", { TestAllGpus: true } ]
            ,[ "LwlbwsBgPulse", { TestAllGpus: true } ]
        ]);
    }
}

function Diag629SpecTestsOnePState
(
    testList       // (TestList)
  , PerfPtIdx      // (integer) how many PerfPoints we've tested already
  , pstate         // pstate being tested
)
{

    var isFirst = (0 == PerfPtIdx);
    var glrTestArgs = scaleLoops(40*20, 20, isFirst ? 1.0 : 0.1, PerfPtIdx, false);
    var shortGlrTestArgs = scaleLoops(20*20, 20, isFirst ? 1.0 : 0.1, PerfPtIdx, false);

    Diag629SpecSkuCheckTests(testList, PerfPtIdx, pstate);

    if (isFirst)
    {
        testList.AddTest("CheckAVFS", { AdcMargin: 7.0, TestConfiguration: { Loops: 13 }});
    }

    testList.AddTests(
    [
        [ "FastMatsTest", setIterations(2) ]
      , [ "WfMatsMedium", {} ]
      , [ "NewWfMatsNarrow", {} ]
      , [ "CpyEngTest", {} ]
    ]);

    if (!Platform.IsPPC)
    {
        testList.AddTests(
        [
            ["CheckDma", { SkipMissingPstates : true, NoP2P : true } ]
           ,[ "I2MTest", {} ]
        ]);
    }

    testList.AddTests(
    [
        [ "PexBandwidth", g_Test146Args ]
      , [ "SecTest", {} ]
    ]);

    if (!g_SkipPcieSpeedChange && !Platform.IsPPC)
    {
        testList.AddTests([[ "PcieSpeedChange", g_PcieSpeedChangeArgs ]]);
    }

    testList.AddTests(
    [
        [ "Random2dFb", {} ]
      , [ "Random2dNc", {} ]
      , [ "LineTest", {} ]
    ]);

    testList.AddTests(
    [
        [ "MSENCTest", {} ]
    ]);

    if (g_BoardHasDisplay)
    {
        testList.AddTestsWithParallelGroup(PG_DISPLAY,
        [
            [ "EvoLwrs", {} ]
          , [ "EvoOvrl", setLoops(isFirst ? 20 : 2) ]
          , [ "LwDisplayRandom", {"TestConfiguration":{"Loops":200,"RestartSkipCount":10}}]
          , [ "LwDisplayHtol", {} ]
          , [ "SorLoopback", {} ]
        ],
        {"display":true});
    }

    if (!g_SeuSkipGl)
    {
        testList.AddTestsWithParallelGroup(PG_GRAPHICS,
        [
            [ "GlrA8R8G8B8", glrTestArgs ]
          , [ "GlrFsaa2x", glrTestArgs ]
          , [ "GlrFsaa4x", glrTestArgs ]
          , [ "GlrMrtRgbU", glrTestArgs ]
          , [ "GlrMrtRgbF", glrTestArgs ]
          , [ "GlrY8", glrTestArgs ]
          , [ "GlrR5G6B5", glrTestArgs ]
          , [ "GlrFsaa8x", glrTestArgs ]
          , [ "GlrFsaa4v4", shortGlrTestArgs ]
          , [ "GlrFsaa8v8", shortGlrTestArgs ]
          , [ "GlrFsaa8v24", shortGlrTestArgs ]
          , [ "GLComputeTest" ]
        ]);
        testList.AddTest("SyncParallelTests", {"ParallelId" : PG_GRAPHICS});
    }
    if (g_BoardHasDisplay)
    {
        testList.AddTest("SyncParallelTests", {"ParallelId" : PG_DISPLAY});
    }

    if (!g_SeuSkipGl)
    {
        if (pstate === 8)
        {
            testList.AddTest("GlrA8R8G8B8GCx");
        }
    }

    testList.AddTests(
    [
        [ "Elpg", {} ]
      , [ "ElpgGraphicsStress", {} ]
      , [ "DeepIdleStress", { } ]
    ]);

    if (!g_SeuSkipGl)
    {
        testList.AddTests(
        [
            [ "GLStress", setLoopMs(isFirst ? 15000 : 1500) ]
          , [ "GLStressZ", setLoopMs(isFirst ? 5000 : 500) ]
          , [ "GLStressPulse", setLoopMs(isFirst ? 5000 : 500) ]
          , [ "GLRandomCtxSw", glrTestArgs]
        ]);
    }

    // reduce spew when run in the background
    testList.AddTestArgs("NewWfMatsCEOnly", { InnerLoops: 250 });

    testList.AddTests(
    [
        [ "NewLwdaMatsTest", {} ]
      , [ "LwdaMatsPatCombi", {} ]
      , [ "NewLwdaMatsRandom", {} ]
      , [ "DPStressTest", setLoops(2000) ]
      , [ "LwdaStress2", {} ]
      , [ "LwdaRandom", {} ]
      , [ "LwdaL2Mats", {} ]
      , [ "LwdaXbar", {} ]
      , [ "IntAzaliaLoopback", {} ]
      , [ "MMERandomTest", {} ]
    ]);
    if (!Platform.IsPPC)
    {
        testList.AddTests([[ "Bar1RemapperTest", {} ]]);
    }

    AddLinpackTests(testList, "LwdaLinpackDgemm", "EFT_Stab", 1.0, 2*60*1000, "", false, true);

    testList.AddTests(
    [
        [ "LwdaLinpackPulseDP", {CheckLoops: 100000, RuntimeMs: 2*60*1000} ]
    ]);

    AddLinpackTests(testList, "LwdaLinpackSgemm", "EFT_Stab", 1.0, 2*60*1000, "", false, true);

    testList.AddTests(
    [
        [ "LwdaLinpackPulseSP", {} ]
      , [ "LwdaLinpackPulseSP", {Ksize: 96} ]
      , [ "LwdaLinpackHgemm", {} ]
      , [ "LwdaLinpackPulseHP", {} ]
    ]);

    AddLinpackTests(testList, "LwdaLinpackHMMAgemm", "EFT_Stab", 1.0, 2*60*1000, "", false, true);
    AddLinpackTests(testList, "LwdaLinpackHMMAgemm", "EFT_Stab", 1.0, 2*60*1000,
                        "h884_fp16", false, true);

    testList.AddTest("LwdaLinpackPulseHMMA");
    AddLinpackTests(testList, "LwdaLinpackIgemm", "EFT_Stab", 1.0, 2*60*1000, "", false, true);
    testList.AddTest("LwdaLinpackPulseIP");

    // CaskLinpack tests
    testList.AddTests(
    [
         ["CaskLinpackSgemm",    {}]
        ,["CaskLinpackIgemm",    {}]
        ,["CaskLinpackDgemm",    {}]
        ,["CaskLinpackHgemm",    {}]
        ,["CaskLinpackHMMAgemm", {}]
        ,["CaskLinpackIMMAgemm", {}]
        ,["CaskLinpackDMMAgemm", {}]
        ,["CaskLinpackE8M10",    {}]
        ,["CaskLinpackE8M7",     {}]
        ,["CaskLinpackBMMA",     {}]
        ,["CaskLinpackINT4",     {}]
        ,["CaskLinpackSparseHMMA", {}]
        ,["CaskLinpackSparseIMMA", {}]
        ,["CaskLinpackPulseSgemm", {}]
        ,["CaskLinpackPulseIgemm", {}]
        ,["CaskLinpackPulseDgemm", {}]
        ,["CaskLinpackPulseHgemm", {}]
        ,["CaskLinpackPulseHMMAgemm", {}]
        ,["CaskLinpackPulseIMMAgemm", {}]
        ,["CaskLinpackPulseDMMAgemm", {}]
        ,["CaskLinpackPulseE8M10", {}]
        ,["CaskLinpackPulseE8M7", {}]
        ,["CaskLinpackPulseBMMA", {}]
        ,["CaskLinpackPulseINT4", {}]
        ,["CaskLinpackPulseSparseHMMA", {}]
        ,["CaskLinpackPulseSparseIMMA", {}]
    ]);

    // Remove EccFbTest, EccL2Test, EccL1 until bug 800106 is addressed

    testList.AddTests(
    [
        [ "EccSM", {} ]
      , [ "EccSMShm", {} ]
      , [ "EccTex", {} ]
      , [ "NewWfMatsShort", {} ]
      , [ "NewWfMats", {} ]
      ]);

    if (!g_SeuSkipGl)
    {
        testList.AddTest("GLStressFbPulse");
    }

    if (!g_SeuSkipGl)
    {
        testList.AddTest("GLInfernoTest", {LoopMs: 2*60*1000, EndLoopPeriodMs: 60000});
    }

    testList.AddTests(
    [
        [ "LwdaLdgTest", {} ]
      , [ "LwdaRadixSortTest", {DirtyExitOnFail: true, TestConfiguration:{TimeoutMs: 320000}} ]
    ]);
}

#include "linpack_tests.js"

function ValidatePex16(Subdev, Widths)
{
    var rc = OK;
    if ((Widths[0] != 16) || (Widths[1] != 16))
    {
        g_SeuValidationErrorStrings.push(Out.Sprintf(
                   "GPU %d:0 : PCI Express x8 width was detected. If your" +
                   " product is installed\n" +
                   "          in a x8 PCI Express slot, please run the " +
                   "test using the option \"x8\".\n",
                   Subdev.ParentDevice.DeviceInst));
        rc = RC.BAD_COMMAND_LINE_ARGUMENT;
    }
    return rc;
}

Log.Never("PexSpeedToGen");
function PexSpeedToGen(Speed)
{
    switch (Speed)
    {
        case Pci.Speed2500MBPS:
            return 1;
        case Pci.Speed5000MBPS:
            return 2;
        case Pci.Speed8000MBPS:
            return 3;
        default:
            return 0;
    }
    return 0
}

function ValidatePexSpeed(Subdev, ExpectedSpeed, ActualSpeed, AdjustForArgs)
{
    var rc = OK;
    var adjustedExpected = ExpectedSpeed;

    if (AdjustForArgs)
    {
        // "gen1" and "gen2" command line options should affect the affected
        // speed so that if they are used incorrectly the fieldiag will actually
        // fail due to Pex speed validation
        if (g_SeuPcieGen1)
            adjustedExpected = Pci.Speed2500MBPS;
        else if (g_SeuPcieGen2)
            adjustedExpected = Pci.Speed5000MBPS;

    }

    if (adjustedExpected == ActualSpeed)
        return OK;

    var genFound = PexSpeedToGen(ActualSpeed);
    var genExpected = PexSpeedToGen(ExpectedSpeed);
    var genAdjExpected = PexSpeedToGen(adjustedExpected);
    var errStr = Out.Sprintf(
                "GPU %d:0 : PCI Express Gen %d speed was detected " +
                "(Gen %d expected).\n",
                Subdev.ParentDevice.DeviceInst,
                genFound, genAdjExpected);

    if (genAdjExpected == genExpected)
    {
        errStr += Out.Sprintf(
                "          If the system only supports Gen %d " +
                "speeds, please run\n" +
                "          with the option \"gen%d\".\n",
                genFound, genFound);
    }
    else if (genFound == genExpected)
    {
        errStr += Out.Sprintf(
              "          Please remove the \"gen1\" and \"gen2\" arguments.\n");
    }
    else
    {
        errStr += Out.Sprintf(
                "          If the system only supports Gen %d " +
                "speeds, please run\n" +
                "          with the option \"gen%d\" (remove \"gen%d\").\n",
                genFound, genFound, genAdjExpected);
    }

    g_SeuValidationErrorStrings.push(errStr);
    return RC.BAD_COMMAND_LINE_ARGUMENT;
}

// In order to reasonably expect retrieval of the board name to succeed,
// pre-validate items that depend on either the current motherboard
// configuration and are controllable via the command line, or items
// that depend on VBIOS/Inforom (e.g. ECC) that are used in ValidSkuCheck
// for board name lookup.
//
// We do this before board name lookup so that we can provide the user
// with a more specific error than "the board doesn't match" since they
// cannot view the log to get the specific failure in stealth builds
function ValidatePreBoardName(subdev)
{
    var rc = OK;
    var finalRc = OK;

    if (!g_SeuPcieX8 && ((g_SeuPcieLanes < 0) || (g_SeuPcieLanes == 16)))
    {
        var linkWidths = new Array;
        CHECK_RC(subdev.GetPcieLinkWidths(linkWidths));
        rc = ValidatePex16(subdev, linkWidths);
        if (finalRc == OK)
            finalRc = rc;
    }

    var eccStatus = new Array;
    CHECK_RC(subdev.GetEccEnabled(eccStatus));
    if (!eccStatus[SubdevConst.ECC_SUPPORTED_IDX] ||
        !eccStatus[SubdevConst.ECC_ENABLED_MASK_IDX])
    {
        g_SeuValidationErrorStrings.push(Out.Sprintf(
                   "GPU %d:0 : The card under test has ECC disabled. Please " +
                   "enable ECC and proceed\n" +
                   "          to re-test the card.\n",
                   subdev.ParentDevice.DeviceInst));
        rc = RC.SOFTWARE_ERROR;
        if (finalRc == OK)
            finalRc = rc;
    }

    return finalRc;
}

function IsTuringTeslaBoard(BoardDef)
{
    if (BoardDef == null)
    {
        return false;
    }

    switch(BoardDef.BoardName)
    {
        // TU104 boards
        case "PG183-0200": // devid = 0x1eb8         - TU104
            return true;
    }

    return false;
}

// The 629 spec should only be run on certain boards
function IsTeslaBoard(BoardDef)
{
    return IsTuringTeslaBoard(BoardDef);
}

function GetTempOption(BoardDef)
{
    var tempOption = g_SeuGpuTemp;

    switch (BoardDef.BoardName)
    {
        default:
            break;

        case "PG183-0200":
            if (tempOption == "") tempOption = "int";
            break;
    }

    return tempOption;
}

function SetTempArgs(DevInst)
{
    var rc = OK;

    var Subdev = new GpuSubdevice(new TestDevice(DevInst));
    var tempOption = GetTempOption(GetBoardDef(Subdev));

    // Device specific temperature related arguments
    switch (tempOption)
    {
        default:
        case "":
            CHECK_RC(ArgBgExtTempFlush(DevInst, 10000, 1000));
            break;
        case "int":
            CHECK_RC(ArgBgIntTempFlush(DevInst, 10000, 1000));
            break;
        case "disabled":
            break;
    }

    return rc;
}

// Only board specific options that are applied *after* HandleGpuPostInitArgs
// can be changed in this function
function SetBoardSpecificOptions(Subdev)
{
    var rc = OK;

    var boardDef = GetBoardDef(Subdev);
    // This should never be null at the point where this is called
    if (boardDef == null)
        return RC.SOFTWARE_ERROR;

    var linkWidths = new Array;

    var monitorGpio9 = false;

    g_Test1Args["HwSlowdownMax"] = 0;

    // Before we used PcieSpeedCapability property to get PCIE speed capability
    // instead of GetPexSpeedAtPState. It turned out that in that case RM
    // returned the maximum PCIE speed the GPU is capable of, regardles of what
    // the particular board supported. So we switch to GetPexSpeedAtPState to
    // read the capability from VBIOS instead.
    var boardPcieSpeedCapability = [];
    CHECK_RC(Subdev.Perf.GetPexSpeedAtPState(0, boardPcieSpeedCapability));

    // Need to set PCIE speed change arguments  based on comparing board
    // capability versus the actual link speed ("gen1" or "gen2" should not
    // affect setting the mismatch because they are actually used to compensate
    // for the running the fieldiag on an intentionally mismatching board)
    if (Subdev.PcieLinkSpeed < boardPcieSpeedCapability[0])
        g_PcieSpeedChangeArgs.AllowCoverageHole = true;

    if (g_SeuPcieX8)
    {
        if (typeof(g_ValidSkuArgs2.OverrideSubTests) === "undefined")
        {
            g_ValidSkuArgs2.OverrideSubTests = new Object;
        }
        g_ValidSkuArgs2.OverrideSubTests["PcieLanes"] = 8;
    }
    if (g_SeuPcieLanes > 0)
    {
        if (typeof(g_ValidSkuArgs2.OverrideSubTests) === "undefined")
        {
            g_ValidSkuArgs2.OverrideSubTests = new Object;
        }
        g_ValidSkuArgs2.OverrideSubTests["PcieLanes"] = g_SeuPcieLanes;
    }

    // The function below tries to model a switch statement with one difference:
    // there can be several cases exelwted.
    // Usage: ExelwteBoardSpecificProcs(v, x, doX, y, doY, doDefault);
    // Default part is optional. Fallthrough is modeled by passing an array of
    // values: ExelwteBoardSpecificProcs(v, [x, y], doXY, [y, z], doYZ);
    function ExelwteBoardSpecificProcs(v) {
        var rc = OK;

        var i;
        var caseWasFound = false;
        for (i = 1 ; i < arguments.length - 1; i += 2 )
        {
            var vals = arguments[i] instanceof Array ?
                       arguments[i] : [arguments[i]];
            for (var j = 0; j < vals.length; j++)
            {
                if (v === vals[j])
                {
                    CHECK_RC(arguments[i + 1]()); // execute a case
                    caseWasFound = true;
                }
            }
        }
        if (!caseWasFound && "function" === typeof arguments[i])
        {
            // execute default
            CHECK_RC(arguments[i]());
        }

        return rc;
    }

    // Note that ExelwteBoardSpecificProcs below is grouped into two tiers: the
    // "default" section of the top level has another ExelwteBoardSpecificProcs
    // inside. The purpose of this is to have two different procedures of
    // checking the PEX speed: for Gemini and non-Gemini boards. If you add a
    // Gemini board to this spec, you have to add it to the PEX check section,
    // see comments below where it has to be added. Non-Gemini boards have to be
    // mentioned below only if they have specific board options, the PEX speed
    // check will be carried automatically.
    //
    // If a Gemini board has specific options, add it to the outer
    // ExelwteBoardSpecificProcs, if a non-Gemini board has specific options,
    // add it to the inner ExelwteBoardSpecificProcs.

    CHECK_RC(ExelwteBoardSpecificProcs(boardDef.BoardName,
        // all Gemini boards have to be listed here for proper PEX speed check
        // There are lwrrently no Volta Gemini boards
        function () // default
        {
            var rc = OK;

            // Add non Gemini boards here only if they have board specific
            // options.
            CHECK_RC(ExelwteBoardSpecificProcs(boardDef.BoardName,
                ["PG500-0201", "PG500-0203", "PG503-0201", "PG503-0202",
                 "PG503-0203", "PG503-0204", "PG503-0240", "PG503-0250",
                 "PG504-0200"],
                function ()
                {
                    let testdevice = new TestDevice(Subdev.DevInst);

                    if ("undefined" !== typeof(testdevice.LwLink) && !g_SeuSkipLwLinkTests)
                    {
                        if (null === g_SeuLwLinkMask)
                        {
                            // Verify all LwLink links in test 1 (CheckConfig)
                            g_VerifyLwlMask = (1 << testdevice.LwLink.MaxLinks) - 1;
                        }
                        else
                        {
                            // Verify specific LwLink links in test 1
                            g_VerifyLwlMask = g_SeuLwLinkMask;
                        }
                    }
                }
            ));

            CHECK_RC(ArgCheckLinkWidth(Subdev.DevInst,
                                       0,
                                       g_SeuPcieX8 ? 8 : (g_SeuPcieLanes > 0) ? g_SeuPcieLanes : 16,
                                       g_SeuPcieX8 ? 8 : (g_SeuPcieLanes > 0) ? g_SeuPcieLanes : 16));
            if (g_SeuPcieX8)
            {
                CHECK_RC(ArgCheckPxl(8));
            }

            if (g_SeuPcieLanes > 0)
            {
                CHECK_RC(ArgCheckPxl(g_SeuPcieLanes));
            }


            // Validate the PEX speed at the GPU and allow the "gen1" and "gen2"
            // arguments to affect the validation
            CHECK_RC(ValidatePexSpeed(Subdev,
                                      boardPcieSpeedCapability[0],
                                      Subdev.PcieLinkSpeed,
                                      true));

            // This only applies for non-gemini boards since they have a PLX,
            // and the link at the GPU should always be gen3 capable.  On
            // gemini boards, "gen1" and "gen2" only apply to the handling
            // of the link between the PLX and rootport
            if (g_SeuPcieGen1)
            {
                if ("undefined" === typeof g_ValidSkuArgs2.OverrideSubTests)
                {
                    g_ValidSkuArgs2.OverrideSubTests = new Object;
                }
                g_ValidSkuArgs2.OverrideSubTests["InitGen"] = "Gen1";
                g_Test146Args.LinkSpeedsToTest = 1;
            }
            else if (g_SeuPcieGen2)
            {
                if ("undefined" === typeof g_ValidSkuArgs2.OverrideSubTests)
                {
                    g_ValidSkuArgs2.OverrideSubTests = new Object;
                }
                g_ValidSkuArgs2.OverrideSubTests["InitGen"] = "Gen2";
                g_Test146Args.LinkSpeedsToTest = 3;
            }

            // If the Gen speed is gen 1, then in order to get to this
            // point, it is neccesarry for "gen1" to have been passed
            // on the command line, in which case we should skip link
            // speed checking
            if (!g_SeuPcieGen1)
            {
                // Otherwise, check gen2 if forced or the top speed
                // capability of the GPU if unforced.
                //
                // -check_linkspeed 0 #### - since linkspeed is only
                // checked once before all tests have started running
                // and the first pstate run is P0, only checking for
                // the appropriate link speed is necessary as part of
                // the spec
                CHECK_RC(ArgCheckLinkSpeed(
                    Subdev.DevInst, "0",
                    g_SeuPcieGen2 ? 5000 : boardPcieSpeedCapability[0]));
            }
            else
            {
                // For non gemini boards, if "gen1" was passed then skip
                // the PCIE speed change, since there is theoretically no
                // speed that is possible to switch to in this case
                g_SkipPcieSpeedChange = true;
            }

            return rc;
        }
    ));

    if (monitorGpio9)
    {
        CHECK_RC(Subdev.Gpio.StopErrorCounter());
        CHECK_RC(Subdev.Gpio.SetIntrNotification(9, GpioConst.FALLING, true));
        CHECK_RC(Subdev.Gpio.SetActivityLimit(9, GpioConst.FALLING, 0, true));
        CHECK_RC(Subdev.Gpio.StartErrorCounter());
    }

    return rc;
}

function HandleGpuPostInitArgs629()
{
    var rc = OK;
    var finalRc = OK;
    var TeslaBoards = new Array();

    var targetSubdev = null;
    if ("undefined" !== typeof g_SeuPciid && null !== g_SeuPciid)
    {
        targetSubdev = GpuDevMgr.GetFirstGpu();
        if (targetSubdev === null)
        {
            g_SeuValidationErrorStrings.push(Out.Sprintf(
                "Device %02x:%02x.%x not found.  Please confirm the device " +
                "number, card insertion in the\n" +
                "PCI Express slot, and power connections to the Tesla card.\n",
                g_SeuPciidBus, g_SeuPciidDevice, g_SeuPciidFunction));
            return RC.BAD_COMMAND_LINE_ARGUMENT;
        }
    }
    else if ((g_SeuDevice !== null) && (g_SeuDevice >= GpuDevMgr.NumDevices))
    {
        g_SeuValidationErrorStrings.push(Out.Sprintf(
                   "Device %d not found.  Please confirm the device number, " +
                   "card insertion in the\n" +
                   "PCI Express slot, and power connections to the Tesla card.\n",
                   g_SeuDevice));
        return RC.BAD_COMMAND_LINE_ARGUMENT;
    }

    // Do the normal post gpu initialization
    CHECK_RC(HandleGpuPostInitArgs());

    // Validate that the boards that fieldiag is being run on are
    // compatible with the diag
    g_SkuCheckOrInitFail = true;
    if (targetSubdev !== null || g_SeuDevice !== null)
    {
        var subdev = targetSubdev !== null
                         ? targetSubdev
                         : new GpuSubdevice(g_SeuDevice, 0);

        g_SerialNumber = subdev.BoardSerialNumber;

        // Exit if validation does not succeed (since matching a board
        // will definitely not succeed and the corrective actions are already
        // printed above)
        CHECK_RC(ValidatePreBoardName(subdev));

        if (IsTeslaBoard(GetBoardDef(subdev)))
        {
            TeslaBoards.push(subdev.DevInst);
        }
        else
        {
            if (targetSubdev !== null)
            {
                g_SeuValidationErrorStrings.push(Out.Sprintf(
                    "Device %02x:%02x.%x: The card under test does not " +
                    "match a valid Tesla product\n" +
                    "           configuration.\n",
                    g_SeuPciidBus, g_SeuPciidDevice, g_SeuPciidFunction));
            }
            else
            {
                g_SeuValidationErrorStrings.push(Out.Sprintf(
                    "GPU %d:0 : The card under test does not match a " +
                    "valid Tesla product\n" +
                    "          configuration.\n",
                    subdev.ParentDevice.DeviceInst));
            }
            return RC.SOFTWARE_ERROR;
        }
    }
    else
    {
        var firstIter = true;
        for (var subdev = GpuDevMgr.GetFirstGpu();
             subdev != null; subdev = GpuDevMgr.GetNextGpu(subdev))
        {
            if (firstIter)
            {
                g_SerialNumber = subdev.BoardSerialNumber;
                firstIter = false;
            }

            rc = ValidatePreBoardName(subdev);
            if (finalRc == OK)
                finalRc = rc;

            // If validation succeeded, then go ahead and validate the board
            // is a valid tesla board
            if (rc == OK)
            {
                if (IsTeslaBoard(GetBoardDef(subdev)))
                {
                    TeslaBoards.push(subdev.DevInst);
                }
                else
                {
                    g_SeuValidationErrorStrings.push(Out.Sprintf(
                               "GPU %d:0 : The card under test does not " +
                               "match a valid Tesla product\n" +
                               "          configuration.\n",
                               subdev.ParentDevice.DeviceInst));
                    if (finalRc == OK)
                        finalRc = RC.SOFTWARE_ERROR;
                }
            }
        }
    }
    // We continue processing here even if we know from the code above that
    // there are not supported boards in the system and we will return an error
    // at the end. This intentional behavior has its aim to report all possible
    // errors, not waiting while the user removes not supported boards.


    // The temperature arguments need to be processed for all devices prior
    // to validating the configuration (reading the temperature for some
    // configurations requires that the command line arguments be set
    // correctly)
    for (var i = 0; i < TeslaBoards.length; i++)
    {
        CHECK_RC(SetTempArgs(TeslaBoards[i]));
    }

    // Need to reinit the thermal block (normally called during
    // HandleGpuPostInitArgs) since changing the temperature options requires
    // re-initialization
    CHECK_RC(InitThermal());

    var bQuadroFused = true;
    var bSkipExtInThermalSanity = false;

    // Validate tesla board specific functionality
    for (var i = 0; i < TeslaBoards.length; i++)
    {
        var subdev = new GpuSubdevice(new TestDevice(TeslaBoards[i]));
        var boardDef = GetBoardDef(subdev);

        // Ensure the lwdqro bit is the same across all GPUs under test
        if (i == 0)
        {
            bQuadroFused = subdev.ParentDevice.CheckCapsBit(GraphicsCaps.prototype.QUADRO_GENERIC);
        }
        else if (bQuadroFused != subdev.ParentDevice.CheckCapsBit(GraphicsCaps.prototype.QUADRO_GENERIC))
        {
            g_SeuValidationErrorStrings.push(Out.Sprintf(
                       "Error: Mixed GPU configuration detected, " +
                       "conlwrrent testing is not possible.\n" +
                       "Please use \"device=n\" to test " +
                       "each device individually\n"));
            if (finalRc == OK)
                finalRc = RC.SOFTWARE_ERROR;
        }

        var tempOption = GetTempOption(GetBoardDef(subdev));

        try
        {
            switch (tempOption)
            {
                default:
                case "":
                    subdev.Thermal.ChipTempViaExt;
                    break;
                case "int":
                    subdev.Thermal.ChipTempViaInt;
                    break;
                case "disabled":
                    g_SkipThermalSanity = true;
                    break;
            }
        }
        catch (exception)
        {
            if (typeof(exception) === "number")
            {
                g_SeuValidationErrorStrings.push(Out.Sprintf(
                            "GPU %d:0 : The GPU temperature was not accessible. " +
                            "Please check the\n" +
                            "          documentation for configuration options " +
                            "for your product or\n" +
                            "          use \"gpu_temp=disabled\".\n",
                            subdev.ParentDevice.DeviceInst));
                if (finalRc == OK)
                    finalRc = RC.BAD_COMMAND_LINE_ARGUMENT;
            }
            else
                throw new Error("Unknown exception found");
        }
    }

    if (finalRc == OK)
    {
        if (bSkipExtInThermalSanity)
        {
            for (var i = 0; i < TeslaBoards.length; i++)
            {
                CHECK_RC(ArgTestArg(TeslaBoards[i], "CheckThermalSanity", "SkipExternal", "1"));
            }
        }

        for (var i = 0; i < TeslaBoards.length; i++)
        {
            var subdev = new GpuSubdevice(new TestDevice(TeslaBoards[i]));

            SetBoardSpecificOptions(subdev);

            if (subdev.Power.NumPowerSensors != 0)
            {
                CHECK_RC(ArgBgPowerLoggerFlush(TeslaBoards[i], 10000, 1000));
            }
        }
        g_SkuCheckOrInitFail = false;
    }

    return finalRc;
}

function VerifyGomMode629()
{
    var rc = VerifyGomMode();
    if (OK != rc)
    {
        g_SeuValidationErrorStrings.push(Out.Sprintf(
                "Multiple GPU Operational Modes (GOM) detected preventing " +
                "conlwrrent testing.\nPlease test each GPU individually " +
                "using the 'device = n' command line option\nor update " +
                "all Tesla boards with the same GOM setting.\n",
                g_SeuDevice));
    }
    return rc;
}

function VerifyGomModeWrapper()
{
    return VerifyGomMode629();
}

//------------------------------------------------------------------------------
// Print any specific error messages due to GPU initialization failure. Used
// only in stealth MODS builds by default. Note that PrintGpuInitError is called
// unconditionally even if there is no error and thus retval is equal to RC.OK.
//
function PrintGpuInitError(retval)
{
    if (OK !== retval)
    {
        g_SkuCheckOrInitFail = true;
    }
    if ((retval == RC.ONLY_OBSOLETE_DEVICES_FOUND) ||
        (retval == RC.PCI_DEVICE_NOT_FOUND))
    {
        Out.Printf(Out.PriAlways,
                   "No Tesla products were detected. Please confirm the " +
                   "device number, card\n" +
                   "insertion in the PCI Express slot, and power " +
                   "connections to the card\n" +
                   "under test.\n");
        g_SeuRetestBanner = true;
    }
    else if (retval == RC.CANNOT_HOOK_INTERRUPT)
    {
        Out.Printf(Out.PriAlways,
                  "The Tesla cannot hook a necessary hardware interrupt.\n" +
                  "Please run with the option \"poll_interrupts\".\n");
        g_SeuRetestBanner = true;
    }
    else if (retval == RC.WAS_NOT_INITIALIZED)
    {
        Out.Printf(Out.PriAlways,
                  "The Tesla card does not have the necessary external " +
                  "power cables attached.\n" +
                  "Please check the external power cables and then retest.\n");
        g_SeuRetestBanner = true;
    }
    return retval;
}

// This function is called after CheckConfig test. It has to be specified as
// PostRunCallback CheckConfig test argument. We check whether LwLink links
// connectivity test passed. If not, we print out not connected links and
// flag that a retest message has to be printed.
function CheckConfigPostRun(retval)
{
    var rc = OK;

    if (RC.LWLINK_BUS_ERROR === retval)
    {
        var subdev = this.BoundGpuSubdevice;

        var retestMessage =
            "GPU " + subdev.ParentDevice.DeviceInst +
            ":0 : LWLINK connectivity was not detected for link(s): ";
        if ("undefined" !== typeof this.inactiveLinks &&
            null !== this.inactiveLinks &&
            this.inactiveLinks instanceof Array)
        {
            var firstIter = true;
            for (var i in this.inactiveLinks)
            {
                if (firstIter)
                {
                    firstIter = false;
                }
                else
                {
                    retestMessage += ", ";
                }
                retestMessage += this.inactiveLinks[i];
            }
        }
        else
        {
                retestMessage += "unknown";
        }

        retestMessage += ".\n" +
            "If the system does not have full LWLINK connectivity, please use command lines\n" +
            "\"skip_lwlink\" or \"lwlink_mask=0x?\" to specify the LWLINK topology. Otherwise,\n" +
            "please try reseating the card to confirm full electrical connectivity of the\n" +
            "interface and retest.\n"

        Out.Printf(Out.PriAlways, retestMessage);

        g_SeuRetestBanner = true;
        g_ContinueModsOnError = false;
    }

    return rc;
}

// Point the post init callback to the 629 version so that it can sanity check
// the attached boards and ensure that they are valid for use with the fieldiag
Gpu.PostInitCallback = "HandleGpuPostInitArgs629";
