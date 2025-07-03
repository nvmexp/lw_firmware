//***************************************************************************
// Test specification for running MODS on emulation
//
// To run this test spec, execute the following command:
// mods -gpubios 0 gf119_emu_sddr3.rom gputest.js -readspec emu.spc
//
// Another useful switch: -only_pci_dev bus:dev.fun
// This selects only one PCI device to be initialized by RM, other devices
// will be ignored.
//***************************************************************************

g_SpecArgs  = "-maxwh 640 480";
g_SpecArgs += " -poll_hw_hz 10000";
g_SpecArgs += " -timeout_ms 1800000";
g_SpecArgs += " -verbose";
g_SpecArgs += " -disable_mods_console";

if (OperatingSystem == "Sim")
    g_SpecArgs += " -poll_interrupts";

//------------------------------------------------------------------------------
function userSpec(testList, PerfPtList)
{    
    testList.TestMode = 128;

    var glrTestArgs = {TestConfiguration:
        {Loops:3*10, RestartSkipCount:10}};

    testList.AddTests(
    [
        [ "GlrA8R8G8B8", glrTestArgs]
       ,[ "GLStress", {"LoopMs":0, "TestConfiguration":{"Loops":5}}]
       ,[ "GLCombustPulse", {"LoopMs":0, "TestConfiguration":{"Loops":4}, "DrawRepeats":20}]
       ,[ "GLRandomCtxSw", glrTestArgs]
       ,[ "LwdaRandom", {TestConfiguration:{Loops:4, RestartSkipCount:1}}]
       ,[ "CpyEngTest", {TestConfiguration:{Loops:5}}]
       ,[ "Random2dFb", {TestConfiguration:{Loops:2*200}}]
       ,[ "MatsTest", {Coverage: 1, MaxFbMb: 2}]
    ]);
}

