/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
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
#include "pmu_gc6.h"
#include "dev_gc6_island.h"
#include "dev_trim.h"
#include "dev_fbpa.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

#include "ctrl/ctrl2080/ctrl2080fb.h"

/* ------------------------- Type Definitions ------------------------------- */

/*!
 *  @brief Set any FB/regulator state before GC6 power down
 */
GCX_GC6_ENTRY_STATUS
pgGc6PrepareFbForGc6_TU10X(LwU8 ramType)
{
    LwU32                fbioCtl;
    LwU32                fbioCtlClear;
    LwU32                val;
    LwU32                val2;

    if ((PMUCFG_FEATURE_ENABLED(PMU_GC6_FBSR_IN_PMU)) &&
        (ramType != LW2080_CTRL_FB_INFO_RAM_TYPE_UNKNOWN))
    {
        //
        // Error logging the FBSR sequence.
        // There may be multiple error been logged here and
        // plesae start with the first error seen.
        // We try our best to continue since link is down
        //
        pgGc6LogErrorStatus(fbFlushL2AllRamsAndCaches_HAL(&Fb));

        pgGc6LogErrorStatus(fbForceIdle_HAL(&Fb));

        pgGc6LogErrorStatus(fbEnterSrAndPowerDown_HAL(&Fb, ramType));

        pgGc6LogErrorStatus(fbPreGc6Clamp_HAL(&Fb));
    }

    // Assert FBCLAMP first on Pascal - see 1713528 #83
    val  = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE);
    val  = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_ARMED_SEQ_STATE, _FB_CLAMP,
                       _ASSERT, val);
    val2 = REG_RD32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE);
    val2 = FLD_SET_DRF(_PGC6, _SCI_PWR_SEQ_STATE, _UPDATE, _TRIGGER, val2);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_ARMED_SEQ_STATE, val);
    REG_WR32(FECS, LW_PGC6_SCI_PWR_SEQ_STATE, val2);

    OS_PTIMER_SPIN_WAIT_NS(1000);

    fbGc6EntryClockDisable_HAL(&Fb);

    //
    // For details, refer to 1664838 #93
    // Power down regulators for all FB partitions
    // including Floorswept
    //

    fbioCtl     = REG_RD32(FECS, LW_PTRIM_SYS_FBIO_SPARE);
    fbioCtlClear = FLD_SET_DRF_NUM(_PTRIM, _SYS_FBIO_SPARE, _FBIO_SPARE, 0x0, fbioCtl);
    REG_WR32(FECS, LW_PTRIM_SYS_FBIO_SPARE, fbioCtlClear);

    return GCX_GC6_OK;
}

