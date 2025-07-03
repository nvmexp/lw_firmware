//
//    PART4.CPP - VGA Core Test Suite (Part 4)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 15 November 2005
//
//    Routines in this file:
//    SyncResetTest              Set sync reset and verify system behavior (no retrace, SEQ[3] cleared).
//    SimulatedCPUMaxBandwidthTest  Handle the non-timing parts of set sync reset testing.
//    CPUMaxBandwidthTest           Time writes to video memory with CRTC enabled and with CRTC disabled.
//    SimulatedCPUMaxBandwidthTest  Handle the non-timing parts of CRTC enabled and CRTC disabled testing.
//    WriteMapReadPlaneTest         Write various values at various memory locations and compare the read value
//    Std256CharSetTest          Display every character in the font set
//    Ext512CharSetTest          Display every character in the font set
//    EightLoadedFontsTest       Load eight different fonts, display two at a time, and verify appropriate font was selected.
//    LoadUpsideDownFont            Load an entire font upside down into a specific block
//    LoadSidewaysFont           Load an entire font sideways into a specific block
//    Text64KTest                Setup a text mode that addresses A000h for 64K and set various display start values.
//    LineCharTest               Display the character set with the line graphics character bit enabled and disabled.
//    LargeCharTest              Display a large character set (16x32)
//    FontFetchStressTest           Stress the font fetching mechanism of the hardware
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int SyncResetTest (void);
int SimulatedSyncResetTest (void);
int CPUMaxBandwidthTest (void);
int SimulatedCPUMaxBandwidthTest (void);
int WriteMapReadPlaneTest (void);
int Std256CharSetTest (void);
int Ext512CharSetTest (void);
int EightLoadedFontsTest (void);
int Text64KTest (void);
int LineCharTest (void);
int LargeCharTest (void);
int FontFetchStressTest (void);

// Structures needed by some tests
typedef struct tagWRITETABLE
{
   WORD     offset;
   BYTE     mask;
   BYTE     data;
} WRITETABLE;

typedef struct tagLARGECHAR
{
   BYTE     chr;           // ASCII code
   BYTE     glyphL[32];       // Left font glyph
   BYTE     glyphR[32];       // Right font glyph
} LARGECHAR;

#define  REF_PART    4
#define  REF_TEST    1
//
//    T0401
//    SyncResetTest - Set sync reset and verify system behavior (no retrace, SEQ[3] cleared).
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SyncResetTest (void)
{
   int      nErr;
   BYTE  temp;
   WORD  wSimType;

   nErr = ERROR_NONE;

   // This test can not be simulated.
   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
      return (SimulatedSyncResetTest ());

   SetMode (0x03);
   TextStringOut ("The following test is self-running and will take about 5 seconds.", ATTR_NORMAL, 1, 1, 0);
   TextStringOut ("Press any key to continue, <ESC> to exit.", ATTR_NORMAL, 1, 3, 0);
   if (SimGetKey () == KEY_ESCAPE)
      goto SyncResetTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   // There should be retrace happening
   if (!WaitVerticalRetrace ())
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            LOWORD (70000l), HIWORD (70000l), 0, 0);
      goto SyncResetTest_exit;
   }

   SimDumpMemory ("T0401.VGA");

   IOWordWrite (SEQ_INDEX, 0x3F03);    // Set SEQ[3] to a non-zero value
   IOWordWrite (SEQ_INDEX, 0x0100);    // Sync reset
   if (WaitVerticalRetrace ())            // Should be no sync pulses
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            0, 0, LOWORD (70000l), HIWORD (70000l));
      goto SyncResetTest_exit;
   }
   IOByteWrite (SEQ_INDEX, 0x03);
   temp = (BYTE) IOByteRead (SEQ_DATA);
   if (temp != 0x3F)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, SEQ_INDEX, 0x03, 0x3F, temp);
      goto SyncResetTest_exit;
   }

   IOWordWrite (SEQ_INDEX, 0x0000);    // Async reset
   if (WaitVerticalRetrace ())            // Should still be no sync pulses
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            0, 0, LOWORD (70000l), HIWORD (70000l));
      goto SyncResetTest_exit;
   }
   IOByteWrite (SEQ_INDEX, 0x03);
   temp = (BYTE) IOByteRead (SEQ_DATA);
   if (temp != 0x00)                   // Should reset SEQ[3]
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, SEQ_INDEX, 0x03, 0x00, temp);
      goto SyncResetTest_exit;
   }

   IOWordWrite (SEQ_INDEX, 0x0300);    // End sync reset
   if (!WaitVerticalRetrace ())        // Should get sync pulses back
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            0, 0, LOWORD (70000l), HIWORD (70000l));
      goto SyncResetTest_exit;
   }

SyncResetTest_exit:
   IOWordWrite (SEQ_INDEX, 0x0300);    // End sync reset
   SystemCleanUp ();
   return (nErr);
}

//
//    SimulatedCPUMaxBandwidthTest - Handle the non-timing parts of set sync reset testing.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SimulatedSyncResetTest (void)
{
   int      nErr;
   BYTE  temp;

   nErr = ERROR_NONE;
   SimSetState (TRUE, TRUE, FALSE);       // Don't load RAMDAC
   SetMode (0x92);
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Set sync reset and verify SEQ[3] doesn't change.");
   IOWordWrite (SEQ_INDEX, 0x3F03);       // Set SEQ[3] to a non-zero value
   IOWordWrite (SEQ_INDEX, 0x0100);       // Sync reset
   IOByteWrite (SEQ_INDEX, 0x03);
   temp = (BYTE) IOByteRead (SEQ_DATA);
   if (temp != 0x3F)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, SEQ_INDEX, 0x03, 0x3F, temp);
      goto SimulatedSyncResetTest_exit;
   }

   SimComment ("Set sync reset and verify SEQ[3] resets to zero.");
   IOWordWrite (SEQ_INDEX, 0x0000);       // Async reset
   IOByteWrite (SEQ_INDEX, 0x03);
   temp = (BYTE) IOByteRead (SEQ_DATA);
   if (temp != 0x00)                   // Should reset SEQ[3]
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, SEQ_INDEX, 0x03, 0x00, temp);
      goto SimulatedSyncResetTest_exit;
   }

