//
//    PART8.CPP - VGA Core Test Suite (Part 8)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       14 December 2004
//    Last modified: 15 November 2005
//
//    Routines in this file:
//    DACMaskTest          Step through various values of the DAC mask, verifying each step through visual inspection.
//    DACReadWriteStatusTest  Write to the DAC write index and the DAC read index and read back the status.
//    DACAutoIncrementTest Three I/O writes should increment to the next index.
//    DACSparkleTest       Fill the screen with a pattern and shift the palette around without waiting for vertical retrace.
//    DACStressTest        Animate the DAC to be timing dependent
//
#include <stdio.h>
#include <stdlib.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
#include "crccheck.h"
#include "core/include/disp_test.h"
namespace LegacyVGA {
extern   BOOL  _bSyncBeforeCapture;
#endif

// Function prototypes of the global tests
int DACMaskTest (void);
int DACReadWriteStatusTest (void);
int DACAutoIncrementTest (void);
int DACSparkleTest (void);
int DACStressTest (void);

// Structures needed by some tests
typedef struct tagFRAMEDATA
{
   WORD  saddr;      // Start address
   BYTE  palidx;     // Palette page
   BYTE  red;     // Center color (red)
   BYTE  green;      // Center color (green)
   BYTE  blue;    // Center color (blue)
} FRAMEDATA;

static int  nArguments = 0;         // For now - LGC

#define  REF_PART    8
#define  REF_TEST    1
//
//    T0801
//    DACMaskTest - Step through various values of the DAC mask, verifying each step through visual inspection.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int DACMaskTest (void)
{
   int         nErr, i;
   SEGOFF      lpVideo;
   WORD     wXRes, wYRes;

   nErr = ERROR_NONE;
   if (!DisplayInspectionMessage ())
      goto DACMaskTest_exit;

   SimSetState (TRUE, TRUE, TRUE);
   SetMode (0x12);
   GetResolution (&wXRes, &wYRes);
   lpVideo = (SEGOFF) 0xA0000000;

   // Fill memory with 0FFh
   SimComment ("Fill memory with 0FFh.");
   for (i = 0; (WORD) i < (WORD) (wXRes/8)*wYRes; i++)
      MemByteWrite (lpVideo++, 0xFF);
   SimDumpMemory ("T0801.VGA");

   // Load the DAC with a known set
   SimComment ("Load the DAC with a known set.");
   FillDAC (0, 0, 0);                  // Clear the DAC
   SetDac (1, 0, 0, 0x3F);             // 1 = Blue
   SetDac (2, 0, 0x3F, 0);             // 2 = Green
   SetDac (4, 0, 0x3F, 0x3F);          // 4 = Cyan
   SetDac (8, 0x3F, 0, 0);             // 8 = Red
   SetDac (0x10, 0x3F, 0, 0x3F);       // 10h = Magenta
   SetDac (0x20, 0x3F, 0x3F, 0);       // 20h = Yellow
   SetDac (0x40, 0x3F, 0x2A, 0);       // 40h = Orange
   SetDac (0x80, 0x2A, 0x2A, 0x2A);    // 80h = Gray
   SetDac (0xFF, 0x3F, 0x3F, 0x3F);    // FFh = Bright White

   // Set attribute controller to send 0FFh to the RAMDAC
   SimComment ("Set attribute controller to send 0FFh to the RAMDAC.");
   IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x0F);
   IOByteWrite (ATC_INDEX, 0x3F);
   IOByteWrite (ATC_INDEX, 0x34);
   IOByteWrite (ATC_INDEX, 0x0C);

   for (i = 0; i < 8; i++)
   {
      IOByteWrite (DAC_MASK, (BYTE) (1 << i));
      if (!FrameCapture (REF_PART, REF_TEST))
      {
         if (GetKey () == KEY_ESCAPE)
         {
            nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
            goto DACMaskTest_exit;
         }
      }
   }

   // Verify last location
   SimComment ("Verify last location.");
   IOByteWrite (DAC_MASK, 0xFF);
   if (!FrameCapture (REF_PART, REF_TEST))
   {
      if (GetKey () == KEY_ESCAPE)
         nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);
   }

DACMaskTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("DAC Mask Test completed.");
   else
      SimComment ("DAC Mask Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    2
//
//    T0802
//    DACReadWriteStatusTest - Write to the DAC write index and the DAC read index and read back the status.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int DACReadWriteStatusTest (void)
{
   int      nErr;
   BYTE  temp;
   BOOL  bSim;

   nErr = ERROR_NONE;
   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

   SimSetState (TRUE, TRUE, FALSE);          // Ignore DAC writes
   SetMode (0x03);
   SimSetState (TRUE, TRUE, TRUE);

   // Verify write status returns a "0"
   SimComment ("Verify write status returns a \"0\".");
   if (!bSim) _disable ();
   IOByteWrite (DAC_WINDEX, 0x01);
   temp = IOByteRead (DAC_RINDEX);
   if (!bSim) _enable ();
   if (temp != 0)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_RINDEX, 0, 0, temp);
      goto DACReadWriteStatusTest_exit;
   }

   SimDumpMemory ("T0802.VGA");
   SimSetState (TRUE, TRUE, TRUE);

   // Verify read status returns a "3"
   SimComment ("Verify read status returns a \"3\".");
   if (!bSim) _disable ();
   IOByteWrite (DAC_RINDEX, 0x01);
   temp = IOByteRead (DAC_RINDEX);
   if (!bSim) _enable ();
   if (temp != 3)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_RINDEX, 0, 3, temp);
      goto DACReadWriteStatusTest_exit;
   }

   // Verify status is unchanged by write to mask
   SimComment ("Verify status is unchanged by write to mask.");
   if (!bSim) _disable ();
   IOByteWrite (DAC_MASK, 0xFF);
   temp = IOByteRead (DAC_RINDEX);
   if (temp != 3)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_RINDEX, 0, 3, temp);
      goto DACReadWriteStatusTest_exit;
   }
   if (!bSim) _enable ();

   // Verify status is unchanged by write to data
   SimComment ("Verify status is unchanged by write to data.");
   if (!bSim) _disable ();
   IOByteWrite (DAC_DATA, 0x3F);
   temp = IOByteRead (DAC_RINDEX);
   if (temp != 3)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_RINDEX, 0, 3, temp);
      goto DACReadWriteStatusTest_exit;
   }
   if (!bSim) _enable ();

DACReadWriteStatusTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("DAC Read/Write Status Test passed.");
   else
      SimComment ("DAC Read/Write Status Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    3
//
//    T0803
//    DACAutoIncrementTest - Three I/O writes should increment to the next index.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int DACAutoIncrementTest (void)
{
   int      nErr;
   BYTE  temp, red, green, blue;
   BOOL  bSim;

   nErr = ERROR_NONE;
   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

   SimSetState (TRUE, TRUE, TRUE);
   SetMode (0x03);

   // Write index and read it back
   SimComment ("Write index and read it back.");
   if (!bSim) _disable ();
   IOByteWrite (DAC_WINDEX, 0x08);
   temp = IOByteRead (DAC_WINDEX);
   if (!bSim) _enable ();
   if (temp != 8)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_WINDEX, 0, 8, temp);
      goto DACAutoIncrementTest_exit;
   }

   SimDumpMemory ("T0803.VGA");

   // Write data and read back the index again
   SimComment ("Write data and read back the index again.");
   if (!bSim) _disable ();
   IOByteWrite (DAC_DATA, 0x15);
   IOByteWrite (DAC_DATA, 0x2A);
   IOByteWrite (DAC_DATA, 0x3F);
   temp = IOByteRead (DAC_WINDEX);
   if (!bSim) _enable ();
   if (temp != 9)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_WINDEX, 0, 9, temp);
      goto DACAutoIncrementTest_exit;
   }

   // Write "read index" and read it back (should be one past written value)
   SimComment ("Write read index and read it back (should be one past written value).");
   if (!bSim) _disable ();
   IOByteWrite (DAC_RINDEX, 0x08);
   temp = IOByteRead (DAC_WINDEX);
   if (!bSim) _enable ();
   if (temp != 9)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_WINDEX, 0, 9, temp);
      goto DACAutoIncrementTest_exit;
   }

   // Read back and verify the previously written data and index
   SimComment ("Read back and verify the previously written data and index.");
   if (!bSim) _disable ();
   red = IOByteRead (DAC_DATA);
   green = IOByteRead (DAC_DATA);
   blue = IOByteRead (DAC_DATA);
   temp = IOByteRead (DAC_WINDEX);
   if (!bSim) _enable ();
   if (temp != 0x0A)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_WINDEX, 0, 0x0A, temp);
      goto DACAutoIncrementTest_exit;
   }
   if (red != 0x15)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_DATA, 0, 0x15, red);
      goto DACAutoIncrementTest_exit;
   }
   if (green != 0x2A)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_DATA, 1, 0x2A, green);
      goto DACAutoIncrementTest_exit;
   }
   if (blue != 0x3F)
   {
      nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST, DAC_DATA, 2, 0x3F, blue);
      goto DACAutoIncrementTest_exit;
   }

DACAutoIncrementTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("DAC Auto-Increment Test passed.");
   else
      SimComment ("DAC Auto-Increment Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    4
//
//    T0804
//    DACSparkleTest - Fill the screen with a pattern and shift the palette around without waiting for vertical retrace.
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int DACSparkleTest (void)
{
   static BYTE dac[256][3] = {
      // Vary blue by +2
      {0x0, 0x0, 0x0},     // Index 0
      {0x0, 0x0, 0x2},     // Index 1
      {0x0, 0x0, 0x4},     // Index 2
      {0x0, 0x0, 0x6},     // Index 3
      {0x0, 0x0, 0x8},     // Index 4
      {0x0, 0x0, 0x0A},    // Index 5
      {0x0, 0x0, 0x0C},    // Index 6
      {0x0, 0x0, 0x0E},    // Index 7
      {0x0, 0x0, 0x10},    // Index 8
      {0x0, 0x0, 0x12},    // Index 9
      {0x0, 0x0, 0x14},    // Index 10
      {0x0, 0x0, 0x16},    // Index 11
      {0x0, 0x0, 0x18},    // Index 12
      {0x0, 0x0, 0x1A},    // Index 13
      {0x0, 0x0, 0x1C},    // Index 14
      {0x0, 0x0, 0x1E},    // Index 15
      {0x0, 0x0, 0x20},    // Index 16
      {0x0, 0x0, 0x22},    // Index 17
      {0x0, 0x0, 0x24},    // Index 18
      {0x0, 0x0, 0x26},    // Index 19
      {0x0, 0x0, 0x28},    // Index 20
      {0x0, 0x0, 0x2A},    // Index 21
      {0x0, 0x0, 0x2C},    // Index 22
      {0x0, 0x0, 0x2E},    // Index 23
      {0x0, 0x0, 0x30},    // Index 24
      {0x0, 0x0, 0x32},    // Index 25
      {0x0, 0x0, 0x34},    // Index 26
      {0x0, 0x0, 0x36},    // Index 27
      {0x0, 0x0, 0x38},    // Index 28
      {0x0, 0x0, 0x3A},    // Index 29
      {0x0, 0x0, 0x3C},    // Index 30
      {0x0, 0x0, 0x3F},    // Index 31
      // Vary green by +2
      {0x0, 0x02, 0x03F},     // Index 32
      {0x0, 0x04, 0x03F},     // Index 33
      {0x0, 0x06, 0x03F},     // Index 34
      {0x0, 0x08, 0x03F},     // Index 35
      {0x0, 0x0A, 0x03F},     // Index 36
      {0x0, 0x0C, 0x03F},     // Index 37
      {0x0, 0x0E, 0x03F},     // Index 38
      {0x0, 0x10, 0x03F},     // Index 39
      {0x0, 0x12, 0x03F},     // Index 40
      {0x0, 0x14, 0x03F},     // Index 41
      {0x0, 0x16, 0x03F},     // Index 42
      {0x0, 0x18, 0x03F},     // Index 43
      {0x0, 0x1A, 0x03F},     // Index 44
      {0x0, 0x1C, 0x03F},     // Index 45
      {0x0, 0x1E, 0x03F},     // Index 46
      {0x0, 0x20, 0x03F},     // Index 47
      {0x0, 0x22, 0x03F},     // Index 48
      {0x0, 0x24, 0x03F},     // Index 49
      {0x0, 0x26, 0x03F},     // Index 50
      {0x0, 0x28, 0x03F},     // Index 51
      {0x0, 0x2A, 0x03F},     // Index 52
      {0x0, 0x2C, 0x03F},     // Index 53
      {0x0, 0x2E, 0x03F},     // Index 54
      {0x0, 0x30, 0x03F},     // Index 55
      {0x0, 0x32, 0x03F},     // Index 56
      {0x0, 0x34, 0x03F},     // Index 57
      {0x0, 0x36, 0x03F},     // Index 58
      {0x0, 0x38, 0x03F},     // Index 59
      {0x0, 0x3A, 0x03F},     // Index 60
      {0x0, 0x3C, 0x03F},     // Index 61
      {0x0, 0x3F, 0x03F},     // Index 62
      // Vary blue by -2
      {0x0, 0x3F, 0x3C},      // Index 63
      {0x0, 0x3F, 0x3A},      // Index 64
      {0x0, 0x3F, 0x38},      // Index 65
      {0x0, 0x3F, 0x36},      // Index 66
      {0x0, 0x3F, 0x34},      // Index 67
      {0x0, 0x3F, 0x32},      // Index 68
      {0x0, 0x3F, 0x30},      // Index 69
      {0x0, 0x3F, 0x2E},      // Index 70
      {0x0, 0x3F, 0x2C},      // Index 71
      {0x0, 0x3F, 0x2A},      // Index 72
      {0x0, 0x3F, 0x28},      // Index 73
      {0x0, 0x3F, 0x26},      // Index 74
      {0x0, 0x3F, 0x24},      // Index 75
      {0x0, 0x3F, 0x22},      // Index 76
      {0x0, 0x3F, 0x20},      // Index 77
      {0x0, 0x3F, 0x1E},      // Index 78
      {0x0, 0x3F, 0x1C},      // Index 79
      {0x0, 0x3F, 0x1A},      // Index 80
      {0x0, 0x3F, 0x18},      // Index 81
      {0x0, 0x3F, 0x16},      // Index 82
      {0x0, 0x3F, 0x14},      // Index 83
      {0x0, 0x3F, 0x12},      // Index 84
      {0x0, 0x3F, 0x10},      // Index 85
      {0x0, 0x3F, 0x0E},      // Index 86
      {0x0, 0x3F, 0x0C},      // Index 87
      {0x0, 0x3F, 0x0A},      // Index 88
      {0x0, 0x3F, 0x08},      // Index 89
      {0x0, 0x3F, 0x06},      // Index 90
      {0x0, 0x3F, 0x04},      // Index 91
      {0x0, 0x3F, 0x02},      // Index 92
      {0x0, 0x3F, 0x00},      // Index 93
      // Vary red by +2
      {0x02, 0x03F, 0x00}, // Index 94
      {0x04, 0x03F, 0x00}, // Index 95
      {0x06, 0x03F, 0x00}, // Index 96
      {0x08, 0x03F, 0x00}, // Index 97
      {0x0A, 0x03F, 0x00}, // Index 98
      {0x0C, 0x03F, 0x00}, // Index 99
      {0x0E, 0x03F, 0x00}, // Index 100
      {0x10, 0x03F, 0x00}, // Index 101
      {0x12, 0x03F, 0x00}, // Index 102
      {0x14, 0x03F, 0x00}, // Index 103
      {0x16, 0x03F, 0x00}, // Index 104
      {0x18, 0x03F, 0x00}, // Index 105
      {0x1A, 0x03F, 0x00}, // Index 106
      {0x1C, 0x03F, 0x00}, // Index 107
      {0x1E, 0x03F, 0x00}, // Index 108
      {0x20, 0x03F, 0x00}, // Index 109
      {0x22, 0x03F, 0x00}, // Index 110
      {0x24, 0x03F, 0x00}, // Index 111
      {0x26, 0x03F, 0x00}, // Index 112
      {0x28, 0x03F, 0x00}, // Index 113
      {0x2A, 0x03F, 0x00}, // Index 114
      {0x2C, 0x03F, 0x00}, // Index 115
      {0x2E, 0x03F, 0x00}, // Index 116
      {0x30, 0x03F, 0x00}, // Index 117
      {0x32, 0x03F, 0x00}, // Index 118
      {0x34, 0x03F, 0x00}, // Index 119
      {0x36, 0x03F, 0x00}, // Index 120
      {0x38, 0x03F, 0x00}, // Index 121
      {0x3A, 0x03F, 0x00}, // Index 122
      {0x3C, 0x03F, 0x00}, // Index 123
      {0x3F, 0x03F, 0x00}, // Index 124
      // Vary green by -2
      {0x3F, 0x03C, 0x00}, // Index 125
      {0x3F, 0x03A, 0x00}, // Index 126
      {0x3F, 0x038, 0x00}, // Index 127
      {0x3F, 0x036, 0x00}, // Index 128
      {0x3F, 0x034, 0x00}, // Index 129
      {0x3F, 0x032, 0x00}, // Index 130
      {0x3F, 0x030, 0x00}, // Index 131
      {0x3F, 0x02E, 0x00}, // Index 132
      {0x3F, 0x02C, 0x00}, // Index 133
      {0x3F, 0x02A, 0x00}, // Index 134
      {0x3F, 0x028, 0x00}, // Index 135
      {0x3F, 0x026, 0x00}, // Index 136
      {0x3F, 0x024, 0x00}, // Index 137
      {0x3F, 0x022, 0x00}, // Index 138
      {0x3F, 0x020, 0x00}, // Index 139
      {0x3F, 0x01E, 0x00}, // Index 140
      {0x3F, 0x01C, 0x00}, // Index 141
      {0x3F, 0x01A, 0x00}, // Index 142
      {0x3F, 0x018, 0x00}, // Index 143
      {0x3F, 0x016, 0x00}, // Index 144
      {0x3F, 0x014, 0x00}, // Index 145
      {0x3F, 0x012, 0x00}, // Index 146
      {0x3F, 0x010, 0x00}, // Index 147
      {0x3F, 0x00E, 0x00}, // Index 148
      {0x3F, 0x00C, 0x00}, // Index 149
      {0x3F, 0x00A, 0x00}, // Index 150
      {0x3F, 0x008, 0x00}, // Index 151
      {0x3F, 0x006, 0x00}, // Index 152
      {0x3F, 0x004, 0x00}, // Index 153
      {0x3F, 0x002, 0x00}, // Index 154
      {0x3F, 0x000, 0x00}, // Index 155
      // Vary blue by +2
      {0x3F, 0x000, 0x02}, // Index 156
      {0x3F, 0x000, 0x04}, // Index 157
      {0x3F, 0x000, 0x06}, // Index 158
      {0x3F, 0x000, 0x08}, // Index 159
      {0x3F, 0x000, 0x0A}, // Index 160
      {0x3F, 0x000, 0x0C}, // Index 161
      {0x3F, 0x000, 0x0E}, // Index 162
      {0x3F, 0x000, 0x10}, // Index 163
      {0x3F, 0x000, 0x12}, // Index 164
      {0x3F, 0x000, 0x14}, // Index 165
      {0x3F, 0x000, 0x16}, // Index 166
      {0x3F, 0x000, 0x18}, // Index 167
      {0x3F, 0x000, 0x1A}, // Index 168
      {0x3F, 0x000, 0x1C}, // Index 169
      {0x3F, 0x000, 0x1E}, // Index 170
      {0x3F, 0x000, 0x20}, // Index 171
      {0x3F, 0x000, 0x22}, // Index 172
      {0x3F, 0x000, 0x24}, // Index 173
      {0x3F, 0x000, 0x26}, // Index 174
      {0x3F, 0x000, 0x28}, // Index 175
      {0x3F, 0x000, 0x2A}, // Index 176
      {0x3F, 0x000, 0x2C}, // Index 177
      {0x3F, 0x000, 0x2E}, // Index 178
      {0x3F, 0x000, 0x30}, // Index 179
      {0x3F, 0x000, 0x32}, // Index 180
      {0x3F, 0x000, 0x34}, // Index 181
      {0x3F, 0x000, 0x36}, // Index 182
      {0x3F, 0x000, 0x38}, // Index 183
      {0x3F, 0x000, 0x3A}, // Index 184
      {0x3F, 0x000, 0x3C}, // Index 185
      {0x3F, 0x000, 0x3F}, // Index 186
      // Vary green by +2
      {0x3F, 0x002, 0x3F}, // Index 187
      {0x3F, 0x004, 0x3F}, // Index 188
      {0x3F, 0x006, 0x3F}, // Index 189
      {0x3F, 0x008, 0x3F}, // Index 190
      {0x3F, 0x00A, 0x3F}, // Index 191
      {0x3F, 0x00C, 0x3F}, // Index 192
      {0x3F, 0x00E, 0x3F}, // Index 193
      {0x3F, 0x010, 0x3F}, // Index 194
      {0x3F, 0x012, 0x3F}, // Index 195
      {0x3F, 0x014, 0x3F}, // Index 196
      {0x3F, 0x016, 0x3F}, // Index 197
      {0x3F, 0x018, 0x3F}, // Index 198
      {0x3F, 0x01A, 0x3F}, // Index 199
      {0x3F, 0x01C, 0x3F}, // Index 200
      {0x3F, 0x01E, 0x3F}, // Index 201
      {0x3F, 0x020, 0x3F}, // Index 202
      {0x3F, 0x022, 0x3F}, // Index 203
      {0x3F, 0x024, 0x3F}, // Index 204
      {0x3F, 0x026, 0x3F}, // Index 205
      {0x3F, 0x028, 0x3F}, // Index 206
      {0x3F, 0x02A, 0x3F}, // Index 207
      {0x3F, 0x02C, 0x3F}, // Index 208
      {0x3F, 0x02E, 0x3F}, // Index 209
      {0x3F, 0x030, 0x3F}, // Index 210
      {0x3F, 0x032, 0x3F}, // Index 211
      {0x3F, 0x034, 0x3F}, // Index 212
      {0x3F, 0x036, 0x3F}, // Index 213
      {0x3F, 0x038, 0x3F}, // Index 214
      {0x3F, 0x03A, 0x3F}, // Index 215
      {0x3F, 0x03C, 0x3F}, // Index 216
      {0x3F, 0x03E, 0x3F}, // Index 217
      // Vary (approximately) red by -2, green by -2, blue by -2
      {0x3F, 0x03F, 0x3F}, // Index 218
      {0x3E, 0x03E, 0x3E}, // Index 219
      {0x3D, 0x03D, 0x3D}, // Index 220
      {0x3C, 0x03C, 0x3C}, // Index 221
      {0x3B, 0x03B, 0x3B}, // Index 222
      {0x3A, 0x03A, 0x3A}, // Index 223
      {0x39, 0x039, 0x39}, // Index 224
      {0x38, 0x038, 0x38}, // Index 225
      {0x37, 0x037, 0x37}, // Index 226
      {0x36, 0x036, 0x36}, // Index 227
      {0x34, 0x034, 0x34}, // Index 228
      {0x32, 0x032, 0x32}, // Index 229
      {0x30, 0x030, 0x30}, // Index 230
      {0x2E, 0x02E, 0x2E}, // Index 231
      {0x2C, 0x02C, 0x2C}, // Index 232
      {0x2A, 0x02A, 0x2A}, // Index 233
      {0x28, 0x028, 0x28}, // Index 234
      {0x26, 0x026, 0x26}, // Index 235
      {0x24, 0x024, 0x24}, // Index 236
      {0x22, 0x022, 0x22}, // Index 237
      {0x20, 0x020, 0x20}, // Index 238
      {0x1E, 0x01E, 0x1E}, // Index 239
      {0x1C, 0x01C, 0x1C}, // Index 240
      {0x1A, 0x01A, 0x1A}, // Index 241
      {0x18, 0x018, 0x18}, // Index 241
      {0x16, 0x016, 0x16}, // Index 243
      {0x14, 0x014, 0x14}, // Index 244
      {0x12, 0x012, 0x12}, // Index 245
      {0x10, 0x010, 0x10}, // Index 246
      {0x0E, 0x00E, 0x0E}, // Index 247
      {0x0C, 0x00C, 0x0C}, // Index 248
      {0x0A, 0x00A, 0x0A}, // Index 249
      {0x08, 0x008, 0x08}, // Index 250
      {0x06, 0x006, 0x06}, // Index 251
      {0x04, 0x004, 0x04}, // Index 252
      {0x02, 0x002, 0x02}, // Index 253
      {0x01, 0x001, 0x01}, // Index 254
      {0x00, 0x000, 0x00}  // Index 255
   };
   static BYTE dacShadow[256][3];
   int         nErr, i, j, nFrames, nSkew;
   SEGOFF      lpVideo;
   BYTE     red, green, blue, ridx, widx, byPixel;
   BOOL     bCapture;
   WORD     wXRes, wYRes;

   nErr = ERROR_NONE;
   nFrames = 8 - 1;
   if (!DisplayInspectionMessage ())
      goto DACSparkleTest_exit;

   SimSetState (TRUE, TRUE, FALSE);       // DAC will be loaded later
   SetMode (0x13);
   GetResolution (&wXRes, &wYRes);
   SimSetState (TRUE, TRUE, TRUE);
   if (SimGetFrameSize ())
      nSkew = 1;        // Full frame can gradually shift pixels
   else
      nSkew = 9;        // Small frame needs to offset pixels to see full DAC range

   lpVideo = (SEGOFF) 0xA0000000;

   SimComment ("Write an interesting pattern into memory.");
   for (i = 0; i < (int) wYRes; i++)
   {
      byPixel = (BYTE) (i * nSkew);
      if (byPixel == 0) byPixel++;           // 1..255 only
      for (j = 0; j < (int) wXRes; j++)
      {
         MemByteWrite (lpVideo++, byPixel++);
         if (byPixel == 0) byPixel++;        // 1..255 only
      }
   }
   SimDumpMemory ("T0804.VGA");

   // Load DAC and initialize DAC shadow
   SimComment ("Load DAC and initialize DAC shadow.");
   SetDacBlock ((LPBYTE) dac, 0, 256);
   for (i = 0; i < 256; i++)
   {
      dacShadow[i][0] = dac[i][0];
      dacShadow[i][1] = dac[i][1];
      dacShadow[i][2] = dac[i][2];
   }

   bCapture = FrameCapture (REF_PART, REF_TEST);

   ridx = 2;
   widx = 1;
   if (bCapture)
   {
      for (i = 0; i < nFrames; i++)
      {
         SimComment ("Rotate DAC values.");
         for (j = 0; j < 256; j++)
         {
            // Get DAC values at this index
            GetDac (ridx, &red, &green, &blue);
            if (dacShadow[ridx][0] != red)
            {
               nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                                 ridx, 0, dacShadow[ridx][0], red);
               goto DACSparkleTest_exit;
            }
            if (dacShadow[ridx][1] != green)
            {
               nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                                 ridx, 1, dacShadow[ridx][1], green);
               goto DACSparkleTest_exit;
            }
            if (dacShadow[ridx][2] != blue)
            {
               nErr = FlagError (ERROR_IOFAILURE, REF_PART, REF_TEST,
                                 ridx, 2, dacShadow[ridx][2], blue);
               goto DACSparkleTest_exit;
            }

            // Set next DAC location to new values and update DAC shadow
            SetDac (widx, red, green, blue);
            dacShadow[widx][0] = red;
            dacShadow[widx][1] = green;
            dacShadow[widx][2] = blue;

            // Next location(s)
            if (++ridx == 0) ridx++;
            if (++widx == 0) widx++;
         }
#ifdef LW_MODS
         _bSyncBeforeCapture = TRUE;
#endif
         FrameCapture (REF_PART, REF_TEST);
      }
   }
   else
   {
      while (!_kbhit())
      {
         GetDac (ridx, &red, &green, &blue);
         SetDac (widx, red, green, blue);
         if (++ridx == 0) ridx++;
         if (++widx == 0) widx++;
      }
   }

   if (GetKey () == KEY_ESCAPE)
      nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);

