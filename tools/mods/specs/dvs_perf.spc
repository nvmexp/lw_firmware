/*
 * LWIDIA_COPYRIGHT_BEGIN
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: //sw/integ/gpu_drv/stage_rel/diag/mods/specs/dvs_perf.spc#7 $");

/*
 * This spec file is intended to regress a handful of tests' performance
 * characteristics by using tight bounds on performance values like
 * GFLOPs, RuntimeMs, etc. By running tests for much longer, this spec
 * will let us "average out" errors caused by RM power-capping that
 * are seen in much shorter runs, which cause a lot of variation in
 * performance values.
 *
 * There are three phases to this spec file:
 *  1. Cooldown the board to some target temperature
 *  2. Linpack tests at 0.tdp with no power-capping
 *  3. Various stress tests at 0.max with power-capping enabled
 *
 * Phase 2 is intended to better mimic how Tesla BU uses GFLOPS to screen
 * out underperforming parts.
 *
 */

g_TestMode = (1<<6); // TM_DVS

// Enable background loggers as these can be useful in explaining performance
// changes. Also, nothing else in DVS/PVS is regressing these arguments.
g_SpecArgs =  "-bg_print_ms 5000";
g_SpecArgs += " -bg_power 1000";
g_SpecArgs += " -bg_int_temp 1000";
g_SpecArgs += " -bg_clocks gpc,m 1000";
g_SpecArgs += " -bg_part_clocks gpc,m 1000";
g_SpecArgs += " -bg_core_voltage 1000";
g_SpecArgs += " -bg_fan 1000";
g_SpecArgs += " -bg_thermal_slowdown 1000";
g_SpecArgs += " -bg_gpc_limits verbose 1000";
g_SpecArgs += " -dump_stats";

function userSpec(testList, PerfPtList)
{
    testList.TestMode = (1<<6); // TM_DVS

    testList.AddTests(
    [
        ["SetPState", { InfPts: { PStateNum: 0, LocationStr: "min" }}],
        ["Cooldown", { DelayMs: 60000 }]
    ]);

    // Use a slightly shorter runtime as these tests are bypassing power
    // and thermal capping.
    let runtimeMs = 7500;
    testList.AddTests(
    [
        ["SetPState", { InfPts: { PStateNum: 0, LocationStr: "tdp" },
                        PStateLockType: PerfConst.HardLock }],
        ["CaskLinpackHMMAgemm", { VirtualTestId: "HMMA_no_pwrcap",
                                  RuntimeMs: runtimeMs, PrintPerf: true }],
        ["LwdaLinpackHMMAgemm", { VirtualTestId: "HMMA_no_pwrcap",
                                  RuntimeMs: runtimeMs, PrintPerf: true }],
        ["LwdaLinpackSgemm", { VirtualTestId: "SGEMM_no_pwrcap",
                               RuntimeMs: runtimeMs, PrintPerf: true }]
    ]);

    runtimeMs = 15000;
    testList.AddTests(
    [
        ["SetPState", { InfPts: { PStateNum: 0, LocationStr: "max" },
                        PStateLockType: PerfConst.StrictLock }],
        ["CaskLinpackHMMAgemm", { RuntimeMs: runtimeMs, PrintPerf: true }],
        ["LwdaLinpackHMMAgemm", { RuntimeMs: runtimeMs, PrintPerf: true }],
        ["LwdaLinpackIMMAgemm", { RuntimeMs: runtimeMs, PrintPerf: true }],
        ["LwdaLinpackSgemm", { RuntimeMs: runtimeMs, PrintPerf: true }],
        ["LwdaLinpackDgemm", { RuntimeMs: runtimeMs, PrintPerf: true }],
        ["VkStress", { LoopMs: runtimeMs, PrintPerf: true }],
        ["VkPowerStress", { LoopMs: runtimeMs, PrintPerf: true }],
        ["PerfPunish", { PunishTimeMs: runtimeMs, DumpCoverage: true }],
        ["PerfPunish", { VirtualTestId: "PerfPunish_no_pstate_switching",
                         PunishTimeMs: runtimeMs, DumpCoverage: true, UserPStates: [0], 
                         MeasureVfSwitchTime: true}]
    ]);
}

