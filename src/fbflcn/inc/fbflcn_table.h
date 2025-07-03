/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_TABLE_H
#define FBFLCN_TABLE_H

// from /chips_a/source/xref/chips_a/sdk/lwpu/inc/
#include <lwtypes.h>
#include "lwuproc.h"
#include "config/fbfalcon-config.h"

#include "fbflcn_defines.h"
#include "fbflcn_table_headers.h"

// FIXME: the Lw64 define is used in some of the bios headers but not directly in my code
//        there doesn't seem any additional need to include more header files from bios location
#define Lw64 LwU64

#include "vbios/ucode_interface.h"
#include "vbios/ucode_postcodes.h"



// sw/main/bios/core88/chip/gv100/pmu/inc/dev_gc6_island_addendum.h
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"



// PLL Information TABLE ID's
// full definition: https://wiki.lwpu.com/engwiki/index.php/Resman/PLLs/PLL_Information_Table_5.0_Specification#PLL_Information_Table_Entry
#define PLL_INFORMATION_TABLE_HBMPLL_ID 0xe
#define PLL_INFORMATION_TABLE_REFMPLL_ID 0xc
#define PLL_INFORMATION_TABLE_MPLL_ID 0x4

//
// DIRT-ID's for lookup of tables in VDPA table:
//    All tables to be accessed in the fbfalcon binary need to be accessed through this structure,
//    Defines are coming form the bios definitions and need to be replicated here.
//    Definitions can be found on this wiki:
//    https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Data_ID_Reference_Table
#define FW_MEMORY_PLL_INFORMATION_TABLE_ID 0x02
#define FW_MEMORY_INFORMATION_TABLE_ID 0x0A
#define FW_MEMORY_PERFORMANCE_TABLE_ID 0x0E
#define FW_MEMORY_CLOCK_TABLE_ID 0x0F
#define FW_MEMORY_TWEAK_TABLE_ID 0x10
#define FW_MEMORY_BOOT_TRAINING_TABLE_ID 0x3F
#define FW_MEMORY_TRAINING_TABLE_ID 0x0B
#define FW_MEMORY_TRAINING_PATTERN_TABLE_ID 0x0C
#define FW_BIT_INTERNAL_USE_ONLY 0x2E
#define LW_GFW_DIRT_GPIO_TABLE 0x85

// TODO: this defintions are comming from common/cert20/ucode_postcodes.h (lwrrently in review)
// they define the format we want to us in LW_PBUS_VBIOS_SCRATCH(..)
#define LW_UCODE_POST_CODE_FBFALCON_FRTS_REG11           0x11     // scratch register offset. LW_PBUS_VBIOS_SCRATCH(17)
#define LW_UCODE_POST_CODE_FBFALCON_FRTS_ERR_CODE        15:0     // error code generate by fbFalcon when processing VBIOS table
#define LW_UCODE_POST_CODE_FBFALCON_FRTS_PROG_CODE       23:16    // progress code generate by fbFalcon when processing VBIOS table

//typedef enum
//{
//	     LW_UCODE_FBFALCON_FRTS_PROG_NOT_STARTED = 0,                            // 0x0 - fbFalcon processing VBIOS DIRT table not started
//	     LW_UCODE_FBFALCON_FRTS_PROG_START,                                      // 0x1 - fbFalcon processing VBIOS DIRT table started
//	     LW_UCODE_FBFALCON_FRTS_PROG_MEMORY_INFORMATION_TABLE_PARSED,            // 0x2 - fbFalcon completed MemInfoTbl Parsing
//	     LW_UCODE_FBFALCON_FRTS_PROG_MEMORY_TRAINING_TABLE_PARSED,               // 0x3 - fbFalcon completed MemTrnTbl Parsing
//	     LW_UCODE_FBFALCON_FRTS_PROG_MEMORY_TRAINING_PATTERN_TABLE_PARSED,       // 0x4 - fbFalcon completed MemTrnPatTbl Parsing
//	     // more progress code here..
//} LW_UCODE_FBFALCON_FRTS_PROG_CODE;

