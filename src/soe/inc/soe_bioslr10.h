#ifndef SOE_BIOSLR10_H_INCLUDED
#define SOE_BIOSLR10_H_INCLUDED


/*!
 * @file    soe_bioslr10.h 
 * @copydoc soe_bioslr10.c
 */

#define bios_U008  LwU32
#define bios_U016  LwU32
#define bios_U032  LwU32
#define bios_S008  LwS32
#define bios_S016  LwS32
#define bios_S032  LwS32

/* related to LR10 functions */
#define NUM_NPORT_ENGINE_LR10                   36
#define LWSWITCH_NUM_LINKS_LR10         (NUM_NPORT_ENGINE_LR10)

/* LR10 BIT definitions */
#define BIT_HEADER_ID                     0xB8FF
#define BIT_HEADER_SIGNATURE              0x00544942  // "BIT\0"
#define BIT_HEADER_SIZE_OFFSET            8
#define BIT_HEADER_LATEST_KNOWN_VERSION   0x100

#define PCI_ROM_HEADER_SIZE               0x18
#define PCI_DATA_STRUCT_SIZE              0x1c
#define PCI_ROM_HEADER_PCI_DATA_SIZE      (PCI_ROM_HEADER_SIZE + PCI_DATA_STRUCT_SIZE) // ROM Header + PCI Dat Structure size
#define PCI_EXP_ROM_SIGNATURE             0xaa55
#define PCI_DATA_STRUCT_SIGNATURE         0x52494350 // "PCIR" in dword format

#define LWLINK_CONFIG_DATA_HEADER_VER_20    0x2
#define LWLINK_CONFIG_DATA_HEADER_20_SIZE   8
#define LWLINK_CONFIG_DATA_HEADER_20_FMT    "6b1w"

typedef struct _PCI_DATA_STRUCT
{
    bios_U032       sig;                    //  00h: Signature, the string "PCIR" or LWPU's alternate "NPDS"
    bios_U016       vendorID;               //  04h: Vendor Identification
    bios_U016       deviceID;               //  06h: Device Identification
    bios_U016       deviceListPtr;          //  08h: Device List Pointer
    bios_U016       pciDataStructLen;       //  0Ah: PCI Data Structure Length
    bios_U008       pciDataStructRev;       //  0Ch: PCI Data Structure Revision
    bios_U008       classCode[3];           //  0Dh: Class Code
    bios_U016       imageLen;               //  10h: Image Length (units of 512 bytes)
    bios_U016       vendorRomRev;           //  12h: Revision Level of the Vendor's ROM
    bios_U008       codeType;               //  14h: holds NBSI_OBJ_CODE_TYPE (0x70) and others
    bios_U008       lastImage;              //  15h: Last Image Indicator: bit7=1 is lastImage
    bios_U016       maxRunTimeImageLen;     //  16h: Maximum Run-time Image Length (units of 512 bytes)
    bios_U016       configUtilityCodePtr;   //  18h: Pointer to Configurations Utility Code Header
    bios_U016       CMDTFCLPEntryPointPtr;  //  1Ah: Pointer to DMTF CLP Entry Point
} PCI_DATA_STRUCT, *PPCI_DATA_STRUCT;

#define PCI_DATA_STRUCT_FMT "1d4w4b2w2b3w"


// BIT_TOKEN_LWINIT_PTRS       0x49 // 'I' Initialization Table Pointers
struct BIT_DATA_LWINIT_PTRS_V1
{
   bios_U016 InitScriptTablePtr;      // Init script table pointer
   bios_U016 MacroIndexTablePtr;      // Macro index table pointer
   bios_U016 MacroTablePtr;           // Macro table pointer
   bios_U016 ConditionTablePtr;       // Condition table pointer
   bios_U016 IoConditionTablePtr;     // IO Condition table pointer
   bios_U016 IoFlagConditionTablePtr; // IO Flag Condition table pointer
   bios_U016 InitFunctionTablePtr;    // Init Function table pointer
   bios_U016 VBIOSPrivateTablePtr;    // VBIOS private table pointer
   bios_U016 DataArraysTablePtr;      // Data arrays table pointer
   bios_U016 PCIESettingsScriptPtr;   // PCI-E settings script pointer
   bios_U016 DevinitTablesPtr;        // Pointer to tables required by Devinit opcodes
   bios_U016 DevinitTablesSize;       // Size of tables required by Devinit opcodes
   bios_U016 BootScriptsPtr;          // Pointer to Devinit Boot Scripts
   bios_U016 BootScriptsSize;         // Size of Devinit Boot Scripts
   bios_U016 LwlinkConfigDataPtr;     // Pointer to LWLink Config Data
};
#define BIT_DATA_LWINIT_PTRS_V1_30_FMT "15w"
typedef struct BIT_DATA_LWINIT_PTRS_V1 BIT_DATA_LWINIT_PTRS_V1;

