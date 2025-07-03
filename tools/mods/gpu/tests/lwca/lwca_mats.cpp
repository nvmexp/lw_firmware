/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2014-2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdamemtest.h"
#include "gpu/tests/gpumtest.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "core/include/memerror.h"
#include "gpu/tests/lwca/mats/lwdamats.h"
#include "gpu/utility/surfrdwr.h"
#include "gpu/utility/chanwmgr.h"
#include "core/include/utility.h"

#ifndef INCLUDED_STL_SET
#include <set>
#endif

//! LWCA-based MATS (Modified Algorithmic Test Sequence) memory test.
//!
//! This test runs a LWCA kernel to perform a MATS memory test.
//! The description on how this test works is in the kernel's
//! source file, lwdamats.lw.
class LwdaMatsTest : public LwdaMemTest
{
public:
    LwdaMatsTest();
    virtual ~LwdaMatsTest() { }
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    // JS property accessors
    SETGET_PROP(Iterations,         UINT32);
    SETGET_PROP(MaxErrorsPerBlock,  UINT32);
    SETGET_PROP(MemChunkSize,       UINT32);
    SETGET_PROP(MemAccessSize,      UINT32);
    SETGET_PROP(ColumnTestMemChunkSleep, UINT32);
    SETGET_PROP(StartPhys,          UINT64);
    SETGET_PROP(EndPhys,            UINT64);
    SETGET_PROP(NumBlocks,          UINT32);
    SETGET_PROP(NumThreads,         UINT32);
    SETGET_PROP(ColumnStep,         UINT32);
    SETGET_PROP(ColumnToTest,       UINT32);
    SETGET_PROP(ColumnIlwStep,      UINT32);
    SETGET_PROP(ColumnSleep,        UINT32);
    SETGET_PROP(Direction,          string);
    SETGET_PROP(ColumnTest,         bool);
    SETGET_PROP(ShmooEnabled,       bool);
    SETGET_PROP(ShmooPatternIdx,    UINT32);
    GET_PROP(NumErrors,             UINT32);

private:
    void CallwlateKernelGeometry();
    UINT32 GetResultsSize() const;
    bool GetTestRange(UINT64 physAddr, UINT64 size,
                      UINT64* pStartOffset, UINT64* pEndOffset);
    bool SelectDevicePointers(UINT32 ichunk, LwdaMatsInput *input);
    RC LaunchLwdaColumnKernels(UINT32 pattern1, UINT32 pattern2,
                               UINT32 columnIlwStep, bool *pCorrupted);
    RC LaunchLwdaColumnKernel(UINT32 offsetIdx, UINT32 rdPattern,
                              UINT32 wrPattern, LwdaMatsInput *input);

    // Since LwdaMemTest defines virtual ReportErrors(), we need to add the
    // using here to ensure it doesn't get hidden by the overridden ReportErrors
    // defined below
    using LwdaMemTest::ReportErrors;
    RC ReportErrors(const char* results, bool* pCorrupted,
                    size_t resultsSize, UINT64 testedMemSize);

    // JS properties
    UINT32 m_Iterations;
    UINT32 m_MaxErrorsPerBlock;
    UINT32 m_MemChunkSize;
    UINT32 m_MemAccessSize;
    UINT32 m_ColumnTestMemChunkSleep;
    UINT64 m_StartPhys;
    UINT64 m_EndPhys;
    UINT32 m_NumBlocks;
    UINT32 m_NumThreads;
    UINT32 m_ColumnStep;
    UINT32 m_ColumnToTest;
    UINT32 m_ColumnIlwStep;
    UINT32 m_ColumnSleep;
    string m_Direction;
    bool m_ColumnTest;
    bool m_ShmooEnabled;
    UINT32 m_ShmooPatternIdx;
    UINT32 m_NumErrors;

    Lwca::Module m_Module;
    Lwca::Function m_InitLwdaMats;
    Lwca::Function m_LwdaMats;
    Lwca::Function m_LwdaColumns;
    Surface2D m_ResultsSurf;
    Lwca::DeviceMemory m_ResultsMem;
    UINT32 m_ActualMaxErrorsPerBlock;
    typedef std::map<UINT64, UINT64> PhysToVirt;

    enum TestDir
    {
        ascending,
        descending,
        both
    };
    TestDir m_TestDir;

    struct PatternSet
    {
        UINT32 pattern1;
        UINT32 pattern2;
    };
    vector<PatternSet> m_Patterns;

    class ReportedError
    {
    public:
        UINT64 address;
        UINT64 actual;
        UINT64 expected;
        UINT64 reread1;
        UINT64 reread2;
        UINT32 pteKind;
        UINT32 pageSizeKB;

        bool operator<(const ReportedError& e) const
        {
            if (address != e.address) return address < e.address;
            if (actual != e.actual) return actual < e.actual;
            if (expected != e.expected) return expected < e.expected;
            if (reread1 != e.reread1) return reread1 < e.reread1;
            if (reread2 != e.reread2) return reread2 < e.reread2;
            if (pteKind != e.pteKind) return pteKind < e.pteKind;
            return pageSizeKB < e.pageSizeKB;
        }
    };
    typedef multiset<UINT64> TimeStamps;
    typedef map<ReportedError, TimeStamps> ReportedErrors;
    ReportedErrors m_ReportedErrors;
};

