/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <boost/math/special_functions/round.hpp>
#include "core/utility/ptrnclss.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpupm.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/pwrwait.h"
#include "gpu/tests/lwdamemtest.h"
#include "gpu/tests/lwca/mats/newlwdamats.h"

using boost::math::round;
using namespace NewLwdaMats;

namespace
{
    struct NewLwdaErrorRecord : public MemError::ErrorRecord
    {
        UINT32 blockIdx;
        UINT32 threadIdx;
        UINT32 outerIteration;
        UINT32 innerIteration;
        bool   direction;
        UINT32 patternIdx;
    };

    RC NewLwdaLogMemoryError
    (
        MemError& err,
        UINT64 location,
        UINT32 actual,
        UINT32 expected,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        UINT64 timeStamp,
        UINT32 blockIdx,
        UINT32 threadIdx,
        UINT32 outerIteration,
        UINT32 innerIteration,
        bool   direction,
        UINT32 patternIdx
    )
    {
        NewLwdaErrorRecord *pTemp = new NewLwdaErrorRecord;
        unique_ptr<MemError::ErrorRecord> temp(pTemp);
        RC rc;

        pTemp->addr           = location;
        pTemp->width          = 32;
        pTemp->actual         = actual;
        pTemp->expected       = expected;
        pTemp->pteKind        = pteKind;
        pTemp->pageSizeKB     = pageSizeKB;
        pTemp->timeStamp      = timeStamp;
        pTemp->blockIdx       = blockIdx;
        pTemp->threadIdx      = threadIdx;
        pTemp->outerIteration = outerIteration;
        pTemp->innerIteration = innerIteration;
        pTemp->direction      = direction;
        pTemp->patternIdx     = patternIdx;
        pTemp->patternOffset  = 0xffffffff;
        pTemp->patternName    = "";

        CHECK_RC(err.LogUnknownMemoryError(move(temp)));

        return OK;
    }

    // Translate combination of Fbio subpartitions, rank, ... to 1D index and back
    struct DQIdxTranslator
    {
        UINT32 subpDim;
        UINT32 pchanDim;
        UINT32 rankDim;
        UINT32 beatDim;

        UINT32 Encode
        (
            const UINT32 virtualFbio,
            const UINT32 subpartition,
            const UINT32 pseudoChannel,
            const UINT32 rank,
            const UINT32 beatOffset,
            const INT32  bitOffset
        )
        {
            UINT32 idx = virtualFbio;
            idx = (idx * subpDim)  + subpartition;
            idx = (idx * pchanDim) + pseudoChannel;
            idx = (idx * rankDim)  + rank;
            idx = (idx * beatDim)  + beatOffset;
            idx = (idx * 32)       + bitOffset;     // 32 bits in a word

            return idx;
        }

        void Decode
        (
            UINT32 idx,
            UINT32 *pVirtualFbio,
            UINT32 *pSubpartition,
            UINT32 *pPseudoChannel,
            UINT32 *pRank,
            UINT32 *pBeatOffset,
            INT32  *pBitOffset
        )
        {
            *pBitOffset      = (idx)             % 32;
            *pBeatOffset     = (idx /= 32)       % beatDim;
            *pRank           = (idx /= beatDim)  % rankDim;
            *pPseudoChannel  = (idx /= rankDim)  % pchanDim;
            *pSubpartition   = (idx /= pchanDim) % subpDim;
            *pVirtualFbio    = (idx /= subpDim);
        }
    };

    using FlushBehavior = UINT32;
    enum : UINT32
    {
        NO_FLUSH    = 0,    // Don't make intermediate flushes
        FLUSH_CHUNK = 1,    // Flush after each kernel launch
        FLUSH_OUTER = 2     // Flush after each outer iteration
    };
}

//! LWCA-based MATS (Modified Algorithmic Test Sequence) memory test.
//!
//! This test runs a LWCA kernel to perform a MATS memory test.
//! The description on how this test works is in the kernel's
//! source file, lwdamats.lw.
class NewLwdaMatsTest final : public LwdaMemTest
{
public:
    NewLwdaMatsTest();
    virtual ~NewLwdaMatsTest() = default;
    RC InitFromJs() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    // JS property accessors
    SETGET_PROP(RuntimeMs,          UINT32);
    SETGET_PROP(Iterations,         UINT32);
    SETGET_PROP(MaxInnerIterations, UINT32);
    SETGET_PROP(MaxErrorsPerBlock,  UINT32);
    SETGET_PROP(SkipErrorReporting, bool);
    SETGET_PROP(StartPhys,          UINT64);
    SETGET_PROP(EndPhys,            UINT64);
    SETGET_PROP(MinAllocSize,       UINT64);
    SETGET_PROP(NumBlocks,          UINT32);
    SETGET_PROP(NumThreads,         UINT32);
    SETGET_PROP(RunExperiments,     bool);
    SETGET_PROP(PatternName,        string);
    SETGET_PROP(UseRandomData,      bool);
    SETGET_PROP(LevelMask,          UINT32);
    SETGET_PROP(UseSafeLevels,      bool);
    SETGET_PROP(BusTest,            bool);
    SETGET_PROP(BusPatBytes,        UINT32);
    SETGET_PROP(RandomAccessBytes,  UINT32);
    SETGET_PROP(ReadOnly,           bool);
    SETGET_PROP(PrintKernelTime,    bool);
    SETGET_PROP(PulseUs,            double);
    SETGET_PROP(DelayUs,            double);
    SETGET_PROP(UseRandomShmoo,     bool);
    SETGET_PROP(UseLinearShmoo,     bool);
    SETGET_PROP(UseRandomStepWidth, bool);
    SETGET_PROP(StepUs,             double);
    SETGET_PROP(StepWidth,          UINT64);
    SETGET_PROP(StepWidthUs,        double);
    SETGET_PROP(MinStepWidthUs,     double);
    SETGET_PROP(MinPulseUs,         double);
    SETGET_PROP(TestMscg,           bool);
    SETGET_PROP(MscgEntryThreshold, double);
    SETGET_PROP(DoGCxCycles,        UINT32);
    SETGET_PROP(ForceGCxPstate,     UINT32);
    SETGET_PROP(DumpInitData,       bool);
    SETGET_PROP(DisplayAnyChunk,    bool);
    SETGET_PROP(ReportBitErrors,    bool);
    SETGET_PROP(OverflowBufferBytes,UINT32);
    SETGET_PROP(FlushBehavior,      UINT32);
    GET_PROP(NumErrors,             UINT32);

private:
    struct ChunkData
    {
        ChunkData(UINT64 ptr, UINT64 addr, UINT64 start, UINT64 end, UINT64 len,
                  GpuUtility::MemoryChunkDesc* d)
            : memDevPtr(ptr)
            , nominalAddr(addr)
            , startOffset(start)
            , endOffset(end)
            , size(len)
            , desc(d)
        { }
        RC Initialize
        (
            UINT32 seed,
            const Lwca::Instance *pLwdaInstance
        );

        UINT64 memDevPtr;
        UINT64 nominalAddr;
        UINT64 startOffset;
        UINT64 endOffset;
        UINT64 size;
        GpuUtility::MemoryChunkDesc* desc;
        LwdaMatsInput input = {};
        LwdaMatsInput initInput = {};
        UINT32 numDirections = 0;
        UINT64 memLoops = 0;
        UINT64 totalTicks = 0;
        UINT64 pulseTicks = 0;
        Random random;
        Lwca::Event snoopEvent;
    };

    RC SetupMemError();
    void CallwlateKernelGeometry();
    void SetDefaultChunkSize();
    UINT32 GetResultsSize() const;
    bool GetTestRange(UINT32 ichunk, UINT64* pStartOffset, UINT64* pEndOffset);
    RC TrackBitErrors(bool* pCorrupted, UINT64 testedMemSize);
    RC TrackErrors(const char* results, bool* pCorrupted, UINT64 testedMemSize);

    // Overriding from LwdaMemTest
    RC ReportErrors() override;
    void LogNewLwdaMemError(UINT64 fbOffset, UINT32 actual, UINT32 expected, UINT64 encFailData,
        const GpuUtility::MemoryChunkDesc& chunk);

    RC ReportAllocatedSize();
    RC DetermineTestableChunks(vector<ChunkData> *pTestableChunks);
    void InitializeLwdaMatsInputs(ChunkData *pChunk);
    RC CallwlatePulseTicks(ChunkData *pChunk);
    RC LwdaMatsLaunchWithPulse(ChunkData *pChunk);
    RC RunChunkOuterIteration(vector<ChunkData> *pTestableChunks, UINT32 outer, bool *pStop);
    RC CheckMscgCounters
    (
        UINT32 entryCountStart,
        UINT32 inGateStartUs,
        UINT32 unGateStartUs
    );
    UINT64 LwrrentProgressIteration() const;

    RC ForkConsumers(void);
    RC JoinConsumers(void);
    RC FlushBuffer(void);
    // Entry function for forked threads to handle error logs
    static void ConsumeErrors(void *);
    // Helper function to process bit error buffer
    static UINT32 ProcessBuffer
    (
        NewLwdaMatsTest *pTest,
        BitError *pBuffer,
        const UINT32 start,
        const UINT32 end,
        const UINT32 stepSize,
        const bool returnEarly
    );

    class NewLwdaReportedError final : public ReportedError
    {
    public:
        UINT32 BlockIdx;
        UINT32 ThreadIdx;
        UINT32 OuterIteration;
        UINT32 InnerIteration;
        UINT32 PatternIdx;
        bool   Direction;

        bool operator<(const NewLwdaReportedError& e) const
        {
            if (Address != e.Address) return Address < e.Address;

            // Order should be given preference to these parameters for
            // NewLwdaMats test since multiple rd/wr occur on the same address
            // so we want to order them chronologically, then by block/thread ID
            if (OuterIteration != e.OuterIteration) return OuterIteration < e.OuterIteration;
            if (InnerIteration != e.InnerIteration) return InnerIteration < e.InnerIteration;
            if (Direction != e.Direction) return Direction;
            if (PatternIdx != e.PatternIdx) return PatternIdx < e.PatternIdx;
            if (BlockIdx != e.BlockIdx) return BlockIdx < e.BlockIdx;
            if (ThreadIdx != e.ThreadIdx) return ThreadIdx < e.ThreadIdx;

            if (Actual != e.Actual) return Actual < e.Actual;
            if (Expected != e.Expected) return Expected < e.Expected;
            if (PteKind != e.PteKind) return PteKind < e.PteKind;
            return PageSizeKB < e.PageSizeKB;
        }
    };

    typedef map<NewLwdaReportedError, TimeStamps> NewLwdaReportedErrors;
    NewLwdaReportedErrors m_NewLwdaReportedErrors;

    // JS properties (many defaults set in gpudecls.js)
    UINT32 m_RuntimeMs = 0;  // Target main activity runtime in milliseconds.
                             // If set takes priority over m_Iterations.
    UINT32 m_Iterations = 0;
    UINT32 m_MaxInnerIterations = 0;
    UINT32 m_MaxErrorsPerBlock = 0;
    bool   m_SkipErrorReporting = false;
    UINT64 m_StartPhys = 0;
    UINT64 m_EndPhys = 0;
    UINT64 m_MinAllocSize = 0;
    UINT32 m_NumThreads = 0;
    bool   m_RunExperiments = false;
    string m_PatternName = "ShortMats";
    bool   m_UseRandomData = false;
    UINT32 m_LevelMask = 0x0;
    bool   m_UseSafeLevels = true;
    bool   m_BusTest = false;
    UINT32 m_BusPatBytes = 0;
    UINT32 m_RandomAccessBytes = 0;
    bool   m_ReadOnly = false;
    bool   m_PrintKernelTime = false;
    double m_PulseUs = 0;
    double m_DelayUs = 0;
    bool   m_UseRandomShmoo = false;
    bool   m_UseLinearShmoo = false;
    bool   m_UseRandomStepWidth = false;
    double m_StepUs = 10;
    UINT64 m_StepWidth = 0;
    double m_StepWidthUs = 0;
    double m_MinStepWidthUs = 0;
    double m_MinPulseUs = 0;
    bool   m_TestMscg = false;
    double m_MscgEntryThreshold = 0.75;
    UINT32 m_DoGCxCycles = GCxPwrWait::GCxModeDisabled;
    UINT32 m_ForceGCxPstate = Perf::ILWALID_PSTATE;
    bool   m_DisplayAnyChunk = true;
    bool   m_DumpInitData = false;
    UINT32 m_NumErrors = 0;

    Lwca::Module m_Module;
    Lwca::Function m_InitLwdaMats;
    Lwca::Function m_FillLwdaMats;
    Lwca::Function m_LwdaMats;
    Lwca::DeviceMemory m_PatternsMem;
    Lwca::HostMemory   m_HostResults;
    Lwca::HostMemory   m_DebugPtr;
    Lwca::HostMemory   m_OverflowBufStatus;
    Lwca::HostMemory   m_OverflowBufIdx;
    Lwca::Event m_LaunchBegin;
    Lwca::Event m_LaunchEnd;

    UINT32 m_ActualNumInnerIterations = 0;
    UINT32 m_NumOuterIterations = 0;
    UINT64 m_BytesRead = 0;
    UINT64 m_BytesWritten = 0;
    UINT64 m_KernLaunchCount = 0;
    bool m_TestedSomething = false;
    UINT64 m_LwrTestStep = 0;
    UINT64 m_RunStartMs = 0;
    bool m_Use256BitLoadsAndStores = false;

    vector<UINT32> m_Patterns;
    vector<UINT32> m_ActualPatterns;

    UINT32 m_NumBlocks = 0;
    UINT64 m_BytesPerTick = 0;

    bool m_ExternalDelay = false;
    bool m_MscgUnderTest = false;
    bool m_WasMscgEnabled = false;
    unique_ptr<GCxBubble> m_pGCxBubble;

