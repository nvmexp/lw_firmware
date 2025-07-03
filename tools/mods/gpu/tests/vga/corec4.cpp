//
//    COREC4.CPP - VGA Core "C" Library File #5 (Graphics Routines)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       13 December 1994
//    Last modified: 16 November 2005
//
//    Routines in this file:
//    TextCharOut             Write a character attribute at a specific location
//    TextStringOut           Write a character string at a specific location
//    PlanarCharOut           Write a character at a specific location in graphics mode
//    CGA4CharOut             Write a character at a specific location in CGA 4-color
//    CGA2CharOut             Write a character at a specific location in CGA 2-color
//    MonoGrCharOut           Write a character at a specific location in 1 BPP mode
//    VGACharOut              Write a character at a specific location in 256 color mode
//    Line4                Draw a line from pt A to pt B with a given color in 4-color mode
//    HLine4Internal          Internal horizontal line handler for 4 BPP mode
//    VLine4Internal          Internal vertical line handler for 4 BPP mode
//    CalcStart4              Callwlate the start address in 4 BPP mode
//    SetLine4Columns            Set row offset for the line routines
//    SetLine4StartAddress    Set the start address for the line routines
//
//
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

//
//    TextCharOut - Write a character attribute at a specific location
//
//    Entry:   chr      ASCII character code
//          attr  Attribute
//          col      Column
//          row      Row
//          page  Page
//    Exit: None
//
void TextCharOut (BYTE chr, BYTE attr, BYTE col, BYTE row, BYTE page)
{
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;
   static SEGOFF  lpRegenLength = (SEGOFF) 0x0000044C;
   static SEGOFF  lpMode = (SEGOFF) 0x00000449;
   SEGOFF         lpVideo;
   WORD        offset;

   SetLwrsorPosition (col, row, page);
   lpVideo = (SEGOFF) (0xB8000000);
   if (MemByteRead (lpMode) == 0x07) lpVideo = (SEGOFF) (0xB0000000);
   offset = (((MemWordRead (lpColumns) * row) + col) * 2) +
            (page * MemWordRead (lpRegenLength));
   MemByteWrite (lpVideo + offset, chr);
   MemByteWrite (lpVideo + offset + 1, attr);
}

//
//    TextStringOut - Write a character string at a specific location
//
//    Entry:   lpsz     Pointer to string
//          attr     Attribute
//          col         Column
//          row         Row
//          page     Page
//    Exit: None
//
void TextStringOut (const char * lpsz, BYTE attr, BYTE col, BYTE row, BYTE page)
{
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;
   static SEGOFF  lpRegenLength = (SEGOFF) 0x0000044C;
   static SEGOFF  lpMode = (SEGOFF) 0x00000449;
   SEGOFF         lpVideo;
   WORD        offset;
   size_t         n;

   lpVideo = (SEGOFF) (0xB8000000);
   if (MemByteRead (lpMode) == 0x07) lpVideo = (SEGOFF) (0xB0000000);
   offset = (((MemWordRead (lpColumns) * row) + col) * 2) +
            (page * MemWordRead (lpRegenLength));
   lpVideo += offset;
   n = strlen (lpsz);
   SetLwrsorPosition ((BYTE) (col + n), row, page);
   while (n-- != 0)
   {
      MemByteWrite (lpVideo++, *lpsz++);
      MemByteWrite (lpVideo++, attr);
   }
}

//
//    PlanarCharOut - Write a character at a specific location in graphics mode
//
//    Entry:   chr      ASCII character code
//          color Color
//          col      Column
//          row      Row
//          page  Page
//    Exit: None
//
void PlanarCharOut (BYTE chr, BYTE color, BYTE col, BYTE row, BYTE page)
{
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;
   static SEGOFF  lpRegenLength = (SEGOFF) 0x0000044C;
   static SEGOFF  lpINT43 = (SEGOFF) 0x0000010C;
   static SEGOFF  lpCharHeight = (SEGOFF) 0x00000485;
   SEGOFF         lpVideo;
   WORD        offset, height, next;
   LPBYTE         lpFont;

   SetLwrsorPosition (col, row, page);
   lpVideo = (SEGOFF) (0xA0000000);
   next = MemWordRead (lpColumns);
   height = (WORD) MemByteRead (lpCharHeight);
   offset = ((next * (WORD) row * height) + (WORD) col) +
            (((WORD) page) * MemWordRead (lpRegenLength));
   lpVideo += offset;
   lpFont = (LPBYTE)(size_t)(MemDwordRead (lpINT43));
   lpFont += height * (WORD) chr;

   IOWordWrite (GDC_INDEX, 0xFF08);          // Bit mask = all enabled
   IOWordWrite (GDC_INDEX, 0x0005);          // Write mode 0
   IOWordWrite (GDC_INDEX, 0x0001);          // Disable set/reset stuff
   IOWordWrite (GDC_INDEX, 0x0000);
   IOByteWrite (SEQ_INDEX, 0x02);               // Set sequencer index
   while (height-- != 0)
   {
      IOByteWrite (SEQ_DATA, 0x0F);             // Enable all planes
      MemByteWrite (lpVideo, 0x00);
      IOByteWrite (SEQ_DATA, color);            // Enable some planes
      MemByteWrite (lpVideo, *lpFont++);
      lpVideo += next;
   }
   IOWordWrite (SEQ_INDEX, 0x0F02);          // Enable all planes
}

