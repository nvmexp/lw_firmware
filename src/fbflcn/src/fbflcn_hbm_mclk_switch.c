/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <falcon-intrinsics.h>

// include manuals
#include "dev_fbpa.h"
#include "dev_trim.h"

#include "rmfbflcncmdif.h"

#include "fbflcn_hbm_mclk_switch.h"
#include "fbflcn_defines.h"
#include "priv.h"
#include "fbflcn_helpers.h"
#include "fbflcn_table.h"
#include "fbflcn_access.h"
#include "fbflcn_timer.h"

#include "dev_fbfalcon_csb.h"
#include "dev_pri_ringstation_fbp.h"

#include "osptimer.h"
#include "memory.h"
#include "fbflcn_objfbflcn.h"
#include "config/g_memory_hal.h"



PerfMemTweak2xBaseEntry *pMemTweakTable;
PerfMemClk11BaseEntry *pMemClkBaseTable;
PerfMemClk11StrapEntry *pMemClkStrapTable;
PLLInfo5xEntry *pInfoEntryTable;


/*
 *-----------------------------------------------------------------------------
 * PRIV Profiling macro declarations
 * redirect all priv accesses through a pre and post loggin function when
 * priv logging is enabled.
 *-----------------------------------------------------------------------------
 */


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
// 2. A response is sent to either Host/RM (via FB-Falcon MQ) or PMU-Falcon
//    (PMU-Falcon CQx). Response should include header info, sender ID info,
//    and an ack for the FB programming that will be carried out. An interrupt
//    is also sent to inform the recipient of this "Ready" message.
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
//
// Folowing Mailbox registers are used to monitor
//
// Mailbox(1): Reflects the intrstat & intren register as loaded at the beginning
//             of the interrupt, each interrupt will process only one bit, priority
//             LSB to MSB,  (cmd queues are decoded after this register)
//
// Mailbox(2): Error codes from the last operation, falcon does not stop though
//             in theory there's something seriously wrong.  Use for debug
//
// Mailbox(10): Returns checksum for cmdqueue 0 access
// Mailbox(11): Returns checksum for cmdqueue 1 access
// Mailbox(12): Returns checksum for cmdqueue 2 access
// Mailbox(13): Returns checksum for cmdqueue 3 access
//
// Mailbox(14): Checksum of message queue (udpated while filling message queue, result
//              valid after interrupt has been asserted
//
// Mailbox(15): start/stop bit,  this will be set to 1 at the beginning of the test,
//              setting this to 0 will stop the main thread and eventually lead to a
//              falcon halt. Should be programmed before finishing test.
//              Run flags, bits are defined as follows:
//              0 - start/stop bit
//              1 - enable command queue handling
//              2 - enable message queue transmission




//-----------------------------------------------------------------


#define HBM_MCLK_PERIOD_1GHZ    1000
#define HBM_MCLK_PERIOD_500MHZ  2000
#define HBM_MCLK_PERIOD_400MHZ  2500

#define CMD_STOP_FB             0x00000001
#define CMD_START_FB            0x00000002
#define CMD_DIS_OFIFO_CHK       0x00000003
#define CMD_ENA_OFIFO_CHK       0x00000004
#define CMD_DIS_ASR_CHKR        0x00000005
#define CMD_ENA_ASR_CHKR        0x00000006
#define CMD_STOP_CLKEN_PM       0x00000007
#define CMD_START_CLKEN_PM      0x00000008
#define CMD_FORCE_BYP_PLL       0x00000009
#define CMD_SWITCH_PLL          0x0000000a

#define ACK_STOP_FB             0x0000fffe
#define ACK_START_FB            0x0000fffd
#define ACK_DIS_OFIFO_CHK       0x0000fffc
#define ACK_ENA_OFIFO_CHK       0x0000fffb
#define ACK_DIS_ASR_CHKR        0x0000fffa
#define ACK_ENA_ASR_CHKR        0x0000fff9
#define ACK_STOP_CLKEN_PM       0x0000fff8
#define ACK_START_CLKEN_PM      0x0000fff7
#define ACK_FORCE_BYP_PLL       0x0000fff6
#define ACK_SWITCH_PLL          0x0000fff5


LwU32 get_rfc (void);

void disable_asr_acpd_gv100 (void);


void set_acpd_off (void);
void enable_rdqs_diff_mode (void);
void precharge_all (void);
void enable_self_refresh (void);
void set_clk_ctrl_stop (void);
void update_pll_coeff (void);
void disable_periodic_ddllcal (void);
void set_vref_values (void);
void configure_powerdown (void);
void set_dramclk_bypass (void);
void set_hbm_coeff_cfg (void);
void change_receivers (void);
void stop_clk_en_padmacro (void);
void switch_pll (void);
void start_clk_en_padmacro (void);
void enable_ddll (void);
void hbm_program_ddll_trims (void);
void pgm_fbio_hbm_ddll_cal (void);
//LwU32 ddll_cal_ilwalid_range (LwU32 exp_lower, LwU32 exp_upper, LwU32 actual);
//void training5_check_ddll_calibration_values_hbm (void);
void digital_dll_calibration (void);
void set_timing_values (void);
void set_clk_ctrl_normal (void);
void disable_self_refresh (void);
void enable_periodic_refresh (void);
void reset_read_fifo (void);
void set_acpd_value (void);
void disable_refsb_gv100(void);
void get_timing_values_gv100(void);

LwU32 hbm_mclk_switch (void);
LwU32 bit_assign (LwU32 reg_val, LwU32 bitpos, LwU32 bitval, LwU32 bitsize);
const LwU32 freq_period_colw = 1000000;


REG_BOX newRegValues;
REG_BOX lwrrentRegValues;

REG_BOX* lwr = &lwrrentRegValues;
REG_BOX* new = &newRegValues;

LwU32 gbl_mclk_freq_final;
LwU32 gbl_mclk_freq_new;
LwU32 gbl_mclk_mdiv;
LwU32 gbl_mclk_ndiv;
LwU32 gbl_mclk_pdiv;
LwU32 gbl_mclk_period_final;
LwU32 gbl_mclk_period_new;

LwU32 gbl_hbm_dramclk_period;
LwU32 gDisabledFBPAMask;
LwU32 gLowestIndexValidPartition;

LwU32 unit_level_sim = 0;


// assume access to the new config0/10 value
// //
LwU32 get_rfc (void)
{
    LwU32 config0_rfc;
    LwU32 config10_rfc_msb;
    LwU32 rfc;

    config0_rfc = REF_VAL(LW_PFB_FBPA_CONFIG0_RFC,new->config0);
    config10_rfc_msb = REF_VAL(LW_PFB_FBPA_CONFIG10_RFC_MSB,new->config10);

    rfc = (config10_rfc_msb << DRF_SHIFT(LW_PFB_FBPA_CONFIG0_RFC)) | config0_rfc;

    return (rfc);
}

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

void
enable_dram_ack
(
    LwU8 enable
)
{
    lwr->cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACK, enable, lwr->cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, lwr->cfg0);
}


