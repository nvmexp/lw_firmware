/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2014,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gputest.h"
#include "core/include/lwrm.h"
#include "gpu/utility/surf2d.h"
#include "class/cl95b1.h" // LW95B1_VIDEO_MSVLD
#include "core/include/fpicker.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "lwos.h"
#include "core/include/circsink.h"
#include "core/include/golden.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpugoldensurfaces.h"
#include "gpu/js/fpk_comm.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "gpu/include/gralloc.h"
#include "ctrl/ctrl0080.h"
#include "core/include/jsonlog.h"

//-----------------------------------------------------------------------------
//! \file cl88b4.cpp
//! \brief GPU mfg test for the SEC (SECurity) engine.
//!
//! The SEC engine is a DMA engine that also handles encryption and
//! decryption, as required by HD-DVD video data in memory-spaces that might
//! be accessible to hackers trying to make unlicensed copies of movies.
//!
//! We will test a randomized sequence of transfers using all possible features.
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
class SecTest : public GpuTest
{
public:
    SecTest();
    virtual ~SecTest();
    virtual bool IsSupported();
    virtual RC Run();
    virtual RC Setup();
    virtual RC Cleanup();

    RC     SetDefaultPickers(UINT32 first, UINT32 last);
    void Print() const;
    bool SemaphoreIsWritten();

    enum CompareType
    {
        ShouldBeSame,
        ShouldBeDifferent
    };
    enum misc
    {
        SUBCHAN = 0,                    // subchannel for the SEC object
        SEM_DONE_VAL = 0xd1a60000       // arbitrary unlikely semaphore value
    };
    enum CtxDmaIdx
    {
        SEMAPHORE_CTX_DMA_IDX = 0,
        FB_CTX_DMA_IDX = 1,
        HOST_CTX_DMA_IDX = 2,
    };

private:
    RC   AllocSurfaces();
    void FillSurfaces();
    void FreeSurfaces();
    RC   DoRandomLoops();
    RC   DoOneLoop();
    RC   SendCopyMethods(Surface2D * pSrc, UINT32 srcOffset, Surface2D * pDst);
    RC   SetupSEC();
    RC   SetupSemaphore();
    RC   WaitSemaphore();
    RC   CheckCopy(Surface2D * pSrc, UINT32 SrcOffset, Surface2D * pDst, CompareType cmp);
    void PrintCopyAction(Surface2D * pSrc, UINT32 srcOffset, Surface2D * pDst, const char * opStr) const;

    Goldelwalues *          m_pGolden;
    GpuTestConfiguration *  m_pTestConfig;
    GpuGoldenSurfaces *     m_pGGSurfs;
    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray *      m_pPickers;
    LwRm::Handle            m_hRmEvent;
    Tasker::EventID         m_TaskerEventId;
    Surface2D               m_SurfSemaphore;
    Surface2D             * m_pSurfFb;
    Surface2D               m_SurfHost;
    bool                    m_AwakenAllowed;
    UINT32                  m_SubdevMask;
    SelwrityAlloc           m_Sec;
    LwRm::Handle            m_hCh;
    Channel *               m_pCh;

    // Details about the previous transfer:
    UINT32                  m_BlockCount;
    UINT32                  m_ByteCount;
    UINT32                  m_SrcOffset;
    UINT32                  m_DstOffset;
    UINT32                  m_SrcBlockOffset;
    UINT32                  m_DstBlockOffset;
    UINT32                  m_SrcCtxDmaIdx;
    UINT32                  m_DstCryptCtxDmaIdx;
    UINT32                  m_DstClearCtxDmaIdx;
    bool                    m_SrcIsFb;
    bool                    m_DstIsFb;
    UINT32                  m_CopyAction;  // plain/encrypt/decrypt
    bool                    m_SeparateSemaphore;  // else sema-release with trigger
    bool                    m_Awaken;
    UINT32                  m_AppId;
    UINT32                  m_ContentKey[4];
    UINT32                  m_IV[4];
    UINT32                  m_EncryptedContentKey[4];
    UINT32                  m_SessionKey[4];
    Surface2D *             m_pSurfSrc;
    Surface2D *             m_pSurfDstCrypt;
    Surface2D *             m_pSurfDstClear;
};

//-----------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ SecTest object.

JS_CLASS_INHERIT
(
    SecTest,
    GpuTest,
    "SEC test."
);

//-----------------------------------------------------------------------------
static void PrintFunc(void * pvSecTest)
{
    SecTest * pSecTest = (SecTest *)pvSecTest;
    return pSecTest->Print();
}

//-----------------------------------------------------------------------------
SecTest::SecTest()
{
    SetName("SecTest");
    m_AwakenAllowed = false;
    m_pCh = NULL;
    m_hCh = 0;
    m_hRmEvent = 0;
    m_TaskerEventId = NULL;
    m_pGGSurfs = NULL;
    m_pTestConfig = GetTestConfiguration();
    m_pGolden = GetGoldelwalues();
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_SEC_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
    m_pSurfFb = nullptr;
    m_SubdevMask = 0;
    m_BlockCount = 0;
    m_ByteCount = 0;
    m_SrcOffset = 0;
    m_DstOffset = 0;
    m_SrcBlockOffset = 0;
    m_DstBlockOffset = 0;
    m_SrcCtxDmaIdx = 0;
    m_DstCryptCtxDmaIdx = 0;
    m_DstClearCtxDmaIdx = 0;
    m_SrcIsFb = false;
    m_DstIsFb = false;
    m_CopyAction = 0;
    m_SeparateSemaphore = false;
    m_Awaken = false;
    m_AppId = 0;
    memset(&m_ContentKey, 0, sizeof(m_ContentKey));
    memset(&m_IV, 0, sizeof(m_IV));
    memset(&m_EncryptedContentKey, 0, sizeof(m_EncryptedContentKey));
    memset(&m_SessionKey, 0, sizeof(m_SessionKey));
    m_pSurfDstCrypt = 0;
    m_pSurfSrc = 0;
    m_pSurfDstClear = 0;
}

