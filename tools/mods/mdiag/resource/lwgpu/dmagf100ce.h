/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2016, 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _DMA_READER_GF100CE_H_
#define _DMA_READER_GF100CE_H_

#include "dmard.h"

/// Reads from surfaces using GF100 copy engine
class DmaReaderGF100CE : public DmaReader
{
    friend DmaReader* DmaReader::CreateDmaReader
    (
        EngineType engineType,
        WfiType wfiType,
        LWGpuResource* lwgpu,
        LWGpuChannel* channel,
        UINT32 size,
        const ArgReader* params,
        LwRm *pLwRm,
        SmcEngine *pSmcEngine,
        UINT32 subdev
    );

protected:
    /// c'tor
    DmaReaderGF100CE(WfiType wfiType, LWGpuChannel* ch, UINT32 size);

public:
    /// d'tor
    virtual ~DmaReaderGF100CE();

    //! gets the dma reader class id
    virtual UINT32 GetClassId() const;

    // dma line from vidmem to sysmem
    virtual RC ReadLine
    (
        UINT32 hSrcCtxDma, UINT64 SrcOffset, UINT32 SrcPitch,
        UINT32 LineCount, UINT32 LineLength,
        UINT32 gpuSubdevice, const Surface2D* srcSurf,
        MdiagSurf* dstSurf = 0, UINT64 DstOffset = 0
    );

    // copy surf to m_CopyLocation
    virtual RC CopySurface
    (
        MdiagSurf* surf,
        _PAGE_LAYOUT Layout,
        UINT32 gpuSubdevice,
        UINT32 Offset = 0
    );

    virtual RC SendBasicKindMethod(const Surface2D* srcSurf, MdiagSurf* dstSurf) { return OK; }

private:
    // Prevent copying
    DmaReaderGF100CE(const DmaReaderGF100CE&);
    DmaReaderGF100CE& operator=(const DmaReaderGF100CE&);
};

#endif
