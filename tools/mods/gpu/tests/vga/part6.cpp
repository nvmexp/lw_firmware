//
//    PART6.CPP - VGA Core Test Suite (Part 6)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 15 November 2005
//
//    Routines in this file:
//    PaletteAddressSourceTest      Determine that the attribute controller index contains a bit (bit 5) that disables and enables the CRTC access to the internal (EGA-style) palette.
//    BlinkVsIntensityTest       Enable and disable the text attribute blink and intensity bit with a known pattern in video memory.
//    VideoStatusTest               Set various palette data and read it back from the status register.
//    InternalPaletteTest           Set all possible internal palette values and verify that the proper index is sent to the RAMDAC.
//    SimulatedInternalPaletteTest  Handle the non-timing parts of verifying all possible internal palette values
//    OverscanTest               Place varying values in the overscan register and verify that the correct DAC location is addressed.
//    SimulatedOverscanTest         Handle the non-timing parts of place varying values in the overscan register and verifing that the correct DAC location is addressed.
//    V54Test                    Draw a pattern into memory and change the palette by enabling the V54 select bit and switching through four "sub-pages" of color data.
//    V67Test                    Draw a pattern into memory and change the palette by changing the "color page".
//    PixelWidthTest             Draw a sequence of vertical lines and double the line width by setting the pixel data to change on every other dot clock.
//    ColorPlaneEnableTest       Fill video memory and cycle through all possible color plane enable values (16).
//    GraphicsModeBlinkTest         Verify that the blink enable bit works in graphics mode.
//
#include <stdio.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
extern   BOOL  _bSyncBeforeCapture;
#endif

// Function prototypes of the global tests
int PaletteAddressSourceTest (void);
int BlinkVsIntensityTest (void);
int VideoStatusTest (void);
int InternalPaletteTest (void);
int SimulatedInternalPaletteTest (void);
int OverscanTest (void);
int SimulatedOverscanTest (void);
int V54Test (void);
int V67Test (void);
int PixelWidthTest (void);
int ColorPlaneEnableTest (void);
int GraphicsModeBlinkTest (void);

// Structures needed by some tests
typedef struct tagMUXDATA
{
   BYTE  bit0;
   BYTE  shl0;
   BYTE  shr0;
   BYTE  bit1;
   BYTE  shl1;
   BYTE  shr1;
} MUXDATA;

#define  REF_PART    6
#define  REF_TEST    1
//
//    T0601
//    PaletteAddressSourceTest - Determine that the attribute controller index contains a bit (bit 5) that disables and enables the CRTC access to the internal (EGA-style) palette.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int PaletteAddressSourceTest (void)
{
   int      nErr;
   BOOL  bFullVGA;
   int      nRow, nCol, nRowInc;
   char  szMsg0[] = "Now you see it.                      ";
   char  szMsg1[] = "Now you don't.                       ";
   char  szMsg2[] = "Now you see it again (with overscan).";

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto PAST_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      nRow = 1;
      nCol = 1;
      nRowInc = 1;
   }
   else
   {
      nRow = 0;
      nCol = 0;
      nRowInc = 0;
   }

   SimSetState (TRUE, TRUE, FALSE);       // Ignore DAC writes
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   // Enabled
   SimComment (szMsg0);
   TextStringOut (szMsg0, ATTR_NORMAL, nCol, nRow, 0);
#ifdef LW_MODS
   _bSyncBeforeCapture = TRUE;
#endif
   WaitLwrsorBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto PAST_exit;
      }
   }
   nRow += nRowInc;

   SimDumpMemory ("T0601.VGA");

   // Disabled
   SimComment (szMsg1);
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x00);
   TextStringOut (szMsg1, ATTR_NORMAL, nCol, nRow, 0);
#ifdef LW_MODS
   _bSyncBeforeCapture = TRUE;
#endif
   WaitLwrsorBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto PAST_exit;
      }
   }
   nRow += nRowInc;

   // Blue screen with overscan
   SimComment (szMsg1);
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x11);
   IOByteWrite (ATC_INDEX, 0x01);
   WaitLwrsorBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto PAST_exit;
      }
   }

   // Re-enabled
   SimComment (szMsg2);
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x20);
   TextStringOut (szMsg2, ATTR_NORMAL, nCol, nRow, 0);
