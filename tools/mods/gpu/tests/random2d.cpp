/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

/**
 * @file   random2d.cpp
 * @brief  Implement classes for the Random2d graphics test.
 */

#include "random2d.h"
#include "core/include/jscript.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "random.h"
#include "core/include/tee.h"
#include "core/include/platform.h"
#include <math.h>
#include "gpu/include/gpugoldensurfaces.h"
#include "core/include/circsink.h"
#include "Lwcm.h"
#include "lwos.h"
#include "class/cl502d.h" // LW50_TWOD
#include "class/cl902d.h" // FERMI_TWOD_A
#include "core/include/memcheck.h"

// @@@ To Do list for Random2d:
//
// Add support for block-linear surfaces
//  - JS options to control features
//  - many other tests will need this, common interface?
//  - Goldelwalues needs updated for this also
//
// Need better handling of bad picker syntax from .js.
//
// LW50_TWOD open issues:
//  - Conditional rendering support (need semaphore surface)

// Some strings shared by various Print functions:

static const char * strFT[] =
{
    "false",
    "true"
};
const char * strLayout[] =
{
    "Pitch",
    "Swizzled",
    "BlockLinear"
};
const char * strPattSelects[] =
{
    "8x8",
    "64x1",
    "1x64",
    "color"
};
const char * strMonoFormats[] =
{
    "?zero!",
    "CGA6_M1",
    "LE_M1"
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PickHelper::PickHelper(Random2d * pRnd2d, int numPickers, GrClassAllocator * pGrAlloc) :
    m_Pickers(numPickers),
    m_pRandom2d(pRnd2d),
    m_pGrAlloc(pGrAlloc)
{
    MASSERT(m_pRandom2d);
    MASSERT(m_pGrAlloc);
}

//------------------------------------------------------------------------------
PickHelper::~PickHelper()
{
    delete m_pGrAlloc;  // Derived class constructor alloc'd this on the heap.
}

//------------------------------------------------------------------------------
RC PickHelper::PickerFromJsval(int index, jsval value)
{
    return m_Pickers.PickerFromJsval(index, value);
}

//------------------------------------------------------------------------------
RC PickHelper::PickerToJsval(int index, jsval * pvalue)
{
    return m_Pickers.PickerToJsval(index, pvalue);
}

//------------------------------------------------------------------------------
RC PickHelper::PickToJsval(int index, jsval * pvalue)
{
    return m_Pickers.PickToJsval(index, pvalue);
}

//------------------------------------------------------------------------------
/*virtual*/
bool PickHelper::IsSupported(GpuDevice *pGpuDev) const
{
    return m_pGrAlloc->IsSupported(pGpuDev);
}

//------------------------------------------------------------------------------
/*virtual*/
void PickHelper::Restart(void)
{
    m_Pickers.Restart();
}

//------------------------------------------------------------------------------
/*virtual*/
RC PickHelper::CopySrcToDst(Channel * pCh)
{
    MASSERT(!"PickHelper::CopySrcToDst not implemented in base class.");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
/*virtual*/
RC PickHelper::CopyHostToSrc(Channel * pCh)
{
    MASSERT(!"PickHelper::CopyHostToSrc not implemented in base class");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
/*virtual*/
RC PickHelper::ClearScreen(Channel * pCh, UINT32 DacColorDepth)
{
    MASSERT(!"PickHelper::ClearScreen not implemented in base class");
    return RC::SOFTWARE_ERROR;
}

//------------------------------------------------------------------------------
LwRm::Handle PickHelper::GetHandle() const
{
    return m_pGrAlloc->GetHandle();
}

//------------------------------------------------------------------------------
UINT32 PickHelper::GetClass() const
{
    return m_pGrAlloc->GetClass();
}

//------------------------------------------------------------------------------
void PickHelper::SetClassOverride(UINT32 requestedClass)
{
    m_pGrAlloc->SetClassOverride(requestedClass);
}

//------------------------------------------------------------------------------
void PickHelper::CheckInitialized()
{
    m_Pickers.CheckInitialized();
}

//------------------------------------------------------------------------------
void PickHelper::SetContext(FancyPicker::FpContext * pFpCtx)
{
    m_Pickers.SetContext(pFpCtx);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MiscPickHelper::MiscPickHelper(Random2d * pRnd2d) :
    PickHelper(pRnd2d, RND2D_MISC_NUM_PICKERS, new TwodAlloc /* unused */)
{
    m_DrawClass = 0;
    m_ForceNotify = false;
}

//------------------------------------------------------------------------------
/* virtual */
MiscPickHelper::~MiscPickHelper()
{
}

//------------------------------------------------------------------------------
/*virtual*/
RC MiscPickHelper::Alloc
(
    GpuTestConfiguration * pTestConfig,
    Channel             ** ppCh,
    LwRm::Handle         * phCh
)
{
    return OK;
}

//------------------------------------------------------------------------------
/*virtual*/
RC MiscPickHelper::Instantiate(Channel * pCh)
{
    return OK;
}

//------------------------------------------------------------------------------
/*virtual*/
void MiscPickHelper::Free(void)
{
}

//------------------------------------------------------------------------------
/* virtual */
void MiscPickHelper::SetDefaultPicker(UINT32 first, UINT32 last)
{
    switch (first)
    {
        case RND2D_MISC_SWAP_SRC_DST:
            if (static_cast<int>(last) < RND2D_MISC_SWAP_SRC_DST) break;
            // (pick once per frame) do/don't swap src and dest (to allow rendering to either tiled or untiled)
            m_Pickers[RND2D_MISC_SWAP_SRC_DST].ConfigRandom();
            m_Pickers[RND2D_MISC_SWAP_SRC_DST].AddRandRange(1, 0, 1);
            m_Pickers[RND2D_MISC_SWAP_SRC_DST].CompileRandom();
            m_Pickers[RND2D_MISC_SWAP_SRC_DST].SetPickOnRestart(true);

        case RND2D_MISC_STOP:
            if (last < RND2D_MISC_STOP) break;
            // (PickFirst) stop testing, return current pass/fail code
            m_Pickers[RND2D_MISC_STOP].ConfigConst(false);

        case RND2D_MISC_DRAW_CLASS:
            if (last < RND2D_MISC_DRAW_CLASS) break;
            // (PickFirst) which draw object (twod/etc) to use this loop
            m_Pickers[RND2D_MISC_DRAW_CLASS].ConfigRandom();
            m_Pickers[RND2D_MISC_DRAW_CLASS].AddRandRange(1, RND2D_FIRST_DRAW_CLASS, RND2D_NUM_RANDOMIZERS - 1);
            m_Pickers[RND2D_MISC_DRAW_CLASS].CompileRandom();

        case RND2D_MISC_NOTIFY:
            if (last < RND2D_MISC_NOTIFY) break;
            // (PickFirst) do/don't set and wait for a notifier at the end of the draw
            m_Pickers[RND2D_MISC_NOTIFY].ConfigRandom();
            m_Pickers[RND2D_MISC_NOTIFY].AddRandItem(1, true);
            m_Pickers[RND2D_MISC_NOTIFY].AddRandItem(9, false);
            m_Pickers[RND2D_MISC_NOTIFY].CompileRandom();

        case RND2D_MISC_FORCE_BIG_SIMPLE:
            if (last < RND2D_MISC_FORCE_BIG_SIMPLE) break;
            // (PickFirst) do/don't force non-overlapping draws (for debugging).
            m_Pickers[RND2D_MISC_FORCE_BIG_SIMPLE].ConfigConst(false);

        case RND2D_MISC_SUPERVERBOSE:
            if (last < RND2D_MISC_SUPERVERBOSE) break;
            // (PickLast) do/don't print picks just before sending them
            m_Pickers[RND2D_MISC_SUPERVERBOSE].ConfigConst(false);

        case RND2D_MISC_SKIP:
            if (last < RND2D_MISC_SKIP) break;
            // (PickLast) do/don't draw (if returns nonzero/true, nothing is sent to HW this loop)
            m_Pickers[RND2D_MISC_SKIP].ConfigConst(false);

        case RND2D_MISC_DST_FILL_COLOR:
            if (last < RND2D_MISC_DST_FILL_COLOR) break;
            // random table for the once-per-restart single-color fill of the destination surface
            m_Pickers[RND2D_MISC_DST_FILL_COLOR].ConfigRandom();
            m_Pickers[RND2D_MISC_DST_FILL_COLOR].AddRandItem(1, 0);                // 25% black
            m_Pickers[RND2D_MISC_DST_FILL_COLOR].AddRandItem(1, 0xffffffff);       // 25% white
            m_Pickers[RND2D_MISC_DST_FILL_COLOR].AddRandRange(5, 0, 0xffffffff);   // 50% random
            m_Pickers[RND2D_MISC_DST_FILL_COLOR].CompileRandom();
            m_Pickers[RND2D_MISC_DST_FILL_COLOR].SetPickOnRestart(true);

        case RND2D_MISC_SLEEPMS:
            if (last < RND2D_MISC_SLEEPMS) break;
            // (PickFirst) milliseconds to sleep after each channel flush.
            m_Pickers[RND2D_MISC_SLEEPMS].ConfigConst(0);

        case RND2D_MISC_FLUSH:
            if (last < RND2D_MISC_FLUSH) break;
            // (PickLast) do/don't flush even if not notifying this loop.
            m_Pickers[RND2D_MISC_FLUSH].ConfigConst(0);

        case RND2D_MISC_EXTRA_NOOPS:
            if (last < RND2D_MISC_EXTRA_NOOPS) break;
            // (PickLast) number of NOOP commands to add after each loop.
            m_Pickers[RND2D_MISC_EXTRA_NOOPS].ConfigConst(0);
    }
}

//------------------------------------------------------------------------------
/* virtual */
void MiscPickHelper::Pick()
{
}

//------------------------------------------------------------------------------
/* virtual */
void MiscPickHelper::Print(Tee::Priority pri) const
{
}

//------------------------------------------------------------------------------
/* virtual */
RC MiscPickHelper::Send(Channel * pCh)
{
    return OK;
}

//------------------------------------------------------------------------------
void MiscPickHelper::PickFirst()
{
    // Choose things that the other pickers might need to know.
    m_Pickers[RND2D_MISC_STOP            ].Pick();
    m_Pickers[RND2D_MISC_NOTIFY          ].Pick();
    m_Pickers[RND2D_MISC_FORCE_BIG_SIMPLE].Pick();
    m_Pickers[RND2D_MISC_SLEEPMS         ].Pick();

    m_DrawClass = m_Pickers[RND2D_MISC_DRAW_CLASS].Pick();

    if ((m_DrawClass < RND2D_FIRST_DRAW_CLASS) ||
        (m_DrawClass >= RND2D_NUM_RANDOMIZERS))
    {
        m_DrawClass = m_pRandom2d->GetRandom(RND2D_FIRST_DRAW_CLASS, RND2D_NUM_RANDOMIZERS-1);
    }

    // Don't use a class where no version was found that was compatible
    // with the current GPU.
    while (!m_pRandom2d->IsAllocated(m_DrawClass))
    {
        m_DrawClass = m_DrawClass + 1;
        if (m_DrawClass >= RND2D_NUM_RANDOMIZERS)
            m_DrawClass = RND2D_FIRST_DRAW_CLASS;
    }
}

//------------------------------------------------------------------------------
void MiscPickHelper::PickLast()
{
    // Now that the other pickers have picked, choose whether or not to send
    // this loop to the hardware, and whether or not to stop the test, etc.
    m_Pickers[RND2D_MISC_SKIP        ].Pick();
    m_Pickers[RND2D_MISC_SUPERVERBOSE].Pick();
    m_Pickers[RND2D_MISC_FLUSH       ].Pick();
    m_Pickers[RND2D_MISC_EXTRA_NOOPS ].Pick();
}

//------------------------------------------------------------------------------
void MiscPickHelper::ForceNotify(bool t)
{
    m_ForceNotify = t;
}

//------------------------------------------------------------------------------
bool MiscPickHelper::Stop(void) const
{
    return(0 != m_Pickers[RND2D_MISC_STOP].GetPick());
}

//------------------------------------------------------------------------------
bool MiscPickHelper::Skip(void) const
{
    return(0 != m_Pickers[RND2D_MISC_SKIP].GetPick());
}

//------------------------------------------------------------------------------
bool MiscPickHelper::SuperVerbose(void) const
{
    return(0 != m_Pickers[RND2D_MISC_SUPERVERBOSE].GetPick());
}

//------------------------------------------------------------------------------
UINT32 MiscPickHelper::WhoDraws(void) const
{
    return m_DrawClass;
}

//------------------------------------------------------------------------------
bool MiscPickHelper::Notify(void) const
{
    if (m_ForceNotify)
        return true;

    return(0 != m_Pickers[RND2D_MISC_NOTIFY].GetPick());
}

//------------------------------------------------------------------------------
bool MiscPickHelper::Flush(void) const
{
    if (Notify())
        return true;

    return(0 != m_Pickers[RND2D_MISC_FLUSH].GetPick());
}

//------------------------------------------------------------------------------
bool MiscPickHelper::SwapSrcDst(void) const
{
    return(0 != m_Pickers[RND2D_MISC_SWAP_SRC_DST].GetPick());
}

//------------------------------------------------------------------------------
UINT32 MiscPickHelper::ClearColor() const
{
    return m_Pickers[RND2D_MISC_DST_FILL_COLOR].GetPick();
}

//------------------------------------------------------------------------------
UINT32 MiscPickHelper::SleepMs() const
{
    return m_Pickers[RND2D_MISC_SLEEPMS].GetPick();
}

//------------------------------------------------------------------------------
UINT32 MiscPickHelper::Noops() const
{
    return m_Pickers[RND2D_MISC_EXTRA_NOOPS].GetPick();
}

//------------------------------------------------------------------------------
bool MiscPickHelper::ForceBigSimple(void) const
{
    return(0 != m_Pickers[RND2D_MISC_FORCE_BIG_SIMPLE].GetPick());
}

//------------------------------------------------------------------------------
// In the debug mode "ForceBigSimple", some draw classes (ideally all...)
// will arrange to never overlap their draws in a frame.
// This makes it easier to track a miscomparing pixel in a dumped .TGA file
// back to the loop that drew it.
// This function tells the other helpers which part of the output surface
// they are allowed to draw into.
//
void MiscPickHelper::GetBigSimpleBoundingBox(MiscPickHelper::BoundingBox * pbb)
{
    UINT32 loopsPerFrame = m_pRandom2d->GetLoopsPerFrame();
    if (!loopsPerFrame)
        loopsPerFrame = 1;

    UINT32 colsPerFrame = (UINT32)(sqrt((double)loopsPerFrame) + 0.99);
    if (!colsPerFrame)
        colsPerFrame = 1;

    UINT32 rowsPerFrame = (loopsPerFrame + colsPerFrame - 1) / colsPerFrame;

    const Surface2D *miDst;
    miDst = m_pRandom2d->GetMemoryInfo(Random2d::DstMem);

    UINT32 lineBytes = miDst->GetPitch();
    if (0 == lineBytes)
        lineBytes = miDst->GetWidth() * miDst->GetBytesPerPixel();

    UINT32 colW = lineBytes / colsPerFrame;
    MASSERT(miDst->GetSize() < 0xFFFFFFFF);
    UINT32 rowH = UINT32( (miDst->GetSize() / lineBytes) / rowsPerFrame );

    // @@@ Everything in this function above here could be callwlated once per frame to save CPU time.

    UINT32 loopOfFrame = m_pRandom2d->GetLoop() % loopsPerFrame;
    UINT32 col = loopOfFrame % colsPerFrame;
    UINT32 row = loopOfFrame / colsPerFrame;

    pbb->Offset = (row * rowH * lineBytes) + (col * colW);
    pbb->Width  = colW;
    pbb->Height = rowH;
}

//------------------------------------------------------------------------------
// LW50_TWOD encapsulates all 2D operations. It handles points, lines,
// triangles, rects, scaled or indexed images from pushbuffer data, and
// scaled images from memory.
//------------------------------------------------------------------------------
TwodPickHelper::TwodPickHelper(Random2d * pRnd2d) :
    PickHelper(pRnd2d, RND2D_TWOD_NUM_PICKERS, new TwodAlloc)
{
}

//------------------------------------------------------------------------------
/* virtual */
TwodPickHelper::~TwodPickHelper()
{
}

//------------------------------------------------------------------------------
/*virtual*/
RC TwodPickHelper::Alloc
(
    GpuTestConfiguration * pTestConfig,
    Channel             ** ppCh,
    LwRm::Handle         * phCh
)
{
    return pTestConfig->AllocateChannelWithEngine(ppCh,
                                                  phCh,
                                                  m_pGrAlloc);
}

//------------------------------------------------------------------------------
/*virtual*/
void TwodPickHelper::Free()
{
    m_pGrAlloc->Free();
}

//------------------------------------------------------------------------------
/* virtual */
void TwodPickHelper::SetDefaultPicker(UINT32 first, UINT32 last)
{
    switch (first)
    {
        case RND2D_TWOD_PRIM:
            if (static_cast<int>(last) < RND2D_TWOD_PRIM) break;
            // Which 2d primitive to draw.
            m_Pickers[RND2D_TWOD_PRIM].ConfigRandom();
            m_Pickers[RND2D_TWOD_PRIM].AddRandItem(1,  RND2D_TWOD_PRIM_Points);
            m_Pickers[RND2D_TWOD_PRIM].AddRandItem(1,  RND2D_TWOD_PRIM_Lines);
            m_Pickers[RND2D_TWOD_PRIM].AddRandItem(1,  RND2D_TWOD_PRIM_PolyLine);
            m_Pickers[RND2D_TWOD_PRIM].AddRandItem(1,  RND2D_TWOD_PRIM_Triangles);
            m_Pickers[RND2D_TWOD_PRIM].AddRandItem(1,  RND2D_TWOD_PRIM_Rects);
            m_Pickers[RND2D_TWOD_PRIM].AddRandItem(1,  RND2D_TWOD_PRIM_Imgcpu);
            m_Pickers[RND2D_TWOD_PRIM].AddRandItem(1,  RND2D_TWOD_PRIM_Imgmem);
            m_Pickers[RND2D_TWOD_PRIM].CompileRandom();

        case RND2D_TWOD_SURF_FORMAT:
            if (last < RND2D_TWOD_SURF_FORMAT) break;
            // LW502D_SET_DST_FORMAT and LW502D_SET_SRC_FORMAT
            m_Pickers[RND2D_TWOD_SURF_FORMAT].ConfigRandom();
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8R8G8B8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8RL8GL8BL8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A2R10G10B10 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8B8G8R8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8BL8GL8RL8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A2B10G10R10 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8R8G8B8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8RL8GL8BL8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8B8G8R8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8BL8GL8RL8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_R5G6B5 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A1R5G5B5 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X1R5G5B5 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y16 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y32 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Z1R5G5B5 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_O1R5G5B5 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Z8R8G8B8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_O8R8G8B8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y1_8X8 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF16 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32_GF32 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_X16 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_X32 );
            m_Pickers[RND2D_TWOD_SURF_FORMAT].CompileRandom();

        case RND2D_TWOD_FERMI_SURF_FORMAT:
            if (last < RND2D_TWOD_FERMI_SURF_FORMAT) break;
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].ConfigRandom();

            // LW50_TWOD
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8R8G8B8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8RL8GL8BL8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A2R10G10B10 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8B8G8R8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A8BL8GL8RL8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A2B10G10R10 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8R8G8B8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8RL8GL8BL8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8B8G8R8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X8BL8GL8RL8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_R5G6B5 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_A1R5G5B5 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_X1R5G5B5 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y32 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Z1R5G5B5 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_O1R5G5B5 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Z8R8G8B8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_O8R8G8B8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_Y1_8X8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32_GF32 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_X16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_X32 );

            // FERMI_TWOD_A
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_R16_G16_B16_A16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_RN16_GN16_BN16_AN16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_BF10GF11RF11 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_AN8BN8GN8RN8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_RF16_GF16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_R16_G16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_RN16_GN16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_G8R8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_GN8RN8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_RN16 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_RN8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].AddRandItem(1, LW902D_SET_DST_FORMAT_V_A8 );
            m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].CompileRandom();

        case RND2D_TWOD_SURF_OFFSET:
            if (last < RND2D_TWOD_SURF_OFFSET) break;
            // Surface2d-relative byte offset for src and dst surfaces
            m_Pickers[RND2D_TWOD_SURF_OFFSET].ConfigRandom();
            m_Pickers[RND2D_TWOD_SURF_OFFSET].AddRandItem(2,  0);
            m_Pickers[RND2D_TWOD_SURF_OFFSET].AddRandRange(1, 1, 4*1024*1024);
            m_Pickers[RND2D_TWOD_SURF_OFFSET].CompileRandom();

        case RND2D_TWOD_SURF_PITCH:
            if (last < RND2D_TWOD_SURF_PITCH) break;
            // byte-pitch (2 picks per loop: dst, src), 0 == modeset pitch.
            m_Pickers[RND2D_TWOD_SURF_PITCH].ConfigRandom();
            m_Pickers[RND2D_TWOD_SURF_PITCH].AddRandItem(3, 0);
            m_Pickers[RND2D_TWOD_SURF_PITCH].AddRandRange(1, 0, 0xffff);
            m_Pickers[RND2D_TWOD_SURF_PITCH].CompileRandom();

        case RND2D_TWOD_PCPU_INDEX_WRAP:
            if (last < RND2D_TWOD_PCPU_INDEX_WRAP) break;
            // LW502D_SET_PIXELS_FROM_CPU_INDEX_WRAP
            m_Pickers[RND2D_TWOD_PCPU_INDEX_WRAP].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_INDEX_WRAP].AddRandRange(1, 0, 1);
            m_Pickers[RND2D_TWOD_PCPU_INDEX_WRAP].CompileRandom();

        case RND2D_TWOD_NUM_TPCS:
            if (last < RND2D_TWOD_NUM_TPCS) break;
            // LW502D_SET_NUM_TPCS
            m_Pickers[RND2D_TWOD_NUM_TPCS].ConfigConst(LW502D_SET_NUM_TPCS_V_ALL);

        case RND2D_TWOD_CLIP_ENABLE:
            if (last < RND2D_TWOD_CLIP_ENABLE) break;
            // LW502D_SET_CLIP_ENABLE
            m_Pickers[RND2D_TWOD_CLIP_ENABLE].ConfigRandom();
            m_Pickers[RND2D_TWOD_CLIP_ENABLE].AddRandRange(1, 0, 1);
            m_Pickers[RND2D_TWOD_CLIP_ENABLE].CompileRandom();

        case RND2D_TWOD_CLIP_LEFT:
            if (last < RND2D_TWOD_CLIP_LEFT) break;
            // pixels at left of dst surf to be undrawn
            m_Pickers[RND2D_TWOD_CLIP_LEFT].ConfigRandom();
            m_Pickers[RND2D_TWOD_CLIP_LEFT].AddRandItem(4, 0);
            m_Pickers[RND2D_TWOD_CLIP_LEFT].AddRandRange(1, 0,64);
            m_Pickers[RND2D_TWOD_CLIP_LEFT].CompileRandom();

        case RND2D_TWOD_CLIP_TOP:
            if (last < RND2D_TWOD_CLIP_TOP) break;
            // pixels at top of dst surf to be undrawn
            m_Pickers[RND2D_TWOD_CLIP_TOP].ConfigRandom();
            m_Pickers[RND2D_TWOD_CLIP_TOP].AddRandItem(4, 0);
            m_Pickers[RND2D_TWOD_CLIP_TOP].AddRandRange(1, 0,64);
            m_Pickers[RND2D_TWOD_CLIP_TOP].CompileRandom();

        case RND2D_TWOD_CLIP_RIGHT:
            if (last < RND2D_TWOD_CLIP_RIGHT) break;
            // pixels at right of dst surf to be undrawn
            m_Pickers[RND2D_TWOD_CLIP_RIGHT].ConfigRandom();
            m_Pickers[RND2D_TWOD_CLIP_RIGHT].AddRandItem(4, 0);
            m_Pickers[RND2D_TWOD_CLIP_RIGHT].AddRandRange(1, 0,64);
            m_Pickers[RND2D_TWOD_CLIP_RIGHT].CompileRandom();

        case RND2D_TWOD_CLIP_BOTTOM:
            if (last < RND2D_TWOD_CLIP_BOTTOM) break;
            // pixels at right of dst surf to be undrawn
            m_Pickers[RND2D_TWOD_CLIP_BOTTOM].ConfigRandom();
            m_Pickers[RND2D_TWOD_CLIP_BOTTOM].AddRandItem(4, 0);
            m_Pickers[RND2D_TWOD_CLIP_BOTTOM].AddRandRange(1, 0,64);
            m_Pickers[RND2D_TWOD_CLIP_BOTTOM].CompileRandom();

        case RND2D_TWOD_SOURCE:
            if (last < RND2D_TWOD_SOURCE) break;
            // for ops that have a source, use src->dst vs. dst->dst
            m_Pickers[RND2D_TWOD_SOURCE].ConfigRandom();
            m_Pickers[RND2D_TWOD_SOURCE].AddRandItem(1, RND2D_TWOD_SOURCE_DstToDst);
            m_Pickers[RND2D_TWOD_SOURCE].AddRandItem(1, RND2D_TWOD_SOURCE_SrcToDst);
            m_Pickers[RND2D_TWOD_SOURCE].AddRandItem(1, RND2D_TWOD_SOURCE_SysToDst);
            m_Pickers[RND2D_TWOD_SOURCE].CompileRandom();

        case RND2D_TWOD_RENDER_ENABLE_MODE:
            if (last < RND2D_TWOD_RENDER_ENABLE_MODE) break;
            // LW502D_SET_RENDER_ENABLE_C_MODE
            m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].ConfigRandom();
            m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].AddRandItem(1, LW502D_SET_RENDER_ENABLE_C_MODE_FALSE);
            m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].AddRandItem(99, LW502D_SET_RENDER_ENABLE_C_MODE_TRUE);
            // @@@ coverage hole
            //m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].AddRandItem(1, LW502D_SET_RENDER_ENABLE_C_MODE_CONDITIONAL);
            //m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].AddRandItem(1, LW502D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_EQUAL);
            //m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].AddRandItem(1, LW502D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_NOT_EQUAL);
            m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].CompileRandom();

        case RND2D_TWOD_BIG_ENDIAN_CTRL:
            if (last < RND2D_TWOD_BIG_ENDIAN_CTRL) break;
            // LW502D_SET_BIG_ENDIAN_CONTROL
            m_Pickers[RND2D_TWOD_BIG_ENDIAN_CTRL].ConfigConst(0);

        case RND2D_TWOD_CKEY_ENABLE:
            if (last < RND2D_TWOD_CKEY_ENABLE) break;
            // LW502D_SET_COLOR_KEY_ENABLE
            m_Pickers[RND2D_TWOD_CKEY_ENABLE].ConfigRandom();
            m_Pickers[RND2D_TWOD_CKEY_ENABLE].AddRandItem(1, 0);
            m_Pickers[RND2D_TWOD_CKEY_ENABLE].AddRandItem(2, 1);
            m_Pickers[RND2D_TWOD_CKEY_ENABLE].CompileRandom();

        case RND2D_TWOD_CKEY_FORMAT:
            if (last < RND2D_TWOD_CKEY_FORMAT) break;
            // LW502D_SET_COLOR_KEY_FORMAT
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].ConfigRandom();
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].AddRandItem(1, LW502D_SET_COLOR_KEY_FORMAT_V_A16R5G6B5);
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].AddRandItem(1, LW502D_SET_COLOR_KEY_FORMAT_V_A1R5G5B5);
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].AddRandItem(1, LW502D_SET_COLOR_KEY_FORMAT_V_A8R8G8B8);
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].AddRandItem(1, LW502D_SET_COLOR_KEY_FORMAT_V_A2R10G10B10);
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].AddRandItem(1, LW502D_SET_COLOR_KEY_FORMAT_V_Y8);
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].AddRandItem(1, LW502D_SET_COLOR_KEY_FORMAT_V_Y16);
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].AddRandItem(1, LW502D_SET_COLOR_KEY_FORMAT_V_Y32);
            m_Pickers[RND2D_TWOD_CKEY_FORMAT].CompileRandom();

        case RND2D_TWOD_CKEY_COLOR:
            if (last < RND2D_TWOD_CKEY_COLOR) break;
            // LW502D_SET_COLOR_KEY
            m_Pickers[RND2D_TWOD_CKEY_COLOR].ConfigRandom();
            m_Pickers[RND2D_TWOD_CKEY_COLOR].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND2D_TWOD_CKEY_COLOR].CompileRandom();

        case RND2D_TWOD_ROP:
            if (last < RND2D_TWOD_ROP) break;
            // LW502D_SET_ROP
            m_Pickers[RND2D_TWOD_ROP].ConfigRandom();
            m_Pickers[RND2D_TWOD_ROP].AddRandRange(3, 0, 255 );// any
            m_Pickers[RND2D_TWOD_ROP].AddRandItem( 2,  0x96 ); // rop xor
            m_Pickers[RND2D_TWOD_ROP].AddRandItem( 2,  0xCA ); // pat selects src vs dest
            m_Pickers[RND2D_TWOD_ROP].AddRandItem( 1,  0xF0 ); // pattern only
            m_Pickers[RND2D_TWOD_ROP].AddRandItem( 1,  0xCC ); // src only
            m_Pickers[RND2D_TWOD_ROP].AddRandItem( 1,  0xAA ); // dst only
            m_Pickers[RND2D_TWOD_ROP].AddRandItem( 1,  0xFE ); // rop or
            m_Pickers[RND2D_TWOD_ROP].AddRandItem( 1,  0x80 ); // rop and
            m_Pickers[RND2D_TWOD_ROP].CompileRandom();

        case RND2D_TWOD_BETA1:
            if (last < RND2D_TWOD_BETA1) break;
            // LW502D_SET_BETA1
            m_Pickers[RND2D_TWOD_BETA1].ConfigRandom();
            m_Pickers[RND2D_TWOD_BETA1].AddRandRange(80, 0x00000000, 0x7FFFFFFF );
            m_Pickers[RND2D_TWOD_BETA1].AddRandRange(10, 0x00000000, 0xFFFFFFFF );
            m_Pickers[RND2D_TWOD_BETA1].AddRandItem(  5, 0x00000000 );
            m_Pickers[RND2D_TWOD_BETA1].AddRandItem(  5, 0x7FFFFFFF );
            m_Pickers[RND2D_TWOD_BETA1].CompileRandom();

        case RND2D_TWOD_BETA4:
            if (last < RND2D_TWOD_BETA4) break;
            // LW502D_SET_BETA4
            m_Pickers[RND2D_TWOD_BETA4].ConfigRandom();
            m_Pickers[RND2D_TWOD_BETA4].AddRandRange(2, 0x00000000, 0xFFFFFFFF );
            m_Pickers[RND2D_TWOD_BETA4].AddRandItem( 1, 0xFFFFFFFF );
            m_Pickers[RND2D_TWOD_BETA4].AddRandItem( 1, 0x00000000 );
            m_Pickers[RND2D_TWOD_BETA4].CompileRandom();

        case RND2D_TWOD_OP:
            if (last < RND2D_TWOD_OP) break;
            // LW502D_SET_OPERATION
            m_Pickers[RND2D_TWOD_OP].ConfigRandom();
            m_Pickers[RND2D_TWOD_OP].AddRandItem(1, LW502D_SET_OPERATION_V_SRCCOPY_AND);
            m_Pickers[RND2D_TWOD_OP].AddRandItem(1, LW502D_SET_OPERATION_V_ROP_AND);
            m_Pickers[RND2D_TWOD_OP].AddRandItem(1, LW502D_SET_OPERATION_V_BLEND_AND);
            m_Pickers[RND2D_TWOD_OP].AddRandItem(1, LW502D_SET_OPERATION_V_SRCCOPY);
            m_Pickers[RND2D_TWOD_OP].AddRandItem(1, LW502D_SET_OPERATION_V_ROP);
            m_Pickers[RND2D_TWOD_OP].AddRandItem(1, LW502D_SET_OPERATION_V_SRCCOPY_PREMULT);
            m_Pickers[RND2D_TWOD_OP].AddRandItem(1, LW502D_SET_OPERATION_V_BLEND_PREMULT);
            m_Pickers[RND2D_TWOD_OP].CompileRandom();

        case RND2D_TWOD_PRIM_COLOR_FORMAT:
            if (last < RND2D_TWOD_PRIM_COLOR_FORMAT) break;
            // LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2R10G10B10);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8B8G8R8);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2B10G10R10);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8R8G8B8);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8B8G8R8);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_R5G6B5);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A1R5G5B5);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X1R5G5B5);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y8);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y16);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y32);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z1R5G5B5);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O1R5G5B5);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z8R8G8B8);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O8R8G8B8);
            m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].CompileRandom();

        case RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT:
            if (last < RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT) break;
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].ConfigRandom();

            // LW50_TWOD
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2R10G10B10);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8B8G8R8);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2B10G10R10);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8R8G8B8);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8B8G8R8);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_R5G6B5);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A1R5G5B5);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X1R5G5B5);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y8);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y16);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y32);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z1R5G5B5);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O1R5G5B5);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z8R8G8B8);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O8R8G8B8);

            // FERMI_TWOD_A
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32_BF32_AF32);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF16_GF16_BF16_AF16);
            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].AddRandItem(1, LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32);

            m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].CompileRandom();

        case RND2D_TWOD_LINE_TIE_BREAK_BITS:
            if (last < RND2D_TWOD_LINE_TIE_BREAK_BITS) break;
            // LW502D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].ConfigRandom();
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0000);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0001);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0010);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0011);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0100);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0101);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0110);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x0111);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem(10, 0x1000); // HW default
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x1001);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x1010);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x1011);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x1100);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x1101);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x1110);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].AddRandItem( 1, 0x1111);
            m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].CompileRandom();

        case RND2D_TWOD_PCPU_DATA_TYPE:
            if (last < RND2D_TWOD_PCPU_DATA_TYPE) break;
            // LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE
            m_Pickers[RND2D_TWOD_PCPU_DATA_TYPE].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_DATA_TYPE].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR);
            m_Pickers[RND2D_TWOD_PCPU_DATA_TYPE].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_INDEX);
            m_Pickers[RND2D_TWOD_PCPU_DATA_TYPE].CompileRandom();

        case RND2D_TWOD_PATT_OFFSET:
            if (last < RND2D_TWOD_PATT_OFFSET) break;
            // LW502D_SET_PATTERN_OFFSET
            m_Pickers[RND2D_TWOD_PATT_OFFSET].ConfigRandom();
            m_Pickers[RND2D_TWOD_PATT_OFFSET].AddRandRange(1, 0, 63);
            m_Pickers[RND2D_TWOD_PATT_OFFSET].CompileRandom();

        case RND2D_TWOD_PATT_SELECT:
            if (last < RND2D_TWOD_PATT_SELECT) break;
            // LW502D_SET_PATTERN_SELECT
            m_Pickers[RND2D_TWOD_PATT_SELECT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PATT_SELECT].AddRandItem(1, LW502D_SET_PATTERN_SELECT_V_MONOCHROME_8x8);
            m_Pickers[RND2D_TWOD_PATT_SELECT].AddRandItem(1, LW502D_SET_PATTERN_SELECT_V_MONOCHROME_64x1);
            m_Pickers[RND2D_TWOD_PATT_SELECT].AddRandItem(1, LW502D_SET_PATTERN_SELECT_V_MONOCHROME_1x64);
            m_Pickers[RND2D_TWOD_PATT_SELECT].AddRandItem(1, LW502D_SET_PATTERN_SELECT_V_COLOR);
            m_Pickers[RND2D_TWOD_PATT_SELECT].CompileRandom();

        case RND2D_TWOD_PATT_MONO_COLOR_FORMAT:
            if (last < RND2D_TWOD_PATT_MONO_COLOR_FORMAT) break;
            // LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8R5G6B5);
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A1R5G5B5);
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8);
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8Y8);
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8Y16);
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32);
            m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].CompileRandom();

        case RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT:
            if (last < RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT) break;
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].ConfigRandom();

            // LW50_TWOD
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8R5G6B5);
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A1R5G5B5);
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8);
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8Y8);
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8Y16);
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32);

            // FERMI_TWOD_A
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].AddRandItem(1, LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_BYTE_EXPAND);
            m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].CompileRandom();

        case RND2D_TWOD_PATT_MONO_FORMAT:
            if (last < RND2D_TWOD_PATT_MONO_FORMAT) break;
            // LW502D_SET_MONOCHROME_PATTERN_FORMAT
            m_Pickers[RND2D_TWOD_PATT_MONO_FORMAT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PATT_MONO_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_CGA6_M1);
            m_Pickers[RND2D_TWOD_PATT_MONO_FORMAT].AddRandItem(1, LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_LE_M1);
            m_Pickers[RND2D_TWOD_PATT_MONO_FORMAT].CompileRandom();

        case RND2D_TWOD_PATT_COLOR:
            if (last < RND2D_TWOD_PATT_COLOR) break;
            // LW502D_SET_MONOCHROME_PATTERN_COLOR0,1
            m_Pickers[RND2D_TWOD_PATT_COLOR].ConfigRandom();
            m_Pickers[RND2D_TWOD_PATT_COLOR].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND2D_TWOD_PATT_COLOR].CompileRandom();

        case RND2D_TWOD_PATT_MONO_PATT:
            if (last < RND2D_TWOD_PATT_MONO_PATT) break;
            // LW502D_SET_MONOCHROME_PATTERN0,1
            m_Pickers[RND2D_TWOD_PATT_MONO_PATT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PATT_MONO_PATT].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND2D_TWOD_PATT_MONO_PATT].CompileRandom();

        case RND2D_TWOD_SENDXY_16:
            if (last < RND2D_TWOD_SENDXY_16) break;
            // if true, use _PRIM_POINT_X_Y, else _PRIM_POINT_Y.
            m_Pickers[RND2D_TWOD_SENDXY_16].ConfigRandom();
            m_Pickers[RND2D_TWOD_SENDXY_16].AddRandRange(1, 0, 1);
            m_Pickers[RND2D_TWOD_SENDXY_16].CompileRandom();

        case RND2D_TWOD_NUM_VERTICES:
            if (last < RND2D_TWOD_NUM_VERTICES) break;
            // number of solid-prim vertices to send
            m_Pickers[RND2D_TWOD_NUM_VERTICES].ConfigRandom();
            m_Pickers[RND2D_TWOD_NUM_VERTICES].AddRandRange(5, 0, 20);
            m_Pickers[RND2D_TWOD_NUM_VERTICES].AddRandRange(1, 0, 1024);
            m_Pickers[RND2D_TWOD_NUM_VERTICES].CompileRandom();

        case RND2D_TWOD_PRIM_WIDTH:
            if (last < RND2D_TWOD_PRIM_WIDTH) break;
            // width of a solid primitive in pixels
            m_Pickers[RND2D_TWOD_PRIM_WIDTH].ConfigRandom();
            m_Pickers[RND2D_TWOD_PRIM_WIDTH].AddRandItem(  1,  0);
            m_Pickers[RND2D_TWOD_PRIM_WIDTH].AddRandRange(20,  1, 19);
            m_Pickers[RND2D_TWOD_PRIM_WIDTH].AddRandRange( 5, 20, 199);
            m_Pickers[RND2D_TWOD_PRIM_WIDTH].AddRandRange( 1, 200, 1999);
            m_Pickers[RND2D_TWOD_PRIM_WIDTH].CompileRandom();

        case RND2D_TWOD_PRIM_HEIGHT:
            if (last < RND2D_TWOD_PRIM_HEIGHT) break;
            // height of a solid primitive, in pixels
            m_Pickers[RND2D_TWOD_PRIM_HEIGHT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PRIM_HEIGHT].AddRandItem(  1,  0);
            m_Pickers[RND2D_TWOD_PRIM_HEIGHT].AddRandRange(20,  1, 19);
            m_Pickers[RND2D_TWOD_PRIM_HEIGHT].AddRandRange( 5, 20, 199);
            m_Pickers[RND2D_TWOD_PRIM_HEIGHT].AddRandRange( 1, 200, 1999);
            m_Pickers[RND2D_TWOD_PRIM_HEIGHT].CompileRandom();

        case RND2D_TWOD_PCPU_CFMT:
            if (last < RND2D_TWOD_PCPU_CFMT) break;
            // LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT
            m_Pickers[RND2D_TWOD_PCPU_CFMT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2R10G10B10);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8B8G8R8);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2B10G10R10);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X8R8G8B8);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X8B8G8R8);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_R5G6B5);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A1R5G5B5);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X1R5G5B5);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y8);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y16);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y32);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Z1R5G5B5);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_O1R5G5B5);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Z8R8G8B8);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_O8R8G8B8);
            m_Pickers[RND2D_TWOD_PCPU_CFMT].CompileRandom();

        case RND2D_TWOD_PCPU_IFMT:
            if (last < RND2D_TWOD_PCPU_IFMT) break;
            // LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT
            m_Pickers[RND2D_TWOD_PCPU_IFMT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_IFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1);
            m_Pickers[RND2D_TWOD_PCPU_IFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I4);
            m_Pickers[RND2D_TWOD_PCPU_IFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8);
            m_Pickers[RND2D_TWOD_PCPU_IFMT].CompileRandom();

        case RND2D_TWOD_PCPU_MFMT:
            if (last < RND2D_TWOD_PCPU_MFMT) break;
            // LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT
            m_Pickers[RND2D_TWOD_PCPU_MFMT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_MFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_CGA6_M1);
            m_Pickers[RND2D_TWOD_PCPU_MFMT].AddRandItem(1, LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_LE_M1);
            m_Pickers[RND2D_TWOD_PCPU_MFMT].CompileRandom();

        case RND2D_TWOD_PCPU_COLOR:
            if (last < RND2D_TWOD_PCPU_COLOR) break;
            // LW502D_SET_PIXELS_FROM_CPU_COLOR0,1 and DATA
            m_Pickers[RND2D_TWOD_PCPU_COLOR].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_COLOR].AddRandRange(8,0, 0xffffffff);
            m_Pickers[RND2D_TWOD_PCPU_COLOR].AddRandItem(1, 0);
            m_Pickers[RND2D_TWOD_PCPU_COLOR].AddRandItem(1, 0xffffffff);
            m_Pickers[RND2D_TWOD_PCPU_COLOR].CompileRandom();

        case RND2D_TWOD_PCPU_MONO_OPAQUE:
            if (last < RND2D_TWOD_PCPU_MONO_OPAQUE) break;
            // LW502D_SET_PIXELS_FROM_CPU_MONO_OPACITY
            m_Pickers[RND2D_TWOD_PCPU_MONO_OPAQUE].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_MONO_OPAQUE].AddRandItem(1,0);
            m_Pickers[RND2D_TWOD_PCPU_MONO_OPAQUE].AddRandItem(1,1);
            m_Pickers[RND2D_TWOD_PCPU_MONO_OPAQUE].CompileRandom();

        case RND2D_TWOD_PCPU_SRC_W:
            if (last < RND2D_TWOD_PCPU_SRC_W) break;
            // LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH
            m_Pickers[RND2D_TWOD_PCPU_SRC_W].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_SRC_W].AddRandRange(50, 1, 32);
            m_Pickers[RND2D_TWOD_PCPU_SRC_W].AddRandRange( 9, 1, 128);
            m_Pickers[RND2D_TWOD_PCPU_SRC_W].AddRandRange( 1, 1, 0x7fff);
            m_Pickers[RND2D_TWOD_PCPU_SRC_W].CompileRandom();

        case RND2D_TWOD_PCPU_SRC_H:
            if (last < RND2D_TWOD_PCPU_SRC_H) break;
            // LW502D_SET_PIXELS_FROM_CPU_SRC_HEIGHT
            m_Pickers[RND2D_TWOD_PCPU_SRC_H].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_SRC_H].AddRandRange(50, 1, 32);
            m_Pickers[RND2D_TWOD_PCPU_SRC_H].AddRandRange( 9, 1, 128);
            m_Pickers[RND2D_TWOD_PCPU_SRC_H].AddRandRange( 1, 1, 0x7fff);
            m_Pickers[RND2D_TWOD_PCPU_SRC_H].CompileRandom();

        case RND2D_TWOD_PCPU_SRC_DXDU_INT:
            if (last < RND2D_TWOD_PCPU_SRC_DXDU_INT) break;
            // LW502D_SET_PIXELS_FROM_CPU_DX_DU_INT
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].AddRandItem(10, 0);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].AddRandItem(25, 1);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].AddRandItem(25,  0xffffffff); // -1
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].AddRandRange(1, 2, 10);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].AddRandRange(1, 0xfffffff4, 0xfffffffe); // -10 to -2
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].CompileRandom();

        case RND2D_TWOD_PCPU_SRC_DXDU_FRAC:
            if (last < RND2D_TWOD_PCPU_SRC_DXDU_FRAC) break;
            // LW502D_SET_PIXELS_FROM_CPU_DX_DU_FRAC
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_FRAC].AddRandItem(1,0);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_FRAC].AddRandRange(2, 0, 0xffffffff);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_FRAC].CompileRandom();

        case RND2D_TWOD_PCPU_SRC_DYDV_INT:
            if (last < RND2D_TWOD_PCPU_SRC_DYDV_INT) break;
            // LW502D_SET_PIXELS_FROM_CPU_DY_DV_INT
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].AddRandItem(10, 0);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].AddRandItem(25, 1);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].AddRandItem(25, 0xffffffff); // -1
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].AddRandRange(1, 2, 10 );
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].AddRandRange(1, 0xfffffff4, 0xfffffffe); // -10 to -2
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].CompileRandom();

        case RND2D_TWOD_PCPU_SRC_DYDV_FRAC:
            if (last < RND2D_TWOD_PCPU_SRC_DYDV_FRAC) break;
            // LW502D_SET_PIXELS_FROM_CPU_DY_DV_FRAC
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_FRAC].AddRandItem(1,0);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_FRAC].AddRandRange(2,0, 0xffffffff);
            m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_FRAC].CompileRandom();

        case RND2D_TWOD_PCPU_DST_X_INT:
            if (last < RND2D_TWOD_PCPU_DST_X_INT) break;
            // LW502D_SET_PIXELS_FROM_CPU_DST_X0_INT
            m_Pickers[RND2D_TWOD_PCPU_DST_X_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_DST_X_INT].AddRandRange(2, 0, 1024); // 0 to 1024
            m_Pickers[RND2D_TWOD_PCPU_DST_X_INT].AddRandRange(1, 0xffffff00, 0xffffffff); // -256 to -1
            m_Pickers[RND2D_TWOD_PCPU_DST_X_INT].CompileRandom();

        case RND2D_TWOD_PCPU_DST_X_FRAC:
            if (last < RND2D_TWOD_PCPU_DST_X_FRAC) break;
            // LW502D_SET_PIXELS_FROM_CPU_DST_X0_FRAC
            m_Pickers[RND2D_TWOD_PCPU_DST_X_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_DST_X_FRAC].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND2D_TWOD_PCPU_DST_X_FRAC].CompileRandom();

        case RND2D_TWOD_PCPU_DST_Y_INT:
            if (last < RND2D_TWOD_PCPU_DST_Y_INT) break;
            // LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT
            m_Pickers[RND2D_TWOD_PCPU_DST_Y_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_DST_Y_INT].AddRandRange(2, 0, 1024);
            m_Pickers[RND2D_TWOD_PCPU_DST_Y_INT].AddRandRange(1, 0xffffff00, 0xffffffff);  // -256 to -1
            m_Pickers[RND2D_TWOD_PCPU_DST_Y_INT].CompileRandom();

        case RND2D_TWOD_PCPU_DST_Y_FRAC:
            if (last < RND2D_TWOD_PCPU_DST_Y_FRAC) break;
            // LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC
            m_Pickers[RND2D_TWOD_PCPU_DST_Y_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PCPU_DST_Y_FRAC].AddRandRange(1, 0, 0xffffffff);
            m_Pickers[RND2D_TWOD_PCPU_DST_Y_FRAC].CompileRandom();

        case RND2D_TWOD_PMEM_BLOCK_SHAPE:
            if (last < RND2D_TWOD_PMEM_BLOCK_SHAPE) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE
            m_Pickers[RND2D_TWOD_PMEM_BLOCK_SHAPE].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_BLOCK_SHAPE].AddRandItem(1, LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_AUTO);
            m_Pickers[RND2D_TWOD_PMEM_BLOCK_SHAPE].AddRandItem(1, LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_SHAPE_8X4);
            m_Pickers[RND2D_TWOD_PMEM_BLOCK_SHAPE].AddRandItem(1, LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_SHAPE_16X2);
            m_Pickers[RND2D_TWOD_PMEM_BLOCK_SHAPE].CompileRandom();

        case RND2D_TWOD_PMEM_SAFE_OVERLAP:
            if (last < RND2D_TWOD_PMEM_SAFE_OVERLAP) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP
            m_Pickers[RND2D_TWOD_PMEM_SAFE_OVERLAP].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_SAFE_OVERLAP].AddRandRange(1,0,1);
            m_Pickers[RND2D_TWOD_PMEM_SAFE_OVERLAP].CompileRandom();

        case RND2D_TWOD_PMEM_ORIGIN:
            if (last < RND2D_TWOD_PMEM_ORIGIN) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN
            m_Pickers[RND2D_TWOD_PMEM_ORIGIN].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_ORIGIN].AddRandItem(1, LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CENTER);
            m_Pickers[RND2D_TWOD_PMEM_ORIGIN].AddRandItem(1, LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CORNER);
            m_Pickers[RND2D_TWOD_PMEM_ORIGIN].CompileRandom();

        case RND2D_TWOD_PMEM_FILTER:
            if (last < RND2D_TWOD_PMEM_FILTER) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER
            m_Pickers[RND2D_TWOD_PMEM_FILTER].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_FILTER].AddRandItem(1, LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT);
            m_Pickers[RND2D_TWOD_PMEM_FILTER].AddRandItem(1, LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_BILINEAR);
            m_Pickers[RND2D_TWOD_PMEM_FILTER].CompileRandom();

        case RND2D_TWOD_PMEM_DST_X0:
            if (last < RND2D_TWOD_PMEM_DST_X0) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DST_X0
            m_Pickers[RND2D_TWOD_PMEM_DST_X0].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DST_X0].AddRandRange(1,0,0x7fff);
            m_Pickers[RND2D_TWOD_PMEM_DST_X0].CompileRandom();

        case RND2D_TWOD_PMEM_DST_Y0:
            if (last < RND2D_TWOD_PMEM_DST_Y0) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0
            m_Pickers[RND2D_TWOD_PMEM_DST_Y0].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DST_Y0].AddRandRange(1,0,0x7fff);
            m_Pickers[RND2D_TWOD_PMEM_DST_Y0].CompileRandom();

        case RND2D_TWOD_PMEM_DST_W:
            if (last < RND2D_TWOD_PMEM_DST_W) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH
            m_Pickers[RND2D_TWOD_PMEM_DST_W].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DST_W].AddRandRange(10,1,32);
            m_Pickers[RND2D_TWOD_PMEM_DST_W].AddRandRange( 5,1,256);
            m_Pickers[RND2D_TWOD_PMEM_DST_W].AddRandRange( 1,1,0x7fff);
            m_Pickers[RND2D_TWOD_PMEM_DST_W].CompileRandom();

        case RND2D_TWOD_PMEM_DST_H:
            if (last < RND2D_TWOD_PMEM_DST_H) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT
            m_Pickers[RND2D_TWOD_PMEM_DST_H].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DST_H].AddRandRange(10,1,32);
            m_Pickers[RND2D_TWOD_PMEM_DST_H].AddRandRange( 5,1,256);
            m_Pickers[RND2D_TWOD_PMEM_DST_H].AddRandRange( 1,1,0x7fff);
            m_Pickers[RND2D_TWOD_PMEM_DST_H].CompileRandom();

        case RND2D_TWOD_PMEM_DUDX_FRAC:
            if (last < RND2D_TWOD_PMEM_DUDX_FRAC) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC
            m_Pickers[RND2D_TWOD_PMEM_DUDX_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DUDX_FRAC].AddRandRange(1,0,0xffffffff);
            m_Pickers[RND2D_TWOD_PMEM_DUDX_FRAC].CompileRandom();

        case RND2D_TWOD_PMEM_DUDX_INT:
            if (last < RND2D_TWOD_PMEM_DUDX_INT) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_INT
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].AddRandItem(30,0);
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].AddRandItem(30,1);
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].AddRandItem(30,0xffffffff); // -1
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].AddRandRange(4,2,10);
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].AddRandRange(4,0xfffffff6,0xfffffffe); // -10 to -2
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].AddRandRange(1,0, 0x7fff); // any positive
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].AddRandRange(1,0xffff0000, 0xffff7fff); // any negative
            m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].CompileRandom();

        case RND2D_TWOD_PMEM_DVDY_FRAC:
            if (last < RND2D_TWOD_PMEM_DVDY_FRAC) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC
            m_Pickers[RND2D_TWOD_PMEM_DVDY_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DVDY_FRAC].AddRandRange(0,1,0xffffffff);
            m_Pickers[RND2D_TWOD_PMEM_DVDY_FRAC].CompileRandom();

        case RND2D_TWOD_PMEM_DVDY_INT:
            if (last < RND2D_TWOD_PMEM_DVDY_INT) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_INT
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].AddRandItem(30,0);
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].AddRandItem(30,1);
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].AddRandItem(30,0xffffffff); // -1
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].AddRandRange(4,2,10);
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].AddRandRange(4,0xfffffff6,0xfffffffe); // -10 to -2
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].AddRandRange(1,0, 0x7fff); // any positive
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].AddRandRange(1,0xffff0000, 0xffff7fff); // any negative
            m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].CompileRandom();

        case RND2D_TWOD_PMEM_SRC_X0_FRAC:
            if (last < RND2D_TWOD_PMEM_SRC_X0_FRAC) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC
            m_Pickers[RND2D_TWOD_PMEM_SRC_X0_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_SRC_X0_FRAC].AddRandRange(1,0,0xffffffff);
            m_Pickers[RND2D_TWOD_PMEM_SRC_X0_FRAC].CompileRandom();

        case RND2D_TWOD_PMEM_SRC_X0_INT:
            if (last < RND2D_TWOD_PMEM_SRC_X0_INT) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT
            m_Pickers[RND2D_TWOD_PMEM_SRC_X0_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_SRC_X0_INT].AddRandRange(1,0,0x7fff);
            m_Pickers[RND2D_TWOD_PMEM_SRC_X0_INT].AddRandRange(1,0xffff0000,0xffff7fff);
            m_Pickers[RND2D_TWOD_PMEM_SRC_X0_INT].CompileRandom();

        case RND2D_TWOD_PMEM_SRC_Y0_FRAC:
            if (last < RND2D_TWOD_PMEM_SRC_Y0_FRAC) break;
            // LW502D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC
            m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_FRAC].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_FRAC].AddRandRange(1,0,0xffffffff);
            m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_FRAC].CompileRandom();

        case RND2D_TWOD_PMEM_SRC_Y0_INT:
            if (last < RND2D_TWOD_PMEM_SRC_Y0_INT) break;
            // LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT
            m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_INT].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_INT].AddRandRange(1,0,0x7fff);
            m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_INT].AddRandRange(1,0xffff0000,0xffff7fff);
            m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_INT].CompileRandom();

        case RND2D_TWOD_PMEM_DIRECTION:
            if (last < RND2D_TWOD_PMEM_DIRECTION) break;
            // LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL
            // LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL
            m_Pickers[RND2D_TWOD_PMEM_DIRECTION].ConfigRandom();
            m_Pickers[RND2D_TWOD_PMEM_DIRECTION].AddRandItem(1, LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_HW_DECIDES);
            m_Pickers[RND2D_TWOD_PMEM_DIRECTION].AddRandItem(1, LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_LEFT_TO_RIGHT);
            m_Pickers[RND2D_TWOD_PMEM_DIRECTION].AddRandItem(1, LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_RIGHT_TO_LEFT);
            m_Pickers[RND2D_TWOD_PMEM_DIRECTION].CompileRandom();

        case RND2D_TWOD_PATT_LOAD_METHOD:
            if (last < RND2D_TWOD_PATT_LOAD_METHOD) break;
            // which pattern download method to use
            m_Pickers[RND2D_TWOD_PATT_LOAD_METHOD].ConfigRandom();
            m_Pickers[RND2D_TWOD_PATT_LOAD_METHOD].AddRandItem(1, LW502D_COLOR_PATTERN_X8R8G8B8(0));
            m_Pickers[RND2D_TWOD_PATT_LOAD_METHOD].AddRandItem(1, LW502D_COLOR_PATTERN_R5G6B5(0));
            m_Pickers[RND2D_TWOD_PATT_LOAD_METHOD].AddRandItem(1, LW502D_COLOR_PATTERN_X1R5G5B5(0));
            m_Pickers[RND2D_TWOD_PATT_LOAD_METHOD].AddRandItem(1, LW502D_COLOR_PATTERN_Y8(0));
            m_Pickers[RND2D_TWOD_PATT_LOAD_METHOD].CompileRandom();
    }
}

