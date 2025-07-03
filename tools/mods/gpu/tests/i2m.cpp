/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file i2m.cpp
 * @brief: Test for class KEPLER_INLINE_TO_MEMORY_A and newer
 */

#include "core/include/channel.h"
#include "gpu/utility/blocklin.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "gpu/include/gralloc.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surffill.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include "class/cla040.h" // KEPLER_INLINE_TO_MEMORY_A
#include "class/cla06fsubch.h"

class I2MTest : public GpuTest
{
public:
    I2MTest();
    ~I2MTest();

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();
    virtual bool IsSupported();
    virtual bool IsSupportedVf();

    SETGET_PROP(SaveSurfaces, bool);

private:
    enum
    {
        s_I2M = LWA06F_SUBCHANNEL_I2M,
    };
    struct TransferParams
    {
        enum Destination
        {
            Pitch,
            BlockLinear,
            Sys
        };

        Destination Dest;
        UINT32      LineLengthIn;
        UINT32      LineCount;
        UINT32      OffsetOut;
        UINT32      PitchOut;
        UINT32      DstWidth;
        UINT32      DstHeight;
        UINT32      DstOriginBytesX;
        UINT32      DstOriginSamplesY;
        bool        SysMemBarDisabled;
        bool        BigSemaphore;
        bool        ReductionEnabled;
        bool        ReductionFormatSigned;
        UINT32      ReductionOp;
        UINT32      SemaphorePayload;
        UINT32      InputOffset;
    };

    bool                        m_SaveSurfaces;
    GpuTestConfiguration       *m_pTestConfig;
    Random                     *m_pRandom;

    Channel                    *m_pCh;
    LwRm::Handle                m_hCh;

    Inline2MemoryAlloc          m_I2MAlloc;

    Surface2D                  *m_pDisplaySurf;
    Surface2D                   m_BLSurf;

    Surface2D                   m_SysSurf;
    Surface2D                   m_FBSemaphoreSurf;
    Surface2D                   m_SysSemaphoreSurf;
    vector<UINT08>              m_RandomValues;
    SurfaceUtils::FillMethod    m_FillMethod;
    DmaWrapper                  m_DmaWrap;

    RC AllocateSurfaces();
    RC PickTransferParameters(TransferParams *TP);
    RC PrintTransferParameters(UINT32 Loop, const TransferParams *TP);
    static UINT32 GetExpectedSemaphoreValue(const UINT32 oldValue, const TransferParams& tp);
    RC LoopTransfers();
    RC VerifyTransfer(UINT32 LoopIdx, const TransferParams *TP);
    RC SaveDestinationSurface(UINT32 LoopIdx, TransferParams::Destination Des);

    RC SurfaceCreateAndDmaCopy(Surface2D* pSrcSurf, Surface2D *pDstSurf);
};
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(I2MTest, GpuTest, "Inline To Memory class test");

CLASS_PROP_READWRITE(I2MTest, SaveSurfaces, bool,
                     "Save complete content of all destination "
                     "surfaces for each loop");

//-----------------------------------------------------------------------------
I2MTest::I2MTest()
: m_SaveSurfaces(false)
, m_pCh(0)
, m_hCh(0)
, m_pDisplaySurf(0)
, m_FillMethod(SurfaceUtils::FillMethod::Any)
{
    m_pTestConfig = GetTestConfiguration();
    m_pRandom = &(GetFpContext()->Rand);
}

//-----------------------------------------------------------------------------
I2MTest::~I2MTest()
{
}

