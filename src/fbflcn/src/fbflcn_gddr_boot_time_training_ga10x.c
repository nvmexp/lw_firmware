#include <falcon-intrinsics.h>
#include <falc_debug.h>
#include <falc_trace.h>

#include "fbflcn_gddr_boot_time_training_ga10x.h"
#include "fbflcn_gddr_mclk_switch.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "memory.h"
#include "data.h"
#include "priv.h"


#include "osptimer.h"
#include "fbflcn_objfbflcn.h"
extern OBJFBFLCN Fbflcn;
#include "config/g_memory_private.h"

#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"
#include "dev_trim.h"
#include "dev_pri_ringstation_fbp.h"
#include "memory_gddr.h"

//Boot training silicon debug
//Full documentation available in P4 @ //hw/doc/gpu/ampere/ampere/verif/FB/GA10x_Boot_Training_Breakpoints.docx
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
#define BOOT_TR_DEBUG_ENABLE_RD     0x3F //All breakpoints = 0x3F
#define BOOT_TR_DEBUG_ENABLE_WR_DQ  0xFC //All breakpoints = 0xFC
#define BOOT_TR_DEBUG_ENABLE_WR_MTA 0xFC //All breakpoints = 0xFC

#ifdef BOOT_TR_DEBUG_ENABLE
LwBool bFlagBootTrDebugEnRd    = LW_FALSE; //Tied to bootTrainingTable.flag_g6_rd_tr_hybrid_non_vref_en
LwBool bFlagBootTrDebugEnWrDQ  = LW_FALSE; //Tied to bootTrainingTable.flag_g6_wr_tr_hybrid_non_vref_en
LwBool bFlagBootTrDebugEnWrMTA = LW_FALSE; //Tied to bootTrainingTable.flag_g6_wr_tr_hybrid_vref_en
LwBool bFlagBootTrDebugEnMisc  = LW_FALSE; //Tied to bootTrainingTable.flag_collect_schmoo_data
#endif

//Commands issued in FALCON_MONITOR
#define BOOT_TR_DEBUG_CMD_RD     0
#define BOOT_TR_DEBUG_CMD_WR_DQ  1
#define BOOT_TR_DEBUG_CMD_WR_MTA 2
#define BOOT_TR_DEBUG_CMD_MISC   3
#define BOOT_TR_DEBUG_CMD_GPU_INIT_VREF_SWEEP           0
#define BOOT_TR_DEBUG_CMD_GPU_INIT_VREF                 1
#define BOOT_TR_DEBUG_CMD_PI_OFFSET_SWEEP               2
#define BOOT_TR_DEBUG_CMD_PI_OFFSET_AREA_VREF           3
#define BOOT_TR_DEBUG_CMD_PI_OFFSET_AVG_AREA_VREF_DFE   4
#define BOOT_TR_DEBUG_CMD_FINAL_AREA_VREF_DFE           5
#define BOOT_TR_DEBUG_CMD_VREF_INIT_DRAM_SWEEP_DQ       6
#define BOOT_TR_DEBUG_CMD_DRAM_INIT_VREF                7
#define BOOT_TR_DEBUG_CMD_MISC_END_OF_TRAINING          0x0C
#define BOOT_TR_DEBUG_CMD_FALCON_CONTINUE               0x3F

//Commands to read internal arrays
#define BOOT_TR_DEBUG_ARRAY_AREA_PER_DQ        0
#define BOOT_TR_DEBUG_ARRAY_AREA_PER_DBI       1
#define BOOT_TR_DEBUG_ARRAY_VREF_PER_DQ        2
#define BOOT_TR_DEBUG_ARRAY_VREF_PER_DBI       3
#define BOOT_TR_DEBUG_ARRAY_BEST_DFE_PER_DQ    4
#define BOOT_TR_DEBUG_ARRAY_BEST_DFE_PER_DBI   5
#define BOOT_TR_DEBUG_ARRAY_BEST_VREF_PER_DQ   6
#define BOOT_TR_DEBUG_ARRAY_BEST_VREF_PER_DBI  7

//Bit positions FALCON_MONITOR
#define BOOT_TR_DEBUG_LWMT_REQ         31
#define BOOT_TR_DEBUG_FALCON_REQ       30
#define BOOT_TR_DEBUG_READ_ARRAY       29
#define BOOT_TR_DEBUG_STEP_LSB          6
#define BOOT_TR_DEBUG_COMMAND_LSB       0
#define BOOT_TR_DEBUG_ARR_INDEX1_LSB   17
#define BOOT_TR_DEBUG_ARR_INDEX0_LSB    8
#define BOOT_TR_DEBUG_ARR_EYE_LSB       6
#define BOOT_TR_DEBUG_ARR_TARGET_LSB    0
#define BOOT_TR_DEBUG_LWRR_EYE_LSB      8
#define BOOT_TR_DEBUG_LWRR_DFE_LSB     10
#define BOOT_TR_DEBUG_LWRR_LOOP_LSB    14

//Masks for FALCON_MONITOR -- Note: These should be applied AFTER shifting by LSB
#define BOOT_TR_DEBUG_STEP_MASK         0x3
#define BOOT_TR_DEBUG_COMMAND_MASK      0x3F
#define BOOT_TR_DEBUG_ARR_INDEX_MASK    0x1FF
#define BOOT_TR_DEBUG_ARR_EYE_MASK      0x3
#define BOOT_TR_DEBUG_ARR_TARGET_MASK   0x3F

#define LW_PFB_FBPA_TRAINING_CMD0               LW_PFB_FBPA_TRAINING_CMD(0)
#define LW_PFB_FBPA_TRAINING_CMD1               LW_PFB_FBPA_TRAINING_CMD(1)

//TODO [bswamy] New Defines for G6X
#define TRAINING_DIR_READ  0
#define TRAINING_DIR_WRITE 1

#define TOTAL_NUM_DFE      8 //PAM4 - For G6 we will use the low + mid arrays to cover all 16 DFE
#define NUM_EYES_GDDR6X    3

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
static LwU32 gPafalconDebugWriteAdr = 0;
extern AON_STATE_INFO gAonStateInfo;
#endif

typedef struct REGBOX1 {
LwU32 pfb_fbpa_training_rw_vref_ctrl;
LwU32 pfb_fbpa_training_rw_cors_intrpltr_ctrl;
LwU32 pfb_fbpa_training_rw_fine_intrpltr_ctrl;

LwU32 pfb_fbpa_training_patram;
LwU32 pfb_fbpa_training_char_bank_ctrl;
LwU32 pfb_fbpa_training_char_bank_ctrl2;
LwU32 pfb_fbpa_training_char_burst;
LwU32 pfb_fbpa_training_char_ctrl;
LwU32 pfb_fbpa_training_char_turn;

LwU32 fbio_pwr_ctrl;
LwU32 fbio_pwr_ctrl1;
} REGBOX1;

REGBOX1  lwrrentRegValues1;
REGBOX1* saved_reg = &lwrrentRegValues1;

// Reading flags from VBIOS
LwBool bFlagG6AddrTrEn, bFlagG6WckTrEn, bFlagG6RdTrEn, bFlagG6RdTrPrbsEn;
LwBool bFlagG6WrTrPrbsEn;
LwBool bFlagG6EdcTrackingEn, bFlagG6RdEdcEnabled, bFlagG6WrEdcEnabled;
LwBool bFlagG6VrefTrackingEn;

//LwBool bFlagDoAddrTr     = LW_TRUE;
//LwBool bFlagDoWckTr      = LW_TRUE;
LwBool bFlagDoRdTr       = LW_TRUE;
LwBool bFlagDoWrTr       = LW_TRUE;
LwBool bFlagDoP0P8Final  = LW_TRUE;
LwBool bFlagNoDFEChange  = LW_FALSE;

//Boot Training Config
LwU32 gbl_ddr_mode = LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X;

//"Best" Arrays - Track the best dfe and vref per DQ for each receiver
//Note that these are for all DQs/DBIs across all PA and SUBP`
LwU16 best_vref_per_dq_low[TOTAL_DQ_BITS]    GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "1");
LwU16 best_vref_per_dq_mid[TOTAL_DQ_BITS]    GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "2");
LwU16 best_vref_per_dq_high[TOTAL_DQ_BITS]   GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "3");
LwU16 best_vref_per_dbi_low[TOTAL_DBI_BITS]  GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "4");
LwU16 best_vref_per_dbi_mid[TOTAL_DBI_BITS]  GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "5");
LwU16 best_vref_per_dbi_high[TOTAL_DBI_BITS] GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "6");
LwU8 best_dfe_per_dq[TOTAL_DQ_BITS]          GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "7");
LwU8 best_dfe_per_dbi[TOTAL_DBI_BITS]        GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("restoreArea", "8");

//Arrays to store area and VREF. These are 2D arrays indexed by [DQ][DFE]
//TODO: For low/mid/high it is probably more code efficient to add a 3rd dimension
//so we can avoid repeated code and if/else statements for which eye we're training
LwU16 area_per_dq_low[TOTAL_DQ_BITS][TOTAL_NUM_DFE];
LwU16 area_per_dq_mid[TOTAL_DQ_BITS][TOTAL_NUM_DFE];
LwU16 area_per_dq_high[TOTAL_DQ_BITS][TOTAL_NUM_DFE];

LwU16 area_per_dbi_low[TOTAL_DBI_BITS][TOTAL_NUM_DFE];
LwU16 area_per_dbi_mid[TOTAL_DBI_BITS][TOTAL_NUM_DFE];
LwU16 area_per_dbi_high[TOTAL_DBI_BITS][TOTAL_NUM_DFE];

LwU8 vref_per_dq_low[TOTAL_DQ_BITS][TOTAL_NUM_DFE];
LwU8 vref_per_dq_mid[TOTAL_DQ_BITS][TOTAL_NUM_DFE];
LwU8 vref_per_dq_high[TOTAL_DQ_BITS][TOTAL_NUM_DFE];

LwU8 vref_per_dbi_low[TOTAL_DBI_BITS][TOTAL_NUM_DFE];
LwU8 vref_per_dbi_mid[TOTAL_DBI_BITS][TOTAL_NUM_DFE];
LwU8 vref_per_dbi_high[TOTAL_DBI_BITS][TOTAL_NUM_DFE];

LwU8 vref_per_edc_low[TOTAL_DBI_BITS]; //Workaround to save EDC VREF values

//This temp variable needs to be global due to limited stack size
//It is only used in function update_area_moving_avg()
LwU16 temp_area[2*TOTAL_NUM_DFE]; //Area per DFE

//These values are populated from the boot training tables. The settings below are 
//hardcoded defaults. See parseBootTrainingTable() for actual values from vBIOS 
LwU8 dfe_start_rd       = 3;
LwU8 dfe_min_rd         = 0;
LwU8 dfe_max_rd         = 1; //10;
LwU8 dfe_step_rd        = 1;

LwU8 pi_cors_min_rd     = 0;
LwU8 pi_cors_max_rd     = 15;
LwU8 pi_cors_step_rd    = 2;
LwU8 pi_cors_step_ln_rd = 0;

LwS8 pi_fine_min_rd     = -35;
LwU8 pi_fine_max_rd     = 35;
LwU8 pi_fine_step_rd    = 4; //2;
LwU8 pi_fine_step_ln_rd = 0;

LwS8 pi_offset_min_rd   = -4; //-8;
LwU8 pi_offset_max_rd   = 4; //8;
LwU8 pi_offset_step_rd  = 1;

LwU8 vref_min_rd       = 64;
LwU8 vref_max_rd       = 200;
LwU8 vref_step_rd      = 1; //5;
LwU8 vref_min_low      = 72;
LwU8 vref_max_low      = 74; //122;
LwU8 vref_min_mid      = 139;
LwU8 vref_max_mid      = 141; //189;
LwU8 vref_min_high     = 206;
LwU8 vref_max_high     = 208; //255;

LwU8 dfe_min_wr         = 0;
LwU8 dfe_max_wr         = 1; //10;
LwU8 dfe_step_wr        = 1;

LwS8 pi_cors_min_wr     = 0;
LwU8 pi_cors_max_wr     = 15;
LwU8 pi_cors_step_wr    = 2;
LwU8 pi_cors_step_ln_wr = 0;

LwS8 pi_fine_min_wr     = -35;
LwU8 pi_fine_max_wr     = 35;
LwU8 pi_fine_step_wr    = 4; //2;
LwU8 pi_fine_step_ln_wr = 0;

LwS8 pi_offset_min_wr   = -4; //-8;
LwU8 pi_offset_max_wr   = 4; //8;
LwU8 pi_offset_step_wr  = 1;

LwU8 vref_min_wr        = 20;
LwU8 vref_max_wr        = 22; //63;
LwU8 vref_step_wr       = 1; //2;

LwBool cors_step_override_rd = LW_FALSE;
LwBool cors_step_override_wr = LW_FALSE;

LwBool enable_pi_offset_tr_rd = LW_TRUE;
LwBool enable_pi_offset_tr_wr = LW_TRUE;
LwBool enable_moving_avg_rd = LW_FALSE; //LW_TRUE;
LwBool enable_moving_avg_wr = LW_FALSE; //LW_TRUE;
LwBool dbi_en_rd = LW_FALSE;
LwBool dbi_en_wr = LW_FALSE;
LwBool boot_tr_mta_en = LW_FALSE;
LwBool dbi_en = LW_FALSE;

LwBool initial_vref_bkv = LW_FALSE;
LwBool initial_vref_area = LW_TRUE;

LwU8 averaging_loops_rd = 1; //4
LwU8 averaging_loops_wr = 1; //4
LwU8 eyes = NUM_EYES_GDDR6X;
LwU8 initial_vref_max_window = 0;

//Times for each step of boot training
#define BOOT_TR_TIME_CHAR            0
#define BOOT_TR_TIME_ADDR            1
#define BOOT_TR_TIME_WCK             2
#define BOOT_TR_TIME_RD_VREF         3
#define BOOT_TR_TIME_RD_DFE          4
#define BOOT_TR_TIME_RD_TR           5
#define BOOT_TR_TIME_WR_DQ_VREF      6
#define BOOT_TR_TIME_WR_DQ_VREF_OFF  7
#define BOOT_TR_TIME_WR_DQ_TR        8
#define BOOT_TR_TIME_WR_TR           9
#define BOOT_TR_TIME_BOOT_TR         10
LwU32 bootTrStepTimes[11];

//Boot Training Debug Functions
#ifdef BOOT_TR_DEBUG_ENABLE
LwU32 send_area_per_dq_dbi (
    LwBool isDQ,
    LwU32  read_info
)
{
    LwU16 lwrrent_dfe    = (read_info >> BOOT_TR_DEBUG_ARR_INDEX1_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;
    LwU16 lwrrent_dq_dbi = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;
    LwU8  eye_sel        = (read_info >> BOOT_TR_DEBUG_ARR_EYE_LSB)    & BOOT_TR_DEBUG_ARR_EYE_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    LwU32 max_index = (isDQ ? TOTAL_DQ_BITS : TOTAL_DBI_BITS);
    for (; lwrrent_dq_dbi < max_index; lwrrent_dq_dbi++) {
        for (; lwrrent_dfe < TOTAL_NUM_DFE; lwrrent_dfe++) {
            LwU16 area = 0;
            if (isDQ) {
                switch (eye_sel) {
                    case EYE_LOW:  area = area_per_dq_low[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_MID:  area = area_per_dq_mid[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_HIGH: area = area_per_dq_high[lwrrent_dq_dbi][lwrrent_dfe]; break;
                }
            }
            else {
                switch (eye_sel) {
                    case EYE_LOW:  area = area_per_dbi_low[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_MID:  area = area_per_dbi_mid[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_HIGH: area = area_per_dbi_high[lwrrent_dq_dbi][lwrrent_dfe]; break;
                }
            }
            mailbox_wr_data |= (area << (16*(num_data_written % 2)));

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
        lwrrent_dfe = 0; //Reset DFE for next loop
    }
    return mailbox0_data;
}


LwU32 send_vref_per_dq_dbi (
    LwBool isDQ,
    LwU32 read_info
)
{
    LwU16 lwrrent_dfe    = (read_info >> BOOT_TR_DEBUG_ARR_INDEX1_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;
    LwU16 lwrrent_dq_dbi = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;
    LwU8  eye_sel        = (read_info >> BOOT_TR_DEBUG_ARR_EYE_LSB)    & BOOT_TR_DEBUG_ARR_EYE_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    LwU32 max_index = (isDQ ? TOTAL_DQ_BITS : TOTAL_DBI_BITS);
    for (; lwrrent_dq_dbi < max_index; lwrrent_dq_dbi++) {
        for (; lwrrent_dfe < TOTAL_NUM_DFE; lwrrent_dfe++) {
            LwU8 vref = 0;
            if (isDQ) {
                switch (eye_sel) {
                    case EYE_LOW:  vref = vref_per_dq_low[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_MID:  vref = vref_per_dq_mid[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_HIGH: vref = vref_per_dq_high[lwrrent_dq_dbi][lwrrent_dfe]; break;
                }
            }
            else {
                switch (eye_sel) {
                    case EYE_LOW:  vref = vref_per_dbi_low[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_MID:  vref = vref_per_dbi_mid[lwrrent_dq_dbi][lwrrent_dfe]; break;
                    case EYE_HIGH: vref = vref_per_dbi_high[lwrrent_dq_dbi][lwrrent_dfe]; break;
                }
            }
            mailbox_wr_data |= (vref << (8*(num_data_written % 4)));

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
        lwrrent_dfe = 0; //Reset DFE for next loop
    }

    return mailbox0_data;
}

LwU32 send_best_dfe_per_dq_dbi (
    LwBool isDQ,
    LwU32 read_info
)
{
    LwU16 lwrrent_dq_dbi = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    LwU32 max_index = (isDQ ? TOTAL_DQ_BITS : TOTAL_DBI_BITS);
    for (; lwrrent_dq_dbi < max_index; lwrrent_dq_dbi++) {
        LwU8 best_dfe = 0;
        if (isDQ) {
            best_dfe = best_dfe_per_dq[lwrrent_dq_dbi];
        }
        else {
            best_dfe = best_dfe_per_dbi[lwrrent_dq_dbi];
        }
        mailbox_wr_data |= (best_dfe << (8*(num_data_written % 4)));

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

LwU32 send_best_vref_per_dq_dbi (
    LwBool isDQ,
    LwU32  read_info
)
{
    LwU16 lwrrent_dq_dbi = (read_info >> BOOT_TR_DEBUG_ARR_INDEX0_LSB) & BOOT_TR_DEBUG_ARR_INDEX_MASK;
    LwU8  eye_sel        = (read_info >> BOOT_TR_DEBUG_ARR_EYE_LSB)    & BOOT_TR_DEBUG_ARR_EYE_MASK;

    LwU8 num_data_written = 0;
    LwU32 mailbox_wr_data = 0;
    LwU32 mailbox0_data = 0;
    LwU32 max_index = (isDQ ? TOTAL_DQ_BITS : TOTAL_DBI_BITS);
    for (; lwrrent_dq_dbi < max_index; lwrrent_dq_dbi++) {
        LwU16 vref = 0;
        if (isDQ) {
            switch (eye_sel) {
                case EYE_LOW:  vref = best_vref_per_dq_low[lwrrent_dq_dbi]; break;
                case EYE_MID:  vref = best_vref_per_dq_mid[lwrrent_dq_dbi]; break;
                case EYE_HIGH: vref = best_vref_per_dq_high[lwrrent_dq_dbi]; break;
            }
        }
        else {
            switch (eye_sel) {
                case EYE_LOW:  vref = best_vref_per_dbi_low[lwrrent_dq_dbi]; break;
                case EYE_MID:  vref = best_vref_per_dbi_mid[lwrrent_dq_dbi]; break;
                case EYE_HIGH: vref = best_vref_per_dbi_high[lwrrent_dq_dbi]; break;
            }
        }
        mailbox_wr_data |= (vref << (16*(num_data_written % 2)));

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
        LwU8 falcon_mon_cmd = falcon_mon_data & 0x3F;
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
                case BOOT_TR_DEBUG_ARRAY_AREA_PER_DQ:
                    mailbox0_data = send_area_per_dq_dbi(LW_TRUE, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_AREA_PER_DBI:
                    mailbox0_data = send_area_per_dq_dbi(LW_FALSE, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_VREF_PER_DQ:
                    mailbox0_data = send_vref_per_dq_dbi(LW_TRUE, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_VREF_PER_DBI:
                    mailbox0_data = send_vref_per_dq_dbi(LW_FALSE, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_BEST_DFE_PER_DQ:
                    mailbox0_data = send_best_dfe_per_dq_dbi(LW_TRUE, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_BEST_DFE_PER_DBI:
                    mailbox0_data = send_best_dfe_per_dq_dbi(LW_FALSE, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_BEST_VREF_PER_DQ:
                    mailbox0_data = send_best_vref_per_dq_dbi(LW_TRUE, falcon_mon_data);
                    break;
                case BOOT_TR_DEBUG_ARRAY_BEST_VREF_PER_DBI:
                    mailbox0_data = send_best_vref_per_dq_dbi(LW_FALSE, falcon_mon_data);
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
        LwU8 training_major_step,
        LwU8 training_minor_step,
        LwU8 lwrrent_eye,
        LwU8 lwrrent_dfe,
        LwU8 averaging_loop_count
)
{
    //Determine if this breakpoint is enabled. If not, exit now
    //Debug enable bit is {MajorStep, MinorStep[3:0]}
    LwBool breakpointEn = LW_FALSE;
    /*
    switch (training_major_step) {
        case BOOT_TR_DEBUG_CMD_RD:
            breakpointEn = (BOOT_TR_DEBUG_ENABLE_RD >> training_minor_step) & 0x1;
            break;
        case BOOT_TR_DEBUG_CMD_WR_DQ:
            breakpointEn = (BOOT_TR_DEBUG_ENABLE_WR_DQ >> training_minor_step) & 0x1;
            break;
        case BOOT_TR_DEBUG_CMD_WR_MTA:
            breakpointEn = (BOOT_TR_DEBUG_ENABLE_WR_MTA >> training_minor_step) & 0x1;
            break;
        return;
    }
    */
    //[bswamy] It was requested to add independent flags for rd, wr, wr mta which are configurable
    //in vBIOS and have all breakpoints enabled for each. The above code has
    //been replaced with a simple check on training_major_step
    switch (training_major_step) {
        case BOOT_TR_DEBUG_CMD_RD:
            breakpointEn = bFlagBootTrDebugEnRd;
            break;
        case BOOT_TR_DEBUG_CMD_WR_DQ:
            breakpointEn = bFlagBootTrDebugEnWrDQ;
            break;
        case BOOT_TR_DEBUG_CMD_WR_MTA:
            breakpointEn = bFlagBootTrDebugEnWrMTA;
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
    falcon_mon_cmd |= averaging_loop_count << BOOT_TR_DEBUG_LWRR_LOOP_LSB;
    falcon_mon_cmd |= lwrrent_dfe << BOOT_TR_DEBUG_LWRR_DFE_LSB;
    falcon_mon_cmd |= lwrrent_eye << BOOT_TR_DEBUG_LWRR_EYE_LSB;
    falcon_mon_cmd |= training_major_step << BOOT_TR_DEBUG_STEP_LSB;
    falcon_mon_cmd |= training_minor_step;
    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, falcon_mon_cmd);

    //Now that the command has been sent, wait for LWMK to send back a command
    falcon_lwmt_monitor_cmd();
}
#else
void falcon_boot_tr_breakpoint (
        LwU8 training_major_step,
        LwU8 training_minor_step,
        LwU8 lwrrent_eye,
        LwU8 lwrrent_dfe,
        LwU8 averaging_loop_count
)
{
    //Empty function definition
    return;
}  
#endif

//Some vBIOS fields need to be sign extended since they have negative values
//Helper function to take the MSB and extend it through the remaining bits
LwS8 signExtendByte (
    LwS8 value,
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

// Simple translator to get a unicast address based off the broadcast address
// and the partition index
LwU32 multi_broadcast_wr_vref_dfe (LwU32 priv_addr, LwU32 partition, LwU32 subp, LwU32 byte)
{
    LwU32 retval;

    retval = priv_addr + (partition * 0x00004000) + (subp * 48) + (byte * 12);

    return (retval);
}

void set_fbio_pwr_ctrl
(
    LwU32 fbio_pwr_ctrl_val,
    LwU32 fbio_pwr_ctrl1_val
)
{
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_PWR_CTRL, fbio_pwr_ctrl_val);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_PWR_CTRL1, fbio_pwr_ctrl1_val);
}

void func_disable_char_bank_ctrl
(
        void
)
{
    LwU32 enable = 0;
    LwU32 fbpa_training_char_bank_ctrl;
    // Disabling bank_ctrl_enable for address training, as it gets set for Char load prbs at P8
    // Set LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_ENABLE to 0 before starting training otherwise char logic interferes with training.
    fbpa_training_char_bank_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL);
    fbpa_training_char_bank_ctrl = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_CHAR_BANK_CTRL,  _ENABLE,         enable,     fbpa_training_char_bank_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL,  fbpa_training_char_bank_ctrl);
}

void func_set_prbs_dual_mode
(
        LwU32 prbs_mode,
        LwU32 dual_mode
)
{
    //LwU32 fbpa_training_patram;
    //fbpa_training_patram= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    //fbpa_training_patram = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_PATRAM,  _DUAL_MODE, dual_mode,  fbpa_training_patram);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM,  fbpa_training_patram);

    memorySetTrainingControlPrbsMode_HAL(&Fbflcn, prbs_mode);
}

void func_setup_g6_addr_tr (void)
{

    //func_set_prbs_dual_mode(0,1);
    FuncLoadAdrPatterns(gbl_ddr_mode);
    func_disable_char_bank_ctrl();
    //func_ma2sdr(1);                        // FORCE_MA_TO_SDR_ALIGNMENT = 1 before kicking off address training

    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_TADR_CTRL,gTT.bootTrainingTable.ADDR_TADR_CTRL);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_CTRL,gTT.bootTrainingTable.ADDR_CTRL);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_CORS_CTRL,gTT.bootTrainingTable.ADDR_CORS_CTRL);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_FINE_CTRL,gTT.bootTrainingTable.ADDR_FINE_CTRL);
    return;
}

void func_setup_g6_wck_tr(void)
{

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_WCK_EDC_LIMIT,gTT.bootTrainingTable.WCK_EDC_LIMIT);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_WCK_INTRPLTR_CTRL_EXT,gTT.bootTrainingTable.WCK_INTRPLTR_CTRL_EXT);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_WCK_INTRPLTR_CTRL,gTT.bootTrainingTable.WCK_INTRPLTR_CTRL);
    return;
}

void func_run_training (
        LwU32 adr,
        LwU32 wck,
        LwU32 rd,
        LwU32 wr,
        LwU32 periodic,
        LwU32 char_engine)
{
    LwU32 go = 1;

    // Updating remaining fields, so it doesn't have incorrect values
    LwU32 control = 2;
    LwU32 quse = 0;
    LwU32 quse_prime = 0;
    LwU32 rw_level = 1;
    LwU32 step = 0;
    LwU32 strobe = 0;
    LwU32 fbpa_training_cmd;

    fbpa_training_cmd = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CMD(0));
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _ADR,          adr,          fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _CHAR_ENGINE,  char_engine,  fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _CONTROL,      control,      fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _PERIODIC,     periodic,     fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _QUSE,         quse,         fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _QUSE_PRIME,   quse_prime,   fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _RD,           rd,           fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _RW_LEVEL,     rw_level,     fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _STEP,         step,         fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _STROBE,       strobe,       fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _WCK,          wck,          fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _WR,           wr,           fbpa_training_cmd);
    fbpa_training_cmd = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_CMD,     _GO,           go,           fbpa_training_cmd);

    // Subp0 training kicked off only if supb isn't disabled;
    // if partition isn't floorswept but just disabled, subp0 could still kick off training and fail.
    LwU8 idx;
    LwU8 subp;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_TRAINING_CMD(0)), idx, subp, 0),fbpa_training_cmd);
                }
            }
        }
    }
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(1),     fbpa_training_cmd);
    return;
}

