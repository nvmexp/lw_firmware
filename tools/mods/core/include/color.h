/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013,2015-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_COLOR_H
#define INCLUDED_COLOR_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

#include "core/include/rc.h"

namespace ColorUtils
{
    // Color formats
    enum Format
    {
        LWFMT_NONE,             // placeholder
        R5G6B5,                 // 16 bit rgb
        A8R8G8B8,               // 32 bit rgb (actually 24-bit, with transparency in first 8 bits)
        R8G8B8A8,               // 32 bit rgb (actually 24-bit, with transparency in last 8 bits)
        A2R10G10B10,            // 32 bit rgb (actually 30-bit, with transparency in first 2 bits)
        R10G10B10A2,            // 32 bit rgb (actually 30-bit, with transparency in last 2 bits)
        CR8YB8CB8YA8,           // color-compressed video format
        YB8CR8YA8CB8,           // color-compressed video format
        Z1R5G5B5,               // 15 bit rgb
        Z24S8,                  // 32-bit Zeta buffer: 24 Z, 8 stencil
        Z16,                    // 16-bit Zeta buffer: 16 Z, 0 stencil
        RF16,                   // 16-bit float, red only
        RF32,                   // 32-bit float, red only
        RF16_GF16_BF16_AF16,    // 16-bit float, all 4 elements
        RF32_GF32_BF32_AF32,    // 32-bit float, all 4 elements
        Y8,                     // 8-bit unsigned brightness (mono)
        B8_G8_R8,               // 24-bit color
        Z24,                    // 24-bit Z
        I8,
        VOID8,
        VOID16,
        A2V10Y10U10,
        A2U10Y10V10,
        VE8YO8UE8YE8,
        UE8YO8VE8YE8,
        YO8VE8YE8UE8,
        YO8UE8YE8VE8,
        YE16_UE16_YO16_VE16,
        YE10Z6_UE10Z6_YO10Z6_VE10Z6,
        UE16_YE16_VE16_YO16,
        UE10Z6_YE10Z6_VE10Z6_YO10Z6,
        VOID32,
        CPST8,
        CPSTY8CPSTC8,
        AUDIOL16_AUDIOR16,
        AUDIOL32_AUDIOR32,
        A2B10G10R10,
        A8B8G8R8,
        A1R5G5B5,
        Z8R8G8B8,
        Z32,                    // 32-bit Z
        X8R8G8B8,
        X1R5G5B5,
        AN8BN8GN8RN8,
        AS8BS8GS8RS8,
        AU8BU8GU8RU8,
        X8B8G8R8,
        A8RL8GL8BL8,
        X8RL8GL8BL8,
        A8BL8GL8RL8,
        X8BL8GL8RL8,
        RF32_GF32_BF32_X32,
        RS32_GS32_BS32_AS32,
        RS32_GS32_BS32_X32,
        RU32_GU32_BU32_AU32,
        RU32_GU32_BU32_X32,
        R16_G16_B16_A16,
        RN16_GN16_BN16_AN16,
        RU16_GU16_BU16_AU16,
        RS16_GS16_BS16_AS16,
        RF16_GF16_BF16_X16,
        RF32_GF32,
        RS32_GS32,
        RU32_GU32,
        RS32,
        RU32,
        AU2BU10GU10RU10,
        RF16_GF16,
        RS16_GS16,
        RN16_GN16,
        RU16_GU16,
        R16_G16,
        BF10GF11RF11,
        G8R8,
        GN8RN8,
        GS8RS8,
        GU8RU8,
        R16,
        RN16,
        RS16,
        RU16,
        R8,
        RN8,
        RS8,
        RU8,
        A8,
        ZF32,
        S8Z24,
        X8Z24,
        V8Z24,
        ZF32_X24S8,
        X8Z24_X16V8S8,
        ZF32_X16V8X8,
        ZF32_X16V8S8,
        S8,

        X2BL10GL10RL10_XRBIAS,
        R16_G16_B16_A16_LWBIAS,
        X2BL10GL10RL10_XVYCC,

