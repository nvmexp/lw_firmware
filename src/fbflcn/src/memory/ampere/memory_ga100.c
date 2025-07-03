/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

#include "fbflcn_helpers.h"
#include "fbflcn_defines.h"
#include "priv.h"

// include manuals
#include "dev_fbfalcon_csb.h"
#include "dev_fbpa.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pwr_pri.h"

#include "fbflcn_access.h"
#include "fbflcn_table.h"
#include "fbflcn_hbm_mclk_switch.h"

#include "osptimer.h"
#include "memory.h"
#include "config/g_memory_hal.h"

LwU32 gbl_mclk_period_new;

#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH        0:0
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_READY  0x1
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT   0x0

#define SEQ_VBLANK_TIMEOUT_NS ((45478) * 1000)


/*This is funtionally same as the GV100 functionality but had to be split out since we cant include memory_gv100 which has ptrim registers*/

GCC_ATTRIB_SECTION("memory", "memoryWaitForDisplayOkToSwitch_GA100")
void
memoryWaitForDisplayOkToSwitch_GA100
(
    void
)
{
     FLCN_TIMESTAMP  waitTimerNs = { 0 };
     osPTimerTimeNsLwrrentGet(&waitTimerNs);

     LwU32 mb0;
     LwU8 ok_to_switch = LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT;
     do {
         mb0 = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(0));
         ok_to_switch = REF_VAL(LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH,mb0);
     }
     while ((ok_to_switch != LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_READY) &&
            (osPTimerTimeNsElapsedGet(&waitTimerNs) < SEQ_VBLANK_TIMEOUT_NS) );
}


GCC_ATTRIB_SECTION("memory", "enable_dram_ack_ga100")
void
enable_dram_ack_ga100
(
    LwU8 enable
)
{

    lwr->cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACK, enable, lwr->cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, lwr->cfg0);
}

GCC_ATTRIB_SECTION("memory", "memoryGetMrsValueHbm_GA100")
void
memoryGetMrsValueHbm_GA100
(
        void
)
{
    // MRS values are not fully stored in the mem tweak tables.  The are in the
    // memory index table used to boot and will contain all the physical & static
    // settings (e.g 4 high vs 8 high).
    // mclk switches have to replace only partial subsets of data typicaly from
    // other registers from the tables

    // read the current values


    //lwr->mrs0 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS);
    lwr->mrs1 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS1);
    lwr->mrs2 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS2);
    lwr->mrs3 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS3);
    lwr->mrs4 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);
    //gbl_mrs4 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS4);  // full static values with no update required during mclk switch
    //lwr->mrs15 = REG_RD32(LOG, LW_PFB_FBPA_GENERIC_MRS15);

    // set new values
    //
    // MR0
    //   OP[7]: Test Mode                   static
    //   OP[6]: Add/CMD Parity              static
    //   OP[5]: DQ Write Parity             static
    //   OP[4]: DQ Read Parity              static
    //   OP[3]: rsvd
    //   OP[2]: TCSR                        static
    //   OP[1]: DBIac Write                 table
    //   OP[0]: DBIac Read                  table
    //
    // MR1
    //   OP[7:5] Driver Strength            table  <- mem tweak table: pmtbe375368.DrvStrength   THIS IS ONLY 2 BIT, NEED 3
    //   OP[4:0] Write Recovery [WR]        table  <- mem tweak table: config2.wr
    //
    // MR2
    //   OP[7:3] Read Latency [RL]          table  <- mem tweak table: config1.cl
    //   OP[2:0] Write Latency [WL]         table  <- mem tweak table: config1.wl
    //
    // MR3
    //   OP[7] BL                           static
    //   OP[6] Bank Group                   static
    //   OP[5:0] Active to Prechare [RAS]   table   <- mem tweak table: config0.ras
    //
    // MR4  lwrrently all static values, no update required during mclk switch
    //   OP[7:4] rsvd
    //   OP[3:2] Parity Latency [PL]        static
    //   OP[1]   DM                         static
    //   OP[0]   ECC                        static
    //
    // MR15
    //   OP[7:3] rsvd
    //   OP[2:0] Optional Internal Vref     table

    // MRS0
    //LwU32 mrs0 = lwr->mrs0;

    // dbi settings have turned into boot time options removing the need
    // to write/update MRS0
    // read dbi
    //LwU8 dbi_read = 1;     // FIXME:  needs to come from table
    //mrs0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS, _ADR_HBM_RDBI, dbi_read, mrs0);

    // write dbi
    //LwU8 dbi_write = 1;    // FIXME:  needs to come from table
    //mrs0 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS, _ADR_HBM_WDBI, dbi_write, mrs0);
    //new->mrs0 = mrs0;


    // MRS1
    LwU32 mrs1 = lwr->mrs1;

    // drive strength have turned into a boot time options,
    //LwU8 drv_strength = 0x7;  // FIXME: needs to come from table
    //mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_HBM_DRV, drv_strength, mrs1);

    // write recovery
    mrs1 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS1, _ADR_HBM_WR,
            REF_VAL(LW_PFB_FBPA_CONFIG2_WR, new->config2),    // get wr from config2
            mrs1);
    new->mrs1 = mrs1;


    // MRS2
    LwU32 mrs2 = lwr->mrs2;
    // read latency (cl) this should be programmed to CL-2
    mrs2 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS2, _ADR_HBM_RL,
            (REF_VAL(LW_PFB_FBPA_CONFIG1_CL,new->config1) - 2),    // get CL from config1
            mrs2);

    // write latency (wl), this should be programmed to WL-1
    mrs2 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS2, _ADR_HBM_WL,
            (REF_VAL(LW_PFB_FBPA_CONFIG1_WL,new->config1) - 1),    // get WL from config1
            mrs2);
    new->mrs2=mrs2;

    // MRS3
    LwU32 mrs3 = lwr->mrs3;
    // active to precharge (ras)
    mrs3 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS3, _ADR_HBM_RAS,
            REF_VAL(LW_PFB_FBPA_CONFIG0_RAS,new->config0),   // get ras from config0
            mrs3);
    new->mrs3 = mrs3;

    // MRS4
    LwU32 mrs4 = lwr->mrs4;
    // mrs 4 need to be programmed based on read latency (cl) for HBM 2E
    if(REF_VAL(LW_PFB_FBPA_CONFIG1_CL,new->config1) > 33)
    {
          mrs4 |= (0x1 << 5);     // set bit 5
    }
    else
    {
          mrs4 &= ~((LwU32)0x1 << 5);     // clear bit 5
    }
    // mrs 4 need to be programmed based on write latency (wl) for HBM 2E
    if(REF_VAL(LW_PFB_FBPA_CONFIG1_WL,new->config1) > 8)
    {
          mrs4 |= (0x1 << 4);     // set bit 4
    }
    else
    {
          mrs4 &= ~((LwU32)0x1 << 4);     // clear bit 4
    }
    new->mrs4=mrs4;

    // MSR15
    //LwU32 mrs15 = lwr->mrs15;

    // internal vref has turned into a boot time options, removing the need
    // to update mrs15 during an mclk switch/
    // optional internal vref
    //LwU8 ivref = 7;  // FIXME: this needs to come from the table
    //mrs15 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS15, _ADR_HBM_IVREF, ivref, mrs15);
    //new->mrs15 = mrs15;
}