void
func_setup_misc_training_ctrl (void)
{
    LwU8 training_ctrl_gddr5_commands = 1;
    memorySetTrainingControlGddr5Commands_HAL(&Fbflcn,training_ctrl_gddr5_commands);
    return;
}

void func_setup_training_timings (void)
{
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TIMING,gTT.bootTrainingTable.GEN_TIMING);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TIMING2,gTT.bootTrainingTable.GEN_TIMING2);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TIMING3,gTT.bootTrainingTable.GEN_TIMING3);
    return;
}

void func_setup_rd_prbs_regs (void)
{
    //Need to set CHAR_MAX > CHAR_MIN
    LwU32 fbpa_training_patram = gTT.bootTrainingTable.READ_PATRAM;
    fbpa_training_patram = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATRAM, _CHAR_MAX, 15, fbpa_training_patram);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM, fbpa_training_patram);

    LwU8 patram_prbs_burst_cnt = REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_PRBS_BURST_CNT, fbpa_training_patram);
    LwU8 training_prbs_ilw     = REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_PRBS_ILW_EN, fbpa_training_patram);
    LwU32 training_char_burst =  training_prbs_ilw ? (( patram_prbs_burst_cnt * 2) -1) : (patram_prbs_burst_cnt -1);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BURST, training_char_burst);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_TURN, 0x00000000);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2,gTT.bootTrainingTable.READ_CHAR_BANK_CTRL2);

    LwU32 seed;
    for (seed = 0; seed < MAX_TRAINING_PATRAM_PRBS_SEEDS; seed++) {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM_PRBS_SEED(seed),gTT.bootTrainingTable.TRAINING_PATRAM_PRBS_SEED[seed]);
    }

    // SubP0 and SubP1
    LwU8 pa;
    for (pa = 0; pa < 2; pa++) {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(pa),gTT.bootTrainingTable.READ_ADR0);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADR(pa),gTT.bootTrainingTable.READ_PATTERN_PTR0);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(pa),gTT.bootTrainingTable.READ_ADR1);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADR(pa),gTT.bootTrainingTable.READ_PATTERN_PTR1);
    }

    return;
}

LwU8 addOffsetAndClampTo64(LwU32 val, LwS8 offset)
{
    LwS32 value = val + offset;
    if (value < 0)
    {
        value = 0;
    }
    if (value > 63)
    {
        value = 63;
    }
    return value;
}

void func_char_init_for_prbs(
        void
)
{
    func_setup_rd_prbs_regs();

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_CTRL,gTT.bootTrainingTable.CHAR_CTRL);

    // GDDR5_COMMANDS = 0 for CHAR PRBS initialization
    func_set_prbs_dual_mode(1,1);
    LwU8 training_ctrl_gddr5_commands = 0;
    memorySetTrainingControlGddr5Commands_HAL(&Fbflcn,training_ctrl_gddr5_commands);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL,gTT.bootTrainingTable.READ_CHAR_BANK_CTRL);

    // Check for Char Burst settings:
    // Halt if patram_burst_cnt is programmed incorrectly
    LwU32 charBankCtrl    = gTT.bootTrainingTable.READ_CHAR_BANK_CTRL;
    LwU8 charBankCtrlSetAMax     = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_A_MAX,charBankCtrl);
    LwU8 charBankCtrlSetAMin     = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_A_MIN,charBankCtrl);

    LwU8 charBankCtrlSetBMax     = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_B_MAX,charBankCtrl);
    LwU8 charBankCtrlSetBMin     = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL_SET_B_MIN,charBankCtrl);

    LwU32 charBankCtrl2 = gTT.bootTrainingTable.READ_CHAR_BANK_CTRL2;
    LwU8 CharBurstCntA         = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2_BURST_CNT_A,charBankCtrl2);
    LwU8 CharBurstCntB         = REF_VAL(LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2_BURST_CNT_B,charBankCtrl2);

    LwU32 charBurstCnt = ((charBankCtrlSetAMax - charBankCtrlSetAMin +1) * CharBurstCntA) +
            ((charBankCtrlSetBMax - charBankCtrlSetBMin +1) * CharBurstCntB);

    LwU32 trainingPatram = gTT.bootTrainingTable.READ_PATRAM;
    LwU32 patramPrbsBurstCnt   = REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_PRBS_BURST_CNT,trainingPatram);
    LwU8  trainingPrbsIlw     = REF_VAL(LW_PFB_FBPA_TRAINING_PATRAM_PRBS_ILW_EN, trainingPatram);

    if( (patramPrbsBurstCnt * (trainingPrbsIlw + 1)) != charBurstCnt) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_CHAR_SETTING_ERROR);
    }

    //func_run_training(addr=0, wck=0, rd=0, wr=1, periodic=0, char=1);
    LwU8 idx;
    LwU8 subp;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_TRAINING_CMD(0)), idx, subp, 0),0x888c0000);
                }
            }
        }
    }
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(1), 0x888c0000);

    gddr_wait_for_training_done();

    if(!gddr_check_training_passed()) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_CHAR_TRAINING_ERROR);
    }

    func_disable_char_bank_ctrl(); // Set BANK_CTRL = 0 after char as it interferes with RD/ WR training

    // Restoring gddr5_commands
    training_ctrl_gddr5_commands = 1;
    memorySetTrainingControlGddr5Commands_HAL(&Fbflcn,training_ctrl_gddr5_commands);

    // Disable prbs_dual_mode during address training
    func_set_prbs_dual_mode(0,1);
    // Restore for subsequent RD/WR PRBS tr
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BURST, 0x00000000);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2, 0x00002020);

    //Restore read settings in training patram
    LwU32 fbpa_training_patram = gTT.bootTrainingTable.READ_PATRAM;
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM, fbpa_training_patram);
}

void set_ldff_cmd_cnt
(
    void
)
{
    LwU32 training_rw_ldff_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_LDFF_CTRL);

    //In boot training LDFF depth is always 15 in G6X and 5 in G6
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        training_rw_ldff_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_RW_LDFF_CTRL, _CMD_CNT, _GDDR5_MAX,  training_rw_ldff_ctrl);
    }
    else {
        training_rw_ldff_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_RW_LDFF_CTRL, _CMD_CNT, _GDDR6X_MAX, training_rw_ldff_ctrl);
    }

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_LDFF_CTRL, training_rw_ldff_ctrl);
}

void enable_txeq
(
    void
)
{
    //TX EQ is enabled by setting SPARE[0] = 1 in the boot training table
    if (gTT.bootTrainingTable.SPAREDATA & 0x1) {
        LwU8 byte_base = 15; //Pin sub address for byte0
        LwU32 mrs9 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
        mrs9 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_TXEQ, _ENABLED, mrs9);
        LwU8 byte_idx;
        for(byte_idx = 0 ; byte_idx < 2; byte_idx++) {
            mrs9 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_PIN_SUB_ADDRESS, (byte_idx*16 + byte_base), mrs9);
            REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9, mrs9);
        }
    }
}

void enable_noise_gen
(
    void
)
{
    //Noise generation is enabled by setting SPARE[1] = 1 in the boot training table
    if ((gTT.bootTrainingTable.SPAREDATA >> 1) & 0x1) {
        LwU32 noise_cfg = REG_RD32(BAR0, LW_PFB_FBPA_NOISE_CFG1);
        noise_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_NOISE_CFG1, _ENABLE, 1,      noise_cfg);
        noise_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_NOISE_CFG1, _BANK,   0xFFEE, noise_cfg);
        REG_WR32(LOG, LW_PFB_FBPA_NOISE_CFG1, noise_cfg);

        LwU32 noise_cfg2 = 0x40;
        REG_WR32(LOG, LW_PFB_FBPA_NOISE_CFG2, noise_cfg2);
    }
}

void disable_noise_gen
(
    void
)
{
  LwU32 noise_cfg = REG_RD32(BAR0, LW_PFB_FBPA_NOISE_CFG1);
  noise_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_NOISE_CFG1, _ENABLE, 0,      noise_cfg);
  REG_WR32(LOG, LW_PFB_FBPA_NOISE_CFG1, noise_cfg);
}

void disable_pattern_shift_ilwert 
(
    void
)
{
    //Boot training shouldn't be doing pattern shift or ilwert but it may be set
    //by mclk switch code depending on the pstate tables for p0
    LwU32 training_ctrl2;
    training_ctrl2 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_WR, _DIS, training_ctrl2);
    training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_RD, _DIS, training_ctrl2);
    training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _ILW_PATT_WR,   _DIS, training_ctrl2);
    training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _ILW_PATT_RD,   _DIS, training_ctrl2);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2, training_ctrl2);
}

void disable_functional_edc
(
    void
)
{
    LwU32 fbpa_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _RDEDC, _DISABLED, fbpa_cfg0);
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _WREDC, _DISABLED, fbpa_cfg0);
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _REPLAY, _DISABLED, fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

    LwU32 generic_mrs4 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS4);
    generic_mrs4 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_RDCRC, _DISABLED, generic_mrs4);
    generic_mrs4 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_WRCRC, _DISABLED, generic_mrs4);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4, generic_mrs4);

    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
}

void set_vref_tracking_disable_training_updates
(
  LwBool disable 
)
{
  LwU32 vref_tracking = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_VREF_TRACKING);
  vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _DISABLE_TRAINING_UPDATES, disable, vref_tracking);
  REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING, vref_tracking);
}

void initialize_best_arrays
(
     void
)
{
    LwU16 idx;
    for (idx = 0; idx < TOTAL_DQ_BITS; idx++) {
        best_dfe_per_dq[idx] = 0;
        best_vref_per_dq_low[idx] = 0;
        best_vref_per_dq_mid[idx] = 0;
        best_vref_per_dq_high[idx] = 0;
    }
    for (idx = 0; idx < TOTAL_DBI_BITS; idx++) {
        best_dfe_per_dbi[idx] = 0;
        best_vref_per_dbi_low[idx] = 0;
        best_vref_per_dbi_mid[idx] = 0;
        best_vref_per_dbi_high[idx] = 0;
    }
}

void set_gpu_dfe
(
   LwBool enable
)
{
    LwU32 byte_pad_ctrl2;
    byte_pad_ctrl2 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2);
    byte_pad_ctrl2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _E_RX_DFE, enable, byte_pad_ctrl2);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2, byte_pad_ctrl2);
}

void set_rd_vref_tr (
    LwBool enable 
)
{
    LwU32 training_ctrl;
    training_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
    training_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CTRL, _VREF_TRAINING, enable, training_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL, training_ctrl);
}

void set_wr_vref_tr (
    LwBool enable
)
{
    LwU32 training_rw_wr_vref_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL);
    training_rw_wr_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_WR_VREF_CTRL, _EN, enable, training_rw_wr_vref_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL, training_rw_wr_vref_ctrl);
}

void set_pi_sweep (
    LwU8 direction
)
{
    LwU32 rw_cors_intrpltr_ctrl;
    LwU32 rw_fine_intrpltr_ctrl;

    rw_cors_intrpltr_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL);
    rw_fine_intrpltr_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL);

    if(direction == TRAINING_DIR_READ) {
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _MIN,     pi_cors_min_rd,     rw_cors_intrpltr_ctrl);
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _MAX,     pi_cors_max_rd,     rw_cors_intrpltr_ctrl);
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _STEP,    pi_cors_step_rd,    rw_cors_intrpltr_ctrl);
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _STEP_LN, pi_cors_step_ln_rd, rw_cors_intrpltr_ctrl);

        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _MIN,     pi_fine_min_rd,     rw_fine_intrpltr_ctrl);
        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _MAX,     pi_fine_max_rd,     rw_fine_intrpltr_ctrl);
        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _STEP,    pi_fine_step_rd,    rw_fine_intrpltr_ctrl);
        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _STEP_LN, pi_fine_step_ln_rd, rw_fine_intrpltr_ctrl);
    }

    if(direction == TRAINING_DIR_WRITE) {
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _MIN,     pi_cors_min_wr,     rw_cors_intrpltr_ctrl);
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _MAX,     pi_cors_max_wr,     rw_cors_intrpltr_ctrl);
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _STEP,    pi_cors_step_wr,    rw_cors_intrpltr_ctrl);
        rw_cors_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, _STEP_LN, pi_cors_step_ln_wr, rw_cors_intrpltr_ctrl);

        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _MIN,     pi_fine_min_wr,     rw_fine_intrpltr_ctrl);
        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _MAX,     pi_fine_max_wr,     rw_fine_intrpltr_ctrl);
        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _STEP,    pi_fine_step_wr,    rw_fine_intrpltr_ctrl);
        rw_fine_intrpltr_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, _STEP_LN, pi_fine_step_ln_wr, rw_fine_intrpltr_ctrl);
    }

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, rw_cors_intrpltr_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, rw_fine_intrpltr_ctrl);
}

void set_pi_offset_sweep (
    LwU8 direction
)
{
    LwU32 rw_pi_offset_ctrl;
    rw_pi_offset_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_PI_OFFSET_CTRL);

    if(direction == TRAINING_DIR_READ) {
        rw_pi_offset_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PI_OFFSET_CTRL, _MIN,  pi_offset_min_rd,  rw_pi_offset_ctrl);
        rw_pi_offset_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PI_OFFSET_CTRL, _MAX,  pi_offset_max_rd,  rw_pi_offset_ctrl);
        rw_pi_offset_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PI_OFFSET_CTRL, _STEP, pi_offset_step_rd, rw_pi_offset_ctrl);
    }

    if(direction == TRAINING_DIR_WRITE) {
        rw_pi_offset_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PI_OFFSET_CTRL, _MIN,  pi_offset_min_wr,  rw_pi_offset_ctrl);
        rw_pi_offset_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PI_OFFSET_CTRL, _MAX,  pi_offset_max_wr,  rw_pi_offset_ctrl);
        rw_pi_offset_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PI_OFFSET_CTRL, _STEP, pi_offset_step_wr, rw_pi_offset_ctrl);
    }

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_PI_OFFSET_CTRL, rw_pi_offset_ctrl);
}

void set_cors_step_override(
    LwBool enable
)
{
    LwU32 training_ctrl2;
    training_ctrl2 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    training_ctrl2 = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CTRL2, _CORS_STEP_OVERRIDE, enable, training_ctrl2);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2, training_ctrl2);
}

void set_gpu_dfe_sweep_nrz(
    LwU8 dfe_val
)
{
    LwU32 reg_write_val;
    LwU32 reg_addr;
    reg_write_val = 0;

    //Note: Even though we're using _PAD0..3 for these field sets, the same value applies to all registers and
    //fields regardless of the field name
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD0, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD1, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD2, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD3, dfe_val, reg_write_val);

    //Write all of the VREF_DFE_CTRL registers in a loop. This goes from
    //LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1 - LW_PFB_FBPA_FBIO_SUBP1BYTE3_VREF_DFE_CTRL3
    for(reg_addr = 0x009A1990 ; reg_addr <= 0x009A19EC ; reg_addr += 4) {
        REG_WR32(LOG, reg_addr, reg_write_val);
    }
}

void set_gpu_dfe_sweep_pam4(
    LwU8 dfe_val
)
{
    LwU32 reg_write_val;
    LwU32 reg_addr;
    reg_write_val = 0;

    //Note: Even though we're using _PAD0..8 for these field sets, the same value applies to all registers and
    //fields regardless of the field name
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD0, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD1, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD2, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD3, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD4, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD5, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD6, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD7, dfe_val, reg_write_val);
    reg_write_val = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_LOWER_DFE_CTRL1, _PAD8, dfe_val, reg_write_val);

    //Write all of the VREF_DFE_CTRL registers in a loop
    //Note: VREF_UPPER_DFE is in a different address range than LOWER and MID so there are 2 separate loops
    for(reg_addr = 0x009A1FC0; reg_addr <= 0x009A1FFC; reg_addr += 4) {
        REG_WR32(LOG, reg_addr, reg_write_val);
    }
    for(reg_addr = 0x009A1820; reg_addr <= 0x009A183C; reg_addr += 4) {
        REG_WR32(LOG, reg_addr, reg_write_val);
    }
}

