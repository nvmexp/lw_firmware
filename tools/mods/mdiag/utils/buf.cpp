/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <stdlib.h>
#include <string.h>
#include "core/include/massert.h"
#include "buf.h"
#include "gpu/utility/blocklin.h"
#include "core/include/platform.h"
#include "core/include/pool.h"
#include "core/include/xp.h"
#include "core/include/zlib.h"

static void c565to888(UINT08 *src, UINT08 *dst, UINT32 npixels);
static void y16to888(UINT08 *src, UINT08 *dst, UINT32 npixels);
static void y32to888(UINT08 *src, UINT08 *dst, UINT32 npixels);
static void ca8b8g8r8toa8r8g8b8(unsigned char *src, unsigned char *dst, int npixels);

namespace ImageFile
{
    void RLEncode(UINT08 *inB, UINT32 iSize, UINT32 colorByteSize,
            vector<UINT08> *outBp, UINT32 *oSizep);
}

Buf::Buf() : m_buffConfig(new LwrieBufferConfig()) //temp hack
{
}

void Buf::SetConfigBuffer(BufferConfig* config) {
    m_buffConfig.reset(config);
}

// colwert format to bytes/pixel
// returns 0 on invalid format
int Buf::rasSizeofFormat(RasterFormat format) {
    return m_buffConfig->RasSizeofFormat(CTRaster(format, 0));
}

int LwrieBufferConfig::RasSizeofFormat(const CTRaster& format) const
{
    switch (format.m_LwrieRaster) {
        case RASTER_A8B8G8R8:       return 4;
        case RASTER_A8R8G8B8:       return 4;
        case RASTER_B8:             return 1;
        case RASTER_F_W16Z16Y16X16: return 8;
        case RASTER_F_W32Z32Y32X32: return 16;
        case RASTER_R5G6B5:         return 2;
        case RASTER_R8G8B8:         return 3;
        case RASTER_VOID16:         return 2;
        case RASTER_VOID32:         return 4;
        case RASTER_X1R5G5B5:       return 2;
        case RASTER_X8B8G8R8:       return 4;
        case RASTER_X8R8G8B8:       return 4;
        case RASTER_Y16:            return 2;
        case RASTER_Y32:            return 4;
        case RASTER_Y8:             return 1;
        default:                    return 0;
    }
}

int TeslaBufferConfig::RasSizeofFormat(const CTRaster& format) const
{
    return format.m_TeslaRaster->GetBytesPerPixel();
}

int LwrieBufferConfig::TgaPixelSize(const CTRaster& format) const {
    return RasSizeofFormat(format);
}

int TeslaBufferConfig::TgaPixelSize(const CTRaster& format) const {
    return format.m_TeslaRaster->TgaPixelSize();
}

void
Buf::FetchBufLine(
        UINT32 Line,
        UINT32 Width,     /* width of image */
        uintptr_t Offset,
        UINT32 Pitch,
        RasterFormat memFormat,   /* memory format */
        void *DataLine)
// FetchBufLine - fetch line from specified region of framebuffer
{
    int PerPixelBytes = rasSizeofFormat(memFormat);
    if (PerPixelBytes == 0)
    {
        ErrPrintf("FetchBufLine: unsupported memory format.\n");
        return;  // TODO: change function to return status
    }

    UINT32 linesize = PerPixelBytes * Width;
    uintptr_t src = Offset + Line * Pitch;

    Platform::VirtualRd((void*)src, DataLine, linesize);
}

void
Buf::FetchBufData(
        UINT32 Width,     /* width of image */
        UINT32 Height,        /* height of image */
        uintptr_t Offset,
        UINT32 Pitch,
        UINT32 depth,
        void *Data)
// FetchBufData - fetch data from specified region of framebuffer
//
// we have this "Really" function so I can have a Raw and a Cooked version without
// changing the interface for the default Cooked version, and without duplicating all
// the code in this function.
{
    unsigned int h;
    UINT32 linesize;

    //int PerPixelBytes = rasSizeofFormat(memFormat);
    int PerPixelBytes = depth;
    if (PerPixelBytes == 0)
    {
        ErrPrintf("FetchBufData: unsupported memory format.\n");
        return;  // TODO: change function to return status
    }

    DebugPrintf("FetchBufData: Data=%p Offset=%p Pitch=0x%x Width=0x%x Height=0x%x PerPixelBytes=0x%x\n",
            Data,(void *)Offset,Pitch,Width,Height,PerPixelBytes);
    linesize = PerPixelBytes * Width;

    for (h = 0; h < Height; h++) {
        // this little code fragment is LOTS faster than a conditional breakpoint.
        // static vars let you change them in a debugger, and they retain their value.
        static unsigned int stop_at_x = 40000;  // humungous numbers, so we don't actually fflush something by default
        if ( h == stop_at_x )
        {
            fflush(stdout);   // just something to stick a breakpoint on...
        }
#if 0
        FetchBufLine(h, Width, Offset, Pitch, memFormat, Data);
        Data = (void*) ((int) Data + linesize);
#else
        // ssiou: FetchBufLine seems has lot of overhead, and worse,
        // since it recallwlate its line size, 24bit format handle is strange
        // note that for 24 bits (3 bytes per pixel), the linesize
        // does not necessary equals to pitch
        uintptr_t src = Offset + h * Pitch;
        Platform::VirtualRd((void*)src, Data, linesize);
        Data = (void*) ((uintptr_t) Data + linesize);
#endif  // 0
    }
}