//
//    CGA4CharOut - Write a character at a specific location in CGA 4-color
//
//    Entry:   chr      ASCII character code
//          attr  color
//          col      Column
//          row      Row
//          page  Page (must be 0)
//    Exit: None
//
//    Assume character height is an even number.
//
void CGA4CharOut (BYTE chr, BYTE color, BYTE col, BYTE row, BYTE page)
{
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;
   static SEGOFF  lpINT43 = (SEGOFF) 0x0000010C;
   static SEGOFF  lpCharHeight = (SEGOFF) 0x00000485;
   SEGOFF         lpVideo;
   LPBYTE         lpFont;
   WORD        offset, height, next, wTemp;
   BYTE        byTemp, byCount;

   color &= 0x03;
   SetLwrsorPosition (col, row, page);
   lpVideo = (SEGOFF) (0xB8000000);
   next = MemWordRead (lpColumns) * 2;
   height = (WORD) MemByteRead (lpCharHeight);
   offset = (next * (WORD) row * height) / 2 + (col * 2);
   lpFont = (LPBYTE)(size_t)(MemDwordRead (lpINT43));
   lpFont += height * (WORD) chr;
   while (height--)
   {
      // Build the font word
      byTemp = *lpFont++;
      wTemp = 0;
      byCount = 8;
      while (byCount--)
      {
         if (byTemp & 1) wTemp |= color;
         byTemp = byTemp >> 1;
         wTemp = RotateWordRight (wTemp, 2);
      }

      // Swap the low-order and high-order bytes in the word
      wTemp = RotateWordRight (wTemp, 8);

      MemWordWrite (lpVideo + offset, wTemp);
      offset = (offset ^ 0x2000) + (next * (1 - (height & 0x01)));
   }
}

//
//    CGA2CharOut - Write a character at a specific location in CGA 2-color
//
//    Entry:   chr      ASCII character code
//          color color
//          col      Column
//          row      Row
//          page  Page (must be 0)
//    Exit: None
//
//    Assume character height is an even number.
//
void CGA2CharOut (BYTE chr, BYTE color, BYTE col, BYTE row, BYTE page)
{
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;
   static SEGOFF  lpINT43 = (SEGOFF) 0x0000010C;
   static SEGOFF  lpCharHeight = (SEGOFF) 0x00000485;
   SEGOFF         lpVideo;
   LPBYTE         lpFont;
   WORD        offset, height, next;

   SetLwrsorPosition (col, row, page);
   lpVideo = (SEGOFF) (0xB8000000);
   next = MemWordRead (lpColumns);
   height = (WORD) MemByteRead (lpCharHeight);
   offset = (next * (WORD) row * height) / 2 + col;
   lpFont = (LPBYTE)(size_t)(MemDwordRead (lpINT43));
   lpFont += height * (WORD) chr;
   while (height--)
   {
      MemByteWrite (lpVideo + offset, *(lpFont++));
      offset = (offset ^ 0x2000) + (next * (1 - (height & 0x01)));
   }
}

//
//    MonoGrCharOut - Write a character at a specific location in 1 BPP mode
//
//    Entry:   chr      ASCII character code
//          attr  color
//          col      Column
//          row      Row
//          page  Page (must be 0)
//    Exit: None
//
//    Assume character height is an even number.
//
void MonoGrCharOut (BYTE chr, BYTE color, BYTE col, BYTE row, BYTE page)
{
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;
   static SEGOFF  lpINT43 = (SEGOFF) 0x0000010C;
   static SEGOFF  lpCharHeight = (SEGOFF) 0x00000485;
   LPBYTE         lpFont;
   WORD        height, next;

   height = (WORD) MemByteRead (lpCharHeight);
   next = MemWordRead (lpColumns);
   lpFont = (LPBYTE)(size_t)(MemDwordRead (lpINT43));
   lpFont += height * (WORD) chr;

   SetLwrsorPosition (col, row, page);
   DrawMonoChar (col, row, chr, lpFont, height, next);
}

