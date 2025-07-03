/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _DMA_READER_H_
#define _DMA_READER_H_

#include "mdiag/utils/types.h"
#include "mdiag/utils/surf_types.h"

#ifndef INCLUDED_GPU_H
#include "core/include/gpu.h"
#endif

class RC;
class ArgReader;
class IGpuSurfaceMgr;
class LWGpuResource;
class LWGpuChannel;
class MdiagSurf;
class SmcEngine;

struct PollDmaDoneArgs
{
    const static UINT32 ExpectedPayload = 0x12347777;
    uintptr_t pSemaphorePayload;
};

//! Reads from surfaces using DMA.
class DmaReader
{
public:

    enum EngineType
    {
        CE,
        GPUCRC_FECS,
        CEPRI
    };

    enum WfiType
    {
        POLL,
        NOTIFY,
        INTR,
        SLEEP
    };

    static EngineType GetEngineType(const ArgReader* params, LWGpuResource* lwgpu);
    static WfiType GetWfiType(const ArgReader* params);

    EngineType GetEngineType() const { return m_EngineType; }

    //! virtual d'tor
    virtual ~DmaReader();

    //! Read the specified number of bytes into the internal DMA transfer buffer.
    //! \param dmaCtxHandle Handle of DMA context.
    //! \param pos Position within the surface where to start reading.
    //! \param size Number of bytes to read.
    //! \param subdev Subdevice from which to read.
    RC Read(UINT32 dmaCtxHandle, UINT64 pos, UINT32 size, UINT32 subdev, const Surface2D* srcSurf);

    //! dma line from vidmem to sysmem
    virtual RC ReadLine
    (
        UINT32 hSrcCtxDma, UINT64 SrcOffset, UINT32 SrcPitch,
        UINT32 LineCount, UINT32 LineLength,
        UINT32 gpuSubdevice, const Surface2D* srcSurf,
        MdiagSurf* dstSurf = 0, UINT64 DstOffset = 0
    ) = 0;

    //! gets the dma reader class id
    virtual UINT32 GetClassId() const = 0;

    //! Allocates appropriate dma object via RM
    virtual bool AllocateObject();

    //! Allocates dma notifier and handles
    bool AllocateNotifier(_PAGE_LAYOUT Layout);

    //! Allocates destination dma buffer and handles
    bool AllocateDstBuffer(_PAGE_LAYOUT Layout, UINT32 size, _DMA_TARGET target);

    //! Returns DMA transfer buffer.
    uintptr_t GetBuffer() const { return m_DstDmaBase; }

    //! Returns size of the DMA transfer buffer.
    UINT32 GetBufferSize() const { return m_DstBufSize; }

    //! Returns destination dma buffer handle
    UINT32 GetDstBufHandle() const { return m_DstDmaHandle; }

    //! Returns destination dma buffer handle
    UINT64 GetDstGpuOffset() const { return m_DstGpuOffset; }

    //! Poll for dma transfer completion
    static bool PollDmaDone(void* pollargs);

    //! Factory function to create appropriate DMAReader objects
    static DmaReader* CreateDmaReader
    (
        EngineType engineType,
        WfiType wfiType,
        LWGpuResource* lwgpu,
        LWGpuChannel* channel,
        UINT32 size,
        const ArgReader* params,
        LwRm *pLwRm,
        SmcEngine *pSmcEngine,
        UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV
    );

    void SetCreateDmaChannel(LwRm* pLwRm, SmcEngine* pSmcEngine, UINT32 engineId);
    LWGpuChannel* GetChannel() { return m_Channel; }

    void SetSubDevice (UINT32 subdev) { m_Subdev = subdev; }

    //! Bind surfaces to be CRC checked to the new CRC channel
    int BindSurface ( UINT32 handle );

    // copy surf to m_CopyLocation
    virtual RC CopySurface
    (
        MdiagSurf* surf,
        _PAGE_LAYOUT Layout,
        UINT32 gpuSubdevice,
        UINT32 Offset = 0
    ) { return OK; }

    void SetCopyLocation(Memory::Location location) { m_CopyLocation = location; }

    UINT32 GetCopyHandle() const { return m_CopyDmaHandle; }
    UINT64 GetCopyBuffer() const { return m_CopyGpuOffset; }

    RC AllocateBuffer(_PAGE_LAYOUT Layout, UINT64 size);
    MdiagSurf* GetCopySurface() { return m_pSurface; }

    virtual bool NoPitchCopyOfBlocklinear() const { return false; }

    virtual RC ReadBlocklinearToPitch
    (
        UINT64 sourceVirtAddr,
        UINT32 x,
        UINT32 y,
        UINT32 z,
        UINT32 lineLength,
        UINT32 lineCount,
        UINT32 gpuSubdevice,
        const Surface2D* srcSurf
    );

protected:
    //! c'tor
    DmaReader(WfiType wfiType, LWGpuChannel* ch, UINT32 size, EngineType engineType);

    LWGpuChannel* m_Channel;
    LWGpuResource* m_LWGpuResource;
    UINT32        m_DmaObjHandle;

    // destination dma buffer
    UINT32    m_DstBufSize;
    UINT32    m_DstDmaHandle;
    uintptr_t m_DstDmaBase;
    UINT64    m_DstGpuOffset;
    Memory::Location m_DstLocation;

    // dma notifier
    UINT32    m_NotifierHandle;
    uintptr_t m_NotifierBase;
    UINT64    m_NotifierGpuOffset;

    // channel class
    UINT32 m_ChClass;

    // Subdevice
    UINT32 m_Subdev;
    const ArgReader* m_Params;

    WfiType m_wfiType;
    EngineType m_EngineType;

    MdiagSurf* m_pSurface;
    UINT32    m_CopyDmaHandle;
    UINT64    m_CopyGpuOffset;
    Memory::Location m_CopyLocation;

private:
    // Prevent copying
    DmaReader(const DmaReader&);
    DmaReader& operator=(const DmaReader&);

    bool IsNewChannelNeeded();

    // tag the newly created channel/subchannel
    // free them in destructor
    bool m_IsNewChannelCreated;
    bool m_IsNewObjCreated;
};

#endif
