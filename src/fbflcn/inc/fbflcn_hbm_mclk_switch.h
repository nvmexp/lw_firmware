/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_HBM_MCLK_SWITCH_H
#define FBFLCN_HBM_MCLK_SWITCH_H


#include "fbflcn_defines.h"
#include <lwtypes.h>
#include "lwuproc.h"
#include "config/fbfalcon-config.h"

#define VBIOS_TABLE_SUPPORT

#ifndef VBIOS_TABLE_SUPPORT
#define LEGACY_STANDSIM_SUPPORT
#endif // ~VBIOS_TABLE_SUPPORT

typedef struct {
    LwU32 config0;
    LwU32 config1;
    LwU32 config2;
    LwU32 config3;
    LwU32 config4;
    LwU32 config5;
    LwU32 config6;
    LwU32 config7;
    LwU32 config8;
    LwU32 config9;
    LwU32 config10;
    LwU32 config11;  // TODO: this lwrrently is not part of the vbios bug has to be added bug #1874302
    LwU32 dram_acpd;
    LwU32 cfg0;
    LwU32 dram_asr;
    LwU32 hbm_cfg0;
    LwU32 hbm_ddllcal_ctrl1;
    LwU32 delay_broadcast_misc0;
    LwU32 delay_broadcast_misc1;
    //LwU32 mrs0;
    LwU32 mrs1;
    LwU32 mrs2;
    LwU32 mrs3;
    LwU32 mrs4;
//#if (FBFALCONCFG_FEATURE_ENABLED(FBFALCON_JOB_MCLK_HBM_GH100))
#if ((FBFALCONCFG_FEATURE_ENABLED(FBFALCONCORE_ADDITIONS_GH10X)) && (FBFALCONCFG_FEATURE_ENABLED(SUPPORT_HBM)))
    LwU32 mrs5;
#endif
    //LwU32 mrs15;
    LwU32 timing1;
    LwU32 timing10;
    LwU32 timing11;
    LwU32 timing12;
    LwU32 timing21;
    LwU32 timing22;
} REG_BOX;

// MRS register fields define
#define LW_PFB_FBPA_GENERIC_MRS_ADR_HBM_RDBI    0:0
#define LW_PFB_FBPA_GENERIC_MRS_ADR_HBM_WDBI    1:1
#define LW_PFB_FBPA_GENERIC_MRS1_ADR_HBM_DRV    7:5
#define LW_PFB_FBPA_GENERIC_MRS1_ADR_HBM_WR     4:0
#define LW_PFB_FBPA_GENERIC_MRS2_ADR_HBM_RL     7:3
#define LW_PFB_FBPA_GENERIC_MRS2_ADR_HBM_WL     2:0
#define LW_PFB_FBPA_GENERIC_MRS3_ADR_HBM_RAS    5:0
#define LW_PFB_FBPA_GENERIC_MRS5_HBM3_RTP       3:0
#define LW_PFB_FBPA_GENERIC_MRS15_ADR_HBM_IVREF 2:0

#define FBFLCN_SWITCH_MCLK_CHANGE 0
#define FBFLCN_SWTICH_TREFI_CHANGE 1

void get_timing_values_gv100 (void);
void disable_asr_acpd_gv100 (void);
void disable_refsb_gv100(void);

extern REG_BOX newRegValues;
extern REG_BOX lwrrentRegValues;

extern REG_BOX* lwr;
extern REG_BOX* new;

void set_acpd_off(void);
void precharge_all (void);
void enable_self_refresh (void);
void disable_self_refresh (void);
void enable_periodic_refresh (void);
void reset_read_fifo (void);

void set_acpd_value (void);


LwU32 doMclkSwitch(LwU16 targetFreqMHz);

//TODO: Fix this by creating HAL for this from
//memory.h and memory/turing/memory_gddr_tu10x.c
void wait_for_training_done(void);

#endif // FBFLCN_HBM_MCLK_SWITCH_H
