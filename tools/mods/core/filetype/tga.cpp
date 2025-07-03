/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Read/Write TGA image files.
//
// Some of this code is based on example code from ftp.truevision.com.
// Those parts are
//    Copyright (c) 1989, 1990, 1992 Truevision, Inc.
// The rest is
//    Copyright (C) 2000 LWPU Corporation. All Rights Reserved.

#include "core/include/imagefil.h"
#include "core/include/memoryif.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/fileholder.h"
#include <stdio.h>
#include <string.h>
#include <vector>

namespace ImageFile
{
   // Here's some declarations private to this file.

   /****************************************************************************
   **
   **   For more information about the original Truevision TGA(tm) file format,
   **   or for additional information about the new extensions to the
   **   Truevision TGA file, refer to the "Truevision TGA File Format
   **   Specification Version 2.0" available from Truevision or your
   **   Truevision dealer.
   **
   **  FILE STRUCTURE FOR THE ORIGINAL TRUEVISION TGA FILE
   **     FIELD 1 : NUMBER OF CHARACTERS IN ID FIELD (1 BYTES)
   **     FIELD 2 : COLOR MAP TYPE (1 BYTES)
   **     FIELD 3 : IMAGE TYPE CODE (1 BYTES)
   **                   = 0 NO IMAGE DATA INCLUDED
   **                   = 1 UNCOMPRESSED, COLOR-MAPPED IMAGE
   **                   = 2 UNCOMPRESSED, TRUE-COLOR IMAGE
   **                   = 3 UNCOMPRESSED, BLACK AND WHITE IMAGE
   **                   = 9 RUN-LENGTH ENCODED COLOR-MAPPED IMAGE
   **                   = 10 RUN-LENGTH ENCODED TRUE-COLOR IMAGE
   **                   = 11 RUN-LENGTH ENCODED BLACK AND WHITE IMAGE
   **     FIELD 4 : COLOR MAP SPECIFICATION (5 BYTES)
   **               4.1 : COLOR MAP ORIGIN (2 BYTES)
   **               4.2 : COLOR MAP LENGTH (2 BYTES)
   **               4.3 : COLOR MAP ENTRY SIZE (2 BYTES)
   **     FIELD 5 : IMAGE SPECIFICATION (10 BYTES)
   **               5.1 : X-ORIGIN OF IMAGE (2 BYTES)
   **               5.2 : Y-ORIGIN OF IMAGE (2 BYTES)
   **               5.3 : WIDTH OF IMAGE (2 BYTES)
   **               5.4 : HEIGHT OF IMAGE (2 BYTES)
   **               5.5 : IMAGE PIXEL SIZE (1 BYTE)
   **               5.6 : IMAGE DESCRIPTOR BYTE (1 BYTE)
   **     FIELD 6 : IMAGE ID FIELD (LENGTH SPECIFIED BY FIELD 1)
   **     FIELD 7 : COLOR MAP DATA (BIT WIDTH SPECIFIED BY FIELD 4.3 AND
   **               NUMBER OF COLOR MAP ENTRIES SPECIFIED IN FIELD 4.2)
   **     FIELD 8 : IMAGE DATA FIELD (WIDTH AND HEIGHT SPECIFIED IN
   **               FIELD 5.3 AND 5.4)
   ****************************************************************************/

   struct DevDir
   {
      UINT16  tagValue;
      UINT32  tagOffset;
      UINT32  tagSize;
   };

