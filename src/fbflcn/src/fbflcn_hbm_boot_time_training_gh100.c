#include <falcon-intrinsics.h>

#include "fbflcn_hbm_boot_time_training_gh100.h"
#include "fbflcn_hbm_periodic_training.h"
#include "fbflcn_defines.h"
#include "fbflcn_hbm_shared_gh100.h"
#include "fbflcn_helpers.h"

#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
#include <falc_debug.h>
#include <falc_trace.h>
#include "fbflcn_gddr_mclk_switch.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "memory.h"
#include "data.h"
#include "priv.h"

#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"
#include "dev_fuse.h"
#include "dev_trim.h"
#include "dev_pri_ringstation_fbp.h"

#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
extern OBJFBFLCN Fbflcn;
#include "config/g_memory_private.h"
//#include "memory_gddr.h"      // conflict with REGBOX, but has gddr_wait_for_training_done
#include "fbflcn_hbm_mclk_switch.h"
#include <string.h>
#else
void *Fbflcn = NULL;
#include "fbflcn_tb_only.h"    // gddr_wait_for_training_done
#endif
#include "dev_xtl_ep_pri.h"

//TODO combine from REG_BOX in fbflcn_hbm_mclk_switch.h? -MW
typedef struct REGBOX {
  LwU32 pfb_fbpa_fbio_broadcast;
  LwU32 pfb_fbpa_cfg0;
  LwU32 pfb_fbpa_config3;
  LwU32 pfb_fbpa_dram_asr;
  LwU32 pfb_fbpa_training_cmd;
  LwU32 pfb_fbpa_fbio_hbm_delay_broadcast_misc0;
  LwU32 pfb_fbpa_fbio_hbm_delay_broadcast_misc1;
  LwU32 pfb_fbpa_fbio_hbm_ddllcal_ctrl1;
  LwU32 pfb_fbpa_hbm_char;
  LwU32 pfb_fbpa_training_char_ctrl;
  LwU32 pfb_fbpa_timing21;
  LwU32 pfb_fbio_hbm_cfg0;
  LwU32 pfb_fbio_hbm_cfg13;
  LwU32 pfb_fbio_hbm_trng_misc;
  LwU32 pfb_fbpa_fbio_hbm_wdqs_cfg;
} REGBOX;

REGBOX  lwr_reg;
REGBOX* saved_reg = &lwr_reg;

//Flags and VBIOS settings
//setting up the tables, and parsing out the information
LwBool bFlagDbiEnRd, bFlagDbiEnWr, bFlagEccEn;      // comes from registers
LwBool bFlagRdTrEn, bFlagWrTrEn, bFlagWrLvlEn, bFlagDcaEn;
LwBool bFlagAteNoFail = LW_FALSE;
LwBool bFlagEnablePeriodicTr = LW_FALSE; // if periodic training interval == 0
LwBool bFlagSkipRdqsDelayAdj = LW_FALSE;

// for simulation only, prevents reloading of rd patram on 2nd boot training call
LwBool bFlagRdLoadPatram = LW_TRUE;

//Delay settings
LwU16  wdqs_initial_delay = 0; //Global so we can do mclk switch without breaking the sweep range
LwU8   rd_rdqs_delay_min, rd_rdqs_delay_max;
LwU8   rd_dq_delay_min, rd_dq_delay_max;
LwU16  wr_delay_min, wr_delay_max;
LwU8   rd_rdqs_delay_step;
LwU8   rd_dq_delay_step;
LwU8   wr_delay_step;
LwS16  wr_lvl_delay_min;
LwU8   wr_lvl_delay_max;
LwU8   wr_lvl_delay_step;
LwU8   wr_lvl_derr_samples;
LwU32  wr_derr_rd_delay;
LwU8   min_eye_width;

//Boot Training Config
LwU32  gbl_hbm_mode = LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM3;
LwU32  hbm2_flip_strap_per_pa = 0; //One bit per PA
LwU32  hbm3_flip_strap_per_pa = 0; //One bit per PA

//Arrays to store per-pin information for current and best eyes
//Since we need to track multiple eyes, per_pin_lwrrent_eyes stores information for the current eye
//per_pin_best_eyes stores the best eye for each pin and is populated once an entire eye has been detected
//Data is packed as follows (Note: There are defines for each field)
//[15:12] - BS Setting
//[11:6]  - Trimmer (PI) setting
//[5:0]   - Window Size
typedef struct __attribute__((__packed__)) PER_PIN_EYE_INFO {
    LwU8   width:    6;   // [5:0]
    LwU16  delay:    10;  // [15:6] -- [15:12] is BS, [11:6] is Trimmer
} PER_PIN_EYE_INFO;

PER_PIN_EYE_INFO lwrrent_eye_per_dq[TOTAL_DQ_BITS];
PER_PIN_EYE_INFO best_eye_per_dq[TOTAL_DQ_BITS];
PER_PIN_EYE_INFO lwrrent_eye_per_dbi[TOTAL_DBI_BITS];
PER_PIN_EYE_INFO best_eye_per_dbi[TOTAL_DBI_BITS];
PER_PIN_EYE_INFO lwrrent_eye_per_ecc[TOTAL_ECC_BITS];
PER_PIN_EYE_INFO best_eye_per_ecc[TOTAL_ECC_BITS];

#define NUM_PATRAM_ENTRIES      32
#define PATRAM_BURSTS_PER_ENTRY 8
#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
extern LwU32 CharPatramDQ_rd[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];
extern LwU8  CharPatramDBI_rd[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];
extern LwU8  CharPatramECC_rd[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];
extern LwU32 CharPatramDQ_wr[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];
extern LwU8  CharPatramDBI_wr[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];
extern LwU8  CharPatramECC_wr[NUM_PATRAM_ENTRIES*PATRAM_BURSTS_PER_ENTRY];
#endif

//Arrays for handling RDQS training
typedef struct __attribute__((__packed__)) RDQS_DELAY_TRACKING {
    LwU8   first_passing_delay:  8; //[7:0]  - First delay setting where all pins fail
    LwU8   passing_setting_seen: 1; //[8]    - Flag to indicate at least one pass was seen
    LwU8   reserved:             7; //[15:9] - Reserved
} RDQS_DELAY_TRACKING;
RDQS_DELAY_TRACKING first_pass_per_rdqs[TOTAL_DQS_BITS];

//Arrays for handling write leveling
#define DCA_NEG_7_SETTING  7
#define DCA_ZERO_SETTING   0
#define DCA_PLUS_1_SETTING 9
#define DCA_PLUS_7_SETTING 15
#define DCA_T_DCMM         1000 //1us
typedef struct __attribute__((__packed__)) DCA_TRAINING_INFO {
    LwU8   best_dca_setting:  4; //[3:0] - The DCA setting for this WDQS where we saw a 1<->0 transition
    LwU8   prev_dcm_result:   1; //[4]   - The previous DCM result, to detect a 1<->0 transition
    LwU8   training_complete: 1; //[5]   - Indicates we saw a 1<->0 transition so we shouldn't move DCA
    LwU8   reserved:          2; //[7:6] - Unused
} DCA_TRAINING_INFO;
DCA_TRAINING_INFO dca_setting_per_wdqs[TOTAL_DQS_BITS];

//prev_early_late_per_wdqs serves several purposes, all related to tracking early/late. See struct
typedef struct __attribute__((__packed__)) WDQS_EARLY_LATE_INFO {
    LwU8   wdqs_is_early:      1;  //[0] - Previous delay value of wdqs_is_early
    LwU8   first_setting_late: 1;  //[1] - Was the first setting "late"
    LwU8   early_late_seen:    1;  //[2] - Have we seen the early->late transition
    LwU8   derr_samples:       1;  //[3] - OR of all previous samples of DERR at this setting
    LwU8   reserved:           4;  //[7:4] - Reserved bits
} WDQS_EARLY_LATE_INFO;
WDQS_EARLY_LATE_INFO prev_early_late_per_wdqs[TOTAL_DQS_BITS];
LwU16 early_late_transition_per_wdqs[TOTAL_DQS_BITS];

FLCN_TIMESTAMP startTime = { 0 };
LwU16 debug_mbox_idx = 2;

void ppei_andOp(PER_PIN_EYE_INFO* const p, const LwU16 ref)
{
    p->width = ref & 0x3F;
    p->delay = (ref >> 6) & 0x3FF;
}
LwBool ppei_isNotEqual(const PER_PIN_EYE_INFO p, const LwU16 v) {
    return ((p.width != v) || (p.delay != v));
}

void rdt_andOp(RDQS_DELAY_TRACKING* const r, const LwU16 ref)
{
    r->first_passing_delay  = ref       & 0xFF;
    r->passing_setting_seen = (ref >> 8) & 0x1;
}
LwBool rdt_isNotEqual(const RDQS_DELAY_TRACKING r, const LwU16 v) {
    return ((r.first_passing_delay != v) ||
            (r.passing_setting_seen != v));
}

void weli_andOp(WDQS_EARLY_LATE_INFO* const w, const LwU16 ref)
{
    w->wdqs_is_early      = ref        & 0x1;
    w->first_setting_late = (ref >> 1) & 0x1;
    w->early_late_seen    = (ref >> 2) & 0x1;
    w->reserved           = 0;
}
LwBool weli_isNotEqual(const WDQS_EARLY_LATE_INFO w, const LwU16 v) {
    return ((w.wdqs_is_early != v) ||
            (w.first_setting_late != v) ||
            (w.early_late_seen != v) ||
            (w.reserved != v));
}

#define HBM_TRAINING_WDQS_MIN_DELAY 0
#define HBM_TRAINING_WDQS_MAX_DELAY ((4*64) - 1)


//Defines for constants to make coding cleaner and configurable
#define HBM_CHAR_PATRAM_ENTRIES            32
#define HBM_CHAR_PATRAM_ENTRY_DQ_BITS      256
#define HBM_CHAR_PATRAM_ENTRY_DBI_ECC_BITS 32
#define HBM_CHAR_PATRAM_ENTRY_WIDTH        (HBM_CHAR_PATRAM_ENTRY_DQ_BITS + (2*HBM_CHAR_PATRAM_ENTRY_DBI_ECC_BITS))
#define HBM_CHAR_PATRAM_LOOP_CNT           ((HBM_CHAR_PATRAM_ENTRY_DQ_BITS/32) * HBM_CHAR_PATRAM_ENTRIES)

#define TRAINING_DIR_READ      0
#define TRAINING_DIR_WRITE     1
#define TRAINING_DIR_BOTH      2
#define TRAINING_DIR_READ_RDQS 3

//=================================================================================================
// Tracking variables for ATE
//=================================================================================================
//The DFT team runs with bFlagAteNoFail = LW_TRUE so training will always run to completion
//After it completes, they need a way to read back status to know which training steps failed
//The variables below track per-dword status for each training step to help narrow down what failed
GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "wdqs_pass_per_dword")
LwBool wdqs_pass_per_dword[(MAX_PARTS*SUBPS*CHANNELS_IN_SUBP*DWORDS_IN_CHANNEL)];
GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "rd_dq_pass_per_dword")
LwBool rd_dq_pass_per_dword[(MAX_PARTS*SUBPS*CHANNELS_IN_SUBP*DWORDS_IN_CHANNEL)];
GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "wr_dq_pass_per_dword")
LwBool wr_dq_pass_per_dword[(MAX_PARTS*SUBPS*CHANNELS_IN_SUBP*DWORDS_IN_CHANNEL)];
GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "wr_dbi_pass_per_dword")
LwBool wr_dbi_pass_per_dword[(MAX_PARTS*SUBPS*CHANNELS_IN_SUBP*DWORDS_IN_CHANNEL)];

GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "wdqs_training_pass_per_pa")
LwU32  wdqs_training_pass_per_pa = 0xFFFFFFFF;
GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "rd_dq_training_pass_per_pa")
LwU32  rd_dq_training_pass_per_pa = 0xFFFFFFFF;
GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "wr_dq_training_pass_per_pa")
LwU32  wr_dq_training_pass_per_pa = 0xFFFFFFFF;
GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("trainingData", "wr_dbi_training_pass_per_pa")
LwU32  wr_dbi_training_pass_per_pa = 0xFFFFFFFF;


void initialize_status_arrays(void)
{
    NIO_PRINT("Entering initialize_status_arrays");
    memset(wdqs_pass_per_dword, 1, sizeof(wdqs_pass_per_dword));
    memset(rd_dq_pass_per_dword, 1, sizeof(rd_dq_pass_per_dword));
    memset(wr_dq_pass_per_dword, 1, sizeof(wr_dq_pass_per_dword));
    memset(wr_dbi_pass_per_dword, 1, sizeof(wr_dbi_pass_per_dword));
}

//=================================================================================================
// Falcon Breakpoint Implementation for Silicon Debug
//=================================================================================================
//Boot training silicon debug
//Full documentation available in P4 @ //hw/doc/gpu/hopper/hopper/verif/FB/GH100_Boot_Training_Breakpoints.docx
//Uncomment BOOT_TR_DEBUG_ENABLE to enable.
//Values for BOOT_TR_DEBUG_ENABLE_* are possible breakpoints as defined below.
//Set the bits corresponding to the breakpoints you would like to enable in the proper variable
//  BIT   RD|WR   TRAINING PART
//   0    RD      GPU_INIT_VREF_SWEEP
//   1    RD      GPU_INIT_VREF
//   2    RD      PI_OFFSET_SWEEP
//   3    RD      PI_OFFSET_AREA_VREF
//   4    RD      PI_OFFSET_AVG_AREA_VREF_DFE
//   5    RD      FINAL_AREA_VREF_DFE
//
//   6    WR_DQ   VREF_INIT_DRAM_SWEEP_DQ
//   7    WR_DQ   DRAM_INIT_VREF
//   2    WR_DQ   PI_OFFSET_SWEEP
//   3    WR_DQ   PI_OFFSET_AREA_VREF
//   4    WR_DQ   PI_OFFSET_AVG_AREA_VREF_DFE
//   5    WR_DQ   FINAL_AREA_VREF_DFE
//
//   6    WR_MTA  VREF_INIT_DRAM_SWEEP_DQ
//   7    WR_MTA  DRAM_INIT_VREF
//   2    WR_MTA  PI_OFFSET_SWEEP
//   3    WR_MTA  PI_OFFSET_AREA_VREF
//   4    WR_MTA  PI_OFFSET_AVG_AREA_VREF_DFE
//   5    WR_MTA  FINAL_AREA_VREF_DFE
#define BOOT_TR_DEBUG_ENABLE        1

#ifdef BOOT_TR_DEBUG_ENABLE
LwBool bFlagBootTrDebugEnDca    = LW_FALSE;
LwBool bFlagBootTrDebugEnWdqs   = LW_FALSE;
LwBool bFlagBootTrDebugEnRdRdqs = LW_FALSE;
LwBool bFlagBootTrDebugEnRdDq   = LW_FALSE;
LwBool bFlagBootTrDebugEnWrDq   = LW_FALSE;
LwBool bFlagBootTrDebugEnWrDbi  = LW_FALSE;
LwBool bFlagBootTrDebugEnMisc   = LW_FALSE;
#endif

//Commands issued in FALCON_MONITOR
#define BOOT_TR_DEBUG_CMD_DCA     0
#define BOOT_TR_DEBUG_CMD_WDQS    1
#define BOOT_TR_DEBUG_CMD_RD_RDQS 2
#define BOOT_TR_DEBUG_CMD_RD_DQ   3
#define BOOT_TR_DEBUG_CMD_WR_DQ   4
#define BOOT_TR_DEBUG_CMD_WR_DBI  5
#define BOOT_TR_DEBUG_CMD_MISC    8
#define BOOT_TR_DEBUG_CMD_INIT_SWEEP                     0
#define BOOT_TR_DEBUG_CMD_READ_STATUS                    1
#define BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_START            2
#define BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_COMPLETE         3
#define BOOT_TR_DEBUG_CMD_MISC_RD_TR_PATRAM_START        8
#define BOOT_TR_DEBUG_CMD_MISC_PRE_MCLK_SWITCH           9
#define BOOT_TR_DEBUG_CMD_MISC_MCLK_SWITCH_COMPLETE      10
#define BOOT_TR_DEBUG_CMD_MISC_TRAINING_COMPLETE         11
#define BOOT_TR_DEBUG_CMD_MISC_RD_RDQS_TRAINING_COMPLETE 0
#define BOOT_TR_DEBUG_CMD_MISC_RD_DQ_TRAINING_COMPLETE   1
#define BOOT_TR_DEBUG_CMD_MISC_WR_DQ_TRAINING_COMPLETE   2
#define BOOT_TR_DEBUG_CMD_MISC_WR_DBI_TRAINING_COMPLETE  3
#define BOOT_TR_DEBUG_CMD_FALCON_CONTINUE               0xF

//Commands to read internal arrays
#define BOOT_TR_DEBUG_ARRAY_LWRRENT_EYE_PER_DQ             0
#define BOOT_TR_DEBUG_ARRAY_BEST_EYE_PER_DQ                1
#define BOOT_TR_DEBUG_ARRAY_LWRRENT_EYE_PER_DBI            2
#define BOOT_TR_DEBUG_ARRAY_BEST_EYE_PER_DBI               3
#define BOOT_TR_DEBUG_ARRAY_LWRRENT_EYE_PER_ECC            4
#define BOOT_TR_DEBUG_ARRAY_BEST_EYE_PER_ECC               5
#define BOOT_TR_DEBUG_ARRAY_FIRST_PASS_PER_RDQS            6
#define BOOT_TR_DEBUG_ARRAY_DCA_SETTING_PER_WDQS           7
#define BOOT_TR_DEBUG_ARRAY_PREV_EARLY_LATE_PER_WDQS       8
#define BOOT_TR_DEBUG_ARRAY_EARLY_LATE_TRANSITION_PER_WDQS 9

//Bit positions FALCON_MONITOR
#define BOOT_TR_DEBUG_LWMT_REQ         31
#define BOOT_TR_DEBUG_FALCON_REQ       30
#define BOOT_TR_DEBUG_READ_ARRAY       29
#define BOOT_TR_DEBUG_STEP_LSB          4
#define BOOT_TR_DEBUG_COMMAND_LSB       0
#define BOOT_TR_DEBUG_ARR_INDEX0_LSB    8
#define BOOT_TR_DEBUG_ARR_TARGET_LSB    0
#define BOOT_TR_DEBUG_LWRR_DELAY_LSB    8

//Masks for FALCON_MONITOR -- Note: These should be applied AFTER shifting by LSB
#define BOOT_TR_DEBUG_STEP_MASK         0xF
#define BOOT_TR_DEBUG_COMMAND_MASK      0xF
#define BOOT_TR_DEBUG_ARR_INDEX_MASK    0xFFFF
#define BOOT_TR_DEBUG_ARR_TARGET_MASK   0x3F

//Boot Training Debug Functions
#ifdef BOOT_TR_DEBUG_ENABLE
LwU32 send_data_eye_arr(
    PER_PIN_EYE_INFO *data_eye_arr,
    LwU16  max_index,
    LwU32  read_info
)
{
    LwU16 lwrrent_bit_idx = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    for (; lwrrent_bit_idx < max_index; lwrrent_bit_idx++) {
        LwU16 eye_data = (data_eye_arr[lwrrent_bit_idx].delay << 6) | data_eye_arr[lwrrent_bit_idx].width;
        mailbox_wr_data |= (eye_data << (16*(num_data_written % 2)));

        num_data_written++;
        //For every 2 pieces of data written, need to actually write the mailbox
        if ((num_data_written % 2) == 0) {
            LwU8 mailbox_idx = ((num_data_written/2) - 1);
            FW_MBOX_WR32(mailbox_idx, mailbox_wr_data);
            if (mailbox_idx == 0) {
                mailbox0_data = mailbox_wr_data;
            }
            mailbox_wr_data = 0; //Reset mailbox write data for next loop
        }

        //If the maximum amount of data was written, return
        if (num_data_written >= 32) {
            return mailbox0_data;
        }
    }
    return mailbox0_data;
}

