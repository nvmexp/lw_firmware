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
#include "dev_pafalcon_pri.h"
#include "dev_fbpa.h"
#include "dev_fb.h"
#include "dev_trim.h"
#include "dev_pmgr.h"
#include "dev_pwr_pri.h"
#include "dev_top.h"
#include "rmfbflcncmdif.h"
#include "dev_pri_ringstation_fbp.h"
#include "segment.h"
#include "seq_decoder.h"        // needed for PA_SEQ_DMEM_START

#include "osptimer.h"
#include "memory.h"
#include "memory_gddr.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_memory_private.h"
#include "temp_tracking.h"

PLLInfo5xEntry* pInfoEntryTable_Refmpll;
PLLInfo5xEntry* pInfoEntryTable_Mpll;
MemInfoEntry  *mInfoEntryTable;
EdcTrackingGainTable my_EdcTrackingGainTable;
LwU32 gbledctrgain;
LwU32 gblvreftrgain;

extern LwU8 gPlatformType;     // defined and initialized in main.c

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

#define LW_PFB_FBPA_TRAINING_CMD0               LW_PFB_FBPA_TRAINING_CMD(0)
#define LW_PFB_FBPA_TRAINING_CMD1               LW_PFB_FBPA_TRAINING_CMD(1)

#define REFMPLL_BASE_FREQ                             27
#define DRAMPLL_BASE_FREQ_REFMPLL_OUT_200            200
#define DRAMPLL_BASE_FREQ_REFMPLL_OUT_250            250
#define REFMPLL_HIGH_VCO_SELDIVBY2_EN_BASE_FREQ     2500
#define REFMPLL_HIGH_VCO_SELDIVBY2_EN_MAX_FREQ      3999
#define DRAMPLL_N_UNDERFLOW_BASE_FREQ               5000

#define S32_DIV_ROUND(n,d)  ((((n) < 0) != ((d) < 0)) ? (((n) - (d)/2) / (d)) : (((n) + (d)/2) / (d)))
#define U32_DIV_ROUND(n,d)  (((n) + (d)/2) / (d))

#define PMCT_STRAP_MEMORY_DLL_DISABLED   1

// enum for sync function, only 4 bits wide
enum SyncFN {
    FN_GDDR_FB_STOP = 0,
    FN_GDDR_SET_FBVREF,
    FN_GDDR_PGM_GPIO_FBVDD,
    FN_PGM_CALGROUP_VTT_VAUX_GEN,
    FN_GDDR_RECALIBRATE,
    FN_GDDR_PA_SYNC,
    FN_SAVE_VREF_VALUES
//    FN_GDDR6_LOAD_RDPAT
};


// control by sw config read at initialization
extern LwBool gbl_selfref_opt;
extern LwBool gbl_en_fb_mclk_sw;
extern LwBool gbl_pa_bdr_load;      // for backdoor load of sequencer data
extern LwBool gbl_fb_use_ser_priv;
extern LwU8 gPlatformType;

LwBool gbl_pam4_mode = LW_FALSE;
LwBool gbl_asr_acpd = LW_FALSE;
LwBool gbl_ac_abi = LW_TRUE;
LwBool gbl_mta_en = LW_FALSE;
LwBool GblFbioIntrpltrOffsetInc = LW_FALSE;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
LwU32 gbl_pa_mclk_sw_seq[PA_MAX_SEQ_SIZE]
      GCC_ATTRIB_USED() GCC_ATTRIB_SECTION("sequencerData", "gSeqBuffer");
/*
#define MAX_SYNC_REQ    20
LwU16 gbl_pa_sync_req[MAX_SYNC_REQ];
*/
#endif

LwU32 gbl_pa_instr_cnt;
LwU8 gbl_fb_sync_cnt=0;
LwU16 gbl_prev_fbio_idx;

FLCN_TIMESTAMP  startFbStopTimeNs = { 0 };
#define bit_assign(_reg,_pos,_bval,_bs)   ((_reg&~((((LwU32)0x1<<_bs)-1)<<_pos))|(_bval<<_pos))

#define LW_PFB_FBPA_GENERIC_MRS5_ADR_GDDR6X_DFE_CALIBRATION     4:3 /* DFE Calibration field */

#define LW_PFB_FBPA_GENERIC_MRS6_ADR_GDDR6X_PIN_SUB_ADDRESS    11:7 /* pin address field */
#define LW_PFB_FBPA_GENERIC_MRS6_ADR_GDDR6X_PIN_LEVEL           6:0 /* pin payload includes vrefd, ctle, bias and common mode */

#define LW_PFB_FBPA_GENERIC_MRS8_ADR_CATH                       3:2 /* Addr Term MSB */
#define LW_PFB_FBPA_GENERIC_MRS8_ADR_PLL_RANGE                  4:4 /* PLL Range LSB */
#define LW_PFB_FBPA_GENERIC_MRS8_ADR_MICRON_PLL_RANGE_MSB       5:5 /* PLL Range MSB */

void stop_fb (void);
void start_fb (void);
void gddr_retrieve_lwrrent_freq(void);
void gddr_callwlate_target_freq_coeff(void);
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
LwBool gSaveRequiredAfterFirstMclkSwitch = LW_TRUE;
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
LwU32 gbl_refmpll_freqMHz;
LwU32 gbl_ddr_mode;
LwU32 gbl_g5x_cmd_mode;
LwU32 gbl_prev_dramclk_mode;
LwBool gbl_first_p0_switch_done = LW_FALSE;
LwU32 gbl_dq_term_mode_save;
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
LwBool GblFirstSwitchVrefCodeSave = LW_FALSE;
LwU8 GblSaveDqVrefVal;
LwU8 GblSaveDqsVrefVal;
LwU8 GblSaveDqMidVrefVal;
LwU8 GblSaveDqUpperVrefVal;
LwBool GblAutoCalUpdate = LW_FALSE;
LwBool GblSkipDrampllClamp = LW_FALSE;
LwBool GblInBootTraining = LW_FALSE;


LwU32 gddr_target_clk_path;
LwU32 refmpll_m_val;
LwU32 refmpll_n_val;
LwU32 refmpll_p_val;
LwU32 drampll_m_val;
LwU32 drampll_n_val;
LwU32 drampll_p_val;
LwU32 drampll_divby2;
LwS32 refmpll_sdm_din;
LwS32 refmpll_ssc_min_rm;
LwS32 refmpll_ssc_max_rm;
LwS32 refmpll_ssc_step;
LwU32 fbio_mode_switch;
LwU32 tb_sig_g5x_cmd_mode;
LwU32 tb_sig_dramclk_mode;
LwU32 Mrs8AddrTerm;
LwU32 Mrs1AddrTerm;
LwU32 g6x_fbio_broadcast;


GCC_ATTRIB_SECTION("memory", "gddr_ceil")
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

GCC_ATTRIB_SECTION("memory", "gddr_get_target_clk_source")
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

GCC_ATTRIB_SECTION("resident", "gddr_retrieve_lwrrent_freq")
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
    refmpll_coeff = REG_RD32(BAR0, LW_PFB_FBPA_REFMPLL_COEFF);
    lwr_refmpll_n_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_NDIV,refmpll_coeff);
    lwr_refmpll_m_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_MDIV,refmpll_coeff);
    lwr_refmpll_p_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_PLDIV,refmpll_coeff);

    ref_vco_freq = lwr_refmpll_n_val * gbl_refpll_base_freq ;
    gbl_lwrrent_freqMHz =  ref_vco_freq / (lwr_refmpll_m_val * lwr_refmpll_p_val) ;
    gbl_lwrrent_freqKHz = (gbl_lwrrent_freqMHz * 1000);

    if (gbl_prev_dramclk_mode ==  LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) {
        drampll_coeff = REG_RD32(BAR0, LW_PFB_FBPA_DRAMPLL_COEFF);

        lwr_drampll_n_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_NDIV,drampll_coeff);
        lwr_drampll_m_val = REF_VAL(LW_PFB_FBPA_REFMPLL_COEFF_MDIV,drampll_coeff);
        lwr_drampll_divby2_val = ((REF_VAL(LW_PFB_FBPA_DRAMPLL_COEFF_SEL_DIVBY2,drampll_coeff)) & 0x1);

        mpll_vco_freq = lwr_drampll_n_val * gbl_lwrrent_freqMHz;
        gbl_lwrrent_freqMHz = (mpll_vco_freq ) / ( lwr_drampll_m_val << lwr_drampll_divby2_val);

	
	//doubling the clock frequency for gddr6x drampll mode
        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
        {
            gbl_lwrrent_freqMHz *= 2;    
        }
        gbl_lwrrent_freqKHz = gbl_lwrrent_freqMHz * 1000;
    }

    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_callwlate_ssd_variables")
void
gddr_callwlate_ssd_variables
(
        LwU32 refmpll_vco_freq_num
)
{
    LwU32 repeat_sdm;
    LwS32 refmpll_frac_num;
    LwU32 refmpll_frac_mod;
    LwS32 refmpll_frac_den;
    LwU32 refmpll_n_int;
    LwU32 refmpll_sdm_frac_int_rm;
    LwU8 spread_lo = gTT.perfMemClkBaseEntry.RefMPLLSCFreqDelta;    // = ppm / 100
    LwU32 refmpll_rise;
    LwU32 increment_sdm;
    LwU32 refmpll_frac_int;
    LwU32 deltarefmpll_ssc_min_rm;

    // mdiv = (freqInput / freqURMax), ceiling
    // ndiv = (((freqVcoTgt * mdiv) - (freqInput / 2)) / freqInput), round
    // sdm = (((freqVcoTgt * mdiv * 8192) / freqInput) - (ndiv * 8192) - 4096), round

    refmpll_frac_den = gbl_refpll_base_freq;

    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)
    {
        refmpll_frac_den *= drampll_n_val;
    }

    refmpll_n_int = (refmpll_n_val * 1024);// / refmpll_frac_den;
    refmpll_frac_int = (refmpll_vco_freq_num * 1024) / refmpll_frac_den;
    refmpll_frac_mod = (refmpll_vco_freq_num * 1024) % refmpll_frac_den;

    refmpll_frac_num = refmpll_frac_mod + (refmpll_frac_den * (refmpll_frac_int - refmpll_n_int - 512));
    refmpll_frac_num = refmpll_frac_num * 8;
    refmpll_sdm_din = S32_DIV_ROUND(refmpll_frac_num, refmpll_frac_den);

    if (refmpll_frac_num < 0)
    {
        refmpll_sdm_din = 131072 + refmpll_sdm_din;
    }

    refmpll_sdm_frac_int_rm = refmpll_sdm_din ;
    if (refmpll_sdm_din > 65536)        // 2^16
    {
        refmpll_sdm_frac_int_rm = refmpll_sdm_frac_int_rm - 131072 ;
    }

    // divide by 10000, instead of 100000 since spread_lo = ppm / 100
    deltarefmpll_ssc_min_rm = (((refmpll_sdm_frac_int_rm + 4096 + (refmpll_n_val*8192)) * spread_lo) / 10000) ;

    refmpll_ssc_min_rm = refmpll_sdm_din - deltarefmpll_ssc_min_rm;

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

    increment_sdm = U32_DIV_ROUND(refmpll_rise, (gbl_refpll_base_freq*1000));
    repeat_sdm = 2 ;
    if (increment_sdm % 2 == 0)
    {
        increment_sdm = increment_sdm / 2;
        repeat_sdm = repeat_sdm / 2 ;
    }

    refmpll_ssc_step = ((repeat_sdm - 1) << 12) |  increment_sdm;

    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_callwlate_target_freq_coeff")
void
gddr_callwlate_target_freq_coeff
(
        void
)
{
    LwU32 refmpll_vco_freq;
    LwU32 refmpll_vco_freq_num;     // numerator only if DRAMPLL, else refmpll_vco_freq
    LwU32 refmpll_vco_max = gBiosTable.pIEp_refmpll->VCOMax;

    if (gbl_unit_sim_run) {gbl_refpll_base_freq = 20;}


    //Considering prev clock path
    refmpll_vco_freq = 0;
    refmpll_vco_freq_num = 0;
    gbl_computation_freqMHz = 0;
    refmpll_m_val = 1;

    if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) || (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH))  {

        //Callwlation based on Bob's Algorithm for freq less than 1Ghz
        refmpll_p_val = refmpll_vco_max / gbl_target_freqMHz; //multiplying by 1000 to get 3 decimal resolution
        refmpll_vco_freq = gbl_target_freqMHz * refmpll_p_val;
        refmpll_vco_freq_num = refmpll_vco_freq;
        refmpll_n_val = refmpll_vco_freq / gbl_refpll_base_freq;
        gbl_refmpll_freqMHz = gbl_target_freqMHz;

    } else if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        //generating ref pll frequency between 200Mhz to 300Mhz
        //Fixing the p_val to 10 and m_val to 1;

        //if the frequency is less than 4Ghz then we would enable dram sel_divby 2 and
        //compute frequency for double the target frequency
        if (gbl_target_freqMHz < gBiosTable.pIEp_mpll->VCOMin) {
            gbl_computation_freqMHz = gbl_target_freqMHz * 2;
            drampll_divby2 = 1;
        } else {
            gbl_computation_freqMHz = gbl_target_freqMHz;
            drampll_divby2 = 0;
        }
#define OLD_PLL_ALGO 0

#if OLD_PLL_ALGO
        if ((gbl_target_freqMHz < DRAMPLL_N_UNDERFLOW_BASE_FREQ) &&
           ((gbl_target_freqMHz < REFMPLL_HIGH_VCO_SELDIVBY2_EN_BASE_FREQ) ||
            (gbl_target_freqMHz > REFMPLL_HIGH_VCO_SELDIVBY2_EN_MAX_FREQ))) {

            gbl_refmpll_freqMHz = DRAMPLL_BASE_FREQ_REFMPLL_OUT_200;
        } else {
            gbl_refmpll_freqMHz = DRAMPLL_BASE_FREQ_REFMPLL_OUT_250;
        }

        drampll_n_val = (gbl_computation_freqMHz / gbl_refmpll_freqMHz);
        refmpll_p_val = (refmpll_vco_max * drampll_n_val) / gbl_computation_freqMHz;
#else   // new algo
        LwU32 drampll_n_val_min;
        LwU32 drampll_n_val_max;
        LwS32 refmpll_vco_delta = refmpll_vco_max;
        LwU32 loop_var;

        //Ceiling the min drampll_n so that we do not violate the clkin max
        //As we use integers, add (divisor-1) to the dividend to get the ceil
        drampll_n_val_min = (gbl_computation_freqMHz + gBiosTable.pIEp_mpll->URMax - 1)/gBiosTable.pIEp_mpll->URMax;
        //Clamp the min value to 20 -> use vbios tables for figuring this min value
        drampll_n_val_min = drampll_n_val_min > gBiosTable.pIEp_mpll->NMin ? drampll_n_val_min : gBiosTable.pIEp_mpll->NMin;
        //Need to use floor for max value, using integers already takes care of this
        drampll_n_val_max = (gbl_computation_freqMHz / gBiosTable.pIEp_mpll->URMin);
        //Initialize the drampll_n to min, if max < min due to clamping and we never end up entering the loop below
        drampll_n_val = gBiosTable.pIEp_mpll->NMin;

        // find drampll_n_val for fastest VCO
        for(loop_var=drampll_n_val_min; loop_var<=drampll_n_val_max; loop_var++) 
        {
            refmpll_p_val = refmpll_vco_max * loop_var / gbl_computation_freqMHz;

            gbl_refmpll_freqMHz = (gbl_computation_freqMHz / loop_var );
            refmpll_vco_freq = (gbl_computation_freqMHz * refmpll_p_val) / loop_var;
            if ((refmpll_vco_max - refmpll_vco_freq) < refmpll_vco_delta) 
            {
                drampll_n_val = loop_var;
                refmpll_vco_delta = refmpll_vco_max - refmpll_vco_freq;
            }
        }

        refmpll_p_val = (refmpll_vco_max * drampll_n_val) / gbl_computation_freqMHz;
        gbl_refmpll_freqMHz = (gbl_computation_freqMHz / drampll_n_val);
#endif
        // refmpll_vco_freq to be used for gddr_callwlate_ssd_variables, denominator is drampll_n_val
        refmpll_vco_freq_num = gbl_computation_freqMHz * refmpll_p_val;     // note numerator only
        refmpll_vco_freq = refmpll_vco_freq_num / drampll_n_val;
        refmpll_n_val = refmpll_vco_freq_num / (gbl_refpll_base_freq * drampll_n_val);

        gddr_callwlate_drampll_coeff();
    }

    // bug 3196573, where we need to force SSC to be enabled for G6x p5/p8
    // bug 3194340, 3194409 contains the VBIOS fixes
    // VBIOS fix CL in VBIOS_P4_CL_FOR_FORCE_SSC_P58_BUG_3196573
    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) &&
        (gTT.pIUO->bP4MagicNumber < VBIOS_P4_CL_FOR_FORCE_SSC_P58_BUG_3196573) && 
       ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) ||
        (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)))
    {
        GblRefmpllSpreadEn = 1;
    }
    else
    {
        GblRefmpllSpreadEn = gTT.perfMemClkBaseEntry.Flags1.PMC11EF1MPLLSSC;
    }

    GblSdmEn = gTT.perfMemClkBaseEntry.Flags0.PMC11EF0SDM;
    if ((GblRefmpllSpreadEn) || (GblSdmEn)) {
        gddr_callwlate_ssd_variables(refmpll_vco_freq_num);
    }

    // print out pll settings for review
    FW_MBOX_WR32( 8, gbl_target_freqMHz);
    FW_MBOX_WR32( 9, drampll_n_val);
    FW_MBOX_WR32(10, drampll_divby2);
    FW_MBOX_WR32(11, refmpll_n_val);
    FW_MBOX_WR32(12, refmpll_p_val);
    FW_MBOX_WR32(13, refmpll_sdm_din);
    FW_MBOX_WR32(14, refmpll_vco_freq);

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

GCC_ATTRIB_SECTION("memory", "gddr_callwlate_drampll_coeff")
void
gddr_callwlate_drampll_coeff
(
        void
)
{
    LwU32 drampll_vco_freq;

    drampll_p_val  = 0;

    drampll_m_val  = 1;
    //    drampll_n_val computed in refmpll_coeff
    drampll_vco_freq = gbl_computation_freqMHz / drampll_m_val;

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


GCC_ATTRIB_SECTION("memory", "gddr_callwlate_onesrc_coeff")
void
gddr_callwlate_onesrc_coeff
(
        void
)
{

    gbl_onsrc_divby_val = (int) gbl_target_freqKHz / 1600;
    return;
}


// get vref_code3 for current gddr_target_clk_path and restore fbio_delay
GCC_ATTRIB_SECTION("memory", "gddr_save_vref_code3")
inline
void
gddr_save_vref_code3
(
void
)
{
    LwU32 fbio_delay;

    lwr_reg->pfb_fbpa_fbio_delay = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DELAY);
    fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _PRIV_SRC, GDDR_PRIV_SRC_REG_SPACE_ONESRC, lwr_reg->pfb_fbpa_fbio_delay);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY,fbio_delay);   // switch priv_src to read vref_code3

    new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0 = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
    lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0;

    fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _PRIV_SRC, GDDR_PRIV_SRC_REG_SPACE_REFMPLL, lwr_reg->pfb_fbpa_fbio_delay);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY,fbio_delay);   // switch priv_src to read vref_code3

    new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1 = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
    lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1;

    fbio_delay = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY, _PRIV_SRC, GDDR_PRIV_SRC_REG_SPACE_DRAMPLL, lwr_reg->pfb_fbpa_fbio_delay);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY,fbio_delay);   // switch priv_src to read vref_code3

    new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2 = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
    lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2;
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY, lwr_reg->pfb_fbpa_fbio_delay);  // restore

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_in_boot_training")
inline
void
gddr_in_boot_training
(
    LwBool in_boot_trng
)
{
  GblInBootTraining = in_boot_trng;
  return; 
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_shadow_reg_space")
void
gddr_pgm_shadow_reg_space
(
    void
)
{
    LwU32 fbio_delay;
    LwU32 fbiotrng_priv_cmd=0;

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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY,fbio_delay);

    //VREF update only done only when boot time training (initial switch to p0) or 
    //if boot time training is skipped in the vbios
    //From Ampere bringup - Dq_VREF_MID code not perf copy protected. 
    if (((GblInBootTraining == LW_TRUE) || (GblSkipBootTimeTrng == 1)) && (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH))
    {

      //Disabling the dqs update such that vref code write (broadcast_misc1) doesn't affect edc vrefs - esp with vref tracking
      fbiotrng_priv_cmd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_PRIV_CMD, _DISABLE_DQS_UPDATE,1,fbiotrng_priv_cmd);
      SEQ_REG_RMW32(LOG,LW_PFB_FBPA_FBIOTRNG_SUBP0_PRIV_CMD,fbiotrng_priv_cmd,DRF_SHIFTMASK(LW_PFB_FBPA_FBIOTRNG_SUBP0_PRIV_CMD_DISABLE_DQS_UPDATE));
      SEQ_REG_RMW32(LOG,LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD,fbiotrng_priv_cmd,DRF_SHIFTMASK(LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD_DISABLE_DQS_UPDATE));

      if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) 
      {
         
         lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_CODE, REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD8,lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3),lwr_reg->pfb_fbio_delay_broadcast_misc1);
         lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQS_VREF_CODE, REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD9,lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3),lwr_reg->pfb_fbio_delay_broadcast_misc1);
         lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_MID_CODE, REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD8,lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3),lwr_reg->pfb_fbio_delay_broadcast_misc1);
      }
      else //gddr6x
      {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 =  gTT.perfMemClkBaseEntry.spare_field6;
      }
      SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1,lwr_reg->pfb_fbio_delay_broadcast_misc1);

      //Restoring the dqs update to allow vref tracking to update the edc vrefs
      fbiotrng_priv_cmd = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_PRIV_CMD, _DISABLE_DQS_UPDATE,0,fbiotrng_priv_cmd);
      SEQ_REG_RMW32(LOG,LW_PFB_FBPA_FBIOTRNG_SUBP0_PRIV_CMD,fbiotrng_priv_cmd,DRF_SHIFTMASK(LW_PFB_FBPA_FBIOTRNG_SUBP0_PRIV_CMD_DISABLE_DQS_UPDATE));
      SEQ_REG_RMW32(LOG,LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD,fbiotrng_priv_cmd,DRF_SHIFTMASK(LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD_DISABLE_DQS_UPDATE));
    }
    return;
}

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
GCC_ATTRIB_SECTION("resident", "fb_pa_sync")
void
fb_pa_sync
(
    LwU8 segid,
    LwU8    fn
)
{
// status:
//   segid[15:8]
//   sync_fn[7:4]
//   sync_stat[3:2]   1=req/ack, 3=clr
//   running[1]
//   done[0]

    LwU32 status;
    LwU32 ctrl;

    // base status (segid, function)
    status = ((segid & 0xFF) << 8) | ((fn & 0xF) << 4);
    ctrl = status  | PA_CTRL_START;
    status |= PA_STATUS_RUNNING;

    // FB polls for sync request

    // PA sets its status requesting sync
    seq_pa_write(LW_CFBFALCON_SYNC_STATUS, (status|PA_STATUS_SYNC_REQ|PA_STATUS_SYNC_VALID));

    // Save sync req for FB decode
    //if (gbl_fb_sync_cnt+1) >= MAX_SYNC_REQ)
    //  halt error
    //else
    //  gbl_pa_sync_req[gbl_fb_sync_cnt] = (status|PA_STATUS_SYNC_REQ|PA_STATUS_SYNC_VALID);

    // PA polls on sync_ctrl for segid, sync_fn & sync_ack bit to be set
    SEQ_REG_POLL32(LW_CFBFALCON_SYNC_CTRL, (ctrl|PA_CTRL_SYNC_ACK), 0xFFFF);

    // FB falcon poll that all PA's are requesting same sync_fn,
    // writes sync_ctrl with ack bit,
    // FB exelwtes sync function, waits for sync_clr in sync_status

    // PA set sync_clear
    seq_pa_write(LW_CFBFALCON_SYNC_STATUS, (status|PA_STATUS_SYNC_CLEAR));

    // PA polls for sync_stat in sync_ctrl to be 0
    SEQ_REG_POLL32(LW_CFBFALCON_SYNC_CTRL, ctrl, 0xFFFF);

    // FB writes sync_ctrl to clear sync_ack

    // for FB to keep track of how many sync points are needed
    gbl_fb_sync_cnt++;
}
#endif

