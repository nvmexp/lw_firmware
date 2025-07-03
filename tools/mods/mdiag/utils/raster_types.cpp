/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017, 2020-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <string>
#include "surf_types.h"
#include "gpu/utility/blocklin.h"
#include "lwos.h"
#include "lwmisc.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "mdiag/sysspec.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"

#include "class/cl9097.h"
#include "class/cl9097tex.h"
#include "class/clb097tex.h"
#include "class/clcb97tex.h"
#include "class/clb197.h"

enum ComponentType
{
    Float,
    Uint,
    Sint,
    Unorm,
    Snorm,
};

inline float abs_diff(float a, float b)
{
    if (*(UINT32 *)(&a) == *(UINT32 *)(&b)) //in case NaNs are compared
        return 0.;
    return a > b ? a - b : b - a;
}

inline UINT32 abs_diff(UINT32 a, UINT32 b)
{
    return a > b ? a - b : b - a;
}

inline UINT32 abs_diff_snorm(UINT32 a, UINT32 b, int ibits)
{
    UINT32 val = 1 << (ibits-1);

    if (!(a^b))
        return 0;            // For most cases, quick return
    else if (!(a^val) || !(b^val))
        return (1<<ibits)-1; // Make a rule, any other numbers compare to the
                             // most negtive number, return an abnormal value.
                             // e.g. for snorm8, 0x80 an 0x81 are both -1.0,
                             // but 0x80 is abnormal, which only matches 0x80
                             // otherwise return 0xff. In normal cases, the
                             // diff range is [0, 0xfe]. Bug 393259.
    // Shift to [1, 1<<ibits -1]
    if (a < val) a += val;
    else         a -= val;

    if (b < val) b += val;
    else         b -= val;

    return abs_diff(a,b);
}
//------------------------------------------------------------------------------
UINT32 GetTextureHeaderDataTypes(ComponentType Type)
{
    switch (Type)
    {
        case Float:
            return
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_FLOAT) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_FLOAT) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_FLOAT) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_FLOAT);
        case Uint:
            return
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UINT) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        case Sint:
            return
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_SINT) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_SINT) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_SINT) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_SINT);
        case Unorm:
            return
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM);
        case Snorm:
            return
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_SNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_SNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_SNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_SNORM);
        default:
            MASSERT(!"Invalid component type");
            return 0;
    }
}

void SetMaxwellTextureHeaderDataTypes(UINT32* state, ComponentType Type)
{
    switch (Type)
    {
        case Float:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_FLOAT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_FLOAT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_FLOAT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_FLOAT, state);
            break;
        case Uint:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UINT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
            break;
        case Sint:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_SINT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_SINT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_SINT, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_SINT, state);
            break;
        case Unorm:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
            break;
        case Snorm:
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_SNORM, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_SNORM, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_SNORM, state);
            FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_SNORM, state);
            break;
        default:
            MASSERT(!"Invalid component type");
    }
}

void SetHopperTextureHeaderDataTypes(UINT32* state, ComponentType Type)
{
    switch (Type)
    {
        case Float:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_FLOAT, state);
            break;
        case Uint:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UINT, state);
            break;
        case Sint:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SINT, state);
            break;
        case Unorm:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
            break;
        case Snorm:
            FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SNORM, state);
            break;
        default:
            MASSERT(!"Invalid component type");
    }
}

//------------------------------------------------------------------------------
LW50Raster::LW50Raster(ColorUtils::Format surfFormat, UINT32 devFormat, unsigned bpp, unsigned tgaBpp) :
    m_surfFormat(surfFormat), m_devFormat(devFormat), m_bpp(bpp), m_tgaBpp(tgaBpp)
{
}

LW50Raster::~LW50Raster()
{
}

unsigned LW50Raster::ImageOffset2GobOffset(unsigned offset, unsigned image_width_texels,
                               unsigned log2_gob_width_texel, unsigned log2_gob_height_texel) const
{
    XYZ lwr = unlinearmap(offset/m_bpp, image_width_texels);
    return (((lwr.y & Mask(log2_gob_height_texel)) << log2_gob_width_texel)+
                                    (lwr.x & Mask(log2_gob_width_texel)));
}

void LW50Raster::ToTgaExt(unsigned char *PixelSize, unsigned char *Desc) const
{
    *PixelSize = 8*m_tgaBpp;
    *Desc = 8;
}

void LW50Raster::ColwertToTga(UINT08 *to, UINT08 *from, UINT32 size) const
{
    memcpy(to, from, size * m_tgaBpp);
}

//------------------------------------------------------------------------------
class LW50CRaster: public LW50Raster
{
public:
    LW50CRaster(ColorUtils::Format s, UINT32 f, unsigned int m, unsigned int t) :
        LW50Raster(s, f, m, t) {}

    virtual bool ZFormat() const { return false; }

    virtual bool IsBlack(UINT32 n) const
    {
        PixelFB pixel = GetPixel(n);
        return pixel.Red()   == 0 &&
               pixel.Green() == 0 &&
               pixel.Blue()  == 0 &&
               pixel.Alpha() == 0;
    }
};

//------------------------------------------------------------------------------
class LW50ZRaster: public LW50Raster
{
private:
    bool zfloat;
    UINT32 zbits, sbits;
public:
    LW50ZRaster(ColorUtils::Format s, UINT32 f, unsigned int m, unsigned int t, bool zfloat, UINT32 zbits, UINT32 sbits) :
        LW50Raster(s, f, m, t), zfloat(zfloat), zbits(zbits), sbits(sbits) {}

    virtual bool ZFormat() const { return true; }

    // these functions are not supposed to get called for Z formats
    virtual bool IsBlack(UINT32 n) const
    {
        MASSERT(!"not implemented");
        return false;
    }

    virtual bool GetZFloat() const { return zfloat; }
    virtual UINT32 GetZBits() const { return zbits; }
    virtual UINT32 GetStencilBits() const { return sbits; }

    virtual bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        MASSERT(!"not implemented");
        return false;
    }

    virtual PixelFB GetPixel(UINT32 n) const
    {
        MASSERT(!"not implemented yet");
        return PixelFB(0,0,0,0);
    }
};

//------------------------------------------------------------------------------
class A8R8G8B8: public LW50CRaster
{
public:
    A8R8G8B8(): LW50CRaster(
        ColorUtils::A8R8G8B8, LW9097_SET_COLOR_TARGET_FORMAT_V_A8R8G8B8, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetAUNorm(8) << 24;
        if (SrgbWrite)
        {
            c |= color.GetBsRGB8();
            c |= color.GetGsRGB8() << 8;
            c |= color.GetRsRGB8() << 16;
        }
        else
        {
            c |= color.GetBUNorm(8);
            c |= color.GetGUNorm(8) << 8;
            c |= color.GetRUNorm(8) << 16;
        }
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool SupportsSrgbWrite() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8) &&
               delta_blue  <= threshold.getUNorm(0, 8) &&
               delta_alpha <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Alpha() , pf.GetAUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff0000) >> 16,
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff),
                       (pixel & 0xff000000) >> 24);
    }
};

//------------------------------------------------------------------------------
class X8R8G8B8: public LW50CRaster
{
public:
    X8R8G8B8(): LW50CRaster(
        ColorUtils::X8R8G8B8, LW9097_SET_COLOR_TARGET_FORMAT_V_X8R8G8B8, 4, 4) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = 0;
        if (SrgbWrite)
        {
            c |= color.GetBsRGB8();
            c |= color.GetGsRGB8() << 8;
            c |= color.GetRsRGB8() << 16;
        }
        else
        {
            c |= color.GetBUNorm(8);
            c |= color.GetGUNorm(8) << 8;
            c |= color.GetRUNorm(8) << 16;
        }
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool SupportsSrgbWrite() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        if (LwRmPtr()->IsClassSupported(FERMI_A, pGpuDevice))
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
        else
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8) &&
               delta_blue  <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff0000) >> 16,
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff),
                       0);
    }
};

//------------------------------------------------------------------------------
class A8B8G8R8: public LW50CRaster
{
public:
    A8B8G8R8(): LW50CRaster(
        ColorUtils::A8B8G8R8, LW9097_SET_COLOR_TARGET_FORMAT_V_A8B8G8R8, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetAUNorm(8) << 24;
        if (SrgbWrite)
        {
            c |= color.GetRsRGB8();
            c |= color.GetGsRGB8() << 8;
            c |= color.GetBsRGB8() << 16;
        }
        else
        {
            c |= color.GetRUNorm(8);
            c |= color.GetGUNorm(8) << 8;
            c |= color.GetBUNorm(8) << 16;
        }
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool SupportsSrgbWrite() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8) &&
               delta_blue  <= threshold.getUNorm(0, 8) &&
               delta_alpha <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Alpha() , pf.GetAUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       (pixel & 0xff000000) >> 24);
    }
};

//------------------------------------------------------------------------------
class AN8BN8GN8RN8: public LW50CRaster
{
public:
    AN8BN8GN8RN8(): LW50CRaster(
        ColorUtils::AN8BN8GN8RN8, LW9097_SET_COLOR_TARGET_FORMAT_V_AN8BN8GN8RN8, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRSNorm(8) |
                  (color.GetGSNorm(8) << 8) |
                  (color.GetBSNorm(8) << 16) |
                  (color.GetASNorm(8) << 24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff_snorm(p1.Red()   , p2.Red(), 8);
        UINT32 delta_green = abs_diff_snorm(p1.Green() , p2.Green(), 8);
        UINT32 delta_blue  = abs_diff_snorm(p1.Blue()  , p2.Blue(), 8);
        UINT32 delta_alpha = abs_diff_snorm(p1.Alpha() , p2.Alpha(), 8);

        return delta_red   <= threshold.getSNorm(0, 8) &&
               delta_green <= threshold.getSNorm(0, 8) &&
               delta_blue  <= threshold.getSNorm(0, 8) &&
               delta_alpha <= threshold.getSNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSNorm(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Green() , pf.GetGSNorm(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Blue()  , pf.GetBSNorm(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Alpha() , pf.GetASNorm(8)) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       (pixel & 0xff000000) >> 24);
    }
};

//------------------------------------------------------------------------------
class AS8BS8GS8RS8: public LW50CRaster
{
public:
    AS8BS8GS8RS8(): LW50CRaster(
        ColorUtils::AS8BS8GS8RS8, LW9097_SET_COLOR_TARGET_FORMAT_V_AS8BS8GS8RS8, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRSInt(8) |
                  (color.GetGSInt(8) << 8) |
                  (color.GetBSInt(8) << 16) |
                  (color.GetASInt(8) << 24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SINT, state);
;
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_green <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_blue  <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_alpha <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSInt(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Green() , pf.GetGSInt(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Blue()  , pf.GetBSInt(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Alpha() , pf.GetASInt(8)) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       (pixel & 0xff000000) >> 24);
    }
};

//------------------------------------------------------------------------------
class AU8BU8GU8RU8: public LW50CRaster
{
public:
    AU8BU8GU8RU8(): LW50CRaster(
        ColorUtils::AU8BU8GU8RU8, LW9097_SET_COLOR_TARGET_FORMAT_V_AU8BU8GU8RU8, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRUInt(8) |
                  (color.GetGUInt(8) << 8) |
                  (color.GetBUInt(8) << 16) |
                  (color.GetAUInt(8) << 24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UINT, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8) &&
               delta_blue  <= threshold.getUNorm(0, 8) &&
               delta_alpha <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);
        return abs_diff(p.Red()   , pf.GetRUInt(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUInt(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Blue()  , pf.GetBUInt(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Alpha() , pf.GetAUInt(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       (pixel & 0xff000000) >> 24);
    }
};

//------------------------------------------------------------------------------
class X8B8G8R8: public LW50CRaster
{
public:
    X8B8G8R8(): LW50CRaster(
        ColorUtils::X8B8G8R8, LW9097_SET_COLOR_TARGET_FORMAT_V_X8B8G8R8, 4, 4) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = 0;
        if (SrgbWrite)
        {
            c |= color.GetRsRGB8();
            c |= color.GetGsRGB8() << 8;
            c |= color.GetBsRGB8() << 16;
        }
        else
        {
            c |= color.GetRUNorm(8);
            c |= color.GetGUNorm(8) << 8;
            c |= color.GetBUNorm(8) << 16;
        }
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool SupportsSrgbWrite() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        if (LwRmPtr()->IsClassSupported(FERMI_A, pGpuDevice))
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
        else
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8) &&
               delta_blue  <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       0);
    }
};

//------------------------------------------------------------------------------
class A8RL8GL8BL8: public LW50CRaster
{
public:
    A8RL8GL8BL8(): LW50CRaster(
        ColorUtils::A8RL8GL8BL8, LW9097_SET_COLOR_TARGET_FORMAT_V_A8RL8GL8BL8, 4, 4) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetBsRGB8() |
                  (color.GetGsRGB8() << 8) |
                  (color.GetRsRGB8() << 16)|
                  (color.GetAUNorm(8) << 24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool IsSrgb() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getsRGB8(0) &&
               delta_green <= threshold.getsRGB8(0) &&
               delta_blue  <= threshold.getsRGB8(0) &&
               delta_alpha <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Green() , pf.GetGsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Blue()  , pf.GetBsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Alpha() , pf.GetAUNorm(8))<= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff0000) >> 16,
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff),
                       (pixel & 0xff000000) >> 24);
    }
};

