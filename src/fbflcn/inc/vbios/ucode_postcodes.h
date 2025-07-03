/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef UCODE_POSTCODES_H
#define UCODE_POSTCODES_H


/* ------------ uCode POST for fwseclic  --------------------------------
  fwseclic uCode POST Code has following parts:
      1.  Error Code for HULK Processing                                                  LW_PBUS_VBIOS_SCRATCH(5)  bit 9:0
      2.  Error Code for BIOS Cert Verificaiton                                           LW_PBUS_VBIOS_SCRATCH(5)  bit 25:16
      3.  Error Code for UGPU License processing.                                         LW_PBUS_VBIOS_SCRATCH(6)  bit 9:0
      4.  uCode exelwtion Progress Code, recording fwseclic uCode exelwtion process.      LW_PBUS_VBIOS_SCRATCH(5)  bit 15:12
      5.  uCode feature exelwtion code, recording exelwted features in the uCode.         LW_PBUS_VBIOS_SCRATCH(5)  bit 31:28

  This set of defines is used by fwseclic to support uCode POST Code.
  Scratch registers used are defined in //src/sw/main/bios/core84/etc/scratch.xml
------------------------------------------------------------------------------------------*/
#define LW_UCODE_POST_CODE_FWSECLIC_BASE                             LW_PBUS_VBIOS_SCRATCH
#define LW_UCODE_POST_CODE_FWSECLIC_REG                                       0x5      //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(5)
#define LW_UCODE_POST_CODE_FWSECLIC_HULK_ERR_CODE                             9:0      //  error code, for HULK verification and Processing. Defined in LW_UCODE_ERR_CODE_TYPE
#define LW_UCODE_POST_CODE_FWSECLIC_RSVD1                                   11:10      //  Reserved area 1
#define LW_UCODE_POST_CODE_FWSECLIC_PROG                                    15:12      //  fwseclic uCode exelwtion progress code
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_NOT_STARTED                  0x00000000
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_STARTED                      0x00000001
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_IN_SELWREMODE                0x00000002
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_HULK_PROCESS_COMPLETE        0x00000003
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_VBIOS_VERIFY_COMPLETE        0x00000004
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_EXIT_SELWREMODE              0x00000005
#define LW_UCODE_POST_CODE_FWSECLIC_SET_GPU_DEVICE_ID_COMPLETE        0x00000006
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_COMPLETE                     0x00000008
#define LW_UCODE_POST_CODE_FWSECLIC_PROG_ABORTED                      0x00000009
#define LW_UCODE_POST_CODE_FWSECLIC_BIOSCERT_ERR_CODE                      25:16        //  error code, for BIOS Cert verification. defined in LW_UCODE_ERR_CODE_TYPE
#define LW_UCODE_POST_CODE_FWSECLIC_RSVD2                                  27:26        //  Reserved area 2
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT                                   31:28        //  fwseclic feature exelwtion code field 0
                                                                                        //  Each bit defined as a feature. fwseclic sets the bit when feature has been ilwoked.
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_BIOSCERT                          28:28        //  BIOS Cert feature has been exelwted.
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_BIOSCERT_YES                 0x00000001
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_HULK                              29:29        //  HULK License processing has been been exelwted.
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_HULK_YES                     0x00000001
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_UGPU                              30:30        //  UGPU License processing has been been exelwted.
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_UGPU_YES                     0x00000001
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_DEVID_OVERRIDE                    31:31        // Devid Override (List | Variant ROM) has been exelwted.
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_DEVID_OVERRIDE_YES           0x00000001

#define LW_UCODE_POST_CODE_FWSECLIC_REG1                                     0x6        //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(6)
#define LW_UCODE_POST_CODE_FWSECLIC_UGPU_ERR_CODE                            9:0        //  error code, for HULK Process verification. defined in LW_UCODE_ERR_CODE_TYPE

/* -------------- Return Error Codes for each command handler --------------*/
// Note:  The keyword in error codes:  CERT2x and BCRT2x all refer to BIOS Cert Block Version 2.x in BIOS Cert project.
//                                     CERT refer to License/Certificate,  including HULK, BIOS Cert, Lwflash and others used License/Certificate
//        To make things clear, for error codes related to BIOS Cert Block,  newer error code changed to use BCRT2x keyword only

