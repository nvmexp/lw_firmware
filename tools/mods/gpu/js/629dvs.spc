function Diag629Spec(testList, PerfPtList)
{
    testList.TestMode = 128;

    testList.AddTest("CheckConfig", g_Test1Args);

    if (g_BoardHasDynamicPageRetirement)
    {
        testList.AddTest("CheckInfoROM", { DprPBLTest : true, PBLCapacity : 63,
                                           L1EccDbeTest : true, L2EccDbeTest : true,
                                           SmEccDbeTest : true, TexUncorrTest : true });
    }
    else
    {
        testList.AddTest("CheckInfoROM", { EccDbeTest : true });
    }

    testList.AddTests(
    [
        [ "ValidSkuCheck", g_ValidSkuArgs ]
      , [ "ValidSkuCheck2", g_ValidSkuArgs2 ]
      , [ "Bar1RemapperTest", {} ]
      , [ "CpyEngTest", {} ]
      , [ "LwdaL2Mats", { Iterations : 10 } ]
      , [ "LwdaLdgTest", { MaxFbMb : 10 } ]
      , [ "LwdaLinpackSgemm", { RuntimeMs : 10000 } ]
      , [ "LwdaLinpackDgemm", { RuntimeMs : 10000 } ]
      , [ "LwdaLinpackPulseSP", { RuntimeMs : 10000 } ]
      , [ "LwdaLinpackPulseDP", { RuntimeMs : 10000 } ]
      , [ "CaskLinpackSgemm", { RuntimeMs : 10000 } ]
      , [ "CaskLinpackDgemm", { RuntimeMs : 10000 } ]
      , [ "CaskLinpackPulseSgemm", { RuntimeMs : 10000 } ]
      , [ "CaskLinpackPulseDgemm", { RuntimeMs : 10000 } ]
      , [ "LwdaMatsPatCombi", { Iterations : 10 } ]
      , [ "LwdaRadixSortTest", { DirtyExitOnFail: true, TestConfiguration: { TimeoutMs: 320000 } } ]
      , [ "LwdaRandom", {} ]
      , [ "LwdaStress2", {} ]
      , [ "LwdaXbar", {} ]
      , [ "DeepIdleStress", {} ]
      , [ "DPStressTest", {} ]
      , [ "Elpg", {} ]
      , [ "ElpgGraphicsStress", {} ]
      , [ "FastMatsTest", {} ]
      , [ "I2MTest", {} ]
      , [ "IntAzaliaLoopback", {} ]
      , [ "LineTest", {} ]
      , [ "MMERandomTest", {} ]
      , [ "NewLwdaMatsTest", {} ]
      , [ "NewWfMats", {} ]
      , [ "NewWfMatsNarrow", {} ]
      , [ "NewWfMatsCEOnly", {} ]
      , [ "NewWfMatsShort", {} ]
      , [ "PcieSpeedChange", g_PcieSpeedChangeArgs ]
      , [ "PexBandwidth", { IgnoreBwCheck : true } ]
      , [ "Random2dFb", {} ]
      , [ "Random2dNc", {} ]
      , [ "WfMatsMedium", {} ]
    ]);
    
    if (!Platform.IsPPC)
    {
       testList.AddTest("CheckDma", { SkipMissingPstates : true, NoP2P : true });
    }

    if (g_BoardHasDisplay)
    {
        testList.AddTestsWithParallelGroup(PG_DISPLAY,
        [
            [ "EvoLwrs", {} ]
          , [ "EvoOvrl", {} ]
          , [ "LwDisplayRandom", {}]
          , [ "LwDisplayHtol", {} ]
          , [ "SorLoopback", {} ]
        ],
        {"display":true});
    }

    if (!g_SeuSkipGl)
    {
        testList.AddTestsWithParallelGroup(PG_GRAPHICS,
        [
            [ "GlrA8R8G8B8", {} ]
          , [ "GlrFsaa2x", {} ]
          , [ "GlrFsaa4x", {} ]
          , [ "GlrMrtRgbU", {} ]
          , [ "GlrMrtRgbF", {} ]
          , [ "GlrY8", {} ]
          , [ "GlrR5G6B5", {} ]
          , [ "GlrFsaa8x", {} ]
          , [ "GlrFsaa4v4", {} ]
          , [ "GlrFsaa8v8", {} ]
          , [ "GlrFsaa8v24", {} ]
          , [ "GLComputeTest", {} ]
          , [ "GLStressFbPulse", {} ]
          , [ "GLStress", {} ]
          , [ "GLStressZ", {} ]
          , [ "GLStressPulse", {} ]
          , [ "GLRandomCtxSw", {} ]
          , [ "GLInfernoTest", {} ]
        ]);
        testList.AddTest("SyncParallelTests", {"ParallelId" : PG_GRAPHICS});
    }
    if (g_BoardHasDisplay)
    {
        testList.AddTest("SyncParallelTests", {"ParallelId" : PG_DISPLAY});
    }
}
