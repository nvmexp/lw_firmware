//
//    COREC2.CPP - VGA Core "C" Library File #3 (Error Handling)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       23 Februay 1994
//    Last modified: 16 November 2005
//
//    Routines in this file:
//    FlagError            Set global error variables and return DOS ERRORLEVEL value
//    DisplayError         Display error statistics to the STDOUT device
//
//
#include <stdio.h>
#include <string.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

char  szT0201[] = "Motherboard Setup";
char  szT0202[] = "Adapter Setup";
char  szT0203[] = "Limited VGA Setup";
char  szT0301[] = "Cursor Type";
char  szT0302[] = "Cursor Location";
char  szT0303[] = "Cursor Disable";
char  szT0304[] = "Sync Pulse Timing";
char  szT0305[] = "Text Mode Skew";
char  szT0306[] = "Vertical Times Two";
char  szT0307[] = "Dot Clock/2";
char  szT0308[] = "VLoad/2 and VLoad/4";
char  szT0309[] = "8-Dot vs. 9-Dot CRTC Timing";
char  szT0310[] = "Write Protect";
char  szT0311[] = "Double Scan";
char  szT0312[] = "Vertical Interrupt";
char  szT0313[] = "Panning";
char  szT0314[] = "Underline Location";
char  szT0315[] = "Sync Disable";
char  szT0316[] = "Line Compare";
char  szT0317[] = "256 Color Panning";
char  szT0318[] = "Count By 2, Count By 4";
char  szT0319[] = "Sync Skew/Display Skew";
char  szT0320[] = "Preset Row Scan";
char  szT0321[] = "Cursor Skew";
char  szT0322[] = "Non-BIOS Panning and Split Screen";
char  szT0401[] = "Sync and Async Reset";
char  szT0402[] = "Screen Off/CPU Maximum Bandwidth";
char  szT0403[] = "Write Map Mask/Read Plane Select";
char  szT0404[] = "Standard 256 Character Set";
char  szT0405[] = "Extended 512 Character Set";
char  szT0406[] = "Eight Loaded Fonts";
char  szT0407[] = "64K Text Mode";
char  szT0408[] = "9-Dot Line Characters";
char  szT0409[] = "Large Character";
char  szT0410[] = "Font Stress";
char  szT0501[] = "Set/Reset";
char  szT0502[] = "Color Compare";
char  szT0503[] = "ROPs and Data Rotation";
char  szT0504[] = "Write Mode 1";
char  szT0505[] = "Write Mode 2";
char  szT0506[] = "Write Mode 3";
char  szT0507[] = "Video Segment";
char  szT0508[] = "Bit Mask";
char  szT0509[] = "Non-Planar Read Mode 1";
char  szT0510[] = "Non-Planar Write Mode 1";
char  szT0511[] = "Non-Planar Write Mode 2";
char  szT0512[] = "Non-Planar Write Mode 3";
char  szT0513[] = "Shift Register Mode";
char  szT0601[] = "Palette Address Source";
char  szT0602[] = "Blink vs. Intensity";
char  szT0603[] = "Video Status";
char  szT0604[] = "Internal Palette";
char  szT0605[] = "Overscan";
char  szT0606[] = "V54 Select";
char  szT0607[] = "V67 Select";
char  szT0608[] = "Pixel Width";
char  szT0609[] = "Color Plane Enable";
char  szT0610[] = "Graphics Mode Blink";
char  szT0701[] = "Sync Polarity";
char  szT0702[] = "Page Select";
char  szT0703[] = "Clock Selects";
char  szT0704[] = "RAM Enable";
char  szT0705[] = "3B4h vs. 3D4h CRTC I/O Address";
char  szT0706[] = "Switch Readback";
char  szT0707[] = "Sync Status";
char  szT0708[] = "VSYNC or VDISPE Mode";
char  szT0801[] = "DAC Mask";
char  szT0802[] = "DAC Read/Write Status";
char  szT0803[] = "DAC Auto Increment";
char  szT0804[] = "DAC Read/Modify/Write (Sparkle)";
char  szT0805[] = "DAC Stress";
char  szT0901[] = "I/O Read/Write";
char  szT0902[] = "Byte/Word/Dword Mode";
char  szT0903[] = "Chain/2 / Chain/4";
char  szT0904[] = "CGA/Herlwles Addressing";
char  szT0905[] = "Latch";
char  szT0906[] = "Mode 0/1";
char  szT0907[] = "Mode 2/3";
char  szT0908[] = "Mode 4/5";
char  szT0909[] = "Mode 6";
char  szT0910[] = "Mode 7";
char  szT0911[] = "Mode D";
char  szT0912[] = "Mode E/10/11/12";
char  szT0913[] = "Mode F";
char  szT0914[] = "Mode 13";
char  szT0915[] = "Hi-Res Monochrome #1";
char  szT0916[] = "Hi-Res Monochrome #2";
char  szT0917[] = "Hi-Res Color";
char  szT0918[] = "Mode X";
char  szT0919[] = "BYTE, WORD, DWORD Memory Access";
char  szT0920[] = "Pseudo-Random Access Stress";
char  szT1001[] = "Undolwmented Fake Lightpen";
char  szT1002[] = "Undolwmented Latch Readback";
char  szT1003[] = "Undolwmented ATC Toggle State";
char  szT1004[] = "Undolwmented ATC Index";
char  szT1101[] = "LW50 I/O Read/Write Extended Registers";
char  szT1102[] = "LW50 Sync Pulse Timing";
char  szT1103[] = "LW50 Text Mode Skew";
char  szT1104[] = "LW50 Vertical Times Two";
char  szT1105[] = "LW50 Sync Skew/Display Skew";
char  szT1106[] = "LW50 Cursor Blink Test";
char  szT1107[] = "LW50 Graphics Blink Test";
char  szT1108[] = "LW50 Method Generation Test";
char  szT1109[] = "LW50 Blank Stress Test";
char  szT1110[] = "LW50 Line Compare Test";
char  szT1111[] = "LW50 Start Address Latch Test";
char  szT1112[] = "LW50 Input Status 1 Test";
char  szT1113[] = "LW50 Double Buffer Test";
char  szT1201[] = "LW50 Mode 3 Performance Test";
char  szT1202[] = "LW50 Mode 12 Performance Test";
char  szT1203[] = "LW50 Mode 13 Performance Test";
char  szT1204[] = "LW50 Mode 101 Performance Test";

