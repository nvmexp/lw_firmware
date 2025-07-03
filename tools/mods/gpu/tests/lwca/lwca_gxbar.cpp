/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwdastst.h"
#include "gpu/tests/lwca/cache/gxbar.h"
#include "core/utility/ptrnclss.h"
#include "core/include/platform.h"
#include <numeric>

class LwdaGXbar : public LwdaStreamTest
{
public:
    LwdaGXbar()
    : LwdaStreamTest()
    {
        SetName("LwdaGXbar");
    }

    RC InitFromJs() override;
    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC Setup() override;
    RC Cleanup() override;
    RC Run() override;

    // JS property accessors
    SETGET_PROP(RuntimeMs, UINT32);
    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(ReadOnly, bool);
    SETGET_PROP(InnerIterations, UINT64);
    SETGET_PROP(SharedMemBytes, UINT32);
    SETGET_PROP(NumThreads, UINT32);
    SETGET_PROP(ErrorLogLen, UINT32);
    SETGET_PROP(VerifyResults, bool);
    SETGET_PROP(DumpMiscompares, bool);
    SETGET_PROP(ReportFailingSmids, bool);
    SETGET_PROP(SmidFailureLimit, UINT64);
    SETGET_PROP(UseRandomData, bool);
    SETGET_JSARRAY_PROP_LWSTOM(Patterns);
    SETGET_PROP(PatternName, string);
    SETGET_PROP(UsePulse, bool);
    SETGET_PROP(LaunchDelayUs, UINT32);
    SETGET_PROP(PulseNs, UINT32);
    SETGET_PROP(DutyPct, UINT32);

private:
    void GenerateRouting(const UINT32 maxClusterSize);
    RC LaunchCgaKernels();
    RC GetPatternByName();

    GXbarParams m_Params = {};
    Lwca::Module m_Module;
    Lwca::Function m_RunFunc;

    Lwca::HostMemory   m_HostLaunchCount;

    Lwca::DeviceMemory m_MiscompareCount;
    Lwca::HostMemory   m_HostMiscompareCount;
    Lwca::HostMemory   m_HostErrorLog;

    Lwca::HostMemory   m_HostRouting;
    Lwca::DeviceMemory m_AccVal;
    Lwca::DeviceMemory m_PatternsMem;

    Random m_Random;

    vector<UINT32> m_SMCounts;
    vector<Lwca::Stream> m_Streams;

    // JS properties
    UINT32 m_RuntimeMs = 10000;
    bool   m_KeepRunning = false;
    bool   m_ReadOnly = false;
    UINT64 m_InnerIterations = 1024;
    UINT32 m_SharedMemBytes = 0;
    UINT32 m_NumThreads = 0;
    UINT32 m_ErrorLogLen = 8192;
    bool   m_VerifyResults = true;
    bool   m_DumpMiscompares = false;
    bool   m_ReportFailingSmids = true;
    UINT64 m_SmidFailureLimit = 1;
    bool m_UseRandomData = true;
    vector<UINT32> m_Patterns;
    string m_PatternName;
    bool m_UsePulse = false;
    UINT32 m_LaunchDelayUs = 0;
    UINT32 m_PulseNs = 300;
    UINT32 m_DutyPct = 60;

};

RC LwdaGXbar::SetPatterns(JsArray *val)
{
    JavaScriptPtr pJs;
    return pJs->FromJsArray(*val, &m_Patterns);
}

RC LwdaGXbar::GetPatterns(JsArray *val) const
{
    JavaScriptPtr pJs;
    return pJs->ToJsArray(m_Patterns, val);
}

JS_CLASS_INHERIT(LwdaGXbar, LwdaStreamTest, "GXbar Stress Test");
CLASS_PROP_READWRITE(LwdaGXbar, RuntimeMs, UINT32,
                     "Run the kernel for at least the specified amount of time. "
                     "If RuntimeMs=0 TestConfiguration.Loops will be used.");
