//
//    DACDATA.INC - Default RAMDAC tables
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
//    Written by:    Larry Coffey
//    Date:       22 May 1990
//    Last modified: 30 January 2005
//
//
// 16-color mode DAC table
//
#ifdef LW_MODS
namespace LegacyVGA {
#endif

BYTE tbl16ColorDAC[] =
{
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x2A,
   0x00, 0x2A, 0x00,
   0x00, 0x2A, 0x2A,
   0x2A, 0x00, 0x00,
   0x2A, 0x00, 0x2A,
   0x2A, 0x2A, 0x00,
   0x2A, 0x2A, 0x2A,
   0x00, 0x00, 0x15,
   0x00, 0x00, 0x3F,
   0x00, 0x2A, 0x15,
   0x00, 0x2A, 0x3F,
   0x2A, 0x00, 0x15,
   0x2A, 0x00, 0x3F,
   0x2A, 0x2A, 0x15,
   0x2A, 0x2A, 0x3F,
   0x00, 0x15, 0x00,
   0x00, 0x15, 0x2A,
   0x00, 0x3F, 0x00,
   0x00, 0x3F, 0x2A,
   0x2A, 0x15, 0x00,
   0x2A, 0x15, 0x2A,
   0x2A, 0x3F, 0x00,
   0x2A, 0x3F, 0x2A,
   0x00, 0x15, 0x15,
   0x00, 0x15, 0x3F,
   0x00, 0x3F, 0x15,
   0x00, 0x3F, 0x3F,
   0x2A, 0x15, 0x15,
   0x2A, 0x15, 0x3F,
   0x2A, 0x3F, 0x15,
   0x2A, 0x3F, 0x3F,
   0x15, 0x00, 0x00,
   0x15, 0x00, 0x2A,
   0x15, 0x2A, 0x00,
   0x15, 0x2A, 0x2A,
   0x3F, 0x00, 0x00,
   0x3F, 0x00, 0x2A,
   0x3F, 0x2A, 0x00,
   0x3F, 0x2A, 0x2A,
   0x15, 0x00, 0x15,
   0x15, 0x00, 0x3F,
   0x15, 0x2A, 0x15,
   0x15, 0x2A, 0x3F,
   0x3F, 0x00, 0x15,
   0x3F, 0x00, 0x3F,
   0x3F, 0x2A, 0x15,
   0x3F, 0x2A, 0x3F,
   0x15, 0x15, 0x00,
   0x15, 0x15, 0x2A,
   0x15, 0x3F, 0x00,
   0x15, 0x3F, 0x2A,
   0x3F, 0x15, 0x00,
   0x3F, 0x15, 0x2A,
   0x3F, 0x3F, 0x00,
   0x3F, 0x3F, 0x2A,
   0x15, 0x15, 0x15,
   0x15, 0x15, 0x3F,
   0x15, 0x3F, 0x15,
   0x15, 0x3F, 0x3F,
   0x3F, 0x15, 0x15,
   0x3F, 0x15, 0x3F,
   0x3F, 0x3F, 0x15,
   0x3F, 0x3F, 0x3F
};
//
// CGA-type modes DAC table
//
BYTE tblCGADAC[] =
{
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x2A,
   0x00, 0x2A, 0x00,
   0x00, 0x2A, 0x2A,
   0x2A, 0x00, 0x00,
   0x2A, 0x00, 0x2A,
   0x2A, 0x15, 0x00,
   0x2A, 0x2A, 0x2A,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x2A,
   0x00, 0x2A, 0x00,
   0x00, 0x2A, 0x2A,
   0x2A, 0x00, 0x00,
   0x2A, 0x00, 0x2A,
   0x2A, 0x15, 0x00,
   0x2A, 0x2A, 0x2A,
   0x15, 0x15, 0x15,
   0x15, 0x15, 0x3F,
   0x15, 0x3F, 0x15,
   0x15, 0x3F, 0x3F,
   0x3F, 0x15, 0x15,
   0x3F, 0x15, 0x3F,
   0x3F, 0x3F, 0x15,
   0x3F, 0x3F, 0x3F,
   0x15, 0x15, 0x15,
   0x15, 0x15, 0x3F,
   0x15, 0x3F, 0x15,
   0x15, 0x3F, 0x3F,
   0x3F, 0x15, 0x15,
   0x3F, 0x15, 0x3F,
   0x3F, 0x3F, 0x15,
   0x3F, 0x3F, 0x3F,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x2A,
   0x00, 0x2A, 0x00,
   0x00, 0x2A, 0x2A,
   0x2A, 0x00, 0x00,
   0x2A, 0x00, 0x2A,
   0x2A, 0x15, 0x00,
   0x2A, 0x2A, 0x2A,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x2A,
   0x00, 0x2A, 0x00,
   0x00, 0x2A, 0x2A,
   0x2A, 0x00, 0x00,
   0x2A, 0x00, 0x2A,
   0x2A, 0x15, 0x00,
   0x2A, 0x2A, 0x2A,
   0x15, 0x15, 0x15,
   0x15, 0x15, 0x3F,
   0x15, 0x3F, 0x15,
   0x15, 0x3F, 0x3F,
   0x3F, 0x15, 0x15,
   0x3F, 0x15, 0x3F,
   0x3F, 0x3F, 0x15,
   0x3F, 0x3F, 0x3F,
   0x15, 0x15, 0x15,
   0x15, 0x15, 0x3F,
   0x15, 0x3F, 0x15,
   0x15, 0x3F, 0x3F,
   0x3F, 0x15, 0x15,
   0x3F, 0x15, 0x3F,
   0x3F, 0x3F, 0x15,
   0x3F, 0x3F, 0x3F
};
//
// Monochrome DAC
//
BYTE tblMonoDAC[] =
{
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x2A, 0x2A, 0x2A,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x00,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F,
   0x3F, 0x3F, 0x3F
};

//
// 256-color mode DAC
//
BYTE tbl256ColorDAC[] =
{
   0x00, 0x00, 0x00,
   0x00, 0x00, 0x2A,
   0x00, 0x2A, 0x00,
   0x00, 0x2A, 0x2A,
   0x2A, 0x00, 0x00,
   0x2A, 0x00, 0x2A,
   0x2A, 0x15, 0x00,
   0x2A, 0x2A, 0x2A,
   0x15, 0x15, 0x15,
   0x15, 0x15, 0x3F,
   0x15, 0x3F, 0x15,
   0x15, 0x3F, 0x3F,
   0x3F, 0x15, 0x15,
   0x3F, 0x15, 0x3F,
   0x3F, 0x3F, 0x15,
   0x3F, 0x3F, 0x3F,
   0x00, 0x00, 0x00,
   0x05, 0x05, 0x05,
   0x08, 0x08, 0x08,
   0x0B, 0x0B, 0x0B,
   0x0E, 0x0E, 0x0E,
   0x11, 0x11, 0x11,
   0x14, 0x14, 0x14,
   0x18, 0x18, 0x18,
   0x1C, 0x1C, 0x1C,
   0x20, 0x20, 0x20,
   0x24, 0x24, 0x24,
   0x28, 0x28, 0x28,
   0x2D, 0x2D, 0x2D,
   0x32, 0x32, 0x32,
   0x38, 0x38, 0x38,
   0x3F, 0x3F, 0x3F,
   0x00, 0x00, 0x3F,
   0x10, 0x00, 0x3F,
   0x1F, 0x00, 0x3F,
   0x2F, 0x00, 0x3F,
   0x3F, 0x00, 0x3F,
   0x3F, 0x00, 0x2F,
   0x3F, 0x00, 0x1F,
   0x3F, 0x00, 0x10,
   0x3F, 0x00, 0x00,
   0x3F, 0x10, 0x00,
   0x3F, 0x1F, 0x00,
   0x3F, 0x2F, 0x00,
   0x3F, 0x3F, 0x00,
   0x2F, 0x3F, 0x00,
   0x1F, 0x3F, 0x00,
   0x10, 0x3F, 0x00,
   0x00, 0x3F, 0x00,
   0x00, 0x3F, 0x10,
   0x00, 0x3F, 0x1F,
   0x00, 0x3F, 0x2F,
   0x00, 0x3F, 0x3F,
   0x00, 0x2F, 0x3F,
   0x00, 0x1F, 0x3F,
   0x00, 0x10, 0x3F,
   0x1F, 0x1F, 0x3F,
   0x27, 0x1F, 0x3F,
   0x2F, 0x1F, 0x3F,
   0x37, 0x1F, 0x3F,
   0x3F, 0x1F, 0x3F,
   0x3F, 0x1F, 0x37,
   0x3F, 0x1F, 0x2F,
   0x3F, 0x1F, 0x27,
   0x3F, 0x1F, 0x1F,
   0x3F, 0x27, 0x1F,
   0x3F, 0x2F, 0x1F,
   0x3F, 0x37, 0x1F,
   0x3F, 0x3F, 0x1F,
   0x37, 0x3F, 0x1F,
   0x2F, 0x3F, 0x1F,
   0x27, 0x3F, 0x1F,
   0x1F, 0x3F, 0x1F,
   0x1F, 0x3F, 0x27,
   0x1F, 0x3F, 0x2F,
   0x1F, 0x3F, 0x37,
   0x1F, 0x3F, 0x3F,
   0x1F, 0x37, 0x3F,
   0x1F, 0x2F, 0x3F,
   0x1F, 0x27, 0x3F,
   0x2D, 0x2D, 0x3F,
   0x31, 0x2D, 0x3F,
   0x36, 0x2D, 0x3F,
   0x3A, 0x2D, 0x3F,
   0x3F, 0x2D, 0x3F,
   0x3F, 0x2D, 0x3A,
   0x3F, 0x2D, 0x36,
   0x3F, 0x2D, 0x31,
   0x3F, 0x2D, 0x2D,
   0x3F, 0x31, 0x2D,
   0x3F, 0x36, 0x2D,
   0x3F, 0x3A, 0x2D,
   0x3F, 0x3F, 0x2D,
   0x3A, 0x3F, 0x2D,
   0x36, 0x3F, 0x2D,
   0x31, 0x3F, 0x2D,
   0x2D, 0x3F, 0x2D,
   0x2D, 0x3F, 0x31,
   0x2D, 0x3F, 0x36,
   0x2D, 0x3F, 0x3A,
   0x2D, 0x3F, 0x3F,
   0x2D, 0x3A, 0x3F,
   0x2D, 0x36, 0x3F,
   0x2D, 0x31, 0x3F,
   0x00, 0x00, 0x1C,
   0x07, 0x00, 0x1C,
   0x0E, 0x00, 0x1C,
   0x15, 0x00, 0x1C,
   0x1C, 0x00, 0x1C,
   0x1C, 0x00, 0x15,
   0x1C, 0x00, 0x0E,
   0x1C, 0x00, 0x07,
   0x1C, 0x00, 0x00,
   0x1C, 0x07, 0x00,
   0x1C, 0x0E, 0x00,
   0x1C, 0x15, 0x00,
   0x1C, 0x1C, 0x00,
   0x15, 0x1C, 0x00,
   0x0E, 0x1C, 0x00,
   0x07, 0x1C, 0x00,
   0x00, 0x1C, 0x00,
   0x00, 0x1C, 0x07,
   0x00, 0x1C, 0x0E,
   0x00, 0x1C, 0x15,
   0x00, 0x1C, 0x1C,
   0x00, 0x15, 0x1C,
   0x00, 0x0E, 0x1C,
   0x00, 0x07, 0x1C,
   0x0E, 0x0E, 0x1C,
   0x11, 0x0E, 0x1C,
   0x15, 0x0E, 0x1C,
   0x18, 0x0E, 0x1C,
   0x1C, 0x0E, 0x1C,
   0x1C, 0x0E, 0x18,
   0x1C, 0x0E, 0x15,
   0x1C, 0x0E, 0x11,
   0x1C, 0x0E, 0x0E,
   0x1C, 0x11, 0x0E,
   0x1C, 0x15, 0x0E,
   0x1C, 0x18, 0x0E,
   0x1C, 0x1C, 0x0E,
   0x18, 0x1C, 0x0E,
   0x15, 0x1C, 0x0E,
   0x11, 0x1C, 0x0E,
   0x0E, 0x1C, 0x0E,
   0x0E, 0x1C, 0x11,
   0x0E, 0x1C, 0x15,
   0x0E, 0x1C, 0x18,
   0x0E, 0x1C, 0x1C,
   0x0E, 0x18, 0x1C,
   0x0E, 0x15, 0x1C,
   0x0E, 0x11, 0x1C,
   0x14, 0x14, 0x1C,
   0x16, 0x14, 0x1C,
   0x18, 0x14, 0x1C,
   0x1A, 0x14, 0x1C,
   0x1C, 0x14, 0x1C,
   0x1C, 0x14, 0x1A,
   0x1C, 0x14, 0x18,
   0x1C, 0x14, 0x16,
   0x1C, 0x14, 0x14,
   0x1C, 0x16, 0x14,
   0x1C, 0x18, 0x14,
   0x1C, 0x1A, 0x14,
   0x1C, 0x1C, 0x14,
   0x1A, 0x1C, 0x14,
   0x18, 0x1C, 0x14,
   0x16, 0x1C, 0x14,
   0x14, 0x1C, 0x14,
   0x14, 0x1C, 0x16,
   0x14, 0x1C, 0x18,
   0x14, 0x1C, 0x1A,
   0x14, 0x1C, 0x1C,
   0x14, 0x1A, 0x1C,
   0x14, 0x18, 0x1C,
   0x14, 0x16, 0x1C,
   0x00, 0x00, 0x10,
   0x04, 0x00, 0x10,
   0x08, 0x00, 0x10,
   0x0C, 0x00, 0x10,
   0x10, 0x00, 0x10,
   0x10, 0x00, 0x0C,
   0x10, 0x00, 0x08,
   0x10, 0x00, 0x04,
   0x10, 0x00, 0x00,
   0x10, 0x04, 0x00,
   0x10, 0x08, 0x00,
   0x10, 0x0C, 0x00,
   0x10, 0x10, 0x00,
   0x0C, 0x10, 0x00,
   0x08, 0x10, 0x00,
   0x04, 0x10, 0x00,
   0x00, 0x10, 0x00,
   0x00, 0x10, 0x04,
   0x00, 0x10, 0x08,
   0x00, 0x10, 0x0C,
   0x00, 0x10, 0x10,
   0x00, 0x0C, 0x10,
   0x00, 0x08, 0x10,
   0x00, 0x04, 0x10,
   0x08, 0x08, 0x10,
   0x0A, 0x08, 0x10,
   0x0C, 0x08, 0x10,
   0x0E, 0x08, 0x10,
   0x10, 0x08, 0x10,
   0x10, 0x08, 0x0E,
   0x10, 0x08, 0x0C,
   0x10, 0x08, 0x0A,
   0x10, 0x08, 0x08,
   0x10, 0x0A, 0x08,
   0x10, 0x0C, 0x08,
   0x10, 0x0E, 0x08,
   0x10, 0x10, 0x08,
   0x0E, 0x10, 0x08,
   0x0C, 0x10, 0x08,
   0x0A, 0x10, 0x08,
   0x08, 0x10, 0x08,
   0x08, 0x10, 0x0A,
   0x08, 0x10, 0x0C,
   0x08, 0x10, 0x0E,
   0x08, 0x10, 0x10,
   0x08, 0x0E, 0x10,
   0x08, 0x0C, 0x10,
   0x08, 0x0A, 0x10,
   0x0B, 0x0B, 0x10,
   0x0C, 0x0B, 0x10,
   0x0D, 0x0B, 0x10,
   0x0F, 0x0B, 0x10,
   0x10, 0x0B, 0x10,
   0x10, 0x0B, 0x0F,
   0x10, 0x0B, 0x0D,
   0x10, 0x0B, 0x0C,
   0x10, 0x0B, 0x0B,
   0x10, 0x0C, 0x0B,
   0x10, 0x0D, 0x0B,
   0x10, 0x0F, 0x0B,
   0x10, 0x10, 0x0B,
   0x0F, 0x10, 0x0B,
   0x0D, 0x10, 0x0B,
   0x0C, 0x10, 0x0B,
   0x0B, 0x10, 0x0B,
   0x0B, 0x10, 0x0C,
   0x0B, 0x10, 0x0D,
   0x0B, 0x10, 0x0F,
   0x0B, 0x10, 0x10,
   0x0B, 0x0F, 0x10,
   0x0B, 0x0D, 0x10,
   0x0B, 0x0C, 0x10
};

#ifdef LW_MODS
}
#endif

//
//    Copyright (c) 1994-2004 Elpin Systems, Inc.
//    Copyright (c) 2004-2005 SillyTutu.com, Inc.
//    All rights reserved.
//