void set_gpu_dfe_sweep(
    LwU8 dfe_val
)
{
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        set_gpu_dfe_sweep_nrz(dfe_val);
    }
    else if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        set_gpu_dfe_sweep_pam4(dfe_val);
    }
}

void set_vref_sweep_gpu_nrz
(
    void
)
{
    LwU32 training_rw_vref_ctrl;
    training_rw_vref_ctrl = 0;

    training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_VREF_CTRL, _MIN,  vref_min_rd,  training_rw_vref_ctrl);
    training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_VREF_CTRL, _MAX,  vref_max_rd,  training_rw_vref_ctrl);
    training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_VREF_CTRL, _STEP, vref_step_rd, training_rw_vref_ctrl);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL, training_rw_vref_ctrl);
}

void set_vref_sweep_gpu_pam4(
    LwU8 eye
)
{
    LwU32 training_rw_vref_ctrl = 0;
    LwU8 vref_min = 0;
    LwU8 vref_max = 0;

    switch (eye) {
        case EYE_LOW:
            vref_min = vref_min_low;
            vref_max = vref_max_low;
            break;
        case EYE_MID:
            vref_min = vref_min_mid;
            vref_max = vref_max_mid;
            break;
        case EYE_HIGH:
            vref_min = vref_min_high;
            vref_max = vref_max_high;
            break;
        default:
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE);
    }

    training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_VREF_CTRL, _MIN,  vref_min,     training_rw_vref_ctrl);
    training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_VREF_CTRL, _MAX,  vref_max,     training_rw_vref_ctrl);
    training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_VREF_CTRL, _STEP, vref_step_rd, training_rw_vref_ctrl);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL, training_rw_vref_ctrl);
}

void set_vref_sweep_gpu(
    LwU8 eye
)
{
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        set_vref_sweep_gpu_nrz();
    }
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        set_vref_sweep_gpu_pam4(eye);
    }
}

void set_eye_mask
(
    LwU8 eye,
    LwBool clear
)
{
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        //Only need eye masking for G6X
        LwU32 fbio_broadcast = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);

        // First clear whatever is stored in eye mask, so zero out
        // the 2-bit field
        LwU32 eye_mask_clear = ~(3 << 6);
        fbio_broadcast = fbio_broadcast & eye_mask_clear;

        //Now set the eye mask
        if (!clear) {
          LwU32 eye_mask = (eye + 1) << 6;
          fbio_broadcast = fbio_broadcast | eye_mask;
        }

        REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST, fbio_broadcast);
    }
} // End:set_eye_mask

void clear_receiver_mask
(
    void
)
{
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        //Only need receiver masking for G6X
        LwU32 fbio_broadcast = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);

        //Clear the receiver mask
        LwU32 rx_mask_clear = ~(3 << 21);
        fbio_broadcast = fbio_broadcast & rx_mask_clear;

        REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST, fbio_broadcast);
    }
}


void set_receiver_mask
(
    LwU8 vref
)
{
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        LwU32 fbio_broadcast = 0;
        LwU32 rx_mask = (vref + 1) << 21;

        //First clear the receiver mask, then set it
        clear_receiver_mask();
        fbio_broadcast = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);
        fbio_broadcast = fbio_broadcast | rx_mask;
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST, fbio_broadcast);
    }
}

void set_rd_vref_tr_area
(
    void
)
{
    LwU32 training_rw_vref_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL);
    training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_VREF_CTRL, _AREA_BASED, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_AREA_BASED_ENABLED, training_rw_vref_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL, training_rw_vref_ctrl);
}

void set_wr_vref_tr_area
(
   LwBool enable
)
{
    LwU32 training_rw_wr_vref_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL);
    training_rw_wr_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_WR_VREF_CTRL, _AREA_BASED, enable, training_rw_wr_vref_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL, training_rw_wr_vref_ctrl);
}

void set_tag_register
(
    LwBool first,
    LwBool enable,
    LwU32  tag
)
{
    LwU32 training_tag = 0;
    training_tag = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_TAG, _TAG,           tag,                                                 training_tag);
    training_tag = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_TAG, _STORE_OPTIMAL, LW_PFB_FBPA_TRAINING_TAG_STORE_OPTIMAL_GREATER_PIN,  training_tag);
    training_tag = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_TAG, _FIRST_TAG,     first,                                               training_tag);
    training_tag = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_TAG, _ENABLE,        enable,                                              training_tag);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TAG, training_tag);
}

void set_pi_offset_tr
(
    LwU8 direction
)
{
    LwU32 training_ctrl2 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    if(direction == TRAINING_DIR_READ) {
        training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _PI_OFFSET_RD, _EN, training_ctrl2);
    }
    if(direction == TRAINING_DIR_WRITE) {
        training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _PI_OFFSET_WR, _EN, training_ctrl2);
    }
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2, training_ctrl2);
}

void clear_pi_offset_tr
(
    void
)
{
    LwU32 training_ctrl2 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    LwU32 clear_pi_offset_mask = ~(3 << 8);
    training_ctrl2 = training_ctrl2 & clear_pi_offset_mask;
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2, training_ctrl2);
}

void read_per_pin_area
(
    LwU8 dfe,
    LwU8 vref
)
{
    //Base addresses for debug registers
    LwU32 debug_ctrl_base = 0x009A09C0;
    LwU32 debug_dq_base   = 0x00900938;
    LwU32 debug_ctrl_selection_base = 0x00000090;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 lwr_subp;
        for(lwr_subp = 0; lwr_subp < SUBPS; lwr_subp++) {
            LwU32 debug_ctrl_addr = debug_ctrl_base + (lwr_subp*4);
            LwU8 aclwm_area_idx;
            for(aclwm_area_idx = 0; aclwm_area_idx < 4; aclwm_area_idx++) {
                // Set debug ctrl register
                LwU32 debug_ctrl_selection = debug_ctrl_selection_base + aclwm_area_idx;
                REG_WR32(LOG, debug_ctrl_addr, debug_ctrl_selection);

                LwU8 debug_dq_idx;
                for(debug_dq_idx = 0; debug_dq_idx < 4; debug_dq_idx++) {
                    //Read debug DQ register
                    //These are addressed (in order) as DEBUG_DQ0_SUBP0, DEBUG_DQ0_SUBP1, DEBUG_DQ1_SUBP0, ...
                    //So when indexing, do SUBPS*4 + dq*8
                    LwU32 debug_dq_addr = debug_dq_base +
                                          ((pa_idx * 4 ) << 12) +
                                          (lwr_subp * 4) +
                                          (debug_dq_idx * 8 );

                    LwU32 debug_reg_read = REG_RD32(BAR0, debug_dq_addr);

                    LwU32 area_0 = debug_reg_read & 0x0000ffff;
                    LwU32 area_1 = (debug_reg_read & 0xffff0000) >> 16;

                    LwU16 index0 = (pa_idx * SUBPS * DQ_BITS_IN_SUBP) +
                                   (lwr_subp * DQ_BITS_IN_SUBP) +
                                   (aclwm_area_idx * 8) +
                                   (debug_dq_idx * 2);
                    LwU16 index1 = index0 + 1;

                    // Store in the array
                    if(vref == EYE_LOW) {
                        area_per_dq_low[index0][dfe] = area_0;
                        area_per_dq_low[index1][dfe] = area_1;
                    }
                    if(vref == EYE_MID) {
                        area_per_dq_mid[index0][dfe] = area_0;
                        area_per_dq_mid[index1][dfe] = area_1;
                    }
                    if(vref == EYE_HIGH) {
                        area_per_dq_high[index0][dfe] = area_0;
                        area_per_dq_high[index1][dfe] = area_1;
                    }
                }
            }
        }
    }
}

void read_per_dbi_area
(
    LwU8 dfe,
    LwU8 vref
)
{
    //Base addresses for debug registers
    LwU32 debug_ctrl_base = 0x009A09C0;
    LwU32 debug_dq_base   = 0x00900938;
    LwU32 debug_ctrl_selection_base = 0x00000094;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 lwr_subp;
        for(lwr_subp = 0; lwr_subp < SUBPS; lwr_subp++) {
            LwU32 debug_ctrl_addr = debug_ctrl_base + (lwr_subp*4);
            LwU8 aclwm_area_idx;
            for(aclwm_area_idx = 0; aclwm_area_idx < 1; aclwm_area_idx++) {
                // Set debug ctrl register
                LwU32 debug_ctrl_selection = debug_ctrl_selection_base + aclwm_area_idx;
                REG_WR32(LOG, debug_ctrl_addr, debug_ctrl_selection);

                LwU8 debug_dbi_idx;
                for(debug_dbi_idx = 0; debug_dbi_idx < 2; debug_dbi_idx++) {
                    //Read debug DQ register
                    //These are addressed (in order) as DEBUG_DQ0_SUBP0, DEBUG_DQ0_SUBP1, DEBUG_DQ1_SUBP0, ...
                    //So when indexing, do SUBPS*4 + dq*8
                    LwU32 debug_dq_addr = debug_dq_base +
                                          ((pa_idx * 4 ) << 12) +
                                          (lwr_subp * 4) +
                                          (debug_dbi_idx * 8 );

                    LwU32 debug_reg_read = REG_RD32(BAR0, debug_dq_addr);

                    LwU32 area_0 = debug_reg_read & 0x0000ffff;
                    LwU32 area_1 = (debug_reg_read & 0xffff0000) >> 16;

                    LwU16 index0 = (pa_idx * SUBPS * DBI_BITS_IN_SUBP) +
                                   (lwr_subp * DBI_BITS_IN_SUBP) +
                                   (aclwm_area_idx * 4) +
                                   (debug_dbi_idx * 2);
                    LwU16 index1 = index0 + 1;

                    // Store in the array
                    if(vref == EYE_LOW) {
                        area_per_dbi_low[index0][dfe] = area_0;
                        area_per_dbi_low[index1][dfe] = area_1;
                    }
                    if(vref == EYE_MID) {
                        area_per_dbi_mid[index0][dfe] = area_0;
                        area_per_dbi_mid[index1][dfe] = area_1;
                    }
                    if(vref == EYE_HIGH) {
                        area_per_dbi_high[index0][dfe] = area_0;
                        area_per_dbi_high[index1][dfe] = area_1;
                    }
                }
            }
        }
    }
}

// A generic function to run the moving average on one of the area_per_* arrays
// This function updates the area number based on moving average of 3. First
// area is average of 0 and 1, and the last area is average of last, last - 1
// The input *pArr is a pointer to all data for a single DQ|DBI index in area_per_*[][]
void area_array_moving_average (
    LwU8   dfe_min,
    LwU8   dfe_max,
    LwU8   dfe_step,
    LwU16  *pArr
)
{
    LwU8 dfe_idx;
    for(dfe_idx = dfe_min; dfe_idx <= dfe_max; dfe_idx += dfe_step) {
        temp_area[dfe_idx] = pArr[dfe_idx];
    }
    pArr[dfe_min] = (temp_area[dfe_min] + temp_area[dfe_min + dfe_step])/2;

    for(dfe_idx = (dfe_min + dfe_step); dfe_idx < (dfe_max - dfe_step); dfe_idx += dfe_step) {
        pArr[dfe_idx] = (temp_area[dfe_idx - dfe_step] +
                         temp_area[dfe_idx] +
                         temp_area[dfe_idx + dfe_step]) / 3;

    }
    pArr[dfe_max] = (temp_area[dfe_max] + temp_area[dfe_max - dfe_step])/2;
}

void update_area_moving_avg
(
    LwU8   dfe_min,
    LwU8   dfe_max,
    LwU8   dfe_step,
    LwBool en_dbi
)
{
    // this function updates the area number based on
    // moving average of 3. First area is average of
    // 0 and 1, and the last area is average of last, last -1
    LwU16 dq_idx;
    for(dq_idx = 0; dq_idx < TOTAL_DQ_BITS; dq_idx++) {
        area_array_moving_average(dfe_min, dfe_max, dfe_step, area_per_dq_low[dq_idx]);

        //If this is G6X, repeat the averaging loop for mid and high receivers
        if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
            area_array_moving_average(dfe_min, dfe_max, dfe_step, area_per_dq_mid[dq_idx]);
            area_array_moving_average(dfe_min, dfe_max, dfe_step, area_per_dq_high[dq_idx]);
        }
    } // Moving average done for all DQs

    // Repeat loop for DBI if needed
    if (en_dbi) {
        LwU32 dbi_idx;
        for(dbi_idx = 0; dbi_idx < TOTAL_DBI_BITS; dbi_idx++) {
            area_array_moving_average(dfe_min, dfe_max, dfe_step, area_per_dbi_low[dbi_idx]);

            //If this is G6X, repeat the averaging loop for mid and high receivers
            if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                area_array_moving_average(dfe_min, dfe_max, dfe_step, area_per_dbi_mid[dbi_idx]);
                area_array_moving_average(dfe_min, dfe_max, dfe_step, area_per_dbi_high[dbi_idx]);
            }
        } // Moving average done for all DBIs
    }
}

LwU32 min_of
(
    LwU32 a,
    LwU32 b,
    LwU32 c
)
{
    LwU32 min_val = b;
    if (a < b) {
        min_val = a;
    }
    if (c < min_val) {
        min_val = c;
    }
    return min_val;
}

void select_optimal_dfe_vref
(
    LwU8   min,
    LwU8   max,
    LwU8   step,
    LwBool en_dbi,
    LwU8   direction
)
{
    LwU32  lwrrent_max = 0;
    LwU8   lwrrent_best_dfe = 0;
    LwU32  min_area;
    LwBool found_nonzero_area[NUM_EYES_GDDR6X] = {LW_FALSE, LW_FALSE, LW_FALSE};

    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        // For GDDR6x - Find the smallest of the three eyes
        // and select the DFE which has the larges of the
        // smallest of the three eyes
        //In the case of write training, we only want to update best_vref_* and want to update EITHER DQ or DBI but not both
        //This is to keep the values in the best_* arrays accurate for debug purposes
        if ((direction == TRAINING_DIR_READ) || ((direction == TRAINING_DIR_WRITE) && !en_dbi)) {
            LwU16 dq_idx;
            for(dq_idx = 0; dq_idx < TOTAL_DQ_BITS; dq_idx++) {
                //Determine if this partition and subp are enabled
                LwU8 pa_idx   = (dq_idx / (DQ_BITS_IN_SUBP*SUBPS));
                if (!isPartitionEnabled(pa_idx)) { continue; }
                
                LwU8 subp_idx = ((dq_idx / DQ_BITS_IN_SUBP) % SUBPS);
                if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

                if (direction == TRAINING_DIR_READ) {
                    LwU8 dfe_idx;
                    for(dfe_idx = min; dfe_idx <= max; dfe_idx = dfe_idx + step) {
                        min_area = min_of(area_per_dq_low[dq_idx][dfe_idx], area_per_dq_mid[dq_idx][dfe_idx], area_per_dq_high[dq_idx][dfe_idx]);
                        if(min_area > lwrrent_max) {
                          lwrrent_max = min_area;
                          lwrrent_best_dfe = dfe_idx;
                        }

                        if (area_per_dq_low[dq_idx][dfe_idx]  > 0) {
                            found_nonzero_area[EYE_LOW] = LW_TRUE;
                        }
                        if (area_per_dq_mid[dq_idx][dfe_idx]  > 0) {
                            found_nonzero_area[EYE_MID] = LW_TRUE;
                        }
                        if (area_per_dq_high[dq_idx][dfe_idx] > 0) {
                            found_nonzero_area[EYE_HIGH] = LW_TRUE;
                        }
                    }

                    // Swept through all DFEs for all pins - Best DFE and corresponding
                    // index are now available for this pin. Store in the optimal array
                    best_dfe_per_dq[dq_idx] = best_dfe_per_dq[dq_idx] + lwrrent_best_dfe;

                    //Error check: If a DQ has 0 area across all DFE then we cannot train. Fail here
                    LwU8 eye_idx;
                    for (eye_idx = EYE_LOW; eye_idx < eyes; eye_idx++) {
                        if (found_nonzero_area[eye_idx] == LW_FALSE) {
                            FW_MBOX_WR32(12, 0x00000000); //Indicate that this is a DQ issue in MBOX 12
                            FW_MBOX_WR32(13, dq_idx); //Current DQ to MBOX 13 for debug
                            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_2STEP_PI_EYE_LOW_ERROR + eye_idx);
                        }
                        //Reset for next iteration
                        found_nonzero_area[eye_idx] = LW_FALSE;
                    }
                }
                best_vref_per_dq_low[dq_idx] = best_vref_per_dq_low[dq_idx] + vref_per_dq_low[dq_idx][lwrrent_best_dfe];
                best_vref_per_dq_mid[dq_idx] = best_vref_per_dq_mid[dq_idx] + vref_per_dq_mid[dq_idx][lwrrent_best_dfe];
                best_vref_per_dq_high[dq_idx] = best_vref_per_dq_high[dq_idx] + vref_per_dq_high[dq_idx][lwrrent_best_dfe];

                // Prepare for the next pin
                lwrrent_max = 0;
            }
        }

        if(en_dbi) {
            lwrrent_max = 0;

            LwU32 dbi_idx;
            for(dbi_idx = 0; dbi_idx < TOTAL_DBI_BITS; dbi_idx++) {
                //Determine if this partition and subp are enabled
                LwU8 pa_idx   = (dbi_idx / (DBI_BITS_IN_SUBP*SUBPS));
                if (!isPartitionEnabled(pa_idx)) { continue; }
                
                LwU8 subp_idx = ((dbi_idx / DBI_BITS_IN_SUBP) % SUBPS);
                if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

                if (direction == TRAINING_DIR_READ) {
                    LwU8 dfe_idx;
                    for(dfe_idx = min; dfe_idx <= max; dfe_idx = dfe_idx + step) {
                        min_area = min_of(area_per_dbi_low[dbi_idx][dfe_idx], area_per_dbi_mid[dbi_idx][dfe_idx], area_per_dbi_high[dbi_idx][dfe_idx]);
                        if(min_area > lwrrent_max) {
                            lwrrent_max = min_area;
                            lwrrent_best_dfe = dfe_idx;
                        }
                        if (area_per_dbi_low[dbi_idx][dfe_idx]  > 0) {
                            found_nonzero_area[EYE_LOW] = LW_TRUE;
                        }
                        if (area_per_dbi_mid[dbi_idx][dfe_idx]  > 0) {
                            found_nonzero_area[EYE_MID] = LW_TRUE;
                        }
                        if (area_per_dbi_high[dbi_idx][dfe_idx] > 0) {
                            found_nonzero_area[EYE_HIGH] = LW_TRUE;
                        }
                    }
                    // Swept through all DFEs for all pins - Best DFE and corresponding
                    // index are now available for this pin. Store in the optimal array
                    best_dfe_per_dbi[dbi_idx] = best_dfe_per_dbi[dbi_idx] + lwrrent_best_dfe;

                    //Error check: If a DQ has 0 area across all DFE then we cannot train. Fail here
                    LwU8 eye_idx;
                    for (eye_idx = EYE_LOW; eye_idx < eyes; eye_idx++) {
                        if (found_nonzero_area[eye_idx] == LW_FALSE) {
                            FW_MBOX_WR32(12, 0x00000DB1); //Indicate that this is a DBI issue in MBOX 12
                            FW_MBOX_WR32(13, dbi_idx);    //Current DQX to MBOX 13 for debug
                            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_2STEP_PI_EYE_LOW_ERROR + eye_idx);
                        }
                        //Reset for next iteration
                        found_nonzero_area[eye_idx] = LW_FALSE;
                    }
                }
                best_vref_per_dbi_low[dbi_idx] = best_vref_per_dbi_low[dbi_idx] + vref_per_dbi_low[dbi_idx][lwrrent_best_dfe];
                best_vref_per_dbi_mid[dbi_idx] = best_vref_per_dbi_mid[dbi_idx] + vref_per_dbi_mid[dbi_idx][lwrrent_best_dfe];
                best_vref_per_dbi_high[dbi_idx] = best_vref_per_dbi_high[dbi_idx] + vref_per_dbi_high[dbi_idx][lwrrent_best_dfe];

                // Prepare for the next pin
                lwrrent_max = 0;
            }
        }
    }
    else if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        lwrrent_max = 0 ;
        LwU16 dq_idx;
        for(dq_idx= 0 ; dq_idx < TOTAL_DQ_BITS; dq_idx++) {
            //Determine if this partition and subp are enabled
            LwU8 pa_idx   = (dq_idx / (DQ_BITS_IN_SUBP*SUBPS));
            if (!isPartitionEnabled(pa_idx)) { continue; }

            LwU8 subp_idx = ((dq_idx / DQ_BITS_IN_SUBP) % SUBPS);
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            LwU8 dfe_idx;
            for(dfe_idx = min; dfe_idx <= max ; dfe_idx = dfe_idx + step) {
                if(area_per_dq_low[dq_idx][dfe_idx] > lwrrent_max) {
                    lwrrent_max = area_per_dq_low[dq_idx][dfe_idx];
                    lwrrent_best_dfe = dfe_idx;
                }
            }

            best_dfe_per_dq[dq_idx] = best_dfe_per_dq[dq_idx] + lwrrent_best_dfe;
            best_vref_per_dq_low[dq_idx] = best_vref_per_dq_low[dq_idx] + vref_per_dq_low[dq_idx][lwrrent_best_dfe];
            lwrrent_max = 0;
        }

        if(en_dbi) {
            lwrrent_max = 0;
            LwU32 dbi_idx;
            for(dbi_idx= 0 ; dbi_idx < TOTAL_DBI_BITS; dbi_idx = dbi_idx +1 ) {
                //Determine if this partition and subp are enabled
                LwU8 pa_idx   = (dbi_idx / (DBI_BITS_IN_SUBP*SUBPS));
                if (!isPartitionEnabled(pa_idx)) { continue; }

                LwU8 subp_idx = ((dbi_idx / DBI_BITS_IN_SUBP) % SUBPS);
                if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

                LwU8 dfe_idx;
                for(dfe_idx = min; dfe_idx <= max ; dfe_idx = dfe_idx + step) {
                    if(area_per_dbi_low[dbi_idx][dfe_idx] > lwrrent_max) {
                        lwrrent_max = area_per_dbi_low[dbi_idx][dfe_idx];
                        lwrrent_best_dfe = dfe_idx;
                    }
                }

                best_dfe_per_dbi[dbi_idx] = best_dfe_per_dbi[dbi_idx] + lwrrent_best_dfe;
                best_vref_per_dbi_low[dbi_idx] = best_vref_per_dbi_low[dbi_idx] + vref_per_dbi_low[dbi_idx][lwrrent_best_dfe];
                lwrrent_max = 0;
            }
        }
    }
}

