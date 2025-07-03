/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
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
#include "dev_trim.h"
#include "dev_pri_ringstation_fbp.h"
#include "dev_pwr_pri.h"
#include "fbflcn_hbm_mclk_switch.h"

#include "osptimer.h"
#include "memory.h"
#include "config/g_memory_hal.h"


#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH        0:0
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_READY  0x1
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH_WAIT   0x0


#define SEQ_VBLANK_TIMEOUT_NS ((45478) * 1000)

void
memoryWaitForDisplayOkToSwitch_GV100
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


/*  stop_fb_through_pmu
 *
 * this is a workaround to access the LW_ACCESS/LW_PFIFO registers in the host register space that are used to shutdown
 * host traffic. As the fbfalcon is not able to reach the host register the stop_fb sequence is initiated by sending
 * an interrupt to the pmu falcon which in turn does the host priv register access.
 * 1.   In icPreInit on PMU, set up the _CTXSW and _MTHD interrupts to interrupt the PMU:
 *   a. Write to IRQDEST to route both interrupts to falcon at IV0 (_HOST_MTHD and _HOST_CTXSW to _FALCON and _TARGET_MTHD and _TARGET_MTHD to _IRQ0)
 *   b. Write to IRQMSET to unmask both interrupts (_CTXSW_SET and _MTHD_SET)
 * 2.   RM sends a command to PMU to switch mclk
 * 3.   PMU programs necessary display registers if applicable
 * 4.   PMU sends a command to FBfalcon to initiate mclk switch
 * 5.   FBFalcon does the mclk switch sequence till it needs to engage fb_stop
 * 6.   In the stop_fb() function, FB falcon sends an interrupt to the PMU and starts polling on FB_STOP_ACK to be asserted:
 *          https://rmopengrok.lwpu.com/source/xref/chips_a/uproc/fbflcn/src/fbflcn_hbm_mclk_switch.c#636
 *   a.  Write LW_PPWR_FALCON_IRQSSET_CTXSW bit to _SET (leave all other bits at 0). This register is written by multiple entities, but doesnt cause a race condition since it is write 1 to set for every bit
 *   b.  Start polling on FB_STOP_ACK
 * 7.   PMU handles the _CTXSW interrupt and in the ISR itself, it exelwtes the entire sequence in the existing function fbStop_HAL() except the final poll that waits for the ack.
 * 8.   This causes FB falcon to come out of its polling from step 5b.
 * 9.   Fb falcon does the critical portion of mclk switch.
 * 10.  In the start_fb() function, FB falcon again interrupts the PMU and starts polling on LW_CFBFALCON_FIRMWARE_MAILBOX(1) to read the value LW_CFBFALCON_FIRMWARE_MAILBOX_VALUE_MAGIC
 *       https://rmopengrok.lwpu.com/source/xref/chips_a/uproc/fbflcn/src/fbflcn_hbm_mclk_switch.c#2699
 *   a. Write LW_PPWR_FALCON_IRQSSET_MTHD bit to _SET (leave all other bits at 0). This register is written by multiple entities, but doest cause a race condition since it is write 1 to set for every bit
 *   b. Start polling on LW_CFBFALCON_FIRMWARE_MAILBOX(1) to read back _VALUE_MAGIC
 * 11.  PMU handles the _MTHD interrupt in the ISR and exelwtes everything in the function fbStart_HAL(), and then writes LW_CFBFALCON_FIRMWARE_MAILBOX(1) to _VALUE_MAGIC
 * 12.  This causes the FB falcon to come out of polling from step 9b and before it exits the start_fb() function, it clears the LW_CFBFALCON_FIRMWARE_MAILBOX(1) register to 0
 * 13.  FB falcon sends completion request back to PMU
 * 14.  PMU restores any display programming as necessary
 * 15.  PMU acks back to RM that mclk switch is done
 *
 */