        // YUV color formats
        Y8_U8__Y8_V8_N422,
        U8_Y8__V8_Y8_N422,
        Y8___U8V8_N444,
        Y8___U8V8_N422,
        Y8___U8V8_N422R,
        Y8___V8U8_N420,
        Y8___U8___V8_N444,
        Y8___U8___V8_N420,
        Y10___U10V10_N444,
        Y10___U10V10_N422,
        Y10___U10V10_N422R,
        Y10___V10U10_N420,
        Y10___U10___V10_N444,
        Y10___U10___V10_N420,
        Y12___U12V12_N444,
        Y12___U12V12_N422,
        Y12___U12V12_N422R,
        Y12___V12U12_N420,
        Y12___U12___V12_N444,
        Y12___U12___V12_N420,

        // CheetAh
        A8Y8U8V8,
        I1,
        I2,
        I4,
        A4R4G4B4,
        R4G4B4A4,
        B4G4R4A4,
        R5G5B5A1,
        B5G5R5A1,
        A8R6x2G6x2B6x2,
        A8B6x2G6x2R6x2,
        X1B5G5R5,
        B5G5R5X1,
        R5G5B5X1,
        U8,     // Used for planar YUV
        V8,     // Used for planar YUV
        CR8,    // Used for planar YCrCb
        CB8,    // Used for planar YCrCb
        U8V8,   // Used for semi-planar YUV
        V8U8,   // Used for semi-planar YVU
        CR8CB8, // Used for planar YCrCb
        CB8CR8, // Used for planar YCbCr

        // Below formats are used for textures
        R32_G32_B32_A32,
        R32_G32_B32,
        R32_G32,
        R32_B24G8,
        G8R24,
        G24R8,
        R32,
        A4B4G4R4,
        A5B5G5R1,
        A1B5G5R5,
        B5G6R5,
        B6G5R5,
        Y8_VIDEO,
        G4R4,
        R1,
        E5B9G9R9_SHAREDEXP,
        G8B8G8R8,
        B8G8R8G8,
        DXT1,
        DXT23,
        DXT45,
        DXN1,
        DXN2,
        BC6H_SF16,
        BC6H_UF16,
        BC7U,
        X4V4Z24__COV4R4V,
        X4V4Z24__COV8R8V,
        V8Z24__COV4R12V,
        X8Z24_X20V4S8__COV4R4V,
        X8Z24_X20V4S8__COV8R8V,
        ZF32_X20V4X8__COV4R4V,
        ZF32_X20V4X8__COV8R8V,
        ZF32_X20V4S8__COV4R4V,
        ZF32_X20V4S8__COV8R8V,
        X8Z24_X16V8S8__COV4R12V,
        ZF32_X16V8X8__COV4R12V,
        ZF32_X16V8S8__COV4R12V,
        V8Z24__COV8R24V,
        X8Z24_X16V8S8__COV8R24V,
        ZF32_X16V8X8__COV8R24V,
        ZF32_X16V8S8__COV8R24V,
        B8G8R8A8,
        ASTC_2D_4X4,
        ASTC_2D_5X4,
        ASTC_2D_5X5,
        ASTC_2D_6X5,
        ASTC_2D_6X6,
        ASTC_2D_8X5,
        ASTC_2D_8X6,
        ASTC_2D_8X8,
        ASTC_2D_10X5,
        ASTC_2D_10X6,
        ASTC_2D_10X8,
        ASTC_2D_10X10,
        ASTC_2D_12X10,
        ASTC_2D_12X12,
        ASTC_SRGB_2D_4X4,
        ASTC_SRGB_2D_5X4,
        ASTC_SRGB_2D_5X5,
        ASTC_SRGB_2D_6X5,
        ASTC_SRGB_2D_6X6,
        ASTC_SRGB_2D_8X5,
        ASTC_SRGB_2D_8X6,
        ASTC_SRGB_2D_8X8,
        ASTC_SRGB_2D_10X5,
        ASTC_SRGB_2D_10X6,
        ASTC_SRGB_2D_10X8,
        ASTC_SRGB_2D_10X10,
        ASTC_SRGB_2D_12X10,
        ASTC_SRGB_2D_12X12,