void
disable_asr_acpd_gv100
(
    void
)
{
    LwU32 ack_dis_asr_chkr;

    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, CMD_DIS_ASR_CHKR);

    ack_dis_asr_chkr = 0;
    while ((ack_dis_asr_chkr != ACK_DIS_ASR_CHKR) && unit_level_sim)
    {
        ack_dis_asr_chkr = REG_RD32(LOG, LW_PFB_FBPA_FALCON_MONITOR);
    }

    LwU32 dram_asr = lwr->dram_asr;
    dram_asr = FLD_SET_DRF(_PFB, _FBPA_DRAM_ASR, _EN, _DISABLED, dram_asr);
    dram_asr = FLD_SET_DRF(_PFB, _FBPA_DRAM_ASR, _ACPD, _DISABLED, dram_asr);
    lwr->dram_asr = dram_asr;
    REG_WR32(LOG, LW_PFB_FBPA_DRAM_ASR, lwr->dram_asr);
}

void
get_timing_values_gv100
(
    void
)
{
    //NOTE: Sangitac: These come from MemTweak tables.
    lwr->hbm_cfg0 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_CFG0);
    lwr->cfg0 = REG_RD32(LOG, LW_PFB_FBPA_CFG0);                // TODO: find remaining values in bios
    lwr->dram_asr = REG_RD32(LOG, LW_PFB_FBPA_DRAM_ASR);        // TODO: find entries in bios

    lwr->delay_broadcast_misc0 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0);
    new->delay_broadcast_misc0 = lwr->delay_broadcast_misc0;
    //new->delay_broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_TRIM, pMemClkStrapTableExtensionTemp->hbm_tx_trim, new->delay_broadcast_misc0);
    //new->counterdelay_broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_TRIM, pMemClkStrapTableExtensionTemp->hbm_wl_trim, new->delay_broadcast_misc0);

    lwr->delay_broadcast_misc1 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1);
    new->delay_broadcast_misc1 = lwr->delay_broadcast_misc1;
    //new->delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_TRIM, pMemClkStrapTableExtensionTemp->hbm_rx_trim, new->delay_broadcast_misc1);

    lwr->hbm_ddllcal_ctrl1 =  REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1);
    new->hbm_ddllcal_ctrl1 = lwr->hbm_ddllcal_ctrl1;         // new should not be used add flags to lwr

    lwr->timing1  = REG_RD32(LOG, LW_PFB_FBPA_TIMING1);        // subset in bios
    new->timing10 = TABLE_VAL (pMemTweakTable->Timing10);
    lwr->timing11 = REG_RD32(LOG, LW_PFB_FBPA_TIMING11);       // subset in bios
    lwr->timing12 = REG_RD32(LOG, LW_PFB_FBPA_TIMING12);       // subset in bios
    new->timing21 = TABLE_VAL (pMemTweakTable->Timing21);
    lwr->timing21 = REG_RD32(LOG, LW_PFB_FBPA_TIMING21);
    new->timing22 = TABLE_VAL (pMemTweakTable->Timing22);


    new->config0  = TABLE_VAL (pMemTweakTable->Config0);
    // we don't seem to need the current config0 value, don't load from fbpa
    //  lwr->config0  = REG_RD32(LOG, LW_PFB_FBPA_CONFIG0);
    lwr->config0  = new->config0;

    new->config1  = TABLE_VAL (pMemTweakTable->Config1);
    new->config2  = TABLE_VAL (pMemTweakTable->Config2);
    new->config3  = TABLE_VAL (pMemTweakTable->Config3);
    new->config4  = TABLE_VAL (pMemTweakTable->Config4);
    new->config5  = TABLE_VAL (pMemTweakTable->Config5);
    new->config6  = TABLE_VAL (pMemTweakTable->Config6);
    new->config7  = TABLE_VAL (pMemTweakTable->Config7);
    new->config8  = TABLE_VAL (pMemTweakTable->Config8);
    new->config9  = TABLE_VAL (pMemTweakTable->Config9);

    new->config10 = TABLE_VAL (pMemTweakTable->Config10);
    lwr->config10 = new->config10;                             // no use for current config10, dont'load from fbpa
    lwr->config11 = REG_RD32(LOG, LW_PFB_FBPA_CONFIG11);
    new->config11 = lwr->config11;

    new->hbm_cfg0 = lwr->hbm_cfg0;
    // subsitute new subsets of registes from the bios into the current reg value
    // Update relevant bit fields with values from table
    LwU32 table_val;

    // CFG0
    // acpd from  PerfMemClk11EntryFlags0
    new->cfg0 = lwr->cfg0;
    table_val = TABLE_VAL (pMemClkBaseTable->Flags0);
    PerfMemClk11EntryFlags0 *flags0 = (PerfMemClk11EntryFlags0*)(&table_val);
    LwU8 acpd = flags0->PMC11EF0ACPD;
    new->cfg0  = FLD_SET_DRF_NUM(_PFB, _FBPA_CFG0, _DRAM_ACPD, acpd, new->cfg0);

    // DRAM_ASR
    // ASR_EN from PerfMemClk11StrapEntryFlags1
    new->dram_asr = lwr->dram_asr;
    table_val = TABLE_VAL (pMemClkStrapTable->Flags1);
    PerfMemClk11StrapEntryFlags1* flags1 =(PerfMemClk11StrapEntryFlags1*)(&table_val);
    LwU8 asr = flags1->PMC11SEF1ASR;
    new->dram_asr = FLD_SET_DRF_NUM(_PFB, _FBPA_DRAM_ASR, _EN, asr, new->dram_asr);


    // Timing1: R2P
    table_val    = TABLE_VAL (pMemTweakTable->pmtbe391384);
    struct PerfMemTweak2xBaseEntry391384 *pmtbe_391384 = (struct PerfMemTweak2xBaseEntry391384 *) (& table_val);
    LwU32 timing1_r2p  = pmtbe_391384->Timing1_R2P;
    new->timing1 = FLD_SET_DRF_NUM(_PFB, _FBPA_TIMING1, _R2P, timing1_r2p, new->timing1);

    // Timing11: KO
    table_val    = TABLE_VAL (pMemTweakTable->pmtbe367352);
    struct PerfMemTweak2xBaseEntry367352 *pmtbe_367352 = (struct PerfMemTweak2xBaseEntry367352 *) (& table_val);
    LwU32 timing11_ko  = pmtbe_367352->Timing11_KO;
    new->timing11 = FLD_SET_DRF_NUM(_PFB, _FBPA_TIMING11, _KO, timing11_ko, new->timing11);

    // Timing12: CKE
    LwU32 timing12_cke = pmtbe_367352->Timing12_CKE;
    new->timing12 = FLD_SET_DRF_NUM(_PFB, _FBPA_TIMING12, _CKE, timing12_cke, new->timing12);
}



void disable_refsb_gv100 (void)
{
    // disable refsb in timing 21

    lwr->timing21 = FLD_SET_DRF(_PFB, _FBPA_TIMING21, _REFSB, _DISABLED, lwr->timing21);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_TIMING21, lwr->timing21);
}

