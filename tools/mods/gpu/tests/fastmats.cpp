/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file fastmats.cpp  Implements the FastMats gpu FB memory test.
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/framebuf.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/gpu.h"
#include "core/include/threadpool.h"

#include "core/include/cpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tee.h"
#include "gputestc.h"
#include "core/include/massert.h"
#include "gpumtest.h"
#include "random.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/include/notifier.h"
#include "gpu/utility/surf2d.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "class/cl502d.h" // LW50_TWOD
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cla06fsubch.h"
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "gpu/include/gralloc.h"
#include "gpu/include/nfysema.h"
#include "lwos.h"
#include "core/utility/ptrnclss.h"
#include "core/include/xp.h"
#include <algorithm>
#include <map>

#define END_PATTERN PatternClass::END_PATTERN

//------------------------------------------------------------------------------
//! \brief FAST MATS Memory Test.
//!
//! MATS is short for Modified Algorithmic Test Sequence
//!
//! Based on an algorithm described in "March tests for word-oriented
//! memories" by A.J. van de Goor, Delft University, the Netherlands
//!
//! FAST MATS OVERVIEW:
//!
//! FastMats tests framebuffer memory using hardware rendering.
//!
//! 1.  It first divides the framebuffer into "boxes" of 64x64 pixels.
//! 2.  Render a pattern to a random rectangle
//! 3.  DMA the contents of that rendered box to system memory
//! 4.  Pick a different random rectangle and render a pattern to it
//!      (note: #3 and #4 occur in parallel to make the test more stressful)
//! 5.  Start the rendering of the pattern to the next random rectangle.
//! 6.  Check the contents of the box DMA'd to system memory in #3.
//!      (note: #5 and #6 occur in parallel to make the test more stressful)
//! 7.  Repeat steps 3-6 for all boxes
//! 8.  Repeat steps 3-7 for all patterns
//------------------------------------------------------------------------------

class FastMatsTest : public GpuMemTest
{
public:
    FastMatsTest();
    virtual ~FastMatsTest();

    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);
    virtual bool IsSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    virtual bool GetTestsAllSubdevices() { return true; }

    // Ick!  Public data!
    UINT32 * m_UserPatterns;

    void CheckPCIDataThread(UINT32 tid);

private:
    enum
    {
        s_SubMemToMem = 2,
        s_SubTwoD     = LWA06F_SUBCHANNEL_2D, // = 3
        s_SubCE       = LWA06F_SUBCHANNEL_COPY_ENGINE, // = 4

        PAGESIZE = 1024 * 4, //4kb in bytes
    };

    // Graphics mode to run the test.
    struct
    {
        UINT32 width;
        UINT32 height;
        UINT32 depth;
        UINT32 refreshRate;
    } m_Mode;

    GpuTestConfiguration * m_pTestConfig;

    Notifier m_Notifier2d;
    Notifier m_NotifierMemToMem;
    NotifySemaphore m_TwodSemaphore;
    NotifySemaphore m_CeSemaphore;

    FLOAT64 m_TimeoutMs;
    UINT32 m_Pitch;

    UINT32 m_BoxHeight;
    UINT32 m_BoxWidth;
    UINT32 m_XferSize;
    UINT32 m_BoxWidthBytes;
    UINT32 m_XferSizeBytes;
    UINT32 m_NumBoxes;
    UINT32 * m_ForwardSwizzle;
    UINT32 * m_BackwardSwizzle;
    UINT32 * m_Swizzle;

    UINT32 m_BoxesPerRow;
    UINT32 m_BoxesPerMegabyte;
    UINT32 m_BytesPerBoxRow;

    PatternClass m_PatternClass;
    UINT32 *m_DataPatterns;
    UINT32 m_NumPatterns;
    string m_PatternName;

    //Surface  info
    Surface2D  m_Pci;

    // Channel parameters.
    Channel *    m_pCh;
    Channel *    m_pCeCh;
    LwRm::Handle m_hCh;
    LwRm::Handle m_hCeCh;

    DmaCopyAlloc m_CopyEngine;
    TwodAlloc    m_TwoD;

    // We allocate one FB ctxDma for each memory chunk.
    struct FbCtxDma
    {
        UINT64    offset;          //!< Offset of the ctxDma in the framebuffer
        UINT64    ctxDmaOffsetGpu; //!< GPU virtual address of the owning chunk
        UINT64    size;            //!< Size of the ctxDma
        UINT32    pteKind;
        UINT32    pageSizeKB;
        LwRm::Handle handle; // ctxDma handle

        // Used to sort by offset
        bool operator<(const FbCtxDma &rhs) const { return offset<rhs.offset; }
    };
    vector<FbCtxDma> m_FbCtxDmas;

    FLOAT64 m_Coverage;
    UINT32 m_Iterations;
    UINT32 m_Background;
    UINT32 m_Pattern;
    // Cleo's birthday
    static const UINT32 s_CePayload = 0x20050401;
    volatile INT32 m_TwodPayload;

    // Callwlate various critical dimensions & sizes
    RC CallwlateBoxes();

    // Allocate the graphic surfaces.
    RC AllocateContextDma();

    // Allocate and Instantiate the graphic objects.
    RC InstantiateObjects();

    // Run fastmats.
    RC DoFastMats();

    // fill or check a box with a specific color
    RC FillBox(UINT32 boxNum, UINT32 colorNum);
    RC CopyToPci(UINT32 boxNum, UINT32 destOffsetBytes, UINT32 subdev);
    RC CheckPciBuf(UINT32 boxNum, UINT32 colorNum, UINT32 subdev);
    RC CopyEngineCopyToPci(UINT64 srcOffset64,
                           UINT64 dstOffset64,
                           UINT32 subdevMask);

    // functions to handle notifiers
    RC   WaitCeNotifier();
    RC   WaitForDmaCompletion();
    bool Check4aNotifier ();
    RC   Wait4aNotifier();

    // misc functions
    UINT64 BoxNumToFbOffset(UINT32 boxNum) const;
    FbCtxDma *GetFbCtxDma(UINT64 fbOffset);
    RC FillAlternateBox(UINT32 i, UINT32 colorNum);

    class CheckPCIDataTask
    {
    public:

        explicit CheckPCIDataTask(FastMatsTest *pFastMats) :
            m_pFastMats(pFastMats), m_ThreadId(0) {}

        void SetThreadId(UINT32 threadId) { m_ThreadId = threadId; }

        void operator()()
        {
            m_pFastMats->CheckPCIDataThread(m_ThreadId);
        }

    private:
        FastMatsTest *m_pFastMats;
        UINT32 m_ThreadId;
    };

    void LaunchCheckPCIDataThreadsSync(UINT32 color, UINT32 pattern);

    void *m_ThreadErrorMutex;
    UINT32 m_NumThreads;
    UINT32 m_ThreadWorkLen;
    UINT32 m_ThreadPattern;
    UINT32 m_ThreadColor;
    bool m_ThreadErrorFound;

    typedef map<UINT32, pair<UINT32, UINT32> > ThreadErrorMap;
    ThreadErrorMap m_ThreadErrors;

    Tasker::ThreadPool<CheckPCIDataTask> m_CheckPCIDataTaskPool;
    vector<CheckPCIDataTask> m_CheckPCIDataTasks;

