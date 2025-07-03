//
//    PART5.CPP - VGA Core Test Suite (Part 5)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 15 November 2005
//
//    Routines in this file:
//    SetResetTest         Cycle through each combination of Set/Reset and Enable
//    ColorCompareTest     Draw a pattern into memory and read it back with various color compare values.
//    ROPTest              Cycle through all possible combinations and match expected data with actual data
//    WriteMode1Test       Draw pattern into memory and duplicate it using write mode 1
//    WriteMode2Test       Draw a series of rectangles without doing any I/O to change colors.
//    SetLatchedBytes         Set a block of video memory after latching a data value
//    WriteMode3Test       Draw font images over a colored background without affecting the background.
//    VideoSegmentTest     Set the video segment to various locations and verify that memory exists.
//    BitMaskTest          Fill video memory with at pattern and set all possible bit mask combinations.
//    NonPlanarReadModeTest   Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//    NonPlanarWriteMode1Test Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//    NonPlanarWriteMode2Test Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//    NonPlanarWriteMode3Test Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//    ShiftRegisterModeTest   Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//
#include <stdio.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int SetResetTest (void);
int ColorCompareTest (void);
int ROPTest (void);
int WriteMode1Test (void);
int WriteMode2Test (void);
int WriteMode3Test (void);
int VideoSegmentTest (void);
int BitMaskTest (void);
int NonPlanarReadModeTest (void);
int NonPlanarWriteMode1Test (void);
int NonPlanarWriteMode2Test (void);
int NonPlanarWriteMode3Test (void);
int ShiftRegisterModeTest (void);

#define  REF_PART    5
#define  REF_TEST    1
#define  COUNT_SETRESET          16
#define  COUNT_SETRESETENABLE    16
//
//    T0501
//    SetResetTest - Cycle through each combination of Set/Reset and Enable
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SetResetTest (void)
{
   int      nErr;
   int      i, j, k;
   SEGOFF   lpVideo;
   BYTE  byExpected, byActual, byMemData;

   nErr = ERROR_NONE;
   SimSetState (TRUE, TRUE, FALSE);    // I/O, memory, no DAC
   SetMode (0x92);                     // Don't bother clearing memory, it'll be written later
   SimSetState (TRUE, TRUE, TRUE);

   // Fill memory with a non-zero, non-FFh value
   byMemData = 0x55;
   lpVideo = (SEGOFF) 0xA0000000;
   SimComment ("Load 256 bytes of memory with 0x55, to prepare for Set/Reset test.");
   for (i = 0; i < COUNT_SETRESET*COUNT_SETRESETENABLE; i++)
   {
      MemByteWrite (lpVideo++, byMemData);
   }
   SimDumpMemory ("T0501.VGA");

   // Write each possible combination of Set/Reset and Set/Reset Enable
   lpVideo = (SEGOFF) 0xA0000000;
   SimComment ("Write every possible combination of Set/Reset and Set/Reset Enable.");
   for (i = 0; i < COUNT_SETRESET; i++)            // Cycle through each Set/Reset
   {
      IOByteWrite (GDC_INDEX, 0x00);
      IOByteWrite (GDC_DATA, (BYTE) i);
      for (j = 0; j < COUNT_SETRESETENABLE; j++)      // Cycle through each Enable
      {
         IOByteWrite (GDC_INDEX, 0x01);
         IOByteWrite (GDC_DATA, (BYTE) j);
         MemByteRead (lpVideo);                 // Load latches
         MemByteWrite (lpVideo, 0);
         lpVideo++;
      }
   }

   // Verify the data
   SimComment ("Verify the actual data matches the expected value.");
   lpVideo = (SEGOFF) 0xA0000000;
   IOByteWrite (GDC_INDEX, 0x04);
   for (i = 0; i < COUNT_SETRESET; i++)            // Cycle through each Set/Reset
   {
      for (j = 0; j < COUNT_SETRESETENABLE; j++)   // Cycle through each enable
      {
         for (k = 0; k < 4; k++)                   // Cycle through each plane
         {
            IOByteWrite (GDC_DATA, (BYTE) k);
            // Callwlate enable
            byExpected = (j & (0x01 << k)) == 0 ? 0x00 : 0xFF;
            // Callwlate memory byte
            byExpected = ((i & (0x01 << k)) == 0 ? 0x00 : 0xFF) & byExpected;
            if (!MemByteTest (lpVideo, byExpected, &byActual))
            {
               nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                 LOWORD (lpVideo), HIWORD (lpVideo),
                                 byExpected, byActual);
               goto SetResetTest_exit;
            }
         }
         lpVideo++;
      }
   }

SetResetTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Set/Reset Test passed.");
   else
      SimComment ("Set/Reset Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T0502
//    ColorCompareTest - Draw a pattern into memory and read it back with various color compare values.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int ColorCompareTest (void)
{
   static BYTE abyExpected[80] = {
      0xFF, 0xFF, 0xFF, 0x55, 0xAA, 0x55, 0x33, 0x33,
      0xCC, 0x44, 0x22, 0x11, 0x0F, 0x0F, 0x0F, 0x05,
      0xA0, 0x50, 0x30, 0x30, 0x0C, 0x04, 0x02, 0x01,
      0x00, 0xFF, 0x00, 0x55, 0x00, 0x55, 0x00, 0x33,
      0xCC, 0x00, 0x22, 0x00, 0x0F, 0x00, 0x0F, 0x00,
      0x00, 0x50, 0x00, 0x30, 0x00, 0x04, 0x00, 0x01,
      0xFF, 0xFF, 0xFF, 0x55, 0xAA, 0x55, 0x33, 0x33,
      0xCC, 0x44, 0x22, 0x11, 0x0F, 0x0F, 0x0F, 0x05,
      0xA0, 0x50, 0x30, 0x30, 0x0C, 0x04, 0x02, 0x01,
      0x00, 0xFF, 0x00, 0x55, 0x00, 0x55, 0x00, 0x33
   };
   WORD     wOffset;
   int         i, j;
   BYTE     byColor, byActual, byMask, tmp;
   int         nErr;
   SEGOFF      lpVideo;

   nErr = ERROR_NONE;
   SimSetState (TRUE, TRUE, FALSE);    // No DAC
   SetMode (0x92);                     // Don't bother clearing memory, it'll be written later
   SimSetState (TRUE, TRUE, TRUE);

   lpVideo = (SEGOFF) 0xA0000000;
   wOffset = (640/8)*(480/2);          // Half way down the screen
   byColor = 0;
   SimComment ("Load known data into 80 bytes of video memory.");
   for (i = 0; i < 80; i++)
   {
      byMask = 0x01;
      for (j = 0; j < 8; j++)
      {
         byMask = RotateByteRight (byMask, 1);
         SetPixelAt (lpVideo + wOffset + i, byMask, byColor);
         byColor = (byColor + 1) % 16;
      }
   }
   SimDumpMemory ("T0502.VGA");

   SimComment ("Set read mode 1 (color compare) and verify that expected data matches actual data.");
   IOByteWrite (GDC_INDEX, 0x05);
   tmp = IOByteRead (GDC_DATA);
   IOByteWrite (GDC_DATA, (BYTE) (tmp | 0x08));    // Set read mode 1
   for (i = 0; i < 80; i++)
   {
      IOByteWrite (GDC_INDEX, 0x07);               // Color don't care
      IOByteWrite (GDC_DATA, (BYTE) ((i/3) % 16)); // Cycle colors on every third pass
      IOByteWrite (GDC_INDEX, 0x02);               // Color compare
      IOByteWrite (GDC_DATA, (BYTE) (i % 16));     // Cycle through colors
      if (!MemByteTest (lpVideo + wOffset + i, abyExpected[i], &byActual))
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           wOffset + i, HIWORD (lpVideo),
                           abyExpected[i], byActual);
         goto ColorCompareTest_exit;
      }
   }

ColorCompareTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Color Compare Test passed.");
   else
      SimComment ("Color Compare Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T0503
//    ROPTest - Cycle through all possible combinations and match expected data with actual data
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int ROPTest (void)
{
   int      nErr, i, j, cTestData, nDataIdx;
   SEGOFF   lpVideo;
   WORD  wOffset;
   WORD  wSimType;
   BOOL  bFullVGA;
   BYTE  byRop, byRotate, byData, byExpected, byActual;
   BYTE  byBefore[4];
   BYTE  tblTestData[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x55, 0xAA, 0x66, 0x99, 0xFF};

   nErr = ERROR_NONE;
   cTestData = sizeof (tblTestData) / sizeof (BYTE);
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));
   SimSetState (TRUE, TRUE, FALSE);    // I/O, no DAC
   SetMode (0x92);                     // Don't bother clearing memory, it'll be written later
   SimSetState (TRUE, TRUE, TRUE);

   lpVideo = (SEGOFF) 0xA0000000;

   SimComment ("Load 16 blocks of data into video memory.");
   IOWordWrite (GDC_INDEX, 0x0F01);       // Enable set/reset
   wOffset = 0;
   for (i = 0; i < 16; i++)
   {
      IOByteWrite (GDC_INDEX, 0x00);
      IOByteWrite (GDC_DATA, (BYTE) i);
      for (j = 0; j < cTestData; j++)
         MemByteWrite (lpVideo + wOffset++, 0);
   }
   IOWordWrite (GDC_INDEX, 0x0001);       // Disable set/reset

   SimDumpMemory ("T0503.VGA");

   // Go through ROP and rotate combinations
   SimComment ("Cycle through ROP and rotate combination, verifying memory along the way.");
   wOffset = 0;
   byRop = byRotate = byData = 0;
   nDataIdx = 0;
   while (wOffset < (WORD) (16 * cTestData))
   {
      if (bFullVGA)
      {
         if (_kbhit ())
            if (GetKey () == KEY_ESCAPE)
            {
               nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
               goto ROPTest_exit;
            }
      }
      byData = tblTestData[nDataIdx];
      if (++nDataIdx >= cTestData) nDataIdx = 0;

      IOByteWrite (GDC_INDEX, 0x03);
      IOByteWrite (GDC_DATA, (BYTE) ((byRop << 3) | byRotate));   // Set ROP and rotate value

      // Get video memory needed for diagnostic (latches data as well)
      IOByteWrite (GDC_INDEX, 0x04);
      for (i = 0; i < 4; i++)
      {
         IOByteWrite (GDC_DATA, (BYTE) i);
         byBefore[i] = MemByteRead (lpVideo + wOffset);
      }

      // Write to memory
      MemByteWrite (lpVideo + wOffset, byData);

      // Get the results of the operation
      for (i = 0; i < 4; i++)
      {
         IOByteWrite (GDC_DATA, (BYTE) i);
         byExpected = RotateByteRight (byData, byRotate);
         switch (byRop)
         {
            case 1:
               byExpected &= byBefore[i];
               break;
            case 2:
               byExpected |= byBefore[i];
               break;
            case 3:
               byExpected ^= byBefore[i];
               break;
         }
         if (!MemByteTest (lpVideo + wOffset, byExpected, &byActual))
         {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                     wOffset, HIWORD (lpVideo), byExpected, byActual);
            goto ROPTest_exit;
         }
      }

      wOffset++;
      byRop = (BYTE) ((byRop + 1) & 0x03);
      byRotate = (BYTE) ((byRotate + 1) & 0x7);
      byData++;                        // Let byte wrap around
   }

ROPTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("ROP Test passed.");
   else
      SimComment ("ROP Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    4
//
//    T0504
//    WriteMode1Test - Draw pattern into memory and duplicate it using write mode 1
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int WriteMode1Test (void)
{
   int         nErr, i, nWidth;
   SEGOFF      lpVideo;
   BOOL     bFullVGA;
   WORD     wSimType, dest, nLength, j, wScanSize, xRes, yRes;
   BYTE     byExpected, byActual;
   BYTE     byData, byIncr;
   char     szBuffer[128];

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));

   SimSetState (TRUE, TRUE, FALSE);    // I/O, memory, no DAC
   SetMode (0x12);                     // Need the memory clear
   SimSetState (TRUE, TRUE, FALSE);

   lpVideo = (SEGOFF) 0xA0000000;

   // Keep it short for simulation
   GetResolution (&xRes, &yRes);
   nWidth = xRes / 8;
   if (bFullVGA)
   {
      wScanSize = 120;
   }
   else
   {
      wScanSize = 2;
   }
   sprintf (szBuffer, "Testing %dx%d mode, for %d scan lines.", xRes, yRes, wScanSize);
   SimComment (szBuffer);

   // Draw a pattern into memory
   SimComment ("Draw a pattern into memory.");
   byData = 0;
   byIncr = 1;
   for (i = 0; i < xRes; i++)
   {
      Line4 (i, 0, i, wScanSize - 1, byData);
      // Don't use a regular pattern
      if ((++byData & 0x0F) == 0)
      {
         byData += byIncr;
         byIncr++;
      }
   }

   SimDumpMemory ("T0504.VGA");

   SimComment ("Set write mode 1 and copy memory.");
   IOWordWrite (GDC_INDEX, 0x0105);       // Write mode 1
   IOWordWrite (SEQ_INDEX, 0x0F02);       // Enable all planes

   // Move "wScanSize" scan lines
   dest = wScanSize * nWidth;             // Nth scan line
   nLength = wScanSize * nWidth;          // Move N scan lines
   MoveBytes (lpVideo + dest, lpVideo, nLength);

   // Set bit mask and do it again
   SimComment ("Set the bit mask and do it again.");
   IOWordWrite (GDC_INDEX, 0x5508);       // Bit mask
   dest = (wScanSize * 2) * nWidth;
   MoveBytes (lpVideo + dest, lpVideo, nLength);

   // Set map mask and do it again
   SimComment ("Set the map mask and do it again.");
   IOWordWrite (GDC_INDEX, 0xFF08);       // Bit mask
   IOWordWrite (SEQ_INDEX, 0x0302);       // Map mask
   dest = (wScanSize * 3) * nWidth;
   MoveBytes (lpVideo + dest, lpVideo, nLength);

   // Read back memory and verify that it is what is expected
   SimComment ("Read back memory and verify that it is what is expected.");
   for (i = 0; i < 4; i++)
   {
      sprintf (szBuffer, "Read back plane %d.", i);
      SimComment (szBuffer);

      SimComment ("Read back write mode 1 copy.");
      IOByteWrite (GDC_INDEX, 0x04);
      IOByteWrite (GDC_DATA, (BYTE) i);      // Read map select
      dest = wScanSize * nWidth;          // Nth scan line
      for (j = 0; j < nLength; j++)
      {
         byExpected = MemByteRead (lpVideo + j);
         if (!MemByteTest (lpVideo + dest + j, byExpected, &byActual))
         {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                     dest + j, HIWORD (lpVideo), byExpected, byActual);
            goto WriteMode1Test_exit;
         }
      }

      SimComment ("Read back bitmask copy.");
      dest = (wScanSize * 2) * nWidth;    // (N*2)th scan line
      for (j = 0; j < nLength; j++)
      {
         byExpected = MemByteRead (lpVideo + j);
         if (!MemByteTest (lpVideo + dest + j, byExpected, &byActual))
         {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                     dest + j, HIWORD (lpVideo), byExpected, byActual);
            goto WriteMode1Test_exit;
         }
      }

      SimComment ("Read back map mask copy.");
      // Plane 0 & 1 only should compare
      dest = (wScanSize * 3) * nWidth;    // (N*3)th scan line
      if ((i == 0) || (i == 1))
      {
         for (j = 0; j < nLength; j++)
         {
            byExpected = MemByteRead (lpVideo + j);
            if (!MemByteTest (lpVideo + dest + j, byExpected, &byActual))
            {
               nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        dest + j, HIWORD (lpVideo), byExpected, byActual);
               goto WriteMode1Test_exit;
            }
         }
      }
      else
      {
         byExpected = 0;
         for (j = 0; j < nLength; j++)
         {
            if (!MemByteTest (lpVideo + dest + j, byExpected, &byActual))
            {
               nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        dest + j, HIWORD (lpVideo), byExpected, byActual);
               goto WriteMode1Test_exit;
            }
         }
      }
   }

WriteMode1Test_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Write Mode 1 Test passed.");
   else
      SimComment ("Write Mode 1 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    5
void SetLatchedBytes (SEGOFF, BYTE, int);
//
//    T0505
//    WriteMode2Test - Draw a series of rectangles without doing any I/O to change colors.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int WriteMode2Test (void)
{
   int         nErr;
   SEGOFF      lpVideo;
   WORD     wSimType;
   int         i, j;
   BYTE     byROP, byRotate, byData, byCPU, byExpected, byActual;
   BYTE     byMem;
   int         nBlockSize, nColSize;
   BOOL     bFullVGA;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));

   SimSetState (TRUE, TRUE, FALSE);       // I/O, no memory, no DAC
   SetMode (0x92);                        // Memory will be initialized later
   lpVideo = (SEGOFF) 0xA0000000;
   SimSetState (TRUE, TRUE, FALSE);       // I/O, memory, no DAC

   // Keep it short for simulation
   if (bFullVGA)
   {
      SimComment ("80 blocks and 30 columns.");
      nBlockSize = 80;
      nColSize = (480/16);
   }
   else
   {
      SimComment ("Use 5 blocks and 10 columns.");
      nBlockSize = 5;
      nColSize = 10;
   }

   SimComment ("Set write mode 2 and load several blocks of memory.");
   IOByteWrite (GDC_INDEX, 0x05);
   IOByteWrite (GDC_DATA, (BYTE) (IOByteRead (GDC_DATA) | 2)); // Write mode 2

   for (i = 0; i < 16; i++)                           // Cycle through each color
   {
      for (j = 0; j < nColSize*nBlockSize; j++)
         MemByteWrite (lpVideo++, (BYTE) i);
   }

   // Might have to break this function into two files.
   SimDumpMemory ("T0505.VGA");

   SimComment ("Cycle through each plane and verify that the actual data matches the expected data.");
   lpVideo = (SEGOFF) 0xA0000000;
   byData = byROP = byRotate = 0;
   for (i = 0; i < nBlockSize*16; i++)
   {
      IOByteWrite (GDC_INDEX, 0x03);
      IOByteWrite (GDC_DATA, (BYTE) (byRotate | (byROP << 3)));
      SetLatchedBytes (lpVideo, byData, nColSize);

      for (j = 0; j < 4; j++)                   // Cycle through each plane
      {
         IOWordWrite (GDC_INDEX, (j << 8) | 0x04);

         // Callwlate video memory byte
         byMem = 0;
         if ((i / nBlockSize) & (1 << j))
            byMem = 0xFF;

         // Callwlate CPU byte
         byCPU = 0;
         if (byData & (1 << j))
            byCPU = 0xFF;

         byExpected = byCPU;
         switch (byROP)
         {
            case 1:
               byExpected &= byMem;
               break;
            case 2:
               byExpected |= byMem;
               break;
            case 3:
               byExpected ^= byMem;
               break;
         }
         for (j = 0; j < nColSize; j++)
         {
            if (!MemByteTest (lpVideo + j, byExpected, &byActual))
            {
               nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo + j), HIWORD (lpVideo), byExpected, byActual);
               goto WriteMode2Test_exit;
            }
         }
      }

      lpVideo += nColSize;
      byData++;
      byROP = ((byROP + 1) & 0x03);
      byRotate = ((byRotate + 1) & 0x07);
   }