//-----------------------------------------------------------------------------
/* virtual */
SecTest::~SecTest()
{
}

//-----------------------------------------------------------------------------
/* virtual */
bool SecTest::IsSupported()
{
    // Not supported on CheetAh
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        return false;
    }

    return m_Sec.IsSupported(GetBoundGpuDevice());
}

//-----------------------------------------------------------------------------
/* virtual */
RC SecTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());


    // Reserve a Display object, disable UI, set default graphics mode,
    // alloc a default primary surface, SetImage.
    CHECK_RC(GpuTest::AllocDisplayAndSurface(false/*blockLinear*/));

    CHECK_RC( m_pTestConfig->AllocateChannel(&m_pCh, &m_hCh) );
    CHECK_RC( AllocSurfaces() );

    // Initialize our Goldelwalues object.
    m_pGGSurfs = new GpuGoldenSurfaces(GetBoundGpuDevice());
    m_pGGSurfs->AttachSurface(m_pSurfFb, "C", GpuTest::GetPrimaryDisplay());
    CHECK_RC(m_pGolden->SetSurfaces(m_pGGSurfs));
    m_pGolden->SetPrintCallback(&PrintFunc, this);

    CHECK_RC( SetupSEC() );

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */
RC SecTest::Run()
{
    // For SLI devices, send methods to only one of the gpus.
    m_SubdevMask = 1 << GetBoundGpuSubdevice()->GetSubdeviceInst();

    const RC rc = DoRandomLoops();

    if ((OK != rc) &&
        (RC::MEM_TO_MEM_RESULT_NOT_MATCH != rc) &&
        (RC::ENCRYPT_DECRYPT_FAILED != rc) &&
        (Tee::GetCirlwlarSink()))
    {
        // Unexpected error, the GPU might have hit some exception or hung.
        // Dump the cirlwlar buffer now.
        Tee::GetCirlwlarSink()->Dump(Tee::FileOnly);
    }

    return rc;
}

//-----------------------------------------------------------------------------
/* virtual */
RC SecTest::Cleanup()
{
    StickyRC firstRc;
    LwRmPtr pLwRm;

    if (m_TaskerEventId)
    {
        Tasker::FreeEvent(m_TaskerEventId);
        m_TaskerEventId = 0;
    }
    if (m_hRmEvent)
    {
        pLwRm->Free(m_hRmEvent);
        m_hRmEvent = 0;
    }
    // Normally this is unnecessary since the object will be freed when the
    // channel is freed.
    // But in shared-channel mode, the channel doesn't really get freed at
    // the end of the test, and we would leak the object.
    m_Sec.Free();

    if (m_pCh)
    {
        if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
            firstRc = m_pCh->WriteSetSubdevice(Channel::AllSubdevicesMask);
        firstRc = m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = NULL;
    }

    delete m_pGGSurfs;
    m_pGGSurfs = NULL;
    m_pGolden->ClearSurfaces();

    FreeSurfaces();
    firstRc = GpuTest::Cleanup();

    return firstRc;
}

