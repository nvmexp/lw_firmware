/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SCANLINE_READER_H_
#define _SCANLINE_READER_H_

#include "mdiag/utils/types.h"

class IGpuSurfaceMgr;
class Surface2D;
class MdiagSurf;
class RC;

namespace SurfaceUtils {
    class ISurfaceReader;
}

//! Reads subsequent scanlines from a surface using SurfaceReader.
class ScanlineReader
{
public:
    virtual ~ScanlineReader() { }
    //! Restarts reading the surface from another subdevice.
    void Restart(UINT32 subdev);
    //! Reads part of current scanline and advances to the next scanline.
    //!
    //! Reads the chosen range of pixels from the current scanline into
    //! the buffer and then advances to next scanline.
    //! \param pBuf Buffer into which the piece of scanline is read.
    //!             The buffer must be large enough to hold width*depth
    //!             bytes, where 'depth' is the number of bytes per pixel.
    //! \param x Index of first pixel in the scanline to read.
    //! \param width Number of pixels to read.
    virtual RC ReadScanline(UINT08* pBuf, UINT32 x, UINT32 width) = 0;
    //! Advances a couple of scanlines forward by skipping scanlines.
    void SkipScanlines(UINT32 numScanlines) { m_ScanlineIndex += numScanlines; }
    //! Returns surface being read for obtaining surface properties.
    const Surface2D& GetSurface() const; // Not MdiagSurf because m_SurfaceReader returns Surface2D
    void SetOrigSurface(MdiagSurf* pOrigSurface) { m_pOrigSurface = pOrigSurface; }
    MdiagSurf* GetOrigSurface() { return m_pOrigSurface; }
    //! Returns required surface width.
    UINT32 GetSurfaceWidth() const;
    //! Returns required surface height.
    UINT32 GetSurfaceHeight() const;
    //! Returns window's X position.
    UINT32 GetWindowX() const;
    //! Returns window's Y position.
    UINT32 GetWindowY() const;
    //! Callwlates position from global scanline index.
    void CalcPosition(UINT32 scanline, UINT32* iSubSurf, UINT32* iLayer, UINT32* y);
    //! Returns granularity of the surface for byte swapping
    UINT32 GetGranularity() const;

protected:
    ScanlineReader(SurfaceUtils::ISurfaceReader& surfReader, IGpuSurfaceMgr& surfManager)
        : m_SurfaceReader(surfReader), m_SurfaceManager(surfManager), m_Subdev(0) { }
    //! Returns surface reader. For derived classes.
    SurfaceUtils::ISurfaceReader& GetReader() const { return m_SurfaceReader; }
    //! Returns surface manager. For derived classes.
    IGpuSurfaceMgr& GetSurfMgr() const { return m_SurfaceManager; }
    //! Returns lwrrently used subdevice.
    UINT32 GetLwrrentSubdev() const { return m_Subdev; }

    UINT32 m_ScanlineIndex;         //!< Index of current scanline.

private:
    //! Notifies the scanline reader that new surface reading has restarted.
    virtual void RestartImpl() = 0;

    // Prevent copying
    ScanlineReader(const ScanlineReader&);
    ScanlineReader& operator=(const ScanlineReader&);

    SurfaceUtils::ISurfaceReader& m_SurfaceReader;
    IGpuSurfaceMgr& m_SurfaceManager;
    UINT32 m_Subdev;

    MdiagSurf* m_pOrigSurface;
};

#endif // _SCANLINE_READER_H_
