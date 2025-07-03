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

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_DMAWRAPPER_H
#define INCLUDED_DMAWRAPPER_H

#include "nfysema.h"
#include "core/include/channel.h"
#include "core/include/rc.h"
#include "core/include/memoryif.h"
#include "gralloc.h"
#include <memory>

class Surface2D;
class GpuDevice;
class IDmaStrategy;
class GpuTestConfiguration;

//! \class DmaWrapper
//!
//! \brief Copy memory from one surface to another.
//!
//! This is a wrapper class around Vic, 2D and CopyEngine classes.
//! General idea is to make it as interchangeable as possible.
class DmaWrapper
{
public:
    enum PREF_TRANS_METH
    {
         COPY_ENGINE
        ,TWOD
        ,VIC
        ,DEFAULT_METH
    };

    static const UINT32 DEFAULT_ENGINE_INSTANCE = ~0U;

    DmaWrapper() = default;
    explicit DmaWrapper(GpuTestConfiguration* pTestConfig);
    DmaWrapper
    (
        Surface2D*  pSrcSurface2D,
        Surface2D*  pDstSurface2D,
        GpuTestConfiguration* pTestConfig
    );

    ~DmaWrapper();
    RC Cleanup();
    RC Initialize
    (
        GpuTestConfiguration* pTestConfig, //!< TestConfig to allocate channels on
        Memory::Location NfyLoc,    //!< Memory in which to alloc semaphore.
        PREF_TRANS_METH transferMethod = DEFAULT_METH  //! Dma dev to use.
    );

    RC Initialize
    (
        GpuTestConfiguration* pTestConfig, //!< TestConfig to allocate channels on
        Memory::Location NfyLoc,           //!< Memory in which to alloc semaphore.
        PREF_TRANS_METH transferMethod,    //! Dma dev to use.
        UINT32 Instance                    //! Instance number of the device
    );

    RC Initialize
    (
        GpuTestConfiguration* pTestConfig, //!< TestConfig to allocate channels on
        Channel *pChannel,                 //!< Reuse an existed channel if any
        Memory::Location NfyLoc,           //!< Memory in which to alloc semaphore.
        PREF_TRANS_METH transferMethod,    //! Dma dev to use.
        UINT32 Instance                    //! Instance number of the device
    );

    RC SetSurfaces(Surface2D* src, Surface2D* dst);
    RC SetSrcDstSurf(Surface2D* src, Surface2D* dst);
    RC SetCtxDmaOffsets(UINT64 srcCtxDmaOffset, UINT64 dstCtxDmaOffset);
    RC IsCopyComplete(bool *pComplete);
    bool IsInitialized() { return m_Initialized; }
    void SetCopyMask(UINT32 copyMask);
    void SetUsePipelinedCopies(bool bUsePipelining);
    void SetUsePhysicalCopy(bool bSrcPhysical, bool bDstPhysical);
    void SetPhysicalCopyTarget(Memory::Location srcLoc, Memory::Location dstLoc);
    void SetFlush(bool bEnable);

    UINT32 GetPitchAlignRequirement() const;

    RC Copy
    (
        UINT32 SrcOriginX,        //!< Starting src X, in bytes not pixels.
        UINT32 SrcOriginY,        //!< Starting src Y, in lines.
        UINT32 WidthBytes,        //!< Width of copied rect, in bytes.
        UINT32 Height,            //!< Height of copied rect, in bytes
        UINT32 DstOriginX,        //!< Starting dst X, in bytes not pixels.
        UINT32 DstOriginY,        //!< Starting dst Y, in lines.
        FLOAT64 TimeoutMs,        //!< Timeout for dma operation
        UINT32 SubdevNum,         //!< Subdevice number we are working on
        bool   ShouldWait = true, //!< Should method wait for completion.
        bool   flush = true,      //!< Immediately flush GPU methods.
        bool   writeSemaphores = true//!< Write semaphores , used for synchronization
    );
    RC Wait(FLOAT64 TimeoutMs);
    RC GetCopyTimeUs(UINT64 *pStartTimeUs, UINT64 *pEndTimeUs);
    RC SaveCopyTime(bool bSaveCopyTime);
    void WriteStartTimeSemaphore(bool bWrite);

private:
    IDmaStrategy*   m_TransImpl         = nullptr;
    bool            m_Initialized       = false;
    PREF_TRANS_METH m_TransferMethod    = DEFAULT_METH;
    Surface2D *     m_pSrcSurf          = nullptr;
    Surface2D *     m_pDstSurf          = nullptr;
    UINT64          m_SrcCtxDmaOffset   = 0;
    UINT64          m_DstCtxDmaOffset   = 0;