//---------------------------------------------------------------------
// Here's the main random drawing loop:
RC SecTest::DoRandomLoops()
{
    StickyRC rc;
    UINT32 StartLoop        = m_pTestConfig->StartLoop();
    UINT32 RestartSkipCount = m_pTestConfig->RestartSkipCount();
    UINT32 EndLoop          = StartLoop + m_pTestConfig->Loops();

    if ((StartLoop % RestartSkipCount) != 0)
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");

    for (m_pFpCtx->LoopNum = StartLoop; m_pFpCtx->LoopNum < EndLoop; ++m_pFpCtx->LoopNum)
    {
        if ((m_pFpCtx->LoopNum == StartLoop) || ((m_pFpCtx->LoopNum % RestartSkipCount) == 0))
        {
            // Beginning of a "frame".
            m_pFpCtx->Rand.SeedRandom(m_pTestConfig->Seed() + m_pFpCtx->LoopNum);
            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / RestartSkipCount;
            m_pPickers->Restart();
            FillSurfaces();
        }

        m_pGolden->SetLoop(m_pFpCtx->LoopNum);

        const RC localRc = DoOneLoop();
        rc = localRc;

        // Normally we stop at the first error.
        if (localRc)
        {
            Print();
            GetJsonExit()->AddFailLoop(m_pFpCtx->LoopNum);
            if (m_pGolden->GetStopOnError())
            {
                break;
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC SecTest::DoOneLoop()
{
    Printf(Tee::PriDebug, "SecTest::DoOneLoop %d\n", m_pFpCtx->LoopNum);
    RC rc;
    int i;

    // Make random picks.
    m_CopyAction        =      (*m_pPickers)[FPK_SEC_COPY_ACTION].Pick();
    m_SrcIsFb           = 0 != (*m_pPickers)[FPK_SEC_SRC_IS_FB].Pick();
    m_DstIsFb           = 0 != (*m_pPickers)[FPK_SEC_DST_IS_FB].Pick();
    m_BlockCount        =      (*m_pPickers)[FPK_SEC_BLOCK_COUNT].Pick();
    m_SrcBlockOffset    =      (*m_pPickers)[FPK_SEC_SRC_BLOCK_OFFSET].Pick();
    m_DstBlockOffset    =      (*m_pPickers)[FPK_SEC_DST_BLOCK_OFFSET].Pick();
    m_SeparateSemaphore = 0 != (*m_pPickers)[FPK_SEC_SEPARATE_SEMA].Pick();
    m_Awaken            = 0 != (*m_pPickers)[FPK_SEC_AWAKEN_SEMA].Pick();
    m_AppId             =      (*m_pPickers)[FPK_SEC_APP_ID].Pick();

    // Source and Destination offsets need to get 16 byte aligned
    m_SrcOffset = (16 * m_SrcBlockOffset) & ~0xf;
    m_DstOffset = (16 * m_DstBlockOffset) & ~0xf;
    m_ByteCount = m_BlockCount * 16;

    for (i = 0; i < 4; i++)
        m_ContentKey[i] = m_pFpCtx->Rand.GetRandom();
    for (i = 0; i < 4; i++)
        m_IV[i] = m_pFpCtx->Rand.GetRandom();

    // Encrypt the Content key with AES-128
    LW2080_CTRL_CMD_CIPHER_AES_ENCRYPT_PARAMS encrypt;
    memset(&encrypt, 0, sizeof(encrypt));
    memcpy(encrypt.pt, m_ContentKey, 16);
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetBoundGpuSubdevice(),
                                LW2080_CTRL_CMD_CIPHER_AES_ENCRYPT,
                                (void *)&encrypt,
                                sizeof(encrypt)));

    memcpy(m_EncryptedContentKey, encrypt.ct, 16);

    m_pSurfSrc = m_SrcIsFb ? m_pSurfFb: &m_SurfHost;
    m_SrcCtxDmaIdx = m_SrcIsFb ? FB_CTX_DMA_IDX : HOST_CTX_DMA_IDX;

    switch (m_CopyAction)
    {
        default:    // force a valid action
            m_CopyAction = FPK_SEC_COPY_ACTION_encrypt;
            /* fall through */

        case FPK_SEC_COPY_ACTION_encrypt:
        case FPK_SEC_COPY_ACTION_decrypt:
            // During the checking phase, we are going to check that the data
            // *mismatches* the source data, then we are going to *re-do* the copy
            // to a different place with the ilwerse action (en- vs. de-crypt).
            //
            // This should restore the original data, and we can check again, for
            // a *match* this time.
            m_pSurfDstCrypt =   m_DstIsFb  ? m_pSurfFb : &m_SurfHost;
            m_DstCryptCtxDmaIdx = m_DstIsFb ? FB_CTX_DMA_IDX : HOST_CTX_DMA_IDX;
            m_pSurfDstClear = (!m_DstIsFb) ? m_pSurfFb : &m_SurfHost;
            m_DstClearCtxDmaIdx = (!m_DstIsFb) ? FB_CTX_DMA_IDX : HOST_CTX_DMA_IDX;
            break;
    }

    UINT32 minSurfSize = (UINT32)m_pSurfFb->GetSize();
    if (minSurfSize > m_SurfHost.GetSize())
        minSurfSize = (UINT32)m_SurfHost.GetSize();

    if ((m_pSurfSrc == m_pSurfDstClear) || (m_pSurfSrc == m_pSurfDstCrypt))
    {
        // We will use two offsets in the same surface.
        //  - limit size to 1/2 surface size
        //  - adjust offsets to avoid writing past end of surface
        //  - adjust offsets to avoid reading & writing the same bytes

        if (m_ByteCount > minSurfSize / 2)
            m_ByteCount = (minSurfSize / 2);

        if (m_SrcOffset < m_DstOffset)
        {
            if (m_SrcOffset + 2*m_ByteCount > minSurfSize)
                m_SrcOffset = minSurfSize - 2*m_ByteCount;
            if (m_DstOffset + m_ByteCount > minSurfSize)
                m_DstOffset = minSurfSize - m_ByteCount;
            if (m_DstOffset < m_SrcOffset + m_ByteCount)
                m_DstOffset = m_SrcOffset + m_ByteCount;
        }
        else
        {
            if (m_DstOffset + 2*m_ByteCount > minSurfSize)
                m_DstOffset = minSurfSize - 2*m_ByteCount;
            if (m_SrcOffset + m_ByteCount > minSurfSize)
                m_SrcOffset = minSurfSize - m_ByteCount;
            if (m_SrcOffset < m_DstOffset + m_ByteCount)
                m_SrcOffset = m_DstOffset + m_ByteCount;
        }
    }
    else
    {
        // Copying between two different surfaces:
        //  - limit size to lesser of surface sizes
        //  - adjust src & dst offsets to avoid reading/writing past surfaces
        if (m_ByteCount > minSurfSize)
            m_ByteCount = minSurfSize;

        if (m_SrcOffset + m_ByteCount > minSurfSize)
            m_SrcOffset = minSurfSize - m_ByteCount;
        if (m_DstOffset + m_ByteCount > minSurfSize)
            m_DstOffset = minSurfSize - m_ByteCount;
    }

    m_BlockCount = m_ByteCount / 16;

    if (!m_AwakenAllowed)
    {
        // Some platforms don't allow mods to hook interrupts to events.
        m_Awaken = false;
    }

    if ((*m_pPickers)[FPK_SEC_SUPERVERBOSE].Pick())
        Print();

    // Send HW ops to channel.
    CHECK_RC(SendCopyMethods(m_pSurfSrc, m_SrcOffset,  m_pSurfDstCrypt));
    CHECK_RC(SendCopyMethods(m_pSurfDstCrypt, m_DstOffset, m_pSurfDstClear));

    // Flush channel, wait for semaphore release.
    CHECK_RC(m_pCh->Flush());
    CHECK_RC(WaitSemaphore());

    // Check results are as expected.
    CHECK_RC(CheckCopy(m_pSurfSrc,      m_SrcOffset, m_pSurfDstCrypt, ShouldBeDifferent));
    CHECK_RC(CheckCopy(m_pSurfDstCrypt, m_DstOffset, m_pSurfDstClear, ShouldBeDifferent));
    CHECK_RC(CheckCopy(m_pSurfSrc,      m_SrcOffset, m_pSurfDstClear, ShouldBeSame));

    return rc;
}

RC SecTest::SetupSEC()
{
    RC rc;
    LwRmPtr pLwRm;

    // Alloc HW objects:
    CHECK_RC(m_Sec.Alloc(m_hCh, GetBoundGpuDevice()));

    if (Platform::HasClientSideResman()  &&
        (GetBoundGpuDevice()->GetNumSubdevices() < 2))
    {
        // This platform allows us to hook an event to a GPU interrupt and
        // wait on it.
        m_AwakenAllowed = true;
        m_TaskerEventId = Tasker::AllocEvent("SEC", false);
        MASSERT(m_TaskerEventId);
        void* const pOsEvent = Tasker::GetOsEvent(
                m_TaskerEventId,
                pLwRm->GetClientHandle(),
                pLwRm->GetDeviceHandle(GetBoundGpuDevice()));
        CHECK_RC(pLwRm->AllocEvent(
                m_Sec.GetHandle(),
                &m_hRmEvent,
                LW01_EVENT_OS_EVENT,
                0,
                pOsEvent));
    }

    // This test is "sli-serial" meaning we will operate on just one GPU
    // of an SLI device.  Tell the (possible) other GPUs to ignore all
    // pushbuffer methods.
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        CHECK_RC(m_pCh->WriteSetSubdevice(m_SubdevMask));

    // Init the HW object:
    m_pCh->SetObject(SUBCHAN, m_Sec.GetHandle() );

    CHECK_RC(SetupSemaphore());

    switch(m_Sec.GetClass())
    {
        case LW95B1_VIDEO_MSVLD:
            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SEMAPHORE_D,
                        DRF_DEF(95B1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                        DRF_DEF(95B1, _SEMAPHORE_D, _OPERATION,      _RELEASE) |
                        DRF_DEF(95B1, _SEMAPHORE_D, _FLUSH_DISABLE,  _FALSE) |
                        DRF_DEF(95B1, _SEMAPHORE_D, _AWAKEN_ENABLE,  _FALSE)));
            break;
        default:
            MASSERT(!"Unknown class.");
            break;
    }

    CHECK_RC(m_pCh->Flush());
    m_Awaken = false;
    CHECK_RC(WaitSemaphore());

    // Get the Session Key
    LW2080_CTRL_CMD_CIPHER_SESSION_KEY_PARAMS params;
    memset(&params, 0, sizeof(params));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(GetBoundGpuSubdevice(),
                                           LW2080_CTRL_CMD_CIPHER_SESSION_KEY,
                                           (void *)&params,
                                           sizeof(params)));
    memcpy(m_SessionKey, params.sKey, 16);

    return rc;
}

