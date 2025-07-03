/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <math.h>
#include "gpu/tests/lwdastst.h"
#include "lwda_etbl/tools_rm.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "ctrl/ctrlc06f.h"
#include "class/cla06c.h"
#include "ctrl/ctrla06c.h"
#include "class/cla06fsubch.h"
#include "gpu/include/gralloc.h"
#include "lwRmApi.h"
#include "cilp/cilpstress.h"

class LwdaCILPStress : public LwdaStreamTest
{
    public:
        LwdaCILPStress();
        virtual ~LwdaCILPStress() = default;
        void PrintJsProperties(Tee::Priority) override;
        bool IsSupported() override;
        RC Setup() override;
        RC Run() override;
        RC Cleanup() override;

        SETGET_PROP(RuntimeMs, UINT32);
        SETGET_PROP(BgTestTimesliceUs, UINT32);
        SETGET_PROP(SupportTimesliceUs, UINT32);
        SETGET_PROP(BgTestPreemptionMode, UINT32);
        SETGET_PROP(SupportPreemptionMode, UINT32);
        SETGET_PROP(NumInnerIterations, UINT32);
        SETGET_PROP(NumBlocks, UINT32);
        SETGET_PROP(NumThreads, UINT32);
        SETGET_PROP(SMemPct, float);
        SETGET_PROP(DumpStats, bool);
        SETGET_PROP(FailureThreshold, UINT32);

    private:
        RC DumpCTXSWStats(UINT32 handleIndex, UINT32& targetPremeptionCount);
        RC SetPreemptionMode(UINT32 handleIndex, UINT32 mode);
        RC SetPreemptionTimeSlice(UINT32 handleIndex, UINT32 timeSlice);

        CILPParams m_Params = {};
        Lwca::Module m_Module;
        Lwca::Function m_RunFunc;
        Lwca::DeviceMemory m_InputData;
        Lwca::HostMemory m_OutputData;

        Random m_Random;
        UINT32 m_RuntimeMs = 0;
        UINT32 m_BgTestTimesliceUs = 200;
        UINT32 m_SupportTimesliceUs = 200;
        UINT32 m_BgTestPreemptionMode = 2;
        UINT32 m_SupportPreemptionMode = 2;
        UINT32 m_NumInnerIterations = 5000;
        UINT32 m_NumBlocks = 0;
        UINT32 m_NumThreads = 0;
        float m_SMemPct = 100;
        bool m_DumpStats = true;
        UINT32 m_NumElements = 0;
        UINT32 m_FailureThreshold = 10;

        // BgTest Handles at index 0 while Support kernel handles at index 1
        LwRm::Handle m_ChannelGrpHandles[2] = {0};
        LWtoolsContextHandlesRm m_CtxHandles[2] = {0};
};

JS_CLASS_INHERIT(LwdaCILPStress, LwdaStreamTest, "LwdaCILPStress Test");
CLASS_PROP_READWRITE(LwdaCILPStress, RuntimeMs, UINT32,
                     "Run the kernel for atleast the specified amount of time."
                     "If set to 0 TestConfiguration.Loops will be used.");
CLASS_PROP_READWRITE(LwdaCILPStress, BgTestTimesliceUs, UINT32,
                     "BgTest Timeslice in Microseconds after which Preemption triggers.");
CLASS_PROP_READWRITE(LwdaCILPStress, SupportTimesliceUs, UINT32,
                     "Support Kernel Timeslice in Microseconds after which Preemption triggers.");
CLASS_PROP_READWRITE(LwdaCILPStress, BgTestPreemptionMode, UINT32,
                     "BgTest Preemption Mode where 0 is WFI, 1 is CTA and 2 is CILP.");
CLASS_PROP_READWRITE(LwdaCILPStress, SupportPreemptionMode, UINT32,
                     "Support Kernel Preemption Mode where 0 is WFI, 1 is CTA and 2 is CILP.");
CLASS_PROP_READWRITE(LwdaCILPStress, NumInnerIterations, UINT32,
                     "Number of Support Kernel's Inner Iterations.");
CLASS_PROP_READWRITE(LwdaCILPStress, NumBlocks, UINT32,
                     "Number of CTA(s) per grid for the Support Kernel.");