JS_CLASS_INHERIT(LwdaMatsTest, LwdaMemTest,
                 "LWCA MATS (Modified Algorithmic Test Sequence)");

CLASS_PROP_READWRITE(LwdaMatsTest, Iterations, UINT32,
                     "Number of times to repeat test");
CLASS_PROP_READWRITE(LwdaMatsTest, MaxErrorsPerBlock, UINT32,
                     "Maximum number of errors reported per each LWCA block (1023=default)");
CLASS_PROP_READWRITE(LwdaMatsTest, MemChunkSize, UINT32,
                     "Size of a memory chunk being tested at once, in MB (0=disable splitting)");
CLASS_PROP_READWRITE(LwdaMatsTest, MemAccessSize, UINT32,
                     "Size of memory accesses when read/writing from a memory location. Supported: 32/64");
CLASS_PROP_READWRITE(LwdaMatsTest, ColumnTestMemChunkSleep, UINT32,
                     "Time to sleep between testing memory chunks during Lwca Column test, in milliseconds");
CLASS_PROP_READWRITE(LwdaMatsTest, StartPhys, UINT64,
                     "Starting physical address");
CLASS_PROP_READWRITE(LwdaMatsTest, EndPhys, UINT64,
                     "Ending physical address");
CLASS_PROP_READWRITE(LwdaMatsTest, NumBlocks, UINT32,
                     "Number of kernel blocks to run");
CLASS_PROP_READWRITE(LwdaMatsTest, NumThreads, UINT32,
                     "Number of kernel threads to run");
CLASS_PROP_READWRITE(LwdaMatsTest, ColumnStep, UINT32,
                     "Interval between columns being tested conlwrrently (when ColumnTest is true)");
CLASS_PROP_READWRITE(LwdaMatsTest, ColumnToTest, UINT32,
                     "Specific column to test (0 -> ColumnStep) during ColumnTest. When specified, all other columns will be skipped. By default, all columns are tested");
CLASS_PROP_READWRITE(LwdaMatsTest, ColumnIlwStep, UINT32,
                     "Interval in bytes between blocks to ilwerse values of patterns - to interleave values in subsequent rows (when ColumnTest is true)");
CLASS_PROP_READWRITE(LwdaMatsTest, ColumnSleep, UINT32,
                     "Sleep time in ms between writes and reads (when ColumnTest is true)");
CLASS_PROP_READWRITE(LwdaMatsTest, Direction, string,
                     "Direction in which the test is performed - both(default), ascending, descending");
CLASS_PROP_READWRITE(LwdaMatsTest, ColumnTest, bool,
                     "Enables directed column test mode");
CLASS_PROP_READWRITE(LwdaMatsTest, ShmooEnabled, bool,
                     "Enable \"shmoo\" mode - that is disable internal loops on some of the parameters");
CLASS_PROP_READWRITE(LwdaMatsTest, ShmooPatternIdx, UINT32,
                     "When shmoo is enabled, the pattern index selects which pattern to test from the pattern pairs");
CLASS_PROP_READONLY(LwdaMatsTest, NumErrors, UINT32,
                     "Number of errors found, read only, valid after Run()");

LwdaMatsTest::LwdaMatsTest()
: m_Iterations(200),
  m_MaxErrorsPerBlock(0),
  m_MemChunkSize(0),
  m_MemAccessSize(32),
  m_ColumnTestMemChunkSleep(0),
  m_StartPhys(0),
  m_EndPhys(0),
  m_NumBlocks(0),
  m_NumThreads(0),
  m_ColumnStep(31),
  m_ColumnToTest(_UINT32_MAX),
  m_ColumnIlwStep(0),
  m_ColumnSleep(40),
  m_Direction("both"),
  m_ColumnTest(false),
  m_ShmooEnabled(false),
  m_ShmooPatternIdx(_UINT32_MAX),
  m_NumErrors(0),
  m_ActualMaxErrorsPerBlock(1023),
  m_TestDir(both)
{
    // Disable the RC watchdog on the subdevice for the duration of the test
    SetDisableWatchdog(true);

    SetName("LwdaMatsTest");
}

