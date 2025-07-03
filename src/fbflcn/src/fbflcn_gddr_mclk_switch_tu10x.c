#include <falcon-intrinsics.h>
#include <falc_debug.h>
#include <falc_trace.h>

#include "fbflcn_gddr_mclk_switch.h"
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "fbflcn_mutex.h"
#include "priv.h"
#include "osptimer.h"

#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"
#include "dev_fb.h"
#include "dev_trim.h"
#include "dev_pmgr.h"
#include "dev_pwr_pri.h"
#include "rmfbflcncmdif.h"
#include "dev_pri_ringstation_fbp.h"
#include "priv.h"

#include "osptimer.h"
#include "memory.h"
#include "memory_gddr.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_memory_private.h"

PLLInfo5xEntry* pInfoEntryTable_Refmpll;
PLLInfo5xEntry* pInfoEntryTable_Mpll;
MemInfoEntry  *mInfoEntryTable;
EdcTrackingGainTable my_EdcTrackingGainTable;
LwU32 gbledctrgain;
LwU32 gblvreftrgain;

//#define SWITCH_LOG



// ****************************************************************************
// ***                                                                      ***
// ***                  FB-Falcon Mclk Switch Microcode                     ***
// ***                                                                      ***
// ****************************************************************************
//
// ****************************************************************************
//
// Microcode to manage Mclk switches by interacting with Host/RM and PMU-Falcon
// and processing Mclk requests from them by controlling and reprogramming FB
// through the Mclk switch sequence.
//
// Mclk switch flow happens as follows:
//
// 1. A command is sent to FB-Falcon by Host/RM (FB-Falcon CQ0) or PMU-Falcon
//    (FB-Falcon CQ1). Command should include header info, sender ID info, and
//    data about the target Mclk frequency plus any other table/SKU info which
//    may be needed to tailor the FB programming to non-default values (eg.
//    desired TB randomizations).
//
// 2. FB-Falcon callwlates the pll co-efficients, does initial mclk switch setup
//    such as saving register values.
//
// 3. FB-Falcon enters a waiting state while it waits for an OK-to-switch
//    interrupt from Display (or other SoC IP).
//
// 4. Upon receipt of the OK-to-switch signal, FB-Falcon initiates and drives
//    the Mclk switch by FB register writes and control of TB components that
//    model/abstract away SoC IPs (that would normally drive FB input pins).
//
// 5. Upon Mclk switch completion, another message is sent to Host/RM and/or
//    PMU-Falcon to inform the requester of the status of the FB.
//
// ****************************************************************************


//defines to improve readability. The training delay device is a vbios
//field. Having these defines to decide which clock path to consider
//based on the frequency.
#define GDDR_TARGET_TRIMMER_PATH 0
#define GDDR_TARGET_REFMPLL_CLK_PATH 1
#define GDDR_TARGET_DRAMPLL_CLK_PATH 2

#define GDDR_PRIV_SRC_REG_SPACE_ONESRC 0
#define GDDR_PRIV_SRC_REG_SPACE_REFMPLL 1
#define GDDR_PRIV_SRC_REG_SPACE_DRAMPLL 2

#define LW_PFB_FBPA_TRAINING_CMD0                                                                    LW_PFB_FBPA_TRAINING_CMD(0)
#define LW_PFB_FBPA_TRAINING_CMD1                                                                    LW_PFB_FBPA_TRAINING_CMD(1)

#define REFMPLL_BASE_FREQ                             27
#define DRAMPLL_BASE_FREQ_REFMPLL_OUT_200            200
#define DRAMPLL_BASE_FREQ_REFMPLL_OUT_250            250
#define REFMPLL_HIGH_VCO_SELDIVBY2_EN_BASE_FREQ     2500
#define REFMPLL_HIGH_VCO_SELDIVBY2_EN_MAX_FREQ      3999
#define DRAMPLL_N_UNDERFLOW_BASE_FREQ               5000
#define DRAMPLL_DIVBY2_DISABLE_FREQ_THRESHOLD       4000


FLCN_TIMESTAMP  startFbStopTimeNs = { 0 };
LwU32 bit_assign (LwU32 reg_val, LwU32 bitpos, LwU32 bitval, LwU32 bitsize);
LwU32 bit_extract (LwU32 reg_val, LwU32 bitpos, LwU32 bitsize);

void stop_fb (void);
void start_fb (void);
void gddr_retrieve_lwrrent_freq(void);
void gddr_callwlate_target_freq_coeff(void);
void gddr_callwlate_refmpll_coeff(void);
void gddr_callwlate_drampll_coeff(void);
void gddr_callwlate_onesrc_coeff(void);
void gddr_get_target_clk_source(void);
LwU32 gddr_mclk_switch (LwBool bPreAddressBootTraining, LwBool bPostAddressBootTraining, LwBool bPostWckRdWrBootTraining);

LwU32 gbl_dram_acpd;
LwU32 gbl_dram_asr;
LwU32 gbl_vendor_id;
LwU32 gbl_type;
LwU32 GblRxDfe;
LwU32 gbl_strap;
LwU32 gbl_index;
LwBool GblEnMclkAddrTraining;
LwBool GblEnWckPinModeTr = LW_FALSE;
LwU32 GblRefmpllSpreadEn;
LwU32 GblSdmEn;
LwBool GblAddrTrPerformedOnce = LW_FALSE;
LwBool GblBootTimeAdrTrng;
LwBool GblBootTimeEdcTrk;
LwBool GblBootTimeVrefTrk;
LwBool GblSkipBootTimeTrng;
LwU32 GblSwitchCount =0;
LwU32 hbm_dramclk_period_ps;
LwU32 rx_ddll;
LwU32 tx_ddll;
LwU32 wl_ddll;
LwU32 unit_level_sim ;

LwU32 gddr_lwrr_clk_source;
LwU32 gddr_target_clk_source;
LwU32 gbl_target_freqKHz;
LwU16 gbl_target_freqMHz;
LwU32 gbl_lwrrent_freqKHz;
LwU16 gbl_lwrrent_freqMHz;
LwU32 gbl_computation_freqMHz;
LwU32 gbl_mclk_freq_new;
LwU32 gbl_onsrc_divby_val;
LwU32 gbl_refpll_base_freq = REFMPLL_BASE_FREQ;
LwF32 gbl_refmpll_freqMHz;
LwU32 gbl_ddr_mode;
LwU32 gbl_g5x_cmd_mode;
LwU32 gbl_prev_dramclk_mode;
LwU32 gbl_first_p0_switch;
LwU32 gbl_periodic_trng;
LwU32 gbl_dq_term_mode_save;
LwU32 gbl_save_fbio_pwr_ctrl;
LwU32 gbl_prev_edc_tr_enabled;
LwU32 gbl_unit_sim_run;
LwU32 gblEnEdcTracking = 0;
LwU32 gblElwrefTracking = 0;
LwU32 gblEdcTrGain;
LwU32 gblVrefTrGain;

LwU32 gbl_lwrr_fbvddq;
LwU32 gbl_target_vauxc_level;
LwBool gbl_bFbVddSwitchNeeded = LW_FALSE;
LwBool gbl_bVauxChangeNeeded = LW_FALSE;
LwBool gbl_fbvdd_low_to_high = LW_FALSE;
LwBool gbl_fbvdd_high_to_low = LW_FALSE;
LwU8 GblSaveDqVrefVal;
LwU8 GblSaveDqsVrefVal;


LwU32 gddr_target_clk_path;
LwU32 refmpll_m_val;
LwU32 refmpll_n_val;
LwU32 refmpll_p_val;
LwU32 drampll_m_val;
LwU32 drampll_n_val;
LwU32 drampll_p_val;
LwU32 drampll_divby2;
LwF32 flt_drampll_n_val;
LwS32 refmpll_sdm_din;
LwS32 refmpll_ssc_min_rm;
LwS32 refmpll_ssc_max_rm;
LwS32 refmpll_ssc_step;
LwU32 fbio_mode_switch;
LwU32 tb_sig_g5x_cmd_mode;
LwU32 tb_sig_dramclk_mode;

LwU32
bit_assign
(
        LwU32 reg_val,
        LwU32 bitpos,
        LwU32 bitval,
        LwU32 bitsize
)
{
    LwU32 bitmask;
    LwU32 bitdata;

    bitmask = ~ (((0x01 << bitsize) - 1) << bitpos);
    bitdata = bitval << bitpos;
    reg_val &= bitmask;
    reg_val |= bitdata;

    return (reg_val);
}

LwU32
bit_extract
(
        LwU32 reg_val,
        LwU32 bitpos,
        LwU32 bitsize
)
{
    LwU32 bitmask;
    LwU32 bitdata;

    bitmask = (0x01 << bitsize) - 1;
    bitdata = (reg_val >> bitpos) & bitmask;

    return (bitdata);
}


void
stop_fb
(
        void
)
{
    LwU32 ack_stop_fb;
    LwU32 cmd_stop_fb;

    cmd_stop_fb = 0x00000000;
    cmd_stop_fb = FLD_SET_DRF (_PPWR, _PMU_GPIO_OUTPUT, _SET_FB_STOP_REQ, _TRIGGER, cmd_stop_fb);

    REG_WR32_STALL(LOG, LW_PPWR_PMU_GPIO_OUTPUT_SET, cmd_stop_fb);

    ack_stop_fb = 0;
    while (ack_stop_fb != LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK_TRUE)
    {
        ack_stop_fb = REG_RD32(LOG, LW_PPWR_PMU_GPIO_INPUT);
        ack_stop_fb = REF_VAL(LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK, ack_stop_fb);
    }

    return;
}


void
start_fb
(
        void
)
{
    LwU32 ack_start_fb;
    LwU32 cmd_start_fb;

    cmd_start_fb = 0x00000000;
    cmd_start_fb = FLD_SET_DRF (_PPWR, _PMU_GPIO_OUTPUT, _CLEAR_FB_STOP_REQ, _CLEAR, cmd_start_fb);

    REG_WR32_STALL(LOG, LW_PPWR_PMU_GPIO_OUTPUT_CLEAR, cmd_start_fb);

    // Send a s/w refresh as a workaround for clk gating bug
    // This should keep the clocks running
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF,0x00000001);

    ack_start_fb = 0x01;
    while (ack_start_fb != LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK_FALSE)
    {
        ack_start_fb = REG_RD32(LOG, LW_PPWR_PMU_GPIO_INPUT);
        ack_start_fb = REF_VAL(LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK,ack_start_fb);
    }

    return;
}

LwU32
gddr_ceil
(
        LwU32 val
)
{
    LwU32 val_int ;
    val_int = val/1000;
    if (val - (val_int *1000) > 0) { val_int = val_int + 1; }

    return val_int;
}

void
gddr_get_target_clk_source
(
        void
)
{
    LwU32 training_dly_dev;

    training_dly_dev = gTT.perfMemClkStrapEntry.Flags3.PMC11SEF3TrnDlyDev;

    if (training_dly_dev == MEMCLK1_STRAPENTRY_TRAINING_DLY_DEVICE_TRIM) {
        //target frequency is requesting for onesrc clock path
        gddr_target_clk_path = GDDR_TARGET_TRIMMER_PATH;
    } else if (training_dly_dev == MEMCLK1_STRAPENTRY_TRAINING_DLY_DEVICE_DLL) {
        //target frequency is requesting for REFMPLL clock path
        gddr_target_clk_path = GDDR_TARGET_REFMPLL_CLK_PATH;
    } else {
        //target frequency is requesting for DRAMPLL clock path
        gddr_target_clk_path = GDDR_TARGET_DRAMPLL_CLK_PATH;
    }

    return;
}

void
gddr_retrieve_lwrrent_freq
(
        void
)
{
    LwU32 ref_vco_freq;
    LwU32 mpll_vco_freq;
    LwU32 refmpll_coeff;
    LwU32 drampll_coeff;
    LwU32 lwr_refmpll_n_val = 0;
    LwU32 lwr_refmpll_m_val = 0;
    LwU32 lwr_refmpll_p_val = 0;
    LwU32 lwr_drampll_n_val = 0;
    LwU32 lwr_drampll_m_val = 0;
    LwU32 lwr_drampll_divby2_val = 0;


    // red refmpll and drampll
    refmpll_coeff = REG_RD32(LOG, LW_PFB_FBPA_REFMPLL_COEFF);
    lwr_refmpll_n_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_NDIV,refmpll_coeff);
    lwr_refmpll_m_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_MDIV,refmpll_coeff);
    lwr_refmpll_p_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_PLDIV,refmpll_coeff);

    ref_vco_freq = lwr_refmpll_n_val * gbl_refpll_base_freq ;
    gbl_lwrrent_freqMHz =  ref_vco_freq / (lwr_refmpll_m_val * lwr_refmpll_p_val) ;
    gbl_lwrrent_freqKHz = (gbl_lwrrent_freqMHz * 1000);

    if (gbl_prev_dramclk_mode ==  LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) {
        drampll_coeff = REG_RD32(LOG, LW_PFB_FBPA_DRAMPLL_COEFF);

        lwr_drampll_n_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_NDIV,drampll_coeff);
        lwr_drampll_m_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_MDIV,drampll_coeff);
        lwr_drampll_divby2_val = ((REF_VAL(LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2,drampll_coeff)) & 0x1);

        mpll_vco_freq = lwr_drampll_n_val * gbl_lwrrent_freqMHz;
        gbl_lwrrent_freqMHz = (mpll_vco_freq ) / ( lwr_drampll_m_val << lwr_drampll_divby2_val);

        gbl_lwrrent_freqKHz = gbl_lwrrent_freqMHz * 1000;
    }

    return;
}

void
gddr_callwlate_target_freq_coeff
(
        void
)
{

    if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
        gddr_callwlate_refmpll_coeff();
    } else if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        gddr_callwlate_refmpll_coeff();
        gddr_callwlate_drampll_coeff();
    } else {
        gddr_callwlate_refmpll_coeff();
    }
    return;
}

void
gddr_callwlate_ssd_variables
(
        LwF32   refmpll_vco_freq
)
{

    LwU32 repeat_sdm;
    LwF32 refmpll_n_fractional ; //real value of N to achieve VCO frequency
    LwF32 refmpll_sdm_frac_dec ;
    LwF32 refmpll_sdm_frac_n_dec ;
    LwF32 refmpll_sdm_frac_int ;
    LwU32 refmpll_sdm_frac_int_rm;
    LwU8 spread_lo;
    LwU8 spread_hi;
    LwF32 refmpll_rise;
    LwU32 increment_sdm;
    LwF32 flt_increment_sdm;
    LwU8 RefMPLLSCFreqDelta = gTT.perfMemClkBaseEntry.RefMPLLSCFreqDelta;
    spread_lo = RefMPLLSCFreqDelta ;
    //down spread only enabled
    spread_hi = 0;

    refmpll_n_fractional = (refmpll_vco_freq * 1024) / gbl_refpll_base_freq ; //FOLLOWUP move compute this in refmpll coeff section once. cast to int for refmpll_n_val
    refmpll_sdm_frac_dec = refmpll_n_fractional - (refmpll_n_val * 1024) - 512 ;
    refmpll_sdm_frac_n_dec = refmpll_sdm_frac_dec / 4.0 ;

    refmpll_sdm_frac_int = ((refmpll_sdm_frac_n_dec) * 32) ;

    if (refmpll_sdm_frac_n_dec < 0)
    {
        refmpll_sdm_frac_int = refmpll_sdm_frac_int + 131072 ;
    }

    refmpll_sdm_din = (LwU32)(refmpll_sdm_frac_int + 0.5) ;

    refmpll_sdm_frac_int_rm = refmpll_sdm_din ;
    if (refmpll_sdm_din > 65536)
    {
        refmpll_sdm_frac_int_rm = refmpll_sdm_frac_int_rm - 131072 ;
    }

    LwU32 deltarefmpll_ssc_min_rm;//, deltarefmpll_ssc_max_rm;
    deltarefmpll_ssc_min_rm = (int)(((refmpll_sdm_frac_int_rm + 4096 + refmpll_n_val*8192) * spread_lo) / 10000) ;
    //deltarefmpll_ssc_max_rm = (int)((refmpll_sdm_frac_int_rm + 4096 + refmpll_n_val*8192) * spread_hi / count_for_1us_delay_in_ps) ;

    refmpll_ssc_min_rm = refmpll_sdm_din - deltarefmpll_ssc_min_rm;
    //refmpll_ssc_max_rm = refmpll_sdm_din + deltarefmpll_ssc_max_rm;
    refmpll_ssc_max_rm = refmpll_sdm_din ;

    if (refmpll_ssc_min_rm < 0)
    {
        refmpll_ssc_min_rm = refmpll_ssc_min_rm + 131072 ;
    }

    if (refmpll_ssc_max_rm < 0)
    {
        refmpll_ssc_max_rm = refmpll_ssc_max_rm + 131072 ;
    }


    //Callwlate ssc_step
    //refmpll_rise =  132 * ( refmpll_ssc_max_rm - refmpll_ssc_min_rm ) ; //*refmpll_m_val which is 1
    refmpll_rise =  132 * ( deltarefmpll_ssc_min_rm ) ; //*refmpll_m_val which is 1
    flt_increment_sdm = refmpll_rise / (gbl_refpll_base_freq*1000) ;
    increment_sdm = flt_increment_sdm  + 0.5;
    repeat_sdm = 2 ;
    FW_MBOX_WR32(6, (LwU32)refmpll_rise);
    if (increment_sdm % 2 == 0)
    {
        increment_sdm = increment_sdm / 2;
        repeat_sdm = repeat_sdm / 2 ;
    }
    FW_MBOX_WR32(5, (LwU32)increment_sdm);
    FW_MBOX_WR32(7, (LwU32)repeat_sdm);

    refmpll_ssc_step = ((repeat_sdm - 1) << 12) |  increment_sdm;

    return;
}

void
gddr_callwlate_refmpll_coeff
(
        void
)
{
    LwU32 refmpll_vco_freq;
    LwF32 flt_refmpll_vco_freq;

    if (gbl_unit_sim_run) {gbl_refpll_base_freq = 20;}


    //Considering prev clock path
    flt_refmpll_vco_freq = 0;
    refmpll_vco_freq = 0;
    gbl_computation_freqMHz = 0;
    if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)  || (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH))  {


        //Callwlation based on Bob's Algorithm for freq less than 1Ghz
        refmpll_m_val = 1;

        refmpll_p_val = ((gBiosTable.pIEp_refmpll->VCOMax)/gbl_target_freqMHz); //multiplying by 1000 to get 3 decimal resolution
        flt_refmpll_vco_freq = gbl_target_freqMHz * refmpll_p_val;
        refmpll_vco_freq = flt_refmpll_vco_freq;
        refmpll_n_val = refmpll_vco_freq/gbl_refpll_base_freq;
        gbl_refmpll_freqMHz = gbl_target_freqMHz;

    } else if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        //generating ref pll frequency between 200Mhz to 300Mhz
        //Fixing the p_val to 10 and m_val to 1;

        //if the frequency is less than 4Ghz then we would enable dram sel_divby 2 and
        //compute frequency for double the target frequency
        if (gbl_target_freqMHz < DRAMPLL_DIVBY2_DISABLE_FREQ_THRESHOLD) {
            gbl_computation_freqMHz = gbl_target_freqMHz * 2;
            drampll_divby2 = 1;
        } else {
            gbl_computation_freqMHz = gbl_target_freqMHz;
            drampll_divby2 = 0;
        }


        refmpll_m_val = 1;
        if (gbl_target_freqMHz < DRAMPLL_N_UNDERFLOW_BASE_FREQ) {
            if ((gbl_target_freqMHz >= REFMPLL_HIGH_VCO_SELDIVBY2_EN_BASE_FREQ) &&
                    (gbl_target_freqMHz <= REFMPLL_HIGH_VCO_SELDIVBY2_EN_MAX_FREQ)) {
                flt_drampll_n_val = (gbl_computation_freqMHz / DRAMPLL_BASE_FREQ_REFMPLL_OUT_250);
            } else {
                flt_drampll_n_val = (gbl_computation_freqMHz / DRAMPLL_BASE_FREQ_REFMPLL_OUT_200);
            }
        } else {

            flt_drampll_n_val = (gbl_computation_freqMHz / DRAMPLL_BASE_FREQ_REFMPLL_OUT_250);
        }

        gbl_refmpll_freqMHz = (gbl_computation_freqMHz / flt_drampll_n_val) ;
        refmpll_p_val = (gBiosTable.pIEp_refmpll->VCOMax)/gbl_refmpll_freqMHz;
        flt_refmpll_vco_freq = gbl_refmpll_freqMHz * refmpll_p_val;
        refmpll_n_val = flt_refmpll_vco_freq/gbl_refpll_base_freq;
        refmpll_vco_freq = flt_refmpll_vco_freq;
    }

    GblRefmpllSpreadEn = gTT.perfMemClkBaseEntry.Flags1.PMC11EF1MPLLSSC;
    GblSdmEn = gTT.perfMemClkBaseEntry.Flags0.PMC11EF0SDM;
    if ((GblRefmpllSpreadEn) || (GblSdmEn)) {
        gddr_callwlate_ssd_variables(flt_refmpll_vco_freq);
    }

    //clock m,p,n val check
    if ((refmpll_vco_freq < TABLE_VAL(gBiosTable.pIEp_refmpll->VCOMin)) || (refmpll_vco_freq > TABLE_VAL(gBiosTable.pIEp_refmpll->VCOMax) )) {
        //Change mbox 1 to 13,12,11,10 descending order
        //add error or log template with log number defined for gddr_mclk and error log being defined on top.
        FW_MBOX_WR32(10, 0xDEADEC00);
        FW_MBOX_WR32(11, TABLE_VAL(gBiosTable.pIEp_refmpll->VCOMin));
        FW_MBOX_WR32(12, refmpll_vco_freq);
        FW_MBOX_WR32(13, TABLE_VAL(gBiosTable.pIEp_refmpll->VCOMax));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_VCO_ERROR);
    }
    if ((refmpll_m_val < TABLE_VAL(gBiosTable.pIEp_refmpll->MMin)) || (refmpll_m_val > TABLE_VAL(gBiosTable.pIEp_refmpll->MMax))) {
        FW_MBOX_WR32(10, 0xDEADEC01);
        FW_MBOX_WR32(11, TABLE_VAL(gBiosTable.pIEp_refmpll->MMin));
        FW_MBOX_WR32(12, refmpll_m_val);
        FW_MBOX_WR32(13, TABLE_VAL(gBiosTable.pIEp_refmpll->MMax));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_M_VAL_ERROR);
    }
    if ((refmpll_n_val < TABLE_VAL(gBiosTable.pIEp_refmpll->NMin)) || (refmpll_n_val > TABLE_VAL(gBiosTable.pIEp_refmpll->NMax))) {
        FW_MBOX_WR32(10, 0xDEADEC02);
        FW_MBOX_WR32(11, TABLE_VAL(gBiosTable.pIEp_refmpll->NMin));
        FW_MBOX_WR32(12, refmpll_n_val);
        FW_MBOX_WR32(13, TABLE_VAL(gBiosTable.pIEp_refmpll->NMax));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_N_VAL_ERROR);
    }
    if ((refmpll_p_val < TABLE_VAL(gBiosTable.pIEp_refmpll->PLMin)) || (refmpll_p_val > TABLE_VAL(gBiosTable.pIEp_refmpll->PLMax))) {
        FW_MBOX_WR32(10, 0xDEADEC03);
        FW_MBOX_WR32(11, TABLE_VAL(gBiosTable.pIEp_refmpll->PLMin));
        FW_MBOX_WR32(12, refmpll_p_val);
        FW_MBOX_WR32(13, TABLE_VAL(gBiosTable.pIEp_refmpll->PLMax));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_REMPLL_P_VAL_ERROR);
    }


    return;

}

void
gddr_callwlate_drampll_coeff
(
        void
)
{
    LwU32 drampll_vco_freq;

    drampll_p_val  = 0;

    drampll_m_val  = 1;
    drampll_n_val = flt_drampll_n_val;
    drampll_vco_freq = gbl_refmpll_freqMHz * drampll_n_val / drampll_m_val;

    //clock m,p,n val check
    // the clock range check does includ pmct maximum frequency to allow for overclocking targets
    if ( (drampll_vco_freq < TABLE_VAL(pInfoEntryTable_Mpll->VCOMin)) ||
            ( (drampll_vco_freq > TABLE_VAL(pInfoEntryTable_Mpll->VCOMax)) &&
                    (drampll_vco_freq > gTT.perfMemClkBaseEntry.Maximum.PMCBEMMFreq) ))
    {
        FW_MBOX_WR32(9, 0xDEADEC10);
        FW_MBOX_WR32(10, gTT.perfMemClkBaseEntry.Maximum.PMCBEMMFreq);
        FW_MBOX_WR32(11, TABLE_VAL(pInfoEntryTable_Mpll->VCOMin));
        FW_MBOX_WR32(12, drampll_vco_freq);
        FW_MBOX_WR32(13, TABLE_VAL(pInfoEntryTable_Mpll->VCOMax));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_VCO_ERROR);
    }
    if ((drampll_m_val < TABLE_VAL(pInfoEntryTable_Mpll->MMin)) || (drampll_m_val > TABLE_VAL(pInfoEntryTable_Mpll->MMax))) {
        FW_MBOX_WR32(10, 0xDEADEC11);
        FW_MBOX_WR32(11, TABLE_VAL(pInfoEntryTable_Mpll->MMin));
        FW_MBOX_WR32(12, drampll_m_val);
        FW_MBOX_WR32(13, TABLE_VAL(pInfoEntryTable_Mpll->MMax));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_M_VAL_ERROR);
    }
    if ((drampll_n_val < TABLE_VAL(pInfoEntryTable_Mpll->NMin)) || (drampll_n_val > TABLE_VAL(pInfoEntryTable_Mpll->NMax))) {
        FW_MBOX_WR32(10, 0xDEADEC12);
        FW_MBOX_WR32(11, TABLE_VAL(pInfoEntryTable_Mpll->NMin));
        FW_MBOX_WR32(12, drampll_n_val);
        FW_MBOX_WR32(13, TABLE_VAL(pInfoEntryTable_Mpll->NMax));
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_N_VAL_ERROR);
    }

    return;
}