GCC_ATTRIB_SECTION("memory", "gddr_load_reg_values")
void
gddr_load_reg_values
(
        void
)
{
    lwr_reg->pfb_fbpa_fbio_spare                    = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_SPARE);
    lwr_reg->pfb_fbpa_fbio_cfg1                     = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CFG1);
    lwr_reg->pfb_fbpa_fbio_cfg8                     = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CFG8);

    lwr_reg->pfb_fbpa_fbio_cfg_pwrd                 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CFG_PWRD);
    lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl             = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL);
    lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1            = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL1);
    lwr_reg->pfb_fbpa_fbio_cfg10                    = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CFG10);
    lwr_reg->pfb_fbpa_fbio_config5                  = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CONFIG5);
    lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2           = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2);
    lwr_reg->pfb_fbpa_fbio_adr_ddll                 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_ADR_DDLL);
    lwr_reg->pfb_fbpa_training_patram               = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_PATRAM);
    lwr_reg->pfb_fbpa_training_ctrl                 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL);
    lwr_reg->pfb_fbpa_training_rw_ldff_ctrl         = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_LDFF_CTRL);
    lwr_reg->pfb_fbpa_ltc_bandwidth_limiter         = REG_RD32(BAR0, LW_PFB_FBPA_LTC_BANDWIDTH_LIMITER);

    //reading the fbpa register to make RMW         the fields that gets affected for new frequency
    lwr_reg->pfb_fbpa_generic_mrs                   = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS);
    lwr_reg->pfb_fbpa_generic_mrs1                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS1);
    lwr_reg->pfb_fbpa_generic_mrs2                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS2);
    lwr_reg->pfb_fbpa_generic_mrs3                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS3);
    lwr_reg->pfb_fbpa_generic_mrs4                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS4);
    lwr_reg->pfb_fbpa_generic_mrs5                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS5);
    lwr_reg->pfb_fbpa_generic_mrs6                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS6);
    lwr_reg->pfb_fbpa_generic_mrs7                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS7);
    lwr_reg->pfb_fbpa_generic_mrs8                  = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS8);
    lwr_reg->pfb_fbpa_generic_mrs10                 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS10);
    lwr_reg->pfb_fbpa_generic_mrs14                 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS14);
    lwr_reg->pfb_fbpa_timing1                       = REG_RD32(BAR0, LW_PFB_FBPA_TIMING1);
    lwr_reg->pfb_fbpa_timing11                      = REG_RD32(BAR0, LW_PFB_FBPA_TIMING11);
    lwr_reg->pfb_fbpa_timing12                      = REG_RD32(BAR0, LW_PFB_FBPA_TIMING12);
    lwr_reg->pfb_fbpa_timing13                      = REG_RD32(BAR0, LW_PFB_FBPA_TIMING13);
    lwr_reg->pfb_fbpa_timing24                      = REG_RD32(BAR0, LW_PFB_FBPA_TIMING24);
    lwr_reg->pfb_fbpa_cfg0                          = REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    lwr_reg->pfb_fbpa_fbio_broadcast                = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);
    lwr_reg->pfb_fbpa_fbio_bg_vtt                   = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BG_VTT);
    lwr_reg->pfb_fbpa_fbio_edc_tracking             = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_EDC_TRACKING);
    lwr_reg->pfb_fbpa_fbio_edc_tracking1            = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_EDC_TRACKING1);
    lwr_reg->pfb_fbpa_refctrl                       = REG_RD32(BAR0, LW_PFB_FBPA_REFCTRL);
    lwr_reg->pfb_fbpa_ptr_reset                     = REG_RD32(BAR0, LW_PFB_FBPA_PTR_RESET);
    lwr_reg->pfb_fbpa_training_cmd0                 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CMD0);
    lwr_reg->pfb_fbpa_training_cmd1                 = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CMD1);
    lwr_reg->pfb_fbpa_ref                           = REG_RD32(BAR0, LW_PFB_FBPA_REF);
    lwr_reg->pfb_fbio_cal_wck_vml                   = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CAL_WCK_VML);
    lwr_reg->pfb_fbio_cal_clk_vml                   = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CAL_CLK_VML);
    lwr_reg->pfb_fbpa_refmpll_cfg                   = REG_RD32(BAR0, LW_PFB_FBPA_REFMPLL_CFG);
    lwr_reg->pfb_fbpa_refmpll_cfg2                  = REG_RD32(BAR0, LW_PFB_FBPA_REFMPLL_CFG2);
    lwr_reg->pfb_fbpa_refmpll_coeff                 = REG_RD32(BAR0, LW_PFB_FBPA_REFMPLL_COEFF);
    lwr_reg->pfb_fbpa_refmpll_ssd0                  = REG_RD32(BAR0, LW_PFB_FBPA_REFMPLL_SSD0);
    lwr_reg->pfb_fbpa_refmpll_ssd1                  = REG_RD32(BAR0, LW_PFB_FBPA_REFMPLL_SSD1);
    lwr_reg->pfb_fbpa_fbio_refmpll_config           = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG);
    lwr_reg->pfb_fbpa_drampll_cfg                   = REG_RD32(BAR0, LW_PFB_FBPA_DRAMPLL_CFG);
    lwr_reg->pfb_fbpa_drampll_coeff                 = REG_RD32(BAR0, LW_PFB_FBPA_DRAMPLL_COEFF);
    lwr_reg->pfb_fbpa_fbio_drampll_config           = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG);
    lwr_reg->pfb_fbpa_fbio_cmos_clk                 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CMOS_CLK);
    lwr_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen  = REG_RD32(BAR0, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN);
    lwr_reg->pfb_fbpa_fbio_cmd_delay                = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CMD_DELAY);
    lwr_reg->pfb_fbpa_fbiotrng_subp0_wck            = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCK);
    lwr_reg->pfb_fbpa_fbiotrng_subp0_wckb           = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCKB);
    lwr_reg->pfb_fbpa_fbiotrng_subp1_wck            = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCK);
    lwr_reg->pfb_fbpa_fbiotrng_subp1_wckb           = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCKB);
    lwr_reg->pfb_fbpa_clk_ctrl                      = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CLK_CTRL);
    lwr_reg->pfb_fbpa_fbiotrng_subo0_cmd_brlshft1   = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0_CMD_BRLSHFT1);
    lwr_reg->pfb_fbpa_nop                           = REG_RD32(BAR0, LW_PFB_FBPA_NOP);
    lwr_reg->pfb_fbpa_fbio_edc_tracking_debug       = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG);
    lwr_reg->pfb_fbpa_fbio_delay                    = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DELAY);
    lwr_reg->pfb_fbpa_testcmd                       = REG_RD32(BAR0, LW_PFB_FBPA_TESTCMD);
    lwr_reg->pfb_fbpa_testcmd_ext                   = REG_RD32(BAR0, LW_PFB_FBPA_TESTCMD_EXT);
    lwr_reg->pfb_fbpa_testcmd_ext_1                 = REG_RD32(BAR0, LW_PFB_FBPA_TESTCMD_EXT_1);
    lwr_reg->pfb_fbpa_fbio_pwr_ctrl                 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_PWR_CTRL);
    lwr_reg->pfb_fbpa_fbio_brlshft                  = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BRLSHFT);
    lwr_reg->pfb_fbpa_fbio_vref_tracking            = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_VREF_TRACKING);
    lwr_reg->pfb_fbpa_fbio_vref_tracking2           = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_VREF_TRACKING2);
    lwr_reg->pfb_fbpa_training_edc_ctrl             = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_EDC_CTRL);
    lwr_reg->pfb_fbpa_fbio_vtt_ctrl2                = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_VTT_CTRL2);
    lwr_reg->pfb_fbpa_training_rw_periodic_ctrl     = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_RW_PERIODIC_CTRL);
    lwr_reg->pfb_fbpa_training_ctrl2                = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_CTRL2);
    lwr_reg->pfb_fbio_delay_broadcast_misc1         = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1);
    lwr_reg->pfb_fbpa_fbio_subp0byte0_vref_tracking1 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_SUBP0BYTE0_VREF_TRACKING1);
    lwr_reg->pfb_fbio_calmaster_cfg                  = REG_RD32(BAR0, LW_PFB_FBIO_CALMASTER_CFG);
    lwr_reg->pfb_fbio_calmaster_cfg2                 = REG_RD32(BAR0, LW_PFB_FBIO_CALMASTER_CFG2);
    lwr_reg->pfb_fbio_calmaster_cfg3                 = REG_RD32(BAR0, LW_PFB_FBIO_CALMASTER_CFG3);
    lwr_reg->pfb_fbio_calmaster_rb                   = REG_RD32(BAR0, LW_PFB_FBIO_CALMASTER_RB);
    lwr_reg->pfb_fbio_cml_clk2                       = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CML_CLK2);
    lwr_reg->pfb_fbio_misc_config                    = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_MISC_CONFIG);

    lwr_reg->pfb_ptrim_sys_fbio_cfg                 = REG_RD32(BAR0, LW_PTRIM_SYS_FBIO_CFG);
    lwr_reg->pfb_fbpa_fbio_mddll_cntl               = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_MDDLL_CNTL);
    lwr_reg->pfb_fbpa_dram_asr                      = REG_RD32(BAR0, LW_PFB_FBPA_DRAM_ASR);
    lwr_reg->pfb_fbpa_config12                      = REG_RD32(BAR0, LW_PFB_FBPA_CONFIG12);
    lwr_reg->pfb_fbpa_fbio_intrpltr_offset          = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET);
    lwr_reg->pfb_fbpa_sw_config                     = REG_RD32(BAR0, LW_PFB_FBPA_SW_CONFIG);
    lwr_reg->pfb_fbpa_fbiotrng_subp0_priv_cmd       = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP0_PRIV_CMD);
    lwr_reg->pfb_fbpa_fbiotrng_subp1_priv_cmd       = REG_RD32(BAR0, LW_PFB_FBPA_FBIOTRNG_SUBP1_PRIV_CMD);
    lwr_reg->pfb_fbpa_fbio_wck_wclk_pad_ctrl        = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_WCK_CLK_PAD_CTRL);

    // get vref_code3 for current gddr_target_clk_path and restore fbio_delay
    if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) {
       lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0;
       new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0;
    } else if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
       lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1;
       new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1;
    } else {
       lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2;
       new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2;
    }
    

    gbl_prev_edc_tr_enabled = REF_VAL(LW_PFB_FBPA_FBIO_EDC_TRACKING_EN,lwr_reg->pfb_fbpa_fbio_edc_tracking);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_calc_exp_freq_reg_values")
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

    if (cl >= 0x5)
    {
        cl = cl - 0x5;
    }
    else
    {
        bError = LW_TRUE;
    }
    //extended cl
    msb_cl = REF_VAL(4:4, cl);
    cl     = REF_VAL(3:0, cl);

    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5)
    {
        new_reg->pfb_fbpa_config2  = gTT.perfMemTweakBaseEntry.Config2;
    } else { //g5x and g6
        //for gddr6 only
        if (((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) && (wr > 0x23))
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
    msb_wr =  REF_VAL(4:4, wr);
    wr     =  REF_VAL(3:0, wr);

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
    mem_info_entry = TABLE_VAL(mInfoEntryTable->mie1500);
    MemInfoEntry1500*  my_mem_info_entry = (MemInfoEntry1500 *) (&mem_info_entry);

    gbl_vendor_id = REF_VAL(15:12,mem_info_entry);
    gbl_index = REF_VAL(11:8,mem_info_entry);
    gbl_strap = REF_VAL(7:4,mem_info_entry);
    gbl_type = REF_VAL(3:0,mem_info_entry);

    LwU8 AddrTerm;
    Mrs8AddrTerm = 0;
    if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) { //FIXME - check with gautam/Abhijith/Bob

        if (clamshell == LW_PFB_FBPA_CFG0_CLAMSHELL_X32) {
            AddrTerm = AddrTerm0;
        } else {
            AddrTerm = AddrTerm1;
        }

        mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8, _ADR_GDDR6X_CA_TERM,AddrTerm,mrs_reg);
        if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            
            mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8, _ADR_CATH,AddrTerm,mrs_reg);

            if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
                mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS8,_ADR_PLL_RANGE, 1, mrs_reg);
            }
        }
        Mrs8AddrTerm = mrs_reg;

    } else {
        if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) {
            mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8,_ADR_MICRON_PLL_RANGE_MSB,gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5PllRange,mrs_reg); //PLL Range MSB
        }
    }

    if ((gbl_ddr_mode !=  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X )) {
        mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8, _ADR_GDDR5_CL_EHF,msb_cl,mrs_reg);
        mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS8, _ADR_GDDR5_WR_EHF,msb_wr,mrs_reg);
    } else {
        mrs_reg = bit_assign(mrs_reg,8,msb_cl,1);
        mrs_reg = bit_assign(mrs_reg,9,msb_wr,1);
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

    Mrs1AddrTerm = 0;
    if (!((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))) {
        if (clamshell == LW_PFB_FBPA_CFG0_CLAMSHELL_X32) {
            mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS1,_ADR_GDDR5_ADDR_TERM,AddrTerm0,mrs_reg);
        } else {
            mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS1,_ADR_GDDR5_ADDR_TERM,AddrTerm1,mrs_reg);
        }
        Mrs1AddrTerm = mrs_reg;
    } else {
        if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) {
          if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) { //pll range not supported for g6x
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
        }
        //if (gbl_target_freqMHz > 4900) {
        //  mrs_reg = bit_assign(mrs_reg,5,1,1);
        //}
    } 
    mrs_reg = FLD_SET_DRF_NUM(_PFB,_FBPA_GENERIC_MRS1,_ADR_GDDR5_DATA_TERM,gTT.perfMemTweakBaseEntry.pmtbe375368.DataTerm,mrs_reg);
    mrs_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_DRIVE_STRENGTH,gTT.perfMemTweakBaseEntry.pmtbe383376.DrvStrength,mrs_reg);

    new_reg->pfb_fbpa_generic_mrs1 = mrs_reg  ;

    //Programming MRS2
    new_reg->pfb_fbpa_generic_mrs2 = lwr_reg->pfb_fbpa_generic_mrs2;
    //Programming MRS3
    LwU32 tCCDL;
    tCCDL = REF_VAL(LW_PFB_FBPA_CONFIG3_CCDL,gTT.perfMemTweakBaseEntry.Config3);
    new_reg->pfb_fbpa_generic_mrs3 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_BANK_GROUPS,tCCDL,lwr_reg->pfb_fbpa_generic_mrs3);
    if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
        new_reg->pfb_fbpa_generic_mrs3 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR6X_WR_SCALING,mrs3_wr_scaling,new_reg->pfb_fbpa_generic_mrs3);
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

    new_reg->pfb_fbpa_timing12 = spare_reg;
    new_reg->pfb_fbpa_timing11 = spare_reg1;
    new_reg->pfb_fbpa_config0  = gTT.perfMemTweakBaseEntry.Config0;
    new_reg->pfb_fbpa_config1  = gTT.perfMemTweakBaseEntry.Config1;
    new_reg->pfb_fbpa_timing10 = gTT.perfMemTweakBaseEntry.Timing10;

    //timing24 - programming xs offset for hw based srx
    if (gbl_selfref_opt == LW_TRUE) 
    {
       // was hardcoded to 0xA previously
       new_reg->pfb_fbpa_timing24 = FLD_SET_DRF_NUM(_PFB,_FBPA_TIMING24,_XS_OFFSET, gTT.perfMemClkStrapEntry.Gddr6Param0.PMC11SEG6P0Timing24TxsOffset,lwr_reg->pfb_fbpa_timing24);
    }

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

    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
      if (gbl_pam4_mode == LW_TRUE)
      {
        new_reg->pfb_fbpa_fbio_broadcast = FLD_SET_DRF(_PFB, _FBPA_FBIO_BROADCAST, _PAM4_MODE, _ENABLE,new_reg->pfb_fbpa_fbio_broadcast);
      }
      else
      {
        new_reg->pfb_fbpa_fbio_broadcast = FLD_SET_DRF(_PFB, _FBPA_FBIO_BROADCAST, _PAM4_MODE, _DISABLE,new_reg->pfb_fbpa_fbio_broadcast);
      }
      g6x_fbio_broadcast =  new_reg->pfb_fbpa_fbio_broadcast;
    }
    new_reg->pfb_fbpa_fbio_broadcast = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BROADCAST,_G5X_CMD_MODE,gbl_g5x_cmd_mode,new_reg->pfb_fbpa_fbio_broadcast);


    //gbl_vendor_id = my_mem_info_entry->MIEVendorID;

    if (my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) {
        if ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
            new_reg->pfb_fbpa_generic_mrs8 = bit_assign(new_reg->pfb_fbpa_generic_mrs8,9,gbl_g5x_cmd_mode,1); //A9 operation mode - QDR
        } else if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            if ((isDensity16GB) && (gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0MemDLL == PMCT_STRAP_MEMORY_DLL_DISABLED)) {
                new_reg->pfb_fbpa_generic_mrs10 = bit_assign(new_reg->pfb_fbpa_generic_mrs10,9,0,1);//forcing to use DDR mode
            }
            else {
                new_reg->pfb_fbpa_generic_mrs10 = bit_assign(new_reg->pfb_fbpa_generic_mrs10,9,gbl_g5x_cmd_mode,1);//reusing g5x_cmd_mode to g6 qdr mode
            }
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
	new_reg->pfb_fbpa_fbio_vtt_ctrl2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CTRL2, _CDB_VAUXC, gbl_target_vauxc_level,lwr_reg->pfb_fbpa_fbio_vtt_ctrl2);
	new_reg->pfb_fbpa_fbio_vtt_ctrl2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CTRL2, _CDB_VAUXP, gbl_target_vauxc_level,new_reg->pfb_fbpa_fbio_vtt_ctrl2);
	new_reg->pfb_fbpa_fbio_vtt_ctrl2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VTT_CTRL2, _CDB_VCLAMP, gbl_target_vauxc_level,new_reg->pfb_fbpa_fbio_vtt_ctrl2);

        //Comp Vauxc
        new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBIO_CALGROUP_VTTGEN_VAUXGEN, _VTT_LV_VAUXC, gbl_target_vauxc_level,lwr_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
        new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBIO_CALGROUP_VTTGEN_VAUXGEN, _VTT_LV_VAUXP, gbl_target_vauxc_level,new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
        new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen = FLD_SET_DRF_NUM(_PFB, _FBIO_CALGROUP_VTTGEN_VAUXGEN, _VTT_LV_VCLAMP, gbl_target_vauxc_level,new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
        FW_MBOX_WR32(13, new_reg->pfb_fbpa_fbio_vtt_ctrl2);
        FW_MBOX_WR32(14, new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
    }

    //fbio_cfg12
    spare_reg = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CFG12);
    GblRxDfe = gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4RxDfe;
    GblEnMclkAddrTraining = gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4AddrTraining;

    //using vbios spare_field6[0] to determine if we need to enable wck pin mode
    LwU8 spare_field6 = gTT.perfMemClkBaseEntry.spare_field6;
    if (spare_field6 & 0x1)  GblEnWckPinModeTr = LW_TRUE;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TRAINING_DATA_SUPPORT))
    //Checking if boot time training table was done and if address training was enabled there
    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
        (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
        BOOT_TRAINING_FLAGS *pTrainingFlags = &gTT.bootTrainingTable.MemBootTrainTblFlags;
        GblBootTimeAdrTrng  = pTrainingFlags->flag_g6_addr_tr_en;
        GblSkipBootTimeTrng = pTrainingFlags->flag_skip_boot_training;

        if ((GblSkipBootTimeTrng == 0) && (GblBootTimeAdrTrng == 1) && (GblAddrTrPerformedOnce == LW_FALSE)){
            enableEvent(EVENT_SAVE_TRAINING_DATA);
            GblAddrTrPerformedOnce = LW_TRUE;
        }
    }
#endif   // FBFALCON_JOB_TRAINING_DATA_SUPPORT

    spare_reg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG12,_TX_EN_EARLY,gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4TXEnEarly,spare_reg);
    EdcRate = 0;
    if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
        (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
        EdcRate = REF_VAL(LW_PFB_FBPA_CFG0_EDC_RATE, lwr_reg->pfb_fbpa_cfg0);
    }


    //addition apart from doc
    if ((gbl_g5x_cmd_mode && ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)))  || ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (EdcRate == LW_PFB_FBPA_CFG0_EDC_RATE_HALF))) {
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
	    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) //tRAS handled differently for g6x
        {
            new_reg->pfb_fbpa_generic_mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR6X_RAS,tRAS,lwr_reg->pfb_fbpa_generic_mrs5);
            if (tRAS > 0x3F) //RAS value more than 63 need to set MSB - MRS5[5]
            {
                new_reg->pfb_fbpa_generic_mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR6X_RAS_MSB,1,new_reg->pfb_fbpa_generic_mrs5);
            }
            else
            {
                new_reg->pfb_fbpa_generic_mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR6X_RAS_MSB,0,new_reg->pfb_fbpa_generic_mrs5);
            }
        }
        else
        {
          new_reg->pfb_fbpa_generic_mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR5_RAS,tRAS,lwr_reg->pfb_fbpa_generic_mrs5);
        }
    }


    //updating CRCWL and CRCCL values - addition apart from doc
    crcwl = REF_VAL(LW_PFB_FBPA_CONFIG5_WRCRC,gTT.perfMemTweakBaseEntry.Config5);
    crccl = REF_VAL(LW_PFB_FBPA_TIMING12_RDCRC,new_reg->pfb_fbpa_timing12);
    crcwl = crcwl - 7;
    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) && (gbl_g5x_cmd_mode)) {
      if (crccl > 3) {
	    crccl -=  4;   
      }
    }
    new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4,_ADR_GDDR5_CRCWL,crcwl,lwr_reg->pfb_fbpa_generic_mrs4);
    new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4,_ADR_GDDR5_CRCCL,crccl,new_reg->pfb_fbpa_generic_mrs4);

    gblEnEdcTracking  =  gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6EdcTrackEn;
    gblElwrefTracking =  gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6VrefTrackEn;

    gblEdcTrGain = gTT.perfMemClkStrapEntry.EdcTrackingGain;
    gblVrefTrGain = gTT.perfMemClkStrapEntry.VrefTrackingGain;
    gbl_mta_en = gTT.perfMemClkBaseEntry.Flags0.PMC11EF0MTA;
    gbl_asr_acpd = gTT.perfMemClkStrapEntry.Flags7.PMC11SEF7ASRACPD;
// set gbl_ac_abi via PMCT CABI 2704949, 2781005
    gbl_ac_abi = gTT.perfMemClkStrapEntry.Flags9.PMC11SEF9EnCabiMode;
    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_power_on_internal_vref")
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

GCC_ATTRIB_SECTION("memory", "gddr_disable_acpd_and_replay")
void
gddr_disable_acpd_and_replay
(
        void
)
{
    LwU32 fbpa_cfg0;

    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    //disable replay
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _REPLAY,_DISABLED,fbpa_cfg0);
    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_disable_refresh")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_REFCTRL,fbpa_refctrl);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_disable_periodic_training")
void
gddr_disable_periodic_training
(
        void
)
{
    LwU32 training_cmd;

    if ((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL) ||
        (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL)) {

        //TODO:must do on all partition
        training_cmd = lwr_reg->pfb_fbpa_training_cmd1;
        training_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _DISABLED ,training_cmd);
        lwr_reg->pfb_fbpa_training_cmd1 = training_cmd;
        lwr_reg->pfb_fbpa_training_cmd0 = training_cmd;

        gddr_pgm_trng_lower_subp(training_cmd);

        //disabling periodic training average
        lwr_reg->pfb_fbpa_training_rw_periodic_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_RW_PERIODIC_CTRL,_EN_AVG, _INIT, lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);

        //disabling periodic shift training
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_WR, _DIS, lwr_reg->pfb_fbpa_training_ctrl2);
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_RD, _DIS, lwr_reg->pfb_fbpa_training_ctrl2);
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB,_FBPA_TRAINING_CTRL2,_ILW_PATT_WR, _DIS, lwr_reg->pfb_fbpa_training_ctrl2);
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB,_FBPA_TRAINING_CTRL2,_ILW_PATT_RD, _DIS, lwr_reg->pfb_fbpa_training_ctrl2);

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw) {
            seq_pa_write(LW_PFB_FBPA_TRAINING_CMD1,training_cmd);
            seq_pa_write(LW_PFB_FBPA_TRAINING_RW_PERIODIC_CTRL,lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);
            seq_pa_write(LW_PFB_FBPA_TRAINING_CTRL2,lwr_reg->pfb_fbpa_training_ctrl2);
        }
        else
#endif
        {
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,training_cmd);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_PERIODIC_CTRL,lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2,lwr_reg->pfb_fbpa_training_ctrl2);
        }
    }

    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_send_one_refresh")
void
gddr_send_one_refresh
(
        void
)
{
    LwU32 fbpa_ref;

    fbpa_ref = lwr_reg->pfb_fbpa_ref;
    fbpa_ref = FLD_SET_DRF(_PFB, _FBPA_REF, _CMD,_REFRESH_1,fbpa_ref);
    lwr_reg->pfb_fbpa_ref = fbpa_ref;

    //Adding more SW refresh to account for loss of refresh between periodic ref and self ref in slow ltcclk - bug #1706386
    LwU8 i;
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {
        for (i=0; i < 10; i++)
        {
            seq_pa_write(LW_PFB_FBPA_REF, fbpa_ref);
        }
    }
    else
#endif
    {
        //Adding more refresh from -standsim
        for (i=0; i < 5; i++)
        {
            REG_WR32(LOG, LW_PFB_FBPA_REF,fbpa_ref);
        }
    }
    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_send_one_refresh")
void
gddr_set_memvref_range
(
        void
)
{
    LwU32 fbpa_mrs7;
    LwU32 mem_info_entry;


    fbpa_mrs7 = new_reg->pfb_fbpa_generic_mrs7;
    //disabling low freq mode at source frequency - 
    //enabling the lf mode, if pmct says, would be done at target freq
    if (gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0LowFreq == 0 ){
        if (!((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
              (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))) {
            fbpa_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_LOW_FREQ_MODE,_DISABLED,fbpa_mrs7);
        } else {
            fbpa_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR6X_LOW_FREQ_MODE,_DISABLED,fbpa_mrs7);
        }
    }

    //programming Internal Vrefc
    //Asatish FOLLOWUP need to program twice - based on speed  // Followup to confirm with Gautam/Abhijith/bob
    LwU8 InternalVrefc;
    InternalVrefc = gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5G5XVref;
    if (InternalVrefc == 0) { //disabled  70%
        if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
            (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) ||
            (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X)) {
            fbpa_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR6X_HALF_VREFC, _OFF,fbpa_mrs7);
        }
    }

    //Need to program VDD_RANGE before SRE bug :
    // Followup to confirm with Gautam/Abhijith/bob
    LwU8 Mrs7VddRange = gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4MRS7VDD;
    if (Mrs7VddRange == 0) { //Enable
        fbpa_mrs7 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_VDD_RANGE, 1, fbpa_mrs7);
    } else { //Disable
        fbpa_mrs7 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_VDD_RANGE, 0, fbpa_mrs7);
    }

    new_reg->pfb_fbpa_generic_mrs7 = fbpa_mrs7;
    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS7,fbpa_mrs7);
    SEQ_WAIT_NS(500);

    //changed the sequence from Pascal
    //Updating the Addr termination always at low speed clock for high speed
    //target frequency
    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        if ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
            SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS1,Mrs1AddrTerm);
        } else {
            SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS8,Mrs8AddrTerm);
        }
    }
    SEQ_WAIT_NS(500);
    //if dram is from micron and is G5x mode change the MRS14 CTLE
    mem_info_entry = TABLE_VAL(mInfoEntryTable->mie1500);
    MemInfoEntry1500*  my_mem_info_entry = (MemInfoEntry1500 *) (&mem_info_entry);
    if ((my_mem_info_entry->MIEVendorID == MEMINFO_ENTRY_VENDORID_MICRON) &&
        ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X) ||
         (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
         (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)))
    {
        LwU32 MicronCoreVoltage;
        LwU32 MicronSubRegisterSelect;
        //MicronCoreVoltage = pMemClkStrapTable->MicronGDDR5MRS14Offset;
        //MicronCoreVoltage = selectedMemClkStrapTable.MicronGDDR5MRS14Offset.PMC11SEG5xMRS14Voltage;

        MicronSubRegisterSelect = ((gTT.perfMemClkStrapEntry.MicronGDDR5MRS14Offset.PMC11SEG5xMRS14SubRegSel << 2 ) |
                (gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6MRS14SubRegSelect));
        MicronCoreVoltage = gTT.perfMemClkStrapEntry.MicronGDDR5MRS14Offset.PMC11SEG5xMRS14Voltage;
        new_reg->pfb_fbpa_generic_mrs14 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS14, _ADR_MICRON_CORE_VOLTAGE_ADDENDUM, MicronCoreVoltage, lwr_reg->pfb_fbpa_generic_mrs14);
        new_reg->pfb_fbpa_generic_mrs14 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS14, _ADR_MICRON_SUBREG_SELECT, MicronSubRegisterSelect, new_reg->pfb_fbpa_generic_mrs14);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS14,new_reg->pfb_fbpa_generic_mrs14);
    }

    return;

}


GCC_ATTRIB_SECTION("memory", "gddr_update_gpio_trigger_wait_done")
void
gddr_update_gpio_trigger_wait_done
(
        void
)
{
    LwU32 GpioTrigger;
    LwU32 trig_update;
    //update trigger
    GpioTrigger = REG_RD32(BAR0, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER);
    GpioTrigger = FLD_SET_DRF(_PMGR,_GPIO_OUTPUT_CNTL_TRIGGER, _UPDATE, _TRIGGER, GpioTrigger);
    REG_WR32(LOG, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER,GpioTrigger);

    GpioTrigger = 0;

    do {

        GpioTrigger = REG_RD32(BAR0, LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER);
        trig_update = REF_VAL(LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE,GpioTrigger);
        FW_MBOX_WR32(10, trig_update);
        FW_MBOX_WR32(11, GpioTrigger);

    }while (trig_update == LW_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_PENDING) ;

    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_set_fbvref")
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
    REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1000);

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

    gpio_vref_output_cntl = REG_RD32(BAR0,LW_GPIO_OUTPUT_CNTL(vrefGPIOInx));

    LwU8 lwr_fbvref = gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1FBVREF;

    if(lwr_fbvref == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVREF_LOW)
    {
        gpio_vref_output_cntl = FLD_SET_DRF(_GPIO,_OUTPUT_CNTL, _IO_OUTPUT,_1, gpio_vref_output_cntl);
    }
    else
    {
        gpio_vref_output_cntl = FLD_SET_DRF(_GPIO,_OUTPUT_CNTL, _IO_OUTPUT,_0, gpio_vref_output_cntl);
    }
    gpio_vref_output_cntl = FLD_SET_DRF(_GPIO, _OUTPUT_CNTL, _IO_OUT_EN, _YES,gpio_vref_output_cntl);

    REG_WR32(LOG,LW_GPIO_OUTPUT_CNTL(vrefGPIOInx),gpio_vref_output_cntl);

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


GCC_ATTRIB_SECTION("memory", "gddr_precharge_all")
void
gddr_precharge_all
(
        void
)
{
    LwU32 dram_ack_en;
    LwU32 dram_ack_dis;

    // Enable DRAM_ACK
    dram_ack_en = lwr_reg->pfb_fbpa_cfg0;
    dram_ack_en = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED, dram_ack_en);

    // Disable DRAM_ACK
    dram_ack_dis = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED, dram_ack_en);
    lwr_reg->pfb_fbpa_cfg0 = dram_ack_dis;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw)
    {
        seq_pa_write(LW_PFB_FBPA_CFG0, dram_ack_en);

    // check this do we need to wait in the PA?
    // Force a precharge
        seq_pa_write_stall(LW_PFB_FBPA_PRE, 0x00000001);

        seq_pa_write(LW_PFB_FBPA_CFG0, dram_ack_dis);
    }
    else
#endif
    {
        REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack_en);

    // check this do we need to wait in the PA?
        // Force a precharge
        REG_WR32_STALL(LOG, LW_PFB_FBPA_PRE, 0x00000001);

        REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack_dis);
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_self_refresh")
void
gddr_enable_self_refresh
(
        void
)
{
    if (gbl_selfref_opt == LW_FALSE)
    {
// to be deprecated
        LwU32 testcmd_ext=0;
        LwU32 testcmd = 0;
        LwU32 testcmd_ext_1=0;
        //
        //First cycle of SREF -- Issue SRE command with CKE_n = 1
        //G6 uses TESTCMD_EXT for CA9_Rise
        if((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
            testcmd_ext_1 = lwr_reg->pfb_fbpa_testcmd_ext_1;
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _ADR,       _DDR6_SELF_REF_ENTRY1,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB0,      _DDR6_SELF_REF_ENTRY1,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB1,      _DDR6_SELF_REF_ENTRY1,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _USE_ADR,   _DDR6_SELF_REF_ENTRY1,testcmd_ext_1);
            lwr_reg->pfb_fbpa_testcmd_ext_1 = testcmd_ext_1;
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT_1,testcmd_ext_1);

            testcmd_ext = lwr_reg->pfb_fbpa_testcmd_ext;
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ACT,      _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM0,_DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM1,_DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ADR_DRAM1,_DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _DEBUG,    _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ODT,      _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _RW_CMD,   _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _MRS_CMD,  _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _DR_CMD,   _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _4CK_CMD,  _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _LAM_CMD,  _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ACT_CMD,  _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _SID,      _DDR6_SELF_REF_ENTRY1,testcmd_ext);
            lwr_reg->pfb_fbpa_testcmd_ext = testcmd_ext;
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT,testcmd_ext);

            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,_DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0, _DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1, _DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,_DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,_DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,_DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,_DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _BANK_4, _DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _BANK, _DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR, _DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR_UPPER, _DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD,_DDR6_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,  _DDR6_SELF_REF_ENTRY1,testcmd);

        } else {
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS, _DDR5_SELF_REF_ENTRY1, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS, _DDR5_SELF_REF_ENTRY1, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE , _DDR5_SELF_REF_ENTRY1, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,_DDR5_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,_DDR5_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0, _DDR5_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1, _DDR5_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD,_DDR5_SELF_REF_ENTRY1,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,  _DDR5_SELF_REF_ENTRY1,testcmd);
        }

        lwr_reg->pfb_fbpa_testcmd = testcmd;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD,testcmd);

        //========================================================
        //Second cycle of SREF - Issue a NOP with CKE_n=1 and HOLD
        //G6 uses TESTCMD_EXT for CA9_Rise

        if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)  || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
            testcmd_ext_1 = lwr_reg->pfb_fbpa_testcmd_ext_1;
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _ADR,       _DDR6_SELF_REF_ENTRY2,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB0,      _DDR6_SELF_REF_ENTRY2,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB1,      _DDR6_SELF_REF_ENTRY2,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _USE_ADR,   _DDR6_SELF_REF_ENTRY2,testcmd_ext_1);
            lwr_reg->pfb_fbpa_testcmd_ext_1 = testcmd_ext_1;
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT_1,testcmd_ext_1);

            testcmd_ext = lwr_reg->pfb_fbpa_testcmd_ext;
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ACT,      _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM0,_DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM1,_DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ADR_DRAM1,_DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _DEBUG,    _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ODT,      _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _RW_CMD,   _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _MRS_CMD,  _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _DR_CMD,   _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _4CK_CMD,  _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _LAM_CMD,  _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ACT_CMD,  _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _SID,      _DDR6_SELF_REF_ENTRY2,testcmd_ext);
            lwr_reg->pfb_fbpa_testcmd_ext = testcmd_ext;
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT,testcmd_ext);

            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,_DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0, _DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1, _DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,_DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,_DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,_DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,_DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _BANK_4, _DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _BANK, _DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR, _DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR_UPPER, _DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD,_DDR6_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,  _DDR6_SELF_REF_ENTRY2,testcmd);
        } else {
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES, _DDR5_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE, _DDR5_SELF_REF_ENTRY2,testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,_DDR5_SELF_REF_ENTRY2, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,_DDR5_SELF_REF_ENTRY2, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE ,_DDR5_SELF_REF_ENTRY2, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0 , _DDR5_SELF_REF_ENTRY2, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1 , _DDR5_SELF_REF_ENTRY2, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD, _DDR5_SELF_REF_ENTRY2, testcmd);
            testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO  , _DDR5_SELF_REF_ENTRY2, testcmd);
        }
        lwr_reg->pfb_fbpa_testcmd = testcmd;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD,testcmd);
    }
    else
    {
        LwU32 dram_ack_en;
        LwU32 dram_ack_dis;

        // Enable DRAM_ACK
        dram_ack_en = lwr_reg->pfb_fbpa_cfg0;
        dram_ack_en = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED, dram_ack_en);

        // Disable DRAM_ACK
        dram_ack_dis = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED, dram_ack_en);
        lwr_reg->pfb_fbpa_cfg0 = dram_ack_dis;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw)
        {
            seq_pa_write(LW_PFB_FBPA_CFG0, dram_ack_en);
            seq_pa_write(LW_PFB_FBPA_SELF_REF, LW_PFB_FBPA_SELF_REF_CMD_ENABLED);
            seq_pa_write(LW_PFB_FBPA_CFG0, dram_ack_dis);
        }
        else
