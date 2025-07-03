/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file elpg.cpp
 * @brief Engine Level Power Gating test
 *
 *
 */
#include "core/include/rc.h"
#include "core/include/jscript.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "gpu/include/gpudev.h"
#include "gpu/perf/pmusub.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "class/cl502d.h" // TWOD class
#include "class/cl86b6.h" // Video compositor
#include "class/cla06fsubch.h"
#include "ctrl/ctrl2080/ctrl2080mc.h"
#include "gpu/include/gralloc.h"
#include "core/include/fpicker.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/perf/pwrwait.h"
#include "core/include/jsonlog.h"

class Elpg: public GpuTest
{

public:
    Elpg();
    ~Elpg();

    virtual bool IsSupported();
    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();
    RC           SetDefaultsForPicker(UINT32 Idx);

    SETGET_PROP(ElpgMask, UINT32);
    SETGET_PROP(PwrRailMask, UINT32);
    SETGET_PROP(GrIdleThresh, UINT32);
    SETGET_PROP(GrPostPowerupThresh, UINT32);
    SETGET_PROP(VidIdleThresh, UINT32);
    SETGET_PROP(VidPostPowerupThresh, UINT32);
    SETGET_PROP(VicIdleThresh, UINT32);
    SETGET_PROP(VicPostPowerupThresh, UINT32);
    SETGET_PROP(DefaultPgOnTimeoutMs, UINT32);
    SETGET_PROP(DefaultPgOffTimeoutMs, UINT32);
    SETGET_PROP(DefaultPrOnTimeoutMs, UINT32);
    SETGET_PROP(DefaultPrOffTimeoutMs, UINT32);
    SETGET_PROP(PrintPowerEvents, bool);
    SETGET_PROP(EnableDeepIdle, bool);
    SETGET_PROP(DeepIdleTimeoutMs, FLOAT64);
    SETGET_PROP(ShowDeepIdleStats, bool);

private:
    enum
    {
        s_TwoD  =  LWA06F_SUBCHANNEL_2D, // = 3
        s_VIC   =  4,
    };

    // not sure if we will need his in the future.
    // These numbers are picked in a way that it would pass in simulation
    enum ElpgWaitTime
    {
        PG_OFF_TIMEOUT_SEC = 30,
        PG_ON_TIMEOUT_SEC = 10,
        PR_OFF_TIMEOUT_SEC = 10,     // not sure what a good time is yet..
        PR_ON_TIMEOUT_SEC = 10,
    };

    RC VariableInit();
    RC ElpgSetup();
    RC EngineSetup();
    RC SendGrMethods();
    RC SendVicMethods();
    bool ShouldSkip(UINT32 Engine);

    RC RunElpgTests();

    RC GrMethodElpgTest(bool bDeepIdle);
    RC GrPrivRegElpgTest(bool bDeepIdle);
    RC VicMethodElpgTest(bool bDeepIdle);
    RC VicPrivRegElpgTest(bool bDeepIdle);
    RC PowerRailGatePrivRegTest();
    RC PowerRailGateMethodTest();
    RC PowerWait(PMU::PgTypes Type, bool bDeepIdle);

    bool            m_PrintPowerEvents;
    bool            m_EnableDeepIdle;
    FLOAT64         m_DeepIdleTimeoutMs;
    bool            m_ShowDeepIdleStats;
    bool            m_RmEnableGr;
    bool            m_RmEnableVic;
    bool            m_RmEnablePwrRail;
    bool            m_VariableInitDone;
    bool            m_PmuElpgEnabled;
    ElpgWait       *m_pGrElpgWait;
    ElpgWait       *m_pVicElpgWait;
    ElpgWait       *m_pPwrRailWait;

    // Channels and Handles for the Graphics/Video engine
    Channel        *m_pTwodCh;
    Channel        *m_pVicCh;
    LwRm::Handle    m_hTwodCh;
    LwRm::Handle    m_hVicCh;

    ModsEvent      *m_pElpgEvent;

    // Thresholds to set for ELPG
    UINT32          m_ElpgMask;
    UINT32          m_PwrRailMask;
    UINT32          m_GrIdleThresh;
    UINT32          m_GrPostPowerupThresh;
    UINT32          m_VidIdleThresh;
    UINT32          m_VidPostPowerupThresh;
    UINT32          m_VicIdleThresh;
    UINT32          m_VicPostPowerupThresh;
    // Original thresholds - need to restore these
    vector<LW2080_CTRL_POWERGATING_PARAMETER> m_OrgThresholds;

    FLOAT64         m_PgOnTimeoutMs;
    FLOAT64         m_PgOffTimeoutMs;
    FLOAT64         m_PrOnTimeoutMs;
    FLOAT64         m_PrOffTimeoutMs;
    UINT32          m_DefaultPgOnTimeoutMs;
    UINT32          m_DefaultPgOffTimeoutMs;
    UINT32          m_DefaultPrOnTimeoutMs;
    UINT32          m_DefaultPrOffTimeoutMs;