   struct TgaFile
   {
      UINT08  idLength;       /* length of ID string */
      UINT08  mapType;        /* color map type */
      UINT08  imageType;      /* image type code */
      UINT16  mapOrigin;      /* starting index of map */
      UINT16  mapLength;      /* length of map */
      UINT08  mapWidth;       /* width of map in bits */
      UINT16  xOrigin;        /* x-origin of image */
      UINT16  yOrigin;        /* y-origin of image */
      UINT16  imageWidth;     /* width of image */
      UINT16  imageHeight;    /* height of image */
      UINT08  pixelDepth;     /* bits per pixel */
      UINT08  imageDesc;      /* image descriptor */
      char    idString[256];  /* image ID string */
      UINT16  devTags;        /* number of developer tags in directory */
      DevDir  *devDirs;       /* pointer to developer directory entries */
      UINT16  extSize;        /* extension area size */
      char    author[41];     /* name of the author of the image */
      char    authorCom[4][81];   /* author's comments */
      UINT16  month;          /* date-time stamp */
      UINT16  day;
      UINT16  year;
      UINT16  hour;
      UINT16  minute;
      UINT16  second;
      char    jobID[41];      /* job identifier */
      UINT16  jobHours;       /* job elapsed time */
      UINT16  jobMinutes;
      UINT16  jobSeconds;
      char    softID[41];     /* software identifier/program name */
      UINT16  versionNum;     /* software version designation */
      UINT08  versionLet;
      UINT32  keyColor;       /* key color value as A:R:G:B */
      UINT16  pixNumerator;   /* pixel aspect ratio */
      UINT16  pixDenominator;
      UINT16  gammaNumerator; /* gamma correction factor */
      UINT16  gammaDenominator;
      UINT32  colorCorrectOffset; /* offset to color correction table */
      UINT32  stampOffset;    /* offset to postage stamp data */
      UINT32  scanLineOffset; /* offset to scan line table */
      UINT08  alphaAttribute; /* alpha attribute description */
      UINT32  *scanLineTable; /* address of scan line offset table */
      UINT08  stampWidth;     /* width of postage stamp */
      UINT08  stampHeight;    /* height of postage stamp */
      void    *postStamp;     /* address of postage stamp data */
      UINT16  *colorCorrectTable;
      UINT32  extAreaOffset;  /* extension area offset */
      UINT32  devDirOffset;   /* developer directory offset */
      char    signature[18];  /* signature string */
   };

   RC LE_Read08(UINT08 * addr, FileHolder &fp);
   RC LE_Read16(UINT16 * addr, FileHolder &fp);
   RC LE_Read32(UINT32 * addr, FileHolder &fp);
   RC LE_Read32(UINT32 data, FileHolder &fp);
   RC LE_Write16(UINT16 data, FileHolder &fp);
   RC LE_Write08(UINT08 data, FileHolder &fp);

   void RLEncode(UINT08 *inB, UINT32 iSize, UINT32 colorByteSize,
           vector<UINT08> * outBp, UINT32 *oSizep);
}

namespace
{
    void Write(const void* ptr, size_t size, FILE* stream)
    {
        if (size != fwrite(ptr, 1, size, stream))
        {
            Printf(Tee::PriHigh, "Error writing TGA file\n");
        }
    }
}

//
// The RLE (run length encoding) compression scheme looks like this:
//
//    A count byte with the value 0 to 127, followed by (count+1) pixels,
// OR
//    A count byte with the value 128 to 255, followed by a single pixel
//    to be repeated (count+1-128) times.
//
class tgaCompressor
{
private:
   enum { m_BufPixels = 128 };

   UINT08   m_Buf[m_BufPixels*4];   // buffer of data (Little endian)
   FILE *   m_Fp;                   // file pointer
   UINT08 * m_pBuf;                 // point to recently used part of buf
   UINT32   m_LastPixel;            // A8R8G8B8 version of what's at m_pBuf
   UINT08   m_Count;                // Pixels (left/so far) in current run
   bool     m_IsRepeating;          // Is this a run of same pixel?
   UINT32   m_BytesPerPixel;        // 3 (or 4 if using alpha)

public:
   tgaCompressor(FILE * fp, UINT32 bpp = 3);
   ~tgaCompressor();
   RC ReadNext(UINT32 * NewPixel);
   RC WriteNext(UINT32 NewPixel);
   RC Flush(); // flush pending writes.
};

//------------------------------------------------------------------------------
tgaCompressor::tgaCompressor(FILE * fp, UINT32 bpp)
{
   m_Fp           = fp;
   m_pBuf         = m_Buf;
   m_Count        = 0;
   m_IsRepeating  = 0;
   m_LastPixel    = 0;

   if( bpp == 4 )
      m_BytesPerPixel   = 4;
   else
      m_BytesPerPixel   = 3;
}