CLASS_PROP_READWRITE(LwdaGXbar, KeepRunning, bool,
                     "While this is true, Run will continue even beyond RuntimeMs.");
CLASS_PROP_READWRITE(LwdaGXbar, ReadOnly, bool,
                     "Run GXBAR stress workload without SM<->SM shared memory writes.");
CLASS_PROP_READWRITE(LwdaGXbar, InnerIterations, UINT64,
                     "Number of times per LWCA kernel launch to run the "
                     "memory access pattern over the L1 cache");
CLASS_PROP_READWRITE(LwdaGXbar, SharedMemBytes, UINT32,
                     "Amount of shared memory in bytes to test per CTA (0 means test all shared memory");
CLASS_PROP_READWRITE(LwdaGXbar, NumThreads, UINT32,
                     "Number of kernel threads to run per LWCA block");
CLASS_PROP_READWRITE(LwdaGXbar, ErrorLogLen, UINT32,
                     "Max number of errors that can be logged with detailed information");
CLASS_PROP_READWRITE(LwdaGXbar, VerifyResults, bool,
                     "Check for errors while running the GXBAR stress workload");
CLASS_PROP_READWRITE(LwdaGXbar, DumpMiscompares, bool,
                     "Print out the miscomparing L1 offset data");
CLASS_PROP_READWRITE(LwdaGXbar, ReportFailingSmids, bool,
                     "Report which SMID and HW TPC read the miscomparing L1 data");
CLASS_PROP_READWRITE(LwdaGXbar, SmidFailureLimit, UINT64,
                     "Lower bound of miscompares at which to report a SM/TPC as failing");
CLASS_PROP_READWRITE(LwdaGXbar, UseRandomData, bool,
                     "Use random data in place of Patterns.");
CLASS_PROP_READWRITE_JSARRAY(LwdaGXbar, Patterns, JsArray,
                     "User-defined array of patterns");
CLASS_PROP_READWRITE(LwdaGXbar, PatternName, string,
                     "Name of the pattern to be used when explicit Patterns are not provided");
CLASS_PROP_READWRITE(LwdaGXbar, UsePulse, bool,
                     "Use Pulse Mode or not. Default is false"); 
CLASS_PROP_READWRITE(LwdaGXbar, LaunchDelayUs, UINT32,
                     "Time in microseconds to wait between kernel launches");
CLASS_PROP_READWRITE(LwdaGXbar, PulseNs, UINT32,
                     "Pulse step in nanoseconds");
CLASS_PROP_READWRITE(LwdaGXbar, DutyPct, UINT32,
                     "Percent of time (from 1 to 99) when the workload is exelwting");

//! Generate a random SM<->SM mapping, for each cluster
//!
//! This function uses a modification of the Fisher-Yates shuffle with the constraint
//! that an SM cannot map to itself, the reason being that a CTA cannot access its
//! own shared memory over GXBAR, since it gets redirected to local shared memory.
//!
//! In theory you can have a CTA on an SM access shared memory of another CTA on the same SM
//! over GXBAR, but that gets needlessly complicated and isn't the main use case
//! (The cluster API actually discourages multiple CTAs / SM)
//!
void LwdaGXbar::GenerateRouting(const UINT32 maxClusterSize)
{
    const UINT64 startTime = Platform::GetTimeNS();
    for (UINT32 clusterIdx = 0; clusterIdx < m_SMCounts.size(); clusterIdx++)
    {
        // Each cluster has an SM<->SM mapping with 'maxClusterSize' reserved entries
        // We only use a number equal to the SMs in the cluster, though
        const UINT32 clusterSize = m_SMCounts[clusterIdx];
        UINT32* pRouting =
            static_cast<UINT32*>(m_HostRouting.GetPtr()) + clusterIdx * maxClusterSize;

        // Init the mapping, the array index is the src SM and the array value is the dst SM
        // Each SM starts off reading from itself
        for (UINT32 i = 0; i < clusterSize; i++)
        {
            pRouting[i] = i;
        }

        // Modified version of Fisher-Yates shuffle
        // Avoid self-mapping (SM reading from itself) by randomly swapping
        // if the index (src) is equal to the value (dst)
        m_Random.Shuffle(clusterSize, pRouting);
        for (UINT32 i = 0; i < clusterSize; i++)
        {
            if (pRouting[i] == i)
            {
                const UINT32 tmp = pRouting[i];
                const UINT32 randOff = m_Random.GetRandom(1, clusterSize - 1);
                const UINT32 randIdx = (i + randOff) % clusterSize;
                pRouting[i] = pRouting[randIdx];
                pRouting[randIdx] = tmp;
            }
        }
    }
    const UINT64 endTime = Platform::GetTimeNS();
    VerbosePrintf("SM<->SM Routing Generation Time: %fms\n", static_cast<float>(endTime - startTime) / 1e6);
}

