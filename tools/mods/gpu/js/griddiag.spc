/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2014,2017,2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/griddiag.spc#36 $");

// Set the test mode here as well.  The test mode can be used to filter prints
// in the informational header and needs to be set before addModsGpuTests is
// called

var g_SeuTestMode = (1 << 2); // TM_MFG_BOARD (cannot include mods.h due to how
                              // preprocessing is done in official builds)

g_ShortInflectPtList =
[
    {PStateNum: 0, LocationStr: "max", Clks: [{Domain:Gpu.ClkM, FreqHz:2700000000}]},
];

g_LongInflectPtList =
[
    {PStateNum: 0, LocationStr: "max", Clks: [{Domain:Gpu.ClkM, FreqHz:2700000000}]},
    {PStateNum: 0, LocationStr: "nom", Clks: [{Domain:Gpu.ClkM, FreqHz:2700000000}]},
    {PStateNum: 8, LocationStr: "nom"},
];

g_SpecArgs = "-dump_inforom -blink_lights";
g_SpecArgs += " -test_devid 0x11bf -verify_num_gpus 8";
g_SpecArgs += " -timeout_ms 180000 -rc_timeout_sec 300";
g_SpecArgs += " -pwr_cap 0x1 -pstate_soft"

// PEX arguments
// -corr_error_tol 0xFFFFFFFF is redundant with -perlink
g_SpecArgs += " -extra_pexcheck -pex_verbose 0xC -corr_error_tol 0xFFFFFFFF";
g_SpecArgs += " -pex_nak_rcvd_tol 200 -pex_nak_sent_tol 200 -pex_l0s_tol 200";
g_SpecArgs += " -pex_line_error_tol 200 -pex_crc_tol 200 -perlink_corr_error 0 200 200";
g_SpecArgs += " -perlink_corr_error 1 200 200 -check_linkwidth 0 16 16 -check_linkwidth 1 16 16";
g_SpecArgs += " -check_linkspeed 0 8000 -check_linkspeed 1 8000"

// Temperature and power arguments
g_SpecArgs += " -bg_print_ms 60000";
g_SpecArgs += " -bg_flush";
g_SpecArgs += " -bg_int_temp 5000 -itmp_range 10 90 -no_thermal_slowdown";
g_SpecArgs += " -bg_power 5000 -pwr_range_mw 1 5 75000 -pwr_range_mw 2 5 106000";
g_SpecArgs += " -max_pwr_range 0x1 4000 75000 -max_pwr_range 0x2 4000 106000";

if (g_SeuDevice == -1)
{
    g_SpecArgs += " -conlwrrent_devices_sync";
}

if (g_SeuTestType == "long")
{
    g_SpecArgs += " -null_display -un_supp_req_tol 200 -loops 8";
}
else if (g_SeuTestType == "longdisplay")
{
    g_SpecArgs += " -loops 25";
}

function ValidatePex16(Subdev, Widths)
{
    var rc = OK;
    if ((Widths[0] != 16) || (Widths[1] != 16))
    {
        g_SeuValidationErrorStrings.push(Out.Sprintf(
                   "GPU %d:0 : PCI Express x8 width was detected. Check " +
                   "board connections.\n",
                   Subdev.ParentDevice.DeviceInst));
        rc = RC.UNSUPPORTED_SYSTEM_CONFIG;
    }
    return rc;
}

function ValidatePexSpeed(Subdev, ExpectedSpeed, ActualSpeed)
{
    var rc = OK;

    if (ExpectedSpeed == ActualSpeed)
        return OK;

    var genFound = (ActualSpeed == 5000) ? 2 : 1;
    var genExpected = (ExpectedSpeed == 5000) ? 2 : 3;

    g_SeuValidationErrorStrings.push(Out.Sprintf(
                "GPU %d:0 : PCI Express Gen %d speed was detected " +
                "(Gen %d expected).\n" +
                "          Check board connections.\n",
                Subdev.ParentDevice.DeviceInst,
                genFound, genExpected));
    return RC.UNSUPPORTED_SYSTEM_CONFIG;
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
    var linkWidths = new Array;

    rc = ValidatePexSpeed(subdev, 8000, subdev.PcieLinkSpeed);
    if (finalRc == OK)
        finalRc = rc;

    CHECK_RC(subdev.GetPcieLinkWidths(linkWidths));
    rc = ValidatePex16(subdev, linkWidths);
    if (finalRc == OK)
        finalRc = rc;

    return finalRc;
}