/*
 * wait_for_ok_to_switch
 *  pool the mailbox0 bit that's directly tied to the ok_to_switch_signal from
 *  display.
 *  This is simpler to do than implementing an interrupt capture and getting back
 *  to this thread. The signal should be 1 if display is turned off or not in use,
 *  so the wait is applicable in all cases.
 */

// TODO: we need to think about a possible timeout mechanism

// TODO: move these definitions to proper locations, lwrrently they can't be
//       added to the manuals due to the array nature of the mailbox registers
//       and the mkprivblk parser issue with multiple overlapping bit range
//       definitions. We should probably take this out entirely of the
//       Mailbox structure and implement and independent register.
#define LW_CFBFALCON_FIRMWARE_MAILBOX0_OK_TO_SWITCH        0:0
#define LW_CFBFALCON_FIRMAREE_MAILBOX0_OK_TO_SWITCH_READY  0x1
#define LW_CFBFALCON_FIRMAREE_MAILBOX0_OK_TO_SWITCH_WAIT   0x0


void
set_acpd_off
(
    void
)
{
    lwr->cfg0 = FLD_SET_DRF(_PFB, _FBPA_CFG0, _DRAM_ACPD, _NO_POWERDOWN, lwr->cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_CFG0, lwr->cfg0);
}

void
enable_rdqs_diff_mode
(
    void
)
{
/*
 * E_DIFF mode is always one
    LwU32 HBMParam1 = TABLE_VAL (pMemClkStrapTable->HBMParam1);
    PerfMemClk11StrapEntryHBMParam1* pHBMParam1 = (PerfMemClk11StrapEntryHBMParam1*)(&HBMParam1);
    LwU8 rdqs_rcvr_sel = pHBMParam1->PMC11SEP1HbmRdqsRcvrSel;

    // enable diff mode when the rdqs receiver is set to diff-amp else set turn off
    new->delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_E_DIFF_MODE, rdqs_rcvr_sel, new->delay_broadcast_misc1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, new->delay_broadcast_misc1);
*/
}

void
precharge_all
(
    void
)
{
    // Enable DRAM_ACK
    enable_dram_ack(1);

    // Force a precharge
    REG_WR32_STALL(LOG, LW_PFB_FBPA_PRE, 0x00000001);

    // Disable DRAM_ACK
    enable_dram_ack(0);
}

void
enable_self_refresh
(
    void
)
{
    memoryEnableSelfRefreshHbm_HAL();

    OS_PTIMER_SPIN_WAIT_NS(1 + (60 * gbl_mclk_period_new) / 1000);
}


void
set_clk_ctrl_stop
(
    void
)
{
    LwU32 clk_ctrl;

    clk_ctrl = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_CMD_CFG);
    clk_ctrl = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_CMD_CFG, _CLK_CNTRL, _STOP, clk_ctrl);

    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CMD_CFG, clk_ctrl);
}


