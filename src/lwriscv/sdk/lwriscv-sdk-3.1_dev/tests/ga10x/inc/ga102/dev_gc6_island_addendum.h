/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    dev_gc6_island_addendum.h
 * @brief   GPU specific defines that are missing in the dev_gc6_island.h manual.
 */

#ifndef __ga102_dev_gc6_island_addendum_h__
#define __ga102_dev_gc6_island_addendum_h__


// Max available period for SCI_VID_PWM
#define LW_PGC6_SCI_VID_CFG0_PERIOD_MAX                  0x00000FFF /* ----V */

// GC6 phases
#define LW_PGC6_BSI_PHASE                                31:0
#define LW_PGC6_BSI_PHASE_OFFSET                         1
#define LW_PGC6_BSI_PHASE_SETUP                          0
#define LW_PGC6_BSI_PHASE_ACR_LOCKDOWN_NONSELWRE         1
#define LW_PGC6_BSI_PHASE_ACR_LOCKDOWN_SELWRE            2
#define LW_PGC6_BSI_PHASE_COPY_NONSELWRE                 3
#define LW_PGC6_BSI_PHASE_COPY_SELWRE                    4
#define LW_PGC6_BSI_PHASE_SCRIPT                         5
#define LW_PGC6_BSI_ACR_BOOTSTRAP_NONSELWRE              6
#define LW_PGC6_BSI_ACR_BOOTSTRAP_SELWRE                 7
#define LW_PGC6_BSI_ACR_BOOTSTRAP_DUMMY                  8
#define LW_PGC6_BSI_PMU_BOOTSTRAP                        9
// change to the future last phase index
#define LW_PGC6_BSI_GC6_PHASE_MAX                        LW_PGC6_BSI_PMU_BOOTSTRAP
// the boot phase where GPU is actually up
#define LW_PGC6_BSI_GC6_BOOTUP_COMPLETE_PHASE            (LW_PGC6_BSI_GC6_PHASE_MAX + LW_PGC6_BSI_PHASE_OFFSET)

// compaction profiling is done only in cold boot with compaction bin.
#define LW_PGC6_BSI_SCRATCH_INDEX_PMU_COMPACTION_COPY    (0)
#define LW_PGC6_BSI_SCRATCH_INDEX_PMU_COMPACTION_EXEC    (1)
// BSI RAM copy is done only in cold boot with PMU RTOS
#define LW_PGC6_BSI_SCRATCH_INDEX_BSI_COPY               (1)
//
// Register to sync self driving reset
// Host reset in fmodel resets most of the chip and engine post unload can go wrong
// Host reset was implemented in GA10X fmodel
//
#define LW_PGC6_BSI_SCRATCH_INDEX_BSI_RESET_SYNC         (1)
// GC6 entry error logs GC6 error happend after L2
// TODO-SC remove BSI one in GA102+
#define LW_PGC6_BSI_SCRATCH_INDEX_GC6_ENTRY_ERROR        (0)
#define LW_PGC6_SCI_SCRATCH_INDEX_GC6_ENTRY_ERROR        (0)
// GC6 D3Hot entry confirms this is lwrrently a D3Hot transaction
#define LW_PGC6_BSI_SCRATCH_INDEX_GC6_D3HOT_ENTRY        (2)
// GCOFF D3Hot entry happens before GPU state load
#define LW_PGC6_BSI_SCRATCH_INDEX_GCOFF_D3HOT_ENTRY      (0)

#define LW_PGC6_BSI_SELWRE_SCRATCH_0_MEMINFO_INDEX      4:0
#define LW_PGC6_BSI_SELWRE_SCRATCH_0_RSVD               31:5