void
gddr_callwlate_onesrc_coeff
(
        void
)
{

    gbl_onsrc_divby_val = (int) gbl_target_freqKHz / 1600;
    return;
}

void
gddr_load_reg_values
(
        void
)
{
    lwr_reg->pfb_fbpa_fbio_spare                    = REG_RD32(LOG, LW_PFB_FBPA_FBIO_SPARE);
    lwr_reg->pfb_fbpa_fbio_cfg1                     = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG1);
    lwr_reg->pfb_fbpa_fbio_cfg8                     = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG8);

    lwr_reg->pfb_fbpa_fbio_cfg_pwrd                 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD);
    lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl             = REG_RD32(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL);
    lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1            = REG_RD32(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL1);
    lwr_reg->pfb_fbpa_fbio_cfg10                    = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG10);
    lwr_reg->pfb_fbpa_fbio_config5                  = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CONFIG5);
    lwr_reg->pfb_fbpa_training_cmd_1                = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CMD1);//check
    lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2           = REG_RD32(LOG, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2);
    lwr_reg->pfb_fbpa_fbio_adr_ddll                 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_ADR_DDLL);
    lwr_reg->pfb_fbpa_training_patram               = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_PATRAM);
    lwr_reg->pfb_fbpa_training_ctrl                 = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CTRL);
    lwr_reg->pfb_fbpa_ltc_bandwidth_limiter         = REG_RD32(LOG, LW_PFB_FBPA_LTC_BANDWIDTH_LIMITER);

    //reading the fbpa register to make RMW         the fields that gets affected for new frequency
    lwr_reg->pfb_fbpa_generic_mrs                   = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS);
    lwr_reg->pfb_fbpa_generic_mrs1                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS1);
    lwr_reg->pfb_fbpa_generic_mrs2                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS2);
    lwr_reg->pfb_fbpa_generic_mrs3                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS3);
    lwr_reg->pfb_fbpa_generic_mrs4                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);
    lwr_reg->pfb_fbpa_generic_mrs5                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS5);
    lwr_reg->pfb_fbpa_generic_mrs6                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS6);
    lwr_reg->pfb_fbpa_generic_mrs7                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS7);
    lwr_reg->pfb_fbpa_generic_mrs8                  = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS8);
    lwr_reg->pfb_fbpa_generic_mrs10                 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS10);
    lwr_reg->pfb_fbpa_generic_mrs14                 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS14);
    lwr_reg->pfb_fbpa_timing1                       = REG_RD32(LOG, LW_PFB_FBPA_TIMING1);
    lwr_reg->pfb_fbpa_timing11                      = REG_RD32(LOG, LW_PFB_FBPA_TIMING11);
    lwr_reg->pfb_fbpa_timing12                      = REG_RD32(LOG, LW_PFB_FBPA_TIMING12);
    lwr_reg->pfb_fbpa_cfg0                          = REG_RD32(LOG, LW_PFB_FBPA_CFG0);
    lwr_reg->pfb_fbpa_fbio_broadcast                = REG_RD32(LOG, LW_PFB_FBPA_FBIO_BROADCAST);
    lwr_reg->pfb_fbpa_fbio_bg_vtt                   = REG_RD32(LOG, LW_PFB_FBPA_FBIO_BG_VTT);
    lwr_reg->pfb_fbpa_fbio_edc_tracking             = REG_RD32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING);
    lwr_reg->pfb_fbpa_refctrl                       = REG_RD32(LOG, LW_PFB_FBPA_REFCTRL);
    lwr_reg->pfb_fbpa_ptr_reset                     = REG_RD32(LOG, LW_PFB_FBPA_PTR_RESET);
    lwr_reg->pfb_fbpa_training_cmd0                 = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CMD0);
    lwr_reg->pfb_fbpa_training_cmd1                 = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CMD1);
    lwr_reg->pfb_fbpa_ref                           = REG_RD32(LOG, LW_PFB_FBPA_REF);
    lwr_reg->pfb_fbio_cal_wck_vml                   = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CAL_WCK_VML);
    lwr_reg->pfb_fbio_cal_clk_vml                   = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CAL_CLK_VML);
    lwr_reg->pfb_fbpa_refmpll_cfg                   = REG_RD32(LOG, LW_PFB_FBPA_REFMPLL_CFG);
    lwr_reg->pfb_fbpa_refmpll_cfg2                  = REG_RD32(LOG, LW_PFB_FBPA_REFMPLL_CFG2);
    lwr_reg->pfb_fbpa_refmpll_coeff                 = REG_RD32(LOG, LW_PFB_FBPA_REFMPLL_COEFF);
    lwr_reg->pfb_fbpa_refmpll_ssd0                  = REG_RD32(LOG, LW_PFB_FBPA_REFMPLL_SSD0);
    lwr_reg->pfb_fbpa_refmpll_ssd1                  = REG_RD32(LOG, LW_PFB_FBPA_REFMPLL_SSD1);
    lwr_reg->pfb_fbpa_fbio_refmpll_config           = REG_RD32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);
    lwr_reg->pfb_fbpa_drampll_cfg                   = REG_RD32(LOG, LW_PFB_FBPA_DRAMPLL_CFG);
    lwr_reg->pfb_fbpa_drampll_coeff                 = REG_RD32(LOG, LW_PFB_FBPA_DRAMPLL_COEFF);
    lwr_reg->pfb_fbpa_fbio_drampll_config           = REG_RD32(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG);
    lwr_reg->pfb_fbpa_fbio_cmos_clk                 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CMOS_CLK);
    lwr_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen       = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN);
    lwr_reg->pfb_fbpa_fbio_cfg_brick_vauxgen        = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN);
    lwr_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen  = REG_RD32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN);
    lwr_reg->pfb_fbpa_fbio_cmd_delay                = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CMD_DELAY);
    lwr_reg->pfb_fbpa_fbiotrng_subp0_wck            = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCK);
    lwr_reg->pfb_fbpa_fbiotrng_subp0_wckb           = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCKB);
    lwr_reg->pfb_fbpa_fbiotrng_subp1_wck            = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCK);
    lwr_reg->pfb_fbpa_fbiotrng_subp1_wckb           = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCKB);
    lwr_reg->pfb_fbpa_clk_ctrl                      = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CLK_CTRL);
    lwr_reg->pfb_fbpa_fbiotrng_subo0_cmd_brlshft1   = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1);
    lwr_reg->pfb_fbpa_nop                           = REG_RD32(LOG, LW_PFB_FBPA_NOP);
    lwr_reg->pfb_fbpa_fbio_edc_tracking_debug       = REG_RD32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG);
    lwr_reg->pfb_fbpa_fbio_delay                    = REG_RD32(LOG, LW_PFB_FBPA_FBIO_DELAY);
    lwr_reg->pfb_fbpa_testcmd                       = REG_RD32(LOG, LW_PFB_FBPA_TESTCMD);
    lwr_reg->pfb_fbpa_testcmd_ext                   = REG_RD32(LOG, LW_PFB_FBPA_TESTCMD_EXT);
    lwr_reg->pfb_fbpa_testcmd_ext_1                 = REG_RD32(LOG, LW_PFB_FBPA_TESTCMD_EXT_1);
    lwr_reg->pfb_fbpa_fbio_pwr_ctrl                 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_PWR_CTRL);
    lwr_reg->pfb_fbpa_fbio_brlshft                  = REG_RD32(LOG, LW_PFB_FBPA_FBIO_BRLSHFT);
    lwr_reg->pfb_fbpa_fbio_vref_tracking            = REG_RD32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING);
    lwr_reg->pfb_fbpa_training_edc_ctrl             = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_EDC_CTRL);
    lwr_reg->pfb_fbpa_fbio_vtt_ctrl1                = REG_RD32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL1);
    lwr_reg->pfb_fbpa_fbio_vtt_ctrl0                = REG_RD32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL0);
    lwr_reg->pfb_fbpa_training_rw_periodic_ctrl     = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_RW_PERIODIC_CTRL);
    lwr_reg->pfb_fbpa_training_ctrl2                = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CTRL2);
    lwr_reg->pfb_fbio_delay_broadcast_misc1         = REG_RD32(LOG, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1);
    lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
    lwr_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1);
    lwr_reg->pfb_fbio_calmaster_cfg                  = REG_RD32(LOG, LW_PFB_FBIO_CALMASTER_CFG);
    lwr_reg->pfb_fbio_calmaster_cfg2                 = REG_RD32(LOG, LW_PFB_FBIO_CALMASTER_CFG2);
    lwr_reg->pfb_fbio_calmaster_cfg3                 = REG_RD32(LOG, LW_PFB_FBIO_CALMASTER_CFG3);
    lwr_reg->pfb_fbio_calmaster_rb                   = REG_RD32(LOG, LW_PFB_FBIO_CALMASTER_RB);
    lwr_reg->pfb_fbio_cml_clk2                       = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CML_CLK2);

    gbl_prev_edc_tr_enabled = REF_VAL(LW_PFB_FBPA_FBIO_EDC_TRACKING_EN,lwr_reg->pfb_fbpa_fbio_edc_tracking);
    return;
}

void
gddr_calc_exp_freq_reg_values
(
        void
)
{
    LwU32 cl;
    LwU32 wl;
    LwU32 wr;
    LwU32 msb_cl;
    LwU32 msb_wr;
    LwU32 mrs_reg;
    LwU32 r2p;
    LwU32 spare_reg;
    LwU32 clamshell;
    LwU32 spare_reg1;
    LwU32 crccl;
    LwU32 crcwl;
    LwU32 EdcRate;
    LwU32 mrs3_wr_scaling;

    LwU16 mem_info_entry;
    gbl_ddr_mode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast);

    cl =  REF_VAL(LW_PFB_FBPA_CONFIG1_CL, gTT.perfMemTweakBaseEntry.Config1);
    wl =  REF_VAL(LW_PFB_FBPA_CONFIG1_WL, gTT.perfMemTweakBaseEntry.Config1);
    wr =  REF_VAL(LW_PFB_FBPA_CONFIG2_WR, gTT.perfMemTweakBaseEntry.Config2);

    LwBool bError = LW_FALSE;
    mrs3_wr_scaling = 0x0;

    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5)
    {
        if (cl >= 0x5)
        {
            cl = cl - 0x5;
        }
        else
        {
            bError = LW_TRUE;
        }
        //extended cl
        msb_cl = bit_extract(cl, 4, 1);
        cl     = bit_extract(cl, 0 ,4);

        if (wr >= 0x4)
        {
            wr = wr - 0x4;
        }
        else
        {
            bError = LW_TRUE;
        }
        //extended wr
        msb_wr =  bit_extract(wr, 4, 1);
        wr     =  bit_extract(wr, 0,4);
        new_reg->pfb_fbpa_config2  = gTT.perfMemTweakBaseEntry.Config2;
    } else { //g5x and g6
        if (cl >= 0x5)
        {
            cl = cl - 0x5;
        }
        else
        {
            bError = LW_TRUE;
        }
        //extended cl
        msb_cl = bit_extract(cl, 4, 1);
        cl     = bit_extract(cl, 0,4);

        //for gddr6 only 
        if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (wr > 0x23))
        { 
           mrs3_wr_scaling = 0x1;
           //ensuring the tWR is always even number after value 35, 
           //changing config2 and mrs registers to be always even
           if (wr %2 != 0)
           {
                 wr += 1;
           }
           new_reg->pfb_fbpa_config2 =
               FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG2, _WR, wr, gTT.perfMemTweakBaseEntry.Config2);
           wr /= 2;
        } else {	   
           mrs3_wr_scaling = 0x0;
               new_reg->pfb_fbpa_config2  = gTT.perfMemTweakBaseEntry.Config2;
        }

        if (wr >= 0x4)
        {
            wr = wr - 0x4;
        }
        else
        {
            bError = LW_TRUE;
        }
        //extended wr
        msb_wr =  bit_extract(wr, 4, 1);
        wr     =  bit_extract(wr, 0,4);
    }
    if (bError == LW_TRUE)
    {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_COMPUTATIONAL_RANGE_ERROR);
    }

    LwU32 AddrTerm0 = gTT.perfMemTweakBaseEntry.pmtbe375368.AddrCmdTerm0;
    LwU32 AddrTerm1 = gTT.perfMemTweakBaseEntry.pmtbe375368.AddrCmdTerm1;

    //MRS
    mrs_reg = lwr_reg->pfb_fbpa_generic_mrs;
    mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS, _ADR_GDDR5_WL,wl,mrs_reg);
    mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS,_ADR_GDDR5_CL,cl,mrs_reg);
    mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS,_ADR_GDDR5_WR,wr,mrs_reg);

    new_reg->pfb_fbpa_generic_mrs = mrs_reg;

    clamshell = REF_VAL(LW_PFB_FBPA_CFG0_CLAMSHELL, lwr_reg->pfb_fbpa_cfg0);
    //MRS8
    mrs_reg = 0;
    mrs_reg = lwr_reg->pfb_fbpa_generic_mrs8;
    if (gbl_ddr_mode !=  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8, _ADR_GDDR5_CL_EHF,msb_cl,mrs_reg);
        mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8, _ADR_GDDR5_WR_EHF,msb_wr,mrs_reg);
    } else {
        mrs_reg = bit_assign(mrs_reg,8,msb_cl,1);
        mrs_reg = bit_assign(mrs_reg,9,msb_wr,1);
    }

    mem_info_entry = TABLE_VAL(mInfoEntryTable->mie1500);
    MemInfoEntry1500*  my_mem_info_entry = (MemInfoEntry1500 *) (&mem_info_entry);

    gbl_vendor_id = bit_extract(mem_info_entry,12,4);
    gbl_index = bit_extract(mem_info_entry,8,4);
    gbl_strap = bit_extract(mem_info_entry,4,4);
    gbl_type = bit_extract(mem_info_entry,0,4);

    LwU8 AddrTerm;
    if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {

        if (clamshell == LW_PFB_FBPA_CFG0_CLAMSHELL_X32) {
            AddrTerm = AddrTerm0;
        } else {
            AddrTerm = AddrTerm1;
        }

        if ((AddrTerm != 0) && (AddrTerm != 3)) {
            mrs_reg = bit_assign(mrs_reg,0,AddrTerm,2);
            //CAH termination must be one higher than the CAL terminnation.
            mrs_reg = bit_assign(mrs_reg,2,AddrTerm+1,2);
        } else {
            mrs_reg = bit_assign(mrs_reg,0,AddrTerm,2);
            mrs_reg = bit_assign(mrs_reg,2,AddrTerm,2);
        }
        if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
            mrs_reg = bit_assign(mrs_reg,4,1,1);
        }

    } else {
        if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) {
            mrs_reg = bit_assign(mrs_reg,5,gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5PllRange,1); //PLL Range MSB
        }
    }
    new_reg->pfb_fbpa_generic_mrs8 = mrs_reg;

    //FOLLOWUP Need to update VBIOS
    if ((my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_SAMSUNG) ||
            (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_HYNIX)){
        lwr_reg->pfb_fbpa_training_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL,_WCK_PADS,_EIGHT,lwr_reg->pfb_fbpa_training_ctrl  );
        REG_WR32(BAR0, LW_PFB_FBPA_TRAINING_CTRL,lwr_reg->pfb_fbpa_training_ctrl );
    }


    //Programming MRS1
    mrs_reg = lwr_reg->pfb_fbpa_generic_mrs1;
    mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS1,_ADR_GDDR5_DATA_TERM,gTT.perfMemTweakBaseEntry.pmtbe375368.DataTerm,mrs_reg);

    if (!(gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
        if (clamshell == LW_PFB_FBPA_CFG0_CLAMSHELL_X32) {
            mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS1,_ADR_GDDR5_ADDR_TERM,AddrTerm0,mrs_reg);
        } else {
            mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS1,_ADR_GDDR5_ADDR_TERM,AddrTerm1,mrs_reg);
        }
    } else {
        if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) {
          //bug 2428434, 2453990 : making pll range 2 bits, repurposing unused PMC11SEF5VDDP and PllRange for PLL range		
          //https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Memory_Clock_Table/1.1_Spec#Memory_Clock_Table_Strap_Entry
          //vbios flags5[5:3] = {PMC11SEF5VDDP[1:0],QdrSpeed(pllrangemsb)}
          //VBIOS Flags5[5:3]   Current label        Programmed MRS 1   Proposed label
          //---------------------------------------------------------------------------
          //0x0                 Low Speed QDR        0x0                PLL Range 0
          //0x1                 High Speed QDR       0x2                PLL Range 2
          //0x2                 N/A                  0x1                PLL Range 1
          //0x3                 Middle Speed QDR     0x3                PLL Range 3
          LwU8 G6PllRange =0;
          G6PllRange = G6PllRange | (gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5VDDP & 0x3);
          G6PllRange = G6PllRange << 1;
          G6PllRange = G6PllRange | (gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5PllRange & 0x1);
          if (G6PllRange == 0x2) {
            mrs_reg = bit_assign(mrs_reg,4,1,2);
          } else if (G6PllRange == 0x1) {
            mrs_reg = bit_assign(mrs_reg,4,2,2);
          } else {
            mrs_reg = bit_assign(mrs_reg,4,G6PllRange,2); //PLL Range MSB
          }		      
        }
        //if (gbl_target_freqMHz > 4900) {
        //  mrs_reg = bit_assign(mrs_reg,5,1,1);
        //}
    }
    mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_DRIVE_STRENGTH,gTT.perfMemTweakBaseEntry.pmtbe383376.DrvStrength,mrs_reg);

    new_reg->pfb_fbpa_generic_mrs1 = mrs_reg  ;

    //Programming MRS2
    new_reg->pfb_fbpa_generic_mrs2 = lwr_reg->pfb_fbpa_generic_mrs2;
    //Programming MRS3
    LwU32 tCCDL;
    tCCDL = REF_VAL(LW_PFB_FBPA_CONFIG3_CCDL,gTT.perfMemTweakBaseEntry.Config3);
    new_reg->pfb_fbpa_generic_mrs3 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_BANK_GROUPS,tCCDL,lwr_reg->pfb_fbpa_generic_mrs3);
    if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
      new_reg->pfb_fbpa_generic_mrs3 = bit_assign(new_reg->pfb_fbpa_generic_mrs3,8,mrs3_wr_scaling,2);
    }
    //Programming MRS10
    new_reg->pfb_fbpa_generic_mrs10 = lwr_reg->pfb_fbpa_generic_mrs10;

    //timing1
    r2p = lwr_reg->pfb_fbpa_timing1;
    r2p = FLD_SET_DRF_NUM(_PFB,_FBPA_TIMING1,_R2P,gTT.perfMemTweakBaseEntry.pmtbe391384.Timing1_R2P,r2p);

    new_reg->pfb_fbpa_timing1 = r2p;



    //timing11 & timing12
    spare_reg = lwr_reg->pfb_fbpa_timing12;
    spare_reg1 = lwr_reg->pfb_fbpa_timing11;
    spare_reg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_TIMING11,_KO,gTT.perfMemTweakBaseEntry.pmtbe367352.Timing11_KO,spare_reg1);
    spare_reg  = FLD_SET_DRF_NUM(_PFB, _FBPA_TIMING12,_CKE,gTT.perfMemTweakBaseEntry.pmtbe367352.Timing12_CKE,spare_reg);
    spare_reg  = FLD_SET_DRF_NUM(_PFB, _FBPA_TIMING12,_RDCRC,gTT.perfMemTweakBaseEntry.pmtbe415408.RDCRC,spare_reg);
    //During simulation with gp10x vbios RDCRC read from vbios was 0
    //TODO check with tu102 vbios
    spare_reg  = FLD_SET_DRF_NUM(_PFB, _FBPA_TIMING12,_RDCRC,0x2,spare_reg);

    new_reg->pfb_fbpa_timing12 = spare_reg;
    new_reg->pfb_fbpa_timing12 = FLD_SET_DRF(_PFB, _FBPA_TIMING12,_LOCKPLL, _3000,new_reg->pfb_fbpa_timing12);
    new_reg->pfb_fbpa_timing11 = spare_reg1;
    new_reg->pfb_fbpa_config0  = gTT.perfMemTweakBaseEntry.Config0;
    new_reg->pfb_fbpa_config1  = gTT.perfMemTweakBaseEntry.Config1;
    new_reg->pfb_fbpa_timing10 = gTT.perfMemTweakBaseEntry.Timing10;

    //FB UNIT SIM ONLY
    //gbl_g5x_cmd_mode = tb_sig_g5x_cmd_mode;


    //Not used in Gddr
    ////MemoryDLL

    //VDDQ

    //     MicronGDDR5MRS14Offset.PMC11SEG5xMRS14Voltage(PerfMemClk11StrapEntryFlags1* ) (&pmct_table_strap_entry0);
    new_reg->gpio_fbvddq = gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1FBVDDQ;

    //FBIO Broadcast - g5x command mode
    if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) {
        gbl_g5x_cmd_mode = gTT.perfMemClkBaseEntry.Flags1.PMC11EF1G5XCmdMode;
    } else  {
        gbl_g5x_cmd_mode = 0;
    }

    //FB UNIT SIM ONLY
    //gbl_g5x_cmd_mode = tb_sig_g5x_cmd_mode;
    new_reg->pfb_fbpa_fbio_broadcast = lwr_reg->pfb_fbpa_fbio_broadcast;
    new_reg->pfb_fbpa_fbio_broadcast = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BROADCAST,_G5X_CMD_MODE,gbl_g5x_cmd_mode,new_reg->pfb_fbpa_fbio_broadcast);


    //gbl_vendor_id = my_mem_info_entry->MIEVendorID;

    if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) {
        if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            new_reg->pfb_fbpa_generic_mrs8 = bit_assign(new_reg->pfb_fbpa_generic_mrs8,9,gbl_g5x_cmd_mode,1); //A9 operation mode - QDR
        } else {
            new_reg->pfb_fbpa_generic_mrs10 = bit_assign(new_reg->pfb_fbpa_generic_mrs10,9,gbl_g5x_cmd_mode,1);//reusing g5x_cmd_mode to g6 qdr mode
            new_reg->pfb_fbpa_generic_mrs10 = bit_assign(new_reg->pfb_fbpa_generic_mrs10,20,0xA,6);
            //new_reg->pfb_fbpa_generic_mrs10 = bit_assign(new_reg->pfb_fbpa_generic_mrs10,0,0xC,4);
        }
    }
    //new_reg->pfb_fbpa_generic_mrs10 = bit_assign(new_reg->pfb_fbpa_generic_mrs10,9,1,1);//reusing g5x_cmd_mode to g6 qdr mode
    //new_reg->pfb_fbpa_generic_mrs10 = bit_assign(new_reg->pfb_fbpa_generic_mrs10,20,0xA,6);

    //FBIO_BG_VTT - CDBVAUXC
    new_reg->pfb_fbpa_fbio_bg_vtt =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BG_VTT, _LV_CDBVAUXC,gTT.perfMemTweakBaseEntry.pmtbe407400.CDBVauxc,new_reg->pfb_fbpa_fbio_bg_vtt);

    //Setting vauxc registers from target vauxc level found in gddr_check_vaux_change function
    if (gbl_bVauxChangeNeeded) {
        //VAUXC
        //cdb
        new_reg->pfb_fbpa_fbio_vtt_ctrl1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CTRL1, _CDB_LV_VAUXC,  gbl_target_vauxc_level, lwr_reg->pfb_fbpa_fbio_vtt_ctrl1);
        //byte (Dq)
        new_reg->pfb_fbpa_fbio_vtt_ctrl0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CTRL0, _BYTE_LV_VAUXC,  gbl_target_vauxc_level, lwr_reg->pfb_fbpa_fbio_vtt_ctrl0);
        //cmd
        new_reg->pfb_fbpa_fbio_vtt_ctrl0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CTRL0, _CMD_LV_VAUXC,   gbl_target_vauxc_level, new_reg->pfb_fbpa_fbio_vtt_ctrl0);

        //VCLAMP - BYTE (dq)
        new_reg->pfb_fbpa_fbio_cfg_brick_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_BRICK_VAUXGEN, _REG_LVCLAMP, gbl_target_vauxc_level,lwr_reg->pfb_fbpa_fbio_cfg_brick_vauxgen);
        //VAUXP - BYTE (dq)
        new_reg->pfb_fbpa_fbio_cfg_brick_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_BRICK_VAUXGEN, _REG_LVAUXP, gbl_target_vauxc_level,new_reg->pfb_fbpa_fbio_cfg_brick_vauxgen);

        //VCLAMP - cmd
        new_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_VTTGEN_VAUXGEN, _REG_VCLAMP_LVL, gbl_target_vauxc_level, lwr_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen);
        //VAUXP - cmd
        new_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_VTTGEN_VAUXGEN, _REG_VAUXP_LVL,  gbl_target_vauxc_level, new_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen);


        //Comp Vauxc
        new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBIO_CALGROUP_VTTGEN_VAUXGEN, _VTT_LV_VAUXC, gbl_target_vauxc_level,lwr_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
        new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBIO_CALGROUP_VTTGEN_VAUXGEN, _VTT_LV_VAUXP, gbl_target_vauxc_level,new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
        new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBIO_CALGROUP_VTTGEN_VAUXGEN, _VTT_LV_VCLAMP, gbl_target_vauxc_level,new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
        FW_MBOX_WR32(10, new_reg->pfb_fbpa_fbio_vtt_ctrl1);
        FW_MBOX_WR32(11, new_reg->pfb_fbpa_fbio_vtt_ctrl0);
        FW_MBOX_WR32(12, new_reg->pfb_fbpa_fbio_cfg_brick_vauxgen);
        FW_MBOX_WR32(13, new_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen);
        FW_MBOX_WR32(14, new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
    }

    //fbio_cfg12
    spare_reg = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG12);
    GblRxDfe = gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4RxDfe;
    GblEnMclkAddrTraining = gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4AddrTraining;

    //using vbios spare_field6[0] to determine if we need to enable wck pin mode
    LwU8 spare_field6 = gTT.perfMemClkBaseEntry.spare_field6;
    if (spare_field6 & 0x1)  GblEnWckPinModeTr = LW_TRUE;

    //to indicate MSCG to perform wck pin mode training for P5
    //Spare_Bit[30] is used for MSCG handshake
    LwU32 fbpa_spare;
    LwU16 idx;
    for (idx = 0; idx < 6; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            fbpa_spare = REG_RD32(LOG, unicast(LW_PFB_FBPA_SPARE,idx));
            if ((GblEnWckPinModeTr == LW_TRUE) && (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) &&
		(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
              fbpa_spare = bit_assign(fbpa_spare,30,1,1);
            } else {
              fbpa_spare = bit_assign(fbpa_spare,30,0,1);
            }
            REG_WR32_STALL(LOG, unicast(LW_PFB_FBPA_SPARE,idx), fbpa_spare);
        }
    }

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT))
    //Checking if boot time training table was done and if address training was enabled there
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        BOOT_TRAINING_FLAGS *pTrainingFlags = &gTT.bootTrainingTable.MemBootTrainTblFlags;
        GblBootTimeAdrTrng  = pTrainingFlags->flag_g6_addr_tr_en;
        GblSkipBootTimeTrng = pTrainingFlags->flag_skip_boot_training;

        if ((GblSkipBootTimeTrng == 0) && (GblBootTimeAdrTrng == 1)){
            enableEvent(EVENT_SAVE_TRAINING_DATA);
            GblAddrTrPerformedOnce = LW_TRUE;
        }
    }
