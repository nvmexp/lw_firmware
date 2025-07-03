/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdamemtest.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/jscript.h"
#include "gpu/include/gpudev.h"
#include "random.h"
#include "gpu/utility/mapfb.h"
#include "lwdawrap.h"
#include "core/include/utility.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/tests/lwca/mats/lwdamext.h"
#include "core/include/jsonlog.h"

using GpuUtility::MemoryChunkDesc;
using GpuUtility::MemoryChunks;

//------------------------------------------------------------------------------
//! \brief New LWCA-based MATS Memory Test
//!
//! MATS is short for Modified Algorithmic Test Sequence
//!
//! This is a new implementation of the existing MATS infrastructure that
//! exelwtes its test sequences as a LWCA kernel.
//------------------------------------------------------------------------------
class LwdaMatsExtTest : public LwdaMemTest
{
public:
    LwdaMatsExtTest();
    virtual ~LwdaMatsExtTest();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // JavaScript property accessors
    SETGET_PROP(ChunkAlignment,  UINT32);
    SETGET_PROP(ChunksPerThread, UINT32);
    SETGET_PROP(ErrorMemory,     UINT32);
    SETGET_PROP(BoundMaxBytes,   UINT32);
    SETGET_PROP(BoundMaxBoxes,   UINT32);
    SETGET_PROP(Iterations,      UINT32);
    SETGET_PROP(Coverage,        FLOAT64);
    SETGET_PROP(MaxChunkSize,    UINT32);
    SETGET_PROP(UseLdg,          bool);
    SETGET_JSARRAY_PROP_LWSTOM(PatternTable);

private:
    RC PrepareMemoryForTest();
    RC SequenceToConstant(const MemoryCommands &src, Lwca::Global *dest);
    RC DoMatsTest();
    RC ExelwteLinearTest(KernelInput &input, int threads, int blocks);
    RC ExelwteBoxTest(KernelInput &input, int threads, int blocks);
    RC LogMemoryErrors(bool *pCorrupted, UINT64 offset, UINT32 threads,
                       UINT32 errors, UINT32 pteKind, UINT32 pageSizeKB);

    // JavaScript properties
    UINT32 m_ChunkAlignment;
    UINT32 m_ChunksPerThread;
    UINT32 m_ErrorMemory;
    UINT32 m_BoundMaxBytes;
    UINT32 m_BoundMaxBoxes;
    UINT32 m_Iterations;
    FLOAT64 m_Coverage;
    UINT32 m_MaxChunkSize;
    bool    m_UseLdg;
    JsArray m_PatternTable;
    JsArray m_TestCommands;

    // Internal properties
    TestConfiguration *m_pTestConfig;
    Random *m_pRandom;
    MemoryRanges m_TestRanges;
    CommandSequences m_Sequences;
    MatsHelper m_MatsHelper;
    bool m_IsBoxTest;

    UINT32 m_Pitch;

    Lwca::Module m_Module;
    Lwca::Function m_pLinearTest;
    Lwca::Function m_pBoxTest;
    Lwca::Event m_BoxEvent;
    Lwca::DeviceMemory m_OutputMem;
    Lwca::HostMemory m_OutputBuf;
    Lwca::HostMemory m_CommandBuf;
    Lwca::HostMemory m_BoxBuf;

    enum
    {
        ProcessorCores    = 8,
        MinChunkSize      = 128 * 1024,
        CommandBufferSize = sizeof(CommandData) * CONST_MAX_COMMAND,
        BoxBufferSize     = sizeof(BoxData) * CONST_MAX_BOXES
    };
};

#define LWDAMATSEXT_CONST_PROPERTY(name, init, help)  \
    static SProperty Display_##name                   \
    (                                                 \
        LwdaMatsExtTest_Object,                       \
        #name,                                        \
        0,                                            \
        init,                                         \
        0,                                            \
        0,                                            \
        JSPROP_READONLY,                              \
        "CONSTANT - " help                            \
    )

JS_CLASS_INHERIT(LwdaMatsExtTest, LwdaMemTest, "New LWCA-based MATS");

