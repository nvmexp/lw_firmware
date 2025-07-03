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

/**
 * @file nwfmats.cpp
 * @brief Implements NewWfMatsTest.
 */

//#define DEBUG_CHECK_RC 1

//@@@ To Do:
// - make benchmark data available to JS scripting

#include "nwfmats.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/framebuf.h"
#include "core/include/lwrm.h"
#include "core/include/channel.h"
#include "core/include/gpu.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include "random.h"
#include "gputestc.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/massert.h"
#include <cmath>
#include "class/cl502d.h" // LW50_TWOD
#include "class/cl5062.h" // LW50_CONTEXT_SURFACES_2D
#include "class/cl902d.h" // FERMI_TWOD
#include "class/cl90b5.h" // GF100_DMA_COPY
#include "class/cla0b5.h" // KEPLER_DMA_COPY_A
#include "class/clc0b5.h" // PASCAL_DMA_COPY_A
#include "class/cla06f.h"
#include "lwos.h"
#include <stack>
#include "core/include/platform.h"
#include "gpu/utility/chansub.h"
#include "core/include/mgrmgr.h"
#include "core/include/bitrot.h"
#include "core/include/utility.h"
#include "core/include/circsink.h"

#include <algorithm>

namespace
{
    // TODO Remove this when there are no issues
    class DetachThread
    {
        public:
            explicit DetachThread(bool enable)
            : m_pDetach(enable ? new Tasker::DetachThread : nullptr)
            {
            }

        private:
            unique_ptr<Tasker::DetachThread> m_pDetach;
    };
}

//------------------------------------------------------------------------------
NewWfMatsTest::NewWfMatsTest()
  : m_NumCopyEngines(0)
  , m_NumBlitEngines(1)
  , m_ReportBandwidth(true)
{
    SetName("NewWfMatsTest");

    m_PatternSet = PatternClass::PATTERNSET_ALL;
    m_PatternClass.SelectPatternSet( m_PatternSet );
    m_PatternCount = m_PatternClass.GetLwrrentNumOfSelectedPatterns();
    m_CpuPattern = 0;
    m_CpuPatternLength = 0;

    m_pRandom = &(GetFpContext()->Rand);
    m_pTestConfig = GetTestConfiguration();

    m_LwrrentReadbackSubdev = 0;

    m_TimeoutMs = 1000;

    m_BoxPixels = 0;
    m_BoxBytes = 0;
    m_BoxesPerMegabyte = 0.0;
    m_DecodeSpan = 0;

    m_BlitsPerBoxCopy = 1;

    m_NumBlitLoops = 1;

    m_CpuTestBytes = static_cast<UINT32>(8_MB);
    m_OuterLoops = 1;
    m_InnerLoops = 1;

    m_NumBoxes = 0;
    m_BlitNumBoxes = 0;
    m_CpuNumBoxes = 0;

    m_ExtraOffsetForHandleError = 0;

    m_TimeBlitLoopStarted = 0;
    m_NumKeepBusyCalls = 0;
    m_CtxDmaOffsetFitIn32Bits = false;

    m_pChSubXor = 0;
    m_pAuxCh = 0;
    m_hAuxCh = 0;

    m_BoxHeight = 0;
    m_ExitEarly = 0;
    m_UseXor = true;
    m_Method = Use2D;
    m_AuxMethod = UseCE;
    m_LegacyUseNarrow = false;
    m_UseNarrowBlits = false;
    m_DoPcieTraffic = true;
    m_UseBlockLinear = false;
    m_VerifyChannelSwizzle = false;
    m_SaveReadBackData = false;
    m_UseVariableBlits = false;
    m_MaxBlitLoopErrors = 1000000;
    m_AuxPayload = 0;

    m_LogError = true;
    m_RetryHasErrors = false;
}

