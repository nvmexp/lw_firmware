//
//    COREC6.CPP - VGA Core "C" Library File #6 (VESA BIOS Extensions [VBE])
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       16 November 2005
//    Last modified: 16 November 2005
//
//    Routines in this file:
//    VBESetMode        Set a VBE mode
//    VBEGetInfo        Retrieve information about the VBE subsystem
//    VBEGetModeInfo    Retrieve information about the VBE mode
//
//
#include <stdio.h>
#include <string.h>
#include    <assert.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
namespace LegacyVGA {
#endif

//
//    VBESetMode - Set a VBE mode
//
//    Entry:   wMode    Mode number
//    Exit: <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL VBESetMode (WORD wMode)
{
   return (FALSE);
}

//
//    VBEGetInfo - Retrieve information about the VBE subsystem
//
//    Entry:   pvbevi      Pointer to the VBEVESAINFO data structure
//    Exit: <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL VBEGetInfo (PVBEVESAINFO pvbevi)
{
   return (FALSE);
}

//
//    VBEGetModeInfo - Retrieve information about the VBE mode
//
//    Entry:   wMode    Mode to get information about
//          pvbevi      Pointer to the VBEVESAINFO data structure
//    Exit: <BOOL>      Success flag (TRUE = Successful, FALSE = Not)
//
BOOL VBEGetModeInfo (WORD wMode, PVBEMODEINFO pvbemi)
{
   return (FALSE);
}

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2000 Elpin Systems, Inc.
//    Copyright (c) 2005 SillyTutu.com, Inc.
//    All rights reserved.
//
