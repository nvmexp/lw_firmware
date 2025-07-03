//
//    PART3.CPP - VGA Core Test Suite (Part 3)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 21 November 2005
//
//    Routines in this file:
//    LwrsorTypeTest       Display the cursor in various sizes next to a character block the same expected size.
//    LwrsorLocationTest      Display the cursor at various positions on the screen and prompt the user for correct location.
//    LwrsorDisableTest    Display the text mode cursor enable/disable functionality
//    SyncPulseTimingTest     Set a given video timing and then alter the sync position.
//    TextModeSkewTest     Set a known graphics mode video timing and compare the image after setting the text mode bit.
//    VerticalTimesTwoTest Set a known mode using different sets of CRTC values (one normal, the other vert times 2)
//    DotClock2Test        Set a known video timing and verify that the dot clock is divided by 2.
//    Vload2Vload4Test     Verify the combination of VLoad/N and Count by N
//    CharWidthTest        Set a known 9-dot mode, change it to 8-dot mode and measure the results. The line rate should increase about 12.5% from 31.5 KHz to 35.4 Khz.
//    CRTCWriteProtectTest Verify that the lower CRTC registers cannot be written when write protect is enabled.
//    DoubleScanTest       Fill the screen with a pattern of horizontal lines and set the double scan bit.
//    VerticalInterruptTest   Set a known timing and count the number of vertical periods in a given time.
//    T0312_irq2handler    Handler for the IRQ2 vector
//    PanningTest          Scroll an image that is larger than visible memory.
//    UnderLineLocationTest   Set various values for the underline location
//    SyncDisableTest         Disable syncs and verify that no pulses are generated.
//    LineCompareTest         Smooth scroll a split screen window to the top of the display
//    Panning256Test       In mode 13h, pan by half characters
//    CountBy4Test         In mode 12h, set a known pattern and visually inspect the count by 2, count by 4 functionality.
//    SkewTest          In mode 12h, set a known pattern and visually inspect the sync skew and display skew.
//    PresetRowScanTest    In mode 3, smooth scroll a screen.
//    LwrsorSkewTest       In mode 03h, set each of the four cursor skew positions
//    NonBIOSLineCompareTest  Pan a split-screen using the Renaissance CRTC values
//
#include <stdio.h>
#include <stdlib.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
#include "crccheck.h"
#include "core/include/disp_test.h"
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int LwrsorTypeTest (void);
int LwrsorLocationTest (void);
int LwrsorDisableTest (void);
int SyncPulseTimingTest (void);
int TextModeSkewTest (void);
int VerticalTimesTwoTest (void);
int DotClock2Test (void);
int Vload2Vload4Test (void);
int CharWidthTest (void);
int CRTCWriteProtectTest (void);
int DoubleScanTest (void);
int VerticalInterruptTest (void);
int PanningTest (void);
int UnderLineLocationTest (void);
int SyncDisableTest (void);
int LineCompareTest (void);
int Panning256Test (void);
int CountBy4Test (void);
int SkewTest (void);
int PresetRowScanTest (void);
int LwrsorSkewTest (void);
int NonBIOSLineCompareTest (void);

// Structures needed by some tests
typedef struct tagLWRSORIMAGE
{
   BYTE     chr;
   BYTE     start;
   BYTE     stop;
   BYTE     offset;
   BYTE     glyph[16];
} LWRSORIMAGE;

typedef struct tagLWRSORPOS
{
   BYTE     page;          // Page # (0 - 7)
   BYTE     col;           // Left = 0, Right side = 1
   BYTE     row;           // Top = 0, Bottom = 1
   int         deltacol;         // Added to col (can be negative)
   int         deltarow;         // Added to row (can be negative)
} LWRSORPOS;

typedef struct tagDACTABLE
{
   BYTE  idx;
   BYTE  red;
   BYTE  green;
   BYTE  blue;
} DACTABLE;

static int  nArguments = 0;         // For now - LGC