public:
    SETGET_PROP(Coverage, FLOAT64);
    SETGET_PROP(Iterations, UINT32);
    SETGET_PROP(Background, UINT32);
    SETGET_PROP(Pattern, UINT32);
    SETGET_PROP(NumThreads, UINT32);
};

//------------------------------------------------------------------------------
// FastMatsTest object, properties and methods.
//------------------------------------------------------------------------------

JS_CLASS_INHERIT(FastMatsTest, GpuMemTest, "Fast MATS");

CLASS_PROP_READWRITE(FastMatsTest, Coverage, FLOAT64,
                     "Percent of available framebuffer to test.");
CLASS_PROP_READWRITE(FastMatsTest, Iterations, UINT32,
                     "Number of times to repeat test");
CLASS_PROP_READWRITE(FastMatsTest, Background, UINT32,
                     "Use Background noise blits");
CLASS_PROP_READWRITE(FastMatsTest, Pattern, UINT32,
                     "Select the pattern with which the algorithm will be rendered");
CLASS_PROP_READWRITE(FastMatsTest, NumThreads, UINT32,
                     "Number of CPU threads to execute sysmem reads from");

// default pattern is shortmats. there is nothing specific with it.
// can be anything else.

C_(FastMatsTest_SetUserPattern);
static STest FastMatsTest_SetUserPattern
(
    FastMatsTest_Object,
    "SetUserPattern",
    C_FastMatsTest_SetUserPattern,
    1,
    "Set a user-defined pattern"
);

C_(FastMatsTest_ClearUserPattern);
static STest FastMatsTest_ClearUserPattern
(
    FastMatsTest_Object,
    "ClearUserPattern",
    C_FastMatsTest_ClearUserPattern,
    1,
    "Reset pattern to default"
);

C_(FastMatsTest_SetUserPattern)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;
    JsArray  ptnArray;
    *pReturlwalue = JSVAL_NULL;
    UINT32 i;

    FastMatsTest *pTest = (FastMatsTest *)JS_GetPrivate(pContext, pObject);

    // User must pass the patterns to run.
    if (NumArguments == 0)
    {
        Printf(Tee::PriNormal,
               "Usage: FastMats.SetUserPattern(arg1, arg2, arg3, ...)\n");
        Printf(Tee::PriNormal, "       FastMats.SetUserPattern([array])\n");

        return JS_FALSE;
    }

    // if the user passed in an array for the first argument
    if (OK == (pJavaScript->FromJsval(pArguments[0], &ptnArray)))
    {
        UINT32 size = (UINT32)ptnArray.size();

        Printf(Tee::PriLow, "Parsing an array of size %d\n", size);
        if (size == 0)
        {
            Printf(Tee::PriNormal, "Can't parse an array of size zero\n");
            return JS_FALSE;
        }

        // Clear out any previously set pattern
        if (pTest->m_UserPatterns)
        {
            delete[] pTest->m_UserPatterns;
            pTest->m_UserPatterns = 0;
        }

        pTest->m_UserPatterns = new UINT32[size + 1];

        for (i = 0; i < size; i++)
        {
            if (OK != pJavaScript->FromJsval(ptnArray[i],
                                             &(pTest->m_UserPatterns[i])))
            {
                delete[] pTest->m_UserPatterns;
                pTest->m_UserPatterns=0;

                Printf(Tee::PriNormal,
                       "Can't parse element %d of the array.\n", i);
                return JS_FALSE;
            }
        }
        pTest->m_UserPatterns[size] = END_PATTERN;
    }
    else
      // if the user passed in a list of ints e.g. (1, 2, 3, 4, 5)
    {
        // Clear out any previously set pattern
        if (pTest->m_UserPatterns)
        {
            delete[] pTest->m_UserPatterns;
            pTest->m_UserPatterns = 0;
        }
        pTest->m_UserPatterns = new UINT32[NumArguments + 1];

        for (i = 0; i < NumArguments; i++)
        {
            if (OK != pJavaScript->FromJsval(pArguments[i],
                                             &(pTest->m_UserPatterns[i])))
            {
                delete[] pTest->m_UserPatterns;
                pTest->m_UserPatterns=0;

                JS_ReportError(pContext,
                               "Can't colwert parameters from JavaScript");
                return JS_FALSE;
            }
        }
        pTest->m_UserPatterns[ NumArguments ] = END_PATTERN;
    }

    // print out the list of parameters
    Printf(Tee::PriLow, "List of parameters parsed:\n");

    i=0;
    while (END_PATTERN != pTest->m_UserPatterns[i])
        Printf(Tee::PriLow, "%08x  ", pTest->m_UserPatterns[i++]);

    Printf(Tee::PriLow, "\n");

    return JS_TRUE;
}

C_(FastMatsTest_ClearUserPattern)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    FastMatsTest *pTest = (FastMatsTest *)JS_GetPrivate(pContext, pObject);

    // User must pass the patterns to run.
    if (NumArguments != 0)
    {
        Printf(Tee::PriNormal, "Usage: FastMats.ClearUserPattern()\n");

        return JS_FALSE;
    }

    if (pTest->m_UserPatterns)
    {
        delete[] pTest->m_UserPatterns;
        pTest->m_UserPatterns = 0;
    }

    // print out the list of parameters
    Printf(Tee::PriLow,
           "Deleted user patterns, default patterns will be used\n");

    return JS_TRUE;
}

//------------------------------------------------------------------------------
// Implementation of FastMatsTest:: functions
//------------------------------------------------------------------------------

FastMatsTest::FastMatsTest()
: m_ThreadErrorMutex(Tasker::AllocMutex("FastMatsTest::m_ThreadErrorMutex",
                                        Tasker::mtxUnchecked))