function IsGridBoard(BoardDef)
{
    if (BoardDef == null)
    {
        return false;
    }

    switch(BoardDef.BoardName)
    {
        // GK104 boards
        case "P2055-302-G2": // devid = 0x11BF
        case "P2055-302-G1": // devid = 0x11BF
            return true;
    }
    return false;
}

function HandleGpuPostInitArgsGrid()
{
    var rc = OK;
    var finalRc = OK;

    if ((g_SeuDevice != -1) && (g_SeuDevice >= GpuDevMgr.NumDevices))
    {
        g_SeuValidationErrorStrings.push(Out.Sprintf(
                   "Device %d not found.  Please confirm the device number, " +
                   "card insertion in the\n" +
                   "PCI Express slot, and power connections to the Grid card.\n",
                   g_SeuDevice));
        return RC.BAD_COMMAND_LINE_ARGUMENT;
    }

    if (OK != VerifyNumGpus())
    {
        g_SeuValidationErrorStrings.push(Out.Sprintf(
                   "Expected %d GPUs, but only %d found.  Please check all " +
                   "cards are seated\n" +
                   "firmly in their PCI Express slots, and power connections " +
                   "to all cards.\n",
                   g_VerifyNumGpus, GpuDevMgr.NumGpus));
        return RC.BAD_COMMAND_LINE_ARGUMENT;
    }

    // Do the normal post gpu initialization
    CHECK_RC(HandleGpuPostInitArgs());

    // Validate that the boards that griddiag is being run on are
    // compatible with the diag
    if (g_SeuDevice != -1)
    {
        var subdev = new GpuSubdevice(g_SeuDevice, 0);

        // Exit if validation does not succeed (since matching a board
        // will definitely not succeed and the corrective actions are already
        // printed above)
        CHECK_RC(ValidatePreBoardName(subdev));

        if (!IsGridBoard(GetBoardDef(subdev)))
        {
            g_SeuValidationErrorStrings.push(Out.Sprintf(
                       "GPU %d:0 : The card under test does not match a " +
                       "valid Grid product\n" +
                       "          configuration.\n",
                       g_SeuDevice));
            return RC.UNSUPPORTED_SYSTEM_CONFIG;
        }
    }
    else
    {
        for (var subdev = GpuDevMgr.GetFirstGpu();
             subdev != null; subdev = GpuDevMgr.GetNextGpu(subdev))
        {
            rc = ValidatePreBoardName(subdev);
            if (finalRc == OK)
                finalRc = rc;

            // If validation succeeded, then go ahead and validate the board
            // is a valid grid board
            if (rc == OK)
            {
                if (!IsGridBoard(GetBoardDef(subdev)))
                {
                    g_SeuValidationErrorStrings.push(Out.Sprintf(
                               "GPU %d:0 : The card under test does not " +
                               "match a valid Grid product\n" +
                               "          configuration.\n",
                               subdev.ParentDevice.DeviceInst));
                    if (finalRc == OK)
                        finalRc = RC.UNSUPPORTED_SYSTEM_CONFIG;
                }
            }
        }
    }

    return finalRc;
}

//------------------------------------------------------------------------------
function addModsGpuTests(testlist, InflectPtList)
{
    testlist.TestMode = (1<<2); // TM_MFG_BOARD

    InflectPtList = g_ShortInflectPtList;
    var addTestsFunc = addShortTests;
    var goldenGeneration = false;
    if ((g_SeuTestType == "long") || (g_SeuTestType == "longdisplay"))
    {
        InflectPtList = g_LongInflectPtList;
        addTestsFunc = (g_SeuTestType == "long") ? addLongTests : addLongDisplayTests;
    }

    if (Golden.Store == Golden.Action)
    {
        // Do just one pstate for golden generation.
        InflectPtList = InflectPtList.slice(0,1);
        goldenGeneration = true;
    }

    for (var i = 0; i < InflectPtList.length; i++)
    {

        var InflectPt = InflectPtList[i];

        testlist.AddTest("SetPState", {PerfTable: InflectPtList, PerfTableIdx: i})
        var isLast = (InflectPtList.length-1 == i);

        var PStateNum = 0;
        if (InflectPt != null)
            PStateNum = InflectPt.PStateNum;

        addTestsFunc(testlist, i, PStateNum, isLast, goldenGeneration);
    }

    if ((g_SeuTestType == "long") && !goldenGeneration)
    {
        addExtraLongTests(testlist);
    }

    // Override test duration of optional tests (must use -add to run these):
    testlist.AddTestsArgs([
         ["LwdaMatsTest",       setIterations(50)]
        ,["LwdaColumnTest",     setIterations(5)]
        ,["NewWfMats",          {"CpuTestBytes" : 1*1024*1024}]
        ,["NewWfMatsShort",     {"CpuTestBytes" : 1*1024*1024}]
        ,["NewWfMatsBusTest",   {"CpuTestBytes" : 1*1024*1024}]
        ]);
}