typedef enum
 {
     LW_UCODE_FBFALCON_FRTS_ERR_NO_ERROR_NOT_USED = 0,
     LW_UCODE_FBFALCON_FRTS_ERR_MEMORY_INFORMATION_TABLE_NOT_FOUND,                   // 0x1
     LW_UCODE_FBFALCON_FRTS_ERR_MEMORY_INFORMATION_TABLE_WRONG_VERSION,               // 0x2
     LW_UCODE_FBFALCON_FRTS_ERR_MEMORY_TRAINING_TABLE_NOT_FOUND,                      // 0x3
     LW_UCODE_FBFALCON_FRTS_ERR_MEMORY_TRAINING_TABLE_WRONG_VERSION,                  // 0x4
     LW_UCODE_FBFALCON_FRTS_ERR_MEMORY_TRAINING_PATTERN_TABLE_NOT_FOUND,              // 0x5
     LW_UCODE_FBFALCON_FRTS_ERR_MEMORY_TRAINING_PATTERN_TABLE_WRONG_VERSION,          // 0x6
     // More error code added here..
 } LW_UCODE_FBFALCON_FRTS_ERR_CODE;


// more definitions   this should come from common/cert20/ucode_interface.h
//
// Note:  This secure input structure for FRTS uses secure scratch registers.
//        The secure scratch registers defines for FRTS are in //sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref/volta/gv100/dev_gc6_island_addendum.h
//
//#define  LW_IN_CMD_FRTS_SEC_CTRL_VERSION                                3:0   // Version field of IN_CMD_FRTS_SEC
//#define  LW_IN_CMD_FRTS_SEC_CTRL_VERSION_1                              1     // Version 1
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE                             7:4   // Media Type field of IN_CMD_FRTS_SEC
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_FB                          0     // FRTS copy VBIOS data back to FB
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_SYSMEM_NONCOHERENT          1     // FRTS copy VBIOS data back to _NONCOHERENT_SYSMEM
#define  LW_IN_CMD_FRTS_SEC_CTRL_MEDIA_TYPE_SYSMEM_COHERENT             2     // FRTS copy VBIOS data back to _COHERENT_SYSMEM,
 #if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
   #define  FBFLCN_MAX_PATRAM_BUFFER_SIZE                                  96
 #else 
   #define  FBFLCN_MAX_PATRAM_BUFFER_SIZE                                  256
 #endif  

// these will come from sw/main/bios/common/cert20/ucode_postcodes.h
// IMPORTANT these are out of an enum, once they show up I'm not sure if the compiler will flag this as multiple definitions
//#define LW_UCODE_ERR_CODE_CERT21_CERT_VERSION_UNEXPECTED 0x7e
//#define LW_UCODE_ERR_CODE_CERT21_FRTS_DEVINIT_NOT_COMPLETED 0x7f
//#define LW_UCODE_ERR_CODE_CERT21_FRTS_SELWRE_INPUT_STRUCT_VERSION_UNEXPECTED 0x80
//#define LW_UCODE_ERR_CODE_CERT21_FRTS_DMA_UNEXPECTED_MEDIA_TYPE 0x81


// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

//#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_SIZE             19:0
//#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_INSTANCE         23:20
//#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE        31:24


#define MAX_ADDR_TRAINING_BUFFER_SIZE 256


typedef struct sMiniDirtTableEntry {
    LwU32 dirtId;
    LwU32 offset_start;
    LwU32 size;
} sMiniDirtTableEntry;
typedef sMiniDirtTableEntry* pMiniDirtTableEntry;


extern BIOS_TABLE_STRUCT gBiosTable;

extern BIOS_TARGET_TABLE gTT;

extern LwBool gTableCheckLog;


//*****************************************************************************
//   FUNCTIONS
//*****************************************************************************

LwBool isPrivSecEnabled(void)
    GCC_ATTRIB_SECTION("init", "initTable");

LwU32 initTable(void)
    GCC_ATTRIB_SECTION("init", "initTable");

void loadFb2DMem(LwU32 from, LwU32 to, LwU32 size)
    GCC_ATTRIB_SECTION("init", "loadTable");