SimulatedSyncResetTest_exit:
   IOWordWrite (SEQ_INDEX, 0x0300);       // End sync reset
   if (nErr == ERROR_NONE)
      SimComment ("Sync Reset Test passed.");
   else
      SimComment ("Sync Reset Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T0402
//    CPUMaxBandwidthTest - Time writes to video memory with CRTC enabled and with CRTC disabled.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int CPUMaxBandwidthTest (void)
{
   int         nErr, nMemSize;
   SEGOFF      lpVideo;
   LPBYTE      lpBuffer;
   DWORD    actualtime, time0, time1, counter0, counter1;
   WORD     wSimType;

   nErr = ERROR_NONE;
   lpBuffer = NULL;           // Memory is not yet allocated

   // This test can not be simulated.
   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
      return (SimulatedCPUMaxBandwidthTest ());

   SetMode (0x03);
   TextStringOut ("The following test is self-running and will take about 10 seconds.", ATTR_NORMAL, 1, 1, 0);
   TextStringOut ("Press any key to continue, <ESC> to exit.", ATTR_NORMAL, 1, 3, 0);
   if (GetKey () == KEY_ESCAPE)
      goto CPUMaxBandwidthTest_exit;
   SetMode (0x03);

   // Allocate a buffer and fill it with the full block character
   nMemSize = 0x8000;
   lpVideo = (SEGOFF) 0xB8000000;
   lpBuffer = (LPBYTE) malloc (nMemSize);
   if (lpBuffer == NULL)
   {
      nErr = FlagError (ERROR_INTERNAL, REF_PART, REF_TEST, 0, 0, 0, 0);
      goto CPUMaxBandwidthTest_exit;
   }
   memset (lpBuffer, 0xDB, nMemSize);

   // Count the number of bytes that can be written in a given period of time
   actualtime = time0 = GetSystemTicks ();
   time1 = time0 + FIVE_SECONDS;
   counter0 = 0;
   while (actualtime < time1)
   {
      for (int i = 0; i < nMemSize; i++)
         *(LPBYTE)((size_t)(lpVideo + i)) = *(lpBuffer + i);
//       MemByteWrite (lpVideo + i, *(lpBuffer + i));
      actualtime = GetSystemTicks ();
      counter0++;
   }

   WaitAttrBlink (BLINK_ON);
   FrameCapture (REF_PART, REF_TEST);

   // Give the CPU maximum bandwidth
   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x20));

   actualtime = time0 = GetSystemTicks ();
   time1 = time0 + FIVE_SECONDS;
   counter1 = 0;
   while (actualtime < time1)
   {
      for (int i = 0; i < nMemSize; i++)
         *(LPBYTE)((size_t)(lpVideo + i)) = *(lpBuffer + i);
//       MemByteWrite (lpVideo + i, *(lpBuffer + i));
      actualtime = GetSystemTicks ();
      counter1++;
   }

   // There should be more updates with the screen disabled
   if (counter1 < counter0)
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);

   WaitAttrBlink (BLINK_ON);
   FrameCapture (REF_PART, REF_TEST);

CPUMaxBandwidthTest_exit:
   if (lpBuffer) free (lpBuffer);
   SystemCleanUp ();
   return (nErr);
}

