/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/lwrm.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/fileholder.h"
#include "gpu/include/notifier.h"
#include "lwos.h"
#include "surf2d.h"
#include "surfrdwr.h"
#include "gpu/include/dmawrap.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "cheetah/dev/vic/vic_api.h"
#include "class/cl902d.h"
#include "class/cl90b5.h"
#include "class/cla06fsubch.h"  //LWA06F_SUBCHANNEL_COPY_ENG
#include "class/cla0b5.h"
#include "class/clb0b5.h"
#include "class/clb0b6.h" // LWB0B6_VIDEO_COMPOSITOR
#include "class/clb1b6.h" // LWB1B6_VIDEO_COMPOSITOR
#include "class/clc5b6.h" // LWC5B6_VIDEO_COMPOSITOR
#include "class/clc0b5.h" //PASCAL_DMA_COPY_A
#include "class/clc1b5.h" //PASCAL_DMA_COPY_B
#include "class/clc3b5.h" //VOLTA_DMA_COPY_A
#include "class/clc5b5.h" //TURING_DMA_COPY_A
#include "class/clc6b5.h" //AMPERE_DMA_COPY_A
#include "class/clc7b5.h" //AMPERE_DMA_COPY_B
#include "class/clc8b5.h" // HOPPER_DMA_COPY_A
#include "class/clc9b5.h" // BLACKWELL_DMA_COPY_A
#include "class/cle26e.h"

//Number chosen is rather arbitrary, and tested to work.
static const UINT32 MAX_CE_FLUSHES = 20;

namespace
{
    const UINT32 MAX_VIC_DIM = (1U << 14) - 16;
    typedef unique_ptr<Surface2D> Surface2DPtr;

    UINT32 Get902DSrcFormatFromSurface2DFormat(Surface2D *pSurface)
    {
        ColorUtils::Format format = pSurface->GetColorFormat();
        switch(format)
        {
            case ColorUtils::R5G6B5:                 // 16 bit rgb
                return LW902D_SET_SRC_FORMAT_V_R5G6B5;
            case ColorUtils::A8R8G8B8:               // 32 bit rgb (actually 24-bit, with transparency in first 8 bits)
                return LW902D_SET_SRC_FORMAT_V_A8R8G8B8;
            case ColorUtils::A2R10G10B10:            // 32 bit rgb (actually 30-bit, with transparency in first 2 bits)
                return LW902D_SET_SRC_FORMAT_V_A2R10G10B10;
            case ColorUtils::Z1R5G5B5:               // 15 bit rgb
                return LW902D_SET_SRC_FORMAT_V_Z1R5G5B5;
            case ColorUtils::RF16:                   // 16-bit float, red only
                return LW902D_SET_SRC_FORMAT_V_RF16;
            case ColorUtils::RF32:                   // 32-bit float, red only
                return LW902D_SET_SRC_FORMAT_V_RF32;
            case ColorUtils::RF16_GF16_BF16_AF16:    // 16-bit float, all 4 elements
                return LW902D_SET_SRC_FORMAT_V_RF16_GF16_BF16_AF16;
            case ColorUtils::RF32_GF32_BF32_AF32:    // 32-bit float, all 4 elements
                return LW902D_SET_SRC_FORMAT_V_RF32_GF32_BF32_AF32;
            case ColorUtils::Y8:                     // 8-bit unsigned brightness (mono)
                return LW902D_SET_SRC_FORMAT_V_Y8;
            case ColorUtils::A2B10G10R10:
                return LW902D_SET_SRC_FORMAT_V_A2B10G10R10;
            case ColorUtils::A8B8G8R8:
                return LW902D_SET_SRC_FORMAT_V_A8B8G8R8;
            case ColorUtils::A1R5G5B5:
                return LW902D_SET_SRC_FORMAT_V_A1R5G5B5;
            case ColorUtils::Z8R8G8B8:
                return LW902D_SET_SRC_FORMAT_V_Z8R8G8B8;
            case ColorUtils::X8R8G8B8:
                return LW902D_SET_SRC_FORMAT_V_X8R8G8B8;
            case ColorUtils::X1R5G5B5:
                return LW902D_SET_SRC_FORMAT_V_X1R5G5B5;
            case ColorUtils::AN8BN8GN8RN8:
                return LW902D_SET_SRC_FORMAT_V_AN8BN8GN8RN8;
            case ColorUtils::X8B8G8R8:
                return LW902D_SET_SRC_FORMAT_V_X8B8G8R8;
            case ColorUtils::A8RL8GL8BL8:
                return LW902D_SET_SRC_FORMAT_V_A8RL8GL8BL8;
            case ColorUtils::X8RL8GL8BL8:
                return LW902D_SET_SRC_FORMAT_V_X8RL8GL8BL8;
            case ColorUtils::A8BL8GL8RL8:
                return LW902D_SET_SRC_FORMAT_V_A8BL8GL8RL8;
            case ColorUtils::X8BL8GL8RL8:
                return LW902D_SET_SRC_FORMAT_V_X8BL8GL8RL8;
            case ColorUtils::RF32_GF32_BF32_X32:
                return LW902D_SET_SRC_FORMAT_V_RF32_GF32_BF32_X32;
            case ColorUtils::R16_G16_B16_A16:
                return LW902D_SET_SRC_FORMAT_V_R16_G16_B16_A16;
            case ColorUtils::RN16_GN16_BN16_AN16:
                return LW902D_SET_SRC_FORMAT_V_RN16_GN16_BN16_AN16;
            case ColorUtils::RF16_GF16_BF16_X16:
                return LW902D_SET_SRC_FORMAT_V_RF16_GF16_BF16_X16;
            case ColorUtils::RF32_GF32:
                return LW902D_SET_SRC_FORMAT_V_RF32_GF32;
            case ColorUtils::RF16_GF16:
                return LW902D_SET_SRC_FORMAT_V_RF16_GF16;
            case ColorUtils::R16_G16:
                return LW902D_SET_SRC_FORMAT_V_R16_G16;
            case ColorUtils::BF10GF11RF11:
                return LW902D_SET_SRC_FORMAT_V_BF10GF11RF11;
            case ColorUtils::G8R8:
                return LW902D_SET_SRC_FORMAT_V_G8R8;
            case ColorUtils::GN8RN8:
                return LW902D_SET_SRC_FORMAT_V_GN8RN8;
            case ColorUtils::RN16:
                return LW902D_SET_SRC_FORMAT_V_RN16;
            case ColorUtils::RN8:
                return LW902D_SET_SRC_FORMAT_V_RN8;
            case ColorUtils::A8:
                return LW902D_SET_SRC_FORMAT_V_A8;
            default:
                break;
        }

        // In case that we don't have an exactly mapped 902D format,
        // since we are doing format colwersion for the purpose of pixel copy,
        // choose a format with the same pixel bytes.
        // As long as the source and the destination have the same format,
        // we will be fine.
        switch (ColorUtils::PixelBytes(format))
        {
            case 1:
                return LW902D_SET_SRC_FORMAT_V_Y8;
            case 2:
                return LW902D_SET_SRC_FORMAT_V_Y16;
            case 4:
                return LW902D_SET_SRC_FORMAT_V_A8R8G8B8;
            case 8:
                return LW902D_SET_SRC_FORMAT_V_R16_G16_B16_A16;
            case 16:
                return LW902D_SET_SRC_FORMAT_V_RF32_GF32_BF32_AF32;
            default:
                break;
        }

        return 0;
    }

    UINT32 Get902DDstFormatFromSurface2DFormat(Surface2D *pSurface)
    {
        ColorUtils::Format format = pSurface->GetColorFormat();
        switch(format)
        {
            case ColorUtils::R5G6B5:                 // 16 bit rgb
                return LW902D_SET_DST_FORMAT_V_R5G6B5;
            case ColorUtils::A8R8G8B8:               // 32 bit rgb (actually 24-bit, with transparency in first 8 bits)
                return LW902D_SET_DST_FORMAT_V_A8R8G8B8;
            case ColorUtils::A2R10G10B10:            // 32 bit rgb (actually 30-bit, with transparency in first 2 bits)
                return LW902D_SET_DST_FORMAT_V_A2R10G10B10;
            case ColorUtils::Z1R5G5B5:               // 15 bit rgb
                return LW902D_SET_DST_FORMAT_V_Z1R5G5B5;
            case ColorUtils::RF16:                   // 16-bit float, red only
                return LW902D_SET_DST_FORMAT_V_RF16;
            case ColorUtils::RF32:                   // 32-bit float, red only
                return LW902D_SET_DST_FORMAT_V_RF32;
            case ColorUtils::RF16_GF16_BF16_AF16:    // 16-bit float, all 4 elements
                return LW902D_SET_DST_FORMAT_V_RF16_GF16_BF16_AF16;
            case ColorUtils::RF32_GF32_BF32_AF32:    // 32-bit float, all 4 elements
                return LW902D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32;
            case ColorUtils::Y8:                     // 8-bit unsigned brightness (mono)
                return LW902D_SET_DST_FORMAT_V_Y8;
            case ColorUtils::A2B10G10R10:
                return LW902D_SET_DST_FORMAT_V_A2B10G10R10;
            case ColorUtils::A8B8G8R8:
                return LW902D_SET_DST_FORMAT_V_A8B8G8R8;
            case ColorUtils::A1R5G5B5:
                return LW902D_SET_DST_FORMAT_V_A1R5G5B5;
            case ColorUtils::Z8R8G8B8:
                return LW902D_SET_DST_FORMAT_V_Z8R8G8B8;
            case ColorUtils::X8R8G8B8:
                return LW902D_SET_DST_FORMAT_V_X8R8G8B8;
            case ColorUtils::X1R5G5B5:
                return LW902D_SET_DST_FORMAT_V_X1R5G5B5;
            case ColorUtils::AN8BN8GN8RN8:
                return LW902D_SET_DST_FORMAT_V_AN8BN8GN8RN8;
            case ColorUtils::X8B8G8R8:
                return LW902D_SET_DST_FORMAT_V_X8B8G8R8;
            case ColorUtils::A8RL8GL8BL8:
                return LW902D_SET_DST_FORMAT_V_A8RL8GL8BL8;
            case ColorUtils::X8RL8GL8BL8:
                return LW902D_SET_DST_FORMAT_V_X8RL8GL8BL8;
            case ColorUtils::A8BL8GL8RL8:
                return LW902D_SET_DST_FORMAT_V_A8BL8GL8RL8;
            case ColorUtils::X8BL8GL8RL8:
                return LW902D_SET_DST_FORMAT_V_X8BL8GL8RL8;
            case ColorUtils::RF32_GF32_BF32_X32:
                return LW902D_SET_DST_FORMAT_V_RF32_GF32_BF32_X32;
            case ColorUtils::R16_G16_B16_A16:
                return LW902D_SET_DST_FORMAT_V_R16_G16_B16_A16;
            case ColorUtils::RN16_GN16_BN16_AN16:
                return LW902D_SET_DST_FORMAT_V_RN16_GN16_BN16_AN16;
            case ColorUtils::RF16_GF16_BF16_X16:
                return LW902D_SET_DST_FORMAT_V_RF16_GF16_BF16_X16;
            case ColorUtils::RF32_GF32:
                return LW902D_SET_DST_FORMAT_V_RF32_GF32;
            case ColorUtils::RF16_GF16:
                return LW902D_SET_DST_FORMAT_V_RF16_GF16;
            case ColorUtils::R16_G16:
                return LW902D_SET_DST_FORMAT_V_R16_G16;
            case ColorUtils::BF10GF11RF11:
                return LW902D_SET_DST_FORMAT_V_BF10GF11RF11;
            case ColorUtils::G8R8:
                return LW902D_SET_DST_FORMAT_V_G8R8;
            case ColorUtils::GN8RN8:
                return LW902D_SET_DST_FORMAT_V_GN8RN8;
            case ColorUtils::RN16:
                return LW902D_SET_DST_FORMAT_V_RN16;
            case ColorUtils::RN8:
                return LW902D_SET_DST_FORMAT_V_RN8;
            case ColorUtils::A8:
                return LW902D_SET_DST_FORMAT_V_A8;
            default:
                break;
        }

        // In case that we don't have an exactly mapped 902D format,
        // since we are doing format colwersion for the purpose of pixel copy,
        // choose a format with the same pixel bytes.
        // As long as the source and the destination have the same format,
        // we will be fine.
        switch (ColorUtils::PixelBytes(format))
        {
            case 1:
                return LW902D_SET_DST_FORMAT_V_Y8;
            case 2:
                return LW902D_SET_DST_FORMAT_V_Y16;
            case 4:
                return LW902D_SET_SRC_FORMAT_V_A8R8G8B8;
            case 8:
                return LW902D_SET_DST_FORMAT_V_R16_G16_B16_A16;
            case 16:
                return LW902D_SET_DST_FORMAT_V_RF32_GF32_BF32_AF32;
            default:
                break;
        }

        return 0;
    }
}

//! \class IDmaStrategy
//!
//! \brief Interface for various DMA transfer implementations.
//!
//! This class provides interface for various classes that
//! provide memory copying services. They are plugged into
//! DmaWrapper based on capability/preference.
class IDmaStrategy
{
public:
    virtual ~IDmaStrategy(){};
    virtual RC SetSurfaces
    (
        Surface2D* src,
        Surface2D* dst
    ) = 0;
    virtual RC SetCtxDmaOffsets
    (
        UINT64 srcCtxDmaOffset,
        UINT64 dstCtxDmaOffset
    ) = 0;
    virtual RC Copy
    (
        UINT32 SrcOriginX,  //!< Starting src X, in bytes not pixels.
        UINT32 SrcOriginY,  //!< Starting src Y, in lines.
        UINT32 WidthBytes,  //!< Width of copied rect, in bytes.
        UINT32 Height,      //!< Height of copied rect, in bytes
        UINT32 DstOriginX,  //!< Starting dst X, in bytes not pixels.
        UINT32 DstOriginY,  //!< Starting dst Y, in lines.
        FLOAT64 TimeoutMs,  //!< Timeout for dma operation
        UINT32 SubdevNum,   //!< Subdevice number we are working on
        bool flushChannel,  //!< flush the channel
        bool writeSemaphores,//!<Write Semaphores after copy completion?
        UINT32 FlushId      //!< Identifier of copy operation.
                            //!< Used for waiting on operation to complete.
                            //!< Current CE implementation expects these to
                            //!< be unique and incrementing to function
                            //!< properly.
    ) = 0;

