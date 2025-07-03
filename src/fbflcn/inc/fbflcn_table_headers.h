/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_TABLE_HEADERS_H
#define FBFLCN_TABLE_HEADERS_H

//
// This file is source into testbench and bringup tools, as such it should not have includes specific to either //hw nor //sw tree
//
#include <lwtypes.h>
#define Lw64 LwU64

#include "vbios/ucode_interface.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_fbfalcon_private.h"


// Addendum to manual definitions
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#define LW_PFB_FBPA_GENERIC_MRS14_ADR_MICRON_CORE_VOLTAGE_ADDENDUM  5:0
#else
#define LW_PFB_FBPA_GENERIC_MRS14_ADR_MICRON_CORE_VOLTAGE_ADDENDUM  LW_PFB_FBPA_GENERIC_MRS14_ADR_MICRON_CORE_VOLTAGE
#endif

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
#include "vbios/cert30.h"
    #define FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR           BCRT30_VDPA_ENTRY_INT_BLK_BASE_SPTR
    #define FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE           LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_SIZE
    #define FBFLCN_VDPA_ENTRY_TYPE_USAGE                  LW_BCRT30_VDPA_ENTRY_TYPE_DIRT_ID
#else
    #include "vbios/cert20.h"
    #define FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR           BCRT20_VDPA_ENTRY_INT_BLK_BASE_SPTR
    #define FBFLCN_VDPA_ENTRY_INT_BLK_SIZE_SIZE           LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_SIZE
    #define FBFLCN_VDPA_ENTRY_TYPE_USAGE                  LW_BCRT20_VDPA_ENTRY_TYPE_USAGE
#endif


#define db LwU8
#define dw LwU16
#define dd LwU32

//
// FBFalcon DataSpec
// this table helps to organize and access the vbios tables in the fbfalcon dmem space
//
// table definitions:
// https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures#RM_Table_Specs
//

//------------------------------------------------------------------------------------------------
// MEMTABLES Header structure
//
#define FBFALCON_DATA_SCRIPT_SPECIFIC_HDR_V1 0xaf01

typedef struct FBFalconDataSpecificHeaderV1
{
    dw Version;                 // FBFALCON_DATA_SCRIPT_SPECIFIC_HDR_V1
    dw HeaderSize;              // sizeof(FBFalconDataSpecificHeaderV1)
    dw DataSize;
    dw MemInfoTblOffset;
    dw MemPartInfoTblOffset;
    dw MemTrainingTblOffset;
    dw MemTrainingPatTblOffset;
    dw PerfTblOffset;
    dw PerfMclkTblOffset;
    dw PstateMclkFreqTblOffset;
    dw PerfMemTweakTblOffset;
    dw Rsvd1;
} FBFalconDataSpecificHeaderV1;




//************************************************************************************************
//   DEFINES
//*****************************/*******************************************************************
//
// BIT internal use only
// file: sw/main/bios/cor88/inc/bit.inc
// wiki: https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/BIT#BIT_INTERNAL_USE_ONLY_.28Version_2.29

typedef struct __attribute__((__packed__)) BitInternalUseOnlyV2
{
        dd bVersion                ;// ; BIOS Internal Version
        db bOEMVersion             ;// ; BIOS Internal OEM Version
        dw bFeatures               ;// ; BIOS Internal Features
        dd bP4MagicNumber          ;// ; Perforce Checkin Number
        dw bBoardID                ;// ; Board ID

} BITInternalUseOnlyV2;

// Memory Information table
// file: sw/main/bios/core88/core/pmu/devinit2.h

#define PLL_INFO_4X_VERSION 0x40
#define PLL_INFO_5X_VERSION 0x50

// PLL Information table
// file: sw/main/bios/cor88/inc/pllinfodef.inc
// wiki:  4.0: https://wiki.lwpu.com/engwiki/index.php/Resman/PLLs/PLL_Information_Table_4.0_Specification
//        5.0 (volta) https://wiki.lwpu.com/engwiki/index.php/Resman/PLLs/PLL_Information_Table_5.0_Specification#PLL_Information_Table_Header

typedef struct __attribute__((__packed__)) PLLInfo5xHeader
{
        db Version;     // PLL_INFO_5X_VERSION           ; PLL Information Table Version
        db HeaderSize;  // ; Size of PLL Information Table Header in bytes
        db EntrySize;   // ; Size of PLL Information Table Entry in bytes
        db EntryCount;  // ; Number of PLL Information Table Entries
} PLLInfo5xHeader;

typedef struct __attribute__((__packed__)) PLLInfo5xEntry
{
        db PLLID;        // ; PLL Identifier
        dw ReferenceMin; // ; Minimum Reference Clock (ClockIn) Frequency (MHz)
        dw ReferenceMax; // ; Maximum Reference Clock (ClockIn) Frequency (MHz)
        dw VCOMin;       // ; Minimum Voltage-Controlled Oscillator (VCO) Frequency (MHz)
        dw VCOMax;       // ; Maximum Voltage-Controlled Oscillator (VCO) Frequency (MHz)
        dw URMin;        // ; Minimum Update Rate (ClockIn/M) (MHz)
        dw URMax;        // ; Maximum Update Rate (ClockIn/M) (MHz)
        db MMin;         // ; Minimum Reference Clock Divider (M) coefficient
        db MMax;         // ; Maximum Reference Clock Divider (M) coefficient
        db NMin;         // ; Minimum VCO Feedback Divider (N) coefficient
        db NMax;         // ; Maximum VCO Feedback Divider (N) coefficient
        db PLMin;        // ; Minimum Linear Post Divider (PL) coefficient
        db PLMax;        // ; Maximum Linear Post Divider (PL) coefficient
} PLLInfo5xEntry;


//------------------------------------------------------------------------------------------------
// Memory Info Table
// origin: http://rmopengrok.lwpu.com/source/xref/sw/main/bios/core86/inc/meminfodef.inc#126
// field definitions: https://lwgrok-02:8621/resman/xref/gfw/sw/main/bios/core96/etc/genmeminfo.pl

// Fixed size of number of entries in memory info table (type will be skip entry if not in use)
#define MEMINFO_ENTRY_COUNT  16

// MemInfoHeader.Version
#define MEMINFO_VERSION                     0x010

// MemInfoHeader.Flags.MIFEntLUMeth
#define MEMINFO_FLAGS_ENT_LU_METH_STRAP     0x0
#define MEMINFO_FLAGS_ENT_LU_METH_HYBRIDA   0x1
#define MEMINFO_FLAGS_ENT_LU_METH_HYBRIDB   0x2
#define MEMINFO_FLAGS_ENT_LU_METH_MEMORYID  0x3

// MemInfoHeader.Flags.MIFBusType
#define MEMINFO_FLAGS_BUSTYPE_NATIVE        0x0
#define MEMINFO_FLAGS_BUSTYPE_MS01          0x1

// MemInfoEntry.mie1500.MIEType
// https://lwgrok-02:8621/resman/xref/gfw/sw/main/bios/core96/etc/genmeminfo.pl
#define  MEMINFO_E_TYPE_DDR2         0x0000
#define  MEMINFO_E_TYPE_DDR3         0x0001
#define  MEMINFO_E_TYPE_GDDR3        0x0002
#define  MEMINFO_E_TYPE_GDDR5        0x0003
#define  MEMINFO_E_TYPE_DDR4         0x0004
#define  MEMINFO_E_TYPE_HBM1         0x0005
#define  MEMINFO_E_TYPE_HBM2         0x0006
#define  MEMINFO_E_TYPE_LPDDR4       0x0007
#define  MEMINFO_E_TYPE_GDDR5x       0x0008
#define  MEMINFO_E_TYPE_GDDR6        0x0009
#define  MEMINFO_E_TYPE_GDDR6x       0x000A
#define  MEMINFO_E_TYPE_RESERVED     0x000B
#define  MEMINFO_E_TYPE_HBM3         0x000C
#define  MEMINFO_E_TYPE_GDDR4        0x000E
#define  MEMINFO_E_TYPE_SKIP         0x000F

// MemInfoEntry.mie1500.MIEVendorID
#define MEMINFO_ENTRY_VENDORID_RSVD0        0x00
#define MEMINFO_ENTRY_VENDORID_SAMSUNG      0x01
#define MEMINFO_ENTRY_VENDORID_QIMONDA      0x02
#define MEMINFO_ENTRY_VENDORID_ELPIDA       0x03
#define MEMINFO_ENTRY_VENDORID_ETRON        0x04
#define MEMINFO_ENTRY_VENDORID_NANYA        0x05
#define MEMINFO_ENTRY_VENDORID_HYNIX        0x06
#define MEMINFO_ENTRY_VENDORID_MOSEL        0x07
#define MEMINFO_ENTRY_VENDORID_WINBOND      0x08
#define MEMINFO_ENTRY_VENDORID_ESMT         0x09
#define MEMINFO_ENTRY_VENDORID_RSVDA        0x0A
#define MEMINFO_ENTRY_VENDORID_RSVDB        0x0B
#define MEMINFO_ENTRY_VENDORID_RSVDC        0x0C
#define MEMINFO_ENTRY_VENDORID_RSVDD        0x0D
#define MEMINFO_ENTRY_VENDORID_RSVDE        0x0E
#define MEMINFO_ENTRY_VENDORID_MICRON       0x0F

// MemInfoEntry.mie1500.MIEHBMVendorID              [15:12]
// as an alias to MemInfoEntry.mie1500.MIEVendorID
// This follows the JEDEC HBM Spec definition of the Manufacturer ID field of the IEEE1500 Device ID Wrapper Data Register
#define  MEMINFO_E_HBM_VENDID_SAMSUNG            0x01
#define  MEMINFO_E_HBM_VENDID_HYNIX              0x06
#define  MEMINFO_E_HBM_VENDID_MICRON             0x0F

// MemInfoEntry.mie3116.MIEDensity
#define MEMINFO_ENTRY_DENSITY_256M              0x00
#define MEMINFO_ENTRY_DENSITY_512M              0x01
#define MEMINFO_ENTRY_DENSITY_1G                0x02
#define MEMINFO_ENTRY_DENSITY_2G                0x03
#define MEMINFO_ENTRY_DENSITY_4G                0x04
#define MEMINFO_ENTRY_DENSITY_8G                0x05
#define MEMINFO_ENTRY_DENSITY_16G               0x06

#define MEMINFO_E_HBM_DENS_1G_4H                0x01  // 1Gb 4-High
#define MEMINFO_E_HBM_DENS_2G_4H                0x02  // 2Gb 4-High
#define MEMINFO_E_HBM_DENS_4G_4H                0x03  // 4Gb 4-High
#define MEMINFO_E_HBM_DENS_8G_8H                0x04  // 8Gb 8-High
#define MEMINFO_E_HBM_DENS_6G_4H                0x05  // 6Gb 4-High
#define MEMINFO_E_HBM_DENS_8G_4H                0x06  // 8Gb 4-High
#define MEMINFO_E_HBM_DENS_12G_8H               0x08  // 12Gb 8-High
#define MEMINFO_E_HBM_DENS_16G_8H               0x0A  // 16Gb 8-High

#define MEMINFO_E_HBM3_DENS_2GB        0x00
#define MEMINFO_E_HBM3_DENS_8GB_8H     0x01   //  4 Gb (8Gb 8-High)
#define MEMINFO_E_HBM3_DENS_8GB_12H    0x02   //  6 Gb (8Gb 12-High)
#define MEMINFO_E_HBM3_DENS_8GB_16H    0x03   //  8 Gb (8Gb 16-High)
#define MEMINFO_E_HBM3_DENS_4GB        0x04   //  4 Gb
#define MEMINFO_E_HBM3_DENS_16GB_8H    0x05   //  8 Gb (16Gb 8-High)
#define MEMINFO_E_HBM3_DENS_16GB_12H   0x06   // 12 Gb (16Gb 12-High)
#define MEMINFO_E_HBM3_DENS_16GB_16H   0x07   // 16 Gb (16Gb 16-High)
#define MEMINFO_E_HBM3_DENS_6GB        0x08   //  6 Gb
#define MEMINFO_E_HBM3_DENS_24GB_8H    0x09   // 12 Gb (24Gb 8-High)
#define MEMINFO_E_HBM3_DENS_24GB_12H   0x0A   // 18 Gb (24Gb 12-High)
#define MEMINFO_E_HBM3_DENS_24GB_16H   0x0B   // 24 Gb (24Gb 16-High)
#define MEMINFO_E_HBM3_DENS_8GB        0x0C   //  8 Gb
#define MEMINFO_E_HBM3_DENS_32GB_8H    0x0D   // 16 Gb (32Gb 8-High)
#define MEMINFO_E_HBM3_DENS_32GB_12H   0x0E   // 24 Gb (32Gb 12-High)
#define MEMINFO_E_HBM3_DENS_32GB_16H   0x0F   // 32 Gb (32Gb 16-High)

// Field definition for memory vendor id subfields
// these are for parsing data from the memory and dont correspond directly
// to table fields
#define MEMINFO_E_HBM_VENDID_MANUFACTURING_YEAR_2021  0x1

// MemInfoEntry.mie3116.MIEOrganization
#define MEMINFO_ENTRY_ORGANIZATION_X4           0x00
#define MEMINFO_ENTRY_ORGANIZATION_X8           0x01
#define MEMINFO_ENTRY_ORGANIZATION_X16          0x02
#define MEMINFO_ENTRY_ORGANIZATION_X32          0x03

// MemInfoEntry.mie3116.MIEWorkaround0
#define MEMINFO_ENTRY_WORKAROUND0_DISABLE       0x00
#define MEMINFO_ENTRY_WORKAROUND0_ENABLE        0x01