#endif   // FBFALCON_JOB_TRAINING_DATA_SUPPORT

    spare_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG12,_TX_EN_EARLY,gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4TXEnEarly,spare_reg);
    EdcRate = 0;
    if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        EdcRate = REF_VAL(LW_PFB_FBPA_CFG0_EDC_RATE, lwr_reg->pfb_fbpa_cfg0);
    }


    //addition apart from doc
    if ((gbl_g5x_cmd_mode && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6))  || ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (EdcRate == LW_PFB_FBPA_CFG0_EDC_RATE_HALF))) {
        spare_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG12, _DOUBLE_EDC_INTERP_OFFSET, 1, spare_reg);
    } else {
        spare_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG12, _DOUBLE_EDC_INTERP_OFFSET, 0, spare_reg);
    }
    new_reg->pfb_fbpa_fbio_cfg12 = spare_reg;

    //programming vrefd offset for gddr5
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
      LwU8 VrefdOffset = gTT.perfMemClkStrapEntry.VREFDOffsets;
      new_reg->pfb_fbpa_generic_mrs6 = lwr_reg->pfb_fbpa_generic_mrs6;
      new_reg->pfb_fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR5_VREFD_OFFSET_BYTE23, (VrefdOffset & 0xF),new_reg->pfb_fbpa_generic_mrs6);
      new_reg->pfb_fbpa_generic_mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR5_VREFD_OFFSET_BYTE01, ((VrefdOffset >> 4) & 0xF),new_reg->pfb_fbpa_generic_mrs6);
    }

    new_reg->pfb_fbpa_generic_mrs7 = lwr_reg->pfb_fbpa_generic_mrs7;

    //tRAS for Memory -Asatish addition apart from doc
    // Bug 2343349: tRas is only part of the Micron spec and should not be added for any other drams

    new_reg->pfb_fbpa_generic_mrs5 = lwr_reg->pfb_fbpa_generic_mrs5;
    if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON)
    {
        LwU32 tRAS;
        tRAS = REF_VAL(LW_PFB_FBPA_CONFIG0_RAS,new_reg->pfb_fbpa_config0);
        new_reg->pfb_fbpa_generic_mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR5_RAS,tRAS,lwr_reg->pfb_fbpa_generic_mrs5);
    }


    //updating CRCWL and CRCCL values - addition apart from doc
    crcwl = REF_VAL(LW_PFB_FBPA_CONFIG5_WRCRC,gTT.perfMemTweakBaseEntry.Config5);
    crccl = REF_VAL(LW_PFB_FBPA_TIMING12_RDCRC,new_reg->pfb_fbpa_timing12);
    crcwl = crcwl - 7;
    new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4,_ADR_GDDR5_CRCWL,crcwl,lwr_reg->pfb_fbpa_generic_mrs4);
    new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4,_ADR_GDDR5_CRCCL,crccl,new_reg->pfb_fbpa_generic_mrs4);

    gblEnEdcTracking  =  gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6EdcTrackEn;
    gblElwrefTracking =  gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6VrefTrackEn;

    gblEdcTrGain = gTT.perfMemClkStrapEntry.EdcTrackingGain;
    gblVrefTrGain = gTT.perfMemClkStrapEntry.VrefTrackingGain;
    return;
}

void
gddr_power_on_internal_vref
(
        void
)
{
    LwU32 fbpa_fbio_cfg10;
    //, pmct_table_strap_entry_flag2;
    //pmct_table_strap_entry_flag2 = TABLE_VAL(pMemClkStrapTable->Flags2);
    //gbl_target_pmct_flags2 = &selectedMemClkStrapTable.Flags2;


    if (gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2DQWDQSRcvSel != 0) { 
        fbpa_fbio_cfg10 = lwr_reg->pfb_fbpa_fbio_cfg10;
        fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _E_INTERNAL_VREF,_ENABLED,fbpa_fbio_cfg10);
        lwr_reg->pfb_fbpa_fbio_cfg10 = fbpa_fbio_cfg10;
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG10,fbpa_fbio_cfg10);
    }

    return;
}

void
gddr_disable_acpd_and_replay
(
        void
)
{
    LwU32 fbpa_cfg0;

    //disable acpd
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD,_NO_POWERDOWN,lwr_reg->pfb_fbpa_cfg0);
    //disable replay
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _REPLAY,_DISABLED,fbpa_cfg0);
    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}

void
gddr_disable_refresh
(
        void
)
{
    LwU32 fbpa_refctrl;
    fbpa_refctrl = lwr_reg->pfb_fbpa_refctrl;
    fbpa_refctrl = FLD_SET_DRF(_PFB, _FBPA_REFCTRL,_VALID,_0,fbpa_refctrl);
    lwr_reg->pfb_fbpa_refctrl = fbpa_refctrl;
    REG_WR32(LOG, LW_PFB_FBPA_REFCTRL,fbpa_refctrl);
    return;
}

void
gddr_disable_periodic_training
(
        void
)
{
    LwU32 lwr_clk_source;
    LwU32 training_cmd;

    lwr_clk_source = REF_VAL(LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE, gddr_lwrr_clk_source);
    if ((lwr_clk_source == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) ||
            (lwr_clk_source == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL)) {
        //TODO:must do on all partition
        training_cmd = lwr_reg->pfb_fbpa_training_cmd0;
        training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _DISABLED ,training_cmd);
        lwr_reg->pfb_fbpa_training_cmd0 = training_cmd;
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD0,training_cmd);
        training_cmd = lwr_reg->pfb_fbpa_training_cmd1;
        training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _DISABLED ,training_cmd);
        lwr_reg->pfb_fbpa_training_cmd1 = training_cmd;
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,training_cmd);

        //disabling periodic training average
        lwr_reg->pfb_fbpa_training_rw_periodic_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_RW_PERIODIC_CTRL,_EN_AVG, _INIT, lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_PERIODIC_CTRL,lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);

        //disabling periodic shift training
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_WR, _DIS, lwr_reg->pfb_fbpa_training_ctrl2);
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_RD, _DIS, lwr_reg->pfb_fbpa_training_ctrl2);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2,lwr_reg->pfb_fbpa_training_ctrl2);

    }

    return;
}

void
gddr_send_one_refresh
(
        void
)
{
    LwU32 fbpa_ref;

    fbpa_ref = lwr_reg->pfb_fbpa_ref;
    fbpa_ref = FLD_SET_DRF(_PFB, _FBPA_REF, _CMD,_REFRESH_1,fbpa_ref);
    REG_WR32(LOG, LW_PFB_FBPA_REF,fbpa_ref);

    //Adding more SW refresh to account for loss of refresh between periodic ref and self ref in slow ltcclk - bug #1706386
    fbpa_ref = FLD_SET_DRF(_PFB, _FBPA_REF, _CMD,_REFRESH_1,fbpa_ref);
    REG_WR32(LOG, LW_PFB_FBPA_REF,fbpa_ref);

    fbpa_ref = FLD_SET_DRF(_PFB, _FBPA_REF, _CMD,_REFRESH_1,fbpa_ref);
    lwr_reg->pfb_fbpa_ref = fbpa_ref;
    REG_WR32(LOG, LW_PFB_FBPA_REF,fbpa_ref);
    return;
}

void
gddr_set_memvref_range
(
        void
)
{
    LwU32 fbpa_mrs7;
    LwU32 mem_info_entry;


    fbpa_mrs7 = new_reg->pfb_fbpa_generic_mrs7;
    //If target freq is less than 1GHz then LF_Mode in MRS7 is set
    if (gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0LowFreq == 1 ){
        if (!(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
            fbpa_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_LOW_FREQ_MODE,_ENABLED,fbpa_mrs7);
        } else {
            fbpa_mrs7 = bit_assign(fbpa_mrs7,3,1,1);
        }
    } else {
        if (!(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
            fbpa_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_LOW_FREQ_MODE,_DISABLED,fbpa_mrs7);
        } else {
            fbpa_mrs7 = bit_assign(fbpa_mrs7,3,0,1);
        }
    }
    //programming Internal Vrefc
    //Asatish FOLLOWUP need to program twice - based on speed
    LwU8 InternalVrefc;
    InternalVrefc = gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5G5XVref;
    if (InternalVrefc == 0) { //disabled  70%
        if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
            (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X)) {
            fbpa_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_TEMP_SENSE, _DISABLED,fbpa_mrs7);
        }
    }

    //Need to program VDD_RANGE before SRE bug :
    LwU8 Mrs7VddRange = gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4MRS7VDD;
    if (Mrs7VddRange == 0) { //Enable
        fbpa_mrs7 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_VDD_RANGE, 1, fbpa_mrs7);
    } else { //Disable
        fbpa_mrs7 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_VDD_RANGE, 0, fbpa_mrs7);
    }
    new_reg->pfb_fbpa_generic_mrs7 = fbpa_mrs7;
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS7,fbpa_mrs7);


    //if dram is from micron and is G5x mode change the MRS14 CTLE
    mem_info_entry = TABLE_VAL(mInfoEntryTable->mie1500);
    MemInfoEntry1500*  my_mem_info_entry = (MemInfoEntry1500 *) (&mem_info_entry);
    if ((my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) &&
            ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)))
    {
        LwU32 MicronCoreVoltage;
        LwU32 MicronSubRegisterSelect;
        //MicronCoreVoltage = pMemClkStrapTable->MicronGDDR5MRS14Offset;
        //MicronCoreVoltage = selectedMemClkStrapTable.MicronGDDR5MRS14Offset.PMC11SEG5xMRS14Voltage;

	MicronSubRegisterSelect = ((gTT.perfMemClkStrapEntry.MicronGDDR5MRS14Offset.PMC11SEG5xMRS14SubRegSel << 2 ) |
					                   (gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6MRS14SubRegSelect));
        MicronCoreVoltage = gTT.perfMemClkStrapEntry.MicronGDDR5MRS14Offset.PMC11SEG5xMRS14Voltage;
        new_reg->pfb_fbpa_generic_mrs14 = bit_assign(lwr_reg->pfb_fbpa_generic_mrs14,0,MicronCoreVoltage,5);
        new_reg->pfb_fbpa_generic_mrs14 = bit_assign(new_reg->pfb_fbpa_generic_mrs14,9,MicronSubRegisterSelect,3);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS14,new_reg->pfb_fbpa_generic_mrs14);
    }

    return;

}




void
gddr_update_gpio_trigger_wait_done
(
        void
)
{
    LwU32 GpioTrigger;
    LwU32 trig_update;
    //update trigger
    GpioTrigger = REG_RD32(LOG, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER);
    GpioTrigger = FLD_SET_DRF(_PMGR,_GPIO_OUTPUT_CNTL_TRIGGER, _UPDATE, _TRIGGER, GpioTrigger);
    REG_WR32(LOG, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER,GpioTrigger);

    GpioTrigger = 0;

    do {

        GpioTrigger = REG_RD32(LOG, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER);
        trig_update = REF_VAL(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE,GpioTrigger);
        FW_MBOX_WR32(10, trig_update);
        FW_MBOX_WR32(11, GpioTrigger);

    }while (trig_update == LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_PENDING) ;

    return;
}

void
gddr_set_fbvref
(
        void
)
{
    // FOLLOWUP: gpio setup is wrong and work in progress (functionality needs to be added in stand_sim as well)
    //        pending items: Avinash - update sequence and standsim support
    //                       Stefan - add mutex protection
    //        disable code for now

    LwU32 gpio_vref_output_cntl;

    LwU8 vrefGPIOInx;
    vrefGPIOInx = memoryGetGpioIndexForVref_HAL();

    //obtain the mutex for accessing GPIO register
    LwU8 token = 0;

    // Issue a FENCE to make sure all prior FB priv writes have been exelwted, 
    // before issuing a priv write to a different client
    REG_WR32(LOG,LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1000);
    OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3322161 - PRI_FENCE ordering corner case timing hole

    MUTEX_HANDLER fbvrefgpioMutex;
    fbvrefgpioMutex.registerMutex = LW_PPWR_PMU_MUTEX(12);
    fbvrefgpioMutex.registerIdRelease = LW_PPWR_PMU_MUTEX_ID_RELEASE    ;
    fbvrefgpioMutex.registerIdAcquire = LW_PPWR_PMU_MUTEX_ID;
    fbvrefgpioMutex.valueInitialLock = LW_PPWR_PMU_MUTEX_VALUE_INITIAL_LOCK;
    fbvrefgpioMutex.valueAcquireInit = LW_PPWR_PMU_MUTEX_ID_VALUE_INIT;
    fbvrefgpioMutex.valueAcquireNotAvailable = LW_PPWR_PMU_MUTEX_ID_VALUE_NOT_AVAIL;
    LW_STATUS status = mutexAcquireByIndex_GV100(&fbvrefgpioMutex, MUTEX_REQ_TIMEOUT_NS, &token);

    if(status != LW_OK) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_MUTEX_NOT_OBTAINED);
    }

    gpio_vref_output_cntl = REG_RD32(LOG,LW_PMGR_GPIO_OUTPUT_CNTL(vrefGPIOInx));

    LwU8 lwr_fbvref = gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1FBVREF;

    if(lwr_fbvref == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVREF_LOW)
    {
        gpio_vref_output_cntl = FLD_SET_DRF(_PMGR,_GPIO_OUTPUT_CNTL, _IO_OUTPUT,_1, gpio_vref_output_cntl);
    } 
    else
    {
        gpio_vref_output_cntl = FLD_SET_DRF(_PMGR,_GPIO_OUTPUT_CNTL, _IO_OUTPUT,_0, gpio_vref_output_cntl);
    }
    gpio_vref_output_cntl = FLD_SET_DRF(_PMGR, _GPIO_OUTPUT_CNTL, _IO_OUT_EN, _YES,gpio_vref_output_cntl);
    
    REG_WR32(LOG,LW_PMGR_GPIO_OUTPUT_CNTL(vrefGPIOInx),gpio_vref_output_cntl);

    //update trigger and wait for trigger done
    gddr_update_gpio_trigger_wait_done();

    //release the mutex
    status = mutexReleaseByIndex_GV100(&fbvrefgpioMutex, token);
    if(status != LW_OK) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_GPIO_MUTEX_NOT_RELEASED);
    }

    //settle time 20 us, as originally followed by RM code.
    OS_PTIMER_SPIN_WAIT_US(20);

    return;
}

void
gddr_precharge_all
(
        void
)
{

    LwU32 dram_ack;

    // Enable DRAM_ACK
    dram_ack = lwr_reg->pfb_fbpa_cfg0;
    dram_ack = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED, dram_ack);

    lwr_reg->pfb_fbpa_cfg0 = dram_ack;
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack);

    // Force a precharge
    REG_WR32_STALL(LOG, LW_PFB_FBPA_PRE, 0x00000001);

    // Disable DRAM_ACK
    dram_ack = lwr_reg->pfb_fbpa_cfg0;
    dram_ack = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED, dram_ack);

    lwr_reg->pfb_fbpa_cfg0 = dram_ack;
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack);

    return;
}

void
gddr_enable_self_refresh
(
        void
)
{
    LwU32 testcmd_ext;
    LwU32 testcmd = 0;
    LwU32 testcmd_ext_1;
    //
    //First cycle of SREF -- Issue SRE command with CKE_n = 1
    //G6 uses TESTCMD_EXT for CA9_Rise
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        testcmd_ext = lwr_reg->pfb_fbpa_testcmd_ext;
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT, _ACT,_DDR6_SELF_REF_ENTRY1,testcmd_ext);
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM0,      _OFF,                   testcmd_ext);
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM1,      _OFF,                   testcmd_ext);
        testcmd_ext = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD_EXT,  _ADR_DRAM1,  0,                      testcmd_ext);
        lwr_reg->pfb_fbpa_testcmd_ext = testcmd_ext;
        REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT,testcmd_ext);
    }

    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,_DDR5_SELF_REF_ENTRY1,testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,_DDR5_SELF_REF_ENTRY1,testcmd);
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,_DDR6_SELF_REF_ENTRY1,testcmd);//value 1
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,_DDR6_SELF_REF_ENTRY1,testcmd);//value 0
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,_DDR6_SELF_REF_ENTRY1,testcmd);//value 0
    } else {
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS, _DDR5_SELF_REF_ENTRY1, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS, _DDR5_SELF_REF_ENTRY1, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE , _DDR5_SELF_REF_ENTRY1, testcmd);
    }

    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0, _DDR5_SELF_REF_ENTRY1,testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1, _DDR5_SELF_REF_ENTRY1,testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD,_DDR5_SELF_REF_ENTRY1,testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,  _DDR5_SELF_REF_ENTRY1,testcmd);

    lwr_reg->pfb_fbpa_testcmd = testcmd;
    REG_WR32(LOG, LW_PFB_FBPA_TESTCMD,testcmd);

    //========================================================
    //Second cycle of SREF - Issue a NOP with CKE_n=1 and HOLD
    //G6 uses TESTCMD_EXT for CA9_Rise

    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        testcmd_ext_1 = lwr_reg->pfb_fbpa_testcmd_ext_1;
        testcmd_ext_1 = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB0, 1, testcmd_ext_1);
        testcmd_ext_1 = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB1, 1, testcmd_ext_1);
        REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT_1,testcmd_ext_1);

        testcmd_ext = lwr_reg->pfb_fbpa_testcmd_ext;
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ACT,            _DDR6_SELF_REF_ENTRY2, testcmd_ext);
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM0,      _ON,                   testcmd_ext);
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM1,      _ON,                   testcmd_ext);
        testcmd_ext = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD_EXT,  _ADR_DRAM1,      255,                   testcmd_ext);
        
        lwr_reg->pfb_fbpa_testcmd_ext = testcmd_ext;
        REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT,testcmd_ext);
    }

    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES, _DDR5_SELF_REF_ENTRY2,testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE, _DDR5_SELF_REF_ENTRY2,testcmd);
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0 , _DDR5_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1 , _DDR5_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,  _DDR6_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,  _DDR6_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE ,  _DDR6_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD, _BANK, 15,                   testcmd);
        testcmd = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD, _ADR , 255,                  testcmd);
    } else {
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,_DDR5_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,_DDR5_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE ,_DDR5_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0 , _DDR5_SELF_REF_ENTRY2, testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1 , _DDR5_SELF_REF_ENTRY2, testcmd);
    }

    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY2, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO  , _DDR5_SELF_REF_ENTRY2, testcmd);

    lwr_reg->pfb_fbpa_testcmd = testcmd;
    REG_WR32(LOG, LW_PFB_FBPA_TESTCMD,testcmd);
    return;
}

void
gddr_clear_wck_trimmer
(
        void
)
{
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0_WCK_DDLL,0x00000000);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1_WCK_DDLL,0x00000000);
    return;
}

void
gddr_set_vml_mode
(
        void
)
{
    LwU32 clk_vml;
    LwU32 wck_vml;

    wck_vml = lwr_reg->pfb_fbio_cal_wck_vml;
    wck_vml = FLD_SET_DRF(_PFB, _FBPA_FBIO_CAL_WCK_VML, _SEL, _VML, wck_vml);
    lwr_reg->pfb_fbio_cal_wck_vml = wck_vml;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CAL_WCK_VML,wck_vml);

    clk_vml = lwr_reg->pfb_fbio_cal_clk_vml;
    clk_vml = FLD_SET_DRF(_PFB, _FBPA_FBIO_CAL_CLK_VML, _SEL, _VML, clk_vml);
    lwr_reg->pfb_fbio_cal_clk_vml = clk_vml;
    REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CAL_CLK_VML,clk_vml);


    return;
}

void
gddr_power_intrpl_dll
(
        void
)
{
    LwU32 fbio_cfg_pwrd;
    LwU32 fbpa_refmpll_cfg;
    LwU32 ctrl_wck_pads;


    fbio_cfg_pwrd = lwr_reg->pfb_fbpa_fbio_cfg_pwrd;
    ctrl_wck_pads = REF_VAL(LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS,lwr_reg->pfb_fbpa_training_ctrl);

    if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) {
        if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
            //power down DRAMPLL - Interpolator
            fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _INTERP_PWRD, _ENABLE,fbio_cfg_pwrd);
            //power down high speed mode receiver - NEW
            fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_GDDR5_PWRD, _ENABLE, fbio_cfg_pwrd);

            if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) || (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) {
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_TX_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQDQS_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CK_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                if (ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
                    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                }
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_DIFFAMP_PWRD,_DISABLE,fbio_cfg_pwrd);
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DIFFAMP_PWRD,_DISABLE,fbio_cfg_pwrd);

                fbpa_refmpll_cfg = lwr_reg->pfb_fbpa_fbio_refmpll_config;
                fbpa_refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _IDDQ , _POWER_ON,fbpa_refmpll_cfg);
                lwr_reg->pfb_fbpa_fbio_refmpll_config = fbpa_refmpll_cfg;
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,fbpa_refmpll_cfg);
            }
            fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CLK_TX_PWRD,_ENABLE,fbio_cfg_pwrd);
            fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_TX_PWRD,_INIT,fbio_cfg_pwrd);
            if (ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_TX_PWRD,_INIT,fbio_cfg_pwrd);
            }
        }
    } else if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) {
        //if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) { // Change from document - cannot power down refmpll if target clk path is drampll
        //    //power down REFMPLL - DLL
        //    fbpa_refmpll_cfg = lwr_reg->pfb_fbpa_fbio_refmpll_config;
        //    fbpa_refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _IDDQ, _POWER_OFF,fbpa_refmpll_cfg);
        //    lwr_reg->pfb_fbpa_fbio_refmpll_config = fbpa_refmpll_cfg;
        //    REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,fbpa_refmpll_cfg);

        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_TX_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQDQS_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CK_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_DIFFAMP_PWRD,_ENABLE,fbio_cfg_pwrd);

        //}
    }

    lwr_reg->pfb_fbpa_fbio_cfg_pwrd = fbio_cfg_pwrd;
    REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);
    return;
}

