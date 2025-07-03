/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   rmt_detectedid.h
 *
 * @brief  Header file for RM DetectEdid Test
 *
 *
 */

typedef struct
{
    UINT08  pclocklo;             // Pixel clock /10,000 (ie: 135Mhz = 13500)
    UINT08  pclockhi;

    UINT08  hactive;            // Lower 8 bits of HActive
    UINT08  hblank;             // Lower 8 bits of HBlank

    UINT08  hactblank;          // HActive/HBlank High Bits
    UINT08  vactive;            // Lower 8 bits of VActive
    UINT08  vblank;             // Lower 8 bits of VBlank
    UINT08  vactblank;          // VActive/VBlank High Bits
    UINT08  HSOffset;           // Lower 8 bits of HSync Offset
    UINT08  HSPulseWidth;       // Lower 8 bits of HSync Pulse Width
    UINT08  VSPwOff;            // VSOffset/PulseWidth High bits.
    UINT08  SyncCombo;          // Hi bits
    UINT08  hImageSize;         // mm - low 8 bits
    UINT08  vImageSize;         // mm - low 8 bits
    UINT08  ImageSizeHi;        // mm - hi 4 bits
    UINT08  HBorder;            // Pixels
    UINT08  VBorder;            // Pixels

    UINT08  Flags;
} DTB;

typedef struct
{
  UINT08  Flag0Lo;     // must be 0
  UINT08  Flag0Hi;     // must be 0
  UINT08  Flag1;       // must be 0
  UINT08  DataTypeTag;
  UINT08  Flag2;       // must be 0
  UINT08  Data[13];    // different based on data type tag
} DTDB;

#define DTT_MONITOR_SERIAL_NUMBER 0xFF
#define DTT_ASCII_STRING          0xFE
#define DTT_MONITOR_RANGE_LIMITS  0xFD
#define DTT_MONITOR_NAME          0xFC
#define DTT_ADD_COLOR_POINT_DATA  0xFB
#define DTT_ADD_STANDARD_TIMINGS  0xFA

#define LW_EDID1_HACTBLANK_HBLANKHI                                  3:0
#define LW_EDID1_HACTBLANK_HACTIVEHI                                 7:4
#define LW_EDID1_VACTBLANK_VBLANKHI                                  3:0
#define LW_EDID1_VACTBLANK_VACTIVEHI                                 7:4
#define LW_EDID1_VSPWOFF_VS_PWLO                                     3:0
#define LW_EDID1_VSPWOFF_VS_OFFLO                                    7:4
#define LW_EDID1_SYNCCOMBO_VS_PW_HI                                  1:0
#define LW_EDID1_SYNCCOMBO_VS_OFF_HI                                 3:2
#define LW_EDID1_SYNCCOMBO_HS_PW_HI                                  5:4
#define LW_EDID1_SYNCCOMBO_HS_OFF_HI                                 7:6
#define LW_EDID1_IMAGESIZEHI_V                                       3:0
#define LW_EDID1_IMAGESIZEHI_H                                       7:4
#define LW_EDID1_FLAGS_STEREO_INTERLEAVED                            0:0
#define LW_EDID1_FLAGS_HPOLARITY                                     1:1
#define LW_EDID1_FLAGS_VPOLARITY                                     2:2
#define LW_EDID1_FLAGS_SYNC_CONFIG                                   4:3
#define LW_EDID1_FLAGS_STEREO_MODE                                   6:5
#define LW_EDID1_FLAGS_INTERLACED                                    7:7

typedef struct
{
    UINT08 header;           // Block Header == 0x40
    UINT08 version;          // Version number 1->255 "0" is invalid
    struct
    {
        UINT08 standard;
        struct
        {
            UINT08 type;
            UINT08 year;
            UINT08 month;
            UINT08 day;
        } version;
        struct
        {
            UINT08 flags;
            UINT08 data;
        } format;
        struct
        {
            UINT08 minlinkfreq;
            UINT08 maxlinkfreqlo;
            UINT08 maxlinkfreqhi;
            UINT08 crossoverlo;
            UINT08 crossoverhi;
        } clock;
    } digital;
    struct
    {
        struct
        {
            UINT08 layout;
            UINT08 config;
            UINT08 shape;
        } subpixel;
        struct
        {
            UINT08 horizontal;
            UINT08 vertical;
        } pixelpitch;
        UINT08 flags;
    } display;
    struct
    {
        UINT08 misc;
        struct
        {
            UINT08 flags;
            UINT08 vfreqlo;
            UINT08 vfreqhi;
            UINT08 hfreqlo;
            UINT08 hfreqhi;
        } framerate;
        UINT08 orientation;
        struct
        {
            UINT08 startup;
            UINT08 preferred;
            UINT08 caps1;
            UINT08 caps2;
        } colordecode;
        struct
        {
            UINT08 flags;
            struct
            {
                UINT08 blue;
                UINT08 green;
                UINT08 red;
            } bgr;
            struct
            {
                UINT08 cb_pb;
                UINT08 y;
                UINT08 cr_pr;
            } ycrcb_yprpb;
        } colordepth;
        UINT08 aspect;
        UINT08 packetizedinfo[16];
    } caps;
    UINT08 unused[17];
    UINT08 audio[9];
    struct
    {
        UINT08 lumcount;
        UINT08 blue[15];
        UINT08 green[15];
        UINT08 red[15];
    } gamma;
    UINT08 checksum;
} DI_EXT;

typedef struct
{
    // 8 bytes of header
    UINT08 header1;            // Initial byte. EDID1 = 00h
    UINT08 midhead[6];         // Middle 6 bytes. EDID1 = All FFh
    UINT08 header2;            // End Header. EDID1 = 00h.

    // 10 bytes of manufacturers info
    UINT08 mfgEISAidlo;        // EISA manufacturers ID
    UINT08 mfgEISAidhi;
    UINT08 productcodelo;      // Product code for this display
    UINT08 productcodehi;
    UINT08 serial[4];          // Serial number of display
    UINT08 mfgdatemonth;       // Month of manufacture
    UINT08 mfgdateyear;        // Year of manufacture

    // 2 bytes of EDID version information
    UINT08 edidversion;        // Major version number
    UINT08 edidrevision;       // Revision number

    // 5 bytes - basic display parameters & features
    UINT08 videoinputtype;     // Video Input Definition
    UINT08 imagesizeh;         // Horizontal Image size in cm.
    UINT08 imagesizev;         // Vertical Image size in cm.
    UINT08 gamma;              // Display Transfer Characteristics
    UINT08 features;           // Feature Support

    // 10 bytes - Color characteristics
    UINT08 colorchars[10];     // Color Characteristics

    // 3 bytes of established timing information
    UINT08 established[2];     // Established timings
    UINT08 mfgtimings;         // Manufacturers timings.

    // 16 bytes of standard timing definitions
    UINT08 stdtiming[16];      // Even offsets = (Horizontal pixels/8)-31
                               // Odd  offsets = Vertical aspect and refresh

    // 72 bytes of detailed timing descriptors
    DTB    detailtiming[4];    // Detailed Timing Descritpors

    UINT08 Extensions;         // Number of extension blocks
                               // (lwrrently only handle 1)
    UINT08 checksum;           // Checksum of block
} EDID1;