LwU32 send_first_pass_per_rdqs(
    LwU32 read_info
)
{
    LwU16 lwrrent_bit_idx = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    for (; lwrrent_bit_idx < TOTAL_DQS_BITS; lwrrent_bit_idx++) {
        LwU16 eye_data = (first_pass_per_rdqs[lwrrent_bit_idx].passing_setting_seen << 8) |
                         first_pass_per_rdqs[lwrrent_bit_idx].first_passing_delay;
        mailbox_wr_data |= (eye_data << (16*(num_data_written % 2)));

        num_data_written++;
        //For every 2 pieces of data written, need to actually write the mailbox
        if ((num_data_written % 2) == 0) {
            LwU8 mailbox_idx = ((num_data_written/2) - 1);
            FW_MBOX_WR32(mailbox_idx, mailbox_wr_data);
            if (mailbox_idx == 0) {
                mailbox0_data = mailbox_wr_data;
            }
            mailbox_wr_data = 0; //Reset mailbox write data for next loop
        }

        //If the maximum amount of data was written, return
        if (num_data_written >= 32) {
            return mailbox0_data;
        }
    }
    return mailbox0_data;
}

LwU32 send_dca_setting_per_wdqs(
    LwU32 read_info
)
{
    LwU16 lwrrent_bit_idx = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    for (; lwrrent_bit_idx < TOTAL_DQS_BITS; lwrrent_bit_idx++) {
        LwU8 dca_data = (dca_setting_per_wdqs[lwrrent_bit_idx].training_complete << 5) |
                        (dca_setting_per_wdqs[lwrrent_bit_idx].prev_dcm_result   << 4) |
                        dca_setting_per_wdqs[lwrrent_bit_idx].best_dca_setting;
        mailbox_wr_data |= (dca_data << (8*(num_data_written % 4)));

        num_data_written++;
        //For every 4 pieces of data written, need to actually write the mailbox
        if ((num_data_written % 4) == 0) {
            LwU8 mailbox_idx = ((num_data_written/4) - 1);
            FW_MBOX_WR32(mailbox_idx, mailbox_wr_data);
            if (mailbox_idx == 0) {
                mailbox0_data = mailbox_wr_data;
            }
            mailbox_wr_data = 0; //Reset mailbox write data for next loop
        }

        //If the maximum amount of data was written, return
        if (num_data_written >= 64) {
            return mailbox0_data;
        }
    }

    return mailbox0_data;
}

LwU32 send_prev_early_late_per_wdqs(
    LwU32 read_info
)
{
    LwU16 lwrrent_bit_idx = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    for (; lwrrent_bit_idx < TOTAL_DQS_BITS; lwrrent_bit_idx++) {
        LwU8 early_late_info = (prev_early_late_per_wdqs[lwrrent_bit_idx].derr_samples       << 3) |
                               (prev_early_late_per_wdqs[lwrrent_bit_idx].early_late_seen    << 2) |
                               (prev_early_late_per_wdqs[lwrrent_bit_idx].first_setting_late << 1) |
                               prev_early_late_per_wdqs[lwrrent_bit_idx].wdqs_is_early;
        mailbox_wr_data |= (early_late_info << (8*(num_data_written % 4)));

        num_data_written++;
        //For every 4 pieces of data written, need to actually write the mailbox
        if ((num_data_written % 4) == 0) {
            LwU8 mailbox_idx = ((num_data_written/4) - 1);
            FW_MBOX_WR32(mailbox_idx, mailbox_wr_data);
            if (mailbox_idx == 0) {
                mailbox0_data = mailbox_wr_data;
            }
            mailbox_wr_data = 0; //Reset mailbox write data for next loop
        }

        //If the maximum amount of data was written, return
        if (num_data_written >= 64) {
            return mailbox0_data;
        }
    }

    return mailbox0_data;
}

LwU32 send_early_late_transition_per_wdqs(
    LwU32 read_info
)
{
    LwU16 lwrrent_bit_idx = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    for (; lwrrent_bit_idx < TOTAL_DQS_BITS; lwrrent_bit_idx++) {
        LwU16 early_late_transition = early_late_transition_per_wdqs[lwrrent_bit_idx];
        mailbox_wr_data |= (early_late_transition << (16*(num_data_written % 2)));

        num_data_written++;
        //For every 2 pieces of data written, need to actually write the mailbox
        if ((num_data_written % 2) == 0) {
            LwU8 mailbox_idx = ((num_data_written/2) - 1);
            FW_MBOX_WR32(mailbox_idx, mailbox_wr_data);
            if (mailbox_idx == 0) {
                mailbox0_data = mailbox_wr_data;
            }
            mailbox_wr_data = 0; //Reset mailbox write data for next loop
        }

        //If the maximum amount of data was written, return
        if (num_data_written >= 32) {
            return mailbox0_data;
        }
    }
    return mailbox0_data;
}

void falcon_lwmt_monitor_cmd (
    void
)
{
    while (1) {
        OS_PTIMER_SPIN_WAIT_US(500);
        LwU32 falcon_mon_data = REG_RD32(BAR0, LW_PFB_FBPA_FALCON_MONITOR);
        LwU8 falcon_mon_cmd = falcon_mon_data & BOOT_TR_DEBUG_COMMAND_MASK;
        LwBool lwmt_req     = (falcon_mon_data >> BOOT_TR_DEBUG_LWMT_REQ) & 0x1;

        //FALCON_CONTINUE means break out of this loop and return to normal operation
        if (lwmt_req && (falcon_mon_cmd == BOOT_TR_DEBUG_CMD_FALCON_CONTINUE)) {
            return;
        }

        //The remaining possible commands must have READ_ARRAY set to be valid
        LwBool read_array = (falcon_mon_data >> BOOT_TR_DEBUG_READ_ARRAY) & 0x1;
        if (lwmt_req && read_array) {
            LwU8  arr_sel  = (falcon_mon_data >> BOOT_TR_DEBUG_ARR_TARGET_LSB) & BOOT_TR_DEBUG_ARR_TARGET_MASK;
            LwU32 mailbox0_data = 0;

            switch (arr_sel) {
                case BOOT_TR_DEBUG_ARRAY_LWRRENT_EYE_PER_DQ:
                    mailbox0_data = send_data_eye_arr(lwrrent_eye_per_dq, TOTAL_DQ_BITS, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_BEST_EYE_PER_DQ:
                    mailbox0_data = send_data_eye_arr(best_eye_per_dq, TOTAL_DQ_BITS, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_LWRRENT_EYE_PER_DBI:
                    mailbox0_data = send_data_eye_arr(lwrrent_eye_per_dbi, TOTAL_DBI_BITS, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_BEST_EYE_PER_DBI:
                    mailbox0_data = send_data_eye_arr(best_eye_per_dbi, TOTAL_DBI_BITS, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_LWRRENT_EYE_PER_ECC:
                    mailbox0_data = send_data_eye_arr(lwrrent_eye_per_ecc, TOTAL_ECC_BITS, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_BEST_EYE_PER_ECC:
                    mailbox0_data = send_data_eye_arr(best_eye_per_ecc, TOTAL_ECC_BITS, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_FIRST_PASS_PER_RDQS:
                    mailbox0_data = send_first_pass_per_rdqs(falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_DCA_SETTING_PER_WDQS:
                    mailbox0_data = send_dca_setting_per_wdqs(falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_PREV_EARLY_LATE_PER_WDQS:
                    mailbox0_data = send_prev_early_late_per_wdqs(falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_EARLY_LATE_TRANSITION_PER_WDQS:
                    mailbox0_data = send_early_late_transition_per_wdqs(falcon_mon_data);
                    break;
            }

            //Zero out the upper half of mailbox0_data
            //MAILBOX0 lower bits are used for signaling so won't return valid data so we can return that
            //data in falcon_monitor instead
            mailbox0_data &= 0x0000FFFF;

            //Clear all bits and set FALCON_REQ and READ_ARRAY
            REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, (0x60000000 | mailbox0_data));
        }
    }
}

//Write current breakpoint location to FALCON_MONITOR and populate the INFO field
//appropriately for the current training step
void falcon_boot_tr_breakpoint (
        LwU8  training_major_step,
        LwU8  training_minor_step,
        LwU16 lwrrent_delay
)
{
    //Determine if this breakpoint is enabled. If not, exit now
    //Each class of breakpoints is controlled by a VBIOS flag
    LwBool breakpointEn = LW_FALSE;
    switch (training_major_step) {
        case BOOT_TR_DEBUG_CMD_DCA:
            breakpointEn = bFlagBootTrDebugEnDca;
            break;
        case BOOT_TR_DEBUG_CMD_WDQS:
            breakpointEn = bFlagBootTrDebugEnWdqs;
            break;
        case BOOT_TR_DEBUG_CMD_RD_RDQS:
            breakpointEn = bFlagBootTrDebugEnRdRdqs;
            break;
        case BOOT_TR_DEBUG_CMD_RD_DQ:
            breakpointEn = bFlagBootTrDebugEnRdDq;
            break;
        case BOOT_TR_DEBUG_CMD_WR_DQ:
            breakpointEn = bFlagBootTrDebugEnWrDq;
            break;
        case BOOT_TR_DEBUG_CMD_WR_DBI:
            breakpointEn = bFlagBootTrDebugEnWrDbi;
            break;
        case BOOT_TR_DEBUG_CMD_MISC:
            breakpointEn = bFlagBootTrDebugEnMisc;
            break;
        default:
            return;
    }
    if (!breakpointEn) {
        return;
    }

    //Build up the command to send to LWMT via FALCON_MONITOR
    LwU32 falcon_mon_cmd = 0;
    falcon_mon_cmd |= 1 << BOOT_TR_DEBUG_FALCON_REQ;
    falcon_mon_cmd |= lwrrent_delay << BOOT_TR_DEBUG_LWRR_DELAY_LSB;
    falcon_mon_cmd |= training_major_step << BOOT_TR_DEBUG_STEP_LSB;
    falcon_mon_cmd |= training_minor_step;
    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, falcon_mon_cmd);

    //Now that the command has been sent, wait for LWMK to send back a command
    falcon_lwmt_monitor_cmd();
}
#else
void falcon_boot_tr_breakpoint (
        LwU8  training_major_step,
        LwU8  training_minor_step,
        LwU16 lwrrent_delay
)
{
    //Empty function definition
    return;
}
#endif

void wr_mbox_elapsed(
        LwU8 mailbox_idx,
        PFLCN_TIMESTAMP startTime
)
{
    FW_MBOX_WR32(mailbox_idx, osPTimerTimeNsElapsedGet(startTime));
    osPTimerTimeNsLwrrentGet(startTime);
}
//=================================================================================================

LwBool isSubpEnabled(
    LwU8 subp_idx
)
{
    #if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
        return isLowerHalfPartitionEnabled(subp_idx);
    #else
        return ((active_subp >> subp_idx) & 0x1);
    #endif
}

//Some vBIOS fields need to be sign extended since they have negative values
//Helper function to take the MSB and extend it through the remaining bits
LwS16 signExtendDword (
    LwS16 value,
    LwU8 msb
)
{
    //If the MSB is a 1, extend it
    if (((value >> msb) & 0x1) == 1) {
        return (value | ~((1 << (msb+1)) - 1));
    }

    //By default, no sign extension
    return value;
}

#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
void
wait_for_training_done
(
        void
)
{
    LwU32 fbpa_trng_status;
    LwU32 subp0_trng_status;
    LwU32 subp1_trng_status;
    NIO_PRINT("Entering wait_for_training_done");
// is this correct?
    do {
        // return TRAINING_STATUS_SUBP0_STATUS_FINISHED if subp0 is disabled
        fbpa_trng_status = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_STATUS);
        subp0_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS, fbpa_trng_status);
        subp1_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS, fbpa_trng_status);
    } while ((subp0_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS_RUNNING) || (subp1_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS_RUNNING));

    return;
}
#endif

void parseBootTrainingTable (void)
{
    //Grab all the tables we need
    BOOT_TRAINING_FLAGS *pTrainingFlags      = &gTT.bootTrainingTable.MemBootTrainTblFlags;
    BOOT_TRAINING_HBM_TRAINING *pHBMTrainingRdRdqs = &gTT.bootTrainingTable.HBM_TRAINING_READ_RDQS;
    BOOT_TRAINING_HBM_TRAINING *pHBMTrainingWrLvl = &gTT.bootTrainingTable.HBM_TRAINING_WRITE_LEVEL;
    BOOT_TRAINING_HBM_MISC1 *pHBMTrainingMisc1 = &gTT.bootTrainingTable.HBM_TRAINING_MISC1;
    BOOT_TRAINING_HBM_PERIODIC_TRAINING *pHBMTrainingPeriodicTr = &gTT.bootTrainingTable.PeriodicTr;
    // these 2 reuses the RW_FINE_INTERPLTR_CTRL from gddr
    BOOT_TRAINING_HBM_TRAINING *pHBMTrainingRdDq = (BOOT_TRAINING_HBM_TRAINING*) &gTT.bootTrainingTable.READ_RW_FINE_INTRPLTR_CTRL;
    BOOT_TRAINING_HBM_TRAINING *pHBMTrainingWr = (BOOT_TRAINING_HBM_TRAINING*) &gTT.bootTrainingTable.WRITE_RW_FINE_INTRPLTR_CTRL;

    //Flags
    bFlagWrLvlEn   = pHBMTrainingMisc1->flag_write_leveling_en;
    bFlagRdTrEn    = pTrainingFlags->flag_g6_rd_tr_en;
    bFlagWrTrEn    = pTrainingFlags->flag_g6_wr_tr_en;
    bFlagDcaEn     = pHBMTrainingMisc1->flag_dca_en;

    //After RD DQ sweep, skip moving the RDQS DDLL backwards if the flag is set
    bFlagSkipRdqsDelayAdj = pTrainingFlags->flag_g6_rd_tr_prbs_en;

    #ifdef BOOT_TR_DEBUG_ENABLE
      //Debug flags (breakpoints)
      bFlagBootTrDebugEnDca    = pTrainingFlags->flag_g6_refresh_en_prbs_tr;
      bFlagBootTrDebugEnWdqs   = pTrainingFlags->flag_g6_wck_tr_en;
      bFlagBootTrDebugEnRdRdqs = pTrainingFlags->flag_g6_rd_tr_hybrid_non_vref_en;
      bFlagBootTrDebugEnRdDq   = pTrainingFlags->flag_g6_rd_tr_hybrid_vref_en;
      bFlagBootTrDebugEnWrDq   = pTrainingFlags->flag_g6_wr_tr_hybrid_non_vref_en;
      bFlagBootTrDebugEnWrDbi  = pTrainingFlags->flag_g6_wr_tr_hybrid_vref_en;
      bFlagBootTrDebugEnMisc   = pTrainingFlags->flag_g6_addr_tr_en;
    #endif
    // do not halt on training in ATE binary with included bios.  Once the flag is available and
    // this code can be unified (the bios has to be pulled into the ATE binary to apply!)
    // Note: initialized to FALSE
    #if (FBFALCONCFG_FEATURE_ENABLED(VBIOS_INCLUDED_IN_BINARY))
        bFlagAteNoFail = pTrainingFlags->flag_ate_nofail_en;
    #endif

    if (pHBMTrainingPeriodicTr->interval != 0)
    {
        bFlagEnablePeriodicTr = LW_TRUE;
    }
    NIO_PRINT("bFlagEnablePeriodicTr = 0x%08x", bFlagEnablePeriodicTr);

    //Safety check - We should never run ATE mode in Nitro
    #if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
        if (bFlagAteNoFail) {
          NIO_ERROR("ATE no fail flag is set for Nitro-based training");
        }
    #endif

    //DBI and ECC flags just come from devinit programming
    LwU32 fbio_config_dbi = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CONFIG_DBI);
    bFlagDbiEnWr = REF_VAL(LW_PFB_FBPA_FBIO_CONFIG_DBI_OUT_ON, fbio_config_dbi);
    bFlagDbiEnRd = REF_VAL(LW_PFB_FBPA_FBIO_CONFIG_DBI_IN_ON,  fbio_config_dbi);

    LwU32 fbio_hbm_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CFG0);
    bFlagEccEn = REF_VAL(LW_PFB_FBPA_FBIO_HBM_CFG0_DRAM_ECC, fbio_hbm_cfg0);

    //Delay settings
    rd_rdqs_delay_min   = pHBMTrainingRdRdqs->TRAINING_DELAY_MIN & 0xFF;
    rd_rdqs_delay_max   = pHBMTrainingRdRdqs->TRAINING_DELAY_MAX & 0xFF;
    rd_rdqs_delay_step  = pHBMTrainingRdRdqs->TRAINING_DELAY_STEP;
    rd_dq_delay_min     = pHBMTrainingRdDq->TRAINING_DELAY_MIN & 0xFF;
    rd_dq_delay_max     = pHBMTrainingRdDq->TRAINING_DELAY_MAX & 0xFF;
    rd_dq_delay_step    = pHBMTrainingRdDq->TRAINING_DELAY_STEP;
    wr_delay_min        = pHBMTrainingWr->TRAINING_DELAY_MIN;
    wr_delay_max        = pHBMTrainingWr->TRAINING_DELAY_MAX;
    wr_delay_step       = pHBMTrainingWr->TRAINING_DELAY_STEP;
    wr_lvl_delay_min    = signExtendDword(pHBMTrainingWrLvl->TRAINING_DELAY_MIN, 9); //Delay min is a signed negative value
    wr_lvl_delay_max    = pHBMTrainingWrLvl->TRAINING_DELAY_MAX;
    wr_lvl_delay_step   = pHBMTrainingWrLvl->TRAINING_DELAY_STEP;
    wr_lvl_derr_samples = pHBMTrainingMisc1->DERR_sample_count;
    wr_derr_rd_delay    = gTT.bootTrainingTable.DERR_READ_DELAY;

    //RD/WR eye width threshold
    min_eye_width       = gTT.bootTrainingTable.FBIO_EDC_TRACKING & 0x3F;

    //Callwlate burst count
    LwU32 char_burst_count[2];
    LwU8 i;
    for (i = TRAINING_DIR_READ; i <= TRAINING_DIR_WRITE; i++) {
        LwU32 char_bank_ctrl  = (i == TRAINING_DIR_READ) ? gTT.bootTrainingTable.READ_CHAR_BANK_CTRL  : gTT.bootTrainingTable.WRITE_CHAR_BANK_CTRL;
        LwU32 char_bank_ctrl2 = (i == TRAINING_DIR_READ) ? gTT.bootTrainingTable.READ_CHAR_BANK_CTRL2 : gTT.bootTrainingTable.WRITE_CHAR_BANK_CTRL2;

        LwU8 char_set_a_min        = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_A_MIN,  char_bank_ctrl);
        LwU8 char_set_a_max        = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_A_MAX,  char_bank_ctrl);
        LwU8 char_set_b_min        = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_B_MIN,  char_bank_ctrl);
        LwU8 char_set_b_max        = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_B_MAX,  char_bank_ctrl);
        LwU8 char_set_a_burst_cnt  = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2_BURST_CNT_A, char_bank_ctrl2);
        LwU8 char_set_b_burst_cnt  = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2_BURST_CNT_B, char_bank_ctrl2);

        char_burst_count[i] = ((char_set_a_max - char_set_a_min + 1) * char_set_a_burst_cnt) + ((char_set_b_max - char_set_b_min + 1) * char_set_b_burst_cnt) - 1; //Make tr2dramc predictor happy
    }
    //Sanity check -- callwlation should match what is in the boot training tables
    if (char_burst_count[TRAINING_DIR_READ] != gTT.bootTrainingTable.READ_CHAR_BURST) {
        FW_MBOX_WR32(1, char_burst_count[TRAINING_DIR_READ]);
        FW_MBOX_WR32(2, gTT.bootTrainingTable.READ_CHAR_BURST);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_CHAR_SETTING_ERROR);
    }
    if (char_burst_count[TRAINING_DIR_WRITE] != gTT.bootTrainingTable.WRITE_CHAR_BURST) {
        FW_MBOX_WR32(1, char_burst_count[TRAINING_DIR_WRITE]);
        FW_MBOX_WR32(2, gTT.bootTrainingTable.WRITE_CHAR_BURST);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_CHAR_SETTING_ERROR);
    }
}

//=============================================================================
//Helper functions for subp pc flip
//HBM2 and HBM3 flip the subp and pc depending on which PA is being addressed
//This means that writing back the final training results to the registers
//needs to be done taking these bits into account
void read_flip_straps(void)
{
    LwU32 fbpa_debug2_base = 0x00902340;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Skip the active partition
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 fbpa_debug2_addr = fbpa_debug2_base + (pa_idx << 14);
        LwU32 fbpa_debug2_data = REG_RD32(BAR0, fbpa_debug2_addr);

        hbm2_flip_strap_per_pa |= (REF_VAL(LW_PFB_FBPA_DEBUG_2_HBM2_PC_FLIP_STRAP, fbpa_debug2_data) << pa_idx);
        hbm3_flip_strap_per_pa |= (REF_VAL(LW_PFB_FBPA_DEBUG_2_HBM3_PC_FLIP_STRAP, fbpa_debug2_data) << pa_idx);
    }
    NIO_PRINT("hbm2_flip_strap_per_pa = 0x%08x", hbm2_flip_strap_per_pa);
    NIO_PRINT("hbm3_flip_strap_per_pa = 0x%08x", hbm3_flip_strap_per_pa);
}

