/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016, 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _DMA_READER_VOLTACE_H_
#define _DMA_READER_VOLTACE_H_

#include "dmagf100ce.h"

/// Reads from surfaces using GF100 copy engine
class DmaReaderVoltaCE : public DmaReaderGF100CE
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
    DmaReaderVoltaCE(WfiType wfiType, LWGpuChannel* ch, UINT32 size);

public:
    /// d'tor
    virtual ~DmaReaderVoltaCE();

    virtual RC SendBasicKindMethod(const Surface2D* srcSurf, MdiagSurf* dstSurf);

private:
    // Prevent copying
    DmaReaderVoltaCE(const DmaReaderVoltaCE&);
    DmaReaderVoltaCE& operator=(const DmaReaderVoltaCE&);
};

#endif