//-----------------------------------------------------------------------------
RC I2MTest::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplayAndSurface(false));
    CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                      &m_hCh,
                                                      &m_I2MAlloc));
    CHECK_RC(m_pCh->SetObject(s_I2M, m_I2MAlloc.GetHandle()));
    CHECK_RC(AllocateSurfaces());

    // When CPU FB Mappings are through Lwlink, CPU mapped-writes on block-linear surfaces is not possible.
    // Hence, choosing the fillMethod as Gpu based fill by default. When the surface is not block-linear,
    // the surface fills can be using CPU/GPU and this selection is done in SurfaceUtils::FillSurface
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_CPU_FB_MAPPINGS_THROUGH_LWLINK))
    {
        m_FillMethod = SurfaceUtils::FillMethod::Gpufill;
        CHECK_RC(m_DmaWrap.Initialize(
            GetTestConfiguration(),
            m_TestConfig.NotifierLocation()));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC I2MTest::AllocateSurfaces()
{
    RC rc;

    m_pDisplaySurf = GetDisplayMgrTestContext()->GetSurface2D();
    CHECK_RC(m_pDisplaySurf->Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_pDisplaySurf->BindGpuChannel(m_hCh));

    UINT32 Width = m_pDisplaySurf->GetWidth();
    UINT32 Height = m_pDisplaySurf->GetHeight();
    ColorUtils::Format ColorFormat = m_pDisplaySurf->GetColorFormat();

    m_BLSurf.SetWidth(Width);
    m_BLSurf.SetHeight(Height);
    m_BLSurf.SetLayout(Surface2D::BlockLinear);
    m_BLSurf.SetColorFormat(ColorFormat);
    CHECK_RC(m_BLSurf.Alloc(GetBoundGpuDevice()));

    if (m_FillMethod != SurfaceUtils::FillMethod::Gpufill)
    {
        CHECK_RC(m_BLSurf.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }

    CHECK_RC(m_BLSurf.BindGpuChannel(m_hCh));

    m_SysSurf.SetWidth(Width);
    m_SysSurf.SetHeight(Height);
    m_SysSurf.SetLocation(Memory::Coherent);
    m_SysSurf.SetColorFormat(ColorFormat);
    CHECK_RC(m_SysSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SysSurf.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_SysSurf.BindGpuChannel(m_hCh));

    m_FBSemaphoreSurf.SetWidth(16/4);
    m_FBSemaphoreSurf.SetHeight(1);
    m_FBSemaphoreSurf.SetColorFormat(ColorUtils::VOID32);
    CHECK_RC(m_FBSemaphoreSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_FBSemaphoreSurf.Map(
        GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_FBSemaphoreSurf.BindGpuChannel(m_hCh));

    m_SysSemaphoreSurf.SetWidth(16/4);
    m_SysSemaphoreSurf.SetHeight(1);
    m_SysSemaphoreSurf.SetLocation(Memory::Coherent);
    m_SysSemaphoreSurf.SetColorFormat(ColorUtils::VOID32);
    CHECK_RC(m_SysSemaphoreSurf.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_SysSemaphoreSurf.Map(
        GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_SysSemaphoreSurf.BindGpuChannel(m_hCh));

    return OK;
}

//-----------------------------------------------------------------------------
RC I2MTest::Run()
{
    RC rc;

    UINT32 Width  = m_pDisplaySurf->GetWidth();
    UINT32 Height = m_pDisplaySurf->GetHeight();
    UINT32 Depth  = 8*ColorUtils::PixelBytes(m_pDisplaySurf->GetColorFormat());
    UINT32 Pitch  = m_pDisplaySurf->GetPitch();

    CHECK_RC(Memory::FillConstant(m_pDisplaySurf->GetAddress(), 0,
             Width, Height, Depth, Pitch));

    CHECK_RC(SurfaceUtils::FillSurface(&m_BLSurf, 0, m_BLSurf.GetBitsPerPixel(),
                                            GetBoundGpuSubdevice(), m_FillMethod));

    CHECK_RC(Memory::FillConstant(m_SysSurf.GetAddress(), 0,
             Width, Height, Depth, m_SysSurf.GetPitch()));

    CHECK_RC(m_FBSemaphoreSurf.Fill(0,
             GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_SysSemaphoreSurf.Fill(0,
             GetBoundGpuSubdevice()->GetSubdeviceInst()));

    m_pRandom->SeedRandom(m_pTestConfig->Seed());
    const UINT32 NumRandomValues = 1024;
    m_RandomValues.resize(NumRandomValues);
    for (UINT32 ValueIdx = 0; ValueIdx < (NumRandomValues/4); ValueIdx++)
        *(UINT32*)(&m_RandomValues[4*ValueIdx]) = m_pRandom->GetRandom();

    CHECK_RC(LoopTransfers());

    return rc;
}

//-----------------------------------------------------------------------------
RC I2MTest::PickTransferParameters(TransferParams *TP)
{
    RC rc;

    memset(TP, 0, sizeof(TransferParams));

    UINT32 Width = m_pDisplaySurf->GetWidth();
    UINT32 Height = m_pDisplaySurf->GetHeight();
    UINT32 BPPixel = m_pDisplaySurf->GetBytesPerPixel();

    UINT32 MaxLineLength = Width;
    if (MaxLineLength > m_RandomValues.size())
        MaxLineLength = (UINT32)m_RandomValues.size();

    UINT32 MaxTransferSize = BPPixel * Width * Height;
    if (MaxTransferSize > m_RandomValues.size())
        MaxTransferSize = (UINT32)m_RandomValues.size();

    TP->Dest = (TransferParams::Destination)
        m_pRandom->GetRandom(TransferParams::Pitch, TransferParams::Sys);
    TP->LineLengthIn = m_pRandom->GetRandom(4, MaxLineLength);
    // LWA040_LOAD_INLINE_DATA accepts input in DWORDs, so align the line length on 4 bytes
    TP->LineLengthIn &= ~3U;

    UINT32 MaxLineCount = MaxTransferSize/TP->LineLengthIn;
    if (MaxLineCount > Height)
        MaxLineCount = Height;

    TP->LineCount = m_pRandom->GetRandom(1, MaxLineCount);

    UINT32 TransferSize = TP->LineLengthIn * TP->LineCount;

    TP->InputOffset = m_pRandom->GetRandom(0, MaxTransferSize - TransferSize);

    TP->DstWidth = BPPixel*Width;
    TP->DstHeight = Height;
    TP->SysMemBarDisabled =
        ((m_pRandom->GetRandom() & 0xf) == 0) ? true : false;
    TP->BigSemaphore = (m_pRandom->GetRandom() & 1) ? true : false;
    TP->SemaphorePayload = m_pRandom->GetRandom();
    TP->ReductionEnabled = false;
    TP->ReductionFormatSigned = false;
    TP->ReductionOp = 0;

    // Reduction is only allowed for COMPLETION_TYPE=RELEASE_SEMAPHORE,
    // which we set if SysMemBarDisabled is false.
    const bool reductionEnabled = !TP->SysMemBarDisabled &&
        (m_pRandom->GetRandom() & 1) != 0;

    if (reductionEnabled)
    {
        TP->ReductionEnabled = true;
        TP->ReductionOp = m_pRandom->GetRandom() &
            DRF_MASK(LWA040_LAUNCH_DMA_REDUCTION_OP);
        if (TP->ReductionOp != LWA040_LAUNCH_DMA_REDUCTION_OP_RED_INC &&
            TP->ReductionOp != LWA040_LAUNCH_DMA_REDUCTION_OP_RED_DEC)
        {
            TP->ReductionFormatSigned =
                (m_pRandom->GetRandom() & 1) ? true : false;
        }
    }

    if (TP->Dest == TransferParams::BlockLinear)
    {
        UINT32 AllocSize = (UINT32)m_BLSurf.GetSize();

        UINT32 MaxOriginBytesX = Width - TP->LineLengthIn;
        if (MaxOriginBytesX > 1<<20)
            MaxOriginBytesX = 1<<20;
        TP->DstOriginBytesX = m_pRandom->GetRandom(0, MaxOriginBytesX);

        UINT32 MaxOriginSamplesY = Height - TP->LineCount;
        if (MaxOriginSamplesY > 1<<16)
            MaxOriginSamplesY = 1<<16;
        TP->DstOriginSamplesY = m_pRandom->GetRandom(0, MaxOriginSamplesY);

        unique_ptr<BlockLinear> pBLAddressing;
        BlockLinear::Create(&pBLAddressing,
                            TP->DstWidth, TP->DstHeight,
                            1, // depth
                            m_BLSurf.GetLogBlockWidth(),
                            m_BLSurf.GetLogBlockHeight(),
                            m_BLSurf.GetLogBlockDepth(),
                            1, // Bpp
                            GetBoundGpuDevice(),
                            BlockLinear::SelectBlockLinearLayout(
                                m_BLSurf.GetPteKind(), GetBoundGpuDevice()));

        UINT32 EndOfTransferOffsetWithoutOffset = UNSIGNED_CAST(UINT32,
            pBLAddressing->Address(TP->LineLengthIn + TP->DstOriginBytesX,
                        TP->LineCount + TP->DstOriginSamplesY, 0));

        if (EndOfTransferOffsetWithoutOffset > AllocSize)
        {
            TP->OffsetOut = 0;
        }
        else
        {
            TP->OffsetOut =
                m_pRandom->GetRandom(0,
                    AllocSize - EndOfTransferOffsetWithoutOffset);
            TP->OffsetOut &= ~0x1ff; // Has to be multiple of 512
        }

        MASSERT(TP->OffsetOut < AllocSize);
    }
    else
    {
        UINT32 Pitch = 0;
        UINT32 AllocSize = 0;
        switch (TP->Dest)
        {
            case TransferParams::Pitch:
                Pitch = m_pDisplaySurf->GetPitch();
                AllocSize = (UINT32)m_pDisplaySurf->GetSize();
                break;
            case TransferParams::Sys:
                Pitch = m_SysSurf.GetPitch();
                AllocSize = (UINT32)m_SysSurf.GetSize();
                break;
            default:
                MASSERT(!"Unknown destination");
                return RC::SOFTWARE_ERROR;
        }

        TP->PitchOut = m_pRandom->GetRandom(TP->LineLengthIn, Pitch);

        TP->OffsetOut = m_pRandom->GetRandom(0,
            AllocSize - (TP->LineCount*Pitch));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC I2MTest::PrintTransferParameters(UINT32 Loop, const TransferParams *TP)
{
    const char *TransferDest = NULL;
    switch (TP->Dest)
    {
        case TransferParams::Pitch:
            TransferDest = "Pitch";
            break;
        case TransferParams::Sys:
            TransferDest = "Sys";
            break;
        case TransferParams::BlockLinear:
            TransferDest = "BL";
            break;
        default:
            MASSERT(!"Unknown destination");
            return RC::SOFTWARE_ERROR;
    }

    VerbosePrintf(
        "Loop %d: Transfer to %s, LineLength=%d, LineCount=%d, OffsetOut=%d, "
        "PitchOut=%d, DstWidth=%d, DstHeight=%d, DstOriginBytesX=%d, "
        "DstOriginSamplesY=%d, SysMemBarDisabled=%s, BigSemaphore=%s, "
        "Reduction=%s, ReductionFormatSigned=%s, ReductionOp=%d, "
        "SemaphorePayload=0x%08x, InputOffset=%d.\n",
        Loop,
        TransferDest,
        TP->LineLengthIn,
        TP->LineCount,
        TP->OffsetOut,
        TP->PitchOut,
        TP->DstWidth,
        TP->DstHeight,
        TP->DstOriginBytesX,
        TP->DstOriginSamplesY,
        TP->SysMemBarDisabled ? "true" : "false",
        TP->BigSemaphore ? "true" : "false",
        TP->ReductionEnabled ? "enabled" : "disabled",
        TP->ReductionFormatSigned ? "true" : "false",
        TP->ReductionOp,
        TP->SemaphorePayload,
        TP->InputOffset);

    return OK;
}

//-----------------------------------------------------------------------------
static RC SaveSurface
(
    Surface2D *Surface,
    const char *Filename
)
{
    if (Surface->GetSize() == 0)
        return RC::SOFTWARE_ERROR;

    FileHolder File(Filename, "wb");
    if (File.GetFile() == 0)
    {
        Printf(Tee::PriHigh, "Error: Unable to open file %s for write!\n",
            Filename);
        return RC::CANNOT_OPEN_FILE;
    }
    Surface2D::MappingSaver oldMapping(*Surface);
    if (!Surface->IsMapped() && OK != Surface->Map())
    {
        Printf(Tee::PriHigh,
            "Error: Unable to dump SurfaceName surface - unable to map"
            "the surface!\n");
        return RC::COULD_NOT_MAP_PHYSICAL_ADDRESS;
    }
    UINT32 Size = (UINT32)Surface->GetSize();
    UINT08 Buffer[4096];
    for (UINT32 Offset = 0; Offset < Size; Offset+=sizeof(Buffer))
    {
        UINT32 TransferSize = (Size - Offset) < sizeof(Buffer) ?
            (Size - Offset) : sizeof(Buffer);
            Platform::VirtualRd((UINT08*)Surface->GetAddress() + Offset,
                Buffer, TransferSize);
        if (fwrite(Buffer, TransferSize, 1, File.GetFile()) != 1)
            return RC::FILE_WRITE_ERROR;
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC I2MTest::SaveDestinationSurface
(
    UINT32 LoopIdx,
    TransferParams::Destination Des
)
{
    RC rc;
    if (!m_SaveSurfaces)
        return OK;

    Surface2D *Surface;
    char SurfaceID;

    Surface2D pitchSurface;

    switch (Des)
    {
        case TransferParams::Pitch:
            Surface = m_pDisplaySurf;
            SurfaceID = 'p';
            break;
        case TransferParams::Sys:
            Surface = &m_SysSurf;
            SurfaceID = 's';
            break;
        case TransferParams::BlockLinear:
            // When the fillMethod is a GpuFill, it implies Map() cannot be called on the block-linear
            // surface. Hence, we are copying the surface to a pitch-linear layout before calling Map()
            if (m_FillMethod == SurfaceUtils::FillMethod::Gpufill)
            {
                SurfaceCreateAndDmaCopy(&m_BLSurf, &pitchSurface);
                Surface = &pitchSurface;
            }
            else
            {
                Surface = &m_BLSurf;
            }
            SurfaceID = 'b';
            break;
        default:
            MASSERT(!"Unknown destination");
            return RC::SOFTWARE_ERROR;
    }

    char Filename[64];
    sprintf(Filename, "i2m_l%03d_%c.bin", LoopIdx, SurfaceID);
    CHECK_RC(SaveSurface(Surface, Filename));
    pitchSurface.Free();

    return rc;
}

//-----------------------------------------------------------------------------
UINT32 I2MTest::GetExpectedSemaphoreValue(const UINT32 oldValue, const TransferParams& tp)
{
    const UINT32 newValue = tp.SemaphorePayload;
    if (!tp.ReductionEnabled)
    {
        return newValue;
    }

    const INT32 oldValueSigned = oldValue;
    const INT32 newValueSigned = newValue;

    switch (tp.ReductionOp)
    {
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_ADD:
            return oldValue + newValue;
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_MIN:
            if (tp.ReductionFormatSigned)
            {
                return min(oldValueSigned, newValueSigned);
            }
            else
            {
                return min(oldValue, newValue);
            }
            break;
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_MAX:
            if (tp.ReductionFormatSigned)
            {
                return max(oldValueSigned, newValueSigned);
            }
            else
            {
                return max(oldValue, newValue);
            }
            break;
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_INC:
            return (oldValue >= newValue) ? 0 : (oldValue + 1);
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_DEC:
            return (oldValue == 0 || oldValue > newValue)
                ? newValue : (oldValue - 1);
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_AND:
            return oldValue & newValue;
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_OR:
            return oldValue | newValue;
        case LWA040_LAUNCH_DMA_REDUCTION_OP_RED_XOR:
            return oldValue ^ newValue;
        default:
            MASSERT(!"Invalid ReductionOp!");
            break;
    }
    return 0xDEADBEEF;
}

//-----------------------------------------------------------------------------
RC I2MTest::LoopTransfers()
{
    RC rc;

    const UINT32 StartLoop        = m_pTestConfig->StartLoop();
    const UINT32 RestartSkipCount = m_pTestConfig->RestartSkipCount();
    const UINT32 Loops            = m_pTestConfig->Loops();
    const UINT32 Seed             = m_pTestConfig->Seed();
    const UINT32 EndLoop          = StartLoop + Loops;

    for (UINT32 LoopIdx = StartLoop; LoopIdx < EndLoop; LoopIdx++)
    {
        if ((LoopIdx == StartLoop) || ((LoopIdx % RestartSkipCount) == 0))
        {
            Printf(Tee::PriLow, "\n\tRestart: loop %d, seed %#010x\n",
                   LoopIdx, Seed + LoopIdx);

            m_pRandom->SeedRandom(Seed + LoopIdx);
        }

        TransferParams TP;
        CHECK_RC(PickTransferParameters(&TP));

        UINT64 SemaphoreOffset = 0;
        UINT32 *SemaphoreAddress = 0;
        switch (TP.Dest)
        {
            case TransferParams::Pitch:
            case TransferParams::BlockLinear:
                SemaphoreOffset  = m_FBSemaphoreSurf.GetCtxDmaOffsetGpu();
                SemaphoreAddress = (UINT32*)m_FBSemaphoreSurf.GetAddress();
                break;
            case TransferParams::Sys:
                SemaphoreOffset  = m_SysSemaphoreSurf.GetCtxDmaOffsetGpu();
                SemaphoreAddress = (UINT32*)m_SysSemaphoreSurf.GetAddress();
                break;
            default:
                MASSERT(!"Unknown destination");
                return RC::SOFTWARE_ERROR;
        }

        const UINT32 oldSemaphoreValue = MEM_RD32(SemaphoreAddress);

        CHECK_RC(PrintTransferParameters(LoopIdx, &TP));

        CHECK_RC(m_pCh->Write(s_I2M, LWA040_LINE_LENGTH_IN, TP.LineLengthIn));
        CHECK_RC(m_pCh->Write(s_I2M, LWA040_LINE_COUNT, TP.LineCount));
        CHECK_RC(m_pCh->Write(s_I2M, LWA040_PITCH_OUT, TP.PitchOut));
        CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_DST_WIDTH, TP.DstWidth));
        CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_DST_HEIGHT, TP.DstHeight));

        CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_I2M_SEMAPHORE_C,
            TP.SemaphorePayload));

        CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_I2M_SEMAPHORE_A,
            LwU64_HI32(SemaphoreOffset)));
        CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_I2M_SEMAPHORE_B,
            LwU64_LO32(SemaphoreOffset)));

        UINT32 LaunchValue = 0;
        UINT64 CombinedOffsetOut = 0;
        switch (TP.Dest)
        {
            case TransferParams::Pitch:
                LaunchValue |= DRF_DEF(A040, _LAUNCH_DMA, _DST_MEMORY_LAYOUT,
                    _PITCH);
                CombinedOffsetOut = TP.OffsetOut +
                    m_pDisplaySurf->GetCtxDmaOffsetGpu();
                break;
            case TransferParams::Sys:
                LaunchValue |= DRF_DEF(A040, _LAUNCH_DMA, _DST_MEMORY_LAYOUT,
                    _PITCH);
                CombinedOffsetOut = TP.OffsetOut +
                    m_SysSurf.GetCtxDmaOffsetGpu();
                break;
            case TransferParams::BlockLinear:
                LaunchValue |= DRF_DEF(A040, _LAUNCH_DMA, _DST_MEMORY_LAYOUT,
                    _BLOCKLINEAR);
                CombinedOffsetOut = TP.OffsetOut +
                    m_BLSurf.GetCtxDmaOffsetGpu();

                CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_DST_BLOCK_SIZE,
                    DRF_NUM(A040, _SET_DST_BLOCK_SIZE, _WIDTH,
                        m_BLSurf.GetLogBlockWidth()) |
                    DRF_NUM(A040, _SET_DST_BLOCK_SIZE, _HEIGHT,
                        m_BLSurf.GetLogBlockHeight()) |
                    DRF_NUM(A040, _SET_DST_BLOCK_SIZE, _DEPTH,
                        m_BLSurf.GetLogBlockDepth())));

                CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_DST_DEPTH,
                    m_BLSurf.GetDepth()));

                CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_DST_LAYER, 0));

                CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_DST_ORIGIN_BYTES_X,
                    TP.DstOriginBytesX));
                CHECK_RC(m_pCh->Write(s_I2M, LWA040_SET_DST_ORIGIN_SAMPLES_Y,
                    TP.DstOriginSamplesY));

                break;
            default:
                MASSERT(!"Unknown destination");
                return RC::SOFTWARE_ERROR;
        }

        CHECK_RC(m_pCh->Write(s_I2M, LWA040_OFFSET_OUT_UPPER,
            LwU64_HI32(CombinedOffsetOut)));
        CHECK_RC(m_pCh->Write(s_I2M, LWA040_OFFSET_OUT,
            LwU64_LO32(CombinedOffsetOut)));

        LaunchValue |= DRF_NUM(A040, _LAUNCH_DMA, _COMPLETION_TYPE,
            TP.SysMemBarDisabled ?
                LWA040_LAUNCH_DMA_COMPLETION_TYPE_FLUSH_DISABLE :
                LWA040_LAUNCH_DMA_COMPLETION_TYPE_RELEASE_SEMAPHORE);

        LaunchValue |= DRF_DEF(A040, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE);

        LaunchValue |= DRF_NUM(A040, _LAUNCH_DMA, _SEMAPHORE_STRUCT_SIZE,
            TP.BigSemaphore ?
                LWA040_LAUNCH_DMA_SEMAPHORE_STRUCT_SIZE_FOUR_WORDS :
                LWA040_LAUNCH_DMA_SEMAPHORE_STRUCT_SIZE_ONE_WORD);

        LaunchValue |= DRF_NUM(A040, _LAUNCH_DMA, _REDUCTION_ENABLE,
            TP.ReductionEnabled ?
                LWA040_LAUNCH_DMA_REDUCTION_ENABLE_TRUE :
                LWA040_LAUNCH_DMA_REDUCTION_ENABLE_FALSE);

        LaunchValue |= DRF_NUM(A040, _LAUNCH_DMA, _REDUCTION_OP,
            TP.ReductionOp);

        LaunchValue |= DRF_NUM(A040, _LAUNCH_DMA, _REDUCTION_FORMAT,
            TP.ReductionFormatSigned ?
                LWA040_LAUNCH_DMA_REDUCTION_FORMAT_SIGNED_32 :
                LWA040_LAUNCH_DMA_REDUCTION_FORMAT_UNSIGNED_32);

        LaunchValue |= DRF_NUM(A040, _LAUNCH_DMA, _SYSMEMBAR_DISABLE,
            TP.SysMemBarDisabled ?
                LWA040_LAUNCH_DMA_SYSMEMBAR_DISABLE_TRUE :
                LWA040_LAUNCH_DMA_SYSMEMBAR_DISABLE_FALSE);

        CHECK_RC(m_pCh->Write(s_I2M, LWA040_LAUNCH_DMA, LaunchValue));

        UINT32 InlineDataSize4 = (TP.LineLengthIn + 3)/4;
        for (UINT32 LineIdx = 0; LineIdx < TP.LineCount; LineIdx++)
        {
            CHECK_RC(m_pCh->WriteNonInc(s_I2M, LWA040_LOAD_INLINE_DATA,
                InlineDataSize4,
                (UINT32*)&m_RandomValues[LineIdx*TP.LineLengthIn +
                    TP.InputOffset]));
        }

        if (TP.SysMemBarDisabled)
            continue;

        CHECK_RC(m_pCh->Flush());

        // Wait for channel to become idle instead of relying on
        // the backend semaphore, because some combinations of
        // reduction operations don't change the semaphore's value.
        //
        // IdleChannel() uses a host semaphore with wait-for-idle flag.
        //
        // Please note that we land here only if sysmembar is
        // not disabled. With sysmembar in the backend semaphore
        // we are sure that the host semaphore will be written
        // after the backend semaphore, so when we read the
        // backend semaphore we will read the updated value.
        CHECK_RC(m_pTestConfig->IdleChannel(m_pCh));

        // Flush GPU cache
        if (GetBoundGpuSubdevice()->IsSOC())
        {
            CHECK_RC(m_pCh->WriteL2FlushDirty());
            CHECK_RC(m_pTestConfig->IdleChannel(m_pCh));
        }

        RC save_rc = SaveDestinationSurface(LoopIdx, TP.Dest);

        CHECK_RC(rc);
        CHECK_RC(save_rc);

        const UINT32 newSemaphoreValue = MEM_RD32(SemaphoreAddress);

        const UINT32 expectedValue =
            GetExpectedSemaphoreValue(oldSemaphoreValue, TP);

        if (newSemaphoreValue != expectedValue)
        {
            Printf(Tee::PriHigh,
                "Error: Read semaphore value 0x%08x is different than "
                "expected 0x%08x.\n",
                newSemaphoreValue, expectedValue);
            return RC::BAD_MEMORY;
        }

        CHECK_RC(VerifyTransfer(LoopIdx, &TP));
    };

    return rc;
}