DACSparkleTest_exit:
   if (nErr == ERROR_NONE)
      SimComment ("DAC Sparkle Test completed.");
   else
      SimComment ("DAC Sparkle Test failed.");
   SystemCleanUp ();
   return (nErr);
}

#undef   REF_TEST
#define  REF_TEST    5
//
//    T0805
//    DACStressTest - Animate the DAC to be timing dependent
//
//    Entry:   None
//    Exit: <int>    DOS ERRORLEVEL value
//
int DACStressTest (void)
{
   int         nErr, i, j, k, nStartLine, nBorders;
   SEGOFF      lpVideo;
   WORD     wXRes, wYRes, wRowOffset, wOffset;
   WORD     wSimType;
   BOOL     bFullVGA;
   static BYTE dac1[16][3] = {
      {0x00, 0x00, 0x00},     // Index 0
      {0x3F, 0x3F, 0x3F},     // Index 1
      {0x2A, 0x2A, 0x2A},     // Index 2
      {0x1F, 0x1F, 0x1F},     // Index 3
      {0x2A, 0x2A, 0x2A},     // Index 4
      {0x3F, 0x3F, 0x3F},     // Index 5
      {0x00, 0x00, 0x00},     // Index 6
      {0x00, 0x00, 0x00},     // Index 7
      {0x2A, 0x00, 0x00},     // Index 8
      {0x3F, 0x1F, 0x00},     // Index 9
      {0x3F, 0x2A, 0x00},     // Index A
      {0x3F, 0x3F, 0x00},     // Index B
      {0x00, 0x00, 0x00},     // Index C
      {0x00, 0x00, 0x00},     // Index D
      {0x00, 0x00, 0x00},     // Index E
      {0x00, 0x00, 0x00}      // Index F
   };
   static BYTE dac2[16][3] = {
      {0x00, 0x00, 0x00},     // Index 0
      {0x3F, 0x3F, 0x3F},     // Index 1
      {0x2A, 0x2A, 0x2A},     // Index 2
      {0x1F, 0x1F, 0x1F},     // Index 3
      {0x2A, 0x2A, 0x2A},     // Index 4
      {0x3F, 0x3F, 0x3F},     // Index 5
      {0x00, 0x00, 0x00},     // Index 6
      {0x00, 0x00, 0x00},     // Index 7
      {0x00, 0x00, 0x00},     // Index 8
      {0x00, 0x00, 0x00},     // Index 9
      {0x00, 0x00, 0x00},     // Index A
      {0x00, 0x00, 0x00},     // Index B
      {0x2A, 0x00, 0x3F},     // Index C
      {0x3F, 0x1F, 0x3F},     // Index D
      {0x3F, 0x2A, 0x3F},     // Index E
      {0x3F, 0x3F, 0x3F}      // Index F
   };
   static BYTE tblBlock1[8][4] = {
      {0x49, 0x24, 0x00, 0xFF},
      {0x92, 0x49, 0x00, 0xFF},
      {0x24, 0x92, 0x00, 0xFF},
      {0x49, 0x24, 0x00, 0xFF},
      {0xFF, 0xFF, 0x00, 0xFF},
      {0xFF, 0xFF, 0x00, 0xFF},
      {0xFF, 0xFF, 0x00, 0xFF},
      {0xFF, 0xFF, 0x00, 0xFF}
   };
   static BYTE tblBlock2[8][4] = {
      {0x49, 0x24, 0xFF, 0xFF},
      {0x92, 0x49, 0xFF, 0xFF},
      {0x24, 0x92, 0xFF, 0xFF},
      {0x49, 0x24, 0xFF, 0xFF},
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0xFF, 0xFF, 0xFF, 0xFF},
      {0xFF, 0xFF, 0xFF, 0xFF}
   };
   static FRAMEDATA  tblFrameData[] = {
      {0x0000, 0, 0x3F, 0x00, 0x00},
      {0x0000, 1, 0x3C, 0x08, 0x00},
      {0x2000, 0, 0x38, 0x0C, 0x00},
      {0x2000, 1, 0x34, 0x10, 0x00},
      {0x4000, 0, 0x30, 0x14, 0x00},
      {0x4000, 1, 0x2C, 0x18, 0x00},
      {0x6000, 0, 0x28, 0x1C, 0x00},
      {0x6000, 1, 0x24, 0x20, 0x00},
      {0x8000, 0, 0x20, 0x24, 0x00},
      {0x8000, 1, 0x1C, 0x28, 0x00},
      {0xA000, 0, 0x18, 0x2C, 0x00},
      {0xA000, 1, 0x14, 0x30, 0x00},
      {0xC000, 0, 0x10, 0x34, 0x00},
      {0xC000, 1, 0x0C, 0x38, 0x00},
      {0xE000, 0, 0x08, 0x3C, 0x00},
      {0xE000, 1, 0x00, 0x3F, 0x00},
      {0xE000, 0, 0x08, 0x3C, 0x00},
      {0xC000, 1, 0x0C, 0x38, 0x00},
      {0xC000, 0, 0x10, 0x34, 0x00},
      {0xA000, 1, 0x14, 0x30, 0x00},
      {0xA000, 0, 0x18, 0x2C, 0x00},
      {0x8000, 1, 0x1C, 0x28, 0x00},
      {0x8000, 0, 0x20, 0x24, 0x00},
      {0x6000, 1, 0x24, 0x20, 0x00},
      {0x6000, 0, 0x28, 0x1C, 0x00},
      {0x4000, 1, 0x2C, 0x18, 0x00},
      {0x4000, 0, 0x30, 0x14, 0x00},
      {0x2000, 1, 0x34, 0x10, 0x00},
      {0x2000, 0, 0x38, 0x0C, 0x00},
      {0x0000, 1, 0x3C, 0x08, 0x00}
   };
   static int cFrameData = sizeof (tblFrameData) / sizeof (FRAMEDATA);

   nErr = ERROR_NONE;
   wSimType = SimGetType ();
   if (!DisplayInspectionMessage ())
      goto DACStressTest_exit;

   bFullVGA = SimGetFrameSize ();
   SimSetState (TRUE, TRUE, FALSE);    // DAC will be loaded later
   SetMode (0x0D);
   SimSetState (TRUE, TRUE, TRUE);
   GetResolution (&wXRes, &wYRes);
   wRowOffset = wXRes / 8;
   lpVideo = (SEGOFF) 0xA0000000;
   SetLine4Columns (wRowOffset);
   if (SimGetFrameSize ())
   {
      nStartLine = 5;
      nBorders = 5;
   }
   else
   {
      nStartLine = 1;
      nBorders = 1;
   }

   //
   // Load memory
   //
   // Draw borders
   SimComment ("Draw borders.");
   for (j = 0; j < 8; j++)
   {
      SetLine4StartAddress (j * 0x2000);
      for (i = 0; i < nBorders; i++)
      {
         // Border 1
         Line4 (i, i, wXRes - (1 + i), i, (BYTE) (i + 1));
         Line4 (wXRes - (1 + i), i, wXRes - (1 + i), wYRes - (1 + i), (BYTE) (i + 1));
         Line4 (wXRes - (1 + i), wYRes - (1 + i), i, wYRes - (1 + i), (BYTE) (i + 1));
         Line4 (i, wYRes - (1 + i), i, i, (BYTE) (i + 1));
      }
   }

   // Fix up things the line routine messed up
   SimComment ("Fix up things the line routine messed up.");
   SetLine4StartAddress (0);
   IOWordWrite (GDC_INDEX, 0xFF08);
   IOWordWrite (GDC_INDEX, 0x0001);
   IOWordWrite (GDC_INDEX, 0x0003);

   // Draw the blocks
   SimComment ("Draw the blocks.");
   for (k = 0; k < 8; k++)
   {
      for (i = 0; i < 4; i++)
      {
         wOffset = (k * 0x2000) + (nStartLine * wRowOffset) + 1 + (k * 2);
         IOByteWrite (SEQ_INDEX, 0x02);
         IOByteWrite (SEQ_DATA, (BYTE) (1 << i));
         for (j = 0; j < 8; j++)
         {
            MemByteWrite (lpVideo + wOffset, tblBlock1[j][i]);
            MemByteWrite (lpVideo + wOffset + 1, tblBlock2[j][i]);
            wOffset += wRowOffset;
         }
      }
   }

   SimDumpMemory ("T0805.VGA");

   // Set the attribute controller
   SimComment ("Set the attribute controller.");
   for (i = 0; i < 16; i++)
   {
      IOByteRead (INPUT_CSTATUS_1);
      IOByteWrite (ATC_INDEX, i);
      IOByteWrite (ATC_INDEX, i);
   }
   IOByteWrite (ATC_INDEX, 0x34);
   IOByteWrite (ATC_INDEX, 0x00);
   IOByteWrite (ATC_INDEX, 0x30);
   IOByteWrite (ATC_INDEX, IOByteRead (ATC_RDATA) | 0x80);
   IOByteWrite (ATC_INDEX, 0x20);

   // Load the RAMDAC
   SimComment ("Load the RAMDAC.");
   SetDacBlock ((LPBYTE) dac1, 0, 16);
   SetDacBlock ((LPBYTE) dac2, 16, 16);

   // For debugging, if anything is on the command line then wait
   // for a keystroke from the user before running
   if (bFullVGA && (nArguments > 1))
      SimGetKey ();

   //
   // Start the animation
   //
   SimComment ("Start the animation.");
   StartCapture (1);