WriteMode2Test_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Write Mode 2 Test passed.");
   else
      SimComment ("Write Mode 2 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

//
//    SetLatchedBytes - Set a block of video memory after latching a data value
//
//    Entry:   lpDest      Pointer to destination
//          byData      CPU data value
//          nLength     Number of bytes to move
//    Exit: None
//
//    Note: Due to the way VGA latches work, only one BYTE at at time can
//          be moved into video memory. Therefore, the "C" routine, "_fmemset"
//          is useless since it doesn't do the latch load step and is also
//          optimized for speed which may use WORD or DWORD moves.
//
void SetLatchedBytes (SEGOFF lpDest, BYTE byData, int nLength)
{
   int   i;

   for (i = 0; i < nLength; i++)
   {
      MemByteRead (lpDest);
      MemByteWrite (lpDest++, byData);
   }
}

#undef   REF_TEST
#define  REF_TEST    6
#define  COUNT_BITMASKS       2
#define  CHR_INCER_1          3
#define  CHR_INCER_2          7
//
//    T0506
//    WriteMode3Test - Draw font images over a colored background without affecting the background.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int WriteMode3Test (void)
{
   int      nErr, i, j, k, l, nCountChars;
   SEGOFF   lpVideo;
   BYTE  byMemData, byCPUData, byExpected, byActual, byBitMask;
   BYTE  tblBitMasks[COUNT_BITMASKS] = {0x55, 0xFF};
   WORD  wSimType;
   BOOL  bFullVGA;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));
   lpVideo = (SEGOFF) 0xA0000000;

   SimSetState (TRUE, FALSE, FALSE);      // I/O, memory, no DAC
   SetMode (0x92);                     // Memory will be filled later
   SimSetState (TRUE, TRUE, TRUE);

   // If simulation, use only an eighth of the combinations
   if (bFullVGA)
   {
      nCountChars = 256;
      SimComment ("Use 256 character combinations.");
   }
   else
   {
      nCountChars = 16;
      SimComment ("Use 16 character combinations.");
   }

   // Fill memory with various data combinations
   SimComment ("Fill memory with various data combinations.");
   byMemData = 0x00;
   IOByteWrite (SEQ_INDEX, 0x02);
   for (i = 0; i < 16*nCountChars*COUNT_BITMASKS; i++)
   {
      for (j = 0; j < 4; j++)
      {
         IOByteWrite (SEQ_DATA, (BYTE) (0x01 << j));
         MemByteWrite (lpVideo, byMemData);
         byMemData += CHR_INCER_1;
      }
      lpVideo++;
   }

   IOWordWrite (SEQ_INDEX, 0x0F02);
   SimDumpMemory ("T0506.VGA");

   // Set write mode 3 and draw a combination of data into memory
   SimComment ("Set write mode 3 and draw a combination of data into memory.");
   lpVideo = (SEGOFF) 0xA0000000;
   IOWordWrite (GDC_INDEX, 0x0305);
   byCPUData = 0x00;
   for (i = 0; i < nCountChars; i++)
   {
      for (j = 0; j < COUNT_BITMASKS; j++)
      {
         IOWordWrite (GDC_INDEX, (tblBitMasks[j] << 8) | 0x08);
         IOByteWrite (GDC_INDEX, 0);
         for (k = 0; k < 16; k++)
         {
            IOByteWrite (GDC_DATA, (BYTE) k);
            MemByteRead (lpVideo);           // Load latches
            MemByteWrite (lpVideo, byCPUData);
            lpVideo++;
         }
      }
      byCPUData += CHR_INCER_2;
   }

   // Verify video data
   SimComment ("Verify the video data.");
   lpVideo = (SEGOFF) 0xA0000000;
   byMemData = 0x00;
   byCPUData = 0x00;
   IOByteWrite (GDC_INDEX, 0x04);
   for (i = 0; i < nCountChars; i++)         // Cycle through CPU data
   {
      for (j = 0; j < COUNT_BITMASKS; j++)   // Cycle through bitmasks
      {
         for (k = 0; k < 16; k++)            // Cycle through Set/Reset data
         {
            for (l = 0; l < 4; l++)          // Cycle through each plane
            {
               IOByteWrite (GDC_DATA, (BYTE) l);
               // Callwlate Set/Reset data
               byExpected = ((k & (1 << l)) == 0) ? 0x00 : 0xFF;
               byBitMask = byCPUData & tblBitMasks[j];
               byExpected = (byExpected & byBitMask) | (byMemData & ~byBitMask);
               if (!MemByteTest (lpVideo, byExpected, &byActual))
               {
                  nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                                    LOWORD (lpVideo), HIWORD (lpVideo),
                                    byExpected, byActual);
                  goto WriteMode3Test_exit;
               }
               byMemData += CHR_INCER_1;
            }
            lpVideo++;
         }
      }
      byCPUData += CHR_INCER_2;
   }