//
//    SimulatedCPUMaxBandwidthTest - Handle the non-timing parts of CRTC enabled and CRTC disabled testing.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SimulatedCPUMaxBandwidthTest (void)
{
   int      nRow, nCol, nColCount, i;
   BOOL     bFullVGA;
   BYTE     chr, attr;

   // Init local variables
   bFullVGA = SimGetFrameSize ();

   // Set 640x480 planar mode
   SimSetState (TRUE, TRUE, FALSE);          // Don't load RAMDAC
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);

   // Set position variables based on screen size
   if (bFullVGA)
   {
      nRow = 14;
      nCol = 8;
      nColCount = 64;
   }
   else
   {
      nRow = 0;
      nCol = 0;
      nColCount = 40;
   }

   // Draw a pretty picture
   chr = 0x30;
   attr = 0x01;
   for (i = 0; i < nColCount; i++)
   {
      PlanarCharOut (chr, attr, (BYTE) nCol + i, (BYTE) nRow, 0);
      chr++; attr++;
      attr &= 0x0F;
   }
   SimDumpMemory ("T0402.VGA");

   // Screen on image
   SimComment ("Capture image - screen on");
   FrameCapture (REF_PART, REF_TEST);

   // Give the CPU maximum bandwidth
   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x20));

   // Screen off image
   SimComment ("Capture image - screen off");
   FrameCapture (REF_PART, REF_TEST);

   // Restore the screen
   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) & 0xDF));

   // Bright white overscan
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x31);
   IOByteWrite (ATC_INDEX, 0x3F);

   // Screen on image, with overscan
   SimComment ("Capture image - screen on, with overscan");
   FrameCapture (REF_PART, REF_TEST);

   // Give the CPU maximum bandwidth
   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x20));

   // Screen off image, with overscan
   SimComment ("Capture image - screen off, with overscan");
   FrameCapture (REF_PART, REF_TEST);

   SimComment ("CPU Max Bandwidth Test completed, images produced.");
   SystemCleanUp ();
   return ERROR_NONE;
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T0403
//    WriteMapReadPlaneTest - Write various values at various memory locations and compare the read value
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int WriteMapReadPlaneTest (void)
{
   static WRITETABLE wrt[] = {
      {0x0000, 0x0F, 0xFF},
      {0x0002, 0x0A, 0x55},
      {0x0004, 0x05, 0xAA},
      {0x0008, 0x01, 0x5A},
      {0x0010, 0x02, 0xA5},
      {0x0020, 0x04, 0x99},
      {0x0040, 0x08, 0x66},
      {0x0080, 0x03, 0x69},
      {0x0100, 0x05, 0x96},
      {0x0200, 0x09, 0x01},
      {0x0400, 0x06, 0x02},
      {0x0800, 0x07, 0x04},
      {0x1000, 0x08, 0x08},
      {0x2000, 0x0B, 0x10},
      {0x4000, 0x0C, 0x20},
      {0x8000, 0x0D, 0x40},
      {0xC000, 0x0E, 0x80},
      {0xF000, 0x00, 0xFF},
      {0xFFFF, 0x0F, 0xFF},
   };
   int            nErr;
    unsigned int  i, n;
   SEGOFF         lpVideo;
   BYTE        temp0, temp1;
   BOOL        bFullVGA;
   char        szBuffer[128];

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();

   SimSetState (TRUE, TRUE, FALSE);          // Don't load RAMDAC
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);

   lpVideo = (SEGOFF) 0xA0000000;

   // Load video memory
   SimDumpMemory ("T0403.VGA");        // Don't dump memory after the load, because loading memory is the test
   SimComment ("Load video memory.");
   IOByteWrite (SEQ_INDEX, 0x02);
   for (i = 0; i < (sizeof (wrt) / sizeof (WRITETABLE)); i++)
   {
      IOByteWrite (SEQ_DATA, wrt[i].mask);
      MemByteWrite (lpVideo + wrt[i].offset, wrt[i].data);
   }

   // Verify video memory
   SimComment ("Verify video memory");
   for (n = 0; n < 4; n++)
   {
      sprintf (szBuffer, "Verifying plane %d", n);
      SimComment (szBuffer);

      lpVideo = (SEGOFF) 0xA0000000;
      IOByteWrite (GDC_INDEX, 0x04);
      IOByteWrite (GDC_DATA, (BYTE) n);
      for (i = 0; i < (sizeof (wrt) / sizeof (WRITETABLE)); i++)
      {
         while (LOWORD (lpVideo) < wrt[i].offset)
         {
            temp0 = MemByteRead (lpVideo++);
            if (temp0)           // Video data should be a 0
            {
               nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpVideo), HIWORD (lpVideo), 0, temp0);
               goto WriteMapReadPlaneTest_exit;
            }

            // Skip the full memory read in simulation
            if (!bFullVGA)
               lpVideo = (HIWORD (lpVideo) << 16) + wrt[i].offset;
         }
         temp0 = MemByteRead (lpVideo++);
         temp1 = wrt[i].data;
         if ((wrt[i].mask & (1 << n)) == 0) temp1 = 0;
         if (temp0 != temp1)
         {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpVideo), HIWORD (lpVideo), temp1, temp0);
            goto WriteMapReadPlaneTest_exit;
         }
      }
   }

WriteMapReadPlaneTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Write Map / Read Plane Test passed.");
   else
      SimComment ("Write Map / Read Plane Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    4
//
//    T0404
//    Std256CharSetTest - Display every character in the font set
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int Std256CharSetTest (void)
{
   int      nErr;
   SEGOFF   lpVideo;
   int      i, j;
   BYTE  chr;
   BOOL  bFullVGA;
   int      nRowCount, nColCount, nNextRow;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto Std256CharSetTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      lpVideo = (SEGOFF) 0xB8000650;         // 10 rows down, 8th column
      nRowCount = 4;
      nColCount = 64;
      nNextRow = 32;
   }
   else
   {
      lpVideo = (SEGOFF) 0xB8000000;         // 0 rows down, 0th column
      nRowCount = 1;
      nColCount = 40;
      nNextRow = 0;
   }

   SimSetState (TRUE, TRUE, FALSE);       // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Load video memory with character set.");
   DisableLwrsor ();
   chr = 0;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, (BYTE) (255 - chr));
         chr++;
      }
      lpVideo += nNextRow;
   }

   if (bFullVGA)
      TextStringOut ("9x16 Font:", 0x0F, 8, 9, 0);

   SimDumpMemory ("T0404.VGA");

   SimComment ("Show 256 character set.");
   WaitAttrBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }
   else
   {
      WaitAttrBlink (BLINK_OFF);
      FrameCapture (REF_PART, REF_TEST);
   }