RasterFormat LwrieBufferConfig::RasterFmt(UINT32 bpp) const
{
    RasterFormat memFormat = RASTER_A8R8G8B8; // default value

    // colwert bpp back to 1,2,3,or4
    switch (bpp)
    {
        case  1: memFormat = RASTER_B8; break;
        case  2: memFormat = RASTER_R5G6B5; break;
        case  3: memFormat = RASTER_R8G8B8; break;
        case  4: memFormat = RASTER_A8R8G8B8; break;
        case  8: memFormat = RASTER_F_W16Z16Y16X16; break;
        case 16: memFormat = RASTER_F_W32Z32Y32X32; break;
        default:
                 ErrPrintf("Invalid bpp %d\n", bpp);
    }

    return memFormat;
}

RasterFormat TeslaBufferConfig::RasterFmt(UINT32 bpp) const {
    ErrPrintf("Invalid bpp %d\n", bpp);
    return RASTER_ERROR;
}

void LwrieBufferConfig::ColwertToTga(const CTRaster& CTrasFormat,UINT08* to,UINT08* from, UINT32 size) const {

    RasterFormat rasFormat = CTrasFormat.m_LwrieRaster;
    RasterFormat TgaFormat = this->TgaFormat(CTrasFormat).m_LwrieRaster;
    UINT32 bytesPerRow = size * TgaPixelSize(CTrasFormat);

    // colwert if we need to do so
    if (TgaFormat == RASTER_R8G8B8 && rasFormat == RASTER_R5G6B5){
        c565to888(from, to, size);
    } else  if ((TgaFormat == RASTER_A8R8G8B8 && rasFormat == RASTER_A8B8G8R8) ||
            (TgaFormat == RASTER_A8R8G8B8 && rasFormat == RASTER_X8B8G8R8)) {
        ca8b8g8r8toa8r8g8b8(from, to, size);
    } else if (TgaFormat == RASTER_R8G8B8 && rasFormat == RASTER_R8G8B8) {
        memcpy(to, from, bytesPerRow);
        // cb8g8r8tor8g8b8(from, to, size);
    } else if (TgaFormat == RASTER_R8G8B8 && rasFormat == RASTER_Y16) {
        y16to888(from, to, size);
    } else if (TgaFormat == RASTER_R8G8B8 && rasFormat == RASTER_Y32) {
        y32to888(from, to, size);
    } else {
        // write directly from raster
        memcpy(to, from, bytesPerRow);
    }
}

void TeslaBufferConfig::ColwertToTga(const CTRaster& r, UINT08* to,UINT08* from, UINT32 size) const {
    r.m_TeslaRaster->ColwertToTga(to, from, size);
}

int
Buf::Buf2Dib(
        UINT08 *data                    /* frame buffer data */
        , unsigned long Width           /* width of image */
        , unsigned long Height          /* height of image */
        , RasterFormat memFormat        /* memory format */
        , int  BPP                      /* bits-per-pixel of the DIB bitmap */
        , vector<UINT08>* pBitMap       /* DIB bitmap */
        , unsigned int *scanlineSize    /* rounded up to 4-byte multiple */
        , int bigEndian                 /* is buffer big endian? */
        )