// MemInfoEntry.mie3116.MIEWFeature0
#define MEMINFO_ENTRY_FEATURE0_DISABLE          0x00
#define MEMINFO_ENTRY_FEATURE0_ENABLE           0x01
#define MEMINFO_ENTRY_LP3_MODE_DISABLE          0x00
#define MEMINFO_ENTRY_LP3_MODE_ENABLE           0x01

// MemInfoEntry.mie3116.MIEWorkaround1
#define MEMINFO_ENTRY_WORKAROUND1_DISABLE       0x00
#define MEMINFO_ENTRY_WORKAROUND1_ENABLE        0x01

// MemInfoEntry.mie3116.MIEWorkaround2
#define MEMINFO_ENTRY_WORKAROUND2_DISABLE       0x00
#define MEMINFO_ENTRY_WORKAROUND2_ENABLE        0x01

// MemInfoEntry.mie5540.MIEGddr5EdcReplay
#define MEMINFO_ENTRY_GDDR5_EDC_REPLAY_DISABLE  0x00
#define MEMINFO_ENTRY_GDDR5_EDC_REPLAY_ENABLE   0x01

// MemInfoEntry.mie5540.MIEGddr5Edc
#define MEMINFO_ENTRY_GDDR5_EDC_DISABLE         0x00
#define MEMINFO_ENTRY_GDDR5_EDC_ENABLE          0x01

//*****************************************************************************

// Memory ID Deswizzle Table
#define MEMINFO_MEMIDDESWIZZLE_BYTE_COUNT      32

//PerfMemclk strap entry, flags 3
#define MEMCLK1_STRAPENTRY_TRAINING_DLY_DEVICE_TRIM  0
#define MEMCLK1_STRAPENTRY_TRAINING_DLY_DEVICE_DLL   1
#define MEMCLK1_STRAPENTRY_TRAINING_DLY_DEVICE_INTER 2

//*****************************************************************************
//   DATA STRUCTURES
//*****************************************************************************

#define MEM_INFO_HEADER_FLAGS_MIFEntLUMeth 2:0
#define MEM_INFO_HEADER_FLAGS_MIFBusType   5:3
#define MEM_INFO_HEADER_FLAGS_MIFRsvd6     7:6

typedef struct __attribute__((__packed__)) MemInfoFlags
{
    LwU32 MIFEntLUMeth:   3;                       // [2:0] = Entry Look-Up Method
    LwU32 MIFBusType:     3;                       // [5:3] = Bus Type
    LwU32 MIFRsvd6:       2;                       // [7:6] = Reserved
} MemInfoFlags;

typedef struct __attribute__((__packed__)) MemInfoHeader
{
    db Version;       // MEMINFO_VERSION             ; Memory Information Table Version
    db HeaderSize;    // sizeof(MemInfoHeader)       ; Size of Memory Information Table Header in bytes
    db EntrySize;     // sizeof(MemInfoEntry)        ; Size of Memory Information Table Entry in bytes
    db EntryCount;    //                             ; Number of Memory Information Table Entries
    MemInfoFlags Flags;          //                             ; Memory Information Table Flags
    dw Deswizzle;
} MemInfoHeader;

#define MEMINFOENTRY_MIE1500_MIEType      3:0
#define MEMINFOENTRY_MIE1500_MIEStrap     7:4
#define MEMINFOENTRY_MIE1500_MIEIndex    11:8
#define MEMINFOENTRY_MIE1500_MIEVendorID 15:12

typedef struct __attribute__((__packed__)) MemInfoEntry1500
{
    LwU32 MIEType:        4;
    LwU32 MIEStrap:       4;        // Strap
    LwU32 MIEIndex:       4;        // Index
    LwU32 MIEVendorID:    4;        // Vendor ID
} MemInfoEntry1500;

#define MEMINFOENTRY_MIE3116_MIERevisionID     3:0
#define MEMINFOENTRY_MIE3116_MIEDensity        7:4
#define MEMINFOENTRY_MIE3116_MIEOrganization  10:8
#define MEMINFOENTRY_MIE3116_MIEWorkaround0   11:11
#define MEMINFOENTRY_MIE3116_MIEFeature0      12:12
#define MEMINFOENTRY_MIE3116_MIEWorkaround1   13:13
#define MEMINFOENTRY_MIE3116_MIEWorkaround2   14:14
#define MEMINFOENTRY_MIE3116_MIEWorkaround3   15:15

typedef struct __attribute__((__packed__)) MemInfoEntry3116
{
    LwU8 MIERevisionID:   4;       // Memory Revision Identification (Die Revision)
    LwU8 MIEDensity:      4;       // Memory Density
    LwU8 MIEOrganization: 3;       // Memory Organization
    LwU8 MIEWorkaround0:  1;       // Generic Workaround 0
    LwU8 MIEFeature0:     1;       // Generic Feature 0 LP3 mode
    LwU8 MIEWorkaround1:  1;       // Generic Workaround 1
    LwU8 MIEWorkaround2:  1;       // Generic Workaround 2
    LwU8 MIEWorkaround3:  1;       // Generic Workaround 3
} MemInfoEntry3116;

#define MEMINFOENTRY_MIE3932_MIEPartitionGroup  3:0
#define MEMINFOENTRY_MIE3932_MIEStrapBit4       4:4
#define MEMINFOENTRY_MIE3932_MIERsvd37          5:5
#define MEMINFOENTRY_MIE3932_MIEIndexBit4       6:6
#define MEMINFOENTRY_MIE3932_MIERsvd39          7:7

typedef struct __attribute__((__packed__)) MemInfoEntry3932
{
    LwU8 MIEPartitionGroup:  4;     // Partition Group
    LwU8 MIEStrapBit4:       1;     // Bit[4:4] of straps
    LwU8 MIERsvd37:          1;     // Reserved for future expansion of straps(bit 37) workaround 5
    LwU8 MIEIndexBit4:       1;     // Bit[4:4] of Index
    LwU8 MIERsvd39:          1;     // Reserved for future expansion of index(bit 39)
} MemInfoEntry3932;

#define MEMINFOENTRY_MIE5540_MIEMclkBGEnableFreq   12:0
#define MEMINFOENTRY_MIE5540_MIEWorkaround4        13:13
#define MEMINFOENTRY_MIE5540_MIEGddr5Edc           14:14
#define MEMINFOENTRY_MIE5540_MIEGddr5EdcReplay     15:15

typedef struct __attribute__((__packed__)) MemInfoEntry5540
{
    LwU16 MIEMclkBGEnableFreq: 13;   // MCLK Bank Grouping Enable Frequency
    LwU16 MIEWorkaround4:      1;    // Generic Workaround 4
    LwU16 MIEGddr5Edc:         1;    // EDC
    LwU16 MIEGddr5EdcReplay:   1;    // EDC Replay
} MemInfoEntry5540;

#define MEMINFOENTRY_MIE6536_MIEHbmStackHeight      0:0
#define MEMINFOENTRY_MIE6536_MIEHbmModelPartNumMask 7:1

typedef struct __attribute__((__packed__)) MemInfoEntry6356
{
    LwU8 MIEHbmStackHeight:      1;   // HBM stack height
    LwU8 MIEHbmModelPartNumMask: 7;   // HBM Model part number mask
} MemInfoEntry6356;

/* bios definition is using a straight array declaration so this definition
   is not needed.
typedef struct __attribute__((__packed__)) MemInfoEntry8764
{
    LwU8 mieTmrsPhase0Index:     8;   // Index into TMRS Sequence List Table for meminit phase 0
    LwU8 mieTmrsPhase1Index:     8;   // Index into TMRS Sequence List Table for meminit phase 1
    LwU8 mieTmrsPhase2Index:     8;   // Index into TMRS Sequence List Table for meminit phase 2
} MemInfoEntry8764;
 */

#define MEMINFO_ENTRY_TMRS_NUM_PHASES 3

typedef struct __attribute__((__packed__)) MemInfoEntry9588
{
    LwU8 MIEMaxTjLimit:          8;   // Maximum Tj limit (in C)
} MemInfoEntry9588;

typedef struct __attribute__((__packed__)) MemInfoEntry
{
    MemInfoEntry1500 mie1500;
    MemInfoEntry3116 mie3116;
    MemInfoEntry3932 mie3932;
    MemInfoEntry5540 mie5540;
    MemInfoEntry6356 mie6536;
} MemInfoEntry;

typedef struct __attribute__((__packed__)) MemInfoDeswizzle
{
    db deswizzle[32];
} MemInfoDeswizzle;


//------------------------------------------------------------------------------------------------
// Memory Partition Info Table
// origin: http://rmopengrok.lwpu.com/source/xref/sw/main/bios/core86/inc/mempartitioninfodef.inc#36
//

#define MEMPARTITIONINFO_VERSION 0x22  /* Todo: find proper definition */

typedef struct __attribute__((__packed__)) MemPartitionInfoHeader {
    db Version;        // MEMPARTITIONINFO_VERSION        ; Memory Partition Information Table Version
    db HeaderSize;     // sizeof(MemPartitionInfoHeader)  ; Size of Memory Partition Information Table Header in bytes
    db EntrySize;      // sizeof(MemPartitionInfoEntry)   ; Size of Memory Partition Information Table Entry in bytes
    db EntryCount;     //
} MemPartitionInfoHeader;

// MemPartitionInfoEntry.mpie1508.MPIEAccessGroup       [2:0]
#define  MEMPARTITIONINFO_E_ACCESSGROUP__SB              0
#define  MEMPARTITIONINFO_E_ACCESSGROUP__FW              3
#define  MEMPARTITIONINFO_E_ACCESSGROUP__IM             ~0x07
#define  MEMPARTITIONINFO_E_ACCESSGROUP__SF              0x01
#define  MEMPARTITIONINFO_E_ACCESSGROUP_MC_0             0x00
#define  MEMPARTITIONINFO_E_ACCESSGROUP_MC_1             0x01
#define  MEMPARTITIONINFO_E_ACCESSGROUP_MC_2             0x02
#define  MEMPARTITIONINFO_E_ACCESSGROUP_UNICAST          0x06
#define  MEMPARTITIONINFO_E_ACCESSGROUP_BCAST            0x07

enum MemPartitionAccessGroupEnum {
    access_multicast_0 = MEMPARTITIONINFO_E_ACCESSGROUP_MC_0,
    access_multicast_1 = MEMPARTITIONINFO_E_ACCESSGROUP_MC_1,
    access_multicast_2 = MEMPARTITIONINFO_E_ACCESSGROUP_MC_2,
    access_unicast =  MEMPARTITIONINFO_E_ACCESSGROUP_UNICAST,
    access_broadcast =  MEMPARTITIONINFO_E_ACCESSGROUP_BCAST
};


#define MEMPARTITIONINFOENTRY_MPIE0700_MPIEPartitionGroup     3:0
#define MEMPARTITIONINFOENTRY_MPIE0700_MPIEPhysicalPartition  7:4

typedef struct __attribute__((__packed__)) MemPartitionInfoEntry0700
{
    LwU32 MPIEPartitionGroup:     4;                         // [3:0] = Logical Partition Group
    LwU32 MPIEPhysicalPartition:  4;                         // [7:4] = Physical Partition
} MemPartitionInfoEntry0700;

#define MEMPARTITIONINFOENTRY_MPIE1508_MPIEAccessGroup        2:0
#define MEMPARTITIONINFOENTRY_MPIE1508_MPIEUsesHybridClocks   3:3
#define MEMPARTITIONINFOENTRY_MPIE1508_MPIERsvd4              7:4

typedef struct __attribute__((__packed__)) MemPartitionInfoEntry1508
{
    enum MemPartitionAccessGroupEnum MPIEAccessGroup: 3;  // [2:0] = Access Group
    LwU32 MPIEUsesHybridClocks:   1;                         // [3:3] = Uses Hybrid Clocks
    LwU32 MPIERsvd4:              4;                         // [7:4] = Reserved
} MemPartitionInfoEntry1508;

typedef struct __attribute__((__packed__)) MemPartitionInfoEntry
{
    MemPartitionInfoEntry0700 mpie0700;
    MemPartitionInfoEntry1508 mpie1508;
} MemPartitionInfoEntry;


//------------------------------------------------------------------------------------------------
// Memory Training Pattern Table
// origin: http://rmopengrok.lwpu.com/source/xref/sw/main/bios/core86/inc/memtraindef.inc#122
//


# define MEMTRAIN_VERSION 0x10
# define MEMTRAIN_ENTRY_PARAM_SKIP 0xee  /* Todo: find proper definition */
# define MEMTRAIN_ENTRY_PARAM_RDDLY                  0x00
# define MEMTRAIN_ENTRY_PARAM_WRDLY                  0x01
# define MEMTRAIN_ENTRY_PARAM_DATVREF                0x02
# define MEMTRAIN_ENTRY_PARAM_DQSVREF                0x03
# define MEMTRAIN_ENTRY_PARAM_ADDRDLY                0x04
# define MEMTRAIN_ENTRY_PARAM_QUSEDLY                0x05
# define MEMTRAIN_ENTRY_PARAM_RDDBI                  0x06
# define MEMTRAIN_ENTRY_PARAM_WRDBI                  0x07
# define MEMTRAIN_ENTRY_PARAM_RDEDC                  0x08
# define MEMTRAIN_ENTRY_PARAM_WREDC                  0x09

#define MEMTRAINHEADER_MEMORYCLOCK_MTMCFreq    13:0
#define MEMTRAINHEADER_MEMORYCLOCK_MTMCRsvd14  15:14

