/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _BUF_H
#define _BUF_H

#include <stdio.h>
#include "types.h"
#include "mdiag/sysspec.h"

#ifndef INCLUDED_STL_MEMORY
#include <memory>
#define INCLUDED_STL_MEMORY
#endif

#include "raster_types.h"

/* Header signatutes for various resources */
#define BFT_ICON   0x4349   /* 'IC' */
#define BFT_BITMAP 0x4d42   /* 'BM' */
#define BFT_LWRSOR 0x5450   /* 'PT' */

/* constants for the biCompression field */
#define BI_RGB        0L
#define BI_RLE8       1L
#define BI_RLE4       2L
#define BI_BITFIELDS  3L

#define SWAPULONG(i)    (i+0)
#define SWAPUSHORT(i)    (i+0)

#include <stdlib.h>

/* Header definition. */
typedef struct TGA_Header_  {
    unsigned char ImageIDLength;        /* length of Identifier String. */
    unsigned char CoMapType;            /* 0 = no map */
    unsigned char ImgType;              /* image type (see below for values) */
    unsigned char Index_lo, Index_hi;   /* index of first color map entry */
    unsigned char Length_lo, Length_hi; /* number of entries in color map */
    unsigned char CoSize;               /* size of color map entry (15,16,24,32) */
    unsigned char X_org_lo, X_org_hi;   /* x origin of image */
    unsigned char Y_org_lo, Y_org_hi;   /* y origin of image */
    unsigned char Width_lo, Width_hi;   /* width of image */
    unsigned char Height_lo, Height_hi; /* height of image */
    unsigned char PixelSize;            /* pixel size (8,16,24,32) */
    unsigned char Desc;                 /* 4 bits, number of attribute bits per pixel */
} TGA_Header;

/* if we create the tga file, we will put the following in the ImageID field */
typedef struct TGA_ImageId_ {
    unsigned char Magic;                /* must be a specific magic value */
    unsigned char ImageFmt;             /* encodes special raster formats */
} TGA_ImageId;

/* Definitions for image types. */
#define TGA_NULL 0
#define TGA_MAP 1
#define TGA_RGB 2
#define TGA_MONO 3
#define TGA_RLEMAP 9
#define TGA_RLERGB 10
#define TGA_RLEMONO 11
#define TGA_RLE 8

#define TGA_DESC_ALPHA_MASK     ((unsigned char)0xF)    /* number of alpha channel bits */
#define TGA_DESC_ORG_MASK       ((unsigned char)0x30)   /* origin mask */
#define TGA_ORG_BOTTOM_LEFT     0x00
#define TGA_ORG_BOTTOM_RIGHT    0x10
#define TGA_ORG_TOP_LEFT        0x20
#define TGA_ORG_TOP_RIGHT       0x30

#define TGA_LWIDIA_MAGIC        0xAF            /* not an ascii char so unlikely... */

#define SIZEOF_BITMAPFILEHEADER_PACKED  (   \
    sizeof(UINT16) +      /* bfType      */   \
    sizeof(UINT32) +     /* bfSize      */   \
    sizeof(UINT16) +      /* bfReserved1 */   \
    sizeof(UINT16) +      /* bfReserved2 */   \
    sizeof(UINT32))      /* bfOffBits   */

class BufferConfig{
public:
    virtual CTRaster TgaFormat(const CTRaster&) const = 0;
    virtual void ToTgaExt(const CTRaster&, unsigned char *, unsigned char *) const = 0;
    virtual int RasSizeofFormat(const CTRaster& format) const = 0;
    virtual int TgaPixelSize(const CTRaster& format) const = 0;
    virtual RasterFormat RasterFmt(UINT32) const = 0;
    virtual void ColwertToTga(const CTRaster&, UINT08*,UINT08*, UINT32) const = 0;
};

class LwrieBufferConfig : public BufferConfig {
    public:
    LwrieBufferConfig() {}
    CTRaster TgaFormat(const CTRaster&) const;
    void ToTgaExt(const CTRaster&, unsigned char *, unsigned char *) const;
    int RasSizeofFormat(const CTRaster& format) const;
    int TgaPixelSize(const CTRaster& format) const;
    RasterFormat RasterFmt(UINT32) const;
    void ColwertToTga(const CTRaster&, UINT08*,UINT08*, UINT32) const;
};

class TeslaBufferConfig : public BufferConfig {
public:
    TeslaBufferConfig() {}
    CTRaster TgaFormat(const CTRaster&) const;
    void ToTgaExt(const CTRaster&, unsigned char *, unsigned char *) const;
    int RasSizeofFormat(const CTRaster& format) const;
    int TgaPixelSize(const CTRaster& format) const;
    RasterFormat RasterFmt(UINT32) const;
    void ColwertToTga(const CTRaster&, UINT08*,UINT08*, UINT32) const;
};