void LwdaMatsTest::PrintJsProperties(Tee::Priority pri)
{
    LwdaMemTest::PrintJsProperties(pri);

    Printf(pri, "LwdaMatsTest Js Properties:\n");
    Printf(pri, "\tIterations:\t\t\t%u\n", m_Iterations);
    Printf(pri, "\tMaxErrorsPerBlock:\t\t%u\n", m_MaxErrorsPerBlock);
    Printf(pri, "\tMemChunkSize:\t\t\t%uMB\n", m_MemChunkSize);
    Printf(pri, "\tMemAccessSize:\t\t\t%u\n", m_MemAccessSize);
    Printf(pri, "\tColumnTestMemChunkSleep:\t%u ms\n", m_ColumnTestMemChunkSleep);
    Printf(pri, "\tStartPhys:\t\t\t0x%08llx\n", m_StartPhys);
    Printf(pri, "\tEndPhys:\t\t\t0x%08llx\n", m_EndPhys);
    Printf(pri, "\tNumBlocks:\t\t\t%u\n", m_NumBlocks);
    Printf(pri, "\tNumThreads:\t\t\t%u\n", m_NumThreads);
    Printf(pri, "\tColumnStep:\t\t\t%u\n", m_ColumnStep);
    Printf(pri, "\tColumnToTest:\t\t\t%u\n", m_ColumnToTest);
    Printf(pri, "\tColumnIlwStep:\t\t\t%u\n", m_ColumnIlwStep);
    Printf(pri, "\tColumnSleep:\t\t\t%u ms\n", m_ColumnSleep);
    Printf(pri, "\tDirection:\t\t\t%s\n", m_Direction.c_str());
    Printf(pri, "\tColumnTest:\t\t\t%s\n", m_ColumnTest?"true":"false");
    Printf(pri, "\tShmooEnabled:\t\t\t%s\n", m_ShmooEnabled?"true":"false");
    Printf(pri, "\tShmooPatternIdx:\t\t0x%08x\n", m_ShmooPatternIdx);

    if (m_Patterns.size())
    {
        Printf(pri, "\tPatterns:\t\t\t[[%#.8x,%#.8x]",
               m_Patterns[0].pattern1, m_Patterns[0].pattern2);
        for (UINT32 i = 1; i < m_Patterns.size(); i++)
        {
            Printf(pri, ", [%#.8x,%#.8x]",
                   m_Patterns[i].pattern1, m_Patterns[i].pattern2);
        }
        Printf(pri, "]\n");
    }
}

RC LwdaMatsTest::InitFromJs()
{
    RC rc;

    CHECK_RC(LwdaMemTest::InitFromJs());

    vector<UINT32> patterns;
    JavaScriptPtr pJs;
    rc = pJs->GetProperty(GetJSObject(), "Patterns", &patterns);
    if (rc == RC::ILWALID_OBJECT_PROPERTY)
    {
        rc.Clear();
    }
    CHECK_RC(rc);

    // Verify that the patterns provided by the user contain an even num of elems
    if ((patterns.size() % 2) != 0)
    {
        Printf(Tee::PriWarn, "Patterns argument is invalid. It must contain"
                                " an even array of UINT32 elements.\n");
        return RC::ILWALID_ARGUMENT;
    }

    PatternSet pattern;
    for (UINT32 i = 0; i < patterns.size(); i += 2)
    {
        pattern.pattern1 = patterns[i];
        pattern.pattern2 = patterns[i+1];
        m_Patterns.push_back(pattern);
    }

    switch (m_MemAccessSize)
    {
        case 32:
            break;

        case 64:
            // Lwrrently, only Test 127 supports 64-bit mem accesses
            if (m_ColumnTest)
            {
                break;
            }
            // Intentional fall-through

        default:
            Printf(Tee::PriWarn, "MemAccessSize value is not supported: %u\n", m_MemAccessSize);
            return RC::ILWALID_ARGUMENT;
    }

    return rc;
}

RC LwdaMatsTest::Setup()
{
    RC rc;

    CHECK_RC(LwdaMemTest::Setup());

    if (m_ColumnToTest < _UINT32_MAX && m_ColumnToTest >= m_ColumnStep)
    {
        Printf(Tee::PriWarn, "ColumnToTest has invalid value: %u. It must be between 0 and (ColumnStep-1).\n", m_ColumnToTest);
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // Initialize context and module
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("lwmats", &m_Module));

    // Prepare kernels
    CallwlateKernelGeometry();
    m_InitLwdaMats = m_Module.GetFunction("InitLwdaMats");
    CHECK_RC(m_InitLwdaMats.InitCheck());
    m_LwdaMats = m_Module.GetFunction("LwdaMats");
    CHECK_RC(m_LwdaMats.InitCheck());
    m_LwdaColumns = m_Module.GetFunction("LwdaColumns");
    CHECK_RC(m_LwdaColumns.InitCheck());

    // Allocate memory for results
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(GetResultsSize(), &m_ResultsMem));

    // Allocate memory to test
    CHECK_RC(AllocateEntireFramebuffer());

    return OK;
}

RC LwdaMatsTest::Cleanup()
{
    // Free the memory allocated to m_Patterns
    vector<PatternSet> dummy1;
    m_Patterns.swap(dummy1);
    m_ResultsMem.Free();
    m_Module.Unload();
    m_NumErrors = 0;
    return LwdaMemTest::Cleanup();
}

void LwdaMatsTest::CallwlateKernelGeometry()
{
    // Default confirmed experimentally
    if (m_NumThreads == 0)
    {
        m_NumThreads = 1024;
    }

    // One block per SM
    if (m_NumBlocks == 0)
    {
        m_NumBlocks = GetBoundLwdaDevice().GetShaderCount();
    }
}

UINT32 LwdaMatsTest::GetResultsSize() const
{
    const UINT32 blockSize = sizeof(RangeErrors)
        + (m_ActualMaxErrorsPerBlock-1)*sizeof(BadValue);
    return blockSize;
}

