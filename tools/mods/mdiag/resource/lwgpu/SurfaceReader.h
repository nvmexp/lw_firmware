/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2016, 2018, 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SURFACE_READER_H_
#define _SURFACE_READER_H_

#include "mdiag/utils/types.h"

#include "surfasm.h"
#include "gpu/utility/surf2d.h"
#include "mdiag/utils/IGpuVerif.h"

class LWGpuResource;
class IGpuSurfaceMgr;
class DmaReader;
class ArgReader;
class TraceChannel;
class LWGpuResource;
class MdiagSurf;
class SmcEngine;

// For each MiagSurf we need CRC-checked, we need to keep track of     which region
// we are interested in and what word size that region uses.
class CheckDmaBuffer
{
public:
    CheckDmaBuffer();
    MdiagSurf* pDmaBuf;
    string Filename;
    UINT32 Offset;
    UINT32 Granularity;
    bool   CrcMatch;
    UINT32 Width;
    UINT32 Height;
    CHECK_METHOD Check;

    const VP2TilingParams *pTilingParams;
    const BlockLinear     *pBlockLinear;

    UINT32 CrcChainClass;

    void   SetBufferSize(UINT64 size);
    UINT64 GetBufferSize() const;
    void   SetUntiledSize(UINT64 size);
    UINT64 GetUntiledSize() const;
private:
    UINT64 BufferSize;
    UINT64 UntiledSize;
};

class SurfaceReader
{
public:
    SurfaceReader(LwRm* pLwRm, SmcEngine* pSmcEngine, LWGpuResource *inLwgpu, 
        IGpuSurfaceMgr* surfMgr, const ArgReader* params,
        UINT32 trcArch, TraceChannel* channel, bool dmaCheck);

    virtual ~SurfaceReader();

    // GpuVerif will get its m_DmaReader from here.
    DmaReader* GetDmaReader() { return m_DmaReader; }

    DmaReader* GetSurfaceCopier() { return m_SurfaceCopier; }

    bool SetupDmaBuffer( UINT32 sz, UINT32 gpuSubdevice );
    bool SetupSurfaceCopier( UINT32 gpuSubdevice );

    // A vector swap will fill pOutSurface.
    RC ReadSurface(vector<UINT08>* pOutSurface,
                   MdiagSurf *pSurface,
                   UINT32 gpuSubdevice,
                   bool rawCrc,
                   const SurfaceAssembler::Rectangle &rect);

    // Output is ppSurface.
    // If chunkSize is non-zero, chunkSize is the number of bytes
    // to read at one time.
    RC ReadDmaBuffer(UINT08 **ppSurface,
        CheckDmaBuffer *pCheck,
        UINT32 gpuSubdevice,
        UINT32 chunkSize);

    RC CopySurface(MdiagSurf *surf, UINT32 gpusubdevice, UINT32 Offset = 0);

    UINT32 GetCopyHandle() const;
    UINT64 GetCopyBuffer() const;

private:

    bool IsVP2TilingActive   (const VP2TilingParams *pTilingParams);
    RC DoVP2UntilingLayout           (UINT08 **ppSurface, CheckDmaBuffer *pCheck);
    RC DoVP2UntilingLayoutPitchLinear(UINT08 **ppSurface, CheckDmaBuffer *pCheck);
    RC DoVP2UntilingLayout16x16      (UINT08 **ppSurface, CheckDmaBuffer *pCheck);
    RC DoVP2UntilingLayoutBlockLinear(UINT08 **ppSurface, CheckDmaBuffer *pCheck);
    RC DoVP2UntilingLayoutBlockLinearTexture(UINT08 **ppSurface, CheckDmaBuffer *pCheck);
    RC DoVP2UntilingLayoutBlockLinear(UINT08 **ppSurface, CheckDmaBuffer *pCheck, BlockLinear *pBL);

    RC DumpRawMemory(MdiagSurf *surface, UINT64 offset, UINT32 size);

    LwRm*               m_pLwRm;
    SmcEngine*          m_pSmcEngine;
    DmaReader*          m_DmaReader;
    LWGpuResource*      m_lwgpu;
    IGpuSurfaceMgr*     m_surfMgr;
    const ArgReader*    m_Params;
    TraceChannel*       m_Channel;
    DmaReader*          m_SurfaceCopier;
    bool                m_DmaCheck;
    UINT32              m_class;
    bool                m_ReadRawImage;

    bool NeedCeVprTrusted(const MdiagSurf *surf, UINT32 gpuSubdevice);
    bool NeedCeCcTrusted(const MdiagSurf *surf, UINT32 gpuSubdevice);
    void SetCeVprTrusted(UINT32 gpuSubdevice);
    void SetCeCcTrusted(UINT32 gpuSubdevice);
    void RestoreCeVprTrusted(UINT32 gpuSubdevice);
    void RestoreCeCcTrusted(UINT32 gpuSubdevice);

    map<UINT32, bool> m_MarkedCeVprTrusted; //   <gpuSubdev, CETrusted>
    map<UINT32, bool> m_MarkedCeCcTrusted; //   <gpuSubdev, CETrusted>
    map<UINT32, UINT32> m_SavedVprRegInfo; // <gpuSubdev, SavedVprRegValue>
    map<UINT32, UINT32> m_SavedCcRegInfo; // <gpuSubdev, SavedCcRegValue>
};

#endif