#endif
        {
            REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack_en);
            REG_WR32(LOG, LW_PFB_FBPA_SELF_REF, LW_PFB_FBPA_SELF_REF_CMD_ENABLED);
            REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack_dis);
        }
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_set_vml_mode")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CAL_WCK_VML,wck_vml);

    clk_vml = lwr_reg->pfb_fbio_cal_clk_vml;
    clk_vml = FLD_SET_DRF(_PFB, _FBPA_FBIO_CAL_CLK_VML, _SEL, _VML, clk_vml);
    lwr_reg->pfb_fbio_cal_clk_vml = clk_vml;
    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CAL_CLK_VML,clk_vml);


    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_power_intrpl_dll")
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
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                if (ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
                    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_DIGDLL_PWRD,_DISABLE,fbio_cfg_pwrd);
                }
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_DIFFAMP_PWRD,_DISABLE,fbio_cfg_pwrd);
                fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DIFFAMP_PWRD,_DISABLE,fbio_cfg_pwrd);

                fbpa_refmpll_cfg = lwr_reg->pfb_fbpa_fbio_refmpll_config;
                fbpa_refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _IDDQ , _POWER_ON,fbpa_refmpll_cfg);
                lwr_reg->pfb_fbpa_fbio_refmpll_config = fbpa_refmpll_cfg;
                SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,fbpa_refmpll_cfg);
            }
            fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _CLK_TX_PWRD,_ENABLE,fbio_cfg_pwrd);
        }
    } else if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) {
        //if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) { // Change from document - cannot power down refmpll if target clk path is drampll
        //    //power down REFMPLL - DLL
        //    fbpa_refmpll_cfg = lwr_reg->pfb_fbpa_fbio_refmpll_config;
        //    fbpa_refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _IDDQ, _POWER_OFF,fbpa_refmpll_cfg);
        //    lwr_reg->pfb_fbpa_fbio_refmpll_config = fbpa_refmpll_cfg;
        //    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,fbpa_refmpll_cfg);

        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_TX_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQDQS_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        //    fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_DIFFAMP_PWRD,_ENABLE,fbio_cfg_pwrd);

        //}
    }

    lwr_reg->pfb_fbpa_fbio_cfg_pwrd = fbio_cfg_pwrd;
    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);
    
    //ga10x change - powering down mddll for non p5 state
    if (gddr_target_clk_path != GDDR_TARGET_REFMPLL_CLK_PATH) {
      SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_MDDLL_CNTL, 
	FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MDDLL_CNTL, _DQ_MDDLL_PWRD,1, lwr_reg->pfb_fbpa_fbio_mddll_cntl));
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_refmpll_cfg_and_lock_wait")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_CFG,refmpll_cfg);
//    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_REFMPLL_CFG,refmpll_cfg);

    FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1000);
    SEQ_WAIT_NS(50);

    //configure the pll coefficients
    refmpll_coeff = lwr_reg->pfb_fbpa_refmpll_coeff;
    refmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_COEFF, _MDIV ,refmpll_m_val, refmpll_coeff);
    refmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_COEFF, _NDIV ,refmpll_n_val, refmpll_coeff);
    refmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_REFMPLL_COEFF, _PLDIV,refmpll_p_val, refmpll_coeff);
    lwr_reg->pfb_fbpa_refmpll_coeff = refmpll_coeff;

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

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {
        seq_pa_write(LW_PFB_FBPA_REFMPLL_COEFF,refmpll_coeff);
        seq_pa_write(LW_PFB_FBPA_REFMPLL_SSD0,lwr_reg->pfb_fbpa_refmpll_ssd0);
        seq_pa_write(LW_PFB_FBPA_REFMPLL_SSD1,lwr_reg->pfb_fbpa_refmpll_ssd1);
//        seq_pa_write(LW_PFB_FBPA_REFMPLL_CFG2,lwr_reg->pfb_fbpa_refmpll_cfg2);
        seq_pa_write_stall(LW_PFB_FBPA_REFMPLL_CFG2,lwr_reg->pfb_fbpa_refmpll_cfg2);

        // add poll for lock == false
        // changed to poll on enable due to possible race condition on 2 partitions
        // bug 200657070
        lock_val = DRF_DEF(_PFB, _FBPA_REFMPLL_CFG, _PLL_LOCK, _FALSE);
        SEQ_REG_POLL32(LW_PFB_FBPA_REFMPLL_CFG, refmpll_cfg,
            DRF_SHIFTMASK(LW_PFB_FBPA_REFMPLL_CFG_ENABLE));
    }
    else
#endif
    {
        REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_COEFF,refmpll_coeff);
        REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_SSD0,lwr_reg->pfb_fbpa_refmpll_ssd0);
        REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_SSD1,lwr_reg->pfb_fbpa_refmpll_ssd1);
        REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_CFG2,lwr_reg->pfb_fbpa_refmpll_cfg2);
    }
    FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1001);
    SEQ_WAIT_NS(50);

    //Enable refmpll
    refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE,_YES,refmpll_cfg);
    refmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_REFMPLL_CFG, _ENABLE, _YES,refmpll_cfg);
    lwr_reg->pfb_fbpa_refmpll_cfg = refmpll_cfg;
    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_REFMPLL_CFG,refmpll_cfg);

    FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1002);

    //wait for refmpll lock
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {
        lock_val = DRF_DEF(_PFB, _FBPA_REFMPLL_CFG, _PLL_LOCK, _TRUE);
        SEQ_REG_POLL32(LW_PFB_FBPA_REFMPLL_CFG, lock_val,
                DRF_SHIFTMASK(LW_PFB_FBPA_REFMPLL_CFG_PLL_LOCK));
    }
    else
#endif
    {
        do {
            refmpll_cfg =0;
            refmpll_cfg = REG_RD32(BAR0, LW_PFB_FBPA_REFMPLL_CFG);
            lock_val = REF_VAL(LW_PFB_FBPA_REFMPLL_CFG_PLL_LOCK,refmpll_cfg);
        }while (lock_val != 1);
    }


    if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
        fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _REFMPLL, fbio_mode_switch);
        lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV,fbio_mode_switch);
    } else {
        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL, fbio_mode_switch);
        lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
        REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_wdqs_rcvr_sel")
void
gddr_pgm_wdqs_rcvr_sel
(
        void
)
{
    //pmct_table_strap_entry_flag2;

    //LwU8 rcvr_sel = gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2RDQSRcvSel;
    LwU8 rcvr_sel = 0x1;

    //pmct_table_strap_entry_flag2 = TABLE_VAL(pMemClkStrapTable->Flags2);
    // gbl_target_pmct_flags2 = (PerfMemClk11StrapEntryFlags2* ) (&pmct_table_strap_entry_flag2);
    //pmctFlags2 = gTT.perfMemClkStrapEntry.Flags2 &selectedMemClkStrapTable.Flags2;
    if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
        fbio_mode_switch = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _RX_RCVR_SEL, rcvr_sel, fbio_mode_switch);
        lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV, fbio_mode_switch);
    } else {
        LwU32 fbio_cfg_ctrl;
        fbio_cfg_ctrl = lwr_reg->pfb_ptrim_sys_fbio_cfg;
        fbio_cfg_ctrl = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_CFG_, CFG10_RCVR_SEL,rcvr_sel,fbio_cfg_ctrl);
        // Keeping for now for asatish
        //    FW_MBOX_WR32(1, 0xAAAAAAAA);
        //    FW_MBOX_WR32(1, gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2DQWDQSRcvSel);
        //    FW_MBOX_WR32(1, gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2RCVCtrl);
        //    FW_MBOX_WR32(1, gTT.perfMemClkStrapEntry.Flags2.PMC11SEF2RDQSRcvSel);
        //    FW_MBOX_WR32(1, fbio_cfg_ctrl);
        lwr_reg->pfb_ptrim_sys_fbio_cfg = fbio_cfg_ctrl;
        REG_WR32(LOG, LW_PTRIM_SYS_FBIO_CFG,fbio_cfg_ctrl);
    }


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
GCC_ATTRIB_SECTION("memory", "gdrRecalibrate")
void
gdrRecalibrate
(
        void
)
{
    PerfMemClk11Header* pMCHp = gBiosTable.pMCHp;
    PerfMemClkHeaderFlags0 memClkHeaderflags;
    EXTRACT_TABLE(&pMCHp->pmchf0,memClkHeaderflags);
    GblAutoCalUpdate = memClkHeaderflags.PMCHF0ClkCalUpdate;
    new_reg->pfb_fbio_calmaster_cfg2 = lwr_reg->pfb_fbio_calmaster_cfg2;
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
        new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG2, _TRIGGER_CAL_CYCLE, _ENABLED, new_reg->pfb_fbio_calmaster_cfg2);
        new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF_NUM(_PFB, _FBIO_CALMASTER_CFG2, _COMP_POWERUP_DLY, 95, new_reg->pfb_fbio_calmaster_cfg2);
        REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG2,new_reg->pfb_fbio_calmaster_cfg2);
        //REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(0),new_reg->pfb_fbio_calmaster_cfg2);

        //new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG2, _TRIGGER_CAL_CYCLE, _INACTIVE, new_reg->pfb_fbio_calmaster_cfg2);
        //REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG2,new_reg->pfb_fbio_calmaster_cfg2);
        //REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(1),new_reg->pfb_fbio_calmaster_cfg2);

        OS_PTIMER_SPIN_WAIT_US(35);

        //Starting calibration for vdd change and vauxc change
        // uncomment the below two lines if we want to track a lock in cal master since calibrating
        //lwr_reg->pfb_fbio_calmaster_rb = REG_RD32(LOG, LW_PFB_FBIO_CALMASTER_RB);
        //REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_3(0),lwr_reg->pfb_fbio_calmaster_rb);
        // Restoring powerup_dly to init value at end of mclk switch
        new_reg->pfb_fbio_calmaster_cfg2 = FLD_SET_DRF(_PFB, _FBIO_CALMASTER_CFG2, _COMP_POWERUP_DLY, _1024, new_reg->pfb_fbio_calmaster_cfg2);
        REG_WR32(LOG, LW_PFB_FBIO_CALMASTER_CFG2,new_reg->pfb_fbio_calmaster_cfg2);
    }
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_pwrd")
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
    lwr_reg->pfb_fbpa_fbio_cfg_pwrd = fbio_cfg_pwrd;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);

    //ga10x change - powering up mddll for p5
    if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
      SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_MDDLL_CNTL, 
	FLD_SET_DRF(_PFB, _FBPA_FBIO_MDDLL_CNTL, _DQ_MDDLL_PWRD,_INIT, lwr_reg->pfb_fbpa_fbio_mddll_cntl));
    }



    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_dq_term")
void
gddr_pgm_dq_term
(
        void
)
{
    //fbio_cfg10 - GPU termination
    LwU8 GpuTermination = gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0GPUTerm;
    LwU8 GpuDqsTermination = gTT.perfMemClkStrapEntry.Flags3.PMC11SEF3GPUDQSTerm;


//    BITInternalUseOnlyV2* mypIUO = gTT.pIUO;
//    if (mypIUO->bP4MagicNumber < VBIOS_P4_CL_FOR_DQ_DQS_TERMINATION) {
//        GpuTermination  = 0;
//        GpuDqsTermination  = 0;
//    }

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

    SEQ_REG_WR32(BAR0, LW_PFB_FBPA_FBIO_CFG10,lwr_reg->pfb_fbpa_fbio_cfg10);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_reg_values")
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

    fbio_cfg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _TX_EN_SETUP_DQ, gTT.perfMemClkStrapEntry.Flags4.PMC11SEF4TXEnSetupDQ,fbio_cfg1);
    if(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) 
    {
        if (gbl_pam4_mode == LW_TRUE) 
        {
            fbio_cfg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _TX_EN_HOLD_DQ, 3,fbio_cfg1);
        } 
        else
        {
            fbio_cfg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG1, _TX_EN_HOLD_DQ, 0,fbio_cfg1);
        }
    } 
    lwr_reg->pfb_fbpa_fbio_cfg1 = fbio_cfg1;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    //FB unit sim only programe the low freq of fbio_cfg1 brl_shift lowspeed mode

    //Timing1
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TIMING1,new_reg->pfb_fbpa_timing1);

    //Timing10
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TIMING10,new_reg->pfb_fbpa_timing10);

    //timing11
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TIMING11,new_reg->pfb_fbpa_timing11);

    //timing12
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TIMING12,new_reg->pfb_fbpa_timing12);

    //New for Ampere- programming timing24_xs_offset
    //for hw based SRX
    if (gbl_selfref_opt == LW_TRUE) {
      SEQ_REG_WR32(LOG,LW_PFB_FBPA_TIMING24,new_reg->pfb_fbpa_timing24);
    }
    //config0
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG0,new_reg->pfb_fbpa_config0);

    //config1
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG1,new_reg->pfb_fbpa_config1);

    //config2
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG2,new_reg->pfb_fbpa_config2);

    //config3
    LwU32 config3_temp;
    config3_temp = gTT.perfMemTweakBaseEntry.Config3;
    #include "g_lwconfig.h"
#if LWCFG(GLOBAL_FEATURE_SANDBAG)
    #include "lwSandbagList.h"
    #include "dev_lw_xve.h"
    #include "dev_fuse.h"
    // Validate the memory tuning controller high secure fuse
    LwU32 val = REG_RD32(BAR0, LW_FUSE_SPARE_BIT_1);
    // use hardcoded tfaw if not p5/p8 for bug 3263169
    #define HARDCODED_TFAW_VALUE_48           48
    if ((gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) &&
        (val == LW_FUSE_SPARE_BIT_1_DATA_ENABLE))
    {
        // Check if DevID is sandbagged
        LwU8 i;
        LwU32 devId = DRF_VAL(_XVE, _ID, _DEVICE_CHIP, REG_RD32(BAR0, DEVICE_BASE(LW_PCFG) + LW_XVE_ID));
        for (i = 0; i < LW_ARRAY_ELEMENTS(aGLSandbagAllowList); i++)
        {
            if (aGLSandbagAllowList[i].ulDevID == devId)
            {
                config3_temp = FLD_SET_DRF_NUM(_PFB_FBPA, _CONFIG3, _FAW, HARDCODED_TFAW_VALUE_48, config3_temp);
                break;
            }
        }
    }
#endif
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG3,config3_temp);

    //config4
    LwU32 config4_temp;
    LwU32 timing21_temp;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
    getTimingValuesForRefi(&config4_temp, &timing21_temp);
#else
    config4_temp = gTT.perfMemTweakBaseEntry.Config4;
    timing21_temp = gTT.perfMemTweakBaseEntry.Timing21;
#endif

    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG4,config4_temp);

    //config5
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG5,gTT.perfMemTweakBaseEntry.Config5);

    //config6
    LwU32 config6_temp = gTT.perfMemTweakBaseEntry.Config6;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG6,config6_temp);

    //config7
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG7,gTT.perfMemTweakBaseEntry.Config7);

    //config8
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG8,gTT.perfMemTweakBaseEntry.Config8);

    //config9
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG9,gTT.perfMemTweakBaseEntry.Config9);

    //config10
    LwU32 config10_temp;
    config10_temp = gTT.perfMemTweakBaseEntry.Config10;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG10,gTT.perfMemTweakBaseEntry.Config10);

    //config11
    LwU32 config11_temp;
    config11_temp = gTT.perfMemTweakBaseEntry.Config11;

    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
      if ((gbl_mta_en == LW_TRUE) && (gbl_pam4_mode == LW_TRUE))
      {
         config11_temp = FLD_SET_DRF(_PFB, _FBPA_CONFIG11, _MTA, _ON, config11_temp);
      }
      else
      {
         config11_temp = FLD_SET_DRF(_PFB, _FBPA_CONFIG11, _MTA, _OFF, config11_temp);
      }
    }
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG11,config11_temp);

    //FBIO_BG_VTT
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_BG_VTT,new_reg->pfb_fbpa_fbio_bg_vtt);

    //FBIO Broadcast
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST,new_reg->pfb_fbpa_fbio_broadcast);

    //timing21
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TIMING21,timing21_temp);

    //timing22
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TIMING22,gTT.perfMemTweakBaseEntry.Timing22);

    //cal_trng_arb
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CAL_TRNG_ARB,gTT.perfMemTweakBaseEntry.Cal_Trng_Arb);

    //fbio_cfg10
    if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
        gddr_pgm_dq_term();
    }

    //fbio_cfg12
    //wck_8ui mode not supported for g6x yet, bug 2738210 comment #3 FIXME to remove g6x in if case
    if ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) && ((gbl_g5x_cmd_mode) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6))) {
        //LW_PFB_FBPA_FBIO_CFG12_WCK_INTRP_8UI_MODE_ENABLE
        fbio_cfg12 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG12, _WCK_INTRP_8UI_MODE, _ENABLE, new_reg->pfb_fbpa_fbio_cfg12);
    } else {
        //LW_PFB_FBPA_FBIO_CFG12_WCK_INTRP_8UI_MODE_DISABLE
        fbio_cfg12 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG12, _WCK_INTRP_8UI_MODE, _DISABLE, new_reg->pfb_fbpa_fbio_cfg12);
    }

    fbio_cfg12 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CFG12, _INTERP_CAP, gTT.perfMemClkBaseEntry.Param0.PMC11EP0INTERPCAP, fbio_cfg12);

    //Asatish for saving time from p8<->p5 - removing this to update wck intrp mode - TB sequence
    //Asatish removing optimization for TB sequence
    //if(!((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) &&
    //        ((gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) ||
    //                (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)))) {
    new_reg->pfb_fbpa_fbio_cfg12 = fbio_cfg12;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG12,fbio_cfg12);
    //}

    new_reg->pfb_fbpa_fbio_adr_ddll = lwr_reg->pfb_fbpa_fbio_adr_ddll;
    if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH)
    {
      new_reg->pfb_fbpa_fbio_adr_ddll = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_ADR_DDLL, _SUBP0_CMD_CLK_TRIM ,gTT.perfMemClkStrapEntry.ClkDly,new_reg->pfb_fbpa_fbio_adr_ddll);
      new_reg->pfb_fbpa_fbio_adr_ddll = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_ADR_DDLL, _SUBP1_CMD_CLK_TRIM ,gTT.perfMemClkStrapEntry.ClkDly,new_reg->pfb_fbpa_fbio_adr_ddll);
    }
    else
    {
      new_reg->pfb_fbpa_fbio_adr_ddll = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_ADR_DDLL, _SUBP0_CMD_CLK_TRIM ,0,new_reg->pfb_fbpa_fbio_adr_ddll);
      new_reg->pfb_fbpa_fbio_adr_ddll = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_ADR_DDLL, _SUBP1_CMD_CLK_TRIM ,0,new_reg->pfb_fbpa_fbio_adr_ddll);
    }

    SEQ_REG_WR32_STALL(LOG,LW_PFB_FBPA_FBIO_ADR_DDLL, new_reg->pfb_fbpa_fbio_adr_ddll);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_disable_self_refresh")
void
gddr_disable_self_refresh
(
        void
)
{
    if (gbl_selfref_opt == LW_FALSE)
    {
// to be deprecated
        LwU32 testcmd_ext;
        LwU32 testcmd_ext_1;
        LwU32 testcmd;

	testcmd = lwr_reg->pfb_fbpa_testcmd;

        if((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
            testcmd_ext_1 = lwr_reg->pfb_fbpa_testcmd_ext_1;
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _ADR,       _DDR6_SELF_REF_EXIT,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB0,      _DDR6_SELF_REF_EXIT,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _CSB1,      _DDR6_SELF_REF_EXIT,testcmd_ext_1);
            testcmd_ext_1 = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT_1,  _USE_ADR,   _DDR6_SELF_REF_EXIT,testcmd_ext_1);
            lwr_reg->pfb_fbpa_testcmd_ext_1 = testcmd_ext_1;
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT_1,testcmd_ext_1);

            testcmd_ext = lwr_reg->pfb_fbpa_testcmd_ext;
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ACT,      _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM0,_DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _VLD_DRAM1,_DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ADR_DRAM1,_DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _DEBUG,    _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ODT,      _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _RW_CMD,   _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _MRS_CMD,  _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _DR_CMD,   _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _4CK_CMD,  _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _LAM_CMD,  _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _ACT_CMD,  _DDR6_SELF_REF_EXIT,testcmd_ext);
            testcmd_ext = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT,  _SID,      _DDR6_SELF_REF_EXIT,testcmd_ext);
            lwr_reg->pfb_fbpa_testcmd_ext = testcmd_ext;
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_TESTCMD_EXT,testcmd_ext);
        }

        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CKE,       _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS0,       _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CS1,       _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RAS,       _DDR6_SELF_REF_EXIT,testcmd);//value 1
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _CAS,       _DDR6_SELF_REF_EXIT,testcmd);//value 0
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _WE,        _DDR6_SELF_REF_EXIT,testcmd);//value 0
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _RES,       _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _BANK_4,    _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _BANK,      _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR,       _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _ADR_UPPER, _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _HOLD,      _DDR6_SELF_REF_EXIT,testcmd);
        testcmd = FLD_SET_DRF(_PFB, _FBPA_TESTCMD, _GO,        _DDR6_SELF_REF_EXIT,testcmd);

        lwr_reg->pfb_fbpa_testcmd = testcmd;
        SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD,testcmd);
    }
    else
    {
        LwU32 dram_ack_en;
        LwU32 dram_ack_dis;

        // Enable DRAM_ACK
        dram_ack_en = lwr_reg->pfb_fbpa_cfg0;
        dram_ack_en = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _ENABLED, dram_ack_en);

        // Disable DRAM_ACK
        dram_ack_dis = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACK, _DISABLED, dram_ack_en);
        lwr_reg->pfb_fbpa_cfg0 = dram_ack_dis;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw)
        {
            seq_pa_write(LW_PFB_FBPA_CFG0, dram_ack_en);
            seq_pa_write(LW_PFB_FBPA_SELF_REF, LW_PFB_FBPA_SELF_REF_CMD_DISABLED);
            seq_pa_write(LW_PFB_FBPA_CFG0, dram_ack_dis);
        }
        else
#endif
        {
            REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack_en);
            REG_WR32(LOG, LW_PFB_FBPA_SELF_REF, LW_PFB_FBPA_SELF_REF_CMD_DISABLED);
            REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack_dis);
        }
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_refresh")
void
gddr_enable_refresh
(
        void
)
{
    LwU32 fbpa_refctrl;
    fbpa_refctrl = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _VALID, _1, lwr_reg->pfb_fbpa_refctrl);
    lwr_reg->pfb_fbpa_refctrl = fbpa_refctrl;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_REFCTRL,fbpa_refctrl);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr6_pgm_wck_term")
void
gddr6_pgm_wck_term
(
        LwU32 wcktermination,
	LwU32 clamshell
)
{
    LwU32 mrs10;
    mrs10 = new_reg->pfb_fbpa_generic_mrs10;
    LwU8 g6x_skip_clamshell_dependency;
    g6x_skip_clamshell_dependency = (gTT.perfMemClkBaseEntry.spare_field8 >> 24) & 0xFF; 
      if (wcktermination) {
        if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
            ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) && (g6x_skip_clamshell_dependency == 0x1)))
        {
          mrs10 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS10, _ADR_GDDR6X_WCK_TERM, _60,mrs10); // 1
        }else {
          if (clamshell == LW_PFB_FBPA_CFG0_CLAMSHELL_X32) {
            mrs10 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS10, _ADR_GDDR6X_WCK_TERM, _60,mrs10); // 1
          } else {
            mrs10 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS10, _ADR_GDDR6X_WCK_TERM, _120,mrs10); // 2
          }
        }
      } else {
        mrs10 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS10, _ADR_GDDR6X_WCK_TERM, _DISABLED,mrs10); // 0
      }
    new_reg->pfb_fbpa_generic_mrs10 = mrs10;
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_mrs3_reg")
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
        if ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
            mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_RDQS, _ENABLED, mrs3);
        } else {
            mrs2 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS2, _ADR_GDDR6X_RDQS_MODE, _ON, mrs2);
        }
    } else {
        //disable ADR_GDDR5_RDQS
        if ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
            mrs3 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS3, _ADR_GDDR5_RDQS, _DISABLED, mrs3);
        } else {
            mrs2 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS2, _ADR_GDDR6X_RDQS_MODE, _OFF, mrs2);
        }
    }

    clamshell = lwr_reg->pfb_fbpa_cfg0;
    clamshell = REF_VAL(LW_PFB_FBPA_CFG0_CLAMSHELL,clamshell);
    //To check for vbios wck termination
    //if flag is set - ZQ_DIV2, else DISABLED

    LwU32 wck_term = gTT.perfMemClkBaseEntry.Flags1.PMC11EF1WCKTermination;
    //    FW_MBOX_WR32(1, wck_term);
    // Followup to confirm with Gautam/Abhijith/bob
    if (!((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
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
        gddr6_pgm_wck_term(wck_term,clamshell);
    }

    new_reg->pfb_fbpa_generic_mrs3 = mrs3;
    new_reg->pfb_fbpa_generic_mrs2 = mrs2;

    return;

}


GCC_ATTRIB_SECTION("memory", "gddr_update_mrs_reg")
void
gddr_update_mrs_reg
(
        void
)
{

    //reordering sequence to match pascal seq

    //Setup MRS1
    LwU8 MemoryDll = gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0MemDLL;
    LwU32 generic_mrs1 = new_reg->pfb_fbpa_generic_mrs1;
    //Enable g5 pll in cascaded mode
    //    if ((gbl_g5x_cmd_mode) && (gbl_vendor_id == MEMINFO_ENTRY_VENDORID_MICRON))
    if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
      if ((MemoryDll == 0 )&& (gbl_vendor_id == MEMINFO_ENTRY_VENDORID_MICRON))  {
          generic_mrs1 =
              FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_PLL, _ENABLED,  generic_mrs1);
      } else {
          generic_mrs1 =
              FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR5_PLL, _DISABLED, generic_mrs1);
      }
    }
    //enable MTA and PAM4 mode for g6x
    else
    {
      if (gbl_pam4_mode == LW_TRUE)
      {
        generic_mrs1 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR6X_PAM4, _PAM4, generic_mrs1);
      }
      else
      {
        generic_mrs1 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR6X_PAM4, _GDDR6, generic_mrs1);
      }

      if ((gbl_mta_en == LW_TRUE) && (gbl_pam4_mode == LW_TRUE))
      {
        generic_mrs1 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR6X_MTA, _ENABLED, generic_mrs1);
      }
      else
      {
        generic_mrs1 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS1, _ADR_GDDR6X_MTA, _DISABLED, generic_mrs1);
      }
    }
    new_reg->pfb_fbpa_generic_mrs1 = generic_mrs1;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {       // run mclk sw on PA Falcon
        //Generic MRS3
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS3,new_reg->pfb_fbpa_generic_mrs3);

        //Generic MRS1
        //changed the sequence from Pascal for G5x
        //Updating the Addr termination always at low speed clock for low speed
        //target frequency
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS1,generic_mrs1);

        //Generic MRS
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS,new_reg->pfb_fbpa_generic_mrs);

        //changed the sequence from Pascal for G6
        //Updating the Addr termination always at low speed clock for low speed
        //target frequency
        //Generic MRS8
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS8, new_reg->pfb_fbpa_generic_mrs8);

        //Generic MRS5
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS5,new_reg->pfb_fbpa_generic_mrs5);
    }
    else
