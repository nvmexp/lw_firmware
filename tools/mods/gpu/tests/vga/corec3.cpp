//
//    COREC3.CPP - VGA Core "C" Library File #4 (Mode Set)
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       27 February 1994
//    Last modified: 16 November 2005
//
//    Routines in this file:
//    SetMode                 Set a video mode
//    GetDac                  Get values from a specific DAC location
//    SetDac                  Set a specific DAC location to a given value
//    FillDAC                 Load the entire RAMDAC with a single value
//    FillDACLength           Load a range of RAMDAC registers with a single value
//    SetDacBlock             Set a range of DAC registers
//    SetRegs                 Load a parameter table into the VGA registers
//    GetParmTable            Retrieve the current mode table
//    SetBIOSVars             Set BIOS variables based on mode number
//    ModeLoadFont            Load appropriate font for the mode set
//    ModeLoadDAC             Load appropriate DAC values for the mode set
//    ModeClearMemory            Clear video memory for a given mode
//    SetAll2CPU              Set/clear "all-to-CPU" bit in sequencer
//
//
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

//
//    SetMode - Set a BIOS video mode
//
//    Entry:   wMode    Video mode
//    Exit: None
//
void SetMode (WORD wMode)
{
   LPPARMENTRY lpParms;
   WORD        wMode7;
   static char szTemp[48];

   // Tell the vector file what is going on
   sprintf (szTemp, "Begin Set Mode 0%Xh", wMode);
   VectorComment (szTemp);

   wMode7 = wMode & 0x7F;                    // 7-bit mode number
   if ((wMode7 > 0x13) || ((wMode7 >= 0x08) && (wMode7 <= 0x0C)))
      wMode7 = 0;

   // Give the simulator a chance to override the normal parms
   // to possibly generate a "small frame".
   if ((lpParms = (LPPARMENTRY) SimGetParms (wMode7)) == NULL)
      lpParms = GetParmTable (wMode7);

   SetBIOSVars (wMode7, lpParms);
   SetRegs (lpParms);

   SetAll2CPU (TRUE);
   ModeLoadDAC (wMode7, lpParms);

   // Just like the BIOS, if the high-order bit is set, then don't
   // clear memory.
   if ((wMode & 0x80) != 0x80)
      ModeClearMemory (wMode7);

   // Load the font after the memory clear, in case a fast memory clear
   // was used internally by the simulator.
   ModeLoadFont (wMode7, lpParms);
   SetAll2CPU (FALSE);

   // Tell the vector file what is going on
   VectorComment ("End Set Mode");

   // Notify the vgasim layer that we have changed modes
   SimModeDirty();
}

//
//    SetDac - Set a specific DAC location to a given value
//
//    Entry:   index    Index of external palette
//          red         Red value
//          green    Green value
//          blue     Blue value
//    Exit: None
//
void SetDac (BYTE index, BYTE red, BYTE green, BYTE blue)
{
   BOOL  bSim;

   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

   if (!bSim) _disable ();
   IOByteWrite (DAC_WINDEX, index);
   IOByteWrite (DAC_DATA, red);
   IOByteWrite (DAC_DATA, green);
   IOByteWrite (DAC_DATA, blue);
   if (!bSim) _enable ();
}

//
//    GetDac - Get values from a specific DAC location
//
//    Entry:   index    Index of external palette
//          lpRed    Pointer to returned red value
//          lpGreen     Pointer to returned green value
//          lpBlue      Pointer to returned blue value
//    Exit: None
//
void GetDac (BYTE index, LPBYTE lpRed, LPBYTE lpGreen, LPBYTE lpBlue)
{
   BOOL  bSim;

   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

   if (!bSim) _disable ();
   IOByteWrite (DAC_RINDEX, index);
   *lpRed = (BYTE) IOByteRead (DAC_DATA);
   *lpGreen = (BYTE) IOByteRead (DAC_DATA);
   *lpBlue = (BYTE) IOByteRead (DAC_DATA);
   if (!bSim) _enable ();
}