//------------------------------------------------------------------------------
class X8RL8GL8BL8: public LW50CRaster
{
public:
    X8RL8GL8BL8(): LW50CRaster(
        ColorUtils::X8RL8GL8BL8, LW9097_SET_COLOR_TARGET_FORMAT_V_X8RL8GL8BL8, 4, 4) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetBsRGB8() |
                  (color.GetGsRGB8() << 8) |
                  (color.GetRsRGB8() << 16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool IsSrgb() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        if (LwRmPtr()->IsClassSupported(FERMI_A, pGpuDevice))
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
        else
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= threshold.getsRGB8(0) &&
               delta_green <= threshold.getsRGB8(0) &&
               delta_blue  <= threshold.getsRGB8(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Green() , pf.GetGsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Blue()  , pf.GetBsRGB8()) <= threshold.getsRGB8(0);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff0000) >> 16,
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff),
                       0);
    }
};

//------------------------------------------------------------------------------
class A8BL8GL8RL8: public LW50CRaster
{
public:
    A8BL8GL8RL8(): LW50CRaster(
        ColorUtils::A8BL8GL8RL8, LW9097_SET_COLOR_TARGET_FORMAT_V_A8BL8GL8RL8, 4, 4) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRsRGB8() |
                  (color.GetGsRGB8() << 8) |
                  (color.GetBsRGB8() << 16)|
                  (color.GetAUNorm(8) << 24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool IsSrgb() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getsRGB8(0) &&
               delta_green <= threshold.getsRGB8(0) &&
               delta_blue  <= threshold.getsRGB8(0) &&
               delta_alpha <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Green() , pf.GetGsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Blue()  , pf.GetBsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Alpha() , pf.GetAUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       (pixel & 0xff000000) >> 24);
    }
};

//------------------------------------------------------------------------------
class X8BL8GL8RL8: public LW50CRaster
{
public:
    X8BL8GL8RL8(): LW50CRaster(
        ColorUtils::X8BL8GL8RL8, LW9097_SET_COLOR_TARGET_FORMAT_V_X8BL8GL8RL8, 4, 4) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRsRGB8() |
                  (color.GetGsRGB8() << 8) |
                  (color.GetBsRGB8() << 16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool IsSrgb() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        if (LwRmPtr()->IsClassSupported(FERMI_A, pGpuDevice))
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
        else
        {
            return
                DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
                DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
                DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
        }
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_X8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= threshold.getsRGB8(0) &&
               delta_green <= threshold.getsRGB8(0) &&
               delta_blue  <= threshold.getsRGB8(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Green() , pf.GetGsRGB8()) <= threshold.getsRGB8(0) &&
               abs_diff(p.Blue()  , pf.GetBsRGB8()) <= threshold.getsRGB8(0);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       0);
    }
};

//------------------------------------------------------------------------------
class R5G6B5: public LW50CRaster
{
public:
    R5G6B5(): LW50CRaster(
        ColorUtils::R5G6B5, LW9097_SET_COLOR_TARGET_FORMAT_V_R5G6B5, 2, 2) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetBUNorm(5) | (color.GetGUNorm(6) << 5) | (color.GetRUNorm(5) << 11);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _B5G6R5) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_B5G6R5, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_B5G6R5, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_green <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_blue  <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Green() , pf.GetGUNorm(6)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT16 pixel = *(UINT16 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xf800) >> 11,
                       (pixel & 0x7e0) >> 5,
                       (pixel & 0x1f),
                       0);
    }
};

//------------------------------------------------------------------------------
class X32_X32_X32_X32: public LW50CRaster
{
private:
    ComponentType m_Type;

public:
    X32_X32_X32_X32(ColorUtils::Format SurfFormat, UINT32 DevFormat, ComponentType Type):
        LW50CRaster(SurfFormat, DevFormat, 16, 4)
    {
        m_Type = Type;
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R32_G32_B32_A32) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A) |
            GetTextureHeaderDataTypes(m_Type);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R32_G32_B32_A32, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
        SetMaxwellTextureHeaderDataTypes(state, m_Type);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R32_G32_B32_A32, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
        SetHopperTextureHeaderDataTypes(state, m_Type);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT32 *)(m_image_base + n*m_bpp + 0),
                       *(UINT32 *)(m_image_base + n*m_bpp + 4),
                       *(UINT32 *)(m_image_base + n*m_bpp + 8),
                       *(UINT32 *)(m_image_base + n*m_bpp + 12));
    }
};

//------------------------------------------------------------------------------
class RF32_GF32_BF32_AF32: public X32_X32_X32_X32
{
public:
    RF32_GF32_BF32_AF32(): X32_X32_X32_X32(
        ColorUtils::RF32_GF32_BF32_AF32,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_AF32, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c[4];
        c[0] = color.GetRFloat32();
        c[1] = color.GetGFloat32();
        c[2] = color.GetBFloat32();
        c[3] = color.GetAFloat32();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red   = abs_diff(p1.RedFloat32()   , p2.RedFloat32());
        float delta_green = abs_diff(p1.GreenFloat32() , p2.GreenFloat32());
        float delta_blue  = abs_diff(p1.BlueFloat32()  , p2.BlueFloat32());
        float delta_alpha = abs_diff(p1.AlphaFloat32() , p2.AlphaFloat32());

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0) &&
               delta_alpha <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.RedFloat32()   , pf.GetRFloat32f()) <= threshold.getFloat32(0) &&
               abs_diff(p.GreenFloat32() , pf.GetGFloat32f()) <= threshold.getFloat32(0) &&
               abs_diff(p.BlueFloat32()  , pf.GetBFloat32f()) <= threshold.getFloat32(0) &&
               abs_diff(p.AlphaFloat32() , pf.GetAFloat32f()) <= threshold.getFloat32(0);
    }
};

//------------------------------------------------------------------------------
class RF32_GF32_BF32_X32: public X32_X32_X32_X32
{
public:
    RF32_GF32_BF32_X32(): X32_X32_X32_X32(
        ColorUtils::RF32_GF32_BF32_X32,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32_BF32_X32, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c[4];
        c[0] = color.GetRFloat32();
        c[1] = color.GetGFloat32();
        c[2] = color.GetBFloat32();
        c[3] = 0;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red   = abs_diff(p1.RedFloat32()   , p2.RedFloat32());
        float delta_green = abs_diff(p1.GreenFloat32() , p2.GreenFloat32());
        float delta_blue  = abs_diff(p1.BlueFloat32()  , p2.BlueFloat32());

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.RedFloat32()   , pf.GetRFloat32f()) <= threshold.getFloat32(0) &&
               abs_diff(p.GreenFloat32() , pf.GetGFloat32f()) <= threshold.getFloat32(0) &&
               abs_diff(p.BlueFloat32()  , pf.GetBFloat32f()) <= threshold.getFloat32(0);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT32 *)(m_image_base + n*m_bpp + 0),
                       *(UINT32 *)(m_image_base + n*m_bpp + 4),
                       *(UINT32 *)(m_image_base + n*m_bpp + 8),
                       0);
    }
};

//------------------------------------------------------------------------------
class RS32_GS32_BS32_AS32: public X32_X32_X32_X32
{
public:
    RS32_GS32_BS32_AS32(): X32_X32_X32_X32(
        ColorUtils::RS32_GS32_BS32_AS32,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_AS32, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        INT32 c[4];
        c[0] = color.GetRSInt(32);
        c[1] = color.GetGSInt(32);
        c[2] = color.GetBSInt(32);
        c[3] = color.GetASInt(32);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getSNorm(0, 32) &&
               delta_green <= threshold.getSNorm(0, 32) &&
               delta_blue  <= threshold.getSNorm(0, 32) &&
               delta_alpha <= threshold.getSNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSInt(32)) <= threshold.getSNorm(0, 32) &&
               abs_diff(p.Green() , pf.GetGSInt(32)) <= threshold.getSNorm(0, 32) &&
               abs_diff(p.Blue()  , pf.GetBSInt(32)) <= threshold.getSNorm(0, 32) &&
               abs_diff(p.Alpha() , pf.GetASInt(32)) <= threshold.getSNorm(0, 32);
    }
};

//------------------------------------------------------------------------------
class RS32_GS32_BS32_X32: public X32_X32_X32_X32
{
public:
    RS32_GS32_BS32_X32(): X32_X32_X32_X32(
        ColorUtils::RS32_GS32_BS32_X32,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32_BS32_X32, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        INT32 c[4];
        c[0] = color.GetRSInt(32);
        c[1] = color.GetGSInt(32);
        c[2] = color.GetBSInt(32);
        c[3] = 0;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= threshold.getSNorm(0, 32) &&
               delta_green <= threshold.getSNorm(0, 32) &&
               delta_blue  <= threshold.getSNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSInt(32)) <= threshold.getSNorm(0, 32) &&
               abs_diff(p.Green() , pf.GetGSInt(32)) <= threshold.getSNorm(0, 32) &&
               abs_diff(p.Blue()  , pf.GetBSInt(32)) <= threshold.getSNorm(0, 32);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT32 *)(m_image_base + n*m_bpp + 0),
                       *(UINT32 *)(m_image_base + n*m_bpp + 4),
                       *(UINT32 *)(m_image_base + n*m_bpp + 8),
                       0);
    }
};

