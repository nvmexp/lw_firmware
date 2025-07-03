#include <falcon-intrinsics.h>
#include <falc_debug.h>
#include <falc_trace.h>

#include "fbflcn_gddr_boot_time_training_tu10x.h"
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

extern TRAINING_DATA gTD;

// Local defines
#define LW_PFB_FBPA_GENERIC_MRS15_PSEUDO_CH                         9:8
#define LW_PFB_FBPA_GENERIC_MRS15_CH_SEL                            1:0
#define LW_PFB_FBPA_GENERIC_MRS9_DFE                                3:0
#define LW_PFB_FBPA_GENERIC_MRS9_PIN_SUB_ADDRESS                   11:7
#define LW_PFB_FBPA_GENERIC_MRS6_VREF                               6:0
#define LW_PFB_FBPA_GENERIC_MRS6_PIN_SUB_ADDRESS                   11:7
#define LW_PFB_FBPA_GENERIC_MRS2_EDC_HR                            11:11
#define LW_PFB_FBPA_GENERIC_MRS2_EDC_MODE                           8:8
#define LW_PFB_FBPA_GENERIC_MRS2_CADT_SRF                          10:10
#define LW_PFB_FBPA_FBIO_SPARE_FORCE_MA2SDR                         7:7
#define LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0                         7:0
#define LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1                        15:8
#define LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2                        23:16
#define LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3                        31:24
#define LW_PFB_FBPA_TRAINING_8BIT_WR_VREF                          19:12
#define LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW                   11:0
#define LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW                   23:12
#define LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA                     15:0
#define LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA                     31:16
#define EDC_TRACKING_RD_GAIN                                       10:5
#define VREF_TRACKING_GAIN_INITIAL                                 16:11
#define VREF_TRACKING_GAIN_FINAL                                   22:17
#define EDC_TRACKING_STRAPA                                         5:0
#define EDC_TRACKING_STRAPB                                        11:6
#define EDC_TRACKING_STRAPC                                        17:12
#define EDC_TRACKING_STRAPD                                        23:18
#define EDC_TRACKING_STRAPE                                        29:24


typedef struct REGBOX1 {
LwU32 pfb_fbpa_training_wck_edc_limit;
LwU32 pfb_fbpa_training_wck_intrpltr_ctrl_ext;
LwU32 pfb_fbpa_training_wck_intrpltr_ctrl;
LwU32 pfb_fbpa_training_rw_vref_ctrl;
LwU32 pfb_fbpa_training_ctrl;
LwU32 pfb_fbpa_training_rw_brlshft_quick_ctrl;
LwU32 pfb_fbpa_training_rw_cors_intrpltr_ctrl;
LwU32 pfb_fbpa_training_rw_fine_intrpltr_ctrl;
LwU32 pfb_fbpa_training_rw_wr_vref_ctrl;
LwU32 pfb_fbpa_training_cmd;

LwU32 training_patram_store;
LwU32 training_char_bank_ctrl_store;
LwU32 training_char_bank_ctrl2_store;
LwU32 training_char_burst_store;
LwU32 training_ctrl_store;
LwU32 training_char_ctrl_store;
LwU32 training_ctrl2;
LwU32 training_tag;
} REGBOX1;

REGBOX1 lwrrentRegValues1;
REGBOX1*  saved_reg = &lwrrentRegValues1;

// Reading flags from VBIOS
LwBool bFlagG6AddrTrEn, bFlagG6WckTrEn, bFlagG6RdTrEn, bFlagG6RdTrPrbsEn;
LwBool bFlagG6RdTrAreaEn, bFlagG6RdTrHybridNolwrefEn, bFlagG6RdTrHybridVrefEn;
LwBool FlagG6RdTrHwVrefEn , FlagG6RdTrRedoPrbsSettings, FlagG6RdTrPatternShiftTrEn;
LwBool FlagG6WrTrEn, bFlagG6WrTrPrbsEn, FlagG6WrTrAreaEn, FlagG6WrTrHybridNolwrefEn;
LwBool FlagG6WrTrHybridVrefEn, FlagG6WrTrHwVrefEn, FlagG6PatternShiftTrEn;
LwBool bFlagG6RdTrHwVrefEn, bFlagG6RdTrRedoPrbsSettings, bFlagG6WrTrEn, bFlagG6WrTrAreaEn;
LwBool bFlagG6WrTrHybridNolwrefEn, bFlagG6WrTrHybridVrefEn, bFlagG6WrTrHybridVrefEn;
LwBool bFlagG6WrTrHwVrefEn, bFlagG6TrainRdWrDifferent, bFlagG6TrainPrbsRdWrDifferent;
LwBool bFlagG6EdcTrackingEn, bFlagG6RdEdcEnabled, bFlagG6WrEdcEnabled, bFlagG6RefreshEnPrbsTr;
LwBool bFlagG6VrefTrackingEn;

// Falcon debug variable
LwU32 falcon_to_determine_best_window = 0;

// Reading values from VBIOS
LwU32 var_wr_vref_min = 0;                          // NOTE has to come from VBIOS
LwU32 var_wr_vref_max = 2;                          // NOTE has to come from VBIOS
LwU32 var_wr_vref_step = 2;                         // NOTE has to come from VBIOS

LwU32 aclwm_bkv = 0;
LwU32 edge_offset_bkv = 0;
LwU32 edc_tracking_mode = 1;				// NOTE has to come from VBIOS
LwU32 edc_relock_wait_time = 10;			// NOTE has to come from VBIOS
LwU32 pi_movement_en	= 0;				// NOTE has to come from VBIOS
LwU32 dq_sel	= 4;						// NOTE has to come from VBIOS
LwU32 byte_sel = 4;						// NOTE has to come from VBIOS
LwU32 dir_sel = 0;						// NOTE has to come from VBIOS
LwU32 disable_refresh_recenter = 0;		// NOTE has to come from VBIOS
LwU32 disable_wdqs_pi_updates	 = 1;		// NOTE has to come from VBIOS
LwU32 zero_out_offset_cal = 0;			// NOTE has to come from VBIOS
LwU32 rd_flip_dir = 0;
LwU32 wr_flip_dir = 0;
LwU32 rd_gain = 32;
LwU32 wr_gain = 8;
LwU32 edge_intrp_offset = 64;
LwU32 edge_brlshft = 0;
LwU32 center_brlshft = 0;
LwU32 dis_tr_update = 0;
LwU32 block_cnt = 7;
LwU32 en = 0;
LwU32 hunt_mode = 0;
LwU32 use_center = 1;
LwU32 half_rate_edc = 1;
//LwU32 flip_direction = 0;
//LwU32 gain = 0; 							// vref_gain. has to come from VBIOS
//LwU32 disable_training_updates = 0;
//LwU32 aclwm = 0;
//LwU32 vref_en = 0;
//LwU32 debug_vref_movement_en = 0;
//LwU32 vref_edge_pwrd = 0;
//LwU32 edge_hssa_pwrd = 0;
//LwU32 edge_interp_pwrd = 0;


FLCN_TIMESTAMP bootTrTimeNs ={0};
LwU32 bootTrElapsedTime;


// Simple translator to get a unicast address based off the broadcast address
// and the partition index
LwU32 multi_broadcast_wr_vref_dfe (LwU32 priv_addr, LwU32 partition, LwU32 subp, LwU32 byte)
{
    LwU32 retval;

    retval = priv_addr + (partition * 0x00004000) + (subp * 48) + (byte * 12);

    return (retval);
}

void func_ma2sdr
(
        LwU32 force_ma2sdr
)
{
    LwU32 fbpa_fbio_spare;
    fbpa_fbio_spare= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_SPARE);
    fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE,       _FORCE_MA2SDR,  force_ma2sdr,      fbpa_fbio_spare);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE,       fbpa_fbio_spare);

}

void func_edc_ctrl
(
        LwU32 edc_tracking_mode,
        LwU32 edc_relock_wait_time
)
{
    LwU32 fbpa_training_edc_ctrl;
    fbpa_training_edc_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_EDC_CTRL);
    fbpa_training_edc_ctrl = FLD_SET_DRF_NUM(_PFB,  _FBPA_TRAINING_EDC_CTRL,        _EDC_TRACKING_MODE,      edc_tracking_mode,          fbpa_training_edc_ctrl);
    fbpa_training_edc_ctrl = FLD_SET_DRF_NUM(_PFB,  _FBPA_TRAINING_EDC_CTRL,        _EDC_RELOCK_WAIT_TIME,   edc_relock_wait_time,       fbpa_training_edc_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_EDC_CTRL,        fbpa_training_edc_ctrl);
}


void func_setup_edc_rate
(
        LwU8 edc_rate
)
{
    LwU32 fbpa_cfg0;
    fbpa_cfg0= REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB,       _FBPA_CFG0,     _EDC_RATE,  edc_rate,      fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,     fbpa_cfg0);

    LwU32 generic_mrs2;
    generic_mrs2 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS2);
    generic_mrs2 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS2, _EDC_MODE, ~edc_rate, generic_mrs2);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS2,generic_mrs2);

}

void func_edc_tracking
(
        void
)
{
    EdcTrackingGainTable my_EdcTrackingGainTable;
    // Step a. TRAINING_EDC_CTRL_EDC_TRACKING_MODE=1
    // VBIOS will have edc_tracking_mode=1 and edc_relock_wait_time=10
    REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_EDC_CTRL,gTT.bootTrainingTable.MISC_EDC_CTRL);
    LwU8 edcTrackingRdGainInitial = gTT.bootTrainingTable.EDC_VREF_TRACKING.EDC_READ_GAIN_INITIAL;
    LwU8 vrefTrackingGainInitial  = gTT.bootTrainingTable.EDC_VREF_TRACKING.VREF_GAIN_INITIAL;
    LwU8 VrefTrackingGainFinal    = gTT.bootTrainingTable.EDC_VREF_TRACKING.VREF_GAIN_FINAL;

    LwU32 edcTrackingRdGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN;
    LwU8 edcTrackingRdGainFinalStrap0 = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap1 = REF_VAL(EDC_TRACKING_STRAPB ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap2 = REF_VAL(EDC_TRACKING_STRAPC ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap3 = REF_VAL(EDC_TRACKING_STRAPD ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap4 = REF_VAL(EDC_TRACKING_STRAPE ,edcTrackingRdGain);

    edcTrackingRdGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN1;
    LwU8 edcTrackingRdGainFinalStrap5 = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap6 = REF_VAL(EDC_TRACKING_STRAPB ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap7 = REF_VAL(EDC_TRACKING_STRAPC ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap8 = REF_VAL(EDC_TRACKING_STRAPD ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap9 = REF_VAL(EDC_TRACKING_STRAPE ,edcTrackingRdGain);

    edcTrackingRdGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN2;
    LwU8 edcTrackingRdGainFinalStrap10 = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap11 = REF_VAL(EDC_TRACKING_STRAPB ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap12 = REF_VAL(EDC_TRACKING_STRAPC ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap13 = REF_VAL(EDC_TRACKING_STRAPD ,edcTrackingRdGain);
    LwU8 edcTrackingRdGainFinalStrap14 = REF_VAL(EDC_TRACKING_STRAPE ,edcTrackingRdGain);

    edcTrackingRdGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN3;
    LwU8 edcTrackingRdGainFinalStrap15 = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingRdGain);

    LwU32 edcTrackingWrGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN4;
    LwU8 edcTrackingWrGainInitial     = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap0 = REF_VAL(EDC_TRACKING_STRAPB ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap1 = REF_VAL(EDC_TRACKING_STRAPC ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap2 = REF_VAL(EDC_TRACKING_STRAPD ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap3 = REF_VAL(EDC_TRACKING_STRAPE ,edcTrackingRdGain);

    edcTrackingWrGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN5;
    LwU8 edcTrackingWrGainFinalStrap4 = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap5 = REF_VAL(EDC_TRACKING_STRAPB ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap6 = REF_VAL(EDC_TRACKING_STRAPC ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap7 = REF_VAL(EDC_TRACKING_STRAPD ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap8 = REF_VAL(EDC_TRACKING_STRAPE ,edcTrackingRdGain);

    edcTrackingWrGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN6;
    LwU8 edcTrackingWrGainFinalStrap9 = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap10 = REF_VAL(EDC_TRACKING_STRAPB ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap11 = REF_VAL(EDC_TRACKING_STRAPC ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap12 = REF_VAL(EDC_TRACKING_STRAPD ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap13 = REF_VAL(EDC_TRACKING_STRAPE ,edcTrackingRdGain);

    edcTrackingWrGain = gTT.bootTrainingTable.EDC_TRACKING_RDWR_GAIN7;
    LwU8 edcTrackingWrGainFinalStrap14 = REF_VAL(EDC_TRACKING_STRAPA ,edcTrackingWrGain);
    LwU8 edcTrackingWrGainFinalStrap15 = REF_VAL(EDC_TRACKING_STRAPB ,edcTrackingWrGain);

    LwU8 edcTrackingRdGainFinal = 0;
    LwU8 edcTrackingWrGainFinal = 0;
    if (gBiosTable.strap ==0 ) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap0;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap0;
    } else if (gBiosTable.strap == 1) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap1;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap1;
    } else if (gBiosTable.strap == 2) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap2;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap2;
    } else if (gBiosTable.strap == 3) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap3;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap3;
    } else if (gBiosTable.strap == 4) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap4;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap4;
    } else if (gBiosTable.strap == 5) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap5;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap5;
    } else if (gBiosTable.strap == 6) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap6;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap6;
    } else if (gBiosTable.strap == 7) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap7;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap7;
    } else if (gBiosTable.strap == 8) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap8;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap8;
    } else if (gBiosTable.strap == 9) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap9;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap9;
    } else if (gBiosTable.strap == 10) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap10;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap10;
    } else if (gBiosTable.strap == 11) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap11;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap11;
    } else if (gBiosTable.strap == 12) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap12;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap12;
    } else if (gBiosTable.strap == 13) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap13;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap13;
    } else if (gBiosTable.strap == 14) {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap14;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap14;
    } else {
        edcTrackingRdGainFinal = edcTrackingRdGainFinalStrap15;
        edcTrackingWrGainFinal = edcTrackingWrGainFinalStrap15;
    }

    my_EdcTrackingGainTable.EDCTrackingRdGainInitial = edcTrackingRdGainInitial;
    my_EdcTrackingGainTable.EDCTrackingRdGainFinal   = edcTrackingRdGainFinal;
    my_EdcTrackingGainTable.EDCTrackingWrGainInitial = edcTrackingWrGainInitial;
    my_EdcTrackingGainTable.EDCTrackingWrGainFinal   = edcTrackingWrGainFinal;
    my_EdcTrackingGainTable.VrefTrackingGainInitial  = vrefTrackingGainInitial;
    my_EdcTrackingGainTable.VrefTrackingGainFinal    = VrefTrackingGainFinal;

    //updating interpolator offset
    func_fbio_intrpltr_offsets(38,60);
    //Edc tracking setup
    gddr_setup_edc_tracking(0,&my_EdcTrackingGainTable);

    FLCN_TIMESTAMP  startFbStopTimeNs = { 0 };

    //Start edc tracking
    gddr_start_edc_tracking(0,bFlagG6EdcTrackingEn,bFlagG6VrefTrackingEn,&my_EdcTrackingGainTable,startFbStopTimeNs);

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
    LwU32 fbpa_training_patram;
    fbpa_training_patram= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    fbpa_training_patram = FLD_SET_DRF_NUM(_PFB,    _FBPA_TRAINING_PATRAM,  _DUAL_MODE, dual_mode,  fbpa_training_patram);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM,  fbpa_training_patram);

    memorySetTrainingControlPrbsMode_HAL(&Fbflcn, prbs_mode);
}

