/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/tests/lwca/cache/l1tag.h"

class LwdaL1Tag : public LwdaStreamTest
{
public:
    LwdaL1Tag()
    : LwdaStreamTest()
    {
        SetName("LwdaL1Tag");
    }

    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC Setup() override;
    RC Cleanup() override;
    RC Run() override;

    // JS property accessors
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(InnerIterations, UINT64);
    SETGET_PROP(ErrorLogLen, UINT32);
    SETGET_PROP(DumpMiscompares, bool);
    SETGET_PROP(ReportFailingSmids, bool);
    SETGET_PROP(SmidFailureLimit, UINT64);

private:
    L1TagParams m_Params = {};
    Lwca::Module m_Module;
    Lwca::DeviceMemory m_L1Data;

    Lwca::DeviceMemory m_MiscompareCount;
    Lwca::HostMemory   m_HostMiscompareCount;
    Lwca::HostMemory   m_HostErrorLog;

    Random m_Random;
    map<UINT16, UINT64> m_SmidMiscompareCount;
    UINT32 m_NumThreads = 0;
    UINT32 m_NumBlocks  = 0;

    // JS properties
    UINT32 m_RuntimeMs = 10000;
    bool   m_KeepRunning = false;
    UINT64 m_InnerIterations = 1024;
    UINT32 m_ErrorLogLen = 8192;
    bool   m_DumpMiscompares = false;
    bool   m_ReportFailingSmids = true;
    UINT64 m_SmidFailureLimit = 1;
};

JS_CLASS_INHERIT(LwdaL1Tag, LwdaStreamTest, "L1Tag Stress Test");
CLASS_PROP_READWRITE(LwdaL1Tag, RuntimeMs, UINT32,
                     "Run the kernel for at least the specified amount of time. "
                     "If RuntimeMs=0 TestConfiguration.Loops will be used.");
CLASS_PROP_READWRITE(LwdaL1Tag, KeepRunning, bool,
                     "While this is true, Run will continue even beyond RuntimeMs.");
CLASS_PROP_READWRITE(LwdaL1Tag, InnerIterations, UINT64,
                     "Number of times per LWCA kernel launch to run the "
                     "memory access pattern over the L1 cache");
CLASS_PROP_READWRITE(LwdaL1Tag, ErrorLogLen, UINT32,
                     "Max number of errors that can be logged with detailed information");
CLASS_PROP_READWRITE(LwdaL1Tag, DumpMiscompares, bool,
                     "Print out the miscomparing L1 offset data");
CLASS_PROP_READWRITE(LwdaL1Tag, ReportFailingSmids, bool,
                     "Report which SMID and HW TPC read the miscomparing L1 data");
CLASS_PROP_READWRITE(LwdaL1Tag, SmidFailureLimit, UINT64,
                     "Lower bound of miscompares at which to report a SM/TPC as failing");

bool LwdaL1Tag::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_70)
    {
        Printf(Tee::PriLow, "LwdaL1Tag does not support this SM version\n");
        return false;
    }

    return true;
}

void LwdaL1Tag::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "LwdaL1Tag Js Properties:\n");
    Printf(pri, "    RuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "    KeepRunning:                    %s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "    InnerIterations:                %llu\n", m_InnerIterations);
    Printf(pri, "    ErrorLogLen:                    %u\n", m_ErrorLogLen);
    Printf(pri, "    DumpMiscompares:                %s\n", m_DumpMiscompares ? "true" : "false");
    Printf(pri, "    ReportFailingSmids:             %s\n", m_ReportFailingSmids ? "true" : "false");
    if (m_ReportFailingSmids)
    {
        Printf(pri, "    SmidFailureLimit:               %llu\n", m_SmidFailureLimit);
    }
}