//The following function gets the actual channel
//HBM3 Flipped (non-flipped is just a 1:1 match between HBM CH/PC and FBIO CH/DW)
//    DRAMC   |  HBM  | FBIO Priv
//SUBP PAC CH | CH PC |  CH  DW
//------------+-------+----------
//  0   0  0  | 3  1  |  2   1
//  0   0  1  | 3  0  |  2   0
//  0   1  0  | 2  1  |  3   1
//  0   1  1  | 2  0  |  3   0
//  1   0  0  | 1  1  |  0   1
//  1   0  1  | 1  0  |  0   0
//  1   1  0  | 0  1  |  1   1
//  1   1  1  | 0  0  |  1   0
//
//HBM2 Flipped (non-flipped is just a 1:1 match between HBM CH/PC/DW and FBIO CH/DW)
//    DRAMC   |  HBM  | FBIO Priv
//SUBP PAC CH | CH PC |  CH  DW
//------------+-------+----------
//  0   0  0  | 1  1  |  3   1    Upper DW
//  0   0  0  | 1  1  |  3   0    Lower DW
//
//  0   1  1  | 1  0  |  2   1    Upper DW
//  0   1  1  | 1  0  |  2   0    Lower DW
//
//  1   0  0  | 0  1  |  1   1    Upper DW
//  1   0  0  | 0  1  |  1   0    Lower DW
//
//  1   1  1  | 0  0  |  0   1    Upper DW
//  1   1  1  | 0  0  |  0   0    Lower DW
LwU8 get_flipped_channel (
    LwU8  pa_idx,
    LwU8  subp_idx,
    LwU8  channel_idx
)
{
    LwU32 flipped_channel = (subp_idx*CHANNELS_IN_SUBP) + channel_idx; //Default to no flip
    if (gbl_hbm_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM3) {
        if ((hbm3_flip_strap_per_pa >> pa_idx) & 0x1) {
            flipped_channel = (3 - flipped_channel);
        }
    }
    else {
        if ((hbm2_flip_strap_per_pa >> pa_idx) & 0x1) {
            flipped_channel = (3 - flipped_channel);
        }
    }

    return flipped_channel;
}

LwU8 get_flipped_dword (
    LwU8  pa_idx,
    LwU8  dword
)
{
    if (gbl_hbm_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM3) {
        return (dword ^ ((hbm3_flip_strap_per_pa >> pa_idx) & 0x1));
    }

    return dword;
}

//=============================================================================
//Enable or disable FBIO periodic DDLLCAL. We do this during DCA and write leveling
//to prevent glitches on WDQS
void set_fbio_periodic_ddllcal_disable (
    LwBool disable
)
{
    LwU32 ddllcal_ctrl1 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1);
    //While enabling back periodic calibration, program _IGNORE_START_ENABLE to 1 so that mddll locks fast enough.
    if (disable == LW_FALSE) {
      ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _IGNORE_START, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1_IGNORE_START_ENABLE, ddllcal_ctrl1);
    }
    ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _DISABLE_PERIODIC_UPDATE, disable, ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);
}

//=============================================================================
// TODO fix this patram by integrating with fbflcn_table.c?  -MW
//Load patterns from the boot training tables into the patram
void load_char_patram(
    LwU8 dir
)
{
    NIO_PRINT("Entering load_char_patram");
    LwU32 *CharPatramDQ;
    LwU8  *CharPatramDBI, *CharPatramECC;
    if (dir == TRAINING_DIR_READ) {
        CharPatramDQ  = CharPatramDQ_rd;
        CharPatramDBI = CharPatramDBI_rd;
        CharPatramECC = CharPatramECC_rd;
    }
    else {
        CharPatramDQ  = CharPatramDQ_wr;
        CharPatramDBI = CharPatramDBI_wr;
        CharPatramECC = CharPatramECC_wr;
    }

    LwU8 subp_idx;
    for (subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
        //Set ACT_ADR=ENABLED to target the read/write patram (instead of addr patram)
        LwU32 training_pattern_ptr = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(subp_idx));
        training_pattern_ptr = FLD_SET_DRF(_PFB, _FBPA_TRAINING_PATTERN_PTR, _ACT_ADR, _ENABLED, training_pattern_ptr);

        //The HBM patram is 32 entries of 320 bits each (256 bits of data + 32 bits of DBI + 32 bits of ECC)
        //The pattern is written in 32 bit chunks of DQ and 4 + 4 bit chunks of DBI/ECC
        //This means we need a total of 8 iterations to fill a single entry (8*32 DQ + 8*(4+4) DBI+ECC)
        //32 total entries * 8 iterations = CHAR_PATRAM_WIDTH = 256
        LwU16 char_patram_ptr;
        for(char_patram_ptr = 0; char_patram_ptr < HBM_CHAR_PATRAM_LOOP_CNT; char_patram_ptr++) {
            training_pattern_ptr = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATTERN_PTR, _DP, char_patram_ptr, training_pattern_ptr);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(subp_idx), training_pattern_ptr);

            LwU32 training_dp_cntd = 0;
            training_dp_cntd = FLD_SET_DRF    (_PFB, _FBPA_TRAINING_DP_CNTD, _SEL, _CHAR_ENGINE,                         training_dp_cntd);
            training_dp_cntd = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_DP_CNTD, _DBI, (CharPatramDBI[char_patram_ptr] & 0xF), training_dp_cntd);
            training_dp_cntd = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_DP_CNTD, _EDC, (CharPatramECC[char_patram_ptr] & 0xF), training_dp_cntd);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DP_CNTD(subp_idx), training_dp_cntd);

            LwU32 training_dp = CharPatramDQ[char_patram_ptr];
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DP(subp_idx), training_dp);
        }
    }
}

void set_min_ccds(void)
{
    NIO_PRINT("Entering set_min_ccds");
    saved_reg->pfb_fbpa_config3 = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG3);
    if (REF_VAL(LW_PFB_FBPA_CONFIG3_CCDS, saved_reg->pfb_fbpa_config3) < 2) {
        LwU32 config3_val = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG3, _CCDS, LW_PFB_FBPA_CONFIG3_CCDS_MAX, saved_reg->pfb_fbpa_config3);
        REG_WR32(LOG, LW_PFB_FBPA_CONFIG3, config3_val);
    }
}

void set_dbi_enable(
    LwBool enable
)
{
    NIO_PRINT("Entering set_dbi_enable(%0d)", enable);
    //Set DBI enable/disable for HBM Char
    LwU32 hbm_char = REG_RD32(BAR0, LW_PFB_FBPA_HBM_CHAR);
    hbm_char = FLD_SET_DRF_NUM(_PFB, _FBPA_HBM_CHAR, _DBI, enable, hbm_char);
    REG_WR32(LOG, LW_PFB_FBPA_HBM_CHAR, hbm_char);

    //Enable/Disable DBI on FBIO
    LwU32 fbio_config_dbi = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CONFIG_DBI);
    fbio_config_dbi = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CONFIG_DBI, _OUT_ON, enable, fbio_config_dbi);
    fbio_config_dbi = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CONFIG_DBI, _IN_ON,  enable, fbio_config_dbi);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CONFIG_DBI, fbio_config_dbi);

    //Enable/Disable DBI on the HBM memory using MRS
    LwU32 mrs0 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS);
    mrs0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS, _ADR_HBM_RDBI, enable, mrs0);
    mrs0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS, _ADR_HBM_WDBI, enable, mrs0);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS, mrs0);
}

void set_ecc_enable(
    LwBool enable
)
{
    NIO_PRINT("Entering set_ecc_enable(%0d)", enable);
    //Set ECC enable/disable for HBM Char
    LwU32 hbm_char = REG_RD32(BAR0, LW_PFB_FBPA_HBM_CHAR);
    hbm_char = FLD_SET_DRF_NUM(_PFB, _FBPA_HBM_CHAR, _ECC, enable, hbm_char);
    REG_WR32(LOG, LW_PFB_FBPA_HBM_CHAR, hbm_char);

    //Enable/Disable ECC on FBIO
    LwU32 fbio_hbm_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CFG0);
    fbio_hbm_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG0, _DRAM_ECC, enable, fbio_hbm_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CFG0, fbio_hbm_cfg0);

    //Enable/Disable ECC on the HBM memory using MRS
    //FIXME Have design add field defines for these so we can use FLD_SET_DRF* instead of bit shifting
    LwU32 mrs9 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
    mrs9 = (mrs9 & 0xFFFFFFFE) | enable; //[0] is MD where a value of 1 means enable
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9, mrs9);
}

//Program the address (row/bank) for char to use
//FBPA_TRAINING_PATTERN_PTR*
//FBPA_TRAINING_ADR*
void pgm_char_addr(
    LwU8 dir
)
{
    LwU8 subp_idx;
    for (subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
        LwU32 training_pattern_ptr = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(subp_idx));
        training_pattern_ptr = FLD_SET_DRF    (_PFB, _FBPA_TRAINING_PATTERN_PTR, _ACT_ADR, _ENABLED, training_pattern_ptr);
        training_pattern_ptr = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATTERN_PTR, _ADR,     0,    training_pattern_ptr);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(subp_idx), training_pattern_ptr);
        if (dir == TRAINING_DIR_READ) {
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADR(subp_idx), gTT.bootTrainingTable.READ_ADR0);
        }
        else {
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADR(subp_idx), gTT.bootTrainingTable.WRITE_ADR0);
        }

        training_pattern_ptr = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATTERN_PTR, _ADR,     1,    training_pattern_ptr);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(subp_idx), training_pattern_ptr);
        if (dir == TRAINING_DIR_READ) {
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADR(subp_idx), gTT.bootTrainingTable.READ_ADR1);
        }
        else {
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADR(subp_idx), gTT.bootTrainingTable.WRITE_ADR1);
        }
    }
}

//Program the basic char controls in:
//FBPA_TRAINING_CHAR_CTRL
//FBPA_HBM_CHAR
//FBPA_TRAINING_ARRAY_CTRL
//FBPA_TRAINING_PATRAM
void pgm_char_controls(
    LwU8 dir
)
{
    NIO_PRINT("Entering pgm_char_controls");
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_CTRL, gTT.bootTrainingTable.CHAR_CTRL);

    //Enable both PC
    LwU32 hbm_char = REG_RD32(BAR0, LW_PFB_FBPA_HBM_CHAR);
    hbm_char = FLD_SET_DRF(_PFB, _FBPA_HBM_CHAR, _PC, _PC01, hbm_char);
    REG_WR32(LOG, LW_PFB_FBPA_HBM_CHAR, hbm_char);

    //Set up the address for char to use
    pgm_char_addr(dir);

    //Set up the burst and turn counts
    if (dir == TRAINING_DIR_READ) {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BURST, gTT.bootTrainingTable.READ_CHAR_BURST);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_TURN, 0); //There are no turns for read training
    }
    else {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BURST, gTT.bootTrainingTable.WRITE_CHAR_BURST);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_TURN, gTT.bootTrainingTable.WRITE_CHAR_TURN);
    }

    //Disable GDDR5 commands
    memorySetTrainingControlGddr5Commands_HAL(&Fbflcn, LW_PFB_FBPA_TRAINING_ARRAY_CTRL_GDDR5_COMMANDS_DISABLED);

    //Set patram indices to be used for char
    LwU32 training_patram = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    if (dir == TRAINING_DIR_READ) {
        training_patram = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATRAM, _CHAR_MIN, REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_CHAR_MIN, gTT.bootTrainingTable.READ_PATRAM), training_patram);
        training_patram = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATRAM, _CHAR_MAX, REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_CHAR_MAX, gTT.bootTrainingTable.READ_PATRAM), training_patram);
    }
    else {
        training_patram = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATRAM, _CHAR_MIN, REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_CHAR_MIN, gTT.bootTrainingTable.WRITE_PATRAM), training_patram);
        training_patram = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATRAM, _CHAR_MAX, REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_CHAR_MAX, gTT.bootTrainingTable.WRITE_PATRAM), training_patram);
    }
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM, training_patram);
}

//Set up char bank control
void pgm_char_bank_ctrl(
    LwU8 dir
)
{
    NIO_PRINT("Entering pgm_char_bank_ctrl");
    if (dir == TRAINING_DIR_READ) {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL,  gTT.bootTrainingTable.READ_CHAR_BANK_CTRL);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2, gTT.bootTrainingTable.READ_CHAR_BANK_CTRL2);
    }
    else {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL,  gTT.bootTrainingTable.WRITE_CHAR_BANK_CTRL);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2, gTT.bootTrainingTable.WRITE_CHAR_BANK_CTRL2);
    }
}

void char_setup(
    LwU8   dir,
    LwBool dbi_en
)
{
    NIO_PRINT("Entering char_setup");
    #if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
    set_min_ccds();
    #endif

    set_dbi_enable(dbi_en);
    set_ecc_enable(bFlagEccEn);
    pgm_char_controls(dir);
    pgm_char_bank_ctrl(dir);
    //TODO Do we plan to use counter mode at all? If set add a function for setup
}

void start_training(
    LwU8 dir
)
{
    NIO_PRINT("Entering start_training(%0d)", dir);

    //Save the contents of DRAM_ASR and clear it
    saved_reg->pfb_fbpa_dram_asr = REG_RD32(BAR0, LW_PFB_FBPA_DRAM_ASR);
    REG_WR32(LOG, LW_PFB_FBPA_DRAM_ASR, 0);

    //Set up TRAINING_CMD - Note: CMD0 and CMD1 should be identical
    LwU32 training_cmd = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CMD(1));
    training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CHAR_ENGINE, _ENABLED, training_cmd);
    training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,          _NOW,     training_cmd);
    if ((dir == TRAINING_DIR_READ) || (dir == TRAINING_DIR_BOTH)) {
        training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD, _ENABLED,  training_cmd);
    }
    else {
        training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD, _DISABLED, training_cmd);
    }
    if ((dir == TRAINING_DIR_WRITE) || (dir == TRAINING_DIR_BOTH)) {
        training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED,  training_cmd);
    }
    else {
        training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _DISABLED, training_cmd);
    }

    //Start training
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(0), training_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(1), training_cmd);
}

//Initialize the eye tracking arrays to all 0s
void initialize_eye_arrays(void)
{
    NIO_PRINT("Entering initialize_eye_arrays");
    memset(lwrrent_eye_per_dq, 0, sizeof(lwrrent_eye_per_dq));
    memset(best_eye_per_dq, 0, sizeof(best_eye_per_dq));
    memset(lwrrent_eye_per_dbi, 0, sizeof(lwrrent_eye_per_dbi));
    memset(best_eye_per_dbi, 0, sizeof(best_eye_per_dbi));
    memset(lwrrent_eye_per_ecc, 0, sizeof(lwrrent_eye_per_ecc));
    memset(best_eye_per_ecc, 0, sizeof(best_eye_per_ecc));
    memset(first_pass_per_rdqs, 0, sizeof(first_pass_per_rdqs));
    /*
    LwU16 dq_dbi_idx;
    for (dq_dbi_idx = 0; dq_dbi_idx < TOTAL_DQ_BITS; dq_dbi_idx++) {
        lwrrent_eye_per_dq[dq_dbi_idx] = 0;
        best_eye_per_dq[dq_dbi_idx] = 0;
    }
    for (dq_dbi_idx = 0; dq_dbi_idx < TOTAL_DBI_BITS; dq_dbi_idx++) {
        lwrrent_eye_per_dbi[dq_dbi_idx] = 0;
        best_eye_per_dbi[dq_dbi_idx] = 0;
    }
    for (dq_dbi_idx = 0; dq_dbi_idx < TOTAL_ECC_BITS; dq_dbi_idx++) {
        lwrrent_eye_per_ecc[dq_dbi_idx] = 0;
        best_eye_per_ecc[dq_dbi_idx] = 0;
    }
    for (dq_dbi_idx = 0; dq_dbi_idx < TOTAL_DQS_BITS; dq_dbi_idx++) {
        first_pass_per_rdqs[dq_dbi_idx] = 0;
    }
    */
}

void update_eye_arrays(
    LwU16  delay,
    LwU16  prev_delay,
    LwU16  pin_idx,
    LwBool pin_is_failing,
    LwBool is_dbi,
    LwBool is_ecc,
    LwU8   dir
)
{
    //NIO_PRINT("Entering update_eye_arrays(%0d, %0d, %0d, %0d, %0d, %0d)", delay, pin_idx, pin_is_failing, is_dbi, is_ecc, dir);
    //Grab pointers to the correct arrays so the function can be generic
    PER_PIN_EYE_INFO *lwrrent_eye = is_dbi ? lwrrent_eye_per_dbi : is_ecc ? lwrrent_eye_per_ecc : lwrrent_eye_per_dq;
    PER_PIN_EYE_INFO *best_eye    = is_dbi ? best_eye_per_dbi    : is_ecc ? best_eye_per_ecc    : best_eye_per_dq;

    LwU8 delay_offset = delay - prev_delay;

    //NIO_PRINT("delay=%0d, pin_idx=%0d, fail=%0d", delay, pin_idx, pin_is_failing);

    //The current eye is always reset to 0 if we're in a failing region. This
    //means if the eye width is non-zero, we're in the middle of a passing
    //region (or just failed after being in a passing region) and need to
    //update the eye
    //If the current eye size is 0 and we passed, set up for a new eye
    if (lwrrent_eye[pin_idx].width > 0) {
        //If we are still passing, just increment the width by the delay offset
        //If failing, the eye just closed. Update best if needed, then reset the current eye information
        if (!pin_is_failing) {
            if ((lwrrent_eye[pin_idx].width + delay_offset) < 63) {
                lwrrent_eye[pin_idx].width += delay_offset;
            }
            else {
                lwrrent_eye[pin_idx].width = 63;
            }
            //NIO_PRINT("\tSaw pass in the middle of an eye for pin %0d. New width = %0d", pin_idx, lwrrent_eye[pin_idx].width);
        }
        else {
            //The eye just closed.  If this eye is bigger than the previous best, update
            NIO_PRINT("\tSaw fail in the middle of an eye for PA%0d %s pin %0d. Current width = %0d, (Previous) Best width = %0d",
                    (pin_idx / ((is_dbi ? DBI_BITS_IN_SUBP : is_ecc ? ECC_BITS_IN_SUBP : DQ_BITS_IN_SUBP) * SUBPS)),
                    (is_dbi ? "DBI" : is_ecc ? "ECC" : "DQ"),
                    (pin_idx % ((is_dbi ? DBI_BITS_IN_SUBP : is_ecc ? ECC_BITS_IN_SUBP : DQ_BITS_IN_SUBP) * SUBPS)),
                    lwrrent_eye[pin_idx].width,
                    best_eye[pin_idx].width);
            if (lwrrent_eye[pin_idx].width > best_eye[pin_idx].width) {
                best_eye[pin_idx] = lwrrent_eye[pin_idx];
            }

            //No matter what, reset so we can start tracking a new eye
            lwrrent_eye[pin_idx].width = 0;
            lwrrent_eye[pin_idx].delay = 0;
        }
    }
    else {
        if (!pin_is_failing) {
            //First pass for the eye. Save the current delay and set the width to 1 since
            //we only know that this specific setting passed.
            lwrrent_eye[pin_idx].delay = delay;
            lwrrent_eye[pin_idx].width = 1;
            NIO_PRINT("\tNew eye started for PA%0d %s pin %0d - Delay = %0d",
                    (pin_idx / ((is_dbi ? DBI_BITS_IN_SUBP : is_ecc ? ECC_BITS_IN_SUBP : DQ_BITS_IN_SUBP) * SUBPS)),
                    (is_dbi ? "DBI" : is_ecc ? "ECC" : "DQ"),
                    (pin_idx % ((is_dbi ? DBI_BITS_IN_SUBP : is_ecc ? ECC_BITS_IN_SUBP : DQ_BITS_IN_SUBP) * SUBPS)),
                    delay);
        }
    }
}

