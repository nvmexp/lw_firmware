/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _RASTERTYPES_H
#define _RASTERTYPES_H

#include <assert.h>
#include <algorithm>
#include <string>
#include <string.h>
#include <map>
#include "types.h"
#include "core/include/color.h"

class GpuDevice;

typedef enum {
    RASTER_ERROR            = -1,           //to return an invalid format
    RASTER_X1R5G5B5         = 1,
    RASTER_R5G6B5           = 3,
    RASTER_A8R8G8B8         = 4,
    RASTER_X8R8G8B8         = 5,
    RASTER_Y8               = 6,
    RASTER_B8               = 14,           // blue channel only
    RASTER_Y16              = 16,
    RASTER_Y32              = 17,
    RASTER_F_W16Z16Y16X16   = 18,
    RASTER_F_W32Z32Y32X32   = 19,
    RASTER_A8B8G8R8         = 21,
    RASTER_X8B8G8R8         = 22,

    RASTER_LW50             = 77,

// additional non-Lwpu formats for ease of use with tga
    RASTER_VOID32           = 100,          // generic 32 bits/pixel format
    RASTER_R8G8B8           = 101,          // 24 bits/pixel
    RASTER_VOID16           = 102,          // generic 16 bits/pixel format
    RASTER_FORCELONG        = 0x7FFFFFFFL   // force this enum to be a long
} RasterFormat;

enum TextureMode
{
    TEXTURE_MODE_NORMAL,
    TEXTURE_MODE_STENCIL,
    TEXTURE_MODE_VCAA,
};

UINT32 Float32ToFloat16(float f);
UINT16 Float10ToFloat16(UINT16 x);
UINT16 Float11ToFloat16(UINT16 x);
UINT16 Float16ToFloat10(UINT16 x);
UINT16 Float16ToFloat11(UINT16 x);
float Float10ToFloat32(UINT16 x);
float Float11ToFloat32(UINT16 x);
UINT16 Float32ToFloat10(float f);
UINT16 Float32ToFloat11(float f);
UINT32 float_to_unorm(UINT32 val, UINT32 ibits);
UINT32 float_to_snorm(UINT32 val, UINT32 ibits);
UINT08 get_lin_srgb8_ulp06(UINT32);

template <int n>
class FloatArray
{
public:
    FloatArray()
    {
        std::fill(m_floats, m_floats+n, 0);
    }
    FloatArray(UINT32 *a)
    {
        std::copy(a, a+n, m_floats);
    }
    FloatArray(float *f)
    {
        assert(sizeof(UINT32) == sizeof(float));
        memcpy(m_floats, f, n*sizeof(float));
    }
    FloatArray(const FloatArray& that)
    {
        if (this != &that)
            std::copy(that.m_floats, that.m_floats+n, m_floats);
    }
    FloatArray& operator=(const FloatArray& that)
    {
        if (this != &that)
            std::copy(that.m_floats, that.m_floats+n, m_floats);
        return *this;
    }

    inline UINT32 getRaw(unsigned i) const
    {
        return m_floats[i];
    }

    inline void setRaw(unsigned i, UINT32 val)
    {
        m_floats[i] = val;
    }

    inline float getFloat32(unsigned i) const
    {
        return *(const float *)&m_floats[i];
    }

    inline UINT32 getUNorm(unsigned i, unsigned ibits) const
    {
        return float_to_unorm(m_floats[i], ibits);
    }

    inline UINT32 getSNorm(unsigned i, unsigned ibits) const
    {
        return float_to_snorm(m_floats[i], ibits);
    }

    inline UINT32 getUInt(unsigned i, unsigned ibits) const
    {
        if (m_floats[i] > ((UINT64(1) << ibits)-1))
            return (UINT64(1) << ibits)-1;
        else
            return m_floats[i];
    }