//------------------------------------------------------------------------------
NewWfMatsTest::~NewWfMatsTest()
{
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::InitFromJs()
{
    RC rc;

    CHECK_RC(GpuMemTest::InitFromJs());

    m_PatternClass.FillRandomPattern( m_pTestConfig->Seed() );

    m_TimeoutMs = m_pTestConfig->TimeoutMs();

    if (m_pTestConfig->SurfaceWidth() != c_BoxWidthPixels)
    {
        Printf(Tee::PriNormal,
                "NewWfMats always uses a %d pixel-wide surface.\n"
                "NewWfMats will display oddly with mode %dx%d...\n",
                c_BoxWidthPixels,
                m_pTestConfig->SurfaceWidth(),
                m_pTestConfig->SurfaceHeight());
    }

    m_PatternCount = m_PatternClass.GetLwrrentNumOfSelectedPatterns();
    m_PatternClass.ResetFillIndex();
    m_PatternClass.ResetCheckIndex();

    if (m_Method != Use2D)
        m_UseXor = false;

    m_VariableBlitSeed = m_pTestConfig->Seed();

    return rc;
}

//------------------------------------------------------------------------------
bool NewWfMatsTest::IsSupported()
{
    GpuDevice * pGpuDev = GetBoundGpuDevice();

    if (!GpuMemTest::IsSupported())
        return false;

    m_TwoD.SetOldestSupportedClass(LW50_TWOD);

    if (m_Method == UseCE)
    {
        // Use a temporary CE since the allocation of CEs happens in ::Setup()
        DmaCopyAlloc tmpCE;
        return tmpCE.IsSupported(pGpuDev);
    }
    else if (m_Method == UseVIC)
    {
        return m_VICDma.IsSupported(pGpuDev);
    }
    else if (m_Method == Use2D)
    {
        return m_TwoD.IsSupported(pGpuDev);
    }
    else
    {
        MASSERT(!"Unimplemented method!");
        Printf(Tee::PriError, "Unimplemented method!\n");
        return false;
    }
}

//------------------------------------------------------------------------------
void NewWfMatsTest::PrintJsProperties(Tee::Priority pri)
{
    GpuMemTest::PrintJsProperties(pri);

    // Must match NewWfMatsTest::Method enum
    const char* method[] = { "Use2D", "UseCE", "UseVIC" };

    const char * ft[] = {"false", "true"};

    Printf(pri, "\tCpuPattern:\t\t\t0x%x\n", m_CpuPattern);
    Printf(pri, "\tCpuTestBytes:\t\t\t%d\n", m_CpuTestBytes);
    Printf(pri, "\tOuterLoops:\t\t\t%d\n", m_OuterLoops);
    Printf(pri, "\tInnerLoops:\t\t\t%d\n", m_InnerLoops);
    Printf(pri, "\tBoxHeight:\t\t\t%d\n", m_BoxHeight);
    Printf(pri, "\tExitEarly:\t\t\t%s\n", ft[m_ExitEarly != 0]);
    Printf(pri, "\tUseXor:\t\t\t\t%s\n", ft[m_UseXor]);
    Printf(pri, "\tNumCopyEngines:\t\t\t%u\n", m_NumCopyEngines);
    Printf(pri, "\tMethod:\t\t\t\t%s\n", method[m_Method]);
    Printf(pri, "\tAuxMethod:\t\t\t%s\n", method[m_AuxMethod]);
    Printf(pri, "\tUseNarrowBlits:\t\t\t%s\n", ft[m_UseNarrowBlits]);
    Printf(pri, "\tLegacyUseNarrow:\t\t%s\n", ft[m_LegacyUseNarrow]);
    Printf(pri, "\tDoPcieTraffic:\t\t\t%s\n", ft[m_DoPcieTraffic]);
    Printf(pri, "\tUseBlockLinear:\t\t\t%s\n", ft[m_UseBlockLinear]);
    Printf(pri, "\tVerifyChannelSwizzle:\t\t%s\n", ft[m_VerifyChannelSwizzle]);
    Printf(pri, "\tSaveReadBackData:\t\t%s\n", ft[m_SaveReadBackData]);
    Printf(pri, "\tUseVariableBlits:\t\t%s\n", ft[m_UseVariableBlits]);
    Printf(pri, "\tVariableBlitSeed:\t\t%d\n", m_VariableBlitSeed);
    Printf(pri, "\tMaxBlitLoopErrors:\t\t%d\n", m_MaxBlitLoopErrors);
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::Setup()
{
    RC rc;

    if (m_Method != Use2D &&
        (m_LegacyUseNarrow || m_UseNarrowBlits || m_UseVariableBlits))
    {
        Printf(Tee::PriError, "Non-standard blit modes require TWOD class.\n");
        return RC::BAD_PARAMETER;
    }

    if ((m_UseNarrowBlits || m_LegacyUseNarrow) && m_UseVariableBlits)
    {
        Printf(Tee::PriNormal,
               "NewWfMats cannot use both Narrow and Variable size blits.\n");
        return RC::BAD_PARAMETER;
    }

    if (m_Method == UseVIC)
    {
        m_AuxMethod = UseVIC;
    }
    else
    {
        m_AuxMethod = UseCE;
    }

    // Warn user about block-linear mode on pseudochannel-enabled parts (Bug 1833087)
    if (m_UseBlockLinear &&
        GetBoundGpuSubdevice()->GetFB()->GetPseudoChannelsPerSubpartition() > 1)
    {
        Printf(Tee::PriWarn,
               "In block-linear mode the pseudochannel of errors is incorrectly reported.\n"
               "This also results in incorrectly reported error pins. (Bug 1833087)\n");
    }

    // Reserve for the max number of CEs potentially available. This might be
    // resized down later in AllocAdditionalCopyEngineResources if fewer CEs are
    // to be used.
    const UINT32 maxNumCEs = GetBoundGpuSubdevice()->GetMaxCeCount();
    m_BlitLoopStatus.reserve(maxNumCEs);
    m_pCh.reserve(maxNumCEs);
    m_hCh.reserve(maxNumCEs);
    m_pChSubShift.reserve(maxNumCEs);
    m_CopyEng.reserve(maxNumCEs);
    m_Notifier.reserve(maxNumCEs);

    m_BlitLoopStatus.resize(1);
    m_pCh.resize(1);
    m_hCh.resize(1);
    m_pChSubShift.resize(1);
    m_CopyEng.resize(1);
    m_Notifier.resize(1);

    CHECK_RC(GpuMemTest::Setup());

    CHECK_RC(GpuTest::AllocDisplay());

    if (m_Method == UseVIC)
    {
        m_pTestConfig->SetChannelType(TestConfiguration::TegraTHIChannel);
    }
    else
    {
        m_pTestConfig->SetChannelType(TestConfiguration::GpFifoChannel);
    }

    m_BlitLoopStatus.resize(GetBoundGpuDevice()->GetNumSubdevices());
    for (UINT32 s = 0; s < m_BlitLoopStatus.size(); s++)
    {
        m_BlitLoopStatus[s].resize(1);
    }

    if (m_AuxMethod == UseCE || m_Method == UseCE)
    {
        m_pTestConfig->SetAllowMultipleChannels(true);
    }

    // Initially allocate one engine which will be used during setup
    if (m_Method == Use2D)
    {
        // Allocate the twod object before the FB otherwise there may not be
        // enough space left for the context buffers
        CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh[0],
                                                          &m_hCh[0],
                                                          &m_TwoD));
    }
    else if (m_Method == UseCE)
    {
        CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh[0],
                                                          &m_hCh[0],
                                                          &m_CopyEng[0]));
    }
    else
    {
        CHECK_RC(m_pTestConfig->AllocateChannel(&m_pCh[0], &m_hCh[0]));
    }

    if (m_AuxMethod == UseCE)
    {
        CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pAuxCh,
                                                          &m_hAuxCh,
                                                          &m_AuxCopyEng));
    }

    if (m_Method != UseVIC)
    {
        // Unlike most tests, NewWfMats does not want auto-flush behavior
        m_pCh[0]->SetAutoFlush(false, 0);
    }

    CHECK_RC(AllocFb());

    // Need to calc box-height after AllocFb and before AllocSysmem!
    CHECK_RC(CalcBoxHeight());
    CHECK_RC(BuildBoxTable());

    // Need to Callwlate Number of blit Engine resources after BuildBoxTable()
    // since we cap number of engines to the number of Blit Loops
    CHECK_RC(CalcNumCopyEngines());
    CHECK_RC(AllocAdditionalCopyEngineResources());

    // If additional CopyEngines were allocated, increase number of Blit Engines
    m_NumBlitEngines = max(m_NumBlitEngines, m_NumCopyEngines);

    CHECK_RC(AllocSysmem());
    CHECK_RC(SetupGraphics());

    // Don't time out while writing the channel if the GpFifo is full
    FLOAT64 blitLoopTimeoutMs = m_TimeoutMs * GuessBlitLoopSeconds();
    for (UINT32 i = 0; i < m_NumBlitEngines; i++)
    {
        if (blitLoopTimeoutMs > m_pCh[i]->GetDefaultTimeoutMs())
        {
            m_pCh[i]->SetDefaultTimeoutMs(blitLoopTimeoutMs);
        }
    }

    // Set up our "channel subroutines":
    // These are sysmem surfaces full of methods and data that we will
    // send to the gpu repeatedly with gpentries.
    for (UINT32 i = 0; i < m_NumBlitEngines; i++)
    {
        m_pChSubShift[i] = new ChannelSubroutine(m_pCh[i], m_pTestConfig);
    }

    switch (m_Method)
    {
        case UseCE:
            FillChanSubCopyEngine();
            break;

        case UseVIC:
            PrepareVICBoxes();
            break;

        case Use2D:
            m_pChSubXor = new ChannelSubroutine(m_pCh[0], m_pTestConfig);
            FillChanSubTwoD();
            break;

        default:
            MASSERT(!"Unimplemented method!");
            break;
    }
    if (m_Method != UseVIC)
    {
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            CHECK_RC(m_pChSubShift[i]->FinishSubroutine());

            Printf(Tee::PriDebug,
                    "%s SHIFT pushbuffer is 0x%x (%d) bytes at gpu vaddr 0x%02x:%08x\n",
                    __FUNCTION__,
                    m_pChSubShift[i]->Size(),
                    m_pChSubShift[i]->Size(),
                    UINT32( m_pChSubShift[i]->Offset() >> 32 ),
                    UINT32( m_pChSubShift[i]->Offset() & 0xffffffff ));
        }
    }

    if (m_UseXor)
    {
        CHECK_RC(m_pChSubXor->FinishSubroutine());

        Printf(Tee::PriDebug,
                "%s XOR pushbuffer is 0x%x (%d) bytes at gpu vaddr 0x%02x:%08x\n",
                __FUNCTION__,
                m_pChSubXor->Size(),
                m_pChSubXor->Size(),
                UINT32( m_pChSubXor->Offset() >> 32 ),
                UINT32( m_pChSubXor->Offset() & 0xffffffff ));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::Run()
{
    StickyRC firstRc = OK;

    for (UINT32 i = 0; i < m_OuterLoops; i++)
    {
        firstRc = DoWfMats(i);
        if (OK != firstRc)
            break;
    }

    return firstRc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::RunLiveFbioTuning()
{
    RC rc;
    UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();

    // Write the initial patterns to the FB memory that will be used by
    // the blit loop.
    CHECK_RC(FillBlitLoopBoxes());

    UINT32 numToSend[LW2080_MAX_SUBDEVICES];

    numToSend[0] = m_InnerLoops * m_PatternCount;
    for (UINT32 s = 1; s < numSubdevices; s++)
    {
        numToSend[s] = numToSend[0];
    }

    // ToDo: wait for vblank
    // ToDo: block all R/W to instance memory somehow
    // ToDo: swap normal FBIO register values for tuning values

    InitBlitLoopStatus();
    CHECK_RC(SendSubroutines(numToSend));
    CHECK_RC(WaitForBlitLoopDone(m_DoPcieTraffic, false));

    // ToDo: restore normal values to FBIO tuning registers
    // ToDo: allow instance memory access again

    for (UINT32 s = 0; s < numSubdevices; s++)
        CHECK_RC( CheckBlitBoxes(s) );

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::RunLiveFbioTuningWrapper()
{
    RunWrapperContext ctx;

    RunWrapperFirstHalf(&ctx);

    if (OK == ctx.FirstRc)
    {
        ctx.FirstRc = RunLiveFbioTuning();
    }

    RunWrapperSecondHalf (&ctx);

    return ctx.FirstRc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::Cleanup()
{
    StickyRC firstRc;

    VerbosePrintf("NewWfMatsTest::Cleanup\n");

    if (0 == m_ExitEarly)
    {
        // Idle the channel before freeing anything, to avoid hanging the gpu
        // if we're exiting abnormally with a busy channel.
        for (UINT32 i = 0; i < m_pCh.size(); i++)
        {
            StickyRC loopRc;

            if (m_pCh[i])
                loopRc = m_pCh[i]->WaitIdle(m_TimeoutMs);
            if (loopRc)
            {
                Printf(Tee::PriWarn,
                       "%s: channel is not idle.\n", __FUNCTION__);

                const FLOAT64 longTimeout = m_TimeoutMs * 20;

                Printf(Tee::PriWarn,
                        "Waiting up to %.0f seconds, Ctrl-C to cancel...\n",
                        longTimeout / 1000.0);
                Tee::GetCirlwlarSink()->Dump(Tee::FileOnly);

                loopRc = m_pCh[i]->WaitIdle(longTimeout);
            }

            delete m_pChSubShift[i];
            m_pChSubShift[i] = 0;

            if (m_pCh[i])
            {
                m_TwoD.Free();
                m_CopyEng[i].Free();
                m_VICDma.Free();

                m_MapFb.UnmapFbRegion();

                for (UINT32 s = 0; s < m_BlitLoopStatus.size(); s++)
                {
                    m_BlitLoopStatus[s][i].SemaphoreBuf.Free();
                }

                m_CompareBuf.Free();

                m_Notifier[i].Free();

                if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
                    loopRc = m_pCh[i]->WriteSetSubdevice(Channel::AllSubdevicesMask);

                loopRc = m_pTestConfig->FreeChannel(m_pCh[i]);
                m_pCh[i] = 0;
            }

            firstRc = loopRc;
        }

        delete m_pChSubXor;
        m_pChSubXor = 0;

        m_AuxSemaphore.Free();
        if (m_pAuxCh)
        {
            m_AuxCopyEng.Free();
            firstRc = m_pTestConfig->FreeChannel(m_pAuxCh);
            m_pAuxCh = 0;
        }
        firstRc = GpuMemTest::Cleanup();
    }

    // Free memory allocated to m_BoxTable
    vector<Box> dummy;
    m_BoxTable.swap(dummy);

    m_CpuBoxIndices.clear();
    m_BlitBoxIndices.clear();
    m_DecodeSpan = 0;

    return firstRc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::AllocFb()
{
    RC rc;

    // Reserve 1 MB surface so that there is room in the FrameBuffer
    // to map host memory surfaces after AllocEntireFramebuffer is called
    Surface2D dummySurf;
    dummySurf.SetWidth(static_cast<UINT32>(1_MB));
    dummySurf.SetHeight(1);
    dummySurf.SetColorFormat(ColorUtils::VOID8);
    dummySurf.SetLocation(Memory::Optimal);
    CHECK_RC(dummySurf.Alloc(GetBoundGpuDevice()));

    // Allocate Framebuffer memory
    CHECK_RC(AllocateEntireFramebuffer(
            m_UseBlockLinear,
            m_hCh[0]));

    // Free surface to allow it to be used for other objects
    dummySurf.Free();

    m_CtxDmaOffsetFitIn32Bits = true;

    GpuUtility::MemoryChunks::iterator iChunk;
    for (iChunk= GetChunks()->begin(); iChunk != GetChunks()->end(); ++iChunk)
    {
        UINT64 endOffset = iChunk->size + iChunk->ctxDmaOffsetGpu;
        if (0 != (endOffset >> 32))
        {
            // We must send xxx_OFFSET_UPPER methods.
            m_CtxDmaOffsetFitIn32Bits = false;
        }
    }

    Printf(Tee::PriDebug, "NewWfMats::m_CtxDmaOffsetFitIn32Bits = %s\n",
            m_CtxDmaOffsetFitIn32Bits ? "true" : "false");

    // Set display to view the first allocated region big enough to
    // hold it, if any.

    GpuUtility::MemoryChunkDesc *pDisplayChunk = nullptr;
    UINT64 displayOffset = 0;
    RC trc = GpuUtility::FindDisplayableChunk(
            GetChunks(),
            &pDisplayChunk,
            &displayOffset,
            GetStartLocation(),
            GetEndLocation(),
            m_pTestConfig->DisplayHeight(),
            c_BoxWidthBytes,
            GetBoundGpuDevice());
    if (trc == OK)
    {
        CHECK_RC(GetDisplayMgrTestContext()->DisplayMemoryChunk(
            pDisplayChunk,
            displayOffset,
            m_pTestConfig->SurfaceWidth(),
            m_pTestConfig->SurfaceHeight(),
            m_pTestConfig->DisplayDepth(),
            c_BoxWidthBytes));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::AllocSysmem()
{
    RC rc;

    // Determine the size of the buffer we need to check boxes.  Make it big
    // enough to hold two boxes.
    UINT32 compareBufSize = (m_BoxBytes * 2);
    if (GetNumRechecks())
    {
        // Retry needs system mem of one box size
        compareBufSize = (m_BoxBytes * 3);
    }

    // Allocate Dest (PCI)
    CHECK_RC(m_pTestConfig->ApplySystemModelFlags(
                Memory::Coherent, &m_CompareBuf));
    m_CompareBuf.SetWidth(compareBufSize);
    m_CompareBuf.SetPitch(compareBufSize);
    m_CompareBuf.SetHeight(1);
    m_CompareBuf.SetColorFormat(ColorUtils::Y8);
    m_CompareBuf.SetLocation(Memory::Coherent);
    m_CompareBuf.SetProtect(Memory::ReadWrite);
    CHECK_RC(m_CompareBuf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_CompareBuf.Map());
    CHECK_RC(m_CompareBuf.BindGpuChannel(m_hCh[0]));

    const UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();
    const UINT32 semaphoreBufSize = 4096; // one page

    for (UINT32 i = 0; i < m_NumBlitEngines; i++)
    {
        // Allocate Notifier memory and Context DMAs
        if (m_Method != UseVIC)
        {
            CHECK_RC(m_Notifier[i].Allocate(m_pCh[i], 2,
                                     m_pTestConfig));
        }

        // Set up per-gpu Semaphore surfaces in sysmem to track how far ahead
        // of each GPU we are in the channel.
        for (UINT32 subInst = 0; subInst < numSubdevices; subInst++)
        {
            Surface2D * pSem = & m_BlitLoopStatus[subInst][i].SemaphoreBuf;

            CHECK_RC(m_pTestConfig->ApplySystemModelFlags(
                        Memory::Coherent, pSem));
            pSem->SetWidth(semaphoreBufSize);
            pSem->SetPitch(semaphoreBufSize);
            pSem->SetHeight(1);
            pSem->SetColorFormat(ColorUtils::Y8);
            pSem->SetLocation(Memory::Coherent);
            pSem->SetProtect(Memory::ReadWrite);
            if (m_Method == UseVIC)
            {
                pSem->SetVASpace(Surface2D::TegraVASpace);
            }

            CHECK_RC(pSem->Alloc(GetBoundGpuDevice()));
            CHECK_RC(pSem->Map(subInst));
            CHECK_RC(pSem->BindGpuChannel(m_hCh[i]));

            if (numSubdevices > 1)
                CHECK_RC(m_pCh[i]->WriteSetSubdevice(1<<subInst));

            if (m_Method != UseVIC)
            {
                CHECK_RC(m_pCh[i]->SetContextDmaSemaphore(pSem->GetCtxDmaHandleGpu()));
                CHECK_RC(m_pCh[i]->SetSemaphoreOffset(pSem->GetCtxDmaOffsetGpu()));
            }
        }

        if (numSubdevices > 1)
            CHECK_RC(m_pCh[i]->WriteSetSubdevice(Channel::AllSubdevicesMask));

    }

    if (m_AuxMethod == UseCE || m_AuxMethod == UseVIC)
    {
        m_AuxSemaphore.Setup(NotifySemaphore::ONE_WORD,
                             Memory::Coherent,
                             1);     //Single semaphore instance is needed.
        if (m_AuxMethod == UseCE)
        {
            CHECK_RC(m_AuxSemaphore.Allocate(m_pAuxCh, 0));
        }
        else
        {
            CHECK_RC(m_AuxSemaphore.Allocate(m_pCh[0], 0));
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::CalcNumCopyEngines()
{
    RC rc;

    if (m_Method != UseCE)
    {
        if (m_NumCopyEngines > 0)
        {
            Printf(Tee::PriError,
                   "Copy engine is not being used, but NumCopyEngines is set to: %u\n",
                   m_NumCopyEngines);
            rc = RC::BAD_PARAMETER;
        }

        return rc;
    }

    // Get the number of enabled CEs available for use on this device
    CHECK_RC(m_CopyEng[0].GetEnabledPhysicalEnginesList(GetBoundGpuDevice(),
                                                        &m_EnabledCopyEngine));
    UINT32 maxAvailableCEs = static_cast<UINT32>(m_EnabledCopyEngine.size());

    // If no value was specified for m_NumCopyEngines, then set it to max possible
    m_NumCopyEngines = (m_NumCopyEngines ? m_NumCopyEngines : maxAvailableCEs);
    if (m_NumCopyEngines > maxAvailableCEs)
    {
        Printf(Tee::PriError, "Too many CopyEngines requested: %u. Max available: %u\n", m_NumCopyEngines, maxAvailableCEs);
        rc = RC::BAD_PARAMETER;
    }
    else if (m_NumBlitLoops < m_NumCopyEngines)
    {
        // Each CE will be allocated one or more BlitLoops to process, therefore
        // if there are less Blit Loops than CEs, we cap the number of CEs used
        // so that we don't have CEs with nothing to do
        m_NumCopyEngines = m_NumBlitLoops;
        Printf(Tee::PriWarn,
               "Not enough blit loops to utilize all CEs. Reducing # of CEs to %u\n",
               m_NumCopyEngines);
    }
    Printf(Tee::PriLow, "Using %u copy engine(s)\n", m_NumCopyEngines);

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::AllocAdditionalCopyEngineResources()
{
    RC rc;

    // No need to allocate more resources if we aren't using CE to perform
    // fb->fb copies
    if (m_Method != UseCE)
    {
        return OK;
    }

    // The resizing of resources below should be done such that a reallocation
    // is not required, therefore we'll sanity check that the vectors already
    // have enough memory allocated for the resize. If not, then the constructor
    // did not properly reserve enough memory for these resources
    MASSERT(m_CopyEng.capacity() >= m_NumCopyEngines);

    for (UINT32 s = 0; s < m_BlitLoopStatus.size(); s++)
    {
        m_BlitLoopStatus[s].resize(m_NumCopyEngines);
    }

    m_pCh.resize(m_NumCopyEngines);
    m_hCh.resize(m_NumCopyEngines);
    m_pChSubShift.resize(m_NumCopyEngines);
    m_CopyEng.resize(m_NumCopyEngines);
    m_Notifier.resize(m_NumCopyEngines);

    // We've already allocated 1 channel, therefore only allocate new channels
    // starting at index 1.
    for (UINT32 i = 1; i < m_NumCopyEngines; i++)
    {
        CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh[i],
                                                          &m_hCh[i],
                                                          &m_CopyEng[i],
                                                          m_EnabledCopyEngine[i]));

        // Unlike most tests, NewWfMats does not want auto-flush behavior.
        m_pCh[i]->SetAutoFlush(false, 0);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SetupGraphics()
{
    RC rc;

    if (m_AuxMethod == UseCE)
    {
        CHECK_RC(m_CompareBuf.BindGpuChannel(m_hAuxCh));
        CHECK_RC(m_pAuxCh->SetObject(s_CopyEng, m_AuxCopyEng.GetHandle()));

        m_AuxSemaphore.SetPayload(m_AuxPayload);

        const UINT64 semapaOffset64 = m_AuxSemaphore.GetCtxDmaOffsetGpu(0);

        if (m_AuxCopyEng.GetClass() == GF100_DMA_COPY)
        {
            CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_SET_APPLICATION_ID,
                DRF_DEF(90B5, _SET_APPLICATION_ID, _ID, _NORMAL)));
        }

        CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_SET_SEMAPHORE_A, 2,
            /* LW90B5_SET_SEMAPHORE_A */
            static_cast<UINT32>(semapaOffset64 >> 32),
            /* LW90B5_SET_SEMAPHORE_B */
            static_cast<UINT32>(semapaOffset64)));

        CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_PITCH_IN,
            c_BoxWidthBytes));
        CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_PITCH_OUT,
            c_BoxWidthBytes));
        CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_LINE_LENGTH_IN,
            c_BoxWidthBytes));
        CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_LINE_COUNT,
            m_BoxHeight));
    }
    else if (m_AuxMethod == UseVIC)
    {
        // m_CompareBuf is already bound to the main channel
        m_AuxSemaphore.SetPayload(m_AuxPayload);
    }

    if (m_Method == UseCE)
    {
        for (UINT32 i = 0; i < m_NumCopyEngines; i++)
        {
            CHECK_RC(SetupCopyEngine(i));
        }
    }
    else if (m_Method == UseVIC)
    {
        CHECK_RC(SetupVIC());
    }
    else if (m_Method == Use2D)
    {
        CHECK_RC(SetupGraphicsTwoD());
    }
    else
    {
        MASSERT(!"Unimplemented method!");
    }

    for (UINT32 i = 0; i < m_NumBlitEngines; i++)
    {
        CHECK_RC(m_pCh[i]->Flush());
    }

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SetupCopyEngine(UINT32 idx)
{
    VerbosePrintf("%s\n", __FUNCTION__);
    RC rc;

    m_pCh[idx]->SetObject(s_CopyEng, m_CopyEng[idx].GetHandle());

    const UINT64 notifOffset64 = m_Notifier[idx].GetCtxDmaOffsetGpu(0);

    if (m_CopyEng[idx].GetClass() == GF100_DMA_COPY)
    {
        CHECK_RC(m_pCh[idx]->Write(s_CopyEng, LW90B5_SET_APPLICATION_ID,
                    DRF_DEF(90B5, _SET_APPLICATION_ID, _ID, _NORMAL)));
    }

    CHECK_RC(m_pCh[idx]->Write(s_CopyEng, LWC0B5_SET_SEMAPHORE_A, 2,
                /* CLC0B5_SET_SEMAPHORE_A */
                UINT32(notifOffset64 >> 32),
                /* CLC0B5_SET_SEMAPHORE_B */
                UINT32(notifOffset64)));

    if (m_UseBlockLinear)
    {
        CHECK_RC(m_pCh[idx]->Write(s_CopyEng, LWC0B5_SET_DST_BLOCK_SIZE, 6,
                /* CLC0B5_SET_DST_BLOCK_SIZE */
                DRF_DEF(C0B5, _SET_DST_BLOCK_SIZE, _WIDTH, _ONE_GOB) |
                DRF_DEF(C0B5, _SET_DST_BLOCK_SIZE, _HEIGHT, _ONE_GOB) |
                DRF_DEF(C0B5, _SET_DST_BLOCK_SIZE, _DEPTH, _ONE_GOB) |
                DRF_DEF(C0B5, _SET_DST_BLOCK_SIZE, _GOB_HEIGHT,
                        _GOB_HEIGHT_FERMI_8),
                /* CLC0B5_SET_DST_WIDTH */
                c_TwoDTransferWidth * 4,
                /* CLC0B5_SET_DST_HEIGHT */
                c_TwoDTransferHeightMult*m_BoxHeight,
                /* CLC0B5_SET_DST_DEPTH */
                1,
                /* LWC0B5_SET_DST_LAYER */
                0,
                /* LWC0B5_SET_DST_ORIGIN */
                0));
        CHECK_RC(m_pCh[idx]->Write(s_CopyEng, LWC0B5_SET_SRC_BLOCK_SIZE, 6,
                /* LWC0B5_SET_SRC_BLOCK_SIZE */
                DRF_DEF(C0B5, _SET_SRC_BLOCK_SIZE, _WIDTH, _ONE_GOB) |
                DRF_DEF(C0B5, _SET_SRC_BLOCK_SIZE, _HEIGHT, _ONE_GOB) |
                DRF_DEF(C0B5, _SET_SRC_BLOCK_SIZE, _DEPTH, _ONE_GOB) |
                DRF_DEF(C0B5, _SET_SRC_BLOCK_SIZE, _GOB_HEIGHT,
                        _GOB_HEIGHT_FERMI_8),
                /* LWC0B5_SET_SRC_WIDTH */
                c_TwoDTransferWidth * 4,
                /* LWC0B5_SET_SRC_HEIGHT */
                c_TwoDTransferHeightMult*m_BoxHeight,
                /* LWC0B5_SET_SRC_DEPTH */
                1,
                /* LWC0B5_SET_SRC_LAYER */
                0,
                /* LWC0B5_SET_SRC_ORIGIN */
                0));
    }

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SetupGraphicsTwoD()
{
    VerbosePrintf("%s\n", __FUNCTION__);
    RC rc;

    m_pCh[0]->SetObject(s_TwoD, m_TwoD.GetHandle());

    if (m_UseBlockLinear)
    {
        m_pCh[0]->Write(s_TwoD,
                     LW502D_SET_SRC_MEMORY_LAYOUT,
                     LW502D_SET_SRC_MEMORY_LAYOUT_V_BLOCKLINEAR);
        m_pCh[0]->Write(s_TwoD,
                     LW502D_SET_DST_MEMORY_LAYOUT,
                     LW502D_SET_DST_MEMORY_LAYOUT_V_BLOCKLINEAR);
    }
    else
    {
        m_pCh[0]->Write(s_TwoD,
                     LW502D_SET_SRC_MEMORY_LAYOUT,
                     LW502D_SET_SRC_MEMORY_LAYOUT_V_PITCH);
        m_pCh[0]->Write(s_TwoD,
                     LW502D_SET_DST_MEMORY_LAYOUT,
                     LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH);
    }

    m_pCh[0]->Write(s_TwoD,
                 LW502D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT,
                 c_TwoDTransferHeightMult*m_BoxHeight);

    if (m_UseNarrowBlits)
    {
        // Narrow mode, i.e. transfer the box using N 1-pixel-wide blits.
        // Lower bandwidth, but exercises the FBIO byte-enable lines better.

        m_pCh[0]->Write(s_TwoD,
                 LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH,
                 1);
        m_BlitsPerBoxCopy = c_TwoDTransferWidth;
    }
    else if (m_LegacyUseNarrow)
    {
        // Legacy semi-broken narrow mode, bug 452249.
        //
        // Here we blit a 32-pixel-wide box 1024 times at incrementing X offsets
        // in source and dest.  After the first blit, successive copies partly
        // overwrite already-copied memory, and are more and more clipped at the
        // right as they overlap the surface width.
        // After the first 31 blits, the others are all offscreen to the right
        // and are effectively no-ops.
        //
        // Note that c_BoxWidthPixels is 1024, the display surface width, but
        // we're programming the LW50_TWOD class with a 32-pixel width.
        // This keeps the box boundaries from overlapping in linear memory space
        // and allows simpler address decoding on the block-linear surface.
        //
        // We retain this broken version in parallel with the fix because many
        // board quals have been done with the old version.

        m_pCh[0]->Write(s_TwoD,
                 LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH,
                 c_TwoDTransferWidth);
        m_BlitsPerBoxCopy = c_BoxWidthPixels;
    }
    else
    {
        // Normal mode -- one full-size blit per box copy.

        m_pCh[0]->Write(s_TwoD,
                 LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH,
                 c_TwoDTransferWidth);
        m_BlitsPerBoxCopy = 1;
    }
    m_pCh[0]->Write(s_TwoD, LW502D_SET_SRC_WIDTH, c_TwoDTransferWidth);
    m_pCh[0]->Write(s_TwoD, LW502D_SET_DST_WIDTH, c_TwoDTransferWidth);
    m_pCh[0]->Write(s_TwoD, LW502D_SET_SRC_HEIGHT,
                 c_TwoDTransferHeightMult*m_BoxHeight);
    m_pCh[0]->Write(s_TwoD, LW502D_SET_DST_HEIGHT,
                 c_TwoDTransferHeightMult*m_BoxHeight);

    // See "Ternary Raster Operations" in online MSoft documentation.
    const UINT32 ropIlwertPattern = 0x5A;  // xor pattern across memory

    m_pCh[0]->Write(s_TwoD, LW502D_SET_ROP,        ropIlwertPattern);

    if (m_UseVariableBlits)
    {
        m_pCh[0]->Write(s_TwoD, LW502D_SET_SRC_FORMAT, LW502D_SET_DST_FORMAT_V_Y8);
        m_pCh[0]->Write(s_TwoD, LW502D_SET_DST_FORMAT, LW502D_SET_DST_FORMAT_V_Y8);
    }
    else
    {
        m_pCh[0]->Write(s_TwoD, LW502D_SET_SRC_FORMAT,
                     LW502D_SET_DST_FORMAT_V_A8R8G8B8);
        m_pCh[0]->Write(s_TwoD, LW502D_SET_DST_FORMAT,
                     LW502D_SET_DST_FORMAT_V_A8R8G8B8);
    }

    m_pCh[0]->Write(s_TwoD,
                 LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT,
                 LW502D_SET_MONOCHROME_PATTERN_COLOR_FORMAT_V_Y32);
    m_pCh[0]->Write(s_TwoD,
                 LW502D_SET_MONOCHROME_PATTERN_FORMAT,
                 LW502D_SET_MONOCHROME_PATTERN_FORMAT_V_CGA6_M1);
    m_pCh[0]->Write(s_TwoD,
                 LW502D_SET_PATTERN_SELECT,
                 LW502D_SET_PATTERN_SELECT_V_MONOCHROME_8x8);
    m_pCh[0]->Write(s_TwoD, LW502D_SET_MONOCHROME_PATTERN_COLOR0, 4,
        /* LW502D_SET_MONOCHROME_PATTERN_COLOR0 */ 0xFFFFFFFF,
        /* LW502D_SET_MONOCHROME_PATTERN_COLOR1 */ 0xFFFFFFFF,
        /* LW502D_SET_MONOCHROME_PATTERN0 */       0xFFFFFFFF,
        /* LW502D_SET_MONOCHROME_PATTERN1 */       0xFFFFFFFF);

    CHECK_RC(m_Notifier[0].Instantiate(s_TwoD));

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SetupVIC()
{
    VerbosePrintf("%s\n", __FUNCTION__);
    RC rc;

    const Surface2D::Layout layout         = m_UseBlockLinear
                                             ? Surface2D::BlockLinear
                                             : Surface2D::Pitch;
    const UINT32            logBlockHeight = 4;
    const UINT32            width          = m_UseBlockLinear
                                             ? c_TwoDTransferWidth * 1 // *1 to WAR old compiler bug
                                             : c_BoxWidthPixels * 1;   // *1 to WAR old compiler bug
    const UINT32            height         = m_UseBlockLinear
                                             ? c_TwoDTransferHeightMult * m_BoxHeight
                                             : m_BoxHeight;

    VICDmaWrapper::SurfDesc srcSurf = {};
    srcSurf.width        = width;
    srcSurf.copyWidth    = width;
    srcSurf.copyHeight   = height;
    srcSurf.layout       = layout;
    srcSurf.logBlkHeight = logBlockHeight;
    srcSurf.colorFormat  = ColorUtils::A8R8G8B8;
    srcSurf.mappingType  = m_UseBlockLinear ? Channel::MapBlockLinear : Channel::MapPitch;

    VICDmaWrapper::SurfDesc dstSurf = {};
    dstSurf.width        = width;
    dstSurf.copyWidth    = width;
    dstSurf.copyHeight   = height;
    dstSurf.layout       = layout;
    dstSurf.logBlkHeight = logBlockHeight;
    dstSurf.colorFormat  = ColorUtils::A8R8G8B8;
    dstSurf.mappingType  = m_UseBlockLinear ? Channel::MapBlockLinear : Channel::MapPitch;

    MASSERT(m_pCh.size() == 1);
    CHECK_RC(m_VICDma.Initialize(m_pCh[0], GetBoundGpuDevice(), m_TimeoutMs, srcSurf, dstSurf));

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::CalcBoxHeight()
{
    if (0 == m_BoxHeight)
    {
        // No user override, choose a sane value.
        m_BoxHeight = 64;
    }

    if (GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(Tee::PriLow,
               "WfMats initial box height of %d - updating for SOC\n",
                m_BoxHeight);
        const UINT64 pageSize = Platform::GetPageSize();
        while (m_BoxHeight * c_BoxWidthBytes < pageSize)
        {
            m_BoxHeight *= 2;
        }
        if ((m_BoxHeight * c_BoxWidthBytes) % pageSize != 0)
        {
            Printf(Tee::PriError, "Unable to compute box height "
                    "which is a multiple of page size (%lluB)\n", pageSize);
            return RC::SOFTWARE_ERROR;
        }
    }

    VerbosePrintf("WfMats box height of %d\n", m_BoxHeight);

    m_BoxPixels         = c_BoxWidthPixels * m_BoxHeight;
    m_BoxBytes          = c_BoxWidthBytes * m_BoxHeight;
    m_BoxesPerMegabyte  = FLOAT64(1 << 20) / FLOAT64(m_BoxBytes);

    return OK;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::HandleError
(
    UINT32 offset,
    UINT32 expected,
    UINT32 actual,
    UINT32 patternOffset,
    const string &patternName
)
{
    Tasker::AttachThread attach;
    RC rc;

    if (GetNumRechecks())
        m_RetryHasErrors = true;

    if (m_LogError)
    {
        MemError * pErrObj = &GetMemError(m_LwrrentReadbackSubdev);

        MASSERT(GetChunks()->size() >= 1);

        // Assuming Kinds and Strides are the same in all the chunks.
        // We can't look in the proper chunk because Offset is not the
        // exact value

        GpuUtility::MemoryChunks::iterator chunk = GetChunks()->begin();
        const UINT08 partChanDecode =
            m_PartChanDecodesByBlitLoop[m_LwrrentBlitLoop][offset/m_DecodeSpan];

        if ((m_MaxBlitLoopErrors == 0) ||
            (m_ErrCountByPartChan[partChanDecode] < m_MaxBlitLoopErrors) ||
            (m_ErrMaskByPartChan[partChanDecode] != 0xFFFFFFFF))
        {
            if (patternOffset != 0xffffffff)
            {
                CHECK_RC(pErrObj->LogOffsetError(
                                32,
                                offset + m_ExtraOffsetForHandleError,
                                actual,
                                expected,
                                chunk->pteKind,
                                chunk->pageSizeKB,
                                patternName,
                                patternOffset));
            }
            else
            {
                CHECK_RC(pErrObj->LogOffsetError(
                                32,
                                offset + m_ExtraOffsetForHandleError,
                                actual,
                                expected,
                                chunk->pteKind,
                                chunk->pageSizeKB,
                                "",
                                0));
            }
        }

        m_ErrCountByPartChan[partChanDecode]++;
        m_ErrMaskByPartChan[partChanDecode] |= (expected ^ actual);
    }
    return OK;
}

//------------------------------------------------------------------------------
/* static */
RC NewWfMatsTest::HandleError
(
    void * pvThis,
    UINT32 offset,
    UINT32 expected,
    UINT32 actual,
    UINT32 patternOffset,
    const string &patternName
)
{
    RC rc;
    NewWfMatsTest *pThis = (NewWfMatsTest *)pvThis;
    CHECK_RC(pThis->HandleError(offset, expected, actual,
                                patternOffset, patternName));
    return OK;
}

//------------------------------------------------------------------------------
// Builds the table that contains box information(ctxdma handle, offset etc.)
// and sets up the arrays that are going to be used by the WfMats algorithm.
//------------------------------------------------------------------------------
RC NewWfMatsTest::BuildBoxTable()
{
    FrameBuffer * pFb = GetBoundGpuSubdevice()->GetFB();

    // Callwlate how many Boxes we will test.
    GpuUtility::MemoryChunks::iterator iChunk;

    m_NumBoxes = 0;

    for (iChunk= GetChunks()->begin(); iChunk != GetChunks()->end(); ++iChunk)
    {
        // FB byte offsets:
        UINT64 start = iChunk->fbOffset;
        UINT64 end   = iChunk->size + start;

        if (start < (UINT64(GetStartLocation()) << 20))
            start = (UINT64(GetStartLocation()) << 20);

        if (start % m_BoxBytes)
            start = (start / m_BoxBytes + 1) * m_BoxBytes;

        if (end > (UINT64(GetEndLocation()) << 20))
            end = (UINT64(GetEndLocation()) << 20);

        if (end > start)
            m_NumBoxes += UINT32((end - start) / m_BoxBytes);
    }

    m_BoxTable.resize(m_NumBoxes);

    // nwfmats requires that all boxes within the same blit-loop have the
    // exact same partition and channel address decode at each offset.
    //
    // For Tesla and earlier parts, we could choose a box size such that
    // every box had the same part/chan swizzle, and we used just one blit-loop.
    //
    // In Tesla2 parts with 2 independently-sequenced 32-bit channels per
    // 64-bit partition, we had to use two blit-loops.
    //
    // Starting with gf104, we can no longer find any box-size that allows
    // just 2 blit-loops.  It looks like the general rule will be one blit
    // loop per partition/channel, i.e. 8 for a 4-partition gf104.
    //
    // If any of the blit-loops end up empty we'll reduce m_BlitLoops later.

    const UINT32 burstBytes  = pFb->GetBurstSize();
    const UINT32 numChannels = pFb->GetSubpartitions();
    const UINT32 numPseudoChannels = pFb->GetPseudoChannelsPerSubpartition();

    const UINT32 partChanCombos = pFb->GetFbioCount() * numChannels * numPseudoChannels;

    map<UINT32, vector<UINT08> > verifSwizzle;
    typedef map<UINT32, UINT32> BoxCountsCont;
    BoxCountsCont boxCounts;

    UINT32 iBox = 0;
    for (iChunk= GetChunks()->begin(); iChunk != GetChunks()->end(); ++iChunk)
    {
        // for each chunk mark the appropriate boxes as testable

        // FB byte offsets:
        UINT64 start = iChunk->fbOffset;
        UINT64 end   = iChunk->size + start;

        if (start < (UINT64(GetStartLocation()) << 20))
            start = (UINT64(GetStartLocation()) << 20);

        if (start % m_BoxBytes)
            start = (start / m_BoxBytes + 1) * m_BoxBytes;

        if (end > (UINT64(GetEndLocation()) << 20))
            end = (UINT64(GetEndLocation()) << 20);

        for (UINT64 offset = start;
             offset + m_BoxBytes <= end; offset += m_BoxBytes)
        {
            // On Fermi and Kepler address decode wraps around at 4GB.
            // We need to discard any box which span 4GB boundary,
            // because such box contains mixed address decode and
            // does not match any boxes, so the test won't be able
            // to copy it to another box.
            if ((offset < 4_GB) && (offset+m_BoxBytes > 4_GB))
            {
                Printf(Tee::PriLow, "Discarding box %u from 0x%llx to 0x%llx "
                        "because it spans 4GB boundary\n",
                        iBox, offset, offset+m_BoxBytes);
                m_NumBoxes--;
                continue;
            }

            m_BoxTable[iBox].FbOffset        = offset;
            if (m_Method == UseVIC)
            {
                // For VIC use physical memory handle and offset within
                // that memory allocation.
                m_BoxTable[iBox].CtxDmaHandle = iChunk->hMem;
                m_BoxTable[iBox].CtxDmaOffset = offset - iChunk->fbOffset;
            }
            else
            {
                m_BoxTable[iBox].CtxDmaHandle = iChunk->hCtxDma;
                m_BoxTable[iBox].CtxDmaOffset =
                    offset - iChunk->fbOffset + iChunk->ctxDmaOffsetGpu;
            }
            UINT32 partChan = 0;

            if (partChanCombos > 1)
            {
                partChan = CalcPartChanHash(offset);

                // Verify if channel swizzling is the same for all
                // boxes in the same class We need check only the
                // beginning of each N-dword burst, by assuming that
                // partition/channel cannot change during a burst
                // read/write transaction.
                if (m_VerifyChannelSwizzle)
                {
                    // Get full decode for the current box
                    if (iBox < 100)
                        Printf(Tee::PriNormal,
                               "VerifyChannelSwizzle box %d PartChan %d\n",
                               iBox, partChan);
                    UINT32 chunkSize = 0;
                    vector<UINT08> partChans;
                    partChans.reserve(m_BoxBytes / burstBytes);
                    RC rc;
                    CHECK_RC(GpuUtility::DecodeRangePartChan(
                            GetBoundGpuSubdevice(),
                            *iChunk,
                            offset,
                            offset+m_BoxBytes,
                            &partChans,
                            &chunkSize));

                    // Compare against old chunk layout
                    if (verifSwizzle[partChan].empty())
                    {
                        verifSwizzle[partChan].swap(partChans);
                    }
                    else
                    {
                        if (verifSwizzle[partChan] != partChans)
                        {
                            Printf(Tee::PriError,
                                "Error: Unexpected partition/channel/pseudochannel swizzling at offset 0x%llx\n",
                                offset);
                            //MASSERT(!"Unexpected channel swizzling!");
                            return RC::SOFTWARE_ERROR;
                        }
                    }
                }
            }

            // Save box's first partition:channel:pseudochannel
            m_BoxTable[iBox].StartingPartChan = partChan;
            BoxCountsCont::iterator boxCountIt = boxCounts.find(partChan);
            if (boxCountIt == boxCounts.end())
            {
                boxCounts[partChan] = 0;
            }
            boxCounts[partChan]++;

            iBox++;
        }
    }
    verifSwizzle.clear();
    MASSERT(iBox == m_NumBoxes);

    // Now that we have the box-counts by starting partition:channel:
    //  - Drop back to as few blit-loops as we can get away with.
    //  - Create an index of starting partition:channel to blit-loop.
    //  - Find the smallest non-zero box-count.

    m_NumBoxes = 0;
    m_NumBlitLoops = 0;
    UINT32 maxBlitLoopBoxes = _UINT32_MAX;
    map<UINT32, INT32> startingPartChanToBlitLoop;

    for (BoxCountsCont::const_iterator boxCountIt = boxCounts.begin();
         boxCountIt != boxCounts.end();
         ++boxCountIt)
    {
        const string userFriendlyDecode =
            GetUserFriendlyPartChanHash(boxCountIt->first);
        Printf(Tee::PriLow,
                "NewWfMats: Decode %s (0x%08x) has %u boxes\n",
                userFriendlyDecode.c_str(),
                boxCountIt->first,
                boxCountIt->second);

        m_NumBoxes += boxCountIt->second;

        // Blit-loops require at least two boxes with the same address decoding
        if (boxCountIt->second >= 2)
        {
            startingPartChanToBlitLoop[boxCountIt->first] = m_NumBlitLoops;
            m_NumBlitLoops++;
            if (maxBlitLoopBoxes > boxCountIt->second)
            {
                maxBlitLoopBoxes = boxCountIt->second;
            }
        }
    }
    VerbosePrintf("NewWfMats: NumBlitLoops is %d\n", m_NumBlitLoops);

    if (_UINT32_MAX == maxBlitLoopBoxes)
    {
        Printf(Tee::PriError,
            "Found no two boxes with matching partition/channel decoding.\n");
        if (GetMaxFbMb())
        {
            Printf(Tee::PriError,
                "Testarg 'MaxFbMb' is likely too small for this test configuration, "
                "consider increasing it.\n");
        }
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT32 minBoxes = 2 * (1 + m_NumBlitLoops);
    if (m_NumBoxes < minBoxes)
    {
        // Need 2 for CPU and 2 for each blit-loop.
        Printf(Tee::PriError, "Need at least %d boxes.\n", minBoxes);
        return RC::SOFTWARE_ERROR;
    }

    // m_CpuNumBoxes is the number of boxes NOT in the blit loop; i.e., those
    // boxes tested using ordinary PCI read and write cycles.
    // This is the biggest factor in the test time for the card.

    m_CpuNumBoxes  = m_CpuTestBytes / m_BoxBytes;
    if (m_DoPcieTraffic && m_CpuNumBoxes < 1)
    {
        m_CpuNumBoxes = 1;
    }
    if (m_CpuNumBoxes > (m_NumBoxes - 2*m_NumBlitLoops))
    {
        // Leave at least 2 for each blit loop.
        m_CpuNumBoxes = m_NumBoxes - 2*m_NumBlitLoops;
    }
    if ((m_CpuNumBoxes == 0) && m_DoPcieTraffic)
    {
        Printf(Tee::PriError, "Need one cpu box for DoPcieTraffic.\n");
        return RC::SOFTWARE_ERROR;
    }

    // Each of the blit loops needs to hold 1 + N * m_PatternCount boxes.
    // The 1+ is for the "spare" box used to hold a temp copy of one pattern
    // during each rotation.

    m_BlitNumBoxes = min(maxBlitLoopBoxes,
                         (m_NumBoxes - m_CpuNumBoxes) / m_NumBlitLoops);

    if (m_BlitNumBoxes < 2)
    {
        Printf(Tee::PriError, "Need two blit boxes.\n");
        return RC::SOFTWARE_ERROR;
    }
    m_BlitNumBoxes -= 1; // spare box

    if (m_PatternCount == 0)
    {
        Printf(Tee::PriNormal,
               "NewWfMats: There are zero patterns selected!\n");
        return RC::SOFTWARE_ERROR;
    }

    // Force m_BlitNumBoxes to be an exact multiple of m_PatternCount, the +1
    // is not counted in m_BlitNumBoxes.
    if (m_BlitNumBoxes < m_PatternCount)
    {
        m_PatternCount = m_BlitNumBoxes;
    }
    m_BlitNumBoxes -= (m_BlitNumBoxes % m_PatternCount);

    VerbosePrintf(
            "PatternCount=%d, BlitNumBoxes=%d, %.1f reps.\n",
            m_PatternCount,
            m_BlitNumBoxes,
            m_BlitNumBoxes/double(m_PatternCount));

    // Give the Cpu loop as many boxes as the user requested (round up),
    // possibly leaving some boxes unused.

    m_CpuNumBoxes = (m_CpuTestBytes + m_BoxBytes-1) / m_BoxBytes;

    if (m_DoPcieTraffic && m_CpuNumBoxes < 1)
    {
        m_CpuNumBoxes = 1;
    }
    m_CpuNumBoxes = min(m_CpuNumBoxes,
            (m_NumBoxes - m_NumBlitLoops*(m_BlitNumBoxes+1)));

    const UINT32 unusedNumBoxes =
        m_NumBoxes - (m_CpuNumBoxes + m_NumBlitLoops*(m_BlitNumBoxes+1));

    MASSERT(m_NumBoxes >= m_CpuNumBoxes + m_NumBlitLoops*(m_BlitNumBoxes+1));

    // Alloc arrays of box indices.

    m_CpuBoxIndices.clear();
    m_CpuBoxIndices.reserve(m_CpuNumBoxes);

    m_ErrCountByPartChan.clear();
    m_ErrCountByPartChan.resize(partChanCombos, 0);
    m_ErrMaskByPartChan.clear();
    m_ErrMaskByPartChan.resize(partChanCombos, 0);
    m_PartChanDecodesByBlitLoop.resize(m_NumBlitLoops);

    m_BlitBoxIndices.resize(m_NumBlitLoops);
    for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
    {
        m_BlitBoxIndices[loop].clear();
        m_BlitBoxIndices[loop].reserve(m_BlitNumBoxes);

        m_PartChanDecodesByBlitLoop[loop].clear();
        m_PartChanDecodesByBlitLoop[loop].reserve(m_BoxBytes / burstBytes);
    }

    // Build a temp array of shuffled indices to distribute.
    vector<UINT32> tmpBoxIndices(m_NumBoxes);
    for (UINT32 i = 0; i < tmpBoxIndices.size(); i++)
        tmpBoxIndices[i] = i;

    m_pRandom->SeedRandom( m_pTestConfig->Seed() );
    m_pRandom->Shuffle(static_cast<UINT32>(tmpBoxIndices.size()), &tmpBoxIndices[0]);

    // Deal out the shuffled box-indexes into each blit-loop until
    // each is full, then to the spares, then put the remainders into cpu.

    vector<bool> foundSpares(m_NumBlitLoops, false);
    m_SpareBoxIndices.resize(m_NumBlitLoops);

    for (UINT32 ii = 0; ii < tmpBoxIndices.size(); ii++)
    {
        const UINT32 boxIdx   = tmpBoxIndices[ii];
        const UINT32 partChan = m_BoxTable[boxIdx].StartingPartChan;

        // If a box has a unique decoding it can only be used as a CPU box
        if (boxCounts[partChan] == 1)
        {
            if (m_CpuBoxIndices.size() < m_CpuNumBoxes)
            {
                m_CpuBoxIndices.push_back(boxIdx);
            }
        }
        // Otherwise it can be used as a CPU box, a blit-loop box, or a blit-loop spare
        else if (boxCounts[partChan] >= 2)
        {
            const INT32 loop = startingPartChanToBlitLoop[partChan];
            if (m_BlitBoxIndices[loop].size() < m_BlitNumBoxes)
            {
                // Put this box on the blit-loop for boxes that start with
                // an address that goes to this channel.
                m_BlitBoxIndices[loop].push_back(boxIdx);
            }
            else if (!foundSpares[loop])
            {
                // The spare box must be of the right channel.
                m_SpareBoxIndices[loop] = boxIdx;
                foundSpares[loop] = true;
            }
            else if (m_CpuBoxIndices.size() < m_CpuNumBoxes)
            {
                m_CpuBoxIndices.push_back(boxIdx);
            }
            // else leave this box untested.
        }
    }

    MASSERT(m_CpuBoxIndices.size() == m_CpuNumBoxes);
    for (UINT32 ii = 0; ii < m_NumBlitLoops; ++ii)
    {
        MASSERT(m_BlitBoxIndices[ii].size() == m_BlitNumBoxes);
        MASSERT(foundSpares[ii]);
        RC rc;
        iChunk = GetChunks()->begin();
        CHECK_RC(GpuUtility::DecodeRangePartChan(
                GetBoundGpuSubdevice(),
                *iChunk,
                m_BoxTable[m_BlitBoxIndices[ii][0]].FbOffset,
                m_BoxTable[m_BlitBoxIndices[ii][0]].FbOffset + m_BoxBytes,
               &m_PartChanDecodesByBlitLoop[ii],
               &m_DecodeSpan));
    }

    // Seed the random generator for selecting random blit widths
    if (m_UseVariableBlits)
    {
        m_pRandom->SeedRandom(m_VariableBlitSeed);
    }

    VerbosePrintf(
            "Total Boxes = %d, blit=%d %0.1fMB cpu=%d %0.1fMB (%0.1f%%) unused=%d\n",
            m_NumBoxes,
            m_NumBlitLoops * (m_BlitNumBoxes + 1),
            static_cast<float>(m_NumBlitLoops *
                               (m_BlitNumBoxes + 1)) * m_BoxBytes / 1_MB,
            m_CpuNumBoxes,
            (m_CpuNumBoxes * m_BoxBytes)/1e6,
            100.0*m_CpuNumBoxes / m_NumBoxes,
            unusedNumBoxes);

    return OK;
}

//! Callwlates unique identifier for partition/channel layout for a box starting
//! at the given physical address.
UINT32 NewWfMatsTest::CalcPartChanHash(UINT64 addr)
{
    const GpuUtility::MemoryChunkDesc& chunk0 = *GetChunks()->begin();

    // Callwlate chunk size
    if (m_DecodeSpan == 0)
    {
        const UINT64 physAddr0 = chunk0.fbOffset;
        vector<UINT08> PartChans;
        if (OK != GpuUtility::DecodeRangePartChan(
                        GetBoundGpuSubdevice(),
                        chunk0,
                        physAddr0,
                        physAddr0+m_BoxBytes,
                        &PartChans,
                        &m_DecodeSpan))
        {
            Printf(Tee::PriError, "Unable to decode partition/channel layout "
                    "for chunk starting at 0x%llx!\n", physAddr0);
            MASSERT(0);
            return 0;
        }
    }

    // Get framebuffer properties
    FrameBuffer* const pFb = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numFbios    = pFb->GetFbioCount();
    const UINT32 numChannels = pFb->GetSubpartitions();
    const UINT32 numPseudoChannels = pFb->GetPseudoChannelsPerSubpartition();

    // Collect partition/channel at:
    //   addr, addr+m_ChunkSize, addr+m_ChunkSize*2, addr+m_ChunkSize*4, ...
    // Partitions and channels at these addresses are folded into an integer.
    // Note that partition/channel stays the same within each chunk.
    UINT32 bin = 0;
    const UINT32 maxSamplePoints =
        static_cast<UINT32>(floor(log(double(bin-1)) /
                            log(double(numFbios * numChannels * numPseudoChannels))));
    for (UINT32 i = 0; i < maxSamplePoints; i++)
    {
        const UINT32 offset = (i == 0) ? 0 : (1 << (i-1))*m_DecodeSpan;
        FbDecode decode;
        if (offset >= m_BoxBytes)
        {
            break;
        }
        pFb->DecodeAddress(&decode, addr+offset,
                           chunk0.pteKind, chunk0.pageSizeKB);

        bin *= numFbios * numChannels * numPseudoChannels;
        bin += decode.virtualFbio * numChannels * numPseudoChannels;
        bin += decode.subpartition * numPseudoChannels;
        bin += decode.pseudoChannel;
    }
    return bin;
}

string NewWfMatsTest::GetUserFriendlyPartChanHash(UINT32 hash)
{
    FrameBuffer* const pFb = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numFbios    = pFb->GetFbioCount();
    const UINT32 numChannels = pFb->GetSubpartitions();
    const UINT32 numPseudoChannels = pFb->GetPseudoChannelsPerSubpartition();

    string partChanStr;
    const int maxSamplePoints =
        static_cast<UINT32>(floor(log(double((std::numeric_limits<UINT32>::max)())) /
                                  log(double(numFbios * numChannels * numPseudoChannels))));
    for (int i=0; i < maxSamplePoints; i++)
    {
        const UINT32 pseudoChannel = hash % numPseudoChannels;
        hash /= numPseudoChannels;
        const UINT32 chan = hash % numChannels;
        hash /= numChannels;
        const UINT32 virtualFbio = hash % numFbios;
        hash /= numFbios;
        if (!partChanStr.empty())
        {
            partChanStr = "-" + partChanStr;
        }
        const char letter = pFb->VirtualFbioToLetter(virtualFbio);
        partChanStr = Utility::StrPrintf("%c%u%u", letter, chan, pseudoChannel) + partChanStr;
    }

    return partChanStr;
}

//------------------------------------------------------------------------------
// main loop...does the test algorithm
//------------------------------------------------------------------------------
RC NewWfMatsTest::DoWfMats(UINT32 iteration)
{
    RC rc = OK;
    UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();

    // Write the initial patterns to the FB memory that will be used by
    // the blit loop.
    CHECK_RC(FillBlitLoopBoxes());

    InitBlitLoopStatus();
    CHECK_RC(KeepGpuBusy());

    if (m_ExitEarly)
    {
        Printf(Tee::PriNormal, "Exiting WfMats Early\n");
        return OK;
    }

    CHECK_RC( DoPlainOldMats(iteration) );
    CHECK_RC( WaitForBlitLoopDone(false /*doPcieTraffic*/, true) );

    for (UINT32 s = 0; s < numSubdevices; s++)
    {
        CHECK_RC( CheckBlitBoxes(s) );
    }

    return rc;
}

//------------------------------------------------------------------------------
void NewWfMatsTest::ReportBandwidth(Tee::Priority pri)
{
    UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();

    for (UINT32 s = 0; s < numSubdevices; s++)
    {
        double blitBytes = 0.0;
        double tics = 0.0;

        UINT32 numBlitLoopsPerEngine = (UINT32)(m_NumBlitLoops / m_NumBlitEngines);
        UINT32 remBlitLoops = m_NumBlitLoops % m_NumBlitEngines;
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            if (m_BlitLoopStatus[s][i].NumCompleted <= m_BlitLoopStatus[s][i].NumSent)
            {
                // Each channel-subroutine reads & writes all the blit-loop boxes.
                const UINT32 numBlits =
                    (numBlitLoopsPerEngine + ((i < remBlitLoops) ? 1 : 0)) *
                    (m_BlitNumBoxes + 1);

                blitBytes += m_BlitLoopStatus[s][i].NumCompleted * numBlits *
                        double(m_BoxBytes);
            }

            if (s == GetBoundGpuSubdevice()->GetSubdeviceInst())
            {
                // The PlainOldMats routine reads & writes all the cpu-loop
                // boxes once per cpu-pattern, but possibly with multiple
                // "inner loops".
                // Only one gpu/subdevice of an SLI device gets the CPU operations.

                // Not reporting this right now, leaving this in for documentation.
                //cpuBytes = double(m_InnerLoops) *
                //        double(m_CpuPatternLength) *
                //        double(m_CpuNumBoxes) *
                //        double(m_BoxBytes);
            }

            if (m_BlitLoopStatus[s][i].TimeDone != m_TimeBlitLoopStarted)
            {
                tics = max(tics, double(INT64(m_BlitLoopStatus[s][i].TimeDone -
                                           m_TimeBlitLoopStarted)));
            }
        }

        // Times 2 for read+write.
        GpuTest::RecordBps(blitBytes*2.0, tics / Xp::QueryPerformanceFrequency(), pri);
    }
}

//------------------------------------------------------------------------------
void NewWfMatsTest::FillChanSubCopyEngine()
{
    Printf(Tee::PriDebug, "%s\n", __FUNCTION__);

    FillChanSubCommon(&NewWfMatsTest::BoxCopyEngine);
}

//------------------------------------------------------------------------------
void NewWfMatsTest::PrepareVICBoxes()
{
    Printf(Tee::PriDebug, "%s\n", __FUNCTION__);

    m_VICCopies.clear();
    m_VICCopies.reserve(m_BlitNumBoxes + 2 * m_NumBlitLoops);
    FillChanSubCommon(&NewWfMatsTest::BoxVIC);
}

//------------------------------------------------------------------------------
void NewWfMatsTest::FillChanSubTwoD()
{
    Printf(Tee::PriDebug, "%s\n", __FUNCTION__);

    if (m_UseXor)
    {
        // XOR each box (read the box, ilwert the data, write the box).
        // Results in the original pattern ilwerted, plus any aclwmulated errs.
        //
        // We must remember to execute the ChannelSubroutine an even number
        // of times so that we don't report false errors.

        SetCtxDmaTwoD(m_pChSubXor);
        SetRopXorTwoD(m_pChSubXor, true/*set up 2D class to ilwert in place*/);

        for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
        {
            for (UINT32 blitIdx = 0; blitIdx < m_BlitNumBoxes; blitIdx++)
            {
                UINT32 boxIdx = m_BlitBoxIndices[loop][blitIdx];
                BoxTwoD(m_pChSubXor, boxIdx, boxIdx);
            }
        }
    }

    SetCtxDmaTwoD(m_pChSubShift[0]);
    SetRopXorTwoD(m_pChSubShift[0], false/*set up 2D class to copy src->dst*/);

    FillChanSubCommon(&NewWfMatsTest::BoxTwoD);
}

//------------------------------------------------------------------------------
void NewWfMatsTest::FillChanSubCommon(tBoxCopyFnp boxCopyFnp)
{
    UINT32 dstBoxIdx;
    UINT32 srcBoxIdx;

    // First, save away the data in box [0] to the spare box.
    //   Box [0] is read, spare is written
    for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
    {
        dstBoxIdx = m_SpareBoxIndices[loop];
        srcBoxIdx = m_BlitBoxIndices[loop][0];
        (this->*boxCopyFnp)(m_pChSubShift[loop % m_NumBlitEngines], srcBoxIdx, dstBoxIdx);
    }

    // Next, move the boxes down one slot.
    //   Boxes [0]..[N-2] are written.
    //   Boxes [1]..[N-1] are read.
    for (UINT32 iBlit = 0; iBlit < m_BlitNumBoxes-1; iBlit++)
    {
        for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
        {
            dstBoxIdx = m_BlitBoxIndices[loop][iBlit];
            srcBoxIdx = m_BlitBoxIndices[loop][iBlit+1];
            (this->*boxCopyFnp)(m_pChSubShift[loop % m_NumBlitEngines], srcBoxIdx, dstBoxIdx);
        }
    }

    // Finally, write the last box with the saved copy of box [0].
    //   Box [N-1] is written.
    for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
    {
        dstBoxIdx = m_BlitBoxIndices[loop][m_BlitNumBoxes-1];
        srcBoxIdx = m_SpareBoxIndices[loop];
        (this->*boxCopyFnp)(m_pChSubShift[loop % m_NumBlitEngines], srcBoxIdx, dstBoxIdx);
    }
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::DoPlainOldMats(UINT32 iteration)
{
    INT32   direction            = 1;
    RC      rc                   = OK;
    UINT32  *pPat                = 0;
    INT32   patIdx               = 0;

    DetachThread detach(m_DoDetach);

    Printf(Tee::PriDebug, "Entering NewWfMatsTest::DoPlainOldMats()\n");

    const UINT32 MAX_PAT_LENGTH = 20;
    UINT32 patternData[MAX_PAT_LENGTH];

    m_PatternClass.FillMemoryWithPattern( patternData,
            MAX_PAT_LENGTH /* dontcare */, 1, MAX_PAT_LENGTH,
            m_pTestConfig->DisplayDepth(), m_CpuPattern, &m_CpuPatternLength);

    if (m_CpuPatternLength > MAX_PAT_LENGTH-1)
        m_CpuPatternLength = MAX_PAT_LENGTH-1;

    patternData[m_CpuPatternLength] = PatternClass::END_PATTERN;

    pPat = patternData;
    if (iteration == 0)
    {
        // That is only correct when m_CpuPatternLength stays the same during outer loops
        // Right now it is like that
        PrintProgressInit(m_InnerLoops * m_OuterLoops * m_CpuNumBoxes * (m_CpuPatternLength - 1),
                          "All loops * number of cpu boxes * pattern length");
    }

    // Do the mats algorithm.
    for (UINT32 i = 0; i < m_InnerLoops; i++)
    {
        // initialize the "mats" portion of the tiles
        for (UINT32 box=0; box < m_CpuNumBoxes; box++)
        {
            if (0 == (box & 0x7))
            {
                CHECK_RC( KeepGpuBusy() );
            }
            FillBox(m_CpuBoxIndices[box], pPat[0], direction);
        }

        patIdx=0;
        UINT32 alreadyProgressed = (iteration * m_InnerLoops + i) * m_CpuNumBoxes * (m_CpuPatternLength - 1);
        while (pPat[++patIdx ] != PatternClass::END_PATTERN)
        {
            // for each box...
            if (direction > 0)
            {
                for (UINT32 box=0; box < m_CpuNumBoxes; box++)
                {
                    if (0 == (box & 0x3))
                    {
                        CHECK_RC( KeepGpuBusy() );
                    }
                    CHECK_RC(CheckFillBox(m_CpuBoxIndices[box], pPat[patIdx-1],
                                          pPat[patIdx], direction));
                    PrintProgressUpdate(alreadyProgressed + box + 1);
                }
            }
            else
            {
                for (INT32 box = m_CpuNumBoxes - 1; box >= 0; box--)
                {
                    if (0 == (box & 0x3))
                    {
                        CHECK_RC( KeepGpuBusy() );
                    }
                    CHECK_RC(CheckFillBox(m_CpuBoxIndices[box], pPat[patIdx-1],
                                          pPat[patIdx], direction));
                    PrintProgressUpdate(alreadyProgressed + m_CpuNumBoxes - box);
                }
            }
            alreadyProgressed += m_CpuNumBoxes;
            direction = -direction;
        }
    }

    Printf(Tee::PriDebug, "Leaving NewWfMatsTest::DoPlainOldMats()\n");
    return rc;
}

//------------------------------------------------------------------------------
void NewWfMatsTest::InitBlitLoopStatus()
{
    for (UINT32 s = 0; s < m_BlitLoopStatus.size(); s++)
    {
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            Surface2D * pSem = & (m_BlitLoopStatus[s][i].SemaphoreBuf);
            MEM_WR32(pSem->GetAddress(), 0);

            m_BlitLoopStatus[s][i].NumCompleted = 0;
            m_BlitLoopStatus[s][i].NumSent = 0;
            m_BlitLoopStatus[s][i].TimeDone = 0;
            m_BlitLoopStatus[s][i].LwrPatShift = 0;
            m_BlitLoopStatus[s][i].LwrXor = 0;

            // Our goal is to stay about 1GB of FB traffic ahead of the GPU.
            // Each subroutine call reads/writes all blit-loop boxes.
            //
            // Make the min target 2 though, so we don't ever let the GPU go idle.
            m_BlitLoopStatus[s][i].TargetAheadOnEntry = max(2.0,
                    m_NumBlitLoops * (m_BlitNumBoxes+1) * m_BoxBytes / m_NumBlitEngines / 1e9);

            m_BlitLoopStatus[s][i].TargetPending =
                    m_BlitLoopStatus[s][i].TargetAheadOnEntry;
        }
    }
    m_NumKeepBusyCalls = 0;
    m_TimeBlitLoopStarted = Xp::QueryPerformanceCounter();
}

//------------------------------------------------------------------------------
//! Send enough gpfifo calls to our ChannelSubroutine to keep the
//! GPU busy running its blit loop for a while longer.
RC NewWfMatsTest::KeepGpuBusy()
{
    UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();

    if ((1 == m_NumKeepBusyCalls) &&
        (Platform::GetSimulationMode() == Platform::Amodel))
    {
        // On amodel, the gpu is "infinitely fast" from mods' perspective,
        // so let's never send again after the first call.
        return OK;
    }

    m_NumKeepBusyCalls++;

    // Queue up enough subroutine calls so that we stay ahead of the gpu.
    // Use a control loop to colwerge on the right number.

    UINT32 numToSend[LW2080_MAX_SUBDEVICES] = {0};
    for (UINT32 sub = 0; sub < numSubdevices; sub++)
    {
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            // Read the current semaphore value to see how many loops this GPU
            // has completed so far.
            Surface2D * pSem = &(m_BlitLoopStatus[sub][i].SemaphoreBuf);
            m_BlitLoopStatus[sub][i].NumCompleted = MEM_RD32(pSem->GetAddress());

            if (m_BlitLoopStatus[sub][i].NumCompleted > m_BlitLoopStatus[sub][i].NumSent)
            {
                Printf(Tee::PriError,
                        "Error reading semaphore on subdev %d: %x"
                        "(expected 0 to %x)\n",
                        sub,
                        m_BlitLoopStatus[sub][i].NumCompleted,
                        m_BlitLoopStatus[sub][i].NumSent);
                return RC::SOFTWARE_ERROR;
            }

            UINT32 numAhead = m_BlitLoopStatus[sub][i].NumSent -
                    m_BlitLoopStatus[sub][i].NumCompleted;

            // In order to *enter* this function next time N calls ahead of
            // the GPU, we need to *exit* this function "more ahead" than that.
            //
            // TargetPending is how much ahead we should *exit* this function at,
            // in order to be N loops ahead next time.
            //
            // Our initial guess is N. i.e. we guess that each subroutine takes
            // a very very long time to complete.
            //
            // Each time we enter this function not far enough ahead, increase
            // our guess for TargetPending.
            // Each time we enter this function too far ahead, decrease our guess
            // for TargetPending.
            //
            // This is a simple PI control loop.

            m_BlitLoopStatus[sub][i].TargetPending +=
                    0.25 * (m_BlitLoopStatus[sub][i].TargetAheadOnEntry - numAhead);

            double numCalls = (m_BlitLoopStatus[sub][i].TargetPending - numAhead);

            if (numCalls > 0.0)
                numToSend[sub] = UINT32(numCalls);

        Printf(Tee::PriDebug,
                "%s %d sub %d done=%d pending=%d tgt=%.1f sending=%d\n",
                 __FUNCTION__,
                 m_NumKeepBusyCalls,
                 sub,
                 m_BlitLoopStatus[sub][i].NumCompleted,
                 numAhead,
                 m_BlitLoopStatus[sub][i].TargetPending,
                 numToSend[sub]);
        }
    }

    RC rc = SendSubroutines(numToSend);
    return rc;
}

//------------------------------------------------------------------------------
//! Queue up N calls to the blit loop in the channel, and flush them.
RC NewWfMatsTest::SendSubroutines(UINT32 * numToSend)
{
    RC rc;
    const UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();

    DetachThread detach(m_DoDetach);

    while (1)
    {
        // Build a mask of which subdevices should process the following
        // blit loop methods.

        UINT32 subdevMask = 0;
        for (UINT32 sub = 0; sub < numSubdevices; sub++)
        {
            if (numToSend[sub])
            {
                subdevMask |= (1<<sub);
                numToSend[sub]--;
            }
        }
        if (0 == subdevMask)
            break;

        // Put the blit-loop methods into the channel for the selected
        // subdevices to read in parallel.
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            CHECK_RC(SendSubroutine(subdevMask, m_pChSubShift[i], i));
        }

        if (m_UseXor)
        {
            // If any subdevs have completed one full rotation of the pattern,
            // send an XOR pass for them so the next rotation uses the ilwerted
            // version of all patterns.

            UINT32 subdevMaskForXor = 0;
            for (UINT32 sub = 0; sub < numSubdevices; sub++)
            {
                if ((subdevMask & (1<<sub)) &&
                    (0 == m_BlitLoopStatus[sub][0].LwrPatShift))
                {
                    subdevMaskForXor |= (1<<sub);
                }
            }
            if (subdevMaskForXor)
                CHECK_RC(SendSubroutine(subdevMaskForXor, m_pChSubXor, 0));
        }

        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            if (1 < numSubdevices)
            {
                CHECK_RC(m_pCh[i]->WriteSetSubdevice(Channel::AllSubdevicesMask));
            }

            CHECK_RC(m_pCh[i]->Flush());
        }

        Tasker::AttachThread attach;
        CHECK_RC(EndLoop());
#ifndef USE_NEW_TASKER
        Tasker::Yield();
#endif
    }

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SubroutineSleep()
{
    return OK;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SendFinalXorsIfNeeded()
{
    RC rc;
    const UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();

    UINT32 subdevMask = 0;
    for (UINT32 sub = 0; sub < numSubdevices; sub++)
    {
        if (m_BlitLoopStatus[sub][0].LwrXor)
        {
            // This subdevice is lwrrently using ilwerted patterns.
            // Send a final xor pass to fix it.
            subdevMask |= (1<<sub);

            MASSERT(m_UseXor); // how could we get here otherwise?
        }
    }
    if (subdevMask)
        CHECK_RC(SendSubroutine(subdevMask, m_pChSubXor, 0));

    CHECK_RC(m_pCh[0]->Flush());

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SendSubroutine
(
    UINT32 subdevMask,
    ChannelSubroutine * pChSub,
    UINT32 idx
)
{
    RC rc;
    UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();

    // Write the actual gpfifo entry.

    if (1 < numSubdevices)
        CHECK_RC(m_pCh[idx]->WriteSetSubdevice(subdevMask));

    if (m_Method == UseVIC)
    {
        CHECK_RC(SendVICCopies());
    }
    else
    {
        CHECK_RC(pChSub->Call(m_pCh[idx]));
    }

    for (UINT32 sub = 0; sub < numSubdevices; sub++)
    {
        if (subdevMask & (1<<sub))
        {
            // Update blit-loop status.

            m_BlitLoopStatus[sub][idx].NumSent++;

            // If NumSent ever wraps around (exceeds 4 billion gpfifo entries),
            // WaitForBlitLoopDone may become confused.
            // GF100 with MaxFbMb 5 would take weeks to hit wraparound.
            MASSERT(m_BlitLoopStatus[sub][idx].NumSent > 0);

            if (m_pChSubShift[idx] == pChSub)
            {
                m_BlitLoopStatus[sub][idx].LwrPatShift++;
                if (m_PatternCount == m_BlitLoopStatus[sub][idx].LwrPatShift)
                {
                    m_BlitLoopStatus[sub][idx].LwrPatShift = 0;
                }
            }
            else
            {
                m_BlitLoopStatus[sub][idx].LwrXor ^= 1;
            }

            // Add a semaphore release method to let us know when
            // this gpu has completed this blit loop.

            if (1 < numSubdevices)
                CHECK_RC(m_pCh[idx]->WriteSetSubdevice(1<<sub));

            if (m_Method == UseVIC)
            {
                Surface2D&   sem     = m_BlitLoopStatus[0][0].SemaphoreBuf;
                const UINT32 payload = m_BlitLoopStatus[sub][idx].NumSent;

                CHECK_RC(m_VICDma.SemaphoreRelease(sem, 0, payload));
            }
            else
            {
                CHECK_RC(m_pCh[idx]->SemaphoreRelease(m_BlitLoopStatus[sub][idx].NumSent));
            }
        }
    }

    SubroutineSleep();

    return rc;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SendVICCopies()
{
    RC rc;
    typedef vector<VICCopy>::const_iterator Iterator;
    const Iterator end = m_VICCopies.end();
    int iFlush = 0;
    for (Iterator it = m_VICCopies.begin(); it != end; ++it)
    {
        const VICCopy& copy = *it;
        CHECK_RC(m_VICDma.Copy(copy.srcHandle, copy.srcOffset,
                               copy.dstHandle, copy.dstOffset));

        // Flush occasionally to avoid overflowing the pushbuffer
        if (++iFlush % 16 == 0)
        {
            CHECK_RC(m_VICDma.Flush());
        }
    }
    CHECK_RC(m_VICDma.Flush());
    return rc;
}

//------------------------------------------------------------------------------
//! Structure used to communicate between NewWfMatsTest::WaitForBlitLoopDone
//! and the Tasker::PollFunc callback routine.
struct BlitLoopDoneData
{
    BlitLoopDoneData(UINT32 numSubdevices, UINT32 numBlitEngines)
    : pLwrTrafficAddress(NULL)
    , pLowTrafficAddress(NULL)
    , pHighTrafficAddress(NULL)
    , trafficPasses(0)
    , incrementTrafficAddress(false)
    {
        expectedValues.resize(numSubdevices);
        actualValues.resize(numSubdevices);
        pSemaphoreBuffers.resize(numSubdevices);
        timeDone.resize(numSubdevices);
        for (UINT32 i = 0; i < numSubdevices; i++)
        {
            expectedValues[i].resize(numBlitEngines);
            actualValues[i].resize(numBlitEngines);
            pSemaphoreBuffers[i].resize(numBlitEngines);
            timeDone[i].resize(numBlitEngines);
        }
    }

    // The outer vector represents gpuSubdevices, while the inside vector
    // represents blitEngines
    vector< vector<UINT32> >            expectedValues;
    vector< vector<UINT32> >            actualValues;
    vector< vector<volatile UINT32 *> > pSemaphoreBuffers;
    vector< vector<UINT64> >            timeDone;

    volatile UINT32 * pLwrTrafficAddress;
    volatile UINT32 * pLowTrafficAddress;
    volatile UINT32 * pHighTrafficAddress;
    UINT32            trafficPasses;
    bool              incrementTrafficAddress;
};

//------------------------------------------------------------------------------
static bool NewWfMatsBlitLoopDone (void * pv)
{
    BlitLoopDoneData *pData = (BlitLoopDoneData*)pv;

    UINT32 numSubdevices = static_cast<UINT32>(pData->expectedValues.size());
    if (pData->pLwrTrafficAddress)
    {
        // Generate some meaningless pcie bus traffic, which the HW guys say
        // makes the test more stressful.

        volatile UINT32 * addr = pData->pLwrTrafficAddress;
        UINT32 tmp = MEM_RD32(addr);

        if (pData->incrementTrafficAddress)
        {
            pData->pLwrTrafficAddress++;
            if (pData->pLwrTrafficAddress >= pData->pHighTrafficAddress)
            {
                // switch directions
                pData->incrementTrafficAddress = false;
                pData->trafficPasses++;
            }
        }
        else
        {
            pData->pLwrTrafficAddress--;
            if (pData->pLwrTrafficAddress <= pData->pLowTrafficAddress)
            {
                // switch directions
                pData->incrementTrafficAddress = true;
                pData->trafficPasses++;
            }
        }
        MEM_WR32(addr, ~tmp);
    }

    UINT32 numBlitEngines = static_cast<UINT32>(pData->expectedValues[0].size());
    for (UINT32 s = 0; s < numSubdevices; s++)
    {
        for (UINT32 i = 0; i < numBlitEngines; i++)
        {
            pData->actualValues[s][i] = MEM_RD32(pData->pSemaphoreBuffers[s][i]);
        }
    }

    bool ret = true;
    for (UINT32 s = 0; s < numSubdevices; s++)
    {
        for (UINT32 i = 0; i < numBlitEngines; i++)
        {
            if (pData->actualValues[s][i] < pData->expectedValues[s][i])
            {
                // This subdevice hasn't done its last semaphore release.
                ret = false;
            }
            else if (pData->actualValues[s][i] > pData->expectedValues[s][i])
            {
                // Uh oh -- corrupted semaphore?
                // ASSUMES that wraparound of NumSent won't ever happen.

                Printf(Tee::PriWarn,
                        "%s subdev %d expect %x got %x (corrupt semaphore)\n",
                        __FUNCTION__, s, pData->expectedValues[s][i],
                        pData->actualValues[s][i]);
                ret = false;
            }
            else if (pData->timeDone[s][i] == 0)
            {
                // This subdevice just finished.
                // Record a timestamp for bandwidth reporting.
                pData->timeDone[s][i] = Xp::QueryPerformanceCounter();
            }
        }
    }

    return ret;
}

//------------------------------------------------------------------------------
//! Wait for all gpus to complete their blit loops.
//!
RC NewWfMatsTest::WaitForBlitLoopDone(bool doPcieTraffic, bool doneOldMats)
{
    RC rc;

    // Make sure the final patterns are the non-ilwerted versions.
    CHECK_RC(SendFinalXorsIfNeeded());

    const UINT32 numSubdevices = GetBoundGpuDevice()->GetNumSubdevices();
    BlitLoopDoneData data(numSubdevices, m_NumBlitEngines);

    // Set up a data structure to communicate with the non-member
    // function used by Tasker::Poll.
    for (UINT32 s = 0; s < numSubdevices; s++)
    {
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            data.expectedValues[s][i]  = m_BlitLoopStatus[s][i].NumSent;
            data.actualValues[s][i]    = 0xffffffff;
            data.pSemaphoreBuffers[s][i] = (volatile UINT32 *)
                    m_BlitLoopStatus[s][i].SemaphoreBuf.GetAddress();
            data.timeDone[s][i] = m_BlitLoopStatus[s][i].TimeDone;
        }
    }

    if (doPcieTraffic)
    {
        // Tell the poll function to do a bunch of dummy read/write traffic
        // on one of the boxes reserved for the CPU loop, just to generate
        // some pcie traffic to the FB.
        // This is supposed to make the test more stressful.

        data.pLowTrafficAddress = (UINT32 *) m_MapFb.MapFbRegion(
            GetChunks(),
            m_BoxTable[m_CpuBoxIndices[0]].FbOffset,
            m_BoxBytes,
            GetBoundGpuSubdevice());

        data.pHighTrafficAddress = data.pLowTrafficAddress +
            m_BoxBytes/sizeof(UINT32) - 1;

        data.pLwrTrafficAddress = data.pLowTrafficAddress;
        data.incrementTrafficAddress = true;
        data.trafficPasses = 0;
    }

    UINT32 numQueued = 0;
    UINT32 allSent = 0;
    for (UINT32 s = 0; s < numSubdevices; s++)
    {
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            numQueued += m_BlitLoopStatus[s][i].NumSent -
                    m_BlitLoopStatus[s][i].NumCompleted;
            allSent += m_BlitLoopStatus[s][i].NumSent;
        }
    }
    if (!doneOldMats)
    {
        PrintProgressInit(allSent, "All sent subroutines");
    }
    // Default mods timeout on HW is 1 second.
    // We use this default timeout below waiting for the blit-loop semaphore
    // to roll over, but 1 second isn't long enough for really big FBs.
    //
    // Let the (normally 1 second) timeout fail N times before we declare that
    // the GPU is hung.

    const UINT32 maxTries = GuessBlitLoopSeconds();
    Printf(Tee::PriDebug, "Wait up to %g ms per chan-sub call.\n",
            maxTries * m_TimeoutMs);

    UINT32 triesLeft = maxTries;
    Tasker::UpdateHwPollFreq newPollFreq;
    const FLOAT64 origPollHz = Tasker::GetMaxHwPollHz();
    if (doPcieTraffic && (origPollHz <= 0.0))
    {
        // Hit PCIE BAR1 no faster than 1khz to avoid blocking RM's
        // register polling
        newPollFreq.Apply(1000.0); // bug 893186
    }

    while ( (triesLeft > 0) &&
            (numQueued > 0) &&
            ((RC::TIMEOUT_ERROR == rc) || (OK == rc)) )
    {
        VerbosePrintf("%s %d channel subroutines pending...\n",
               __FUNCTION__, numQueued);
        UINT32 newNumQueued = 0;

        rc.Clear();
        {
            DetachThread detach(m_DoDetach);
            rc = POLLWRAP_HW(NewWfMatsBlitLoopDone, &data, m_TimeoutMs);
        }

        for (UINT32 s = 0; s < numSubdevices; s++)
        {
            for (UINT32 i = 0; i < m_NumBlitEngines; i++)
            {
                if (data.actualValues[s][i] <= m_BlitLoopStatus[s][i].NumSent)
                {
                    m_BlitLoopStatus[s][i].NumCompleted = data.actualValues[s][i];
                    m_BlitLoopStatus[s][i].TimeDone = data.timeDone[s][i];

                    newNumQueued += m_BlitLoopStatus[s][i].NumSent -
                            m_BlitLoopStatus[s][i].NumCompleted;
                }
                else
                {
                    // Corrupt semaphore, don't update NumCompleted or numQueued.
                    // We have no way of knowing how "done" this subdevice is.
                    // Continue waiting for other subdevices if any.
                    rc = RC::DATA_MISMATCH;
                }
            }
        }

        if (newNumQueued < numQueued)
        {
            // Gpu is not hung, reset our timeout counter.
            triesLeft = maxTries;
            numQueued = newNumQueued;
            if (!doneOldMats)
            {
                PrintProgressUpdate(allSent - numQueued);
            }
        }
        else
        {
            // No forward progress, decrement our timeout counter.
            --triesLeft;
        }
    }

    for (UINT32 s = 0; s < numSubdevices; s++)
    {
        bool reportBandwidth = m_ReportBandwidth;
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            if (m_BlitLoopStatus[s][i].NumCompleted != m_BlitLoopStatus[s][i].NumSent)
            {
                reportBandwidth = false;
                Printf(Tee::PriWarn,
                        "%s subdev %d semaphore timeout:\n"
                        "  expected %d, current semaphore is %d\n",
                        __FUNCTION__,
                        s,
                        m_BlitLoopStatus[s][i].NumSent,
                        data.actualValues[s][i]);
            }
        }

        if (reportBandwidth)
        {
            ReportBandwidth(GetVerbosePrintPri());
        }
    }

    if (doPcieTraffic)
    {
        double trafficBytes = data.trafficPasses * m_BoxBytes;

        ptrdiff_t d = data.pLwrTrafficAddress - data.pLowTrafficAddress;

        if (data.incrementTrafficAddress)
            trafficBytes += sizeof(UINT32) * d;
        else
            trafficBytes += m_BoxBytes - sizeof(UINT32) * d;

        VerbosePrintf(
            "%s subdev %d did %.6fMB pcie traffic\n",
            __FUNCTION__,
            GetBoundGpuSubdevice()->GetSubdeviceInst(),
            trafficBytes/1e6);
    }
    return rc;
}

//-----------------------------------------------------------------------------
// Guess how long a single blit-loop ought to take (for setting timeouts).
//
UINT32 NewWfMatsTest::GuessBlitLoopSeconds()
{
    const double numBlits = m_NumBlitLoops * (m_BlitNumBoxes+1);
    const double numBytes = numBlits * m_BoxBytes;

    // bytesPerSecond is very pessimistic
    // We scale it based on the number of SMs, because performance of
    // 2D version of NewWfMats depends on the number of SMs available.
    // We also scale it based on the GpcClk, since low clocks harm 2D
    // engine performance.
    //
    // This give us some margin so we don't declare failure too soon.
    UINT64 freqHz = 0;
    GetBoundGpuSubdevice()->GetClock(Gpu::ClkGpc, &freqHz, nullptr, nullptr, nullptr);
    const double bytesPerSecond =
       (freqHz / 2.0e9) * 0.05e9 * GetBoundGpuSubdevice()->ShaderCount();

    const UINT32 result = static_cast<UINT32>(numBytes / bytesPerSecond);

    return max(result, UINT32(2));
}

//------------------------------------------------------------------------------
// check the "blit" set of tiles to make sure all of them contain the original
// data that was in there before the loop.  If there are any differences, then
// a memory error has oclwrred.
//------------------------------------------------------------------------------
RC NewWfMatsTest::CheckBlitBoxes(UINT32 subdev)
{
    RC rc = OK;

    CallbackArguments args;
    CHECK_RC(FireCallbacks(ModsTest::MISC_B, Callbacks::STOP_ON_ERROR,
                           args, "Pre-CheckBlitBoxes"));

    VerbosePrintf("CheckBlitBoxes()\n");

    // For SLI devices, send methods to only one of the gpus.
    UINT32 subdevMask = 1 << subdev;

    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
    {
        for (UINT32 i = 0; i < m_NumBlitEngines; i++)
        {
            m_pCh[i]->WriteSetSubdevice(subdevMask);
        }
    }

    // Used by the error-reporting callback function.
    m_LwrrentReadbackSubdev = subdev;

    if (m_SaveReadBackData)
    {
        // Init with blank entries to store one copy of each pattern.
        vector<SavedReadBackData> tmp (m_PatternCount);
        m_ReadBackData.swap(tmp);
    }

    vector<UINT32> patternIds;
    CHECK_RC(m_PatternClass.GetSelectedPatterns(&patternIds));

    for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
    {

        // When logging errors, we need to colwert the offset within the box
        // to an absolute physical address of some box within that same
        // blit-loop.  That way the address-decode will get the right partition
        // and channel.
        m_ExtraOffsetForHandleError =
            m_BoxTable[ m_BlitBoxIndices[loop][0] ].FbOffset;
        m_LwrrentBlitLoop = loop;

        for (UINT32 iiBox = 0; iiBox < m_BlitNumBoxes; iiBox++)
        {
            UINT32 iThisBox = m_BlitBoxIndices[loop][iiBox];
            UINT32 thisCheckBuf = iiBox%2;

            if (iiBox == 0)
            {
                // Start the transfer of the first box.
                CHECK_RC(TransferBoxToSystemMemory(iThisBox,
                                                   thisCheckBuf, subdev));
            }
            // Else, transfer was started by previous loop.

            // Wait for transfer of current box to complete.
            CHECK_RC(WaitForTransfer());

            DetachThread detach(m_DoDetach);

            // Check after Notifier.Wait to not break with a pending DMA.

            if (iiBox + 1 < m_BlitNumBoxes)
            {
                // Start transfer of the next loop's box, to keep the GPU busy
                // while the CPU is checking this loop's box for errors.
                UINT32 iNextBox = m_BlitBoxIndices[loop][iiBox+1];
                UINT32 nextCheckBuf = (iiBox+1)%2;
                CHECK_RC(TransferBoxToSystemMemory(iNextBox,
                                                   nextCheckBuf, subdev));
            }

            const UINT32 patShift = m_BlitLoopStatus[subdev][loop % m_NumBlitEngines].LwrPatShift;
            // Check this box for errors.
            const UINT32 patternId =
                patternIds[ (iiBox + patShift) % m_PatternCount ];
            UINT32 * bufAddr = static_cast<UINT32*>(m_CompareBuf.GetAddress());
            if (m_BitLanePatterns.empty())
            {
                const UINT32 origCheckBuf = thisCheckBuf;
                for (UINT32 rechkIdx = 0;
                     rechkIdx <= GetNumRechecks(); rechkIdx++)
                {
                    if (GetNumRechecks())
                    {
                        Tasker::AttachThread attach;
                        if (rechkIdx == 1)
                            CHECK_RC(WaitForTransfer());

                        if (rechkIdx >= 1)
                        {
                            // Re-reads use 3rd slot in "ping/pong" buffer.
                            thisCheckBuf = 2;
                            CHECK_RC(TransferBoxToSystemMemory(
                                            iThisBox, thisCheckBuf, subdev));
                            CHECK_RC(WaitForTransfer());
                        }

                        // Do not log error for retries other than last
                        m_LogError = false;
                        if (rechkIdx == GetNumRechecks())
                            m_LogError = true;
                    }

                    m_RetryHasErrors = false;
                    CHECK_RC(m_PatternClass.CheckMemory(
                                    bufAddr + m_BoxPixels * thisCheckBuf,
                                    m_BoxBytes,
                                    1, // line
                                    m_BoxBytes, // widthInBytes
                                    m_pTestConfig->DisplayDepth(), // depth
                                    patternId,
                                    &NewWfMatsTest::HandleError,   // callback
                                    this));

                    // m_RetryHasErrors can only be true if error is
                    // found for that particular retry and dma recheck
                    // is enabled
                    if (m_RetryHasErrors)
                    {
                        if (thisCheckBuf != origCheckBuf)
                        {
                            if (!IsMemorySame(bufAddr +
                                              m_BoxPixels * origCheckBuf,
                                              bufAddr +
                                              m_BoxPixels * thisCheckBuf))
                            {
                                // Intermittent and FB error should be reported
                                SetIntermittentError(true);
                            }
                        }

                    }
                    else
                    {
                        if (rechkIdx > 0)
                            // Intermittent error should be reported.
                            // Test pass after few(one or more) retries
                            SetIntermittentError(true);

                        break;
                    }

                }
            }
            else
            {
                const UINT32* const expectedBox =
                    &m_BitLaneBoxDataByLoop[loop][0];
                for (UINT32 offset=0; offset < m_BoxBytes; offset+=4)
                {
                    const UINT32 index = offset / 4;
                    const UINT32 actual = MEM_RD32(bufAddr+index);
                    const UINT32 expected = expectedBox[index];
                    if (actual != expected)
                    {
                        HandleError(offset, expected, actual, 0xffffffff, "");
                    }
                }
            }

            if (m_SaveReadBackData)
            {
                // Save raw data for JS code, one box per data pattern only.

                if (iiBox < m_PatternCount)
                {
                    // Store this saved data in the right point to undo the
                    // shift (so m_ReadBackData[0] has the first pattern).
                    SavedReadBackData & srd = m_ReadBackData[patternId];

                    srd.FbOffset    = m_BoxTable[iThisBox].FbOffset;
                    srd.PteKind     = GetChunks()->begin()->pteKind;
                    srd.PageSizeKB  = GetChunks()->begin()->pageSizeKB;
                    srd.Data.assign(bufAddr + m_BoxPixels * thisCheckBuf,
                            bufAddr + m_BoxPixels * (1+thisCheckBuf));
                }
            }
        }

        for (UINT32 loop = 0; loop < m_ErrCountByPartChan.size(); loop++)
        {
            Printf(Tee::PriLow, "Part Chan Decode %d Error Count %d\n",
                        loop, m_ErrCountByPartChan[loop]);
        }

    }
    return rc;
}

// Compare system memories data for the same box with diffrent copy
bool NewWfMatsTest::IsMemorySame(const UINT32 *buf1, const UINT32 *buf2)
{
    for (UINT32 count = 0; count < m_BoxBytes/sizeof(UINT32); count++)
    {
        if (MEM_RD32(buf1 + count) != MEM_RD32(buf2 + count))
            return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Transfer the contents of a box to main (system) memory
//------------------------------------------------------------------------------
RC NewWfMatsTest::TransferBoxToSystemMemory( UINT32 boxNum, UINT32 bufNum,
                                             UINT32 subdev)
{
    switch (m_AuxMethod)
    {
        case UseCE:
            return TransferBoxToSystemMemoryCE(boxNum, bufNum, subdev);
        case UseVIC:
            return TransferBoxToSystemMemoryVIC(boxNum, bufNum);
        default:
            MASSERT(!"Invalid configuration!");
            return RC::SOFTWARE_ERROR;
    }
}

RC NewWfMatsTest::TransferBoxToSystemMemoryCE
(
    UINT32 boxNum,
    UINT32 bufNum,
    UINT32 subdev
)
{
    RC rc = OK;

    UINT64 srcOffset64 = m_BoxTable[boxNum].CtxDmaOffset;

    UINT64 dstOffset64 = m_CompareBuf.GetCtxDmaOffsetGpu() +
            ((bufNum % 2) ? m_BoxBytes : 0);

    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_OFFSET_IN_UPPER,
        2,
        static_cast<UINT32>(srcOffset64>>32), /* LW90B5_OFFSET_IN_UPPER */
        static_cast<UINT32>(srcOffset64))); /* LW90B5_OFFSET_IN_LOWER */

    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_OFFSET_OUT_UPPER,
        2,
        static_cast<UINT32>(dstOffset64>>32), /* LW90B5_OFFSET_OUT_UPPER */
        static_cast<UINT32>(dstOffset64))); /* LW90B5_OFFSET_OUT_LOWER */

    UINT32 layoutMem = 0;
    layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
    layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);

    if (m_AuxCopyEng.GetClass() == GF100_DMA_COPY)
    {
        UINT32 addressMode = 0;
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL);
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL);

        addressMode |= DRF_DEF(90B5,
                               _ADDRESSING_MODE,
                               _SRC_TARGET,
                               _LOCAL_FB);

        switch (m_CompareBuf.GetLocation())
        {
            case Memory::Fb:
                addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE,
                                       _DST_TARGET, _LOCAL_FB);
                break;

            case Memory::Coherent:
                addressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _DST_TARGET,
                                       _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                addressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _DST_TARGET, _NONCOHERENT_SYSMEM);
                break;

            default:
                Printf(Tee::PriError, "Unsupported memory type for source\n");
                return RC::BAD_MEMLOC;
        }

        CHECK_RC(m_pAuxCh->Write(s_CopyEng,
                                 LW90B5_ADDRESSING_MODE, addressMode));
    }
    else
    {
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
    }

    // When running on SOC assume that the FB surfaces being copied into sysmem
    // are cached and therefore need a flush
    const bool bCacheFlushReqd = GetBoundGpuSubdevice()->IsSOC();

    // Generate new semaphore payload
    const UINT32 dummyPayload = IncPayload();
    const UINT32 semaPayload = IncPayload();
    m_AuxSemaphore.SetOneTriggerPayload(0, semaPayload);

    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_SET_SEMAPHORE_PAYLOAD,
                             bCacheFlushReqd ? dummyPayload : semaPayload));

    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_LAUNCH_DMA,
                          layoutMem |
                          DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                                  _RELEASE_ONE_WORD_SEMAPHORE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                                  _NON_PIPELINED) |
                          DRF_DEF(90B5,
                                  _LAUNCH_DMA,
                                  _MULTI_LINE_ENABLE,
                                  _TRUE)));

    if (bCacheFlushReqd)
    {
        const UINT64 auxSemaOffset = m_AuxSemaphore.GetCtxDmaOffsetGpu(0);

        CHECK_RC(m_pAuxCh->SetSemaphoreOffset(auxSemaOffset));
        CHECK_RC(m_pAuxCh->WriteL2FlushDirty());
        CHECK_RC(m_pAuxCh->SemaphoreRelease(semaPayload));
    }

    CHECK_RC(m_pAuxCh->Flush());

    return rc;
}