#ifdef LW_MODS
   DispTest::UpdateNow(LegacyVGA::vga_timeout_ms());            // Send an update and wait a few frames
   WaitVerticalRetrace ();                                     // Sync the beginning to the vsync pulse
   WaitVerticalRetrace ();                                     // Sync the beginning to the vsync pulse
   WaitVerticalRetrace ();                                     // Sync the beginning to the vsync pulse
   LegacyVGA::CRCManager()->CaptureFrame(0, 1, 1, LegacyVGA::vga_timeout_ms());
   LegacyVGA::CRCManager()->CaptureFrame(0, 1, 1, LegacyVGA::vga_timeout_ms());
#else
   FrameCapture (REF_PART, REF_TEST);
#endif
   WaitVerticalRetrace ();                                  // Sync the beginning to the vsync pulse
   while (TRUE)
   {
      for (i = 0; i < cFrameData; i++)
      {
         IOByteWrite (CRTC_CINDEX, 0x0C);
         IOByteWrite (CRTC_CDATA, HIBYTE (tblFrameData[i].saddr));
#ifdef LW_MODS
         LegacyVGA::CRCManager()->CaptureFrame(1, 1, 0, LegacyVGA::vga_timeout_ms());
         WaitVerticalRetrace ();
#else
         WaitNotVerticalRetrace ();                            // Wait for start address to be latched before loading immediate values
         FrameCapture (REF_PART, REF_TEST);
#endif
         IOByteRead (INPUT_CSTATUS_1);
         IOByteWrite (ATC_INDEX, 0x34);
         IOByteWrite (ATC_INDEX, tblFrameData[i].palidx);            // Set RAMDAC page
         SetDac (0x0B, tblFrameData[i].red, tblFrameData[i].green, tblFrameData[i].blue);
         SetDac (0x1F, tblFrameData[i].red, tblFrameData[i].green, tblFrameData[i].blue);
      }
      if (wSimType & SIM_SIMULATION)
         break;
      else
      {
         if (_kbhit ())       // Continue until a key is hit
            break;
      }
   }
#ifdef LW_MODS
   LegacyVGA::CRCManager()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
   LegacyVGA::CRCManager()->WaitCapture(1, 0, LegacyVGA::vga_timeout_ms());
#else
   FrameCapture (REF_PART, REF_TEST);
#endif

//DACStressTest_abort:
   if (GetKey () == KEY_ESCAPE)
      nErr = FlagError (ERROR_USERABORT, REF_PART, REF_TEST, 0, 0, 0, 0);

DACStressTest_exit:
   EndCapture ();
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