// Buf2Dib - colwert framebuffer memory contents to dib data
{
    unsigned int w;
    int h;
    int BytesPerPixel;
    unsigned int i, j, a, r, g, b, color, hpixels, nBytes;
    unsigned int adj;

    BytesPerPixel = BPP/8;
    *scanlineSize = (Width * BytesPerPixel + 3) & (~3);
    nBytes = Height * *scanlineSize;
    /* Allocate memory for DIB bitmap */
    pBitMap->resize(nBytes);

    adj = 0;

    // can't do this through VPLI because globalLWSetup is not available
    if (bigEndian) {
        switch(rasSizeofFormat(memFormat)) {
            case 1: adj = 0; break;
            case 2: adj = 1; break;
            case 4: adj = 3; break;
            default:
                    ErrPrintf("Buf2Dib: unknown byte count %d\n",rasSizeofFormat(memFormat));
                    return 0;
        }
    }
    DebugPrintf("Buf2Dib: bigEndian=%d adj=%d\n",bigEndian,adj);

    a = 0xFF;  // default alpha is FF

    /* Read out the frame buffer data in reverse order */
    j=0;
    for(h=(int)Height-1; h >= 0; h--) {
        hpixels = h * Width;
        for(w=0; w < Width; w++) {
            switch(memFormat) {
                case RASTER_R5G6B5:
                    i = (hpixels + w) * 2;
                    b = (data[i^adj] & 0x1F) << 3;
                    g = ((data[i^adj] & 0xE0) >> 3)
                        | ((data[(i+1)^adj] & 0x07) << 5);
                    r = (data[(i+1)^adj] & 0xF8) << 0;
                    break;
                case RASTER_X1R5G5B5:
                    i = (hpixels + w) * 2;
                    b = (data[i^adj] & 0x1F) << 3;
                    g = ((data[i^adj] & 0xE0) >> 2) | ((data[(i+1)^adj] & 0x03) << 6);
                    r = (data[(i+1)^adj] & 0x7C) << 1;
                    break;
                case RASTER_A8R8G8B8:
                    i = (hpixels + w) * 4;
                    a = data[(i+3)^adj];
                case RASTER_X8R8G8B8:
                    i = (hpixels + w) * 4;
                    b = data[(i+0)^adj];
                    g = data[(i+1)^adj];
                    r = data[(i+2)^adj];
                    break;
                case RASTER_A8B8G8R8:
                    i = (hpixels + w) * 4;
                    a = data[(i+3)^adj];
                case RASTER_X8B8G8R8:
                    i = (hpixels + w) * 4;
                    r = data[(i+0)^adj];
                    g = data[(i+1)^adj];
                    b = data[(i+2)^adj];
                    break;
                case RASTER_Y8:
                    i = hpixels + w;
                    b = data[i^adj];
                    g = data[i^adj];
                    r = data[i^adj];
                    break;
                case RASTER_Y16:
                    i = (hpixels + w) * 2;
                    b = 0;
                    g = data[i];
                    r = data[i+1];
                    break;
                case RASTER_Y32:
                    i = (hpixels + w) * 4;
                    b = data[i+1];
                    g = data[i+2];
                    r = data[i+3];
                    break;
                default:
                    ErrPrintf("Buf2Dib: unsupported memory format %d\n",memFormat);
                    return(0);
                    break;
            }
            color = ( (a & 0xFF ) << 24)
                | ((r & 0xFF) << 16)
                | ((g & 0xFF) << 8 )
                | ((b & 0xFF)      );
            (*pBitMap)[j+w*3+0]    = (color>>0)  & 0xff;
            (*pBitMap)[j+w*3+1]  = (color>>8)  & 0xff;
            (*pBitMap)[j+w*3+2]  = (color>>16) & 0xff;
            //BRINGUP -- alpha is dropped
        }
        j += *scanlineSize;
    }
    return 1;
}

int Buf::dibSave
// dibSave - save a dib file
(
 const char    *filename,
 UINT08          *pBits,
 unsigned long  Width,
 unsigned long  Height,
 unsigned long  scanlineSize,
 unsigned long  BPP
)
{
    unsigned int ImageSize;
    BITMAPFILEHEADER bf;
    BITMAPINFOHEADER bi;

    FileIO *fh;
    DebugPrintf("open %s read/write ...\n", filename);
    fh = Sys::GetFileIO(filename, "wb");
    if (fh == NULL)
    {
        ErrPrintf("can't open output bmp file %s , fh= %p\n",filename, fh);
        return -1;
    }

    /* Fill in the fields of the bitmapinfo header */
    switch (BPP){
        case 1:
            bi.biClrUsed = bi.biClrImportant = SWAPULONG(2);
        case 4:
            bi.biClrUsed = bi.biClrImportant = SWAPULONG(16);
        case 8:
            bi.biClrUsed = bi.biClrImportant = SWAPULONG(256);
        default:
            /* A 24 bitcount DIB has no color table */
            bi.biClrUsed = bi.biClrImportant = 0;
    }
    bi.biSize          = SWAPULONG(sizeof(BITMAPINFOHEADER));
    bi.biWidth         = SWAPULONG(Width);
    bi.biHeight        = SWAPULONG(Height);
    bi.biPlanes        = SWAPUSHORT(1);
    bi.biBitCount      = (UINT16) SWAPUSHORT(BPP);
    bi.biCompression   = SWAPULONG(BI_RGB);
    bi.biSizeImage     = SWAPULONG(0);
    bi.biXPelsPerMeter = SWAPULONG(0);
    bi.biYPelsPerMeter = SWAPULONG(0);
    if (BPP == 15)
        BPP = 16;

    bi.biSizeImage     = SWAPULONG((UINT32) scanlineSize)*SWAPULONG(bi.biHeight);

    /* Fill in the fields of the file header */
    bf.bfType     = SWAPUSHORT(BFT_BITMAP);
    bf.bfSize     = SWAPULONG(SWAPULONG(bi.biSize)
            + SWAPULONG(bi.biClrUsed) * sizeof (RGBQUAD)
            + SWAPULONG(bi.biSizeImage)
            + SIZEOF_BITMAPFILEHEADER_PACKED);
    bf.bfReserved = SWAPULONG(0);
    bf.bfOffBits  = SWAPULONG(SIZEOF_BITMAPFILEHEADER_PACKED
            + SWAPULONG(bi.biSize)
            + SWAPULONG(bi.biClrUsed) * sizeof (RGBQUAD));
    /* Write the file header */
    /* write bfType*/
    fh->FileWrite(&(bf.bfType), sizeof (UINT08), sizeof (UINT16) / sizeof (UINT08));
    /* now pass over extra word, and only write next 3 DWORDS!*/
    fh->FileWrite(&(bf.bfSize),  sizeof (UINT08), sizeof(UINT32)*3 / sizeof (UINT08));

    /* Write the DIB header */
    fh->FileWrite (&bi, sizeof (UINT08), sizeof (BITMAPINFOHEADER) / sizeof (UINT08));
    /* Write the bits */
    ImageSize = SWAPULONG(bi.biSizeImage) / sizeof (UINT08);
    fh->FileWrite(pBits, sizeof (UINT08),  ImageSize);
    fh->FileClose();
    Sys::ReleaseFileIO(fh);

    return 1;
}