typedef struct __attribute__((__packed__)) MemTrainMemoryClock
{
    LwU32 MTMCFreq:   14;         // [13:0] = Frequency (MHz)
    LwU32 MTMCRsvd14:  2;         // [15:14] = Reserved
} MemTrainMemoryClock;

typedef struct __attribute__((__packed__)) MemTrainHeader
{
    db Version;                   // MEMTRAIN_VERSION            ; Memory Training Table Version
    db HeaderSize;                // sizeof(MemTrainHeader)      ; Size of Memory Training Table Header in bytes
    db BaseEntrySize;             // sizeof(MemTrainBaseEntry)   ; Size of Memory Training Table Base Entry in bytes
    db StrapEntrySize;            // sizeof(MemTrainStrapEntry)  ; Size of Memory Training Table Strap Entry in bytes
    db StrapEntryCount;           //                             ; Number of Memory Training Table Strap Entries per Memory Training Table Entry
    db EntryCount;                //                             ; Number of Memory Training Table Entries
    MemTrainMemoryClock MemoryClock;
} MemTrainHeader;

#define MEMTRAINBASEENTRY_MTBERsvd4   3:0
#define MEMTRAINBASEENTRY_MTBEParam   7:4

typedef struct __attribute__((__packed__)) MemTrainBaseEntry
{
    LwU32 MTBERsvd4:  4;                // Reserved
    LwU32 MTBEParam:  4;                // MEMTRAIN_ENTRY_PARAM_SKIP   ; Specifies the training parameter this entry defines
} MemTrainBaseEntry;

typedef struct __attribute__((__packed__)) MemTrainStrapEntry
{
    db PatternEntryIdx;              //  0FFh                       ; Index of an entry in the Memory Training Pattern Table
} MemTrainStrapEntry;

#define MEMTRAIN_PATTERN_10_VERSION 0x10
#define MEMTRAIN_PATTERN_11_VERSION 0x11

typedef struct __attribute__((__packed__)) MemTrainPattern10Header
{
    db Version;                 // MEMTRAIN_PATTERN_10_VERSION                  ; Memory Training Pattern Table Version
    db HeaderSize;              // sizeof(MemTrainPattern10Header)              ; Size of Memory Training Pattern Table Header in bytes
    db BaseEntrySize;           // sizeof(MemTrainPatternBaseEntry10)           ; Size of Memory Training Pattern Table Base Entry in bytes
    db PatternBufferSize;       // sizeof(MemTrainPatternEntry10.PatternBuffer) ; Size of Memory Training Pattern Buffer in each entry in bytes
    db EntryCount;              //                                              ; Number of Memory Training Pattern Table Entries
} MemTrainPattern10Header;

typedef struct __attribute__((__packed__)) MemTrainPattern11Header
{
    db Version;                 // MEMTRAIN_PATTERN_11_VERSION                  ; Memory Training Pattern Table Version
    db HeaderSize;              // sizeof(MemTrainPattern10Header)              ; Size of Memory Training Pattern Table Header in bytes
    db BaseEntrySize;           // sizeof(MemTrainPatternBaseEntry11)           ; Size of Memory Training Pattern Table Base Entry in bytes
    #if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
      db PatternBufferSize;       // sizeof(MemTrainPatternEntry11.PatternBuffer) ; Size of Memory Training Pattern Buffer in each entry in bytes
    #else
      dw PatternBufferSize;       // sizeof(MemTrainPatternEntry11.PatternBuffer) ; Ga10x pattern buffer increased to 256 - 2 bytes
    #endif

    db EntryCount;              //                                              ; Number of Memory Training Pattern Table Entries
} MemTrainPattern11Header;


union MemTrainPatternHeader {
    MemTrainPattern10Header MTPH10;
    MemTrainPattern11Header MTPH11;
};

#define MEMTRAINPATTERNSIZES_MTPSComp    5:0
#define MEMTRAINPATTERNSIZES_MTPSUncomp  7:6

typedef struct __attribute__((__packed__)) MemTrainPatternSizes
{
    LwU8 MTPSComp:   6;           // [5:0] = Compressed (bits)
    LwU8 MTPSUncomp: 2;           // [7:6] = Uncompressed
} MemTrainPatternSizes;                 // Sizes of uncompressed and compressed patterns

#define MEMTRAINPATTERN_FLAGS0_MTPF0DataType     2:0
#define MEMTRAINPATTERN_FLAGS0_MTPF0Rsvd3        3:3
#define MEMTRAINPATTERN_FLAGS0_MTPF0Replication  5:4
#define MEMTRAINPATTERN_FLAGS0_MTPF0Periodic     6:6
#define MEMTRAINPATTERN_FLAGS0_MTPF0Rsvd         7:7

typedef struct __attribute__((__packed__)) MemTrainPatternFlags0
{
    LwU8 MTPF0DataType:      3;   // [2:0] = Data Type
    LwU8 MTPF0Rsvd3:         1;   // [3:3] = Reserved
    LwU8 MTPF0Replication:   2;   // [5:4] = Replication
    LwU8 MTPF0Periodic:      1;   // [6:6] = Periodic
    LwU8 MTPF0Rsvd:          1;   // [7:7] = Reserved
} MemTrainPatternFlags0;

typedef struct __attribute__((__packed__)) MemTrainPatternBaseEntry10
{
   MemTrainPatternSizes PatternSizes;     // struct MemTrainPatternSizes
    db PatternCount;                      // Number of valid patterns in the Pattern Buffer
    MemTrainPatternFlags0 Flags0;         // struct MemTrainPatternFlags0   ; Memory Training Pattern Table Entry Flags Byte 0
    db PatternEntryIdx;                   //  0FFh        ; Index of an entry in the Memory Training Pattern Table
} MemTrainPatternBaseEntry10;

typedef struct __attribute__((__packed__)) MemTrainPatternEntry10
{
    MemTrainPatternBaseEntry10 BaseEntry;
    db PatternBuffer[96];               // MEMTRAIN_PATTERN_BUFFER_SIZE dup( 00h )
} MemTrainPatternEntry10;

typedef struct __attribute__((__packed__)) MemTrainPatternBaseEntry11
{
    MemTrainPatternSizes PatternSizes;      // struct MemTrainPatternSizes Sizes of uncompressed and compressed patterns
    dw PatternCount;                        // Number of valid patterns in the Pattern Buffer
    MemTrainPatternFlags0 Flags0;           //  struct MemTrainPatternFlags0             ; Memory Training Pattern Table Entry Flags Byte 0
    db PatternEntryIdx;                     //  0FFh        ; Index of an entry in the Memory Training Pattern Table
} MemTrainPatternBaseEntry11;

typedef struct __attribute__((__packed__)) MemTrainPatternEntry11
{
    MemTrainPatternBaseEntry11 BaseEntry;
    #if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
      db PatternBuffer;                // MEMTRAIN_PATTERN_BUFFER_SIZE dup( 00h )
    #else
      db PatternBuffer[256];                // MEMTRAIN_PATTERN_BUFFER_SIZE dup( 00h )
    #endif
} MemTrainPatternEntry11;

//------------------------------------------------------------------------------------------------
// Performance Table
// origin: https://rmopengrok.lwpu.com/source/xref/sw/main/bios/core90/inc/perfdef.inc
//

#define PERF_VERSION_5X 0x50
#define PERF_VERSION_6X 0x60

// ;PStateMClkFreqBaseEntry.State
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P15       0x0
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P14       0x1
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P13       0x2
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P12       0x3
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P11       0x4
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P10       0x5
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P9        0x6
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P8        0x7
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P7        0x8
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P6        0x9
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P5        0xA
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P4        0xB
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P3        0xC
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P2        0xD
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P1        0xE
#define PSTATE_MCLK_FREQ_ENTRY_STATE_P0        0xF
#define PSTATE_MCLK_FREQ_ENTRY_STATE_SKIP      0xFF


#define PERF5XHEADER_FLAGS0_PH5xF0PstateReq 0:0
#define PERF5XHEADER_FLAGS0_PH5xF0Rsvd7     7:1

typedef struct __attribute__((__packed__)) Perf5xHeaderFlags0
{
    LwU8 PH5xF0PstateReq:      1;                       // [0:0] = P-states Requirement
    LwU8 PH5xF0Rsvd7:          7;                       // [7:1] = Reserved
} Perf5xHeaderFlags0;

typedef struct __attribute__((__packed__)) Perf5xHeader
{
    db Version;             // PERF_VERSION_5X             ; Performance Table Version
    db HeaderSize;          // sizeof Perf5xHeader         ; Size of Performance Table Header
    db BaseEntrySize;       // sizeof Perf5xBaseEntry      ; Size of Performance Table Base Entry
    db BaseEntryCount;      //                             ; Size of Performance Table Base Entry
    db ClockEntrySize;      // sizeof Perf5xClockEntry     ; Size of Performance Table Clock Entry
    db ClockEntryCount;     //                             ; Number of Performance Table Clock Entries per Performance Table Entry
    Perf5xHeaderFlags0 Flags0;      // struct Perf5xHeaderFlags0
    db InitState;           // PERF_ENTRY_STATE_SKIP       ; Initialization State
    db CPISupportLevel;     //                             ; Copy-perf-to-init (CPI) Support Level
    db CPIFeatures;         //                             ; Copy-perf-to-init (CPI) Features
} Perf5xHeader;


#define PERF5XBASEENTRY_FLAGS0_PBEF0Capping  1:0
#define PERF5XBASEENTRY_FLAGS0_PBEF0LWDA     2:2
#define PERF5XBASEENTRY_FLAGS0_PBEF0OCOV     3:3
#define PERF5XBASEENTRY_FLAGS0_PBEF0SkpSt    4:4
#define PERF5XBASEENTRY_FLAGS0_PBEF0Rsvd5    7:5

typedef struct __attribute__((__packed__)) Perf5xBaseEntryFlags0
{
    LwU8 PBEF0Capping: 2;                               // [1:0] = GFA Exclusion
    LwU8 PBEF0LWDA:    1;                               // [2:2] = Canoas Mode Exclusion
    LwU8 PBEF0OCOV:    1;                               // [3:3] = Non-SLI External Control Exclusion
    LwU8 PBEF0SkpSt:   1;                               // [4:4] = Skip this state from FB utilization callwlation.
    LwU8 PBEF0Rsvd5:   3;                               // [7:5] = Reserved
} Perf5xBaseEntryFlags0;

typedef struct __attribute__((__packed__)) Perf5xBaseEntry
{
    db State;                          // PERF_ENTRY_STATE_SKIP        ; Performance State Identifier
    Perf5xBaseEntryFlags0 Flags0;      // struct Perf5xBaseEntryFlags0                         ; Performance Table Base Entry Flags Byte 0
    db LpwrEntryIdx;                   // ; Index to LPWR Entry Index table
} Perf5xBaseEntry;


#define PERF5XCLOCKENTRY_PARAM0_PCEP0Nominal 13:0
#define PERF5XCLOCKENTRY_PARAM0_PCEF0Rsvd15  15:14

typedef struct __attribute__((__packed__)) Perf5xClockEntryParam0
{
    dw PCEP0Nominal: 14;                             // [13:0] = Source Clock Domain
    dw PCEF0Rsvd15:   2;                              // [15:14] = Reserved
} Perf5xClockEntryParam0;

#define PERF5XCLOCKENTRY_PARAM1_PCEP1Freqmin  13:0
#define PERF5XCLOCKENTRY_PARAM1_PCEP1Freqmax  27:14
#define PERF5XCLOCKENTRY_PARAM1_PCEF1Rsvd31   31:28

typedef struct __attribute__((__packed__)) Perf5xClockEntryParam1
{
    LwU16 PCEP1Freqmin: 14;                         // [13:0] = Frequency Min
    LwU16 PCEP1Freqmax: 14;                         // [27:14] = Frequency Max
    LwU16 PCEF1Rsvd31:   4;                          // [31:28] = Reserved
} Perf5xClockEntryParam1;

typedef struct __attribute__((__packed__)) Perf5xClockEntry
{                                      // [13:0] = Source Clock Domain
    Perf5xClockEntryParam0 Param0;     //  ; Path setup information
    Perf5xClockEntryParam1 Param1;     //  ; Performance Table Clock Entry Flags Byte 0
} Perf5xClockEntry;


#define PERF6XHEATER_PSTATE_ENABLED 0x0
#define PERF6XHEATER_PSTATE_DISABLED 0x1

typedef struct __attribute__((__packed__)) Perf6xHeaderFlags0
{
    LwU8 PH6xF0PstateReq:       1;                       // [0:0] = P-states Requirement
    LwU8 PH6xF0ArbitrationLock: 1;
    LwU8 PH6xF0Rsvd7:           6;                       // [7:1] = Reserved
} Perf6xHeaderFlags0;

typedef struct __attribute__((__packed__)) Perf6xHeader
{
    db Version;             // PERF_VERSION_5X             ; Performance Table Version
    db HeaderSize;          // sizeof Perf5xHeader         ; Size of Performance Table Header
    db BaseEntrySize;       // sizeof Perf5xBaseEntry      ; Size of Performance Table Base Entry
    db BaseEntryCount;      //                             ; Size of Performance Table Base Entry
    db ClockEntrySize;      // sizeof Perf5xClockEntry     ; Size of Performance Table Clock Entry
    db ClockEntryCount;     //                             ; Number of Performance Table Clock Entries per Performance Table Entry
    Perf6xHeaderFlags0 Flags0;               //            ; Performance Table Flags Byte 0
    db InitState;           // PERF_ENTRY_STATE_SKIP       ; Initialization State
    db CPISupportLevel;     //                             ; Copy-perf-to-init (CPI) Support Level
    db CPIFeatures;         //                             ; Copy-perf-to-init (CPI) Features
} Perf6xHeader;