RC NewWfMatsTest::TransferBoxToSystemMemoryVIC
(
    UINT32 boxNum,
    UINT32 bufNum
)
{
    const UINT32 boxOffset = static_cast<UINT32>(m_BoxTable[boxNum].CtxDmaOffset);
    const UINT32 boxHandle = m_BoxTable[boxNum].CtxDmaHandle;

    const UINT32 dstOffset = static_cast<UINT32>(((bufNum % 2) ? m_BoxBytes : 0));
    const UINT32 dstHandle = m_CompareBuf.GetTegraMemHandle();

    RC rc;
    CHECK_RC(m_VICDma.Copy(boxHandle, boxOffset,
                           dstHandle, dstOffset));

    const UINT32 semaPayload = IncPayload();
    m_AuxSemaphore.SetOneTriggerPayload(0, semaPayload);

    CHECK_RC(m_VICDma.SemaphoreRelease(m_AuxSemaphore.GetSurface(0), 0, semaPayload));

    CHECK_RC(m_pCh[0]->Flush());

    return rc;
}

//------------------------------------------------------------------------------
// Transfer the contents of a main memory buffer to box to fb memory
//------------------------------------------------------------------------------
RC NewWfMatsTest::TransferSystemMemoryToBox(UINT32 boxNum)
{
    switch (m_AuxMethod)
    {
        case UseCE:
            return TransferSystemMemoryToBoxCE(boxNum);
        case UseVIC:
            return TransferSystemMemoryToBoxVIC(boxNum);
        default:
            MASSERT(!"Invalid configuration!");
            return RC::SOFTWARE_ERROR;
    }
}

