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

#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwdastst.h"
#include "regfile/rfstress.h"

class RegFileStress : public LwdaStreamTest
{
    public:
        RegFileStress();
        virtual ~RegFileStress() = default;
        void PrintJsProperties(Tee::Priority) override;
        bool IsSupported() override;
        RC Setup() override;
        RC Run() override;
        RC Cleanup() override;

        SETGET_PROP(RuntimeMs, UINT32);
        SETGET_PROP(InnerIterations, UINT32);
        SETGET_PROP(NumPatterns, UINT32);
        SETGET_PROP(NumBlocks, UINT32);
        SETGET_PROP(ReadOnly, bool);
        SETGET_PROP(ErrorLogLen, UINT32);
        SETGET_PROP(DumpMiscompares, UINT32);

    private:
        RegFileParams m_Params = {};
        Lwca::Module m_Module;
        Lwca::Function m_Function;
        Lwca::DeviceMemory m_PatternsMem;
        Lwca::HostMemory m_MiscompareCount;
        Lwca::HostMemory m_HostErrorLog;

        Random m_Random;
        UINT32 m_NumBlocks = 0;
        UINT32 m_NumThreads = 256;
        UINT32 m_InnerIterations = 100;
        UINT32 m_RuntimeMs = 10000;
        UINT32 m_NumPatterns = 5;
        vector<UINT32> m_Patterns;
        bool m_ReadOnly = false;
        UINT32 m_ErrorLogLen = 8192;
        bool m_DumpMiscompares = false;
};

JS_CLASS_INHERIT(RegFileStress, LwdaStreamTest, "Register File Stress Test");
CLASS_PROP_READWRITE(RegFileStress, RuntimeMs, UINT32,
                     "Run the kernel for atleast the specified amount of time");
CLASS_PROP_READWRITE(RegFileStress, InnerIterations, UINT32,
                     "Number of times per kernel launch to run a pattern over the register file");
CLASS_PROP_READWRITE(RegFileStress, NumPatterns, UINT32,
                     "Number of Randomly generated patterns");
CLASS_PROP_READWRITE(RegFileStress, NumBlocks, UINT32,
                     "Number of CTA(s) per kernel launch.");
CLASS_PROP_READWRITE(RegFileStress, ReadOnly, bool,
                     "Read Only Mode where we write once per kernel launch.");
CLASS_PROP_READWRITE(RegFileStress, ErrorLogLen, UINT32,
                     "Max number of errors that can be logged with detailed information");
CLASS_PROP_READWRITE(RegFileStress, DumpMiscompares, bool,
                     "Print detailed error information");

RegFileStress::RegFileStress()
{
    SetName("RegFileStress");
}

void RegFileStress::PrintJsProperties(Tee::Priority pri)
{
    LwdaStreamTest::PrintJsProperties(pri);
    Printf(pri, "RegFileStress Js Properties:\n");
    Printf(pri, "\tRumtimeMs:           %u\n", m_RuntimeMs);
    Printf(pri, "\tInnerIterations:     %u\n", m_InnerIterations);
    Printf(pri, "\tNumPatterns:         %u\n", m_NumPatterns);
    Printf(pri, "\tNumBlocks:           %u\n", m_NumBlocks);
    Printf(pri, "\tReadOnly:            %s\n", m_ReadOnly ? "true" : "false");
    Printf(pri, "\tErrorLogLen:         %u\n", m_ErrorLogLen);
    Printf(pri, "\tDumpMiscompares:     %s\n", m_DumpMiscompares ? "true" : "false");
}

bool RegFileStress::IsSupported()
{
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_90)
    {
        Printf(Tee::PriLow, "RegFileStress is not supported on this SM version\n");
        return false;
    }
    return true;
}

RC RegFileStress::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("rfstress", &m_Module));
    if (m_NumBlocks == 0)
    {
        m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();
    }

    // Random Init
    m_Random.SeedRandom(GetTestConfiguration()->Seed());

    if (!m_ReadOnly)
    {
        // generating a random list of patterns
        for (UINT32 i = 0; i < m_NumPatterns; i++)
        {
            m_Patterns.push_back(m_Random.GetRandom());
        }
    
        const size_t patternsSizeBytes = sizeof(m_Patterns[0]) * m_NumPatterns;
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(patternsSizeBytes, &m_PatternsMem));
        CHECK_RC(m_PatternsMem.Set(m_Patterns.data(), patternsSizeBytes));
    }
    CHECK_RC(GetLwdaInstance().AllocHostMem(sizeof(UINT64), &m_MiscompareCount));
    CHECK_RC(GetLwdaInstance().AllocHostMem(m_ErrorLogLen * sizeof(RegFileError), &m_HostErrorLog));

    // Set kernel parameters
    m_Params.patternsPtr = m_ReadOnly ? 0 : m_PatternsMem.GetDevicePtr();
    m_Params.errorLogPtr = m_HostErrorLog.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.errorCountPtr = m_MiscompareCount.GetDevicePtr(GetBoundLwdaDevice());
    m_Params.numPatterns = m_NumPatterns;
    m_Params.numInnerIterations = m_InnerIterations;
    m_Params.errorLogLen = m_ErrorLogLen;

    return rc;
}