//
//    FillDAC - Load the entire RAMDAC with a single value
//
//    Entry:   red      Red value
//          green Green value
//          blue  Blue value
//    Exit: None
//
void FillDAC (BYTE red, BYTE green, BYTE blue)
{
   int      i;
   BOOL  bSim;

   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

   if (!bSim) _disable ();
   IOByteWrite (DAC_WINDEX, 0);        // Assume auto-increment
   for (i = 0; i < 256; i++)
   {
      IOByteWrite (DAC_DATA, red);
      IOByteWrite (DAC_DATA, green);
      IOByteWrite (DAC_DATA, blue);
   }
   if (!bSim) _enable ();
}

//
//    FillDACLength - Load a range of RAMDAC registers with a single value
//
//    Entry:   idx      Starting indx
//          length   Number of registers
//          red      Red value
//          green Green value
//          blue  Blue value
//    Exit: None
//
void FillDACLength (BYTE idx, WORD length, BYTE red, BYTE green, BYTE blue)
{
   int      i;
   BOOL  bSim;

   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;

   if (!bSim) _disable ();
   IOByteWrite (DAC_WINDEX, idx);         // Assume auto-increment
   for (i = 0; i < (int) length; i++)
   {
      IOByteWrite (DAC_DATA, red);
      IOByteWrite (DAC_DATA, green);
      IOByteWrite (DAC_DATA, blue);
   }
   if (!bSim) _enable ();
}

//
//    SetDacBlock - Set a range of DAC registers
//
//    Entry:   lpDac    Pointer to DAC array
//          start    Starting index
//          count    Number of registers to load
//    Exit: None
//
void SetDacBlock (LPBYTE lpDac, BYTE start, WORD count)
{
   BOOL  bSim;

   bSim = (SimGetType () & SIM_SIMULATION) ? TRUE : FALSE;
   if (!bSim) _disable ();
   IOByteWrite (DAC_WINDEX, start);
   while (count--)
   {
      IOByteWrite (DAC_DATA, *lpDac++);
      IOByteWrite (DAC_DATA, *lpDac++);
      IOByteWrite (DAC_DATA, *lpDac++);
   }
   if (!bSim) _enable ();
}

//
//    SetRegs - Load a parameter table into the VGA registers
//
//    Entry:   lpParms     Pointer to 64 byte parameter table (PARMENTRY)
//    Exit: None
//
void SetRegs (LPPARMENTRY lpParms)
{
   int      i;
   WORD  wCRTC;

   // Blank screen during the register changes (will be
   // re-enabled at the end of this routine)
   IOByteRead (INPUT_MSTATUS_1); IOByteRead (INPUT_CSTATUS_1);
   IOByteWrite (ATC_INDEX, 0x00);

   // Load the Sequencer and the clock
   IOWordWrite (SEQ_INDEX, 0x0100);    // Sync reset
   for (i = 0; i < 4; i++)
   {
      IOByteWrite (SEQ_INDEX, (BYTE) (i + 1));
      IOByteWrite (SEQ_DATA, lpParms->seq_data[i]);
   }

   IOByteWrite (MISC_OUTPUT, lpParms->misc);
   IOWordWrite (SEQ_INDEX, 0x0300);    // End sync reset

   // Load the CRTC
   wCRTC = CRTC_MINDEX;
   if (lpParms->misc & 1) wCRTC = CRTC_CINDEX;
   IOWordWrite (wCRTC, 0x2011);           // Disable write protection
   for (i = 0; i < 25; i++)
   {
      IOByteWrite (wCRTC, (BYTE) i);
      IOByteWrite ((WORD) (wCRTC + 1), lpParms->crtc_data[i]);
   }

   // Load the GDC
   for (i = 0; i < 9; i++)
   {
      IOByteWrite (GDC_INDEX, (BYTE) i);
      IOByteWrite (GDC_DATA, lpParms->gdc_data[i]);
   }

   // Load the attribute controller
   IOByteRead ((WORD) (wCRTC + 6));
   for (i = 0; i < 20; i++)
   {
      IOByteWrite (ATC_INDEX, (BYTE) i);
      IOByteWrite (ATC_INDEX, lpParms->atc_data[i]);
   }
   IOByteWrite (ATC_INDEX, 0x34);         // VGA register not in parm table (enable
   IOByteWrite (ATC_INDEX, 0x00);         // CRTC palette access, as well)

   IOByteWrite (wCRTC + 6, 0);            // Clear VSYNC select
}

