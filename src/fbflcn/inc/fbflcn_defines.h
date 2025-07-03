//sw/dev/gpu_drv/chips_a/uproc/fbflcn/inc/fbflcn_defines.h#22 - edit change 23871950 (text+x)
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_DEFINES_H
#define FBFLCN_DEFINES_H

/*!
 * @file   fbflcn_defines.h
 * @brief  Main declarations used throughout the fbfalcon code base. Controls debug output and
 *         assignements controlling resource usage such as mailbox registers
 *         Defines common funtion and intefaces to the falcon commands
 */

#include <lwmisc.h>
#include <lwstatus.h>

// Fixed assignements of MAiblox register use
//
//
// Mailbox(0): contains a copy of hw errors, should not be used for any other purpose
//
// Mailbox(1):  shows the reached mclk period once the pll has settled
// Mailbox(2):  fbflcn code revision: sw define
// Mailbox(3):  fbpa hw revision: read from LW_PFB_FBPA_FALCON_CTRL_VER
//

// debug output enabled by defines LOG_{UNIT}:
//
// Mailbox(5):  debug output, typically progress markers
// Mailbox(6):  debug output, typically results and processing values
// Mailbox(7):  debug output, lwrrently assigned to event logging
//
// Mailbox(11): used for priv logging when enabled
// Mailbox(12): used for priv logging when enabled
// Mailbox(13): used for priv logging when enabled

// Mailbox(14): used for PC on halt
// Mailbox(15): used for error/completion code
//

#define FBFLCN_ERROR_CODE_START_TEMP_TRACKING                    0xe001
#define FBFLCN_ERROR_CODE_HALT_COMMAND                           0xe002
#define FBFLCN_ERROR_CODE_REACHED_END_OF_MAIN                    0xe003
#define FBFLCN_ERROR_CODE_MCLKSWITCH_ILLEGAL_FREQUENCY           0xe004
#define FBFLCN_ERROR_CODE_MCLKSWTTCH_PLL_LOOP_OVERRUN            0xe005

#define FBFLCN_ERROR_CODE_SELWRITY_VIOLATION_AON_SCRATCH_GROUP3  0xe006
#define FBFLCN_ERROR_CODE_AON_SCRATCH_GROUP_UNSUCCESSFUL         0xe007

#define FBFLCN_ERROR_CODE_MIT_TABLE_SIZE_ERROR                   0xe008
#define FBFLCN_ERROR_CODE_MINI_DIRT_TABLE_SIZE_ERROR             0x0009
#define FBFLCN_ERROR_CODE_PLLINFOTABLE_TABLE_SIZE_ERROR          0xe00a
#define FBFLCN_ERROR_CODE_MEMORYCLOCKTABLE_TABLE_SIZE_ERROR      0xe00c
#define FBFLCN_ERROR_CODE_MEMTWEAKTABLE_TABLE_SIZE_ERROR         0xe00e

#define FBFLCN_ERROR_CODE_I1500_STRAP_READ_FAILED                0xe00f
#define FBFLCN_ERROR_CODE_STRAP_ACCESS_FAILED                    0xe010
#define FBFLCN_ERROR_CODE_HBM_PLL_INFO_NOT_FOUND                 0xe011
#define FBFLCN_ERROR_CODE_GDDR_REFMPLL_INFO_NOT_FOUND            0xe012
#define FBFLCN_ERROR_CODE_GDDR_MPLL_INFO_NOT_FOUND               0xe013
#define FBFLCN_ERROR_CODE_FW_IMAGE_IDENTIFIER_NOT_GFWI           0xe014
#define FBFLCN_ERROR_CODE_VDPA_POINTER_NOT_FOUND                 0xe015
#define FBFLCN_ERROR_CODE_VBIOS_TABLE_OVERFLOW                   0xe016
#define FBFLCN_ERROR_CODE_PMCT_ENTRY_NOT_FOUND                   0xe017
#define FBFLCN_ERROR_CODE_THERM_WR_SCRATCH_ACCESS_DENIED         0xe018
#define FBFLCN_ERROR_CODE_TEMPERATURE_READ_FAILED                0xe019
#define FBFLCN_ERROR_CODE_FBFLCN_HALT_REQUEST_EXELWTED           0xe01a
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_TABLE_SIZE_ERROR         0xe01c
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_TABLE_VERSION_ERROR      0xe01d
#define FBFLCN_ERROR_CODE_PERFORMANCE_TABLE_SIZE_ERROR           0xe01e
#define FBFLCN_ERROR_CODE_MISSING_NOMINAL_PSTATE_FREQUENCY       0xe01f
#define FBFLCN_ERROR_CODE_COMPUTATIONAL_RANGE_ERROR              0xe020
#define FBFLCN_ERROR_CODE_MIT_TABLE_ENTRY_LOOKUP_ERROR           0xe021
#define FBFLCN_ERROR_CODE_HEADER_REVISION_ERROR                  0xe022
#define FBFLCN_ERROR_CODE_VALIDATION_ERROR                       0xe023
#define FBFLCN_ERROR_CODE_PMU_ADDRESS_OUT_OF_RANGE               0xe024
#define FBFLCN_ERROR_CODE_PMU_ILLEGAL_APERTURE_DEFINITION        0xe025
#define FBFLCN_ERROR_CODE_ILLEGAL_MEMTWEAK_INDEX                 0xe026
#define FBFLCN_ERROR_CODE_INTERNAL_USE_ONLY_ERROR                0xe027
#define FBFLCN_ERROR_CODE_TABLE_SANITY_CHECK_FAILURE             0xe028

