/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file mats.cpp   Implements the MATS test.

#ifndef MATS_STANDALONE
#include "gpumtest.h"
#include "core/include/mgrmgr.h"
#include "core/include/jscript.h"
#include "core/include/gpu.h"
#include "core/include/framebuf.h"
#include "core/include/tee.h"
#include "core/include/script.h"
#include "core/tests/testconf.h"
#include "core/include/lwrm.h"
#include "core/include/massert.h"
#include "core/include/types.h"
#include "random.h"
#include "gpu/utility/gpuutils.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "core/utility/ptrnclss.h"
#include <stdio.h>
#include "gpu/utility/mapfb.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "matshelp.h"
#include "core/include/abort.h"
#endif

// Generates read and write memory routines for each mode of operation
// (immediate value, current address, ilwerse of current address)
//
// This method seems preferable to hand writing a routine for each possible
// memory size (4 sizes * 3 modes * 2 types = 24 routines)
#define MATS_DECLARE_MEMROUTINE(x)                                          \
RC MemCmd_WR_IMM_##x(const MemoryCommand &mem)                              \
{                                                                           \
    if (m_WrDelay != 0)                                                     \
        Platform::DelayNS(m_WrDelay);                                       \
    MEM_WR##x(mem.ptr->Virtual, static_cast<UINT##x>(mem.value));           \
    return OK;                                                              \
}                                                                           \
                                                                            \
RC MemCmd_WR_ADR_##x(const MemoryCommand &mem)                              \
{                                                                           \
    if (m_WrDelay != 0)                                                     \
        Platform::DelayNS(m_WrDelay);                                       \
    MEM_WR##x(mem.ptr->Virtual, static_cast<UINT##x>(mem.ptr->Offset));     \
    return OK;                                                              \
}                                                                           \
                                                                            \
RC MemCmd_WR_ILW_##x(const MemoryCommand &mem)                              \
{                                                                           \
    if (m_WrDelay != 0)                                                     \
        Platform::DelayNS(m_WrDelay);                                       \
    MEM_WR##x(mem.ptr->Virtual, static_cast<UINT##x>(~mem.ptr->Offset));    \
    return OK;                                                              \
}                                                                           \
                                                                            \
RC MemCmd_RD_IMM_##x(const MemoryCommand &mem)                              \
{                                                                           \
    RC rc;                                                                  \
    if (m_RdDelay != 0)                                                     \
        Platform::DelayNS(m_RdDelay);                                       \
    UINT##x value_read = MEM_RD##x(mem.ptr->Virtual);                       \
    UINT##x value_expect = static_cast<UINT##x>(mem.value);                 \
    if (value_read != value_expect)                                         \
        CHECK_RC(MemoryError(mem, value_read, value_expect));               \
    return OK;                                                              \
}                                                                           \
                                                                            \
RC MemCmd_RD_ADR_##x(const MemoryCommand &mem)                              \
{                                                                           \
    RC rc;                                                                  \
    if (m_RdDelay != 0)                                                     \
        Platform::DelayNS(m_RdDelay);                                       \
    UINT##x value_read = MEM_RD##x(mem.ptr->Virtual);                       \
    UINT##x value_expect = static_cast<UINT##x>(mem.ptr->Offset);           \
    if (value_read != value_expect)                                         \
        CHECK_RC(MemoryError(mem, value_read, value_expect));               \
    return OK;                                                              \
}                                                                           \
                                                                            \
RC MemCmd_RD_ILW_##x(const MemoryCommand &mem)                              \
{                                                                           \
    RC rc;                                                                  \
    if (m_RdDelay != 0)                                                     \
        Platform::DelayNS(m_RdDelay);                                       \
    UINT##x value_read = MEM_RD##x(mem.ptr->Virtual);                       \
    UINT##x value_expect = static_cast<UINT##x>(~mem.ptr->Offset);          \
    if (value_read != value_expect)                                         \
        CHECK_RC(MemoryError(mem, value_read, value_expect));               \
    return OK;                                                              \
}                                                                           \

// Build up table of member function pointers to be assigned to each command
#define MATS_DECLARE_MEMROUTINE_PTRS(x)                                     \
    &MatsTest::MemCmd_RD_IMM_##x,                                           \
    &MatsTest::MemCmd_WR_IMM_##x,                                           \
    &MatsTest::MemCmd_RD_ADR_##x,                                           \
    &MatsTest::MemCmd_WR_ADR_##x,                                           \
    &MatsTest::MemCmd_RD_ILW_##x,                                           \
    &MatsTest::MemCmd_WR_ILW_##x,                                           \
    &MatsTest::MemCmd_Noop,                                                 \
    &MatsTest::MemCmd_Noop

// Slightly less clunky looking method of calling a pointer to member function
#define CALL_THIS(x) ((this)->*(x))

static const INT32 STATUS_PIXELS = 24;
static const INT32 STATUS_BAR_WIDTH = 8;

//------------------------------------------------------------------------------
//! \brief MATS Memory Test.
//!
//! MATS is short for Modified Algorithmic Test Sequence
//!
//! Based on an algorithm described in "March tests for word-oriented
//! memories" by A.J. van de Goor, Delft University, the Netherlands
class MatsTest : public GpuMemTest
{
public:
    MatsTest();
    virtual ~MatsTest();

    virtual bool IsSupportedVf();
    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

#ifndef MATS_STANDALONE
private:
#endif

