/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Copied from
//     //hw/chpsoln/gpu/ga100_ga101/correlation/Noise_features/Scripts/GA100_NMEAS_LITE_Noise_Measure/GA100_NMEAS_LITE_files_for_MODS/test_define.spc //$

g_SpecArgs = "";
g_TestMode = 16; // TM_MFG_BOARD

function userSpec(testList)
{
    testList.TestMode = 16;
    testList.AddTests([
/*
        [
             505,
             {
                 "Actors":
                 [
                     {
                         "GpuInst"   : 0,
                         "ActorType" : PwmConst.FREQ_ACTOR,
                         "TargetPerfPoint":
                         {
                             "PStateNum"   : 0,
                             "LocationStr" : "max",
                             "Regime"      : PerfConst.VOLTAGE_REGIME
                         },
                         "LowerPerfPoint":
                         {
                             "PStateNum"   : 0,
                             "LocationStr" : "min",
                             "Regime"      : PerfConst.FREQUENCY_REGIME
                         },
                         "DutyCyclePeriodUs" : 100000,
                         "DutyCyclePct"      : 10,
                         "SamplerPeriodUs"   : 5000,
                         "PrintStats"        : true
                     }
                 ]
             }
        ],
        [
            310,
            {
                "VirtualTestId" : "3100096",
                "Msize"         : 3968,
                "Nsize"         : 1024,
                "Ksize"         : 16384,
                "Verbose"       : true,
                "RuntimeMs"     : 15000,
                "KernelType"    : "s16816_128x128",
                "MNKMode"       : 2,
                "PrintPerf"     : true
            }
        ],
        [
            630,
            {
                "VirtualTestId" : "6300065",
                "Msize"         : 1024,
                "Nsize"         : 3968,
                "Ksize"         : 512,
                "Verbose"       : true,
                "RuntimeMs"     : 15000,
                "KernelName"    : "fp16_s16832gemm_sp_fp16_256x128_ldg8_relu_f2f_stages_64x3_nt_v1", //$
                "MNKMode"       : 2,
                "PrintPerf"     : true
            }
        ],
*/
        [311, {}],
/*
        [623, {}],
        [621, {}],
        [192, {}],
        [630, {}],
        [631, {}],

        [506, {}],
*/
    ]);
};
