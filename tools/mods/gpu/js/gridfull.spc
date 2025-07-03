/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/gridfull.spc#25 $");

//***************************************************************************
// Global argument setup
//***************************************************************************
g_SpecArgs  = " -edc_tol 100 -no_thermal_slowdown";
g_SpecArgs += " -corr_error_tol 0xFFFFFFFF -blink_lights -timeout_ms 120000";
g_SpecArgs += " -perlink_corr_error 0 255 255 -perlink_corr_error 1 255 255";
g_SpecArgs += " -edc_verbose 0x2 -pex_verbose 0xC -extra_pexcheck";
g_SpecArgs += " -pex_line_error_tol 255 -pex_crc_tol 255 -itmp_range -10 105";
g_SpecArgs += " -pex_nak_rcvd_tol 255 -pex_nak_sent_tol 255 -pex_l0s_tol 255";
g_SpecArgs += " -dump_inforom -conlwrrent_devices -conlwrrent_devices_sync";
g_SpecArgs += " -bg_print_ms 60000 -bg_flush";
g_SpecArgs += " -bg_power 5000 -bg_int_temp 5000";

var g_SeuTestMode = (1<<2); // TM_MFG_BOARD

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

    if (!g_SeuPcieX8)
    {
        CHECK_RC(subdev.GetPcieLinkWidths(linkWidths));
        rc = ValidatePex16(subdev, linkWidths);
        if (finalRc == OK)
            finalRc = rc;
    }

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
        // GK107 boards
        case "P2400-0000"  : // devid = 0x0FEF
        case "P2400-0002"  : // devid = 0x0FEF
        case "P2401-0502"  : // devid = 0x0FF2
        // GK104 boards
        case "P2055-0050"  : // devid = 0x118A
        case "P2055-0052"  : // devid = 0x118A
        case "P2055-0550"  : // devid = 0x11BF
        case "P2055-0552"  : // devid = 0x11BF
        case "P2055-302-G2": // devid = 0x11BF
        case "P2055-302-G1": // devid = 0x11BF
            return true;
    }
    return false;
}

function HandleGpuPostInitArgsGridFull()
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
function addModsGpuTests(spec, InflectPtList)
{
    spec.TestMode = (1<<2); // TM_MFG_BOARD
    var goldenGeneration = false;

    if (Golden.Store == Golden.Action)
    {
        // Do just one pstate for golden generation.
        InflectPtList = InflectPtList.slice(0,1);
        goldenGeneration = true;
    }

    for (var i = 0; i < InflectPtList.length; i++)
    {
        var InflectPt = InflectPtList[i];

        spec.AddTest("SetPState", {PerfTable: InflectPtList, PerfTableIdx: i})
            var isLast = (InflectPtList.length - 1 == i);

        var PStateNum = 0;
        if (InflectPt != null)
            PStateNum = InflectPt.PStateNum;

        addBoardMfgTestsOnePState(spec, i, PStateNum, isLast, goldenGeneration);
    }
}