void
gddr_pgm_refmpll_cfg_and_lock_wait
(
        void
)
{
    LwU32 refmpll_cfg;
    LwU32 refmpll_coeff;
    LwU32 lock_val;


    //disable refmpll before configuring coefficients
    refmpll_cfg = lwr_reg->pfb_fbpa_refmpll_cfg;
    refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE, _NO, refmpll_cfg);
    lwr_reg->pfb_fbpa_refmpll_cfg = refmpll_cfg;
    REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_CFG,refmpll_cfg);

    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1000);
    OS_PTIMER_SPIN_WAIT_NS(50);

    //configure the pll coefficients
    refmpll_coeff = lwr_reg->pfb_fbpa_refmpll_coeff;
    refmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_COEFF, _MDIV ,refmpll_m_val, refmpll_coeff);
    refmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_COEFF, _NDIV ,refmpll_n_val, refmpll_coeff);
    refmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_COEFF, _PLDIV,refmpll_p_val, refmpll_coeff);
    lwr_reg->pfb_fbpa_refmpll_coeff = refmpll_coeff;
    REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_COEFF,refmpll_coeff);

    if (GblRefmpllSpreadEn) {
        lwr_reg->pfb_fbpa_refmpll_ssd0 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_SSD0, _SDM_SSC_STEP,refmpll_ssc_step,lwr_reg->pfb_fbpa_refmpll_ssd0);
        lwr_reg->pfb_fbpa_refmpll_ssd1 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_SSD1, _SDM_SSC_MAX,refmpll_ssc_max_rm,lwr_reg->pfb_fbpa_refmpll_ssd1);
        lwr_reg->pfb_fbpa_refmpll_ssd1 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_SSD1, _SDM_SSC_MIN,refmpll_ssc_min_rm,lwr_reg->pfb_fbpa_refmpll_ssd1);
        lwr_reg->pfb_fbpa_refmpll_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_CFG2, _SSD_EN_SSC, 1,lwr_reg->pfb_fbpa_refmpll_cfg2);
    } else {
        lwr_reg->pfb_fbpa_refmpll_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_CFG2, _SSD_EN_SSC, 0,lwr_reg->pfb_fbpa_refmpll_cfg2);
    }



    if (GblSdmEn) {
        lwr_reg->pfb_fbpa_refmpll_ssd0 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_SSD0, _SDM_DIN,refmpll_sdm_din,lwr_reg->pfb_fbpa_refmpll_ssd0);
        lwr_reg->pfb_fbpa_refmpll_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_CFG2, _SSD_EN_SDM, 1,lwr_reg->pfb_fbpa_refmpll_cfg2);
    } else {
        lwr_reg->pfb_fbpa_refmpll_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_CFG2, _SSD_EN_SDM, 0,lwr_reg->pfb_fbpa_refmpll_cfg2);
    }


    REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_SSD0,lwr_reg->pfb_fbpa_refmpll_ssd0);
    REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_SSD1,lwr_reg->pfb_fbpa_refmpll_ssd1);
    REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_CFG2,lwr_reg->pfb_fbpa_refmpll_cfg2);

    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1001);

    OS_PTIMER_SPIN_WAIT_NS(50);

    //Enable refmpll
    refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE,_YES,refmpll_cfg);
    refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE, _YES,refmpll_cfg);
    lwr_reg->pfb_fbpa_refmpll_cfg = refmpll_cfg;
    REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_CFG,refmpll_cfg);

    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1002);
    OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole

    //wait for refmpll lock
    do {
        refmpll_cfg =0;
        refmpll_cfg = REG_RD32(LOG, LW_PFB_FBPA_REFMPLL_CFG);
        lock_val = REF_VAL(LW_PFB_FBPA_REFMPLL_CFG_PLL_LOCK,refmpll_cfg);
    }while (lock_val != 1);

    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL, fbio_mode_switch);
    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    return;
}

void
gddr_pgm_wdqs_rcvr_sel
(
        void
)
{
    LwU32 ptrim_sys_fbio_cfg;
    //pmct_table_strap_entry_flag2;

    //pmct_table_strap_entry_flag2 = TABLE_VAL(pMemClkStrapTable->Flags2);
    // gbl_target_pmct_flags2 = (PerfMemClk11StrapEntryFlags2* ) (&pmct_table_strap_entry_flag2);
    //pmctFlags2 = gTT.perfMemClkStrapEntry.Flags2 &selectedMemClkStrapTable.Flags2;
    ptrim_sys_fbio_cfg = REG_RD32(LOG, LW_PTRIM_SYS_FBIO_CFG);

    // LwU8 rcvr_sel = gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2RDQSRcvSel;
    LwU8 rcvr_sel = 0x1;
    ptrim_sys_fbio_cfg = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_CFG_, CFG10_RCVR_SEL,rcvr_sel,ptrim_sys_fbio_cfg);
// Keeping for now for asatish
    //    FW_MBOX_WR32(1, 0xAAAAAAAA);
    //    FW_MBOX_WR32(1, gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2DQWDQSRcvSel);
    //    FW_MBOX_WR32(1, gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2RCVCtrl);
    //    FW_MBOX_WR32(1, gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2RDQSRcvSel);
    //    FW_MBOX_WR32(1, ptrim_sys_fbio_cfg);
    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_CFG,ptrim_sys_fbio_cfg);

    return;
}

/*!
* @brief Re-calibrate the clock pad
*       Bug 1665412 - Due to larger variation on calibrated driver values with new processes:
*       if you calibrate the driver at low frequency (low voltage), you are less
*       likely to have that value work at high frequency (high voltage).
*       Hence while switching to a higher frequency(higher FBVDD) we need to recalibrate
*       at the higher voltage and update clock cal code before we start FB traffic at
*       higher frequency.
*/

void
gdrRecalibrate
(
    void
)
{
    PerfMemClk11Header* pMCHp = gBiosTable.pMCHp;
    PerfMemClkHeaderFlags0 memClkHeaderflags;
    EXTRACT_TABLE(&pMCHp->pmchf0,memClkHeaderflags);
    if(memClkHeaderflags.PMCHF0ClkCalUpdate)
    {
        LwU8 rbRise = REF_VAL(LW_PFB_FBIO_CALMASTER_RB_RISE, lwr_reg->pfb_fbio_calmaster_rb);
        LwU8 rbFall = REF_VAL(LW_PFB_FBIO_CALMASTER_RB_FALL, lwr_reg->pfb_fbio_calmaster_rb);
        LwU8 rbTerm = REF_VAL(LW_PFB_FBIO_CALMASTER_RB_TERM, lwr_reg->pfb_fbio_calmaster_rb);

        // Clamp start drv strengths to 0 to avoid underflow
        LwU8 rbRiseStart = (rbRise >= 16) ? (rbRise -16) : 0;
        LwU8 rbFallStart = (rbFall >= 16) ? (rbFall -16) : 0;
        LwU8 rbTermStart = (rbTerm >= 16) ? (rbTerm -16) : 0;

        // Following lines need to be uncommented if we want to priv log of figuring out if cal locked
        //lwr_reg->pfb_fbio_calmaster_rb = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_RB, _CAL_CYCLE, _INIT, lwr_reg->pfb_fbio_calmaster_rb);
        //REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_RB,lwr_reg->pfb_fbio_calmaster_rb);

        new_reg->pfb_fbio_calmaster_cfg = FLD_SET_DRF_NUM(_PFB, _FBIO_CALMASTER_CFG, _INTERVAL, 5, lwr_reg->pfb_fbio_calmaster_cfg);
        REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG,new_reg->pfb_fbio_calmaster_cfg);

        //LwU32 new_reg->pfb_fbio_calmaster_cfg3 = REG_RD32(LOG, LW_PFB_FBIO_CALMASTER_CFG3);
        new_reg->pfb_fbio_calmaster_cfg3 = lwr_reg->pfb_fbio_calmaster_cfg3;
        new_reg->pfb_fbio_calmaster_cfg3 = FLD_SET_DRF_NUM(_PFB, _FBIO_CALMASTER_CFG3, _RISE_START_VAL, rbRiseStart, new_reg->pfb_fbio_calmaster_cfg3);
        new_reg->pfb_fbio_calmaster_cfg3 = FLD_SET_DRF_NUM(_PFB, _FBIO_CALMASTER_CFG3, _FALL_START_VAL, rbFallStart, new_reg->pfb_fbio_calmaster_cfg3);
        new_reg->pfb_fbio_calmaster_cfg3 = FLD_SET_DRF_NUM(_PFB, _FBIO_CALMASTER_CFG3, _TERM_START_VAL, rbTermStart, new_reg->pfb_fbio_calmaster_cfg3);
        REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG3,new_reg->pfb_fbio_calmaster_cfg3);

        //Trigger_cal_cycle is self clearing bit; No need to issue another write to clear it.
        //Programming powerup_dly to take ~3.5 us to reduce cal_done time during mclk switch. Will be restored later at end of mclk switch
        new_reg->pfb_fbio_calmaster_cfg2 = lwr_reg->pfb_fbio_calmaster_cfg2;
        new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG2, _TRIGGER_CAL_CYCLE, _ENABLED, new_reg->pfb_fbio_calmaster_cfg2);
        new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBIO_CALMASTER_CFG2, _COMP_POWERUP_DLY, 95, new_reg->pfb_fbio_calmaster_cfg2);
        REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG2,new_reg->pfb_fbio_calmaster_cfg2);
        //REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(0),new_reg->pfb_fbio_calmaster_cfg2);

        //new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG2, _TRIGGER_CAL_CYCLE, _INACTIVE, new_reg->pfb_fbio_calmaster_cfg2);
        //REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG2,new_reg->pfb_fbio_calmaster_cfg2);
        //REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(1),new_reg->pfb_fbio_calmaster_cfg2);

         OS_PTIMER_SPIN_WAIT_US(35);
    }
}

void
gddr_pgm_shadow_reg_space
(
        void
)
{
    LwU32 fbio_delay;

    fbio_delay = lwr_reg->pfb_fbpa_fbio_delay;
    if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) {
        fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _SRC, GDDR_PRIV_SRC_REG_SPACE_ONESRC,fbio_delay);
        fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _PRIV_SRC, GDDR_PRIV_SRC_REG_SPACE_ONESRC,fbio_delay);
    } else if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
        fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _SRC, GDDR_PRIV_SRC_REG_SPACE_REFMPLL,fbio_delay);
        fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _PRIV_SRC, GDDR_PRIV_SRC_REG_SPACE_REFMPLL,fbio_delay);
    } else { //drampll
        fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _SRC, GDDR_PRIV_SRC_REG_SPACE_DRAMPLL,fbio_delay);
        fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _PRIV_SRC, GDDR_PRIV_SRC_REG_SPACE_DRAMPLL,fbio_delay);
    }

    lwr_reg->pfb_fbpa_fbio_delay = fbio_delay;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY,fbio_delay);
    return;
}

void
gddr_enable_pwrd
(
        void
)
{
    LwU32 fbio_cfg_pwrd;
    LwU32 fbpa_training_ctrl;
    LwU32 ctrl_wck_pads;

    fbpa_training_ctrl = lwr_reg->pfb_fbpa_training_ctrl;
    ctrl_wck_pads = REF_VAL(LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS,fbpa_training_ctrl);

    fbio_cfg_pwrd =  lwr_reg->pfb_fbpa_fbio_cfg_pwrd;
    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CLK_TX_PWRD, _DISABLE,fbio_cfg_pwrd);
    fbio_cfg_pwrd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_TX_PWRD, 0,fbio_cfg_pwrd);
    if (ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
        fbio_cfg_pwrd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_TX_PWRD, 0,fbio_cfg_pwrd);
    }
    lwr_reg->pfb_fbpa_fbio_cfg_pwrd = fbio_cfg_pwrd;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);

    return;
}

void
gddr_pgm_dq_term
(
        void
)
{
    //fbio_cfg10 - GPU termination
    LwU8 GpuTermination = gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0GPUTerm;
    LwU8 GpuDqsTermination = gTT.perfMemClkStrapEntry.Flags3.PMC11SEF3GPUDQSTerm;


    BITInternalUseOnlyV2* mypIUO = gTT.pIUO;
    if (mypIUO->bP4MagicNumber < VBIOS_P4_CL_FOR_DQ_DQS_TERMINATION) {
        GpuTermination  = 0;
        GpuDqsTermination  = 0;
    }

    //dq_term
    if (GpuTermination == 0 ) { //Enable
        lwr_reg->pfb_fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE ,_PULLUP,lwr_reg->pfb_fbpa_fbio_cfg10);
    } else {
        lwr_reg->pfb_fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE ,_NONE,lwr_reg->pfb_fbpa_fbio_cfg10);
    }

    //rdqs/wdqs term
    if (GpuDqsTermination == 0 ) { //Enable
        lwr_reg->pfb_fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _RDQS_TERM_MODE ,_PULLUP,lwr_reg->pfb_fbpa_fbio_cfg10);
        lwr_reg->pfb_fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _WDQS_TERM_MODE ,_PULLUP,lwr_reg->pfb_fbpa_fbio_cfg10);
    } else {
        lwr_reg->pfb_fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _RDQS_TERM_MODE ,_NONE,lwr_reg->pfb_fbpa_fbio_cfg10);
        lwr_reg->pfb_fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _WDQS_TERM_MODE ,_NONE,lwr_reg->pfb_fbpa_fbio_cfg10);
    }

    REG_WR32(BAR0, LW_PFB_FBPA_FBIO_CFG10,lwr_reg->pfb_fbpa_fbio_cfg10);
    return;
}

void
gddr_pgm_reg_values
(
        void
)
{
    LwU32 fbio_cfg12 ;
    LwU32 fbio_cfg1;
    fbio_cfg1  = lwr_reg->pfb_fbpa_fbio_cfg1;
    if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
        fbio_cfg1  = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _BRLSHFT_LOWSPEED_MODE,_ENABLED,fbio_cfg1);
    } else {
        fbio_cfg1  = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _BRLSHFT_LOWSPEED_MODE,_DISABLED,fbio_cfg1);
        fbio_cfg1  = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _CMD_BRLSHFT, _BIT,fbio_cfg1);
        fbio_cfg1  = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _RCV_CLOCKING, _CONTINUOUS,fbio_cfg1);
    }
    lwr_reg->pfb_fbpa_fbio_cfg1 = fbio_cfg1;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    //FB unit sim only programe the low freq of fbio_cfg1 brl_shift lowspeed mode

    //Timing1
    REG_WR32(LOG, LW_PFB_FBPA_TIMING1,new_reg->pfb_fbpa_timing1);

    //Timing10
    REG_WR32(LOG, LW_PFB_FBPA_TIMING10,new_reg->pfb_fbpa_timing10);

    //timing11
    REG_WR32(LOG, LW_PFB_FBPA_TIMING11,new_reg->pfb_fbpa_timing11);

    //timing12
    REG_WR32(LOG, LW_PFB_FBPA_TIMING12,new_reg->pfb_fbpa_timing12);

    //config0
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG0,new_reg->pfb_fbpa_config0);

    //config1
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG1,new_reg->pfb_fbpa_config1);

    //config2
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG2,new_reg->pfb_fbpa_config2);

    //config3
    LwU32 config3_temp;
    if ((gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) && 
        (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
	 config3_temp = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG3, _FAW,0x8,gTT.perfMemTweakBaseEntry.Config3);
    } else {
       config3_temp = 	gTT.perfMemTweakBaseEntry.Config3;
    }    
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG3,config3_temp);

    //config4
    LwU32 config4_temp;
    if ((gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) && 
        (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
	 config4_temp = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG4, _RRD,0x2,gTT.perfMemTweakBaseEntry.Config4);
    } else {
       config4_temp = 	gTT.perfMemTweakBaseEntry.Config4;
    }    
    // if P8 & Hynix & refresh(14:0) < 0x25 set it to 0x33.  bug 3184254
    if ((gbl_vendor_id == MEMINFO_ENTRY_VENDORID_HYNIX) && (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) {
        LwU16 refresh = bit_extract(config4_temp,0,15);     // get bits 14:0, refresh+refresh_lo
        if (refresh < 0x25) {
            config4_temp = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG4, _REFRESH, 0x6, config4_temp);
            config4_temp = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG4, _REFRESH_LO, 0x3, config4_temp);
        }
    }
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG4,config4_temp);

    //config5
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG5,gTT.perfMemTweakBaseEntry.Config5);

    //config6
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG6,gTT.perfMemTweakBaseEntry.Config6);

    //config7
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG7,gTT.perfMemTweakBaseEntry.Config7);

    //config8
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG8,gTT.perfMemTweakBaseEntry.Config8);

    //config9
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG9,gTT.perfMemTweakBaseEntry.Config9);

    //config10
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG10,gTT.perfMemTweakBaseEntry.Config10);

    //config11
    LwU32 config11_temp;
    if ((gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) && 
        (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
	 config11_temp = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG11, _RRDL,0x2,gTT.perfMemTweakBaseEntry.Config11);
    } else {
       config11_temp = 	gTT.perfMemTweakBaseEntry.Config11;
    }    
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG11,config11_temp);

    //FBIO_BG_VTT
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_BG_VTT,new_reg->pfb_fbpa_fbio_bg_vtt);

    //FBIO Broadcast
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST,new_reg->pfb_fbpa_fbio_broadcast);


    //timing21
    REG_WR32(LOG, LW_PFB_FBPA_TIMING21,gTT.perfMemTweakBaseEntry.Timing21);

    //timing22
    REG_WR32(LOG, LW_PFB_FBPA_TIMING22,gTT.perfMemTweakBaseEntry.Timing22);

    //cal_trng_arb
    REG_WR32(LOG, LW_PFB_FBPA_CAL_TRNG_ARB,gTT.perfMemTweakBaseEntry.Cal_Trng_Arb);

    //fbio_cfg10
    if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
        gddr_pgm_dq_term();
    }

    //fbio_cfg12
    if ((gbl_g5x_cmd_mode) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
        //LW_PFB_FBPA_FBIO_CFG12_WCK_INTRP_8UI_MODE_ENABLE
        fbio_cfg12 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG12, _WCK_INTRP_8UI_MODE, _ENABLE, new_reg->pfb_fbpa_fbio_cfg12);
    } else {
        //LW_PFB_FBPA_FBIO_CFG12_WCK_INTRP_8UI_MODE_DISABLE
        fbio_cfg12 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG12, _WCK_INTRP_8UI_MODE, _DISABLE, new_reg->pfb_fbpa_fbio_cfg12);
    }
    //Asatish for saving time from p8<->p5
    if(!((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) && 
            ((gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) ||
                    (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)))) {
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG12,fbio_cfg12);
    }

    return;
}

void
gddr_disable_self_refresh
(
        void
)
{
    LwU32 testcmd_ext;
    LwU32 testcmd;

    testcmd = lwr_reg->pfb_fbpa_testcmd;

    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        testcmd_ext = lwr_reg->pfb_fbpa_testcmd_ext;
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT, _ACT,_DDR6_SELF_REF_EXIT,testcmd_ext);
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM0,      _OFF,                   testcmd_ext);
        testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM1,      _OFF,                   testcmd_ext);
        testcmd_ext = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD_EXT,  _ADR_DRAM1,  0,                      testcmd_ext);
        lwr_reg->pfb_fbpa_testcmd_ext = testcmd_ext;
        REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT,testcmd_ext);
    }

    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES, _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE, _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS, _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS, _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,  _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0, _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1, _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD,_DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,  _DDR5_SELF_REF_EXIT, testcmd);
    testcmd = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD, _BANK, 0,                   testcmd);
    testcmd = FLD_SET_DRF_NUM(_PFB, _FBPA_TESTCMD, _ADR , 0,                  testcmd);

    lwr_reg->pfb_fbpa_testcmd = testcmd;
    REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD,testcmd);
    return;
}

void
gddr_enable_refresh
(
        void
)
{
    LwU32 fbpa_refctrl;
    fbpa_refctrl = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _VALID, _1, lwr_reg->pfb_fbpa_refctrl);
    lwr_reg->pfb_fbpa_refctrl = fbpa_refctrl;
    REG_WR32(LOG, LW_PFB_FBPA_REFCTRL,fbpa_refctrl);
    return;
}


void
gddr_enable_address_training
(
        void
)
{
    LwU32 fbpa_training_cmd;
    LwU32 fbpa_config1;


    //Address training Enabled for subp0
    fbpa_training_cmd = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CMD0);
    fbpa_training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _ADR, _ENABLED,fbpa_training_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD0,fbpa_training_cmd);

    //Address training Enabled for subp1
    fbpa_training_cmd = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CMD1);
    fbpa_training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _ADR, _ENABLED, fbpa_training_cmd);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_training_cmd);

    //qpop offset must be set to 2 for address training bug 1015705
    fbpa_config1 = REG_RD32(LOG, LW_PFB_FBPA_CONFIG1);
    fbpa_config1 = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG1, _QPOP_OFFSET,2,fbpa_config1);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG1,fbpa_config1);

    return;
}

void
gddr6_pgm_wck_term
(
        LwU32 clamshell,
        LwU32 wcktermination
)
{
    LwU32 mrs10;
    mrs10 = new_reg->pfb_fbpa_generic_mrs10;
    if (wcktermination) {
        if (clamshell == LW_PFB_FBPA_CFG0_CLAMSHELL_X32) {
            mrs10 = bit_assign(mrs10,10,LW_PFB_FBPA_GENERIC_MRS3_ADR_GDDR5_WCK_TERM_ZQ_DIV2,2);
        } else {
            mrs10 = bit_assign(mrs10,10,LW_PFB_FBPA_GENERIC_MRS3_ADR_GDDR5_WCK_TERM_ZQ,2);
        }
    } else {
        mrs10 = bit_assign(mrs10,10,LW_PFB_FBPA_GENERIC_MRS3_ADR_GDDR5_WCK_TERM_DISABLED,2);
    }

    new_reg->pfb_fbpa_generic_mrs10 = mrs10;
    return;
}


void
gddr_pgm_mrs3_reg
(
        void
)
{
    LwU32 mrs3;
    LwU32 meminfo_entry_mie5540;
    LwU32 vbios_freq_threshold;
    LwU32 clamshell;
    LwU32 mrs2;
    meminfo_entry_mie5540 = TABLE_VAL(mInfoEntryTable->mie5540);
    MemInfoEntry5540* my_meminfo_entry = (MemInfoEntry5540*) (&meminfo_entry_mie5540);

    vbios_freq_threshold = my_meminfo_entry->MIEMclkBGEnableFreq;//VBIOS configured in Mhz

    //force values
    //    vbios_freq_threshold = 4250;
    mrs3 = new_reg->pfb_fbpa_generic_mrs3;
    mrs2 = new_reg->pfb_fbpa_generic_mrs2;
    //    if ((gbl_target_freqMHz > vbios_freq_threshold) && (vbios_freq_threshold != 0)) {
    if (gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4BG_8 == 1) {
        //enable bank grouping
        mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_BANK_GROUPS, _ENABLED,mrs3);
    } else {
        //disable bank grouping
        mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_BANK_GROUPS, _DISABLED,mrs3);
    }

    if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
        //set ADR_GDDR5_RDQS
        if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_RDQS, _ENABLED, mrs3);
        } else {
            mrs2 = bit_assign(mrs2,9,1,1);
        }
    } else {
        //disable ADR_GDDR5_RDQS
        if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_RDQS, _DISABLED, mrs3);
        } else {
            mrs2 = bit_assign(mrs2,9,0,1);
        }
    }

    clamshell = lwr_reg->pfb_fbpa_cfg0;
    clamshell = REF_VAL(LW_PFB_FBPA_CFG0_CLAMSHELL,clamshell);
    //To check for vbios wck termination
    //if flag is set - ZQ_DIV2, else DISABLED

    LwU32 wck_term = gTT.perfMemClkBaseEntry.Flags1.PMC11EF1WCKTermination;
    //    FW_MBOX_WR32(1, wck_term);

    if (!(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
        if (wck_term) {
            if (clamshell == LW_PFB_FBPA_CFG0_CLAMSHELL_X32) {
                mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_WCK_TERM, _ZQ_DIV2,mrs3);
            } else {
                mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_WCK_TERM, _ZQ,mrs3);
            }
        } else {
            mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_WCK_TERM, _DISABLED,mrs3);
        }
    } else {
        gddr6_pgm_wck_term(clamshell,wck_term);
        //From Script
        // mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_BANK_GROUPS,_CCDL_3,mrs3 );
    }

    new_reg->pfb_fbpa_generic_mrs3 = mrs3;
    new_reg->pfb_fbpa_generic_mrs2 = mrs2;

    return;

}


void
gddr_update_mrs_reg
(
        void
)
{

    //reordering sequence to match pascal seq
    //

    //Generic MRS3
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS3,new_reg->pfb_fbpa_generic_mrs3);

      //Generic MRS1
    LwU8 MemoryDll = gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0MemDLL;
    //Enable g5 pll in cascaded mode