//------------------------------------------------------------------------------
/*virtual*/
void TwodPickHelper::Restart()
{
    PickHelper::Restart();
    for (int i = 0; i < 64; i++)
        m_PattColorImage[i] = m_pRandom2d->GetRandom();
}

//------------------------------------------------------------------------------
bool TwodPickHelper::IsPfcColor()
{
    return (m_PcpuDataType == LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR);
}

//------------------------------------------------------------------------------
bool TwodPickHelper::IsPfcMono()
{
    return (m_PcpuDataType == LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_INDEX) &&
           (m_PcpuIndexFormat == LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1);
}

//------------------------------------------------------------------------------
bool TwodPickHelper::IsPfcIndex()
{
    return (m_PcpuDataType == LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_INDEX) &&
           (m_PcpuIndexFormat != LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1);
}

//------------------------------------------------------------------------------
/* virtual */
void TwodPickHelper::Pick()
{
    // All random operations first:
    m_Prim                  = m_Pickers[RND2D_TWOD_PRIM].Pick();
    m_SendXY16         = 0 != m_Pickers[RND2D_TWOD_SENDXY_16].Pick();
    m_PrimColor[0]          = m_Pickers[RND2D_TWOD_PATT_COLOR].Pick();
    m_PrimColor[1]          = m_Pickers[RND2D_TWOD_PATT_COLOR].Pick();
    m_PrimColor[2]          = m_Pickers[RND2D_TWOD_PATT_COLOR].Pick();
    m_PrimColor[3]          = m_Pickers[RND2D_TWOD_PATT_COLOR].Pick();
    m_NumTpcs               = m_Pickers[RND2D_TWOD_NUM_TPCS].Pick();
    m_Source                = m_Pickers[RND2D_TWOD_SOURCE].Pick();
    m_RenderEnable          = m_Pickers[RND2D_TWOD_RENDER_ENABLE_MODE].Pick();
    m_BigEndianCtrl         = m_Pickers[RND2D_TWOD_BIG_ENDIAN_CTRL].Pick();

    m_DstOffset             = m_Pickers[RND2D_TWOD_SURF_OFFSET].Pick();
    m_DstPitch              = m_Pickers[RND2D_TWOD_SURF_PITCH].Pick();

    m_SrcOffset             = m_Pickers[RND2D_TWOD_SURF_OFFSET].Pick();
    m_SrcPitch              = m_Pickers[RND2D_TWOD_SURF_PITCH].Pick();

    if (m_pGrAlloc->GetClass() == FERMI_TWOD_A)
    {
        m_DstFmt                = m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].Pick();
        m_SrcFmt                = m_Pickers[RND2D_TWOD_FERMI_SURF_FORMAT].Pick();
        m_PrimColorFormat       = m_Pickers[RND2D_TWOD_FERMI_PRIM_COLOR_FORMAT].Pick();
        m_PattMonoColorFormat   = m_Pickers[RND2D_TWOD_FERMI_PATT_MONO_COLOR_FORMAT].Pick();
    }
    else
    {
        m_DstFmt                = m_Pickers[RND2D_TWOD_SURF_FORMAT].Pick();
        m_SrcFmt                = m_Pickers[RND2D_TWOD_SURF_FORMAT].Pick();
        m_PrimColorFormat       = m_Pickers[RND2D_TWOD_PRIM_COLOR_FORMAT].Pick();
        m_PattMonoColorFormat   = m_Pickers[RND2D_TWOD_PATT_MONO_COLOR_FORMAT].Pick();
    }

    m_ClipEnable       = 0 != m_Pickers[RND2D_TWOD_CLIP_ENABLE].Pick();
    m_ClipX                 = m_Pickers[RND2D_TWOD_CLIP_LEFT].Pick();
    m_ClipY                 = m_Pickers[RND2D_TWOD_CLIP_TOP].Pick();
    m_ClipW                 = m_Pickers[RND2D_TWOD_CLIP_RIGHT].Pick();
    m_ClipH                 = m_Pickers[RND2D_TWOD_CLIP_BOTTOM].Pick();

    m_CKeyEnable       = 0 != m_Pickers[RND2D_TWOD_CKEY_ENABLE].Pick();
    m_CKeyFormat            = m_Pickers[RND2D_TWOD_CKEY_FORMAT].Pick();
    m_CKeyColor             = m_Pickers[RND2D_TWOD_CKEY_COLOR].Pick();

    m_Rop                   = m_Pickers[RND2D_TWOD_ROP].Pick();
    m_Beta1                 = m_Pickers[RND2D_TWOD_BETA1].Pick();
    m_Beta4                 = m_Pickers[RND2D_TWOD_BETA4].Pick();
    m_Op                    = m_Pickers[RND2D_TWOD_OP].Pick();
    m_LineTieBreakBits      = m_Pickers[RND2D_TWOD_LINE_TIE_BREAK_BITS].Pick();

    m_PattOffsetX           = m_Pickers[RND2D_TWOD_PATT_OFFSET].Pick();
    m_PattOffsetY           = m_Pickers[RND2D_TWOD_PATT_OFFSET].Pick();
    m_PattSelect            = m_Pickers[RND2D_TWOD_PATT_SELECT].Pick();
    m_PattMonoFormat        = m_Pickers[RND2D_TWOD_PATT_MONO_FORMAT].Pick();
    m_PattMonoColor[0]      = m_Pickers[RND2D_TWOD_PATT_COLOR].Pick();
    m_PattMonoColor[1]      = m_Pickers[RND2D_TWOD_PATT_COLOR].Pick();
    m_PattMonoPatt[0]       = m_Pickers[RND2D_TWOD_PATT_MONO_PATT].Pick();
    m_PattMonoPatt[1]       = m_Pickers[RND2D_TWOD_PATT_MONO_PATT].Pick();
    m_PattColorLoadMethod   = m_Pickers[RND2D_TWOD_PATT_LOAD_METHOD].Pick();

    m_DoNotify = m_pRandom2d->m_Misc.Notify();

    m_PcpuDataType          = m_Pickers[RND2D_TWOD_PCPU_DATA_TYPE].Pick();
    m_PcpuColorFormat       = m_Pickers[RND2D_TWOD_PCPU_CFMT].Pick();
    m_PcpuIndexFormat       = m_Pickers[RND2D_TWOD_PCPU_IFMT].Pick();
    m_PcpuMonoFormat        = m_Pickers[RND2D_TWOD_PCPU_MFMT].Pick();
    m_PcpuIdxWrap      = 0 != m_Pickers[RND2D_TWOD_PCPU_INDEX_WRAP].Pick();
    m_PcpuMonoColor[0]      = m_Pickers[RND2D_TWOD_PCPU_COLOR].Pick();
    m_PcpuMonoColor[1]      = m_Pickers[RND2D_TWOD_PCPU_COLOR].Pick();
    m_PcpuMonoOpacity  = 0 != m_Pickers[RND2D_TWOD_PCPU_MONO_OPAQUE].Pick();
    m_PcpuSrcW              = m_Pickers[RND2D_TWOD_PCPU_SRC_W].Pick();
    m_PcpuSrcH              = m_Pickers[RND2D_TWOD_PCPU_SRC_H].Pick();
    m_PcpuDxDuInt   = (INT32) m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_INT].Pick();
    m_PcpuDxDuFrac          = m_Pickers[RND2D_TWOD_PCPU_SRC_DXDU_FRAC].Pick();
    m_PcpuDyDvInt   = (INT32) m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_INT].Pick();
    m_PcpuDyDvFrac          = m_Pickers[RND2D_TWOD_PCPU_SRC_DYDV_FRAC].Pick();
    m_PcpuDstXInt   = (INT32) m_Pickers[RND2D_TWOD_PCPU_DST_X_INT].Pick();
    m_PcpuDstXFrac          = m_Pickers[RND2D_TWOD_PCPU_DST_X_FRAC].Pick();
    m_PcpuDstYInt   = (INT32) m_Pickers[RND2D_TWOD_PCPU_DST_Y_INT].Pick();
    m_PcpuDstYFrac          = m_Pickers[RND2D_TWOD_PCPU_DST_Y_FRAC].Pick();

    m_PmemBlockShape        = m_Pickers[RND2D_TWOD_PMEM_BLOCK_SHAPE].Pick();
    m_PmemSafeOverlap  = 0 != m_Pickers[RND2D_TWOD_PMEM_SAFE_OVERLAP].Pick();
    m_PmemOrigin            = m_Pickers[RND2D_TWOD_PMEM_ORIGIN].Pick();
    m_PmemFilter            = m_Pickers[RND2D_TWOD_PMEM_FILTER].Pick();
    m_PmemDstX0             = m_Pickers[RND2D_TWOD_PMEM_DST_X0].Pick();
    m_PmemDstY0             = m_Pickers[RND2D_TWOD_PMEM_DST_Y0].Pick();
    m_PmemDstW              = m_Pickers[RND2D_TWOD_PMEM_DST_W].Pick();
    m_PmemDstH              = m_Pickers[RND2D_TWOD_PMEM_DST_H].Pick();
    m_PmemDuDxFrac          = m_Pickers[RND2D_TWOD_PMEM_DUDX_FRAC].Pick();
    m_PmemDuDxInt   = (INT32) m_Pickers[RND2D_TWOD_PMEM_DUDX_INT].Pick();
    m_PmemDvDyFrac          = m_Pickers[RND2D_TWOD_PMEM_DVDY_FRAC].Pick();
    m_PmemDvDyInt   = (INT32) m_Pickers[RND2D_TWOD_PMEM_DVDY_INT].Pick();
    m_PmemSrcX0Frac         = m_Pickers[RND2D_TWOD_PMEM_SRC_X0_FRAC].Pick();
    m_PmemSrcX0Int  = (INT32) m_Pickers[RND2D_TWOD_PMEM_SRC_X0_INT].Pick();
    m_PmemSrcY0Frac         = m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_FRAC].Pick();
    m_PmemSrcY0Int  = (INT32) m_Pickers[RND2D_TWOD_PMEM_SRC_Y0_INT].Pick();
    m_PmemDirectionH        = m_Pickers[RND2D_TWOD_PMEM_DIRECTION].Pick();
    m_PmemDirectiolw        = m_Pickers[RND2D_TWOD_PMEM_DIRECTION].Pick();

    // Now, fix the picks to avoid HW exceptions at runtime.

    // We're trying to avoid launch checks on the execute methods.
    // These aren't well dolwmented, so read the source:
    //     //hw/lw5x/class/mfs/class/2d/lw50_twod.mfs
    //     //hw/fermi1_gf100/class/mfs/class/2d/fermi_twod.mfs

    bool isSolidPrim = false;
    bool isPxFromMem = false;
    bool isPxFromCpu = false;

    switch (m_Prim)
    {
        case RND2D_TWOD_PRIM_Imgcpu:
            isPxFromCpu = true;
            break;

        case RND2D_TWOD_PRIM_Imgmem:
            isPxFromMem = true;
            break;

        default:
            MASSERT(!"TwodPickHelper::Pick unhandled Prim");
            m_Prim = RND2D_TWOD_PRIM_Points;
            // fall through

        case RND2D_TWOD_PRIM_Points:
        case RND2D_TWOD_PRIM_Lines:
        case RND2D_TWOD_PRIM_PolyLine:
        case RND2D_TWOD_PRIM_Triangles:
        case RND2D_TWOD_PRIM_Rects:
            isSolidPrim = true;
            break;
    }

    switch (m_RenderEnable)
    {
        case LW502D_SET_RENDER_ENABLE_C_MODE_FALSE:
        case LW502D_SET_RENDER_ENABLE_C_MODE_TRUE:
            break;

        case LW502D_SET_RENDER_ENABLE_C_MODE_CONDITIONAL:
        case LW502D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_EQUAL:
        case LW502D_SET_RENDER_ENABLE_C_MODE_RENDER_IF_NOT_EQUAL:
            // @@@ coverage hole
            Printf(Tee::PriHigh, "semaphore-reading conditional render modes not suppored, sorry.\n");
            /* fall through */

        default:
            MASSERT(!"TwodPickHelper::Pick unhandled RenderEnable");
            m_RenderEnable = LW502D_SET_RENDER_ENABLE_C_MODE_TRUE;
    }

    switch (m_PcpuDataType)
    {
        case LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR:
        case LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_INDEX:
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PcpuDataType");
            m_PcpuDataType = LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR;
    }
    switch (m_PcpuIndexFormat)
    {
        case LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1:
        case LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I4:
        case LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8:
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PcpuIndexFormat");
            m_PcpuIndexFormat = LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8;
            break;
    }

    m_DstSurf = m_pRandom2d->GetMemoryInfo(Random2d::DstMem);

    // Fix all destination surface picks.

    bool   allowBlendOp = true; // BLEND_AND, BLEND_PREMULT, SRCCOPY_PREMULT
    bool   allowRopOp = true;   // ROP, ROP_AND

    UINT32 dstBitsPerPixel;
    bool   dstIsFpOrSrgb;
    bool   dstIsY;
    bool   dstIsTesla;
    bool   dstIsValidForSolid;
    GetFmtInfo (m_DstFmt, &dstBitsPerPixel, &dstIsFpOrSrgb, &dstIsY, &dstIsTesla, &dstIsValidForSolid);
    if (0 == dstBitsPerPixel)
    {
        m_DstFmt = LW502D_SET_DST_FORMAT_V_A8R8G8B8;
        GetFmtInfo (m_DstFmt, &dstBitsPerPixel, &dstIsFpOrSrgb, &dstIsY, &dstIsTesla, &dstIsValidForSolid);
    }

    // @@@ Still not sure what the actual rules are here, but there are problems
    // when using incorrect surface formats with blocklinear surfaces.
    // This is a placeholder to get on with things.  2006-04-12 CWD
    if ((m_DstSurf->GetLayout() == Surface2D::BlockLinear) &&
        (dstBitsPerPixel != m_DstSurf->GetBytesPerPixel() * 8))
    {
        switch (m_DstSurf->GetBytesPerPixel())
        {
            case 2:
                m_DstFmt = LW502D_SET_DST_FORMAT_V_R5G6B5;
                break;

            default:
            case 4:
                m_DstFmt = LW502D_SET_DST_FORMAT_V_A8R8G8B8;
                break;
        }
        GetFmtInfo (m_DstFmt, &dstBitsPerPixel, &dstIsFpOrSrgb, &dstIsY, &dstIsTesla, &dstIsValidForSolid);
    }

    // You can only render to a signed-RGB or floating-point dst color format
    // using pixels-from-cpu indexed, or pixels-from-memory.
    // Even then, only SRCCOPY and SRCCOPY_AND are allowed.
    //
    // The Fermi-exclusive PixelFromMemory color formats are not allowed for
    // any PixelFromCpu or solid primitive ops.
    if (dstIsFpOrSrgb || !dstIsTesla)
    {
        bool ilwalidFormat = false;

        switch (m_Prim)
        {
            case RND2D_TWOD_PRIM_Points:
            case RND2D_TWOD_PRIM_Lines:
            case RND2D_TWOD_PRIM_PolyLine:
            case RND2D_TWOD_PRIM_Triangles:
            case RND2D_TWOD_PRIM_Rects:
                // A few floating point color formats that were invalid for
                // LW50_TWOD are now valid for FERMI_TWOD_A.
                if (dstIsValidForSolid)
                {
                    // The primitive color format must match the dst format,
                    // except for DstFmt = RF32_GF32, which may use either
                    // PrimFmt = RF32_GF32_BF32_AF32 or PrimFmt = RF32_GF32
                    switch (m_DstFmt)
                    {
                    case LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32:
                    case LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16:
                    case LW502D_SET_DST_FORMAT_V_RF32_GF32:
                        m_PrimColorFormat = m_DstFmt;
                        break;
                    default:
                        MASSERT(!"TwodPickHelper::Pick invalid dst format for solid prim");
                        ilwalidFormat = true;
                        break;
                    }
                }
                else
                {
                    ilwalidFormat = true;
                }
                break;

            case RND2D_TWOD_PRIM_Imgcpu:
                if (IsPfcColor() || !dstIsTesla)
                {
                    ilwalidFormat = true;
                }
                else
                {
                    if (LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1 == m_PcpuIndexFormat)
                        m_PcpuIndexFormat = LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I4;
                }
                break;

            case RND2D_TWOD_PRIM_Imgmem:
                break;
        }

        if (ilwalidFormat)
        {
            m_DstFmt = LW502D_SET_DST_FORMAT_V_A8R8G8B8;
            GetFmtInfo (m_DstFmt, &dstBitsPerPixel, &dstIsFpOrSrgb, &dstIsY, &dstIsTesla, &dstIsValidForSolid);
        }
    }

    // Bug 594577: Not sure what the exact dependency is, but
    // apparently if dst is not one of the following color
    // formats, then prim can't be either.
    if (isSolidPrim &&
        m_DstFmt != LW902D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32 &&
        m_DstFmt != LW902D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16 &&
        m_DstFmt != LW902D_SET_DST_FORMAT_V_RF32_GF32)
    {
        switch (m_PrimColorFormat)
        {
            case LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32_BF32_AF32:
            case LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF16_GF16_BF16_AF16:
            case LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32:
                m_PrimColorFormat =
                    LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8;
                break;
        }
    }

    // Fix m_Source.
    switch (m_Source)
    {
        default:
            m_Source = RND2D_TWOD_SOURCE_SrcToDst;
            /* fall through */
        case RND2D_TWOD_SOURCE_SrcToDst:
            m_SrcSurf = m_pRandom2d->GetMemoryInfo(Random2d::SrcMem);
            break;

        case RND2D_TWOD_SOURCE_DstToDst:
            if (isPxFromMem || (isPxFromCpu && IsPfcIndex()))
            {
                // We are copying pixels with image-from-memory or
                // we are using image-from-cpu with an indexed palette.
                // Don't read & write the same surface, see bug 234281.
                //
                // Ideally, we would allow same-surface but make sure
                // that src and dst are not the same pixels, but that's
                // a tricky thing to code up, given that src & dst pixel-format,
                // pitch, and offset might not match.
                m_Source = RND2D_TWOD_SOURCE_SysToDst;
                m_SrcSurf = m_pRandom2d->GetMemoryInfo(Random2d::SysMem);
            }
            else
            {
                m_SrcSurf = m_pRandom2d->GetMemoryInfo(Random2d::DstMem);
            }
            break;

        case RND2D_TWOD_SOURCE_SysToDst:
            m_SrcSurf = m_pRandom2d->GetMemoryInfo(Random2d::SysMem);
            break;
    }

    // "Y" dst formats:
    //   - require src, prim, patt, and ckey fmts to match dst
    //   - don't allow blending
    //  Y1_8X8 src format: (pixels from memory only)
    //      if src or dst is Y1_8X8, other must also be Y1_8X8
    //      if src,dst are Y1_8X8:
    //          src,dst memory layout must be BLOCKLINEAR
    //          sample mode must be POINT
    if (dstIsY)
    {
        switch (m_DstFmt)
        {
            case LW502D_SET_DST_FORMAT_V_Y1_8X8:
                if ((m_Prim == RND2D_TWOD_PRIM_Imgmem) &&
                    (m_DstSurf->GetLayout() == Surface2D::BlockLinear))
                {
                    m_CKeyEnable = false;
                    m_PmemFilter = LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT;
                    break;
                }
                // Else, force Y8 and fall through.
                m_DstFmt = LW502D_SET_DST_FORMAT_V_Y8;
                GetFmtInfo (m_DstFmt, &dstBitsPerPixel, &dstIsFpOrSrgb, &dstIsY, &dstIsTesla, &dstIsValidForSolid);

            case LW502D_SET_DST_FORMAT_V_Y8:
                m_CKeyFormat = LW502D_SET_COLOR_KEY_FORMAT_V_Y8;
                m_PattMonoColorFormat = LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8Y8;
                break;
            case LW502D_SET_DST_FORMAT_V_Y16:
                m_CKeyFormat = LW502D_SET_COLOR_KEY_FORMAT_V_Y16;
                m_PattMonoColorFormat = LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8Y16;
                break;
            case LW502D_SET_DST_FORMAT_V_Y32:
                m_CKeyFormat = LW502D_SET_COLOR_KEY_FORMAT_V_Y32;
                m_PattMonoColorFormat = LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32;
                break;
        }

        if ((m_SrcSurf->GetLayout() == Surface2D::BlockLinear) &&
            (dstBitsPerPixel != m_SrcSurf->GetBytesPerPixel() * 8))
        {
            // For "Y" dst formats, src format must be the same as dst format.
            // But this dst format doesn't match the src surface's BlockLinear type.
            // We have to fall back to a non-Y format for src and dst.
            switch (m_DstFmt)
            {
                case LW502D_SET_DST_FORMAT_V_Y1_8X8:
                    m_DstFmt = LW502D_SET_DST_FORMAT_V_A8R8G8B8;
                    break;
                case LW502D_SET_DST_FORMAT_V_Y8:
                    m_DstFmt = LW502D_SET_DST_FORMAT_V_A1R5G5B5;
                    break;
                case LW502D_SET_DST_FORMAT_V_Y16:
                    m_DstFmt = LW502D_SET_DST_FORMAT_V_R5G6B5;
                    break;
                case LW502D_SET_DST_FORMAT_V_Y32:
                    m_DstFmt = LW502D_SET_DST_FORMAT_V_A2R10G10B10;
                    break;
            }
            GetFmtInfo (m_DstFmt, &dstBitsPerPixel, &dstIsFpOrSrgb, &dstIsY, &dstIsTesla, &dstIsValidForSolid);
        }
        else
        {
            m_PrimColorFormat = m_DstFmt;
            m_SrcFmt = m_DstFmt;
            m_PcpuColorFormat = m_DstFmt;
            allowBlendOp = false;
        }
    }

    if (m_DstSurf->GetLayout() == Surface2D::BlockLinear)
    {
        m_DstPitch = m_DstSurf->GetWidth() * m_DstSurf->GetBytesPerPixel();
        m_DstOffset = 0; // @@@ learn how to patch this correctly
        // @@@ bpp must match block-linear?

        m_DstW = (m_DstPitch * 8) / dstBitsPerPixel;
        m_DstH = m_DstSurf->GetHeight();
    }
    else
    {
        // Pitch-linear surface.
        // Note that some modes (i.e. 800x600) have pitch > w*bpp, and in these
        // cases we shouldn't read or write the padding bytes.

        UINT32 padBytes = m_DstSurf->GetPitch() -
                          m_DstSurf->GetWidth() * m_DstSurf->GetBytesPerPixel();

        if ((m_DstPitch == 0) || (padBytes > 0))
        {
            m_DstPitch = m_DstSurf->GetPitch();
        }
        else
        {
            // No padding, we can use wacky "wrong" pitches.
            if (m_DstPitch < m_MinPitch)
                m_DstPitch = m_MinPitch;
            m_DstPitch &= ~(m_MinPitch-1);
        }

        if (m_DstOffset > m_DstSurf->GetSize() - m_DstPitch)
            m_DstOffset = 0;
        m_DstOffset &= ~(m_MinPitch - 1);

        m_DstW = ((m_DstPitch - padBytes) * 8) / dstBitsPerPixel;
        if (padBytes)
        {
            m_DstW -= ((m_DstOffset % m_DstPitch) * 8) / dstBitsPerPixel;
        }
        MASSERT(m_DstSurf->GetSize() < 0xFFFFFFFF);
        m_DstH = UINT32( (m_DstSurf->GetSize() - m_DstOffset) / m_DstPitch );
    }

    // Fix all source surface picks.

    UINT32 srcBitsPerPixel;
    bool   srcIsFpOrSrgb;
    bool   srcIsY;
    bool   srcIsTesla;
    bool   srcIsValidForSolid;
    GetFmtInfo (m_SrcFmt, &srcBitsPerPixel, &srcIsFpOrSrgb, &srcIsY, &srcIsTesla, &srcIsValidForSolid);
    if (0 == srcBitsPerPixel)
    {
        m_SrcFmt = LW502D_SET_SRC_FORMAT_V_A8R8G8B8;
        GetFmtInfo (m_SrcFmt, &srcBitsPerPixel, &srcIsFpOrSrgb, &srcIsY, &srcIsTesla, &srcIsValidForSolid);
    }

    if (srcIsY)
    {
        // Must match dst Y format.
        if (!dstIsY)
        {
            m_SrcFmt = LW502D_SET_SRC_FORMAT_V_A8R8G8B8;
            GetFmtInfo (m_SrcFmt, &srcBitsPerPixel, &srcIsFpOrSrgb, &srcIsY, &srcIsTesla, &srcIsValidForSolid);
        }
    }

    // @@@ Still not sure what the actual rules are here, but there are problems
    // when using incorrect surface formats with blocklinear surfaces.
    // This is a placeholder to get on with things.  2006-04-12 CWD
    if ((m_SrcSurf->GetLayout() == Surface2D::BlockLinear) &&
        (srcBitsPerPixel != m_SrcSurf->GetBytesPerPixel() * 8))
    {
        switch (m_SrcSurf->GetBytesPerPixel())
        {
            case 2:
                m_SrcFmt = LW502D_SET_SRC_FORMAT_V_R5G6B5;
                break;

            default:
            case 4:
                m_SrcFmt = LW502D_SET_SRC_FORMAT_V_A8R8G8B8;
                break;
        }
        GetFmtInfo (m_SrcFmt, &srcBitsPerPixel, &srcIsFpOrSrgb, &srcIsY, &srcIsTesla, &srcIsValidForSolid);
    }

    if (m_SrcSurf->GetLayout() == Surface2D::BlockLinear)
    {
        m_SrcPitch = m_SrcSurf->GetWidth() * m_SrcSurf->GetBytesPerPixel();
        m_SrcOffset = 0; // @@@ learn how to patch this correctly

        m_SrcW = (m_SrcPitch * 8) / srcBitsPerPixel;
        m_SrcH = m_SrcSurf->GetHeight();
    }
    else
    {
        // Pitch-linear surface.
        // Note that some modes (i.e. 800x600) have pitch > w*bpp, and in these
        // cases we shouldn't read or write the padding bytes.

        UINT32 padBytes = m_SrcSurf->GetPitch() -
                          m_SrcSurf->GetWidth() * m_SrcSurf->GetBytesPerPixel();

        if ((m_SrcPitch == 0) || (padBytes > 0))
        {
            m_SrcPitch = m_SrcSurf->GetPitch();
        }
        else
        {
            // No padding, we can use wacky "wrong" pitches.
            if (m_SrcPitch < m_MinPitch)
                m_SrcPitch = m_MinPitch;
            m_SrcPitch &= ~(m_MinPitch-1);
        }

        if (m_SrcOffset > m_SrcSurf->GetSize() - m_SrcPitch)
            m_SrcOffset = 0;
        m_SrcOffset &= ~(m_MinPitch - 1);

        m_SrcW = ((m_SrcPitch - padBytes) * 8) / srcBitsPerPixel;
        if (padBytes)
        {
            m_SrcW -= ((m_SrcOffset % m_SrcPitch) * 8) / srcBitsPerPixel;
        }
        m_SrcH = UINT32( (m_SrcSurf->GetSize() - m_SrcOffset) / m_SrcPitch );
    }

    // Fix the clip rectangle picks.

    if (m_ClipX >= m_DstW)
        m_ClipX = 0;
    if (m_ClipY >= m_DstH)
        m_ClipY = 0;
    m_ClipX &= 0xffff; // class limit
    m_ClipY &= 0xffff; // class limit

    // We picked "pixels to clip off the right", colwert to clip-width.
    m_ClipW = m_DstW - (m_ClipX + m_ClipW);
    m_ClipH = m_DstH - (m_ClipY + m_ClipH);

    if (m_ClipW > 0x10000)
        m_ClipW = 0x10000; // class limit
    if (m_ClipH > 0x10000)
        m_ClipH = 0x10000; // class limit

    if (m_ClipW < 1)
        m_ClipW = 1;
    if (m_ClipW + m_ClipX > m_DstW)
        m_ClipW = m_DstW - m_ClipX;

    if (m_ClipH < 1)
        m_ClipH = 1;
    if (m_ClipH + m_ClipY > m_DstH)
        m_ClipH = m_DstH - m_ClipY;

    // Fix color-key format.
    switch (m_CKeyFormat)
    {
        case LW502D_SET_COLOR_KEY_FORMAT_V_A16R5G6B5:
        case LW502D_SET_COLOR_KEY_FORMAT_V_A1R5G5B5:
        case LW502D_SET_COLOR_KEY_FORMAT_V_A8R8G8B8:
        case LW502D_SET_COLOR_KEY_FORMAT_V_A2R10G10B10:
            break;

        case LW502D_SET_COLOR_KEY_FORMAT_V_Y8:
        case LW502D_SET_COLOR_KEY_FORMAT_V_Y16:
        case LW502D_SET_COLOR_KEY_FORMAT_V_Y32:
            if (!dstIsY)
                m_CKeyFormat = LW502D_SET_COLOR_KEY_FORMAT_V_A8R8G8B8;
            break;

        default:
            MASSERT(!"TwodPickHelper::Pick unhandled CKeyFormat");
            m_CKeyFormat = LW502D_SET_COLOR_KEY_FORMAT_V_A16R5G6B5;
            break;
    }
    if (dstIsFpOrSrgb || srcIsFpOrSrgb || !dstIsTesla)
        m_CKeyEnable = false;

    // Fix some misc 2d state.
    switch (m_PrimColorFormat)
    {
        // LW50_TWOD
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2R10G10B10:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8B8G8R8:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2B10G10R10:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8R8G8B8:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X8B8G8R8:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_R5G6B5:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A1R5G5B5:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_X1R5G5B5:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z1R5G5B5:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O1R5G5B5:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Z8R8G8B8:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_O8R8G8B8:
            break;

        // FERMI_TWOD_A
        case LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32_BF32_AF32:
        case LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF16_GF16_BF16_AF16:
        case LW902D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_RF32_GF32:
            break;

        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y8:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y16:
        case LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y32:
            // Ensure that the primitive color format is compatible with the the color
            // key format.  Change the primitive color format instead of the color
            // key format because changing the color key format may necessitate changes
            // in the destination format as well
            if (m_CKeyEnable &&
                (m_CKeyFormat != LW502D_SET_COLOR_KEY_FORMAT_V_Y8) &&
                (m_CKeyFormat != LW502D_SET_COLOR_KEY_FORMAT_V_Y16) &&
                (m_CKeyFormat != LW502D_SET_COLOR_KEY_FORMAT_V_Y32))
            {
                m_PrimColorFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8;
            }
            break;

        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PrimColorFormat");
            m_PrimColorFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8;
    }

    // Fix pattern (GDI paintbrush) picks:
    m_PattOffsetX &= 0x3f;  // limit range to 0..63
    m_PattOffsetY &= 0x3f;  // limit range to 0..63
    switch (m_PattSelect)
    {
        case LW502D_SET_PATTERN_SELECT_V_MONOCHROME_8x8:
        case LW502D_SET_PATTERN_SELECT_V_MONOCHROME_64x1:
        case LW502D_SET_PATTERN_SELECT_V_MONOCHROME_1x64:
            break;

        case LW502D_SET_PATTERN_SELECT_V_COLOR:
            // Ensure that the pattern select matches the specified destination format
            // This is a new launch check requirement introduced in class 902D (and
            // needs to be applied regardless of whether the pattern select will
            // actually be used by the hardware)
            if ((m_pGrAlloc->GetClass() == FERMI_TWOD_A) &&
                (isSolidPrim || isPxFromMem || IsPfcIndex()) &&
                (dstIsFpOrSrgb || !dstIsTesla))
            {
                m_PattSelect = LW502D_SET_PATTERN_SELECT_V_MONOCHROME_8x8;
            }
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PattSelect");
            m_PattSelect = LW502D_SET_PATTERN_SELECT_V_COLOR;
            break;
    }
    switch (m_PattMonoColorFormat)
    {
        case LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8R5G6B5:
        case LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A1R5G5B5:
        case LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8:
            break;
        case LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8Y8:
        case LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8X8Y16:
        case LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32:
            if (!dstIsY)
                m_PattMonoColorFormat = LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8;
            break;
        case LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_BYTE_EXPAND:
            switch (m_DstFmt)
            {
                case LW502D_SET_DST_FORMAT_V_RF32_GF32:
                case LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16:
                case LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_X16:
                case LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32:
                case LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_X32:
                    break;
                default:
                    // Any other color...
                    m_PattMonoColorFormat = LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8;
                    break;
            }
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PattMonoColorFormat");
            m_PattMonoColorFormat = LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8;
            break;
    }
    switch (m_PattMonoFormat)
    {
        case LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_CGA6_M1:
        case LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_LE_M1:
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PattMonoFormat");
            m_PattMonoFormat = LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_LE_M1;
    }
    switch (m_PattColorLoadMethod)
    {
        case LW502D_COLOR_PATTERN_X8R8G8B8(0):
        case LW502D_COLOR_PATTERN_R5G6B5(0):
        case LW502D_COLOR_PATTERN_X1R5G5B5(0):
        case LW502D_COLOR_PATTERN_Y8(0):
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PattColorLoadMethod");
            m_PattColorLoadMethod = LW502D_COLOR_PATTERN_X8R8G8B8(0);
            break;
    }

    switch (m_PcpuColorFormat)
    {
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2R10G10B10:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8B8G8R8:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2B10G10R10:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X8R8G8B8:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X8B8G8R8:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_R5G6B5:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A1R5G5B5:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_X1R5G5B5:
            break;

        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y8:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y16:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y32:
            if (m_DstFmt != m_PcpuColorFormat)
                m_PcpuColorFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8;
            break;

        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Z1R5G5B5:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_O1R5G5B5:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Z8R8G8B8:
        case LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_O8R8G8B8:
            break;

        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PcpuColorFormat");
            m_PcpuColorFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8;
            break;
    }
    switch (m_PcpuMonoFormat)
    {
        case LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_CGA6_M1:
        case LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_LE_M1:
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PcpuMonoFormat");
            m_PcpuMonoFormat = LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_LE_M1;
            break;
    }
    switch (m_PmemBlockShape)
    {
        case LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_AUTO:
        case LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_SHAPE_8X4:
        case LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_SHAPE_16X2:
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PmemBlockShape");
            m_PmemBlockShape = LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_AUTO;
            break;
    }
    switch (m_PmemOrigin)
    {
        case LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CENTER:
        case LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CORNER:
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PmemOrigin");
            m_PmemOrigin = LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CENTER;
            break;
    }
    switch (m_PmemFilter)
    {
        case LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT:
        case LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_BILINEAR:
            break;
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled PmemFilter");
            m_PmemFilter = LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT;
            break;
    }

    // Determine allowed ROP/blend operations

    if (!dstIsTesla && isPxFromMem)
    {
        // New Fermi formats ONLY support SRCCOPY and SRCCOPY_AND with PFM
        allowBlendOp = false;
        allowRopOp = false;
    }
    else if (dstIsFpOrSrgb && (isSolidPrim || isPxFromMem || IsPfcIndex()))
    {
        // If  BYTE_EXPAND: disable blending,
        // if !BYTE_EXPAND: disable blending and ROP
        allowBlendOp = false;

        if (m_PattMonoColorFormat != LW902D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_BYTE_EXPAND)
        {
            allowRopOp = false;
        }
    }

    switch (m_Op)
    {
        default:
            MASSERT(!"TwodPickHelper::Pick unhandled Op");
            m_Op = LW502D_SET_OPERATION_V_SRCCOPY;
            // Fall through

        case LW502D_SET_OPERATION_V_SRCCOPY:
        case LW502D_SET_OPERATION_V_SRCCOPY_AND:
            break;

        case LW502D_SET_OPERATION_V_BLEND_PREMULT:
        case LW502D_SET_OPERATION_V_SRCCOPY_PREMULT:
            if (!allowBlendOp)
                m_Op = LW502D_SET_OPERATION_V_SRCCOPY;
            break;

        case LW502D_SET_OPERATION_V_ROP:
            if (!allowRopOp)
                m_Op = LW502D_SET_OPERATION_V_SRCCOPY;
            break;

        case LW502D_SET_OPERATION_V_BLEND_AND:
            if (!allowBlendOp)
               m_Op = LW502D_SET_OPERATION_V_SRCCOPY_AND;
            break;

        case LW502D_SET_OPERATION_V_ROP_AND:
            if (!allowRopOp)
               m_Op = LW502D_SET_OPERATION_V_SRCCOPY_AND;
            break;
    }

    if ((m_PmemDirectionH == LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_HW_DECIDES) !=
        (m_PmemDirectiolw == LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL_HW_DECIDES))
    {
        // Directions must be both software selected, or both hardware selected
        m_PmemDirectionH = m_PmemDirectiolw;
    }

    // Now that we've settled everything else, pick the actual render data.
    switch (m_Prim)
    {
        case RND2D_TWOD_PRIM_Points:
        case RND2D_TWOD_PRIM_Lines:
        case RND2D_TWOD_PRIM_PolyLine:
        case RND2D_TWOD_PRIM_Triangles:
        case RND2D_TWOD_PRIM_Rects:
            PickVertices();
            break;

        case RND2D_TWOD_PRIM_Imgcpu:
            PickPixels();
            break;

        case RND2D_TWOD_PRIM_Imgmem:
            PickImage();
            break;
    }
}

