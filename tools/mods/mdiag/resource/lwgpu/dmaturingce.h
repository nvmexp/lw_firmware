/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _DMA_READER_TURINGCE_H_
#define _DMA_READER_TURINGCE_H_

#include "dmagf100ce.h"

/// Reads from surfaces using Turing copy engine
class DmaReaderTuringCE : public DmaReaderGF100CE
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
    DmaReaderTuringCE(WfiType wfiType, LWGpuChannel* ch, UINT32 size);

public:
    /// d'tor
    virtual ~DmaReaderTuringCE();

    virtual bool NoPitchCopyOfBlocklinear() const { return true; }

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

private:
    // Prevent copying
    DmaReaderTuringCE(const DmaReaderTuringCE&);
    DmaReaderTuringCE& operator=(const DmaReaderTuringCE&);
};

#endif