RC I2MTest::SurfaceCreateAndDmaCopy(Surface2D* pSrcSurf, Surface2D *pDstSurf)
{
    RC rc;

    pDstSurf->SetWidth(pSrcSurf->GetWidth());
    pDstSurf->SetHeight(pSrcSurf->GetHeight());
    pDstSurf->SetLayout(Surface2D::Pitch);
    pDstSurf->SetColorFormat(pSrcSurf->GetColorFormat());
    pDstSurf->SetLocation(Memory::Coherent);
    CHECK_RC(pDstSurf->Alloc(GetBoundGpuDevice()));
    CHECK_RC(pDstSurf->BindGpuChannel(m_hCh));

    CHECK_RC(m_DmaWrap.SetSurfaces(pSrcSurf, pDstSurf));
    CHECK_RC(m_DmaWrap.Copy(
        0,
        0,
        pSrcSurf->GetPitch(),
        pSrcSurf->GetHeight(),
        0,
        0,
        m_pTestConfig->TimeoutMs(),
        GetBoundGpuSubdevice()->GetSubdeviceInst()));

    return rc;
}

//-----------------------------------------------------------------------------
RC I2MTest::VerifyTransfer(UINT32 LoopIdx, const TransferParams *TP)
{
    RC rc;
    UINT32 RandomByteIdx = TP->InputOffset;
    // When fillMethod is GpuFill, it implies CPU FB Mappings are through LWLINK and
    // Map() cannot be called on those surfaces
    if (TP->Dest == TransferParams::BlockLinear && m_FillMethod != SurfaceUtils::FillMethod::Gpufill)
    {
        unique_ptr<BlockLinear> pBLAddressing;
        BlockLinear::Create(&pBLAddressing,
                            TP->DstWidth, TP->DstHeight,
                            1, // depth
                            m_BLSurf.GetLogBlockWidth(),
                            m_BLSurf.GetLogBlockHeight(),
                            m_BLSurf.GetLogBlockDepth(),
                            1, // Bpp
                            GetBoundGpuDevice(),
                            BlockLinear::SelectBlockLinearLayout(
                                m_BLSurf.GetPteKind(), GetBoundGpuDevice()));

        for (UINT32 LineIdx = 0; LineIdx < TP->LineCount; LineIdx++)
        {
            for (UINT32 ByteIdx = 0; ByteIdx < TP->LineLengthIn;
                 ByteIdx++, RandomByteIdx++)
            {
                UINT08 *Address = (UINT08*)m_BLSurf.GetAddress() +
                    pBLAddressing->Address(ByteIdx + TP->DstOriginBytesX,
                        LineIdx + TP->DstOriginSamplesY, 0) +
                    TP->OffsetOut;

                UINT08 ReadValue = MEM_RD08(Address);
                UINT08 ExpectedValue = m_RandomValues[RandomByteIdx];

                if (ReadValue != ExpectedValue)
                {
                    Printf(Tee::PriHigh, "Error at loop %d: Expected 0x%02x, "
                        "but read 0x%02x at line %d, byte index %d.\n",
                        LoopIdx,
                        ExpectedValue,
                        ReadValue,
                        LineIdx,
                        ByteIdx);

                    return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
                }
            }
        }
    }
    else
    {
        vector<UINT08> LineBuffer;
        LineBuffer.resize(TP->LineLengthIn);
        Surface2D pitchSurface;
        // When mapping block-linear surfaces is not supported, it
        // should be copied to a pitch-linear layout surface
        if (TP->Dest == TransferParams::BlockLinear)
        {
            CHECK_RC(SurfaceCreateAndDmaCopy(&m_BLSurf, &pitchSurface));
            pitchSurface.Map();
        }

        for (UINT32 LineIdx = 0; LineIdx < TP->LineCount; LineIdx++)
        {
            UINT08 *Address = 0;

            switch (TP->Dest)
            {
                case TransferParams::Pitch:
                    Address = (UINT08*)m_pDisplaySurf->GetAddress() +
                        TP->OffsetOut + TP->PitchOut * LineIdx;
                    break;
                case TransferParams::Sys:
                    Address = (UINT08*)m_SysSurf.GetAddress() +
                        TP->OffsetOut + TP->PitchOut * LineIdx;
                    break;
                case TransferParams::BlockLinear:
                    Address = (UINT08*)pitchSurface.GetAddress() +
                        TP->OffsetOut + TP->PitchOut * LineIdx;
                    break;
                default:
                    MASSERT(!"Unknown destination");
                    return RC::SOFTWARE_ERROR;
            }

            Platform::VirtualRd(Address, &LineBuffer[0], TP->LineLengthIn);

            for (UINT32 ByteIdx = 0; ByteIdx < TP->LineLengthIn;
                 ByteIdx++, RandomByteIdx++)
            {
                if (LineBuffer[ByteIdx] != m_RandomValues[RandomByteIdx])
                {
                    Printf(Tee::PriHigh, "Error: Expected 0x%02x, "
                        "but read 0x%02x at line %d, byte offset %d.\n",
                        m_RandomValues[RandomByteIdx],
                        LineBuffer[ByteIdx],
                        LineIdx,
                        ByteIdx);

                    return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
                }
            }
        }
        pitchSurface.Free();
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC I2MTest::Cleanup()
{
    StickyRC rc;

    m_SysSemaphoreSurf.Free();
    m_FBSemaphoreSurf.Free();
    m_SysSurf.Free();
    m_BLSurf.Free();

    if (m_pCh)
    {
        rc = m_pTestConfig->FreeChannel(m_pCh);
        m_pCh = 0;
        m_hCh = 0;
    }

    rc = GpuTest::Cleanup();

    m_pDisplaySurf = NULL;

    //The fillMethod is Gpufill only when CPU FB mappings are through LWLINK.
    //The dmaWrapper is used only when LWLINK connections are present
    if (m_FillMethod == SurfaceUtils::FillMethod::Gpufill)
    {
        rc = m_DmaWrap.Cleanup();
    }

    return rc;
}

//-----------------------------------------------------------------------------
bool I2MTest::IsSupported()
{
    return m_I2MAlloc.IsSupported(GetBoundGpuDevice());
}

//-----------------------------------------------------------------------------
bool I2MTest::IsSupportedVf()
{
    return !(GetBoundGpuSubdevice()->IsSMCEnabled());
}