//------------------------------------------------------------------------------
function addBoardMfgTestsOnePState
(
    spec          // (TestList)
    ,pStateIdx    // (integer) how many pstates we've tested already
    ,pStateNr     // (integer) actual pState number
    ,isLast       // (bool) if true, add one-time late tests
    ,goldenGeneration // (bool) if true, goldens are being generated
)
{
    var isFirst = (0 == pStateIdx);

    if (isFirst)
    {
        // A frame is 20 loops.
        // Goldens end after 30 frames.
        // At first PerfPoint (P0.max) run frames 0 to 14.
        glrTestArgs = scaleLoops(30*20, 20, 15/30, 0, goldenGeneration);
    }
    else
    {
        // All subsequent PerfPoints run one frame starting with 16.
        // Wrap back to frame 0 if we have too many PerfPoints.
        glrTestArgs = scaleLoops(30*20, 20, 1/30, pStateIdx, goldenGeneration);
    }

    if (isFirst)
    {
        spec.AddTests([
            "CheckConfig"
            ,"CheckInfoROM"
            ,["FuseRdCheck",    setLoops(100)]
            ,"ValidSkuCheck"
            ,"ValidSkuCheck2"
            ,"CheckThermalSanity"
            ,"CheckFanSanity"
            ,"CheckPrimarion"
            ,"CheckPwrSensors"
            ,"DevIDCheck"
            ,"CheckExternalDPs"
            ,"CheckUController"
            ,"GpuPllLockTest"
            ]);
    }

    spec.AddTests([
        ["MatsTest", {Coverage: (isFirst ? 1 : 0.015)}]
        ,["FastMatsTest", {Coverage: (isFirst ? 100 : 4)}]
        ,["WfMatsMedium", {CpuTestBytes: (isFirst ? 6*1024*1024 : 512*1024),
                           MaxFbMb: (isFirst ? 0 : 64)}]
        ,["NewWfMatsNarrow", {MaxFbMb: (isFirst ? 5 : 2), InnerLoops: (isFirst ? 32 : 4)}]
        ,"CpyEngTest"
        ,"I2MTest"
        ,["HostBusTest",    setLoops(500)]
        ,["CheckDma",     setLoops(5*200)]
        ,"Class74C1"
        ,"SecTest"
        ,"PcieSpeedChange"
        ]);

    if (isFirst)
    {
        spec.AddTests([
             "PerfSwitch"
            ,"Ventura"
            ]);
    }

    spec.AddTests([
        ["Class07c",    setLoops(5)]
        ,["Class05f",   setLoops(1000)]
        ,["Random2dFb",     setLoops(10*200)]
        ,"Random2dNc"
        ,"LineTest"
        ,["Class07a",   setLoops(10)]
        ,"Class1774"
        ,"Class4176"
        ,"Class417a"
        ,"Class4075"
        ]);
    spec.AddTestsWithParallelGroup(PG_DISPLAY, [
        ["EvoOvrl",        {"TestConfiguration" : { "Loops" : (isFirst ? 10 : 1) }}]
        ],
        {"display":true});
    spec.AddTestsWithParallelGroup(PG_GRAPHICS, [
        ["GlrA8R8G8B8",    glrTestArgs]
        ,["GlrFsaa2xQx",   glrTestArgs]
        ,["GlrFsaa4xGs",   glrTestArgs]
        ,["GlrFsaa4xFos",  glrTestArgs]
        ,["GlrMrtRgbU",    glrTestArgs]
        ,["GlrMrtRgbF",    glrTestArgs]
        ,["GlrY8",         glrTestArgs]
        ,["GlrFsaa4v4",    glrTestArgs]
        ,["GlrR5G6B5",     glrTestArgs]
        ,["GlrFsaa8x",     glrTestArgs]
        ,["GlrFsaa8v8",    glrTestArgs]
        ,["GlrFsaa8v24",   glrTestArgs]
        ,["GlrA8R8G8B8Sys",glrTestArgs]
        ]);
    spec.AddTests([
        ["SyncParallelTests", {"ParallelId" : PG_DISPLAY}]
       ,["SyncParallelTests", {"ParallelId" : PG_GRAPHICS}]
       ]);
    spec.AddTests([
        ["Elpg",           setLoops(16)]
        ,["ElpgGraphicsStress", {LoopMs: (isFirst ? 250 : 100)}]
        ,["DeepIdleStress",     {LoopMs: 150}]
        ,["GLStress",      { "LoopMs" : (isFirst ? 5000 : 1000)}]
        ,["GLStressPulse", { "LoopMs" : (isFirst ? 2000 : 250)}]
        ,["GLRandomCtxSw", glrTestArgs]
        ]);

    if (isFirst)
    {
        spec.AddTests([
            ,"CheckPowerPhases"
            ]);
    }

    spec.AddTests([
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
        spec.AddTests([
            "IntAzaliaLoopback"
            ]);
    }

    spec.AddTests([
        ["MMERandomTest",       setLoops(100 * (isFirst ? 10 : 2))]
        ,["Bar1RemapperTest",   setLoops(5 * (isFirst ? 10 : 2))]
        ,["EccFbTest",          setLoops(7000)]
        ,["EccL2Test",          setLoops(20000)]
        ,["EccSMTest",          setLoops(32)]
        ]);

    if (isLast)
    {
        spec.AddTests([
            "CheckOvertemp"
            ,"CheckFbCalib"
            ,["KFuseSanity",    setLoops(1)]
            ,"Optimus"
            ]);
    }
}

Gpu.PostInitCallback = "HandleGpuPostInitArgsGridFull";