, m_NumThreads(3) // Empirically determined to give best performance
, m_ThreadWorkLen(0)
, m_ThreadPattern(0x0)
, m_ThreadColor(0x0)
, m_ThreadErrorFound(false)
{
    SetName("FastMatsTest");
    m_UserPatterns = NULL;

    m_Mode.width       = 0;
    m_Mode.height      = 0;
    m_Mode.depth       = 0;
    m_Mode.refreshRate = 0;

    m_pTestConfig = GetTestConfiguration();
    m_PatternName = "";

    // m_Notifier2d
    // m_NotifierMemToMem

    m_TimeoutMs = 0;
    m_Pitch = 0;

    m_BoxHeight = 0;
    m_BoxWidth = 0;
    m_XferSize = 0;
    m_BoxWidthBytes = 0;
    m_XferSizeBytes = 0;

    m_BoxesPerRow = 0;
    m_BoxesPerMegabyte = 0;
    m_BytesPerBoxRow = 0;

    m_NumBoxes = 0;
    m_ForwardSwizzle = 0;
    m_BackwardSwizzle = 0;
    m_Swizzle = 0;

    // m_PatternClass
    m_DataPatterns = NULL;
    m_NumPatterns = 0;

    m_pCh = NULL;
    m_hCh = 0;
    m_pCeCh = NULL;
    m_hCeCh = 0;

    // m_Surfaces2D
    // m_CopyEngine
    // m_TwoD

    // m_FbCtxDmas

    m_Coverage = 100;
    m_Iterations = 1;
    m_Background = 1;
    m_Pattern = 0;
    m_TwodPayload = 2011;
}

FastMatsTest::~FastMatsTest()
{
    Tasker::FreeMutex(m_ThreadErrorMutex);
}

RC FastMatsTest::AllocateContextDma()
{
    RC        rc;

    // Allocate 1 notifier structure for TWOD
    CHECK_RC(m_Notifier2d.Allocate(m_pCh, 1, m_pTestConfig));
    Printf(Tee::PriLow, "Allocated TwoD Notifier\n");

    m_CeSemaphore.Setup(NotifySemaphore::ONE_WORD,
                        m_pTestConfig->NotifierLocation(),
                        1);
    CHECK_RC(m_CeSemaphore.Allocate(m_pCeCh, 0));
    m_CeSemaphore.SetPayload(0);

    {
        //Setup Host semaphore for Twod engine(cl502d) in FillBox
        m_TwodSemaphore.Setup(NotifySemaphore :: ONE_WORD,
                m_pTestConfig->NotifierLocation(),
                GetBoundGpuDevice()->GetNumSubdevices());
        for (UINT32 subInst = 0;
                subInst < GetBoundGpuDevice()->GetNumSubdevices();
                subInst++)
        {
            //Set the address to write the semaphore payload
            // Tell this subdevice the surface address.
            CHECK_RC(m_TwodSemaphore.Allocate(m_pCh, subInst));
            if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
                CHECK_RC(m_pCh->WriteSetSubdevice(1<<subInst));
            CHECK_RC(m_pCh->SetContextDmaSemaphore(
                        m_TwodSemaphore.GetCtxDmaHandleGpu(subInst)));
            CHECK_RC(m_pCh->SetSemaphoreOffset(
                        m_TwodSemaphore.GetCtxDmaOffsetGpu(subInst)));
        }
        // Restore the default "all subdevices" mask in the channel.
        if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
            m_pCh->WriteSetSubdevice(Channel::AllSubdevicesMask);
        //Clear the semaphore memory first for all subdevices
        m_TwodSemaphore.SetPayload(0xDEAD);
    }

    // Allocate framebuffer memory
    //
    // allocate and set up ctxdma for PCI memory
    UINT32 bufSize = 3*m_XferSizeBytes;

    if (GetNumRechecks())
    {
        bufSize = (1 + GetNumRechecks()) * m_XferSizeBytes;
    }

    CHECK_RC(m_pTestConfig->ApplySystemModelFlags(Memory::Coherent, &m_Pci));
    m_Pci.SetWidth(bufSize);  // Up to three copies in case of error
    m_Pci.SetPitch(bufSize);
    m_Pci.SetHeight(1);
    m_Pci.SetColorFormat(ColorUtils::Y8);
    m_Pci.SetLocation(Memory::Coherent);
    m_Pci.SetProtect(Memory::ReadWrite);
    CHECK_RC(m_Pci.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_Pci.Map());
    CHECK_RC(m_Pci.BindGpuChannel(m_hCh));

    // Poisons buffer so that stale data isn't read if a DMA fails
    Platform::MemSet(m_Pci.GetAddress(), 0xCD, bufSize);

    Printf(Tee::PriLow, "Created a ctxdma for PCI\n");

    CHECK_RC(AllocateEntireFramebuffer(false,       // Pitch Linear
                                       m_hCh));

    // Create a ctxDma for the framebuffer chunks (or partial chunks)
    // that lie between GetStartLocation() and GetEndLocation().
    //
    UINT64 start = GetStartLocation() * 1_MB;
    UINT64 end   = GetEndLocation() * 1_MB;
    for (GpuUtility::MemoryChunks::iterator chunk = GetChunks()->begin();
         chunk != GetChunks()->end(); ++chunk)
    {
        UINT64 startOffset = max(start, chunk->fbOffset);
        UINT64 endOffset = min(end, chunk->fbOffset + chunk->size);
        if (startOffset < endOffset)
        {
            FbCtxDma newCtxDma;
            newCtxDma.offset          = startOffset;
            newCtxDma.ctxDmaOffsetGpu = chunk->ctxDmaOffsetGpu;
            newCtxDma.size            = endOffset - startOffset;
            newCtxDma.pteKind         = chunk->pteKind;
            newCtxDma.pageSizeKB      = chunk->pageSizeKB;
            newCtxDma.handle          = chunk->hCtxDma;
            m_FbCtxDmas.push_back(newCtxDma);
        }
    }

    if (m_FbCtxDmas.size() == 0)
    {
        Printf(Tee::PriError,
               "No FB memory from StartLocation(0x%x) to EndLocation(0x%x)\n",
               GetStartLocation(), GetEndLocation());
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    Printf(Tee::PriLow, "Finished FB ctxdma allocation\n");

    // Sort the FB ctxDmas by offset (so we can do a binary search)
    sort(m_FbCtxDmas.begin(), m_FbCtxDmas.end());

    // Set display to view the first allocated region big enough to
    // hold it, if any.
    //
    GpuUtility::MemoryChunkDesc *pDisplayChunk = nullptr;
    UINT64 displayOffset = 0;
    RC trc = GpuUtility::FindDisplayableChunk(GetChunks(),
                                              &pDisplayChunk,
                                              &displayOffset,
                                              GetStartLocation(),
                                              GetEndLocation(),
                                              m_Mode.height, m_Pitch,
                                              GetBoundGpuDevice());
    if (trc == OK)
    {
        CHECK_RC(GetDisplayMgrTestContext()->DisplayMemoryChunk(
            pDisplayChunk,
            displayOffset,
            m_Mode.width,
            m_Mode.height,
            m_Mode.depth,
            m_Pitch));
    }

    return OK;
}

RC FastMatsTest::InstantiateObjects()
{
    m_pCh->SetObject(s_SubTwoD, m_TwoD.GetHandle());
    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT,
                            LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32);

    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN_FORMAT,
                            LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_CGA6_M1);

    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN0, 0xaaaaaaaa);
    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN1, 0xaaaaaaaa);

    m_pCh->Write(s_SubTwoD, LW502D_SET_PATTERN_OFFSET,
                 DRF_NUM(502D, _SET_PATTERN_OFFSET, _X, 0) |
                 DRF_NUM(502D, _SET_PATTERN_OFFSET, _Y, 0));

    m_pCh->Write(s_SubTwoD, LW502D_SET_PATTERN_OFFSET, 0);
    m_pCh->Write(s_SubTwoD, LW502D_SET_PATTERN_SELECT,
                            LW502D_SET_PATTERN_SELECT_V_MONOCHROME_64x1);

    m_pCh->Write(s_SubTwoD,
         LW502D_SET_SRC_MEMORY_LAYOUT,
         LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH);
    m_pCh->Write(s_SubTwoD,
         LW502D_SET_DST_MEMORY_LAYOUT,
         LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH);

    m_pCh->Write(s_SubTwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH,
                 m_BoxWidth);
    m_pCh->Write(s_SubTwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT,
                 m_BoxHeight);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_WIDTH, m_BoxWidth);
    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_WIDTH, m_BoxWidth);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_HEIGHT, m_BoxHeight);
    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_HEIGHT, m_BoxHeight);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_FORMAT, LW502D_SET_SRC_FORMAT_V_Y32);
    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_FORMAT, LW502D_SET_DST_FORMAT_V_Y32);

    m_pCh->Write(s_SubTwoD, LW502D_SET_ROP, 0xf0); // copy pattern across memory
    m_pCh->Write(s_SubTwoD, LW502D_SET_OPERATION,
                 LW502D_SET_OPERATION_V_ROP_AND);

    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_PITCH, m_Pitch);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_PITCH, m_Pitch);

    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_OFFSET_UPPER, 0);
    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_OFFSET_LOWER, 0);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_OFFSET_UPPER, 0);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_OFFSET_LOWER, 0);

    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_WIDTH,  m_Mode.width);
    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_HEIGHT, m_Mode.height);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_WIDTH,  m_Mode.width);
    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_HEIGHT, m_Mode.height);

    m_pCh->Write(s_SubTwoD, LW502D_RENDER_SOLID_PRIM_MODE,
                            LW502D_RENDER_SOLID_PRIM_MODE_V_RECTS);

    m_pCh->Write(s_SubTwoD, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT,
                            LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y32);

    // blue, should never actually get rendered
    m_pCh->Write(s_SubTwoD, LW502D_SET_RENDER_SOLID_PRIM_COLOR, 0xFF);

    m_Notifier2d.Instantiate(s_SubTwoD);

    RC rc;
    // Instantiate Object
    CHECK_RC(m_pCeCh->SetObject(s_SubCE, m_CopyEngine.GetHandle()));
    if (GF100_DMA_COPY == m_CopyEngine.GetClass())
    {
        // Set application for copy engine
        CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_SET_APPLICATION_ID,
                        DRF_DEF(90B5, _SET_APPLICATION_ID, _ID, _NORMAL)));
    }
    return OK;
}