// reserved scratch registers for FW data runtime security or in short, FRTS
// Note:
//    1. We have two FRTS Secure Structures defined in  LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(0) to LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(2)
//        Input Structure:
//           typedef struct {
//              LwU16   ctrl;           // version, Media Type,  wpr ID and etc.,   in LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(2) 15:0
//              LwU32   wpr_offset;     // Start offset of VBIOS FW section in WPR,   in  LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(1) 31:0
//              LwU16   wpr_size;       // VBIOS FW section,  in LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(0) 15:0
//           } IN_CMD_FRTS_SEC, *PIN_CMD_FRTS_SEC;
//        Output Structure:
//           typedef struct {
//             LwU16    flag;       // command and ACR status and etc.  in LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(2) 31:16
//           } OUT_CMD_FRTS_SEC, *POUT_CMD_FRTS_SEC;
//
//    2. Both WPR OFFSET and SIZE fields are in unit of 4K Bytes.
//
//
#define LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0           LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(0)
#define LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE                                   15:0
#define LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_WPR_SIZE_1MB_IN_4K                        0x100
#define LW_PGC6_AON_FRTS_INPUT_WPR_SIZE_SELWRE_SCRATCH_GROUP_03_0_RSVD                                      31:16

#define LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1         LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(1)
#define LW_PGC6_AON_FRTS_INPUT_WPR_OFFSET_SELWRE_SCRATCH_GROUP_03_1_WPR_OFFSET                               31:0


#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(2)
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_USAGE                                           15:0
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_VERSION                                         3:0
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_VERSION_VER_1                                     1
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_MEDIA_TYPE                                      7:4
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_MEDIA_TYPE_FB                                   0x0
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_MEDIA_TYPE_SYSMEM                               0x1
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_WPR_ID                                         15:8
#define LW_PGC6_AON_FRTS_INPUT_CTRL_SELWRE_SCRATCH_GROUP_03_2_USED                                          31:16

#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_03(2)
#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_USED                                          15:0
#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_CMD_SUCCESSFUL                               16:16
#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_CMD_SUCCESSFUL_YES                               1
#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_CMD_SUCCESSFUL_NO                                0
#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_ACR_READY                                    17:17
#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_ACR_READY_YES                                    1
#define LW_PGC6_AON_FRTS_OUTPUT_FLAG_SELWRE_SCRATCH_GROUP_03_2_ACR_READY_NO                                     0

// Reserved for storing GSP Ucode version
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00                             LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04(0)
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_GSP_UCODE_VERSION                                              4:0
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04__PRIV_LEVEL_MASK
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK_READ_PROTECTION                                2:0
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED      0x00000007
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK_READ_VIOLATION                                 3:3
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04_00_PRIV_LEVEL_MASK_READ_VIOLATION_REPORT_ERROR             0x00000001

// Reserved scratch registers for VPR
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15                                           LW_PGC6_BSI_SELWRE_SCRATCH_15

#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_MAX_RANGE_START_ADDR_MB_ALIGNED                                19:0

#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VBIOS_UDE_VERSION                                                 24:20

#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_SCRUBBER_BINARY_VERSION                                           28:25

#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF                                                       31:29
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_CHIP_RESET                                 0x00000000
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_DEVINIT_DONE                               0x00000001
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_SCRUBBER_BIN_TAKING_OVER                   0x00000002
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_SCRUBBER_BIN_DONE                          0x00000003
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_RESERVED_FOR_FUTURE0                       0x00000004
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_RESERVED_FOR_FUTURE1                       0x00000005
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_RESERVED_FOR_FUTURE2                       0x00000006
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_HANDOFF_VALUE_RESERVED_FOR_FUTURE3                       0x00000007



#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14                                           LW_PGC6_BSI_SELWRE_SCRATCH_14

#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_LWRRENT_VPR_RANGE_START_ADDR_MB_ALIGNED                            19:0
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_ACR_BINARY_VERSION                                                23:20
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_14_RESERVED                                                          31:24


#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13                                           LW_PGC6_BSI_SELWRE_SCRATCH_13
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13_MAX_VPR_SIZE_MB                                                    15:0
#define LW_PGC6_BSI_VPR_SELWRE_SCRATCH_13_LWRRENT_VPR_SIZE_MB                                               31:16

// Reserved for storing HDCP VPR Policy
#define LW_PGC6_AON_VPR_SELWRE_SCRATCH_VPR_BLANKING_POLICY_CTRL            LW_PGC6_AON_SELWRE_SCRATCH_GROUP_04(3)

// Reserved scratch register for storing LOCAL_MEMORY_RANGE
#define LW_PGC6_BSI_SELWRE_SCRATCH_MMU_LOCAL_MEMORY_RANGE                           LW_PGC6_BSI_SELWRE_SCRATCH_12