// Mem scrub tiemout is an indication that the fbfalcon has timed out
// waiting for the pafalcon to finish its internal memory scrubbing
#define FBFLCN_ERROR_CODE_MEM_SCRUB_TIMEOUT                      0xe029

#define FBFLCN_ERROR_CODE_QUEUE_DIRECT_QUEUE_ACCESS_VIOLATION    0xe02a
#define FBFLCN_ERROR_CODE_CORE_RUNNING_AT_INIT                   0xe02b
#define FBFLCN_ERROR_CODE_CORE_AUTH_EN_VIOLATION                 0xe02c
#define FBFLCN_ERROR_CODE_QUEUE_REQUEST_BEFORE_SYSMEM_ALLOCATION 0xe02d

#define FBFLCN_ERROR_CODE_TRAINING_DATA_EXCEEDS_MWPR1_HI         0xe030
#define FBFLCN_ERROR_CODE_TRAINING_HEADER_DATA_ERRORS            0xe031
#define FBFLCN_ERROR_CODE_TRAINING_DATA_ERRORS                   0xe032
#define FBFLCN_ERROR_CODE_TRAINING_INSUFFICIENT_ACCESS_SYS_SAVE  0xe033
#define FBFLCN_ERROR_CODE_TRAINING_INSUFFICIENT_ACCESS_SYS_LOAD  0xe034
#define FBFLCN_ERROR_CODE_TRAINING_SHA256_MISSMATCH              0xe035
#define FBFLCN_ERROR_CODE_TRAINING_DATA_EXCEEDS_SYSMEM_SIZE      0xe036
#define FBFLCN_ERROR_CODE_TRAINING_ILLEGAL_MEMORY_OFFSET         0xe037
#define FBFLCN_ERROR_CODE_TRAINING_BUFFER_NOT_MULTIPE_256B       0xe038
#define FBFLCN_ERROR_CODE_TRAINING_SELFTEST_FAILURE              0xe039

#define FBFLCN_ERROR_CODE_HW_WOSC_COUNT_EQ_0                     0xe03a
#define FBFLCN_ERROR_CODE_IEEE_WOSC_COUNT_ILWALID                0xe03b
#define FBFLCN_ERROR_CODE_PERIODIC_TIMEOUT_EXCEEDED              0xe03c
#define FBFLCN_ERROR_CODE_HYNIX_MIXED_ES1_AND_ES2                0xe03d

#define FBFLCN_ERROR_CODE_PARTITION_CNT_AND_MASK_MISSMATCH       0xe040

#define FBFLCN_ERROR_CODE_GPIO_TABLE_NOT_FOUND                   0xe058
#define FBFLCN_ERROR_CODE_GPIO_TABLE_HEADER_CHECK_FAILURE        0xe059
#define FBFLCN_ERROR_CODE_UNDEFINED_GPIO_VREF_ACCESS             0xe05a
#define FBFLCN_ERROR_CODE_UNDEFINED_GPIO_FBVDDQ_ACCESS           0xe05b
#define FBFLCN_ERROR_CODE_GPIO_COUNT_OVERFLOW                    0xe05c

#define FBFLCN_ERROR_FRTS_COMMAND_ACR_NOT_READY                  0xe060
#define FBFLCN_ERROR_CODE_CERT21_FRTS_DMA_UNEXPECTED_MEDIA_TYPE  0xe061
#define FBFLCN_ERROR_CODE_SAVE_AND_RESTORE_UNEXPECTED_MEDIA_TYPE 0xe065
#define FBFLCN_ERROR_CODE_SAVE_AND_RESTORE_MEMORY_ALLOCATION     0xe066

