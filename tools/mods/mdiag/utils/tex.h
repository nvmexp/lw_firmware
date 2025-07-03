/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef TEX_H
#define TEX_H

#include <vector>
#include "types.h"
#include "gpu/utility/blocklin.h"
#include "surf_types.h"
#include "core/include/color.h"

class GpuDevice;
class GpuSubdevice;
class TraceModule;

////////////////////////////////////////////////////////////////////////////////
//
// TextureHeaderState is a struct for storing the information from the texture
// headers of a trace that uses textures.  A texture trace has one texture
// header file, but that file can contain multiple texture headers.  Each of
// those texture headers will be stored in a TextureHeaderState struct.
//
struct TextureHeaderState
{
    UINT32 DWORD0;
    UINT32 DWORD1;
    UINT32 DWORD2;
    UINT32 DWORD3;
    UINT32 DWORD4;
    UINT32 DWORD5;
    UINT32 DWORD6;
    UINT32 DWORD7;

    TextureHeaderState() : DWORD0(0), DWORD1(0), DWORD2(0), DWORD3(0), DWORD4(0), DWORD5(0), DWORD6(0), DWORD7(0) {}
    TextureHeaderState(UINT32 d0, UINT32 d1, UINT32 d2, UINT32 d3, UINT32 d4, UINT32 d5, UINT32 d6, UINT32 d7) :
        DWORD0(d0), DWORD1(d1), DWORD2(d2), DWORD3(d3), DWORD4(d4), DWORD5(d5), DWORD6(d6), DWORD7(d7) {}
    TextureHeaderState(UINT32* pData)
    {
        DWORD0 = pData[0]; DWORD1 = pData[1]; DWORD2 = pData[2]; DWORD3 = pData[3];
        DWORD4 = pData[4]; DWORD5 = pData[5]; DWORD6 = pData[6]; DWORD7 = pData[7];
    }
};

////////////////////////////////////////////////////////////////////////////////
//
// TextureHeader is a class for processing texture information from a
// TextureHeaderState object.
//
// The TextureHeader class itself is an abstract base class as there are
// different texture headers formats (e.g. Kepler and Maxwell).  There is a
// derived class for each texture header format.
//
class TextureHeader
{
public:
    enum class HeaderFormat
    {
        Unknown,
        Kepler,
        Maxwell,
        Hopper
    };

    static TextureHeader* CreateTextureHeader
    (
        const TextureHeaderState* state,
        GpuSubdevice* pGpuSubdev,
        HeaderFormat headerFormat
    );

    virtual ~TextureHeader();
    UINT08* DoLayout(UINT08* in, UINT32 size) const;
    bool SanityCheck() const;
    UINT32 Size() const;
    virtual UINT32 GetHeapAttr() const = 0;
    virtual UINT32 GetHeapType() const = 0;
    virtual bool Compressible() const = 0;
    UINT32 TexWidth() const { return m_layout->Width(); }
    UINT32 TexHeight() const { return m_layout->Height(); }
    UINT32 TexDepth() const { return m_layout->Depth(); }
    bool   PixelOffset(UINT32 array_index, UINT32 lwbemap_face, UINT32 miplevel,
                       UINT32 x, UINT32 y, UINT32 z, UINT32* offset) const;
    virtual UINT32 BlockWidth() const = 0;
    virtual UINT32 BlockHeight() const = 0;
    virtual UINT32 BlockDepth() const = 0;
    virtual bool IsBlocklinear() const = 0;
    virtual void FixBlockSize() const = 0;
    virtual ColorUtils::Format GetColorFormat() const = 0;

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
    ) = 0;

    enum L1_PROMOTION
    {
        L1_PROMOTION_NONE = 0,
        L1_PROMOTION_2V,
        L1_PROMOTION_2H,
        L1_PROMOTION_ALL
    };

    L1_PROMOTION GetOptimalSectorPromotion() const;

    static RC ModifySectorPromotion
    (
        UINT32* state,
        L1_PROMOTION newPromotion,
        HeaderFormat headerFormat
    );

