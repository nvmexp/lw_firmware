//
//    PART10.CPP - VGA Core Test Suite (Part 10)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 15 November 2005
//
//    Routines in this file:
//    UndocLightpenTest    Verify that CRTC[3].7 sets/clears access to CRTC[10] and CRTC[11]
//    UndocLatchTest       Load various values into memory, load latches, and verify contents.
//    UndocATCToggleTest      Set the ATC toggle to a known state and verify it can be read from an undolwmented register (CRTC[24])
//    UndocATCIndexTest    Set the ATC index to a known state and verify it can be read from an undolwmented register (CRTC[26])
//
#include <stdio.h>
#include <stdlib.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

// Function prototypes of the global tests
int UndocLightpenTest (void);
int UndocLatchTest (void);
int UndocATCToggleTest (void);
int UndocATCIndexTest (void);

#define  REF_PART    10
#define  REF_TEST    1
//
//    T1001
//    UndocLightpenTest - Verify that CRTC[3].7 sets/clears access to CRTC[10] and CRTC[11]
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
//    Note: Later versions of the Model 70 do not support this feature and
//          therefore diminishes the relevance of this test.
//
int UndocLightpenTest (void)
{
   int      nErr;
   BYTE  temp;

   nErr = ERROR_NONE;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x82);

   // Clear write protect
   SimComment ("Clear write protect.");
   IOByteWrite (CRTC_CINDEX, 0x11);
   IOByteWrite (CRTC_CDATA, (BYTE) (IOByteRead (CRTC_CDATA) & 0x7F));

   // Verify read/write-ability (don't mess with interrupt bits)
   SimComment ("Verify read/write-ability (don't mess with interrupt bits).");
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x30)) != 0)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                        CRTC_CDATA, 0x11, 0x00, temp);
      goto UndocLightpenTest_exit;
   }
   IOByteWrite (CRTC_CINDEX, 0x10);
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x00)) != 0)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                        CRTC_CDATA, 0x10, 0x00, temp);
      goto UndocLightpenTest_exit;
   }
   IOByteWrite (CRTC_CINDEX, 0x03);
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x00)) != 0)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                        CRTC_CDATA, 0x03, 0x00, temp);
      goto UndocLightpenTest_exit;
   }

   // Clear CRTC[3].7
   SimComment ("Clear CRTC[3].7.");
   temp = (BYTE) IOByteRead (CRTC_CDATA);
   IOByteWrite (CRTC_CDATA, (BYTE) (temp & 0x7F));

   // Verify read/write-ability is lost
   SimComment ("Verify read/write-ability is lost.");
   IOByteWrite (CRTC_CINDEX, 0x11);
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x30)) != 0xCF)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                        CRTC_CDATA, 0x11, 0xFF, temp);
      goto UndocLightpenTest_exit;
   }
   IOByteWrite (CRTC_CINDEX, 0x10);
   if ((temp = IsIObitFunctional (CRTC_CDATA, CRTC_CDATA, 0x00)) != 0xFF)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                        CRTC_CDATA, 0x10, 0xFF, temp);
      goto UndocLightpenTest_exit;
   }

   // Restore CRTC[3].7
   SimComment ("Restore CRTC[3].7.");
   IOByteWrite (CRTC_CINDEX, 0x03);
   IOByteWrite (CRTC_CDATA, temp);

UndocLightpenTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Undoc Lightpen Test passed.");
   else
      SimComment ("Undoc Lightpen Test failed.");

   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T1002
