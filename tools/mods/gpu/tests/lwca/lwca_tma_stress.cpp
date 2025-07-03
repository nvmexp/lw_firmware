/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/utility/ptrnclss.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwdamemtest.h"
#include "gpu/tests/lwca/tma/tmastress.h"

//------------------------------------------------------------------------------
//! \brief LWCA memory test utilizing the Tensor Memory Access (TMA) unit
//!
class LwdaTMAStress final : public LwdaMemTest
{
public:
    LwdaTMAStress();
    virtual ~LwdaTMAStress() = default;

    RC InitFromJs() override;
    void PrintJsProperties(Tee::Priority) override;
    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

    // JS property accessors
    SETGET_PROP(RuntimeMs,          UINT32);
    SETGET_PROP(Iterations,         UINT32);
    SETGET_PROP(MaxInnerIterations, UINT32);
    SETGET_PROP(StartPhys,          UINT64);
    SETGET_PROP(EndPhys,            UINT64);
    SETGET_PROP(UseRandomData,      bool);
    SETGET_PROP(SkipErrorReporting, bool);
    SETGET_PROP(PatternSubsetSize,  UINT32);
    SETGET_PROP(PatternName,        string);
    SETGET_PROP(PulseUs,            double);
    SETGET_PROP(DelayUs,            double);
    SETGET_PROP(UseTMA,             bool);
    SETGET_PROP(PrefetchMode,       bool);
	SETGET_JSARRAY_PROP_LWSTOM(BoxDims);
	SETGET_JSARRAY_PROP_LWSTOM(Patterns);

private:
    struct ChunkData
    {
        ChunkData
        (
            UINT64 ptr,
            UINT64 addr,
            UINT64 start,
            UINT64 end,
            UINT64 len,
            GpuUtility::MemoryChunkDesc* d
        )
            : memDevPtr(ptr)
            , nominalAddr(addr)
            , startOffset(start)
            , endOffset(end)
            , size(len)
            , desc(d)
        {}

        RC Initialize(UINT32 seed, const Lwca::Instance *pLwdaInstance);

        UINT64 memDevPtr;
        UINT64 nominalAddr;
        UINT64 startOffset;
        UINT64 endOffset;
        UINT64 size;
        GpuUtility::MemoryChunkDesc* desc;
        TMAStressInput tmaStressInput = {};
        CheckFillInput checkFillInput = {};
        Random random;
        Dimensions tensorDims = {};
        lwdaTmaDescv2 tmaDesc = {};
    };

    void CallwlateKernelGeometry();
    const char * GetFillFuncName();
    const char * GetCheckFuncName();
    RC SetupErrorReporting();
    void SetDefaultChunkSize();
    RC DetermineTestableChunks(vector<ChunkData>*);
    bool GetTestRange(UINT32, UINT64*, UINT64*);
    RC SetupTensorDims(ChunkData *);
    void InitBaseInput(ChunkData *);
    void InitTMATensorDescriptor(ChunkData *);
    RC SetTensorDescriptor(lwdaTmaDescv2 *pDesc);
    RC RunChunkOuterIteration(vector<ChunkData>*, UINT32, bool);
    bool ErrorsFound();
    RC TrackErrors();
    RC CheckResultsCorrupted();
    RC GetPatternByName();

    RC LaunchBatch(ChunkData *, UINT64, UINT64);
    RC LaunchBatchesWithPulse(ChunkData *);

    // JS properties
    UINT32 m_RuntimeMs = 0;
    UINT32 m_Iterations = 0;
    UINT32 m_MaxInnerIterations = 0;
    UINT64 m_StartPhys = 0;
    UINT64 m_EndPhys = 0;
    bool   m_UseRandomData = false;
    bool   m_SkipErrorReporting = false;

    UINT32 m_NumOuterIterations = 0;
    UINT32 m_NumInnerIterations = 0;
    UINT32 m_NumBlocks = 0;
    UINT32 m_NumFillThreads = 0;
    UINT32 m_NumCheckThreads = 0;
    UINT32 m_NumStressThreads = 0;
    UINT32 m_MaxGlobalErrors = 0;
    UINT32 m_MaxLocalErrors = 0;
    UINT64 m_BytesTransferred = 0;
    double m_TotalExecTimeMs = 0.0;
    UINT64 m_TestStepsCompleted = 0;
    UINT64 m_MaxTestSteps = 0;

    bool m_UseTMA = false;
    bool m_PrefetchMode = false;

    // Patterns
    vector<UINT32> m_Patterns;
    UINT32 m_PatternSubsetSize = 4;
    string m_PatternName = "";

    UINT32 m_NumDims = 1;
    vector<UINT32> m_BoxDims;

    UINT32 m_NumBoxElems = 0;
    UINT32 m_BoxMemSize = 0;
    UINT32 m_NumBoxesInSMem = 0;

	// Pulsing
    double m_PulseUs = 0.0;
    double m_DelayUs = 0.0;

    Lwca::Module       m_Module;
    Lwca::HostMemory   m_HostResults;
    Lwca::Function     m_FillFunc;
    Lwca::Function     m_TMAStressFunc;
    Lwca::Function     m_CheckFunc;
    Lwca::Event m_LaunchBegin;
    Lwca::Event m_LaunchEnd;
    Lwca::Event m_BatchBegin;
    Lwca::Event m_BatchEnd;
};

LwdaTMAStress::LwdaTMAStress()
{
    SetName("LwdaTMAStress");
}

RC LwdaTMAStress::SetPatterns(JsArray *val)
{
    JavaScriptPtr pJs;
    return pJs->FromJsArray(*val, &m_Patterns);
}

RC LwdaTMAStress::GetPatterns(JsArray *val) const
{
    JavaScriptPtr pJs;
    return pJs->ToJsArray(m_Patterns, val);
}

RC LwdaTMAStress::SetBoxDims(JsArray *val)
{
    JavaScriptPtr pJs;
    return pJs->FromJsArray(*val, &m_BoxDims);
}

