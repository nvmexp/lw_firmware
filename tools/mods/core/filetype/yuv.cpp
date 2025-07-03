/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

// Read/Write YUV image files.
//
// If DoSwapBytes is true, each pair of bytes is swapped on the way.
// This colwerts between CR8YB8CB8YA8 and YB8CR8YA8CB8,
// or between ECR8EYB8ECB8EYA8 and EYB8ECR8EYA8ECB8.
//
// Other than this, the functions do no data colwersion.
//
// Note that these YUV files are an lWpu invention, and are not
// standardized even within lWpu.  The files themselves do not
// contain any header data other than an ID string, width, and height.
//
// Extension      Format
// .LC0           ITU-R BT.601 format YbCrYaCb  (bytes 3..0)
// .LC1           same, but CrYbCbYa
// .LC2           ITU-R BT.709 format YbCrYaCb
// .LC3           same, but CrYbCbYa
// .YUV           same as .LC0, probably
// .BIN           binary, otherwise same as .LC0, probably
//
// Where is EYbECrEYaECb color format?
//

#include "core/include/imagefil.h"
#include "core/include/memoryif.h"
#include "core/include/utility.h"
#include "core/include/fileholder.h"
#include <stdio.h>
#include <string.h>
#include <vector>

namespace ImageFile
{
   // Here's some declarations private to this file.

   struct YuvBinaryHeader
   {
      char     Magic[4];      // Must always be 'Y' 'U' 'V' ' '
      UINT08   Width[4];      // Assemble into UINT32, little endian
      UINT08   Height[4];     // Assemble into UINT32, little endian
   };

   RC ReadYuvAscii
   (
      const char *   FileName,
      bool           DoSwapBytes,
      bool           DoTile,
      void *         MemAddress,
      UINT32         MemWidth,
      UINT32         MemHeight,
      UINT32         MemPitch,
      UINT32 *       ImageWidth,
      UINT32 *       ImageHeight
   );
   RC WriteYuvAscii
   (
      const char *   FileName,
      bool           DoSwapBytes,
      void *         MemAddress,
      UINT32         MemWidth,
      UINT32         MemHeight,
      UINT32         MemPitch
   );

   RC ReadYuvBinary
   (
      const char *   FileName,
      bool           DoSwapBytes,
      bool           DoTile,
      void *         MemAddress,
      UINT32         MemWidth,
      UINT32         MemHeight,
      UINT32         MemPitch,
      UINT32 *       ImageWidth,
      UINT32 *       ImageHeight
   );