//------------------------------------------------------------------------------
void TwodPickHelper::PickVertices()
{
    UINT32 vxNum  = m_Pickers[RND2D_TWOD_NUM_VERTICES].Pick();
    UINT32 vxSize = m_SendXY16 ? 4 : 8;
    UINT32 vxNumMax = m_DataSize / vxSize;

    if (vxNum > vxNumMax)
        vxNum = vxNumMax;

    switch (m_Prim)
    {
        default:
            MASSERT(!"TwodPickHelper::PickVertices unknown Prim");
            m_Prim = RND2D_TWOD_PRIM_Points;
            /* fall through */
        case RND2D_TWOD_PRIM_Points:
            m_VxPerPrim = 1;
            break;

        case RND2D_TWOD_PRIM_Rects:
            m_VxPerPrim = 2;
            break;

        case RND2D_TWOD_PRIM_Lines:
            m_VxPerPrim = 2;
            break;

        case RND2D_TWOD_PRIM_Triangles:
            m_VxPerPrim = 3;
            break;

        case RND2D_TWOD_PRIM_PolyLine:
            m_VxPerPrim = vxNum;
            if (m_VxPerPrim < 1)
               m_VxPerPrim = 1;
            break;
    }
    // Send a whole number of primitives.
    vxNum = vxNum - (vxNum  % m_VxPerPrim);
    if (vxNum < m_VxPerPrim)
        vxNum = m_VxPerPrim;

    MASSERT(vxNum <= vxNumMax);

    bool forceBigSimple = m_pRandom2d->m_Misc.ForceBigSimple();
    MiscPickHelper::BoundingBox bbox = {0,0,0};

    if (forceBigSimple)
    {
        vxNum = m_VxPerPrim;
        m_pRandom2d->m_Misc.GetBigSimpleBoundingBox(&bbox);
    }

    m_NumWordsToSend = (vxNum * vxSize) / 4;

    for (UINT32 v = 0; v < vxNum; v += m_VxPerPrim)
    {
        INT32 dstW = INT32(m_DstW);
        INT32 dstH = INT32(m_DstH);

        INT32 width     = INT32(m_Pickers[RND2D_TWOD_PRIM_WIDTH].Pick());
        INT32 height    = INT32(m_Pickers[RND2D_TWOD_PRIM_HEIGHT].Pick());
        if (width < 0)
            width = 0;
        if (height < 0)
            height = 0;

        INT32 leftX     = INT32(m_pRandom2d->GetRandom(0, m_DstW)) - width/2;
        INT32 topY      = INT32(m_pRandom2d->GetRandom(0, m_DstH)) - height/2;

        if (forceBigSimple)
        {
            UINT32 bpp = m_DstPitch / m_DstW;
            width  = bbox.Width / bpp;
            height = bbox.Height;
            topY   = bbox.Offset / m_DstSurf->GetPitch();
            leftX  = (bbox.Offset % m_DstSurf->GetPitch()) / bpp;
        }

        if (!m_ClipEnable)
        {
            // Prevent limit violations on write.
            if (leftX < 0)
                leftX = 0;
            if (leftX >= dstW)
                leftX = dstW - 1;

            if (width < 0)
                width = 0;
            if (leftX + width > dstW)
                width = dstW - leftX;

            if (topY < 0)
                topY = 0;
            if (topY >= dstH)
                topY = dstH - 1;

            if (height < 0)
                height = 0;
            if (topY + height >= dstH)
                height = dstH - topY;
        }

        if (m_SendXY16)
        {
            // With 16-bit vertices, and Y8 surface formats, we can easily
            // have surface larger than the max x or y value.
            if (leftX < -32768)
                leftX = -32768;
            if (leftX > 32767)
                leftX = 32767;

            if (width < 0)
                width = 0;
            if (leftX + width > 32767)
                width = 32767 - leftX;

            if (topY < -32768)
                topY = -32768;
            if (topY > 32767)
                topY = 32767;

            if (height < 0)
                height = 0;
            if (topY + height > 32767)
                height = 32767 - topY;
        }

        // Pick all vertices in this primitive to sit within the bounding box
        // defined above.
        // Note: I am assuming that negative x,y are allowed, which was true
        // in the legacy classes, but is not dolwmented in lw50_twod...
        for (UINT32 vn = 0; vn < m_VxPerPrim; vn++)
        {
            INT32 x = leftX;
            INT32 y = topY;

            if (m_Prim == RND2D_TWOD_PRIM_Rects)
            {
                if (vn == 1)
                {
                    x += width;
                    y += height;
                }
            }
            else
            {
                x += INT32(m_pRandom2d->GetRandom(0, width));
                y += INT32(m_pRandom2d->GetRandom(0, height));

                if (forceBigSimple)
                {
                    switch (vn)
                    {
                        case 0: x = leftX;         y = topY;          break;
                        case 1: x = leftX;         y = topY + height; break;
                        case 2: x = leftX + width; y = topY + height; break;
                        case 3: x = leftX + width; y = topY;          break;

                        default: // leave subsequent vxs random
                            break;
                    }
                }
            }

            if (!m_ClipEnable)
            {
                if (x < 0)
                    x = 0;
                if (x >= INT32(m_DstW))
                    x = INT32(m_DstW) - 1;
                if (y < 0)
                    y = 0;
                if (y >= INT32(m_DstH))
                    y = INT32(m_DstH) - 1;
            }

            if (m_SendXY16)
            {
                m_Data[v + vn] = (UINT32(x) & 0xffff) | ((UINT32(y) & 0xffff)<<16);
            }
            else
            {
                m_Data[2*(v+vn)]   = UINT32(x);
                m_Data[2*(v+vn)+1] = UINT32(y);
            }
        }
    }
}