CLASS_PROP_READWRITE(LwdaCILPStress, NumThreads, UINT32,
                     "Number of threads per LWCA block for the Support Kernel.");
CLASS_PROP_READWRITE(LwdaCILPStress, SMemPct, float,
                     "Percent from 0 to 100 of Maximum Shared Memory allocated per block for the Support Kernel.");
CLASS_PROP_READWRITE(LwdaCILPStress, DumpStats, bool,
                     "Print Detailed BgTest Context Switch Statistics.");
CLASS_PROP_READWRITE(LwdaCILPStress, FailureThreshold, UINT32,
                     "BgTest Preemption count under which the test would fail. Based on the BgTest Preemption Mode.");

LwdaCILPStress::LwdaCILPStress()
{
    SetName("LwdaCILPStress");
}

void LwdaCILPStress::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);
    Printf(pri, "LwdaCILPStress controls:\n");
    Printf(pri, "\tRuntimeMs:               %u\n", m_RuntimeMs);
    Printf(pri, "\tBgTestTimesliceUs:       %u\n", m_BgTestTimesliceUs);
    Printf(pri, "\tSupportTimesliceUs:      %u\n", m_SupportTimesliceUs);
    Printf(pri, "\tBgTestPreemptionMode:    %u\n", m_BgTestPreemptionMode);
    Printf(pri, "\tSupportPreemptionMode:   %u\n", m_SupportPreemptionMode);
    Printf(pri, "\tNumInnerIterations:      %u\n", m_NumInnerIterations);
    Printf(pri, "\tNumBlocks:               %u\n", m_NumBlocks);
    Printf(pri, "\tNumThreads:              %u\n", m_NumThreads);
    Printf(pri, "\tSMemPct:                 %f\n", m_SMemPct);
    Printf(pri, "\tDumpStats:               %s\n", m_DumpStats ? "true" : "false");
    Printf(pri, "\tFailureThreshold:        %u\n", m_FailureThreshold);
}

bool LwdaCILPStress::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_80)
    {
        Printf(Tee::PriLow, "LwdaCILPStress is not supported on this SM version\n");
        return false;
    }
    return true;
}

RC LwdaCILPStress::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    //Required to enable interrupts
    CHECK_RC(AllocDisplay());

    // Enabling multiple Contexts with the Primary Context as active
    CHECK_RC(GetLwdaInstance().InitContext(GetBoundLwdaDevice(), 1, 0));

    // Fetching handles for the Primary Context which controls the BgTest
    CHECK_RC(GetLwdaInstance().GetComputeEngineChannelGroup(&m_ChannelGrpHandles[0]));
    m_CtxHandles[0].struct_size = sizeof(LWtoolsContextHandlesRm);
    CHECK_RC(GetLwdaInstance().GetContextHandles(&m_CtxHandles[0]));

    // Context Switch to the Secondary Context at Index 1
    CHECK_RC(GetLwdaInstance().SwitchContext(GetBoundLwdaDevice(), 1));

    // Fetching handles for the Secondary Context which controls the Support kernel
    CHECK_RC(GetLwdaInstance().GetComputeEngineChannelGroup(&m_ChannelGrpHandles[1]));
    m_CtxHandles[1].struct_size = sizeof(LWtoolsContextHandlesRm);
    CHECK_RC(GetLwdaInstance().GetContextHandles(&m_CtxHandles[1]));

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("cilpstress", &m_Module));
    if (m_NumBlocks == 0)
    {
        m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();
    }
    if (m_NumThreads == 0)
    {
        m_NumThreads = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    }
    m_RunFunc = m_Module.GetFunction("CILPSupport", m_NumBlocks, m_NumThreads);
    CHECK_RC(m_RunFunc.InitCheck());
    CHECK_RC(m_RunFunc.SetSharedSizeMax());
    UINT32 bytesPerCta = 0;
    CHECK_RC(m_RunFunc.GetSharedSize(&bytesPerCta));
    if (m_SMemPct > 100 || m_SMemPct < 0)
    {
        Printf(Tee::PriError, "Incorrect Shared Memory Percentage set\n");
        return RC::BAD_PARAMETER;
    }
    bytesPerCta = static_cast<UINT32>(floor(m_SMemPct * bytesPerCta) / 100);
    CHECK_RC(m_RunFunc.SetSharedSize(bytesPerCta));

    // Random Input Data for the Support kernel
    m_NumElements = m_NumBlocks * m_NumThreads;
    m_Random.SeedRandom(GetTestConfiguration()->Seed());
    vector<UINT32> m_RandomData;
    for (UINT32 i = 0; i < m_NumElements; i++)
    {
        m_RandomData.push_back(m_Random.GetRandom());
    }

    const size_t numElementsSizeBytes = sizeof(m_RandomData[0]) * m_NumElements;
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(numElementsSizeBytes, &m_InputData));
    CHECK_RC(m_InputData.Set(m_RandomData.data(), numElementsSizeBytes));
    CHECK_RC(GetLwdaInstance().AllocHostMem(numElementsSizeBytes, &m_OutputData));

    m_Params.inputDataPtr = m_InputData.GetDevicePtr();
    m_Params.outputDataPtr = m_OutputData.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.numInnerIterations = m_NumInnerIterations;
    m_Params.bytesPerCta = bytesPerCta;
    m_Params.randSeed = m_Random.GetRandom();

    return rc;
}