void
memoryStopFb_GV100
(
    LwU8            waitForDisplay,
    PFLCN_TIMESTAMP pStartFbStopTimeNs
)
{
    if (waitForDisplay)
    {
        memoryWaitForDisplayOkToSwitch_HAL();
    }

    // step 6a) send interrupt to pmu

    // It is not necessary to read-modify-write IRQSSET (zeros are ignored)
    REG_WR32(LOG, LW_PPWR_FALCON_IRQSSET, DRF_DEF(_PPWR, _FALCON_IRQSSET, _CTXSW, _SET));

    // step 6b) poll for stop ack
    LwU32 ack_stop_fb = 0;
    while (ack_stop_fb != LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK_TRUE)
    {
        ack_stop_fb = REG_RD32(LOG, LW_PPWR_PMU_GPIO_INPUT);
        ack_stop_fb = REF_VAL(LW_PPWR_PMU_GPIO_INPUT_FB_STOP_ACK, ack_stop_fb);
    }

    // Record the time immediately after the FB stop is asserted
    osPTimerTimeNsLwrrentGet(pStartFbStopTimeNs);
}


LwU32
memoryStartFb_GV100
(
    PFLCN_TIMESTAMP pStartFbStopTimeNs
)
{
    LwU32 ack_start_fb;
    LwU32        fbStopTimeNs = 0;

    //
    // Clear the mailbox register to ensure that we are safe even if some other
    // code unknowingly happens to set the LW_CFBFALCON_FIRMWARE_MAILBOX(1)
    // register to the magic value after the last time we de reset. Although,
    // our code relies on the assumption that this should not be acceptable
    // it does not harm to be prepared for that scenario too.
    //
    FW_MBOX_WR32(1, 
                    LW_CFBFALCON_FIRMWARE_MAILBOX_VALUE_NONE);

    // step 10a) send interrupt to pmu for deasserting FB stop
    // It is not necessary to read-modify-write IRQSSET (zeros are ignored)
    REG_WR32(LOG, LW_PPWR_FALCON_IRQSSET, DRF_DEF(_PPWR, _FALCON_IRQSSET, _MTHD, _SET));

    // Send a s/w refresh as a workaround for clk gating bug
    // This should keep the clocks running
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);

    ack_start_fb = 0;
    while (ack_start_fb != LW_CFBFALCON_FIRMWARE_MAILBOX_VALUE_MAGIC)
    {
        ack_start_fb = REG_RD32(CSB, LW_CFBFALCON_FIRMWARE_MAILBOX(1));
    }

    // Find out the time elapsed for which FB stop was asserted
    fbStopTimeNs = osPTimerTimeNsElapsedGet(pStartFbStopTimeNs);

    //
    // Although it's not necessary to clear the mailbox register here since
    // we're doing that before triggering the interrupt earlier in this
    // function, adding it here just to follow the good practice of clearing it
    // after every use.
    //
    FW_MBOX_WR32(1, 
                    LW_CFBFALCON_FIRMWARE_MAILBOX_VALUE_NONE);

    return fbStopTimeNs;
}


GCC_ATTRIB_SECTION("memory", "enable_dram_ack_gv100")
void
enable_dram_ack_gv100
(
    LwU8 enable
)
{

    lwr->cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACK, enable, lwr->cfg0);
	REG_WR32(LOG, LW_PFB_FBPA_CFG0, lwr->cfg0);
}


GCC_ATTRIB_SECTION("memory", "memoryGetMrsValueHbm_GV100")
void
memoryGetMrsValueHbm_GV100
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

    // MSR15
    //LwU32 mrs15 = lwr->mrs15;

    // internal vref has turned into a boot time options, removing the need
    // to update mrs15 during an mclk switch/
    // optional internal vref
    //LwU8 ivref = 7;  // FIXME: this needs to come from the table
    //mrs15 = FLD_SET_DRF_NUM(_PFB, _FBPA_GENERIC_MRS15, _ADR_HBM_IVREF, ivref, mrs15);
    //new->mrs15 = mrs15;
}