void func_setup_g6_addr_tr (void)
{

    func_set_prbs_dual_mode(0,1);
    FuncLoadAdrPatterns(12);
    func_disable_char_bank_ctrl();
    func_ma2sdr(1);                        // FORCE_MA_TO_SDR_ALIGNMENT = 1 before kicking off address training
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_TADR_CTRL,gTT.bootTrainingTable.ADDR_TADR_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_CTRL,gTT.bootTrainingTable.ADDR_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_CORS_CTRL,gTT.bootTrainingTable.ADDR_CORS_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ADDR_FINE_CTRL,gTT.bootTrainingTable.ADDR_FINE_CTRL);
    return;
}

void func_restore_training_ctrls(void)
{
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(0),saved_reg->pfb_fbpa_training_cmd);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(1),saved_reg->pfb_fbpa_training_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_WCK_EDC_LIMIT,saved_reg->pfb_fbpa_training_wck_edc_limit);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_WCK_INTRPLTR_CTRL_EXT,saved_reg->pfb_fbpa_training_wck_intrpltr_ctrl_ext);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_WCK_INTRPLTR_CTRL,saved_reg->pfb_fbpa_training_wck_intrpltr_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL, saved_reg->pfb_fbpa_training_rw_vref_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL, saved_reg->pfb_fbpa_training_rw_wr_vref_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2, saved_reg->training_ctrl2 );
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TAG, saved_reg->training_tag);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL,saved_reg->pfb_fbpa_training_ctrl);                                  
    // GDDR5_COMMANDS = 1 for periodic tr
    LwU8 training_ctrl_gddr5_commands = 1;
    LwU32 training_ctrl_vref_training = 0;
    LwU32 fbpa_training_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
    fbpa_training_ctrl = FLD_SET_DRF_NUM(_PFB,      _FBPA_TRAINING_CTRL,    _VREF_TRAINING,         training_ctrl_vref_training,      fbpa_training_ctrl);   
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL,    fbpa_training_ctrl);

    memorySetTrainingControlGddr5Commands_HAL(&Fbflcn,training_ctrl_gddr5_commands);

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_BRLSHFT_QUICK_CTRL,saved_reg->pfb_fbpa_training_rw_brlshft_quick_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL,saved_reg->pfb_fbpa_training_rw_cors_intrpltr_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL,saved_reg->pfb_fbpa_training_rw_fine_intrpltr_ctrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_WCK_EDC_LIMIT,saved_reg->pfb_fbpa_training_wck_edc_limit);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM, saved_reg->training_patram_store);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL, saved_reg->training_char_bank_ctrl_store);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2, saved_reg->training_char_bank_ctrl2_store);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BURST,  saved_reg->training_char_burst_store);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_CTRL, saved_reg->training_char_ctrl_store);
    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL, saved_reg->training_ctrl_store);

}

void func_setup_g6_wck_tr(void)
{

    saved_reg->pfb_fbpa_training_wck_edc_limit= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_WCK_EDC_LIMIT);
    saved_reg->pfb_fbpa_training_wck_intrpltr_ctrl_ext = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_WCK_INTRPLTR_CTRL_EXT);
    saved_reg->pfb_fbpa_training_wck_intrpltr_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_WCK_INTRPLTR_CTRL);
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

void func_wait_for_training_done 
(
        void
)
{
    LwU32 fbpa_trng_status;
    LwU32 subp0_trng_status;
    LwU32 subp1_trng_status;

    do {
        //gddr_delay_boot(1*count_for_1us_delay_in_ps);
        // return TRAINING_STATUS_SUBP0_STATUS_FINISHED if subp0 is disabled
        fbpa_trng_status = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_STATUS);
        subp0_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS, fbpa_trng_status);
        subp1_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS, fbpa_trng_status);
    }while ((subp0_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS_RUNNING) || (subp1_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS_RUNNING));

    return;
}

void func_disable_edc_edc_trk( void)
{
    LwU32 rdedc = 0;            // This gbl variable does NOT come from VBIOS. Is this required to be gbl?
    LwU32 wredc = 0;            // This gbl variable does NOT come from VBIOS. Is this required to be gbl?
    LwU32 fbpa_cfg0;
    fbpa_cfg0= REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB,       _FBPA_CFG0,     _RDEDC,  rdedc,      fbpa_cfg0);
    fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB,       _FBPA_CFG0,     _WREDC,  wredc,      fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,     fbpa_cfg0);

    LwU32 fbpa_generic_mrs4;
    LwU32 adr_gddr5_rdcrc = 0;  // This gbl variable does NOT come from VBIOS. Is this required to be gbl?
    LwU32 adr_gddr5_wrcrc = 0;  // This gbl variable does NOT come from VBIOS. Is this required to be gbl?
    fbpa_generic_mrs4= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS4);
    fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS4,     _ADR_GDDR5_RDCRC,        adr_gddr5_rdcrc,    fbpa_generic_mrs4);
    fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS4,     _ADR_GDDR5_WRCRC,        adr_gddr5_wrcrc,    fbpa_generic_mrs4);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4,    fbpa_generic_mrs4);

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
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TIMING2,gTT.bootTrainingTable.GEN_TIMING2);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TIMING3,gTT.bootTrainingTable.GEN_TIMING3);
    return;
}

void func_setup_rd_prbs_regs (void)
{
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_PATRAM,gTT.bootTrainingTable.READ_PATRAM);

    LwU32 fbpa_training_patram = gTT.bootTrainingTable.READ_PATRAM;
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

void func_setup_rd_tr (
        LwU32 hybrid,
        LwU32 vref_hw,
        LwU32 prbs,
        LwU32 area,
        LwU32 redo_prbs_setting
)
{
    if (prbs) {
        if ( redo_prbs_setting ) {
            func_setup_rd_prbs_regs();
        }
        func_set_prbs_dual_mode(1,1);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BURST, 0x00000000);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2, 0x00002020);
    } else {
        func_disable_char_bank_ctrl();
        func_set_prbs_dual_mode(0,1);
    }

    if(vref_hw) {
        LwU32 fbpa_training_rw_vref_ctrl;
        LwU32 area_based;
        saved_reg->pfb_fbpa_training_rw_vref_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL);
        fbpa_training_rw_vref_ctrl = gTT.bootTrainingTable.READ_RW_VREF_CTRL;
        if( area && hybrid) {

            area_based = 1;
            fbpa_training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB,      _FBPA_TRAINING_RW_VREF_CTRL,    _AREA_BASED,     area_based,         fbpa_training_rw_vref_ctrl);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL,    fbpa_training_rw_vref_ctrl);

        } else {
            area_based = 0;
            fbpa_training_rw_vref_ctrl = FLD_SET_DRF_NUM(_PFB,      _FBPA_TRAINING_RW_VREF_CTRL,    _AREA_BASED,     area_based,         fbpa_training_rw_vref_ctrl);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_VREF_CTRL,    fbpa_training_rw_vref_ctrl);

        }
        // Enable hw vref training
        //saved_reg->pfb_fbpa_training_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
        //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL,gTT.bootTrainingTable->READ_CTRL);
        LwU32 fbpaTrainingCtrl;
        LwU32 vrefTraining = 1;
        fbpaTrainingCtrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
        fbpaTrainingCtrl = FLD_SET_DRF_NUM(_PFB,      _FBPA_TRAINING_CTRL,    _VREF_TRAINING,  vrefTraining,      fbpaTrainingCtrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL,    fbpaTrainingCtrl);

    }

    // This is PI training setting, applicable to all modes
    saved_reg->pfb_fbpa_training_rw_brlshft_quick_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_BRLSHFT_QUICK_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_BRLSHFT_QUICK_CTRL,gTT.bootTrainingTable.READ_RW_BRLSHFT_QUICK_CTRL);

    saved_reg->pfb_fbpa_training_rw_cors_intrpltr_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL,gTT.bootTrainingTable.READ_RW_CORS_INTRPLTR_CTRL);

    saved_reg->pfb_fbpa_training_rw_fine_intrpltr_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL,gTT.bootTrainingTable.READ_RW_FINE_INTRPLTR_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_ERR_LIMIT,gTT.bootTrainingTable.READ_RW_ERR_LIMIT);

    return;
}

void func_read_8_bit_output_per_pin_debug_registers
(
        LwU32 part,
        LwU32 *pfbpa_training_debug_dq0,
        LwU32 *pfbpa_training_debug_dq1,
        LwU32 *pfbpa_training_debug_dq2,
        LwU32 *pfbpa_training_debug_dq3,
        LwU32 *pfbpa_training_debug_dq4,
        LwU32 *pfbpa_training_debug_dq5,
        LwU32 *pfbpa_training_debug_dq6,
        LwU32 *pfbpa_training_debug_dq7,
        LwU32 *pfbpa_training_debug_dq8,
        LwU32 *pfbpa_training_debug_dq9,
        LwU32 *pfbpa_training_debug_dq10,
        LwU32 *pfbpa_training_debug_dq11,
        LwU32 *pfbpa_training_debug_dq12,
        LwU32 *pfbpa_training_debug_dq13,
        LwU32 *pfbpa_training_debug_dq14,
        LwU32 *pfbpa_training_debug_dq15

)
{
    LwU32 training_debug_dq0;
    LwU32 training_debug_dq1;
    LwU32 training_debug_dq2;
    LwU32 training_debug_dq3;

    training_debug_dq0   = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_DQ0(part));
    *pfbpa_training_debug_dq0 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0, training_debug_dq0);
    *pfbpa_training_debug_dq1 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1, training_debug_dq0);
    *pfbpa_training_debug_dq2 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2, training_debug_dq0);
    *pfbpa_training_debug_dq3 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3, training_debug_dq0);

    training_debug_dq1   = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_DQ1(part));
    *pfbpa_training_debug_dq4 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0, training_debug_dq1);
    *pfbpa_training_debug_dq5 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1, training_debug_dq1);
    *pfbpa_training_debug_dq6 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2, training_debug_dq1);
    *pfbpa_training_debug_dq7 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3, training_debug_dq1);

    training_debug_dq2   = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_DQ2(part));
    *pfbpa_training_debug_dq8  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0, training_debug_dq2);
    *pfbpa_training_debug_dq9  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1, training_debug_dq2);
    *pfbpa_training_debug_dq10 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2, training_debug_dq2);
    *pfbpa_training_debug_dq11 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3, training_debug_dq2);

    training_debug_dq3   = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_DQ3(part));
    *pfbpa_training_debug_dq12  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0, training_debug_dq3);
    *pfbpa_training_debug_dq13  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1, training_debug_dq3);
    *pfbpa_training_debug_dq14 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2, training_debug_dq3);
    *pfbpa_training_debug_dq15 = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3, training_debug_dq3);

}