#ifdef LW_MODS
   _bSyncBeforeCapture = TRUE;
#endif
   WaitLwrsorBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

PAST_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Palette Address Source Test completed, images produced.");
   else
      SimComment ("Palette Address Source Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T0602
//    BlinkVsIntensityTest - Enable and disable the text attribute blink and intensity bit with a known pattern in video memory.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int BlinkVsIntensityTest (void)
{
   int      nErr, i, j;
   SEGOFF   lpVideo;
   BYTE  chr;
   BOOL  bFullVGA;
   int      nRowCount, nColCount, nNextRow;
   BYTE  byCharStart;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto BlinkVsIntensityTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      lpVideo = (SEGOFF) 0xB8000650;         // 10 rows down, 8th column
      nRowCount = 4;
      nColCount = 64;
      nNextRow = 32;
      byCharStart = 0;
   }
   else
   {
      lpVideo = (SEGOFF) 0xB8000000;         // 0 rows down, 0th column
      nRowCount = 1;
      nColCount = 40;
      nNextRow = 0;
      byCharStart = 108;
   }

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);
   DisableLwrsor ();

   SimComment ("Load memory with a character pattern.");
   chr = byCharStart;
   for (i = 0; i < nRowCount; i++)           // 4 rows
   {
      for (j = 0; j < nColCount; j++)        // 64 words
      {
         MemByteWrite (lpVideo++, chr);      // ASCII character code
         MemByteWrite (lpVideo++, chr);      // Use it as the attribute as well
         chr++;
      }
      lpVideo += nNextRow;
   }

   SimDumpMemory ("T0602.VGA");

   SimComment ("Blink is on.");
   WaitAttrBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto BlinkVsIntensityTest_exit;
      }
   }

   SimComment ("Blink is off.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) & 0xF7));

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

BlinkVsIntensityTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Blink vs. Intensity Test completed, images produced.");
   else
      SimComment ("Blink vs. Intensity Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T0603
//    VideoStatusTest - Set various palette data and read it back from the status register.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int VideoStatusTest (void)
{
   static MUXDATA mx[] = {
      {0x01, 4, 0, 0x04, 3, 0},
      {0x10, 0, 0, 0x20, 0, 0},
      {0x02, 3, 0, 0x08, 2, 0},
      {0x40, 0, 2, 0x80, 0, 2}
   };
   BYTE     tblData[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
                        0x00, 0x55, 0xAA, 0xFF, 0xCC, 0x33, 0x66, 0x99};
   BYTE     temp, byExpected, byActual;
   int         nErr, i, j;
   WORD     wSimType;
   char     szBuffer[128];

   nErr = ERROR_NONE;

   wSimType = SimGetType ();

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);
   SimDumpMemory ("T0603.VGA");

   for (i = 0; i < 4; i++)
   {
      // Set video status mux
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x32);
      temp = IOByteRead (ATC_RDATA);
      IOByteWrite (ATC_INDEX, (BYTE) ((temp & 0x0F) | (i << 4)));

      sprintf (szBuffer, "ATC[32h] = %02Xh", ((temp & 0x0F) | (i << 4)));
      SimComment (szBuffer);

      for (j = 0; j < (int) sizeof (tblData); j++)
      {
         IOByteRead (INPUT_CSTATUS_1);
         IOByteWrite (ATC_INDEX, 0x00);            // Set palette bits 0-5
         IOByteWrite (ATC_INDEX, (BYTE) (tblData[j] & 0x3F));
         IOByteWrite (ATC_INDEX, 0x34);            // Set palette bits 6-7
         IOByteWrite (ATC_INDEX, (BYTE) ((tblData[j] & 0xC0) >> 4));
         IOByteWrite (ATC_INDEX, 0x31);            // Must include overscan
         IOByteWrite (ATC_INDEX, tblData[j]);

         sprintf (szBuffer, "ATC[0] = %02Xh; ATC[34h] = %02Xh; ATC[31h] = %02Xh", tblData[j] & 0x3F, (tblData[j] & 0xC0) >> 4, tblData[j]);
         SimComment (szBuffer);

         if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
         {
            FrameCapture (REF_PART, REF_TEST);
         }
         else
         {
            while (((byActual = IOByteRead (INPUT_CSTATUS_1)) & 1) == 1);
            byActual &= 0x30;
            byExpected = (((tblData[j] & mx[i].bit0) << mx[i].shl0) >> mx[i].shr0) |
                           (((tblData[j] & mx[i].bit1) << mx[i].shl1) >> mx[i].shr1);
            if (byActual != byExpected)
            {
               nErr = FlagError (ERROR_VIDEO, REF_PART, REF_TEST, 0, 0, byExpected, byActual);
               goto VideoStatusTest_exit;
            }
         }
      }
   }

VideoStatusTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Video Status Test completed, images produced.");
   else
      SimComment ("Video Status Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    4
//
//    T0604
//    InternalPaletteTest - Set all possible internal palette values and verify that the proper index is sent to the RAMDAC.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int InternalPaletteTest (void)
{
   int      nErr, i, j;
   SEGOFF   lpVideo;
   BYTE  byActual;
   WORD  wSimType;

   nErr = ERROR_NONE;

   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
      return (SimulatedInternalPaletteTest ());

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);
   SimDumpMemory ("T0604.VGA");

   for (i = 0; i < 16; i++)
   {
      // Fill memory with one value
      lpVideo = (SEGOFF) 0xA0000000;
      IOWordWrite (GDC_INDEX, 0x0205);       // Write mode 2
      for (j = 0; (WORD) j < (WORD) (640/8) * 480; j++)
         MemByteWrite (lpVideo++, (BYTE) i);

      IOWordWrite (GDC_INDEX, 0x0005);       // Write mode 0
      for (j = 0; j < 64; j++)
      {
         IOByteRead (INPUT_CSTATUS_1);
         IOByteWrite (ATC_INDEX, (BYTE) i);
         IOByteWrite (ATC_INDEX, (BYTE) j);  // Video data
         IOByteWrite (ATC_INDEX, 0x31);
         IOByteWrite (ATC_INDEX, (BYTE) j);  // Overscan = video data
         byActual = GetVideoData ();
         if (byActual != (BYTE) j)
         {
            nErr = FlagError (ERROR_VIDEO, REF_PART, REF_TEST, 0, 0, j, byActual);
            goto InternalPaletteTest_exit;
         }
      }
   }

InternalPaletteTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Internal Palette Test completed, images produced.");
   else
      SimComment ("Internal Palette Test failed.");
   SystemCleanUp ();
   return (nErr);
}

//
//    SimulatedInternalPaletteTest - Handle the non-timing parts of verifying all possible internal palette values
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SimulatedInternalPaletteTest (void)
{
   int      nErr, i, j;
   SEGOFF   lpVideo;
   BYTE  byData;

   nErr = ERROR_NONE;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);

   // Fill memory with one value
   SimComment ("Load memory with pattern");
   lpVideo = (SEGOFF) 0xA0000000;
   IOWordWrite (GDC_INDEX, 0x0205);          // Write mode 2
   for (i = 0; i < 16; i++)
   {
      for (j = 0; j < 43; j++)               // Write 43 bytes to create an odd pattern
         MemByteWrite (lpVideo++, (BYTE) i);
   }
   IOWordWrite (GDC_INDEX, 0x0005);          // Write mode 0
   SimDumpMemory ("T0604.VGA");

   byData = 0x00;
   for (i = 0; i < 64 / 16; i++)
   {
      IOByteRead (INPUT_CSTATUS_1);
      for (j = 0; j < 16; j++)
      {
         IOByteWrite (ATC_INDEX, (BYTE) j);
         IOByteWrite (ATC_INDEX, byData++);     // Video data
      }
      IOByteWrite (ATC_INDEX, 0x31);
      IOByteWrite (ATC_INDEX, byData);       // Overscan = last video data

      SimComment ("Capture block of video data");
      FrameCapture (REF_PART, REF_TEST);
   }

   if (nErr == ERROR_NONE)
      SimComment ("Internal Palette Test completed, images produced.");
   else
      SimComment ("Internal Palette Test failed.");
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    5
//
//    T0605
//    OverscanTest - Place varying values in the overscan register and verify that the correct DAC location is addressed.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int OverscanTest (void)
{
   int      nErr, i;
   BYTE  byActual;
   WORD  wSimType;

   nErr = ERROR_NONE;

   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
      return (SimulatedOverscanTest ());

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);

   for (i = 0; i < 64; i++)
   {
      SetDac ((BYTE) i, 0, 0, (BYTE) i);
      SetDac ((BYTE) (i + 64), 0, (BYTE) i, 0);
      SetDac ((BYTE) (i + 128), (BYTE) i, 0, 0);
      SetDac ((BYTE) (i + 192), (BYTE) i, (BYTE) i, (BYTE) i);
   }

   SimDumpMemory ("T0605.VGA");

   for (i = 0; i < 256; i++)
   {
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x11);            // Allow overscan to fill the screen
      IOByteWrite (ATC_INDEX, (BYTE) i);
      byActual = GetNolwideoData ();
      if (byActual != (BYTE) i)
      {
         nErr = FlagError (ERROR_VIDEO, REF_PART, REF_TEST, 0, 0, i, byActual);
         goto OverscanTest_exit;
      }
   }

OverscanTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Overscan Test completed, images produced.");
   else
      SimComment ("Overscan Test failed.");
   SystemCleanUp ();
   return (nErr);
}

//
//    SimulatedOverscanTest - Handle the non-timing parts of place varying values in the overscan register and verifing that the correct DAC location is addressed.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
typedef struct _tagOVERSCANDATA
{
   BYTE  value;         // Overscan value / DAC index
   BYTE  red;        // Red DAC value
   BYTE  green;         // Green DAC value
   BYTE  blue;       // Blue DAC value
} OVERSCANDATA;
//
//
int SimulatedOverscanTest (void)
{
   int      i;
   char  szBuffer[128];
   OVERSCANDATA   tblOSD[] =
   {
      {0x00, 0x15, 0x2A, 0x3F},
      {0x01, 0x00, 0x00, 0x3F},
      {0x02, 0x00, 0x3F, 0x00},
      {0x04, 0x00, 0x3F, 0x3F},
      {0x08, 0x3F, 0x00, 0x00},
      {0x10, 0x3F, 0x00, 0x3F},
      {0x20, 0x3F, 0x3F, 0x00},
      {0x40, 0x1F, 0x1F, 0x1F},
      {0x80, 0x2F, 0x2F, 0x2F},
      {0xFF, 0x3F, 0x3F, 0x3F}
   };
   static int     nSizeTblOSD = sizeof (tblOSD) / sizeof (OVERSCANDATA);

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);

   SimDumpMemory ("T0605.VGA");

   sprintf (szBuffer, "Setting %d different overscan values.", nSizeTblOSD);
   SimComment (szBuffer);
   for (i = 0; i < nSizeTblOSD; i++)
   {
      SetDac ((BYTE) tblOSD[i].value, tblOSD[i].red, tblOSD[i].green, tblOSD[i].blue);

      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x31);
      IOByteWrite (ATC_INDEX, (BYTE) tblOSD[i].value);

      sprintf (szBuffer, "Setting overscan to %d and DAC to %02Xh,%02Xh,%02Xh", tblOSD[i].value, tblOSD[i].red, tblOSD[i].green, tblOSD[i].blue);
      SimComment (szBuffer);
      FrameCapture (REF_PART, REF_TEST);
   }

   SimComment ("Overscan Test completed, images produced.");
   return ERROR_NONE;
}

