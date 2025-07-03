/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgislandtu10x.c
 * @brief  PMU PGISLAND related Hal functions for TU10X.
 *
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "pmu_objfb.h"
#include "dev_gc6_island.h"
#include "dev_pwr_pri.h"
#include "dev_trim.h"
#include "dev_pwr_csb.h"
#include "dbgprintf.h"
#include "rmlsfm.h"

#include "config/g_pg_private.h"

#include "ctrl/ctrl2080/ctrl2080fb.h"

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

#define LW_PGC6_BSI_PHASE_OFFSET                         1
#define LW_PGC6_BSI_PHASE_SCRIPT                         5

// BSIRAM phase alignment size
#define BSI_RAM_PHASE_ALIGNMENT_SIZE_BYTES             0x4

/*!
 * @brief Check the data that is being requested to copy into BSI RAM
 *
 * This function checks the copy request and data destined for the BSI RAM
 * and is to be used for security reasons when the data is compiled by RM.
 */
FLCN_STATUS
pgIslandSanitizeBsiRam_TU10X
(
    LwU32 bsiBootPhaseInfo
)
{
#if (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV)))
    LwU32 val;
    LwU32 i;
    LwU32 numPhases;
    LwU32 numDwords;
    LwU32 srcOffset;
    LwU32 count;
    LwU32 ramCtrl;
    LwU32 descOffset;
    LwU32 bsiPadCtrl = LW_PGC6_BSI_PADCTRL | LW_PGC6_BSI_RAMDATA_OPCODE_BAR0;

    // The bar0ctl value we are using to override to prevent RM from sending level 2 requests
    LwU32 bar0Ctl =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                      DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _TRIG, _TRUE) |
                      DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _0 ));

    // The bar0ctl value we are using to override to prevent RM from sending level 2 requests
    LwU32 bar0CtlNoTrigger =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                                DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |
                                DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _0 ));

    // The bar0ctl value for level 2 requests
    LwU32 bar0CtlL2 =  (DRF_DEF(_PPWR_PMU, _BAR0_CTL, _CMD, _WRITE) |
                       DRF_NUM(_PPWR_PMU, _BAR0_CTL, _WRBE, 0xF) |
                       DRF_DEF(_PPWR_PMU, _BAR0_CTL, _TRIG, _TRUE) |
                       DRF_DEF(_PPWR_PMU, _BAR0_CTL, _BAR0_PRIV_LEVEL, _2 ));
    //
    // Offset BRSS and HUB reserved space to ensure they do not get clobbered.
    // Todo - size check info should be taken from a secure entity
    //
    LwU32 maxSize = pgIslandGetBsiRamSize_HAL(&Pg) -
                        sizeof(BSI_RAM_SELWRE_SCRATCH_V1) -
                        sizeof(ACR_BSI_HUB_DESC_ARRAY);

    // Go through the payload and check the priv phases to ensure that we only send level 0 writes.
    // Parse the descriptors to find the priv start and number of privs

    // Last GC6 phase is always the max phase
    numPhases = REF_VAL(LW_PGC6_BSI_BOOTPHASES_GC6STOP, bsiBootPhaseInfo) + 1;

    for (i = 0; i < numPhases; i++)
    {
        // Get the offset for the priv info of the i-th phase descriptor in the ram
        descOffset = (i * LW_PGC6_BSI_RAMDATA_DESCRIPTOR_SIZE) + LW_PGC6_BSI_RAMDATA_DESCRIPTOR_OFFSET_PRIV;

        // Set address in BSI RAM to read out the priv info
        ramCtrl = REG_RD32(FECS, LW_PGC6_BSI_RAMCTRL);
        ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, descOffset*sizeof(LwU32), ramCtrl);
        REG_WR32(FECS, LW_PGC6_BSI_RAMCTRL, ramCtrl);

        // parse the number of dwords and location of actual writes in the priv section
        val = REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
        numDwords = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_NUMWRITES, val);
        srcOffset = REF_VAL(LW_PGC6_BSI_RAMDATA_PRIV_SRCADDR, val);

        // Validate the number of dwords
        if (srcOffset + numDwords*sizeof(LwU32) > maxSize)
        {
            return FLCN_ERROR;
        }

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
                // Make sure that we have enough dwords for this op
                if ((numDwords - count) < LW_PGC6_BAR0_OP_SIZE_DWORD)
                {
                    return FLCN_ERROR;
                }

               //
               // Add a special case to reset the BSI registers if needed.
               // Todo: move this to a better spot.
               //
                if (val == bsiPadCtrl)
                {
                    // make sure this is a reset
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, 0x8);
                    // override the ctl to priv level 2
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0CtlL2);
                }
                else
                {
                    // dummy read to skip data section
                    REG_RD32(FECS, LW_PGC6_BSI_RAMDATA);
                    // override the ctl to priv level 0
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);
                }
                count += LW_PGC6_BAR0_OP_SIZE_DWORD;
            }
            else
            {
                // This means we are doing a priv write.
                // Make sure that we have enough dwords for this op
                if ((numDwords - count) < LW_PGC6_PRIV_OP_SIZE_DWORD)
                {
                    return FLCN_ERROR;
                }

                //
                // If we are enabling the autotrigger feature, we should not trigger
                // Assumption is that the write following autotrigger enable will set bar0 ctl
                //
                if (val == LW_PPWR_PMU_BAR0_EN)
                {
                    if (FLD_TEST_DRF(_PPWR, _PMU_BAR0_EN, _AUTO_TRIGGER, _ENABLE, REG_RD32(FECS, LW_PGC6_BSI_RAMDATA)))
                    {
                        // This next write should be setting the bar0_ctl.
                        if (REG_RD32(FECS, LW_PGC6_BSI_RAMDATA) == LW_PPWR_PMU_BAR0_CTL)
                        {
                            // force priv level 0
                            REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0CtlNoTrigger);
                        }
                        else
                        {
                            // If the write was not the CTL, Assume RAM contents corrupted
                            return FLCN_ERROR;
                        }
                        // adjust count as we proccessed an extra priv write
                        count += LW_PGC6_PRIV_OP_SIZE_DWORD;
                    }
                }
                // Check if we are writing to BAR0_CTL
                else if (val == LW_PPWR_PMU_BAR0_CTL)
                {
                    // force priv level 0 here too
                    REG_WR32(FECS, LW_PGC6_BSI_RAMDATA, bar0Ctl);
                }
                else
                {
                    // Now to check the write data
                    if (REG_RD32(FECS, LW_PGC6_BSI_RAMDATA) == LW_PPWR_PMU_BAR0_CTL)
                    {
                        // Return error as this exposes a HW bug, see 2000594
                        return FLCN_ERROR;
                    }
                }

                count += LW_PGC6_PRIV_OP_SIZE_DWORD;
            }
        }
    }
#endif // (!((PMU_PROFILE_GH100 || PMU_PROFILE_GH100_RISCV) || (PMU_PROFILE_G00X || PMU_PROFILE_G00X_RISCV)))
    return FLCN_OK;
}

/*!
 * @brief Clear the GC6 island interrupts
 */
void
pgIslandClearInterrupts_TU10X()
{
    LwU32 val;

    val = REG_RD32(FECS, LW_PGC6_SCI_CNTL);
    val = FLD_SET_DRF_NUM(_PGC6, _SCI_CNTL, _SW_STATUS_COPY_CLEAR,
                          LW_PGC6_SCI_CNTL_SW_STATUS_COPY_CLEAR_TRIGGER, val);

    REG_WR32(FECS, LW_PGC6_SCI_CNTL, val);

    // Disable PME WAKE# toggling
    pgGc6AssertWakePin_HAL(&Pg,  LW_FALSE);
}