void LwrieBufferConfig::ToTgaExt(const CTRaster& format, unsigned char* PixelSize, unsigned char* Desc) const
{
    switch(format.m_LwrieRaster) {
        case RASTER_X1R5G5B5:
            *PixelSize = 16; *Desc = 0; break;
        case RASTER_R5G6B5:
            *PixelSize = 24; *Desc = 0; break; // 24 as it is stored as 888; tga does not have a 565 format
        case RASTER_A8R8G8B8:
            *PixelSize = 32; *Desc = 8; break;
        case RASTER_X8R8G8B8:
            *PixelSize = 32; *Desc = 0; break; // save X channel too
        case RASTER_Y8:
        case RASTER_B8:
            *PixelSize = 8;  *Desc = 0; break;
        case RASTER_Y16:
        case RASTER_Y32:
            *PixelSize = 24; *Desc = 0; break; // colwert to R8G8B8
        case RASTER_F_W32Z32Y32X32:
            *PixelSize = 32; *Desc = 8; break;
        case RASTER_F_W16Z16Y16X16:
            *PixelSize = 32; *Desc = 8; break;
        case RASTER_A8B8G8R8:
            *PixelSize = 32; *Desc = 8; break;
        case RASTER_X8B8G8R8:
            *PixelSize = 32; *Desc = 0; break; // save X channel too
        case RASTER_R8G8B8:
            *PixelSize = 24; *Desc = 0; break;
        default:
            MASSERT(0);
    }
}

void TeslaBufferConfig::ToTgaExt(const CTRaster& format, unsigned char* PixelSize, unsigned char* Desc) const
{
    return format.m_TeslaRaster->ToTgaExt(PixelSize, Desc);
}

static
void c565to888(UINT08 *src, UINT08 *dst, UINT32 npixels)
{
    UINT32 i, j, u;

    // replicate bits
    for (i = j = u = 0; u < npixels; ++u) {
        // B
        dst[j] = ((src[i] & 0x1F) << 3) | ((src[i] >> 2) & 7);
        // G
        dst[j+1] = ((src[i] & 0xE0) >> 3) | ((src[i + 1] & 0x07) << 5) | ((src[i+1] >> 1) & 3);
        // R
        dst[j+2] = (src[i + 1] & 0xF8) | ((src[i+1] >> 5) & 7);

        i += 2;
        j += 3;
    }
}

// colwert Y16 to R8G8B8, matching behavior of BufDumpBmp()
static
void y16to888(UINT08 *src, UINT08 *dst, UINT32 npixels)
{
    UINT32 u,i,j;

    for (i = j = u = 0; u < npixels; ++u) {
        dst[j] = 0;
        dst[j+1] = src[i];
        dst[j+2] = src[i+1];
        i += 2;
        j += 3;
    }
}

// colwert Y32 to R8G8B8, matching behavior of BufDumpBmp()
static
void y32to888(UINT08 *src, UINT08 *dst, UINT32 npixels)
{
    UINT32 u,i,j;

    for (i = j = u = 0; u < npixels; ++u) {
        dst[j] = src[i+1];
        dst[j+1] = src[i+2];
        dst[j+2] = src[i+3];
        i += 4;
        j += 3;
    }
}