Std256CharSetTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Standard 256 Character Set Test completed, images produced.");
   else
      SimComment ("Standard 256 Character Set Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    5
//
//    T0405
//    Ext512CharSetTest - Display every character in the font set
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
//    Note: Internal palette is left in the "default" state to show
//          that the pixel path still uses the intensity bit as an
//          index into the palette at the same time it is used as
//          a font select.
//
int Ext512CharSetTest (void)
{
   int      nErr;
   SEGOFF   lpVideo;
   int      i, j;
   BYTE  chr, attr;
   BOOL  bFullVGA;
   int      nRowCount, nColCount, nNextRow, nNextBlock;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto Ext512CharSetTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      lpVideo = (SEGOFF) 0xB8000330;         // 5 rows down, 8th column
      nRowCount = 4;
      nColCount = 64;
      nNextRow = 32;
      nNextBlock = 160;
   }
   else
   {
      lpVideo = (SEGOFF) 0xB8000000;         // 0 rows down, 0th column
      nRowCount = 1;
      nColCount = 13;
      nNextRow = 0;
      nNextBlock = 0;
   }

   SimSetState (TRUE, TRUE, FALSE);       // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);
   DisableLwrsor ();

   SimComment ("Load video memory with character set.");
   if (bFullVGA)
   {
      TextStringOut ("9x16 Font: (Map A)", 0x0F, 8, 4, 0);
      TextStringOut ("8x8 Font: (Map B)", 0x0F, 8, 9, 0);
      TextStringOut ("Alternating 9x16 and 8x8 Font: (Map A & B)", 0x0F, 8, 14, 0);
      TextStringOut ("Text cells are 9x16 -- Map A characters have intensity attributes set",
                  0x0F, 6, 20, 0);
   }

   // Draw characters 00h - 0FFh with attributes 00h - 0FFh (setting bit 3).
   // Map A (9x16 font) is shown.
   chr = 0;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, (BYTE) (chr | 0x08));
         chr++;
      }
      lpVideo += nNextRow;
   }

   // Draw characters 00h - 0FFh with attributes 00h - 0FFh (clearing bit 3)
   // Map B (8x8 font) is shown.
   lpVideo += nNextBlock;                 // Put a space between blocks of characters
   chr = 0;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, (BYTE) (chr & 0xF7));
         chr++;
      }
      lpVideo += nNextRow;
   }

   // Draw characters 00h - 0FFh with attributes 00h - 0FFh (alternating
   // setting and clearing bit 3). This will force every other character
   // to lookup a different font.
   lpVideo += nNextBlock;                 // Put a space between blocks of characters
   chr = 0;
   attr = 0x08;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, (BYTE) ((chr & 0xF7) | attr));
         chr++;
         attr ^= 0x08;     // If clear, set. If set, clear.
      }
      lpVideo += nNextRow;
   }

   // Disable blinking
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) & 0xF7));

   // Load 8x8 font into block 1 and set font blocks 0 & 1. Therefore,
   // map A has the 9x16 font (intensity bit = 1), and map B has the
   // 8x8 font (intensity bit = 0).
   SimComment ("Load 8x8 font into block 1 and set font blocks 0 & 1.");
   Load8x8 (1);
   SimDumpMemory ("T0405.VGA");
   IOWordWrite (SEQ_INDEX, 0x0103);

   SimComment ("Show 512 character set.");
   WaitAttrBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }
   else
   {
      WaitAttrBlink (BLINK_OFF);
      FrameCapture (REF_PART, REF_TEST);
   }

Ext512CharSetTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Extended 512 Character Set Test completed, images produced.");
   else
      SimComment ("Extended 512 Character Set Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    6
void LoadUpsideDownFont (LPBYTE, BYTE, BYTE, WORD, BYTE);
void LoadSidewaysFont (LPBYTE, BYTE, BYTE, WORD, BYTE, BOOL);
//
//    T0406
//    EightLoadedFontsTest - Load eight different fonts, display two at a time, and verify appropriate font was selected.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int EightLoadedFontsTest (void)
{
   static int  nSizes[] = {16, 14, 8, 16, 14, 8, 8, 8};
   static const char *szType[] = {
      "Normal", "Normal", "Normal", "Upside Down",
      "Upside Down", "Upside Down", "Sideways Left", "Sideways Right"
   };
   int         nErr;
   SEGOFF      lpVideo;
   int         i, j;
   BYTE     chr, tmp0, tmp1;
   BOOL     bFullVGA;
   int         nRowCount, nColCount, nNextRow, nNextBlock;
   char     szBuffer[128];

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto EightLoadedFontsTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      lpVideo = (SEGOFF) 0xB8000510;         // 8 rows down, 8th column
      nRowCount = 4;
      nColCount = 64;
      nNextRow = 32;
      nNextBlock = 320;
   }
   else
   {
      lpVideo = (SEGOFF) 0xB8000000;         // 0 rows down, 0th column
      nRowCount = 1;
      nColCount = 20;
      nNextRow = 0;
      nNextBlock = 0;
   }

   SimSetState (TRUE, TRUE, FALSE);          // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Load video memory with character sets.");
   DisableLwrsor ();

   // Draw characters 00h - 0FFh with attribute 0Fh (Map A)
   chr = 0;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, 0x0F);
         chr++;
      }
      lpVideo += nNextRow;
   }

   lpVideo += nNextBlock;                 // Put a gap between blocks of characters
   // Draw characters 00h - 0FFh with attribute 07h (Map B)
   chr = 0;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, 0x07);
         chr++;
      }
      lpVideo += nNextRow;
   }

   // Load various fonts into different font blocks
   SimComment ("Load 8x16 font into block 0.");
   Load8x16 (0);
   SimComment ("Load 8x14 font into block 1.");
   Load8x14 (1);
   SimComment ("Load 8x8 font into block 2.");
   Load8x8 (2);
   SimComment ("Load upside down 8x16 font into block 3.");
   LoadUpsideDownFont (tblFont8x16, 16, 0, 256, 3);
   SimComment ("Load upside down 8x14 font into block 4.");
   LoadUpsideDownFont (tblFont8x14, 14, 0, 256, 4);
   SimComment ("Load upside down 8x8 font into block 5.");
   LoadUpsideDownFont (tblFont8x8, 8, 0, 256, 5);
   SimComment ("Load sideways (left to right) 8x8 font into block 6.");
   LoadSidewaysFont (tblFont8x8, 8, 0, 256, 6, TRUE);
   SimComment ("Load sideways (right to left) 8x8 font into block 7.");
   LoadSidewaysFont (tblFont8x8, 8, 0, 256, 7, FALSE);

   // Tell the user what's going on
   if (bFullVGA)
   {
      TextStringOut ("Eight Loaded Fonts Test", 0x0F, 28, 0, 0);
      for (i = 0; i < 4; i++)
      {
         sprintf (szBuffer, "Block %d: %dx%d Font (%s)", i, 8, nSizes[i], szType[i]);
         TextStringOut (szBuffer, 0x0F, 1, (BYTE) (i + 2), 0);
         sprintf (szBuffer, "Block %d: %dx%d Font (%s)", i + 4, 8, nSizes[i + 4], szType[i + 4]);
         TextStringOut (szBuffer, 0x0F, 41, (BYTE) (i + 2), 0);
      }
   }
   SimDumpMemory ("T0406.VGA");

   // Set each block combination
   for (i = 0; i < 8; i++)
   {
      for (j = 0; j < 8; j++)
      {
         sprintf (szBuffer, "Set map A = block %d; map B = block %d", j, i);
         SimComment (szBuffer);

         tmp0 = ((i & 4) << 2) | (i & 3);
         tmp1 = ((j & 4) << 3) | ((j & 3) << 2);
         IOByteWrite (SEQ_INDEX, 0x03);
         IOByteWrite (SEQ_DATA, (BYTE) (tmp0 | tmp1));

         if (bFullVGA)
         {
            sprintf (szBuffer, "%dx%d Font: (Map A = %d) (%s)       ", 8, nSizes[j], j, szType[j]);
            TextStringOut (szBuffer, 0x0F, 8, 7, 0);
            sprintf (szBuffer, "%dx%d Font: (Map B = %d) (%s)       ", 8, nSizes[i], i, szType[i]);
            TextStringOut (szBuffer, 0x07, 8, 13, 0);
         }

         if (!FrameCapture (REF_PART, REF_TEST))
         {
            if (GetKey () == KEY_ESCAPE)
            {
               nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
               goto EightLoadedFontsTest_exit;
            }
         }
      }
   }

EightLoadedFontsTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Eight Loaded Fonts Test completed, images produced.");
   else
      SimComment ("Eight Loaded Fonts Test failed.");
   SystemCleanUp ();
   return (nErr);
}

//
//    LoadUpsideDownFont - Load an entire font upside down into a specific block
//
//    Entry:   lpFont      Address of font table
//          height      Number of bytes per character
//          start    Starting character code
//          count    Number of font images to load
//          block    Font block
//    Exit: None
//
void LoadUpsideDownFont (LPBYTE lpFont, BYTE height, BYTE start, WORD count, BYTE block)
{
   SEGOFF   lpVideo;
   BYTE  byClear, i;

   PreFontLoad ();

   lpVideo = (SEGOFF) 0xA0000000;
   lpVideo += tblFontBlock[block] + (((int) start) * 32);
   lpFont += ((int) start) * ((int) height);
   byClear = 32 - height;

   while (count-- > 0)
   {
      for (i = 0; i < height; i++)
      {
         MemByteWrite (lpVideo++, *(lpFont + (height - i) - 1));
      }
      lpFont += height;
      for (i = 0; i < byClear; i++)
         MemByteWrite (lpVideo++, 0);
   }

   PostFontLoad ();
}

//
//    LoadSidewaysFont - Load an entire font sideways into a specific block
//
//    Entry:   lpFont      Address of font table
//          height      Number of bytes per character
//          start    Starting character code
//          count    Number of font images to load
//          block    Font block
//          bLeft    Left to right flag (TRUE = left to right, FALSE = right to left)
//    Exit: None
//
void LoadSidewaysFont (LPBYTE lpFont, BYTE height, BYTE start, WORD count, BYTE block, BOOL bLeft)
{
   SEGOFF   lpVideo;
   BYTE  byClear, i, j, byTemp, byMask;
   BYTE  byGlyph[8];

   PreFontLoad ();

   lpVideo = (SEGOFF) 0xA0000000;
   lpVideo += tblFontBlock[block] + (((int) start) * 32);
   lpFont += ((int) start) * ((int) height);
   byClear = 32 - 8;

   while (count-- > 0)
   {
      // Build the glyph
      for (i = 0; i < 8; i++)
      {
         if (bLeft)
            byMask = 0x01 << i;
         else
            byMask = 0x80 >> i;
         byTemp = 0;
         for (j = 0; j < 8; j++)
         {
            if (bLeft)
            {
               byTemp = byTemp << 1;
               byTemp |= ((*(lpFont + j)) & byMask) >> i;
            }
            else
            {
               byTemp = byTemp >> 1;
               byTemp |= ((*(lpFont + j)) & byMask) << i;
            }
         }
         byGlyph[i] = byTemp;
      }
      // Draw the glyph
      for (i = 0; i < height; i++)
         MemByteWrite (lpVideo++, byGlyph[i]);
      for (i = 0; i < byClear; i++)
         MemByteWrite (lpVideo++, 0);
      lpFont += height;
   }

   PostFontLoad ();
}