//------------------------------------------------------------------------------
class RU32_GU32_BU32_AU32: public X32_X32_X32_X32
{
public:
    RU32_GU32_BU32_AU32(): X32_X32_X32_X32(
        ColorUtils::RU32_GU32_BU32_AU32,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_AU32, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c[4];
        c[0] = color.GetRUInt(32);
        c[1] = color.GetGUInt(32);
        c[2] = color.GetBUInt(32);
        c[3] = color.GetAUInt(32);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getUNorm(0, 32) &&
               delta_green <= threshold.getUNorm(0, 32) &&
               delta_blue  <= threshold.getUNorm(0, 32) &&
               delta_alpha <= threshold.getUNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUInt(32)) <= threshold.getUNorm(0, 32) &&
               abs_diff(p.Green() , pf.GetGUInt(32)) <= threshold.getUNorm(0, 32) &&
               abs_diff(p.Blue()  , pf.GetBUInt(32)) <= threshold.getUNorm(0, 32) &&
               abs_diff(p.Alpha() , pf.GetAUInt(32)) <= threshold.getUNorm(0, 32);
    }
};

//------------------------------------------------------------------------------
class RU32_GU32_BU32_X32: public X32_X32_X32_X32
{
public:
    RU32_GU32_BU32_X32(): X32_X32_X32_X32(
        ColorUtils::RU32_GU32_BU32_X32,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32_BU32_X32, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c[4];
        c[0] = color.GetRUInt(32);
        c[1] = color.GetGUInt(32);
        c[2] = color.GetBUInt(32);
        c[3] = 0;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= threshold.getUNorm(0, 32) &&
               delta_green <= threshold.getUNorm(0, 32) &&
               delta_blue  <= threshold.getUNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUInt(32)) <= threshold.getUNorm(0, 32) &&
               abs_diff(p.Green() , pf.GetGUInt(32)) <= threshold.getUNorm(0, 32) &&
               abs_diff(p.Blue()  , pf.GetBUInt(32)) <= threshold.getUNorm(0, 32);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT32 *)(m_image_base + n*m_bpp + 0),
                       *(UINT32 *)(m_image_base + n*m_bpp + 4),
                       *(UINT32 *)(m_image_base + n*m_bpp + 8),
                       0);
    }
};

//------------------------------------------------------------------------------
class X16_X16_X16_X16: public LW50CRaster
{
private:
    ComponentType m_Type;

public:
    X16_X16_X16_X16(ColorUtils::Format SurfFormat, UINT32 DevFormat, ComponentType Type):
        LW50CRaster(SurfFormat, DevFormat, 8, 4)
    {
        m_Type = Type;
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R16_G16_B16_A16) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A) |
            GetTextureHeaderDataTypes(m_Type);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R16_G16_B16_A16, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
        SetMaxwellTextureHeaderDataTypes(state, m_Type);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R16_G16_B16_A16, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
        SetHopperTextureHeaderDataTypes(state, m_Type);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT16 *)(m_image_base + n*m_bpp + 0),
                       *(UINT16 *)(m_image_base + n*m_bpp + 2),
                       *(UINT16 *)(m_image_base + n*m_bpp + 4),
                       *(UINT16 *)(m_image_base + n*m_bpp + 6));
    }
};