//    if ((gbl_g5x_cmd_mode) && (gbl_vendor_id == MEMINFO_ENTRY_VENDORID_MICRON)) {  	    
    if ((MemoryDll == 0 )&& (gbl_vendor_id == MEMINFO_ENTRY_VENDORID_MICRON))  {
        new_reg->pfb_fbpa_generic_mrs1 =
            FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_PLL, _ENABLED,  new_reg->pfb_fbpa_generic_mrs1);
    } else {
        new_reg->pfb_fbpa_generic_mrs1 =
            FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_PLL, _DISABLED, new_reg->pfb_fbpa_generic_mrs1);
    }

    //changed the sequence from Pascal for G5x
    //Updating the Addr termination always at low speed clock for low speed
    //target frequency
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1,new_reg->pfb_fbpa_generic_mrs1);

    //Generic MRS
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS,new_reg->pfb_fbpa_generic_mrs);

    //changed the sequence from Pascal for G6
    //Updating the Addr termination always at low speed clock for low speed
    //target frequency
    //Generic MRS8
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS8, new_reg->pfb_fbpa_generic_mrs8);

    //Generic MRS5
    REG_WR32(BAR0, LW_PFB_FBPA_GENERIC_MRS5,new_reg->pfb_fbpa_generic_mrs5);

    //Generic MRS5
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
      REG_WR32(BAR0, LW_PFB_FBPA_GENERIC_MRS6,new_reg->pfb_fbpa_generic_mrs6);
    }

    LwU8 half_vrefd = gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1MemVREFD;
    if (half_vrefd == 0) {
        new_reg->pfb_fbpa_generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_HALF_VREFD, _70,new_reg->pfb_fbpa_generic_mrs7);
    } else {
        new_reg->pfb_fbpa_generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_HALF_VREFD, _50,new_reg->pfb_fbpa_generic_mrs7);
    }

    LwU8 InternalVrefc;
    InternalVrefc = gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5G5XVref;
    //Vrefc enabled - 50%
    if ((InternalVrefc == 1) &&
            ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X))) {
        new_reg->pfb_fbpa_generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_TEMP_SENSE, _ENABLED,new_reg->pfb_fbpa_generic_mrs7);
    }

    //asatish fixing for wck pin mode for p5
    //changing MRS7[0] to 1 for non drampll path
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
      if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) && (GblEnWckPinModeTr == LW_TRUE)) { 
        new_reg->pfb_fbpa_generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_PLL_STANDBY,_ENABLED,new_reg->pfb_fbpa_generic_mrs7 );
      } else {
        new_reg->pfb_fbpa_generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_PLL_STANDBY,_DISABLED,new_reg->pfb_fbpa_generic_mrs7 );
      }
    }
    //Generic MRS7
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS7,new_reg->pfb_fbpa_generic_mrs7);

    //Following MRS registers are not written in PASCAL,
    //Generic MRS2 - Need for EDCRATE and Bank grouping
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS2,new_reg->pfb_fbpa_generic_mrs2);


    //Generic MRS4 - Addition
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4,new_reg->pfb_fbpa_generic_mrs4);


    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        //Generic MRS10
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS10, new_reg->pfb_fbpa_generic_mrs10);
    }

    return;
}


void
gddr_set_trng_ctrl_reg
(
        void
)
{
    LwU32 fbpa_trng_ctrl;
    LwU32 fbpa_cfg0;
    LwU32 EdcRate;

    fbpa_trng_ctrl = lwr_reg->pfb_fbpa_training_ctrl;
    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    //considered gp102 RM code function fbSetTrainingCtrl_GP102
    fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _FAST_UPDATES,  _ENABLED , fbpa_trng_ctrl);
    fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _ASYNC_UPDATES, _ENABLED , fbpa_trng_ctrl);
    fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _ASR_FAST_EXIT, _ENABLED , fbpa_trng_ctrl);
    fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _VREF_TRAINING, _DISABLED, fbpa_trng_ctrl);
    fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _BRLSHFT_QUICK, _DISABLED, fbpa_trng_ctrl);
    fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _TRIMMER_TRAINING, _DISABLED, fbpa_trng_ctrl);

    //if edc enabled, enable edc training if it is g5x cmd mode
    //rdedc = REF_VAL(LW_PFB_FBPA_CFG0_RDEDC,fbpa_cfg0);
    //wredc = REF_VAL(LW_PFB_FBPA_CFG0_WREDC,fbpa_cfg0);
    EdcRate = REF_VAL(LW_PFB_FBPA_CFG0_EDC_RATE, fbpa_cfg0);

    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (!gbl_g5x_cmd_mode)) {
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB,_FBPA_TRAINING_CTRL, _ASR_FAST_EXIT, _DISABLED,fbpa_trng_ctrl);
    }

    //FOLLOWUP: revisit
    if ((gbl_g5x_cmd_mode && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) || ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (EdcRate == LW_PFB_FBPA_CFG0_EDC_RATE_HALF))) {
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _G5X_EDC_TR_MODE, _ENABLED,fbpa_trng_ctrl);
    } else {
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _G5X_EDC_TR_MODE, _DISABLED,fbpa_trng_ctrl);
    }

    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_DELAY,_4UI,fbpa_trng_ctrl);
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_EARLY_FLIP, _DIABLED,fbpa_trng_ctrl);//manauls has DIABLED instead of DISABLED
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_STOP_CLOCKS,_DISABLED,fbpa_trng_ctrl);
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _BRLSHFT_QUICK, _ENABLED,fbpa_trng_ctrl);

        LwU8 vrefTraining = gTT.perfMemClkStrapEntry.Flags3.PMC11SEF3VREFTrn;
        if (vrefTraining) { //FOLLOWUP : need to enable for every p0 switch?
            fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _VREF_TRAINING,  _ENABLED, fbpa_trng_ctrl);
        }
    } else if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_DELAY, _2UI,fbpa_trng_ctrl);
        if ((GblEnWckPinModeTr) && (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
          fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_STOP_CLOCKS, _ENABLED,fbpa_trng_ctrl);
        } else {
          fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_STOP_CLOCKS, _DISABLED,fbpa_trng_ctrl);
        }
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_EARLY_FLIP, _DIABLED,fbpa_trng_ctrl);
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _BRLSHFT_QUICK, _ENABLED,fbpa_trng_ctrl);

    } else {
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _TRIMMER_TRAINING, _ENABLED,fbpa_trng_ctrl);
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_EARLY_FLIP, _DIABLED,fbpa_trng_ctrl);
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_STOP_CLOCKS, _ENABLED,fbpa_trng_ctrl);
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_DELAY,_2UI,fbpa_trng_ctrl);   
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _BRLSHFT_QUICK, _ENABLED,fbpa_trng_ctrl);
    }

    lwr_reg->pfb_fbpa_training_ctrl = fbpa_trng_ctrl;
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL,fbpa_trng_ctrl);
    return;

}

void
gddr_load_brl_shift_val
(
        LwU32 rd_tr,
        LwU32 wr_tr
)
{
    LwU32 ob_brlshift;
    LwU32 ib_brlshift;

    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
        ib_brlshift = 0x01010101;
    } else {
        ib_brlshift = 0x03030303;
    }

    if (!wr_tr) {
        //ob_brlshift = gTT.perfMemClkStrapEntry.WriteBrlShft;
        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            ob_brlshift = 0x0d0d0d0d;
        } else {
            ob_brlshift = 0x08080808; //force values
        }
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_OB_BRLSHFT3,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_OB_BRLSHFT3,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_OB_BRLSHFT3,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_OB_BRLSHFT3,ob_brlshift);

        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_OB_BRLSHFT3,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_OB_BRLSHFT3,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_OB_BRLSHFT3,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_OB_BRLSHFT1,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_OB_BRLSHFT2,ob_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_OB_BRLSHFT3,ob_brlshift);

        //wait for 1us
        OS_PTIMER_SPIN_WAIT_US(1);
    }

    if (!rd_tr) {
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_IB_BRLSHFT3,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_IB_BRLSHFT3,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_IB_BRLSHFT3,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_IB_BRLSHFT3,ib_brlshift);

        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_IB_BRLSHFT3,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_IB_BRLSHFT3,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_IB_BRLSHFT3,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_IB_BRLSHFT1,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_IB_BRLSHFT2,ib_brlshift);
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_IB_BRLSHFT3,ib_brlshift);

        //wait for 1us
        OS_PTIMER_SPIN_WAIT_US(1);
    }
    return;
}

void
gddr_wait_trng_complete
(
        LwU32 wck,
        LwU32 rd_wr
)
{
    LwU32 fbpa_trng_status;
    LwU32 subp0_trng_status;
    LwU32 subp1_trng_status;

    //FOLLOWUP Add timeout
    do {
        fbpa_trng_status = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_STATUS);
        subp0_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS, fbpa_trng_status);
        subp1_trng_status = REF_VAL(LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS, fbpa_trng_status);
    } while (
        (subp0_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS_RUNNING) ||
        (subp1_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS_RUNNING)
    );

    if ((subp0_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP0_STATUS_ERROR) ||
        (subp1_trng_status == LW_PFB_FBPA_TRAINING_STATUS_SUBP1_STATUS_ERROR)) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_TRAINING_ERROR);
// Keeping for now for asatish
//      FW_MBOX_WR32(12, 0x0000ababa); 
//      FW_MBOX_WR32(1, subp0_trng_status); 
//      FW_MBOX_WR32(2, subp1_trng_status); 
    }

    return;
}

void
gddr_initialize_tb_trng_mon
(
        void
)
{
    LwU32 falcon_mon;

    //waiting for TB initialization completion
    falcon_mon = 0;
    while (falcon_mon != 0x54524743) {
        OS_PTIMER_SPIN_WAIT_US(1);
    }
}


void 
gddr_mem_link_training
(
        void
)
{
    LwU32 fbpa_trng_cmd;
    LwU32 rd_tr_en;
    LwU32 wr_tr_en;

    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        func_fbio_intrpltr_offsets(6,29);
    }

    gddr_set_trng_ctrl_reg();

    fbpa_trng_cmd = lwr_reg->pfb_fbpa_training_cmd0;
    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _ADR,     _DISABLED, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK,     _DISABLED, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD,      _DISABLED, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR,      _DISABLED, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC,_DISABLED, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _STROBE,  _DISABLED, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RW_LEVEL,_PIN,      fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _INTRPLTR, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _STEP,    _INIT,     fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _INIT,      fbpa_trng_cmd);

        //default turn on wck training
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _ENABLED,fbpa_trng_cmd);

        //Performing wck training before rd wr to ensure edc tracking is done in between
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);

        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        //wait for wck training to complete
        gddr_wait_trng_complete(1,0);

        if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 ) && ((gblEnEdcTracking) || (gblElwrefTracking))) {

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT))

            BOOT_TRAINING_FLAGS *pTrainingFlags = &gTT.bootTrainingTable.MemBootTrainTblFlags;
            GblSkipBootTimeTrng = pTrainingFlags->flag_skip_boot_training;
            GblBootTimeEdcTrk   = pTrainingFlags->flag_edc_tracking_en;
            GblBootTimeVrefTrk  = pTrainingFlags->flag_vref_tracking_en;

            if ((GblSkipBootTimeTrng == 0) && (GblBootTimeEdcTrk == 1) && (GblBootTimeVrefTrk== 1)){
                enableEvent(EVENT_SAVE_TRAINING_DATA);
                GblVrefTrackPerformedOnce = LW_TRUE;
            }
#endif  // FBFALCON_JOB_TRAINING_DATA_SUPPORT

            LwU32 pmctedctrackinggain = gTT.perfMemClkStrapEntry.EdcTrackingGain;
            LwU32 pmctvreftrackinggain = gTT.perfMemClkStrapEntry.VrefTrackingGain;
            gbledctrgain = pmctedctrackinggain;
            gblvreftrgain = pmctvreftrackinggain;
            my_EdcTrackingGainTable.EDCTrackingRdGainInitial = pmctedctrackinggain & 0xFF;
            my_EdcTrackingGainTable.EDCTrackingRdGainFinal   = (pmctedctrackinggain >> 8) & 0xFF;
            my_EdcTrackingGainTable.EDCTrackingWrGainInitial = (pmctedctrackinggain >> 16) & 0xFF;
            my_EdcTrackingGainTable.EDCTrackingWrGainFinal   = (pmctedctrackinggain >> 24) & 0xFF;
            my_EdcTrackingGainTable.VrefTrackingGainInitial  = pmctvreftrackinggain & 0xFF;
            my_EdcTrackingGainTable.VrefTrackingGainFinal    = (pmctvreftrackinggain >> 8) & 0xFF;

            //Additional step for setting up edc tracking
            if (((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 ) && ((gblEnEdcTracking) || (gblElwrefTracking)))) {
                gddr_setup_edc_tracking(1,&my_EdcTrackingGainTable);
            }

            //Additional step for setting up edc tracking
            //FOLLOWUP Add vbios flag
            if (((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 ) && ((gblEnEdcTracking) || (gblElwrefTracking)))) {
                //              FW_MBOX_WR32(4, 0x000EDC16);
                func_fbio_intrpltr_offsets(38,60);
                //              FW_MBOX_WR32(2, 0x000EDC16);
            }

            //start edc tracking
            //           FW_MBOX_WR32(4, 0x000EDC33);
            // Tracking current value of VREF_CODE3_PAD9 just before EDC tracking is enabled.
            // This is done at this point as priv_src is applied, read happens only when EDC tracking is enabled
            // and to avoid N priv reads.
            new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
            gddr_start_edc_tracking(1,gblEnEdcTracking,gblElwrefTracking,&my_EdcTrackingGainTable,startFbStopTimeNs);

            //EDC_TR step 10: write vref tracking disable training updates to 1 before rd/wr training
            lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _DISABLE_TRAINING_UPDATES, 1,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            LwU8 spare_field4 = gTT.perfMemClkBaseEntry.spare_field4;
            LwU8 VbiosEdcVrefVbiosCtrl = spare_field4 & 0x1;
            LwU8 VrefTrackingAlwaysOn = spare_field4 & 0x4;
            if (((VbiosEdcVrefVbiosCtrl) && (VrefTrackingAlwaysOn))) {
                lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _EN, 0,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            }
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,lwr_reg->pfb_fbpa_fbio_vref_tracking);

            if ((gblElwrefTracking) && (!GblVrefTrackPerformedOnce)) {
                enableEvent(EVENT_SAVE_TRAINING_DATA);
                GblVrefTrackPerformedOnce = LW_TRUE;
            }
        }

        //disabling wck training
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _DISABLED,fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _INIT,      fbpa_trng_cmd);

        //FOLLOWUP add WAR for Hynix memory for dcc handling - Hynix used for legacy support

        //default Enable RD/WR training
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD, _ENABLED, fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED, fbpa_trng_cmd);

        //start wck/rd/wr training for each subp

        lwr_reg->pfb_fbpa_training_cmd1 = fbpa_trng_cmd;
        lwr_reg->pfb_fbpa_training_cmd0 = fbpa_trng_cmd;
        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        ////FB UNIT LEVEL only
        ////Hinting tb training monitor to initialize training and load patrams
        //gddr_initialize_tb_trng_mon();

        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
        rd_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_RD,fbpa_trng_cmd);
        wr_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_WR,fbpa_trng_cmd);

        //wait for wck/rd/wr training complete
        gddr_wait_trng_complete(0,1);

        //load vbios values for brl shifters if RD/WR training is disabled
        gddr_load_brl_shift_val(rd_tr_en, wr_tr_en);

    } else {
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _ADR,      _DISABLED,   fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _WCK,      _DISABLED,   fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _RD,       _DISABLED,   fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _WR,       _DISABLED,   fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _PERIODIC, _DISABLED,   fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _STROBE,   _ENABLED,    fbpa_trng_cmd); //enabled
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _RW_LEVEL, _BYTE,       fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _GO,       _INIT,        fbpa_trng_cmd);

        if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
          //to ensure wck training for gddr6 is done as trimmer mode for refmpll path 
          //must be reverted back to ddll mode for write training 
          if ((GblEnWckPinModeTr) && (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 )) {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _TRIMMER,fbpa_trng_cmd);
          } else {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _DLL,fbpa_trng_cmd);
          }
        } else {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _TRIMMER,fbpa_trng_cmd);
        }

        //by default enabling wck training only
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _ENABLED,fbpa_trng_cmd);

        //Performing wck training before rd wr to ensure edc tracking is done in between
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        //wait for wck training to complete
        gddr_wait_trng_complete(1,0);

        //disabling wck training
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _DISABLED,fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,       _INIT,        fbpa_trng_cmd);

	    //reverting back to ddll mode for p5 wr training in g6
    	if ((GblEnWckPinModeTr == LW_TRUE) && (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) && 
            (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 )) {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _DLL,fbpa_trng_cmd);
        }

        //default Enable RD/WR training
        //disabling read training fbpa_trng_cmd = FLD_SET_DRF(_PFB _FBPA_TRAINING_CMD, _RD, _ENABLED,fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED,fbpa_trng_cmd);

        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
        rd_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_RD,fbpa_trng_cmd);
        wr_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_WR,fbpa_trng_cmd);

        //wait for completion of training update.
        gddr_wait_trng_complete(0,1);

        //load vbios values for brl shifters if RD/WR training is disabled
        //gddr_load_brl_shift_val(rd_tr_en, wr_tr_en);
    }
    return;
}

void
gddr_enable_edc_replay
(
        void
)
{
    LwU32 fbpa_cfg0;
    LwU32 fbpa_cfg8;
    LwU32 generic_mrs4;

    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        if (!((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X) && (!gbl_g5x_cmd_mode))) {
            fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
            fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _RDEDC, _ENABLED,fbpa_cfg0);
            fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _WREDC, _ENABLED,fbpa_cfg0);
            fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _REPLAY,_ENABLED,fbpa_cfg0);
            lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
            REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

            generic_mrs4 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);
            //Addition avinash
            generic_mrs4 = FLD_SET_DRF(_PFB,_FBPA_GENERIC_MRS4,_ADR_GDDR5_RDCRC,_ENABLED,generic_mrs4);
            generic_mrs4 = FLD_SET_DRF(_PFB,_FBPA_GENERIC_MRS4,_ADR_GDDR5_WRCRC,_ENABLED,generic_mrs4);

            REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4,generic_mrs4);
            //Ensure MRS command is sent to Dram
            gddr_flush_mrs_reg();
        }
    } else {
        fbpa_cfg8 = lwr_reg->pfb_fbpa_fbio_cfg8;
        fbpa_cfg8 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG8, _RFU, _INTERP_EDC_PWRD_DISABLE,fbpa_cfg8);
        lwr_reg->pfb_fbpa_fbio_cfg8 = fbpa_cfg8;
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG8,fbpa_cfg8);
    }
    return;
}

void
gddr_enable_gddr5_lp3
(
        void
)
{
    LwU32 mrs5;
    LwU32 meminfo3116;
    LwU32 lp3_mode;

    mrs5 = new_reg->pfb_fbpa_generic_mrs5;
    meminfo3116 = TABLE_VAL(mInfoEntryTable->mie3116);
    MemInfoEntry3116* my_meminfo3116 = (MemInfoEntry3116*) (&meminfo3116);
    lp3_mode = bit_extract(meminfo3116,12,1);
    mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR5_LP3, lp3_mode,mrs5);
    //asatish emu FOLLOWUP // for emulation only
    //    mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR5_LP3, 1,mrs5);
    gbl_periodic_trng = !my_meminfo3116->MIEFeature0;
    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS5,mrs5);
    return;

}

void
gddr_reset_read_fifo
(
        void
)
{
    LwU32 fbio_cfg1;

    fbio_cfg1 = lwr_reg->pfb_fbpa_fbio_cfg1;
    fbio_cfg1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET, _FIFO_RESET,fbio_cfg1);
    lwr_reg->pfb_fbpa_fbio_cfg1 = fbio_cfg1;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    return;
}

void
gddr_enable_periodic_trng
(
        void
)
{
    //enabling Periodic training average
    LwU8 PeriodicTrngAvg = gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6PerTrngAvg;

    if (PeriodicTrngAvg) {
        lwr_reg->pfb_fbpa_training_rw_periodic_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PERIODIC_CTRL,_EN_AVG, 1, lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_PERIODIC_CTRL,lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);
    }

    //enabling periodic shift training
    LwU8 PeriodicShftAvg = gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6PerShiftTrng;
    if (PeriodicShftAvg) {
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_WR, _EN, lwr_reg->pfb_fbpa_training_ctrl2);
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_RD, _EN, lwr_reg->pfb_fbpa_training_ctrl2);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2,lwr_reg->pfb_fbpa_training_ctrl2);
    }

    LwU32 fbpa_trng_cmd;

    LwU8 periodicTrng = gTT.perfMemClkStrapEntry.Flags3.PMC11SEF3PerTrn;

    if (periodicTrng) {
        if ((gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) || (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)) {
            fbpa_trng_cmd = REG_RD32(LOG, LW_PFB_FBPA_TRAINING_CMD1);

            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _ENABLED,fbpa_trng_cmd);
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR,       _ENABLED,fbpa_trng_cmd);
            if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
                fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD,_ENABLED,fbpa_trng_cmd);
            } else {
                fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD,_DISABLED,fbpa_trng_cmd);
            }
            gddr_pgm_trng_lower_subp(fbpa_trng_cmd);

            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC,_ENABLED,fbpa_trng_cmd);
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR,_ENABLED,fbpa_trng_cmd);
            if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
                fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD,_ENABLED,fbpa_trng_cmd);
            } else {
                fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD,_DISABLED,fbpa_trng_cmd);
            }
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
        }
    }
    return;
}

void
gddr_start_fb
(
        void
)
{
    LwU32 fbpa_cfg0;

    //reusing gv100 function
    start_fb();

    //configuring acpd with value before the mclk seq
    fbpa_cfg0 = REG_RD32(LOG, LW_PFB_FBPA_CFG0);
    fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACPD, gbl_dram_acpd,fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}

void
gddr_set_rpre_r2w_bus
(
        void
)
{
    LwU32 fbpa_config2;

    fbpa_config2 = new_reg->pfb_fbpa_config2;
    fbpa_config2 = FLD_SET_DRF(_PFB, _FBPA_CONFIG2, _RPRE,_1,fbpa_config2);
    fbpa_config2 = FLD_SET_DRF(_PFB, _FBPA_CONFIG2, _R2W_BUS,_MIN,fbpa_config2);
    new_reg->pfb_fbpa_config2 = fbpa_config2;
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG2,fbpa_config2);

    return;
}

void
gddr_disable_low_pwr_en_edc_early
(
        void
)
{
    LwU32 fbpa_cfg0;

    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _LOW_POWER, _0,fbpa_cfg0);
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _EDC_EARLY_VALUE,_1,fbpa_cfg0);
    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}

void
gddr_pgm_brl_shifter
(
        LwS32 enable
)
{
    LwU32 fbpa_fbio_cfg10;

    fbpa_fbio_cfg10 = lwr_reg->pfb_fbpa_fbio_cfg10;
    if (enable) {
        fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _IB_BRLSHFT, _ENABLED,fbpa_fbio_cfg10);
    } else {
        fbpa_fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _IB_BRLSHFT, _DISABLED,fbpa_fbio_cfg10);
    }
    lwr_reg->pfb_fbpa_fbio_cfg10 = fbpa_fbio_cfg10;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG10,fbpa_fbio_cfg10);
    return;
}

void
gddr_disable_low_pwr_edc
(
        void
)
{
    LwU32 fbpa_cfg0;

    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5)  {
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _LOW_POWER, _0,fbpa_cfg0);
    }
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _RDEDC, _DISABLED,fbpa_cfg0);
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _WREDC, _DISABLED,fbpa_cfg0);
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _REPLAY,_DISABLED,fbpa_cfg0);

    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}

void
gddr_disable_crc_mrs4
(
        void
)
{
    LwU32 generic_mrs4;

    //generic_mrs4 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);
    generic_mrs4 = new_reg->pfb_fbpa_generic_mrs4;
    generic_mrs4 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_RDCRC, _DISABLED,generic_mrs4);
    generic_mrs4 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_WRCRC, _DISABLED,generic_mrs4);
    new_reg->pfb_fbpa_generic_mrs4 = generic_mrs4;
    //    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4,generic_mrs4);
    return;
}

void
gddr_power_down_edc_intrpl
(
        void
)
{
    LwU32 fbpa_fbio_cfg8;

    fbpa_fbio_cfg8 = lwr_reg->pfb_fbpa_fbio_cfg8;
    fbpa_fbio_cfg8 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG8, _RFU, _INTERP_EDC_PWRD_ENABLE,fbpa_fbio_cfg8);
    lwr_reg->pfb_fbpa_fbio_cfg8 = fbpa_fbio_cfg8;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG8,fbpa_fbio_cfg8);
    return;
}

void
gddr_clr_intrpl_padmacro_quse
(
        void
)
{
    LwU32 fbpa_fbio_cfg1;

    fbpa_fbio_cfg1 = lwr_reg->pfb_fbpa_fbio_cfg1;
    fbpa_fbio_cfg1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _CMD_BRLSHFT, _LEGACY,fbpa_fbio_cfg1);
    fbpa_fbio_cfg1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET, _FIFO_RESET,fbpa_fbio_cfg1);
    fbpa_fbio_cfg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _EXTENDED_QUSE, gTT.perfMemClkBaseEntry.Flags0.PMC11EF0ExtQuse,fbpa_fbio_cfg1);
    lwr_reg->pfb_fbpa_fbio_cfg1 = fbpa_fbio_cfg1;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbpa_fbio_cfg1);
    return;
}

void
gddr_clr_ib_padmacro_ptr
(
        void
)
{
    LwU32 fbio_cfg1;

    fbio_cfg1 = lwr_reg->pfb_fbpa_fbio_cfg1;
    fbio_cfg1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET, _INIT,fbio_cfg1);
    lwr_reg->pfb_fbpa_fbio_cfg1 = fbio_cfg1;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    return;
}

void
gddr_disable_drampll
(
        void
)
{
    LwU32 drampll_cfg;

    drampll_cfg = lwr_reg->pfb_fbpa_drampll_cfg;
    drampll_cfg = FLD_SET_DRF(_PFB, _FBPA_DRAMPLL_CFG, _ENABLE, _NO, drampll_cfg);
    lwr_reg->pfb_fbpa_drampll_cfg = drampll_cfg;
    REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_CFG,drampll_cfg);
    return;

}