typedef struct __attribute__((__packed__)) Perf6xBaseEntryFlags0
{
    LwU8 PBEF0Rsvd5:   3;                               // [7:5] = Reserved
    LwU8 PBEF0SkpSt:   1;                               // [4:4] = Skip this state from FB utilization callwlation.
    LwU8 PBEF0OCOV:    1;                               // [3:3] = Non-SLI External Control Exclusion
    LwU8 PBEF0LWDA:    1;                               // [2:2] = Canoas Mode Exclusion
    LwU8 PBEF0Capping: 2;                               // [1:0] = GFA Exclusion
} Perf6xBaseEntryFlags0;

typedef struct __attribute__((__packed__)) Perf6xBaseEntry
{
    db State;          // PERF_ENTRY_STATE_SKIP        ; Performance State Identifier
    db Flags0;         // struct Perf5xBaseEntryFlags0                         ; Performance Table Base Entry Flags Byte 0
    Perf6xBaseEntryFlags0 LpwrEntryIdx;   //                              ; Index to LPWR Entry Index table
    db PCIeEntryIdx;
    db LWLinkEntryIdx;
} Perf6xBaseEntry;

typedef struct __attribute__((__packed__)) Perf6xClockEntryParam0
{
    LwU16 PCEP0Nominal: 14;                             // [13:0] = Source Clock Domain
    LwU16 PCEF0Rsvd15:   2;                              // [15:14] = Reserved
} Perf6xClockEntryParam0;

typedef struct __attribute__((__packed__)) Perf6xClockEntryParam1
{
    LwU16 PCEP1Freqmin: 14;                         // [13:0] = Frequency Min
    LwU16 PCEP1Freqmax: 14;                         // [27:14] = Frequency Max
    LwU16 PCEF1Rsvd31:   4;                         // [31:28] = Reserved
} Perf6xClockEntryParam1;

typedef struct __attribute__((__packed__)) Perf6xClockEntry
{                                      // [13:0] = Source Clock Domain
    Perf6xClockEntryParam0 Param0;     // struct Perf5xClockEntryParam0   // ; Path setup information
    Perf6xClockEntryParam1 Param1;     // struct Perf5xClockEntryParam1   // ; Performance Table Clock Entry Flags Byte 0
} Perf6xClockEntry;


union PerfBaseEntry {
    Perf5xBaseEntry PB5x;
    Perf6xBaseEntry PB6x;
};


//------------------------------------------------------------------------------------------------
// P-State memory clock frequency table
// origin: http://rmopengrok.lwpu.com/source/xref/sw/main/bios/core86/inc/perfdef.inc#798
//

#define PSTATE_MCLK_FREQ_10_VERSION 0x44 /* Todo: find proper definition */

typedef struct __attribute__((__packed__)) PerfPStateMemClkHeader
{
    db Version;            // PSTATE_MCLK_FREQ_10_VERSION         ; P-State Memory Clock Frequency Table Version 1.0
    db HeaderSize;         // sizeof PerfPStateMemClkHeader       ; Size of P-State Memory Clock Frequency Table Header
    db BaseEntrySize;      // sizeof PStateMClkFreq10BaseEntry    ; Size of P-State Memory Clock Frequency Table Base Entry
    db StrapEntrySize;     // sizeof PStateMClkFreq10StrapEntry   ; Size of P-State Memory Clock Frequency Table Clock Entry
    db StrapEntryCount;    //                                     ; Number of P-State Memory Clock Frequency Table Clock Entries
                           //                                     ; per P-State Memory Clock Frequency Table Entry
    db EntryCount;         //                                     ; Number of P-State Memory Clock Frequency Table Entries
} PerfPStateMemClkHeader;

typedef struct __attribute__((__packed__)) PSMCLKFTBaseEntry
{
    LwU16 PSMCLKFRKey;
} PSMCLKFTBaseEntry;

typedef struct __attribute__((__packed__)) PStateMClkFreq10BaseEntry
{
    dw Key;                // PSMCLKFTBaseEntry
} PStateMClkFreq10BaseEntry;

typedef struct __attribute__((__packed__)) PSMCLKFTStrapEntry
{
    LwU16 PSMCLKFTFrequency;
} PSMCLKFTStrapEntry;

typedef struct __attribute__((__packed__)) PStateMClkFreq10StrapEntry
{
    dw Frequency;          // PSMCLKFTStrapEntry
} PStateMClkFreq10StrapEntry;


//------------------------------------------------------------------------------------------------
// Performance Memory Clock Table
// origin: http://rmopengrok.lwpu.com/source/xref/sw/main/bios/core86/inc/perfdef.inc#830
//

#define PERF_MCLK_11_VERSION 0x11  /* Todo: find proper definition */

//------------------------------------------------------------------------------------------------------------------
// PerfMemClk Header Entry - Sub Struct definition
//------------------------------------------------------------------------------------------------------------------

#define PERFMEMCLK11HEADER_FLAGS0_PMCHF0RxDfeCtrl          0:0
#define PERFMEMCLK11HEADER_FLAGS0_PMCHF0DRAMPLLPowerOFF    1:1
#define PERFMEMCLK11HEADER_FLAGS0_PMCHF0ClkCalUpdate       2:2
#define PERFMEMCLK11HEADER_FLAGS0_PMCHF0Rsvd1              7:3

typedef struct  __attribute__((__packed__)) PerfMemClkHeaderFlags0
{
    LwU8 PMCHF0RxDfeCtrl:       1;      // [0:0] = RX Decision Feedback Equalization Control
    LwU8 PMCHF0DRAMPLLPowerOFF: 1;      // [1:1] = Power Off DRAMPLL When Unused
    LwU8 PMCHF0ClkCalUpdate:    1;      // [2:2] = Clock Cal Update
    LwU8 PMCHF0Rsvd1:           5;      // [7:3] = Reserved
} PerfMemClkHeaderFlags0;

typedef struct __attribute__((__packed__)) PerfMemClk11Header
{
    db Version;             // PERF_MCLK_11_VERSION          ; Memory Clock Table Version 1.1
    db HeaderSize;          // sizeof(PerfMemClk11Header)    ; Size of Memory Clock Table Header
    db BaseEntrySize;       // sizeof(PerfMemClk11BaseEntry) ; Size of Memory Clock Table Base Entry
    db StrapEntrySize;      // sizeof PerfMemClk11StrapEntry ; Size of Memory Clock Table Clock Entry
    db StrapEntryCount;     //                               ; Number of Memory Clock Table Clock Entries per Memory Clock Table Entry
    db EntryCount;          //                               ; Number of Memory Clock Table Entries
    PerfMemClkHeaderFlags0 pmchf0;
    db FBVDDSettleTime;     //                               ; Time to delay after switching FBVDD/Q
    dd CFG_PWRD;            //                               ; CFG_PWRD Dword value for FBVDD/Q WAR
    dw FBVDDQHigh;          //                               ; High voltage for FBVDD/Q
    dw FBVDDQLow;           //                               ; Low voltage for FBVDD/Q
    dd ScriptListPtr;       //                               ; Pointer to a list of memory script pointers.
    db ScriptListCount;     //                               ; Number of memory script pointers in the ScriptList
    dd CmdScriptListPtr;    //                               ; Pointer to a list of pointers to devinit scripts used for G5X mode
    db CmdScriptListCount;  //                               ; Number of 32-bit devinit script pointers in the list from Cmd Script List
    dw PerfMClkTableGPIOIndex; // ; Indices into GPIO Assignment Table for FB GPIO functions of interest [ Bug 2051282 ]
                               // ;   [7:0]: FBVDDQ_SELECT_function,  [15:0] FBVREF_SELECT_function
} PerfMemClk11Header;


// starting with Turing we added additional entries for the GPIO seleciton to the Perf Mem Clk Table
#define SIZE_OF_PERF_MEM_CLK11HEADER_GV100 26
#define SIZE_OF_PERF_MEM_CLK11HEADER_TU10X sizeof(PerfMemClk11Header)



//------------------------------------------------------------------------------------------------------------------
// PerfMemClk Base Entry - Sub Struct definition
//------------------------------------------------------------------------------------------------------------------

typedef struct __attribute__((__packed__)) PerfMemClkBaseEntryMinMax
{
    LwU16 PMCBEMMFreq:   14;               // [13:0] = Frequency
    LwU16 PMCBEMMRsvd14:  2;               // [15:14] = Reserved
} PerfMemClkBaseEntryMinMax;

//------------------------------------------------------------------------------------------------------------------
// PerfMemClk Header Entry Definition
//------------------------------------------------------------------------------------------------------------------

#define PERFMEMCLK11ENTRY_FLAGS0_PMC11EF0ACPD           0:0
#define PERFMEMCLK11ENTRY_FLAGS0_PMC11EF0GearShft       1:1
#define PERFMEMCLK11ENTRY_FLAGS0_PMC11EF0ExtQuse        3:2
#define PERFMEMCLK11ENTRY_FLAGS0_PMC11EF0WriteTraining  4:4
#define PERFMEMCLK11ENTRY_FLAGS0_PMC11EF0Rsvd5          5:5
#define PERFMEMCLK11ENTRY_FLAGS0_PMC11EF0SDM            6:6
#define PERFMEMCLK11ENTRY_FLAGS0_PMC11EF0DelayComp      7:7

typedef struct __attribute__((__packed__)) PerfMemClk11EntryFlags0
{
    LwU8 PMC11EF0ACPD:           1;        // [0:0] = ACPD (DRAM Active Powerdown)
    LwU8 PMC11EF0GearShft:       1;        // [1:1] = Gear Shift
    LwU8 PMC11EF0ExtQuse:        2;        // [3:2] = Extended QUSE
    LwU8 PMC11EF0WriteTraining:  1;        // [4:4] = Write Training
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    LwU8 PMC11EF0MTA:            1;        // [5:5] = MTA
#else
    LwU8 PMC11EF0Rsvd:           1;        // [5:5] = Reserved
#endif
    LwU8 PMC11EF0SDM:            1;        // [6:6] = SDM
    LwU8 PMC11EF0DelayComp:      1;        // [7:7] = Delay Compensation
} PerfMemClk11EntryFlags0;


//------------------------------------------------------------------------------------------------------------------
// PerfMemClk Base Entry Definition
//------------------------------------------------------------------------------------------------------------------


#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0MRIF    8:0
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0MWIF   17:9
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0RBH    18:18
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0WBH    19:19
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0MSRC   24:20
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0SWSOC  25:25
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0CWS    26:26
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0IBR    27:27
#define PERFMEMCLK11BASEENTRY_CONFIG0_PMCFBPAC0Rsvd28 31:28

typedef struct __attribute__((__packed__)) PerfMemClkFBPAConfig0
{
    LwU16 PMCFBPAC0MRIF:      9;            // [8:0] = Maximum Reads In Flight
    LwU16 PMCFBPAC0MWIF:      9;            // [17:9] = Maximum Writes In Flight
    LwU16 PMCFBPAC0RBH:       1;            // [18:18] = Read Bank Hit
    LwU16 PMCFBPAC0WBH:       1;            // [19:19] = Write Bank Hit
    LwU16 PMCFBPAC0MSRC:      5;            // [24:20] = Maximum Sorted Read Chain
    LwU16 PMCFBPAC0SWSOC:     1;            // [25:25] = Stop Write Sorting On Clean
    LwU16 PMCFBPAC0CWS:       1;            // [26:26] = Complete Write Snapshot
    LwU16 PMCFBPAC0IBR:       1;            // [27:27] = Iso Block Refreshes
    LwU16 PMCFBPAC0Rsvd28:    4;            // Reserved
} PerfMemClkFBPAConfig0;

#define PERFMEMCLK11BASEENTRY_CONFIG1_PMCFBPAC1RL      3:0
#define PERFMEMCLK11BASEENTRY_CONFIG1_PMCFBPAC1WL      7:4
#define PERFMEMCLK11BASEENTRY_CONFIG1_PMCFBPAC1MRPT   11:8
#define PERFMEMCLK11BASEENTRY_CONFIG1_PMCFBPAC1MWPT   15:12
#define PERFMEMCLK11BASEENTRY_CONFIG1_PMCFBPAC1RLO    19:16
#define PERFMEMCLK11BASEENTRY_CONFIG1_PMCFBPAC1WLO    23:20
#define PERFMEMCLK11BASEENTRY_CONFIG1_PMCFBPAC1LI     31:24

typedef struct __attribute__((__packed__)) PerfMemClkFBPAConfig1
{
    LwU8 PMCFBPAC1RL:        4;            // [3:0] = Read Limit
    LwU8 PMCFBPAC1WL:        4;            // [7:4] = Write Limit
    LwU8 PMCFBPAC1MRPT:      4;            // [11:8] = Minimum Reads Per Turn
    LwU8 PMCFBPAC1MWPT:      4;            // [15:12] = Minimum Writes Per Turn
    LwU8 PMCFBPAC1RLO:       4;            // [19:16] = Read Latency Offset
    LwU8 PMCFBPAC1WLO:       4;            // [23:20] = Write Latency Offset
    LwU8 PMCFBPAC1LI:        8;            // [31:24] = Latency Interval
} PerfMemClkFBPAConfig1;