//
//    GetParmTable - Retrieve the current mode table
//
//    Entry:   mode        Mode number
//    Exit: <LPPARMENTRY>  Pointer to 64-byte mode table
//
//    Assume "mode" has been qualified.
//
LPPARMENTRY GetParmTable (WORD mode)
{
   static BYTE tblText200[] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 17, 18, 26, 27, 28
   };
   static BYTE tblText350[] = {
      19, 20, 21, 22, 4, 5, 6, 7
   };
   static BYTE tblText400[] = {
      23, 23, 24, 24, 4, 5, 6, 25
   };
   LPPARMENTRY    lpParms;
   static SEGOFF  lpVGAInfo = (SEGOFF) (0x00000489);

   lpParms = (LPPARMENTRY) &tblParameterTable;

   // 200 scan line text or graphics modes
   if ((mode > 7) || ((MemByteRead (lpVGAInfo) & 0x80) == 0x80))
      return (lpParms + tblText200[mode]);

   // 400 scan line text
   if ((MemByteRead (lpVGAInfo) & 0x10) == 0x10)
      return (lpParms + tblText400[mode]);

   // 350 scan line text
   return (lpParms + tblText350[mode]);
}

//
//    SetBIOSVars - Set BIOS variables based on mode number
//
//    Entry:   mode     Mode number
//          lpParms     Pointer PARMENTRY data structure
//    Exit: None
//
void SetBIOSVars (WORD mode, LPPARMENTRY lpParms)
{
   static SEGOFF  lpEquip = (SEGOFF) (0x00000410);
   BOOL        bMono;
   int            i;
   WORD        w;

   bMono = (mode == 0x07) || (mode == 0x0F);

   MemByteWrite ((SEGOFF) (0x00000449), (BYTE) mode);
   MemWordWrite ((SEGOFF) (0x0000044A), lpParms->columns);
   w = lpParms->regen_length_low | (((WORD) lpParms->regen_length_high) << 8);
   MemWordWrite ((SEGOFF) (0x0000044C), w);
   for (i = 0; i < 9; i += 2)
      MemWordWrite ((SEGOFF) (0x0000044E + i), 0);
   MemByteWrite ((SEGOFF) (0x00000460), lpParms->crtc_data[0x0B]);
   MemByteWrite ((SEGOFF) (0x00000461), lpParms->crtc_data[0x0A]);
   MemByteWrite ((SEGOFF) (0x00000462), 0);
   MemWordWrite ((SEGOFF) (0x00000463), (WORD) (bMono ? 0x3B4 : 0x3D4));
   MemByteWrite ((SEGOFF) (0x00000484), lpParms->rows);
   MemWordWrite ((SEGOFF) (0x00000485), lpParms->char_height);
   MemByteWrite (lpEquip, (BYTE) ((MemByteRead (lpEquip) & 0xCF) | (bMono ? 0x30 : 0x20)));
}

//
//    ModeLoadFont - Load appropriate font for the mode set
//
//    Entry:   mode     Mode number
//          lpParms     Pointer PARMENTRY data structure
//    Exit: None
//
void ModeLoadFont (WORD mode, LPPARMENTRY lpParms)
{
   static SEGOFF  lpINT43 = (SEGOFF) (0x0000010C);
   LPBYTE         lpFont;
   LPBYTE         lpFastFont;

   lpFont = tblFont8x8;
   lpFastFont = mapFont8x8;
   if (lpParms->char_height == 14)
   {
      lpFont = tblFont8x14;
      lpFastFont = mapFont8x14;
   }
   if (lpParms->char_height == 16)
   {
      lpFont = tblFont8x16;
      lpFastFont = mapFont9x16;
   }
   MemDwordWrite (lpINT43, (DWORD)((size_t)(lpFont)));

   if ((mode <= 0x03) || (mode == 0x07))
   {
      // Do a fast load of the font for simulation purposes. If this routine
      // does not exist (as for the physical model) then do a real font load.
      if (!SimLoadFont (lpFastFont))
      {
         LoadFont (lpFont, lpParms->char_height, 0, 256, 0);
         if (lpParms->char_height == 16)
            LoadFixup (tblFont9x16, 16, 0);
      }
      // Force the 9x14 fixup font to be loaded if needed
      if ((lpParms->char_height == 14) && (mode == 0x07))
         LoadFixup (tblFont9x14, 14, 0);
   }
}