//------------------------------------------------------------------------------
bool FastMatsTest::IsSupported()
{
    if (!GpuMemTest::IsSupported())
    {
        return false;
    }
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    return m_CopyEngine.IsSupported(pGpuDevice) &&
           m_TwoD.IsSupported(pGpuDevice);
}

//------------------------------------------------------------------------------
RC FastMatsTest::Setup()
{
    RC rc;
    CHECK_RC(GpuMemTest::Setup());

    CHECK_RC(GpuTest::AllocDisplay());

    // Update parameters to match the video mode.
    m_Pitch = m_Mode.width * m_Mode.depth / 8;
    CHECK_RC(AdjustPitchForScanout(&m_Pitch));

    // Ensure that if the RM doesn't know the pitch,
    //  MODS does not crash. m_Pitch is set in CallwlateBoxes
    if (m_Pitch == 0)
    {
        Printf(Tee::PriNormal, "Error: RM cannot determine pitch.\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 pitchInPixels = m_Pitch / 4;

    m_BoxWidth = 1024;

    // LW5 can come out with a surface pitch that is an odd multiple of 32
    // pixels.  In this case, m_BoxWidth doesn't divide evenly.  So, let's
    // use 32-pixel-wide boxes.  This problem usually happens in 800x600.
    if (pitchInPixels % m_BoxWidth)
    {
        m_BoxWidth /= 2;

        // If we still don't divide evenly, something is really wrong.  Bail.
        if (pitchInPixels % m_BoxWidth)
        {
            Printf(Tee::PriNormal, "Surface pitch does not divide evenly.\n");
            CHECK_RC(RC::UNSUPPORTED_FUNCTION);
        }
    }

    m_BoxHeight        = 64;
    m_XferSize         = m_BoxWidth * m_BoxHeight;
    m_BoxWidthBytes    = 4 * m_BoxWidth; // *4 colwerts 32bpp to bytes
    m_XferSizeBytes    = 4 * m_XferSize; // *4 colwerts 32bpp to bytes
    m_BoxesPerRow      = m_Pitch / m_BoxWidthBytes;
    m_BoxesPerMegabyte = static_cast<UINT32>(1_MB / m_XferSizeBytes);
    m_BytesPerBoxRow   = m_Pitch * m_BoxHeight;

    m_pTestConfig->SetAllowMultipleChannels(true);
    CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCeCh,
                                                      &m_hCeCh,
                                                      &m_CopyEngine));
    CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                      &m_hCh,
                                                      &m_TwoD));
    Printf(Tee::PriLow, "Allocated TWOD.\n");

    CHECK_RC(AllocateContextDma());
    CHECK_RC(InstantiateObjects());

    CHECK_RC(CallwlateBoxes());

    // Setup static data for parallel threads
    m_ThreadWorkLen = (m_XferSize + m_NumThreads - 1) / m_NumThreads;
    CHECK_RC(m_CheckPCIDataTaskPool.Initialize(m_NumThreads, true));
    m_CheckPCIDataTasks.resize(m_NumThreads, CheckPCIDataTask(this));
    for (UINT32 i = 0; i < m_CheckPCIDataTasks.size(); i++)
    {
        m_CheckPCIDataTasks[i].SetThreadId(i);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC FastMatsTest::Run()
{
    StickyRC firstRc;

    for (UINT32 i = 0; i < m_Iterations; i++)
    {
        firstRc = DoFastMats();
    }

    return firstRc;
}

//------------------------------------------------------------------------------
RC FastMatsTest::Cleanup()
{
    StickyRC firstRc;

    m_CheckPCIDataTaskPool.ShutDown();
    m_CheckPCIDataTasks.clear();
    m_ThreadErrors.clear();

    if (m_pCh)
    {
        firstRc = m_pCh->WaitIdle(m_TimeoutMs);
        m_FbCtxDmas.clear();
        m_TwodSemaphore.Free();
        m_Pci.Free();
        m_TwoD.Free();
        m_Notifier2d.Free();
        m_NotifierMemToMem.Free();
        firstRc = m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = 0;
        m_hCh = 0;
    }
    if (m_pCeCh)
    {
        m_CopyEngine.Free();
        m_CeSemaphore.Free();
        firstRc = m_pTestConfig->FreeChannel(m_pCeCh);
        m_pCeCh = 0;
        m_hCeCh = 0;
    }

    delete[] m_ForwardSwizzle;
    m_ForwardSwizzle = 0;

    delete[] m_BackwardSwizzle;
    m_BackwardSwizzle = 0;

    firstRc = GpuMemTest::Cleanup();
    return firstRc;
}

//------------------------------------------------------------------------------
// main loop...does the fastmats algorithm
RC FastMatsTest::DoFastMats()
{
    RC rc;

    const UINT32 MAX_PAT_LENGTH = 20;
    UINT32 patData[MAX_PAT_LENGTH];

    m_PatternClass.GetPatternName(m_Pattern, &m_PatternName);

    m_PatternClass.FillMemoryWithPattern(
            patData, MAX_PAT_LENGTH, 1, MAX_PAT_LENGTH,
            m_pTestConfig->DisplayDepth(), m_Pattern, &m_NumPatterns);

    if (m_NumPatterns > MAX_PAT_LENGTH-1)
        m_NumPatterns = MAX_PAT_LENGTH-1;

    patData[m_NumPatterns] = END_PATTERN;
    m_DataPatterns = patData;

    // Override patterns if there are user specified paterns
    if (m_UserPatterns)
        m_DataPatterns = m_UserPatterns;

    // fill the memory with the first pattern
    for (UINT32 box = 0; box < m_NumBoxes; box++)
    {
        CHECK_RC(FillBox(m_Swizzle[box], 0));
    }

    // Only run the EndLoop check every 10% of the boxes to avoid slowing down
    // the test to a crawl.
    UINT32 endLoopCheckBoxes = m_NumBoxes / 10;
    if (endLoopCheckBoxes == 0)
        endLoopCheckBoxes = 1;

    for (UINT32 pat = 0; pat < m_NumPatterns; ++pat)
    {
        //Get hostside semaphore;
        CHECK_RC(m_TwodSemaphore.Wait(m_TimeoutMs));  //SK
        // switch the testing order of the boxes with each pass through the loop
        m_Swizzle = (m_Swizzle == m_ForwardSwizzle ?
                     m_BackwardSwizzle : m_ForwardSwizzle);
        // for each box...
        for (UINT32 box = 0; box < m_NumBoxes; box++)
        {
            UINT32 subdev;
            for (subdev = 0; subdev < GetBoundGpuDevice()->GetNumSubdevices();
                    subdev++)
            {
                // ...start to copy the box to PCI memory...
                CHECK_RC(CopyToPci(m_Swizzle[box], 0, subdev));

                if (m_Background)
                {
                    //...FillAlternateBox generates some traffic on
                    //the box to create noise
                    CHECK_RC(FillAlternateBox(box, pat));
                }

                // ...make sure the transfer is complete...
                CHECK_RC(WaitForDmaCompletion());

                // .. then check the contents of the PCI memory...
                CHECK_RC(CheckPciBuf(box, pat, subdev));
            }
            // .. and fill the box with the next pattern
            CHECK_RC(FillBox(m_Swizzle[box], pat+1));

            if (0 == (box % endLoopCheckBoxes))
            {
                const UINT32 donePct = ((pat * m_NumBoxes + box) * 100) /
                    (m_NumBoxes * m_NumPatterns);

                CHECK_RC(EndLoop(donePct));
            }
        }
    }

    return rc;
}

// the purpose of this routine is to generate extra bus activity to create
// noise.  The algorithm will select Pat1 or Pat2 depending on whether the
// Copy-to-pci-space routine has already oclwrred for this particular box
RC FastMatsTest::FillAlternateBox(UINT32 i, UINT32 pat)
{
    UINT32 newLoc = m_NumBoxes - 1 - i;

    if (newLoc > i)
        return FillBox(m_Swizzle[newLoc], pat);
    else
        return FillBox(m_Swizzle[newLoc], pat+1);
}

void FastMatsTest::CheckPCIDataThread(UINT32 tid)
{
    UINT32 *pAddressIn = (UINT32*) m_Pci.GetAddress();

    UINT32 startIdx = tid*m_ThreadWorkLen;
    UINT32 endIdx = min((startIdx + m_ThreadWorkLen), m_XferSize);
    for (UINT32 i = startIdx; i < endIdx; i++)
    {
        UINT32 data = MEM_RD32(&pAddressIn[i]);

        // Decode pattern to expected color
        UINT32 expectedData =
            ((m_ThreadPattern >> (31 - (i % 32))) & 1) ? ~m_ThreadColor : m_ThreadColor;

        if (data != expectedData)
        {
            if (GetNumRechecks())
            {
                m_ThreadErrorFound = true;
                break;
            }

            Tasker::MutexHolder holder(m_ThreadErrorMutex);
            m_ThreadErrors[i] = make_pair(data, expectedData);
        }
    }
}

void FastMatsTest::LaunchCheckPCIDataThreadsSync(UINT32 color, UINT32 pattern)
{
    m_ThreadErrorFound = false;
    m_ThreadErrors.clear();
    m_ThreadColor = color;
    m_ThreadPattern = pattern;

    // Kick off threads
    for (UINT32 i = 0; i < m_CheckPCIDataTasks.size(); i++)
    {
        m_CheckPCIDataTaskPool.EnqueueTask(m_CheckPCIDataTasks[i]);
    }

    // Wait for all threads to complete
    m_CheckPCIDataTaskPool.WaitUntilTasksCompleted(Tasker::NO_TIMEOUT);
}

// Makes sure the contents of the Pci memory is correct
RC FastMatsTest::CheckPciBuf(UINT32 index, UINT32 colorNum, UINT32 subdev)
{
    UINT32 *pAddressIn = (UINT32*) m_Pci.GetAddress();
    UINT32 i;
    RC rc = OK;

    // If graphics is idle, then create some extra bus activity while we are
    // checking the data
    if ((m_Background) && m_Notifier2d.IsSet(0))
    {
        CHECK_RC(FillAlternateBox(index, colorNum));
    }

    // Get pattern, byte swap for expected color
    UINT32 pattern = Utility::ByteSwap32(
            m_DataPatterns[(m_Swizzle[index] + colorNum) % m_NumPatterns]);
    UINT32 color = m_DataPatterns[colorNum];

    bool errorCopiesDone = false;

    LaunchCheckPCIDataThreadsSync(color, pattern);

    // Report errors that were found
    for (const auto &it: m_ThreadErrors)
    {
        UINT32 i = it.first;

        // If an error was detected, we want to callwlate the physical
        // address of the failure.  This is important for two reasons:
        // 1.  We want to go re-read that address again directly from the
        //     framebuffer...this can help the user determine if the
        //     problem was because of a failing read or a failing write
        //
        // 2.  We want to report back to the user what the actual failing
        //     address was.

        // This math callwlates that address.  Note that the 64x64
        // "box" has been spread out into main memory as a 4096x1
        // "box" in system memory.  So, to callwlate the original
        // location we need to take the quotient and modulus to
        // get the original (x, y) coordinates.
        UINT32 xOffset  = (i % m_BoxWidth) * 4;
        UINT32 yOffset  = (i / m_BoxWidth) * m_Pitch;

        UINT64 errorLoc = (BoxNumToFbOffset(m_Swizzle[index]) +
                           xOffset + yOffset);
        FbCtxDma *pSrcCtxDma = GetFbCtxDma(errorLoc);

        // Only do this once.
        if (!errorCopiesDone)
        {
            CHECK_RC(CopyToPci(m_Swizzle[index], 1*m_XferSizeBytes, subdev));
            CHECK_RC(WaitForDmaCompletion());
            CHECK_RC(CopyToPci(m_Swizzle[index], 2*m_XferSizeBytes, subdev));
            CHECK_RC(WaitForDmaCompletion());
            errorCopiesDone = true;
        }

        // re-read the failing address two more times to see how repeatable
        // the error is.  Instead of reading it directly in the framebuffer,
        // repeat the transaction that copied it from fb->sys.
        UINT32 err1 = MEM_RD32(
                &pAddressIn[i + 1*m_XferSizeBytes/sizeof(UINT32)]);
        UINT32 err2 = MEM_RD32(
                &pAddressIn[i + 2*m_XferSizeBytes/sizeof(UINT32)]);

        UINT32 data = it.second.first;
        UINT32 expectedData = it.second.second;

        CHECK_RC(GetMemError(subdev).LogMemoryError(
                        32,
                        errorLoc,
                        data,
                        expectedData,
                        err1,
                        err2,
                        pSrcCtxDma->pteKind,
                        pSrcCtxDma->pageSizeKB,
                        MemError::NO_TIME_STAMP,
                        m_PatternName,
                        colorNum * sizeof(UINT32)));
    }

    // If Rechecking is enabled and errors were detected
    UINT32 readNum = 1;
    if (GetNumRechecks() && m_ThreadErrorFound)
    {
        bool retriesAreInconsistent = false;
        bool thisRetryHadErrors = false;
        for (; readNum < 1 + GetNumRechecks(); readNum++)
        {
            thisRetryHadErrors = false;
            CHECK_RC(CopyToPci(m_Swizzle[index],
                               readNum * m_XferSizeBytes, subdev));
            CHECK_RC(WaitForDmaCompletion());
            for (i = 0; i < m_XferSize; i++)
            {
                UINT32 data = MEM_RD32(
                        &pAddressIn[i + readNum *m_XferSizeBytes/sizeof(UINT32)]);
                UINT32 expectedData = (i % 2) ? color : ~color;
                if (data != expectedData)
                {
                    thisRetryHadErrors = true;
                }

                if (data != MEM_RD32(&pAddressIn[i +
                                     (readNum - 1) * m_XferSizeBytes /
                                     sizeof(UINT32)]))
                {
                    retriesAreInconsistent = true;
                }

            }

            // thisRetryHadErrors is false when last retry passes
            if (!thisRetryHadErrors)
            {
                // Here intermittent error means that retry needed for
                // test success
                SetIntermittentError(true);
                break;
            }

        }

        // thisRetryHadErrors is true means that none of the retry pass.
        // All failed
        if (thisRetryHadErrors)
        {
            for (i=0; i < m_XferSize; i++)
            {
                UINT32 data = MEM_RD32(&pAddressIn[
                        i + GetNumRechecks() *m_XferSizeBytes/sizeof(UINT32)]);
                UINT32 xOffset  = (i % m_BoxWidth) * 4;
                UINT32 yOffset  = (i / m_BoxWidth) * m_Pitch;

                UINT64 errorLoc = (BoxNumToFbOffset(m_Swizzle[index]) +
                                   xOffset + yOffset);
                FbCtxDma *pSrcCtxDma = GetFbCtxDma(errorLoc);

                UINT32 expectedData = (i % 2) ? color : ~color;
                if (data != expectedData)
                {
                    // We assume the DMA to sysmem or the sysmem itself can be
                    // bad. Now the FB "read" vs. "write" distinction can no
                    // longer be made.

                    CHECK_RC(GetMemError(subdev).LogUnknownMemoryError(
                                    32,
                                    errorLoc,
                                    data,
                                    expectedData,
                                    pSrcCtxDma->pteKind,
                                    pSrcCtxDma->pageSizeKB));
                }

            }

            if (retriesAreInconsistent)
            {
                // Errors are not same for all retries.
                // Some retry failed with diffrent error
                SetIntermittentError(true);
            }

        }
    }

    // Poisons buffer so that stale data isn't read if a DMA fails
    // Use MemSet to improve test run time
    Platform::MemSet(pAddressIn, 0xCD,
                     readNum * m_XferSizeBytes);
    return rc;
}

// Return absolute fb offset of the top-left-corner of the given box
UINT64 FastMatsTest::BoxNumToFbOffset(UINT32 boxNum) const
{
    const UINT64 x = (static_cast<UINT64>(boxNum % m_BoxesPerRow) *
                      m_BoxWidthBytes);
    const UINT64 y = (static_cast<UINT64>(boxNum / m_BoxesPerRow) *
                      m_BytesPerBoxRow);
    return x + y;
}

// Given an offset into the framebuffer, find the FbCtxDma that
// contains that offset.  Uses a binary search.
FastMatsTest::FbCtxDma *FastMatsTest::GetFbCtxDma(UINT64 fbOffset)
{
    size_t lo = 0;
    size_t hi = m_FbCtxDmas.size();
    while (lo < hi)
    {
        size_t mid = (lo + hi) / 2;
        FbCtxDma *pFbCtxDma = &m_FbCtxDmas[mid];
        if (fbOffset < pFbCtxDma->offset)
            hi = mid;
        else if (fbOffset >= pFbCtxDma->offset + pFbCtxDma->size)
            lo = mid + 1;
        else
            return pFbCtxDma;
    }
    MASSERT(!"Illegal offset passed into FastMatsTest::GetFbCtxDma");
    return NULL;
}

RC FastMatsTest::CopyEngineCopyToPci(UINT64 srcOffset64,
                                     UINT64 dstOffset64,
                                     UINT32 subdevMask)
{
    RC rc;

    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        m_pCeCh->WriteSetSubdevice(subdevMask);

    m_CeSemaphore.SetPayload(0);

    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_OFFSET_IN_UPPER,
        2,
        static_cast<UINT32>(srcOffset64 >> 32), /* LW90B5_OFFSET_IN_UPPER */
        static_cast<UINT32>(srcOffset64))); /* LW90B5_OFFSET_IN_LOWER */

    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_OFFSET_OUT_UPPER,
        2,
        static_cast<UINT32>(dstOffset64 >> 32), /* LW90B5_OFFSET_OUT_UPPER */
        static_cast<UINT32>(dstOffset64))); /* LW90B5_OFFSET_OUT_LOWER */

    UINT32 layoutMem = 0;
    layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
    layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);

    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_PITCH_IN,  m_Pitch));
    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_PITCH_OUT, m_Pitch/m_BoxesPerRow));

    if (GF100_DMA_COPY == m_CopyEngine.GetClass())
    {
        UINT32 addressMode = 0;
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL);
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL);
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _LOCAL_FB);
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET,
                               _COHERENT_SYSMEM);
        CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_ADDRESSING_MODE, addressMode));
    }
    else
    {
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
    }

    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_LINE_LENGTH_IN, m_BoxWidthBytes));
    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_LINE_COUNT, m_BoxHeight));

    CHECK_RC(m_pCeCh->Write(s_SubCE,
        LW90B5_SET_SEMAPHORE_A,
        2,
        LwU64_HI32(m_CeSemaphore.GetCtxDmaOffsetGpu(0)), /* LW90B5_SET_SEMAPHORE_A */
        LwU64_LO32(m_CeSemaphore.GetCtxDmaOffsetGpu(0)))); /* LW90B5_SET_SEMAPHORE_B */

    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_SET_SEMAPHORE_PAYLOAD,
                            s_CePayload));

    CHECK_RC(m_pCeCh->Write(s_SubCE, LW90B5_LAUNCH_DMA,
                            layoutMem |
                            DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                                    _RELEASE_ONE_WORD_SEMAPHORE) |
                            DRF_DEF(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                            DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                            DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                                    _NON_PIPELINED) |
                            DRF_DEF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE,
                                    _TRUE)));

    // Restore the default "all subdevices" mask in the channel.
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        m_pCeCh->WriteSetSubdevice(Channel::AllSubdevicesMask);

    m_pCeCh->Flush();
    return OK;
}