WriteMode3Test_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Write Mode 3 Test passed.");
   else
      SimComment ("Write Mode 3 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    7
//
//    T0507
//    VideoSegmentTest - Set the video segment to various locations and verify that memory exists.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int VideoSegmentTest (void)
{
   static SEGOFF  tblAddress[] = {
      (SEGOFF) 0xB0000000, (SEGOFF) 0xA000FFFF,
      (SEGOFF) 0xB0007FFF, (SEGOFF) 0xB8007FFF
   };
   int      nErr, i;
   BYTE  temp;
   char  szBuffer[128];

   nErr = ERROR_NONE;
   SimSetState (TRUE, FALSE, FALSE);         // I/O, no memory, no DAC
   SetMode (0x12);
   SimSetState (TRUE, TRUE, FALSE);       // I/O, no memory, no DAC
   SimDumpMemory ("T0507.VGA");

   for (i = 1; i < 4; i++)
   {
      IOByteWrite (GDC_INDEX, 0x06);
      temp = (BYTE) IOByteRead (GDC_DATA);
      temp = (BYTE) ((temp & 0xF3) | (i << 2));
      IOByteWrite (GDC_DATA, temp);

      sprintf (szBuffer, "Setting GDC[6] to %02Xh and testing memory access at location: %04X:%04X.", temp, HIWORD (tblAddress[i]), LOWORD (tblAddress[i]));
      SimComment (szBuffer);

      if (!RWMemoryTest (HIWORD (tblAddress[i]), LOWORD (tblAddress[i])))
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                  LOWORD (tblAddress[i]), HIWORD (tblAddress[i]),
                  0, 0xFF);
         break;
      }
   }

   if (nErr == ERROR_NONE)
      SimComment ("Video Segment Test passed.");
   else
      SimComment ("Video Segment Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    8
//
//    T0508
//    BitMaskTest - Fill video memory with at pattern and set all possible bit mask combinations.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int BitMaskTest (void)
{
   int         nErr, i, j, nBlockSize, nColSize;
   SEGOFF      lpVideo;
   BYTE     byMask, byChr, byData, byExpected, byActual, bySR;
   WORD     wSimType, wOffset;
   BOOL     bFullVGA;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));

   SimSetState (TRUE, TRUE, FALSE);       // I/O, memory, no DAC
   SetMode (0x92);                        // Memory will be initialized later
   SimSetState (TRUE, TRUE, TRUE);
   lpVideo = (SEGOFF) 0xA0000000;

   // Limit the test in simulation
   if (bFullVGA)
   {
      SimComment ("Use 30 blocks and 80 columns.");
      nBlockSize = (480/16);
      nColSize = 80;
   }
   else
   {
      SimComment ("Use 10 blocks and 4 columns.");
      nBlockSize = 10;
      nColSize = 4;
   }

   // Fill memory with a pattern
   SimComment ("Fill memory with a pattern.");
   IOWordWrite (GDC_INDEX, 0x0F01);       // Enable set/reset
   for (i = 0; i < 16; i++)
   {
      IOByteWrite (GDC_INDEX, 0x00);
      IOByteWrite (GDC_DATA, (BYTE) i);
      wOffset = i*nColSize*nBlockSize;
      for (j = 0; j < nColSize*nBlockSize; j++)
         MemByteWrite (lpVideo + wOffset++, 0);
   }

   SimDumpMemory ("T0508.VGA");

   IOWordWrite (GDC_INDEX, 0x0001);       // Disable set/reset

   // Cycle through all combinations of bit masks
   SimComment ("Cycle through all combinations of bit masks.");
   byMask = 0;
   byChr = 0xFF;
   while (LOWORD (lpVideo) < (WORD) (nColSize * nBlockSize * 16))
   {
      IOByteWrite (GDC_INDEX, 0x08);
      IOByteWrite (GDC_DATA, byMask);
      byMask++;
      MemByteRead (lpVideo);        // Load latches
      MemByteWrite (lpVideo++, byChr);
   }

   // Read all of memory and verify that it is correct
   SimComment ("Read all of memory and verify that it is correct.");
   lpVideo = (SEGOFF) 0xA0000000;
   byMask = 0;
   while (LOWORD (lpVideo) < (WORD) (nColSize * nBlockSize * 16))
   {
      // Callwlate the set/reset data
      bySR = (BYTE) ((LOWORD (lpVideo)) / ((WORD) (nColSize*nBlockSize)));

      // Read each plane
      for (i = 0; i < 4; i++)
      {
         // Callwlate the video data on this plane
         byData = bySR & (1 << i);
         if (byData != 0) byData = 0xFF;

         // Callwlate the new video data after the CPU write
         byExpected = (byData & ~byMask) | (byChr & byMask);

         // Set read map select and get the actual data
         IOByteWrite (GDC_INDEX, 0x04);
         IOByteWrite (GDC_DATA, (BYTE) i);

         if (!MemByteTest (lpVideo, byExpected, &byActual))
         {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                     LOWORD (lpVideo), HIWORD (lpVideo), byExpected, byActual);
            goto BitMaskTest_exit;
         }
      }
      byMask++;
      lpVideo++;
   }

BitMaskTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Bit Mask Test passed.");
   else
      SimComment ("Bit Mask Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    9
//
//    T0509
//    NonPlanarReadModeTest - Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int NonPlanarReadModeTest (void)
{
   int      nErr, i, nWriteBytes, lwerifyBytes, nBytes2;
   SEGOFF   lpVideo, lpTemp;
   WORD  wSimType;
   BOOL  bFullVGA;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));

   SimSetState (TRUE, TRUE, FALSE);       // I/O, memory, no DAC
   SetMode (0x93);                        // Memory will be initialized later
   lpVideo = (SEGOFF) 0xA0000000;
   SimSetState (TRUE, TRUE, FALSE);       // I/O, memory, no DAC

   if (bFullVGA)
   {
      nWriteBytes = 16*1024;
      lwerifyBytes = 0xFFFF;
      nBytes2 = 0x4000;
   }
   else
   {
      nWriteBytes = 1;
      lwerifyBytes = 4;
      nBytes2 = 0x10;
   }

   // Fill memory with a known pattern
   SimComment ("Fill memory with a known pattern.");
   for (i = 0; i < nWriteBytes; i++)
   {
      MemByteWrite (lpVideo++, 0x00);
      MemByteWrite (lpVideo++, 0x55);
      MemByteWrite (lpVideo++, 0xAA);
      MemByteWrite (lpVideo++, 0xFF);
   }

   SimDumpMemory ("T0509.VGA");

   SimComment ("Set read mode 1.");
   IOWordWrite (GDC_INDEX, 0x4805);       // Read mode 1
   lpVideo = (SEGOFF) 0xA0000000;
   lpTemp = VerifyBytes (lpVideo, 0, lwerifyBytes);
   if (lpTemp)
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpTemp),
                        HIWORD (lpTemp), 0, MemByteRead (lpTemp));
      goto NonPlanarReadModeTest;
   }

   SimComment ("Color compare for \"0101\" in planes 0..3.");
   IOWordWrite (GDC_INDEX, 0x0A02);    // Color compare for "0101" in planes 0..3
   lpTemp = VerifyBytes (lpVideo, 0x55, lwerifyBytes);
   if (lpTemp)
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpTemp),
                        HIWORD (lpTemp), 0x55, MemByteRead (lpTemp));
      goto NonPlanarReadModeTest;
   }

   SimComment ("Color compare for \"0011\" in planes 0..3.");
   IOWordWrite (GDC_INDEX, 0x0C02);    // Color compare for "0011" in planes 0..3
   lpTemp = VerifyBytes (lpVideo, 0xAA, lwerifyBytes);
   if (lpTemp)
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpTemp),
                        HIWORD (lpTemp), 0xAA, MemByteRead (lpTemp));
      goto NonPlanarReadModeTest;
   }

   // Now test chain/2 stuff
   SimComment ("Now test chain/2 stuff.");
   SetMode (0x85);                     // Memory will be initialized later
   lpVideo = (SEGOFF) 0xB8000000;
   for (i = 0; i < nBytes2; i++)
      MemByteWrite (lpVideo + i, 0x1B);

   SimComment ("Set read mode 1 and color don't care for non-visible planes.");
   IOWordWrite (GDC_INDEX, 0x3805);    // Read mode 1
   IOWordWrite (GDC_INDEX, 0x0307);    // Color "don't" care about non-visible planes
   lpTemp = VerifyBytes (lpVideo, 0xE4, nBytes2);
   if (lpTemp)
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpTemp),
                        HIWORD (lpTemp), 0xE4, MemByteRead (lpTemp));
      goto NonPlanarReadModeTest;
   }