//------------------------------------------------------------------------------
// Fill data for pixels-from-cpu (i.e. image from pushbuffer) draws.
void TwodPickHelper::PickPixels()
{
    m_PcpuSrcW &= 0xffff;
    m_PcpuSrcH &= 0xffff;

    if (m_PcpuSrcW < 1)
        m_PcpuSrcW = 1;
    if (m_PcpuSrcH < 1)
        m_PcpuSrcH = 1;

    if (!m_ClipEnable)
    {
        // By experiment, here are the rules for avoiding limit violations:
        //  - we will send m_PcpuSrcW * m_PcpuSrcH input pixels
        //  - input width and height are unsigned
        //  - output rect is m_PcpuSrcW * dXdU wide by m_PcpuSrcH * dYdV high.
        //  - dXdU and dYdV are real numbers (may be negative or fractional)
        //  - except that for index data, dXdU and dYdV are effectively 1.0
        //  - if (dXdU < 0), output rect grows to left, etc.
        //  - if (abs(dXdU) < 0), output rect is less wide than input rect, etc.
        //  - input surface appears to be unused
        //
        // We need to fix up our input W,H, output X,Y, dXdU, and dYdV to
        // keep from writing off the edge of the surface (unless clip-rect is
        // enabled).
        // We need to figure out how much random input data to generate based on
        // the input W,H and pixel data format.

        double DxDu = m_PcpuDxDuInt;
        if (m_PcpuDxDuInt >= 0)
            DxDu += m_PcpuDxDuFrac * 1.0 / 4294967295.0;
        else
            DxDu += m_PcpuDxDuFrac * -1.0 / 4294967295.0;

        double DyDv = m_PcpuDyDvInt;
        if (m_PcpuDyDvInt >= 0)
            DyDv += m_PcpuDyDvFrac * 1.0 / 4294967295.0;
        else
            DyDv += m_PcpuDyDvFrac * -1.0 / 4294967295.0;

        if (LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR != m_PcpuDataType)
        {
            DxDu = 1.0;
            m_PcpuDxDuInt = 1;
            m_PcpuDxDuFrac = 0;
            DyDv = 1.0;
            m_PcpuDyDvInt = 1;
            m_PcpuDyDvFrac = 0;
        }
        if ((fabs(DxDu) < 1.0e-3) ||    // Avoid div-0 errors below.
            (fabs(DxDu) > m_DstW/2))   // Avoid a single input pixel overdrawing the whole screen
        {
            DxDu = 1.0;
            m_PcpuDxDuInt = 1;
            m_PcpuDxDuFrac = 0;
        }
        if ((fabs(DyDv) < 1.0e-3) ||    // Avoid div-0 errors below.
            (fabs(DyDv) > m_DstH/2))    // Avoid a single input pixel overdrawing the whole screen
        {
            DyDv = 1.0;
            m_PcpuDyDvInt = 1;
            m_PcpuDyDvFrac = 0;
        }

        if (0 > m_PcpuDstXInt)
        {
            // Don't start off-surface to the left.
            m_PcpuDstXInt = 0;
        }
        double DstX0 = m_PcpuDstXInt + m_PcpuDstXFrac * 1.0 / 4294967295.0;

        if (0 > m_PcpuDstYInt)
        {
            // Don't start off-surface to the top.
            m_PcpuDstYInt = 0;
        }
        double DstY0 = m_PcpuDstYInt + m_PcpuDstYFrac * 1.0 / 4294967295.0;

        double DstX1 = DstX0 + m_PcpuSrcW * DxDu;
        double DstY1 = DstY0 + m_PcpuSrcH * DyDv;

        if (UINT32(DstX0 + 0.5) > m_DstW)
        {
            // Don't start off-surface to the right.
            if (DxDu >= 0.0)
                m_PcpuDstXInt = 0;
            else
                m_PcpuDstXInt = m_DstW-1;

            DstX0 = m_PcpuDstXInt + m_PcpuDstXFrac * 1.0 / 4294967295.0;
            DstX1 = DstX0 + m_PcpuSrcW * DxDu;
        }
        if (UINT32(DstY0 + 0.5) > m_DstH)
        {
            // Don't start off-surface to the bottom.
            if (DyDv >= 0.0)
                m_PcpuDstYInt = 0;
            else
                m_PcpuDstYInt = m_DstH-1;

            DstY0 = m_PcpuDstYInt + m_PcpuDstYFrac * 1.0 / 4294967295.0;
            DstY1 = DstY0 + m_PcpuSrcH * DyDv;
        }
        if (DstX1 > m_DstW)
        {
            // Don't end off-surface to the right (DxDu is positive).
            m_PcpuSrcW = UINT32((m_DstW - DstX0) / DxDu);
            if (m_PcpuSrcW < 1)
            {
                m_PcpuSrcW = 1;
                m_PcpuDstXInt = INT32(m_DstW - DxDu) - 1;
            }
        }
        if (DstX1 < 0.0)
        {
            // Don't end off-surface to the left (DxDu is negative).
            m_PcpuSrcW = UINT32(DstX0 / -DxDu);
            if (m_PcpuSrcW < 1)
            {
                m_PcpuSrcW = 1;
                m_PcpuDstXInt = INT32(-DxDu) + 1;
            }
        }
        if (DstY1 > m_DstH)
        {
            // Don't end off-surface to the bottom (DyDv is positive).
            m_PcpuSrcH = UINT32((m_DstH - DstY0) / DyDv);
            if (m_PcpuSrcH < 1)
            {
                m_PcpuSrcH = 1;
                m_PcpuDstYInt = INT32(m_DstH - DyDv) - 1;
            }
        }
        if (DstY1 < 0.0)
        {
            // Don't end off-surface to the top (DyDv is negative).
            m_PcpuSrcH = UINT32(DstY0 / -DyDv);
            if (m_PcpuSrcH < 1)
            {
                m_PcpuSrcH = 1;
                m_PcpuDstYInt = INT32(-DyDv) + 1;
            }
        }
    }

    UINT32 pixelsToSend = m_PcpuSrcW * m_PcpuSrcH;
    UINT32 bitsPerPixel = 0;
    if (LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR == m_PcpuDataType)
    {
        bool isFpOrSrgb;
        bool isY;
        bool isTesla;
        bool isValidForSolid;
        GetFmtInfo(m_PcpuColorFormat, &bitsPerPixel, &isFpOrSrgb, &isY, &isTesla, &isValidForSolid);
    }
    else
    {
        switch (m_PcpuIndexFormat)
        {
            case LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1:  bitsPerPixel = 1; break;
            case LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I4:  bitsPerPixel = 4; break;
            case LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8:  bitsPerPixel = 8; break;
        }
    }
    MASSERT(bitsPerPixel);
    m_NumWordsToSend = (pixelsToSend * bitsPerPixel + 31) / 32;

    for (UINT32 i = 0; i < m_NumWordsToSend && i < m_DataSize/sizeof(UINT32); i++)
        m_Data[i] = m_pRandom2d->GetRandom();
}