//-----------------------------------------------------------------------------
void SecTest::PrintCopyAction
(
    Surface2D * pSrc,
    UINT32      srcOffset,
    Surface2D * pDst,
    const char * opStr

) const
{
    if (!pSrc || !pDst || !opStr)
        return;

    const char * surfLocNames[] =
    {
        "Fb",
        "Coh",
        "NonCoh"
    };

    Printf(Tee::PriNormal, " %s %d blocks from %s surf+0x%08x to %s surf+0x%08x\n",
            opStr,
            m_BlockCount,
            surfLocNames[pSrc->GetLocation()],
            srcOffset,
            surfLocNames[pDst->GetLocation()],
            m_DstOffset);
}

void SecTest::Print() const
{
    const char * modestr = "";
    const char * padstr = "";

    switch (m_AppId)
    {
        case FPK_SEC_APP_ID_ctr64:           modestr = " ctr64"; break;
    }

    Printf(Tee::PriNormal, "loop %d Sem:%s%s%s%s\n",
            m_pFpCtx->LoopNum,
            m_SeparateSemaphore ? "separate" : "on_dma",
            m_Awaken ? "+awaken" : "",
            modestr,
            padstr
            );
    switch (m_CopyAction)
    {
        case FPK_SEC_COPY_ACTION_encrypt:
            PrintCopyAction(m_pSurfSrc, m_SrcOffset,  m_pSurfDstCrypt, "ENCRYPT");
            PrintCopyAction(m_pSurfDstCrypt, m_DstOffset, m_pSurfDstClear, "DECRYPT");
            break;

        case FPK_SEC_COPY_ACTION_decrypt:
            PrintCopyAction(m_pSurfSrc, m_SrcOffset,  m_pSurfDstCrypt, "DECRYPT");
            PrintCopyAction(m_pSurfDstCrypt, m_DstOffset, m_pSurfDstClear, "ENCRYPT");
            break;
    }
}