//------------------------------------------------------------------------------
tgaCompressor::~tgaCompressor()
{
}

//------------------------------------------------------------------------------
RC tgaCompressor::ReadNext(UINT32 * NewPixel)
{
   StickyRC rc = OK;

   if( m_Count == 0 )
   {
      UINT32 UniquePixelsInRun;

      if (1 != fread(&m_Count, sizeof(UINT08), 1, m_Fp))
      {
          rc = RC::FILE_READ_ERROR;
      }

      if( m_Count >= 128 )
      {
         m_IsRepeating     = true;
         m_Count           = m_Count + 1 - 128;
         UniquePixelsInRun = 1;
      }
      else
      {
         m_IsRepeating     = false;
         m_Count           = m_Count + 1;
         UniquePixelsInRun = m_Count;
      }
      if( m_BytesPerPixel * UniquePixelsInRun !=
          fread(m_Buf, 1, m_BytesPerPixel * UniquePixelsInRun, m_Fp))
      {
         rc = RC::FILE_READ_ERROR;
      }

      m_pBuf = m_Buf;

      if( m_IsRepeating )
      {
         m_LastPixel = m_pBuf[0] +              // blue
                       (m_pBuf[1] << 8) +       // green
                       (m_pBuf[2] << 16);       // red
         if( m_BytesPerPixel == 4 )
            m_LastPixel += (m_pBuf[3] << 24);   // alpha
         else
            m_LastPixel += (0xFF << 24);        // alpha (opaque)
      }
   }

   if( m_IsRepeating )
   {
      *NewPixel = m_LastPixel;
   }
   else
   {
      *NewPixel   = m_pBuf[0] +              // blue
                    (m_pBuf[1] << 8) +       // green
                    (m_pBuf[2] << 16);       // red
      if( m_BytesPerPixel == 4 )
         *NewPixel   += (m_pBuf[3] << 24);   // alpha
      else
         *NewPixel   += (0xFF << 24);        // alpha (opaque)

      m_pBuf += m_BytesPerPixel;
   }

   m_Count--;

   return rc;
}

//------------------------------------------------------------------------------
RC tgaCompressor::WriteNext(UINT32 NewPixel)
{
   RC rc = OK;

   switch( m_Count )
   {
      case 128:
            // Run is at max length.

            rc = Flush();
            // FALL THROUGH -- m_Count is now 0.

      case 0:
            // First pixel of a new run.

            m_pBuf[0] = (NewPixel & 0x0000FF);        // blue
            m_pBuf[1] = (NewPixel & 0x00FF00) >> 8;   // green
            m_pBuf[2] = (NewPixel & 0xFF0000) >> 16;  // red
            if( m_BytesPerPixel == 4 )
               m_pBuf[3] = (NewPixel & 0xFF000000) >> 24;   // alpha
            m_IsRepeating = true;
            break;

      case 1:
            // Second pixel in a new run -- maybe colwert repeating to non-repeating.

            if( m_LastPixel != NewPixel )
            {
               m_IsRepeating = false;
               m_pBuf += m_BytesPerPixel;
               m_pBuf[0] = (NewPixel & 0x0000FF);        // blue
               m_pBuf[1] = (NewPixel & 0x00FF00) >> 8;   // green
               m_pBuf[2] = (NewPixel & 0xFF0000) >> 16;  // red
               if( m_BytesPerPixel == 4 )
                  m_pBuf[3] = (NewPixel & 0xFF000000) >> 24;   // alpha
            }
            break;

      default:
            if( m_IsRepeating )
            {
               if( m_LastPixel != NewPixel )
               {
                  // End of repeating run.

                  Flush();
                  m_pBuf[0] = (NewPixel & 0x0000FF);        // blue
                  m_pBuf[1] = (NewPixel & 0x00FF00) >> 8;   // green
                  m_pBuf[2] = (NewPixel & 0xFF0000) >> 16;  // red
                  if( m_BytesPerPixel == 4 )
                     m_pBuf[3] = (NewPixel & 0xFF000000) >> 24; // alpha
                  m_IsRepeating = true;
               }
            }
            else // not m_IsRepeating
            {
               if( m_LastPixel == NewPixel )
               {
                  // End of non-repeating run.

                  m_Count--;     // don't flush last pixel -- it's part of the new run.
                  Flush();
                  m_pBuf[0] = (NewPixel & 0x0000FF);        // blue
                  m_pBuf[1] = (NewPixel & 0x00FF00) >> 8;   // green
                  m_pBuf[2] = (NewPixel & 0xFF0000) >> 16;  // red
                  if( m_BytesPerPixel == 4 )
                     m_pBuf[3] = (NewPixel & 0xFF000000) >> 24; // alpha
                  m_IsRepeating = true;
                  m_Count = 1;
               }
               else
               {
                  // Add to non-repeating run.

                  m_pBuf += m_BytesPerPixel;
                  m_pBuf[0] = (NewPixel & 0x0000FF);        // blue
                  m_pBuf[1] = (NewPixel & 0x00FF00) >> 8;   // green
                  m_pBuf[2] = (NewPixel & 0xFF0000) >> 16;  // red
                  if( m_BytesPerPixel == 4 )
                     m_pBuf[3] = (NewPixel & 0xFF000000) >> 24; // alpha
               }
            }
   }

   m_LastPixel = NewPixel;
   m_Count++;
   return rc;
}