#define  REF_PART    3
#define  REF_TEST    1
//
//    T0301
//    LwrsorTypeTest - Display the cursor in various sizes next to a character block the same expected size.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int LwrsorTypeTest (void)
{
   static LWRSORIMAGE   ci[] = {
      {128, 0, 0, 1,
         {0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {129, 0, 1, 1,
         {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {130, 0, 2, 1,
         {0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {131, 1, 1, 1,
         {0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {132, 6, 7, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {133, 13, 13, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00}},
      {134, 13, 14, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00}},
      {135, 13, 15, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF}},
      {136, 14, 15, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF}},
      {137, 15, 15, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}},
      {138, 15, 0, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {139, 7, 6, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {140, 0, 15, 1,
         {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}},
      {141, 46, 15, 1,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {191, 9, 10, 0,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {192, 9, 10, 0,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
      {193, 4, 13, 0,
         {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}
   };
   int      nCI, i, nErr;
   BYTE  nColumn, nRow, nColumnInc, nRowInc;
   BOOL  bFullVGA;
   char  szBuffer[128];

   nCI = sizeof (ci) / sizeof (LWRSORIMAGE);    // Number of entries in "ci"
   sprintf (szBuffer, "Number of cursor type tests: %d", nCI);
   SimComment (szBuffer);

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto LwrsorTypeTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      nColumn = 40;
      nRow = 4;
      nColumnInc = 0;
      nRowInc = 1;
   }
   else
   {
      nColumn = 0;
      nRow = 0;
      nColumnInc = 2;
      nRowInc = 0;
   }

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   if (bFullVGA)
      TextStringOut ("Compare cursor to image. Press <ENTER> if correct, <ESC> if error.", ATTR_NORMAL, 1, 1, 0);

   // Load the font images
   for (i = 0; i < nCI; i++)
   {
      LoadFontGlyph (ci[i].chr, 16, 0, &ci[i].glyph[0]);
      TextCharOut (ci[i].chr, ATTR_NORMAL, (BYTE) (nColumn + (i * nColumnInc)), (BYTE) (nRow + (i * nRowInc)), 0);
   }

   // Setup to visually show 8-wide vs. 9-wide cursor for inspection
   if (bFullVGA)
   {
      TextStringOut ("8-pixel wide cursor: \xBF", ATTR_NORMAL, 19, 18, 0);
      TextStringOut ("9-pixel wide cursor: \xC0", ATTR_NORMAL, 19, 19, 0);
      TextStringOut ("Overlapping cursor: \xC1", ATTR_NORMAL, 20, 20, 0);
   }

   SimDumpMemory ("T0301.VGA");

   // Show cursor at each step of cursor type tests
   for (i = 0; i < nCI; i++)
   {
      sprintf (szBuffer, "Test start scan = %d, and stop scan = %d", ci[i].start, ci[i].stop);
      SimComment (szBuffer);

      IOByteWrite (CRTC_CINDEX, 0x0A);
      IOByteWrite (CRTC_CDATA, ci[i].start);
      IOByteWrite (CRTC_CINDEX, 0x0B);
      IOByteWrite (CRTC_CDATA, ci[i].stop);
      SetLwrsorPosition ((BYTE) (nColumn + ci[i].offset), nRow, 0);
      WaitLwrsorBlink (BLINK_ON);
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto LwrsorTypeTest_exit;
         }
      }
      nRow += nRowInc;
      nColumn += nColumnInc;
   }

LwrsorTypeTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T0302
//    LwrsorLocationTest - Display the cursor at various positions on the screen and prompt the user for correct location.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int LwrsorLocationTest (void)
{
   static LWRSORPOS  cp[] = {
      {0, 0, 0, 0, 0},
      {0, 1, 0, 0, 0},
      {0, 1, 1, 0, 0},
      {0, 0, 1, 0, 0},
      {0, 0, 0, 0, 1},
      {0, 1, 0, 0, 1},
      {4, 0, 0, 0, 0},
      {4, 1, 0, 0, 0},
      {4, 1, 1, 0, 0},
      {4, 0, 1, 0, 0},
      {4, 0, 0, 0, 1},
      {4, 1, 0, 0, 1}
   };
   BYTE  byChar;
   int      nErr, nCP, nPageSize, nRows, nColumns, nRowLength, i, row, col;
   WORD  offset;
   SEGOFF   lpVideo;
   BOOL  bFullVGA;
   char  szBuffer[128];

   nCP = sizeof (cp) / sizeof (LWRSORPOS);      // Number of entries in table
   byChar = '0';
   sprintf (szBuffer, "Number of cursor location tests: %d", nCP);
   SimComment (szBuffer);

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto LwrsorLocationTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      nRows = 25;
      nColumns = 80;
      nRowLength = nColumns*2;
   }
   else
   {
      nRows = 2;
      nColumns = 40;
      nRowLength = nColumns*2;
   }
   lpVideo = (SEGOFF) 0xB8000000;
   nPageSize = 0x1000;

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (0x03);

   if (bFullVGA)
   {
      TextStringOut ("Cursor will be placed at 12 locations, press <ESC> if an error oclwrs (Page 0).",
                     ATTR_NORMAL, 1, 1, 0);
      TextStringOut ("Cursor will be placed at 12 locations, press <ESC> if an error oclwrs (Page 4).",
                     ATTR_NORMAL, 1, 1, 4);
      if (SimGetKey () == KEY_ESCAPE)
         return (FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0));
   }

   SimDumpMemory ("T0302.VGA");

   // Make a full block cursor
   IOWordWrite (CRTC_CINDEX, 0x000A);
   IOWordWrite (CRTC_CINDEX, 0x0F0B);

   for (i = 0; i < nCP; i++)
   {
      sprintf (szBuffer, "Set page %d", cp[i].page);
      SimComment (szBuffer);

      offset = cp[i].page * (nPageSize / 2);
      IOByteWrite (CRTC_CINDEX, 0x0C);
      IOByteWrite (CRTC_CDATA, HIBYTE (offset));
      IOByteWrite (CRTC_CINDEX, 0x0D);
      IOByteWrite (CRTC_CDATA, LOBYTE (offset));

      row = cp[i].row * (nRows - 1) + cp[i].deltarow;
      col = cp[i].col * (nColumns - 1) + cp[i].deltacol;
      offset = (offset * 2) + (row * nRowLength) + (col * 2);

      sprintf (szBuffer, "Set cursor to col,row = %d,%d", col, row);
      SimComment (szBuffer);

      MemByteWrite (lpVideo + offset, byChar);
      if (++byChar == ('9' + 1)) byChar = 'A';
      offset = offset >> 1;
      IOByteWrite (CRTC_CINDEX, 0x0E);
      IOByteWrite (CRTC_CDATA, HIBYTE (offset));
      IOByteWrite (CRTC_CINDEX, 0x0F);
      IOByteWrite (CRTC_CDATA, LOBYTE (offset));

      WaitLwrsorBlink (BLINK_ON);
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            break;
         }
      }
   }

LwrsorLocationTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T0303
//    LwrsorDisableTest - Display the text mode cursor enable/disable functionality
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int LwrsorDisableTest (void)
{
   int      nErr;
   BYTE  temp;
   WORD  key;
   BOOL  bFullVGA;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto LwrsorDisableTest_exit;

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   IOByteWrite (CRTC_CINDEX, 0x0A);
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0xDF)) != 0)
      return (FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, CRTC_CINDEX, 0x0A, 0, temp));

   SimComment ("Show a block cursor.");
   IOWordWrite (CRTC_CINDEX, 0x000A);
   IOWordWrite (CRTC_CINDEX, 0x1F0B);

   if (bFullVGA)
      TextStringOut ("Does the cursor look like this block (Y/N)?: \xDB", ATTR_NORMAL, 0, 1, 0);
   else
      TextStringOut ("Block Cursor (\xDB) ", ATTR_NORMAL, 0, 0, 0);

   WaitLwrsorBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      key = GetKey ();
      if (key == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto LwrsorDisableTest_exit;
      }
      else if ((key == KEY_N) || (key == KEY_n))
      {
         nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto LwrsorDisableTest_exit;
      }
   }

   SimComment ("Disable the cursor.");
   IOWordWrite (CRTC_CINDEX, 0x200A);

   if (bFullVGA)
   {
      TextStringOut ("Is the cursor disabled (Y/N)?", ATTR_NORMAL, 0, 3, 0);
      TextStringOut ("(Press \"N\" if there is an error)", ATTR_NORMAL, 0, 4, 0);
   }
   else
      TextStringOut ("Disabled: ", ATTR_NORMAL, 20, 0, 0);

   WaitLwrsorBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      key = GetKey ();
      if (key == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
      }
      else if ((key == KEY_N) || (key == KEY_n))
      {
         nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      }
   }
   SimDumpMemory ("T0303.VGA");

LwrsorDisableTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    4
//
//    T0304
//    SyncPulseTimingTest - Set a given video timing and then alter the sync position.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SyncPulseTimingTest (void)
{
   int      nErr, i, nCount;
   WORD  wXRes, wYRes;
   BYTE  bySyncS, bySyncE;
   BOOL  bFullVGA;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto SyncPulseTimingTest_exit;

   if (bFullVGA)
   {
      bySyncS = 0x50;
      bySyncE = 0x9C;
      nCount = 7;
   }
   else
   {
      bySyncS = 0x28;
      bySyncE = 0x8D;
      nCount = 4;
   }

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);
   GetResolution (&wXRes, &wYRes);

   SimComment ("Draw some lines into memory.");
   SetLine4Columns ((WORD) (wXRes / 8));
   Line4 (0, 0, (WORD) (wXRes - 1), 0, 0x0F);
   Line4 ((WORD) (wXRes - 1), 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
   Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
   Line4 (0, 0, 0, (WORD) (wYRes - 1), 0x0F);
   Line4 (0, 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
   Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), 0, 0x0F);
   SimDumpMemory ("T0304.VGA");

   IOByteWrite (CRTC_CINDEX, 0x11);
   IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));   // Disable write protect
   for (i = 0; i < nCount; i++)
   {
      SimComment ("Move the sync pulse.");
      IOByteWrite (CRTC_CINDEX, 0x04);
      IOByteWrite (CRTC_CDATA, bySyncS);
      IOByteWrite (CRTC_CINDEX, 0x05);
      IOByteWrite (CRTC_CDATA, bySyncE);
      bySyncS++;
      bySyncE = (bySyncE & 0xE0) | ((bySyncE + 1) & 0x1F);
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            break;
         }
      }
   }

SyncPulseTimingTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    5
//
//    T0305
//    TextModeSkewTest - Set a known graphics mode video timing and compare the image after setting the text mode bit.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int TextModeSkewTest (void)
{
   int      nErr;

   nErr = ERROR_NONE;

   SimSetState (TRUE, TRUE, FALSE);          // No RAMDAC writes
   SetMode (0x12);

   SimComment ("Turn on overscan.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x31);
   IOByteWrite (ATC_INDEX, 0x3F);            // Turn on overscan

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto TextModeSkewTest_exit;
      }
   }

   SimComment ("Treat pipeline as text.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, 0x00);         // Treat pixel pipeline as "text"

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto TextModeSkewTest_exit;
      }
   }

TextModeSkewTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    6
//
//    T0306
//    VerticalTimesTwoTest - Set a known mode using different sets of CRTC values (one normal, the other vert times 2)
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int VerticalTimesTwoTest (void)
{
   static WORD tblCRTCFull[] = {
      0x0011, 0x0506, 0x1107, 0xF510, 0x0611, 0xEF12, 0xF315, 0x0216, 0xE717
   };
   static WORD tblCRTCSmall[] = {
      0x0011, 0x0B06, 0x1007, 0x0A10, 0x0B11, 0x0912, 0x0A15, 0x0B16, 0xE717
   };
   int      nErr, i, ntbl;
   WORD  wXRes, wYRes, wYStart, wLinear;
   BOOL  bFullVGA;

   ntbl = sizeof (tblCRTCFull) / sizeof (WORD);
   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto VerticalTimesTwoTest_exit;

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);
   GetResolution (&wXRes, &wYRes);

   // Draw a pattern
   SimComment ("Draw a pattern into memory.");
   SetLine4Columns ((WORD) (wXRes / 8));
   Line4 (0, 0, (WORD) (wXRes - 1), 0, 0x0F);
   Line4 ((WORD) (wXRes - 1), 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
   Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
   Line4 (0, 0, 0, (WORD) (wYRes - 1), 0x0F);
   Line4 (0, 0, (WORD) (wXRes - 1), (WORD) (wYRes - 1), 0x0F);
   Line4 (0, (WORD) (wYRes - 1), (WORD) (wXRes - 1), 0, 0x0F);

   // Start the second image one scan line after the end of the first image
   SimComment ("Start the second image one scan line after the end of the first image.");
   wYStart = wYRes + 1;
   Line4 (0, wYStart, (WORD) (wXRes - 1), wYStart, 0x0F);
   Line4 ((WORD) (wXRes - 1), wYStart, (WORD) (wXRes - 1), (WORD) (wYStart + wYRes/2 - 1), 0x0F);
   Line4 (0, (WORD) (wYStart + wYRes/2 - 1), (WORD) (wXRes - 1), (WORD) (wYStart + wYRes/2 - 1), 0x0F);
   Line4 (0, wYStart, 0, (WORD) (wYStart + wYRes/2 - 1), 0x0F);
   Line4 (0, wYStart, (WORD) (wXRes - 1), (WORD) (wYStart + wYRes/2 - 1), 0x0F);
   Line4 (0, (WORD) (wYStart + wYRes/2 - 1), (WORD) (wXRes - 1), wYStart, 0x0F);

   SimDumpMemory ("T0306.VGA");

   SimComment ("Normal Image");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VerticalTimesTwoTest_exit;
      }
   }

   // Change the CRTC values
   SimComment ("Set Vertical X2 Values");
   for (i = 0; i < ntbl; i++)
   {
      if (bFullVGA)
         IOWordWrite (CRTC_CINDEX, tblCRTCFull[i]);
      else
         IOWordWrite (CRTC_CINDEX, tblCRTCSmall[i]);
   }

   SimComment ("Vertical X2 Image");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VerticalTimesTwoTest_exit;
      }
   }

   // Show interaction of vertical times two and double scan
   // Change the display start address to start at the second image
   wLinear = (wXRes / 8) * (wYStart);
   IOByteWrite (CRTC_CINDEX, 0x0C);
   IOByteWrite (CRTC_CDATA, HIBYTE (wLinear));
   IOByteWrite (CRTC_CINDEX, 0x0D);
   IOByteWrite (CRTC_CDATA, LOBYTE (wLinear));

   // Set double scan
   IOWordWrite (CRTC_CINDEX, 0xC009);
   WaitNotVerticalRetrace ();                   // Wait for start address to be latched at VSync start
   SimComment ("Vertical X2 Image with Double Scan");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

VerticalTimesTwoTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    7
//
//    T0307
//    DotClock2Test - Set a known video timing and verify that the dot clock is divided by 2.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int DotClock2Test (void)
{
   int      nErr;
   DWORD hz0, hz1;
   WORD  wSimType;

   nErr = ERROR_NONE;

   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
   {
      printf ("\n\nThis test ilwolves system timings and will not function aclwrately"
               "\nin a simulated environment.\n");
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      return (nErr);
   }

   SetMode (0x03);
   TextStringOut ("The following test is self-running and will take about 20 seconds.", ATTR_NORMAL, 1, 1, 0);
   TextStringOut ("During this test, the monitor may lose sync.", ATTR_NORMAL, 1, 2, 0);
   TextStringOut ("Press any key to continue, <ESC> to exit.", ATTR_NORMAL, 1, 4, 0);
   if (SimGetKey () == KEY_ESCAPE)
      goto DotClock2Test_exit;
   TextStringOut ("Please wait...", ATTR_BLINK, 1, 6, 0);

   // Verify clock is 28.322 MHz here
   hz0 = GetFrameRate ();           // Frame rate in hertz * 1000
   if (hz0 == 0)
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST, LOWORD (70000l), HIWORD (70000l), 0, 0);
      goto DotClock2Test_exit;
   }

   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x08));    // Clock/2

   // Verify clock is 14.161 MHz here
   // (Actually determine if the frame rate is approximately half the original
   // frame rate plus/minus 5 percent)
   hz1 = GetFrameRate ();
   if ((hz1 < (hz0 / 2 - hz0 / 20)) || (hz1 > (hz0 / 2 + hz0 / 20)))
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
               LOWORD (hz0 / 2), HIWORD (hz0 / 2), LOWORD (hz1), HIWORD (hz1));
      goto DotClock2Test_exit;
   }

   // Set a mode that uses the 25.175 MHz dot clock
   SetMode (0x12);

   // Verify clock is 25.175 MHz here
   hz0 = GetFrameRate ();           // Frame rate in hertz * 1000
   if (hz0 == 0)
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
               LOWORD (60000l), HIWORD (60000l), 0, 0);
      goto DotClock2Test_exit;
   }

   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (IOByteRead (SEQ_DATA) | 0x08));    // Clock/2

   // Verify clock is 12.087 MHz here
   // (Actually determine if the frame rate is approximately half the original
   // frame rate plus/minus 5 percent)
   hz1 = GetFrameRate ();
   if ((hz1 < (hz0 / 2 - hz0 / 20)) || (hz1 > (hz0 / 2 + hz0 / 20)))
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
               LOWORD (hz0 / 2), HIWORD (hz0 / 2), LOWORD (hz1), HIWORD (hz1));

DotClock2Test_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    8
//
//    T0308
//    Vload2Vload4Test - Verify the combination of VLoad/N and Count by N
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int Vload2Vload4Test (void)
{
   int      nErr;
   BYTE  temp;
   BYTE  byRow, byColumn, byChar;
   WORD  wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto VL2VL4_exit;

   SimSetState (TRUE, TRUE, FALSE);          // No RAMDAC writes
   SetMode (0x0D);
   SimSetState (TRUE, TRUE, TRUE);
   GetResolution (&wXRes, &wYRes);

   // Fill memory with characters
   SimComment ("Fill memory with characters");
   for (byRow = 0; byRow < ((wYRes / 8) + 1); byRow++)
   {
      byChar = '0';
      for (byColumn = 0; byColumn < (wXRes / 8); byColumn++)
      {
         PlanarCharOut (byChar++, 0x0F, byColumn, byRow, 0);
         if (byChar > '9') byChar = '0';
      }
   }

   SimDumpMemory ("T0308.VGA");

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VL2VL4_exit;
      }
   }

   // Set display enable skew to 1
   SimComment ("Set display enable skew to 1");
   IOByteWrite (CRTC_CINDEX, 0x11);
   IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x7F);
   IOByteWrite (CRTC_CINDEX, 0x03);
   IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) | 0x20);

   // Should be alternating characters of white & gray
   IOByteWrite (SEQ_INDEX, 0x01);
   temp = (BYTE) IOByteRead (SEQ_DATA);
   IOByteWrite (SEQ_DATA, (BYTE) (temp | 0x04));         // Set VLoad/2

   SimComment ("Should be alternating characters of white & gray.");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VL2VL4_exit;
      }
   }

   // Count by two
   SimComment ("Set count by two.");
   IOWordWrite (CRTC_CINDEX, 0xEB17);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VL2VL4_exit;
      }
   }

   // Count by four
   SimComment ("Set count by four.");
   IOWordWrite (CRTC_CINDEX, 0xE317);
   IOWordWrite (CRTC_CINDEX, 0x2014);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VL2VL4_exit;
      }
   }

   // Clear Count By N
   SimComment ("Clear Count By N.");
   IOWordWrite (CRTC_CINDEX, 0x0014);
   IOWordWrite (CRTC_CINDEX, 0xE317);

   // Set display enable skew to 3
   SimComment ("Set display enable skew to 3.");
   IOByteWrite (CRTC_CINDEX, 0x03);
   IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) | 0x60);

   // Should be alternating bars of white, gray, cyan, and blue
   IOByteWrite (SEQ_INDEX, 0x01);
   IOByteWrite (SEQ_DATA, (BYTE) (temp | 0x10));   // VLoad/4 has precedence over VLoad/2

   SimComment ("Should be alternating bars of white, gray, cyan, and blue.");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VL2VL4_exit;
      }
   }

   // Count by two
   SimComment ("Set count by two.");
   IOWordWrite (CRTC_CINDEX, 0xEB17);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VL2VL4_exit;
      }
   }

   // Count by four
   SimComment ("Set count by four.");
   IOWordWrite (CRTC_CINDEX, 0xE317);
   IOWordWrite (CRTC_CINDEX, 0x2014);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VL2VL4_exit;
      }
   }