// [part][test]
char *szTestNames[11][22] = {
   {  // Part 2
      szT0201, szT0202, szT0203
   },
   {  // Part 3
      szT0301, szT0302, szT0303, szT0304, szT0305, szT0306, szT0307, szT0308,
      szT0309, szT0310, szT0311, szT0312, szT0313, szT0314, szT0315, szT0316,
      szT0317, szT0318, szT0319, szT0320, szT0321, szT0322
   },
   {  // Part 4
      szT0401, szT0402, szT0403, szT0404, szT0405, szT0406, szT0407, szT0408,
      szT0409, szT0410
   },
   {  // Part 5
      szT0501, szT0502, szT0503, szT0504, szT0505, szT0506, szT0507, szT0508,
      szT0509, szT0510, szT0511, szT0512, szT0513
   },
   {  // Part 6
      szT0601, szT0602, szT0603, szT0604, szT0605, szT0606, szT0607, szT0608,
      szT0609, szT0610
   },
   {  // Part 7
      szT0701, szT0702, szT0703, szT0704, szT0705, szT0706, szT0707, szT0708
   },
   {  // Part 8
      szT0801, szT0802, szT0803, szT0804, szT0805
   },
   {  // Part 9
      szT0901, szT0902, szT0903, szT0904, szT0905, szT0906, szT0907, szT0908,
      szT0909, szT0910, szT0911, szT0912, szT0913, szT0914, szT0915, szT0916,
      szT0917, szT0918, szT0919, szT0920
   },
   {  // Part 10
      szT1001, szT1002, szT1003, szT1004
   },
   {  // Part 11
      szT1101, szT1102, szT1103, szT1104, szT1105, szT1106, szT1107, szT1108,
      szT1109, szT1110, szT1111, szT1112, szT1113
   },
   {  // Part 12
      szT1201, szT1202, szT1203, szT1204
   }
};

//
//    FlagError - Set global error variables and return DOS ERRORLEVEL value
//
//    Entry:   err         Error code (required)
//          part     Chapter of VGA Core Test Specification (required)
//          test     Test # of VGA Core Test Specification (required)
//          address     Address of failure (if appropriate)
//          index    Index of I/O register (if appropriate)
//          expected Expected data (if appropriate)
//          actual      Actual data (if appropriate)
//    Exit: <int>    DOS ERRORLEVEL code
//
int FlagError (int err, int part, int test, DWORD address, WORD index, DWORD expected, DWORD actual)
{
   _vcerr_code = err;
   _vcerr_part = part;
   _vcerr_test = test;
   _vcerr_address = address;
   _vcerr_index = index;
   _vcerr_expected = expected;
   _vcerr_actual = actual;
   return (err);
}