#endif
    {
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS3,new_reg->pfb_fbpa_generic_mrs3);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1,generic_mrs1);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS,new_reg->pfb_fbpa_generic_mrs);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS8, new_reg->pfb_fbpa_generic_mrs8);
        REG_WR32(BAR0, LW_PFB_FBPA_GENERIC_MRS5,new_reg->pfb_fbpa_generic_mrs5);
    }

    /*
     * Program MR6
     */

    //Generic MRS6 fpr GDDR5
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
        SEQ_REG_WR32(BAR0, LW_PFB_FBPA_GENERIC_MRS6,new_reg->pfb_fbpa_generic_mrs6);
    }

    //MRS6 Receiver programming and calibration for GDDR6X
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
        // Mode for 16GB Micron parts using dfe calibration for ctle+dfe (see bug: 3262932 )
        if (isMicron16GB)
        {
            LwU8 dq_ctle_and_dfe = gTT.perfMemClkStrapEntry.Flags10.PMC11SEF1016GDqCtleDfe;
            LwU8 dqx_ctle_and_dfe = gTT.perfMemClkStrapEntry.Flags10.PMC11SEF1016GDqxCtleDfe;

            LwU32 mrs6 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS6, _BA, _MRS, 0);    // identify as MR6

            // set CTLE + DFE for DQ[7:0]
            const LwU8 pin_sub_address_ctle_dfe_dq0 =  0xb;   // 0b01011
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_SUB_ADDRESS, pin_sub_address_ctle_dfe_dq0, mrs6);
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dq_ctle_and_dfe, mrs6);

            LwU32 mrs6DqCtleDfeMidDq0 = mrs6;
            LwU32 mrs6DqCtleDfeHighDq0 = mrs6;

            LwU8 ctleDfeExtension = gTT.perfMemClkStrapEntry.Flags10.PMC11SEF1016GDfeExtension;
            if (ctleDfeExtension) {
                LwU8 dqCtleDfeMid = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqCtleDfeMid;
                LwU8 dqCtleDfeHigh = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqCtleDfeHigh;

                mrs6DqCtleDfeMidDq0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqCtleDfeMid, mrs6);
                mrs6DqCtleDfeHighDq0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqCtleDfeHigh, mrs6);
            }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
            if (!gbl_en_fb_mclk_sw)
            {   // run mclk sw on PA Falcon
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeMidDq0);
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeHighDq0);
            }
            else
#endif
            {   // run mclk sw on FB Falcon
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeMidDq0);
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeHighDq0);
            }

            // set CTLE + DEF for DQX0
            const LwU8 pin_sub_address_ctle_dfe_dqx0 =  0xc;   // 0b01100
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_SUB_ADDRESS, pin_sub_address_ctle_dfe_dqx0, mrs6);
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqx_ctle_and_dfe, mrs6);

            LwU32 mrs6DqxCtleDfeMidDq0 = mrs6;
            LwU32 mrs6DqxCtleDfeHighDq0 = mrs6;

            if (ctleDfeExtension) {
                LwU8 dqxCtleDfeMid = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqxCtleDfeMid;
                LwU8 dqxCtleDfeHigh = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqxCtleDfeHigh;

                mrs6DqxCtleDfeMidDq0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqxCtleDfeMid, mrs6);
                mrs6DqxCtleDfeHighDq0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqxCtleDfeHigh, mrs6);
            }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
            if (!gbl_en_fb_mclk_sw)
            {   // run mclk sw on PA Falcon
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeMidDq0);
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeHighDq0);
            }
            else
#endif
            {   // run mclk sw on FB Falcon
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeMidDq0);
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeHighDq0);
            }

            // set CTLE + DFE for DQ[15:8]
            const LwU8 pin_sub_address_ctle_dfe_dq1 =  0x1b;   // 0b11011
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_SUB_ADDRESS, pin_sub_address_ctle_dfe_dq1, mrs6);
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dq_ctle_and_dfe, mrs6);

            LwU32 mrs6DqCtleDfeMidDq1 = mrs6;
            LwU32 mrs6DqCtleDfeHighDq1 = mrs6;

            if (ctleDfeExtension) {
                LwU8 dqCtleDfeMid = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqCtleDfeMid;
                LwU8 dqCtleDfeHigh = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqCtleDfeHigh;

                mrs6DqCtleDfeMidDq1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqCtleDfeMid, mrs6);
                mrs6DqCtleDfeHighDq1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqCtleDfeHigh, mrs6);
            }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
            if (!gbl_en_fb_mclk_sw)
            {   // run mclk sw on PA Falcon
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeMidDq1);
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeHighDq1);
            }
            else
#endif
            {   // run mclk sw on FB Falcon
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeMidDq1);
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqCtleDfeHighDq1);
            }

            // set CTLE + DFE for DQX1
            const LwU8 pin_sub_address_ctle_dfe_dq1x =  0x1c;   // 0b11100
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_SUB_ADDRESS, pin_sub_address_ctle_dfe_dq1x, mrs6);
            mrs6 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqx_ctle_and_dfe, mrs6);

            LwU32 mrs6DqxCtleDfeMidDq1 = mrs6;
            LwU32 mrs6DqxCtleDfeHighDq1 = mrs6;

            if (ctleDfeExtension) {
                LwU8 dqxCtleDfeMid = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqxCtleDfeMid;
                LwU8 dqxCtleDfeHigh = gTT.perfMemClkStrapEntry.Flags11.PMC11SEF1116GDqxCtleDfeHigh;

                mrs6DqxCtleDfeMidDq1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqxCtleDfeMid, mrs6);
                mrs6DqxCtleDfeHighDq1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS6, _ADR_GDDR6X_PIN_LEVEL, dqxCtleDfeHigh, mrs6);
            }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
            if (!gbl_en_fb_mclk_sw)
            {   // run mclk sw on PA Falcon
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeMidDq1);
                seq_pa_write(LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeHighDq1);
            }
            else
#endif
            {   // run mclk sw on FB Falcon
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6);    // writing VREFDL, VREFDM and VREFDU back to back
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeMidDq1);
                REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS6, mrs6DqxCtleDfeHighDq1);
            }

            // Trigger Calibration
            LwU32 mrs5 = new_reg->pfb_fbpa_generic_mrs5;
            if (((dqx_ctle_and_dfe & 0xf) != 0) || ((dq_ctle_and_dfe & 0xf) != 0))
            {
                // calibration start: 0b01
                mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR6X_DFE_CALIBRATION, 0x1, mrs5);
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)
                {   // run mclk sw on PA Falcon
                    seq_pa_write(LW_PFB_FBPA_GENERIC_MRS5, mrs5);
                }
                else
#endif
                {   // run mclk sw on FB Falcon
                    REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS5, mrs5);    // writing VREFDL, VREFDM and VREFDU back to back
                }

                // wait tDFECAL
                SEQ_WAIT_US(20);
            }
            else
            {
                // calibration stop: 0b00
                mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR6X_DFE_CALIBRATION, 0x0, mrs5);
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)
                {   // run mclk sw on PA Falcon
                    seq_pa_write(LW_PFB_FBPA_GENERIC_MRS5, mrs5);
                }
                else
#endif
                {   // run mclk sw on FB Falcon
                    REG_WR32(CSB, LW_PFB_FBPA_GENERIC_MRS5, mrs5);
                }
            }
            // save the value for further access to mrs5, e.g when updating lp3
            new_reg->pfb_fbpa_generic_mrs5 = mrs5;
        }  //  if (isMicron16GB)
    }

    /*
     *  Program MR7
     */

    LwU8 half_vrefd = gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1MemVREFD;
    LwU32 generic_mrs7 = new_reg->pfb_fbpa_generic_mrs7;
    if (half_vrefd == 0) {
        generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_HALF_VREFD, _70,generic_mrs7);
    } else {
        generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_HALF_VREFD, _50,generic_mrs7);
    }

    LwU8 InternalVrefc;
    InternalVrefc = gTT.perfMemClkStrapEntry.Flags5.PMC11SEF5G5XVref;
    //Vrefc enabled - 50%
    // Followup to confirm with Gautam/Abhijith/bob
    if ((InternalVrefc == 1) &&
        ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)||
         (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
         (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X))) {
        generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR6X_HALF_VREFC, _ON,generic_mrs7);
    }

    //asatish fixing for wck pin mode for p5
    //changing MRS7[0] to 1 for non drampll path
    //changing for ga10x - MRS7 pin mode to be always enabled bug 2549883
    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
        if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
            generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7,  _ADR_GDDR6X_WCK_AP,_PIN,generic_mrs7);
        } else {
            generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7,  _ADR_GDDR6X_WCK_AP,_PD,generic_mrs7);
        }
    }

    //asatish adding enabling of low freq mode at target freq - change from Turing.
     if (gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0LowFreq == 1 ){
        if (!((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)||
              (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))) {
            generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR5_LOW_FREQ_MODE,_ENABLED,generic_mrs7);
        } else {
            generic_mrs7 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS7, _ADR_GDDR6X_LOW_FREQ_MODE,_ENABLED,generic_mrs7);
        }
     }

    new_reg->pfb_fbpa_generic_mrs7 = generic_mrs7;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {       // run mclk sw on PA Falcon
        //Generic MRS7
        seq_pa_write_stall(LW_PFB_FBPA_GENERIC_MRS7,generic_mrs7);

        SEQ_WAIT_NS(500);

        //Following MRS registers are not written in PASCAL,
        //Generic MRS2 - Need for EDCRATE and Bank grouping
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS2,new_reg->pfb_fbpa_generic_mrs2);

        //Generic MRS4 - Addition
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS4,new_reg->pfb_fbpa_generic_mrs4);

    }
    else
#endif
    {
        REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS7,generic_mrs7);
        SEQ_WAIT_NS(500);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS2,new_reg->pfb_fbpa_generic_mrs2);
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4,new_reg->pfb_fbpa_generic_mrs4);
    }

    // set MRS9 for tx_eq
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {

        const LwU8 BYTE0_OPCODE = 0xF;
        const LwU8 BYTE1_OPCODE = 0x1F;

        // r-m-w on mrs9 don't work well due to the sub selection through the pin address
        // the only thing we get back is the mrs ba field
        // mrs9 = REG_RD32(BAR0, LW_PFB_FBPA_GENERIC_MRS9);
        LwU32 mrs9 = FLD_SET_DRF(_PFB, _FBPA_GENERIC_MRS9, _BA, _MRS, 0);

        if (isMicron16GB)
        {
            // TODO: define should be added to manuals but brining this to lwgpu, ga10x, ad10x and gh100 branch
            //       will nedd some effort & time (stefans - work in progress)
            #define LW_PFB_FBPA_GENERIC_MRS9_ADR_GDDR6X_TX_PREEMPH  1:0

            LwU8 tx_preemph;
            tx_preemph = gTT.perfMemClkStrapEntry.Flags10.PMC11SEF1016GPreEmphasis;
            mrs9 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_TX_PREEMPH, tx_preemph, mrs9);
        }
        else
        {
            LwU8 dram_tx_eq;
            dram_tx_eq = gTT.perfMemClkStrapEntry.Flags10.PMC11SEF10DramTxEq;
            mrs9 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9, _ADR_GDDR6X_TXEQ, dram_tx_eq, mrs9);
        }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {       // run mclk sw on PA Falcon
        // byte0
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS9, FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9,
            _ADR_GDDR6X_PIN_SUB_ADDRESS, BYTE0_OPCODE, mrs9)
        );

        // byte1
        seq_pa_write(LW_PFB_FBPA_GENERIC_MRS9, FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9,
            _ADR_GDDR6X_PIN_SUB_ADDRESS, BYTE1_OPCODE, mrs9)
        );
    }
    else
#endif
    {
        // byte0
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9, FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9,
            _ADR_GDDR6X_PIN_SUB_ADDRESS, BYTE0_OPCODE, mrs9)
        );

        // byte1
        REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS9, FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS9,
            _ADR_GDDR6X_PIN_SUB_ADDRESS, BYTE1_OPCODE, mrs9)
        );
    }
    }

    if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
        (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
        //Generic MRS10
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS10, new_reg->pfb_fbpa_generic_mrs10);
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_set_trng_ctrl_reg")
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
//TODO: add ASR_FAST_EXIT from VBIOS, not needed yet
//Flags10.PMC11SEF10AsrExitTraining
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

    //TODO: Check if we should remove this if statement
    if (((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) && (!gbl_g5x_cmd_mode)) {
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB,_FBPA_TRAINING_CTRL, _ASR_FAST_EXIT, _DISABLED,fbpa_trng_ctrl);
    }

    //FOLLOWUP: revisit
    if ((gbl_g5x_cmd_mode && ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))) || ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (EdcRate == LW_PFB_FBPA_CFG0_EDC_RATE_HALF))) {
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
        fbpa_trng_ctrl = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL, _WCK_STOP_CLOCKS, _ENABLED,fbpa_trng_ctrl);
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL,fbpa_trng_ctrl);

    LwBool wr_pat_shift=gTT.perfMemClkStrapEntry.Flags8.PMC11SEF7WPatShft;
    LwBool wr_pat_ilw=gTT.perfMemClkStrapEntry.Flags8.PMC11SEF7WPatIlw;
    //read shares same signal for pattern shift and ilw
    LwBool rd_pat_shift_ilw=gTT.perfMemClkStrapEntry.Flags7.PMC11SEF7RPatShftIlw;
    lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB,_FBPA_TRAINING_CTRL2,_SHIFT_PATT_WR, wr_pat_shift,lwr_reg->pfb_fbpa_training_ctrl2);
    lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB,_FBPA_TRAINING_CTRL2,_SHIFT_PATT_RD, rd_pat_shift_ilw,lwr_reg->pfb_fbpa_training_ctrl2);
    lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB,_FBPA_TRAINING_CTRL2,_ILW_PATT_WR, wr_pat_ilw,lwr_reg->pfb_fbpa_training_ctrl2);
    lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB,_FBPA_TRAINING_CTRL2,_ILW_PATT_RD, rd_pat_shift_ilw, lwr_reg->pfb_fbpa_training_ctrl2);

    lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_CTRL2,_DCC, gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6DCC, lwr_reg->pfb_fbpa_training_ctrl2);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2, lwr_reg->pfb_fbpa_training_ctrl2);

    return;

}


GCC_ATTRIB_SECTION("memory", "gddr_load_brl_shift_val")
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
        // ob_brlshift = gTT.perfMemClkStrapEntry.WriteBrlShft;
        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
            ob_brlshift = 0x0d0d0d0d;
        } else {
            ob_brlshift = 0x08080808; //force values
        }
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw) {
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_OB_BRLSHFT3,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_OB_BRLSHFT3,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_OB_BRLSHFT3,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_OB_BRLSHFT3,ob_brlshift);

            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_OB_BRLSHFT3,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_OB_BRLSHFT3,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_OB_BRLSHFT3,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_OB_BRLSHFT1,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_OB_BRLSHFT2,ob_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_OB_BRLSHFT3,ob_brlshift);
        }
        else
#endif
        {
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
        }
        //wait for 1us
        SEQ_WAIT_US(1);
    }

    if (!rd_tr) {
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw) {
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_IB_BRLSHFT3,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE1_IB_BRLSHFT3,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE2_IB_BRLSHFT3,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE3_IB_BRLSHFT3,ib_brlshift);

            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE0_IB_BRLSHFT3,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE1_IB_BRLSHFT3,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE2_IB_BRLSHFT3,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_IB_BRLSHFT1,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_IB_BRLSHFT2,ib_brlshift);
            seq_pa_write(LW_PFB_FBPA_FBIOTRNG_SUBP1BYTE3_IB_BRLSHFT3,ib_brlshift);
        }
        else
#endif
        {
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
        }
        //wait for 1us
        SEQ_WAIT_US(1);
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_wait_trng_complete")
void
gddr_wait_trng_complete
(
        LwU32 wck,
        LwU32 rd_wr
)
{
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw)
    {
        SEQ_REG_TRNCHK(LW_PFB_FBPA_TRAINING_STATUS);
    }
    else
#endif
    {
        LwU32 fbpa_trng_status;
        LwU32 subp0_trng_status;
        LwU32 subp1_trng_status;

        //FOLLOWUP Add timeout
        do {
            fbpa_trng_status = REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_STATUS);
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
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_initialize_tb_trng_mon")
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

GCC_ATTRIB_SECTION("memory", "gddr_edc_tracking")
void
gddr_edc_tracking
(
        void
)
{


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
            if ((((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))
              && ((gblEnEdcTracking) || (gblElwrefTracking))))
            {
                gddr_setup_edc_tracking(1,&my_EdcTrackingGainTable);
            }
             //Additional step for setting up edc tracking
             //FOLLOWUP Add vbios flag
             if ((((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 ) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))
	          && ((gblEnEdcTracking) || (gblElwrefTracking)))) {
                 func_fbio_intrpltr_offsets( REF_VAL(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_IB, lwr_reg->pfb_fbpa_fbio_intrpltr_offset)+ 32,
				              REF_VAL(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_OB, lwr_reg->pfb_fbpa_fbio_intrpltr_offset) + 32);
		 GblFbioIntrpltrOffsetInc = LW_TRUE;
             }

            //start edc tracking
            //           FW_MBOX_WR32(4, 0x000EDC33);
            // Tracking current value of VREF_CODE3_PAD9 just before EDC tracking is enabled.
            // This is done at this point as priv_src is applied, read happens only when EDC tracking is enabled
            // and to avoid N priv reads.
            // not needed anymore since read in load_reg_values
            //            new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);

            gddr_start_edc_tracking(1,gblEnEdcTracking,gblElwrefTracking,&my_EdcTrackingGainTable,startFbStopTimeNs);

// add this back in after fix for vref error check
            if ((gblElwrefTracking) && (GblVrefTrackPerformedOnce == 0)) {
                if (DRF_VAL(_PFB,_FBPA_SW_CONFIG,_EDC_TR_VREF_SAVE_AND_RESTORE_OPT,lwr_reg->pfb_fbpa_sw_config) == LW_PFB_FBPA_SW_CONFIG_EDC_TR_VREF_SAVE_AND_RESTORE_OPT_INIT)      
		        {
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                  if (!gbl_en_fb_mclk_sw) {
                    // save current fbio_vref_tracking for FN call later
                    new_reg->pfb_fbpa_fbio_vref_tracking = lwr_reg->pfb_fbpa_fbio_vref_tracking;
                    fb_pa_sync(54, FN_SAVE_VREF_VALUES);
                  }
		          else 
#endif
		          {
                     func_save_vref_values();                                                                                                                              
		          }
		        }
		        else
                {
                   lwr_reg->pfb_fbpa_fbio_vref_tracking2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING2, _SHADOW_LOAD, 0x1, lwr_reg->pfb_fbpa_fbio_vref_tracking2);
                   SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING2,  lwr_reg->pfb_fbpa_fbio_vref_tracking2);
                }
            }

            //EDC_TR step 10: write vref tracking disable training updates to 1 before rd/wr training
            lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _DISABLE_TRAINING_UPDATES, 1,lwr_reg->pfb_fbpa_fbio_vref_tracking);

            LwU8 VbiosEdcVrefVbiosCtrl = gTT.perfMemClkBaseEntry.Param0.PMC11EP0EdcVrefVbiosCtrl;
            LwU8 VrefTrackingAlwaysOn = gTT.perfMemClkBaseEntry.spare_field4 & 0x4;
            if ((((VbiosEdcVrefVbiosCtrl) && (VrefTrackingAlwaysOn))) || 
                (DRF_VAL(_PFB,_FBPA_SW_CONFIG,_EDC_TR_VREF_SAVE_AND_RESTORE_OPT,lwr_reg->pfb_fbpa_sw_config) != LW_PFB_FBPA_SW_CONFIG_EDC_TR_VREF_SAVE_AND_RESTORE_OPT_INIT)) 
	        {
                lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _EN, 0,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            }
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,lwr_reg->pfb_fbpa_fbio_vref_tracking);

            if ((DRF_VAL(_PFB,_FBPA_SW_CONFIG,_EDC_TR_VREF_SAVE_AND_RESTORE_OPT,lwr_reg->pfb_fbpa_sw_config) != LW_PFB_FBPA_SW_CONFIG_EDC_TR_VREF_SAVE_AND_RESTORE_OPT_INIT) &&
		(gblElwrefTracking)) 
	    {

              lwr_reg->pfb_fbpa_fbio_vref_tracking2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING2, _SHADOW_SELECT, 0x1, lwr_reg->pfb_fbpa_fbio_vref_tracking2);
              lwr_reg->pfb_fbpa_fbio_vref_tracking2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING2, _SHADOW_LOAD, 0x0, lwr_reg->pfb_fbpa_fbio_vref_tracking2);
              SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING2,  lwr_reg->pfb_fbpa_fbio_vref_tracking2);
              lwr_reg->pfb_fbpa_fbio_vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _EN, 1,lwr_reg->pfb_fbpa_fbio_vref_tracking);
              SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            }
            if ((gblElwrefTracking) && (!GblVrefTrackPerformedOnce)) {
                enableEvent(EVENT_SAVE_TRAINING_DATA);
                GblVrefTrackPerformedOnce = LW_TRUE;
            }
}

GCC_ATTRIB_SECTION("memory", "gddr_mem_link_training")
void
gddr_mem_link_training
(
        void
)
{
    LwU32 fbpa_trng_cmd;
    LwU32 rd_tr_en;
    LwU32 wr_tr_en;

    //Need to revert the interpltr offsets 
//    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
//      if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) 
//      {
//        func_fbio_intrpltr_offsets(30,35);
//      }
//      else
//      {
//        func_fbio_intrpltr_offsets(6,29);
//      }
//    }

    gddr_set_trng_ctrl_reg();

    //Removing the 32 increase of offset if it was added in edc tracking
    if (GblFbioIntrpltrOffsetInc == LW_TRUE)
    {
       func_fbio_intrpltr_offsets( REF_VAL(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_IB, lwr_reg->pfb_fbpa_fbio_intrpltr_offset)- 32,
      		              REF_VAL(LW_PFB_FBPA_FBIO_INTRPLTR_OFFSET_OB, lwr_reg->pfb_fbpa_fbio_intrpltr_offset) - 32);
       GblFbioIntrpltrOffsetInc = LW_FALSE;
    }

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
        // this line below is not needed right?, since it gets overwritten later -MW
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _INIT,      fbpa_trng_cmd);

        //default turn on wck training
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _ENABLED,fbpa_trng_cmd);

        //Performing wck training before rd wr to ensure edc tracking is done in between
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);

        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        //wait for wck training to complete
        gddr_wait_trng_complete(1,0);

        //FB UNIT sim only
        //Hinting tb training monitor to check wck training values
        SEQ_REG_WR32 (LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05452493);
	
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

        if (((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))
	      && ((gblEnEdcTracking) || (gblElwrefTracking))) {
          gddr_edc_tracking();
        }

        //disabling wck training
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _DISABLED,fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _INIT,      fbpa_trng_cmd);

        //FOLLOWUP add WAR for Hynix memory for dcc handling - Hynix used for legacy support

        if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
        {
          //default Enable RD/WR training
          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD, _ENABLED, fbpa_trng_cmd);
          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED, fbpa_trng_cmd);

          //start wck/rd/wr training for each subp

          lwr_reg->pfb_fbpa_training_cmd1 = fbpa_trng_cmd;
          lwr_reg->pfb_fbpa_training_cmd0 = fbpa_trng_cmd;
          gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

          ////FB UNIT LEVEL only
          ////Hinting tb training monitor to initialize training and load patrams
          //gddr_initialize_tb_trng_mon();
          SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0x54524753);

          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
          gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
          rd_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_RD,fbpa_trng_cmd);
          wr_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_WR,fbpa_trng_cmd);
        }
        else //gddr6x - splitting the read/write training to have different ldff set for wr tr
        {
          LwU8 rd_ldff_cnt = 0xF;       // Read LDFF is hardcoded
          lwr_reg->pfb_fbpa_training_rw_ldff_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_LDFF_CTRL, _CMD_CNT, rd_ldff_cnt, lwr_reg->pfb_fbpa_training_rw_ldff_ctrl);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_LDFF_CTRL,lwr_reg->pfb_fbpa_training_rw_ldff_ctrl);

          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD, _ENABLED, fbpa_trng_cmd);
          lwr_reg->pfb_fbpa_training_cmd1 = fbpa_trng_cmd;
          lwr_reg->pfb_fbpa_training_cmd0 = fbpa_trng_cmd;
          gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
          rd_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_RD,fbpa_trng_cmd);

          ////FB UNIT LEVEL only
          ////Hinting tb training monitor to initialize training and load patrams
          //gddr_initialize_tb_trng_mon();
          SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0x54524753);

          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
          gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

          //wait for wck/rd/wr training complete
          gddr_wait_trng_complete(0,1);
          SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0xFBFBD001);

          LwU8 wr_ldff_cnt=gTT.perfMemClkStrapEntry.Flags7.PMC11SEF7SwtchCmdCnt;
          lwr_reg->pfb_fbpa_training_rw_ldff_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_LDFF_CTRL, _CMD_CNT, wr_ldff_cnt, lwr_reg->pfb_fbpa_training_rw_ldff_ctrl);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_LDFF_CTRL,lwr_reg->pfb_fbpa_training_rw_ldff_ctrl);

          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD, _DISABLED, fbpa_trng_cmd);
          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO, _INIT,      fbpa_trng_cmd);
          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED, fbpa_trng_cmd);
          lwr_reg->pfb_fbpa_training_cmd1 = fbpa_trng_cmd;
          lwr_reg->pfb_fbpa_training_cmd0 = fbpa_trng_cmd;
          gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
          SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0x54524753);

          fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
          gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
          SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
          wr_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_WR,fbpa_trng_cmd);
        }
        //wait for wck/rd/wr training complete
        gddr_wait_trng_complete(0,1);
        SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0xFBFBD001);

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
        // this line below is not needed right?, since it gets overwritten later -MW
        fbpa_trng_cmd = FLD_SET_DRF( _PFB, _FBPA_TRAINING_CMD, _GO,       _INIT,       fbpa_trng_cmd);

        if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
            //to ensure wck training for gddr6 is done as trimmer mode for refmpll path
            //must be reverted back to ddll mode for write training
            //changing for ga10x - pin mode to be always enabled bug 2549883  - NO hack for wck training
            //          if ((GblEnWckPinModeTr) && (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 )) {
            //            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _TRIMMER,fbpa_trng_cmd);
            //          } else {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _DLL,fbpa_trng_cmd);
            //          }
        } else {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _TRIMMER,fbpa_trng_cmd);
        }

        //by default enabling wck training only
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _ENABLED,fbpa_trng_cmd);

        //Performing wck training before rd wr to ensure edc tracking is done in between
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        //wait for wck training to complete
        gddr_wait_trng_complete(1,0);

        //disabling wck training
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WCK, _DISABLED,fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,       _INIT,        fbpa_trng_cmd);

        //FB UNIT sim only
        //Hinting tb training monitor to check wck training values
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x05452493);

        //reverting back to ddll mode for p5 wr training in g6
        //changing for ga10x - MRS7 pin mode to be always enabled bug 2549883
        //       if ((GblEnWckPinModeTr == LW_TRUE) && (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) &&
        //            (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 )) {
        //            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _CONTROL, _DLL,fbpa_trng_cmd);
        //        }

        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) 
	    {
                LwU8 wr_ldff_cnt=gTT.perfMemClkStrapEntry.Flags7.PMC11SEF7SwtchCmdCnt;
	        lwr_reg->pfb_fbpa_training_rw_ldff_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_LDFF_CTRL, _CMD_CNT, wr_ldff_cnt, lwr_reg->pfb_fbpa_training_rw_ldff_ctrl);
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_LDFF_CTRL,lwr_reg->pfb_fbpa_training_rw_ldff_ctrl);
        }

        //default Enable RD/WR training
        //disabling read training fbpa_trng_cmd = FLD_SET_DRF(_PFB _FBPA_TRAINING_CMD, _RD, _ENABLED,fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR, _ENABLED,fbpa_trng_cmd);

        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        // save before GO since we don't want the go bit set in the regbox val
        lwr_reg->pfb_fbpa_training_cmd1 = fbpa_trng_cmd;
        lwr_reg->pfb_fbpa_training_cmd0 = fbpa_trng_cmd;

        //FB UNIT LEVEL only
        //Hinting tb training monitor to initialize training
        //gddr_initialize_tb_trng_mon();
        SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0x54524753);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _GO,      _NOW,      fbpa_trng_cmd);
        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);

        rd_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_RD,fbpa_trng_cmd);
        wr_tr_en = REF_VAL(LW_PFB_FBPA_TRAINING_CMD_WR,fbpa_trng_cmd);

        //wait for completion of training update.
        gddr_wait_trng_complete(0,1);

        ////FB UNIT LEVEL only
        //Hinting training monitor to initiate checks
        SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR, 0xFBFBD001);

        //load vbios values for brl shifters if RD/WR training is disabled
        //gddr_load_brl_shift_val(rd_tr_en, wr_tr_en);

        // !!fbpa_trng_cmd not written back to lwr_reg on purpose (AS)
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_edc_replay")
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
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

            generic_mrs4 = new_reg->pfb_fbpa_generic_mrs4;
            //Addition avinash
            generic_mrs4 = FLD_SET_DRF(_PFB,_FBPA_GENERIC_MRS4,_ADR_GDDR5_RDCRC,_ENABLED,generic_mrs4);
            generic_mrs4 = FLD_SET_DRF(_PFB,_FBPA_GENERIC_MRS4,_ADR_GDDR5_WRCRC,_ENABLED,generic_mrs4);

            SEQ_REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4,generic_mrs4);
            //Ensure MRS command is sent to Dram
            gddr_flush_mrs_reg();
        }
    } else {
        fbpa_cfg8 = lwr_reg->pfb_fbpa_fbio_cfg8;
        fbpa_cfg8 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG8, _RFU, _INTERP_EDC_PWRD_DISABLE,fbpa_cfg8);
        lwr_reg->pfb_fbpa_fbio_cfg8 = fbpa_cfg8;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG8,fbpa_cfg8);
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_gddr5_lp3")
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

    lp3_mode = REF_VAL(12:12, meminfo3116);
    mrs5 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS5, _ADR_GDDR5_LP3, lp3_mode,mrs5);

    SEQ_REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS5,mrs5);
    return;

}