    // Bit error reporting
    vector<UINT32> m_ErrorBitCounter;   // Keeps track of bit errors in each DQ
    vector<UINT32> m_ErrorWordCounter;  // Keeps track of how many word miscompares were reported (wordError count)
    DQIdxTranslator m_DQIdxTranslator = {};
    UINT32 m_OverflowBufferBytes = 0;
    UINT32 m_MaxOvfBlockErrors = 0;
    UINT64 m_MaxErrorsNotFlushed = 0ULL;
    UINT64 m_PrevNumErrors = 0ULL;
    bool m_ReportBitErrors = false;
    bool m_ShouldJoin = false;
    vector<Tasker::ThreadID> m_ConsumerTIDs;
    UINT32 m_NumWorkerThreads = 0;
    UINT64 m_TimeSpentFlushingUs = 0ULL;
    UINT64 m_L2ClksSpentFlushing = 0ULL;
    Gpu::ClkDomain m_L2ClkDomain = Gpu::ClkUnkUnsp;
    bool m_RecordL2Clk = true;
    FlushBehavior m_FlushBehavior = NO_FLUSH;
};

NewLwdaMatsTest::NewLwdaMatsTest()
{
    SetName("NewLwdaMatsTest");
}

void NewLwdaMatsTest::PrintJsProperties(Tee::Priority pri)
{
    LwdaMemTest::PrintJsProperties(pri);

    Printf(pri, "NewLwdaMatsTest Js Properties:\n");
    Printf(pri, "\tRuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "\tIterations:                     %u\n", m_Iterations);
    Printf(pri, "\tMaxInnerIterations:             %u\n", m_MaxInnerIterations);
    Printf(pri, "\tMaxErrorsPerBlock:              %u\n", m_MaxErrorsPerBlock);
    Printf(pri, "\tSkipErrorReporting:             %s\n", m_SkipErrorReporting ? "true" : "false");
    Printf(pri, "\tStartPhys:                      %08llx\n", m_StartPhys);
    Printf(pri, "\tEndPhys:                        %08llx\n", m_EndPhys);
    Printf(pri, "\tMinAllocSize:                   %08llx\n", m_MinAllocSize);
    Printf(pri, "\tNumBlocks:                      %u\n", m_NumBlocks);
    Printf(pri, "\tNumThreads:                     %u\n", m_NumThreads);
    Printf(pri, "\tRunExperiments:                 %s\n", m_RunExperiments ? "true" : "false");
    Printf(pri, "\tPatternName:                    %s\n", m_PatternName.c_str());
    Printf(pri, "\tUseRandomData:                  %s\n", m_UseRandomData ? "true" : "false");
    Printf(pri, "\tLevelMask:                      0x%x\n", m_LevelMask);
    Printf(pri, "\tUseSafeLevels:                  %s\n", m_UseSafeLevels ? "true" : "false");
    Printf(pri, "\tBusTest:                        %s\n", m_BusTest ? "true" : "false");
    if (m_BusTest)
    {
        Printf(pri, "\tBusPatBytes:                    %d\n", m_BusPatBytes);
    }
    Printf(pri, "\tRandomAccessBytes:              %u\n", m_RandomAccessBytes);
    Printf(pri, "\tReadOnly:                       %s\n", m_ReadOnly ? "true" : "false");
    Printf(pri, "\tPrintKernelTime:                %s\n", m_PrintKernelTime ? "true" : "false");
    Printf(pri, "\tPulseUs:                        %f\n", m_PulseUs);
    Printf(pri, "\tDelayUs:                        %f\n", m_DelayUs);
    Printf(pri, "\tUseRandomShmoo:                 %s\n", m_UseRandomShmoo ? "true" : "false");
    Printf(pri, "\tUseLinearShmoo:                 %s\n", m_UseLinearShmoo ? "true" : "false");
    if (m_UseRandomShmoo || m_UseLinearShmoo)
    {
        Printf(pri, "\tMinPulseUs:                     %f\n", m_MinPulseUs);
    }
    if (m_UseLinearShmoo)
    {
        Printf(pri, "\tStepUs:                         %f\n", m_StepUs);
    }
    if (m_StepWidthUs)
    {
        Printf(pri, "\tStepWidthUs:                    %f\n", m_StepWidthUs);
    }
    else
    {
        Printf(pri, "\tStepWidth:                      %llu\n", m_StepWidth);
    }
    Printf(pri, "\tUseRandomStepWidth:             %s\n", m_UseRandomStepWidth ? "true" : "false");
    if (m_UseRandomStepWidth)
    {
        Printf(pri, "\tMinStepWidthUs:                 %f\n", m_MinStepWidthUs);
    }
    Printf(pri, "\tTestMscg:                       %s\n", m_TestMscg ? "true" : "false");
    Printf(pri, "\tMscgEntryThreshold:             %f\n", m_MscgEntryThreshold);
    Printf(pri, "\tDoGCxCycles:                    %u\n", m_DoGCxCycles);
    if (Perf::ILWALID_PSTATE == m_ForceGCxPstate)
    {
        Printf(pri, "\tForceGCxPstate:                 invalid PState (default)\n");
    }
    else
    {
        Printf(pri, "\tForceGCxPstate:                 %u\n", m_ForceGCxPstate);
    }

    if (m_Patterns.size() && !m_UseRandomData)
    {
        Printf(pri, "\tPatterns:\t\t\t[%#.8x", m_Patterns[0]);
        for (UINT32 i = 1; i < m_Patterns.size(); i++)
        {
            Printf(pri, ", %#.8x", m_Patterns[i]);
        }
        Printf(pri, "]\n");
    }
    Printf(pri, "\tReportBitErrors:                %s\n", m_ReportBitErrors ? "true" : "false");
    if (m_ReportBitErrors)
    {
        Printf(pri, "\tOverflowBufferBytes:                   %u\n", m_OverflowBufferBytes);
        Printf(pri, "\tFlushBehavior:                         %u\n", m_FlushBehavior);
    }
}

RC NewLwdaMatsTest::InitFromJs()
{
    RC rc;

    CHECK_RC(LwdaMemTest::InitFromJs());

    // Get Patterns
    //
    // UseRandomData does not use patterns. It still loads ShortMats into m_ActualPatterns
    // though so that the kernel runs for the same duration.
    JavaScriptPtr pJs;
    rc = pJs->GetProperty(GetJSObject(), "Patterns", &m_Patterns);
    if (rc == RC::ILWALID_OBJECT_PROPERTY)
    {
        rc.Clear();
    }
    else if (m_UseRandomData)
    {
        Printf(Tee::PriError, "Patterns cannot be specified when using random data\n");
        return RC::BAD_PARAMETER;
    }

    if ((m_PatternName != "ShortMats") && m_UseRandomData)
    {
        Printf(Tee::PriError, "PatternName cannot be specified when using random data\n");
        return RC::BAD_PARAMETER;
    }

    m_ActualPatterns.clear();
    if (m_Patterns.empty())
    {
        PatternClass::PatternContainer Patterns
            = PatternClass::GetAllPatternsList();
        size_t iPattern;
        for (iPattern = 0; iPattern < Patterns.size(); iPattern++)
        {
            if (Patterns[iPattern]->patternName == m_PatternName)
            {
                break;
            }
        }
        if (iPattern < Patterns.size())
        {
            const UINT32* const ThePattern
                = Patterns[iPattern]->thePattern;
            for (size_t i=0;
                    ThePattern[i] != PatternClass::END_PATTERN;
                    i++)
            {
                m_ActualPatterns.push_back(ThePattern[i]);
            }

            // Check final number of patterns
            if (m_ActualPatterns.size()+1 > maxLwdaMatsPatterns-1)
            {
                Printf(Tee::PriError, "Too many patterns (%u) in set \"%s\", "
                                      "only %u patterns are supported\n",
                        static_cast<UINT32>(m_ActualPatterns.size()),
                        m_PatternName.c_str(),
                        static_cast<UINT32>(maxLwdaMatsPatterns-1));
                return RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            m_ActualPatterns.push_back(0);
            m_ActualPatterns.push_back(~0U);
        }
    }
    else
    {
        m_ActualPatterns = m_Patterns;
    }
    // Append first pattern as last for subsequent iterations
    m_ActualPatterns.push_back(m_ActualPatterns[0]);

    // Set flag indicating whether any kernel delay is external to the
    // memory-test kernel (for example, if we are using MSCG)
    //
    // By default the delay happens in the same kernel that tests the memory
    if (m_TestMscg || m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        m_ExternalDelay = true;
    }

    if (m_TestMscg && m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        Printf(Tee::PriError,
               "TestMscg and DoGCxCycles are mutually exclusive, test one or the other.\n");
        return RC::BAD_PARAMETER;
    }

    // Check pulsing args
    if (m_DelayUs && !m_PulseUs)
    {
        Printf(Tee::PriError, "DelayUs requires PulseUs to be set.\n");
        return RC::BAD_PARAMETER;
    }
    if (m_UseRandomShmoo && m_UseLinearShmoo)
    {
        Printf(Tee::PriError,
               "UseRandomShmoo and UseLinearShmoo are mutually exclusive, use one or the other.\n");
        return RC::BAD_PARAMETER;
    }
    if ((m_UseRandomShmoo || m_UseLinearShmoo) &&
         m_PulseUs == 0)
    {
        Printf(Tee::PriError,
               "UseRandomShmoo/UseLinearShmoo must use PulseUs to set the max pulse length\n");
        return RC::BAD_PARAMETER;
    }

    // Check StepWidth args
    if (m_StepWidth && m_StepWidthUs)
    {
        Printf(Tee::PriError,
               "StepWidth and StepWidthUs are mutually exclusive, use one or the other.\n");
        return RC::BAD_PARAMETER;
    }
    if (m_StepWidthUs && !m_PulseUs)
    {
        Printf(Tee::PriError,
               "StepWidthUs must use PulseUs to set the pulse length\n");
        return RC::BAD_PARAMETER;
    }
    if ((m_StepWidth || m_StepWidthUs) && !(m_UseRandomShmoo || m_UseLinearShmoo))
    {
        Printf(Tee::PriError,
               "StepWidth/StepWidthUs requires UseRandomShmoo/UseLinearShmoo to be in use\n");
        return RC::BAD_PARAMETER;
    }
    if (m_UseRandomStepWidth)
    {
        if (!m_UseRandomShmoo && !m_UseLinearShmoo)
        {
            Printf(Tee::PriError,
                   "UseRandomStepWidth requires UseRandomShmoo/UseLinearShmoo to be in use\n");
            return RC::BAD_PARAMETER;
        }
        if (!m_StepWidthUs)
        {
            Printf(Tee::PriError,
                   "UseRandomStepWidth must use StepWidthUs to set the max step width\n");
            return RC::BAD_PARAMETER;
        }
    }

    // Validate RandomAccessBytes usage
    if (m_RandomAccessBytes)
    {
        if (!m_UseRandomData && !m_BusTest && !m_LevelMask)
        {
            Printf(Tee::PriError,
                   "RandomAccessBytes must be used with UseRandomData, BusTest, or PAM4\n");
            return RC::BAD_PARAMETER;
        }

        if (Utility::CountBits(m_RandomAccessBytes) != 1 ||
            m_RandomAccessBytes < sizeof(uint4) ||
            m_RandomAccessBytes > 32 * sizeof(uint4))
        {
            Printf(Tee::PriError,
                   "RandomAccessBytes must be power of two between %zu and %zu\n",
                   sizeof(uint4), 32 * sizeof(uint4));
            return RC::BAD_PARAMETER;
        }
    }

    // Validate ReadOnly usage
    if (m_ReadOnly && !m_UseRandomData && !m_BusTest && !m_LevelMask)
    {
        Printf(Tee::PriError,
               "ReadOnly must be used with UseRandomData, BusTest, or PAM4\n");
               return RC::BAD_PARAMETER;
    }

    // Validate different test modes
    if ((m_UseRandomData && m_BusTest) ||
        (m_UseRandomData && m_LevelMask) ||
        (m_BusTest && m_LevelMask))
    {
        Printf(Tee::PriError,
               "UseRandomData, BusTest, and LevelMask are mutually exclusive, only use one.\n");
        return RC::BAD_PARAMETER;
    }
    if (m_BusTest)
    {
        // Check SM version
        if (GetBoundLwdaDevice().GetCapability() < Lwca::Capability::SM_60)
        {
            Printf(Tee::PriError, "BusTest is only supported on SM 6.0+.\n");
            return RC::BAD_PARAMETER;
        }

        // Set default size of bus pattern to the total length of a burst, including pseudochannels
        if (m_BusPatBytes == 0)
        {
            m_BusPatBytes =
                GetBoundGpuSubdevice()->GetFB()->GetBurstSize() *
                GetBoundGpuSubdevice()->GetFB()->GetPseudoChannelsPerSubpartition();
        }

        // The bus pattern must be able to be issued by a warp
        if (Utility::CountBits(m_BusPatBytes) != 1 ||
            m_BusPatBytes < sizeof(uint4) ||
            m_BusPatBytes > 32 * sizeof(uint4))
        {
            Printf(Tee::PriError,
                   "BusPatBytes must be power of two between %zu and %zu\n",
                   sizeof(uint4), 32 * sizeof(uint4));
            return RC::BAD_PARAMETER;
        }

        // Verify that we have enough patterns to form complete BusPats
        const size_t totalPatBytes = (m_ActualPatterns.size() - 1) * sizeof(m_ActualPatterns[0]);
        if (totalPatBytes % m_BusPatBytes)
        {
            Printf(Tee::PriError,
                   "The total number of bytes in all patterns (%zu) must be a multiple "
                   "of BusPatBytes (%d)\n",
                   totalPatBytes, m_BusPatBytes);
            return RC::BAD_PARAMETER;
        }
    }
    if (m_LevelMask)
    {
        // Check SM version
        if (GetBoundLwdaDevice().GetCapability() != Lwca::Capability::SM_86)
        {
            Printf(Tee::PriError, "LevelMask is only supported on SM 8.6\n");
            return RC::BAD_PARAMETER;
        }

        // Verify that selected levels are safe
        if (m_UseSafeLevels && m_LevelMask == 0xF)
        {
            Printf(Tee::PriError,
                   "LevelMask 0xF is not safe, to continue set UseSafeLevels=false\n");
            return RC::BAD_PARAMETER;
        }
    }

    // Take advantage of 256-bit wide loads/stores
    if (GetBoundLwdaDevice().GetCapability() == Lwca::Capability::SM_89 &&
        !m_BusTest && !m_LevelMask)
    {
        if (m_RandomAccessBytes > 0 && m_RandomAccessBytes < 32)
        {
            Printf(Tee::PriError, "RandomAccessBytes should be >=32 on SM 8.9.\n");
            return RC::BAD_PARAMETER;
        }
        m_Use256BitLoadsAndStores = true;
    }

    return rc;
}

RC NewLwdaMatsTest::Setup()
{
    RC rc;

    CHECK_RC(LwdaMemTest::Setup());
    CHECK_RC(SetupMemError());

    // Enable GCx testing or MSCG if specified on the command line
    if (m_DoGCxCycles != GCxPwrWait::GCxModeDisabled)
    {
        // Check that delay is set
        if (!m_PulseUs || !m_DelayUs)
        {
            Printf(Tee::PriError,
                   "When testing GCx 'PulseUs' and 'DelayUs' must be set.\n");
            return RC::BAD_PARAMETER;
        }

        // Check that the specified GCx mode is supported
        m_pGCxBubble = make_unique<GCxBubble>(GetBoundGpuSubdevice());
        if (!m_pGCxBubble->IsGCxSupported(m_DoGCxCycles, m_ForceGCxPstate))
        {
            Printf(Tee::PriError,
                   "DoGCxCycles=%d not supported on this system with ForceGCxPstate=%s\n",
                   m_DoGCxCycles,
                   (m_ForceGCxPstate == Perf::ILWALID_PSTATE) ?
                       "none" :
                       Utility::StrPrintf("%d", m_ForceGCxPstate).c_str());
            return RC::BAD_PARAMETER;
        }

        // For GCxBubble
        CHECK_RC(GpuTest::AllocDisplayAndSurface(false));
        m_pGCxBubble->SetGCxParams(
            GetBoundRmClient(),
            GetBoundGpuDevice()->GetDisplay(),
            GetDisplayMgrTestContext(),
            m_ForceGCxPstate,
            GetTestConfiguration()->Verbose(),
            GetTestConfiguration()->TimeoutMs(),
            m_DoGCxCycles
        );
    }
    else if (m_TestMscg)
    {
        // Check that delay is set
        if (!m_PulseUs || !m_DelayUs)
        {
            Printf(Tee::PriError,
                   "When testing MSCG 'PulseUs' and 'DelayUs' must be set.\n");
            return RC::BAD_PARAMETER;
        }

        // Check if MSCG is supported
        PMU* pPmu;
        CHECK_RC(GetBoundGpuSubdevice()->GetPmu(&pPmu));
        if (!pPmu->IsMscgSupported())
        {
            Printf(Tee::PriError, "Used testarg 'TestMscg' but MSCG is not supported\n");
            return RC::BAD_PARAMETER;
        }

        // Does the PState allow MSCG?
        Perf *pPerf = GetBoundGpuSubdevice()->GetPerf();
        UINT32 pstateNum;
        CHECK_RC(pPerf->GetLwrrentPState(&pstateNum));

        bool supported = false;
        CHECK_RC(pPmu->PStateSupportsLpwrFeature(pstateNum, PMU::LpwrFeature::MSCG, &supported));
        if (!supported)
        {
            Printf(Tee::PriError,
                   "Used testarg 'TestMscg' but MSCG is not supported at the current PState\n");
            return RC::BAD_PARAMETER;
        }

        // Save MSCG testing state
        CHECK_RC(pPmu->GetMscgEnabled(&m_WasMscgEnabled));
        m_MscgUnderTest = true;

        // Reset MSCG to reset counters
        if (m_WasMscgEnabled)
        {
            CHECK_RC(pPmu->SetMscgEnabled(false));
        }
        CHECK_RC(pPmu->SetMscgEnabled(true));
    }

    // Set default maximum number of errors per block
    if (m_MaxErrorsPerBlock == 0)
    {
        // The goal is to use as much shared memory available for storing Errors (BadValue or BitError).
        // To Callwlate this, we take the maxSharedMemPerBlock and then subtract sum of bytes that are allocated
        // in the kernel as Static Shared Memory.
        // Lwcc allocates more static shared mem than necessary so we align up. Memory alignment(?)
        const UINT32 maxSharedMemPerBlock = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK_OPTIN);
        const UINT32 staticSharedMem =
            static_cast<UINT32>(ALIGN_UP(sizeof(UINT32) * (maxLwdaMatsPatterns + 2), 16U));
        const UINT32 errTypeSize = m_ReportBitErrors ? sizeof(BitError) : sizeof(BadValue);
        m_MaxErrorsPerBlock = (maxSharedMemPerBlock - staticSharedMem) / errTypeSize;
    }

    // Check number of patterns
    if (m_Patterns.size() > maxLwdaMatsPatterns-1)
    {
        Printf(Tee::PriError, "Too many patterns (%u), only %u patterns are supported\n",
                static_cast<UINT32>(m_Patterns.size()),
                static_cast<UINT32>(maxLwdaMatsPatterns-1));
        return RC::SOFTWARE_ERROR;
    }

    // Initialize module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("ncmats", &m_Module));
    CallwlateKernelGeometry();

    // Prepare memory-test kernels
    if (m_UseRandomData)
    {
        if (m_Use256BitLoadsAndStores)
        {
            m_InitLwdaMats = m_Module.GetFunction("InitLwdaMatsRandomWide", m_NumBlocks, m_NumThreads);
            m_LwdaMats = m_Module.GetFunction(
                m_RandomAccessBytes ? "LwdaMatsRandomOffsetWide" : "LwdaMatsRandomWide",
                m_NumBlocks, m_NumThreads);
            m_FillLwdaMats = m_Module.GetFunction("FillLwdaMatsRandomWide", m_NumBlocks, m_NumThreads);
        }
        else
        {
            m_InitLwdaMats = m_Module.GetFunction("InitLwdaMatsRandom", m_NumBlocks, m_NumThreads);
            m_LwdaMats = m_Module.GetFunction(
                m_RandomAccessBytes ? "LwdaMatsRandomOffset" : "LwdaMatsRandom",
                m_NumBlocks, m_NumThreads);
            m_FillLwdaMats = m_Module.GetFunction("FillLwdaMatsRandom", m_NumBlocks, m_NumThreads);
        }
    }
    else if (m_LevelMask)
    {
        m_InitLwdaMats = m_Module.GetFunction("InitLwdaMatsPAM4", m_NumBlocks, m_NumThreads);
        m_LwdaMats = m_Module.GetFunction(
            m_RandomAccessBytes ? "LwdaMatsPAM4Offset" : "LwdaMatsPAM4",
            m_NumBlocks, m_NumThreads);
        m_FillLwdaMats = m_Module.GetFunction("FillLwdaMatsPAM4", m_NumBlocks, m_NumThreads);
    }
    else if (m_BusTest)
    {
        m_InitLwdaMats = m_Module.GetFunction("InitLwdaMatsBus", m_NumBlocks, m_NumThreads);
        m_LwdaMats = m_Module.GetFunction(
            m_RandomAccessBytes ? "LwdaMatsBusOffset" : "LwdaMatsBus",
            m_NumBlocks, m_NumThreads);
        m_FillLwdaMats = m_Module.GetFunction("FillLwdaMatsBus", m_NumBlocks, m_NumThreads);
    }
    else
    {
        if (m_Use256BitLoadsAndStores)
        {
            m_InitLwdaMats = m_Module.GetFunction("InitLwdaMatsWide", m_NumBlocks, m_NumThreads);
            m_LwdaMats = m_Module.GetFunction("LwdaMatsWide", m_NumBlocks, m_NumThreads);
        }
        else
        {
            m_InitLwdaMats = m_Module.GetFunction("InitLwdaMats", m_NumBlocks, m_NumThreads);
            m_LwdaMats = m_Module.GetFunction("LwdaMats", m_NumBlocks, m_NumThreads);
        }
    }
    int staticShMem = m_LwdaMats.GetAttribute(LW_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES);
    int totalShMem = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK_OPTIN);
    int dynamicShMem = totalShMem - staticShMem;
    m_LwdaMats.SetAttribute(LW_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT,
                            LW_SHAREDMEM_CARVEOUT_MAX_SHARED);
    m_LwdaMats.SetAttribute(LW_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, dynamicShMem);

    CHECK_RC(m_InitLwdaMats.InitCheck());
    CHECK_RC(m_LwdaMats.InitCheck());
    if (m_ReadOnly)
    {
        CHECK_RC(m_FillLwdaMats.InitCheck());
    }

    m_LaunchBegin = GetLwdaInstance().CreateEvent();
    m_LaunchEnd = GetLwdaInstance().CreateEvent();

    if (m_ReportBitErrors)
    {
        if (GetBoundGpuSubdevice()->IsSOC())
        {
            Printf(Tee::PriError, "ReportBitErrors is lwrrently only supported on dGPUs\n");
            return RC::BAD_PARAMETER;
        }

        // Overflow buffer set to twice the size of default hostResults buffer
        if (!m_OverflowBufferBytes)
        {
            m_OverflowBufferBytes = 2 * m_MaxErrorsPerBlock * m_NumBlocks * sizeof(BitError);
        }

        // Overflow buffer bytes set such that errors fit compactly and spread evenly
        m_OverflowBufferBytes = ALIGN_DOWN(m_OverflowBufferBytes, m_NumBlocks * sizeof(BitError));
        m_MaxOvfBlockErrors = m_OverflowBufferBytes / (m_NumBlocks * sizeof(BitError));

        CHECK_RC(GetLwdaInstance().AllocHostMem(m_NumBlocks * sizeof(BufferStatus), &m_OverflowBufStatus));
        CHECK_RC(GetLwdaInstance().AllocHostMem(m_NumBlocks * sizeof(UINT32), &m_OverflowBufIdx));
        CHECK_RC(m_OverflowBufStatus.Clear());
        CHECK_RC(m_OverflowBufIdx.Clear());

        FrameBuffer* pFB = GetBoundGpuSubdevice()->GetFB();
        const UINT32 bitCounterSize = pFB->GetFbioCount()
                                    * pFB->GetSubpartitions()
                                    * pFB->GetPseudoChannelsPerSubpartition()
                                    * pFB->GetRankCount()
                                    * pFB->GetBeatSize()
                                    * 32;
        m_ErrorBitCounter.resize(bitCounterSize);
        m_ErrorWordCounter.resize(bitCounterSize / 32);

        m_DQIdxTranslator =
        {
            pFB->GetSubpartitions(),
            pFB->GetPseudoChannelsPerSubpartition(),
            pFB->GetRankCount(),
            pFB->GetBeatSize()
        };

        m_NumWorkerThreads = min(m_NumBlocks, Xp::GetNumCpuCores());
    }

    CHECK_RC(GetLwdaInstance().AllocHostMem(GetResultsSize(), &m_HostResults));
    CHECK_RC(m_HostResults.Clear());

    // If requested allocate memory for dumping data
    // Useful for debugging
    if (m_DumpInitData)
    {
        const size_t patSize = m_Use256BitLoadsAndStores ? sizeof(uint4x2) : sizeof(uint4);
        CHECK_RC(GetLwdaInstance().AllocHostMem(m_NumThreads * m_NumBlocks * patSize,
                                                &m_DebugPtr));
        CHECK_RC(m_DebugPtr.Clear());
    }

    // Set the shared size of kernel to hold Errors (BadValue or BitError)
    if (m_ReportBitErrors)
    {
        CHECK_RC(m_LwdaMats.SetSharedSize(m_MaxErrorsPerBlock * sizeof(BitError)));
    }
    else
    {
        CHECK_RC(m_LwdaMats.SetSharedSize(m_MaxErrorsPerBlock * sizeof(BadValue)));
    }

    // Copy patterns to framebuffer if not using random data
    if (!m_UseRandomData)
    {
        // Print out patterns
        Printf(GetVerbosePrintPri(), "NewLwdaMats patterns:");
        for (size_t i = 0; i < m_ActualPatterns.size(); i++)
        {
            Printf(GetVerbosePrintPri(), " 0x%08x", m_ActualPatterns[i]);
        }
        Printf(GetVerbosePrintPri(), "\n");

        // Allocate memory for patterns
        const size_t patternsSize =
            sizeof(m_ActualPatterns[0]) * m_ActualPatterns.size();

        CHECK_RC(GetLwdaInstance().AllocDeviceMem(patternsSize, &m_PatternsMem));
        CHECK_RC(m_PatternsMem.Set(m_ActualPatterns.data(), patternsSize));
    }

    // Bug 2443288
    // Set default ChunkSizeMb if required to improve perf
    SetDefaultChunkSize();

    // Allocate memory to test
    CHECK_RC(AllocateEntireFramebuffer());

    // Display some chunk if requested
    if (m_DisplayAnyChunk)
    {
        CHECK_RC(DisplayAnyChunk());
    }

    // Setup L2 clk domain to use
    // Newer GPUs don't have ltcclk. They have a single xbarclk that drives both the XBAR and the L2 caches.
    m_L2ClkDomain = GetBoundGpuSubdevice()->HasDomain(Gpu::ClkLtc) ?
                    Gpu::ClkLtc : Gpu::ClkXbar;

    return RC::OK;
}