VL2VL4_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    9
//
//    CharWidthTest - Set a known 9-dot mode, change it to 8-dot mode and measure the results. The line rate should increase about 12.5% from 31.5 KHz to 35.4 Khz.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int CharWidthTest (void)
{
   int      nErr;
   DWORD hz0, hz1, hz2;
   WORD  wSimType;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
   {
      printf ("\n\nThis test ilwolves system timings and will not function aclwrately"
               "\nin a simulated environment.\n");
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      return (nErr);
   }
   SetMode (0x03);

   TextStringOut ("The following test is self-running and will take about 10 seconds.", ATTR_NORMAL, 1, 1, 0);
   TextStringOut ("During this test, the monitor may lose sync.", ATTR_NORMAL, 1, 2, 0);
   TextStringOut ("Press any key to continue, <ESC> to exit.", ATTR_NORMAL, 1, 4, 0);
   if (SimGetKey () == KEY_ESCAPE)
      goto CharWidthTest_exit;
   TextStringOut ("Please wait...", ATTR_BLINK, 1, 6, 0);

   hz0 = GetFrameRate ();        // Get frame rate*1000
   if (hz0 == 0)
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            LOWORD (70000l), HIWORD (70000l), 0, 0);
      goto CharWidthTest_exit;
   }

   IOWordWrite (SEQ_INDEX, 0x0101);    // Set 8-dot mode

   // Verify that the line rate and therefore the frame rate increased by
   // about 12.5% (allow 5% error)
   hz1 = GetFrameRate ();
   hz2 = hz0 + (hz0 / 8);        // Add 12.5%
   if ((hz2 < (hz1 - (hz1 / 20))) || (hz2 > (hz1 + (hz1 / 20))))
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
               LOWORD (hz2), HIWORD (hz2), LOWORD (hz1), HIWORD (hz1));

CharWidthTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    10
//
//    CRTCWriteProtectTest - Verify that the lower CRTC registers cannot be written when write protect is enabled.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int CRTCWriteProtectTest (void)
{
   int      nErr, i;
   BYTE  vse, temp;

   nErr = ERROR_NONE;
   SimSetState (TRUE, TRUE, FALSE);    // I/O, no memory, no RAMDAC writes
   SetMode (0x92);                     // Set the mode without touching memory

   SimComment ("Unwrite-protect CRTC 0-7.");
   IOByteWrite (CRTC_CINDEX, 0x11);
   vse = (BYTE) IOByteRead (CRTC_CDATA);
   IOByteWrite (CRTC_CDATA, (BYTE) (vse & 0x7F));     // Unwrite protect CRTC 0-7

   // Verify that it's unprotected by doing an I/O operation on index 2
   SimComment ("Verify that it's unprotected by doing an I/O operation on index 2.");
   IOByteWrite (CRTC_CINDEX, 0x02);
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x00)) != 0)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, CRTC_CINDEX, 0x02, 0, temp);
      goto CRTCWriteProtectTest_exit;
   }

   SimComment ("Write protect 'em.");
   IOByteWrite (CRTC_CINDEX, 0x11);
   IOByteWrite (CRTC_CDATA, (BYTE) (vse | 0x80));     // Write protect 'em

   // Verify CRTC indexes 0 thru 6
   SimComment ("Verify CRTC indexes 0 thru 6.");
   for (i = 0; i < 7; i++)
   {
      IOByteWrite (CRTC_CINDEX, (BYTE) i);
      if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x00)) != 0xFF)
      {
         nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, CRTC_CINDEX, (WORD) i, 0xFF, temp);
         goto CRTCWriteProtectTest_exit;
      }
   }

   // Verify CRTC index 7
   SimComment ("Verify CRTC index 7.");
   IOByteWrite (CRTC_CINDEX, 0x07);
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x00)) != 0xEF)
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, CRTC_CINDEX, 0x07, 0xEF, temp);

CRTCWriteProtectTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    11
//
//    DoubleScanTest - Fill the screen with a pattern of horizontal lines and set the double scan bit.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int DoubleScanTest (void)
{
   int      nErr, i;
   WORD  wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto DoubleScanTest_exit;

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);
   GetResolution (&wXRes, &wYRes);

   // Draw a white line on every other scan line
   SimComment ("Draw a white line on every other scan line.");
   SetLine4Columns ((WORD) (wXRes / 8));
   for (i = 0; i < (int) wYRes; i += 2)
      Line4 (0, (WORD) i, (WORD) (wXRes - 1), (WORD) i, 0x0F);

   SimDumpMemory ("T0311.VGA");

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto DoubleScanTest_exit;
      }
   }

   SimComment ("Set double scan.");
   IOByteWrite (CRTC_CINDEX, 0x09);
   IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) | 0x80));

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto DoubleScanTest_exit;
      }
   }

DoubleScanTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    12

//
//    T0312
//    VerticalInterruptTest - Set a known timing and count the number of vertical periods in a given time.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int VerticalInterruptTest (void)
{
   printf ("\n\nThis test ilwolves system-specific functions that cannot be obtained"
            "\nusing ANSI standard \"C\" functions or in a simulated environment.\n");
   return (FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0));
}