GCC_ATTRIB_SECTION("memory", "memoryEnableSelfRefreshHbm_GA100")
void
memoryEnableSelfRefreshHbm_GA100
(
    void
)
{
    LwU32 refctrl_valid;
    LwU32 selfref_enabled;

    refctrl_valid = REG_RD32(LOG, LW_PFB_FBPA_REFCTRL);
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _VALID, _0, refctrl_valid);


    // Clear PUT and GET fields every time REFCTRL is written - this avoids a corner
    // case where GET field may advance past PUT field while reading REFCTRL
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _PUT, _0, refctrl_valid);
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _GET, _0, refctrl_valid);

     REG_WR32_STALL(LOG, LW_PFB_FBPA_REFCTRL, refctrl_valid);

    // Force a couple of s/w refreshes first
     REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);
    // Clear PUT and GET fields every time REFCTRL is written - this avoids a corner


    // Enable DRAM_ACK
    enable_dram_ack_ga100(1);

    // Force a couple of s/w refreshes first
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);

    selfref_enabled = REG_RD32(LOG, LW_PFB_FBPA_SELF_REF);
    selfref_enabled = FLD_SET_DRF(_PFB, _FBPA_SELF_REF, _CMD, _ENABLED, selfref_enabled);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_SELF_REF, selfref_enabled);

    // Disable DRAM_ACK
    enable_dram_ack_ga100(0);

}


GCC_ATTRIB_SECTION("memory", "memorySetDramClkBypassHbm_GA100")
void
memorySetDramClkBypassHbm_GA100
(
    void
)
{
    LwU32 cmd_force_byp_pll;


    cmd_force_byp_pll = REG_RD32(LOG, unicastMasterReadFromBroadcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG));
    cmd_force_byp_pll = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBMPLL_CFG, _BYPASSPLL, 0x1, cmd_force_byp_pll);

    REG_WR32_STALL(LOG, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG), cmd_force_byp_pll);

}