    //! \brief Waits for the DmaCopy operation to complete.
    //!
    //! Waits for particular dma implementation to signal completion.
    //! If particular class supports custom semaphore values,
    //! FlushId parameter specifies the value to wait on.
    //! Wait is expected to return when value of FlushId or greater
    //! gets written into semaphore surface.
    //! In case dma class doesn't support custom semaphore values,
    //! FlushId parameter is ignored, and Wait returns after the implementation
    //! specific signal gets triggered (in case of MemToMem notified bit gets flipped)
    virtual RC Wait
    (
        FLOAT64 TimeoutMs,  //!< Timeout for dma operation
        UINT32 FlushId      //!< Smallest value to return on.
    ) = 0;
    virtual RC IsCopyComplete(UINT32 FlushId, bool *pComplete)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual RC Initialize() = 0;
    virtual void SetCopyMask(UINT32 copyMask) { MASSERT(0); }
    virtual void SetUsePipelinedCopies(bool bUsePipelining) { MASSERT(0); }
    virtual void SetFlush(bool bEnable) { MASSERT(0); }
    virtual void SetUsePhysicalCopy(bool bSrcPhysical, bool bDstPhysical) { MASSERT(0); }
    virtual void SetPhysicalCopyTarget(Memory::Location srcLoc, Memory::Location dstLoc)
        { MASSERT(0); }
    virtual UINT32 GetPitchAlignRequirement() const = 0;
    virtual RC SaveCopyTime(bool bSaveCopyTime)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual RC GetCopyTimeUs(UINT64 *pStartTimeUs, UINT64 *pEndTimeUs)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    virtual void WriteStartTimeSemaphore(bool bWrite) { MASSERT(0); }
};

//! \class CopyEngineStrategy
//!
//! \brief Copies memory chunks using copy engine device on gpu.
//!
//! Current CE implementation only supports Pitch surfaces.
//! Only one subdevice is suported. In case of multiple subdevc,
//! an error is returned.
class CopyEngineStrategy : public IDmaStrategy
{
public:
    CopyEngineStrategy
    (
        GpuTestConfiguration * pTestConfig,
        Channel *   pCh,
        Memory::Location NotifierLocation,
        UINT32      InstanceNum,
        GpuDevice * pGpuDev
    );
    virtual ~CopyEngineStrategy();
    static bool IsSupported(GpuDevice* pGpuDev)
    {
        DmaCopyAlloc alloc;
        return alloc.IsSupported(pGpuDev);
    }
    RC SetSurfaces(Surface2D *src, Surface2D *dst);
    RC SetCtxDmaOffsets
    (
        UINT64 srcCtxDmaOffset,
        UINT64 dstCtxDmaOffset
    );
    RC IsCopyComplete(UINT32 FlushId, bool *pComplete);
    void SetCopyMask(UINT32 copyMask);
    void SetUsePipelinedCopies(bool bUsePipelining);
    void SetFlush(bool bEnable);
    void SetUsePhysicalCopy(bool bSrcPhysical, bool bDstPhysical);
    void SetPhysicalCopyTarget(Memory::Location srcLoc, Memory::Location dstLoc);
    RC Copy
    (
        UINT32 SrcOriginX,   //!< Starting src X, in bytes not pixels.
        UINT32 SrcOriginY,   //!< Starting src Y, in lines.
        UINT32 WidthBytes,   //!< Width of copied rect, in bytes.
        UINT32 Height,       //!< Height of copied rect, in bytes
        UINT32 DstOriginX,   //!< Starting dst X, in bytes not pixels.
        UINT32 DstOriginY,   //!< Starting dst Y, in lines.
        FLOAT64 TimeoutMs,   //!< Timeout for dma operation
        UINT32 SubdevNum,    //!< Unused in CopyEngine.
        bool flushChannel,   //!< flush the channel
        bool writeSemaphores,//!< Write Semaphores after copy completion?
        UINT32 FlushId
    );
    RC Wait
    (
        FLOAT64 TimeoutMs,
        UINT32 FlushId
    );

    virtual RC Initialize();
    RC GetCopyTimeUs(UINT64 *pStartTimeUs, UINT64 *pEndTimeUs);
    RC SaveCopyTime(bool bSaveCopyTime);
    void WriteStartTimeSemaphore(bool bWrite);

private:
    GpuDevice* GetGpuDev(){return m_pGpuDev;}
    GpuSubdevice* GetGpuSubdev(){return 0;}

    RC InitSemaphore(Memory::Location SemaphoreLocation);
    UINT64 GetCtxSurfaceDmaOffset
    (
        Surface2D* surf,
        UINT64 offsetOverride,
        UINT32 X,
        UINT32 Y
    );
    RC WriteMethods
    (
        UINT32 SrcOriginX,
        UINT32 SrcOriginY,
        UINT32 DstOriginX,
        UINT32 DstOriginY,
        UINT32 Width,
        UINT32 Height,
        UINT32 Payload
    );
    RC Write90b5Methods
    (
        UINT32 SrcOriginX,
        UINT32 SrcOriginY,
        UINT32 DstOriginX,
        UINT32 DstOriginY,
        UINT32 Width,
        UINT32 Height,
        UINT32 Payload
    );

    virtual UINT32 GetPitchAlignRequirement() const
    {
        return 1;
    }

    Surface2D* m_SrcSurf;
    Surface2D* m_DstSurf;
    UINT64     m_SrcCtxDmaOffset;
    UINT64     m_DstCtxDmaOffset;

    bool m_Initialized;
    bool m_WriteSemaphores;

    DmaCopyAlloc    m_DmaCopyAlloc;
    typedef std::map<Memory::Location, NotifySemaphore*> MapLocationSemaphore;
    MapLocationSemaphore   m_Semaphores;
    GpuTestConfiguration * m_pTestConfig;
    Channel *        m_pCh;
    GpuDevice *      m_pGpuDev;
    UINT32           m_EngineInstance;
    Memory::Location m_SelectedNfyLoc;
    INT08            m_CopyMask;
    bool             m_EnableCopyWithHoles;
    bool             m_UsePiplinedCopies;
    bool             m_DisableFlush;
    bool             m_bSrcPhysical;
    Memory::Location m_SrcPhysicalTarget;
    bool             m_bDstPhysical;
    Memory::Location m_DstPhysicalTarget;

    Surface2DPtr    m_StartSemaSurf;
    UINT64          m_CopyStartTimeUs;
    UINT64          m_CopyEndTimeUs;
    bool            m_SaveCopyTime;
    bool            m_WriteStartTimeSemaphore;
    bool            m_bAllocatedChannel;
};

//! \class TWODStrategy
//! \brief Wrapperaround Copy using 2D engine
class TWODStrategy : public IDmaStrategy
{
public:
    TWODStrategy
    (
        GpuTestConfiguration * pTestConfig,
        Channel              * pCh,
        GpuDevice            * pGpuDev
    );
    virtual ~TWODStrategy();
    static bool IsSupported(GpuDevice* pGpuDev)
    {
        return LwRmPtr()->IsClassSupported(FERMI_TWOD_A, pGpuDev);
    }
    RC SetSurfaces(Surface2D* src, Surface2D* dst);
    RC SetCtxDmaOffsets
    (
        UINT64 srcCtxDmaOffset,
        UINT64 dstCtxDmaOffset
    );
    RC Copy
    (
        UINT32 srcOriginX,
        UINT32 srcOriginY,
        UINT32 width,
        UINT32 height,
        UINT32 dstOriginX,
        UINT32 dstOriginY,
        FLOAT64 timeoutMs,
        UINT32 subdevNum,   //!< Unused
        bool flushChannel,  //!< Unused
        bool writeSemaphores,//!< Unused
        UINT32 flushid       //!< Unused
    );
    RC Wait
    (
        FLOAT64 timeoutms,
        UINT32 flushId      //!< Unused.
    );
    RC Initialize();
    RC GetCopyTimeUs(UINT64 *pStartTimeUs, UINT64 *pEndTimeUs);
    RC SaveCopyTime(bool bSaveCopyTime);
    void WriteStartTimeSemaphore(bool bWrite);

private:
    GpuDevice* GetGpuDev() const { return m_pGpuDev; }
    GpuSubdevice* GetGpuSubdev() const { return 0; }

    virtual UINT32 GetPitchAlignRequirement() const
    {
        return 1;
    }

    bool                   m_Initialized;
    GpuTestConfiguration * m_pTestConfig;
    Channel              * m_pCh;
    Surface2D            * m_pSrc;
    Surface2D            * m_pDest;
    UINT64                 m_SrcCtxDmaOffset;
    UINT64                 m_DstCtxDmaOffset;
    GpuDevice            * m_pGpuDev;
    TwodAlloc              m_twod;

    Surface2DPtr m_StartSemaSurf;
    Surface2DPtr m_EndSemaSurf;
    UINT64       m_CopyStartTimeUs;
    UINT64       m_CopyEndTimeUs;
    bool         m_SaveCopyTime;
    bool         m_WriteStartTimeSemaphore;
    bool         m_bAllocatedChannel;
};

TWODStrategy::TWODStrategy
(
    GpuTestConfiguration * pTestConfig,
    Channel              * pCh,
    GpuDevice            * pGpuDev
)
: m_Initialized(false)
, m_pTestConfig(pTestConfig)
, m_pCh(pCh)
, m_pSrc(0)
, m_pDest(0)
, m_SrcCtxDmaOffset(0)
, m_DstCtxDmaOffset(0)
, m_pGpuDev(pGpuDev)
, m_StartSemaSurf(nullptr)
, m_EndSemaSurf(nullptr)
, m_CopyStartTimeUs(0)
, m_CopyEndTimeUs(0)
, m_SaveCopyTime(false)
, m_WriteStartTimeSemaphore(true)
, m_bAllocatedChannel(false)
{
}

TWODStrategy::~TWODStrategy()
{
    if (m_pTestConfig && m_pCh && m_bAllocatedChannel)
        m_pTestConfig->FreeChannel(m_pCh);
}

RC TWODStrategy::SetSurfaces(Surface2D* src, Surface2D* dst)
{
    MASSERT(src);
    MASSERT(dst);
    m_pSrc = src;
    m_pDest = dst;
    if (m_pCh)
    {
        m_pSrc->BindGpuChannel(m_pCh->GetHandle());
        m_pDest->BindGpuChannel(m_pCh->GetHandle());
    }
    return OK;
}

RC TWODStrategy::SetCtxDmaOffsets(UINT64 srcCtxDmaOffset, UINT64 dstCtxDmaOffset)
{
    m_SrcCtxDmaOffset = srcCtxDmaOffset;
    m_DstCtxDmaOffset = dstCtxDmaOffset;
    return OK;
}

RC TWODStrategy::Wait(FLOAT64 timeoutMs, UINT32)
{
    RC rc;
    CHECK_RC(m_pCh->WaitIdle(timeoutMs));
    if (m_SaveCopyTime)
    {
        MASSERT(m_StartSemaSurf);
        UINT08 *pSemStart = static_cast<UINT08 *>(m_StartSemaSurf->GetAddress());
        m_CopyStartTimeUs = (MEM_RD64(static_cast<UINT08 *>(pSemStart) + 8)) / 1000ULL;

        MASSERT(m_EndSemaSurf);
        UINT08 *pSemEnd = static_cast<UINT08 *>(m_EndSemaSurf->GetAddress());
        m_CopyEndTimeUs = (MEM_RD64(static_cast<UINT08 *>(pSemEnd) + 8)) / 1000ULL;
    }

    return rc;
}

RC TWODStrategy::Copy
(
        UINT32 srcOriginX,
        UINT32 srcOriginY,
        UINT32 widthBytes,
        UINT32 height,
        UINT32 dstOriginX,
        UINT32 dstOriginY,
        FLOAT64 timeoutMs,
        UINT32 subdevNum,   //!< Unused
        bool flushChannel,  //!< Unused
        bool writeSemaphores,//!< Unused
        UINT32 flushid       //!< Unused
)
{
    RC rc;
    CHECK_RC(Initialize());

    // Verify the format support
    UINT32 DstFormat = Get902DDstFormatFromSurface2DFormat(m_pDest);
    UINT32 SrcFormat = Get902DSrcFormatFromSurface2DFormat(m_pSrc);

    if (DstFormat == 0 || SrcFormat == 0 || (m_pSrc->GetColorFormat() != m_pDest->GetColorFormat()))
    {
        return RC::UNSUPPORTED_COLORFORMAT;
    }

    if (m_SaveCopyTime && m_WriteStartTimeSemaphore)
    {
        MASSERT(m_StartSemaSurf);
        m_pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelWithTime | Channel::FlagSemRelWithWFI);
        CHECK_RC(m_pCh->SetSemaphoreOffset(m_StartSemaSurf->GetCtxDmaOffsetGpu()));
        CHECK_RC(m_pCh->SemaphoreRelease(0));
    }

    const UINT32 twod = LWA06F_SUBCHANNEL_2D;

    if (Surface::BlockLinear == m_pSrc->GetLayout())
    {
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION,
                              LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION_V_PROMOTE_TO_4));
    }
    else
    {
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION,
                              LW902D_SET_PIXELS_FROM_MEMORY_SECTOR_PROMOTION_V_NO_PROMOTION));
    }

    // Set src and dest surface layout (pitch vs. BL)
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_MEMORY_LAYOUT,
            (Surface::BlockLinear == m_pSrc->GetLayout())
                ? LW902D_SET_SRC_MEMORY_LAYOUT_V_BLOCKLINEAR
                : LW902D_SET_SRC_MEMORY_LAYOUT_V_PITCH));
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_MEMORY_LAYOUT,
            (Surface::BlockLinear == m_pDest->GetLayout())
                ? LW902D_SET_DST_MEMORY_LAYOUT_V_BLOCKLINEAR
                : LW902D_SET_DST_MEMORY_LAYOUT_V_PITCH));

    if (Surface::BlockLinear == m_pSrc->GetLayout())
    {
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_BLOCK_SIZE,
                              DRF_NUM(902D, _SET_SRC_BLOCK_SIZE, _HEIGHT,
                                      m_pSrc->GetLogBlockHeight()) |
                              DRF_NUM(902D, _SET_SRC_BLOCK_SIZE, _DEPTH,
                                      m_pSrc->GetLogBlockDepth())));
    }
    if (Surface::BlockLinear == m_pDest->GetLayout())
    {
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_BLOCK_SIZE,
                              DRF_NUM(902D, _SET_DST_BLOCK_SIZE, _HEIGHT,
                                      m_pDest->GetLogBlockHeight()) |
                              DRF_NUM(902D, _SET_DST_BLOCK_SIZE, _DEPTH,
                                      m_pDest->GetLogBlockDepth())));
    }

    // Set src and dst surface and transfer sizes
    if (Surface::Pitch == m_pSrc->GetLayout())
    {
        // Do not set pitch for blocklinear.
        // Or risk GPU exception.
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_PITCH,
                m_pSrc->GetPitch()));
    }
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_WIDTH,
            m_pSrc->GetWidth()));
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_DEPTH,
            m_pSrc->GetDepth()));

    if (Surface::Pitch == m_pDest->GetLayout())
    {
        // Do not set pitch for blocklinear.
        // Or risk GPU exception.
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_PITCH,
                m_pDest->GetPitch()));
    }
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_WIDTH,
            m_pDest->GetWidth()));
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_DEPTH,
            m_pDest->GetDepth()));
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_LAYER,
            0));

    // Set operation to copy
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_OPERATION,
            LW902D_SET_OPERATION_V_SRCCOPY));

    // Set src and dest surface format
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_FORMAT, SrcFormat));
    CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_FORMAT, DstFormat));

    UINT64 srcOffset64 = m_SrcCtxDmaOffset;
    UINT64 dstOffset64 = m_DstCtxDmaOffset;
    if (!srcOffset64)
        srcOffset64 = m_pSrc->GetCtxDmaOffsetGpu();
    if (!dstOffset64)
        dstOffset64 = m_pDest->GetCtxDmaOffsetGpu();

    const UINT32 chunkRem  = (height % TwodAlloc::MAX_COPY_HEIGHT);
    const UINT32 numChunks = (height / TwodAlloc::MAX_COPY_HEIGHT) + (chunkRem ? 1 : 0);

    srcOffset64 += ALIGN_DOWN(srcOriginY, TwodAlloc::MAX_COPY_HEIGHT);
    dstOffset64 += ALIGN_DOWN(dstOriginY, TwodAlloc::MAX_COPY_HEIGHT);

    srcOriginY %= TwodAlloc::MAX_COPY_HEIGHT;
    dstOriginY %= TwodAlloc::MAX_COPY_HEIGHT;

    const UINT64 chunkSize = static_cast<UINT64>(TwodAlloc::MAX_COPY_HEIGHT) * widthBytes;
    for (UINT32 lwrChunk = 0; lwrChunk < numChunks; lwrChunk++)
    {
        const bool bLastChunk = (lwrChunk == (numChunks - 1));

        CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_HEIGHT,
                 bLastChunk ? chunkRem : TwodAlloc::MAX_COPY_HEIGHT));
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_HEIGHT,
                 bLastChunk ? chunkRem : TwodAlloc::MAX_COPY_HEIGHT));

        // Arbitrary offsets do not work with twoD, offsets must
        // point to the start of a GOB (for block linear) or be a multiple  of 128
        // for all other formats.  Just point to the start of the surface and then
        // use methods to provide actual offset
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_SRC_OFFSET_UPPER, 2,
                static_cast<UINT32>(srcOffset64>>32),
                static_cast<UINT32>(srcOffset64)));
        CHECK_RC(m_pCh->Write(twod, LW902D_SET_DST_OFFSET_UPPER, 2,
                static_cast<UINT32>(dstOffset64>>32),
                static_cast<UINT32>(dstOffset64)));
        // Previous methods only describe the surface itself, not how to perform the
        // transfer.  Setup the transfer so that one pixel on input equals one pixel
        // on the output
        CHECK_RC(m_pCh->Write(twod,
                              LW902D_SET_PIXELS_FROM_MEMORY_DST_X0,
                              11,
                              dstOriginX / m_pDest->GetBytesPerPixel(),           // LW902D_SET_PIXELS_FROM_MEMORY_DST_X0       //$
                              dstOriginY,                                         // LW902D_SET_PIXELS_FROM_MEMORY_DST_Y0       //$
                              widthBytes/m_pSrc->GetBytesPerPixel(),              // LW902D_SET_PIXELS_FROM_MEMORY_DST_WIDTH    //$
                              bLastChunk ? chunkRem : TwodAlloc::MAX_COPY_HEIGHT, // LW902D_SET_PIXELS_FROM_MEMORY_DST_HEIGHT   //$
                              0,                                                  // LW902D_SET_PIXELS_FROM_MEMORY_DU_DX_FRAC   //$
                              1,                                                  // LW902D_SET_PIXELS_FROM_MEMORY_DU_DX_INT    //$
                              0,                                                  // LW902D_SET_PIXELS_FROM_MEMORY_DV_DY_FRAC   //$
                              1,                                                  // LW902D_SET_PIXELS_FROM_MEMORY_DV_DY_INT    //$
                              0,                                                  // LW902D_SET_PIXELS_FROM_MEMORY_SRC_X0_FRAC  //$
                              srcOriginX / m_pSrc->GetBytesPerPixel(),            // LW902D_SET_PIXELS_FROM_MEMORY_SRC_X0_INT   //$
                              0));                                                // LW902D_SET_PIXELS_FROM_MEMORY_SRC_Y0_FRAC  //$

        // Send blit operation method
        CHECK_RC(m_pCh->Write(twod, LW902D_PIXELS_FROM_MEMORY_SRC_Y0_INT, srcOriginY));
        srcOffset64 += chunkSize;
        dstOffset64 += chunkSize;
    }

    if (m_SaveCopyTime)
    {
        MASSERT(m_EndSemaSurf);
        m_pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelWithTime | Channel::FlagSemRelWithWFI);
        CHECK_RC(m_pCh->SetSemaphoreOffset(m_EndSemaSurf->GetCtxDmaOffsetGpu()));
        CHECK_RC(m_pCh->SemaphoreRelease(0));
    }

    // Flush the channel
    CHECK_RC(m_pCh->Flush());

    return OK;
}