void
gddr_disable_refmpll
(
        void
)
{
    LwU32 refmpll_cfg;

    refmpll_cfg = lwr_reg->pfb_fbpa_refmpll_cfg;
    refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE,_NO,refmpll_cfg);
    lwr_reg->pfb_fbpa_refmpll_cfg = refmpll_cfg;
    REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_CFG,refmpll_cfg);
    return;
}

void
gddr_enable_cmos_cml_clk
(
        LwS32 cml
)
{
    LwU32 fbio_cmos_clk;

    fbio_cmos_clk = lwr_reg->pfb_fbpa_fbio_cmos_clk;
    if (cml) {
        fbio_cmos_clk = FLD_SET_DRF(_PFB, _FBPA_FBIO_CMOS_CLK, _SEL, _CML,fbio_cmos_clk);
    } else {
        fbio_cmos_clk = FLD_SET_DRF(_PFB, _FBPA_FBIO_CMOS_CLK, _SEL, _CMOS,fbio_cmos_clk);
    }
    lwr_reg->pfb_fbpa_fbio_cmos_clk = fbio_cmos_clk;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CMOS_CLK,fbio_cmos_clk);
    return;
}

void
gddr_cfg_padmacro_sync_ptr_offset
(
        void
)
{
    LwU32 fbio_cfg1;

    fbio_cfg1 = lwr_reg->pfb_fbpa_fbio_cfg1;
    fbio_cfg1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _PADMACRO_SYNC,_ENABLE,fbio_cfg1);
    fbio_cfg1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG1, _IB_PMACRO_PTR_OFFSET,_INIT,fbio_cfg1);
    lwr_reg->pfb_fbpa_fbio_cfg1 = fbio_cfg1;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    return;
}

void
gddr_enable_edc_vbios
(
        void
)
{
    LwU32 fbpa_cfg0;
    LwU32 meminfo5540;
    LwU32 EdcRate;
    LwU32 VbiosEdc;
    LwU32 VbiosEdcReplay;

    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    meminfo5540 = TABLE_VAL(mInfoEntryTable->mie5540);
    VbiosEdc = bit_extract(meminfo5540, 14,1);
    VbiosEdcReplay = bit_extract(meminfo5540, 15,1);
    if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5 ) ||
            (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 ) ||
            ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X ) && (gbl_g5x_cmd_mode))) {
        //FOLLOWUP MIEGddr5Edc taken for both read edc and write edc
        fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _RDEDC,VbiosEdc,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _WREDC,VbiosEdc,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _REPLAY,VbiosEdcReplay,fbpa_cfg0);
        lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
        REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

        EdcRate = REF_VAL(LW_PFB_FBPA_CFG0_EDC_RATE,fbpa_cfg0);

        if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            new_reg->pfb_fbpa_generic_mrs2 = bit_assign(new_reg->pfb_fbpa_generic_mrs2,8,!EdcRate,1);
        }
        if (VbiosEdc) {
            //As the dram is in self ref, the MRS4 reg will be written at step 48
            new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_RDCRC,0,new_reg->pfb_fbpa_generic_mrs4);
            new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_WRCRC,0,new_reg->pfb_fbpa_generic_mrs4);
        }
    }
    return;
}

void
gddr_power_up_wdqs_intrpl
(
        void
)
{
    LwU32 fbio_cfg8;
    fbio_cfg8 = lwr_reg->pfb_fbpa_fbio_cfg8;
    fbio_cfg8 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG8, _RFU, _INTERP_EDC_PWRD_DISABLE,fbio_cfg8);
    lwr_reg->pfb_fbpa_fbio_cfg8 = fbio_cfg8;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG8,fbio_cfg8);
    return;
}

void
gddr_pgm_drampll_cfg_and_lock_wait
(
        void
)
{
    LwU32 bcast_drampll_cfg;
    LwU32 bcast_drampll_coeff;

    bcast_drampll_cfg   = lwr_reg->pfb_fbpa_drampll_cfg;
    bcast_drampll_coeff = lwr_reg->pfb_fbpa_drampll_coeff;

    //disable drampll before configuring co-efficients
    bcast_drampll_cfg = FLD_SET_DRF(_PFB, _FBPA_DRAMPLL_CFG, _ENABLE, _NO,bcast_drampll_cfg);
    REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_CFG,bcast_drampll_cfg);

    OS_PTIMER_SPIN_WAIT_NS(50);

    //configure the pll co-efficients
    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _MDIV,drampll_m_val,bcast_drampll_coeff);
    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _NDIV,drampll_n_val,bcast_drampll_coeff);
    //    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _PLDIV,drampll_p_val,bcast_drampll_coeff);
    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _SEL_DIVBY2,drampll_divby2,bcast_drampll_coeff);
    lwr_reg->pfb_fbpa_drampll_coeff = bcast_drampll_coeff;
    REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_COEFF,bcast_drampll_coeff);

    OS_PTIMER_SPIN_WAIT_NS(50);

    //Enable drampll
    bcast_drampll_cfg = FLD_SET_DRF(_PFB, _FBPA_DRAMPLL_CFG, _ENABLE, _YES,bcast_drampll_cfg);
    lwr_reg->pfb_fbpa_drampll_cfg = bcast_drampll_cfg;
    REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_CFG,bcast_drampll_cfg);
    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    // wait for pll lock

    // start with lockMask as all disabled partitions then start disabling
    // active partitions based on lock until we have achieved lockTarget
    // this minimizes the number of priv accesses
    LwU32 lockMask = getDisabledPartitionMask();
    LwU32 lockTarget = getPartitionMask() | lockMask;

    LwU8 current = 0;   // partition counter
    LwU8 i = 0;         // index
    LwU32 timeout = 0;
    do
    {
        LwU8 partitionLocked = ((lockMask >> i) & 0x1);
        if(partitionLocked == 0)
        {
            LwU32 refmpllCfg = REG_RD32(BAR0, unicast(LW_PFB_FBPA_DRAMPLL_CFG,i));
            LwU8 lock = REF_VAL(LW_PFB_FBPA_DRAMPLL_CFG_PLL_LOCK,refmpllCfg) & 0x1;
            lockMask |= (lock << i);
            current++;
        }
        if (current >= gNumberOfPartitions) {
            current = 0;
            i=0;
        }
        else
        {
            i++;
        }
        timeout++;
    }
    while ((lockMask != lockTarget) && (timeout < 10000));

    if (timeout >= 10000)
    {
        FW_MBOX_WR32(6, getDisabledPartitionMask());
        FW_MBOX_WR32(7, getPartitionMask());
        FW_MBOX_WR32(11, gNumberOfPartitions);
        FW_MBOX_WR32(12, lockTarget);
        FW_MBOX_WR32(13, lockMask);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_NO_LOCK);
    }
    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    return;
}

void
gddr_clear_rpre
(
        void
)
{
    LwU32 fbpa_config2;

    fbpa_config2 = new_reg->pfb_fbpa_config2;
    fbpa_config2 = FLD_SET_DRF_NUM(_PFB, _FBPA_CONFIG2, _RPRE,0,fbpa_config2);
    new_reg->pfb_fbpa_config2 = fbpa_config2;
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG2,fbpa_config2);
    return;
}

void
gddr_pgm_fbio_cfg_pwrd
(
        void
)
{
    LwU32 fbio_cfg_pwrd;

    //FOLLOWUP need to add RM flag - unsure about the source of RM Flag PDB_PROP_FB_USE_WINIDLE_PWRDS_AND_HYBRID_PADS
    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        fbio_cfg_pwrd = lwr_reg->pfb_fbpa_fbio_cfg_pwrd;
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CML_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _INTERP_PWRD,_DISABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DIFFAMP_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_TX_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQDQS_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _QUSE_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CK_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_DIFFAMP_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_RX_GDDR5_PWRD, _DISABLE,fbio_cfg_pwrd);
        //Confirm with Ish - needed for P0?
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);

        gddr_pgm_dq_term();

        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_GDDR5_PWRD, _DISABLE,fbio_cfg_pwrd);
        lwr_reg->pfb_fbpa_fbio_cfg_pwrd = fbio_cfg_pwrd;
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);
    }
    return;
}

void
gddr_bypass_drampll
(
        LwS32 bypass_en
)
{
    LwU32 drampll_config;
    drampll_config = lwr_reg->pfb_fbpa_fbio_drampll_config;
    if (bypass_en) {
        drampll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _BYPASS,_ENABLE,drampll_config);
    } else {
        drampll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _BYPASS,_DISABLE,drampll_config);
    }
    lwr_reg->pfb_fbpa_fbio_drampll_config = drampll_config;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);
    return;
}

void
gddr_bypass_refmpll
(
        LwS32 bypass_en
)
{
    LwU32 refmpll_config;
    refmpll_config = lwr_reg->pfb_fbpa_fbio_refmpll_config;
    if (bypass_en) {
        refmpll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _BYPASS, _ENABLE,refmpll_config);
    } else {
        refmpll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _BYPASS, _DISABLE,refmpll_config);
    }
    lwr_reg->pfb_fbpa_fbio_refmpll_config = refmpll_config;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
    return;
}

void
gddr_en_sync_mode_refmpll
(
        void
)
{
    LwU32 refmpll_config;
    refmpll_config = lwr_reg->pfb_fbpa_fbio_refmpll_config;
    refmpll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _SYNC_MODE, _ENABLE,refmpll_config);
    lwr_reg->pfb_fbpa_fbio_refmpll_config = refmpll_config;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
    return;
}

void
gddr_clk_switch_drampll_2_onesrc
(
        void
)
{

    // //clamp the Drampll, NEEDED?
    // fbio_mode_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMPLL_STOP_SYNCMUX,1,fbio_mode_switch);
    //clamp the mode switch
    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_ENABLE,fbio_mode_switch);
    //unclamp onesrc
    //fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP,_DISABLE,1);
    //switch to onesrc
    REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL,fbio_mode_switch);
    REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    OS_PTIMER_SPIN_WAIT_NS(100);

    //bypass drampll
    gddr_bypass_drampll(1);
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1);

    OS_PTIMER_SPIN_WAIT_NS(100);

    //unclamp drampll
    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_DISABLE,fbio_mode_switch);
    REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    OS_PTIMER_SPIN_WAIT_US(2);

    //disable drampll as the target path is not cascaded
    gddr_disable_drampll();
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x2);

    OS_PTIMER_SPIN_WAIT_NS(100);

    //    //clamp refmpll
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX,_ENABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    //bypass refmpll
    gddr_bypass_refmpll(1);
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x3);

    OS_PTIMER_SPIN_WAIT_NS(100);

    //    //unclamp refmpll
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX,_DISABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    return;
}

void
gddr_clk_switch_onesrc_2_refmpll
(
        void
)
{
    OS_PTIMER_SPIN_WAIT_NS(5);

    //Enable sync mode on refmpll
    gddr_en_sync_mode_refmpll();

    //    //clamp refmpll if used in requested clk path
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _ENABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    //disable the refmpll bypass
    gddr_bypass_refmpll(0);
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x4);

    OS_PTIMER_SPIN_WAIT_NS(100);

    //    //unclamp refmpll
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH,_REFMPLL_STOP_SYNCMUX,_DISABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    return;
}

void
gddr_clk_switch_refmpll_2_onesrc
(
        void
)
{

    //Enable sync mode on refmpll
    gddr_en_sync_mode_refmpll();

    //    //clamp refmpll
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _ENABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    //unclamp onesource
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP,_DISABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    OS_PTIMER_SPIN_WAIT_NS(100);

    //bypass refmpll
    gddr_bypass_refmpll(1);
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x5);

    //switch to onesrc
    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _ONESOURCE,fbio_mode_switch);

    OS_PTIMER_SPIN_WAIT_NS(100);

    //    //unclamp refmpll
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _DISABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    return;
}

void
gddr_clk_switch_refmpll_2_drampll
(
        void
)
{
    OS_PTIMER_SPIN_WAIT_NS(100);

    //clamp the Drampll
    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_ENABLE,fbio_mode_switch);
    REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    OS_PTIMER_SPIN_WAIT_NS(100);

    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(2),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    //disable drampll bypass
    gddr_bypass_drampll(0);
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x6);

    OS_PTIMER_SPIN_WAIT_NS(100);

    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(3),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _DRAMPLL, fbio_mode_switch);
    REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    //unclamp drampll
    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_DISABLE,fbio_mode_switch);
    REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    OS_PTIMER_SPIN_WAIT_US(2);

    REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_1(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    return;
}

void gddr_reset_testcmd_ptr(void) {
    LwU32 ptr_reset;

    ptr_reset = lwr_reg->pfb_fbpa_ptr_reset;
    ptr_reset = FLD_SET_DRF(_PFB, _FBPA_PTR_RESET, _TESTCMD, _ON,ptr_reset);
    lwr_reg->pfb_fbpa_ptr_reset = ptr_reset;
    REG_WR32(LOG, LW_PFB_FBPA_PTR_RESET,ptr_reset);

    //Wait for 1us to help the reset propogate
    OS_PTIMER_SPIN_WAIT_US(1);

    //de-asserting ptr_reset
    ptr_reset = lwr_reg->pfb_fbpa_ptr_reset;
    ptr_reset = FLD_SET_DRF(_PFB, _FBPA_PTR_RESET, _TESTCMD, _OFF,ptr_reset);
    lwr_reg->pfb_fbpa_ptr_reset = ptr_reset;
    REG_WR32(LOG, LW_PFB_FBPA_PTR_RESET,ptr_reset);

    //Wait for 1us to help the reset propogate
    OS_PTIMER_SPIN_WAIT_US(1);
    return;
}

void
gddr_pgm_cmd_brlshift
(
        void
)
{

    LwU32 cmd_brlshift;
    LwU32 fbio_cmd_delay;
    LwU32 brlshift_legacy;

    brlshift_legacy = REF_VAL(LW_PFB_FBPA_FBIO_CFG1_CMD_BRLSHFT,lwr_reg->pfb_fbpa_fbio_cfg1);
    cmd_brlshift = lwr_reg->pfb_fbpa_fbio_brlshft;
    fbio_cmd_delay = lwr_reg->pfb_fbpa_fbio_cmd_delay;

    if (brlshift_legacy == LW_PFB_FBPA_FBIO_CFG1_CMD_BRLSHFT_BIT) {
        LwU8 CmdScriptPtrIndex = gTT.perfMemClkBaseEntry.Flags2; //cmd_script_ptr_index
        fbio_cmd_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CMD_DELAY, _CMD_SRC,CmdScriptPtrIndex,fbio_cmd_delay);
        fbio_cmd_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CMD_DELAY, _CMD_PRIV_SRC,CmdScriptPtrIndex,fbio_cmd_delay);
        lwr_reg->pfb_fbpa_fbio_cmd_delay = fbio_cmd_delay;
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_CMD_DELAY,fbio_cmd_delay);
    } else {

        if ((gbl_g5x_cmd_mode) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
            cmd_brlshift = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BRLSHFT, _CMD,4,cmd_brlshift);
        } else if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            cmd_brlshift = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BRLSHFT, _CMD,6,cmd_brlshift);
        } else {
            cmd_brlshift = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BRLSHFT, _CMD,2,cmd_brlshift);
        }
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_BRLSHFT,cmd_brlshift);
    }

    return;
}

void
gddr_en_dq_term_save
(
        void
)
{
    LwU32 fbio_cfg10;
    fbio_cfg10 = lwr_reg->pfb_fbpa_fbio_cfg10;
    gbl_dq_term_mode_save = REF_VAL(LW_PFB_FBPA_FBIO_CFG10_DQ_TERM_MODE,fbio_cfg10);
    fbio_cfg10 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE ,_PULLUP,fbio_cfg10);
    lwr_reg->pfb_fbpa_fbio_cfg10 = fbio_cfg10;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG10,fbio_cfg10);
    return;
}

void
gddr_restore_dq_term
(
        void
)
{
    LwU32 fbio_cfg10;
    fbio_cfg10 = lwr_reg->pfb_fbpa_fbio_cfg10;
    fbio_cfg10 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG10, _DQ_TERM_MODE, gbl_dq_term_mode_save,fbio_cfg10);
    lwr_reg->pfb_fbpa_fbio_cfg10 = fbio_cfg10;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG10,fbio_cfg10);
    return;
}

void
gddr_disable_edc_tracking
(
        void
)
{
    LwU32 fbpa_fbio_edc_tracking;
    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_prev_edc_tr_enabled == 1)) {
        fbpa_fbio_edc_tracking = lwr_reg->pfb_fbpa_fbio_edc_tracking;
        fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,_EN, 0,fbpa_fbio_edc_tracking);
        LwU32 edc_hold_pattern = 0xf;
        new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB,       _FBPA_GENERIC_MRS4,   _ADR_GDDR5_EDC_HOLD_PATTERN ,    edc_hold_pattern,   new_reg->pfb_fbpa_generic_mrs4);
        lwr_reg->pfb_fbpa_fbio_edc_tracking = fbpa_fbio_edc_tracking;
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,fbpa_fbio_edc_tracking);
        lwr_reg->pfb_fbpa_training_edc_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_EDC_CTRL, _EDC_TRACKING_MODE,0, lwr_reg->pfb_fbpa_training_edc_ctrl);
        REG_WR32(LOG, LW_PFB_FBPA_TRAINING_EDC_CTRL,lwr_reg->pfb_fbpa_training_edc_ctrl);

        //disabling Vref tracking
        lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _EN, 0,lwr_reg->pfb_fbpa_fbio_vref_tracking);
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,lwr_reg->pfb_fbpa_fbio_vref_tracking);

    }
    return;
}

void
gddr_edc_tr_dbg_ref_recenter
(
        LwU32 val
)
{
    LwU32 edc_tr_dbg;
    LwU32 vref_tr;
    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) &&(gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)) {
        edc_tr_dbg = lwr_reg->pfb_fbpa_fbio_edc_tracking_debug;
        vref_tr    = lwr_reg->pfb_fbpa_fbio_vref_tracking;
        if ((val ==1) && ((gblEnEdcTracking) || (gblElwrefTracking))) {
            vref_tr    = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _DISABLE_TRAINING_UPDATES, 1,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            edc_tr_dbg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING_DEBUG, _DISABLE_REFRESH_RECENTER, 1,edc_tr_dbg);
        } else if ((val == 0) && ((gblEnEdcTracking) || (gblElwrefTracking))) {
            lwr_reg->pfb_fbpa_fbio_edc_tracking	 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _DISABLE_TRAINING_UPDATES, 1,   lwr_reg->pfb_fbpa_fbio_edc_tracking);
            edc_tr_dbg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING_DEBUG, _DISABLE_REFRESH_RECENTER, 0,edc_tr_dbg);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,lwr_reg->pfb_fbpa_fbio_edc_tracking);
        }
        else {
            lwr_reg->pfb_fbpa_fbio_edc_tracking	 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,        _DISABLE_TRAINING_UPDATES, 0,   lwr_reg->pfb_fbpa_fbio_edc_tracking);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,lwr_reg->pfb_fbpa_fbio_edc_tracking);
            vref_tr    = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _DISABLE_TRAINING_UPDATES, 0,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            edc_tr_dbg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING_DEBUG, _DISABLE_REFRESH_RECENTER, 0,edc_tr_dbg);
        }
        lwr_reg->pfb_fbpa_fbio_edc_tracking_debug = edc_tr_dbg;
        lwr_reg->pfb_fbpa_fbio_vref_tracking = vref_tr;
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG, edc_tr_dbg);
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING, vref_tr);

    }
}

void
gddr_save_clr_fbio_pwr_ctrl
(
        void
)
{
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_PWR_CTRL,0x00000000);
    return;
}

void
gddr_restore_fbio_pwr_ctrl
(
        void
)
{
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_PWR_CTRL,lwr_reg->pfb_fbpa_fbio_pwr_ctrl);
    return;
}

void
gddr_pgm_clk_pattern
(
        void
)
{
    LwU32 wck_pattern;
    LwU32 ck_pattern;
    LwU32 fbpa_cfg0;
    LwU32 fbiotrng_wck;
    LwU32 fbio_clk_ctrl;
    LwU32 fbpa_training_ctrl;
    LwU32 trng_ctrl_wck_pads;

    fbpa_training_ctrl = lwr_reg->pfb_fbpa_training_ctrl;
    trng_ctrl_wck_pads = REF_VAL(LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS, fbpa_training_ctrl);

    if ((gbl_g5x_cmd_mode) && (gbl_vendor_id == MEMINFO_ENTRY_VENDORID_MICRON)) {
        if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            wck_pattern = 0x33;
            ck_pattern = 0x0f;
        } else {
            wck_pattern = 0xcc;
            ck_pattern = 0x3c;
        }
    }
    else {
        wck_pattern = 0x55;
        if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            ck_pattern = 0x33;
        } else {
            ck_pattern = 0x3c;
        }
    }

    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    if ((gbl_g5x_cmd_mode) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
        //fbpa_cfg0
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _BL, _BL16,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _TIMING_UNIT, _DRAMDIV2,fbpa_cfg0);
    } else {
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _BL, _BL8,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _TIMING_UNIT, _DRAMDIV2,fbpa_cfg0);
    }
    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

    //fbpa_fbiotrng_subp0_wck
    fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp0_wck;
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCK, _CTRL_BYTE01,wck_pattern,fbiotrng_wck);
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCK, _CTRL_BYTE23,wck_pattern,fbiotrng_wck);
    lwr_reg->pfb_fbpa_fbiotrng_subp0_wck = fbiotrng_wck;
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCK,fbiotrng_wck);

    if (trng_ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
        //fbpa_fbiotrng_subp0_wckb
        fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp0_wckb;
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCKB, _CTRL_BYTE01, wck_pattern,fbiotrng_wck);
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCKB, _CTRL_BYTE23, wck_pattern,fbiotrng_wck);
        lwr_reg->pfb_fbpa_fbiotrng_subp0_wckb = fbiotrng_wck;
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCKB,fbiotrng_wck);
    }

    //fbpa_fbiotrng_subp1_wck
    fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp1_wck;
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCK, _CTRL_BYTE01,wck_pattern,fbiotrng_wck);
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCK, _CTRL_BYTE23,wck_pattern,fbiotrng_wck);
    lwr_reg->pfb_fbpa_fbiotrng_subp1_wck = fbiotrng_wck;
    REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCK,fbiotrng_wck);

    if (trng_ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
        //fbpa_fbiotrng_subp1_wckb
        fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp1_wckb;
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCKB, _CTRL_BYTE01, wck_pattern,fbiotrng_wck);
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCKB, _CTRL_BYTE23, wck_pattern,fbiotrng_wck);
        lwr_reg->pfb_fbpa_fbiotrng_subp1_wckb = fbiotrng_wck;
        REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCKB,fbiotrng_wck);
    }

    //fbio_clk_ctrl
    fbio_clk_ctrl = lwr_reg->pfb_fbpa_clk_ctrl;
    fbio_clk_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CLK_CTRL, _CLK0, ck_pattern, fbio_clk_ctrl);
    fbio_clk_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CLK_CTRL, _CLK1, ck_pattern, fbio_clk_ctrl);
    lwr_reg->pfb_fbpa_clk_ctrl = fbio_clk_ctrl;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CLK_CTRL,fbio_clk_ctrl);

    return;
}