#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_11                                           LW_PGC6_BSI_SELWRE_SCRATCH_11
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_11_WPR1_SIZE_IN_128KB_BLOCKS                                          15:0
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_11_WPR2_SIZE_IN_128KB_BLOCKS                                         31:16


#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10                                           LW_PGC6_BSI_SELWRE_SCRATCH_10
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_WPR2_START_ADDR_128K_ALIGNED                                       22:0
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_RESERVED                                                          23:23
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_READ_WPR2                                                   27:24
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_READ_WPR2_SELWRE0                                           24:24
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_READ_WPR2_SELWRE1                                           25:25
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_READ_WPR2_SELWRE2                                           26:26
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_READ_WPR2_SELWRE3                                           27:27
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_WRITE_WPR2                                                  31:28
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_WRITE_WPR2_SELWRE0                                          28:28
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_WRITE_WPR2_SELWRE1                                          29:29
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_WRITE_WPR2_SELWRE2                                          30:30
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_10_ALLOW_WRITE_WPR2_SELWRE3                                          31:31

#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9                                             LW_PGC6_BSI_SELWRE_SCRATCH_9
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_WPR1_START_ADDR_128K_ALIGNED                                        22:0
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_RESERVED                                                           23:23
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_READ_WPR1                                                    27:24
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_READ_WPR1_SELWRE0                                            24:24
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_READ_WPR1_SELWRE1                                            25:25
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_READ_WPR1_SELWRE2                                            26:26
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_READ_WPR1_SELWRE3                                            27:27
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_WRITE_WPR1                                                   31:28
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_WRITE_WPR1_SELWRE0                                           28:28
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_WRITE_WPR1_SELWRE1                                           29:29
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_WRITE_WPR1_SELWRE2                                           30:30
#define LW_PGC6_BSI_WPR_SELWRE_SCRATCH_9_ALLOW_WRITE_WPR1_SELWRE3                                           31:31


// Reserved scratch registers for HUB Encryption
#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_START_IDX                                                     4

#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_7                                  LW_PGC6_BSI_SELWRE_SCRATCH_7
#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_7_KEY_PART                                                 31:0

#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_6                                  LW_PGC6_BSI_SELWRE_SCRATCH_6
#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_6_KEY_PART                                                 31:0

#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_5                                  LW_PGC6_BSI_SELWRE_SCRATCH_5
#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_5_KEY_PART                                                 31:0

#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_4                                  LW_PGC6_BSI_SELWRE_SCRATCH_4
#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_4_KEY_PART                                                 31:0

#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH(i) \
                              LW_PGC6_BSI_SELWRE_SCRATCH(LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH_START_IDX + i)

#define LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH__SIZE_1                                                       4

#define LW_PGC6_SCI_TEMP_SENSOR_ID_COUNT                                                                        1

// Sizes for LW_PGC6_BSI_PWRCLK_DOMAIN_FEATURES_BSI_RAM_OPCODE
#define LW_PGC6_BAR0_OP_SIZE_DWORD 3
#define LW_PGC6_PRIV_OP_SIZE_DWORD 2

// BSI RAM size (bytes) to reserve for general purpose storage
#define BSI_RAM_RSVD_SIZE   512

// Defining size of phase descriptor array as it is a static value
#define RM_PMU_PG_ISLAND_PHASE_DESCRIPTOR_SIZE  20