//
//    VGACharOut - Write a character at a specific location in 256 color mode
//
//    Entry:   chr      ASCII character code
//          attr  color
//          col      Column
//          row      Row
//          page  Page (must be 0)
//    Exit: None
//
//    Assume character height is an even number.
//
void VGACharOut (BYTE chr, BYTE color, BYTE col, BYTE row, BYTE page)
{
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;
   static SEGOFF  lpINT43 = (SEGOFF) 0x0000010C;
   static SEGOFF  lpCharHeight = (SEGOFF) 0x00000485;
   SEGOFF         lpVideo;
   LPBYTE         lpFont;
   WORD           offset, height, next;
   BYTE           byTemp, byCount;

   SetLwrsorPosition (col, row, page);
   lpVideo = (SEGOFF) (0xA0000000);
   next = MemWordRead (lpColumns) * 8;
   height = (WORD) MemByteRead (lpCharHeight);
   offset = (next * (WORD) row * height) + (col * 8);
   lpFont = (LPBYTE)(size_t)(MemDwordRead (lpINT43));
   lpFont += height * (WORD) chr;
   next -= 8;
   lpVideo += offset;
   while (height--)
   {
      byTemp = *lpFont++;
      byCount = 8;
      while (byCount--)
      {
         if ((byTemp & 0x80) == 0x80)
            MemByteWrite (lpVideo++, color);
         else
            MemByteWrite (lpVideo++, 0);
         byTemp = byTemp << 1;
      }
      lpVideo += next;
   }
}

//
//    Line4 - Draw a line from pt A to pt B with a given color in 4-color mode
//
//    Entry:   x1       Starting X coordinate
//          y1       Starting Y coordinate
//          x2       Ending X coordinate
//          y2       Ending Y coordinate
//          color    Line color
//    Exit: None
//
void Line4 (WORD x1, WORD y1, WORD x2, WORD y2, BYTE color)
{
   WORD     errx, erry, major, count;
   int      delta_x, delta_y, next;
   DWORD    dwOffset;
   SEGOFF   lpVideo;
   BYTE     byShifter, byMask;

   _line_color = color;
   IOWordWrite (0x3C4, 0x0F02);                 // Enable all planes
   IOWordWrite (0x3CE, 0x0000);                 // Clear set/reset
   IOByteWrite (0x3CE, 0x01);
   IOByteWrite (0x3CF, (BYTE) (~_line_color));  // Set enable set/reset to current color
   IOByteWrite (0x3CE, 0x03);
   IOByteWrite (0x3CF, (BYTE) (_line_rop << 3));// Set rasterop
   IOWordWrite (0x3CE, 0x0005);                 // Write mode 0

   if (x1 == x2)                          // Vertical line
   {
      VLine4Internal (x1, y1, y2);
   }
   else if (y1 == y2)                     // Horizontal line
   {
      HLine4Internal (x1, y2, x2);
   }
   else
   {
      if (x1 > x2)                        // Limit drawing to one quadrant
      {
         SwapWords (&x1, &x2);
         SwapWords (&y1, &y2);
      }
      errx = 0;
      erry = 0;
      delta_x = x2 - x1;
      dwOffset = CalcStart4 (x1, y1, &byShifter);
      lpVideo = (SEGOFF) (0xA0000000) + dwOffset;
      next = (BYTE) _line_columns;
      delta_y = y2 - y1;
      if (delta_y < 0)
      {
         delta_y = -delta_y;
         next = -next;
      }
      major = __max (delta_x, delta_y);
      count = major + 1;
      byMask = 0x80 >> byShifter;
      while (count--)
      {
         IOByteWrite (0x3CE, 0x08);
         IOByteWrite (0x3CF, byMask);
         MemByteRead (lpVideo);
         MemByteWrite (lpVideo, 0xFF);
         errx += delta_x;
         erry += delta_y;
         if (errx >= major)
         {
            errx -= major;
            byMask = RotateByteRight (byMask, 1);
            if (++byShifter >= 8)
            {
               byShifter = 0;
               lpVideo++;
            }
         }
         if (erry >= major)
         {
            erry -= major;
            lpVideo += next;
         }
      }
   }
}