RC NewWfMatsTest::TransferSystemMemoryToBoxWait()
{
    switch (m_AuxMethod)
    {
        case UseCE:
            return TransferSystemMemoryToBoxCEWait();
        case UseVIC:
            return TransferSystemMemoryToBoxVICWait();
        default:
            MASSERT(!"Invalid configuration!");
            return RC::SOFTWARE_ERROR;
    }
}

RC NewWfMatsTest::TransferSystemMemoryToBoxCE(UINT32 boxNum)
{
    RC rc;

    UINT64 dstOffset64 = m_BoxTable[boxNum].CtxDmaOffset;
    UINT64 srcOffset64 = m_CompareBuf.GetCtxDmaOffsetGpu();

    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_OFFSET_IN_UPPER,
        2,
        static_cast<UINT32>(srcOffset64>>32), /* LW90B5_OFFSET_IN_UPPER */
        static_cast<UINT32>(srcOffset64))); /* LW90B5_OFFSET_IN_LOWER */

    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_OFFSET_OUT_UPPER,
        2,
        static_cast<UINT32>(dstOffset64>>32), /* LW90B5_OFFSET_OUT_UPPER */
        static_cast<UINT32>(dstOffset64))); /* LW90B5_OFFSET_OUT_LOWER */

    UINT32 layoutMem = 0;
    layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
    layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);

    if (m_AuxCopyEng.GetClass() == GF100_DMA_COPY)
    {
        UINT32 addressMode = 0;
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL);
        addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL);
        switch (m_CompareBuf.GetLocation())
        {
            case Memory::Fb:
                addressMode |= DRF_DEF(90B5, _ADDRESSING_MODE,
                                       _SRC_TARGET, _LOCAL_FB);
                break;

            case Memory::Coherent:
                addressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _SRC_TARGET,
                                       _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                addressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _SRC_TARGET, _NONCOHERENT_SYSMEM);
                break;

            default:
                Printf(Tee::PriError, "Unsupported memory type for source\n");
                return RC::BAD_MEMLOC;
        }

        addressMode |= DRF_DEF(90B5,
                               _ADDRESSING_MODE,
                               _DST_TARGET,
                               _LOCAL_FB);
        CHECK_RC(m_pAuxCh->Write(s_CopyEng,
                                 LW90B5_ADDRESSING_MODE, addressMode));
    }
    else
    {
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
    }

    const UINT32 semaPayload = IncPayload();
    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_SET_SEMAPHORE_PAYLOAD,
                             semaPayload));

    // Use NON_PIPELINED to WAR hw bug 618956.
    CHECK_RC(m_pAuxCh->Write(s_CopyEng, LW90B5_LAUNCH_DMA,
                          layoutMem |
                          DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                                  _RELEASE_ONE_WORD_SEMAPHORE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                                  _NON_PIPELINED) |
                          DRF_DEF(90B5,
                                  _LAUNCH_DMA,
                                  _MULTI_LINE_ENABLE,
                                  _TRUE)));
    return rc;
}