void
gddr_update_arb_registers
(
        void
)
{
    LwU32 fbpa_act_arb_cfg2;
    LwU32 fbpa_read_bankq_cfg;
    LwU32 fbpa_write_bankq_cfg;
    LwU32 fbpa_read_ft_cfg;
    LwU32 fbpa_write_ft_cfg;
    LwU32 fbpa_dir_arb_cfg0;
    LwU32 fbpa_dir_arb_cfg1;

    fbpa_act_arb_cfg2   = REG_RD32(LOG, LW_PFB_FBPA_ACT_ARB_CFG2);
    fbpa_read_bankq_cfg = REG_RD32(LOG, LW_PFB_FBPA_READ_BANKQ_CFG);
    fbpa_write_bankq_cfg = REG_RD32(LOG, LW_PFB_FBPA_WRITE_BANKQ_CFG);
    fbpa_read_ft_cfg    = REG_RD32(LOG, LW_PFB_FBPA_READ_FT_CFG);
    fbpa_write_ft_cfg   = REG_RD32(LOG, LW_PFB_FBPA_WRITE_FT_CFG);
    fbpa_dir_arb_cfg0   = REG_RD32(LOG, LW_PFB_FBPA_DIR_ARB_CFG0);
    fbpa_dir_arb_cfg1   = REG_RD32(LOG, LW_PFB_FBPA_DIR_ARB_CFG1);

    //MAX READS/WRITES in flight
    fbpa_act_arb_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBPA_ACT_ARB_CFG2, _MAX_READS_IN_FLIGHT,
            gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0MRIF,fbpa_act_arb_cfg2);
    fbpa_act_arb_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBPA_ACT_ARB_CFG2, _MAX_WRITES_IN_FLIGHT,
            gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0MWIF,fbpa_act_arb_cfg2);

    //READ_BANKQ_HIT
    if (gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0RBH) {
        fbpa_read_bankq_cfg = FLD_SET_DRF(_PFB, _FBPA_READ_BANKQ_CFG, _BANK_HIT, _ENABLED,fbpa_read_bankq_cfg);
    } else {
        fbpa_read_bankq_cfg = FLD_SET_DRF(_PFB, _FBPA_READ_BANKQ_CFG, _BANK_HIT, _DISABLED,fbpa_read_bankq_cfg);
    }

    //WRITE_BANKQ_HIT
    if (gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0WBH) {
        fbpa_write_bankq_cfg = FLD_SET_DRF(_PFB, _FBPA_WRITE_BANKQ_CFG, _BANK_HIT, _ENABLED,fbpa_write_bankq_cfg);
    } else {
        fbpa_write_bankq_cfg = FLD_SET_DRF(_PFB, _FBPA_WRITE_BANKQ_CFG, _BANK_HIT, _DISABLED,fbpa_write_bankq_cfg);
    }

    //Max sorted read_chain
    fbpa_read_ft_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_READ_FT_CFG, _MAX_CHAIN,
            gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0MSRC,fbpa_read_ft_cfg);

    //Forcing Max chain for p5 and p8 to 0x7
    if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
        fbpa_read_ft_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_READ_FT_CFG, _MAX_CHAIN,
            0x7,fbpa_read_ft_cfg);
    }

    //max sorted write on clean
    if (gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0SWSOC) {
        fbpa_write_ft_cfg = FLD_SET_DRF(_PFB, _FBPA_WRITE_FT_CFG, _EVICT_ON_CLEAN, _ENABLED,fbpa_write_ft_cfg);
    } else {
        fbpa_write_ft_cfg = FLD_SET_DRF(_PFB, _FBPA_WRITE_FT_CFG, _EVICT_ON_CLEAN, _DISABLED,fbpa_write_ft_cfg);
    }


    //complete write snapshot
    if (gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0CWS) {
        fbpa_dir_arb_cfg1 = FLD_SET_DRF(_PFB, _FBPA_DIR_ARB_CFG1, _COMPLETE_SNAP_WR, _ENABLED,fbpa_dir_arb_cfg1);
    } else {
        fbpa_dir_arb_cfg1 = FLD_SET_DRF(_PFB, _FBPA_DIR_ARB_CFG1, _COMPLETE_SNAP_WR, _DISABLED,fbpa_dir_arb_cfg1);
    }

    //ISO block refreshes
    if(gTT.perfMemClkBaseEntry.FBPAConfig0.PMCFBPAC0IBR) {
        fbpa_dir_arb_cfg1 = FLD_SET_DRF(_PFB, _FBPA_DIR_ARB_CFG1, _VC1_BLOCKS_SPSM, _ENABLED,fbpa_dir_arb_cfg1);
    } else {
        fbpa_dir_arb_cfg1 = FLD_SET_DRF(_PFB, _FBPA_DIR_ARB_CFG1, _VC1_BLOCKS_SPSM, _DISABLED,fbpa_dir_arb_cfg1);
    }


    //READ_limit
    fbpa_dir_arb_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG0, _READ_LIMIT,
            gTT.perfMemClkBaseEntry.FBPAConfig1.PMCFBPAC1RL,fbpa_dir_arb_cfg0);
    //Write_limit
    fbpa_dir_arb_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG0, _WRITE_LIMIT,
            gTT.perfMemClkBaseEntry.FBPAConfig1.PMCFBPAC1WL,fbpa_dir_arb_cfg0);
    //Min Reads per turn
    fbpa_dir_arb_cfg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG1, _MIN_READ_INT_PER_TURN,
            gTT.perfMemClkBaseEntry.FBPAConfig1.PMCFBPAC1MRPT, fbpa_dir_arb_cfg1); 
    //Min Writes per turn
    fbpa_dir_arb_cfg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG1, _MIN_WRITE_INT_PER_TURN,
            gTT.perfMemClkBaseEntry.FBPAConfig1.PMCFBPAC1MWPT, fbpa_dir_arb_cfg1);
    //read_latency_offset
    fbpa_dir_arb_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG0, _READ_LATENCY_OFFSET,
            gTT.perfMemClkBaseEntry.FBPAConfig1.PMCFBPAC1RLO,fbpa_dir_arb_cfg0);
    //write_latency_offset
    fbpa_dir_arb_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG0, _WRITE_LATENCY_OFFSET,
            gTT.perfMemClkBaseEntry.FBPAConfig1.PMCFBPAC1WLO,fbpa_dir_arb_cfg0);


    REG_WR32(LOG, LW_PFB_FBPA_ACT_ARB_CFG2,fbpa_act_arb_cfg2);
    REG_WR32(LOG, LW_PFB_FBPA_READ_BANKQ_CFG,fbpa_read_bankq_cfg);
    REG_WR32(LOG, LW_PFB_FBPA_WRITE_BANKQ_CFG,fbpa_write_bankq_cfg);
    REG_WR32(LOG, LW_PFB_FBPA_READ_FT_CFG,fbpa_read_ft_cfg);
    REG_WR32(LOG, LW_PFB_FBPA_WRITE_FT_CFG,fbpa_write_ft_cfg);
    REG_WR32(LOG, LW_PFB_FBPA_DIR_ARB_CFG0,fbpa_dir_arb_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_DIR_ARB_CFG1,fbpa_dir_arb_cfg1);

    return;
}

void
gddr_pgm_pad_ctrl2
(
        void
)
{
    lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _E_RX_DFE, GblRxDfe,lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2,lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2);
    return;
}

void
gddr_pgm_cmd_fifo_depth
(
        void
)
{
    LwU32 fbpa_spare;
    LwU16 idx;
    //    FW_MBOX_WR32(8, 0xABABABAB);
    for (idx = 0; idx < 6; idx++)
    {
        if (isPartitionEnabled(idx))
        {
            fbpa_spare = REG_RD32(LOG, unicast(LW_PFB_FBPA_SPARE,idx));
            if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) {
                fbpa_spare = FLD_SET_DRF(_PFB, _FBPA_SPARE, _BITS_CMDFF_DEPTH, _SAFE, fbpa_spare);
            } else  {
                fbpa_spare = FLD_SET_DRF(_PFB, _FBPA_SPARE, _BITS_CMDFF_DEPTH, _MAX, fbpa_spare);
            }
            REG_WR32_STALL(LOG, unicast(LW_PFB_FBPA_SPARE,idx), fbpa_spare);
            fbpa_spare = REG_RD32(LOG, unicast(LW_PFB_FBPA_SPARE,idx));
        }
    }
    return;
}

void
gddr_pgm_cfg_pwrd_war
(
        PerfMemClk11Header* pMCHp
)
{
    LwU32 cfg_pwrd;

    cfg_pwrd = TABLE_VAL(pMCHp->CFG_PWRD);

    if (cfg_pwrd != 0) {
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,cfg_pwrd);
    }

    return;
}

void
gddr_pgm_gpio_fbvdd
(
        void
)
{
    LwU32 GpioVddReg;
    LwU32 FbVddSettleTime;
    LwU8 FbvddGpioIndex;

    FbvddGpioIndex = memoryGetGpioIndexForFbvddq_HAL();
    PerfMemClk11Header* pMCHp = gBiosTable.pMCHp;

    //check for cfg pwrd war
    gddr_pgm_cfg_pwrd_war(pMCHp);

    //obtain the mutex for accessing GPIO register
    LwU8 token = 0;

    MUTEX_HANDLER fbvddgpioMutex;
    fbvddgpioMutex.registerMutex = LW_PPWR_PMU_MUTEX(12);
    fbvddgpioMutex.registerIdRelease = LW_PPWR_PMU_MUTEX_ID_RELEASE    ;
    fbvddgpioMutex.registerIdAcquire = LW_PPWR_PMU_MUTEX_ID;
    fbvddgpioMutex.valueInitialLock = LW_PPWR_PMU_MUTEX_VALUE_INITIAL_LOCK;
    fbvddgpioMutex.valueAcquireInit = LW_PPWR_PMU_MUTEX_ID_VALUE_INIT;
    fbvddgpioMutex.valueAcquireNotAvailable = LW_PPWR_PMU_MUTEX_ID_VALUE_NOT_AVAIL;
    LW_STATUS status = mutexAcquireByIndex_GV100(&fbvddgpioMutex, MUTEX_REQ_TIMEOUT_NS, &token);

    if(status != LW_OK) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_MUTEX_NOT_OBTAINED);
    }

    GpioVddReg = REG_RD32(LOG,(LW_PMGR_GPIO_OUTPUT_CNTL(FbvddGpioIndex)));

    //https://rmopengrok.lwpu.com/source/xref/chips_a/drivers/resman/kernel/clk/fermi/clkgf117.c#3924
    if (gbl_lwrr_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW) {
        GpioVddReg = FLD_SET_DRF(_PMGR,_GPIO_OUTPUT_CNTL, _IO_OUTPUT,_0, GpioVddReg);
    } else {
        GpioVddReg = FLD_SET_DRF(_PMGR,_GPIO_OUTPUT_CNTL, _IO_OUTPUT,_1, GpioVddReg);
    }
    GpioVddReg = FLD_SET_DRF(_PMGR,_GPIO_OUTPUT_CNTL, _IO_OUT_EN,_YES, GpioVddReg);

    REG_WR32_STALL(LOG, LW_PMGR_GPIO_OUTPUT_CNTL(8),GpioVddReg);

    //update trigger and wait for trigger done
    gddr_update_gpio_trigger_wait_done();

    //release the mutex
    status = mutexReleaseByIndex_GV100(&fbvddgpioMutex, token);
    if(status != LW_OK) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_GPIO_MUTEX_NOT_RELEASED);
    }

    //settle time
    FbVddSettleTime = TABLE_VAL(pMCHp->FBVDDSettleTime);
    OS_PTIMER_SPIN_WAIT_US(FbVddSettleTime);

    return;
}

void
gddr_pgm_save_vref_val
(
        void
)
{
    LwU32 dq_vref;
    LwU32 dqs_vref;

    //FOLLOWUP : Needs Vbios update - using spare field for now
    //spare_field3[7:0] Dq_vref
    //spare_field[15:8] Dqs_vref
    dq_vref  =  REF_VAL(LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1_DQ_VREF_CODE,gTT.perfMemClkBaseEntry.spare_field3);
    dqs_vref =  REF_VAL(LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1_DQS_VREF_CODE,gTT.perfMemClkBaseEntry.spare_field3);

    //save current vref value
    //reading values from VREF code3, pad8 - Dq, Pad9 - EDC (wdqs)
    lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
    GblSaveDqVrefVal  = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD8,lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3);
    GblSaveDqsVrefVal = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD9,lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3);

    //ensure dq_vref is actually configured else load with 70% (0x48) vref
    if (dq_vref != 0x00) {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_CODE, dq_vref,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    } else {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_CODE, 0x4D,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    }
    //Dqs vref
    if (dqs_vref != 0x00) {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQS_VREF_CODE, dqs_vref,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    } else {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQS_VREF_CODE, 0x48,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    }
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1,lwr_reg->pfb_fbio_delay_broadcast_misc1);

    return;
}

void
gddr_restore_vref_val
(
        void
)
{
    lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_CODE, GblSaveDqVrefVal,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQS_VREF_CODE, GblSaveDqsVrefVal,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    return;
}

LwU32
gddr_mclk_switch
(
        LwBool bPreAddressBootTraining,
        LwBool bPostAddressBootTraining,
        LwBool bPostWckRdWrBootTraining
)
{
    LwU32           fbStopTimeNs      = 0;

    if (!bPostAddressBootTraining && !bPostWckRdWrBootTraining) {
        // This portion needs to be exelwted if this is doMclkSwitchInSingleStage or doMclkSwitchInMultipleStages with bPreAddressBootTraining set.
        LwU32 fbio_ddll_cal_ctrl;
        LwU32 fbio_ddll_cal_ctrl1;
        //LwU32 fbio_addr_ddll;

        //step 3:
        //read current clock source
        gddr_lwrr_clk_source = REG_RD32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
        lwr_reg->pfb_fbpa_fbio_broadcast = REG_RD32(LOG, LW_PFB_FBPA_FBIO_BROADCAST);
        gbl_ddr_mode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast);
        gddr_get_target_clk_source();

        //ignoring the callwlation of current freq as it is required only for reporting
        //gddr_retrieve_lwrrent_freq();
        //includes step 5 to callwlate onesrc divider value.
        gddr_callwlate_target_freq_coeff();

        //step 4:
        gddr_load_reg_values();

        //step 9:
        gddr_calc_exp_freq_reg_values();

        if ((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce) &&
                (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
            FuncLoadAdrPatterns(gbl_ddr_mode);
        }

        //step 13 //FOLLOWUP : accessing PLL bypass registers -> to be performed after fb_stop. - change for document

        //step 15 // callwlate time needed for mlk switch
        //based on priv profilining and time consumption for each priv access the read modified write can
        //be made to initial read and shadow the write values in the register.
        gddr_power_on_internal_vref();

        //Step 16 - Read ACPD
        gbl_dram_acpd = REF_VAL(LW_PFB_FBPA_CFG0_DRAM_ACPD,lwr_reg->pfb_fbpa_cfg0);
        //gbl_dram_asr = REG_RD32(LOG, LW_PFB_FBPA_DRAM_ASR);

        //FB STOP
        //Step 17 - FB_stop
        memoryStopFb_HAL(&Fbflcn, 1, &startFbStopTimeNs);
        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

        ////reusing gv100 function
        //stop_fb();

        //update MRS1 soon entering FB_STOP for termination - from pascal MRS review
        //changed the sequence from Pascal
        //Updating the Addr termination always at low speed clock for high speed
        //target frequency
        if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
            if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1,new_reg->pfb_fbpa_generic_mrs1);
            } else {
                REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS8,new_reg->pfb_fbpa_generic_mrs8);
            }
        }

        //New addition - gddr6
        gddr_disable_edc_tracking();

        //Step 18 - Skipped : EMRS fields accessed.
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) 
            {
                    REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1,new_reg->pfb_fbpa_generic_mrs1);
                    gddr_flush_mrs_reg();
            }
            //step 21 -
            // Addr Term gets update for G6 just after FbStop for G6 for low to high speed change. Vref needs to get updated just after termination
            // However, for a high speed to low speed change, Addr Term gets updated just before fbStart. Vref will get updated at that point.
            // G5 will follow same sequence for ease of code maintenance
            if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) 
            {
                gddr_set_fbvref();
            }
        }

        //Step 22 - Disable ACPD and REPLAY
        gddr_disable_acpd_and_replay();

        //Step 24 - Disable Refresh
        gddr_disable_refresh(); // FOLLOWUP:wait for 1us??

        OS_PTIMER_SPIN_WAIT_US(1);

        //Step 25 - Disable periodic training
        gddr_disable_periodic_training();

        //Step 26 - Send one refresh command
        gddr_send_one_refresh();

        //FOLLOWUP: wait for 1us??
        //Step 27 -
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_set_memvref_range();
        }

        //DRAM Enters Self Refresh
        //Step 28/29
        //Issue Precharge command
        gddr_precharge_all();

        OS_PTIMER_SPIN_WAIT_US(1);

        //Enable self refresh via testcmd and wait for 1us
        //Reset the testcmd pointers before entering to self refresh for G5x and G6
        gddr_reset_testcmd_ptr();
        gddr_enable_self_refresh();

        OS_PTIMER_SPIN_WAIT_US(1);

        //Clear WCK per Byte trimmers
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_clear_wck_trimmer();
            //Pgm rx dfe based on vbios
            gddr_pgm_pad_ctrl2();
        }

        //Power down/up the Interpolators/DLL based on existing clock path and requested clock path
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_power_intrpl_dll();
        }

        //CYA Step
        //Resetting Entry wdat_ptr and edc_ptr for some % of randoms
        ////        FW_MBOX_WR32(4, 0x00000622);
        //        REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xAAFBFEED);
        ////        FW_MBOX_WR32(2, 0x00000622);

        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

        fbio_mode_switch = REG_RD32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH);

        //MODE 1 : High speed to low/Mid speed
        //DRAMPLL --> REFMPLL or ONESRC
        //cascaded to non cascaded
        if ((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) &&
                (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH)) {

            //Change to vml mode
            //not required for turing
            //gddr_set_vml_mode();

            //step 31
            //setting RPRE, R2W_BUS_MIN to 1 due to bug - 510373 - done for all mem types
    	    gddr_set_rpre_r2w_bus();

            //step 32.4
            //Disable barrel shifters
            gddr_pgm_brl_shifter(0);
            //add function to program (dq,rdqs,qdqs) term mode -review

            //step 32.5A
            //Enable Low_power for g5, Disable wredc, rdedc and replay
            gddr_disable_low_pwr_edc();

            //step 32.5B
            //Disable WRCRC and RDCRC for dram through MRS command
            //check mrs commands cannot pass thru here
            gddr_disable_crc_mrs4();

            //step 32.6
            //Power down wdqs interpolators since edc pins will not be used in CMOS mode
            gddr_power_down_edc_intrpl();

            //step 32.7
            //Reset all the interpolators to a Zero phase
            //reset all IB_PADMACRO_PTR_OFFSET(to clear the inbound fifo after a clock switch).
            //Update extended quse settings
            gddr_clr_intrpl_padmacro_quse();

            //step 32.8
            //clear IB_PADMACRO_PTR_OFFSET
            gddr_clr_ib_padmacro_ptr();

            REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x7);
            OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole
            //step 32.11

            //mclk time profiling
            REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

            //clk switch drampll to onesrc
            gddr_clk_switch_drampll_2_onesrc();
            //            FW_MBOX_WR32(1, 0x00000823);

            if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) || (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) {
                gddr_pgm_refmpll_cfg_and_lock_wait();

                //clock switch from one source to refmpll
                gddr_clk_switch_onesrc_2_refmpll();
            } else { //Target path ONESRC
                //disable refmpll
                gddr_disable_refmpll();
            }

            //step 32.13
            //CMOS_CLK_SEL
            //gddr_enable_cmos_cml_clk(0);

            //change the WDQS receiver sel based on vbios
            gddr_pgm_wdqs_rcvr_sel();

            //Switch the dramclk mode and unclamp the clocks
            if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) ||(gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) {
                //switch to REFMPLL and unclamp refmpll
                fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL,fbio_mode_switch);
                fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _DISABLE,fbio_mode_switch);
            }
            else {
                //switch to onesrc and unclamp onesrc
                fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _ONESOURCE,fbio_mode_switch);
                fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP, _DISABLE,fbio_mode_switch);
            }

            REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

            //step 32.16
            //enable_padmacro_sync
            //release ptr offset
            gddr_cfg_padmacro_sync_ptr_offset();

            ////Additional Step
            //gddr_disable_low_pwr_en_edc_early();
        }

        //MODE 2 : Low speed to Mid speed or vice versa or Mid Speed to Mid Speed
        //ONESRC <--> REFMPLL
        else if (((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_ONESOURCE ) &&
                (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)) ||
                ((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) &&
                        (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) ||
                        ((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) &&
                                (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)) ||
                                ((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_ONESOURCE) &&
                                        (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH))) {

            if (gbl_bFbVddSwitchNeeded || gbl_bVauxChangeNeeded)
            {
                // skip the pll update when doing the vvdd voltage switch, which is a low speed to low speed
                // switch at the same frequency
            }
            else 
            {
                REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x8);
                OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole

                REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
                //clk_switch refmpll to onesrc
                gddr_clk_switch_refmpll_2_onesrc();

                if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) ||(gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) {
                    //programming refmpll config reg and wait for lock
                    gddr_pgm_refmpll_cfg_and_lock_wait();
                    //clk switch onesrc to refmpll
                    gddr_clk_switch_onesrc_2_refmpll();
                } else {
                    //program onesrc coefficients
                }

                //change the WDQS receiver sel based on vbios
                gddr_pgm_wdqs_rcvr_sel();

                //Switch the dramclk mode and unclamp the clocks
                if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) ||(gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) {
                    //switch to REFMPLL and unclamp refmpll
                    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL,fbio_mode_switch);
                    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX,_DISABLE,fbio_mode_switch);
                }
                else {
                    //switch to onesrc and unclamp onesrc
                    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE,_ONESOURCE,fbio_mode_switch);
                    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP,_DISABLE,fbio_mode_switch);
                }

                REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

                //FB unit sim only
                //            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,0xFBFBFEED);
                //            REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,gbl_mclk_freq_new); //MHz
            }

            //Step 34: Update VDDQ
            if (gbl_bFbVddSwitchNeeded) {
                //program actual fbvdd
                gddr_pgm_gpio_fbvdd();
            }

            //VAUXP, VCLAMP, VAUXC programming
            if (gbl_bVauxChangeNeeded) {
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL1,new_reg->pfb_fbpa_fbio_vtt_ctrl1);
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL0,new_reg->pfb_fbpa_fbio_vtt_ctrl0);
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN,  new_reg->pfb_fbpa_fbio_cfg_brick_vauxgen);
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN, new_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen);
                REG_WR32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN, new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
                //100ns settle time for vauxc
                OS_PTIMER_SPIN_WAIT_NS(100);
            }

            //Additional Step
            gddr_disable_low_pwr_en_edc_early();
        }

        //MODE 3 : Low speed to Low speed
        //ONESRC to ONESRC
        else if ((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_ONESOURCE) &&
                (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH))    {

        }

        //MODE 4 : Low/Mid Speed to High Speed
        //REFMPLL or ONESRC --> DRAMPLL
        //non cascaded to cascaded
        else if ((gbl_prev_dramclk_mode != LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) &&
                (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)) {

            //Enable EDC based on vbios
            gddr_enable_edc_vbios();

            //step 38.2
            //Power up WDQS interpolators even if EDC is disabled
            //FOLLOWUP add support for edc tracking
            gddr_power_up_wdqs_intrpl();

            REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x9);
            OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole

            REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

            //step 38.4
            //clamp the refmpll if used in current clock path
            if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) {
                //clk_switch refmpll to onesrc
                gddr_clk_switch_refmpll_2_onesrc();
            }

            //Enabling, programming REFMPLL and DRAMPLL
            //programming refmpll config reg and wait for lock
            gddr_pgm_refmpll_cfg_and_lock_wait();

            //clk switch onesrc to refmpll
            gddr_clk_switch_onesrc_2_refmpll();

            gddr_pgm_drampll_cfg_and_lock_wait();

            //clk switch refmpll to drampll
            gddr_clk_switch_refmpll_2_drampll();

            //step 38.10
            //change the WDQS receiver sel based on vbios
            gddr_pgm_wdqs_rcvr_sel();

            //CMOS_CLK_SEL - CML
            //gddr_enable_cmos_cml_clk(1);

            //***************
            //CLOCK ENABLED
            //***************

            //Switch the dramclk mode and unclamp the clocks
            fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _DRAMPLL,fbio_mode_switch);
            fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_DISABLE,fbio_mode_switch);
            REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

            //FB unit sim only
            if (gbl_g5x_cmd_mode) {
                //                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,0xFBFDFEED);
            } else {
                //                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,0xFBFBFEED);
            }

            //step 38.14
            //Enable inbound barrel shift
            gddr_pgm_brl_shifter(1);

            //step 38.15
            //clear RPRE
            gddr_clear_rpre();

            //step 38.17
            //Disable Low power and Set edc_early_value to 1
            gddr_disable_low_pwr_en_edc_early();

            //step 38.18
            //to review
            gddr_pgm_fbio_cfg_pwrd();

        }

        //MODE 5 : High speed to High speed
        //DRAMPLL --> DRAMPLL
        else if ((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) &&
                (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)) {


            //adding this function to enable edc and replay for high speed to high speed switch
            gddr_enable_edc_vbios();
            //step 38.2
            //Power up WDQS interpolators even if EDC is disabled
            //FOLLOWUP add support for edc tracking
            gddr_power_up_wdqs_intrpl();

            REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE,0xA);
            OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole

            REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

            if (gbl_bFbVddSwitchNeeded || gbl_bVauxChangeNeeded)
            {
                // skip the pll update when doing the vvdd voltage switch, which is a high seeed to high speed
                // switch at the same frequency
            }
            else
            {
                //clk switch drampll to onesrc
                gddr_clk_switch_drampll_2_onesrc();

                //Enabling, programming REFMPLL and DRAMPLL
                //programming refmpll config reg and wait for lock
                gddr_pgm_refmpll_cfg_and_lock_wait();

                //clk switch onesrc to refmpll
                gddr_clk_switch_onesrc_2_refmpll();


                //programming drampll config reg and wait for lock
                gddr_pgm_drampll_cfg_and_lock_wait();

                //clk switch refmpll to drampll
                gddr_clk_switch_refmpll_2_drampll();

            }

            //***************
            //NO CLOCKS
            //***************
            //step 38.10
            //change the WDQS receiver sel based on vbios
            gddr_pgm_wdqs_rcvr_sel();

            //CMOS_CLK_SEL - CML
            //gddr_enable_cmos_cml_clk(1);

            //***************
            //CLOCK ENABLED
            //***************

            //Switch the dramclk mode and unclamp the clocks
            // fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _DRAMPLL,fbio_mode_switch);
            // fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_DISABLE,fbio_mode_switch);
            // REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

            //FB unit sim only
            if (gbl_g5x_cmd_mode) {
                //                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,0xFBFDFEED);
            } else {
                //                REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR,0xFBFBFEED);
            }

            if (gbl_bFbVddSwitchNeeded) {
                //program actual fbvdd
                gddr_pgm_gpio_fbvdd();
            }

            //VAUXP, VCLAMP, VAUXC programming
            if (gbl_bVauxChangeNeeded) {
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL1,new_reg->pfb_fbpa_fbio_vtt_ctrl1);
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL0,new_reg->pfb_fbpa_fbio_vtt_ctrl0);
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN,  new_reg->pfb_fbpa_fbio_cfg_brick_vauxgen);
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN, new_reg->pfb_fbpa_fbio_cfg_vttgen_vauxgen);
                REG_WR32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN, new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
                //100ns settle time for vauxc
                OS_PTIMER_SPIN_WAIT_NS(100);
            }
        }

        //programming cmd fifo depth
        gddr_pgm_cmd_fifo_depth();

        //step 41:
        //programming the shadow register space index - fbio delay priv src
        //from the logs
        //00 - Onesrc - p8
        //11 - Refmpll - p5
        //22 - Drampll - p0

        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_pgm_shadow_reg_space();
            //        FW_MBOX_WR32(2, 0x00000029);
        }

        //Asatish addition new from doc - programming the clock pattern before the DDLL cal
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_pgm_clk_pattern();
        }

        //step 40: //Due to bug 1536307 step 40 and 41 are interchanged
        //Initiate DDLL CAL if REFMPLL

        //re-enable CLK_TX_PWRD, WCK_TX_PWRD and WCKB_TX_PWRD
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_enable_pwrd();
        }

        if (gddr_target_clk_path ==  GDDR_TARGET_REFMPLL_CLK_PATH) {
            fbio_ddll_cal_ctrl  = lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl;
            fbio_ddll_cal_ctrl1 = lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1;
            fbio_ddll_cal_ctrl  = FLD_SET_DRF(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _CALIBRATE, _GO, fbio_ddll_cal_ctrl);
            fbio_ddll_cal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1, _OVERRIDE_DDLLCAL_CODE, 1,fbio_ddll_cal_ctrl1);
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl  = fbio_ddll_cal_ctrl;
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1 = fbio_ddll_cal_ctrl1;
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL,fbio_ddll_cal_ctrl);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL1,fbio_ddll_cal_ctrl1);
            //Clear WCK_DDLL to 0 if using DLL training delay device
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP0_WCK_DDLL,0x00000000);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_SUBP1_WCK_DDLL,0x00000000);
        }
 
        // Bug 1665412: Re-calibrate the clock pad only if it's a voltage switch
        if ((gbl_bFbVddSwitchNeeded) || (gbl_bVauxChangeNeeded))
        {
            gdrRecalibrate();
        }
            
        //Starting calibration for vdd change and vauxc change
        if ((gbl_bFbVddSwitchNeeded) || (gbl_bVauxChangeNeeded)) {
            // uncomment the below two lines if we want to track a lock in cal master since calibrating
            //lwr_reg->pfb_fbio_calmaster_rb = REG_RD32(LOG, LW_PFB_FBIO_CALMASTER_RB);
            //REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(0),lwr_reg->pfb_fbio_calmaster_rb);
            // Restoring powerup_dly to init value at end of mclk switch
            new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG2, _COMP_POWERUP_DLY, _1024, new_reg->pfb_fbio_calmaster_cfg2);
            REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG2,new_reg->pfb_fbio_calmaster_cfg2);
            // Logic looks for a 0->1 transition to update drive strength on clk pads
            new_reg->pfb_fbio_cml_clk2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CML_CLK2, _FORCE_CAL_UPDATE,0,lwr_reg->pfb_fbio_cml_clk2);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_CML_CLK2,new_reg->pfb_fbio_cml_clk2);
            new_reg->pfb_fbio_cml_clk2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CML_CLK2, _FORCE_CAL_UPDATE,1,lwr_reg->pfb_fbio_cml_clk2);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_CML_CLK2,new_reg->pfb_fbio_cml_clk2);
            //wait for deterministic time Sangitac to provide time taken  - NOT required
        }

        //Step 42a
        //After certain delay stop ddll calibration
        if (gddr_target_clk_path ==  GDDR_TARGET_REFMPLL_CLK_PATH) {
            //RM flag DDLL_CALIBRATION_SETTLE_TIME_US 10
            // Trying to hide the ddll cal delay behind autocal on a P5(L/H) to P5(H/L) switch
            if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
            {
                OS_PTIMER_SPIN_WAIT_US(11);
            }
            fbio_ddll_cal_ctrl = lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl;
            fbio_ddll_cal_ctrl = FLD_SET_DRF(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _CALIBRATE,_STOP,fbio_ddll_cal_ctrl);
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl = fbio_ddll_cal_ctrl;
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL,fbio_ddll_cal_ctrl);
        }

        //Step 42b
        //Write all the config/timing registers
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_pgm_reg_values();
        }

        //Note : must be called after gddr_pgm_reg_values - Dependency on fbio_cfg1_cmd_brlshft value
        //        if (!((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce))) {

        //Asatish for saving time from p8<->p5
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
        if(!((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) &&
                ((gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) ||
                        (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)))) {
            gddr_pgm_cmd_brlshift();
        }
        }

        //Step 42
        //Disable self refresh
        gddr_disable_self_refresh();

        //Step 43
        //wait for 1us - when taking FB out of self ref
        OS_PTIMER_SPIN_WAIT_US(1);

        gddr_save_clr_fbio_pwr_ctrl();

        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
    }
    if(!bPreAddressBootTraining && !bPostAddressBootTraining && !bPostWckRdWrBootTraining) {

        //Step 46 
        //Kick off Address Training
        if(gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {

            LwU32 fbpa_fbio_spare;
            fbpa_fbio_spare= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_SPARE);
            if (GblEnMclkAddrTraining) {
                LwU32 force_ma2sdr = 1;
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE, _FORCE_MA_SDR, force_ma2sdr, fbpa_fbio_spare);
            } else {
                LwU32 force_ma2sdr = 0;
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE, _FORCE_MA_SDR, force_ma2sdr, fbpa_fbio_spare);
            }
            //Asatish for saving time from p8<->p5
            if(!((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) &&
                    ((gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) ||
                            (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)))) {
                REG_WR32(BAR0, LW_PFB_FBPA_FBIO_SPARE,       fbpa_fbio_spare);
            }

            if ((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce)) {
                LwBool tr_status;
                tr_status = gddr_addr_training(gbl_ddr_mode);
                if (!tr_status) {
                    FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_TRAINING_ERROR);
                } else {
                    FW_MBOX_WR32(13, 0xD0D0ADD2);
                }
            }
        }
    }

    if(!bPreAddressBootTraining && !bPostWckRdWrBootTraining) {
        //Step 44A
        //Issue a precharge command
        gddr_precharge_all();

        //Step 44B
        //Issue a refresh command
        gddr_send_one_refresh();

        //Step 45
        //Enable Normal Refresh
        //Enable Refsb if PoR ??
        gddr_enable_refresh();

        //Step 46
        //Kick off Address Training
        //Only for first P0 switch after power up (including after suspend/resume or GC6), unless already done in boot training

        if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
            LwU32 fbpa_fbio_spare;
            fbpa_fbio_spare= REG_RD32(BAR0, LW_PFB_FBPA_FBIO_SPARE);
            if (GblEnMclkAddrTraining) {
                LwU32 force_ma2sdr = 1;
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE,       _FORCE_MA_SDR,  force_ma2sdr,      fbpa_fbio_spare);
            } else {
                LwU32 force_ma2sdr = 0;
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE,       _FORCE_MA_SDR,  force_ma2sdr,      fbpa_fbio_spare);
            }
            REG_WR32(BAR0, LW_PFB_FBPA_FBIO_SPARE,       fbpa_fbio_spare);
        }
        if ((GblEnMclkAddrTraining) && (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5)) {

            if ((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce)) {
                LwBool tr_status;
                tr_status = gddr_addr_training(gbl_ddr_mode);
                if (!tr_status) {
                    FW_MBOX_WR32(13, 0xDEADADD2);
                    FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_TRAINING_ERROR);
                } else {
                    FW_MBOX_WR32(13, 0xD0D0ADD2);
                }
            }
        }

        //Step 47
        //MRS3 write
        //Disable Bank Grouping, if new mclk freq is > vbios MIEMclkBGEnableFreq, then enable bank grouping
        //Enable ADR_GDDR5_RDQS if not cascaded, else disable it
        //if x32 set adr_gddr5_wck_term to ZQ_DIV2 else set to ZQ
        gddr_pgm_mrs3_reg();

        //Step 48
        //Update MRS/EMRS registers
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_update_mrs_reg();
        }

        //Step 50
        //Flush MRS/EMRS writes.
        //FOLLOWUP - update FBVREF - same as step 21.
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_flush_mrs_reg();
            //if ((gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) && (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) 
            if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH)
            {
                gddr_set_fbvref();
            }

        }

        //Enabling DQ term mode and save before training bug : 2000479
        //	Terminating DQ for low speed- needed for training 2x dramclk - Turing
        //	Higher speeds are always terminated
        //	Updating Vref values for Terminated mode
        if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
            gddr_pgm_save_vref_val();
            gddr_en_dq_term_save();
        }

        //Additional Step - bug 2008194 for g6
        if (!bPostAddressBootTraining) {
            gddr_edc_tr_dbg_ref_recenter(1);
        }

        if (bPostAddressBootTraining) {
            gddr_set_trng_ctrl_reg();
        }
    }
    if(!bPreAddressBootTraining && !bPostAddressBootTraining && !bPostWckRdWrBootTraining) {

        //Step 54
        //Referred RM function : fbBuildPerSwitchMemoryLinkTrainingSeqGddr5_GF117
        //if (gbl_unit_sim_run) {
        //load read pattern table when address training is enabled
        if ((GblEnMclkAddrTraining) && (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)
                && (!GblAddrTrPerformedOnce)) {
            FuncLoadRdPatterns(gbl_ddr_mode);
            //FuncLoadRdPattern_g6();
        }

        gddr_mem_link_training();
        //} else {
        //    gddr_load_brl_shift_val(1,1);
        //}
    }

    if(!bPreAddressBootTraining && !bPostAddressBootTraining) {

        //Additional step - bug 1904021
        gddr_restore_fbio_pwr_ctrl();

        //Additional Step - bug 2008194 for g6
        if (!bPostWckRdWrBootTraining) {
            gddr_edc_tr_dbg_ref_recenter(0);
        }

        //Restore dq term value after training bug : 2000479
        if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
            gddr_restore_vref_val();
            gddr_restore_dq_term();
        }

        //CYA Step
        //Resetting exit wdat_ptr and edc_ptr for some % of randoms
        //        FW_MBOX_WR32(4, 0x00000136);
        //        REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xABFBFEED);
        //        FW_MBOX_WR32(2, 0x00000136);

        //EDC enabled for drampll before in step 38.1
        ////Step 55
        ////Enable edc/replay and power down wdqs intrpl if unused
        //gddr_enable_edc_replay();

        //Step 56
        //Enable GDDR5_lp3 on MRS5
        //FOLLOWUP HYNIX increase refresh strength - TMRS sequence
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_enable_gddr5_lp3();
        }

        //Step 57
        if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
            gddr_reset_read_fifo();
        }

        //Step 59
        //if (gbl_unit_sim_run) {
        gddr_enable_periodic_trng();
        //}

        //Step 58
        //Start FB
        //Restore ACPD
        //FOLLOWUP TMRS seq to improve timings on HYNIX 4GB GDDR5

        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_1(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

        //Programming acpd
        LwU8 VbiosAcpd = gTT.perfMemClkBaseEntry.Flags0.PMC11EF0ACPD;
        lwr_reg->pfb_fbpa_cfg0 =  FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACPD, VbiosAcpd,lwr_reg->pfb_fbpa_cfg0);
        REG_WR32(LOG, LW_PFB_FBPA_CFG0,lwr_reg->pfb_fbpa_cfg0);

        fbStopTimeNs = memoryStartFb_HAL(&Fbflcn, &startFbStopTimeNs);
        //gddr_start_fb();

        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_1(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
        gddr_update_arb_registers();

        if ((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce)) {
            enableEvent(EVENT_SAVE_TRAINING_DATA);
            GblAddrTrPerformedOnce = LW_TRUE;
        }
    }
    return fbStopTimeNs;
}