#define FBFLCN_ERROR_CODE_DMA_ERROR                              0xe06a
#define FBFLCN_ERROR_CODE_DMA_CHECK_DISABLED_ERROR               0xe06b

#define FBFLCN_ERROR_CODE_OK_TO_SWITCH_TIMEOUT_EXCEEDED          0xe070
#define FBFLCN_PROGRESS_CODE_WAITING_FOR_OK_TO_SWITCH            0xa070

#define FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT                 0xe080
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_VCO_ERROR           0xe081
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_M_VAL_ERROR         0xe082
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_N_VAL_ERROR         0xe083
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_P_VAL_ERROR         0xe084
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_VCO_ERROR          0xe085
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_M_VAL_ERROR        0xe086
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_N_VAL_ERROR        0xe087
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_P_VAL_ERROR        0xe088
// PA errors for ga10x
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_PTRIM_OPT_ERROR  0xe089
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_ERROR                   0xe08a
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_ILWALID_SYNC_FN            0xe08b
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_PRIV_ERROR       0xe08c
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_SIZE_ERROR       0xe08d

#define FBFLCN_ERROR_CODE_MCLK_SWITCH_TRAINING_ERROR                0xe090
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_NO_LOCK               0xe091
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_VAUX_DESGIN_PGM_ERROR         0xe092
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_VAUX_VBIOS_PGM_ERROR          0xe093
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_MUTEX_NOT_OBTAINED            0xe094
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_GPIO_MUTEX_NOT_RELEASED       0xe095
#define FBFLCN_ERROR_CODE_MCLK_SWITCH_ASR_ACPD_WITHOUT_ACPD         0xe096
#define FBFLCN_ERROR_CODE_PERIODIC_TRAINING_GPIO_MUTEX_NOT_RELEASED 0xe097
#define FBFLCN_ERROR_CODE_I1500_GPIO_MUTEX_NOT_RELEASED             0xe098
#define FBFLCN_ERROR_CODE_PRIV_READCHECK_ERROR                      0xe099

//Turing defines
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_ADDRESS_TRAINING_ERROR   0xe0a0
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WCK_TRAINING_ERROR       0xe0a1
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_TRAINING_ERROR        0xe0a2
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_TRAINING_ERROR        0xe0a3
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_VREF_TRACKING_ERROR      0xe0a4
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_CHAR_TRAINING_ERROR      0xe0a5
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_CHAR_SETTING_ERROR       0xe0a6

//Ampere defines
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_VREF_EYE_LOW_ERROR             0xe304
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_VREF_EYE_MID_ERROR             0xe305
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_VREF_EYE_HIGH_ERROR            0xe306
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_DFE_PI_ERROR                   0xe307
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_2STEP_PI_EYE_LOW_ERROR         0xe308
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_2STEP_PI_EYE_MID_ERROR         0xe309
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_2STEP_PI_EYE_HIGH_ERROR        0xe30a
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_FINAL_PI_ERROR                 0xe30b
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_VREF_EYE_LOW_ERROR          0xe30c
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_VREF_EYE_MID_ERROR          0xe30d
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_VREF_EYE_HIGH_ERROR         0xe30e
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_PI_ERROR                    0xe30f
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_2STEP_PI_EYE_LOW_ERROR      0xe310
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_2STEP_PI_EYE_MID_ERROR      0xe311
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_2STEP_PI_EYE_HIGH_ERROR     0xe312
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_FINAL_PI_ERROR              0xe313
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_VREF_EYE_LOW_ERROR         0xe314
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_VREF_EYE_MID_ERROR         0xe315
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_VREF_EYE_HIGH_ERROR        0xe316
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_PI_ERROR                   0xe317
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_2STEP_PI_EYE_LOW_ERROR     0xe318
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_2STEP_PI_EYE_MID_ERROR     0xe319
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_2STEP_PI_EYE_HIGH_ERROR    0xe31a
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_FINAL_PI_ERROR             0xe31b
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE                    0xe31c
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_ILWALID_EYE                    0xe31d
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_VREF_STEP1_ERROR               0xe31e
#define FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_VREF_STEP1_ERROR               0xe31f