LwU32 round_average_value(
    LwU32 total,
    LwU8 n
)
{
    //We want to take the averaging results and round up. Floating point requires a library which takes too
    //much memory so instead do the rounding with integer math.
    //Instead, take total*2 and divide by N. Treat bit [0] as the 0.5 value and bits [31:1] as the result.
    //If [0] = 1, round up
    //Examples:
    //n=4, total = 1+1+2+2 = 6 => 6/4 = 1.5, want to round up. (6*2)/4 = 3 = 2'b11 => bit[0] = 1, [31:1] = 1 ==> round up to 2
    //n=4, total = 1+1+1+2 = 5 => 5/4 = 1.25, want to round down. (5*2)/4 = 2 = 2'b10 => bit[0] = 0, [31:1] = 1 ==> round down to 1
    LwU32 shifted_divide = (total << 1) / n;
    if ((shifted_divide & 0x1) == 1) {
        //Round up
        return ((total/n) + 1);
    }

    //Round down
    return (total/n);
}

void average_optimal_dfe_vref
(
    LwU8   n,
    LwBool en_dbi,
    LwBool only_dbi
)
{
    // FIXME - There can be more floor/ceil/int updates also to the averaging function
    if(!only_dbi) {
        LwU16 dq_idx;
        for(dq_idx = 0 ; dq_idx < TOTAL_DQ_BITS; dq_idx++) {
            best_dfe_per_dq[dq_idx]      = (round_average_value(best_dfe_per_dq[dq_idx],      n) & 0xFF);
            best_vref_per_dq_low[dq_idx] = (round_average_value(best_vref_per_dq_low[dq_idx], n) & 0xFFFF);
            if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                best_vref_per_dq_mid[dq_idx]  = (round_average_value(best_vref_per_dq_mid[dq_idx],  n) & 0xFFFF);
                best_vref_per_dq_high[dq_idx] = (round_average_value(best_vref_per_dq_high[dq_idx], n) & 0xFFFF);
            }
        }
    }

    if(en_dbi) {
        LwU32 dbi_idx;
        for(dbi_idx = 0 ; dbi_idx < TOTAL_DBI_BITS; dbi_idx++) {
            best_dfe_per_dbi[dbi_idx]      = (round_average_value(best_dfe_per_dbi[dbi_idx],      n) & 0xFF);
            best_vref_per_dbi_low[dbi_idx] = (round_average_value(best_vref_per_dbi_low[dbi_idx], n) & 0xFFFF);
            if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                best_vref_per_dbi_mid[dbi_idx]  = (round_average_value(best_vref_per_dbi_mid[dbi_idx],  n) & 0xFFFF);
                best_vref_per_dbi_high[dbi_idx] = (round_average_value(best_vref_per_dbi_high[dbi_idx], n) & 0xFFFF);
            }
        }
    }
} // End: average_optimal_dfe_vref

void add_dfe_vref_offsets 
(
    LwU8   direction,
    LwBool en_dbi,
    LwBool only_dbi
)
{
    BOOT_TRAINING_VREF_OFFSET1 *pVrefOffset1 = &gTT.bootTrainingTable.VREF_DFE_2;
    BOOT_TRAINING_VREF_OFFSET2 *pVrefOffset2 = &gTT.bootTrainingTable.VREF_DFE_3;

    LwS8 dfe_offset = signExtendByte(pVrefOffset1->dfe_offset_low, 2);
    LwS8 vref_offset_low;
    LwS8 vref_offset_mid;
    LwS8 vref_offset_high;
    if (direction == TRAINING_DIR_READ) {
        vref_offset_low  = signExtendByte(pVrefOffset1->vref_offset_low_ib,  5);
        vref_offset_mid  = signExtendByte(pVrefOffset1->vref_offset_mid_ib,  5);
        vref_offset_high = signExtendByte(pVrefOffset1->vref_offset_high_ib, 5);
    }
    else {
        vref_offset_low  = signExtendByte(pVrefOffset2->vref_offset_low_ob,  3);
        vref_offset_mid  = signExtendByte(pVrefOffset2->vref_offset_mid_ob,  3);
        vref_offset_high = signExtendByte(pVrefOffset2->vref_offset_high_ob, 3);
    }

    if(!only_dbi) {
        LwU16 dq_idx;

        for(dq_idx = 0 ; dq_idx < TOTAL_DQ_BITS; dq_idx++) {
            best_dfe_per_dq[dq_idx] += dfe_offset;
            best_vref_per_dq_low[dq_idx] += vref_offset_low;
            if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                best_vref_per_dq_mid[dq_idx]  += vref_offset_mid;
                best_vref_per_dq_high[dq_idx] += vref_offset_high;
            }
        }
    }

    if(en_dbi) {
        if (direction == TRAINING_DIR_READ) {
            vref_offset_high = signExtendByte((gTT.bootTrainingTable.SPAREDATA >> 28) & 0xF, 3);
            vref_offset_mid  = signExtendByte((gTT.bootTrainingTable.SPAREDATA >> 24) & 0xF, 3);
            vref_offset_low  = signExtendByte((gTT.bootTrainingTable.SPAREDATA >> 20) & 0xF, 3);
        }
        else {
            vref_offset_high = signExtendByte((gTT.bootTrainingTable.SPAREDATA >> 16) & 0xF, 3);
            vref_offset_mid  = signExtendByte((gTT.bootTrainingTable.SPAREDATA >> 12) & 0xF, 3);
            vref_offset_low  = signExtendByte((gTT.bootTrainingTable.SPAREDATA >> 8)  & 0xF, 3);
        }

        LwU32 dbi_idx;
        for(dbi_idx = 0 ; dbi_idx < TOTAL_DBI_BITS; dbi_idx++) {
            best_dfe_per_dbi[dbi_idx] += dfe_offset;
            best_vref_per_dbi_low[dbi_idx] += vref_offset_low;
            if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                best_vref_per_dbi_mid[dbi_idx]  += vref_offset_mid;
                best_vref_per_dbi_high[dbi_idx] += vref_offset_high;
            }
        }
    }
} // End: add_dfe_vref_offsets

void read_per_pin_vref_ib
(
    LwU8 dfe,
    LwU8 vref
)
{
    // This function reads the per-pin VREF programmed by training engine.
    // Since we are reading these value from FBIO, some amount of de-swizzling
    // will need to be handled in software
    LwU32 actual_swizzle_addr;
    LwU32 actual_vref_addr;
    LwU32 swizzle_addr_base = 0x009000A0;
    LwU8  swizzle_array[8];
    LwU8  vref_read_array[8];

    LwU8  byte_swizzle_array[SUBPS*BYTE_PER_SUBP];

    LwU8 final_vref_array[8];

    LwU32 vref_base_addr = 0x00901700;
    switch (vref) {
        case EYE_LOW:
            vref_base_addr = 0x00901700;
            break;
        case EYE_MID:
            vref_base_addr = 0x00901760;
            break;
        case EYE_HIGH:
            vref_base_addr = 0x009017C0;
            break;
        default:
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE);
    }

    LwU8 pa_idx;
    for(pa_idx = 0; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        actual_vref_addr    = vref_base_addr + ((pa_idx*4) << 12);

        // There is one byte swizzle register per partition Read it here and
        // populate byte swizzle. That will determine which bit swizzle
        // register to read later on
        memoryGetByteSwizzle_HAL(&Fbflcn,pa_idx,byte_swizzle_array);

        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { 
              //Update the address before moving to the next subp so we read the correct values
              actual_vref_addr += 4*BYTE_PER_SUBP*3; //+4/VREF * 3 VREF * BYTE_PER_SUBP
              continue; 
            }

            //actual_vref_addr    = actual_vref_addr + (subp_idx * 48);

            LwU32 byte_idx;
            for(byte_idx = 0; byte_idx < BYTE_PER_SUBP; byte_idx++) {
                actual_swizzle_addr = swizzle_addr_base + ((pa_idx*4) << 12) 
                                                        + (subp_idx*16) 
                                                        + ((byte_swizzle_array[subp_idx*BYTE_PER_SUBP + byte_idx] - subp_idx*4)*4);
                //actual_vref_addr    = actual_vref_addr + (byte_idx * 12);

                // Read the swizzle register
                LwU32 bit_swizzle_read = REG_RD32(BAR0, actual_swizzle_addr);

                // create an array of this swizzle value
                LwU8 bit_idx;
                for(bit_idx = 0 ; bit_idx < 8; bit_idx++) {
                    LwU32 swizzle_mask = 15 << (bit_idx * 4);
                    LwU32 swizzle_data = (bit_swizzle_read & swizzle_mask) >> (bit_idx * 4);
                    if(swizzle_data > 4) {
                        swizzle_data = swizzle_data - 1;
                    }
                    swizzle_array[bit_idx] = swizzle_data;
                }

                // swizzle values are now in swizzle_array for this byte
                // Each byte has three VREF registers
                LwU8 vref_idx;
                for(vref_idx = 0; vref_idx < 3; vref_idx++) {
                    LwU32 pad_vref_read = REG_RD32(BAR0, actual_vref_addr);
                    actual_vref_addr = actual_vref_addr + 4; //Move to the next address for the next loop iteration

                    LwU8 pad_vref0 = pad_vref_read & 0x000000ff;
                    LwU8 pad_vref1 = (pad_vref_read & 0x0000ff00) >> 8;
                    LwU8 pad_vref2 = (pad_vref_read & 0x00ff0000) >> 16;
                    LwU8 pad_vref3 = (pad_vref_read & 0xff000000) >> 24;
                    if(vref_idx == 0) {
                        vref_read_array[0] = pad_vref0;
                        vref_read_array[1] = pad_vref1;
                        vref_read_array[2] = pad_vref2;
                        vref_read_array[3] = pad_vref3;
                    }
                    if(vref_idx == 1) {
                        vref_read_array[4] = pad_vref1;
                        vref_read_array[5] = pad_vref2;
                        vref_read_array[6] = pad_vref3;
                    }
                    if(vref_idx == 2) {
                        vref_read_array[7] = pad_vref0;
                    }
                }

                LwU32 vref_array_base_index = pa_idx*SUBPS*BYTE_PER_SUBP*8 +
                                              subp_idx *   BYTE_PER_SUBP*8 +
                                              byte_idx *   8;

                LwU8 logical_dq;
                for(bit_idx = 0; bit_idx < 8 ; bit_idx = bit_idx +1) {
                    logical_dq = swizzle_array[bit_idx];
                    final_vref_array[logical_dq] = vref_read_array[bit_idx];
                }

                for(bit_idx = 0; bit_idx < 8 ; bit_idx = bit_idx + 1) {
                    if(vref == EYE_LOW) {
                        vref_per_dq_low[vref_array_base_index + bit_idx][dfe] = final_vref_array[bit_idx];
                    }

                    if(vref == EYE_MID) {
                        vref_per_dq_mid[vref_array_base_index + bit_idx][dfe] = final_vref_array[bit_idx];
                    }

                    if(vref == EYE_HIGH) {
                        vref_per_dq_high[vref_array_base_index + bit_idx][dfe] = final_vref_array[bit_idx];
                    }
                }
            }
        }
    }
}


void read_per_dbi_vref_ib
(
    LwU8 dfe,
    LwU8 vref,
    LwBool is_edc
)
{
    // This function reads the per-dbi VREF programmed by training engine.
    // DBI/DQX is not expected to be swizzled
    LwU32 actual_vref_addr;

    LwU32 vref_base_addr = 0x00901700;
    switch (vref) {
        case EYE_LOW:
            vref_base_addr = 0x00901700;
            break;
        case EYE_MID:
            vref_base_addr = 0x00901760;
            break;
        case EYE_HIGH:
            vref_base_addr = 0x009017C0;
            break;
        default:
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_ILWALID_EYE);
    }

    LwU8 pa_idx;
    for(pa_idx = 0; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        //actual_vref_addr    = vref_base_addr + ((pa_idx*4) << 12);

        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            //actual_vref_addr    = actual_vref_addr + (subp_idx * 48);

            LwU8 byte_idx;
            for(byte_idx = 0; byte_idx < BYTE_PER_SUBP; byte_idx++) {
                //actual_vref_addr    = actual_vref_addr + (byte_idx * 12) + 4;
                //DBI is in VREF_CODE2_PAD4
                actual_vref_addr    = vref_base_addr + ((pa_idx*4) << 12) 
                                                     + (subp_idx * 48)
                                                     + (byte_idx * 12) 
                                                     + 4;

                //If reading EDC it is in VREF_CODE3_PAD9
                if (is_edc) {
                  actual_vref_addr += 4;
                }
                LwU32 pad_vref_read = REG_RD32(BAR0, actual_vref_addr);

                //Extract out EDC (bits [15:8]) or DBI/DQX (bits [7:0])
                LwU8  pad_vref0 = is_edc ? ((pad_vref_read >> 8) & 0x000000ff) :
                                           (pad_vref_read & 0x000000ff);
                LwU32 vref_array_base_index = pa_idx*SUBPS*BYTE_PER_SUBP +
                                              subp_idx * BYTE_PER_SUBP  +
                                              byte_idx ;

                if (is_edc) {
                  //We only care about EDC for the low eye
                  //We aren't sweeping over DFE so no need to track it
                  vref_per_edc_low[vref_array_base_index] = pad_vref0;
                }
                else {
                  if(vref == EYE_LOW) {
                      vref_per_dbi_low[vref_array_base_index][dfe] = pad_vref0;
                  }

                  if(vref == EYE_MID) {
                      vref_per_dbi_mid[vref_array_base_index][dfe] = pad_vref0;
                  }

                  if(vref == EYE_HIGH) {
                      vref_per_dbi_high[vref_array_base_index][dfe] = pad_vref0;
                  }
                }
            }
        }
    }
}

void set_cmd_hold_update
(
    LwBool enable
)
{
    // This register is written only by HW, so it should generally have
    // the same value. Using subp1 value here. Generally good to use
    // subp1 value for cases where both subp0 and subp1 values are
    // expected to be same and we are reading just one for RMW.
    LwU32 priv_cmd = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD);
    priv_cmd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_PRIV_CMD, _HOLD_UPDATE,        enable, priv_cmd);
    //priv_cmd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_PRIV_CMD, _DISABLE_DQS_UPDATE, enable, priv_cmd);

    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_PRIV_CMD, priv_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD, priv_cmd);
}

void program_final_dfe_gpu_nrz
(
    void
)
{
    // Program final DFE for NRZ case
    // best optimal DFE is stored per pin in best_dfe_per_dq and
    // best_dfe_per_dbi
    LwU32 dfe_base_addr = 0x00901990;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 dfe_actual_addr = dfe_base_addr + ((pa_idx * 4) << 12);
        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx = subp_idx + 1) {
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { 
              //Update the target address so we program subp1 correctly
              dfe_actual_addr += 4*BYTE_PER_SUBP*3; //+4/DFE * 3 DFE * BYTE_PER_SUBP
              continue; 
            }

            //dfe_actual_addr = dfe_actual_addr + (subp_idx * 48);
            LwU8 byte_idx;
            for(byte_idx = 0; byte_idx < BYTE_PER_SUBP; byte_idx++) {
                //dfe_actual_addr = dfe_actual_addr + (byte_idx*12);
                // Each byte has 3 DFE registers
                LwU8 dfe_idx;
                for(dfe_idx = 0; dfe_idx < 3; dfe_idx++) {
                    // Each register can have 4 pads
                    LwU32 dfe_pad0 = 0;
                    LwU32 dfe_pad1 = 0;
                    LwU32 dfe_pad2 = 0;
                    LwU32 dfe_pad3 = 0;
                    if(dfe_idx == 0) {
                        LwU32 index0 = pa_idx*SUBPS*BYTE_PER_SUBP*8 +
                                       subp_idx*BYTE_PER_SUBP*8 +
                                       byte_idx*8;
                        dfe_pad0 = best_dfe_per_dq[index0];
                        dfe_pad1 = best_dfe_per_dq[index0+1];
                        dfe_pad2 = best_dfe_per_dq[index0+2];
                        dfe_pad3 = best_dfe_per_dq[index0+3];
                    }

                    if(dfe_idx == 1) {
                        // This one has DBI/DQX in it
                        LwU32 index0 = pa_idx*SUBPS*BYTE_PER_SUBP +
                                       subp_idx*BYTE_PER_SUBP +
                                       byte_idx;
                        LwU32 index1 = pa_idx*SUBPS*BYTE_PER_SUBP*8 +
                                       subp_idx*BYTE_PER_SUBP*8 +
                                       byte_idx*8 + 4;

                        dfe_pad0 = best_dfe_per_dbi[index0];
                        dfe_pad1 = best_dfe_per_dq[index1];
                        dfe_pad2 = best_dfe_per_dq[index1+1];
                        dfe_pad3 = best_dfe_per_dq[index1+2];
                    }

                    if(dfe_idx == 2) {
                        LwU32 index0 = pa_idx*SUBPS*BYTE_PER_SUBP*8 +
                                       subp_idx * BYTE_PER_SUBP * 8 +
                                       byte_idx * 8 + 7;
                        dfe_pad0 = best_dfe_per_dq[index0];
                        dfe_pad1 = 0;
                        dfe_pad2 = 0;
                        dfe_pad3 = 0;
                    }

                    //Build up register write data - use a generic register here just to make field names work
                    LwU32 dfe_write_data = 0;
                    dfe_write_data = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD0, dfe_pad0, dfe_write_data);
                    dfe_write_data = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD1, dfe_pad1, dfe_write_data);
                    dfe_write_data = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD2, dfe_pad2, dfe_write_data);
                    dfe_write_data = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD3, dfe_pad3, dfe_write_data);

                    REG_WR32(LOG, dfe_actual_addr, dfe_write_data);
                    dfe_actual_addr = dfe_actual_addr + 4; //Move to the next DFE address for the next loop iteration
                }
            }
        }
    }
}

void program_final_dfe_gpu_pam4
(
    void
)
{
    // Program final DFE for PAM-4.
    // Even though there is per-eye DFE control at this point, we are going to
    // be programming the same value for all eyes

    LwU32 dfe_low_base = 0x00901FC0;
    LwU32 dfe_mid_base = 0x00901FE0;
    LwU32 dfe_high_base = 0x00901820;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 dfe_actual_addr_low  = dfe_low_base  + ((pa_idx * 4) << 12);
        LwU32 dfe_actual_addr_mid  = dfe_mid_base  + ((pa_idx * 4) << 12);
        LwU32 dfe_actual_addr_high = dfe_high_base + ((pa_idx * 4) << 12);

        LwU8 subp_idx;
        for(subp_idx = 0 ; subp_idx < SUBPS; subp_idx++) {
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) {
              //Update the DFE register addresses so we correctly program subp1
              //In the loop below we add 4 for every byte to move to the next priv
              dfe_actual_addr_low  += 4*BYTE_PER_SUBP;
              dfe_actual_addr_mid  += 4*BYTE_PER_SUBP;
              dfe_actual_addr_high += 4*BYTE_PER_SUBP;
              continue;
            }

            //dfe_actual_addr_low = dfe_actual_addr_low + subp_idx*16;
            //dfe_actual_addr_mid = dfe_actual_addr_mid + subp_idx*16;
            //dfe_actual_addr_high = dfe_actual_addr_high + subp_idx*16;

            LwU8 byte_idx;
            for(byte_idx = 0 ; byte_idx < BYTE_PER_SUBP; byte_idx++) {
                LwU32 base_dq_index = pa_idx*SUBPS*BYTE_PER_SUBP*8 +
                                      subp_idx*BYTE_PER_SUBP*8 +
                                      byte_idx*8;
                LwU32 base_dbi_index = pa_idx * SUBPS * BYTE_PER_SUBP +
                                       subp_idx * BYTE_PER_SUBP +
                                       byte_idx;

                LwU8 shift_index;
                LwU32 dfe_write_data = 0;
                LwU8 bit_idx;
                for(bit_idx = 0; bit_idx < 8; bit_idx++) {
                    if(bit_idx > 3) {
                        shift_index = (bit_idx + 1) * 3;
                    } else {
                        shift_index = bit_idx * 3;
                    }
                    dfe_write_data = dfe_write_data | (best_dfe_per_dq[base_dq_index + bit_idx] << shift_index);
                }
                // Add in DBI/DQX
                dfe_write_data = dfe_write_data | (best_dfe_per_dbi[base_dbi_index] << 12);

                REG_WR32(LOG, dfe_actual_addr_low,  dfe_write_data);
                REG_WR32(LOG, dfe_actual_addr_mid,  dfe_write_data);
                REG_WR32(LOG, dfe_actual_addr_high, dfe_write_data);

                //Move to the next DFE addresses to be used in the next loop iteration
                dfe_actual_addr_low = dfe_actual_addr_low + 4;
                dfe_actual_addr_mid = dfe_actual_addr_mid + 4;
                dfe_actual_addr_high = dfe_actual_addr_high + 4;
            }
        }
    }
}