#define BIT_TOKEN_BIOSDATA          0x42 // 'B' BIOS Data
#define BIT_TOKEN_LWINIT_PTRS       0x49 // 'I'
#define BIT_TOKEN_INTERNAL_USE      0x69 // 'i' Internal Use Only Data

struct BIT_HEADER_V1_00
{
    bios_U016 Id;            // BMP=0x7FFF/BIT=0xB8FF
    bios_U032 Signature;     // 0x00544942 - BIT Data Structure Signature
    bios_U016 BCD_Version;   // BIT Version - 0x0100 for 1.00
    bios_U008 HeaderSize;    // This version is 12 bytes long
    bios_U008 TokenSize;     // This version has 6 byte long Tokens
    bios_U008 TokenEntries;  // Number of Entries
    bios_U008 HeaderChksum;  // 0 Checksum of the header
};
#define BIT_HEADER_V1_00_FMT "1w1d1w4b"
typedef struct BIT_HEADER_V1_00 BIT_HEADER_V1_00;

struct BIT_TOKEN_V1_00
{
    bios_U008 TokenId;
    bios_U008 DataVersion;
    bios_U016 DataSize;
    bios_U016 DataPtr;
};
#define BIT_TOKEN_V1_00_FMT "2b2w"
typedef struct BIT_TOKEN_V1_00 BIT_TOKEN_V1_00;

#define PCI_VENDOR_ID_LWIDIA            0x10DE

typedef struct _lwlink_Config_Data_Header_20
{
    bios_U008 Version;           // LWLink Config Data Structure version
    bios_U008 HeaderSize;        // Size of header
    bios_U008 BaseEntrySize;
    bios_U008 BaseEntryCount;
    bios_U008 LinkEntrySize;
    bios_U008 LinkEntryCount;
    bios_U016 Reserved;          // Reserved
} LWLINK_CONFIG_DATA_HEADER_20, *PLWLINK_CONFIG_DATA_HEADER_20;

#define LW_LWLINK_VBIOS_PARAM0_LINK                             0:0
#define LW_LWLINK_VBIOS_PARAM0_LINK_ENABLE                      0x0
#define LW_LWLINK_VBIOS_PARAM0_LINK_DISABLE                     0x1
#define LW_LWLINK_VBIOS_PARAM0_ACTIVE_REPEATER                  1:1
#define LW_LWLINK_VBIOS_PARAM0_ACTIVE_REPEATER_NOT_PRESENT      0x0
#define LW_LWLINK_VBIOS_PARAM0_ACTIVE_REPEATER_PRESENT          0x1
#define LW_LWLINK_VBIOS_PARAM0_ACDC_MODE                        2:2
#define LW_LWLINK_VBIOS_PARAM0_ACDC_MODE_DC                     0x0
#define LW_LWLINK_VBIOS_PARAM0_ACDC_MODE_AC                     0x1
#define LW_LWLINK_VBIOS_PARAM0_RECEIVER_DETECT                  3:3
#define LW_LWLINK_VBIOS_PARAM0_RECEIVER_DETECT_DISABLE          0x0
#define LW_LWLINK_VBIOS_PARAM0_RECEIVER_DETECT_ENABLE           0x1
#define LW_LWLINK_VBIOS_PARAM0_RESTORE_PHY_TRAINING             4:4
#define LW_LWLINK_VBIOS_PARAM0_RESTORE_PHY_TRAINING_DISABLE     0x0
#define LW_LWLINK_VBIOS_PARAM0_RESTORE_PHY_TRAINING_ENABLE      0x1
#define LW_LWLINK_VBIOS_PARAM0_SLM                              5:5
#define LW_LWLINK_VBIOS_PARAM0_SLM_DISABLE                      0x0
#define LW_LWLINK_VBIOS_PARAM0_SLM_ENABLE                       0x1
#define LW_LWLINK_VBIOS_PARAM0_L2                               6:6
#define LW_LWLINK_VBIOS_PARAM0_L2_DISABLE                       0x0
#define LW_LWLINK_VBIOS_PARAM0_L2_ENABLE                        0x1
#define LW_LWLINK_VBIOS_PARAM0_RESERVED                         7:7

#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE                        7:0
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_50_00000               0x00
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_16_00000               0x01
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_20_00000               0x02
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_25_00000               0x03
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_25_78125               0x04
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_32_00000               0x05
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_40_00000               0x06
#define LW_LWLINK_VBIOS_PARAM1_LINE_RATE_53_12500               0x07

#define LW_LWLINK_VBIOS_PARAM2_LINE_CODE_MODE                   7:0
#define LW_LWLINK_VBIOS_PARAM2_LINE_CODE_MODE_NRZ               0x00
#define LW_LWLINK_VBIOS_PARAM2_LINE_CODE_MODE_NRZ_128B130       0x01
#define LW_LWLINK_VBIOS_PARAM2_LINE_CODE_MODE_NRZ_PAM4          0x03

