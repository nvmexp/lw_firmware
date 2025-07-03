//
//    COREC1.CPP - VGA Core "C" Library File #2 (Font)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       17 February 1994
//    Last modified: 16 November 2005
//
//    Routines in this file:
//    PreFontLoad          Prepare VGA controller for font load
//    PostFontLoad         Undo preparation for font load
//    LoadFontGlyph        Load a specific character image
//    Load8x8              Load the 8x8 font into a specific block
//    Load8x14          Load the 8x14 font into a specific block
//    Load8x16          Load the 8x16 font into a specific block
//    LoadFont          Load a given font into a specific block
//    DrawTransparentChar     Draw a font image into graphics mode memory
//    DrawMonoChar         Draw a 1 bit per pixel graphics character into video memory
//    LoadFixup            Load specific 9 dot characters from a fixup table
//
//
#include <stdio.h>
#include <string.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

//
//    PreFontLoad - Prepare VGA controller for font load
//
//    Entry:   None
//    Exit: None
//
void PreFontLoad (void)
{
   BOOL  bSim;

   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;
   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x20)); // Blank screen
   if (!bSim) _disable ();
   IOByteWrite (SEQ_INDEX, 0x00);
   IOByteWrite (SEQ_DATA, 0x01);                         // Sync reset
   IOByteWrite (GDC_INDEX, 0x05);
   IOByteWrite (GDC_DATA, 0x00);
   IOByteWrite (GDC_INDEX, 0x06);
   IOByteWrite (GDC_DATA, 0x05);
   IOByteWrite (SEQ_INDEX, 0x02);
   IOByteWrite (SEQ_DATA, 0x04);
   IOByteWrite (SEQ_INDEX, 0x04);
   IOByteWrite (SEQ_DATA, 0x06);
   IOByteWrite (SEQ_INDEX, 0x00);
   IOByteWrite (SEQ_DATA, 0x03);                         // End sync reset
   if (!bSim) _enable ();
}

//
//    PostFontLoad - Undo preparation for font load
//
//    Entry:   None
//    Exit: None
//
void PostFontLoad (void)
{
   BYTE  gdc06;
   BOOL  bSim;

   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

   gdc06 = 0x0E;                                      // Assume B800h
   if ((IOByteRead (MISC_INPUT) & 0x01) == 0) gdc06 = 0x0A;    // Nope, set to B000h

   if (!bSim) _disable ();
   IOByteWrite (SEQ_INDEX, 0x00);
   IOByteWrite (SEQ_DATA, 0x01);                         // Sync reset
   IOByteWrite (GDC_INDEX, 0x05);
   IOByteWrite (GDC_DATA, 0x10);
   IOByteWrite (GDC_INDEX, 0x06);
   IOByteWrite (GDC_DATA, gdc06);
   IOByteWrite (SEQ_INDEX, 0x02);
   IOByteWrite (SEQ_DATA, 0x03);
   IOByteWrite (SEQ_INDEX, 0x04);
   IOByteWrite (SEQ_DATA, 0x02);
   IOByteWrite (SEQ_INDEX, 0x00);
   IOByteWrite (SEQ_DATA, 0x03);                         // End sync reset
   if (!bSim) _enable ();
   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) & 0xDF)); // Unblank screen
}

//
//    LoadFontGlyph - Load a specific character image
//
//    Entry:   chr         Character position within font memory
//          height      Height of character (bytes per glyph)
//          block    Font block
//          lpGlyph     Pointer to glyph data
//    Exit: None
//
void LoadFontGlyph (BYTE chr, BYTE height, BYTE block, LPBYTE lpGlyph)
{
   SEGOFF   lpVideo;
   BYTE  byClear, i;

   PreFontLoad ();

   lpVideo = (SEGOFF) 0xA0000000;
   lpVideo += tblFontBlock[block] + (chr * 32);
   byClear = 32 - height;

   for (i = 0; i < height; i++)
      MemByteWrite (lpVideo++, *lpGlyph++);
   for (i = 0; i < byClear; i++)
      MemByteWrite (lpVideo++, 0);

   PostFontLoad ();
}

//
//    Load8x8 - Load the 8x8 font into a specific block
//
//    Entry:   block    Font block
//    Exit: None
//
void Load8x8 (BYTE block)
{
   LoadFont (tblFont8x8, 8, 0, 256, block);
}

//
//    Load8x14 - Load the 8x14 font into a specific block
//
//    Entry:   block    Font block
//    Exit: None
//
void Load8x14 (BYTE block)
{
   LoadFont (tblFont8x14, 14, 0, 256, block);
}

//
//    Load8x16 - Load the 8x16 font into a specific block
//
//    Entry:   block    Font block
//    Exit: None
//
void Load8x16 (BYTE block)
{
   LoadFont (tblFont8x16, 16, 0, 256, block);
}

//
//    LoadFont - Load a given font into a specific block
//
//    Entry:   lpFont      Address of font table
//          height      Number of bytes per character
//          start    Starting character code
//          count    Number of font images to load
//          block    Font block
//    Exit: None
//
void LoadFont (LPBYTE lpFont, BYTE height, BYTE start, WORD count, BYTE block)
{
   SEGOFF   lpVideo;
   BYTE     byClear, i;

   PreFontLoad ();

   lpVideo = (SEGOFF) 0xA0000000;
   lpVideo += tblFontBlock[block] + (((int) start) * 32);
   lpFont += ((int) start) * ((int) height);
   byClear = 32 - height;

   while (count-- > 0)
   {
      for (i = 0; i < height; i++)
         MemByteWrite (lpVideo++, *lpFont++);
      for (i = 0; i < byClear; i++)
         MemByteWrite (lpVideo++, 0);
   }

   PostFontLoad ();
}