    inline UINT32 getSInt(unsigned i, unsigned ibits) const
    {
        // if a value is too large/small clamp it to maximum/minimum possible value
        if (sign(i))
            if (0x7fffffff - (m_floats[i] & 0x7fffffff) > ((UINT64(1) << (ibits - 1))-1))
                return UINT64(1) << (ibits - 1);
            else
                return (((UINT64(1) << (ibits - 1))-1) - (0x7fffffff - (m_floats[i] & 0x7fffffff))) |
                    (UINT64(1) << (ibits - 1));
        else
            if ((m_floats[i] & 0x7fffffff) > ((UINT64(1) << (ibits - 1))-1))
                return ((UINT64(1) << (ibits - 1))-1);
            else
                return (m_floats[i] & 0x7fffffff);
    }

    inline UINT16 getFloat16(unsigned i) const
    {
        return UINT16(Float32ToFloat16(getFloat32(i)));
    }

    // F11 is e5m6
    inline UINT16 getFloat11(unsigned i) const
    {
        return Float32ToFloat11(getFloat32(i));
    }

    // F10 is e5m5
    inline UINT16 getFloat10(unsigned i) const
    {
        return Float32ToFloat10(getFloat32(i));
    }

    //taken from //hw/lw5x/clib/lwshared/srgbutils.cpp
    inline UINT32 getsRGB8(unsigned i) const
    {
        UINT16 ifp16 = getFloat16(i);
        UINT08 isrgb;

        if (ifp16 & 0x8000) //negative number
            ifp16 = 0;

        if (ifp16 < 0x3c00)
            isrgb = get_lin_srgb8_ulp06(ifp16);
        else
            isrgb = 0xff;

        return isrgb;
    }

private:
    UINT16 getFloat(unsigned i, unsigned e, unsigned m) const
    {
        if (sign(i))                                            //no signed floats, clamping
            return 0;
        else if ((exponent(i) - 127) > (1 << (e-1)))            //overflow, clamping to max
        {
            return (1 << (e + m + 1)) - 1;
        }
        else if (abs(exponent(i) - 127) <= abs(1 << (e-1)))     //normalized form can be
        {                                                       //colwerted to normilized form
            return ((exponent(i) - 127 + (1 << (e-1))) << m) | (mantissa(i) >> (23 - m));
        }
        else if (unsigned(127 - (exponent(i))) > (e+m))         //see if it can be colwerted from
        {                                                       //normalized from to denormalized
            return (mantissa(i) & (1 << 24)) >> (23 - m);
        }
        else                                                    //clamping to 0
        {
            return 0;
        }
    }

    double NegBinE(UINT32 x) const {
        double temp(0x1 << x);
        return double(1.)/temp;
    }

    double getExponent(unsigned i) const {
        if (!exponent(i) && !mantissa(i))
            return 0;
        else if (exponent(i) >= 127)
            return double(0x1 << (exponent(i) - 127));
        else
            return NegBinE(127 - exponent(i));
    };

    double NegBinM(UINT32 x) const {
        double ret = 0;

        for (unsigned i = 0; (i < 8*sizeof(x)) || x; i++, x = x >> 1)
            if (x&0x1)
                    ret += double(1)/double(unsigned(0x1 << (23 - i)));
        return ret;
    }

    double getMantissa(unsigned i) const {
        if (!exponent(i) && !mantissa(i))
            return 0;
        else
            return 1 + NegBinM(mantissa(i));
    }

    inline UINT32 extract(unsigned i, unsigned pos, unsigned length) const {
        return (m_floats[i] >> pos) & ((0x1 << length) - 1);
    }
    inline bool sign(unsigned i) const{
        return extract(i, 31, 1) != 0;
    }
    inline UINT08 exponent(unsigned i) const{
        return extract(i, 23,8);
    }
    inline UINT32 mantissa(unsigned i) const{
        return extract(i, 0, 23);
    }

    template <class T>
    inline T abs(T x) const { return (x<0) ? -x : x;}