void func_read_debug_registers
(
        LwU32 part,
        LwU32 sub_part,
        LwU32 *pfbpa_training_debug_dq0,
        LwU32 *pfbpa_training_debug_dq1,
        LwU32 *pfbpa_training_debug_dq2,
        LwU32 *pfbpa_training_debug_dq3
) {
    *pfbpa_training_debug_dq0 = REG_RD32(LOG, unicast_rd(LW_PFB_FBPA_TRAINING_DEBUG_DQ0(sub_part), part));
    *pfbpa_training_debug_dq1 = REG_RD32(LOG, unicast_rd(LW_PFB_FBPA_TRAINING_DEBUG_DQ1(sub_part), part));
    *pfbpa_training_debug_dq2 = REG_RD32(LOG, unicast_rd(LW_PFB_FBPA_TRAINING_DEBUG_DQ2(sub_part), part));
    *pfbpa_training_debug_dq3 = REG_RD32(LOG, unicast_rd(LW_PFB_FBPA_TRAINING_DEBUG_DQ3(sub_part), part));
}

void func_read_debug_area(
        LwU32 return_dq[TOTAL_DQ_BITS],
        LwU32 return_dbi[TOTAL_DBI_BITS],
        LwU32 return_edc[TOTAL_DBI_BITS],
        LwU32 edc_en,
        LwU32 dfe_lwr_setting
) 
{
    LwU32 idx;
    LwU32 subp;
    LwU32 ctrl_select;
    LwU32 true_ctrl_sel;
    LwU32 fbpa_training_debug_ctrl;
    LwU32 i;

    LwU32 dfe_setting_optimal_dq[TOTAL_DQ_BITS];
    LwU32 return_opt_dq[TOTAL_DQ_BITS]={0};
    LwU32 dfe_setting_optimal_dbi[TOTAL_DBI_BITS];
    LwU32 return_opt_dbi[TOTAL_DBI_BITS]={0};
    LwU32 dfe_setting_optimal_edc[TOTAL_DBI_BITS];
    LwU32 return_opt_edc[TOTAL_DBI_BITS]={0};

    LwU32 fbpa_training_debug_dq0;
    LwU32 fbpa_training_debug_dq1;
    LwU32 fbpa_training_debug_dq2;
    LwU32 fbpa_training_debug_dq3;

    for (ctrl_select = LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_ACLWM_AREA_0 ; ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_ACLWM_AREA_4; ctrl_select++) {
        true_ctrl_sel = ctrl_select - LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_ACLWM_AREA_0;
        fbpa_training_debug_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0));
        fbpa_training_debug_ctrl = FLD_SET_DRF_NUM(_PFB,        _FBPA_TRAINING_DEBUG_CTRL,      _SELECT,         ctrl_select,     fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0),      fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(1),      fbpa_training_debug_ctrl);

        for (idx = 0; idx < MAX_PARTS; idx++) 
        {
            if (isPartitionEnabled(idx))
            {
                for(subp = 0; subp < 2; subp++) {
                    if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        //{debug_dq0, debug_dq1, debug_dq2, debug_dq3} = func_read_debug_registers(0);
                        func_read_debug_registers(idx, subp, &fbpa_training_debug_dq0, &fbpa_training_debug_dq1, &fbpa_training_debug_dq2, &fbpa_training_debug_dq3 );

                        // 16-bits each
                        if(ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_ACLWM_AREA_3) {
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8)]      = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8) + 1]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA,fbpa_training_debug_dq0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8) + 2]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8) + 3]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA,fbpa_training_debug_dq1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8) + 4]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8) + 5]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA,fbpa_training_debug_dq2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8) + 6]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq3);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 8) + 7]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA,fbpa_training_debug_dq3);

                        } else if(ctrl_select == LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_ACLWM_AREA_4) {
                            return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 0]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq0);
                            return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 1]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA,fbpa_training_debug_dq0);
                            return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 2]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq1);
                            return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 3]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA,fbpa_training_debug_dq1);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 0]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq2);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 1]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_AREA,fbpa_training_debug_dq2);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 2]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq3);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 3]  = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_AREA,fbpa_training_debug_dq3);
                        }
                    }
                }
            }
        }
    }

    for( i =0; i < TOTAL_DQ_BITS ; i++) {
        if ( return_dq[i] > return_opt_dq[i]){
            return_opt_dq[i] = return_dq[i];
            dfe_setting_optimal_dq[i] = dfe_lwr_setting;
        }
    }
    for( i =0; i < TOTAL_DBI_BITS ; i++) {
        if ( return_dbi[i] > return_opt_dbi[i]){
            return_opt_dbi[i] = return_dbi[i];
            dfe_setting_optimal_dbi[i] = dfe_lwr_setting;
        }
        if (( return_edc[i] > return_opt_edc[i]) && edc_en){
            return_opt_edc[i] = return_edc[i];
            dfe_setting_optimal_edc[i] = dfe_lwr_setting;
        }
    }

    return;
}

void func_read_debug_max_window_ib 
(
        LwU32 return_dq[TOTAL_DQ_BITS],
        LwU32 return_dbi[TOTAL_DBI_BITS],
        LwU32 return_edc[TOTAL_DBI_BITS],
        LwU32 edc_en
) {
    LwU32 idx;
    LwU32 subp;
    LwU32 ctrl_select;
    LwU32 true_ctrl_sel;
    LwU32 fbpa_training_debug_ctrl;
    LwU32 fbpa_training_debug_dq0;
    LwU32 fbpa_training_debug_dq1;
    LwU32 fbpa_training_debug_dq2;
    LwU32 fbpa_training_debug_dq3;
    LwU32 debug_dq_lsb0;
    LwU32 debug_dq_lsb1;
    LwU32 debug_dq_lsb2;
    LwU32 debug_dq_lsb3;
    LwU32 debug_dq_msb0;
    LwU32 debug_dq_msb1;
    LwU32 debug_dq_msb2;
    LwU32 debug_dq_msb3;

    for (ctrl_select = LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_IB_PASSING0 ; ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_IB_PASSING_EDC; ctrl_select++) {

        true_ctrl_sel = ctrl_select - LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_IB_PASSING0;
        fbpa_training_debug_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0));
        fbpa_training_debug_ctrl = FLD_SET_DRF_NUM(_PFB,        _FBPA_TRAINING_DEBUG_CTRL,      _SELECT,         ctrl_select,     fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0),      fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(1),      fbpa_training_debug_ctrl);

        for (idx = 0; idx < MAX_PARTS; idx++) 
        {
            if (isPartitionEnabled(idx)) {
                for(subp = 0; subp < 2; subp++) {
                    if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        //{debug_dq0, debug_dq1, debug_dq2, debug_dq3} = func_read_debug_registers(0);
                        func_read_debug_registers(idx, subp, &fbpa_training_debug_dq0, &fbpa_training_debug_dq1, &fbpa_training_debug_dq2, &fbpa_training_debug_dq3 );

                        debug_dq_lsb0 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq0);
                        debug_dq_msb0 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq0);
                        debug_dq_lsb1 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq1);
                        debug_dq_msb1 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq1);
                        debug_dq_lsb2 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq2);
                        debug_dq_msb2 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq2);
                        debug_dq_lsb3 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq3);
                        debug_dq_msb3 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq3);

                        // Max minus Min gives window width
                        if(ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_IB_PASSING7) {
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4)]      = (debug_dq_msb0 - debug_dq_lsb0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 1]  = (debug_dq_msb1 - debug_dq_lsb1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 2]  = (debug_dq_msb2 - debug_dq_lsb2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 3]  = (debug_dq_msb3 - debug_dq_lsb3);
                        } else if(ctrl_select == LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_IB_PASSING_DBI) {
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 0]  = (debug_dq_msb0 - debug_dq_lsb0);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 1]  = (debug_dq_msb1 - debug_dq_lsb1);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 2]  = (debug_dq_msb2 - debug_dq_lsb2);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 3]  = (debug_dq_msb3 - debug_dq_lsb3);
                        } else { // LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_IB_PASSING_EDC
                            if (edc_en) {
                                return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 0]  = (debug_dq_msb0 - debug_dq_lsb0);
                                return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 1]  = (debug_dq_msb1 - debug_dq_lsb1);
                                return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 2]  = (debug_dq_msb2 - debug_dq_lsb2);
                                return_edc[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 3]  = (debug_dq_msb3 - debug_dq_lsb3);
                            }
                        }
                    }
                }
            }
        }
    }
    return;
}

void func_read_debug_max_window_ob 
(
        LwU32 return_dq[32],
        LwU32 return_dbi[4]
) 
{
    LwU32 ctrl_select;
    LwU32 true_ctrl_sel;
    LwU32 idx;
    LwU32 subp;
    LwU32 fbpa_training_debug_ctrl;
    LwU32 fbpa_training_debug_dq0;
    LwU32 fbpa_training_debug_dq1;
    LwU32 fbpa_training_debug_dq2;
    LwU32 fbpa_training_debug_dq3;
    LwU32 debug_dq_lsb0;
    LwU32 debug_dq_lsb1;
    LwU32 debug_dq_lsb2;
    LwU32 debug_dq_lsb3;
    LwU32 debug_dq_msb0;
    LwU32 debug_dq_msb1;
    LwU32 debug_dq_msb2;
    LwU32 debug_dq_msb3;

    for (ctrl_select = LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_OB_PASSING0; ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_OB_PASSING_DBI; ctrl_select++) {
        true_ctrl_sel = ctrl_select - LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_OB_PASSING0;
        fbpa_training_debug_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0));
        fbpa_training_debug_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_DEBUG_CTRL, _SELECT, ctrl_select, fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0), fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(1),      fbpa_training_debug_ctrl);

        for (idx = 0; idx < MAX_PARTS; idx++) 
        {
            if (isPartitionEnabled(idx))
            {
                for(subp = 0; subp < 2; subp++)
                {
                    if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        //{debug_dq0, debug_dq1, debug_dq2, debug_dq3} = func_read_debug_registers(0);
                        func_read_debug_registers(idx, subp, &fbpa_training_debug_dq0, &fbpa_training_debug_dq1, &fbpa_training_debug_dq2, &fbpa_training_debug_dq3 );

                        debug_dq_lsb0 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq0);
                        debug_dq_msb0 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq0);
                        debug_dq_lsb1 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq1);
                        debug_dq_msb1 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq1);
                        debug_dq_lsb2 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq2);
                        debug_dq_msb2 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq2);
                        debug_dq_lsb3 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MIN_WINDOW,fbpa_training_debug_dq3);
                        debug_dq_msb3 = REF_VAL(LW_PFB_FBPA_TRAINING_DEBUG_DQ_MAX_WINDOW,fbpa_training_debug_dq3);

                        // Max minus Min gives window width
                        if(ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_OB_PASSING7) {
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4)]      = (debug_dq_msb0 - debug_dq_lsb0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 1]  = (debug_dq_msb1 - debug_dq_lsb1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 2]  = (debug_dq_msb2 - debug_dq_lsb2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 3]  = (debug_dq_msb3 - debug_dq_lsb3);
                        } else { //if(ctrl_select == LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_IB_PASSING_DBI) {
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 0]  = (debug_dq_msb0 - debug_dq_lsb0);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 1]  = (debug_dq_msb1 - debug_dq_lsb1);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 2]  = (debug_dq_msb2 - debug_dq_lsb2);
                            return_dbi[(idx * 8) + (subp * 4) + (true_ctrl_sel * 4) + 3]  = (debug_dq_msb3 - debug_dq_lsb3);
                        }
                    }
                }
            }
        }
    }

    return;
}


