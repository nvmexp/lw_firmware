/*
* LWIDIA_COPYRIGHT_BEGIN
* Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
* LWIDIA_COPYRIGHT_END
*/

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/gpu/js/controlrun.spc#15 $");


g_SpecArgs = "-power_cap_tgp_mw 216000 -power_cap_policy 4 174000 -dramclk +2pct,0.all  -vfe_var_override fuse 0 offset 45 -rail_limit_offset_mv lwvdd vmin -25 -chipset_aspm 0 -edc_tol 100 -json -only_family pascal -dump_inforom";

//***************************************************
// Section 1 - Test Conditions
//***************************************************

//Define Inflection Points for static testing
var g_InflPt =
[
  {PStateNum: 0, LocationStr: "max"},
  {PStateNum: 0, LocationStr: "intersect"},
  {PStateNum: 5, LocationStr: "intersect"},
  {PStateNum: 8, LocationStr: "intersect"}
];

// Parameters for Test 145
var g_Switch_Params =
{
    UseInflectionPts: true,
    UserPStates: [0, 5, 8],
    PerfSpecFile: "gp104_pg413_sku0_cr.spc",
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
    UseInflectionPts: false,
    UserPStates: [0],
    PerfSpecFile: "gp104_pg413_sku0_cr.spc",
    PerfSweepTableName: "",
    PerfPointTableName: "",
    PerfJumps: false,
    PerfSweep: true,
    PerfSweepTimeMs: "",
    PerfJumpTimeMs: "",
    PerfPointTimingFP: "[[10, 20000, 20000]]",
    Verbose: false
}

var g_Sweep_Params_p5 =
{
    UseInflectionPts: false,
    UserPStates: [5],
    PerfSpecFile: "gp104_pg413_sku0_cr.spc",
    PerfSweepTableName: "",
    PerfPointTableName: "",
    PerfJumps: false,
    PerfSweep: true,
    PerfSweepTimeMs: "",
    PerfJumpTimeMs: "",
    PerfPointTimingFP: "[[10, 20000, 20000]]",
    Verbose: false
}

//***************************************************
// Section 2 - Test List and Arguments
//***************************************************