GCC_ATTRIB_SECTION("memory", "memoryEnableSelfRefreshHbm_GV100")
void
memoryEnableSelfRefreshHbm_GV100
(
	void
)
{
    LwU32 refctrl_valid;
    LwU32 test_cmd;

    LwU32 test_cmd_act;


    // Enable DRAM_ACK
    enable_dram_ack_gv100(1);

    // Force a couple of s/w refreshes first
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);

    refctrl_valid = REG_RD32(LOG, LW_PFB_FBPA_REFCTRL);
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _VALID, _0, refctrl_valid);

    // Clear PUT and GET fields every time REFCTRL is written - this avoids a corner
    // case where GET field may advance past PUT field while reading REFCTRL
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _PUT, _0, refctrl_valid);
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _GET, _0, refctrl_valid);



    REG_WR32_STALL(LOG, LW_PFB_FBPA_REFCTRL, refctrl_valid);

    // Disable DRAM_ACK
    enable_dram_ack_gv100(0);

   //To maintain NOP with parity enabled, make an additional PRIV write to clear  LW_PFB_FBPA_TESTCMD_EXT_ACT before programming LW_PFB_FBPA_TESTCMD register

    test_cmd_act = REG_RD32(LOG, LW_PFB_FBPA_TESTCMD_EXT);

    test_cmd_act = FLD_SET_DRF(_PFB, _FBPA_TESTCMD_EXT, _ACT, _HBM_SELF_REF_ENTRY1, test_cmd_act);

    REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD_EXT, test_cmd_act);

    // Send SRE command (3 words) to DRAM via TESTCMD reg
    // TESTCMD drives DRAM i/f pins directly
    test_cmd = REF_NUM(LW_PFB_FBPA_TESTCMD_CKE,  LW_PFB_FBPA_TESTCMD_CKE_HBM_SELF_REF_ENTRY1)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CS0,  LW_PFB_FBPA_TESTCMD_CS0_HBM_SELF_REF_ENTRY1)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CS1,  LW_PFB_FBPA_TESTCMD_CS1_HBM_SELF_REF_ENTRY1)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_RAS,  LW_PFB_FBPA_TESTCMD_RAS_HBM_SELF_REF_ENTRY1)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CAS,  LW_PFB_FBPA_TESTCMD_CAS_HBM_SELF_REF_ENTRY1)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_WE,   LW_PFB_FBPA_TESTCMD_WE_HBM_SELF_REF_ENTRY1)   |
               REF_NUM(LW_PFB_FBPA_TESTCMD_RES,  LW_PFB_FBPA_TESTCMD_RES_HBM_SELF_REF_ENTRY1)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_BANK, LW_PFB_FBPA_TESTCMD_BANK_HBM_SELF_REF_ENTRY1) |
               REF_NUM(LW_PFB_FBPA_TESTCMD_ADR,  LW_PFB_FBPA_TESTCMD_ADR_HBM_SELF_REF_ENTRY1)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_HOLD, LW_PFB_FBPA_TESTCMD_HOLD_HBM_SELF_REF_ENTRY1) |
               REF_NUM(LW_PFB_FBPA_TESTCMD_GO,   LW_PFB_FBPA_TESTCMD_GO_HBM_SELF_REF_ENTRY1)   ;

    REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD, test_cmd);

    test_cmd = REF_NUM(LW_PFB_FBPA_TESTCMD_CKE,  LW_PFB_FBPA_TESTCMD_CKE_HBM_SELF_REF_ENTRY2)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CS0,  LW_PFB_FBPA_TESTCMD_CS0_HBM_SELF_REF_ENTRY2)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CS1,  LW_PFB_FBPA_TESTCMD_CS1_HBM_SELF_REF_ENTRY2)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_RAS,  LW_PFB_FBPA_TESTCMD_RAS_HBM_SELF_REF_ENTRY2)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CAS,  LW_PFB_FBPA_TESTCMD_CAS_HBM_SELF_REF_ENTRY2)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_WE,   LW_PFB_FBPA_TESTCMD_WE_HBM_SELF_REF_ENTRY2)   |
               REF_NUM(LW_PFB_FBPA_TESTCMD_RES,  LW_PFB_FBPA_TESTCMD_RES_HBM_SELF_REF_ENTRY2)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_BANK, LW_PFB_FBPA_TESTCMD_BANK_HBM_SELF_REF_ENTRY2) |
               REF_NUM(LW_PFB_FBPA_TESTCMD_ADR,  LW_PFB_FBPA_TESTCMD_ADR_HBM_SELF_REF_ENTRY2)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_HOLD, LW_PFB_FBPA_TESTCMD_HOLD_HBM_SELF_REF_ENTRY2) |
               REF_NUM(LW_PFB_FBPA_TESTCMD_GO,   LW_PFB_FBPA_TESTCMD_GO_HBM_SELF_REF_ENTRY2)   ;


    REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD, test_cmd);

    test_cmd = REF_NUM(LW_PFB_FBPA_TESTCMD_CKE,  LW_PFB_FBPA_TESTCMD_CKE_HBM_SELF_REF_ENTRY3)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CS0,  LW_PFB_FBPA_TESTCMD_CS0_HBM_SELF_REF_ENTRY3)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CS1,  LW_PFB_FBPA_TESTCMD_CS1_HBM_SELF_REF_ENTRY3)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_RAS,  LW_PFB_FBPA_TESTCMD_RAS_HBM_SELF_REF_ENTRY3)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_CAS,  LW_PFB_FBPA_TESTCMD_CAS_HBM_SELF_REF_ENTRY3)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_WE,   LW_PFB_FBPA_TESTCMD_WE_HBM_SELF_REF_ENTRY3)   |
               REF_NUM(LW_PFB_FBPA_TESTCMD_RES,  LW_PFB_FBPA_TESTCMD_RES_HBM_SELF_REF_ENTRY3)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_BANK, LW_PFB_FBPA_TESTCMD_BANK_HBM_SELF_REF_ENTRY3) |
               REF_NUM(LW_PFB_FBPA_TESTCMD_ADR,  LW_PFB_FBPA_TESTCMD_ADR_HBM_SELF_REF_ENTRY3)  |
               REF_NUM(LW_PFB_FBPA_TESTCMD_HOLD, LW_PFB_FBPA_TESTCMD_HOLD_HBM_SELF_REF_ENTRY3) |
               REF_NUM(LW_PFB_FBPA_TESTCMD_GO,   LW_PFB_FBPA_TESTCMD_GO_HBM_SELF_REF_ENTRY3)   ;

    REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD, test_cmd);

}