void func_read_debug_max_window 
(
        LwU32 return_dq[TOTAL_DQ_BITS],
        LwU32 return_dbi[TOTAL_DBI_BITS],
        LwU32 return_edc[TOTAL_DBI_BITS],
        LwU32 edc_en,
        LwU32 ib
) {

    if(ib) {
        //func_read_debug_max_window_ib (&return_dq[TOTAL_DQ_BITS], &return_dbi[TOTAL_DBI_BITS], &return_edc[TOTAL_DBI_BITS], edc_en);
        func_read_debug_max_window_ib (return_dq, return_dbi, return_edc, edc_en);
    } else {
        //func_read_debug_max_window_ob (&return_dq[TOTAL_DQ_BITS], &return_dbi[TOTAL_DBI_BITS]);
        func_read_debug_max_window_ob (return_dq, return_dbi);
    }
    return;
}

void func_save_2d_vref_values 
(
        LwU32 opt_vref_dq[TOTAL_DQ_BITS],
        LwU32 opt_vref_dbi[TOTAL_DBI_BITS],
        LwU32 opt_vref_edc[TOTAL_DBI_BITS],
        LwU32 edc_en
)
{
    LwU8 idx;
    LwU8 subp;
    LwU8 byte;
    LwU32 vref_code1;
    LwU32 vref_code2;
    LwU32 vref_code3;
    LwU32 offset;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                for( byte = 0 ; byte < 4; byte++)
                {
                    if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        offset = (subp * 48) + (byte * 12);
                        vref_code1 = REG_RD32(BAR0, unicast_rd_offset(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1, idx, offset ));
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 0] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1_PAD0 , vref_code1);
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 1] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1_PAD1 , vref_code1);
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 2] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1_PAD2 , vref_code1);
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 3] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1_PAD3 , vref_code1);

                        vref_code2 = REG_RD32(BAR0, unicast_rd_offset(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2, idx, offset ));
                        opt_vref_dbi[(idx * 8) + (subp * 4) + (byte * 4) + 0]  = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2_PAD4 , vref_code2);
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 1] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2_PAD5 , vref_code2);
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 2] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2_PAD6 , vref_code2);
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 3] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2_PAD7 , vref_code2);

                        vref_code3 = REG_RD32(BAR0, unicast_rd_offset(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2, idx, offset ));
                        opt_vref_dq[(idx * 64) + (subp * 32) + (byte * 4) + 0] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD8 , vref_code3);
                        if(edc_en) {
                            opt_vref_edc[(idx * 8) + (subp * 4)  + (byte * 4) + 1] = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD9 , vref_code3);
                        }
                    }
                }
            }
        }
    }
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


void func_program_hybrid_opt_in_fbio 
(
        LwU32 opt_hybrid_dq[TOTAL_DQ_BITS],
        LwU32 opt_hybrid_dbi[TOTAL_DBI_BITS],
        LwU32 opt_hybrid_edc[TOTAL_DBI_BITS],
        LwU32 edc_en
)
{
    LwU32 vref_dfe_ctrl1 = 0, vref_dfe_ctrl2 = 0, vref_dfe_ctrl3 = 0;
    LwU8 idx;
    LwU8 subp;
    LwU8 byte;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                for( byte = 0 ; byte < 4; byte++)
                {
                    if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        vref_dfe_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD0,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 0], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl1);
                        vref_dfe_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD1,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 1], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl1);
                        vref_dfe_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD2,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 2], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl1);
                        vref_dfe_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD3,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 3], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl1);

                        vref_dfe_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD0,
                                addOffsetAndClampTo64(opt_hybrid_dbi[(idx * 8) + (subp * 4) + byte ], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl2);
                        vref_dfe_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD1,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 4], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl2);
                        vref_dfe_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD2,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 5], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl2);
                        vref_dfe_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD3,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 6], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl2);

                        vref_dfe_ctrl3 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD0,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 2 * 4) + 7], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                vref_dfe_ctrl3);
                        if(edc_en) {
                            vref_dfe_ctrl3 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD1,
                                    addOffsetAndClampTo64(opt_hybrid_edc[(idx * 8) + (subp * 4) + (byte * 2 * 4) + 1], gTT.bootTrainingTable.BKV_OFFSET.IB_DFE),
                                    vref_dfe_ctrl3);
                        }

                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1), idx, subp, byte),vref_dfe_ctrl1);
                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL2), idx, subp, byte),vref_dfe_ctrl2);
                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3), idx, subp, byte),vref_dfe_ctrl3);
                    }
                }
            }
        }
    }
}

LwU32 func_check_training_passed
(
        void
) 
{
    LwU32 fbpa_trng_status;
    LwU32 subp0_trng_status;
    LwU32 subp1_trng_status;
    LwU32 training_passed;

    fbpa_trng_status = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_STATUS);
    subp0_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS, fbpa_trng_status);
    subp1_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS, fbpa_trng_status);

    training_passed = ((subp0_trng_status == 0) && (subp1_trng_status == 0));

    return training_passed;
}


void func_set_tag_register
(
        LwU32 first,
        LwU32 value,
        LwU32 enable

)
{
    LwU32 tag_enable = enable;
    LwU32 first_tag = first;
    LwU32 store_optimal = LW_PFB_FBPA_TRAINING_TAG_STORE_OPTIMAL_GREATER_PIN;
    LwU32 tag = value;
    LwU32 fbpa_training_tag;
    fbpa_training_tag = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_TAG);
    fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _TAG,            tag,                fbpa_training_tag);
    fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _ENABLE,         tag_enable,         fbpa_training_tag);
    fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _FIRST_TAG,      first_tag,          fbpa_training_tag);
    fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _STORE_OPTIMAL,  store_optimal,      fbpa_training_tag);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TAG,     fbpa_training_tag);


}

void func_read_tag(
        LwU32 return_dq[TOTAL_DQ_BITS],
        LwU32 return_dbi[TOTAL_DBI_BITS],
        LwU32 return_edc[TOTAL_DBI_BITS],
        LwU32 edc_en
) 
{
    LwU32 idx;
    LwU32 subp;
    LwU32 ctrl_select;
    LwU32 true_ctrl_sel;
    LwU32 fbpa_training_debug_ctrl;
    LwU32 fbpa_training_debug_dq0;
    LwU32 fbpa_training_debug_dq1;
    LwU32 fbpa_training_debug_dq2;
    LwU32 fbpa_training_debug_dq3;

    for (ctrl_select = LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_TAG0; ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_TAG2; ctrl_select++) {
        true_ctrl_sel = ctrl_select - LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_TAG0;
        fbpa_training_debug_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0));
        fbpa_training_debug_ctrl = FLD_SET_DRF_NUM(_PFB,        _FBPA_TRAINING_DEBUG_CTRL,      _SELECT,         ctrl_select,     fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0),      fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(1),      fbpa_training_debug_ctrl);

        for (idx = 0; idx < MAX_PARTS; idx++) 
        {
            if (isPartitionEnabled(idx))
            {
                for(subp = 0; subp < 2; subp++)
                {
                    if((subp == 1) || ((subp == 0) && ( isLowerHalfPartitionEnabled(idx))))
                    {
                        func_read_debug_registers(idx, subp, &fbpa_training_debug_dq0, &fbpa_training_debug_dq1, &fbpa_training_debug_dq2, &fbpa_training_debug_dq3 );

                        if(ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_DQ_TAG1) {
                            // 8-bits each
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16)]      = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0,fbpa_training_debug_dq0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 1]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1,fbpa_training_debug_dq0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 2]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2,fbpa_training_debug_dq0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 3]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3,fbpa_training_debug_dq0);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 4]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0,fbpa_training_debug_dq1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 5]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1,fbpa_training_debug_dq1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 6]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2,fbpa_training_debug_dq1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 7]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3,fbpa_training_debug_dq1);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 8]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0,fbpa_training_debug_dq2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 9]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1,fbpa_training_debug_dq2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 10] = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2,fbpa_training_debug_dq2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 11] = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3,fbpa_training_debug_dq2);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 12] = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0,fbpa_training_debug_dq3);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 13] = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1,fbpa_training_debug_dq3);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 14] = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2,fbpa_training_debug_dq3);
                            return_dq[(idx * 64) + (subp * 32) + (true_ctrl_sel * 16) + 15] = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3,fbpa_training_debug_dq3);
                        } else {
                            return_dbi[(idx * 8) + (subp * 4) + 0]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0,fbpa_training_debug_dq0);
                            return_dbi[(idx * 8) + (subp * 4) + 1]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1,fbpa_training_debug_dq0);
                            return_dbi[(idx * 8) + (subp * 4) + 2]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2,fbpa_training_debug_dq0);
                            return_dbi[(idx * 8) + (subp * 4) + 3]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3,fbpa_training_debug_dq0);
                            return_edc[(idx * 8) + (subp * 4) + 0]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ0,fbpa_training_debug_dq1);
                            return_edc[(idx * 8) + (subp * 4) + 1]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ1,fbpa_training_debug_dq1);
                            return_edc[(idx * 8) + (subp * 4) + 2]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ2,fbpa_training_debug_dq1);
                            return_edc[(idx * 8) + (subp * 4) + 3]  = REF_VAL(LW_PFB_FBPA_TRAINING_8bit_DEBUG_DQ3,fbpa_training_debug_dq1);
                        }

                    }
                }
            }
        }
    }

    return;
}


void func_program_read_dfe
(
        LwU32 read_dfe
)
{
    LwU32 vref_vref_ctrl1 = 0;
    LwU16 idx, subp, byte;
    idx = 0;
    //for (idx = 0; idx < MAX_PARTS; idx++)
    //{
    if (isPartitionEnabled(idx))
    {
        for(subp = 0; subp < 2; subp++)
        {
            for( byte = 0 ; byte < 4; byte++)
            {
                if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD0, read_dfe, vref_vref_ctrl1);
                    vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD1, read_dfe, vref_vref_ctrl1);
                    vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD2, read_dfe, vref_vref_ctrl1);
                    vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1, _PAD3, read_dfe, vref_vref_ctrl1);

                    REG_WR32(LOG, multi_broadcast_wr_vref_dfe((LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL1), idx, subp, byte),vref_vref_ctrl1);
                    REG_WR32(LOG, multi_broadcast_wr_vref_dfe((LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL2), idx, subp, byte),vref_vref_ctrl1);
                    REG_WR32(LOG, multi_broadcast_wr_vref_dfe((LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_DFE_CTRL3), idx, subp, byte),vref_vref_ctrl1);
                }
            }
        }
    }
    //}

}