//! \brief Initializes a single TWOD instance.
//! Performes a per-instance initialization. Before this gets called,
//! per-class initialization is already done.
//! \return RC
RC TWODStrategy::Initialize()
{
    RC rc;
    Printf(Tee::PriDebug, "DMAWRAP: Entering TWODStrategy::Initialize\n");

    if (!m_Initialized)
    {
        if (!m_pCh)
        {
            LwRm::Handle hCh;
            CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                              &hCh,
                                                              &m_twod));
            m_bAllocatedChannel = true;
        }
        else if (m_pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID) &&
                 (m_pCh->GetEngineId() != LW2080_ENGINE_TYPE_GRAPHICS))
        {
            Printf(Tee::PriError,
                   "%s : Externally provided channel is not compatible with 2D\n",
                   __FUNCTION__);
            return RC::SOFTWARE_ERROR;
        }

        if (m_pSrc)
            m_pSrc->BindGpuChannel(m_pCh->GetHandle());
        if (m_pDest)
            m_pDest->BindGpuChannel(m_pCh->GetHandle());

        // Set 2D engine on our subchannel
        m_pCh->SetObject(LWA06F_SUBCHANNEL_2D, m_twod.GetHandle());
        m_Initialized = true;
    }

    Printf(Tee::PriDebug, "DMAWRAP: Exiting TWODStrategy::Initialize\n");
    return OK;
}

RC TWODStrategy::GetCopyTimeUs(UINT64 *pStartTimeUs, UINT64 *pEndTimeUs)
{
    *pStartTimeUs = m_CopyStartTimeUs;
    *pEndTimeUs = m_CopyEndTimeUs;
    return OK;
}

RC TWODStrategy::SaveCopyTime(bool bSaveCopyTime)
{
    RC rc;

    m_SaveCopyTime = bSaveCopyTime;
    if (!m_SaveCopyTime)
    {
        return rc;
    }

    if (!m_StartSemaSurf)
    {
        m_StartSemaSurf = make_unique<Surface2D>();
        m_StartSemaSurf->SetWidth(4);
        m_StartSemaSurf->SetHeight(1);
        m_StartSemaSurf->SetColorFormat(ColorUtils::VOID8);
        m_StartSemaSurf->SetLocation(Memory::NonCoherent);
        m_StartSemaSurf->SetProtect(Memory::ReadWrite);
        m_StartSemaSurf->SetLayout(Surface2D::Pitch);

        CHECK_RC(m_StartSemaSurf->Alloc(GetGpuDev()));
        CHECK_RC(m_StartSemaSurf->Fill(0));
        CHECK_RC(m_StartSemaSurf->Map());

        m_StartSemaSurf->BindGpuChannel(m_pCh->GetHandle());
    }

    if (!m_EndSemaSurf)
    {
        m_EndSemaSurf = make_unique<Surface2D>();
        m_EndSemaSurf->SetWidth(4);
        m_EndSemaSurf->SetHeight(1);
        m_EndSemaSurf->SetColorFormat(ColorUtils::VOID8);
        m_EndSemaSurf->SetLocation(Memory::NonCoherent);
        m_EndSemaSurf->SetProtect(Memory::ReadWrite);
        m_EndSemaSurf->SetLayout(Surface2D::Pitch);

        CHECK_RC(m_EndSemaSurf->Alloc(GetGpuDev()));
        CHECK_RC(m_EndSemaSurf->Fill(0));
        CHECK_RC(m_EndSemaSurf->Map());

        m_EndSemaSurf->BindGpuChannel(m_pCh->GetHandle());
    }

    return rc;
}

void TWODStrategy::WriteStartTimeSemaphore(bool bWrite)
{
    m_WriteStartTimeSemaphore = bWrite;
}

struct VICDmaWrapper::Impl
{
    Channel*                pCh;
    GpuDevice*              pGpuDev;
    FLOAT64                 timeoutMs;
    VICDmaWrapper::SurfDesc srcSurf;
    VICDmaWrapper::SurfDesc dstSurf;
    bool                    bChanInit;

    UINT32                  vicClass;
    Surface2D               filterSurface;
    Surface2D               configSurface;

    enum { OLDEST_SUPPORTED_CLASS = LWB0B6_VIDEO_COMPOSITOR };
    enum { NEWEST_SUPPORTED_CLASS = LWC5B6_VIDEO_COMPOSITOR };

    Impl()
        : pCh(nullptr)
        , pGpuDev(nullptr)
        , timeoutMs(0)
        , bChanInit(false)
        , vicClass(0)
    {
        memset(&srcSurf, 0, sizeof(srcSurf));
        memset(&dstSurf, 0, sizeof(dstSurf));
    }
    RC AllocDataSurface(Surface2D* pSurface, const void* pData, size_t size);
    RC InitVic4();
    UINT32 GetConfigSize() const;
    static UINT32 GetCacheWidth(const VICDmaWrapper::SurfDesc& surf);
    static Vic4::BLK_KIND GetBlkKind(Surface2D::Layout layout);
    static Vic4::PIXELFORMAT GetPixelFormat(ColorUtils::Format color);
};

bool VICDmaWrapper::SurfDesc::operator==(const SurfDesc& other) const
{
    return width        == other.width        &&
           copyWidth    == other.copyWidth    &&
           copyHeight   == other.copyHeight   &&
           layout       == other.layout       &&
           logBlkHeight == other.logBlkHeight &&
           colorFormat  == other.colorFormat  &&
           mappingType  == other.mappingType;
}

VICDmaWrapper::SurfDesc VICDmaWrapper::SurfDesc::FromSurface2D(const Surface2D& surf)
{
    SurfDesc desc;

    desc.width        = surf.GetPitch() / surf.GetBytesPerPixel();
    desc.copyWidth    = desc.width;
    desc.copyHeight   = surf.GetHeight();
    desc.layout       = surf.GetLayout();
    desc.logBlkHeight = surf.GetLogBlockHeight();
    desc.colorFormat  = surf.GetColorFormat();
    desc.mappingType  = Channel::MapDefault;

    switch (desc.layout)
    {
        case Surface2D::Pitch:
            MASSERT(desc.mappingType != Channel::MapBlockLinear);
            if (desc.mappingType == Channel::MapDefault)
                desc.mappingType = Channel::MapPitch;
            break;

        case Surface2D::BlockLinear:
            MASSERT(desc.mappingType != Channel::MapPitch);
            if (desc.mappingType == Channel::MapDefault)
                desc.mappingType = Channel::MapBlockLinear;
            break;

        default:
            MASSERT(!"Unsupported surface layout!");
            break;
    }

    return desc;
}

VICDmaWrapper::VICDmaWrapper()
{
}

VICDmaWrapper::~VICDmaWrapper()
{
    Free();
}

RC VICDmaWrapper::Free()
{
    RC rc;
    if (m_pImpl.get())
    {
        CHECK_RC(m_pImpl->pCh->WaitIdle(m_pImpl->timeoutMs));
        m_pImpl->filterSurface.Free();
        m_pImpl->configSurface.Free();
        m_pImpl.reset(nullptr);
    }
    return rc;
}

bool VICDmaWrapper::IsSupported(GpuDevice* pGpuDev)
{
    VideoCompositorAlloc vicAlloc;
    vicAlloc.SetOldestSupportedClass(Impl::OLDEST_SUPPORTED_CLASS);
    vicAlloc.SetNewestSupportedClass(Impl::NEWEST_SUPPORTED_CLASS);
    return vicAlloc.IsSupported(pGpuDev);
}

RC VICDmaWrapper::Initialize
(
    Channel*   pCh,
    GpuDevice* pGpuDev,
    FLOAT64    timeoutMs,
    SurfDesc   srcSurf,
    SurfDesc   dstSurf
)
{
    RC rc;

    MASSERT(pCh);

    // Dimensions fields have limited width.
    // If image width equals to copied rectangle width (width==copyWidth),
    // then allow input width or height to exceed maximum height and adjust
    // both width and height accordingly, keeping the area fixed.
    const auto AdjustRect = [](auto& desc)
    {
        if (desc.width != desc.copyWidth)
        {
            return;
        }
        if (desc.copyHeight > MAX_VIC_DIM)
        {
            MASSERT(desc.copyWidth < MAX_VIC_DIM);
            while (desc.copyHeight > MAX_VIC_DIM)
            {
                MASSERT((desc.copyHeight & 1) == 0);
                desc.copyHeight >>= 1;
                desc.copyWidth  <<= 1;
                MASSERT(desc.copyWidth < MAX_VIC_DIM);
            }
            desc.width = desc.copyWidth;
            MASSERT(desc.copyWidth < MAX_VIC_DIM);
        }
        else if (desc.copyWidth > MAX_VIC_DIM)
        {
            MASSERT(desc.copyHeight < MAX_VIC_DIM);
            while (desc.copyWidth > MAX_VIC_DIM)
            {
                MASSERT((desc.copyWidth & 1) == 0);
                desc.copyHeight <<= 1;
                desc.copyWidth  >>= 1;
                MASSERT(desc.copyHeight < MAX_VIC_DIM);
            }
            desc.width = desc.copyWidth;
            MASSERT(desc.copyWidth < MAX_VIC_DIM);
        }
    };
    AdjustRect(srcSurf);
    AdjustRect(dstSurf);
    if (srcSurf.copyWidth > MAX_VIC_DIM   ||
        srcSurf.copyWidth > srcSurf.width ||
        srcSurf.copyWidth == 0            ||
        srcSurf.copyHeight > MAX_VIC_DIM  ||
        srcSurf.copyHeight == 0)
    {
        MASSERT(!"Invalid source surface dimensions!");
        return RC::SOFTWARE_ERROR;
    }
    if (dstSurf.copyWidth > MAX_VIC_DIM   ||
        dstSurf.copyWidth > dstSurf.width ||
        dstSurf.copyWidth == 0            ||
        dstSurf.copyHeight > MAX_VIC_DIM  ||
        dstSurf.copyHeight == 0)
    {
        MASSERT(!"Invalid destination surface dimensions!");
        return RC::SOFTWARE_ERROR;
    }

    if (m_pImpl.get()               &&
        m_pImpl->pCh     == pCh     &&
        m_pImpl->pGpuDev == pGpuDev &&
        m_pImpl->srcSurf == srcSurf &&
        m_pImpl->dstSurf == dstSurf)
    {
        m_pImpl->timeoutMs = timeoutMs;
        return OK;
    }

#ifdef DEBUG // Used only by MASSERT
    const UINT32 srcWidthB = srcSurf.width * ColorUtils::PixelBytes(srcSurf.colorFormat);
    const UINT32 dstWidthB = dstSurf.width * ColorUtils::PixelBytes(dstSurf.colorFormat);
#endif
    MASSERT(srcSurf.layout != Surface2D::Pitch || srcWidthB % 256 == 0);
    MASSERT(dstSurf.layout != Surface2D::Pitch || dstWidthB % 256 == 0);
    MASSERT(srcSurf.layout != Surface2D::BlockLinear || srcWidthB % 64 == 0);
    MASSERT(dstSurf.layout != Surface2D::BlockLinear || dstWidthB % 64 == 0);

    CHECK_RC(Free());

    m_pImpl.reset(new Impl);

    m_pImpl->pCh       = pCh;
    m_pImpl->pGpuDev   = pGpuDev;
    m_pImpl->timeoutMs = timeoutMs;
    m_pImpl->srcSurf   = srcSurf;
    m_pImpl->dstSurf   = dstSurf;

    VideoCompositorAlloc vicAlloc;
    vicAlloc.SetOldestSupportedClass(Impl::OLDEST_SUPPORTED_CLASS);
    vicAlloc.SetNewestSupportedClass(Impl::NEWEST_SUPPORTED_CLASS);
    MASSERT(vicAlloc.IsSupported(pGpuDev));
    if (!vicAlloc.IsSupported(pGpuDev))
    {
        Printf(Tee::PriError, "VIC is not supported on this device\n");
        return RC::SOFTWARE_ERROR;
    }
    m_pImpl->vicClass = vicAlloc.GetSupportedClass(pGpuDev);

    CHECK_RC(pCh->SetClass(0, LWE26E_CMD_SETCL_CLASSID_GRAPHICS_VIC));

    switch (m_pImpl->vicClass)
    {
        case LWB0B6_VIDEO_COMPOSITOR:
        case LWB1B6_VIDEO_COMPOSITOR:
        case LWC5B6_VIDEO_COMPOSITOR:
            CHECK_RC(m_pImpl->InitVic4());
            break;

        default:
            MASSERT(!"Unknown class");
            break;
    }

    return rc;
}