GCC_ATTRIB_SECTION("memory", "gddr_reset_read_fifo")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_periodic_trng")
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
    }

    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
    {
        LwU8 periodic_cmd_cnt= gTT.perfMemClkStrapEntry.Flags8.PMC11SEF7PrTrCmdCnt; 
        lwr_reg->pfb_fbpa_training_rw_periodic_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_RW_PERIODIC_CTRL,_CMD_CNT,periodic_cmd_cnt, lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);
    }
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_RW_PERIODIC_CTRL,lwr_reg->pfb_fbpa_training_rw_periodic_ctrl);

    //enabling periodic shift training
    LwU8 PeriodicShftAvg = gTT.perfMemClkStrapEntry.Flags6.PMC11SEF6PerShiftTrng;
    if (PeriodicShftAvg) {
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_WR, _EN, lwr_reg->pfb_fbpa_training_ctrl2);
        lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CTRL2, _SHIFT_PATT_RD, _EN, lwr_reg->pfb_fbpa_training_ctrl2);
    }
    lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB,_FBPA_TRAINING_CTRL2,_ILW_PATT_WR, gTT.perfMemClkStrapEntry.Flags8.PMC11SEF7PrIlw ,lwr_reg->pfb_fbpa_training_ctrl2);
    lwr_reg->pfb_fbpa_training_ctrl2 = FLD_SET_DRF_NUM(_PFB,_FBPA_TRAINING_CTRL2,_ILW_PATT_RD, gTT.perfMemClkStrapEntry.Flags8.PMC11SEF7PrIlw,lwr_reg->pfb_fbpa_training_ctrl2);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CTRL2,lwr_reg->pfb_fbpa_training_ctrl2);

    LwU8 periodicTrng = gTT.perfMemClkStrapEntry.Flags3.PMC11SEF3PerTrn;

    if ((periodicTrng) && (
        (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) ||
        (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)))
    {
        LwU32 fbpa_trng_cmd;

        fbpa_trng_cmd = lwr_reg->pfb_fbpa_training_cmd1;
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _PERIODIC, _ENABLED,fbpa_trng_cmd);
        fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _WR,       _ENABLED,fbpa_trng_cmd);
        if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD,_ENABLED,fbpa_trng_cmd);
        } else {
            fbpa_trng_cmd = FLD_SET_DRF(_PFB, _FBPA_TRAINING_CMD, _RD,_DISABLED,fbpa_trng_cmd);
	    }

        lwr_reg->pfb_fbpa_training_cmd0 = fbpa_trng_cmd;
        lwr_reg->pfb_fbpa_training_cmd1 = fbpa_trng_cmd;
        gddr_pgm_trng_lower_subp(fbpa_trng_cmd);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_TRAINING_CMD1,fbpa_trng_cmd);
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_start_fb")
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
    fbpa_cfg0 = REG_RD32(BAR0, LW_PFB_FBPA_CFG0);
    fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACPD, gbl_dram_acpd,fbpa_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_set_rpre_r2w_bus")
void
gddr_set_rpre_r2w_bus
(
        void
)
{
    LwU32 fbpa_config2;

    fbpa_config2 = new_reg->pfb_fbpa_config2;
    fbpa_config2 = FLD_SET_DRF(_PFB, _FBPA_CONFIG2, _RPRE,_1,fbpa_config2);
    new_reg->pfb_fbpa_config2 = fbpa_config2;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG2,fbpa_config2);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_disable_low_pwr_en_edc_early")
void
gddr_disable_low_pwr_en_edc_early
(
        void
)
{
    LwU32 fbpa_cfg0;

    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _LOW_POWER, _1,fbpa_cfg0);
    } else {
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _LOW_POWER, _0,fbpa_cfg0);
    }
    fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _EDC_EARLY_VALUE,_1,fbpa_cfg0);
    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_brl_shifter")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG10,fbpa_fbio_cfg10);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_disable_low_pwr_edc")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);
    return;
}

GCC_ATTRIB_SECTION("memory", "gddr_disable_crc_mrs4")
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
    // SEQ_REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS4,generic_mrs4);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_power_down_edc_intrpl")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG8,fbpa_fbio_cfg8);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_clr_intrpl_padmacro_quse")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbpa_fbio_cfg1);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_clr_ib_padmacro_ptr")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_disable_drampll")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_CFG,drampll_cfg);
    return;

}


GCC_ATTRIB_SECTION("memory", "gddr_disable_refmpll")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_REFMPLL_CFG,refmpll_cfg);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_cmos_cml_clk")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CMOS_CLK,fbio_cmos_clk);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_cfg_padmacro_sync_ptr_offset")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG1,fbio_cfg1);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_enable_edc_vbios")
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
    VbiosEdc = REF_VAL(14:14, meminfo5540);
    VbiosEdcReplay = REF_VAL(15:15, meminfo5540);
    if (gTT.perfMemClkBaseEntry.spare_field4 & 0x1) return;

    if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5 ) ||
            (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6 ) ||
            (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X ) ||
            ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X ) && (gbl_g5x_cmd_mode))) {
        //FOLLOWUP MIEGddr5Edc taken for both read edc and write edc
        fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _RDEDC,VbiosEdc,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _WREDC,VbiosEdc,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _REPLAY,VbiosEdcReplay,fbpa_cfg0);
        lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

        EdcRate = REF_VAL(LW_PFB_FBPA_CFG0_EDC_RATE,fbpa_cfg0);

        if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||
            (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)){
            new_reg->pfb_fbpa_generic_mrs2 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS2, _ADR_GDDR6X_CRC_WRTR,!EdcRate,new_reg->pfb_fbpa_generic_mrs2);
        }
        if (VbiosEdc) {
            //As the dram is in self ref, the MRS4 reg will be written at step 48
            new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_RDCRC,0,new_reg->pfb_fbpa_generic_mrs4);
            new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_WRCRC,0,new_reg->pfb_fbpa_generic_mrs4);
        }
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_power_up_wdqs_intrpl")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG8,fbio_cfg8);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_drampll_cfg_and_lock_wait")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_CFG,bcast_drampll_cfg);

    SEQ_WAIT_NS(50);

    //configure the pll co-efficients
    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _MDIV,drampll_m_val,bcast_drampll_coeff);
    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _NDIV,drampll_n_val,bcast_drampll_coeff);
    //    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _PLDIV,drampll_p_val,bcast_drampll_coeff);
    bcast_drampll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAMPLL_COEFF, _SEL_DIVBY2,drampll_divby2,bcast_drampll_coeff);
    lwr_reg->pfb_fbpa_drampll_coeff = bcast_drampll_coeff;
//    SEQ_REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_COEFF,bcast_drampll_coeff);
    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_DRAMPLL_COEFF,bcast_drampll_coeff);

    SEQ_WAIT_NS(50);

    //Enable drampll
    bcast_drampll_cfg = FLD_SET_DRF(_PFB, _FBPA_DRAMPLL_CFG, _ENABLE, _YES,bcast_drampll_cfg);
    lwr_reg->pfb_fbpa_drampll_cfg = bcast_drampll_cfg;

    // wait for pll lock
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw)
    {
        seq_pa_write(LW_PFB_FBPA_DRAMPLL_CFG, bcast_drampll_cfg);
        SEQ_REG_POLL32(LW_PFB_FBPA_DRAMPLL_CFG, DRF_DEF(_PFB,_FBPA_DRAMPLL_CFG,_PLL_LOCK,_TRUE),
                DRF_SHIFTMASK(LW_PFB_FBPA_DRAMPLL_CFG_PLL_LOCK));
    }
    else
#endif
    {
        LwBool notLocked = LW_TRUE;
        LwU32 timeout = 0;
        LwU32 refmpllCfg;

        REG_WR32(LOG, LW_PFB_FBPA_DRAMPLL_CFG, bcast_drampll_cfg);
        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
        do
        {
            refmpllCfg = REG_RD32(BAR0, LW_PFB_FBPA_DRAMPLL_CFG);
            if (REF_VAL(LW_PFB_FBPA_DRAMPLL_CFG_PLL_LOCK,refmpllCfg) == LW_PFB_FBPA_DRAMPLL_CFG_PLL_LOCK_TRUE)
            {
                notLocked = LW_FALSE;
            }
            timeout++;
        }
        while (notLocked && (timeout < 10000));

        if (timeout >= 10000)
        {
            FW_MBOX_WR32(6, getDisabledPartitionMask());
            FW_MBOX_WR32(7, getPartitionMask());
            FW_MBOX_WR32(11, gNumberOfPartitions);
            FW_MBOX_WR32(12, refmpllCfg);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DRAMPLL_NO_LOCK);
        }
        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_clear_rpre")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG2,fbpa_config2);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_fbio_cfg_pwrd")
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
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCK_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _WCKB_DIGDLL_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_DIFFAMP_PWRD,_ENABLE,fbio_cfg_pwrd);
        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _RDQS_RX_GDDR5_PWRD, _DISABLE,fbio_cfg_pwrd);
        //Confirm with Ish - needed for P0?
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);

        gddr_pgm_dq_term();

        fbio_cfg_pwrd = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG_PWRD, _DQ_RX_GDDR5_PWRD, _DISABLE,fbio_cfg_pwrd);
        lwr_reg->pfb_fbpa_fbio_cfg_pwrd = fbio_cfg_pwrd;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD,fbio_cfg_pwrd);
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_bypass_drampll")
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
        if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
        {
            drampll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _LD_PULSE_DIV4,_DISABLE,drampll_config);
        }
    } else {
        drampll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _BYPASS,_DISABLE,drampll_config);
        if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) &&  (gbl_pam4_mode == LW_TRUE))
        {
            drampll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _LD_PULSE_DIV4,_ENABLE,drampll_config);
        }
    }
    lwr_reg->pfb_fbpa_fbio_drampll_config = drampll_config;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);
//    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_bypass_refmpll")
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

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw) {
        fb_pa_sync((bypass_en & 0x1), FN_GDDR_PA_SYNC);
//        seq_pa_write(LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
        seq_pa_write_stall(LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
        fb_pa_sync(10, FN_GDDR_PA_SYNC);
    }
    else
#endif
    {
        REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_en_sync_mode_refmpll")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_clk_switch_drampll_2_onesrc")
void
gddr_clk_switch_drampll_2_onesrc
(
        void
)
{

    // fbio_mode_switch = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMPLL_STOP_SYNCMUX,1,fbio_mode_switch);
    //clamp the mode switch
    //unclamp onesrc
    //fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _ONESOURCE_STOP,_DISABLE,1);
    //switch to onesrc

    //Setting FBIO_SPARE[12] to disable tx_e_serializer
    //Ga10x -change needed for Reliablity concerns
    lwr_reg->pfb_fbpa_fbio_spare = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE_VALUE, _SERIALIZER, _DISABLE, lwr_reg->pfb_fbpa_fbio_spare);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);

    GblSkipDrampllClamp = (gTT.perfMemClkBaseEntry.spare_field5 & 0x1);

    if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
        LwU32 drampll_config;
        drampll_config = lwr_reg->pfb_fbpa_fbio_drampll_config;
        if (GblSkipDrampllClamp == LW_FALSE) {
          drampll_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,0x1,drampll_config);
          lwr_reg->pfb_fbpa_fbio_drampll_config = drampll_config;
          SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);  // enable clamp
        }

        fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _REFMPLL, fbio_mode_switch);
        lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
        SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV,fbio_mode_switch);   // switch to refmpll
    } else {
        if (GblSkipDrampllClamp == LW_FALSE) {
          fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_ENABLE,fbio_mode_switch);
          lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
          REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
        }
        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL,fbio_mode_switch);
        lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
        REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
    }

    SEQ_WAIT_NS(100);

    //bypass drampll
    gddr_bypass_drampll(1);
    FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x1);  // only exelwted if FB seq

    SEQ_WAIT_NS(100);

    //unclamp drampll
    if (GblSkipDrampllClamp == LW_FALSE) {
      if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
          LwU32 drampll_config;
          drampll_config = lwr_reg->pfb_fbpa_fbio_drampll_config;
          drampll_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,0x0,drampll_config);
          lwr_reg->pfb_fbpa_fbio_drampll_config = drampll_config;
          SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);
      } else {
          fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_DISABLE,fbio_mode_switch);
          lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
          REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
      }
    }

    SEQ_WAIT_US(2);

    //Setting FBIO_SPARE[12] to disable tx_e_serializer
    //Ga10x -change needed for Reliablity concerns
    lwr_reg->pfb_fbpa_fbio_spare = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE_VALUE, _SERIALIZER, _ENABLE, lwr_reg->pfb_fbpa_fbio_spare);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);

    //G6x - programming pam4 mode once clocks are up
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
      SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST,g6x_fbio_broadcast);
    }

    //disable drampll as the target path is not cascaded
    gddr_disable_drampll();
    FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x2);

    SEQ_WAIT_NS(100);

    //bypass refmpll
    gddr_bypass_refmpll(1);
    FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x3);

    SEQ_WAIT_NS(100);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_clk_switch_onesrc_2_refmpll")
void
gddr_clk_switch_onesrc_2_refmpll
(
        void
)
{
    SEQ_WAIT_NS(5);

    //Enable sync mode on refmpll
    gddr_en_sync_mode_refmpll();

    //disable the refmpll bypass
    gddr_bypass_refmpll(0);

    if (gbl_en_fb_mclk_sw && !gbl_fb_use_ser_priv) {
        REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x4);
    }

    SEQ_WAIT_US(2);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_clk_switch_refmpll_2_onesrc")
void
gddr_clk_switch_refmpll_2_onesrc
(
        void
)
{

    //Enable sync mode on refmpll
    gddr_en_sync_mode_refmpll();

    SEQ_WAIT_NS(100);

    //bypass refmpll
    gddr_bypass_refmpll(1);
    FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x5);

    //switch to onesrc
    if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
        fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _ONESOURCE, fbio_mode_switch);
        lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
    } else {
        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _ONESOURCE, fbio_mode_switch);
        lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
    }

    SEQ_WAIT_NS(100);

    // !! MODE_SWITCH register not written to HW since onesrc is not supported

    //    //unclamp refmpll
    //    fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _DISABLE,fbio_mode_switch);
    //    REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_clk_switch_refmpll_2_drampll")
void
gddr_clk_switch_refmpll_2_drampll
(
        void
)
{
    //Setting FBIO_SPARE[12] to disable tx_e_serializer
    //Ga10x -change needed for Reliablity concerns
    lwr_reg->pfb_fbpa_fbio_spare = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE_VALUE, _SERIALIZER, _DISABLE, lwr_reg->pfb_fbpa_fbio_spare);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);
//    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);

    SEQ_WAIT_NS(100);

    //clamp the Drampll
    GblSkipDrampllClamp = (gTT.perfMemClkBaseEntry.spare_field5 & 0x1);
    if (GblSkipDrampllClamp == LW_FALSE) {
      if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
          LwU32 drampll_config;
          drampll_config = lwr_reg->pfb_fbpa_fbio_drampll_config;
          drampll_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,0x1,drampll_config);
          lwr_reg->pfb_fbpa_fbio_drampll_config = drampll_config;
          SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);
      } else {
          fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_ENABLE,fbio_mode_switch);
          lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
          REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
      }
    }

    SEQ_WAIT_NS(100);

    FB_REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(2),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    //disable drampll bypass
    gddr_bypass_drampll(0);
    FB_REG_WR32_STALL(BAR0,LW_PPRIV_FBP_FBPS_PRI_FENCE,0x6);

    SEQ_WAIT_NS(100);

    FB_REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(3),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
        fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _DRAMPLL, fbio_mode_switch);
        lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
        SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV,fbio_mode_switch);

        //unclamp drampll
        if (GblSkipDrampllClamp == LW_FALSE) {
          LwU32 drampll_config;
          drampll_config = lwr_reg->pfb_fbpa_fbio_drampll_config;
          drampll_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,0x0,drampll_config);
          lwr_reg->pfb_fbpa_fbio_drampll_config = drampll_config;
          SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);
        }
    } else {
        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _DRAMPLL, fbio_mode_switch);
        REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);

        //unclamp drampll
        if (GblSkipDrampllClamp == LW_FALSE) {
          fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_DISABLE,fbio_mode_switch);
          lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
          REG_WR32_STALL(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
        }
    }
    SEQ_WAIT_US(2);
    
    //Setting FBIO_SPARE[12] to 0 tx_e_serializer
    //Ga10x -change needed for Reliablity concerns
    lwr_reg->pfb_fbpa_fbio_spare = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE_VALUE, _SERIALIZER, _ENABLE, lwr_reg->pfb_fbpa_fbio_spare);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);
//    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);

    //G6x - programming pam4 mode once clocks are up
    if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) {
      SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST,g6x_fbio_broadcast);
//      SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_BROADCAST,g6x_fbio_broadcast);
    }
    FB_REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_1(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_reset_testcmd_ptr")
inline
void
gddr_reset_testcmd_ptr
(
        void
)
{
    LwU32 ptr_reset_on;
    LwU32 ptr_reset_off;

    ptr_reset_on = lwr_reg->pfb_fbpa_ptr_reset;
    ptr_reset_on = FLD_SET_DRF(_PFB, _FBPA_PTR_RESET, _TESTCMD, _ON,ptr_reset_on);

    ptr_reset_off = FLD_SET_DRF(_PFB, _FBPA_PTR_RESET, _TESTCMD, _OFF,ptr_reset_on);
    lwr_reg->pfb_fbpa_ptr_reset = ptr_reset_off;

    SEQ_REG_WR32(LOG, LW_PFB_FBPA_PTR_RESET,ptr_reset_on);

    //Wait for 1us to help the reset propogate
    SEQ_WAIT_US(1);

    //de-asserting ptr_reset
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_PTR_RESET,ptr_reset_off);

    //Wait for 1us to help the reset propogate
    SEQ_WAIT_US(1);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_cmd_brlshift")
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
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CMD_DELAY,fbio_cmd_delay);
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_en_dq_term_save")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG10,fbio_cfg10);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_restore_dq_term")
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
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG10,fbio_cfg10);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_disable_edc_tracking")
void
gddr_disable_edc_tracking
(
        void
)
{
    if (((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))
        && (gbl_prev_edc_tr_enabled == 1)) {
        LwU32 fbpa_fbio_edc_tracking;
        LwU32 edc_ctrl;
        LwU32 vref_tracking;
        LwU32 mrs12_val;

        fbpa_fbio_edc_tracking = lwr_reg->pfb_fbpa_fbio_edc_tracking;
        edc_ctrl = lwr_reg->pfb_fbpa_training_edc_ctrl;
        vref_tracking = lwr_reg->pfb_fbpa_fbio_vref_tracking;

        fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING,_EN, 0, fbpa_fbio_edc_tracking);
        lwr_reg->pfb_fbpa_fbio_edc_tracking = fbpa_fbio_edc_tracking;
        // hardcoding is ok since we're disabling
        new_reg->pfb_fbpa_generic_mrs4 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS4, _ADR_GDDR5_EDC_HOLD_PATTERN, 0xF, new_reg->pfb_fbpa_generic_mrs4);

        //disabling PRBS pattern for edc tracking
        mrs12_val = DRF_DEF(_PFB,_FBPA_GENERIC_MRS12,_ADR_GDDR6X_PRBS,_OFF);
        SEQ_REG_RMW32(LOG, LW_PFB_FBPA_GENERIC_MRS12,     mrs12_val,  DRF_SHIFTMASK(LW_PFB_FBPA_GENERIC_MRS12_ADR_GDDR6X_PRBS));

        edc_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_EDC_CTRL, _EDC_TRACKING_MODE, 0, edc_ctrl);
        lwr_reg->pfb_fbpa_training_edc_ctrl = edc_ctrl;
	//enabling pwrd if edctracking was performed before - edc tracking disables pwrd
        if (REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast) == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)
        {
          vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,   _VREF_EDGE_PWRD,      1,   vref_tracking);
          vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,   _EDGE_HSSA_PWRD,      1,   vref_tracking);
          vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING,   _EDGE_INTERP_PWRD,    1,   vref_tracking);
        }

        vref_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _EN, 0, vref_tracking);
        lwr_reg->pfb_fbpa_fbio_vref_tracking = vref_tracking;
        lwr_reg->pfb_fbpa_fbio_vref_tracking2 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING2, _SHADOW_SELECT, 0x0, lwr_reg->pfb_fbpa_fbio_vref_tracking2);

	lwr_reg->pfb_fbpa_fbio_edc_tracking_debug = FLD_SET_DRF_NUM(_PFB,    _FBPA_FBIO_EDC_TRACKING_DEBUG,  _DISABLE_WDQS_PI_UPDATES,        0,    lwr_reg->pfb_fbpa_fbio_edc_tracking_debug);
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
        {
            seq_pa_write(LW_PFB_FBPA_FBIO_EDC_TRACKING,fbpa_fbio_edc_tracking);
            seq_pa_write(LW_PFB_FBPA_TRAINING_EDC_CTRL,edc_ctrl);
	    seq_pa_write(LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,lwr_reg->pfb_fbpa_fbio_edc_tracking_debug);
            //disabling Vref tracking
            seq_pa_write(LW_PFB_FBPA_FBIO_VREF_TRACKING,vref_tracking);
            seq_pa_write(LW_PFB_FBPA_FBIO_VREF_TRACKING2,  lwr_reg->pfb_fbpa_fbio_vref_tracking2);
        }
        else
#endif
        {
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,fbpa_fbio_edc_tracking);
            REG_WR32(LOG, LW_PFB_FBPA_TRAINING_EDC_CTRL,edc_ctrl);
	    REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG,lwr_reg->pfb_fbpa_fbio_edc_tracking_debug);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING,vref_tracking);
            REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING2,  lwr_reg->pfb_fbpa_fbio_vref_tracking2);
        }
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_edc_tr_dbg_ref_recenter")
void
gddr_edc_tr_dbg_ref_recenter
(
        LwU32 val
)
{
    LwU32 edc_tr_dbg;
    LwU32 vref_tr;
    if (((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) ||(gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))
	 &&(gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)) {
        edc_tr_dbg = lwr_reg->pfb_fbpa_fbio_edc_tracking_debug;
        vref_tr    = lwr_reg->pfb_fbpa_fbio_vref_tracking;
        if ((val ==1) && ((gblEnEdcTracking) || (gblElwrefTracking))) {
            vref_tr    = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _DISABLE_TRAINING_UPDATES, 1,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            edc_tr_dbg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING_DEBUG, _DISABLE_REFRESH_RECENTER, 1,edc_tr_dbg);
        } else if ((val == 0) && ((gblEnEdcTracking) || (gblElwrefTracking))) {
            lwr_reg->pfb_fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING, _DISABLE_TRAINING_UPDATES, 1, lwr_reg->pfb_fbpa_fbio_edc_tracking);
            edc_tr_dbg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING_DEBUG, _DISABLE_REFRESH_RECENTER, 0,edc_tr_dbg);
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,lwr_reg->pfb_fbpa_fbio_edc_tracking);
        }
        else {
            lwr_reg->pfb_fbpa_fbio_edc_tracking = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING, _DISABLE_TRAINING_UPDATES, 0, lwr_reg->pfb_fbpa_fbio_edc_tracking);
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING,lwr_reg->pfb_fbpa_fbio_edc_tracking);
            vref_tr    = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_VREF_TRACKING, _DISABLE_TRAINING_UPDATES, 0,lwr_reg->pfb_fbpa_fbio_vref_tracking);
            edc_tr_dbg = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_EDC_TRACKING_DEBUG, _DISABLE_REFRESH_RECENTER, 0,edc_tr_dbg);
        }
        lwr_reg->pfb_fbpa_fbio_edc_tracking_debug = edc_tr_dbg;
        lwr_reg->pfb_fbpa_fbio_vref_tracking = vref_tr;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_EDC_TRACKING_DEBUG, edc_tr_dbg);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_VREF_TRACKING, vref_tr);

    }
}


GCC_ATTRIB_SECTION("memory", "gddr_save_clr_fbio_pwr_ctrl")
void
gddr_save_clr_fbio_pwr_ctrl
(
        void
)
{
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_PWR_CTRL,0x00000000);

    lwr_reg->pfb_fbpa_fbio_wck_wclk_pad_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_WCK_CLK_PAD_CTRL,
             _WCK_TX_PWRD_CLK_SER, 0, lwr_reg->pfb_fbpa_fbio_wck_wclk_pad_ctrl);

    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_WCK_CLK_PAD_CTRL, lwr_reg->pfb_fbpa_fbio_wck_wclk_pad_ctrl);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_restore_fbio_pwr_ctrl")
void
gddr_restore_fbio_pwr_ctrl
(
        void
)
{
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_PWR_CTRL,lwr_reg->pfb_fbpa_fbio_pwr_ctrl);

    LwU32 fbpa_fbio_broadcast1;
    if (gTT.perfMemClkStrapEntry.Flags9.PMC11SEF9EnMaxQPwr)
    {
        fbpa_fbio_broadcast1 =  DRF_DEF(_PFB,_FBPA_FBIO_BROADCAST1,_POWER_EN_INTERP,_ENABLED) |
             DRF_DEF(_PFB,_FBPA_FBIO_BROADCAST1,_POWER_EN_RCVR,_ENABLED) |
             DRF_DEF(_PFB,_FBPA_FBIO_BROADCAST1,_POWER_EN_RX_CLK,_ENABLED);
    
        if ((((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) &&
              ((gbl_vendor_id == MEMINFO_ENTRY_VENDORID_SAMSUNG) ||
               (gbl_vendor_id == MEMINFO_ENTRY_VENDORID_HYNIX))) ||
              (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) &&
              (gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1ASR == 0))    // asr_acpd_lp2 disabled
        {
            lwr_reg->pfb_fbpa_fbio_wck_wclk_pad_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_WCK_CLK_PAD_CTRL,
                     _WCK_TX_PWRD_CLK_SER, 1, lwr_reg->pfb_fbpa_fbio_wck_wclk_pad_ctrl);

            SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_WCK_CLK_PAD_CTRL, lwr_reg->pfb_fbpa_fbio_wck_wclk_pad_ctrl);
        }
    }
    else
    {
        fbpa_fbio_broadcast1 = 0;
    }
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_BROADCAST1, fbpa_fbio_broadcast1);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_clk_pattern")
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

    //qdr
    if ((gbl_g5x_cmd_mode) && (gbl_vendor_id == MEMINFO_ENTRY_VENDORID_MICRON)) {
        LwU8 wck_pattern_mask = 1;
        if ((gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {    // g5x
            wck_pattern = 0x33;
            ck_pattern = 0x0f;
        } else {        //gddr6
            // new for br8
            if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) { //g6 mode
                if ((isDensity16GB) && (gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0MemDLL == PMCT_STRAP_MEMORY_DLL_DISABLED)) {
                    wck_pattern = 0xaa;     // need to change the clocks to half data rate
                    wck_pattern_mask = 0;
                }
                else {
                    wck_pattern = 0x99;     // was xcc
                }
            } else { //g6x mode
                wck_pattern = 0xcc;
            }
            ck_pattern = 0x3c;      // was xc3
        }
        lwr_reg->pfb_fbio_misc_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MISC_CONFIG, _WCK_PATTERN_MASK, wck_pattern_mask,lwr_reg->pfb_fbio_misc_config);
    }
    else {
        //  for br8
        if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
            // highspeed
            wck_pattern = 0xaa;     // was x55
            ck_pattern = 0x3c;      // was xc3
        } else {
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
            wck_pattern = 0xaa;
            ck_pattern = 0xF0;      // was xc3
#else
            wck_pattern = 0x55;
            ck_pattern = 0x0F;      // was xc3
#endif
        }
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_AD10X))
        lwr_reg->pfb_fbio_misc_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MISC_CONFIG, _WCK_PATTERN_MASK, 3,lwr_reg->pfb_fbio_misc_config);
#else
        lwr_reg->pfb_fbio_misc_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_MISC_CONFIG, _WCK_PATTERN_MASK, 0,lwr_reg->pfb_fbio_misc_config);