// copies the data in the given box to PCI memory
RC FastMatsTest::CopyToPci(UINT32 boxNum, UINT32 destOffsetBytes, UINT32 subdev)
{
    RC rc;

    // We'll have to figure out what to do for GetCtxDmaOffsetGpu() if we add
    // block linear support
    UINT64 srcFbOffset = BoxNumToFbOffset(boxNum);
    FbCtxDma *pSrcCtxDma = GetFbCtxDma(srcFbOffset);

    UINT64 srcOffset64 = (srcFbOffset - pSrcCtxDma->offset) +
                         pSrcCtxDma->ctxDmaOffsetGpu;
    UINT64 dstOffset64 = m_Pci.GetCtxDmaOffsetGpu() + destOffsetBytes;

    // For SLI devices, send methods to only one of the gpus.
    UINT32 subdevMask = 1 << subdev;

    CHECK_RC(CopyEngineCopyToPci(srcOffset64, dstOffset64, subdevMask));
    return rc;
}

// wait for the class 4A (GDI_RECT_TEXT) notifier
RC FastMatsTest::Wait4aNotifier()
{
    return m_Notifier2d.Wait(0, m_TimeoutMs);
}
// check the class 4A (GDI_RECT_TEXT) notifier
bool FastMatsTest::Check4aNotifier()
{
    return m_Notifier2d.IsSet(0);
}