    RC FillPatternRgb();

    //--------------------------------------------------------------------------
    // Javascript properties
    //--------------------------------------------------------------------------
    Random * m_pRandom;
    MapFb m_MapFb;

    //--------------------------------------------------------------------------
    // Private properties that are not private in stand-alone mats.exe
    //--------------------------------------------------------------------------

    UINT64 m_Iterations;
    UINT32 m_DataWidth;
    UINT32 m_Unit; // TODO: Hook back up to JS like in earlier trees
    FLOAT64 m_Coverage;
    UINT32 m_UpperBound;
    bool m_ShowStatus;
    bool m_DisableHostClockSlowdown;

    JsArray m_PatternTable;
    JsArray m_TestCommands;
    MemoryRanges m_TestRanges;
    CommandSequences m_Sequences;
    MatsHelper m_MatsHelper;

    // Second use of private.  Everything below here is private in both versions
    // of mats.
private:
    //--------------------------------------------------------------------------
    // Private properties
    //--------------------------------------------------------------------------

    TestConfiguration * m_pTestConfig;

    UINT32 m_Pitch;
    UINT32 m_ResX;// set to = pitch in pixels (m_Pitch / 4)
    UINT32 m_Pattern;
    UINT64 m_ImageOffset;

    UINT32 m_WrDelay;
    UINT32 m_RdDelay;

    UINT32 m_YieldCounter;
    UINT32 m_DrawCounter;
    INT32  m_BarPosition;
    bool   m_OldHostClockSlowdownDisabled;
    UINT32 m_RuntimeMs;                     //!< Fixed test run time

    PointerParams m_RdPtr;
    PointerParams m_WrPtr;

    GpuUtility::MemoryChunkDesc *GetChunkDesc(UINT64 offset);

    MATS_DECLARE_MEMROUTINE(08);
    MATS_DECLARE_MEMROUTINE(16);
    MATS_DECLARE_MEMROUTINE(32);
    MATS_DECLARE_MEMROUTINE(64);

    RC   MemCmd_Noop(const MemoryCommand &cmd);
    void SetCommandParams(MemoryCommands *cmd);
    RC   DoIdleTasks();
    bool TestRuntimeExceeded(UINT64 startTimeMs) const;

    RC MemoryError(const MemoryCommand &mem, UINT08 read, UINT08 expect);
    RC MemoryError(const MemoryCommand &mem, UINT16 read, UINT16 expect);
    RC MemoryError(const MemoryCommand &mem, UINT32 read, UINT32 expect);
    RC MemoryError(const MemoryCommand &mem, UINT64 read, UINT64 expect);

    RC RunLinearPattern (const MemoryRanges &testRanges, CommandSequence &seq);
    RC RunBoxPattern(const MemoryRanges &testRanges, CommandSequence &seq);

    volatile UINT08 *MapBoxRegion(UINT32 box);
    volatile UINT08 *MapAhead(UINT64 offset, UINT32 size, UINT64 maxMem);
    volatile UINT08 *MapBehind(UINT64 offset, UINT32 size, UINT64 minMem);

public:

    SETGET_PROP_LWSTOM(DataWidth,     UINT32);
    SETGET_PROP(Coverage,             FLOAT64);
    SETGET_PROP(ShowStatus,           bool);
    SETGET_PROP(Iterations,           UINT64);
    SETGET_PROP(Pattern,              UINT32);
    SETGET_PROP(WrDelay,              UINT32);
    SETGET_PROP(RdDelay,              UINT32);
    SETGET_PROP(UpperBound,           UINT32);
    SETGET_PROP(DisableHostClockSlowdown,  bool);
    SETGET_PROP(RuntimeMs,            UINT32);

    SETGET_JSARRAY_PROP_LWSTOM(PatternTable)
};

#ifndef MATS_STANDALONE
JS_CLASS_INHERIT(MatsTest, GpuMemTest,
                 "MATS (Modified Algorithmic Test Sequence)");

CLASS_PROP_READWRITE(MatsTest, DataWidth, UINT32,
                     "Data Pattern Width (8, 16 or 32 bits)");
CLASS_PROP_READWRITE(MatsTest, Coverage, FLOAT64,
                     "Percent of framebuffer to test.");
CLASS_PROP_READWRITE(MatsTest, ShowStatus, bool,
                     "Display status bar in the upper left corner.");
CLASS_PROP_READWRITE(MatsTest, Iterations, UINT64,
                     "Number of times to repeat test");
CLASS_PROP_READWRITE(MatsTest, Pattern, UINT32,
                     "Selects the pattern");
CLASS_PROP_READWRITE(MatsTest, WrDelay, UINT32,
                     "Set the delay in nsec between FB writes");
CLASS_PROP_READWRITE(MatsTest, RdDelay, UINT32,
                     "Set the delay in nsec between FB reads");
CLASS_PROP_READWRITE(MatsTest, UpperBound, UINT32,
                     "Maximum value for bounded memory tests (bytes/boxes)");
CLASS_PROP_READWRITE(MatsTest, DisableHostClockSlowdown, bool,
                     "disable host clock slowdown");
CLASS_PROP_READWRITE(MatsTest, RuntimeMs, UINT32,
                     "Fixed test runtime in msec. If test completes before time has been reached, "
                     "the test is looped. Uses sim time in simulators, not wall clock time.");
CLASS_PROP_READWRITE_JSARRAY(MatsTest, PatternTable, JsArray,
        "Set pattern table for TestCommand(s) to index into");