CLASS_PROP_READWRITE(LwdaMatsExtTest, ChunkAlignment, UINT32,
        "Size, in bytes, of each chunk.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, ChunksPerThread, UINT32,
        "Maximum number of chunks to process per hardware thread.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, ErrorMemory, UINT32,
        "Amount of memory reserved for error recording, in bytes.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, BoundMaxBytes, UINT32,
        "Maximum value for bounded linear memory tests.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, BoundMaxBoxes, UINT32,
        "Maximum value for bounded box memory tests.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, Iterations, UINT32,
        "Number of iterations to perform per run.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, Coverage, FLOAT64,
        "Percent of framebuffer to test for box tests.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, MaxChunkSize, UINT32,
        "Maximum number of bytes tested at once.");
CLASS_PROP_READWRITE(LwdaMatsExtTest, UseLdg, bool,
       "Use LDG (read via tex-cache) for read-before-write in linear tests.");
CLASS_PROP_READWRITE_JSARRAY(LwdaMatsExtTest, PatternTable, JsArray,
        "Set pattern table for TestComamnd(s) to index into");

LWDAMATSEXT_CONST_PROPERTY(LinearNormal, MEMFMT_LINEAR,
                       "Linear memory pattern type.");
LWDAMATSEXT_CONST_PROPERTY(LinearBounded, MEMFMT_LINEAR_BOUNDED,
                       "Linear memory pattern type with upper bound limit.");
LWDAMATSEXT_CONST_PROPERTY(BoxNormal, MEMFMT_BOX,
                       "Box memory pattern type.");
LWDAMATSEXT_CONST_PROPERTY(BoxBounded, MEMFMT_BOX_BOUNDED,
                       "Box memory pattern type with upper bound limit.");
LWDAMATSEXT_CONST_PROPERTY(MemoryUp, MEMDIR_UP,
                       "Traverse from low to high memory.");
LWDAMATSEXT_CONST_PROPERTY(MemoryDown, MEMDIR_DOWN,
                       "Traverse from high to low memory.");

RC LwdaMatsExtTest::GetPatternTable(JsArray *val) const
{
    MASSERT(val != NULL);
    *val = m_PatternTable;
    return OK;
}

RC LwdaMatsExtTest::SetPatternTable(JsArray *val)
{
    MASSERT(val != NULL);
    m_PatternTable = *val;
    return OK;
}

LwdaMatsExtTest::LwdaMatsExtTest()
: m_ChunkAlignment(static_cast<UINT32>(1_MB))
, m_ChunksPerThread(0)
, m_ErrorMemory(static_cast<UINT32>(1_MB))
, m_BoundMaxBytes(0)
, m_BoundMaxBoxes(0)
, m_Iterations(0)
, m_Coverage(100)
, m_MaxChunkSize(0)
, m_UseLdg(false)
, m_IsBoxTest(false)
, m_Pitch(0)
{
    SetName("LwdaMatsExtTest");
    m_pTestConfig = GetTestConfiguration();
    m_pRandom = &(GetFpContext()->Rand);
}

LwdaMatsExtTest::~LwdaMatsExtTest()
{
}