LwU32 func_run_rd_tr_hybrid_non_vref
(
        LwU32 area
) 
{
    // Piggybacking on VREF fields that match DFE requirements
    LwU32 fbpaTrainingRwDfeCtrl = gTT.bootTrainingTable.READ_TRAINING_DFE;
    LwU8 ib_dfe_min             = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  fbpaTrainingRwDfeCtrl);
    LwU8 ib_dfe_max             = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  fbpaTrainingRwDfeCtrl);
    LwU8 ib_dfe_step            = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_STEP, fbpaTrainingRwDfeCtrl);

    //LwU32 no_of_dfe_settings;
    //LwU32 hybrid_result_dq[TOTAL_DQ_BITS];
    //LwU32 hybrid_result_dbi[TOTAL_DBI_BITS];
    //LwU32 hybrid_result_edc[TOTAL_DBI_BITS];
    //LwU32 temp_result_dq[TOTAL_DQ_BITS];
    //LwU32 temp_result_dbi[TOTAL_DBI_BITS];
    //LwU32 temp_result_edc[TOTAL_DBI_BITS];

    LwU16 i;
    LwU16 idx, subp, bit;

    //no_of_dfe_settings = ((ib_dfe_max - ib_dfe_min) / ib_dfe_step) + 1;

    // Enable GPU DFE
    LwU32 e_rx_dfe = 1;
    LwU32 fbpa_fbio_byte_pad_ctrl2;
    fbpa_fbio_byte_pad_ctrl2= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2);
    fbpa_fbio_byte_pad_ctrl2 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIO_BYTE_PAD_CTRL2,      _E_RX_DFE,       e_rx_dfe,   fbpa_fbio_byte_pad_ctrl2);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2,      fbpa_fbio_byte_pad_ctrl2);

    // Initialize each optimum tag value to fail value of 0xff
    // Initialize max window/aclwm_area to 0
    // Signal to indicate to TB falcon_read_area_tr_trigger = 1
    //REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x5452478);

    OS_PTIMER_SPIN_WAIT_US(10);

    for (i =0; i < TOTAL_DQ_BITS; i++) {  
        gTD.rdHybridDfeDq[i] = 0xff;
        gTD.rdHybridDfeDqDbiValid = 0;
        //hybrid_result_dq[i] = 0x0;
    }
    for (i=0; i < TOTAL_DBI_BITS; i++) {
        gTD.rdHybridDfeDbi[i] = 0xff;
        gTD.rdHybridDfeEdc[i] = 0xff;
        gTD.rdHybridDfeEdcValid = 0;
        //hybrid_result_dbi[i] = 0x0;
        //hybrid_result_edc[i] = 0x0;
    }
    LwS32 tag;
    LwU8 training_passed;

    //for (set=0; set<no_of_dfe_settings; set=set+1) {
    for(tag = ib_dfe_max; tag >= ib_dfe_min; tag = tag - ib_dfe_step) {
        //LwU16 ib = 1;


        LwU32 enable = LW_PFB_FBPA_TRAINING_TAG_ENABLE_ENABLED;
        LwU32 first_tag = (tag == ib_dfe_max);
        LwU32 store_optimal = LW_PFB_FBPA_TRAINING_TAG_STORE_OPTIMAL_GREATER_PIN;
        //LwU32 tag = ib_dfe_min + (set * ib_dfe_step);
        LwU32 fbpa_training_tag;
        fbpa_training_tag = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_TAG);
        fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _TAG,            tag,                fbpa_training_tag);
        fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _ENABLE,         enable,             fbpa_training_tag);
        fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _FIRST_TAG,      first_tag,          fbpa_training_tag);
        fbpa_training_tag = FLD_SET_DRF_NUM(_PFB,       _FBPA_TRAINING_TAG,     _STORE_OPTIMAL,  store_optimal,      fbpa_training_tag);         
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_TAG,     fbpa_training_tag);

        //LwU32 pad = ib_dfe_min + (set * ib_dfe_step);

        // Broadcast writes
        func_program_read_dfe(tag);

        // Wait for VREF to settle
        OS_PTIMER_SPIN_WAIT_US(10);

        // Trigger training

        func_run_training(0, 0, 1, 0, 0, 0);         // addr=0, wck=0, rd=1, wr=0, periodic=0, char=0
        func_wait_for_training_done();


        // Exit DFE outer loop if training fails
        func_check_training_passed();

    }
    func_set_tag_register(0, 0, 0);

    if (!falcon_to_determine_best_window) {
        //func_read_tag(temp_result_dq, temp_result_dbi, temp_result_edc, !bFlagG6EdcTrackingEn);
        func_read_tag(gTD.rdHybridDfeDq, gTD.rdHybridDfeDbi, gTD.rdHybridDfeEdc, !bFlagG6EdcTrackingEn);
        gTD.rdHybridDfeDqDbiValid = 1;
        gTD.rdHybridDfeEdcValid = !bFlagG6EdcTrackingEn;
    }

    // check if hybrid training passed
    // NOTE: We are looking for training_passed = 1 for a clean pass
    training_passed = 1; 
    for (idx = 0; idx < MAX_PARTS; idx++) {
        if (isPartitionEnabled(idx)) {
            for(subp = 0; subp < 2; subp++) {
                if ((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx)))) {
                    for( bit = 0 ; bit < DQ_BITS_IN_SUBP; bit++) {
                        if(gTD.rdHybridDfeDq[(idx * 64) + (subp * 32) + bit] == 0xff) {
                            training_passed = 0;
                            break;
                        }
                    }
                }
            }
        }
    }

    for (idx = 0; idx < MAX_PARTS; idx++) {
        if (isPartitionEnabled(idx)) {
            for(subp = 0; subp < 2; subp++) {
                if ((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx)))) {
                    for( bit = 0 ; bit < DBI_BITS_IN_SUBP; bit++) {
                        if(gTD.rdHybridDfeDbi[(idx * 8) + (subp * 4) + bit] == 0xff) {
                            training_passed = 0;
                            break;
                        }
                        if(!bFlagG6EdcTrackingEn) {
                            if(gTD.rdHybridDfeEdc[(idx * 8) + (subp * 4) + bit] == 0xff) {
                                training_passed = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }


    // program FBIO with results of hybrid training.
    if(!training_passed) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_TRAINING_ERROR);
    }

    // apply offset and write dfe cntrl to design
    func_program_hybrid_opt_in_fbio(gTD.rdHybridDfeDq, gTD.rdHybridDfeDbi, gTD.rdHybridDfeEdc, 0);

    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xaabbaa8);
    // If area based analysis is enabled, do another run of hw vref training
    if (area) {
        //REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xaabbaab);
        OS_PTIMER_SPIN_WAIT_US(10);
        // Signal to indicate to TB falcon_read_area_tr_trigger = 1; and we are in second loop
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xaabbaa9);
        //func_run_training(addr=0, wck=0, rd=1, wr=0, periodic=0, char=0);
        func_run_training(0, 0, 1, 0, 0, 0);
        func_wait_for_training_done();
    }
    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xaabbaaa);
    training_passed = func_check_training_passed();
    if(!training_passed) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_TRAINING_ERROR);
    }

    // 2D VREF values for FbFlcn to save and restore in second ucode
    //func_save_2d_vref_values(&opt_vref_dq[0], &opt_vref_dbi[0], &opt_vref_edc[0], !bFlagG6EdcTrackingEn);

    // Add bkv offset to vref codes
    // read vref codes from design
    func_save_2d_vref_values(gTD.rdVrefDq, gTD.rdVrefDbi, gTD.rdVrefEdc, !bFlagG6EdcTrackingEn);
    // wrote back vref codes with offset
    func_program_optimal_vref_fbio(gTD.rdVrefDq, gTD.rdVrefDbi, gTD.rdVrefEdc, !bFlagG6EdcTrackingEn);

    gTD.rdVrefDqDbiValid = 1;
    gTD.rdVrefEdcValid = !bFlagG6EdcTrackingEn;

    // Signal to indicate to TB falcon_read_area_tr_trigger = 1
    //REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x5452480);

    //Signal to indicate to TB for checking of final_value_check
    //REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x5452481);

    return training_passed;


}

void func_setup_wr_tr
(
        LwU32 hybrid,
        LwU32 vref_hw,
        LwU32 prbs,
        LwU32 area,
        LwU32 redo_prbs_setting,
        LwU32 redo_sweep_setting
)
{
    if (prbs) {
        if ( redo_prbs_setting ) {
            func_setup_rd_prbs_regs();
            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xabbcbbab);

        }
        func_set_prbs_dual_mode(1,1);
    } else {
        func_set_prbs_dual_mode(0,1);
        func_disable_char_bank_ctrl();
    }
    if(vref_hw) {
        LwU32 fbpa_training_rw_wr_vref_ctrl;
        LwU32 area_based;
        saved_reg->pfb_fbpa_training_rw_wr_vref_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL);
        fbpa_training_rw_wr_vref_ctrl = gTT.bootTrainingTable.WRITE_RW_WR_VREF_CTRL;

        if(area && hybrid) {
            area_based = 1;
            fbpa_training_rw_wr_vref_ctrl = FLD_SET_DRF_NUM(_PFB,   _FBPA_TRAINING_RW_WR_VREF_CTRL, _AREA_BASED,     area_based,         fbpa_training_rw_wr_vref_ctrl);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL, fbpa_training_rw_wr_vref_ctrl);
        }
        else {
            // Just hw loop VREF training
            // non area based
            area_based = 0;
            fbpa_training_rw_wr_vref_ctrl = FLD_SET_DRF_NUM(_PFB,   _FBPA_TRAINING_RW_WR_VREF_CTRL, _AREA_BASED,     area_based,         fbpa_training_rw_wr_vref_ctrl);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_WR_VREF_CTRL, fbpa_training_rw_wr_vref_ctrl);
        }
    }
    // Set up PI sweeps - applicable for all training
    // These are shared with read training. So if these
    // are the same as read training no need to change
    // else, program values for write training

    saved_reg->pfb_fbpa_training_rw_brlshft_quick_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_BRLSHFT_QUICK_CTRL);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_BRLSHFT_QUICK_CTRL,gTT.bootTrainingTable.WRITE_RW_BRLSHFT_QUICK_CTRL);

    if(redo_sweep_setting) {
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_CORS_INTRPLTR_CTRL,gTT.bootTrainingTable.WRITE_RW_CORS_INTRPLTR_CTRL);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_FINE_INTRPLTR_CTRL,gTT.bootTrainingTable.WRITE_RW_FINE_INTRPLTR_CTRL);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_ERR_LIMIT,gTT.bootTrainingTable.WRITE_RW_ERR_LIMIT);
    }

    return;
}

void func_program_vref_in_fbio
(
        LwU32 tag_index
)
{
    LwU32 vref_vref_ctrl1 = 0;
    LwU16 idx, subp, byte;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                for( byte = 0 ; byte < 4; byte++)
                {
                    if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD0, tag_index, vref_vref_ctrl1);
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD1, tag_index, vref_vref_ctrl1);
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD2, tag_index, vref_vref_ctrl1);
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD3, tag_index, vref_vref_ctrl1);

                        REG_WR32(LOG, multi_broadcast_wr_vref_dfe((LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1), idx, subp, byte),vref_vref_ctrl1);
                        REG_WR32(LOG, multi_broadcast_wr_vref_dfe((LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2), idx, subp, byte),vref_vref_ctrl1);
                        REG_WR32(LOG, multi_broadcast_wr_vref_dfe((LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3), idx, subp, byte),vref_vref_ctrl1);
                    }
                }
            }
        }
    }
}


void func_program_optimal_vref_fbio
(
        LwU32 opt_hybrid_dq[TOTAL_DQ_BITS],
        LwU32 opt_hybrid_dbi[TOTAL_DBI_BITS],
        LwU32 opt_hybrid_edc[TOTAL_DBI_BITS],
        LwU32 edc_en
)
{
    LwU32 vref_vref_ctrl1 = 0, vref_vref_ctrl2 = 0, vref_vref_ctrl3 = 0;
    LwU16 idx, subp, byte;
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                for( byte = 0 ; byte < 4; byte++)
                {
                    if ((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        // VREF_CTRL1
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD0,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 0], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl1);
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD1,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 1], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl1);
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD2,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 2], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl1);
                        vref_vref_ctrl1 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD3,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 3], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl1);

                        // VREF_CTRL2
                        vref_vref_ctrl2 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD0,
                                addOffsetAndClampTo64(opt_hybrid_dbi[(idx * 8) + (subp * 4) + (byte * 4) + 0], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl2);
                        vref_vref_ctrl2 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD1,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 4], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl2);
                        vref_vref_ctrl2 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD2,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 5], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl2);
                        vref_vref_ctrl2 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD3,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 6], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl2);

                        // VREF_CTRL3
                        vref_vref_ctrl3 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD0,
                                addOffsetAndClampTo64(opt_hybrid_dq[(idx * 64) + (subp * 32) + (byte * 4) + 7], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                vref_vref_ctrl3);
                        if(edc_en) {
                            vref_vref_ctrl3 =  FLD_SET_DRF_NUM(_PFB,     _FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1,   _PAD1,
                                    addOffsetAndClampTo64(opt_hybrid_edc[(idx * 8) + (subp * 4) + (byte * 4) + 1], gTT.bootTrainingTable.BKV_OFFSET.IB_VREF),
                                    vref_vref_ctrl3);
                        }

                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE1), idx, subp, byte),vref_vref_ctrl1);
                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE2), idx, subp, byte),vref_vref_ctrl2);
                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3), idx, subp, byte),vref_vref_ctrl3);
                    }
                }
            }
        }
    }
}

LwU32 func_run_rd_tr_pi_only
(
        void
)
{
    LwU32 training_status;
    // Just trigger training
    //func_run_training(addr=0, wck =0, rd=1 , wr=0, periodic =0, char=0);
    func_run_training(0, 0, 1, 0, 0, 0);

    // Wait for training to end
    func_wait_for_training_done();

    // Check training status
    training_status = func_check_training_passed();

    if(!training_status) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_RD_TRAINING_ERROR);
    }

    return training_status;
}

void func_set_wr_dbi
(
        LwU32 gpu_dbi,
        LwU32 dram_dbi
) 
{
    LwU32 fbpa_fbio_config_dbi;
    fbpa_fbio_config_dbi= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CONFIG_DBI);
    fbpa_fbio_config_dbi = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_CONFIG_DBI,  _OUT_ON,         gpu_dbi,     fbpa_fbio_config_dbi);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CONFIG_DBI,  fbpa_fbio_config_dbi);

    LwU32 fbpa_generic_mrs1;
    fbpa_generic_mrs1= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS1);
    fbpa_generic_mrs1 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS1,     _ADR_GDDR5_WDBI,         ~dram_dbi,        fbpa_generic_mrs1);
    fbpa_generic_mrs1 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS1,     _ADR_GDDR5_RDBI,         ~dram_dbi,        fbpa_generic_mrs1);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1,     fbpa_generic_mrs1);

} // end: func_set_wr_dbi

