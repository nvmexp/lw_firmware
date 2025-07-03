/*
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pwrgm20x.c
 * @brief  PMU Power State (GCx) Hal Functions for GM20X
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_gc6_island.h"
#include "pmu_gc6.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/*-------------------------- Macros------------------------------------------ */

void pgGc6BsiStateSave_GM20X()
{
    LwU32 regVal;

    // Disable CLKREQ and RXSTAT-idle to cause any BSI_EVENTs before entry.
    regVal = REG_RD32(FECS, LW_PGC6_BSI_CTRL);

    // Save off the original value
    REG_WR32(FECS, LW_PGC6_BSI_SCRATCH(2), regVal);

    regVal = FLD_SET_DRF(_PGC6, _BSI_CTRL, _BSI2SCI_CLKREQ, _DISABLE, regVal);
    regVal = FLD_SET_DRF(_PGC6, _BSI_CTRL, _BSI2SCI_RXSTAT, _DISABLE, regVal);
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, regVal);

}

void pgGc6BsiStateRestore_GM20X()
{
    LwU32 regVal;

    // Restore BSI_CTRL
    regVal = REG_RD32(FECS, LW_PGC6_BSI_SCRATCH(2));
    REG_WR32(FECS, LW_PGC6_BSI_CTRL, regVal);
}
