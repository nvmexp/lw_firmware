/*
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6tu10x_only.c
 * @brief  PMU GC6 Hal Functions for TU10X
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_lw_xp.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "pmu_bar0.h"
#include "dev_master.h"
#include "dev_pwr_pri.h"
#include "dev_trim.h"
#include "dev_top.h"
#include "pmu_gc6.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */

// Size of BSI descriptor in dwords
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE              5

// Sections of each dword in the descriptor
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_IMEM0      0
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_IMEM1      1
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_DMEM0      2
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_DMEM1      3
#define LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_PRIV       4

#define LW_PGC6_BAR0_OP_SIZE_DWORD                       3
#define LW_PGC6_PRIV_OP_SIZE_DWORD                       2

#define LW_PGC6_SCI_TO_THERM_MULTIPLIER                  4

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Function to gate/ungate clocks for FGC6 sequence
 * @param[in]   bGate: Parameter decide to gate or ungate clocks.
 *                   The value passed is directly written to the DISABLE field of different clock registers
 *                   1 = gate clocks
 *                   0 = ungate clocks
 */
void
pgFgc6ClkGating_TU10X(LwBool bGate)
{
    LwU32 val;
    // Colwerting LwBool to LwU32 variable
    LwU32 gateVal = (bGate ? 1 : 0);

    val = REG_RD32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _SYSCLK_XPXV_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _HOSTCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_XCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = REG_RD32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_PCIE_DIS_OVRD, gateVal, val);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_PCIE_DIS, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);
}

/*!
 * @brief FGC6 pre-entry sequence. This function does all the required register writes to prepare for
 * FGC6 entry. The steps are from the FGC6 IAS.
 */
void
pgFgc6PreEntrySeq_TU10X(void)
{
    LwU32 val, val2;
    // Setting gateVal 1 to gate all clocks. 1 is written to _DISABLE field of all clock registers in the sequence
    LwU32 gateVal= 1;

    // Disable CLK_REQ
    val = REG_RD32(FECS, LW_XP_PEX_PLL);
    val2 = FLD_TEST_DRF(_XP, _PEX_PLL, _CLK_REQ_EN, _DISBLED, val);

    if (!val2)
    {
        val = FLD_SET_DRF(_XP, _PEX_PLL, _CLK_REQ_EN, _DISBLED, val);
    }

    REG_WR32(FECS, LW_XP_PEX_PLL, val);

    // Switch XCLK to PEXREFCLK
    val = REG_RD32(FECS, LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC);
    val = FLD_SET_DRF(_PTRIM, _SYS_IND_SYS_CORE_CLKSRC, _HOSTCLK, _PEX_REFCLK, val);

    REG_WR32(FECS, LW_PTRIM_SYS_IND_SYS_CORE_CLKSRC, val);

    // Enable CLKREQ in FGC6
    val = REG_RD32(FECS, LW_PGC6_BSI_XTAL_DOMAIN_FEATURES);
    val = FLD_SET_DRF(_PGC6, _BSI_XTAL_DOMAIN_FEATURES , _CLKREQ_IN_GC6, _ENABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_XTAL_DOMAIN_FEATURES, val);

    // Assert PCIE reset_precredit
    val = REG_RD32(FECS, LW_PGC6_BSI_FGC6_PCIE);
    val = FLD_SET_DRF(_PGC6, _BSI_FGC6_PCIE, _RST_PRECREDIT_IF, _ASSERT, val);
    REG_WR32(FECS, LW_PGC6_BSI_FGC6_PCIE, val);

    // Switch XCLK to PEXREFCLK
    val = REG_RD32(BAR0, LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL);
    val = FLD_SET_DRF(_PTRIM, _SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL, _XCLK_AUTO, _DISABLE, val);
    val = FLD_SET_DRF(_PTRIM, _SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL, _XCLK, _PEX_REFCLK, val);
    REG_WR32(BAR0, LW_PTRIM_SYS_IND_SYS_CORE_CLK_PEXCLK_CTRL, val);

    // Gate clocks. (cannot reuse pgFgc6ClkGating due to different overlays used for entry and exit)
    val = REG_RD32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _SYSCLK_XPXV_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _HOSTCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_XCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = REG_RD32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_PCIE_DIS_OVRD, gateVal, val);
    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_PCIE_DIS, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);

    // TODO: wait for all _STOPPED to be 1. need timeout

    // Enable PCIE LATCH
    val = REG_RD32(FECS, LW_PGC6_BSI_FGC6_PCIE);
    val = FLD_SET_DRF(_PGC6, _BSI_FGC6_PCIE, _LATCH, _ENABLE, val);
    REG_WR32_STALL(FECS, LW_PGC6_BSI_FGC6_PCIE, val);

    // Enable PCIE CLAMP
    val = FLD_SET_DRF(_PGC6, _BSI_FGC6_PCIE, _CLAMP, _ENABLE, val);
    REG_WR32_STALL(FECS, LW_PGC6_BSI_FGC6_PCIE, val);

    OS_PTIMER_SPIN_WAIT_NS(1000);

    // Ungate all clocks again
    gateVal= 0;

    val = REG_RD32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _SYSCLK_XPXV_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _HOSTCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_XCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_FGC6_CLK_GATE, val);

    val = REG_RD32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV);

    val = FLD_SET_DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, gateVal, val);
    REG_WR32(FECS, LW_PTRIM_SYS_AUX_TX_RDET_DIV, val);

    OS_PTIMER_SPIN_WAIT_NS(4000);
}