//-----------------------------------------------------------------------------
RC SecTest::SetDefaultPickers(UINT32 first, UINT32 last)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();

    switch (first)
    {
        case FPK_SEC_BLOCK_COUNT:
            if (static_cast<int>(last) < FPK_SEC_BLOCK_COUNT)
                break;
            (*pPickers)[FPK_SEC_BLOCK_COUNT].ConfigRandom();
            (*pPickers)[FPK_SEC_BLOCK_COUNT].AddRandRange(100,      1, 1024/16);
            (*pPickers)[FPK_SEC_BLOCK_COUNT].AddRandRange( 10,   1024/16, 1024);
            (*pPickers)[FPK_SEC_BLOCK_COUNT].AddRandRange( 1, 1024, 1024*1024/16);
            (*pPickers)[FPK_SEC_BLOCK_COUNT].CompileRandom();

        case FPK_SEC_SRC_BLOCK_OFFSET:
            if (last < FPK_SEC_SRC_BLOCK_OFFSET)
                break;
            (*pPickers)[FPK_SEC_SRC_BLOCK_OFFSET].ConfigRandom();
            (*pPickers)[FPK_SEC_SRC_BLOCK_OFFSET].AddRandRange(1,1, 3*1024*1024/16);
            (*pPickers)[FPK_SEC_SRC_BLOCK_OFFSET].CompileRandom();

        case FPK_SEC_DST_BLOCK_OFFSET:
            if (last < FPK_SEC_DST_BLOCK_OFFSET)
                break;
            (*pPickers)[FPK_SEC_DST_BLOCK_OFFSET].ConfigRandom();
            (*pPickers)[FPK_SEC_DST_BLOCK_OFFSET].AddRandRange(1,1, 3*1024*1024/16);
            (*pPickers)[FPK_SEC_DST_BLOCK_OFFSET].CompileRandom();

        case FPK_SEC_SRC_IS_FB:
            if (last < FPK_SEC_SRC_IS_FB)
                break;
            (*pPickers)[FPK_SEC_SRC_IS_FB].ConfigRandom();
            (*pPickers)[FPK_SEC_SRC_IS_FB].AddRandItem(1,0);
            (*pPickers)[FPK_SEC_SRC_IS_FB].AddRandItem(1,1);
            (*pPickers)[FPK_SEC_SRC_IS_FB].CompileRandom();

        case FPK_SEC_DST_IS_FB:
            if (last < FPK_SEC_DST_IS_FB)
                break;
            (*pPickers)[FPK_SEC_DST_IS_FB].ConfigRandom();
            (*pPickers)[FPK_SEC_DST_IS_FB].AddRandItem(1,0);
            (*pPickers)[FPK_SEC_DST_IS_FB].AddRandItem(1,1);
            (*pPickers)[FPK_SEC_DST_IS_FB].CompileRandom();

        case FPK_SEC_COPY_ACTION:
            if (last < FPK_SEC_COPY_ACTION)
                break;
            (*pPickers)[FPK_SEC_COPY_ACTION].ConfigRandom();
            (*pPickers)[FPK_SEC_COPY_ACTION].AddRandItem(1,FPK_SEC_COPY_ACTION_encrypt);
            (*pPickers)[FPK_SEC_COPY_ACTION].AddRandItem(1,FPK_SEC_COPY_ACTION_decrypt);
            (*pPickers)[FPK_SEC_COPY_ACTION].CompileRandom();

        case FPK_SEC_SEPARATE_SEMA:
            if (last < FPK_SEC_SEPARATE_SEMA)
                break;
            (*pPickers)[FPK_SEC_SEPARATE_SEMA].ConfigRandom();
            (*pPickers)[FPK_SEC_SEPARATE_SEMA].AddRandRange(1, 0, 1);
            (*pPickers)[FPK_SEC_SEPARATE_SEMA].CompileRandom();

        case FPK_SEC_AWAKEN_SEMA:
            if (last < FPK_SEC_AWAKEN_SEMA)
                break;
            (*pPickers)[FPK_SEC_AWAKEN_SEMA].ConfigRandom();
            (*pPickers)[FPK_SEC_AWAKEN_SEMA].AddRandRange(1,0,1);
            (*pPickers)[FPK_SEC_AWAKEN_SEMA].CompileRandom();

        case FPK_SEC_SUPERVERBOSE:
            if (last < FPK_SEC_SUPERVERBOSE)
                break;
            (*pPickers)[FPK_SEC_SUPERVERBOSE].ConfigConst(0);

        case FPK_SEC_APP_ID:
            if (last < FPK_SEC_APP_ID)
                break;
            (*pPickers)[FPK_SEC_APP_ID].ConfigRandom();
            (*pPickers)[FPK_SEC_APP_ID].AddRandItem(1,FPK_SEC_APP_ID_ctr64);
            (*pPickers)[FPK_SEC_APP_ID].CompileRandom();
    }
    return OK;
}