RC NewWfMatsTest::TransferSystemMemoryToBoxCEWait()
{
    RC rc;

    CHECK_RC(m_pAuxCh->Flush());

    m_AuxSemaphore.SetOneTriggerPayload(0, m_AuxPayload);
    CHECK_RC(m_AuxSemaphore.Wait(m_TimeoutMs));

    return rc;
}

RC NewWfMatsTest::TransferSystemMemoryToBoxVIC(UINT32 boxNum)
{
    const UINT32 boxOffset = static_cast<UINT32>(m_BoxTable[boxNum].CtxDmaOffset);
    const UINT32 boxHandle = m_BoxTable[boxNum].CtxDmaHandle;

    const UINT32 srcHandle = m_CompareBuf.GetTegraMemHandle();

    return m_VICDma.Copy(srcHandle, 0, boxHandle, boxOffset);
}

RC NewWfMatsTest::TransferSystemMemoryToBoxVICWait()
{
    RC rc;

    const UINT32 semaPayload = IncPayload();

    CHECK_RC(m_VICDma.SemaphoreRelease(m_AuxSemaphore.GetSurface(0), 0, semaPayload));

    CHECK_RC(m_pCh[0]->Flush());

    m_AuxSemaphore.SetOneTriggerPayload(0, semaPayload);
    CHECK_RC(m_AuxSemaphore.Wait(m_TimeoutMs));

    return rc;
}