//    UndocLatchTest - Load various values into memory, load latches, and verify contents.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int UndocLatchTest (void)
{
   int         nErr, i, nLength;
   SEGOFF      lpVideo;
   BYTE     pattern, temp;
   WORD     offset;

   nErr = ERROR_NONE;
   lpVideo = (SEGOFF) (0xA0000000);
   nLength = 64;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x92);                           // Memory will be set later

   // Fill memory with a pattern
   SimComment ("Fill memory with a pattern.");
   pattern = 0x15;
   for (i = 0; i < 4; i++)
   {
      IOByteWrite (SEQ_INDEX, 0x02);
      IOByteWrite (SEQ_DATA, (BYTE) (1 << i));
      MemoryFill (lpVideo, pattern, (WORD) nLength);
      pattern = RotateByteLeft (pattern, 1);
   }
   SimDumpMemory ("T1002.VGA");

   SimComment ("Read memory at each location and verify latches can be read from register.");
   for (offset = 0; offset < (WORD) nLength; offset++)
   {
      temp = MemByteRead (lpVideo + offset);    // Load the latches
      pattern = 0x15;
      for (i = 0; i < 4; i++)
      {
         IOByteWrite (GDC_INDEX, 0x04);
         IOByteWrite (GDC_DATA, (BYTE) i);
         IOByteWrite (CRTC_CINDEX, 0x22);
         temp = (BYTE) IOByteRead (CRTC_CDATA);
         if (temp != pattern)
         {
            nErr = FlagError (ERROR_MEMORY, REF_PART, REF_TEST,
                              HIWORD (lpVideo + offset),
                              LOWORD (lpVideo + offset), pattern, temp);
            goto UndocLatchTest_exit;
         }
         pattern = RotateByteLeft (pattern, 1);
      }
   }

UndocLatchTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Undoc Latch Test passed.");
   else
      SimComment ("Undoc Latch Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T1003
//    UndocATCToggleTest - Set the ATC toggle to a known state and verify it can be read from an undolwmented register (CRTC[24])
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int UndocATCToggleTest (void)
{
   int      nErr;
   BYTE  temp;

   nErr = ERROR_NONE;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x82);

   SimComment ("Set ATC toggle state and verify it can be read from undoc register.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (CRTC_CINDEX, 0x24);
   temp = (BYTE) IOByteRead (CRTC_CDATA);
   if ((temp & 0x80) == 0x80)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                        CRTC_CDATA, 0x24, 0x00, temp & 0x80);
      goto UndocATCToggleTest_exit;
   }
   IOByteWrite (ATC_INDEX, 0x20);
   temp = (BYTE) IOByteRead (CRTC_CDATA);
   if ((temp & 0x80) == 0x00)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                        CRTC_CDATA, 0x24, 0x80, temp & 0x80);
      goto UndocATCToggleTest_exit;
   }

UndocATCToggleTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Undoc ATC Toggle Test passed.");
   else
      SimComment ("Undoc ATC Toggle Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    4
//
//    UndocATCIndexTest - Set the ATC index to a known state and verify it can be read from an undolwmented register (CRTC[26])
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int UndocATCIndexTest (void)
{
   int      i, nErr;
   BYTE  temp;

   nErr = ERROR_NONE;

   SimSetState (TRUE, TRUE, FALSE);       // Ignore DAC writes
   SetMode (0x03);

   SimComment ("Test that the 32 possible values of ATC index register can be read back from the undoc register.");
   IOByteWrite (CRTC_CINDEX, 0x26);
   IOByteRead (ATC_INDEX);
   for (i = 0; i < 32; i++)
   {
      // Reset toggle to index state and write the ATC index
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, i);

      // Attribute controller index should be read back as the low order
      // five bits of CRTC[26].
      temp = IOByteRead (CRTC_CDATA);
      if ((temp & 0x3F) != i)
      {
         nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, CRTC_CDATA, 0x26, i, temp & 0x3F);
         goto UndocATCIndexTest_exit;
      }
   }

   // Reset toggle to index state and test palette address bit. This bit
   // should also be able to be read back from CRTC[26].
   SimComment ("Reset toggle to index state and test palette address bit and verify it can be read back.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x20);
   temp = IOByteRead (CRTC_CDATA);
   if ((temp & 0x3F) != 0x20)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, CRTC_CDATA, 0x26, 0x20, temp & 0x3F);
      goto UndocATCIndexTest_exit;
   }

UndocATCIndexTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("Undoc ATC Index Test passed.");
   else
      SimComment ("Undoc ATC Index Test failed.");
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