void
update_pll_coeff
(
    void
)
{
    LwU32 pll_mdiv_val;
    LwU32 pll_ndiv_val;
    LwU32 pll_pdiv_val;
    LwU32 mclk_freq_val;
    LwU32 local_mclk_freq_new;
    LwU32 max_freq_reached;
    LwU32 loop_cnt  = 0;

    // load pll limitations from table
    LwU8 tMDIVmax = TABLE_VAL(pInfoEntryTable->MMax);
    LwU8 tMDIVmin = TABLE_VAL(pInfoEntryTable->MMin);
    // FIXME: some model issue seem to indicate there is a limitation on mdiv at 2
    tMDIVmax = 0x2;
    LwU8 tNDIVmax = TABLE_VAL(pInfoEntryTable->NMax);
    LwU8 tNDIVmin = TABLE_VAL(pInfoEntryTable->NMin);
    //LwU8 tPDIVmax = TABLE_VAL(pInfoEntryTable->PLMax);
    //LwU8 tPDIVmin = TABLE_VAL(pInfoEntryTable->PLMin);
    //LwU16 ur_max = TABLE_VAL(pInfoEntryTable->URMax);
    //LwU16 ur_min = TABLE_VAL(pInfoEntryTable->URMin);
    LwU16 tVCOmax = TABLE_VAL(pInfoEntryTable->VCOMax);
    LwU16 tVCOmin = TABLE_VAL(pInfoEntryTable->VCOMin);
    //LwU16 reference_max = TABLE_VAL(pInfoEntryTable->ReferenceMax);  // the 27 for the divider could come from here
    //LwU16 reference_min = TABLE_VAL(pInfoEntryTable->ReferenceMin);
    //LwU8 pll_id = TABLE_VAL(pInfoEntryTable->PLLID);

//
// debug mechanism to override the pll settings
//#define PINFOENTRYTABLE_HARDCODED
#ifdef PINFOENTRYTABLE_HARDCODED
    LwU8 tMDIVmax = 0xff;
    LwU8 tMDIVmin = 0x1;
    tMDIVmax = 0x2;
    LwU8 tNDIVmax = 0xff;
    LwU8 tNDIVmin = 0x28;
    LwU8 tPDIVmax = 0x1f;
    LwU8 tPDIVmin = 0x1;
    //LwU16 ur_max = TABLE_VAL(pInfoEntryTable->URMax);
    //LwU16 ur_min = TABLE_VAL(pInfoEntryTable->URMin);
    LwU16 tVCOmax = 0x4b0;
    LwU16 tVCOmin = 0x258;
    //LwU16 reference_max = TABLE_VAL(pInfoEntryTable->ReferenceMax);  // the 27 for the divider could come from here
    //LwU16 reference_min = TABLE_VAL(pInfoEntryTable->ReferenceMin);
    //LwU8 pll_id = TABLE_VAL(pInfoEntryTable->PLLID);
#endif

    /// Bug 200358415 fix for OC,  VCO min/max should be a theoretical max frequency that we would never
    //     hit, but lwrrently it's programmed to POR settings and we can't abort/halt the system at that
    //     frequency.
    //
    //LwU32 tFREQmin = TABLE_VAL(pMemClkBaseTable->Minimum) & 0x3FFF;
    LwU32 tFREQmax = TABLE_VAL(pMemClkBaseTable->Maximum) & 0x3FFF;

#ifdef LOG_MCLKSWITCH
    FW_MBOX_WR32(9, 0xEEEEf00c);
    FW_MBOX_WR32(9, tMDIVmax);   // 0xff
    FW_MBOX_WR32(9, tMDIVmin);   // 0x1
    FW_MBOX_WR32(9, tNDIVmax);   // 0xff
    FW_MBOX_WR32(9, tNDIVmin);   // 0x28
    //FW_MBOX_WR32(9, tPDIVmax);   // 0x1F
    //FW_MBOX_WR32(9, tPDIVmin);   // 0x1
    FW_MBOX_WR32(9, tVCOmax);    // 0x4b0
    FW_MBOX_WR32(9, tVCOmin);    // 0x258
    //FW_MBOX_WR32(9, tFREQmin);   // 0x320
    FW_MBOX_WR32(9, tFREQmax);   // 0x4af
    FW_MBOX_WR32(9, 0xEEEEf00c);
#endif  // LOG_MCLKSWITCH

    const LwU32 pll_clkin     = 27;   // 27.027028
    LwU32 error = 0;

    local_mclk_freq_new = gbl_mclk_freq_new + 1;

#ifdef LOG_MCLKSWITCH
    FW_MBOX_WR32(12, local_mclk_freq_new);
#endif  // LOG_MCLKSWITCH

    //if ((local_mclk_freq_new > tFREQmax) || (local_mclk_freq_new < tFREQmin))
    //{
    //    FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLKSWITCH_ILLEGAL_FREQUENCY);
    //}
    gbl_mclk_freq_final = 0;
    max_freq_reached    = 0;
    LwU32 computationSteps = 0;
    while (gbl_mclk_freq_final == 0)
    {
        for (pll_mdiv_val = tMDIVmin; pll_mdiv_val <= tMDIVmax; pll_mdiv_val++)
        {
            // limiting pdiv value to a constant 1.
            // NOTE: Sangita/Stefans  this does not reduce resolution in the 800 - 1200MHz range but
            //       would need to be updated to reach lower frequencies.  Need to review if we want
            //       to hard code or if the bios should limit pdiv as well as mdiv (which typically runs
            //       in the lower 10's an not all the way up to 255 as set in mdiv  - ok for gv100
            //
            // NOTE: for overclocking we allow overun on VCO above VCOmax, this should only happen when pdiv
            //       is 1.  Should this post divider be bigger than 1 then I think we should still guarantee
            //       that the VCO is running in its proper range.
            // NOTE: Nitinr  Modifying pdiv range to 2 from 1 to get lower frequencies < 800 Mhz for GA100. Depends on
            //       PLL model VCOmin/VCOmax so need to review it again.

            if(local_mclk_freq_new <= tVCOmin)
            {
              pll_pdiv_val = 2;
            }
            else
            {
              pll_pdiv_val = 1;
            }
            //for (pll_pdiv_val = tPDIVmin; pll_pdiv_val <= tPDIVmax; pll_pdiv_val++)
            //{
                if (((pll_pdiv_val * local_mclk_freq_new) >= tVCOmin) && ((pll_pdiv_val * local_mclk_freq_new) <= (tVCOmax + 200)))
                {
                    // Ndiv should *always* be rounded down, e.g. 1.78 should
                    // be 1 not 2 to get accurate results
                    pll_ndiv_val = (local_mclk_freq_new * pll_pdiv_val * pll_mdiv_val) / pll_clkin;
                    if ((pll_ndiv_val >= tNDIVmin) && (pll_ndiv_val <= tNDIVmax))
                    {
                        mclk_freq_val = (pll_ndiv_val * pll_clkin) / (pll_pdiv_val * pll_mdiv_val);
                        if (mclk_freq_val > local_mclk_freq_new)
                        {
                            error++;
                            FW_MBOX_WR32(9, 0xEEEE0E0E);
                        }
                        if ((gbl_mclk_freq_final < mclk_freq_val) && (mclk_freq_val <= local_mclk_freq_new))
                        {
                            gbl_mclk_ndiv = pll_ndiv_val;
                            gbl_mclk_pdiv = pll_pdiv_val;
                            gbl_mclk_mdiv = pll_mdiv_val;
                            gbl_mclk_freq_final = mclk_freq_val;
                        }
                    }
                    // ndiv value is monotoneously increasing, once we hit its max point we can break
                    // out of it.
                    // FIXME: reducing mdiv to a much smaller range makes this seem unnecessary
                    // if (pll_ndiv_val > tNDIVmax)
                    //{
                    //    break;
                    //}
                }
                computationSteps++;
            //}
        }
    // This is for the mclk_switch case
        if (gbl_mclk_freq_final == 0)
        {
            if ((local_mclk_freq_new < tFREQmax) && (max_freq_reached == 0))
            {
                max_freq_reached = 1;
            }
            else
            {
                local_mclk_freq_new--;
            }
            loop_cnt++;
        }
        if (loop_cnt > 1000)
        {
            FBFLCN_HALT(FBFLCN_ERROR_CODE_MCLKSWTTCH_PLL_LOOP_OVERRUN);
        }
    }
    gbl_mclk_period_final = freq_period_colw / gbl_mclk_freq_final;

#ifdef LOG_MCLKSWITCH
    FW_MBOX_WR32(10, computationSteps);
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, gbl_mclk_mdiv);
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, gbl_mclk_pdiv);
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, gbl_mclk_ndiv);
#endif // LOG_MCLKSWITCH

    FW_MBOX_WR32(1, gbl_mclk_period_final);
}

void
disable_periodic_ddllcal
(
    void
)
{
    LwU32 ddllcal_ctrl1 = lwr->hbm_ddllcal_ctrl1;

    ddllcal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _DISABLE_PERIODIC_UPDATE, _INIT, ddllcal_ctrl1);
    ddllcal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _OVERRIDE_DDLLCAL_CODE, _INIT, ddllcal_ctrl1);

    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);
    lwr->hbm_ddllcal_ctrl1 = ddllcal_ctrl1;

}

void
set_vref_values
(
    void
)
{
    LwU32 HBMParam1 = TABLE_VAL (pMemClkStrapTable->HBMParam1);
    PerfMemClk11StrapEntryHBMParam1* pHBMParam1 = (PerfMemClk11StrapEntryHBMParam1*)(&HBMParam1);
    LwU8 rdqs_rcvr_sel = pHBMParam1->PMC11SEP1HbmRdqsRcvrSel;
    LwU8 dq_rcvr_sel = pHBMParam1->PMC11SEP1HbmDqRcvrSel;

    // enable diff mode when the rdqs receiver is set to diff-amp else set turn off
    //new->delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_E_DIFF_MODE, rdqs_rcvr_sel, new->delay_broadcast_misc1);

    // NOTE: Sangitac: These come from PMCT (Performance Memory Clock Table) Strap entry
    // VBIOS should have Receiver select options : CMOS or Diff amp

    LwU32 bcast1 = new->delay_broadcast_misc1;

    if (dq_rcvr_sel == 0) {
        // DQ - CMOS
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _DQ_RCVR_SEL, 0x1, bcast1);
    } else {
        // DQ - Diff-Amp  (vs vref)
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_VREF_PWRD, 0x0, bcast1);
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_BIAS_PWRD, 0x0, bcast1);
    }

    if (rdqs_rcvr_sel == 0) {
        // RDQS - CMOS
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_RCVR_SEL, 0x1, bcast1);
        //bcast1 = FLD_SET_DRF_NUM(_PFB,_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1,_RDQS_E_DIFF_MODE,0x1,bcast1);
    } else {
        // RDQS - Diff-Amp
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_RX_BIAS_PWRD, 0x0, bcast1);
    }

    REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, bcast1);

    // Wait for 500 ns
    OS_PTIMER_SPIN_WAIT_NS(500);

    if (dq_rcvr_sel == 0) {
        // DQ - CMOS
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_VREF_PWRD, 0x1, bcast1);
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_BIAS_PWRD, 0x1, bcast1);
    } else {
        // DQ - Diff-Amp  (vs vref)
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _DQ_RCVR_SEL, 0x2, bcast1);
    }

    if (rdqs_rcvr_sel == 0) {
        // RDQS - CMOS
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_RX_BIAS_PWRD, 0x1, bcast1);
    } else {
        // RDQS - Diff-Amp
        bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_RCVR_SEL, 0x3, bcast1);
        //bcast1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RDQS_E_DIFF_MODE, 0x1, bcast1);
    }


    REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, bcast1);
    new->delay_broadcast_misc1 = bcast1;

}

