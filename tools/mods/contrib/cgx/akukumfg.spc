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

let g_ProductName = "akuku";

#include "dgxmfg.spc"

// ********************************************************
//              General functions
// ********************************************************

function HandleGpuPostInitArgs_cgxcommon()
{
    let rc = OK;
    CHECK_RC(HandleGpuPostInitArgs());
    if (g_SpecFunc === "pexTestSpec_connectivity")
    {
        for (let devInst = 0; devInst < TestDeviceMgr.NumDevices; devInst++)
        {
            let testdevice = new TestDevice(devInst);
            let subdev = new GpuSubdevice(testdevice);
            if (testdevice.Type === TestDeviceConst.TYPE_LWIDIA_GPU)
            {
                CHECK_RC(ArgCheckLinkSpeed(testdevice.DevInst, "0", 8000));
                CHECK_RC(ArgCheckLinkSpeed(testdevice.DevInst, "1", 8000));
                CHECK_RC(ArgCheckLinkWidth(testdevice.DevInst, "0", 16, 16));
                CHECK_RC(ArgCheckLinkWidth(testdevice.DevInst, "1", 16, 16));
                // For Constellation GPUs
                if (subdev.PciDeviceId === 0x1bb1)
                {
                    CHECK_RC(ArgCheckLinkSpeed(testdevice.DevInst, "2", 8000));
                    CHECK_RC(ArgCheckLinkSpeed(testdevice.DevInst, "3", 8000));
                    CHECK_RC(ArgCheckLinkWidth(testdevice.DevInst, "2", 8, 8));
                    CHECK_RC(ArgCheckLinkWidth(testdevice.DevInst, "3", 8, 8));
                }
            }
        }
    }
    else if (g_SpecFunc === "thermalTestSpec_gpus_conlwrrent")
    {
        for (let devInst = 0; devInst < TestDeviceMgr.NumDevices; devInst++)
        {
            let testdevice = new TestDevice(devInst);
            CHECK_RC(ArgBgThermMon(testdevice.DevInst, 1000, 1000));
            CHECK_RC(ArgBgTotalPowerLogger(testdevice.DevInst, 1000, 1000));
            // TODO MODSDGX-573 - 10000 is a WAR and needs to replaced with higher precision (1000ms) print/read interval
            CHECK_RC(ArgBgClocksLogger(testdevice.DevInst, "Gpu.ClkGpc", 10000, 10000));
            CHECK_RC(ArgBgClocksLogger(testdevice.DevInst, "Gpu.ClkM", 10000, 10000));
        }
    }
    return rc;
}
Gpu.PostInitCallback = "HandleGpuPostInitArgs_cgxcommon";

function thermalTestSpec_gpus_conlwrrent(spec, PerfPtList)
{
    spec.AddTests(
    [
         [ "SetPState",             { InfPts: g_DefaultPerfPt }]
        ,[ "LwdaLinpackHMMAgemm",   { RuntimeMs: 90 * 60 * 1000,
                                      Verbose: true,
                                      SynchronousMode: true,
                                      VerifyResults: false }]
    ]);
}

function gpuTestSpec_power(spec, PerfPtList)
{
    spec.AddTests(
    [
        // Msize not specified so that test can automatically pick
        ["LwdaLinpackHMMAgemm", { Nsize: 10240, Ksize: 10240,
                                  RuntimeMs: 16*60*1000, PrintPerf: true }]
       ,["LwdaLinpackHMMAgemm", { Nsize: 10240, Ksize: 10240,
                                  RuntimeMs: 15*60*1000, PrintPerf: true,
                                  SynchronousMode : 1, LaunchDelayUs : 200 }]
    ]);
}