#endif
    }

    fbpa_cfg0 = lwr_reg->pfb_fbpa_cfg0;
    if ((gbl_g5x_cmd_mode) || ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))) {
        //fbpa_cfg0
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _BL, _BL16,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _TIMING_UNIT, _DRAMDIV2,fbpa_cfg0);
    } else {
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _BL, _BL8,fbpa_cfg0);
        fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _TIMING_UNIT, _DRAMDIV2,fbpa_cfg0);
    }
    lwr_reg->pfb_fbpa_cfg0 = fbpa_cfg0;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,fbpa_cfg0);

    //fbpa_fbiotrng_subp0_wck
    fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp0_wck;
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCK, _CTRL_BYTE01,wck_pattern,fbiotrng_wck);
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCK, _CTRL_BYTE23,wck_pattern,fbiotrng_wck);
    lwr_reg->pfb_fbpa_fbiotrng_subp0_wck = fbiotrng_wck;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCK,fbiotrng_wck);

    if (trng_ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
        //fbpa_fbiotrng_subp0_wckb
        fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp0_wckb;
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCKB, _CTRL_BYTE01, wck_pattern,fbiotrng_wck);
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP0_WCKB, _CTRL_BYTE23, wck_pattern,fbiotrng_wck);
        lwr_reg->pfb_fbpa_fbiotrng_subp0_wckb = fbiotrng_wck;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0_WCKB,fbiotrng_wck);
    }

    //fbpa_fbiotrng_subp1_wck
    fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp1_wck;
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCK, _CTRL_BYTE01,wck_pattern,fbiotrng_wck);
    fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCK, _CTRL_BYTE23,wck_pattern,fbiotrng_wck);
    lwr_reg->pfb_fbpa_fbiotrng_subp1_wck = fbiotrng_wck;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCK,fbiotrng_wck);

    if (trng_ctrl_wck_pads == LW_PFB_FBPA_TRAINING_CTRL_WCK_PADS_EIGHT) {
        //fbpa_fbiotrng_subp1_wckb
        fbiotrng_wck = lwr_reg->pfb_fbpa_fbiotrng_subp1_wckb;
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCKB, _CTRL_BYTE01, wck_pattern,fbiotrng_wck);
        fbiotrng_wck = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIOTRNG_SUBP1_WCKB, _CTRL_BYTE23, wck_pattern,fbiotrng_wck);
        lwr_reg->pfb_fbpa_fbiotrng_subp1_wckb = fbiotrng_wck;
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP1_WCKB,fbiotrng_wck);
    }

    //fbio_clk_ctrl
    fbio_clk_ctrl = lwr_reg->pfb_fbpa_clk_ctrl;
    fbio_clk_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CLK_CTRL, _CLK0, ck_pattern, fbio_clk_ctrl);
    fbio_clk_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CLK_CTRL, _CLK1, ck_pattern, fbio_clk_ctrl);
    lwr_reg->pfb_fbpa_clk_ctrl = fbio_clk_ctrl;
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CLK_CTRL,fbio_clk_ctrl);

    // for br8
    if (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
        SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_MISC_CONFIG,lwr_reg->pfb_fbio_misc_config);
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_update_arb_registers")
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
    LwU32 fbpa_dir_arb_cfg5=0;
    LwU32 fbpa_dir_arb_cfg6=0;
    LwU32 fbpa_dir_arb_cfg7;
    LwU32 fbpa_write_arb_cfg;
    LwU32 fbpa_read_arb_cfg;
    LwU32 fbpa_act_arb_cfg;
    LwU32 fbpa_act_arb_cfg1;
    LwU32 fbpa_misc_dramc;


    fbpa_act_arb_cfg2   = REG_RD32(BAR0, LW_PFB_FBPA_ACT_ARB_CFG2);
    fbpa_read_bankq_cfg = REG_RD32(BAR0, LW_PFB_FBPA_READ_BANKQ_CFG);
    fbpa_write_bankq_cfg = REG_RD32(BAR0, LW_PFB_FBPA_WRITE_BANKQ_CFG);
    fbpa_read_ft_cfg    = REG_RD32(BAR0, LW_PFB_FBPA_READ_FT_CFG);
    fbpa_write_ft_cfg   = REG_RD32(BAR0, LW_PFB_FBPA_WRITE_FT_CFG);
    fbpa_dir_arb_cfg0   = REG_RD32(BAR0, LW_PFB_FBPA_DIR_ARB_CFG0);
    fbpa_dir_arb_cfg1   = REG_RD32(BAR0, LW_PFB_FBPA_DIR_ARB_CFG1);
    fbpa_dir_arb_cfg7 = REG_RD32(BAR0, LW_PFB_FBPA_DIR_ARB_CFG7);

    fbpa_write_arb_cfg = REG_RD32(BAR0, LW_PFB_FBPA_WRITE_ARB_CFG);
    fbpa_read_arb_cfg = REG_RD32(BAR0, LW_PFB_FBPA_READ_ARB_CFG);
    fbpa_act_arb_cfg = REG_RD32(BAR0, LW_PFB_FBPA_ACT_ARB_CFG);
    fbpa_act_arb_cfg1 = REG_RD32(BAR0, LW_PFB_FBPA_ACT_ARB_CFG1);
    fbpa_misc_dramc = REG_RD32(BAR0, LW_PFB_FBPA_MISC_DRAMC);


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
    //latency Interval
    fbpa_dir_arb_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG0, _LATENCY_INTERVAL,
            gTT.perfMemClkBaseEntry.FBPAConfig1.PMCFBPAC1LI,fbpa_dir_arb_cfg0);


    //write cmd fifo depth
    fbpa_write_arb_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_WRITE_ARB_CFG, _CMD_FIFO_DEPTH,
            gTT.perfMemClkBaseEntry.Param2.PMC11EP2WriteCmdFifoDepth, fbpa_write_arb_cfg);
    //write bg holdoff
    fbpa_write_arb_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_WRITE_ARB_CFG, _BANK_GROUP_HOLDOFF,
            gTT.perfMemClkBaseEntry.Param2.PMC11EP2WriteBankGroupHoldoff, fbpa_write_arb_cfg);
    //read bg holdoff
    fbpa_read_arb_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_READ_ARB_CFG, _BANK_GROUP_HOLDOFF,
            gTT.perfMemClkBaseEntry.Param2.PMC11EP2ReadBankGroupHoldoff, fbpa_read_arb_cfg);
    //act bgsec
    if(gTT.perfMemClkBaseEntry.Param2.PMC11EP2ActArbCfgBgsec) {
        fbpa_act_arb_cfg = FLD_SET_DRF(_PFB, _FBPA_ACT_ARB_CFG, _BGSEC, _ENABLED,fbpa_act_arb_cfg);
    } else {
        fbpa_act_arb_cfg = FLD_SET_DRF(_PFB, _FBPA_ACT_ARB_CFG, _BGSEC, _DISABLED,fbpa_act_arb_cfg);
    }
    //act wr bank hit bgsec weight
    if(gTT.perfMemClkBaseEntry.Param2.PMC11EP2WrBankHitBgsecWt) {
        fbpa_act_arb_cfg1 = FLD_SET_DRF(_PFB, _FBPA_ACT_ARB_CFG1, _WR_BANK_HIT_BGSEC_WT, _ENABLED,fbpa_act_arb_cfg1);
    } else {
        fbpa_act_arb_cfg1 = FLD_SET_DRF(_PFB, _FBPA_ACT_ARB_CFG1, _WR_BANK_HIT_BGSEC_WT, _DISABLED,fbpa_act_arb_cfg1);
    }
    //dramc bg enable on ccd
    if(gTT.perfMemClkBaseEntry.Param2.PMC11EP2BgEnableInCcd) {
        fbpa_misc_dramc = FLD_SET_DRF(_PFB, _FBPA_MISC_DRAMC, _BG_ENABLE_ON_CCD, _ENABLE,fbpa_misc_dramc);
    } else {
        fbpa_misc_dramc = FLD_SET_DRF(_PFB, _FBPA_MISC_DRAMC, _BG_ENABLE_ON_CCD, _DISABLE,fbpa_misc_dramc);
    }

    //programming additionals arb registers bug 2987773 based on size of  vbios struct - to avoid revlock
    if (sizeof(gTT.perfMemClkBaseEntry.Param2) > 2) {
         fbpa_write_arb_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_WRITE_ARB_CFG, _WRITEPULL_REQ,
            gTT.perfMemClkBaseEntry.Param2.PMC11EP2WrArbCfgWrPullReq, fbpa_write_arb_cfg);

        fbpa_write_ft_cfg = FLD_SET_DRF_NUM(_PFB, _FBPA_WRITE_FT_CFG, _MAX_CHAIN_CLOSE_FL, 
            gTT.perfMemClkBaseEntry.Param2.PMC11EP2WrFtCfgMaxChainCloseFL,fbpa_write_ft_cfg);

        fbpa_dir_arb_cfg1 = FLD_SET_DRF_NUM(_PFB, _FBPA_DIR_ARB_CFG1, _NO_TURN_IF_BANK_HIT,
            gTT.perfMemClkBaseEntry.Param2.PMC11EP2ArbCfgNoTurnIfBankHit, fbpa_dir_arb_cfg1);
    }
	    
    // DIR_ARB_CFG5/6/7 contains sector count values for dir arb decision bug 3004541
    LwBool arb_cfg567_valid = LW_FALSE;
    PerfMemClk11Header* pMCHp = gBiosTable.pMCHp;
    // check that the entries are available by the increase of perf mem clk table entry size
    if (pMCHp->BaseEntrySize > 85)
    {
        arb_cfg567_valid = LW_TRUE;
        
        // High Eff Rd/Wr Sectors
        fbpa_dir_arb_cfg5 = DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG5,_HIGH_EFF_RD_SECTORS,
                gTT.perfMemClkBaseEntry.Param3.PMC11EP3HighEffRdSectors) |
            DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG5,_HIGH_EFF_WR_SECTORS,
                gTT.perfMemClkBaseEntry.Param3.PMC11EP3HighEffWrSectors);

        // Low Eff Rd/Wr Sectors
        fbpa_dir_arb_cfg6 = DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG6,_LOW_EFF_RD_SECTORS,
                gTT.perfMemClkBaseEntry.Param4.PMC11EP4LowEffRdSectors) |
            DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG6,_LOW_EFF_WR_SECTORS,
                gTT.perfMemClkBaseEntry.Param4.PMC11EP4LowEffWrSectors);

        // Low Eff Min Rd Int
        fbpa_dir_arb_cfg7 = FLD_SET_DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG7,_LOW_EFF_MIN_RD_INT,
                gTT.perfMemClkBaseEntry.Param5.PMC11EP5LowEffMinRdInt, fbpa_dir_arb_cfg7);
        // Low Eff Min Wr Int
        fbpa_dir_arb_cfg7 = FLD_SET_DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG7,_LOW_EFF_MIN_WR_INT,
                gTT.perfMemClkBaseEntry.Param5.PMC11EP5LowEffMinWrInt, fbpa_dir_arb_cfg7);
        // High Eff Rd Limit
        fbpa_dir_arb_cfg7 = FLD_SET_DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG7,_HIGH_EFF_RD_LIMIT,
                gTT.perfMemClkBaseEntry.Param5.PMC11EP5HighEffRdLimit, fbpa_dir_arb_cfg7);
        // High Eff Wr Limit
        fbpa_dir_arb_cfg7 = FLD_SET_DRF_NUM(_PFB,_FBPA_DIR_ARB_CFG7,_HIGH_EFF_WR_LIMIT,
                gTT.perfMemClkBaseEntry.Param5.PMC11EP5HighEffWrLimit, fbpa_dir_arb_cfg7);
    }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
    {
        seq_pa_write(LW_PFB_FBPA_ACT_ARB_CFG2,fbpa_act_arb_cfg2);
        seq_pa_write(LW_PFB_FBPA_READ_BANKQ_CFG,fbpa_read_bankq_cfg);
        seq_pa_write(LW_PFB_FBPA_WRITE_BANKQ_CFG,fbpa_write_bankq_cfg);
        seq_pa_write(LW_PFB_FBPA_READ_FT_CFG,fbpa_read_ft_cfg);
        seq_pa_write(LW_PFB_FBPA_WRITE_FT_CFG,fbpa_write_ft_cfg);
        seq_pa_write(LW_PFB_FBPA_DIR_ARB_CFG0,fbpa_dir_arb_cfg0);
        seq_pa_write(LW_PFB_FBPA_DIR_ARB_CFG1,fbpa_dir_arb_cfg1);
        seq_pa_write(LW_PFB_FBPA_WRITE_ARB_CFG,fbpa_write_arb_cfg);
        seq_pa_write(LW_PFB_FBPA_READ_ARB_CFG,fbpa_read_arb_cfg); 
        seq_pa_write(LW_PFB_FBPA_ACT_ARB_CFG,fbpa_act_arb_cfg);
        seq_pa_write(LW_PFB_FBPA_ACT_ARB_CFG1,fbpa_act_arb_cfg1); 
        seq_pa_write(LW_PFB_FBPA_MISC_DRAMC,fbpa_misc_dramc);
        if (arb_cfg567_valid)
        {
            seq_pa_write(LW_PFB_FBPA_DIR_ARB_CFG5,fbpa_dir_arb_cfg5);
            seq_pa_write(LW_PFB_FBPA_DIR_ARB_CFG6,fbpa_dir_arb_cfg6);
            seq_pa_write(LW_PFB_FBPA_DIR_ARB_CFG7,fbpa_dir_arb_cfg7);
        }
    }
    else
#endif
    {
        REG_WR32(LOG, LW_PFB_FBPA_ACT_ARB_CFG2,fbpa_act_arb_cfg2);
        REG_WR32(LOG, LW_PFB_FBPA_READ_BANKQ_CFG,fbpa_read_bankq_cfg);
        REG_WR32(LOG, LW_PFB_FBPA_WRITE_BANKQ_CFG,fbpa_write_bankq_cfg);
        REG_WR32(LOG, LW_PFB_FBPA_READ_FT_CFG,fbpa_read_ft_cfg);
        REG_WR32(LOG, LW_PFB_FBPA_WRITE_FT_CFG,fbpa_write_ft_cfg);
        REG_WR32(LOG, LW_PFB_FBPA_DIR_ARB_CFG0,fbpa_dir_arb_cfg0);
        REG_WR32(LOG, LW_PFB_FBPA_DIR_ARB_CFG1,fbpa_dir_arb_cfg1);
        REG_WR32(LOG, LW_PFB_FBPA_WRITE_ARB_CFG,fbpa_write_arb_cfg);
        REG_WR32(LOG, LW_PFB_FBPA_READ_ARB_CFG,fbpa_read_arb_cfg); 
        REG_WR32(LOG, LW_PFB_FBPA_ACT_ARB_CFG,fbpa_act_arb_cfg);
        REG_WR32(LOG, LW_PFB_FBPA_ACT_ARB_CFG1,fbpa_act_arb_cfg1); 
        REG_WR32(LOG, LW_PFB_FBPA_MISC_DRAMC,fbpa_misc_dramc);
        if (arb_cfg567_valid)
        {
            REG_WR32(LOG, LW_PFB_FBPA_DIR_ARB_CFG5,fbpa_dir_arb_cfg5);
            REG_WR32(LOG, LW_PFB_FBPA_DIR_ARB_CFG6,fbpa_dir_arb_cfg6);
            REG_WR32(LOG, LW_PFB_FBPA_DIR_ARB_CFG7,fbpa_dir_arb_cfg7);
        }
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_pad_ctrl2")
void
gddr_pgm_pad_ctrl2
(
        void
)
{
    LwU32 VbiosEdc;
    LwU32 meminfo5540;
    meminfo5540 = TABLE_VAL(mInfoEntryTable->mie5540);
    VbiosEdc = REF_VAL(14:14, meminfo5540);

    lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _E_RX_DFE, GblRxDfe,lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2);
    if (gblEnEdcTracking || VbiosEdc)
    {
        lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _E_RX_DFE_EDC, GblRxDfe,lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2);
    }
    else
    {
        lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2 =  FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_BYTE_PAD_CTRL2, _E_RX_DFE_EDC, 0,lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2);
    }
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_BYTE_PAD_CTRL2,lwr_reg->pfb_fbpa_fbio_byte_pad_ctrl2);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_tx_eq_ac")
void
gddr_pgm_tx_eq_ac
(
        void
)
{
// bug 2781005  Update/Add a per strap bit in PMCT tables called TX_EQ_AC
    LwU8 eq_ac = gTT.perfMemClkStrapEntry.Flags9.PMC11SEF9EnPreemphTxEqAc;
    LwU32 fbio_cmd_tx_e_preemph_ctrl;

    LwU8 dq_tx_eq_ac = gTT.perfMemClkStrapEntry.Flags9.PMC11SEF9EnDqTxEqAc;
    LwU8 wck_tx_eq_ac = gTT.perfMemClkStrapEntry.Flags9.PMC11SEF9EnWckTxEqAc;
    LwU8 ca_tx_eq_ac = gTT.perfMemClkStrapEntry.Flags9.PMC11SEF9EnCkTxEqAc;
    LwU32 fbio_pad_ctrl_misc1;

    fbio_pad_ctrl_misc1 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_PAD_CTRL_MISC1);
    fbio_cmd_tx_e_preemph_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CMD_TX_E_PREEMPH_CTRL);

    // bug 3152070
    if (eq_ac > 0)
    {
        //eq_ac field(27:22), 6 bits
        eq_ac = gTT.perfMemClkStrapEntry.Flags10.PMC11SEF10CtrlEqAc & 0x3F;
    }
    fbio_cmd_tx_e_preemph_ctrl = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_CMD_TX_E_PREEMPH_CTRL,_EQ_AC, eq_ac, fbio_cmd_tx_e_preemph_ctrl);

    // possibly need another spare for this...
    // dq_tx_eq_ac field(7:2), 6 bits
    fbio_pad_ctrl_misc1  = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_PAD_CTRL_MISC1,_DQ_TX_EQ_AC, dq_tx_eq_ac, fbio_pad_ctrl_misc1);
    fbio_pad_ctrl_misc1  = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_PAD_CTRL_MISC1,_WCK_TX_EQ_AC, wck_tx_eq_ac, fbio_pad_ctrl_misc1);
    fbio_pad_ctrl_misc1  = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_PAD_CTRL_MISC1,_CK_TX_EQ_AC, ca_tx_eq_ac, fbio_pad_ctrl_misc1);

    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CMD_TX_E_PREEMPH_CTRL, fbio_cmd_tx_e_preemph_ctrl);
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_PAD_CTRL_MISC1, fbio_pad_ctrl_misc1);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_cmd_fifo_depth")
void
gddr_pgm_cmd_fifo_depth
(
        void
)
{
    LwU32 fbpa_spare;
    LwU16 idx;
    //    FW_MBOX_WR32(8, 0xABABABAB);
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    if (!gbl_en_fb_mclk_sw)
    {
        if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) {
            fbpa_spare = DRF_DEF(_PFB, _FBPA_SPARE, _BITS_CMDFF_DEPTH, _SAFE);
        } else {
            fbpa_spare = DRF_DEF(_PFB, _FBPA_SPARE, _BITS_CMDFF_DEPTH, _MAX);
        }

        SEQ_REG_RMW32(LOG,LW_PFB_FBPA_SPARE,fbpa_spare,DRF_SHIFTMASK(LW_PFB_FBPA_SPARE_BITS_CMDFF_DEPTH));
    }
    else
#endif
    {
        for (idx = 0; idx < MAX_PARTS; idx++)
        {
            if (isPartitionEnabled(idx))
            {
                fbpa_spare = REG_RD32(BAR0, unicast(LW_PFB_FBPA_SPARE,idx));
                if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) {
                    fbpa_spare = FLD_SET_DRF(_PFB, _FBPA_SPARE, _BITS_CMDFF_DEPTH, _SAFE, fbpa_spare);
                } else  {
                    fbpa_spare = FLD_SET_DRF(_PFB, _FBPA_SPARE, _BITS_CMDFF_DEPTH, _MAX, fbpa_spare);
                }
                REG_WR32_STALL(LOG, unicast(LW_PFB_FBPA_SPARE,idx), fbpa_spare);
                fbpa_spare = REG_RD32(BAR0, unicast(LW_PFB_FBPA_SPARE,idx));
            }
        }
    }
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_pgm_cfg_pwrd_war")
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


GCC_ATTRIB_SECTION("memory", "gddr_pgm_gpio_fbvdd")
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

    GpioVddReg = REG_RD32(BAR0,(LW_GPIO_OUTPUT_CNTL(FbvddGpioIndex)));

    //https://rmopengrok.lwpu.com/source/xref/chips_a/drivers/resman/kernel/clk/fermi/clkgf117.c#3924
    if (gbl_lwrr_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW) {
        GpioVddReg = FLD_SET_DRF(_GPIO,_OUTPUT_CNTL, _IO_OUTPUT,_0, GpioVddReg);
    } else {
        GpioVddReg = FLD_SET_DRF(_GPIO,_OUTPUT_CNTL, _IO_OUTPUT,_1, GpioVddReg);
    }
    GpioVddReg = FLD_SET_DRF(_GPIO,_OUTPUT_CNTL, _IO_OUT_EN,_YES, GpioVddReg);

    // for med = tristate
    // FLD_SET_DRF(_GPIO,_OUTPUT_CNTL,_SEL,_NORMAL, GpioVddReg);
    // FLD_SET_DRF(_GPIO,_OUTPUT_CNTL,_IO_OUT_EN,_NO, GpioVddReg);
    // FLD_SET_DRF(_GPIO,_OUTPUT_CNTL,_OPEN_DRAIN,_DISABLE, GpioVddReg);
    // FLD_SET_DRF(_GPIO,_OUTPUT_CNTL,_PULLD,_UP/_DOWN/_NONE, GpioVddReg);
    //

    REG_WR32_STALL(LOG, LW_GPIO_OUTPUT_CNTL(8),GpioVddReg);

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


GCC_ATTRIB_SECTION("memory", "gddr_pgm_save_vref_val")
void
gddr_pgm_save_vref_val
(
        void
)
{
    LwU32 dq_vref;
    LwU32 dqs_vref;

    //FOLLOWUP : Needs Vbios update - using spare field for now
    //spare_field3[7:0]  Dq_vref
    //spare_field3[15:8] Dqs_vref
    dq_vref  =  REF_VAL(LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1_DQ_VREF_CODE,gTT.perfMemClkBaseEntry.spare_field3);
    dqs_vref =  REF_VAL(LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1_DQS_VREF_CODE,gTT.perfMemClkBaseEntry.spare_field3);

    //save current vref value
    //reading values from VREF code3, pad8 - Dq, Pad9 - EDC (wdqs)
    // not needed since read in load_reg_values
    //    lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = REG_RD32(LOG, LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3);
    GblSaveDqVrefVal  = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD8,lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3);
    GblSaveDqsVrefVal = REF_VAL(LW_PFB_FBPA_FBIOTRNG_SUBP0BYTE0_VREF_CODE3_PAD9,lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3);

    //ensure dq_vref is actually configured else load with 70% (0x48) vref
    if (dq_vref != 0x00) {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_CODE, dq_vref,lwr_reg->pfb_fbio_delay_broadcast_misc1);
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_MID_CODE, dq_vref,lwr_reg->pfb_fbio_delay_broadcast_misc1);
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_UPPER_CODE, dq_vref,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    } else {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_CODE, 0x9A,lwr_reg->pfb_fbio_delay_broadcast_misc1); 
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_MID_CODE, 0x9A,lwr_reg->pfb_fbio_delay_broadcast_misc1); 
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_UPPER_CODE, 0x9A,lwr_reg->pfb_fbio_delay_broadcast_misc1); 
    }
    //Dqs vref
    if (dqs_vref != 0x00) {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQS_VREF_CODE, dqs_vref,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    } else {
        lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQS_VREF_CODE, 0x9D,lwr_reg->pfb_fbio_delay_broadcast_misc1); //FIXME Akashyap to confirm the values for g6x
    }
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1,lwr_reg->pfb_fbio_delay_broadcast_misc1);

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_restore_vref_val")
void
gddr_restore_vref_val
(
        void
)
{
    lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_CODE, GblSaveDqVrefVal,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQS_VREF_CODE, GblSaveDqsVrefVal,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    lwr_reg->pfb_fbio_delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DELAY_BROADCAST_MISC1, _DQ_VREF_MID_CODE, GblSaveDqVrefVal,lwr_reg->pfb_fbio_delay_broadcast_misc1); 
    SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_DELAY_BROADCAST_MISC1,lwr_reg->pfb_fbio_delay_broadcast_misc1);
    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_calibration_delay")
void
gddr_calibration_delay
(
        void
)
{
    LwU32 wait_time;
    LwU8 trimmer_step  = 2;
    LwU8 ignore_start  = REF_VAL(LW_PFB_FBPA_FBIO_DDLLCAL_CTRL_IGNORE_START, lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl);
    LwU8 start_trm     = REF_VAL(LW_PFB_FBPA_FBIO_DDLLCAL_CTRL_START_TRM,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl );
    LwU8 sample_delay  = REF_VAL(LW_PFB_FBPA_FBIO_DDLLCAL_CTRL_SAMPLE_DELAY, lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl );
    LwU8 sample_count  = REF_VAL(LW_PFB_FBPA_FBIO_DDLLCAL_CTRL_SAMPLE_COUNT, lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl );

    //TODO tighten the wait time based on simulation waves - asatish, sangitac
    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
      if ((gbl_g5x_cmd_mode) || (gbl_vendor_id != MEMINFO_ENTRY_VENDORID_MICRON)) {
        if (gbl_pam4_mode == LW_TRUE) { //g6x pam4
		//FIXME - to fix period x 4 x 2 with sangitac
           wait_time     = ((((1000000/gbl_target_freqMHz) * 4 * 2) / (trimmer_step * 2) - (ignore_start ? 0 : start_trm)) *
                              (2 + (sample_delay + 2) + ((1 << sample_count) + 1))) + 100;
	    } else { //g6 micron qdr or Samsung ddr
           wait_time     = ((((1000000/gbl_target_freqMHz) * 4 * 8) / (trimmer_step * 2) - (ignore_start ? 0 : start_trm)) *
                              (2 + (sample_delay + 2) + ((1 << sample_count) + 1))) + 100;
	    }
      } else {
        wait_time     = ((((1000000/gbl_target_freqMHz) * 4 * 2) / (trimmer_step * 2) - (ignore_start ? 0 : start_trm)) *
                        (2 + (sample_delay + 2) + ((1 << sample_count) + 1))) + 100;
      }
    } else {
      //wait_time     = ((((1000000/gbl_target_freqMHz) / (trimmer_step * 2) - (ignore_start ? 0 : start_trm)) * 2) *
      //	         (2 + (sample_delay + 2) + ((1 << sample_count) + 1))) + 100;

      // P5 in legacy mode. ddllcal_value /=8; (expected_cal_value - start_trim)* number of br16clks. Counted in br2clks.
      wait_time     = (((((1000000/gbl_target_freqMHz) / (trimmer_step * 2)/8) - (ignore_start ? 0 : start_trm)) * 2) *
		         (2 + (sample_delay + 2) + ((1 << sample_count) + 1))) + 100;
      //FIXME to correct the delay value
      wait_time *= 8;
    }  

    LwU32 wait_ns = 1+ (wait_time *(1000000/gbl_target_freqMHz))/1000;

    FW_MBOX_WR32(7, 0xaaaaaaa1);
    FW_MBOX_WR32(7, trimmer_step);
    FW_MBOX_WR32(7, ignore_start);
    FW_MBOX_WR32(7, start_trm);
    FW_MBOX_WR32(7, sample_delay);
    FW_MBOX_WR32(7, sample_count);
    FW_MBOX_WR32(7, wait_time);
    FW_MBOX_WR32(7, wait_ns);
    //FIXME change the error code
    if ((wait_time & 0x80000000) > 0 ) {
      FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_TRAINING_ERROR);
    }
    SEQ_WAIT_NS(wait_ns);
}


GCC_ATTRIB_SECTION("memory", "gddr_exit_delay")
void
gddr_exit_delay
(
        void
)
{
    LwU8 sample_count  = REF_VAL(LW_PFB_FBPA_FBIO_DDLLCAL_CTRL_SAMPLE_COUNT, lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl );
    LwU32 wait_exit_delay = ((1 << sample_count) + 200);
    LwU32 wait_ns = 1000/gbl_target_freqMHz;
    FW_MBOX_WR32(7, 0xaaaaaaa2);
    FW_MBOX_WR32(7, wait_exit_delay);
    FW_MBOX_WR32(7, wait_ns);
    if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
        wait_ns = 10000;    // 10us
    } else {
        wait_ns = wait_exit_delay * wait_ns;
    }
    SEQ_WAIT_NS(wait_ns);
}


GCC_ATTRIB_SECTION("memory", "gddr_self_ref_exit_delay")
void
gddr_self_ref_exit_delay
(
        void
)
{
    LwU32 SreDelaytCK;
    LwU32 tRFC;
    LwU8 Config3Pdex;
    LwU16 Asr2AsrEx;
    LwU8 Asr2Nrd;
    LwU32 DramclkPeriodps;
    LwU32 tCK;


    Config3Pdex = REF_VAL(LW_PFB_FBPA_CONFIG3_PDEX,gTT.perfMemTweakBaseEntry.Config3);
    Asr2AsrEx = REF_VAL(LW_PFB_FBPA_TIMING13_ASR2ASREX,lwr_reg->pfb_fbpa_timing13);
    Asr2Nrd = REF_VAL(LW_PFB_FBPA_TIMING13_ASR2NRD, lwr_reg->pfb_fbpa_timing13);

    tRFC = (REF_VAL(LW_PFB_FBPA_CONFIG10_RFC_MSB,gTT.perfMemTweakBaseEntry.Config10) << DRF_SHIFT(LW_PFB_FBPA_CONFIG0_RFC))  | REF_VAL(LW_PFB_FBPA_CONFIG0_RFC, gTT.perfMemTweakBaseEntry.Config0);

    if (Config3Pdex > Asr2Nrd)
    {
        SreDelaytCK = Config3Pdex;
    }
    else
    {
        SreDelaytCK = Asr2Nrd;
    }

    if (Asr2AsrEx > SreDelaytCK)
    {
        SreDelaytCK = Asr2AsrEx;
    }

    if (tRFC > SreDelaytCK)
    {
        SreDelaytCK = tRFC;
    }

    DramclkPeriodps = (1000000/gbl_target_freqMHz);

    tCK = U32_DIV_ROUND((DramclkPeriodps * 4), 1000);

    SEQ_WAIT_NS((SreDelaytCK * tCK));

}