//
//    ModeLoadDAC - Load appropriate DAC values for the mode set
//
//    Entry:   mode     Mode number
//          lpParms     Pointer PARMENTRY data structure
//    Exit: None
//
void ModeLoadDAC (WORD mode, LPPARMENTRY lpParms)
{
   static SEGOFF  lpInfo = (SEGOFF) (0x00000489);
   LPBYTE         lpDAC;
   int            nLength;

   IOByteWrite (0x3C6, 0xFF);
   lpDAC = tbl16ColorDAC;
   nLength = nDAC16color;

   if ( ((mode < 3) && ((MemByteRead (lpInfo) & 0x80) == 0x80)) ||
      ((mode >= 4) && (mode <= 6)) ||
      ((mode >= 0x0D) && (mode <= 0x0E)) )
   {
      lpDAC = tblCGADAC;
      nLength = nDACcga;
   }

   if ((mode == 0x07) || (mode == 0x0F))
   {
      lpDAC = tblMonoDAC;
      nLength = nDACmono;
   }

   if (mode == 0x13)
   {
      lpDAC = tbl256ColorDAC;
      nLength = nDAC256color;
   }

   // Do a fast load of the DAC for simulation purposes. If this routine
   // does not exist (as for the physical model) then do a real DAC load.
   if (!SimLoadDac (lpDAC, nLength))
   {
      SetDacBlock (lpDAC, 0, (WORD) nLength);

      // Clear unused DAC locations
      FillDACLength ((BYTE) nLength, (WORD) (256 - nLength), 0, 0, 0);  // Std BIOS doesn't do this
   }
}

//
//    ModeClearMemory - Clear video memory for a given mode
//
//    Entry:   mode  Mode number
//    Exit: None
//
void ModeClearMemory (WORD mode)
{
   WORD     wFill, wLength;
   SEGOFF   lpVideo;

   switch (mode)
   {
      case 0:
      case 1:
      case 2:
      case 3:

         lpVideo = (SEGOFF) 0xB8000000;
         wFill = 0x0720;
         wLength = 0x4000;
         break;

      case 4:
      case 5:
      case 6:

         lpVideo = (SEGOFF) 0xB8000000;
         wFill = 0x0000;
         wLength = 0x4000;
         break;

      case 7:

         lpVideo = (SEGOFF) 0xB0000000;
         wFill = 0x0720;
         wLength = 0x4000;
         break;

      case 0x0D:
      case 0x0E:
      case 0x0F:
      case 0x10:
      case 0x11:
      case 0x12:
      case 0x13:
      default:

         lpVideo = (SEGOFF) 0xA0000000;
         wFill = 0x0000;
         wLength = 0x8000;
         break;
   }

   // Do a fast memory clear for simulation purposes. If this routine does
   // not exist (as for the physical model) then do a real memory clear.
   // Note that the high-order word of the DWORD is going to be "0" for
   // each of the standard VGA modes.
   if (!SimClearMemory ((DWORD) wFill))
   {
      while (wLength--)
      {
         MemWordWrite (lpVideo, wFill);
         lpVideo += 2;
      }
   }
}

//
//    SetAll2CPU - Set/clear "all-to-CPU" bit in sequencer
//
//    Entry:   bSetFlag    Flag (TRUE = Set all CPU access, FALSE = Normal)
//    Exit:    None
//
void SetAll2CPU (BOOL bSetFlag)
{
   BYTE  byTemp;

   IOByteWrite (SEQ_INDEX, 0x01);
   byTemp = (BYTE) (IOByteRead (SEQ_DATA) & 0xDF);
   if (bSetFlag) byTemp |= 0x20;
   IOByteWrite (SEQ_DATA, byTemp);
}

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
