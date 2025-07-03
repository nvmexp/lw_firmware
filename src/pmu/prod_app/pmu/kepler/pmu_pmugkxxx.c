/*
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pmugkxxx.c
 * @brief  PMU Hal Functions for GKXXX
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "dev_master.h"
#include "dev_pwr_falcon_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */
/*!
 * @brief   Set up DMEM APERTURE (EXT2PRIV timeout).
 *
 * @details
 *
 * There have been various values for various chips.  According to Praveen
 * Joginipally (08-May-2018), there is no way to callwlate an absolute
 * upper limit for these transactions.  As such, we choose a number that is
 * large enough to make timeouts very rare except when there is an
 * actual failure.
 *
 * The timeout is driven by the PWRCLK (along with the PMU).  The number of
 * cycles before a timeout is issued is callwlated as:
 *      cycles = (1 + TIME_OUT) * 256 * (2 ^ TIME_UNIT)
 *
 * Assuming a 540MHz PWRCLK, we have the timeout time as:
 *      time = cycles / 540MHz
 *
 * Examples that we have used in the past:
 *        485us = (1+255) * 256 * (2^2) / 540MHz
 *       3880us = (1+255) * 256 * (2^5) / 540MHz
 *       1122us = (1+147) * 256 * (2^4) / 540MHz
 *      10055us = (1+166) * 256 * (2^7) / 540MHz
 *
 * Relevant bugs include:
 * http://lwbugs/200322686
 * http://lwbugs/200239961
 * http://lwbugs/902676
 *
 * The Falcon EXT2PRIV timeout should be greater than the Host PRI
 * Timeout of 128K cycles at 277MHz hostclk. Otherwise accesses to
 * host registers from the ucode can timeout if host itself is blocked
 * waiting on a PRI timeout.
 *
 * On GP107 PG212 SKU500, the HOST2FB request timeout is set to 4 ms due to
 * a bug and consequently, the Falcon EXT2PRIV timeout should be greater
 * than this.
 *
 * Note [01/06/2017] : Starting GK208 Host PRI timeout is not based on
 * hostclk as per comment http://lwbugs/902676/41.
 *
 * Without this _STALL write following BAR0 access may fail.
 *
 * There was pmuPreInitDmemAperture_GV10X.
 * However, now we use the same value in all chips since we're setting it to
 * a value larger than all previously used values.
 *
 * If you need to modify this value for a specific chip or SKU,
 * be aware that Pmu.chipInfo.id or Pmu.gpuDevId may not yet be set.
 * In the past, we had pmuPreInitDmemApertureTimeoutOverride_GP10X
 * (halified) which was called from main.c to deal with this issue.
 */
void
pmuPreInitDmemAperture_GMXXX(void)
{
#ifdef UPROC_FALCON
    REG_WR32_STALL(CSB, LW_CMSDEC_FALCON_DMEMAPERT,
        DRF_NUM(_CMSDEC_FALCON, _DMEMAPERT, _ENABLE,      1) |
        DRF_NUM(_CMSDEC_FALCON, _DMEMAPERT, _TIME_UNIT,   7) |
        DRF_NUM(_CMSDEC_FALCON, _DMEMAPERT, _TIME_OUT,  166));  // 10055us
#endif // UPROC_FALCON
}