GCC_ATTRIB_SECTION("memory", "gddr_disable_cmd_periodic_update_cal")
inline void
gddr_disable_cmd_periodic_update_cal
(
        void
)
{
    lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1 = FLD_SET_DRF(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1,_CMD_DISABLE_PERIODIC_UPDATE,_INIT,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1);
    lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1 = FLD_SET_DRF(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1,_DISABLE_PERIODIC_UPDATE,_INIT,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1);
    SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_DDLLCAL_CTRL1,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1);
}

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
// TODO: stefans/mwuu  we should unify dmem/imem access to the pafalcon between bootloader and sequence loader
GCC_ATTRIB_SECTION("resident", "send_pa_seq")
inline void
send_pa_seq
(
        void
)
{
    LwU16 i=0;
    LwU32 dmemc;


    dmemc = REG_RD32(BAR0, LW_PPAFALCON_FALCON_DMEMC(0));
    // set auto increment write
    //need to set BLK, OFFS, SETTAG, SETLVL?
    dmemc = FLD_SET_DRF(_PPAFALCON_FALCON, _DMEMC, _AINCW, _TRUE, dmemc);
    dmemc = FLD_SET_DRF(_PPAFALCON_FALCON, _DMEMC, _VA, _FALSE, dmemc);  // needed?
    dmemc = FLD_SET_DRF_NUM(_PPAFALCON_FALCON, _DMEMC, _ADDRESS, _pafalcon_location_table[SEQUENCER_DATA], dmemc);

    REG_WR32(LOG, LW_PPAFALCON_FALCON_DMEMC(0), dmemc);

    if (gbl_pa_instr_cnt >= PA_MAX_SEQ_SIZE)
    {
        FW_MBOX_WR32(13,gbl_pa_instr_cnt);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_FALCON_SIZE_ERROR);
    }
    while (i < gbl_pa_instr_cnt)
    {
        REG_WR32(LOG, LW_PPAFALCON_FALCON_DMEMD(0), gbl_pa_mclk_sw_seq[i]);
        i++;
    }
}
#endif


GCC_ATTRIB_SECTION("resident", "fb_sync_poll")
LwU32
fb_sync_poll
(
    LwU32 sync_val,
    LwU32 sync_mask
)
{
    LwBool poll = LW_TRUE;
    LwU32 sync_status;
    LwU32 sync_status2;

    do
    {
        sync_status = REG_RD32(BAR0,LW_PPAFALCON_SYNC_STATUS);
        sync_status2 = REG_RD32(BAR0,LW_PPAFALCON_SYNC_STATUS2);

        if ((0xFFFF0000 & sync_status2) != 0)       // error detected
        {
            poll = LW_FALSE;
            LwU32 sync_ctrl = REG_RD32(BAR0, LW_PPAFALCON_SYNC_CTRL);
            FW_MBOX_WR32(11, sync_ctrl);
            FW_MBOX_WR32(12, sync_status2);
            FW_MBOX_WR32(13, sync_status);
            REG_WR32(BAR0, LW_PPAFALCON_SYNC_CTRL, (sync_ctrl & ~PA_CTRL_START)); // clear start bit
            FBFLCN_HALT(((0xFFFF & sync_status)<<16) | FBFLCN_ERROR_CODE_MCLK_SWITCH_PA_ERROR);
        }
        FW_MBOX_WR32(10, sync_status);

        // check that the sync_status(AND)
        if ((sync_mask & sync_status) == (sync_mask & sync_val))
        {
            poll = LW_FALSE;
        }
        else
        {
            OS_PTIMER_SPIN_WAIT_NS(1000);
        }
//TODO: add timeout?
    } while (poll);

    // save current sync_status for reuse
    return sync_status;
}


GCC_ATTRIB_SECTION("resident", "fb_decode")
void
fb_decode
(
        LwU8  segID_end
)
{
    LwU8 i=0;
    LwU32 ctrl_val;
    LwU32 sync_status;
    LwU8 fn_enum;

    while (i < gbl_fb_sync_cnt)
    {
        // FB polls for sync request
        //sync_status = gbl_pa_sync_req[i];
        sync_status = PA_STATUS_SYNC_REQ | PA_STATUS_RUNNING;
        sync_status = fb_sync_poll((sync_status|PA_STATUS_SYNC_VALID), (0xF|PA_STATUS_SYNC_VALID));       // 0x1006, 0x1516, 0x0156

        // sync_status now contains sync_fn
        fn_enum = (sync_status >> 4) & 0xF;                 //  0, 1, 5

        sync_status &= 0xFFF0;  // clear sync_req & running    0x1000, 0x1510, 0x0150

        // setup sync_ctrl, set START bit(1)
        ctrl_val = sync_status | PA_CTRL_START;             // 0x1001  0x1511, 0x0151

        // write ctrl ack(4)
        REG_WR32(LOG, LW_PPAFALCON_SYNC_CTRL, (ctrl_val | PA_CTRL_SYNC_ACK));  // sync_ack = 0x4

        // process sync fn
        switch (fn_enum)
        {
            case FN_GDDR_FB_STOP:
                memoryStopFb_HAL(&Fbflcn, 1, &startFbStopTimeNs);
                REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
                break;
            case FN_GDDR_PGM_GPIO_FBVDD:
                gddr_pgm_gpio_fbvdd();
                break;
            case FN_GDDR_SET_FBVREF:
                gddr_set_fbvref();
                break;
            case FN_PGM_CALGROUP_VTT_VAUX_GEN:
                REG_WR32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN, new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
                //100ns settle time for vauxc
                OS_PTIMER_SPIN_WAIT_NS(100);
                break;
            case FN_GDDR_RECALIBRATE:
                gdrRecalibrate();
                break;
            case FN_GDDR_PA_SYNC:
                break;
//            case FN_GDDR6_LOAD_RDPAT:
//                FuncLoadRdPatterns(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6);
//                break;
            case FN_SAVE_VREF_VALUES:
                lwr_reg->pfb_fbpa_fbio_vref_tracking = new_reg->pfb_fbpa_fbio_vref_tracking;
//                func_save_vref_values();
                break;
            default:
            // invalid function...
                FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_ILWALID_SYNC_FN);
                break;
        }

        //poll sync_status sync_clear[3:2] = 'b11       clear 0xC, running 2 = 0xE
        sync_status |= PA_STATUS_SYNC_CLEAR | PA_STATUS_RUNNING;    // 0x100E, 0x151E, 0x015E
        fb_sync_poll(sync_status, 0xFFFF);            // 0x100E, 0x151E, 0x015E

        // write ctrl with ack=0
        REG_WR32(LOG, LW_PPAFALCON_SYNC_CTRL, ctrl_val);
        i++;
    }

// poll status done & check segid_end
    sync_status = (segID_end<<8) | PA_STATUS_DONE;
    fb_sync_poll(sync_status, 0xFFFF);
}

GCC_ATTRIB_SECTION("memory", "gddr_mclk_switch")
LwU32
gddr_mclk_switch
(
        LwBool bPreAddressBootTraining,
        LwBool bPostAddressBootTraining,
        LwBool bPostWckRdWrBootTraining
)
{
    LwU32 fbStopTimeNs      = 0;
    gbl_fb_sync_cnt = 0;
    gbl_pa_instr_cnt = 2;       // [0]=start+size, [1]=checksum
// TODO: possibly in the future to clear the entire array?

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
    gbl_pa_mclk_sw_seq[1] = 0;  // clear checksum to 0
#endif
    gbl_prev_fbio_idx = 0;

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
    stopTempTracking();
#endif
    if (!bPostAddressBootTraining && !bPostWckRdWrBootTraining) {
        // This portion needs to be exelwted if this is doMclkSwitchInSingleStage or doMclkSwitchInMultipleStages with bPreAddressBootTraining set.
        //LwU32 fbio_addr_ddll;

        //step 3:
        //read current clock source
        if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
            lwr_reg->pfb_fbio_ctrl_ser_priv = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV);   //mode=13:12
            gddr_lwrr_clk_source = lwr_reg->pfb_fbio_ctrl_ser_priv;
        } else {
            lwr_reg->pfb_ptrim_fbio_mode_switch = REG_RD32(BAR0, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
            gddr_lwrr_clk_source = lwr_reg->pfb_ptrim_fbio_mode_switch;
        }
        lwr_reg->pfb_fbpa_fbio_broadcast = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_BROADCAST);
        gbl_ddr_mode = REF_VAL(LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE, lwr_reg->pfb_fbpa_fbio_broadcast);
        gddr_get_target_clk_source();

        if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) &&
            (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH))
        {
            gbl_pam4_mode = LW_TRUE;
	  //excluding halving of freq for vauxc/vdd switch
	  //as no pll programming happens
	        if ((gbl_bFbVddSwitchNeeded == LW_FALSE) && 
              (gbl_bVauxChangeNeeded == LW_FALSE)) {
                gbl_target_freqMHz = (gbl_target_freqMHz + 1) / 2;
 	        }
        } 
	    else
        {
            gbl_pam4_mode = LW_FALSE;
        }

        //ignoring the callwlation of current freq as it is required only for reporting
        //gddr_retrieve_lwrrent_freq();
        //includes step 5 to callwlate onesrc divider value.
        gddr_callwlate_target_freq_coeff();

        //step 4:
        gddr_load_reg_values();

        //step 9:
        gddr_calc_exp_freq_reg_values();

//not needed for ga10x        if ((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce) &&
//not needed for ga10x                (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6)) {
//not needed for ga10x            FuncLoadAdrPatterns(gbl_ddr_mode);

        //step 13 //FOLLOWUP : accessing PLL bypass registers -> to be performed after fb_stop. - change for document

        //step 15 // callwlate time needed for mlk switch
        //based on priv profilining and time consumption for each priv access the read modified write can
        //be made to initial read and shadow the write values in the register.
        gddr_power_on_internal_vref();

        //Step 16 - Read ACPD
        gbl_dram_acpd = REF_VAL(LW_PFB_FBPA_CFG0_DRAM_ACPD,lwr_reg->pfb_fbpa_cfg0);
	

	//Ga10x - Clearing CFG0_ACPD before fb stop
        lwr_reg->pfb_fbpa_cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD,_NO_POWERDOWN,lwr_reg->pfb_fbpa_cfg0);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,lwr_reg->pfb_fbpa_cfg0);

        //gbl_dram_asr = REG_RD32(LOG, LW_PFB_FBPA_DRAM_ASR);

        //---------------------------
        //FB STOP
        //Step 17 - FB_stop
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw) {       // run mclk sw on PA Falcon
            fb_pa_sync(16, FN_GDDR_FB_STOP);
// DEBUG: to cause interrupt handler in PA to run
//seq_pa_write(LW_CFBFALCON_FALCON_IRQSSET,DRF_SHIFTMASK(LW_CFBFALCON_FALCON_IRQSSET_SWGEN1)); // doesn't work
//seq_pa_write(LW_CFBFALCON_BAR0_PRIV_TIMEOUT,0xA);  // works
        }
        else
#endif
        {
            memoryStopFb_HAL(&Fbflcn, 1, &startFbStopTimeNs);
            REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
        }
	
            if (GblFirstSwitchVrefCodeSave == LW_FALSE) {
              gddr_save_vref_code3(); 
              GblFirstSwitchVrefCodeSave = LW_TRUE;
            }

            // get vref_code3 for current gddr_target_clk_path and restore fbio_delay
            if (gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) {
               lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0;
               new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0;
            } else if (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
               lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1;
               new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1;
            } else {
               lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = lwr_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2;
               new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3 = new_reg->pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2;
            }

        //update MRS1 soon entering FB_STOP for termination - from pascal MRS review


        //Step 18 - Skipped : EMRS fields accessed.
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5)
            {
                SEQ_REG_WR32(LOG, LW_PFB_FBPA_GENERIC_MRS1,new_reg->pfb_fbpa_generic_mrs1);
                gddr_flush_mrs_reg();
            }
            //step 21 -
            // Addr Term gets update for G6 just after FbStop for G6 for low to high speed change. Vref needs to get updated just after termination
            // However, for a high speed to low speed change, Addr Term gets updated just before fbStart. Vref will get updated at that point.
            // G5 will follow same sequence for ease of code maintenance
            if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)
            {
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
                {
                    fb_pa_sync(21, FN_GDDR_SET_FBVREF);
                }
                else
#endif
                {
                    gddr_set_fbvref();
                }
            }
        }

        //Step 22 - Disable ACPD and REPLAY
        gddr_disable_acpd_and_replay();

        //Step 24 - Disable Refresh
        gddr_disable_refresh(); // FOLLOWUP:wait for 1us??

        SEQ_WAIT_US(1);

        //Step 25 - Disable periodic training
        gddr_disable_periodic_training();

        //New addition - gddr6
        gddr_disable_edc_tracking();

        //New addition for ga10x uarch update
        gddr_disable_cmd_periodic_update_cal();

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

        SEQ_WAIT_US(1);

        //Enable self refresh via testcmd and wait for 1us
        //Reset the testcmd pointers before entering to self refresh for G5x and G6
        if (gbl_selfref_opt == LW_FALSE)
        {
            gddr_reset_testcmd_ptr();
        }
        gddr_enable_self_refresh();

        SEQ_WAIT_US(1);

        //Clear WCK per Byte trimmers
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {

            //Power down/up the Interpolators/DLL based on existing clock path and requested clock path
            gddr_power_intrpl_dll();
        }

        //clear fbio pwr ctrl 
        gddr_save_clr_fbio_pwr_ctrl();

        //CYA Step
        //Resetting Entry wdat_ptr and edc_ptr for some % of randoms
        ////        FW_MBOX_WR32(4, 0x00000622);
        // SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xAAFBFEED);
        ////        FW_MBOX_WR32(2, 0x00000622);

        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(0),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));

        if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
            fbio_mode_switch = lwr_reg->pfb_fbio_ctrl_ser_priv;
        } else {
            fbio_mode_switch = lwr_reg->pfb_ptrim_fbio_mode_switch;
        }

        if (gTT.perfMemClkBaseEntry.spare_field4 & 0x1) {
          gddr_disable_low_pwr_edc();
          gddr_disable_crc_mrs4();
        }
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

            if (gbl_en_fb_mclk_sw && !gbl_fb_use_ser_priv) {
                REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x7);
            }
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
            if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
                if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) ||(gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH)) {
                    //switch to REFMPLL and unclamp refmpll
                    LwU32 refmpll_config = lwr_reg->pfb_fbpa_fbio_refmpll_config;
                    refmpll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _INIT, refmpll_config);
                    lwr_reg->pfb_fbpa_fbio_refmpll_config = refmpll_config;
                    fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _REFMPLL, fbio_mode_switch);
                    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV, fbio_mode_switch);
                    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
                }
                else {
                    //switch to onesrc and unclamp onesrc
                    fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _ONESOURCE, fbio_mode_switch);
                    //                    fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _ONESOURCES_STOP_DISABLE, fbio_mode_switch);
                    SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV, fbio_mode_switch);
                }
                lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
            } else {
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
                lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
                REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
            }

            //FB unit sim only
            if(gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
                SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xFBFBFEED);
            } else {
                SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xFBFAFEED);
            }
            SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,gbl_target_freqMHz); //MHz

            //step 32.16
            //enable_padmacro_sync
            //release ptr offset
            gddr_cfg_padmacro_sync_ptr_offset();

            ////Additional Step
            gddr_disable_low_pwr_en_edc_early();
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

            if (!(gbl_bFbVddSwitchNeeded || gbl_bVauxChangeNeeded))
            {
                FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x8);
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
                    if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
                        //switch to REFMPLL and unclamp refmpll
                        LwU32 refmpll_config = lwr_reg->pfb_fbpa_fbio_refmpll_config;
                        refmpll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _INIT, refmpll_config);
                        lwr_reg->pfb_fbpa_fbio_refmpll_config = refmpll_config;
                        fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _REFMPLL, fbio_mode_switch);
                        lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
                        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV, fbio_mode_switch);
                        SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,refmpll_config);
                    } else {
                        //switch to REFMPLL and unclamp refmpll
                        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _REFMPLL,fbio_mode_switch);
                        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _DISABLE,fbio_mode_switch);
                        lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
                        REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
                    }
                }
                //FB unit sim only
                if(gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
                    SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xFBFBFEED);
                } else {
                    SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xFBFAFEED);
                }
                SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,gbl_target_freqMHz); //MHz
            }
	    else 
	    {

              // skip the pll update when doing the vvdd voltage switch, which is a low speed to low speed
              // switch at the same frequency

              //Step 34: Update VDDQ
              if (gbl_bFbVddSwitchNeeded) {
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                  if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
                  {
                      fb_pa_sync(34, FN_GDDR_PGM_GPIO_FBVDD);
                  }
                  else
#endif
                  {
                      //program actual fbvdd
                      gddr_pgm_gpio_fbvdd();
                  }
              }

              //VAUXP, VCLAMP, VAUXC programming
              if (gbl_bVauxChangeNeeded) {
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
                {
                    //Ga10x vaux change require to stop the clock to avoid glitches
		            //bug 2750445 describes the sequence.
                    //Note: Sync point needed here due to A/B partition needs to be sync'd for refmpll
                    if (!gbl_bFbVddSwitchNeeded)
                    {
                        fb_pa_sync(33, FN_GDDR_PA_SYNC);
                    }
                    seq_pa_write(LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _ENABLE, lwr_reg->pfb_fbpa_fbio_refmpll_config));
	                seq_pa_write(LW_PFB_FBPA_FBIO_VTT_CTRL2,new_reg->pfb_fbpa_fbio_vtt_ctrl2);

                    fb_pa_sync(35, FN_PGM_CALGROUP_VTT_VAUX_GEN);
                    // wait done in FN_PGM_CALGROUP_VTT_VAUX_GEN
		    //reenable clocks
                    seq_pa_write(LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _DISABLE, lwr_reg->pfb_fbpa_fbio_refmpll_config));
                }
                else
#endif
                {
                    //Ga10x vaux change require to stop the clock to avoid glitches
		    //bug 2750445 describes the sequence.
                    if (gbl_fb_use_ser_priv) {
                        REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG,FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _ENABLE, lwr_reg->pfb_fbpa_fbio_refmpll_config));
		    } else {
                        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _ENABLE,fbio_mode_switch);
                        lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
                        REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
		    }

                    REG_WR32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL2,new_reg->pfb_fbpa_fbio_vtt_ctrl2);
                    REG_WR32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN, new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
                    //100ns settle time for vauxc
                    OS_PTIMER_SPIN_WAIT_NS(100);

		    //reenable clocks
                    if (gbl_fb_use_ser_priv) {
                      lwr_reg->pfb_fbpa_fbio_refmpll_config = FLD_SET_DRF(_PFB, _FBPA_FBIO_REFMPLL_CONFIG, _STOP_SYNCMUX, _DISABLE, lwr_reg->pfb_fbpa_fbio_refmpll_config);
                      REG_WR32(LOG, LW_PFB_FBPA_FBIO_REFMPLL_CONFIG, lwr_reg->pfb_fbpa_fbio_refmpll_config);
		    } else {
                        fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _REFMPLL_STOP_SYNCMUX, _DISABLE,fbio_mode_switch);
                        lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
                        REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
		    }
                }
		//adding delay for clock settle time
                SEQ_WAIT_US(2);
              }
              //FB unit sim only
              if(gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
                  SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xFBFBFEED);
              } else {
                  SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,0xFBFAFEED);
              }
              SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,gbl_target_freqMHz); //MHz
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

            FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0x9);

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

            GblSkipDrampllClamp = (gTT.perfMemClkBaseEntry.spare_field5 & 0x1);

            //Switch the dramclk mode and unclamp the clocks
            if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
                fbio_mode_switch = FLD_SET_DRF(_PFB, _FBPA_FBIO_CTRL_SER_PRIV, _MODE_SWITCH, _DRAMPLL,fbio_mode_switch);
                lwr_reg->pfb_fbio_ctrl_ser_priv = fbio_mode_switch;
                SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV, fbio_mode_switch);

		if (GblSkipDrampllClamp == LW_FALSE) {
                  LwU32 drampll_config;
                  drampll_config = lwr_reg->pfb_fbpa_fbio_drampll_config;
                  drampll_config = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,0x0,drampll_config);
                  lwr_reg->pfb_fbpa_fbio_drampll_config = drampll_config;
                  SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,drampll_config);
		}
            } else {
                fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _DRAMCLK_MODE, _DRAMPLL,fbio_mode_switch);
		if (GblSkipDrampllClamp == LW_FALSE) {
                  fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0,_DISABLE,fbio_mode_switch);
		}
                lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
                REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
            }

            //FB unit sim only
            LwU32 falcon_monitor = 0xFBFAFEED;
            if (gbl_g5x_cmd_mode) {
                falcon_monitor = 0xFBFDFEED;
            } else if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
                falcon_monitor = 0xFBFCFEED;
            } else if(gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
                falcon_monitor = 0xFBFBFEED;
            }
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, falcon_monitor);
            SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,gbl_target_freqMHz); //MHz

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

            FB_REG_WR32_STALL(BAR0, LW_PPRIV_FBP_FBPS_PRI_FENCE,0xA);
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

            if (gbl_bFbVddSwitchNeeded) {
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw) {   // run mclk sw on PA Falcon
                    fb_pa_sync(38, FN_GDDR_PGM_GPIO_FBVDD);
                }
                else
#endif
                {  //program actual fbvdd
                    gddr_pgm_gpio_fbvdd();
                }
            }

            //VAUXP, VCLAMP, VAUXC programming
            if (gbl_bVauxChangeNeeded) {

                //Setting FBIO_SPARE[12] to disable tx_e_serializer
		//Ga10x -change needed for Reliablity concerns
                lwr_reg->pfb_fbpa_fbio_spare = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE_VALUE, _SERIALIZER, _DISABLE, lwr_reg->pfb_fbpa_fbio_spare);
                SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);

                GblSkipDrampllClamp = (gTT.perfMemClkBaseEntry.spare_field5 & 0x1);
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
                {

                    //Ga10x vaux change require to stop the clock to avoid glitches
		    //bug 2750445 describes the sequence.
                    if (GblSkipDrampllClamp) {
                      seq_pa_write(LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,_ENABLE,lwr_reg->pfb_fbpa_fbio_drampll_config));
		            }

	            seq_pa_write(LW_PFB_FBPA_FBIO_VTT_CTRL2,new_reg->pfb_fbpa_fbio_vtt_ctrl2);
                    fb_pa_sync(39, FN_PGM_CALGROUP_VTT_VAUX_GEN);
                    // wait done in FN_PGM_CALGROUP_VTT_VAUX_GEN

		    //reenable clocks
                    if (GblSkipDrampllClamp) {
                      seq_pa_write(LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,_DISABLE,lwr_reg->pfb_fbpa_fbio_drampll_config));
		            }

                }
                else
#endif
                {

                    //Ga10x vaux change require to stop the clock to avoid glitches
		    //bug 2750445 describes the sequence.
                    if (GblSkipDrampllClamp) {
                      if (gbl_fb_use_ser_priv) {
                          REG_WR32(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,_ENABLE,lwr_reg->pfb_fbpa_fbio_drampll_config));
		      } else {
                          fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0, _ENABLE,fbio_mode_switch);
                          lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
                          REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
		      }
		    }

                    REG_WR32(LOG, LW_PFB_FBPA_FBIO_VTT_CTRL2,new_reg->pfb_fbpa_fbio_vtt_ctrl2);
                    REG_WR32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN, new_reg->pfb_fbpa_fbio_calgroup_vttgen_vauxgen);
                    //100ns settle time for vauxc
                    OS_PTIMER_SPIN_WAIT_NS(100);

                    if (GblSkipDrampllClamp) {
                      if (gbl_fb_use_ser_priv) {
                        REG_WR32(LOG, LW_PFB_FBPA_FBIO_DRAMPLL_CONFIG,FLD_SET_DRF(_PFB, _FBPA_FBIO_DRAMPLL_CONFIG, _CLAMP_P0,_DISABLE,lwr_reg->pfb_fbpa_fbio_drampll_config));
		      } else {
                          fbio_mode_switch = FLD_SET_DRF(_PTRIM, _SYS_FBIO_MODE_SWITCH, _CLAMP_P0, _DISABLE,fbio_mode_switch);
                          lwr_reg->pfb_ptrim_fbio_mode_switch = fbio_mode_switch;
                          REG_WR32(LOG, LW_PTRIM_SYS_FBIO_MODE_SWITCH,fbio_mode_switch);
		      }
		    }

                }
		
		//adding delay for clock settle time
                SEQ_WAIT_US(2);

                //Setting FBIO_SPARE[12] to 0 tx_e_serializer
		//Ga10x -change needed for Reliablity concerns
                lwr_reg->pfb_fbpa_fbio_spare = FLD_SET_DRF(_PFB, _FBPA_FBIO_SPARE_VALUE, _SERIALIZER, _ENABLE, lwr_reg->pfb_fbpa_fbio_spare);
                SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);
            }

            //FB unit sim only
            LwU32 falcon_monitor = 0xFBFAFEED;
            if (gbl_g5x_cmd_mode) {
                falcon_monitor = 0xFBFDFEED;
            } else {
                if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
                    falcon_monitor = 0xFBFCFEED;
                } else if(gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) {
                    falcon_monitor = 0xFBFBFEED;
                }
            }
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, falcon_monitor);
            SEQ_REG_WR32(LOG,LW_PFB_FBPA_FALCON_MONITOR,gbl_target_freqMHz); //MHz

        }
        // set tx_eq_ac from PMCT straps
        gddr_pgm_tx_eq_ac();

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
            //Pgm rx dfe based on vbios
            gddr_pgm_pad_ctrl2();

            gddr_pgm_shadow_reg_space();
            //        FW_MBOX_WR32(2, 0x00000029);

        //Asatish addition new from doc - programming the clock pattern before the DDLL cal
            gddr_pgm_clk_pattern();

        //step 40: //Due to bug 1536307 step 40 and 41 are interchanged
        //Initiate DDLL CAL if REFMPLL

        //re-enable CLK_TX_PWRD 
            gddr_enable_pwrd();
        }

        LwU32 fbio_ddll_cal_ctrl;
        LwU32 fbio_ddll_cal_ctrl1;

        if ((gddr_target_clk_path ==  GDDR_TARGET_REFMPLL_CLK_PATH) ||
            (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)) {
            if ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) {
//wck_8ui mode not supported for g6x yet, bug 2738210 comment #3 FIXME to enable below if and remove the gate for g6x
//if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) { //enable for high speed mode
                if ((gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) &&  (gbl_ddr_mode != LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)) { //enable for high speed mode
                  new_reg->pfb_fbpa_fbio_cfg12 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG12, _WCK_INTRP_8UI_MODE, _ENABLE, new_reg->pfb_fbpa_fbio_cfg12);
	            } else { //disable for strobe mode
                  new_reg->pfb_fbpa_fbio_cfg12 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG12, _WCK_INTRP_8UI_MODE, _DISABLE, new_reg->pfb_fbpa_fbio_cfg12);
	            }
                SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_CFG12,new_reg->pfb_fbpa_fbio_cfg12);
            }

	    //updating start trm value before calibration, previously hardcoded to 8
	    lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _START_TRM,gTT.perfMemClkBaseEntry.Flags30.PMC11EF30STARTTRM,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl);

            fbio_ddll_cal_ctrl  = lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl;
            fbio_ddll_cal_ctrl1 = lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1;
            fbio_ddll_cal_ctrl  = FLD_SET_DRF(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _CALIBRATE, _GO, fbio_ddll_cal_ctrl);
            fbio_ddll_cal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1, _OVERRIDE_DDLLCAL_CODE, 0,fbio_ddll_cal_ctrl1);
            fbio_ddll_cal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1, _DISABLE_UPDATE, 0,fbio_ddll_cal_ctrl1);
            fbio_ddll_cal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1, _CMD_DISABLE_UPDATE, 0,fbio_ddll_cal_ctrl1);
            if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)
            {
                //Ddll calibration legacy mode to be enabled only for freq less than 4ghz - 2Ghz for g6x FIXME Sangitac to confirm
                if (((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) && (gbl_target_freqMHz < 4000)) || 
		    ((gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X) && (gbl_target_freqMHz < 2000))) {
                    fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _LEGACY_MODE, 1, fbio_ddll_cal_ctrl);
                } else  {
                    fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _LEGACY_MODE, 0, fbio_ddll_cal_ctrl);
                }
                fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _END_COUNT, 0, fbio_ddll_cal_ctrl);
                fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _USE_LEGACY_TRIM, 0, fbio_ddll_cal_ctrl);
            }
            else
            {
                fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _LEGACY_MODE, 1, fbio_ddll_cal_ctrl);
                fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _USE_LEGACY_TRIM, 1, fbio_ddll_cal_ctrl);
                fbio_ddll_cal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1, _CMD_DISABLE_PERIODIC_UPDATE, 1,fbio_ddll_cal_ctrl1);
            }

            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1 = fbio_ddll_cal_ctrl1;
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl  = fbio_ddll_cal_ctrl;

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
            if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
            {
                seq_pa_write(LW_PFB_FBPA_FBIO_DDLLCAL_CTRL1,fbio_ddll_cal_ctrl1);
                seq_pa_write_stall(LW_PFB_FBPA_FBIO_DDLLCAL_CTRL,fbio_ddll_cal_ctrl);
            }
            else
