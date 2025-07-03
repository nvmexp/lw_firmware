/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

g_SpecArgs = "";
if (!Platform.IsPPC)
{
    g_SpecArgs += "-hot_reset";
}
g_TestMode = (1 << 2); // TM_MFG_BOARD

function addModsGpuTests(testList, perfPoints/*unused*/)
{
    testList.TestMode = (1 << 2); // TM_MFG_BOARD

    runTraffic(testList, perfPoints);
}

// ********************************************************
//              LwSwitch qual tests
// ********************************************************
function runLwSwitchQual(testList, perfPoints/*unused*/)
{
    testList.TestMode = (1 << 2); // TM_MFG_BOARD

    testList.AddTests(
    [
        [ "LwLinkEyeDiagram"/*248*/, {"TargetLwSwitch" : true}]
       ,[ "PcieEyeDiagram"/*249*/, {"TargetLwSwitch" : true}]
       ,[ "LwLinkBwStress"/*246*/, {}]
       ,[ "LwlbwsLowPower"/*64*/, {}]
       ,[ "LwlbwsThermalThrottling", {ThermalThrottlingOnCount : 8000, ThermalThrottlingOffCount : 16000}]
       ,[ "LwlbwsLpModeSwToggle"/*16*/, {}]
       ,[ "LwlbwsLpModeHwToggle"/*77*/, {}]
    ]);
}