void
configure_powerdown
(
    void
)
{
    // powerdown trimmers, to remove excessive load variations during update
    // rx_trim does not required this as its not free running.

    // LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0
    lwr->delay_broadcast_misc0 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_DDLL_PWRD, _ENABLE, lwr->delay_broadcast_misc0);
    lwr->delay_broadcast_misc0 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_DDLL_PWRD, _ENABLE, lwr->delay_broadcast_misc0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, lwr->delay_broadcast_misc0);
}

void
set_dramclk_bypass
(
    void
)
{
    LwU32 ack_dis_ofifo_chk;
    LwU32 timeout = 100;
    LwU32 loop = 0;

    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, CMD_DIS_OFIFO_CHK);

    ack_dis_ofifo_chk = 0;
    while ((ack_dis_ofifo_chk != ACK_DIS_OFIFO_CHK) && unit_level_sim) {
        ack_dis_ofifo_chk = REG_RD32(LOG, LW_PFB_FBPA_FALCON_MONITOR);
        if (loop >= timeout) {
            break;
        }
        loop++;
    }

    memorySetDramClkBypassHbm_HAL();

    // Forcing bypass mode (temporarily) implies a fixed 500MHz frequency, so
    // update dramclk period to reflect that
    gbl_hbm_dramclk_period = HBM_MCLK_PERIOD_500MHZ;
}

void
set_hbm_coeff_cfg
(
    void
)
{
    LwU32 hbmpll_cfg;
    LwU32 hbmpll_coeff;

    // Disable PLL here to avoid clk glitches while loading new coefficients
    hbmpll_cfg = REG_RD32(LOG, unicastMasterReadFromBroadcastRegister (LW_PFB_FBPA_FBIO_HBMPLL_CFG));
    hbmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBMPLL_CFG, _ENABLE, _NO, hbmpll_cfg);
    hbmpll_cfg = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBMPLL_CFG, _EN_LCKDET, _DISABLE, hbmpll_cfg);

    REG_WR32(LOG,multicastMasterWriteFromBrodcastRegister (LW_PFB_FBPA_FBIO_HBMPLL_CFG), hbmpll_cfg);
    // fence multicast on serial priv
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE, 0x0);
    OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole

    // Set up new coefficients and write the changes
    hbmpll_coeff = REG_RD32(LOG, unicastMasterReadFromBroadcastRegister (LW_PFB_FBPA_FBIO_HBMPLL_COEFF));

    hbmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBMPLL_COEFF, _MDIV, gbl_mclk_mdiv, hbmpll_coeff);
    hbmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBMPLL_COEFF, _NDIV, gbl_mclk_ndiv, hbmpll_coeff);
    hbmpll_coeff = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBMPLL_COEFF, _PLDIV, gbl_mclk_pdiv, hbmpll_coeff);

    REG_WR32(LOG,multicastMasterWriteFromBrodcastRegister (LW_PFB_FBPA_FBIO_HBMPLL_COEFF), hbmpll_coeff);
    // fence multicast on serial priv
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE, 0x0);
    OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole

    hbmpll_cfg = FLD_SET_DRF(_PFB,_FBPA_FBIO_HBMPLL_CFG, _ENABLE, _YES, hbmpll_cfg);
    hbmpll_cfg = FLD_SET_DRF(_PFB,_FBPA_FBIO_HBMPLL_CFG, _EN_LCKDET, _ENABLE, hbmpll_cfg);

    REG_WR32(LOG,multicastMasterWriteFromBrodcastRegister (LW_PFB_FBPA_FBIO_HBMPLL_CFG), hbmpll_cfg);
    // fence multicast on serial priv
    REG_WR32(LOG, LW_PPRIV_FBP_FBPS_PRI_FENCE, 0x0);
    OS_PTIMER_SPIN_WAIT_NS(20);  // Bug 3332161 - PRI_FENCE ordering corner case timing hole

    /*
     * pooling 1 partition per site for a pll lock
     * there's only 1 pll per site, shared across all partitions
     */

    LwU8 idx;
    // loop through each site
    for (idx = 0; idx < 4; idx++) {
        // get the disabled mask for each site (including 4 partitions)
        LwU8 mask = (((0xf << (idx*4)) & gDisabledFBPAMask) >> (idx*4)) & 0xf;
        // proceed only if one of the partitions in the site is enabled
        if (mask != 0xf)
        {
            // start partition of the site
            LwU8 partition = idx * 4;
            // find the first active partition of the site
            LwU8 disabled = mask & 0x1;
            while (disabled == 1)
            {
                mask = (mask >> 1) & 0xf;
                disabled = mask & 0x1;
                partition++;
            }
            // wait for lock on the partition
            LwU8 lock = 0;
            while (lock == 0)
            {
                hbmpll_cfg = REG_RD32(LOG, unicast (LW_PFB_FBPA_FBIO_HBMPLL_CFG, partition));
                lock = REF_VAL(LW_PFB_FBPA_FBIO_HBMPLL_CFG_PLL_LOCK, hbmpll_cfg);
            }
        }
    }
}

void change_receivers (void)
{
    /*
     * It looks like we do all of this already in the vref section
     */
    /*
    // NOTE: Sangitac: These come from PMCT (Performance Memory Clock Table) Strap entry
    // VBIOS should have Receiver select options : CMOS or Diff amp
    LwU32 bcast1_data;

    bcast1_data = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1);

    // NOTE: Sangitac: CMOS mode
    if (gbl_mclk_period_new <= HBM_MCLK_PERIOD_1GHZ)
    {
        bcast1_data = bit_assign (bcast1_data, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1_DQ_RCVR_SEL_BIT,      0x02, 3);
        bcast1_data = bit_assign (bcast1_data, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1_RDQS_E_DIFF_MODE_BIT, 0x01, 1);
        bcast1_data = bit_assign (bcast1_data, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1_RDQS_RCVR_SEL_BIT,    0x03, 3);
    }
    else
    {
        // NOTE: Sangitac: DIFF AMP mode
        bcast1_data = bit_assign (bcast1_data, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1_DQ_RCVR_SEL_BIT,      0x01, 3);
        bcast1_data = bit_assign (bcast1_data, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1_RDQS_E_DIFF_MODE_BIT, 0x01, 1);
        bcast1_data = bit_assign (bcast1_data, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1_RDQS_RCVR_SEL_BIT,    0x01, 3);
    }

    REG_WR32_STALL(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, bcast1_data);
    */
}


void
stop_clk_en_padmacro
(
    void
)
{
    memoryStopClkenPadmacroHbm_HAL();

    // Wait 20 hbm dram clk cycles here
    OS_PTIMER_SPIN_WAIT_NS( 1 + (20 * gbl_hbm_dramclk_period)/1000);
}