RC LwdaMatsExtTest::InitFromJs()
{
    RC rc = OK;

    CHECK_RC(LwdaMemTest::InitFromJs());

    m_pRandom->SeedRandom(m_pTestConfig->Seed());

    if ((m_Coverage < 1) || m_Coverage > 100)
    {
        Printf(Tee::PriNormal, "MatsTest.Coverage must be a value from 1 to "
               "100, value is %.1f.\n", m_Coverage);
        return RC::SOFTWARE_ERROR;
    }

    if (!m_ChunkAlignment)
    {
        Printf(Tee::PriNormal, "Chunk alignment must be >= 1 byte in size.\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_MaxChunkSize == 0)
    {
        m_MaxChunkSize = 64*1024*1024;
    }

    // Retrieve the test commands in InitFromJs rather than a normal Set/Get
    // function since copying a complex JS array (i.e. one whose elements
    // are not basic types) can result in the individual element being garbage
    // collected before they are actually parsed.  Parsing TestCommands also
    // depends on parsing of Patterns and therefore cannot be parsed directly
    // in the handler
    //
    // Specifically ignore the return code here since it will fail if not
    // present
    JavaScriptPtr()->GetProperty(GetJSObject(), "TestCommands", &m_TestCommands);
    return rc;
}

void LwdaMatsExtTest::PrintJsProperties(Tee::Priority pri)
{
    LwdaMemTest::PrintJsProperties(pri);

    Printf(pri, "\tChunkAlignment:\t\t\t%d\n",  m_ChunkAlignment);
    Printf(pri, "\tChunksPerThread:\t\t%d\n",   m_ChunksPerThread);
    Printf(pri, "\tErrorMemory:\t\t\t%d\n",     m_ErrorMemory);
    Printf(pri, "\tBoundMaxBytes:\t\t\t%d\n",   m_BoundMaxBytes);
    Printf(pri, "\tBoundMaxBoxes:\t\t\t%d\n",   m_BoundMaxBoxes);
    Printf(pri, "\tIterations:\t\t\t%d\n",      m_Iterations);
    Printf(pri, "\tCoverage:\t\t\t%.1f\n",      m_Coverage);
    Printf(pri, "\tMaxChunkSize:\t\t\t%u\n",    m_MaxChunkSize);
    Printf(pri, "\tUseLdg:\t\t\t\t%s\n",        m_UseLdg ? "true" : "false");
}

bool LwdaMatsExtTest::IsSupported()
{
    // For now we don't support this test on CheetAh
    if (GetBoundGpuSubdevice()->IsSOC())
        return false;

    return LwdaMemTest::IsSupported();
}

RC LwdaMatsExtTest::Setup()
{
    RC rc;

    CHECK_RC(LwdaMemTest::Setup());

    CHECK_RC(LwdaMemTest::GetDisplayPitch(&m_Pitch));

    // Parse the command sequences from the JS file
    m_MatsHelper.Reset();
    m_Sequences.clear();

    CHECK_RC(m_MatsHelper.ParseTable(m_PatternTable));
    CHECK_RC(m_MatsHelper.ParseSequenceList(m_TestCommands, &m_Sequences));

    // Send the LwdaMatsExt program to device memory and prepare each kernel
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("lwmext", &m_Module));

    m_pLinearTest = m_Module.GetFunction("LinearTest");
    m_pBoxTest    = m_Module.GetFunction("BoxTest");

    m_BoxEvent = GetLwdaInstance().CreateEvent();

    CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_ErrorMemory, &m_OutputMem));
    CHECK_RC(AllocateEntireFramebuffer());
    CHECK_RC(PrepareMemoryForTest());

    // allocate page-locked host memory
    CHECK_RC(GetLwdaInstance().AllocHostMem(m_ErrorMemory, &m_OutputBuf));
    CHECK_RC(GetLwdaInstance().AllocHostMem(CommandBufferSize, &m_CommandBuf));
    CHECK_RC(GetLwdaInstance().AllocHostMem(BoxBufferSize, &m_BoxBuf));

    CHECK_RC(DisplayAnyChunk());
    return OK;
}

RC LwdaMatsExtTest::Run()
{
    RC rc;

    for (UINT32 iter = 0; iter < m_Iterations; ++iter)
        CHECK_RC(DoMatsTest());

    return rc;
}

RC LwdaMatsExtTest::Cleanup()
{
    // Free the memory allocated to m_TestRanges
    MemoryRanges dummy1;
    m_TestRanges.swap(dummy1);

    m_BoxBuf.Free();
    m_OutputBuf.Free();
    m_CommandBuf.Free();

    m_OutputMem.Free();

    m_BoxEvent.Free();
    m_Module.Unload();

    return LwdaMemTest::Cleanup();
}