//Read the TRAINING_DEBUG_DQ registers to get per-RDQS status.
//
//Return a bool indicating if at least one pin per RDQS is
//passing. This bool is used to end the training sweep
LwBool read_per_rdqs_pass_fail_status
(
    LwU16  delay
)
{
    NIO_PRINT("Entering read_per_rdqs_pass_fail_status(%0d)", delay);

    //Return value indicating all pins are failing
    LwBool all_rdqs_see_pass = LW_TRUE;

    //Base addresses for debug registers
    LwU32 debug_ctrl_base = 0x009A09C0;
    LwU32 debug_dq_base   = 0x00900938;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            // Set debug ctrl register - First read the DQ pins
            LwU32 debug_ctrl_addr = debug_ctrl_base + (subp_idx*4);
            LwU32 debug_ctrl_selection = LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_FAIL_DQ;
            REG_WR32(LOG, debug_ctrl_addr, debug_ctrl_selection);

            //Callwlate the starting RDQS index for this subp
            //[bswamy] I would prefer to have this all callwlated with no assumptions about number of
            //pins per RDQS but the logic would be unnecessarily complicated. There are 32 DQ per RDQS
            //so a single DEBUG_DQ register corresponds to a single RDQS
            //Since we need to track if all pins are failing across DQ + DBI + ECC, keep a boolean per RDQS
            LwU16 rdqs_idx = (pa_idx   * DQS_BITS_IN_PA) +
                             (subp_idx * DQS_BITS_IN_SUBP);

            LwU8 debug_dq_idx;
            for(debug_dq_idx = 0; debug_dq_idx < 4; debug_dq_idx++) {
                //If the L2 slice is disabled, don't read back training data
                if (isL2SliceDisabled(pa_idx, subp_idx, (debug_dq_idx/DWORDS_IN_CHANNEL))) {
                    continue;
                }

                //Read debug DQ register
                //These are addressed (in order) as DEBUG_DQ0_SUBP0, DEBUG_DQ0_SUBP1, DEBUG_DQ1_SUBP0, ...
                //So when indexing, do SUBPS*4 + dq*8
                LwU32 debug_dq_addr = debug_dq_base +
                                      ((pa_idx * 4 ) << 12) +
                                      (subp_idx * 4) +
                                      (debug_dq_idx * 8 );

                LwU32 debug_reg_read = REG_RD32(BAR0, debug_dq_addr);

                //Note: DEBUG_DQ* uses a value of "1" on a given pin to indicate it failed
                LwU16 pin_idx;
                for (pin_idx = 0; pin_idx < DQ_BITS_IN_DWORD; pin_idx++) {
                    LwBool pin_is_failing = (debug_reg_read & 0x1);
                    first_pass_per_rdqs[(rdqs_idx + debug_dq_idx)].passing_setting_seen |= !pin_is_failing;
                    debug_reg_read = debug_reg_read >> 1; //Shift the next pass/fail bit to [0]
                }
            }

            //Read the results for DBI and ECC, if enabled
            if (bFlagDbiEnRd || bFlagEccEn) {
                //Set debug ctrl register for DBI and ECC pins
                debug_ctrl_selection = LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_FAIL_DBI;
                REG_WR32(LOG, debug_ctrl_addr, debug_ctrl_selection);

                //We only need to read DEBUG_DQ0 for DBI and ECC
                //These are addressed (in order) as DEBUG_DQ0_SUBP0, DEBUG_DQ0_SUBP1, DEBUG_DQ1_SUBP0, ...
                //So when indexing, do SUBPS*4 + dq*8
                LwU32 debug_dq_addr = debug_dq_base +
                                      ((pa_idx * 4 ) << 12) +
                                      (subp_idx * 4);

                LwU32 debug_reg_read = REG_RD32(BAR0, debug_dq_addr);

                LwU16 pin_idx;
                for (pin_idx = 0; pin_idx < DBI_BITS_IN_SUBP; pin_idx++) {
                    if (bFlagDbiEnRd) {
                        LwU16 rdqs_idx_offset = (pin_idx / DBI_BITS_IN_DWORD);
                        LwBool pin_is_failing = (debug_reg_read & 0x1);
                        first_pass_per_rdqs[(rdqs_idx + rdqs_idx_offset)].passing_setting_seen |= !pin_is_failing;
                    }

                    //The ECC feedback is every other bit in the upper 16 bits ([16], [18], ...)
                    if (bFlagEccEn && ((pin_idx % 2) == 0)) {
                        LwU16 rdqs_idx_offset = (pin_idx >> 1) / ECC_BITS_IN_DWORD;
                        LwBool pin_is_failing = ((debug_reg_read >> 16) & 0x1);
                        first_pass_per_rdqs[(rdqs_idx + rdqs_idx_offset)].passing_setting_seen |= !pin_is_failing;
                    }
                    debug_reg_read = debug_reg_read >> 1; //Shift the next pass/fail bit to [0]
                }
            }

            //All of the pass/fail states for this subp have been read. We can now update
            //first_pass_per_rdqs for each RDQS in the subp
            //If we saw our first passing pin, want to set the delay = delay + step to ensure
            //the best possible DQ sweep and avoid lwtting off the end of an eye.
            LwU8 rdqs_idx_offset;
            for (rdqs_idx_offset = 0; rdqs_idx_offset < DQS_BITS_IN_SUBP; rdqs_idx_offset++) {
                //If the L2 slice for this RDQS is disabled, don't let it cause a failure
                if (isL2SliceDisabled(pa_idx, subp_idx, (rdqs_idx_offset/DWORDS_IN_CHANNEL))) {
                    continue;
                }

                LwU16 rdqs_idx_actual = rdqs_idx + rdqs_idx_offset;
                all_rdqs_see_pass &= first_pass_per_rdqs[rdqs_idx_actual].passing_setting_seen;

                if ((first_pass_per_rdqs[rdqs_idx_actual].passing_setting_seen) &&
                    (first_pass_per_rdqs[rdqs_idx_actual].first_passing_delay == 0)) {
                    NIO_PRINT("RDQS Sweep: Saw a passing pin for PA%0d (SUBP%0d) RDQS%0d at delay %0d (rdqs_idx_actual=%0d, DQS_BITS_IN_PA=%0d)", pa_idx, subp_idx, (DQS_BITS_IN_SUBP*subp_idx + rdqs_idx_offset), delay, rdqs_idx_actual, DQS_BITS_IN_PA);
                    first_pass_per_rdqs[rdqs_idx_actual].first_passing_delay = delay + rd_rdqs_delay_step;
                }
            }
        }
    }

    return all_rdqs_see_pass;
}

//Read the TRAINING_DEBUG_DQ registers to get per-pin status.
//Update the current and best eye arrays
//
//Return a bool indicating if all pins are failing. This bool is used to
//end the training sweep
LwBool read_per_pin_pass_fail_status
(
    LwU16  delay,
    LwU16  prev_delay,
    LwBool is_dbi_ecc,
    LwU8   dir
)
{
    NIO_PRINT("Entering read_per_pin_pass_fail_status(%0d, %0d, %0d, %0d)", delay, prev_delay, is_dbi_ecc, dir);

    //Return value indicating all pins are failing
    LwBool all_pins_failing = LW_TRUE;

    //Base addresses for debug registers
    LwU32 debug_ctrl_base = 0x009A09C0;
    LwU32 debug_dq_base   = 0x00900938;

    LwBool bFlagDbiEn      = ((dir == TRAINING_DIR_READ) || (dir == TRAINING_DIR_READ_RDQS)) ? bFlagDbiEnRd : bFlagDbiEnWr;
    LwU8 pins_per_debug_dq = is_dbi_ecc ? 16 : 32;
    LwU16 bits_per_subp    = is_dbi_ecc ? DBI_BITS_IN_SUBP : DQ_BITS_IN_SUBP;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwBool all_pins_failing_subp0 = LW_FALSE; //[bswamy] Hack for training TB
        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            // Set debug ctrl register
            LwU32 debug_ctrl_addr = debug_ctrl_base + (subp_idx*4);
            LwU32 debug_ctrl_selection = is_dbi_ecc ? LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_FAIL_DBI : LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_HBM_CHAR_FAIL_DQ;
            REG_WR32(LOG, debug_ctrl_addr, debug_ctrl_selection);

            LwU8 debug_dq_idx;
            for(debug_dq_idx = 0; debug_dq_idx < 4; debug_dq_idx++) {
                //If the L2 slice is disabled, don't read back training data. DBI/ECC are handled below
                if (!is_dbi_ecc && isL2SliceDisabled(pa_idx, subp_idx, (debug_dq_idx/DWORDS_IN_CHANNEL))) {
                    continue;
                }

                //Read debug DQ register
                //These are addressed (in order) as DEBUG_DQ0_SUBP0, DEBUG_DQ0_SUBP1, DEBUG_DQ1_SUBP0, ...
                //So when indexing, do SUBPS*4 + dq*8
                LwU32 debug_dq_addr = debug_dq_base +
                                      ((pa_idx * 4 ) << 12) +
                                      (subp_idx * 4) +
                                      (debug_dq_idx * 8 );

                LwU32 debug_reg_read = REG_RD32(BAR0, debug_dq_addr);

                //Callwlate the starting index in the per_pin* arrays for this set of bits
                LwU16 per_pin_start_index = (pa_idx * SUBPS * bits_per_subp) +
                                            (subp_idx * bits_per_subp) +
                                            (debug_dq_idx * pins_per_debug_dq);
                LwU16 pin_idx_ecc = (pa_idx * SUBPS * ECC_BITS_IN_SUBP) +
                                    (subp_idx * ECC_BITS_IN_SUBP) +
                                    (debug_dq_idx * pins_per_debug_dq);

                //Note: DEBUG_DQ* uses a value of "1" on a given pin to indicate it failed
                LwU16 pin_idx;
                for (pin_idx = per_pin_start_index; pin_idx < (per_pin_start_index + pins_per_debug_dq); pin_idx++) {
                    if (is_dbi_ecc) {
                        //DEBUG_DQ0[31:16] is ECC
                        //DEBUG_DQ0[15:0]  is DBI
                        if (bFlagDbiEn && !isL2SliceDisabled(pa_idx, subp_idx, ((pin_idx - per_pin_start_index)/(DBI_BITS_IN_DWORD*DWORDS_IN_CHANNEL)))) {
                            all_pins_failing &= (debug_reg_read & 0x1);
                            update_eye_arrays(delay, prev_delay, pin_idx,     (debug_reg_read & 0x1),         LW_TRUE, LW_FALSE, dir); //DBI
                        }

                        //The ECC feedback is every other bit in the upper 16 bits ([16], [18], ...)
                        if (bFlagEccEn && ((pin_idx % 2) == 0)) {
                            if (!isL2SliceDisabled(pa_idx, subp_idx, ((pin_idx - per_pin_start_index)/(2*ECC_BITS_IN_DWORD*DWORDS_IN_CHANNEL)))) {
                                all_pins_failing &= ((debug_reg_read >> 16) & 0x1);
                                update_eye_arrays(delay, prev_delay, pin_idx_ecc, ((debug_reg_read >> 16) & 0x1), LW_FALSE, LW_TRUE, dir); //ECC
                            }

                            //Always increment the pin index even if the slice is disabled so we store data correctly
                            pin_idx_ecc++;
                        }
                    }
                    else {
                        all_pins_failing &= (debug_reg_read & 0x1);
                        update_eye_arrays(delay, prev_delay, pin_idx, (debug_reg_read & 0x1), LW_FALSE, LW_FALSE, dir);
                    }
                    debug_reg_read = debug_reg_read >> 1; //Shift the next pass/fail bit to [0]
                }

                //DBI/ECC only use DEBUG_DQ0 so break the loop and move to the next SUBP
                //Otherwise we'll grab invalid data and have incorrect eye tracking
                if (is_dbi_ecc) {
                    break;
                }
            }

            //[bswamy] Hack for training TB - Only a single subp is enabled so we need to
            //ignore the disabled one to prevent false failures
            if (subp_idx == 0) {
                all_pins_failing_subp0 = all_pins_failing;
            }
            if (!isSubpEnabled(subp_idx)) {
                //For the disabled subp we will never see all pins failing
                //If subp0 is disabled, reset all_pins_failing to true. The status will be updated on the
                //next loop iteration for subp1
                //If subp1 is disabled, use the status from subp0 since that is all we care about
                if (subp_idx == 0) {
                    all_pins_failing = LW_TRUE;
                }
                else {
                    all_pins_failing = all_pins_failing_subp0;
                }
            }
        }
    }

    return all_pins_failing;
}

//After training (read or write) is complete, it is possible that the lwrrent_eye_per_* array actually has
//the best eye. Do one final call to update_eye_arrays with pin_is_failing=true to force an update of the
//best_* arrays.
void end_of_training_update_eye_arrays(
    LwU8 dir,
    LwBool dbi_only
)
{
    NIO_PRINT("Entering end_of_training_update_eye_arrays(%0d, %0d)", dir, dbi_only);

    LwU16 pin_idx; //Current pin being updated
    LwBool bFlagDbiEn = (dir == TRAINING_DIR_READ) ? bFlagDbiEnRd : bFlagDbiEnWr;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 subp_idx;
        for (subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            //Update all of the DQ bits
            if (!dbi_only) {
                LwU16 per_pin_start_index = (pa_idx * SUBPS    * DQ_BITS_IN_SUBP +
                                                      subp_idx * DQ_BITS_IN_SUBP);
                for (pin_idx = per_pin_start_index; pin_idx < (per_pin_start_index + DQ_BITS_IN_SUBP); pin_idx++) {
                    update_eye_arrays(0xFFFF, 0xFFFF, pin_idx, LW_TRUE, LW_FALSE, LW_FALSE, dir);
                }
            }

            //Update all of the DBI bits
            if (bFlagDbiEn || dbi_only) {
                LwU16 per_pin_start_index = (pa_idx * SUBPS    * DBI_BITS_IN_SUBP +
                                                      subp_idx * DBI_BITS_IN_SUBP);
                for (pin_idx = per_pin_start_index; pin_idx < (per_pin_start_index + DBI_BITS_IN_SUBP); pin_idx++) {
                    update_eye_arrays(0xFFFF, 0xFFFF, pin_idx, LW_TRUE, LW_TRUE,  LW_FALSE, dir); //DBI
                }
            }

            //Update all of the ECC bits
            if (bFlagEccEn && !dbi_only) {
                LwU16 per_pin_start_index = (pa_idx * SUBPS    * ECC_BITS_IN_SUBP +
                                                      subp_idx * ECC_BITS_IN_SUBP);
                for (pin_idx = per_pin_start_index; pin_idx < (per_pin_start_index + ECC_BITS_IN_SUBP); pin_idx++) {
                    update_eye_arrays(0xFFFF, 0xFFFF, pin_idx, LW_TRUE, LW_FALSE, LW_TRUE,  dir); //ECC
                }
            }
        }
    }
}

void force_dq_swap_pulse(
    LwBool enable
)
{
    LwU32 fbio_hbm_cfg13 = saved_reg->pfb_fbio_hbm_cfg13;
    fbio_hbm_cfg13 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG13_BRICK_SWAP_PULSE, _DQ_FORCE, enable, fbio_hbm_cfg13);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CFG13_BRICK_SWAP_PULSE, fbio_hbm_cfg13);
}

void reset_ififo_rdqs_div_pointers(void)
{
    NIO_PRINT("Entering reset_ififo_rdqs_div_pointers");
    LwU32 fbio_hbm_cfg0 = saved_reg->pfb_fbio_hbm_cfg0;
    fbio_hbm_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG0, _FBIO_IFIFO_PTR_RESET, 0x0, fbio_hbm_cfg0);
    fbio_hbm_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG0, _PAD_RDQS_DIV_RST,     0x1, fbio_hbm_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CFG0, fbio_hbm_cfg0);

    fbio_hbm_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG0, _FBIO_IFIFO_PTR_RESET, 0x1, fbio_hbm_cfg0);
    fbio_hbm_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG0, _PAD_RDQS_DIV_RST,     0x0, fbio_hbm_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CFG0, fbio_hbm_cfg0);
}

void set_ib_dq_force_perdiodic_updates(
    LwBool enable
)
{
  LwU32 hbm_trng_misc = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC);
  hbm_trng_misc = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_MISC, _IB_DQ_FORCE_PERIODIC_UPDATES, enable, hbm_trng_misc);
  REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC, hbm_trng_misc);
}

void set_ob_dq_force_perdiodic_updates(
    LwBool enable
)
{
  LwU32 hbm_trng_misc = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC);
  hbm_trng_misc = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_MISC, _OB_DQ_FORCE_PERIODIC_UPDATES, enable, hbm_trng_misc);
  REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC, hbm_trng_misc);
}

void program_fbio_rx_ddll_delay_broadcast(
    LwU8 delay
)
{
    NIO_PRINT("Entering program_fbio_rx_ddll_delay_broadcast(%0d)", delay);
    LwU32 broadcast_misc1 = saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc1;
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_TRIM, delay, broadcast_misc1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, broadcast_misc1);
}

void program_fbio_ib_delay_broadcast(
    LwU8 delay
)
{
    NIO_PRINT("Entering program_fbio_ib_delay_broadcast(%0d)", delay);
    LwU32 hbm_trng_broadcast2 = 0;
    hbm_trng_broadcast2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST2, _IB_DDLL, delay, hbm_trng_broadcast2);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST2, hbm_trng_broadcast2);
}

void program_fbio_ob_delay_broadcast(
    LwU16 delay
)
{
    NIO_PRINT("Entering program_fbio_ob_delay_broadcast(%0d)", delay);
    LwU32 hbm_trng_broadcast0 = 0;
    hbm_trng_broadcast0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST0, _OB_DDLL, REF_VAL(5:0, delay), hbm_trng_broadcast0);
    hbm_trng_broadcast0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST0, _OB_BRL,  REF_VAL(9:6, delay), hbm_trng_broadcast0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST0, hbm_trng_broadcast0);
}

//Function to add offset to final trained value.
LwU16 add_offset(
    LwU16 eye_center,
    LwU8  dir
)
{
    LwS16 offset = (dir == TRAINING_DIR_READ) ? signExtendDword(gTT.bootTrainingTable.VREF_DFE_2.trim_offset_ib, 5) :
                                                signExtendDword(gTT.bootTrainingTable.VREF_DFE_2.trim_offset_ob, 5); //offset is a signed number

    LwU16 shifted_eye_center = eye_center + offset;
    //After adding offset, if the value overflows/underflows, return 0;
    if(shifted_eye_center > 1023) {
        return 0;
    } else {
        return shifted_eye_center;
    }
}
//Find the eye center for the given pin
//If there is no eye, set all_pins_passing = false and return max value for BS + Trimmer
LwU16 find_eye_center(
    const PER_PIN_EYE_INFO* const pin_eye_info,
    LwBool* const all_pins_passing
)
{
    //If there is no eye, set all_pins_passing to false and return immediately
    if (pin_eye_info->width <= min_eye_width) {
        *all_pins_passing = LW_FALSE;
        return 0xFFFF;
    }

    //If there is an eye, return the center of it
    return (pin_eye_info->delay + (pin_eye_info->width/2));
}

void program_final_ib_rdqs_delays (
    void
)
{
    NIO_PRINT("Entering program_final_ib_rdqs_delays");

    //Base address for the first RX DDLL register - RX DDLL is one per DWORD
    LwU32 hbm_rx_ddll_base = 0x009010e0;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            LwU8 channel_idx;
            for (channel_idx = 0; channel_idx < CHANNELS_IN_SUBP; channel_idx++) {
                LwU8 dword_idx;
                for (dword_idx = 0; dword_idx < DWORDS_IN_CHANNEL; dword_idx++) {
                    //Registers are grouped as WL_DDLL, TX_DDLL, RX_DDLL per channel/dword
                    //so add 12 to jump between RX_DDLL registers
                    //Note: subp is account for in get_flipped_channel so it isn't in the callwlation below
                    LwU32 hbm_rx_ddll_addr = hbm_rx_ddll_base +
                                             (pa_idx << 14) +
                                             //(subp_idx    * 48) + //48 = 2 channel/subp (2 ch * 2 DW * 12)
                                             (get_flipped_channel(pa_idx, subp_idx, channel_idx) * 24) + //24 = 2 DW/channel (2 DW * 12)
                                             (get_flipped_dword(pa_idx, dword_idx) * 12);

                    //Get the base rdqs pin index for this PA/SUBP/CH/DW
                    LwU16 per_pin_index_rdqs = (pa_idx      * DQS_BITS_IN_PA) +
                                               (subp_idx    * DQS_BITS_IN_SUBP) +
                                               (channel_idx * DQS_BITS_IN_CHANNEL) +
                                               (dword_idx);

                    //Program RX DDLL (RDQS) to the first delay where all pins were failing
                    LwU32 rx_ddll_data = REG_RD32(BAR0, hbm_rx_ddll_addr);
                    rx_ddll_data = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL, _TRIM, first_pass_per_rdqs[per_pin_index_rdqs].first_passing_delay, rx_ddll_data);
                    REG_WR32(LOG, hbm_rx_ddll_addr, rx_ddll_data);
                }
            }
        }
    }
}