//
// Reserving GROUP_05 for GFW use cases
//
// Requirements:
// 1. All registers in this group will have level 0 read access and level 3 write access
// 2. Only FwSec shall change the PLM for this group
// 3. Each register in this group should only be written to by a single HS uCode
// 4. No HULK license should reduce the PLM for this group
//
// Group 5 Index 0 reserved for GFW BOOT use cases:
// BOOT_PROGRESS is to denote progress of boot uCodes
// VALIDATION_STATUS_* fields defined to match PCI spec for LW_XVE_ROM_CTRL_VALIDATION_STATUS in dev_lw_xve3g_fn0.ref (HW refman)
// RM header: https://p4viewer.lwpu.com/get///sw/dev/gpu_drv/chips_a/drivers/common/inc/hwref/turing/tu102/dev_lw_xve.h
// Reserving bit 15 to start next allocation on a nibble boundary
//
// Validation Status enumeration details from the PCIE ECN (the ECN is attached in http://lwbugs/1844636, attachment: ECN-Expansion ROM Validation-2017-07-20.pdf).
//
// VALIDATION_STATUS:
//
// 000b               Not supported
// 001b               In Progress
// 010b               Pass No Trust              Valid contents, trust test was not performed
// 011b               Pass Trusted               Valid and trusted contents
// 100b               Fail                       Invalid contents
// 101b               Pass Untrusted             Valid but untrusted contents (e.g., Out of Date, Expired or Revoked Certificate)
// 110b               Warn No trust              Passed with implementation-specific warning. Valid contents, trust test was not performed
// 111b               Warn trusted               Passed with implementation-specific warning. Valid and trusted contents
//
//
// VALIDATION_DETAILS:
// Expansion ROM Validation Details  contains optional, implementation-specific details associated with Expansion ROM Validation.
// -    If the Function does not support validation, this field is RsvdP.
// -    This field is optional. When validation is supported and this field is not implemented, this field must be hardwired to 0000b.
//      Any unused bits in this field are permitted to be hardwired to 0b.
// -    If validation is in progress (Expansion ROM Validation Status is 001b), non-zero values of this field represent implementation-specific
//      indications of the phase of the validation progress (e.g., 50% complete). The value 0000b indicates that no validation progress information is provided.
// -    If validation is completed (Expansion ROM Validation Status 010b to 111b inclusive), non-zero values in this field represent additional implementation-specific
//      information. The value 0000b indicates that no information is provided.
//
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT                                                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05(0)
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS                                                                          7:0
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_NOT_STARTED                                                       0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_STARTED                                                           0x00000001
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_BOOT_VALIDATION_COMPLETED                                         0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_FBFALCON_BOOT_TRAINING_COMPLETED                                  0x00000003
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_FB_SCRUB_STARTED                                                  0x00000004
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_FB_SCRUB_SYNC_RM_KMD_HEAP_REGION_COMPLETED                        0x00000005
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_FB_SCRUB_SYNC_DISPLAY_REGION_COMPLETED                            0x00000006
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_FB_SCRUB_ASYNC_REST_OF_FB_STARTED                                 0x00000007
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_VPR_RANGE_SETUP_COMPLETED                                         0x00000008
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_COMPLETED                                                         0x000000FF
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS                                                                10:8
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_UNSUPPORTED                                              0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_IN_PROGRESS                                              0x00000001
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_PASS_NO_TRUST                                            0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_PASS_TRUSTED                                             0x00000003
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_FAIL                                                     0x00000004
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_PASS_UNTRUSTED                                           0x00000005
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_WARN_NO_TRUST                                            0x00000006
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_STATUS_WARN_TRUSTED                                             0x00000007
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_DETAILS                                                              14:11
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_VALIDATION_DETAILS_INIT                                                    0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_RESERVED                                                                        15:15

// Group 5 Index 1 use case:
// Firmware Protection: Write-Protect Mode
// https://confluence.lwpu.com/display/GFWT/Firmware+Protection
// When WRITE_PROTECT_MODE == _ENABLED, LWFlash will not write to the EEPROM
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1                                                         LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05(1)
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1_WRITE_PROTECT_MODE                                                                         0:0
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1_WRITE_PROTECT_MODE_DISABLED                                                         0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1_WRITE_PROTECT_MODE_ENABLED                                                          0x00000001

// Physical (absolute) offset and size of EEPROM that can be modified at run-time. Both offset and size are units of 4 KB.
// The InfoROM file system starts at InfoROM Carveout Offset.
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1_INFOROM_CARVEOUT_OFFSET                                                                   19:8
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1_INFOROM_CARVEOUT_SIZE                                                                    31:20