RC LwdaCILPStress::DumpCTXSWStats(UINT32 handleIndex, UINT32& targetPremeptionCount)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_GR_GET_CTXSW_STATS_PARAMS statsParams = {0};
    statsParams.hChannel = m_ChannelGrpHandles[handleIndex];

    UINT32 retval = LwRmControl(m_CtxHandles[handleIndex].rmClient,
                                m_CtxHandles[handleIndex].rmSubDevice,
                                LW2080_CTRL_CMD_GR_GET_CTXSW_STATS,
                                &statsParams, sizeof(statsParams));
    rc = RmApiStatusToModsRC(retval);
    if (rc == RC::OK)
    {
        if (m_DumpStats)
        {
            Printf(Tee::PriNormal, "%s Context Switch Statistics ==>\n",
                                    handleIndex ? "Support Kernel" : "BgTest");
            Printf(Tee::PriNormal, "saves      %u\n", statsParams.saveCnt);
            Printf(Tee::PriNormal, "restores   %u\n", statsParams.restoreCnt);
            Printf(Tee::PriNormal, "WFI saves  %u\n", statsParams.wfiSaveCnt);
            Printf(Tee::PriNormal, "CTA saves  %u\n", statsParams.ctaSaveCnt);
            Printf(Tee::PriNormal, "CILP saves %u\n", statsParams.cilpSaveCnt);
            Printf(Tee::PriNormal, "GfxP saves %u\n", statsParams.gfxpSaveCnt);
        }
        switch (m_BgTestPreemptionMode)
        {
            case 0:
                targetPremeptionCount = statsParams.wfiSaveCnt;
                break;
            case 1:
                targetPremeptionCount = statsParams.ctaSaveCnt;
                break;
            case 2:
                targetPremeptionCount = statsParams.cilpSaveCnt;
                break;
            default:
                return RC::SOFTWARE_ERROR;;
        }
    }
    return rc;
}

RC LwdaCILPStress::SetPreemptionMode(UINT32 handleIndex, UINT32 mode)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_GR_SET_CTXSW_PREEMPTION_MODE_PARAMS pArgs = {0};
    pArgs.hChannel = m_ChannelGrpHandles[handleIndex];
    pArgs.flags = FLD_SET_DRF(2080, _CTRL_GR_SET_CTXSW_PREEMPTION_MODE_FLAGS, _CILP, _SET, pArgs.flags);

    switch (mode)
    {
        case 0:
            pArgs.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_WFI;
            break;
        case 1:
            pArgs.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CTA;
            break;
        case 2:
            pArgs.cilpPreemptMode = LW2080_CTRL_SET_CTXSW_PREEMPTION_MODE_COMPUTE_CILP;
            break;
        default:
            Printf(Tee::PriError, "Incorrect Compute Preemption Mode value %u specified."
                                  " Legal values are 0, 1 & 2\n", mode);
            return RC::BAD_PARAMETER;
    }

    UINT32 retval = LwRmControl(m_CtxHandles[handleIndex].rmClient,
                                m_CtxHandles[handleIndex].rmSubDevice,
                                LW2080_CTRL_CMD_GR_SET_CTXSW_PREEMPTION_MODE,
                                &pArgs, sizeof(pArgs));
    rc = RmApiStatusToModsRC(retval);
    return rc;
}