//------------------------------------------------------------------------------
void TwodPickHelper::PickImage()
{
    // Always start within the dest surface.
    m_PmemDstX0 = m_PmemDstX0 % m_DstW;
    m_PmemDstY0 = m_PmemDstY0 % m_DstH;

    if (!m_ClipEnable)
    {
        // Make sure the output rect is within the destination surface.
        if (m_PmemDstX0 + m_PmemDstW > m_DstW)
            m_PmemDstW = m_DstW - m_PmemDstX0;
        if (m_PmemDstY0 + m_PmemDstH > m_DstH)
            m_PmemDstH = m_DstH - m_PmemDstY0;
    }
    else
    {
        // Just avoid the launch check and let the cliprect protect the
        // dest surface against limit violations.
        m_PmemDstW &= 0xffff;
        m_PmemDstH &= 0xffff;
    }

    // Now, try to avoid limit violations caused by reading off the edges
    // of the source surface.

    double DuDx = m_PmemDuDxInt;
    if (m_PmemDuDxInt >= 0)
        DuDx += m_PmemDuDxFrac * 1.0 / 4294967295.0;
    else
        DuDx += m_PmemDuDxFrac * -1.0 / 4294967295.0;

    double DvDy = m_PmemDvDyInt;
    if (m_PmemDvDyInt >= 0)
        DvDy += m_PmemDvDyFrac * 1.0 / 4294967295.0;
    else
        DvDy += m_PmemDvDyFrac * -1.0 / 4294967295.0;

    if ((fabs(DuDx) < 1.0e-3) ||    // Avoid div-0 errors below.
        (fabs(DuDx) > m_SrcW/2))    // Avoid a single input pixel overdrawing the whole screen
    {
        DuDx = 1.0;
        m_PmemDuDxInt = 1;
        m_PmemDuDxFrac = 0;
    }
    if ((fabs(DvDy) < 1.0e-3) ||    // Avoid div-0 errors below.
        (fabs(DvDy) > m_SrcH/2))    // Avoid a single input pixel overdrawing the whole screen
    {
        DvDy = 1.0;
        m_PmemDvDyInt = 1;
        m_PmemDvDyFrac = 0;
    }

    if (0 > m_PmemSrcX0Int)
    {
        // Don't start off-surface to the left.
        m_PmemSrcX0Int = 0;
    }
    double SrcX0 = m_PmemSrcX0Int + m_PmemSrcX0Frac * 1.0 / 4294967295.0;

    if (0 > m_PmemSrcY0Int)
    {
        // Don't start off-surface to the top.
        m_PmemSrcY0Int = 0;
    }
    double SrcY0 = m_PmemSrcY0Int + m_PmemSrcY0Frac * 1.0 / 4294967295.0;

    double SrcX1 = SrcX0 + m_PmemDstW * DuDx;
    double SrcY1 = SrcY0 + m_PmemDstH * DvDy;

    if (UINT32(SrcX0 + 0.5) > m_SrcW)
    {
        // Don't start off-surface to the right.
        if (DuDx >= 0.0)
            m_PmemSrcX0Int = 0;
        else
            m_PmemSrcX0Int = m_SrcW-1;

        SrcX0 = m_PmemSrcX0Int + m_PmemSrcX0Frac * 1.0 / 4294967295.0;
        SrcX1 = SrcX0 + m_PmemDstW * DuDx;
    }
    if (UINT32(SrcY0 + 0.5) > m_SrcH)
    {
        // Don't start off-surface to the bottom.
        if (DvDy >= 0.0)
            m_PmemSrcY0Int = 0;
        else
            m_PmemSrcY0Int = m_SrcH-1;

        SrcY0 = m_PmemSrcY0Int + m_PmemSrcY0Frac * 1.0 / 4294967295.0;
        SrcY1 = SrcY0 + m_PmemDstH * DvDy;
    }
    if (SrcX1 > m_SrcW)
    {
        // Don't end off-surface to the right (DuDx is positive).
        m_PmemDstW = UINT32((m_SrcW - SrcX0) / DuDx);
        if (m_PmemDstW < 1)
        {
            m_PmemDstW = 1;
            m_PmemSrcX0Int = INT32(m_SrcW - DuDx) - 1;
        }
    }
    if (SrcX1 < 0.0)
    {
        // Don't end off-surface to the left (DuDx is negative).
        m_PmemDstW = UINT32(SrcX0 / -DuDx);
        if (m_PmemDstW < 1)
        {
            m_PmemDstW = 1;
            m_PmemSrcX0Int = INT32(-DuDx) + 1;
        }
    }
    if (SrcY1 > m_SrcH)
    {
        // Don't end off-surface to the bottom (DvDy is positive).
        m_PmemDstH = UINT32((m_SrcH - SrcY0) / DvDy);
        if (m_PmemDstH < 1)
        {
            m_PmemDstH = 1;
            m_PmemSrcY0Int = INT32(m_SrcH - DvDy) - 1;
        }
    }
    if (SrcY1 < 0.0)
    {
        // Don't end off-surface to the top (DvDy is negative).
        m_PmemDstH = UINT32(SrcY0 / -DvDy);
        if (m_PmemDstH < 1)
        {
            m_PmemDstH = 1;
            m_PmemSrcY0Int = INT32(-DvDy) + 1;
        }
    }
}

//------------------------------------------------------------------------------
// TODO: Much of this work is duplicated in ColorUtils. It may be worthwhile to
//       refactor some of this code at some point.
//
// '!IsTesla' is not a great classification, but this is unfortunately how the
// checks appear to have been designed for FERMI_TWOD_A. Refer to
//      //hw/fermi_gf100/class/mfs/class/2d/fermi_twod.mfs
// for the launch check code that these rules are based off of.
void TwodPickHelper::GetFmtInfo
(
    UINT32 fmt,
    UINT32 * pBitsPerPixel,
    bool * pIsFpOrSrgb,
    bool * pIsY,
    bool * pIsTesla,
    bool * pIsValidForSolid
)
{
    // default to failure
    *pBitsPerPixel = 0;

    switch (fmt)
    {
        case LW502D_SET_DST_FORMAT_V_A8R8G8B8:
        case LW502D_SET_DST_FORMAT_V_A2R10G10B10:
        case LW502D_SET_DST_FORMAT_V_A8B8G8R8:
        case LW502D_SET_DST_FORMAT_V_A2B10G10R10:
        case LW502D_SET_DST_FORMAT_V_X8R8G8B8:
        case LW502D_SET_DST_FORMAT_V_X8B8G8R8:
            *pBitsPerPixel = 32;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_A8RL8GL8BL8:
        case LW502D_SET_DST_FORMAT_V_A8BL8GL8RL8:
        case LW502D_SET_DST_FORMAT_V_X8RL8GL8BL8:
        case LW502D_SET_DST_FORMAT_V_X8BL8GL8RL8:
            *pBitsPerPixel = 32;
            *pIsFpOrSrgb = true;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = false;
            break;

        case LW502D_SET_DST_FORMAT_V_R5G6B5:
        case LW502D_SET_DST_FORMAT_V_A1R5G5B5:
        case LW502D_SET_DST_FORMAT_V_X1R5G5B5:
            *pBitsPerPixel = 16;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_Y8:
            *pBitsPerPixel = 8;
            *pIsFpOrSrgb = false;
            *pIsY = true;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_Y16:
            *pBitsPerPixel = 16;
            *pIsFpOrSrgb = false;
            *pIsY = true;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_Y32:
            *pBitsPerPixel = 32;
            *pIsFpOrSrgb = false;
            *pIsY = true;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_Z1R5G5B5:
        case LW502D_SET_DST_FORMAT_V_O1R5G5B5:
            *pBitsPerPixel = 16;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_Z8R8G8B8:
        case LW502D_SET_DST_FORMAT_V_O8R8G8B8:
            *pBitsPerPixel = 32;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_Y1_8X8:
            *pBitsPerPixel = 1;
            *pIsFpOrSrgb = false;
            *pIsY = true;
            *pIsTesla = true;
            *pIsValidForSolid = true;
            break;

        case LW502D_SET_DST_FORMAT_V_RF16:
            *pBitsPerPixel = 16;
            *pIsFpOrSrgb = true;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = false;
            break;

        case LW502D_SET_DST_FORMAT_V_RF32:
            *pBitsPerPixel = 32;
            *pIsFpOrSrgb = true;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = false;
            break;

        case LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_X16:
            *pBitsPerPixel = 64;
            *pIsFpOrSrgb = true;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = false;
            break;

        case LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_X32:
            *pBitsPerPixel = 128;
            *pIsFpOrSrgb = true;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = false;
            break;

        // The following three formats are not new to Fermi, however, under
        // Fermi they may be used in solid primitive operations, whereas
        // this was not allowed on older chips.

        case LW502D_SET_DST_FORMAT_V_RF32_GF32:
        case LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16:
            *pBitsPerPixel = 64;
            *pIsFpOrSrgb = true;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = m_pGrAlloc->GetClass() == FERMI_TWOD_A;
            break;

        case LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32:
            *pBitsPerPixel = 128;
            *pIsFpOrSrgb = true;
            *pIsY = false;
            *pIsTesla = true;
            *pIsValidForSolid = m_pGrAlloc->GetClass() == FERMI_TWOD_A;
            break;

        // The remaining color formats are exclusive to FERMI_TWO_A. Note that
        // there are some signed and floating point formats here, but they are
        // handled differently than LW50_TWOD FpOrSrgb types.

        case LW902D_SET_DST_FORMAT_V_RN8:
        case LW902D_SET_DST_FORMAT_V_A8:
            *pBitsPerPixel = 8;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = false;
            *pIsValidForSolid = false;
            break;

        case LW902D_SET_DST_FORMAT_V_G8R8:
        case LW902D_SET_DST_FORMAT_V_GN8RN8:
        case LW902D_SET_DST_FORMAT_V_RN16:
            *pBitsPerPixel = 16;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = false;
            *pIsValidForSolid = false;
            break;

        case LW902D_SET_DST_FORMAT_V_BF10GF11RF11:
        case LW902D_SET_DST_FORMAT_V_AN8BN8GN8RN8:
        case LW902D_SET_DST_FORMAT_V_RF16_GF16:
        case LW902D_SET_DST_FORMAT_V_R16_G16:
        case LW902D_SET_DST_FORMAT_V_RN16_GN16:
            *pBitsPerPixel = 32;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = false;
            *pIsValidForSolid = false;
            break;

        case LW902D_SET_DST_FORMAT_V_R16_G16_B16_A16:
        case LW902D_SET_DST_FORMAT_V_RN16_GN16_BN16_AN16:
            *pBitsPerPixel = 64;
            *pIsFpOrSrgb = false;
            *pIsY = false;
            *pIsTesla = false;
            *pIsValidForSolid = false;
            break;

        default:
            MASSERT(!"TwodPickHelper::GetFmtInfo unknown color format");
    }
}