// These bits will be used as secure ledger while exelwting FWSEC SB command.
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER                                               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05(2)
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_SET_GPUDEVID                                                                     1:0
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_SET_GPUDEVID_NOT_COMPLETED                                                0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_SET_GPUDEVID_COMPLETED                                                    0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_PRELTSSM_DEVINIT                                                                 3:2
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_PRELTSSM_DEVINIT_NOT_COMPLETED                                            0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_PRELTSSM_DEVINIT_COMPLETED                                                0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_PREDEVINIT_LICENSE_PROCESS                                                       5:4
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_PREDEVINIT_LICENSE_PROCESS_NOT_COMPLETED                                  0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_PREDEVINIT_LICENSE_PROCESS_COMPLETED                                      0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_FLOORSWEEP                                                                       7:6
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_FLOORSWEEP_NOT_COMPLETED                                                  0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_FLOORSWEEP_COMPLETED                                                      0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_POSTLTSSM_DEVINIT                                                                9:8
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_POSTLTSSM_DEVINIT_NOT_COMPLETED                                           0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_POSTLTSSM_DEVINIT_COMPLETED                                               0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FRTS                                                                      11:10
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FRTS_NOT_COMPLETED                                                   0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FRTS_COMPLETED                                                       0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FBFALCON                                                                  13:12
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FBFALCON_NOT_COMPLETED                                               0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FBFALCON_COMPLETED                                                   0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_VPR_PROGRAMMING                                                           15:14
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_VPR_PROGRAMMING_NOT_COMPLETED                                        0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_VPR_PROGRAMMING_COMPLETED                                            0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FRTS_CLOSE                                                                17:16
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FRTS_CLOSE_NOT_COMPLETED                                             0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_BOOT_FRTS_CLOSE_COMPLETED                                                 0x00000002
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_POSTDEVINIT_LICENSE_PROCESS                                                    19:18
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_POSTDEVINIT_LICENSE_PROCESS_NOT_COMPLETED                                 0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_2_SB_LEDGER_POSTDEVINIT_LICENSE_PROCESS_COMPLETED                                     0x00000002

#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_3                                                         LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05(3)

// Upper 12 bits of 4KB aligned offset of ROM Directory in the GFW image.
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_3_ROM_DIRECTORY_OFFSET                                                                      11:0
// Upper 12 bits of 4KB aligned rom address offset. Secondary image is guarenteed to be at 4KB boundary.
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_3_ROM_ADDRESS_OFFSET                                                                       23:12

//
// Reserving GROUP_17 for GFW PL1 write use cases
//
// Requirements:
// 1. Allow level 0 read access and levels [1,3] write access
// 2. Only FwSec shall change the PLM for this group
// 3. The register in this group should only be written to by the GFW PL1 uCode
// 4. No HULK license should further reduce the PLM for this group
//
// Bit Allocations:
//
// [0:0] PL1 writable Write-Protect Mode
//       https://confluence.lwpu.com/display/GFWT/Firmware+Protection
//       When WRITE_PROTECT_MODE_1 == _ENABLED, LWFlash will not write to the EEPROM
//
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_WRITE_PROTECT_MODE                                                                           0:0
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_WRITE_PROTECT_MODE_DISABLED                                                           0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_WRITE_PROTECT_MODE_ENABLED                                                            0x00000001

// [1:1] PL1 writeable USB State
//       USB_STATE is added as part of requirement to disable USB in bug http://lwbugs/2182366.
//       LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_STATE will be set through PCI Config PBI interface, handled by PBI PreOS app.
//       Along with configuring LW_PTRIM_SYS_XUSB_UTMIPLL_CFG, PBI PreOs need to store the current USB State in secure persistent storage (AON register) so that it can be selwrely restored on GC6 exit.
//       We can only actively disable USB when the default settings (Fuse and board settings) enables it, but we cannot enable USB if the default setting do not allow it.
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_STATE                                                                                    1:1
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_STATE_DEFAULT                                                                     0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_STATE_DISABLED                                                                    0x00000001

// [2:2] PL1 writeable USB Lock
//       USB_LOCK is added as part of requirement to disable USB in bug http://lwbugs/2182366
//       LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_LOCK will be set through PCI Config PBI interface, handled by PBI PreOS app.
//       When LOCK == _ENABLED, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_STATE cannot be changed.
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_LOCK                                                                                     2:2
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_LOCK_DISABLED                                                                     0x00000000
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_17_USB_LOCK_ENABLED                                                                      0x00000001