#undef   REF_TEST
#define  REF_TEST    13
#define  GRMODE         0x0D
//
//    T0313
//    PanningTest - Scroll an image that is larger than visible memory.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int PanningTest (void)
{
   int      nErr;
   int      i;
   WORD  offset, wXRes, wYRes, wXVirtRes, wYVirtRes;
   BYTE  temp;
   BOOL  bFullVGA;

   nErr = ERROR_NONE;
   offset = 0;                               // Prevent warnings on stupid compilers
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto PanningTest_exit;

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (GRMODE);
   GetResolution (&wXRes, &wYRes);
   wXVirtRes = wXRes + 48;
   wYVirtRes = wYRes + 16;
   SimSetState (TRUE, TRUE, TRUE);

   SetLine4Columns (wXVirtRes / 8);
   IOByteWrite (CRTC_CINDEX, 0x13);
   IOByteWrite (CRTC_CDATA, (BYTE) (wXVirtRes / 16));

   // Draw a pattern
   SimComment ("Draw a pattern into memory.");
   Line4 (0, 0, wXVirtRes - 1, 0, 0xF);
   Line4 (wXVirtRes - 1, 0, wXVirtRes - 1, wYVirtRes - 1, 0xF);
   Line4 (0, wYVirtRes - 1, wXVirtRes - 1, wYVirtRes - 1, 0xF);
   Line4 (0, 0, 0, wYVirtRes - 1, 0xF);
   Line4 (0, 0, wXVirtRes - 1, wYVirtRes - 1, 0xF);
   Line4 (0, wYVirtRes - 1, wXVirtRes - 1, 0, 0xF);

   SimDumpMemory ("T0313.VGA");

   // For debugging, if anything is on the command line then wait
   // for a keystroke from the user before panning
   if (bFullVGA && (nArguments > 1))
      (void)SimGetKey ();

   // Pan right
   SimComment ("Pan right.");
   for (i = 0; i <= (int) (wXVirtRes - wXRes); i++)
   {
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
            if (GetKey () == KEY_ESCAPE) goto PanningTest_abort;
         WaitNotVerticalRetrace ();
      }

      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x33);
      IOByteWrite (ATC_INDEX, (BYTE) (i % 8));
      IOByteWrite (CRTC_CINDEX, 0x08);
      temp = (BYTE) IOByteRead (CRTC_CDATA);
      temp = (BYTE) ((temp & 0x9F) | (((i / 8) % 4) << 5));
      IOByteWrite (CRTC_CDATA, temp);
      temp = (BYTE) (i / 32);
      temp = (BYTE) (temp * 4);
      IOByteWrite (CRTC_CINDEX, 0x0D);
      IOByteWrite (CRTC_CDATA, temp);
   }

   // Pan down
   SimComment ("Pan down.");
   for (i = 0; i < (int) (wYVirtRes - wYRes); i++)
   {
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
            if (GetKey () == KEY_ESCAPE) goto PanningTest_abort;
         WaitVerticalRetrace ();
      }

      IOByteWrite (CRTC_CINDEX, 0x0C);
      offset = IOByteRead (CRTC_CDATA) << 8;
      IOByteWrite (CRTC_CINDEX, 0x0D);
      offset += IOByteRead (CRTC_CDATA);
      offset += wXVirtRes / 8;               // Next row down
      IOByteWrite (CRTC_CDATA, LOBYTE (offset));
      IOByteWrite (CRTC_CINDEX, 0x0C);
      IOByteWrite (CRTC_CDATA, HIBYTE (offset));
   }

   // Pan left
   SimComment ("Pan left.");
   for (i = wXVirtRes - wXRes; i >= 0; i--)
   {
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
            if (GetKey () == KEY_ESCAPE) goto PanningTest_abort;
         WaitNotVerticalRetrace ();
      }

      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x33);
      IOByteWrite (ATC_INDEX, (BYTE) (i % 8));

      if ((i % 8) == 7)
      {
         offset--;
         IOByteWrite (CRTC_CINDEX, 0x0D);
         IOByteWrite (CRTC_CDATA, LOBYTE (offset));
         IOByteWrite (CRTC_CINDEX, 0x0C);
         IOByteWrite (CRTC_CDATA, HIBYTE (offset));
      }
   }

   // Pan Up
   SimComment ("Pan up.");
   for (i = wYVirtRes - wYRes; i >= 0; i--)
   {
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
            if (GetKey () == KEY_ESCAPE) goto PanningTest_abort;
         WaitVerticalRetrace ();
      }

      IOByteWrite (CRTC_CINDEX, 0x0C);
      offset = IOByteRead (CRTC_CDATA) << 8;
      IOByteWrite (CRTC_CINDEX, 0x0D);
      offset += IOByteRead (CRTC_CDATA);
      offset -= wXVirtRes / 8;               // Next row down
      IOByteWrite (CRTC_CDATA, LOBYTE (offset));
      IOByteWrite (CRTC_CINDEX, 0x0C);
      IOByteWrite (CRTC_CDATA, HIBYTE (offset));
   }

   if (SimGetKey () == KEY_ESCAPE) goto PanningTest_abort;

PanningTest_exit:
   SystemCleanUp ();
   return (nErr);

// User hit the <ESC> key during panning
PanningTest_abort:
   nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   goto PanningTest_exit;
}

#undef   GRMODE
#undef   REF_TEST
#define  REF_TEST    14
//
//    T0314
//    UnderLineLocationTest - Set various values for the underline location
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int UnderLineLocationTest (void)
{
   static BYTE tblAttr[] = {
      0x00, 0x07, 0x01, 0x07, 0x08, 0x0F, 0x09, 0x0F,
      0x80, 0x70, 0x81, 0x70, 0x88, 0xF0, 0x89, 0xF0
   };
   int         nErr, i, j, nRows, nColumns, nNext;
   SEGOFF      lpVideo;
   BYTE     chr, chrStart;
   WORD     wOffset, wXRes, wYRes;
   BOOL     bFullVGA;
   char     szBuffer[128];

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto UnderLineLocationTest_exit;

   // If the VGA simulation library is being used to generate tests
   // on a system where full VGA access is too slow, then use a small
   // frame with only one character row.
   if (bFullVGA)
   {
      wOffset = 1616;                           // 10 rows down, 8th column
      chrStart = 0;
   }
   else
   {
      wOffset = 0;
      chrStart = 0xB0;
   }

   SimSetState (TRUE, TRUE, FALSE);             // No RAMDAC writes
   SetMode (0x03);
   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);
   nRows = __min (wYRes / 16, 4);
   nColumns = __min (wXRes / 9, 64);
   nNext = (wXRes / 9 - nColumns) * 2;

   lpVideo = (SEGOFF) 0xB8000000;
   lpVideo += wOffset;

   SimComment ("Draw characters into memory.");
   chr = chrStart;
   for (i = 0; i < nRows; i++)
   {
      for (j = 0; j < nColumns; j++)
      {
         MemByteWrite (lpVideo++, chr);
         MemByteWrite (lpVideo++, tblAttr[chr % 8]);
         chr++;
      }
      lpVideo += nNext;
   }

   SimDumpMemory ("T0314.VGA");
   DisableLwrsor ();

   // Walk through each scan line (plus one)
   SimComment ("Walk through each scan line (plus one).");
   for (i = 0; i < 17; i++)
   {
      sprintf (szBuffer, "Set underline to scanline %d.", i);
      SimComment (szBuffer);

      IOByteWrite (CRTC_CINDEX, 0x14);
      IOByteWrite (CRTC_CDATA, (BYTE) i);
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            break;
         }
      }
   }

UnderLineLocationTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    15
//
//    T0315
//    SyncDisableTest - Disable syncs and verify that no pulses are generated.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SyncDisableTest (void)
{
   int      nErr;
   WORD  wSimType;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
   {
      printf ("\n\nThis test ilwolves system timings and will not function aclwrately"
               "\nin a simulated environment.\n");
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      return (nErr);
   }

   SetMode (0x03);
   TextStringOut ("The following test is self-running and will take about 5 seconds.", ATTR_NORMAL, 1, 1, 0);
   TextStringOut ("Press any key to continue, <ESC> to exit.", ATTR_NORMAL, 1, 3, 0);
   if (SimGetKey () == KEY_ESCAPE)
      goto SyncDisableTest_exit;
   SetMode (0x12);

   // There should be retrace happening
   if (!WaitVerticalRetrace ())
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            LOWORD (60000l), HIWORD (60000l), 0, 0);
      goto SyncDisableTest_exit;
   }

   IOByteWrite (CRTC_CINDEX, 0x17);
   IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));   // Disable syncs

   // There should be no retrace now
   if (WaitVerticalRetrace ())
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);

SyncDisableTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    16
//
//    T0316
//    LineCompareTest - Smooth scroll a split screen window to the top of the display
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int LineCompareTest (void)
{
   static BYTE pattern0[] = {
      '0', 0x0F, '1', 0x0F, '2', 0x0F, '3', 0x0F, '4', 0x0F,
      '5', 0x0F, '6', 0x0F, '7', 0x0F, '8', 0x0F, '9', 0x0F
   };
   static BYTE pattern1[] = {
      'A', 0x70, 'B', 0x70, 'C', 0x70, 'D', 0x70, 'E', 0x70,
      'F', 0x70, 'G', 0x70, 'H', 0x70, 'I', 0x70, 'J', 0x70,
      'K', 0x70, 'L', 0x70, 'M', 0x70, 'N', 0x70, 'O', 0x70,
      'P', 0x70, 'Q', 0x70, 'R', 0x70, 'S', 0x70, 'T', 0x70,
      'U', 0x70, 'V', 0x70, 'W', 0x70, 'X', 0x70, 'Y', 0x70,
      'Z', 0x70
   };
   int         nErr, n, nMemSize, nRows, nColumns, nRowCount, i;
   SEGOFF      lpVideo;
   BYTE     temp, byPan;
   WORD     wSimType;
   WORD     wXRes, wYRes;
   BOOL     bFullVGA;
   char     szBuffer[128];

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto LineCompareTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // No RAMDAC writes
   SetMode (0x03);
   GetResolution (&wXRes, &wYRes);
   nRows = wYRes / 16;
   nColumns = wXRes / 9;
   SimSetState (TRUE, TRUE, TRUE);

   // Fill page 0 memory with a pattern
   SimComment ("Fill page 0 memory with a pattern.");
   lpVideo = (SEGOFF) 0xB8000000;
   nMemSize = nColumns*nRows*2;
   i = n = 0;
   while (n <= nMemSize)
   {
      MemByteWrite (lpVideo++, pattern0[i]);
      if (++i >= (int) sizeof (pattern0)) i = 0;
      n++;
   }

   // Fill page 1 memory with a pattern
   SimComment ("Fill page 1 memory with a pattern.");
   lpVideo = (SEGOFF) 0xB8001000;
   nMemSize = nColumns*nRows*2;
   i = n = 0;
   while (n <= nMemSize)
   {
      MemByteWrite (lpVideo++, pattern1[i]);
      if (++i >= (int) sizeof (pattern1)) i = 0;
      n++;
   }

   SimDumpMemory ("T0316.VGA");

   // Disable pixel panning for bottom half of screen
   SimComment ("Disable pixel panning for bottom half of screen.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, (BYTE) (IOByteRead (ATC_RDATA) | 0x20));

   // Set display start to 1000h
   SimComment ("Set display start to 1000h.");
   IOWordWrite (CRTC_CINDEX, 0x080C);
   IOWordWrite (CRTC_CINDEX, 0x000D);

   // Set preset row scan to 7 to show that the internal row scan counter
   // is NOT set to the preset row scan count at line compare. It appears
   // that all relevant internal counters are reset to "0" at line compare.
   SimComment ("Set preset row scan to 7 to show that the internal row scan counter is NOT set to the preset row scan count at line compare.");
   IOWordWrite (CRTC_CINDEX, 0x0708);

   // Capture the initial frame and set the number of rows based on
   // whether this is a visual test or whether it is an automated test.
   if (!bFullVGA || (wSimType & SIM_SIMULATION))
      nRowCount = __min (wYRes - 1, 34);
   else
      nRowCount = 200;

   sprintf (szBuffer, "Number of lines to scroll: %d", nRowCount);
   SimComment (szBuffer);

   byPan = 0;
#ifdef LW_MODS
   // Disable the cursor
   SimComment ("Disable the cursor.");
   IOWordWrite (CRTC_CINDEX, 0x200A);

   SimComment ("Wait for VGA to stabilize.");
   DispTest::UpdateNow(LegacyVGA::vga_timeout_ms());           // Send an update and wait a few frames
   WaitVerticalRetrace ();                                     // Sync the beginning to the vsync pulse
   WaitVerticalRetrace ();                                     // Sync the beginning to the vsync pulse
   WaitVerticalRetrace ();                                     // Sync the beginning to the vsync pulse
   LegacyVGA::CRCManager()->CaptureFrame (0, 1, 1, LegacyVGA::vga_timeout_ms());
   LegacyVGA::CRCManager()->CaptureFrame (0, 1, 1, LegacyVGA::vga_timeout_ms());
   WaitVerticalRetrace ();                                     // Sync the beginning to the vsync pulse
#else
   WaitLwrsorBlink (BLINK_OFF);                                // Ignore blinking in this test
   (void)FrameCapture (REF_PART, REF_TEST);
#endif
   for (i = nRowCount, n = 0; i >= 0; i--, n++)
   {
      sprintf (szBuffer, "Lines to go: %d", i);
      SimComment (szBuffer);

      // Do comparison one scan line before the end.
      // Standard VGA is off by one.
#ifdef LW_MODS
      LegacyVGA::CRCManager()->CaptureFrame (1, 1, 0, LegacyVGA::vga_timeout_ms());
#else
      if (!FrameCapture (REF_PART, REF_TEST))
      {

         if (i == 0)
         {
            if (GetKey () == KEY_ESCAPE)
            {
               nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
               goto LineCompareTest_exit;
            }
         }
         else
         {
            if (_kbhit ())
            {
               if (GetKey () == KEY_ESCAPE)
               {
                  nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
                  goto LineCompareTest_exit;
               }
            }
         }
      }
#endif

      WaitNotVerticalRetrace ();

      // CRTC[9].6 is line compare bit 9
      IOByteWrite (CRTC_CINDEX, 0x09);
      temp = (BYTE) (IOByteRead (CRTC_CDATA) & 0xBF);
      IOByteWrite (CRTC_CDATA, (BYTE) (temp | ((i & 0x200) >> 3)));

      // CRTC[7].4 is line compare bit 8
      IOByteWrite (CRTC_CINDEX, 0x07);
      temp = (BYTE) (IOByteRead (CRTC_CDATA) & 0xEF);
      IOByteWrite (CRTC_CDATA, (BYTE) (temp | ((i & 0x100) >> 4)));

      // CRTC[18].0..7 is line compare bits 0-7
      IOByteWrite (CRTC_CINDEX, 0x18);
      IOByteWrite (CRTC_CDATA, (BYTE) (i & 0xFF));

      IOByteWrite (ATC_INDEX, 0x33);
      temp = (BYTE) IOByteRead (ATC_RDATA);
      temp++;
      if (temp == 8)
      {
         byPan++;
         if (byPan > 3)
         {
            byPan = 0;
            IOByteWrite (CRTC_CINDEX, 0x0D);
            IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) + 4));
         }
         IOByteWrite (CRTC_CINDEX, 0x08);
         IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x9F) | (byPan << 5));
      }
      else if (temp > 8)
      {
         temp = 0;
      }
      IOByteWrite (ATC_INDEX, temp);
   }

#ifdef LW_MODS
   LegacyVGA::CRCManager()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
   LegacyVGA::CRCManager()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
#else
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }
#endif

LineCompareTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    17
//
//    T0317
//    Panning256Test - In mode 13h, pan by half characters
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int Panning256Test (void)
{
   static DACTABLE   dac[] = {
      {0xF0, 0x3F, 0x3F, 0x00},  // This entry must match last two
      {0xF1, 0x00, 0x00, 0x2A},
      {0xF2, 0x00, 0x2A, 0x00},
      {0xF3, 0x00, 0x2A, 0x2A},
      {0xF4, 0x2A, 0x00, 0x00},
      {0xF5, 0x2A, 0x00, 0x2A},
      {0xF6, 0x2A, 0x2A, 0x00},
      {0xF7, 0x2A, 0x2A, 0x2A},
      {0xF8, 0x15, 0x15, 0x15},
      {0xF9, 0x00, 0x00, 0x3F},
      {0xFA, 0x00, 0x3F, 0x00},
      {0xFB, 0x00, 0x3F, 0x3F},
      {0xFC, 0x3F, 0x00, 0x00},
      {0xFD, 0x3F, 0x00, 0x3F},
      {0xFE, 0x3F, 0x3F, 0x00},  // This entry will be used by previous half-pixel (DFh)
      {0xFF, 0x3F, 0x3F, 0x00},  // This entry must match previous and first entries
      {0xF0, 0x3F, 0x3F, 0x00}   // Repeat first entry
   };
   int         nErr;
   SEGOFF      lpVideo;
   int         i, j;
   WORD     wXRes, wYRes;
   BOOL     bFullVGA;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto Panning256Test_exit;

   SimSetState (TRUE, TRUE, FALSE);       // No RAMDAC writes
   SetMode (0x13);
   SimSetState (TRUE, TRUE, TRUE);           // Allow RAMDAC writes
   GetResolution (&wXRes, &wYRes);
   lpVideo = (SEGOFF) 0xA0000000;

   // Screw up the internal palette to show that the pixel pipeline goes
   // through the internal palette in alternating nibbles. The pixel is
   // actually constructed on the other side of the internal palette.
   // Turn all "E" nibbles into "F" nibbles:
   SimComment ("Screw up the internal palette to show that the pixel pipeline goes through the internal palette in alternating nibbles.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x0E);
   IOByteWrite (ATC_INDEX, 0x0F);
   IOByteWrite (ATC_INDEX, 0x20);

   // There should be no bright white pixels in the picture
   SimComment ("There should be no bright white pixels in the picture.");
   FillDAC (0x3F, 0x3F, 0x3F);
   SimComment ("Make sure overscan will be 0.");
   SetDac (0, 0, 0, 0);             // So overscan will be 0

   // Set DAC to match altered pixels
   SimComment ("Set the DAC to match altered pixels.");
   for (i = 0, j = 1; i < 16; i++, j++)
   {
      SetDac (dac[i].idx, dac[i].red, dac[i].green, dac[i].blue);
      SetDac ((BYTE) (((dac[i].idx & 0x0F) << 4) | (dac[j].idx >> 4)),
               dac[j].red, dac[j].green, dac[j].blue);
   }

   // Fill memory with a repeating pattern using pixel data that will
   // reveal the pipeline through the attribute controller. Also. fill
   // one extra scan line because the panning will scroll data off the
   // left and the last scan line needs to pull data from the "next"
   // scan line.
   SimComment ("Fill memory with a repeating pattern using pixel data that will reveal the pipeline through the attribute controller.");
   for (i = 0; i < (int) (wYRes + 1); i++)
   {
      for (j = 0; j < (int) wXRes; j++)
      {
         MemByteWrite (lpVideo++, (BYTE) ((j & 0x0F) | 0xE0));
      }
   }
   SimDumpMemory ("T0317.VGA");

   // For debugging, if anything is on the command line then wait
   // for a keystroke from the user before panning
   if (bFullVGA && (nArguments > 1))
      (void)SimGetKey ();

   // Pan from left to right setting pixel panning from 0 through 7,
   // byte panning from 0 through 3, and incrementing start address
   // by 4 when necessary. Note that an entire frame is generated
   // before the pixel panning, byte panning, and start address
   // values take effect due to the wait for "not" vertical retrace.
   SimComment ("Pan from left to right.");
   StartCapture (1);
   (void)FrameCapture (REF_PART, REF_TEST);
   for (i = 1; i < 33; i++)
   {
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
            if (GetKey () == KEY_ESCAPE) goto Panning256Test_exit;
         WaitNotVerticalRetrace ();
      }

      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x33);
      IOByteWrite (ATC_INDEX, (BYTE) (i & 0x07));
      IOByteWrite (CRTC_CINDEX, 0x08);
      IOByteWrite (CRTC_CDATA, (BYTE) (((i >> 3) & 0x03) << 5));
      IOByteWrite (CRTC_CINDEX, 0x0D);
      IOByteWrite (CRTC_CDATA, (BYTE) (((i >> 3) >> 2) << 2));
   }

   // Pan from right to left setting pixel panning from 7 through 0,
   // byte panning from 1 through 0, and decrementing start address
   // by 2 when necessary. Note that an entire frame is generated
   // before the pixel panning, byte panning, and start address
   // values take effect due to the wait for "not" vertical retrace.
   SimComment ("Pan from right to left.");
   for (i = 32; i >= 0; i--)
   {
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
            if (GetKey () == KEY_ESCAPE) goto Panning256Test_exit;
         WaitNotVerticalRetrace ();
      }

      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x33);
      IOByteWrite (ATC_INDEX, (BYTE) (i & 0x07));
      IOByteWrite (CRTC_CINDEX, 0x08);
      IOByteWrite (CRTC_CDATA, (BYTE) (((i >> 3) & 0x01) << 5));
      IOByteWrite (CRTC_CINDEX, 0x0D);
      IOByteWrite (CRTC_CDATA, (BYTE) (((i >> 3) >> 1) << 1));
   }

   if (!FrameCapture (REF_PART, REF_TEST))
      (void)GetKey ();

Panning256Test_exit:
   EndCapture ();
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    18
//
//    T0318
//    CountBy4Test - In mode 12h, set a known pattern and visually inspect the count by 2, count by 4 functionality.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int CountBy4Test (void)
{
   int         nErr, i, j;
   SEGOFF      lpVideo;
   WORD     offset, wXRes, wYRes, wRowOffset;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto CountBy4Test_exit;

   SimSetState (TRUE, TRUE, FALSE);          // No RAMDAC writes
   SetMode (0x12);
   GetResolution (&wXRes, &wYRes);
   lpVideo = (SEGOFF) 0xA0000000;
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Write a pattern into memory.");
   wRowOffset = wXRes / 8;
   for (i = 0; i < (int) wRowOffset; i++)
   {
      IOByteWrite (SEQ_INDEX, 0x02);
      IOByteWrite (SEQ_DATA, (BYTE) (16 - (i % 16)));
      offset = i;
      for (j = 0; j < (int) wYRes; j++)
      {
         MemByteWrite (lpVideo + offset, 0xAA);
         offset += wRowOffset;
      }
   }
   SimDumpMemory ("T0318.VGA");

   // Verify "normal" screen
   SimComment ("Verify \"normal\" screen.");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto CountBy4Test_exit;
      }
   }

   // Set count by 2
   SimComment ("Set count by 2.");
   IOWordWrite (CRTC_CINDEX, 0xAB17);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto CountBy4Test_exit;
      }
   }

   // Set count by 2 and WORD mode
   SimComment ("Set count by 2 and WORD mode.");
   IOWordWrite (CRTC_CINDEX, 0xEB17);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto CountBy4Test_exit;
      }
   }

   // Set count by 4, BYTE mode
   SimComment ("Set count by 4, BYTE mode.");
   IOWordWrite (CRTC_CINDEX, 0xA317);
   IOWordWrite (CRTC_CINDEX, 0x3F14);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto CountBy4Test_exit;
      }
   }

   // Set WORD mode
   SimComment ("Set WORD mode.");
   IOWordWrite (CRTC_CINDEX, 0xE317);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto CountBy4Test_exit;
      }
   }

   // Set count by 2 mode while in count by 4 (verify count by 2 has
   // priority over count by 4)
   SimComment ("Set count by 2 mode while in count by 4 (verify count by 2 has priority over count by 4).");
   IOWordWrite (CRTC_CINDEX, 0xAB17);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

CountBy4Test_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    19
//
//    T0319
//    SkewTest - In mode 12h, set a known pattern and visually inspect the sync skew and display skew.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SkewTest (void)
{
   int      nErr, i;
   WORD  wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto SkewTest_exit;

   // Set 640x480 graphics, draw a rectangle around the screen, and
   // draw two diagonal lines across the center.
   SimSetState (TRUE, TRUE, FALSE);
   SetMode (0x12);

   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Draw a pattern into memory.");
   SetLine4Columns (wXRes / 8);
   Line4 (0, 0, wXRes - 1, 0, 0x0F);
   Line4 (wXRes -1, 0, wXRes - 1, wYRes - 1, 0x0F);
   Line4 (0, wYRes -1, wXRes - 1, wYRes -1, 0x0F);
   Line4 (0, 0, 0, wYRes, 0x0F);
   Line4 (0, 0, wXRes - 1, wYRes - 1, 0x0F);
   Line4 (0, wYRes - 1, wXRes - 1, 0, 0x0F);

   SimDumpMemory ("T0319.VGA");

   // Verify screen is as expected
   SimComment ("Verify screen is as expected.");
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto SkewTest_exit;
      }
   }

   // Unlock CRTC
   SimComment ("Unlock CRTC.");
   IOByteWrite (CRTC_CINDEX, 0x11);
   IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));

   for (i = 0; i < 4; i++)
   {
      SimComment ("Set skew.");
      IOByteWrite (CRTC_CINDEX, 0x03);
      IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x9F);
      IOByteWrite (CRTC_CINDEX, 0x05);
      IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));

      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto SkewTest_exit;
         }
      }
   }
   for (i = 0; i < 4; i++)
   {
      SimComment ("Set skew.");
      IOByteWrite (CRTC_CINDEX, 0x03);
      IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));
      IOByteWrite (CRTC_CINDEX, 0x05);
      IOByteWrite (CRTC_CDATA, IOByteRead (CRTC_CDATA) & 0x9F);

      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto SkewTest_exit;
         }
      }
   }

   for (i = 0; i < 4; i++)
   {
      SimComment ("Set skew.");
      IOByteWrite (CRTC_CINDEX, 0x03);
      IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));
      IOByteWrite (CRTC_CINDEX, 0x05);
      IOByteWrite (CRTC_CDATA, (BYTE)((IOByteRead (CRTC_CDATA) & 0x9F) | (i << 5)));

      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto SkewTest_exit;
         }
      }
   }

SkewTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    20
//
//    T0320
//    PresetRowScanTest - In mode 3, smooth scroll a screen.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int PresetRowScanTest (void)
{
   BYTE     byTable[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
    unsigned    i, n, nFrames;
   int         nErr, j;
   SEGOFF      lpVideo;
   WORD     offset;
   WORD     wSimType;
   BOOL     bFullVGA;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto PresetRowScanTest_exit;

   SimSetState (TRUE, TRUE, FALSE);
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   lpVideo = (SEGOFF) 0xB8000000;

   // Fill memory with a known pattern
   SimComment ("Fill memory with a known pattern.");
   n = 0;
   for (i = 0; i < 70; i++)            // Enough to fill 70*16 scan lines
   {
      for (j = 0; j < 80; j++)
      {
         MemByteWrite (lpVideo++, byTable[n]);
         MemByteWrite (lpVideo++, 0x07);
      }
      if (++n >= sizeof (byTable)) n = 0;
   }
   SimDumpMemory ("T0320.VGA");

   // Capture the initial frame and set the scroll variable based on
   // whether this is a visual or an automated test. For the visual
   // test, 700 + 400 (original screen) scans eventually make their
   // appearance on screen.
   if (!bFullVGA || (wSimType & SIM_SIMULATION))
      nFrames = 34;
   else
      nFrames = 700;

   // The preset row seems to get loaded immediately, while the row address
   // takes effect on the next frame. Therefore, change the row address first,
   // then wait for the end of the frame before changing the preset row address.
   SimComment ("Smooth scroll.");
   StartCapture (1);
   (void)FrameCapture (REF_PART, REF_TEST);
   for (i = 0; i < nFrames; i++)
   {
      offset = (i / 16) * 80;
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
         {
            if (GetKey () == KEY_ESCAPE)
            {
               nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
               goto PresetRowScanTest_exit;
            }
         }
         WaitNotVerticalRetrace ();
      }

      IOByteWrite (CRTC_CINDEX, 0x0C);
      IOByteWrite (CRTC_CDATA, HIBYTE (offset));
      IOByteWrite (CRTC_CINDEX, 0x0D);
      IOByteWrite (CRTC_CDATA, LOBYTE (offset));
      WaitVerticalRetrace ();
      IOByteWrite (CRTC_CINDEX, 0x08);
      IOByteWrite (CRTC_CDATA, (BYTE) (i % 16));
   }

PresetRowScanTest_exit:
   EndCapture ();
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    21
//
//    T0321
//    LwrsorSkewTest - In mode 03h, set each of the four cursor skew positions
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int LwrsorSkewTest (void)
{
    unsigned i;
   int      nErr;
   BYTE  tblSkew[] = {0x0E, 0x2E, 0x4E, 0x6E};

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto LwrsorSkewTest_exit;

   SimSetState (TRUE, TRUE, FALSE);
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Draw a pattern into memory.");
   TextStringOut ("0123", 0x07, 0, 0, 0);

   // Cursor at position 0
   SimComment ("Set cursor to position 0.");
   IOByteWrite (CRTC_CINDEX, 0x0E);
   IOByteWrite (CRTC_CDATA, 0x00);
   IOByteWrite (CRTC_CINDEX, 0x0F);
   IOByteWrite (CRTC_CDATA, 0x00);

   SimDumpMemory ("T0321.VGA");

   // Verify screen is as expected (0 skew)
   WaitLwrsorBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto LwrsorSkewTest_exit;
      }
   }

   for (i = 0; i < sizeof (tblSkew); i++)
   {
      SimComment ("Skew cursor.");
      IOByteWrite (CRTC_CINDEX, 0x0B);
      IOByteWrite (CRTC_CDATA, tblSkew[i]);

      // Verify screen is as expected (cursor skewed "i" times)
      WaitLwrsorBlink (BLINK_ON);
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto LwrsorSkewTest_exit;
         }
      }
   }

LwrsorSkewTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    22
//
//    T0322
//    NonBIOSLineCompareTest - Pan a split-screen using the Renaissance CRTC values
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int NonBIOSLineCompareTest (void)
{
   static PARMENTRY  parmTest = {
      0x50, 0x1D, 0x10,
      0x00, 0xA0,
      {0x11, 0x0F, 0x00, 0x0E},
      0xE3,
      {0x5F, 0x4F, 0x53, 0xE1, 0x58, 0x00,
      0x0B, 0x2E, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0xEA, 0x8C,
      0xDF, 0x0A, 0x7F, 0xE7, 0x04, 0xE3,
      0xC8},
      {0x01, 0x3F, 0x00, 0x3F, 0x00, 0x3F,
      0x00, 0x3F, 0x00, 0x3F, 0x00, 0x3F,
      0x00, 0x3F, 0x00, 0x3F, 0x21, 0x00,
      0x01, 0x00},
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x05, 0x0F, 0xFF}
   };
   static PARMENTRY  parmSmallTest = {
      0x50, 0x1D, 0x10,
      0x00, 0xA0,
      {0x11, 0x0F, 0x00, 0x0E},
      0xE3,
      {0x2D, 0x27, 0x28, 0xF0, 0x2B, 0xE0,
      0x17, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0xFF, 0x00, 0x15, 0x06,
      0x13, 0x05, 0x7F, 0x14, 0x17, 0xE3,
      0x0A},
      {0x01, 0x3F, 0x00, 0x3F, 0x00, 0x3F,
      0x00, 0x3F, 0x00, 0x3F, 0x00, 0x3F,
      0x00, 0x3F, 0x00, 0x3F, 0x21, 0x00,
      0x01, 0x00},
      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x05, 0x0F, 0xFF}
   };
   int      nErr, i, j, nPans, nRowCount, nColCount;
   BYTE  chr;
   BOOL  bFullVGA, bCapture;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto NonBIOSLineCompareTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);

   // Load the registers
   SimComment ("Load a non-BIOS mode into the registers.");
   if (bFullVGA)
   {
      SetRegs (&parmTest);
      nRowCount = 30;
      nColCount = 80;
   }
   else
   {
      SetRegs (&parmSmallTest);
      nRowCount = 1;
      nColCount = 40;
   }

   SimComment ("Draw known pattern into memory.");
   chr = 1;
   for (i = 0; i < nRowCount; i++)
   {
      for (j = 0; j < nColCount; j++)
         DrawMonoChar (j, i, chr + j, tblFont8x16, 16, nColCount);
      chr++;
   }

   SimDumpMemory ("T0322.VGA");

   bCapture = FrameCapture (REF_PART, REF_TEST);
   if (!bCapture)
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto NonBIOSLineCompareTest_exit;
      }
   }

   // Now pan the beast
   SimComment ("Now pan the beast.");
   nPans = 320;
   if (bCapture) nPans = 34;
   for (i = 0; i < nPans; i++)
   {
      WaitNotVerticalRetrace ();
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (_kbhit ())
         {
            if (GetKey () == KEY_ESCAPE)
            {
               nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
               goto NonBIOSLineCompareTest_exit;
            }
         }
      }
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, 0x33);
      IOByteWrite (ATC_INDEX, (BYTE) (i & 0x7));               // Pixel panning
      IOByteWrite (CRTC_CINDEX, 0x08);
      IOByteWrite (CRTC_CDATA, (BYTE) ((i & 0x18) << 2));      // Byte panning
      IOByteWrite (CRTC_CINDEX, 0x0C);
      IOByteWrite (CRTC_CDATA, (BYTE) (i >> 13));              // Display start high
      IOByteWrite (CRTC_CINDEX, 0x0D);
      IOByteWrite (CRTC_CDATA, (BYTE) ((i >> 5) & 0xFF));      // Display start low
   }

   WaitNotVerticalRetrace ();
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

NonBIOSLineCompareTest_exit:
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