typedef enum
{
        LW_UCODE_ERR_CODE_CMD_NOERROR = 0,
        LW_UCODE_ERR_CODE_CMD_TIMEOUT,                                        // 0x1
        LW_UCODE_ERR_CODE_CMD_DEPENDENCY,                                     // 0x2
        LW_UCODE_ERR_CODE_CMD_EID_RD_ERROR,                                   // 0x3
        LW_UCODE_ERR_CODE_CMD_ERD_BUF_WR_ERROR,                               // 0x4
        LW_UCODE_ERR_CODE_CMD_EWR_BUF_RD_ERROR,                               // 0x5
        LW_UCODE_ERR_CODE_CMD_UNSUPPORTED_GPU,                                // 0x6
        LW_UCODE_ERR_CODE_CMD_UNSUPPORTED_COMMAND,                            // 0x7
        LW_UCODE_ERR_CODE_CMD_UNSUPPORTED_PARAMETER,                          // 0x8
        LW_UCODE_ERR_CODE_CMD_SELWRE_REV_LOCK_VIOLATION,                      // 0x9
        LW_UCODE_ERR_CODE_LOAD_VBIOS_VERIFY_UCODE_FAIL,                       // 0xA
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_DEBUG_FUSE_BOARD,                  // 0xB
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_DEVID_FAIL,                        // 0xC
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_CERT_NOT_FOUND,                    // 0xD
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_CERT_PARSE_FAIL,                   // 0xE
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_CERT_VERIFY_FAIL,                  // 0xF
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_HAT_FAIL,                          // 0x10
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_BIOS_SIG_FAIL,                     // 0x11
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_HULK_INIT_FAIL,                    // 0x12
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_HULK_KA_NOT_FOUND,                 // 0x13
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_HULK_TYPE_ILWALID,                 // 0x14
        LW_UCODE_ERR_CODE_CMD_VBIOS_VERIFY_HULK_SIG_ILWALID,                  // 0x15
        LW_UCODE_ERR_CODE_CERT_UNKNOWN_ERROR,                                 // 0x16
        LW_UCODE_ERR_CODE_CERT_EXT_NOT_FOUND,                                 // 0x17
        LW_UCODE_ERR_CODE_CERT_SIGNATURE_NOT_FOUND,                           // 0x18
        LW_UCODE_ERR_CODE_CERT_RSA1K_SIGNATURE_ILWALID,                       // 0x19
        LW_UCODE_ERR_CODE_CERT_EXT_NO_SUB_STRUCT_FOUND,                       // 0x1a
        LW_UCODE_ERR_CODE_CERT_UNSUPPORTED_VERSION,                           // 0x1b
        LW_UCODE_ERR_CODE_CERT_NO_EXTENSION_EXIST,                            // 0x1c
        LW_UCODE_ERR_CODE_CERT_T7QV1_PAYLOAD_SIZE_ERROR,                      // 0x1d
        LW_UCODE_ERR_CODE_CERT_T7_SW_FEATURE_PAYLOAD_SIZE_ERROR,              // 0x1e
        LW_UCODE_ERR_CODE_CERT_T7_UNSUPPORTED_HW_STRUCT_VERSION,              // 0x1f
        LW_UCODE_ERR_CODE_CERT_T7_EXTENSIONS_NUM_EXCEED_LIMIT,                // 0x20
        LW_UCODE_ERR_CODE_CERT_UGPU_PERSONALITY_MIS_MATCH,                    // 0x21
        LW_UCODE_ERR_CODE_CERT_UNKNOWN_HULK_FEATURE,                          // 0x22
        LW_UCODE_ERR_CODE_CERT_HULK_ECID_MISMATCH,                            // 0x23
        LW_UCODE_ERR_CODE_CERT_HULK_ECID_ENCODING_UNKNOWN,                    // 0x24
        LW_UCODE_ERR_CODE_ECID_ENCODING_ALGO_UNKNOWN,                         // 0x25
        LW_UCODE_ERR_CODE_CERT_T7_REG_OVERRIDE_TYPE_UNKNOWN,                  // 0x26
        LW_UCODE_ERR_CODE_LICVERIFY_UNSUPPORTED_LIC_TYPE,                     // 0x27
        LW_UCODE_ERR_CODE_UNSUPPORTED_CONFIG,                                 // 0x28
        LW_UCODE_ERR_CODE_BSI_INFO_BRSS_ILWALID,                              // 0x29
        LW_UCODE_ERR_CODE_IMEM_TO_DMEM_COPY_ILWALID_PARA,                     // 0x2A
        LW_UCODE_ERR_CODE_DERIVED_KEY_TYPE_ILWALID,                           // 0x2B
        LW_UCODE_ERR_CODE_UCODE_NOT_IN_HS_MODE,                               // 0x2C
        LW_UCODE_ERR_CODE_VBIOS_DEVINIT_OFFSETS_ILWALID,                      // 0x2D
        LW_UCODE_ERR_CODE_VBIOS_DEVINIT_SIG_ILWALID,                          // 0x2E  This code used by older uCode.
                                                                              //       Now moved to _DEVINIT_TABLES_SIG_ILWALID and _DEVINIT_SCRIPTS_SIG_ILWALID
                                                                              //       to differentiate devinit script and table signature failure
        LW_UCODE_ERR_CODE_CERT_HULK_DEVID_MISMATCH,                           // 0x2F
        LW_UCODE_ERR_CODE_CERT_HULK_NO_ID_MATCH_FOUND,                        // 0x30
        LW_UCODE_ERR_CODE_CERT_HULK_DATA_BUFFER_TOO_SMALL,                    // 0x31
        LW_UCODE_ERR_CODE_CERT_HULK_INFOROM_NOT_FOUND,                        // 0x32
        LW_UCODE_ERR_CODE_CERT_HULK_INFOROM_UL_GLOB_NOT_FOUND,                // 0x33
        LW_UCODE_ERR_CODE_CERT_HULK_INFOROM_HLK_OBJ_NOT_VALID,                // 0x34
        LW_UCODE_ERR_CODE_CERT_UGPU_LICENSE_PROCESSING_FAILED,                // 0x35
        LW_UCODE_ERR_CODE_UGPU_PROCESSING_FAILED_ILWALID_ULF_OBJECT,          // 0x36
        LW_UCODE_ERR_CODE_UGPU_PROCESSING_FAILED_ILWALID_UPR_OBJECT,          // 0x37
        LW_UCODE_ERR_CODE_CERT20_INTBLK_VDPA_HEADER_ILWALID,                  // 0x38
        LW_UCODE_ERR_CODE_CERT20_INTBLK_INT_SIG_HEADER_ILWALID,               // 0x39
        LW_UCODE_ERR_CODE_CERT20_INTBLK_INT_SIG_CRYPTO_UNDEFINED,             // 0x3a
        LW_UCODE_ERR_CODE_CERT20_VDPA_UNEXPECTED_MAJOR_TYPE,                  // 0x3b
        LW_UCODE_ERR_CODE_CERT20_VDPA_UNEXPECTED_MINOR_TYPE,                  // 0x3c
        LW_UCODE_ERR_CODE_CERT20_VDPA_ENTRY_SIZE_LARGER_THAN_DATA_BUFFER,     // 0x3d
        LW_UCODE_ERR_CODE_CERT20_VDPA_UNEXPECTED_CODE_TYPE,                   // 0x3e
        LW_UCODE_ERR_CODE_CERT20_VDPA_NOT_FINALIZED,                          // 0x3f
        LW_UCODE_ERR_CODE_CERT20_VDPA_SIG_ILWALID,                            // 0x40
        LW_UCODE_ERR_CODE_CERT20_VDPA_ENTRY_NOT_FOUND,                        // 0x41
        LW_UCODE_ERR_CODE_CERT20_VDPA_CERT_INTBLK_MISMATCH,                   // 0x42
        LW_UCODE_ERR_CODE_CERT20_VDPA_ENTRY_FOUND_DATA_MISMATCH,              // 0x43
        LW_UCODE_ERR_CODE_CERT20_VDPA_DATA_ILWALID,                           // 0x44
        LW_UCODE_ERR_CODE_CERT20_VDPA_FLASH_SIZE_LARGER_THAN_EXPECTED,        // 0x45
        LW_UCODE_ERR_CODE_CERT20_VDPA_DEVID_MISMATCH,                         // 0x46
        LW_UCODE_ERR_CODE_VBIOS_DEVINIT_TABLES_SIG_ILWALID,                   // 0x47
        LW_UCODE_ERR_CODE_VBIOS_DEVINIT_SCRIPTS_SIG_ILWALID,                  // 0x48
        LW_UCODE_ERR_CODE_FWSECLIC_PCI_EXP_ROM_NOT_FOUND,                     // 0x49
        LW_UCODE_ERR_CODE_VERIFY_ENG_HULK_LICENSE_NOT_PRESENT,                // 0x4a
        LW_UCODE_ERR_CODE_VERIFY_ENG_HULK_LICENSE_KA_NOT_FOUND,               // 0x4b
        LW_UCODE_ERR_CODE_VERIFY_ENG_HULK_LICENSE_TYPE_ILWALID,               // 0x4c
        LW_UCODE_ERR_CODE_VERIFY_ENG_HULK_3AES_SIG_MISMATCH_WITH_GPU_FUSE,    // 0x4d
        LW_UCODE_ERR_CODE_VERIFY_ENG_HULK_NO_3AES_SIG,                        // 0x4e
        LW_UCODE_ERR_CODE_VERIFY_ENG_HULK_LICENSE_HULK_AES_SIG_ILWALID,       // 0x4f
        LW_UCODE_ERR_CODE_VERIFY_ENG_HULK_LICENSE_LWF_ENG_AES_SIG_ILWALID,    // 0x50
        LW_UCODE_ERR_CODE_CHECK_ERASE_LICENSE_ERASE_DISALLOWED,               // 0x51
        LW_UCODE_ERR_CODE_CMD_PREP_LICENSE_SIZE_OVERFLOW,                     // 0x52
        LW_UCODE_ERR_CODE_CMD_EWR_NO_ERASE_NOT_PERMITTED,                     // 0x53
        LW_UCODE_ERR_CODE_CMD_EWR_NO_VERIFY_NOT_PERMITTED,                    // 0x54
        LW_UCODE_ERR_CODE_CMD_ESE_NOT_PERMITTED,                              // 0x55
        LW_UCODE_ERR_CODE_CMD_ECE_NOT_PERMITTED,                              // 0x56
        LW_UCODE_ERR_CODE_CERT20_VDPA_UNEXPECTED_INSTANCE,                    // 0x57
        LW_UCODE_ERR_CODE_DEVID_MATCH_LIST_MORE_DEVIDS_THAN_BUFFERS,          // 0x58
        LW_UCODE_ERR_CODE_DEVID_MATCH_LIST_SIG_ILWALID,                       // 0x59
        LW_UCODE_ERR_CODE_DEVID_MATCH_LIST_DEVID_MATCH_FAILED,                // 0x5a
        LW_UCODE_ERR_CODE_DEVID_MATCH_LIST_DEVID_NOT_FOR_THE_GPU,             // 0x5b
        LW_UCODE_ERR_CODE_DEVID_MATCH_LIST_DEVID_OUT_OF_HAT_COVERAGE,         // 0x5c
        LW_UCODE_ERR_CODE_PUSH_POLL_DMEM_COPY_BUFFER_OVERFLOW,                // 0x5d
        LW_UCODE_ERR_CODE_PUSH_POLL_DMEM_COPY_DATA_OUT_OF_RANGE,              // 0x5E
        LW_UCODE_ERR_CODE_CERT20_INTBLK_VDPA_BLOCK_OVERSIZE,                  // 0x5F
        LW_UCODE_ERR_CODE_VERIFY_LICENSE_UGPU_SIG_ILWALID,                    // 0x60
        LW_UCODE_ERR_CODE_CERT_HULK_OPCODE_UNDEFINED,                         // 0x61
        LW_UCODE_ERR_CODE_CERT_HULK_REG_READ_BACK_VERIFY_MISMATCH,            // 0x62
        LW_UCODE_ERR_CODE_CERT_HULK_REG_RMW_RBV_READ_BACK_VERIFY_MISMATCH,    // 0x63
        LW_UCODE_ERR_CODE_PRI_TRANSACTION_ERROR,                              // 0x64
        LW_UCODE_ERR_CODE_HULK_INST_IN_SYS_VERIFICATION_FAILED,               // 0x65
        LW_UCODE_ERR_CODE_SEC2MUTEX_DISPATCHER_BRSS_MUTEX_ACQUIRE_FAILED,     // 0x66
        LW_UCODE_ERR_CODE_SEC2MUTEX_DISPATCHER_BRSS_MUTEX_RELEASE_FAILED,     // 0x67
        LW_UCODE_ERR_CODE_SELWRE_DATA_GENERIC_HEADER_NOT_FOUND,               // 0x68
        LW_UCODE_ERR_CODE_SELWRE_DATA_SIGNATURE_ILWALID,                      // 0x69
        LW_UCODE_ERR_CODE_CERT_HULK_STRICT_ID_DEVID_MISMATCH,                 // 0x6a
        LW_UCODE_ERR_CODE_CERT_HULK_STRICT_ID_ECID_MISMATCH,                  // 0x6b
        LW_UCODE_ERR_CODE_CMD_EWR_OK_TO_FLASH_CHECK_FAILED,                   // 0x6c
        LW_UCODE_ERR_CODE_CERT20_LWF_DMEM_SIG_ILWALID,                        // 0x6d
        LW_UCODE_ERR_CODE_HW_SPI_TIMEOUT,                                     // 0x6e
        LW_UCODE_ERR_CODE_COPY_BUFFER_MAX_NUMBER_REACHED,                     // 0x6f
        LW_UCODE_ERR_CODE_PUSH_POLL_DMEM_COPY_BUFFER_NULL_POINTER,            // 0x70
        LW_UCODE_ERR_CODE_FWSECLIC_BRSS_INFO_ILWALID,                         // 0x71
        LW_UCODE_ERR_CODE_FWSECLIC_BRSS_WRITE_FAILED,                         // 0x72
        LW_UCODE_ERR_CODE_CERT21_FMT_HAT_ENTRY_NUMBER_ILWALID,                // 0x73
        LW_UCODE_ERR_CODE_CERT21_FMT_HAT_ENTRY_FOMMATTER_TOO_LONG,            // 0x74
        LW_UCODE_ERR_CODE_CERT21_FMT_FORMATTER_DATA_BLOCK_OVER_SIZE,          // 0x75
        LW_UCODE_ERR_CODE_CERT21_FMT_UNEXPECTED_FORMATTER_TYPE,               // 0x76
        LW_UCODE_ERR_CODE_CERT21_FMT_EXCEED_FORMATTER_LENGTH,                 // 0x77
        LW_UCODE_ERR_CODE_EEPROM_OTP_DEVICE_UNSUPPORTED,                      // 0x78
        LW_UCODE_ERR_CODE_EEPROM_OTP_ERASE_NOT_PRESENT,                       // 0x79
        LW_UCODE_ERR_CODE_EEPROM_OTP_FACTORY_LOCK_NOT_PRESENT,                // 0x7a
        LW_UCODE_ERR_CODE_EEPROM_OTP_FACTORY_REGION_NOT_PRESENT,              // 0x7b
        LW_UCODE_ERR_CODE_EEPROM_OTP_USER_ADDRESS_OUT_OF_RANGE,               // 0x7c
        LW_UCODE_ERR_CODE_EEPROM_OTP_FACTORY_ADDRESS_OUT_OF_RANGE,            // 0x7d
        LW_UCODE_ERR_CODE_CERT21_CERT_VERSION_UNEXPECTED,                     // 0x7e
        LW_UCODE_ERR_CODE_CERT21_FRTS_DEVINIT_NOT_COMPLETED,                  // 0x7f
        LW_UCODE_ERR_CODE_CERT21_FRTS_SELWRE_INPUT_STRUCT_VERSION_UNEXPECTED, // 0x80
        LW_UCODE_ERR_CODE_CERT21_FRTS_DMA_UNEXPECTED_MEDIA_TYPE,              // 0x81
        LW_UCODE_ERR_CODE_PLAY_READY_PDUB_SIG_ILWALID,                        // 0x82
        LW_UCODE_ERR_CODE_PLAY_READY_PDUB_ENTRY_NOT_FOUND,                    // 0x83
        LW_UCODE_ERR_CODE_PLAY_READY_EXIT_FOR_DEVINIT_NOT_RUN,                // 0x84
        LW_UCODE_ERR_CODE_PLAY_READY_PDUB_PRIV_CONN_STATE_MISMATCH,           // 0x85
        LW_UCODE_ERR_CODE_PLAY_READY_OTP_ENTRY_NOT_AVAILABLE,                 // 0x86
        LW_UCODE_ERR_CODE_PLAY_READY_SEC2_MUTEX_ACQUIRE_FAILED,               // 0x87
        LW_UCODE_ERR_CODE_PLAY_READY_SEC2_MUTEX_RELEASE_FAILED,               // 0x88
        LW_UCODE_ERR_CODE_VERIFY_ENG_LICENSE_INCORRECT_TYPE,                  // 0x89
        LW_UCODE_ERR_CODE_ILWALID_FALCON,                                     // 0x8A
        LW_UCODE_ERR_CODE_NUM_REPAIR_ENTRIES_EXCEEDS_MAX_ALLOWED,             // 0x8B
        LW_UCODE_ERR_CODE_ILWALID_REPAIR_OBJECT,                              // 0x8C
        LW_UCODE_ERR_CODE_BCRT2x_CERT_BUFFER_OVERFLOW,                        // 0x8D
        LW_UCODE_ERR_CODE_BCRT2x_HAT_ENTRIES_BUFFER_OVERFLOW,                 // 0x8E
        LW_UCODE_ERR_CODE_BCRT2x_HAT_HEADER_OVER_SIZE,                        // 0x8F
        LW_UCODE_ERR_CODE_BCRT2x_RSA_SIG_HEADER_OVER_SIZE,                    // 0x90
        LW_UCODE_ERR_CODE_EEPROM_WRITE_PROTECT_MODE_ENABLED,                  // 0x91
        LW_UCODE_ERR_CODE_BCRT2X_CERT_BLOCK_VERSION_UNEXPECTED,               // 0x92
        LW_UCODE_ERR_CODE_BCRT2X_CERT_CONTROL_HEADER_OVERFLOW,                // 0x93
        LW_UCODE_ERR_CODE_BCRT2X_MAX_SELWRITYZONE_REACHED,                    // 0x94
        LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIGNATURES_SIZE_CHECK_FAILED,   // 0x95
        LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_STRUCT_SIZE_CHECK_FAILED,   // 0x96
        LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_ZONE_NUM_ILWALID,           // 0x97
        LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_ALGO_ILWALID,               // 0x98
        LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_BUILT_IN_SEC_ZONE_MISSING,      // 0x99
        LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIGNATURE_ILWALID,              // 0x9A
        LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_NOT_FOUND,                  // 0x9B
        LW_UCODE_ERR_CODE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH,             // 0x9C
        LW_UCODE_ERR_CODE_BCRT2X_VDPA_INTBLK_ENTRIES_NUM_EXCEED_MAX           // 0x9D
} LW_UCODE_ERR_CODE_TYPE;

#define LW_UCODE_POST_CODE_FWSECLIC_FEAT1                                  11:10        //  fwseclic feature exelwtion code field 1
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_RM_HULK                           10:10        //  HULK License from RM/MODS processing has been been exelwted.
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_RM_HULK_YES                  0x00000001
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_FRTS                              11:11        //  runtime security has been exelwted.
#define LW_UCODE_POST_CODE_FWSECLIC_FEAT_FRTS_YES                     0x00000001

//
// Error code field in the code that have other status fields on upper bits
//
#define LW_UCODE_ERR_CODE_FIELD                                      9:0

// The following defines are for boot verification status reporting.
// Keeping bits 15:12 for this to be in sync with the FWSECLIC_STAT field
#define LW_UCODE_STATUS_BOOT_VERIF_STAT                             15:12
#define LW_UCODE_STATUS_BOOT_VERIF_STAT_DEFAULT                     0x00000000
#define LW_UCODE_STATUS_BOOT_VERIF_STAT_VALID_OC_HULK               0x00000001
#define LW_UCODE_STATUS_BOOT_VERIF_STAT_VBIOS_VERIF_NOT_EXELWTED    0x00000002
#define LW_UCODE_STATUS_BOOT_VERIF_STAT_VALID_VBIOS                 0x00000003
#define LW_UCODE_STATUS_DEVID_IN_OVERRIDE_LIST                      16:16
#define LW_UCODE_STATUS_DEVID_IN_OVERRIDE_LIST_YES                    1
#define LW_UCODE_STATUS_DEVID_IN_OVERRIDE_LIST_NO                     0