//
// Reserving GROUP_18 for GFW use cases
//
// Requirements:
// 1. Allow level 0 read access and level 3 write access
// 2. Only FwSec shall change the PLM for this group
// 3. The register in this group should only be written to by the GFW uCode
// 4. No HULK license should further reduce the PLM for this group
//

// Physical (absolute) offset and size of EEPROM allocated to the Erase Ledger. Both offset and size are units of 4 KB.
// https://confluence.lwpu.com/display/CSSRM/Erase+Ledger+Design
// The ledger sectors start at InfoROM Carveout Offset.
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_18_ERASE_LEDGER_CARVEOUT_OFFSET                                                                11:0
#define LW_PGC6_AON_SELWRE_SCRATCH_GROUP_18_ERASE_LEDGER_CARVEOUT_SIZE                                                                 23:12

//
// AON secure scratch usage for Falcon SubWpr settings save-restore across GC6
// Lwrrently GROUPs 00, 01, 02, 10, 09, 08, 07 are under use
// Further we plan to use GROUP 06 and lower i.e. we plan to grow downwards
//
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_0_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(0)
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_0_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(1)
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_1_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(2)
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_1_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(3)
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_2_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_08(2)
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_2_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_08(3)
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_0_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_1_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_PMU_SUB_WPR_ID_2_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_08_PRIV_LEVEL_MASK

#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_0_CFGA               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(4)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_0_CFGB               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(5)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_1_CFGA               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(6)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_1_CFGB               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00(7)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_3_CFGA               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_07(0)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_3_CFGB               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_07(1)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_4_CFGA               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_07(2)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_4_CFGB               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_07(3)
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_0_PRIV_LEVEL_MASK    LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_1_PRIV_LEVEL_MASK    LW_PGC6_AON_SELWRE_SCRATCH_GROUP_00_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_3_PRIV_LEVEL_MASK    LW_PGC6_AON_SELWRE_SCRATCH_GROUP_07_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_SEC2_SUB_WPR_ID_4_PRIV_LEVEL_MASK    LW_PGC6_AON_SELWRE_SCRATCH_GROUP_07_PRIV_LEVEL_MASK

#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_0_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(0)
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_0_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(1)
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_1_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(2)
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_1_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(3)
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_2_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_08(0)
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_2_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_08(1)
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_0_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_1_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_GSP_SUB_WPR_ID_2_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_08_PRIV_LEVEL_MASK

#define LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_0_CFGA               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(4)
#define LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_0_CFGB               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(5)
#define LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_1_CFGA               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(6)
#define LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_1_CFGB               LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01(7)
#define LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_0_PRIV_LEVEL_MASK    LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_FECS_SUB_WPR_ID_1_PRIV_LEVEL_MASK    LW_PGC6_AON_SELWRE_SCRATCH_GROUP_01_PRIV_LEVEL_MASK

#define LW_PGC6_SELWRE_SCRATCH_GPCCS_SUB_WPR_ID_0_CFGA              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(0)
#define LW_PGC6_SELWRE_SCRATCH_GPCCS_SUB_WPR_ID_0_CFGB              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(1)
#define LW_PGC6_SELWRE_SCRATCH_GPCCS_SUB_WPR_ID_1_CFGA              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(2)
#define LW_PGC6_SELWRE_SCRATCH_GPCCS_SUB_WPR_ID_1_CFGB              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(3)
#define LW_PGC6_SELWRE_SCRATCH_GPCCS_SUB_WPR_ID_0_PRIV_LEVEL_MASK   LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_GPCCS_SUB_WPR_ID_1_PRIV_LEVEL_MASK   LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02_PRIV_LEVEL_MASK

#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_0_CFGA              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(4)
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_0_CFGB              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(5)
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_1_CFGA              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(6)
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_1_CFGB              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02(7)
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_2_CFGA              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_10(0)
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_2_CFGB              LW_PGC6_AON_SELWRE_SCRATCH_GROUP_10(1)
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_0_PRIV_LEVEL_MASK   LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_1_PRIV_LEVEL_MASK   LW_PGC6_AON_SELWRE_SCRATCH_GROUP_02_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_FBFALCON_SUB_WPR_ID_2_PRIV_LEVEL_MASK   LW_PGC6_AON_SELWRE_SCRATCH_GROUP_10_PRIV_LEVEL_MASK