//
//    DrawTranparentChar - Draw a font image into graphics mode memory
//
//    Entry:   x        X position
//          y        Y position
//          chr         ASCII character code
//          color    Color of image
//          height      Font height
//          fptr     Font data
//    Exit: None
//
void DrawTransparentChar (WORD x, WORD y, BYTE chr, BYTE color, BYTE height, LPBYTE lpFont)
{
   BYTE        orgSEQ02, orgGDC00, orgGDC05, orgGDC08;
   BYTE        lmask, rmask, ldata, rdata, lshift, rshift;
   SEGOFF         lpVideo;
   WORD        wNextRow;
   static SEGOFF  lpColumns = (SEGOFF) 0x0000044A;

   wNextRow = MemWordRead (lpColumns);

   IOByteWrite (SEQ_INDEX, 0x02);
   orgSEQ02 = (BYTE) IOByteRead (SEQ_DATA);
   IOByteWrite (SEQ_DATA, 0x0F);                      // Enable all planes

   IOByteWrite (GDC_INDEX, 0x00);
   orgGDC00 = (BYTE) IOByteRead (GDC_DATA);
   IOByteWrite (GDC_DATA, color);                     // Set/reset

   IOByteWrite (GDC_INDEX, 0x05);
   orgGDC05 = (BYTE) IOByteRead (GDC_DATA);
   IOByteWrite (GDC_DATA, (BYTE) (orgGDC05 | 0x03));  // Write mode 3

   IOByteWrite (GDC_INDEX, 0x08);
   orgGDC08 = (BYTE) IOByteRead (GDC_DATA);

   // Characters may span two bytes
   lpVideo = (SEGOFF) 0xA0000000;
   lpVideo += (wNextRow * y) + (x / 8);
   lshift = (BYTE) (x & 7);
   rshift = (BYTE) (8 - lshift);
   lmask = (BYTE) ((0xFF >> lshift));
   rmask = (BYTE) (~lmask);
   lpFont += height * chr;

   // Draw the character
   while (height > 0)
   {
      ldata = (BYTE) ((*lpFont) >> lshift);
      rdata = (BYTE) ((*lpFont) << rshift);
      if (ldata)
      {
         IOByteWrite (GDC_INDEX, 0x08);
         IOByteWrite (GDC_DATA, lmask);
         MemByteRead (lpVideo);           // Load the latches
         MemByteWrite (lpVideo, ldata);
      }

      if (rdata)
      {
         IOByteWrite (GDC_INDEX, 0x08);
         IOByteWrite (GDC_DATA, rmask);
         MemByteRead (lpVideo + 1);       // Load the latches
         MemByteWrite (lpVideo + 1, rdata);
      }

      height--;
      lpFont++;
      lpVideo += wNextRow;
   }

   IOByteWrite (SEQ_INDEX, 0x02);
   IOByteWrite (SEQ_DATA, orgSEQ02);

   IOByteWrite (GDC_INDEX, 0x00);
   IOByteWrite (GDC_DATA, orgGDC00);

   IOByteWrite (GDC_INDEX, 0x05);
   IOByteWrite (GDC_DATA, orgGDC05);

   IOByteWrite (GDC_INDEX, 0x08);
   IOByteWrite (GDC_DATA, orgGDC08);
}

//
//    DrawMonoChar - Draw a 1 bit per pixel graphics character into video memory
//
//    Entry:   col      Column position
//          row      Row position
//          chr      ASCII character
//          lpFont   Pointer to font table
//          height   Character height
//          rowoff   Row offset
//    Exit: None
//
void DrawMonoChar (WORD col, WORD row, BYTE chr, LPBYTE lpFont, WORD height, WORD rowoff)
{
   SEGOFF   lpVideo;

   lpVideo = (SEGOFF) 0xA0000000;
   lpVideo += (row * rowoff * height) + col;
   lpFont += chr * height;
   while (height--)
   {
      MemByteWrite (lpVideo, *lpFont++);
      lpVideo += rowoff;
   }
}

//
//    LoadFixup - Load specific 9 dot characters from a fixup table
//
//    Entry:   lpFixup     Pointer to fixup table
//          height      Number of bytes per character
//          block    Font block
//    Exit: None
//
void LoadFixup (LPBYTE lpFixup, BYTE height, BYTE block)
{
   static WORD tblBlock[] =
   {
      0x0000, 0x4000, 0x8000, 0xC000, 0x2000, 0x6000, 0xA000, 0xE000
   };
   int      nStart, i;
   SEGOFF   lpVideo;

   lpVideo = (SEGOFF) (0xA0000000 + (DWORD) tblBlock[block]);

   PreFontLoad ();

   while (*lpFixup)
   {
      nStart = (*lpFixup++) * 32;
      for (i = 0; i < (int) height; i++)
         MemByteWrite (lpVideo + nStart++, *lpFixup++);
   }

   PostFontLoad ();
}

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