#define LW_UCODE_POST_CODE_FWSECLIC_BASE                             LW_PBUS_VBIOS_SCRATCH
#define LW_UCODE_POST_CODE_FWSECLIC_REG1                                     0x6

#define LW_UCODE_POST_CODE_PREOS_PROG                                      15:12        // For PreOS uCode progress
#define LW_UCODE_POST_CODE_PREOS_PROG_NOT_STARTED                     0x00000000
#define LW_UCODE_POST_CODE_PREOS_PROG_STARTED                         0x00000001
#define LW_UCODE_POST_CODE_PREOS_PROG_EXIT                            0x00000002
#define LW_UCODE_POST_CODE_PREOS_PROG_EXIT_SELWREMODE                 0x00000003
#define LW_UCODE_POST_CODE_PREOS_PROG_ABORTED                         0x00000004
#define LW_UCODE_POST_CODE_PREOS_STAT_CODE                                 23:16        //  for PreOS init and periodic routines to report their progress and status. Defined in LW_UCODE_PREOS_STAT

typedef enum
{
        LW_UCODE_PREOS_STAT_NO_ACTIVITY = 0,
        LW_UCODE_PREOS_STAT_FANCTRL_INIT_START,                       // 0x1
        LW_UCODE_PREOS_STAT_FANCTRL_INIT_END,                         // 0x2
        LW_UCODE_PREOS_STAT_FANCTRL_PERIODIC_START,                   // 0x3
        LW_UCODE_PREOS_STAT_FANCTRL_PERIODIC_END,                     // 0x4
        LW_UCODE_PREOS_STAT_OOBMON_INIT_START,                        // 0x5
        LW_UCODE_PREOS_STAT_OOBMON_INIT_END,                          // 0x6
        LW_UCODE_PREOS_STAT_OOBMON_PERIODIC_START,                    // 0x7
        LW_UCODE_PREOS_STAT_OOBMON_PERIODIC_END,                      // 0x8
        LW_UCODE_PREOS_STAT_SWITCHER_INIT_START,                      // 0x9
        LW_UCODE_PREOS_STAT_SWITCHER_INIT_END,                        // 0xA
        LW_UCODE_PREOS_STAT_SWITCHER_PERIODIC_START,                  // 0xB
        LW_UCODE_PREOS_STAT_SWITCHER_PERIODIC_END,                    // 0xC
        LW_UCODE_PREOS_STAT_SWITCHER_SELWRE_DATA_SIGNATURE_ILWALID,   // 0xD
        LW_UCODE_PREOS_STAT_SWITCHER_BRSS_MUTEX_ACQUIRE_FAILED,       // 0xE
        LW_UCODE_PREOS_STAT_SWITCHER_BRSS_MUTEX_RELEASE_FAILED,       // 0xF
        LW_UCODE_PREOS_STAT_PREOS_UCODE_REVISION_ILWALID,             // 0x10
        LW_UCODE_PREOS_STAT_BOARD_BINNING_SETUP_FAILED,               // 0x11
        LW_UCODE_PREOS_STAT_DPCD_AUX_ILWALID_DATA,                    // 0x12
        LW_UCODE_PREOS_STAT_DPCD_AUX_HPD_ERROR,                       // 0x13
        LW_UCODE_PREOS_STAT_DPCD_ILWALID_AUX_PORT,                    // 0x14
        LW_UCODE_PREOS_STAT_DPCD_AUX_RW_RETRY,                        // 0x15
        LW_UCODE_PREOS_STAT_DPCD_AUX_READ_ERROR,                      // 0x16
        LW_UCODE_PREOS_STAT_DPCD_AUX_WRITE_ERROR,                     // 0x17
        LW_UCODE_PREOS_STAT_DPCD_AUX_REPLYTYPE_DEFER,                 // 0x18
        LW_UCODE_PREOS_STAT_DPCD_AUX_DATA_OUT_OF_BOUND,               // 0x19
        LW_UCODE_PREOS_STAT_SWITCHER_BRSS_INFO_ILWALID,               // 0x1A
        LW_UCODE_PREOS_STAT_SWITCHER_DEVICE_ID_MISMATCH,              // 0x1B
        LW_UCODE_PREOS_STAT_SWITCHER_BRSS_WRITE_FAILED,               // 0x1C
        LW_UCODE_PREOS_STAT_ECC_ENABLE_SETUP_FAILED,                  // 0x1D
        LW_UCODE_PREOS_STAT_INFOROMTASK_INIT_START,                   // 0x1E
        LW_UCODE_PREOS_STAT_INFOROMTASK_INIT_END,                     // 0x1F
        LW_UCODE_PREOS_STAT_RPR_FETCH_FAILED,                         // 0x20
} LW_UCODE_PREOS_STAT;

#define LW_UCODE_POST_CODE_BOOTLOADER_PROGRESS_CODE                        27:24        //  Record uCode bootloader exelwtion progress
#define LW_UCODE_POST_CODE_BOOTLOADER_PROG_NOT_STARTED                0x00000000
#define LW_UCODE_POST_CODE_BOOTLOADER_PROG_STARTED                    0x00000001
#define LW_UCODE_POST_CODE_BOOTLOADER_PROG_EXIT                       0x00000002
#define LW_UCODE_POST_CODE_BOOTLOADER_PROG_ERROR_EXIT                 0x00000003
#define LW_UCODE_POST_CODE_BOOTLOADER_ITERATIONS                           31:28        // Record uCode Boot loader ilwoked times

#define LW_UCODE_POST_CODE_UCODE_BASE                              LW_PBUS_VBIOS_SCRATCH
#define LW_UCODE_POST_CODE_UDE_REG                                          0xC      //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(12)
#define LW_UCODE_POST_CODE_UDE_PROG                                         3:0
#define LW_UCODE_POST_CODE_UDE_PROG_NOT_STARTED                             0x00000000
#define LW_UCODE_POST_CODE_UDE_PROG_STARTED                                 0x00000001
#define LW_UCODE_POST_CODE_UDE_PROG_DEVINIT_START                           0x00000002
#define LW_UCODE_POST_CODE_UDE_PROG_DEVINIT_COMPLETE                        0x00000003
#define LW_UCODE_POST_CODE_UDE_PROG_EXIT_SELWREMODE                         0x00000004
#define LW_UCODE_POST_CODE_UDE_PROG_EXIT                                    0x00000005
#define LW_UCODE_POST_CODE_UDE_PROG_ABORTED                                 0x00000006

#define LW_UCODE_POST_CODE_COMPACTION_REG                                   0xC      //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(12)
#define LW_UCODE_POST_CODE_COMPACTION_PROG                                  7:4
#define LW_UCODE_POST_CODE_COMPACTION_PROG_NOT_STARTED                      0x00000000
#define LW_UCODE_POST_CODE_COMPACTION_PROG_STARTED                          0x00000001
#define LW_UCODE_POST_CODE_COMPACTION_PROG_COMPACT_START                    0x00000002
#define LW_UCODE_POST_CODE_COMPACTION_PROG_COMPACT_COMPLETE                 0x00000003
#define LW_UCODE_POST_CODE_COMPACTION_PROG_EXIT_SELWREMODE                  0x00000004
#define LW_UCODE_POST_CODE_COMPACTION_PROG_EXIT                             0x00000005
#define LW_UCODE_POST_CODE_COMPACTION_PROG_ABORTED                          0x00000006

#define LW_UCODE_POST_CODE_GC6_REG                                          0xC      //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(12)
#define LW_UCODE_POST_CODE_GC6_PROG                                         11:8
#define LW_UCODE_POST_CODE_GC6_PROG_NOT_STARTED                             0x00000000
#define LW_UCODE_POST_CODE_GC6_PROG_STARTED                                 0x00000001
#define LW_UCODE_POST_CODE_GC6_PROG_DEVINIT_START                           0x00000002
#define LW_UCODE_POST_CODE_GC6_PROG_DEVINIT_COMPLETE                        0x00000003
#define LW_UCODE_POST_CODE_GC6_PROG_EXIT_SELWREMODE                         0x00000004
#define LW_UCODE_POST_CODE_GC6_PROG_EXIT                                    0x00000005

#define LW_UCODE_POST_CODE_UDE_ERR_CODE                                     19:12 //    The error codes are defined in GfwUcodeErrorStatus below.

enum GfwUdeUcodeErrorStatus
{
    LW_UCODE_STATUS_ERR_NONE = 0,                           // 0x0
    LW_SIGNATURE_ERR_DATA_NOT_PRESENT,                      // 0x1
    LW_SIGNATURE_TYPE_MISMATCH,                             // 0x2
    LW_SIGNATURE_ERR_DATA_SIZE_MISMATCH,                    // 0x3
    LW_SIGNATURE_ERR_DEVINIT_TABLE,                         // 0x4
    LW_SIGNATURE_ERR_DEVINIT_SCRIPT,                        // 0x5
    LW_SIGNATURE_ERR_WAR_BINARY,                            // 0x6
    LW_SIGNATURE_ERR_ILWALID_VBIOS,                         // 0x7
    LW_SIGNATURE_ERR_UNALIGNED_DATA,                        // 0x8
    LW_SIGNATURE_ERR_DEVINIT_DEVICE_ID_MISMATCH,            // 0x9
    LW_UDE_CLOCK_BOOST_SEQUENCE_FAILURE,                    // 0xA
    LW_UDE_UCODE_VALIDATION_FAILURE,                        // 0xB
    LW_UCODE_PRIV_SEC_FUSE_CHECK_FAILURE,                   // 0xC
    LW_UDE_IFF_FUSE_CHECK_FAILURE,                          // 0xD
    LW_UDE_EXELWTION_ALREADY_DONE,                          // 0xE
    LW_UDE_GC6_EXELWTION_MUTEX_FAILURE,                     // 0xF
    LW_UDE_BRSS_QUERY_MUTEX_ACQUIRE_FAILURE,                // 0x10
    LW_UDE_BRSS_QUERY_MUTEX_RELEASE_FAILURE,                // 0x11
    LW_UDE_VPR_SCRATCH_WRITE_MUTEX_ACQUIRE_FAILURE,         // 0x12
    LW_UDE_VPR_SCRATCH_WRITE_MUTEX_RELEASE_FAILURE,         // 0x13
    LW_UDE_VPR_SCRUBBER_BINARY_HANDOFF_FAILURE,             // 0x14
    LW_UDE_UCODE_SELWRE_DATA_SIGNATURE_ILWALID,             // 0x15
    LW_UDE_UCODE_BRSS_STRUCTURE_ILWALID,                    // 0x16
    LW_UDE_UCODE_UDE_VERSION_ILWALID,                       // 0x17
    LW_UDE_IMEM_TO_DMEM_COPY_ILWALID_PARAMETER,             // 0x18
    // 0x19 to 0xFF Reserved
};

#define LW_UCODE_POST_CODE_COMPACTION_ERR_CODE                              27:20

enum GfwCompactionUcodeErrorStatus
{
    LW_COMPACTION_ERR_NONE = 0,                             // 0x0
    LW_COMPACTION_SIGNATURE_ERR_DATA_NOT_PRESENT,           // 0x1 Actually corresponds to LW_SIGNATURE_ERR_DATA_NOT_PRESENT.
    LW_COMPACTION_SIGNATURE_TYPE_MISMATCH,                  // 0x2 Actually corresponds to LW_SIGNATURE_TYPE_MISMATCH.
    LW_COMPACTION_SIGNATURE_ERR_DATA_SIZE_MISMATCH,         // 0x3 Actually corresponds to LW_SIGNATURE_ERR_DATA_SIZE_MISMATCH.
    LW_SIGNATURE_ERR_COMPACT_SCRIPT,                        // 0x4
    LW_COMPACTION_ERR_VALIDATION,                           // 0x5
    LW_COMPACTION_ERR_MEMCPY,                               // 0x6
    LW_COMPACTION_ERR_XMEMSEL,                              // 0x7
    LW_COMPACTION_ERR_FIXUP,                                // 0x8
    LW_COMPACTION_ERR_XLAT,                                 // 0x9
    LW_COMPACTION_ERR_SIGNATURE,                            // 0xA
    LW_COMPACTION_ERR_SEC2MUTEX_RELEASE_FAILED,             // 0xB
    LW_COMPACTION_ERR_SEC2MUTEX_ACQUIRE_FAILED,             // 0xC
    LW_COMPACTION_ERR_SELWRE_DATA_SIGNATURE_ILWALID,        // 0xD
    LW_COMPACTION_ERR_ILWALID_UCODE,                        // 0xE
    LW_COMPACTION_ERR_ILWALID_ORDER_OF_EXELWTION,           // 0xF
    LW_COMPACTION_ERR_DEVICE_ID_MISMATCH,                   // 0x10
    LW_COMPACTION_ERR_BRSS_INFO_ILWALID                     // 0x11
    // 0x12 to 0xFF Reserved
};