static
void ca8b8g8r8toa8r8g8b8(unsigned char *src, unsigned char *dst, int npixels)
{
    int i;

    for (i = 0; i < npixels*4; i+=4) {
        // B
        dst[i]   = src[i+2];
        // G
        dst[i+1] = src[i+1];
        // R
        dst[i+2] = src[i];
        // A
        dst[i+3] = src[i+3];
    }
}

static void shuffledtolinear(UINT32 Width, UINT32 Height, UINT32 Depth,
        UINT08 *dst, UINT08 *src, UINT32 u, UINT32 v)
{
    UINT32 offset, u_offset, v_offset, j, k, byteshift, uu;

    /* the U address bits are of the form
       uuuu...0u0u0u
       where there are k 0u pairs.
       the V address bits are of the form
       vvvv...v0v0v0
       where there are k v0 pairs.
       */

    k = min(Width, Height);    // size of largest square in miplevel
    // j = log2(k)
    for (j=0, k >>= 1; k != 0; j++, k >>= 1) ;

    v_offset = (v >> j) << (2*j);   // get the vvv part
    for (k = 0; k < j; ++k)
        v_offset |= (v & (1<<k)) << (k+1);

    byteshift = Depth == 8 ? 0 : Depth == 16 ? 1 : 2;
    for (uu = u; uu < u + Width; ++uu) {
        u_offset = (uu >> j) << (2*j);   // get the uuu part
        for (k = 0; k < j; ++k)
            u_offset |= (uu & (1<<k)) << k;
        offset = (u_offset | v_offset) << byteshift;
        // copy
        switch(byteshift) {
            case 2:
                src[uu*4  ] = dst[offset  ];
                src[uu*4+1] = dst[offset+1];
                src[uu*4+2] = dst[offset+2];
                src[uu*4+3] = dst[offset+3];
                break;
            case 1:
                src[uu*2  ] = dst[offset  ];
                src[uu*2+1] = dst[offset+1];
                break;
            default:
            case 0:
                src[uu    ] = dst[offset  ];
                break;
        }
    }
}

CTRaster LwrieBufferConfig::TgaFormat(const CTRaster& format) const
{
    RasterFormat ret = RASTER_VOID32;
    switch(format.m_LwrieRaster) {
        case RASTER_X1R5G5B5:       ret = RASTER_X1R5G5B5;
                                    break;
        case RASTER_R5G6B5:         ret = RASTER_R8G8B8;
                                    break;
        case RASTER_A8R8G8B8:       ret = RASTER_A8R8G8B8;
                                    break;
        case RASTER_X8R8G8B8:       ret = RASTER_A8R8G8B8;  // we want to see the alpha channel anyway
                                    break;
        case RASTER_Y8:             ret = RASTER_Y8;
                                    break;
        case RASTER_Y16:            ret = RASTER_R8G8B8;
                                    break;
        case RASTER_Y32:            ret = RASTER_R8G8B8;
                                    break;
        case RASTER_VOID16:         ret = RASTER_VOID16;
                                    break;
        case RASTER_VOID32:         ret = RASTER_VOID32;
                                    break;
        case RASTER_R8G8B8:         ret = RASTER_R8G8B8;
                                    break;
        case RASTER_B8:             ret = RASTER_Y8;        // use palettized to show the channel
                                    break;
        case RASTER_F_W32Z32Y32X32: ret = RASTER_A8R8G8B8;
                                    break;
        case RASTER_F_W16Z16Y16X16: ret = RASTER_A8R8G8B8;
                                    break;
        case RASTER_A8B8G8R8:       ret = RASTER_A8R8G8B8;
                                    break;
        case RASTER_X8B8G8R8:       ret = RASTER_A8R8G8B8; // we want to see the alpha channel anyway
                                    break;
        default:
                                    ErrPrintf("Unknown raster type\n");
                                    MASSERT(0);
                                    break;
    }
    return CTRaster(ret, 0);
}

CTRaster TeslaBufferConfig::TgaFormat(const CTRaster& r) const{
    return CTRaster (RASTER_LW50, r.m_TeslaRaster);
}

// Returns number of bytes written
int WriteBlockHeader(FileIO *fh, UINT32 blockSize, UINT32 origSize, const char *name, int compressed)
{
    // Each extended block written to the TGA consists
    // of a 8 byte descriptor string and a 4 byte size.
    // The upper two bits of the 4 byte size describe the
    // format. Zero means uncompressed. 11 means zlib
    // compressed.
    char descstr[9];
    UINT08 temp8;

    if(compressed) blockSize |= 0xC0000000; else origSize = blockSize;

    memset(descstr, 0, sizeof(descstr));
    strcpy(descstr, name);
    temp8 = blockSize         & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    temp8 = (blockSize >> 8)  & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    temp8 = (blockSize >> 16) & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    temp8 = (blockSize >> 24) & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    temp8 = origSize         & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    temp8 = (origSize >> 8)  & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    temp8 = (origSize >> 16) & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    temp8 = (origSize >> 24) & 0xFF; fh->FileWrite((void*)&temp8, 1, 1);
    fh->FileWrite((void*)descstr, 8, 1);

    return 16;
}