void
switch_pll
(
    void
)
{

    // PLL switching scenario we care about is "vco-to-vco"

    // Update dramclk to reflect change in PLL state; also store the new
    // dramclk period to a Mailbox for consumption by the testbench
    gbl_hbm_dramclk_period = gbl_mclk_period_final;
    FW_MBOX_WR32_STALL(7, gbl_mclk_period_new);
    FW_MBOX_WR32_STALL(8, gbl_mclk_period_final);

    memorySwitchPllHbm_HAL();


    // FIXME BOZO this address will need to change - ready to go
    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, CMD_SWITCH_PLL);
    // Wait 20 new clk cycles after performing PLL switchover
    OS_PTIMER_SPIN_WAIT_NS( 1+ (20 * gbl_hbm_dramclk_period)/1000);
}


void
start_clk_en_padmacro
(
    void
)
{
    LwU32 ack_ena_ofifo_chk;

    // Wait 20 new clk cycles before enabling pad macro clock
    OS_PTIMER_SPIN_WAIT_NS( 1+ (20 * gbl_hbm_dramclk_period)/1000);

    memoryStartClkenPadmacroHbm_HAL();

    // Wait 50 new clk cycles after enabling pad macro clock
    OS_PTIMER_SPIN_WAIT_NS( 1 + (50 * gbl_hbm_dramclk_period)/1000);

    // FIXME: this is for debug only (assumption by Sangita that we need more settle time for priv in the PA's)
    FW_MBOX_WR32(6, gbl_hbm_dramclk_period);
    OS_PTIMER_SPIN_WAIT_NS(1);
    //^^^

    REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, CMD_ENA_OFIFO_CHK);

    ack_ena_ofifo_chk = 0;
    while ((ack_ena_ofifo_chk != ACK_ENA_OFIFO_CHK) && unit_level_sim)
    {
        ack_ena_ofifo_chk = REG_RD32(LOG, LW_PFB_FBPA_FALCON_MONITOR);
    }
}


void
enable_ddll
(
    void
)
{

    // LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1
    // did not turn this off so no need to turn it back on  TODO: check this assumption
    //lwr->delay_broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_DDLL_PWRD, dllCal, lwr->delay_broadcast_misc1);
    //REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, lwr->delay_broadcast_misc1);

    // LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0
    LwU32 broadcast_misc0 = lwr->delay_broadcast_misc0;
    broadcast_misc0 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_DDLL_PWRD, _DISABLE, broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_DDLL_PWRD, _DISABLE, broadcast_misc0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, broadcast_misc0);
    lwr->delay_broadcast_misc0 = broadcast_misc0;

    // Wait 20 ltc clk cycles
    OS_PTIMER_SPIN_WAIT_NS(20);
}



void
hbm_program_ddll_trims
(
    void
)
{
    // get ddllcal flag from the bios table
    // LwU32 flag3 = TABLE_VAL(pMemClkStrapTable->Flags3); // FIXME
    // PerfMemClk11StrapEntryFlags3* pFlag3 = (PerfMemClk11StrapEntryFlags3*)(&flag3); // FIXME
    // LwU8 dllCal = pFlag3->PMC11SEF3DLLCal; // FIXME
    LwU8 bypass = 0x0;

    // write rx/tx/wl trim and bypass based of cal_ddll bit
    LwU32 HBMParam1 = TABLE_VAL (pMemClkStrapTable->HBMParam1);
    PerfMemClk11StrapEntryHBMParam1* pHBMParam1 = (PerfMemClk11StrapEntryHBMParam1*) (&HBMParam1);
    LwU32 hbm_rx_trim = pHBMParam1->PMC11SEP1Hbm_rx_trim;
    //LwU8 dq_rcvr_sel = pHBMParam1->PMC11SEP1HbmDqRcvrSel;

    LwU32 HBMParam0 = TABLE_VAL (pMemClkStrapTable->HBMParam0);
    PerfMemClk11StrapEntryHBMParam0* pHBMParam0 = (PerfMemClk11StrapEntryHBMParam0*)(&HBMParam0);
    LwU32 hbm_tx_trim = pHBMParam0->PMC11SEP0Hbm_tx_trim;
    LwU32 hbm_wl_trim = pHBMParam0->PMC11SEP0Hbm_wl_trim;

    LwU32 broadcast_misc0 = lwr->delay_broadcast_misc0;
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_TRIM, hbm_tx_trim, broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_TRIM, hbm_wl_trim, broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _TX_DDLL_BYPASS, bypass, broadcast_misc0);
    broadcast_misc0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0, _WL_DDLL_BYPASS, bypass, broadcast_misc0);

    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC0,broadcast_misc0);
    lwr->delay_broadcast_misc0 = broadcast_misc0;

    lwr->delay_broadcast_misc1 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1);
    LwU32 broadcast_misc1 = lwr->delay_broadcast_misc1;
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_TRIM, hbm_rx_trim, broadcast_misc1);
    broadcast_misc1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1, _RX_DDLL_BYPASS, bypass, broadcast_misc1);

    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DELAY_BROADCAST_MISC1,broadcast_misc1);
    lwr->delay_broadcast_misc1 = broadcast_misc1;



    // Need to turn clocks off and back on to load new trims  TODO: does this still hold valid with table based approach
    // First turn clocks off
    LwU32 hbm_ddll_cal_ctrl1 = lwr->hbm_ddllcal_ctrl1;
    hbm_ddll_cal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CLK_GATE, _OFF, hbm_ddll_cal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, hbm_ddll_cal_ctrl1);

    OS_PTIMER_SPIN_WAIT_NS(20);

    // Now turn clocks back on
    //hbm_ddll_cal_ctrl1 = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1);
    //hbm_ddll_cal_ctrl1 = bit_assign (hbm_ddll_cal_ctrl1, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1_CLK_GATE_BIT, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1_CLK_GATE_ON, 1);

    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, lwr->hbm_ddllcal_ctrl1);
}