        ETC2_RGB,
        ETC2_RGB_PTA,
        ETC2_RGBA,
        EAC,
        EACX2,
        Format_NUM_FORMATS      // last enum value
    };
    enum Element
    {
        elRed
        ,elGreen
        ,elBlue
        ,elAlpha
        ,elDepth
        ,elStencil
        ,elOther
        ,elIgnored
        ,elNone

        ,elNUM_ELEMENT_TYPES
        ,elNUM_USED_ELEMENT_TYPES = elOther+1
    };
    enum
    {
        MaxElementsPerFormat = 4
    };

    // PNG file color formats
    // These specify the bit widths of components but not what the components represent.
    //
    // Components are always arranged in the following standard order (when applicable):
    //    (R,  G,  B,  A)
    //    (R,  G,  B,  Z)
    //    (YB, CR, YA, CB)
    //    (--- Z ---,  S)   (zeta, stencil)
    //
    // Note that C10C10C10C2 is not a standard PNG color format but is used internally
    // and packed into 4 bytes of the PNG file.
    enum PNGFormat
    {
        LWPNGFMT_NONE,    // placeholder
        C1,               // 1 (1-bit) component/pixel
        C2,               // 1 (2-bit) component/pixel
        C4,               // 1 (4-bit) component/pixel
        C8,               // 1 (8-bit) component/pixel
        C16,              // 1 (16-bit) component/pixel
        C8C8,             // 2 (8-bit) components/pixel
        C16C16,           // 2 (16-bit) components/pixel
        C8C8C8,           // 3 (8-bit) components/pixel
        C16C16C16,        // 3 (16-bit) components/pixel
        C8C8C8C8,         // 4 (8-bit) components/pixel
        C10C10C10C2,      // 4 components/pixel (10, 10, 10, and 2 bits each)
        C16C16C16C16,     // 4 (16-bit) components/pixel
        C32C32C32C32,     // 4 (32-bit) components/pixel
        C10C10C10,        // 3 (10-bit) components/pixel
        C12C12C12         // 3 (12-bit) components/pixel

    };

    enum YCrCbType
    {
        YCRCBTYPE_NONE,
        CCIR601,
        CCIR709,
        CCIR2020
    };

    // Return string containing color format name.
    string FormatToString(Format fmt);
    string PNGFormatToString(PNGFormat fmt);
    const char* ElementToString(int el);

    // Return true if format is out of valid range, false otherwise.
    bool FormatIsOutOfRange (ColorUtils::Format fmt);

    UINT32 WordSize(ColorUtils::Format fmt);
    bool IsFloat(ColorUtils::Format fmt);
    bool IsRgb(ColorUtils::Format fmt);
    bool IsYuv(ColorUtils::Format fmt);
    bool IsDepth(ColorUtils::Format fmt);
    bool IsSigned(ColorUtils::Format fmt);
    bool IsNormalized(ColorUtils::Format fmt);
    bool IsYuvSemiPlanar(ColorUtils::Format fmt);
    bool IsYuvPlanar(ColorUtils::Format fmt);
    UINT32 GetYuvBpc(ColorUtils::Format fmt);

    // Return color format corresponding to string.
    Format StringToFormat(const string& str);
    PNGFormat StringToPNGFormat(const string& str);
    YCrCbType StringToCrCbType(const string& str);

    // Colwert various other ways of specifying a color format
    // to these mods-specific enum values.
    Format ColorDepthToFormat (UINT32 ColorDepth);
    Format ZDepthToFormat     (UINT32 ZDepth);

    // Return number of bytes per pixel
    UINT32 PixelBytes(Format fmt);
    UINT32 PixelBytes(PNGFormat fmt);

    // Returns number of bytes per pixel in .tga file
    UINT32 TgaPixelBytes(Format fmt);

    // Return number of bits per pixel
    UINT32 PixelBits(Format fmt);
    UINT32 PixelBits(PNGFormat fmt);

    // Return number of bits per component for RGBA (non-YUV) formats.
    void CompBitsRGBA (Format fmt,
                       UINT32 *pRedCompBits,
                       UINT32 *pGreenCompBits,
                       UINT32 *pBlueCompBits,
                       UINT32 *pAlphaCompBits);

    // Returns number of Z bits of a Z-buffer format
    UINT32 ZBits(Format fmt);

