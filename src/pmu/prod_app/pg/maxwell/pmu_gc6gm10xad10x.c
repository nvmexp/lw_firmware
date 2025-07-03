/*
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_gc6gm10xad10x.c
 * @brief  PMU Power State (GCx) Hal Functions for GM10X
 *
 * Functions implement chip-specific power state (GCx) init and sequences.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "pmu_gc6.h"
#include "dev_bus.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpg.h"
#include "config/g_pg_private.h"

/*-------------------------- Macros ------------------------------------------ */
/*-------------------------- Type Definitions ------------------------------------------ */


/* ------------------------- Prototypes ------------------------------------ */

/* ------------------------- Public Functions ------------------------------ */

/*!
 * @brief Enebale LTSSM HOLDOFF to prevent link tansit to polling
 */
void
pgGc6LtssmHoldOffEnable_GM10X(void)
{
    LwU32 val;

    val = REG_RD32(BAR0, LW_PBUS_HOLD_LTSSM);
    val = FLD_SET_DRF(_PBUS, _HOLD_LTSSM, _VAL, _EN, val);
    REG_WR32(BAR0, LW_PBUS_HOLD_LTSSM, val);
}