   RC WriteYuvBinary
   (
      const char *   FileName,
      bool           DoSwapBytes,
      void *         MemAddress,
      UINT32         MemWidth,
      UINT32         MemHeight,
      UINT32         MemPitch
   );

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RC ImageFile::ReadYuv
(
   const char *   FileName,
   bool           DoSwapBytes,
   bool           DoTile,
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch,
   UINT32 *       ImageWidth,
   UINT32 *       ImageHeight
)
{
   // First, try to read it as a binary YUV file.

   RC rc = ImageFile::ReadYuvBinary( FileName, DoSwapBytes, DoTile, MemAddress,
                                      MemWidth, MemHeight, MemPitch, ImageWidth, ImageHeight);

   if( RC(RC::ILWALID_FILE_FORMAT) == rc )
   {
      // Whoops, not a valid binary file.  Let's try Ascii.

      rc = ImageFile::ReadYuvAscii( FileName, DoSwapBytes, DoTile, MemAddress,
                                     MemWidth, MemHeight, MemPitch, ImageWidth, ImageHeight);
   }
   return rc;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RC ImageFile::WriteYuv
(
   const char *   FileName,
   bool           DoSwapBytes,
   bool           DoAscii,
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch
)
{
   if( DoAscii )
      return ImageFile::WriteYuvAscii( FileName, DoSwapBytes, MemAddress,
                                        MemWidth, MemHeight, MemPitch);
   else
      return ImageFile::WriteYuvBinary( FileName, DoSwapBytes, MemAddress,
                                         MemWidth, MemHeight, MemPitch);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RC ImageFile::ReadYuvAscii
(
   const char *   FileName,
   bool           DoSwapBytes,
   bool           DoTile,
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch,
   UINT32 *       ImageWidth,
   UINT32 *       ImageHeight
)
{
   INT32 rc = OK;
   FileHolder Fp;

   // Force even MemWidth -- we work on pairs of pixels.
   MemWidth &= ~1;

   // Attempt to open the file for reading.
   CHECK_RC(Fp.Open(FileName, "r"));

   UINT32 FileWidth;
   UINT32 FileHeight;
   do // A one-time do loop, for colwenient error handling.
   {
      // Read the header -- magic string, width, height.
      const UINT32 ReadBufSize = 128;
      char ReadBuf[ReadBufSize];

      if( (NULL == fgets(ReadBuf, ReadBufSize, Fp.GetFile())) ||
          (0 != strncmp(ReadBuf, "YUV", 3)) ||
          (2 != fscanf( Fp.GetFile(), " %d %d", &FileWidth, &FileHeight)) ||
          (FileWidth & 1))
      {
         rc = RC::ILWALID_FILE_FORMAT;
         break;
      }

      *ImageWidth  = FileWidth;
      *ImageHeight = FileHeight;

      // Read and parse each line, colwert the data, and store it in memory.
      for( UINT32 row = 0; (row < FileHeight) && (row < MemHeight); row++ )
      {
         // Handle two pixels (columns) at a time.
         for( UINT32 col = 0; col < FileWidth; col += 2 )
         {
            UINT32 PxPair[4]; // one pair of pixels
            if( 4 != fscanf( Fp.GetFile(), " %d %d %d %d",
                             &PxPair[0], &PxPair[1], &PxPair[2], &PxPair[3] ))
            {
               rc = RC::CANNOT_PARSE_FILE;
               break;
            }

            if( col < MemWidth )
            {
               UINT08 PackedPxPair[4];
               if( DoSwapBytes )
               {
                  PackedPxPair[0] = (UINT08) PxPair[1];
                  PackedPxPair[1] = (UINT08) PxPair[0];
                  PackedPxPair[2] = (UINT08) PxPair[3];
                  PackedPxPair[3] = (UINT08) PxPair[2];
               }
               else
               {
                  PackedPxPair[0] = (UINT08) PxPair[0];
                  PackedPxPair[1] = (UINT08) PxPair[1];
                  PackedPxPair[2] = (UINT08) PxPair[2];
                  PackedPxPair[3] = (UINT08) PxPair[3];
               }
               memcpy(((char *)MemAddress) + row*MemPitch + 2*col,
                     &PackedPxPair[0], 4);
            }
         }
         if( rc != OK )
            break;
      }

   } while(0); // End of one-time do loop.

   if( DoTile && (OK == rc) )
      Memory::Tile(MemAddress, FileWidth, FileHeight, 16, MemPitch, MemWidth, MemHeight);

   return RC(rc);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RC ImageFile::WriteYuvAscii
(
   const char *   FileName,
   bool           DoSwapBytes,
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch
)
{
   // Force even MemWidth -- we work on pairs of pixels.
   RC rc     = OK;
   MemWidth &= ~1;
   FileHolder Fp;

   // Attempt to open the file for overwriting
   CHECK_RC(Fp.Open(FileName, "w"));

   do // A one-time do loop, for colwenient error handling.
   {
      // Write the header -- magic string, width, height.
      if( 0 == fprintf( Fp.GetFile(), "YUV\n%d %d\n", MemWidth, MemHeight ))
      {
         rc = RC::FILE_WRITE_ERROR;
         break;
      }

      // Colwert and write each line of the image.
      for( UINT32 row = 0; row < MemHeight; row++ )
      {
         // Handle two pixels (columns) at a time.
         for( UINT32 col = 0; col < MemWidth; col += 2 )
         {
            UINT08 PxPair[4]; // one pair of pixels

            memcpy( &PxPair[0], ((char *)MemAddress) + row*MemPitch + 2*col, 4);

            if( DoSwapBytes )
            {
               fprintf( Fp.GetFile(), "%d %d %d %d ",
                        PxPair[1], PxPair[0], PxPair[3], PxPair[2] );
            }
            else
            {
               fprintf( Fp.GetFile(), "%d %d %d %d ",
                        PxPair[0], PxPair[1], PxPair[2], PxPair[3] );
            }
         }
         if( rc != OK )
            break;
         fprintf( Fp.GetFile(), "\n" );
      }

   } while(0); // End of one-time do loop.

   return RC(rc);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RC ImageFile::ReadYuvBinary
(
   const char *   FileName,
   bool           DoSwapBytes,
   bool           DoTile,
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch,
   UINT32 *       ImageWidth,
   UINT32 *       ImageHeight
)
{
   vector<char> SwapBuf;
   INT32    rc       = OK;

   // Force even MemWidth -- we work on pairs of pixels.
   MemWidth &= ~1;

   // Attempt to open the file for reading.
   FileHolder Fp;

   CHECK_RC(Fp.Open(FileName, "rb"));

   UINT32 FileWidth  = 0;
   UINT32 FileHeight = 0;

   {  // Cleanup

      // Read the header -- magic string, width, height.
      ImageFile::YuvBinaryHeader Hdr;

      if( sizeof(Hdr) != fread( &Hdr, 1, sizeof(Hdr), Fp.GetFile() ) )
      {
         return RC::FILE_READ_ERROR;
      }

      // Explicitly colwert chars to int, little-endian.  Portable to Mac, Sun.
      FileWidth = UINT32(Hdr.Width[0]) +
                  (UINT32(Hdr.Width[1]) << 8) +
                  (UINT32(Hdr.Width[2]) << 16) +
                  (UINT32(Hdr.Width[3]) << 24);
      FileHeight = UINT32(Hdr.Height[0]) +
                  (UINT32(Hdr.Height[1]) << 8) +
                  (UINT32(Hdr.Height[2]) << 16) +
                  (UINT32(Hdr.Height[3]) << 24);

      if( (Hdr.Magic[0] != 'Y') ||
          (Hdr.Magic[1] != 'U') ||
          (Hdr.Magic[2] != 'V') ||
          (Hdr.Magic[3] != ' ') ||
          (FileWidth & 1))
      {
         return RC::ILWALID_FILE_FORMAT;
      }

      *ImageWidth  = FileWidth;
      *ImageHeight = FileHeight;

      if( DoSwapBytes )
      {
         // Get a buffer in host CPU memory, so we don't end up
         // doing byte read/writes over PCI bus.
         SwapBuf.resize(FileWidth * 2);
      }

      UINT32 MinWidth = MemWidth < FileWidth ? MemWidth : FileWidth;

      // Read and colwert each line.

      for( UINT32 row = 0; (row < FileHeight) && (row < MemHeight); row++ )
      {
         if( DoSwapBytes )
         {
            // Read into swap buffer, swap, then copy to destination memory.

            if( FileWidth*2 != fread( &SwapBuf[0], 1, FileWidth*2, Fp.GetFile() ))
            {
               return RC::FILE_READ_ERROR;
            }
            for( UINT32 col = 0; col < MinWidth; col += 2 )
            {
               char tmp = SwapBuf[col];
               SwapBuf[col] = SwapBuf[col+1];
               SwapBuf[col+1] = tmp;
            }
            memcpy(((char *)MemAddress) + row*MemPitch, &SwapBuf[0], MinWidth*2);
         }
         else
         {
            // Read directly into destination memory.

            if( MinWidth*2 != fread( ((char *)MemAddress) + row*MemPitch,
                                     1, MinWidth*2, Fp.GetFile() ))
            {
               return RC::FILE_READ_ERROR;
            }
            // Skip over the rest of the file line, if longer than mem line.
            if( FileWidth > MinWidth )
            {
               if (fseek( Fp.GetFile(), FileWidth - MinWidth, SEEK_LWR ) != 0)
               {
                  return RC::FILE_READ_ERROR;
               }
            }
         }
      }

   }

   if( DoTile )
      Memory::Tile(MemAddress, FileWidth, FileHeight, 16, MemPitch, MemWidth, MemHeight);

   return RC(rc);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
RC ImageFile::WriteYuvBinary
(
   const char *   FileName,
   bool           DoSwapBytes,
   void *         MemAddress,
   UINT32         MemWidth,
   UINT32         MemHeight,
   UINT32         MemPitch
)
{
   vector<char> SwapBuf;
   INT32    rc       = OK;
   FileHolder Fp;

   // Force even MemWidth -- we work on pairs of pixels.
   MemWidth &= ~1;

   // Attempt to open the file for overwriting
   CHECK_RC(Fp.Open(FileName, "wb"));

   if( DoSwapBytes )
      SwapBuf.resize(MemWidth * 2);

   do // A one-time do loop, for colwenient error handling.
   {
      // Write the header -- magic string, width, height.
      ImageFile::YuvBinaryHeader Hdr;

      Hdr.Magic[0] = 'Y';
      Hdr.Magic[1] = 'U';
      Hdr.Magic[2] = 'V';
      Hdr.Magic[3] = ' ';

      // Explicitly store as little-endian, for portability.
      Hdr.Width[0] = (UINT08)(MemWidth & 0xFF);
      Hdr.Width[1] = (UINT08)((MemWidth>>8) & 0xFF);
      Hdr.Width[2] = (UINT08)((MemWidth>>16) & 0xFF);
      Hdr.Width[3] = (UINT08)((MemWidth>>24) & 0xFF);
      Hdr.Height[0] = (UINT08)(MemHeight & 0xFF);
      Hdr.Height[1] = (UINT08)((MemHeight>>8) & 0xFF);
      Hdr.Height[2] = (UINT08)((MemHeight>>16) & 0xFF);
      Hdr.Height[3] = (UINT08)((MemHeight>>24) & 0xFF);

      if( sizeof(Hdr) != fwrite( &Hdr, sizeof(Hdr), 1, Fp.GetFile()) )
      {
         rc = RC::FILE_WRITE_ERROR;
         break;
      }

      // Colwert and write each line of the image.
      for( UINT32 row = 0; row < MemHeight; row++ )
      {
         if( DoSwapBytes )
         {
            memcpy( &SwapBuf[0], ((char *)MemAddress) + row*MemPitch, MemWidth*2 );

            for( UINT32 col = 0; col < MemWidth; col += 2 )
            {
               char tmp = SwapBuf[col];
               SwapBuf[col] = SwapBuf[col+1];
               SwapBuf[col+1] = tmp;
            }
            if( MemWidth*2 != fwrite( &SwapBuf[0], MemWidth*2, 1, Fp.GetFile() ))
            {
               rc = RC::FILE_WRITE_ERROR;
               break;
            }
         }
         else
         {
            if( MemWidth*2 != fwrite( ((char *)MemAddress) + row*MemPitch,
                                       MemWidth*2, 1, Fp.GetFile() ))
            {
               rc = RC::FILE_WRITE_ERROR;
               break;
            }
         }
      }

   } while(0); // End of one-time do loop.

   return RC(rc);
}

