/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "gpu/tests/lwdastst.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
//TODO remove this include after debugging PTIMER issues
#include "volta/gv100/dev_timer.h"

//! LWCA-based test for verifying subcontexts
//-------------------------------------------------------------------------------------------------
class LwdaSubctx : public LwdaStreamTest
{
public:
    LwdaSubctx();
    virtual ~LwdaSubctx() { }
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual INT32 NumSecondaryCtx() { return m_NumContexts-1;} //$
    SETGET_PROP(UseCpuStopwatch, bool);
    SETGET_PROP(StartVal, UINT32);
    SETGET_PROP(EndVal, UINT32);
    SETGET_PROP(NumContexts, UINT32)
    SETGET_PROP(Threshold, double);
    SETGET_PROP(Debug, bool);
private:
    struct PerContextInfo
    {
        Lwca::DeviceMemory  InputBuffer;
        Lwca::DeviceMemory  OutputBuffer;
        Lwca::Module        Module;
        Lwca::Function      KernelFunction;
        UINT64              startTime;
        UINT64              endTime;
        double              elapsedMs;
        UINT64              startClks;
        UINT64              endClks;
        UINT64              elapsedClks;
    };

    enum subctxState
    {
        ctxDisabled = 0,
        ctxEnabled = 1
    };

    bool   m_InitDone = false;
    bool   m_UseCpuStopwatch = false;
    UINT32 m_StartVal = 0x0;
    UINT32 m_EndVal = 0x54321;
    UINT32 m_GridWidth = 1;
    UINT32 m_BlockWidth = 1;
    UINT32 m_OutputBufferSize = 12; // [0]=count, [1]=StartTimeLo, [2]=StartTimeHi,
                                    // [3]=EndTimeLo, [4]=EndTimeHi
                                    // [5]=StartClock64Lo, [6]=StartClock64Hi
                                    // [7]=EndClock64Lo, [8]=EndClock64Hi
                                    // [9]=SM_ID
                                    // [10..11]=rsvd for debug
    INT32  m_NumContexts = 2;       // 2 is the minimum number of contexts to verify this test.
    double m_Threshold = 0.60;
    bool   m_Debug = false;
    static const INT32 MaxContexts = 64;  //Max allowed by LWCA layer

    vector<PerContextInfo> m_Info;
    virtual RC SetupContext(INT32 ctxId);
    void PrintJsProperties(Tee::Priority pri);
};

//-------------------------------------------------------------------------------------------------
LwdaSubctx::LwdaSubctx()
{
    SetName("LwdaSubctx");
}

//-------------------------------------------------------------------------------------------------
bool LwdaSubctx::IsSupported()
{
    // Check if compute is supported at all
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    return cap >= Lwca::Capability::SM_70 && cap != Lwca::Capability::SM_89 && cap <= Lwca::Capability::SM_90;
}

//-------------------------------------------------------------------------------------------------
RC LwdaSubctx::Setup()
{
    RC rc;
    if (GetBoundGpuDevice()->GetFamily() == GpuDevice::Hopper)
    {
        m_UseCpuStopwatch = true;
    }

    if (m_NumContexts > MaxContexts)
    {
        Printf(Tee::PriWarn, "Test only supports up to %d contexts, setting NumContexts=%d\n",
               MaxContexts, MaxContexts);
        m_NumContexts = MaxContexts;
    }
    m_Info.resize(m_NumContexts);

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(AllocDisplay());     // Required to enable interrupts
    m_InitDone = true;
    return rc;
}

void LwdaSubctx::PrintJsProperties(Tee::Priority pri)
{
    const char* tf[2] = { "false", "true" };
    LwdaStreamTest::PrintJsProperties(pri);
    Printf(pri, "LwdaSubctx controls:\n");
    Printf(pri, "\t%-32s %d\n",     "StartVal:",    m_StartVal);
    Printf(pri, "\t%-32s 0x%x\n",   "EndVal:",      m_EndVal);
    Printf(pri, "\t%-32s %d\n",     "NumContexts:", m_NumContexts);
    Printf(pri, "\t%-32s %d\n",     "GridWidth:",   m_GridWidth);
    Printf(pri, "\t%-32s %d\n",     "BlockWidth:",  m_BlockWidth);
    Printf(pri, "\t%-32s %3.2f\n",  "Threshold:",   m_Threshold);
    Printf(pri, "\t%-32s %s\n",     "Debug:",       tf[m_Debug]);
    Printf(pri, "\t%-32s %s\n",     "UseCpuStopwatch:", tf[m_UseCpuStopwatch]);
}