FBFLCN_VDPA_ENTRY_INT_BLK_BASE_SPTR findDirtIdTableEntry(LwU8 dirt_id)
	GCC_ATTRIB_SECTION("init", "findDirtTableEntry");

LwU32 loadTableSegment (LwU32 from, LwU32 *to, LwU32 size)
	GCC_ATTRIB_SECTION("init", "loadTableSegment");


#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_PREPARSING_VDPA))

pMiniDirtTableEntry findMiniDirtIdEntry(LwU8 dirt_id)
    GCC_ATTRIB_SECTION("init", "findMiniDirtIdEntry");

#endif

void loadVDPATable(LwU32*)
    GCC_ATTRIB_SECTION("init", "loadVDPATable");



void loadMemoryInformationTable(LwU32*)
    GCC_ATTRIB_SECTION("init", "loadMemoryInformationTable");

void loadPLLInformationTable(LwU32*)
    GCC_ATTRIB_SECTION("init", "loadPLLInformationTable");
void loadPLLInformationEntries(void)
    GCC_ATTRIB_SECTION("init", "loadPLLInformationEntries");

void loadMemoryClockTable(LwU32*)
    GCC_ATTRIB_SECTION("init", "loadMemoryClockTable");

void sanityCheckMemTweakTable(void)
    GCC_ATTRIB_SECTION("init", "sanityCheckMemTweakTable");

void loadMemTweakTable(LwU32*)
    GCC_ATTRIB_SECTION("init", "loadMemTweakTable");

void loadVBIOSInternalUseOnly(LwU32*)
    GCC_ATTRIB_SECTION("init", "loadVBIOSInternalUseOnly");


#if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_TABLE_SUPPORT_PERFORMANCE_TABLE))

void loadPerformanceTable(LwU32*)
    GCC_ATTRIB_SECTION("init", "loadPerformanceTable");
LW_STATUS loadNominalFrequencies(void)
    GCC_ATTRIB_SECTION("init", "loadNominalFrequencies");
#endif


#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_GDDR) || FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X))

void LoadTrainingPatram(void)
    GCC_ATTRIB_SECTION("init", "LoadTrainingPatram");
void parseTrainingPatramTable(void)
    GCC_ATTRIB_SECTION("init", "parseTrainingPatramTable");

LwU32 LoadAddrTrainingPatram(void)
#if (!(FBFALCONCFG_CHIP_ENABLED(GA10X)))
    GCC_ATTRIB_SECTION("resident", "LoadAddrTrainingPatram");
#else
    GCC_ATTRIB_SECTION("init", "LoadAddrTrainingPatram");
#endif

void FuncLoadRdPatterns(LwU32)
    GCC_ATTRIB_SECTION("resident", "FuncLoadRdPatterns");

void FuncLoadWrPatterns(LwU32)
    GCC_ATTRIB_SECTION("resident", "FuncLoadWrPatterns");

void FuncLoadAdrPatterns(LwU32)
    GCC_ATTRIB_SECTION("resident", "FuncLoadAdrPatterns");

void FuncLoadG6SimAdrPattern(void)
    GCC_ATTRIB_SECTION("resident", "FuncLoadG6SimAdrPattern");

#if (!(FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X)))
LwU8 LoadDataTrainingPatram(LwBool)
    GCC_ATTRIB_SECTION("resident", "LoadDataTrainingPatram");
#else
LwU16 LoadDataTrainingPatram(LwBool)
    GCC_ATTRIB_SECTION("resident", "LoadDataTrainingPatram");
#endif


void funcLoadPascalG5AdrPattern(void)
    GCC_ATTRIB_SECTION("resident", "funcLoadPascalG5AdrPattern");

#endif

void decodeStrap(void)
    GCC_ATTRIB_SECTION("init", "decodeStrap");

LW_STATUS loadTable(void)
    GCC_ATTRIB_SECTION("init", "loadTable");

#if (FBFALCONCFG_FEATURE_ENABLED(DECODE_HBM_MEMORY_DEVICE_ID))
void loadStrap(void)
    GCC_ATTRIB_SECTION("init", "loadStrap");
#endif


#endif  // FBFLCN_TABLE_H