RC VICDmaWrapper::Impl::AllocDataSurface
(
    Surface2D*  pSurface,
    const void* pData,
    size_t      size
)
{
    MASSERT(pSurface);
    RC rc;

    pSurface->SetWidth(static_cast<UINT32>(size));
    pSurface->SetHeight(1);
    pSurface->SetColorFormat(ColorUtils::Y8);
    pSurface->SetVASpace(Surface2D::TegraVASpace);
    pSurface->SetAlignment(Vic4::SURFACE_ALIGN);
    CHECK_RC(pSurface->Alloc(pGpuDev));

    if (pData)
    {
        SurfaceUtils::MappingSurfaceWriter surfaceWriter(*pSurface);
        CHECK_RC(surfaceWriter.WriteBytes(0, pData, size));
    }
    else
    {
        CHECK_RC(pSurface->Fill(0));
    }
    return rc;
}

RC VICDmaWrapper::Impl::InitVic4()
{
    RC rc;
    UINT32 configSize = 0;
    switch (vicClass)
    {
        case LWB0B6_VIDEO_COMPOSITOR:
            CHECK_RC(AllocDataSurface(&filterSurface, Vic4::pFilter,
                                      Vic4::filterSize));
            configSize = sizeof(Vic4::ConfigStruct);
            break;

        case LWB1B6_VIDEO_COMPOSITOR:
            CHECK_RC(AllocDataSurface(&filterSurface, Vic41::pFilter,
                                      Vic41::filterSize));
            configSize = sizeof(Vic41::ConfigStruct);
            break;

        case LWC5B6_VIDEO_COMPOSITOR:
            CHECK_RC(AllocDataSurface(&filterSurface, Vic42::pFilter,
                                      Vic42::filterSize));
            configSize = sizeof(Vic42::ConfigStruct);
            break;

        default:
            MASSERT(!"Unknown class");
            return RC::SOFTWARE_ERROR;
    }

    const Vic4::PIXELFORMAT srcFormat    = GetPixelFormat(srcSurf.colorFormat);
    const Vic4::PIXELFORMAT dstFormat    = GetPixelFormat(dstSurf.colorFormat);
    const Vic4::BLK_KIND    srcBlkKind   = GetBlkKind(srcSurf.layout);
    const Vic4::BLK_KIND    dstBlkKind   = GetBlkKind(dstSurf.layout);
    const UINT32 srcBlkHeight = srcSurf.layout == Surface2D::BlockLinear
                                ? srcSurf.logBlkHeight : 0;
    const UINT32 dstBlkHeight = dstSurf.layout == Surface2D::BlockLinear
                                ? dstSurf.logBlkHeight : 0;
    const UINT32 srcLeft      = 0;
    const UINT32 dstLeft      = 0;
    const UINT32 srcRight     = srcSurf.copyWidth - 1;
    const UINT32 dstRight     = dstSurf.copyWidth - 1;
    const UINT32 srcTop       = 0;
    const UINT32 dstTop       = 0;
    const UINT32 srcBottom    = srcSurf.copyHeight - 1;
    const UINT32 dstBottom    = dstSurf.copyHeight - 1;
    const UINT32 srcWidth     = srcSurf.width;
    const UINT32 dstWidth     = dstSurf.width;
    const UINT32 srcHeight    = srcSurf.copyHeight;
    const UINT32 dstHeight    = dstSurf.copyHeight;

    Vic41::ConfigStruct cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.outputConfig.TargetRectLeft   = 0;
    cfg.outputConfig.TargetRectRight  = dstWidth - 1;
    cfg.outputConfig.TargetRectTop    = 0;
    cfg.outputConfig.TargetRectBottom = dstHeight - 1;
    cfg.outputConfig.AlphaFillMode    = Vic4::DXVAHD_ALPHA_FILL_MODE_COMPOSITED;
    cfg.outputSurfaceConfig.OutPixelFormat   = dstFormat;
    cfg.outputSurfaceConfig.OutBlkKind       = dstBlkKind;
    cfg.outputSurfaceConfig.OutBlkHeight     = dstBlkHeight;
    cfg.outputSurfaceConfig.OutSurfaceWidth  = dstWidth - 1;
    cfg.outputSurfaceConfig.OutSurfaceHeight = dstHeight - 1;
    cfg.outputSurfaceConfig.OutLumaWidth     = dstWidth - 1;
    cfg.outputSurfaceConfig.OutLumaHeight    = dstHeight - 1;
    cfg.slotStruct[0].slotConfig.SlotEnable         = true;
    cfg.slotStruct[0].slotConfig.LwrrentFieldEnable = true;
    cfg.slotStruct[0].slotConfig.FrameFormat        = Vic4::DXVAHD_FRAME_FORMAT_PROGRESSIVE;
    cfg.slotStruct[0].slotConfig.MotionAclwmWeight  = 0x6;
    cfg.slotStruct[0].slotConfig.NoiseIir           = 0x300;
    cfg.slotStruct[0].slotConfig.SoftClampHigh      = 0x3ff;
    cfg.slotStruct[0].slotConfig.PlanarAlpha        = 0x3ff;
    cfg.slotStruct[0].slotConfig.SourceRectLeft     = srcLeft << 16;
    cfg.slotStruct[0].slotConfig.SourceRectRight    = srcRight << 16;
    cfg.slotStruct[0].slotConfig.SourceRectTop      = srcTop << 16;
    cfg.slotStruct[0].slotConfig.SourceRectBottom   = srcBottom << 16;
    cfg.slotStruct[0].slotConfig.DestRectLeft       = dstLeft;
    cfg.slotStruct[0].slotConfig.DestRectRight      = dstRight;
    cfg.slotStruct[0].slotConfig.DestRectTop        = dstTop;
    cfg.slotStruct[0].slotConfig.DestRectBottom     = dstBottom;
    cfg.slotStruct[0].slotSurfaceConfig.SlotPixelFormat   = srcFormat;
    cfg.slotStruct[0].slotSurfaceConfig.SlotBlkKind       = srcBlkKind;
    cfg.slotStruct[0].slotSurfaceConfig.SlotBlkHeight     = srcBlkHeight;
    cfg.slotStruct[0].slotSurfaceConfig.SlotCacheWidth    = GetCacheWidth(srcSurf);
    cfg.slotStruct[0].slotSurfaceConfig.SlotSurfaceWidth  = srcWidth - 1;
    cfg.slotStruct[0].slotSurfaceConfig.SlotSurfaceHeight = srcHeight - 1;
    cfg.slotStruct[0].slotSurfaceConfig.SlotLumaWidth     = srcWidth - 1;
    cfg.slotStruct[0].slotSurfaceConfig.SlotLumaHeight    = srcHeight - 1;
    cfg.slotStruct[0].colorMatrixStruct.matrix_coeff00 = 0x100;
    cfg.slotStruct[0].colorMatrixStruct.matrix_coeff11 = 0x100;
    cfg.slotStruct[0].colorMatrixStruct.matrix_coeff22 = 0x100;
    cfg.slotStruct[0].blendingSlotStruct.AlphaK1             = 0x3FF;
    cfg.slotStruct[0].blendingSlotStruct.AlphaK2             = 0;
    cfg.slotStruct[0].blendingSlotStruct.SrcFactCMatchSelect = Vic4::BLEND_SRCFACTC_K1;
    cfg.slotStruct[0].blendingSlotStruct.DstFactCMatchSelect = Vic4::BLEND_DSTFACTC_ZERO;
    cfg.slotStruct[0].blendingSlotStruct.SrcFactAMatchSelect = Vic4::BLEND_SRCFACTA_K1;
    cfg.slotStruct[0].blendingSlotStruct.DstFactAMatchSelect = Vic4::BLEND_DSTFACTA_ZERO;

    CHECK_RC(AllocDataSurface(&configSurface, &cfg, configSize));

    return rc;
}

UINT32 VICDmaWrapper::Impl::GetConfigSize() const
{
    switch (vicClass)
    {
        case LWB0B6_VIDEO_COMPOSITOR:
            return sizeof(Vic4::ConfigStruct);

        case LWB1B6_VIDEO_COMPOSITOR:
            return sizeof(Vic41::ConfigStruct);

        case LWC5B6_VIDEO_COMPOSITOR:
            return sizeof(Vic42::ConfigStruct);

        default:
            MASSERT(!"Unknown class");
            return 0;
    }
}

RC VICDmaWrapper::Copy
(
    UINT32 hMemSrc,
    UINT32 srcOffset,
    UINT32 hMemDst,
    UINT32 dstOffset
)
{
    MASSERT(m_pImpl.get());

    RC rc;
    Channel* pCh = m_pImpl->pCh;

    {
        MASSERT(srcOffset % Vic4::SURFACE_ALIGN == 0);
        MASSERT(dstOffset % Vic4::SURFACE_ALIGN == 0);
        if (srcOffset % Vic4::SURFACE_ALIGN != 0 ||
            dstOffset % Vic4::SURFACE_ALIGN != 0)
        {
            Printf(Tee::PriError, "Unaligned surface offset!\n");
            return RC::SOFTWARE_ERROR;
        }

        if (!m_pImpl->bChanInit)
        {
            CHECK_RC(pCh->Write(0, LWB0B6_VIDEO_COMPOSITOR_SET_APPLICATION_ID, 0x1));
            CHECK_RC(pCh->Write(0,
                    LWB0B6_VIDEO_COMPOSITOR_SET_CONTROL_PARAMS,
                    DRF_NUM(B0B6_VIDEO_COMPOSITOR, _SET_CONTROL_PARAMS,
                            _CONFIG_STRUCT_SIZE, m_pImpl->GetConfigSize() >> 4)));
            CHECK_RC(pCh->Write(0, LWB0B6_VIDEO_COMPOSITOR_SET_CONTEXT_ID, 0));
            CHECK_RC(pCh->WriteWithSurface(0,
                    LWB0B6_VIDEO_COMPOSITOR_SET_CONFIG_STRUCT_OFFSET,
                    m_pImpl->configSurface, 0, Vic4::SURFACE_SHIFT));
            CHECK_RC(pCh->WriteWithSurface(0,
                    LWB0B6_VIDEO_COMPOSITOR_SET_FILTER_STRUCT_OFFSET,
                    m_pImpl->filterSurface, 0, Vic4::SURFACE_SHIFT));
            CHECK_RC(pCh->Write(0, LWB0B6_VIDEO_COMPOSITOR_SET_PALETTE_OFFSET, 0));
            m_pImpl->bChanInit = true;
        }
        CHECK_RC(pCh->WriteWithSurfaceHandle(0,
                LWB0B6_VIDEO_COMPOSITOR_SET_OUTPUT_SURFACE_LUMA_OFFSET,
                hMemDst, dstOffset, Vic4::SURFACE_SHIFT,
                m_pImpl->dstSurf.mappingType));
        switch (m_pImpl->vicClass)
        {
            case LWB0B6_VIDEO_COMPOSITOR:
                CHECK_RC(pCh->WriteWithSurfaceHandle(0,
                        LWB0B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(0),
                        hMemSrc, srcOffset, Vic4::SURFACE_SHIFT,
                        m_pImpl->srcSurf.mappingType));
                break;
            case LWB1B6_VIDEO_COMPOSITOR:
                CHECK_RC(pCh->WriteWithSurfaceHandle(0,
                        LWB1B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(0),
                        hMemSrc, srcOffset, Vic41::SURFACE_SHIFT,
                        m_pImpl->srcSurf.mappingType));
                break;
            case LWC5B6_VIDEO_COMPOSITOR:
                CHECK_RC(pCh->WriteWithSurfaceHandle(0,
                        LWC5B6_VIDEO_COMPOSITOR_SET_SURFACE0_LUMA_OFFSET(0),
                        hMemSrc, srcOffset, Vic42::SURFACE_SHIFT,
                        m_pImpl->srcSurf.mappingType));
                break;
            default:
                MASSERT(!"Unknown class");
        }
        CHECK_RC(pCh->Write(0,
                LWB0B6_VIDEO_COMPOSITOR_EXELWTE,
                DRF_DEF(B0B6, _VIDEO_COMPOSITOR_EXELWTE, _AWAKEN, _ENABLE)));
    }
    return rc;
}

RC VICDmaWrapper::SemaphoreRelease
(
    const Surface2D& semaSurf,
    UINT32           offset,
    UINT32           payload
)
{
    return SemaphoreRelease(m_pImpl->pCh, semaSurf, offset, payload, Channel::FlagSemRelNone);
}