void WriteTGATag(FileIO *fh, const char *key, UINT32 width, UINT32 height, UINT08 zformat, UINT08 cformat)
{
    // Because the FileIO class doesn't support ftell(), we'll
    // express the offset as the number of extra bytes written
    // to the file. At the end of the TGA file we'll write a
    // tag and the number of bytes written (inclusive of the tag).
    UINT32 ebw = 0;
    UINT32 temp32;

    // Write a marker to indicate the start of the extended data
    temp32 = 0x11111111;
    fh->FileWrite((void*)&temp32, 4, 1); ebw += 4;

    // Write the key data (lwrrently contains only the CRC key,
    // could contain any other strings).
    ebw += WriteBlockHeader(fh, (key == NULL) ? 0 : (4 + strlen(key)), 0xFFFFFFFF, "HASH", 0);
    if(key != NULL)
    {
        char str[1024];
        strcpy(str, "key=");
        fh->FileWrite(str, 4, 1); ebw += 4;
        strcpy(str, key);
        fh->FileWrite(str, strlen(str), 1); ebw += (UINT32)strlen(str);
    }

    // Write the format codes.
    ebw += WriteBlockHeader(fh, 2, 0xFFFFFFFF, "FMTCODE", 0);
    fh->FileWrite((void*)&zformat, 1, 1); ebw += 1;
    fh->FileWrite((void*)&cformat, 1, 1); ebw += 1;

    // Write the extended width info.
    ebw += WriteBlockHeader(fh, 8, 0xFFFFFFFF, "EXTSIZE", 0);
    fh->FileWrite((void*)&width, sizeof(UINT32), 1); ebw += sizeof(UINT32);
    fh->FileWrite((void*)&height, sizeof(UINT32), 1); ebw += sizeof(UINT32);

    // Write the tag and number of bytes written
    ebw += 8; // (Include the 8 bytes we're about to write)
    temp32 = 0xA2A2A2A2;
    fh->FileWrite((void*)&temp32, 4, 1);
    fh->FileWrite((void*)&ebw, 4, 1);
}