/*!
 * @brief FGC6 pre-exit sequence. This function follows all the required register writes to
 * prepare for FGC6 exit. The step are from the FGC6 IAS
 */
void
pgFgc6PreExitSeq_TU10X(void)
{
    LwU32 val;

    val = REG_RD32(FECS, LW_PGC6_BSI_FGC6_PCIE);

    // Check if we are in a FGC6 cycle
    if (!FLD_TEST_DRF(_PGC6, _BSI_FGC6_PCIE, _LATCH, _ENABLE, val))
    {
        return;
    }

    // Gate clocks. (Passing LW_TRUE as parameter to gate all clocks in the sequence)
    pgFgc6ClkGating_HAL(NULL, LW_TRUE);

    // Disable PCIE CLAMP
    val = FLD_SET_DRF(_PGC6, _BSI_FGC6_PCIE, _CLAMP, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_FGC6_PCIE, val);

    // Disable PCIE LATCH
    val = REG_RD32(FECS, LW_PGC6_BSI_FGC6_PCIE);
    val = FLD_SET_DRF(_PGC6, _BSI_FGC6_PCIE, _LATCH, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_FGC6_PCIE, val);

    // Ungate Clocks. (Passing LW_FALSE as parameter to ungate all clocks in sequence)
    pgFgc6ClkGating_HAL(NULL, LW_FALSE);

    // De-assert PCIE reset_precredit
    val = REG_RD32(FECS, LW_PGC6_BSI_FGC6_PCIE);
    val = FLD_SET_DRF(_PGC6, _BSI_FGC6_PCIE, _RST_PRECREDIT_IF, _DEASSERT, val);
    REG_WR32(FECS, LW_PGC6_BSI_FGC6_PCIE, val);

    // Disable CLKREQ in FGC6
    val = REG_RD32(FECS, LW_PGC6_BSI_XTAL_DOMAIN_FEATURES);
    val = FLD_SET_DRF(_PGC6, _BSI_XTAL_DOMAIN_FEATURES , _CLKREQ_IN_GC6, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_XTAL_DOMAIN_FEATURES, val);
}

/*
 * Remove LWLINK exit sequence which is done via priv writes in phase1.
 * This function will be called when LWLINK is disabled/has no active links.
 * Lwrrently phase1 is hardcoded with these priv writes and cannot be conditionally removed
 * So patching from PMU before entry is the only choice
 *
 */