void
pgm_fbio_hbm_ddll_cal
(
    void
)
{
    LwU32 hbm_pwr_ctrl;
    LwU32 hbm_pwr_ctrl_orig;
    LwU32 trimmer_step;
    LwU32 wait_time;
    LwU32 wait_time_exit;
    LwU32 ddllcal_ctrl;
    LwU32 ddllcal_ctrl1;
    LwU32 ignore_start;
    LwU32 start_trm;
    LwU32 sample_delay;
    LwU32 sample_count;

    // NOTE: Sangita : has to look at PMCT strap table flag "DLL Calibration". If enabled, then the following has to be run.
    // New flag will need to be added to VBIOS to match current S4 VBIOS called "Periodic DDLLCAL"
    // There will be 3 conditions.
    // 1. No calibration
    // 2. Calibration + No periodic calibration
    // 3. Calibration + periodic calibration
    hbm_pwr_ctrl_orig = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_PWR_CTRL);
    hbm_pwr_ctrl = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_PWR_CTRL, _BLCG_BYTE, _DISABLED, hbm_pwr_ctrl_orig);

    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_PWR_CTRL, hbm_pwr_ctrl);

    // get ddllcal flag from the bios table
    LwU32 flag3 = TABLE_VAL(pMemClkStrapTable->Flags3);
    PerfMemClk11StrapEntryFlags3* pFlag3 = (PerfMemClk11StrapEntryFlags3*)(&flag3);
    LwU8 dllCal = pFlag3->PMC11SEF3DLLCal;

    // get periodic ddllcal bit from the bios table
    LwU32 flag5 = TABLE_VAL(pMemClkStrapTable->Flags5);
    PerfMemClk11StrapEntryFlags5* pFlag5 = (PerfMemClk11StrapEntryFlags5*)(&flag5);
    LwU8 dllPeriodicCal = pFlag5->PMC11SEF5PeriodicDDLLCal;

    // Disable clock gate at start of DDLL cal
    ddllcal_ctrl1  = lwr->hbm_ddllcal_ctrl1;
    ddllcal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CLK_GATE, _OFF, ddllcal_ctrl1);
    ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _DISABLE_PERIODIC_UPDATE, !dllPeriodicCal, ddllcal_ctrl1);
    ddllcal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _OVERRIDE_DDLLCAL_CODE, _INIT, ddllcal_ctrl1);
    ddllcal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CALIBRATE, _GO, ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);

    // OVERRIDE_DDLLCAL_CODE has to be programmed at least 1 cycle after
    // enabling periodic update to avoid passing 0 start value to calibration
    // when IGNORE_START is enabled
    ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _OVERRIDE_DDLLCAL_CODE, 0x0, ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);

    trimmer_step  = 3;
    ddllcal_ctrl  = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL);
    ignore_start  = REF_VAL(LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1_IGNORE_START, ddllcal_ctrl);
    start_trm     = REF_VAL(LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL_START_TRM,ddllcal_ctrl);
    sample_delay  = REF_VAL(LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL_SAMPLE_DELAY,ddllcal_ctrl);
    sample_count  = REF_VAL(LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL_SAMPLE_COUNT,ddllcal_ctrl);

    //FW_MBOX_WR32(7, 0xaaaaaaaa1);
    wait_time     = ((gbl_hbm_dramclk_period / (trimmer_step * 2) - (ignore_start ? 0 : start_trm)) *
                      (2 + (sample_delay + 2) + ((1 << sample_count) + 1))) + 100;

//#ifdef LOG_MCLKSWITCH
    FW_MBOX_WR32(7, 0xaaaaaaaa);
    FW_MBOX_WR32(7, trimmer_step);
    FW_MBOX_WR32(7, ddllcal_ctrl);
    FW_MBOX_WR32(7, ddllcal_ctrl1);
    FW_MBOX_WR32(7, ignore_start);
    FW_MBOX_WR32(7, start_trm);
    FW_MBOX_WR32(7, sample_delay);
    FW_MBOX_WR32(7, sample_count);
    FW_MBOX_WR32(7, wait_time);
    FW_MBOX_WR32(7, gbl_hbm_dramclk_period);
    FW_MBOX_WR32(7, 0xbbbbbbbb);
//#endif


    OS_PTIMER_SPIN_WAIT_NS ( 1 + (wait_time * gbl_hbm_dramclk_period *10)/1000);
    FW_MBOX_WR32(7, 0xcccccccc);

    ddllcal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CALIBRATE, _STOP, ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);

    // Restore BLCG_BYTE
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_PWR_CTRL, hbm_pwr_ctrl_orig);

    wait_time_exit = (1 << sample_count) + 200;

    OS_PTIMER_SPIN_WAIT_NS (1 + (wait_time_exit * gbl_hbm_dramclk_period) / 1000);


    // Set CLK_GATE bit accordingly after everything else is done
    ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CLK_GATE, dllCal, ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);
    lwr->hbm_ddllcal_ctrl1 = ddllcal_ctrl1;
}


void
digital_dll_calibration
(
    void
)
{
    LwU32 ddllcal_ctrl1 = lwr->hbm_ddllcal_ctrl1;
    ddllcal_ctrl1 = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CLK_GATE, _OFF, ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);
    lwr->hbm_ddllcal_ctrl1 = ddllcal_ctrl1;

    hbm_program_ddll_trims();    // rx/tx/wl trims

    pgm_fbio_hbm_ddll_cal();     // periodic cal setup    TODO: we disable the clk inside there again though it looks like we turned
                                 // this of in this function already

    LwU32 fbio_cfg_pwrd = REG_RD32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD);
    fbio_cfg_pwrd = FLD_SET_DRF(_PFB,_FBPA_FBIO_CFG_PWRD,_CLK_TX_PWRD,_DISABLE, fbio_cfg_pwrd);    // TODO: do we need to enable this somewhere ?
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_CFG_PWRD, fbio_cfg_pwrd);

    // get ddllcal flag from the bios table
    LwU32 flag3 = TABLE_VAL(pMemClkStrapTable->Flags3);
    PerfMemClk11StrapEntryFlags3* pFlag3 = (PerfMemClk11StrapEntryFlags3*)(&flag3);
    LwU8 dllCal = pFlag3->PMC11SEF3DLLCal;

    ddllcal_ctrl1 = lwr->hbm_ddllcal_ctrl1;
    ddllcal_ctrl1 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_DDLLCAL_CTRL1, _CLK_GATE, dllCal, ddllcal_ctrl1);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_DDLLCAL_CTRL1, ddllcal_ctrl1);
    lwr->hbm_ddllcal_ctrl1 = ddllcal_ctrl1;
}

void
set_timing_values
(
    void
)
{
    REG_WR32(LOG, LW_PFB_FBPA_TIMING1,  new->timing1);
    REG_WR32(LOG, LW_PFB_FBPA_TIMING10, new->timing10);
    REG_WR32(LOG, LW_PFB_FBPA_TIMING11, new->timing11);
    REG_WR32(LOG, LW_PFB_FBPA_TIMING12, new->timing12);

    REG_WR32(LOG, LW_PFB_FBPA_CONFIG0, new->config0);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG1, new->config1);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG2, new->config2);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG3, new->config3);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG4, new->config4);

    // FIXIME: #1874302 - sthis should load real config 11 from bios as a short time workaround
    // we copy RRD to the RRDL field in config11
    LwU32 rrd = REF_VAL(LW_PFB_FBPA_CONFIG4_RRD,new->config4);
    new->config11 = FLD_SET_DRF_NUM (_PFB, _FBPA_CONFIG11, _RRDL, rrd, new->config11);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG11, new->config11);

    REG_WR32(LOG, LW_PFB_FBPA_CONFIG5, new->config5);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG6, new->config6);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG7, new->config7);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG8, new->config8);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG9, new->config9);
    REG_WR32(LOG, LW_PFB_FBPA_CONFIG10, new->config10);
}


void
set_clk_ctrl_normal
(
    void
)
{
    LwU32 cmd_cfg;

    cmd_cfg = REG_RD32(LOG, LW_PFB_FBPA_FBIO_HBM_CMD_CFG);
    cmd_cfg = FLD_SET_DRF(_PFB, _FBPA_FBIO_HBM_CMD_CFG, _CLK_CNTRL, _NORMAL, cmd_cfg);

    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CMD_CFG, cmd_cfg);

    // Update dramclk to reflect change in PLL state
    gbl_hbm_dramclk_period = gbl_mclk_period_final;
}

