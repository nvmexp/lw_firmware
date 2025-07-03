/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   b1remap.cpp
 * @brief  Implementation of BAR1 Remapper test.
 *
 */

#include "gputest.h"
#include "core/include/platform.h"
#include "core/include/fpicker.h"
#include "gpu/utility/surf2d.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/nfysema.h"
#include "gpu/include/gralloc.h"
#include "class/cl907f.h"
#include "class/cl90b5.h"
#include "class/cla06fsubch.h"
#include "class/cla0b5.h"
#include "class/clc0b5.h"
#include "ctrl/ctrl907f.h"
#include "core/include/channel.h"
#include "core/include/fpicker.h"
#include "gpu/js/fpk_comm.h"
#include "core/include/utility.h"
#include "core/include/jsonlog.h"
#include "sim/IChip.h"
#include "lwos.h"  // LWOS32_TYPE_DEPTH

#include <vector>

//! BAR1 Remapper Test.
//!
//! The BAR1 Remapper is capable of making a block linear surface
//! appear pitch linear on the Cpu.  This test creates a block linear
//! surface with random parameters and then copies data onto it using
//! the GPU from a pitch linear surface.  The block linear surface is
//! then read back through BAR1 via the CPU after activating the BAR1
//! remapper.
//!
class Bar1RemapperTest: public GpuTest
{
public:
    Bar1RemapperTest();
    virtual ~Bar1RemapperTest();

    bool IsSupported();
    RC   Setup();
    RC   Run();
    RC   Cleanup();

    RC SetDefaultsForPicker(UINT32 Idx);

    SETGET_PROP(PrintPri, UINT32);
    SETGET_PROP(PrintErrors, bool);
    SETGET_PROP(ShowSurfaceProgress, bool);

private:
    //! This enum describes the subchannels used by the Bar1RemapperTest
    enum
    {
        s_DmaCopy = LWA06F_SUBCHANNEL_COPY_ENGINE
    };

    //! This structure describes an error oclwring during readback of
    //! the block linear surface via the remapper
    struct ErrorData
    {
        UINT32 X;             //!< X pixel location
        UINT32 Y;             //!< Y pixel location
        UINT32 Subpixel;      //!< For Bpp > 4, subpixel of the error
        UINT32 ActualValue;   //!< Actual value of the pixel
        UINT32 ExpectedValue; //!< Expected value of the pixel
    };
    // Pointers to generic test data
    GpuTestConfiguration   *m_pTestConfig;
    FancyPicker::FpContext *m_pFpCtx;
    FancyPickerArray       *m_pPickers;

    // Channel parameters.
    Channel *               m_pCh;
    LwRm::Handle            m_hCh;

    // HW Class allocators
    DmaCopyAlloc             m_DmaCopyAlloc;
    NotifySemaphore          m_Semaphore;

    //! Source Pitch-Linear surface of the DMA copy
    Surface2D       m_SrcPitchSurface;

    //! Destination Block Linear surface of the DMA copy
    Surface2D       m_DstBLSurface;

    //! The block linear remapper is not present in fmodel unless backdoor
    //! access is disabled, this saves the current state so it may be restored
    UINT32          m_OrigBackDoor;
    UINT32          m_BackDoorEscapeIndex;

    //! Handle for the Block Linear Remapper
    LwRm::Handle    m_hRemapper;

    // Variables set from JS
    UINT32          m_PrintPri;         //!< Print Priority for surface details
    bool            m_PrintErrors;      //!< Print errors when they occur
    bool            m_ShowSurfaceProgress; //!< Print progress bars for surface
                                           //!< access

    //! Error data for a particular copy
    vector<ErrorData> m_Errors;

    RC SetupSurfaces();
    RC Copy();
    RC CheckDestSurface();
    RC AccessTestSurface(void  *pAddress,
                         UINT32 Width,
                         UINT32 Height,
                         UINT32 Pitch,
                         UINT32 Bpp,
                         bool   bWrite);
    void PrintErrors();

    //! Pixel Accessor base class.
    //!
    //! This class (and the classes derived from it) are used to access pixels
    //! on either the source (for writing) or destination (for verification)
    //! surfaces.  Each subclass is for a specific bpp surface since the
    //! data is encoded differently depending upon the bpp
    //!
    class PixelAccessor
    {
    public:
        virtual ~PixelAccessor() { }
        void WritePixel(void *pDst, UINT32 X, UINT32 Y);
        RC VerifyPixel(void *pSrc, UINT32 X, UINT32 Y,
                       vector<ErrorData> *pErrors);
    protected:
        virtual UINT32 GetExpectedValue(UINT32 X, UINT32 Y) = 0;
        virtual void   SetPixel(void *pDst, UINT32 Subpixel, UINT32 Value) = 0;
        virtual UINT32 GetPixel(void *pSrc, UINT32 Subpixel) = 0;
        virtual UINT32 GetSubPixelCount();
    };

    //! Pixel Accessor class for 8bpp surfaces.
    //!
    class PixelAccessor08 : public PixelAccessor
    {
    public:
        PixelAccessor08() { }
        virtual ~PixelAccessor08() { }
    protected:
        virtual UINT32 GetExpectedValue(UINT32 X, UINT32 Y);
        virtual void   SetPixel(void *pDst, UINT32 Subpixel, UINT32 Value);
        virtual UINT32 GetPixel(void *pSrc, UINT32 Subpixel);
    };