#undef   REF_TEST
#define  REF_TEST    6
//
//    T0606
//    V54Test - Draw a pattern into memory and change the palette by enabling the V54 select bit and switching through four "sub-pages" of color data.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int V54Test (void)
{
   int         nErr, i, j;
   static BYTE dac[64][3];
   WORD     wXRes, wYRes, x;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto V54Test_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);

   // Load the DAC with a known pattern
   SimComment ("Load the DAC with a known pattern.");
   for (i = 0; i < 16; i++)
   {
      dac[i][0] = 0x3F;
      dac[i][1] = 0;
      dac[i][2] = 0;
      dac[i+16][0] = 0;
      dac[i+16][1] = 0x3F;
      dac[i+16][2] = 0;
      dac[i+32][0] = 0;
      dac[i+32][1] = 0;
      dac[i+32][2] = 0x3F;
      dac[i+48][0] = 0x3F;
      dac[i+48][1] = 0x3F;
      dac[i+48][2] = 0x3F;
   }
   dac[0][0] = 0;          // First location in each block needs to
   dac[16][1] = 0;         //  be 0 for background color to be black
   dac[32][2] = 0;
   dac[48][0] = 0;
   dac[48][1] = 0;
   dac[48][2] = 0;

   SetDacBlock ((LPBYTE) dac, 0, 64);

   SimDumpMemory ("T0606.VGA");

   // Load the attribute controller with "linear" values
   SimComment ("Load the attribute controller with \"linear\" values.");
   for (i = 0; i < 16; i++)
   {
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, (BYTE) i);
      IOByteWrite (ATC_INDEX, (BYTE) i);
   }
   IOByteWrite (ATC_INDEX, 0x20);         // Re-enable video

   // Draw lines
   SimComment ("Draw lines.");
   SetLine4Columns (wXRes / 8);
   for (x = 0, i = 0; i < (int) (wXRes / 10); i++, x+=10)
      Line4 (x, 0, x, wYRes - 1, (BYTE) (i%16));

   // Switch pages (first time through should have no effect)
   SimComment ("Switch pages (first time through should have no effect).");
   for (j = 0; j < 2; j++)
   {
      for (i = 0; i < 4; i++)
      {
         IOByteRead (INPUT_CSTATUS_1);
         IOByteWrite (ATC_INDEX, 0x34);
         IOByteWrite (ATC_INDEX, (BYTE) i);
         if (!FrameCapture (REF_PART, REF_TEST))
         {
            if (GetKey () == KEY_ESCAPE)
            {
               nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
               goto V54Test_exit;
            }
         }
      }
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x30);
      IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) | 0x80));
   }