//-----------------------------------------------------------------------------
RC SecTest::AllocSurfaces()
{
    RC rc;

    Printf(Tee::PriDebug, "SecTest::AllocSurfaces SEMAPHORE\n");
    m_SurfSemaphore.SetWidth(16);
    m_SurfSemaphore.SetHeight(1);
    m_SurfSemaphore.SetColorFormat(ColorUtils::VOID32);
    m_SurfSemaphore.SetLocation(m_pTestConfig->NotifierLocation());
    m_SurfSemaphore.SetProtect(Memory::ReadWrite);
    m_SurfSemaphore.SetLayout(Surface2D::Pitch);
    m_SurfSemaphore.SetDisplayable(false);
    m_SurfSemaphore.SetType(LWOS32_TYPE_IMAGE);
    m_SurfSemaphore.SetAddressModel(Memory::Paging);
    CHECK_RC(m_SurfSemaphore.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SurfSemaphore.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_SurfSemaphore.BindGpuChannel(m_hCh));

    Printf(Tee::PriDebug, "SecTest::AllocSurfaces FB\n");
    m_pSurfFb = GetDisplayMgrTestContext()->GetSurface2D();
    CHECK_RC(m_pSurfFb->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_pSurfFb->BindGpuChannel(m_hCh));

    if (m_pSurfFb->GetSize() > 0xFFFFFFFF)
        return RC::DATA_TOO_LARGE;

    Printf(Tee::PriDebug, "SecTest::AllocSurfaces HOST\n");
    m_SurfHost.SetWidth(m_pTestConfig->SurfaceWidth());
    m_SurfHost.SetHeight(m_pTestConfig->SurfaceHeight());
    m_SurfHost.SetColorFormat(ColorUtils::ColorDepthToFormat(m_pTestConfig->DisplayDepth()));
    m_SurfHost.SetLocation(Memory::NonCoherent);
    m_SurfHost.SetProtect(Memory::ReadWrite);
    m_SurfHost.SetLayout(Surface2D::Pitch);
    m_SurfHost.SetDisplayable(false);
    m_SurfHost.SetType(LWOS32_TYPE_IMAGE);
    m_SurfHost.SetAddressModel(Memory::Paging);
    CHECK_RC(m_SurfHost.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SurfHost.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_SurfHost.BindGpuChannel(m_hCh));

    if (m_SurfHost.GetSize() > 0xFFFFFFFF)
        return RC::DATA_TOO_LARGE;

    return rc;
}

//-----------------------------------------------------------------------------
void SecTest::FillSurfaces()
{
    Printf(Tee::PriDebug, "SecTest::FillSurfaces\n");

    MASSERT(m_pSurfFb->GetAddress());     // This should have been mapped already.
    MASSERT(m_SurfHost.GetAddress());   // This should have been mapped already.

    // Fill each 32-bit word of each surface with the !lsb32 of its virtual addr.
    for (int i = 0; i < 2; i++)
    {
        Surface2D * pSurf;
        if (i == 0)
            pSurf = m_pSurfFb;
        else
            pSurf = &m_SurfHost;

        UINT32 * p = (UINT32 *)(pSurf->GetAddress());
        UINT32 * pEnd = p + (pSurf->GetSize() / sizeof(UINT32));

        while (p < pEnd)
        {
            MEM_WR32(p, ~UINT32(LwUPtr(p)));
            p++;
        }
    }
}

//-----------------------------------------------------------------------------
void SecTest::FreeSurfaces()
{
    m_SurfHost.Free();
    m_SurfSemaphore.Free();
}