    UINT32          m_SimEmulationFactor;
    PMU            *m_pPmu;

    TwodAlloc       m_TwoD;
    VideoCompositorAlloc m_Vic;

    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray       *m_pPickers;
    ElpgOwner               m_ElpgOwner;
};
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Elpg, GpuTest, "Elpg Test");

CLASS_PROP_READWRITE(Elpg, ElpgMask, UINT32,
                     "UINT32: Each bit represents an engine that is enabled for Engine level gating test");
CLASS_PROP_READWRITE(Elpg, PwrRailMask, UINT32,
                     "UINT32: Each bit represents an engine that is enabled for Power Rail testing");
CLASS_PROP_READWRITE(Elpg, GrIdleThresh, UINT32,
                     "UINT32: The number of clock cycles that the engine needs o be idle before power gating is triggered");
CLASS_PROP_READWRITE(Elpg, GrPostPowerupThresh, UINT32,
                     "UINT32: The number of clock cycles after engine power-up before checking for idle");
CLASS_PROP_READWRITE(Elpg, VidIdleThresh, UINT32,
                     "UINT32: The number of clock cycles that the engine needs o be idle before power gating is triggered");
CLASS_PROP_READWRITE(Elpg, VidPostPowerupThresh, UINT32,
                     "UINT32: The number of clock cycles after engine power-up before checking for idle");
CLASS_PROP_READWRITE(Elpg, VicIdleThresh, UINT32,
                     "UINT32: The number of clock cycles that the engine needs o be idle before power gating is triggered");
CLASS_PROP_READWRITE(Elpg, VicPostPowerupThresh, UINT32,
                     "UINT32: The number of clock cycles after engine power-up before checking for idle");
CLASS_PROP_READWRITE(Elpg, PrintPowerEvents, bool,
                     "bool: Print power events summary");
CLASS_PROP_READWRITE(Elpg, EnableDeepIdle, bool,
                     "bool: Enable deep idle transitions");
CLASS_PROP_READWRITE(Elpg, DeepIdleTimeoutMs, FLOAT64,
                     "UINT32: Set the timeout for entering deep idle");
CLASS_PROP_READWRITE(Elpg, ShowDeepIdleStats, bool,
                     "bool: Show deep idle statistics for each transition.");
CLASS_PROP_READWRITE(Elpg, DefaultPgOnTimeoutMs, UINT32,
                     "UINT32: Default Power gate On TimeoutMs.");
CLASS_PROP_READWRITE(Elpg, DefaultPgOffTimeoutMs, UINT32,
                     "UINT32: Default Power gate Off TimeoutMs.");
CLASS_PROP_READWRITE(Elpg, DefaultPrOnTimeoutMs, UINT32,
                     "UINT32: Default Power rail gating On TimeoutMs.");
CLASS_PROP_READWRITE(Elpg, DefaultPrOffTimeoutMs, UINT32,
                     "UINT32: Default Power rail gating Off TimeoutMs.");