void func_set_hybrid_parameter_in_dram
(
        LwU32 dbi,
        LwU32 tag_index
)
{
    LwU32 subp_mask;
    LwU32 pin_sub_address;
    LwU32 fbpa_generic_mrs9;

    if(dbi)
    {
        subp_mask = 0;                                 // LW_PFB_FBPA_GENERIC_MRS9_SUBP_MASK for both SUBP0 and SUBP1
        pin_sub_address = 8;
        fbpa_generic_mrs9= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _DFE,                  tag_index,            fbpa_generic_mrs9);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9,    fbpa_generic_mrs9);

        pin_sub_address = 24;
        //fbpa_generic_mrs9= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
        //fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _DFE,                  tag_index,            fbpa_generic_mrs9);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9,    fbpa_generic_mrs9);
    } else {
        // GDDR6 encoding for Byte 0
        subp_mask = 0;                                 // LW_PFB_FBPA_GENERIC_MRS9_SUBP_MASK
        pin_sub_address = 15;
        fbpa_generic_mrs9= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _DFE,                  tag_index,            fbpa_generic_mrs9);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9,    fbpa_generic_mrs9);

        // GDDR6 encoding for Byte 1

        //subp_mask = 0;                                 // LW_PFB_FBPA_GENERIC_MRS9_SUBP_MASK
        pin_sub_address = 31;
        //fbpa_generic_mrs9= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
        //fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs9);
        fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _DFE,                  tag_index,            fbpa_generic_mrs9);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9,    fbpa_generic_mrs9);
    }
}

void func_program_vref_in_dram
(
        //LwU32 hybrid_param,
        LwU32 opt_hybrid_dq[0],
        LwU32 dq,
        LwU32 dbi,
        LwS8 offset
)
{
    // First program MRS15 with LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_NONE so MRS15 commands reach both DRAMs in the case of x16
    LwU32 double_byte_index;
    LwU32 dq_index;
    LwU32 dbi_index;
    LwU32 pin_sub_address;
    LwS32 dfe;
    LwU32 fbpa_generic_mrs6;
    LwU32 fbpa_generic_mrs15;
    LwU32 subp_mask;
    LwU32 ch_sel;
    LwU32 pseudo_ch;
    LwU16 idx, subp;

    fbpa_generic_mrs6= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS6);
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if(isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    for(double_byte_index=0;double_byte_index<2;double_byte_index++) {
                        subp_mask = (subp == 1) ? LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_SUBP0 : LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_SUBP1;
                        ch_sel    = double_byte_index ? 3 : 0;
                        pseudo_ch = double_byte_index ? 0 : 3;
                        fbpa_generic_mrs15= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);
                        fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _SUBP_MASK,      subp_mask,  fbpa_generic_mrs15);
                        fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _CH_SEL,         ch_sel,     fbpa_generic_mrs15);
                        fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _PSEUDO_CH,      pseudo_ch,  fbpa_generic_mrs15);
                        REG_WR32(LOG, unicast((LW_PFB_FBPA_GENERIC_MRS15), idx),fbpa_generic_mrs15);

                        if(dq)
                        {
                            for(dq_index = 0; dq_index < 16; dq_index++)
                            {
                                subp_mask = (subp == 1) ? LW_PFB_FBPA_GENERIC_MRS6_SUBP_MASK_SUBP0 : LW_PFB_FBPA_GENERIC_MRS6_SUBP_MASK_SUBP1;
                                pin_sub_address = (dq_index < 8) ? dq_index : 16 + (dq_index %8); // To match the GDDR6 DRAM MRS encoding
                                dfe  = (LwS32) opt_hybrid_dq[ dq_index + (16 * double_byte_index) + (subp * 32)];
                                dfe += offset;
                                if (dfe < 0)
                                {
                                    dfe = 0;
                                }
                                if (dfe > 127)
                                {
                                    dfe = 127;
                                }
                                fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS6,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs6);
                                fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS6,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs6);
                                fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS6,    _VREF,                 dfe,              fbpa_generic_mrs6);
                                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS6,    fbpa_generic_mrs6);
                                REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_GENERIC_MRS6), idx, 0, 0),fbpa_generic_mrs6);
                            }
                        }
                        if(dbi) {
                            for(dbi_index = 0; dbi_index < 2; dbi_index++) {
                                subp_mask = (subp == 1) ? LW_PFB_FBPA_GENERIC_MRS6_SUBP_MASK_SUBP0 : LW_PFB_FBPA_GENERIC_MRS6_SUBP_MASK_SUBP1;
                                pin_sub_address = (dbi_index == 0) ? 8 : (8 + 16) ; // To match the GDDR6 DRAM MRS encoding
                                dfe  = (LwS32) opt_hybrid_dq[ dbi_index + (2 * double_byte_index) + (subp * 4)];
                                dfe += offset;
                                if (dfe < 0)
                                {
                                    dfe = 0;
                                }
                                if (dfe > 127)
                                {
                                    dfe = 127;
                                }
                                fbpa_generic_mrs6= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS6);
                                fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS6,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs6);
                                fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS6,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs6);
                                fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS6,    _VREF,                 dfe,              fbpa_generic_mrs6);
                                REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_GENERIC_MRS6), idx, 0, 0),fbpa_generic_mrs6);
                            }
                        }
                    }
                }
            }
        }
    }
    // Restore MRS15 to write to both channels at this point
    subp_mask = LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_NONE;
    ch_sel    = 0;
    pseudo_ch = 0;
    fbpa_generic_mrs15= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);
    fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _SUBP_MASK,      subp_mask,  fbpa_generic_mrs15);
    fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _CH_SEL,         ch_sel,     fbpa_generic_mrs15);
    fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _PSEUDO_CH,      pseudo_ch,  fbpa_generic_mrs15);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15,    fbpa_generic_mrs15);

    return;
}

void func_program_hybrid_opt_in_dram
(
        //LwU32 hybrid_param,
        LwU32 opt_hybrid_dq[TOTAL_DQ_BITS],
        LwU32 dq,
        LwU32 dbi,
        LwS8 offset
)
{
    // First program MRS15 with LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_NONE so MRS15 commands reach both DRAMs in the case of x16
    LwU32 double_byte_index;
    LwU32 dq_index;
    LwU32 dbi_index;
    LwU32 pin_sub_address;
    LwS32 dfe;
    LwU32 fbpa_generic_mrs9;
    LwU32 fbpa_generic_mrs15;
    LwU32 subp_mask;
    LwU32 ch_sel;
    LwU32 pseudo_ch;
    LwU16 idx, subp;

    fbpa_generic_mrs9= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
    for (idx = 0; idx < MAX_PARTS; idx++)
    {
        if(isPartitionEnabled(idx))
        {
            for(subp = 0; subp < 2; subp++)
            {
                if((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                {
                    for(double_byte_index=0;double_byte_index<2;double_byte_index++) {
                        subp_mask = (subp == 1) ? LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_SUBP0 : LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_SUBP1;
                        ch_sel    = double_byte_index ? 3 : 0;
                        pseudo_ch = double_byte_index ? 0 : 3;
                        fbpa_generic_mrs15= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);
                        fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _SUBP_MASK,      subp_mask,  fbpa_generic_mrs15);
                        fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _CH_SEL,         ch_sel,     fbpa_generic_mrs15);
                        fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _PSEUDO_CH,      pseudo_ch,  fbpa_generic_mrs15);
                        REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_GENERIC_MRS15), idx, 0, 0),fbpa_generic_mrs15);

                        if(dq)
                        {
                            for(dq_index = 0; dq_index < 16; dq_index++)
                            {
                                subp_mask = (subp == 1) ? LW_PFB_FBPA_GENERIC_MRS9_SUBP_MASK_SUBP0 : LW_PFB_FBPA_GENERIC_MRS9_SUBP_MASK_SUBP1;
                                pin_sub_address = (dq_index < 8) ? dq_index : 16 + (dq_index %8); // To match the GDDR6 DRAM MRS encoding
                                dfe  = (LwS32) opt_hybrid_dq[ dq_index + (16 * double_byte_index) + (subp * 32)];
                                dfe += offset;
                                if (dfe < 0)
                                {
                                    dfe = 0;
                                }
                                if (dfe > 127)
                                {
                                    dfe = 127;
                                }
                                fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs9);
                                fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs9);
                                fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _DFE,                  dfe,              fbpa_generic_mrs9);
                                //REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9,    fbpa_generic_mrs9);
                                REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_GENERIC_MRS9), idx, 0, 0),fbpa_generic_mrs9);
                            }
                        }
                        if(dbi) {
                            for(dbi_index = 0; dbi_index < 2; dbi_index++) {
                                subp_mask = (subp == 1) ? LW_PFB_FBPA_GENERIC_MRS9_SUBP_MASK_SUBP0 : LW_PFB_FBPA_GENERIC_MRS9_SUBP_MASK_SUBP1;
                                pin_sub_address = (dbi_index == 0) ? 8 : (8 + 16) ; // To match the GDDR6 DRAM MRS encoding
                                dfe  = (LwS32)opt_hybrid_dq[ dbi_index + (2 * double_byte_index) + (subp * 4)];
                                dfe += offset;
                                if (dfe < 0)
                                {
                                    dfe = 0;
                                }
                                if (dfe > 127)
                                {
                                    dfe = 127;
                                }
                                fbpa_generic_mrs9= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
                                fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _SUBP_MASK,            subp_mask,        fbpa_generic_mrs9);
                                fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _PIN_SUB_ADDRESS,      pin_sub_address,  fbpa_generic_mrs9);
                                fbpa_generic_mrs9 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS9,    _DFE,                  dfe,              fbpa_generic_mrs9);
                                REG_WR32(LOG, unicast_wr_vref_dfe((LW_PFB_FBPA_GENERIC_MRS9), idx, 0, 0),fbpa_generic_mrs9);
                            }
                        }
                    }
                }
            }
        }
    }
    // Restore MRS15 to write to both channels at this point
    subp_mask = LW_PFB_FBPA_GENERIC_MRS15_SUBP_MASK_NONE;
    ch_sel    = 0;
    pseudo_ch = 0;
    fbpa_generic_mrs15= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS15);
    fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _SUBP_MASK,      subp_mask,  fbpa_generic_mrs15);
    fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _CH_SEL,         ch_sel,     fbpa_generic_mrs15);
    fbpa_generic_mrs15 = FLD_SET_DRF_NUM(_PFB,      _FBPA_GENERIC_MRS15,    _PSEUDO_CH,      pseudo_ch,  fbpa_generic_mrs15);
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS15,    fbpa_generic_mrs15);

    return;
}


void func_save_wr_vref_values
(
        LwU32 wr_vref_data[TOTAL_DQ_BITS],
        LwU32 wr_vref_dbi[TOTAL_DBI_BITS]
)
{   

    // This function is to track the VREF values that the training engine selected during 2D training.
    // The purpose of this function is for FbFlcn to save these arrays. These arrays have information on
    //   vref values (per bit) that the training engine programmed in the DRAM via MRS (non shadow)

    LwU32 ctrl_select;
    LwU32 true_ctrl_sel;
    LwU32 fbpa_training_debug_ctrl;
    LwU32 fbpa_training_debug_dq0;
    LwU32 fbpa_training_debug_dq1;
    LwU32 fbpa_training_debug_dq2;
    LwU32 fbpa_training_debug_dq3;
    LwU32 idx;
    LwU32 subp;

    for (ctrl_select = LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_CHAR_ERRCNT0 ; ctrl_select <= LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_CHAR_ERRCNT11; ctrl_select+=3) {
        true_ctrl_sel = ctrl_select - LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_CHAR_ERRCNT0;
        fbpa_training_debug_ctrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0));
        fbpa_training_debug_ctrl = FLD_SET_DRF_NUM(_PFB,        _FBPA_TRAINING_DEBUG_CTRL,      _SELECT,         ctrl_select,     fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0),      fbpa_training_debug_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(1),      fbpa_training_debug_ctrl);

        for (idx = 0; idx < MAX_PARTS; idx++) 
        {
            if (isPartitionEnabled(idx))
            {
                for(subp = 0; subp < 2; subp++) {
                    if ((subp == 1) || ( (subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        //{debug_dq0, debug_dq1, debug_dq2, debug_dq3} = func_read_debug_registers(0);
                        func_read_debug_registers(idx, subp, &fbpa_training_debug_dq0, &fbpa_training_debug_dq1, &fbpa_training_debug_dq2, &fbpa_training_debug_dq3 );

                        wr_vref_data[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 0] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq0);
                        wr_vref_data[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 1] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq1);
                        wr_vref_data[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 2] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq2);
                        wr_vref_data[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 3] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq3);

                        true_ctrl_sel = ctrl_select - LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_CHAR_ERRCNT0 + 1;
                        fbpa_training_debug_ctrl = FLD_SET_DRF_NUM(_PFB,        _FBPA_TRAINING_DEBUG_CTRL,      _SELECT,         ctrl_select,     fbpa_training_debug_ctrl);
                        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0),      fbpa_training_debug_ctrl);
                        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(1),      fbpa_training_debug_ctrl);

                        wr_vref_data[(idx * 8 ) + (subp * 4 ) + (true_ctrl_sel * 4) + 0] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq0);
                        wr_vref_dbi[ (idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 4] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq1);
                        wr_vref_data[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 5] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq2);
                        wr_vref_data[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 6] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq3);

                        true_ctrl_sel = ctrl_select - LW_PFB_FBPA_TRAINING_DEBUG_CTRL_SELECT_CHAR_ERRCNT0 + 2;
                        fbpa_training_debug_ctrl = FLD_SET_DRF_NUM(_PFB,        _FBPA_TRAINING_DEBUG_CTRL,      _SELECT,         ctrl_select,     fbpa_training_debug_ctrl);
                        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(0),      fbpa_training_debug_ctrl);
                        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_DEBUG_CTRL(1),      fbpa_training_debug_ctrl);

                        wr_vref_data[(idx * 64) + (subp * 32) + (true_ctrl_sel * 4) + 7] = REF_VAL(LW_PFB_FBPA_TRAINING_8BIT_WR_VREF, fbpa_training_debug_dq0);
                    }
                }
            }
        }
    }
}