//-----------------------------------------------------------------------------
RC SecTest::SendCopyMethods
(
    Surface2D * pSrc,
    UINT32 srcOffset,
    Surface2D * pDst
)
{
    Printf(Tee::PriDebug, "SecTest::SendCopyMethods\n");
    bool dstInClear = (pDst == m_pSurfDstClear);
    UINT32 i;
    RC rc;

    // This test is "sli-serial" meaning we will operate on just one GPU
    // of an SLI device.  Tell the (possible) other GPUs to ignore all
    // pushbuffer methods.
    if (GetBoundGpuDevice()->GetNumSubdevices() > 1)
        CHECK_RC(m_pCh->WriteSetSubdevice(m_SubdevMask));

    switch (m_AppId)
    {
        default:
            // Fall through to CTR64
            m_AppId = FPK_SEC_APP_ID_ctr64;

        case FPK_SEC_APP_ID_ctr64:
            switch(m_Sec.GetClass())
            {
                case LW95B1_VIDEO_MSVLD:
                    CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SET_APPLICATION_ID,
                              DRF_DEF(95B1, _SET_APPLICATION_ID, _ID, _CTR64)));
                    break;
                default:
                    MASSERT(!"Unknown class.");
                    break;
            }
            break;
    }

    for (i = 0; i < 4; ++i)
    {
        switch(m_Sec.GetClass())
        {
            case LW95B1_VIDEO_MSVLD:
                CHECK_RC(m_pCh->Write(SUBCHAN,
                                      LW95B1_SET_SESSION_KEY(i),
                                      m_SessionKey[i]));
                break;
            default:
                MASSERT(!"Unknown class.");
                break;
        }
    }

    UINT32 exelwteData = 0;

    UINT64 srcHwOff = pSrc->GetCtxDmaOffsetGpu() + srcOffset;
    UINT64 dstHwOff = pDst->GetCtxDmaOffsetGpu() + m_DstOffset;

    switch(m_Sec.GetClass())
    {
        case LW95B1_VIDEO_MSVLD:
            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SET_BLOCK_COUNT,
                        DRF_NUM(95B1, _SET_BLOCK_COUNT, _VALUE, m_BlockCount)));

            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SET_UPPER_SRC,
                                    (0xff & UINT32(srcHwOff >> 32))));

            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SET_LOWER_SRC,
                                    UINT32(srcHwOff & 0xffffffff)));

            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SET_UPPER_DST,
                                    (0xff & UINT32(dstHwOff >> 32))));

            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SET_LOWER_DST,
                                    UINT32(dstHwOff & 0xffffffff)));
            break;
        default:
            MASSERT(!"Unknown class.");
            break;
    }

    for (i = 0; i < 4; ++i)
    {
        switch(m_Sec.GetClass())
        {
            case LW95B1_VIDEO_MSVLD:
                CHECK_RC(m_pCh->Write(SUBCHAN,
                                      LW95B1_SET_CONTENT_KEY(i),
                                      m_EncryptedContentKey[i]));
                break;
            default:
                MASSERT(!"Unknown class.");
                break;
        }
    }

    for (i = 0; i < 4; ++i)
    {
        switch(m_Sec.GetClass())
        {
            case LW95B1_VIDEO_MSVLD:
                CHECK_RC(m_pCh->Write(SUBCHAN,
                                      LW95B1_SET_CONTENT_INITIAL_VECTOR(i),
                                      m_IV[i]));
                break;
            default:
                MASSERT(!"Unknown class.");
                break;
        }
    }

    if (dstInClear)
    {
        // This is a passthru copy or the second half of a crypt/decrypt pair.
        CHECK_RC(SetupSemaphore());

        if (m_SeparateSemaphore)
        {
            // Since we're using a backend semaphore, we don't need the
            // exelwtion to tell us when it's finished transferring
            switch(m_Sec.GetClass())
            {
                case LW95B1_VIDEO_MSVLD:
                    exelwteData = DRF_DEF(95B1, _EXELWTE, _NOTIFY, _DISABLE);
                    exelwteData |= DRF_DEF(95B1, _EXELWTE, _NOTIFY_ON, _END);
                    exelwteData |= DRF_DEF(95B1, _EXELWTE, _AWAKEN, _DISABLE);
                    break;
                default:
                    MASSERT(!"Unknown class.");
                    break;
            }
        }
        else
        {
            // There is no backend semaphore, so the exelwtion needs to
            // inform us when the transfer is complete
            switch(m_Sec.GetClass())
            {
                case LW95B1_VIDEO_MSVLD:
                    exelwteData = DRF_DEF(95B1, _EXELWTE, _NOTIFY,    _ENABLE);
                    exelwteData |= DRF_DEF(95B1, _EXELWTE, _NOTIFY_ON, _END);
                    if (m_Awaken) // Use interrupt
                        exelwteData |= DRF_DEF(95B1, _EXELWTE, _AWAKEN, _ENABLE);
                    else // poll for the values
                        exelwteData |= DRF_DEF(95B1, _EXELWTE, _AWAKEN, _DISABLE);
                    break;
                default:
                    MASSERT(!"Unknown class.");
                    break;
            }
        }

    }

    switch(m_Sec.GetClass())
    {
        case LW95B1_VIDEO_MSVLD:
            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_EXELWTE, exelwteData));
            break;
        default:
            MASSERT(!"Unknown class.");
            break;
    }

    if ((dstInClear) && m_SeparateSemaphore)
    {
        // This is the last copy of this loop, and there was no semaphore
        // release in the copy trigger, so send a separate semaphore method.
        if (m_Awaken)
        {
            switch(m_Sec.GetClass())
            {
                case LW95B1_VIDEO_MSVLD:
                    CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SEMAPHORE_D,
                                DRF_DEF(95B1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                                DRF_DEF(95B1, _SEMAPHORE_D, _OPERATION,      _RELEASE) |
                                DRF_DEF(95B1, _SEMAPHORE_D, _FLUSH_DISABLE,  _FALSE) |
                                DRF_DEF(95B1, _SEMAPHORE_D, _AWAKEN_ENABLE,  _TRUE)));
                    break;
                default:
                    MASSERT(!"Unknown class.");
                    break;
            }
        }
        else
        {
            switch(m_Sec.GetClass())
            {
                case LW95B1_VIDEO_MSVLD:
                    CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SEMAPHORE_D,
                                DRF_DEF(95B1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                                DRF_DEF(95B1, _SEMAPHORE_D, _OPERATION,      _RELEASE) |
                                DRF_DEF(95B1, _SEMAPHORE_D, _FLUSH_DISABLE,  _FALSE) |
                                DRF_DEF(95B1, _SEMAPHORE_D, _AWAKEN_ENABLE,  _FALSE)));
                    break;
                default:
                    MASSERT(!"Unknown class.");
                    break;
            }
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
bool SecTest::SemaphoreIsWritten()
{
    UINT32 val = MEM_RD32(m_SurfSemaphore.GetAddress());

    return (val == SEM_DONE_VAL + m_pFpCtx->LoopNum);
}

static bool PollFunc(void * pvSecTest )
{
   SecTest * pSecTest = (SecTest *)pvSecTest;
   return pSecTest->SemaphoreIsWritten();
}