//------------------------------------------------------------------------------
RC tgaCompressor::Flush()
{
   RC rc = OK;

   if( m_Count > 0 )
   {
      UINT32 UniquePixelsInRun;

      if( m_IsRepeating )
      {
         UniquePixelsInRun = 1;
         m_Count           = m_Count - 1 + 128;
      }
      else
      {
         UniquePixelsInRun = m_Count;
         m_Count           = m_Count - 1;
      }

      Write(&m_Count, sizeof(UINT08), m_Fp);

      if( m_BytesPerPixel * UniquePixelsInRun !=
          fwrite(m_Buf, 1, m_BytesPerPixel * UniquePixelsInRun, m_Fp))
      {
         rc = RC::FILE_WRITE_ERROR;
      }

      m_Count = 0;
      m_pBuf = m_Buf;
      m_IsRepeating = true;
   }

   return rc;
}

//------------------------------------------------------------------------------
RC ImageFile::ReadTga
(
   const char *   FileName,
   bool           DoTile,
   ColorUtils::Format  MemColorFormat,   // 16 or 32 bit RGB only
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch
)
{
   TgaFile    tga;
   RC         rc = OK;
   FileHolder fp;

   CHECK_RC(fp.Open(FileName, "rb"));

   UINT32 MemPixelBytes = 0;
   {
      //
      // Read the basic header info, colwerting from LE to cpu format.
      //
      LE_Read08( &tga.idLength, fp );    // length of id string
      LE_Read08( &tga.mapType, fp );     // color LUT type (expect 0)
      LE_Read08( &tga.imageType, fp );   // type (expect 10 -- RLE 24-bit)
      LE_Read16( &tga.mapOrigin, fp );   // color LUT offset (expect 0)
      LE_Read16( &tga.mapLength, fp );   // (expect 0)
      LE_Read08( &tga.mapWidth, fp );    // (expect 0)
      LE_Read16( &tga.xOrigin, fp );     // (expect 0)
      LE_Read16( &tga.yOrigin, fp );     // (expect 0)
      LE_Read16( &tga.imageWidth, fp );  // width in pixels
      LE_Read16( &tga.imageHeight, fp ); // height in pixels
      LE_Read08( &tga.pixelDepth, fp );  // color bits per pixel (expect 24)
      CHECK_RC(LE_Read08( &tga.imageDesc, fp ));   // # alpha bits/pixel (expect 8)
                                         // pixel 0 start (expect upper-left)
                                         // interleave (expect none)

      // Read and ignore the image ID (text label).
      memset( tga.idString, 0, 256 );
      if ( tga.idLength > 0 )
      {
          if (tga.idLength !=
                  fread(tga.idString, 1, tga.idLength, fp.GetFile()))
          {
              return RC::FILE_READ_ERROR;
          }
      }

      //
      // Make sure this is an image we are prepared to deal with.
      // We only support one kind of TGA file here.
      //
      if( (tga.mapType   != 0) ||
          (tga.imageType != 10) ||
          (tga.mapLength != 0))
      {
         // This is not a RLE true color image.
         return RC::ILWALID_FILE_FORMAT;
      }
      if( (tga.pixelDepth != 24) && (tga.pixelDepth != 32) )
      {
         return RC::ILWALID_FILE_FORMAT;
      }

      UINT32 MinHeight = (MemHeight < tga.imageHeight) ? MemHeight : tga.imageHeight;

      //
      // Deal with image origin being other than top,left.
      //
      int RowStart = 0;
      int RowIncr  = 1;
      int ColStart = 0;
      int ColIncr  = 1;
      if (tga.imageDesc & (1<<4))
      {
         // X origin is right, not left.
         ColStart = tga.imageWidth - 1;
         ColIncr  = -1;
      }
      if (!(tga.imageDesc & (1<<5)))
      {
         // Y origin is bottom, not top.
         RowStart = MinHeight-1;
         RowIncr  = -1;
      }
      int RowEnd = RowStart + (RowIncr * MinHeight);
      int ColEnd = ColStart + (ColIncr * tga.imageWidth);

      //
      // Read and colwert the pixels, store them in memory.
      //
      //

      MemPixelBytes = ColorUtils::PixelBytes(MemColorFormat);

      tgaCompressor pixelSource(fp.GetFile(), tga.pixelDepth/8);

      for( int row = RowStart; row != RowEnd; row += RowIncr )
      {
         UINT08 * pMem = row * MemPitch + static_cast<UINT08*>(MemAddress);

         for( int col = ColStart; col != ColEnd; col += ColIncr )
         {
            UINT32 pixel;
            CHECK_RC(pixelSource.ReadNext(&pixel));

            if( col < (int)MemWidth )
            {
               // Colwert pixel to correct color format.
               pixel = ColorUtils::Colwert(pixel, ColorUtils::A8R8G8B8, MemColorFormat);
               // Store this pixel in memory.
               Platform::MemCopy(pMem + MemPixelBytes*col, &pixel, MemPixelBytes);
            }
         }
      }
   }

   if( DoTile )
      Memory::Tile(MemAddress, tga.imageWidth, tga.imageHeight, MemPixelBytes*8, MemPitch, MemWidth, MemHeight);

   return rc;
}