V54Test_exit:
   if (nErr == ERROR_NONE)
      SimComment ("V54 Test completed, images produced.");
   else
      SimComment ("V54 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    7
//
//    T0607
//    V67Test - Draw a pattern into memory and change the palette by changing the "color page".
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int V67Test (void)
{
   int            nErr, i;
   static BYTE    dac[256][3];
   WORD        wXRes, wYRes, x;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto V67Test_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);

   // Load the DAC with a known pattern
   SimComment ("Load the DAC with a known pattern.");
   for (i = 0; i < 64; i++)
   {
      // First 64 locations are "normal"
      dac[i][0] = tbl16ColorDAC[i*3];
      dac[i][1] = tbl16ColorDAC[i*3 + 1];
      dac[i][2] = tbl16ColorDAC[i*3 + 2];

      // Next 64 are filled with red
      dac[i+64][0] = 0x3F;                // Red
      dac[i+64][1] = 0;
      dac[i+64][2] = 0;

      // Next 64 are filled with blue
      dac[i+128][0] = 0;                     // Blue
      dac[i+128][1] = 0x3F;
      dac[i+128][2] = 0;

      // Last 64 are filled with green
      dac[i+192][0] = 0;                     // Green
      dac[i+192][1] = 0;
      dac[i+192][2] = 0x3F;
   }
   dac[64][0] = 0;         // First location in each block needs to
   dac[128][1] = 0;        //  be 0 for background color to be black
   dac[192][2] = 0;
   SetDacBlock ((LPBYTE) dac, 0, 256);

   SimDumpMemory ("T0607.VGA");

   // Draw lines
   SimComment ("Draw lines.");
   SetLine4Columns (wXRes / 8);
   for (x = 0, i = 0; i < (int) (wXRes / 10); i++, x+=10)
      Line4 (x, 0, x, wYRes - 1, (BYTE) (i%16));

   // Switch pages (first time through should have no effect)
   SimComment ("Switch pages (first time through should have no effect).");
   for (i = 0; i < 4; i++)
   {
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x34);
      IOByteWrite (ATC_INDEX, (BYTE) (i << 2));
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto V67Test_exit;
         }
      }
   }