RC LwdaCILPStress::SetPreemptionTimeSlice(UINT32 handleIndex, UINT32 timeSliceUs)
{
    RC rc;
    LWA06C_CTRL_TIMESLICE_PARAMS tsParams = {0};
    tsParams.timesliceUs = timeSliceUs;

    UINT32 retval = LwRmControl(m_CtxHandles[handleIndex].rmClient,
                                m_ChannelGrpHandles[handleIndex],
                                LWA06C_CTRL_CMD_SET_TIMESLICE,
                                &tsParams, sizeof(tsParams));
    rc = RmApiStatusToModsRC(retval);
    return rc;
}

RC LwdaCILPStress::Run()
{
    RC rc;
    UINT64 kernLaunchCount = 0;
    const UINT32 totalLoops = GetTestConfiguration()->Loops();
    double durationMs = 0.0;

    // Setting Preemption modes
    CHECK_RC(SetPreemptionMode(0, m_BgTestPreemptionMode));
    CHECK_RC(SetPreemptionMode(1, m_SupportPreemptionMode));

    // Setting Preemption Timslices
    CHECK_RC(SetPreemptionTimeSlice(0, m_BgTestTimesliceUs));
    CHECK_RC(SetPreemptionTimeSlice(1, m_SupportTimesliceUs));

    // Create events for timing kernel
    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());

    // Starting the BgTest (defined in gpudecls.js)
    CallbackArguments args;
    CHECK_RC(FireCallbacks(ModsTest::MISC_A, Callbacks::STOP_ON_ERROR, args, "Bgtest Start"));
    DEFERNAME(releaseBgTest)
    {
        FireCallbacks(ModsTest::MISC_B, Callbacks::STOP_ON_ERROR, args, "Bgtest End");
    };
    const UINT64 startMs = Xp::GetWallTimeMS();
    for
    (
        UINT64 loop = 0;
        m_RuntimeMs ? (Xp::GetWallTimeMS() - startMs) < static_cast<double>(m_RuntimeMs) : loop < totalLoops;
        loop++
    )
    {
        CHECK_RC(startEvent.Record());
        CHECK_RC(m_RunFunc.Launch(m_Params));
        kernLaunchCount++;
        CHECK_RC(stopEvent.Record());
        CHECK_RC(GetLwdaInstance().Synchronize());
        durationMs += stopEvent.TimeMsElapsedSinceF(startEvent);
    }
    releaseBgTest.Cancel();
    CHECK_RC(FireCallbacks(ModsTest::MISC_B, Callbacks::STOP_ON_ERROR, args, "Bgtest End"));

    if (kernLaunchCount && durationMs)
    {
        VerbosePrintf("Total Support Kernel Runtime: %fms\n"
                      "  Avg Support Kernel Runtime: %fms\n",
                      durationMs, durationMs / kernLaunchCount);
    }

    // Fetching Preemption Statistics
    UINT32 targetPremeptionCount = 0;
    CHECK_RC(DumpCTXSWStats(0, targetPremeptionCount));

    // Checking bgTest Premeption Threshold
    if (targetPremeptionCount < m_FailureThreshold)
    {
        Printf(Tee::PriError, "BgTest Preemption Count is below the threshold.\n"
                              "Threshold Value : %u\n"
                              "Obtained Value  : %u\n", m_FailureThreshold, targetPremeptionCount);
        return RC::GPU_COMPUTE_MISCOMPARE;
    }

    return rc;
}

RC LwdaCILPStress::Cleanup()
{
    m_InputData.Free();
    m_OutputData.Free();
    m_Module.Unload();
    return LwdaStreamTest::Cleanup();
}