LwU32 func_run_wr_hybrid_tr_1pass
(
        LwU32 vref,
        LwU32 dq,
        LwU32 dbi
)
{
    LwU32 tag_first;
    LwU32 tag_index;
    LwU32 tag_max;
    LwU32 tag_step;
    //LwU32 settle_time;
    LwU32 training_passed;
    //LwU32 hybrid_result_dq[TOTAL_DQ_BITS];
    //LwU32 hybrid_result_dbi[TOTAL_DBI_BITS];
    //LwU32 temp_result_dq[TOTAL_DQ_BITS];
    //LwU32 temp_result_dbi[TOTAL_DBI_BITS];
    LwU32 area = bFlagG6WrTrAreaEn;
    LwU32 i;
    //LwU32 ib = 0;
    LwU32 tag;
    LwU16 idx, subp, bit;

    // Piggybacking on VREF fields that match DFE requirements
    LwU32 fbpaTrainingWrDfeCtrl        = gTT.bootTrainingTable.WRITE_TRAINING_DFE;
    LwU8 var_wr_hybrid_min             = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MIN,  fbpaTrainingWrDfeCtrl);
    LwU8 var_wr_hybrid_max             = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_MAX,  fbpaTrainingWrDfeCtrl);
    LwU8 var_wr_hybrid_step            = REF_VAL(LW_PFB_FBPA_TRAINING_RW_VREF_CTRL_STEP, fbpaTrainingWrDfeCtrl);

    if(!vref) {
        tag_index = var_wr_hybrid_min;
        tag_max = var_wr_hybrid_max;
        tag_step = var_wr_hybrid_step;
    } else {
        tag_index = var_wr_vref_min;
        tag_max = var_wr_vref_max;
        tag_step = var_wr_vref_step;
    }
    tag_first = 1;

    // Initialize each optimum tag to fail value of 0xff
    // Initialize max_window/aclwm_area to 0
    if(dq) {
        for (i =0; i < TOTAL_DQ_BITS; i++) {
            gTD.wrHybridDfeDq[i] = 0xff;
            gTD.wrVrefDq[i] = 0xff;
            gTD.wrHybridDfeDqDbiValid = 0;
            gTD.wrVrefDqDbiValid = 0;
            //hybrid_result_dq[i] = 0x0;
        }
    }
    if(dbi) {
        for (i =0; i < TOTAL_DBI_BITS; i++) {
            gTD.wrHybridDfeDbi[i] = 0xff;
            gTD.wrVrefDbi[i] = 0xff;
            //hybrid_result_dbi[i] = 0x0;
        }
    }

    //for (tag = tag_index; tag <= tag_max; tag = tag + tag_step) {
    for (tag = tag_max; tag >= tag_index; tag = tag - tag_step) {
        // Set the SW sweep variable in DRAM
        //func_set_hybrid_parameter_in_dram(hybrid_param, tag_index);
        //func_set_hybrid_parameter_in_dram(tag_index);
        func_set_hybrid_parameter_in_dram(dbi,tag);

        // Start wait timer to allow for settling
        //start_timer(settle_time);

        // Set the Tag
        //func_set_tag_register(tag_first, tag_index, 1);
        func_set_tag_register(tag_first, tag, 1);

        // Wait settle timer to end
        //wait_timer_end();

        // Trigger training
        //func_run_training(addr=0, wck=0, rd=0, wr=1, periodic=0, char=0);

        func_run_training(0, 0, 0, 1, 0, 0);
        // Wait for Training to end
        func_wait_for_training_done();
        func_check_training_passed();

        tag_first = 0;
    } // for loop

    //func_set_tag_register(first=0, value=0, enable=0);
    func_set_tag_register(0, 0, 0);
    if (!falcon_to_determine_best_window) {
        if (vref)
        {
            func_read_tag(gTD.wrVrefDq, gTD.wrVrefDbi, gTD.wrHybridDfeEdc, 0);
            // Note: values are saved but these are mrs based so they don't need to be restored for gc6
            gTD.wrVrefDqDbiValid = 0;
        }
        else
        {
            func_read_tag(gTD.wrHybridDfeDq, gTD.wrHybridDfeDbi, gTD.wrHybridDfeEdc, 0);
            // Note: values are saved but these are mrs based so they don't need to be restored for gc6
            gTD.wrHybridDfeDqDbiValid = 0;
        }
    }

    // All hybrid settings are tested at this time

    // check if hybrid training passed
    training_passed = 1;
    //FW_MBOX_WR32(1, TOTAL_DQ_BITS);
    //FW_MBOX_WR32(1, TOTAL_DBI_BITS);

    if(dq)
    {
        for (idx = 0; idx < MAX_PARTS; idx++)
        {
            if (isPartitionEnabled(idx))
            {
                for(subp = 0; subp < 2; subp++)
                {
                    if ((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        for( bit = 0 ; bit < DQ_BITS_IN_SUBP; bit++)
                        {
                            if(gTD.wrHybridDfeDq[(idx * 64) + (subp * 32) + bit] == 0xff)
                            {
                                training_passed = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if(dbi)
    {
        for (idx = 0; idx < MAX_PARTS; idx++)
        {
            if (isPartitionEnabled(idx))
            {
                for(subp = 0; subp < 2; subp++)
                {
                    if ((subp == 1) || ((subp == 0) && (isLowerHalfPartitionEnabled(idx))))
                    {
                        for( bit = 0 ; bit < DBI_BITS_IN_SUBP; bit++)
                        {
                            if(gTD.wrHybridDfeDbi[(idx * 8) + (subp * 4) + bit] == 0xff)
                            {
                                training_passed = 0;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    if(!training_passed) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_TRAINING_ERROR);
    }

    if(!vref)
    {
        if(dq)
        {
            func_program_hybrid_opt_in_dram(gTD.wrHybridDfeDq, 1, 0,  gTT.bootTrainingTable.BKV_OFFSET.OB_DFE);
        }
        if(dbi)
        {
            func_program_hybrid_opt_in_dram(gTD.wrHybridDfeDbi, 1, 0,  gTT.bootTrainingTable.BKV_OFFSET.OB_DFE);;
        }
    }

    // read back design settings from training
    func_save_wr_vref_values(gTD.wrVrefDq, gTD.wrVrefDbi);

    // write back with offsets
    // TODO (stefans):
    //   access to mrs9 (func_program_hybrid_opt_in_dram) and mrs6 (func_program_vref_in_dram), could be unified due to
    //   the high similartiy of data and identical bit defintions on the 2 MRS values.  (Estimated Saving: close to 500B
    //   function is lwrrently 584B)
    if(dq)
    {
        func_program_vref_in_dram(gTD.wrVrefDq, 1, 0, gTT.bootTrainingTable.BKV_OFFSET.OB_VREF);
    }
    if(dbi)
    {
        func_program_vref_in_dram(gTD.wrVrefDbi, 0, 1,gTT.bootTrainingTable.BKV_OFFSET.OB_VREF);
    }

    // If area based analysis is enabled, do another run
    // of hw vref training
    if(area) {
        //func_run_training(addr=0, wck=0, rd=0, wr=1, periodic=0, char=0);
        func_run_training(0, 0, 0, 1, 0, 0);
        func_wait_for_training_done();

        training_passed = func_check_training_passed();
        if(!training_passed) {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_TRAINING_ERROR);
        }
    }

    // FbFlcn saves vref values training engine found
    if (!vref)
    {
        func_save_wr_vref_values(gTD.wrVrefDq, gTD.wrVrefDbi);
        // apply vref offsets
        // program them using   func_program_vref_in_dram ;
        // have to do dq and dbi loops
    }
    return training_passed;

} // end: func_run_wr_hybrid_tr_1pass

LwU32 func_run_wr_hybrid_tr_2pass
(
        LwU32 vref
) 
{
    LwU32 training_pass;

    LwU32 config_dbi= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CONFIG_DBI);
    LwU32 dbi = REF_VAL(LW_PFB_FBPA_FBIO_CONFIG_DBI_OUT_ON, config_dbi);

    //func_set_wr_dbi(gpu_dbi=1, dram_dbi=0); // gpu dbi = EN, dram dbi = DIS
    if(dbi) {
        func_set_wr_dbi(1, 0); // gpu dbi = EN, dram dbi = DIS
    }
    //training_pass = func_run_wr_hybrid_tr_1pass(vref=vref,dq=1, dbi=0);



    training_pass = func_run_wr_hybrid_tr_1pass(vref,1, 0);



    if(training_pass && dbi) {
        // DQ trainig failed - return
        // At this point DQ is trained. Keep its value stable in fbio
        LwU32 use_rd_dbi_expected = 0;
        LwU32 fbpa_training_ctrl2;
        fbpa_training_ctrl2= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
        fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB,     _FBPA_TRAINING_CTRL2,   _USE_RD_DBI_EXPECTED,    use_rd_dbi_expected,        fbpa_training_ctrl2);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2,   fbpa_training_ctrl2);

        //func_set_wr_dbi(gpu_dbi=1, dram_dbi=1); // gpu_dbi = EN, dram_dbi = EN
        func_set_wr_dbi(1, 1); // gpu_dbi = EN, dram_dbi = EN
        //training_pass = func_run_wr_hybrid_tr_1pass(vref=vref, dq=0, dbi=1);



        training_pass = func_run_wr_hybrid_tr_1pass(vref, 0, 1);



    }

    return training_pass;
} // end:func_run_wr_hybrid_tr_2pass

LwU32 func_run_wr_tr_hybrid_non_vref
(
        void
)
{
    LwU32 training_passed;
    LwU32 vref = 0;
    LwU32 dq = 1;
    LwU32 config_dbi= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CONFIG_DBI);
    LwU32 dbi = REF_VAL(LW_PFB_FBPA_FBIO_CONFIG_DBI_OUT_ON, config_dbi);

    if(bFlagG6WrTrPrbsEn) {
        training_passed = func_run_wr_hybrid_tr_2pass(vref);
    } else {
        training_passed = func_run_wr_hybrid_tr_1pass(vref,dq, dbi);
    }
    return training_passed;
}


LwU32 func_run_wr_tr_pi_only
(
        void
)
{
    LwU32 training_passed;
    // Just trigger training
    //func_run_training(addr=0, wck =0, rd=0 , wr=1, periodic =0, char=0);
    func_run_training(0, 0, 0 , 1, 0, 0);

    // Wait for training to end
    func_wait_for_training_done();

    // Check training status
    training_passed = func_check_training_passed();
    if(!training_passed) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WR_TRAINING_ERROR);
    }
    return training_passed;
}//end: func_run_wr_tr_pi_only


void func_char_init_for_prbs(
        void
) 
{
    func_setup_rd_prbs_regs(); 

    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CHAR_CTRL,gTT.bootTrainingTable.CHAR_CTRL);


    // GDDR5_COMMANDS = 0 for CHAR PRBS initialization
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

    func_wait_for_training_done();

    LwU8 status_tr_passed = func_check_training_passed();    

    if(!status_tr_passed) {
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
}

void func_disable_acpd
(
        void
)
{
    LwU32 dram_acpd = 0;
    LwU32 fbpa_cfg0;
    fbpa_cfg0= REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB,       _FBPA_CFG0,     _DRAM_ACPD,      dram_acpd,  fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,     fbpa_cfg0);
}

void func_enable_dram_self_refresh
(
        LwU8 cadtSrf
)
{
    if(bFlagG6AddrTrEn) {
        LwU32 fbpa_generic_mrs2;
        fbpa_generic_mrs2= REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS2);
        fbpa_generic_mrs2 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS2,     _CADT_SRF,    cadtSrf,        fbpa_generic_mrs2);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS2,     fbpa_generic_mrs2);
    }
}


void training_qpop_offset 
(
        LwU32 *pqpop_offset
)
{
    LwU32 fbpa_config1;
    LwU32 fbpa_config1_qpop_offset;
    fbpa_config1= REG_RD32(BAR0, LW_PFB_FBPA_CONFIG1);
    fbpa_config1_qpop_offset = REF_VAL(LW_PFB_FBPA_CONFIG1_QPOP_OFFSET,fbpa_config1);
    fbpa_config1 = FLD_SET_DRF_NUM(_PFB,    _FBPA_CONFIG1,  _QPOP_OFFSET,    *pqpop_offset,        fbpa_config1);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG1,  fbpa_config1);
    *pqpop_offset = fbpa_config1_qpop_offset;
}