typedef struct __attribute__((__packed__)) PerfMemClk11EntryFlags1
{
    LwU8 PMC11EF1MPLLSSC:        1;        // [0:0] = RefMPLL SSC
    LwU8 PMC11EF1MPLLSSType:     1;        // [1:1] = SSC Spread Type
    LwU8 PMC11EF1ScriptIndex:    2;        // [3:2] = Script Index
    LwU8 PMC11EF1ClkDriveMode:   1;        // [4:4] = Clock Drive Mode
    LwU8 PMC11EF1WCKDriveMode:   1;        // [5:5] = WCK Drive Mode
    LwU8 PMC11EF1WCKTermination: 1;        // [6:6] = WCK Termination
    LwU8 PMC11EF1G5XCmdMode:     1;        // [7:7] = G5X Command Mode
} PerfMemClk11EntryFlags1;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
typedef struct __attribute__((__packed__)) PerfMemClk11EntryFlags30
{
    LwU16 PMC11EF30STARTTRM:      11;  //[10:0] = STARTTRM CALIBRATION
} PerfMemClk11EntryFlags30;

typedef struct __attribute__((__packed__)) PerfMemClk11EntryParam0
{
    LwU32 PMC11EP0EdcVrefVbiosCtrl:      1;  //[0:0] = EDC VREF VBIOS Control
    LwU32 PMC11EP0PrbsPattern:           2;  //[2:1] = PRBS Pattern
    LwU32 PMC11EP0EdcHoldPattern:        4;  //[6:3] = EDC Hold Pattern
    LwU32 PMC11EP0INTERPCAP:             3;  //[9:7] = INTERP_CAP
    LwU32 PMC11EP0Rsvd:                 21;  //[31:10] = Reserved
} PerfMemClk11EntryParam0;

typedef struct __attribute__((__packed__)) PerfMemClk11EntryParam1
{
    LwU32 PMC11EP1EdcTrInitialGainDelay:      8;  // [7:0] = EDC Tracking Initial Gain Delay
    LwU32 PMC11EP1EdcTrFinalGainDelay:        8;  // [15:8] = EDC Tracking Final Gain Delay
    LwU32 PMC11EP1VrefTrInitialGainDelay:     8;  // [23:16] = VREF Tracking Initial Gain Delay
    LwU32 PMC11EP1VrefTrFinalGainDelay:       8;  // [31:24] = VREF Tracking Final Gain Delay
} PerfMemClk11EntryParam1;

typedef struct __attribute__((__packed__)) PerfMemClk11EntryParam2
{
    LwU16 PMC11EP2WriteCmdFifoDepth:      5;  // [4:0] = Write Cmd Fifo Depth
    LwU16 PMC11EP2WriteBankGroupHoldoff:  4;  // [8:5] = Write Bank Group Holdoff
    LwU16 PMC11EP2ReadBankGroupHoldoff:   4;  // [12:9] = Read Bank Group Holdoff
    LwU16 PMC11EP2ActArbCfgBgsec:         1;  // [13:13] = Act Arb Cfg Bgsec
    LwU16 PMC11EP2WrBankHitBgsecWt:       1;  // [14:14] = Write Bank Hit Bgsec Wt
    LwU16 PMC11EP2BgEnableInCcd:          1;  // [15:15] = Bg Enable in CCD
    LwU16 PMC11EP2WrArbCfgWrPullReq:      1;  // [16:16] = Write arb -> WRITEPULL_REQ
    LwU16 PMC11EP2WrFtCfgMaxChainCloseFL: 7;  // [24:17] = Friendtracker -> MAX_CHAIN_CLOSE_FL:',
    LwU16 PMC11EP2ArbCfgNoTurnIfBankHit:  1;  // [25:25] = Direction arb -> NO_TURN_IF_HIT:',
} PerfMemClk11EntryParam2;

// bug 3004541 FBPA_DIR_ARB_CFG
typedef struct __attribute__((__packed__)) PerfMemClk11EntryParam3
{
    LwU16 PMC11EP3HighEffRdSectors:     16;  // [15:0]  = High Eff Rd Sectors
    LwU16 PMC11EP3HighEffWrSectors:     16;  // [31:16] = High Eff Wr Sectors
} PerfMemClk11EntryParam3;

typedef struct __attribute__((__packed__)) PerfMemClk11EntryParam4
{
    LwU16 PMC11EP4LowEffRdSectors:     16;  // [15:0]  = Low Eff Rd Sectors
    LwU16 PMC11EP4LowEffWrSectors:     16;  // [31:16] = Low Eff Wr Sectors
} PerfMemClk11EntryParam4;

typedef struct __attribute__((__packed__)) PerfMemClk11EntryParam5
{
    LwU16 PMC11EP5LowEffMinRdInt:        4;  // [3:0]  = Low Eff Min Rd Int
    LwU16 PMC11EP5LowEffMinWrInt:        4;  // [7:4]  = Low Eff Min Wr Int
    LwU16 PMC11EP5HighEffRdLimit:        8;  // [15:8]  = High Eff Rd Limit
    LwU16 PMC11EP5HighEffWrLimit:        8;  // [23:16] = High Eff Wr Limit
} PerfMemClk11EntryParam5;
#endif

typedef struct __attribute__((__packed__)) PerfMemClk11BaseEntry
{
    PerfMemClkBaseEntryMinMax Minimum;  // PerfMemClkBaseEntryMinMax // Minimum frequency of Memory Clock
    PerfMemClkBaseEntryMinMax Maximum;  // PerfMemClkBaseEntryMinMax // Maximum frequency of Memory Clock
    dd ScriptPointer;
    PerfMemClk11EntryFlags0 Flags0;  // PerfMemClk11EntryFlags0   // Memory Clock Table Base Entry Flags Byte 0
    PerfMemClkFBPAConfig0 FBPAConfig0; // PerfMemClkFBPAConfig0 // Memory Clock Table Base Entry FBPA Configuration 0 (bug 619826)
    PerfMemClkFBPAConfig1 FBPAConfig1; // PerfMemClkFBPAConfig1 // Memory Clock Table Base Entry FBPA Configuration 1 (bug 517408)
    PerfMemClk11EntryFlags1 Flags1;    // PerfMemClk11EntryFlags1 // Memory Clock Table Base Entry Flags Byte 1 ( Bug 897767
    db RefMPLLSCFreqDelta;                   // Memory Clock Table Base Entry REFMPLL Spread Freq Delta ( Bug 897767 )
    db Flags2;   //Cmd Script Index
    dd spare_field0;
    dd spare_field1;
    dd spare_field2;
    dd spare_field3;
    dd spare_field4;
    dd spare_field5;
    dd spare_field6;
    dd spare_field7;
    dd spare_field8;
    dd spare_field9;
    #if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
      PerfMemClk11EntryFlags30 Flags30;
      PerfMemClk11EntryParam0 Param0;   // Vref settings
      PerfMemClk11EntryParam1 Param1;   // Gain Delay
      PerfMemClk11EntryParam2 Param2;   // Bank group settings (Bug 2847131) bug 2987773
      PerfMemClk11EntryParam3 Param3;   // High Eff Rd/Wr sector bug 3004541 FBPA_DIR_ARB_CFG
      PerfMemClk11EntryParam4 Param4;   // Low Eff Rd/Wr sector bug 3004541 FBPA_DIR_ARB_CFG
      PerfMemClk11EntryParam5 Param5;   // Low Eff Min Rd/Wr Int and High Eff Rd/Wr Limit bug 3004541
    #endif
} PerfMemClk11BaseEntry;

#define PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_HIGH  0x0
#define PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW   0x1
#define PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVREF_HIGH  0x0
#define PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVREF_LOW   0x1


typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryFlags1
{
    LwU8 PMC11SEF1VDDPMode:    2;          // [1:0] = VDDP Mode
    LwU8 PMC11SEF1FBVDDQ:      1;          // [2:2] = FBVDD/Q Voltage
    LwU8 PMC11SEF1FBVREF:      1;          // [3:3] = GDDR5: FBVREF Voltage
    LwU8 PMC11SEF1MemVREFD:    1;          // [4:4] = GDDR5: Memory VREFD
    LwU8 PMC11SEF1ASR:         1;          // [5:5] = ASR
    LwU8 PMC11SEF1ADLLPwrIBOB: 1;          // [6:6] = Analog DLL Power, Inbound
    LwU8 PMC11SEF1DDLLPwrIB:   1;          // [7:7] = Digital DLL Power, Inbound

} PerfMemClk11StrapEntryFlags1;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryFlags3
{
    LwU8 PMC11SEF3MSCG:          1;        // [0:0] = MSCG
    LwU8 PMC11SEF3PerTrn:        1;        // [1:1] = Periodic Training
    LwU8 PMC11SEF3TrnDlyDev:     2;        // [3:2] = Training Delay Device
    LwU8 PMC11SEF3ADLLPwrOB:     1;        // [4:4] = Analog DLL Power, Outbound
    LwU8 PMC11SEF3VREFTrn:       1;        // [5:5] = VREF Training
    LwU8 PMC11SEF3DLLCal:        1;        // [6:6] = DLL Calibration
    LwU8 PMC11SEF3GPUDQSTerm:    1;        // [7:7] = GPU DQS Termination
} PerfMemClk11StrapEntryFlags3;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryFlags5
{
    LwU8 PMC11SEF5QUSETrn:         1;        // [0:0] = QUSE Training
    LwU8 PMC11SEF5DLCELLTrn:       1;        // [1:1] = DLCELL Training
    LwU8 PMC11SEF5DiffDQS:         1;        // [2:2] = Differential DQS
    LwU8 PMC11SEF5PllRange:        1;        // [3:3] = Pll Range
    LwU8 PMC11SEF5VDDP:            2;        // [5:4] = CALGROUP VDDP Mode
    LwU8 PMC11SEF5G5XVref:         1;        // [6:6] = GDDR5x Internal VrefC
    LwU8 PMC11SEF5PeriodicDDLLCal: 1;        // [7:7] = Periodic DDLL calibration (Bug 200240518)
} PerfMemClk11StrapEntryFlags5;


typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryG5xMRS14
{
    LwU8 PMC11SEG5xMRS14Voltage:   6;     // [5:0] = GDDR5x Micron Memory Voltage Offset
    LwU8 PMC11SEG5xMRS14SubRegSel: 1;     // [6:6] = GDDR5x Subregister Select
    LwU8 PMC11SEG5xMRS14Rsvd1:     1;     // [7:7] = Reserved
} PerfMemClk11StrapEntryG5xMRS14;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryHBMParam0
{
    LwU16 PMC11SEP0Hbm_tx_trim: 11;
    LwU16 PMC11SEP0Hbm_wl_trim: 11;
    LwU16 PMC11SEP0Rsvd:        10;
} PerfMemClk11StrapEntryHBMParam0;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryFlags6
{
     db PMC11SEF6EdcTrackEn: 1;   //[0:0] Enable EDC Tracking ( http://lwbugs/2109204 )
     db PMC11SEF6VrefTrackEn: 1;  //[1:1] Enable VREF Tracking ( http://lwbugs/2109204 )
     db PMC11SEF6PerTrngAvg: 1 ;
     db PMC11SEF6PerShiftTrng: 1 ;
     db PMC11SEF6MRS14SubRegSelect: 2;//[5:4] Micron Gddr(5/5x/6) memory subregister select - MRS14[10:9]
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
     db PMC11SEF6DCC:              2;//[7:6] = Control Duty Cycle Correction
#else
     db Reserved: 2 ;
#endif
}PerfMemClk11StrapEntryFlags6;


#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
typedef struct __attribute__((__packed__))  PerfMemClk11StrapEntryFlags7
{
    db PMC11SEF7SwtchCmdCnt: 5;//[4:0] = Memory Clock Switch Command Count
    db PMC11SEF7RPatShftIlw: 1;//[5:5] = Enable RD Pattern Shift Ilwert
    db PMC11SEF7ASRACPD:     1;//[6:6] = Enable Aggressive Self Refresh ACPD
    db PMC11SEF7ACPD:        1;//[7:7] = Enable DRAM Active Powerdown
} PerfMemClk11StrapEntryFlags7;

typedef struct __attribute__((__packed__))  PerfMemClk11StrapEntryFlags8
{
    db PMC11SEF7PrTrCmdCnt:  5;//[4:0] = Periodic Training Command Count
    db PMC11SEF7PrIlw:       1;//[5:5] = Enable Periodic Pattern Ilwert
    db PMC11SEF7WPatIlw:     1;//[6:6] = Enable WR Pattern Ilwert
    db PMC11SEF7WPatShft:    1;//[7:7] = Enable WR Pattern Shift
}PerfMemClk11StrapEntryFlags8;

typedef struct __attribute__((__packed__))  PerfMemClk11StrapEntryFlags9
{
    db PMC11SEF9EnMaxQPwr:          1;//[0:0] = Enable MaxQ Power
    db PMC11SEF9EnCabiMode:         1;//[1:1] = Enable Command/Address Bit Ilwersion Mode
    db PMC11SEF9EnPreemphTxEqAc:    1;//[2:2] = Enable Preemphasis TX Equalization AC Mode
    db PMC11SEF9EnDqTxEqAc:         1;//[3:3] = Enable DQ TX Equalization AC
    db PMC11SEF9EnWckTxEqAc:        1;//[4:4] = Enable Wck TX Equalization AC
    db PMC11SEF9EnCkTxEqAc:         1;//[5:5] = Enable Ck TX Equalization AC
    db PMC11SEF9EnPrbsHoldPat:      1;//[6:6] = Enable PRBS Hold Pattern
    db PMC11SEF9Rsvd:               1;//[7:7] = Reserved
}PerfMemClk11StrapEntryFlags9;

typedef struct __attribute__((__packed__))  PerfMemClk11StrapEntryGDDR6Param0
{
    dd PMC11SEG6P0Timing24TxsOffset:  6;//[5:0] = Timing24 TXS Offset
    dd PMC11SEG6P0Rsvd:              26;//[31:6] = Reserved
}PerfMemClk11StrapEntryGDDR6Param0;