// *******************************************************************
// SaveBufTGA - save the contents of system memory into a tga file
//              decompress compressed buffers.
int Buf::SaveBufTGA(const char *filename, UINT32 Width, UINT32 Height,
        uintptr_t Offset, UINT32 Pitch, UINT32 bpp,
        RasterFormat rasFormat, int shuffled, const char *key)
{
    return SaveBufTGA(filename, Width, Height, Offset, Pitch, bpp, CTRaster(rasFormat, 0), shuffled, key);

}
int Buf::SaveBufTGA(const char *filename, UINT32 Width, UINT32 Height,
        uintptr_t Offset, UINT32 Pitch, UINT32 bpp,
        const CTRaster& CTrasFormat, int shuffled, const char *key)
{
    FileIO *fh;
    UINT32 wid, ht, depth, stride, s, v;
    UINT32 bytesPerRow;
    UINT32 ipitch;
    TGA_Header hdr;
    TGA_ImageId id;
    UINT08 *addr;
    UINT32 sizeOfFileBuffer;
    UINT32 compress;
    //RasterFormat TgaFormat = lw4totga(rasFormat);
    CTRaster CTTgaFormat = m_buffConfig->TgaFormat(CTrasFormat);
    RasterFormat TgaFormat = CTTgaFormat.m_LwrieRaster;

    // adjust image ht and width for swizzle if necessary
    if (shuffled) {
        wid = 1 << log2(Width);
        ht = 1 << log2(Height);
        ipitch = wid * bpp;
        stride = wid * bpp;
    } else {
        wid = Width;
        ht = Height;
        ipitch = Pitch;
        stride = Width * bpp;
    }

    /********* Get data from frame buffer */
    DebugPrintf("SaveBufTGA: Fetching data from system memory ...\n");

    //int PerPixelBytes = rasSizeofFormat(m_buffConfig->RasterFmt(bpp));
    int PerPixelBytes = bpp;
    if (PerPixelBytes == 0) {
        ErrPrintf("SaveBufTGA: unsupported memory format.\n");
        return 0;
    }

    int nBytes = Height * Width * PerPixelBytes;
    vector<UINT08> Data(nBytes);

    FetchBufData(wid, ht, Offset, ipitch, bpp, &Data[0]);

    if ((fh = Sys::GetFileIO(filename, "w+b")) == NULL)
    {
        ErrPrintf("Couldn't open file %s\n", filename);
        return 0;
    }

    // adjust width for image to account for fat pixel formats
    if(PerPixelBytes == 16)
        Width *= 4;
    else if(PerPixelBytes == 8)
        Width *= 2;

    // TGA stuff
    compress = (Width * Height) > 1024; // only compress bigger images

    // Clamp to 65534 because 64-bit per pixel formats must have an even number of 32-bit pixels per scanline.
    UINT32 clampedWidth = Width, clampedHeight = Height;
    if(Width > 65534)
    {
        WarnPrintf("Width %d out of bounds for TGA, clamping to 65534\n", Width);
        clampedWidth = 65534;
    }
    if(Height > 65534)
    {
        WarnPrintf("Height %d out of bounds for TGA, clamping to 65534\n", Height);
        clampedHeight = 65534;
    }

    hdr.ImageIDLength = sizeof(id);       // Lwpu format goes there
    hdr.CoMapType = 0;            // no colormap
    if (bpp == 1)
        hdr.ImgType = TGA_MONO;
    else
        hdr.ImgType = TGA_RGB;
    // set compression
    hdr.ImgType |= (compress) ? TGA_RLE : (char)0;
    hdr.Index_lo = hdr.Index_hi = hdr.Length_lo = hdr.Length_hi = hdr.CoSize = 0; // no cmap
    hdr.X_org_lo = hdr.X_org_hi = hdr.Y_org_lo = hdr.Y_org_hi = 0;        // 0,0 origin
    hdr.Width_lo = clampedWidth;
    hdr.Width_hi = clampedWidth >> 8;
    hdr.Height_lo = clampedHeight;
    hdr.Height_hi = clampedHeight >> 8;
    id.Magic = TGA_LWIDIA_MAGIC;
    id.ImageFmt = TgaFormat;
    //lw4totgaext(rasFormat, &hdr.PixelSize, &hdr.Desc);
    m_buffConfig->ToTgaExt(CTrasFormat, &hdr.PixelSize, &hdr.Desc);
    hdr.Desc |= TGA_ORG_TOP_LEFT;

    fh->FileWrite(&hdr, sizeof(hdr), 1);
    fh->FileWrite(&id, sizeof(id), 1);

    // get size of OUTPUT here
    s = m_buffConfig->TgaPixelSize(CTTgaFormat);
    bytesPerRow = Width * s;
    depth = bpp * 8; // this is depth of input image, not output
    vector<UINT08> RowBuffer(bytesPerRow);

    vector<UINT08> shufbuf;
    if (shuffled) {
        shufbuf.resize(stride*2); // seems like we need extra space
    }

    for (v = 0; v < Height; ++v) {

        if (shuffled) {
            shuffledtolinear(wid, ht, depth, &Data[0], &shufbuf[0], 0, v);
            m_buffConfig->ColwertToTga(CTrasFormat, &RowBuffer[0], &shufbuf[0], Width);
        }
        else
        {
            addr = &Data[stride * v];
            m_buffConfig->ColwertToTga(CTrasFormat, &RowBuffer[0], addr, Width);
        }

        // compress if needed
        if (compress) {
            vector<UINT08> fileBufferP;
            ImageFile::RLEncode(&RowBuffer[0], Width, s, &fileBufferP, &sizeOfFileBuffer);
            fh->FileWrite(&fileBufferP[0], sizeOfFileBuffer, 1);
        } else
            fh->FileWrite(&RowBuffer[0], bytesPerRow, 1);
    }

    //writing info about used format at the and of the file
    if (CTrasFormat.m_LwrieRaster == RASTER_LW50 && CTrasFormat.m_TeslaRaster != NULL)  //only supported for LW50 and up
        WriteTGATag(fh, key, Width, Height, UINT08(CTrasFormat.m_TeslaRaster->ZFormat()), UINT08(CTrasFormat.m_TeslaRaster->GetDeviceFormat()));

    fh->FileClose();
    Sys::ReleaseFileIO(fh);

    return 1;
}

const unsigned int chunk_size = 16384;

bool Buf::GZipImage(const string& filename) const
{
    FILE *source = fopen(filename.c_str(),"rb");
    if (!source)
    {
        ErrPrintf("Can not open file %s\n", filename.c_str());
        return false;
    }

    gzFile dest = gzopen( (filename+".gz").c_str(), "wb" );
    if (!dest)
    {
        ErrPrintf("Can not open file %s\n", (filename+".gz").c_str());
        return false;
    }

    vector<char> data(chunk_size);

    for (size_t bytes = fread(&data[0], 1, chunk_size, source);
         bytes;
         bytes = fread(&data[0], 1, chunk_size, source)
         )
    {
        gzwrite(dest, &data[0], bytes);
    }

    fclose(source);
    gzclose(dest);

    if (OK != Xp::EraseFile(filename))
        return false;

    return true;
}