//------------------------------------------------------------------------------

#define RLE_RLE_PACKET  0x80
#define RLE_ONE_COLOR_RAW_PACKET 0x00
#define RLE_NUMBER_OF_PIXELS_FILTER 0x7F
#define RLE_MAX_PIXELS_PER_PACKET 0x7F  // number of pixels in packet - 1
#define RLE_ONE_COLOR_IN_PACKET 0x00

static int DiffThanPrevious(UINT08 *buff, UINT32 offset, UINT32 colorByteSize)
{
    int ret = 0;
    UINT32 i;

    for (i = 0; i < colorByteSize; i++)
        ret |= buff[offset - colorByteSize + i] != buff[offset + i];
    return ret;
}

void ImageFile::RLEncode(UINT08 *inB, UINT32 iSize, UINT32 colorByteSize,
        vector<UINT08> * outBp, UINT32 *oSizep)
{
    UINT32 incc, size;
    UINT08 lwrrPacket;
    UINT32 packetHeaderOffset;

/*
    // allocate output buffer generously, assume worst case, when all colors are different
    //  for evey 128 colors waste 1 byte for header; use 100 to be safe;
    size = (iSize + (iSize + 99)/100) * colorByteSize;
*/
    // actually, worst case is a 1-byte format & this pattern: aabcaabcaabc...
    // this expands to 5/4 of the original length
    // TODO: when the row is expanded this much, we should just default to raw
    size = ((iSize*5+3)/4 + (iSize+99)/100) * colorByteSize;

    // i hit a case where above worst case was not enough.
    // iSize = 64, size = 81, but *oSizep grew to 84
    if (size < 512) size = 512;

    outBp->resize(size);

    *oSizep = 0;

    // start new raw packet
    lwrrPacket = RLE_ONE_COLOR_RAW_PACKET;
    packetHeaderOffset = (*oSizep)++;
    (*outBp)[packetHeaderOffset] = lwrrPacket;

    // add first color
    incc = 0; // current color in input buffer
    memcpy(&((*outBp)[*oSizep]), &(inB[incc]), colorByteSize);
    (*oSizep) += colorByteSize;
    incc += colorByteSize;

    // for all remaining colors
    for (; incc < iSize * colorByteSize; incc += colorByteSize)
        // if different color
        if (DiffThanPrevious(inB, incc, colorByteSize))
            // if runlength packet
            if (lwrrPacket & RLE_RLE_PACKET) {
                // start raw packet with current color
                packetHeaderOffset = (*oSizep)++;
                lwrrPacket = RLE_ONE_COLOR_RAW_PACKET;
                (*outBp)[packetHeaderOffset] = lwrrPacket;
                memcpy(&((*outBp)[*oSizep]), &(inB[incc]), colorByteSize);
                (*oSizep) += colorByteSize;
            }
            // else current packet is raw
            else {
                // if full raw packet, start a new one
                if ((lwrrPacket & RLE_NUMBER_OF_PIXELS_FILTER) ==
                    RLE_MAX_PIXELS_PER_PACKET)
                {
                    // start raw packet with current color
                    packetHeaderOffset = (*oSizep)++;
                    lwrrPacket = RLE_ONE_COLOR_RAW_PACKET;
                    (*outBp)[packetHeaderOffset] = lwrrPacket;
                    memcpy(&((*outBp)[*oSizep]), &(inB[incc]), colorByteSize);
                    (*oSizep) += colorByteSize;
                }
                else {
                    // add to raw packet (guaranteed to have room)
                    lwrrPacket ++;
                    (*outBp)[packetHeaderOffset] = lwrrPacket;
                    memcpy(&((*outBp)[*oSizep]), &(inB[incc]), colorByteSize);
                    (*oSizep) += colorByteSize;
                }
            }
        else // same color
            // if runlength packet
            if (lwrrPacket & RLE_RLE_PACKET)
                // if full
                if ((lwrrPacket & RLE_NUMBER_OF_PIXELS_FILTER) ==
                    RLE_MAX_PIXELS_PER_PACKET)
                {
                    // start raw packet with current color
                    //   (raw since don't know whether next color is same as current)
                    packetHeaderOffset = (*oSizep)++;
                    lwrrPacket = RLE_ONE_COLOR_RAW_PACKET;
                    (*outBp)[packetHeaderOffset] = lwrrPacket;
                    memcpy(&((*outBp)[*oSizep]), &(inB[incc]), colorByteSize);
                    (*oSizep) += colorByteSize;
                }
                else {
                    // add color to run length packet
                    lwrrPacket ++;
                    (*outBp)[packetHeaderOffset] = lwrrPacket;
                }
            else // current packet is raw
                // if only 1 color in raw packet
                if ((lwrrPacket & RLE_NUMBER_OF_PIXELS_FILTER) == RLE_ONE_COLOR_IN_PACKET) {
                    // change packet from raw to run length and add current color
                    lwrrPacket |= RLE_RLE_PACKET;
                    lwrrPacket ++;
                    (*outBp)[packetHeaderOffset] = lwrrPacket;
                }
                else {
                    // eliminate prev color from raw packet
                    (*oSizep) -= colorByteSize;
                    lwrrPacket --;
                    (*outBp)[packetHeaderOffset] = lwrrPacket;
                    // start rl packet and add prev and current color
                    lwrrPacket = RLE_ONE_COLOR_RAW_PACKET;
                    lwrrPacket |= RLE_RLE_PACKET;
                    lwrrPacket ++;
                    packetHeaderOffset = (*oSizep)++;
                    (*outBp)[packetHeaderOffset] = lwrrPacket;
                    memcpy(&((*outBp)[*oSizep]), &(inB[incc-colorByteSize]), colorByteSize);
                    (*oSizep) += colorByteSize;
                }
}