NonPlanarReadModeTest:
   if (nErr == ERROR_NONE)
      SimComment ("Non-planar Read Mode Test passed.");
   else
      SimComment ("Non-planar Read Mode Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    10
//
//    T0510
//    NonPlanarWriteMode1Test - Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int NonPlanarWriteMode1Test (void)
{
   int      nErr, i, nWriteBytes, lwerifyBytes, nBytes2;
   SEGOFF   lpVideo;
   WORD  wSimType;
   BOOL  bFullVGA;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));

   SimSetState (TRUE, TRUE, FALSE);          // I/O, memory, no DAC
   SetMode (0x12);                           // To clear all planes of planar memory
   SimSetState (TRUE, FALSE, FALSE);            // I/O, no memory, no DAC
   SetMode (0x93);                           // No need to clear memory a second time
   SimSetState (TRUE, TRUE, TRUE);
   lpVideo = (SEGOFF) 0xA0000000;

   if (bFullVGA)
   {
      nWriteBytes = 4*1024;
      lwerifyBytes = 0x4000;
      nBytes2 = 0x2000;
   }
   else
   {
      nWriteBytes = 0x10;
      lwerifyBytes = 0x40;
      nBytes2 = 0x10;
   }

   // Fill first 4K of memory with a known pattern
   SimComment ("Fill first block of memory with a known pattern.");
   for (i = 0; i < nWriteBytes; i++)
   {
      MemByteWrite (lpVideo++, 0x00);
      MemByteWrite (lpVideo++, 0x55);
      MemByteWrite (lpVideo++, 0xAA);
      MemByteWrite (lpVideo++, 0xFF);
   }

   SimDumpMemory ("T0510.VGA");

   SimComment ("Set write mode 1.");
   IOWordWrite (GDC_INDEX, 0x4105);    // Write mode 1
   lpVideo = (SEGOFF) 0xA0000000;
   for (i = 0; i < lwerifyBytes; i += 4)
   {
      MemByteRead (lpVideo + i);
      MemByteWrite (lpVideo + 0x4001 + i, 0);
      MemByteWrite (lpVideo + 0x8002 + i, 0);
      MemByteWrite (lpVideo + 0xC003 + i, 0);
   }

   // Even though the map mask enables all planes, the chain/4 only allows
   // the write to go to one plane (based on memory addressing).
   // Verify memory
   SimComment ("Verify memory.");
   for (i = 0; i < lwerifyBytes; i += 4)
   {
      // 000h, 055h, 000h, 000h
      if (MemByteRead (lpVideo + 0x4000 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x4000 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0x4000 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0x4001 + i) != 0x55)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x4001 + i),
                           HIWORD (lpVideo), 0x55, MemByteRead (lpVideo + 0x4001 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0x4002 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x4002 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0x4002 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0x4003 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x4003 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0x4003 + i));
         goto NonPlanarWriteMode1Test;
      }

      // 000h, 000h, 0AAh, 000h
      if (MemByteRead (lpVideo + 0x8000 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x8000 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0x8000 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0x8001 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x8001 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0x8001 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0x8002 + i) != 0xAA)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x8002 + i),
                           HIWORD (lpVideo), 0xAA, MemByteRead (lpVideo + 0x8002 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0x8003 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x8003 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0x8003 + i));
         goto NonPlanarWriteMode1Test;
      }

      // 000h, 000h, 000h, 0FFh
      if (MemByteRead (lpVideo + 0xC000 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0xC000 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0xC000 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0xC001 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0xC001 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0xC001 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0xC002 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0xC002 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0xC002 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0xC003 + i) != 0xFF)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0xC003 + i),
                           HIWORD (lpVideo), 0xFF, MemByteRead (lpVideo + 0xC003 + i));
         goto NonPlanarWriteMode1Test;
      }
   }

   // Now test chain/2 stuff
   SimComment ("Now test chain/2 stuff.");
   SetMode (0x85);                     // Memory will be initialized later
   lpVideo = (SEGOFF) 0xB8000000;
   for (i = 0; i < nBytes2; i++)
      MemByteWrite (lpVideo + i, 0x1B);

   SimComment ("Set write mode 1.");
   IOWordWrite (GDC_INDEX, 0x3105);    // Write mode 1
   for (i = 0; i < nBytes2; i += 2)
   {
      MemByteRead (lpVideo + i);
      MemByteWrite (lpVideo + 0x2001 + i, 0);
   }

   // Even though the map mask enables plane 0 & 1, the chain/2 only allows
   // the write to go to one plane (based on memory addressing).
   // Verify memory
   SimComment ("Verify memory.");
   for (i = 0; i < nBytes2; i += 2)
   {
      // 000h, 01Bh
      if (MemByteRead (lpVideo + 0x2000 + i) != 0x00)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x2000 + i),
                           HIWORD (lpVideo), 0x00, MemByteRead (lpVideo + 0x2000 + i));
         goto NonPlanarWriteMode1Test;
      }
      if (MemByteRead (lpVideo + 0x2001 + i) != 0x1B)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                           LOWORD (lpVideo + 0x2001 + i),
                           HIWORD (lpVideo), 0x1B, MemByteRead (lpVideo + 0x2001 + i));
         goto NonPlanarWriteMode1Test;
      }
   }