//! Launch one kernel per GPC on independent streams, launching from largest to smallest GPC
//! 
//! GPCs may have varying numbers of SMs, so in order to ensure full oclwpancy we may have
//! a separate cluster dimension for each GPC under test.
//! This requires separate kernel launches on parallel streams.
//!
//! Although we launch in parallel streams, we must ensure the proper launch order from
//! largest to smallest cluster, otherwise clusters may get scheduled on the incorrect GPC.
//! Launching the largest clusters first prevents small clusters from being scheduled
//! on the larger GPCs (which would prevent us from achieving full oclwpancy.
//!
RC LwdaGXbar::LaunchCgaKernels()
{
    RC rc;
    UINT32* pLaunchCount = static_cast<UINT32*>(m_HostLaunchCount.GetPtr());
    const UINT64 startTime = Platform::GetTimeNS();
    for (UINT32 clusterIdx = 0; clusterIdx < m_SMCounts.size(); clusterIdx++)
    {
        // Pass the cluster (grid) index to the kernel since it doesn't know
        // it's own launch order relative to the other resident kernels
        m_Params.clusterIdx = clusterIdx;

        // Allocate CTAs on all SMs within the GPC
        m_RunFunc.SetGridDim(m_SMCounts[clusterIdx]);

        // Configure the cluster to use all SMs on the current GPC
        // Since we launch the clusters with the largest number of CTAs first,
        // the scheduler should ensure that all SMs are utilized
        CHECK_RC(m_RunFunc.SetClusterDim(m_SMCounts[clusterIdx]));

        // Launch kernel on cluster
        VerbosePrintf("Launching Kernel on %d SM GPC\n", m_SMCounts[clusterIdx]);
        CHECK_RC(m_RunFunc.CooperativeLaunch(m_Streams[clusterIdx], m_Params));

        // To ensure we launch GPC-CGAs on the largest GPC first, ensure that the kernel
        // has launched before launching the next one
        CHECK_RC(
            Tasker::PollHw(GetTestConfiguration()->TimeoutMs(),
            [pLaunchCount, clusterIdx]()->bool
            {
                return Cpu::AtomicRead(pLaunchCount) > clusterIdx;
            },
            __FUNCTION__
        ));
    }
    const UINT64 endTime = Platform::GetTimeNS();
    VerbosePrintf("CGA Kernel Ramp Time: %fms\n", static_cast<float>(endTime - startTime) / 1e6);
    return rc;
}