RC LwdaMatsExtTest::PrepareMemoryForTest()
{
    const UINT64 range_min = static_cast<UINT64>(GetStartLocation()) * 1_MB;
    UINT64 range_max = static_cast<UINT64>(GetEndLocation()) * 1_MB;

    // If ending range is not specified, assume maximum memory
    if (!range_max)
        range_max = ~range_max;

    m_TestRanges.clear();

    for (UINT32 chunk=0; chunk < NumChunks(); chunk++)
    {
        const UINT64 virt       = GetLwdaChunk(chunk).GetDevicePtr();
        const UINT64 mem_phys   = VirtualToPhysical(virt);
        const UINT64 mem_size   = GetChunkDesc(chunk).size;
        const UINT64 mem_offset = virt - mem_phys;

        // add to testable memory ranges
        MemoryRange range;
        range.StartOffset = max(range_min, mem_phys) + mem_offset;
        range.EndOffset   = min(range_max, mem_phys + mem_size) + mem_offset;

        // align to megabyte boundaries
        range.StartOffset = ALIGN_UP(range.StartOffset, 1_MB);
        range.EndOffset   = ALIGN_DOWN(range.EndOffset, 1_MB);

        if ((range.StartOffset > range.EndOffset) || (mem_phys > range_max))
        {
            Printf(Tee::PriLow, "Skipping memory between addresses:\n"
                   "  Physical Start: 0x%08llX\n"
                   "  Physical End:   0x%08llX\n"
                   "  Aligned Start:  0x%08llX\n"
                   "  Aligned End:    0x%08llX\n",
                   mem_phys, mem_phys + mem_size,
                   range.StartOffset, range.EndOffset);
            continue;
        }

        Printf(Tee::PriLow, "LwdaMatsExt phys test range: 0x%llx..0x%llx\n",
                range.StartOffset-mem_offset, range.EndOffset-mem_offset);
        m_TestRanges.push_back(range);
    }

    if (m_TestRanges.empty())
    {
        Printf(Tee::PriNormal,
               "Error: Unable to allocate region of FB between %d and %d MB.\n",
               GetStartLocation(), GetEndLocation());

        return RC::ILWALID_RAM_AMOUNT;
    }

    // Check if this is a box test
    m_IsBoxTest = false;
    for (CommandSequences::const_iterator seq=m_Sequences.begin();
         seq != m_Sequences.end(); ++seq)
    {
        const bool isBoxSeq = (seq->mem_fmt == MEMFMT_BOX) ||
            (seq->mem_fmt == MEMFMT_BOX_BOUNDED);
        if (seq != m_Sequences.begin())
        {
            if (m_IsBoxTest != isBoxSeq)
            {
                Printf(Tee::PriError,
                       "Invalid test configuration! All sequences must be of the same type.\n");
                MASSERT(m_IsBoxTest == isBoxSeq);
                return RC::SOFTWARE_ERROR;
            }
        }
        m_IsBoxTest = isBoxSeq;
    }

    // Init boxes if this is a box test
    m_MatsHelper.Reset();
    if (m_IsBoxTest)
    {
        RC rc;
        CHECK_RC(m_MatsHelper.InitBoxes(m_TestRanges, m_pRandom,
                    m_Pitch, m_Coverage));
    }
    return OK;
}

RC LwdaMatsExtTest::SequenceToConstant
(
    const MemoryCommands &src,
    Lwca::Global *pDest
)
{
    RC rc;
    CommandData *buf_ptr = (CommandData *)m_CommandBuf.GetPtr();

    MASSERT(pDest != nullptr);
    if (CONST_MAX_COMMAND <= src.size())
    {
        Printf(Tee::PriError, "Command count exceeds constant memory size.");
        return RC::BAD_PARAMETER;
    }

    int count = 0;
    for (MemoryCommands::const_iterator lwr_cmd = src.begin();
         lwr_cmd != src.end(); ++lwr_cmd)
    {
        buf_ptr[count].value     = lwr_cmd->value;
        buf_ptr[count].op_type   = lwr_cmd->op_type;
        buf_ptr[count].op_mode   = lwr_cmd->op_mode;
        buf_ptr[count].op_size   = lwr_cmd->op_size;
        buf_ptr[count].increment = lwr_cmd->increment ? 1 : 0;
        buf_ptr[count].runflag   = lwr_cmd->runflag;
        ++count;
    }

    CHECK_RC(pDest->Set(buf_ptr, sizeof(CommandData) * count));
    return OK;
}