#undef   REF_TEST
#define  REF_TEST    7
//
//    T0407
//    Text64KTest - Setup a text mode that addresses A000h for 64K and set various display start values.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int Text64KTest (void)
{
   int      nErr;
   SEGOFF   lpVideo;
   int      i, j, nPageLength;
   BYTE  chr;
   WORD  temp;
   BOOL  bFullVGA;
   int      nRowArrow, nColArrow;
   char  szBuffer[128];

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto Text64KTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      nRowArrow = 11;
      nColArrow = 39;
      nPageLength = 0x1000;
   }
   else
   {
      nRowArrow = 0;
      nColArrow = 19;
      nPageLength = 0x100;                // Real page is 0x1000, visible page is only 0xA0
   }

   SimSetState (TRUE, TRUE, FALSE);          // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   // Set video memory address to A0000h for 64K
   IOWordWrite (GDC_INDEX, 0x0206);
   lpVideo = (SEGOFF) 0xA0000000;

   // Fill video memory with the page number
   SimComment ("Fill video memory with the page number.");
   chr = '0';
   for (i = 0; i < 16; i++)
   {
      lpVideo = (SEGOFF) 0xA0000000 + (i * 0x1000);   // Start at each page boundary
      sprintf (szBuffer, "   Page %d.", i);
      SimComment (szBuffer);

      for (j = 0; j < nPageLength / 2; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, 0x07);
      }
      if (++chr == ('9' + 1)) chr = 'A';
   }

   SimDumpMemory ("T0407.VGA");

   // Cycle through each page
   SimComment ("Cycle through each page.");
   lpVideo = (SEGOFF) 0xA0000000;
   for (i = 0; i < 16; i++)
   {
      sprintf (szBuffer, "   Page %d.", i);
      SimComment (szBuffer);

      // Set screen start address
      temp = (0x1000/2) * i;
      IOByteWrite (CRTC_CINDEX, 0x0C);
      IOByteWrite (CRTC_CDATA, HIBYTE (temp));
      IOByteWrite (CRTC_CINDEX, 0x0D);
      IOByteWrite (CRTC_CDATA, LOBYTE (temp));

      // Set cursor start address
      temp += (80*nRowArrow + nColArrow);
      IOByteWrite (CRTC_CINDEX, 0x0E);
      IOByteWrite (CRTC_CDATA, HIBYTE (temp));
      IOByteWrite (CRTC_CINDEX, 0x0F);
      IOByteWrite (CRTC_CDATA, LOBYTE (temp));

      // Highlight the cursor by drawing an arrow to it and changing
      // the attribute to intensity.
      temp = temp << 1;
      MemByteWrite (lpVideo + temp++, 0x19);
      MemByteWrite (lpVideo + temp, 0x0F);

      // Verify the screen
      WaitLwrsorBlink (BLINK_ON);
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto Text64KTest_exit;
         }
      }
   }