// 0xe0b[0-f] can be used for security-related errors
#define FBFLCN_ERROR_CODE_STACK_CANARY_CORRUPTED                 0xe0b0
#define FBFLCN_ERROR_CODE_CODE_REVOCATION_DEVID_ILWALID          0xe0b1
#define FBFLCN_ERROR_CODE_CODE_REVOCATION_FUSEREV_MISSMATCH      0xe0b2
#define FBFLCN_ERROR_CODE_CODE_REVOCATION_NOT_IN_LS_MODE         0xe0b3
#define FBFLCN_ERROR_CODE_CODE_PRIV_READ_STATUS_ERROR            0xe0b4
#define FBFLCN_ERROR_CODE_CODE_PRIV_WRITE_STATUS_ERROR           0xe0b5
#define FBFLCN_ERROR_CODE_CODE_REVOCATION_BOOT_FUSEREV_MISSMATCH 0xe0b6
#define FBFLCN_ERROR_CODE_ACR_WITHOUT_LS_ERROR                   0xe0b7
#define FBFLCN_ERROR_CODE_PRIV_PLM_VIOLATION_ERROR               0xe0b8
#define FBFLCN_ERROR_CODE_CODE_REVOCATION_FPFFUSEREV_MISSMATCH   0xe0b9
#define FBFLCN_ERROR_CODE_UNEXPECTED_FALCON_ENGID                0xe0ba
#define FBFLCN_ERROR_CODE_PRIV_SEC_NOT_ENABLED                   0xe0bb
#define FBFLCN_ERROR_CODE_ILLEGAL_DEREFERENCE_ERROR              0xe0bc
#define FBFLCN_ERROR_CODE_PRIV_PGC6_AON_PLM_WRITE_VIOLATION_ERROR 0xe0bd
#define FBFLCN_ERROR_CODE_PRIV_PGC6_AON_PLM_READ_VIOLATION_ERROR  0xe0be

// BAR0 Priv engine error codes
#define FBFLCN_ERROR_CODE_PRIV_BAR0_ERROR                        0xe0c0

#define FBFLCN_ERROR_CODE_FBFLCN_HALT_UNPROCESSED_INTERRUPT      0xe0c5

// Interrupt handling
#define FBFLCN_ERROR_CODE_UNRESOLVED_INTERRUPT                   0xe0d1

#define FBFLCN_ERROR_CODE_TRAINING_TABLE_VDPA_ENTRY_MISSING                    0xe11c
#define FBFLCN_ERROR_CODE_MEM_TRAINING_TABLE_VDPA_ENTRY_MISSING                0xe11d
#define FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_TABLE_VDPA_ENTRY_MISSING         0xe11e
#define FBFLCN_ERROR_CODE_MEM_TRAINING_TABLE_RD_WR_EDC_DBI_ID_MISSING          0xe11f
#define FBFLCN_ERROR_CODE_MEM_TRAINING_TABLE_RD_WR_EDC_DBI_STRAP_MISSING       0xe120
#define FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_TABLE_ENTRY_MISSING              0xe122
#define FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_LOAD_PTR_MISSING                 0xe123
#define FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_LOAD_SEC_PTR_MISSING             0xe124
#define FBFLCN_ERROR_CODE_MEM_TRAINING_PATRAM_RD_DATA_EDC_DBI_COUNT_MISMATCH   0xe115

#define FBFLCN_ERROR_CODE_PAFALCON_BINARY_EXCEEDS_EXPECTED_SIZE                0xe200
#define FBFLCN_ERROR_CODE_PAFALCON_BINARY_ALIGNMENT_ERROR                      0xe201
#define FBFLCN_ERROR_CODE_PAFALCON_BINARY_HEADER_ERROR                         0xe202
#define FBFLCN_ERROR_CODE_PAFALCON_BINARY_DOWNLOAD_ERROR                       0xe203
#define FBFLCN_ERROR_CODE_PAFALCON_DESC_ERROR                                  0xe204
#define FBFLCN_ERROR_CODE_PAFALCON_START_ERROR                                 0xe205
#define FBFLCN_ERROR_CODE_PAFALCON_BINARY_DMEM_OVERFLOW_ERROR                  0xe206

#define FBFLCN_ERROR_CODE_SEGMENT_REQUEST_ERROR                                0xe210
#define FBFLCN_ERROR_CODE_SEGMENT_IDENTIFIER_ERROR                             0xe211


#define FBFLCN_ERROR_CODE_0FB_SETUP_ERROR                                      0xe301
#define FBFLCN_ERROR_CODE_PSTATE_DISABLED_ERROR                                0xe302
// Init status error asserts before an mclk request should there be inconsistencies from the table loading,
// such as missing elements, disabled p-states or missing frts all together
// Init status error includes the init_status in the upper 16 bit of the dword.
#define FBFLCN_ERROR_CODE_INIT_STATUS_ERROR                                    0xe303

