/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/gpu.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "lwsurf.h"
#include "surf2d.h"
#include <map>
#include <string>

class Display;

class GpuSubdevice;
class GpuDevice;
class Surface2D;
class Channel;
class LwRm;

//! The GpuUtility namespace.
namespace GpuUtility
{
    struct DisplayImageDescription
    {
        LwRm::Handle       hMem              = 0;
        UINT32             CtxDMAHandle      = 0;
        UINT64             Offset            = 0;
        LwRm::Handle       hMemRight         = 0;
        UINT32             CtxDMAHandleRight = 0;
        UINT64             OffsetRight       = 0;
        UINT32             Height            = 0;
        UINT32             Width             = 0;
        UINT32             AASamples         = 0;
        Surface2D::Layout  Layout            = Surface2D::Pitch;
        UINT32             Pitch             = 0;
        UINT32             LogBlockHeight    = 0;
        UINT32             NumBlocksWidth    = 0;
        ColorUtils::Format ColorFormat       = ColorUtils::LWFMT_NONE;

        RC Print(Tee::Priority Priority) const;
        RC Print(Tee::PriDebugStub) const
        {
            return Print(Tee::PriSecret);
        }
        DisplayImageDescription();
        void Reset();
    };

    struct MemoryChunkDesc
    {
        UINT64 size;                //!< Bytes in this chunk
        UINT64 ctxDmaOffsetGpu;     //!< Ctxdma offset inside hCtxdDma (for PageModel)
        UINT32 hCtxDma;             //!< Handle of GPU ctxdma.
        UINT32 hCtxDmaIso;          //!< Handle of ISO/EVO ctxdma (if any).
        void  *pDisplayMgrTestContext; //!< DisplayMgr::TestContext where the chunk is displayed.
        UINT32 hMem;                //!< Memory handle
        UINT32 pteKind;             //!< Page Table Entry "kind" (for BlockLinear)
        UINT32 pageSizeKB;          //!< Page size in kilobytes.
        bool   isDisplayable;       //!< Chunk can be scanned by the display engine.
        bool   contiguous;          //!< Chunk is physically contiguous
        bool   needsFree = true;    //!< Chunk requires freeing
        UINT64 mapOffset = 0;       //!< Offset within memory handle, for mapping
        UINT64 fbOffset;            //!< Offset of the chunk in FB, if contiguous
    };

    const UINT64 UNDEFINED_OFFSET = _UINT64_MAX; //!< fbOffset if !contiguous

    //! Memory chunks returned by AllocateEntireFramebuffer
    typedef std::vector<MemoryChunkDesc> MemoryChunks;

    RC AllocateEntireFramebuffer(UINT64 minChunkSize,
                                 UINT64 maxChunkSize,
                                 UINT64 maxSize,
                                 MemoryChunks *pChunks,
                                 bool blockLinear,
                                 UINT64 minPageSize,
                                 UINT64 maxPageSize,
                                 UINT32 hChannel,
                                 GpuDevice *pGpuDev,
                                 UINT32 minPercentReqd,
                                 bool contiguous);

    RC FreeEntireFramebuffer(MemoryChunks *pChunks,
                             GpuDevice *pGpuDev);

    RC FindDisplayableChunk(MemoryChunks     *pChunks,
                            MemoryChunkDesc **ppDisplayChunk,
                            UINT64           *pDisplayOffset,
                            UINT64            StartLocation,
                            UINT64            endLocation,
                            UINT32            height,
                            UINT32            pitch,
                            GpuDevice        *pGpuDev);

    RC GetPhysAddress(const MemoryChunks& chunks,
                      UINT64 ctxDmaOffset,
                      PHYSADDR* pPhysAddr,
                      UINT64* pContigSize);

    RC GetPhysAddress(const MemoryChunkDesc& chunk,
                      UINT64 chunkOffset,
                      PHYSADDR* pPhysAddr,
                      UINT64* pContigSize);

    RC GetPteKind(LwU64 gpuAddr,
                  LwRm::Handle hVASpace,
                  GpuDevice* pGpuDev,
                  INT32* pteKind,
                  LwRm* pLwRm);

    RC SetImageOnChunkLegacy(Display   *pDisplay,
                       MemoryChunkDesc *pChunk,
                       UINT64           chunkOffset,
                       UINT32           width,
                       UINT32           height,
                       UINT32           depth,
                       UINT32           pitch,
                       GpuDevice       *pGpuDev);

    RC FillRandomOnGpu(GpuDevice *pGpuDev,
                        LwRm::Handle handle,
                        UINT64 size,
                        UINT32 seed);

    RC FillConstantOnGpu(GpuDevice *pGpuDev,
                        LwRm::Handle handle,
                        UINT64 size,
                        UINT32 constant);

    RC GetBlockedCRCOnGpu(GpuDevice *pGpuDev,
                        LwRm::Handle handle,
                        UINT64 offset,
                        UINT64 size,
                        Memory::Location loc,
                        UINT32 *pCrc);