#define LW_LWFLASH_UCODE_POST_CODE_REG                                      0xC
#define LW_LWFLASH_UCODE_POST_CODE_PROG                                     31:28       //  Record LWFlash uCode exelwtion progress
#define LW_LWFLASH_UCODE_POST_CODE_PROG_NOT_STARTED                         0x00000000
#define LW_LWFLASH_UCODE_POST_CODE_PROG_STARTED                             0x00000001
#define LW_LWFLASH_UCODE_POST_CODE_PROG_OTP_PROGRAM_START                   0x00000002
#define LW_LWFLASH_UCODE_POST_CODE_PROG_OTP_PROGRAM_COMPLETE                0x00000003
#define LW_LWFLASH_UCODE_POST_CODE_PROG_PDUB_VERIFY_START                   0x00000004
#define LW_LWFLASH_UCODE_POST_CODE_PROG_PDUB_VERIFY_COMPLETE                0x00000005
#define LW_LWFLASH_UCODE_POST_CODE_PROG_EXIT                                0x00000006
#define LW_LWFLASH_UCODE_POST_CODE_PROG_ERROR_EXIT                          0x00000007

#define LW_UCODE_POST_CODE_GC6_REG1                                         0xE      //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(13)
#define LW_UCODE_POST_CODE_GC6_ERR_CODE                                     7:0

enum GfwGC6UcodeErrorStatus
{
    LW_UCODE_GC6_STATUS_ERR_NONE = 0,                       // 0x0
    LW_UCODE_GC6_ACR_BINARY_HANDOFF_FAIL,                   // 0x1
    LW_UCODE_UDE_GC6_EXELWTION_ALREADY_DONE,                // 0x2
    LW_UCODE_GC6_EXELWTION_MUTEX_FAILURE,                   // 0x3
    LW_UCODE_GC6_UCODE_VALIDATION_FAILURE,                  // 0x4
    LW_UCODE_GC6_UCODE_UNKNOWN_SW_VERSION,                  // 0x5
    LW_UCODE_GC6_UCODE_ERROR_PARITY_CHECK,                  // 0x6
    // 0x7 to 0xFF Reserved
};

#define LW_UCODE_POST_CODE_FWSECLIC_REG_0E                                  0xE      //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(14)
#define LW_UCODE_POST_CODE_FWSECLIC_FRTS_ERR_CODE                           31:16    //  Error code field for FW Runtime Security,  16 bits
                                                                                     //  Error code defined in enum LW_UCODE_ERR_CODE_TYPE

typedef enum
{
    LW_UCODE_FBFALCON_FRTS_PROG_NOT_STARTED = 0,                            // 0x0 - fbFalcon processing VBIOS DIRT table not started
    LW_UCODE_FBFALCON_FRTS_PROG_START,                                      // 0x1 - fbFalcon processing VBIOS DIRT table started
    LW_UCODE_FBFALCON_FRTS_PROG_MEMORY_INFORMATION_TABLE_PARSED,            // 0x2 - fbFalcon completed MemInfoTbl Parsing
    LW_UCODE_FBFALCON_FRTS_PROG_MEMORY_TRAINING_TABLE_PARSED,               // 0x3 - fbFalcon completed MemTrnTbl Parsing
    LW_UCODE_FBFALCON_FRTS_PROG_MEMORY_TRAINING_PATTERN_TABLE_PARSED,       // 0x4 - fbFalcon completed MemTrnPatTbl Parsing
    // more progress code here..
} LW_UCODE_FBFALCON_FRTS_PROG_CODE;

#define LW_UCODE_POST_CODE_FBFALCON_FRTS_REG11                              0x11        // scratch register offset. LW_PBUS_VBIOS_SCRATCH(17)
#define LW_UCODE_POST_CODE_FBFALCON_FRTS_ERR_CODE                           15:0        // error code generate by fbFalcon when processing VBIOS table
#define LW_UCODE_POST_CODE_FBFALCON_FRTS_PROG_CODE                          23:16       // progress code generate by fbFalcon when processing VBIOS table

//  footnote code
#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_REG13                          0x13        // scratch register offset. LW_PBUS_SW_SCRATCH(19)
#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_HULK                           15:0        // footnote code for HULK
#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_BCRT                           31:16       // footnote code for BIOS Cert

#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_REG14                          0x14        // scratch register offset. LW_PBUS_SW_SCRATCH(20)
#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_UGPU                           15:0        // footnote code for UGPU
#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_FRTS                           31:16       // footnote code for FRTS

#define LW_UCODE_POST_CODE_FBFALCON_REG11                                   0x11        // scratch register offset. LW_PBUS_VBIOS_SCRATCH(17)
#define LW_UCODE_POST_CODE_FBFALCON_ERR_CODE                                15:0        // error code for booting FBFalcon

#define LW_UCODE_POST_CODE_FWSECLIC_SELWREBOOT_REG15                        0x15        // scratch register offset. LW_PBUS_SW_SCRATCH(21)
#define LW_UCODE_POST_CODE_FWSECLIC_SELWREBOOT_ERR_CODE                     15:0        // Error code for FWSECLIC Secure boot command.

#define LW_UCODE_POST_CODE_HULK_BASE                                        LW_PBUS_VBIOS_SCRATCH
#define LW_UCODE_POST_CODE_HULK_REG                                         0x15        //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(21)
#define LW_UCODE_POST_CODE_HULK_PROG                                        19:16
#define LW_UCODE_POST_CODE_HULK_PROG_NOT_STARTED                            0x00000000
#define LW_UCODE_POST_CODE_HULK_PROG_STARTED                                0x00000001
#define LW_UCODE_POST_CODE_HULK_PROG_IN_SELWREMODE                          0x00000002
#define LW_UCODE_POST_CODE_HULK_PROG_LICENSE_PROCESS_COMPLETE               0x00000003
#define LW_UCODE_POST_CODE_HULK_PROG_EXIT_SELWREMODE                        0x00000004
#define LW_UCODE_POST_CODE_HULK_PROG_COMPLETE                               0x00000005

#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_REG                              0x15        //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(21)
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_PROG                             23:20
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_PROG_NOT_STARTED                 0x00000000
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_PROG_STARTED                     0x00000001
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_PROG_IN_SELWREMODE               0x00000002
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_PROG_LICENSE_PROCESS_COMPLETE    0x00000003
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_PROG_EXIT_SELWREMODE             0x00000004
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_PROG_COMPLETE                    0x00000005

#define LW_UCODE_POST_CODE_HULK_REG1                                        0x5         //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(5)
#define LW_UCODE_POST_CODE_HULK_ERR_CODE                                    9:0

#define LW_UCODE_POST_CODE_HULK_REG2                                        0x6         //  scratch register offset. LW_PBUS_VBIOS_SCRATCH(6)
#define LW_UCODE_POST_CODE_HULK_FEAT_RM_HULK                                10:10       //  HULK License from RM/MODS processing has been been exelwted.
#define LW_UCODE_POST_CODE_HULK_FEAT_RM_HULK_YES                            0x00000001

#define LW_UCODE_POST_CODE_HULK_REG3                                        0x13        // scratch register offset. LW_PBUS_SW_SCRATCH(19)
#define LW_UCODE_POST_CODE_HULK_FOOTNOTE                                    15:0        // footnote code for HULK

#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_REG1                             0x18        // scratch register offset. LW_PBUS_SW_SCRATCH(24)
#define LW_UCODE_POST_CODE_PREDEVINIT_HULK_ERR_CODE                         15:0        // Error code for HULK

#define LW_UCODE_POST_CODE_FLOORSWEEP_REG                                   0x18        // scratch register offset. LW_PBUS_SW_SCRATCH(24)
#define LW_UCODE_POST_CODE_FLOORSWEEP_GEN_ERR_CODE                          19:16       // Error code for general floorsweep module
#define LW_UCODE_POST_CODE_FLOORSWEEP_INFOROM_ERR_CODE                      23:20       // Error code for InfoROM floorsweep module
#define LW_UCODE_POST_CODE_FLOORSWEEP_RECONFIG_ERR_CODE                     27:24       // Error code for reconfig floorsweep module
#define LW_UCODE_POST_CODE_FLOORSWEEP_BIOS_TABLE_ERR_CODE                   31:28       // Error code for BIOS table floorsweep module


//  Note:  LW_PBUS_SW_SCRATCH(0x16) is used by RM

#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_REG17                          0x17     // scratch register offset. LW_PBUS_SW_SCRATCH(23)
#define LW_UCODE_POST_CODE_FWSECLIC_FOOTNOTE_SELWREBOOT                     31:0     // Footnote code for FWSECLIC Secure boot command.