//
// Manual Extensions
//

#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH        0:0
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_READY  0x1
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT   0x0

#define FALCON_MEM_BLOCK_SIZE 0x100
//
// Boot training & data loading related defines
//

// settle time for training data restoration to priv    (bug 200470194)
#define INITIAL_TRAINING_RESTORE_DELAY_US 25

//
// Priv Control
//

// Max number of priv retries if feature REGISTER_ACCESS_RETRY is enabled
#define PRIV_MAX_RETRY_ON_READ  32
#define PRIV_MAX_RETRY_ON_WRITE 32



// define different ctx targets for each fbif operation

// bootloader and segment loader share context0 for access to binary
#define CTX_DMA_FBFALCON_READ_FROM_MWPR_0 0x0

// mwpr1 used turing Turing to save training settings to dmem mwpr
#define CTX_DMA_FBFALCON_WRITE_TO_MWPR_1  0x1
#define CTX_DMA_FBFALCON_READ_FROM_MWPR_1 0x2

// context to read from vbios wpr
#define CTX_DMA_FBFALCON_READ_VBIOSTABLES 0x3

// context for system memory buffer for debug and training data
// starting with Amere
#define CTX_DMA_FBFALCON_FOR_SYSMEM_STORAGE 0x4


// context shift masks for rd/wr targets
#define CTX_DMAIDX_SHIFT_IMREAD            (0x00)
#define CTX_DMAIDX_SHIFT_DMREAD            (0x08)
#define CTX_DMAIDX_SHIFT_DMWRITE           (0x0c)

// FBFALCON manual extensions
#define LW_FBFALCON_LOGGING_MASK_TABLE                          0x00000001
#define LW_FBFALCON_LOGGING_MASK_DATA                           0x00000002
#define LW_FBFALCON_LOGGING_LEVEL_CHECK                         0x00000001
#define LW_FBFALCON_LOGGING_LEVEL_STEP_IN_OUT                   0x00000002

//Vbios Magic number for avoiding dq/dqs termination  issue
#define VBIOS_P4_CL_FOR_DQ_DQS_TERMINATION 24483331

#define VBIOS_P4_CL_FOR_FORCE_SSC_P58_BUG_3196573   29385800



//#define LOG_MAIN
#ifdef LOG_MAIN
#define LOG_MAIN_INDEX 0x2
#define LOG_MAIN_MARK(d) (((LOG_MAIN_INDEX & 0xff) << 24) | ((d) & 0x00ffff))
#endif  // LOG_MAIN

//#define LOG_HELPER
#ifdef LOG_HELPER
#define LOG_HELPER_INDEX 0x3
#define LOG_HELPER_MARK(d) (((LOG_HELPER_INDEX & 0xff) << 24) | ((d) & 0x00ffff))
#define LOG_HELPER_EVENT_MAILBOX_INDEX 7
#endif  // LOG_HELPER

//#define LOG_MCLKSWITCH
//#define MONITOR_MCLKSWITCH
//#define LOG_MCLKSWITCH_MAILBOX_INDEX_MCLK_REACHED 1

//#define LOG_IEEE1500
#ifdef LOG_IEEE1500
#define LOG_IEEE1500_INDEX 0x4
#define LOG_IEEE1500_MARK(d) (((LOG_IEEE1500_INDEX & 0xff) << 24) | ((d) & 0x00ffff))
#endif // LOG_IEEE1500
#define STATS_IEEE1500_REPORT_SWITCH_MAILBOX      LW_CFBFALCON_FIRMWARE_MAILBOX(1)
#define STATS_IEEE1500_REPORT_UP_SWITCH_MAILBOX   LW_CFBFALCON_FIRMWARE_MAILBOX(2)
#define STATS_IEEE1500_REPORT_DOWN_SWITCH_MAILBOX LW_CFBFALCON_FIRMWARE_MAILBOX(3)

#define VBIOS_TABLE_SUPPORT
#define TABLE_LOG_DEBUG
#define DEBUG 1


// Compiler side assert
#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate,__LINE__,file)

#define _impl_PASTE(a,b) a##b
#define _impl_CASSERT_LINE(predicate, line, file) \
    typedef LwU8 _impl_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];

#ifndef NULL
#define NULL 0
#endif

#endif  // FBFLCN_DEFINES_H