void program_final_ib_delays(
    void
)
{
    NIO_PRINT("Entering program_final_ib_delays");

    //Base address for the first IB DDLL register and RX DDLL register
    //For DQ DDLL there are 10 registers per DWORD :: REG0-7 cover 32 DQ bits, REG8 covers 4 DBI bits, REG9 covers 2 ECC bits + 2 SEV bits
    //RX DDLL is one per DWORD
    LwU32 hbm_ib_ddll_dq_base = 0x00901840;
    LwU32 hbm_rx_ddll_base = 0x009010e0;

    //Keep track of whether any pin has no eye
    LwBool all_pins_passing = LW_TRUE;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            LwU8 channel_idx;
            for (channel_idx = 0; channel_idx < CHANNELS_IN_SUBP; channel_idx++) {
                //If the L2 slice for this channel is disabled, don't program final values
                if (isL2SliceDisabled(pa_idx, subp_idx, channel_idx)) {
                    continue;
                }
                NIO_PRINT("PA%0d SUBP%0d CH%0d is enabled", pa_idx, subp_idx, channel_idx);

                LwU8 dword_idx;
                for (dword_idx = 0; dword_idx < DWORDS_IN_CHANNEL; dword_idx++) {
                    //Get the address for this pa/subp/channel/dword
                    //Note: subp is account for in get_flipped_channel so it isn't in the callwlations below
                    LwU32 hbm_ib_ddll_dq_addr = hbm_ib_ddll_dq_base +
                                                (pa_idx << 14) +
                                                //(subp_idx * 4 * 10 * CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL) + //4 bytes/reg * 10 regs/dword * 2 dw/channel * 2 channel/subp
                                                (get_flipped_channel(pa_idx, subp_idx, channel_idx) * 4 * 10 * DWORDS_IN_CHANNEL) +
                                                (get_flipped_dword(pa_idx, dword_idx) * 4 * 10);

                    //Registers are grouped as WL_DDLL, TX_DDLL, RX_DDLL per channel/dword
                    //so add 12 to jump between RX_DDLL registers
                    LwU32 hbm_rx_ddll_addr = hbm_rx_ddll_base +
                                             (pa_idx << 14) +
                                             //(subp_idx    * 48) + //48 = 2 channel/subp (2 ch * 2 DW * 12)
                                             (get_flipped_channel(pa_idx, subp_idx, channel_idx) * 24) + //24 = 2 DW/channel (2 DW * 12)
                                             (get_flipped_dword(pa_idx, dword_idx) * 12);

                    //Get the base pin index for this PA/SUBP/CH/DW
                    LwU16 per_pin_index_dq  = (pa_idx * SUBPS * DQ_BITS_IN_SUBP) +
                                              (subp_idx       * DQ_BITS_IN_SUBP) +
                                              (channel_idx    * DQ_BITS_IN_CHANNEL) +
                                              (dword_idx      * DQ_BITS_IN_DWORD);
                    LwU16 per_pin_index_dbi = (pa_idx * SUBPS * DBI_BITS_IN_SUBP) +
                                              (subp_idx       * DBI_BITS_IN_SUBP) +
                                              (channel_idx    * DBI_BITS_IN_CHANNEL) +
                                              (dword_idx      * DBI_BITS_IN_DWORD);
                    LwU16 per_pin_index_ecc = (pa_idx * SUBPS * ECC_BITS_IN_SUBP) +
                                              (subp_idx       * ECC_BITS_IN_SUBP) +
                                              (channel_idx    * ECC_BITS_IN_CHANNEL) +
                                              (dword_idx      * ECC_BITS_IN_DWORD);

                    //Before we can program the individual per-pin delays, we need to find the latest
                    //DQ/DBI/ECC in this DWORD. Note that since we previously moved RDQS to the
                    //latest DQ/DBI/ECC, the pin with the SMALLEST eye center delay is the latest
                    //(the others will have to move more to line up with RDQS since they came earlier).
                    LwU16 latest_eye_center = 0xFFFF;
                    LwU8 pin_idx;
                    //Skip shifting the RX DDLL backwards if the flag is set. Don't remove the common delay.
                    if(!bFlagSkipRdqsDelayAdj) {
                        for (pin_idx = 0; pin_idx < DQ_BITS_IN_DWORD; pin_idx++) {
                            LwU16 lwrrent_dq_eye_center = find_eye_center(&best_eye_per_dq[(per_pin_index_dq + pin_idx)], &all_pins_passing);
                            if ((lwrrent_dq_eye_center < latest_eye_center) && (lwrrent_dq_eye_center != 0xFFFF)) {
                                latest_eye_center = lwrrent_dq_eye_center;
                            }
                        }
                        if (bFlagDbiEnRd) {
                            for (pin_idx = 0; pin_idx < DBI_BITS_IN_DWORD; pin_idx++) {
                                LwU16 lwrrent_dbi_eye_center = find_eye_center(&best_eye_per_dbi[(per_pin_index_dbi + pin_idx)], &all_pins_passing);
                                if ((lwrrent_dbi_eye_center < latest_eye_center) && (lwrrent_dbi_eye_center != 0xFFFF)) {
                                    latest_eye_center = lwrrent_dbi_eye_center;
                                }
                            }
                        }
                        if (bFlagEccEn) {
                            for (pin_idx = 0; pin_idx < ECC_BITS_IN_DWORD; pin_idx++) {
                                LwU16 lwrrent_ecc_eye_center = find_eye_center(&best_eye_per_ecc[(per_pin_index_ecc + pin_idx)], &all_pins_passing);
                                if ((lwrrent_ecc_eye_center < latest_eye_center) && (lwrrent_ecc_eye_center != 0xFFFF)) {
                                    latest_eye_center = lwrrent_ecc_eye_center;
                                }
                            }
                        }
                    } else {
                        latest_eye_center = 0;
                    }

                    //Program RX DDLL (RDQS) to move backwards to the latest eye center or 0
                    //if it can't move back that far
                    LwU32 rx_ddll_data = REG_RD32(BAR0, hbm_rx_ddll_addr);
                    LwU16 rx_ddll_trim = REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL_TRIM, rx_ddll_data);
                    NIO_PRINT("PA%0d SUBP%0d CH%0d DW%0d latest eye center = %0d, rx_ddll_trim = %0d", pa_idx, subp_idx, channel_idx, dword_idx, latest_eye_center, rx_ddll_trim);
                    if (latest_eye_center > rx_ddll_trim) {
                        latest_eye_center = rx_ddll_trim;
                        NIO_PRINT("PA%0d SUBP%0d CH%0d DW%0d Cannot move RDQS < 0. Updating latest eye center = %0d, rx_ddll_trim = 0", pa_idx, subp_idx, channel_idx, dword_idx, latest_eye_center);
                    }
                    rx_ddll_data = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL, _TRIM, (rx_ddll_trim - latest_eye_center), rx_ddll_data);
                    REG_WR32(LOG, hbm_rx_ddll_addr, rx_ddll_data);

                    //DQ bits are programmed in groups of 4
                    for (pin_idx = 0; pin_idx < DQ_BITS_IN_DWORD; pin_idx += 4) {
                        LwU32 hbm_ib_ddll_reg = 0;

                        //Build up the 4 lane settings into a single register value, then write it
                        LwU8 lane_idx;
                        for (lane_idx = 0; lane_idx < 4; lane_idx++) {
                            LwU16 lwrr_pin_center = find_eye_center(&best_eye_per_dq[per_pin_index_dq], &all_pins_passing);
                            if (lwrr_pin_center != 0xFFFF) {
                                lwrr_pin_center = lwrr_pin_center - latest_eye_center;
                                lwrr_pin_center = add_offset(lwrr_pin_center,TRAINING_DIR_READ);
                            } else {
                              NIO_PRINT("UCODE DEBUG: RD TRAINING: No eye found for PA%0d SUBP%0d CH%0d DW%0d pin %0d (DQ%0d)", pa_idx, subp_idx, channel_idx, dword_idx, per_pin_index_dq, (per_pin_index_dq % (DQ_BITS_IN_SUBP * SUBPS)));
                              rd_dq_pass_per_dword[per_pin_index_dq/DQ_BITS_IN_DWORD] = LW_FALSE;
                              rd_dq_training_pass_per_pa &= ~(1 << pa_idx);
                            }
                            hbm_ib_ddll_reg |= REF_VAL(7:0, lwrr_pin_center) << (8*lane_idx);
                            per_pin_index_dq++;
                        }
                        REG_WR32(LOG, hbm_ib_ddll_dq_addr, hbm_ib_ddll_reg);
                        hbm_ib_ddll_dq_addr += 4; //Move to the next register
                    }

                    //After the series of registers for the DQ bits, the next register is for the DBI bits
                    if (bFlagDbiEnRd) {
                        //Build up the 4 lane settings into a single register value, then write it
                        LwU32 hbm_ib_ddll_reg = 0;
                        for (pin_idx = 0; pin_idx < 4; pin_idx++) {
                            LwU16 lwrr_pin_center = find_eye_center(&best_eye_per_dbi[per_pin_index_dbi], &all_pins_passing);
                            if (lwrr_pin_center != 0xFFFF) {
                                lwrr_pin_center = lwrr_pin_center - latest_eye_center;
                                lwrr_pin_center = add_offset(lwrr_pin_center,TRAINING_DIR_READ);
                            } else {
                              NIO_PRINT("UCODE DEBUG: RD TRAINING: No eye found for PA%0d SUBP%0d CH%0d DW%0d pin %0d (DBI%0d)", pa_idx, subp_idx, channel_idx, dword_idx, per_pin_index_dbi, (per_pin_index_dbi % (DBI_BITS_IN_SUBP * SUBPS)));
                              rd_dq_pass_per_dword[per_pin_index_dbi/DBI_BITS_IN_DWORD] = LW_FALSE;
                              rd_dq_training_pass_per_pa &= ~(1 << pa_idx);
                            }
                            hbm_ib_ddll_reg |= REF_VAL(7:0, lwrr_pin_center) << (8*pin_idx);
                            per_pin_index_dbi++;
                        }
                        REG_WR32(LOG, hbm_ib_ddll_dq_addr, hbm_ib_ddll_reg);
                    }

                    //Move to the next register
                    //Do this outside the DBI enable check since whether or not DBI is enabled,
                    //we still need to move to the next
                    hbm_ib_ddll_dq_addr += 4;

                    //Finally do the ECC bits
                    if (bFlagEccEn) {
                        //Build up the 2 lane settings into a single register value, then write it
                        LwU32 hbm_ib_ddll_reg = REG_RD32(BAR0, hbm_ib_ddll_dq_addr);
                        hbm_ib_ddll_reg &= 0xFFFF0000; //Zero out the ECC delays before programming

                        for (pin_idx = 0; pin_idx < 2; pin_idx++) {
                            LwU16 lwrr_pin_center = find_eye_center(&best_eye_per_ecc[per_pin_index_ecc], &all_pins_passing);
                            if (lwrr_pin_center != 0xFFFF) {
                                lwrr_pin_center = lwrr_pin_center - latest_eye_center;
                                lwrr_pin_center = add_offset(lwrr_pin_center,TRAINING_DIR_READ);
                            } else {
                              NIO_PRINT("UCODE DEBUG: RD TRAINING: No eye found for PA%0d SUBP%0d CH%0d DW%0d pin %0d (ECC%0d)", pa_idx, subp_idx, channel_idx, dword_idx, per_pin_index_ecc, (per_pin_index_ecc % (ECC_BITS_IN_SUBP * SUBPS)));
                              rd_dq_pass_per_dword[per_pin_index_ecc/ECC_BITS_IN_DWORD] = LW_FALSE;
                              rd_dq_training_pass_per_pa &= ~(1 << pa_idx);
                            }
                            hbm_ib_ddll_reg |= REF_VAL(7:0, lwrr_pin_center) << (8*pin_idx);
                            per_pin_index_ecc++;
                        }
                        REG_WR32(LOG, hbm_ib_ddll_dq_addr, hbm_ib_ddll_reg);
                    }
                }
            }
        }
    }

    //If any pin failed (had no eye) stop training here and report an error
    if (!all_pins_passing && !bFlagAteNoFail) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_TRAINING_ERROR);
    }
}

void program_final_ob_delays(
    LwBool dbi_only
)
{
    NIO_PRINT("Entering program_final_ob_delays - dbi_only=%0d", dbi_only);

    //Base address for the first OB DDLL and BRL registers
    //For DDLL there are 10 registers per DWORD :: REG0-7 cover 32 DQ bits, REG8 covers 4 DBI bits, REG9 covers 2 ECC bits + 2 SEV bits
    //For BRL there are 5 registers per DWORD :: REG0-3 cover 32 DQ bits, REG4 covers DBI + ECC + SEV
    LwU32 hbm_ob_ddll_dq_base = 0x00901700;
    LwU32 hbm_ob_brl_dq_base  = 0x00901980;

    //Keep track of whether any pin has no eye
    LwBool all_pins_passing = LW_TRUE;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            LwU8 channel_idx;
            for (channel_idx = 0; channel_idx < CHANNELS_IN_SUBP; channel_idx++) {
                //If the L2 slice for this channel is disabled, don't program final values
                if (isL2SliceDisabled(pa_idx, subp_idx, channel_idx)) {
                    continue;
                }
                NIO_PRINT("PA%0d SUBP%0d CH%0d is enabled", pa_idx, subp_idx, channel_idx);

                LwU8 dword_idx;
                for (dword_idx = 0; dword_idx < DWORDS_IN_CHANNEL; dword_idx++) {
                    //Get the address for this pa/subp/channel/dword
                    //Note: subp is account for in get_flipped_channel so it isn't in the callwlations below
                    LwU32 hbm_ob_ddll_dq_addr = hbm_ob_ddll_dq_base +
                                                (pa_idx << 14) +
                                                //(subp_idx * 4 * 10 * CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL) + //4 bytes/reg * 10 regs/dword * 2 dw/channel * 2 channel/subp
                                                (get_flipped_channel(pa_idx, subp_idx, channel_idx) * 4 * 10 * DWORDS_IN_CHANNEL) +
                                                (get_flipped_dword(pa_idx, dword_idx) * 4 * 10);
                    LwU32 hbm_ob_brl_dq_addr  = hbm_ob_brl_dq_base +
                                                (pa_idx << 14) +
                                                //(subp_idx * 4 * 5 * CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL) +  //4 bytes/reg * 5 regs/dword * 2 dw/channel * 2 channel/subp
                                                (get_flipped_channel(pa_idx, subp_idx, channel_idx) * 4 * 5 * DWORDS_IN_CHANNEL) +
                                                (get_flipped_dword(pa_idx, dword_idx) * 4 * 5);

                    //DQ bits are programmed in groups of 4 for DDLL and 8 for BRL
                    LwU16 per_pin_index = (pa_idx * SUBPS * DQ_BITS_IN_SUBP) +
                                          (subp_idx       * DQ_BITS_IN_SUBP) +
                                          (channel_idx    * DQ_BITS_IN_CHANNEL) +
                                          (dword_idx      * DQ_BITS_IN_DWORD);

                    LwU32 hbm_ob_brl_reg = 0;
                    LwU32 hbm_ob_ddll_reg = 0;

                    //Since there are 8 DQs per BRL register but only 4 per DDLL, we
                    //need a flag to determine whether we are working on the upper or
                    //lower half.
                    //Start true since we flip it at the beginning of each loop iteration
                    LwBool writing_upper_brl = LW_TRUE;

                    if (!dbi_only) {
                        LwU8 dq_idx;
                        for (dq_idx = 0; dq_idx < DQ_BITS_IN_DWORD; dq_idx += 4) {
                            writing_upper_brl = !writing_upper_brl; //Flip the upper brl indicator
                            hbm_ob_ddll_reg = 0;
                            if (!writing_upper_brl) {
                                //We're starting a new BRL reg so reset the value
                                hbm_ob_brl_reg = 0;
                            }

                            //Build up the 4 DDLL settings into a single register value, then write it
                            LwU8 lane_idx;
                            for (lane_idx = 0; lane_idx < 4; lane_idx++) {
                                LwU16 eye_center = find_eye_center(&best_eye_per_dq[per_pin_index], &all_pins_passing);
                                if (eye_center == 0xFFFF) {
                                  NIO_PRINT("UCODE DEBUG: WR TRAINING: No eye found for PA%0d SUBP%0d CH%0d DW%0d pin %0d (DQ%0d)", pa_idx, subp_idx, channel_idx, dword_idx, per_pin_index, (per_pin_index % (DQ_BITS_IN_SUBP * SUBPS)));
                                  wr_dq_pass_per_dword[per_pin_index/DQ_BITS_IN_DWORD] = LW_FALSE;
                                  wr_dq_training_pass_per_pa &= ~(1 << pa_idx);
                                } else {
                                  eye_center = add_offset(eye_center,TRAINING_DIR_WRITE);
                                }
                                hbm_ob_ddll_reg |= REF_VAL(5:0, eye_center) << (8*lane_idx);
                                hbm_ob_brl_reg  |= REF_VAL(9:6, eye_center) << ((16*writing_upper_brl) + 4*lane_idx);
                                per_pin_index++;
                            }
                            REG_WR32(LOG, hbm_ob_ddll_dq_addr, hbm_ob_ddll_reg);
                            hbm_ob_ddll_dq_addr += 4; //Move to the next register

                            //Only write to the BRL regs every 8 DQ
                            if (writing_upper_brl) {
                                REG_WR32(LOG, hbm_ob_brl_dq_addr, hbm_ob_brl_reg);
                                hbm_ob_brl_dq_addr += 4; //Move to the next register
                            }
                        }
                    }
                    else {
                        //Move the register pointers to the DBI reg
                        hbm_ob_ddll_dq_addr += (4 * DQ_BITS_IN_DWORD/4);
                        hbm_ob_brl_dq_addr +=  (4 * DQ_BITS_IN_DWORD/8);
                    }


                    //Since DBI and ECC are contained in the same reg and we want to avoid
                    //overwriting good value, need to read the register first
                    //The final write will be done after the DBI and ECC loops
                    hbm_ob_brl_reg = REG_RD32(BAR0, hbm_ob_brl_dq_addr);

                    //After the series of registers for the DQ bits, the next register is for the DBI bits
                    if (bFlagDbiEnWr && dbi_only) {
                        per_pin_index = (pa_idx * SUBPS * DBI_BITS_IN_SUBP) +
                                        (subp_idx       * DBI_BITS_IN_SUBP) +
                                        (channel_idx    * DBI_BITS_IN_CHANNEL) +
                                        (dword_idx      * DBI_BITS_IN_DWORD);

                        //Build up the 4 lane settings into a single register value, then write it
                        hbm_ob_ddll_reg = 0;
                        hbm_ob_brl_reg &= 0xFFFF0000; //Clear the DBI settings only
                        LwU8 lane_idx;
                        for (lane_idx = 0; lane_idx < 4; lane_idx++) {
                            LwU16 eye_center = find_eye_center(&best_eye_per_dbi[per_pin_index], &all_pins_passing);
                            if (eye_center == 0xFFFF) {
                              NIO_PRINT("UCODE DEBUG: WR TRAINING: No eye found for PA%0d SUBP%0d CH%0d DW%0d pin %0d (DBI%0d)", pa_idx, subp_idx, channel_idx, dword_idx, per_pin_index, (per_pin_index % (DBI_BITS_IN_SUBP * SUBPS)));
                              wr_dbi_pass_per_dword[per_pin_index/DBI_BITS_IN_DWORD] = LW_FALSE;
                              wr_dbi_training_pass_per_pa &= ~(1 << pa_idx);
                            } else {
                              eye_center = add_offset(eye_center,TRAINING_DIR_WRITE);
                            }
                            hbm_ob_ddll_reg |= REF_VAL(5:0, eye_center) << (8*lane_idx);
                            hbm_ob_brl_reg  |= REF_VAL(9:6, eye_center) << (4*lane_idx);
                            per_pin_index++;
                        }
                        REG_WR32(LOG, hbm_ob_ddll_dq_addr, hbm_ob_ddll_reg);
                    }

                    //Move to the next register
                    //Do this outside the DBI enable check since whether or not DBI is enabled,
                    //we still need to move to the next
                    hbm_ob_ddll_dq_addr += 4;

                    //Finally do the ECC bits
                    if (bFlagEccEn && !dbi_only) {
                        per_pin_index = (pa_idx * SUBPS * ECC_BITS_IN_SUBP) +
                                        (subp_idx       * ECC_BITS_IN_SUBP) +
                                        (channel_idx    * ECC_BITS_IN_CHANNEL) +
                                        (dword_idx      * ECC_BITS_IN_DWORD);
                        //Build up the 2 lane settings into a single register value, then write it
                        hbm_ob_ddll_reg = REG_RD32(BAR0, hbm_ob_ddll_dq_addr);
                        hbm_ob_ddll_reg &= 0xFFFF0000; //Clear the ECC settings only
                        hbm_ob_brl_reg &= 0xFF00FFFF; //Clear the ECC settings only
                        LwU8 lane_idx;
                        for (lane_idx = 0; lane_idx < 2; lane_idx++) {
                            LwU16 eye_center = find_eye_center(&best_eye_per_ecc[per_pin_index], &all_pins_passing);
                            if (eye_center == 0xFFFF) {
                              NIO_PRINT("UCODE DEBUG: WR TRAINING: No eye found for PA%0d SUBP%0d CH%0d DW%0d pin %0d (ECC%0d)", pa_idx, subp_idx, channel_idx, dword_idx, per_pin_index, (per_pin_index % (ECC_BITS_IN_SUBP * SUBPS)));
                              wr_dq_pass_per_dword[per_pin_index/ECC_BITS_IN_DWORD] = LW_FALSE;
                              wr_dq_training_pass_per_pa &= ~(1 << pa_idx);
                            } else {
                              eye_center = add_offset(eye_center,TRAINING_DIR_WRITE);
                            }
                            hbm_ob_ddll_reg |= REF_VAL(5:0, eye_center) << (8*lane_idx);
                            hbm_ob_brl_reg  |= REF_VAL(9:6, eye_center) << (16 + 4*lane_idx);
                            per_pin_index++;
                        }
                        REG_WR32(LOG, hbm_ob_ddll_dq_addr, hbm_ob_ddll_reg);
                    }

                    //Write the BRL reg now that it has both DBI and ECC data
                    REG_WR32(LOG, hbm_ob_brl_dq_addr, hbm_ob_brl_reg);
                }
            }
        }
    }

    //If any pin failed (had no eye) stop training here and report an error
    if (!all_pins_passing && !bFlagAteNoFail) {
        FW_MBOX_WR32(13, dbi_only); //Help differentiate the training step
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_TRAINING_ERROR);
    }
}