    Memory::Location m_NfyLoc           = Memory::Optimal;

    //! Synchronisation bookkeeping. We keep track of number of waits/flushes
    //! we had, keeping then them in sync. We are in stationary state when
    //! number of waits equals number of flushes (duh.)

    UINT32 m_NumFlushes = 0;  //!< Number of times write operation has been inited.
    UINT32 m_NumWaits   = 0;  //!< Number of times we waited on a write to finish.

    bool IsPitchAlignCorrect() const;
};

//! Utility class for copying rectangles between surfaces using VIC engine on CheetAh.
//!
//! Used as one of the implementations of DmaWrapper, becomes the default on CheetAh.
//! Also used as an option in NewWfMats.
class VICDmaWrapper
{
    public:
        VICDmaWrapper();
        ~VICDmaWrapper();

        //! Frees allocated GPU resources
        RC Free();

        //! Determines if VIC is supported
        static bool IsSupported(GpuDevice* pGpuDev);

        struct SurfDesc
        {
            UINT32               width;        //!< Surface width in pixels
            UINT32               copyWidth;    //!< Width of the copied rectangle, in pixels
            UINT32               copyHeight;   //!< Copy height
            Surface2D::Layout    layout;       //!< Surface layout (e.g. pitch or block-linear)
            UINT32               logBlkHeight; //!< Block-linear surface parameter
            ColorUtils::Format   colorFormat;  //!< Surface color format
            Channel::MappingType mappingType;  //!< The way the surface is mapped in SMMU

            bool operator==(const SurfDesc& other) const;
            static SurfDesc FromSurface2D(const Surface2D& surf);
        };

        //! Allocates and fills the necessary config structures for VIC
        RC Initialize
        (
            Channel*   pCh,
            GpuDevice* pGpuDev,
            FLOAT64    timeoutMs,
            SurfDesc   srcSurf,
            SurfDesc   destSurf
        );

        //! Send methods for copying one rectangle from one place to another
        //!
        //! The function just writes the necessary methods to the channel
        //! and performs flush, but does not wait for any of the methods to finish.
        //! The caller has to call Channel::WaitIdle() in order to wait for all
        //! the scheduled copies to finish.
        //! The object must not be freed before all the copies finish.
        RC Copy
        (
            UINT32 hMemSrc,        //!< Physical memory handle of the source surface
            UINT32 srcOffset,      //!< Source offset, in bytes
            UINT32 hMemDst,        //!< Physical memory handle of the destination surface
            UINT32 destOffset      //!< Destination offset, in bytes
        );

        //! Write a release semaphore method
        RC SemaphoreRelease
        (
            const Surface2D& semaSurf,   //!< Surface containing the semaphore location
            UINT32           offset,     //!< Offset of the semaphore location to write
            UINT32           payload     //!< Payload to write
        );

        //! Write a VIC release semaphore method to an external channel
        static RC SemaphoreRelease
        (
            Channel*             pCh,        //!< Channel to write methods to
            const Surface2D&     semaSurf,   //!< Surface containing the semaphore location
            UINT32               offset,     //!< Offset of the semaphore location to write
            UINT32               payload,    //!< Payload to write
            Channel::SemRelFlags flags       //!< 16B sema if true, 4B sema if false
        );

        //! Flush the channel and ensure that setup methods are sent
        //! the next time Copy is ilwoked.
        RC Flush();

    private:
        // Non-copyable
        VICDmaWrapper(const VICDmaWrapper&);
        VICDmaWrapper& operator=(const VICDmaWrapper&);

        struct Impl;
        unique_ptr<Impl> m_pImpl;
};

#endif // ! INCLUDED_DMAWRAP_H