RC LwdaL1Tag::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("l1tag", &m_Module));

    // Init RNG
    m_Random.SeedRandom(GetTestConfiguration()->Seed());

    // Fetch L1 cache size
    // The test only supports L1 caches up to 256KB
    UINT32 l1PerSMBytes = 0;
    CHECK_RC(GetBoundGpuSubdevice()->GetMaxL1CacheSizePerSM(&l1PerSMBytes));

    if (l1PerSMBytes > (1u << 16) * sizeof(UINT32))
    {
        MASSERT(!"L1 cache is too large, update test to support the current GPU!");
        return RC::SOFTWARE_ERROR;
    }

    // One LWCA CTA per SM
    m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();

    // GA100/GA10B/GH100 is a hardcoded exception, since the L1 cache size (192KB/192KB/256KB) is large enough
    // that two reads are required to pre-load the data
    if (GetBoundLwdaDevice().GetCapability() == Lwca::Capability::SM_80 ||
        GetBoundLwdaDevice().GetCapability() == Lwca::Capability::SM_87 ||
        GetBoundLwdaDevice().GetCapability() == Lwca::Capability::SM_90)
    {
        m_NumThreads =
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    }
    else
    {
        // Each thread owns a line in the L1 cache
        m_NumThreads = l1PerSMBytes / L1_LINE_SIZE_BYTES;
        MASSERT(l1PerSMBytes % L1_LINE_SIZE_BYTES == 0);

        const UINT32 maxThreads =
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
        if (m_NumThreads > maxThreads)
        {
            MASSERT(!"Not enough LWCA threads, update test to support the current GPU!");
            return RC::SOFTWARE_ERROR;
        }
    }

    // Allocate memory for L1
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_NumBlocks * l1PerSMBytes, &m_L1Data));

    // Allocate miscompare count / error log
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeof(UINT64), &m_MiscompareCount));
    CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT64), &m_HostMiscompareCount));
    CHECK_RC(GetLwdaInstance().AllocHostMem(m_ErrorLogLen * sizeof(L1TagError), &m_HostErrorLog));

    // Set kernel parameters
    m_Params.data          = m_L1Data.GetDevicePtr();
    m_Params.sizeBytes     = m_L1Data.GetSize();
    m_Params.errorCountPtr = m_MiscompareCount.GetDevicePtr();
    m_Params.errorLogPtr   = m_HostErrorLog.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.errorLogLen   = m_ErrorLogLen;
    m_Params.iterations    = m_InnerIterations;

    return rc;
}