typedef struct __attribute__((__packed__))  PerfMemClk11StrapEntryFlags10
{
    dd PMC11SEF10AsrExitTraining:    2;//[1:0] = ASR_Exit_Training
    dd PMC11SEF10VrefcOffset:        5;//[6:2] = VREFC_OFFSET
    dd PMC11SEF10DramTxEq:           1;//[7:7] = DRAM_TX_EQ
    dd PMC11SEF10Fbvddq:             2;//[9:8] = FBVDD/Q
    dd PMC11SEF10CtrlEqAc:           6;//[15:10] = CMD_TX_E_PREEMPH_CTRL_EQ_AC
    dd PMC11SEF1016GPreEmphasis:     2;//[17:16] = 16GB GDDR6X Pre-emphasis for Micron 16GB memory (replaces TX_EQ)
    dd PMC11SEF1016GDqCtleDfe:       6;//[23:18] = 16GB GDDR6X DQ CTLE_DFE
    dd PMC11SEF1016GDqxCtleDfe:      6;//[29:24] = 16GB GDDoR6X DQX CTLE_DFE
    dd PMC11SEF1016GDfeExtension:    1;//[30:30] = 16GB DFE extension. If set Flag 11 contains additional Dfe values
    dd PMC11SEF10Rsvd:               1;//[31:30] = Reserved
}PerfMemClk11StrapEntryFlags10;

typedef struct __attribute__((__packed__))  PerfMemClk11StrapEntryFlags11
{
    dd PMC11SEF1116GDqCtleDfeMid:    6;// [5:0]  = 16GB GDDR6X DQ CTLE_DFE Mid
    dd PMC11SEF1116GDqxCtleDfeMid:   6;//[11:6]  = 16GB GDDoR6X DQX CTLE_DFE Mid
    dd PMC11SEF1116GDqCtleDfeHigh:   6;//[17:12] = 16GB GDDR6X DQ CTLE_DFE High
    dd PMC11SEF1116GDqxCtleDfeHigh:  6;//[23:18] = 16GB GDDoR6X DQX CTLE_DFE High
    dd PMC11SEF11Rsvd:               8;//[31:24] = Reserved
}PerfMemClk11StrapEntryFlags11;

#endif

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryHBMParam1
{
    LwU16 PMC11SEP1Hbm_rx_trim:    11;
    LwU16 PMC11SEP1rsvd:           3;
    LwU16 PMC11SEP1HbmDqRcvrSel:   1;
    LwU16 PMC11SEP1HbmRdqsRcvrSel: 1;     // 0: cmos  1:diff-amp
} PerfMemClk11StrapEntryHBMParam1;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
enum TempSamplingIntervalEnum {
    TEMPSAMPLINGINTERVAL_DISABLED = 0,
    TEMPSAMPLINGINTERVAL_1MS = 1,
    TEMPSAMPLINGINTERVAL_5MS = 2,
    TEMPSAMPLINGINTERVAL_10MS = 3,
    TEMPSAMPLINGINTERVAL_20MS = 4,
    TEMPSAMPLINGINTERVAL_50MS = 5,
    TEMPSAMPLINGINTERVAL_100MS = 6,
    TEMPSAMPLINGINTERVAL_200MS = 7,
    TEMPSAMPLINGINTERVAL_500MS = 8,
    TEMPSAMPLINGINTERVAL_1000MS = 9,
};
#endif
typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryHbmRefParam0Struct
{
    LwU16 PMC11HRP0Interval:              6;  // [5:0]  = Temp sampling interval
    LwU16 PMC11HRP0Multiplier:            2;  // [7:6]  = Multiplier
    LwU16 PMC11HRP0CoeffM:                4;  // [11:8] = M
    LwU16 PMC11HRP0CoeffN:                4;  // [15:12] = N
    LwU16 PMC11HRP0SwitchCtrl:            1;  // [16:16]  = Temp Based Switching
    LwU16 PMC11HRP0Rsvd:                  15; // [31:17] = RFU
} PerfMemClk11StrapEntryHbmRefParam0Struct;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryHbmRefParam1Struct
{
    LwU16 PMC11HbmRefParam1ThresholdTempLow;  // [15:0] = HBM lower threshold temperature
    LwU16 PMC11HbmRefParam1ThresholdTempHigh; // [31:16]= HBM higher threshold tempearature
} PerfMemClk11StrapEntryHbmRefParam1Struct;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryFlags0
{
    LwU8 PMC11SEF0VTTGEN:      1;          // [0:0] = VTTGEN
    LwU8 PMC11SEF0BRICK:       1;          // [1:1] = BRICK
    LwU8 PMC11SEF0GPUTerm:     1;          // [2:2] = GPU Termination
    LwU8 PMC11SEF0GPUAutoATR:  1;          // [3:3] = GPU Auto ATR
    LwU8 PMC11SEF0GPUStr40PU:  1;          // [4:4] = GPU Strong 40 Ohm Pullup
    LwU8 PMC11SEF0MemDLL:      1;          // [5:5] = Memory DLL
    LwU8 PMC11SEF0LowFreq:     1;          // [6:6] = Low Frequency Mode
    LwU8 PMC11SEF0AlignMode:   1;          // [7:7] = Alignment Mode
} PerfMemClk11StrapEntryFlags0;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryFlags2
{
    LwU8 PMC11SEF2RCVCtrl:       4;        // [3:0] = Receiver Control
    LwU8 PMC11SEF2DQWDQSRcvSel:  2;        // [5:4] = DQ/WDQS Receiver Select
    LwU8 PMC11SEF2RDQSRcvSel:    2;        // [7:6] = RDQS Receiver Select
} PerfMemClk11StrapEntryFlags2;

typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntryFlags4
{
    LwU8 PMC11SEF4TXEnEarly:     1;        // [0:0] = TX Enable Early
    LwU8 PMC11SEF4TXEnSetupSTR:  1;        // [1:1] = TX Enable Setup STR
    LwU8 PMC11SEF4TXEnSetupDQ:   2;        // [3:2] = TX Enable Setup DQ
    LwU8 PMC11SEF4AddrTraining:  1;        // [4:4] = Address Training
    LwU8 PMC11SEF4BG_8:          1;        // [5:5] = Bank Grouping
    LwU8 PMC11SEF4RxDfe:         1;        // [6:6] = RX Decision Feedback Equalization
    LwU8 PMC11SEF4MRS7VDD:       1;        // [7:7] = MRS7 FBVDD (GDDR5)
} PerfMemClk11StrapEntryFlags4;

 typedef struct __attribute__((__packed__)) PerfMemClk11StrapEntry
 {
    db MemTweakIdx;                          // PERF_MCLK_11_STRAPENTRY_MEMTWEAKIDX_NONE ; Memory Tweak Index
    PerfMemClk11StrapEntryFlags0 Flags0;
    PerfMemClk11StrapEntryFlags1 Flags1;    // PerfMemClk11StrapEntryFlags1  ; Memory Clock Table Strap Entry Flags Byte 1
    PerfMemClk11StrapEntryFlags2 Flags2;
    db ClkDly;                               //  CMOS Clock Delay
    db WriteBrlShft;                         //  Non-training Write Barrel Shifter
    db VREFDOffsets;                         //  GDDR5: Offsets for upper & lower 2 bytes of VREFD
    PerfMemClk11StrapEntryFlags3 Flags3;                               // ; Memory Clock Table Strap Entry Flags Byte 3
    PerfMemClk11StrapEntryFlags4 Flags4;          // struct PerfMemClk11StrapEntryFlags4
    db QUSEDDLL;                             // QUSE DDLL Trimmer
    PerfMemClk11StrapEntryFlags5 Flags5;    // PerfMemClk11StrapEntryFlags5  ; Memory Clock Table Strap Entry Flags Byte 5
    PerfMemClk11StrapEntryG5xMRS14 MicronGDDR5MRS14Offset;      //micron core voltage offset + subregister select
    PerfMemClk11StrapEntryHBMParam0 HBMParam0;                  // PerfMemClk11StrapEntryHBMParam0
    PerfMemClk11StrapEntryHBMParam1 HBMParam1;                  // PerfMemClk11StrapEntryHBMParam1
    PerfMemClk11StrapEntryHbmRefParam0Struct HbmRefParam0;      // PerfMemClk11StrapEntryHbmRefParam0Struct
    PerfMemClk11StrapEntryHbmRefParam1Struct HbmRefParam1;      // PerfMemClk11StrapEntryHbmRefParam1Struct
    dd EdcTrackingGain;
    dw VrefTrackingGain;
    PerfMemClk11StrapEntryFlags6 Flags6;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    PerfMemClk11StrapEntryFlags7 Flags7;
    PerfMemClk11StrapEntryFlags8 Flags8;
    PerfMemClk11StrapEntryFlags9 Flags9;
    PerfMemClk11StrapEntryGDDR6Param0 Gddr6Param0;
    PerfMemClk11StrapEntryFlags10 Flags10;
    PerfMemClk11StrapEntryFlags11 Flags11;
#endif
 } PerfMemClk11StrapEntry;


//typedef struct __attribute__((__packed__)) {    // Memory Clock Table Strap Entry Flags Byte 5
//      LwU32 rsvd:       30;
//	  db dq_rcvr_sel:    1;        // [1:1] = dq receiver selection:   0=cmos, 1=diff_amp
//      db rdqs_rcvr_sel:  1;        // [0:0] = rdqs receiver selection: 0=cmos, 1=diff_amp
//} PerfMemClk11StrapEntryExtenstionTempFlags6;

// typedef struct __attribute__((__packed__)) {
//    PerfMemClk11StrapEntryExtenstionTempFlags6 Flags6;
//	LwU32 hbm_tx_trim;
//	LwU32 hbm_wl_trim;
//	LwU32 hbm_rx_trim;
// } PerfMemClk11StrapEntryExtemsionTemp ;

//------------------------------------------------------------------------------------------------
// MEMTWEAK TABLE
// origin: http://rmopengrok.lwpu.com/source/xref/sw/main/bios/core86/inc/perfdef.inc#1004
//


#define PERF_MTWEAK_2X_VERSION 0x20

 typedef struct __attribute__((__packed__)) PerfMemTweak2xHeader
 {
     db Version;
     db HeaderSize;      // sizeof(PerfMemTweak2xHeader)
     db BaseEntrySize;   // sizeof(PerfMemTweak2xBaseEntry)
     db ExtEntrySize;    // sizeof(PerfMemTweak2xExtEntry)
     db ExtEntryCount;   // Number of Memory Tweak Table Extended Entries per Base Entry
     db EntryCount;      // Number of Memory Tweak Table Entries (Base Entry + Extended Entry Count)
 } PerfMemTweak2xHeader;

// Config4 High Temp
typedef struct __attribute__((__packed__)) PerfMemTweak2xBaseEntryHBMRefresh
{
    LwU16 PMCT2xHBMREFRESH_LO:     3;  // CONFIG4.HBM_REFRESH_LO
    LwU16 PMCT2xHBMREFRESH:        12; // CONFIG4.HBM_REFRESH_LO
    LwU16 PMCT2xHBMREFREFSB:       1;  // TIMING21.REFSB
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    LwU16 PMCT2xDISPATCH_PERIOD:   10; // MemTweakEntry_C4_HBMHTRefresh_DispPer1Prompt
#endif
} PerfMemTweak2xBaseEntryHBMRefresh;

typedef struct __attribute__((__packed__)) PerfMemTweak2xBaseEntry367352
{
    LwU8 Timing12_CKE:   6;     // LW_PFB_FBPA_TIMING12_CKE
    LwU8 Timing11_KO:    7;      // LW_PFB_FBPA_TIMING11_KO
    LwU8 Reserved367352: 3;
} PerfMemTweak2xBaseEntry367352;    // struct PerfMemTweak2xBaseEntry367352

typedef struct __attribute__((__packed__))  PerfMemTweak2xBaseEntry375368
{
    LwU8 DataTerm:       3;
    LwU8 Reserved375368: 1;
    LwU8 AddrCmdTerm0:   2;
    LwU8 AddrCmdTerm1:   2;
} PerfMemTweak2xBaseEntry375368;

typedef struct __attribute__((__packed__))  PerfMemTweak2xBaseEntry383376
{
    LwU8 DrvStrength: 2;
    LwU8 DQADVauxp:   3;
    LwU8 DQADVclamp:  3;
} PerfMemTweak2xBaseEntry383376;

typedef struct __attribute__((__packed__)) PerfMemTweak2xBaseEntry391384
{
    LwU8 DQADVauxc:   3;
    LwU8 Timing1_R2P: 5;                // LW_PFB_FBPA_TIMING1_R2P
} PerfMemTweak2xBaseEntry391384;

typedef struct __attribute__((__packed__)) PerfMemTweak2xBaseEntry399392
{
    LwU8 CKVauxc:   3;
    LwU8 CKPwrDwn:  1;
    LwU8 WCKVauxc:  3;
    LwU8 WCKPwrDwn: 1;
} PerfMemTweak2xBaseEntry399392;

typedef struct __attribute__((__packed__)) PerfMemTweak2xBaseEntry407400
{
    LwU8 ClkDistVauxc:  3;
    LwU8 ClkDistPwrDwn: 1;
    LwU8 CDBVauxc:      3;
    LwU8 CDBPwrDwn:     1;
} PerfMemTweak2xBaseEntry407400;

typedef struct __attribute__((__packed__)) PerfMemTweak2xBaseEntry415408
{
    LwU8 RDCRC:     4;
    LwU8 CDBVauxc4: 4;              // vauxc level 4 bit (bug 2847131)
} PerfMemTweak2xBaseEntry415408;

