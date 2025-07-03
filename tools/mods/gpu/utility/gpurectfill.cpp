/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpurectfill.h"
#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "lwos.h"
#include "surf2d.h"
#include "gpu/include/nfysema.h"
#include "gpu/include/gralloc.h"
#include "core/include/color.h"
#include "class/cl502d.h"
#include "class/cl902d.h"
#include "class/cle22d.h"
#include "class/cla06fsubch.h"  //LWA06F_SUBCHANNEL_2D

// Maximum number of rectanle fills we can flush down 2D without waiting.
// Number chosen is rather arbitrary, and tested to work.
static const UINT32 MAX_TWOD_FLUSHES = 20;

//------------------------------------------------------------------------------
//! \class GpuRectFillImpl
//!
//! \brief Interface for various GPU rectangle fill implementations.
//!
//! This class provides interface for various classes that
//! provide GPU rectangle fill operations. They are plugged into
//! GpuRectFill based on capability.
class GpuRectFillImpl
{
public:
    virtual ~GpuRectFillImpl(){};

    virtual RC SetSurface
    (
        Surface2D*         pSurf,
        ColorUtils::Format format,
        UINT32             width,
        UINT32             height,
        UINT32             pitch
    ) = 0;
    virtual RC Fill
    (
        UINT32  x,           //!< Starting src X, pixels.
        UINT32  y,           //!< Starting src Y, in lines.
        UINT32  width,       //!< Width of copied rect, in pixels.
        UINT32  height,      //!< Height of copied rect, in lines
        UINT32  color,       //!< Color of the fill
        UINT32  flushId      //!< Identifier of fill operation.
                             //!< Used for waiting on operation to complete.
                             //!< Current implementation expects these to
                             //!< be unique and incrementing to function
                             //!< properly.
    ) = 0;

    //! \brief Waits for the rectangle fill operation to complete.
    //!
    //! Waits for particular rect fill implementation to signal completion.
    //! If particular class supports custom semaphore values,
    //! FlushId parameter specifies the value to wait on.
    //! Wait is expected to return when value of FlushId or greater
    //! gets written into semaphore surface.
    //! In case fill class doesn't support custom semaphore values,
    //! (ie. CheetAh implemenation) FlushId parameter is ignored, and Wait returns
    //! after the implementation specific signal gets triggered (in case of
    //! CheetAh when the channel is idle)
    virtual RC Wait
    (
        UINT32  flushId      //!< Smallest value to return on.
    ) = 0;

    virtual RC Initialize(GpuTestConfiguration * pTestConfig) = 0;
};

//------------------------------------------------------------------------------
//! \class Lw50TwodGpuRectFill
//!
//! \brief Fills memory using the LW50 2D class.
//!
class Lw50TwodGpuRectFill : public GpuRectFillImpl
{
public:
    Lw50TwodGpuRectFill(GpuTestConfiguration * pTestConfig) : m_pTestConfig(pTestConfig) { }
    virtual ~Lw50TwodGpuRectFill();
    static bool IsSupported(GpuDevice* pGpuDev)
    {
        TwodAlloc alloc;
        return alloc.IsSupported(pGpuDev);
    }
    virtual RC SetSurface
    (
        Surface2D*         pSurf,
        ColorUtils::Format format,
        UINT32             width,
        UINT32             height,
        UINT32             pitch
    );
    virtual RC Fill
    (
        UINT32  x,
        UINT32  y,
        UINT32  width,
        UINT32  height,
        UINT32  color,
        UINT32  flushId
    );

    virtual RC Wait
    (
        UINT32  flushId
    );

    virtual RC Initialize(GpuTestConfiguration * pTestConfig);

private:
    RC InitSemaphore(Memory::Location semaphoreLocation);
    RC WriteMethods
    (
        UINT32 x,
        UINT32 y,
        UINT32 width,
        UINT32 height,
        UINT32 color,
        UINT32 payload
    );