    UINT32 m_floats[n];
};

class Settable{
public:
    Settable() : m_set(false){}
    bool IsSet() const {return m_set;}
    void Set() {m_set = true;}

private:
    bool m_set;
};

class RGBAFloat : public Settable {
public:
    enum { NUM_COLORS = 4 };

    RGBAFloat() {}
    RGBAFloat(UINT32* c) : m_color(c) { Set(); }
    RGBAFloat(float* f) : m_color(f) { Set(); }

    UINT32 GetRRaw() const { return m_color.getRaw(0); }
    UINT32 GetGRaw() const { return m_color.getRaw(1); }
    UINT32 GetBRaw() const { return m_color.getRaw(2); }
    UINT32 GetARaw() const { return m_color.getRaw(3); }

    void SetRRaw(UINT32 v) { m_color.setRaw(0, v); }
    void SetGRaw(UINT32 v) { m_color.setRaw(1, v); }
    void SetBRaw(UINT32 v) { m_color.setRaw(2, v); }
    void SetARaw(UINT32 v) { m_color.setRaw(3, v); }

    //these four functions return raw bit representation of a color
    inline UINT32 GetRFloat32() const { return m_color.getRaw(0); }
    inline UINT32 GetGFloat32() const { return m_color.getRaw(1); }
    inline UINT32 GetBFloat32() const { return m_color.getRaw(2); }
    inline UINT32 GetAFloat32() const { return m_color.getRaw(3); }

    inline UINT16 GetRFloat16() const { return m_color.getFloat16(0); }
    inline UINT16 GetGFloat16() const { return m_color.getFloat16(1); }
    inline UINT16 GetBFloat16() const { return m_color.getFloat16(2); }
    inline UINT16 GetAFloat16() const { return m_color.getFloat16(3); }

    inline UINT16 GetRFloat11() const { return m_color.getFloat11(0); }
    inline UINT16 GetGFloat11() const { return m_color.getFloat11(1); }
    inline UINT16 GetBFloat11() const { return m_color.getFloat11(2); }
    inline UINT16 GetAFloat11() const { return m_color.getFloat11(3); }

    inline UINT16 GetRFloat10() const { return m_color.getFloat10(0); }
    inline UINT16 GetGFloat10() const { return m_color.getFloat10(1); }
    inline UINT16 GetBFloat10() const { return m_color.getFloat10(2); }
    inline UINT16 GetAFloat10() const { return m_color.getFloat10(3); }

    inline UINT32 GetRUNorm(unsigned ibits) const { return m_color.getUNorm(0, ibits); }
    inline UINT32 GetGUNorm(unsigned ibits) const { return m_color.getUNorm(1, ibits); }
    inline UINT32 GetBUNorm(unsigned ibits) const { return m_color.getUNorm(2, ibits); }
    inline UINT32 GetAUNorm(unsigned ibits) const { return m_color.getUNorm(3, ibits); }

    inline UINT32 GetRSNorm(unsigned ibits) const { return m_color.getSNorm(0, ibits); }
    inline UINT32 GetGSNorm(unsigned ibits) const { return m_color.getSNorm(1, ibits); }
    inline UINT32 GetBSNorm(unsigned ibits) const { return m_color.getSNorm(2, ibits); }
    inline UINT32 GetASNorm(unsigned ibits) const { return m_color.getSNorm(3, ibits); }

    inline UINT32 GetRUInt(unsigned ibits) const { return m_color.getUInt(0, ibits); }
    inline UINT32 GetGUInt(unsigned ibits) const { return m_color.getUInt(1, ibits); }
    inline UINT32 GetBUInt(unsigned ibits) const { return m_color.getUInt(2, ibits); }
    inline UINT32 GetAUInt(unsigned ibits) const { return m_color.getUInt(3, ibits); }