/* -------------- Unified Return error codes for all GFW ucodes --------------*/
typedef enum
{
        LW_UCODE_ERR_CODE_NOERROR = 0,
        /*********  Values 0x001 to 0x1FF are reserved for Fwseclic/HULK-specific error codes.          *********/
        /*********  These values will be written to the following registers:                            *********/
        /*********  LW_UCODE_ERROR_CODE_FWSECLIC_REG_BIOSCERT_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(5)[25:16]  *********/
        /*********  LW_UCODE_ERROR_CODE_FWSECLIC_REG1_UGPU_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(6)[9:0]       *********/
        LW_FWSECLIC_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                      0x001,
        LW_FWSECLIC_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ID_NOT_AVAIL =                                0x002,
        LW_FWSECLIC_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_FAIL =                                0x003,
        LW_FWSECLIC_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                      0x004,
        LW_FWSECLIC_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_RELEASE_FAIL =                                0x005,
        LW_FWSECLIC_ERR_IMEM_TO_DMEM_COPY_ILWALID_PARA =                                                0x006,
        LW_FWSECLIC_ERR_FR_CNTR_PWRCLK_FREQ =                                                           0x007,
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =                   0x008,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =             0x009,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_GETDATA_TIMEOUT =                       0x00A,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =                  0x00B,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                0x00C,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =               0x00D,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT =                    0x00E,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x00F,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_GETDATA_TIMEOUT =                        0x010,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_GETDATA_REQUEST_FAIL =                   0x011,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_NOT_AVAILABLE =                               0x012,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =              0x013,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =             0x014,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                 0x015,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =           0x016,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                     0x017,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                0x018,
        LW_FWSECLIC_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_ERROR =                                 0x019,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                 0x01A,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =           0x01B,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                     0x01C,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                0x01D,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEX_RELEASE_ERROR =                                 0x01E,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =              0x01F,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =             0x020,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =            0x021,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =           0x022,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                0x023,
        LW_FWSECLIC_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =               0x024,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        LW_FWSECLIC_ERR_CODE_CMD_UNSUPPORTED_GPU =                                                      0x025,
        LW_FWSECLIC_ERR_CODE_CMD_UNSUPPORTED_COMMAND =                                                  0x026,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_DEVID_FAIL =                                              0x027,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_CERT_NOT_FOUND =                                          0x028,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_CERT_PARSE_FAIL =                                         0x029,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_CERT_VERIFY_FAIL =                                        0x02A,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_HAT_FAIL =                                                0x02B,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_BIOS_SIG_FAIL =                                           0x02C,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_HULK_INIT_FAIL =                                          0x02D,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_HULK_KA_NOT_FOUND =                                       0x02E,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_HULK_TYPE_ILWALID =                                       0x02F,
        LW_FWSECLIC_ERR_CODE_CMD_VBIOS_VERIFY_HULK_SIG_ILWALID =                                        0x030,
        LW_FWSECLIC_ERR_CODE_CERT_UNKNOWN_ERROR =                                                       0x031,
        LW_FWSECLIC_ERR_CODE_CERT_EXT_NOT_FOUND =                                                       0x032,
        LW_FWSECLIC_ERR_CODE_CERT_EXT_NO_SUB_STRUCT_FOUND =                                             0x033,
        LW_FWSECLIC_ERR_CODE_CERT_UNSUPPORTED_VERSION =                                                 0x034,
        LW_FWSECLIC_ERR_CODE_CERT_NO_EXTENSION_EXIST =                                                  0x035,
        LW_FWSECLIC_ERR_CODE_CERT_T7QV1_PAYLOAD_SIZE_ERROR =                                            0x036,
        LW_FWSECLIC_ERR_CODE_CERT_T7_SW_FEATURE_PAYLOAD_SIZE_ERROR =                                    0x037,
        LW_FWSECLIC_ERR_CODE_CERT_T7_UNSUPPORTED_HW_STRUCT_VERSION =                                    0x038,
        LW_FWSECLIC_ERR_CODE_CERT_T7_EXTENSIONS_NUM_EXCEED_LIMIT =                                      0x039,
        LW_FWSECLIC_ERR_CODE_CERT_UGPU_PERSONALITY_MIS_MATCH =                                          0x03A,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_ECID_MISMATCH =                                                  0x03B,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_ECID_ENCODING_UNKNOWN =                                          0x03C,
        LW_FWSECLIC_ERR_CODE_ECID_ENCODING_ALGO_UNKNOWN =                                               0x03D,
        LW_FWSECLIC_ERR_CODE_CERT_T7_REG_OVERRIDE_TYPE_UNKNOWN =                                        0x03E,
        LW_FWSECLIC_ERR_CODE_DERIVED_KEY_TYPE_ILWALID =                                                 0x03F,
        LW_FWSECLIC_ERR_CODE_UCODE_NOT_IN_HS_MODE =                                                     0x040,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_DEVID_MISMATCH =                                                 0x041,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_NO_ID_MATCH_FOUND =                                              0x042,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_DATA_BUFFER_TOO_SMALL =                                          0x043,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_INFOROM_NOT_FOUND =                                              0x044,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_INFOROM_UL_GLOB_NOT_FOUND =                                      0x045,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_INFOROM_HLK_OBJ_NOT_VALID =                                      0x046,
        LW_FWSECLIC_ERR_CODE_UGPU_PROCESSING_FAILED_ILWALID_ULF_OBJECT =                                0x047,
        LW_FWSECLIC_ERR_CODE_UGPU_PROCESSING_FAILED_ILWALID_UPR_OBJECT =                                0x048,
        LW_FWSECLIC_ERR_CODE_FWSECLIC_PCI_EXP_ROM_NOT_FOUND =                                           0x049,
        LW_FWSECLIC_ERR_CODE_VERIFY_ENG_HULK_3AES_SIG_MISMATCH_WITH_GPU_FUSE =                          0x04A,
        LW_FWSECLIC_ERR_CODE_VERIFY_ENG_HULK_NO_3AES_SIG =                                              0x04B,
        LW_FWSECLIC_ERR_CODE_DEVID_MATCH_LIST_DEVID_MATCH_FAILED =                                      0x04C,
        LW_FWSECLIC_ERR_CODE_VERIFY_LICENSE_UGPU_SIG_ILWALID =                                          0x04D,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_OPCODE_UNDEFINED =                                               0x04E,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_REG_READ_BACK_VERIFY_MISMATCH =                                  0x04F,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_REG_RMW_RBV_READ_BACK_VERIFY_MISMATCH =                          0x050,
        LW_FWSECLIC_ERR_CODE_HULK_INST_IN_SYS_VERIFICATION_FAILED =                                     0x051,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_STRICT_ID_DEVID_MISMATCH =                                       0x052,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_STRICT_ID_ECID_MISMATCH =                                        0x053,
        LW_FWSECLIC_ERR_CODE_PRI_TRANSACTION_ERROR =                                                    0x054,
        LW_FWSECLIC_ERR_CODE_IMEM_TO_DMEM_COPY_ILWALID_PARA =                                           0x055,
        LW_FWSECLIC_ERR_CODE_CERT20_INTBLK_VDPA_HEADER_ILWALID =                                        0x056,
        LW_FWSECLIC_ERR_CODE_CERT20_VDPA_NOT_FINALIZED =                                                0x057,
        LW_FWSECLIC_ERR_CODE_CERT20_VDPA_SIG_ILWALID =                                                  0x058,
        LW_FWSECLIC_ERR_CODE_CERT20_VDPA_ENTRY_NOT_FOUND =                                              0x059,
        LW_FWSECLIC_ERR_CODE_CERT20_VDPA_CERT_INTBLK_MISMATCH =                                         0x05A,
        LW_FWSECLIC_ERR_CODE_CERT20_VDPA_ENTRY_FOUND_DATA_MISMATCH =                                    0x05B,
        LW_FWSECLIC_ERR_CODE_CERT20_VDPA_DATA_ILWALID =                                                 0x05C,
        LW_FWSECLIC_ERR_CODE_CERT20_VDPA_FLASH_SIZE_LARGER_THAN_EXPECTED =                              0x05D,
        LW_FWSECLIC_ERR_CODE_CERT20_INTBLK_VDPA_BLOCK_OVERSIZE =                                        0x05E,
        LW_FWSECLIC_ERR_CODE_CERT21_FMT_HAT_ENTRY_NUMBER_ILWALID =                                      0x05F,
        LW_FWSECLIC_ERR_CODE_CERT21_FMT_HAT_ENTRY_FOMMATTER_TOO_LONG =                                  0x060,
        LW_FWSECLIC_ERR_CODE_CERT21_FMT_FORMATTER_DATA_BLOCK_OVER_SIZE =                                0x061,
        LW_FWSECLIC_ERR_CODE_CERT21_FMT_UNEXPECTED_FORMATTER_TYPE =                                     0x062,
        LW_FWSECLIC_ERR_CODE_CERT21_FMT_EXCEED_FORMATTER_LENGTH =                                       0x063,
        LW_FWSECLIC_ERR_CODE_CERT21_CERT_VERSION_UNEXPECTED =                                           0x064,
        LW_FWSECLIC_ERR_CODE_CERT21_FRTS_DEVINIT_NOT_COMPLETED =                                        0x065,
        LW_FWSECLIC_ERR_CODE_CERT21_FRTS_SELWRE_INPUT_STRUCT_VERSION_UNEXPECTED =                       0x066,
        LW_FWSECLIC_ERR_CODE_CERT21_FRTS_DMA_UNEXPECTED_MEDIA_TYPE =                                    0x067,
        LW_FWSECLIC_ERR_CODE_UDE_WAIT_FOR_COMPLETION_TIMEOUT =                                          0x068,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_UCODE_NOT_EXELWTING_ON_SEC2 =                                    0x069,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_SCRUBBER_BIN_NOT_COMPLETED_ON_VPR_SKU =                          0x06A,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_UDE_NOT_COMPLETED_ON_NON_VPR_SKU =                               0x06B,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_ACR_BIN_LOADED =                                                 0x06C,
        LW_FWSECLIC_ERR_CODE_CERT_HULK_UNSUPPORTED_VERSION =                                            0x06D,
        LW_FWSECLIC_ERR_CODE_SELWREBOOT_ILWALID_INCMD_STRUCT =                                          0x06E,
        LW_FWSECLIC_ERR_CODE_SELWREBOOT_ILWALID_MEDIA_TYPE =                                            0x06F,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_INTBLK_BUFFER_SIZE_OVERFLOW =                                  0x070,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_INTBLK_ENTRIES_NUMBER_OVERLIMIT =                              0x071,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_INTBLK_ENTRIES_ILWALID_ENTRY_TYPE =                            0x072,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_UNEXPECTED_MAJOR_TYPE =                                        0x073,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_UNEXPECTED_PCI_CODE_TYPE =                                     0x074,
        LW_FWSECLIC_ERR_CODE_BOOTLOADER_FALCON_ILWALID =                                                0x075,
        LW_FWSECLIC_ERR_CODE_BOOTLOADER_UCODE_HASH_MISMATCH =                                           0x076,
        LW_FWSECLIC_ERR_CODE_BOOTLOADER_INTERFACE_HASH_MISMATCH =                                       0x077,
        LW_FWSECLIC_ERR_CODE_BOOTLOADER_INTERFACE_NOT_FOUND =                                           0x078,
        LW_FWSECLIC_ERR_CODE_MMU_WPR_UNALIGNED =                                                        0x079,
        LW_FWSECLIC_ERR_CODE_FALCON_LOADER_BUFFER_READ_UNDERFLOW =                                      0x07A,
        LW_FWSECLIC_ERR_CODE_FALCON_LS_SETUP_FAILED =                                                   0x07B,
        LW_FWSECLIC_ERR_CODE_FALCON_LOADER_ILWALID_UCODE_DESCRIPTOR_INFO =                              0x07C,
        LW_FWSECLIC_ERR_CODE_FALCON_LOADER_ILWALID_INTERFACE_DESCRIPTOR_INFO =                          0x07D,
        LW_FWSECLIC_ERR_CODE_SELWREBOOT_GPU_IN_GC6_EXIT =                                               0x07E,
        LW_FWSECLIC_ERR_CODE_FRTS_FW_IMAGE_EXCEEDING_MEDIA_BOUNDARY =                                   0x07F,
        LW_FWSECLIC_ERR_CODE_FALCON_LS_SETUP_UNEXPECTED_FALCON_STATE =                                  0x080,
        LW_FWSECLIC_ERR_CODE_HOLDOFF_CAUSED_EARLY_EXIT =                                                0x081,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIGNATURES_SIZE_CHECK_FAILED =                         0x082,
        LW_FWSECLIC_ERR_CODE_CERT22_CERT_VERSION_UNEXPECTED =                                           0x083,
        LW_FWSECLIC_ERR_CODE_BCRT2X_METADATA_HEADER_OVERFLOW =                                          0x084,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_STRUCT_SIZE_CHECK_FAILED =                         0x085,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_ACTIVE_ZONE_NUMBER_OVERLIMIT =                         0x086,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_NORMAL_ENTRIES_NUMBER_OVERLIMIT =                              0x087,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_ZONE_NUM_ILWALID =                                 0x088,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_ALGO_ILWALID =                                     0x089,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_BUILT_IN_SEC_ZONE_MISSING =                            0x08A,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_NOT_FOUND =                                        0x08B,
        LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIGNATURE_ILWALID =                                    0x08C,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH =                                   0x08D,
        LW_FWSECLIC_ERR_CODE_BCRT2X_CERT_CONTROL_HEADER_OVERFLOW =                                      0x08E,
        LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_UNEXPECTED_CODE_TYPE =                                         0x08F,
        LW_FWSECLIC_ERR_CODE_BCRT2X_MAX_SELWRITYZONE_REACHED =                                          0x090,
        LW_FWSECLIC_ERR_CODE_CERT_TYPE_UNSUPPORTED =                                                    0x091,
        LW_FWSECLIC_ERR_CODE_BIOS_CERT_SIZE_UNSUPPORTED =                                               0x092,
        LW_FWSECLIC_ERR_CODE_LICENSE_SIZE_UNSUPPORTED =                                                 0x093,
        LW_FWSECLIC_ERR_CODE_INPUT_CERT_PTR_NULL =                                                      0x094,
        LW_FWSECLIC_ERR_CODE_CERT_NO_EXTENSIONS_PRESENT =                                               0x095,
        LW_FWSECLIC_ERR_CODE_FRTS_INPUT_DESC_VERSION_SIZE_MISMATCH =                                    0x096,
        LW_FWSECLIC_ERR_CODE_FRTS_INPUT_DESC_MEDIATYPE_MISMATCH =                                       0x097,
        LW_FWSECLIC_ERR_CODE_FRTS_INPUT_DESC_GFWIMAGESIZE_MISMATCH =                                    0x098,
        LW_FWSECLIC_ERR_CODE_FRTS_INPUT_DESC_GFWOFFSETINEEPROM_MISMATCH =                               0x099,
        LW_FWSECLIC_ERR_CODE_FRTS_OUTPUT_DESC_VERSION_SIZE_MISMATCH =                                   0x09A,
        LW_FWSECLIC_ERR_CODE_FRTS_OUTPUT_DESC_MEDIATYPE_MISMATCH =                                      0x09B,
        LW_FWSECLIC_ERR_CODE_FRTS_OUTPUT_DESC_REGIONSIZE_MISMATCH =                                     0x09C,
        LW_FWSECLIC_ERR_CODE_FRTS_SELWREBOOT_NOT_COMPLETED =                                            0x09D,
        LW_FWSECLIC_ERR_CODE_ILWALID_PREOSCTRLIFACE_VERSION =                                           0x09E,
        LW_FWSECLIC_ERR_CODE_ILWALID_PREOSCTRLIFACE_SIZE =                                              0x09F,
        LW_FWSECLIC_ERR_CODE_ILWALID_INFOROMFEATSIFACE_VERSION =                                        0x0A0,
        LW_FWSECLIC_ERR_CODE_ILWALID_INFOROMFEATSIFACE_SIZE =                                           0x0A1,
        LW_FWSECLIC_ERR_CODE_SELWREBOOT_ILWALID_DMEMMAP_VERSION =                                       0x0A2,
        LW_FWSECLIC_ERR_CODE_SELWREBOOT_ILWALID_DMEMMAP_SIGNATURE =                                     0x0A3,
        LW_FWSECLIC_ERR_CODE_SELWREBOOT_ILWALID_DMEMMAP_SIZE =                                          0x0A4,

        /*********  Values 0x200 to 0x3FF are reserved for Shared functions error codes.              *********/
        /*********  These values will be written to their corresponding ucode error code.               *********/
        LW_UCODE_ERR_SEC2MUTEX_ACQUIRE_COLD_BOOT_GC6_UDE_EXELWTION_MUTEX_STATUS_ILWALID_POINTER =       0x200,
        LW_UCODE_ERR_SEC2MUTEX_ACQUIRE_COLD_BOOT_GC6_UDE_EXELWTION_MUTEX_ID_NOT_AVAIL =                 0x201,
        LW_UCODE_ERR_SEC2MUTEX_ACQUIRE_COLD_BOOT_GC6_UDE_EXELWTION_MUTEX_ACQUIRE_FAIL =                 0x202,
        LW_UCODE_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                         0x203,
        LW_UCODE_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ID_NOT_AVAIL =                                   0x204,
        LW_UCODE_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_FAIL =                                   0x205,
        LW_UCODE_ERR_BRSS_READ_IDENTIFIER_CHECK_FAIL =                                                  0x206,
        LW_UCODE_ERR_BRSS_READ_CHECKSUM_CHECK_FAIL =                                                    0x207,
        LW_UCODE_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                         0x208,
        LW_UCODE_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_RELEASE_FAIL =                                   0x209,
        LW_UCODE_ERR_SEC2MUTEX_RELEASE_COLD_BOOT_GC6_UDE_EXELWTION_MUTEX_STATUS_ILWALID_POINTER =       0x20A,
        LW_UCODE_ERR_SEC2MUTEX_RELEASE_COLD_BOOT_GC6_UDE_EXELWTION_MUTEX_RELEASE_FAIL =                 0x20B,
        LW_UCODE_ERR_VERIFY_SELWRE_DATA_GENERIC_HEADER_NOT_FOUND =                                      0x20C,
        LW_UCODE_ERR_VERIFY_SELWRE_DATA_IMEM_TO_DMEM_COPY_ILWALID_PARA =                                0x20D,
        LW_UCODE_ERR_VERIFY_SELWRE_DATA_DERIVE_SEC_DATA_KEY_HELPER_AESECB_NOT_IN_HSMODE =               0x20E,
        LW_UCODE_ERR_VERIFY_SELWRE_DATA_COMPUTE_SEC_DATA_SIGNATURE_HELPER_AESECB_NOT_IN_HSMODE =        0x20F,
        LW_UCODE_ERR_VERIFY_SELWRE_DATA_SIGNATURE_ILWALID =                                             0x210,
        LW_UCODE_ERR_UDE_GC6_EXELWTION_ALREADY_DONE =                                                   0x211,
        LW_UCODE_ERR_SWITCHPWRSYSCLKXTAL16x_PLL_WAIT_TIMEOUT =                                          0x212,
        LW_UCODE_ERR_PRIV_SEC_NOT_ENABLED =                                                             0x213,
        LW_UCODE_ERR_BRSS_WRITE_BRSS_IDENTIFIER_MISMATCH =                                              0x214,
        LW_UCODE_ERR_UCODE_VALIDATION_FAILURE =                                                         0x215,
        LW_UCODE_ERR_CHIP_ID_ILWALID =                                                                  0x216,
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =              0x217,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =        0x218,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXPLM_READ_GETDATA_TIMEOUT =                  0x219,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =             0x21A,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =           0x21B,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =          0x21C,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXID_READ_SENDREQUEST_TIMEOUT =               0x21D,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =         0x21E,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXID_READ_GETDATA_TIMEOUT =                   0x21F,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXID_READ_GETDATA_REQUEST_FAIL =              0x220,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXID_NOT_AVAILABLE =                          0x221,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =         0x222,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =        0x223,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =            0x224,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =      0x225,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                0x226,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =           0x227,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_UDE_EXEC_COMPLETE_MUTEX_ACQUIRE_ERROR =                            0x228,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =            0x229,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =      0x22A,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                0x22B,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =           0x22C,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEX_RELEASE_ERROR =                            0x22D,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =         0x22E,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =        0x22F,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =       0x230,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =      0x231,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =           0x232,
        LW_UCODE_ERR_SEMUTEX_RELEASE_UDE_EXEC_COMPLETE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =          0x233,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites  */
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =                    0x234,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x235,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXPLM_READ_GETDATA_TIMEOUT =                        0x236,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =                   0x237,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x238,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                0x239,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXID_READ_SENDREQUEST_TIMEOUT =                     0x23A,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =               0x23B,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXID_READ_GETDATA_TIMEOUT =                         0x23C,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXID_READ_GETDATA_REQUEST_FAIL =                    0x23D,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXID_NOT_AVAILABLE =                                0x23E,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x23F,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =              0x240,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                  0x241,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =            0x242,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                      0x243,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                 0x244,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_WPR_SCRATCH_MUTEX_ACQUIRE_ERROR =                                  0x245,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                  0x246,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =            0x247,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                      0x248,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                 0x249,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEX_RELEASE_ERROR =                                  0x24A,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x24B,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =              0x24C,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =             0x24D,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =            0x24E,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x24F,
        LW_UCODE_ERR_SEMUTEX_RELEASE_WPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                0x250,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =                      0x251,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =                0x252,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_GETDATA_TIMEOUT =                          0x253,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =                     0x254,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                   0x255,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                  0x256,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT =                       0x257,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =                 0x258,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_GETDATA_TIMEOUT =                           0x259,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_GETDATA_REQUEST_FAIL =                      0x25A,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_NOT_AVAILABLE =                                  0x25B,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x25C,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =                0x25D,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                    0x25E,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x25F,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                        0x260,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                   0x261,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_ERROR =                                    0x262,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                    0x263,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x264,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                        0x265,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                   0x266,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEX_RELEASE_ERROR =                                    0x267,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x268,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =                0x269,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x26A,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =              0x26B,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                   0x26C,
        LW_UCODE_ERR_SEMUTEX_RELEASE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                  0x26D,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        LW_UCODE_ERR_GFWIMAGE_READ_ILWALID_MEDIA_TYPE =                                                 0x26E,
        LW_UCODE_ERR_GFWIMAGE_READ_DMA_ILWALID_SOURCE_TYPE =                                            0x26F,
        LW_UCODE_ERR_GFWIMAGE_READ_DMA_DMEM_OFFSET_NOT_ALIGNED =                                        0x270,
        LW_UCODE_ERR_GFWIMAGE_READ_ILWALID_EEPROM_BASE_OFFSET =                                         0x271,
        LW_UCODE_ERR_GFWIMAGE_READ_DMA_ILWALID_DMA_DIRECTION =                                          0x272,
        LW_UCODE_ERR_CODE_FALCON_ILWALID =                                                              0x273,
        LW_UCODE_ERR_GP_BUFFER_MAX_NUMBER_BUFFER_EXCEED =                                               0x274,
        LW_UCODE_ERR_GP_BUFFER_ALLOCATION_ILWALID_SIZE =                                                0x275,
        LW_UCODE_ERR_GP_BUFFER_ALLOCATION_FAILED =                                                      0x276,
        LW_UCODE_ERR_GP_BUFFER_ALLOCATION_NUMBER_NOT_SUPPORTED =                                        0x277,
        LW_UCODE_ERR_GP_BUFFER_FREE_POINTER_NOT_FOUND =                                                 0x278,
        LW_UCODE_ERR_GP_BUFFER_FREE_BUFFER_MAX_NUMBER_EXCEED =                                          0x279,
        LW_UCODE_ERR_GP_BUFFER_FREE_ILWALID_USAGE_MASK       =                                          0x27A,
        LW_UCODE_ERR_GP_BUFFER_TAKE_NUMBER_NOT_SUPPORTED     =                                          0x27B,
        LW_UCODE_ERR_GP_BUFFER_TAKE_FAILED                   =                                          0x27C,
        LW_UCODE_ERR_GP_BUFFER_ALLOCATION_TOTAL_SIZE_MISMATCH =                                         0x27D,
        LW_UCODE_ERR_GP_BUFFER_TAKE_TOTAL_SIZE_MISMATCH       =                                         0x27E,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_ILWALID_FW_IMAGE =                                             0x27F,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_ILWALID_IFR_IMAGE =                                            0x280,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_IFR_ILWALID_SIZE =                                             0x281,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_ILWALID_EXTIMG_DATA =                                          0x282,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_ILWALID_DATA_STRUCT_SIGN =                                     0x283,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_ILWALID_ROM_SIGN =                                             0x284,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_CERT_BLOCK_NOT_PRESENT =                                       0x285,
        LW_UCODE_ERR_CODE_GFWIMAGEPARSER_INFOROM_BLOCK_NOT_PRESENT =                                    0x286,
        LW_UCODE_ERR_CODE_DMA_DEST_OFFSET_UNALIGNED =                                                   0x287,
        LW_UCODE_ERR_ILWALID_UCODE_REVISION =                                                           0x288,
        LW_UCODE_ERR_ILWALID_FALCON_REV =                                                               0x289,
        LW_UCODE_ERR_UNEXPECTED_FALCON_ENGID =                                                          0x28A,
        LW_UCODE_ERR_UNCORRECTABLE_ERROR_IN_FUSE_BLOCK =                                                0x28B,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =                    0x28C,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x28D,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXPLM_READ_GETDATA_TIMEOUT =                        0x28E,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =                   0x28F,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x290,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                0x291,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXID_READ_SENDREQUEST_TIMEOUT =                     0x292,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =               0x293,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXID_READ_GETDATA_TIMEOUT =                         0x294,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXID_READ_GETDATA_REQUEST_FAIL =                    0x295,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXID_NOT_AVAILABLE =                                0x296,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x297,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =              0x298,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                  0x299,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =            0x29A,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                      0x29B,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                 0x29C,
        LW_UCODE_ERR_SEMUTEX_ACQUIRE_HW_SCRUBBER_MUTEX_ACQUIRE_ERROR =                                  0x29D,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                  0x29E,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =            0x29F,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                      0x2A0,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                 0x2A1,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEX_RELEASE_ERROR =                                  0x2A2,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x2A3,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =              0x2A4,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =             0x2A5,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =            0x2A6,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x2A7,
        LW_UCODE_ERR_SEMUTEX_RELEASE_HW_SCRUBBER_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                0x2A8,
        LW_UCODE_ERR_GP_BUFFER_FREE_POINTER_NULL =                                                      0x2A9,
        LW_UCODE_ERR_IFF_NOT_DONE =                                                                     0x2AA,


        /*********  Values 0x400 to 0x7FF are reserved for UDE-specific error codes.                    *********/
        /*********  These values are will be written to the following register:                         *********/
        /*********  LW_UCODE_ERROR_CODE_UDE_GC6_REG_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(12)[31:16]           *********/
        LW_UDE_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                           0x400,
        LW_UDE_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ID_NOT_AVAIL =                                     0x401,
        LW_UDE_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_FAIL =                                     0x402,
        LW_UDE_ERR_SEC2MUTEX_ACQUIRE_VPR_SCRATCH_MUTEX_STATUS_ILWALID_POINTER =                         0x403,
        LW_UDE_ERR_SEC2MUTEX_ACQUIRE_VPR_SCRATCH_MUTEX_ID_NOT_AVAIL =                                   0x404,
        LW_UDE_ERR_SEC2MUTEX_ACQUIRE_VPR_SCRATCH_MUTEX_ACQUIRE_FAIL =                                   0x405,
        LW_UDE_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                           0x406,
        LW_UDE_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_RELEASE_FAIL =                                     0x407,
        LW_UDE_ERR_SEC2MUTEX_RELEASE_VPR_SCRATCH_MUTEX_STATUS_ILWALID_POINTER =                         0x408,
        LW_UDE_ERR_SEC2MUTEX_RELEASE_VPR_SCRATCH_MUTEX_RELEASE_FAIL =                                   0x409,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_HEADER_DATA_NOT_PRESENT =                            0x40A,
        LW_UDE_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_HEADER_SIGNATURE_TYPE_MISMATCH =                     0x40B,
        LW_UDE_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_IMEM2DMEM_PARAMETER_ILWALID =                        0x40C,
        LW_UDE_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =                 0x40D,
        LW_UDE_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE =         0x40E,
        LW_UDE_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_SIGNATURE_DATA_TYPE_DEVINIT =                        0x40F,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_HEADER_DATA_NOT_PRESENT =                           0x410,
        LW_UDE_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_HEADER_SIGNATURE_TYPE_MISMATCH =                    0x411,
        LW_UDE_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_IMEM2DMEM_PARAMETER_ILWALID =                       0x412,
        LW_UDE_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =                0x413,
        LW_UDE_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE =        0x414,
        LW_UDE_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_SIGNATURE_DATA_TYPE_DEVINIT =                       0x415,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        LW_UDE_ERR_UDE_VERSION_ILWALID =                                                                0x416,
        LW_UDE_ERR_VBIOS_VERIF_COMPLETED_AND_FAILED =                                                   0x417,
        LW_UDE_ERR_FWSECLIC_DID_NOT_EXELWTE =                                                           0x418,
        LW_UDE_ERR_UCODE_DMEM_VERIFICATION_FAILED =                                                     0x419,
        LW_UDE_ERR_UNALIGNED_SCRIPT_HEADER_DATA =                                                       0x41A,
        LW_UDE_ERR_IFF_PROCESSING_ERROR =                                                               0x41B,
        LW_UDE_ERR_IFF_ILWALID_RECORD =                                                                 0x41C,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_DEVINIT_AUTHENTICATE_DATA_PATCHER_HEADER_DATA_NOT_PRESENT =                          0x41D,
        LW_UDE_ERR_DEVINIT_AUTHENTICATE_DATA_PATCHER_HEADER_SIGNATURE_TYPE_MISMATCH =                   0x41E,
        LW_UDE_ERR_DEVINIT_AUTHENTICATE_DATA_PATCHER_IMEM2DMEM_PARAMETER_ILWALID =                      0x41F,
        LW_UDE_ERR_DEVINIT_AUTHENTICATE_DATA_PATCHER_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =               0x420,
        LW_UDE_ERR_DEVINIT_AUTHENTICATE_DATA_PATCHER_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE =       0x421,
        LW_UDE_ERR_DEVINIT_AUTHENTICATE_DATA_PATCHER_SIGNATURE_DATA_TYPE_DEVINIT =                      0x422,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_DEVINIT_VALIDATE_PATCHER_SIGNATURE_ILWALID_DATA =                                    0x423,
        LW_UDE_ERR_DEVINIT_RESIGN_DATA_PATCHER_HEADER_DATA_NOT_PRESENT =                                0x424,
        LW_UDE_ERR_DEVINIT_RESIGN_DATA_PATCHER_HEADER_SIGNATURE_TYPE_MISMATCH =                         0x425,
        LW_UDE_ERR_DEVINIT_RESIGN_DATA_PATCHER_DATA_SIZE_MISMATCH =                                     0x426,
        LW_UDE_ERR_DEVINIT_RESIGN_DATA_PATCHER_IMEM2DMEM_PARAMETER_ILWALID =                            0x427,
        LW_UDE_ERR_DEVINIT_RESIGN_DATA_PATCHER_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =                     0x428,
        LW_UDE_ERR_DEVINIT_RESIGN_DATA_PATCHER_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE =             0x429,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        LW_UDE_ERR_VPR_SCRUBBER_BINARY_HANDOFF_FAILURE =                                                0x42A,
        LW_UDE_ERR_DEVICE_ID_WITH_DEVINIT_SCRIPTS_MISMATCH =                                            0x42B,
        LW_UDE_ERR_DEVICE_ID_WITH_DEVINIT_TABLES_MISMATCH =                                             0x42C,
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =                      0x42D,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =                0x42E,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXPLM_READ_GETDATA_TIMEOUT =                          0x42F,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =                     0x430,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                   0x431,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                  0x432,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXID_READ_SENDREQUEST_TIMEOUT =                       0x433,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =                 0x434,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXID_READ_GETDATA_TIMEOUT =                           0x435,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXID_READ_GETDATA_REQUEST_FAIL =                      0x436,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXID_NOT_AVAILABLE =                                  0x437,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x438,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =                0x439,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                    0x43A,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x43B,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                        0x43C,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                   0x43D,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_SCRATCH_MUTEX_ACQUIRE_ERROR =                                    0x43E,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                    0x43F,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x440,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                        0x441,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                   0x442,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEX_RELEASE_ERROR =                                    0x443,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x444,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =                0x445,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x446,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =              0x447,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                   0x448,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_SCRATCH_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                  0x449,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =                    0x44A,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =              0x44B,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXPLM_READ_GETDATA_TIMEOUT =                        0x44C,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =                   0x44D,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x44E,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                0x44F,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT =                     0x450,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =               0x451,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXID_READ_GETDATA_TIMEOUT =                         0x452,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXID_READ_GETDATA_REQUEST_FAIL =                    0x453,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXID_NOT_AVAILABLE =                                0x454,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x455,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =              0x456,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                  0x457,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =            0x458,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                      0x459,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                 0x45A,
        LW_UDE_ERR_SEMUTEX_ACQUIRE_VPR_WPR_WRITE_MUTEX_ACQUIRE_ERROR =                                  0x45B,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                  0x45C,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =            0x45D,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                      0x45E,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                 0x45F,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEX_RELEASE_ERROR =                                  0x460,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =               0x461,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =              0x462,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =             0x463,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =            0x464,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                 0x465,
        LW_UDE_ERR_SEMUTEX_RELEASE_VPR_WPR_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                0x466,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////

        /*********  Values 0x800 to 0xBFF are reserved for GC6-specific error codes.                    *********/
        /*********  These values are will be written to the following register:                         *********/
        /*********  LW_UCODE_ERROR_CODE_UDE_GC6_REG_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(12)[31:16]           *********/
        LW_GC6_ERR_ACR_BINARY_HANDOFF_FAIL =                                                            0x800,
        LW_GC6_ERR_GC6_EXELWTION_ALREADY_DONE =                                                         0x801,
        LW_GC6_ERR_UCODE_UNKNOWN_SW_VERSION =                                                           0x802,
        LW_GC6_ERR_UCODE_ERROR_PARITY_CHECK =                                                           0x803,
        LW_GC6_ERR_UCODE_GPU_NOT_IN_GC6_EXIT =                                                          0x804,
        LW_GC6_ERR_UNALIGNED_SCRIPT_HEADER_DATA =                                                       0x805,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_GC6_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_HEADER_DATA_NOT_PRESENT =                            0x806,
        LW_GC6_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_HEADER_SIGNATURE_TYPE_MISMATCH =                     0x807,
        LW_GC6_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_IMEM2DMEM_PARAMETER_ILWALID =                        0x808,
        LW_GC6_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =                 0x809,
        LW_GC6_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE =         0x80A,
        LW_GC6_ERR_DEVINIT_TABLE_AUTHENTICATE_DATA_SIGNATURE_DATA_TYPE_DEVINIT =                        0x80B,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_GC6_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_HEADER_DATA_NOT_PRESENT =                           0x80C,
        LW_GC6_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_HEADER_SIGNATURE_TYPE_MISMATCH =                    0x80D,
        LW_GC6_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_IMEM2DMEM_PARAMETER_ILWALID =                       0x80E,
        LW_GC6_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =                0x80F,
        LW_GC6_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE =        0x810,
        LW_GC6_ERR_DEVINIT_SCRIPT_AUTHENTICATE_DATA_SIGNATURE_DATA_TYPE_DEVINIT =                       0x811,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////

        /*********  Values 0xC00 to 0xFFF are reserved for Compaction-specific error codes.             *********/
        /*********  These values are will be written to the following register:                         *********/
        /*********  LW_UCODE_ERROR_CODE_COMPACTION_REG_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(0)[31:16]         *********/
        LW_COMPACTION_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                    0xC00,
        LW_COMPACTION_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ID_NOT_AVAIL =                              0xC01,
        LW_COMPACTION_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_FAIL =                              0xC02,
        LW_COMPACTION_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                    0xC03,
        LW_COMPACTION_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_RELEASE_FAIL =                              0xC04,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_AUTHENTICATE_DATA_HEADER_DATA_NOT_PRESENT =                    0xC05,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_AUTHENTICATE_DATA_HEADER_SIGNATURE_TYPE_MISMATCH =             0xC06,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_AUTHENTICATE_DATA_IMEM2DMEM_PARAMETER_ILWALID =                0xC07,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_AUTHENTICATE_DATA_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =         0xC08,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_AUTHENTICATE_DATA_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE = 0xC09,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_AUTHENTICATE_DATA_SIGNATURE_DATA_TYPE_DEVINIT =                0xC0A,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_RESIGN_DATA_HEADER_DATA_NOT_PRESENT =                          0xC0B,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_RESIGN_DATA_HEADER_SIGNATURE_TYPE_MISMATCH =                   0xC0C,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_RESIGN_DATA_DATA_SIZE_MISMATCH =                               0xC0D,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_RESIGN_DATA_IMEM2DMEM_PARAMETER_ILWALID =                      0xC0E,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_RESIGN_DATA_DERIVEHWKEY_DOAESECB_NOT_IN_HSMODE =               0xC0F,
        LW_COMPACTION_ERR_COMPACT_SCRIPT_RESIGN_DATA_COMPUTEAESSIGNATURE_DOAESECB_NOT_IN_HSMODE =       0xC10,
        LW_COMPACTION_ERR_DEVICE_ID_WITH_DEVINIT_SCRIPTS_MISMATCH =                                     0xC11,
        LW_COMPACTION_ERR_UDE_NOT_EXELWTED =                                                            0xC12,
		LW_COMPACTION_ERR_REGISTERSAVERESTORE_ILWALID_OPCODE =                                          0xC13,
		LW_COMPACTION_ERR_REGISTERSAVERESTORE_ILWALID_REITERATE_COUNT =                                 0xC14,

        /*********  Values 0x1000 to 0x13FF are reserved for HULK specific error codes.                 *********/
        /*********  These values are will be written to the following register:                         *********/
        /*********  LW_UCODE_POST_CODE_HULK_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(5)[9:0]                      *********/
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =                       0x1000,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =                 0x1001,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_GETDATA_TIMEOUT =                           0x1002,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =                      0x1003,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =                    0x1004,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =                   0x1005,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT =                        0x1006,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =                  0x1007,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_GETDATA_TIMEOUT =                            0x1008,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_READ_GETDATA_REQUEST_FAIL =                       0x1009,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXID_NOT_AVAILABLE =                                   0x100A,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =                  0x100B,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =                 0x100C,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =                     0x100D,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =               0x100E,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =                         0x100F,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =                    0x1010,
        LW_HULK_ERR_SEMUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_ERROR =                                     0x1011,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        LW_HULK_ILWALID_COMMAND =                                                                       0x1012,

        /*********  Values 0x1400 to 0x17FF are reserved for Bootloader-specific error codes.           *********/
        /*********  These values are will be written to the following register:                         *********/
        /*********  LW_UCODE_ERROR_CODE_BOOTLOADER_REG_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(14)[15:0]         *********/

        /*********  Values 0x1800 to 0x1BFF are reserved for PREOS Switcher-specific error codes.       *********/
        /*********  These values are will be written to the following register:                         *********/
        /*********  LW_UCODE_ERROR_CODE_PREOS_REG_ERR_CODE(LW_PBUS_VBIOS_SCRATCH(1)[31:16]              *********/
        LW_PREOS_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                         0x1800,
        LW_PREOS_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ID_NOT_AVAIL =                                   0x1801,
        LW_PREOS_ERR_SEC2MUTEX_ACQUIRE_BSI_WRITE_MUTEX_ACQUIRE_FAIL =                                   0x1802,
        LW_PREOS_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =                         0x1803,
        LW_PREOS_ERR_SEC2MUTEX_RELEASE_BSI_WRITE_MUTEX_RELEASE_FAIL =                                   0x1804,
        LW_PREOS_ERR_SEC2MUTEX_EXIT_POINT_RELEASE_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =              0x1805,
        LW_PREOS_ERR_SEC2MUTEX_EXIT_POINT_RELEASE_BSI_WRITE_MUTEX_RELEASE_FAIL =                        0x1806,
        LW_PREOS_ERR_SEC2MUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEX_STATUS_ILWALID_POINTER =              0x1807,
        LW_PREOS_ERR_SEC2MUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEX_ID_NOT_AVAIL =                        0x1808,
        LW_PREOS_ERR_SEC2MUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEX_ACQUIRE_FAIL =                        0x1809,
        LW_PREOS_ERR_DEVICE_ID_MISMATCH =                                                               0x180A,
        LW_PREOS_ERR_UCODE_VALIDATION_REVLOCK_ID_MISMATCH =                                             0x180B,
        LW_PREOS_ERR_UCODE_VALIDATION_CHIP_ID_ILWALID =                                                 0x180C,
        LW_PREOS_ERR_FR_CNTR_PWRCLK_FREQ =                                                              0x180D,
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT =           0x180E,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXPLM_READ_SENDREQUEST_TIMEOUT_DUMMY =     0x180F,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXPLM_READ_GETDATA_TIMEOUT =               0x1810,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXPLM_READ_GETDATA_REQUEST_FAIL =          0x1811,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =        0x1812,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =       0x1813,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT =            0x1814,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXID_READ_SENDREQUEST_TIMEOUT_DUMMY =      0x1815,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXID_READ_GETDATA_TIMEOUT =                0x1816,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXID_READ_GETDATA_REQUEST_FAIL =           0x1817,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXID_NOT_AVAILABLE =                       0x1818,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =      0x1819,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =     0x181A,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =         0x181B,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =   0x181C,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =             0x181D,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =        0x181E,
        LW_PREOS_ERR_SEMUTEX_ACQUIRE_EXIT_POINT_BSI_WRITE_MUTEX_ACQUIRE_ERROR =                         0x181F,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////
        /* These enums are populated by helper function. Any changes here might require updates to call sites. */
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT =         0x1820,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_SENDREQUEST_TIMEOUT_DUMMY =   0x1821,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_TIMEOUT =             0x1822,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_READ_GETDATA_REQUEST_FAIL =        0x1823,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEX_RELEASE_ERROR =                         0x1824,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_CHANNEL_EMPTY =      0x1825,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXMUTEX_WRITE_TIMEOUT_WRITE_COMPLETE =     0x1826,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_CHANNEL_EMPTY =    0x1827,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXRELEASE_WRITE_TIMEOUT_WRITE_COMPLETE =   0x1828,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_CHANNEL_EMPTY =        0x1829,
        LW_PREOS_ERR_SEMUTEX_RELEASE_EXIT_POINT_BSI_WRITE_MUTEXPLM_WRITE_TIMEOUT_WRITE_COMPLETE =       0x182A,
        ////////////////////////////////////////////////////////////////////////////////////////////////////////

        /*********  Values 0x1C00 to 0x1FFF are reserved for PREOS fanctrl-specific error codes.        *********/
        /*********  These values are will be written to the same register as PREOS.                     *********/

        /*********  Values 0x2000 to 0x23FF are reserved for PREOS oobmon-specific error codes.         *********/
        /*********  These values are will be written to the same register as PREOS.                     *********/

        /*********  Values 0x2400 to 0x27FF are reserved for PREOS dpiim-specific error codes.          *********/
        /*********  These values are will be written to the same register as PREOS.                     *********/

        /*********  Values 0x2800 to 0x2BFF are reserved for PREOS Brightness-specific error codes.     *********/
        /*********  These values are will be written to the same register as PREOS.                     *********/

        /*********  Values 0x2C00 to 0x2FFF are reserved for fbFalcon running FRTS                      *********/
        /*********  These values are will be written to LW_UCODE_POST_CODE_FBFALCON_FRTS_REG11,         *********/
        /*********   LW_UCODE_POST_CODE_FBFALCON_FRTS_ERR_CODE  15:0                                    *********/
        LW_FBFALCON_FRTS_ERR_MEMORY_INFORMATION_TABLE_NOT_FOUND =                                       0x2C00,
        LW_FBFALCON_FRTS_ERR_MEMORY_INFORMATION_TABLE_WRONG_VERSION =                                   0x2C01,
        LW_FBFALCON_FRTS_ERR_MEMORY_TRAINING_TABLE_NOT_FOUND =                                          0x2C02,
        LW_FBFALCON_FRTS_ERR_MEMORY_TRAINING_TABLE_WRONG_VERSION =                                      0x2C03,
        LW_FBFALCON_FRTS_ERR_MEMORY_TRAINING_PATTERN_TABLE_NOT_FOUND =                                  0x2C04,
        LW_FBFALCON_FRTS_ERR_MEMORY_TRAINING_PATTERN_TABLE_WRONG_VERSION =                              0x2C05,

}LW_UCODE_UNIFIED_ERROR_TYPE;