#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_0_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_10(2)
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_0_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_10(3)
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_1_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_09(0)
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_1_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_09(1)
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_2_CFGA                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_09(2)
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_2_CFGB                LW_PGC6_AON_SELWRE_SCRATCH_GROUP_09(3)
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_0_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_10_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_1_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_09_PRIV_LEVEL_MASK
#define LW_PGC6_SELWRE_SCRATCH_LWDEC0_SUB_WPR_ID_2_PRIV_LEVEL_MASK     LW_PGC6_AON_SELWRE_SCRATCH_GROUP_09_PRIV_LEVEL_MASK

// Index in SCI delay table reserved for the necessary after RTD3 AUX rail switch in exit path
#define LW_PGC6_SCI_DELAY_TABLE_IDX_RTD3_AUX_RAIL_SWITCH    9

//
// Scratch bits used for SMBPBI preOS server
//
#if defined(LW_PGC6_SCI_LW_SCRATCH__SIZE_1)
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS                                 LW_PGC6_SCI_LW_SCRATCH(0)
#else
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS                                 LW_PGC6_SCI_LW_SCRATCH
#endif
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_TGP_LIMIT_INPUT                 9:0
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_TGP_LIMIT_INPUT_ILWALID         0x0
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_TGP_LIMIT_INPUT_CLEAR           0x3FF
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_TGP_LIMIT_PERSIST               10:10
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_TGP_LIMIT_PERSIST_NO            0x0
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_TGP_LIMIT_PERSIST_YES           0x1
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_TGP_LIMIT                       9:0
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_DEGRADED                        11:11
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_FORCED_DEGRADED_NO              0x0
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_FORCED_DEGRADED_YES             0x1
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_ECC                             13:12
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_ECC_ILWALID                     0x0
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_ECC_DISABLE                     0x2
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_ECC_ENABLE                      0x3
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_MIG                             15:14
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_MIG_ILWALID                     0x0
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_MIG_DISABLE                     0x2
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_MIG_ENABLE                      0x3
#define LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS_LWLINK_HPM_RESERVED             17:16

//
// Scratch bit used to indicate that ECC should be enabled regardless of the
// current inforom setting. The same bit is used by:
//  1) devinit to enable ECC
//  2) RM to force ECC config in inforom to be enabled during RM init. RM is
//     also expected to clear the bit once the setting is commited to inforom
//     so that subsequent GPU inits will default back to the inforom setting.
//
// This scheme allows the ECC setting to be set to enabled by just writing the
// scratch and resetting the GPU.
//
// Scratch register usage https://wiki.lwpu.com/engwiki/index.php/VBIOS/Data_Structures/Scratch_Registers#Others
// More details in bug 2517714.
//
// This is on none GC6 path, exclusive to LW_PGC6_SCI_LW_SCRATCH_GC6_I2C_SCRATCH
//
#define LW_PGC6_SCI_LW_SCRATCH_NONE_GC6_ECC_SCRATCH                         LW_PGC6_SCI_LW_SCRATCH(0)
#define LW_PGC6_SCI_LW_SCRATCH_NONE_GC6_ECC_SCRATCH_FORCE_ECC_ENABLED       24:24
#define LW_PGC6_SCI_LW_SCRATCH_NONE_GC6_ECC_SCRATCH_FORCE_ECC_ENABLED_FALSE 0
#define LW_PGC6_SCI_LW_SCRATCH_NONE_GC6_ECC_SCRATCH_FORCE_ECC_ENABLED_TRUE  1

// This is on GC6 path, mutually exclusive with LW_PGC6_SCI_LW_SCRATCH_SMBPBI_PREOS
#define LW_PGC6_SCI_LW_SCRATCH_GC6_I2C_SCRATCH                              LW_PGC6_SCI_LW_SCRATCH(3)
#define LW_PGC6_SCI_LW_SCRATCH_GC6_I2C_SCRATCH_VALUE                        31:0
#define LW_PGC6_SCI_LW_SCRATCH_GC6_I2C_SCRATCH_VALUE_INIT                   0

#endif // __ga102_dev_gc6_island_addendum_h__

