/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "fbflcn_defines.h"
#include "config/fbfalcon-config.h"

#ifndef MEMORY_H
#define MEMORY_H


#define SUBPS 2
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))
  #if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100))
    #define MAX_PARTS 24
  #else
    #define MAX_PARTS 16
  #endif
  #define DQ_BITS_IN_DWORD  32
  #define DBI_BITS_IN_DWORD 4
#else       // GDDR
  #define MAX_PARTS 6
  #define DQ_BITS_IN_DWORD  8 // DQ_BITS_IN_SUBP 32
  #define DBI_BITS_IN_DWORD 1 // DBI_BITS_IN_SUBP 4
#endif

#define PARTS_PER_SITE  4
#define MAX_SITES  (MAX_PARTS/PARTS_PER_SITE)

#define BYTE_PER_SUBP 4

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#define CMD_BRLSHFT_REGS_IN_SUBP   4
#define CMD_INTRPLTR_REGS_IN_SUBP  7
#define VREF_CODE_CTRL_SETS 3    // three sets of vref codes for PAM 4 support
#else
#define CMD_BRLSHFT_REGS_IN_SUBP   3
#define CMD_INTRPLTR_REGS_IN_SUBP  5
#define VREF_CODE_CTRL_SETS 1    // single set of Vref codes
#endif

#define VREF_DFE_CTRLS_IN_BYTE     3
#define VREF_CODE_CTRLS_IN_BYTE    3

#define VREF_DFE_REGS_IN_BYTE 3
#define VREF_DFE_REGS_IN_SUBP (BYTE_PER_SUBP * VREF_DFE_REGS_IN_BYTE)

#define VREF_DFE_REGS_IN_BYTE 3

#define DWORDS_IN_CHANNEL 2
#define CHANNELS_IN_SUBP  2
#define ECC_BITS_IN_DWORD 2
#define DQ_BITS_IN_CHANNEL  (DWORDS_IN_CHANNEL * DQ_BITS_IN_DWORD)
#define DBI_BITS_IN_CHANNEL (DWORDS_IN_CHANNEL * DBI_BITS_IN_DWORD) 
#define ECC_BITS_IN_CHANNEL (DWORDS_IN_CHANNEL * ECC_BITS_IN_DWORD)
#define DQS_BITS_IN_CHANNEL (DQ_BITS_IN_CHANNEL / DQ_BITS_IN_DWORD)
#define DQ_BITS_IN_SUBP  (CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL * DQ_BITS_IN_DWORD)
#define DBI_BITS_IN_SUBP (CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL * DBI_BITS_IN_DWORD)
#define ECC_BITS_IN_SUBP (CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL * ECC_BITS_IN_DWORD)
#define DQS_BITS_IN_SUBP (DQ_BITS_IN_SUBP / DQ_BITS_IN_DWORD)
#define DQS_BITS_IN_PA   (DQS_BITS_IN_SUBP * SUBPS)
#define CHANNELS_IN_PA (CHANNELS_IN_SUBP * SUBPS)
#define TOTAL_DQ_BITS  (DQ_BITS_IN_SUBP * SUBPS * MAX_PARTS)
#define TOTAL_DBI_BITS (DBI_BITS_IN_SUBP * SUBPS * MAX_PARTS)
#define TOTAL_ECC_BITS (ECC_BITS_IN_SUBP * SUBPS * MAX_PARTS)
#define TOTAL_DQS_BITS (TOTAL_DQ_BITS / DQ_BITS_IN_DWORD)
#define TOTAL_CMD_REGS ((CMD_BRLSHFT_REGS_IN_SUBP + CMD_INTRPLTR_REGS_IN_SUBP ) * SUBPS * MAX_PARTS)

#define TOTAL_VREF_DFE_CTRLS (VREF_DFE_CTRLS_IN_BYTE * BYTE_PER_SUBP * SUBPS * MAX_PARTS)
#define TOTAL_VREF_DFEG6X_CTRLS (VREF_DFE_REGS_IN_BYTE * BYTE_PER_SUBP * SUBPS * MAX_PARTS)
#define TOTAL_VREF_CODE_CTRLS (VREF_CODE_CTRLS_IN_BYTE * VREF_CODE_CTRL_SETS * BYTE_PER_SUBP * SUBPS * MAX_PARTS)