//------------------------------------------------------------------------------
class R16_G16_B16_A16: public X16_X16_X16_X16
{
public:
    R16_G16_B16_A16(): X16_X16_X16_X16(
        ColorUtils::R16_G16_B16_A16,
        LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16_B16_A16, Unorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[4];
        c[0] = color.GetRUNorm(16);
        c[1] = color.GetGUNorm(16);
        c[2] = color.GetBUNorm(16);
        c[3] = color.GetAUNorm(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getUNorm(0, 16) &&
               delta_green <= threshold.getUNorm(0, 16) &&
               delta_blue  <= threshold.getUNorm(0, 16) &&
               delta_alpha <= threshold.getUNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGUNorm(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Alpha() , pf.GetAUNorm(16)) <= threshold.getUNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RN16_GN16_BN16_AN16: public X16_X16_X16_X16
{
public:
    RN16_GN16_BN16_AN16(): X16_X16_X16_X16(
        ColorUtils::RN16_GN16_BN16_AN16,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16_BN16_AN16, Snorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[4];
        c[0] = color.GetRSNorm(16);
        c[1] = color.GetGSNorm(16);
        c[2] = color.GetBSNorm(16);
        c[3] = color.GetASNorm(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff_snorm(p1.Red()   , p2.Red(), 16);
        UINT32 delta_green = abs_diff_snorm(p1.Green() , p2.Green(), 16);
        UINT32 delta_blue  = abs_diff_snorm(p1.Blue()  , p2.Blue(), 16);
        UINT32 delta_alpha = abs_diff_snorm(p1.Alpha() , p2.Alpha(), 16);

        return delta_red   <= threshold.getSNorm(0, 16) &&
               delta_green <= threshold.getSNorm(0, 16) &&
               delta_blue  <= threshold.getSNorm(0, 16) &&
               delta_alpha <= threshold.getSNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSNorm(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGSNorm(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Blue()  , pf.GetBSNorm(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Alpha() , pf.GetASNorm(16)) <= threshold.getSNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RU16_GU16_BU16_AU16: public X16_X16_X16_X16
{
public:
    RU16_GU16_BU16_AU16(): X16_X16_X16_X16(
        ColorUtils::RU16_GU16_BU16_AU16,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16_BU16_AU16, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[4];
        c[0] = color.GetRUInt(16);
        c[1] = color.GetGUInt(16);
        c[2] = color.GetBUInt(16);
        c[3] = color.GetAUInt(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getUNorm(0, 16) &&
               delta_green <= threshold.getUNorm(0, 16) &&
               delta_blue  <= threshold.getUNorm(0, 16) &&
               delta_alpha <= threshold.getUNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUInt(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGUInt(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Blue()  , pf.GetBUInt(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Alpha() , pf.GetAUInt(16)) <= threshold.getUNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RS16_GS16_BS16_AS16: public X16_X16_X16_X16
{
public:
    RS16_GS16_BS16_AS16(): X16_X16_X16_X16(
        ColorUtils::RS16_GS16_BS16_AS16,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16_BS16_AS16, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[4];
        c[0] = color.GetRSInt(16);
        c[1] = color.GetGSInt(16);
        c[2] = color.GetBSInt(16);
        c[3] = color.GetASInt(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getSNorm(0, 16) &&
               delta_green <= threshold.getSNorm(0, 16) &&
               delta_blue  <= threshold.getSNorm(0, 16) &&
               delta_alpha <= threshold.getSNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSInt(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGSInt(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Blue()  , pf.GetBSInt(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Alpha() , pf.GetASInt(16)) <= threshold.getSNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RF16_GF16_BF16_AF16: public X16_X16_X16_X16
{
public:
    RF16_GF16_BF16_AF16(): X16_X16_X16_X16(
        ColorUtils::RF16_GF16_BF16_AF16,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_AF16, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[4];
        c[0] = color.GetRFloat16();
        c[1] = color.GetGFloat16();
        c[2] = color.GetBFloat16();
        c[3] = color.GetAFloat16();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red   = abs_diff(Utility::Float16ToFloat32(p1.Red()),
                                     Utility::Float16ToFloat32(p2.Red()));
        float delta_green = abs_diff(Utility::Float16ToFloat32(p1.Green()),
                                     Utility::Float16ToFloat32(p2.Green()));
        float delta_blue  = abs_diff(Utility::Float16ToFloat32(p1.Blue()),
                                     Utility::Float16ToFloat32(p2.Blue()));
        float delta_alpha = abs_diff(Utility::Float16ToFloat32(p1.Alpha()),
                                     Utility::Float16ToFloat32(p2.Alpha()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0) &&
               delta_alpha <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        float delta_red   = abs_diff(Utility::Float16ToFloat32(p.Red()),
                                     Utility::Float16ToFloat32(pf.GetRFloat16()));
        float delta_green = abs_diff(Utility::Float16ToFloat32(p.Green()),
                                     Utility::Float16ToFloat32(pf.GetGFloat16()));
        float delta_blue  = abs_diff(Utility::Float16ToFloat32(p.Blue()),
                                     Utility::Float16ToFloat32(pf.GetBFloat16()));
        float delta_alpha = abs_diff(Utility::Float16ToFloat32(p.Alpha()),
                                     Utility::Float16ToFloat32(pf.GetAFloat16()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0) &&
               delta_alpha <= threshold.getFloat32(0);
    }
};

//------------------------------------------------------------------------------
class RF16_GF16_BF16_X16: public X16_X16_X16_X16
{
public:
    RF16_GF16_BF16_X16(): X16_X16_X16_X16(
        ColorUtils::RF16_GF16_BF16_X16,
        LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16_BF16_X16, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[4];
        c[0] = color.GetRFloat16();
        c[1] = color.GetGFloat16();
        c[2] = color.GetBFloat16();
        c[3] = 0;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red   = abs_diff(Utility::Float16ToFloat32(p1.Red()),
                                     Utility::Float16ToFloat32(p2.Red()));
        float delta_green = abs_diff(Utility::Float16ToFloat32(p1.Green()),
                                     Utility::Float16ToFloat32(p2.Green()));
        float delta_blue  = abs_diff(Utility::Float16ToFloat32(p1.Blue()),
                                     Utility::Float16ToFloat32(p2.Blue()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        float delta_red   = abs_diff(Utility::Float16ToFloat32(p.Red()),
                                     Utility::Float16ToFloat32(pf.GetRFloat16()));
        float delta_green = abs_diff(Utility::Float16ToFloat32(p.Green()),
                                     Utility::Float16ToFloat32(pf.GetGFloat16()));
        float delta_blue  = abs_diff(Utility::Float16ToFloat32(p.Blue()),
                                     Utility::Float16ToFloat32(pf.GetBFloat16()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0);
    }
};

//------------------------------------------------------------------------------
class X32_X32: public LW50CRaster
{
private:
    ComponentType m_Type;

public:
    X32_X32(ColorUtils::Format SurfFormat, UINT32 DevFormat, ComponentType Type):
        LW50CRaster(SurfFormat, DevFormat, 8, 4)
    {
        m_Type = Type;
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R32_G32) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT) |
            GetTextureHeaderDataTypes(m_Type);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R32_G32, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetMaxwellTextureHeaderDataTypes(state, m_Type);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R32_G32, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetHopperTextureHeaderDataTypes(state, m_Type);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT32 *)(m_image_base + n*m_bpp + 0),
                       *(UINT32 *)(m_image_base + n*m_bpp + 4),
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class RF32_GF32: public X32_X32
{
public:
    RF32_GF32(): X32_X32(
        ColorUtils::RF32_GF32, LW9097_SET_COLOR_TARGET_FORMAT_V_RF32_GF32, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c[2];
        c[0] = color.GetRFloat32();
        c[1] = color.GetGFloat32();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red   = abs_diff(p1.RedFloat32()   , p2.RedFloat32());
        float delta_green = abs_diff(p1.GreenFloat32() , p2.GreenFloat32());

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);
        return abs_diff(p.RedFloat32()   , pf.GetRFloat32f()) <= threshold.getFloat32(0) &&
               abs_diff(p.GreenFloat32() , pf.GetGFloat32f()) <= threshold.getFloat32(0);
    }
};

//------------------------------------------------------------------------------
class RS32_GS32: public X32_X32
{
public:
    RS32_GS32(): X32_X32(
        ColorUtils::RS32_GS32, LW9097_SET_COLOR_TARGET_FORMAT_V_RS32_GS32, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c[2];
        c[0] = color.GetRSInt(32);
        c[1] = color.GetGSInt(32);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= threshold.getSNorm(0, 32) &&
               delta_green <= threshold.getSNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSInt(32)) <= threshold.getSNorm(0, 32) &&
               abs_diff(p.Green() , pf.GetGSInt(32)) <= threshold.getSNorm(0, 32);
    }
};

//------------------------------------------------------------------------------
class RU32_GU32: public X32_X32
{
public:
    RU32_GU32(): X32_X32(
        ColorUtils::RU32_GU32, LW9097_SET_COLOR_TARGET_FORMAT_V_RU32_GU32, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c[2];
        c[0] = color.GetRUInt(32);
        c[1] = color.GetGUInt(32);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= threshold.getUNorm(0, 32) &&
               delta_green <= threshold.getUNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUInt(32)) <= threshold.getUNorm(0, 32) &&
               abs_diff(p.Green() , pf.GetGUInt(32)) <= threshold.getUNorm(0, 32);
    }
};

//------------------------------------------------------------------------------
class X32: public LW50CRaster
{
private:
    ComponentType m_Type;

public:
    X32(ColorUtils::Format SurfFormat, UINT32 DevFormat, ComponentType Type):
        LW50CRaster(SurfFormat, DevFormat, 4, 4)
    {
        m_Type = Type;
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R32) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT) |
            GetTextureHeaderDataTypes(m_Type);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R32, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetMaxwellTextureHeaderDataTypes(state, m_Type);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R32, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetHopperTextureHeaderDataTypes(state, m_Type);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT32 *)(m_image_base + n*m_bpp),
                       0,
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class RS32: public X32
{
public:
    RS32(): X32(ColorUtils::RS32, LW9097_SET_COLOR_TARGET_FORMAT_V_RS32, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRSInt(32);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red(), GetPixel(n2).Red()) <= threshold.getSNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red(), pf.GetRSInt(32)) <= threshold.getSNorm(0, 32);
    }
};

//------------------------------------------------------------------------------
class RU32: public X32
{
public:
    RU32(): X32(ColorUtils::RU32, LW9097_SET_COLOR_TARGET_FORMAT_V_RU32, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRUInt(32);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red(), GetPixel(n2).Red()) <= threshold.getUNorm(0, 32);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red(), pf.GetRUInt(32)) <= threshold.getUNorm(0, 32);
    }
};

//------------------------------------------------------------------------------
class RF32: public X32
{
public:
    RF32(): X32(ColorUtils::RF32, LW9097_SET_COLOR_TARGET_FORMAT_V_RF32, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetRFloat32();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).RedFloat32(), GetPixel(n2).RedFloat32()) <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).RedFloat32(), pf.GetRFloat32f()) <= threshold.getFloat32(0);
    }
};

//------------------------------------------------------------------------------
class A2B10G10R10: public LW50CRaster
{
public:
    A2B10G10R10(): LW50CRaster(
        ColorUtils::A2B10G10R10, LW9097_SET_COLOR_TARGET_FORMAT_V_A2B10G10R10, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c;
        c  = color.GetRUNorm(10);
        c |= color.GetGUNorm(10) << 10;
        c |= color.GetBUNorm(10) << 20;
        c |= color.GetAUNorm(2)  << 30;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A2B10G10R10) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A2B10G10R10, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A2B10G10R10, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return (delta_red   <= threshold.getUNorm(0, 10)) &&
               (delta_green <= threshold.getUNorm(0, 10)) &&
               (delta_blue  <= threshold.getUNorm(0, 10)) &&
               (delta_alpha <= (threshold.getRaw(0) ? 1u : 0u));
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return (abs_diff(p.Red(),   pf.GetRUNorm(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Green(), pf.GetGUNorm(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Blue(),  pf.GetBUNorm(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Alpha(), pf.GetAUNorm(2))  <= (threshold.getRaw(0) ? 1u : 0u));
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *((UINT32 *)((m_image_base + n*m_bpp)));
        return PixelFB((pixel & 0x3ff),
                       (pixel & 0xffc00) >> 10,
                       (pixel & 0x3ff00000) >> 20,
                       (pixel & 0xc0000000) >> 30);
    }
};

//------------------------------------------------------------------------------
class AU2BU10GU10RU10: public LW50CRaster
{
public:
    AU2BU10GU10RU10(): LW50CRaster(
        ColorUtils::AU2BU10GU10RU10, LW9097_SET_COLOR_TARGET_FORMAT_V_AU2BU10GU10RU10, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c;
        c  = color.GetRUInt(10);
        c |= color.GetGUInt(10) << 10;
        c |= color.GetBUInt(10) << 20;
        c |= color.GetAUInt(2)  << 30;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A2B10G10R10) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A2B10G10R10, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A2B10G10R10, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UINT, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return (delta_red   <= threshold.getUNorm(0, 10)) &&
               (delta_green <= threshold.getUNorm(0, 10)) &&
               (delta_blue  <= threshold.getUNorm(0, 10)) &&
               (delta_alpha <= (threshold.getRaw(0) ? 1u : 0u));
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return (abs_diff(p.Red()   , pf.GetRUInt(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Green() , pf.GetGUInt(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Blue()  , pf.GetBUInt(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Alpha() , pf.GetAUInt(2))  <= (threshold.getRaw(0) ? 1u : 0u));
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *((UINT32 *)((m_image_base + n*m_bpp)));
        return PixelFB((pixel & 0x3ff),
                       (pixel & 0xffc00) >> 10,
                       (pixel & 0x3ff00000) >> 20,
                       (pixel & 0xc0000000) >> 30);
    }
};

//------------------------------------------------------------------------------
class X16_X16: public LW50CRaster
{
private:
    ComponentType m_Type;

public:
    X16_X16(ColorUtils::Format SurfFormat, UINT32 DevFormat, ComponentType Type):
        LW50CRaster(SurfFormat, DevFormat, 4, 4)
    {
        m_Type = Type;
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R16_G16) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT) |
            GetTextureHeaderDataTypes(m_Type);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R16_G16, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetMaxwellTextureHeaderDataTypes(state, m_Type);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R16_G16, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetHopperTextureHeaderDataTypes(state, m_Type);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT16 *)(m_image_base + n*m_bpp + 0),
                       *(UINT16 *)(m_image_base + n*m_bpp + 2),
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class RF16_GF16: public X16_X16
{
public:
    RF16_GF16(): X16_X16(
        ColorUtils::RF16_GF16, LW9097_SET_COLOR_TARGET_FORMAT_V_RF16_GF16, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[2];
        c[0] = color.GetRFloat16();
        c[1] = color.GetGFloat16();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red   = abs_diff(Utility::Float16ToFloat32(p1.Red()),
                                     Utility::Float16ToFloat32(p2.Red()));
        float delta_green = abs_diff(Utility::Float16ToFloat32(p1.Green()),
                                     Utility::Float16ToFloat32(p2.Green()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        float delta_red   = abs_diff(Utility::Float16ToFloat32(p.Red()),
                                     Utility::Float16ToFloat32(pf.GetRFloat16()));
        float delta_green = abs_diff(Utility::Float16ToFloat32(p.Green()),
                                     Utility::Float16ToFloat32(pf.GetGFloat16()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0);
    }
};

//------------------------------------------------------------------------------
class RS16_GS16: public X16_X16
{
public:
    RS16_GS16(): X16_X16(
        ColorUtils::RS16_GS16, LW9097_SET_COLOR_TARGET_FORMAT_V_RS16_GS16, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[2];
        c[0] = color.GetRSInt(16);
        c[1] = color.GetGSInt(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= threshold.getSNorm(0, 16) &&
               delta_green <= threshold.getSNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSInt(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGSInt(16)) <= threshold.getSNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RN16_GN16: public X16_X16
{
public:
    RN16_GN16(): X16_X16(
        ColorUtils::RN16_GN16, LW9097_SET_COLOR_TARGET_FORMAT_V_RN16_GN16, Snorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[2];
        c[0] = color.GetRSNorm(16);
        c[1] = color.GetGSNorm(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff_snorm(p1.Red()   , p2.Red(), 16);
        UINT32 delta_green = abs_diff_snorm(p1.Green() , p2.Green(), 16);

        return delta_red   <= threshold.getSNorm(0, 16) &&
               delta_green <= threshold.getSNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSNorm(16)) <= threshold.getSNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGSNorm(16)) <= threshold.getSNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RU16_GU16: public X16_X16
{
public:
    RU16_GU16(): X16_X16(
        ColorUtils::RU16_GU16, LW9097_SET_COLOR_TARGET_FORMAT_V_RU16_GU16, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[2];
        c[0] = color.GetRUInt(16);
        c[1] = color.GetGUInt(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= threshold.getUNorm(0, 16) &&
               delta_green <= threshold.getUNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUInt(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGUInt(16)) <= threshold.getUNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class R16_G16: public X16_X16
{
public:
    R16_G16(): X16_X16(
        ColorUtils::R16_G16, LW9097_SET_COLOR_TARGET_FORMAT_V_R16_G16, Unorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c[2];
        c[0] = color.GetRUNorm(16);
        c[1] = color.GetGUNorm(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= threshold.getUNorm(0, 16) &&
               delta_green <= threshold.getUNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(16)) <= threshold.getUNorm(0, 16) &&
               abs_diff(p.Green() , pf.GetGUNorm(16)) <= threshold.getUNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class A2R10G10B10: public LW50CRaster
{
public:
    A2R10G10B10(): LW50CRaster(
        ColorUtils::A2R10G10B10, LW9097_SET_COLOR_TARGET_FORMAT_V_A2R10G10B10, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c;
        c  = color.GetBUNorm(10);
        c |= color.GetGUNorm(10) << 10;
        c |= color.GetRUNorm(10) << 20;
        c |= color.GetAUNorm(2)  << 30;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A2B10G10R10) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A2B10G10R10, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A2B10G10R10, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return (delta_red   <= threshold.getUNorm(0, 10)) &&
               (delta_green <= threshold.getUNorm(0, 10)) &&
               (delta_blue  <= threshold.getUNorm(0, 10)) &&
               (delta_alpha <= (threshold.getRaw(0) ? 1u : 0u));
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return (abs_diff(p.Red(),   pf.GetRUNorm(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Green(), pf.GetGUNorm(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Blue(),  pf.GetBUNorm(10)) <= threshold.getUNorm(0, 10)) &&
               (abs_diff(p.Alpha() ,pf.GetAUNorm(2))  <= (threshold.getRaw(0) ? 1u : 0u));
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0x3ff00000) >> 20,
                       (pixel & 0xffc00) >> 10,
                       (pixel & 0x3ff),
                       (pixel & 0xc0000000) >> 30);
    }
};

//------------------------------------------------------------------------------
class BF10GF11RF11: public LW50CRaster
{
public:
    BF10GF11RF11(): LW50CRaster(
        ColorUtils::BF10GF11RF11, LW9097_SET_COLOR_TARGET_FORMAT_V_BF10GF11RF11, 4, 4) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c;
        c  = color.GetRFloat11();
        c |= color.GetGFloat11() << 11;
        c |= color.GetBFloat10() << 22;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _BF10GF11RF11) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_FLOAT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_FLOAT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_FLOAT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_BF10GF11RF11, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_FLOAT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_FLOAT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_FLOAT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_BF10GF11RF11, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_FLOAT, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red   = abs_diff(Float11ToFloat32(p1.Red()),
                                     Float11ToFloat32(p2.Red()));
        float delta_green = abs_diff(Float11ToFloat32(p1.Green()),
                                     Float11ToFloat32(p2.Green()));
        float delta_blue  = abs_diff(Float10ToFloat32(p1.Blue()),
                                     Float10ToFloat32(p2.Blue()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        float delta_red   = abs_diff(Float11ToFloat32(p.Red()),
                                     Float11ToFloat32(pf.GetRFloat11()));
        float delta_green = abs_diff(Float11ToFloat32(p.Green()),
                                     Float11ToFloat32(pf.GetGFloat11()));
        float delta_blue  = abs_diff(Float10ToFloat32(p.Blue()),
                                     Float10ToFloat32(pf.GetBFloat10()));

        return delta_red   <= threshold.getFloat32(0) &&
               delta_green <= threshold.getFloat32(0) &&
               delta_blue  <= threshold.getFloat32(0);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB(pixel & 0x7ff,
                       (pixel & 0x3ff800) >> 11,
                       (pixel & 0xffc00000) >> 22,
                       0);
    }
};

//------------------------------------------------------------------------------
class A1R5G5B5: public LW50CRaster
{
public:
    A1R5G5B5(): LW50CRaster(
        ColorUtils::A1R5G5B5, LW9097_SET_COLOR_TARGET_FORMAT_V_A1R5G5B5, 2, 2)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c;
        c  = color.GetBUNorm(5);
        c |= color.GetGUNorm(5) << 5;
        c |= color.GetRUNorm(5) << 10;
        c |= color.GetAUNorm(1) << 15;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A1B5G5R5) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_A);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A1B5G5R5, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_A, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A1B5G5R5, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_A, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_green <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_blue  <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_alpha <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Green() , pf.GetGUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Alpha() , pf.GetAUNorm(1)) <= (threshold.getRaw(0) ? 1u : 0u);
        }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT16 pixel = *(UINT16 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0x7c00) >> 10,
                       (pixel & 0x3e0) >> 5,
                       (pixel & 0x1f),
                       (pixel & 0x8000) >> 15);
    }
};

//------------------------------------------------------------------------------
class X1R5G5B5: public LW50CRaster
{
public:
    X1R5G5B5(): LW50CRaster(
        ColorUtils::X1R5G5B5, LW9097_SET_COLOR_TARGET_FORMAT_V_X1R5G5B5, 2, 2) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c;
        c  = color.GetBUNorm(5);
        c |= color.GetGUNorm(5) << 5;
        c |= color.GetRUNorm(5) << 10;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A1B5G5R5) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A1B5G5R5, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A1B5G5R5, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_B, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());

        return delta_red   <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_green <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_blue  <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Green() , pf.GetGUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(5)) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT16 pixel = *(UINT16 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0x7c00) >> 10,
                       (pixel & 0x3e0) >> 5,
                       (pixel & 0x1f),
                       0);
    }
};

//------------------------------------------------------------------------------
class G8R8: public LW50CRaster
{
public:
    G8R8(): LW50CRaster(
        ColorUtils::G8R8, LW9097_SET_COLOR_TARGET_FORMAT_V_G8R8, 2, 2) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRUNorm(8) |
                  (color.GetGUNorm(8) << 8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT16 pixel = *(UINT16 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class GN8RN8: public LW50CRaster
{
public:
    GN8RN8(): LW50CRaster(
        ColorUtils::GN8RN8, LW9097_SET_COLOR_TARGET_FORMAT_V_GN8RN8, 2, 2) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRSNorm(8) |
                  (color.GetGSNorm(8) << 8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_SNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_SNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff_snorm(p1.Red()   , p2.Red(), 8);
        UINT32 delta_green = abs_diff_snorm(p1.Green() , p2.Green(), 8);

        return delta_red   <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_green <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSNorm(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Green() , pf.GetGSNorm(8)) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT16 pixel = *(UINT16 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class GS8RS8: public LW50CRaster
{
public:
    GS8RS8(): LW50CRaster(
        ColorUtils::GS8RS8, LW9097_SET_COLOR_TARGET_FORMAT_V_GS8RS8, 2, 2) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRSInt(8) |
                  (color.GetGSInt(8) << 8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_SINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_SINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SINT, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= (threshold.getRaw(0) ? 1u : 0u) &&
               delta_green <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRSInt(8)) <= (threshold.getRaw(0) ? 1u : 0u) &&
               abs_diff(p.Green() , pf.GetGSInt(8)) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT16 pixel = *(UINT16 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class GU8RU8: public LW50CRaster
{
public:
    GU8RU8(): LW50CRaster(
        ColorUtils::GU8RU8, LW9097_SET_COLOR_TARGET_FORMAT_V_GU8RU8, 2, 2) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRUInt(8) |
                  (color.GetGUInt(8) << 8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UINT, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUInt(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUInt(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT16 pixel = *(UINT16 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff),
                       (pixel & 0xff00) >> 8,
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class X16: public LW50CRaster
{
private:
    ComponentType m_Type;

public:
    X16(ColorUtils::Format SurfFormat, UINT32 DevFormat, ComponentType Type):
        LW50CRaster(SurfFormat, DevFormat, 2, 2)
    {
        m_Type = Type;
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R16) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT) |
            GetTextureHeaderDataTypes(m_Type);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R16, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetMaxwellTextureHeaderDataTypes(state, m_Type);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R16, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetHopperTextureHeaderDataTypes(state, m_Type);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT16 *)(m_image_base + n*m_bpp),
                       0,
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class R16: public X16
{
public:
    R16(): X16(ColorUtils::R16, LW9097_SET_COLOR_TARGET_FORMAT_V_R16, Unorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRUNorm(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red() , GetPixel(n2).Red()) <= threshold.getUNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRUNorm(16)) <= threshold.getUNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RN16: public X16
{
public:
    RN16(): X16(ColorUtils::RN16, LW9097_SET_COLOR_TARGET_FORMAT_V_RN16, Snorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRSNorm(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff_snorm(GetPixel(n1).Red() , GetPixel(n2).Red(), 16) <= threshold.getSNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRSNorm(16)) <= threshold.getSNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RS16: public X16
{
public:
    RS16(): X16(ColorUtils::RS16, LW9097_SET_COLOR_TARGET_FORMAT_V_RS16, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRSInt(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red() , GetPixel(n2).Red()) <= threshold.getSNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRSInt(16)) <= threshold.getSNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RU16: public X16
{
public:
    RU16(): X16(ColorUtils::RU16, LW9097_SET_COLOR_TARGET_FORMAT_V_RU16, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRUInt(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red() , GetPixel(n2).Red()) <= threshold.getUNorm(0, 16);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRUInt(16)) <= threshold.getUNorm(0, 16);
    }
};

//------------------------------------------------------------------------------
class RF16: public X16
{
public:
    RF16(): X16(ColorUtils::RF16, LW9097_SET_COLOR_TARGET_FORMAT_V_RF16, Float) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 c = color.GetRFloat16();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        float delta_red = abs_diff(Utility::Float16ToFloat32(p1.Red()),
                                   Utility::Float16ToFloat32(p2.Red()));

        return delta_red <= threshold.getFloat32(0);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        float delta_red = abs_diff(Utility::Float16ToFloat32(p.Red()),
                                   Utility::Float16ToFloat32(pf.GetRFloat16()));

        return delta_red <= threshold.getFloat32(0);
    }
};

//------------------------------------------------------------------------------
class X8: public LW50CRaster
{
private:
    ComponentType m_Type;

public:
    X8(ColorUtils::Format SurfFormat, UINT32 DevFormat, ComponentType Type):
        LW50CRaster(SurfFormat, DevFormat, 1, 1)
    {
        m_Type = Type;
    }

    UINT32 Granularity() const { return 1; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R8) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_ONE_FLOAT) |
            GetTextureHeaderDataTypes(m_Type);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetMaxwellTextureHeaderDataTypes(state, m_Type);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_ONE_FLOAT, state);
        SetHopperTextureHeaderDataTypes(state, m_Type);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT08 *)(m_image_base + n*m_bpp),
                       0,
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class R8: public X8
{
public:
    R8(): X8(ColorUtils::R8, LW9097_SET_COLOR_TARGET_FORMAT_V_R8, Unorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT08 c = color.GetRUNorm(8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red() , GetPixel(n2).Red()) <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRUNorm(8)) <= threshold.getUNorm(0, 8);
    }
};

//------------------------------------------------------------------------------
class RN8: public X8
{
public:
    RN8(): X8(ColorUtils::RN8, LW9097_SET_COLOR_TARGET_FORMAT_V_RN8, Snorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT08 c = color.GetRSNorm(8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff_snorm(GetPixel(n1).Red() , GetPixel(n2).Red(), 8) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRSNorm(8)) <= (threshold.getRaw(0) ? 1u : 0u);
    }
};

//------------------------------------------------------------------------------
class RS8: public X8
{
public:
    RS8(): X8(ColorUtils::RS8, LW9097_SET_COLOR_TARGET_FORMAT_V_RS8, Sint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT08 c = color.GetRSInt(8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red() , GetPixel(n2).Red()) <= (threshold.getRaw(0) ? 1u : 0u);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRSInt(8)) <= (threshold.getRaw(0) ? 1u : 0u);
    }
};

//------------------------------------------------------------------------------
class RU8: public X8
{
public:
    RU8(): X8(ColorUtils::RU8, LW9097_SET_COLOR_TARGET_FORMAT_V_RU8, Uint) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT08 c = color.GetRUInt(8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Red() , GetPixel(n2).Red()) <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Red()   , pf.GetRUInt(8)) <= threshold.getUNorm(0, 8);
    }
};

//------------------------------------------------------------------------------
class A8: public X8
{
public:
    A8(): X8(ColorUtils::A8, LW9097_SET_COLOR_TARGET_FORMAT_V_A8, Unorm) {}

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT08 c = color.GetAUNorm(8);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n1).Alpha() , GetPixel(n2).Alpha()) <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        return abs_diff(GetPixel(n).Alpha() , pf.GetAUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(0, 0, 0, *(UINT08 *)(m_image_base + n*m_bpp));
    }
};

//------------------------------------------------------------------------------
class Z24S8: public LW50ZRaster
{
public:
    Z24S8() : LW50ZRaster(
        ColorUtils::Z24S8, LW9097_SET_ZT_FORMAT_V_Z24S8, 4, 4, false, 24, 8)
    {
    }

    void ColwertToTga(UINT08 *to, UINT08 *from, UINT32 size) const
    {
        UINT32 u,i,j;

        for (i = j = u = 0; u < size; ++u)
        {
            to[j]   = from[i+1];
            to[j+1] = from[i+2];
            to[j+2] = from[i+3];
            to[j+3] = from[i];
            i += 4;
            j += 4;
        }
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval = (z.GetUNorm(24) << 8) | s.Get();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        UINT32 val =
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _Z24S8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_G);
                break;
            case TEXTURE_MODE_STENCIL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        return val;
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_Z24S8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_G, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_Z24S8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_ZS, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_G, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        UINT32 data1 = *(UINT32 *)(m_image_base + n1*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n2*m_bpp);
        UINT32 z1 = (data1 & 0xFFFFFF00) >> 8;
        UINT32 z2 = (data2 & 0xFFFFFF00) >> 8;
        UINT32 s1 = data1 & 0xFF;
        UINT32 s2 = data2 & 0xFF;

        return (abs_diff(z1, z2) <= threshold.getUNorm(0, 24)) &&
               (s1 == s2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((data & 0xFFFFFF00) >> 8, data & 0xFF, 0, 0);
    }
};

//------------------------------------------------------------------------------
class S8: public LW50ZRaster
{
public:
    S8() : LW50ZRaster(
        ColorUtils::S8, LWB197_SET_ZT_FORMAT_V_S8, 1, 1, false, 0, 8)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT08 sVal = s.Get();
        for (unsigned i = 0; i < GobSize; i += sizeof(sVal))
        {
            memcpy(&buff[i], &sVal, sizeof(sVal));
        }
    }

    UINT32 Granularity() const { return 1; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_STENCIL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_ZERO) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_STENCIL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_STENCIL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_ZERO, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        UINT32 data1 = *(UINT32 *)(m_image_base + n1*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n2*m_bpp);
        UINT32 s1 = data1 & 0xFF;
        UINT32 s2 = data2 & 0xFF;

        return (s1 == s2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(0,
                       *(UINT08 *)(m_image_base + n*m_bpp),
                       0,
                       0);
    }
};

//------------------------------------------------------------------------------
class ZF32: public LW50ZRaster
{
public:
    ZF32(): LW50ZRaster(
        ColorUtils::ZF32, LW9097_SET_ZT_FORMAT_V_ZF32, 4, 4, true, 32, 0)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval = z.GetFloat();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_FLOAT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_FLOAT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_ZF32, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_ZFS, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        float z1 = *(float *)(m_image_base + n1*m_bpp);
        float z2 = *(float *)(m_image_base + n2*m_bpp);

        return abs_diff(z1, z2) <= threshold.getFloat32(0);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB(data, 0, 0, 0);
    }
};

//------------------------------------------------------------------------------
class S8Z24: public LW50ZRaster
{
public:
    S8Z24(): LW50ZRaster(
        ColorUtils::S8Z24, LW9097_SET_ZT_FORMAT_V_S8Z24, 4, 4, false, 24, 8)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval = (s.Get() << 24 ) | z.GetUNorm(24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        UINT32 val =
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _S8Z24) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
                break;
            case TEXTURE_MODE_STENCIL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_G);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        return val;
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_S8Z24, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_G, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_S8Z24, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SZ, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_G, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        UINT32 data1 = *(UINT32 *)(m_image_base + n1*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n2*m_bpp);
        UINT32 z1 = data1 & 0xFFFFFF;
        UINT32 z2 = data2 & 0xFFFFFF;
        UINT32 s1 = (data1 & 0xFF000000) >> 24;
        UINT32 s2 = (data2 & 0xFF000000) >> 24;

        return (abs_diff(z1, z2) <= threshold.getUNorm(0, 24)) &&
               (s1 == s2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB(data & 0x00FFFFFF, (data & 0xFF000000) >> 24, 0, 0);
    }
};

//------------------------------------------------------------------------------
class X8Z24: public LW50ZRaster
{
public:
    X8Z24(): LW50ZRaster(
        ColorUtils::X8Z24, LW9097_SET_ZT_FORMAT_V_X8Z24, 4, 4, false, 24, 0)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval = z.GetUNorm(24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8Z24) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8Z24, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_X8Z24, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SZ, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        UINT32 data1 = *(UINT32 *)(m_image_base + n1*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n2*m_bpp);
        UINT32 z1 = data1 & 0xFFFFFF;
        UINT32 z2 = data2 & 0xFFFFFF;

        return abs_diff(z1, z2) <= threshold.getUNorm(0, 24);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB(data & 0x00FFFFFF, 0, 0, 0);
    }
};

//------------------------------------------------------------------------------
class V8Z24: public LW50ZRaster
{
public:
    V8Z24(): LW50ZRaster(
        ColorUtils::V8Z24, LW9097_SET_ZT_FORMAT_V_V8Z24, 4, 4, false, 24, 0)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval = 0xff000000 | z.GetUNorm(24);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        UINT32 val =
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
                break;
            case TEXTURE_MODE_VCAA:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_B);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X4V4Z24__COV4R4V);
                break;
            case AAMODE_2X2_VC_12:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _V8Z24__COV4R12V);
                break;
            case AAMODE_4X2_VC_8:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X4V4Z24__COV8R8V);
                break;
            case AAMODE_4X2_VC_24:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _V8Z24__COV8R24V);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
        return val;
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_VCAA:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_B, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X4V4Z24__COV4R4V, state);
                break;
            case AAMODE_2X2_VC_12:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_V8Z24__COV4R12V, state);
                break;
            case AAMODE_4X2_VC_8:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X4V4Z24__COV8R8V, state);
                break;
            case AAMODE_4X2_VC_24:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_V8Z24__COV8R24V, state);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(!"Raster type not supported by Hopper texture header.");
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        UINT32 data1 = *(UINT32 *)(m_image_base + n1*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n2*m_bpp);
        UINT32 z1 = data1 & 0xFFFFFF;
        UINT32 z2 = data2 & 0xFFFFFF;
        UINT32 v1 = (data1 & 0xFF000000) >> 24;
        UINT32 v2 = (data2 & 0xFF000000) >> 24;

        return (abs_diff(z1, z2) <= threshold.getUNorm(0, 24)) &&
               (v1 == v2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB(data & 0x00FFFFFF, 0, (data & 0xFF000000) >> 24, 0);
    }
};

//------------------------------------------------------------------------------
class ZF32_X24S8: public LW50ZRaster
{
public:
    ZF32_X24S8(): LW50ZRaster(
        ColorUtils::ZF32_X24S8, LW9097_SET_ZT_FORMAT_V_ZF32_X24S8, 8, 4, true, 32, 8)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval[2];
        zval[0] = z.GetFloat();
        zval[1] = s.Get();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        UINT32 val =
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X24S8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_FLOAT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
                break;
            case TEXTURE_MODE_STENCIL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_G);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        return val;
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X24S8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_FLOAT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_G, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_ZF32_X24S8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_ZFS, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_G, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        float z1 = *(float *)(m_image_base + n1*m_bpp);
        float z2 = *(float *)(m_image_base + n2*m_bpp);
        UINT32 s1 = *(UINT32 *)(m_image_base + n1*m_bpp + 4) & 0xFF;
        UINT32 s2 = *(UINT32 *)(m_image_base + n2*m_bpp + 4) & 0xFF;

        return (abs_diff(z1, z2) <= threshold.getFloat32(0)) &&
               (s1 == s2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n*m_bpp + 4);
        return PixelFB(data, data2 & 0xFF, 0, 0);
    }
};

//------------------------------------------------------------------------------
class X8Z24_X16V8S8: public LW50ZRaster
{
public:
    X8Z24_X16V8S8(): LW50ZRaster(
        ColorUtils::X8Z24_X16V8S8, LW9097_SET_ZT_FORMAT_V_X8Z24_X16V8S8, 8, 4, false, 24, 8)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval[2];
        zval[0] = z.GetUNorm(24);
        zval[1] = 0xff00 | s.Get();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        UINT32 val =
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
                break;
            case TEXTURE_MODE_STENCIL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_G);
                break;
            case TEXTURE_MODE_VCAA:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_B);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8Z24_X20V4S8__COV4R4V);
                break;
            case AAMODE_2X2_VC_12:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8Z24_X16V8S8__COV4R12V);
                break;
            case AAMODE_4X2_VC_8:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8Z24_X20V4S8__COV8R8V);
                break;
            case AAMODE_4X2_VC_24:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _X8Z24_X16V8S8__COV8R24V);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
        return val;
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_G, state);
                break;
            case TEXTURE_MODE_VCAA:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_B, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8Z24_X20V4S8__COV4R4V, state);
                break;
            case AAMODE_2X2_VC_12:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8Z24_X16V8S8__COV4R12V, state);
                break;
            case AAMODE_4X2_VC_8:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8Z24_X20V4S8__COV8R8V, state);
                break;
            case AAMODE_4X2_VC_24:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_X8Z24_X16V8S8__COV8R24V, state);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(!"Raster type not supported by Hopper texture header.");
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        UINT32 z1 = *(UINT32 *)(m_image_base + n1*m_bpp) & 0xFFFFFF;
        UINT32 z2 = *(UINT32 *)(m_image_base + n2*m_bpp) & 0xFFFFFF;
        UINT32 vs1 = *(UINT32 *)(m_image_base + n1*m_bpp + 4) & 0xFFFF;
        UINT32 vs2 = *(UINT32 *)(m_image_base + n2*m_bpp + 4) & 0xFFFF;

        return (abs_diff(z1, z2) <= threshold.getUNorm(0, 24)) &&
               (vs1 == vs2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n*m_bpp + 4);
        return PixelFB(data & 0x00FFFFFF, data2 & 0xFF, (data2 & 0xFF00) >> 8, 0);
    }
};