    inline UINT32 GetRSInt(unsigned ibits) const { return m_color.getSInt(0, ibits); }
    inline UINT32 GetGSInt(unsigned ibits) const { return m_color.getSInt(1, ibits); }
    inline UINT32 GetBSInt(unsigned ibits) const { return m_color.getSInt(2, ibits); }
    inline UINT32 GetASInt(unsigned ibits) const { return m_color.getSInt(3, ibits); }

    inline UINT32 GetRsRGB8() const { return m_color.getsRGB8(0); }
    inline UINT32 GetGsRGB8() const { return m_color.getsRGB8(1); }
    inline UINT32 GetBsRGB8() const { return m_color.getsRGB8(2); }
    inline UINT32 GetAsRGB8() const { return m_color.getsRGB8(3); }

    inline float GetRFloat32f() const { return m_color.getFloat32(0); }
    inline float GetGFloat32f() const { return m_color.getFloat32(1); }
    inline float GetBFloat32f() const { return m_color.getFloat32(2); }
    inline float GetAFloat32f() const { return m_color.getFloat32(3); }

private:
    //RGBAFloat(const RGBAFloat &); - default is fine
    //RGBAFloat& operator=(const RGBAFloat&); - default is fine

    FloatArray<NUM_COLORS> m_color;
};

class ZFloat : public Settable {
public:
    ZFloat() {}
    ZFloat(UINT32 z) : m_ZFloat(&z) { Set(); }

    UINT32 GetRaw() const { return m_ZFloat.getRaw(0); }
    void SetRaw(UINT32 v) { m_ZFloat.setRaw(0, v); }

    inline UINT32 GetFloat() const { return m_ZFloat.getRaw(0); }
    inline UINT32 GetUNorm(unsigned ibits) const {return m_ZFloat.getUNorm(0, ibits);}

private:
    //ZFloat(const ZFloat&); - default is fine
    //ZFloat operator=(const ZFloat&); - default is fine

    FloatArray<1> m_ZFloat;
};

class Stencil : public Settable
{
public:
    Stencil():m_Stencil(0) {};
    Stencil(UINT32 s):m_Stencil((UINT08)s) { Set(); }

    inline UINT32 Get() const { return m_Stencil; }

private:
    UINT08 m_Stencil;
};

/*------------------------------------------------------------------
 * COLOR RASTERS
 *------------------------------------------------------------------
 */

// PixelFB represents pixel in FB
class PixelFB
{
    public:
        PixelFB() : m_red(0), m_green(0), m_blue(0), m_alpha(0) {}
        PixelFB(UINT32 r, UINT32 g, UINT32 b, UINT32 a) :
            m_red(r), m_green(g), m_blue(b), m_alpha(a) {}

        UINT32 Red()   const {return m_red;}
        UINT32 Green() const {return m_green;}
        UINT32 Blue()  const {return m_blue;}
        UINT32 Alpha() const {return m_alpha;}
        // map Z->Red, Stencil->Green, and VCAA->Blue
        UINT32 Z()       const {return m_red;}
        UINT32 Stencil() const {return m_green;}
        UINT32 VCAA()    const {return m_blue;}

        float RedFloat32()   const {return *(float const*)&m_red;}
        float GreenFloat32() const {return *(float const*)&m_green;}
        float BlueFloat32()  const {return *(float const*)&m_blue;}
        float AlphaFloat32() const {return *(float const*)&m_alpha;}
        float ZFloat32()     const {return *(float const*)&m_red;}

        bool operator==(const PixelFB& that) const
        {
            return m_red   == that.m_red &&
                   m_green == that.m_green &&
                   m_blue  == that.m_blue &&
                   m_alpha == that.m_alpha;
        }

        //this allows to use Pixel as a key in a histogram
        bool operator<(const PixelFB& that) const
        {
            if (m_red == that.m_red)
                if (m_green == that.m_green)
                    if (m_blue == that.m_blue)
                        return m_alpha < that.m_alpha;
                    else
                        return m_blue < that.m_blue;
                else
                    return m_green < that.m_green;
            else
                return m_red < that.m_red;
        }