void
gddr_check_vaux_change
(
        void
)
{
    LwU32 fbio_vtt_cntl0;
    LwU32 fbio_vtt_cntl1;
    LwU32 cfg_brick_vauxgen;
    LwU32 cfg_vttgen_vauxgen;
    LwU32 calgrp_vttgen_vauxgen;
    LwU32 lwr_dq_vauxc_level;
    LwU32 lwr_cmd_vauxc_level;
    LwU32 lwr_cdb_vauxc_level;
    LwU32 lwr_comp_vauxc_level;
    LwU32 lwr_dq_vauxp_level;
    LwU32 lwr_cmd_vauxp_level;
    LwU32 lwr_comp_vauxp_level;
    LwU32 lwr_dq_vclamp_level;
    LwU32 lwr_cmd_vclamp_level;
    LwU32 lwr_comp_vclamp_level;
    LwU8 vbios_dq_vauxc_level;
    LwU8 vbios_cdb_vauxc_level;
    LwU32 VauxcDesignProgramError;


    //dq, cmd :  vauxc, vauxp, vclamp refer bug 2105133 comment 2 for exact field settings
    fbio_vtt_cntl0 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL0);
    lwr_dq_vauxc_level   = REF_VAL(LW_PFB_FBPA_FBIO_VTT_CTRL0_BYTE_LV_VAUXC, fbio_vtt_cntl0);
    lwr_cmd_vauxc_level  = REF_VAL(LW_PFB_FBPA_FBIO_VTT_CTRL0_CMD_LV_VAUXC , fbio_vtt_cntl0);

    cfg_brick_vauxgen = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN);
    cfg_vttgen_vauxgen = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN);

    lwr_dq_vclamp_level = REF_VAL(LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN_REG_LVCLAMP, cfg_brick_vauxgen);
    lwr_dq_vauxp_level  = REF_VAL(LW_PFB_FBPA_FBIO_CFG_BRICK_VAUXGEN_REG_LVAUXP , cfg_brick_vauxgen);

    lwr_cmd_vclamp_level = REF_VAL(LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN_REG_VCLAMP_LVL, lwr_cmd_vauxc_level);
    lwr_cmd_vauxp_level  = REF_VAL(LW_PFB_FBPA_FBIO_CFG_VTTGEN_VAUXGEN_REG_VAUXP_LVL, lwr_cmd_vauxc_level);

    //comp vauxc
    calgrp_vttgen_vauxgen = REG_RD32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN);
    lwr_comp_vauxc_level = REF_VAL(LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN_VTT_LV_VAUXC,calgrp_vttgen_vauxgen);
    lwr_comp_vauxp_level = REF_VAL(LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN_VTT_LV_VAUXP,calgrp_vttgen_vauxgen);
    lwr_comp_vclamp_level = REF_VAL(LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN_VTT_LV_VCLAMP,calgrp_vttgen_vauxgen);

    //cdb vauxc
    //LW_PFB_FBPA_FBIO_VTT_CTRL1, LW_PFB_FBPA_FBIO_VTT_CTRL1_CDB_VAUXC_LOAD
    fbio_vtt_cntl1 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL1);
    lwr_cdb_vauxc_level = REF_VAL(LW_PFB_FBPA_FBIO_VTT_CTRL1_CDB_LV_VAUXC, fbio_vtt_cntl1);


    //vauxc, vauxp, vclamp of [dq,cmd,cdb] must be the same in turing
    VauxcDesignProgramError = lwr_dq_vclamp_level ^ lwr_dq_vauxp_level ^ lwr_dq_vauxc_level
            ^  lwr_cmd_vclamp_level ^ lwr_cmd_vauxp_level ^ lwr_cmd_vauxc_level
            ^  lwr_cdb_vauxc_level ^ lwr_comp_vauxc_level ^ lwr_comp_vauxp_level ^ lwr_comp_vclamp_level;

    if(VauxcDesignProgramError != 0) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_VAUX_DESGIN_PGM_ERROR);
    }

    //VBIOS vauxc values - assuming vauxc, vauxp and vclamp are all configured same values
    //DQ
    vbios_dq_vauxc_level = gTT.perfMemTweakBaseEntry.pmtbe391384.DQADVauxc;


    //CDB
    vbios_cdb_vauxc_level =  gTT.perfMemTweakBaseEntry.pmtbe407400.CDBVauxc;

    if (vbios_dq_vauxc_level != vbios_cdb_vauxc_level) {
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_VAUX_VBIOS_PGM_ERROR);
    }

    gbl_target_vauxc_level = vbios_dq_vauxc_level;
    if (vbios_dq_vauxc_level != lwr_dq_vauxc_level) {
        gbl_bVauxChangeNeeded = 1;
    } else {
        gbl_bVauxChangeNeeded = 0;
    }
    return;

}

LwU32
doMclkSwitch
(
        LwU32 targetFreqKHz
)
{
    return doMclkSwitchPrimary(targetFreqKHz, 0, 0, 0);
}

void
gddr_check_fbvdd_switch
(
        void
)
{

    gbl_lwrr_fbvddq = gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1FBVDDQ;

    LwU8 prev_fbvddq;
    LwU8 fbvddGpioIndex = memoryGetGpioIndexForFbvddq_HAL();
    LwU8 ioInput = REF_VAL(LW_PMGR_GPIO_OUTPUT_CNTL_IO_INPUT,REG_RD32(LOG, LW_PMGR_GPIO_OUTPUT_CNTL(fbvddGpioIndex)));
    if (ioInput == 0)
    {
        prev_fbvddq = PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW;
    }
    else
    {
        prev_fbvddq = PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_HIGH;
    }

    gbl_fbvdd_low_to_high = LW_FALSE;
    gbl_fbvdd_high_to_low = LW_FALSE;
    gbl_bFbVddSwitchNeeded  = LW_FALSE;

    if ((gbl_lwrr_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW) && (prev_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_HIGH)) {
        gbl_fbvdd_high_to_low = LW_TRUE;
        gbl_bFbVddSwitchNeeded  = LW_TRUE;
    } else if ((gbl_lwrr_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_HIGH) && (prev_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW)) {
        gbl_fbvdd_low_to_high = LW_TRUE;
        gbl_bFbVddSwitchNeeded  = LW_TRUE;
    }

    return;
}

void
gddr_assign_local_table_pointer
(
        void
)
{

    pInfoEntryTable_Refmpll = gBiosTable.pIEp_refmpll;
    pInfoEntryTable_Mpll = gBiosTable.pIEp_mpll;
    mInfoEntryTable = gTT.pMIEp;

    return;
}

LwU32
doMclkSwitchInMultipleStages
(
        LwU32 targetFreqKHz,
        LwBool bPreAddressBootTraining,
        LwBool bPostAddressBootTraining,
        LwBool bPostWckRdWrBootTraining
)
{
    return doMclkSwitchPrimary(targetFreqKHz, bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
}

LwU32
doMclkSwitchPrimary
(
        LwU32 targetFreqMHz,
        LwBool bPreAddressBootTraining,
        LwBool bPostAddressBootTraining,
        LwBool bPostWckRdWrBootTraining
)
{

    LwU32 falcon_monitor;
    LwU32 selwre_wr_scratch1;
    LwU32 fbStopTimeNs = 0;

    gbl_bFbVddSwitchNeeded = LW_FALSE;


    // For now set this up that we assume unit level sin when running & compiling in the //hw location
#ifdef FBFLCN_HW_RUNTIME_ELW
    unit_level_sim = 1;
#else
    unit_level_sim = 0;
#endif
    falcon_monitor = REG_RD32(LOG, LW_PFB_FBPA_FALCON_MONITOR);
    //FB unit sim only
    tb_sig_g5x_cmd_mode = bit_extract(falcon_monitor,1,1);
    tb_sig_dramclk_mode = bit_extract(falcon_monitor,0,1);
    selwre_wr_scratch1 = 0;
    gbl_unit_sim_run = bit_extract(selwre_wr_scratch1,0,1);

    // g_mcs definition:
    // LwU8  cmdType;
    // LwU32 targetFreqKHz;
    // LwU32 flags;
    //get_valid_partitions();

    gbl_target_freqMHz = targetFreqMHz;
    gbl_target_freqKHz = targetFreqMHz * 1000;

    fbfalconSelectTable_HAL(&Fbflcn,gbl_target_freqMHz);

    gddr_assign_local_table_pointer();
    gbl_mclk_freq_new = gbl_target_freqMHz;

// Keeping for now for asatish
    //#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    //    privprofilingEnable();
    //    privprofilingReset();
    //#endif
    GblSwitchCount++;
    //    hbm_mclk_switch();

    //checking if we need a voltage switch for this freq switch
    //assuming the boot p-state has always fbvdd as low

    //read current clock source
    gddr_lwrr_clk_source = REG_RD32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
    gbl_prev_dramclk_mode = REF_VAL(LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE,gddr_lwrr_clk_source );
    if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_ONESOURCE) {
        gbl_prev_dramclk_mode = LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL;
    }

    //get current frequency
    gddr_retrieve_lwrrent_freq();
    gddr_check_fbvdd_switch();
    gddr_check_vaux_change();
    FW_MBOX_WR32(12, gbl_target_freqMHz);
    FW_MBOX_WR32(14, GblSwitchCount);

    if (!gbl_bFbVddSwitchNeeded && !gbl_bVauxChangeNeeded) {

        fbStopTimeNs = gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        FW_MBOX_WR32(11, 0x0000DEAD);
        FW_MBOX_WR32(12, gbl_target_freqMHz);
        FW_MBOX_WR32(13, fbStopTimeNs);

    } else if (((gbl_bFbVddSwitchNeeded) && (gbl_fbvdd_low_to_high)) ||
            ((gbl_bVauxChangeNeeded) && (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL))) {
        gbl_target_freqMHz = gbl_lwrrent_freqMHz;

        fbfalconSelectTable_HAL(&Fbflcn,gbl_target_freqMHz);

        gddr_assign_local_table_pointer();

        //performing intermidiate p-state switch for changing fbvdd from low to high or
        //changing vauxc and re-calibrating
        fbStopTimeNs = gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        //REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(0),LW_CEIL(fbStopTimeNs, 1000));
        FW_MBOX_WR32(7, 0x0000DEAD);
        FW_MBOX_WR32(8, gbl_target_freqMHz);
        FW_MBOX_WR32(9, fbStopTimeNs);
        FW_MBOX_WR32(10, GblSwitchCount);

        gbl_bFbVddSwitchNeeded = LW_FALSE;
        gbl_bVauxChangeNeeded = LW_FALSE;

        gbl_target_freqMHz = gbl_mclk_freq_new;

        fbfalconSelectTable_HAL(&Fbflcn, gbl_target_freqMHz);

        gddr_assign_local_table_pointer();

        //read current clock source
        gddr_lwrr_clk_source = REG_RD32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
        gbl_prev_dramclk_mode = REF_VAL(LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE,gddr_lwrr_clk_source );
        if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_ONESOURCE) {
            gbl_prev_dramclk_mode = LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL;
        }

        //performing actual mclk switch post fbvdd change
        fbStopTimeNs = gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        //REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(1),LW_CEIL(fbStopTimeNs, 1000));
        FW_MBOX_WR32(11, 0x0000DEAD);
        FW_MBOX_WR32(12, gbl_target_freqMHz);
        FW_MBOX_WR32(13, fbStopTimeNs);
    } else if (((gbl_bFbVddSwitchNeeded) && (gbl_fbvdd_high_to_low)) ||
            ((gbl_bVauxChangeNeeded) && (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL))) {


        LwBool temp_fbvddswitchneeded = gbl_bFbVddSwitchNeeded;
        LwBool temp_vauxchangeneeded = gbl_bVauxChangeNeeded;
        gbl_bFbVddSwitchNeeded = LW_FALSE;
        gbl_bVauxChangeNeeded = LW_FALSE;

        //performing clock switch to low freq
        fbStopTimeNs = gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        //REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(2),LW_CEIL(fbStopTimeNs, 1000));
        FW_MBOX_WR32(7, 0x0000DEAD);
        FW_MBOX_WR32(8, gbl_target_freqMHz);
        FW_MBOX_WR32(9, fbStopTimeNs);
        FW_MBOX_WR32(10, GblSwitchCount);

        gbl_bVauxChangeNeeded = temp_vauxchangeneeded;
        gbl_bFbVddSwitchNeeded = temp_fbvddswitchneeded;

        //FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT);
        //read current clock source
        gddr_lwrr_clk_source = REG_RD32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
        gbl_prev_dramclk_mode = REF_VAL(LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE,gddr_lwrr_clk_source );
        if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_ONESOURCE) {
            gbl_prev_dramclk_mode = LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL;
        }

        //performing fbvdd switch to lower the voltage or
        //changing vauxc and re-calibrating
        fbStopTimeNs = gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        //REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(3),LW_CEIL(fbStopTimeNs, 1000));
        FW_MBOX_WR32(11, 0x0000DEAD);
        FW_MBOX_WR32(12, gbl_target_freqMHz);
        FW_MBOX_WR32(13, fbStopTimeNs);
    }

// Keeping for now for asatish
    //#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    //    privprofilingDisable();
    //#endif

    return fbStopTimeNs;
    //return RM_FBFLCN_MCLK_SWITCH_MSG_STATUS_OK;
}

