/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/*
################################################################################################################################
N17x NBMFG Script


Rev 01- Initial release


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    NBMFG SPEC : START
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*/

/*
Sections

1 - Test Conditions:
Where we determine which VF points to run. It includes critical points where
we test intermediate points that are interesting and inflection points that we are interested in.

2 - Test List and Arguments:
Test module definitions and arguments. We can define which VF points to run
on which tests. We can also define test arguments for each VF point.

3 - Exelwtion Script:
Top level exelwtion code. This is where the code is actually read and exelwted.
It is split into 3 sub sections: inflection points (static), interesting points
(static), and inflection points (jumping)

4 - Function Definitions:
This is where the functions that are used within the exelwtion script are
defined. These should NOT be modified except by the script owner.

-----------------------------------------------------------------------------
*/
var g_MTTTotalTestTime = 13 * 60 * 1000; //30 min
g_TestMode = 16;


//***************************************************
// Section 1 - Test Conditions
//***************************************************

//Define Inflection Points for static testing
var g_InflPt =
[
  {PStateNum: 0, LocationStr: "max"},
  {PStateNum: 0, LocationStr: "intersect"},
  {PStateNum: 3, LocationStr: "max"},
  {PStateNum: 5, LocationStr: "max"},
  {PStateNum: 5, LocationStr: "intersect"},
  {PStateNum: 8, LocationStr: "intersect"},
  {PStateNum: 8, LocationStr: "max"}

];

// Parameters for Test 6
var g_Switch_Params =
{
    UseInflectionPts: true,
//    UserPStates: [0,5,8],
    PerfSweepTableName: "",
    PerfPointTableName: "",
    PerfJumps: true,
    PerfSweep: false,
    PerfSweepTimeMs: "",
    PerfJumpTimeMs: "",
    PerfPointTimingFP: "[[10, 20000, 20000]]",
    Verbose: false
}

var g_Sweep_Params =
{
    DumpCoverage: true,
    //UserPStates: [0,5,8],
    ThrashPowerCapping: true,
    //PerfSweepTableName: "",
    //PerfPointTableName: "",
    //PerfJumps: false,
    //PerfSweep: true,
    //PerfSweepTimeMs: "",
    //PerfJumpTimeMs: "",
    //PerfPointTimingFP: "[[10, 20000, 20000]]",
    Verbose: false
}

var g_Sweep_Params_p5 =
{
    UseInflectionPts: false,
    UserPStates: [5],
    PerfSweepTableName: "",
    PerfPointTableName: "",
    PerfJumps: false,
    PerfSweep: true,
    PerfSweepTimeMs: "",
    PerfJumpTimeMs: "",
    PerfPointTimingFP: "[[10, 20000, 20000]]",
    Verbose: false
}

var g_Pwm_Params =
{
    Verbose: false,
    PrintStats: true,
    MinTargetTimeMs: 10000 // This is the main test time control for PwmMode
}

//***************************************************
// Section 2 - Test List and Arguments
//***************************************************

// ./mods gputest.js -readspec nbmfg.spc -vfe_version_check_ignore -devid_check_ignore -adc_cal_check_ignore -no_gold -skip 145 -skip 245 -skip 317 -skip 132 -skip 137 -skip 138 -time -blink_lights -disable_mods_console -rmkey RMFermiL2CacheBypass 1 -disable_ecc -hw_speedo_check_ignore -run_on_error -perlink_aspm 0 0 0 -ignore_ot_event

//List of RunOnce-Initial Sanity Tests