// Get Patterns by name
RC LwdaGXbar::GetPatternByName()
{
    PatternClass::PatternContainer patternSets = PatternClass::GetAllPatternsList();
    size_t patternSetIdx;
    for (patternSetIdx = 0; patternSetIdx < patternSets.size(); ++patternSetIdx)
    {
        if (patternSets[patternSetIdx]->patternName == m_PatternName)
        {
            break;
        }
    }
    if (patternSetIdx == patternSets.size())
    {
        Printf(Tee::PriError, "Invalid pattern name %s\n", m_PatternName.c_str());
        return RC::BAD_PARAMETER;
    }
    const UINT32* const patternSet = patternSets[patternSetIdx]->thePattern;
    for (size_t i = 0; patternSet[i] != PatternClass::END_PATTERN; ++i)
    {
        m_Patterns.push_back(patternSet[i]);
    }
    return RC::OK;
}

// Return a UINT32 array in string form
static string PrintUINT32Array(const vector<UINT32> &arr)
{
    string ret = "[";
    UINT32 remaining = static_cast<UINT32>(arr.size());

    for (UINT32 val : arr)
    {
        ret += Utility::StrPrintf("0x%x", val);
        remaining--;
        if (remaining)
        {
            ret += ", ";
        }
    }
    ret += "]";
    return ret;
}

RC LwdaGXbar::InitFromJs()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::InitFromJs());
    
    if ((m_Patterns.size() || !m_PatternName.empty()) && m_UseRandomData)
    {
        Printf(Tee::PriError, "Patterns and/or PatternName cannot be used with UseRandomData\n");
        return RC::BAD_PARAMETER;
    }

    if (m_PatternName.empty()  && !m_UseRandomData)
    {
        m_PatternName = "ShortMats";
    }

    if (m_Patterns.empty() && !m_UseRandomData)
    {
        CHECK_RC(GetPatternByName());
    }
    
    return RC::OK; 
}

bool LwdaGXbar::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap != Lwca::Capability::SM_90)
    {
        Printf(Tee::PriLow, "LwdaGXbar does not support this SM version\n");
        return false;
    }

    return true;
}

void LwdaGXbar::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);

    Printf(pri, "LwdaGXbar Js Properties:\n");
    Printf(pri, "    RuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "    KeepRunning:                    %s\n", m_KeepRunning ? "true" : "false");
    Printf(pri, "    InnerIterations:                %llu\n", m_InnerIterations);
    if (m_SharedMemBytes)
    {
        Printf(pri, "    SharedMemBytes:                 %u\n", m_SharedMemBytes);
    }
    if (m_NumThreads)
    {
        Printf(pri, "    NumThreads:                     %u\n", m_NumThreads);
    }
    Printf(pri, "    ErrorLogLen:                    %u\n", m_ErrorLogLen);
    Printf(pri, "    VerifyResults :                 %s\n", m_VerifyResults ? "true" : "false");
    Printf(pri, "    DumpMiscompares:                %s\n", m_DumpMiscompares ? "true" : "false");
    Printf(pri, "    ReportFailingSmids:             %s\n", m_ReportFailingSmids ? "true" : "false");
    if (m_ReportFailingSmids)
    {
        Printf(pri, "    SmidFailureLimit:               %llu\n", m_SmidFailureLimit);
    }
    Printf(pri, "    UseRandomData:                  %s\n", m_UseRandomData ? "true" : "false");
    if (!m_UseRandomData)
    {
        Printf(pri, "    Patterns:                       %s\n", PrintUINT32Array(m_Patterns).c_str());
        Printf(pri, "    PatternName:                    %s\n", m_PatternName.c_str());
    }
    Printf(pri, "    UsePulse:                     %s\n", m_UsePulse ? "true" : "false");
    if (!m_UsePulse)
    {
        Printf(pri, "    LaunchDelayUs:                  %u\n", m_LaunchDelayUs);
        Printf(pri, "    PulseNs:                        %u\n", m_PulseNs);
        Printf(pri, "    DutyPct:                        %u\n", m_DutyPct);
    }

}