void
pgLwlinkBsiPatchExitSeq_TU10X(LwU32 phase)
{

    LwU32 val;
    LwU32 ramCtrl;
    LwU32 count;
    LwU32 numDwords;
    LwU32 srcOffset;
    LwU32 addr;

    LwU32 bar0Ctl =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                      DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _TRIG, _TRUE) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _0 ));

    addr = ((phase * LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE) + LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_PRIV) * sizeof(LwU32);
    ramCtrl = 0;
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _DISABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, addr, ramCtrl);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);

    // parse the number of dwords and location of actual writes in the priv section
    val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
    numDwords = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_NUMWRITES, val);
    srcOffset = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_SRCADDR, val);

    // Set priv section address in BSI RAM
    ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _ENABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, srcOffset, ramCtrl);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);
    count = 0;

    while (count < numDwords)
    {
        // Read the first dword in the sequence
        val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

        // Check if this is a BAR0 or priv write
        if (FLD_TEST_DRF(_PGC6, _BSI_RAMDATA, _OPCODE, _BAR0, val))
        {
            // check to see if we have our target
            if (val == (LW_PPWR_FALCON_MAILBOX0 | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0))
            {
                break;
            }
            // dummy reads to skip data and ctl section
            REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
            REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

            count += LW_PGC6_BAR0_OP_SIZE_DWORD;
        }
        else
        {
            // dummy read to skip data section
            REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);

            count += LW_PGC6_PRIV_OP_SIZE_DWORD;
        }
    }

    // We didn't find anything to patch.
    if (count >= numDwords)
    {
        return;
    }

    // Go back to previous address
    ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
    addr = REF_VAL(LW_PGC6_BSI_RAMCTRL_ADDR, ramCtrl);
    addr-= LW_PGC6_PRIV_OP_SIZE_DWORD;
    ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, addr, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _ENABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, ramCtrl);
    REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);

    // LWLINK exit sequence patched into Phase1 if Lwlink is present(Bug2197144)
    // Nine DWORDS ONLY. If more is needed, add dummy priv writes to phase1

    // 1. Deassert lwlink reset before unclamp
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PMC_ENABLE | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, 0x42000101);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 2,3. Gate clocks in SYS chiplet
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PTRIM_SYS_AUX_TX_RDET_DIV | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, 1));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PTRIM_SYS_FGC6_CLK_GATE | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, 1) |
                                        DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, 1));
    REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(0), val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // Simulating RMW for latch/clamp
    val = 0;
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _CLAMP, _ENABLE, val);
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _LATCH, _ENABLE, val);
    // 4. Disable SCI LWLINK CLAMP
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PGC6_SCI_FGC6_LWLINK | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _CLAMP, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 5. Disable SCI LWLINK LATCH
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PGC6_SCI_FGC6_LWLINK | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = FLD_SET_DRF(_PGC6, _SCI_FGC6_LWLINK, _LATCH, _DISABLE, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 6,7. Ungate clocks in SYS
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PTRIM_SYS_AUX_TX_RDET_DIV | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, DRF_NUM(_PTRIM, _SYS_AUX_TX_RDET_DIV, _RX_BYPASS_REFCLK_DISABLE, 0));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PTRIM_SYS_FGC6_CLK_GATE | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _MGMT_CLK_DISABLE, 0) |
                                        DRF_NUM(_PTRIM, _SYS_FGC6_CLK_GATE, _ALT_LWL_COMMON_CLK_DISABLE, 0));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    //
    // 8,9. Ungate clocks in LNK0
    // Note: In Turing and Ampere, some of these registers are aliases defined above.
    //
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PTRIM_SYS_LWLINK_FGC6_CLK_GATE | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, DRF_NUM(_PTRIM, _SYS_LWLINK_FGC6_CLK_GATE, _SYSCLK_LNK_DISABLE, 0));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_PTRIM_SYS_LWLINK_AUX_TX_RDET_DIV | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, DRF_NUM(_PTRIM, _SYS_LWLINK_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_LWLINK_DIS_OVRD, 0) |
                                        DRF_NUM(_PTRIM, _SYS_LWLINK_AUX_TX_RDET_DIV, _LX_AUX_TX_RDET_CLK_LWLINK_DIS, 0));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);
}

/*!
 * @brief Poll for link state to change
 * @params[in] gc6DetachMask: the mask is to decide if we want to enter L1 (FGC6)
 *                            or enter L2/LinkDisable (RTD3/GC6)
 */
LwU8
pgGc6WaitForLinkState_TU10X(LwU16 gc6DetachMask)
{
    LwU32 state;
    LwU32 val;
    LwBool bForceL2 = FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                             _FORCE_LINKL2, _TRUE, gc6DetachMask);
    LwBool bForceLinkDisable = FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                             _FORCE_LINKDISABLE, _TRUE, gc6DetachMask);

    // Skip link state
    if (pgGc6IsOnFmodel_HAL())
    {
        goto pgGc6WaitForLinkState_TU10X_EXIT;
    }

    // need a special case for emulation
    val = REG_RD32(BAR0, LW_PTOP_PLATFORM);

    //
    // this needs to have a better approach.
    // If ACPI call fails; pmu should timeout and recover.
    //
    while (LW_TRUE)
    {
        state = REG_RD32(FECS, LW_XP_PL_LTSSM_STATE);

        if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _EMU, val))
        {
            // On emulation, we only check if link is disabled.
            if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_DISABLED, _TRUE, state) ||
                FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_L2, _TRUE, state))
            {
                break;
            }
        }
        else if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_L1, _TRUE, state))
        {
            // Check if this is a FGC6 entry
            if (FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                                 _FAST_GC6_ENTRY, _TRUE, gc6DetachMask))
            {
                // Link in L1
                return LINK_IN_L1;
            }
            else
            {
                // This is not a FGC6 cycle. Continue waiting for other link states
                continue;
            }
        }
        else if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_DISABLED, _TRUE, state)    &&
                 FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _LANES_IN_SLEEP, _TRUE, state) && !bForceL2)
        {
            break;
        }
        else if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_L2, _TRUE, state) && !bForceLinkDisable)
        {
            break;
        }

        if (pgGc6IsD3Hot_HAL())
        {
            // Expecting link to be disabled or L2 but not => D3HOT
            return LINK_D3_HOT_CYCLE;
        }
    }

    //
    // The delay below is needed after we saw link disabled true.
    //
    OS_PTIMER_SPIN_WAIT_NS(4000);

pgGc6WaitForLinkState_TU10X_EXIT:

    // Link is disabled or L2
    return LINK_IN_L2_DISABLED;
}