RC NewLwdaMatsTest::Cleanup()
{
    m_LaunchBegin.Free();
    m_LaunchEnd.Free();

    // Free memory allocated to m_Patterns
    vector<UINT32> dummy1;
    m_Patterns.swap(dummy1);

    // Free memory allocated to m_ActualPatterns
    vector<UINT32> dummy2;
    m_ActualPatterns.swap(dummy2);

    m_PatternsMem.Free();
    m_HostResults.Free();
    m_DebugPtr.Free();
    m_Module.Unload();
    m_NumErrors = 0;

    m_pGCxBubble.reset();
    if (m_TestMscg && m_MscgUnderTest)
    {
        m_MscgUnderTest = false;
        PMU* pPmu;
        if (OK == GetBoundGpuSubdevice()->GetPmu(&pPmu))
        {
            pPmu->SetMscgEnabled(m_WasMscgEnabled);
        }
    }

    if (m_ReportBitErrors)
    {
        m_OverflowBufStatus.Free();
        m_OverflowBufIdx.Free();

        vector<UINT32> dummy3;
        m_ErrorBitCounter.swap(dummy3);
        vector<UINT32> dummy4;
        m_ErrorWordCounter.swap(dummy4);
        vector<Tasker::ThreadID> dummy5;
        m_ConsumerTIDs.swap(dummy5);
    }

    return LwdaMemTest::Cleanup();
}