var g_InitialTests =
{
 CheckConfig:          {name: "CheckConfig",        number: 1,     pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
 CheckConfig2:         {name: "CheckConfig2",       number: 350,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {SkipPwrSensorCheck: true}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  // LWSpecs needs a per-SKU JSON file that is *NOT* part of the MODS package!
  // It must be downloaded from the LWSpecs website: https://lwspecs.lwpu.com/
  // LWSpecs documentation: https://confluence.lwpu.com/x/IM4bCw
  LWSpecs:             {name: "LWSpecs",           number: 14,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {Filename:"LWSpecsData.json"}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  CheckClocks:			{name: "CheckClocks", 		 number: 10, 	pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {DumpPowerInfo:true, NumPowerCapCycles:7, Verbose:true}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  CheckAVFS:			{name: "CheckAVFS", 		 number: 13, 	pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {AdcMargin:3,NegAdcMargin:-3.5,SkipAdcCalCheck:true,Verbose:true,LowTempLwtoff:59.999,LowTempAdcMargin:3,NegLowTempAdcMargin:-4.2}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  PwrRegCheck:			{name: "PwrRegCheck", 		 number: 394, 	pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  CheckThermalSanity:   {name: "CheckThermalSanity", number: 31,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  //CheckFanSanity:       {name: "CheckFanSanity", number: 78,        pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
 //                                       ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  I2CTest:              {name: "I2CTest",            number: 50,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  CheckOvertemp:        {name: "CheckOvertemp",      number: 65,    pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 1, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  CheckInputVoltage:          {name: "CheckInputVoltage",        number: 271,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {},  max_8 : {},crit : {},mem : {}}},
//j
//j  LSFalcon:             {name: "LSFalcon",        number: 55,       pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
//j                                       ,args:   {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  GpuPllLockTest:       {name: "GpuPllLockTest",   number: 170,     pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                       ,args:   {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  I2cDcbSanityTest:     {name: "I2cDcbSanityTest",   number: 293,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}}
 // CheckInfoROM:         {name: "CheckInfoROM",       number: 171,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
 //                                       ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}}

};

// List of test for GC6 and Optimus
 var g_GC6_optimus =
{
   GpuGc6Test:          {name: "GpuGcx",         number: 347,     pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
   GlrA8R8G8B8GCx:      {name: "GlrA8R8G8B8GCx", number: 202,     pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  // CheckGc6EC:          {name: "CheckGc6EC",        number: 188,     pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
  //                                      ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
   Optimus:             {name: "Optimus",        number: 63,      pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}
  };
//List of Memory Tests
var g_MemTests =
{
  FastMatsTest:         {name: "FastMatsTest",       number: 19,    pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {Coverage : 10},int_3 : {Coverage : 10}, int_5 : {Coverage : 10},int_8 : {Coverage : 10},crit : {},mem : {}}},
 // LwdaL2Mats:           {name: "LwdaL2Mats",         number: 154,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
 //                                       ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j  NewLwdaMatsTest:      {name: "NewLwdaMatsTest",    number: 143,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
//j                                        ,args:  {max_0 : {Iterations: 10}, int_0 : {Iterations: 5},int_3 : {Iterations: 5}, int_5 : {Iterations: 5},int_8 : {Iterations: 3},crit : {Iterations: 15}}},
  WfMatsMedium:         {name: "WfMatsMedium",       number: 118,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {MaxFbMb : 8}, int_8 : {MaxFbMb : 8},crit : {},mem : {}}},
  I2MTest:              {name: "I2MTest",            number: 205,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
 WfMatsBgStress:       {name: "WfMatsBgStress",     number: 178,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {"InnerLoops":1, "bgTestArg":{"RuntimeMs":10},"TestConfiguration":{"Loops":1}}, int_0 : {},max_2 : {}, max_3 : {}, max_5 : {}, max_8 : {}, int_8 : {}}},
//j
 NewLwdaMatsRandom:      {name: "NewLwdaMatsRandom",    number: 242,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {"RuntimeMs":30000,"RandomAccessBytes":256,"ParallelGroup":0}, int_0 : {},int_3 : {}, int_5 : {},int_8 : {},crit : {}}}
//j
}

//List of PCI Express Tests
var g_PcieTests =
{
  CpyEngTest:           {name: "CpyEngTest",         number: 100,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j  MultiBoardDma:        {name: "MultiBoardDma",      number: 41,    pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
//j                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  PcieSpeedChange:     {name: "PcieSpeedChange",     number: 30,     pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_3 : 0, int_3 : 1, min_3 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:   {max_0 : {}, int_0 : {},int_5 : {}, int_8 : {},crit : {},mem : {}}},
 PexBandwidth:         {name: "PexBandwidth",       number: 146,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  PcieEyeDiagram:       {name: "PcieEyeDiagram",      number: 249,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {XPassThreshold: 25, XFailThreshold: 24, YPassThreshold: 17, YFailThreshold: 16}, int_0 : {XPassThreshold: 25, XFailThreshold: 24, YPassThreshold: 17, YFailThreshold: 16},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  Bar1RemapperTest:     {name: "Bar1RemapperTest",   number: 151,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}}
}

//List of GLRandom Tests
var g_RandomTests =
{
  Random2dFb:           {name: "Random2dFb",         number: 58,    pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  Random2dNc:           {name: "Random2dNc",         number: 9,     pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LineTest:             {name: "LineTest",           number: 108,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
 // PathRender:           {name: "PathRender",         number: 176,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
 //                                       ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrA8R8G8B8:          {name: "GlrA8R8G8B8",        number: 130,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 1, int_8 : 1, min_8 : 1, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":5,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrR5G6B5:            {name: "GlrR5G6B5",          number: 131,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrFsaa2x:            {name: "GlrFsaa2x",          number: 132,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrFsaa4x:            {name: "GlrFsaa4x",          number: 133,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrMrtRgbU:           {name: "GlrMrtRgbU",         number: 135,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrMrtRgbF:           {name: "GlrMrtRgbF",         number: 136,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrY8:                {name: "GlrY8",              number: 137,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrFsaa8x:            {name: "GlrFsaa8x",          number: 138,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrA8R8G8B8Sys:       {name: "GlrA8R8G8B8Sys",     number: 148,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaRandom:           {name: "LwdaRandom",         number: 119,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrLayered:           {name: "GlrLayered",         number: 231,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":5,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  MsAAPR:               {name: "MsAAPR",             number: 287,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLPRTir:              {name: "GLPRTir",            number: 289,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  FillRectangle:        {name: "FillRectangle",      number: 286,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j
  GlrGrRpg:        {name: "GlrGrRpg",      number: 332,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrMsRppg:        {name: "GlrMsRppg",      number: 334,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrMsRpg:        {name: "GlrMsRpg",      number: 333,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}
//j
}

//List of GLStress Tests
var g_StressTests =

{
  GLStressZ:            {name: "GLStressZ",          number: 166,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                       ,args:  {max_0 : {LoopMs:100}, int_0 : {LoopMs:100},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLStressDots:         {name: "GLStressDots",       number: 23,    pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLStress:             {name: "GLStress",           number: 2,     pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {RuntimeMs:10}, int_0 : {RuntimeMs:100},int_3 : {RuntimeMs:100},int_5 : {RuntimeMs:100},int_8 : {RuntimeMs:100},crit : {},mem : {}}},
  GLStressPulse:        {name: "GLStressPulse",      number: 92,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j
  VkStressPulse:             {name: "VkStressPulse",           number: 192,     pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_3 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaStress2:          {name: "LwdaStress2",        number: 198,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j  LwdaLinpackSgemm:     {name: "LwdaLinpackSgemm",   number: 200,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
//j                                        ,args:  {max_0 : {RuntimeMs:100}, int_0 : {RuntimeMs:100},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CaskLinpackSgemm:   {name: "CaskLinpackSgemm", number: 600,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                           ,args:  {max_0 : {RuntimeMs: 2000}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j
  LwdaLinpackPulseSP:   {name: "LwdaLinpackPulseSP", number: 296,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {RuntimeMs:100}, int_0 : {RuntimeMs:100},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j  LwdaLinpackHgemm:     {name: "LwdaLinpackHgemm",   number: 210,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
//j                                        ,args:  {max_0 : {RuntimeMs:100}, int_0 : {RuntimeMs:100},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j  LwdaLinpackPulseHP:   {name: "LwdaLinpackPulseHP", number: 211,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
//j                                        ,args:  {max_0 : {RuntimeMs:100}, int_0 : {RuntimeMs:100},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CaskLinpackHgemm:   {name: "CaskLinpackHgemm", number: 606,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                           ,args:  {max_0 : {RuntimeMs: 2000}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j
//j  LwdaLinpackIgemm:     {name: "LwdaLinpackIgemm",   number: 212,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
//j                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CaskLinpackIgemm:   {name: "CaskLinpackIgemm", number: 602,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                           ,args:  {max_0 : {RuntimeMs: 2000}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j
  LwdaLinpackPulseIP:   {name: "LwdaLinpackPulseIP", number: 213,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLPowerStress:        {name: "GLPowerStress",      number: 153,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {LoopMs:5}, int_0 : {LoopMs:5},int_3 : {LoopMs:5},int_5 : {LoopMs:5},int_8 : {LoopMs:5},crit : {},mem : {}}},
  LwdaRadixSortTest:    {name: "LwdaRadixSortTest",  number: 185,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLRandomCtxSw:        {name: "GLRandomCtxSw",      number: 54,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":20,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLComputeTest:        {name: "GLComputeTest",      number: 201,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {"TestConfiguration":{"Loops":1,"StartLoop":0}}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLCombustTest:        {name: "GLCombustTest",      number: 172,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLInfernoTest:        {name: "GLInfernoTest",      number: 222,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
//j
  CaskLinpackBMMA:   {name: "CaskLinpackBMMA", number: 640,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {"MNKMode":2,"RuntimeMs":5000,"PrintPerf":0,"Ksize":262144}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
 CaskLinpackSparseHMMA:   {name: "CaskLinpackSparseHMMA", number: 630,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_3 : 0, int_3 : 0, min_3 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                           ,args:  {max_0 : {"MNKMode":2,"RuntimeMs":5000,"PrintPerf":0,"Ksize":31232}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}
//j
}

//List of Display Tests
var g_DisplayTests =
{
//j  SorLoopback:		    {name: "SorLoopback",  number: 11,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, int_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
//j                                       ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, int_5 : {}, max_8 : {}, int_8 : {},crit : {},mem : {}}},
  DispClkSwitch:        {name: "DispClkSwitch",  number: 245,    pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                       ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  DispClkIntersect:     {name: "DispClkIntersect",  number: 302,    pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  LwDisplayRandom:		{name: "LwDisplayRandom", number: 304,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, int_5: 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, int_5: {}, max_8 : {}, int_8 : {}, crit : {},mem : {}}},
  LwDisplayHtol:		{name: "LwDisplayHtol", number: 317,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
  VgaHW:				{name: "VgaHW", 		number: 234,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_2 : 0, max_3 : 0, max_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3 : {}, max_5 : {}, max_8 : {},crit : {},mem : {}}}
}

//List of Video Tests
var g_VideoTests =
{

 LwvidLWDEC:            {name: "LwvidLWDEC",      number: 360,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, max_2 : 0, max_3 : 0, max_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3: {}, max_5 : {}, max_8 : {},crit : {},mem : {}}},
 LWENCTest:            {name: "LWENCTest",      number: 278,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, max_2 : 0, max_3 : 0, max_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},max_2 : {}, max_3 : {}, max_5 : {}, max_8 : {},crit : {},mem : {}}}
}

//Tests which are run in foreground when doing Perf Switching
var g_Switching =
{
  //Memory tests for jumping
  WfMatsMedium:         {name: "WfMatsMedium", number: 118, skip: false, args: {MaxFbMb : 8}},

  //PCIE tests for jumping
//  CpyEngTest:           {name: "CpyEngTest", number: 100, skip: false, args: {}},
//  MMERandomTest:        {name: "MMERandomTest", number: 150, skip: false, args: {}},
//  Bar1RemapperTest:     {name: "Bar1RemapperTest", number: 151, skip: false, args: {}},

  //Random tests for jumping
//  Random2dFb:           {name: "Random2dFb", number: 58, skip: false, args: {}},
//  Random2dNc:           {name: "Random2dNc", number: 9, skip: false, args: {}},
//  LineTest:             {name: "LineTest", number: 108, skip: false, args: {}},
//  GlrA8R8G8B8:          {name: "GlrA8R8G8B8", number: 130, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrR5G6B5:            {name: "GlrR5G6B5", number: 131, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrFsaa2x:            {name: "GlrFsaa2x", number: 132, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrFsaa4x:            {name: "GlrFsaa4x", number: 133, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrMrtRgbU:           {name: "GlrMrtRgbU", number: 135, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrMrtRgbF:           {name: "GlrMrtRgbF", number: 136, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrY8:                {name: "GlrY8", number: 137, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrFsaa8x:            {name: "GlrFsaa8x", number: 138, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  GlrA8R8G8B8Sys:       {name: "GlrA8R8G8B8Sys", number: 148, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
//  LwdaRandom:           {name: "LwdaRandom", number: 119, skip: false, args: {}},

  //Stress Tests for jumping
  //GLStressZ:            {name: "GLStressZ", number: 166, skip: false, args: {}},
//  GLStress:             {name: "GLStress", number: 2, skip: false, args: {}},
//  GLStressPulse:        {name: "GLStressPulse", number: 92, skip: false, args: {}},
//  LwdaStress2:          {name: "LwdaStress2", number: 198, skip: false, args: {}},
//  GLPowerStress:        {name: "GLPowerStress", number: 153, skip: false, args: {}},
//  GLRandomCtxSw:        {name: "GLRandomCtxSw", number: 54, skip: false, args: {}},
//    NewLwdaMatsReadOnlyPulse: {name: "NewLwdaMatsReadOnlyPulse", number: 354, skip: false, args: {RuntimeMs:100}},
	LwdaTTUStress:        {name: "LwdaTTUStress",      number: 340, skip: false, args: {}},
//	NewLwdaMatsRandom:    {name: "NewLwdaMatsRandom",  number: 242, skip: false, args: {}},
//j	LwdaLinpackHMMAgemm:  {name: "LwdaLinpackHMMAgemm", number: 310, skip: false, args: {}},
    CaskLinpackHMMAgemm:  {name: "CaskLinpackHMMAgemm", number: 610, skip: false, args: {RuntimeMs: 2000}},
//j
//  LwdaLinpackPulseHMMA: {name: "LwdaLinpackPulseHMMA", number: 311, skip: false, args: {}},
//j    LwdaLinpackIMMAgemm:  {name: "LwdaLinpackPulseHMMA", number: 312, skip: false, args: {}},
    CaskLinpackIMMAgemm:  {name: "CaskLinpackIMMAgemm", number: 612, skip: false, args: {RuntimeMs: 2000}},
//j
//j    LwdaLinpackPulseIMMA: {name: "LwdaLinpackPulseHMMA", number: 313, skip: false, args: {}},
    CaskLinpackPulseIMMAgemm:  {name: "CaskLinpackPulseIMMAgemm", number: 613, skip: false, args: {RuntimeMs: 2000}},
//j
    VkStress:             {name: "VkStress", number: 267, skip: false, args: {}}
 }

//Tests which are run in foreground when doing Perf Sweep
var g_Sweep =
{
  //Random tests for sweeping
  GlrR5G6B5:            {name: "GlrR5G6B5", number: 131, skip: false, args: {}},
  LwdaRandom:           {name: "LwdaRandom", number: 119, skip: false, args: {}}
 // GLStress:             {name: "GLStress", number: 2, skip: false, args: {LoopMs:1}}
}

//Tests which are run in foreground when doing Perf Sweep for P5
var g_Sweep_p5 =
{
  //Stress Tests for sweeping
 // GLStress:             {name: "GLStress", number: 2, skip: false, args: {LoopMs:100}},
  LwdaRandom:           {name: "LwdaRandom", number: 119, skip: false, args: {LoopMs:100}}
}

//***************************************************
// Section 3 - Exelwtion Script
//***************************************************

function userSpec(spec)
{
  spec.TestMode = 4; // TM_MFG_BOARD2

  //Run Static Inflection Points
  for (var i = 0; i < g_InflPt.length; i++)
  {
    var strStaticPoint = Parse_VFPoint(g_InflPt[i].PStateNum, g_InflPt[i].LocationStr);
    spec.AddTest("SetPState", { PerfTable: g_InflPt, PerfTableIdx: i});
    runAllTests(spec,strStaticPoint);
  }

  //Run Perf Switching Tests
 runPerfPunish(spec, g_Switching, g_Switch_Params);


}

//***************************************************
// Section 4 - Support Functions
// DO NOT MODIFY THIS SECTION
//***************************************************

// Function which runs all the Tests
function runAllTests(spec,pstate)
{
  // Run All Tests in different category based on pre-decided order
  runTests(spec, g_InitialTests,pstate);
  runTests(spec, g_MemTests,pstate);
  runTests(spec, g_PcieTests,pstate);
  runTests(spec, g_RandomTests,pstate);
  runTests(spec, g_StressTests,pstate);
  runTests(spec, g_DisplayTests,pstate);
  runTests(spec, g_VideoTests,pstate);
  runTests(spec, g_GC6_optimus,pstate);
}

// Function to run each Test individually
function runTests(spec, g_Tests, pstate)
{
    var key;

    for(key in g_Tests)
    {
    if(g_Tests[key].pstates[pstate])
        {
			spec.AddTest(g_Tests[key].name, g_Tests[key].args[pstate]);
        }
    }
}

// Function for running Test 145 - Perf Switch/Sweep Tests
function runPerfPunish(spec, g_Tests, g_Params)
{
    var key;
	spec.TestMode = 4;
    for(key in g_Tests)
    {
        if(g_Tests[key].skip == false)
        {
            spec.AddTest("PerfPunish",
                {
                    FgTestNum: g_Tests[key].number,
					FgTestArgs: g_Tests[key].args,
                    DumpCoverage: g_Params.DumpCoverage,
					ThrashPowerCapping: g_Params.ThrashPowerCapping,
                    UserPStates: g_Params.UserPStates,
                   // PerfSpecFile: g_Params.PerfSpecFile,
                    //PerfSweepTableName: g_Params.PerfSweepTableName,
                    //PerfPointTableName: g_Params.PerfPointTableName,
                   // PerfJumps: g_Params.PerfJumps,
                    //PerfSweep: g_Params.PerfSweep,
                    //PerfSweepTimeMs: g_Params.PerfSweepTimeMs,
                    //PerfPointTimingFP: g_Params.PerfPointTimingFP,
                    Verbose: g_Params.Verbose
                }
            );
        }
    }
}

// Function for parsing the VF point into a string
function Parse_VFPoint(intPState, strInflection)
{
  switch (intPState)
  {
    case 0:
      switch (strInflection)
      {
        case "max":
          return "max_0";
        case "mid":
          return "mid_0";
        case "nom":
          return "nom_0";
        case "intersect":
          return "int_0";
        case "min":
          return "min_0";
      }
	  case 3:
      switch (strInflection)
      {
        case "max":
          return "max_3";
        case "mid":
          return "mid_3";
        case "nom":
          return "nom_3";
        case "intersect":
          return "int_3";
        case "min":
          return "min_3";
      }
    case 5:
      switch (strInflection)
      {
        case "max":
          return "max_5";
        case "mid":
          return "mid_5";
        case "nom":
          return "nom_5";
        case "intersect":
          return "int_5";
        case "min":
          return "min_5";
      }
    case 8:
      switch (strInflection)
      {
        case "max":
          return "max_8";
        case "mid":
          return "mid_8";
        case "nom":
          return "nom_8";
        case "intersect":
          return "int_8";
        case "min":
          return "min_8";
      }
  }
  return "failed";
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    NBMFG SPEC : END
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*/



