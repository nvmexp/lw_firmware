/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  dmasurfr.h
 * @brief Surface reader which takes advantage of DMA transfers.
 */

#ifndef INCLUDED_DMASURFR_H
#define INCLUDED_DMASURFR_H

#include "gpu/utility/surfrdwr.h"

class DmaReader;
class BlockLinear;

namespace SurfaceUtils {

//! Surface reader which uses DMA transfers to read the surface.
class DmaSurfaceReader: public ISurfaceReader {
public:
    //! @param Surf %Surface to read.
    //! @param DmaMgr DMA transfer manager.
    DmaSurfaceReader(const Surface2D& Surf, DmaReader& DmaMgr);
    virtual ~DmaSurfaceReader();
    virtual void SetTargetSubdevice(UINT32 Subdev);
    virtual void SetTargetSubdeviceForce(UINT32 Subdev);
    virtual RC ReadBytes(UINT64 Offset, void* Buf, size_t Size);
    virtual RC ReadBlocklinearLines
    (
        const BlockLinear& blAddressMapper,
        UINT32 y,
        UINT32 z,
        UINT32 arrayIndex,
        void* destBuffer,
        UINT32 lineCount
    );
    virtual const Surface2D& GetSurface() const;

private:
    const Surface2D& m_Surf;
    DmaReader& m_DmaMgr;
    UINT32 m_Subdev;
};

} // namespace SurfaceUtils

#endif // INCLUDED_DMASURFR_H