#endif
            {
                REG_WR32(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL1,fbio_ddll_cal_ctrl1);
                REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL,fbio_ddll_cal_ctrl);
            }
        } else { //trimmer mode
            fbio_ddll_cal_ctrl  = lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl;
            fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _LEGACY_MODE, 1, fbio_ddll_cal_ctrl);
            fbio_ddll_cal_ctrl  = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _USE_LEGACY_TRIM, 1, fbio_ddll_cal_ctrl);
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl  = fbio_ddll_cal_ctrl;
            SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL,fbio_ddll_cal_ctrl);
        }

        // Bug 1665412: Re-calibrate the clock pad only if it's a voltage switch
        if ((gbl_bFbVddSwitchNeeded) || (gbl_bVauxChangeNeeded))
        {
            if(GblAutoCalUpdate) {
              // pre-compute these for after gddr recalibate
              LwU32 cml_clk2_update_0;
              LwU32 cml_clk2_update_1;

	      // Logic looks for a 0->1 transition to update drive strength on clk pads
              cml_clk2_update_0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CML_CLK2, _FORCE_CAL_UPDATE,0,lwr_reg->pfb_fbio_cml_clk2);
              cml_clk2_update_1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_CML_CLK2, _FORCE_CAL_UPDATE,1,lwr_reg->pfb_fbio_cml_clk2);
              new_reg->pfb_fbio_cml_clk2 = cml_clk2_update_1;
              //wait for deterministic time Sangitac to provide time taken  - NOT required

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
              if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
              {
                  fb_pa_sync(41, FN_GDDR_RECALIBRATE);
                  seq_pa_write(LW_PFB_FBPA_FBIO_CML_CLK2,cml_clk2_update_0);
                  seq_pa_write(LW_PFB_FBPA_FBIO_CML_CLK2,cml_clk2_update_1);
              }
              else
#endif
              {
                  gdrRecalibrate();
                  REG_WR32(LOG, LW_PFB_FBPA_FBIO_CML_CLK2,cml_clk2_update_0);
                  REG_WR32(LOG, LW_PFB_FBPA_FBIO_CML_CLK2,cml_clk2_update_1);
              }
	        }
        }

        //Step 42a
        //After certain delay stop ddll calibration
        if ((gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH) ||
            (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH)) {
            //RM flag DDLL_CALIBRATION_SETTLE_TIME_US 10
            // Trying to hide the ddll cal delay behind autocal on a P5(L/H) to P5(H/L) switch
            if ((!GblAutoCalUpdate) || ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded)))
            {
                gddr_calibration_delay();
            }
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1,  _DISABLE_UPDATE, 1,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1);
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1, _CMD_DISABLE_UPDATE, 1,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1);
            if (gddr_target_clk_path == GDDR_TARGET_DRAMPLL_CLK_PATH) {
                lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_DDLLCAL_CTRL1, _CMD_DISABLE_PERIODIC_UPDATE, 0,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1);
            }
            SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL1,lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl1);
            fbio_ddll_cal_ctrl = lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl;
            fbio_ddll_cal_ctrl = FLD_SET_DRF(_PFB, _FBPA_FBIO_DDLLCAL_CTRL, _CALIBRATE,_STOP,fbio_ddll_cal_ctrl);
            lwr_reg->pfb_fbpa_fbio_ddllcal_ctrl = fbio_ddll_cal_ctrl;
            SEQ_REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_DDLLCAL_CTRL,fbio_ddll_cal_ctrl);
            gddr_exit_delay();
        }

        //Step 42b
        //Write all the config/timing registers
        if ((!gbl_bFbVddSwitchNeeded) && (!gbl_bVauxChangeNeeded))
        {
            gddr_pgm_reg_values();
        } else { //programming the fbiocfg12 for wck INTRP_8UI_MODE for fbvdd and fbvauxc change
          //New for Ampere- programming timing24_xs_offset
          //for hw based SRX
          if (gbl_selfref_opt == LW_TRUE) {
            SEQ_REG_WR32(LOG,LW_PFB_FBPA_TIMING24,new_reg->pfb_fbpa_timing24);
          }

         if (gbl_ddr_mode == LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) {
               //following tb sequence to ensure the 8ui mode is on after calibration
               new_reg->pfb_fbpa_fbio_cfg12 = FLD_SET_DRF(_PFB, _FBPA_FBIO_CFG12, _WCK_INTRP_8UI_MODE, _ENABLE, new_reg->pfb_fbpa_fbio_cfg12);
               SEQ_REG_WR32(LOG,LW_PFB_FBPA_FBIO_CFG12,new_reg->pfb_fbpa_fbio_cfg12);
        }
   }

        //Note : must be called after gddr_pgm_reg_values - Dependency on fbio_cfg1_cmd_brlshft value
        // if (!((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce)))

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
        //Ga10x - Changing this to a function of RFC and pdExit
	//When using HW based srx, timing24_xs offset is used for 
	//maintaining delay in HW, so skipping the delay function
	if (gbl_selfref_opt == LW_FALSE) 
	{
          gddr_self_ref_exit_delay();
	}

        ////clear fbio pwr ctrl before address training
        ////if address training is enabled
        //if (GblEnMclkAddrTraining)
        //{
        //    gddr_save_clr_fbio_pwr_ctrl();
        //}
        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_0(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
    }
    if(!bPreAddressBootTraining && !bPostAddressBootTraining && !bPostWckRdWrBootTraining) {

        //Step 46
        //Kick off Address Training
        if ((gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6) || (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X)){

            LwU32 fbpa_fbio_spare = lwr_reg->pfb_fbpa_fbio_spare;
            if (GblEnMclkAddrTraining) {
                // force_ma2sdr = 1;
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE,       _FORCE_MA_SDR,  0x1,      fbpa_fbio_spare);
            } else {
                // force_ma2sdr = 0;
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE,       _FORCE_MA_SDR,  0x0,      fbpa_fbio_spare);
            }
            //Asatish for saving time from p8<->p5
            if(!((gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL) &&
                    ((gddr_target_clk_path == GDDR_TARGET_TRIMMER_PATH) ||
                            (gddr_target_clk_path == GDDR_TARGET_REFMPLL_CLK_PATH)))) {
                lwr_reg->pfb_fbpa_fbio_spare = fbpa_fbio_spare;
                SEQ_REG_WR32(LOG, LW_PFB_FBPA_FBIO_SPARE, lwr_reg->pfb_fbpa_fbio_spare);
            }

            if ((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce)) {
                //disable ac abi
                SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG12,
                    FLD_SET_DRF(_PFB, _FBPA_CONFIG12, _CABI_MODE, _DC, lwr_reg->pfb_fbpa_config12)
                );

                LwBool tr_status;
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)
                {
                    seq_pa_write(LW_CFBFALCON_SYNC_STATUS, 0x2E02);
                }
#endif
                tr_status = gddr_addr_training(gbl_ddr_mode);
                if (!tr_status) {
                    FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_TRAINING_ERROR);
                } else {
                    FW_MBOX_WR32(13, 0xD0D0ADD2);
                }
	
                //renable ac abi if PMCT says so
                if (gbl_ac_abi == LW_TRUE) 
                {
                    SEQ_REG_WR32(LOG, LW_PFB_FBPA_CONFIG12,
                        FLD_SET_DRF(_PFB, _FBPA_CONFIG12, _CABI_MODE, _AC, lwr_reg->pfb_fbpa_config12)
                    );
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

        ////clear fbio pwr ctrl for non address training switch
        //if (!GblEnMclkAddrTraining)
        //{
        //    gddr_save_clr_fbio_pwr_ctrl();
        //}

        //Step 46
        //Kick off Address Training
        //Only for first P0 switch after power up (including after suspend/resume or GC6), unless already done in boot training

        if (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5) {
            LwU32 fbpa_fbio_spare = lwr_reg->pfb_fbpa_fbio_spare;

            if (GblEnMclkAddrTraining) {
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE,       _FORCE_MA_SDR,  0x1,      fbpa_fbio_spare);
            } else {
                fbpa_fbio_spare = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_SPARE_VALUE,       _FORCE_MA_SDR,  0x0,      fbpa_fbio_spare);
            }
            lwr_reg->pfb_fbpa_fbio_spare = fbpa_fbio_spare;
            SEQ_REG_RMW32(BAR0, LW_PFB_FBPA_FBIO_SPARE, fbpa_fbio_spare, DRF_SHIFTMASK(LW_PFB_FBPA_FBIO_SPARE_VALUE_FORCE_MA_SDR));
        }
        if ((GblEnMclkAddrTraining) && (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5)) {

            if (!GblAddrTrPerformedOnce) {
                LwBool tr_status;
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)
                {
                    seq_pa_write(LW_CFBFALCON_SYNC_STATUS, 0x2F02);
                }
#endif
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

        //Step 50
        //Flush MRS/EMRS writes.
        //FOLLOWUP - update FBVREF - same as step 21.
            gddr_flush_mrs_reg();

            //if ((gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) && (gbl_ddr_mode ==  LW_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6))
            if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH)
            {
#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
                {
                    fb_pa_sync(50, FN_GDDR_SET_FBVREF);
                }
                else
#endif
                {
                    gddr_set_fbvref();
                }
            }
        }

        //Enabling DQ term mode and save before training bug : 2000479
        //  Terminating DQ for low speed- needed for training 2x dramclk - Turing
        //  Higher speeds are always terminated
        //  Updating Vref values for Terminated mode
        if (gddr_target_clk_path != GDDR_TARGET_DRAMPLL_CLK_PATH) {
            if(gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0GPUTerm != 0) //GpuTermination Disable
	    {
              gddr_pgm_save_vref_val();
              gddr_en_dq_term_save();
	    }
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
        // previously load rd pattern here

        gddr_mem_link_training();
        //else {
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
            if(gTT.perfMemClkStrapEntry.Flags0.PMC11SEF0GPUTerm != 0) //GpuTermination Disable
	    {
              gddr_restore_vref_val();
              gddr_restore_dq_term();
	    }
        }

        //CYA Step
        //Resetting exit wdat_ptr and edc_ptr for some % of randoms
        // FW_MBOX_WR32(4, 0x00000136);
        // SEQ_REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xABFBFEED);
        // FW_MBOX_WR32(2, 0x00000136);

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

	    //Ga10x - programming dram_asr_acpd 
	    if (gbl_asr_acpd == LW_TRUE) 
        {
          lwr_reg->pfb_fbpa_dram_asr = FLD_SET_DRF(_PFB, _FBPA_DRAM_ASR, _ACPD, _ENABLED, lwr_reg->pfb_fbpa_dram_asr);
	    }
	    else 
	    {
          lwr_reg->pfb_fbpa_dram_asr = FLD_SET_DRF(_PFB, _FBPA_DRAM_ASR, _ACPD, _DISABLED, lwr_reg->pfb_fbpa_dram_asr);
	    }
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_DRAM_ASR,lwr_reg->pfb_fbpa_dram_asr);

        //Programming acpd
	    //changed to flags7 acpd as flags0 acpd is deprecated swarm review 27476034
        LwU8 VbiosAcpd = gTT.perfMemClkStrapEntry.Flags7.PMC11SEF7ACPD;
        lwr_reg->pfb_fbpa_cfg0 =  FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACPD, VbiosAcpd,lwr_reg->pfb_fbpa_cfg0);
        SEQ_REG_WR32(LOG, LW_PFB_FBPA_CFG0,lwr_reg->pfb_fbpa_cfg0);

        if ((VbiosAcpd == 0) && (gbl_asr_acpd == LW_TRUE))
        {
            FW_MBOX_WR32(12, VbiosAcpd);
            FW_MBOX_WR32(13, gbl_asr_acpd);
            FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_ASR_ACPD_WITHOUT_ACPD);
        }

        // bug 2761960
        if (VbiosAcpd != 0)
        {
            LwU32 refctrl3;

            refctrl3   = REG_RD32(BAR0, LW_PFB_FBPA_REFCTRL3);

            if (gbl_asr_acpd == LW_FALSE)
            {
                LwU32 ic_ctrl;

                ic_ctrl = REG_RD32(BAR0, LW_PFB_FBPA_IC_CTRL);

                if ((gTT.perfMemClkBaseEntry.spare_field7 & 0x1) != 0)
                {
                    //Enter PD with PBR pending (Ampere fix for HW ACPD) - hopefully new Ampere Baseline
                    ic_ctrl = FLD_SET_DRF(_PFB, _FBPA_IC_CTRL, _DRAMC_ACPD_ON_REFSB_IDLE, _DISABLED, ic_ctrl);

                    //PBR2ABR colwersion, DRAMC traffic condition (pipeline idle)
                    refctrl3 = FLD_SET_DRF(_PFB, _FBPA_REFCTRL3, _PBR2ABR, _ENABLED, refctrl3);
                    refctrl3 = FLD_SET_DRF(_PFB, _FBPA_REFCTRL3, _PBR2ABR_SRC, _DRAMC, refctrl3);
                }
                else
                {
                    ic_ctrl = FLD_SET_DRF(_PFB, _FBPA_IC_CTRL, _DRAMC_ACPD_ON_REFSB_IDLE, _ENABLED, ic_ctrl);

                    //PBR2ABR colwersion, DRAMC traffic condition (pipeline idle)
                    refctrl3 = FLD_SET_DRF(_PFB, _FBPA_REFCTRL3, _PBR2ABR, _DISABLED, refctrl3);
                    refctrl3 = FLD_SET_DRF(_PFB, _FBPA_REFCTRL3, _PBR2ABR_SRC, _DRAMC, refctrl3);
                }

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)
                {
                    seq_pa_write(LW_PFB_FBPA_IC_CTRL, ic_ctrl);
                    seq_pa_write(LW_PFB_FBPA_REFCTRL3, refctrl3);
                }
                else
#endif
                {
                    REG_WR32(LOG, LW_PFB_FBPA_IC_CTRL, ic_ctrl);
                    REG_WR32(LOG, LW_PFB_FBPA_REFCTRL3, refctrl3);
                }
            }
/*
            else    // if (gbl_asr_acpd == LW_TRUE)
            {
                LwU32 acpd_ctrl0;
                LwU32 acpd_ctrl4=0;

                acpd_ctrl0 = REG_RD32(BAR0, LW_PFB_FBPA_ACPD_CTRL0);
                // no need since writing entire register
                //acpd_ctrl4 = REG_RD32(LOG, LW_PFB_FBPA_ACPD_CTRL4);   

                //Exit on one first sector - just set sec count =1, ignore B/W, ignore latency
                acpd_ctrl0 = FLD_SET_DRF(_PFB, _FBPA_ACPD_CTRL0, _CHECK_DISPLAY_OK, _CONSIDER, acpd_ctrl0);
                acpd_ctrl4 = FLD_SET_DRF_NUM(_PFB, _FBPA_ACPD_CTRL4, _SEC_CNT_MIN_ALL, 1, acpd_ctrl4);
                acpd_ctrl4 = FLD_SET_DRF_NUM(_PFB, _FBPA_ACPD_CTRL4, _SEC_CNT_MAX_ALL, 1, acpd_ctrl4);

                //PBR2ABR colwersion with ASR_ACPD prefer ASR
                refctrl3 = FLD_SET_DRF(_PFB, _FBPA_REFCTRL3, _PBR2ABR, _ENABLED, refctrl3);
                refctrl3 = FLD_SET_DRF(_PFB, _FBPA_REFCTRL3, _PBR2ABR_SRC, _PWRM, refctrl3);

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
                if (!gbl_en_fb_mclk_sw)
                {
                    seq_pa_write(LW_PFB_FBPA_ACPD_CTRL0, acpd_ctrl0);
                    seq_pa_write(LW_PFB_FBPA_ACPD_CTRL4, acpd_ctrl4);
                    seq_pa_write(LW_PFB_FBPA_REFCTRL3, refctrl3);
                }
                else
#endif
                {
                    REG_WR32(LOG, LW_PFB_FBPA_ACPD_CTRL0, acpd_ctrl0);
                    REG_WR32(LOG, LW_PFB_FBPA_ACPD_CTRL4, acpd_ctrl4);
                    REG_WR32(LOG, LW_PFB_FBPA_REFCTRL3, refctrl3);
                }
            }
*/
        }

        //moving arb programming before removal of fb stop
        REG_WR32(LOG, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_1(1),osPTimerTimeNsElapsedGet(&startFbStopTimeNs));
        gddr_update_arb_registers();

#if (FBFALCONCFG_FEATURE_ENABLED(PAFALCON_SEQUENCER))
        if (!gbl_en_fb_mclk_sw)     // run mclk sw on PA Falcon
        {
// Sync operation described in sync_poll
            seq_pa_write(LW_CFBFALCON_SYNC_STATUS, 0x3b02);     // Step 59 done
            SEQ_PA_END();

            // callwlate CRC
            SEQ_PA_START(gbl_pa_instr_cnt);
            gbl_pa_mclk_sw_seq[1] = gen_seq_crc32(&gbl_pa_mclk_sw_seq[2],(gbl_pa_instr_cnt-2));

            // for debug of backdoor load
            FW_MBOX_WR32(4, ((LwU32)&gbl_pa_mclk_sw_seq[0]));   // location of seq in FB DMEM
            FW_MBOX_WR32(5, (gbl_pa_instr_cnt*4));              // size in bytes of seq
            FW_MBOX_WR32(6, gbl_fb_sync_cnt);                   // num syncs

            // send sequence over
            if (gbl_pa_bdr_load == LW_FALSE)
            {
                send_pa_seq();
            }

            // start PA's
            REG_WR32_STALL(LOG, LW_PPAFALCON_SYNC_STATUS, 0);   // clear status
            REG_WR32(LOG, LW_PPAFALCON_SYNC_CTRL, 0x0001);      // start

            // poll running or error to see PA's started
            LwU32 sync_status = PA_STATUS_RUNNING;
            sync_status = fb_sync_poll(sync_status, PA_STATUS_RUN_MASK);

            // execute syncs via FB seq
            fb_decode(0x3b);
        }
#endif
        //---------------------------

        fbStopTimeNs = memoryStartFb_HAL(&Fbflcn, &startFbStopTimeNs);


        if ((GblEnMclkAddrTraining) && (!GblAddrTrPerformedOnce)) {
            enableEvent(EVENT_SAVE_TRAINING_DATA);
            GblAddrTrPerformedOnce = LW_TRUE;
        }

// for saving data into wpr the save has to happen once after the first
// mclk switch.
#if (FBFALCONCFG_FEATURE_ENABLED(TRAINING_DATA_IN_SYSMEM))
        if (gSaveRequiredAfterFirstMclkSwitch) {
            enableEvent(EVENT_SAVE_TRAINING_DATA);
            gSaveRequiredAfterFirstMclkSwitch = LW_FALSE;
        }
#endif

    }
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_TEMP_TRACKING))
    initTempTracking();
    startTempTracking();
#endif
      return fbStopTimeNs;
}


GCC_ATTRIB_SECTION("memory", "gddr_check_vaux_change")
void
gddr_check_vaux_change
(
        void
)
{
    LwU32 fbio_vtt_cntl2;
    LwU32 lwr_cdb_vauxc_level;
    LwU32 lwr_cdb_vauxp_level;
    LwU32 lwr_cdb_vclamp_level;
    LwU8 vbios_cdb_vauxc_level;


/*  Not used in Ampere
    LwU32 calgrp_vttgen_vauxgen;
    LwU32 lwr_comp_vauxc_level;
    LwU32 lwr_comp_vauxp_level;
    LwU32 lwr_comp_vclamp_level;
    //comp vauxc
    calgrp_vttgen_vauxgen = REG_RD32(LOG, LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN);
    lwr_comp_vauxc_level = REF_VAL(LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN_VTT_LV_VAUXC,calgrp_vttgen_vauxgen);
    lwr_comp_vauxp_level = REF_VAL(LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN_VTT_LV_VAUXP,calgrp_vttgen_vauxgen);
    lwr_comp_vclamp_level = REF_VAL(LW_PFB_FBIO_CALGROUP_VTTGEN_VAUXGEN_VTT_LV_VCLAMP,calgrp_vttgen_vauxgen);
*/

    //cdb vauxc
    fbio_vtt_cntl2 = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_VTT_CTRL2);
    lwr_cdb_vauxc_level = REF_VAL(LW_PFB_FBPA_FBIO_VTT_CTRL2_CDB_VAUXC, fbio_vtt_cntl2);
    lwr_cdb_vauxp_level = REF_VAL(LW_PFB_FBPA_FBIO_VTT_CTRL2_CDB_VAUXP, fbio_vtt_cntl2);
    lwr_cdb_vclamp_level = REF_VAL(LW_PFB_FBPA_FBIO_VTT_CTRL2_CDB_VCLAMP, fbio_vtt_cntl2);

    //vauxc, vauxp, vclamp of cdb must be the same
    if ((lwr_cdb_vauxc_level != lwr_cdb_vauxp_level) || (lwr_cdb_vclamp_level != lwr_cdb_vauxp_level)) {
        FW_MBOX_WR32(13,fbio_vtt_cntl2);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_VAUX_DESGIN_PGM_ERROR);
    }

    //CDB
    vbios_cdb_vauxc_level =  gTT.perfMemTweakBaseEntry.pmtbe415408.CDBVauxc4;   // 4 bit vauxc level

    gbl_target_vauxc_level = vbios_cdb_vauxc_level;
    if (vbios_cdb_vauxc_level != lwr_cdb_vauxc_level) {
        gbl_bVauxChangeNeeded = 1;
    } else {
        gbl_bVauxChangeNeeded = 0;
    }
    return;

}


GCC_ATTRIB_SECTION("memory", "doMclkSwitch")
LwU32
doMclkSwitch
(
        LwU32 targetFreqMHz
)
{
    return doMclkSwitchPrimary(targetFreqMHz, 0, 0, 0);
}


GCC_ATTRIB_SECTION("memory", "gddr_check_fbvdd_switch")
void
gddr_check_fbvdd_switch
(
        void
)
{

    gbl_lwrr_fbvddq = gTT.perfMemClkStrapEntry.Flags1.PMC11SEF1FBVDDQ;

    LwU8 prev_fbvddq;
    LwU8 fbvddGpioIndex = memoryGetGpioIndexForFbvddq_HAL();
    LwU8 ioInput = REF_VAL(LW_GPIO_OUTPUT_CNTL_IO_INPUT,
                           REG_RD32(BAR0, LW_GPIO_OUTPUT_CNTL(fbvddGpioIndex)));
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

    if ((gbl_lwrr_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW) &&
            (prev_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_HIGH)) {
        gbl_fbvdd_high_to_low = LW_TRUE;
        gbl_bFbVddSwitchNeeded  = LW_TRUE;
    } else if ((gbl_lwrr_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_HIGH) &&
            (prev_fbvddq == PERF_MCLK_11_STRAPENTRY_FLAGS2_FBVDDQ_LOW)) {
        gbl_fbvdd_low_to_high = LW_TRUE;
        gbl_bFbVddSwitchNeeded  = LW_TRUE;
    }

    return;
}


GCC_ATTRIB_SECTION("memory", "gddr_assign_local_table_pointer")
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


GCC_ATTRIB_SECTION("memory", "doMclkSwitchInMultipleStages")
LwU32
doMclkSwitchInMultipleStages
(
        LwU32 targetFreqMHz,
        LwBool bPreAddressBootTraining,
        LwBool bPostAddressBootTraining,
        LwBool bPostWckRdWrBootTraining
)
{
    return doMclkSwitchPrimary(targetFreqMHz, bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
}


GCC_ATTRIB_SECTION("resident", "setPrevDramclkMode")
void
setPrevDramclkMode
(
        void
)
{
    if ((!gbl_en_fb_mclk_sw)||(gbl_fb_use_ser_priv)) {
        gddr_lwrr_clk_source = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_CTRL_SER_PRIV);
        gbl_prev_dramclk_mode = REF_VAL(LW_PFB_FBPA_FBIO_CTRL_SER_PRIV_MODE_SWITCH,gddr_lwrr_clk_source);
    } else {
        gddr_lwrr_clk_source = REG_RD32(BAR0, LW_PTRIM_SYS_FBIO_MODE_SWITCH);
        gbl_prev_dramclk_mode = REF_VAL(LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE,gddr_lwrr_clk_source);
    }

    if (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_ONESOURCE) {
        gbl_prev_dramclk_mode = LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL;
    }
}


GCC_ATTRIB_SECTION("memory", "doMclkSwitchPrimary")
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

    //REG_WR32(LOG, LW_CFBFALCON_SYNC_STATUS, targetFreqMHz);
    LwU32 DebugHaltCode = REG_RD32(CSB,LW_CFBFALCON_SYNC_CTRL);

    //Adding Debug halt at start of mclk switch
    if (DebugHaltCode == 0xDEBD1E10) {
        FW_MBOX_WR32(13,targetFreqMHz);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT);
    }
    
    // For now set this up that we assume unit level sin when running & compiling in the //hw location
#ifdef FBFLCN_HW_RUNTIME_ELW
    unit_level_sim = 1;
#else
    unit_level_sim = 0;
#endif
    falcon_monitor = REG_RD32(BAR0, LW_PFB_FBPA_FALCON_MONITOR);
    //FB unit sim only
    tb_sig_g5x_cmd_mode = REF_VAL(1:1, falcon_monitor);
    tb_sig_dramclk_mode = REF_VAL(0:0, falcon_monitor);
    selwre_wr_scratch1 = 0;
    gbl_unit_sim_run = REF_VAL(0:0, selwre_wr_scratch1);

    // g_mcs definition:
    // LwU8  cmdType;
    // LwU32 targetFreqKHz;
    // LwU32 flags;
    //get_valid_partitions();

    gbl_target_freqMHz = targetFreqMHz;
    gbl_target_freqKHz = targetFreqMHz * 1000;

    fbfalconSelectTable_HAL(&Fbflcn, gbl_target_freqMHz);

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
    setPrevDramclkMode();
    if (gbl_target_freqMHz == 0 ) {
        FW_MBOX_WR32(10,targetFreqMHz);
        FW_MBOX_WR32(11,gbl_target_freqMHz);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT);
    }

    //get current frequency
    gddr_retrieve_lwrrent_freq();
    gddr_check_fbvdd_switch();
    gddr_check_vaux_change();
    FW_MBOX_WR32(12, gbl_target_freqMHz);
    FW_MBOX_WR32(14, GblSwitchCount);
    FW_MBOX_WR32(10, gbl_bFbVddSwitchNeeded);
    FW_MBOX_WR32(11, gbl_bVauxChangeNeeded);

    if (!gbl_bFbVddSwitchNeeded && !gbl_bVauxChangeNeeded) {

        fbStopTimeNs = gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        FW_MBOX_WR32(11, 0x0000DEAD);
        FW_MBOX_WR32(12, gbl_target_freqMHz);

    } else if (((gbl_bFbVddSwitchNeeded) && (gbl_fbvdd_low_to_high)) ||
            ((gbl_bVauxChangeNeeded) && (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_REFMPLL))) {
        gbl_target_freqMHz = gbl_lwrrent_freqMHz;

        fbfalconSelectTable_HAL(&Fbflcn,gbl_target_freqMHz);

        gddr_assign_local_table_pointer();

        //performing intermidiate p-state switch for changing fbvdd from low to high or
        //changing vauxc and re-calibrating
        FW_MBOX_WR32(12,gbl_target_freqMHz);
        fbStopTimeNs = gddr_mclk_switch(0, 0, 0); //VAUXC switch should go through the full mclk switch
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
        setPrevDramclkMode();

        //performing actual mclk switch post fbvdd change
        fbStopTimeNs = gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        //REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(1),LW_CEIL(fbStopTimeNs, 1000));
        FW_MBOX_WR32(11, 0x0000DEAD);
        FW_MBOX_WR32(12, gbl_target_freqMHz);
    } else if (((gbl_bFbVddSwitchNeeded) && (gbl_fbvdd_high_to_low)) ||
            ((gbl_bVauxChangeNeeded) && (gbl_prev_dramclk_mode == LW_PTRIM_SYS_FBIO_MODE_SWITCH_DRAMCLK_MODE_DRAMPLL))) {


        LwBool temp_fbvddswitchneeded = gbl_bFbVddSwitchNeeded;
        LwBool temp_vauxchangeneeded = gbl_bVauxChangeNeeded;
        gbl_bFbVddSwitchNeeded = LW_FALSE;
        gbl_bVauxChangeNeeded = LW_FALSE;

        //performing clock switch to low freq
        gddr_mclk_switch(bPreAddressBootTraining, bPostAddressBootTraining, bPostWckRdWrBootTraining);
        //REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(2),LW_CEIL(fbStopTimeNs, 1000));
        FW_MBOX_WR32(7, 0x0000DEAD);
        FW_MBOX_WR32(8, gbl_target_freqMHz);
        FW_MBOX_WR32(10, GblSwitchCount);

        gbl_bVauxChangeNeeded = temp_vauxchangeneeded;
        gbl_bFbVddSwitchNeeded = temp_fbvddswitchneeded;

        //FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT);
        //read current clock source
        setPrevDramclkMode();

        //performing fbvdd switch to lower the voltage or
        //changing vauxc and re-calibrating
        FW_MBOX_WR32(12, gbl_target_freqMHz);
        fbStopTimeNs = gddr_mclk_switch(0, 0, 0); //VAUXC switch should go through the full mclk switch
        //REG_WR32(CSB, LW_CFBFALCON_FALCON_COMMON_SCRATCH_GROUP_2(3),LW_CEIL(fbStopTimeNs, 1000));
        FW_MBOX_WR32(11, 0x0000DEAD);
        FW_MBOX_WR32(12, gbl_target_freqMHz);
    }
    REG_WR32(LOG, LW_CFBFALCON_SYNC_STATUS2, fbStopTimeNs);
    FW_MBOX_WR32(15, GblSwitchCount);

    //Adding Debug halt at end of mclk switch
    if ((REF_VAL(LW_CFBFALCON_SYNC_CTRL_ARG,DebugHaltCode) == 0xDEBD) &&
        (REF_VAL(LW_CFBFALCON_SYNC_CTRL_SEGMENT,DebugHaltCode) == 0x1E) &&
	 (REF_VAL(LW_CFBFALCON_SYNC_CTRL_CMD,DebugHaltCode) == 0xF0)) {
        FW_MBOX_WR32(12,fbStopTimeNs);
        FW_MBOX_WR32(13,targetFreqMHz);
        FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLK_SWITCH_DEBUG_HALT);
    }
    // Keeping for now for asatish
    //#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_PRIV_PROFILING))
    //    privprofilingDisable();
    //#endif

    return fbStopTimeNs;
    //return RM_FBFLCN_MCLK_SWITCH_MSG_STATUS_OK;
}