RC LwdaGXbar::Setup()
{
    RC rc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    CHECK_RC(LwdaStreamTest::Setup());

    // Max threads by default
    if (m_NumThreads == 0)
    {
        m_NumThreads =
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    }
    // Require full warp usage
    if (m_NumThreads % 32)
    {
        Printf(Tee::PriError,
               "NumThreads (%d) must be divisible by WarpSize (32)\n", m_NumThreads);
        return RC::BAD_PARAMETER;
    }

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("gxbar", &m_Module));

    // Get Run function (Set 0 CTAs here since that is set before launch)
    m_RunFunc = m_Module.GetFunction(
        (m_ReadOnly) ? "GXbarTestReadOnly" : "GXbarTestWriteOnly",
        0,
        m_NumThreads
    );
    CHECK_RC(m_RunFunc.InitCheck());

    // Reserve all shared memory
    CHECK_RC(m_RunFunc.SetSharedSizeMax());

    // If SharedMemBytes is 0 (default), test all shared memory
    if (m_SharedMemBytes == 0)
    {
        CHECK_RC(m_RunFunc.GetSharedSize(&m_SharedMemBytes));
    }

    // Init RNG
    m_Random.SeedRandom(GetTestConfiguration()->Seed());

    // Allocate miscompare count / error log
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeof(UINT64), &m_MiscompareCount));
    CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT64), &m_HostMiscompareCount));
    CHECK_RC(GetLwdaInstance().AllocHostMem(m_ErrorLogLen * sizeof(GXbarError), &m_HostErrorLog));

    // Allocate launch count
    CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT32), &m_HostLaunchCount));

    // Allocate memory for write used to prevent compiler optimizations
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(sizeof(DataType), &m_AccVal));

    // Generate list of GPCs from greatest to smallest SM counts
    UINT32 smCount = 0;
    UINT32 maxClusterSize = 0;
    for (UINT32 virtGpc = 0; virtGpc < pSubdev->GetGpcCount(); virtGpc++)
    {
        m_SMCounts.push_back(pSubdev->GetTpcCountOnGpc(virtGpc) * pSubdev->GetMaxSmPerTpcCount());
        smCount += m_SMCounts[virtGpc];
        maxClusterSize = std::max(maxClusterSize, m_SMCounts[virtGpc]);
    }
    MASSERT(smCount == GetBoundLwdaDevice().GetShaderCount());
    std::sort(m_SMCounts.begin(), m_SMCounts.end(), std::greater<UINT32>());
    
    // Set Maximum Cluster Size
    CHECK_RC(m_RunFunc.SetMaximumClusterSize(maxClusterSize));
    
    // Allocate routing map
    CHECK_RC(GetLwdaInstance().AllocHostMem(
             m_SMCounts.size() * maxClusterSize * sizeof(UINT32), &m_HostRouting));

    // Allocate streams
    for (UINT32 clusterIdx = 0; clusterIdx < m_SMCounts.size(); clusterIdx++)
    {
        m_Streams.push_back(GetLwdaInstance().CreateStream());
        CHECK_RC(m_Streams[clusterIdx].InitCheck());
    }

    // Set user-defined Patterns
    if (!m_UseRandomData)
    {
        const size_t patternsSizeBytes = sizeof(m_Patterns[0]) * m_Patterns.size();
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(patternsSizeBytes, &m_PatternsMem));
        CHECK_RC(m_PatternsMem.Set(m_Patterns.data(), patternsSizeBytes));
    }

    // Pulse Argument check
    if (m_DutyPct < 1 || m_DutyPct > 99)
    {
        Printf(Tee::PriError, "Incorrect Duty Percentage value set\n");
        return RC::BAD_PARAMETER;
    }

    // PeriodNs overflow check
    if ((static_cast<UINT64>(m_PulseNs) * 100) > UINT_MAX)
    {
        Printf(Tee::PriError, "PeriodNs will overflow. Reduce PulseNs value\n");
        return RC::BAD_PARAMETER;
    }

    // Set kernel parameters
    m_Params.iterations     = m_InnerIterations;
    m_Params.bytesPerCta    = m_SharedMemBytes;
    m_Params.numClusters    = static_cast<UINT32>(m_SMCounts.size());
    m_Params.maxClusterSize = maxClusterSize;
    m_Params.routingPtr     = m_HostRouting.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.launchCountPtr = m_HostLaunchCount.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.errorCountPtr  = m_MiscompareCount.GetDevicePtr();
    m_Params.errorLogPtr    = m_HostErrorLog.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.errorLogLen    = m_ErrorLogLen;
    m_Params.accValPtr      = m_AccVal.GetDevicePtr();
    m_Params.verifyResults  = static_cast<UINT32>(m_VerifyResults);
    m_Params.patternsPtr    = m_PatternsMem.GetDevicePtr();
    m_Params.numPatterns    = static_cast<UINT32>(m_Patterns.size());
    m_Params.useRandomData  = static_cast<UINT32>(m_UseRandomData);
    m_Params.usePulse       = static_cast<UINT32>(m_UsePulse);
    m_Params.periodNs       = (m_PulseNs * 100) / m_DutyPct;
    m_Params.pulseNs        = m_PulseNs;

    return rc;
}