GCC_ATTRIB_SECTION("memory", "memorySetDramClkBypassHbm_GV100")
void
memorySetDramClkBypassHbm_GV100
(
    void
)
{

    LwU32 cmd_force_byp_pll;


    cmd_force_byp_pll = REG_RD32(LOG, LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0);
    cmd_force_byp_pll = FLD_SET_DRF_NUM(_PTRIM, _FBPA_BCAST_HBMPLL_CFG0, _BYPASSPLL, 0x1, cmd_force_byp_pll);

    REG_WR32_STALL(LOG, LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0, cmd_force_byp_pll);

}


GCC_ATTRIB_SECTION("memory", "memoryStopClkenPadmacroHbm_GV100")
void
memoryStopClkenPadmacroHbm_GV100
(
    void
)
{
    LwU32 cmd_stop_clken_pm;

    cmd_stop_clken_pm = REG_RD32(LOG, LW_PTRIM_FBPA_BCAST_HBMPAD_CFG0);
    cmd_stop_clken_pm = FLD_SET_DRF_NUM(_PTRIM, _FBPA_BCAST_HBMPAD_CFG0, _CLK_EN_PADMACRO, 0x0, cmd_stop_clken_pm);

    REG_WR32_STALL(LOG, LW_PTRIM_FBPA_BCAST_HBMPAD_CFG0, cmd_stop_clken_pm);

}