RC RegFileStress::Run()
{
    RC rc;
    StickyRC stickyRc;
    if (m_ReadOnly)
    {
        m_Function = m_Module.GetFunction("regstressRO", m_NumBlocks, m_NumThreads);
    }
    else
    {
        m_Function = m_Module.GetFunction("regstress", m_NumBlocks, m_NumThreads);
    }
    
    CHECK_RC(m_Function.InitCheck());
    UINT64 totalNumMiscompares = 0;
    
    // For Block Dimension 256 current max available register count is 253 ~> 64K registers
    const UINT32 maxRegularRegistersPerThread = 253;
    // Essential Registers used for storing iterator counters, pointers etc
    // less bookkeeping registers required in ReadOnly mode as we already know numPatterns and
    // we don't care about pattern pointer
    const UINT32 numRegularRegisterHelpers = (m_ReadOnly) ? 5 : 7;
    const UINT32 totalRegistersInUse = (maxRegularRegistersPerThread - numRegularRegisterHelpers);
    UINT64 bytesReadPerKernel = 0;
    UINT64 bytesWrittenPerKernel = 0;
 
    if (!m_ReadOnly)
    {
        bytesReadPerKernel = m_NumBlocks * m_NumThreads * m_InnerIterations * m_NumPatterns *
                             totalRegistersInUse * sizeof(UINT32);
        bytesWrittenPerKernel = bytesReadPerKernel;
    }
    else
    {
        bytesReadPerKernel = m_NumBlocks * m_NumThreads * m_InnerIterations * sizeof(UINT32) *
                             totalRegistersInUse;
        bytesWrittenPerKernel = m_NumBlocks * m_NumThreads * sizeof(UINT32) * totalRegistersInUse;
    }

    UINT64 totalBytesRead = 0;
    UINT64 totalBytesWritten = 0;
    float totalKernelTime = 0.0;
    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());
    const UINT64 startMs = Xp::GetWallTimeMS();

    for
    (
        UINT64 loop = 0;
        (m_RuntimeMs ? (Xp::GetWallTimeMS() - startMs) < static_cast<double>(m_RuntimeMs) :
         loop < GetTestConfiguration()->Loops());
        loop++
    )
    {
        CHECK_RC(m_MiscompareCount.Clear());
        
        // For ReadOnly mode there is just one random pattern written once per kernel launch
        // To reduce register bookkeeping we pass this by value which is stored in the constant memory
        if (m_ReadOnly)
        {
            m_Params.numPatterns = m_Random.GetRandom();
        }
        CHECK_RC(startEvent.Record());
        CHECK_RC(m_Function.Launch(m_Params));
        CHECK_RC(stopEvent.Record());
        CHECK_RC(GetLwdaInstance().Synchronize());
        
        totalBytesRead += bytesReadPerKernel;
        totalBytesWritten += bytesWrittenPerKernel;
        totalKernelTime += stopEvent.TimeMsElapsedSinceF(startEvent);

        const UINT64 numMiscompares = *static_cast<UINT64*>(m_MiscompareCount.GetPtr());
        totalNumMiscompares += numMiscompares;
        if (numMiscompares > 0)
        {
            Printf(Tee::PriError,
                   "RegFileStress found %llu miscompares(s) on loop %llu\n", numMiscompares, loop);
            RegFileError* pMiscompares = static_cast<RegFileError*>(m_HostErrorLog.GetPtr());
            if (numMiscompares > m_ErrorLogLen)
            {
                Printf(Tee::PriWarn,
                       "%llu errors, but ErrorLog only contains %u entries. "
                       "Some failures may not be reported.\n",
                       numMiscompares, m_ErrorLogLen);
            }
            for (UINT32 i = 0; i < numMiscompares && i < m_ErrorLogLen; i++)
            {
                const auto& error = pMiscompares[i];
                if (m_DumpMiscompares)
                {
                    Printf(Tee::PriError, "%u\n"
                           "ReadVal     : 0x%x\n"
                           "ExpectedVal : 0x%x\n"
                           "Iteration   : %llu\n"
                           "Smid        : %u\n"
                           "WarpId      : %u\n"
                           "LaneId      : %u\n"
                           "RegisterId  : %u\n"
                           "\n",
                           i,
                           error.readVal,
                           error.expectedVal,
                           m_InnerIterations - static_cast<UINT64>(error.iteration),
                           error.smid,
                           error.warpid,
                           error.laneid,
                           error.registerid
                    );
                }
            }
            stickyRc = RC::GPU_COMPUTE_MISCOMPARE;
        }
    }

    // Register Throughput Callwlation
    printf("Total Bytes Read or Written: %llu\n", totalBytesRead + totalBytesWritten);
    return stickyRc;
}

RC RegFileStress::Cleanup()
{
    vector<UINT32>().swap(m_Patterns);
    m_PatternsMem.Free();
    m_MiscompareCount.Free();
    m_HostErrorLog.Free();
    m_Module.Unload();
    return LwdaStreamTest::Cleanup();
}