    Surface2D*         m_pSurface    = nullptr;               //!< Surface to perform the fill on
    ColorUtils::Format m_SurfFormat  = ColorUtils::LWFMT_NONE;//!< Format override for the surface
    UINT32             m_SurfWidth   = 0;                     //!< Width override for the surface
    UINT32             m_SurfHeight  = 0;                     //!< Height override for the surface
    UINT32             m_SurfPitch   = 0;                     //!< Pitch override for the surface
    bool               m_Initialized = false;                 //!< True if initialized

    TwodAlloc              m_TwodAlloc;                       //!< TwoD object for the fill
    GpuTestConfiguration * m_pTestConfig    = nullptr;
    Channel              * m_pCh            = nullptr;        //!< Pointer to channel to use for 2D
                                                              //!< methods
    Memory::Location       m_SelectedNfyLoc = Memory::Optimal;//!< location for the notifier

    using MapLocationSemaphore =
        map<Memory::Location, unique_ptr<NotifySemaphore>>;
    MapLocationSemaphore m_Semaphores;//!< Notifier list
};

//----------------------------------------------------------------------------
//! \brief Destructor - delete any allocated semaphores
//!
Lw50TwodGpuRectFill::~Lw50TwodGpuRectFill()
{
    m_TwodAlloc.Free();
    if (m_pCh)
        m_pTestConfig->FreeChannel(m_pCh);
}