RC LwdaL1Tag::Run()
{
    RC rc;

    StickyRC stickyRc;
    const bool bPerpetualRun = m_KeepRunning;
    UINT64 kernLaunchCount = 0;
    double durationMs = 0.0;

    if (!bPerpetualRun)
    {
        if (m_RuntimeMs)
        {
            CHECK_RC(PrintProgressInit(m_RuntimeMs));
        }
        else
        {
            CHECK_RC(PrintProgressInit(GetTestConfiguration()->Loops()));
        }
    }

    // Get Init function
    Lwca::Function initFunc = m_Module.GetFunction("InitL1Data", m_NumBlocks, m_NumThreads);
    CHECK_RC(initFunc.InitCheck());
    CHECK_RC(initFunc.SetAttribute(LW_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT,
                                   LW_SHAREDMEM_CARVEOUT_MAX_L1));

    // Get Run function
    Lwca::Function runFunc = m_Module.GetFunction("L1TagTest", m_NumBlocks, m_NumThreads);
    CHECK_RC(runFunc.InitCheck());
    CHECK_RC(runFunc.SetAttribute(LW_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT,
                                  LW_SHAREDMEM_CARVEOUT_MAX_L1));

    // Create events for timing kernel
    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());

    // If KeepRunning is true, run at least once and keep running until it is false.
    // Otherwise run for RuntimeMs if it is set.
    // Otherwise run for TestConfiguration.Loops loops.
    UINT64 totalNumErrors = 0;
    for
    (
        UINT64 loop = 0;
        bPerpetualRun || (m_RuntimeMs ?
                          durationMs < static_cast<double>(m_RuntimeMs) :
                          loop < GetTestConfiguration()->Loops());
        loop++
    )
    {
        // Clear error counter
        CHECK_RC(m_MiscompareCount.Clear());

        // Use a different RNG seed each loop
        m_Params.randSeed = m_Random.GetRandom();

        // Init data buffer
        CHECK_RC(initFunc.Launch(m_Params));

        // Launch kernel, recording elaspsed time with events
        CHECK_RC(startEvent.Record());
        CHECK_RC(runFunc.Launch(m_Params));
        kernLaunchCount++;
        CHECK_RC(stopEvent.Record());

        // Get error count
        CHECK_RC(m_MiscompareCount.Get(m_HostMiscompareCount.GetPtr(),
                                       m_HostMiscompareCount.GetSize()));

        // Synchronize and get time for kernel completion
        GetLwdaInstance().Synchronize();
        durationMs += stopEvent.TimeMsElapsedSinceF(startEvent);

        // Update test progress
        if (!bPerpetualRun)
        {
            if (m_RuntimeMs)
            {
                CHECK_RC(PrintProgressUpdate(std::min(static_cast<UINT64>(durationMs),
                                                      static_cast<UINT64>(m_RuntimeMs))));
            }
            else
            {
                CHECK_RC(PrintProgressUpdate(loop + 1));
            }
        }

        // Handle errors
        const UINT64 numMiscompares = *static_cast<UINT64*>(m_HostMiscompareCount.GetPtr());
        totalNumErrors += numMiscompares;
        if (numMiscompares > 0)
        {
            Printf(Tee::PriError,
                   "LwdaL1Tag found %llu miscompare(s) on loop %llu\n", numMiscompares, loop);
            L1TagError* pErrors = static_cast<L1TagError*>(m_HostErrorLog.GetPtr());

            if (numMiscompares > m_ErrorLogLen)
            {
                Printf(Tee::PriWarn,
                       "%llu miscompares, but error log only contains %u entries. "
                       "Some failing SMID/TPCs may not be reported.\n",
                       numMiscompares, m_ErrorLogLen);
            }
            for (UINT32 i = 0; i < numMiscompares && i < m_ErrorLogLen; i++)
            {
                const auto& error = pErrors[i];
                if (m_DumpMiscompares)
                {
                    Printf(Tee::PriError, "%u\n"
                           "TestStage  : %s\n"
                           "DecodedOff : 0x%04X\n"
                           "ExpectedOff: 0x%04X\n"
                           "Iteration  : %llu\n"
                           "InnerLoop  : %d\n"
                           "Smid       : %d\n"
                           "Warpid     : %d\n"
                           "Laneid     : %d\n"
                           "\n",
                           i,
                           (error.testStage == TestStage::PreLoad) ? "PreLoad" : "RandomLoad",
                           error.decodedOff,
                           error.expectedOff,
                           static_cast<UINT64>(error.iteration),
                           error.innerLoop,
                           error.smid,
                           error.warpid,
                           error.laneid
                    );
                }
                if (m_ReportFailingSmids)
                {
                    m_SmidMiscompareCount[error.smid]++;
                }
            }
            stickyRc = RC::GPU_COMPUTE_MISCOMPARE;
            if (GetGoldelwalues()->GetStopOnError())
            {
                break;
            }
        }

        // In perpetual-run mode exit when KeepRunning is set to false
        if (bPerpetualRun && !m_KeepRunning)
        {
            break;
        }
    }

    // Print which SM/TPCs failed, if any
    if (m_ReportFailingSmids)
    {
        CHECK_RC(ReportFailingSmids(m_SmidMiscompareCount, m_SmidFailureLimit));
    }

    // Kernel runtime and error prints useful for debugging
    // Guard against divide-by-zero errors (that shouldn't occur)
    const double durationSec = durationMs / 1000.0;
    if (totalNumErrors && durationMs)
    {
        VerbosePrintf("\n");
        VerbosePrintf("TotalErrors: %llu\n"
                      "Errors/s   : %f\n",
                      totalNumErrors, static_cast<double>(totalNumErrors) / durationSec);
        VerbosePrintf("\n");
    }
    if (kernLaunchCount && durationMs)
    {
        VerbosePrintf("Total Kernel Runtime: %fms\n"
                      "  Avg Kernel Runtime: %fms\n",
                      durationMs, durationMs / kernLaunchCount);
    }

    // Callwlate total bandwidth
    // Two reads per memory location: one at the location and one that is random
    if (durationSec)
    {
        const UINT64 bytesTransferred =
            kernLaunchCount * m_InnerIterations * m_L1Data.GetSize() * 2;
        GpuTest::RecordBps(
            static_cast<double>(bytesTransferred), durationSec, GetVerbosePrintPri());
    }

    return stickyRc;
}

RC LwdaL1Tag::Cleanup()
{
    m_L1Data.Free();
    m_MiscompareCount.Free();
    m_HostMiscompareCount.Free();
    m_HostErrorLog.Free();

    m_Module.Unload();
    return LwdaStreamTest::Cleanup();
}

