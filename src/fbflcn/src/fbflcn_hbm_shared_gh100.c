/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <falcon-intrinsics.h>
#include "fbflcn_defines.h"
#include "fbflcn_helpers.h"
#if (!FBFALCONCFG_FEATURE_ENABLED(SUPPORT_NITROC))
#include "priv.h"
#include "dev_fbpa.h"
#endif

void
save_fbio_hbm_delay_broadcast_misc
(
    LwU32 *misc0,
    LwU32 *misc1
)
{
    LwU32 reg_data;

    //The broadcast registers are write only so we need to read an individual channel/dword register
    //to get the settings. It is safe to assume that before training they are all identical
    LwU32 broadcast_misc0 = 0;
    LwU32 broadcast_misc1 = 0;
    reg_data = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_TRIM,                REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL_TRIM, reg_data),                       broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_DDLL_BYPASS,         REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL_BYPASS, reg_data),                     broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_DDLL_BYPASS_CLK_SEL, REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL_BYPASS_CLK_SEL, reg_data),             broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_DDLL_CLK_SEL,        REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL_CLK_SEL, reg_data),                    broadcast_misc0);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _TX_DDLL_USE_CLK_SEL,    REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_TX_DDLL_USE_CLK_SEL, reg_data),                broadcast_misc1);
                                                                                                                                                                                                       
    reg_data = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_WL_DDLL);                                                                                                                           
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_TRIM,                REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_WL_DDLL_TRIM, reg_data),                       broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_DDLL_BYPASS,         REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_WL_DDLL_BYPASS, reg_data),                     broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_DDLL_BYPASS_CLK_SEL, REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_WL_DDLL_BYPASS_CLK_SEL, reg_data),             broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_DDLL_CLK_SEL,        REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_WL_DDLL_CLK_SEL, reg_data),                    broadcast_misc0);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _WL_DDLL_USE_CLK_SEL,    REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_WL_DDLL_USE_CLK_SEL, reg_data),                broadcast_misc1);
                                                                                                                                                                                                       
    //reg_data = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_DDLLCAL);                                                                                                                           
    //broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _DDLLCAL_PWRD,           REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_DDLLCAL_PWRD, reg_data),                       broadcast_misc0);
    //broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _DDLLCAL_PWRD,           REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_DDLLCAL_PWRD, reg_data),                       broadcast_misc1);
                                                                                                                                                                                                       
    reg_data = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL);                                                                                                                           
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_TRIM,                REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL_TRIM, reg_data),                       broadcast_misc1);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_DDLL_BYPASS,         REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL_BYPASS, reg_data),                     broadcast_misc1);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_DDLL_BYPASS_CLK_SEL, REF_VAL(LW_PFB_FBPA_FBIO_HBM_CHANNEL0_DWORD0_RX_DDLL_BYPASS_CLK_SEL, reg_data),             broadcast_misc1);
                                                                                                                                                                                                       
    reg_data = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CFG2_CHANNEL0_DWORD0_BRICK);                                                                                                                        
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_RCVR_SEL,          REF_VAL(LW_PFB_FBPA_FBIO_HBM_CFG2_CHANNEL0_DWORD0_BRICK_RDQS_RCVR_SEL, reg_data),           broadcast_misc1);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_E_DIFF_MODE,       REF_VAL(LW_PFB_FBPA_FBIO_HBM_CFG2_CHANNEL0_DWORD0_BRICK_RDQS_E_DIFF_MODE, reg_data),        broadcast_misc1);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _DQ_RCVR_SEL,            REF_VAL(LW_PFB_FBPA_FBIO_HBM_CFG2_CHANNEL0_DWORD0_BRICK_DQ_RCVR_SEL, reg_data),             broadcast_misc1);
                                                                                                                                                                                                       
    reg_data = REG_RD32(BAR0, LW_PFB_FBPA_FBIO_HBM_CFG1_CHANNEL0_DWORD0_PWRD2);                                                                                                                                        
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_VREF_PWRD,           REF_VAL(LW_PFB_FBPA_FBIO_HBM_CFG1_CHANNEL0_DWORD0_PWRD2_BRICK_RX_VREF_PWRD, reg_data),      broadcast_misc1);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_BIAS_PWRD,           REF_VAL(LW_PFB_FBPA_FBIO_HBM_CFG1_CHANNEL0_DWORD0_PWRD2_BRICK_RX_BIAS_PWRD, reg_data),      broadcast_misc1);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_RX_BIAS_PWRD,      REF_VAL(LW_PFB_FBPA_FBIO_HBM_CFG1_CHANNEL0_DWORD0_PWRD2_BRICK_RDQS_RX_BIAS_PWRD, reg_data), broadcast_misc1);

    //Save the built up values
    //saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc0 = broadcast_misc0;
    //saved_reg->pfb_fbpa_fbio_hbm_delay_broadcast_misc1 = broadcast_misc1;
    *misc0 = broadcast_misc0;
    *misc1 = broadcast_misc1;
}