#define TOTAL_VREF_TRACKING_SHADOW_CTRLS (2 * BYTE_PER_SUBP * SUBPS * MAX_PARTS)

typedef enum REGISTER_ACCESS_ENUM { REGISTER_READ=0, REGISTER_WRITE=1 } REGISTER_ACCESS_TYPE;

typedef enum VREF_CODE_FIELD { EYE_LOW=0, EYE_MID=1, EYE_HIGH=2 } VREF_CODE_FIELD;


LwU32 gSwConfig;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))

typedef struct
{
    LwBool enabled;
    LwU8 numValidPa;
    LwU8 pa[PARTS_PER_SITE];
    LwU8 valid_channel;
} HBM_BCAST_STRUCT;

HBM_BCAST_STRUCT hbm_bcast_data[MAX_SITES];

//-----------------------------------------------------------------------------------------------------------------
// Device ID content and field definitions
//-----------------------------------------------------------------------------------------------------------------
#define IEEE1500_INSTRUCTION_DEVICE_ID 0x0e
// See HBM3 spec for complete list of entries, for now we're only interested in detecting the manufacturer
// and package setup
// Bit         Bit
// Position    Field                       Description
//
// [111:104]   MANUFACTURING_YEAR[7:0]     Binary encoded year:
//                                         2020 = 00000000; 2024 = 00000100
//
#define DEVICE_ID_HBM3_MANUFACTURING_YEAR  (111%32):(104%32)
// [103:96]    MANUFACTURING_WEEK[7:0]     Binary encoded week:
//                                         WW52 = 00110100
//
#define DEVICE_ID_HBM3_MANUFACTURING_WEEK  (103%32):(96%32)
// [95:32]     SERIAL_NO[63:0]             Unique device ID
//
#define DEVICE_ID_HBM3_SERIAL_NO  (95%32):(32%32)
// [31:28]     MANUFACTURER_ID[3:0]        0001 - Samsung
//                                         0110 - SK Hynix
//                                         1111 - Micron
//                                         All others - Reserved
#define DEVICE_ID_HBM3_MANUFACTURER_ID_FIELD   31:28
//
// [27:24]     DENSITY[3:0]                Memory density per channel (see HBM3 Channel Addressing)
//                                         0000 - 2 Gb
//                                         0001 - 4 Gb (8Gb 8-High)
//                                         0010 - 6 Gb (8Gb 12-High)
//                                         0011 - 8 Gb (8Gb 16-High)
//                                         0100 - 4 Gb
//                                         0101 - 8 Gb (16Gb 8-High)
//                                         0110 - 12 Gb (16Gb 12-High)
//                                         0111 - 16 Gb (16Gb 16-High)
//                                         1000 - 6 Gb
//                                         1001 - 12 Gb (24Gb 8-High)
//                                         1010 - 18 Gb (24Gb 12-High)
//                                         1011 - 24 Gb (24Gb 16-High)
//                                         1100 - 8 Gb
//                                         1101 - 16 Gb (32Gb 8-High)
//                                         1110 - 24 Gb (32Gb 12-High)
//                                         1111 - 32 Gb (32Gb 16-High)
#define DEVICE_ID_HBM3_DENSITY_FIELD  27:24
//
// [23:8]      CHANNEL_AVAILABLE[15:0]     Channel Available
//                                         0 - Channel not present / not working
//                                         1 - Channel present / working
//                                         Channel encoding (1 bit per channel):
//                                         bit 8: channel a
//                                         bit 9: channel b
//                                         ...
//                                         bit 22: channel o
//                                         bit 23: channel p
#define DEVICE_ID_HBM3_CHANNEL_AVAILABLE_FIELD  23:8
//
// [7:0]       MODEL_PART_NUMBER[7:0]      Vendor reserved
//
#define DEVICE_ID_HBM3_MODEL_PART_NUMBER_FIELD  7:0

typedef struct
{
    LwU8 partNumber;
    LwU8 manufacturingYear;
    LwU8 manufacturingWeek;
    LwU8 density;
    LwU8 manufacturer;
    // LwU32 serialHi;
    // LwU32 serialLow;
    // LwU32 channelAvailable;
    LwBool hynix_es1_workaround;

} HBM_DEVICE_ID_STRUCT;

HBM_DEVICE_ID_STRUCT hbm_device_id;

#endif //#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM))
#endif //#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))


#endif  //MEMORY_H