RC VICDmaWrapper::SemaphoreRelease
(
    Channel*             pCh,
    const Surface2D&     semaSurf,
    UINT32               offset,
    UINT32               payload,
    Channel::SemRelFlags flags
)
{
    RC rc;

    MASSERT(flags == Channel::FlagSemRelNone || flags == Channel::FlagSemRelWithTime);

    CHECK_RC(pCh->WriteWithSurface(0,
                LWB0B6_VIDEO_COMPOSITOR_SEMAPHORE_A,
                semaSurf, offset, 32));

    CHECK_RC(pCh->WriteWithSurface(0,
                LWB0B6_VIDEO_COMPOSITOR_SEMAPHORE_B,
                semaSurf, offset, 0));

    CHECK_RC(pCh->Write(0,
                LWB0B6_VIDEO_COMPOSITOR_SEMAPHORE_C,
                DRF_NUM(B0B6_VIDEO_COMPOSITOR, _SEMAPHORE_C, _PAYLOAD,
                    payload)));

    CHECK_RC(pCh->Write(0,
                LWB0B6_VIDEO_COMPOSITOR_SEMAPHORE_D,
                flags == Channel::FlagSemRelWithTime ?
                    DRF_DEF(B0B6_VIDEO_COMPOSITOR, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR) :
                    DRF_DEF(B0B6_VIDEO_COMPOSITOR, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                DRF_DEF(B0B6_VIDEO_COMPOSITOR, _SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE) |
                DRF_DEF(B0B6_VIDEO_COMPOSITOR, _SEMAPHORE_D, _OPERATION, _RELEASE)));

    return rc;
}

RC VICDmaWrapper::Flush()
{
    RC rc;
    Channel* pCh = m_pImpl->pCh;

    CHECK_RC(pCh->Flush());
    m_pImpl->bChanInit = false;
    return rc;
}

//! \class VicStrategy
//! \brief Wrapperaround Copy using VIC engine
class VicStrategy : public IDmaStrategy
{
public:
    VicStrategy(GpuTestConfiguration * pTestConfig, Channel *pCh, GpuDevice *pGpuDev);
    virtual ~VicStrategy();
    static bool IsSupported(GpuDevice* pGpuDev);
    virtual RC SetSurfaces(Surface2D *pSrc, Surface2D *pDst);
    virtual RC SetCtxDmaOffsets(UINT64 srcCtxDmaOffset, UINT64 dstCtxDmaOffset);
    virtual RC Copy
    (
        UINT32 srcOriginX,
        UINT32 srcOriginY,
        UINT32 widthBytes,
        UINT32 height,
        UINT32 dstOriginX,
        UINT32 dstOriginY,
        FLOAT64 timeoutMs,
        UINT32 subdevNum,
        bool flushChannel,
        bool writeSemaphores,
        UINT32 flushId
    );
    virtual RC Wait(FLOAT64 timeoutMs, UINT32 flushId);
    virtual RC Initialize();

private:
    virtual UINT32 GetPitchAlignRequirement() const
    {
        return 256;
    }

    bool                   m_Initialized;
    GpuTestConfiguration * m_pTestConfig;
    Channel              * m_pCh;
    GpuDevice            * m_pGpuDev;
    Surface2D            * m_pSrc;
    Surface2D            * m_pDst;
    VICDmaWrapper          m_Dma;
};

VicStrategy::VicStrategy(GpuTestConfiguration * pTestConfig, Channel *pCh, GpuDevice *pGpuDev) :
    m_Initialized(false),
    m_pTestConfig(pTestConfig),
    m_pCh(pCh),
    m_pGpuDev(pGpuDev),
    m_pSrc(nullptr),
    m_pDst(nullptr)
{
}

VicStrategy::~VicStrategy()
{
    m_Dma.Free();
    if (m_pTestConfig && m_pCh)
        m_pTestConfig->FreeChannel(m_pCh);
}

/* static */ bool VicStrategy::IsSupported(GpuDevice* pGpuDev)
{
    return VICDmaWrapper::IsSupported(pGpuDev);
}

/* virtual */ RC VicStrategy::SetSurfaces(Surface2D *pSrc, Surface2D *pDst)
{
    MASSERT(pSrc);
    MASSERT(pDst);
    m_pSrc = pSrc;
    m_pDst = pDst;
    if (m_pCh)
    {
        m_pSrc->BindGpuChannel(m_pCh->GetHandle());
        m_pDst->BindGpuChannel(m_pCh->GetHandle());
    }
    return OK;
}

/* virtual */ RC VicStrategy::SetCtxDmaOffsets(UINT64 src, UINT64 dst)
{
    Printf(Tee::PriError, "GPU VAs not supported with VIC!\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC VicStrategy::Copy
(
    UINT32 srcOriginX,
    UINT32 srcOriginY,
    UINT32 widthBytes,
    UINT32 height,
    UINT32 dstOriginX,
    UINT32 dstOriginY,
    FLOAT64 timeoutMs,
    UINT32 subdevNum,
    bool flushChannel,
    bool writeSemaphores,
    UINT32 flushId
)
{
    MASSERT(m_pSrc);
    MASSERT(m_pDst);
    RC rc;

    auto srcSurf = VICDmaWrapper::SurfDesc::FromSurface2D(*m_pSrc);
    srcSurf.copyWidth    = widthBytes / m_pSrc->GetBytesPerPixel();
    srcSurf.copyHeight   = height;
    const UINT32 srcOffs = UNSIGNED_CAST(UINT32,
        m_pSrc->GetPixelOffset(srcOriginX/m_pSrc->GetBytesPerPixel(), srcOriginY));

    auto dstSurf = VICDmaWrapper::SurfDesc::FromSurface2D(*m_pDst);
    dstSurf.copyWidth    = widthBytes / m_pDst->GetBytesPerPixel();
    dstSurf.copyHeight   = height;
    const UINT32 dstOffs = UNSIGNED_CAST(UINT32,
        m_pDst->GetPixelOffset(dstOriginX/m_pDst->GetBytesPerPixel(), dstOriginY));

    CHECK_RC(m_Dma.Initialize(m_pCh, m_pGpuDev, timeoutMs, srcSurf, dstSurf));
    CHECK_RC(m_Dma.Copy(m_pSrc->GetTegraMemHandle(), srcOffs,
                        m_pDst->GetTegraMemHandle(), dstOffs));
    CHECK_RC(m_Dma.Flush());

    return rc;
}

/* virtual */ RC VicStrategy::Wait(FLOAT64 timeoutMs, UINT32 flushId)
{
    return m_pCh->WaitIdle(timeoutMs);
}

/* virtual */ RC VicStrategy::Initialize()
{
    RC rc;
    Printf(Tee::PriDebug, "DMAWRAP: Entering VicStrategy::Initialize\n");
    if (!m_Initialized)
    {
        if (!m_pCh)
        {
            LwRm::Handle hCh;
            CHECK_RC(m_pTestConfig->AllocateChannel(&m_pCh, &hCh, LW2080_ENGINE_TYPE_VIC));
        }

        if (m_pSrc)
            m_pSrc->BindGpuChannel(m_pCh->GetHandle());
        if (m_pDst)
            m_pDst->BindGpuChannel(m_pCh->GetHandle());
        m_Initialized = true;
    }

    Printf(Tee::PriDebug, "DMAWRAP: Exiting VicStrategy::Initialize\n");
    return rc;
}

UINT32 VICDmaWrapper::Impl::GetCacheWidth(const VICDmaWrapper::SurfDesc& surf)
{
    UINT32 cacheWidth;
    if (surf.layout == Surface2D::BlockLinear)
    {
        cacheWidth = Vic4::CACHE_WIDTH_32Bx8;
        if ((surf.copyHeight & 7) != 0)
            cacheWidth = Vic4::CACHE_WIDTH_64Bx4;
    }
    else
    {
        cacheWidth = Vic4::CACHE_WIDTH_64Bx4;
        if ((surf.copyHeight & 3) != 0)
        {
            cacheWidth = Vic4::CACHE_WIDTH_256Bx1;
        }
    }
    return cacheWidth;
}

Vic4::BLK_KIND VICDmaWrapper::Impl::GetBlkKind(Surface2D::Layout layout)
{
    switch (layout)
    {
        case Surface2D::Pitch:
            return Vic4::BLK_KIND_PITCH;
        case Surface2D::BlockLinear:
            return Vic4::BLK_KIND_GENERIC_16Bx2;
        default:
            MASSERT(!"Unknown layout");
    }
    return Vic4::BLK_KIND_PITCH;
}

Vic4::PIXELFORMAT VICDmaWrapper::Impl::GetPixelFormat(ColorUtils::Format color)
{
    switch (color)
    {
        case ColorUtils::A8R8G8B8:
            return Vic4::T_A8R8G8B8;
        case ColorUtils::Y8:
            return Vic4::T_L8;
        case ColorUtils::U8V8:
            return Vic4::T_U8V8;
        case ColorUtils::R16:
            return Vic4::T_R8G8; // There is no corresponding VIC format
        case ColorUtils::VOID16:
            return Vic4::T_R8G8; // There is no corresponding VIC format
        case ColorUtils::R16_G16:
            return Vic4::T_X8R8G8B8; // There is no corresponding VIC format
        case ColorUtils::VOID32:
            return Vic4::T_X8R8G8B8; // There is no corresponding VIC format
        case ColorUtils::A8B8G8R8:
            return Vic4::T_A8B8G8R8;
        default:
            {
                const string name = ColorUtils::FormatToString(color);
                Printf(Tee::PriNormal, "ERROR: Unsupported color format: %s\n", name.c_str());
            }
            MASSERT(!"Unsupported format");
    }
    return Vic4::T_L8;
}

DmaWrapper::DmaWrapper(GpuTestConfiguration* pTestConfig)
{
    Initialize(pTestConfig, Memory::Optimal);
}

DmaWrapper::DmaWrapper
(
    Surface2D*  pSrcSurface2D,
    Surface2D*  pDstSurface2D,
    GpuTestConfiguration* pTestConfig
)
{
    Initialize(pTestConfig, Memory::Optimal);
    SetSurfaces(pSrcSurface2D, pDstSurface2D);
}

DmaWrapper::~DmaWrapper()
{
    Cleanup();
}

RC DmaWrapper::Cleanup()
{
    if(!m_Initialized)
        return OK;
    if(m_TransImpl)
    {
        delete m_TransImpl;
        m_TransImpl = nullptr;
    }

    m_pSrcSurf        = nullptr;
    m_pDstSurf        = nullptr;
    m_SrcCtxDmaOffset = 0;
    m_DstCtxDmaOffset = 0;
    m_Initialized = false;
    return OK;
}

RC DmaWrapper::Initialize
(
    GpuTestConfiguration* pTestConfig, //!< TestConfig to allocate channels on
    Channel *pChannel,                 //!< Reuse an existed channel if any
    Memory::Location NfyLoc,           //!< Memory in which to alloc semaphore.
    PREF_TRANS_METH TransferMethod,    //! Dma dev to use.
    UINT32 Instance                    //! Instance number of the device
)
{
    RC rc;

    if (m_Initialized)
    {
        Printf(Tee::PriHigh, "DmaWrapper is already initialized.\n");
        return RC::SOFTWARE_ERROR;
    }

    if (pTestConfig == nullptr)
    {
        Printf(Tee::PriHigh, "DmaWrapper::Initialize: GpuTestConfiguration* can't be 0.\n");
        return RC::SOFTWARE_ERROR;
    }

    m_NfyLoc = NfyLoc;
    GpuDevice *pGpuDev = pTestConfig->GetGpuDevice();
    if (pGpuDev == nullptr)
    {
        Printf(Tee::PriHigh, "DmaWrapper::Initialize: GpuDevice in GpuTestConfiguration can't be 0.\n");
        return RC::SOFTWARE_ERROR;
    }

    const bool hasCE      = CopyEngineStrategy::IsSupported(pGpuDev);
    const bool hasVic     = VicStrategy::IsSupported(pGpuDev) && pTestConfig->AllowVIC();
    if ((DmaWrapper::DEFAULT_METH == TransferMethod)
     || (DmaWrapper::COPY_ENGINE  == TransferMethod && ! hasCE)
     || (DmaWrapper::VIC          == TransferMethod && ! hasVic))
    {
        if (hasVic)
        {
            TransferMethod = DmaWrapper::VIC;
        }
        else if (hasCE)
        {
            TransferMethod = DmaWrapper::COPY_ENGINE;
        }
        else
        {
            Printf(Tee::PriHigh, "DmaWrapper::Initialize: No supported DMA mem copy method found!\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    //CopyEngine isn't yet made to support SLI devices
    //TODO Make it suport sli devices and remove this.
    if((DmaWrapper::COPY_ENGINE == TransferMethod) &&
       (pGpuDev->GetNumSubdevices() > 1))
    {
        Printf(Tee::PriHigh,
               "CopyEngine doesn't yet support SLI devices. Aborting.\n");
        return RC::SOFTWARE_ERROR;
    }
    m_TransferMethod = TransferMethod;

    pTestConfig->SetAllowMultipleChannels(true);
    Utility::CleanupOnReturnArg<TestConfiguration,
                                void,
                                TestConfiguration::ChannelType>
        restoreChanType(pTestConfig,
                        &TestConfiguration::SetChannelType,
                        pTestConfig->GetChannelType());

    if (pChannel)
    {
        TestConfiguration::ChannelType ChType = pTestConfig->GetChannelType();

        if (pGpuDev->GetSubdevice(0)->IsSOC())
        {
            LwRm *pLwRm = pTestConfig->GetRmClient(); // previous bound or client 0
            if (m_TransferMethod == VIC)
            {
                if (ChType != TestConfiguration::TegraTHIChannel || pChannel->GetClass() != LWE2_CHANNEL_DMA)
                {
                    return RC::TEST_CONFIGURATION_ILWALID_CHANNEL_TYPE;
                }
            }
            else
            {
                if (ChType != TestConfiguration::GpFifoChannel)
                {
                    return RC::TEST_CONFIGURATION_ILWALID_CHANNEL_TYPE;
                }

                UINT32 FifoClass;
                CHECK_RC(pLwRm->GetFirstSupportedClass(pTestConfig->GetNumFifoClasses(),
                    pTestConfig->GetFifoClasses(), &FifoClass, pTestConfig->GetGpuDevice()));

                if (pChannel->GetClass() != FifoClass)
                {
                    return RC::TEST_CONFIGURATION_ILWALID_CHANNEL_TYPE;
                }

            }
        }
    }
    else
    {
        if (pGpuDev->GetSubdevice(0)->IsSOC())
        {
            if (m_TransferMethod == VIC)
            {
                pTestConfig->SetChannelType(TestConfiguration::TegraTHIChannel);
            }
            else
            {
                pTestConfig->SetChannelType(TestConfiguration::GpFifoChannel);
            }
        }
    }

    switch(m_TransferMethod)
    {
        case COPY_ENGINE:
            Printf(Tee::PriDebug,
                   "DMAWRAP: Instatiating CopyEngine.\n");
            m_TransImpl = new CopyEngineStrategy(pTestConfig,
                                                 pChannel,
                                                 m_NfyLoc,
                                                 Instance,
                                                 pGpuDev);
            break;
        case TWOD:
            Printf(Tee::PriDebug,
                   "DMAWRAP: Instatiating TWOD.\n");
            m_TransImpl = new TWODStrategy(pTestConfig,
                                           pChannel,
                                           pGpuDev);
            break;
        case VIC:
            Printf(Tee::PriDebug,
                   "DMAWRAP: Instatiating VIC.\n");
            m_TransImpl = new VicStrategy(pTestConfig,
                                          pChannel,
                                          pGpuDev);
            break;
        default:
            Printf(Tee::PriHigh,
                   "DMAWRAP: Unsupported transfer method: %d\n",
                   m_TransferMethod);
            return RC::BAD_PARAMETER;
    }
    if(!m_TransImpl)
        return RC::CANNOT_ALLOCATE_MEMORY;

    if(m_pSrcSurf && m_pDstSurf)
    {
        m_TransImpl->SetSurfaces(m_pSrcSurf, m_pDstSurf);
    }

    if (m_SrcCtxDmaOffset || m_DstCtxDmaOffset)
    {
        CHECK_RC(m_TransImpl->SetCtxDmaOffsets(m_SrcCtxDmaOffset, m_DstCtxDmaOffset));
    }
    CHECK_RC(m_TransImpl->Initialize());

    m_Initialized = true;
    return OK;
}

RC DmaWrapper::Initialize
(
    GpuTestConfiguration* pTestConfig, //!< TestConfig to allocate channels on
    Memory::Location NfyLoc,           //!< Memory in which to alloc semaphore.
    PREF_TRANS_METH TransferMethod,    //! Dma dev to use.
    UINT32 Instance                    //! Instance number of the device
)
{
    return Initialize(pTestConfig, nullptr, NfyLoc, TransferMethod, Instance);
}

RC DmaWrapper::Initialize
(    GpuTestConfiguration* pTestConfig,
    Memory::Location NfyLoc,
    PREF_TRANS_METH TransferMethod
)
{
    return Initialize(pTestConfig, NfyLoc, TransferMethod, DEFAULT_ENGINE_INSTANCE);
}

bool DmaWrapper::IsPitchAlignCorrect() const
{
    UINT32 pitchAlign = GetPitchAlignRequirement();
    const char *errorMsgTmpl =
        "ERROR: DMA implementation has requirements on alignment of the surface pitch.\n"
        "Pitch alignment requirement = %u\n"
        "%s surface (\"%s\") pitch = %u\n";
    bool surfPitchAlignError = false;
    if (Surface2D::Pitch == m_pSrcSurf->GetLayout() && 0 != m_pSrcSurf->GetPitch() % pitchAlign)
    {
        Printf(Tee::PriHigh, errorMsgTmpl,
            pitchAlign,
            "Source",
            m_pSrcSurf->GetName().c_str(),
            m_pSrcSurf->GetPitch()
        );
        surfPitchAlignError = true;
    }
    if (Surface2D::Pitch == m_pDstSurf->GetLayout() && 0 != m_pDstSurf->GetPitch() % pitchAlign)
    {
        Printf(Tee::PriHigh, errorMsgTmpl,
            pitchAlign,
            "Destination",
            m_pDstSurf->GetName().c_str(),
            m_pDstSurf->GetPitch()
        );
        surfPitchAlignError = true;
    }

    return !surfPitchAlignError;
}

RC DmaWrapper::SetSurfaces(Surface2D* src, Surface2D* dst)
{
    RC rc;
    MASSERT(nullptr != src);
    MASSERT(nullptr != dst);
    m_pSrcSurf = src;
    m_pDstSurf = dst;

    if (m_TransImpl)
    {
        if (!IsPitchAlignCorrect())
        {
            return RC::SOFTWARE_ERROR;
        }
        rc = m_TransImpl->SetSurfaces(src, dst);
    }
    return rc;
}

RC DmaWrapper::SetSrcDstSurf(Surface2D* src, Surface2D* dst)
{
    return SetSurfaces(src, dst);
}

RC DmaWrapper::SetCtxDmaOffsets(UINT64 srcCtxDmaOffset, UINT64 dstCtxDmaOffset)
{
    RC rc;

    m_SrcCtxDmaOffset = srcCtxDmaOffset;
    m_DstCtxDmaOffset = dstCtxDmaOffset;
    if(m_TransImpl)
    {
        rc = m_TransImpl->SetCtxDmaOffsets(srcCtxDmaOffset, dstCtxDmaOffset);
    }
    return rc;
}

void DmaWrapper::SetCopyMask(UINT32 copyMask)
{
   m_TransImpl->SetCopyMask(copyMask);
}

void DmaWrapper::SetUsePipelinedCopies(bool bUsePipelining)
{
    m_TransImpl->SetUsePipelinedCopies(bUsePipelining);
}

void DmaWrapper::SetFlush(bool bEnable)
{
   m_TransImpl->SetFlush(bEnable);
}

void DmaWrapper::SetUsePhysicalCopy(bool bSrcPhysical, bool bDstPhysical)
{
    m_TransImpl->SetUsePhysicalCopy(bSrcPhysical, bDstPhysical);
}

void DmaWrapper::SetPhysicalCopyTarget(Memory::Location srcLoc, Memory::Location dstLoc)
{
    m_TransImpl->SetPhysicalCopyTarget(srcLoc, dstLoc);
}

UINT32 DmaWrapper::GetPitchAlignRequirement() const
{
    return m_TransImpl->GetPitchAlignRequirement();
}

RC DmaWrapper::Copy
(
    UINT32 SrcOriginX, //!< Starting src X, in bytes not pixels.
    UINT32 SrcOriginY, //!< Starting src Y, in lines.
    UINT32 WidthBytes, //!< Width of copied rect, in bytes.
    UINT32 Height,     //!< Height of copied rect, in bytes
    UINT32 DstOriginX, //!< Starting dst X, in bytes not pixels.
    UINT32 DstOriginY, //!< Starting dst Y, in lines.
    FLOAT64 TimeoutMs, //!< Timeout for dma operation
    UINT32 SubdevNum,  //!< Subdevice number we are working on
    bool   ShouldWait, //!< Wait for copy completion.
    bool   flush,      //!< Immediately flush GPU methods.
    bool   writeSemaphores//!< Write semaphores , used for synchronization
)
{
    RC rc;
    if (!m_Initialized)
    {
        return RC::SOFTWARE_ERROR;
    }
    if (!m_TransImpl)
    {
        return RC::SOFTWARE_ERROR;
    }
    if (!IsPitchAlignCorrect())
    {
        return RC::SOFTWARE_ERROR;
    }

    // Wait makes sure last op has finished. If we already waited
    // on previous flush, wait will return without side-effects.
    if(m_NumFlushes >= (m_NumWaits + MAX_CE_FLUSHES))
    {
        Printf(Tee::PriDebug,
               "DMAWRAP: Maxed out flushes, waiting. waits: %d flushes %d\n.",
               m_NumWaits,
               m_NumFlushes);
        //Arbitrarily choose to wait for half of the flushed ops.
        m_TransImpl->Wait(TimeoutMs, m_NumWaits + (MAX_CE_FLUSHES / 2));
        m_NumWaits = m_NumWaits + (MAX_CE_FLUSHES/2);
    }

    if (m_pSrcSurf->GetLayout() == Surface2D::Pitch &&
        WidthBytes > m_pSrcSurf->GetPitch())
    {
        Printf(Tee::PriNormal, "ERROR: Invalid copy width for src pitch surface\n");
        return RC::SOFTWARE_ERROR;
    }
    else if (m_pDstSurf->GetLayout() == Surface2D::Pitch &&
             WidthBytes > m_pDstSurf->GetPitch())
    {
        Printf(Tee::PriNormal, "ERROR: Invalid copy width for dest pitch surface\n");
        return RC::SOFTWARE_ERROR;
    }

    // If we want to wait for completion, we need to flush and write semaphores
    if (ShouldWait)
    {
        flush = true;
        writeSemaphores = true;
    }
    if (writeSemaphores)
    {
        m_NumFlushes++;
    }
    CHECK_RC(m_TransImpl->Copy(SrcOriginX,
                               SrcOriginY,
                               WidthBytes,
                               Height,
                               DstOriginX,
                               DstOriginY,
                               TimeoutMs,
                               SubdevNum,
                               flush,
                               writeSemaphores,
                               m_NumFlushes));

    if (ShouldWait)
    {
        Printf(Tee::PriDebug, "DMAWRAP: Waiting after flushing.\n");
        CHECK_RC(Wait(TimeoutMs));
    }
    return rc;
}

//! \brief Waits for a flushed write operation to complete.
//!
//! Performs wait on implementation-specific synchronisation primitive.
//! Wrapper pays attention to wait before every flush, and to ignore double
//! waits.
RC DmaWrapper::Wait(FLOAT64 TimeoutMs)
{
    RC rc;
    if(m_NumWaits != m_NumFlushes)
    {
        Printf(Tee::PriDebug, "DMAWRAP: flushes: %d waits: %d\n", m_NumWaits, m_NumFlushes);
        CHECK_RC(m_TransImpl->Wait(TimeoutMs, m_NumFlushes));
    }
    m_NumWaits = m_NumFlushes;
    return rc;

}

//! \brief Checks if a copy has completed
RC DmaWrapper::IsCopyComplete(bool *pComplete)
{
    RC rc;
    CHECK_RC(m_TransImpl->IsCopyComplete(m_NumFlushes, pComplete));
    return rc;
}

//! \brief Get timing information
RC DmaWrapper::GetCopyTimeUs(UINT64 *pStartTimeUs, UINT64 *pEndTimeUs)
{
    if (!m_TransImpl)
    {
        return RC::SOFTWARE_ERROR;
    }
    MASSERT(pStartTimeUs);
    MASSERT(pEndTimeUs);
    return m_TransImpl->GetCopyTimeUs(pStartTimeUs, pEndTimeUs);
}

//! \brief Enable or disable semaphore usage to store timing information
RC DmaWrapper::SaveCopyTime(bool bSaveCopyTime)
{
    if (!m_TransImpl)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (bSaveCopyTime && !m_Initialized)
    {
        Printf(Tee::PriError, "Engine not initialized \n");
        return RC::SOFTWARE_ERROR;
    }

    return m_TransImpl->SaveCopyTime(bSaveCopyTime);
}

void DmaWrapper::WriteStartTimeSemaphore(bool bWrite)
{
    m_TransImpl->WriteStartTimeSemaphore(bWrite);
}

//-----------------------------------------------------------------------------
//
//  class CopyengineStrategy
//
//-----------------------------------------------------------------------------

CopyEngineStrategy::CopyEngineStrategy
(
    GpuTestConfiguration * pTestConfig,
    Channel*   p_Ch,
    Memory::Location NotifierLocation,
    UINT32     InstanceNum,
    GpuDevice* pGpuDev
):
    m_SrcSurf(nullptr),
    m_DstSurf(nullptr),
    m_SrcCtxDmaOffset(0),
    m_DstCtxDmaOffset(0),
    m_Initialized(false),
    m_WriteSemaphores(false),
    m_pTestConfig(pTestConfig),
    m_pCh(p_Ch),
    m_pGpuDev(pGpuDev),
    m_EngineInstance(InstanceNum),
    m_SelectedNfyLoc(NotifierLocation),
    m_CopyMask(0),
    m_EnableCopyWithHoles(false),
    m_UsePiplinedCopies(false),
    m_DisableFlush(false),
    m_bSrcPhysical(false),
    m_SrcPhysicalTarget(Memory::Optimal),
    m_bDstPhysical(false),
    m_DstPhysicalTarget(Memory::Optimal),
    m_StartSemaSurf(nullptr),
    m_CopyStartTimeUs(0),
    m_CopyEndTimeUs(0),
    m_SaveCopyTime(false),
    m_WriteStartTimeSemaphore(true),
    m_bAllocatedChannel(false)
{
}

CopyEngineStrategy::~CopyEngineStrategy()
{
    for (MapLocationSemaphore::iterator sem_iter = m_Semaphores.begin();
         sem_iter != m_Semaphores.end();
         sem_iter++)
    {
        delete sem_iter->second;
    }

    if (m_pTestConfig && m_pCh && m_bAllocatedChannel)
        m_pTestConfig->FreeChannel(m_pCh);
}

RC CopyEngineStrategy::SetSurfaces(Surface2D* src, Surface2D* dst)
{
    MASSERT(src);
    MASSERT(dst);
    m_SrcSurf = src;
    m_DstSurf = dst;
    if (m_pCh)
    {
        m_SrcSurf->BindGpuChannel(m_pCh->GetHandle());
        m_DstSurf->BindGpuChannel(m_pCh->GetHandle());
    }
    return OK;
}

RC CopyEngineStrategy::SetCtxDmaOffsets(UINT64 srcCtxDmaOffset, UINT64 dstCtxDmaOffset)
{
    m_SrcCtxDmaOffset = srcCtxDmaOffset;
    m_DstCtxDmaOffset = dstCtxDmaOffset;
    return OK;
}

void CopyEngineStrategy::SetCopyMask(UINT32 CopyMask)
{
    INT32 SrcBPP = m_SrcSurf->GetBytesPerPixel();
    INT32 DstBPP = m_DstSurf->GetBytesPerPixel();
    if (SrcBPP == DstBPP && CopyMask < static_cast<UINT32>(1<<DstBPP))
    {
        m_EnableCopyWithHoles = true;
        m_CopyMask = CopyMask;
    }
    else
    {
        Printf(Tee::PriHigh, "Warning: Ignoring copying with holes: "
            "Invalid Mask: %u.\n", CopyMask);
    }
}

void CopyEngineStrategy::SetUsePipelinedCopies(bool bUsePipelining)
{
    m_UsePiplinedCopies = bUsePipelining;
}

void CopyEngineStrategy::SetFlush(bool bEnable)
{
    m_DisableFlush = !bEnable;
}

void CopyEngineStrategy::SetUsePhysicalCopy(bool bSrcPhysical, bool bDstPhysical)
{
    m_bSrcPhysical = bSrcPhysical;
    m_bDstPhysical = bDstPhysical;
}

void CopyEngineStrategy::SetPhysicalCopyTarget(Memory::Location srcLoc, Memory::Location dstLoc)
{
    m_SrcPhysicalTarget = srcLoc;
    m_DstPhysicalTarget = dstLoc;
}

//! \brief Copy m_SrcSurf to m_DstSurf.
//!
//! Performs a single copy of a subsurface of m_SrcSurf into m_DstSurf.
//! Positions and dimensions of the subsurface to be copied are determined
//! by method parameters.
//!
//! \param SrcOriginX - X offset from beggining of Surface2D in pixels
//! \param SrcOriginY - Y offset from beggining of Surface2D in pixels
//! \param Width - Width of the section of the surface
//!     to be blitted in pixels
//! \param Height - Width of the section of the surface
//!     to be blitted in pixels
//! \param DstOriginX - X offset from beggining of Surface
//! \param DstOriginY - Y offset from beggining of Surface
//! \param TimeoutMs
//! \param SubdevNum
//!
//! \return RC
RC CopyEngineStrategy::Copy
(
    UINT32 SrcOriginX,    //!< Starting src X, in bytes not pixels.
    UINT32 SrcOriginY,    //!< Starting src Y, in lines.
    UINT32 Width,         //!< Width of copied rect, in bytes.
    UINT32 Height,        //!< Height of copied rect, in lines
    UINT32 DstOriginX,    //!< Starting dst X, in bytes not pixels.
    UINT32 DstOriginY,    //!< Starting dst Y, in lines.
    FLOAT64 TimeoutMs,    //!< Timeout for dma operation
    UINT32 SubdevNum,     //!< Unused in CE. Subdev num.
    bool flush,           //!< Flush methods to channel
    bool writeSemaphores, //!< write semaphores? used for synchronization
    UINT32 FlushId        //!< Copy op Id. Used for synchronization
)
{
    RC rc;
    Printf(Tee::PriDebug, "DMAWRAP: Entering CES::Copy\n");
    CHECK_RC(Initialize());
    m_WriteSemaphores = writeSemaphores;
    if (m_WriteSemaphores)
    {
        m_SelectedNfyLoc = m_DstSurf->GetLocation();
        CHECK_RC(InitSemaphore(m_SelectedNfyLoc));
        NotifySemaphore *pSemaphore = m_Semaphores[m_SelectedNfyLoc];
        CHECK_RC(m_pCh->SetContextDmaSemaphore(pSemaphore->GetCtxDmaHandleGpu(0)));
        CHECK_RC(m_pCh->SetSemaphoreOffset(pSemaphore->GetCtxDmaOffsetGpu(0)));
    }

    if (m_SaveCopyTime && m_WriteStartTimeSemaphore)
    {
        MASSERT(m_StartSemaSurf);
        m_pCh->SetSemaphoreReleaseFlags(Channel::FlagSemRelWithTime | Channel::FlagSemRelWithWFI);
        CHECK_RC(m_pCh->SetSemaphoreOffset(m_StartSemaSurf->GetCtxDmaOffsetGpu()));
        CHECK_RC(m_pCh->SemaphoreRelease(0));
    }

    CHECK_RC(WriteMethods(SrcOriginX,
                          SrcOriginY,
                          DstOriginX,
                          DstOriginY,
                          Width,
                          Height,
                          FlushId));
    if (flush)
    {
        CHECK_RC(m_pCh->Flush());
    }

    Printf(Tee::PriDebug, "DMAWRAP: Exiting CES::Copy\n");
    return OK;
}

RC CopyEngineStrategy::IsCopyComplete(UINT32 FlushId, bool *pComplete)
{
    RC rc;
    if (m_Semaphores.find(m_SelectedNfyLoc) == m_Semaphores.end())
    {
        Printf(Tee::PriNormal,
               "DMAWRAP: Wait called before copy!\n");
        return RC::SOFTWARE_ERROR;
    }

    NotifySemaphore *pSemaphore = m_Semaphores[m_SelectedNfyLoc];
    pSemaphore->SetOneTriggerPayload(0, FlushId);
    MASSERT(pComplete);
    CHECK_RC(pSemaphore->IsCopyComplete(pComplete));
    return rc;
}

RC CopyEngineStrategy::Wait(FLOAT64 TimeoutMs, UINT32 FlushId)
{
    RC rc;
    Printf(Tee::PriDebug, "DMAWRAP: Starting a wait on Sem.\n");
    if (m_Semaphores.find(m_SelectedNfyLoc) == m_Semaphores.end())
    {
        Printf(Tee::PriNormal,
               "DMAWRAP: Wait called before copy!\n");
        return RC::SOFTWARE_ERROR;
    }

    NotifySemaphore *pSemaphore = m_Semaphores[m_SelectedNfyLoc];

    pSemaphore->SetOneTriggerPayload(0, FlushId);
    //Synchronization protocol in DmaWrapper works by autoincrementing each subsquent
    //copy operation. These are queued in channel and completed in sequence.
    //After each copy is done, gpu writes id of the copy into semaphore.
    //therefore, to be sure a copy op is done we wait for it's id, or greater one
    //to be written to semaphore.
    CHECK_RC(pSemaphore->Wait(TimeoutMs, &NotifySemaphore::CondGreaterEq));
    Printf(Tee::PriDebug, "DMAWRAP: Finished a wait on Sem.\n");

    if (m_SaveCopyTime)
    {
        MASSERT(m_StartSemaSurf);
        UINT08 *pSemStart = static_cast<UINT08 *>(m_StartSemaSurf->GetAddress());
        m_CopyStartTimeUs = (MEM_RD64(static_cast<UINT08 *>(pSemStart) + 8)) / 1000ULL;

        NotifySemaphore *pSemaphore = m_Semaphores[m_SelectedNfyLoc];
        m_CopyEndTimeUs = pSemaphore->GetTimestamp(0) / 1000ULL;
    }

    return rc;
}

//! \brief Initializes a single CopyEngine instance for a copy operation.
//!
//! Performes a per-instance initialization. Before this gets called,
//! per-class initialization is already done.
//!
//! \return RC
RC CopyEngineStrategy::Initialize()
{
    RC rc;
    Printf(Tee::PriDebug, "DMAWRAP: Entering CopyEngineStrategy::Initialize\n");
    if(!m_Initialized)
    {
        UINT32 engineId;
        if (m_EngineInstance == DmaWrapper::DEFAULT_ENGINE_INSTANCE)
        {
            CHECK_RC(m_DmaCopyAlloc.GetDefaultEngineId(m_pTestConfig->GetGpuDevice(),
                                                       m_pTestConfig->GetRmClient(),
                                                       &engineId));
        }
        else
        {
            CHECK_RC(m_DmaCopyAlloc.GetEngineId(m_pTestConfig->GetGpuDevice(),
                                                m_pTestConfig->GetRmClient(),
                                                m_EngineInstance,
                                                &engineId));
        }
        if (!m_pCh)
        {
            LwRm::Handle hCh;
            CHECK_RC(m_pTestConfig->AllocateChannelWithEngine(&m_pCh,
                                                              &hCh,
                                                              &m_DmaCopyAlloc,
                                                              engineId));
            m_bAllocatedChannel = true;
        }
        else if (m_pGpuDev->GetSubdevice(0)->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
        {
            UINT32 hostEngineId;
            CHECK_RC(Channel::GetHostEngineId(m_pTestConfig->GetGpuDevice(),
                                              m_pTestConfig->GetRmClient(),
                                              engineId,
                                              &hostEngineId));
            if (m_pCh->GetEngineId() != hostEngineId)
            {
                Printf(Tee::PriError,
                       "%s : Externally provided channel is not compatible with Copy Engine\n",
                       __FUNCTION__);
                return RC::SOFTWARE_ERROR;
            }
        }
        CHECK_RC(m_pCh->SetObject(LWA06F_SUBCHANNEL_COPY_ENGINE, m_DmaCopyAlloc.GetHandle()));

        if (m_SrcSurf)
            m_SrcSurf->BindGpuChannel(m_pCh->GetHandle());
        if (m_DstSurf)
            m_DstSurf->BindGpuChannel(m_pCh->GetHandle());

        m_Initialized = true;
    }
    Printf(Tee::PriDebug, "DMAWRAP: Exiting CopyEngineStrategy::Initialize\n");
    return OK;
}

//! \brief Setup a surface to act as a semaphore.
//!
//! \return RC
RC CopyEngineStrategy::InitSemaphore(Memory::Location SemaphoreLocation)
{
    RC rc;

    if (m_Semaphores.find(SemaphoreLocation) == m_Semaphores.end())
    {
        unique_ptr<NotifySemaphore> pNewSemaphore(new NotifySemaphore);
        pNewSemaphore->Setup(NotifySemaphore::FOUR_WORDS,
                             SemaphoreLocation,
                             1);     //Single semaphore instance is needed.
        CHECK_RC(pNewSemaphore->Allocate(m_pCh, 0));
        pNewSemaphore->SetPayload(0);
        m_Semaphores[SemaphoreLocation] = pNewSemaphore.release();
    }

    return OK;
}

//! \brief Performs a copyengine write between member surfaces.
//!
//! Blits a subsurface @Width pixels wide, @Heigth pixels high, from
//! starting from point (@SrcOriginX, @SrcOriginY) into destination
//! surface, from point (@DstOriginX, @DstOriginY).
//!
//! Memory layout of each of the surfaces can be either pitch or Blocklinear.
//! Copying of overlapping surfaces is not defined.
//!
//! \return RC
RC CopyEngineStrategy::WriteMethods
(
    UINT32 SrcOriginX,  //!<    X coordinate on src surf, in bytes
    UINT32 SrcOriginY,  //!<    Y coordinate on src surf, in lines
    UINT32 DstOriginX,  //!<    X coordinate on dst surf, in bytes
    UINT32 DstOriginY,  //!<    Y coordinate on dst surf, in lines
    UINT32 Width,
    UINT32 Height,
    UINT32 Payload      //!<    Value to write upon completion
)
{
    RC rc;
    Printf(Tee::PriDebug, "DMAWRAP: Exiting WriteMethods.\n");

    if (m_EnableCopyWithHoles)
    {
        // When pixel remapping is disabled, width is in bytes.
        // When enabled (copying with holes), its in pixels
        Width = Width / m_DstSurf->GetBytesPerPixel();;
    }

    switch(m_DmaCopyAlloc.GetClass())
    {
        case GF100_DMA_COPY:
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
        case PASCAL_DMA_COPY_A:
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
            rc = Write90b5Methods(SrcOriginX,
                                  SrcOriginY,
                                  DstOriginX,
                                  DstOriginY,
                                  Width,
                                  Height,
                                  Payload);
            break;
        default:
            Printf(Tee::PriNormal,
                   "DMAWRAP: Unknown CopyEngine class. ERROR!.\n");
            rc = RC::UNSUPPORTED_DEVICE;
            break;
    }
    Printf(Tee::PriDebug, "DMAWRAP: Exiting WriteMethods.\n");
    return rc;
};

#define UINT64_UPPER(n) (UINT32((n) >> 32))
#define UINT64_LOWER(n) (UINT32(n))

//! \brief Returns a 64 bit address of the pixel in a surface.
//!
//! Callwlates the address of a pixel in a given surface, depending on
//! X and Y coordinates and layout
//! (pitch, blocklinear or anything else supported by Surface2D).
//! Only first layer of the first surface in array are checked.
//! (z = 0, a = 0, look at Surface2D class).
//!
//! \param surf -- Surface to which pixel belongs to
//! \param offsetOverride -- If non-zero use override offset instead of surface
//!                          offset
//! \param X -- X coordinate of pixel, in pixels
//! \param Y -- Y coordinate of pixel, in pixels
//!
//! \return UINT64 -- address of the pixel, needed as a ie. copyengine param.
UINT64 CopyEngineStrategy::GetCtxSurfaceDmaOffset
(
    Surface2D* surf,
    UINT64 offsetOverride,
    UINT32 X,
    UINT32 Y
)
{
    UINT64 SurfCtxDmaOffset = offsetOverride;

    if (!SurfCtxDmaOffset)
        SurfCtxDmaOffset = surf->GetCtxDmaOffsetGpu();

    // Arbitrary offsets do not work with block linear formats, offsets must
    // point to the start of a GOB and then pixel/line offsets must be provided
    // in block linear specific methods.  For block linear just point to the
    // start of the surface and then use those methods for offset
    if (surf->GetLayout() != Surface2D::BlockLinear)
    {
        UINT64 PixelOffset = surf->GetPixelOffset(X / surf->GetBytesPerPixel(), Y);
        SurfCtxDmaOffset += PixelOffset;
    }
    return SurfCtxDmaOffset;
}

//! Fermi+ CE write implementation.
RC CopyEngineStrategy::Write90b5Methods
(
    UINT32 SrcOriginX,
    UINT32 SrcOriginY,
    UINT32 DstOriginX,
    UINT32 DstOriginY,
    UINT32 Width,
    UINT32 Height,
    UINT32 Payload
)
{
    RC rc;
    UINT32 SubChan = LWA06F_SUBCHANNEL_COPY_ENGINE;
    UINT32 layoutMem = 0;
    UINT32 Class = m_DmaCopyAlloc.GetClass();
    //Callwlate memory offset based on Memory layout and coordinates.
    UINT64 srcOffset64 = GetCtxSurfaceDmaOffset(m_SrcSurf, m_SrcCtxDmaOffset,
                                                SrcOriginX, SrcOriginY);
    UINT64 dstOffset64 = GetCtxSurfaceDmaOffset(m_DstSurf, m_DstCtxDmaOffset,
                                                DstOriginX, DstOriginY);

    UINT32 originXMax = DRF_MASK(LW90B5_SET_SRC_ORIGIN_X);
    UINT32 originYMax = DRF_MASK(LW90B5_SET_SRC_ORIGIN_Y);
    switch (Class)
    {
        case GF100_DMA_COPY:
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
        case PASCAL_DMA_COPY_A:
            break;
        default: // Volta+
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
            originXMax = DRF_MASK(LWC3B5_SRC_ORIGIN_X_VALUE);
            originYMax = DRF_MASK(LWC3B5_SRC_ORIGIN_Y_VALUE);
            break;
    }

    if (((Surface2D::BlockLinear == m_SrcSurf->GetLayout()) &&
         ((SrcOriginX > originXMax) ||
          (SrcOriginY > originYMax))) ||
        ((Surface2D::BlockLinear == m_DstSurf->GetLayout()) &&
         ((DstOriginX > originXMax) ||
          (DstOriginY > originYMax))))
    {
        Printf(Tee::PriHigh,
               "%s : Surface offsets too large for block linear format!\n",
               MODS_FUNCTION);
        return RC::SOFTWARE_ERROR;
    }

    if (Class == GF100_DMA_COPY)
    {
        // Set application for copy engine
        CHECK_RC(m_pCh->Write(SubChan,
                              LW90B5_SET_APPLICATION_ID,
                              DRF_DEF(90B5, _SET_APPLICATION_ID, _ID, _NORMAL)));
    }

    // Pre Pascal, offsets are limited to 40 bits, post pascal 
    const UINT32 srcOffsetUpper = UINT64_UPPER(srcOffset64);
    const UINT32 dstOffsetUpper = UINT64_UPPER(dstOffset64);
    switch (Class)
    {
        default:
        case GF100_DMA_COPY:
        case KEPLER_DMA_COPY_A:
        case MAXWELL_DMA_COPY_A:
            // 40 bit max offsets
            if (((~DRF_MASK(LW90B5_OFFSET_IN_UPPER_UPPER) & srcOffsetUpper) != 0) ||
                ((~DRF_MASK(LW90B5_OFFSET_OUT_UPPER_UPPER) & dstOffsetUpper) != 0))
            {
                Printf(Tee::PriError,
                       "Copy Engine source or destination offset too high for current GPU\n");
            }
            CHECK_RC(m_pCh->Write(SubChan, LW90B5_OFFSET_IN_UPPER,
                2,
                srcOffsetUpper, /* LW90B5_OFFSET_IN_UPPER */
                UINT64_LOWER(srcOffset64))); /* LW90B5_OFFSET_IN_LOWER */

            CHECK_RC(m_pCh->Write(SubChan, LW90B5_OFFSET_OUT_UPPER,
                2,
                dstOffsetUpper, /* LW90B5_OFFSET_OUT_UPPER */
                UINT64_LOWER(dstOffset64))); /* LW90B5_OFFSET_OUT_LOWER */
            break;

        case PASCAL_DMA_COPY_A:
        case PASCAL_DMA_COPY_B:
        case VOLTA_DMA_COPY_A:
        case TURING_DMA_COPY_A:
        case AMPERE_DMA_COPY_A:
        case AMPERE_DMA_COPY_B:
            // 48 bit max offsets
            if (((~DRF_MASK(LWC0B5_OFFSET_IN_UPPER_UPPER) & srcOffsetUpper) != 0) ||
                ((~DRF_MASK(LWC0B5_OFFSET_OUT_UPPER_UPPER) & dstOffsetUpper) != 0))
            {
                Printf(Tee::PriError,
                       "Copy Engine source or destination offset too high for current GPU\n");
            }
            CHECK_RC(m_pCh->Write(SubChan, LWC0B5_OFFSET_IN_UPPER,
                2,
                srcOffsetUpper, /* LWC0B5_OFFSET_IN_UPPER */
                UINT64_LOWER(srcOffset64))); /* LWC0B5_OFFSET_IN_LOWER */

            CHECK_RC(m_pCh->Write(SubChan, LWC0B5_OFFSET_OUT_UPPER,
                2,
                dstOffsetUpper, /* LWC0B5_OFFSET_OUT_UPPER */
                UINT64_LOWER(dstOffset64))); /* LWC0B5_OFFSET_OUT_LOWER */
            break;

        case HOPPER_DMA_COPY_A:
        case BLACKWELL_DMA_COPY_A:
            // 56 bit max offsets
            if (((~DRF_MASK(LWC8B5_OFFSET_IN_UPPER_UPPER) & srcOffsetUpper) != 0) ||
                ((~DRF_MASK(LWC8B5_OFFSET_OUT_UPPER_UPPER) & dstOffsetUpper) != 0))
            {
                Printf(Tee::PriError,
                       "Copy Engine source or destination offset too high for current GPU\n");
            }
            CHECK_RC(m_pCh->Write(SubChan, LWC8B5_OFFSET_IN_UPPER,
                2,
                srcOffsetUpper, /* LWC8B5_OFFSET_IN_UPPER */
                UINT64_LOWER(srcOffset64))); /* LWC8B5_OFFSET_IN_LOWER */

            CHECK_RC(m_pCh->Write(SubChan, LWC8B5_OFFSET_OUT_UPPER,
                2,
                dstOffsetUpper, /* LWC8B5_OFFSET_OUT_UPPER */
                UINT64_LOWER(dstOffset64))); /* LWC8B5_OFFSET_OUT_LOWER */
            break;
    }

    if (Surface2D::Pitch == m_SrcSurf->GetLayout())
    {
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH);
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_PITCH_IN,  m_SrcSurf->GetPitch()));
    }
    else
    {
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _BLOCKLINEAR);
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_SRC_BLOCK_SIZE,
            DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _WIDTH,
                m_SrcSurf->GetLogBlockWidth()) |
            DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _HEIGHT,
                m_SrcSurf->GetLogBlockHeight()) |
            DRF_NUM(90B5, _SET_SRC_BLOCK_SIZE, _DEPTH,
                m_SrcSurf->GetLogBlockDepth()) |
            DRF_DEF(90B5, _SET_SRC_BLOCK_SIZE, _GOB_HEIGHT, _GOB_HEIGHT_FERMI_8)));

        UINT32 SrcBPP = m_SrcSurf->GetBytesPerPixel();
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_SRC_WIDTH,
            m_SrcSurf->GetAllocWidth()*SrcBPP));
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_SRC_HEIGHT,
            m_SrcSurf->GetHeight()));
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_SRC_DEPTH,
            m_SrcSurf->GetDepth()));

        switch (Class)
        {
            case GF100_DMA_COPY:
            case KEPLER_DMA_COPY_A:
            case MAXWELL_DMA_COPY_A:
            case PASCAL_DMA_COPY_A:
                CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_SRC_ORIGIN,
                                      DRF_NUM(90B5, _SET_SRC_ORIGIN, _X, SrcOriginX) |
                                      DRF_NUM(90B5, _SET_SRC_ORIGIN, _Y, SrcOriginY)));
                break;
            default: // Volta+
            case PASCAL_DMA_COPY_B:
            case VOLTA_DMA_COPY_A:
            case TURING_DMA_COPY_A:
            case AMPERE_DMA_COPY_A:
            case AMPERE_DMA_COPY_B:
            case HOPPER_DMA_COPY_A:
            case BLACKWELL_DMA_COPY_A:
                CHECK_RC(m_pCh->Write(SubChan, LWC3B5_SRC_ORIGIN_X, 2,
                                      DRF_NUM(C3B5, _SRC_ORIGIN_X, _VALUE, SrcOriginX),
                                      DRF_NUM(C3B5, _SRC_ORIGIN_Y, _VALUE, SrcOriginY)));
                break;
        }
    }

    if (Surface2D::Pitch == m_DstSurf->GetLayout())
    {
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH);
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_PITCH_OUT, m_DstSurf->GetPitch()));
    }
    else
    {
        layoutMem |= DRF_DEF(90B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _BLOCKLINEAR);
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_DST_BLOCK_SIZE,
            DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _WIDTH,
                m_DstSurf->GetLogBlockWidth()) |
            DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _HEIGHT,
                m_DstSurf->GetLogBlockHeight()) |
            DRF_NUM(90B5, _SET_DST_BLOCK_SIZE, _DEPTH,
                m_DstSurf->GetLogBlockDepth()) |
            DRF_DEF(90B5, _SET_DST_BLOCK_SIZE, _GOB_HEIGHT, _GOB_HEIGHT_FERMI_8)));

        UINT32 DstBPP = m_DstSurf->GetBytesPerPixel();
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_DST_WIDTH,
            m_DstSurf->GetAllocWidth()*DstBPP));
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_DST_HEIGHT,
            m_DstSurf->GetHeight()));
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_DST_DEPTH,
            m_DstSurf->GetDepth()));

        switch (Class)
        {
            case GF100_DMA_COPY:
            case KEPLER_DMA_COPY_A:
            case MAXWELL_DMA_COPY_A:
            case PASCAL_DMA_COPY_A:
                CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_DST_ORIGIN,
                                      DRF_NUM(90B5, _SET_DST_ORIGIN, _X, DstOriginX) |
                                      DRF_NUM(90B5, _SET_DST_ORIGIN, _Y, DstOriginY)));
                break;
            default: // Volta+
            case PASCAL_DMA_COPY_B:
            case VOLTA_DMA_COPY_A:
            case TURING_DMA_COPY_A:
            case AMPERE_DMA_COPY_A:
            case AMPERE_DMA_COPY_B:
            case HOPPER_DMA_COPY_A:
            case BLACKWELL_DMA_COPY_A:
                CHECK_RC(m_pCh->Write(SubChan, LWC3B5_DST_ORIGIN_X, 2,
                                      DRF_NUM(C3B5, _DST_ORIGIN_X, _VALUE, DstOriginX),
                                      DRF_NUM(C3B5, _DST_ORIGIN_Y, _VALUE, DstOriginY)));
                break;
        }
    }

    if (Class == GF100_DMA_COPY)
    {
        UINT32 AddressMode = 0;
        AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TYPE, _VIRTUAL);
        AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _DST_TYPE, _VIRTUAL);
        switch (m_SrcSurf->GetLocation())
        {
            case Memory::Fb:
                AddressMode |= DRF_DEF(90B5, _ADDRESSING_MODE, _SRC_TARGET, _LOCAL_FB);
                break;

            case Memory::Coherent:
                AddressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _SRC_TARGET,
                                       _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                AddressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _SRC_TARGET, _NONCOHERENT_SYSMEM);
                break;

            default:
                Printf(Tee::PriHigh,"Unsupported memory type for source\n");
                return RC::BAD_MEMLOC;
        }

        switch (m_DstSurf->GetLocation())
        {
            case Memory::Fb:
                AddressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _DST_TARGET,
                                       _LOCAL_FB);
                break;

            case Memory::Coherent:
                AddressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _DST_TARGET,
                                       _COHERENT_SYSMEM);
                break;

            case Memory::NonCoherent:
                AddressMode |= DRF_DEF(90B5,
                                       _ADDRESSING_MODE,
                                       _DST_TARGET,
                                       _NONCOHERENT_SYSMEM);
                break;

            default:
                Printf(Tee::PriHigh,"Unsupported memory type for destination\n");
                return RC::BAD_MEMLOC;
        }
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_ADDRESSING_MODE, AddressMode));
    }
    else
    {
        if (m_bSrcPhysical)
        {
            layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _PHYSICAL);

            Memory::Location srcPhysTarget = m_SrcPhysicalTarget;
            if (srcPhysTarget == Memory::Optimal)
                srcPhysTarget = m_SrcSurf->GetLocation();

            UINT32 physTargetValue = LWA0B5_SET_SRC_PHYS_MODE_TARGET_LOCAL_FB;
            switch (srcPhysTarget)
            {
                case Memory::Fb:
                    physTargetValue = LWA0B5_SET_SRC_PHYS_MODE_TARGET_LOCAL_FB;
                    break;
                case Memory::Coherent:
                    physTargetValue = LWA0B5_SET_SRC_PHYS_MODE_TARGET_COHERENT_SYSMEM;
                    break;
                case Memory::NonCoherent:
                case Memory::CachedNonCoherent:
                    physTargetValue = LWA0B5_SET_SRC_PHYS_MODE_TARGET_NONCOHERENT_SYSMEM;
                    break;
                case Memory::Optimal:
                default:
                    Printf(Tee::PriError,
                           "Unsupported source physical target type : %d\n",
                           srcPhysTarget);
                    return RC::SOFTWARE_ERROR;
            }
            CHECK_RC(m_pCh->Write(SubChan, LWA0B5_SET_SRC_PHYS_MODE, 1, physTargetValue));
        }
        else
        {
            layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _VIRTUAL);
        }
        if (m_bDstPhysical)
        {
            layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL);

            Memory::Location dstPhysTarget = m_DstPhysicalTarget;
            if (dstPhysTarget == Memory::Optimal)
                dstPhysTarget = m_DstSurf->GetLocation();

            UINT32 physTargetValue = LWA0B5_SET_DST_PHYS_MODE_TARGET_LOCAL_FB;
            switch (dstPhysTarget)
            {
                case Memory::Fb:
                    physTargetValue = LWA0B5_SET_DST_PHYS_MODE_TARGET_LOCAL_FB;
                    break;
                case Memory::Coherent:
                    physTargetValue = LWA0B5_SET_DST_PHYS_MODE_TARGET_COHERENT_SYSMEM;
                    break;
                case Memory::NonCoherent:
                case Memory::CachedNonCoherent:
                    physTargetValue = LWA0B5_SET_DST_PHYS_MODE_TARGET_NONCOHERENT_SYSMEM;
                    break;
                case Memory::Optimal:
                default:
                    Printf(Tee::PriError,
                           "Unsupported source physical target type : %d\n",
                           dstPhysTarget);
                    return RC::SOFTWARE_ERROR;
            }
            CHECK_RC(m_pCh->Write(SubChan, LWA0B5_SET_DST_PHYS_MODE, 1, physTargetValue));
        }
        else
        {
            layoutMem |= DRF_DEF(A0B5, _LAUNCH_DMA, _DST_TYPE, _VIRTUAL);
        }
    }

    CHECK_RC(m_pCh->Write(SubChan, LW90B5_LINE_LENGTH_IN, Width));
    CHECK_RC(m_pCh->Write(SubChan, LW90B5_LINE_COUNT, Height));

    UINT32 semaFlag = DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _NONE);
    if (m_WriteSemaphores)
    {
        // Setup the completion semaphore
        CHECK_RC(m_pCh->Write(SubChan,
            LW90B5_SET_SEMAPHORE_A,
            2,
            UINT64_UPPER(m_Semaphores[m_SelectedNfyLoc]->GetCtxDmaOffsetGpu(0)), /* LW90B5_SET_SEMAPHORE_A */
            UINT64_LOWER(m_Semaphores[m_SelectedNfyLoc]->GetCtxDmaOffsetGpu(0)))); /* LW90B5_SET_SEMAPHORE_B */

        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_SEMAPHORE_PAYLOAD, Payload));
        semaFlag = DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, _RELEASE_FOUR_WORD_SEMAPHORE);
    }

    UINT32 remapFlag = DRF_DEF(90B5, _LAUNCH_DMA, _REMAP_ENABLE, _FALSE);
    if (m_EnableCopyWithHoles)
    {
        MASSERT(m_SrcSurf->GetBytesPerPixel() > 0 && m_SrcSurf->GetBytesPerPixel() <= 4 &&
                m_DstSurf->GetBytesPerPixel() > 0 && m_DstSurf->GetBytesPerPixel() <= 4);

        UINT32 remapComponent = 0;
        remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _COMPONENT_SIZE, _ONE);
        remapComponent |= DRF_NUM(90B5, _SET_REMAP_COMPONENTS, _NUM_SRC_COMPONENTS, m_SrcSurf->GetBytesPerPixel() - 1);
        remapComponent |= DRF_NUM(90B5, _SET_REMAP_COMPONENTS, _NUM_DST_COMPONENTS, m_DstSurf->GetBytesPerPixel() - 1);

        (m_CopyMask & 0x1) ?
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_X, _SRC_X):
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_X, _NO_WRITE);
        (m_CopyMask & 0x2) ?
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_Y, _SRC_Y):
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_Y, _NO_WRITE);
        (m_CopyMask & 0x4) ?
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_Z, _SRC_Z):
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_Z, _NO_WRITE);
        (m_CopyMask & 0x8) ?
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_W, _SRC_W):
            remapComponent |= DRF_DEF(90B5, _SET_REMAP_COMPONENTS, _DST_W, _NO_WRITE);

        remapFlag = DRF_DEF(90B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE);
        CHECK_RC(m_pCh->Write(SubChan, LW90B5_SET_REMAP_COMPONENTS, remapComponent));
    }

    UINT32 flushFlag = DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE);
    if (m_DisableFlush)
    {
        flushFlag = DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _FALSE);
    }
    if (Class >= VOLTA_DMA_COPY_A)
    {
        flushFlag |= DRF_DEF(C3B5, _LAUNCH_DMA, _FLUSH_TYPE, _SYS);
    }

    UINT32 pipelineFlag = DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED);
    if (m_UsePiplinedCopies)
    {
        pipelineFlag = DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _PIPELINED);
    }

    // Use NON_PIPELINED to WAR hw bug 618956.
    CHECK_RC(m_pCh->Write(SubChan, LW90B5_LAUNCH_DMA,
                          layoutMem |
                          semaFlag |
                          remapFlag |
                          DRF_DEF(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE, _NONE) |
                          flushFlag |
                          pipelineFlag |
                          DRF_DEF(90B5,
                                  _LAUNCH_DMA,
                                  _MULTI_LINE_ENABLE,
                                  _TRUE)));

    return OK;
}