//
//    HLine4Internal - Internal horizontal line handler for 4 BPP mode
//
//    Entry:   x1    Starting X
//          y     Starting and ending Y
//          x2    Ending X
//    Exit: None
//
void HLine4Internal (WORD x1, WORD y, WORD x2)
{
   DWORD    dwOffset;
   WORD     cPix;
   BYTE     byShifter, byMask, byBitMask;
   SEGOFF   lpVideo;

   // Always go left to right
   if (x1 > x2)
      SwapWords (&x1, &x2);

   dwOffset = CalcStart4 (x1, y, &byShifter);
   lpVideo = (SEGOFF) (0xA0000000) + dwOffset;
   cPix = (x2 - x1) + 1;                  // Number of pixels to draw
   if (byShifter != 0)
   {
      byMask = (BYTE) (0xFF >> byShifter);
      if (cPix < (WORD) (8 - byShifter))  // Partial byte on both sides
      {
         byBitMask = 0xFF << ((8 - byShifter) - cPix);
         byBitMask = byBitMask & byMask;
         IOByteWrite (0x3CE, 0x08);
         IOByteWrite (0x3CF, byBitMask);
         MemByteRead (lpVideo);
         MemByteWrite (lpVideo, 0xFF);
         return;
      }
      // Partial byte (mask left side)
      IOByteWrite (0x3CE, 0x08);
      IOByteWrite (0x3CF, byMask);
      MemByteRead (lpVideo);
      MemByteWrite (lpVideo, 0xFF);
      cPix = cPix - (8 - byShifter);
      lpVideo++;
   }

   // Do a run of bytes
   IOWordWrite (0x3CE, 0xFF08);
   while (cPix >= 8)
   {
      MemByteRead (lpVideo);
      MemByteWrite (lpVideo, 0xFF);
      lpVideo++;
      cPix -= 8;
   }

   // Last byte
   if (cPix != 0)
   {
      byMask = (BYTE) ~(0xFF >> cPix);
      IOByteWrite (0x3CE, 0x08);
      IOByteWrite (0x3CF, byMask);
      MemByteRead (lpVideo);
      MemByteWrite (lpVideo, 0xFF);
   }
}

//
//    VLine4Internal - Internal vertical line handler for 4 BPP mode
//
//    Entry:   x     Starting and ending X
//          y1    Starting Y
//          y2    Ending Y
//    Exit: None
//
void VLine4Internal (WORD x, WORD y1, WORD y2)
{
   DWORD    dwOffset;
   SEGOFF   lpVideo;
   WORD     cPix;
   BYTE     byShifter, byMask;

   // Always go top to bottom
   if (y1 > y2)
      SwapWords (&y1, &y2);

   // Callwlate start address
   dwOffset = CalcStart4 (x, y1, &byShifter);
   lpVideo = (SEGOFF) (0xA0000000) + dwOffset;

   // Callwlate number of pixels and bitmask
   cPix = (y2 - y1) + 1;
#if 0
   _asm {
      mov   ah,080h
      mov   cl,[byShifter]
      ror   ah,cl
      mov   [byMask],ah
   }
#else
   byMask = RotateByteRight (0x80, byShifter);
#endif

   IOByteWrite (0x3CE, 0x08);
   IOByteWrite (0x3CF, byMask);

   while (cPix--)
   {
      MemByteRead (lpVideo);
      MemByteWrite (lpVideo, 0xFF);
      lpVideo += (BYTE) _line_columns;
   }
}

//
//    CalcStart4 - Callwlate the start address in 4 BPP mode
//
//    Entry:   x     Starting X
//    Exit: y     Starting Y
//
DWORD CalcStart4 (WORD x, WORD y, LPBYTE byShifter)
{
   *byShifter = (BYTE) (x & 0x07);
   return ((DWORD) ((DWORD) _line_startaddr + ((DWORD) y * (DWORD) _line_columns) + (DWORD) (x / 8)));
}

//
//    SetLineColumns - Set row offset for the line routines
//
//    Entry:   columns     Row offset (in bytes)
//    Exit: None
//
void SetLine4Columns (WORD columns)
{
   _line_columns = columns;
}

//
//    SetLine4StartAddress - Set the start address for the line routines
//
//    Entry:   wStartAddr  Start address
//    Exit: None
//
void SetLine4StartAddress (WORD wStartAddr)
{
   _line_startaddr = wStartAddr;
}

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