typedef struct __attribute__((__packed__)) PerfMemTweak2xBaseEntry
{
    dd Config0;                           // LW_PFB_FBPA_CONFIG0
    dd Config1;                           // LW_PFB_FBPA_CONFIG1
    dd Config2;                           // LW_PFB_FBPA_CONFIG2
    dd Config3;                           // LW_PFB_FBPA_CONFIG3
    dd Config4;                           // LW_PFB_FBPA_CONFIG4
    dd Config5;                           // LW_PFB_FBPA_CONFIG5
    dd Config6;                           // LW_PFB_FBPA_CONFIG6
    dd Config7;                           // LW_PFB_FBPA_CONFIG7
    dd Config8;                           // LW_PFB_FBPA_CONFIG8
    dd Config9;                           // LW_PFB_FBPA_CONFIG9
    dd Timing10;                          // LW_PFB_FBPA_TIMING10
    PerfMemTweak2xBaseEntry367352 pmtbe367352;
    PerfMemTweak2xBaseEntry375368 pmtbe375368;
    PerfMemTweak2xBaseEntry383376 pmtbe383376;
    PerfMemTweak2xBaseEntry391384 pmtbe391384;
    PerfMemTweak2xBaseEntry399392 pmtbe399392;
    PerfMemTweak2xBaseEntry407400 pmtbe407400;
    PerfMemTweak2xBaseEntry415408 pmtbe415408;
    dd Timing21;                          // LW_PFB_FBPA_TIMING21
    dd Timing22;                          // LW_PFB_FBPA_TIMING22
    dd Cal_Trng_Arb;
    dd Config10;
    dd Config11;
    PerfMemTweak2xBaseEntryHBMRefresh Config4HBMRefresh;  // PerfMemTweak2xBaseEntryHBMRefresh
} PerfMemTweak2xBaseEntry;


typedef struct __attribute__((__packed__))  PerfMemTweak2xExtEntryRegister
{
    LwU32 PMTEERRsvd:     8;
    LwU32 PMTEEPrivAddr: 24;
} PerfMemTweak2xExtEntryRegister;

typedef struct __attribute__((__packed__)) PerfMemTweak2xExtEntry
{
    PerfMemTweak2xExtEntryRegister PMTEERegister;
    dd PMTEEMask;
    dd PMTEEValue;
} PerfMemTweak2xExtEntry;

//---------------------------------------------------------------------------------------------------------------------------
// Boot Training Table Definition
//---------------------------------------------------------------------------------------------------------------------------
//
// vbios defintion comfing from //sw/main/bios/core90/inc/memboottraintbldef.inc , implmementation tracked in bug 2024779
// Table definitions updated from bug 2101908

typedef struct __attribute__((__packed__)) BOOT_TRAINING_FLAGS {
    LwU8 flag_g6_addr_tr_en:                  1; //  [0:0]
    LwU8 flag_g6_wck_tr_en:                   1; //  [1:1]
    LwU8 flag_g6_rd_tr_en:                    1; //  [2:2]
    LwU8 flag_g6_rd_tr_prbs_en:               1; //  [3:3]
    LwU8 flag_g6_rd_tr_area_en:               1; //  [4:4]
    LwU8 flag_g6_rd_tr_hybrid_non_vref_en:    1; //  [5:5]
    LwU8 flag_g6_rd_tr_hybrid_vref_en:        1; //  [6:6]
    LwU8 flag_g6_rd_tr_hw_vref_en:            1; //  [7:7]
    LwU8 flag_g6_rd_tr_redo_prbs_settings:    1; //  [8:8]
    LwU8 flag_g6_rd_tr_pattern_shift_tr_en:   1; //  [9:9]
    LwU8 flag_g6_wr_tr_en:                    1; //  [10:10]
    LwU8 flag_g6_wr_tr_prbs_en:               1; //  [11:11]
    LwU8 flag_g6_wr_tr_area_en:               1; //  [12:12]
    LwU8 flag_g6_wr_tr_hybrid_non_vref_en:    1; //  [13:13]
    LwU8 flag_g6_wr_tr_hybrid_vref_en:        1; //  [14:14]
    LwU8 flag_g6_wr_tr_hw_vref_en:            1; //  [15:15]
    LwU8 flag_g6_pattern_shift_tr_en:         1; //  [16:16]
    LwU8 flag_g6_train_rd_wr_different:       1; //  [17:17]
    LwU8 flag_g6_train_prbs_rd_wr_different:  1; //  [18:18]
    LwU8 flag_g6_refresh_en_prbs_tr:          1; //  [19:19]
    LwU8 flag_edc_tracking_en:                1; //  [20:20]
    LwU8 flag_vref_tracking_en:               1; //  [21:21]
    LwU8 flag_rd_edc_enabled:                 1; //  [22:22]
    LwU8 flag_wr_edc_enabled:                 1; //  [23:23]
    LwU8 flag_edc_tracking_prbs_hold_pattern: 1; //  [24:24]
    LwU8 flag_shift_pattern_wr:               1; //  [25:25]
    LwU8 flag_shift_pattern_rd:               1; //  [26:26]
    LwU8 flag_falcon_to_determine_best_window:1; //  [27:27]
    LwU8 flag_collect_schmoo_data:            1; //  [28:28]
    LwU8 flag_skip_boot_training:             1; //  [29:29]
// new for vbios
    LwU8 flag_ate_nofail_en:                  1; //  [30:30]
    LwU8 flag_rsved:                          1; //  [31:31]
// old
//    LwU8 flag_rsved:                          2; //  [31:30]
} BOOT_TRAINING_FLAGS;


typedef struct __attribute__((__packed__)) BOOT_TRAINING_SETTLE_TIME {
    LwU8 RD_VREF;
    LwU8 RD_DFE;
    LwU8 WR_VREF;
    LwU8 WR_DFE;
} BOOT_TRAINING_SETTLE_TIME;


typedef struct __attribute__((__packed__)) BOOT_TRAINING_EDC_MISC_SETTLE {
    LwU8 EDGE_OFFSET_BKV;
    LwU8 ACLWMULATOR_RESET_VAL;
    LwU8 VREF_INITIAL_SETTLE_TIME;
    LwU8 VREF_FINAL_SETTLE_TIME;
} BOOT_TRAINING_EDC_MISC_SETTLE;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_BKV_OFFSET {
    LwS8 IB_VREF;
    LwS8 IB_DFE;
    LwS8 OB_VREF;
    LwS8 OB_DFE;
} BOOT_TRAINING_BKV_OFFSET;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_EDC_VREF_TRACKING {
    LwU8 EDC_HOLD_PATTERN:      4;     // [3:0]
    LwU8 HUNT_MODE:             1;     // [4:4]
    LwU8 EDC_READ_GAIN_INITIAL: 6;     // [10:5]
    LwU8 VREF_GAIN_INITIAL:     6;     // [16:11]
    LwU8 VREF_GAIN_FINAL:       6;     // [22:17]
    LwU8 EDC_SETTLE_TIME:       4;     // [26:23]
    LwU8 RFU:                   5;     // [31:27]
} BOOT_TRAINING_EDC_VREF_TRACKING;

#define BOOT_TRAINING_TABLE_1X_VERSION  0x10
#define BOOT_TRAINING_TABLE_11_VERSION  0x11
#define MAX_TRAINING_PATRAM_PRBS_SEEDS  0x8        // seeds 0-7

typedef struct __attribute__((__packed__)) BOOT_TRAINING_VREF_CONTROL {
    LwU8 INIT_VREF_SCHEME:                         2;   // [1:0]
#define INIT_VREF_SCHEME_AREA_BASED   0x0
#define INIT_VREF_SCHEME_MAX_PI_PASED 0x1
#define INIT_VREF_SCHEME_BKV          0x2
    LwU8 CORS_STEP_OVERRIDE_INIT:                  1;   // [2:2]
#define CORS_STEP_OVERRIDE_INIT_ENABLE  0x0
#define CORS_STEP_OVERRIDE_INIT_DISABLE 0x1
    LwU8 CORS_STEP_VALUE_RD:                       4;   // [6:3]
    LwU8 CORS_STEP_VALUE_WR:                       4;   // [10:7]
    LwU8 OPTIMAL_DFE_RD:                           2;   // [12:11]
#define OPTIMAL_DFE_RD_LARGEST_EYE     0x0
#define OPTIMAL_DFE_RD_ROLLING_AVERAGE 0x1
#define OPTIMAL_DFE_RD_NO_SW_OPTIMAL   0x2
    LwU8 OPTIMAL_DFE_WR:                           2;   // [15:13]
#define OPTIMAL_DFE_WR_LARGEST_EYE     0x0
#define OPTIMAL_DFE_WR_ROLLING_AVERAGE 0x1
#define OPTIMAL_DFE_WR_NO_SW_OPTIMAL   0x2
    LwU8 USE_BKV_VREF_SWEEP:                       1;   // [15:15]
#define USE_BKV_VREF_SWEEP_DISABLE     0x0
#define USE_BKV_VREF_SWEEP_ENABLE      0x1
    LwU8 CORS_STEP_OVERRIDE_SWEEP_RD:              1;   // [16:16]
    LwU8 CORS_STEP_OVERRIDE_SWEEP_WR:              1;   // [17:17]
    LwU8 AVERAGING_LOOP_RD:                        4;   // [21:18]
    LwU8 AVERAGING_LOOP_WR:                        4;   // [25:22]
    LwU8 RESERVED:                                 6;   // [31:26]
} BOOT_TRAINING_VREF_CONTROL;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_VREF_OFFSET1 {
    LwU8 dfe_offset_low:      3; // [2:0]
    LwU8 dfe_offset_mid:      3; // [5:3]
    LwU8 def_offset_high:     3; // [8:6]
    LwU8 vref_offset_low_ib:  6; // [14:9]
    LwU8 vref_offset_mid_ib:  6; // [20:15]
    LwU8 vref_offset_high_ib: 6; // [26:21]
    LwU8 reserved:            5; // [31:27]
} BOOT_TRAINING_VREF_OFFSET1;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_VREF_OFFSET2 {
    LwU8 vref_offset_low_ob:   4;   // [3:0]
    LwU8 vref_offset_mid_ob:   4;   // [7:4]
    LwU8 vref_offset_high_ob:  4;   // [11:8]
    LwU32 reserved:          20;   // [31:12]
} BOOT_TRAINING_VREF_OFFSET2;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_PI_OFFSET_TRAINING {
    LwU8 PI_OFFSET_ENABLE:                         1;    // [0:0]
#define PI_OFFSET_ENABLE_DISABLED      0x0
#define PI_OFFSET_ENABLE_ENABLED       0x1
    LwS8 PI_OFFSET_CTRL_MIN:                       6;    // [6:1]
    LwU8 PI_OFFSET_CTRL_MAX:                       6;    // [12:7]
    LwU8 PI_OFFSET_CTRL_STEP:                      4;    // [16:13]
    LwU16 RESERVED:                               15;    // [31:17]
} BOOT_TRAINING_PI_OFFSET_TRAINING;

// newly added for GH100, bug 3236636
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X)) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
typedef struct __attribute__((__packed__)) BOOT_TRAINING_HBM_TRAINING {
    LwU16 TRAINING_DELAY_MIN:                     10;    // [9:0]
    LwU16 TRAINING_DELAY_MAX:                     10;    // [19:10]
    LwU8  TRAINING_DELAY_STEP:                     4;    // [23:20]
    LwU16 RESERVED:                                8;    // [31:24]
} BOOT_TRAINING_HBM_TRAINING;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_HBM_PERIODIC_TRAINING {
    LwU8  interval:                                8;    // [7:0]   periodic_tr_interval
    LwU8  oscillator_run_time:                     4;    // [11:8]
    LwU8  ecs_read_ratio:                          8;    // [19:12] PeriodicTr Ratio for ESC reads
    LwU16 RESERVED:                               12;    // [31:20]
} BOOT_TRAINING_HBM_PERIODIC_TRAINING;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_HBM_MISC1 {
    LwU16 pattern_load_freq:                      16;    // [15:0]
    LwU8  rdqs_training_offset:                    5;    // [20:16]
    LwU8  flag_write_leveling_en:                  1;    // [21:21]
    LwU8  DERR_sample_count:                       6;    // [27:22]
    LwU8  flag_dca_en:                             1;    // [28:28]
    LwU8  RESERVED:                                3;    // [31:29]
} BOOT_TRAINING_HBM_MISC1;

typedef struct __attribute__((__packed__)) BOOT_TRAINING_HBM_TRIM_OFFSET {
    LwU16 rsvd1:               9; // [8:0]
    LwU8  trim_offset_ib:      6; // [14:9]
    LwU8  trim_offset_ob:      6; // [20:15]
    LwU16 reserved:           11; // [31:21]
} BOOT_TRAINING_HBM_TRIM_OFFSET;

#endif