function addExtraLongTests(testlist)
{
    testlist.AddTests([
         [ "SetPState", {InfPts : { PStateNum : 0, LocationStr : "max" }}]
        ,[ "GLCombustTest", {"Watts": 135, "LoopMs": 5*60*1000,"EndLoopPeriodMs": 60000}]
    ]);

    let PerfSpec = testlist.CloneEmptyTestList();
    PerfSpec.AddTestArgs("PerfSwitch",    { IgnorePowerLimits : true });
    PerfSpec.AddTestArgs("CheckDma",      { NoP2P : true });
    PerfSpec.AddTests([
         ["FastMatsTest",       { Coverage: 80 }]
        ,["WfMatsMedium",       { CpuTestBytes: 6 * 1024*1024,
                                  MaxFbMb     : 64 }]
        ,["NewWfMatsNarrow",    { MaxFbMb: 2, InnerLoops: 16 }]
        ,["Random2dFb",         setLoops(10*1000)]
        ,"Random2dNc"
        ,"LineTest"
        ,["GlrA8R8G8B8",        scaleLoops(40*20, 20, 0.5, 0)]
        ,["GlrFsaa4x",          scaleLoops(40*20, 20, 0.5, 0)]
        ,["GlrFsaa8x",          scaleLoops(40*20, 20, 0.5, 0)]
        ,["ElpgGraphicsStress", { LoopMs: 150 }]
        ,["DeepIdleStress",     { LoopMs: 200 }]
        ,["GLStress",           { LoopMs: 3000 }]
        ,["GLStressPulse",      { LoopMs: 1500 }]
        ,["NewLwdaMatsTest",    { Iterations: 6,
                                  MaxFbMb:    128 }]
        ,["LwdaRandom",         setLoops(30*75)]
        ]);

    if ("undefined" !== typeof PerfSpec.TestsToRun)
    {
        for (var i = 0; i < PerfSpec.TestsToRun.length; i++)
        {
            testlist.AddTest("PerfSwitch", {FgTestNum  : PerfSpec.TestsToRun[i].Number,
                                        FgTestArgs : PerfSpec.TestsToRun[i].TestArgs});
        }
    }
}

function shortGlrTestArgs(numFrames, pStateIdx, goldenGeneration)
{
    if (0 == pStateIdx)
    {
        // A frame is 20 loops.
        // Goldens end after 'numFrames' frames.
        // At first PerfPoint (P0.max) run first half of sequence.
        return scaleLoops(numFrames*20, 20, 0.5, 0, goldenGeneration);
    }
    else
    {
        // All subsequent PerfPoints run one frame starting where the previous
        // PerfPoint left off.
        // Wrap back to frame 0 if we have too many PerfPoints.
        return scaleLoops(numFrames*20, 20, 1/numFrames, pStateIdx, goldenGeneration);
    }
}

