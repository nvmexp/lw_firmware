//
//    COREDATA.C - VGA Core global data definitions
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       17 February 1994
//    Last modified: 16 April 2005
//
//
#define  _DATAFILE_
#include <stdlib.h>
#include "vgacore.h"
#include "vgasim.h"

#ifdef LW_MODS
using namespace LegacyVGA;
#endif

// Include font, parameter table, and DAC data
#ifdef   __cplusplus
extern "C" {         /* Assume C declarations for C++ */
#endif               /* __cplusplus */

#include "font8X8.h"
#include "font8X14.h"
#include "font8X16.h"
#include "font9X14.h"
#include "font9X16.h"
#include "map8x8.h"
#include "map8x14.h"
#include "map9x16.h"
#include "biosparm.h"
#include "dacdata.h"

char  _copyright[] = "Copyright (c) 1994-2004 Elpin Systems, Inc., Copyright (c) 2004-2005 SillyTutu.com, Inc., All rights reserved.";
char  _author[] = "Written by Larry Coffey. 2/17/94";

int   _vcerr_code = ERROR_NONE;
int   _vcerr_part = 0;
int   _vcerr_test = 0;
DWORD _vcerr_address = 0;
WORD  _vcerr_index = 0;
DWORD _vcerr_expected = 0;
DWORD _vcerr_actual = 0;

WORD  tblFontBlock[] = {0x0000, 0x4000, 0x8000, 0xC000, 0x2000, 0x6000, 0xA000, 0xE000};

BYTE  _line_color = 0x07;
BYTE  _line_rop = 0x00;
WORD  _line_columns = 80;
WORD  _line_startaddr = 0;

int   nDAC16color = sizeof (tbl16ColorDAC) / 3;
int   nDACcga = sizeof (tblCGADAC) / 3;
int   nDACmono = sizeof (tblMonoDAC) / 3;
int   nDAC256color = sizeof (tbl256ColorDAC) / 3;

BOOL  _bCaptureStream = FALSE;

#ifdef __cplusplus
}                    /* End of extern "C" { */
#endif                  /* __cplusplus */

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