//------------------------------------------------------------------------------
class ZF32_X16V8X8: public LW50ZRaster
{
public:
    ZF32_X16V8X8(): LW50ZRaster(
        ColorUtils::ZF32_X16V8X8, LW9097_SET_ZT_FORMAT_V_ZF32_X16V8X8, 8, 4, true, 32, 0)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval[2];
        zval[0] = z.GetFloat();
        zval[1] = 0xff00;
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        UINT32 val =
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_FLOAT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
                break;
            case TEXTURE_MODE_VCAA:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_B);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X20V4X8__COV4R4V);
                break;
            case AAMODE_2X2_VC_12:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X16V8X8__COV4R12V);
                break;
            case AAMODE_4X2_VC_8:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X20V4X8__COV8R8V);
                break;
            case AAMODE_4X2_VC_24:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X16V8X8__COV8R24V);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
        return val;
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_FLOAT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_VCAA:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_B, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X20V4X8__COV4R4V, state);
                break;
            case AAMODE_2X2_VC_12:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X16V8X8__COV4R12V, state);
                break;
            case AAMODE_4X2_VC_8:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X20V4X8__COV8R8V, state);
                break;
            case AAMODE_4X2_VC_24:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X16V8X8__COV8R24V, state);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(!"Raster type not supported by Hopper texture header.");
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        float z1 = *(float *)(m_image_base + n1*m_bpp);
        float z2 = *(float *)(m_image_base + n2*m_bpp);
        UINT32 v1 = *(UINT32 *)(m_image_base + n1*m_bpp + 4) & 0xFF00;
        UINT32 v2 = *(UINT32 *)(m_image_base + n2*m_bpp + 4) & 0xFF00;

        return (abs_diff(z1, z2) <= threshold.getFloat32(0)) &&
               (v1 == v2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n*m_bpp + 4);
        return PixelFB(data, 0, (data2 & 0xFF00) >> 8, 0);
    }
};