void
disable_self_refresh
(
    void
)
{
    LwU32 config0_rfc_wait;
    memoryDisableSelfrefreshHbm_HAL();

    config0_rfc_wait = get_rfc();
    config0_rfc_wait *= gbl_mclk_period_new;

    // Wait for SRX to complete
    OS_PTIMER_SPIN_WAIT_NS(100 + (config0_rfc_wait/1000));
}

void
enable_periodic_refresh
(
    void
)
{
    LwU32 refctrl_valid;

    // Enable DRAM_ACK
    enable_dram_ack(1);

    // We need at least 2 refreshes here to take into account the previous waits
    REG_WR32_STALL(LOG, LW_PFB_FBPA_PRE, 0x00000001);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);

    // Set VALID bit in REFCTRL to enable refresh requests
    refctrl_valid = REG_RD32(LOG, LW_PFB_FBPA_REFCTRL);
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _VALID, _1, refctrl_valid);

    // Clear PUT and GET fields every time REFCTRL is written - this avoids a corner
    // case where GET field may advance past PUT field while reading REFCTRL
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _PUT, _0, refctrl_valid);
    refctrl_valid = FLD_SET_DRF(_PFB, _FBPA_REFCTRL, _GET, _0, refctrl_valid);

    REG_WR32_STALL(LOG, LW_PFB_FBPA_REFCTRL, refctrl_valid);

    // We need atleast 2 more refreshes here to take into account the previous waits
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);
    REG_WR32_STALL(LOG, LW_PFB_FBPA_REF, 0x00000001);

    // Disable DRAM_ACK
    enable_dram_ack(0);

    // Enable REFSB as set in final value of timing21
    REG_WR32(LOG, LW_PFB_FBPA_TIMING21, new->timing21);
}

void
reset_read_fifo
(
    void
)
{
    LwU32 hbm_cfg0;

    hbm_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG0, _FBIO_IFIFO_PTR_RESET, 0x0, lwr->hbm_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CFG0, hbm_cfg0);
    hbm_cfg0 = FLD_SET_DRF_NUM(_PFB, _FBPA_FBIO_HBM_CFG0, _FBIO_IFIFO_PTR_RESET, 0x1, lwr->hbm_cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_FBIO_HBM_CFG0, hbm_cfg0);
}

void
set_acpd_value
(
    void
)
{
    LwU32 ack_ena_asr_chkr;

    REG_WR32(LOG, LW_PFB_FBPA_CFG0, new->cfg0);
    REG_WR32(LOG, LW_PFB_FBPA_DRAM_ASR, new->dram_asr);

    REG_WR32_STALL(LOG, LW_PFB_FBPA_FALCON_MONITOR, CMD_ENA_ASR_CHKR);

    ack_ena_asr_chkr = 0;
    while ((ack_ena_asr_chkr != ACK_ENA_ASR_CHKR) && unit_level_sim)
    {
        ack_ena_asr_chkr = REG_RD32(LOG, LW_PFB_FBPA_FALCON_MONITOR);
    }
}


LwU32 hbm_mclk_switch (void)
{
    FLCN_TIMESTAMP  startFbStopTimeNs = { 0 };
    LwU32           fbStopTimeNs      = 0;

    detectLowestIndexValidPartition();

    // Callwlate requested mclk period before beginning
    gbl_mclk_period_new = freq_period_colw / gbl_mclk_freq_new;

    get_timing_values_gv100();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f002);
#endif

    memoryGetMrsValueHbm_HAL();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f003);
#endif

    disable_asr_acpd_gv100();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f001);
#endif

    disable_refsb_gv100();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f004);
#endif

    memoryStopFb_HAL(&Fbflcn, 1, &startFbStopTimeNs);

#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f005);
#endif

    set_acpd_off();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f008);
#endif

    //enable_rdqs_diff_mode();
    //#ifdef MONITOR_MCLKSWITCH
    // REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f008);
    //#endif

    precharge_all();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f009);
#endif

    enable_self_refresh();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f00a);
#endif

    set_clk_ctrl_stop();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f00b);
#endif

    update_pll_coeff();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f00c);
#endif

    disable_periodic_ddllcal();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f00d);
#endif

    set_vref_values();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f00e);
#endif

    configure_powerdown();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f00f);
#endif

    set_dramclk_bypass();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f010);
#endif
    //REG_WR32(LOG, LW_PFB_FBPA_FALCON_MONITOR, 0xB000B010);

    set_hbm_coeff_cfg();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f011);
#endif

    //change_receivers();
    //#ifdef MONITOR_MCLKSWITCH
    // REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f012);
    //#endif

    stop_clk_en_padmacro();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f013);
#endif

    switch_pll();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f014);
#endif

    start_clk_en_padmacro();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f015);
#endif

    enable_ddll();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f016);
#endif

    digital_dll_calibration();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f017);
#endif

    set_timing_values();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f018);
#endif

    set_clk_ctrl_normal();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f019);
#endif

    disable_self_refresh();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f01a);
#endif

    enable_periodic_refresh();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f01b);
#endif

    memorySetMrsValuesHbm_HAL();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f01c);
#endif

    reset_read_fifo();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f01d);
#endif

    set_acpd_value();
#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f01e);
#endif

    fbStopTimeNs = memoryStartFb_HAL(&Fbflcn, &startFbStopTimeNs);

#ifdef MONITOR_MCLKSWITCH
    REG_WR32(BAR0, LW_PFB_FBPA_FALCON_MONITOR, 0x0000f01f);
#endif

    return fbStopTimeNs;
}



LwU32
doMclkSwitch
(
    LwU16 targetFreqMHz
)
{

    LwU32 fbStopTimeNs = 0;

// For now set this up that we assume unit level sin when running & compiling in the //hw location
#ifdef FBFLCN_HW_RUNTIME_ELW
    unit_level_sim = 0;
#endif
    unit_level_sim = 0;

#ifdef LOG_MCLKSWITCH
    FW_MBOX_WR32(10, targetFreqMHz);
#endif
    fbfalconSelectTable_HAL(&Fbflcn,targetFreqMHz);

    // assign local helper pointers
    pMemTweakTable = gTT.pMTBEp;
    pMemClkBaseTable = gTT.pMCBEp;
    pMemClkStrapTable = gTT.pMCSEp;
    pInfoEntryTable = gTT.pIEp;

    gbl_mclk_freq_new = targetFreqMHz;

#ifdef PRIV_PROFILING
    privprofilingEnable();
    privprofilingReset();
#endif

    fbStopTimeNs = hbm_mclk_switch();

#ifdef PRIV_PROFILING
    privprofilingDisable();
#endif

#ifdef LOG_MCLKSWITCH
    FW_MBOX_WR32(10, 0xb1ff0000);
#endif

    return fbStopTimeNs;
    // return RM_FBFLCN_MCLK_SWITCH_MSG_STATUS_OK;
}