Text64KTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Text 64K Test completed, images produced.");
   else
      SimComment ("Text 64K Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    8
//
//    T0408
//    LineCharTest - Display the character set with the line graphics character bit enabled and disabled.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int LineCharTest (void)
{
   int      nErr;
   SEGOFF   lpVideo;
   int      i, j;
   BYTE  chr, temp;
   BOOL  bFullVGA;
   int      nRowCount, nColCount, nNextRow;
   BYTE  byStartChar;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto LineCharTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      lpVideo = (SEGOFF) 0xB8000510;         // 8 rows down, 8th column
      nRowCount = 4;
      nColCount = 64;
      nNextRow = 32;
      byStartChar = 0;
   }
   else
   {
      lpVideo = (SEGOFF) 0xB8000000;         // 0 rows down, 0th column
      nRowCount = 1;
      nColCount = 40;
      nNextRow = 0;
      byStartChar = 0xC0;
   }

   SimSetState (TRUE, TRUE, FALSE);          // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   DisableLwrsor ();

   // Show character set using attributes from 00h to FFh
   SimComment ("Show character set using attributes from 00h to FFh.");
   chr = byStartChar;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, chr);
         chr++;
      }
      lpVideo += nNextRow;
   }

   if (bFullVGA)
   {
      // Show character set using attributes from 80h to FFh, 00h to 7Fh
      SimComment ("Show character set using attributes from 80h to FFh, 00h to 7Fh.");
      chr = byStartChar;
      for (i = 0; i < nRowCount; i++)
      {
         for (j = 0; j < nColCount; j++)
         {
            MemByteWrite (lpVideo++, chr);
            MemByteWrite (lpVideo++, (BYTE) (chr ^ 0x80));
            chr++;
         }
         lpVideo += nNextRow;
      }

      // Draw a box around the character set
      SimComment ("Draw a box around the character set.");
      lpVideo = (SEGOFF) 0xB800046E;                     // 7 rows down, 7th column
      for (i = 0; i < 128; i += 2)
      {
         MemByteWrite (lpVideo + i + 2, 205);            // Double horizontal bar character
         MemByteWrite (lpVideo + i + 160*9 + 2, 205);    // Double horizontal bar character
      }
      for (i = 0; i < 8; i++)
      {
         MemByteWrite (lpVideo + 160 + 160*i, 186);         // Double vertical bar character
         MemByteWrite (lpVideo + 160 + 65*2 + 160*i, 186);  // Double vertical bar character
      }
      MemByteWrite (lpVideo, 201);                    // Double upper left bar character
      MemByteWrite (lpVideo + 65*2, 187);                // Double upper right bar character
      MemByteWrite (lpVideo + 160*9, 200);               // Double lower left bar character
      MemByteWrite (lpVideo + 160*9 + 65*2, 188);           // Double upper right bar character
   }

   SimDumpMemory ("T0408.VGA");

   WaitAttrBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto LineCharTest_exit;
      }
   }

   // Turn off 9-dot expansion of line characters
   SimComment ("Turn off 9-dot expansion of line characters.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   temp = (BYTE) IOByteRead (ATC_RDATA);
   IOByteWrite (ATC_INDEX, (BYTE) (temp & 0xFB));

   WaitAttrBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

LineCharTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Line Character Test completed, images produced.");
   else
      SimComment ("Line Character Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    9
//
//    LargeCharTest - Display a large character set (16x32)
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int LargeCharTest (void)
{
   static LARGECHAR  lc[] = {
      {0x30,
         {0x00, 0x00, 0x00, 0x00, 0x07, 0x1F, 0x3F, 0x78,
          0xF0, 0xF0, 0xF0, 0xF0, 0xF3, 0xF3, 0xF3, 0xF3,
          0xF0, 0xF0, 0xF0, 0xF0, 0x78, 0x3F, 0x1F, 0x07,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
         {0x00, 0x00, 0x00, 0x00, 0x80, 0xE0, 0xF0, 0x78,
          0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C, 0x3C,
          0x3C, 0x3C, 0x3C, 0x3C, 0x78, 0xF0, 0xE0, 0x80,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
      },
      {0x31,
         {0x00, 0x00, 0x00, 0x00, 0x03, 0x07, 0x0F, 0x1F,
          0x3F, 0x3B, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
          0x03, 0x03, 0x03, 0x03, 0x03, 0x07, 0x3F, 0x3F,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
         {0x00, 0x00, 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xC0,
          0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
          0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xE0, 0xFC, 0xFC,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
      },
      {0x32,
         {0x00, 0x00, 0x00, 0x00, 0x3F, 0x7F, 0xF0, 0xF0,
          0xF0, 0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x0F,
          0x1F, 0x3E, 0x7C, 0xF8, 0xF8, 0xF8, 0xFF, 0xFF,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
         {0x00, 0x00, 0x00, 0x00, 0xF0, 0xF8, 0x3C, 0x3C,
          0x3C, 0x3C, 0x78, 0xF0, 0xF0, 0xE0, 0xC0, 0x80,
          0x00, 0x00, 0x00, 0x00, 0x3C, 0x3C, 0xFC, 0xFC,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
      },
      {0x33,
         {0x00, 0x00, 0x00, 0x00, 0x3F, 0x7F, 0xFF, 0xF0,
          0xF0, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x00,
          0x00, 0x00, 0x00, 0xF0, 0xF0, 0xFF, 0x7F, 0x3F,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
         {0x00, 0x00, 0x00, 0x00, 0xF8, 0xFC, 0xFE, 0x3E,
          0x3E, 0x3E, 0x3E, 0x7C, 0xF8, 0xF8, 0xFC, 0x3E,
          0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0xFE, 0xFC, 0xF8,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
      }
   };
   BYTE     cscans[] = {
         0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
         0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E,
         0x1D, 0x1B, 0x19, 0x17, 0x15, 0x13, 0x11, 0x0F,
         0x0D, 0x0B, 0x09, 0x07, 0x05, 0x03, 0x01, 0x20
   };
   BYTE     cscane[] = {
         0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
         0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F,
         0x1D, 0x1B, 0x19, 0x17, 0x15, 0x13, 0x11, 0x0F,
         0x0D, 0x0B, 0x09, 0x07, 0x05, 0x03, 0x01, 0x1F
   };
   int      nErr, nLC, n, i, j;
   SEGOFF   lpVideo;
   BYTE  attr;
   BOOL  bFullVGA;
   int      nRowCount, nColCount;
   char  szBuffer[128];

   nLC = sizeof (lc) / sizeof (LARGECHAR);
   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto LargeCharTest_exit;

   if (bFullVGA)
   {
      nRowCount = 12;
      nColCount = 40;
   }
   else
   {
      nRowCount = 1;
      nColCount = 20;
   }

   SimSetState (TRUE, TRUE, FALSE);    // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   // Set CRTC values
   SimComment ("Modify CRTC values.");
   IOByteWrite (MISC_OUTPUT, 0x63);    // 25 MHz clock
   IOWordWrite (SEQ_INDEX, 0x0101);    // 8-dot characters
   IOWordWrite (SEQ_INDEX, 0x0103);    // Block 0 & 1
   IOWordWrite (CRTC_CINDEX, 0x5F09);     // Character height = 32
   IOWordWrite (CRTC_CINDEX, 0x1D0A);     // Cursor scan start at 29
   IOWordWrite (CRTC_CINDEX, 0x1E0B);     // Cursor scan stop at 30
   IOWordWrite (CRTC_CINDEX, 0x0E0F);     // Cursor position
   if (bFullVGA)
      IOWordWrite (CRTC_CINDEX, 0x7F12);  // Display end after 12 rows
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x33);
   IOByteWrite (ATC_INDEX, 0x00);         // Pixel panning to 0
   IOByteWrite (ATC_INDEX, 0x32);
   IOByteWrite (ATC_INDEX, 0x07);

   lpVideo = (SEGOFF) 0xB8000000;

   // Load the glyphs
   SimComment ("Load the glyphs.");
   for (i = 0; i < nLC; i++)
   {
      LoadFontGlyph (lc[i].chr, 32, 0, &lc[i].glyphL[0]);
      LoadFontGlyph (lc[i].chr, 32, 1, &lc[i].glyphR[0]);
   }

   // Fill video memory with large characters
   SimComment ("Fill video memory with large characters.");
   n = 0;
   attr = 0x0;
   for (i = 0; i < nRowCount; i++)        // Rows
   {
      for (j = 0; j < nColCount; j++)     // Columns
      {
         MemByteWrite (lpVideo, lc[n].chr);
         MemByteWrite (lpVideo+1, (BYTE) (attr | 0x08));
         MemByteWrite (lpVideo+2, lc[n].chr);
         MemByteWrite (lpVideo+3, attr);
         if (++n >= nLC) n = 0;
         lpVideo += 4;
         if ((++attr) & 0x08) attr = (attr & 0xF7) + 0x10;
      }
   }

   SimDumpMemory ("T0409.VGA");

   WaitLwrsorBlink (BLINK_ON);
   WaitAttrBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto LargeCharTest_exit;
      }
   }

   // Walk through each underline location and set various cursor styles
   SimComment ("Walk through each underline location and set various cursor styles.");
   for (i = 0; i < 32; i++)
   {
      sprintf (szBuffer, "Set underline location (%d), and cursor scan start (%d) and end (%d) for frame %d", i, cscans[i], cscane[i], i);
      SimComment (szBuffer);

      IOByteWrite (CRTC_CINDEX, 0x14);
      IOByteWrite (CRTC_CDATA, (BYTE) i);       // Underline location
      IOByteWrite (CRTC_CINDEX, 0x0A);
      IOByteWrite (CRTC_CDATA, cscans[i]);      // Cursor scan start
      IOByteWrite (CRTC_CINDEX, 0x0B);
      IOByteWrite (CRTC_CDATA, cscane[i]);      // Cursor scan end
      WaitLwrsorBlink (BLINK_ON);
      WaitAttrBlink (BLINK_ON);
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto LargeCharTest_exit;
         }
      }
   }

LargeCharTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Large Character Test completed, images produced.");
   else
      SimComment ("Large Character Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    10
//
//    FontFetchStressTest - Stress the font fetching mechanism of the hardware
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int FontFetchStressTest (void)
{
   int      nErr;
   SEGOFF   lpVideo, lpFont1, lpFont2;
   int      i, j, k, nOffset;
   BYTE  chr, attr, byGlyphFlag, chrStart, attrStart, byFontSelect;
   BOOL  bFullVGA;
   int      nRowCount, nColCount;
   LPBYTE   pFont1, pFont2;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto FontFetchStressTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      lpVideo = (SEGOFF) 0xB8000000;
      nRowCount = 50;
      nColCount = 80;
   }
   else
   {
      lpVideo = (SEGOFF) 0xB8000000;
      nRowCount = 3;
      nColCount = 40;
   }

   SimSetState (TRUE, TRUE, FALSE);             // Don't load RAMDAC
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);
   DisableLwrsor ();

   //
   // The 8x8 font is loaded in pieces to allow the character to be generated in pieces.
   // The font is used two glyph lines at a time per character, so that it takes four
   // characters to make up one "real" character (vertically stacked).
   //
   // For example, assuming 80 column text, to display character 0, 1, and 2, the video
   // memory there could be:
   //
   //          CH A  CH A  CH A
   //    B8000 00 07 01 07 02 07...
   //    B80A0 20 07 21 07 22 07...
   //    B8140 40 07 41 07 42 07...
   //    B81E0 80 07 81 07 82 07...
   //
   // To create this effect, the font is loaded two scans at a time where the first
   // character is loaded at offset 0, the next two at offset 32, the next two at
   // offset 64, and the last two at offset 96. Note that each offset is multiplied
   // by 32 (the size of the glyph field used by the VGA hardware).
   //
   // To further "stress" the font loading mechanism, the alternate font select is used
   // to force the hardware to grab the glyph data from alternating locations in memory.
   //
   // Load the font
   //
   SimComment ("Load the font.");
   lpFont1 = (SEGOFF) 0xA0006000;
   lpFont2 = (SEGOFF) 0xA000E000;
   nOffset = 0;
   pFont1 = tblFont8x8 + (8 * '0');             // Start at character '0'
   pFont2 = tblFont8x8 + (8 * 'A');             // Start at character 'A'
   PreFontLoad ();
   for (i = 0; i < 10; i++)
   {
      for (j = 0; j < 2; j++)
      {
         MemByteWrite (lpFont1 + nOffset + j + (0x00 * 32), *(pFont1 + j + 0));
         MemByteWrite (lpFont1 + nOffset + j + (0x20 * 32), *(pFont1 + j + 2));
         MemByteWrite (lpFont1 + nOffset + j + (0x40 * 32), *(pFont1 + j + 4));
         MemByteWrite (lpFont1 + nOffset + j + (0x60 * 32), *(pFont1 + j + 6));
         MemByteWrite (lpFont2 + nOffset + j + (0x00 * 32), *(pFont2 + j + 0));
         MemByteWrite (lpFont2 + nOffset + j + (0x20 * 32), *(pFont2 + j + 2));
         MemByteWrite (lpFont2 + nOffset + j + (0x40 * 32), *(pFont2 + j + 4));
         MemByteWrite (lpFont2 + nOffset + j + (0x60 * 32), *(pFont2 + j + 6));
      }
      pFont1 += 8;                           // Next character
      pFont2 += 8;                           // Next character
      nOffset += 32;
   }
   PostFontLoad ();

   // Load the character / attributes
   SimComment ("Load the character / attributes.");
   chr = 0x00;
   attr = 0x01;
   for (i = 0; i < nRowCount; i++)                 // Number of rows
   {
      byGlyphFlag = 0;
      chrStart = i % 10;
      attrStart = attr;
      byFontSelect = (i & 1) * 0x08;               // Odd rows, start with alternate font
      for (j = 0; j < 4; j++)                   // Number of font pieces
      {
         chr = chrStart;
         attr = attrStart;
         for (k = 0; k < nColCount; k++)           // Number of columns
         {
            MemByteWrite (lpVideo++, chr | byGlyphFlag);
            attr |= byFontSelect;
            byFontSelect ^= 0x08;               // Alternate fonts on every character
            MemByteWrite (lpVideo++, attr);
            if (++chr > 9) chr = 0;
            attr = (attr & 0x07) + 1;
            if (attr > 7) attr = 0x01;
         }
         byGlyphFlag += 0x20;
      }
   }

   SimDumpMemory ("T0410.VGA");

   // Set up the registers
   SimComment ("Set up the registers.");
   IOWordWrite (CRTC_CINDEX, 0x4109);              // 2 scanline character height
   IOWordWrite (SEQ_INDEX, 0x3D03);             // Select font 3 and 7

   // Capture the frame
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

FontFetchStressTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Font Fetch Stress Test completed, images produced.");
   else
      SimComment ("Font Fetch Stress Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