//------------------------------------------------------------------------------
class ZF32_X16V8S8: public LW50ZRaster
{
public:
    ZF32_X16V8S8(): LW50ZRaster(
        ColorUtils::ZF32_X16V8S8, LW9097_SET_ZT_FORMAT_V_ZF32_X16V8S8, 8, 4, true, 32, 8)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 zval[2];
        zval[0] = z.GetFloat();
        zval[1] = 0xff00 | s.Get();
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 4; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        UINT32 val =
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_FLOAT) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
                break;
            case TEXTURE_MODE_STENCIL:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_G) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_G);
                break;
            case TEXTURE_MODE_VCAA:
                val |=
                    DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_B) |
                    DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_B);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X20V4S8__COV4R4V);
                break;
            case AAMODE_2X2_VC_12:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X16V8S8__COV4R12V);
                break;
            case AAMODE_4X2_VC_8:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X20V4S8__COV8R8V);
                break;
            case AAMODE_4X2_VC_24:
                val |= DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _ZF32_X16V8S8__COV8R24V);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
        return val;
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_FLOAT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        switch (Mode)
        {
            case TEXTURE_MODE_NORMAL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
                break;
            case TEXTURE_MODE_STENCIL:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_G, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_G, state);
                break;
            case TEXTURE_MODE_VCAA:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_B, state);
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_B, state);
                break;
            default:
                MASSERT(!"Invalid Mode");
        }
        switch (AAMode)
        {
            case AAMODE_2X2_VC_4:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X20V4S8__COV4R4V, state);
                break;
            case AAMODE_2X2_VC_12:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X16V8S8__COV4R12V, state);
                break;
            case AAMODE_4X2_VC_8:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X20V4S8__COV8R8V, state);
                break;
            case AAMODE_4X2_VC_24:
                FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_ZF32_X16V8S8__COV8R24V, state);
                break;
            default:
                MASSERT(!"Invalid AAMode");
        }
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(!"Raster type not supported by Hopper texture header.");
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        float z1 = *(float *)(m_image_base + n1*m_bpp);
        float z2 = *(float *)(m_image_base + n2*m_bpp);
        UINT32 vs1 = *(UINT32 *)(m_image_base + n1*m_bpp + 4) & 0xFFFF;
        UINT32 vs2 = *(UINT32 *)(m_image_base + n2*m_bpp + 4) & 0xFFFF;

        return (abs_diff(z1, z2) <= threshold.getFloat32(0)) &&
               (vs1 == vs2);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 data = *(UINT32 *)(m_image_base + n*m_bpp);
        UINT32 data2 = *(UINT32 *)(m_image_base + n*m_bpp + 4);
        return PixelFB(data, data2 & 0xFF, (data2 & 0xFF00) >> 8, 0);
    }
};