protected:
    TextureHeader(GpuSubdevice* pGpuSubdev);

    void CreateLayout();
    virtual bool VCAADepthBuffer() const = 0;
    virtual bool AllSourceInB() const = 0;
    virtual UINT32 TexelSize() const = 0;
    virtual UINT32 TexelScale() const = 0;
    virtual UINT32 TextureWidth() const = 0;
    virtual UINT32 TextureWidthScale() const = 0;
    virtual UINT32 TextureHeight() const = 0;
    virtual UINT32 TextureHeightScale() const = 0;
    virtual UINT32 TextureDepth() const = 0;
    virtual UINT32 ArraySize() const = 0;
    virtual UINT32 BorderWidth() const = 0;
    virtual UINT32 BorderHeight() const = 0;
    virtual UINT32 BorderDepth() const = 0;
    virtual UINT32 DimSize() const = 0;
    virtual UINT32 TexturePitch() const = 0;
    virtual UINT32 MaxMipLevel() const = 0;
    void BlockShrinkingSanityCheck(MdiagSurf* surface, GpuDevice* gpuDevice) const;

    inline UINT32 MinimumMipWidth()         const;
    inline UINT32 MinimumMipHeight()        const;

    class TextureParams;

    TextureParams* m_layout;
    GpuSubdevice  *m_pGpuSubdevice;

    class TextureParams
    {
        public:
            TextureParams(UINT32 borderWidth, UINT32 borderHeight, UINT32 borderDepth)
                : m_BorderWidth(borderWidth), m_BorderHeight(borderHeight), m_BorderDepth(borderDepth) {}

            //returns pointer to an allocated buffer and it's size
            virtual UINT08* DoLayout(UINT08* in, UINT32 size) const = 0;
            virtual bool SanityCheck() const = 0;
            virtual UINT32 Size() const = 0;
            virtual UINT32 Width() const = 0;
            virtual UINT32 Height() const = 0;
            virtual UINT32 Depth() const = 0;
            virtual bool   PixelOffset(UINT32 array_index, UINT32 lwbemap_face, UINT32 miplevel,
                                       UINT32 x, UINT32 y, UINT32 z, UINT32* offset) const = 0;
            virtual UINT32 GetHeapAttr() const {return 0;}
            virtual UINT32 GetHeapType() const {return 0;}

            virtual ~TextureParams() {}
        protected:
            UINT32 m_BorderWidth;
            UINT32 m_BorderHeight;
            UINT32 m_BorderDepth;
    };

    class TextureParamsBlockLinear : public TextureParams
    {
        public:
            TextureParamsBlockLinear(UINT32 bw, UINT32 bh, UINT32 bd,
                    UINT32 tw, UINT32 th, UINT32 td,
                    UINT32 ws, UINT32 hs,
                    UINT32 ml,
                    UINT32 minimumMipWidth, UINT32 minimumMipHeight,
                    UINT32 tsize, UINT32 tscale,
                    UINT32 ds, UINT32 as,
                    UINT32 borderWidth, UINT32 borderHeight, UINT32 borderDepth,
                    GpuDevice *pGpuDev);
            ~TextureParamsBlockLinear();

            UINT08* DoLayout(UINT08* in, UINT32 size) const;
            bool SanityCheck() const;
            UINT32 Size() const {return m_miplevel_size * dim_size * array_size;}
            UINT32 Width() const {return texture_width;}
            UINT32 Height() const {return texture_height;}
            UINT32 Depth() const {return texture_depth;}
            bool   PixelOffset(UINT32 array_index, UINT32 lwbemap_face, UINT32 miplevel,
                               UINT32 x, UINT32 y, UINT32 z, UINT32* offset) const;
            UINT32 GetMipWidth(UINT32 mipLevel) const;
            UINT32 GetMipHeight(UINT32 mipLevel) const;
            UINT32 GetMipDepth(UINT32 mipLevel) const;

        private:
            UINT32 block_width;
            UINT32 block_height;
            UINT32 block_depth;
            UINT32 texture_width;
            UINT32 texture_height;
            UINT32 texture_depth;
            UINT32 width_scale;
            UINT32 height_scale;
            UINT32 mip_levels;
            UINT32 m_MinimumMipWidth;
            UINT32 m_MinimumMipHeight;
            UINT32 texel_size;
            UINT32 texel_scale;
            UINT32 dim_size;
            UINT32 array_size;

            vector<BlockLinear*> m_bl;
            UINT32 m_miplevel_size;
            GpuDevice *m_pGpuDevice;
    };

    class TextureParamsPitchLinear : public TextureParams
    {
        public:
            TextureParamsPitchLinear
            (
                UINT32 tw,
                UINT32 th,
                UINT32 widthScale,
                UINT32 heightScale,
                UINT32 p,
                UINT32 ts,
                UINT32 tsc,
                UINT32 borderWidth,
                UINT32 borderHeight,
                UINT32 borderDepth
            );

            UINT08* DoLayout(UINT08* in, UINT32 size) const;
            bool SanityCheck() const;
            UINT32 Size() const;
            UINT32 Width() const {return texture_width;}
            UINT32 Height() const {return texture_height;}
            UINT32 Depth() const {return 1;}
            bool   PixelOffset(UINT32 array_index, UINT32 lwbemap_face, UINT32 miplevel,
                               UINT32 x, UINT32 y, UINT32 z, UINT32* offset) const;
        UINT32 ScaledWidth() const;
        UINT32 ScaledHeight() const;
        UINT32 ScaledPitch() const;

        private:
            static const UINT32 pitch_alignment = 0x20;

            UINT32 texture_width;
            UINT32 texture_height;
            UINT32 m_WidthScale;
            UINT32 m_HeightScale;
            UINT32 texture_pitch;
            UINT32 texel_size;
            UINT32 texel_scale;
    };

};

#endif /* TEX_H */