//List of RunOnce-Initial Sanity Tests
var g_InitialTests =
{
  CheckConfig:          {name: "CheckConfig",        number: 1,     pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  ValidSkuCheck2:       {name: "ValidSkuCheck2",      number: 217,  pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CheckThermalSanity:   {name: "CheckThermalSanity", number: 31,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CheckFanSanity:       {name: "CheckFanSanity", number: 78,        pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}, 
  I2CTest:              {name: "I2CTest",            number: 50,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CheckOvertemp:        {name: "CheckOvertemp",      number: 65,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  KFuseSanity:          {name: "KFuseSanity",        number: 106,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {}, int_8 : {},crit : {},mem : {}}},
  LSFalcon:             {name: "LSFalcon",        number: 55,       pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                       ,args:   {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GpuPllLockTest:       {name: "GpuPllLockTest",   number: 170,     pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                       ,args:   {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  I2cDcbSanityTest:     {name: "I2cDcbSanityTest",   number: 293,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {UseI2cReads: true}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CheckInfoROM:         {name: "CheckInfoROM",       number: 171,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}


};

//List of Memory Tests
var g_MemTests =
{
  FastMatsTest:         {name: "FastMatsTest",       number: 19,    pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {Coverage : 50},int_8 : {Coverage : 50},crit : {},mem : {}}},
  LwdaL2Mats:           {name: "LwdaL2Mats",         number: 154,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  NewLwdaMatsTest:      {name: "NewLwdaMatsTest",    number: 143,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {Iterations: 15}, int_0 : {Iterations: 7}, int_5 : {Iterations: 5},int_8 : {Iterations: 3},crit : {Iterations: 15}}},
  WfMatsMedium:         {name: "WfMatsMedium",       number: 118,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {}, int_8 : {MaxFbMb : 8},crit : {},mem : {}}},
  I2MTest:              {name: "I2MTest",            number: 205,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CheckFbCalib:         {name: "CheckFbCalib",       number: 48,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}
}

//List of PCI Express Tests
var g_PcieTests =
{
  CpyEngTest:           {name: "CpyEngTest",         number: 100,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  CheckDma:             {name: "CheckDma",      number: 41,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  PcieSpeedChange:     {name: "PcieSpeedChange",     number: 30,     pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:   {max_0 : {}, int_0 : {},int_5 : {}, int_8 : {},crit : {},mem : {}}},
  IntAzaliaLoopback:    {name: "IntAzaliaLoopback",  number: 103,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  MMERandomTest:        {name: "MMERandomTest",      number: 150,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  Bar1RemapperTest:     {name: "Bar1RemapperTest",   number: 151,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}
}

//List of GLRandom Tests
var g_RandomTests =
{
  Random2dFb:           {name: "Random2dFb",         number: 58,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  Random2dNc:           {name: "Random2dNc",         number: 9,     pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LineTest:             {name: "LineTest",           number: 108,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  PathRender:           {name: "PathRender",         number: 176,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrA8R8G8B8:          {name: "GlrA8R8G8B8",        number: 130,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrR5G6B5:            {name: "GlrR5G6B5",          number: 131,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrFsaa2x:            {name: "GlrFsaa2x",          number: 132,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrFsaa4x:            {name: "GlrFsaa4x",          number: 133,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrMrtRgbU:           {name: "GlrMrtRgbU",         number: 135,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrMrtRgbF:           {name: "GlrMrtRgbF",         number: 136,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrY8:                {name: "GlrY8",              number: 137,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrFsaa8x:            {name: "GlrFsaa8x",          number: 138,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrA8R8G8B8Sys:       {name: "GlrA8R8G8B8Sys",     number: 148,   pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaRandom:           {name: "LwdaRandom",         number: 119,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrLayered:           {name: "GlrLayered",         number: 231,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  MsAAPR:               {name: "MsAAPR",             number: 287,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLPRTir:              {name: "GLPRTir",            number: 289,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  FillRectangle:        {name: "FillRectangle",      number: 286,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GlrA8R8G8B8Rppg:      {name: "GlrA8R8G8B8Rppg",    number: 303,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}}										
										
}

//List of GLStress Tests
var g_StressTests =
{
  GLStressZ:            {name: "GLStressZ",          number: 166,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 1, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLStressDots:         {name: "GLStressDots",       number: 23,    pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLStress:             {name: "GLStress",           number: 2,     pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 1, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLStressPulse:        {name: "GLStressPulse",      number: 92,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 1, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaStress2:          {name: "LwdaStress2",        number: 198,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaLinpackSgemm:     {name: "LwdaLinpackSgemm",   number: 200,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {RuntimeMs:20*1000}, int_0 : {RuntimeMs:20*1000},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaLinpackPulseSP:   {name: "LwdaLinpackPulseSP", number: 296,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {RuntimeMs:20*1000}, int_0 : {RuntimeMs:20*1000},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaLinpackHgemm:     {name: "LwdaLinpackHgemm",   number: 210,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {RuntimeMs:10*1000}, int_0 : {RuntimeMs:10*1000},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaLinpackPulseHP:   {name: "LwdaLinpackPulseHP", number: 211,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaLinpackIgemm:     {name: "LwdaLinpackIgemm",   number: 212,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwdaLinpackPulseIP:   {name: "LwdaLinpackPulseIP", number: 213,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},  
  GLPowerStress:        {name: "GLPowerStress",      number: 153,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {RuntimeMs:5*1000},int_8 : {RuntimeMs:5*1000},crit : {},mem : {}}},
  LwdaRadixSortTest:    {name: "LwdaRadixSortTest",  number: 185,   pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLRandomCtxSw:        {name: "GLRandomCtxSw",      number: 54,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {"TestConfiguration":{"Loops":400,"StartLoop":0}},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLComputeTest:        {name: "GLComputeTest",      number: 201,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 1, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLCombustTest:        {name: "GLCombustTest",      number: 172,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 1, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GLInfernoTest:        {name: "GLInfernoTest",      number: 222,   pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 1, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  GpuTrace:             {name: "GpuTrace",           number: 20,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {Trace: "./618-GP00B-HV40-002/test.hdr", TestConfiguration:{"Loops": 2}}, int_0 : {Trace: "./618-GP00B-HV40-002/test.hdr",
										TestConfiguration:{"Loops": 1}}, int_5 : {Trace: "./618-GP00B-HV40-002/test.hdr",  TestConfiguration:{"Loops": 1}},int_8 : {},crit : {},mem : {}}},
  PowerBalancingTest:	{name: "PowerBalancingTest", number: 279,   pstates: {max_0 : 0, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {Verbose: true},int_8 : {},crit : {},mem : {}}}
}

//List of Display Tests
var g_DisplayTests =
{
  DispClkSwitch:        {name: "DispClkSwitch",  number: 245,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                       ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  EvoLwrs:              {name: "EvoLwrs",        number: 4,      pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwDisplayRandom:      {name: "LwDisplayRandom",number: 304,    pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LwDisplayHtol:        {name: "LwDisplayHtol",  number: 317,    pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  SorLoopback:          {name: "SorLoopback",    number: 11,     pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  DispClkIntersect:     {name: "DispClkIntersect",  number: 302,    pstates: {max_0 : 1, int_0 : 0, min_0 : 0, max_5 : 0, int_5 : 0, min_5 : 0, max_8 : 0, int_8 : 0, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  EvoOvrl:              {name: "EvoOvrl",        number: 7,      pstates: {max_0 : 0, int_0 : 1, min_0 : 0, max_5 : 0, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}
}

//List of Video Tests
var g_VideoTests =
{
  LWDECTest:            {name: "LWDECTest",      number: 277,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 1, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}},
  LWENCTest:            {name: "LWENCTest",      number: 278,    pstates: {max_0 : 1, int_0 : 1, min_0 : 0, max_5 : 1, int_5 : 1, min_5 : 0, max_8 : 0, int_8 : 1, min_8 : 0, crit: 0, mem: 0}
                                        ,args:  {max_0 : {}, int_0 : {},int_5 : {},int_8 : {},crit : {},mem : {}}}
}

//Tests which are run in foreground when doing Perf Sweep
var g_Switching =
{

  //Memory tests for jumping
  WfMatsMedium:         {name: "WfMatsMedium", number: 118, skip: false, args: {}},

  //PCIE tests for jumping
  CpyEngTest:           {name: "CpyEngTest", number: 100, skip: false, args: {}},
  MMERandomTest:        {name: "MMERandomTest", number: 150, skip: false, args: {}},
  Bar1RemapperTest:     {name: "Bar1RemapperTest", number: 151, skip: false, args: {}},

  //Random tests for jumping
  Random2dFb:           {name: "Random2dFb", number: 58, skip: false, args: {}},
  Random2dNc:           {name: "Random2dNc", number: 9, skip: false, args: {}},
  LineTest:             {name: "LineTest", number: 108, skip: false, args: {}},
  GlrA8R8G8B8:          {name: "GlrA8R8G8B8", number: 130, skip: false, args: {}},
  GlrR5G6B5:            {name: "GlrR5G6B5", number: 131, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrFsaa2x:            {name: "GlrFsaa2x", number: 132, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrFsaa4x:            {name: "GlrFsaa4x", number: 133, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrMrtRgbU:           {name: "GlrMrtRgbU", number: 135, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrMrtRgbF:           {name: "GlrMrtRgbF", number: 136, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrY8:                {name: "GlrY8", number: 137, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrFsaa8x:            {name: "GlrFsaa8x", number: 138, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrFsaa4v4:           {name: "GlrFsaa4v4", number: 139, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrFsaa8v8:           {name: "GlrFsaa8v8", number: 140, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrFsaa8v24:          {name: "GlrFsaa8v24", number: 141, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  GlrA8R8G8B8Sys:       {name: "GlrA8R8G8B8Sys", number: 148, skip: false, args: {"TestConfiguration":{"Loops":400,"StartLoop":0}}},
  LwdaRandom:           {name: "LwdaRandom", number: 119, skip: false, args: {}},

  //Stress Tests for jumping
  GLStressZ:            {name: "GLStressZ", number: 166, skip: false, args: {}},
  GLStress:             {name: "GLStress", number: 2, skip: false, args: {}},
  GLStressPulse:        {name: "GLStressPulse", number: 92, skip: false, args: {}},
  LwdaStress2:          {name: "LwdaStress2", number: 198, skip: false, args: {}},
  GLPowerStress:        {name: "GLPowerStress", number: 153, skip: false, args: {}},
  GLRandomCtxSw:        {name: "GLRandomCtxSw", number: 54, skip: false, args: {}},
  GpuTrace:             {name: "GpuTrace", number: 20, skip: false, args: {Trace: "./618-GP00B-HV40-002/test.hdr", TestConfiguration:{"Loops": 1}}}
}

//Tests which are run in foreground when doing Perf Sweep
var g_Sweep =
{
  //Random tests for sweeping
  GlrR5G6B5:            {name: "GlrR5G6B5", number: 131, skip: false, args: {}},
  LwdaRandom:           {name: "LwdaRandom", number: 119, skip: false, args: {}},
  GLStress:             {name: "GLStress", number: 2, skip: false, args: {}}
}

//Tests which are run in foreground when doing Perf Sweep for P5
var g_Sweep_p5 =
{
  //Stress Tests for sweeping
  GLStress:             {name: "GLStress", number: 2, skip: false, args: {}}
}
//***************************************************
// Section 3 - Exelwtion Script
//***************************************************

function userSpec(spec)
{
  spec.TestMode = (1<<2); // TM_MFG_BOARD

  if (Golden.Store == Golden.Action)
  {
    // Do just one pstate for gpugen.js.
    // g_InflPt = g_InflPt.slice(0,1);
  }
    
  //Run Static Inflection Points
  for (var i = 0; i < g_InflPt.length; i++)
  {
    var strStaticPoint = Parse_VFPoint(g_InflPt[i].PStateNum, g_InflPt[i].LocationStr);
    spec.AddTest("SetPState", { PerfTable: g_InflPt, PerfTableIdx: i});
    runAllTests(spec,strStaticPoint);
  }

  //Run Perf Switching Tests
  runPerfSwitch(spec, g_Switching, g_Switch_Params);
  runPerfSwitch(spec, g_Sweep, g_Sweep_Params);
  runPerfSwitch(spec, g_Sweep_p5, g_Sweep_Params_p5);

  //Run T275 "Base Boost test" 
  spec.AddTest("SetPState", { PerfTable: g_InflPt, PerfTableIdx: 0});
  spec.AddTest("BaseBoostClockTest", {fixed_ambient_temp: 31});
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
function runPerfSwitch(spec, g_Tests, g_Params)
{
    var key;

    for(key in g_Tests)
    {
        if(g_Tests[key].skip == false)
        {
            spec.AddTest("PerfSwitch",
                {
                    FgTestNum: g_Tests[key].number,
					FgTestArgs: g_Tests[key].args,
                    UseInflectionPts: g_Params.UseInflectionPts,
                    UserPStates: g_Params.UserPStates,
                    PerfSpecFile: g_Params.PerfSpecFile,
                    PerfSweepTableName: g_Params.PerfSweepTableName,
                    PerfPointTableName: g_Params.PerfPointTableName,
                    PerfJumps: g_Params.PerfJumps,
                    PerfSweep: g_Params.PerfSweep,
                    PerfSweepTimeMs: g_Params.PerfSweepTimeMs,
                    PerfPointTimingFP: g_Params.PerfPointTimingFP,
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