RC ImageFile::WriteTga
(
   const char *   FileName,
   ColorUtils::Format  MemColorFormat,
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch,
   bool           AlphaToRgb, /* = false */
   bool           YIlwerted,   /* = false */
   bool           ColwertToA8R8G8B8,
   bool           ForceRaw
)
{
    RC         rc      = OK;
    FileHolder fp;

    CHECK_RC(fp.Open(FileName, "wb"));

    UINT32 MemPixelBytes = ColorUtils::PixelBytes(MemColorFormat);
    UINT32 TGAPixelBytes = ColorUtils::TgaPixelBytes(MemColorFormat);
    UINT08 tgaPerMem = 1; // number of tga pixels per one mem pixel

    //
    // Write the basic header info, colwerting from cpu to LE format.
    //
    LE_Write08( 0, fp );          // length of id string
    LE_Write08( 0, fp );          // color LUT type
    if(ForceRaw)
        LE_Write08( 2, fp );         // type (raw)
    else
        LE_Write08( 10, fp );         // type (expect 10 -- RLE 24-bit)
    LE_Write16( 0, fp );          // color LUT (colormap) offset
    LE_Write16( 0, fp );          // colormap length
    LE_Write08( 0, fp );          // colormap bit-depth
    LE_Write16( 0, fp );          // X origin
    LE_Write16( 0, fp );          // Y origin
    if (ColwertToA8R8G8B8)
    {
        LE_Write16( MemWidth, fp );   // width in pixels
        LE_Write16( MemHeight, fp );  // height in pixels
        if( MemPixelBytes > 3 )
            LE_Write08( 32, fp );      // color bits per pixel (has alpha)
        else
            LE_Write08( 24, fp );      // color bits per pixel (no alpha)
    }
    else
    {
        if (MemPixelBytes > TGAPixelBytes)
            tgaPerMem = MemPixelBytes/TGAPixelBytes;

        LE_Write16( MemWidth * tgaPerMem, fp );   // width in pixels
        LE_Write16( MemHeight, fp );  // height in pixels
        LE_Write08( 8*TGAPixelBytes, fp );
    }
    LE_Write08( 0x28, fp );       // 8 bits alpha, upper-left origin

    //
    // Read and colwert the pixels, store them in memory.
    //
    //
    vector<UINT32> lineBuf(MemWidth);

    { // cleanup block
        tgaCompressor pixelSink(fp.GetFile(), (MemPixelBytes > 3) ? 4 : 3);

        if (ColwertToA8R8G8B8)
        {
            for( UINT32 row = 0; row < MemHeight; row++ )
            {
                const char * pMem;
                if (YIlwerted)
                    pMem = (MemHeight - 1 - row) * MemPitch + (const char*)MemAddress;
                else
                    pMem = row * MemPitch + (const char *)MemAddress;

                ColorUtils::Colwert( pMem, MemColorFormat,
                                     (char*) &lineBuf[0], ColorUtils::A8R8G8B8, MemWidth);

                for (UINT32 col = 0; col < MemWidth; ++col)
                {
                    UINT32 pixel = lineBuf[col];
                    if (AlphaToRgb)
                    {
                        pixel = ((pixel & 0xff000000) >> 8)
                              + ((pixel & 0xff000000) >> 16)
                              + ((pixel & 0xff000000) >> 24);
                    }
                    CHECK_RC(pixelSink.WriteNext(pixel));
                }
            }
            pixelSink.Flush();
        }
        else
        {
            vector<UINT08> lineTga(MemWidth * TGAPixelBytes * tgaPerMem);
            vector<UINT08> lineMem(MemWidth * MemPixelBytes);
            const char * pMem;
            vector<UINT08> lineTgaRLE;
            UINT32 sizeTga;

            for( UINT32 row = 0; row < MemHeight; row++ )
            {
                if (YIlwerted)
                    pMem = (MemHeight - 1 - row) * MemPitch + (const char*)MemAddress;
                else
                    pMem = row * MemPitch + (const char *)MemAddress;

                Platform::MemCopy(&lineMem[0], pMem, MemWidth*MemPixelBytes);
                ColorUtils::ColwertToTga((const char*)&lineMem[0], (char*)&lineTga[0], MemColorFormat, MemWidth);

                RLEncode(&lineTga[0],
                         MemWidth,
                         ColorUtils::TgaPixelBytes(MemColorFormat),
                         &lineTgaRLE,
                         &sizeTga);

                if (ForceRaw)
                {
                    Write((const void*)&lineTga[0],
                            MemWidth * ColorUtils::TgaPixelBytes(MemColorFormat),
                            fp.GetFile());
                }
                else
                {
                    Write((const void*)&lineTgaRLE[0], sizeTga, fp.GetFile());
                }
            }

            //writing tga tag to the end of the file, so glivd and other tool
            //can figure out what format is in tga file
            UINT32 temp32;
            UINT32 ebw = 0;
            string fname = ColorUtils::FormatToString(MemColorFormat);
            UINT32 fsize = (UINT32) fname.size() + 1;
            char descstr[9];
            memset(descstr, 0, sizeof(descstr));
            strcpy(descstr, "FMTSTR");

            temp32 = 0x11111111;
            Write((const void*)&temp32, sizeof(temp32), fp.GetFile()); ebw += 4;

            Write((const void*)&fsize, sizeof(fsize), fp.GetFile()); ebw += 4;
            Write((const void*)&fsize, sizeof(fsize), fp.GetFile()); ebw += 4;
            Write((const void*)descstr, 8, fp.GetFile()); ebw += 8;
            Write((const void*)fname.c_str(), fsize, fp.GetFile()); ebw += fsize;

            ebw += 8;
            temp32 = 0xA2A2A2A2;
            Write((const void*)&temp32, sizeof(temp32), fp.GetFile());
            Write((const void*)&ebw, sizeof(ebw), fp.GetFile());
        }

    }

    return rc;
}