//----------------------------------------------------------------------------
//! \brief Sets the surface to use for the fills providing a color format to
//!        use instead of the actual surface color format
//!
//! \param pSurf  : Pointer to surface to fill a rectangle on
//! \param format : Color format to use for the surface (instead of the format
//!                 in the Surface2D)
//! \param width  : Width to use for the surface (instead of the width
//!                 in the Surface2D)
//! \param height : Height to use for the surface (instead of the height
//!                 in the Surface2D)
//! \param pitch  : Pitch to use for the surface (instead of the pitch
//!                 in the Surface2D)
//!
//! \return OK
//!
RC Lw50TwodGpuRectFill::SetSurface
(
    Surface2D*         pSurf,
    ColorUtils::Format format,
    UINT32             width,
    UINT32             height,
    UINT32             pitch
)
{
    MASSERT(pSurf);
    MASSERT(format != ColorUtils::LWFMT_NONE);
    MASSERT(width  != 0);
    MASSERT(height != 0);
    MASSERT(pitch  != 0);

    m_pSurface   = pSurf;
    m_SurfFormat = format;
    m_SurfWidth  = width;
    m_SurfHeight = height;
    m_SurfPitch  = pitch;
    if (m_pCh && pSurf)
        pSurf->BindGpuChannel(m_pCh->GetHandle());
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Fill the rectangle specified in the provided surface. Performs a
//!        single rectangle fill onto m_pSurface.  Positions and dimensions of
//!        the filled rectangle are determined by method parameters.
//!
//! \param x         : X offset from beggining of Surface2D in pixels
//! \param y         : Y offset from beggining of Surface2D in lines
//! \param width     : Width of the rectangle to be filled in pixels
//! \param height    : Height of the rectangle to be filled in lines
//! \param color     : Color of the rectangle fill
//! \param timeoutMs : Timeout when waiting for the fill to complete
//! \param flushId   : ID of the current flush
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC Lw50TwodGpuRectFill::Fill
(
    UINT32  x,
    UINT32  y,
    UINT32  width,
    UINT32  height,
    UINT32  color,
    UINT32  flushId
)
{
    RC rc;
    Printf(Tee::PriDebug, "GpuRectFill: Entering Lw50TwodGpuRectFill::Fill\n");
    CHECK_RC(Initialize(m_pTestConfig));
    m_SelectedNfyLoc = m_pSurface->GetLocation();
    CHECK_RC(InitSemaphore(m_SelectedNfyLoc));
    NotifySemaphore *pSemaphore = m_Semaphores[m_SelectedNfyLoc].get();
    CHECK_RC(m_pCh->SetContextDmaSemaphore(pSemaphore->GetCtxDmaHandleGpu(0)));
    CHECK_RC(m_pCh->SetSemaphoreOffset(pSemaphore->GetCtxDmaOffsetGpu(0)));
    CHECK_RC(WriteMethods(x,
                          y,
                          width,
                          height,
                          color,
                          flushId));
    CHECK_RC(m_pCh->Flush());
    Printf(Tee::PriDebug, "GpuRectFill: Exiting Lw50TwodGpuRectFill::Fill\n");
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Wait for the specified fill to complete
//!
//! \param flushId   : Flush ID of the rectangle fill to wait for.
//!
//! \return OK if successful, not OK otherwise
//!
RC Lw50TwodGpuRectFill::Wait(UINT32 flushId)
{
    RC rc;
    Printf(Tee::PriDebug, "GpuRectFill: Starting a wait on Sem.\n");
    if (m_Semaphores.find(m_SelectedNfyLoc) == m_Semaphores.end())
    {
        Printf(Tee::PriNormal,
               "GpuRectFill: Wait called before copy!\n");
        return RC::SOFTWARE_ERROR;
    }

    NotifySemaphore *pSemaphore = m_Semaphores[m_SelectedNfyLoc].get();

    pSemaphore->SetTriggerPayload(flushId);
    //Synchronization protocol in GpuRectFill works by autoincrementing each subsquent
    //fill operation. These are queued in channel and completed in sequence.
    //After each fill is done, gpu writes id of the fill into semaphore.
    //therefore, to be sure a fill op is done we wait for it's id, or greater one
    //to be written to semaphore.
    CHECK_RC(pSemaphore->Wait(m_pTestConfig->TimeoutMs(), &NotifySemaphore::CondGreaterEq));
    Printf(Tee::PriDebug, "GpuRectFill: Finished a wait on Sem.\n");
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Initializes a single Twod instance for a fill operation. Performes a
//!        per-instance initialization. Before this gets called, per-class
//!        initialization is already done.
//!
//! \return OK if successful, not OK otherwise
RC Lw50TwodGpuRectFill::Initialize(GpuTestConfiguration * pTestConfig)
{
    RC rc;
    Printf(Tee::PriDebug,
           "GpuRectFill: Entering Lw50TwodGpuRectFill::Initialize\n");
    if (!m_Initialized)
    {
        LwRm::Handle unusedChHandle;
        Printf(Tee::PriDebug, "GpuRectFill: Allocating TwoD.\n");
        m_pTestConfig = pTestConfig;
        CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                          &unusedChHandle,
                                                          &m_TwodAlloc));
        CHECK_RC(m_pCh->SetObject(LWA06F_SUBCHANNEL_2D, m_TwodAlloc.GetHandle()));
        Printf(Tee::PriDebug, "DMAWRAP: Finished Allocating Twod.\n");

        if (m_pSurface)
            m_pSurface->BindGpuChannel(m_pCh->GetHandle());
        m_Initialized = true;
    }
    Printf(Tee::PriDebug,
           "GpuRectFill: Exiting Lw50TwodGpuRectFill::Initialize\n");
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Initialize a semaphore in the specified memory location.
//!
//! \param SemaphoreLocation : Memory location to allocate the semaphore
//!
//! \return OK if successful, not OK otherwise
RC Lw50TwodGpuRectFill::InitSemaphore(Memory::Location semaphoreLocation)
{
    RC rc;

    if (m_Semaphores.find(semaphoreLocation) == m_Semaphores.end())
    {
        unique_ptr<NotifySemaphore> pNewSemaphore(new NotifySemaphore);
        pNewSemaphore->Setup(NotifySemaphore::ONE_WORD,
                             semaphoreLocation,
                             1);     //Single semaphore instance is needed.
        CHECK_RC(pNewSemaphore->Allocate(m_pCh, 0));
        pNewSemaphore->SetPayload(0);
        m_Semaphores[semaphoreLocation] = move(pNewSemaphore);
    }

    return OK;
}

//----------------------------------------------------------------------------
//! \brief Performs a Twod write for a rectangle fill. Fills a rectangle @Width
//!        pixels wide, @Height lines high, at starting point (@X, @Y) with
//!        color Color onto the surface. Memory layout of each the surface can
//!        be either pitch or Blocklinear. Supported Twod classes are LW50_TWOD
//!        and FERMI_TWOD_A
//!
//! \param  x       : X coordinate on dest surf, in pixels
//! \param  y       : Y coordinate on dest surf, in lines
//! \param  width   : width of the rectangle, in pixels
//! \param  height  : height of the rectangle in lines
//! \param  color   : color of the rectangle
//! \param  payload : Value to write upon completion
//!
//! \return OK if successful, not OK otherwise
RC Lw50TwodGpuRectFill::WriteMethods
(
    UINT32 x,
    UINT32 y,
    UINT32 width,
    UINT32 height,
    UINT32 color,
    UINT32 payload
)
{
    RC rc;
    Printf(Tee::PriDebug, "GpuRectFill: Entering WriteMethods.\n");

    // Do this first since it provides validation for the color format
    UINT32 dstFormat = 0;
    UINT32 pcpuFormat = 0;
    UINT32 renderFormat = 0;
    switch (m_SurfFormat)
    {
        case ColorUtils::Y8:
            dstFormat = LW502D_SET_DST_FORMAT_V_Y8;
            pcpuFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_Y8;
            renderFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_Y8;
            break;
        case ColorUtils::R5G6B5:
            dstFormat = LW502D_SET_DST_FORMAT_V_R5G6B5;
            pcpuFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_R5G6B5;
            renderFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_R5G6B5;
            break;
        case ColorUtils::VOID32:
        case ColorUtils::A8R8G8B8:
            dstFormat = LW502D_SET_DST_FORMAT_V_A8R8G8B8;
            pcpuFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8R8G8B8;
            renderFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8R8G8B8;
            break;
        case ColorUtils::A2R10G10B10:
            dstFormat = LW502D_SET_DST_FORMAT_V_A2R10G10B10;
            pcpuFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2R10G10B10;
            renderFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2R10G10B10;
            break;
        case ColorUtils::A8B8G8R8:
            dstFormat = LW502D_SET_DST_FORMAT_V_A8B8G8R8;
            pcpuFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A8B8G8R8;
            renderFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A8B8G8R8;
            break;
        case ColorUtils::A2B10G10R10:
            dstFormat = LW502D_SET_DST_FORMAT_V_A2B10G10R10;
            pcpuFormat = LW502D_SET_PIXELS_FROM_CPU_COLOR_FORMAT_V_A2B10G10R10;
            renderFormat = LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT_V_A2B10G10R10;
            break;
        default:
            MASSERT(!"Unsupported color format!");
            return RC::SOFTWARE_ERROR;
    }

    UINT64     off = m_pSurface->GetCtxDmaOffsetGpu();

    switch(m_TwodAlloc.GetClass())
    {
        case LW50_TWOD:
            CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D,
                                  LW502D_SET_DST_CONTEXT_DMA,
                                  m_pSurface->GetCtxDmaHandleGpu()));
            break;
        default:
            ;
    }
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_OFFSET_UPPER,
                          0xff & UINT32(off >> 32)));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_OFFSET_LOWER,
                          UINT32(off)));

    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_FORMAT,
                          dstFormat));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_WIDTH,
                          m_SurfWidth));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_HEIGHT,
                          m_SurfHeight));

    if (m_pSurface->GetLayout() == Surface2D::BlockLinear)
    {
        CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D,
                              LW502D_SET_DST_MEMORY_LAYOUT,
                              LW502D_SET_DST_MEMORY_LAYOUT_V_BLOCKLINEAR));

        CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_BLOCK_SIZE,
                              DRF_NUM(502D, _SET_DST_BLOCK_SIZE, _WIDTH,
                                      m_pSurface->GetLogBlockWidth()) |
                              DRF_NUM(502D, _SET_DST_BLOCK_SIZE, _HEIGHT,
                                      m_pSurface->GetLogBlockHeight()) |
                              DRF_NUM(502D, _SET_DST_BLOCK_SIZE, _DEPTH,
                                      m_pSurface->GetLogBlockDepth())));
        CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_DEPTH, 1));
        CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_LAYER, 0));
    }
    else
    {
        CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D,
                              LW502D_SET_DST_MEMORY_LAYOUT,
                              LW502D_SET_DST_MEMORY_LAYOUT_V_PITCH));
    }
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_DST_PITCH,
                          m_SurfPitch));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_SET_OPERATION,
                          LW502D_SET_OPERATION_V_SRCCOPY));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D,
                          LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE, 2,
                          LW502D_SET_PIXELS_FROM_CPU_DATA_TYPE_V_COLOR,
                          pcpuFormat));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D,
                          LW502D_SET_RENDER_SOLID_PRIM_COLOR_FORMAT,
                          renderFormat));

    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D,
                          LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH, 10,
        /* LW502D_SET_PIXELS_FROM_CPU_SRC_WIDTH */      1,
        /* LW502D_SET_PIXELS_FROM_CPU_SRC_HEIGHT */     1,
        /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_FRAC */     0,
        /* LW502D_SET_PIXELS_FROM_CPU_DX_DU_INT */      width,
        /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_FRAC */     0,
        /* LW502D_SET_PIXELS_FROM_CPU_DY_DV_INT */      height,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_FRAC */    0,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_X0_INT */     x,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_FRAC */    0,
        /* LW502D_SET_PIXELS_FROM_CPU_DST_Y0_INT */     y));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_PIXELS_FROM_CPU_DATA,
                          color));

    // Only need to flush cache on GPU/CheetAh shared surfaces with GPU caching
    // enabled to ensure that CPU and CheetAh will both see the correctly
    // modified GPU data
    bool bCacheFlushReqd = (m_TwodAlloc.GetClass() != LW50_TWOD) &&
                  (m_pSurface->GetVASpace() == Surface2D::GPUAndTegraVASpace);

    // Memory flush operations do not wait until the engine is idle so it is
    // necessary to perform them after the SemaphoreRelease (which is done with
    // WFI enabled)
    //
    // Do a fake release here to cause the engine (need to use 0 so that there
    // is no chance of this unblocking a Wait operation)
    CHECK_RC(m_pCh->SemaphoreRelease(bCacheFlushReqd ? 0 : payload));
    CHECK_RC(m_pCh->Write(LWA06F_SUBCHANNEL_2D, LW502D_NO_OPERATION, 0));

    if (bCacheFlushReqd)
    {
        CHECK_RC(m_pCh->WriteL2FlushDirty());
        CHECK_RC(m_pCh->SemaphoreRelease(payload));
    }
    CHECK_RC(m_pCh->Flush());
    Printf(Tee::PriDebug, "GpuRectFill: Exiting WriteMethods.\n");
    return rc;
}