//=============================================================================
//WDQS Write Leveling Functions
//=============================================================================
void initialize_dca_arrays(void)
{
    NIO_PRINT("Entering initialize_dca_arrays");
    memset(dca_setting_per_wdqs, 0, sizeof(dca_setting_per_wdqs));
}

void initialize_wdqs_arrays(void)
{
    NIO_PRINT("Entering initialize_wdqs_arrays");
    memset(early_late_transition_per_wdqs, 0, sizeof(early_late_transition_per_wdqs));
    memset(prev_early_late_per_wdqs, 0 , sizeof(prev_early_late_per_wdqs));
}

//DCA Functions
void set_hbm_dcm(
    LwBool enable
)
{
    NIO_PRINT("Entering set_hbm_dcm - enable=%0d", enable);

    LwU32 mrs8 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS8);
    mrs8 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8_ADR, _HBM3_DCM, enable, mrs8);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS8, mrs8);
}

void read_derr_status_dca (
    LwU8 dca_setting
)
{
    NIO_PRINT("Entering read_derr_status_dca - dca_setting=%0d", dca_setting);

    //Base address for the DERR register - There is only one per PA
    LwU32 hbm_dq_derr_base = 0x00901ab4;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 hbm_dq_derr_addr = hbm_dq_derr_base + (pa_idx << 14);

        //Callwlate the index into the per-wdqs arrays
        LwU16 wdqs_idx = pa_idx * DQS_BITS_IN_PA;

        //Read the DERR status and update each WDQS for this PA
        LwU32 derr_data = REG_RD32(BAR0, hbm_dq_derr_addr);

        LwU8 i;
        for (i = 0; i < DQS_BITS_IN_PA; i++) {
            LwU8 lwrr_dcm_result = (derr_data >> (4*i)) & 0x1; //DERR is every 4 bits of the register

            //If this WDQS is already trained there's no need to keep tracking
            if (dca_setting_per_wdqs[(wdqs_idx+i)].training_complete == 1) {
                continue;
            }

            if ((dca_setting_per_wdqs[(wdqs_idx+i)].prev_dcm_result != lwrr_dcm_result) &&
                (dca_setting != DCA_NEG_7_SETTING)) {
                //Saw a transition from < 50% to >= 50% duty cycle. This is the optimal setting for this WDQS
                dca_setting_per_wdqs[(wdqs_idx+i)].best_dca_setting = dca_setting;
                dca_setting_per_wdqs[(wdqs_idx+i)].training_complete = 1;
                NIO_PRINT("Saw DCM transition from < 50%% to >= 50%% on PA%0d WDQS%0d at dca_setting=%s%0d", pa_idx, i, ((dca_setting < 8) ? "-" : ""), (dca_setting & 0x7));
            }
            else if ((dca_setting == DCA_NEG_7_SETTING) && (lwrr_dcm_result == 1)) {
                //Special case -- We have DCA = -7 and duty cycle is still > 50%. Mark this WDQS as done
                dca_setting_per_wdqs[(wdqs_idx+i)].best_dca_setting = dca_setting;
                dca_setting_per_wdqs[(wdqs_idx+i)].training_complete = 1;
                NIO_PRINT("At min setting (-7) DCM reports > 50%% duty cycle on PA%0d WDQS%0d", pa_idx, i);
            }
            else if ((dca_setting == DCA_PLUS_7_SETTING) && (lwrr_dcm_result == 0)) {
                //Special case -- We have DCA = +7 and duty cycle is still < 50%. Mark this WDQS as done
                dca_setting_per_wdqs[(wdqs_idx+i)].best_dca_setting = dca_setting;
                dca_setting_per_wdqs[(wdqs_idx+i)].training_complete = 1;
                NIO_PRINT("At max setting (+7) DCM reports < 50%% duty cycle on PA%0d WDQS%0d", pa_idx, i);
            }


            dca_setting_per_wdqs[(wdqs_idx+i)].prev_dcm_result = lwrr_dcm_result;
        }
    }
}

void program_final_dca_settings(void)
{
    NIO_PRINT("Entering program_final_dca_settings");

    //Base address for MR11
    LwU32 fbpa_mr11_base_addr = 0x00902204;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 mr11_addr = fbpa_mr11_base_addr + (pa_idx << 14);

        LwU8 subp_idx;
        for (subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            LwU8 channel_idx;
            for (channel_idx = 0; channel_idx < CHANNELS_IN_SUBP; channel_idx++) {
                //Don't program floorswept slices
                if (isL2SliceDisabled(pa_idx, subp_idx, channel_idx)) {
                    continue;
                }

                LwU32 mr11_data = 0x00B00000; //Set BA=11

                //We want to mask off the other subp and channel. Since the masks are 1 hot, put a
                //1 in the bit position of the other subp/channel
                mr11_data = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS11, _SUBP_MASK, (1 << (subp_idx    ^ 0x1)), mr11_data);
                mr11_data = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS11, _CH_MASK,   (1 << (channel_idx ^ 0x1)), mr11_data);

                LwU8 dword_idx;
                for (dword_idx = 0; dword_idx < DWORDS_IN_CHANNEL; dword_idx++) {
                    LwU16 wdqs_idx = (((DQ_BITS_IN_SUBP*SUBPS) / DQ_BITS_IN_DWORD) * pa_idx) +
                                     //(CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL * subp_idx) +
                                     (DWORDS_IN_CHANNEL * get_flipped_channel(pa_idx, subp_idx, channel_idx)) +
                                     get_flipped_dword(pa_idx, dword_idx);

                    //PC0 is bits [3:0] and PC1 is bits [7:4]
                    //Need to account for DWORD flip ourselves since PA doesn't handle MR11 specially
                    mr11_data |= ((dca_setting_per_wdqs[wdqs_idx].best_dca_setting & 0xF) << (4*get_flipped_dword(pa_idx, dword_idx)));
                }

                //Write the final DCA value
                REG_WR32(LOG, mr11_addr, mr11_data);
            }
        }
    }
}


//Write Leveling Functions
void set_hbm_write_leveling(
    LwBool enable
)
{
    NIO_PRINT("Entering set_hbm_write_leveling - enable=%0d", enable);

    LwU32 mrs8 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS8);
    mrs8 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8_ADR, _HBM3_WDQS2CK, enable, mrs8);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS8, mrs8);
}

LwU16 read_fbio_wdqs_delay(void)
{
    LwU16 wdqs_initial_delay = 0;

    //Base address for the first TX_DDLL register and the overall barrel shifter register
    //Since we just want a single delay setting (and assume it's the same for all), it
    //doesn't matter which subp/channel/dword we grab
    LwU32 hbm_tx_ddll_base = 0x009010dc;
    LwU32 hbm_ob_brl_dq_wdqs_base = 0x00901ab0;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 hbm_ob_brl_dq_wdqs_addr = hbm_ob_brl_dq_wdqs_base + (pa_idx << 14);
        LwU32 hbm_ob_brl_dq_wdqs_data = REG_RD32(BAR0, hbm_ob_brl_dq_wdqs_addr);

        LwU32 hbm_tx_ddll_addr = hbm_tx_ddll_base + (pa_idx << 14);
        LwU32 hbm_tx_ddll_data = REG_RD32(BAR0, hbm_tx_ddll_addr);

        wdqs_initial_delay = (REF_VAL(LW_PFB_FBPA_FBIO_HBM_OB_BRL_DQ_WDQS_CHANNEL0_DWORD0, hbm_ob_brl_dq_wdqs_data) << 6) |
                             (REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL_TRIM, hbm_tx_ddll_data) & 0x3F);

        //Exit the loop since we found an active partition and assume all values are the same
        break;
    }

    //Catch errors early if we can't read the delay
    if (wdqs_initial_delay == 0) {
        NIO_PRINT("Error: Unable to determine initial WDQS delay");
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WCK_TRAINING_ERROR);
    }

    return wdqs_initial_delay;
}

void set_fbio_wdqs_ovrd(
    LwBool enable
)
{
    LwU32 fbio_hbm_wdqs_cfg = saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg ;
    if (enable == LW_TRUE) {
      fbio_hbm_wdqs_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_WDQS_CFG, _OVRD_PATTERN, 0x5,    fbio_hbm_wdqs_cfg);
    } else {
      fbio_hbm_wdqs_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_WDQS_CFG, _OVRD_PATTERN, 0x0,    fbio_hbm_wdqs_cfg);
    }
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_WDQS_CFG, fbio_hbm_wdqs_cfg);
}

void program_fbio_wdqs_delay_broadcast(
    LwU16 wdqs_delay,
    LwU16 wdqs_delay_prev
)
{
    NIO_PRINT("Entering program_fbio_wdqs_delay_broadcast - delay=%0d, delay_prev=%0d", wdqs_delay, wdqs_delay_prev);

    LwU8 wdqs_brl_prev = REF_VAL(9:6, wdqs_delay_prev);
    LwU8 wdqs_brl      = REF_VAL(9:6, wdqs_delay);
    LwU8 wdqs_trim     = REF_VAL(5:0, wdqs_delay);

    //Only write BRL if it changed
    if (wdqs_brl != wdqs_brl_prev) {
        LwU32 hbm_ob_brl_dq_wdqs = 0;
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL0_DWORD0, wdqs_brl, hbm_ob_brl_dq_wdqs);
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL0_DWORD1, wdqs_brl, hbm_ob_brl_dq_wdqs);
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL1_DWORD0, wdqs_brl, hbm_ob_brl_dq_wdqs);
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL1_DWORD1, wdqs_brl, hbm_ob_brl_dq_wdqs);
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL2_DWORD0, wdqs_brl, hbm_ob_brl_dq_wdqs);
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL2_DWORD1, wdqs_brl, hbm_ob_brl_dq_wdqs);
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL3_DWORD0, wdqs_brl, hbm_ob_brl_dq_wdqs);
        hbm_ob_brl_dq_wdqs = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, _CHANNEL3_DWORD1, wdqs_brl, hbm_ob_brl_dq_wdqs);
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_OB_BRL_DQ_WDQS, hbm_ob_brl_dq_wdqs);
    }

    //Update the trimmer setting
    saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_TRIM, wdqs_trim, saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc0);
}

void read_derr_status_wrlvl (
    LwU16 wdqs_delay,
    LwU16 wdqs_start_delay
)
{
    NIO_PRINT("Entering read_derr_status_wrlvl - delay=%0d", wdqs_delay);

    //Base address for the DERR register - There is only one per PA
    LwU32 hbm_dq_derr_base = 0x00901ab4;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 hbm_dq_derr_addr = hbm_dq_derr_base + (pa_idx << 14);

        //Callwlate the index into the per-wdqs arrays
        LwU16 wdqs_idx = pa_idx * DQS_BITS_IN_PA;

        //In this first loop, just sample DERR as many times as defined
        //We can OR together the samples because if any 1s are seen we want to
        //count that as high. That way there are no false 1->0 transitions. In order
        //to have a 1->0 transition all of the samples must be 0.
        LwU8 sample_num;
        for (sample_num = 0; sample_num < wr_lvl_derr_samples; sample_num++) {
            //Read the DERR status and update each WDQS for this PA
            LwU32 derr_data = REG_RD32(BAR0, hbm_dq_derr_addr);
            LwU8 i;
            for (i = 0; i < DQS_BITS_IN_PA; i++) {
                LwU8 wdqs_is_early = (derr_data >> (4*i)) & 0x1; //DERR is every 4 bits of the register
                if (sample_num == 0) {
                    prev_early_late_per_wdqs[(wdqs_idx+i)].derr_samples = wdqs_is_early;
                }
                else {
                    prev_early_late_per_wdqs[(wdqs_idx+i)].derr_samples |= wdqs_is_early;
                }
            }
        }

        //Now that sampling is complete, determine if we saw a transition from 1->0
        LwU8 i;
        for (i = 0; i < DQS_BITS_IN_PA; i++) {
            if ((prev_early_late_per_wdqs[(wdqs_idx+i)].wdqs_is_early == 1) &&
                (prev_early_late_per_wdqs[(wdqs_idx+i)].early_late_seen == 0) &&
                (prev_early_late_per_wdqs[(wdqs_idx+i)].derr_samples == 0)) {
                //We saw the 1->0 transition - Save this delay and mark prev_early_late_per_wdqs.early_late_seen
                early_late_transition_per_wdqs[(wdqs_idx+i)] = wdqs_delay;
                prev_early_late_per_wdqs[(wdqs_idx+i)].early_late_seen = 1;
                NIO_PRINT("Saw early->late transition on PA%0d WDQS%0d at delay=%0d", pa_idx, i, wdqs_delay);
            }

            //Special case to help track when the early->late transition oclwrs when going from
            //max delay -> 0. The above won't detect the transition from 1->0 since we don't loop
            //back around.
            //[bswamy] Do we want this? If the sweep range is limited to +/-1 UI then this situation should
            //never occur. I'm disabling it for now, but leaving the code in case we need it.
            /*
            if ((wdqs_delay == wdqs_start_delay) && (prev_early_late_per_wdqs[(wdqs_idx+i)].derr_samples == 0)) {
              prev_early_late_per_wdqs[(wdqs_idx+i)].first_setting_late = 1;
            }
            */

            if ((prev_early_late_per_wdqs[(wdqs_idx+i)].first_setting_late == 1) && (prev_early_late_per_wdqs[(wdqs_idx+i)].derr_samples == 1) &&
                (wdqs_delay == HBM_TRAINING_WDQS_MAX_DELAY)) {
                early_late_transition_per_wdqs[(wdqs_idx+i)] = 0;
                prev_early_late_per_wdqs[(wdqs_idx+i)].early_late_seen = 1;
                NIO_PRINT("Saw early->late transition on PA%0d WDQS%0d at delay=%0d", pa_idx, i, 0);
            }

            //Save previous wdqs_is_early state
            prev_early_late_per_wdqs[(wdqs_idx+i)].wdqs_is_early = prev_early_late_per_wdqs[(wdqs_idx+i)].derr_samples;
        }
    }
}

void program_final_wdqs_delays(void)
{
    NIO_PRINT("Entering program_final_wdqs_delays");

    //Base address for the first TX_DDLL register and the overall barrel shifter register
    //For TX_DDLL there are 8 registers total (Per CH (4), Per DW (2)). Between each TX_DDLL
    //registers is an RX_DDLL and a WL_DDLL register so add 12 to move between each TX_DDLL.
    LwU32 hbm_tx_ddll_base = 0x009010dc;
    LwU32 hbm_ob_brl_dq_wdqs_base = 0x00901ab0;

    //Keep track of whether any pin has no early->late transition
    LwBool all_pins_passing = LW_TRUE;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 hbm_ob_brl_dq_wdqs_addr = hbm_ob_brl_dq_wdqs_base + (pa_idx << 14);
        LwU32 hbm_ob_brl_dq_wdqs_data = 0;

        //Since WDQS uses a 1:1 mapping but floorsweeping takes flip into account, need
        //to figure out the actual subp and channel associated with each WDQS. Build a
        //quick LUT here
        LwU8 l2SliceDis = 0;
        LwU8 subp_idx;
        LwU8 channel_idx;
        LwU8 dword_idx;
        for (subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            for (channel_idx = 0; channel_idx < CHANNELS_IN_SUBP; channel_idx++) {
                for (dword_idx = 0; dword_idx < DWORDS_IN_CHANNEL; dword_idx++) {
                    LwU8 flipped_wdqs = (get_flipped_channel(pa_idx, subp_idx, channel_idx) * DWORDS_IN_CHANNEL) +
                                        get_flipped_dword(pa_idx, dword_idx);
                    LwU8 flipped_subp_idx = flipped_wdqs / (CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL);
                    LwU8 flipped_channel_idx = (flipped_wdqs % (CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL)) / DWORDS_IN_CHANNEL;
                    l2SliceDis |= (isL2SliceDisabled(pa_idx, flipped_subp_idx, flipped_channel_idx)) <<
                        (subp_idx*CHANNELS_IN_SUBP*DWORDS_IN_CHANNEL + channel_idx*DWORDS_IN_CHANNEL + dword_idx);
                }
            }
        }

        //Program the final values
        for (subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            for (channel_idx = 0; channel_idx < CHANNELS_IN_SUBP; channel_idx++) {
                for (dword_idx = 0; dword_idx < DWORDS_IN_CHANNEL; dword_idx++) {
                    if ((l2SliceDis >> (subp_idx*CHANNELS_IN_SUBP*DWORDS_IN_CHANNEL + channel_idx*DWORDS_IN_CHANNEL + dword_idx)) & 0x1) {
                        continue;
                    }

                    //Note: WDQS is a direct 1:1 mapping always, so no flipped channel/dword
                    LwU32 hbm_tx_ddll_addr = hbm_tx_ddll_base +
                                             (pa_idx << 14) +
                                             (subp_idx * 12 * CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL) +
                                             (channel_idx * 12 * DWORDS_IN_CHANNEL) +
                                             (dword_idx * 12);

                    LwU16 wdqs_idx = (((DQ_BITS_IN_SUBP*SUBPS) / DQ_BITS_IN_DWORD) * pa_idx) +
                                     (CHANNELS_IN_SUBP * DWORDS_IN_CHANNEL * subp_idx) +
                                     (DWORDS_IN_CHANNEL * channel_idx) +
                                     dword_idx;

                    //Make sure we found the early->late transition
                    //This is saved in bit [7] of prev_early_late_per_wdqs
                    if (prev_early_late_per_wdqs[wdqs_idx].early_late_seen != 0x1) {
                        NIO_PRINT("WDQS TR: No transition found for PA%0d SUBP%0d CH%0d DW%0d (WDQS%0d)", pa_idx, subp_idx, channel_idx, dword_idx, wdqs_idx);
                        all_pins_passing = LW_FALSE;
                        wdqs_pass_per_dword[wdqs_idx] = LW_FALSE;
                        wdqs_training_pass_per_pa &= ~(1 << pa_idx);
                    }

                    //The trimmer code is the lower 6 bits of the delay
                    LwU32 hbm_tx_ddll_data = REG_RD32(BAR0, hbm_tx_ddll_addr);
                    hbm_tx_ddll_data = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL, _TRIM, (early_late_transition_per_wdqs[wdqs_idx] & 0x3F), hbm_tx_ddll_data);
                    REG_WR32(LOG, hbm_tx_ddll_addr, hbm_tx_ddll_data);

                    //The BS value is bits [9:6] of the delay
                    hbm_ob_brl_dq_wdqs_data |= ((early_late_transition_per_wdqs[wdqs_idx] >> 6) & 0xF) <<
                                                (16 * subp_idx +
                                                 8  * channel_idx +
                                                 4  * dword_idx);
                }
            }
        }

        //Now that all channels and dwords are done for this PA, update the full ob_brl register
        REG_WR32(LOG, hbm_ob_brl_dq_wdqs_addr, hbm_ob_brl_dq_wdqs_data);
    }

    //If any pin failed (had no early->late transition) stop training here and report an error
    if (!all_pins_passing && !bFlagAteNoFail) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WCK_TRAINING_ERROR);
    }
}