GCC_ATTRIB_SECTION("memory", "memoryStopClkenPadmacroHbm_GA100")
void
memoryStopClkenPadmacroHbm_GA100
(
    void
)
{
    LwU32 cmd_stop_clken_pm;

    cmd_stop_clken_pm = REG_RD32(LOG, unicastMasterReadFromBroadcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG));
    cmd_stop_clken_pm = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBMPLL_CFG, _CLK_EN_PADMACRO, 0x0, cmd_stop_clken_pm);

    REG_WR32_STALL(LOG, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG), cmd_stop_clken_pm);


}


GCC_ATTRIB_SECTION("memory", "memorySwitchPllHbm_GA100")
void
memorySwitchPllHbm_GA100
(
    void
)
{
    // LwU32 ack_switch_pll;
    LwU32 cmd_switch_pll;

    cmd_switch_pll = REG_RD32(LOG, unicastMasterReadFromBroadcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG));
    cmd_switch_pll = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBMPLL_CFG, _SYNCMODE, 0x1, cmd_switch_pll);
    cmd_switch_pll = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBMPLL_CFG, _BYPASSPLL, 0x0, cmd_switch_pll);

    REG_WR32_STALL(LOG,  multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG), cmd_switch_pll);


}


GCC_ATTRIB_SECTION("memory", "memoryStartClkenPadmacroHbm_GA100")
void
memoryStartClkenPadmacroHbm_GA100
(
    void
)
{

    LwU32 cmd_start_clken_pm;


    cmd_start_clken_pm = REG_RD32(LOG, unicastMasterReadFromBroadcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG));
    cmd_start_clken_pm = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_HBMPLL_CFG , _CLK_EN_PADMACRO, 0x1, cmd_start_clken_pm);

    REG_WR32_STALL(LOG, multicastMasterWriteFromBrodcastRegister(LW_PFB_FBPA_FBIO_HBMPLL_CFG), cmd_start_clken_pm);

}


GCC_ATTRIB_SECTION("memory", "memoryDisableSelfrefreshHbm_GA100")
void
memoryDisableSelfrefreshHbm_GA100
(
    void
)
{
    LwU32 selfref_disabled;

    // Enable DRAM_ACK
    // dram_ack = REG_RD32(LOG, LW_PFB_FBPA_CFG0);
    // dram_ack = FLD_SET_DRF (_PFB,FBPA_CFG0,_DRAM_ACK,_ENABLED,dram_ack);

    // REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack);

    // Send SRX command (2 words) to DRAM via TESTCMD reg
    // TESTCMD drives DRAM i/f pins directly

    selfref_disabled = REG_RD32(LOG, LW_PFB_FBPA_SELF_REF);
    selfref_disabled = FLD_SET_DRF(_PFB, _FBPA_SELF_REF, _CMD, _DISABLED, selfref_disabled);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_SELF_REF, selfref_disabled);

   }


GCC_ATTRIB_SECTION("memory", "memorySetMrsValuesHbm_GA100")
void
memorySetMrsValuesHbm_GA100
(
    void
)
{

    // Enable DRAM_ACK
    enable_dram_ack_ga100(1);

    //REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS, new->mrs0);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS1, new->mrs1);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS2, new->mrs2);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS3, new->mrs3);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS4, new->mrs4);
    //REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS15, new->mrs15);

    // Disable DRAM_ACK
}


GCC_ATTRIB_SECTION("memory", "memorySetTrainingControlPrbsMode_GA100")
void
memorySetTrainingControlPrbsMode_GA100
(
    LwU8 prbsMode
)
{
    LwU32 fbpaTrainingArrayCtrl;
    fbpaTrainingArrayCtrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_ARRAY_CTRL);
    fbpaTrainingArrayCtrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_ARRAY_CTRL, _PRBS_MODE, prbsMode, fbpaTrainingArrayCtrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ARRAY_CTRL,  fbpaTrainingArrayCtrl);
}


GCC_ATTRIB_SECTION("memory", "memorySetTrainingControlGddr5Commands_GA100")
void
memorySetTrainingControlGddr5Commands_GA100
(
    LwU8 gddr5Command
)
{
    LwU32 fbpaTrainingArrayCtrl;
    fbpaTrainingArrayCtrl= REG_RD32(BAR0, LW_PFB_FBPA_TRAINING_ARRAY_CTRL);
    fbpaTrainingArrayCtrl = FLD_SET_DRF_NUM(_PFB, _FBPA_TRAINING_ARRAY_CTRL, _GDDR5_COMMANDS, gddr5Command, fbpaTrainingArrayCtrl);
    REG_WR32(LOG, LW_PFB_FBPA_TRAINING_ARRAY_CTRL,  fbpaTrainingArrayCtrl);
}