//
// Constant Properties
//
#define MATS_CONST_PROPERTY(name, init, help) \
    static SProperty Display_##name           \
    (                                         \
        MatsTest_Object,                      \
        #name,                                \
        0,                                    \
        init,                                 \
        0,                                    \
        0,                                    \
        JSPROP_READONLY,                      \
        "CONSTANT - " help                    \
    )

// Pattern types
MATS_CONST_PROPERTY(LinearNormal, MEMFMT_LINEAR,
                    "Linear memory pattern type.");
MATS_CONST_PROPERTY(LinearBounded, MEMFMT_LINEAR_BOUNDED,
                    "Linear memory pattern type with upper bound limit.");
MATS_CONST_PROPERTY(BoxNormal, MEMFMT_BOX,
                    "Box memory pattern type.");
MATS_CONST_PROPERTY(BoxBounded, MEMFMT_BOX_BOUNDED,
                    "Box memory pattern type with upper bound limit.");

// Memory direction
MATS_CONST_PROPERTY(MemoryUp, MEMDIR_UP,
                    "Traverse from low to high memory.");
MATS_CONST_PROPERTY(MemoryDown, MEMDIR_DOWN,
                    "Traverse from high to low memory.");

UINT32 MatsTest::GetDataWidth() const
{
    return m_DataWidth;
}

RC MatsTest::SetDataWidth(UINT32 val)
{
    if ((val!= 8) && (val!=16) && (val!=32))
    {
        Printf(Tee::PriError, "DataWidth legal values are 8, 16 & 32.");
        return RC::ILWALID_ARGUMENT;
    }

    m_DataWidth = val;

    return OK;
}

RC MatsTest::GetPatternTable(JsArray *val) const
{
    MASSERT(val != NULL);
    *val = m_PatternTable;
    return OK;
}

RC MatsTest::SetPatternTable(JsArray *val)
{
    MASSERT(val != NULL);
    m_PatternTable = *val;
    return OK;
}

#endif // #ifndef MATS_STANDALONE

//
//  Internal MATS routines
//
// script generated constructor
MatsTest::MatsTest()
: m_pRandom(&(GetFpContext()->Rand))
, m_Iterations(1)
, m_DataWidth(32)
, m_Unit(static_cast<UINT32>(1_MB))
, m_Coverage(100)
, m_UpperBound(0)
, m_ShowStatus(true)
, m_DisableHostClockSlowdown(true)
, m_pTestConfig(GetTestConfiguration())
, m_Pitch(0)
, m_ResX(0)
, m_Pattern(1)
, m_ImageOffset(0)
, m_WrDelay(0)
, m_RdDelay(0)
, m_YieldCounter(0)
, m_DrawCounter(0)
, m_BarPosition(-STATUS_BAR_WIDTH)
, m_OldHostClockSlowdownDisabled(false)
, m_RuntimeMs(0)
, m_RdPtr({})
, m_WrPtr({})
{
    SetName("MatsTest");
}

MatsTest::~MatsTest()
{
}

//------------------------------------------------------------------------------
bool MatsTest::IsSupportedVf()
{
#ifndef MATS_STANDALONE
    return !(GetBoundGpuSubdevice()->IsSMCEnabled());
#else
    return false;
#endif
}

