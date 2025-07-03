/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @brief   Structures for the VBIOS BIT internal use table.
 */

#ifndef VBIOS_BIT_H
#define VBIOS_BIT_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ VBIOS Definitions ------------------------------- */
/*!
 * Size of Vbios build GUID from BIT structure
 */
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_GUID_SIZE                       16U

typedef struct GCC_ATTRIB_PACKED()
{
    LwU32 Version;                          // BIOS Binary Version Ex. 5.40.00.01.12 = 0x05400001
    LwU8 OemVersion;                        // OEM Version Number  Ex. 5.40.00.01.12 = 0x12
                                            // OEM cannot override the two fields above
    LwU16 Features;                         // BIOS Compiled Features
    LwU32 P4MagicNumber;                    // Perforce Checkin Number of release.
    LwU16 BoardId;                          // Board identifier for the specific board that
                                            // the BIOS is built on. This field is utilized
                                            // by LWPU utilities.
    LwU16 DACDataPtr;                       // Pointer to DAC related data
    LwU8 bBuildDate[8];                     // VBIOS Build Date
    LwU16 bGPUDevID;                        // GPU Device ID
    LwU32 bFlag1;                           // Internal Flag 1
    LwU32 bFlag2;                           // Internal Flag 2
    LwU16 bPresvrTbl;                       // Preservation Table Pointer
    LwU8 bLWFlashID;                        // LWFlash rev ID
    LwU8 bHierarchyID;                      // Position in a board hierarchy
    LwU8 bFlag3;                            // Internal Flag 3

    LwU16 wAlternateDeviceID;
    LwU32 bKDABuffer;                       // Pointer to the KDA Buffer
    LwU32 bFlag4;                           // HCLK Max Limit (if set)

    LwU8 sLWChipSKU[3];                     // LW Chip SKU Number
    LwU8 sLWChipSKUMod;                     // LW Chip SKU Modifier
    LwU8 sLWProject[4];                     // LW Project (Board) Number
    LwU8 sLWProjectSKU[4];                  // LW Project (Board) SKU Number
    LwU8 sLWCDP[5];                         // Collaborative Design Project Number
    LwU8 sLWProjectSKUMod;                  // LW Project (Board) SKU Modifier
    LwU8 bLWBusinessCycle;                  // LW Business cycle information
    LwU8 bFlag5;                            // Internal Flag 5
    LwU8 bCertFlag;                         // Cert Flag
    LwU8 bFlag6;                            // Internal Flag 6

    LwU16 alternateBoardId;                 // Alternate Board ID for LWFLASH to match image with
    LwU8 buildGuid[VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_GUID_SIZE];  // Build-Time GUID
    LwU32 pDevidOverrideListPtr;            // Pointer to DevID Override List

    LwU16 minimumNetlistRev;                // The minimum value required in LW_PBUS_EMULATION_REV0
    LwU8 minimumRmRevlockLvl;               // The minimum RM revlock level required for use with this VBIOS
    LwU8 lwrrentVbiosRevlockLvl;            // The revlock level of this VBIOS

    LwU32 floorsweepingTablePtr;            // Pointer to floorsweeping table.
    LwU32 istTablePtr;                      // Pointer to IS table
    LwU8 productCycle;                      // Product cycle number
    LwU8 businessUnit;                      // Business unit for the board
    LwU32 selwreBootFeaturesTblPtr;         // Pointer to secure boot features table
} VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2;

#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_13                         13U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_33                         33U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_38                         38U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_44                         44U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_48                         48U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_65                         65U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_66                         66U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_67                         67U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_68                         68U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_70                         70U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_96                         96U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_SIZE_110                       110U

#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG3                               7:0
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG3_COMPUTE_ONLY                 0x02
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG3_ECC_DEFAULT_SETTING_ENABLED  0x08
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG3_VBIOS_TYPE_BIGVBIOS          0x10
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG3_GDDR5_EDC_ENABLED            0x40
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG3_GDDR5_EDC_REPLAY_ENABLED     0x80

// From https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/BIT#BIT_INTERNAL_USE_ONLY_.28Version_2.29
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5                               7:0
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_SKU_SUPPORTS_ECC             0x01
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_EDISON                       0x04
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_SLT                          0x08
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_SMB                          0x10

// defines for GPU Operation modes
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_SKU_OPERATION_MODE_CONFIGURABLE        1:1
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_SKU_OPERATION_MODE_CONFIGURABLE_OFF    0x00
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_SKU_OPERATION_MODE_CONFIGURABLE_ON     0x01
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_DEFAULT_OPERATION_MODE            7:5
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_DEFAULT_OPERATION_MODE_A          0x00
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_DEFAULT_OPERATION_MODE_B          0x01
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_DEFAULT_OPERATION_MODE_C          0x02
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_DEFAULT_OPERATION_MODE_D          0x03
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG5_DEFAULT_OPERATION_MODE_E          0x04

// defines for GC6 Type
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6                                       7:0
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_GC6_TYPE                              3:0
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_GC6_TYPE_NONE                         0x00
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_GC6_TYPE_EC                           0x01  // GC6 EC Sequenced
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_GC6_TYPE_IS                           0x02  // GC6 Island Seqeunced
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_GC6_TYPE_IS_SP                        0x03  // GC6 Island Sequenced + Split rail FB
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_LOCKED_CLOCKS_MODE                    5:4
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_LOCKED_CLOCKS_MODE_SUPPORTED          4:4
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_LOCKED_CLOCKS_MODE_SUPPORTED_DISABLED 0x00
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_LOCKED_CLOCKS_MODE_SUPPORTED_ENABLED  0x01
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_LOCKED_CLOCKS_MODE_ENABLED            5:5
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_LOCKED_CLOCKS_MODE_ENABLED_NO         0x00
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_LOCKED_CLOCKS_MODE_ENABLED_YES        0x01
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_WS_FEATURE_OVERRIDE                   6:6
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_WS_FEATURE_OVERRIDE_NO                0x00
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_WS_FEATURE_OVERRIDE_YES               0x01
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_REDUCE_MINING_PERF                    7:7
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_REDUCE_MINING_PERF_NO                 0x00
#define LW_VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BFLAG6_REDUCE_MINING_PERF_YES                0x01

#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_FEATURES_MOBILE_BIOS         0x0010 // This is a Mobile BIOS (lwrrently always set if panel code included)
// Once there was DFP and TV bits here.  But those were always set and Ian and Mark K. suggested that we
// remove the defines so that we can use them for something else later.  So, while these bits are available
// for Core5, they were never used.  So, I'm pulling them.  For core 6, those bits are not defined.
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_FEATURES_8X14_FONTS_INCLUDED 0x0080 // 8x14 Fonts are included
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_FEATURES_FONTS_COMPRESSED    0x0100 // Fonts are compressed


// Note book Business cycle information found in bLWBusinessCycle above.
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BUSINESS_CYCLE_LW11X                      11U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BUSINESS_CYCLE_LW12X                      12U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BUSINESS_CYCLE_LW13X                      13U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BUSINESS_CYCLE_LW14X                      14U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BUSINESS_CYCLE_LW14X_REFRESH              20U
#define VBIOS_BIT_INTERNAL_USE_ONLY_TABLE_V2_BUSINESS_CYCLE_LW14X_LATE_REFRESH         21U

/* ------------------------ Types Definitions ------------------------------- */

#endif // VBIOS_BIT_H