RC NewLwdaMatsTest::ChunkData::Initialize
(
    UINT32 seed,
    const Lwca::Instance *pLwdaInstance
)
{
    RC rc;

    random.SeedRandom(seed);

    snoopEvent = pLwdaInstance->CreateEvent();
    CHECK_RC(snoopEvent.InitCheck());

    return rc;
}

//--------------------------------------------------------------------
//! \brief Configure MemError to print extra columns in PrintErrorLog()
//!
RC NewLwdaMatsTest::SetupMemError()
{
#define GET_CELL(printfArgs)                                                \
    [&](const MemError::ErrorRecord &errArg, const FbDecode &dec)->string   \
    {                                                                       \
        if (dynamic_cast<const NewLwdaErrorRecord*>(&errArg) == nullptr)    \
            return "";                                                      \
        const NewLwdaErrorRecord &err =                                     \
            static_cast<const NewLwdaErrorRecord&>(errArg);                 \
        return Utility::StrPrintf printfArgs;                               \
    }

    for (unique_ptr<MemError> &pMemError: GetMemErrors())
    {
        pMemError->ExtendErrorLog("BID", "Block Index",
                                  GET_CELL(("%u", err.blockIdx)));
        pMemError->ExtendErrorLog("OUTER", "Outer iteration",
                                  GET_CELL(("%u", err.outerIteration)));
        pMemError->ExtendErrorLog("INNER", "Inner iteration",
                                  GET_CELL(("%u", err.innerIteration)));
        pMemError->ExtendErrorLog("DIR", "Direction: F = forward, R = reverse",
                                  GET_CELL(("%c", err.direction ? 'F' : 'R')));
        if (m_UseRandomData || m_BusTest)
        {
            pMemError->ExtendErrorLog("LNUM", "Loop Number",
                                  GET_CELL(("%u", err.patternIdx)));
        }
        else
        {
            pMemError->ExtendErrorLog("PID", "Pattern Index",
                                  GET_CELL(("%u", err.patternIdx)));
        }
    }
    return OK;
}

void NewLwdaMatsTest::CallwlateKernelGeometry()
{
    if (m_NumThreads == 0)
    {
        m_NumThreads =
            GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    }

    if (m_NumBlocks == 0)
    {
        switch (GetBoundLwdaDevice().GetCapability())
        {
            // GA100 requires higher TPC utilization in order to achieve maximum throughput
            // (This is especially true when ReadOnly=1, gpcclk and NumBlocks can limit perf)
            case Lwca::Capability::SM_80:
            // TODO Treat GH100 the same as GA100
            case Lwca::Capability::SM_90:
            // The same on CheetAh
            case Lwca::Capability::SM_87:
                m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();
                break;

            // Otherwise we launch number of blocks equal to the number of L2 slices.
            // This seems to work well across different architectures.
            default:
                m_NumBlocks = min(GetBoundLwdaDevice().GetShaderCount(),
                                  GetBoundGpuSubdevice()->GetFB()->GetL2SliceCount());
                break;
        }
    }

    // If we are in random-offset mode then each kernel "tick" accesses 16/32 bytes
    const size_t patSize = m_Use256BitLoadsAndStores ? sizeof(uint4x2) : sizeof(uint4);
    m_BytesPerTick = (m_NumThreads * m_NumBlocks) * patSize;
}

// When RandomAccessBytes is used, certain SKUs have issues with random accesses (Bug 2443288)
//
// The expected cause is thrashing of the Translation Lookaside Buffer (TLB) which caches the
// Virt->Phys address translations of the MMU.
//
// Workaround this issue by setting ChunkSizeMb to the size at which this thrashing starts to occur
// when RandomAccessBytes=128 (smaller values have more thrashing).
//
void NewLwdaMatsTest::SetDefaultChunkSize()
{
    // Only set ChunkSizeMb if we haven't specified it on the command line and
    // we are using RandomAccessBytes
    if (m_RandomAccessBytes && !m_ChunkSizeMb)
    {
        const Lwca::Capability::Enum cap = GetBoundLwdaDevice().GetCapability();
        switch (cap)
        {
            // Do nothing for CheetAh and old archs
            case Lwca::Capability::SM_50:
            case Lwca::Capability::SM_52:
            case Lwca::Capability::SM_53:
            case Lwca::Capability::SM_72:
                break;

            case Lwca::Capability::SM_60:
                m_ChunkSizeMb = 2 * 1024;
                break;
            case Lwca::Capability::SM_61:
                m_ChunkSizeMb = 6 * 1024;
                break;
            case Lwca::Capability::SM_70:
                m_ChunkSizeMb = 8 * 1024;
                break;
            case Lwca::Capability::SM_75:
                m_ChunkSizeMb = 12 * 1024;
                break;
            case Lwca::Capability::SM_80:
            case Lwca::Capability::SM_86:
            case Lwca::Capability::SM_89:
                // GA100 was tested with ChunkSizeMb up to 48GB without perf degradation,
                // but leave it the same as Turing at 12GB since 96GB SKUs are untested
                m_ChunkSizeMb = 12 * 1024;
                break;
            case Lwca::Capability::SM_87:
            case Lwca::Capability::SM_90:
                // TODO Tune this once GH100 comes back
                m_ChunkSizeMb = 12 * 1024;
                break;
            default:
                MASSERT(!"Define SetDefaultChunkSize for this architecture!");
                break;
        }
    }
}

UINT32 NewLwdaMatsTest::GetResultsSize() const
{
    // We subtract 1 from m_MaxErrorsPerBlock since the RangeErrors/RangeBitErrors struct
    // already reserves memory for 1 BadValue
    const UINT32 blockSize = (m_ReportBitErrors)
        ? (sizeof(RangeBitErrors) + (m_MaxOvfBlockErrors * m_NumBlocks - 1) * sizeof(BitError))
        : (sizeof(RangeErrors) + (m_MaxErrorsPerBlock * m_NumBlocks - 1) * sizeof(BadValue));
    return blockSize;
}

bool NewLwdaMatsTest::GetTestRange
(
    UINT32 ichunk,
    UINT64* pStartOffset,
    UINT64* pEndOffset
)
{
    // Determine physical address and size of chunk.  If
    // non-contiguous, pretend the chunk starts at phys addr 0 so that
    // code that relies on Start/End/StartPhys/EndPhys keeps working.
    //
    const Lwca::ClientMemory& mem = GetLwdaChunk(ichunk);
    const UINT64 physAddr = (GetChunkDesc(ichunk).contiguous ?
                             VirtualToPhysical(mem.GetDevicePtr()) : 0);
    const UINT64 size = mem.GetSize();

    // Determine requested physical memory range
    UINT64 startAddr = GetStartLocation();
    startAddr *= 1024*1024;
    UINT64 endAddr = GetEndLocation();
    if (endAddr == 0)
    {
        endAddr = ~endAddr;
    }
    else
    {
        endAddr = (endAddr+1)*1024*1024;
    }

    // Skip test if the memory block is outside of the tested range
    if ((startAddr > physAddr + size)
        || (endAddr <= physAddr))
    {
        return false;
    }

    // Adjust start address to the beginning of allocated block
    if (startAddr < physAddr)
    {
        startAddr = physAddr;
    }

    // Apply physical address boundaries
    if (m_StartPhys != 0)
    {
        if (m_StartPhys < startAddr || m_StartPhys > endAddr)
        {
            return false;
        }
        startAddr = m_StartPhys;
    }
    if (m_EndPhys != 0)
    {
        if (m_EndPhys < startAddr || m_EndPhys > endAddr)
        {
            return false;
        }
        endAddr = m_EndPhys;
    }

    // Compute offsets from the beginning of the memory block
    *pStartOffset = startAddr - physAddr;
    *pEndOffset = endAddr - physAddr;

    // Skip the block if we landed beyond the end
    if (*pStartOffset >= size)
    {
        return false;
    }

    // Clamp end offset to the end of the allocated block
    if (*pEndOffset > size)
    {
        *pEndOffset = size;
    }
    *pEndOffset &= ~static_cast<UINT64>(3); // align on 32-bit words

    return true;
}