RC FastMatsTest::WaitCeNotifier()
{
    m_CeSemaphore.SetOneTriggerPayload(0, s_CePayload);
    return m_CeSemaphore.Wait(m_TimeoutMs);
}

RC FastMatsTest::WaitForDmaCompletion()
{
    RC rc;
    CHECK_RC(WaitCeNotifier());
    return rc;
}

// Fill the given box with the given color
RC FastMatsTest::FillBox(UINT32 boxNum, UINT32 colorNum)
{
    RC rc = OK;

    UINT64 fbOffset = BoxNumToFbOffset(boxNum);
    FbCtxDma *pCtxDma = GetFbCtxDma(fbOffset);
    UINT64 dmaOffset = (fbOffset - pCtxDma->offset) + pCtxDma->ctxDmaOffsetGpu;

    UINT32 color = m_DataPatterns[colorNum];
    UINT32 pattern = m_DataPatterns[(boxNum + colorNum) % m_NumPatterns];

    m_Notifier2d.Clear(0);

    switch (m_TwoD.GetClass())
    {
        case LW50_TWOD:
            m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_CONTEXT_DMA,
                         pCtxDma->handle);
            m_pCh->Write(s_SubTwoD, LW502D_SET_DST_CONTEXT_DMA,
                         pCtxDma->handle);
            break;
        default:
            break;
    }

    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_OFFSET_UPPER,
        (UINT32)(dmaOffset >> 32));
    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_OFFSET_UPPER,
        (UINT32)(dmaOffset >> 32));

    m_pCh->Write(s_SubTwoD, LW502D_SET_SRC_OFFSET_LOWER, (UINT32)dmaOffset);
    m_pCh->Write(s_SubTwoD, LW502D_SET_DST_OFFSET_LOWER, (UINT32)dmaOffset);

    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN0, pattern);
    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN1, pattern);

    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN_COLOR0, color);
    m_pCh->Write(s_SubTwoD, LW502D_SET_MONOCHROME_PATTERN_COLOR1, ~color);

    m_pCh->Write(s_SubTwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_X0, 0);
    m_pCh->Write(s_SubTwoD, LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT, 0);

    m_Notifier2d.Write(s_SubTwoD);
    m_pCh->Write(s_SubTwoD, LW502D_NO_OPERATION, 0);

    // action method
    m_pCh->Write(s_SubTwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0, 0);
    m_pCh->Write(s_SubTwoD, LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT, 0);

    // Set hostside semaphore;
    m_TwodPayload++; //Change value before every set
    Printf(Tee::PriLow, "\tBoxNum %d GpuInst %u\n",
            boxNum, GetBoundGpuSubdevice()->GetGpuInst());
    m_TwodSemaphore.SetPayload(0);
    m_TwodSemaphore.SetTriggerPayload(m_TwodPayload);
    m_pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelWithWFI);
    CHECK_RC(m_pCh->SemaphoreRelease(
                m_TwodSemaphore.GetTriggerPayload(0)));

    m_pCh->Flush(); // Flush pending insctructions

    if (!m_Background)
        CHECK_RC(Wait4aNotifier());
    return OK;
}