NonPlanarWriteMode1Test:
   if (nErr == ERROR_NONE)
      SimComment ("Non-planar Write Mode 1 Test passed.");
   else
      SimComment ("Non-planar Write Mode 1 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    11
//
//    T0511
//    NonPlanarWriteMode2Test - Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int NonPlanarWriteMode2Test (void)
{
   int      nErr, j, nTblSize, nTblIdx;
   SEGOFF   lpVideo;
   DWORD l, offset, dwMemSize;
   BYTE  byActual, byExpected, byHistory[4];
   WORD  wSimType;
   BOOL  bFullVGA;
   // Note that the low order nibble is used for the chain/4 test and
   // the low order two bits are used for the chain/2 test. All other
   // bits will (are supposed to) be ignored by the hardware.
   // Use an odd number of values to force a rotation through the
   // memory byte ordering (so that 0 isn't always going to byte 0,
   // 1 isn't always going to byte 1, etc.).
   BYTE     byTblValues[] = {
            0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87,
            0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F,
            0x66, 0x99, 0x55, 0xAA, 0xFF, 0x00, 0x88
            };

   nErr = ERROR_NONE;
   nTblSize = sizeof (byTblValues);
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));
   SetMode (0x12);                           // To clear memory

   SimSetState (TRUE, TRUE, FALSE);          // I/O, memory, no DAC
   SetMode (0x93);                           // No need to clear a 2nd time
   lpVideo = (SEGOFF) 0xA0000000;

   if (bFullVGA)
   {
      dwMemSize = 1000;
   }
   else
   {
      dwMemSize = 100;
   }

   // Just use the first 1000 bytes so that the strange memory wrapping
   // from the high order memory address bits is not a factor here. That
   // functionality is tested in another test. (For reference, the VGA,
   // in chain/4 mode, wraps the two high order address bits down to where
   // the bits used as the plane selects were.)
   nTblIdx = 0;
   SimComment ("Test each of a block of bytes for write thru each plane.");
   for (l = 0; l < dwMemSize; l++)
   {
      // Set write mode 2 and write a test value across the planes
      IOWordWrite (GDC_INDEX, 0x4205);
      IOWordWrite (SEQ_INDEX, 0x0E04);       // Restore value
      MemByteWrite (lpVideo + l, byTblValues[nTblIdx]);

      // Save a history of the values written to each chained plane. The
      // value only affects the particular addressed plane, since the
      // chain/4 addressing blocks writes based on the MA0..1 bits
      // which are used as plane selects.
      if ((l & 3) == 0)
         byHistory[0] = byHistory[1] = byHistory[2] = byHistory[3] = 0;
      byHistory[l & 3] = byTblValues[nTblIdx];

      // Set planar mode
      IOWordWrite (GDC_INDEX, 0x0005);
      IOWordWrite (SEQ_INDEX, 0x0604);

      // Test the values from each byte of the planes
      for (j = 0; j < 4; j++)
      {
         IOWordWrite (GDC_INDEX, (j << 8) | 0x04);    // Set read plane select
         for (offset = (l & ~3); offset < ((l & ~3) + 4); offset++)
         {
            // Callwlate the expected values. Note that only the byte
            // of the addressed location (none of the other planes) is
            // affected. Therefore, a history of the 4 bytes written to the
            // chained address is used to callwlate the expected value.
            if ((offset & 3) != 0)
               byExpected = 0;
            else
               byExpected = (byHistory[j] & (1 << j)) ? 0xFF : 0x00;

            byActual = MemByteRead (lpVideo + offset);
            if (byExpected != byActual)
            {
               nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpVideo + offset), HIWORD (lpVideo), byExpected, byActual);
               goto NonPlanarWriteMode2Test;
            }
         }
      }
      if (++nTblIdx >= nTblSize) nTblIdx = 0;
   }
   SimDumpMemory ("T0511A.VGA");

   // Now, verify that CGA mode and chain/2 work as expected
   SimComment ("Now, verify that CGA mode and chain/2 work as expected.");
   SetMode (0x05);                  // Need to clear memory!
   lpVideo = (SEGOFF) 0xB8000000;

   SimComment ("Test each of a block of bytes for write thru each plane.");
   nTblIdx = 0;
   for (l = 0; l < dwMemSize; l++)
   {
      // Set write mode 2 and write a test value across the planes
      IOWordWrite (GDC_INDEX, 0x3205);
      IOWordWrite (GDC_INDEX, 0x0F06);
      IOWordWrite (SEQ_INDEX, 0x0204);       // Restore values
      MemByteWrite (lpVideo + l, byTblValues[nTblIdx]);

      // Save a history of the values written to each chained plane. The
      // value only affects the particular addressed plane, since the
      // chain/2 addressing blocks writes based on the MA0 bit which
      // is used as a plane select.
      if ((l & 1) == 0)
         byHistory[0] = byHistory[1] = 0;
      byHistory[l & 1] = byTblValues[nTblIdx];

      // Set planar mode
      IOWordWrite (GDC_INDEX, 0x0005);
      IOWordWrite (GDC_INDEX, 0x0D06);
      IOWordWrite (SEQ_INDEX, 0x0604);

      // Test the values from each byte of the planes
      for (j = 0; j < 2; j++)
      {
         IOWordWrite (GDC_INDEX, (j << 8) | 0x04);    // Set read plane select
         for (offset = (l & ~1); offset < ((l & ~1) + 2); offset++)
         {
            // Callwlate the expected values. Note that only the byte
            // of the addressed location (none of the other planes) is
            // affected. Therefore, a history of the 2 bytes written to the
            // chained address is used to callwlate the expected value.
            if ((offset & 1) != 0)
               byExpected = 0;
            else
               byExpected = (byHistory[j] & (1 << j)) ? 0xFF : 0x00;

            byActual = MemByteRead (lpVideo + offset);
            if (byExpected != byActual)
            {
               nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST, LOWORD (lpVideo + offset), HIWORD (lpVideo), byExpected, byActual);
               goto NonPlanarWriteMode2Test;
            }
         }
      }
      if (++nTblIdx >= nTblSize) nTblIdx = 0;
   }
   SimDumpMemory ("T0511B.VGA");