void func_setup_addr_brlshft(void)
{
    LwU32 pad = 8;
    LwU32 fbpa_fbiotrng_subp0_cmd_brlshft1;
    fbpa_fbiotrng_subp0_cmd_brlshft1= REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD0,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD1,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD2,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD3,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD4,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD5,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD6,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    fbpa_fbiotrng_subp0_cmd_brlshft1 = FLD_SET_DRF_NUM(_PFB,        _FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1,      _PAD7,   pad,        fbpa_fbiotrng_subp0_cmd_brlshft1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT2,      fbpa_fbiotrng_subp0_cmd_brlshft1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT3,      fbpa_fbiotrng_subp0_cmd_brlshft1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT1,      fbpa_fbiotrng_subp0_cmd_brlshft1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT2,      fbpa_fbiotrng_subp0_cmd_brlshft1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_CMD_BRLSHFT3,      fbpa_fbiotrng_subp0_cmd_brlshft1);

}

LwU32 doBootTimeTraining (void)
{
    LwU32 p0Freq = gBiosTable.nominalFrequencyP0;
    LwU32 p8Freq = gBiosTable.nominalFrequencyP8;

    // Parsing BootTrainingTable flags
    BOOT_TRAINING_FLAGS *pTrainingFlags = &gTT.bootTrainingTable.MemBootTrainTblFlags;
    bFlagG6AddrTrEn                    = pTrainingFlags->flag_g6_addr_tr_en;
    bFlagG6WckTrEn                     = pTrainingFlags->flag_g6_wck_tr_en;
    bFlagG6RdTrEn                      = pTrainingFlags->flag_g6_rd_tr_en;
    bFlagG6RdTrPrbsEn                  = pTrainingFlags->flag_g6_rd_tr_prbs_en;
    bFlagG6RdTrAreaEn                  = pTrainingFlags->flag_g6_rd_tr_area_en;
    bFlagG6RdTrHybridNolwrefEn         = pTrainingFlags->flag_g6_rd_tr_hybrid_non_vref_en;
    bFlagG6RdTrHybridVrefEn            = pTrainingFlags->flag_g6_rd_tr_hybrid_vref_en;

    bFlagG6RdTrHwVrefEn                = pTrainingFlags->flag_g6_rd_tr_hw_vref_en;
    bFlagG6RdTrRedoPrbsSettings        = pTrainingFlags->flag_g6_rd_tr_redo_prbs_settings;
    //bFlagG6RdTrPatternShiftTrEn        = pTrainingFlags->flag_g6_rd_tr_pattern_shift_tr_en;
    bFlagG6WrTrEn                      = pTrainingFlags->flag_g6_wr_tr_en;
    bFlagG6WrTrPrbsEn                  = pTrainingFlags->flag_g6_wr_tr_prbs_en;
    bFlagG6WrTrAreaEn                  = pTrainingFlags->flag_g6_wr_tr_area_en;
    bFlagG6WrTrHybridNolwrefEn         = pTrainingFlags->flag_g6_wr_tr_hybrid_non_vref_en;
    bFlagG6WrTrHybridVrefEn            = pTrainingFlags->flag_g6_wr_tr_hybrid_vref_en;
    bFlagG6WrTrHwVrefEn                = pTrainingFlags->flag_g6_wr_tr_hw_vref_en;
    // bFlagG6PatternShiftTrEn            = pTrainingFlags->flag_g6_pattern_shift_tr_en;
    bFlagG6TrainRdWrDifferent          = pTrainingFlags->flag_g6_train_rd_wr_different;
    bFlagG6TrainPrbsRdWrDifferent      = pTrainingFlags->flag_g6_train_prbs_rd_wr_different;
    bFlagG6RefreshEnPrbsTr             = pTrainingFlags->flag_g6_refresh_en_prbs_tr;       
    bFlagG6EdcTrackingEn               = pTrainingFlags->flag_edc_tracking_en;
    bFlagG6VrefTrackingEn              = pTrainingFlags->flag_vref_tracking_en;
    bFlagG6RdEdcEnabled                = pTrainingFlags->flag_rd_edc_enabled;
    bFlagG6WrEdcEnabled                = pTrainingFlags->flag_wr_edc_enabled;
    //bFlagG6EdcTrackingPrbsHoldPattern  = pTrainingFlags->flag_edc_tracking_prbs_hold_pattern;

    //REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD(0),gTT.bootTrainingTable.GEN_CMD);
    // Working script:
    saved_reg->training_patram_store          = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    saved_reg->training_char_bank_ctrl_store  = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL);
    saved_reg->training_char_bank_ctrl2_store = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_BANK_CTRL2);
    saved_reg->training_char_burst_store      = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_BURST);
    saved_reg->training_char_ctrl_store       = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CHAR_CTRL);
    saved_reg->pfb_fbpa_training_cmd          = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CMD(0));
    saved_reg->training_ctrl2                 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    saved_reg->training_tag                   = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_TAG);
    saved_reg->training_ctrl_store            = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);

    #if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    privprofilingEnable();
    privprofilingReset();
    #endif

    //osPTimerTimeNsLwrrentGet(&bootTrTimeNs);



    //func_set_prbs_dual_mode(0,1);
    // Setup EDC half rate before entering switch to P0
    func_setup_edc_rate(0); // 0 - half rate; 1 - full rate



    // Step 1; Enable address training
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



    // Also add bFlagG6RdTrPrbsEn to if
    if(bFlagG6AddrTrEn) {
        func_enable_dram_self_refresh(1);         
    }

    //osPTimerTimeNsLwrrentGet(&bootTrTimeNs);

    doMclkSwitchInMultipleStages(p0Freq , 1,0,0);            



    func_setup_misc_training_ctrl();       
    func_setup_training_timings();


    LwU32 status_tr_passed;                                      // Training pass/fail status
    //LwU32 pqpop_offset = 2;

    if(bFlagG6AddrTrEn) {
        // Use a QPOP_OFFSET of 2 for address training.  Bug 810795
        //training_qpop_offset(&pqpop_offset);
        func_setup_g6_addr_tr();
        status_tr_passed = gddr_addr_training(12);

        func_enable_dram_self_refresh(0);         

        //status_tr_passed = func_check_training_passed();    

        if(!status_tr_passed) {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_ADDRESS_TRAINING_ERROR);
        }


        // FbFlcn saves address training values
        //memoryMoveCmdTrainingValues_HAL(&Fbflcn,REGISTER_READ);
        //gTD.td.cmdDelayValid = 1;
    } 



    doMclkSwitchInMultipleStages(p0Freq , 0,1,0); 




    if (bFlagG6WckTrEn)  {
        func_setup_g6_wck_tr();

        // ucode handshake with testbench to enable WCK Training Monitor
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x5452476);


        func_run_training(0, 1, 0, 0, 0, 0);
        func_wait_for_training_done();
        status_tr_passed = func_check_training_passed();          

        if(!status_tr_passed) {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_BOOT_TRAINING_WCK_TRAINING_ERROR);
        }

        // ucode handshake with testbench to enable WCK Training Monitor
        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x5452477);

    }

    // EDC Tracking
    if(bFlagG6EdcTrackingEn) {
        if(bFlagG6RdEdcEnabled || bFlagG6WrEdcEnabled) {
            //func_disable_edc_edc_trk();
        }
        // Add 32 to intrpltr_offsets before EDC_TRACKING_EN=1;
        //func_fbio_intrpltr_offsets(38,60);
        func_edc_tracking();

        LwU8 disable_training_updates = 1;
        LwU32 fbpa_fbio_edc_tracking= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_EDC_TRACKING);
        fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _DISABLE_TRAINING_UPDATES,       disable_training_updates,   fbpa_fbio_edc_tracking);
        REG_WR32(BAR0, LW_PFB_FBPA_FBIO_EDC_TRACKING,        fbpa_fbio_edc_tracking);
    }




    // setup for 3D RD DFE Hybrid training for DQ + DBI.
    // Read training
    LwU32 hybrid;
    LwU32 vref_hw;
    //LoadTrainingPatram();
    FuncLoadRdPatterns(12);
    if(bFlagG6RdTrEn) {
        if (bFlagG6RdTrHybridNolwrefEn) {
            // This is the preferred path and 
            // flags in VBIOS should be updated to choose this path
            hybrid  = 1; 
            vref_hw = 1;
            func_setup_rd_tr(hybrid, vref_hw, bFlagG6RdTrPrbsEn, bFlagG6RdTrAreaEn , bFlagG6RdTrRedoPrbsSettings);
            //status_tr_passed = func_run_rd_tr_hybrid_non_vref(bFlagG6RdTrAreaEn);
        } else if (bFlagG6RdTrHybridVrefEn) {
            hybrid  = 1; 
            vref_hw = 0;
            func_setup_rd_tr(hybrid, vref_hw, bFlagG6RdTrPrbsEn, bFlagG6RdTrAreaEn, bFlagG6RdTrRedoPrbsSettings);
            //status_tr_passed = func_run_rd_tr_hybrid_vref();
        } else if (bFlagG6RdTrHwVrefEn) {
            hybrid  = 0; 
            vref_hw = 1;
            func_setup_rd_tr(hybrid, vref_hw, bFlagG6RdTrPrbsEn, bFlagG6RdTrAreaEn, bFlagG6RdTrRedoPrbsSettings);
            //status_tr_passed = func_run_rd_tr_hw_vref();
        } else {
            hybrid  = 0; 
            vref_hw = 0;
            func_setup_rd_tr(hybrid, vref_hw, bFlagG6RdTrPrbsEn, bFlagG6RdTrAreaEn, bFlagG6RdTrRedoPrbsSettings);
            func_run_rd_tr_pi_only();
        }

    }




    // Write training
    LwU32 wr_hybrid  = 1;                                  // NOTE has to come from VBIOS
    LwU32 wr_vref_hw = 1;                                  // NOTE has to come from VBIOS

    //privprofilingEnable(); 
    //privprofilingReset();



    if(bFlagG6WrTrEn) {
        if(bFlagG6WrTrHybridNolwrefEn) {
            // This is the preferred path and 
            // flags in VBIOS should be updated to choose this path
            wr_hybrid  = 1;
            wr_vref_hw = 1;
            func_setup_wr_tr(wr_hybrid, wr_vref_hw, bFlagG6WrTrPrbsEn, bFlagG6WrTrAreaEn, bFlagG6TrainRdWrDifferent,bFlagG6TrainPrbsRdWrDifferent);
            //status_tr_passed = func_run_wr_tr_hybrid_non_vref();
        } else if (bFlagG6WrTrHybridVrefEn) {
            wr_hybrid  = 1;
            wr_vref_hw = 0;
            func_setup_wr_tr(wr_hybrid, wr_vref_hw, bFlagG6WrTrPrbsEn, bFlagG6WrTrAreaEn, bFlagG6TrainRdWrDifferent, 0);
            //status_tr_passed = func_run_wr_tr_hybrid_vref();
        } else if (bFlagG6WrTrHwVrefEn) {
            wr_hybrid  = 0;
            wr_vref_hw = 1;
            func_setup_wr_tr(wr_hybrid, wr_vref_hw, bFlagG6WrTrPrbsEn, bFlagG6WrTrAreaEn, bFlagG6TrainRdWrDifferent, 0);
            //status_tr_passed = func_run_wr_tr_hw_vref();
        } else {
            wr_hybrid  = 0;
            wr_vref_hw = 0;
            func_setup_wr_tr(wr_hybrid, wr_vref_hw, bFlagG6WrTrPrbsEn, bFlagG6WrTrAreaEn, bFlagG6TrainRdWrDifferent, 0);
            func_run_wr_tr_pi_only();
        }
    }



    // Disable PRBS_mode for subsequent periodic training
    func_set_prbs_dual_mode(0,1);

    doMclkSwitchInMultipleStages(p0Freq , 0,0,1);    

    //Skipping restoring registers as DFE/VREF trainings are not done
    //func_restore_training_ctrls();



    // Switch to P8.
    if (p8Freq !=0 ) {
        doMclkSwitchPrimary(p8Freq, 0, 0, 0);
    }


    #if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    privprofilingDisable();
    #endif
    LwU32 result = 0xabcd1234;
    FW_MBOX_WR32(1, 0xfeefeffe);
    return result;
}