    private:
        UINT32 m_red, m_green, m_blue, m_alpha;
};

class LW50Raster
{
public:
    LW50Raster(ColorUtils::Format surfFormat, UINT32 devFormat, unsigned int bpp, unsigned int tgaBpp);
    virtual ~LW50Raster();

    virtual ColorUtils::Format GetSurfaceFormat() const { return m_surfFormat; }
    virtual unsigned int GetBytesPerPixel() const { return m_bpp; }
    virtual UINT32 GetDeviceFormat() const { return m_devFormat; }
    virtual int TgaPixelSize() const { return m_tgaBpp; }

    virtual void ToTgaExt(unsigned char *PixelSize, unsigned char *Desc) const;
    virtual void ColwertToTga(UINT08 *to, UINT08 *from, UINT32 size) const;
    virtual void FillLinearGob
    (
        UINT08 *buff,
        const RGBAFloat &color,
        const ZFloat &z,
        const Stencil &s,
        bool SrgbWrite,
        UINT32 GobSize
    ) = 0;

    virtual UINT32 Granularity() const = 0;
    virtual bool ZFormat() const = 0;
    virtual bool GetZFloat() const { return false; }
    virtual UINT32 GetZBits() const { return 0; }
    virtual UINT32 GetStencilBits() const { return 0; }
    virtual bool SupportsSrgbWrite() const { return false; }
    virtual bool IsSrgb() const { return false; }
    virtual UINT32 GetTextureHeaderDword0(UINT32 AAMode, TextureMode Mode,
                                          GpuDevice *pGpuDevice) const = 0;

    virtual void SetMaxwellTextureHeaderFormat(UINT32* state, UINT32 AAMode,
        TextureMode Mode) const = 0;
    virtual void SetHopperTextureHeaderFormat(UINT32* state, UINT32 AAMode,
        TextureMode Mode) const {};

    virtual void SetBase(uintptr_t base) { m_image_base = base; }

    virtual bool IsBlack(UINT32 n) const = 0;
    virtual bool ComparePixels(UINT32 n1, UINT32 n2, const FloatArray<1>& threshold) const = 0;
    virtual bool ComparePixelsRGBA(UINT32 n, const RGBAFloat&, const FloatArray<1>& threshold) const = 0;
    virtual PixelFB GetPixel(UINT32 n) const = 0;

protected:
    inline unsigned ImageOffset2GobOffset(unsigned offset, unsigned width, unsigned gw, unsigned gh) const;

    inline unsigned Exp2(unsigned x) const { return (1 << x); }
    inline unsigned Mask(unsigned x) const { return Exp2(x) - 1; }
    inline unsigned get_bit(unsigned x, unsigned n, unsigned m) const {
        return (((x >> n) & 0x1) << m);
    }

    ColorUtils::Format m_surfFormat;
    UINT32 m_devFormat;
    unsigned int m_bpp, m_tgaBpp;
    uintptr_t m_image_base;
};

map<const string, LW50Raster*> CreateColorMap();
map<const string, LW50Raster*> CreateZetaMap();

//! \brief deallocate a map obtained with CreateColorMap or CreateZetaMap
void DeleteMap(map<const string, LW50Raster*>* map_);

//a holder to cary two formats around
//for now lwrie part uses CTRaster.m_LwrieRaster, tesla part uses CTRaster.m_TeslaRaster
//ideally CTRaster should be removed and LW50Raster* should be used everywhere
struct CTRaster
{
    RasterFormat m_LwrieRaster;
    LW50Raster* m_TeslaRaster;
    CTRaster(RasterFormat c, LW50Raster* t) : m_LwrieRaster(c), m_TeslaRaster(t) {}
};

#endif //_RASTERTYPES_H