NonPlanarWriteMode2Test:
   if (nErr == ERROR_NONE)
      SimComment ("Non-planar Write Mode 2 Test passed.");
   else
      SimComment ("Non-planar Write Mode 2 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    12
//
//    T0512
//    NonPlanarWriteMode3Test - Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int NonPlanarWriteMode3Test (void)
{
   int      nErr, i;
   SEGOFF   lpVideo;
   DWORD offset, dwMemSize;
   BYTE  bySetReset, byMask, temp, byMemData, plane, byData;
   WORD  wSimType;
   BOOL  bFullVGA;

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   bFullVGA = SimGetFrameSize () && (!(wSimType & SIM_SIMULATION));

   SimSetState (TRUE, TRUE, FALSE);       // I/O, memory, no DAC
   SetMode (0x93);                        // No need to clear memory
   lpVideo = (SEGOFF) 0xA0000000;
   SimSetState (TRUE, TRUE, TRUE);           // I/O, memory, DAC

   if (bFullVGA)
   {
      dwMemSize = 0x10000;
   }
   else
   {
      dwMemSize = 0x100;
   }

   // Preload memory
   for (offset = 0; offset < dwMemSize; offset += 256)
   {
      temp = (BYTE) (offset / 256);
      for (i = 0; i < 256; i++)
         MemByteWrite (lpVideo + offset + i, temp);
   }

   SimDumpMemory ("T0512.VGA");

   IOWordWrite (GDC_INDEX, 0x4305);                // Set write mode 3

   // Fill with a pattern
   SimComment ("Fill with a pattern.");
   byMask = 0;
   bySetReset = 0;
   for (offset = 0; offset < dwMemSize; offset++)
   {
      IOByteWrite (GDC_INDEX, 0x00);
      IOByteWrite (GDC_DATA, bySetReset);
      temp = MemByteRead (lpVideo + offset);       // Load latches
      MemByteWrite (lpVideo + offset, byMask);
      byMask += 3;
      bySetReset++;
   }

   // Verify memory
   SimComment ("Verify memory.");
   byMask = 0;
   bySetReset = 0;
   for (offset = 0; offset < dwMemSize; offset++)
   {
      byMemData = (BYTE) (offset / 256);
      plane = (BYTE) (offset % 4);
      byData = (bySetReset & (1 << plane)) ? 0xFF : 0x00;
      byData = byData & byMask;
      byMemData = byMemData & ~byMask;
      byData = byData | byMemData;
      if (MemByteRead (lpVideo + offset) != byData)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo + offset),
                        HIWORD (lpVideo), byData, MemByteRead (lpVideo + offset));
         goto NonPlanarWriteMode3Test;
      }
      byMask += 3;
      bySetReset++;
   }

   // Now chain/2 mode
   SimComment ("Now chain/2 mode.");
   SimSetState (TRUE, TRUE, FALSE);
   SetMode (0x85);                  // Memory clear not needed
   SimSetState (TRUE, TRUE, TRUE);
   lpVideo = (SEGOFF) 0xB8000000;

   // Preload memory
   SimComment ("Load memory.");
   for (offset = 0; offset < dwMemSize / 2; offset += 128)
   {
      temp = (BYTE) (offset / 128);
      for (i = 0; i < 128; i++)
         MemByteWrite (lpVideo + offset + i, temp);
   }

   SimComment ("Set write mode 3 and fill with a pattern.");
   IOWordWrite (GDC_INDEX, 0x3305);                // Set write mode 3

   // Fill with a pattern
   byMask = 0;
   bySetReset = 0;
   for (offset = 0; offset < dwMemSize / 2; offset++)
   {
      IOByteWrite (GDC_INDEX, 0x00);
      IOByteWrite (GDC_DATA, bySetReset);
      temp = MemByteRead (lpVideo + offset);       // Load latches
      MemByteWrite (lpVideo + offset, byMask);
      byMask += 3;
      bySetReset++;
   }

   // Verify memory
   SimComment ("Verify memory.");
   byMask = 0;
   bySetReset = 0;
   for (offset = 0; offset < dwMemSize / 2; offset++)
   {
      byMemData = (BYTE) (offset / 128);
      plane = (BYTE) (offset % 2);
      byData = (bySetReset & (1 << plane)) ? 0xFF : 0x00;
      byData = byData & byMask;
      byMemData = byMemData & ~byMask;
      byData = byData | byMemData;
      if (MemByteRead (lpVideo + offset) != byData)
      {
         nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo + offset),
                        HIWORD (lpVideo), byData, MemByteRead (lpVideo + offset));
         goto NonPlanarWriteMode3Test;
      }
      byMask += 3;
      bySetReset++;
   }

NonPlanarWriteMode3Test:
   if (nErr == ERROR_NONE)
      SimComment ("Non-planar Write Mode 3 Test passed.");
   else
      SimComment ("Non-planar Write Mode 3 Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    13
//
//    ShiftRegisterModeTest - Fill memory with a pattern and read back for an expected value in both mode 13h and mode 05h.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int ShiftRegisterModeTest (void)
{
   int         nErr, n;
   SEGOFF      lpVideo;
   DWORD    offset, dwMax;
   BYTE     pattern[] = {0x82, 0x22, 0x0A, 0x2};
   BYTE     red[] = {0x00, 0x00, 0x2A, 0x00};
   BYTE     green[] = {0x00, 0x2A, 0x00, 0x2A};
   BYTE     blue[] = {0x2A, 0x00, 0x00, 0x00};
   BYTE     pattern2[] = {0x55, 0x33};
   BOOL     bFullVGA;

   nErr = ERROR_NONE;
   bFullVGA = SimGetFrameSize ();
   if (!DisplayInspectionMessage ())
      goto ShiftRegisterModeTest;

   SimComment ("Start with mode 13h.");
   SimSetState (TRUE, TRUE, FALSE);          // I/O, memory, no DAC
   SetMode (0x13);
   SimSetState (FALSE, FALSE, FALSE);
   lpVideo = (SEGOFF) 0xA0000000;
   offset = 0;
   n = 0;
   SimSetState (TRUE, TRUE, TRUE);

   SimComment ("Load a known pattern.");
   dwMax = bFullVGA ? 0x10000 : 0x2000;
   for (offset = 0; offset < dwMax; offset++)
   {
      MemByteWrite (lpVideo++, pattern[n]);
      if (++n >= (int) sizeof (pattern)) n = 0;
   }

   SimDumpMemory ("T0513.VGA");
   SimSetState (TRUE, TRUE, TRUE);

   // Make the screen somewhat aesthetically pleasing (Note: don't mess
   // with any DAC index that is used cross-plane. For example, DAC[02h]
   // should always be 00h,2Ah,00h). Screen should be B,G,R,G at this point.
   SimComment ("Set strategic DAC registers.");
   for (n = 0; n < (int) sizeof (pattern); n++)
   {
      SetDac (pattern[n], red[n], green[n], blue[n]);
   }

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto ShiftRegisterModeTest;
      }
   }

   // Set cross planar shifting. Screen should be B,G,R,W at this point.
   SimComment ("Set cross planar shifting. Screen should be B,G,R,W at this point.");
   IOWordWrite (GDC_INDEX, 0x0005);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto ShiftRegisterModeTest;
      }
   }

   SimComment ("Now use mode 05h.");
   SetMode (0x05);
   lpVideo = (SEGOFF) 0xB8000000;
   offset = 0;

   // Fill the screen with a pattern. Screen should be C,C,C,C,K,W,K,W.
   SimComment ("Fill the screen with a pattern. Screen should be C,C,C,C,K,W,K,W.");
   n = 0;
   dwMax = bFullVGA ? 0x8000 : 0x1000;
   for (offset = 0; offset < dwMax; offset++)
   {
      MemByteWrite (lpVideo++, pattern2[n]);
      if (++n >= (int) sizeof (pattern2)) n = 0;
   }
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto ShiftRegisterModeTest;
      }
   }

   // Set cross planar shifting. Screen should be K,C,M,W at this point.
   SimComment ("Set cross planar shifting. Screen should be K,C,M,W at this point.");
   IOWordWrite (GDC_INDEX, 0x1005);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto ShiftRegisterModeTest;
      }
   }

ShiftRegisterModeTest:
   if (nErr == ERROR_NONE)
      SimComment ("Shift Register Mode Test completed, images produced.");
   else
      SimComment ("Shift Register Mode Test failed.");
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