bool LwdaMatsTest::GetTestRange
(
    UINT64 physAddr,
    UINT64 size,
    UINT64* pStartOffset,
    UINT64* pEndOffset
)
{
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

RC LwdaMatsTest::Run()
{
    // This was called in Setup(), but call it again here because a shmoo
    // (e.g. test 187) can modify the geometry between Setup() and Run().
    CallwlateKernelGeometry();

   // Check direction
    if (m_Direction == ENCJSENT("both"))
        m_TestDir = both;
    else if (m_Direction == ENCJSENT("ascending"))
        m_TestDir = ascending;
    else if (m_Direction == ENCJSENT("descending"))
        m_TestDir = descending;
    else
    {
        Printf(Tee::PriError, "Invalid test direction \"%s\"\n",
               m_Direction.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    m_InitLwdaMats.SetBlockDim(m_NumThreads);
    m_InitLwdaMats.SetGridDim(m_NumBlocks);
    m_LwdaMats.SetBlockDim(m_NumThreads);
    m_LwdaMats.SetGridDim(m_NumBlocks);
    m_LwdaColumns.SetBlockDim(m_NumThreads);
    m_LwdaColumns.SetGridDim(m_NumBlocks);

    RC rc;
    Printf(GetVerbosePrintPri(),
           "LwdaMats is testing LWCA device \"%s\"\n",
           GetBoundLwdaDevice().GetName().c_str());

    const UINT32 numChunks = NumChunks();
    bool corrupted = false;
    m_NumErrors = 0;
    m_ReportedErrors.clear();

    // Check patterns
    if (m_Patterns.empty())
    {
        if (m_ColumnTest)
        {
            const PatternSet columnPatterns[] =
            {
                {0x00000000, 0x33333333}, {0x00000000, 0x55555555}, {0x00000000, 0xAAAAAAAA}, {0x00000000, 0xCCCCCCCC}, {0x00000000, 0xFFFFFFFF},
                {0x33333333, 0x00000000}, {0x33333333, 0x55555555}, {0x33333333, 0xAAAAAAAA}, {0x33333333, 0xCCCCCCCC}, {0x33333333, 0xFFFFFFFF},
                {0x55555555, 0x00000000}, {0x55555555, 0x33333333}, {0x55555555, 0xAAAAAAAA}, {0x55555555, 0xCCCCCCCC}, {0x55555555, 0xFFFFFFFF},
                {0xAAAAAAAA, 0x00000000}, {0xAAAAAAAA, 0x33333333}, {0xAAAAAAAA, 0x55555555}, {0xAAAAAAAA, 0xCCCCCCCC}, {0xAAAAAAAA, 0xFFFFFFFF},
                {0xCCCCCCCC, 0x00000000}, {0xCCCCCCCC, 0x33333333}, {0xCCCCCCCC, 0x55555555}, {0xCCCCCCCC, 0xAAAAAAAA}, {0xCCCCCCCC, 0xFFFFFFFF},
                {0xFFFFFFFF, 0x00000000}, {0xFFFFFFFF, 0x33333333}, {0xFFFFFFFF, 0x55555555}, {0xFFFFFFFF, 0xAAAAAAAA}, {0xFFFFFFFF, 0xCCCCCCCC}
            };

            m_Patterns = vector<PatternSet>(columnPatterns,
                columnPatterns +
                (sizeof(columnPatterns)/sizeof(columnPatterns[0])));
        }
        else
        {
            const PatternSet pattern = { 0, ~0U };
            m_Patterns.push_back(pattern);
        }
    }

    // Report allocated size
    {
        UINT64 size = 0;
        for (UINT32 ichunk=0; ichunk < numChunks; ichunk++)
        {
            size += GetChunkDesc(ichunk).size;
        }
        Printf(GetVerbosePrintPri(),
               "LwdaMats allocated 0x%llX bytes (%.1f MB) for testing\n",
               size, size/(1024.0*1024.0));
    }

    const bool quitOnError = GetGoldelwalues()->GetStopOnError();

    // Test subsequent patterns
    size_t ipattern = 0;
    size_t ipatternEnd = m_Patterns.size();

    // If we're shmooing (Test 187), then JS needs to select which patterns are
    // to be tested by specifying the pattern pair index, otherwise all patterns
    // will be tested on every loop
    if (m_ShmooPatternIdx != _UINT32_MAX)
    {
        if (m_ShmooPatternIdx >= m_Patterns.size())
        {
            Printf(Tee::PriError, "ShmooPatternIdx(%u) is larger than number of available pattern pairs (%zu)!\n", m_ShmooPatternIdx, m_Patterns.size());
            return RC::BAD_PARAMETER;
        }

        ipattern = m_ShmooPatternIdx;
        ipatternEnd = m_ShmooPatternIdx + 1;
    }

    // Add TestProgress. In Test 187, this is handled by JS.
    if (!m_ShmooEnabled)
    {
        if (m_ColumnTest)
        {
            PrintProgressInit(ipatternEnd);
        }
        else
        {
            PrintProgressInit(ipatternEnd * numChunks);
        }
    }

    for (; ipattern < ipatternEnd; ipattern++)
    {
        const PatternSet& pattern = m_Patterns[ipattern];

        if (m_ColumnTest)
        {
            CHECK_RC(LaunchLwdaColumnKernels(pattern.pattern1,
                                             pattern.pattern2,
                                             m_ColumnIlwStep, &corrupted));
            if (m_NumErrors)
            {
                if (!m_ShmooEnabled)
                {
                    PrintErrorUpdate(ipattern, RC::BAD_MEMORY);
                }
                if (quitOnError)
                {
                    Printf(Tee::PriError,
                           "LwdaColumns: Failure detected on pattern idx %zu. Quitting.\n",
                           ipattern);
                    GetJsonExit()->AddFailLoop(static_cast<UINT32>(ipattern));
                    break;
                }
            }

            if (!m_ShmooEnabled) // Repeat the run with row ilwersion disabled
            {
                CHECK_RC(LaunchLwdaColumnKernels(pattern.pattern1,
                                                 pattern.pattern2,
                                                 0, &corrupted));
                if (m_NumErrors)
                {
                    PrintErrorUpdate(ipattern, RC::BAD_MEMORY);
                    if (quitOnError)
                    {
                        Printf(Tee::PriError,
                               "LwdaColumns: Failure detected on pattern idx \
                                %zu repeat. Quitting.\n",
                               ipattern);
                        GetJsonExit()->AddFailLoop(static_cast<UINT32>(ipattern));
                        break;
                    }
                }
            }
            CHECK_RC(EndLoop(static_cast<UINT32>(ipattern)));
            if (!m_ShmooEnabled)
            {
                PrintProgressUpdate(ipattern);
            }
            continue;
        }

        // Test subsequent chunks
        bool testedSomething = false;
        for (UINT32 ichunk=0; ichunk < numChunks; ichunk++)
        {
            const Lwca::ClientMemory& lwdaChunk = GetLwdaChunk(ichunk);
            const UINT64 virt = lwdaChunk.GetDevicePtr();
            UINT64 physAddr;
            if (Xp::HasClientSideResman())
            {
                physAddr = VirtualToPhysical(virt);
            }
            else
            {
                // VirtualToPhysical uses LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR
                // RM call, which is only supported in client side RM. Since we
                // are interested only in the address of the beginning of the
                // chunk, we can simply retrieve it from the chunk's fbOffset.
                // Most likely we could substitute VirtualToPhysical above with
                // fbOffset in all cases. However restricting it only to kernel
                // side RM, since I (vandrejev) couldn't get information why
                // LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR was used in the first
                // place instead of fbOffset.
                physAddr = GetChunkDesc(ichunk).fbOffset;
            }

            // Limit test to the current range
            UINT64 startOffset;
            UINT64 endOffset;
            if (!GetTestRange(physAddr, lwdaChunk.GetSize(),
                              &startOffset, &endOffset))
            {
                continue;
            }
            if (startOffset >= endOffset)
            {
                Printf(Tee::PriNormal,
                       "ERROR: Unsupported test range %08llX to %08llX.\n",
                       physAddr+startOffset,
                       physAddr+endOffset);
                return RC::ILWALID_RAM_AMOUNT;
            }
            UINT64 startDevPtr = virt + startOffset;
            const UINT64 endDevPtr = virt + endOffset;

            // Prepare memory
            {
                const LwdaMatsInput input =
                {
                    /*mem*/             startDevPtr,
                    /*memEnd*/          endDevPtr,
                    /*numIterations*/   0,
                    /*pattern1*/        pattern.pattern1,
                    /*pattern2*/        pattern.pattern2,
                    /*direction*/       ascending,
                    /*step*/            0,
                    /*ilwstep*/         0,
                    /*offset*/          0,
                    /*memAccessSize*/   m_MemAccessSize,
                    /*resultsSize*/     GetResultsSize(),
                    /*results*/         m_ResultsMem.GetDevicePtr()
                };
                CHECK_RC(m_InitLwdaMats.Launch(input));
            }

            // Perform the test in small steps
            const UINT32 numOuterIterations = 1;
            const UINT32 step = m_MemChunkSize * 1024*1024;
            while (startDevPtr < endDevPtr)
            {
                // Compute test range for the kernel
                const UINT64 nextStartDevPtr =
                    ((startDevPtr + step + step/4 > endDevPtr)
                     || (m_MemChunkSize == 0))
                        ? endDevPtr
                        : startDevPtr + step;

                // Callwlate number of iterations
                UINT32 numInnerIterations = m_Iterations;
                if (m_TestDir == both)
                {
                    numInnerIterations /= 2;
                }
                // Must be even, because first and every even iteration
                // expects pattern1 to be in memory.
                numInnerIterations += numInnerIterations & 1;

                // Prepare test parameters
                LwdaMatsInput input =
                {
                    /*mem*/             startDevPtr,
                    /*memEnd*/          nextStartDevPtr,
                    /*numIterations*/   numInnerIterations,
                    /*pattern1*/        pattern.pattern1,
                    /*pattern2*/        pattern.pattern2,
                    /*direction*/       static_cast<UINT32>(
                                            (m_TestDir == both) ? ascending : m_TestDir),
                    /*step*/            0,
                    /*ilwstep*/         0,
                    /*offset*/          0,
                    /*memAccessSize*/   m_MemAccessSize,
                    /*resultsSize*/     GetResultsSize(),
                    /*results*/         m_ResultsMem.GetDevicePtr()
                };

                // Printf test configuration
                const double memSize = (input.memEnd-input.mem)/(1024.0*1024.0);
                const char* direction = m_Direction.c_str();
                Printf(GetVerbosePrintPri(), "LwdaMats test configuration:\n"
                    "    kernel          %d.%d\n"
                    "    numBlocks       %u\n"
                    "    numThreads      %u\n"
                    "    mem             0x%08llX\n"
                    "    memEnd          0x%08llX (%.1fMB)\n"
                    "    numIterations   %u\n"
                    "    pattern1        0x%08X\n"
                    "    pattern2        0x%08X\n"
                    "    direction       %s\n"
                    "    resultsSize     %u\n"
                    "    results         0x%08llX\n",
                    GetBoundLwdaDevice().GetCapability().MajorVersion(),
                    GetBoundLwdaDevice().GetCapability().MinorVersion(),
                    (UINT32)m_NumBlocks, (UINT32)m_NumThreads,
                    input.mem,
                    input.memEnd,
                    memSize,
                    (UINT32)input.numIterations,
                    (UINT32)input.pattern1,
                    (UINT32)input.pattern2,
                    direction,
                    (UINT32)input.resultsSize,
                    input.results);
                Printf(GetVerbosePrintPri(),
                       "Running %u iterations on %.1fMB of memory in %s direction(s),"
                       " phys addr 0x%08llX..0x%08llX\n",
                    (input.numIterations * numOuterIterations *
                     ((m_TestDir == both) ? 2 : 1)),
                    memSize, direction,
                    physAddr+(startDevPtr-lwdaChunk.GetDevicePtr()),
                    physAddr+(nextStartDevPtr-lwdaChunk.GetDevicePtr()));

                // Trigger memory fault. This is a nop unless -emu_mem_trigger arg is used
                GetBoundGpuSubdevice()->EmuMemTrigger();

                // Execute test
                for (UINT32 iter=0; iter < numOuterIterations; iter++)
                {
                    CHECK_RC(m_LwdaMats.Launch(input));
                    testedSomething = true;
                }

                // Execute test in the other direction
                if (m_TestDir == both)
                {
                    input.direction = descending;
                    for (UINT32 iter=0; iter < numOuterIterations; iter++)
                    {
                        CHECK_RC(m_LwdaMats.Launch(input));
                    }
                }

                // Advance to next range
                startDevPtr = nextStartDevPtr;

                CHECK_RC(EndLoop(static_cast<UINT32>(ipattern)));
            }

            // Report errors
            vector<char, Lwca::HostMemoryAllocator<char> >
                results(GetResultsSize(),
                        char(),
                        Lwca::HostMemoryAllocator<char>(GetLwdaInstancePtr(),
                                                        GetBoundLwdaDevice()));
            CHECK_RC(m_ResultsMem.Get(&results[0], (UINT32)results.size()));
            CHECK_RC(GetLwdaInstance().Synchronize());
            CHECK_RC(ReportErrors(&results[0], &corrupted,
                                  static_cast<UINT32>(results.size()),
                                  lwdaChunk.GetSize()));
            if (!m_ShmooEnabled)
            {
                if (m_NumErrors)
                {
                    PrintErrorUpdate(ichunk + ipattern*numChunks, RC::BAD_MEMORY);
                }
                PrintProgressUpdate(ichunk + ipattern*numChunks);
            }
        }
        if (!testedSomething)
        {
            Printf(Tee::PriError,
                   "LwdaMats is unable to test the specified memory range.\n");
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
    }

    // Indicate that we have completed all loops
    if (!m_ShmooEnabled)
    {
        if (m_ColumnTest)
        {
            PrintProgressUpdate(ipatternEnd);
        }
        else
        {
            PrintProgressUpdate(ipatternEnd * numChunks);
        }
    }

    // Log all bad memory locations first
    for (const auto &err: m_ReportedErrors)
    {
        CHECK_RC(GetMemError(0).LogMemoryError(
                        m_MemAccessSize,
                        err.first.address,
                        err.first.actual,
                        err.first.expected,
                        err.first.reread1,
                        err.first.reread2,
                        err.first.pteKind,
                        err.first.pageSizeKB,
                        *err.second.begin(),
                        "",
                        0));
    }

    // Log repeated memory failures
    for (const auto &err: m_ReportedErrors)
    {
        bool isFirstTimeStamp = true;
        for (const auto timeStamp: err.second)
        {
            if (!isFirstTimeStamp)
            {
                CHECK_RC(GetMemError(0).LogMemoryError(
                                m_MemAccessSize,
                                err.first.address,
                                err.first.actual,
                                err.first.expected,
                                err.first.reread1,
                                err.first.reread2,
                                err.first.pteKind,
                                err.first.pageSizeKB,
                                timeStamp,
                                "",
                                0));
            }
            isFirstTimeStamp = false;
        }
    }

    if (corrupted)
    {
        Printf(Tee::PriNormal, "The results may be irreliable because the memory which holds them was corrupted.\n");
    }
    return corrupted ? RC::BAD_MEMORY : OK;
}

bool LwdaMatsTest::SelectDevicePointers
(
    UINT32 ichunk,
    LwdaMatsInput *input
)
{
    const Lwca::ClientMemory& lwdaChunk = GetLwdaChunk(ichunk);
    UINT32 rowStartAddrAdjust = 0;
    UINT64 virt = lwdaChunk.GetDevicePtr();

    UINT64 physAddr = VirtualToPhysical(virt);
    UINT64 startOffset;
    UINT64 endOffset;
    if (!GetTestRange(physAddr, lwdaChunk.GetSize(), &startOffset, &endOffset))
    {
        return false;
    }

    if (input->ilwstep != 0)
    {
        UINT32 ilwStepBytes = 4*input->ilwstep;
        rowStartAddrAdjust =
            ilwStepBytes - UINT32(virt % ilwStepBytes);
        rowStartAddrAdjust %= ilwStepBytes;
        startOffset += rowStartAddrAdjust;
    }

    if (startOffset >= endOffset)
    {
        Printf(Tee::PriWarn,
               "ERROR: Unsupported test range %08llX to %08llX.\n",
               physAddr+startOffset,
               physAddr+endOffset);
        return false;
    }

    input->mem = virt + startOffset;
    input->memEnd = virt + endOffset;

    // Is there anything to test:
    return input->memEnd > input->mem;
}

RC LwdaMatsTest::LaunchLwdaColumnKernel
(
    UINT32 offsetIdx,
    UINT32 rdPattern,
    UINT32 wrPattern,
    LwdaMatsInput *input
)
{
    RC rc;

    input->offset = offsetIdx;
    input->pattern1 = rdPattern;
    input->pattern2 = wrPattern;

    // Trigger memory fault. This is a nop unless -emu_mem_trigger arg is used
    GetBoundGpuSubdevice()->EmuMemTrigger();

    for (UINT32 ichunk = 0; ichunk < NumChunks(); ichunk++)
    {
        if (!SelectDevicePointers(ichunk, input))
            continue;

        if (!m_MemChunkSize)
        {
            CHECK_RC(m_LwdaColumns.Launch(*input));
        }
        else
        {
            const UINT32 chunkStep = m_MemChunkSize * 1024*1024;
            const UINT64 chunkEnd = input->memEnd;
            input->memEnd = input->mem;
            do
            {
                input->memEnd = min(input->memEnd + chunkStep, chunkEnd);

                CHECK_RC(m_LwdaColumns.Launch(*input));
                if (m_ColumnTestMemChunkSleep > 0)
                {
                    CHECK_RC(GetLwdaInstance().Synchronize());
                    Tasker::Sleep(m_ColumnTestMemChunkSleep);
                }

                input->mem += chunkStep;
            } while (input->mem < chunkEnd);
        }
    }

    if (m_ColumnSleep > 0)
    {
        CHECK_RC(GetLwdaInstance().Synchronize());
        Tasker::Sleep(m_ColumnSleep);
    }

    return rc;
}

RC LwdaMatsTest::LaunchLwdaColumnKernels
(
    UINT32 pattern1,
    UINT32 pattern2,
    UINT32 columnIlwStep,
    bool *pCorrupted // Set true if mem errors oclwred, else retain old value
)
{
    RC rc;
    LwdaMatsInput input =
    {
        /*mem*/             0,
        /*memEnd*/          0,
        /*numIterations*/   0,
        /*pattern1*/        pattern1,
        /*pattern2*/        pattern2,
        /*direction*/       0,
        /*step*/            m_ColumnStep,
        /*ilwstep*/         columnIlwStep/4,
        /*offset*/          0,
        /*memAccessSize*/   m_MemAccessSize,
        /*resultsSize*/     GetResultsSize(),
        /*results*/         m_ResultsMem.GetDevicePtr()
    };

    Printf(GetVerbosePrintPri(), "LwdaColumn test configuration:\n"
        "    pattern1        0x%08X\n"
        "    pattern2        0x%08X\n"
        "    iterations      %u\n"
        "    sleep           %u ms\n"
        "    column step     %u\n"
        "    column ilw step %u\n",
        (UINT32)pattern1,
        (UINT32)pattern2,
        m_Iterations,
        m_ColumnSleep,
        m_ColumnStep,
        columnIlwStep);

    if (m_ColumnToTest != _UINT32_MAX)
    {
        Printf(GetVerbosePrintPri(), "    column to test  %u\n", m_ColumnToTest);
    }
    if (m_MemChunkSize)
    {
        Printf(GetVerbosePrintPri(), "    max chunk size  %u\n", m_ColumnToTest);
        Printf(GetVerbosePrintPri(), "    chunk sleep     %ums\n", m_ColumnTestMemChunkSleep);
    }

    bool nothingTested = true;
    UINT64 totalSize = 0;
    for (UINT32 ichunk=0; ichunk < NumChunks(); ichunk++)
    {
        if (!SelectDevicePointers(ichunk, &input))
            continue;

        nothingTested = false;
        const Lwca::ClientMemory& lwdaChunk = GetLwdaChunk(ichunk);
        const UINT64 virt = lwdaChunk.GetDevicePtr();
        const UINT64 physAddr = VirtualToPhysical(virt);
        totalSize += lwdaChunk.GetSize();

        const double memSize = (input.memEnd-input.mem)/(1024.0f*1024.0f);
        Printf(GetVerbosePrintPri(), "Running on %6.1fMB of memory,"
            " phys addr 0x%08llX..0x%08llX\n",
            memSize,
            physAddr + (input.mem - virt),
            physAddr + (input.memEnd - virt));

        CHECK_RC(m_InitLwdaMats.Launch(input));
    }

    if (nothingTested)
    {
        Printf(Tee::PriWarn, "ERROR: All allocated memory chunks were skipped, therefore nothing was tested.\n");
        return RC::ILWALID_RAM_AMOUNT;
    }

    // If a specific column was requested for testing, then limit the for-loop
    UINT32 minOffset = 0;
    UINT32 maxOffset = m_ColumnStep;
    if (m_ColumnToTest < m_ColumnStep)
    {
        minOffset = m_ColumnToTest;
        maxOffset = m_ColumnToTest + 1;
    }

    for (UINT32 iteration = 0; iteration < m_Iterations; iteration++)
    {
        for (UINT32 offsetIdx = minOffset; offsetIdx < maxOffset; offsetIdx++)
        {
            // Memory has already been scrubed with pattern1, so now we want to
            // read pattern1, and write pattern2
            CHECK_RC(LaunchLwdaColumnKernel(offsetIdx, pattern1, pattern2, &input));

            // The last LaunchLwdaColumnKernel wrote pattern2, so now we want to
            // read pattern2, and write pattern1
            CHECK_RC(LaunchLwdaColumnKernel(offsetIdx, pattern2, pattern1, &input));
        }
    }

    // Report errors
    vector<char, Lwca::HostMemoryAllocator<char> >
        results(GetResultsSize(),
                char(),
                Lwca::HostMemoryAllocator<char>(GetLwdaInstancePtr(),
                                                GetBoundLwdaDevice()));
    CHECK_RC(m_ResultsMem.Get(&results[0], 0, (UINT32)results.size()));
    CHECK_RC(GetLwdaInstance().Synchronize());
    CHECK_RC(ReportErrors(&results[0], pCorrupted,
                          static_cast<UINT32>(results.size()), totalSize));

    return rc;
}

RC LwdaMatsTest::ReportErrors
(
    const char* results,
    bool* pCorrupted, // Set true if mem errors oclwred, else retain old value
    size_t resultsSize,
    UINT64 testedMemSize
)
{
    MASSERT(results != nullptr);
    MASSERT(pCorrupted != nullptr);
    const size_t errBlockSize = resultsSize;
    const UINT32 numErrBlocks = 1;
    RC rc;

    UINT32 numCorruptions = 0;
    const UINT32 maxCorruptions = 10;
    UINT64 totalNumErrors = 0;

    for (UINT32 ithread=0; ithread < numErrBlocks; ithread++)
    {
        const RangeErrors* const errors = reinterpret_cast<const RangeErrors*>(
                &results[ithread*errBlockSize]);

        // Validate errors
        if ((errors->numReportedErrors > m_ActualMaxErrorsPerBlock)
             || (errors->numReportedErrors > errors->numErrors))
        {
            numCorruptions++;
            if (numCorruptions <= maxCorruptions)
            {
                Printf(Tee::PriNormal,
                        "Error results in block %u are corrupted (%u reported errors?!)\n",
                        ithread, errors->numReportedErrors);
            }
            continue;
        }
        if (errors->numReportedErrors >
            std::min<UINT64>(m_ActualMaxErrorsPerBlock, errors->numErrors))
        {
            numCorruptions++;
            if (numCorruptions <= maxCorruptions)
            {
                Printf(Tee::PriNormal,
                        "Error results in block %u are corrupted (%u reported errors with %u max, but %llu total errors?!)\n",
                        ithread, errors->numReportedErrors,
                        m_ActualMaxErrorsPerBlock, errors->numErrors);
            }
            continue;
        }
        if ((errors->numErrors > 0) && (errors->badBits == 0))
        {
            numCorruptions++;
            if (numCorruptions <= maxCorruptions)
            {
                Printf(Tee::PriNormal,
                        "Error results in block %u are corrupted (%llu total errors, but bad bits 0x%0*llx?!)\n",
                        ithread, errors->numErrors, m_MemAccessSize/4, errors->badBits);
            }
            continue;
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
                            "Error results in block %u are corrupted (actual value 0x%0*llx the same as expected?!)\n",
                            ithread, m_MemAccessSize/4, badValue.actual);
                }
                badValueCorrupted = true;
                break;
            }
        }
        if (badValueCorrupted)
        {
            continue;
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

            // Find a chunk that the address belongs to:
            const UINT32 ichunk = FindChunkByVirtualAddress(badValue.address);
            if (ichunk == ~0U)
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
            const GpuUtility::MemoryChunkDesc& chunk = GetChunkDesc(ichunk);

            ReportedError error;
            error.address = VirtualToPhysical(badValue.address);
            error.actual = badValue.actual;
            error.expected = badValue.expected;
            error.reread1 = badValue.reread1;
            error.reread2 = badValue.reread2;
            error.pteKind = chunk.pteKind;
            error.pageSizeKB = chunk.pageSizeKB;
            m_ReportedErrors[error].insert(badValue.timestamp);
        }
    }

    bool tooManyErrors = false;
    if (m_ColumnTest)
    {
        tooManyErrors = (totalNumErrors >
                         (2* (testedMemSize/4) * m_Iterations));
    }
    else
    {
        UINT64 allowedNumErrors = testedMemSize * m_Iterations / 4;
        if (m_TestDir == both)
        {
            allowedNumErrors *= 2;
        }
        tooManyErrors = totalNumErrors > allowedNumErrors;
    }

    // Validate total number of errors
    if (tooManyErrors)
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
            m_NumErrors = static_cast<UINT32>(totalNumErrors);
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