/***************************************************************************************************************
*
*     footnote code definitions
*
*     Note1:  Footnote code is used to annotate error code.
*            Different error code could have footnode code with same value.
*            Combining error code, feature code and footnote code will form an unique value to present an unique error case.
*
*     Note2: Footnote code label format:
*            LW_[uCode Name]_FOOTNOTE_[Error Code label without LW_[uCode]_ERR_CODE prefix]_[footnote code fields]
*            For example:  a fwseclic footnote code field to store the iteration informaiton for error code LW_FWSECLIC_ERR_CODE_CERT_HULK_REG_READ_BACK_VERIFY_MISMATCH,
*                Its label should be:   LW_FWSECLIC_FOOTNOTE_CERT_HULK_REG_READ_BACK_VERIFY_MISMATCH_ITERATION
*    Note3:  Multiple error codes can share one footnote code define. to avoid a very long footnot code name,
*            you can mention the error codes in footnote code common,  and only choose one error code to be used in footnote code name.
*
*****************************************************************************************************************/

//
//  footnote code for below error code:
//      LW_UCODE_ERR_GP_BUFFER_ALLOCATION_FAILED
//      LW_UCODE_ERR_GP_BUFFER_ALLOCATION_NUMBER_NOT_SUPPORTED
//      LW_UCODE_ERR_GP_BUFFER_ALLOCATION_ILWALID_SIZE
//      LW_UCODE_ERR_GP_BUFFER_ALLOCATION_TOTAL_SIZE_MISMATCH
//
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_VV
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_SOURCE_RM_HULK_INST1                                  1
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_SOURCE_VV_INST1                                      10
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_SOURCE_VV_INST2                                      11
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_SOURCE_VV_INST3                                      12
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_SOURCE_FRTS_INST1                                    30
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_SOURCE_VDPA_INST1                                    60
#define LW_UCODE_FOOTNOTE_GP_BUFFER_ALLOCATION_FAILED_SOURCE_INFOROM_INST1                                 90

