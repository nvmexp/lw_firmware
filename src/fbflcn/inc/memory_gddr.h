/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef MEMORY_GDDR_H
#define MEMORY_GDDR_H

#if (FBFALCONCFG_CHIP_ENABLED(TU10X))
#define LW_PFB_FBPA_FBIO_SPARE_VALUE_FORCE_MA_SDR                         7:7
#endif


#include "osptimer.h"
#include <lwtypes.h>
#include "lwuproc.h"
#include "config/fbfalcon-config.h"
#include "fbflcn_table_headers.h"

LwU32 GblDdrMode;
LwBool GblVrefTrackPerformedOnce;
typedef struct REGBOX {
LwU32 pfb_fbpa_fbio_spare;
LwU32 ptrim_fbpa_bcast_dll_cfg;     // delete?
LwU32 pfb_fbpa_fbio_cfg1;
LwU32 pfb_fbpa_fbio_cfg8;
LwU32 pfb_fbpa_fbio_cfg10;
LwU32 pfb_fbpa_fbio_cfg12;
LwU32 pfb_fbpa_cfg0;
LwU32 pfb_fbpa_n_cfg0;
LwU32 pfb_fbpa_fbio_cfg_pwrd;
LwU32 pfb_fbpa_fbio_ddllcal_ctrl;
LwU32 pfb_fbpa_fbio_ddllcal_ctrl1;
LwU32 pfb_fbpa_fbio_config5;
LwU32 pfb_fbpa_training_cmd_1;
LwU32 pfb_fbpa_fbio_byte_pad_ctrl2;
LwU32 pfb_fbpa_fbio_adr_ddll;
LwU32 pfb_fbpa_training_patram;
LwU32 pfb_fbpa_training_ctrl;
LwU32 pfb_fbpa_ltc_bandwidth_limiter;
LwU32 pfb_fbpa_generic_mrs;
LwU32 pfb_fbpa_generic_mrs1;
LwU32 pfb_fbpa_generic_mrs2;
LwU32 pfb_fbpa_generic_mrs3;
LwU32 pfb_fbpa_generic_mrs4;
LwU32 pfb_fbpa_generic_mrs5;
LwU32 pfb_fbpa_generic_mrs6;
LwU32 pfb_fbpa_generic_mrs7;
LwU32 pfb_fbpa_generic_mrs8;
LwU32 pfb_fbpa_generic_mrs10;
LwU32 pfb_fbpa_generic_mrs14;
LwU32 pfb_fbpa_timing1;
LwU32 pfb_fbpa_timing10;
LwU32 pfb_fbpa_timing11;
LwU32 pfb_fbpa_timing12;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
LwU32 pfb_fbpa_timing13;
LwU32 pfb_fbpa_timing24;
#endif
LwU32 pfb_fbpa_config0;
LwU32 pfb_fbpa_config1;
LwU32 pfb_fbpa_config2;
LwU32 pfb_fbpa_fbio_broadcast;
LwU32 gpio_fbvddq;
LwU32 pfb_fbpa_training_cmd0;
LwU32 pfb_fbpa_training_cmd1;
LwU32 pfb_fbpa_refctrl;
LwU32 pfb_fbpa_ref;
LwU32 pfb_fbpa_ptr_reset;
LwU32 pfb_fbpa_fbio_bg_vtt;
LwU32 pfb_fbpa_fbio_edc_tracking;
LwU32 pfb_fbio_cal_wck_vml;
LwU32 pfb_fbio_cal_clk_vml;
LwU32 pfb_fbpa_refmpll_cfg;
LwU32 pfb_fbpa_refmpll_cfg2;
LwU32 pfb_fbpa_refmpll_coeff;
LwU32 pfb_fbpa_refmpll_ssd0;
LwU32 pfb_fbpa_refmpll_ssd1;
LwU32 pfb_fbpa_fbio_refmpll_config;
LwU32 pfb_fbpa_drampll_cfg;
LwU32 pfb_fbpa_drampll_coeff;
LwU32 pfb_fbpa_fbio_drampll_config;
LwU32 pfb_fbpa_fbio_cmos_clk;
LwU32 pfb_fbpa_fbio_cfg_vttgen_vauxgen;
LwU32 pfb_fbpa_fbio_cfg_brick_vauxgen;
LwU32 pfb_fbpa_fbio_cmd_delay;
LwU32 pfb_fbpa_fbiotrng_subp0_wck;
LwU32 pfb_fbpa_fbiotrng_subp0_wckb;
LwU32 pfb_fbpa_fbiotrng_subp1_wck;
LwU32 pfb_fbpa_fbiotrng_subp1_wckb;
LwU32 pfb_fbpa_clk_ctrl;
LwU32 pfb_fbpa_fbiotrng_subo0_cmd_brlshft1;
LwU32 pfb_fbpa_nop;
LwU32 pfb_fbpa_fbio_edc_tracking_debug;
LwU32 pfb_fbpa_fbio_delay;
LwU32 pfb_fbpa_testcmd;
LwU32 pfb_fbpa_testcmd_ext;
LwU32 pfb_fbpa_testcmd_ext_1;
LwU32 pfb_fbpa_fbio_pwr_ctrl;
LwU32 pfb_fbpa_fbio_brlshft;
LwU32 pfb_fbpa_fbio_vref_tracking;
LwU32 pfb_fbpa_training_edc_ctrl;
LwU32 pfb_fbpa_fbio_calgroup_vttgen_vauxgen;
LwU32 pfb_fbpa_fbio_vtt_ctrl0;
LwU32 pfb_fbpa_fbio_vtt_ctrl1;
LwU32 pfb_fbpa_training_rw_periodic_ctrl;
LwU32 pfb_fbpa_training_ctrl2;
LwU32 pfb_fbio_delay_broadcast_misc1;
LwU32 pfb_fbpa_fbiotrng_subp0byte0_vref_code3;
LwU32 pfb_fbpa_fbio_subp0byte0_vref_tracking1;
LwU32 pfb_fbio_calmaster_cfg2;
LwU32 pfb_fbio_calmaster_cfg3;
LwU32 pfb_fbio_calmaster_rb;
LwU32 pfb_fbio_cml_clk2;
LwU32 pfb_fbio_calmaster_cfg;
#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))
LwU32 pfb_fbio_misc_config;
LwU32 pfb_ptrim_fbio_mode_switch;
LwU32 pfb_ptrim_sys_fbio_cfg;
LwU32 pfb_fbio_ctrl_ser_priv;
LwU32 pfb_fbpa_training_rw_ldff_ctrl;
LwU32 pfb_fbpa_fbio_mddll_cntl;
LwU32 pfb_fbpa_dram_asr;
LwU32 pfb_fbpa_config12;
LwU32 pfb_fbpa_fbio_vtt_ctrl2;
LwU32 pfb_fbpa_fbio_edc_tracking1;
LwU32 pfb_fbpa_fbio_vref_tracking2;
LwU32 pfb_fbpa_fbio_intrpltr_offset;
LwU32 pfb_fbpa_sw_config;
LwU32 pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy0;
LwU32 pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy1;
LwU32 pfb_fbpa_fbiotrng_subp0byte0_vref_code3_perf_copy2;
LwU32 pfb_fbpa_fbiotrng_subp0byte0_vref_mid_code3;
LwU32 pfb_fbpa_fbiotrng_subp0byte0_vref_upper_code3;
LwU32 pfb_fbpa_fbiotrng_subp0_priv_cmd;
LwU32 pfb_fbpa_fbiotrng_subp1_priv_cmd;
LwU32 pfb_fbpa_fbio_wck_wclk_pad_ctrl;
#endif
} REGBOX;