typedef struct __attribute__((__packed__)) {
    db Version;                     // BOOT_TRAINING_TABLE_1X_VERSION     ; Boot Time Training Table Structure Version
    dw StructSize;                  // sizeof MemBootTrainTable           ; Size of Boot Time Training Table Structure
    db Reserved;                    // 0                                  ; Reserved for expansion of header-data like entry size/count
    BOOT_TRAINING_FLAGS MemBootTrainTblFlags;

    // Field definitions are matching the //hw manual entries and should directly resolve to a LW_PFB_FBPA_... registers
    dd GEN_CTRL;
    dd GEN_TIMING;
    dd GEN_TIMING2;
    dd GEN_TIMING3;
    dd ADDR_TADR_CTRL;
    dd ADDR_CTRL;
    dd ADDR_CORS_CTRL;
    dd ADDR_FINE_CTRL;
    dd WCK_EDC_LIMIT;
    dd WCK_INTRPLTR_CTRL_EXT;
    dd WCK_INTRPLTR_CTRL;
    dd READ_CHAR_BANK_CTRL;
    dd READ_CHAR_BANK_CTRL2;
    dd READ_RW_VREF_CTRL;
    dd READ_CTRL;
    dd READ_RW_BRLSHFT_QUICK_CTRL;
    dd READ_RW_CORS_INTRPLTR_CTRL;
    dd READ_RW_FINE_INTRPLTR_CTRL;
    dd READ_RW_ERR_LIMIT;
    dd READ_PATRAM;
    dd READ_ADR0;
    dd READ_ADR1;
    dd READ_PATTERN_PTR0;
    dd READ_PATTERN_PTR1;
    dd WRITE_RW_WR_VREF_CTRL;
    dd WRITE_RW_BRLSHFT_QUICK_CTRL;
    dd WRITE_RW_CORS_INTRPLTR_CTRL;
    dd WRITE_RW_FINE_INTRPLTR_CTRL;
    dd WRITE_RW_ERR_LIMIT;
    dd WRITE_PATRAM;
    dd WRITE_CHAR_BANK_CTRL;
    dd WRITE_CHAR_BANK_CTRL2;
    dd WRITE_ADR0;
    dd WRITE_ADR1;
    dd WRITE_PATTERN_PTR0;
    dd WRITE_PATTERN_PTR1;
    dd WRITE_CTRL2;
    dd MISC_EDC_CTRL;
    dd MISC_CTRL;
    dd CHAR_CTRL;
    dd TRAINING_PATRAM_PRBS_SEED[MAX_TRAINING_PATRAM_PRBS_SEEDS];
    dd READ_TRAINING_DFE;
    dd WRITE_TRAINING_DFE;
    BOOT_TRAINING_SETTLE_TIME SETTLE_TIME;
    dd FBIO_EDC_TRACKING;
    BOOT_TRAINING_EDC_VREF_TRACKING EDC_VREF_TRACKING;
    BOOT_TRAINING_EDC_MISC_SETTLE EDC_MISC_SETTLE;
    BOOT_TRAINING_BKV_OFFSET BKV_OFFSET;
    dd READ_TRAINING_VREF;
    dd WRITE_TRAINING_VREF;
    dd EDC_TRACKING_RDWR_GAIN;
    dd EDC_TRACKING_RDWR_GAIN1;
    dd EDC_TRACKING_RDWR_GAIN2;
    dd EDC_TRACKING_RDWR_GAIN3;
    dd EDC_TRACKING_RDWR_GAIN4;
    dd EDC_TRACKING_RDWR_GAIN5;
    dd EDC_TRACKING_RDWR_GAIN6;
    dd EDC_TRACKING_RDWR_GAIN7;
    dd FBP_SUB;
    dd EDC_PRBS;
    // placeholder for ne data - Bug http://lwbugs/2101908
    dd SPAREDATA;
    // new field additions for Ampere - Bug http://lwbugs/2810693
    BOOT_TRAINING_PI_OFFSET_TRAINING PI_Offset_Training_R;
    BOOT_TRAINING_PI_OFFSET_TRAINING PI_Offset_Training_W;
    BOOT_TRAINING_VREF_CONTROL VREF_DFE_1;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))
    BOOT_TRAINING_HBM_TRIM_OFFSET VREF_DFE_2;
#else
    BOOT_TRAINING_VREF_OFFSET1 VREF_DFE_2;
#endif
    BOOT_TRAINING_VREF_OFFSET2 VREF_DFE_3;
// newly added for GH100, bug 3236636, additions for Hopper HBM
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X)) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
    dd HBM_CHAR;
    dd READ_CHAR_BURST;
    dd WRITE_CHAR_BURST;
    dd WRITE_CHAR_TURN;
    BOOT_TRAINING_HBM_TRAINING HBM_TRAINING_READ_RDQS;
// pattern load freq, rdqs training offset, DERR sample count, flag_dca_en
    BOOT_TRAINING_HBM_MISC1 HBM_TRAINING_MISC1;
    BOOT_TRAINING_HBM_TRAINING HBM_TRAINING_WRITE_LEVEL;    // WRITE_LEVELLING_SWEEP
    dd DERR_READ_DELAY;
// periodic training interval, Oscillator run time, Ratio for ESC reads
    BOOT_TRAINING_HBM_PERIODIC_TRAINING PeriodicTr;
    dd SPARE1;
    dd SPARE2;
#endif
} BOOT_TRAINING_TABLE;


typedef struct {
    MemTrainBaseEntry* 	pMTBEpRdDly;
    MemTrainBaseEntry*  pMTBEpWrDly;
    MemTrainBaseEntry*  pMTBEpDatVref;
    MemTrainBaseEntry*  pMTBEpDqsVref;
    MemTrainBaseEntry*  pMTBEpAddrDly;
    MemTrainBaseEntry*  pMTBEpQuseDly;
    MemTrainBaseEntry*  pMTBEpRdDbi;
    MemTrainBaseEntry*  pMTBEpWrDbi;
    MemTrainBaseEntry*  pMTBEpRdEdc;
    MemTrainBaseEntry*  pMTBEpWrEdc;
    MemTrainStrapEntry* pMTSEpRdDly;
    MemTrainStrapEntry* pMTSEpWrDly;
    MemTrainStrapEntry* pMTSEpDatVref;
    MemTrainStrapEntry* pMTSEDqsVref;
    MemTrainStrapEntry* pMTSEpAddrDly;
    MemTrainStrapEntry* pMTSEpQuseDly;
    MemTrainStrapEntry* pMTSEpRdDbi;
    MemTrainStrapEntry* pMTSEpWrDbi;
    MemTrainStrapEntry* pMTSEpRdEdc;
    MemTrainStrapEntry* pMTSEpWrEdc;
    #if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
      MemTrainPatternEntry11* pMTPEp_save[10];
    #else
      MemTrainPatternEntry10* pMTPEp_save[10];
    #endif
}MemTrainingPatternEntry;

typedef struct {
    LwU32 WrDatPatternDp[200];
    LwU32 WrDatPatternDpContd[200];
    LwU32 RdDatPatternDp[200];
    LwU32 RdDatPatternDpContd[200];
    LwU32 AddrPattern[256];
}MemLocalPatternBuffer;


typedef struct {
    db EDCTrackingRdGainInitial;
    db EDCTrackingRdGainFinal;
    db EDCTrackingWrGainInitial;
    db EDCTrackingWrGainFinal;
    db VrefTrackingGainInitial;
    db VrefTrackingGainFinal;
}EdcTrackingGainTable;


// GPIO Table structure defines
// https://rmopengrok.lwpu.com/source/xref/sw/main/bios/core92/inc/dcbdef.inc
//


typedef struct __attribute__((__packed__)) DCB40_GPIO_Header
{
    db version;          // 41h             ; Version 4.1
#define DCB40_GPIO_TABLE_VERSION 0x41
    db header_size;      // SIZE GPIO_HEAD  ; # of bytes in the header
    db entry_count;      // ?               ; Number of GPIO entries
    db entry_size;       // SIZE DEV_DCB_GPIO_EntryLO + SIZE DEV_DCB_GPIO_EntryHI  ; Size of each GPIO entry
    dw extern_gpio_tbl;  // ?               ; Pointer to External GPIO Assignment Table
} DCB40_GPIO_Header;


// https://wiki.lwpu.com/engwiki/index.php/Resman/Display_and_DCB_Dolwmentation/DCB_4.x_Specification#GPIO_Assignment_Table_Entry
typedef struct __attribute__((__packed__)) GPIO41Entry
{
    LwU8 GPIONum:         6;     //  The GPIO number associated with this entry.
#define LW_GPIO_NUM_UNDEFINED 0xFF
    LwU8 IOType:          1;     //  Normal or Dedicated Lockpin
    LwU8 InitState:       1;     //  0=Off or 1=On
    LwU8 Function:        8;     //  Function#, backward compatible with version 4.0
#define LW_GPIO_FUNC_FBVDDQ_SELECT  0x18
#define LW_GPIO_FUNC_FBVREF_SELECT  0x2E
    LwU8 OutputHWSel:     8;     //  value written to HW for Output function
    LwU8 InputHWSel:      5;     //  value written to HW for Input  function
    LwU8 GSyncHeader:     1;     //  bug 572312; RM responsible for discerning RasterSync or FlipLock
    LwU8 Reserved:        1;     //
    LwU8 PWM:             1;     //  GPIO is used as Pulse Width Modulate function
    LwU8 LockPinNum:      4;     //  must be 0xF if not a Dedicated Lockpin
    LwU8 OffData:         1;     //  Data   state for Logical Off of this Function
    LwU8 OffEnable:       1;     //  Enable state for Logical Off of this Function
    LwU8 OnData:          1;     //  Data   state for Logical On  of this Function
    LwU8 OnEnable:        1;     //  Enable state for Logical On  of this Function
    LwU8 SCICopy:         1;     //  Enable PMGR<->SCI state copy of this Function
    LwU8 virtual_3v3:     1;
    LwU8 Reserved_42_47:  6;
} GPIO41Entry;



//----------------------------------------------------------------------------------------------
// storage format definitions for the vbios content in the fbfalcon ucode.
//----------------------------------------------------------------------------------------------

//
// global sturct to keep pointers to vbios data
// Note: These Structures are used in the testing environment under //hw/lwgpu/ip/memsys/fb/2.0/defs/public/ucode/system_src.
//       Any change/addition or order modification needs to be replicated in that environment by moving this file over ot the
//       /hw elwiornment.
//


// access definitions to the the bios tables

#define BIOS_ACCESS_MEMORY_DENSITY    gTT.pMIEp->mie3116.MIEDensity
#define BIOS_ACCESS_MEMORY_VENDOR_ID  gTT.pMIEp->mie1500.MIEVendorID

#define isDensity16GB  (BIOS_ACCESS_MEMORY_DENSITY == MEMINFO_ENTRY_DENSITY_16G)
#define isMicron16GB  ((BIOS_ACCESS_MEMORY_VENDOR_ID == MEMINFO_ENTRY_VENDORID_MICRON) && \
                       (BIOS_ACCESS_MEMORY_DENSITY == MEMINFO_ENTRY_DENSITY_16G))

typedef struct {

    // When pre parsing the vdpa entry we can no longer keep pointers to its records to access
    // during processing as they will be overwritten. All lookup definitions moved to the mini dirt table.

    FW_IMAGE_DESCRIPTOR* fw_descriptor;
    FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR vdpaTable;

    LwU8 strap;
    MemInfoEntry* mIEp;
    MemInfoHeader* mIHp;

    PLLInfo5xHeader* pIHp;
    PLLInfo5xEntry* pIEp;              // points to the HBM pll entry
    PLLInfo5xEntry* pIEp_refmpll;
    PLLInfo5xEntry* pIEp_mpll;

    PerfMemClk11Header *pMCHp;
    PerfMemClk11BaseEntry *pMCBEp;
    LwU8 pMCBEentryCount;
    LwU8 pMCBEentrySize;
    LwU8 pMCBEstrapCount;
    LwU8 pMCBEstrapSize;

    PerfMemTweak2xHeader* pMTHp;
    PerfMemTweak2xBaseEntry* pMTBEp;   // points to the first entry
    LwU32 pMTBEentryCount;
    LwU32 pMTBEentrySize;
    LwU32 pMTBEextEntryCount;
    LwU32 pMTBEextEntrySize;

    BOOT_TRAINING_TABLE* pBootTrainingTable;

    Perf6xHeader* pPTH;
    LwU16 nominalFrequencyP0;
    LwU16 nominalFrequencyP8;

    MemTrainHeader* pMemTHp;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
    MemTrainPattern11Header* pMemPatHp;
#else
    MemTrainPattern10Header* pMemPatHp;
#endif
    MemTrainingPatternEntry instMTPEp;
    BITInternalUseOnlyV2 *pIUO;

#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_GPIO_TABLE))
    DCB40_GPIO_Header* pGPIOHeader;
    GPIO41Entry* pFirstGpioEntry;
    LwU8 gGPIO_PIN_FBVDDQ;
    LwU8 gGPIO_PIN_VREF;
#endif

} BIOS_TABLE_STRUCT;

// HBM uses a different table defintions due to gv100's legacy, this will have to update with the
// next depoloyed hbm binary (Ampere is not released and that effort will be deferred)
#if (FBFALCONCFG_CHIP_ENABLED(GV10X) && FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM2))
typedef struct {
    PLLInfo5xEntry* pIEp;
    PLLInfo5xEntry* pIEp_refmpll;
    PLLInfo5xEntry* pIEp_mpll;
    PerfMemClk11BaseEntry *pMCBEp;
    PerfMemClk11StrapEntry *pMCSEp;
    PerfMemTweak2xBaseEntry* pMTBEp;
    MemInfoEntry *pMIEp;
    BITInternalUseOnlyV2 *pIUO;
} BIOS_TARGET_TABLE;
#else
typedef struct {
    PLLInfo5xEntry* pIEp;
    PLLInfo5xEntry* pIEp_refmpll;
    PLLInfo5xEntry* pIEp_mpll;
    PerfMemClk11BaseEntry  perfMemClkBaseEntry;
    PerfMemClk11StrapEntry perfMemClkStrapEntry;
    PerfMemTweak2xBaseEntry perfMemTweakBaseEntry;
    MemInfoEntry *pMIEp;
    BITInternalUseOnlyV2 *pIUO;
#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_TRAINING_TABLE) || FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT))
    BOOT_TRAINING_TABLE bootTrainingTable;
#endif
} BIOS_TARGET_TABLE;
#endif


#endif // FBFLCN_TABLE_HEADERS_H