    //! Pixel Accessor class for 16bpp surfaces.
    //!
    class PixelAccessor16 : public PixelAccessor
    {
    public:
        PixelAccessor16() { }
        virtual ~PixelAccessor16() { }
    protected:
        virtual UINT32 GetExpectedValue(UINT32 X, UINT32 Y);
        virtual void   SetPixel(void *pDst, UINT32 Subpixel, UINT32 Value);
        virtual UINT32 GetPixel(void *pSrc, UINT32 Subpixel);
    };

    //! Pixel Accessor class for 32bpp (and greater) surfaces.
    //!
    class PixelAccessor32 : public PixelAccessor
    {
    public:
        PixelAccessor32(UINT32 subPixCount) : m_SubPixCount(subPixCount) { }
        virtual ~PixelAccessor32() { }
    protected:
        virtual UINT32 GetExpectedValue(UINT32 X, UINT32 Y);
        virtual void   SetPixel(void *pDst, UINT32 Subpixel, UINT32 Value);
        virtual UINT32 GetPixel(void *pSrc, UINT32 Subpixel);
        virtual UINT32 GetSubPixelCount();
    private:
        //! For surfaces greater than 32bpp this is the number of subpixels
        //! that this accessor will need to set/get
        UINT32 m_SubPixCount;

    };
};