RC LwdaGXbar::Run()
{
    RC rc;
    GpuSubdevice* pSubdev = GetBoundGpuSubdevice();
    map<UINT16, UINT64> smidMiscompareCount;

    StickyRC stickyRc;
    const bool bPerpetualRun = m_KeepRunning;
    UINT64 kernLaunchCount = 0;

    // Use clocks for determining utilization
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    bool bClocksSupported = (Platform::GetSimulationMode() != Platform::Fmodel &&
                             !Platform::IsVirtFunMode() && pPerf->IsPState30Supported());
    UINT64 clkBegin  = 0;
    UINT64 clkEnd    = 0;
    UINT64 numClocks = 0;
    UINT64 timeBeginNs = 0;
    UINT64 timeEndNs = 0;
    double durationMs = 0;


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

        // Clear CGA launch counter
        CHECK_RC(m_HostLaunchCount.Clear());

        // Use a different RNG seed each loop
        m_Params.randSeed = m_Random.GetRandom();

        // Generate SM<->SM routing
        GenerateRouting(m_Params.maxClusterSize);

        // Wait for data clearing to complete and record starting clock/time
        // Use the CPU timer since we are launching kernels across multiple streams
        CHECK_RC(GetLwdaInstance().SynchronizeContext());
        if (bClocksSupported)
        {
            CHECK_RC(pPerf->MeasureAvgClkCount(Gpu::ClkGpc, &clkBegin));
        }
        timeBeginNs = Platform::GetTimeNS();

        // Launch set of run kernels, recording elaspsed time with events
        // One kernel per GPC
        CHECK_RC(LaunchCgaKernels());
        kernLaunchCount++;

        // Wait for all run kernels to complete and record ending clock/time
        // Use the CPU timer since we are launching kernels across multiple streams
        CHECK_RC(GetLwdaInstance().SynchronizeContext());
        if (m_UsePulse && m_LaunchDelayUs)
        {
            Platform::Delay(m_LaunchDelayUs);
        }
        if (bClocksSupported)
        {
            CHECK_RC(pPerf->MeasureAvgClkCount(Gpu::ClkGpc, &clkEnd));
            numClocks += clkEnd - clkBegin;
        }
        timeEndNs = Platform::GetTimeNS();
        durationMs += static_cast<double>(timeEndNs - timeBeginNs) / 1e6;

        // Get error count
        CHECK_RC(m_MiscompareCount.Get(m_HostMiscompareCount.GetPtr(),
                                       m_HostMiscompareCount.GetSize()));
        CHECK_RC(GetLwdaInstance().Synchronize());

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
        if (m_VerifyResults && numMiscompares > 0)
        {
            Printf(Tee::PriError,
                   "LwdaGXbar found %llu miscompare(s) on loop %llu\n", numMiscompares, loop);
            GXbarError* pErrors = static_cast<GXbarError*>(m_HostErrorLog.GetPtr());

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
                           "ReadVal    : 0x%04X\n"
                           "ExpectedVal: 0x%04X\n"
                           "Iteration  : %llu\n"
                           "InnerLoop  : %d\n"
                           "Smid       : %d\n"
                           "Warpid     : %d\n"
                           "Laneid     : %d\n"
                           "\n",
                           i,
                           error.readVal,
                           error.expectedVal,
                           static_cast<UINT64>(error.iteration),
                           error.innerLoop,
                           error.smid,
                           error.warpid,
                           error.laneid
                    );
                }
                if (m_ReportFailingSmids)
                {
                    smidMiscompareCount[error.smid]++;
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
        CHECK_RC(ReportFailingSmids(smidMiscompareCount, m_SmidFailureLimit));
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

    ///////////////////////////
    // Report total bandwidth /
    ///////////////////////////

    // Callwlate actual test performance
    const UINT64 smCount = std::accumulate(m_SMCounts.begin(), m_SMCounts.end(), 0);
    const UINT64 bytesTransferred =
        smCount * kernLaunchCount * m_InnerIterations * m_SharedMemBytes;
    const double testThroughputPerSecond = static_cast<double>(bytesTransferred) / (1e3 * durationMs);

    // Callwlate theoretical performance
    //
    // 4 32-byte ports per CPC
    //
    // There are 6 SMs per CPC, each with 1 32B port. That means that the max SM throughput
    // is 32B/clk per SM for 4 or fewer SMs in the CPC, but if there are 6 SM each SM
    // gets throttled to 21B/clk per SM. To ensure consistency the HW (by default)
    // always throttles SMs to 21B/clk.
    UINT32 numCpcs = 0;
    for (UINT32 virtGpc = 0; virtGpc < pSubdev->GetGpcCount(); virtGpc++)
    {
        UINT32 physGpc = 0;
        CHECK_RC(pSubdev->VirtualGpcToHwGpc(virtGpc, &physGpc));
        numCpcs += Utility::CountBits(pSubdev->GetFsImpl()->CpcMask(physGpc));
    }
    const UINT64 networkBytesPerClock = 32 * 4 * numCpcs;
        
    // Print out performance
    VerbosePrintf("Arch Throughput (bytes/clk): %llu\n"
                  "SM Count:                    %llu\n"
                  "Bytes Transferred:           %llu\n"
                  "Throughput (bytes/s):        %f\n"
                  "\n"
                  ,networkBytesPerClock
                  ,smCount
                  ,bytesTransferred
                  ,testThroughputPerSecond);

    // Print out efficiency
    if (bClocksSupported)
    {
        const double avgGpcClkMHz = static_cast<double>(numClocks) / durationMs / 1e3;
        const double testThroughputPerClock = static_cast<double>(bytesTransferred) / numClocks;
        const double networkEfficiencyPct =
            100.0 * static_cast<double>(testThroughputPerClock) / networkBytesPerClock;
        VerbosePrintf("Avg GpcClk (MHz)             %f\n"
                      "Throughput (bytes/clk):      %f\n"
                      "Network Efficiency (%):      %f\n"
                      ,avgGpcClkMHz
                      ,testThroughputPerClock
                      ,networkEfficiencyPct);
    }

    return stickyRc;
}

RC LwdaGXbar::Cleanup()
{
    // Reset SM count vector
    vector<UINT32>().swap(m_SMCounts);
    // Free streams
    vector<Lwca::Stream>().swap(m_Streams);

    m_HostLaunchCount.Free();
    m_MiscompareCount.Free();
    m_HostMiscompareCount.Free();
    m_HostErrorLog.Free();
    m_HostRouting.Free();
    m_AccVal.Free();
    m_PatternsMem.Free();

    m_Module.Unload();
    return LwdaStreamTest::Cleanup();
}