RC CopyEngineStrategy::GetCopyTimeUs(UINT64 *pStartTimeUs, UINT64 *pEndTimeUs)
{
    *pStartTimeUs = m_CopyStartTimeUs;
    *pEndTimeUs = m_CopyEndTimeUs;
    return OK;
}

RC CopyEngineStrategy::SaveCopyTime(bool bSaveCopyTime)
{
    RC rc;

    m_SaveCopyTime = bSaveCopyTime;

    if (!m_SaveCopyTime)
    {
        return rc;
    }

    if (!m_StartSemaSurf)
    {
        m_StartSemaSurf = make_unique<Surface2D>();
        m_StartSemaSurf->SetWidth(4);
        m_StartSemaSurf->SetHeight(1);
        m_StartSemaSurf->SetColorFormat(ColorUtils::VOID8);
        m_StartSemaSurf->SetLocation(Memory::NonCoherent);
        m_StartSemaSurf->SetProtect(Memory::ReadWrite);
        m_StartSemaSurf->SetLayout(Surface2D::Pitch);

        CHECK_RC(m_StartSemaSurf->Alloc(GetGpuDev()));
        CHECK_RC(m_StartSemaSurf->Fill(0));
        CHECK_RC(m_StartSemaSurf->Map());

        m_StartSemaSurf->BindGpuChannel(m_pCh->GetHandle());
    }
    return rc;
}

void CopyEngineStrategy::WriteStartTimeSemaphore(bool bWrite)
{
    m_WriteStartTimeSemaphore = bWrite;
}