//------------------------------------------------------------------------------
RC ImageFile::LE_Read08(UINT08 * addr, FileHolder &fp)
{
   RC rc = (1 == fread(addr, 1, 1, fp.GetFile())) ? OK : RC::FILE_READ_ERROR;
   return rc;
}

//------------------------------------------------------------------------------
RC ImageFile::LE_Read16(UINT16 * addr, FileHolder &fp)
{
   RC rc = (2 == fread(addr, 1, 2, fp.GetFile())) ? OK : RC::FILE_READ_ERROR;
   return rc;
}

//------------------------------------------------------------------------------
RC ImageFile::LE_Read32(UINT32 * addr, FileHolder &fp)
{
   RC rc = (4 == fread(addr, 1, 4, fp.GetFile())) ? OK : RC::FILE_READ_ERROR;
   return rc;
}

// //------------------------------------------------------------------------------
// RC ImageFile::LE_Read24(UINT32 * addr, FileHolder &fp
// {
//    RC rc = (3 == fread(addr, 1, 3, fp.GetFile())) ? OK : RC::FILE_READ_ERROR;
//    return rc;
// }

//------------------------------------------------------------------------------
RC ImageFile::LE_Write08(UINT08 data, FileHolder &fp)
{
   RC rc = (1 == fwrite(&data, 1, 1, fp.GetFile())) ? OK : RC::FILE_READ_ERROR;
   return rc;
}

//------------------------------------------------------------------------------
RC ImageFile::LE_Write16(UINT16 data, FileHolder &fp)
{
   RC rc = (2 == fwrite(&data, 1, 2, fp.GetFile())) ? OK : RC::FILE_READ_ERROR;
   return rc;
}

// //------------------------------------------------------------------------------
// RC ImageFile::LE_Write32(UINT32 data, FileHolder &fp)
// {
//    RC rc = (4 == fwrite(&data, 1, 4, fp.GetFile())) ? OK : RC::FILE_READ_ERROR;
//    return rc;
// }