void program_final_dfe_gpu
(
    void
)
{
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        program_final_dfe_gpu_nrz();
    }

    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
        program_final_dfe_gpu_pam4();
    }
}

void program_final_vref_gpu
(
     void
)
{
    LwU32 vref_base_low  = 0x00901700;
    LwU32 vref_base_mid  = 0x00901760;
    LwU32 vref_base_high  = 0x009017C0;

    LwU32 vref_write_data;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU32 vref_actual_addr_low = vref_base_low + ((pa_idx * 4) << 12);
        LwU32 vref_actual_addr_mid = vref_base_mid + ((pa_idx * 4) << 12);
        LwU32 vref_actual_addr_high = vref_base_high + ((pa_idx * 4) << 12);
        LwU8 subp_idx;
        for(subp_idx = 0 ; subp_idx < SUBPS; subp_idx++) {
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) {
              //Update the VREF register addresses so we correctly program subp1
              //In the loop below we add 4 for every VREF (3) to move to the next priv
              //and we do this for each byte (BYTE_PER_SUBP)
              vref_actual_addr_low  += 4*BYTE_PER_SUBP*3;
              vref_actual_addr_mid  += 4*BYTE_PER_SUBP*3;
              vref_actual_addr_high += 4*BYTE_PER_SUBP*3;
              continue;
            }

            //vref_actual_addr_low = vref_actual_addr_low + (subp_idx * 48);
            //vref_actual_addr_mid = vref_actual_addr_mid + (subp_idx * 48);
            //vref_actual_addr_high = vref_actual_addr_high + (subp_idx * 48);
            
            LwU8 byte_idx;
            for(byte_idx = 0 ; byte_idx < BYTE_PER_SUBP; byte_idx++) {
                //vref_actual_addr_low = vref_actual_addr_low + byte_idx*12;
                //vref_actual_addr_mid = vref_actual_addr_mid + byte_idx*12;
                //vref_actual_addr_high = vref_actual_addr_high + byte_idx*12;
                LwU8 vref_idx;
                for(vref_idx = 0 ; vref_idx < 3; vref_idx++) {
                    //vref_actual_addr_low = vref_actual_addr_low + byte_idx*12 + vref_idx*4;
                    //vref_actual_addr_mid = vref_actual_addr_mid + byte_idx*12 + vref_idx*4;
                    //vref_actual_addr_high = vref_actual_addr_high + byte_idx*12 + vref_idx*4;
                    if(vref_idx == 0 ) {
                        LwU32 index0 = pa_idx * SUBPS * BYTE_PER_SUBP * 8 +
                                       subp_idx * BYTE_PER_SUBP * 8 +
                                       byte_idx * 8;

                        vref_write_data = 0;
                        vref_write_data =  best_vref_per_dq_low[index0] |
                                          (best_vref_per_dq_low[index0+1] << 8) |
                                          (best_vref_per_dq_low[index0+2] << 16) |
                                          (best_vref_per_dq_low[index0+3] << 24);
                        REG_WR32(LOG, vref_actual_addr_low , vref_write_data);

                        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                            vref_write_data = 0;
                            vref_write_data =  best_vref_per_dq_mid[index0] |
                                              (best_vref_per_dq_mid[index0+1] << 8) |
                                              (best_vref_per_dq_mid[index0+2] << 16) |
                                              (best_vref_per_dq_mid[index0+3] << 24);
                            REG_WR32(LOG, vref_actual_addr_mid, vref_write_data);

                            vref_write_data = 0;
                            vref_write_data =  best_vref_per_dq_high[index0] |
                                              (best_vref_per_dq_high[index0+1] << 8) |
                                              (best_vref_per_dq_high[index0+2] << 16) |
                                              (best_vref_per_dq_high[index0+3] << 24);
                            REG_WR32(LOG, vref_actual_addr_high, vref_write_data);
                        }
                    }

                    if (vref_idx == 1) {
                        // This one has DBI/DQX programming
                        LwU32 index0 = pa_idx * SUBPS * BYTE_PER_SUBP +
                                       subp_idx * BYTE_PER_SUBP +
                                       byte_idx;

                        LwU32 index1 = pa_idx * SUBPS * BYTE_PER_SUBP * 8 +
                                       subp_idx* BYTE_PER_SUBP * 8 +
                                       byte_idx * 8 +
                                       4;

                        vref_write_data = 0;
                        vref_write_data =  best_vref_per_dbi_low[index0] |
                                          (best_vref_per_dq_low[index1] << 8) |
                                          (best_vref_per_dq_low[index1+1] << 16) |
                                          (best_vref_per_dq_low[index1+2] << 24);
                        REG_WR32(LOG, vref_actual_addr_low , vref_write_data);

                        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                            vref_write_data = 0;
                            vref_write_data =  best_vref_per_dbi_mid[index0] |
                                              (best_vref_per_dq_mid[index1] << 8) |
                                              (best_vref_per_dq_mid[index1+1] << 16) |
                                              (best_vref_per_dq_mid[index1+2] << 24);
                            REG_WR32(LOG, vref_actual_addr_mid, vref_write_data);

                            vref_write_data = 0;
                            vref_write_data =  best_vref_per_dbi_high[index0] |
                                              (best_vref_per_dq_high[index1] << 8) |
                                              (best_vref_per_dq_high[index1+1] << 16) |
                                              (best_vref_per_dq_high[index1+2] << 24);

                            REG_WR32(LOG, vref_actual_addr_high, vref_write_data);
                        }
                    }

                    if (vref_idx == 2) {
                        LwU32 index0  = pa_idx * SUBPS * BYTE_PER_SUBP * 8 +
                                        subp_idx* BYTE_PER_SUBP * 8 +
                                        byte_idx * 8 +
                                        7;
                        LwU32 edc_idx = pa_idx * SUBPS * BYTE_PER_SUBP +
                                        subp_idx * BYTE_PER_SUBP +
                                        byte_idx;

                        vref_write_data = 0;
                        vref_write_data = best_vref_per_dq_low[index0] |
                                          (vref_per_edc_low[edc_idx] << 8);
                        REG_WR32(LOG, vref_actual_addr_low , vref_write_data);

                        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
                            vref_write_data = 0;
                            vref_write_data = best_vref_per_dq_mid[index0];
                            REG_WR32(LOG, vref_actual_addr_mid, vref_write_data);

                            vref_write_data = 0;
                            vref_write_data = best_vref_per_dq_high[index0];
                            REG_WR32(LOG, vref_actual_addr_high, vref_write_data);
                        }
                    }

                    //Update addresses for the next loop iteration
                    vref_actual_addr_low = vref_actual_addr_low +   4;
                    vref_actual_addr_mid = vref_actual_addr_mid +   4;
                    vref_actual_addr_high = vref_actual_addr_high + 4;
                } //vref_idx
            } //byte_idx
        } //subp_idx
    } //pa_idx
}

void set_dram_dbi
(
    LwBool enable
)
{
    LwU32 mrs1 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS1);
    //Note: DBI is active low so ilwert the enable bit
    mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_RDBI, ~enable, mrs1);
    mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_WDBI, ~enable, mrs1);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1, mrs1);
}

void set_dram_mta
(
    LwBool enable
)
{
    LwU32 mrs1 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS1);
    mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR6X_MTA, enable, mrs1);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1, mrs1);
}

void set_rd_dbi_expected 
(
    LwBool enable
)
{
    LwU32 training_ctrl2 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    training_ctrl2 = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CTRL2, _USE_RD_DBI_EXPECTED, enable, training_ctrl2);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2, training_ctrl2);
}

void set_vref_sweep_dram
(
    void
)
{
    LwU32 training_rw_wr_vref_ctrl = 0;
    training_rw_wr_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_WR_VREF_CTRL, _MIN,  vref_min_wr,  training_rw_wr_vref_ctrl);
    training_rw_wr_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_WR_VREF_CTRL, _MAX,  vref_max_wr,  training_rw_wr_vref_ctrl);
    training_rw_wr_vref_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_WR_VREF_CTRL, _STEP, vref_step_wr, training_rw_wr_vref_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL, training_rw_wr_vref_ctrl);
}

void set_receiver_mask_dram
(
    LwU8   vref,
    LwBool reset_mask
)
{
    //OPTIMIZE - Can directly use (vref+1) to set mask value
    LwU8 mask_value = 0;
    if (!reset_mask) {
        switch (vref) {
            case EYE_LOW:
                mask_value = 1;
                break;
            case EYE_MID:
                mask_value = 2;
                break;
            case EYE_HIGH:
                mask_value = 3;
                break;
            default:
                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_ILWALID_EYE);
        }
    }

    LwU8 byte_base = 14; //Pin sub address for byte0
    LwU32 mrs9 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
    mrs9 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_RX_MASK,         mask_value,                mrs9);
    LwU8 byte_idx;
    for(byte_idx = 0 ; byte_idx < 2; byte_idx++) {
        mrs9 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_PIN_SUB_ADDRESS, (byte_idx*16 + byte_base), mrs9);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9, mrs9);
    }
}

void read_per_pin_vref_ob
(
    LwU8 dfe,
    LwU8 vref
)
{
    LwU32 debug_ctrl_base = 0x009A09C0;
    LwU32 debug_dq_base = 0x00900938;
    LwU32 write_vref_mask = 0x000ff000;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        //LwU32 debug_dq_base_addr = debug_dq_base + ((pa_idx * 4) << 12);
        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            //debug_dq_base_addr = debug_dq_base_addr + (subp_idx*4);
            LwU32 debug_dq_base_addr = debug_dq_base + ((pa_idx*4) << 12) + (subp_idx*4);
            LwU32 debug_ctrl_addr = debug_ctrl_base + (subp_idx * 4);
            // Now we need to set the debug ctrl from 0x30 to 0x3B to read out the VREF
            LwU8 num_byte = 0;
            LwU8 select_idx;
            for(select_idx = 0x30 ; select_idx <= 0x39; select_idx = select_idx + 3) {
                LwU8 select_idx_offset;
                for(select_idx_offset = 0 ; select_idx_offset < 3; select_idx_offset++) {
                    // setup debug ctrl
                    LwU32 debug_ctrl_select = select_idx + select_idx_offset;
                    REG_WR32(LOG, debug_ctrl_addr, debug_ctrl_select);

                    // Now read debug DQ registers
                    if(select_idx_offset == 0) {
                        LwU8 bit_idx;
                        for(bit_idx = 0 ; bit_idx < 4; bit_idx = bit_idx + 1) {
                            LwU32 debug_dq_final_addr = debug_dq_base_addr + (bit_idx * 8);
                            LwU32 read_vref = REG_RD32(BAR0, debug_dq_final_addr);
                            LwU8  vref0 = (read_vref & write_vref_mask) >> 12;
                            LwU16 index0 = (pa_idx * SUBPS * DQ_BITS_IN_SUBP) + (subp_idx * DQ_BITS_IN_SUBP) + (num_byte * 8) + bit_idx;
                            if(vref == EYE_LOW) {
                                vref_per_dq_low[index0][dfe] = vref0;
                            }
                            if(vref == EYE_MID) {
                                vref_per_dq_mid[index0][dfe] = vref0;
                            }
                            if(vref == EYE_HIGH) {
                                vref_per_dq_high[index0][dfe] = vref0;
                            }
                        }
                    }

                    if(select_idx_offset == 1) {
                        // DEBUB_DQ0 will carry DBI/DQX, which we do not want here
                        LwU8 bit_idx;
                        for (bit_idx = 1; bit_idx < 4; bit_idx = bit_idx + 1) {
                            LwU32 debug_dq_final_addr = debug_dq_base_addr + (bit_idx * 8);
                            LwU32 read_vref = REG_RD32(BAR0, debug_dq_final_addr);
                            LwU8  vref0 = (read_vref & write_vref_mask) >> 12;
                            LwU16 index0 = (pa_idx * SUBPS * DQ_BITS_IN_SUBP) + (subp_idx * DQ_BITS_IN_SUBP) + (num_byte * 8) + 4 + (bit_idx - 1);
                            if(vref == EYE_LOW) {
                                vref_per_dq_low[index0][dfe] = vref0;
                            }
                            if(vref == EYE_MID) {
                                vref_per_dq_mid[index0][dfe] = vref0;
                            }
                            if(vref == EYE_HIGH) {
                                vref_per_dq_high[index0][dfe] = vref0;
                            }
                        }
                    }

                    if(select_idx_offset == 2) {
                        // Only need to read DEBUB_DQ0 for the last DQ of this byte
                        LwU32 debug_dq_final_addr = debug_dq_base_addr;
                        LwU32 read_vref = REG_RD32(BAR0, debug_dq_final_addr);
                        LwU8  vref0 = (read_vref & write_vref_mask) >> 12;
                        LwU16 index0 = (pa_idx * SUBPS * DQ_BITS_IN_SUBP) + (subp_idx * DQ_BITS_IN_SUBP) + (num_byte * 8) + 7;
                        if(vref == EYE_LOW) {
                            vref_per_dq_low[index0][dfe] = vref0;
                        }
                        if(vref == EYE_MID) {
                            vref_per_dq_mid[index0][dfe] = vref0;
                        }
                        if(vref == EYE_HIGH) {
                            vref_per_dq_high[index0][dfe] = vref0;
                        }
                    }
                }
                num_byte = num_byte + 1;
            }
        }
    }
}

void read_per_pin_vref_ob_dbi
(
    LwU8 dfe,
    LwU8 vref
)
{
    // In this case, need to read a single debug_dq register 
    // for DBI/DQX VREF
    LwU32 debug_ctrl_addr_base = 0x009A09C0;
    LwU32 debug_dq0_base = 0x00900938;
    LwU32 vref_mask = 0x000ff000;
    LwU8  debug_ctrl_value_base = 0x31;

    LwU8 pa_idx;
    for(pa_idx = 0 ; pa_idx < MAX_PARTS ; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        LwU8 subp_idx;
        for (subp_idx = 0 ; subp_idx < SUBPS; subp_idx++) {
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            LwU32 debug_ctrl_addr = debug_ctrl_addr_base + subp_idx*4;
            LwU32 debug_dq0_addr  = debug_dq0_base + 
                                    ((pa_idx*4) << 12) +
                                    subp_idx*4;

            LwU8 byte_idx;
            for (byte_idx = 0; byte_idx < 4; byte_idx++) {
                LwU32 debug_ctrl_value = debug_ctrl_value_base + byte_idx*3;
                REG_WR32(LOG, debug_ctrl_addr, debug_ctrl_value);

                LwU32 reg_read = REG_RD32(BAR0, debug_dq0_addr);
                LwU8  vref_dbi = (reg_read & vref_mask) >> 12;

                LwU16 index = pa_idx*SUBPS*BYTE_PER_SUBP +
                              subp_idx*BYTE_PER_SUBP +
                              byte_idx;

                if(vref == EYE_LOW) {
                    vref_per_dbi_low[index][dfe] = vref_dbi;
                }
                if(vref == EYE_MID) {
                    vref_per_dbi_mid[index][dfe] = vref_dbi;
                }
                if(vref == EYE_HIGH) {
                    vref_per_dbi_high[index][dfe] = vref_dbi;
                }
            } // bytes = 4 per SUBPS. 1 dbi/dqx per byte
        } // SUBPS = 2 per PA
    } // PAs
}

void program_optimal_vref_dq_ob
(
    void
)
{
    LwU32 mrs6_base = 0x00900360;
    LwU32 mrs15_store = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);

    LwU8 pa_idx;
    for(pa_idx = 0; pa_idx < MAX_PARTS ; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        //LwU32 mrs6_addr = mrs6_base + ((pa_idx * 4) << 12);
        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            //SW will use subp0 and supb1 versions of MRS6 to prgram the final VREF
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            //mrs6_addr = mrs6_addr + subp_idx * 4;
            LwU32 mrs6_addr = mrs6_base + ((pa_idx*4) << 12) + subp_idx * 4;
            LwU8 channel_idx;
            for(channel_idx = 0; channel_idx < 2; channel_idx++) {
                // Allow write to this channel only
                LwU8 ch0 = 3*(channel_idx == 0);
                LwU8 ch1 = 3*(channel_idx == 1);
                LwU32 mrs15_data = ch1 |
                                   (ch0 << 8) |
                                   (0xf << 20);
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_data);

                LwU8 byte_idx;
                for (byte_idx = 0; byte_idx < 2; byte_idx++) {
                    LwU8 bit_idx;
                    for (bit_idx = 0 ; bit_idx < 8; bit_idx++) {
                        LwU16 index = pa_idx*SUBPS*2*2*8 +
                                      subp_idx*2*2*8 +
                                      channel_idx*2*8 +
                                      byte_idx * 8 +
                                      bit_idx;

                        LwU16 mrs6_vref = 0;
                        LwU8 eye_idx;
                        for (eye_idx = 0 ; eye_idx < eyes; eye_idx++) {
                            if(eye_idx == EYE_LOW) {
                                mrs6_vref = best_vref_per_dq_low[index];
                            }
                            if(eye_idx == EYE_MID) {
                                mrs6_vref = best_vref_per_dq_mid[index];
                            }
                            if(eye_idx == EYE_HIGH) {
                                mrs6_vref = best_vref_per_dq_high[index];
                            }

                            // GDDR6x requires 3 VREFs to be programmed in
                            // sequence for each DQ. All eyes are being
                            // programmed, so no masking is needed
                            LwU32 mrs6_data = mrs6_vref |                    //VREF
                                              (16*byte_idx + bit_idx) << 7 | //Pin address
                                              (0x6 << 20) ;
                            REG_WR32(LOG, mrs6_addr,mrs6_data);
                        } // eyes = 3 per DQ for GDDR6x , 1 per DQ for GDDR6
                    } // DQs = 8 per byte
                } // bytes = 2 per channel
            } // channels = 2 per subp
        } // subp
    } // PAs

    // Restore broadcast operation on MRS15
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_store);
}

void program_optimal_dfe_dq_ob
(
    void
)
{
    LwU32 mrs9_base = 0x00900368;
    LwU32 mrs15_store = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);

    LwU8 pa_idx;
    for(pa_idx = 0; pa_idx < MAX_PARTS ; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        //LwU32 mrs9_addr = mrs9_base + ((pa_idx * 4) << 12);
        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx = subp_idx +1 ) {
            // SW will use subp0 and supb1 versions of 
            // MRS9 to prgram the final VREF
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            //mrs9_addr = mrs9_addr + subp_idx * 4;
            LwU32 mrs9_addr = mrs9_base + ((pa_idx*4) << 12) + subp_idx * 4;
            LwU8 channel_idx;
            for(channel_idx = 0; channel_idx < 2; channel_idx++) {
                // Allow write to this channel only
                LwU8  ch0 = 3*(channel_idx == 0);
                LwU8  ch1 = 3*(channel_idx == 1);
                LwU32 mrs15_data = ch1 |
                                   (ch0 << 8) |
                                   (0xf << 20);
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_data);

                LwU8 byte_idx;
                for (byte_idx = 0; byte_idx < 2; byte_idx++) {
                    // This function will not be called for PAM4
                    LwU8 bit_idx;
                    for (bit_idx = 0 ; bit_idx < 8; bit_idx++) {
                        LwU16 index = pa_idx*SUBPS*2*2*8 +
                                      subp_idx*2*2*8 +
                                      channel_idx*2*8 +
                                      byte_idx * 8 +
                                      bit_idx;

                        LwU32 mrs9_data = best_dfe_per_dq[index] |       //DFE
                                          (16*byte_idx + bit_idx) << 7 | //Pin address
                                          (0x9 << 20) ;
                        REG_WR32(LOG, mrs9_addr,mrs9_data);
                    } // DQs = 8 per byte
                } // bytes = 2 per channel
            } // channels = 2 per SUBPS
        } // SUBPS
    } // PAs

    // Restore broadcast operation on MRS15
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_store);
}

void program_optimal_vref_dbi_ob
(
    void
)
{
    LwU32 mrs6_base = 0x00900360;
    LwU32 mrs15_store = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);

    LwU8 pa_idx;
    for(pa_idx = 0; pa_idx < MAX_PARTS ; pa_idx++) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        //LwU32 mrs6_addr = mrs6_base + ((pa_idx * 4) << 12);
        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
            // SW will use subp0 and supb1 versions of 
            // MRS6 to prgram the final VREF
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            //mrs6_addr = mrs6_addr + subp_idx * 4;
            LwU32 mrs6_addr = mrs6_base + ((pa_idx*4) << 12) + subp_idx * 4;
            LwU8 channel_idx;
            for(channel_idx = 0; channel_idx < 2; channel_idx++) {
                // Allow write to this channel only
                LwU8 ch0 = 3*(channel_idx == 0);
                LwU8 ch1 = 3*(channel_idx == 1);
                LwU32 mrs15_data = ch1 |
                                   (ch0 << 8) |
                                   (0xf << 20);
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_data);

                LwU8 byte_idx;
                for (byte_idx = 0; byte_idx < 2; byte_idx++) {
                    LwU16 index = pa_idx*SUBPS*2*2 +
                                  subp_idx*2*2 +
                                  channel_idx*2 +
                                  byte_idx;


                    LwU16 mrs6_vref = 0;
                    LwU8 eye_idx;
                    for (eye_idx = 0 ; eye_idx < eyes ; eye_idx++) {
                        if(eye_idx == EYE_LOW) {
                            mrs6_vref = best_vref_per_dbi_low[index];
                        }
                        if(eye_idx == EYE_MID) {
                            mrs6_vref = best_vref_per_dbi_mid[index];
                        }
                        if(eye_idx == EYE_HIGH) {
                            mrs6_vref = best_vref_per_dbi_high[index];
                        }

                        // GDDR6x requires 3 VREFs to be programmed in sequence
                        // for each DQ. All eyes are being programmed, so no
                        // masking is needed
                        LwU32 mrs6_data = mrs6_vref |                //VREF
                                          (16*byte_idx + 8) << 7 |   //Pin address
                                          (0x6 << 20) ;
                        REG_WR32(LOG, mrs6_addr,mrs6_data);
                    } // eyes = 3 per DQ for GDDR6x , 1 per DQ for GDDR6
                } // bytes = 2 per channel
            } // channels = 2 per subp
        } // subp
    } // PAs

    // Restore broadcast operation on MRS15
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_store);
}