RC LwdaTMAStress::GetBoxDims(JsArray *val) const
{
    JavaScriptPtr pJs;
    return pJs->ToJsArray(m_BoxDims, val);
}

//------------------------------------------------------------------------------
//! Initialize and verify JS properties here
//! Private member variables can be initialized in Setup
//!
RC LwdaTMAStress::InitFromJs()
{
    RC rc;

    CHECK_RC(LwdaMemTest::InitFromJs());

    // Configure outer and inner iterations
    m_NumInnerIterations =
        m_RuntimeMs ? m_MaxInnerIterations : min(m_MaxInnerIterations, m_Iterations);
    MASSERT(m_NumInnerIterations);
    m_NumOuterIterations = CEIL_DIV(m_Iterations, m_NumInnerIterations);

    // Adjust "Iterations" parameter to be divisible by number of inner iterations
    const UINT32 prevIterations = m_Iterations;
    m_Iterations = m_NumOuterIterations * m_NumInnerIterations;

    Printf
    (
        GetVerbosePrintPri(),
        "Requested iterations over memory: %u. Adjusted iterations over memory: %u\n",
        prevIterations,
        m_NumOuterIterations * m_NumInnerIterations
    );

    if ((m_Patterns.size() || !m_PatternName.empty()) && m_UseRandomData)
    {
        Printf(Tee::PriError, "Patterns and/or PatternName cannot be used with UseRandomData\n");
        return RC::BAD_PARAMETER;
    }
    if (m_PatternName.empty())
    {
        m_PatternName = "ShortMats";
    }
    if (m_Patterns.empty() && !m_UseRandomData)
    {
        CHECK_RC(GetPatternByName());
    }
    if ((m_Patterns.size() % 4) != 0)
    {
        Printf(Tee::PriError, "Number of patterns %lu must be a multiple of 4!\n", m_Patterns.size());
        return RC::BAD_PARAMETER;
    }
    if (!m_UseRandomData && (m_PatternSubsetSize > m_Patterns.size()))
    {
        Printf(Tee::PriError,
               "PatternSubsetSize (%u) cannot be greater "
               "than number of patterns (%lu)!\n",
                m_PatternSubsetSize,
                m_Patterns.size());
        return RC::BAD_PARAMETER;
    }
    if ((m_Patterns.size() % m_PatternSubsetSize) != 0)
    {
        Printf(Tee::PriError,
               "PatternSubsetSize (%u) must evenly divide "
               "number of patterns (%lu)!\n",
                m_PatternSubsetSize,
                m_Patterns.size());
        return RC::BAD_PARAMETER;
    }
    if ((m_PatternSubsetSize % 4) != 0)
    {
        Printf(Tee::PriError, "PatternSubsetSize %u must be a multiple of 4!\n", m_PatternSubsetSize);
        return RC::BAD_PARAMETER;
    }
    if (m_BoxDims.empty())
    {
        if (m_UseTMA)
        {
            // TODO: tune these parameters. The current ones are tuned for fmodel
            m_BoxDims.assign(2, m_PatternSubsetSize);
        }
        else
        {
            // By default, set the box as a 1-D object of length
            // (#max LWCA threads * PatternSubsetSize)
            // which ensures even distribution of work within a block.
            const UINT32 dimLength =
                GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK) * m_PatternSubsetSize;
            m_BoxDims.assign(1, dimLength);
        }
    }
    else
    {
        if (m_BoxDims.size() > 1)
        {
            Printf(Tee::PriError, "Only one-dimensional tensors/boxes are supported at this time!\n");
        }
        // Only dimension 0 has to be multiple of 4 for performance reasons (vector op.)
        if (m_BoxDims[0] == 0 || (m_BoxDims[0] % 4) != 0)
        {
            Printf(Tee::PriError, "Invalid length (%u) for dimension 0. Must be non-zero and a multiple of 4!\n", m_BoxDims[0]);
            return RC::BAD_PARAMETER;
        }
        for (UINT32 i = 1; i < m_BoxDims.size(); i++)
        {
            if (m_BoxDims[i] == 0)
            {
                Printf(Tee::PriError, "Invalid length (%u) for dimension %u. Must be non-zero!\n", m_BoxDims[i], i);
                return RC::BAD_PARAMETER;
            }
        }
    }
    m_NumDims = static_cast<UINT32>(m_BoxDims.size());

    if (!m_UseRandomData && (m_PatternSubsetSize > m_BoxDims[0]))
    {
        Printf(Tee::PriError,
               "Pattern subset size (%u) cannot be greater than length "
               " of box dimension 0 (%u)!\n",
               m_PatternSubsetSize,
               m_BoxDims[0]);
        return RC::BAD_PARAMETER;
    }

    if (m_UseTMA && (GetBoundLwdaDevice().GetCapability() < Lwca::Capability::SM_90))
    {
        Printf(Tee::PriError, "TMA functionality is only available on or after SM 9.0!\n");
        return RC::BAD_PARAMETER;
    }
    // For TMA, tensor sizes are of type UINT32, so we cannot describe
    // memory regions greater than 2^32 bytes as a single, one-dimensional tensor.
    if (m_UseTMA && m_NumDims <= 1)
    {
        Printf(Tee::PriError, "When UseTMA=true, NumDims must be > 1\n");
        return RC::BAD_PARAMETER;
    }
    if (!m_UseTMA && m_PrefetchMode)
    {
        Printf(Tee::PriError, "PrefetchMode can only be used with UseTMA=true");
        return RC::BAD_PARAMETER;
    }
    // Need to rely on ECC when in prefetch mode
    if (m_PrefetchMode)
    {
        m_SkipErrorReporting = true;
    }

	if (m_PulseUs && !m_DelayUs)
    {
        m_DelayUs = m_PulseUs;
    }
    if (m_DelayUs && !m_PulseUs)
    {
        m_PulseUs = m_DelayUs;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Get pattern set by name
//!
RC LwdaTMAStress::GetPatternByName()
{
    RC rc;

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

    MASSERT((m_Patterns.size() % 4) == 0);

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Return a UINT32 array in string form
//!
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

//------------------------------------------------------------------------------
//! \brief Print properties of the JS object for the class
//!
void LwdaTMAStress::PrintJsProperties(Tee::Priority pri)
{
    LwdaMemTest::PrintJsProperties(pri);
    Printf(pri, "LwdaTMAStress Js Properties:\n");
    Printf(pri, "\tRuntimeMs:                      %u\n", m_RuntimeMs);
    Printf(pri, "\tIterations:                     %u\n", m_Iterations);
    Printf(pri, "\tMaxInnerIterations:             %u\n", m_MaxInnerIterations);
    Printf(pri, "\tStartPhys:                      %08llx\n", m_StartPhys);
    Printf(pri, "\tEndPhys:                        %08llx\n", m_EndPhys);
    Printf(pri, "\tUseRandomData:                  %s\n", m_UseRandomData ? "true" : "false");
    Printf(pri, "\tSkipErrorReporting:             %s\n", m_SkipErrorReporting ? "true" : "false");
    Printf(pri, "\tPatterns:                       %s\n", PrintUINT32Array(m_Patterns).c_str());
    Printf(pri, "\tPatternSubsetSize:              %u\n", m_PatternSubsetSize);
    Printf(pri, "\tPatternName:                    %s\n", m_PatternName.c_str());
    Printf(pri, "\tBoxDims:                        %s\n", PrintUINT32Array(m_BoxDims).c_str());
    Printf(pri, "\tUseTMA:                         %s\n", m_UseTMA ? "true" : "false");
    Printf(pri, "\tPrefetchMode:                   %s\n", m_PrefetchMode ? "true" : "false");
	if (m_PulseUs)
    {
        Printf(pri, "\tPulseUs:                        %f\n", m_PulseUs);
        Printf(pri, "\tDelayUs:                        %f\n", m_DelayUs);
    }
}

//------------------------------------------------------------------------------
//! \brief Check whether LwdaTMAStress is supported
//!
bool LwdaTMAStress::IsSupported()
{
    if (!LwdaMemTest::IsSupported())
    {
        return false;
    }

    // TODO: this should be >= Lwca::capability::SM_90, maybe make this based on
    // Makefile define?
    return GetBoundLwdaDevice().GetCapability() >= Lwca::Capability::SM_80;
}

//------------------------------------------------------------------------------
//! \brief Setup the properties needed to run the test
//!
RC LwdaTMAStress::Setup()
{
    RC rc;

    CHECK_RC(LwdaMemTest::Setup());

    // Callwlate optimal number of blocks and threads for kernels
    CallwlateKernelGeometry();

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("tmastress", &m_Module));

    const char *fillFuncName = GetFillFuncName();
    m_FillFunc = m_Module.GetFunction(fillFuncName, m_NumBlocks, m_NumFillThreads);
    m_FillFunc.InitCheck();

    m_TMAStressFunc = m_Module.GetFunction("TMAStress", m_NumBlocks, m_NumStressThreads);
    m_TMAStressFunc.InitCheck();

    const char *checkFuncName = GetCheckFuncName();
    m_CheckFunc = m_Module.GetFunction(checkFuncName, m_NumBlocks, m_NumCheckThreads);
    m_CheckFunc.InitCheck();

    // When UseTMA=true, we're using TMA to fill memory, so we need to allocate
    // shared memory
    if (m_UseTMA)
    {
        CHECK_RC(m_FillFunc.SetSharedSizeMax());
    }
    CHECK_RC(m_TMAStressFunc.SetSharedSizeMax());
    CHECK_RC(m_CheckFunc.SetSharedSizeMax());

    // Callwlate the number of boxes that fit into smem
    // This is used in the main stress kernel, where each block loads multiple
    // boxes into smem before storing them into FB
    // This is irrelevant for Check/Fill kernels since we check/fill one box
    // at a time
    if (m_UseTMA)
    {
        UINT32 dynamicSharedMemSize = 0;
        m_TMAStressFunc.GetSharedSize(&dynamicSharedMemSize);

        // Callwlate number of elements in a box
        m_NumBoxElems = 1;
        for (UINT32 i = 0; i < m_NumDims; i++)
        {
            m_NumBoxElems *= m_BoxDims[i];
        }
        m_BoxMemSize = m_NumBoxElems * sizeof(m_BoxDims[0]);

        // Each smem region assigned to a box needs to be 128B aligned
        // to ensure the starting address of each box itself is 128B aligned
        const UINT32 alignedBoxMemSize = ALIGN_UP(m_BoxMemSize, 128U);

        // Callwlate smem size in bytes reserved for mbarrier
        const UINT32 reservedMemSize = SMEM_OFFSET * sizeof(AlignStruct128B);
        m_NumBoxesInSMem = (dynamicSharedMemSize - reservedMemSize) / alignedBoxMemSize;

        if (m_NumBoxesInSMem == 0)
        {
            Printf(Tee::PriError,
                   "Not enough shared memory (%u) to accommodate a box! "
                   "Consider decreasing box dimensions\n",
                   dynamicSharedMemSize);
            return RC::BAD_PARAMETER;
        }
    }

    // Create events for measuring kernel time
    m_LaunchBegin = GetLwdaInstance().CreateEvent();
    m_LaunchEnd = GetLwdaInstance().CreateEvent();
    if (m_PulseUs)
    {
        m_BatchBegin = GetLwdaInstance().CreateEvent();
        m_BatchEnd = GetLwdaInstance().CreateEvent();
    }

    m_MaxTestSteps = m_RuntimeMs ? m_RuntimeMs : m_NumOuterIterations;
    PrintProgressInit(m_MaxTestSteps);

    if (!m_SkipErrorReporting)
    {
        CHECK_RC(SetupErrorReporting());
    }

    CHECK_RC(AllocateEntireFramebuffer());

    // If using patterns, copy array of patterns to constant memory
    if (!m_Patterns.empty())
    {
        const size_t patternSetSize = m_Patterns.size() * sizeof(m_Patterns[0]);
        Lwca::Global patternsConst = m_Module.GetGlobal("g_Patterns");
        CHECK_RC(patternsConst.InitCheck());

        // Throw error if there's not enough constant memory to accommodate
        // the provided array of patterns
        if (patternSetSize > patternsConst.GetSize())
        {
            Printf(Tee::PriError,
                   "Could not allocate enough memory to accomodate provided "
                   "number of patterns. Max number of patterns allowed: %llu",
                    patternsConst.GetSize() / sizeof(UINT32));
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        patternsConst.Set(m_Patterns.data(), patternSetSize);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Callwlate number of blocks and threads to run in the kernels
//!
void LwdaTMAStress::CallwlateKernelGeometry()
{
    // We should be using all the shaders to maximize throughput for all
    // Fill, Stress, and Check kernels
    m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();

    // Use max number of threads per block for filling and checking memory
    const UINT32 numMaxThreadsPerBlock =
        GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK);
    m_NumFillThreads = numMaxThreadsPerBlock;
    m_NumCheckThreads = numMaxThreadsPerBlock;

    // TODO: revisit this TMA stress kernel - just 1 thread per block enough?
    // m_NumStressThreads = 1;
    m_NumStressThreads = numMaxThreadsPerBlock;

    // TODO: revisit this post-silicon. This makes it easy to debug
    if (m_UseTMA)
    {
        m_NumBlocks = 1;
        m_NumStressThreads = 1;
        m_NumFillThreads = 1;
        m_NumCheckThreads = numMaxThreadsPerBlock;
    }
}

//------------------------------------------------------------------------------
//! Get the name of the fill kernel, based on test parameters
//!
const char * LwdaTMAStress::GetFillFuncName()
{
     return m_UseRandomData ? "FillRandom" : "FillPattern";
}

//------------------------------------------------------------------------------
//! Get the name of the check kernel, based on test parameters
//!
const char * LwdaTMAStress::GetCheckFuncName()
{
    return m_UseRandomData ? "CheckRandom" : "CheckPattern";
}

//------------------------------------------------------------------------------
//! Setup infrastructure needed to report mem errors
//!
RC LwdaTMAStress::SetupErrorReporting()
{
    RC rc;

    UINT32 checkFuncSharedMemSize = 0;
    CHECK_RC(m_CheckFunc.GetSharedSize(&checkFuncSharedMemSize));

    // When using TMA, we have to set aside some amount of dsmem for issuing
    // load instructions. Use the rest of the available memory for error reporting.
    if (m_UseTMA)
    {
        // Callwlate amount of smem used for mbarrier
        const UINT32 reservedMemSize = SMEM_OFFSET * sizeof(AlignStruct128B);
        const UINT32 availableMemSize =
            (checkFuncSharedMemSize - m_BoxMemSize - reservedMemSize);

        m_MaxLocalErrors = availableMemSize / sizeof(BadValue);
    }
    else
    {
        m_MaxLocalErrors = checkFuncSharedMemSize / sizeof(BadValue);
    }
    m_MaxGlobalErrors = m_MaxLocalErrors * m_NumBlocks;

    // RangeErrors struct already includes 1 BadValue
    const UINT32 resultsSize =
        sizeof(RangeErrors) + ((m_MaxGlobalErrors - 1) * sizeof(BadValue));

    CHECK_RC(GetLwdaInstance().AllocHostMem(resultsSize, &m_HostResults));
    CHECK_RC(m_HostResults.Clear());

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Run the test
//!
RC LwdaTMAStress::Run()
{
    RC rc;

    vector<ChunkData> testableChunks;
    CHECK_RC(DetermineTestableChunks(&testableChunks));

    for (auto& chunk : testableChunks)
    {
        CHECK_RC(SetupTensorDims(&chunk));
        InitBaseInput(&chunk);
    }

    // Run outer loops
    const UINT64 runStartMs = Xp::GetWallTimeMS();
    UINT32 outer = 0;
    m_TestStepsCompleted = 0;
    bool shouldFillMemory = true;

    while (m_TestStepsCompleted < m_MaxTestSteps)
    {
        CHECK_RC(RunChunkOuterIteration(&testableChunks, outer, shouldFillMemory));

        const UINT64 elapsedTime = Xp::GetWallTimeMS() - runStartMs;
        m_TestStepsCompleted = m_RuntimeMs ?
                               min(elapsedTime, static_cast<UINT64>(m_RuntimeMs)) :
                               static_cast<UINT64>(outer + 1);

        PrintProgressUpdate(m_TestStepsCompleted);

        if (!m_SkipErrorReporting && ErrorsFound())
        {
            PrintErrorUpdate(m_TestStepsCompleted, RC::BAD_MEMORY);
            if (GetGoldelwalues()->GetStopOnError())
            {
                GetJsonExit()->AddFailLoop(outer);
                break;
            }
            // If we have encountered errors, we need to fill the memory again
            // with the correct patterns/data.
            shouldFillMemory = true;
        }
        else if (!m_UseRandomData)
        {
            // If we have not encountered errors, there's no need to launch
            // additional fill kernels in the next outer iteration.
            // The exception is when UseRandomData=true, since we use different
            // sets of random data for each outer iteration.
            shouldFillMemory = false;
        }

        CHECK_RC(EndLoop(outer));

        ++outer;
    }

    // Callwlate total bandwidth
    const double execTimeS = m_TotalExecTimeMs / 1000.0;
    const INT32 pri = GetMemError(0).GetAutoStatus() ?
                          Tee::PriNormal :
                          GetVerbosePrintPri();
    GpuTest::RecordBps
    (
        static_cast<double>(m_BytesTransferred),
        execTimeS,
        static_cast<Tee::Priority>(pri)
    );

    // Keep track of reported errors
    if (!m_SkipErrorReporting)
    {
        CHECK_RC(TrackErrors());
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Determine chunks in our tested mem range and initialize
//! corresponding chunk data
//!
RC LwdaTMAStress::DetermineTestableChunks(vector<ChunkData> *pTestableChunks)
{
    RC rc;

    pTestableChunks->reserve(NumChunks());
    for (UINT32 ichunk = 0; ichunk < NumChunks(); ++ichunk)
    {
        // Limit test to the current range
        UINT64 startOffset;
        UINT64 endOffset;
        if (!GetTestRange(ichunk, &startOffset, &endOffset))
        {
            continue;
        }

        GpuUtility::MemoryChunkDesc * const pDesc =
            &LwdaMemTest::GetChunkDesc(ichunk);

        const UINT64 devPtr = GetLwdaChunk(ichunk).GetDevicePtr();
        const UINT64 nominalAddr = pDesc->contiguous ?
                                       VirtualToPhysical(devPtr) :
                                       0;
        const UINT64 testableChunkMemSize = endOffset - startOffset;

        pTestableChunks->emplace_back
        (
            devPtr,
            nominalAddr,
            startOffset,
            endOffset,
            testableChunkMemSize,
            pDesc
        );

        CHECK_RC(pTestableChunks->back().Initialize
        (
            GetTestConfiguration()->Seed(),
            GetLwdaInstancePtr()
        ));
    }

    if (pTestableChunks->empty())
    {
        Printf
        (
            Tee::PriError,
            "Could not find testable memory chunks!\n"
        );
        return RC::BAD_PARAMETER;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! Trim chunk memory range to fit user phys mem specifications, if any
//! @return true if if the trimmed range is valid, false if not
//!
bool LwdaTMAStress::GetTestRange
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
    const Lwca::ClientMemory *pMem = &GetLwdaChunk(ichunk);
    const UINT64 memSize = pMem->GetSize();
    const UINT64 chunkStartPAddr =
        LwdaMemTest::GetChunkDesc(ichunk).contiguous ?
            LwdaMemTest::VirtualToPhysical(pMem->GetDevicePtr()) :
            0;
    const UINT64 chunkEndPAddr = chunkStartPAddr + memSize;

    // Determine requested physical memory range
    UINT64 fbStartPAddr = LwdaMemTest::GetStartLocation() * 1_MB;
    UINT64 fbEndPAddr = LwdaMemTest::GetEndLocation();
    fbEndPAddr = (fbEndPAddr == 0) ? ~fbEndPAddr : (fbEndPAddr + 1) * (1024 * 1024);
    MASSERT(fbEndPAddr >= fbStartPAddr);

    UINT64 startPAddr = std::max(fbStartPAddr, chunkStartPAddr);
    UINT64 endPAddr = std::min(fbEndPAddr, chunkEndPAddr);

    if (endPAddr <= startPAddr)
    {
        return false;
    }

    // Apply physical address boundaries
    if (m_StartPhys)
    {
        if (m_StartPhys < startPAddr || m_StartPhys > endPAddr)
        {
            return false;
        }
        startPAddr = m_StartPhys;
    }
    if (m_EndPhys)
    {
        if (m_EndPhys < startPAddr || m_EndPhys > endPAddr)
        {
            return false;
        }
        endPAddr = m_EndPhys;
    }

    // Compute offsets from the beginning of the memory block
    *pStartOffset = startPAddr - chunkStartPAddr;
    *pEndOffset = endPAddr - chunkStartPAddr;

    // align on 32-bit words
    *pEndOffset &= ~static_cast<UINT64>(3); // align on 32-bit words

    // Make tested mem range 32-byte aligned (vector of 4 UINT32)
    const UINT64 lwrrSize = *pEndOffset - *pStartOffset;
    const UINT64 adjustedSize = ALIGN_DOWN(*pEndOffset - *pStartOffset, sizeof(uint4));
    if (lwrrSize > adjustedSize)
    {
        Printf
        (
            Tee::PriWarn,
            "Skipping the last %llu bytes from chunk %u\n",
            lwrrSize - adjustedSize,
            ichunk
        );
    }
    *pEndOffset = *pStartOffset + adjustedSize;

    return true;
}

//------------------------------------------------------------------------------
//! \brief Setup tensor dimensions for this memory chunk.
//!
//! The number of dimensions "n" is set by the user through arg "BoxDims", or it
//! is set by default to 2. We then go through the following steps:
//!
//! 1) Represent the 1d array of memory as an n-dimensional of equal dimension lengths
//! 2) Extend the length of the last dimension as much as possible to improve memory coverage
//!
//! e.g.
//!     - 3375 bytes of testable memory | 3-dimensional tensor
//!     - Take lwbe root of 3375 and round down to nearest integer
//!       [15, 15 , 15]
//!     - Align the length of first dimension to 4 so we can use vectorized loads/stores in LWCA
//!       [12, 15, 15]
//!     - Extend last dimension so we use maximize memory coverage
//!       3375 / (12 * 15) = 18.75; rounded down = 18
//!       [12, 15, 18]
//!
RC LwdaTMAStress::SetupTensorDims(ChunkData *pChunk)
{
    RC rc;

    const UINT64 numElemsInChunk = pChunk->size / sizeof(UINT32);

    // To colwert 1d array to n-dimensional array of equally sized lengths, we take
    // the nth root of the 1d array size, then round down the result.
    double nthRoot = pow(static_cast<double>(numElemsInChunk), (1.0 / m_NumDims));
    UINT64 rootOfClosestPerfectPower = static_cast<UINT64>(nthRoot);

    std::fill_n(pChunk->tensorDims.vals, m_NumDims, rootOfClosestPerfectPower);

    // Length of 1st dimension should be aligned to 4 so we can use vector operations
    pChunk->tensorDims.vals[0] = ALIGN_DOWN(pChunk->tensorDims.vals[0], 4U);

    // Length of last dimension is stretched out as much as possible to increase
    // memory coverage
    UINT64 divider = 1;
    for (UINT32 i = 0; i < m_NumDims - 1; i++)
    {
        divider *= pChunk->tensorDims.vals[i];
    }
    pChunk->tensorDims.vals[m_NumDims - 1] = numElemsInChunk / divider;

    UINT64 numElemsInTensor = 1;
    // Callwlate number of words in this tensor and check to make sure the box
    // fits into the tensor.
    for (UINT32 i = 0; i < m_NumDims; i++)
    {
        const UINT64 tensorDimLength = pChunk->tensorDims.vals[i];
        const UINT64 boxDimLength = m_BoxDims[i];
        if (!tensorDimLength || tensorDimLength < boxDimLength)
        {
            Printf(Tee::PriError, "Cannot fit box into tensor! Consider decreasing box dimensions.\n");
            return RC::BAD_PARAMETER;
        }

        numElemsInTensor *= tensorDimLength;
    }

    // Adjust endOffset since we may have truncated the testable memory region
    // in order to represent it as a tensor
    pChunk->endOffset = pChunk->startOffset + (numElemsInTensor * sizeof(UINT32));

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Prepare TMA tensor descriptor for use
//!
void LwdaTMAStress::InitTMATensorDescriptor
(
    ChunkData *pChunk
)
{
    UINT32 tensorSizes[5] = {0};
    UINT32 paddedTensorSizes[5] = {0};
    for (UINT32 i = 0; i < m_NumDims; i++)
    {
        tensorSizes[i] = static_cast<UINT32>(pChunk->tensorDims.vals[i]);
        // Padded tensor sizes such that boxes fit evenly into the tensor
        // Needed to callwlate tensorStrides
        paddedTensorSizes[i] =
            ALIGN_UP(static_cast<UINT32>(pChunk->tensorDims.vals[i]), m_BoxDims[i]);
    }

    UINT64 tensorStrides[4] = {0};
    tensorStrides[0] = paddedTensorSizes[0] * sizeof(UINT32);
    for (UINT32 i = 1; i < m_NumDims - 1; i++)
    {
        tensorStrides[i] = tensorStrides[i - 1] * paddedTensorSizes[i];
    }

    // Don't skip over elements in any dimension
    UINT32 traversalStrides[5] = {1, 1, 1, 1, 1};

    lwdaSetTmaTileDescriptorv2(&pChunk->tmaDesc,
    /*GlobalMemAddr*/          reinterpret_cast<void*>(pChunk->tmaStressInput.mem),
    /*NumDimensions*/          m_NumDims,
    /*Element Format*/         U32,
    /*Interleaved*/            INTERLEAVE_DISABLED,
    /*SMEMswizzleMode*/        SWIZZLE_DISABLED,
    /*L2sectorPromotion*/      PROMOTION_DISABLED,
    /*TensorSize*/             &tensorSizes[0],
    /*TensorStride*/           &tensorStrides[0],
    /*TraversalStride*/        &traversalStrides[0],
    /*BoxSize*/                m_BoxDims.data(),
    /*OOBfillMode*/            0,
    /*F32toTF32*/              0);
}

//------------------------------------------------------------------------------
//! Initialize kernel input for each memory chunk
//!
void LwdaTMAStress::InitBaseInput(ChunkData *pChunk)
{
    UINT64 startDevPtr = pChunk->memDevPtr + pChunk->startOffset;
    if (m_UseTMA)
    {
        // For TMA, global memory address must be 16B aligned
        startDevPtr = ALIGN_UP(startDevPtr, 16U);
    }
    const UINT64 endDevPtr = pChunk->memDevPtr + pChunk->endOffset;
    MASSERT(startDevPtr < endDevPtr);

    Dimensions boxDims = {0};
    UINT64 numBoxesInTensor = 1;
    for (UINT32 i = 0; i < m_NumDims; i++)
    {
        boxDims.vals[i] = m_BoxDims[i];
        numBoxesInTensor *= CEIL_DIV(pChunk->tensorDims.vals[i], static_cast<UINT64>(m_BoxDims[i]));
    }

    pChunk->tmaStressInput =
    {
        /*mem*/                startDevPtr,
        /*memEnd*/             endDevPtr,
        /*useTMA*/             m_UseTMA,
        /*prefetchMode*/       m_PrefetchMode,
        /*numIterations*/      m_NumInnerIterations,
        /*numDims*/            m_NumDims,
        /*boxDims*/            boxDims,
        /*tensorDims*/         pChunk->tensorDims,
        /*numBoxesInTensor*/   numBoxesInTensor,
        /*numBoxesInSMem*/     m_NumBoxesInSMem,
        /*boxMemSize*/         m_BoxMemSize,
		/*startingBoxIdx*/     0,
        /*numBoxesInBatch*/    numBoxesInTensor,
        /*delayNs*/            static_cast<UINT64>(m_DelayUs * 1000.0),
        /*dummyVar*/           0
    };

    pChunk->checkFillInput =
    {
        /*mem*/                startDevPtr,
        /*memEnd*/             endDevPtr,
        /*results*/            m_SkipErrorReporting ? 0 : m_HostResults.GetDevicePtr(GetBoundLwdaDevice()),
        /*useTMA*/             m_UseTMA,
        /*randSeed*/           static_cast<UINT64>(GetTestConfiguration()->Seed()),
        /*outerIter*/          0,
        /*maxGlobalErrors*/    m_MaxGlobalErrors,
        /*macLocalErrors*/     m_MaxLocalErrors,
        /*numPatSubsetsInBox*/ m_NumBoxElems / m_PatternSubsetSize,
        /*numPatternSubsets*/  static_cast<UINT32>(m_Patterns.size() / m_PatternSubsetSize),
        /*numPatVecsInSubset*/ m_PatternSubsetSize / 4,
        /*numDims*/            m_NumDims,
        /*boxDims*/            boxDims,
        /*tensorDims*/         pChunk->tensorDims,
        /*numBoxesInTensor*/   numBoxesInTensor
    };

    if (m_UseTMA)
    {
        InitTMATensorDescriptor(pChunk);
    }

    const FLOAT32 memSizeMB = (endDevPtr - startDevPtr) / (1024.0f * 1024.0f);
    Printf
    (
        GetVerbosePrintPri(),
        "LwdaTMAStress: Running %s on %.1fMB of memory,"
        " phys addr 0x%08llX..0x%08llX\n",
        m_RuntimeMs ?
            Utility::StrPrintf("%u ms", m_RuntimeMs).c_str() :
            Utility::StrPrintf("%u iterations", m_Iterations).c_str(),
        memSizeMB,
        pChunk->nominalAddr + pChunk->startOffset,
        pChunk->nominalAddr + pChunk->endOffset
    );
}
//------------------------------------------------------------------------------
//! \brief Set tensor descriptor in constant memory
//!
RC LwdaTMAStress::SetTensorDescriptor
(
    lwdaTmaDescv2 *pDesc
)
{
    RC rc;

    Lwca::Global global = m_Module.GetGlobal("g_TMADesc");
    CHECK_RC(global.InitCheck());

    if (global.GetDevicePtr() & 0x3FULL)
    {
        Printf(Tee::PriError, "Constant memory address should be 64B-aligned");
        return RC::SOFTWARE_ERROR;
    }
    global.Set(pDesc, sizeof(lwdaTmaDescv2));

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Launch a batch of workload
//!
//! A "batch" is any number or set of boxes we iterate over in succession
//!
RC LwdaTMAStress::LaunchBatch
(
    ChunkData *pChunk,
    UINT64 startingBoxIdx,
    UINT64 numBoxesInBatch
)
{
    RC rc;

    // Modify batch parameters
    pChunk->tmaStressInput.startingBoxIdx = startingBoxIdx;
    pChunk->tmaStressInput.numBoxesInBatch = numBoxesInBatch;

    CHECK_RC(m_TMAStressFunc.Launch(pChunk->tmaStressInput));

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Launch batches of workload such that each batch runs for "PulseUs" microseconds
//! and there's an idle time of "DelayUs" microseconds in between batches.
//!
RC LwdaTMAStress::LaunchBatchesWithPulse
(
    ChunkData *pChunk
)
{
    RC rc;
    MASSERT(m_PulseUs);
    MASSERT(m_DelayUs);

    // Total number boxes to be iterated over.
    const UINT64 totalNumBoxes =
        pChunk->tmaStressInput.numBoxesInTensor * m_NumInnerIterations;

    // Set number of boxes in batch to number of blocks as a starting point.
    // This number will get adjusted based on kernel runtime as we run more batches.
    UINT64 numBoxesInBatch = m_NumBlocks;
    UINT64 numBoxesIteratedSoFar = 0;
    double batchExecTimeUs = 0.0;

    while (numBoxesIteratedSoFar < totalNumBoxes)
    {
        numBoxesInBatch = std::min(numBoxesInBatch, (totalNumBoxes - numBoxesIteratedSoFar));

        // Begin batch timer
        CHECK_RC(m_BatchBegin.Record());

        // Launch batch
        const UINT64 startingBoxIdx =
            numBoxesIteratedSoFar % pChunk->tmaStressInput.numBoxesInTensor;
        CHECK_RC(LaunchBatch(pChunk, startingBoxIdx, numBoxesInBatch));

        // End batch timer. Record kernel exelwtion time.
        CHECK_RC(m_BatchEnd.Record());
        CHECK_RC(m_BatchEnd.Synchronize());
        batchExecTimeUs = m_BatchEnd.TimeMsElapsedSinceF(m_BatchBegin) * 1000.0;

        Printf(Tee::PriDebug,
               "Batch exelwtion time: %f us, "
               "Number of boxes per batch: %llu ,"
               "Number of boxters iterated over so far: %llu ,"
               "Total number of boxes (InnerIterations * number of boxes in tensor): %llu\n",
               batchExecTimeUs, numBoxesInBatch, numBoxesIteratedSoFar, totalNumBoxes);

        numBoxesIteratedSoFar += numBoxesInBatch;

        // Adjust number of boxes per batch
        if (batchExecTimeUs < m_DelayUs)
        {
            Printf(Tee::PriError,
                   "Kernel did not idle for the specified amount of time! "
                   "Total kernel duration: %f us, Expected delay duration: %f\n",
                   batchExecTimeUs, m_DelayUs);
            return RC::CANNOT_MEET_WAVEFORM;
        }
        const double activeDurationUs = batchExecTimeUs - m_DelayUs;
        const double multiplier = m_PulseUs / activeDurationUs;

        // Ensure number of boxes per batch is a multiple of #blocks
        numBoxesInBatch = ALIGN_DOWN(static_cast<UINT64>(numBoxesInBatch * multiplier),
                                     static_cast<UINT64>(m_NumBlocks));
        numBoxesInBatch = std::max(1ULL, numBoxesInBatch);
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Run outer chunk iteration
//!
RC LwdaTMAStress::RunChunkOuterIteration
(
    vector<ChunkData> *pTestableChunks,
    UINT32 outer,
    bool shouldFillMemory
)
{
    RC rc;

    // Adjust dynamic chunk inputs params (i.e. params not iniitialized in
    // InitBaseInput such as "outerIter") here
    for (auto& chunk : *pTestableChunks)
    {
        chunk.checkFillInput.outerIter = outer;
    }

    // Fill memory with either random data or patterns
    if (shouldFillMemory)
    {
        for (auto& chunk : *pTestableChunks)
        {
            if (m_UseTMA)
            {
                CHECK_RC(SetTensorDescriptor(&chunk.tmaDesc));
            }
            CHECK_RC(m_FillFunc.Launch(chunk.checkFillInput));
        }
    }

    // Run the stress kernel
    CHECK_RC(m_LaunchBegin.Record());
    for (auto& chunk : *pTestableChunks)
    {
		if (m_UseTMA)
        {
            CHECK_RC(SetTensorDescriptor(&chunk.tmaDesc));
        }

		if (m_PulseUs)
        {
            CHECK_RC(LaunchBatchesWithPulse(&chunk));
        }
        else
        {
            const UINT64 numBoxesInBatch =
                chunk.tmaStressInput.numBoxesInTensor * m_NumInnerIterations;
            CHECK_RC(LaunchBatch(&chunk, 0, numBoxesInBatch));
        }
        const UINT64 memSize = chunk.endOffset - chunk.startOffset;
        const UINT32 numAccesses = 2;
        m_BytesTransferred += memSize * numAccesses * m_NumInnerIterations;
    }
    CHECK_RC(m_LaunchEnd.Record());
    CHECK_RC(m_LaunchEnd.Synchronize());
    m_TotalExecTimeMs += m_LaunchEnd.TimeMsElapsedSinceF(m_LaunchBegin);

    // Check for mem errors and report them to host memory if there are any
    if (!m_SkipErrorReporting)
    {
        for (auto& chunk : *pTestableChunks)
        {
            if (m_UseTMA)
            {
                CHECK_RC(SetTensorDescriptor(&chunk.tmaDesc));
            }
            CHECK_RC(m_CheckFunc.Launch(chunk.checkFillInput));
        }
    }

    CHECK_RC(GetLwdaInstance().Synchronize());

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Check if there were errors after running the stress kernel
//!
bool LwdaTMAStress::ErrorsFound()
{
    // This method shouldn't be called when we're skipping error reports
    MASSERT(!m_SkipErrorReporting);
    const UINT64 numErrors =
        reinterpret_cast<const RangeErrors*>(m_HostResults.GetPtr())->numErrors;

    return numErrors > 0;
}

//------------------------------------------------------------------------------
//! \brief Log found errors
//!
RC LwdaTMAStress::TrackErrors()
{
    RC rc;

    CHECK_RC(CheckResultsCorrupted());

    const RangeErrors* const results =
        reinterpret_cast<const RangeErrors*>(m_HostResults.GetPtr());

    UINT64 totalNumErrors = 0;
    totalNumErrors += results->numErrors;
    const UINT64 numUnloggedErrors
        = results->numErrors - results->numReportedErrors;

    if (numUnloggedErrors > 0)
    {
        CHECK_RC(GetMemError(0).LogMysteryError(numUnloggedErrors));
    }

    // Report logged errors
    for (UINT32 ierr = 0; ierr < results->numReportedErrors; ++ierr)
    {
        const BadValue& badValue = results->reportedValues[ierr];

        // Colwert error's virtual address to physical address
        const UINT64 fbOffset = VirtualToPhysical(badValue.address);
        GpuUtility::MemoryChunkDesc *pDesc =
            &GetChunkDesc(FindChunkByVirtualAddress(badValue.address));

        CHECK_RC(GetMemError(0).LogUnknownMemoryError
        (
            32,
            fbOffset,
            badValue.actual,
            badValue.expected,
            pDesc->pteKind,
            pDesc->pageSizeKB
        ));
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! Valid error results
//!
RC LwdaTMAStress::CheckResultsCorrupted()
{
    const RangeErrors* const results =
        reinterpret_cast<const RangeErrors*>(m_HostResults.GetPtr());

    if (results->numReportedErrors > results->numErrors)
    {
        Printf
        (
            Tee::PriError,
            "Error results are corrupted. Number of reported errors: %u "
            "greater than total number of errors: %llu",
            results->numReportedErrors,
            results->numErrors
        );

        return RC::BAD_MEMORY;
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Cleanup
//!
RC LwdaTMAStress::Cleanup()
{
    m_LaunchBegin.Free();
    m_LaunchEnd.Free();
    m_BatchBegin.Free();
    m_BatchEnd.Free();
    m_HostResults.Free();
    m_Module.Unload();

    return LwdaMemTest::Cleanup();
}

//------------------------------------------------------------------------------
//! \brief Initialize chunk
//!
RC LwdaTMAStress::ChunkData::Initialize
(
    UINT32 seed,
    const Lwca::Instance *pLwdaInstance
)
{
    RC rc;

    random.SeedRandom(seed);

    return rc;
}

JS_CLASS_INHERIT(LwdaTMAStress, LwdaMemTest,
                 "Lwca TMA stress test");
CLASS_PROP_READWRITE(LwdaTMAStress, RuntimeMs, UINT32,
                     "How long to run the active portion of the test, in milliseconds. Overrides \"Iterations\"");
CLASS_PROP_READWRITE(LwdaTMAStress, Iterations, UINT32,
                     "Number of times to repeat test");
CLASS_PROP_READWRITE(LwdaTMAStress, MaxInnerIterations, UINT32,
                     "Maximum number of iterations with which to ilwoke the kernel");
CLASS_PROP_READWRITE(LwdaTMAStress, StartPhys, UINT64,
                     "Starting physical address");
CLASS_PROP_READWRITE(LwdaTMAStress, EndPhys, UINT64,
                     "Ending physical address");
CLASS_PROP_READWRITE(LwdaTMAStress, UseRandomData, bool,
                     "Use random data in place of Patterns. Each LWCA thread uses different random values");
CLASS_PROP_READWRITE(LwdaTMAStress, SkipErrorReporting, UINT64,
                     "Skip reporting of memory errors");
CLASS_PROP_READWRITE_JSARRAY(LwdaTMAStress, Patterns, JsArray,
                     "User-defined array of patterns");
CLASS_PROP_READWRITE(LwdaTMAStress, PatternSubsetSize, UINT32,
                     "Size of pattern subsets. Each box is filled sequentially with patterns from the corresponding subset");
CLASS_PROP_READWRITE(LwdaTMAStress, PatternName, string,
                     "Name of pattern to be used in LwdaTMAStress when explicit Patterns are not provided.");
CLASS_PROP_READWRITE_JSARRAY(LwdaTMAStress, BoxDims, JsArray,
                     "List of box dimension lengths.");
CLASS_PROP_READWRITE(LwdaTMAStress, UseTMA, bool,
                     "Use TMA hardware and instructions to perform memory test");
CLASS_PROP_READWRITE(LwdaTMAStress, PrefetchMode, bool,
                     "Prefetch patterns from FB to cache. Will not check for errors in this mode");
CLASS_PROP_READWRITE(LwdaTMAStress, PulseUs, double,
                     "Time spent by the kernel doing memory accesses in microseconds.");
CLASS_PROP_READWRITE(LwdaTMAStress, DelayUs, double,
                     "Time spent by the kernel idling after each batch of memory accesses.");