GCC_ATTRIB_SECTION("memory", "memorySwitchPllHbm_GV100")
void
memorySwitchPllHbm_GV100
(
    void
)
{
    // LwU32 ack_switch_pll;
    LwU32 cmd_switch_pll;


    cmd_switch_pll = REG_RD32(LOG, LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0);
    cmd_switch_pll = FLD_SET_DRF_NUM(_PTRIM, _FBPA_BCAST_HBMPLL_CFG0, _SYNCMODE, 0x1, cmd_switch_pll);
    cmd_switch_pll = FLD_SET_DRF_NUM(_PTRIM, _FBPA_BCAST_HBMPLL_CFG0, _BYPASSPLL, 0x0, cmd_switch_pll);

    REG_WR32_STALL(LOG, LW_PTRIM_FBPA_BCAST_HBMPLL_CFG0, cmd_switch_pll);

}


GCC_ATTRIB_SECTION("memory", "memoryStartClkenPadmacroHbm_GV100")
void
memoryStartClkenPadmacroHbm_GV100
(
    void
)
{

    LwU32 cmd_start_clken_pm;

    cmd_start_clken_pm = REG_RD32(LOG, LW_PTRIM_FBPA_BCAST_HBMPAD_CFG0);
    cmd_start_clken_pm = FLD_SET_DRF_NUM(_PTRIM, _FBPA_BCAST_HBMPAD_CFG0, _CLK_EN_PADMACRO, 0x1, cmd_start_clken_pm);

    REG_WR32_STALL(LOG, LW_PTRIM_FBPA_BCAST_HBMPAD_CFG0, cmd_start_clken_pm);

}


GCC_ATTRIB_SECTION("memory", "memoryDisableSelfrefreshHbm_GV100")
void
memoryDisableSelfrefreshHbm_GV100
(
    void
)
{

    LwU32 test_cmd;

    // Enable DRAM_ACK
    // dram_ack = REG_RD32(LOG, LW_PFB_FBPA_CFG0);
    // dram_ack = bit_assign (dram_ack, LW_PFB_FBPA_CFG0_DRAM_ACK_BIT, 0x01, 1);

    // REG_WR32(LOG, LW_PFB_FBPA_CFG0, dram_ack);

    // Send SRX command (2 words) to DRAM via TESTCMD reg
    // TESTCMD drives DRAM i/f pins directly

    test_cmd = 0x00000000;

    test_cmd = FLD_SET_DRF_NUM (_PFB, _FBPA_TESTCMD, _CKE, 0x01, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _CS0,  _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _CS1,  _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _RAS,  _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _CAS,  _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _WE,   _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _RES,  _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _BANK, _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _ADR,  _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _HOLD, _HBM_SELF_REF_EXIT1, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _GO,   _HBM_SELF_REF_EXIT1, test_cmd);

    REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD, test_cmd);

    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _CKE,  _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _CS0,  _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _CS1,  _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _RAS,  _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _CAS,  _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _WE,   _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _RES,  _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _BANK, _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _ADR,  _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _HOLD, _HBM_SELF_REF_EXIT2, test_cmd);
    test_cmd = FLD_SET_DRF (_PFB, _FBPA_TESTCMD, _GO,   _HBM_SELF_REF_EXIT2, test_cmd);

    REG_WR32_STALL(LOG, LW_PFB_FBPA_TESTCMD, test_cmd);

}


GCC_ATTRIB_SECTION("memory", "memorySetMrsValuesHbm_GV100")
void
memorySetMrsValuesHbm_GV100
(
	 void
)
{

	// Enable DRAM_ACK
	enable_dram_ack_gv100(1);

    //REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS, new->mrs0);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS1, new->mrs1);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS2, new->mrs2);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS3, new->mrs3);
    //REG_WR32_STALL(LOG, LW_PFB_FBPA_GENERIC_MRS15, new->mrs15);

    // Disable DRAM_ACK
}


//
// getGPIOIndexForFBVDDQ
//   find the GPIO index for the FB VDDQ GPIO control. This index is can be read from the
//   PMCT Header as a copy of the gpio tables.
//
LwU8
memoryGetGpioIndexForFbvddq_GV100
(
        void
)
{
    return 8;
}

//
// getGPIOIndexForVref
//   find the GPIO index for the VREF GPIO control. This index is can be read from the
//   PMCT Header as a copy of the gpio tables.
//
LwU8
memoryGetGpioIndexForVref_GV100
(
        void
)
{
    return 10;
}

