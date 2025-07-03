//
//    PART7.CPP - VGA Core Test Suite (Part 7)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 6 May 2005
//
//    Routines in this file:
//    SyncPolarityTest        Set each of the four possible sync polarity settings.
//    PageSelectTest          Place values into memory in different page bit configurations and verify that those values ended up where they were supposed to go.
//    ClockSelectsTest        Set the standard clocks and measure the results.
//    RAMEnableTest           Test a byte of memory, disable memory and test it again.
//    CRTCAddressTest            Set the CRTC I/O address to each location and verify that it is addressable.
//    SwitchReadbackTest         Attempt to do an EGA-style switch readback. On a VGA this yields 0Fh. Then attempt to do a monitor detect using the switch sense bit.
//    SyncStatusTest          Verify that the sync signal changes and that the rate is accurate.
//    VSyncOrVDispTest        Visually inspect the sync pulse widened from vertical display end to display start.
//
#include <stdio.h>
#include <stdlib.h>

#include    <assert.h>

#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int SyncPolarityTest (void);
int PageSelectTest (void);
int ClockSelectsTest (void);
int RAMEnableTest (void);
int CRTCAddressTest (void);
int SwitchReadbackTest (void);
int SyncStatusTest (void);
int VSyncOrVDispTest (void);

#define  REF_PART    7
#define  REF_TEST    1
//
//    T0701
//    SyncPolarityTest - Set each of the four possible sync polarity settings.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SyncPolarityTest (void)
{
   int      nErr, i;
   BYTE  misc;
   WORD  wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto SyncPolarityTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);
   GetResolution (&wXRes, &wYRes);

   SetLine4Columns (wXRes / 8);
   Line4 (0, 0, wXRes - 1, 0, 0x0F);
   Line4 (wXRes - 1, 0, wXRes - 1, wYRes - 1, 0x0F);
   Line4 (0, wYRes - 1, wXRes - 1, wYRes - 1, 0x0F);
   Line4 (0, 0, 0, wYRes - 1, 0x0F);
   Line4 (0, 0, wXRes - 1, wYRes - 1, 0x0F);
   Line4 (0, wYRes - 1, wXRes - 1, 0, 0x0F);

   SimDumpMemory ("T0701.VGA");

   misc = IOByteRead (MISC_INPUT);
   for (i = 0; i < 4; i++)
   {
      IOByteWrite (MISC_OUTPUT, (BYTE) ((misc & 0x3F) | i << 6));

      // Changing polarities may effect a frame capture, so give the lib a chance to handle it
      SimModeDirty ();

      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto SyncPolarityTest_exit;
         }
      }
   }

SyncPolarityTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T0702
//    PageSelectTest - Place values into memory in different page bit configurations and verify that those values ended up where they were supposed to go.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int PageSelectTest (void)
{
   int      nErr, i;
   SEGOFF   lpVideo;
   BYTE  byExpected, byActual;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto PageSelectTest_exit;

   SimSetState (TRUE, TRUE, FALSE);             // Ignore DAC writes
   SetMode (0x03);
   lpVideo = (SEGOFF) 0xB8000000;
   SimDumpMemory ("T0702.VGA");

   // Clear page select bit, the ilwerse of which replaces LA0 in chain/2 mode
   IOByteWrite (MISC_OUTPUT, (BYTE) (IOByteRead (MISC_INPUT) & 0xDF));
   MemByteWrite (lpVideo++, '0');
   MemByteWrite (lpVideo++, '1');
   MemByteWrite (lpVideo++, '2');
   MemByteWrite (lpVideo, '3');
   IOByteWrite (MISC_OUTPUT, (BYTE) (IOByteRead (MISC_INPUT) | 0x20));

   // Force into "linear non-chained" mode
   IOWordWrite (GDC_INDEX, 0x0005);
   IOWordWrite (GDC_INDEX, 0x0506);
   IOWordWrite (SEQ_INDEX, 0x0604);

   // Verify memory is as it should be
   lpVideo = (SEGOFF) 0xA0000001;
   byExpected = '0';
   if (!MemByteTest (lpVideo, byExpected, &byActual))
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo), HIWORD (lpVideo), byExpected, byActual);
      goto PageSelectTest_exit;
   }
   byExpected = '2';
   if (!MemByteTest (lpVideo + 2, byExpected, &byActual))
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo + 2), HIWORD (lpVideo), byExpected, byActual);
      goto PageSelectTest_exit;
   }
   IOWordWrite (GDC_INDEX, 0x0104);
   byExpected = '1';
   if (!MemByteTest (lpVideo, byExpected, &byActual))
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo), HIWORD (lpVideo), byExpected, byActual);
      goto PageSelectTest_exit;
   }
   byExpected = '3';
   if (!MemByteTest (lpVideo + 2, byExpected, &byActual))
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo + 2), HIWORD (lpVideo), byExpected, byActual);
      goto PageSelectTest_exit;
   }

   // Verify page select can be ilwerted via SEQ[4].1
   // (This should make a blinking purple screen)
   SetMode (0x03);
   lpVideo = (SEGOFF) 0xB8000000;
   IOByteWrite (MISC_OUTPUT, (BYTE) (IOByteRead (MISC_INPUT) & 0xDF));
   IOWordWrite (SEQ_INDEX, 0x0004);

   for (i = 0; i < 0x1000; i++)
      MemByteWrite (lpVideo + i, 0xDB);

   WaitAttrBlink (BLINK_ON);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

PageSelectTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T0703
//    ClockSelectsTest - Set the standard clocks and measure the results.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int ClockSelectsTest (void)
{
   int      nErr;
   BYTE  temp;
   DWORD hz, khz, mhz, expected, lower, upper;
   WORD  wSimType;

   nErr = ERROR_NONE;

   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
   {
      printf ("\nThis test ilwolves system timings and will not function aclwrately"
               "\nin a simulated environment.\n");
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      return (nErr);
   }

   SetMode (0x03);
   TextStringOut ("The following test is self-running and will take about 10 seconds.", ATTR_NORMAL, 1, 1, 0);
   TextStringOut ("Press any key to continue, <ESC> to exit.", ATTR_NORMAL, 1, 3, 0);
   if (SimGetKey () == KEY_ESCAPE)
      goto ClockSelectsTest_exit;

   IOWordWrite (SEQ_INDEX, 0x0100);    // Sync reset
   temp = IsIObitFunctional (MISC_INPUT, MISC_OUTPUT, 0xF3);
   IOWordWrite (SEQ_INDEX, 0x0300);    // End sync reset
   if (temp != 0)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
               MISC_OUTPUT, 0, 0, temp);
      goto ClockSelectsTest_exit;
   }

   SetMode (0x12);               // Set 25.175 MHz clock
   hz = GetFrameRate ();
   khz = (hz * 525) / 1000;      // Vertical total is 525 lines
   mhz = (khz * 800) / 1000;     // Horizontal total is 800 pixels

   // Callwlate range of "okay" values (within 5%)
   expected = 25175;
   lower = expected - expected/20;
   upper = expected + expected/20;
   if ((mhz < lower) || (mhz > upper))
   {
      nErr = FlagError (ERROR_ILWALIDCLOCK, REF_PART, REF_TEST,
               LOWORD (expected), HIWORD (expected), LOWORD (mhz), HIWORD (mhz));
      goto ClockSelectsTest_exit;
   }

   SetMode (0x03);               // Set 28.321 MHz clock
   hz = GetFrameRate ();
   khz = (hz * 449) / 1000;      // Vertical total is 449 lines
   mhz = (khz * 900) / 1000;     // Horizontal total is 900 pixels

   // Callwlate range of "okay" values (within 5%)
   expected = 28321;
   lower = expected - expected/20;
   upper = expected + expected/20;
   if ((mhz < lower) || (mhz > upper))
   {
      nErr = FlagError (ERROR_ILWALIDCLOCK, REF_PART, REF_TEST,
               LOWORD (expected), HIWORD (expected), LOWORD (mhz), HIWORD (mhz));
      goto ClockSelectsTest_exit;
   }

ClockSelectsTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    4
//
//    T0704
//    RAMEnableTest - Test a byte of memory, disable memory and test it again.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int RAMEnableTest (void)
{
   int         nErr;
   SEGOFF      lpVideo;

   nErr = ERROR_NONE;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x92);                           // Memory doesn't matter
   SimSetState (TRUE, TRUE, TRUE);
   lpVideo = (SEGOFF) 0xA0000000;
   SimDumpMemory ("T0704.VGA");

   if (!RWMemoryTest (HIWORD (lpVideo), LOWORD (lpVideo)))
   {
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo), HIWORD (lpVideo), 0, 0xFF);
      goto RAMEnableTest_exit;
   }

   IOByteWrite (MISC_OUTPUT, (BYTE) (IOByteRead (MISC_INPUT) & 0xFD));  // Disable memory
   if (RWMemoryTest (HIWORD (lpVideo), LOWORD (lpVideo)))
      nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                        LOWORD (lpVideo), HIWORD (lpVideo), 0xFF, 0);

RAMEnableTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    5
//
//    T0705
//    CRTCAddressTest - Set the CRTC I/O address to each location and verify that it is addressable.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int CRTCAddressTest (void)
{
   static WORD wtblCRTC[] = {CRTC_MINDEX, CRTC_CINDEX};
   int         nErr, i;
   BYTE     temp;
   WORD     wCRTC;

   nErr = ERROR_NONE;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x03);
   SimSetState (TRUE, TRUE, FALSE);
   SimDumpMemory ("T0705.VGA");

   for (i = 0; i < 2; i++)
   {
      temp = (BYTE) IOByteRead (MISC_INPUT);
      temp = (BYTE) ((temp & 0xFE) | i);
      IOByteWrite (MISC_OUTPUT, temp);
      wCRTC = wtblCRTC[i];
      if ((temp = IsIObitFunctional (wCRTC, wCRTC, 0xC0)) != 0)
      {
         nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, wCRTC, 0, 0, temp);
         goto CRTCAddressTest_exit;
      }
   }

CRTCAddressTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    6
//
//    T0706
//    SwitchReadbackTest - Attempt to do an EGA-style switch readback. On a VGA this yields 0Fh. Then attempt to do a monitor detect using the switch sense bit.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SwitchReadbackTest (void)
{
    unsigned i;
   int   nErr;
   BYTE  misc, switches, temp, orgmisc;
   WORD  trigger, tmp, level, wSimType;
   BYTE  tblDAC[] = {0x00, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20,
                     0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C, 0x3F};

   nErr = ERROR_NONE;

   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
   {
      printf ("\n\nThis test ilwolves system timings and will not function aclwrately"
               "\nin a simulated environment.\n");
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      return (nErr);
   }

   SimSetState (TRUE, TRUE, FALSE);             // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);
   SimDumpMemory ("T0706.VGA");

   // Attempt to read EGA-style switches (meaningless on the VGA)
   switches = 0;
   orgmisc = IOByteRead (MISC_INPUT);
   misc = orgmisc & 0xF3;
   for (i = 0; i < 4; i++)
   {
      IOByteWrite (MISC_OUTPUT, (BYTE) (misc | (i << 2)));
      temp = IOByteRead (INPUT_STATUS_0);
      switches = switches | ((temp & 0x10) >> 4) << (3 - i);
   }
   IOByteWrite (MISC_OUTPUT, orgmisc);

   // Now read it as the monitor sense (analog comparator)
   trigger = 0;
   WaitNotVerticalRetrace ();
   for (i = 0; i < sizeof (tblDAC); i++)
   {
      FillDAC (tblDAC[i], tblDAC[i], tblDAC[i]);
      _disable ();
      while ((IOByteRead (INPUT_CSTATUS_1) & 0x01) == 0x00);
      while ((IOByteRead (INPUT_CSTATUS_1) & 0x01) == 0x01);
      temp = IOByteRead (INPUT_STATUS_0);
      _enable ();
      trigger |= (BYTE) (((temp & 0x10) >> 4) << i);
   }

   // Find first trigger
   tmp = trigger;
   i = 0;
   while ((tmp & 0x0001) != 0)
   {
      tmp = tmp >> 1;
      i++;
   }
   level = (WORD) ((((long) tblDAC[i]) * 100l) / 64);

   // If the level never triggered or triggered on a "0" value, then the
   // comparator isn't working properly.
   if ((i == 0) || (i >= sizeof (tblDAC)))
   {
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

   SystemCleanUp ();
   if (i < sizeof (tblDAC))
   {
      printf ("\nEGA-style switches = %02Xh"
              "\lwGA trigger = %04Xh; DAC = (%02Xh, %02Xh, %02Xh); Level = %d%%",
              switches, trigger, tblDAC[i], tblDAC[i], tblDAC[i], level);
   }
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    7
//
//    T0707
//    SyncStatusTest - Verify that the sync signal changes and that the rate is accurate.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int SyncStatusTest (void)
{
   int         nErr;
   DWORD    time0, time1;
   BOOL     bChanged;
   BYTE     temp;
   int         nLines, nFrames;
   WORD     wSimType;

   nErr = ERROR_NONE;
   temp = 0;                        // Prevent warnings on stupid compilers
   wSimType = SimGetType ();
   if ((wSimType & SIM_VECTORS) || (wSimType & SIM_SIMULATION))
   {
      printf ("\nThis test ilwolves system timings and will not function aclwrately"
               "\nin a simulated environment.\n");
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      return (nErr);
   }

   SetMode (0x03);
   TextStringOut ("The following test is self-running and will take about 5 seconds.", ATTR_NORMAL, 1, 1, 0);
   TextStringOut ("Press any key to continue, <ESC> to exit.", ATTR_NORMAL, 1, 3, 0);
   if (SimGetKey () == KEY_ESCAPE)
      goto SyncStatusTest_exit;
   SetMode (0x12);

   // Wait for not vertical retrace
   time0 = GetSystemTicks ();
   time1 = time0 + FIVE_SECONDS;
   bChanged = FALSE;
   while (!bChanged && (time0 < time1))
   {
      if ((IOByteRead (INPUT_CSTATUS_1) & 0x08) == 0x00) bChanged = TRUE;
      time0 = GetSystemTicks ();
   }

   // Did we time out?
   if (!bChanged)
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            LOWORD (60000l), HIWORD (60000l), 0, 0);
      goto SyncStatusTest_exit;
   }

   // Wait for vertical retrace
   time0 = GetSystemTicks ();
   time1 = time0 + FIVE_SECONDS;
   bChanged = FALSE;
   while (!bChanged && (time0 < time1))
   {
      if (((temp = IOByteRead (INPUT_CSTATUS_1)) & 0x08) == 0x08) bChanged = TRUE;
      time0 = GetSystemTicks ();
   }

   // Did we time out?
   if (!bChanged)
   {
      nErr = FlagError (ERROR_ILWALIDFRAMES, REF_PART, REF_TEST,
            LOWORD (60000l), HIWORD (60000l), 0, 0);
      goto SyncStatusTest_exit;
   }
   // Is display disabled (it should be)?
   if ((temp & 1) == 0)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, INPUT_CSTATUS_1, 0, 1, 0);
      goto SyncStatusTest_exit;
   }

   // Wait for display enable
   time0 = GetSystemTicks ();
   time1 = time0 + FIVE_SECONDS;
   bChanged = FALSE;
   while (!bChanged && (time0 < time1))
   {
      if (((temp = IOByteRead (INPUT_CSTATUS_1)) & 0x01) == 0x00) bChanged = TRUE;
      time0 = GetSystemTicks ();
   }

   // Did we time out?
   if (!bChanged)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, INPUT_CSTATUS_1, 0, 0, 1);
      goto SyncStatusTest_exit;
   }

   // If everything is working properly, only one (or at most two) scan lines
   // have gone by. Now count the number of display enables until vertical
   // retrace. This number should be about 481.
   _disable ();
   nLines = 1;
   while ((temp & 0x08) == 0)
   {
      // Wait while the display is active
      while ((temp & 0x01) == 0)
      {
#ifdef __MSVC16__
         // Does not work when linked to simulation library
         _asm {
            mov   dx,INPUT_CSTATUS_1
            in    al,dx
            mov   [temp],al
         }
#else
         temp = IOByteRead (INPUT_CSTATUS_1);
#endif
      }
      // Wait while the display is inactive
      while (((temp & 0x01) == 0x01) && ((temp & 0x08) == 0x00))
      {
#ifdef __MSVC16__
         // Does not work when linked to simulation library
         _asm {
            mov   dx,INPUT_CSTATUS_1
            in    al,dx
            mov   [temp],al
         }
#else
         temp = IOByteRead (INPUT_CSTATUS_1);
#endif
      }
      nLines++;
   }
   _enable ();

   if ((nLines < 479) || (nLines > 482))
   {
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      goto SyncStatusTest_exit;
   }

   // Now time vertical sync. There should be approximately 60 of them
   // per second (or 300 vertical syncs during a five second period).
   // Wait for vertical sync before starting.
   nFrames = 0;
   WaitVerticalRetrace ();
   time0 = GetSystemTicks ();
   time1 = time0 + FIVE_SECONDS;
   while (time0 < time1)
   {
      WaitVerticalRetrace ();
      nFrames++;
      time0 = GetSystemTicks ();
   }

   if ((nFrames < 290) || (nFrames > 310))
   {
      nErr = FlagError (ERROR_UNEXPECTED, REF_PART, REF_TEST, 0, 0, 0, 0);
      goto SyncStatusTest_exit;
   }

SyncStatusTest_exit:
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    8
//
//    T0708
//    VSyncOrVDispTest - Visually inspect the sync pulse widened from vertical display end to display start.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int VSyncOrVDispTest (void)
{
   int      nErr;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto VSyncOrVDispTest_exit;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x12);
   SimSetState (TRUE, TRUE, TRUE);

   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x31);
   IOByteWrite (ATC_INDEX, 0x01);         // Blue overscan

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
      {
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
         goto VSyncOrVDispTest_exit;
      }
   }

   IOByteWrite (FEAT_CWCONTROL, 0x08);

   // Changing sync characteristics may effect a frame capture, so give the lib a chance to handle it
   SimModeDirty ();

   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }
   IOByteWrite (FEAT_CWCONTROL, 0x00);

VSyncOrVDispTest_exit:
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