RC NewWfMatsTest::WaitForTransfer()
{
    Tasker::Yield();

    if (m_AuxMethod == UseCE || m_AuxMethod == UseVIC)
    {
        return m_AuxSemaphore.Wait(m_TimeoutMs);
    }
    else
    {
        return m_Notifier[0].Wait(1, m_TimeoutMs);
    }
}

//------------------------------------------------------------------------------
// Fill the initial boxes in the blit loop
//------------------------------------------------------------------------------
RC NewWfMatsTest::FillBlitLoopBoxes()
{
    RC rc;

    CallbackArguments args;
    CHECK_RC(FireCallbacks(ModsTest::MISC_A, Callbacks::STOP_ON_ERROR,
                           args, "Pre-FillBlitLoopBoxes"));

    UINT32 i, idx;
    UINT32 patternCount = 0;

    patternCount = m_PatternClass.GetLwrrentNumOfSelectedPatterns();

    if (m_BitLanePatterns.empty())
    {
        for ( idx=0; idx< patternCount; idx++)
        {
            m_PatternClass.FillMemoryIncrementing(
                    (UINT32 *)m_CompareBuf.GetAddress(),
                    m_BoxBytes, // pitchInBytes -- doesnt matter in this case
                    1, // line
                    m_BoxBytes, // widthInBytes
                    32); // pixel-bits

            for ( i = idx; i < m_BlitNumBoxes; i += patternCount)
            {
                for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
                {
                    CHECK_RC(TransferSystemMemoryToBox(m_BlitBoxIndices[loop][i]));
                }
            }
            CHECK_RC(TransferSystemMemoryToBoxWait());
        }
    }
    else
    {
        // Prepare arrays with original box contents
        m_BitLaneBoxDataByLoop.resize(m_NumBlitLoops);

        // Go through all box classes
        for (UINT32 loop = 0; loop < m_NumBlitLoops; loop++)
        {
            // Get partition/channel configuration for this box class
            vector<UINT08> chunks;
            UINT32 chunkSize = 0;
            const Box& box = m_BoxTable[m_BlitBoxIndices[loop][0]];
            CHECK_RC(GpuUtility::DecodeRangePartChan(
                        GetBoundGpuSubdevice(),
                        *GetChunks()->begin(),
                        box.FbOffset,
                        box.FbOffset+m_BoxBytes,
                        &chunks,
                        &chunkSize));

            // Join per partition/channel configuration
            vector<UINT32> partChanIndex(m_BitLanePatterns.size(), 0);
            m_BitLaneBoxDataByLoop[loop].clear();
            m_BitLaneBoxDataByLoop[loop].reserve(m_BoxBytes/4);
            for (i = 0; i < m_BoxBytes; i+=4)
            {
                const UINT32 partChan = chunks[i/chunkSize];
                UINT32& index = partChanIndex[partChan];
                const UINT32 value = m_BitLanePatterns[partChan][index];
                index++;
                if (index > m_BitLanePatterns[partChan].size())
                {
                    index = 0;
                }
                m_BitLaneBoxDataByLoop[loop].push_back(value);
            }

            // Copy box data to comparebuf
            Platform::MemCopy(m_CompareBuf.GetAddress(),
                              &m_BitLaneBoxDataByLoop[loop][0], m_BoxBytes);

            // Fill all boxes with the patterns
            for (i = 0; i < m_BlitNumBoxes; i++)
            {
                CHECK_RC(TransferSystemMemoryToBox(m_BlitBoxIndices[loop][i]));
            }
            CHECK_RC(TransferSystemMemoryToBoxWait());
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// Fill the given box with the given color
//------------------------------------------------------------------------------
void NewWfMatsTest::FillBox(UINT32 boxNum, UINT32 color, INT32 direction)
{
    volatile UINT32 *pBase;
    volatile UINT32 *pLocation;

    const INT32 intBoxHeight = INT32(m_BoxHeight);
    const INT32 intBoxWidth  = INT32(c_BoxWidthPixels);

    DetachThread detach(m_DoDetach);

    pBase = static_cast<UINT32*>(m_MapFb.MapFbRegion(
                                        GetChunks(),
                                        m_BoxTable[boxNum].FbOffset,
                                        m_BoxBytes, // size
                                        GetBoundGpuSubdevice()));

    if (direction > 0)
    {
        for (INT32 row = 0; row < intBoxHeight; row++)
        {
            pLocation = pBase + row * c_BoxWidthPixels;
            for (INT32 col = 0; col < intBoxWidth; col++)
            {
                MEM_WR32(pLocation, color);

                // Conform to the hardware polling requirement
                // requested by "-poll_hw_hz" (i.e. HW should not be
                // accessed faster than a certain rate).  By default
                // this will not sleep or yield unless the command
                // line argument is present.
                Tasker::PollMemSleep();

                pLocation++;
            }
#ifndef USE_NEW_TASKER
            if ((row & 0xf) == 0)
                Tasker::Yield();
#endif
        }
    }
    else
    {
        for (INT32 row = intBoxHeight - 1; row >= 0; row--)
        {
            pLocation = pBase + row * intBoxWidth + intBoxWidth - 1;
            for (INT32 col = intBoxWidth - 1; col >= 0; col--)
            {
                MEM_WR32(pLocation, color);

                // Conform to the hardware polling requirement
                // requested by "-poll_hw_hz" (i.e. HW should not be
                // accessed faster than a certain rate).  By default
                // this will not sleep or yield unless the command
                // line argument is present.
                Tasker::PollMemSleep();

                pLocation--;
            }
#ifndef USE_NEW_TASKER
            if ((row & 0xf) == 0)
                Tasker::Yield();
#endif
        }
    }
}

//------------------------------------------------------------------------------
// Check each location in the given box to see if it is equal to "Check", then
// write that location to "Fill"
//------------------------------------------------------------------------------
RC NewWfMatsTest::CheckFillBox
(
    UINT32 boxNum,
    UINT32 check,
    UINT32 fill,
    INT32 direction
)
{
    UINT32 readData;
    UINT32 reRead1;
    UINT32 reRead2;
    RC rc;

    const INT32 intBoxHeight = static_cast<INT32>(m_BoxHeight);
    const INT32 intBoxWidth  = static_cast<INT32>(c_BoxWidthPixels);

    MemError * pErrObj =
        &GetMemError(GetBoundGpuSubdevice()->GetSubdeviceInst());

    GpuUtility::MemoryChunks::iterator chunk = GetChunks()->begin();
    const UINT32 pteKind = chunk->pteKind;
    const UINT32 pageSizeKB = chunk->pageSizeKB;

    DetachThread detach(m_DoDetach);

    volatile UINT32 *base = static_cast<UINT32 *>(
            m_MapFb.MapFbRegion(GetChunks(),
                                m_BoxTable[boxNum].FbOffset,
                                m_BoxBytes, // size
                                GetBoundGpuSubdevice()));

    if (direction > 0)
    {
        for (INT32 row = 0; row < intBoxHeight; row++)
        {
            UINT32 rowOffset    = row * c_BoxWidthPixels;
            UINT32 endRowOffset = rowOffset + intBoxWidth;
            do
            {
                volatile UINT32 * addr = base + rowOffset;

                readData = MEM_RD32(addr);

                if (readData != check)
                {
                    reRead1 =  MEM_RD32(addr);
                    reRead2 =  MEM_RD32(addr);

                    Tasker::AttachThread attach;

                    CHECK_RC(pErrObj->LogMemoryError(
                                    32,
                                    m_BoxTable[boxNum].FbOffset
                                    + rowOffset * sizeof(UINT32),
                                    readData,
                                    check,
                                    reRead1,
                                    reRead2,
                                    pteKind,
                                    pageSizeKB,
                                    MemError::NO_TIME_STAMP,
                                    "",
                                    0));
                }

                MEM_WR32(addr, fill);

                // Conform to the hardware polling requirement
                // requested by "-poll_hw_hz" (i.e. HW should not be
                // accessed faster than a certain rate).  By default
                // this will not sleep or yield unless the command
                // line argument is present.
                Tasker::PollMemSleep();

                rowOffset++;
            }
            while (rowOffset < endRowOffset);

#ifndef USE_NEW_TASKER
            if ((row & 0xf) == 0)
                Tasker::Yield();
#endif
        }
    }
    else
    {
        for (INT32 row = intBoxHeight - 1; row >= 0; row--)
        {
            UINT32 rowOffset    = row * c_BoxWidthPixels + intBoxWidth - 1;
            UINT32 endRowOffset = rowOffset - intBoxWidth;
            do
            {
                volatile UINT32 * addr = base + rowOffset;

                readData = MEM_RD32(addr);

                if (readData != check)
                {
                    reRead1 =  MEM_RD32(addr);
                    reRead2 =  MEM_RD32(addr);

                    Tasker::AttachThread attach;

                    CHECK_RC(pErrObj->LogMemoryError(
                                    32,
                                    m_BoxTable[boxNum].FbOffset
                                    + rowOffset * sizeof(UINT32),
                                    readData,
                                    check,
                                    reRead1,
                                    reRead2,
                                    pteKind,
                                    pageSizeKB,
                                    MemError::NO_TIME_STAMP,
                                    "",
                                    0));
                }
                MEM_WR32(addr, fill);

                // Conform to the hardware polling requirement
                // requested by "-poll_hw_hz" (i.e. HW should not be
                // accessed faster than a certain rate).  By default
                // this will not sleep or yield unless the command
                // line argument is present.
                Tasker::PollMemSleep();

                rowOffset--;
            }
            while (rowOffset != endRowOffset);

#ifndef USE_NEW_TASKER
            if ((row & 0xf) == 0)
                Tasker::Yield();
#endif
        }
    }
    return OK;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SetPatternSet( jsval *tmpJ )
{
    RC            rc = OK;
    JavaScriptPtr pJs;
    JsArray jsa;
    rc = pJs->FromJsval(*tmpJ, &jsa);
    if (OK == rc) // it is an array of pattern no's
    {
        vector<UINT32> SelectedPatterns;
        UINT32 tmpI = 0;
        for ( UINT32 i = 0; i < jsa.size(); i++ )
        {
            CHECK_RC(pJs->FromJsval(jsa[i], &tmpI));
            SelectedPatterns.push_back( tmpI );
        }
        m_PatternClass.SelectPatterns( &SelectedPatterns );
        //Printf(Tee::PriNormal, "PatternSet is set to a vector of pattern "
        //    "numbers!\n");
    }
    else // it is a PatternClass::PatternSet selection
    {
        UINT32 pttrnSelection;
        CHECK_RC(pJs->FromJsval(*tmpJ, &pttrnSelection));

        if ( pttrnSelection !=
             static_cast<UINT32>(PatternClass::PATTERNSET_NORMAL) &&
             pttrnSelection !=
             static_cast<UINT32>(PatternClass::PATTERNSET_STRESSFUL) &&
             pttrnSelection !=
             static_cast<UINT32>(PatternClass::PATTERNSET_USER) &&
             pttrnSelection !=
             static_cast<UINT32>(PatternClass::PATTERNSET_ALL) )
        {
            Printf(Tee::PriNormal, "WfMats.PatternSet value is invalid!\n");
            return RC::SOFTWARE_ERROR;
        }
        else
        {
            m_PatternSet =
                    static_cast<PatternClass::PatternSets>(pttrnSelection);
            m_PatternClass.SelectPatternSet(m_PatternSet);
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
void NewWfMatsTest::SetCtxDmaTwoD
(
    ChannelSubroutine * pChSub
)
{
    if (m_TwoD.GetClass() != FERMI_TWOD_A)
    {
        // All boxes are in the same ctxdma.
        // Send it once now, rather than in each BoxTwoD call.

        GpuUtility::MemoryChunks::iterator ichunk = GetChunks()->begin();
        pChSub->Write(s_TwoD, LW502D_SET_SRC_CONTEXT_DMA, ichunk->hCtxDma);
        pChSub->Write(s_TwoD, LW502D_SET_DST_CONTEXT_DMA, ichunk->hCtxDma);
    }
}

//------------------------------------------------------------------------------
void NewWfMatsTest::BoxTwoD
(
    ChannelSubroutine * pChSub,
    UINT32 sourceBox,
    UINT32 destBox
)
{
    UINT64 srcOffset64 = m_BoxTable[sourceBox].CtxDmaOffset;
    UINT64 dstOffset64 = m_BoxTable[destBox  ].CtxDmaOffset;

    if (m_CtxDmaOffsetFitIn32Bits)
    {
        // No need to send UPPER methods.
        pChSub->Write(s_TwoD, LW502D_SET_SRC_OFFSET_LOWER,
                UINT32(srcOffset64));
        pChSub->Write(s_TwoD, LW502D_SET_DST_OFFSET_LOWER,
                UINT32(dstOffset64));
    }
    else
    {
        pChSub->Write(s_TwoD, LW502D_SET_SRC_OFFSET_UPPER, 2,
            /* LW502D_SET_SRC_OFFSET_UPPER */ UINT32(srcOffset64>>32),
            /* LW502D_SET_SRC_OFFSET_LOWER */ UINT32(srcOffset64));
        pChSub->Write(s_TwoD, LW502D_SET_DST_OFFSET_UPPER, 2,
            /* LW502D_SET_DST_OFFSET_UPPER */ UINT32(dstOffset64>>32),
            /* LW502D_SET_DST_OFFSET_LOWER */ UINT32(dstOffset64));
    }

    if (m_UseVariableBlits)
    {
        vector<UINT32> BlitWidths;
        GetRandomBlitWidths(&BlitWidths, c_TwoDTransferWidth * 4);

        UINT32 offset = 0;
        for (UINT32 i = 0; i < BlitWidths.size(); i++)
        {
            pChSub->Write(s_TwoD,
                LW502D_SET_PIXELS_FROM_MEMORY_DST_WIDTH,
                BlitWidths[i]);

            pChSub->Write(s_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT,
                          offset);
            pChSub->Write(s_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_X0, offset);
            pChSub->Write(s_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0, 0);
            pChSub->Write(s_TwoD, LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT, 0);

            offset += BlitWidths[i];
        }
    }
    else if (m_BlitsPerBoxCopy > 1)
    {
        // Narrow or Legacy mode, use many blits at increasing X offsets.
        for (UINT32 i = 0; i < m_BlitsPerBoxCopy; i++)
        {
            pChSub->Write(s_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT, i);
            pChSub->Write(s_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_X0, i);
            pChSub->Write(s_TwoD, LW502D_SET_PIXELS_FROM_MEMORY_DST_Y0, 0);
            pChSub->Write(s_TwoD, LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT, 0);
        }
    }
    else  // Normal mode, one 32-pixel-wide blit.
    {
        pChSub->Write(s_TwoD, LW502D_PIXELS_FROM_MEMORY_SRC_Y0_INT, 0);
    }
}

//------------------------------------------------------------------------------
void NewWfMatsTest::BoxCopyEngine
(
    ChannelSubroutine * pChSub,
    UINT32 sourceBox,
    UINT32 destBox
)
{
    const UINT64 srcOffset64 = m_BoxTable[sourceBox].CtxDmaOffset;
    const UINT64 dstOffset64 = m_BoxTable[destBox  ].CtxDmaOffset;

    UINT32 launchFlags =
                /* 0 == LWC0B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE */
                /* 0 == LWC0B5_LAUNCH_DMA_REMAP_ENABLE_FALSE */
                DRF_DEF(C0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                DRF_DEF(C0B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _TRUE) |
                DRF_DEF(C0B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _NONE) |
                DRF_DEF(C0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED);

    UINT32 effectiveWidth;
    UINT32 effectiveHeight;

    if (m_UseBlockLinear)
    {
        launchFlags |=
                (DRF_DEF(C0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _BLOCKLINEAR) |
                 DRF_DEF(C0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _BLOCKLINEAR));
        effectiveWidth  = c_TwoDTransferWidth * 4;
        effectiveHeight = c_TwoDTransferHeightMult*m_BoxHeight;
    }
    else
    {
        launchFlags |=
                (DRF_DEF(C0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                 DRF_DEF(C0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH));
        effectiveWidth  = c_BoxWidthBytes;
        effectiveHeight = m_BoxHeight;
    }

    pChSub->Write(s_CopyEng, LWC0B5_OFFSET_IN_UPPER, 8,
                /* LWC0B5_OFFSET_IN_UPPER */
                UINT32(srcOffset64 >> 32),
                /* LWC0B5_OFFSET_IN_LOWER */
                UINT32(srcOffset64),
                /* LWC0B5_OFFSET_OUT_UPPER */
                UINT32(dstOffset64 >> 32),
                /* LWC0B5_OFFSET_OUT_LOWER */
                UINT32(dstOffset64),
                /* LWC0B5_PITCH_IN */
                effectiveWidth,
                /* LWC0B5_PITCH_OUT */
                effectiveWidth,
                /* LWC0B5_LINE_LENGTH_IN */
                effectiveWidth,
                /* LWC0B5_LINE_COUNT */
                effectiveHeight);

    if (m_CopyEng[0].GetClass() == GF100_DMA_COPY)
    {
        pChSub->Write(s_CopyEng, LW90B5_ADDRESSING_MODE,
                    DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL) |
                    DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL) |
                    DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _LOCAL_FB) |
                    DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _LOCAL_FB));
    }
    else
    {
        launchFlags |= DRF_DEF(C0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        launchFlags |= DRF_DEF(C0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
    }

    pChSub->Write(s_CopyEng, LWC0B5_LAUNCH_DMA, launchFlags);
}

//------------------------------------------------------------------------------
void NewWfMatsTest::BoxVIC
(
    ChannelSubroutine* pChSub,
    UINT32             sourceBox,
    UINT32             destBox
)
{
    const UINT32 srcHandle = static_cast<UINT32>(m_BoxTable[sourceBox].CtxDmaHandle);
    const UINT32 dstHandle = static_cast<UINT32>(m_BoxTable[destBox  ].CtxDmaHandle);
    const UINT32 srcOffset = static_cast<UINT32>(m_BoxTable[sourceBox].CtxDmaOffset);
    const UINT32 dstOffset = static_cast<UINT32>(m_BoxTable[destBox  ].CtxDmaOffset);

    m_VICCopies.push_back(VICCopy(srcHandle, srcOffset, dstHandle, dstOffset));
}

//------------------------------------------------------------------------------
// Send the rop state.  Assumes that the caller will take care of
// Flush().
//------------------------------------------------------------------------------
void NewWfMatsTest::SetRopXorTwoD
(
    ChannelSubroutine * pChSub,
    bool UseXor
)
{
    // The Y32 format is very slow in G80 with TWOD (1/2 speed),
    // but the A8R8G8B8 format doesn't work correctly with the XOR pass.
    if (UseXor)
    {
        pChSub->Write(s_TwoD, LW502D_SET_OPERATION,
                      LW502D_SET_OPERATION_V_ROP_AND);
        pChSub->Write(s_TwoD, LW502D_SET_SRC_FORMAT,
                      LW502D_SET_DST_FORMAT_V_Y32);
        pChSub->Write(s_TwoD, LW502D_SET_DST_FORMAT,
                      LW502D_SET_DST_FORMAT_V_Y32);
    }
    else
    {
        pChSub->Write(s_TwoD, LW502D_SET_OPERATION,
                      LW502D_SET_OPERATION_V_SRCCOPY);
        pChSub->Write(s_TwoD, LW502D_SET_SRC_FORMAT,
                      LW502D_SET_DST_FORMAT_V_A8R8G8B8);
        pChSub->Write(s_TwoD, LW502D_SET_DST_FORMAT,
                      LW502D_SET_DST_FORMAT_V_A8R8G8B8);
    }
}

RC NewWfMatsTest::SelectPatterns( vector<UINT32> * pVec )
{
    return m_PatternClass.SelectPatterns(pVec);
}

RC NewWfMatsTest::GetSavedReadBackData (UINT32 pattern, jsval *pjsv)
{
    RC rc;
    JavaScriptPtr pJs;
    JsArray       outerArray;
    jsval         tmpjs;

    if (pattern >= m_ReadBackData.size())
        return RC::ILWALID_OBJECT_PROPERTY;

    // [0] is Data
    JsArray innerArray;
    for (UINT32 i = 0; i < m_ReadBackData[pattern].Data.size(); ++i)
    {
        CHECK_RC(pJs->ToJsval(m_ReadBackData[pattern].Data[i], &tmpjs));
        innerArray.push_back(tmpjs);
    }
    CHECK_RC(pJs->ToJsval(&innerArray, &tmpjs));
    outerArray.push_back(tmpjs);

    // [1] is FbOffset
    CHECK_RC(pJs->ToJsval(m_ReadBackData[pattern].FbOffset, &tmpjs));
    outerArray.push_back(tmpjs);

    // [2] is ignored (formerly partStride)
    CHECK_RC(pJs->ToJsval(0, &tmpjs));
    outerArray.push_back(tmpjs);

    // [3] is PteKind
    CHECK_RC(pJs->ToJsval(m_ReadBackData[pattern].PteKind, &tmpjs));
    outerArray.push_back(tmpjs);

    // [4] is PageSizeKB
    CHECK_RC(pJs->ToJsval(m_ReadBackData[pattern].PageSizeKB, &tmpjs));
    outerArray.push_back(tmpjs);

    return pJs->ToJsval(&outerArray, pjsv);
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::GetBestRotations( UINT32 maxPerChanPatLen, JsArray * pjsa )
{
    MASSERT(pjsa);

    if (0 == m_ReadBackData.size())
        return RC::ILWALID_OBJECT_PROPERTY;

    RC rc;
    vector<UINT32> patternNums;

    CHECK_RC(m_PatternClass.GetSelectedPatterns(&patternNums));

    // We should have one readback-buffer for each data-pattern, per blitloop.
    MASSERT(patternNums.size() * m_NumBlitLoops == m_ReadBackData.size());

    FbBitRotation::Rots rots;
    vector<UINT32> patBuf;

    for (UINT32 i = 0; i < m_ReadBackData.size(); ++i)
    {
        // fill pattern buffer
        const UINT32 patNum = patternNums[i % patternNums.size() ];
        const UINT32 patLen = m_PatternClass.GetPatternLen(patNum);

        patBuf.assign(patLen, 0);

        CHECK_RC(m_PatternClass.FillMemoryWithPattern(
                &patBuf[0],
                patLen*sizeof(UINT32),
                1,
                patLen*sizeof(UINT32),
                32,
                patNum,
                NULL));

        FbBitRotation::Rots tmprots;

        // get best-match rotations vs. readback buffer
        CHECK_RC(FbBitRotation::FindBest(
                GetBoundGpuSubdevice()->GetFB(),
                m_ReadBackData[i].FbOffset,
                m_ReadBackData[i].PteKind,
                m_ReadBackData[i].PageSizeKB,
                &patBuf[0],
                patLen,
                maxPerChanPatLen,
                &m_ReadBackData[i].Data[0],
                UINT32(m_ReadBackData[i].Data.size()),
                &tmprots));

        // Save highest-correlation rotation value for each FBIO bit.
        if (rots.size() == 0)
        {
            rots = tmprots;
        }
        else
        {
            for (size_t ibit = 0; ibit < rots.size(); ibit++)
            {
                if (tmprots[ibit].corr > rots[ibit].corr)
                    rots[ibit] = tmprots[ibit];
            }
        }
    }

    // Colwert results to JS data array.
    JavaScriptPtr pJs;

    pjsa->clear();

    for (UINT32 i = 0; i < rots.size(); ++i)
    {
        jsval jsv;
        CHECK_RC(pJs->ToJsval(rots[i].shift, &jsv));
        pjsa->push_back(jsv);
    }

    return rc;
}

RC NewWfMatsTest::GetRandomBlitWidths
(
    vector<UINT32>* pBlitWidths,
    UINT32 totalWidth
)
{
    MASSERT(pBlitWidths);
    pBlitWidths->clear();
    // create a vector of blit sizes with a sum of TotalWidth
    while (totalWidth > 0)
    {
        UINT32 width = m_pRandom->GetRandom(1, totalWidth);
        totalWidth = totalWidth - width;
        pBlitWidths->push_back(width);
    }

    return OK;
}

//------------------------------------------------------------------------------
// E.g. on Fermi, 4 conselwtive bursts go out to the memory chip in
// reverse order.
RC NewWfMatsTest::ApplyBurstReversal(vector<UINT08>* pPattern, FrameBuffer* pFB)
{
    const UINT32 burstLength = pFB->GetBurstSize() / pFB->GetAmapColumnSize();
    const UINT32 numReversedBursts = pFB->GetNumReversedBursts();
    if (1 != Utility::CountBits(numReversedBursts))
    {
        Printf(Tee::PriError,
               "Burst reversal algorithm requires number of reversed bursts"
               " to be power of 2\n");
        return RC::SOFTWARE_ERROR;
    }
    if (1 != Utility::CountBits(burstLength))
    {
        Printf(Tee::PriError,
               "Burst reversal algorithm requires burst length"
               " to be power of 2\n");
        return RC::SOFTWARE_ERROR;
    }
    vector<UINT08> patternWithReversals(pPattern->size());
    const UINT32 mask = (numReversedBursts - 1) * burstLength;
    for (UINT32 i=0; i < pPattern->size(); i++)
    {
        patternWithReversals[i] = (*pPattern)[i ^ mask];
    }
    pPattern->swap(patternWithReversals);
    return OK;
}

//------------------------------------------------------------------------------
//! Fixes user bit lane pattern of 0s and 1s to be ready for use by the test.
//!
//! The function extends the pattern so that its length is a multiple of
//! burst length times the number of reversed bursts.
//!
//! On Fermi, each burst consists of 8 dwords (32 bytes), which is
//! also named L2 sector and each L2 cache line consists of 128 bytes,
//! so each cache line consists of four bursts.
//!
//! When a cache line is flushed out to the DRAM, the four bursts
//! are sent out in reverse order, but dwords within each burst
//! go out in ascending order. So if we enumerate dwords for four conselwtive
//! burst from 0 through 31 (8 dwords per burst times 4 bursts), then the dwords
//! will be sent out to the DRAM as: 24..31, 16..23, 8..15, 0..7.
RC NewWfMatsTest::DecodeUserBitLanePatternForAll
(
    GpuSubdevice* pGpuSubdevice,
    const UINT08* pattern,
    UINT32 size,
    vector<UINT08>* pOutPattern
)
{
    MASSERT(pattern);
    MASSERT(pOutPattern);

    // Check input
    if (size == 0)
    {
        Printf(Tee::PriError, "Invalid empty pattern\n");
        return RC::BAD_PARAMETER;
    }
    for (UINT32 i=0; i < size; i++)
    {
        if (pattern[i] > 1)
        {
            Printf(Tee::PriError,
                   "Invalid pattern - only 0s and 1s are allowed\n");
            return RC::BAD_PARAMETER;
        }
    }

    // Get FB configuration
    FrameBuffer* const pFB = pGpuSubdevice->GetFB();
    const UINT32 burstLength = pFB->GetBurstSize() / pFB->GetAmapColumnSize();

    // Expand the pattern to cover entire burst lengths
    pOutPattern->clear();
    if ((size % burstLength) != 0)
    {
        const UINT32 newSize = size + burstLength - (size % burstLength);
        pOutPattern->reserve(size+burstLength);
        while (pOutPattern->size() < newSize)
        {
            pOutPattern->insert(pOutPattern->end(), pattern, pattern+size);
        }
        pOutPattern->resize(newSize);
        size = newSize;
    }
    else
    {
        pOutPattern->insert(pOutPattern->end(), pattern, pattern+size);
    }

    // Perform burst reversal. On Fermi, 4 conselwtive bursts go out to
    // the memory chip in reverse order.
    const UINT32 numReversedBursts = pFB->GetNumReversedBursts();
    if (numReversedBursts > 1)
    {
        // Check pattern length against number of reversed bursts
        const UINT32 numDwords = numReversedBursts * burstLength;
        if (((size > numDwords) && ((size%numDwords) != 0)) ||
            ((size < numDwords) && ((numDwords%size) != 0)))
        {
            Printf(Tee::PriError,
                    "Invalid pattern length. Due to burst reversal "
                    "pattern length must be expandable to %u burst lengths "
                    "(%u pattern elements)\n", numReversedBursts, numDwords);
            return RC::BAD_PARAMETER;
        }

        // Expand pattern to support burst reversal
        if (size < numDwords)
        {
            vector<UINT08> expandedPattern;
            expandedPattern.reserve(numDwords);
            while (expandedPattern.size() < numDwords)
            {
                expandedPattern.insert(expandedPattern.end(),
                        pOutPattern->begin(), pOutPattern->end());
            }
            MASSERT(expandedPattern.size() == numDwords);
            pOutPattern->swap(expandedPattern);
            size = numDwords;
        }

        // Perform burst reversal
        RC rc;
        CHECK_RC(ApplyBurstReversal(pOutPattern, pFB));
    }
    return OK;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SetBitLanePatternForAll(const UINT08* pattern, UINT32 size)
{
    RC rc;
    vector<UINT08> tmpPattern;
    CHECK_RC(DecodeUserBitLanePatternForAll(
            GetBoundGpuSubdevice(),
            pattern,
            size,
            &tmpPattern));
    size = static_cast<UINT32>(tmpPattern.size());

    // Get FB configuration
    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numFbios    = pFB->GetFbioCount();
    const UINT32 numChannels = pFB->GetSubpartitions();

    // Set the pattern
    const UINT32 numPartChan = numFbios * numChannels;
    m_BitLanePatterns.resize(numPartChan);
    m_BitLanePatterns[0].clear();
    m_BitLanePatterns[0].reserve(size);
    for (UINT32 i=0; i < size; i++)
    {
        m_BitLanePatterns[0].push_back(tmpPattern[i] ? ~0U : 0U);
    }
    for (UINT32 i=1; i < numPartChan; i++)
    {
        m_BitLanePatterns[i] = m_BitLanePatterns[0];
    }

    // Print the pattern
    Printf(Tee::PriDebug, "Set pattern for all partitions, channels:\n");
    for (UINT32 i=0; i < size; i++)
    {
        Printf(Tee::PriDebug, "%u", (UINT32)tmpPattern[i]);
    }
    Printf(Tee::PriDebug, "\n");
    return OK;
}

//------------------------------------------------------------------------------
//! Fixes user bit lane pattern of 0s and 1s to be ready for use by the test.
//!
//! This function assumes that the DecodeUserBitLanePatternForAll has already
//! been called and therefore we expect the user pattern to be of expected size,
//! or the expected size is a multiple of the pattern size.
//!
//! See the description of DecodeUserBitLanePatternForAll to find out what this
//! function is doing exactly.
RC NewWfMatsTest::DecodeUserBitLanePattern
(
    GpuSubdevice* pGpuSubdevice,
    const UINT08* pattern,
    UINT32 size,
    UINT32 expectedSize,
    vector<UINT08>* pOutPattern
)
{
    MASSERT(pattern);
    MASSERT(pOutPattern);

    // Check input
    if (size == 0)
    {
        Printf(Tee::PriError, "Invalid empty pattern\n");
        return RC::BAD_PARAMETER;
    }
    for (UINT32 i=0; i < size; i++)
    {
        if (pattern[i] > 1)
        {
            Printf(Tee::PriError,
                   "Invalid pattern - only 0s and 1s are allowed\n");
            return RC::BAD_PARAMETER;
        }
    }

    // Expand the pattern to cover entire burst lengths
    vector<UINT08> tmpPattern;
    if (size > expectedSize)
    {
        Printf(Tee::PriError,
            "Pattern too long - use SetPatternForAll to reset allowed size\n");
        return RC::BAD_PARAMETER;
    }
    if (size < expectedSize)
    {
        tmpPattern.reserve(expectedSize);
        while (tmpPattern.size() < expectedSize)
        {
            tmpPattern.insert(tmpPattern.end(), pattern, pattern+size);
        }
        tmpPattern.resize(expectedSize);
        pattern = &tmpPattern[0];
        size = expectedSize;
    }
    else
    {
        tmpPattern.insert(tmpPattern.end(), pattern, pattern+size);
        pattern = &tmpPattern[0];
    }

    // Perform burst reversal
    FrameBuffer* const pFB = pGpuSubdevice->GetFB();
    const UINT32 numReversedBursts = pFB->GetNumReversedBursts();
    if (numReversedBursts > 1)
    {
        RC rc;
        CHECK_RC(ApplyBurstReversal(&tmpPattern, pFB));
        pattern = &tmpPattern[0];
    }

    // Return new pattern
    pOutPattern->clear();
    pOutPattern->insert(pOutPattern->end(), pattern, pattern+size);

    return OK;
}

//------------------------------------------------------------------------------
RC NewWfMatsTest::SetBitLanePattern
(
    char partitionLetter,
    UINT32 bit,
    const UINT08* pattern,
    UINT32 size
)
{
    // Get FB configuration
    FrameBuffer* const pFB = GetBoundGpuSubdevice()->GetFB();
    const UINT32 numFbios    = pFB->GetFbioCount();
    const UINT32 numChannels = pFB->GetSubpartitions();
    const UINT32 burstLength = pFB->GetBurstSize() / pFB->GetAmapColumnSize();
    const UINT32 numPartChan = numFbios * numChannels;

    // Create full pattern set if it's empty
    if (m_BitLanePatterns.empty())
    {
        const UINT32 fullSize = size + burstLength - (size % burstLength);
        m_BitLanePatterns.resize(numPartChan);
        m_BitLanePatterns[0].clear();
        m_BitLanePatterns[0].resize(fullSize, 0);
        for (UINT32 i=1; i < numPartChan; i++)
        {
            m_BitLanePatterns[i] = m_BitLanePatterns[0];
        }
    }

    // Colwert user pattern to FB-friendly pattern
    RC rc;
    vector<UINT08> tmpPattern;
    CHECK_RC(DecodeUserBitLanePattern(
            GetBoundGpuSubdevice(),
            pattern,
            size,
            static_cast<UINT32>(m_BitLanePatterns[0].size()),
            &tmpPattern));
    size = static_cast<UINT32>(tmpPattern.size());

    // Colwert letter to partition index
    const UINT32 hwFbio = partitionLetter - 'A';
    if ((pFB->GetFbioMask() & (1 << hwFbio)) == 0)
    {
        Printf(Tee::PriError,
                "Invalid partition letter %c\n", partitionLetter);
        return RC::BAD_PARAMETER;
    }
    const UINT32 virtualFbio = pFB->HwFbioToVirtualFbio(hwFbio);

    if (bit >= numChannels*32)
    {
        Printf(Tee::PriError,
                "Invalid bit %u (there are only %u bits)\n",
                bit, numChannels*32);
        return RC::BAD_PARAMETER;
    }
    const UINT32 channel = bit / 32;

    // Copy pattern to be blit-ready
    const UINT32 partChan = virtualFbio * numChannels + channel;
    const UINT32 bitMask = 1 << (bit % 32);
    for (UINT32 i=0; i < size; i++)
    {
        m_BitLanePatterns[partChan][i] =
            (m_BitLanePatterns[partChan][i] & ~bitMask)
                | (tmpPattern[i] ? bitMask : 0);
    }

    // Print the pattern
    Printf(Tee::PriDebug, "Set pattern for partition %u, bit %u:\n",
            virtualFbio, bit);
    for (UINT32 i=0; i < size; i++)
    {
        Printf(Tee::PriDebug, "%u", (UINT32)tmpPattern[i]);
    }
    Printf(Tee::PriDebug, "\n");
    return OK;
}

//------------------------------------------------------------------------------
// Link NewWfMatsTest to JavaScript.
//------------------------------------------------------------------------------

JS_CLASS_INHERIT(NewWfMatsTest, GpuMemTest,
                 "Very high-bandwidth memory test using blits");

CLASS_PROP_READWRITE(NewWfMatsTest, CpuPattern, UINT32,
                     "Which pattern (0-31) to use for dumb framebuffer accesses.");
CLASS_PROP_READWRITE(NewWfMatsTest, CpuTestBytes, UINT32,
                     "Size of FB tested with CPU r/w (controls test time)");
CLASS_PROP_READWRITE(NewWfMatsTest, OuterLoops, UINT32,
                     "Number of times to repeat outer loop (controls test time)");
CLASS_PROP_READWRITE(NewWfMatsTest, InnerLoops, UINT32,
                     "Number of times to repeat inner loop (controls test time)");
CLASS_PROP_READWRITE(NewWfMatsTest, BoxHeight, UINT32,
                     "Box height (0 = pick automatically)");
CLASS_PROP_READWRITE(NewWfMatsTest, ExitEarly, UINT32,
                     "Don't set this unless you know what you are doing");
CLASS_PROP_READWRITE(NewWfMatsTest, UseXor, bool,
                     "Bool: Use Xor ROP operations");
CLASS_PROP_READWRITE_LWSTOM(NewWfMatsTest, UseCopyEngine,
                     "Bool: Use CopyEngine instead of 2D");
CLASS_PROP_READWRITE(NewWfMatsTest, NumCopyEngines, UINT32,
                     "Number of CopyEngines to use when UseCopyEngine is enabled (default is max available)");
CLASS_PROP_READWRITE(NewWfMatsTest, Method, UINT32,
                     "Hardware method used for copying boxes");
CLASS_PROP_READWRITE(NewWfMatsTest, AuxMethod, UINT32,
                     "Hardware method for init/readback transfers");
CLASS_PROP_READWRITE(NewWfMatsTest, LegacyUseNarrow, bool,
                     "Bool: Use semi-narrow blits (bug 452249 compatibility mode)");
CLASS_PROP_READWRITE(NewWfMatsTest, UseNarrowBlits, bool,
                     "Bool: Use narrow blits (more DQM activity but less BW)");
CLASS_PROP_READWRITE(NewWfMatsTest, DoPcieTraffic, bool,
                     "Bool: Do cpu r/w over pcie during blit-loop in RunLiveFbioTuning");
CLASS_PROP_READWRITE(NewWfMatsTest, UseBlockLinear, bool,
                     "Bool: Use BlockLinear (default is false).");
CLASS_PROP_READWRITE(NewWfMatsTest, VerifyChannelSwizzle, bool,
                     "Bool: Verify whether the test correctly handles channel swizzling (default is false).");
CLASS_PROP_READWRITE(NewWfMatsTest, SaveReadBackData, bool,
                     "UINT32: Save the data read back from FB into system memory.");
CLASS_PROP_READWRITE(NewWfMatsTest, UseVariableBlits, bool,
                     "Bool: Vary the width of each blit operation.");
CLASS_PROP_READWRITE(NewWfMatsTest, VariableBlitSeed, UINT32,
                     "Seed for the pseudo-random value generator use for selecting variable blit widths.");
CLASS_PROP_READWRITE(NewWfMatsTest, MaxBlitLoopErrors, UINT32,
                     "Max errors to decode for blit-loop boxes (0 = no limit).");
CLASS_PROP_READWRITE(NewWfMatsTest, DoDetach, bool,
                     "Bool: Improve multi-threading (default: true)");

PROP_CONST(NewWfMatsTest, PRE_FILL,      ModsTest::MISC_A);
PROP_CONST(NewWfMatsTest, PRE_CHECK,     ModsTest::MISC_B);

PROP_CONST(NewWfMatsTest, Use2D,         NewWfMatsTest::Use2D);
PROP_CONST(NewWfMatsTest, UseCE,         NewWfMatsTest::UseCE);
PROP_CONST(NewWfMatsTest, UseVIC,        NewWfMatsTest::UseVIC);

P_(NewWfMatsTest_Set_UseCopyEngine)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);

    bool value = false;
    RC rc = JavaScriptPtr()->FromJsval(*pValue, &value);
    if (rc == OK)
    {
        rc = pTest->SetMethod(value ? NewWfMatsTest::UseCE : NewWfMatsTest::Use2D);
    }

    if (rc == OK)
    {
        return JS_TRUE;
    }
    else
    {
        JS_ReportError(pContext, "Failed to set NewWfMats.UseCopyEngine");
        return JS_FALSE;
    }
}

P_(NewWfMatsTest_Get_UseCopyEngine)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);
    if (OK != JavaScriptPtr()->ToJsval(pTest->GetMethod() == NewWfMatsTest::UseCE, pValue))
    {
        JS_ReportError(pContext, "Failed to get NewWfMatsTest.UseCopyEngine");
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

//------------------------------------------------------------------------------
P_(NewWfMatsTest_Set_PatternSet)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);

    if ( OK == pTest->SetPatternSet(pValue) )
        return JS_TRUE;
    else
    {
        JS_ReportError(pContext, "Failed to set NewWfMats.PatternSet");
        return JS_FALSE;
    }
}
P_(NewWfMatsTest_Get_PatternSet)
{
    MASSERT(pContext != 0);
    MASSERT(pValue   != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);
    UINT32 result = static_cast<UINT32>(pTest->GetPatternSet());
    if (OK != JavaScriptPtr()->ToJsval(result, pValue))
    {
        JS_ReportError(pContext, "Failed to get NewWfMatsTest.PatternSet" );
        *pValue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}
static SProperty NewWfMatsTest_PatternSet
        (
        NewWfMatsTest_Object,
        "PatternSet",
        0,
        0,
        NewWfMatsTest_Get_PatternSet,
        NewWfMatsTest_Set_PatternSet,
        0,
        "Each bit represents a blit pattern"
        );

C_(NewWfMats_SetPattern);
static STest NewWfMats_SetPattern
        (
        NewWfMatsTest_Object,
        "SetPattern",
        C_NewWfMats_SetPattern,
        0,
        "Set the pattern to be used with the provided ID"
        );

C_(NewWfMats_SetPattern)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);

    if ( NumArguments != 1 )  // there should be only one argument provided
    {
        Printf(Tee::PriNormal,
               "Usage: NewWfMats.SetPattern(PatternID)\n");
        RETURN_RC(OK);
    }
    JavaScriptPtr pJavaScript;
    UINT32 id;
    if ( OK != pJavaScript->FromJsval(pArguments[0], &id ) )
    {
        JS_ReportWarning(pContext, "Error in NewWfMats.SetPattern()");
        RETURN_RC(RC::SOFTWARE_ERROR);
    }
    else
    {
        vector<UINT32> vec;
        vec.push_back( id );
        RETURN_RC(pTest->SelectPatterns( &vec ));
    }
}

C_(NewWfMats_RunLiveFbioTuning);
static STest NewWfMats_RunLiveFbioTuning
        (
        NewWfMatsTest_Object,
        "RunLiveFbioTuning",
        C_NewWfMats_RunLiveFbioTuning,
        0,
        "Load trial FBIO tuning, and run a single blit loop during vblank."
        );

C_(NewWfMats_RunLiveFbioTuning)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);

    if ( NumArguments != 0 )
    {
        Printf(Tee::PriNormal,
               "Usage: NewWfMats.RunLiveFbioTuning()\n");
        RETURN_RC(OK);
    }
    RETURN_RC(pTest->RunLiveFbioTuningWrapper());
}

C_(NewWfMats_GetSavedReadBackData);
static SMethod NewWfMats_GetSavedReadBackData
        (
        NewWfMatsTest_Object,
        "GetSavedReadBackData",
        C_NewWfMats_GetSavedReadBackData,
        1,
        "Get saved data from one blit-loop box."
        );

C_(NewWfMats_GetSavedReadBackData)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;
    UINT32 pattern;

    if ( (NumArguments != 1) ||
         (OK != pJavaScript->FromJsval(pArguments[0], &pattern)))
    {
        JS_ReportError(pContext,
                       "Usage: NewWfMats.GetSavedReadBackData(pattern#)\n");
        return JS_FALSE;
    }

    if (pTest->GetSavedReadBackData(pattern, pReturlwalue) != OK)
    {
        JS_ReportError(pContext, "Error getting box data\n");
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_(NewWfMats_GetBestRotations);
static STest NewWfMats_GetBestRotations
        (
        NewWfMatsTest_Object,
        "GetBestRotations",
        C_NewWfMats_GetBestRotations,
        2,
        "Get best-match data-delays per-data-bit."
        );

C_(NewWfMats_GetBestRotations)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    NewWfMatsTest *pTest = (NewWfMatsTest *)JS_GetPrivate(pContext, pObject);
    JavaScriptPtr pJavaScript;
    UINT32     maxPerChanPatLen;
    JSObject * pReturlwals;

    if ( (NumArguments != 2) ||
         (OK != pJavaScript->FromJsval(pArguments[0], &maxPerChanPatLen)) ||
         (OK != pJavaScript->FromJsval(pArguments[1], &pReturlwals)) )
    {
        JS_ReportError(pContext,
                       "Usage: NewWfMats.GetBestRotations(perChanPatLen, [Array])\n");
        return JS_FALSE;
    }

    JsArray jsa;
    RC rc;
    C_CHECK_RC(pTest->GetBestRotations(maxPerChanPatLen, &jsa));

    for (UINT32 i = 0; i < jsa.size(); ++i)
    {
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, i, jsa[i]));
    }
    RETURN_RC(OK);
}