//-------------------------------------------------------------------------------------------------
RC LwdaSubctx::SetupContext(INT32 ctxIdx)
{
    RC rc;
    CHECK_RC(GetLwdaInstance().SwitchContext(GetBoundLwdaDevice(), ctxIdx));

    // load the lwbin
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("subctx", &m_Info[ctxIdx].Module));

    // load the function
    m_Info[ctxIdx].KernelFunction = m_Info[ctxIdx].Module.GetFunction("subctxtest",
                                    m_GridWidth, m_BlockWidth);
    CHECK_RC(m_Info[ctxIdx].KernelFunction.InitCheck());

    // allocate buffers
    const UINT32 inputBufferSize = 3;

    CHECK_RC(GetLwdaInstance().AllocDeviceMem(inputBufferSize*sizeof(UINT32), 
                                              &m_Info[ctxIdx].InputBuffer));

    // Set data in inputBuffer
    vector<UINT32> initData(inputBufferSize, 0);
    initData[0] = m_StartVal;
    initData[1] = m_EndVal;
    initData[2] = GetTestConfiguration()->Loops();
    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> >
        hostData(inputBufferSize * sizeof(UINT32), UINT32(),
                Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    std::copy(initData.begin(), initData.end(), hostData.begin());

    CHECK_RC(m_Info[ctxIdx].InputBuffer.Set(&hostData[0], inputBufferSize * sizeof(UINT32)));

    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_OutputBufferSize * sizeof(UINT32),
                                              &m_Info[ctxIdx].OutputBuffer));

    CHECK_RC(GetLwdaInstance().Synchronize(GetBoundLwdaDevice()));
    return rc;
}