RC LwdaMatsExtTest::DoMatsTest()
{
    RC rc;

    // Determine the capabilities of the device under testing
    INT32 num_mp = GetBoundLwdaDevice().GetShaderCount();
    MASSERT(num_mp > 0);

    Lwca::Global pCommandConst = m_Module.GetGlobal("CommandBuffer");

    UINT32 iter = 0;
    for (UINT32 chunk=0; chunk < NumChunks(); chunk++)
    {
        const Lwca::ClientMemory& lwdaChunk = GetLwdaChunk(chunk);
        GpuUtility::MemoryChunkDesc& memChunk = GetChunkDesc(chunk);

        // If there is no box test in the sequence, divide tested chunks
        // into smaller ones to avoid RC timeouts.
        const UINT64 maxChunkSize =
            m_IsBoxTest ? memChunk.size : m_MaxChunkSize;
        UINT64 chunkSize = 0;

        for (UINT64 chunkOffs = 0;
             chunkOffs < memChunk.size; chunkOffs += chunkSize)
        {
            chunkSize = min<UINT64>(maxChunkSize, memChunk.size - chunkOffs);

            const UINT64 virt        = lwdaChunk.GetDevicePtr();
            const UINT64 addr_offset = VirtualToPhysical(virt) - virt;

            // Provide both aligned and absolute addresses - the first and last
            // threads may operate on memory outside of the aligned region
            UINT64 addr_first = virt + chunkOffs;
            UINT64 addr_last  = chunkSize + addr_first;

            KernelInput input;
            input.output    = m_OutputMem.GetDevicePtr();
            input.mem_start = ALIGN_DOWN(addr_first, m_ChunkAlignment);
            input.mem_end   = ALIGN_UP(addr_last, m_ChunkAlignment);
            input.resx      = m_Pitch / 4;
            input.useLdg    = m_UseLdg;

            DisplayChunk(&memChunk, input.mem_start - virt);

            UINT32 mem_size   = (UINT32)(input.mem_end - input.mem_start);
            UINT32 num_blocks = num_mp * ProcessorCores;
            UINT32 num_threads;

            if (!m_ChunksPerThread)
            {
                // If chunks aren't specified, split memory based on
                // the maximum number of independent threads
                input.chunksize = ALIGN_UP(mem_size / num_blocks,
                                           m_ChunkAlignment);
                num_threads     = 1;
            }
            else
            {
                // Figure out how many threads to launch based on how
                // much memory is allocated to each individual thread
                input.chunksize = m_ChunksPerThread * m_ChunkAlignment;
                num_threads     = mem_size / (input.chunksize * num_blocks) + 1;
            }

            // Determine how many errors we can allow per thread,
            // based on how much space we reserved solely for error
            // recording
            INT32 max_threads = num_blocks * num_threads;
            INT32 max_errors  =
                (m_ErrorMemory - max_threads * sizeof(ThreadErrors))
                / (max_threads * sizeof(ErrorDescriptor));

            if (max_errors <= 0)
            {
                // There should be space for at least 1 error per thread
                Printf(Tee::PriNormal, "Error: ErrorMemory is not large enough "
                       "to store error reports for %d threads.\n", max_threads);
                return RC::BAD_PARAMETER;
            }

            input.maxerrors = max_errors;

            Printf(Tee::PriLow,
                   "LwdaMatsExt testing 0x%llx..0x%llx with %u blocks and %u threads\n",
                    addr_first, addr_last, num_blocks, num_threads);

            for (CommandSequences::iterator seq = m_Sequences.begin();
                 seq != m_Sequences.end(); ++seq)
            {
                // Clear out previous test results and prepare the
                // current sequence
                CHECK_RC(m_OutputMem.Clear());
                CHECK_RC(SequenceToConstant(seq->operations, &pCommandConst));

                input.commands  = (UINT32)seq->operations.size();
                input.runflags  = seq->runflags;
                input.memsize   = seq->memsize;
                input.ascending = seq->ascending ? 1 : 0;
                input.boxcount  = m_MatsHelper.GetBoxCount();
                input.mem_min   = addr_first;
                input.mem_max   = addr_last;

                // Limit the test coverage if sequence format is bounded
                if (seq->bounded)
                {
                    input.boxcount = min( input.boxcount, m_BoundMaxBoxes );
                    input.mem_max  = min( addr_last,
                                          addr_first + m_BoundMaxBytes );
                }

                if (m_IsBoxTest)
                    CHECK_RC(ExelwteBoxTest(input, num_threads, num_blocks));
                else
                    CHECK_RC(ExelwteLinearTest(input, num_threads, num_blocks));

                // Fetch output results and log any memory errors
                CHECK_RC(m_OutputMem.Get(&m_OutputBuf));
                CHECK_RC(GetLwdaInstance().Synchronize());
                bool foundErrors = false;
                CHECK_RC(LogMemoryErrors(&foundErrors, addr_offset,
                                         max_threads, max_errors,
                                         memChunk.pteKind,
                                         memChunk.pageSizeKB));
                if (foundErrors)
                {
                    if (GetGoldelwalues()->GetStopOnError())
                    {
                        return OK;
                    }
                    GetJsonExit()->AddFailLoop(iter);
                }

                iter++;

                CHECK_RC(EndLoop());
            }
        }

        if (m_IsBoxTest)
        {
            break;
        }
    }

    return OK;
}