//------------------------------------------------------------------------------
// TODO: Much of this work is duplicated in ColorUtils. It may be worthwhile to
//       refactor some of this code at some point.
const char * TwodPickHelper::FmtToName(UINT32 fmt) const
{
    switch (fmt)
    {
        // LW50_TWOD
        case LW502D_SET_DST_FORMAT_V_Y1_8X8: return "Y1_8X8";
        case LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32: return "RF32_GF32_BF32_AF32";
        case LW502D_SET_DST_FORMAT_V_RF32_GF32_BF32_X32: return "RF32_GF32_BF32_X32";
        case LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16: return "RF16_GF16_BF16_AF16";
        case LW502D_SET_DST_FORMAT_V_RF32_GF32: return "RF32_GF32";
        case LW502D_SET_DST_FORMAT_V_RF16_GF16_BF16_X16: return "RF16_GF16_BF16_X16";
        case LW502D_SET_DST_FORMAT_V_A8R8G8B8: return "A8R8G8B8";
        case LW502D_SET_DST_FORMAT_V_A8RL8GL8BL8: return "A8RL8GL8BL8";
        case LW502D_SET_DST_FORMAT_V_A2B10G10R10: return "A2B10G10R10";
        case LW502D_SET_DST_FORMAT_V_A8B8G8R8: return "A8B8G8R8";
        case LW502D_SET_DST_FORMAT_V_A8BL8GL8RL8: return "A8BL8GL8RL8";
        case LW502D_SET_DST_FORMAT_V_A2R10G10B10: return "A2R10G10B10";
        case LW502D_SET_DST_FORMAT_V_RF32: return "RF32";
        case LW502D_SET_DST_FORMAT_V_X8R8G8B8: return "X8R8G8B8";
        case LW502D_SET_DST_FORMAT_V_X8RL8GL8BL8: return "X8RL8GL8BL8";
        case LW502D_SET_DST_FORMAT_V_R5G6B5: return "R5G6B5";
        case LW502D_SET_DST_FORMAT_V_A1R5G5B5: return "A1R5G5B5";
        case LW502D_SET_DST_FORMAT_V_Y16: return "Y16";
        case LW502D_SET_DST_FORMAT_V_RF16: return "RF16";
        case LW502D_SET_DST_FORMAT_V_Y8: return "Y8";
        case LW502D_SET_DST_FORMAT_V_X1R5G5B5: return "X1R5G5B5";
        case LW502D_SET_DST_FORMAT_V_X8B8G8R8: return "X8B8G8R8";
        case LW502D_SET_DST_FORMAT_V_X8BL8GL8RL8: return "X8BL8GL8RL8";
        case LW502D_SET_DST_FORMAT_V_Z1R5G5B5: return "Z1R5G5B5";
        case LW502D_SET_DST_FORMAT_V_O1R5G5B5: return "O1R5G5B5";
        case LW502D_SET_DST_FORMAT_V_Z8R8G8B8: return "Z8R8G8B8";
        case LW502D_SET_DST_FORMAT_V_O8R8G8B8: return "O8R8G8B8";
        case LW502D_SET_DST_FORMAT_V_Y32: return "Y32";

        // FERMI_TWOD_A
        case LW902D_SET_DST_FORMAT_V_R16_G16_B16_A16: return "R16_G16_B16_A16";
        case LW902D_SET_DST_FORMAT_V_RN16_GN16_BN16_AN16: return "RN16_GN16_BN16_AN16";
        case LW902D_SET_DST_FORMAT_V_BF10GF11RF11: return "BF10GF11RF11";
        case LW902D_SET_DST_FORMAT_V_AN8BN8GN8RN8: return "AN8BN8GN8RN8";
        case LW902D_SET_DST_FORMAT_V_RF16_GF16: return "RF16_GF16";
        case LW902D_SET_DST_FORMAT_V_R16_G16: return "R16_G16";
        case LW902D_SET_DST_FORMAT_V_RN16_GN16: return "RN16_GN16";
        case LW902D_SET_DST_FORMAT_V_G8R8: return "G8R8";
        case LW902D_SET_DST_FORMAT_V_GN8RN8: return "GN8RN8";
        case LW902D_SET_DST_FORMAT_V_RN16: return "RN16";
        case LW902D_SET_DST_FORMAT_V_RN8: return "RN8";
        case LW902D_SET_DST_FORMAT_V_A8: return "A8";
    }
    return "???";
}

//------------------------------------------------------------------------------
/* virtual */
void TwodPickHelper::Print(Tee::Priority pri) const
{
    const char * strPrim[] =
    {
        "POINTS",
        "LINES",
        "POLYLINE",
        "TRIANGLES",
        "RECTS",
        "Imgcpu",
        "Imgmem"
    };
    const char * strPattColFmt[] =
    {
        "A8X8R5G6B5",
        "A1R5G5B5",
        "A8R8G8B8",
        "A8Y8",
        "A8X8Y16",
        "Y32",
        "BYTE_EXPAND"
    };
    const char * strTwodOps[] =
    {
        "SRCCOPY_AND",
        "ROP_AND",
        "BLEND_AND",
        "SRCCOPY",
        "ROP",
        "SRCCOPY_PREMULT",
        "BLEND_PREMULT"
    };

    Printf(pri, "TWOD Op=%s ",strTwodOps[m_Op]);

    switch (m_Op)
    {
        case LW502D_SET_OPERATION_V_SRCCOPY_AND:
        case LW502D_SET_OPERATION_V_SRCCOPY:
            break;

        case LW502D_SET_OPERATION_V_ROP:
        case LW502D_SET_OPERATION_V_ROP_AND:
            Printf(pri, " rop=0x%02x Patt xy=%d,%d sel=%s ",
                    m_Rop,
                    m_PattOffsetX,
                    m_PattOffsetY,
                    strPattSelects[m_PattSelect]);
            if (m_PattSelect == LW502D_SET_PATTERN_SELECT_V_COLOR)
            {
                Printf(pri, " cfmt=");
                switch (m_PattColorLoadMethod)
                {
                    case LW502D_COLOR_PATTERN_X8R8G8B8(0):
                        Printf(pri, "X8R8G8B8");
                        break;
                    case LW502D_COLOR_PATTERN_R5G6B5(0):
                        Printf(pri, "R5G6B5");
                        break;
                    case LW502D_COLOR_PATTERN_X1R5G5B5(0):
                        Printf(pri, "X1R5G5B5");
                        break;
                    case LW502D_COLOR_PATTERN_Y8(0):
                        Printf(pri, "Y8");
                        break;
                    default:
                        MASSERT(!"TwodPickHelper::Print unknown PattColorLoadMethod");
                }
            }
            else
            {
                Printf(pri, " cfmt=%s c0,1=0x%08x,0x%08x fmt=%s patt=%02x %02x %02x %02x  %02x %02x %02x %02x",
                        strPattColFmt[m_PattMonoColorFormat],
                        m_PattMonoColor[0],
                        m_PattMonoColor[1],
                        strMonoFormats[m_PattMonoFormat + 1],
                        m_PattMonoPatt[0] & 0xff,
                        (m_PattMonoPatt[0] >>  8) & 0xff,
                        (m_PattMonoPatt[0] >> 16) & 0xff,
                        (m_PattMonoPatt[0] >> 24) & 0xff,
                        m_PattMonoPatt[1] & 0xff,
                        (m_PattMonoPatt[1] >>  8) & 0xff,
                        (m_PattMonoPatt[1] >> 16) & 0xff,
                        (m_PattMonoPatt[1] >> 24) & 0xff);
            }
            break;

        case LW502D_SET_OPERATION_V_BLEND_AND:
            Printf(pri, " Beta1=0x%08x", m_Beta1);
            break;

        case LW502D_SET_OPERATION_V_SRCCOPY_PREMULT:
        case LW502D_SET_OPERATION_V_BLEND_PREMULT:
            Printf(pri, " Beta4=0x%08x", m_Beta4);
            break;
    }
    Printf(pri, "\n");

    Printf(pri, "  src %s %dx%d %s pitch:%d offset:0x%02x%08x + 0x%x\n",
           FmtToName(m_SrcFmt),
           m_SrcW,
           m_SrcH,
           strLayout[m_SrcSurf->GetLayout()],
           m_SrcPitch,
           UINT32(m_SrcSurf->GetCtxDmaOffsetGpu() >> 32),
           UINT32(m_SrcSurf->GetCtxDmaOffsetGpu()),
           m_SrcOffset);
    Printf(pri, "  dst %s %dx%d %s pitch:%d offset:0x%02x%08x + 0x%x\n",
           FmtToName(m_DstFmt),
           m_DstW,
           m_DstH,
           strLayout[m_DstSurf->GetLayout()],
           m_DstPitch,
           UINT32(m_DstSurf->GetCtxDmaOffsetGpu() >> 32),
           UINT32(m_DstSurf->GetCtxDmaOffsetGpu()),
           m_DstOffset);

    Printf(pri, " NumTpcs=%d RenEna=%d BEctrl=0x%08x \n",
            m_NumTpcs,
            m_RenderEnable,
            m_BigEndianCtrl);

    if (m_ClipEnable)
    {
        Printf(pri, " Clip xy=%d,%d wh=%d,%d\n",
                m_ClipX,
                m_ClipY,
                m_ClipW,
                m_ClipH);
    }
    if (m_CKeyEnable)
    {
        Printf(pri, " CKey fmt=%d color=0x%08x\n",
            m_CKeyFormat,
            m_CKeyColor);
    }

    Printf(pri, " %s", strPrim[m_Prim]);
    switch (m_Prim)
    {
        case RND2D_TWOD_PRIM_Points:
        case RND2D_TWOD_PRIM_Lines:
        case RND2D_TWOD_PRIM_PolyLine:
        case RND2D_TWOD_PRIM_Triangles:
        case RND2D_TWOD_PRIM_Rects:
        {
            UINT32 numV;
            if (m_SendXY16)
                numV = m_NumWordsToSend;
            else
                numV = m_NumWordsToSend/2;

            Printf(pri, " Vtx N=%d %s colFmt=%s LinTieBits=0x%08x\n",
                numV,
                m_SendXY16 ? "16b" : "32b",
                FmtToName(m_PrimColorFormat),
                m_LineTieBreakBits);
            Printf(pri, "  color=[0x%08x, 0x%08x, 0x%08x, 0x%08x]\n",
                m_PrimColor[0],
                m_PrimColor[1],
                m_PrimColor[2],
                m_PrimColor[3]);

            for (UINT32 v = 0; v < numV; v += m_VxPerPrim)
            {
                for (UINT32 v2 = v; v2 < v + m_VxPerPrim; v2++)
                {
                    if (m_SendXY16)
                        Printf(pri, " %d,%d",
                                INT16(m_Data[v2] & 0xffff),
                                INT16((m_Data[v2] >> 16) & 0xffff));
                    else
                        Printf(pri, " %d,%d",
                                INT32(m_Data[v2*2]),
                                INT32(m_Data[v2*2 + 1]));
                }
                Printf(pri, "\n");
            }
            break;
        }
        case RND2D_TWOD_PRIM_Imgcpu:
        {
            if (LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR == m_PcpuDataType)
            {
                Printf(pri, " color %s\n", FmtToName(m_PcpuColorFormat));
            }
            else
            {
                if (m_PcpuIndexFormat == LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I1)
                {
                    Printf(pri, " idx/mono %s %s %s 0x%08x,0x%08x",
                            strMonoFormats[m_PcpuMonoFormat + 1],
                            FmtToName(m_PcpuColorFormat),
                            m_PcpuMonoOpacity ? "opaque" : "",
                            m_PcpuMonoColor[0],
                            m_PcpuMonoColor[1]);

                }
                else
                {
                    Printf(pri, " idx/palette %dbit",
                            LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I4 == m_PcpuIndexFormat ? 4 : 8);

                }
                Printf(pri, " %s\n", m_PcpuIdxWrap ? "idxwrap" : "");
            }

            double DxDu = m_PcpuDxDuInt;
            if (m_PcpuDxDuInt >= 0)
                DxDu += m_PcpuDxDuFrac * 1.0 / 4294967295.0;
            else
                DxDu += m_PcpuDxDuFrac * -1.0 / 4294967295.0;

            double DyDv = m_PcpuDyDvInt;
            if (m_PcpuDyDvInt >= 0)
                DyDv += m_PcpuDyDvFrac * 1.0 / 4294967295.0;
            else
                DyDv += m_PcpuDyDvFrac * -1.0 / 4294967295.0;

            double DstX = m_PcpuDstXInt;
            if (m_PcpuDstXInt >= 0)
                DstX += m_PcpuDstXFrac * 1.0 / 4294967295.0;
            else
                DstX += m_PcpuDstXFrac * -1.0 / 4294967295.0;

            double DstY = m_PcpuDstYInt;
            if (m_PcpuDstYInt >= 0)
                DstY += m_PcpuDstYFrac * 1.0 / 4294967295.0;
            else
                DstY += m_PcpuDstYFrac * -1.0 / 4294967295.0;

            Printf(pri, " in: %dx%d DxDu: %.3f DyDv: %.3f out: %.1f,%.1f\n",
                    m_PcpuSrcW,
                    m_PcpuSrcH,
                    DxDu,
                    DyDv,
                    DstX,
                    DstY);
            break;
        }

        case RND2D_TWOD_PRIM_Imgmem:
        {
            Printf(pri, " blkshp: %s %s %s %s\n",
                    m_PmemBlockShape == LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_AUTO ? "auto"
                        : m_PmemBlockShape == LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_SHAPE_8X4 ? "8x4"
                        : "16x2",
                    m_PmemSafeOverlap ? "safeOvl" : "",
                    m_PmemOrigin == LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CENTER ? "center"
                        : "corner",
                    m_PmemFilter == LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT ? "point"
                        : "bilinear");

            double DuDx = m_PmemDuDxInt;
            if (m_PmemDuDxInt >= 0)
                DuDx += m_PmemDuDxFrac * 1.0 / 4294967295.0;
            else
                DuDx += m_PmemDuDxFrac * -1.0 / 4294967295.0;

            double DvDy = m_PmemDvDyInt;
            if (m_PmemDvDyInt >= 0)
                DvDy += m_PmemDvDyFrac * 1.0 / 4294967295.0;
            else
                DvDy += m_PmemDvDyFrac * -1.0 / 4294967295.0;

            double SrcX0 = m_PmemSrcX0Int + m_PmemSrcX0Frac * 1.0 / 4294967295.0;
            double SrcY0 = m_PmemSrcY0Int + m_PmemSrcY0Frac * 1.0 / 4294967295.0;

            Printf(pri, "  In : %.2fx%.2f at %.2f,%.2f (dudx %.3f dvdy %.3f)\n",
                    DuDx * m_PmemDstW,
                    DvDy * m_PmemDstH,
                    SrcX0,
                    SrcY0,
                    DuDx,
                    DvDy);
            Printf(pri, "  Out: %dx%d at %d,%d\n",
                    m_PmemDstW,
                    m_PmemDstH,
                    m_PmemDstX0,
                    m_PmemDstY0);
            break;
        }
        default:
            MASSERT(!"TwodPickHelper::Print unknown Prim");
    }
}

//------------------------------------------------------------------------------
/*virtual*/
RC TwodPickHelper::Instantiate(Channel * pCh)
{
    pCh->SetObject(scTwod, m_pGrAlloc->GetHandle());

    // Semaphore, only used for the conditional-render state.
    // We don't test this right now.
    // pCh->Write(scTwod, LW502D_SET_SEMAPHORE_CONTEXT_DMA, @);
    // pCh->Write(scTwod, LW502D_SET_RENDER_ENABLE_A, @);
    // pCh->Write(scTwod, LW502D_SET_RENDER_ENABLE_B, @);

    return m_pRandom2d->m_Notifier.Instantiate(scTwod);
}

//------------------------------------------------------------------------------
void TwodPickHelper::SetDefaultPicks(UINT32 wSrc, UINT32 wDst, UINT32 colorDepth)
{
    m_SrcSurf = m_pRandom2d->GetMemoryInfo((Random2d::WhichMem)wSrc);
    m_DstSurf = m_pRandom2d->GetMemoryInfo((Random2d::WhichMem)wDst);

    m_Prim              = LW502D_RENDER_SOLID_PRIM_MODE_V_RECTS;
    m_VxPerPrim         = 4;
    m_SendXY16          = false;
    m_PrimColor[0]      = 0;
    m_PrimColor[1]      = 0;
    m_PrimColor[2]      = 0;
    m_PrimColor[3]      = 0;
    m_Source            = RND2D_TWOD_SOURCE_SrcToDst;
    m_PcpuIdxWrap       = LW502D_SET_PIXELS_FROM_CPU_INDEX_WRAP_V_NO_WRAP;
    m_NumTpcs           = LW502D_SET_NUM_TPCS_V_ALL;
    m_RenderEnable      = LW502D_SET_RENDER_ENABLE_C_MODE_TRUE;
    m_BigEndianCtrl     = 0;

    switch (colorDepth)
    {
        case 32:
        default:
            m_DstFmt = LW502D_SET_DST_FORMAT_V_A8R8G8B8;
            m_SrcFmt = LW502D_SET_SRC_FORMAT_V_A8R8G8B8;
            break;

        case 16:
            m_DstFmt = LW502D_SET_DST_FORMAT_V_R5G6B5;
            m_SrcFmt = LW502D_SET_SRC_FORMAT_V_R5G6B5;
            break;

        case 15:
            m_DstFmt = LW502D_SET_DST_FORMAT_V_A1R5G5B5;
            m_SrcFmt = LW502D_SET_SRC_FORMAT_V_A1R5G5B5;
            break;
    }

    m_DstOffset         = 0;
    m_DstPitch          = m_DstSurf->GetPitch();
    m_DstW              = m_DstSurf->GetWidth();
    m_DstH              = m_DstSurf->GetHeight();

    m_SrcOffset         = 0;
    m_SrcPitch          = m_SrcSurf->GetPitch();
    m_SrcH              = m_SrcSurf->GetHeight();
    m_SrcW              = m_SrcSurf->GetWidth();

    m_ClipEnable        = false;
    m_ClipX             = 0;
    m_ClipY             = 0;
    m_ClipW             = m_DstW;
    m_ClipH             = m_SrcW;

    m_CKeyEnable        = false;
    m_CKeyFormat        = LW502D_SET_COLOR_KEY_FORMAT_V_A8R8G8B8;
    m_CKeyColor         = 0;

    m_Rop               = 0;
    m_Beta1             = 0;
    m_Beta4             = 0;
    m_Op                = LW502D_SET_OPERATION_V_SRCCOPY;
    m_PrimColorFormat   = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8;
    m_LineTieBreakBits  = 0;

    m_PattOffsetX       = 0;
    m_PattOffsetY       = 0;
    m_PattSelect        = LW502D_SET_PATTERN_SELECT_V_MONOCHROME_8x8;
    m_PattMonoColorFormat   = LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_A8R8G8B8;
    m_PattMonoFormat    = LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_LE_M1;
    m_PattMonoColor[0]  = 0;
    m_PattMonoColor[1]  = 0xffffffff;
    m_PattMonoPatt[0]   = 0;
    m_PattMonoPatt[1]   = 0;
    m_PattColorLoadMethod = LW502D_COLOR_PATTERN_X8R8G8B8(0);
    memset(m_PattColorImage, 0, sizeof(m_PattColorImage));

    m_NumWordsToSend    = 0;
    m_DoNotify          = false;

    memset(m_Data, 0, sizeof(m_Data));
}