C_(NewWfMats_SetBitLanePatternForAll);
static STest NewWfMats_SetBitLanePatternForAll
        (
        NewWfMatsTest_Object,
        "SetBitLanePatternForAll",
        C_NewWfMats_SetBitLanePatternForAll,
        0,
        "Sets the specified bit lane pattern for all partitions, channels and burst orders."
        );

C_(NewWfMats_SetBitLanePatternForAll)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    NewWfMatsTest* const pTest =
        static_cast<NewWfMatsTest*>(JS_GetPrivate(pContext, pObject));

    JavaScriptPtr pJavaScript;
    JsArray jsPattern;
    if ((NumArguments != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsPattern)))
    {
        JS_ReportError(pContext,
               "Usage: NewWfMats.SetBitLanePatternForAll(PatternArray)\n");
        return JS_FALSE;
    }
    vector<UINT08> pattern;
    pattern.reserve(jsPattern.size());
    for (size_t i=0; i < jsPattern.size(); i++)
    {
        UINT16 value;
        if ((OK != pJavaScript->FromJsval(jsPattern[i], &value)) || (value > 1))
        {
            JS_ReportError(pContext,
                   "Invalid pattern value\n");
            return JS_FALSE;
        }
        pattern.push_back(static_cast<UINT08>(value));
    }

    RETURN_RC(pTest->SetBitLanePatternForAll(
                    &pattern[0], static_cast<UINT32>(pattern.size())));
}