class BlockLinear;

class Buf {

protected:

    typedef struct tagRGBQUAD {
        UINT08 rgbBlue;
        UINT08 rgbGreen;
        UINT08 rgbRed;
        UINT08 rgbReserved;
    } RGBQUAD, * LPRGBQUAD;

    typedef struct tagBITMAPINFOHEADER{
        UINT32 biSize;
        int    biWidth;
        int    biHeight;
        UINT16 biPlanes;
        UINT16 biBitCount;
        UINT32 biCompression;
        UINT32 biSizeImage;
        int    biXPelsPerMeter;
        int    biYPelsPerMeter;
        UINT32 biClrUsed;
        UINT32 biClrImportant;
    } BITMAPINFOHEADER, *LPBITMAPINFOHEADER, *PBITMAPINFOHEADER;

    typedef struct tagBITMAPFILEHEADER {
        UINT16 bfType;
        UINT32 bfSize;
        UINT32 bfReserved;
        UINT32 bfOffBits;
    } BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;

    void FetchBufLine(
        UINT32 Line,
        UINT32 Width,           /* width of image */
        uintptr_t Offset,
        UINT32 Pitch,
        RasterFormat memFormat, /* memory format */
        void *Data);

    void FetchBufData(
        UINT32 Width,           /* width of image */
        UINT32 Height,          /* height of image */
        uintptr_t Offset,
        UINT32 Pitch,
        UINT32 depth,
        void *Data);

    int Buf2Dib(
          UINT08 *data                  /* frame buffer data */
        , unsigned long Width           /* width of image */
        , unsigned long Height          /* height of image */
        , RasterFormat memFormat        /* memory format */
        , int  BPP                      /* bits-per-pixel of the DIB bitmap */
        , vector<UINT08>* pBitMap       /* DIB bitmap */
        , unsigned int *scanlineSize    /* rounded up to 4-byte multiple */
        , int bigEndian                 /* is buffer big endian? */
        );

    int dibSave
    (
        const char    *filename,
        UINT08        *pBits,
        unsigned long  Width,
        unsigned long  Height,
        unsigned long  scanlineSize,
        unsigned long  BPP);

public:
    Buf();

    void SetConfigBuffer(BufferConfig*);
    int rasSizeofFormat(RasterFormat format);

    int SaveBufTGA(const char *filename, UINT32 Width, UINT32 Height,
                   uintptr_t Offset, UINT32 Pitch, UINT32 depth,
                   const CTRaster& format, int shuffled, const char *key = NULL);
    //for old tests
    int SaveBufTGA(const char *filename, UINT32 Width, UINT32 Height,
                   uintptr_t Offset, UINT32 Pitch, UINT32 depth,
                   RasterFormat format, int shuffled, const char *key = NULL);

    RC CopyBlockLinearBuf(uintptr_t Src, uintptr_t SrcS, void *Dst, UINT32 MaxSize,
                          UINT32 BaseX, UINT32 BaseY, UINT32 BaseZ,
                          UINT32 Width, UINT32 Height, UINT32 Depth, UINT32 ArraySize, UINT32 ArrayPitch,
                          const BlockLinear *bl, UINT64 Pitch);

    void CopyBuf(uintptr_t Src, uintptr_t SrcS, void *Dst,
                 UINT32 BaseX, UINT32 BaseY, UINT32 BaseZ,
                 UINT32 Width, UINT32 Height, UINT32 Depth, UINT32 ArraySize, UINT32 ArrayPitch,
                 UINT32 BytesPerPixel, UINT64 Pitch, UINT32 LayerPitch);
private:
    unique_ptr<BufferConfig> m_buffConfig;
    bool GZipImage(const string& filename) const;
protected:

    struct FormatInfo
    {
        FormatInfo(bool isZ, UINT08 aformat, UINT32 slen) :
            magic(0xaeaeaeae),
            type_a(isZ ? 1 : 0), format_a(aformat),
            format_b(aformat), type_b(isZ ? 1 : 0),
            strlength1(UINT08(slen & 0xFF)),
            strlength2(UINT08((slen >> 8) & 0xFF)),
            strlength3(UINT08((slen >> 16) & 0xFF)),
            strlength4(UINT08((slen >> 24) & 0xFF)) {}
        // Preceeded by strlength bytes of pascal style string.
        UINT32 magic;
        UINT08 type_a;
        UINT08 format_a;
        UINT08 format_b; //making sure that result does not depend on endianess
        UINT08 type_b;
        UINT08 strlength1; // also to make sure result doesn't depend on endianess
        UINT08 strlength2;
        UINT08 strlength3;
        UINT08 strlength4;
    };
};

#endif