RC NewLwdaMatsTest::Run()
{
    RC rc;

    Printf(GetVerbosePrintPri(), "NewLwdaMats is testing LWCA device \"%s\"\n",
            GetBoundLwdaDevice().GetName().c_str());

    // Check patterns
    if (m_ActualPatterns.empty())
    {
        Printf(Tee::PriError, "Patterns should have been set already!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(ReportAllocatedSize());

    // Disable the RC watchdog on the subdevice for the duration of the test
    LwRm::DisableRcWatchdog disableRcWatchdog(GetBoundGpuSubdevice());

    // Measure test time
    m_BytesRead = 0;
    m_BytesWritten = 0;

    // Callwlate number of outer/inner iterations
    m_ActualNumInnerIterations =
        m_RuntimeMs ? m_MaxInnerIterations : min(m_MaxInnerIterations, m_Iterations);
    m_NumOuterIterations = (m_Iterations - 1) / m_ActualNumInnerIterations + 1;
    Printf(GetVerbosePrintPri(),
           "NewLwdaMats outer iterations: %s, inner iterations: %u\n",
           m_RuntimeMs ?
               Utility::StrPrintf("%u ms", m_RuntimeMs).c_str() :
               Utility::StrPrintf("%u", m_NumOuterIterations).c_str(),
           m_ActualNumInnerIterations);

    // Initialize some test variables
    m_TestedSomething = false;
    bool resultsCorrupted = false;
    m_NumErrors = 0;

    vector<ChunkData> testableChunks;

    // Determine all the chunks that are testable
    CHECK_RC(DetermineTestableChunks(&testableChunks));

    m_LwrTestStep = 0;
    const UINT64 totalExpectedSteps = m_RuntimeMs ? m_RuntimeMs :
        (static_cast<UINT64>(NumChunks()) * m_NumOuterIterations) + 1;
    PrintProgressInit(totalExpectedSteps);

    UINT64 testedMemSize = 0;
    // Test subsequent chunks
    for (auto& chunk : testableChunks)
    {
        InitializeLwdaMatsInputs(&chunk);

        // Callwlate the length of the pulse if PulseUs is in use
        if (m_PulseUs)
        {
            CHECK_RC(CallwlatePulseTicks(&chunk));
        }

        // Ilwalidate L2 cache before clearing tested memory
        // so that the clearing will fully initialize L2
        // to minimize number of L2 misses during the test
        if (m_RunExperiments)
        {
            CHECK_RC(GetBoundGpuSubdevice()->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN_FB_FLUSH));
        }

        // Initialize memory
        CHECK_RC(m_InitLwdaMats.Launch(chunk.initInput));
        testedMemSize += chunk.size;

        // Dump initialized memory if requested
        if (m_DumpInitData)
        {
            CHECK_RC(GetLwdaInstance().Synchronize());
            if (m_Use256BitLoadsAndStores)
            {
                const uint4x2* pDebug = reinterpret_cast<const uint4x2*>(m_DebugPtr.GetPtr());
                for (UINT32 i = 0; i < m_NumThreads * m_NumBlocks; i++)
                {
                    Printf(Tee::PriNormal,
                           "%p: 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
                           pDebug + i, pDebug[i].a.x, pDebug[i].a.y, pDebug[i].a.z, pDebug[i].a.w,
                           pDebug[i].b.x, pDebug[i].b.y, pDebug[i].b.z, pDebug[i].b.w);
                }
            }
            else
            {
                const uint4* pDebug = reinterpret_cast<const uint4*>(m_DebugPtr.GetPtr());
                for (UINT32 i = 0; i < m_NumThreads * m_NumBlocks; i++)
                {
                    Printf(Tee::PriNormal, "%p: 0x%08x 0x%08x 0x%08x 0x%08x\n",
                           pDebug + i, pDebug[i].x, pDebug[i].y, pDebug[i].z, pDebug[i].w);
                }
            }
        }
    }

    // Initiate perfmon experiments
    if (m_RunExperiments)
    {
        CHECK_RC(GetLwdaInstance().Synchronize());
        GpuPerfmon* pPerfMon = nullptr;
        CHECK_RC(GetBoundGpuSubdevice()->GetPerfmon(&pPerfMon));
        CHECK_RC(pPerfMon->BeginExperiments());
    }

    UINT32 entryCountStart = 0;
    UINT32 inGateStartUs = 0;
    UINT32 unGateStartUs = 0;

    // Get the initial MSCG state
    if (m_TestMscg)
    {
        PMU* pPmu;
        CHECK_RC(GetBoundGpuSubdevice()->GetPmu(&pPmu));
        CHECK_RC(pPmu->GetMscgEntryCount(&entryCountStart));
        CHECK_RC(pPmu->GetLwrrentPgTimeUs(PMU::ELPG_MS_ENGINE, &inGateStartUs, &unGateStartUs));
    }

    m_KernLaunchCount = 0;
    bool stop = false;

    Perf *pPerf = GetBoundGpuSubdevice()->GetPerf();
    UINT64 l2ClkStart = 0;
    UINT64 l2ClkEnd = 0;
    m_RecordL2Clk = false;

    // Add an event for callwlating exelwtion time
    Lwca::Event startEvent(GetLwdaInstance().CreateEvent());
    CHECK_RC(startEvent.Record());

    if (m_ReportBitErrors)
    {
        CHECK_RC(ForkConsumers());
    }

    if (!Platform::IsVirtFunMode() && !Platform::UsesLwgpuDriver())
    {
        if (pPerf->MeasureAvgClkCount(m_L2ClkDomain, &l2ClkStart) == RC::OK)
        {
            m_RecordL2Clk = true;
        }
    }
    m_RunStartMs = Xp::GetWallTimeMS();
    UINT32 outer = 0;
    do
    {
        CHECK_RC(RunChunkOuterIteration(&testableChunks, outer, &stop));
        outer++;
        if (m_ReportBitErrors && m_FlushBehavior == FLUSH_OUTER)
        {
            CHECK_RC(FlushBuffer());
        }
        if (m_RuntimeMs)
        {
            stop = stop || (Xp::GetWallTimeMS() - m_RunStartMs) >= m_RuntimeMs;
        }
        else
        {
            stop = stop || outer >= m_NumOuterIterations;
        }
    } while (!stop);
    MASSERT(m_KernLaunchCount > 0);

    // Add an event for callwlating exelwtion time
    Lwca::Event stopEvent(GetLwdaInstance().CreateEvent());
    CHECK_RC(stopEvent.Record());
    CHECK_RC(stopEvent.Synchronize());
    if (m_RecordL2Clk)
    {
        CHECK_RC(pPerf->MeasureAvgClkCount(m_L2ClkDomain, &l2ClkEnd));
    }

    if (m_ReportBitErrors)
    {
        CHECK_RC(GetLwdaInstance().Synchronize());
        CHECK_RC(JoinConsumers());
    }

    // Remember time it took for the kernels to execute
    const double timeElapsedMs = stopEvent.TimeMsElapsedSinceF(startEvent);

    if (m_pGCxBubble)
    {
        // Print GCx counters if in use
        m_pGCxBubble->PrintStats();
    }
    else if (m_TestMscg)
    {
        // Check MSCG counters and fail if the tolerance is exceeded
        CHECK_RC(CheckMscgCounters(entryCountStart, inGateStartUs, unGateStartUs));
    }

    // Finalize perfmon experiments
    if (m_RunExperiments)
    {
        CHECK_RC(GetLwdaInstance().Synchronize());
        GpuPerfmon* pPerfMon = nullptr;
        CHECK_RC(GetBoundGpuSubdevice()->GetPerfmon(&pPerfMon));
        CHECK_RC(pPerfMon->EndExperiments());
    }

    CHECK_RC(GetLwdaInstance().Synchronize());

    // Keep track of reported errors
    if (m_ReportBitErrors)
    {
        CHECK_RC(TrackBitErrors(&resultsCorrupted, testedMemSize));
    }
    else
    {
        CHECK_RC(TrackErrors(reinterpret_cast<const char*>(m_HostResults.GetPtr()),
                             &resultsCorrupted, testedMemSize));
    }

    if (resultsCorrupted || (m_NumErrors > 0))
    {
        PrintErrorUpdate(LwrrentProgressIteration(), RC::BAD_MEMORY);
    }

    if (!m_TestedSomething)
    {
        Printf(Tee::PriError, "Error: NewLwdaMats is unable to test the "
                              "specified memory range.\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    // Print time spent flushing buffer
    double timeSpentFlushingMs = m_TimeSpentFlushingUs / 1000.0;
    if (m_ReportBitErrors && m_FlushBehavior != NO_FLUSH)
    {
        Printf
        (
            GetVerbosePrintPri(),
            "%f ms out of %f ms spent flushing buffer\n",
            timeSpentFlushingMs,
            timeElapsedMs
        );
    }

    // Callwlate total bandwidth
    const double execTime = (timeElapsedMs / 1000.0) - (timeSpentFlushingMs / 1000.0);
    if (execTime > 0.0)
    {
        const INT32 pri = GetMemError(0).GetAutoStatus() ?
            Tee::PriNormal : GetVerbosePrintPri();

        UINT64 allocatedSizeBytes = 0;
        for (UINT32 i = 0; i < NumChunks(); i++)
        {
            allocatedSizeBytes += GetLwdaChunk(i).GetSize();
        }
        if (m_RecordL2Clk &&
            (allocatedSizeBytes <= GetBoundGpuSubdevice()->GetFB()->GetL2CacheSize()))
        {
            const UINT64 l2ClkCounts = (l2ClkEnd - l2ClkStart) - m_L2ClksSpentFlushing;
            const UINT64 avgL2FreqHz = static_cast<UINT64>(l2ClkCounts / execTime);
            GpuTest::RecordL2Bps(static_cast<double>(m_BytesRead),
                                 static_cast<double>(m_BytesWritten),
                                 avgL2FreqHz,
                                 execTime,
                                 static_cast<Tee::Priority>(pri));
        }
        GpuTest::RecordBps(static_cast<double>(m_BytesRead + m_BytesWritten),
                           execTime, static_cast<Tee::Priority>(pri));
    }

    if (resultsCorrupted)
    {
        Printf(Tee::PriWarn, "The results may be unreliable because the memory "
                             "which holds them was corrupted.\n");
    }
    rc = ReportErrors();
    if (rc == RC::OK)
    {
        if (resultsCorrupted)
        {
            return RC::BAD_MEMORY;
        }

        PrintProgressUpdate(totalExpectedSteps);
    }

    return rc;
}

RC NewLwdaMatsTest::ReportAllocatedSize()
{
    UINT64 size = 0;
    for (UINT32 i = 0; i < NumChunks(); i++)
    {
        size += GetLwdaChunk(i).GetSize();
    }
    if ((m_MinAllocSize > 0) && (size < m_MinAllocSize))
    {
        Printf(Tee::PriError, "Error: NewLwdaMats expected to allocate %llu "
            "bytes but it could only allocate %llu bytes!\n",
            m_MinAllocSize, size);
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    const INT32 pri = (m_MinAllocSize > 0) ? Tee::PriNormal : GetVerbosePrintPri();
    Printf(pri,
        "NewLwdaMats allocated 0x%llX bytes (%.1f MB) for testing\n",
        size, size / (1024.0*1024.0));

    return RC::OK;
}

RC NewLwdaMatsTest::DetermineTestableChunks(vector<ChunkData> *pTestableChunks)
{
    RC rc;

    pTestableChunks->reserve(NumChunks());
    for (UINT32 ichunk = 0; ichunk < NumChunks(); ichunk++)
    {
        // Limit test to the current range
        UINT64 startOffset;
        UINT64 endOffset;
        if (!GetTestRange(ichunk, &startOffset, &endOffset))
        {
            continue;
        }

        const UINT64 devPtr = GetLwdaChunk(ichunk).GetDevicePtr();
        const UINT64 nominalAddr = (GetChunkDesc(ichunk).contiguous ?
                                    VirtualToPhysical(devPtr) : 0);

        if (startOffset >= endOffset)
        {
            Printf(Tee::PriError,
                    "Unsupported test range %08llX to %08llX.\n",
                    nominalAddr + startOffset,
                    nominalAddr + endOffset);
            return RC::ILWALID_RAM_AMOUNT;
        }

        // Check if we can test the chunk given the current test parameters
        const UINT64 size = GetLwdaChunk(ichunk).GetSize();
        if (size < m_BytesPerTick)
        {
            continue;
        }

        GpuUtility::MemoryChunkDesc* desc = &GetChunkDesc(ichunk);
        pTestableChunks->emplace_back(devPtr, nominalAddr, startOffset, endOffset, size, desc);

        CHECK_RC(pTestableChunks->back().Initialize(
            GetTestConfiguration()->Seed(),
            GetLwdaInstancePtr()));
    }
    if (pTestableChunks->empty())
    {
        Printf(Tee::PriError,
               "Target memory amount is too small for the current number of LWCA threads. "
               "Consider decreasing NumThreads or NumBlocks.\n");
        return RC::BAD_PARAMETER;
    }

    return rc;
}

void NewLwdaMatsTest::InitializeLwdaMatsInputs(ChunkData *pChunk)
{
    const UINT64 startDevPtr = pChunk->memDevPtr + pChunk->startOffset;
    const UINT64 endDevPtr = pChunk->memDevPtr + pChunk->endOffset;

    // Initialize memory with the first pattern
    pChunk->initInput =
    {
        /*state*/           SaveState(),
        /*mem*/             startDevPtr,
        /*memEnd*/          endDevPtr,
        /*patterns*/        m_PatternsMem.GetDevicePtr(),
        /*results*/         m_HostResults.GetDevicePtr(GetBoundLwdaDevice()),
        /*debugPtr*/        m_DumpInitData ? m_DebugPtr.GetDevicePtr(GetBoundLwdaDevice()) : 0,
        /*randSeed*/        static_cast<UINT64>(GetTestConfiguration()->Seed()),
        /*numTicks*/        0,
        /*delayNs*/         0,
        /*numIterations*/   0,
        /*numPatterns*/     static_cast<UINT32>(m_ActualPatterns.size()),
        /*maxGlobalErrors*/ m_MaxErrorsPerBlock * m_NumBlocks,
        /*outerIter*/       0,
        /*maxLocalErrors*/  m_MaxErrorsPerBlock,
        /*randAccessBytes*/ 0,
        /*readOnly*/        0,
        /*busPatBytes*/     m_BusPatBytes,
        /*levelMask*/       m_LevelMask,
        /*useSafeLevels*/   static_cast<UINT32>(m_UseSafeLevels),
        /*reportBitErrors*/ m_ReportBitErrors,
        /*ovfHandler*/      {},
        /*skipErrReport*/   static_cast<UINT32>(m_SkipErrorReporting)
    };

    // Prepare test parameters
    pChunk->input =
    {
        /*state*/           SaveState(),
        /*mem*/             startDevPtr,
        /*memEnd*/          endDevPtr,
        /*patterns*/        m_PatternsMem.GetDevicePtr(),
        /*results*/         m_HostResults.GetDevicePtr(GetBoundLwdaDevice()),
        /*debugPtr*/        m_DumpInitData ? m_DebugPtr.GetDevicePtr(GetBoundLwdaDevice()) : 0,
        /*randSeed*/        static_cast<UINT64>(GetTestConfiguration()->Seed()),
        /*numTicks*/        0,
        /*delayNs*/         0,
        /*numIterations*/   m_ActualNumInnerIterations,
        /*numPatterns*/     static_cast<UINT32>(m_ActualPatterns.size()),
        /*maxGlobalErrors*/ m_MaxErrorsPerBlock * m_NumBlocks,
        /*outerIter*/       0,
        /*maxLocalErrors*/  m_MaxErrorsPerBlock,
        /*randAccessBytes*/ m_RandomAccessBytes,
        /*readOnly*/        m_ReadOnly,
        /*busPatBytes*/     m_BusPatBytes,
        /*levelMask*/       m_LevelMask,
        /*useSafeLevels*/   static_cast<UINT32>(m_UseSafeLevels),
        /*reportBitErrors*/ m_ReportBitErrors,
        /*ovfHandler*/      {
                                m_ReportBitErrors ? m_OverflowBufIdx.GetDevicePtr(GetBoundLwdaDevice()) : 0,
                                m_ReportBitErrors ? m_OverflowBufStatus.GetDevicePtr(GetBoundLwdaDevice()) : 0,
                                m_ReportBitErrors ? m_MaxOvfBlockErrors : 0
                            },
        /*skipErrReport*/   static_cast<UINT32>(m_SkipErrorReporting)
    };

    // Printf test configuration
    const double memSize = (pChunk->input.memEnd- pChunk->input.mem)/(1024.0*1024.0);
    Printf(GetVerbosePrintPri(), "NewLwdaMats test configuration:\n"
            "    numBlocks       %u\n"
            "    numThreads      %u\n"
            "    mem             0x%08llX\n"
            "    memEnd          0x%08llX (%.1fMB)\n"
            "    numIterations   %u\n"
            "    numPatterns     %u\n"
            "    patterns        0x%08llX\n"
            "    maxGlobalErrors %u\n"
            "    results         0x%08llX\n",
            static_cast<unsigned>(m_NumBlocks), static_cast<unsigned>(m_NumThreads),
            pChunk->input.mem,
            pChunk->input.memEnd,
            memSize,
            static_cast<unsigned>(pChunk->input.numIterations),
            static_cast<unsigned>(pChunk->input.numPatterns),
            pChunk->input.patterns,
            static_cast<unsigned>(pChunk->input.maxGlobalErrors),
            pChunk->input.results);
    Printf(GetVerbosePrintPri(),
           "NewLwdaMats: Running %s on %.1fMB of memory,"
            " phys addr 0x%08llX..0x%08llX\n",
            m_RuntimeMs ?
                Utility::StrPrintf("%u ms", m_RuntimeMs).c_str() :
                Utility::StrPrintf("%u iterations",
                    pChunk->input.numIterations*m_NumOuterIterations).c_str(),
            memSize,
            pChunk->nominalAddr + (startDevPtr - pChunk->memDevPtr),
            pChunk->nominalAddr + (endDevPtr - pChunk->memDevPtr));

    const UINT64 targetSize = endDevPtr - startDevPtr;
    const UINT64 memoryLeft = targetSize % m_BytesPerTick;

    // Print how much memory of the chunk the LWCA kernel skips
    if (memoryLeft)
    {
        // Warps run in multiples of a specific number of bytes
        Printf(Tee::PriLow, "Warning: Skipping the last %llu bytes from this chunk.\n",
               memoryLeft);
    }

    pChunk->numDirections = 2; // forward and reverse
    const UINT32 patternLoops = (m_BusTest || m_UseRandomData) ?
                                DEFAULT_PATTERN_ITERATIONS :
                                pChunk->input.numPatterns - 1;
    const UINT64 actualSize = targetSize - memoryLeft;
    pChunk->memLoops = actualSize / m_BytesPerTick;
    pChunk->totalTicks = pChunk->input.numIterations *
        pChunk->numDirections * patternLoops * pChunk->memLoops;
}

RC NewLwdaMatsTest::CallwlatePulseTicks(ChunkData *pChunk)
{
    RC rc;

    LwdaMatsInput timingInput = pChunk->input;
    const LwdaMatsInput timingInit = pChunk->initInput;

    // Don't report errors while callwlating pulse ticks
    timingInput.skipErrReport = static_cast<UINT32>(true);

    // Start PulseTicks at 1000 (1GB on most GV100 boards)
    // It doesn't really matter what we start at, as the algorithm will adjust
    UINT64 pulseTicks = std::min(static_cast<UINT64>(1000), pChunk->totalTicks);

    constexpr UINT32 maxTries = 8;
    UINT32 numTries = 0;
    double timeUs;
    double scale = 1.0;
    do
    {
        // Re-initialize (also clears errors, indirectly resets L2 cache)
        //
        // Since we always test starting from the same location,
        // L2 cache hits can result in incorrect timing
        CHECK_RC(m_InitLwdaMats.Launch(timingInit));

        // Change the runtime based on the previously computed scaling factor
        pulseTicks = static_cast<UINT64>(std::max(pulseTicks * scale, 1.0));
        timingInput.numTicks = pulseTicks;

        // Launch memory kernel to get time
        CHECK_RC(m_LaunchBegin.Record());
        CHECK_RC(m_LwdaMats.Launch(timingInput));
        numTries++;
        CHECK_RC(m_LaunchEnd.Record());
        CHECK_RC(m_LaunchEnd.Synchronize());

        timeUs = m_LaunchEnd.TimeMsElapsedSinceF(m_LaunchBegin) * 1000.0;
        scale = m_PulseUs / timeUs;
        Printf(Tee::PriLow, "Attempt #%d (%llu Ticks): %f us (%f%% error)\n",
            numTries, pulseTicks, timeUs, (100.0 / scale) - 100.0);
    } while (std::abs(1.0 - scale) > 0.01 && numTries < maxTries);
    VerbosePrintf(
        "Target PulseUs: %f us\n"
        "Actual PulseUs: %f us\n"
        "PulseTicks:     %llu\n",
        m_PulseUs, timeUs, pulseTicks);

    pChunk->pulseTicks = pulseTicks;
    return rc;
}

RC NewLwdaMatsTest::LwdaMatsLaunchWithPulse(ChunkData *pChunk)
{
    RC rc;
    Tasker::DetachThread detach;
    const UINT32 patternLoops = (m_BusTest || m_UseRandomData) ?
                                DEFAULT_PATTERN_ITERATIONS :
                                pChunk->input.numPatterns - 1;

    // Set initial target pulse
    double targetPulseUs = m_PulseUs;

    // If DelayUs isn't set, set it equal to the pulse
    double lwrDelayUs = m_DelayUs ? m_DelayUs : targetPulseUs;

    // If StepWidthUs is in use callwlate the required number of steps,
    // otherwise use m_StepWidth
    UINT64 targetNumSteps =
        m_StepWidthUs ?
        static_cast<UINT64>(round(fmax(1.0, m_StepWidthUs / (targetPulseUs + lwrDelayUs)))) :
        std::max(1llu, m_StepWidth);

    UINT64 innerStep = 0;
    bool linearShmooDir = false;
    for (UINT64 tick = 0; tick < pChunk->totalTicks;)
    {
        // Handle random shmoo
        if ((m_UseRandomShmoo || m_UseLinearShmoo) &&
            (innerStep == targetNumSteps))
        {
            // Reset step counter
            innerStep = 0;

            // Update targetPulseUs
            if (m_UseRandomShmoo)
            {
                // Uniformly random pulse is between m_MinPulseUs and m_PulseUs
                const double split = static_cast<double>(pChunk->random.GetRandom()) /
                    static_cast<double>(std::numeric_limits<UINT32>::max());
                const double offsetUs = split * (m_PulseUs - m_MinPulseUs);
                targetPulseUs = m_MinPulseUs + offsetUs;
            }
            else if (m_UseLinearShmoo)
            {
                // Reverse direction of stepping if we reached the bounds
                double pulseStepUs = (linearShmooDir ? 1 : -1) * m_StepUs;
                if (targetPulseUs + pulseStepUs < m_MinPulseUs ||
                    targetPulseUs + pulseStepUs > m_PulseUs)
                {
                    linearShmooDir = !linearShmooDir;
                    pulseStepUs = -pulseStepUs;
                }
                // Change target runtime by step size
                targetPulseUs += pulseStepUs;
            }
            else
            {
                MASSERT(!"UNREACHABLE");
            }

            // Update lwrDelayUs if DelayUs isn't explicitly set
            if (!m_DelayUs)
            {
                lwrDelayUs = targetPulseUs;
            }

            // If requested, randomly pulse step width between m_MinStepWidthUs and m_StepWidthUs
            // using the new targetPulseUs and lwrDelayUs
            if (m_UseRandomStepWidth)
            {
                const double split = static_cast<double>(pChunk->random.GetRandom()) /
                    static_cast<double>(std::numeric_limits<UINT32>::max());
                const double offsetUs = split * (m_StepWidthUs - m_MinStepWidthUs);
                targetNumSteps =
                    static_cast<UINT64>(round(fmax(
                    1.0,
                    (m_MinStepWidthUs + offsetUs) / (targetPulseUs + lwrDelayUs))));
            }
            else if (m_StepWidthUs)
            {
                targetNumSteps =
                    static_cast<UINT64>(round(fmax(
                    1.0,
                    m_StepWidthUs / (targetPulseUs + lwrDelayUs))));
            }
        }

        // Configure kernel state for resuming memory testing
        pChunk->input.numTicks =
            static_cast<UINT64>(round(fmax(1.0, (targetPulseUs / m_PulseUs) * pChunk->pulseTicks)));
        if (!m_ExternalDelay)
        {
            pChunk->input.delayNs = static_cast<UINT64>(lwrDelayUs * 1000.0);
        }
        pChunk->input.state.prevPass = (tick / pChunk->memLoops) +
            pChunk->input.outerIter * (pChunk->totalTicks / pChunk->memLoops);
        pChunk->input.state.loop =
            (tick) % pChunk->memLoops;
        pChunk->input.state.pattern =
            static_cast<UINT32>((tick / pChunk->memLoops) % patternLoops);
        pChunk->input.state.dirReverse =
            static_cast<UINT32>((tick / pChunk->memLoops / patternLoops) % pChunk->numDirections);
        pChunk->input.state.innerIteration =
            static_cast<UINT32>((tick / pChunk->memLoops / patternLoops / pChunk->numDirections));

        Printf(Tee::PriLow,
            "Outer: %4d Dir: %1d Ptrn: %2d Loop: %llu\tPrevPass: %llu\n",
            pChunk->input.state.innerIteration,
            pChunk->input.state.dirReverse,
            pChunk->input.state.pattern,
            pChunk->input.state.loop,
            pChunk->input.state.prevPass);
        Printf(Tee::PriLow,
            "Launch %llu: TargetPulseUs %6.2f DelayUs %6.2f ActualPulseTicks %llu\n",
            m_KernLaunchCount,
            targetPulseUs,
            lwrDelayUs,
            pChunk->input.numTicks);
        if (m_PrintKernelTime)
        {
            CHECK_RC(m_LaunchBegin.Record());
        }
        // Launch memory kernel
        CHECK_RC(m_LwdaMats.Launch(pChunk->input));
        if (m_PrintKernelTime)
        {
            CHECK_RC(m_LaunchEnd.Record());
            CHECK_RC(m_LaunchEnd.Synchronize());
            float measuredTimeUs = m_LaunchEnd.TimeMsElapsedSinceF(m_LaunchBegin) * 1000;
            Printf(Tee::PriNormal,
                   "Actual (w/o Delay) %llu:   %6.2f us\n",
                   m_KernLaunchCount, measuredTimeUs - lwrDelayUs);
        }
        tick += pChunk->input.numTicks;
        m_KernLaunchCount++;
        innerStep++;

        // MSCG and GCX add the delay between kernel launches
        if (m_pGCxBubble)
        {
            MASSERT(m_ExternalDelay);
            // If we are testing GCx we need to sync and then run the GCx bubble
            // with the current delay
            CHECK_RC(GetLwdaInstance().Synchronize());
            CHECK_RC(m_pGCxBubble->ActivateBubble(static_cast<UINT32>(lwrDelayUs / 1000)));
        }
        else if (m_TestMscg)
        {
            MASSERT(m_ExternalDelay);
            // If we are testing MSCG we need to sync and wait on the CPU side,
            // since running LWCA a kernel kicks the GPU out of MSCG
            CHECK_RC(GetLwdaInstance().Synchronize());
            const UINT64 startTimeUs = Xp::GetWallTimeUS();
            while (static_cast<double>(Xp::GetWallTimeUS() - startTimeUs) < lwrDelayUs);
        }
    }

    return rc;
}

// Process bit errors from results/overflow buffer and clear the processed spots
// @return index reached. If returning early, index at which buffer is empty; otherwise return end.
UINT32 NewLwdaMatsTest::ProcessBuffer
(
    NewLwdaMatsTest *pTest,
    BitError *pBuffer,
    const UINT32 start,
    const UINT32 end,
    const UINT32 stepSize,
    const bool returnEarly
)
{
    FbDecode decode = {};
    FrameBuffer* pFB = pTest->GetBoundGpuSubdevice()->GetFB();

    for (UINT32 i = start; i < end; i += stepSize)
    {
        BitError *pBitError = &pBuffer[i];
        BitError bitError =
        {
            Cpu::AtomicRead(&pBitError->address),
            Cpu::AtomicRead(&pBitError->badBits)
        };

        // If address == 0 or badBits == 0, either device has not filled the buffer to this point
        // or the error logs have been corrupted in some way. If there's a mismatch/corruption
        // it'll be detected in TrackBitErrors procedure
        if (!bitError.address || !bitError.badBits)
        {
            // Don't bother checking ahead and return early.
            // returnEarly is set true when we're intermediately processing a buffer that has
            // yet to be marked FULL by device
            if (returnEarly)
            {
                return i;
            }
            continue;
        }

        // Translate FB Virtual address to RBC address
        const GpuUtility::MemoryChunkDesc *pDesc;
        pDesc = &pTest->GetChunkDesc(pTest->FindChunkByVirtualAddress(bitError.address));
        const UINT64 physAddr = pTest->ContiguousVirtualToPhysical(bitError.address);
        MASSERT(physAddr != ~0ULL);
        // The way certain FB (not for GA102) classes are implemented, DecodeAddress is not thread-safe
        // Something to note if this produces garbage error logs
        pFB->DecodeAddress
        (
            &decode,
            physAddr,
            pDesc->pteKind,
            pDesc->pageSizeKB
        );

        // Increment counter for each mismatched bits in a word
        UINT32 bitCounterIdx = 0;
        for (INT32 badBit = Utility::BitScanForward(bitError.badBits);
             badBit >= 0;
             badBit = Utility::BitScanForward(bitError.badBits, badBit + 1))
        {
            // Translate decode composition into 1-d index
            bitCounterIdx = pTest->m_DQIdxTranslator.Encode
                            (
                                decode.virtualFbio,
                                decode.subpartition,
                                decode.pseudoChannel,
                                decode.rank,
                                decode.beatOffset,
                                badBit
                            );
            Cpu::AtomicAdd(&pTest->m_ErrorBitCounter[bitCounterIdx], 1);
        }
        // There could be one or more badBits per word mismatch, and we compare word (32 bits)
        // so this helps keep track of how many 32bit pattern mismatches were encountered in the kernel
        Cpu::AtomicAdd(&pTest->m_ErrorWordCounter[bitCounterIdx / 32], 1);

        // Resetting the values ensure we're not re-incrementing the counters
        Cpu::AtomicWrite(&pBitError->address, 0ULL);
        Cpu::AtomicWrite(&pBitError->badBits, 0);
    }

    return end;
}

// Entry function for consumers to log overflow errors
void NewLwdaMatsTest::ConsumeErrors(void *arg)
{
    Tasker::DetachThread detach;

    UINT32 tid = Tasker::GetThreadIdx();
    NewLwdaMatsTest *pTest = reinterpret_cast<NewLwdaMatsTest*>(arg);
    const UINT32 threadGroupSize = Tasker::GetGroupSize();

    RangeBitErrors *pResults = reinterpret_cast<RangeBitErrors*>(pTest->m_HostResults.GetPtr());
    BitError *const pBuffer = reinterpret_cast<BitError*>(&pResults->overflowBuffer);
    UINT32 *const pBufIdx = reinterpret_cast<UINT32*>(pTest->m_OverflowBufIdx.GetPtr());
    BufferStatus *const pBufStatus = reinterpret_cast<BufferStatus*>(pTest->m_OverflowBufStatus.GetPtr());

    // e.g. blockLwrrIdxes[0] will give us the index at which to resume consume errors from the block buffer
    vector<UINT32> blockLwrrIdxes(CEIL_DIV(pTest->m_NumBlocks, threadGroupSize), 0);

    while (!Cpu::AtomicRead(&pTest->m_ShouldJoin))
    {
        // If numThreads < numBlocks, each thread is responsible for processing multiple blocks
        // blockId refers to which LWCA block we're working with
        for (UINT32 blockId = tid; blockId < pTest->m_NumBlocks; blockId += threadGroupSize)
        {
            BitError *pBlockBuffer = &pBuffer[blockId * pTest->m_MaxOvfBlockErrors];
            const UINT32 blockLwrrIdx = blockLwrrIdxes[blockId / threadGroupSize];
            // The buffer partition for this block is full. Consume all of it so the LWCA threads can resume work
            if (Cpu::AtomicRead(&pBufStatus[blockId]) == BUFFER_FULL)
            {
                // We could input blockLwrrIdx as starting index instead of 0
                // but there seems to be synchronicity issues (~2 errors missing per million on average)
                // perf difference is negligible so leaving it this way for now
                ProcessBuffer(pTest, pBlockBuffer, 0, pTest->m_MaxOvfBlockErrors, 1, false);
                Cpu::AtomicWrite(&pBufStatus[blockId], BUFFER_FLUSHED);
                blockLwrrIdxes[blockId / threadGroupSize] = 0;
            }
            // If buffer partition is not full, but has errors, consume some of it to reduce
            // the amount of errors to consume in one go later
            else
            {
                // Process errors while GPU is not actively waiting
                // Higher number ~ Coarse granuality | Lower number ~ Finer granularity
                const UINT32 processUnitSize = 50;
                const UINT32 endIdx = min(blockLwrrIdx + processUnitSize, pTest->m_MaxOvfBlockErrors);
                blockLwrrIdxes[blockId / threadGroupSize] =
                    ProcessBuffer(pTest, pBlockBuffer, blockLwrrIdx, endIdx, 1, true);
            }
        }
        Tasker::Yield();
    }
    // Flush overflow buffer
    for (UINT32 blockId = tid; blockId < pTest->m_NumBlocks; blockId += threadGroupSize)
    {
        BitError *const pBlockBuffer = &pBuffer[blockId * pTest->m_MaxOvfBlockErrors];
        const UINT32 blockBufIdx = pBufIdx[blockId];
        ProcessBuffer(pTest, pBlockBuffer, 0, blockBufIdx, 1, false);
        // If #block errors == max block errors, device does not reset index to 0, so we reset in host
        Cpu::AtomicWrite(&pBufIdx[blockId], 0);
        Cpu::AtomicWrite(&pBufStatus[blockId], BUFFER_AVAILABLE);
    }
}

RC NewLwdaMatsTest::ForkConsumers(void)
{
    RC rc;

    // Auxiliary variables for keeping track of max number of errors produced in a chunk
    RangeBitErrors *pResults = reinterpret_cast<RangeBitErrors*>(m_HostResults.GetPtr());
    m_PrevNumErrors = pResults->numErrors;

    // Fork threads to consume error logs produced by device
    m_ShouldJoin = false;
    m_ConsumerTIDs = Tasker::CreateThreadGroup(ConsumeErrors, this,
        m_NumWorkerThreads, "ConsumeErrors", true, nullptr);

    if (m_ConsumerTIDs.empty())
    {
        Printf(Tee::PriError, "Could not fork %u ConsumeError threads.\n", m_NumWorkerThreads);
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC NewLwdaMatsTest::JoinConsumers(void)
{
    RC rc;

    // Signal to consumer threads that the chunk has done running
    m_ShouldJoin = true;

    CHECK_RC(Tasker::Join(m_ConsumerTIDs));

    RangeBitErrors *pResults = reinterpret_cast<RangeBitErrors*>(m_HostResults.GetPtr());
    m_MaxErrorsNotFlushed =
        max(m_MaxErrorsNotFlushed, pResults->numErrors - m_PrevNumErrors);

    return RC::OK;
}

RC NewLwdaMatsTest::FlushBuffer()
{
    RC rc;

    // Sync so that produced error logs are visible
    GetLwdaInstance().Synchronize();

    Perf *pPerf = GetBoundGpuSubdevice()->GetPerf();
    UINT64 flushClockStart = 0;
    UINT64 flushClockEnd   = 0;

    if (m_RecordL2Clk)
    {
        CHECK_RC(pPerf->MeasureAvgClkCount(m_L2ClkDomain, &flushClockStart));
    }
    const UINT64 flushStart = Xp::GetWallTimeUS();
    CHECK_RC(JoinConsumers());
    if (m_RecordL2Clk)
    {
        CHECK_RC(pPerf->MeasureAvgClkCount(m_L2ClkDomain, &flushClockEnd));
    }
    m_TimeSpentFlushingUs += Xp::GetWallTimeUS() - flushStart;
    m_L2ClksSpentFlushing += flushClockEnd - flushClockStart;

    CHECK_RC(ForkConsumers());

    return RC::OK;
}

RC NewLwdaMatsTest::RunChunkOuterIteration
(
    vector<ChunkData> *pTestableChunks,
    UINT32 outer,
    bool *pStop // As an input indicates if this is the last iteration to be
                // exelwted. As an output transition from "false" to "true"
                // requests early exit due to an error.
)
{
    RC rc = RC::OK;

    const bool quitOnError = GetGoldelwalues()->GetStopOnError();

    // Execute test
    for (auto& chunk : *pTestableChunks)
    {
        const UINT64 targetSize = chunk.endOffset - chunk.startOffset;

        // Launch the kernel
        chunk.input.outerIter = outer;

        // Trigger memory fault. This is a nop unless -emu_mem_trigger arg is used
        GetBoundGpuSubdevice()->EmuMemTrigger();

        if (m_PulseUs)
        {
            CHECK_RC(LwdaMatsLaunchWithPulse(&chunk));
        }
        else
        {
            // Previous RNG state
            chunk.input.state.prevPass = outer * (chunk.totalTicks / chunk.memLoops);
            if (m_PrintKernelTime)
            {
                CHECK_RC(m_LaunchBegin.Record());
            }
            CHECK_RC(m_LwdaMats.Launch(chunk.input));
            if (m_PrintKernelTime)
            {
                CHECK_RC(m_LaunchEnd.Record());
                CHECK_RC(m_LaunchEnd.Synchronize());
                Printf(Tee::PriNormal,
                    "Launch %llu: %f ms\n",
                    m_KernLaunchCount, m_LaunchEnd.TimeMsElapsedSinceF(m_LaunchBegin));
            }
            m_KernLaunchCount++;
        }

        if (m_ReportBitErrors && m_FlushBehavior == FLUSH_CHUNK)
        {
            CHECK_RC(FlushBuffer());
        }

        m_TestedSomething = true;

        // Record how many MB we have read/written
        const UINT64 numBytes = chunk.totalTicks * m_BytesPerTick;
        m_BytesRead += numBytes;
        if (!m_ReadOnly)
        {
            m_BytesWritten += numBytes;
        }

        // Quick exit if errors were found
        if (outer > 0)
        {
            CHECK_RC(chunk.snoopEvent.Synchronize());
            const UINT64 numErrors = (m_ReportBitErrors)
                ? (reinterpret_cast<const RangeBitErrors*>(m_HostResults.GetPtr())->numErrors)
                : (reinterpret_cast<const RangeErrors*>(m_HostResults.GetPtr())->numErrors);
            if (numErrors > 0)
            {
                rc = RC::BAD_MEMORY;
                if (quitOnError)
                {
                    Printf(Tee::PriError, "NewLwdaMats: Failure detected after "
                        "%u iterations. Terminating after %u iterations.\n",
                        (outer)*m_ActualNumInnerIterations,
                        (outer + 1)*m_ActualNumInnerIterations);
                    GetJsonExit()->AddFailLoop(outer);
                    *pStop = true;
                    break;
                }
            }
        }

        CHECK_RC(GpuTest::EndLoop(outer, LwrrentProgressIteration(), rc));

        if (!*pStop)
        {
            CHECK_RC(chunk.snoopEvent.Record());

            // If ReadOnly, set the data between kernel runs (throughput should
            // decrease a bit)
            // Make sure not to run this before exiting
            if (m_ReadOnly)
            {
                LwdaMatsInput fillInput = chunk.initInput;
                fillInput.outerIter = outer + 1;

                CHECK_RC(m_FillLwdaMats.Launch(fillInput));
                m_BytesWritten += targetSize;
            }
        }

        m_LwrTestStep++;
    }

    return rc;
}

RC NewLwdaMatsTest::CheckMscgCounters
(
    UINT32 entryCountStart,
    UINT32 inGateStartUs,
    UINT32 unGateStartUs
)
{
    RC rc;
    PMU* pPmu;
    UINT32 entryCountEnd = 0;
    UINT32 inGateEndUs = 0;
    UINT32 unGateEndUs = 0;

    CHECK_RC(GetBoundGpuSubdevice()->GetPmu(&pPmu));
    CHECK_RC(pPmu->GetMscgEntryCount(&entryCountEnd));
    CHECK_RC(pPmu->GetLwrrentPgTimeUs(PMU::ELPG_MS_ENGINE, &inGateEndUs, &unGateEndUs));

    const UINT32 entryCount = entryCountEnd - entryCountStart;
    const UINT32 inGateTimeUs = inGateEndUs - inGateStartUs;
    const UINT32 unGateTimeUs = unGateEndUs - unGateStartUs;
    Printf(Tee::PriNormal,
        "MSCG Entry Count:         %d (%llu expected)\n"
        "MSCG InGate Time / Entry: %d us\n"
        "MSCG InGate Time:         %d us\n"
        "MSCG UnGate Time:         %d us\n",
        entryCount, m_KernLaunchCount,
        (entryCount) ? inGateTimeUs / entryCount : 0,
        inGateTimeUs,
        unGateTimeUs);

    const double entryRatio = static_cast<double>(entryCount) / m_KernLaunchCount;
    if (entryRatio < m_MscgEntryThreshold)
    {
        Printf(Tee::PriError,
            "%f < %f MSCG Entry Count tolerance not met. "
            "(Measured %d Expected %llu\n)",
            entryRatio, m_MscgEntryThreshold,
            entryCount, m_KernLaunchCount);
        return RC::EXCEEDED_EXPECTED_THRESHOLD;
    }

    return rc;
}

UINT64 NewLwdaMatsTest::LwrrentProgressIteration() const
{
    if (m_RuntimeMs == 0)
    {
        return m_LwrTestStep;
    }

    const UINT64 lwrrentProgressIteration = Xp::GetWallTimeMS() - m_RunStartMs;
    if (lwrrentProgressIteration > m_RuntimeMs)
    {
        return m_RuntimeMs;
    }

    return lwrrentProgressIteration;
}

void NewLwdaMatsTest::LogNewLwdaMemError
(
    UINT64 fbOffset,
    UINT32 actual,
    UINT32 expected,
    UINT64 encFailData,
    const GpuUtility::MemoryChunkDesc& chunk
)
{
    UINT32 blockIdx;
    UINT32 threadIdx;
    UINT32 outerIteration;
    UINT32 innerIteration;
    UINT32 patternIdx;
    bool   forward;

    BadValue::DecodeFailData(encFailData,
        &blockIdx,
        &threadIdx,
        &outerIteration,
        &innerIteration,
        &patternIdx,
        &forward);

    NewLwdaReportedError error;
    error.Address = fbOffset;
    error.Actual = actual;
    error.Expected = expected;
    error.Reread1 = 0;
    error.Reread2 = 0;
    error.PteKind = chunk.pteKind;
    error.PageSizeKB = chunk.pageSizeKB;
    error.BlockIdx = blockIdx;
    error.ThreadIdx = threadIdx;
    error.OuterIteration = outerIteration;
    error.InnerIteration = innerIteration;
    error.PatternIdx = patternIdx;
    error.Direction = forward;

    // When reporting which iteration failure oclwred in, we'll only report
    // overall iteration (combination of inner/outer) because due to block
    // parallelization, it's possible that errors oclwring simultaneously during
    // different patterns or memory traversal direction
    const UINT32 iteration = outerIteration * m_ActualNumInnerIterations + innerIteration;
    m_NewLwdaReportedErrors[error].insert(iteration);

    if (iteration > 0xFFFFFFFF)
        GetJsonExit()->AddFailLoop(0xFFFFFFFF);
    else
        GetJsonExit()->AddFailLoop(iteration);
}

RC NewLwdaMatsTest::TrackBitErrors
(
    bool* pCorrupted,
    UINT64 testedMemSize
)
{
    RC rc;

    UINT32 virtualFbio;
    UINT32 subpartition;
    UINT32 pseudoChannel;
    UINT32 rank;
    UINT32 beatOffset;
    INT32 bitOffset;

    UINT64 numErrorsConsumed = 0;
    bool countWord = false;

    for (UINT32 idx = 0; idx < m_ErrorBitCounter.size(); idx++)
    {
        if (idx % 32 == 0)
        {
            countWord = true;
        }
        if (m_ErrorBitCounter[idx])
        {
            // Add word error count every 32 (word size) bit errors
            const UINT32 wordErrorCnt = countWord ? m_ErrorWordCounter[idx / 32] : 0;
            m_DQIdxTranslator.Decode
            (
                idx,
                &virtualFbio,
                &subpartition,
                &pseudoChannel,
                &rank,
                &beatOffset,
                &bitOffset
            );
            CHECK_RC
            (
                GetMemError(0).LogFailingBits
                (
                    virtualFbio,
                    subpartition,
                    pseudoChannel,
                    rank,
                    0,                         // Beat mask (0 for all)
                    beatOffset,
                    bitOffset,
                    wordErrorCnt,
                    m_ErrorBitCounter[idx],    // Bit error count
                    MemError::IoType::UNKNOWN
                )
            );
            numErrorsConsumed += wordErrorCnt;
            countWord = false;
        }
    }

    const RangeBitErrors* const pResults =
        reinterpret_cast<const RangeBitErrors*>(m_HostResults.GetPtr());

    // Number of errors does not match number of errors consumed above
    // This probably means something went wrong in the consumer threads while processing/flushing buffers.
    if (pResults->numErrors != numErrorsConsumed)
    {
        *pCorrupted = true;
        Printf
        (
            Tee::PriWarn,
            "Error results are corrupted %llu errors vs %llu consumed\n",
            pResults->numErrors,
            numErrorsConsumed
        );
    }

    const UINT64 allowedNumErrors = testedMemSize * m_Iterations / 4;
    // Validate total number of errors
    if (numErrorsConsumed > allowedNumErrors)
    {
        *pCorrupted = true;
        Printf
        (
            Tee::PriWarn,
            "Error results are corrupted %llu total errors for all threads?!\n",
            numErrorsConsumed
        );
    }

    // To reduce amount of time GPU spends waiting for CPU to consume buffers, we can allocate
    // enough memory to hold the max number of errors produced in a single chunk, since the buffers
    // get flushed in between chunks
    const UINT64 errorCapacity = static_cast<UINT64>(m_MaxOvfBlockErrors) * m_NumBlocks;
    if (m_MaxErrorsNotFlushed > errorCapacity)
    {
        // Be conservative (25% more) in recommending how many bytes to allocate
        // Number of errors per block is most likely uneven
        Printf
        (
            Tee::PriWarn,
            "Test stressfulness may have been decreased as a result of lower overflow buffer memory.\n"
            "         Max errors produced before flushing: %llu Error capacity: %llu\n"
            "         Try allocating at least %llu bytes. This is especially recommended when running in pulse mode.\n",
            m_MaxErrorsNotFlushed, errorCapacity,
            m_MaxErrorsNotFlushed * sizeof(BitError) * 5 / 4
        );
    }

    return rc;
}

RC NewLwdaMatsTest::TrackErrors
(
    const char* results,
    bool* pCorrupted, // Set true if mem errors oclwred, else retain old value
    UINT64 testedMemSize
)
{
    unsigned numCorruptions = 0;
    const unsigned maxCorruptions = 10;
    UINT64 totalNumErrors = 0;
    RC rc;

    const RangeErrors* const errors =
        reinterpret_cast<const RangeErrors*>(results);

    // Validate errors
    if ((errors->numReportedErrors > m_MaxErrorsPerBlock * m_NumBlocks)
         || (errors->numReportedErrors > errors->numErrors))
    {
        numCorruptions++;
        if (numCorruptions <= maxCorruptions)
        {
            Printf(Tee::PriNormal,
                    "Error results are corrupted (%u reported errors?!)\n",
                    errors->numReportedErrors);
        }
        *pCorrupted = true;
        return OK;
    }
    if (errors->numReportedErrors >
        std::min<UINT64>(m_MaxErrorsPerBlock * m_NumBlocks, errors->numErrors))
    {
        numCorruptions++;
        if (numCorruptions <= maxCorruptions)
        {
            Printf(Tee::PriNormal,
                    "Error results are corrupted (%u reported errors with %u max, but %llu total errors?!)\n",
                    errors->numReportedErrors, m_MaxErrorsPerBlock, errors->numErrors);
        }
        *pCorrupted = true;
        return OK;
    }
    bool badValueCorrupted = false;
    for (UINT32 ierr=0; ierr < errors->numReportedErrors; ierr++)
    {
        const BadValue& badValue = errors->reportedValues[ierr];
        if (badValue.actual == badValue.expected)
        {
            numCorruptions++;
            if (numCorruptions <= maxCorruptions)
            {
                Printf(Tee::PriNormal,
                        "Error results are corrupted (actual value 0x%08x the same as expected?!)\n",
                        badValue.actual);
            }
            badValueCorrupted = true;
            break;
        }
    }
    if (badValueCorrupted)
    {
        *pCorrupted = true;
        return OK;
    }

    // Report unlogged errors
    totalNumErrors += errors->numErrors;
    const UINT64 numUnloggedErrors
        = errors->numErrors - errors->numReportedErrors;
    if (numUnloggedErrors > 0)
    {
        CHECK_RC(GetMemError(0).LogMysteryError(numUnloggedErrors));
    }

    // Report logged errors
    for (UINT32 ierr=0; ierr < errors->numReportedErrors; ierr++)
    {
        const BadValue& badValue = errors->reportedValues[ierr];

        // Colwert error's virtual address to physical address
        const UINT64 fbOffset = VirtualToPhysical(badValue.address);
        if (fbOffset == ~static_cast<UINT64>(0U))
        {
            numCorruptions++;
            if (numCorruptions < maxCorruptions)
            {
                Printf(Tee::PriNormal, "Couldn't find a memory allocation "
                    "matching error detected at virtual address 0x%llx\n",
                    badValue.address);
            }
            continue;
        }

        LogNewLwdaMemError(fbOffset, badValue.actual, badValue.expected, badValue.encFailData,
                GetChunkDesc(FindChunkByVirtualAddress(badValue.address)));
    }

    bool TooManyErrors = false;
    const UINT64 allowedNumErrors = testedMemSize * m_Iterations / 4;
    TooManyErrors = totalNumErrors > allowedNumErrors;

    // Validate total number of errors
    if (TooManyErrors)
    {
        numCorruptions++;
        Printf(Tee::PriNormal,
                "Error results are corrupted (%llu total errors for all threads?!)\n",
                totalNumErrors);
    }
    else
    {
        if (totalNumErrors < 0xFFFFFFFFU)
        {
            m_NumErrors += static_cast<UINT32>(totalNumErrors);
        }
        else
        {
            m_NumErrors = 0xFFFFFFFFU;
        }
    }

    if (numCorruptions > 0)
    {
        *pCorrupted = true;
    }
    return OK;
}

/* virtual */ RC NewLwdaMatsTest::ReportErrors()
{
    RC rc;

    // Log all bad memory locations first
    for (const auto &err: m_NewLwdaReportedErrors)
    {
        CHECK_RC(NewLwdaLogMemoryError(
                        GetMemError(0),
                        err.first.Address,
                        err.first.Actual,
                        err.first.Expected,
                        err.first.PteKind,
                        err.first.PageSizeKB,
                        *err.second.begin(),
                        err.first.BlockIdx,
                        err.first.ThreadIdx,
                        err.first.OuterIteration,
                        err.first.InnerIteration,
                        err.first.Direction,
                        err.first.PatternIdx));
    }

    // Log repeated memory failures
    for (const auto &err: m_NewLwdaReportedErrors)
    {
        bool isFirstTimeStamp = true;
        for (const auto timeStamp: err.second)
        {
            if (!isFirstTimeStamp)
            {
                CHECK_RC(NewLwdaLogMemoryError(
                                GetMemError(0),
                                err.first.Address,
                                err.first.Actual,
                                err.first.Expected,
                                err.first.PteKind,
                                err.first.PageSizeKB,
                                timeStamp,
                                err.first.BlockIdx,
                                err.first.ThreadIdx,
                                err.first.OuterIteration,
                                err.first.InnerIteration,
                                err.first.Direction,
                                err.first.PatternIdx));
            }
            isFirstTimeStamp = false;
        }
    }

    m_NewLwdaReportedErrors.clear();

    return rc;
}

JS_CLASS_INHERIT(NewLwdaMatsTest, LwdaMemTest,
                 "LWCA MATS (Modified Algorithmic Test Sequence)");
CLASS_PROP_READWRITE(NewLwdaMatsTest, RuntimeMs, UINT32,
                     "How long to run the active portion of the test, in milliseconds. Overrides \"Iterations\".");
CLASS_PROP_READWRITE(NewLwdaMatsTest, Iterations, UINT32,
                     "Number of times to repeat test");
CLASS_PROP_READWRITE(NewLwdaMatsTest, MaxInnerIterations, UINT32,
                     "Maximum number of iterations with which to ilwoke the kernel");
CLASS_PROP_READWRITE(NewLwdaMatsTest, MaxErrorsPerBlock, UINT32,
                     "Maximum number of errors reported per each LWCA block (1.2) or thread (1.0) (0=default)");
CLASS_PROP_READWRITE(NewLwdaMatsTest, SkipErrorReporting, bool,
                     "Skip reporting of memory errors found by the LWCA kernel");
CLASS_PROP_READWRITE(NewLwdaMatsTest, StartPhys, UINT64,
                     "Starting physical address");
CLASS_PROP_READWRITE(NewLwdaMatsTest, EndPhys, UINT64,
                     "Ending physical address");
CLASS_PROP_READWRITE(NewLwdaMatsTest, MinAllocSize, UINT64,
                     "Minimum number of bytes expected to be available to the test");
CLASS_PROP_READWRITE(NewLwdaMatsTest, NumBlocks, UINT32,
                     "Number of kernel blocks to run");
CLASS_PROP_READWRITE(NewLwdaMatsTest, NumThreads, UINT32,
                     "Number of kernel threads to run");
CLASS_PROP_READWRITE(NewLwdaMatsTest, RunExperiments, bool,
                     "Runs perfmon experiments along with the test.");
CLASS_PROP_READWRITE(NewLwdaMatsTest, PatternName, string,
                     "Name of pattern to be used in NewLwdaMats when explicit Patterns are not provided.");
CLASS_PROP_READWRITE(NewLwdaMatsTest, UseRandomData, bool,
                     "Use random data in place of Patterns. Each LWCA thread uses different random values.");
CLASS_PROP_READWRITE(NewLwdaMatsTest, LevelMask, UINT32,
                     "Test only the PAM4 levels specified by this bitmask, randomizing the levels that are sent across the memory bus");
CLASS_PROP_READWRITE(NewLwdaMatsTest, UseSafeLevels, bool,
                     "Only allow safe level transitions when using LevelMask. This forbids the first level from being Level -3");
CLASS_PROP_READWRITE(NewLwdaMatsTest, BusTest, bool,
                     "Burst patterns sequentially across the memory bus, instead of using the same pattern");
CLASS_PROP_READWRITE(NewLwdaMatsTest, BusPatBytes, UINT32,
                     "Size of bus pattern in bytes in BusTest mode");
CLASS_PROP_READWRITE(NewLwdaMatsTest, RandomAccessBytes, UINT32,
                     "Use random framebuffer offsets with accesses the specified number of bytes wide");
CLASS_PROP_READWRITE(NewLwdaMatsTest, ReadOnly, bool,
                     "Read only memory test (we write new values between kernel launches)");
CLASS_PROP_READWRITE(NewLwdaMatsTest, PrintKernelTime, bool,
                     "Print the time each kernel takes to run");
CLASS_PROP_READWRITE(NewLwdaMatsTest, PulseUs, double,
                     "Time spent by memory access kernel between delays. "
                     "Used to automatically callwlate 'PulseTicks', "
                     "the number of memory accesses per thread between delays.");
CLASS_PROP_READWRITE(NewLwdaMatsTest, DelayUs, double,
                     "The time to wait in us between memory pulses if 'PulseUs' is set");
CLASS_PROP_READWRITE(NewLwdaMatsTest, UseRandomShmoo, bool,
                     "Pulse the memory in random durations between PulseUs and MinPulseUs");
CLASS_PROP_READWRITE(NewLwdaMatsTest, UseLinearShmoo, bool,
                     "Linearly shmoo the memory pulse duration between PulseUs and MinPulseUs");
CLASS_PROP_READWRITE(NewLwdaMatsTest, UseRandomStepWidth, bool,
                     "When using Random or Linear shmoo, randomize the time spent at each step between StepWidthUs and MinStepWidthUs");
CLASS_PROP_READWRITE(NewLwdaMatsTest, StepUs, double,
                     "When using Linear shmoo step by this amount");
CLASS_PROP_READWRITE(NewLwdaMatsTest, StepWidth, UINT64,
                     "When using Random or Linear shmoo, stay at the current duration for at most this many kernel launches");
CLASS_PROP_READWRITE(NewLwdaMatsTest, StepWidthUs, double,
                     "When using Random or Linear shmoo, stay at the current duration for at most this period of time in microseconds");
CLASS_PROP_READWRITE(NewLwdaMatsTest, MinStepWidthUs, double,
                     "When StepWidth is variable, run for at least this long in microseconds");
CLASS_PROP_READWRITE(NewLwdaMatsTest, MinPulseUs, double,
                     "When using Random or Linear shmoo, never pulse below this duration");
CLASS_PROP_READWRITE(NewLwdaMatsTest, TestMscg, bool,
                     "Test whether MSCG is being entered properly during memory delays "
                     "(use with PulseUs and DelayUs)");
CLASS_PROP_READWRITE(NewLwdaMatsTest, MscgEntryThreshold, double,
                     "Threshold ratio at which to fail if MSCG is entered fewer times than expected");
CLASS_PROP_READWRITE(NewLwdaMatsTest, DoGCxCycles, UINT32,
                     "Perform GCx cycles (0: disable, 1: GC5, 2: GC6, 3: RTD3).");
CLASS_PROP_READWRITE(NewLwdaMatsTest, ForceGCxPstate, UINT32,
                     "Force to pstate prior to GCx entry.");
CLASS_PROP_READWRITE(NewLwdaMatsTest, DisplayAnyChunk, bool,
                     "Output any chunk to display if display is enabled");
CLASS_PROP_READWRITE(NewLwdaMatsTest, DumpInitData, bool,
                     "Print out the initial memory pattern for debugging purposes");
CLASS_PROP_READWRITE(NewLwdaMatsTest, ReportBitErrors, bool,
                     "Report all bit errors despite device shared memory limitations");
CLASS_PROP_READWRITE(NewLwdaMatsTest, OverflowBufferBytes, UINT32,
                     "When using ReportBitErrors, number of bytes to allocate to overflow buffer");
CLASS_PROP_READWRITE(NewLwdaMatsTest, FlushBehavior, UINT32,
                     "When using ReportBitErrors, nested depth at which to flush buffer. "
                     "0: After each chunk; 1: After each outer loop; 2: Never");
CLASS_PROP_READONLY(NewLwdaMatsTest, NumErrors, UINT32,
                    "Number of errors found, read only, valid after Run()");