//------------------------------------------------------------------------------
/* virtual */
RC TwodPickHelper::Send(Channel * pCh)
{
    RC rc;

    // Destination surface
    UINT64 dstOffsetHw = m_DstSurf->GetCtxDmaOffsetGpu() + m_DstOffset;
    if (m_pGrAlloc->GetClass() == LW50_TWOD) // No CtxDmas in Fermi and beyond
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_CONTEXT_DMA,
            m_DstSurf->GetCtxDmaHandleGpu()));
    }
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_OFFSET_UPPER,
            0xff & UINT32(dstOffsetHw >> 32)));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_OFFSET_LOWER,
            UINT32(dstOffsetHw)));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_FORMAT,  m_DstFmt));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_WIDTH,   m_DstW));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_HEIGHT,  m_DstH));

    if (m_DstSurf->GetLayout() == Surface2D::BlockLinear)
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_MEMORY_LAYOUT,
                LW502D_SET_DST_MEMORY_LAYOUT_V_BLOCKLINEAR));
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_BLOCK_SIZE,
            DRF_NUM(502D, _SET_DST_BLOCK_SIZE, _WIDTH, m_DstSurf->GetLogBlockWidth()) |
            DRF_NUM(502D, _SET_DST_BLOCK_SIZE, _HEIGHT, m_DstSurf->GetLogBlockHeight()) |
            DRF_DEF(502D, _SET_DST_BLOCK_SIZE, _DEPTH, _ONE_GOB)));
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_DEPTH, 1)); // @@@ randomize?
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_LAYER, 0)); // @@@ randomize?
    }
    else
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_MEMORY_LAYOUT,
                LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH));
    }
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_DST_PITCH, m_DstPitch));

    // Source surface
    UINT64 srcOffsetHw = m_SrcSurf->GetCtxDmaOffsetGpu() + m_SrcOffset;
    if (m_pGrAlloc->GetClass() == LW50_TWOD) // No CtxDmas in Fermi and beyond
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_CONTEXT_DMA,
            m_SrcSurf->GetCtxDmaHandleGpu()));
    }
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_OFFSET_UPPER,
            0xff & UINT32(srcOffsetHw >> 32)));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_OFFSET_LOWER,
            UINT32(srcOffsetHw)));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_FORMAT,  m_SrcFmt));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_WIDTH,   m_SrcW));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_HEIGHT,  m_SrcH));

    if (m_SrcSurf->GetLayout() == Surface2D::BlockLinear)
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_MEMORY_LAYOUT,
                LW502D_SET_SRC_MEMORY_LAYOUT_V_BLOCKLINEAR));
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_BLOCK_SIZE,
            DRF_NUM(502D, _SET_SRC_BLOCK_SIZE, _WIDTH, m_SrcSurf->GetLogBlockWidth()) |
            DRF_NUM(502D, _SET_SRC_BLOCK_SIZE, _HEIGHT, m_SrcSurf->GetLogBlockHeight()) |
            DRF_DEF(502D, _SET_SRC_BLOCK_SIZE, _DEPTH, _ONE_GOB)));
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_DEPTH, 1)); // @@@ randomize?
        if (m_pGrAlloc->GetClass() == LW50_TWOD)
        {
            // This method was removed in later chips
            CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_LAYER, 0)); // @@@ randomize?
        }
    }
    else
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_MEMORY_LAYOUT,
                LW502D_SET_SRC_MEMORY_LAYOUT_V_PITCH));
    }
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_SRC_PITCH, m_SrcPitch));

    CHECK_RC(pCh->Write(scTwod, LW502D_SET_NUM_TPCS, m_NumTpcs));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_BIG_ENDIAN_CONTROL, m_BigEndianCtrl));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_RENDER_ENABLE_C, m_RenderEnable));

    if (m_ClipEnable)
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_CLIP_X0,  5,
                /* LW502D_SET_CLIP_X0 */        m_ClipX,
                /* LW502D_SET_CLIP_Y0 */        m_ClipY,
                /* LW502D_SET_CLIP_WIDTH */     m_ClipW,
                /* LW502D_SET_CLIP_HEIGHT */    m_ClipH,
                /* LW502D_SET_CLIP_ENABLE */    LW502D_SET_CLIP_ENABLE_V_TRUE));
    }
    else
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_CLIP_ENABLE,
                LW502D_SET_CLIP_ENABLE_V_FALSE));
    }

    if (m_CKeyEnable)
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_COLOR_KEY_FORMAT, 3,
                /* LW502D_SET_COLOR_KEY_FORMAT */   m_CKeyFormat,
                /* LW502D_SET_COLOR_KEY */          m_CKeyColor,
                /* LW502D_SET_COLOR_KEY_ENABLE */   LW502D_SET_COLOR_KEY_ENABLE_V_TRUE));
    }
    else
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_COLOR_KEY_ENABLE,
                LW502D_SET_COLOR_KEY_ENABLE_V_FALSE));
    }

    // Misc 2d rendering state
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_ROP,  4,
            /* LW502D_SET_ROP */        m_Rop,
            /* LW502D_SET_BETA1 */      m_Beta1,
            /* LW502D_SET_BETA4 */      m_Beta4,
            /* LW502D_SET_OPERATION */  m_Op));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT,
            m_PrimColorFormat));
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_RENDER_SOLID_LINE_TIE_BREAK_BITS,
            m_LineTieBreakBits));

    // With class 902D, the launch check validates the pattern select
    // against the destination format regardless of the 2D operation.
    // The pattern select must be updated regardless of the 2D operation
    // to avoid graphics exceptions
    // Pattern (gdi "paintbrush")
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_PATTERN_OFFSET, 2,
            DRF_NUM(502D, _SET_PATTERN_OFFSET, _X,  m_PattOffsetX)
          | DRF_NUM(502D, _SET_PATTERN_OFFSET, _Y,  m_PattOffsetY),
            /* LW502D_SET_PATTERN_SELECT */         m_PattSelect));

    // Some launch checks under FERMI_TWOD_A check parameters regardless of
    // whether or not they are actually used. Always ilwoke to avoid exceptions.
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT, 1,
             /* LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT */ m_PattMonoColorFormat));

    if ((m_Op == LW502D_SET_OPERATION_V_ROP) ||
        (m_Op == LW502D_SET_OPERATION_V_ROP_AND))
    {
        CHECK_RC(pCh->Write(scTwod, LW502D_SET_MONOCHROME_PATTERN_FORMAT, 5,
                /* LW502D_SET_MONOCHROME_PATTERN_FORMAT */  m_PattMonoFormat,
                /* LW502D_SET_MONOCHROME_PATTERN_COLOR0 */  m_PattMonoColor[0],
                /* LW502D_SET_MONOCHROME_PATTERN_COLOR1 */  m_PattMonoColor[1],
                /* LW502D_SET_MONOCHROME_PATTERN0 */        m_PattMonoPatt[0],
                /* LW502D_SET_MONOCHROME_PATTERN1 */        m_PattMonoPatt[1]));
        switch (m_PattColorLoadMethod)
        {
            case LW502D_COLOR_PATTERN_X8R8G8B8(0):
                CHECK_RC(pCh->Write(scTwod, LW502D_COLOR_PATTERN_X8R8G8B8(0),
                        64, m_PattColorImage));
                break;
            case LW502D_COLOR_PATTERN_R5G6B5(0):
                CHECK_RC(pCh->Write(scTwod, LW502D_COLOR_PATTERN_R5G6B5(0),
                        32, m_PattColorImage));
                break;
            case LW502D_COLOR_PATTERN_X1R5G5B5(0):
                CHECK_RC(pCh->Write(scTwod, LW502D_COLOR_PATTERN_X1R5G5B5(0),
                        32, m_PattColorImage));
                break;
            case LW502D_COLOR_PATTERN_Y8(0):
                CHECK_RC(pCh->Write(scTwod, LW502D_COLOR_PATTERN_Y8(0),
                        16, m_PattColorImage));
                break;
            default:
                Printf(Tee::PriHigh, "invalid pattern load method: 0x%x\n",
                        m_PattColorLoadMethod);
                MASSERT(!"Generic mods assert failure.<refer above message>");
                return RC::SOFTWARE_ERROR;
        }
    }

    // This test normally doesn't flush the channel until we need to wait for
    // a notifier.  Sometimes this causes us to run out of pushbuffer space
    // and fail when -no_autoflush is used.
    bool needExtraFlushes = false;
    if (m_NumWordsToSend*4 > m_pRandom2d->GetChannelSize())
        needExtraFlushes = true;

    // Some launch checks under FERMI_TWOD_A check parameters regardless of
    // whether or not they are actually used. Always ilwoke to avoid exceptions.
    CHECK_RC(pCh->Write(scTwod, LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE, 3,
                /* LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE */      m_PcpuDataType,
                /* LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT */   m_PcpuColorFormat,
                /* LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT */   m_PcpuIndexFormat));

    // Which 2d prim we are drawing:
    switch (m_Prim)
    {
        case RND2D_TWOD_PRIM_Points:
        case RND2D_TWOD_PRIM_Lines:
        case RND2D_TWOD_PRIM_PolyLine:
        case RND2D_TWOD_PRIM_Triangles:
        case RND2D_TWOD_PRIM_Rects:
        {
            UINT32 primBitsPerPixel;
            bool   primIsFpOrSrgb;
            bool   primIsY;
            bool   primIsTesla;
            bool   primIsValidForSolid;
            GetFmtInfo(m_PrimColorFormat, &primBitsPerPixel, &primIsFpOrSrgb,
                       &primIsY, &primIsTesla, &primIsValidForSolid);

            CHECK_RC(pCh->Write(scTwod, LW502D_RENDER_SOLID_PRIM_MODE, m_Prim));
            if (primBitsPerPixel > 32)
            {
                CHECK_RC(pCh->Write(scTwod,
                                    LW902D_SET_RENDER_SOLID_PRIM_COLOR0,
                                    (primBitsPerPixel + 31) / 32,
                                    &m_PrimColor[0]));
            }
            else
            {
                CHECK_RC(pCh->Write(scTwod, LW502D_SET_RENDER_SOLID_PRIM_COLOR,
                                    m_PrimColor[0]));
            }

            if (m_SendXY16)
            {
                if (m_NumWordsToSend > 0)
                {
                    CHECK_RC(pCh->WriteNonInc(scTwod,
                            LW502D_RENDER_SOLID_PRIM_POINT_X_Y,
                            m_NumWordsToSend,
                            m_Data));
                    if (needExtraFlushes)
                        CHECK_RC(pCh->Flush());
                }
            }
            else
            {
                for (UINT32 wordsSent = 0; wordsSent < m_NumWordsToSend; /**/)
                {
                    // The PRIM_POINT_SET_X(n), _PRIM_POINT_Y(n) pair are
                    // replicated 8 times, so the max length incrementing send
                    // is 16 words.
                    UINT32 wordsThisLoop = 8*2;
                    if (m_NumWordsToSend - wordsSent < wordsThisLoop)
                        wordsThisLoop = m_NumWordsToSend - wordsSent;

                    CHECK_RC(pCh->Write(scTwod,
                            LW502D_RENDER_SOLID_PRIM_POINT_SET_X(0),
                            wordsThisLoop,
                            m_Data + wordsSent));

                    if (needExtraFlushes)
                        CHECK_RC(pCh->Flush());

                    wordsSent += wordsThisLoop;
                }
            }
            break;
        }

        case RND2D_TWOD_PRIM_Imgcpu:
        {
            // LW502D_SET_PIXELS_FROM_CPU_INDEX_WRAP was removed in FERMI_TWOD_A
            // and replaced with LW902D_FLUSH_AND_ILWALIDATE_ROP_MINI_CACHE
            if (m_pGrAlloc->GetClass() == LW50_TWOD)
            {
                CHECK_RC(pCh->Write(scTwod, LW502D_SET_PIXELS_FROM_CPU_INDEX_WRAP, m_PcpuIdxWrap));
            }
            CHECK_RC(pCh->Write(scTwod, LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT, 5,
                    /* LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT */    m_PcpuMonoFormat,
                    /* LW502D_SET_PIXELS_FROM_CPU_WRAP */           m_PcpuIdxWrap,
                    /* LW502D_SET_PIXELS_FROM_CPU_COLOR0 */         m_PcpuMonoColor[0],
                    /* LW502D_SET_PIXELS_FROM_CPU_COLOR1 */         m_PcpuMonoColor[1],
                    /* LW502D_SET_PIXELS_FROM_CPU_MONO_OPACITY */   m_PcpuMonoOpacity));
            CHECK_RC(pCh->Write(scTwod, LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH, 10,
                    /* LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH */      m_PcpuSrcW,
                    /* LW502D_SET_PIXELS_FROM_CPU_SRC_HEIGHT */     m_PcpuSrcH,
                    /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_FRAC */     m_PcpuDxDuFrac,
                    /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_INT */      m_PcpuDxDuInt,
                    /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_FRAC */     m_PcpuDyDvFrac,
                    /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_INT */      m_PcpuDyDvInt,
                    /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_FRAC */    m_PcpuDstXFrac,
                    /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_INT */     m_PcpuDstXInt,
                    /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC */    m_PcpuDstYFrac,
                    /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT */     m_PcpuDstYInt));
            for (UINT32 i = 0; i < m_NumWordsToSend; /**/)
            {
                UINT32 nThisLoop = m_NumWordsToSend - i;
                if (nThisLoop > m_DataSize/sizeof(UINT32))
                    nThisLoop = m_DataSize/sizeof(UINT32);

                CHECK_RC(pCh->WriteNonInc(scTwod, LW502D_PIXELS_FROM_CPU_DATA,
                        nThisLoop, m_Data));

                if (needExtraFlushes)
                    CHECK_RC(pCh->Flush());

                i += nThisLoop;
            }
            break;
        }
        case RND2D_TWOD_PRIM_Imgmem:
        {
            if (m_pGrAlloc->GetClass() == FERMI_TWOD_A)
            {
                CHECK_RC(pCh->Write(scTwod, LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION, 1,
                         /* LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION */
                         DRF_NUM(902D, _SET_PIXELS_FROM_MEMORY_DIRECTION, _HORIZONTAL, m_PmemDirectionH) |
                         DRF_NUM(902D, _SET_PIXELS_FROM_MEMORY_DIRECTION, _VERTICAL,   m_PmemDirectiolw)));
            }

            CHECK_RC(pCh->Write(scTwod, LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE, 1,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE */     m_PmemBlockShape));
            CHECK_RC(pCh->Write(scTwod, LW502D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP, 2,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_SAFE_OVERLAP */    m_PmemSafeOverlap,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE */
                    DRF_NUM(502D, _SET_PIXELS_FROM_MEMORY_SAMPLE_MODE, _ORIGIN, m_PmemOrigin) |
                    DRF_NUM(502D, _SET_PIXELS_FROM_MEMORY_SAMPLE_MODE, _FILTER, m_PmemFilter)));

            CHECK_RC(pCh->Write(scTwod, LW502D_SET_PIXELS_FROM_MEMORY_DST_X0, 12,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DST_X0 */          m_PmemDstX0,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0 */          m_PmemDstY0,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH */       m_PmemDstW,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT */      m_PmemDstH,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC */      m_PmemDuDxFrac,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DU_DX_INT */       m_PmemDuDxInt,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC */      m_PmemDvDyFrac,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_DV_DY_INT */       m_PmemDvDyInt,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC */     m_PmemSrcX0Frac,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT */      m_PmemSrcX0Int,
                    /* LW502D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC */     m_PmemSrcY0Frac,
                    /* LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT */          m_PmemSrcY0Int));
            break;
        }
        default:
            MASSERT(!"TwodPickHelper::Send unknown Prim");
            break;
    }
    if (m_DoNotify)
    {
        m_pRandom2d->m_Notifier.Clear(0);
        CHECK_RC(m_pRandom2d->m_Notifier.Write(scTwod));
        CHECK_RC(pCh->Write(scTwod, LW502D_NO_OPERATION, 0));
    }

    return OK;
}

//------------------------------------------------------------------------------
RC TwodPickHelper::CopySrcToDst(Channel * pCh)
{
    RC rc;

    SetDefaultPicks(Random2d::SrcMem, Random2d::DstMem, 32 /*@@@*/);

    m_Prim              = RND2D_TWOD_PRIM_Imgmem;
    m_PcpuDataType      = LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR;
    m_PcpuColorFormat   = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8;
    m_PcpuIndexFormat   = LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8;
    m_PmemBlockShape    = LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_AUTO;
    m_PmemSafeOverlap   = false;
    m_PmemOrigin        = LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CORNER;
    m_PmemFilter        = LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT;
    m_PmemDstX0         = 0;
    m_PmemDstY0         = 0;
    m_PmemDstW          = m_DstSurf->GetWidth();
    m_PmemDstH          = m_DstSurf->GetHeight();
    m_PmemDuDxFrac      = 0;
    m_PmemDuDxInt       = 1;
    m_PmemDvDyFrac      = 0;
    m_PmemDvDyInt       = 1;
    m_PmemSrcX0Frac     = 0;
    m_PmemSrcX0Int      = 0;
    m_PmemSrcY0Frac     = 0;
    m_PmemSrcY0Int      = 0;
    m_PmemDirectionH    = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_HW_DECIDES;
    m_PmemDirectiolw    = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL_HW_DECIDES;
    m_DoNotify          = true;

    Printf(Tee::PriDebug, "CopySrcToDst\n");
    Print(Tee::PriDebug);
    CHECK_RC( Instantiate(pCh) );
    rc = Send(pCh);
    return rc;
}

//------------------------------------------------------------------------------
RC TwodPickHelper::CopyHostToSrc(Channel * pCh)
{
    RC rc;

    SetDefaultPicks(Random2d::SysMem, Random2d::SrcMem, 32 /*@@@*/);

    m_Prim              = RND2D_TWOD_PRIM_Imgmem;
    m_PcpuDataType      = LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR;
    m_PcpuColorFormat   = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8;
    m_PcpuIndexFormat   = LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8;
    m_PmemBlockShape    = LW502D_SET_PIXELS_FROM_MEMORY_BLOCK_SHAPE_V_AUTO;
    m_PmemSafeOverlap   = false;
    m_PmemOrigin        = LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_ORIGIN_CORNER;
    m_PmemFilter        = LW502D_SET_PIXELS_FROM_MEMORY_SAMPLE_MODE_FILTER_POINT;
    m_PmemDstX0         = 0;
    m_PmemDstY0         = 0;
    m_PmemDstW          = m_DstSurf->GetWidth();
    m_PmemDstH          = m_DstSurf->GetHeight();
    m_PmemDuDxFrac      = 0;
    m_PmemDuDxInt       = 1;
    m_PmemDvDyFrac      = 0;
    m_PmemDvDyInt       = 1;
    m_PmemSrcX0Frac     = 0;
    m_PmemSrcX0Int      = 0;
    m_PmemSrcY0Frac     = 0;
    m_PmemSrcY0Int      = 0;
    m_PmemDirectionH    = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_HW_DECIDES;
    m_PmemDirectiolw    = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL_HW_DECIDES;
    m_DoNotify          = true;

    Printf(Tee::PriDebug, "CopyHostToSrc\n");
    Print(Tee::PriDebug);
    CHECK_RC( Instantiate(pCh) );
    rc = Send(pCh);
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
RC TwodPickHelper::ClearScreen
        (
        Channel * pCh,
        UINT32    DacColorDepth
        )
{
    RC rc;

    SetDefaultPicks(Random2d::DstMem, Random2d::DstMem, DacColorDepth);
    m_Prim              = RND2D_TWOD_PRIM_Imgcpu;
    m_PcpuDataType      = LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR;
    m_PcpuColorFormat   = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8;
    m_PcpuIndexFormat   = LW502D_SET_PIXELS_FROM_CPU_INDEX_FORMAT_V_I8;
    m_PcpuIdxWrap       = LW502D_SET_PIXELS_FROM_CPU_INDEX_WRAP_V_NO_WRAP;
    m_PcpuMonoFormat    = LW502D_SET_PIXELS_FROM_CPU_MONO_FORMAT_V_CGA6_M1;
    m_PcpuMonoOpacity   = 1;
    m_PcpuSrcW          = 1;
    m_PcpuSrcH          = 1;
    m_PcpuDxDuFrac      = 0;
    m_PcpuDxDuInt       = m_DstSurf->GetWidth();
    m_PcpuDyDvFrac      = 0;
    m_PcpuDyDvInt       = m_DstSurf->GetHeight();
    m_PcpuDstXFrac      = 0;
    m_PcpuDstXInt       = 0;
    m_PcpuDstYFrac      = 0;
    m_PcpuDstYInt       = 0;
    m_NumWordsToSend    = 1;
    m_PmemDirectionH    = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_HORIZONTAL_HW_DECIDES;
    m_PmemDirectiolw    = LW902D_SET_PIXELS_FROM_MEMORY_DIRECTION_VERTICAL_HW_DECIDES;
    m_Data[0]           = m_pRandom2d->m_Misc.ClearColor();
    m_DoNotify          = true;

    Printf(Tee::PriDebug, "ClearScreen to 0x%08x\n", m_Data[0]);
    Print(Tee::PriDebug);
    CHECK_RC( Instantiate(pCh) );
    rc = Send(pCh);

    return rc;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// MSVC7 should shut up about "'this' : used in base member initializer list"
#pragma warning( disable : 4355 )

Random2d::Random2d() :
    m_Misc(this),
    m_Twod(this),
    m_RgbClear(false),
    m_CpuCopy(false),
    m_hCh(0),
    m_pCh(NULL),
    m_DstIsSurfA(true),
    m_SrcIsFilled(false),
    m_pGGSurfs(NULL)
{
    SetName("Random2d");

#ifdef DEBUG
    memset(m_AllPickHelpers, 0, sizeof(m_AllPickHelpers));
#endif

    m_pFpCtx = GetFpContext();

    m_AllPickHelpers[RND2D_MISC]   = &m_Misc;
    m_AllPickHelpers[RND2D_TWOD]   = &m_Twod;

#ifdef DEBUG
    // Don't forget to add a line above, if you add a new PickHelper!
    for (int i = 0; i < RND2D_NUM_RANDOMIZERS; i++)
        MASSERT(0 != m_AllPickHelpers[i]);
#endif

    // Tell all PickHelpers to initialize their FancyPicker objects.
    SetDefaultPickers(0, RND2D_NUM_RANDOMIZERS * RND2D_BASE_OFFSET - 1);

#ifdef DEBUG
    // Make sure all FancyPickers have sane default values.
    for (int i = 0; i < RND2D_NUM_RANDOMIZERS; i++)
        m_AllPickHelpers[i]->CheckInitialized();
#endif

    // Tell all PickHelpers the address of the Pick context.
    for (int i = 0; i < RND2D_NUM_RANDOMIZERS; i++)
        m_AllPickHelpers[i]->SetContext(m_pFpCtx);
}

//------------------------------------------------------------------------------
Random2d::~Random2d()
{
    FreeMem();
}

//------------------------------------------------------------------------------
static void GetPatternProperty(vector<UINT32> * pvec, JSObject * testObj, const char * propertyName)
{
    (*pvec).clear();

    JavaScriptPtr pJs;
    JsArray ptnArray;
    jsval jsv;
    if ((OK == (pJs->GetPropertyJsval(testObj, propertyName, &jsv))) &&
        (OK == (pJs->FromJsval(jsv, &ptnArray))))
    {
        (*pvec).reserve(ptnArray.size());

        if (ptnArray.size())
        {
            for (UINT32 i = 0; i < ptnArray.size(); i++)
            {
                UINT32 tmpi;
                pJs->FromJsval(ptnArray[i], &tmpi);
                (*pvec).push_back(tmpi);
            }
        }
    }
}

//------------------------------------------------------------------------------
static void PrintPatternProperty(Tee::Priority pri, const char * propertyName, const vector<UINT32> & vec)
{
    Printf(pri, "\t%s:\t", propertyName);

    if (0 == vec.size())
    {
        Printf(pri, "random\n");
    }
    else
    {
        Printf(pri, "[0x%08x", vec[0]);
        for (UINT32 i = 1; i < vec.size(); i++)
            Printf(pri, ",0x%08x", vec[i]);
        Printf(pri, "]\n");
    }
}

//------------------------------------------------------------------------------
void Random2d::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);

    Printf(pri, "\tCpuCopy:\t%s\n", strFT[m_CpuCopy]);
    Printf(pri, "\tRgbClear:\t%s\n", strFT[m_RgbClear]);
    PrintPatternProperty(pri, "TopFillPattern", m_TopFillPattern);
    PrintPatternProperty(pri, "BottomFillPattern", m_BottomFillPattern);

    m_SurfA.Print(pri, "\tSurfA.");
    m_SurfB.Print(pri, "\tSurfB.");
    m_SurfC.Print(pri, "\tSurfC.");
}

//------------------------------------------------------------------------------
RC Random2d::InitFromJs()
{
    RC rc;

    // Call base-class function (for TestConfiguration and Golden properties).
    CHECK_RC(GpuTest::InitFromJs());

    // Get the fill pattern for src surfaces.
    GetPatternProperty(&m_TopFillPattern,    GetJSObject(), "TopFillPattern");
    GetPatternProperty(&m_BottomFillPattern, GetJSObject(), "BottomFillPattern");

    // Get SurfA properties:
    // Default to match TestConfiguration, probably tiled FB memory.
    m_SurfA.SetWidth(GetTestConfiguration()->SurfaceWidth());
    m_SurfA.SetHeight(GetTestConfiguration()->SurfaceHeight());
    m_SurfA.SetColorFormat(ColorUtils::ColorDepthToFormat(GetTestConfiguration()->DisplayDepth()));
    m_SurfA.SetLocation(GetTestConfiguration()->DstLocation());
    m_SurfA.SetProtect(Memory::ReadWrite);
    m_SurfA.SetTiled(GetTestConfiguration()->UseTiledSurface());
    m_SurfA.SetLayout(Surface2D::Pitch);
    m_SurfA.SetDisplayable(true);
    m_SurfA.SetType(LWOS32_TYPE_IMAGE);
    CHECK_RC(m_SurfA.InitFromJs(GetJSObject(), "SurfA")); // Allow more specific overrides.

    // Get SurfB properties:
    // Default to match TestConfiguration, probably simple pitch-linear FB memory.
    m_SurfB.SetWidth(GetTestConfiguration()->SurfaceWidth());
    m_SurfB.SetHeight(GetTestConfiguration()->SurfaceHeight());
    m_SurfB.SetColorFormat(ColorUtils::ColorDepthToFormat(GetTestConfiguration()->DisplayDepth()));
    m_SurfB.SetLocation(GetTestConfiguration()->SrcLocation());
    m_SurfB.SetProtect(Memory::ReadWrite);
    m_SurfB.SetLayout(Surface2D::Pitch);
    m_SurfB.SetDisplayable(true);
    m_SurfB.SetType(LWOS32_TYPE_IMAGE);
    CHECK_RC(m_SurfB.InitFromJs(GetJSObject(), "SurfB")); // Allow more specific overrides.

    // Get SurfC properties:
    // Default to match TestConfiguration, probably simple non-coherent host memory.
    m_SurfC.SetWidth(GetTestConfiguration()->SurfaceWidth());
    m_SurfC.SetHeight(GetTestConfiguration()->SurfaceHeight());
    m_SurfC.SetColorFormat(ColorUtils::ColorDepthToFormat(GetTestConfiguration()->DisplayDepth()));
    m_SurfC.SetLocation(GetTestConfiguration()->MemoryType());
    m_SurfC.SetProtect(Memory::ReadWrite);
    m_SurfC.SetLayout(Surface2D::Pitch);
    m_SurfC.SetDisplayable(false);
    m_SurfC.SetType(LWOS32_TYPE_IMAGE);
    CHECK_RC(m_SurfC.InitFromJs(GetJSObject(), "SurfC")); // Allow more specific overrides.

    return OK;
}