function addShortTests
(
    testlist          // (TestList)
    ,pStateIdx        // (integer) how many pstates we've tested already
    ,pStateNr         // (integer) actual pState number
    ,isLast           // (bool) if true, add one-time late tests
    ,goldenGeneration // (bool) if true, goldens are being generated
)
{
    var isFirst = (0 == pStateIdx);
    var glrTestArgs = shortGlrTestArgs(40, pStateIdx, goldenGeneration);

    if (isFirst)
    {
        testlist.AddTests([
             "CheckConfig"
            ,"CheckInfoROM"
            ,"ValidSkuCheck"
            ,"ValidSkuCheck2"
            ,"CheckThermalSanity"
            ,"CheckFanSanity"
            ,"CheckPwrSensors"
            ,"GpuPllLockTest"
            ]);
    }

    testlist.AddTests([
         ["FastMatsTest", {Coverage: (isFirst ? 100 : 4)}]
        ,["HostBusTest",    setLoops(500)]
        ,"SecTest"
        ,"PcieSpeedChange"
        ]);

    if (isFirst)
    {
        testlist.AddTests([
            "PerfSwitch"
            ]);
    }

    testlist.AddTests([
         ["Class07c",    setLoops(5)]
        ,["Random2dFb",     setLoops(10*200)]
        ,["Class07a",   setLoops(10)]
        ,"Class1774"
        ,"Class417a"
        ,"AppleGL"
        ,["GlrA8R8G8B8",   glrTestArgs]
        ,["GlrFsaa2x",     glrTestArgs]
        ,["GlrFsaa4x",     glrTestArgs]
        ,["GlrA8R8G8B8Sys",glrTestArgs]
        ,["Elpg",         setLoops(16)]
        ,["GLRandomCtxSw", glrTestArgs]
        ,["GLCombustTest", {"Watts": 135, "LoopMs": 5*60*1000,"EndLoopPeriodMs": 60000}]
        ]);

    testlist.AddTests([
         ["NewLwdaMatsTest", {Iterations: (isFirst ? 10 : 1),
                             MaxFbMb: (isFirst ? 0 : 128)}]
        ,["LwdaGrfTest",    setLoops(16 * (isFirst ? 8 : ((pStateNr > 11) ? 1 : 4)))]
        ,["DPStressTest",   setLoops(isFirst ? 100 : 6)]
        ,["LwdaStress2",    setLoopMs(1000)]
        ,["LwdaRandom",     setLoops(15*75)]
        ,["LwdaL2Mats",     setIterations((pStateNr > 11) ? 100 : 1000)]
        ]);

    if (isFirst)
    {
        testlist.AddTests([
            "IntAzaliaLoopback"
            ]);
    }

    testlist.AddTests([
        ["MMERandomTest",       setLoops(100 * (isFirst ? 10 : 2))]
        ]);

    if (isLast)
    {
        testlist.AddTests([
             "CheckOvertemp"
            ,"CheckFbCalib"
            ,["KFuseSanity",    setLoops(1)]
            ,"Optimus"
            ]);
    }
}

