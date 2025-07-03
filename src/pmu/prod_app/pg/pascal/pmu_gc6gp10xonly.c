/*
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6gp10xonly.c
 * @brief  PMU Power State (GCx) Hal Functions for GP10X only.
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "pmu_gc6.h"
#include "dev_lw_xp.h"
#include "dev_therm.h"
#include "dev_pwr_pri.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "dev_fuse_addendum.h"

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

#define LW_PGC6_BAR0_OP_SIZE_DWORD 3
#define LW_PGC6_PRIV_OP_SIZE_DWORD 2

#define LW_PGC6_SCI_TO_THERM_MULTIPLIER 4

/*-------------------------- Macros------------------------------------------ */
/* ------------------------- Public Functions  ------------------------------ */

/*
 * Patch the writes to LW_THERM_VID* so we have the optimized voltage set
 * for GC6 exit when devinit runs
 */
void 
pgGc6PatchForOptimizedVoltage_GP10X(LwU32 phase)
{
    LwU32 val;
    LwU32 ramCtrl;
    LwU32 count;
    LwU32 numDwords;
    LwU32 srcOffset;
    LwU32 addr;
    LwU32 target;
    LwU32 boot0 = REG_RD32(BAR0, LW_PMC_BOOT_0);
    LwU32 subrev = REG_RD32(FECS, LW_FUSE_OPT_SUBREVISION);

    // These registers are level 2 protected, so we have to patch that too
    LwU32 bar0Ctl =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                      DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |  
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _TRIG, _TRUE) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _2 ));

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
            if (val == (LW_THERM_VID0_PWM(0) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0))
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

    // See bug 1751032 for the detailed sequence

    // 1. LW_THERM_VID0_PWM(0) -> set period to 4x value in corresponding SCI_VID
    val = REG_RD32(FECS, LW_THERM_VID0_PWM(0));
    target = LW_PGC6_SCI_TO_THERM_MULTIPLIER * REF_VAL(LW_PGC6_SCI_VID_CFG0_PERIOD, REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(0)));
    val = FLD_SET_DRF_NUM(_THERM, _VID0_PWM, _PERIOD, target, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 2. LW_THERM_VID0_PWM(1) -> trigger bit and 4x interval in corresponding SCI SEQ table
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_THERM_VID0_PWM(1) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = LW_THERM_VID1_PWM_SETTING_NEW_TRIGGER | 
          (LW_PGC6_SCI_TO_THERM_MULTIPLIER*REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(6)));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 3. LW_THERM_VID1_PWM(0) -> set period to 4x value in corresponding SCI_VID
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_THERM_VID1_PWM(0) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = REG_RD32(FECS, LW_THERM_VID1_PWM(0));
    target = LW_PGC6_SCI_TO_THERM_MULTIPLIER * REF_VAL(LW_PGC6_SCI_VID_CFG0_PERIOD, REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(1)));
    val = FLD_SET_DRF_NUM(_THERM, _VID0_PWM, _PERIOD, target, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    // 4. LW_THERM_VID1_PWM(1) -> trigger bit and 4x interval in corresponding SCI SEQ table
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, LW_THERM_VID1_PWM(1) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0);
    val = LW_THERM_VID1_PWM_SETTING_NEW_TRIGGER | 
          (LW_PGC6_SCI_TO_THERM_MULTIPLIER*REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(7)));
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

    //
    // Check if we need the SRAM glitch WAR - see bug 1847895 for detailed sequence
    // We need the SRAM glitch WAR on GP107-A01P
    //
    if (FLD_TEST_DRF(_PMC, _BOOT_0, _ARCHITECTURE, _GP100, boot0) &&
        FLD_TEST_DRF(_PMC, _BOOT_0, _IMPLEMENTATION, _7, boot0)   &&
        FLD_TEST_DRF(_PMC, _BOOT_0, _MINOR_REVISION, _1, boot0)   &&
        FLD_TEST_DRF(_PMC, _BOOT_0, _MAJOR_REVISION, _A, boot0)   &&
        FLD_TEST_DRF(_FUSE, _OPT_SUBREVISION, _DATA, _P, subrev))
    {
        // 1. Update delay in the SCI sequencer to 300us
        val = 0;
        val = FLD_SET_DRF_NUM(_PGC6, _SCI_PWR_SEQ, _DELAY_TABLE_PERIOD, 3, val);
        val = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ, _DELAY_TABLE_SCALE, _100US, val);
        REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_DELAY_TABLE(8), val);

        // 2. Copy SCI VIDs from sequencer and patch BSI RAM writes to set the voltage when setup phase runs

        //   a) Logic: LW_PGC6_SCI_VID_CFG1(0) -> copy LW_PGC6_SCI_PWR_SEQ_VID_TABLE(6) and set trigger bit
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, (LW_PGC6_SCI_VID_CFG1(0) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0));
        val = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(6));
        val = FLD_SET_DRF(_PGC6, _SCI_VID_CFG1, _UPDATE, _TRIGGER, val);
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);

        //   b) SRAM: LW_PGC6_SCI_VID_CFG1(1) -> copy LW_PGC6_SCI_PWR_SEQ_VID_TABLE(7) and set trigger bit
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, (LW_PGC6_SCI_VID_CFG1(1) | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0));
        val = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(7));
        val = FLD_SET_DRF(_PGC6, _SCI_VID_CFG1, _UPDATE, _TRIGGER, val);
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, val);
        REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);


        // 3. Adjust SCI VID values in the sequencer to set to the lower voltage needed for WAR
        //    Sequence to adjust SCI VID in sequencer is:
        //    a. Read _PERIOD
        //    b. Multiply by 42.5% for Logic, 47.5% for SRAM (duty cycle provided by SSg)
        //       This corresponds to 0.725V/0.775V
        //    c. Write result to _HI
        //
        val = REF_VAL(LW_PGC6_SCI_VID_CFG0_PERIOD, REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(0)));
        val = val * 425 / 1000;
        target = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(6));
        target = FLD_SET_DRF_NUM(_PGC6, _SCI_VID, _CFG1_HI, val, target);
        REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(6), target);

        val = REF_VAL(LW_PGC6_SCI_VID_CFG0_PERIOD, REG_RD32(FECS, LW_PGC6_SCI_VID_CFG0(1)));
        val = val * 475 / 1000;
        target = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(7));
        target = FLD_SET_DRF_NUM(_PGC6, _SCI_VID, _CFG1_HI, val, target);
        REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_VID_TABLE(7), target);
    }
}
