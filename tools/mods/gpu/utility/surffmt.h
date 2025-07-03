/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2013,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
*/
#ifndef INCLUDED_SURFFMT_H
#define INCLUDED_SURFFMT_H

#include <vector>
#include "core/include/types.h"
#include "core/include/rc.h"

class Surface2D;
typedef Surface2D Surface;
class BlockLinear;
class GpuDevice;

// SurfaceFormat classes is designed to provide some fundamental utility
// functions for all kinds of surfaces including RT/textures/other buffers
class SurfaceFormat
{
public:
    virtual ~SurfaceFormat() {}

    // Callwlate whole surface size, mip level/array size/lwbemap are considered
    virtual UINT64  GetSize() = 0;
    // Do layout from a naive pitch buffer
    virtual UINT08* DoLayout(UINT08 *in, UINT32 size) = 0;
    // Colwerte a buffer with layout to be naive pitch
    virtual UINT08* UnDoLayout() = 0;
    // Callwlate byte offset for a pixel with logical dimension info
    virtual RC Offset(UINT64 *offset, UINT32 x, UINT32 y, UINT32 z,
                      UINT32 dim = 0, UINT32 level = 0, UINT32 layer = 0) = 0;
    // Check validation for parameters
    virtual RC SanityCheck() = 0;

    // Callwlate buffer size to hold the surface in naive pitch
    UINT32 GetUnLayoutSize();

    const UINT08* GetLayoutedBuf()   const { return m_BufLayouted;   }
    const UINT08* GetUnLayoutedBuf() const { return m_BufUnLayouted; }
    void ReleaseBuffer();
    UINT08* ReleaseOwnership();

    UINT32 GetScaledWidth();
    UINT32 GetScaledHeight();

    // Factory function
    static SurfaceFormat* CreateFormat(Surface *surf, GpuDevice *gpuDev);
    static SurfaceFormat* CreateFormat(Surface *surf, GpuDevice *gpuDev,
                                       const UINT08* layoutedBuf, UINT32 size);

protected:
    SurfaceFormat(Surface *surf, GpuDevice *gpuDev)
        : m_Surface(surf), m_GpuDevice(gpuDev), m_Size(0),
          m_BufLayouted(0), m_BufUnLayouted(0), m_UnLayoutSize(0) {}
    UINT32 GetWidthScale();     // Only for texture for now
    UINT32 GetHeightScale();    // Only for texture for now
    UINT32 GetTexelScale();     // Only for texture for now
    virtual RC SetLayoutedBuf(const UINT08* layoutedBuf, UINT32 size);
    UINT32 GetMipWidth(UINT32 baseWidth, UINT32 level) const;
    UINT32 GetMipHeight(UINT32 baseHeight, UINT32 level) const;
    UINT32 GetMipDepth(UINT32 baseDepth, UINT32 level) const;

    static const UINT32 m_Border_w = 1;
    static const UINT32 m_Border_h = 1;
    static const UINT32 m_Border_d = 1;

    // From constructor
    const Surface *m_Surface;
    GpuDevice *m_GpuDevice;

    UINT64 m_Size;
    UINT08* m_BufLayouted;
    UINT08* m_BufUnLayouted;

private:
    // Prevent direct access for derived classes to force callwlating
    UINT32 m_UnLayoutSize;
};

class SurfaceFormatPitch : public SurfaceFormat
{
public:
    SurfaceFormatPitch(Surface *surf, GpuDevice *gpuDev);
    ~SurfaceFormatPitch();

    UINT64  GetSize();
    UINT08* DoLayout(UINT08 *in, UINT32 size);
    UINT08* UnDoLayout();
    RC Offset(UINT64 *offset, UINT32 x, UINT32 y, UINT32 z,
              UINT32 level = 0, UINT32 dim = 0, UINT32 layer = 0);
    RC SanityCheck();

private:
    // Some resource types need the pitch size aligned
    UINT32 m_PitchAlignment;
};

class SurfaceFormatBL : public SurfaceFormat
{
public:
    SurfaceFormatBL(Surface *surf, GpuDevice *gpuDev);
    ~SurfaceFormatBL();

    UINT64  GetSize();
    UINT08* DoLayout(UINT08 *in, UINT32 size);
    UINT08* UnDoLayout();
    RC Offset(UINT64 *offset, UINT32 x, UINT32 y, UINT32 z,
              UINT32 level = 0, UINT32 dim = 0, UINT32 layer = 0);
    RC SanityCheck();

private:
    UINT32 SparseTileAlignment();
    void AlignSparseMipLevel(UINT32* width, UINT32* height, UINT32* depth);
    UINT64 AlignSparseArrayElement(UINT64 oldSize);

    bool m_Shrunk; // Only textures need shrunk to right block
    vector<BlockLinear*> m_BL;
    UINT64 m_BufLevelSize;
};

#endif // INCLUDED_SURFFMT_H
