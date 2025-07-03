/*
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6gp10x.c
 * @brief  PMU Power State (GCx) Hal Functions for GP10X
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "pmu_gc6.h"
#include "dev_lw_xp.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/*-------------------------- Macros------------------------------------------ */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Check if response to memory mapped access is enabled
 */
LwBool
pgGc6IsD3Hot_GP10X()
{
    //
    // Use LW_PGC6_BSI_SCRATCH(2) for d3hot negotiation.
    // This register was only referenced in D3cold path for save/restore
    // BSI state but since it's d3hot we don't need it so reuse it here.
    //
    LwU32 skipVal;
    skipVal = REG_RD32(FECS, LW_PGC6_BSI_SCRATCH(2));

    if (skipVal == GC6_D3HOT_RESUME)
    {
        REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(2), GC6_D3HOT_RESUME_ACK);
        return LW_TRUE;
    }
    return LW_FALSE;
}

LwU8
pgGc6WaitForLinkState_GP10X(LwU16 gc6DetachMask)
{
    LwU32 state;
    LwU32 val;

    // No FGC6 support on Pre-Pascal
    PMU_HALT_COND(!FLD_TEST_DRF(_RM_PMU_GC6, _DETACH_MASK,
                         _FAST_GC6_ENTRY, _TRUE, gc6DetachMask));

    // need a special case for emulation
    val = REG_RD32(FECS, LW_PGC6_BSI_SCRATCH(0));

    //
    // this needs to have a better approach.
    // If ACPI call fails; pmu should timeout and recover.
    //
    while (LW_TRUE)
    {
        state = REG_RD32(FECS, LW_XP_PL_LTSSM_STATE);
        if (val == GC6_ON_EMULATION)
        {
            // On emulation, we only check if link is disabled.
            if (FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_DISABLED, _TRUE, state) ||
                FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_L2, _TRUE, state))
            {
                break;
            }
        }
        else
        {
         if ((FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_DISABLED, _TRUE, state) &&
              FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _LANES_IN_SLEEP, _TRUE, state)) ||
              FLD_TEST_DRF(_XP_PL, _LTSSM_STATE, _IS_L2, _TRUE, state))
            {
                break;
            }
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

    // Link is disabled or L2
    return LINK_IN_L2_DISABLED;

}

/*!
 * @brief power island configuration function before trigger
 *
 * This function sets up BSI/SCI settings so SCI/BSI GPIO/INTR can work
 * as configured.
 */
void
pgGc6IslandConfig_GP10X
(
    RM_PMU_GC6_DETACH_DATA *pGc6Detach
)
{
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_OVERRIDE_SEQ_STATE, 0);

    // legacy behavior before Turing and later specific change
    pgGc6DisableAbortAndClearInterrupt_HAL();
}