    // Returns number of stencil bits of a Z-buffer format
    UINT32 StencilBits(Format fmt);

    // Return number of elements (i.e. r,g,b,a,s,d) per pixel.
    UINT32 PixelElements(Format fmt);
    UINT32 PixelElements(PNGFormat fmt);

    // Get the element ordering, assuming little-endian.
    Element ElementAt(Format fmt, int idx);

    // Return whether two formats are in the same colorspace.
    // (i.e. both integer rgb, both float rgb)  Formats in the
    // same colorspace can be colwerted to/from each other
    // using only bit-shift operations.
    bool IsCompatibleColorSpace(Format fmt0, Format fmt1);

    // Return the PNG file format that should be
    // used to store data of the given color format.
    PNGFormat FormatToPNGFormat(Format fmt);

    // Fill all channels with the alpha channel value.
    // This does nothing unless fmt has R,G,B, and A channels.
    void CopyAlphaToRgb(char* pixel, Format fmt);

    // Encode color components into the pixel value with the specified format.
    UINT64 EncodePixel
    (
        Format pixelFormat,
        UINT32 redCr,
        UINT32 greenYo,
        UINT32 blueCb,
        UINT32 alphaYe
    );

    // Colwert one pixel from one color format to another.
    UINT32 Colwert(UINT32 in, Format from, Format to);

    // Colwert one pixel from a PNG file color format.
    void Colwert
    (
        const char * in,
        PNGFormat    from,
        char *       out,
        Format       to
    );

    // Colwert one pixel to a PNG file color format.
    void Colwert
    (
        const char * in,
        Format       from,
        char *       out,
        PNGFormat    to
    );

    // Colwert from one color format to another.
    // Returns number of pixels written to DstBuf.
    UINT32 Colwert
    (
        const char * SrcBuf,
        Format       SrcFmt,
        char *       DstBuf,
        Format       DstFmt,
        UINT32       NumPixels
    );

    // Colwert pixels from a PNG file color format.
    // Returns number of pixels written to DstBuf.
    UINT32 Colwert
    (
        const char * SrcBuf,
        PNGFormat    SrcFmt,
        char *       DstBuf,
        Format       DstFmt,
        UINT32       NumPixels
    );

    // Colwert pixels to a PNG file color format.
    // Returns number of pixels written to DstBuf.
    UINT32 Colwert
    (
        const char * SrcBuf,
        Format       SrcFmt,
        char *       DstBuf,
        PNGFormat    DstFmt,
        UINT32       NumPixels
    );

    // Colwert pixels to TGA file format
    void ColwertToTga( const char * SrcBuf,
                       char * DstBuf,
                       ColorUtils::Format fmt,
                       UINT32 pixels);

    // Colwert pixels to YCrCb formats
    UINT32 YCrCbColwert( const char* SrcBuf,
                         Format      SrcFmt,
                         char*       DstBuf,
                         Format      DstFmt,
                         UINT32      NumPixels,
                         YCrCbType   ycrcb_type);

    UINT32 ColwertYUV422ToYUV444(UINT08* inbuf, UINT08* outbuf,
                                 UINT32 NumInPixels, ColorUtils::Format Format);
    UINT32 ColwertYUV444ToYUV422(UINT08* inbuf, UINT08* outbuf,
                                 UINT32 NumInPixels, ColorUtils::Format Format);

   //! Enable/disable pre-GF11x workaround for scaling FP16 colors.
   //!
   //! By default we assume pre-GF11x case and divide by 0x3f.
   //!
   //! For GF11x and later, this should be set false, in which case we will
   //! properly divide by 65536 instead.
   //!
   void SetFP16DivideWorkaround (bool enable);
   bool GetFP16DivideWorkaround ();

   //! Returns 0x3f (63) for the pre-gf11x case (scale-by-1024-workaround),
   //! or 65536 for newer chips that don't need the hack.
   UINT32 GetFP16Divide ();

   RC TransformPixels
   (
       const char *srcBuf,
       char *dstBuf,
       UINT32 numPixels,
       Format inputFmt,
       Format outputFmt,
       YCrCbType outputCS,
       Format planeFormat,
       UINT32 classCode
   );
} // end of namespace Color

#endif  // INCLUDED_COLOR_H