//Simulation debug only - ensure that we are only writing to array elements for the active PA
//Since all arrays are initialized to 0 for all elements, we should expect to see all 0 values
//for any indices corresponding to a disable PA
#if (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
void checkDisabledPaDataArrays(void)
{
    LwU16 lwr_idx;
    LwU16 min_idx;
    LwU16 max_idx;
    LwU8 pa_idx;
    LwBool error_found = LW_FALSE;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Skip the active partition
        if (isPartitionEnabled(pa_idx)) { continue; }

        min_idx = (pa_idx * DQ_BITS_IN_SUBP * SUBPS);
        max_idx = min_idx + (DQ_BITS_IN_SUBP * SUBPS);
        for (lwr_idx = min_idx; lwr_idx < max_idx; lwr_idx++) {
            if (ppei_isNotEqual(lwrrent_eye_per_dq[lwr_idx],0)) {
                NIO_PRINT("Error: lwrrent_eye_per_dq[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                error_found = LW_TRUE;
            }
            if (ppei_isNotEqual(best_eye_per_dq[lwr_idx],0)) {
                NIO_PRINT("Error: best_eye_per_dq[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                error_found = LW_TRUE;
            }
        }

        min_idx = (pa_idx * DBI_BITS_IN_SUBP * SUBPS);
        max_idx = min_idx + (DBI_BITS_IN_SUBP * SUBPS);
        for (lwr_idx = min_idx; lwr_idx < max_idx; lwr_idx++) {
            if (ppei_isNotEqual(lwrrent_eye_per_dbi[lwr_idx],0)) {
                NIO_PRINT("Error: lwrrent_eye_per_dbi[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                error_found = LW_TRUE;
            }
            if (ppei_isNotEqual(best_eye_per_dbi[lwr_idx],0)) {
                NIO_PRINT("Error: best_eye_per_dbi[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                error_found = LW_TRUE;
            }
        }

        min_idx = (pa_idx * ECC_BITS_IN_SUBP * SUBPS);
        max_idx = min_idx + (ECC_BITS_IN_SUBP * SUBPS);
        for (lwr_idx = min_idx; lwr_idx < max_idx; lwr_idx++) {
            if (ppei_isNotEqual(lwrrent_eye_per_ecc[lwr_idx],0)) {
                NIO_PRINT("Error: lwrrent_eye_per_ecc[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                error_found = LW_TRUE;
            }
            if (ppei_isNotEqual(best_eye_per_ecc[lwr_idx],0)) {
                NIO_PRINT("Error: best_eye_per_ecc[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                error_found = LW_TRUE;
            }
        }
    }

    if (error_found) {
        NIO_ERROR("Found errors while checking arrays for disabled PAs");
    }
}

void checkDisabledPaDqsArrays(LwU8 dir)
{
    LwU16 lwr_idx;
    LwU16 min_idx;
    LwU16 max_idx;
    LwU8 pa_idx;
    LwBool error_found = LW_FALSE;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Skip the active partition
        if (isPartitionEnabled(pa_idx)) { continue; }

        min_idx = (pa_idx * DWORDS_IN_CHANNEL * CHANNELS_IN_SUBP * SUBPS);
        max_idx = min_idx + (DWORDS_IN_CHANNEL * CHANNELS_IN_SUBP * SUBPS);
        for (lwr_idx = min_idx; lwr_idx < max_idx; lwr_idx++) {
            if (dir == TRAINING_DIR_WRITE) {
                if (weli_isNotEqual(prev_early_late_per_wdqs[lwr_idx],0)) {
                    NIO_PRINT("Error: prev_early_late_per_wdqs[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                    error_found = LW_TRUE;
                }
                if (early_late_transition_per_wdqs[lwr_idx] != 0) {
                    NIO_PRINT("Error: early_late_transition_per_wdqs[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                    error_found = LW_TRUE;
                }
            }
            else {
                if (rdt_isNotEqual(first_pass_per_rdqs[lwr_idx],0)) {
                    NIO_PRINT("Error: first_pass_per_rdqs[%0d] != 0 for disabled PA%0d", lwr_idx, pa_idx);
                    error_found = LW_TRUE;
                }
            }
        }
    }

    if (error_found) {
        NIO_ERROR("Found errors while checking arrays for disabled PAs");
    }
}
#else
//Empty function definitions for Falcon -- these will get optimized out in compile
void checkDisabledPaDataArrays(void) {
}
void checkDisabledPaDqsArrays(LwU8 dir) {
}
#endif

LwU32 doBootTimeTraining (void)
{
    //Parse flags and settings from the boot training tables
    parseBootTrainingTable();

    //Read the fuses to find out which L2 slices are disabled
    read_l2_slice_disable_fuse();

    //Read the DEBUG2 register from each PA to get flip strap information
    read_flip_straps();

    //If we are running on ATE, mark all training status as pass by default
    if (bFlagAteNoFail) {
      initialize_status_arrays();
    }

    #if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
    //Determine DDR mode from FBIO broadcast
    saved_reg->pfb_fbpa_fbio_broadcast = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);
    gbl_hbm_mode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, saved_reg->pfb_fbpa_fbio_broadcast);
    #endif

    //Save register state before training. These registers will be restored when training completes
    //We do this before mclk switch because the patram load for read training will change these
    //Mclk switch shouldn't be touching these 3 registers anyway.
    saved_reg->pfb_fbpa_hbm_char = REG_RD32(BAR0, LW_PFB_FBPA_HBM_CHAR);
    saved_reg->pfb_fbpa_training_char_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_CTRL);
    saved_reg->pfb_fbpa_training_cmd = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CMD(1));

    //Write to FALCON_MONITOR to indicate to training monitors that training has started
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xB0070000);

    //If read training is enabled, load the patram patterns into memory now before switching to p0 frequency
    if (bFlagRdTrEn && bFlagRdLoadPatram) {
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_RD_TR_PATRAM_START, 0);

        osPTimerTimeNsLwrrentGet(&startTime);  //Starting point
        //Enable the CFG0_DRAM_ACK and disable ACPD during training. We will restore this at end.
        saved_reg->pfb_fbpa_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
        LwU32 updated_reg_val = saved_reg->pfb_fbpa_cfg0;
        updated_reg_val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK,  _ENABLED,      updated_reg_val);
        updated_reg_val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD, _NO_POWERDOWN, updated_reg_val);
        REG_WR32(LOG, LW_PFB_FBPA_CFG0, updated_reg_val);

        //Disable refsb during training and restore at the end
        saved_reg->pfb_fbpa_timing21 = REG_RD32(BAR0, LW_PFB_FBPA_TIMING21);
        updated_reg_val = saved_reg->pfb_fbpa_timing21;
        updated_reg_val = FLD_SET_DRF(_PFB, _FBPA_TIMING21, _REFSB, _DISABLED, updated_reg_val);
        REG_WR32(LOG, LW_PFB_FBPA_TIMING21, updated_reg_val);

        //Load patterns into the patram
        load_char_patram(TRAINING_DIR_READ);

        //Setup char for read training
        char_setup(TRAINING_DIR_READ, bFlagDbiEnRd);

        //For the write-only run to load patterns set loop count = 1
        LwU32 char_bank_ctrl2_wronly = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CHAR_BANK_CTRL2, _LOOP_CNT, 1, gTT.bootTrainingTable.READ_CHAR_BANK_CTRL2);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2, char_bank_ctrl2_wronly);

        //TODO Load trimmer/bs safe settings for OB (values from vbios)

        //Run write-only training to write the patterns from PATRAM->Memory Array
        //Since we are doing this at a safe/low frequency, no training is required.
        //The only purpose of this step is to get known patterns into the memory so
        //we can do read training at high frequency
        start_training(TRAINING_DIR_WRITE);
        wait_for_training_done();

        bFlagRdLoadPatram = LW_FALSE;

        //Restore register state before mclk switch
        REG_WR32(LOG, LW_PFB_FBPA_CFG0, saved_reg->pfb_fbpa_cfg0);
        REG_WR32(LOG, LW_PFB_FBPA_TIMING21, saved_reg->pfb_fbpa_timing21);
        wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX2
    }

    //=========================================================================
    //MCLK Switch to P0 for the rest of training
    //=========================================================================
    LwU32 p0Freq = REG_RD32(BAR0, (DRF_BASE(LW_XTL) + LW_XTL_EP_PRI_HOT_RESET_SCRATCH_1));
    if (p0Freq == 0)
    {
        p0Freq = gBiosTable.nominalFrequencyP0;
    }
    falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_PRE_MCLK_SWITCH, 0);
    osPTimerTimeNsLwrrentGet(&startTime);  //Starting point
    doMclkSwitch(p0Freq);
    wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX3
    falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_MCLK_SWITCH_COMPLETE, 0);
    osPTimerTimeNsLwrrentGet(&startTime);  //Starting point

    //=========================================================================
    //Training Config
    //Need to program these here since mclk switch may have modified them
    //All registers will be restored at the end of training
    //=========================================================================
    save_fbio_hbm_delay_broadcast_misc(&saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc0,
                &saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc1);

    //Enable CFG0_DRAM_ACK and disable ACPD during training.
    saved_reg->pfb_fbpa_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    LwU32 updated_reg_val = saved_reg->pfb_fbpa_cfg0;
    updated_reg_val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK,  _ENABLED,      updated_reg_val);
    updated_reg_val = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD, _NO_POWERDOWN, updated_reg_val);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, updated_reg_val);

    //Disable refsb during training and restore at the end
    saved_reg->pfb_fbpa_timing21 = REG_RD32(BAR0, LW_PFB_FBPA_TIMING21);
    updated_reg_val = saved_reg->pfb_fbpa_timing21;
    updated_reg_val = FLD_SET_DRF(_PFB, _FBPA_TIMING21, _REFSB, _DISABLED, updated_reg_val);
    REG_WR32(LOG, LW_PFB_FBPA_TIMING21, updated_reg_val);

    //Disable DDLLCAL clock gating during training. We will restore this at the end
    saved_reg->pfb_fbpa_fbio_hbm_ddllcal_ctrl1 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1);
    updated_reg_val = saved_reg->pfb_fbpa_fbio_hbm_ddllcal_ctrl1;
    updated_reg_val = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CLK_GATE, _OFF, updated_reg_val);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, updated_reg_val);

    //Check if periodic calibration is On.
    LwBool periodic_calibration_on;
    if (FLD_TEST_DRF(_PFB_FBPA_FBIO, _HBM_DDLLCAL_CTRL1, _DISABLE_PERIODIC_UPDATE, _INIT, saved_reg->pfb_fbpa_fbio_hbm_ddllcal_ctrl1))
    {
        periodic_calibration_on = LW_FALSE;
    }
    else
    {
        periodic_calibration_on = LW_TRUE;
    }

    //Disable FORCE_UPDATE for IB and OB delays until training completes. They will be enabled just before
    //final value programming
    saved_reg->pfb_fbio_hbm_trng_misc = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC);
    updated_reg_val = saved_reg->pfb_fbio_hbm_trng_misc;
    updated_reg_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_MISC, _IB_DQ_FORCE_PERIODIC_UPDATES, 0, updated_reg_val);
    updated_reg_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_MISC, _OB_DQ_FORCE_PERIODIC_UPDATES, 0, updated_reg_val);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC, updated_reg_val);

    //Clear any DQ/DBI/ECC masks on the traning broadcast registers
    //Set the mask for DBI and/or ECC if they are not enabled to avoid setup/hold
    LwU32 hbm_trng_broadcast_mask = 0;
    hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _CMD, 1, hbm_trng_broadcast_mask);
    hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_SEV, 1, hbm_trng_broadcast_mask);
    if (!bFlagDbiEnRd) {
      hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_DBI, 1, hbm_trng_broadcast_mask);
    }
    if (!bFlagEccEn) {
      hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_ECC, 1, hbm_trng_broadcast_mask);
    }
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, hbm_trng_broadcast_mask);

    //=========================================================================
    //Write Leveling
    //=========================================================================
    //Disable FBIO priv clock gating during DCA and write leveling, save reg
    if (bFlagDcaEn || bFlagWrLvlEn) {
        LwU32 fbio_hbm_cg_overrides = REG_RD32(BAR0, unicastMasterReadFromBroadcastRegister (LW_PFB_FBPA_FBIO_HBM_CG_OVERRIDES));
        REG_WR32(LOG, multicastMasterWriteFromBrodcastRegister (LW_PFB_FBPA_FBIO_HBM_CG_OVERRIDES),
            FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_CG_OVERRIDES, _BLCG_BYTE, _DISABLED, fbio_hbm_cg_overrides));

        if (bFlagDcaEn) {
            initialize_dca_arrays();

            //Disable periodic ddllcal updates to prevent wdqs glitches
            if(periodic_calibration_on) {
              set_fbio_periodic_ddllcal_disable(LW_TRUE);
            }

            //Override WDQS with continuous pattern from fbio
            //Start by setting WDQS_PATTERN to 0 and enable the override - to ensure no spurious toggles
            saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_WDQS_CFG, _OVRD_EN, 0x1, 0x0 );
            REG_WR32(LOG,LW_PFB_FBPA_FBIO_HBM_WDQS_CFG, saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg );

            //Turn on WDQS pattern
            set_fbio_wdqs_ovrd(LW_TRUE);

            //DCA sweeps from +7 to -7. To reduce the complexity of the algorithm and the number
            //of priv accesses needed, sweep all of the DCA settings for all WDQS together using
            //a broadcast MR11 write. Only program the individual WDQS settings at the end
            //This has to be done in 2 loops because of how the encoding is done. Also because
            //dca_setting is unsigned and ZERO = 0, in the first loop we just keep subtracting dca_setting
            //until it wraps to a large number
            LwU8 dca_setting;
            for (dca_setting = DCA_NEG_7_SETTING; dca_setting <= DCA_NEG_7_SETTING; dca_setting--) {
                //Program current DCA setting
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_DCA, BOOT_TR_DEBUG_CMD_INIT_SWEEP, dca_setting);
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS11, (0x00B00000 | ((dca_setting & 0xF) << 4) | (dca_setting & 0xF)));

                //Enable DCM and wait tDCMM=1uS before reading DERR
                set_hbm_dcm(LW_TRUE);
                OS_PTIMER_SPIN_WAIT_NS(DCA_T_DCMM);
                read_derr_status_dca(dca_setting);
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_DCA, BOOT_TR_DEBUG_CMD_READ_STATUS, dca_setting);

                //Disable DCM before changing the DCA setting
                set_hbm_dcm(LW_FALSE);
            }
            for (dca_setting = DCA_PLUS_1_SETTING; dca_setting <= DCA_PLUS_7_SETTING; dca_setting++) {
                //Program current DCA setting
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_DCA, BOOT_TR_DEBUG_CMD_INIT_SWEEP, dca_setting);
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS11, (0x00B00000 | ((dca_setting & 0xF) << 4) | (dca_setting & 0xF)));

                //Enable DCM and wait tDCMM=1uS before reading DERR
                set_hbm_dcm(LW_TRUE);
                OS_PTIMER_SPIN_WAIT_NS(DCA_T_DCMM);
                read_derr_status_dca(dca_setting);
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_DCA, BOOT_TR_DEBUG_CMD_READ_STATUS, dca_setting);

                //Disable DCM before changing the DCA setting
                set_hbm_dcm(LW_FALSE);
            }

            //Turn off WDQS
            set_fbio_wdqs_ovrd(LW_FALSE);
            saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg = 0x0;
            REG_WR32(LOG,LW_PFB_FBPA_FBIO_HBM_WDQS_CFG, saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg );

            //Restore periodic ddllcal updates
            if(periodic_calibration_on) {
              set_fbio_periodic_ddllcal_disable(LW_FALSE);
            }
            wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX4

            //At this point we have tested every DCA setting. The 1<->0 transition points should be
            //saved in dca_setting_per_wdqs. There is no failing condition, worst case we max the
            //codes at either -7 or +7
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_DCA, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_START, 0);
            program_final_dca_settings();
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_DCA, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_COMPLETE, 0);
            osPTimerTimeNsLwrrentGet(&startTime);  //Starting point

        }

        if (bFlagWrLvlEn) {
            initialize_wdqs_arrays();

            //Avoid the X propagation on WDQS pin upon trim value change
            //LW_PFB_FBPA_FBIO_HBM_CFG13_BRICK_SWAP_PULSE_DQ_FORCE swap pulse set to 1
            //Program the trim
            //LW_PFB_FBPA_FBIO_HBM_CFG13_BRICK_SWAP_PULSE_DQ_FORCE swap pulse set to 0
            saved_reg->pfb_fbio_hbm_cfg13 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CFG13_BRICK_SWAP_PULSE);

            //Disable periodic ddllcal updates to prevent wdqs glitches
            if(periodic_calibration_on) {
              set_fbio_periodic_ddllcal_disable(LW_TRUE);
            }

            //Enable write leveling by setting MR0[3] and enabling FBIO DERR
            //feedback and WDQS generation
            set_hbm_write_leveling(LW_TRUE);

            //Override WDQS with continuous pattern from fbio
            //Start by setting WDQS_PATTERN to 0 and enable the override - to ensure no spurious toggles
            saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_WDQS_CFG, _OVRD_EN, 0x1, 0x0 );
            REG_WR32(LOG,LW_PFB_FBPA_FBIO_HBM_WDQS_CFG, saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg );

            //Write leveling sweeps around the initial WDQS setting. To start, read the
            //current WDQS delay and then determine the start and end values based on
            //the initial delay. For each setting:
            //1. Progam the delay
            //2. Enable WDQS override in FBIO
            //3. Read the DERR registers for early/late status
            //4. Disable WDQS override
            if (wdqs_initial_delay == 0) {
                //Only read the initial delay if this is the first time training
                wdqs_initial_delay = read_fbio_wdqs_delay();
            }
            LwU16 wdqs_start_delay = wdqs_initial_delay + wr_lvl_delay_min;
            LwU16 wdqs_delay;
            LwU16 wdqs_delay_prev = wdqs_initial_delay;
            for (wdqs_delay = wdqs_start_delay; wdqs_delay <= (wdqs_initial_delay + wr_lvl_delay_max); wdqs_delay += wr_lvl_delay_step) {
                force_dq_swap_pulse(LW_TRUE);
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WDQS, BOOT_TR_DEBUG_CMD_INIT_SWEEP, wdqs_delay);
                program_fbio_wdqs_delay_broadcast(wdqs_delay, wdqs_delay_prev);
                force_dq_swap_pulse(LW_FALSE);
                set_fbio_wdqs_ovrd(LW_TRUE);
                OS_PTIMER_SPIN_WAIT_NS(wr_derr_rd_delay);
                read_derr_status_wrlvl(wdqs_delay, wdqs_start_delay);
                set_fbio_wdqs_ovrd(LW_FALSE);
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WDQS, BOOT_TR_DEBUG_CMD_READ_STATUS, wdqs_delay);

                //Save previous value. We use this to avoid unnecessary writes to the BRL register
                wdqs_delay_prev = wdqs_delay;

                //We always want to test the last setting, so if step != 1, adjust the value of wdqs_delay
                //to ensure we hit it
                if (((wdqs_delay + wr_lvl_delay_step) > (wdqs_initial_delay + wr_lvl_delay_max)) && (wdqs_delay != (wdqs_initial_delay + wr_lvl_delay_max))) {
                  wdqs_delay = (wdqs_initial_delay + wr_lvl_delay_max) - wr_lvl_delay_step;
                }
            }

            //Turn off WDQS OVRD
            saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg = 0x0;
            REG_WR32(LOG,LW_PFB_FBPA_FBIO_HBM_WDQS_CFG, saved_reg->pfb_fbpa_fbio_hbm_wdqs_cfg );


            //End of write leveling. Disable write leveling before programming final values
            //We want the final setting to be the point where we transition from early to late
            set_hbm_write_leveling(LW_FALSE);
            force_dq_swap_pulse(LW_TRUE);
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WDQS, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_START, 0);
            program_final_wdqs_delays();
            force_dq_swap_pulse(LW_FALSE);

            //Restore periodic ddllcal updates
            if(periodic_calibration_on) {
              set_fbio_periodic_ddllcal_disable(LW_FALSE);
            }

            wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX5
            //Sanity check
            checkDisabledPaDqsArrays(TRAINING_DIR_WRITE);
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WDQS, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_COMPLETE, 0);
            osPTimerTimeNsLwrrentGet(&startTime);  //Starting point
        }

        //Restore previous read value, using broadcast write
        REG_WR32(LOG, multicastMasterWriteFromBrodcastRegister (LW_PFB_FBPA_FBIO_HBM_CG_OVERRIDES), fbio_hbm_cg_overrides);

    }

    //=========================================================================
    //Read Training
    //=========================================================================
    if (bFlagRdTrEn) {
        //Setup char for read training
        char_setup(TRAINING_DIR_READ, bFlagDbiEnRd);

        //read it here so that it can be reused for reseting ififo and RDQS_DIV after changing the RDQS trim values.
        saved_reg->pfb_fbio_hbm_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CFG0);

        //*****************************************************************************************
        //Read Training Part 1 - RDQS
        //In the first part of read training, we sweep RDQS backward until we've found the first
        //setting at which any DQ associated with the RDQS passes. Once this sweep is done, we set
        //RDQS to that position.
        //*****************************************************************************************
        //Program initial IB delay for each pin to 0. Set the RX DDLL (RDQS) delay to rd_rdqs_delay_max to start the sweep.
        program_fbio_ib_delay_broadcast(0);
        program_fbio_rx_ddll_delay_broadcast(rd_rdqs_delay_max);
        reset_ififo_rdqs_div_pointers();
        //TODO Add some type of wait here to let delays propagate and settle? Will the eye array init below cover it?

        //Initialize the arrays tracking the first failing setting for each RDQS
        initialize_eye_arrays();

        //Sweep through the delays in reverse and run char for each setting
        LwS16 lwrrent_delay;
        for (lwrrent_delay = rd_rdqs_delay_max; lwrrent_delay >= rd_rdqs_delay_min; lwrrent_delay -= rd_rdqs_delay_step) {
            //Run training with read only. This will return the patterns we just
            //wrote at low frequency
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_RDQS, BOOT_TR_DEBUG_CMD_INIT_SWEEP, lwrrent_delay);
            start_training(TRAINING_DIR_READ);
            wait_for_training_done();

            //Move on to the next setting. We do this here so the time needed
            //for delays to propagate and clocks to settle can be hidden behind the time spent reading
            //per-pin pass/fail status. Note that the read_per_rdqs_pass_fail_status is for the delay
            //we just tested, but the delay programmed into the FBIO registers will have already moved on
            //to the next setting.
            LwS16 next_delay_setting = (lwrrent_delay - rd_rdqs_delay_step);
            if (next_delay_setting < rd_rdqs_delay_min) {
                program_fbio_rx_ddll_delay_broadcast(rd_rdqs_delay_min);
                reset_ififo_rdqs_div_pointers();
            }
            else {
                program_fbio_rx_ddll_delay_broadcast(next_delay_setting);
                reset_ififo_rdqs_div_pointers();
            }

            //Read the pass/fail status for each pin and update the marker for each RDQS
            //indicating the first delay where any pin passed
            //If every RDQS has seen a passing setting, we can terminate the loop early
            LwBool all_rdqs_see_pass = read_per_rdqs_pass_fail_status(lwrrent_delay);
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_RDQS, BOOT_TR_DEBUG_CMD_READ_STATUS, lwrrent_delay);
            if (all_rdqs_see_pass) {
                break;
            }

            //Special case -- if lwrrent_delay - rd_rdqs_delay_step will go under rd_rdqs_delay_min but we haven't tested
            //the min setting yet, need to set lwrrent_delay = rd_rdqs_delay_min + rd_rdqs_delay_step so we can test the
            //final setting before exiting the loop.
            //E.g. if the loop was for (delay = 10; delay >= 5; delay -= 2) we would test 10, 8, 6, then exit the loop.
            //To ensure we test 5, we set delay = min + step (5+2 here) so the next iteration works as expected.
            if ((lwrrent_delay != rd_rdqs_delay_min) && (next_delay_setting < rd_rdqs_delay_min)) {
                lwrrent_delay = (rd_rdqs_delay_min + rd_rdqs_delay_step);
            }
        }
        wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX6

        //The first passing setting for each RDQS is in first_pass_per_rdqs. Do a
        //final programming step to set the RX_DDLL delay for each RDQS to that point
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_RDQS, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_START, 0);
        program_final_ib_rdqs_delays();
        reset_ififo_rdqs_div_pointers();
        wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX7
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_RDQS, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_COMPLETE, 0);
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_RD_RDQS_TRAINING_COMPLETE, 0);
        osPTimerTimeNsLwrrentGet(&startTime);  //Starting point

        //Sanity check
        checkDisabledPaDqsArrays(TRAINING_DIR_READ);

        //*****************************************************************************************
        //Read Training Part 2 - DQ
        //Now that RDQS is past the eye for every DQ pin, sweep DQ to find the eye center for each
        //pin. Once that has been found, program the final value such that the min delay is 0 and
        //all other delays are relative to it. This is done to save power in the trimmers (rather
        //than just using the eye centers directly)
        //*****************************************************************************************
        //Set up initial delay
        program_fbio_ib_delay_broadcast(rd_dq_delay_min);
        //TODO: Delay needed here to let IB delay propagate?

        //Sweep through the delays and run char for each setting
        LwU16 prev_delay = rd_dq_delay_min;
        for (lwrrent_delay = rd_dq_delay_min; lwrrent_delay <= rd_dq_delay_max; lwrrent_delay += rd_dq_delay_step) {
            //Run training with read only. This will return the patterns we just
            //wrote at low frequency
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_DQ, BOOT_TR_DEBUG_CMD_INIT_SWEEP, lwrrent_delay);
            start_training(TRAINING_DIR_READ);
            wait_for_training_done();

            //Move on to the next setting. We do this here so the time needed
            //for delays to propagate and clocks to settle can be hidden behind the time spent reading
            //per-pin pass/fail status. Note that the read_per_pin_pass_fail_status is for the delay
            //we just tested, but the delay programmed into the FBIO registers will have already moved on
            //to the next setting.
            LwU16 next_delay_setting = (lwrrent_delay + rd_dq_delay_step);
            if (next_delay_setting > rd_dq_delay_max) {
              program_fbio_ib_delay_broadcast(rd_dq_delay_max);
            }
            else {
              program_fbio_ib_delay_broadcast(next_delay_setting);
            }

            //Read the pass/fail status for each pin and update windows
            //If all of the pins are failing across all DQ, we can exit the loop early,
            //but only if we've seen at least one setting where something passed
            read_per_pin_pass_fail_status(lwrrent_delay, prev_delay, LW_FALSE, TRAINING_DIR_READ); //DQ pins
            if (bFlagDbiEnRd | bFlagEccEn) {
                read_per_pin_pass_fail_status(lwrrent_delay, prev_delay, LW_TRUE, TRAINING_DIR_READ); //DBI+ECC pins
            }
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_DQ, BOOT_TR_DEBUG_CMD_READ_STATUS, lwrrent_delay);
            prev_delay = lwrrent_delay;

            //Special case -- if lwrrent_delay + rd_dq_delay_step will go over rd_dq_delay_max but we haven't tested
            //the max setting yet, need to set lwrrent_delay - rd_dq_delay_max - rd_dq_delay_step so we can test the
            //final setting before exiting the loop.
            if ((lwrrent_delay != rd_dq_delay_max) && (next_delay_setting > rd_dq_delay_max)) {
              lwrrent_delay = (rd_dq_delay_max - rd_dq_delay_step);
            }
        }

        //Do a final call to update_eye_arrays in case the best eye is in lwrrent_eye_per_* since it was at
        //the end of the sweep range.
        end_of_training_update_eye_arrays(TRAINING_DIR_READ, LW_FALSE);
        if (bFlagDbiEnRd) {
            end_of_training_update_eye_arrays(TRAINING_DIR_READ, LW_TRUE);
        }
        wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX8

        //Read training has completed. The best eyes will be stored in best_eye_per_dq/best_eye_per_dbi
        //To save power, we want to move RDQS back to be in the center of the latest DQ and then offset
        //all DQ delays relative to that
        //If any pin has no best eye (width=0), then program its final delay to all 1s (Trimmer = 0x3F)
        //and fail after programming all pins so we can see everything failing
        //Allow the updates to take effect immediately instead of waiting for a refresh by setting IB_FORCE=1
        set_ib_dq_force_perdiodic_updates(LW_TRUE);
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_DQ, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_START, 0);
        program_final_ib_delays();
        set_ib_dq_force_perdiodic_updates(LW_FALSE);

        //Reset the IFIFO and pad divider as we change the rdqs trim
        reset_ififo_rdqs_div_pointers();
        wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX9
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD_DQ, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_COMPLETE, 0);
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_RD_DQ_TRAINING_COMPLETE, 0);
        osPTimerTimeNsLwrrentGet(&startTime);  //Starting point

        //Sanity check
        checkDisabledPaDataArrays();
    }

    //=========================================================================
    //Write Training
    //=========================================================================
    if (bFlagWrTrEn) {
        //Clear any DQ/DBI/ECC masks on the traning broadcast registers
        //Set the mask for DBI and/or ECC if they are not enabled to avoid setup/hold
        hbm_trng_broadcast_mask = 0;
        hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _CMD,    1, hbm_trng_broadcast_mask);
        hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_SEV, 1, hbm_trng_broadcast_mask);
        hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_DBI, 1, hbm_trng_broadcast_mask);
        if (!bFlagEccEn) {
          hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_ECC, 1, hbm_trng_broadcast_mask);
        }
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, hbm_trng_broadcast_mask);

        //Reset the DLL OFFSET to 0 since the interface is being trained again.
        //Periodic training will use new reference value after the write training.
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST1 , 0x0);

        //Load patterns into the patram
        load_char_patram(TRAINING_DIR_WRITE);

        //Setup char for write training
        //For the first loop, we want DBI disabled so we only train DQ + ECC
        char_setup(TRAINING_DIR_WRITE, LW_FALSE);

        //Program initial IB delay for each pin. Since they all have the same delay, we can use
        //broadcast writes
        program_fbio_ob_delay_broadcast(wr_delay_min);
        //TODO Add some type of wait here to let delays propagate and settle

        //Initialize the arrays tracking current and best eye
        initialize_eye_arrays();

        //Sweep through the delays and run char for each setting
        LwU16 lwrrent_delay;
        LwU16 prev_delay = wr_delay_min;
        for (lwrrent_delay = wr_delay_min; lwrrent_delay <= wr_delay_max; lwrrent_delay += wr_delay_step) {
            //Run training
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_INIT_SWEEP, lwrrent_delay);
            start_training(TRAINING_DIR_BOTH); //Note: TRAINING_DIR_BOTH means read + write
            wait_for_training_done();

            //Get baseline WOSC while everything is active instead of idle. We only do this on the final
            //training loop
            if ((lwrrent_delay == wr_delay_max) && bFlagEnablePeriodicTr) {
                getBaselineWosc();
            }

            //Move on to the next setting. We do this here so the time needed
            //for delays to propagate and clocks to settle can be hidden behind the time spent reading
            //per-pin pass/fail status. Note that the read_per_pin_pass_fail_status is for the delay
            //we just tested, but the delay programmed into the FBIO registers will have already moved on
            //to the next setting.
            LwU16 next_delay_setting = (lwrrent_delay + wr_delay_step);
            if (next_delay_setting > wr_delay_max) {
              program_fbio_ob_delay_broadcast(wr_delay_max);
            }
            else {
              program_fbio_ob_delay_broadcast(next_delay_setting);
            }

            //Read the pass/fail status for each pin and update windows
            read_per_pin_pass_fail_status(lwrrent_delay, prev_delay, LW_FALSE, TRAINING_DIR_WRITE); //DQ
            if (bFlagEccEn) {
                read_per_pin_pass_fail_status(lwrrent_delay, prev_delay, LW_TRUE, TRAINING_DIR_WRITE); //ECC
            }
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_READ_STATUS, lwrrent_delay);
            prev_delay = lwrrent_delay;

            //Special case -- if lwrrent_delay + wr_delay_step will go over wr_delay_max but we haven't tested
            //the max setting yet, need to set lwrrent_delay - wr_delay_max - wr_delay_step so we can test the
            //final setting before exiting the loop.
            if ((lwrrent_delay != wr_delay_max) && (next_delay_setting > wr_delay_max)) {
              lwrrent_delay = (wr_delay_max - wr_delay_step);
            }
        }

        //Do a final call to update_eye_arrays in case the best eye is in lwrrent_eye_per_* since it was at
        //the end of the sweep range.
        end_of_training_update_eye_arrays(TRAINING_DIR_WRITE, LW_FALSE);
        wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX10

        //First step of write training has completed. The best eyes will be stored in
        //best_eye_per_dq/best_eye_per_dbi. Grab the center of each eye and program the
        //FBIO delay registers with a final value. If any pin has no best eye (width=0),
        //then program its final delay to all 1s (BS=0x7, Trimmer = 0x3F) and fail after
        //programming all pins so we can see everything failing
        //Allow the updates to take effect immediately instead of waiting for a refresh by setting OB_FORCE=1
        set_ob_dq_force_perdiodic_updates(LW_TRUE);
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_START, 0);
        program_final_ob_delays(LW_FALSE);
        set_ob_dq_force_perdiodic_updates(LW_FALSE);
        wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX11
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_COMPLETE, 0);
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_WR_DQ_TRAINING_COMPLETE, 0);
        osPTimerTimeNsLwrrentGet(&startTime);  //Starting point

        //Sanity check
        checkDisabledPaDataArrays();

        //=========================================================================
        //Write DBI Training
        //=========================================================================
        //If DBI is enabled, do another write training loop with DBI enabled now that we
        //know DQ is good
        //We can skip reloading the patram since it has already been done for write patterns
        if (bFlagDbiEnWr) {
            //Setup char for write training
            //For the first loop, we want DBI disabled so we only train DQ + ECC
            char_setup(TRAINING_DIR_WRITE, bFlagDbiEnWr);

            //Program initial IB delay for each pin. Since they all have the same delay, we can use
            //broadcast writes. For DBI training, we want to mask off updates to the DQ and ECC pins
            //since those already have good values
            //Note: This mask will be active throughout DBI training
            LwU32 hbm_trng_broadcast_mask = 0;
            hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _CMD,         1,   hbm_trng_broadcast_mask);
            hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ,          1,   hbm_trng_broadcast_mask);
            hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_DBI,      0,   hbm_trng_broadcast_mask);
            hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_ECC,      1,   hbm_trng_broadcast_mask);
            hbm_trng_broadcast_mask = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, _DQ_SEV,      1,   hbm_trng_broadcast_mask);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, hbm_trng_broadcast_mask);

            //Reset the DLL OFFSET to 0 since the interface is being trained again.
            //Periodic training will use new reference value after the write training.
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST1 , 0x0);

            program_fbio_ob_delay_broadcast(wr_delay_min);
            //TODO Add some type of wait here to let delays propagate and settle? Will the eye array init below cover it?

            //Initialize the arrays tracking current and best eye
            initialize_eye_arrays();

            //Sweep through the delays and run char for each setting
            LwU16 lwrrent_delay;
            LwU16 prev_delay = wr_delay_min;
            for (lwrrent_delay = wr_delay_min; lwrrent_delay <= wr_delay_max; lwrrent_delay += wr_delay_step) {
                //Run training
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DBI, BOOT_TR_DEBUG_CMD_INIT_SWEEP, lwrrent_delay);
                start_training(TRAINING_DIR_BOTH); //Note: TRAINING_DIR_BOTH means read + write
                wait_for_training_done();

                //Move on to the next setting. We do this here so the time needed
                //for delays to propagate and clocks to settle can be hidden behind the time spent reading
                //per-pin pass/fail status. Note that the read_per_pin_pass_fail_status is for the delay
                //we just tested, but the delay programmed into the FBIO registers will have already moved on
                //to the next setting.
                LwU16 next_delay_setting = (lwrrent_delay + wr_delay_step);
                if (next_delay_setting > wr_delay_max) {
                    program_fbio_ob_delay_broadcast(wr_delay_max);
                }
                else {
                    program_fbio_ob_delay_broadcast(next_delay_setting);
                }

                //Read the pass/fail status for each pin and update windows
                read_per_pin_pass_fail_status(lwrrent_delay, prev_delay, LW_TRUE, TRAINING_DIR_WRITE); //DBI
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DBI, BOOT_TR_DEBUG_CMD_READ_STATUS, lwrrent_delay);
                prev_delay = lwrrent_delay;

                //Special case -- if lwrrent_delay + wr_delay_step will go over wr_delay_max but we haven't tested
                //the max setting yet, need to set lwrrent_delay - wr_delay_max - wr_delay_step so we can test the
                //final setting before exiting the loop.
                if ((lwrrent_delay != wr_delay_max) && (next_delay_setting > wr_delay_max)) {
                    lwrrent_delay = (wr_delay_max - wr_delay_step);
                }
            }

            //Do a final call to update_eye_arrays in case the best eye is in lwrrent_eye_per_* since it was at
            //the end of the sweep range.
            end_of_training_update_eye_arrays(TRAINING_DIR_WRITE, LW_TRUE);

            //Reset the training broadcast mask
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_BROADCAST_MASK, 0x1); //Reset value
            wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX12

            //Last step of write training has completed. We only care about DBI this time.
            //The best eyes will be stored in best_eye_per_dbi. Grab the center of each eye
            //and program the FBIO delay registers with a final value. If any pin has no
            //best eye (width=0), then program its final delay to all 1s
            //(BS=0x7, Trimmer = 0x3F) and fail after programming all pins so we can see
            //everything failing
            //Allow the updates to take effect immediately instead of waiting for a refresh by setting OB_FORCE=1
            set_ob_dq_force_perdiodic_updates(LW_TRUE);
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DBI, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_START, 0);
            program_final_ob_delays(LW_TRUE);
            set_ob_dq_force_perdiodic_updates(LW_FALSE);
            wr_mbox_elapsed(debug_mbox_idx++, &startTime); //MBOX13
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DBI, BOOT_TR_DEBUG_CMD_PROGRAM_FINAL_COMPLETE, 0);
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_WR_DBI_TRAINING_COMPLETE, 0);

            //Sanity check
            checkDisabledPaDataArrays();
        }
    }


    //=========================================================================
    //Cleanup
    //=========================================================================
    //Restore saved registers before exiting training
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, saved_reg->pfb_fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_TIMING21, saved_reg->pfb_fbpa_timing21);
    REG_WR32(LOG, LW_PFB_FBPA_HBM_CHAR, saved_reg->pfb_fbpa_hbm_char);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_CTRL, saved_reg->pfb_fbpa_training_char_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(0), saved_reg->pfb_fbpa_training_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(1), saved_reg->pfb_fbpa_training_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, saved_reg->pfb_fbpa_fbio_hbm_ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_TRNG_MISC, saved_reg->pfb_fbio_hbm_trng_misc);

    LwU32 result = 0xabcd1234;

    //For ATE, write the overall status per PA of each training step
    if (bFlagAteNoFail) {
        FW_MBOX_WR32(8, wdqs_training_pass_per_pa);
        FW_MBOX_WR32(9, rd_dq_training_pass_per_pa);
        FW_MBOX_WR32(10, wr_dq_training_pass_per_pa);
        FW_MBOX_WR32(11, wr_dbi_training_pass_per_pa);
    }

    // to signal end of boot training for TB
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xB007d423);
    falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_TRAINING_COMPLETE, 0);
    return result;
}

