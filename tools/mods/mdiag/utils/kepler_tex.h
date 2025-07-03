/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef KEPLER_TEX_H
#define KEPLER_TEX_H

#include "tex.h"

class GpuSubdevice;

////////////////////////////////////////////////////////////////////////////////
//
// KeplerTextureHeader is a class for processing texture information from a
// TextureHeaderState object.  The data in the TextureHeaderState object must
// be in the texture header format used by the Kepler graphics class.
// (KeplerTextureHeader also supports graphics classes prior to Kepler.)
//
class KeplerTextureHeader : public TextureHeader
{
friend class TextureHeader;

public:
    virtual ~KeplerTextureHeader();

    virtual UINT32 GetHeapAttr() const;
    virtual UINT32 GetHeapType() const;
    virtual bool IsBlocklinear() const;
    virtual void FixBlockSize() const;
    virtual bool Compressible() const;
    virtual UINT32 BlockWidth() const;
    virtual UINT32 BlockHeight() const;
    virtual UINT32 BlockDepth() const;
    virtual ColorUtils::Format GetColorFormat() const;

    virtual RC RelocTexture
    (
        MdiagSurf *surface,
        UINT64 offset,
        LW50Raster* raster,
        TextureMode textureMode,
        GpuDevice* gpuDevice,
        bool useOffset,
        bool useSurfaceAttr,
        bool optimalPromotion,
        bool centerSpoof,
        TextureHeaderState *newState
    );

    static RC ModifySectorPromotion(TextureHeaderState* state, L1_PROMOTION newPromotion);

protected:
    KeplerTextureHeader(const TextureHeaderState* state, GpuSubdevice* gpuSubdevice);

    virtual bool VCAADepthBuffer() const;
    virtual bool AllSourceInB() const;
    virtual UINT32 TexelSize() const;
    virtual UINT32 TexelScale() const;
    virtual UINT32 TextureWidth() const;
    virtual UINT32 TextureWidthScale() const;
    virtual UINT32 TextureHeight() const;
    virtual UINT32 TextureHeightScale() const;
    virtual UINT32 TextureDepth() const;
    virtual UINT32 ArraySize() const;
    virtual UINT32 BorderWidth() const;
    virtual UINT32 BorderHeight() const;
    virtual UINT32 BorderDepth() const;
    virtual UINT32 DimSize() const;
    virtual UINT32 TexturePitch() const;
    virtual UINT32 MaxMipLevel() const;

    TextureHeaderState* m_state;
};

#endif