#define LW_LWLINK_VBIOS_PARAM3_REFERENCE_CLOCK_MODE                     1:0
#define LW_LWLINK_VBIOS_PARAM3_REFERENCE_CLOCK_MODE_COMMON              0x0
#define LW_LWLINK_VBIOS_PARAM3_REFERENCE_CLOCK_MODE_RSVD                0x1
#define LW_LWLINK_VBIOS_PARAM3_REFERENCE_CLOCK_MODE_NON_COMMON_NO_SS    0x2
#define LW_LWLINK_VBIOS_PARAM3_REFERENCE_CLOCK_MODE_NON_COMMON_SS       0x3

#define LW_LWLINK_VBIOS_PARAM3_RESERVED1                        3:2
#define LW_LWLINK_VBIOS_PARAM3_CLOCK_MODE_BLOCK_CODE            5:4
#define LW_LWLINK_VBIOS_PARAM3_CLOCK_MODE_BLOCK_CODE_OFF        0x0
#define LW_LWLINK_VBIOS_PARAM3_CLOCK_MODE_BLOCK_CODE_ECC96      0x1
#define LW_LWLINK_VBIOS_PARAM3_CLOCK_MODE_BLOCK_CODE_ECC88      0x2
#define LW_LWLINK_VBIOS_PARAM3_RESERVED2                        7:6

#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM                               7:0
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_RSVD                          0x00
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A0_SINGLE_PRESENT             0x01
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A1_PRESENT_ARRAY              0x02
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A2_FINE_GRAINED_EXHAUSTIVE    0x04
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A3_RSVD                       0x08
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A4_FOM_CENTRIOD               0x10
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A5_RSVD                       0x20
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A6_RSVD                       0x40
#define LW_LWLINK_VBIOS_PARAM4_TXTRAIN_OPTIMIZATION_ALGORITHM_A7_RSVD                       0x80

#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_ADJUSTMENT_ALGORITHM                                 4:0
#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_ADJUSTMENT_ALGORITHM_B0_NO_ADJUSTMENT                0x1
#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_ADJUSTMENT_ALGORITHM_B1_FIXED_ADJUSTMENT             0x2
#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_ADJUSTMENT_ALGORITHM_B2_RSVD                         0x4
#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_ADJUSTMENT_ALGORITHM_B3_RSVD                         0x8

#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_FOM_FORMAT                           7:5
#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_FOM_FORMAT_FOM_A                     0x1
#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_FOM_FORMAT_FOM_B                     0x2
#define LW_LWLINK_VBIOS_PARAM5_TXTRAIN_FOM_FORMAT_FOM_C                     0x4

#define LW_LWLINK_VBIOS_PARAM6_TXTRAIN_MINIMUM_TRAIN_TIME_MANTISSA           3:0
#define LW_LWLINK_VBIOS_PARAM6_TXTRAIN_MINIMUM_TRAIN_TIME_EXPONENT           7:4

#define LWLINK_CONFIG_DATA_LINKENTRY_FMT "7b"
// Version 2.0 Link Entry and Base Entry
typedef struct _lwlink_config_data_linkentry_20
{
    // VBIOS configuration Data
     LwU8  lwLinkparam0;
     LwU8  lwLinkparam1;
     LwU8  lwLinkparam2;
     LwU8  lwLinkparam3;
     LwU8  lwLinkparam4;
     LwU8  lwLinkparam5;
     LwU8  lwLinkparam6;
} LWLINK_CONFIG_DATA_LINKENTRY;


// Union of different VBIOS configuration table formats
typedef union __lwlink_Config_Data_Header
{
    LWLINK_CONFIG_DATA_HEADER_20 ver_20;
} LWLINK_CONFIG_DATA_HEADER, *PLWLINK_CONFIG_DATA_HEADER;

typedef struct _lwlink_vbios_config_data_linkentry_20
{
    // VBIOS configuration Data
     bios_U008  lwLinkparam0;
     bios_U008  lwLinkparam1;
     bios_U008  lwLinkparam2;
     bios_U008  lwLinkparam3;
     bios_U008  lwLinkparam4;
     bios_U008  lwLinkparam5;
     bios_U008  lwLinkparam6;
} LWLINK_VBIOS_CONFIG_DATA_LINKENTRY, *PLWLINK_VBIOS_CONFIG_DATA_LINKENTRY;

//
// LWSwitch driver structures
//

#define LWSWITCH_MAX_LINK_COUNT                         64
#define LWSWITCH_NUM_BIOS_LWLINK_CONFIG_BASE_ENTRY      12

typedef struct
{
    LWLINK_CONFIG_DATA_LINKENTRY link_vbios_entry[1][LWSWITCH_MAX_LINK_COUNT];
    LwU32                        identified_Link_entries[1];
    LwU32                        link_base_entry_assigned;
    LwU64                        vbios_disabled_link_mask;

    LwU32                        bit_address;
    LwU32                        pci_image_address;
    LwU32                        lwlink_config_table_address;
} LWSWITCH_BIOS_LWLINK_CONFIG;

#endif // SOE_BIOSLR10_H_INCLUDED