//
//  footnote code for below error code:
//      LW_UCODE_ERR_GP_BUFFER_TAKE_FAILED
//      LW_UCODE_ERR_GP_BUFFER_TAKE_NUMBER_NOT_SUPPORTED
//
#define LW_UCODE_FOOTNOTE_GP_BUFFER_TAKE_FAILED_VV
#define LW_UCODE_FOOTNOTE_GP_BUFFER_TAKE_FAILED_SOURCE_RM_HULK_INST1                                        1
#define LW_UCODE_FOOTNOTE_GP_BUFFER_TAKE_FAILED_SOURCE_FRTS_INST1                                          30
#define LW_UCODE_FOOTNOTE_GP_BUFFER_TAKE_FAILED_SOURCE_FRTS_INST2                                          31
#define LW_UCODE_FOOTNOTE_GP_BUFFER_TAKE_FAILED_SOURCE_INFOROM_INST1                                       60


//  footnote code for below error code:
//      LW_UCODE_ERR_GP_BUFFER_FREE_ILWALID_USAGE_MASK
//      LW_UCODE_ERR_GP_BUFFER_FREE_BUFFER_MAX_NUMBER_EXCEED
//      LW_UCODE_ERR_GP_BUFFER_FREE_POINTER_NOT_FOUND
#define LW_UCODE_FOOTNOTE_GP_BUFFER_FREE_ILWALID_USAGE_MASK_SOURCE_VV_INST1                                  1
#define LW_UCODE_FOOTNOTE_GP_BUFFER_FREE_ILWALID_USAGE_MASK_SOURCE_FRTS_INST1                               20
#define LW_UCODE_FOOTNOTE_GP_BUFFER_FREE_ILWALID_USAGE_MASK_SOURCE_HULK_INST1                               40
#define LW_UCODE_FOOTNOTE_GP_BUFFER_FREE_ILWALID_USAGE_MASK_SOURCE_VDPA_INST1                               60

