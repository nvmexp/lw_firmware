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

g_SpecArgs  = "-null_display"

//------------------------------------------------------------------------------
function userSpec(testList, perfPoints)
{
    testList.AddTest("SetPState", { InfPts: perfPoints[0] });
    testList.AddTests([
        "VkStress",
        "VkHeatStress",
        "VkPowerStress",
        "VkStressMesh",
        ["VkFusion", {"Raytracing" : {"RunMask" : 1 }}],
        "VkStressSCC",
        "VkStressCompute",
        "GLStressZ",
        "GLComputeTest",
        "GlrY8",
        "GlrFsaa2x",
        "LwdaTTUStress",
        ["NewLwdaMatsTest", {"MaxFbMb" : 512}],
        "CaskLinpackIMMAgemm",
        "CaskLinpackHMMAgemm",
        "CaskLinpackPulseIMMAgemm",
        "CaskLinpackPulseHMMAgemm",
        "CaskLinpackPulseDMMAgemm",
        "CaskLinpackSparseHMMA",
        "TegraLwOfa",
        ["TegraLwEnc", {SkipLoops : "3,7"}], // Skipping VP9 streams
        ["TegraLwdec", {"SkipStreams" : "case_*|AV1_MAIN10_*"}],
        "TegraLwJpg"
    ]);
}

var g_SpecFileIsSupported = SpecFileIsSupportedSocOnly;