RC Buf::CopyBlockLinearBuf(uintptr_t Src, uintptr_t SrcS, void *Dst, UINT32 MaxBytes,
        UINT32 BaseX, UINT32 BaseY, UINT32 BaseZ,
        UINT32 Width, UINT32 Height, UINT32 Depth,
        UINT32 ArraySize, UINT32 ArrayPitch,
        const BlockLinear *bl, UINT64 Pitch)
{
    UINT64 BufSize = max((UINT64)ArrayPitch, bl->Size()) * ArraySize;
    UINT32 BytesPerPixel = bl->BytesPerPixel();

    // Create temporary buffer to put block linear data in
    UINT08 *Data = (UINT08 *)Pool::AllocAligned(BufSize, 16);
    if (!Data)
    {
        ErrPrintf("CopyBlockLinearBuf memory alloc failed\n");
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    UINT08 *DataS = 0;
    if (SrcS)
    {
        DataS = (UINT08 *)Pool::AllocAligned(BufSize, 16);
        if (!DataS)
        {
            ErrPrintf("CopyBlockLinearBuf memory alloc failed\n");
            Pool::Free(Data);
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
    }

    // Get data from frame buffer
    DebugPrintf("CopyBlockLinearBuf: Fetching data from memory ...\n");
    Platform::VirtualRd((void*)Src, Data, min(BufSize, (UINT64)MaxBytes));
    if (SrcS)
        Platform::VirtualRd((void*)SrcS, DataS, min(BufSize, (UINT64)MaxBytes));

    // Colwert to pitch format
    UINT08 *pDst = (UINT08 *)Dst;
    UINT32 DstArrayPitch = Width * Height * Depth * bl->BytesPerPixel();
    UINT32 FirstHalfSize = ((BufSize/2 + (4*1024-1)) / (4*1024)) * (4*1024);
    for (UINT32 a = 0; a < ArraySize; ++a)
    {
        for (UINT32 z = 0; z < Depth; z++)
        {
            for (UINT32 y = 0; y < Height; y++)
            {
                for (UINT32 x = 0; x < Width; x++)
                {
                    UINT32 PitchOffset = a*DstArrayPitch + (z*Height + y)*Pitch + x*BytesPerPixel;
                    UINT32 BlOffset = a*ArrayPitch + bl->Address(BaseX+x, BaseY+y, BaseZ+z);
                    for (UINT32 i = 0; i < BytesPerPixel; i++)
                    {
                        pDst[PitchOffset + i] = SrcS && (BlOffset + i) >= FirstHalfSize ?
                            DataS[BlOffset + i - FirstHalfSize] :
                            Data[BlOffset + i];
                    }
                }
            }
        }
    }

    // Free temporary buffers
    Pool::Free(Data);
    if (SrcS)
        Pool::Free(DataS);

    return OK;
}

void Buf::CopyBuf(uintptr_t Src, uintptr_t SrcS, void *Dst,
        UINT32 BaseX, UINT32 BaseY, UINT32 BaseZ,
        UINT32 Width, UINT32 Height, UINT32 Depth, UINT32 ArraySize, UINT32 ArrayPitch,
        UINT32 BytesPerPixel, UINT64 Pitch, UINT32 LayerPitch)
{
    UINT32 LineSize = BytesPerPixel * Width;

    // Point the source to the first pixel
    Src += BaseX*BytesPerPixel;
    Src += BaseY*Pitch;
    Src += BaseZ*LayerPitch;

    DebugPrintf("CopyBuf: Fetching data from memory ...\n");

    // Copy each line of data in each image
    UINT32 DstArrayPitch = Width * Height * Depth * BytesPerPixel;
    UINT32 FirstHalfSize = (((Depth*LayerPitch)/2 + (4*1024-1)) / (4*1024)) * (4*1024);
    for (UINT32 a = 0; a < ArraySize; ++a)
    {
        for (UINT32 z = 0; z < Depth; z++)
        {
            for (UINT32 y = 0; y < Height; y++)
            {
                UINT32 PitchOffset = a*ArrayPitch + z*LayerPitch + y*Pitch;

                if (SrcS)
                {
                    // this line is split
                    if (PitchOffset < FirstHalfSize && (PitchOffset+LineSize) > FirstHalfSize)
                    {
                        UINT32 firstpart_size = FirstHalfSize - PitchOffset;

                        Platform::VirtualRd((void*)(Src + PitchOffset), Dst, firstpart_size);
                        Platform::VirtualRd((void*)SrcS, (UINT08*)Dst + firstpart_size, LineSize-firstpart_size);
                    }
                    else
                    {
                        if (PitchOffset + LineSize > FirstHalfSize)
                            Platform::VirtualRd((void*)(SrcS + PitchOffset - FirstHalfSize), Dst, LineSize);
                        else
                            Platform::VirtualRd((void*)(Src + PitchOffset), Dst, LineSize);
                    }
                }
                else
                {
                    Platform::VirtualRd((void*)(Src + PitchOffset), Dst, LineSize);
                }
                Dst = (UINT08 *)Dst + LineSize + a*DstArrayPitch;
            }
        }
    }
}