RC LwdaMatsExtTest::ExelwteLinearTest
(
    KernelInput &input,
    int threads,
    int blocks
)
{
    RC rc;
    m_pLinearTest.SetBlockDim(threads);
    m_pLinearTest.SetGridDim(blocks);
    CHECK_RC(m_pLinearTest.Launch(input));
    return OK;
}

RC LwdaMatsExtTest::ExelwteBoxTest(KernelInput &input, int threads, int blocks)
{
    RC rc;
    Lwca::Global pBoxConst = m_Module.GetGlobal("BoxBuffer");
    BoxData *pBoxBuf   = (BoxData *)m_BoxBuf.GetPtr();
    UINT32 box_max     = input.boxcount;
    UINT32 box_counter = 0;

    m_pBoxTest.SetBlockDim(threads);
    m_pBoxTest.SetGridDim(blocks);

    while (box_counter < box_max)
    {
        UINT32 num_boxes = min( CONST_MAX_BOXES, box_max - box_counter );
        input.boxcount = num_boxes;

        // Wait for the previous box set to be copied to FB
        if (box_counter > 0)
        {
            CHECK_RC(m_BoxEvent.Synchronize());
        }

        // Send as many boxes to constant memory as possible for each iteration
        for (UINT32 i = 0; i < num_boxes; ++i)
        {
            int box_id = m_MatsHelper.GetBox(box_counter + i);
            pBoxBuf[i].offset = m_MatsHelper.GetBoxTlc(box_id);
        }

        // Send next box set to FB
        CHECK_RC(pBoxConst.Set(pBoxBuf, num_boxes * sizeof(BoxData)));
        CHECK_RC(m_BoxEvent.Record());

        CHECK_RC(m_pBoxTest.Launch(input));

        box_counter += num_boxes;
    }

    if (box_counter > 0)
    {
        CHECK_RC(m_BoxEvent.Synchronize());
    }

    return OK;
}

RC LwdaMatsExtTest::LogMemoryErrors
(
    bool  *pCorrupted, // Set true if mem errors oclwred, else retain old value
    UINT64 offset,
    UINT32 threads,
    UINT32 errors,
    UINT32 pteKind,
    UINT32 pageSizeKB
)
{
    UINT32 chksum = 0, num_read = 0, num_write = 0, num_error = 0, bad_crc = 0;
    UINT08 *output_ptr = (UINT08 *)m_OutputBuf.GetPtr();
    ThreadErrors *thrd_info;
    ErrorDescriptor *thrd_desc;
    RC rc;

    for (UINT32 i = 0; i < threads; i++)
    {
        thrd_info = (ThreadErrors *)output_ptr;
        thrd_desc = (ErrorDescriptor *)(output_ptr + sizeof(ThreadErrors));
        output_ptr += sizeof(ThreadErrors) + sizeof(ErrorDescriptor) * errors;

        num_read  += thrd_info->reads;
        num_write += thrd_info->writes;
        num_error += thrd_info->errors;

        for (UINT32 err = 0; err < errors; ++err)
        {
            if (!thrd_desc[err].bitcount && !thrd_desc[err].checksum)
                break;

            // Error descriptors must reside in the framebuffer to be copied
            // back to the host, and may be corrupted due to bad memory - report
            // the failure but drop the corrupted report data
            chksum = Checksum((UINT08 *)&thrd_desc[err], CHECKSUM_SIZE);
            if (chksum != thrd_desc[err].checksum)
            {
                ++bad_crc;
                continue;
            }

            const UINT64 address = thrd_desc[err].address + offset;
            CHECK_RC(GetMemError(0).LogMemoryError(
                            thrd_desc[err].bitcount, address,
                            thrd_desc[err].actual, thrd_desc[err].expected,
                            thrd_desc[err].read1,  thrd_desc[err].read2,
                            pteKind, pageSizeKB, MemError::NO_TIME_STAMP,
                            "", 0));
        }
    }

    const INT32 pri = (((num_error > 0) || (bad_crc > 0)) ?
                       Tee::PriWarn : Tee::PriLow);
    Printf(pri, "read (%8X) write (%8X) error (%8X) bad crc (%8X)\n",
            num_read, num_write, num_error, bad_crc);

    if (num_error > 0 || bad_crc > 0)
    {
        *pCorrupted = true;
    }
    return OK;
}