//-------------------------------------------------------------------------------------------------
// To verify that sub-contexts works we need to run the basic test setup, run, cleanup operations
// twice, once with SubcontextState disabled and once again with SubcontextState enabled.
// In theory when sub-contexts are enabled both contexts they will run simultaneously on the
// hardware so the runtime should be about 50% less then when it is disabled.
// Note: We tried using the global timer (PTIMER) to measure the amount of time the kernel runs at
//       the hardware level but this timer is a wall clock timer and we found that on some GPUs
//       the power policies will kick in and lower the GPC clock while the test is running. When
//       this happens we get false failures because it took more clock cycles to complete the 
//       kernel.
//       We tried replacing the global timer with the clk64() lwca device API because that clock 
//       is driven from the GPC clock. However this SM local clock is not reliable when comparing 
//       clk cycles across SMs (which happens when sub-contexts are enabled) because the clks are 
//       not synchronized.
//       We tried using the CPU based clock and measure the amount of time it takes to launch and
//       complete both contexts but this fails intermittently on GA10x WinMfg builds.
//       There is no consistent way to measure this across all GPUs & all OSes. So for Hopper we
//       will use the cpuTimer and all other GPUs we will stick with the clk64() timers.
RC LwdaSubctx::Run()
{
    StickyRC rc;
    INT32 ii;
    double deltaPTimerMs[2] = { 0.0, 0.0 };
    UINT64 deltaClk64[2]     = { 0, 0 };
    double deltaCpuMs[2]    = { 0, 0 };
    const char * szCtxState[2] = { "disabled", "enabled"}; //$
    Tee::Priority pri = GetVerbosePrintPri();
    if (m_Debug)
    {
        pri = Tee::PriNormal;
    }
    for (INT32 ctxState = ctxDisabled; ctxState <= ctxEnabled; ctxState++)
    {
        CHECK_RC(GetLwdaInstance().Init());
        CHECK_RC(GetLwdaInstance().SetSubcontextState(GetBoundLwdaDevice(), ctxState));
        CHECK_RC(GetLwdaInstance().InitContext(GetBoundLwdaDevice(),
                                               NumSecondaryCtx(),
                                               GetDefaultActiveContext()));
        // Sync all conlwrrent devices to prevent races on global resources.
        // specifically enabling/disabling subcontext is a global resource.
        Tasker::WaitOnBarrier();
        for (ii = 0; ii < m_NumContexts; ii++)
        {
            CHECK_RC(SetupContext(ii));
        }

        Printf(pri, "Running %d contexts with SubcontextState %s\n",
            m_NumContexts, szCtxState[ctxState]);
        {
            unique_ptr<Utility::StopWatch> myStopwatch;
            string szHdr = Utility::StrPrintf("CpuTimer subctx %s", szCtxState[ctxState]);
            myStopwatch = make_unique<Utility::StopWatch>(szHdr.c_str(), GetVerbosePrintPri());

            for (ii = 0; ii < m_NumContexts; ii++)
            {
                GetLwdaInstance().SwitchContext(GetBoundLwdaDevice(), ii);
                CHECK_RC(m_Info[ii].KernelFunction.Launch(
                         m_Info[ii].InputBuffer.GetDevicePtr(),
                         m_Info[ii].OutputBuffer.GetDevicePtr()));
            }

            for (ii = 0; ii < m_NumContexts; ii++)
            {
                GetLwdaInstance().SwitchContext(GetBoundLwdaDevice(), ii);
                CHECK_RC(GetLwdaInstance().Synchronize());
            }
            deltaCpuMs[ctxState] = myStopwatch->Stop()/1.0e+6;
        }

        bool kernelFailed = false;
        for (ii = 0; ii < m_NumContexts; ii++)
        {
            GetLwdaInstance().SwitchContext(GetBoundLwdaDevice(), ii);

            // Copy results back to sysmem for verifying
            vector<UINT32, Lwca::HostMemoryAllocator<UINT32> >
                    outputBuf(m_OutputBufferSize,
                    UINT32(),
                    Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(), GetBoundLwdaDevice()));

            CHECK_RC(m_Info[ii].OutputBuffer.Get(&outputBuf[0], outputBuf.size()*sizeof(UINT32)));

            // wait for copy to complete before accessing the data.
            CHECK_RC(GetLwdaInstance().Synchronize());
            //PTimer values
            m_Info[ii].startTime = ((UINT64)outputBuf[1]) << 32 | (UINT64)outputBuf[2];
            m_Info[ii].endTime = ((UINT64)outputBuf[3]) << 32 | (UINT64)outputBuf[4];
            m_Info[ii].elapsedMs = (double)(m_Info[ii].endTime - m_Info[ii].startTime)/1.0e+6;

            //clock64() values
            m_Info[ii].startClks = ((UINT64)outputBuf[5]) << 32 | (UINT64)outputBuf[6];
            m_Info[ii].endClks =  ((UINT64)outputBuf[7]) << 32 | (UINT64)outputBuf[8];
            m_Info[ii].elapsedClks = m_Info[ii].endClks - m_Info[ii].startClks;
            // Header to interpret the buffer values.
            if (ii == 0)
            {
                Printf(pri, "%46s endVal  startTmHi  startTmLo  endTmHi    endTmLo    startClkHi"
                            " startClkLo endClkHi   endClkLo   SM_ID\n"," ");
            }
            Printf(pri, "Ctx:%d SubcontextState:%s outputBuf[0..9]:%#06x %#010x %#010x %#010x"
                          " %#010x %#010x %#010x %#010x %#010x %#04x elapsedPTimer:%3.6lfms"
                          " elapsedClk64():%lld \n",
                          ii, szCtxState[ctxState],
                          outputBuf[0], outputBuf[1], outputBuf[2], outputBuf[3], outputBuf[4],
                          outputBuf[5], outputBuf[6], outputBuf[7], outputBuf[8], outputBuf[9],
                          m_Info[ii].elapsedMs, m_Info[ii].elapsedClks);

            if (outputBuf[0] != m_EndVal)
            {
                Printf(Tee::PriError,
                       "kernel in sub-context:%d did not complete required workload!\n", ii);
                kernelFailed = true;
            }
        }

        // In theory both kernels should run in the same amount of time, so use the 1st kernel
        // for delta comparisons.
        deltaPTimerMs[ctxState] = ((m_Info[0].endTime - m_Info[0].startTime)/1.0e+6);
        deltaClk64[ctxState] = m_Info[0].endClks - m_Info[0].startClks;
        // Cleanup the contexts
        for (INT32 ii = m_NumContexts - 1; ii >= 0;  ii--)
        {
            GetLwdaInstance().SwitchContext(GetBoundLwdaDevice(), ii);
            m_Info[ii].InputBuffer.Free();
            m_Info[ii].OutputBuffer.Free();
            m_Info[ii].Module.Unload();
        }
        GetLwdaInstance().Free();

        // If one of the kernels failed to complete we can't validate the clocks.
        if (kernelFailed)
            return RC::HW_STATUS_ERROR;
    }

    Printf(pri, "SubCtx enable/disable ratios: PTimer:%3.2lf CpuTimer:%3.2f clk64:%3.2f\n",
                 deltaPTimerMs[ctxEnabled] / deltaPTimerMs[ctxDisabled],
                 deltaCpuMs[ctxEnabled] / deltaCpuMs[ctxDisabled],
                 (double)deltaClk64[ctxEnabled] / (double)deltaClk64[ctxDisabled]);

    // Confirm that the kernels run in half the time with sub-contexts enabled.
    if (m_UseCpuStopwatch)
    {
        if ((deltaCpuMs[ctxEnabled] / deltaCpuMs[ctxDisabled]) > m_Threshold)
        {
            Printf(Tee::PriError, "Sub-contexts using CpuStopwatch is not working!\n");
            rc = RC::HW_STATUS_ERROR;
        }
    }
    else // use the clk64 timers
    {
        if (deltaClk64[ctxDisabled] == 0 || deltaClk64[ctxEnabled] == 0 ||
             ((double)deltaClk64[ctxEnabled] / (double)deltaClk64[ctxDisabled]) > m_Threshold)
        {
            Printf(Tee::PriError, "Sub-contexts using clk64() API is not working!\n");
            rc = RC::HW_STATUS_ERROR;
        }
    }

    if (m_Debug)
    {
        if (deltaPTimerMs[ctxDisabled] == 0 || deltaPTimerMs[ctxEnabled] == 0 ||
             ((double)deltaPTimerMs[ctxEnabled] / (double)deltaPTimerMs[ctxDisabled]) > m_Threshold)
        {
            Printf(Tee::PriWarn, "Sub-contexts using PTIMER is not working!\n");
            Printf(Tee::PriWarn, "Check you power policies! see bug:200670359\n");
        }
    }

    return (rc);
}

//-------------------------------------------------------------------------------------------------
RC LwdaSubctx::Cleanup()
{
    RC rc;
    return (GpuTest::Cleanup());
}

//-------------------------------------------------------------------------------------------------
JS_CLASS_INHERIT(LwdaSubctx, LwdaStreamTest, "LWCA test for subcontext");
CLASS_PROP_READWRITE(LwdaSubctx, StartVal,      UINT32, "Initial value for the counter");
CLASS_PROP_READWRITE(LwdaSubctx, EndVal,        UINT32, "Max value of the counter");
CLASS_PROP_READWRITE(LwdaSubctx, NumContexts,   UINT32, "Number of contexts to create (1-64)");
CLASS_PROP_READWRITE(LwdaSubctx, Debug,         bool,   "Enable debug prints");
CLASS_PROP_READWRITE(LwdaSubctx, Threshold,     double,
                     "Max value for subCtx enable/disable runtimes(default 0.60)");
CLASS_PROP_READWRITE(LwdaSubctx, UseCpuStopwatch, bool,
                     "Use Cpu Stopwatch to measure kernel runtimes(default is to use clk64");