extern REGBOX*  lwr_reg;
extern REGBOX*  new_reg;


void gddr_pgm_ma2sdr(LwU32 force_ma2sdr)
    GCC_ATTRIB_SECTION("memory", "gddr_pgm_ma2sdr");

void gddr_training_qpop_offset(LwU32 *pqpop_offset)
    GCC_ATTRIB_SECTION("memory", "gddr_training_qpop_offset");

void gddr_setup_addr_tr(void)
    GCC_ATTRIB_SECTION("memory", "gddr_setup_addr_tr");

void gddr_start_addr_training(void)
    GCC_ATTRIB_SECTION("memory", "gddr_start_addr_training");

void gddr_wait_for_training_done(void)
    GCC_ATTRIB_SECTION("memory", "gddr_wait_for_training_done");

void gddr_flush_mrs_reg(void)
    GCC_ATTRIB_SECTION("memory", "gddr_flush_mrs_reg");

LwBool gddr_check_training_passed(void)
    GCC_ATTRIB_SECTION("memory", "gddr_check_training_passed");

LwBool gddr_addr_training(LwU32 DdrMode)
    GCC_ATTRIB_SECTION("memory", "gddr_addr_training");

void gddr_setup_edc_tracking(LwBool,EdcTrackingGainTable *myEdcTrackingGainTable)
    GCC_ATTRIB_SECTION("memory", "gddr_setup_edc_tracking");

void gddr_start_edc_tracking(LwBool,LwU32,LwU32, EdcTrackingGainTable * myEdcTrackingGainTable,FLCN_TIMESTAMP)
    GCC_ATTRIB_SECTION("memory", "gddr_start_edc_tracking");

void func_fbio_intrpltr_offsets(LwU32, LwU32)
    GCC_ATTRIB_SECTION("memory", "func_fbio_intrpltr_offsets");

void func_track_vref(LwU32 return_vref[TOTAL_DBI_BITS])
    GCC_ATTRIB_SECTION("memory", "func_track_vref");

#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GA10X))

void func_save_vref_values(void)
    GCC_ATTRIB_SECTION("memory", "func_save_vref_values");

#endif

LwU32 unicast_rd(LwU32, LwU32);
LwU32 unicast_rd_offset(LwU32, LwU32, LwU32);
LwU32 unicast_wr_vref_dfe(LwU32, LwU32, LwU32,LwU32);
void gddr_pgm_trng_lower_subp(LwU32);


#endif //MEMORY_GDDR_H