class Z16: public LW50ZRaster
{
public:
    Z16() : LW50ZRaster(
        ColorUtils::Z16, LW9097_SET_ZT_FORMAT_V_Z16, 2, 2, false, 16, 0)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT16 zval = z.GetUNorm(16);
        for (unsigned i = 0; i < GobSize;
                i += sizeof(zval))
            memcpy(&buff[i], &zval, sizeof(zval));
    }

    UINT32 Granularity() const { return 2; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _Z16) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UINT) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_R);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_Z16, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UINT, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_R, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_Z16, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_SZ, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_R, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        UINT32 z1 = *(UINT16 *)(m_image_base + n1*m_bpp);
        UINT32 z2 = *(UINT16 *)(m_image_base + n2*m_bpp);

        return (abs_diff(z1, z2) <= threshold.getUNorm(0, 16));
    }

    PixelFB GetPixel(UINT32 n) const
    {
        return PixelFB(*(UINT16 *)(m_image_base + n*m_bpp), 0, 0, 0);
    }
};

//-----------------------------------------------------------------------------
class B8G8R8A8: public LW50CRaster
{
public:
    B8G8R8A8(): LW50CRaster(
        ColorUtils::B8G8R8A8, LW9097_SET_COLOR_TARGET_FORMAT_V_A8R8G8B8, 4, 4)
    {
    }

    void FillLinearGob(UINT08 *buff, const RGBAFloat &color,
                       const ZFloat &z, const Stencil &s, bool SrgbWrite,
                       UINT32 GobSize)
    {
        UINT32 c = color.GetAUNorm(8);
        if (SrgbWrite)
        {
            c |= color.GetBsRGB8() << 24;
            c |= color.GetGsRGB8() << 16;
            c |= color.GetRsRGB8() << 8;
        }
        else
        {
            c |= color.GetBUNorm(8) << 24;
            c |= color.GetGUNorm(8) << 16;
            c |= color.GetRUNorm(8) << 8;
        }
        for (unsigned i = 0; i < GobSize;
                i += sizeof(c))
            memcpy(&buff[i], &c, sizeof(c));
    }

    UINT32 Granularity() const { return 4; }
    bool SupportsSrgbWrite() const { return true; }

    UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode, GpuDevice *pGpuDevice /* = NULL */) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        return
            DRF_DEF(9097, _TEXHEAD0, _COMPONENT_SIZES, _A8B8G8R8) |
            DRF_DEF(9097, _TEXHEAD0, _R_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _G_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _B_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _A_DATA_TYPE, _NUM_UNORM) |
            DRF_DEF(9097, _TEXHEAD0, _X_SOURCE, _IN_A) |
            DRF_DEF(9097, _TEXHEAD0, _Y_SOURCE, _IN_R) |
            DRF_DEF(9097, _TEXHEAD0, _Z_SOURCE, _IN_G) |
            DRF_DEF(9097, _TEXHEAD0, _W_SOURCE, _IN_B);
    }

    void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _R_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _G_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _B_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _A_DATA_TYPE, _NUM_UNORM, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _X_SOURCE, _IN_A, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _Z_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(B097, _TEXHEAD_BL, _W_SOURCE, _IN_B, state);
    }

    void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode, TextureMode Mode) const
    {
        MASSERT(Mode == TEXTURE_MODE_NORMAL);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _COMPONENTS, _SIZES_A8B8G8R8, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _DATA_TYPE, _TEX_DATA_TYPE_UNORM, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _X_SOURCE, _IN_A, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Y_SOURCE, _IN_R, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _Z_SOURCE, _IN_G, state);
        FLD_SET_DRF_DEF_MW(CB97, _TEXHEAD_V2_BL, _W_SOURCE, _IN_B, state);
    }

    bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1> &threshold) const
    {
        PixelFB p1 = GetPixel(n1);
        PixelFB p2 = GetPixel(n2);

        UINT32 delta_red   = abs_diff(p1.Red()   , p2.Red());
        UINT32 delta_green = abs_diff(p1.Green() , p2.Green());
        UINT32 delta_blue  = abs_diff(p1.Blue()  , p2.Blue());
        UINT32 delta_alpha = abs_diff(p1.Alpha() , p2.Alpha());

        return delta_red   <= threshold.getUNorm(0, 8) &&
               delta_green <= threshold.getUNorm(0, 8) &&
               delta_blue  <= threshold.getUNorm(0, 8) &&
               delta_alpha <= threshold.getUNorm(0, 8);
    }

    bool ComparePixelsRGBA(UINT32 n, const RGBAFloat &pf, const FloatArray<1> &threshold) const
    {
        PixelFB p = GetPixel(n);

        return abs_diff(p.Red()   , pf.GetRUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Green() , pf.GetGUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Blue()  , pf.GetBUNorm(8)) <= threshold.getUNorm(0, 8) &&
               abs_diff(p.Alpha() , pf.GetAUNorm(8)) <= threshold.getUNorm(0, 8);
    }

    PixelFB GetPixel(UINT32 n) const
    {
        UINT32 pixel = *(UINT32 *)(m_image_base + n*m_bpp);
        return PixelFB((pixel & 0xff00) >> 8,
                       (pixel & 0xff0000) >> 16,
                       (pixel & 0xff000000) >> 24,
                       (pixel & 0xff));
    }
};

//------------------------------------------------------------------------------
map<const string, LW50Raster*> CreateColorMap()
{
    map<const string, LW50Raster*> ret;

    ret["RF32_GF32_BF32_AF32"] = new RF32_GF32_BF32_AF32;
    ret["RS32_GS32_BS32_AS32"] = new RS32_GS32_BS32_AS32;
    ret["RU32_GU32_BU32_AU32"] = new RU32_GU32_BU32_AU32;

    ret["RF32_GF32_BF32_X32"] = new RF32_GF32_BF32_X32;
    ret["RS32_GS32_BS32_X32"] = new RS32_GS32_BS32_X32;
    ret["RU32_GU32_BU32_X32"] = new RU32_GU32_BU32_X32;

    ret["R16_G16_B16_A16"] =     new R16_G16_B16_A16;
    ret["RN16_GN16_BN16_AN16"] = new RN16_GN16_BN16_AN16;
    ret["RU16_GU16_BU16_AU16"] = new RU16_GU16_BU16_AU16;
    ret["RS16_GS16_BS16_AS16"] = new RS16_GS16_BS16_AS16;
    ret["RF16_GF16_BF16_AF16"] = new RF16_GF16_BF16_AF16;
    ret["RF16_GF16_BF16_X16"]  = new RF16_GF16_BF16_X16;

    ret["RF32_GF32"] = new RF32_GF32;
    ret["RS32_GS32"] = new RS32_GS32;
    ret["RU32_GU32"] = new RU32_GU32;

    ret["A2B10G10R10"] = new A2B10G10R10;
    ret["AU2BU10GU10RU10"] = new AU2BU10GU10RU10;
    ret["A2R10G10B10"] = new A2R10G10B10;

    ret["RF16_GF16"] = new RF16_GF16;
    ret["R16_G16"] =   new R16_G16;
    ret["RU16_GU16"] = new RU16_GU16;
    ret["RS16_GS16"] = new RS16_GS16;
    ret["RN16_GN16"] = new RN16_GN16;

    ret["BF10GF11RF11"] = new BF10GF11RF11;

    ret["RS32"] = new RS32;
    ret["RU32"] = new RU32;
    ret["RF32"] = new RF32;

    ret["G8R8"] =   new G8R8;
    ret["GN8RN8"] = new GN8RN8;
    ret["GS8RS8"] = new GS8RS8;
    ret["GU8RU8"] = new GU8RU8;

    ret["A8R8G8B8"] = new A8R8G8B8;
    ret["X8R8G8B8"] = new X8R8G8B8;
    ret["A8B8G8R8"] = new A8B8G8R8;
    ret["X8B8G8R8"] = new X8B8G8R8;

    ret["R16"]  =   new R16;
    ret["RN16"] =   new RN16;
    ret["RS16"] =   new RS16;
    ret["RU16"] =   new RU16;
    ret["RF16"] =   new RF16;

    ret["A8"]  =   new A8;
    ret["R8"]  =   new R8;
    ret["RN8"] =   new RN8;
    ret["RS8"] =   new RS8;
    ret["RU8"] =   new RU8;

    ret["A8RL8GL8BL8"] = new A8RL8GL8BL8;
    ret["X8RL8GL8BL8"] = new X8RL8GL8BL8;
    ret["A8BL8GL8RL8"] = new A8BL8GL8RL8;
    ret["X8BL8GL8RL8"] = new X8BL8GL8RL8;

    ret["AN8BN8GN8RN8"] = new AN8BN8GN8RN8;
    ret["AS8BS8GS8RS8"] = new AS8BS8GS8RS8;
    ret["AU8BU8GU8RU8"] = new AU8BU8GU8RU8;

    ret["R5G6B5"] = new R5G6B5;

    ret["A1R5G5B5"] = new A1R5G5B5;
    ret["X1R5G5B5"] = new X1R5G5B5;

    ret["B8G8R8A8"] = new B8G8R8A8;

    return ret;
}