//------------------------------------------------------------------------------
// Initialize the properties.
//------------------------------------------------------------------------------

RC FastMatsTest::InitFromJs()
{
    RC rc;

    CHECK_RC(GpuMemTest::InitFromJs());

    m_PatternClass.FillRandomPattern(m_pTestConfig->Seed());

    m_TimeoutMs       = m_pTestConfig->TimeoutMs();
    m_Mode.width      = m_pTestConfig->SurfaceWidth();
    m_Mode.height     = m_pTestConfig->SurfaceHeight();
    m_Mode.depth      = m_pTestConfig->DisplayDepth();
    m_Mode.refreshRate= m_pTestConfig->RefreshRate();

    if (m_pTestConfig->UseTiledSurface())
    {
        Printf(Tee::PriLow,
               "NOTE: ignoring TestConfiguration.UseTiledSurface.\n");
    }

    return OK;
}

void FastMatsTest::PrintJsProperties(Tee::Priority pri)
{
    GpuMemTest::PrintJsProperties(pri);

    const char * ft[] = {"false", "true"};

    Printf(pri, "\tCoverage:\t\t\t%f\n", m_Coverage);
    Printf(pri, "\tIterations:\t\t\t%d\n", m_Iterations);
    Printf(pri, "\tBackground:\t\t\t%s\n", ft[m_Background]);
    Printf(pri, "\tPattern:\t\t\t0x%x\n", m_Pattern);
}