//  footnote for below error code:
//      LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_UNEXPECTED_MAJOR_TYPE
//
#define LW_FWSECLIC_FOOTNOTE_BCRT2X_VDPA_UNEXPECTED_MAJOR_TYPE_FUNC                                        7:0  // This field hold tbe defines for the function where the error occur
#define LW_FWSECLIC_FOOTNOTE_BCRT2X_VDPA_UNEXPECTED_MAJOR_TYPE_FUNC_vdpaFixupEntries                        1
#define LW_FWSECLIC_FOOTNOTE_BCRT2X_VDPA_UNEXPECTED_MAJOR_TYPE_FUNC_vdpaGetEntryByDirtID                    2
#define LW_FWSECLIC_FOOTNOTE_BCRT2X_VDPA_UNEXPECTED_MAJOR_TYPE_ITERATION                                  16:8  // This field hold the iteration during VDPA entries walk through

//  footnote for below error code:
//      LW_FWSECLIC_ERR_CODE_CERT_NO_EXTENSION_EXIST
//      LW_FWSECLIC_ERR_CODE_CERT_EXT_NOT_FOUND
//
#define LW_FWSECLIC_FOOTNOTE_BCRT2X_CERT_EXT_NOT_FOUND_INST1                                                 1   // this instance is used to preparing vv command output struct for Vendor Name

//  footnote for below error code:
// LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_BUILT_IN_SEC_ZONE_MISSING
// LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIGNATURE_ILWALID
// LW_UCODE_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_NOT_FOUND
// LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_BUILT_IN_SEC_ZONE_MISSING
// LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIGNATURE_ILWALID
// LW_FWSECLIC_ERR_CODE_BCRT2X_SELWRITYZONE_SIG_NOT_FOUND
#define LW_UCODE_FOOTNOTE_BCRT2X_SELWRITYZONE_SIGNATURE_ILWALID_SEC_ZONE                                     7:0 // This field holds the security zone number for its error code

//  footnote for below error code:
//    LW_UCODE_ERR_CODE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH
//    LW_FWSECLIC_ERR_CODE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH
#define LW_UCODE_FOOTNOTE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH_VDPA_MAJOR_TYPE                             7:0  // This field holds the Major Type of the failed VDPA entry
#define LW_UCODE_FOOTNOTE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH_VDPA_MINOR_TYPE                            15:8  // This field holds the Minor Type of the failed VDPA entry
#define LW_UCODE_FOOTNOTE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH_VDPA_USAGE                                 23:16 // This field holds the DIRT ID, ( usage  ) of the failed VDPA entry
#define LW_UCODE_FOOTNOTE_BCRT2X_VDPA_ENTRY_VERIFY_HASH_MISMATCH_VDPA_VERSION                               31:24 // This field holds the version of VDPA Entry Structure of the failed VDPA entry



#endif // UCODE_POSTCODES_H