//! \brief SetupSemaphore sets up all the values needed to use the semaphore
//!
//! \note This has to be done each time we set the App ID
RC SecTest::SetupSemaphore()
{
    RC rc;

    // Bug 411840
    // Make sure to clear out the semaphore before checking it
    MEM_WR32(m_SurfSemaphore.GetAddress(), 0);

    UINT64 semOff = m_SurfSemaphore.GetCtxDmaOffsetGpu();

    switch(m_Sec.GetClass())
    {
        case LW95B1_VIDEO_MSVLD:
            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SEMAPHORE_A,
                                    (0xff & UINT32(semOff >> 32))));

            CHECK_RC(m_pCh->Write(SUBCHAN, LW95B1_SEMAPHORE_B,
                                    UINT32(semOff & 0xffffffff)));

            CHECK_RC(m_pCh->Write(SUBCHAN,
                      LW95B1_SEMAPHORE_C, SEM_DONE_VAL + m_pFpCtx->LoopNum));
            break;
        default:
            MASSERT(!"Unknown class.");
            break;
    }

    return rc;
}

RC SecTest::WaitSemaphore()
{
    Printf(Tee::PriDebug, "SecTest::WaitSemaphore\n");
    RC rc;
    FLOAT64 timeoutMs = m_pTestConfig->TimeoutMs();

    if (m_Awaken)
    {
        rc = Tasker::WaitOnEvent(m_TaskerEventId,
                                 Tasker::FixTimeout(timeoutMs));
    }
    else
    {
        rc = POLLWRAP(&PollFunc, this, timeoutMs);
    }

    if (RC::TIMEOUT_ERROR == rc)
        rc = RC::NOTIFIER_TIMEOUT;
    else if (OK == rc)
        rc = m_pCh->CheckError();

    return rc;
}

//-----------------------------------------------------------------------------
RC SecTest::CheckCopy
(
    Surface2D * pSrc,
    UINT32 SrcOffset,
    Surface2D * pDst,
    CompareType cmp
)
{
    Printf(Tee::PriDebug, "SecTest::CheckCopy\n");
    RC rc;
    UINT32 NumMismatchingBytes = 0;
    UINT32 LongestMatchingSequence = 0;
    UINT32 LwrrentMatchingSequence = 0;

    const volatile UINT08 * pSrcData =
            SrcOffset + (const volatile UINT08*)(pSrc->GetAddress());
    const volatile UINT08 * pDstData =
            m_DstOffset + (const volatile UINT08*)(pDst->GetAddress());

    // Performance note: doing 1-byte reads over the PCI-express bus is
    // horribly slow, so we will use a small local cache to read bigger chunks.
    const UINT32 cacheSz = 128;
    UINT08 srcCache[cacheSz];
    UINT08 dstCache[cacheSz];

    for (UINT32 bytesRead = 0; bytesRead < m_BlockCount * 16; /**/)
    {
        UINT32 bytesThisTime = cacheSz;
        if (bytesThisTime > (m_BlockCount * 16) - bytesRead)
            bytesThisTime = (m_BlockCount * 16) - bytesRead;

        Platform::MemCopy(srcCache, pSrcData + bytesRead, bytesThisTime);
        Platform::MemCopy(dstCache, pDstData + bytesRead, bytesThisTime);

        bytesRead += bytesThisTime;

        for (UINT32 i = 0; i < bytesThisTime; i++)
        {
            if (srcCache[i] == dstCache[i])
            {
                LwrrentMatchingSequence++;
            }
            else
            {
                NumMismatchingBytes++;
                if (LwrrentMatchingSequence > LongestMatchingSequence)
                    LongestMatchingSequence = LwrrentMatchingSequence;
                LwrrentMatchingSequence = 0;
            }
        }
    }
    if (LwrrentMatchingSequence > LongestMatchingSequence)
        LongestMatchingSequence = LwrrentMatchingSequence;

    if (ShouldBeSame == cmp)
    {
        if (NumMismatchingBytes)
        {
            rc = RC::MEM_TO_MEM_RESULT_NOT_MATCH;
        }
    }
    else // ShouldBeDifferent
    {
        // Must do a fuzzy compare here!
        //
        // The likelihood of a random accidental matching byte subsequence of
        // length M is about 1 in pow(2, 8*M).
        //
        // Declare a failure to encrypt if the likelihood of an accidental
        // match is less than 1 in pow(2, 32).
        //
        // In practice, when copying 256 bytes or less, a matching subsequence
        // of 5 bytes will be allowed, 6 will fail.
        // For 64k bytes copied, we will allow a matching subsequence of 6
        // and fail for 7.
        // A matching subsequence of 8 bytes will always fail, since we won't
        // ever copy 4 billion bytes.
        //

        if ((LongestMatchingSequence >= 8) ||
            ((LongestMatchingSequence > 4) &&
             (UINT32(1)<<(8*(LongestMatchingSequence-4)) > (m_BlockCount * 16))))
        {
            rc = RC::ENCRYPT_DECRYPT_FAILED;
        }
    }
    if (rc)
    {
        Printf(Tee::PriLow,
                "SecTest::CheckCopy FAILED at loop %d %s->%s %s\n",
                m_pFpCtx->LoopNum,
                Memory::GetMemoryLocationName(pSrc->GetLocation()),
                Memory::GetMemoryLocationName(pDst->GetLocation()),
                cmp == ShouldBeSame ? "ShouldBeSame" : "ShouldBeDifferent");
        Printf(Tee::PriLow,
                " total=%d different=%d longest matching num-bytes=%d\n",
                (m_BlockCount * 16),
                NumMismatchingBytes,
                LongestMatchingSequence);
    }
    return rc;
}