//
//    DisplayError - Display error statistics to the STDOUT device
//
//    Entry:   None
//    Exit: None
//
void DisplayError (void)
{
   if ((_vcerr_part < 2) || (_vcerr_test < 1))
   {
      printf ("\nUnknown Test (Part %d Test %d):"
               "\n  Failed due to unknown error (error code = %d).\n",
               _vcerr_part, _vcerr_test, _vcerr_code);
      return;
   }

   switch (_vcerr_code)
   {
      case ERROR_NONE:              // No error oclwrred

         printf ("\n%s (Part %d Test %d) completed.\n",
               szTestNames[_vcerr_part - 2][_vcerr_test - 1],
               _vcerr_part, _vcerr_test);
         break;

      case ERROR_USERABORT:         // User aborted test

         printf ("\n%s (Part %d Test %d):"
                  "\n  Not completed due to user intervention.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test);
         break;

      case ERROR_UNEXPECTED:        // Unexpected behavior

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to unexpected behavior.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test);
         break;

      case ERROR_IOFAILURE:         // I/O Failure

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to unexpected I/O value at port %04Xh (Index %02Xh)."
                  "\n  Expected data = %08Xh, Actual data = %08Xh.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test,
                  LOWORD (_vcerr_address), _vcerr_index, _vcerr_expected, _vcerr_actual);
         break;

      case ERROR_CAPTURE:           // Capture buffer didn't match

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to capture buffer mismatch.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test);
         break;

      case ERROR_ILWALIDFRAMES:     // Wrong or no frame rate

         {
            DWORD dwExp, dwAct;

            // Build frame rate values
            dwExp = _vcerr_address + (((DWORD) _vcerr_index) << 16);
            dwAct = _vcerr_expected + (((DWORD) _vcerr_actual) << 16);
            printf ("\n%s (Part %d Test %d):"
                     "\n  Failed due to unexpected frame rate (Expected = %d.%03d, Actual = %d.%03d).\n",
                     szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                     _vcerr_part, _vcerr_test,
                     (WORD) (dwExp / 1000), (WORD) (dwExp % 1000),
                     (WORD) (dwAct / 1000), (WORD) (dwAct % 1000));
         }
         break;

      case ERROR_INTERNAL:          // Test couldn't complete (malloc failure, etc.)

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to a system error and the routine could not complete this test.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test);
         break;

      case ERROR_MEMORY:            // Video memory failure

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to unexpected memory value at %04X:%04Xh"
                  "\n  Expected data = %08Xh, Actual data = %08Xh.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test,
                  _vcerr_index, LOWORD (_vcerr_address), _vcerr_expected, _vcerr_actual);
         break;

      case ERROR_VIDEO:             // Video data failure

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to unexpected video data value."
                  "\n  Expected data = %02Xh, Actual data = %02Xh.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test, LOBYTE (LOWORD (_vcerr_expected)),
                  LOBYTE (LOWORD (_vcerr_actual)));
         break;

      case ERROR_ILWALIDCLOCK:      // Wrong dot clock

         {
            DWORD dwExp, dwAct;

            // Build frame rate values
            dwExp = _vcerr_address + (((DWORD) _vcerr_index) << 16);
            dwAct = _vcerr_expected + (((DWORD) _vcerr_actual) << 16);
            printf ("\n%s (Part %d Test %d):"
                     "\n  Failed due to unexpected dot clock frequency (Expected = %d.%03d, Actual = %d.%03d).\n",
                     szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                     _vcerr_part, _vcerr_test,
                     (WORD) (dwExp / 1000), (WORD) (dwExp % 1000),
                     (WORD) (dwAct / 1000), (WORD) (dwAct % 1000));
         }
         break;

      case ERROR_SIM_INIT:

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to simulation model initialization error.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test);
         break;

      case ERROR_CHECKSUM:

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to unexpected checksum value."
                  "\n  Expected data = %02Xh, Actual data = %02Xh.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test, LOBYTE (LOWORD (_vcerr_expected)),
                  LOBYTE (LOWORD (_vcerr_actual)));
         break;

      case ERROR_FILE:

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to file error (open, read or write error)."
                  "\n  Operating system file error code: %d.\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test, _vcerr_actual);
         break;

      default:

         printf ("\n%s (Part %d Test %d):"
                  "\n  Failed due to unknown error (error code = %d).\n",
                  szTestNames[_vcerr_part - 2][_vcerr_test - 1],
                  _vcerr_part, _vcerr_test, _vcerr_code);
         break;
   }
}

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
