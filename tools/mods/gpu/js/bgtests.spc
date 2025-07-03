g_SpecArgs = "";
g_TestMode = 64; // TM_DVS

function GLInfernoTest_BgTest(bgTest)
{
    return [["StartBgTest", {"BgTest":bgTest}]
            ,["GLInfernoTest", {}]
            ,["StopBgTests", {}]]
}

function LwdaLinpackPulse_BgTest(bgTest)
{
    return [["StartBgTest", {"BgTest":bgTest}]
            ,["LwdaLinpackPulseDP", {}]
            ,["StopBgTests", {}]]
}

function GpuTrace_BgTest(trace, bgTest)
{
    return [["StartBgTest", {"BgTest":bgTest}]
            ,["GpuTrace", {"Trace" : "/data/traces/maxwell_b/" + trace}]
            ,["StopBgTests", {}]]
}

function userSpec(testList, perfPoints/*unused*/)
{
    testList.TestMode = 64;
    testList.AddTest("SetPState", {"InfPts":{"PStateNum":0,"LocationStr":"max","UnsafeDefault":true,"InflectionPt":1}})
    
    //testList.AddTests(GpuTrace_BgTest("unigine_3.0", 4));
    testList.AddTests(GpuTrace_BgTest("unigine_3.0", 7));
    //testList.AddTests(GpuTrace_BgTest("unigine_3.0", 11));
    testList.AddTests(GpuTrace_BgTest("unigine_3.0", 102));
    testList.AddTests(GpuTrace_BgTest("unigine_3.0", 277));
    testList.AddTests(GpuTrace_BgTest("unigine_3.0", 278));
    
    //testList.AddTests(GpuTrace_BgTest("supertrace", 4));
    testList.AddTests(GpuTrace_BgTest("supertrace", 7));
    //testList.AddTests(GpuTrace_BgTest("supertrace", 11));
    testList.AddTests(GpuTrace_BgTest("supertrace", 102));
    testList.AddTests(GpuTrace_BgTest("supertrace", 277));
    testList.AddTests(GpuTrace_BgTest("supertrace", 278));
    
    testList.AddTest("StartDispatcher", {"Func":"SimTempCycle","Sequence":"30,55,70","Frequency":"500"});
    //testList.AddTests(GLInfernoTest_BgTest(4));
    testList.AddTests(GLInfernoTest_BgTest(7));
    //testList.AddTests(GLInfernoTest_BgTest(11));
    testList.AddTests(GLInfernoTest_BgTest(102));
    testList.AddTests(GLInfernoTest_BgTest(277));
    testList.AddTests(GLInfernoTest_BgTest(278));
    testList.AddTests(GLInfernoTest_BgTest(25));
    testList.AddTests(GLInfernoTest_BgTest(225));
    testList.AddTest("StopDispatcher", {});

    //testList.AddTests(LwdaLinpackPulse_BgTest(4));
    testList.AddTests(LwdaLinpackPulse_BgTest(7));
    //testList.AddTests(LwdaLinpackPulse_BgTest(11));
    testList.AddTests(LwdaLinpackPulse_BgTest(102));
    testList.AddTests(LwdaLinpackPulse_BgTest(277));
    testList.AddTests(LwdaLinpackPulse_BgTest(278));
    testList.AddTests(LwdaLinpackPulse_BgTest(25));
    testList.AddTests(LwdaLinpackPulse_BgTest(225));
}