//------------------------------------------------------------------------------
bool Random2d::IsSupported()
{
    // First we have to sync up C++ with any test specific items set from JS
    if (InitFromJs() != OK)
        return false;

    GpuDevice * pGpudev = GetBoundGpuDevice();
    GpuSubdevice * pGpuSubdevice = GetBoundGpuSubdevice();
    Memory::Location SurfALocation =
        Surface2D::GetActualLocation(m_SurfA.GetLocation(), pGpuSubdevice);
    Memory::Location SurfBLocation =
        Surface2D::GetActualLocation(m_SurfB.GetLocation(), pGpuSubdevice);

    if ((SurfALocation == Memory::NonCoherent || SurfALocation == Memory::Coherent) &&
        (m_SurfA.GetLayout() == Surface2D::BlockLinear) &&
        (!pGpuSubdevice->IsSOC()))
    {
        return false;
    }

    if ((SurfBLocation == Memory::NonCoherent || SurfBLocation == Memory::Coherent) &&
        (m_SurfB.GetLayout() == Surface2D::BlockLinear) &&
        (!pGpuSubdevice->IsSOC()))
    {
        return false;
    }

    if ((SurfALocation == Memory::NonCoherent ||
         SurfALocation == Memory::Coherent ||
         SurfBLocation == Memory::NonCoherent ||
         SurfBLocation == Memory::Coherent) &&
         !pGpudev->SupportsRenderToSysMem())
    {
        return false;
    }

    if ((SurfALocation == Memory::Fb || SurfBLocation == Memory::Fb) &&
        (pGpuSubdevice->FbHeapSize() == 0) )
    {
        return false;
    }

    for (int i = 0; i < RND2D_NUM_RANDOMIZERS; i++)
    {
        if (!m_AllPickHelpers[i]->IsSupported(pGpudev))
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
bool Random2d::IsSupportedVf()
{
    return !(GetBoundGpuSubdevice()->IsSMCEnabled());
}

//------------------------------------------------------------------------------
RC Random2d::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());

    // Learn the pitch, decide what size surfaces to allocate.
    UINT32 pitch = GetTestConfiguration()->SurfaceWidth() *
                   GetTestConfiguration()->DisplayDepth() / 8;
    CHECK_RC(AdjustPitchForScanout(&pitch));

    // Allocate memory and GPU resources:
    rc = m_AllPickHelpers[RND2D_TWOD]->Alloc(GetTestConfiguration(), &m_pCh, &m_hCh);
    if (OK != rc)
    {
        // User-supplied class number is bad, or unexpected error.
        Printf(Tee::PriError, "Failed to alloc channel and object of class 0x%x.\n",
               m_AllPickHelpers[RND2D_TWOD]->GetClass());
        return rc;
    }
    CHECK_RC( m_Notifier.Allocate(m_pCh, 10, GetTestConfiguration()) );
    CHECK_RC( AllocMem(pitch) );

    if (m_SurfA.GetDisplayable())
    {
        CHECK_RC(GetDisplayMgrTestContext()->DisplayImage(&m_SurfA,
            DisplayMgr::DONT_WAIT_FOR_UPDATE));
    }


    // Install HW objects, set their context handles, etc.
    for (int i = 0; i < RND2D_NUM_RANDOMIZERS; i++)
    {
        if (IsAllocated(i))
        {
            rc = m_AllPickHelpers[i]->Instantiate(m_pCh);
            if (OK != rc)
            {
                Printf(Tee::PriHigh, "Failed to instantiate object of class 0x%x.\n",
                       m_AllPickHelpers[i]->GetClass());
                return rc;
            }
        }
    }

    // Initialize our Goldelwalues object.
    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    m_pGGSurfs->AttachSurface(GetMemoryInfo(DstMem), "C", 0);
    CHECK_RC(GetGoldelwalues()->SetSurfaces(m_pGGSurfs));
    GetGoldelwalues()->SetPrintCallback(&Random2d_Print, this);

    return rc;
}

//------------------------------------------------------------------------------
RC Random2d::Run()
{
    RC rc;

    UINT32 StartLoop        = GetTestConfiguration()->StartLoop();
    UINT32 RestartSkipCount = GetTestConfiguration()->RestartSkipCount();
    UINT32 EndLoop          = StartLoop + GetTestConfiguration()->Loops();
    UINT32 LoopsPerGolden   = GetGoldelwalues()->GetSkipCount();
    if (0 == LoopsPerGolden)
       LoopsPerGolden = 1;

    if ((StartLoop % RestartSkipCount) != 0)
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");

    for (m_pFpCtx->LoopNum = StartLoop; m_pFpCtx->LoopNum < EndLoop; ++m_pFpCtx->LoopNum)
    {
        if ((m_pFpCtx->LoopNum == StartLoop) || ((m_pFpCtx->LoopNum % RestartSkipCount) == 0))
        {
            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / RestartSkipCount;
            CHECK_RC( Restart() );
        }

        // Make sure we flush & wait before each golden check.
        m_Misc.ForceNotify(m_pFpCtx->LoopNum % LoopsPerGolden == LoopsPerGolden - 1);

        CHECK_RC( DoOneLoop() );

        if (m_Misc.Stop())
            break;

        if (m_Misc.Skip())
            continue;

        GetGoldelwalues()->SetLoop(m_pFpCtx->LoopNum);

        if (OK != (rc = GetGoldelwalues()->Run()))
        {
            Printf(Tee::PriHigh, "Golden error %d after loop %d\n",
                   INT32(rc), m_pFpCtx->LoopNum);
            break;
        }
    }

    // if ((OK != rc) && (Tee::GetCirlwlarSink()))
    // {
    //     Tee::GetCirlwlarSink()->Dump(Tee::FileOnly);
    // }

    if ( OK == rc )
        rc = GetGoldelwalues()->ErrorRateTest(GetJSObject());
    else
        (void) GetGoldelwalues()->ErrorRateTest(GetJSObject());

    return rc;
}

//------------------------------------------------------------------------------
RC Random2d::Cleanup()
{
    StickyRC rc;

    delete m_pGGSurfs;
    m_pGGSurfs = NULL;
    GetGoldelwalues()->ClearSurfaces();

    for (int i = 0; i < RND2D_NUM_RANDOMIZERS; i++)
    {
        m_AllPickHelpers[i]->Free();
    }

    if (GetDisplayMgrTestContext())
    {
        rc = GetDisplayMgrTestContext()->DisplayImage((Surface2D *)NULL,
                DisplayMgr::WAIT_FOR_UPDATE);
    }

    FreeMem();
    m_Notifier.Free();
    rc = GetTestConfiguration()->FreeChannel(m_pCh);
    rc = GpuTest::Cleanup();

    return rc;
}

//------------------------------------------------------------------------------
void FillSrc(void * addr, UINT32 offset, UINT32 bytes, vector<UINT32> & pattern)
{
    char * p = offset + (char *)addr;
    if (pattern.size())
    {
        Memory::FillPattern(p, &pattern, bytes);
    }
    else
    {
        Memory::FillRandom(
                          p,
                          offset,           // seed
                          0, 0xffffffff,    // min, max
                          bytes/4,          // width in pixels
                          1,                // height in pixels
                          32,               // pixel size in bits
                          bytes);           // pitch in bytes
    }
}

//------------------------------------------------------------------------------
RC Random2d::AllocMem
        (
        UINT32 DacPitch
        )
{
    RC rc;

    // Destination -- fancy surface: tiled or host-tiled or blocklinear.
    CHECK_RC(m_SurfA.Alloc(GetBoundGpuDevice()));

    // Source -- simple surface: untiled, pitch-linear.
    CHECK_RC(m_SurfB.Alloc(GetBoundGpuDevice()));

    // Host -- sysmem surface coherent or noncoherent.
    CHECK_RC(m_SurfC.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SurfC.Map());

    // Fix pitch, width, height.
    //
    // If the surface is BlockLinear, we might not have a valid Pitch.
    // If the surface is PitchLinear, we might not have a valid Width.
    //
    // I could fix this in Surface2D, but the arch display tests use that class,
    // and I'm afraid to make any change to the behavior of any visible interface
    // of that class for fear that some of 10,000 poorly-written tests will fail
    // months from now in HW branches I've never heard of.
    //
    for (int i = 0; i < 3; i++)
    {
        Surface2D * pSurf = (i==0) ? &m_SurfA : (i==1) ? &m_SurfB : &m_SurfC;

        if (ColorUtils::LWFMT_NONE == pSurf->GetColorFormat())
            pSurf->SetColorFormat(ColorUtils::ColorDepthToFormat(GetTestConfiguration()->DisplayDepth()));

        if (pSurf->GetSize() > 0xFFFFFFFF)
        {
            Printf(Tee::PriHigh, "Random2d doesn't support surfaces >= 4GB\n");
            MASSERT(!"Generic mods assert failure.<refer above message>");
            return RC::DATA_TOO_LARGE;
        }

        if (0 == pSurf->GetPitch())
            pSurf->SetPitch(UINT32( pSurf->GetSize() / pSurf->GetHeight() ));

        if (0 == pSurf->GetWidth())
            pSurf->SetWidth(pSurf->GetPitch() / pSurf->GetBytesPerPixel());

        if (0 == pSurf->GetHeight())
            pSurf->SetHeight(UINT32( pSurf->GetSize() / pSurf->GetPitch() ));

        Printf(Tee::PriDebug, "Surface %c size 0x%llx, %dx%d pitch %d\n",
               'A' + i,
               pSurf->GetSize(),
               pSurf->GetWidth(),
               pSurf->GetHeight(),
               pSurf->GetPitch());
    }

    // Bind the dma contexts to the GPU.
    CHECK_RC(m_SurfA.BindGpuChannel(m_hCh));
    CHECK_RC(m_SurfB.BindGpuChannel(m_hCh));
    CHECK_RC(m_SurfC.BindGpuChannel(m_hCh));

    // Fill the system memory image.  This image will be copied to whichever
    // of the two FB surfaces is not "Dst" at the beginning of each frame.
    // I.e. before rendering, both the sys and fb source images will have
    // the same data.

    FillSrc(m_SurfC.GetAddress(), 0,                           UINT32(m_SurfC.GetSize()/2), m_TopFillPattern);
    FillSrc(m_SurfC.GetAddress(), UINT32(m_SurfC.GetSize()/2), UINT32(m_SurfC.GetSize()/2), m_BottomFillPattern);

    SeedRandom(0);

    m_DstIsSurfA  = true;
    m_SrcIsFilled = false;

    return OK;
}

//------------------------------------------------------------------------------
void Random2d::FreeMem()
{
    m_SurfA.Free();
    m_SurfB.Free();
    m_SurfC.Free();
}

//------------------------------------------------------------------------------
RC Random2d::Restart(void)
{
    int i;
    RC rc = OK;

    SeedRandom(GetTestConfiguration()->Seed() + m_pFpCtx->LoopNum);

    for (i = 0; i < RND2D_NUM_RANDOMIZERS; i++)
        m_AllPickHelpers[i]->Restart();

    if (m_Misc.Stop())
        return OK;

    Printf(m_Misc.SuperVerbose() ? Tee::PriNormal : Tee::PriLow,
           "\n\tRestart: loop %d, seed %#010x\n",
           m_pFpCtx->LoopNum, GetTestConfiguration()->Seed() + m_pFpCtx->LoopNum);

    // Which 2d helper to use for copying surfaces around?
    PickHelper * pCopyHelper = NULL;  // default to none.
    if (! m_CpuCopy)
    {
        pCopyHelper = &m_Twod;
    }

    if (m_Misc.SwapSrcDst() == m_DstIsSurfA)
    {
        // Whoops, this frame will use swapped surfaces for src vs. dst
        // compared to the previous frame.

        Printf(m_Misc.SuperVerbose() ? Tee::PriNormal : Tee::PriLow,
               "Swapping src and dst surfaces... (new dst is %s)\n",
               m_DstIsSurfA ? "SurfB" : "SurfA");

        if (m_SrcIsFilled && pCopyHelper)
        {
            // Save time, blit old src to new src.
            rc = pCopyHelper->CopySrcToDst(m_pCh);
            if (OK == rc)
                rc = m_pCh->Flush();
            if (OK == rc)
                rc = m_Notifier.Wait(0, GetTestConfiguration()->TimeoutMs());
            if (OK != rc)
            {
                Printf(Tee::PriHigh, "CopySrcToDst failed.\n");
                return rc;
            }
        }
        else
        {
            m_SrcIsFilled = false;
        }
        m_DstIsSurfA  = !m_Misc.SwapSrcDst();

        Surface2D * pSurfDst = m_DstIsSurfA ? &m_SurfA : &m_SurfB;
        if (pSurfDst->GetDisplayable())
        {
            GetDisplayMgrTestContext()->DisplayImage(pSurfDst,
                DisplayMgr::DONT_WAIT_FOR_UPDATE);
        }

        m_pGGSurfs->ReplaceSurface(0, GetMemoryInfo(DstMem));
    }

    if (!m_SrcIsFilled)
    {
        if (pCopyHelper)
        {
            rc = pCopyHelper->CopyHostToSrc(m_pCh);
            if (OK == rc)
                rc = m_pCh->Flush();
            if (OK == rc)
                rc = m_Notifier.Wait(0, GetTestConfiguration()->TimeoutMs());
        }
        else
        {
            // Copy random source image over using CPU read/writes.
            Surface2D * pSrcSurf = &m_SurfC;
            Surface2D * pDstSurf = m_DstIsSurfA ? &m_SurfB : &m_SurfA;

            UINT32 bytes = UINT32(pDstSurf->GetSize());
            if (bytes > pSrcSurf->GetSize())
                bytes = UINT32(pSrcSurf->GetSize());

            rc = pDstSurf->Map();
            if (OK == rc)
            {
                Platform::MemCopy(pDstSurf->GetAddress(), pSrcSurf->GetAddress(), bytes);
                pDstSurf->Unmap();
            }
        }
        m_SrcIsFilled = true;
    }
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "CopyHostToSrc error %d\n", UINT32(rc));
        return rc;
    }

    if (m_RgbClear || !pCopyHelper)
    {
        Surface2D * pDstSurf = m_DstIsSurfA ? &m_SurfA : &m_SurfB;

        if (m_RgbClear)
        {
            rc = pDstSurf->FillPattern(1, 1, "gradient", "color_palette", "horizontal");
        }
        else
        {
            UINT32 clearColor = m_Misc.ClearColor();
            rc = pDstSurf->Fill(clearColor);
        }
    }
    else // HW-accellerated single-color clear
    {
        rc = pCopyHelper->ClearScreen(m_pCh, GetTestConfiguration()->DisplayDepth());
        if (OK == rc)
            rc = m_pCh->Flush();
        if (OK == rc)
            rc = m_Notifier.Wait(0, GetTestConfiguration()->TimeoutMs());
    }
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "ClearScreen error %d\n", UINT32(rc));
        return rc;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Random2d::DoOneLoop(void)
{
    RC rc;
    int i;

    // Do the random picking.
    //
    // The order of calls matters here: some pickers may depend on the results
    // of previous pickers.

    m_Misc.PickFirst();
    if (m_Misc.Stop())
        return OK;

    // Always Pick all context helpers.
    for (i = 0; i < RND2D_FIRST_DRAW_CLASS; i++)
    {
        if (m_AllPickHelpers[i]->GetHandle())
            m_AllPickHelpers[i]->Pick();
    }

    // Pick just the one draw helper.
    MASSERT(m_Misc.WhoDraws() >= RND2D_FIRST_DRAW_CLASS);
    MASSERT(m_Misc.WhoDraws() <  RND2D_NUM_RANDOMIZERS);
    m_AllPickHelpers[m_Misc.WhoDraws()]->Pick();

    // When we are "skipping", we keep the random sequence the same, but
    // don't actually send anything to hardware.

    m_Misc.PickLast();
    if (m_Misc.Skip())
        return OK;

    if (m_Misc.SuperVerbose())
        Print(Tee::PriNormal);

    // Now we send methods to the pushbuffer.

    rc = m_AllPickHelpers[m_Misc.WhoDraws()]->Send(m_pCh);

    if (OK != rc)
    {
        Printf(Tee::PriHigh, "Error %d sending HW methods\n", UINT32(rc));
        Print(Tee::PriHigh);
        return rc;
    }

    for (i = 0; i < (int)m_Misc.Noops(); i++)
    {   // Debugging feature.
        // Note that all draw classes use the same subchannel (7) and that
        // all classes support the same method 0x100 (NOOP).
        CHECK_RC(m_pCh->Write(PickHelper::scTwod, LW502D_NO_OPERATION, 0));
    }

    if (m_Misc.Flush())
    {
        // Flush() is forced true whenever Notify() is true, and may be turned
        // on for other loops where it isn't theoretically needed as a
        // debugging feature.
        m_pCh->Flush();

        if (m_Misc.SleepMs())
        {   // Debugging feature.
            Tasker::Sleep(m_Misc.SleepMs());
        }
    }

    if (m_Misc.Notify())
    {
        // Notify() will be forced true on loops that are golden-checked.
        rc = m_Notifier.Wait(0, GetTestConfiguration()->TimeoutMs());
        if (OK != rc)
        {
            Printf(Tee::PriHigh, "Error %d waiting for notifier\n", UINT32(rc));
            Print(Tee::PriHigh);
        }

        // On SOC, flush the GPU cache for golden values
        if (GetBoundGpuSubdevice()->IsSOC())
        {
            CHECK_RC(m_pCh->WaitIdle(GetTestConfiguration()->TimeoutMs()));
            CHECK_RC(m_pCh->WriteL2FlushDirty());
            CHECK_RC(m_pCh->Flush());
            CHECK_RC(m_pCh->WaitIdle(GetTestConfiguration()->TimeoutMs()));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC Random2d::SetDefaultPickers(UINT32 first, UINT32 last)
{
    if (last >= RND2D_NUM_RANDOMIZERS * RND2D_BASE_OFFSET)
        last = RND2D_NUM_RANDOMIZERS * RND2D_BASE_OFFSET - 1;

    if (first > last)
        return RC::SOFTWARE_ERROR;

    while (first <= last)
    {
        int i = first / RND2D_BASE_OFFSET;

        m_AllPickHelpers[i]->SetDefaultPicker(
                                             first - i*RND2D_BASE_OFFSET,
                                             last  - i*RND2D_BASE_OFFSET);

        first = (i+1) * RND2D_BASE_OFFSET;
    }

    return OK;
}

//------------------------------------------------------------------------------
RC Random2d::PickerFromJsval(int index, jsval value)
{
    if ((index < 0) || (index >= RND2D_NUM_RANDOMIZERS * RND2D_BASE_OFFSET))
        return RC::BAD_PICKER_INDEX;

    return m_AllPickHelpers[index / RND2D_BASE_OFFSET]->PickerFromJsval(
                                                                       index % RND2D_BASE_OFFSET, value);
}

//------------------------------------------------------------------------------
RC Random2d::PickerToJsval(int index, jsval * pvalue)
{
    if ((index < 0) || (index >= RND2D_NUM_RANDOMIZERS * RND2D_BASE_OFFSET))
        return RC::BAD_PICKER_INDEX;

    return m_AllPickHelpers[index / RND2D_BASE_OFFSET]->PickerToJsval(
                                                                     index % RND2D_BASE_OFFSET, pvalue);
}

//------------------------------------------------------------------------------
RC Random2d::PickToJsval(int index, jsval * pvalue)
{
    if ((index < 0) || (index >= RND2D_NUM_RANDOMIZERS * RND2D_BASE_OFFSET))
        return RC::BAD_PICKER_INDEX;

    return m_AllPickHelpers[index / RND2D_BASE_OFFSET]->PickToJsval(
                                                                   index % RND2D_BASE_OFFSET, pvalue);
}

//------------------------------------------------------------------------------
Surface2D *Random2d::GetMemoryInfo
        (
        Random2d::WhichMem  whichMem
        )
{
    if (whichMem == SysMem)
    {
        return &m_SurfC;
    }
    else
    {
        if ((whichMem == DstMem) == m_DstIsSurfA)
            return &m_SurfA;
        else
            return &m_SurfB;
    }
}

//------------------------------------------------------------------------------
bool Random2d::IsAllocated(UINT32 whichHelper) const
{
    MASSERT(whichHelper < RND2D_NUM_RANDOMIZERS);
    MASSERT(m_AllPickHelpers[whichHelper]);

    return(0 != m_AllPickHelpers[whichHelper]->GetClass());
}

//------------------------------------------------------------------------------
FLOAT64 Random2d::TimeoutMs()
{
    return GetTestConfiguration()->TimeoutMs();
}

//------------------------------------------------------------------------------
UINT32 Random2d::GetLoopsPerFrame()
{
    return GetTestConfiguration()->RestartSkipCount();
}

//------------------------------------------------------------------------------
UINT32 Random2d::GetChannelSize()
{
    return GetTestConfiguration()->ChannelSize();
}

//------------------------------------------------------------------------------
void Random2d::SeedRandom(UINT32 s)
{
    return m_pFpCtx->Rand.SeedRandom(s);
}

//------------------------------------------------------------------------------
UINT32 Random2d::GetRandom()
{
    return m_pFpCtx->Rand.GetRandom();
}

//------------------------------------------------------------------------------
UINT32 Random2d::GetRandom(UINT32 min, UINT32 max)
{
    return m_pFpCtx->Rand.GetRandom(min, max);
}

//------------------------------------------------------------------------------
void Random2d::Print(Tee::Priority pri) const
{
    Printf(pri, "--- Loop %d ---\n", m_pFpCtx->LoopNum);

    m_AllPickHelpers[m_Misc.WhoDraws()]->Print(pri);

    if (m_Misc.Notify())
        Printf(pri, "wait for notifier\n");

    Printf(pri, "--- %d ---\n", m_pFpCtx->LoopNum);
}

//-----------------------------------------------------------------------------
/* virtual */ RC Random2d::SetPicker(UINT32 idx, jsval v)
{
    return PickerFromJsval(idx, v);
}

//-----------------------------------------------------------------------------
/* virtual */ UINT32 Random2d::GetPick(UINT32 idx)
{
    jsval result;
    if (OK != PickToJsval(idx, &result))
        return 0;

    return JSVAL_TO_INT(result);
}

//-----------------------------------------------------------------------------
/* virtual */ RC Random2d::GetPicker(UINT32 idx, jsval *v)
{
     return PickerToJsval(idx, v);
}

//------------------------------------------------------------------------------
void Random2d_Print(void * pvRandom2d)
{
    Random2d * pRandom2d = (Random2d *)pvRandom2d;
    pRandom2d->Print(Tee::PriHigh);
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Random2d object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Random2d, GpuTest, "Random 2D graphics test");

CLASS_PROP_READWRITE(Random2d, RgbClear, bool,
        "Use a pretty w,r,g,b gradient for screen clears, instead of solid color.");
CLASS_PROP_READWRITE(Random2d, CpuCopy, bool,
        "Use Cpu writes to copy patterns and clear screen, not 2d HW methods.");
CLASS_PROP_READONLY(Random2d, Loop, UINT32,
        "Current test loop");

static SProperty Random2d_TopFillPattern
        (
        Random2d_Object,
        "TopFillPattern",
        0,
        0,  // default value
        0,
        0,
        0,
        "Fill pattern (array of 32-bit ints) for the top half of source surfaces, use random fill if not an array."
        );
static SProperty Random2d_BottomFillPattern
        (
        Random2d_Object,
        "BottomFillPattern",
        0,
        0,  // default value
        0,
        0,
        0,
        "Fill pattern (array of 32-bit ints) for the bottom half of source surfaces, use random fill if not an array."
        );