C_(NewWfMats_SetBitLanePattern);
static STest NewWfMats_SetBitLanePattern
        (
        NewWfMatsTest_Object,
        "SetBitLanePattern",
        C_NewWfMats_SetBitLanePattern,
        0,
        "Sets the specified bit lane pattern for the specified partition and bit."
        );

C_(NewWfMats_SetBitLanePattern)
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    NewWfMatsTest* const pTest =
        static_cast<NewWfMatsTest*>(JS_GetPrivate(pContext, pObject));

    JavaScriptPtr pJavaScript;
    JsArray jsPattern;
    string partition;
    UINT32 bit;
    if ((NumArguments != 3) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &jsPattern)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &partition)) ||
        (partition.length() != 1) ||
        (OK != pJavaScript->FromJsval(pArguments[2], &bit)))
    {
        JS_ReportError(pContext,
               "Usage: NewWfMats.SetBitLanePattern(PatternArray, Partition, Bit)\n");
        return JS_FALSE;
    }
    vector<UINT08> pattern;
    pattern.reserve(jsPattern.size());
    for (size_t i=0; i < jsPattern.size(); i++)
    {
        UINT16 value;
        if ((OK != pJavaScript->FromJsval(jsPattern[i], &value)) || (value > 1))
        {
            JS_ReportError(pContext,
                   "Invalid pattern value\n");
            return JS_FALSE;
        }
        pattern.push_back(static_cast<UINT08>(value));
    }

    RETURN_RC(pTest->SetBitLanePattern(partition[0], bit, &pattern[0],
                                       static_cast<UINT32>(pattern.size())));
}