RC FastMatsTest::CallwlateBoxes()
{
    Printf(Tee::PriLow, "FastMats CallwlateBoxes()\n");
    Printf(Tee::PriLow, "m_Pitch            = %d\n", m_Pitch           );
    Printf(Tee::PriLow, "m_BoxesPerRow      = %d\n", m_BoxesPerRow     );
    Printf(Tee::PriLow, "m_BoxesPerMegabyte = %d\n", m_BoxesPerMegabyte);
    Printf(Tee::PriLow, "m_BytesPerBoxRow   = %d\n", m_BytesPerBoxRow  );

    // Figure out which boxes need to be tested.  The boxes are
    // numbered as if the entire framebuffer was one big display; even
    // the parts that we can't test.
    vector<UINT32> startBoxes;
    vector<UINT32> endBoxes;
    UINT32 fullNumBoxes = 0;

    for (size_t ii = 0; ii < m_FbCtxDmas.size(); ii++)
    {
        const UINT32 startBox = static_cast<UINT32>(m_BoxesPerRow *
                ((m_FbCtxDmas[ii].offset + m_BytesPerBoxRow - 1) /
                 m_BytesPerBoxRow));
        const UINT32 endBox = static_cast<UINT32>(m_BoxesPerRow *
                ((m_FbCtxDmas[ii].offset + m_FbCtxDmas[ii].size) /
                 m_BytesPerBoxRow));

        if (endBox > startBox)
        {
            startBoxes.push_back(startBox);
            endBoxes.push_back(endBox);
            fullNumBoxes += endBox - startBox;
        }
    }
    MASSERT(startBoxes.size() == endBoxes.size());

    m_NumBoxes = static_cast<UINT32>(fullNumBoxes * m_Coverage / 100.0);

    // number of boxes need to be even. Fix the alignment: bug 896445
    m_NumBoxes &= ~(0x1U);

    MASSERT(m_NumBoxes > 0 && m_NumBoxes <= fullNumBoxes);

    // create an array of the box numbers to be tested, and then shuffle it
    m_ForwardSwizzle  = new UINT32[fullNumBoxes];
    m_BackwardSwizzle = new UINT32[m_NumBoxes];
    m_Swizzle         = m_ForwardSwizzle;

    if ((0 == m_ForwardSwizzle) || (0 == m_BackwardSwizzle))
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    fullNumBoxes = 0;
    for (size_t ii = 0; ii < startBoxes.size(); ii++)
    {
        for (UINT32 box = startBoxes[ii]; box < endBoxes[ii]; box++)
        {
            m_ForwardSwizzle[fullNumBoxes++] = box;
        }
    }

    Random * pRandom = &(GetFpContext()->Rand);
    pRandom->SeedRandom(m_pTestConfig->Seed());
    pRandom->Shuffle(fullNumBoxes, m_ForwardSwizzle);

    // make the BackwareSwizzle array the reverse of the ForwardSwizzle array
    for (UINT32 i=0; i<m_NumBoxes; i++)
    {
        m_BackwardSwizzle[i]= m_ForwardSwizzle[m_NumBoxes - i - 1];
    }

    return OK;
}