function addLongTests
(
    testlist          // (TestList)
    ,pStateIdx    // (integer) how many pstates we've tested already
    ,pStateNr     // (integer) actual pState number
    ,isLast       // (bool) if true, add one-time late tests
    ,goldenGeneration // (bool) if true, goldens are being generated
)
{
    var isFirst = (0 == pStateIdx);
    var glrTestArgs = shortGlrTestArgs(40, pStateIdx, goldenGeneration);
    var smallerGlrTestArgs = shortGlrTestArgs(20, pStateIdx, goldenGeneration);

    if (isFirst)
    {
        testlist.AddTests([
             "CheckConfig"
            ,"CheckInfoROM"
            ,"ValidSkuCheck"
            ,"ValidSkuCheck2"
            ,"CheckThermalSanity"
            ,"CheckFanSanity"
            ,"CheckPwrSensors"
            ,"GpuPllLockTest"
            ]);
    }

    testlist.AddTests([
         ["MatsTest", {Coverage: (isFirst ? 1 : 0.015)}]
        ,["FastMatsTest", {Coverage: (isFirst ? 100 : 4)}]
        ,["WfMatsMedium", {CpuTestBytes: (isFirst ? 6*1024*1024 : 512*1024), MaxFbMb: (isFirst ? 0 : 64)}]
        ,["NewWfMatsNarrow", {MaxFbMb: (isFirst ? 5 : 2), InnerLoops: (isFirst ? 32 : 4)}]
        ,"SorLoopback"
        ,"CpyEngTest"
        ,"I2MTest"
        ,["HostBusTest",    setLoops(500)]
        ,["CheckDma",     setLoops(5*200)]
        ,"SecTest"
        ,"PcieSpeedChange"
        ]);

    if (isFirst)
    {
        testlist.AddTests([
            "PerfSwitch"
            ]);
    }

    testlist.AddTests([
         ["Class07c",    setLoops(5)]
        ,["Class05f",   setLoops(1000)]
        ,["Random2dFb", setLoops(10 * (goldenGeneration ? 1000 : 200)) ] // Need more loops for the PerfSwitch version
        ,"Random2dNc"
        ,"LineTest"
        ,["Class07a",   setLoops(10)]
        ,"Class1774"
        ,"Class4176"
        ,"Class417a"
        ,"Class4075"
        ,"AppleGL"
        ,["GlrA8R8G8B8",   glrTestArgs]
        ,["GlrFsaa2x",     glrTestArgs]
        ,["GlrFsaa4x",     glrTestArgs]
        ,["GlrMrtRgbU",    glrTestArgs]
        ,["GlrMrtRgbF",    glrTestArgs]
        ,["GlrY8",         glrTestArgs]
        ,["GlrR5G6B5",     glrTestArgs]
        ,["GlrFsaa8x",     glrTestArgs]
        ,["GlrA8R8G8B8Sys",glrTestArgs]
        ,["GlrFsaa4v4",    smallerGlrTestArgs]
        ,["GlrFsaa8v8",    smallerGlrTestArgs]
        ,["GlrFsaa8v24",   smallerGlrTestArgs]
        ,["Elpg",         setLoops(16)]
        ,["ElpgGraphicsStress", {LoopMs: (isFirst ? 250 : 100)}]
        ,["DeepIdleStress",     {LoopMs: 150}]
        ,["GLStress",      { "LoopMs" : (isFirst ? 5000 : 1000)}]
        ,["GLStressPulse", { "LoopMs" : (isFirst ? 2000 : 250)}]
        ,["GLRandomCtxSw", glrTestArgs]
        ,["GlrLayered",    glrTestArgs]
        ]);

    testlist.AddTests([
         ["NewLwdaMatsTest", {Iterations: (isFirst ? 10 : 1),
                             MaxFbMb: (isFirst ? 0 : 128)}]
        ,["LwdaGrfTest",    setLoops(16 * (isFirst ? 8 : ((pStateNr > 11) ? 1 : 4)))]
        ,["DPStressTest",   setLoops(isFirst ? 100 : 6)]
        ,["LwdaStress2",    setLoopMs(1000)]
        ,["LwdaRandom",     setLoops(75 * (goldenGeneration ? 30 : 15)) ]  // Need more loops for the PerfSwitch version
        ,["LwdaL2Mats",     setIterations((pStateNr > 11) ? 100 : 1000)]
        ]);

    if (isFirst)
    {
        testlist.AddTests([
            "IntAzaliaLoopback"
            ]);
    }

    testlist.AddTests([
         ["MMERandomTest",       setLoops(100 * (isFirst ? 10 : 2))]
        ,["Bar1RemapperTest",   setLoops(5 * (isFirst ? 10 : 2))]
        ,["EccFbTest",          setLoops(7000)]
        ,["EccL2Test",          setLoops(20000)]
        ,["EccSMTest",          setLoops(32)]
        ]);

    if (isLast)
    {
        testlist.AddTests([
             "CheckOvertemp"
            ,"CheckFbCalib"
            ,["KFuseSanity",    setLoops(1)]
            ,"Optimus"
            ]);
    }
}


function addLongDisplayTests
(
    testlist          // (TestList)
    ,pStateIdx    // (integer) how many pstates we've tested already
    ,pStateNr     // (integer) actual pState number
    ,isLast       // (bool) if true, add one-time late tests
    ,goldenGeneration // (bool) if true, goldens are being generated
)
{
    var isFirst = (0 == pStateIdx);
    testlist.AddTests([
         ["EvoLwrs", {TestConfiguration: {Loops: (isFirst ? 5 : 1)}}]
        ,["EvoOvrl",    {"TestConfiguration" : { "Loops" : (isFirst ? 10 : 1) }}]
        ,"LwDisplayRandom"
        ,"LwDisplayHtol"
        ]);
}


//------------------------------------------------------------------------------
// Print any specific error messages due to GPU initialization failure.  Used
// only in stealth MODS builds by default.
//
function PrintGpuInitError(retval)
{
    if ((retval == RC.ONLY_OBSOLETE_DEVICES_FOUND) ||
        (retval == RC.PCI_DEVICE_NOT_FOUND))
    {
        Out.Printf(Out.PriAlways,
                   "No Grid products were detected. Please confirm the " +
                   "device number, card\n" +
                   "insertion in the PCI Express slot, and power " +
                   "connections to the card\n" +
                   "under test.\n");
    }
    else if (retval == RC.WAS_NOT_INITIALIZED)
    {
        Out.Printf(Out.PriAlways,
                  "The Grid card does not have the necessary external " +
                  "power cables attached.\n" +
                  "Please check the external power cables and then retest.\n");
    }
    return retval;
}

// Point the post init callback to the grid version so that it can sanity check
// the attached boards and ensure that they are valid for use with the griddiag
Gpu.PostInitCallback = "HandleGpuPostInitArgsGrid";