GpuRectFill::GpuRectFill()
{
}
GpuRectFill::~GpuRectFill()
{
    Cleanup();
}

//----------------------------------------------------------------------------
//! \brief Cleanup any allocations
//!
RC GpuRectFill::Cleanup()
{
    m_Initialized = false;
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Initialize the rectangle fill (surfaces still need to be set either
//!        before or after initialization)
//!
//! \param pTestConfig : Pointer to test configuration
//!
RC GpuRectFill::Initialize(GpuTestConfiguration* pTestConfig)
{
    RC rc;

    if (m_Initialized)
    {
        Printf(Tee::PriHigh, "GpuRectFill is already initialized.\n");
        return RC::SOFTWARE_ERROR;
    }

    if (pTestConfig == NULL)
    {
        Printf(Tee::PriHigh,
               "GpuRectFill::Initialize: GpuTestConfiguration* can't be 0.\n");
        return RC::SOFTWARE_ERROR;
    }

    GpuDevice *pGpuDev = pTestConfig->GetGpuDevice();
    if (pGpuDev == NULL)
    {
        Printf(Tee::PriHigh,
               "GpuRectFill::Initialize: GpuDevice in GpuTestConfiguration "
               "can't be 0.\n");
        return RC::SOFTWARE_ERROR;
    }

    const bool useLw50 = Lw50TwodGpuRectFill::IsSupported(pGpuDev);

    // Set allow multiple channels so that the rectangle fill object gets its
    // own channel
    pTestConfig->SetAllowMultipleChannels(true);

    // Setup the channel types correctly.
    TestConfiguration::ChannelType origChannelType =
        pTestConfig->GetChannelType();
    if (useLw50 &&
        (origChannelType == TestConfiguration::TegraTHIChannel))
    {
        // If a non-cheetah 2D class is in use and the test configuration is for
        // a CheetAh channel type, then just use the newest supported class
        // which will be a non-cheetah type
        pTestConfig->SetChannelType(TestConfiguration::UseNewestAvailable);
    }

    DEFER { pTestConfig->SetChannelType(origChannelType); };

    if (useLw50)
    {
        Printf(Tee::PriDebug, "GpuRectFill: Instatiating Lw50TwoD.\n");
        m_pGpuRectFillImpl = make_unique<Lw50TwodGpuRectFill>(pTestConfig);
    }
    else
    {
        Printf(Tee::PriHigh,
               "GpuRectFill::Initialize: No supported rectangle "
               "fill method found!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pGpuRectFillImpl)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    if (m_pSurface)
    {
        rc = m_pGpuRectFillImpl->SetSurface(m_pSurface,
                                            m_SurfFormat,
                                            m_SurfWidth,
                                            m_SurfHeight,
                                            m_SurfPitch);
    }

    CHECK_RC(m_pGpuRectFillImpl->Initialize(pTestConfig));

    m_Initialized = true;
    return OK;
}

//----------------------------------------------------------------------------
//! \brief Sets the surface to use for the fills
//!
//! \param pSurf  : Pointer to surface to fill a rectangle on
//!
//! \return OK if successful, not OK otherwise
//!
RC GpuRectFill::SetSurface(Surface2D* pSurf)
{
    return SetSurface(pSurf, pSurf->GetColorFormat());
}

//----------------------------------------------------------------------------
//! \brief Sets the surface to use for the fills providing a color format to
//!        use instead of the actual surface color format
//!
//! \param pSurf  : Pointer to surface to fill a rectangle on
//! \param format : Color format to use for the surface (instead of the format
//!                 in the Surface2D)
//!
//! \return OK if successful, not OK otherwise
//!
RC GpuRectFill::SetSurface(Surface2D* pSurf, ColorUtils::Format format)
{

    return SetSurface(pSurf,
                      format,
                      pSurf->GetWidth(),
                      pSurf->GetHeight(),
                      pSurf->GetPitch());
}

//----------------------------------------------------------------------------
//! \brief Sets the surface to use for the fills providing a color format to
//!        use instead of the actual surface color format
//!
//! \param pSurf  : Pointer to surface to fill a rectangle on
//! \param format : Color format to use for the surface (instead of the format
//!                 in the Surface2D)
//! \param width  : Width to use for the surface (instead of the width
//!                 in the Surface2D)
//! \param height : Height to use for the surface (instead of the height
//!                 in the Surface2D)
//! \param pitch  : Pitch to use for the surface (instead of the pitch
//!                 in the Surface2D)
//!
//! \return OK if successful, not OK otherwise
//!
RC GpuRectFill::SetSurface
(
    Surface2D*         pSurf,
    ColorUtils::Format format,
    UINT32             width,
    UINT32             height,
    UINT32             pitch
)
{
    RC rc;
    MASSERT(NULL != pSurf);

    m_SurfFormat  = (format == ColorUtils::LWFMT_NONE) ?
                                  pSurf->GetColorFormat() : format;
    m_SurfWidth   = (width == 0)  ? pSurf->GetWidth()  : width;
    m_SurfHeight  = (height == 0) ? pSurf->GetHeight() : height;
    m_SurfPitch   = (pitch == 0)  ? pSurf->GetPitch()  : pitch;

    if (ColorUtils::PixelBytes(m_SurfFormat) != pSurf->GetBytesPerPixel())
    {
        Printf(Tee::PriHigh,
               "GpuRectFill : Color format override must be the same Bpp as "
               "the surface\n");
        return RC::SOFTWARE_ERROR;
    }

    if ((m_SurfPitch * m_SurfHeight) > pSurf->GetSize())
    {
        Printf(Tee::PriHigh,
               "GpuRectFill : Overridden surface size is larger than actual "
               "surface\n");
        return RC::SOFTWARE_ERROR;
    }

    if ((ColorUtils::PixelBytes(m_SurfFormat) * m_SurfWidth) > m_SurfPitch)
    {
        Printf(Tee::PriHigh,
               "GpuRectFill : Overridden surface width does not fit within "
               "the pitch\n");
        return RC::SOFTWARE_ERROR;
    }

    m_pSurface = pSurf;
    if (m_pGpuRectFillImpl)
    {
        rc = m_pGpuRectFillImpl->SetSurface(pSurf,
                                            m_SurfFormat,
                                            m_SurfWidth,
                                            m_SurfHeight,
                                            m_SurfPitch);
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Fill the rectangle specified in the provided surface. Performs a
//!        single rectangle fill onto m_pSurface. Positions and dimensions of
//!        the filled rectangle are determined by method parameters.
//!        Initialize() and SetSurface() must be called prior to
//!        calling Fill()
//!
//! \param x          : X offset from beggining of Surface2D in pixels
//! \param y          : Y offset from beggining of Surface2D in lines
//! \param width      : Width of the rectangle to be filled in pixels
//! \param height     : Height of the rectangle to be filled in lines
//! \param color      : Color of the rectangle fill
//! \param shouldWait : True to wait for completion of the fill
//!
//! \return OK if successful, not OK otherwise
RC GpuRectFill::Fill
(
    UINT32 x,
    UINT32 y,
    UINT32 width,
    UINT32 height,
    UINT32 color,
    bool   shouldWait
)
{
    RC rc;
    if (!m_Initialized)
    {
        Printf(Tee::PriHigh, "GpuRectFill::Fill: Not Initialized!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pGpuRectFillImpl)
    {
        Printf(Tee::PriHigh,
               "GpuRectFill::Fill: No valid GPU fill implementer!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pSurface)
    {
        Printf(Tee::PriHigh,
               "GpuRectFill::Fill: Surface not set!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (((x + width) > m_SurfWidth) || ((y + height) > m_SurfHeight))
    {
        Printf(Tee::PriHigh,
               "GpuRectFill : Requested rectangle lies outside of the "
               "surface\n");
        return RC::SOFTWARE_ERROR;
    }
    // Wait makes sure last op has finished. If we already waited
    // on previous flush, wait will return without side-effects.
    if (m_NumFlushes >= (m_NumWaits + MAX_TWOD_FLUSHES))
    {
        Printf(Tee::PriDebug,
               "GpuRectFill: Maxed out flushes, waiting. waits: %d "
               "flushes %d\n.",
               m_NumWaits,
               m_NumFlushes);
        //Arbitrarily choose to wait for half of the flushed ops.
        m_pGpuRectFillImpl->Wait(m_NumWaits + (MAX_TWOD_FLUSHES / 2));
        m_NumWaits = m_NumWaits + (MAX_TWOD_FLUSHES/2);
    }
    m_NumFlushes++;
    CHECK_RC(m_pGpuRectFillImpl->Fill(x,
                                      y,
                                      width,
                                      height,
                                      color,
                                      m_NumFlushes));
    if (shouldWait)
    {
        Printf(Tee::PriDebug, "GpuRectFill: Waiting after flushing.\n");
        CHECK_RC(Wait());
    }
    return rc;
}

//----------------------------------------------------------------------------
//! \brief Fill the provided surface with white, red, green, and blue bars.
//!        Performs a several rectangle fills onto m_pSurface.
//!
//! \param shouldWait : True to wait for completion of the fill
//!
//! \return OK if successful, not OK otherwise
RC GpuRectFill::FillRgbBars(bool shouldWait)
{
    RC rc;
    if (!m_Initialized)
    {
        Printf(Tee::PriHigh, "GpuRectFill::Fill: Not Initialized!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pGpuRectFillImpl)
    {
        Printf(Tee::PriHigh,
               "GpuRectFill::Fill: No valid GPU fill implementer!\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!m_pSurface)
    {
        Printf(Tee::PriHigh,
               "GpuRectFill::Fill: Surface not set!\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 RedCBits = 0;
    UINT32 GreenCBits = 0;
    UINT32 BlueCBits = 0;
    UINT32 AlphaCBits = 0;
    ColorUtils::CompBitsRGBA(m_SurfFormat, &RedCBits, &GreenCBits, &BlueCBits, &AlphaCBits);
    UINT32 RedShift = 0;
    UINT32 GreenShift = 0;
    UINT32 BlueShift = 0;

    UINT32 LwmulativeShift = 0;
    for (UINT32 Idx = 0; Idx < 4; Idx++)
    {
        switch (ColorUtils::ElementAt(m_SurfFormat, Idx))
        {
            case ColorUtils::elRed:
                RedShift = LwmulativeShift;
                LwmulativeShift += RedCBits;
                break;
            case ColorUtils::elGreen:
                GreenShift = LwmulativeShift;
                LwmulativeShift += GreenCBits;
                break;
            case ColorUtils::elBlue:
                BlueShift = LwmulativeShift;
                LwmulativeShift += BlueCBits;
                break;
            case ColorUtils::elAlpha:
                LwmulativeShift += AlphaCBits;
                break;
            default:
                break;
        }
    };

#define CALC_COLOR(cbits, shift) (((X << cbits) / m_SurfWidth) << shift)
#define CALC_RED   CALC_COLOR(RedCBits,     RedShift)
#define CALC_GREEN CALC_COLOR(GreenCBits, GreenShift)
#define CALC_BLUE  CALC_COLOR(BlueCBits,   BlueShift)
    UINT32 barHeight = m_SurfHeight / 4;
    for (UINT32 Y = 0; Y < m_SurfHeight; Y += barHeight)
    {
        // Adjust the height of the last bar to fill the rest of the
        // surface to make up for rounding errors
        if ((Y + (barHeight*2)) >= m_SurfHeight)
        {
            barHeight = m_SurfHeight - Y;
        }

        for (UINT32 X = 0; X < m_SurfWidth; X++)
        {
            UINT32 color = 0;
            if (Y < barHeight)
            {
                // White bars
                color = CALC_RED | CALC_GREEN | CALC_BLUE;
            }
            else if (Y < barHeight * 2)
            {
                // Red bars
                color = CALC_RED;
            }
            else if (Y < barHeight * 3)
            {
                // Green bars
                color = CALC_GREEN;
            }
            else
            {
                // Blue bars
                color = CALC_BLUE;
            }
            CHECK_RC(Fill(X, Y, 1, barHeight, color, shouldWait));
        }
    }

    return rc;
}

//----------------------------------------------------------------------------
//! \brief Waits for a flushed fill operation to complete. Performs wait on
//!        implementation-specific synchronisation primitive. Wrapper pays
//!        attention to wait before every flush, and to ignore double waits.
//!
//! \return OK if successful, not OK otherwise
RC GpuRectFill::Wait()
{
    RC rc;
    if (m_NumWaits != m_NumFlushes)
    {
        Printf(Tee::PriDebug, "GpuRectFill: flushes: %d waits: %d\n",
               m_NumWaits, m_NumFlushes);
        CHECK_RC(m_pGpuRectFillImpl->Wait(m_NumFlushes));
    }
    m_NumWaits = m_NumFlushes;
    return rc;

}