V67Test_exit:
   if (nErr == ERROR_NONE)
      SimComment ("V67 Test completed, images produced.");
   else
      SimComment ("V67 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    8
//
//    T0608
//    PixelWidthTest - Draw a sequence of vertical lines and double the line width by setting the pixel data to change on every other dot clock.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int PixelWidthTest (void)
{
   int      nErr, i;
   WORD  x, wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto PixelWidthTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Draw lines.");
   for (x = 0, i = 0; x < wXRes; x+=2, i++)
      Line4 (x, 0, x, wYRes - 1, (BYTE) (i%16));

   SimDumpMemory ("T0608.VGA");

   SimGetKey ();

   SimComment ("Change pixel width.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) | 0x40));

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

PixelWidthTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Pixel Width Test completed, images produced.");
   else
      SimComment ("Pixel Width Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    9
//
//    T0609
//    ColorPlaneEnableTest - Fill video memory and cycle through all possible color plane enable values (16).
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int ColorPlaneEnableTest (void)
{
   int         nErr, i;
   SEGOFF      lpVideo;
   WORD     wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto ColorPlaneEnableTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Fill video memory with 0xFF.");
   lpVideo = (SEGOFF) (0xA0000000);
   for (i = 0; (WORD) i < (WORD) (wXRes/8)*wYRes; i++)
      MemByteWrite (lpVideo++, 0xFF);

   SimDumpMemory ("T0609.VGA");

   SimComment ("Cycle thru the 16 colors.");
   for (i = 0; i < 16; i++)
   {
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x32);
      IOByteWrite (ATC_INDEX, (BYTE) i);

      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto ColorPlaneEnableTest_exit;
         }
      }
   }

ColorPlaneEnableTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Color Plane Enable Test completed, images produced.");
   else
      SimComment ("Color Plane Enable Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    10
//
//    T0610
//    GraphicsModeBlinkTest - Verify that the blink enable bit works in graphics mode.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int GraphicsModeBlinkTest (void)
{
   int         nErr, i, j;
   SEGOFF      lpVideo;
   WORD     wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto GraphicsModeBlinkTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);

   // Draw large stripes into memory
   SimComment ("Draw large stripes into memory.");
   lpVideo = (SEGOFF) (0xA0000000);
   IOByteWrite (GDC_INDEX, 0x05);
   IOByteWrite (GDC_DATA, (BYTE) (IOByteRead (GDC_DATA) | 2)); // Write mode 2
   for (i = 0; i < 16; i++)
   {
      for (j = 0; j < (int) (wXRes/8)*((BYTE)wYRes/16); j++)
         MemByteWrite (lpVideo++, (BYTE) i);
   }

   SimDumpMemory ("T0610.VGA");

   // Verify starting frame
   SimComment ("Verify starting frame.");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto GraphicsModeBlinkTest_exit;
      }
   }

   // Enable blinking (color graphics mode)
   SimComment ("Enable blinking (color graphics mode).");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) | 0x08));
   WaitAttrBlink (BLINK_OFF);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto GraphicsModeBlinkTest_exit;
      }
   }
   else
   {
      WaitAttrBlink (BLINK_ON);
      FrameCapture (REF_PART, REF_TEST);
   }

   // Enable monochrome attributes
   SimComment ("Enable monochrome attributes.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) | 0x02));
   WaitAttrBlink (BLINK_OFF);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto GraphicsModeBlinkTest_exit;
      }
   }
   else
   {
      WaitAttrBlink (BLINK_ON);
      FrameCapture (REF_PART, REF_TEST);
   }

GraphicsModeBlinkTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Graphics Mode Blink Test completed, images produced.");
   else
      SimComment ("Graphics Mode Blink Test failed.");
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