//------------------------------------------------------------------------------
Elpg::Elpg()
 :  m_PrintPowerEvents(false),
    m_EnableDeepIdle(false),
    m_DeepIdleTimeoutMs(5000),
    m_ShowDeepIdleStats(false),
    m_RmEnableGr(false),
    m_RmEnableVic(false),
    m_RmEnablePwrRail(false),
    m_VariableInitDone(false),
    m_PmuElpgEnabled(false),
    m_pGrElpgWait(nullptr),
    m_pVicElpgWait(nullptr),
    m_pPwrRailWait(nullptr),
    m_pTwodCh(nullptr),
    m_pVicCh(nullptr),
    m_hTwodCh(0),
    m_hVicCh(0),
    m_pElpgEvent(nullptr),
    m_ElpgMask(0xFFFFFFFF),
    m_PwrRailMask(0xFFFFFFFF),
    m_GrIdleThresh(10000000),
    m_GrPostPowerupThresh(40000000),
    m_VidIdleThresh(10000000),
    m_VidPostPowerupThresh(10000000),
    m_VicIdleThresh(10000000),
    m_VicPostPowerupThresh(40000000),
    m_PgOnTimeoutMs(0U),
    m_PgOffTimeoutMs(0U),
    m_PrOnTimeoutMs(0U),
    m_PrOffTimeoutMs(0U),
    m_DefaultPgOnTimeoutMs(PG_ON_TIMEOUT_SEC*1000),
    m_DefaultPgOffTimeoutMs(PG_OFF_TIMEOUT_SEC*1000),
    m_DefaultPrOnTimeoutMs(PR_ON_TIMEOUT_SEC*1000),
    m_DefaultPrOffTimeoutMs(PR_OFF_TIMEOUT_SEC*1000),
    m_SimEmulationFactor(1),
    m_pPmu(nullptr)
{
    SetName("Elpg");
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_ELPGTEST_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}
//------------------------------------------------------------------------------
Elpg::~Elpg()
{
}
//------------------------------------------------------------------------------
bool Elpg::IsSupported()
{
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    if (!pSubdevice->HasFeature(Device::GPUSUB_HAS_ELPG))
        return false;

    //--------------------------
    // TODO for Keplar:
    // Make sure _FALCON_DEBUG1 register addresses are not changed! or else
    // video ELGP WILL Not unpower-gate on priv-reg access
    // -------------------------

    if (OK != VariableInit())
        return false;

    bool HasSupportedEngines = false;
    if (m_RmEnableGr || m_RmEnableVic)
    {
        HasSupportedEngines = true;
    }

    if (!HasSupportedEngines)
        return false;

    if (m_EnableDeepIdle && !m_pPmu->IsDeepIdleSupported())
    {
        Printf(Tee::PriNormal,
               "Warning: Deep Idle requested but not supported\n");
    }

    if (m_pPmu->IsElpgOwned())
    {
        Printf(Tee::PriNormal, "ELPG is lwrrently in use\n");
        return false;
    }
    return true;
}
//------------------------------------------------------------------------------
RC Elpg::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());
    CHECK_RC(VariableInit());
    CHECK_RC(EngineSetup());
    CHECK_RC(ElpgSetup());
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::Run()
{
    RC rc;
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams;
    UINT32 startGrPGCount = 0;
    UINT32 startVicPGCount = 0;
    UINT32 startPwrRailGateCount = 0;

    if (m_PrintPowerEvents || m_TestConfig.Verbose())
    {

        if (m_RmEnableGr)
        {
            LW2080_CTRL_POWERGATING_PARAMETER Param = {0};
            Param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
            Param.parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;
            pgParams.push_back(Param);
        }
        if (m_RmEnableVic)
        {
            LW2080_CTRL_POWERGATING_PARAMETER Param = {0};
            Param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
            Param.parameterExtended = PMU::ELPG_VIC_ENGINE;
            pgParams.push_back(Param);
        }
        if (m_RmEnablePwrRail)
        {
            LW2080_CTRL_POWERGATING_PARAMETER Param = {0};
            Param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_GATINGCOUNT;
            pgParams.push_back(Param);
        }

        // Get the starting power gate count for the test
        CHECK_RC(m_pPmu->GetPowerGatingParameters(&pgParams));

        for (UINT32 i=0; i<pgParams.size(); i++)
        {
            switch (pgParams[i].parameter)
            {
                case LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT:
                    switch (pgParams[i].parameterExtended)
                    {
                        case PMU::ELPG_GRAPHICS_ENGINE:
                            startGrPGCount = pgParams[i].parameterValue;
                            break;
                        case PMU::ELPG_VIC_ENGINE:
                            startVicPGCount = pgParams[i].parameterValue;
                            break;
                    }
                    break;
                case LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_GATINGCOUNT:
                    startPwrRailGateCount = pgParams[i].parameterValue;
                    break;
            }
        }
    }

    //  yield in case the PMU queue hasn't flushed
    Tasker::Yield();

    CHECK_RC(RunElpgTests());

    if ((rc == OK) && (m_PrintPowerEvents || m_TestConfig.Verbose()))
    {
        // Get the ending power gating count
        CHECK_RC(m_pPmu->GetPowerGatingParameters(&pgParams));
        ElpgWait::PgStats PgOnStats;
        ElpgWait::PgStats PgOffStats;
        for (UINT32 i=0; i<pgParams.size(); i++)
        {
            switch (pgParams[i].parameter)
            {
                case LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT:
                    switch (pgParams[i].parameterExtended)
                    {
                        case PMU::ELPG_GRAPHICS_ENGINE:
                            Printf(Tee::PriNormal, "%s : %d graphics power gating events oclwrred\n",
                                GetName().c_str(), (UINT32)(pgParams[i].parameterValue - startGrPGCount));

                            m_pGrElpgWait->GetPgStats(&PgOnStats, &PgOffStats);
                            break;
                        case PMU::ELPG_VIC_ENGINE:
                            Printf(Tee::PriNormal, "%s : %d VIC power gating events oclwrred\n",
                                GetName().c_str(), (UINT32)(pgParams[i].parameterValue - startVicPGCount));
                            m_pVicElpgWait->GetPgStats(&PgOnStats, &PgOffStats);
                            break;
                    }
                    break;
                case LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_GATINGCOUNT:
                    Printf(Tee::PriNormal, "%s : %d Pwr Rail gating events oclwrred\n",
                        GetName().c_str(), (UINT32)(pgParams[i].parameterValue - startPwrRailGateCount));
                    m_pPwrRailWait->GetPgStats(&PgOnStats, &PgOffStats);
                    break;
            }
            if (PgOnStats.Count > 0)
            {
                Printf(Tee::PriNormal, "%s PgOnStats(in ms) : min:%5lld max:%5lld avg:%5lld\n",
                    GetName().c_str(),
                    PgOnStats.MinMs,
                    PgOnStats.MaxMs,
                    PgOnStats.TotalMs/PgOnStats.Count);
            }

            if (PgOffStats.Count > 0)
            {
                Printf(Tee::PriNormal, "%s PgOffStats(in ms): min:%5lld max:%5lld avg:%5lld\n",
                    GetName().c_str(),
                    PgOffStats.MinMs,
                    PgOffStats.MaxMs,
                    PgOffStats.TotalMs/PgOffStats.Count);
            }
        }

        if (m_EnableDeepIdle)
        {
            LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS endStats = { 0 };
            CHECK_RC(m_pPmu->GetDeepIdleStatistics(PMU::DEEP_IDLE_NO_HEADS,
                                                   &endStats));
            Printf(Tee::PriNormal,
                   "%s : Deep Idle Stats - Attempts (%d) Entries (%d), Exits(%d)\n",
                   GetName().c_str(),
                   (UINT32)endStats.attempts, (UINT32)endStats.entries,
                   (UINT32)endStats.exits);
        }
    }
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::RunElpgTests()
{
    RC rc;
    UINT32 StartLoop        = m_TestConfig.StartLoop();
    UINT32 EndLoop          = StartLoop + m_TestConfig.Loops();
    UINT32 RestartSkipCount = m_TestConfig.RestartSkipCount();

    // If the optimized scheme (skip full reset) for elpg is enabled, then we
    // will get a deny request where a gr ucode reload will be required. Ucode
    // reload only happens at first GR channel allocation.
    // There are 2 ways around this
    // 1.Send work down the pipe (this will make ucode load before a pg_on)
    // 2.disable optimized reset scheme (set g_RmCtxSwPgReset = 1 in gpuargs.js)
    //   to ignore optimization for mods.
    // We are going with option 1.
    if (!ShouldSkip(FPK_ELPGTEST_ENGINE_gr))
        CHECK_RC(SendGrMethods());
    if (!ShouldSkip(FPK_ELPGTEST_ENGINE_vic))
        CHECK_RC(SendVicMethods());

    for (m_pFpCtx->LoopNum = StartLoop;
           m_pFpCtx->LoopNum < EndLoop;
           ++m_pFpCtx->LoopNum)
    {
         // really doubt we'll have too many loops of this test
         if ((m_pFpCtx->LoopNum == StartLoop) ||
             (m_pFpCtx->LoopNum % RestartSkipCount == 0))
         {
             m_pFpCtx->Rand.SeedRandom(m_TestConfig.Seed() + m_pFpCtx->LoopNum);
             m_pFpCtx->RestartNum = m_pFpCtx->LoopNum/RestartSkipCount;
             m_pPickers->Restart();
         }

         Printf(Tee::PriLow, "Loop %d\n", m_pFpCtx->LoopNum);
         const UINT32 Test = (*m_pPickers)[FPK_ELPGTEST_SUBTEST].Pick();
         bool   bDeepIdle = false;

         if (m_EnableDeepIdle)
         {
             bDeepIdle = ((*m_pPickers)[FPK_ELPGTEST_DEEPIDLE].Pick() ==
                          FPK_ELPGTEST_DEEPIDLE_on);
         }
         switch (Test)
         {
             case FPK_ELPGTEST_SUBTEST_grmethods:
                 rc = GrMethodElpgTest(bDeepIdle);
                 break;
             case FPK_ELPGTEST_SUBTEST_grprivreg:
                 rc = GrPrivRegElpgTest(bDeepIdle);
                 break;
             case FPK_ELPGTEST_SUBTEST_vicmethods:
                 rc = VicMethodElpgTest(bDeepIdle);
                 break;
             case FPK_ELPGTEST_SUBTEST_vicprivreg:
                 rc = VicPrivRegElpgTest(bDeepIdle);
                 break;
             case FPK_ELPGTEST_SUBTEST_pwrrailreg:
                 rc = PowerRailGatePrivRegTest();
                 break;
             case FPK_ELPGTEST_SUBTEST_pwrrailmethods:
                 rc = PowerRailGateMethodTest();
                 break;
         }
         if (OK != rc)
         {
            GetJsonExit()->AddFailLoop(m_pFpCtx->LoopNum);
            return rc;
         }
    }
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::Cleanup()
{
    Printf(Tee::PriLow, "Elpg::Cleanup\n");
    if (m_pTwodCh)
    {
        m_TwoD.Free();
        m_TestConfig.FreeChannel(m_pTwodCh);
        m_pTwodCh = 0;
        m_hTwodCh = 0;
    }
    if (m_pVicCh)
    {
        m_Vic.Free();
        m_TestConfig.FreeChannel(m_pVicCh);
        m_pVicCh = 0;
        m_hVicCh = 0;
    }

    if (m_RmEnableGr)
    {
        delete m_pGrElpgWait;
        m_pGrElpgWait = NULL;
    }
    if (m_RmEnableVic)
    {
        delete m_pVicElpgWait;
        m_pVicElpgWait = NULL;
    }
    if (m_RmEnablePwrRail)
    {
        delete m_pPwrRailWait;
        m_pPwrRailWait = NULL;
    }

    // restore old threshold values
    if (m_OrgThresholds.size())
    {
        UINT32 Flags = 0;
        Flags = FLD_SET_DRF(2080, _CTRL_MC_SET_POWERGATING_THRESHOLD,
                            _FLAGS_BLOCKING, _TRUE, Flags);
        m_pPmu->SetPowerGatingParameters(&m_OrgThresholds, Flags);
        m_OrgThresholds.clear();
    }
    m_ElpgOwner.ReleaseElpg();
    m_pPmu->FreeEvent(m_pElpgEvent);
    m_pPmu = NULL;
    return GpuTest::Cleanup();
}
//------------------------------------------------------------------------------
RC Elpg::SetDefaultsForPicker(UINT32 Idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &Fp = (*pPickers)[Idx];
    switch (Idx)
    {
        case FPK_ELPGTEST_SUBTEST:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_ELPGTEST_SUBTEST_grmethods);
            Fp.AddRandItem(1, FPK_ELPGTEST_SUBTEST_grprivreg);
            Fp.AddRandItem(1, FPK_ELPGTEST_SUBTEST_vicmethods);
            Fp.AddRandItem(1, FPK_ELPGTEST_SUBTEST_vicprivreg);
            // one for each of Gr/Vid/Vic
            Fp.AddRandItem(3, FPK_ELPGTEST_SUBTEST_pwrrailreg);
            Fp.AddRandItem(3, FPK_ELPGTEST_SUBTEST_pwrrailmethods);
            Fp.CompileRandom();
            break;
        case FPK_ELPGTEST_VIDEOENGINE:
            Fp.ConfigConst(0);
            break;
        case FPK_ELPGTEST_DEEPIDLE:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_ELPGTEST_DEEPIDLE_off);
            Fp.AddRandItem(1, FPK_ELPGTEST_DEEPIDLE_on);
            Fp.CompileRandom();
            break;
        case FPK_ELPGTEST_ENGINE:
            Fp.ConfigRandom();
            Fp.AddRandItem(1, FPK_ELPGTEST_ENGINE_gr);
            Fp.AddRandItem(1, FPK_ELPGTEST_ENGINE_vic);
            Fp.CompileRandom();
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}
//------------------------------------------------------------------------------
RC Elpg::VariableInit()
{
    RC rc;
    if (m_VariableInitDone)
        return OK;

    CHECK_RC(GetBoundGpuSubdevice()->GetPmu(&m_pPmu));
    UINT32 SupportedMask;
    CHECK_RC(m_pPmu->GetPowerGateSupportEngineMask(&SupportedMask));
    m_RmEnableGr  = (SupportedMask & (1<<PMU::ELPG_GRAPHICS_ENGINE)) &&
                    m_TwoD.IsSupported(GetBoundGpuDevice());
    m_RmEnableVic = (SupportedMask & (1<<PMU::ELPG_VIC_ENGINE)) &&
                    m_Vic.IsSupported(GetBoundGpuDevice());
    Printf(GetVerbosePrintPri(),
           "ELPG supported: (Gr,Vic)=(%d,%d)\n",
           m_RmEnableGr, m_RmEnableVic);

    vector<LW2080_CTRL_POWERGATING_PARAMETER> Param(1);
    Param[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWRRAIL_SUPPORTED;
    if (OK == m_pPmu->GetPowerGatingParameters(&Param))
    {
        m_RmEnablePwrRail = (Param[0].parameterValue != 0);
        Printf(GetVerbosePrintPri(),
               "Power Rail supported = %d\n", m_RmEnablePwrRail);
    }
    Param[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_PMU_ELPG_ENABLED;
    CHECK_RC(m_pPmu->GetPowerGatingParameters(&Param));
    m_PmuElpgEnabled  = (Param[0].parameterValue != 0);

    if (m_PmuElpgEnabled)
    {
        Printf(Tee::PriLow, "PMU ELPG is enabled.\n");
    }

    m_VariableInitDone = true;
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::ElpgSetup()
{
    RC rc;

    if (GetBoundGpuSubdevice()->IsEmOrSim())
    {
        m_SimEmulationFactor = 2;
    }

    CHECK_RC(m_ElpgOwner.ClaimElpg(m_pPmu));

    m_PgOnTimeoutMs  = m_DefaultPgOnTimeoutMs*m_SimEmulationFactor;
    m_PgOffTimeoutMs = m_DefaultPgOffTimeoutMs*m_SimEmulationFactor;
    m_PrOnTimeoutMs  = m_DefaultPrOnTimeoutMs*m_SimEmulationFactor;
    m_PrOffTimeoutMs = m_DefaultPrOffTimeoutMs*m_SimEmulationFactor;

    LW2080_CTRL_POWERGATING_PARAMETER param = {0};

    // Get the number of engines
    param.parameterExtended = 0;
    param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES;
    m_OrgThresholds.push_back(param);
    CHECK_RC(m_pPmu->GetPowerGatingParameters(&m_OrgThresholds));

    UINT32 engineCount = m_OrgThresholds[0].parameterValue;
    for (UINT32 engine = 0; engine < engineCount; engine++)
    {
        memset(&param, 0, sizeof(param));
        param.parameterExtended = engine;
        param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
    }

    UINT32 EnabledEngines;
    CHECK_RC(m_pPmu->GetPowerGateEnableEngineMask(&EnabledEngines));

    m_OrgThresholds.clear();
    for (UINT32 engine = 0; engine < engineCount; engine++)
    {
        if (!(EnabledEngines & (1<<engine)))
            continue;
        param.parameterExtended = engine;
        param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD;
        m_OrgThresholds.push_back(param);
        param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD;
        m_OrgThresholds.push_back(param);
    }

    // save Old ELPG threshold values
    if (m_OrgThresholds.size())
    {
        CHECK_RC(m_pPmu->GetPowerGatingParameters(&m_OrgThresholds));
    }

    UINT32 PgFlags = 0;
    PgFlags = FLD_SET_DRF(2080, _CTRL_MC_SET_POWERGATING_THRESHOLD,
                          _FLAGS_BLOCKING, _TRUE, PgFlags);
    if (m_RmEnableGr)
    {
        vector<LW2080_CTRL_POWERGATING_PARAMETER> setParams;
        param.parameterExtended = PMU::ELPG_GRAPHICS_ENGINE;
        param.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD;
        param.parameterValue    = m_GrIdleThresh;
        setParams.push_back(param);
        param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD;
        param.parameterValue    = m_GrPostPowerupThresh;
        setParams.push_back(param);
        CHECK_RC(m_pPmu->SetPowerGatingParameters(&setParams, PgFlags));

        m_pGrElpgWait = new ElpgWait(GetBoundGpuSubdevice(),
                                     PMU::ELPG_GRAPHICS_ENGINE,
                                     m_TestConfig.Verbose());
        CHECK_RC(m_pGrElpgWait->Initialize());
    }
    if (m_RmEnableVic)
    {
        vector<LW2080_CTRL_POWERGATING_PARAMETER> setParams;
        param.parameterExtended = PMU::ELPG_VIC_ENGINE;
        param.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD;
        param.parameterValue    = m_VicIdleThresh;
        setParams.push_back(param);
        param.parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD;
        param.parameterValue    = m_VicPostPowerupThresh;
        setParams.push_back(param);
        CHECK_RC(m_pPmu->SetPowerGatingParameters(&setParams, PgFlags));

        m_pVicElpgWait = new ElpgWait(GetBoundGpuSubdevice(),
                                      PMU::ELPG_VIC_ENGINE,
                                      m_TestConfig.Verbose());
        CHECK_RC(m_pVicElpgWait->Initialize());
    }
    if (m_RmEnablePwrRail)
    {
        m_pPwrRailWait = new ElpgWait(GetBoundGpuSubdevice(),
                                      PMU::PWR_RAIL,
                                      m_TestConfig.Verbose());
        CHECK_RC(m_pPwrRailWait->Initialize());
    }

    m_pElpgEvent = m_pPmu->AllocEvent(PMU::EVENT_UNIT_ID, false, RM_PMU_UNIT_LPWR);
    MASSERT(m_pElpgEvent);

    if (m_EnableDeepIdle)
    {
        if (!m_pPmu->IsDeepIdleSupported())
            return RC::UNSUPPORTED_FUNCTION;

        CHECK_RC(m_pPmu->ResetDeepIdleStatistics(PMU::DEEP_IDLE_NO_HEADS));
    }
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::EngineSetup()
{
    RC rc;
    m_TestConfig.SetAllowMultipleChannels(true);

    // allocations for graphics engine
    if (m_RmEnableGr)
    {
        Printf(Tee::PriLow, "alloc TwoD object\n");
        CHECK_RC(m_TestConfig.AllocateChannelWithEngine(&m_pTwodCh,
                                                        &m_hTwodCh,
                                                        &m_TwoD));
        m_pTwodCh->SetObject(s_TwoD, m_TwoD.GetHandle());
    }

    if (m_RmEnableVic)
    {
        Printf(Tee::PriLow, "alloc VIC object\n");
        CHECK_RC(m_TestConfig.AllocateChannelWithEngine(&m_pVicCh,
                                                        &m_hVicCh,
                                                        &m_Vic));
        m_pVicCh->SetObject(s_VIC, m_Vic.GetHandle());
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Elpg::SendGrMethods()
{
    RC rc;
    Printf(GetVerbosePrintPri(), " Elpg::SendGrMethods\n");
    // arbitrarily send these methods
    m_pTwodCh->Write(s_TwoD, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT,
                 LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32);
    m_pTwodCh->Write(s_TwoD, LW502D_SET_MONOCHROME_PATTERN_FORMAT,
                 LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_CGA6_M1);
    m_pTwodCh->Write(s_TwoD, LW502D_SET_PATTERN_SELECT,
                 LW502D_SET_PATTERN_SELECT_V_MONOCHROME_8x8);
    m_pTwodCh->Write(s_TwoD, LW502D_NO_OPERATION, 0);
    CHECK_RC(m_pTwodCh->Flush());
    return rc;
}

//-----------------------------------------------------------------------------
// engine is picked by fancy picker
bool Elpg::ShouldSkip(UINT32 Engine)
{
    bool RetVal = false;
    switch (Engine)
    {
        case FPK_ELPGTEST_ENGINE_gr:
            if (!(m_ElpgMask & (1<<PMU::ELPG_GRAPHICS_ENGINE)) || !m_RmEnableGr)
                RetVal = true;
            break;
        case FPK_ELPGTEST_ENGINE_vic:
            if (!(m_ElpgMask & (1<<PMU::ELPG_VIC_ENGINE)) || !m_RmEnableVic)
                RetVal = true;
            break;
    }
    if (RetVal)
    {
        Printf(GetVerbosePrintPri(), "engine %d not supported\n", Engine);
    }

    return RetVal;
}
//------------------------------------------------------------------------------
RC Elpg::SendVicMethods()
{
    RC rc;
    Printf(GetVerbosePrintPri(), " Elpg::SendVicMethods\n");
    m_pVicCh->Write(s_VIC, LW86B6_VIDEO_COMPOSITOR_NOP, 0);
    CHECK_RC(m_pVicCh->Flush());
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::GrMethodElpgTest(bool bDeepIdle)
{
    RC rc;
    if (ShouldSkip(FPK_ELPGTEST_ENGINE_gr))
    {
        Printf(GetVerbosePrintPri(), " skipping GrMethodElpgTest\n");
        return OK;
    }
    Printf(GetVerbosePrintPri(), " Elpg::GrMethodElpgTest\n");

    CHECK_RC(PowerWait(PMU::ELPG_GRAPHICS_ENGINE, bDeepIdle));

    // send graphics method method -> ELPG should power on the system
    CHECK_RC(SendGrMethods());
    CHECK_RC(m_pGrElpgWait->Wait(ElpgWait::PG_OFF, m_PgOffTimeoutMs));
    // Look for power down
    CHECK_RC(m_pGrElpgWait->Wait(ElpgWait::PG_ON, m_PgOnTimeoutMs));
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::GrPrivRegElpgTest(bool bDeepIdle)
{
    RC rc;
    if (ShouldSkip(FPK_ELPGTEST_ENGINE_gr))
    {
        Printf(GetVerbosePrintPri(), " skipping GrPrivRegElpgTest\n");
        return OK;
    }
    Printf(GetVerbosePrintPri(), " Elpg::GrPrivRegElpgTest\n");
    CHECK_RC(PowerWait(PMU::ELPG_GRAPHICS_ENGINE, bDeepIdle));

    // attempt to wake up using register read
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    Printf(GetVerbosePrintPri(), "Elpg::Read to Gr PrivReg.\n");
    pSubdev->GetEngineStatus(GpuSubdevice::GR_ENGINE);

    CHECK_RC(m_pGrElpgWait->Wait(ElpgWait::PG_OFF, m_PgOffTimeoutMs));

    // ELPG should power off the engine
    CHECK_RC(m_pGrElpgWait->Wait(ElpgWait::PG_ON, m_PgOnTimeoutMs));
    return rc;
}

//------------------------------------------------------------------------------
RC Elpg::VicMethodElpgTest(bool bDeepIdle)
{
    RC rc;
    if (ShouldSkip(FPK_ELPGTEST_ENGINE_vic))
    {
        Printf(GetVerbosePrintPri(), " skipping VicMethodElpgTest\n");
        return OK;
    }
    Printf(GetVerbosePrintPri(), " Elpg::VicMethodElpgTest\n");
    CHECK_RC(PowerWait(PMU::ELPG_VIC_ENGINE, bDeepIdle));

    CHECK_RC(SendVicMethods());
    CHECK_RC(m_pVicElpgWait->Wait(ElpgWait::PG_OFF, m_PgOffTimeoutMs));

    // ELPG should power off the engine
    CHECK_RC(m_pVicElpgWait->Wait(ElpgWait::PG_ON, m_PgOnTimeoutMs));
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::VicPrivRegElpgTest(bool bDeepIdle)
{
    RC rc;
    if (ShouldSkip(FPK_ELPGTEST_ENGINE_vic))
    {
        Printf(GetVerbosePrintPri(), " skipping VicPrivRegElpgTest\n");
        return OK;
    }
    Printf(GetVerbosePrintPri(), " Elpg::VicPrivRegElpgTest\n");
    CHECK_RC(PowerWait(PMU::ELPG_VIC_ENGINE, bDeepIdle));

    // attempt to wake up using register read
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // hard-code here I think is okay - MCP89 is the only chip on the roadmap that
    // has VIC engine. not sure about Keplar yet..
    const UINT32 LW_PVIC_FALCON_DEBUG1 = 0x001c1090;
    UINT32 Debug1 = pSubdev->RegRd32(LW_PVIC_FALCON_DEBUG1);
    Printf(GetVerbosePrintPri(), "Elpg::Read VIC DEBUG1 = 0x%x\n", Debug1);

    CHECK_RC(m_pVicElpgWait->Wait(ElpgWait::PG_OFF, m_PgOffTimeoutMs));
    // ELPG should power off the engine
    CHECK_RC(m_pVicElpgWait->Wait(ElpgWait::PG_ON, m_PgOnTimeoutMs));
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::PowerRailGatePrivRegTest()
{
    RC rc;
    const UINT32 Engine = (*m_pPickers)[FPK_ELPGTEST_ENGINE].Pick();
    if (!m_PwrRailMask || !m_RmEnablePwrRail || ShouldSkip(Engine))
    {
        Printf(GetVerbosePrintPri(), " skipping PowerRailGatePrivRegTest\n");
        return OK;
    }
    Printf(GetVerbosePrintPri(), " Elpg::PowerRailGatePrivRegTest\n");
    // make sure we're powered down first
    CHECK_RC(m_pPwrRailWait->Wait(ElpgWait::PG_ON, m_PrOnTimeoutMs));

    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    const UINT32 LW_PVIC_FALCON_DEBUG1 = 0x001c1090;
    switch (Engine)
    {
        case FPK_ELPGTEST_ENGINE_gr:
            pSubdev->GetEngineStatus(GpuSubdevice::GR_ENGINE);
            break;
        case FPK_ELPGTEST_ENGINE_vic:
            pSubdev->RegRd32(LW_PVIC_FALCON_DEBUG1);
            break;
    }

    // wait for PowerRail to come back up:
    CHECK_RC(m_pPwrRailWait->Wait(ElpgWait::PG_OFF, m_PrOffTimeoutMs));
    // wait for Power rail to power down (= Power Rail gating on)
    CHECK_RC(m_pPwrRailWait->Wait(ElpgWait::PG_ON, m_PrOnTimeoutMs));
    return rc;
}
//------------------------------------------------------------------------------
RC Elpg::PowerRailGateMethodTest()
{
    RC rc;
    const UINT32 Engine = (*m_pPickers)[FPK_ELPGTEST_ENGINE].Pick();
    if (!m_PwrRailMask || !m_RmEnablePwrRail || ShouldSkip(Engine))
    {
        Printf(GetVerbosePrintPri(), " skipping PowerRailGateMethodTest\n");
        return OK;
    }
    Printf(GetVerbosePrintPri(), " Elpg::PowerRailGateMethodTest\n");
    // make sure we're powered down first
    CHECK_RC(m_pPwrRailWait->Wait(ElpgWait::PG_ON, m_PrOnTimeoutMs));
    switch (Engine)
    {
        case FPK_ELPGTEST_ENGINE_gr:
            CHECK_RC(SendGrMethods());
            break;
        case FPK_ELPGTEST_ENGINE_vic:
            CHECK_RC(SendVicMethods());
            break;
    }
    // wait for PowerRail to come back up:
    CHECK_RC(m_pPwrRailWait->Wait(ElpgWait::PG_OFF, m_PrOffTimeoutMs));
    // wait for Power rail to power down (= Power Rail gating on)
    CHECK_RC(m_pPwrRailWait->Wait(ElpgWait::PG_ON, m_PrOnTimeoutMs));
    return rc;
}
//------------------------------------------------------------------------------
//! Brief: Wait for the appropriate power event to occur (either power gating a
// particular engine when deep idle is not in use, or waiting for deep idle
// for deep idle transitions
RC Elpg::PowerWait(PMU::PgTypes Type, bool bDeepIdle)
{
    RC rc, firstRc;
    unique_ptr<PwrWait> pwrWait;
    PwrWait::Action WhatToWait = PwrWait::DEFAULT_ACT;
    FLOAT64 TimeoutMs = 0;
    if (bDeepIdle)
    {
        pwrWait.reset(new DeepIdleWait(GetBoundGpuSubdevice(),
                                       GetDisplay(),
                                       NULL,
                                       m_TestConfig.TimeoutMs(),
                                       m_DeepIdleTimeoutMs,
                                       m_ShowDeepIdleStats));
    }
    else
    {
        pwrWait.reset(new ElpgWait(GetBoundGpuSubdevice(),
                                  Type,
                                  m_TestConfig.Verbose()));
        WhatToWait = PwrWait::PG_ON;
        TimeoutMs  = m_PgOnTimeoutMs;
    }

    CHECK_RC(pwrWait->Initialize());
    FIRST_RC(pwrWait->Wait(WhatToWait, TimeoutMs));
    FIRST_RC(pwrWait->Teardown());

    return firstRc;
}

