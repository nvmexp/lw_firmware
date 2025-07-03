/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

AddFileId(Out.PriNormal, "$Id: $");

// For automated runs disable CSL, otherwise enable it for the block you suspect.
SetElw("CSL", "[enable;sendto:csl.log]lwswitch");

g_IsGenAndTest = true;
g_LoadGoldelwalues = false;


let g_LS10SpecArgs  = "";
g_LS10SpecArgs += " -lwlink_force_optimization_algorithm 0x1";
g_LS10SpecArgs += " -lwlink_auto_link_config -lwlink_ignore_init_errors";
g_LS10SpecArgs += " -lwlink_topology_match_file -lwswitchkey SoeDisable 0x1";
g_LS10SpecArgs += " -chipargs -noinstance_lwdec -chipargs -noinstance_lwenc";
g_LS10SpecArgs += " -chipargs -noinstance_lwjpg -chipargs -noinstance_ofa";
g_LS10SpecArgs += "  -pmu_bootstrap_mode 1 -exelwte_rm_devinit_on_pmu 0";
g_LS10SpecArgs += " -chipargs -lwlink_sockets -chipargs -lwlink_token_discovery";
g_LS10SpecArgs += " -lwlink_skip_check -skip_pertest_pexcheck -lwlink_verbose 1";

let g_TrexSimpleTopoArgs = "";
g_TrexSimpleTopoArgs += " -chipargs -chipPOR=ls10";
g_TrexSimpleTopoArgs += " -lwlink_topology_file lstrexsimple.topo -chipargs -ioctrl_config_file_name=lstrexsimple.ini";
g_TrexSimpleTopoArgs += " -lwlink_topology_trex -lwswitchkey ChiplibForcedLinkConfigMask 0x33";

let g_SimpleTopoArgs = "";
g_SimpleTopoArgs += " -chipargs -chipPOR=gh100";
g_SimpleTopoArgs += " -lwlink_topology_file lssimple.topo -chipargs -ioctrl_config_file_name=lssimple.ini";
g_SimpleTopoArgs += " -lwswitchkey ChiplibForcedLinkConfigMask 0x30000 -lwswitchkey ChiplibForcedLinkConfigMask2 0x0";
g_SimpleTopoArgs += " -chipargs -sli_chipargs0=-ioctrl_gpu_num=0";
g_SimpleTopoArgs += " -chipargs -lwlink0=PEER0 -rmkey RMLwLinkSwitchLinks 0x1";

let g_GH100SpecArgs  = "";
g_GH100SpecArgs += " -chipargs -texHandleErrorsLikeRTL"; // For textures in LwdaRandom
g_GH100SpecArgs += " -chipargs -useVirtualization";
g_GH100SpecArgs += " -chipargs -use_gen5";
g_GH100SpecArgs += " -chipargs -noinstance_devices=gr1,gr2,gr3,gr4,gr5,gr6,gr7"; //$ no spaces allowed between ','
g_GH100SpecArgs += " -chipargs -use_floorsweep";
g_GH100SpecArgs += " -chipargs -num_ropltcs=4";
g_GH100SpecArgs += " -chipargs -num_fbps=2";
g_GH100SpecArgs += " -chipargs -num_fbio=4";
g_GH100SpecArgs += " -chipargs -num_tpcs=3"; // Set to 3 to use all of the Gfx capable TPCs.
//g_GH100SpecArgs += " -chipargs -fbpa_en_mask=0x000C3";
g_GH100SpecArgs += " -chipargs -ltc_en_mask=0x000C3";
g_GH100SpecArgs += " -chipargs -fbp_en_mask=0x09";
g_GH100SpecArgs += " -chiplib_deepbind"; // Bug 2424418
// Note: GPC:0, TPCs:0-2 are the only graphics capable TPCs, so GPC:0 can't be floorswept.
g_GH100SpecArgs += " -floorsweep gpc_enable:0x1:gpc_tpc_enable:0:0x7:fbp_enable:0x9:gpc_cpc_enable:0:0x1 -adjust_fs_args"; //$
g_GH100SpecArgs += " -devid_check_ignore";
g_GH100SpecArgs += " -run_on_error";
g_GH100SpecArgs += " -maxwh 160 120";
g_GH100SpecArgs += " -skip_pex_checks"; // skip all pex checks on FModel
g_GH100SpecArgs += " -exelwte_rm_devinit_on_pmu 0 -pmu_bootstrap_mode 1";

// dvs uses this file to regress Fmodel. It does ilwocations of mods using the
// following command lines:
//mods -a -chip lslit2_fmodel_64.so gputest.js -readspec ls10fmod.spc -spec dvsSpecLwLinkTrex
//mods -a -chip ghlit1_fmodel_64.so -gpubios 0 gh100_sim_hbm.rom gputest.js -readspec ls10fmod.spc -spec dvsSpecLwLinkBwStress


//------------------------------------------------------------------------------
function dvsSpecNoTest(testList)
{
}
dvsSpecNoTest.specArgs = g_LS10SpecArgs + g_TrexSimpleTopoArgs + " -no_test";

//-----------------------------------------------------------------------------
function dvsSpecLwLinkTrex(testList)
{
    testList.AddTests(
    [
        ["LwLinkBwStressTrex", {"SkipBandwidthCheck":true,
                                "TestConfiguration":{"ShortenTestForSim":true}}],
        ["LwlbwsTrexLpModeSwToggle", {"SkipBandwidthCheck":true,
                                      "TestConfiguration":{"ShortenTestForSim":true}}],
        ["LoadLwLinkTopology", {"TopologyFile":"lstrexmulticastsimple.topo", "EnableTrex":true}],
        ["LwLinkMulticastTrex", {"TestConfiguration":{"ShortenTestForSim":true}}]
    ]);
}
dvsSpecLwLinkTrex.specArgs = g_LS10SpecArgs + g_TrexSimpleTopoArgs;

function dvsSpecLwLinkBwStress(testList)
{
    testList.AddTests(
    [
        ["LwLinkBwStress", {"SkipBandwidthCheck":true,
                            "UseLwda":false,
                            "UseDmaInit":false,
                            "TestConfiguration":{"ShortenTestForSim":true,
                                                 "SurfaceWidth":32,
                                                 "SurfaceHeight":32,
                                                 "Loops":2}}]
       ,["LwlbwsLpModeSwToggle", {"SkipBandwidthCheck":true,
                                  "UseLwda":false,
                                  "UseDmaInit":false,
                                  "TestConfiguration":{"ShortenTestForSim":true,
                                                       "SurfaceWidth":32,
                                                       "SurfaceHeight":32,
                                                       "Loops":2}}]
    ]);
}
dvsSpecLwLinkBwStress.specArgs = g_GH100SpecArgs + g_LS10SpecArgs + g_SimpleTopoArgs;