//----------------------------------------------------------------------------
//! \brief Constructor
//!
Bar1RemapperTest::Bar1RemapperTest()
:  m_pCh(nullptr),
   m_hCh(0),
   m_OrigBackDoor(~0U),
   m_BackDoorEscapeIndex(~0U), // invalid index
   m_hRemapper((LwRm::Handle)0),
   m_PrintPri(Tee::PriLow),
   m_PrintErrors(false),
   m_ShowSurfaceProgress(false)
{
    SetName("Bar1RemapperTest");
    m_pTestConfig = GetTestConfiguration();

    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_B1REMAP_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

//----------------------------------------------------------------------------
//! \brief Destructor
//!
Bar1RemapperTest::~Bar1RemapperTest()
{
}

//----------------------------------------------------------------------------
//! \brief Determine if the Bar1RemapperTest is supported
//!
//! \return true if Bar1RemapperTest is supported, false if not
bool Bar1RemapperTest::IsSupported()
{
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();

    // Bug 1409476 - BAR1 not supported with GPU driver in the kernel
    if (Platform::UsesLwgpuDriver())
    {
        Printf(Tee::PriLow, "BAR1 not supported in this environment\n");
        return false;
    }

    bool bSysmem = false;
    // Transactions will go over LwLink instead of PCIE
    if (OK != SysmemUsesLwLink(&bSysmem) || bSysmem)
        return false;

    // Not supported if heap size is zero since this test requires block linear
    // surfaces which are not supported in system memory.
    return ((pSubdev->HasFeature(Device::GPUSUB_HAS_FB_HEAP) || pSubdev->IsSOC()) &&
            pSubdev->HasFeature(Device::GPUSUB_HAS_BAR1_REMAPPER) &&
            m_DmaCopyAlloc.IsSupported(GetBoundGpuDevice()));
}

//----------------------------------------------------------------------------
//! \brief Setup the Bar1RemapperTest
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC Bar1RemapperTest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());
    CHECK_RC(GpuTest::AllocDisplay());
    CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                      &m_hCh,
                                                      &m_DmaCopyAlloc));

    CHECK_RC(m_pCh->SetObject(s_DmaCopy,
                              m_DmaCopyAlloc.GetHandle()));

    // Setup and allocate the semaphore for copy engine completion
    // notification
    m_Semaphore.Setup(NotifySemaphore::ONE_WORD,
                      m_pTestConfig->NotifierLocation(),
                      1);
    CHECK_RC(m_Semaphore.Allocate(m_pCh, 0));

    CHECK_RC(GetBoundRmClient()->Alloc(m_hCh, &m_hRemapper,
                                       GF100_REMAPPER, NULL));

    m_SrcPitchSurface.SetWidth(1024);
    m_SrcPitchSurface.SetHeight(768);
    m_SrcPitchSurface.SetLayout(Surface2D::Pitch);
    m_SrcPitchSurface.SetColorFormat(ColorUtils::A8R8G8B8);
    m_SrcPitchSurface.SetProtect(Memory::ReadWrite);
    m_SrcPitchSurface.SetLocation(Memory::Coherent);
    CHECK_RC(m_SrcPitchSurface.Alloc(GetBoundGpuDevice(),
                                     GetBoundRmClient()));

    // Under simulation enable front door access.
    // Only then we can leverage the remapper, because backdoor bypasses it.
    // For SOC devices (fullchip CheetAh SOC) we need to disable the FB
    // remapper, because linsim treats all memory as "local" which corresponds
    // to "FB".
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        m_BackDoorEscapeIndex =
            GetBoundGpuSubdevice()->IsSOC() ? IChip::EBACKDOOR_FB
                                            : IChip::EBACKDOOR_SYS;
        Platform::EscapeRead("BACKDOOR_ACCESS", m_BackDoorEscapeIndex, 4,
                &m_OrigBackDoor);
        Platform::EscapeWrite("BACKDOOR_ACCESS", m_BackDoorEscapeIndex, 4, 0);
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Run the Bar1RemapperTest
//!
//! \return OK if the test was successful, not OK otherwise
//!
RC Bar1RemapperTest::Run()
{
    RC rc;

    UINT32 startLoop = m_pTestConfig->StartLoop();
    UINT32 endLoop = m_pTestConfig->StartLoop() + m_pTestConfig->Loops();
    UINT32 restartSkipCount = m_pTestConfig->RestartSkipCount();
    bool bMismatch = false;

    if ((startLoop % restartSkipCount) != 0)
    {
        Printf(Tee::PriNormal, "WARNING: StartLoop is not a restart point.\n");

        Printf(m_PrintPri,
               "loop  width  height  Bpp  BlWidth  BlHeight    Errors\n");
        Printf(m_PrintPri,
               "=====================================================\n");
    }

    m_pFpCtx->Rand.SeedRandom(m_pTestConfig->Seed() + startLoop);
    m_pPickers->Restart();
    m_pFpCtx->LoopNum = startLoop;
    m_pFpCtx->RestartNum = startLoop / restartSkipCount;

    for ( ; (m_pFpCtx->LoopNum < endLoop) && (rc == OK); m_pFpCtx->LoopNum++)
    {
        // Print the restart and header for the surface information if
        // necessary
        if ((m_pFpCtx->LoopNum % restartSkipCount) == 0)
        {
            m_pFpCtx->RestartNum = m_pFpCtx->LoopNum / restartSkipCount;

            Printf(Tee::PriLow, "\n\tRestart: loop %d, seed %#010x\n",
                   m_pFpCtx->LoopNum,
                   m_pTestConfig->Seed() +  m_pFpCtx->LoopNum);

            Printf(m_PrintPri,
                   "loop  width  height  Bpp  BlHeight    Errors\n");
            Printf(m_PrintPri,
                   "============================================\n");

            m_pFpCtx->Rand.SeedRandom(m_pTestConfig->Seed() +
                                      m_pFpCtx->LoopNum);
            m_pPickers->Restart();
        }

        CHECK_RC(SetupSurfaces());

        Printf(m_PrintPri, "%4d   %4d    %4d   %2d        %2d  ",
               m_pFpCtx->LoopNum,
               m_DstBLSurface.GetWidth(),
               m_DstBLSurface.GetHeight(),
               ColorUtils::PixelBytes(m_DstBLSurface.GetColorFormat()),
               (1 << m_DstBLSurface.GetLogBlockHeight()));

        CHECK_RC(Copy());

        rc = CheckDestSurface();

        Printf(m_PrintPri, "%8d\n", (UINT32)m_Errors.size());
        PrintErrors();

        if (OK != rc)
        {
            GetJsonExit()->AddFailLoop(m_pFpCtx->LoopNum);

            if ((rc == RC::BUFFER_MISMATCH) &&
                !GetGoldelwalues()->GetStopOnError())
            {
                bMismatch = true;
                rc.Clear();
            }
        }
        CHECK_RC(rc);
    }

    if (bMismatch)
    {
        rc = RC::BUFFER_MISMATCH;
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Clean up the Bar1RemapperTest
//!
//! \return OK if cleanup was successful, not OK otherwise
//!
RC Bar1RemapperTest::Cleanup()
{
    StickyRC stickyRc;

    m_SrcPitchSurface.Free();
    m_DstBLSurface.Free();
    m_DmaCopyAlloc.Free();
    m_Semaphore.Free();

    if (m_hRemapper)
        GetBoundRmClient()->Free(m_hRemapper);

    if ((Platform::GetSimulationMode() == Platform::Fmodel) &&
        (m_OrigBackDoor != ~0U))
    {
        Platform::EscapeWrite("BACKDOOR_ACCESS", m_BackDoorEscapeIndex, 4,
                m_OrigBackDoor);
    }

    stickyRc = m_pTestConfig->FreeChannel(m_pCh);
    stickyRc = GpuTest::Cleanup();

    return stickyRc;
}

//----------------------------------------------------------------------------
//! \brief Setup the default values for a particular fancy picker.
//!
//! \param Idx : Index of the fancy picker to set defaults for
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC Bar1RemapperTest::SetDefaultsForPicker(UINT32 Idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &Fp = (*pPickers)[Idx];

    switch (Idx)
    {
        case FPK_B1REMAP_WIDTH:
            Fp.ConfigRandom();
            Fp.AddRandRange(1, 8, 2560);
            Fp.CompileRandom();
            break;
        case FPK_B1REMAP_HEIGHT:
            Fp.ConfigRandom();
            Fp.AddRandRange(1, 4, 2048);
            Fp.CompileRandom();
            break;
        case FPK_B1REMAP_BLOCK_HEIGHT_LOG2_GOBS:
            Fp.ConfigRandom();
            Fp.AddRandRange(1, 0, 5);
            Fp.CompileRandom();
            break;
        case FPK_B1REMAP_COLOR_FORMAT:
            // Choose 1 format with each different bytes per pixel
            Fp.ConfigRandom();
            Fp.AddRandItem(1, ColorUtils::I8);                   // 1-byte
            Fp.AddRandItem(1, ColorUtils::R5G6B5);               // 2-byte
            Fp.AddRandItem(1, ColorUtils::A8R8G8B8);             // 4-byte
            Fp.AddRandItem(1, ColorUtils::RF16_GF16_BF16_AF16);  // 8-byte
            Fp.AddRandItem(1, ColorUtils::RF32_GF32_BF32_AF32);  // 16-byte
            Fp.CompileRandom();
            break;
    }

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Private function which allocates a surface of random size and
//!        initializes the data in the source surface for the copy.
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC Bar1RemapperTest::SetupSurfaces()
{
    RC       rc;
    const UINT32 width = (*m_pPickers)[FPK_B1REMAP_WIDTH].Pick();
    UINT32 height = (*m_pPickers)[FPK_B1REMAP_HEIGHT].Pick();
    const ColorUtils::Format color =
            (ColorUtils::Format)(*m_pPickers)[FPK_B1REMAP_COLOR_FORMAT].Pick();
    const UINT32 logBlHeight =
                      (*m_pPickers)[FPK_B1REMAP_BLOCK_HEIGHT_LOG2_GOBS].Pick();

    const UINT64 maxSurfSizeMb = GetBoundGpuSubdevice()->FramebufferBarSize() >> 1;
    UINT64 reqBytes = (UINT64)width * (UINT64)height *
                      (UINT64)ColorUtils::PixelBytes(color);

    // Clip the height based on the maximum possible surface size
    if (reqBytes > (maxSurfSizeMb << 20))
    {
        height = UINT32((maxSurfSizeMb << 20) / (UINT64)width /
                 (UINT64)ColorUtils::PixelBytes(color));
    }

    // Allocate the destination block linear surface.
    m_DstBLSurface.Free();
    m_DstBLSurface.SetWidth(width);
    m_DstBLSurface.SetHeight(height);
    m_DstBLSurface.SetLayout(Surface2D::BlockLinear);
    m_DstBLSurface.SetColorFormat(color);
    m_DstBLSurface.SetProtect(Memory::ReadWrite);
    m_DstBLSurface.SetLogBlockWidth(0);
    m_DstBLSurface.SetLogBlockHeight(logBlHeight);
    m_DstBLSurface.SetLocation(Memory::Fb);

    // On systems that have generic memory the default block linear kind is not supported
    // by the block linear remapper, for those systems allocate a depth surface which results
    // in a supported PTE kind
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_GENERIC_MEMORY))
        m_DstBLSurface.SetType(LWOS32_TYPE_DEPTH);

    CHECK_RC(m_DstBLSurface.Alloc(GetBoundGpuDevice(), GetBoundRmClient()));

    if (m_SrcPitchSurface.GetSize() < reqBytes)
    {
        // Allocate source surface to match the destination surface.
        m_SrcPitchSurface.Free();
        m_SrcPitchSurface.SetWidth(width);
        m_SrcPitchSurface.SetHeight(height);
        m_SrcPitchSurface.SetLayout(Surface2D::Pitch);
        m_SrcPitchSurface.SetColorFormat(color);
        m_SrcPitchSurface.SetProtect(Memory::ReadWrite);
        m_SrcPitchSurface.SetLocation(Memory::Coherent);
        CHECK_RC(m_SrcPitchSurface.Alloc(GetBoundGpuDevice(),
                                         GetBoundRmClient()));
    }

    // Saving an unmapped surface ensures that the surface will be unmapped
    // when the function exits
    Surface2D::MappingSaver savedMapping(m_SrcPitchSurface);

    // These surfaces can be *very* large.  So much so that we cannot keep
    // multiple copies of the surface mapped, so map/unmap the surface
    // everytime.  The overhead should be small compared to the overhead of
    // actually accessing the surface via the CPU
    CHECK_RC(m_SrcPitchSurface.Map(GetBoundGpuSubdevice()->GetSubdeviceInst(),
                                   GetBoundRmClient()));

    // Initialize the data in the source surface
    CHECK_RC(AccessTestSurface(m_SrcPitchSurface.GetAddress(),
                               width,
                               height,
                               width * ColorUtils::PixelBytes(color),
                               ColorUtils::PixelBytes(color),
                               true));

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Private function which uses the copy engine to copy the source
//!        pitch linear surface to the destination block linear surface.
//!
//! \return OK if setup was successful, not OK otherwise
//!
RC Bar1RemapperTest::Copy()
{
    RC rc;
    UINT32 layoutMem = 0;
    UINT32 AddressMode = 0;
    UINT32 ClassID = m_DmaCopyAlloc.GetClass();

    m_Semaphore.SetPayload(0);
    m_Semaphore.SetTriggerPayload(0x87654321);

    if (ClassID == GF100_DMA_COPY)
    {
        // Set application for copy engine
        CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_APPLICATION_ID,
                              DRF_DEF(90B5, _SET_APPLICATION_ID, _ID, _NORMAL)));
    }

    // Setup surface offsets
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_OFFSET_IN_UPPER,
        2,
        static_cast<UINT32>(m_SrcPitchSurface.GetCtxDmaOffsetGpu() >> 32), /* LW90B5_OFFSET_IN_UPPER */
        static_cast<UINT32>(m_SrcPitchSurface.GetCtxDmaOffsetGpu()))); /* LW90B5_OFFSET_IN_LOWER */

    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_OFFSET_OUT_UPPER,
        2,
        static_cast<UINT32>(m_DstBLSurface.GetCtxDmaOffsetGpu() >> 32), /* LW90B5_OFFSET_OUT_UPPER */
        static_cast<UINT32>(m_DstBLSurface.GetCtxDmaOffsetGpu()))); /* LW90B5_OFFSET_OUT_LOWER */

    layoutMem = DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH) |
                DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _BLOCKLINEAR);

    // Destination specific methods setup, source surface is just a block of
    // data and is always viewed in terms of the destination surface
    UINT32 srcPitch = m_DstBLSurface.GetWidth() *
                      ColorUtils::PixelBytes(m_DstBLSurface.GetColorFormat());
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_PITCH_IN, srcPitch));
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_LINE_LENGTH_IN, srcPitch));

    // Destination specific methods setup
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_PITCH_OUT,
                          m_DstBLSurface.GetPitch()));
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_DST_BLOCK_SIZE,
                          DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _HEIGHT,
                                  m_DstBLSurface.GetLogBlockHeight()) |
                          DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _DEPTH,
                                  m_DstBLSurface.GetLogBlockDepth()) |
                          DRF_DEF(90B5, _SET_DST_BLOCK_SIZE,
                                  _GOB_HEIGHT,
                                  _GOB_HEIGHT_FERMI_8)));
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_DST_DEPTH, 1));
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_DST_LAYER, 0));
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_DST_WIDTH,
                     m_DstBLSurface.GetWidth() *
                     ColorUtils::PixelBytes(m_DstBLSurface.GetColorFormat())));
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_DST_HEIGHT,
                          m_DstBLSurface.GetHeight()));
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_DST_ORIGIN,
                          DRF_NUM(90B5, _SET_DST_ORIGIN, _X, 0) |
                          DRF_NUM(90B5, _SET_DST_ORIGIN, _Y, 0)));

    // Methods which apply to both source and destination
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_LINE_COUNT,
                          m_DstBLSurface.GetHeight()));
    if (ClassID == GF100_DMA_COPY)
    {
        AddressMode = DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL) |
                   DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL) |
                   DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _COHERENT_SYSMEM) |
                   DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TARGET, _LOCAL_FB);
        CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_ADDRESSING_MODE, AddressMode));
    }
    else
    {
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
    }

    CHECK_RC(m_pCh->Write(s_DmaCopy,
        LW90B5_SET_SEMAPHORE_A,
        2,
        UINT32(m_Semaphore.GetCtxDmaOffsetGpu(0) >> 32), /* LW90B5_SET_SEMAPHORE_A */
        UINT32(m_Semaphore.GetCtxDmaOffsetGpu(0)))); /* LW90B5_SET_SEMAPHORE_B */

    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_SET_SEMAPHORE_PAYLOAD, 0x87654321));

    // Start the copy and wait for it to finish
    // Use NON_PIPELINED to WAR hw bug 618956.
    CHECK_RC(m_pCh->Write(s_DmaCopy, LW90B5_LAUNCH_DMA,
                          layoutMem |
                          DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                                  _RELEASE_ONE_WORD_SEMAPHORE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE,
                                  _NON_PIPELINED) |
                          DRF_DEF(90B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE,
                                  _TRUE)));

    CHECK_RC(m_pCh->Flush());
    CHECK_RC(m_Semaphore.Wait(m_pTestConfig->TimeoutMs()));

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Private function which verifies the destination surface by reading
//!        it back with the BAR1 remapper active.
//!
//! \return OK if the destination surface is correct, not OK otherwise
//!
RC Bar1RemapperTest::CheckDestSurface()
{
    StickyRC      rc;
    UINT32  pitchGobs;
    LW907F_CTRL_SET_SURFACE_PARAMS remapSurfParams = {0};
    LW907F_CTRL_SET_BLOCKLINEAR_PARAMS remapBLParams = {0};
    LW907F_CTRL_START_BLOCKLINEAR_REMAP_PARAMS remapStartParams = {0};
    LW907F_CTRL_STOP_BLOCKLINEAR_REMAP_PARAMS remapStopParams = {0};
    UINT08 *pDst = 0;
    const UINT32 gobWidth = GetBoundGpuDevice()->GobWidth();
    const UINT32 gobHeight = GetBoundGpuDevice()->GobHeight();
    const UINT32 gobSize = GetBoundGpuDevice()->GobSize();
    const UINT32 bpp =
                  ColorUtils::PixelBytes(m_DstBLSurface.GetColorFormat());
    LwRm *pLwRm = GetBoundRmClient();
    LwRm::Handle hSubDevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());

    // Saving an unmapped surface ensures that the surface will be unmapped
    // when the function exits
    Surface2D::MappingSaver savedMapping(m_DstBLSurface);

    // These surfaces can be *very* large.  So much so that we cannot keep
    // multiple copies of the surface mapped, so map/unmap the surface
    // everytime.  The overhead should be small compared to the overhead of
    // actually accessing the surface via the CPU
    CHECK_RC(m_DstBLSurface.Map(GetBoundGpuSubdevice()->GetSubdeviceInst(),
                                GetBoundRmClient()));

    pDst = (UINT08 *)m_DstBLSurface.GetAddress();

    // Setup the params for the surface remap call
    pitchGobs = (m_DstBLSurface.GetPitch() + gobWidth - 1) / gobWidth;
    remapSurfParams.pMemory = LW_PTR_TO_LwP64(pDst);
    remapSurfParams.hMemory = m_DstBLSurface.GetMemHandle();
    remapSurfParams.hSubDevice = hSubDevice;
    remapSurfParams.size = ((UINT32)m_DstBLSurface.GetSize() + gobSize - 1) / gobSize;
    remapSurfParams.format  =
        DRF_NUM(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _PITCH, pitchGobs) |
        DRF_DEF(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _MEM_SPACE, _CLIENT);
    switch (bpp)
    {
        case 1:
            remapSurfParams.format |=
                DRF_DEF(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _BYTES_PIXEL, _1);
            break;
        case 2:
            remapSurfParams.format |=
                DRF_DEF(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _BYTES_PIXEL, _2);
            break;
        case 4:
            remapSurfParams.format |=
                DRF_DEF(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _BYTES_PIXEL, _4);
            break;
        case 8:
            remapSurfParams.format |=
                DRF_DEF(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _BYTES_PIXEL, _8);
            break;
        case 16:
            remapSurfParams.format |=
                DRF_DEF(907F, _CTRL_CMD_SET_SURFACE_FORMAT, _BYTES_PIXEL, _16);
            break;
        default:
            Printf(Tee::PriHigh, "%s : Invalid Bpp (%d)\n",
                   GetName().c_str(), bpp);
            return RC::SOFTWARE_ERROR;
    }

    // Setup the params for the block linear remap call
    remapBLParams.pMemory = LW_PTR_TO_LwP64(pDst);
    remapBLParams.hMemory = m_DstBLSurface.GetMemHandle();
    remapBLParams.hSubDevice = hSubDevice;
    remapBLParams.gob  = DRF_DEF(907F, _CTRL_CMD_SET_BLOCKLINEAR_GOB,
                                 _MEM_SPACE, _CLIENT);
    remapBLParams.block   =
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_BLOCK, _HEIGHT,
                m_DstBLSurface.GetLogBlockHeight())|
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_BLOCK, _DEPTH,
                m_DstBLSurface.GetLogBlockDepth());

    UINT32  blockHeight = (1 << m_DstBLSurface.GetLogBlockHeight()) *
                          gobHeight;
    UINT32  imageBlockHeight = (m_DstBLSurface.GetHeight() + blockHeight - 1) /
                               blockHeight;

    UINT32  blockWidth = gobWidth / bpp;
    UINT32  imageBlockWidth = (m_DstBLSurface.GetWidth() + blockWidth - 1) /
                              blockWidth;
    remapBLParams.imageWH =
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_IMAGEWH, _HEIGHT,
                imageBlockHeight) |
        DRF_NUM(907F, _CTRL_CMD_SET_BLOCKLINEAR_IMAGEWH, _WIDTH,
                imageBlockWidth);

    remapBLParams.log2ImageSlice =
          LW907F_CTRL_CMD_SET_BLOCKLINEAR_LOG2IMAGESLICE_0_Z;

    remapStartParams.hSubDevice = hSubDevice;
    remapStopParams.hSubDevice = hSubDevice;

    //
    //sets the surface's parameters for the remapper used by
    //blocklinear remapping
    //
    CHECK_RC(pLwRm->Control(m_hRemapper,
                            LW907F_CTRL_CMD_SET_SURFACE,
                            &remapSurfParams,
                            sizeof(remapSurfParams)));
    //
    //sets the block linear specific parameters for the remapper.
    //
    CHECK_RC(pLwRm->Control(m_hRemapper,
                            LW907F_CTRL_CMD_SET_BLOCKLINEAR,
                            &remapBLParams,
                            sizeof(remapBLParams)));

    //
    // Enable blocklinear remap
    //
    CHECK_RC(pLwRm->Control(m_hRemapper,
                            LW907F_CTRL_CMD_START_BLOCKLINEAR_REMAP,
                            &remapStartParams,
                            sizeof(remapStartParams)));

    // Verify the surface
    m_Errors.clear();
    rc = AccessTestSurface(m_DstBLSurface.GetAddress(),
                           m_DstBLSurface.GetWidth(),
                           m_DstBLSurface.GetHeight(),
                           m_DstBLSurface.GetPitch(),
                           bpp,
                           false);

    // Disable the remapper
    CHECK_RC(pLwRm->Control(m_hRemapper,
                            LW907F_CTRL_CMD_STOP_BLOCKLINEAR_REMAP,
                            &remapStopParams,
                            sizeof(remapStopParams)));

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Private function to access a test surface (either to write the
//!        source surface or to verify the destination surface).
//!
//! \param pAddress : CPU pointer to the first byte in the surface
//! \param Width    : Width of the surface
//! \param Height   : Height of the surface
//! \param Pitch    : Pitch in bytes to use when accessing the surface
//! \param Bpp      : Bytes per pixel of the surface
//! \param bWrite   : true to write the surface, false to verify the surface
//!
//! \return OK if successful (and no miscompares on verification),
//!         not OK otherwise
//!
RC Bar1RemapperTest::AccessTestSurface(void  *pAddress,
                                       UINT32 Width,
                                       UINT32 Height,
                                       UINT32 Pitch,
                                       UINT32 Bpp,
                                       bool   bWrite)
{
    StickyRC firstRc;
    UINT08 *pSrcSurf = (UINT08 *)pAddress;
    UINT08 *pLwrPixel;
    unique_ptr<PixelAccessor> pixelAccess;
    Tasker::DetachThread threadDetacher;

    switch (Bpp)
    {
        case 1:
            pixelAccess.reset(static_cast<PixelAccessor *>(new PixelAccessor08));
            break;
        case 2:
            pixelAccess.reset(static_cast<PixelAccessor *>(new PixelAccessor16));
            break;
        case 4:
        case 8:
        case 16:
            pixelAccess.reset(static_cast<PixelAccessor *>(new PixelAccessor32(Bpp / 4)));
            break;
        default:
            Printf(Tee::PriHigh, "%s : Invalid Bpp (%d)\n",
                   GetName().c_str(), Bpp);
            return RC::SOFTWARE_ERROR;
    }

    vector<UINT08> lineData(Width * Bpp, 0);
    Tee::Priority showProgressPri = m_ShowSurfaceProgress ? Tee::PriHigh :
                                                            Tee::PriNone;

    Printf(showProgressPri, "%s Test Surface.",
           bWrite ? "Writing" : "Verifying");
    for (UINT32 y = 0; y < Height; y++)
    {
        pLwrPixel = &lineData[0];
        if (!bWrite)
        {
            Platform::VirtualRd(pSrcSurf, &lineData[0],
                                (UINT32)lineData.size());
        }
        for (UINT32 x = 0; x < Width; x++, pLwrPixel += Bpp)
        {
            if (bWrite)
            {
                pixelAccess->WritePixel(pLwrPixel, x, y);
            }
            else
            {
                // Always verify the entire surface so that an error count
                // may be generated, but remember the first error via StickyRC
                firstRc = pixelAccess->VerifyPixel(pLwrPixel, x, y, &m_Errors);
            }
        }

        if (bWrite)
        {
            Platform::VirtualWr(pSrcSurf, &lineData[0],
                                (UINT32)lineData.size());
        }
        pSrcSurf += Pitch;

        // Conform to the hardware polling requirement requested by
        // "-poll_hw_hz" (i.e. HW should not be accessed faster than a
        // certain rate).  By default this will not sleep or yield unless
        // the command line argument is present.
        Tasker::PollMemSleep();

        // Add a yield every line to be friendly to other tests
        Tasker::Yield();
        Printf(showProgressPri, ".");
    }

    Printf(showProgressPri, ".%s\n", (firstRc == OK) ? "done" : "ERROR!");
    return firstRc;
}

//----------------------------------------------------------------------------
//! \brief Private function to print the table of errors that oclwred on a
//!        single surface.
//!
void Bar1RemapperTest::PrintErrors()
{
    if (!m_PrintErrors || !m_Errors.size())
        return;

    Printf(Tee::PriHigh,
           "          (X, Y)  Subpix    Expected      Actual\n");
    Printf(Tee::PriHigh,
           "    ============================================\n");
    for (UINT32 i = 0; i < m_Errors.size(); i++)
    {
        Printf(Tee::PriHigh,
               "    (%4d, %4d)       %1d  0x%08x  0x%08x\n",
               m_Errors[i].X, m_Errors[i].Y, m_Errors[i].Subpixel,
               m_Errors[i].ExpectedValue,
               m_Errors[i].ActualValue);
    }
}

//----------------------------------------------------------------------------
//! \brief Write data onto a surface at the specified X and Y location.  The
//!        data written is an encoding of the X and Y location of the pixel
//!        written.
//!
//! \param pDst : Pointer to the destination pixel to write
//! \param X    : X location of the pixel
//! \param Y    : Y location of the pixel
//!
void Bar1RemapperTest::PixelAccessor::WritePixel
(
    void *pDst,
    UINT32 X,
    UINT32 Y
)
{
    for (UINT32 subpix = 0; subpix < GetSubPixelCount(); subpix++)
    {
        SetPixel(pDst, subpix, GetExpectedValue(X, Y));
    }
}

//----------------------------------------------------------------------------
//! \brief Verify data on a surface at the specified X and Y location.  The
//!        data verifed is based upon an encoding of the X and Y location of
//!        the pixel
//!
//! \param pSrc    : Pointer to the source pixel to verify
//! \param X       : X location of the pixel
//! \param Y       : Y location of the pixel
//! \param pErrors : Pointer to storage for errors that occur during
//!                  verification
//!
//! \return OK if the expected and actual values match, not OK otherwise
//!
RC Bar1RemapperTest::PixelAccessor::VerifyPixel
(
    void *pSrc,
    UINT32 X,
    UINT32 Y,
    vector<ErrorData> *pErrors
)
{
    ErrorData errorData;
    UINT32 errorCount = (UINT32)pErrors->size();

    errorData.X = X;
    errorData.Y = Y;
    errorData.ExpectedValue = GetExpectedValue(X, Y);

    for (UINT32 subpix = 0; subpix < GetSubPixelCount(); subpix++)
    {
        errorData.Subpixel = subpix;
        errorData.ActualValue = GetPixel(pSrc, subpix);
        if (errorData.ActualValue != errorData.ExpectedValue)
        {
            pErrors->push_back(errorData);
        }
    }

    return (errorCount != (UINT32)pErrors->size()) ? RC::BUFFER_MISMATCH : OK;
}

//----------------------------------------------------------------------------
//! \brief Return the number of subpixels used in the PixelAccessor.
//!
//! \return The number of subpixels used by the PixelAccessor (1 by default)
//!
UINT32 Bar1RemapperTest::PixelAccessor::GetSubPixelCount()
{
    return 1;
}

//----------------------------------------------------------------------------
//! \brief Get the expected value for a pixel on an 8bpp surface
//!
//! \param X : X location of the pixel
//! \param Y : Y location of the pixel
//!
//! \return The expected value of the pixel at the specified location
//!
UINT32 Bar1RemapperTest::PixelAccessor08::GetExpectedValue(UINT32 X, UINT32 Y)
{
    return (X + Y) & 0xFF;
}

//----------------------------------------------------------------------------
//! \brief Set the value for the pixel to the specified value on an 8bpp
//!        surface
//!
//! \param pDst     : Pointer to the pixel to write
//! \param Subpixel : Subpixel within the pixel to write (must be 0)
//! \param Value    : Value to write the pixel to
//!
void Bar1RemapperTest::PixelAccessor08::SetPixel(void *pDst,
                                                 UINT32 Subpixel,
                                                 UINT32 Value)
{
    MASSERT(!Subpixel);
    UINT08 *pPixel = (UINT08 *)pDst;
    *pPixel = Value;
}

//----------------------------------------------------------------------------
//! \brief Get the value for the pixel on an 8bpp surface
//!
//! \param pDst     : Pointer to the pixel to read
//! \param Subpixel : Subpixel within the pixel to read (must be 0)
//!
//! \return The value of the pixel
//!
UINT32 Bar1RemapperTest::PixelAccessor08::GetPixel(void *pSrc, UINT32 Subpixel)
{
    MASSERT(!Subpixel);
    UINT08 *pPixel = (UINT08 *)pSrc;
    return *pPixel;
}

//----------------------------------------------------------------------------
//! \brief Get the expected value for a pixel on a 16bpp surface
//!
//! \param X : X location of the pixel
//! \param Y : Y location of the pixel
//!
//! \return The expected value of the pixel at the specified location
//!
UINT32 Bar1RemapperTest::PixelAccessor16::GetExpectedValue(UINT32 X, UINT32 Y)
{
    return ((X & 0xFF) << 8) | (Y & 0xFF);
}

//----------------------------------------------------------------------------
//! \brief Set the value for the pixel to the specified value on a 16bpp
//!        surface
//!
//! \param pDst     : Pointer to the pixel to write
//! \param Subpixel : Subpixel within the pixel to write (must be 0)
//! \param Value    : Value to write the pixel to
//!
void Bar1RemapperTest::PixelAccessor16::SetPixel(void *pDst,
                                                 UINT32 Subpixel,
                                                 UINT32 Value)
{
    MASSERT(!Subpixel);
    UINT16 *pPixel = (UINT16 *)pDst;
    *pPixel = Value;
}

//----------------------------------------------------------------------------
//! \brief Get the value for the pixel on a 16bpp surface
//!
//! \param pDst     : Pointer to the pixel to read
//! \param Subpixel : Subpixel within the pixel to read (must be 0)
//!
//! \return The value of the pixel
//!
UINT32 Bar1RemapperTest::PixelAccessor16::GetPixel(void *pSrc, UINT32 Subpixel)
{
    MASSERT(!Subpixel);
    UINT16 *pPixel = (UINT16 *)pSrc;
    return *pPixel;
}

//----------------------------------------------------------------------------
//! \brief Get the expected value for a pixel on a 32bpp surface
//!
//! \param X : X location of the pixel
//! \param Y : Y location of the pixel
//!
//! \return The expected value of the pixel at the specified location
//!
UINT32 Bar1RemapperTest::PixelAccessor32::GetExpectedValue(UINT32 X, UINT32 Y)
{
    return ((X & 0xFFFF) << 16) | (Y & 0xFFFF);
}

//----------------------------------------------------------------------------
//! \brief Set the value for the pixel to the specified value on a 32bpp
//!        surface
//!
//! \param pDst     : Pointer to the pixel to write
//! \param Subpixel : Subpixel within the pixel to write
//! \param Value    : Value to write the pixel to
//!
void Bar1RemapperTest::PixelAccessor32::SetPixel(void *pDst,
                                                 UINT32 Subpixel,
                                                 UINT32 Value)
{
    MASSERT(Subpixel < m_SubPixCount);
    UINT32 *pPixel = (UINT32 *)pDst;
    pPixel += Subpixel;
    *pPixel = Value;
}

//----------------------------------------------------------------------------
//! \brief Get the value for the pixel on a 32bpp surface
//!
//! \param pDst     : Pointer to the pixel to read
//! \param Subpixel : Subpixel within the pixel to read
//!
//! \return The value of the pixel
//!
UINT32 Bar1RemapperTest::PixelAccessor32::GetPixel(void *pSrc, UINT32 Subpixel)
{
    MASSERT(Subpixel < m_SubPixCount);
    UINT32 *pPixel = (UINT32 *)pSrc;
    pPixel += Subpixel;
    return *pPixel;
}

//----------------------------------------------------------------------------
//! \brief Return the number of subpixels used in the PixelAccessor (i.e. the
//!        number of 32-bit values needed to fill the entire pixel).
//!
//! \return The number of subpixels used by the PixelAccessor
//!
UINT32 Bar1RemapperTest::PixelAccessor32::GetSubPixelCount()
{
    return m_SubPixCount;
}

/**
 * Bar1RemapperTest script interface.
 */

JS_CLASS_INHERIT(Bar1RemapperTest, GpuTest, "Bar1RemapperTest object.");

//----------------------------------------------------------------------------
CLASS_PROP_READWRITE(Bar1RemapperTest, PrintPri, UINT32,
        "Print surface information during runs");
CLASS_PROP_READWRITE(Bar1RemapperTest, PrintErrors, bool,
        "Print error information durning runs");
CLASS_PROP_READWRITE(Bar1RemapperTest, ShowSurfaceProgress, bool,
        "Print progress bars for surface access");