//-----------------------------------------------------------------------------
RC MatsTest::InitFromJs()
{
    RC rc = OK;

    CHECK_RC(GpuMemTest::InitFromJs());

    m_pRandom->SeedRandom(m_pTestConfig->Seed());

    if ((m_Coverage == 0) || (m_Coverage > 100))
    {
        Printf(Tee::PriNormal,
               "MatsTest.Coverage must be from 1 to 100 inclusive\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_pTestConfig->UseTiledSurface())
        Printf(Tee::PriLow, "NOTE: tiling not supported, "
               "m_TestConfiguration.UseTiledSurface ignored.\n");

#ifndef MATS_STANDALONE
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
#endif
    return OK;
}

//-----------------------------------------------------------------------------
void MatsTest::PrintJsProperties(Tee::Priority pri)
{
#ifndef MATS_STANDALONE
    GpuMemTest::PrintJsProperties(pri);

    const char * ft[] = {"false", "true"};

    Printf(pri, "\tCoverage:\t\t\t%.1f\n", m_Coverage);
    Printf(pri, "\tDataWidth:\t\t\t%d\n", m_DataWidth);
    Printf(pri, "\tShowStatus:\t\t%s\n", ft[m_ShowStatus]);
    Printf(pri, "\tIterations:\t\t\t%llu\n", m_Iterations);
    Printf(pri, "\tPattern:\t\t\t0x%x\n", m_Pattern);
    Printf(pri, "\tRuntimeMs:\t\t\t%u\n", m_RuntimeMs);
#endif
}

//-----------------------------------------------------------------------------
RC MatsTest::Setup()
{
    RC rc;

    CHECK_RC(GpuMemTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());
#ifndef MATS_STANDALONE
    m_Pitch = m_pTestConfig->SurfaceWidth() * m_pTestConfig->DisplayDepth() / 8;
    CHECK_RC(AdjustPitchForScanout(&m_Pitch));
#else
    CHECK_RC(GetDisplayPitch(&m_Pitch));
#endif
    m_ResX = m_Pitch / 4;

#ifndef MATS_STANDALONE
    if (m_DisableHostClockSlowdown)
    {
        // save current host clock slowdown setting
        LwRmPtr pLwRm;
        LwRm::Handle pRmHandle =
            pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
        LW2080_CTRL_MC_QUERY_HOSTCLK_SLOWDOWN_STATUS_PARAMS qParam = {0};
        CHECK_RC(pLwRm->Control(
                        pRmHandle,
                        LW2080_CTRL_CMD_MC_QUERY_HOSTCLK_SLOWDOWN_STATUS,
                        &qParam, sizeof(qParam)));
        m_OldHostClockSlowdownDisabled = (qParam.bDisabled != 0);
        // set current host clock slowdown
        LW2080_CTRL_MC_SET_HOSTCLK_SLOWDOWN_STATUS_PARAMS sParam = {0};
        sParam.bDisable = LW_TRUE;
        CHECK_RC(pLwRm->Control(pRmHandle,
                                LW2080_CTRL_CMD_MC_SET_HOSTCLK_SLOWDOWN_STATUS,
                                &sParam, sizeof(sParam)));
        Printf(Tee::PriLow, "host clock slowdown disabled\n");
    }
#endif

    CHECK_RC(AllocateEntireFramebuffer(
            false,         // Pitch Linear
            0));

    // Find the parts of the framebuffer that will be tested.
    // This is basically the part(s) of m_MemChunks between
    // GetStartLocation() and GetEndLocation() (in MB) that are aligned
    // to m_Unit boundaries.
    //
    m_TestRanges.clear();
    for (GpuUtility::MemoryChunks::iterator chunk = GetChunks()->begin();
            chunk != GetChunks()->end(); ++chunk)
    {
        MemoryRange testRange;
        testRange.StartOffset = chunk->fbOffset;
        testRange.EndOffset = chunk->fbOffset + chunk->size;

        if (testRange.StartOffset < GetStartLocation() * 1_MB)
            testRange.StartOffset = GetStartLocation() * 1_MB;
        if (testRange.EndOffset > GetEndLocation() * 1_MB)
            testRange.EndOffset = GetEndLocation() * 1_MB;

        testRange.StartOffset =
                ((testRange.StartOffset + m_Unit - 1) / m_Unit) * m_Unit;
        testRange.EndOffset = (testRange.EndOffset / m_Unit) * m_Unit;

        if (testRange.StartOffset < testRange.EndOffset)
        {
            m_TestRanges.push_back(testRange);
            Printf(Tee::PriLow,
                    "MATS will test 0x%08llx to 0x%08llx (%d MB to %d MB)\n",
                    testRange.StartOffset,
                    testRange.EndOffset -1,
                    UINT32(testRange.StartOffset / m_Unit),
                    UINT32(testRange.EndOffset / m_Unit));
        }
    }

    if (m_TestRanges.size() == 0)
    {
        Printf(Tee::PriNormal,
                "ERROR:  No framebuffer memory between start location "
                "(%x) and end location (%x).\n",
                GetStartLocation(), GetEndLocation());
        return RC::ILWALID_RAM_AMOUNT;
    }

#ifndef MATS_STANDALONE
    GpuUtility::MemoryChunkDesc *pDisplayChunk = nullptr;
    UINT64 displayOffset = 0;
    RC trc = GpuUtility::FindDisplayableChunk(
                     GetChunks(),
                     &pDisplayChunk,
                     &displayOffset,
                     GetStartLocation(),
                     GetEndLocation(),
                     m_pTestConfig->SurfaceHeight(),
                     m_Pitch,
                     GetBoundGpuDevice());

    if (trc == OK)
    {
        m_ImageOffset = pDisplayChunk->fbOffset + displayOffset;

        CHECK_RC(GetDisplayMgrTestContext()->DisplayMemoryChunk(
            pDisplayChunk,
            displayOffset,
            m_pTestConfig->SurfaceWidth(),
            m_pTestConfig->SurfaceHeight(),
            m_pTestConfig->DisplayDepth(),
            m_Pitch));
    }
    else
    {
        m_ImageOffset = GpuUtility::UNDEFINED_OFFSET;
    }
#endif

    // Parse the specified pattern table and list of sequences from the script
    m_MatsHelper.Reset();
    m_Sequences.clear();

    CHECK_RC(m_MatsHelper.ParseTable(m_PatternTable));
    CHECK_RC(m_MatsHelper.ParseSequenceList(m_TestCommands, &m_Sequences));

    bool haveInitBoxes = false;
    for (CommandSequences::iterator i = m_Sequences.begin();
         i != m_Sequences.end(); ++i)
    {
        // only initialize boxes if we encounter a box test
        if ((i->mem_fmt == MEMFMT_BOX) && !haveInitBoxes)
        {
            CHECK_RC(m_MatsHelper.InitBoxes(m_TestRanges, m_pRandom,
                                             m_Pitch, m_Coverage));
            CHECK_RC(FillPatternRgb());
            haveInitBoxes = true;
        }
        SetCommandParams(&i->operations);
    }

    // Find the memory range that starts at the status
    // area, and push the starting offset back to the end of that region.
    if (m_ShowStatus)
    {
        for (MemoryRanges::iterator i = m_TestRanges.begin();
                i != m_TestRanges.end(); i++)
        {
            if (i->StartOffset == m_ImageOffset)
            {
                i->StartOffset += (STATUS_PIXELS * sizeof(UINT32));
                break;
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC MatsTest::Run()
{
    RC rc = OK;
    bool timed = m_RuntimeMs > 0;
    bool running = true;
    UINT64 startTimeMs = Platform::GetTimeMS();

    Printf(Tee::PriDebug, "RdDelay = %dns; WrDelay = %dns\n",
           m_RdDelay, m_WrDelay);

    while (running)
    {
        Tasker::DetachThread detach;

        for (UINT64 i = 0; i < m_Iterations && running; i++)
        {
            for (CommandSequences::iterator j = m_Sequences.begin();
                 j != m_Sequences.end() && running; ++j)
            {
                if (j->mem_fmt == MEMFMT_BOX)
                    CHECK_RC(RunBoxPattern(m_TestRanges, *j));
                else if (j->mem_fmt == MEMFMT_LINEAR)
                    CHECK_RC(RunLinearPattern(m_TestRanges, *j));

                Printf(Tee::PriLow, "Successfully completed test pattern %s.\n",
                       j->pattern.c_str());

                if (timed && TestRuntimeExceeded(startTimeMs))
                {
                    // Ran for the required amount of time, stop.
                    running = false;
                }
            }
        }

        if (!timed)
        {
            // Stop after one complete test iteration if we are not running for
            // a specified amount of time.
            running = false;
        }
    }

    Printf(Tee::PriLow, "All test sequences have completed successfully.\n");

    return rc;
}

//------------------------------------------------------------------------------
RC MatsTest::Cleanup()
{
    StickyRC firstRc;

#ifndef MATS_STANDALONE
    if (m_DisableHostClockSlowdown)
    {
        // restore original setting
        LwRmPtr pLwRm;
        LwRm::Handle pRmHandle =
            pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
        LW2080_CTRL_MC_SET_HOSTCLK_SLOWDOWN_STATUS_PARAMS sParam = {0};
        sParam.bDisable = m_OldHostClockSlowdownDisabled;
        firstRc = pLwRm->Control(pRmHandle,
                                 LW2080_CTRL_CMD_MC_SET_HOSTCLK_SLOWDOWN_STATUS,
                                 &sParam, sizeof(sParam));
    }
#endif

    m_MatsHelper.Reset();
    m_Sequences.clear();
    m_TestRanges.clear();
    m_MapFb.UnmapFbRegion();

    firstRc = GpuMemTest::Cleanup();
    return firstRc;
}

RC MatsTest::FillPatternRgb()
{
    // Fill the display area with RGB
    void *addr = m_MapFb.MapFbRegion(GetChunks(), m_ImageOffset,
                     m_Pitch * m_pTestConfig->SurfaceHeight(),
                     GetBoundGpuSubdevice());

    if (addr == NULL)
    {
        // Should this be an error?
        Printf(Tee::PriWarn, "Could not map enough memory for the"
               " entire displayed region, skipping RGB fill\n");
    }
    else
    {
        Memory::FillRgb(addr,
                        m_pTestConfig->SurfaceWidth(),
                        m_pTestConfig->SurfaceHeight(),
                        m_pTestConfig->DisplayDepth(),
                        m_Pitch);

        // No need to UnmapFbRegion here, the MapFb object takes care of
        // that and MATS is going to ask to map parts of FB later in the
        // test anyway, so unmapping here is an unnecessary perf hit.
    }

    return OK; // return RC in case (addr == NULL) becomes an error case
}

// Return pointer to the m_MemChunks entry that contains Offset.
GpuUtility::MemoryChunkDesc *MatsTest::GetChunkDesc(UINT64 offset)
{
    for (GpuUtility::MemoryChunks::iterator chunk = GetChunks()->begin();
         chunk != GetChunks()->end(); ++chunk)
    {
        if (offset >= chunk->fbOffset &&
            offset < chunk->fbOffset + chunk->size)
        {
            return &(*chunk);
        }
    }

    // Should never get here.
    MASSERT(!"Illegal offset passed into MatsTest::GetChunkDesc");
    return NULL;
}

RC MatsTest::MemCmd_Noop(const MemoryCommand &cmd)
{
    return OK;
}

RC MatsTest::DoIdleTasks()
{
    const UINT32 YIELD_TICKS = 128;
    const UINT32 DRAW_TICKS = 32;
    RC rc;

#ifndef MATS_STANDALONE
    CHECK_RC(Abort::Check());
#endif

    if ((++m_YieldCounter >= YIELD_TICKS) && Tasker::IsInitialized())
    {
#ifndef USE_NEW_TASKER
        CHECK_RC(Tasker::Yield());
#endif
        m_YieldCounter = 0;

        // update the screen every few yields
        if (++m_DrawCounter >= DRAW_TICKS)
        {
            if (m_ShowStatus)
            {
                volatile UINT32 *mem = (volatile UINT32 *)m_MapFb.MapFbRegion(
                        GetChunks(), m_ImageOffset,
                        STATUS_PIXELS * sizeof(UINT32),
                        GetBoundGpuSubdevice());
                if (!mem)
                    return RC::CANNOT_ALLOCATE_MEMORY;

                INT32 barStart = max<INT32>(0, m_BarPosition);
                INT32 barEnd = min(m_BarPosition + STATUS_BAR_WIDTH,
                                   STATUS_PIXELS);

                // draw left of bar (black), bar (white), and right of
                // bar (black)
                for (INT32 i = 0; i < barStart; i++)
                    mem[i] = 0;
                for (INT32 i = barStart; i < barEnd; i++)
                    mem[i] = (UINT32)-1;
                for (INT32 i = barEnd; i < STATUS_PIXELS; i++)
                    mem[i] = 0;

                if (++m_BarPosition > STATUS_PIXELS)
                    m_BarPosition = -STATUS_BAR_WIDTH;
            }

            m_DrawCounter = 0;

            // EndLoop calls JS and thus it is not thread safe
            Tasker::AttachThread attach;

            CHECK_RC(EndLoop());
        }
    }

    return OK;
}

bool MatsTest::TestRuntimeExceeded(UINT64 startTimeMs) const
{
    return (Platform::GetTimeMS() - startTimeMs) >= static_cast<UINT64>(m_RuntimeMs);
}

RC MatsTest::MemoryError(const MemoryCommand &mem, UINT08 read, UINT08 expect)
{
    Tasker::AttachThread attach;
    GpuUtility::MemoryChunkDesc *pChunkDesc = GetChunkDesc(mem.ptr->Offset);
    RC rc;
    CHECK_RC(GetMemError(0).LogMemoryError(
                    8, mem.ptr->Offset, read, expect,
                    MEM_RD08(mem.ptr->Virtual), MEM_RD08(mem.ptr->Virtual),
                    pChunkDesc->pteKind, pChunkDesc->pageSizeKB,
                    MemError::NO_TIME_STAMP, "", 0));
    return OK;
}

RC MatsTest::MemoryError(const MemoryCommand &mem, UINT16 read, UINT16 expect)
{
    Tasker::AttachThread attach;
    GpuUtility::MemoryChunkDesc *pChunkDesc = GetChunkDesc(mem.ptr->Offset);
    RC rc;
    CHECK_RC(GetMemError(0).LogMemoryError(
                    16, mem.ptr->Offset, read, expect,
                    MEM_RD16(mem.ptr->Virtual), MEM_RD16(mem.ptr->Virtual),
                    pChunkDesc->pteKind, pChunkDesc->pageSizeKB,
                    MemError::NO_TIME_STAMP, "", 0));
    return OK;
}

RC MatsTest::MemoryError(const MemoryCommand &mem, UINT32 read, UINT32 expect)
{
    Tasker::AttachThread attach;
    GpuUtility::MemoryChunkDesc *pChunkDesc = GetChunkDesc(mem.ptr->Offset);
    RC rc;
    CHECK_RC(GetMemError(0).LogMemoryError(
                    32, mem.ptr->Offset, read, expect,
                    MEM_RD32(mem.ptr->Virtual), MEM_RD32(mem.ptr->Virtual),
                    pChunkDesc->pteKind, pChunkDesc->pageSizeKB,
                    MemError::NO_TIME_STAMP, "", 0));
    return OK;
}

RC MatsTest::MemoryError(const MemoryCommand &mem, UINT64 read, UINT64 expect)
{
    Tasker::AttachThread attach;
    GpuUtility::MemoryChunkDesc *pChunkDesc = GetChunkDesc(mem.ptr->Offset);
    RC rc;
    CHECK_RC(GetMemError(0).LogMemoryError(
                    64, mem.ptr->Offset, read, expect,
                    MEM_RD64(mem.ptr->Virtual), MEM_RD64(mem.ptr->Virtual),
                    pChunkDesc->pteKind, pChunkDesc->pageSizeKB,
                    MemError::NO_TIME_STAMP, "", 0));
    return OK;
}

void MatsTest::SetCommandParams(MemoryCommands *cmd)
{
    static MemoryRoutinePtr routines[] =
    {
        MATS_DECLARE_MEMROUTINE_PTRS(08),
        MATS_DECLARE_MEMROUTINE_PTRS(16),
        MATS_DECLARE_MEMROUTINE_PTRS(32),
        MATS_DECLARE_MEMROUTINE_PTRS(64)
    };

    for (MemoryCommands::iterator i = cmd->begin(); i != cmd->end(); ++i)
    {
        if (i->op_type == OPTYPE_READ)
            i->ptr = &m_RdPtr;
        else
            i->ptr = &m_WrPtr;

        i->run = routines[i->op_type +
                          i->op_mode * NUM_OPTYPE +
                          i->op_size * NUM_OPTYPE * NUM_OPMODE];
    }
}

// RunLinearPattern operates differently depending on whether or not the current
// sequence is ascending or descending through memory. CommandSequence::runflags
// contains a bit field (initialized in ParseCommands) describing what types of
// commands will be run (which is just read or write at the moment). When either
// the read or write pointer reaches the end of the current MemoryRange, it
// clears its runflags bit.
RC MatsTest::RunLinearPattern
(
    const MemoryRanges &testRanges,
    CommandSequence &seq
)
{
    const UINT32 chunkSize = 8;
    RC rc;
    const bool timed = m_RuntimeMs > 0;
    const UINT64 startTimeMs = Platform:: GetTimeMS();
    bool timedOut = false;
    MemoryCommands &ops = seq.operations;
    UINT32 allocSize = (UINT32)ops.size() * chunkSize * seq.memsize;

    if (seq.ascending)
    {
        // perform memory test from low to high memory
        for (MemoryRanges::const_iterator range = testRanges.begin();
             range != testRanges.end() && !timedOut; ++range)
        {
            UINT64 range_max = seq.bounded
                ? range->StartOffset + m_UpperBound - seq.memsize
                : range->EndOffset - seq.memsize;

            m_RdPtr.Offset = m_WrPtr.Offset = range->StartOffset;

            UINT32 counter = chunkSize;
            UINT32 running = seq.runflags;
            while (running && !timedOut)
            {
                if (++counter >= chunkSize)
                {
                    CHECK_RC(DoIdleTasks());
                    m_RdPtr.Virtual = m_WrPtr.Virtual = MapAhead(
                            max(m_RdPtr.Offset, m_WrPtr.Offset),
                            allocSize, range->EndOffset);
                    if (!m_RdPtr.Virtual)
                        return RC::CANNOT_ALLOCATE_MEMORY;
                    counter = 0;

                    if (timed && TestRuntimeExceeded(startTimeMs))
                    {
                        // Ran for required amount of time, stop
                        timedOut = true;
                    }
                }
                for (MemoryCommands::iterator i = ops.begin();
                     i != ops.end() && !timedOut; ++i)
                {
                    if (!(running & i->runflag))
                        continue;
                    CHECK_RC(CALL_THIS(i->run)(*i));

                    PointerParams &ptr = *i->ptr;
                    if (i->increment)
                    {
                        if (ptr.Offset >= range_max)
                            running &= ~i->runflag;
                        ptr.Offset += seq.memsize;
                        ptr.Virtual += seq.memsize;
                    }

                    if (timed && TestRuntimeExceeded(startTimeMs))
                    {
                        // Ran for required amount of time, stop
                        timedOut = true;
                    }
                }
            }
        }
    }
    else
    {
        // perform memory test from high to low memory
        for (MemoryRanges::const_reverse_iterator range = testRanges.rbegin();
             range != testRanges.rend() && !timedOut; ++range)
        {
            UINT64 range_min = range->StartOffset;
            UINT64 range_max = seq.bounded
                ? range->StartOffset + m_UpperBound - seq.memsize
                : range->EndOffset - seq.memsize;
            m_RdPtr.Offset = range_max;
            m_WrPtr.Offset = range_max;

            UINT32 counter = chunkSize;
            UINT32 running = seq.runflags;
            while (running && !timedOut)
            {
                if (++counter >= chunkSize)
                {
                    CHECK_RC(DoIdleTasks());
                    m_RdPtr.Virtual = m_WrPtr.Virtual = MapBehind(
                            min(m_RdPtr.Offset, m_WrPtr.Offset),
                            allocSize, range->StartOffset);
                    if (!m_RdPtr.Virtual)
                        return RC::CANNOT_ALLOCATE_MEMORY;
                    counter = 0;

                    if (timed && TestRuntimeExceeded(startTimeMs))
                    {
                        // Ran for required amount of time, stop
                        timedOut = true;
                    }
                }
                for (MemoryCommands::iterator i = ops.begin();
                     i != ops.end() && !timedOut; ++i)
                {
                    if (!(running & i->runflag))
                        continue;
                    CHECK_RC(CALL_THIS(i->run)(*i));

                    PointerParams &ptr = *i->ptr;
                    if (i->increment)
                    {
                        if (ptr.Offset <= range_min)
                            running &= ~i->runflag;
                        ptr.Offset -= seq.memsize;
                        ptr.Virtual -= seq.memsize;
                    }

                    if (timed && TestRuntimeExceeded(startTimeMs))
                    {
                        // Ran for required amount of time, stop
                        timedOut = true;
                    }
                }
            }
        }
    }

    return OK;
}

// The same idea as RunLinearPattern, but uses the box layouts callwlated in
// InitBoxes instead of MemoryRanges to traverse memory.
RC MatsTest::RunBoxPattern
(
    const MemoryRanges &testRanges,
    CommandSequence &seq
)
{
    int rc = OK;
    const bool timed = m_RuntimeMs > 0;
    const UINT64 startTimeMs = Platform::GetTimeMS();
    bool timedOut = false;
    MemoryCommands &ops = seq.operations;

    double scale = sizeof(UINT32) / (double)seq.memsize;
    UINT32 effectiveWidth = (UINT32)(scale * MatsHelper::BoxWidth);

    UINT32 actualBoxes = seq.bounded
        ? min(m_MatsHelper.GetBoxCount(), m_UpperBound)
        : m_MatsHelper.GetBoxCount();
    UINT64 line_pitch = m_ResX * sizeof(UINT32);
    UINT64 horz_span = MatsHelper::BoxWidth * sizeof(UINT32) - seq.memsize;

    // Boxes start and end on megabyte boundaries, and may still overlap with
    // the status area, so don't allow it to be displayed for these tests
    bool lastStatusState = m_ShowStatus;
    m_ShowStatus = false;

    Tee::SetLowAssertLevel lowAssertLevel;
    if (seq.ascending)
    {
        // draw each box from low to high memory
        for (UINT32 i = 0; i < actualBoxes && !timedOut; i++)
        {
            UINT32 boxNum = m_MatsHelper.GetBox(i);
            volatile UINT08 *virtTlc = MapBoxRegion(boxNum);
            if (!virtTlc)
                return RC::CANNOT_ALLOCATE_MEMORY;

            m_RdPtr.Offset = m_WrPtr.Offset = m_MatsHelper.GetBoxTlc(boxNum);
            m_RdPtr.Virtual = m_WrPtr.Virtual = virtTlc;
            m_RdPtr.Xpos = m_WrPtr.Xpos = 0;
            m_RdPtr.Ypos = m_WrPtr.Ypos = 0;

            UINT32 running = seq.runflags;
            while (running && !timedOut)
            {
                for (MemoryCommands::iterator i = ops.begin();
                     i != ops.end() && !timedOut; ++i)
                {
                    if (!(running & i->runflag))
                        continue;
                    CHECK_RC(CALL_THIS(i->run)(*i));

                    PointerParams &ptr = *i->ptr;
                    ptr.Offset += seq.memsize;
                    ptr.Virtual += seq.memsize;
                    if (++ptr.Xpos >= effectiveWidth)
                    {
                        ptr.Xpos = 0;
                        if (++ptr.Ypos >= MatsHelper::BoxHeight)
                            running &= ~i->runflag;
                        ptr.Offset = m_MatsHelper.GetBoxTlc(boxNum) +
                                     ptr.Ypos * line_pitch;
                        ptr.Virtual = virtTlc + ptr.Ypos * line_pitch;
                    }

                    if (timed && TestRuntimeExceeded(startTimeMs))
                    {
                        // Ran for required amount of time, stop
                        timedOut = true;
                    }
                }
            }

            if (timed && timedOut)
            {
                // Skip remainder of iteration
                continue;
            }

            CHECK_RC(DoIdleTasks());

            if (timed && TestRuntimeExceeded(startTimeMs))
            {
                // Ran for required amount of time, stop
                timedOut = true;
            }
        }
    }
    else
    {
        // draw each box from high to low memory
        for (UINT32 i = 0; i < actualBoxes && !timedOut; i++)
        {
            UINT32 boxNum = m_MatsHelper.GetBox(i);
            UINT64 offMax = (MatsHelper::BoxHeight-1) * line_pitch + horz_span;
            volatile UINT08 *virtTlc = MapBoxRegion(boxNum);
            if (!virtTlc)
                return RC::CANNOT_ALLOCATE_MEMORY;

            m_RdPtr.Offset = m_WrPtr.Offset =
                m_MatsHelper.GetBoxTlc(boxNum) + offMax;
            m_RdPtr.Virtual = m_WrPtr.Virtual = virtTlc + offMax;
            m_RdPtr.Xpos = m_WrPtr.Xpos = effectiveWidth - 1;
            m_RdPtr.Ypos = m_WrPtr.Ypos = MatsHelper::BoxHeight - 1;

            UINT32 running = seq.runflags;
            while (running && !timedOut)
            {
                for (MemoryCommands::iterator i = ops.begin();
                     i != ops.end() && !timedOut; ++i)
                {
                    if (!(running & i->runflag))
                        continue;
                    CHECK_RC(CALL_THIS(i->run)(*i));

                    PointerParams &ptr = *i->ptr;
                    ptr.Offset -= seq.memsize;
                    ptr.Virtual -= seq.memsize;
                    if (ptr.Xpos <= 0)
                    {
                        if (ptr.Ypos <= 0)
                            running &= ~i->runflag;
                        else
                            --ptr.Ypos;
                        ptr.Xpos = effectiveWidth;

                        UINT64 lwrrOff = ptr.Ypos * line_pitch + horz_span;
                        ptr.Offset = m_MatsHelper.GetBoxTlc(boxNum) + lwrrOff;
                        ptr.Virtual = virtTlc + lwrrOff;
                    }
                    --ptr.Xpos;

                    if (timed && TestRuntimeExceeded(startTimeMs))
                    {
                        // Ran for required amount of time, stop
                        timedOut = true;
                    }
                }
            }

            if (timed && timedOut)
            {
                // Skip remainder of iteration
                continue;
            }

            CHECK_RC(DoIdleTasks());

            if (timed && TestRuntimeExceeded(startTimeMs))
            {
                // Ran for required amount of time, stop
                timedOut = true;
            }
        }
    }

    m_ShowStatus = lastStatusState; // if status bar was visible, restore it
    return OK;
}

// Map the region of memory around the specified box
volatile UINT08 *MatsTest::MapBoxRegion(UINT32 box)
{
    size_t alloc = ((m_ResX * (MatsHelper::BoxHeight - 1)) +
                   MatsHelper::BoxWidth) * sizeof(UINT32);

    return (volatile UINT08 *)m_MapFb.MapFbRegion(
            GetChunks(), m_MatsHelper.GetBoxTlc(box), (UINT32)alloc,
            GetBoundGpuSubdevice());
}

// Map memory in the region [offset, min(offset + size, maxMem))
volatile UINT08 *MatsTest::MapAhead(UINT64 offset, UINT32 size, UINT64 maxMem)
{
    UINT32 alloc = ((offset + size) < maxMem) ? size : UINT32(maxMem - offset);

    return (volatile UINT08 *)m_MapFb.MapFbRegion(GetChunks(),
            offset, alloc, GetBoundGpuSubdevice());
}

// Map memory in the region (max(minMem, offset - size), offset]
volatile UINT08 *MatsTest::MapBehind(UINT64 offset, UINT32 size, UINT64 minMem)
{
    UINT64 startPos = ((offset - minMem) < size) ? minMem : (offset - size);
    UINT32 alloc = UINT32(offset - startPos);

    // add 'alloc' to the virtual address so that it is mapped to 'offset'
    return (volatile UINT08 *)m_MapFb.MapFbRegion(GetChunks(),
            startPos, alloc, GetBoundGpuSubdevice()) + alloc;
}