void program_optimal_dfe_dbi_ob
(
    void
)
{
    LwU32 mrs9_base = 0x00900368;
    LwU32 mrs15_store = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);

    LwU8 pa_idx;
    for(pa_idx = 0; pa_idx < MAX_PARTS ; pa_idx = pa_idx + 1) {
        //Check if this PA is enabled. If not, move to the next PA
        //so we don't issue unnecessary/invalid accesses
        if (!isPartitionEnabled(pa_idx)) { continue; }

        //LwU32 mrs9_addr = mrs9_base + ((pa_idx * 4) << 12);
        LwU8 subp_idx;
        for(subp_idx = 0; subp_idx < SUBPS; subp_idx = subp_idx +1 ) {
            //SW will use subp0 and supb1 versions of MRS9 to prgram the final VREF
            if ((subp_idx == 0) && (!isLowerHalfPartitionEnabled(pa_idx))) { continue; }

            //mrs9_addr = mrs9_addr + subp_idx * 4;
            LwU32 mrs9_addr = mrs9_base + ((pa_idx*4) << 12) + subp_idx * 4;
            LwU8 channel_idx;
            for(channel_idx = 0; channel_idx < 2; channel_idx = channel_idx + 1) {
                // Allow write to this channel only
                LwU8  ch0 = 3*(channel_idx == 0);
                LwU8  ch1 = 3*(channel_idx == 1);
                LwU32 mrs15_data = ch1 |
                                   (ch0 << 8) |
                                   (0xf << 20);
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_data);

                LwU8 byte_idx;
                for (byte_idx = 0; byte_idx < 2; byte_idx = byte_idx + 1) {
                    LwU16 index = pa_idx*SUBPS*2*2 +
                                  subp_idx*2*2 +
                                  channel_idx*2 +
                                  byte_idx;

                    // GDDR6x requires 3 VREFs to be programmed in sequence for
                    // each DQ. All eyes are being programmed, so no masking is
                    // needed
                    LwU32 mrs9_data = best_dfe_per_dbi[index] | //DFE
                                      (16*byte_idx + 8) << 7  | //Pin address
                                      (0x9 << 20);
                    REG_WR32(LOG, mrs9_addr,mrs9_data);
                } // bytes = 2 per channel
            } // channels = 2 per SUBPS
        } // SUBPS
    } // PAs

    // Restore broadcast operation on MRS15
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15, mrs15_store);
}

void set_dram_dfe_dq_sweep
(
    LwU8 dfe
)
{
    // Write the requested DFE to both the bytes of a channel
    LwU8 byte_idx;
    for(byte_idx = 0; byte_idx < 2; byte_idx++) {
        LwU32 mr9_data = dfe;
        // add pin address
        mr9_data = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_PIN_SUB_ADDRESS, (15 + 16*byte_idx), mr9_data);
        // set mrs9 field
        mr9_data = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS9, _BA, _MRS, mr9_data);

        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9, mr9_data);
    }
}

void set_dram_dfe_dbi_sweep
(
    LwU8 dfe
)
{
    // Write the requested DFE to both the DBIs of a channel
    LwU8 byte_idx;
    for(byte_idx = 0; byte_idx < 2; byte_idx++) {
        LwU32 mr9_data = dfe;
        // add pin address
        mr9_data = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_PIN_SUB_ADDRESS, (8 + 16*byte_idx), mr9_data);
        // set mrs9 field
        mr9_data = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS9, _BA, _MRS, mr9_data);

        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9, mr9_data);
    }
} // End:set_dram_dfe_dbi_sweep

void start_training
(
    LwU8 direction
)
{
    LwU32 training_cmd_write = 0;
    if (direction == TRAINING_DIR_READ) {
        training_cmd_write = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CMD, _RD,  1, training_cmd_write);
    }
    else if (direction == TRAINING_DIR_WRITE) {
        training_cmd_write = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CMD, _WR,  1, training_cmd_write);
    }
    training_cmd_write = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CMD, _RW_LEVEL,  LW_PFB_FBPA_TRAINING_CMD_RW_LEVEL_PIN,     training_cmd_write);
    training_cmd_write = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CMD, _CONTROL,   LW_PFB_FBPA_TRAINING_CMD_CONTROL_INTRPLTR, training_cmd_write);
    training_cmd_write = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CMD, _GO,        LW_PFB_FBPA_TRAINING_CMD_GO_NOW,           training_cmd_write);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD0, training_cmd_write);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1, training_cmd_write);
}

void do_2_step_pi_offset_tr
(
    LwU8 direction,
    LwU8 dfe_val
)
{
    // Set tag register for first loop
    set_tag_register(1,1,dfe_val);

    if (direction == TRAINING_DIR_READ) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453008); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_EYE_FIRST_TR
    }
    else {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453015); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_EYE_FIRST_TR
    }

    // Trigger training - Area collection loop
    start_training(direction);

    gddr_wait_for_training_done(); //Poll for Training end

    // Clear tag register for second loop
    set_tag_register(0,0,0);

    if (direction == TRAINING_DIR_READ) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453009); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_EYE_SECOND_TR
    }
    else {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453016); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_EYE_SECOND_TR
    }

    // Trigger training - VREF training loop
    start_training(direction);

    gddr_wait_for_training_done(); //Poll for Training end
}

void do_area_based_tr_init
(
    LwU8 direction
)
{
    LwBool first_tag = LW_TRUE;
    LwU32  tag = 10; // This is just arbitrary
    set_tag_register(first_tag,1,tag);
    if (direction == TRAINING_DIR_READ) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453003); //FALCON_BOOT_TR_STATE_RD_VREF_EYE_FIRST_TR
    }
    else {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453010); //FALCON_BOOT_TR_STATE_WR_VREF_EYE_FIRST_TR
    }
    start_training(direction);
    gddr_wait_for_training_done();
    if(!gddr_check_training_passed()) {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_VREF_STEP1_ERROR + direction);
    }

    set_tag_register(0,0,0);
    if (direction == TRAINING_DIR_READ) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453004); //FALCON_BOOT_TR_STATE_RD_VREF_EYE_SECOND_TR
    }
    else {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453011); //FALCON_BOOT_TR_STATE_WR_VREF_EYE_SECOND_TR
    }
    start_training(direction);
    gddr_wait_for_training_done();
}

void rd_train_single_dfe
(
    LwU8 lwr_dfe,
    LwU8 loop_idx
)
{
    if (!bFlagNoDFEChange) {
      set_gpu_dfe_sweep(lwr_dfe);
    }

    // disable VREF training
    set_rd_vref_tr(LW_FALSE);

    // do normal PI training
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453007); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_DFE_SETUP
    start_training(TRAINING_DIR_READ);
    gddr_wait_for_training_done();
    /*
    //[bswamy] It is possible for training to fail so don't report an error here
    //Instead, fail if we find 0 area for any DQ across all DFEs
    // IF TRAINING FAIL, HALT HERE - NO DFE IS EXPECTED TO FAIL PI TRAINING
    if(!gddr_check_training_passed()) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
        FW_MBOX_WR32(13, lwr_dfe); //Current DFE to MBOX 13 for debug
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_DFE_PI_ERROR);
    }
    */
    set_pi_offset_tr(TRAINING_DIR_READ);
    set_rd_vref_tr(LW_TRUE);

    //Need to sweep the eye from MID->UPPER->LOW
    LwU8 eye_idx;
    for(eye_idx = EYE_MID; eye_idx < (eyes+1); eye_idx++) {
        LwU8 actual_eye = (eye_idx % eyes);
        set_vref_sweep_gpu(actual_eye);
        set_rd_vref_tr_area(); //Need this here because set_vref_sweep_gpu will clear area based bit
        set_eye_mask(actual_eye, LW_FALSE);

        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD, BOOT_TR_DEBUG_CMD_PI_OFFSET_SWEEP, actual_eye, lwr_dfe, loop_idx);
        do_2_step_pi_offset_tr(TRAINING_DIR_READ, lwr_dfe);
        /*
        //[bswamy] It is possible for training to fail so don't report an error here
        //Instead, fail if we find 0 area for any DQ across all DFEs
        if(!gddr_check_training_passed()) {
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
            FW_MBOX_WR32(13, lwr_dfe); //Current DFE to MBOX 13 for debug
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_2STEP_PI_EYE_LOW_ERROR + actual_eye);
        }
        */

        read_per_pin_area(lwr_dfe,actual_eye);
        if(dbi_en_rd | boot_tr_mta_en) {
            read_per_dbi_area(lwr_dfe,actual_eye);
        }

        read_per_pin_vref_ib(lwr_dfe,actual_eye);
        if(dbi_en_rd | boot_tr_mta_en) {
            read_per_dbi_vref_ib(lwr_dfe,actual_eye,LW_FALSE);
        }
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300a); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_EYE_CLEANUP
        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD, BOOT_TR_DEBUG_CMD_PI_OFFSET_AREA_VREF, actual_eye, lwr_dfe, loop_idx);
    } // End of eyes loop for PI offset training
    // Have the option of programming the BKV VREF or the inital VREF for all PI trainigna nd for the non swept eye

    // Disable PI offset training at this point
    // to move to the next DFE
    clear_pi_offset_tr();
}

void parseBootTrainingTable (void)
{
    //Grab all the tables we need
    BOOT_TRAINING_FLAGS *pTrainingFlags      = &gTT.bootTrainingTable.MemBootTrainTblFlags;
    BOOT_TRAINING_VREF_CONTROL *pVrefControl = &gTT.bootTrainingTable.VREF_DFE_1;
    BOOT_TRAINING_PI_OFFSET_TRAINING *pPiOffsetTrainingR = &gTT.bootTrainingTable.PI_Offset_Training_R;
    BOOT_TRAINING_PI_OFFSET_TRAINING *pPiOffsetTrainingW = &gTT.bootTrainingTable.PI_Offset_Training_W;

    //Flags
    bFlagG6AddrTrEn       = pTrainingFlags->flag_g6_addr_tr_en;
    bFlagG6WckTrEn        = pTrainingFlags->flag_g6_wck_tr_en;
    bFlagG6RdTrEn         = pTrainingFlags->flag_g6_rd_tr_en;
    bFlagG6RdTrPrbsEn     = pTrainingFlags->flag_g6_rd_tr_prbs_en;
    bFlagG6WrTrPrbsEn     = pTrainingFlags->flag_g6_wr_tr_prbs_en;
    bFlagG6EdcTrackingEn  = pTrainingFlags->flag_edc_tracking_en;
    bFlagG6VrefTrackingEn = pTrainingFlags->flag_vref_tracking_en;
    bFlagG6RdEdcEnabled   = pTrainingFlags->flag_rd_edc_enabled;
    bFlagG6WrEdcEnabled   = pTrainingFlags->flag_wr_edc_enabled;

    //New flags for G6X
    cors_step_override_rd = pVrefControl->CORS_STEP_OVERRIDE_SWEEP_RD;
    cors_step_override_wr = pVrefControl->CORS_STEP_OVERRIDE_SWEEP_WR;

    enable_pi_offset_tr_rd = pPiOffsetTrainingR->PI_OFFSET_ENABLE;
    enable_pi_offset_tr_wr = pPiOffsetTrainingW->PI_OFFSET_ENABLE;
    enable_moving_avg_rd   = (pVrefControl->OPTIMAL_DFE_RD == OPTIMAL_DFE_RD_ROLLING_AVERAGE);

    initial_vref_bkv        = (pVrefControl->INIT_VREF_SCHEME == INIT_VREF_SCHEME_BKV);
    initial_vref_area       = (pVrefControl->INIT_VREF_SCHEME == INIT_VREF_SCHEME_AREA_BASED);
    initial_vref_max_window = (pVrefControl->INIT_VREF_SCHEME == INIT_VREF_SCHEME_MAX_PI_PASED);

    averaging_loops_rd = pVrefControl->AVERAGING_LOOP_RD;
    averaging_loops_wr = pVrefControl->AVERAGING_LOOP_WR;

    //If this is G6, there is only one eye to train
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        eyes = 1;
    }

    //These just come from devinit
    LwU32 fbio_config_dbi = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CONFIG_DBI);
    dbi_en_rd = REF_VAL(LW_PFB_FBPA_FBIO_CONFIG_DBI_OUT_ON, fbio_config_dbi);
    dbi_en_wr = REF_VAL(LW_PFB_FBPA_FBIO_CONFIG_DBI_IN_ON,  fbio_config_dbi);
    dbi_en = (dbi_en_rd | dbi_en_wr);

    //===========================================
    //Read Settings
    //===========================================
    dfe_min_rd    = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  gTT.bootTrainingTable.READ_TRAINING_DFE);
    dfe_max_rd    = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  gTT.bootTrainingTable.READ_TRAINING_DFE);
    dfe_step_rd   = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_STEP, gTT.bootTrainingTable.READ_TRAINING_DFE);
    dfe_start_rd  = REF_VAL(31:24,                                  gTT.bootTrainingTable.READ_TRAINING_DFE);

    pi_cors_min_rd     = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_MIN,     gTT.bootTrainingTable.READ_RW_CORS_INTRPLTR_CTRL);
    pi_cors_max_rd     = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_MAX,     gTT.bootTrainingTable.READ_RW_CORS_INTRPLTR_CTRL);
    pi_cors_step_rd    = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_STEP,    gTT.bootTrainingTable.READ_RW_CORS_INTRPLTR_CTRL);
    pi_cors_step_ln_rd = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_STEP_LN, gTT.bootTrainingTable.READ_RW_CORS_INTRPLTR_CTRL);

    pi_fine_min_rd     = signExtendByte(REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_MIN, gTT.bootTrainingTable.READ_RW_FINE_INTRPLTR_CTRL), 6);
    pi_fine_max_rd     = REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_MAX,     gTT.bootTrainingTable.READ_RW_FINE_INTRPLTR_CTRL);
    pi_fine_step_rd    = REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_STEP,    gTT.bootTrainingTable.READ_RW_FINE_INTRPLTR_CTRL);
    pi_fine_step_ln_rd = REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_STEP_LN, gTT.bootTrainingTable.READ_RW_FINE_INTRPLTR_CTRL);

    pi_offset_min_rd   = signExtendByte(pPiOffsetTrainingR->PI_OFFSET_CTRL_MIN, 5);
    pi_offset_max_rd   = pPiOffsetTrainingR->PI_OFFSET_CTRL_MAX;
    pi_offset_step_rd  = pPiOffsetTrainingR->PI_OFFSET_CTRL_STEP;

    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        vref_min_rd        = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  gTT.bootTrainingTable.READ_TRAINING_VREF);
        vref_max_rd        = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  gTT.bootTrainingTable.READ_TRAINING_VREF);
        vref_step_rd       = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_STEP, gTT.bootTrainingTable.READ_TRAINING_VREF);
    }
    else {
        //To allow separate control for min/max for each receiver, we are using a combination of READ_TRAINING_VREF and TRAINING_RW_VREF_CTRL
        //TRAINING_RW_VREF_CTRL[MIN]  = vref_low_min
        //TRAINING_RW_VREF_CTRL[MAX]  = vref_low_max
        //TRAINING_RW_VREF_CTRL[STEP] = vref_step_rd
        //READ_TRAINING_VREF[7:0]     = vref_mid_min
        //READ_TRAINING_VREF[15:8]    = vref_mid_max
        //READ_TRAINING_VREF[23:16]   = vref_upper_min
        //READ_TRAINING_VREF[31:24]   = vref_upper_max
        vref_step_rd = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_STEP,  gTT.bootTrainingTable.READ_RW_VREF_CTRL);

        LwU8 vref_low_min_setting   = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  gTT.bootTrainingTable.READ_RW_VREF_CTRL);
        LwU8 vref_low_max_setting   = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  gTT.bootTrainingTable.READ_RW_VREF_CTRL);
        LwU8 vref_mid_min_setting   = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  gTT.bootTrainingTable.READ_TRAINING_VREF);
        LwU8 vref_mid_max_setting   = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  gTT.bootTrainingTable.READ_TRAINING_VREF);
        LwU8 vref_upper_min_setting = REF_VAL(23:16,                                  gTT.bootTrainingTable.READ_TRAINING_VREF);
        LwU8 vref_upper_max_setting = REF_VAL(31:24,                                  gTT.bootTrainingTable.READ_TRAINING_VREF);

        //Per Ling Zhang (circuit)
        //Sweep VREF code range during VREF Training
        //                  40 ohm termination 50 ohm termination
        //    VREF top      231+/-25           227 +/- 30
        //    VREF middle   164 +/-25          153 +/-30
        //    VREF bottom   97 +/- 25          79 +/-30
        //Given the above, min_low/mid/high will be set to the min possible in the 50 Ohm case.
        //The values programmed into READ_TRAINING_VREF are used to adjust the min and max ranges
        //vref_min_* = FIXED_VALUE + vref_min_rd
        //vref_max_* = FIXED_VALUE + vref_min_rd + vref_max_rd
        //Note: Due to the above use of 2 registers we now specify the min and max indepedently for each eye
        //They share a single step setting
        vref_min_low      = vref_low_min_setting;
        vref_max_low      = vref_low_max_setting;
        vref_min_mid      = vref_mid_min_setting;
        vref_max_mid      = vref_mid_max_setting;
        vref_min_high     = vref_upper_min_setting;
        vref_max_high     = vref_upper_max_setting;
    }

    //===========================================
    //Write Settings
    //===========================================
    dfe_min_wr         = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  gTT.bootTrainingTable.WRITE_TRAINING_DFE);
    dfe_max_wr         = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  gTT.bootTrainingTable.WRITE_TRAINING_DFE);
    dfe_step_wr        = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_STEP, gTT.bootTrainingTable.WRITE_TRAINING_DFE);

    pi_cors_min_wr     = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_MIN,     gTT.bootTrainingTable.WRITE_RW_CORS_INTRPLTR_CTRL);
    pi_cors_max_wr     = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_MAX,     gTT.bootTrainingTable.WRITE_RW_CORS_INTRPLTR_CTRL);
    pi_cors_step_wr    = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_STEP,    gTT.bootTrainingTable.WRITE_RW_CORS_INTRPLTR_CTRL);
    pi_cors_step_ln_wr = REF_VAL(LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL_STEP_LN, gTT.bootTrainingTable.WRITE_RW_CORS_INTRPLTR_CTRL);

    pi_fine_min_wr     = signExtendByte(REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_MIN, gTT.bootTrainingTable.WRITE_RW_FINE_INTRPLTR_CTRL), 6);
    pi_fine_max_wr     = REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_MAX,     gTT.bootTrainingTable.WRITE_RW_FINE_INTRPLTR_CTRL);
    pi_fine_step_wr    = REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_STEP,    gTT.bootTrainingTable.WRITE_RW_FINE_INTRPLTR_CTRL);
    pi_fine_step_ln_wr = REF_VAL(LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL_STEP_LN, gTT.bootTrainingTable.WRITE_RW_FINE_INTRPLTR_CTRL);

    pi_offset_min_wr   = signExtendByte(pPiOffsetTrainingW->PI_OFFSET_CTRL_MIN, 5);
    pi_offset_max_wr   = pPiOffsetTrainingW->PI_OFFSET_CTRL_MAX;
    pi_offset_step_wr  = pPiOffsetTrainingW->PI_OFFSET_CTRL_STEP;

    vref_min_wr        = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  gTT.bootTrainingTable.WRITE_TRAINING_VREF);
    vref_max_wr        = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  gTT.bootTrainingTable.WRITE_TRAINING_VREF);
    vref_step_wr       = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_STEP, gTT.bootTrainingTable.WRITE_TRAINING_VREF);

    //===========================================
    //Debug Settings
    //===========================================
    #ifdef BOOT_TR_DEBUG_ENABLE
    bFlagBootTrDebugEnRd    = pTrainingFlags->flag_g6_rd_tr_hybrid_non_vref_en;
    bFlagBootTrDebugEnWrDQ  = pTrainingFlags->flag_g6_wr_tr_hybrid_non_vref_en;
    bFlagBootTrDebugEnWrMTA = pTrainingFlags->flag_g6_wr_tr_hybrid_vref_en;
    bFlagBootTrDebugEnMisc  = pTrainingFlags->flag_collect_schmoo_data;
    bFlagDoRdTr             = (pTrainingFlags->flag_g6_rd_tr_hw_vref_en == 0);
    bFlagDoWrTr             = (pTrainingFlags->flag_g6_wr_tr_hw_vref_en == 0);
    #endif

    bFlagDoP0P8Final = (pTrainingFlags->flag_falcon_to_determine_best_window == 0);
    bFlagNoDFEChange = (pTrainingFlags->flag_shift_pattern_rd);

    FW_MBOX_WR32(8, averaging_loops_rd);
    FW_MBOX_WR32(9, averaging_loops_wr);
}

void restore_saved_regs (void)
{
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL,          saved_reg->pfb_fbpa_training_rw_vref_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL, saved_reg->pfb_fbpa_training_rw_cors_intrpltr_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL, saved_reg->pfb_fbpa_training_rw_fine_intrpltr_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM,                saved_reg->pfb_fbpa_training_patram);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL,        saved_reg->pfb_fbpa_training_char_bank_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2,       saved_reg->pfb_fbpa_training_char_bank_ctrl2);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BURST,            saved_reg->pfb_fbpa_training_char_burst);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_CTRL,             saved_reg->pfb_fbpa_training_char_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_TURN,             saved_reg->pfb_fbpa_training_char_turn);

    //Clear the ACT_ADR bit in TRAINING_PATTERN_PTR
    LwU32 training_pattern_ptr = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATTERN_PTR(1));
    training_pattern_ptr = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_PATTERN_PTR, _ACT_ADR, LW_PFB_FBPA_TRAINING_PATTERN_PTR_ACT_ADR_DISABLED, training_pattern_ptr);
    LwU8 subp_idx;
    for(subp_idx = 0; subp_idx < SUBPS; subp_idx++) {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATTERN_PTR(subp_idx), training_pattern_ptr);
    }

    //These regs need to be updated for mclk switch code rather than restored
    lwr_reg->pfb_fbpa_training_ctrl  = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
    lwr_reg->pfb_fbpa_training_ctrl2 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    lwr_reg->pfb_fbpa_training_cmd1  = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CMD1);
    lwr_reg->pfb_fbpa_training_cmd0  = lwr_reg->pfb_fbpa_training_cmd1;
}