    RC GetBlockedCRCOnGpuPerComponent(GpuDevice *pGpuDev,
                        LwRm::Handle handle,
                        UINT64 offset,
                        UINT64 size,
                        Memory::Location loc,
                        vector<UINT32> *pCrc);

    RC CompareBuffers(GpuDevice *pGpuDev,
                LwRm::Handle handle1,
                UINT64 size1,
                Memory::Location loc1,
                LwRm::Handle handle2,
                UINT64 size2,
                Memory::Location loc2,
                bool *pResult);   //if true, buffers are same

    RC CompareBuffersComponents(GpuDevice *pGpuDev,
                LwRm::Handle handle1,
                UINT64 size1,
                Memory::Location loc1,
                LwRm::Handle handle2,
                UINT64 size2,
                Memory::Location loc2,
                UINT32 mask,
                bool *pResult);    //if true, buffers are same

    // Get the timestamp of the LW timer
    UINT32 GetLwTimerMs(GpuSubdevice *subdev);

    void PrintRamContents(Tee::Priority pri,
                          UINT32 partition, UINT32 ram, UINT32 subram,
                          GpuSubdevice *pSubdev);

    const UINT32 DISPLAY_DEFAULT_PUTCHARS = 0xbeefface;

    RC PutChars
    (
        INT32  x,
        INT32  y,
        const string &chars,
        INT32  scale,
        UINT32 foregroundColor,
        UINT32 backgroundColor,
        bool   bUnderline,
        Surface2D    * pSurf
    );

    RC PutChar
    (
        INT32  x,
        INT32  y,
        char   c,
        INT32  scale,
        UINT32 foregroundColor,
        UINT32 backgroundColor,
        bool   bUnderline,
        Surface2D    * pSurf
    );

    //! Draw 'Scaled' 8x8 'Chars' starting at (X,Y) in graphics mode.
    //! Upper left corner is (0,0).
    RC PutChars
    (
        INT32  x,
        INT32  y,
        const string &chars,
        INT32  scale           = 1,
        UINT32 foregroundColor = 0xFFFFFFFF,
        UINT32 backgroundColor = 0,
        UINT32 disp            = DISPLAY_DEFAULT_PUTCHARS,
        GpuSubdevice * pSubdev = nullptr,
        Surface2D::Layout layout = Surface2D::Pitch
    );

    //! Draw 'Scaled' 8x8 'Chars' horizontially centered
    //! Upper left corner is (0,0).
    RC PutCharsHorizCentered
    (
        INT32  screenWidth,
        INT32  y,
        string chars,
        INT32  scale,
        UINT32 foregroundColor,
        UINT32 backgroundColor,
        bool   bUnderline,
        Surface2D* pSurface
    );

    //! Draw 'Scaled' 8x8 'Chars' horizontially centered
    //! Upper left corner is (0,0).
    RC PutCharsHorizCentered
    (
        INT32  screenWidth,
        INT32  y,
        string chars,
        INT32  scale           = 1,
        UINT32 foregroundColor = 0xFFFFFFFF,
        UINT32 backgroundColor = 0,
        UINT32 disp            = DISPLAY_DEFAULT_PUTCHARS,
        GpuSubdevice * pSubdev = 0,
        Surface2D::Layout layout = Surface2D::Pitch
    );

    RC DmaCopySurf(Surface2D* pSrcSurf,
                   Surface2D* pDstSurf,
                   GpuSubdevice* pSubdev,
                   FLOAT64 timeoutMs);

    RC FillRgb(Surface2D* pDstSurf,
               FLOAT64 timeoutMs,
               GpuSubdevice* pSubdev,
               Surface2D* pCachedSurf = 0);

    RC GetGpuCacheExist(GpuDevice *pGpuDevice, bool *pGpuCacheExist, LwRm* pLwRm);
    bool MustRopSysmemBeCached(GpuDevice *pGpuDevice, LwRm* pLwRm, UINT32 hChannel);

    RC DecodeRangePartChan
    (
        GpuSubdevice* pSubdev,
        const GpuUtility::MemoryChunkDesc& chunk,
        UINT64 begin,
        UINT64 end,
        vector<UINT08>* pChunks,
        UINT32* pChunkSize
    );

    void SleepOnPTimer(FLOAT64 milliseconds, GpuSubdevice *pSubdev);

    class RmMsgFilter
    {
    public:
        explicit RmMsgFilter() { }
        ~RmMsgFilter();

        RC Set(LwRm *pLwRm, string rmMsgString);
        RC Restore();

    private:
        // Non-copyable
        RmMsgFilter(const RmMsgFilter&);
        RmMsgFilter& operator=(const RmMsgFilter&);

        static volatile INT32  s_ChangeCount;
        static string s_OrigRmMsg;
        static string s_RmMsgString;
        static LwRm * s_pLwRm;
    };

    class ValueStats
    {
    public:
        void AddPoint(UINT32 value);
        void PrintReport(Tee::Priority priority, const char *name);
        void Reset();
    private:
        map<UINT32, UINT32> m_Oclwrrences;
    };

};