map<const string, LW50Raster *> CreateZetaMap()
{
    map<const string, LW50Raster *> ret;

    ret["Z24S8"]         = new Z24S8;
    ret["X8Z24"]         = new X8Z24;
    ret["S8Z24"]         = new S8Z24;
    ret["V8Z24"]         = new V8Z24;
    ret["ZF32"]          = new ZF32;
    ret["ZF32_X24S8"]    = new ZF32_X24S8;
    ret["X8Z24_X16V8S8"] = new X8Z24_X16V8S8;
    ret["ZF32_X16V8X8"]  = new ZF32_X16V8X8;
    ret["ZF32_X16V8S8"]  = new ZF32_X16V8S8;
    ret["Z16"]           = new Z16;
    ret["S8"]            = new S8;

    return ret;
}

void DeleteMap(map<const string, LW50Raster*>* map_) {
  for (map<const string, LW50Raster*>::iterator it = map_->begin();
    it != map_->end(); ++it) {
        if (it->second)
        {
            delete(it->second);
            it->second = NULL;
        }
    }
}

//------------------------------------------------------------------------------
UINT32 Float32ToFloat16(float f)
{
    UINT32 a = *(UINT32 *)&f;
    int EMAX16 = 0x1f;  // 2^5 - 1
    int EMAX32 = 0Xff;  // 2^8 - 1
    int exp, m, s;
    UINT32 fp16;

    // sign bit.
    s = (a>>31) & 0x1;

    // exponent.
    exp = (a>>23) & 0xff;

    // mantisa. last 23 bits
    m = a & 0x7fffff;

    // +Zero/-Zero
    if( (exp == 0) && (m == 0) ) {
        fp16 = 0;
    }

    // DENORM
    // a. smallest 32bit normalized value = 1/(2^30)
    // b. smallest 16bit nonzero value    = 1/(2^24)
    // since (a)<(b), denormalized 32bit values truncated to zero in 16bit fp.
    else if( (exp == 0) && (m !=0) ){
        fp16 = 0;
    }

    // +Inf/-Inf
    else if( (exp == EMAX32) && (m == 0) ) {
        fp16 = EMAX16<<10;
    }

    // NaN
    else if( (exp == EMAX32) && (m != 0) ) {
        fp16 = (EMAX16<<10) | 0x3ff;
    }

    // NORMALIZED
    else {
        int bias16 = 0xf;   // 2^4 - 1
        int bias32 = 0x7f;  // 2^7 - 1
        int exp16 = exp - bias32 + bias16;
        int m16;

        // Inf
        if( (exp16 >= EMAX16) || (exp16==EMAX16-1 && m>(0x3ff<<13)) ) {
            fp16 = EMAX16<<10;
        }

        // normalize
        else if( exp16>0 ) {
            m16 = m>>13;
            fp16 = (exp16<<10) | m16;
        }

        // denormalize
        else if( exp16 > -10 ) {
            m16 = 0x200 | (m>>14); // first 9 bits of mantisa | 10 0000 0000 (because it's normalized)
            fp16 = m16>>(-exp16);
            exp16 = 0;
        }

        // truncate to zero
        else {
            fp16 = 0;
        }
    }
    if( s==1 ) {
        fp16 |= 0x00008000;
    }
    return fp16;
}

UINT16 Float10ToFloat16(UINT16 x)
{
    // E5M5 -> S1E5M10 is just a shift
    return x << 5;
}

UINT16 Float11ToFloat16(UINT16 x)
{
    // E5M6 -> S1E5M10 is just a shift
    return x << 4;
}

UINT16 Float16ToFloat10(UINT16 x)
{
    // S1E5M10 -> E5M5: negative -> zero, and simply truncate otherwise
    if (x & 0x8000)
        return 0;
    return x >> 5;
}

UINT16 Float16ToFloat11(UINT16 x)
{
    // S1E5M10 -> E5M6: negative -> zero, and simply truncate otherwise
    if (x & 0x8000)
        return 0;
    return x >> 4;
}

float Float10ToFloat32(UINT16 a)
{
    return Utility::Float16ToFloat32(Float10ToFloat16(a));
}

float Float11ToFloat32(UINT16 a)
{
    return Utility::Float16ToFloat32(Float11ToFloat16(a));
}

UINT16 Float32ToFloat10(float f)
{
    return Float16ToFloat10(Float32ToFloat16(f));
}

UINT16 Float32ToFloat11(float f)
{
    return Float16ToFloat11(Float32ToFloat16(f));
}

//------------------------------------------------------------------------------
//used for colwersion form FP16 to srgb8
//should be in sync with //hw/lw5x/include/table_fp16_srgb8_ulp0.6.h
int lin_srgb8_ulp06[0x3c00] = {
#include "table_fp16_srgb8_ulp0.6.h"
};

UINT08 get_lin_srgb8_ulp06(UINT32 i)
{
    return lin_srgb8_ulp06[i];
}

static UINT32 unorm10_to_unorm2(UINT32 u10)
{
    /*  Only 4 bits of the u10 are needed for the colwersion.
     *  u10        -> u2
     *
     *  0000000000 -> 0
     *  ...
     *  0010111111 -> 0
     *  0011000000 -> 1
     *  ...
     *  0111111111 -> 1
     *  1000000000 -> 2
     *  ...
     *  1100111111 -> 2
     *  1101000000 -> 3
     *  ...
     *  1111111111 -> 3
     */
    UINT32 temp = u10 >> 6;
    if (temp < 3)
        return 0;
    else if (temp < 8)
        return 1;
    else if (temp < 13)
        return 2;
    else
        return 3;
}

inline bool sign(UINT32 f)
{
    return (f & 0x80000000) != 0;
}

inline UINT32 exponent(UINT32 f)
{
    return (f & 0x7F800000) >> 23;
}

inline UINT32 mantissa(UINT32 f)
{
    return (f & 0x007FFFFF);
}

static UINT32 float_to_unorm_a(UINT32 val, UINT32 ibits, UINT32 fbits)
{
    // ibits = integer bits
    // fbits = fraction bits (or guard bits)
    float f = *(float *)&val;

    if (f <= 0)
        return 0;
    if (f >= 1)
        return ( (UINT64(1)<<ibits) - 1);

    UINT64 xn = UINT64(f * (UINT64(1) << (ibits + fbits)));
    UINT64 x = xn >> ibits;
    UINT64 h = (1 << (fbits - 1)) - 1;

    return UINT32((xn - x + h) >> fbits);
}

// taken from //hw/lw5x/clib/lwshared/float_to_unorm.cpp
// toksvig agorithm for colwersion, modified to yield 0.5->0x800000
static UINT32 float_to_unorm24_tox(UINT32 f)
{
    int exp = exponent(f) - 127;
    UINT32 mant = mantissa(f) | 1 <<23; // 24 bits

    // -Inf -> 0.0
    if (sign(f) == 1 && exp == (255-127) && mant == 0x800000)
        return 0;

    // +Inf -> 1.0
    if (!sign(f) && exp == (255-127) && mant == 0x800000)
        return 0xffffff;

    // very small -> zero
    if (exp<-25) // f<2^-25
        return 0;

    // large -> one
    if (exp>=0) // f>=1.0
        return 0xffffff;

    // one -> half+eps : decrement mantissa
    if ((exp==-1) && (mant!=0x800000)) // 0.5<f<1.0
        return --mant;

    // exactly half -> return 0x800000
    if ((exp==-1) && (mant==0x800000)) // f==0.5
        return 0x800000;

    // below half, exponent in -25..-2, conditionally increment shifted mantissa
    int shift = -exp-2; // 0..23
    UINT32 before_round = mant>>shift;
    if (before_round&1) // round up
        if (shift>2 || mant&((1<<shift)-1))
            return before_round/2+1;
    return before_round/2;
}

UINT32 float_to_unorm(UINT32 val, UINT32 ibits)
{
    // NaN -> zero
    if ((exponent(val) == 255) && (mantissa(val) != 0))
        return 0;

    // negative -> zero
    if (sign(val))
        return 0;

    // amodel implements colwersions specified by class
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        if ((exponent(val) > 127) || ((exponent(val) == 127) && mantissa(val)))
            return UINT32((UINT64(1) << ibits) - 1);
        if (val == 0x3F000000)
            return UINT32(1 << (ibits-1));
        float f = *(float *)&val;
        return UINT32(f * double((UINT64(1) << ibits)-1) + 0.5);
    }
    else
    {
        if (ibits == 2)
        {
            UINT32 u10 = float_to_unorm_a(val, 10, 4);
            return unorm10_to_unorm2(u10);
        }
        else if (ibits == 5)
        {
            UINT32 u8 = float_to_unorm_a(val, 8, 4);
            return u8 >> 3;
        }
        else if (ibits == 6)
        {
            UINT32 u8 = float_to_unorm_a(val, 8, 4);
            return u8 >> 2;
        }
        else if (ibits == 24)
        {
            return float_to_unorm24_tox(val);
        }
        else
        {
            return float_to_unorm_a(val, ibits, 4);
        }
    }
}

UINT32 float_to_snorm(UINT32 val, UINT32 ibits)
{
    if (sign(val))
    {
        val = -(INT32)float_to_unorm(val & 0x7FFFFFFF, ibits-1);
        val &= UINT32((UINT64(1) << ibits) - 1);
    }
    else
    {
        val = float_to_unorm(val, ibits-1);
    }
    return val;
}