LwU32 doBootTimeTraining (void)
{
    FLCN_TIMESTAMP bootTrStartTime = {0};
    osPTimerTimeNsLwrrentGet(&bootTrStartTime);

    LwU32 p0Freq = gBiosTable.nominalFrequencyP0;
    LwU32 p8Freq = gBiosTable.nominalFrequencyP8;

    //Determine DDR mode from FBIO broadcast
    lwr_reg->pfb_fbpa_fbio_broadcast = REG_RD32(LOG, LW_PFB_FBPA_FBIO_BROADCAST);
    gbl_ddr_mode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast);

    //Parse BootTrainingTable to get flags and training settings
    parseBootTrainingTable();
    
    //Save values which need to be restored after boot training is complete
    saved_reg->pfb_fbpa_training_rw_vref_ctrl          = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL);
    saved_reg->pfb_fbpa_training_rw_cors_intrpltr_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL);
    saved_reg->pfb_fbpa_training_rw_fine_intrpltr_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL);
    saved_reg->pfb_fbpa_training_patram                = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    saved_reg->pfb_fbpa_training_char_bank_ctrl        = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL);
    saved_reg->pfb_fbpa_training_char_bank_ctrl2       = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2);
    saved_reg->pfb_fbpa_training_char_burst            = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_BURST);
    saved_reg->pfb_fbpa_training_char_ctrl             = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_CTRL);
    saved_reg->pfb_fbpa_training_char_turn             = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_TURN);
    saved_reg->fbio_pwr_ctrl                           = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_PWR_CTRL);
    saved_reg->fbio_pwr_ctrl1                          = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_PWR_CTRL1);

    //Save and temporarily clear mclk switch enable
    LwBool save_gbl_en_fb_mclk_sw = gbl_en_fb_mclk_sw;
    gbl_en_fb_mclk_sw = LW_FALSE;

    #if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    privprofilingEnable();
    privprofilingReset();
    #endif

    //osPTimerTimeNsLwrrentGet(&bootTrTimeNs);

    // Step 1: Char in P8
    //This preliminary write to TRAINING_CMD is used to let the falcon shim know we're in char mode
    FLCN_TIMESTAMP bootTrLwrrentStepStartTime = {0};
    osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
    
    LwU32 training_cmd_write = 0;
    training_cmd_write = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CMD, _CHAR_ENGINE, LW_PFB_FBPA_TRAINING_CMD_CHAR_ENGINE_ENABLED, training_cmd_write);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD0, training_cmd_write);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1, training_cmd_write);
    if(bFlagG6RdTrEn && bFlagG6RdTrPrbsEn)
    {
        //func_fbio_intrpltr_offsets(6,28);
        func_char_init_for_prbs();
    } else
    {
        // Ensuring LW_PFB_FBPA_TRAINING_CTRL_GDDR5_COMMANDS = 1;
        LwU8 training_ctrl_gddr5_commands = 1;
        memorySetTrainingControlGddr5Commands_HAL(&Fbflcn,training_ctrl_gddr5_commands);

        // Ensuring BANK_CTRL is set to 0 for RD/WR training to work correctly.
        LwU32 bank_ctrl_enable = 0;
        LwU32 fbpa_training_char_bank_ctrl = gTT.bootTrainingTable.READ_CHAR_BANK_CTRL;
        fbpa_training_char_bank_ctrl = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_CHAR_BANK_CTRL,  _ENABLE,         bank_ctrl_enable,     fbpa_training_char_bank_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL,  fbpa_training_char_bank_ctrl);
    }
    training_cmd_write = 0;
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD0, training_cmd_write);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1, training_cmd_write);
    bootTrStepTimes[BOOT_TR_TIME_CHAR] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);

    //[bswamy] Hack to set a few flags so we can use a single ucode compile
    //to run a combination of boot trainings.
    //By default all flags are set to TRUE so silicon will bypass this
    //[5] = Final P0->P8 Switch
    //[4] = Write Training
    //[3] = Read Training
    //[2] = WCK training
    //[1] = Address training
    //[0] = P8->P0 switch
    LwU32 boot_training_sim_flags = REG_RD32(BAR0, LW_PFB_FBPA_FALCON_MONITOR);
    if (((boot_training_sim_flags >> 16) & 0xFFFF) == 0xB007) {
        bFlagDoRdTr       = (boot_training_sim_flags >> 3) & 0x1;
        bFlagDoWrTr       = (boot_training_sim_flags >> 4) & 0x1;
        bFlagDoP0P8Final  = (boot_training_sim_flags >> 5) & 0x1;
    }

    //osPTimerTimeNsLwrrentGet(&bootTrTimeNs);
    // Step 2: Switch from P8->P0. The "1,0,0" argument means run until just before address training
    // doMclkSwitchInMultipleStages(frequency, preAddrTr, postAddrTr, postWckRdWrTr)
    //Need to disable FBIO clock gating until training is complete
    set_fbio_pwr_ctrl(0, 0);
    gddr_in_boot_training(LW_TRUE); //Indicate to mclk switch code that we're in boot training
    doMclkSwitchInMultipleStages(p0Freq , 1,0,0);

    //Now that we are in P0, parse the flags needed for training
    //This needs to be done here so the proper PMCT table is loaded
    boot_tr_mta_en = gTT.perfMemClkBaseEntry.Flags0.PMC11EF0MTA;

    func_setup_misc_training_ctrl();
    func_setup_training_timings();

    //Step 3: Run address training if enabled
    if(bFlagG6AddrTrEn) {
        osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
        func_setup_g6_addr_tr();
        LwU32 status_tr_passed = gddr_addr_training(gbl_ddr_mode);

        if(!status_tr_passed) {
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_ADDRESS_TRAINING_ERROR);
        }

        // FbFlcn saves address training values
        //memoryMoveCmdTrainingValues_HAL(&Fbflcn,REGISTER_READ);
        //gTD.td.cmdDelayValid = 1;
        bootTrStepTimes[BOOT_TR_TIME_ADDR] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);
    }

    //Step 4: Run the next state of mclk switch until just before WCK/RD/WR training
    //doMclkSwitchInMultipleStages(frequency, preAddrTr, postAddrTr, postWckRdWrTr)
    doMclkSwitchInMultipleStages(p0Freq , 0,1,0);

    //Step 5: If WCK training is enabled, run it
    if (bFlagG6WckTrEn)  {
        osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
        func_run_training(0, 1, 0, 0, 0, 0);
        gddr_wait_for_training_done();
        if(!gddr_check_training_passed()) {
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WCK_TRAINING_ERROR);
        }
        bootTrStepTimes[BOOT_TR_TIME_WCK] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);

        //Hinting tb training monitor to check wck training values
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x5452493);
    }

    //Need to disable functional EDC before we do any training or EDC tracking
    disable_functional_edc();

    // EDC Tracking
    if(bFlagG6EdcTrackingEn || bFlagG6VrefTrackingEn) {
        gddr_edc_tracking();

        LwU32 fbpa_fbio_edc_tracking= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_EDC_TRACKING);
        fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _DISABLE_TRAINING_UPDATES,       1,   fbpa_fbio_edc_tracking);
        REG_WR32(BAR0, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
    }

    //This is where the boot_training.js code begins to match with this file. From here on
    //the algorithm should match the javascript.
    //Pass DFE settings to the training monitors
    //The settings are packed into a 32 bit value
    LwU32 dfe_settings = ((dfe_step_rd << 16) & 0x00FF0000) | ((dfe_max_rd << 8) & 0x0000FF00) | (dfe_min_rd & 0x000000FF);
    FW_MBOX_WR32(6, dfe_settings);
    dfe_settings = ((dfe_step_wr << 16) & 0x00FF0000) | ((dfe_max_wr << 8) & 0x0000FF00) | (dfe_min_wr & 0x000000FF);
    FW_MBOX_WR32(7, dfe_settings);
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453001); //FALCON_BOOT_TR_STATE_SETUP

    //=========================================================================
    //READ Training
    //=========================================================================
    func_set_prbs_dual_mode(bFlagG6RdTrPrbsEn,1);
    set_pi_sweep(TRAINING_DIR_READ);
    set_pi_offset_sweep(TRAINING_DIR_READ);
    set_ldff_cmd_cnt();
    enable_txeq();
    enable_noise_gen();
    disable_pattern_shift_ilwert();

    //Need to disable VREF tracking updates during training.
    //Also need to save off the current EDC VREF values and restore them after
    //read training -- the read_per_dbi_vref_ib function has been overloaded to
    //handle saving EDC VREF values
    //set_vref_tracking_disable_training_updates(LW_TRUE);
    read_per_dbi_vref_ib(0,EYE_LOW,LW_TRUE);

    if (bFlagDoRdTr) {
        FLCN_TIMESTAMP bootTrRdTrStart = {0};
        osPTimerTimeNsLwrrentGet(&bootTrRdTrStart);

        initialize_best_arrays();

        if(enable_pi_offset_tr_rd) {
            LwU8 loop_idx;
            for(loop_idx = 1; loop_idx <= averaging_loops_rd; loop_idx++) {
                set_rd_vref_tr(LW_TRUE);
                // Refer to email from Virendra, dated 2/3/2020 titled
                // Ampere Boot Training - Algorithm Review. DFE needs to
                // be disabled before doing initial VREF training with
                // receiver masking
                set_gpu_dfe(0);

                set_cors_step_override(cors_step_override_rd);
                // Setup DFE value
                // DFE is disabled so we don't need to do this anymore
                //set_gpu_dfe_sweep(dfe_min_rd);

                // Initial VREF training for each eye
                if(!initial_vref_bkv) {
                    osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
                    LwU8 eye_idx;
                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453002); //FALCON_BOOT_TR_STATE_RD_VREF_SETUP
                    for(eye_idx = 0; eye_idx < eyes ; eye_idx++) {
                        set_vref_sweep_gpu(eye_idx);
                        set_eye_mask(eye_idx, LW_FALSE);
                        if(eye_idx != EYE_HIGH) {
                            set_receiver_mask(eye_idx);
                        }
                        else {
                            //Reset eye mask for EYE_HIGH
                            clear_receiver_mask();
                        }
                        if(initial_vref_max_window == 1) {
                            start_training(TRAINING_DIR_READ);
                            gddr_wait_for_training_done();
                            if(!gddr_check_training_passed()) {
                                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                                FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_VREF_EYE_LOW_ERROR + eye_idx);
                            }
                        }

                        if(initial_vref_area) {
                            set_rd_vref_tr_area();
                            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD, BOOT_TR_DEBUG_CMD_GPU_INIT_VREF_SWEEP, eye_idx, 0, loop_idx);
                            do_area_based_tr_init(TRAINING_DIR_READ);
                            if(!gddr_check_training_passed()) {
                                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                                FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_VREF_EYE_LOW_ERROR + eye_idx);
                            }
                        }
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453005); //FALCON_BOOT_TR_STATE_RD_VREF_EYE_CLEANUP
                    } // End of eyes loop for initial training
                    bootTrStepTimes[BOOT_TR_TIME_RD_VREF] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);
                } //!initial_vref_bkv
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD, BOOT_TR_DEBUG_CMD_GPU_INIT_VREF, 0, 0, loop_idx);

                // At this point initial VREF has been programmed for all DQs
                // Now start the actual DFE loop with PI offset training

                // Clear any receiver masking
                clear_receiver_mask();

                // Enable GPU dfe
                // Refer to email from Virendra, dated 2/3/2020 titled
                // Ampere Boot Training - Algorithm Review. DFE needs to
                // be disabled before doing initial VREF training with
                // receiver masking and needs to be enabled after this
                // intial VREF training (which is required to be done
                // without DFE) is done.

                // Simple explanation of why-
                // Even if the DFE code is 0, there
                // is still some base DFE that is applied. Our training
                // pattern is not restricted to two levels specific to the
                // eye that is being trained. The same pattern with all
                // level transitions is used for all eye trainings. So if
                // DFE is not disabled, data dependent feedback from other
                // receivers will corrupt the training results of the
                // receiver lwrrently being trained. So DFE needs to be
                // disabled for this step.
                set_gpu_dfe(1);

                // set area sed. PI Offset is always area based
                if(!initial_vref_area) {
                    set_rd_vref_tr_area();
                }
                // Clear cors step override
                // [fix] - keep cors step override
                // set_cors_step_override(LW_FALSE);
                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453006); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_SETUP

                //------------------------ DFE LOOPS ---------------------------
                //[bswamy] We need to start at dfe_start_rd and move up to dfe_max rd, then reprogram the BKV
                //values and go from dfe_start_rd down to dfe_min_rd.
                //Additionally, we sweep the eyes from MID->UPPER->LOW
                //[bswamy] For dfe_start_rd, we want to train twice -- once with the BKV values and then again with the
                //updated VREFs from training
                rd_train_single_dfe(dfe_start_rd, loop_idx);

                LwS8 lwr_dfe;
                for(lwr_dfe = dfe_start_rd; lwr_dfe <= dfe_max_rd; lwr_dfe = lwr_dfe + dfe_step_rd) {
                    osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
                    rd_train_single_dfe(lwr_dfe, loop_idx);
                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300b); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_DFE_CLEANUP
                    bootTrStepTimes[BOOT_TR_TIME_RD_DFE] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);
                } // End of DFE loop for PI offset training

                //If the starting DFE was not zero, then we're using the new methodology to start the sweep in
                //the middle and go up, then down. The block below will reset to the middle and do the downward
                //sweep
                if (dfe_start_rd != dfe_min_rd) {
                    //Need to restore BKV VREF before starting the next loop
                    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1, gTT.perfMemClkBaseEntry.spare_field6);

                    //[bswamy] For dfe_start_rd, we want to train twice -- once with the BKV values and then again with the
                    //updated VREFs from training
                    rd_train_single_dfe(dfe_start_rd, loop_idx);

                    for(lwr_dfe = dfe_start_rd; lwr_dfe >= dfe_min_rd; lwr_dfe = lwr_dfe - dfe_step_rd) {
                        osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
                        rd_train_single_dfe(lwr_dfe, loop_idx);
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300b); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_DFE_CLEANUP
                        bootTrStepTimes[BOOT_TR_TIME_RD_DFE] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);
                    } // End of DFE loop for PI offset training
                }
                //---------------------- END DFE LOOPS -------------------------

                // At this point all areas have been saved (per-eye for each pin for each DFE)
                // Update area if moving average is enabled - this is fixed moving average of 3
                if(enable_moving_avg_rd) {
                    if(dbi_en_rd | boot_tr_mta_en) {
                        update_area_moving_avg(dfe_min_rd, dfe_max_rd, dfe_step_rd, 1);
                    } else {
                        update_area_moving_avg(dfe_min_rd, dfe_max_rd, dfe_step_rd, 0);
                    }
                }

                // At this point all the areas has been updated with values with moving average of 3
                // applied. Now determine optimal and keep adding to be averaged later on
                if(dbi_en_rd | boot_tr_mta_en) {
                    select_optimal_dfe_vref(dfe_min_rd, dfe_max_rd, dfe_step_rd, 1, TRAINING_DIR_READ);
                } else {
                    select_optimal_dfe_vref(dfe_min_rd, dfe_max_rd, dfe_step_rd, 0, TRAINING_DIR_READ);
                }

                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300c); //FALCON_BOOT_TR_STATE_RD_PI_OFFSET_CLEANUP
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD, BOOT_TR_DEBUG_CMD_PI_OFFSET_AVG_AREA_VREF_DFE, 0, 0, loop_idx);
            } // End of averaging loops

            // At this point, all averaging loops have also been run.
            // The sum of all the optimals is now available in the best* arrays.
            // Average them all out.
            if(dbi_en_rd | boot_tr_mta_en) {
                average_optimal_dfe_vref(averaging_loops_rd, 1, 0);
                add_dfe_vref_offsets(TRAINING_DIR_READ, 1, 0);
            }
            else {
                average_optimal_dfe_vref(averaging_loops_rd, 0, 0);
                add_dfe_vref_offsets(TRAINING_DIR_READ, 0, 0);
            }


            // Program final optimal
            set_cmd_hold_update(1);

            if (!bFlagNoDFEChange) {
              program_final_dfe_gpu();
            }
            program_final_vref_gpu();

            set_cmd_hold_update(0);
            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_RD, BOOT_TR_DEBUG_CMD_FINAL_AREA_VREF_DFE, 0, 0, 0);
        } // PI Offset training
        else {
            // FIXME : Add code for Turing style area based training
            // This will be applicable only for GDDR6 - There is no
            // support for Turing style training for GDDR6x
        }

        // Clear settings for Read VREF and PI offset training
        // Disable VREF training
        set_rd_vref_tr(LW_FALSE);
        // Disable pi_offset_training
        clear_pi_offset_tr();

        bootTrStepTimes[BOOT_TR_TIME_RD_TR] = osPTimerTimeNsElapsedGet(&bootTrRdTrStart);

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
        gPafalconDebugWriteAdr = 0;
        // dq
        dmemWritePAFalcon((LwU32*)&best_vref_per_dq_low[0], sizeof(best_vref_per_dq_low),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_vref_per_dq_low);
        dmemWritePAFalcon((LwU32*)&best_vref_per_dq_mid[0], sizeof(best_vref_per_dq_mid),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_vref_per_dq_mid);
        dmemWritePAFalcon((LwU32*)&best_vref_per_dq_high[0], sizeof(best_vref_per_dq_high),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_vref_per_dq_high);
        // dbi
        dmemWritePAFalcon((LwU32*)&best_vref_per_dbi_low[0], sizeof(best_vref_per_dbi_low),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_vref_per_dbi_low);
        dmemWritePAFalcon((LwU32*)&best_vref_per_dbi_mid[0], sizeof(best_vref_per_dbi_mid),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_vref_per_dbi_mid);
        dmemWritePAFalcon((LwU32*)&best_vref_per_dbi_high[0], sizeof(best_vref_per_dbi_high),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_vref_per_dbi_high);
        // dfe
        dmemWritePAFalcon((LwU32*)&best_dfe_per_dq[0], sizeof(best_dfe_per_dq),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_dfe_per_dq);
        dmemWritePAFalcon((LwU32*)&best_dfe_per_dbi[0], sizeof(best_dfe_per_dbi),gPafalconDebugWriteAdr);
        gPafalconDebugWriteAdr += sizeof(best_dfe_per_dbi);
        // update debug blocks
        gAonStateInfo.debugDataBlks = (gPafalconDebugWriteAdr + 255) / 256;

#endif
    } //bFlagDoRdTr

    // VREF and DFE have been programmed. Run PI training
    // to train BS/PI
    // Since we have not disabled PRBS training, PI training
    // will be run with PRBS patterns
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300d); //FALCON_BOOT_TR_STATE_RD_FINAL
    start_training(TRAINING_DIR_READ);
    gddr_wait_for_training_done();
    if(!gddr_check_training_passed()) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_FINAL_PI_ERROR);
    }

    // Cleanup for Read Training
    //[bswamy] Re-added to allow write to run independently of read. 
    //This can be removed (but won't hurt to execute)
    set_rd_vref_tr(LW_FALSE);
    disable_noise_gen();

    //=========================================================================
    //WRITE Training
    //=========================================================================
    //Even if we're not doing WR training, need to disable MTA and run final PI if MTA is enabled
    if (bFlagDoWrTr || boot_tr_mta_en) { 
        FLCN_TIMESTAMP bootTrWrTrStart = {0};
        osPTimerTimeNsLwrrentGet(&bootTrWrTrStart);

        // First Loop Trains only DQ
        // Disable MTA or DBI in the GPU
        if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            set_dram_dbi(LW_FALSE);
        } else {
            set_dram_mta(LW_FALSE);
        }

        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300e); //FALCON_BOOT_TR_STATE_WR_SETUP

        if (bFlagDoWrTr) {
            // DQ Loop Starts
            // Read data has been already programmed.
            // We can re-use the same structure for
            // best arrays. So initialize them again
            initialize_best_arrays();
            // Enable VREF sweep for write VREF Training
            set_pi_sweep(TRAINING_DIR_WRITE);
            set_pi_offset_sweep(TRAINING_DIR_WRITE);
            set_vref_sweep_dram(); // Sweep range should be same for all eyes
        }

        if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
            if (bFlagDoWrTr) {
                // DQ Loop Training for GDDR6x
                LwU8 loop_idx;
                for(loop_idx = 1; loop_idx <= averaging_loops_wr; loop_idx++) {
                    LwU8 eye_idx;

                    // GDDR6x has no DRAM DFE. 
                    // TX-EQ should be set by init to the desired value already.
                    // Doesn't need to be programmed here in boot ucode
                    clear_pi_offset_tr();
                    set_wr_vref_tr(LW_TRUE);
                    set_cors_step_override(cors_step_override_wr);

                    //[bswamy] WR should never use initial vref BKV
                    //if(!initial_vref_bkv) {
                    osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300f); //FALCON_BOOT_TR_STATE_WR_VREF_SETUP
                    for(eye_idx = 0; eye_idx < eyes; eye_idx++) {
                        set_eye_mask(eye_idx, LW_FALSE);
                        if(eye_idx != EYE_HIGH) {
                            set_receiver_mask_dram(eye_idx, LW_FALSE); 
                        }
                        else {
                            //Reset eye mask
                            set_receiver_mask_dram(eye_idx, LW_TRUE); 
                        }
                        if(initial_vref_max_window == 1) {
                            start_training(TRAINING_DIR_WRITE);
                            gddr_wait_for_training_done(); //poll for training done
                        }
                        if(initial_vref_area || initial_vref_bkv) {
                            set_wr_vref_tr_area(LW_TRUE); 
                            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_VREF_INIT_DRAM_SWEEP_DQ, eye_idx, 0, loop_idx);
                            do_area_based_tr_init(TRAINING_DIR_WRITE);
                            if(!gddr_check_training_passed()) {
                                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                                FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_VREF_EYE_LOW_ERROR + eye_idx);
                            }
                        }
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453012); //FALCON_BOOT_TR_STATE_WR_VREF_EYE_CLEANUP
                    } // End of eyes loop for initial training
                    bootTrStepTimes[BOOT_TR_TIME_WR_DQ_VREF] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);
                    //} //!initial_vref_bkv
                    falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_DRAM_INIT_VREF, 0, 0, loop_idx);

                    // At this point initial VREF has been programmed
                    // for all DQs. Need to run PI offset loop 
                    // without receiver masking to improve VREF

                    // clear receiver masking
                    set_receiver_mask_dram(0, LW_TRUE);
                    if(!initial_vref_area) {
                        set_wr_vref_tr_area(LW_TRUE);
                    }

                    // Clear cors step override
                    set_cors_step_override(LW_FALSE);

                    // Disable VREF training to do PI training
                    set_wr_vref_tr(LW_FALSE);

                    // Do PI training
                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453013); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_SETUP
                    start_training(TRAINING_DIR_WRITE);
                    gddr_wait_for_training_done(); //poll for training done
                    if(!gddr_check_training_passed()) {
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_PI_ERROR);
                    }

                    set_pi_offset_tr(TRAINING_DIR_WRITE);
                    set_wr_vref_tr(LW_TRUE);
                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453014); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_EYE_SETUP

                    osPTimerTimeNsLwrrentGet(&bootTrLwrrentStepStartTime);
                    //LwU8 eye_idx;
                    for(eye_idx = 0; eye_idx < eyes; eye_idx++) {
                        set_eye_mask(eye_idx, LW_FALSE);
                        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_PI_OFFSET_SWEEP, eye_idx, 0, loop_idx);
                        do_2_step_pi_offset_tr(TRAINING_DIR_WRITE, 0);
                        if(!gddr_check_training_passed()) {
                            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_2STEP_PI_EYE_LOW_ERROR + eye_idx);
                        }
                        read_per_pin_vref_ob(0,eye_idx); 
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453017); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_EYE_CLEANUP
                        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_PI_OFFSET_AREA_VREF, eye_idx, 0, loop_idx);
                    }

                    //Update the best_vref_* arrays with the vref data we just read in this averaging loop
                    select_optimal_dfe_vref(0, 0, 0, LW_FALSE, TRAINING_DIR_WRITE);

                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453018); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_CLEANUP
                    falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_PI_OFFSET_AVG_AREA_VREF_DFE, 0, 0, loop_idx);
                    bootTrStepTimes[BOOT_TR_TIME_WR_DQ_VREF_OFF] = osPTimerTimeNsElapsedGet(&bootTrLwrrentStepStartTime);
                }

                // All averaging loops are done
                // Average the VREF to get the final optimal VREF
                average_optimal_dfe_vref(averaging_loops_wr,0,0);

                //Add in any offsets programmed into vBIOS
                add_dfe_vref_offsets(TRAINING_DIR_WRITE, 0, 0);

                // Program the final optimal VREF for DQs
                program_optimal_vref_dq_ob(); 
                set_wr_vref_tr(LW_FALSE);
                clear_pi_offset_tr();
                set_wr_vref_tr_area(LW_FALSE);
                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_DQ, BOOT_TR_DEBUG_CMD_FINAL_AREA_VREF_DFE, 0, 0, 0);

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
                // In this step only the write dq values are populated,
                dmemWritePAFalcon((LwU32*)&best_vref_per_dq_low[0], sizeof(best_vref_per_dq_low),gPafalconDebugWriteAdr);
                gPafalconDebugWriteAdr += sizeof(best_vref_per_dq_low);
                dmemWritePAFalcon((LwU32*)&best_vref_per_dq_mid[0], sizeof(best_vref_per_dq_mid),gPafalconDebugWriteAdr);
                gPafalconDebugWriteAdr += sizeof(best_vref_per_dq_mid);
                dmemWritePAFalcon((LwU32*)&best_vref_per_dq_high[0], sizeof(best_vref_per_dq_high),gPafalconDebugWriteAdr);
                gPafalconDebugWriteAdr += sizeof(best_vref_per_dq_high);
                // udpate debug blocks
                gAonStateInfo.debugDataBlks = (gPafalconDebugWriteAdr + 255) / 256;
#endif
            } //bFlagDoWrTr

            // VREF/DFE is done for all DQ. Run a final PI training to fix OB PI
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545301d); //FALCON_BOOT_TR_STATE_WR_DQ_FINAL
            start_training(TRAINING_DIR_WRITE);
            gddr_wait_for_training_done();
            if(!gddr_check_training_passed()) {
                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQ_FINAL_PI_ERROR);
            }
            bootTrStepTimes[BOOT_TR_TIME_WR_DQ_TR] = osPTimerTimeNsElapsedGet(&bootTrWrTrStart);

            // DQ loop is complete, Start the DBI loop if needed
            if (boot_tr_mta_en) {
                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453019); //FALCON_BOOT_TR_STATE_WR_MTA_SETUP
                // Enable MTA in the dram
                set_dram_mta(LW_TRUE);
                set_rd_dbi_expected(LW_FALSE);

                if (bFlagDoWrTr) {
                    set_vref_sweep_dram(); // Sweep range should be same for all eyes

                    // Initialize arrays used to store final optimal
                    initialize_best_arrays();

                    LwU8 loop_idx;
                    for(loop_idx = 1; loop_idx <= averaging_loops_wr; loop_idx++){
                        LwU8  eye_idx;

                        set_wr_vref_tr(LW_TRUE);
                        clear_pi_offset_tr();
                        set_cors_step_override(cors_step_override_rd);

                        // Initial VREF training
                        //[bswamy] WR should never use initial vref BKV
                        //if(!initial_vref_bkv) {
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545300f); //FALCON_BOOT_TR_STATE_WR_VREF_SETUP
                        for( eye_idx = 0; eye_idx < eyes; eye_idx = eye_idx + 1) {
                            set_eye_mask(eye_idx, LW_FALSE);
                            if(eye_idx != EYE_HIGH) {
                                set_receiver_mask_dram(eye_idx, LW_FALSE); 
                            }
                            else {
                                //Reset eye mask
                                set_receiver_mask_dram(eye_idx, LW_TRUE); 
                            }
                            if(initial_vref_max_window == 1) {
                                start_training(TRAINING_DIR_WRITE);
                                gddr_wait_for_training_done(); //poll for training done
                            }
                            if(initial_vref_area || initial_vref_bkv) {
                                set_wr_vref_tr_area(LW_TRUE);
                                falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_MTA, BOOT_TR_DEBUG_CMD_VREF_INIT_DRAM_SWEEP_DQ, eye_idx, 0, loop_idx);
                                do_area_based_tr_init(TRAINING_DIR_WRITE);
                                if(!gddr_check_training_passed()) {
                                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                                    FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_VREF_EYE_LOW_ERROR + eye_idx);
                                }
                            }
                            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453012); //FALCON_BOOT_TR_STATE_WR_VREF_EYE_CLEANUP
                        } // End of eyes loop for initial training
                        //} //!initial_vref_bkv
                        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_MTA, BOOT_TR_DEBUG_CMD_DRAM_INIT_VREF, 0, 0, loop_idx);

                        // At this point initial VREF has been programmed
                        // for each eye for DQX pins
                        set_receiver_mask_dram(0, LW_TRUE);
                        if(!initial_vref_area) {
                            set_wr_vref_tr_area(LW_TRUE);
                        }

                        // Clear cors step override
                        set_cors_step_override(LW_FALSE);

                        // Disable VREF training to do PI training
                        set_wr_vref_tr(LW_FALSE);	    

                        // Do PI training
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453013); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_SETUP
                        start_training(TRAINING_DIR_WRITE);
                        gddr_wait_for_training_done();
                        if(!gddr_check_training_passed()) {
                            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_PI_ERROR);
                        }
                        // Training should pass here are all DQ and DQX pins should
                        // be able train - although actually only DQX pins are being trained

                        set_pi_offset_tr(TRAINING_DIR_WRITE);
                        set_wr_vref_tr(LW_TRUE);
                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453014); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_EYE_SETUP

                        //LwU8 eye_idx;
                        for(eye_idx = 0; eye_idx < eyes; eye_idx++) {
                            set_eye_mask(eye_idx, LW_FALSE);
                            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_MTA, BOOT_TR_DEBUG_CMD_PI_OFFSET_SWEEP, eye_idx, 0, loop_idx);
                            do_2_step_pi_offset_tr(TRAINING_DIR_WRITE, 0);
                            if(!gddr_check_training_passed()) {
                                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                                FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_2STEP_PI_EYE_LOW_ERROR + eye_idx);
                            }
                            read_per_pin_vref_ob_dbi(0,eye_idx); 
                            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453017); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_EYE_CLEANUP
                            falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_MTA, BOOT_TR_DEBUG_CMD_PI_OFFSET_AREA_VREF, eye_idx, 0, loop_idx);
                        }

                        //Update the best_vref_* arrays with the vref data we just read in this averaging loop
                        select_optimal_dfe_vref(0, 0, 0, LW_TRUE, TRAINING_DIR_WRITE);

                        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05453018); //FALCON_BOOT_TR_STATE_WR_PI_OFFSET_CLEANUP
                        falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_MTA, BOOT_TR_DEBUG_CMD_PI_OFFSET_AVG_AREA_VREF_DFE, 0, 0, loop_idx);
                    } // End of averaging loop for DQX training
                    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545301a); //FALCON_BOOT_TR_STATE_WR_MTA_CLEANUP

                    // All averaging loops are done. Average VREF
                    // to get the final VREF
                    average_optimal_dfe_vref(averaging_loops_wr,1,1);

                    //Add in any offsets programmed into vBIOS
                    add_dfe_vref_offsets(TRAINING_DIR_WRITE, 1, 1);

                    // Program the final optimal VREF for DQX pins
                    program_optimal_vref_dbi_ob(); 
                    set_wr_vref_tr(LW_FALSE);
                    clear_pi_offset_tr();
                    falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_WR_MTA, BOOT_TR_DEBUG_CMD_FINAL_AREA_VREF_DFE, 0, 0, 0);


#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_STORAGE))
                    // In this step only the write dbi values are populated,
                    dmemWritePAFalcon((LwU32*)&best_vref_per_dbi_low[0], sizeof(best_vref_per_dbi_low),gPafalconDebugWriteAdr);
                    gPafalconDebugWriteAdr += sizeof(best_vref_per_dbi_low);
                    dmemWritePAFalcon((LwU32*)&best_vref_per_dbi_mid[0], sizeof(best_vref_per_dbi_mid),gPafalconDebugWriteAdr);
                    gPafalconDebugWriteAdr += sizeof(best_vref_per_dbi_mid);
                    dmemWritePAFalcon((LwU32*)&best_vref_per_dbi_high[0], sizeof(best_vref_per_dbi_high),gPafalconDebugWriteAdr);
                    gPafalconDebugWriteAdr += sizeof(best_vref_per_dbi_high);
                    // update debug blocks
                    gAonStateInfo.debugDataBlks = (gPafalconDebugWriteAdr + 255) / 256;
#endif
                } //bFlagDoWrTr
            }
        } // End GDDR6X training
        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            if(enable_pi_offset_tr_wr) {
                LwU8 loop_idx;
                for(loop_idx = 1; loop_idx <= averaging_loops_wr; loop_idx++) {
                    set_dram_dfe_dq_sweep(dfe_min_wr); 

                    // Initial VREF training for DQs
                    if(!initial_vref_bkv) {
                        set_vref_sweep_dram(); // Sweep range should be same for all eyes
                        if(initial_vref_max_window == 1) {
                            start_training(TRAINING_DIR_WRITE);
                            gddr_wait_for_training_done();
                        }
                        if(initial_vref_area) {
                            set_wr_vref_tr_area(LW_TRUE); 
                            do_area_based_tr_init(TRAINING_DIR_WRITE);
                        }
                    } //!initial_vref_bkv
                    if(!initial_vref_area) {
                        set_wr_vref_tr_area(LW_TRUE);
                    }

                    // clear cors step override
                    set_cors_step_override(LW_FALSE);

                    LwU8 lwr_dfe;
                    for(lwr_dfe = dfe_min_wr; lwr_dfe <= dfe_max_wr; lwr_dfe = lwr_dfe + dfe_step_wr) {
                        set_dram_dfe_dq_sweep(lwr_dfe);
                        set_wr_vref_tr(LW_FALSE);

                        // Do normal PI training
                        // check for status done.  Training will likely fail due to
                        // DBI not being able to train well
                        start_training(TRAINING_DIR_WRITE);
                        gddr_wait_for_training_done();

                        set_pi_offset_tr(TRAINING_DIR_WRITE);
                        set_wr_vref_tr(LW_TRUE);
                        set_wr_vref_tr_area(LW_TRUE);

                        do_2_step_pi_offset_tr(TRAINING_DIR_WRITE,lwr_dfe);

                        read_per_pin_area(lwr_dfe,EYE_LOW); // Will use only low for GDDR6
                        read_per_pin_vref_ob(lwr_dfe,EYE_LOW);

                        clear_pi_offset_tr();
                    } // DFE loops

                    // At this point all areas have been saved per DFE , per DQ
                    // Do moving average here if needed only for DQs
                    update_area_moving_avg(dfe_min_wr, dfe_max_wr, dfe_step_wr, 0);
                    select_optimal_dfe_vref(dfe_min_wr, dfe_max_wr, dfe_step_wr, 0, TRAINING_DIR_WRITE);
                } // Averaging loops for DQ
                // Average DFE and VREf
                average_optimal_dfe_vref(averaging_loops_wr, 0, 0 );

                //Add in any offsets programmed into vBIOS
                add_dfe_vref_offsets(TRAINING_DIR_WRITE, 0, 0);

                program_optimal_vref_dq_ob(); 
                program_optimal_dfe_dq_ob(); 

                set_wr_vref_tr(LW_FALSE);
                clear_pi_offset_tr();
                set_wr_vref_tr_area(LW_FALSE);

                if (dbi_en) {
                    // Repeat the same training for DBI
                    set_dram_dbi(LW_TRUE);
                    set_rd_dbi_expected(LW_FALSE);
                    initialize_best_arrays();
                    set_wr_vref_tr(LW_TRUE);
                    set_cors_step_override(cors_step_override_rd);

                    LwU8 loop_idx;
                    for(loop_idx = 1; loop_idx <= averaging_loops_wr; loop_idx++){
                        // Initial VREF training
                        if(!initial_vref_bkv) {
                            set_vref_sweep_dram(); // Sweep range should be same for all eyes
                            if(initial_vref_max_window == 1) {
                                start_training(TRAINING_DIR_WRITE);
                                gddr_wait_for_training_done(); // poll for training done
                            }
                            if(initial_vref_area) {
                                set_wr_vref_tr_area(LW_TRUE);
                                do_area_based_tr_init(TRAINING_DIR_WRITE);
                            }

                        } //!initial_vref_bkv
                        // Initial VREF is now programmed for DBI pins

                        if(!initial_vref_area) {
                            set_wr_vref_tr_area(LW_TRUE);
                        }

                        set_cors_step_override(LW_FALSE);

                        LwU8 lwr_dfe;
                        for(lwr_dfe = dfe_min_wr;lwr_dfe <= dfe_max_wr; lwr_dfe = lwr_dfe + dfe_step_wr) {
                            set_dram_dfe_dbi_sweep(lwr_dfe);
                            set_wr_vref_tr(LW_FALSE);

                            // Do normal PI training
                            // Should pass training so can check for status here
                            start_training(TRAINING_DIR_WRITE);
                            gddr_wait_for_training_done();
                            if(!gddr_check_training_passed()) {
                                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
                                FW_MBOX_WR32(13, lwr_dfe); //Current DFE to MBOX 13 for debug
                                FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_TRAINING_ERROR);
                            }

                            set_pi_offset_tr(TRAINING_DIR_WRITE);
                            set_wr_vref_tr(LW_TRUE);
                            set_wr_vref_tr_area(LW_TRUE);

                            do_2_step_pi_offset_tr(TRAINING_DIR_WRITE,lwr_dfe);

                            read_per_dbi_area(lwr_dfe,EYE_LOW); 
                            read_per_pin_vref_ob_dbi(lwr_dfe,EYE_LOW); 

                            clear_pi_offset_tr();
                        } // DFE loops

                        // At this point all areas have been saved per DFE, per DBI
                        // The following two functions can be made more efficient by only doing DBI in this call
                        update_area_moving_avg(dfe_min_wr, dfe_max_wr, dfe_step_wr, 1); 
                        select_optimal_dfe_vref(dfe_min_wr, dfe_max_wr, dfe_step_wr, 1, TRAINING_DIR_WRITE); 
                    } // End of averaging loops

                    // Average DFE and VREF
                    average_optimal_dfe_vref(averaging_loops_wr,1, 1);

                    //Add in any offsets programmed into vBIOS
                    add_dfe_vref_offsets(TRAINING_DIR_WRITE, 1, 1);

                    program_optimal_vref_dbi_ob();
                    program_optimal_dfe_dbi_ob();

                    set_wr_vref_tr(LW_FALSE);
                    clear_pi_offset_tr();
                    set_wr_vref_tr_area(LW_FALSE);
                }

            } // PI Offset Training Loop
            else {
                // Turing style training
                // Add here if code size allows
            }
        }
        bootTrStepTimes[BOOT_TR_TIME_WR_TR] = osPTimerTimeNsElapsedGet(&bootTrWrTrStart);
    } //bFlagDoWrTr || boot_tr_mta_en

    // VREF/DFE is done for all - read and write. Run a final PI training to 
    // fix OB PI
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545301b); //FALCON_BOOT_TR_STATE_WR_FINAL
    start_training(TRAINING_DIR_WRITE);
    gddr_wait_for_training_done();
    if(!gddr_check_training_passed()) {
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xDEADDEAD);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_DQX_FINAL_PI_ERROR);
    }


    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0545301c); //FALCON_BOOT_TR_STATE_CLEANUP

    //=========================================================================
    //End of Read/Write Training
    //=========================================================================
    set_rd_dbi_expected(LW_TRUE); //Set RD DBI expected to enable MTA/DBI
    set_eye_mask(0, LW_TRUE);     //Clear the eye mask
    set_tag_register(0,0,0);      //Clear training tag
    func_set_prbs_dual_mode(0,1); // Disable PRBS_mode for subsequent periodic training

    //Restore registers settings as they were when boot training started
    //Need to update some of the registers in the lwr_reg structure so mclk switch code
    //behaves properly
    restore_saved_regs();

    //Complete the P0 mclk switch
    doMclkSwitchInMultipleStages(p0Freq , 0,0,1);

    //Restore FBIO power control settings
    set_fbio_pwr_ctrl(saved_reg->fbio_pwr_ctrl, saved_reg->fbio_pwr_ctrl1);

    //Re-enable training updates for vref tracking
    set_vref_tracking_disable_training_updates(LW_FALSE);

    // Switch to P8.
    gddr_in_boot_training(LW_FALSE); //Indicate to mclk switch code that we're done with boot training
    if ((p8Freq !=0) && bFlagDoP0P8Final) {
        doMclkSwitchPrimary(p8Freq, 0, 0, 0);
    }


    bootTrStepTimes[BOOT_TR_TIME_BOOT_TR] = osPTimerTimeNsElapsedGet(&bootTrStartTime);

    //Dump the boot training times into some mailboxes
    FW_MBOX_WR32(2,  bootTrStepTimes[BOOT_TR_TIME_RD_VREF]);
    FW_MBOX_WR32(3,  bootTrStepTimes[BOOT_TR_TIME_RD_DFE]);
    FW_MBOX_WR32(4,  bootTrStepTimes[BOOT_TR_TIME_RD_TR]);
    FW_MBOX_WR32(5,  bootTrStepTimes[BOOT_TR_TIME_WR_DQ_VREF]);
    FW_MBOX_WR32(6,  bootTrStepTimes[BOOT_TR_TIME_WR_DQ_VREF_OFF]);
    FW_MBOX_WR32(7,  bootTrStepTimes[BOOT_TR_TIME_WR_DQ_TR]);
    FW_MBOX_WR32(8,  bootTrStepTimes[BOOT_TR_TIME_WR_TR]);
    FW_MBOX_WR32(9,  bootTrStepTimes[BOOT_TR_TIME_BOOT_TR]);
    FW_MBOX_WR32(10, bootTrStepTimes[BOOT_TR_TIME_CHAR]);
    FW_MBOX_WR32(11, bootTrStepTimes[BOOT_TR_TIME_ADDR]);
    FW_MBOX_WR32(12, bootTrStepTimes[BOOT_TR_TIME_WCK]);
    falcon_boot_tr_breakpoint(BOOT_TR_DEBUG_CMD_MISC, BOOT_TR_DEBUG_CMD_MISC_END_OF_TRAINING, 0, 0, 0);

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    privprofilingDisable();
#endif

    //Restore mclk switch enable
    gbl_en_fb_mclk_sw = save_gbl_en_fb_mclk_sw;

    //If not doing the final P0->P8 switch, wait here
    if (!bFlagDoP0P8Final) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_HALT_COMMAND);
    }

    LwU32 result = 0xabcd1234;
    FW_MBOX_WR32(1, 0xfeefeffe);
    return result;
}

